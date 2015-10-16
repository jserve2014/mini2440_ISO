/*
 * SiS 300/540/630[S]/730[S],
 * SiS 315[E|PRO]/550/[M]65x/[M]66x[F|M|G]X/[M]74x[GX]/330/[M]76x[GX],
 * XGI V3XT/V5/V8, Z7
 * frame buffer driver for Linux kernels >= 2.4.14 and >=2.6.3
 *
 * Copyright (C) 2001-2005 Thomas Winischhofer, Vienna, Austria.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author:	Thomas Winischhofer <thomas@winischhofer.net>
 *
 * Author of (practically wiped) code base:
 *		SiS (www.sis.com)
 *		Copyright (C) 1999 Silicon Integrated Systems, Inc.
 *
 * See http://www.winischhofer.net/ for more information and updates
 *
 * Originally based on the VBE 2.0 compliant graphic boards framebuffer driver,
 * which is (c) 1998 Gerd Knorr <kraxel@goldbach.in-berlin.de>
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/screen_info.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/selection.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#include "sis.h"
#include "sis_main.h"

static void sisfb_handle_command(struct sis_video_info *ivideo,
				 struct sisfb_cmd *sisfb_command);

/* ------------------ Internal helper routines ----------------- */

static void __init
sisfb_setdefaultparms(void)
{
	sisfb_off		= 0;
	sisfb_parm_mem		= 0;
	sisfb_accel		= -1;
	sisfb_ypan		= -1;
	sisfb_max		= -1;
	sisfb_userom		= -1;
	sisfb_useoem		= -1;
	sisfb_mode_idx		= -1;
	sisfb_parm_rate		= -1;
	sisfb_crt1off		= 0;
	sisfb_forcecrt1		= -1;
	sisfb_crt2type		= -1;
	sisfb_crt2flags		= 0;
	sisfb_pdc		= 0xff;
	sisfb_pdca		= 0xff;
	sisfb_scalelcd		= -1;
	sisfb_specialtiming 	= CUT_NONE;
	sisfb_lvdshl		= -1;
	sisfb_dstn		= 0;
	sisfb_fstn		= 0;
	sisfb_tvplug		= -1;
	sisfb_tvstd		= -1;
	sisfb_tvxposoffset	= 0;
	sisfb_tvyposoffset	= 0;
	sisfb_nocrt2rate	= 0;
#if !defined(__i386__) && !defined(__x86_64__)
	sisfb_resetcard		= 0;
	sisfb_videoram		= 0;
#endif
}

/* ------------- Parameter parsing -------------- */

static void __devinit
sisfb_search_vesamode(unsigned int vesamode, bool quiet)
{
	int i = 0, j = 0;

	/* We don't know the hardware specs yet and there is no ivideo */

	if(vesamode == 0) {
		if(!quiet)
			printk(KERN_ERR "sisfb: Invalid mode. Using default.\n");

		sisfb_mode_idx = DEFAULT_MODE;

		return;
	}

	vesamode &= 0x1dff;  /* Clean VESA mode number from other flags */

	while(sisbios_mode[i++].mode_no[0] != 0) {
		if( (sisbios_mode[i-1].vesa_mode_no_1 == vesamode) ||
		    (sisbios_mode[i-1].vesa_mode_no_2 == vesamode) ) {
			if(sisfb_fstn) {
				if(sisbios_mode[i-1].mode_no[1] == 0x50 ||
				   sisbios_mode[i-1].mode_no[1] == 0x56 ||
				   sisbios_mode[i-1].mode_no[1] == 0x53)
					continue;
			} else {
				if(sisbios_mode[i-1].mode_no[1] == 0x5a ||
				   sisbios_mode[i-1].mode_no[1] == 0x5b)
					continue;
			}
			sisfb_mode_idx = i - 1;
			j = 1;
			break;
		}
	}
	if((!j) && !quiet)
		printk(KERN_ERR "sisfb: Invalid VESA mode 0x%x'\n", vesamode);
}

static void __devinit
sisfb_search_mode(char *name, bool quiet)
{
	unsigned int j = 0, xres = 0, yres = 0, depth = 0, rate = 0;
	int i = 0;
	char strbuf[16], strbuf1[20];
	char *nameptr = name;

	/* We don't know the hardware specs yet and there is no ivideo */

	if(name == NULL) {
		if(!quiet)
			printk(KERN_ERR "sisfb: Internal error, using default mode.\n");

		sisfb_mode_idx = DEFAULT_MODE;
		return;
	}

	if(!strnicmp(name, sisbios_mode[MODE_INDEX_NONE].name, strlen(name))) {
		if(!quiet)
			printk(KERN_ERR "sisfb: Mode 'none' not supported anymore. Using default.\n");

		sisfb_mode_idx = DEFAULT_MODE;
		return;
	}

	if(strlen(name) <= 19) {
		strcpy(strbuf1, name);
		for(i = 0; i < strlen(strbuf1); i++) {
			if(strbuf1[i] < '0' || strbuf1[i] > '9') strbuf1[i] = ' ';
		}

		/* This does some fuzzy mode naming detection */
		if(sscanf(strbuf1, "%u %u %u %u", &xres, &yres, &depth, &rate) == 4) {
			if((rate <= 32) || (depth > 32)) {
				j = rate; rate = depth; depth = j;
			}
			sprintf(strbuf, "%ux%ux%u", xres, yres, depth);
			nameptr = strbuf;
			sisfb_parm_rate = rate;
		} else if(sscanf(strbuf1, "%u %u %u", &xres, &yres, &depth) == 3) {
			sprintf(strbuf, "%ux%ux%u", xres, yres, depth);
			nameptr = strbuf;
		} else {
			xres = 0;
			if((sscanf(strbuf1, "%u %u", &xres, &yres) == 2) && (xres != 0)) {
				sprintf(strbuf, "%ux%ux8", xres, yres);
				nameptr = strbuf;
			} else {
				sisfb_search_vesamode(simple_strtoul(name, NULL, 0), quiet);
				return;
			}
		}
	}

	i = 0; j = 0;
	while(sisbios_mode[i].mode_no[0] != 0) {
		if(!strnicmp(nameptr, sisbios_mode[i++].name, strlen(nameptr))) {
			if(sisfb_fstn) {
				if(sisbios_mode[i-1].mode_no[1] == 0x50 ||
				   sisbios_mode[i-1].mode_no[1] == 0x56 ||
				   sisbios_mode[i-1].mode_no[1] == 0x53)
					continue;
			} else {
				if(sisbios_mode[i-1].mode_no[1] == 0x5a ||
				   sisbios_mode[i-1].mode_no[1] == 0x5b)
					continue;
			}
			sisfb_mode_idx = i - 1;
			j = 1;
			break;
		}
	}

	if((!j) && !quiet)
		printk(KERN_ERR "sisfb: Invalid mode '%s'\n", nameptr);
}

#ifndef MODULE
static void __devinit
sisfb_get_vga_mode_from_kernel(void)
{
#ifdef CONFIG_X86
	char mymode[32];
	int  mydepth = screen_info.lfb_depth;

	if(screen_info.orig_video_isVGA != VIDEO_TYPE_VLFB) return;

	if( (screen_info.lfb_width >= 320) && (screen_info.lfb_width <= 2048) &&
	    (screen_info.lfb_height >= 200) && (screen_info.lfb_height <= 1536) &&
	    (mydepth >= 8) && (mydepth <= 32) ) {

		if(mydepth == 24) mydepth = 32;

		sprintf(mymode, "%ux%ux%u", screen_info.lfb_width,
					screen_info.lfb_height,
					mydepth);

		printk(KERN_DEBUG
			"sisfb: Using vga mode %s pre-set by kernel as default\n",
			mymode);

		sisfb_search_mode(mymode, true);
	}
#endif
	return;
}
#endif

static void __init
sisfb_search_crt2type(const char *name)
{
	int i = 0;

	/* We don't know the hardware specs yet and there is no ivideo */

	if(name == NULL) return;

	while(sis_crt2type[i].type_no != -1) {
		if(!strnicmp(name, sis_crt2type[i].name, strlen(sis_crt2type[i].name))) {
			sisfb_crt2type = sis_crt2type[i].type_no;
			sisfb_tvplug = sis_crt2type[i].tvplug_no;
			sisfb_crt2flags = sis_crt2type[i].flags;
			break;
		}
		i++;
	}

	sisfb_dstn = (sisfb_crt2flags & FL_550_DSTN) ? 1 : 0;
	sisfb_fstn = (sisfb_crt2flags & FL_550_FSTN) ? 1 : 0;

	if(sisfb_crt2type < 0)
		printk(KERN_ERR "sisfb: Invalid CRT2 type: %s\n", name);
}

static void __init
sisfb_search_tvstd(const char *name)
{
	int i = 0;

	/* We don't know the hardware specs yet and there is no ivideo */

	if(name == NULL)
		return;

	while(sis_tvtype[i].type_no != -1) {
		if(!strnicmp(name, sis_tvtype[i].name, strlen(sis_tvtype[i].name))) {
			sisfb_tvstd = sis_tvtype[i].type_no;
			break;
		}
		i++;
	}
}

static void __init
sisfb_search_specialtiming(const char *name)
{
	int i = 0;
	bool found = false;

	/* We don't know the hardware specs yet and there is no ivideo */

	if(name == NULL)
		return;

	if(!strnicmp(name, "none", 4)) {
		sisfb_specialtiming = CUT_FORCENONE;
		printk(KERN_DEBUG "sisfb: Special timing disabled\n");
	} else {
		while(mycustomttable[i].chipID != 0) {
			if(!strnicmp(name,mycustomttable[i].optionName,
			   strlen(mycustomttable[i].optionName))) {
				sisfb_specialtiming = mycustomttable[i].SpecialID;
				found = true;
				printk(KERN_INFO "sisfb: Special timing for %s %s forced (\"%s\")\n",
					mycustomttable[i].vendorName,
					mycustomttable[i].cardName,
					mycustomttable[i].optionName);
				break;
			}
			i++;
		}
		if(!found) {
			printk(KERN_WARNING "sisfb: Invalid SpecialTiming parameter, valid are:");
			printk(KERN_WARNING "\t\"none\" (to disable special timings)\n");
			i = 0;
			while(mycustomttable[i].chipID != 0) {
				printk(KERN_WARNING "\t\"%s\" (for %s %s)\n",
					mycustomttable[i].optionName,
					mycustomttable[i].vendorName,
					mycustomttable[i].cardName);
				i++;
			}
		}
	}
}

/* ----------- Various detection routines ----------- */

static void __devinit
sisfb_detect_custom_timing(struct sis_video_info *ivideo)
{
	unsigned char *biosver = NULL;
	unsigned char *biosdate = NULL;
	bool footprint;
	u32 chksum = 0;
	int i, j;

	if(ivideo->SiS_Pr.UseROM) {
		biosver = ivideo->SiS_Pr.VirtualRomBase + 0x06;
		biosdate = ivideo->SiS_Pr.VirtualRomBase + 0x2c;
		for(i = 0; i < 32768; i++)
			chksum += ivideo->SiS_Pr.VirtualRomBase[i];
	}

	i = 0;
	do {
		if( (mycustomttable[i].chipID == ivideo->chip)			&&
		    ((!strlen(mycustomttable[i].biosversion)) ||
		     (ivideo->SiS_Pr.UseROM &&
		      (!strncmp(mycustomttable[i].biosversion, biosver,
				strlen(mycustomttable[i].biosversion)))))	&&
		    ((!strlen(mycustomttable[i].biosdate)) ||
		     (ivideo->SiS_Pr.UseROM &&
		      (!strncmp(mycustomttable[i].biosdate, biosdate,
				strlen(mycustomttable[i].biosdate)))))		&&
		    ((!mycustomttable[i].bioschksum) ||
		     (ivideo->SiS_Pr.UseROM &&
		      (mycustomttable[i].bioschksum == chksum)))		&&
		    (mycustomttable[i].pcisubsysvendor == ivideo->subsysvendor) &&
		    (mycustomttable[i].pcisubsyscard == ivideo->subsysdevice) ) {
			footprint = true;
			for(j = 0; j < 5; j++) {
				if(mycustomttable[i].biosFootprintAddr[j]) {
					if(ivideo->SiS_Pr.UseROM) {
						if(ivideo->SiS_Pr.VirtualRomBase[mycustomttable[i].biosFootprintAddr[j]] !=
							mycustomttable[i].biosFootprintData[j]) {
							footprint = false;
						}
					} else
						footprint = false;
				}
			}
			if(footprint) {
				ivideo->SiS_Pr.SiS_CustomT = mycustomttable[i].SpecialID;
				printk(KERN_DEBUG "sisfb: Identified [%s %s], special timing applies\n",
					mycustomttable[i].vendorName,
				mycustomttable[i].cardName);
				printk(KERN_DEBUG "sisfb: [specialtiming parameter name: %s]\n",
					mycustomttable[i].optionName);
				break;
			}
		}
		i++;
	} while(mycustomttable[i].chipID);
}

static bool __devinit
sisfb_interpret_edid(struct sisfb_monitor *monitor, u8 *buffer)
{
	int i, j, xres, yres, refresh, index;
	u32 emodes;

	if(buffer[0] != 0x00 || buffer[1] != 0xff ||
	   buffer[2] != 0xff || buffer[3] != 0xff ||
	   buffer[4] != 0xff || buffer[5] != 0xff ||
	   buffer[6] != 0xff || buffer[7] != 0x00) {
		printk(KERN_DEBUG "sisfb: Bad EDID header\n");
		return false;
	}

	if(buffer[0x12] != 0x01) {
		printk(KERN_INFO "sisfb: EDID version %d not supported\n",
			buffer[0x12]);
		return false;
	}

	monitor->feature = buffer[0x18];

	if(!(buffer[0x14] & 0x80)) {
		if(!(buffer[0x14] & 0x08)) {
			printk(KERN_INFO
				"sisfb: WARNING: Monitor does not support separate syncs\n");
		}
	}

	if(buffer[0x13] >= 0x01) {
	   /* EDID V1 rev 1 and 2: Search for monitor descriptor
	    * to extract ranges
	    */
	    j = 0x36;
	    for(i=0; i<4; i++) {
	       if(buffer[j]     == 0x00 && buffer[j + 1] == 0x00 &&
		  buffer[j + 2] == 0x00 && buffer[j + 3] == 0xfd &&
		  buffer[j + 4] == 0x00) {
		  monitor->hmin = buffer[j + 7];
		  monitor->hmax = buffer[j + 8];
		  monitor->vmin = buffer[j + 5];
		  monitor->vmax = buffer[j + 6];
		  monitor->dclockmax = buffer[j + 9] * 10 * 1000;
		  monitor->datavalid = true;
		  break;
	       }
	       j += 18;
	    }
	}

	if(!monitor->datavalid) {
	   /* Otherwise: Get a range from the list of supported
	    * Estabished Timings. This is not entirely accurate,
	    * because fixed frequency monitors are not supported
	    * that way.
	    */
	   monitor->hmin = 65535; monitor->hmax = 0;
	   monitor->vmin = 65535; monitor->vmax = 0;
	   monitor->dclockmax = 0;
	   emodes = buffer[0x23] | (buffer[0x24] << 8) | (buffer[0x25] << 16);
	   for(i = 0; i < 13; i++) {
	      if(emodes & sisfb_ddcsmodes[i].mask) {
		 if(monitor->hmin > sisfb_ddcsmodes[i].h) monitor->hmin = sisfb_ddcsmodes[i].h;
		 if(monitor->hmax < sisfb_ddcsmodes[i].h) monitor->hmax = sisfb_ddcsmodes[i].h + 1;
		 if(monitor->vmin > sisfb_ddcsmodes[i].v) monitor->vmin = sisfb_ddcsmodes[i].v;
		 if(monitor->vmax < sisfb_ddcsmodes[i].v) monitor->vmax = sisfb_ddcsmodes[i].v;
		 if(monitor->dclockmax < sisfb_ddcsmodes[i].d) monitor->dclockmax = sisfb_ddcsmodes[i].d;
	      }
	   }
	   index = 0x26;
	   for(i = 0; i < 8; i++) {
	      xres = (buffer[index] + 31) * 8;
	      switch(buffer[index + 1] & 0xc0) {
		 case 0xc0: yres = (xres * 9) / 16; break;
		 case 0x80: yres = (xres * 4) /  5; break;
		 case 0x40: yres = (xres * 3) /  4; break;
		 default:   yres = xres;	    break;
	      }
	      refresh = (buffer[index + 1] & 0x3f) + 60;
	      if((xres >= 640) && (yres >= 480)) {
		 for(j = 0; j < 8; j++) {
		    if((xres == sisfb_ddcfmodes[j].x) &&
		       (yres == sisfb_ddcfmodes[j].y) &&
		       (refresh == sisfb_ddcfmodes[j].v)) {
		      if(monitor->hmin > sisfb_ddcfmodes[j].h) monitor->hmin = sisfb_ddcfmodes[j].h;
		      if(monitor->hmax < sisfb_ddcfmodes[j].h) monitor->hmax = sisfb_ddcfmodes[j].h + 1;
		      if(monitor->vmin > sisfb_ddcsmodes[j].v) monitor->vmin = sisfb_ddcsmodes[j].v;
		      if(monitor->vmax < sisfb_ddcsmodes[j].v) monitor->vmax = sisfb_ddcsmodes[j].v;
		      if(monitor->dclockmax < sisfb_ddcsmodes[j].d) monitor->dclockmax = sisfb_ddcsmodes[j].d;
		    }
		 }
	      }
	      index += 2;
	   }
	   if((monitor->hmin <= monitor->hmax) && (monitor->vmin <= monitor->vmax)) {
	      monitor->datavalid = true;
	   }
	}

	return monitor->datavalid;
}

static void __devinit
sisfb_handle_ddc(struct sis_video_info *ivideo, struct sisfb_monitor *monitor, int crtno)
{
	unsigned short temp, i, realcrtno = crtno;
	unsigned char  buffer[256];

	monitor->datavalid = false;

	if(crtno) {
	   if(ivideo->vbflags & CRT2_LCD)      realcrtno = 1;
	   else if(ivideo->vbflags & CRT2_VGA) realcrtno = 2;
	   else return;
	}

	if((ivideo->sisfb_crt1off) && (!crtno))
		return;

	temp = SiS_HandleDDC(&ivideo->SiS_Pr, ivideo->vbflags, ivideo->sisvga_engine,
				realcrtno, 0, &buffer[0], ivideo->vbflags2);
	if((!temp) || (temp == 0xffff)) {
	   printk(KERN_INFO "sisfb: CRT%d DDC probing failed\n", crtno + 1);
	   return;
	} else {
	   printk(KERN_INFO "sisfb: CRT%d DDC supported\n", crtno + 1);
	   printk(KERN_INFO "sisfb: CRT%d DDC level: %s%s%s%s\n",
		crtno + 1,
		(temp & 0x1a) ? "" : "[none of the supported]",
		(temp & 0x02) ? "2 " : "",
		(temp & 0x08) ? "D&P" : "",
		(temp & 0x10) ? "FPDI-2" : "");
	   if(temp & 0x02) {
	      i = 3;  /* Number of retrys */
	      do {
		 temp = SiS_HandleDDC(&ivideo->SiS_Pr, ivideo->vbflags, ivideo->sisvga_engine,
				     realcrtno, 1, &buffer[0], ivideo->vbflags2);
	      } while((temp) && i--);
	      if(!temp) {
		 if(sisfb_interpret_edid(monitor, &buffer[0])) {
		    printk(KERN_INFO "sisfb: Monitor range H %d-%dKHz, V %d-%dHz, Max. dotclock %dMHz\n",
			monitor->hmin, monitor->hmax, monitor->vmin, monitor->vmax,
			monitor->dclockmax / 1000);
		 } else {
		    printk(KERN_INFO "sisfb: CRT%d DDC EDID corrupt\n", crtno + 1);
		 }
	      } else {
		 printk(KERN_INFO "sisfb: CRT%d DDC reading failed\n", crtno + 1);
	      }
	   } else {
	      printk(KERN_INFO "sisfb: VESA D&P and FPDI-2 not supported yet\n");
	   }
	}
}

/* -------------- Mode validation --------------- */

static bool
sisfb_verify_rate(struct sis_video_info *ivideo, struct sisfb_monitor *monitor,
		int mode_idx, int rate_idx, int rate)
{
	int htotal, vtotal;
	unsigned int dclock, hsync;

	if(!monitor->datavalid)
		return true;

	if(mode_idx < 0)
		return false;

	/* Skip for 320x200, 320x240, 640x400 */
	switch(sisbios_mode[mode_idx].mode_no[ivideo->mni]) {
	case 0x59:
	case 0x41:
	case 0x4f:
	case 0x50:
	case 0x56:
	case 0x53:
	case 0x2f:
	case 0x5d:
	case 0x5e:
		return true;
#ifdef CONFIG_FB_SIS_315
	case 0x5a:
	case 0x5b:
		if(ivideo->sisvga_engine == SIS_315_VGA) return true;
#endif
	}

	if(rate < (monitor->vmin - 1))
		return false;
	if(rate > (monitor->vmax + 1))
		return false;

	if(sisfb_gettotalfrommode(&ivideo->SiS_Pr,
				  sisbios_mode[mode_idx].mode_no[ivideo->mni],
				  &htotal, &vtotal, rate_idx)) {
		dclock = (htotal * vtotal * rate) / 1000;
		if(dclock > (monitor->dclockmax + 1000))
			return false;
		hsync = dclock / htotal;
		if(hsync < (monitor->hmin - 1))
			return false;
		if(hsync > (monitor->hmax + 1))
			return false;
        } else {
		return false;
	}
	return true;
}

static int
sisfb_validate_mode(struct sis_video_info *ivideo, int myindex, u32 vbflags)
{
	u16 xres=0, yres, myres;

#ifdef CONFIG_FB_SIS_300
	if(ivideo->sisvga_engine == SIS_300_VGA) {
		if(!(sisbios_mode[myindex].chipset & MD_SIS300))
			return -1 ;
	}
#endif
#ifdef CONFIG_FB_SIS_315
	if(ivideo->sisvga_engine == SIS_315_VGA) {
		if(!(sisbios_mode[myindex].chipset & MD_SIS315))
			return -1;
	}
#endif

	myres = sisbios_mode[myindex].yres;

	switch(vbflags & VB_DISPTYPE_DISP2) {

	case CRT2_LCD:
		xres = ivideo->lcdxres; yres = ivideo->lcdyres;

		if((ivideo->SiS_Pr.SiS_CustomT != CUT_PANEL848) &&
		   (ivideo->SiS_Pr.SiS_CustomT != CUT_PANEL856)) {
			if(sisbios_mode[myindex].xres > xres)
				return -1;
			if(myres > yres)
				return -1;
		}

		if(ivideo->sisfb_fstn) {
			if(sisbios_mode[myindex].xres == 320) {
				if(myres == 240) {
					switch(sisbios_mode[myindex].mode_no[1]) {
						case 0x50: myindex = MODE_FSTN_8;  break;
						case 0x56: myindex = MODE_FSTN_16; break;
						case 0x53: return -1;
					}
				}
			}
		}

		if(SiS_GetModeID_LCD(ivideo->sisvga_engine, vbflags, sisbios_mode[myindex].xres,
			 	sisbios_mode[myindex].yres, 0, ivideo->sisfb_fstn,
			 	ivideo->SiS_Pr.SiS_CustomT, xres, yres, ivideo->vbflags2) < 0x14) {
			return -1;
		}
		break;

	case CRT2_TV:
		if(SiS_GetModeID_TV(ivideo->sisvga_engine, vbflags, sisbios_mode[myindex].xres,
				sisbios_mode[myindex].yres, 0, ivideo->vbflags2) < 0x14) {
			return -1;
		}
		break;

	case CRT2_VGA:
		if(SiS_GetModeID_VGA2(ivideo->sisvga_engine, vbflags, sisbios_mode[myindex].xres,
				sisbios_mode[myindex].yres, 0, ivideo->vbflags2) < 0x14) {
			return -1;
		}
		break;
	}

	return myindex;
}

static u8
sisfb_search_refresh_rate(struct sis_video_info *ivideo, unsigned int rate, int mode_idx)
{
	int i = 0;
	u16 xres = sisbios_mode[mode_idx].xres;
	u16 yres = sisbios_mode[mode_idx].yres;

	ivideo->rate_idx = 0;
	while((sisfb_vrate[i].idx != 0) && (sisfb_vrate[i].xres <= xres)) {
		if((sisfb_vrate[i].xres == xres) && (sisfb_vrate[i].yres == yres)) {
			if(sisfb_vrate[i].refresh == rate) {
				ivideo->rate_idx = sisfb_vrate[i].idx;
				break;
			} else if(sisfb_vrate[i].refresh > rate) {
				if((sisfb_vrate[i].refresh - rate) <= 3) {
					DPRINTK("sisfb: Adjusting rate from %d up to %d\n",
						rate, sisfb_vrate[i].refresh);
					ivideo->rate_idx = sisfb_vrate[i].idx;
					ivideo->refresh_rate = sisfb_vrate[i].refresh;
				} else if((sisfb_vrate[i].idx != 1) &&
						((rate - sisfb_vrate[i-1].refresh) <= 2)) {
					DPRINTK("sisfb: Adjusting rate from %d down to %d\n",
						rate, sisfb_vrate[i-1].refresh);
					ivideo->rate_idx = sisfb_vrate[i-1].idx;
					ivideo->refresh_rate = sisfb_vrate[i-1].refresh;
				}
				break;
			} else if((rate - sisfb_vrate[i].refresh) <= 2) {
				DPRINTK("sisfb: Adjusting rate from %d down to %d\n",
						rate, sisfb_vrate[i].refresh);
				ivideo->rate_idx = sisfb_vrate[i].idx;
				break;
			}
		}
		i++;
	}
	if(ivideo->rate_idx > 0) {
		return ivideo->rate_idx;
	} else {
		printk(KERN_INFO "sisfb: Unsupported rate %d for %dx%d\n",
				rate, xres, yres);
		return 0;
	}
}

static bool
sisfb_bridgeisslave(struct sis_video_info *ivideo)
{
	unsigned char P1_00;

	if(!(ivideo->vbflags2 & VB2_VIDEOBRIDGE))
		return false;

	inSISIDXREG(SISPART1,0x00,P1_00);
	if( ((ivideo->sisvga_engine == SIS_300_VGA) && (P1_00 & 0xa0) == 0x20) ||
	    ((ivideo->sisvga_engine == SIS_315_VGA) && (P1_00 & 0x50) == 0x10) ) {
		return true;
	} else {
		return false;
	}
}

static bool
sisfballowretracecrt1(struct sis_video_info *ivideo)
{
	u8 temp;

	inSISIDXREG(SISCR,0x17,temp);
	if(!(temp & 0x80))
		return false;

	inSISIDXREG(SISSR,0x1f,temp);
	if(temp & 0xc0)
		return false;

	return true;
}

static bool
sisfbcheckvretracecrt1(struct sis_video_info *ivideo)
{
	if(!sisfballowretracecrt1(ivideo))
		return false;

	if(inSISREG(SISINPSTAT) & 0x08)
		return true;
	else
		return false;
}

static void
sisfbwaitretracecrt1(struct sis_video_info *ivideo)
{
	int watchdog;

	if(!sisfballowretracecrt1(ivideo))
		return;

	watchdog = 65536;
	while((!(inSISREG(SISINPSTAT) & 0x08)) && --watchdog);
	watchdog = 65536;
	while((inSISREG(SISINPSTAT) & 0x08) && --watchdog);
}

static bool
sisfbcheckvretracecrt2(struct sis_video_info *ivideo)
{
	unsigned char temp, reg;

	switch(ivideo->sisvga_engine) {
	case SIS_300_VGA: reg = 0x25; break;
	case SIS_315_VGA: reg = 0x30; break;
	default:	  return false;
	}

	inSISIDXREG(SISPART1, reg, temp);
	if(temp & 0x02)
		return true;
	else
		return false;
}

static bool
sisfb_CheckVBRetrace(struct sis_video_info *ivideo)
{
	if(ivideo->currentvbflags & VB_DISPTYPE_DISP2) {
		if(!sisfb_bridgeisslave(ivideo)) {
			return sisfbcheckvretracecrt2(ivideo);
		}
	}
	return sisfbcheckvretracecrt1(ivideo);
}

static u32
sisfb_setupvbblankflags(struct sis_video_info *ivideo, u32 *vcount, u32 *hcount)
{
	u8 idx, reg1, reg2, reg3, reg4;
	u32 ret = 0;

	(*vcount) = (*hcount) = 0;

	if((ivideo->currentvbflags & VB_DISPTYPE_DISP2) && (!(sisfb_bridgeisslave(ivideo)))) {

		ret |= (FB_VBLANK_HAVE_VSYNC  |
			FB_VBLANK_HAVE_HBLANK |
			FB_VBLANK_HAVE_VBLANK |
			FB_VBLANK_HAVE_VCOUNT |
			FB_VBLANK_HAVE_HCOUNT);
		switch(ivideo->sisvga_engine) {
			case SIS_300_VGA: idx = 0x25; break;
			default:
			case SIS_315_VGA: idx = 0x30; break;
		}
		inSISIDXREG(SISPART1,(idx+0),reg1); /* 30 */
		inSISIDXREG(SISPART1,(idx+1),reg2); /* 31 */
		inSISIDXREG(SISPART1,(idx+2),reg3); /* 32 */
		inSISIDXREG(SISPART1,(idx+3),reg4); /* 33 */
		if(reg1 & 0x01) ret |= FB_VBLANK_VBLANKING;
		if(reg1 & 0x02) ret |= FB_VBLANK_VSYNCING;
		if(reg4 & 0x80) ret |= FB_VBLANK_HBLANKING;
		(*vcount) = reg3 | ((reg4 & 0x70) << 4);
		(*hcount) = reg2 | ((reg4 & 0x0f) << 8);

	} else if(sisfballowretracecrt1(ivideo)) {

		ret |= (FB_VBLANK_HAVE_VSYNC  |
			FB_VBLANK_HAVE_VBLANK |
			FB_VBLANK_HAVE_VCOUNT |
			FB_VBLANK_HAVE_HCOUNT);
		reg1 = inSISREG(SISINPSTAT);
		if(reg1 & 0x08) ret |= FB_VBLANK_VSYNCING;
		if(reg1 & 0x01) ret |= FB_VBLANK_VBLANKING;
		inSISIDXREG(SISCR,0x20,reg1);
		inSISIDXREG(SISCR,0x1b,reg1);
		inSISIDXREG(SISCR,0x1c,reg2);
		inSISIDXREG(SISCR,0x1d,reg3);
		(*vcount) = reg2 | ((reg3 & 0x07) << 8);
		(*hcount) = (reg1 | ((reg3 & 0x10) << 4)) << 3;
	}

	return ret;
}

static int
sisfb_myblank(struct sis_video_info *ivideo, int blank)
{
	u8 sr01, sr11, sr1f, cr63=0, p2_0, p1_13;
	bool backlight = true;

	switch(blank) {
		case FB_BLANK_UNBLANK:	/* on */
			sr01  = 0x00;
			sr11  = 0x00;
			sr1f  = 0x00;
			cr63  = 0x00;
			p2_0  = 0x20;
			p1_13 = 0x00;
			backlight = true;
			break;
		case FB_BLANK_NORMAL:	/* blank */
			sr01  = 0x20;
			sr11  = 0x00;
			sr1f  = 0x00;
			cr63  = 0x00;
			p2_0  = 0x20;
			p1_13 = 0x00;
			backlight = true;
			break;
		case FB_BLANK_VSYNC_SUSPEND:	/* no vsync */
			sr01  = 0x20;
			sr11  = 0x08;
			sr1f  = 0x80;
			cr63  = 0x40;
			p2_0  = 0x40;
			p1_13 = 0x80;
			backlight = false;
			break;
		case FB_BLANK_HSYNC_SUSPEND:	/* no hsync */
			sr01  = 0x20;
			sr11  = 0x08;
			sr1f  = 0x40;
			cr63  = 0x40;
			p2_0  = 0x80;
			p1_13 = 0x40;
			backlight = false;
			break;
		case FB_BLANK_POWERDOWN:	/* off */
			sr01  = 0x20;
			sr11  = 0x08;
			sr1f  = 0xc0;
			cr63  = 0x40;
			p2_0  = 0xc0;
			p1_13 = 0xc0;
			backlight = false;
			break;
		default:
			return 1;
	}

	if(ivideo->currentvbflags & VB_DISPTYPE_CRT1) {

		if( (!ivideo->sisfb_thismonitor.datavalid) ||
		    ((ivideo->sisfb_thismonitor.datavalid) &&
		     (ivideo->sisfb_thismonitor.feature & 0xe0))) {

			if(ivideo->sisvga_engine == SIS_315_VGA) {
				setSISIDXREG(SISCR, ivideo->SiS_Pr.SiS_MyCR63, 0xbf, cr63);
			}

			if(!(sisfb_bridgeisslave(ivideo))) {
				setSISIDXREG(SISSR, 0x01, ~0x20, sr01);
				setSISIDXREG(SISSR, 0x1f, 0x3f, sr1f);
			}
		}

	}

	if(ivideo->currentvbflags & CRT2_LCD) {

		if(ivideo->vbflags2 & VB2_SISLVDSBRIDGE) {
			if(backlight) {
				SiS_SiS30xBLOn(&ivideo->SiS_Pr);
			} else {
				SiS_SiS30xBLOff(&ivideo->SiS_Pr);
			}
		} else if(ivideo->sisvga_engine == SIS_315_VGA) {
#ifdef CONFIG_FB_SIS_315
			if(ivideo->vbflags2 & VB2_CHRONTEL) {
				if(backlight) {
					SiS_Chrontel701xBLOn(&ivideo->SiS_Pr);
				} else {
					SiS_Chrontel701xBLOff(&ivideo->SiS_Pr);
				}
			}
#endif
		}

		if(((ivideo->sisvga_engine == SIS_300_VGA) &&
		    (ivideo->vbflags2 & (VB2_301|VB2_30xBDH|VB2_LVDS))) ||
		   ((ivideo->sisvga_engine == SIS_315_VGA) &&
		    ((ivideo->vbflags2 & (VB2_LVDS | VB2_CHRONTEL)) == VB2_LVDS))) {
			setSISIDXREG(SISSR, 0x11, ~0x0c, sr11);
		}

		if(ivideo->sisvga_engine == SIS_300_VGA) {
			if((ivideo->vbflags2 & VB2_30xB) &&
			   (!(ivideo->vbflags2 & VB2_30xBDH))) {
				setSISIDXREG(SISPART1, 0x13, 0x3f, p1_13);
			}
		} else if(ivideo->sisvga_engine == SIS_315_VGA) {
			if((ivideo->vbflags2 & VB2_30xB) &&
			   (!(ivideo->vbflags2 & VB2_30xBDH))) {
				setSISIDXREG(SISPART2, 0x00, 0x1f, p2_0);
			}
		}

	} else if(ivideo->currentvbflags & CRT2_VGA) {

		if(ivideo->vbflags2 & VB2_30xB) {
			setSISIDXREG(SISPART2, 0x00, 0x1f, p2_0);
		}

	}

	return 0;
}

/* ------------- Callbacks from init.c/init301.c  -------------- */

#ifdef CONFIG_FB_SIS_300
unsigned int
sisfb_read_nbridge_pci_dword(struct SiS_Private *SiS_Pr, int reg)
{
   struct sis_video_info *ivideo = (struct sis_video_info *)SiS_Pr->ivideo;
   u32 val = 0;

   pci_read_config_dword(ivideo->nbridge, reg, &val);
   return (unsigned int)val;
}

void
sisfb_write_nbridge_pci_dword(struct SiS_Private *SiS_Pr, int reg, unsigned int val)
{
   struct sis_video_info *ivideo = (struct sis_video_info *)SiS_Pr->ivideo;

   pci_write_config_dword(ivideo->nbridge, reg, (u32)val);
}

unsigned int
sisfb_read_lpc_pci_dword(struct SiS_Private *SiS_Pr, int reg)
{
   struct sis_video_info *ivideo = (struct sis_video_info *)SiS_Pr->ivideo;
   u32 val = 0;

   if(!ivideo->lpcdev) return 0;

   pci_read_config_dword(ivideo->lpcdev, reg, &val);
   return (unsigned int)val;
}
#endif

#ifdef CONFIG_FB_SIS_315
void
sisfb_write_nbridge_pci_byte(struct SiS_Private *SiS_Pr, int reg, unsigned char val)
{
   struct sis_video_info *ivideo = (struct sis_video_info *)SiS_Pr->ivideo;

   pci_write_config_byte(ivideo->nbridge, reg, (u8)val);
}

unsigned int
sisfb_read_mio_pci_word(struct SiS_Private *SiS_Pr, int reg)
{
   struct sis_video_info *ivideo = (struct sis_video_info *)SiS_Pr->ivideo;
   u16 val = 0;

   if(!ivideo->lpcdev) return 0;

   pci_read_config_word(ivideo->lpcdev, reg, &val);
   return (unsigned int)val;
}
#endif

/* ----------- FBDev related routines for all series ----------- */

static int
sisfb_get_cmap_len(const struct fb_var_screeninfo *var)
{
	return (var->bits_per_pixel == 8) ? 256 : 16;
}

static void
sisfb_set_vparms(struct sis_video_info *ivideo)
{
	switch(ivideo->video_bpp) {
	case 8:
		ivideo->DstColor = 0x0000;
		ivideo->SiS310_AccelDepth = 0x00000000;
		ivideo->video_cmap_len = 256;
		break;
	case 16:
		ivideo->DstColor = 0x8000;
		ivideo->SiS310_AccelDepth = 0x00010000;
		ivideo->video_cmap_len = 16;
		break;
	case 32:
		ivideo->DstColor = 0xC000;
		ivideo->SiS310_AccelDepth = 0x00020000;
		ivideo->video_cmap_len = 16;
		break;
	default:
		ivideo->video_cmap_len = 16;
		printk(KERN_ERR "sisfb: Unsupported depth %d", ivideo->video_bpp);
		ivideo->accel = 0;
	}
}

static int
sisfb_calc_maxyres(struct sis_video_info *ivideo, struct fb_var_screeninfo *var)
{
	int maxyres = ivideo->sisfb_mem / (var->xres_virtual * (var->bits_per_pixel >> 3));

	if(maxyres > 32767) maxyres = 32767;

	return maxyres;
}

static void
sisfb_calc_pitch(struct sis_video_info *ivideo, struct fb_var_screeninfo *var)
{
	ivideo->video_linelength = var->xres_virtual * (var->bits_per_pixel >> 3);
	ivideo->scrnpitchCRT1 = ivideo->video_linelength;
	if(!(ivideo->currentvbflags & CRT1_LCDA)) {
		if((var->vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED) {
			ivideo->scrnpitchCRT1 <<= 1;
		}
	}
}

static void
sisfb_set_pitch(struct sis_video_info *ivideo)
{
	bool isslavemode = false;
	unsigned short HDisplay1 = ivideo->scrnpitchCRT1 >> 3;
	unsigned short HDisplay2 = ivideo->video_linelength >> 3;

	if(sisfb_bridgeisslave(ivideo)) isslavemode = true;

	/* We need to set pitch for CRT1 if bridge is in slave mode, too */
	if((ivideo->currentvbflags & VB_DISPTYPE_DISP1) || (isslavemode)) {
		outSISIDXREG(SISCR,0x13,(HDisplay1 & 0xFF));
		setSISIDXREG(SISSR,0x0E,0xF0,(HDisplay1 >> 8));
	}

	/* We must not set the pitch for CRT2 if bridge is in slave mode */
	if((ivideo->currentvbflags & VB_DISPTYPE_DISP2) && (!isslavemode)) {
		orSISIDXREG(SISPART1,ivideo->CRT2_write_enable,0x01);
		outSISIDXREG(SISPART1,0x07,(HDisplay2 & 0xFF));
		setSISIDXREG(SISPART1,0x09,0xF0,(HDisplay2 >> 8));
	}
}

static void
sisfb_bpp_to_var(struct sis_video_info *ivideo, struct fb_var_screeninfo *var)
{
	ivideo->video_cmap_len = sisfb_get_cmap_len(var);

	switch(var->bits_per_pixel) {
	case 8:
		var->red.offset = var->green.offset = var->blue.offset = 0;
		var->red.length = var->green.length = var->blue.length = 8;
		break;
	case 16:
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 32:
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		break;
	}
}

static int
sisfb_set_mode(struct sis_video_info *ivideo, int clrscrn)
{
	unsigned short modeno = ivideo->mode_no;

	/* >=2.6.12's fbcon clears the screen anyway */
	modeno |= 0x80;

	outSISIDXREG(SISSR, IND_SIS_PASSWORD, SIS_PASSWORD);

	sisfb_pre_setmode(ivideo);

	if(!SiSSetMode(&ivideo->SiS_Pr, modeno)) {
		printk(KERN_ERR "sisfb: Setting mode[0x%x] failed\n", ivideo->mode_no);
		return -EINVAL;
	}

	outSISIDXREG(SISSR, IND_SIS_PASSWORD, SIS_PASSWORD);

	sisfb_post_setmode(ivideo);

	return 0;
}


static int
sisfb_do_set_var(struct fb_var_screeninfo *var, int isactive, struct fb_info *info)
{
	struct sis_video_info *ivideo = (struct sis_video_info *)info->par;
	unsigned int htotal = 0, vtotal = 0;
	unsigned int drate = 0, hrate = 0;
	int found_mode = 0, ret;
	int old_mode;
	u32 pixclock;

	htotal = var->left_margin + var->xres + var->right_margin + var->hsync_len;

	vtotal = var->upper_margin + var->lower_margin + var->vsync_len;

	pixclock = var->pixclock;

	if((var->vmode & FB_VMODE_MASK) == FB_VMODE_NONINTERLACED) {
		vtotal += var->yres;
		vtotal <<= 1;
	} else if((var->vmode & FB_VMODE_MASK) == FB_VMODE_DOUBLE) {
		vtotal += var->yres;
		vtotal <<= 2;
	} else if((var->vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED) {
		vtotal += var->yres;
		vtotal <<= 1;
	} else 	vtotal += var->yres;

	if(!(htotal) || !(vtotal)) {
		DPRINTK("sisfb: Invalid 'var' information\n");
		return -EINVAL;
	}

	if(pixclock && htotal && vtotal) {
		drate = 1000000000 / pixclock;
		hrate = (drate * 1000) / htotal;
		ivideo->refresh_rate = (unsigned int) (hrate * 2 / vtotal);
	} else {
		ivideo->refresh_rate = 60;
	}

	old_mode = ivideo->sisfb_mode_idx;
	ivideo->sisfb_mode_idx = 0;

	while( (sisbios_mode[ivideo->sisfb_mode_idx].mode_no[0] != 0) &&
	       (sisbios_mode[ivideo->sisfb_mode_idx].xres <= var->xres) ) {
		if( (sisbios_mode[ivideo->sisfb_mode_idx].xres == var->xres) &&
		    (sisbios_mode[ivideo->sisfb_mode_idx].yres == var->yres) &&
		    (sisbios_mode[ivideo->sisfb_mode_idx].bpp == var->bits_per_pixel)) {
			ivideo->mode_no = sisbios_mode[ivideo->sisfb_mode_idx].mode_no[ivideo->mni];
			found_mode = 1;
			break;
		}
		ivideo->sisfb_mode_idx++;
	}

	if(found_mode) {
		ivideo->sisfb_mode_idx = sisfb_validate_mode(ivideo,
				ivideo->sisfb_mode_idx, ivideo->currentvbflags);
	} else {
		ivideo->sisfb_mode_idx = -1;
	}

       	if(ivideo->sisfb_mode_idx < 0) {
		printk(KERN_ERR "sisfb: Mode %dx%dx%d not supported\n", var->xres,
		       var->yres, var->bits_per_pixel);
		ivideo->sisfb_mode_idx = old_mode;
		return -EINVAL;
	}

	ivideo->mode_no = sisbios_mode[ivideo->sisfb_mode_idx].mode_no[ivideo->mni];

	if(sisfb_search_refresh_rate(ivideo, ivideo->refresh_rate, ivideo->sisfb_mode_idx) == 0) {
		ivideo->rate_idx = sisbios_mode[ivideo->sisfb_mode_idx].rate_idx;
		ivideo->refresh_rate = 60;
	}

	if(isactive) {
		/* If acceleration to be used? Need to know
		 * before pre/post_set_mode()
		 */
		ivideo->accel = 0;
#if defined(FBINFO_HWACCEL_DISABLED) && defined(FBINFO_HWACCEL_XPAN)
#ifdef STUPID_ACCELF_TEXT_SHIT
		if(var->accel_flags & FB_ACCELF_TEXT) {
			info->flags &= ~FBINFO_HWACCEL_DISABLED;
		} else {
			info->flags |= FBINFO_HWACCEL_DISABLED;
		}
#endif
		if(!(info->flags & FBINFO_HWACCEL_DISABLED)) ivideo->accel = -1;
#else
		if(var->accel_flags & FB_ACCELF_TEXT) ivideo->accel = -1;
#endif

		if((ret = sisfb_set_mode(ivideo, 1))) {
			return ret;
		}

		ivideo->video_bpp    = sisbios_mode[ivideo->sisfb_mode_idx].bpp;
		ivideo->video_width  = sisbios_mode[ivideo->sisfb_mode_idx].xres;
		ivideo->video_height = sisbios_mode[ivideo->sisfb_mode_idx].yres;

		sisfb_calc_pitch(ivideo, var);
		sisfb_set_pitch(ivideo);

		sisfb_set_vparms(ivideo);

		ivideo->current_width = ivideo->video_width;
		ivideo->current_height = ivideo->video_height;
		ivideo->current_bpp = ivideo->video_bpp;
		ivideo->current_htotal = htotal;
		ivideo->current_vtotal = vtotal;
		ivideo->current_linelength = ivideo->video_linelength;
		ivideo->current_pixclock = var->pixclock;
		ivideo->current_refresh_rate = ivideo->refresh_rate;
		ivideo->sisfb_lastrates[ivideo->mode_no] = ivideo->refresh_rate;
	}

	return 0;
}

static void
sisfb_set_base_CRT1(struct sis_video_info *ivideo, unsigned int base)
{
	outSISIDXREG(SISSR, IND_SIS_PASSWORD, SIS_PASSWORD);

	outSISIDXREG(SISCR, 0x0D, base & 0xFF);
	outSISIDXREG(SISCR, 0x0C, (base >> 8) & 0xFF);
	outSISIDXREG(SISSR, 0x0D, (base >> 16) & 0xFF);
	if(ivideo->sisvga_engine == SIS_315_VGA) {
		setSISIDXREG(SISSR, 0x37, 0xFE, (base >> 24) & 0x01);
	}
}

static void
sisfb_set_base_CRT2(struct sis_video_info *ivideo, unsigned int base)
{
	if(ivideo->currentvbflags & VB_DISPTYPE_DISP2) {
		orSISIDXREG(SISPART1, ivideo->CRT2_write_enable, 0x01);
		outSISIDXREG(SISPART1, 0x06, (base & 0xFF));
		outSISIDXREG(SISPART1, 0x05, ((base >> 8) & 0xFF));
		outSISIDXREG(SISPART1, 0x04, ((base >> 16) & 0xFF));
		if(ivideo->sisvga_engine == SIS_315_VGA) {
			setSISIDXREG(SISPART1, 0x02, 0x7F, ((base >> 24) & 0x01) << 7);
		}
	}
}

static int
sisfb_pan_var(struct sis_video_info *ivideo, struct fb_var_screeninfo *var)
{
	if(var->xoffset > (var->xres_virtual - var->xres)) {
		return -EINVAL;
	}
	if(var->yoffset > (var->yres_virtual - var->yres)) {
		return -EINVAL;
	}

	ivideo->current_base = (var->yoffset * var->xres_virtual) + var->xoffset;

	/* calculate base bpp dep. */
	switch(var->bits_per_pixel) {
	case 32:
		break;
	case 16:
		ivideo->current_base >>= 1;
		break;
	case 8:
	default:
		ivideo->current_base >>= 2;
		break;
	}

	ivideo->current_base += (ivideo->video_offset >> 2);

	sisfb_set_base_CRT1(ivideo, ivideo->current_base);
	sisfb_set_base_CRT2(ivideo, ivideo->current_base);

	return 0;
}

static int
sisfb_open(struct fb_info *info, int user)
{
	return 0;
}

static int
sisfb_release(struct fb_info *info, int user)
{
	return 0;
}

static int
sisfb_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue,
		unsigned transp, struct fb_info *info)
{
	struct sis_video_info *ivideo = (struct sis_video_info *)info->par;

	if(regno >= sisfb_get_cmap_len(&info->var))
		return 1;

	switch(info->var.bits_per_pixel) {
	case 8:
		outSISREG(SISDACA, regno);
		outSISREG(SISDACD, (red >> 10));
		outSISREG(SISDACD, (green >> 10));
		outSISREG(SISDACD, (blue >> 10));
		if(ivideo->currentvbflags & VB_DISPTYPE_DISP2) {
			outSISREG(SISDAC2A, regno);
			outSISREG(SISDAC2D, (red >> 8));
			outSISREG(SISDAC2D, (green >> 8));
			outSISREG(SISDAC2D, (blue >> 8));
		}
		break;
	case 16:
		if (regno >= 16)
			break;

		((u32 *)(info->pseudo_palette))[regno] =
				(red & 0xf800)          |
				((green & 0xfc00) >> 5) |
				((blue & 0xf800) >> 11);
		break;
	case 32:
		if (regno >= 16)
			break;

		red >>= 8;
		green >>= 8;
		blue >>= 8;
		((u32 *)(info->pseudo_palette))[regno] =
				(red << 16) | (green << 8) | (blue);
		break;
	}
	return 0;
}

static int
sisfb_set_par(struct fb_info *info)
{
	int err;

	if((err = sisfb_do_set_var(&info->var, 1, info)))
		return err;

	sisfb_get_fix(&info->fix, -1, info);

	return 0;
}

static int
sisfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct sis_video_info *ivideo = (struct sis_video_info *)info->par;
	unsigned int htotal = 0, vtotal = 0, myrateindex = 0;
	unsigned int drate = 0, hrate = 0, maxyres;
	int found_mode = 0;
	int refresh_rate, search_idx, tidx;
	bool recalc_clock = false;
	u32 pixclock;

	htotal = var->left_margin + var->xres + var->right_margin + var->hsync_len;

	vtotal = var->upper_margin + var->lower_margin + var->vsync_len;

	pixclock = var->pixclock;

	if((var->vmode & FB_VMODE_MASK) == FB_VMODE_NONINTERLACED) {
		vtotal += var->yres;
		vtotal <<= 1;
	} else if((var->vmode & FB_VMODE_MASK) == FB_VMODE_DOUBLE) {
		vtotal += var->yres;
		vtotal <<= 2;
	} else if((var->vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED) {
		vtotal += var->yres;
		vtotal <<= 1;
	} else
		vtotal += var->yres;

	if(!(htotal) || !(vtotal)) {
		SISFAIL("sisfb: no valid timing data");
	}

	search_idx = 0;
	while( (sisbios_mode[search_idx].mode_no[0] != 0) &&
	       (sisbios_mode[search_idx].xres <= var->xres) ) {
		if( (sisbios_mode[search_idx].xres == var->xres) &&
		    (sisbios_mode[search_idx].yres == var->yres) &&
		    (sisbios_mode[search_idx].bpp == var->bits_per_pixel)) {
			if((tidx = sisfb_validate_mode(ivideo, search_idx,
						ivideo->currentvbflags)) > 0) {
				found_mode = 1;
				search_idx = tidx;
				break;
			}
		}
		search_idx++;
	}

	if(!found_mode) {
		search_idx = 0;
		while(sisbios_mode[search_idx].mode_no[0] != 0) {
		   if( (var->xres <= sisbios_mode[search_idx].xres) &&
		       (var->yres <= sisbios_mode[search_idx].yres) &&
		       (var->bits_per_pixel == sisbios_mode[search_idx].bpp) ) {
			if((tidx = sisfb_validate_mode(ivideo,search_idx,
						ivideo->currentvbflags)) > 0) {
				found_mode = 1;
				search_idx = tidx;
				break;
			}
		   }
		   search_idx++;
		}
		if(found_mode) {
			printk(KERN_DEBUG
				"sisfb: Adapted from %dx%dx%d to %dx%dx%d\n",
				var->xres, var->yres, var->bits_per_pixel,
				sisbios_mode[search_idx].xres,
				sisbios_mode[search_idx].yres,
				var->bits_per_pixel);
			var->xres = sisbios_mode[search_idx].xres;
			var->yres = sisbios_mode[search_idx].yres;
		} else {
			printk(KERN_ERR
				"sisfb: Failed to find supported mode near %dx%dx%d\n",
				var->xres, var->yres, var->bits_per_pixel);
			return -EINVAL;
		}
	}

	if( ((ivideo->vbflags2 & VB2_LVDS) ||
	     ((ivideo->vbflags2 & VB2_30xBDH) && (ivideo->currentvbflags & CRT2_LCD))) &&
	    (var->bits_per_pixel == 8) ) {
		/* Slave modes on LVDS and 301B-DH */
		refresh_rate = 60;
		recalc_clock = true;
	} else if( (ivideo->current_htotal == htotal) &&
		   (ivideo->current_vtotal == vtotal) &&
		   (ivideo->current_pixclock == pixclock) ) {
		/* x=x & y=y & c=c -> assume depth change */
		drate = 1000000000 / pixclock;
		hrate = (drate * 1000) / htotal;
		refresh_rate = (unsigned int) (hrate * 2 / vtotal);
	} else if( ( (ivideo->current_htotal != htotal) ||
		     (ivideo->current_vtotal != vtotal) ) &&
		   (ivideo->current_pixclock == var->pixclock) ) {
		/* x!=x | y!=y & c=c -> invalid pixclock */
		if(ivideo->sisfb_lastrates[sisbios_mode[search_idx].mode_no[ivideo->mni]]) {
			refresh_rate =
				ivideo->sisfb_lastrates[sisbios_mode[search_idx].mode_no[ivideo->mni]];
		} else if(ivideo->sisfb_parm_rate != -1) {
			/* Sic, sisfb_parm_rate - want to know originally desired rate here */
			refresh_rate = ivideo->sisfb_parm_rate;
		} else {
			refresh_rate = 60;
		}
		recalc_clock = true;
	} else if((pixclock) && (htotal) && (vtotal)) {
		drate = 1000000000 / pixclock;
		hrate = (drate * 1000) / htotal;
		refresh_rate = (unsigned int) (hrate * 2 / vtotal);
	} else if(ivideo->current_refresh_rate) {
		refresh_rate = ivideo->current_refresh_rate;
		recalc_clock = true;
	} else {
		refresh_rate = 60;
		recalc_clock = true;
	}

	myrateindex = sisfb_search_refresh_rate(ivideo, refresh_rate, search_idx);

	/* Eventually recalculate timing and clock */
	if(recalc_clock) {
		if(!myrateindex) myrateindex = sisbios_mode[search_idx].rate_idx;
		var->pixclock = (u32) (1000000000 / sisfb_mode_rate_to_dclock(&ivideo->SiS_Pr,
						sisbios_mode[search_idx].mode_no[ivideo->mni],
						myrateindex));
		sisfb_mode_rate_to_ddata(&ivideo->SiS_Pr,
					sisbios_mode[search_idx].mode_no[ivideo->mni],
					myrateindex, var);
		if((var->vmode & FB_VMODE_MASK) == FB_VMODE_DOUBLE) {
			var->pixclock <<= 1;
		}
	}

	if(ivideo->sisfb_thismonitor.datavalid) {
		if(!sisfb_verify_rate(ivideo, &ivideo->sisfb_thismonitor, search_idx,
				myrateindex, refresh_rate)) {
			printk(KERN_INFO
				"sisfb: WARNING: Refresh rate exceeds monitor specs!\n");
		}
	}

	/* Adapt RGB settings */
	sisfb_bpp_to_var(ivideo, var);

	/* Sanity check for offsets */
	if(var->xoffset < 0) var->xoffset = 0;
	if(var->yoffset < 0) var->yoffset = 0;

	if(var->xres > var->xres_virtual)
		var->xres_virtual = var->xres;

	if(ivideo->sisfb_ypan) {
		maxyres = sisfb_calc_maxyres(ivideo, var);
		if(ivideo->sisfb_max) {
			var->yres_virtual = maxyres;
		} else {
			if(var->yres_virtual > maxyres) {
				var->yres_virtual = maxyres;
			}
		}
		if(var->yres_virtual <= var->yres) {
			var->yres_virtual = var->yres;
		}
	} else {
		if(var->yres != var->yres_virtual) {
			var->yres_virtual = var->yres;
		}
		var->xoffset = 0;
		var->yoffset = 0;
	}

	/* Truncate offsets to maximum if too high */
	if(var->xoffset > var->xres_virtual - var->xres) {
		var->xoffset = var->xres_virtual - var->xres - 1;
	}

	if(var->yoffset > var->yres_virtual - var->yres) {
		var->yoffset = var->yres_virtual - var->yres - 1;
	}

	/* Set everything else to 0 */
	var->red.msb_right =
		var->green.msb_right =
		var->blue.msb_right =
		var->transp.offset =
		var->transp.length =
		var->transp.msb_right = 0;

	return 0;
}

static int
sisfb_pan_display(struct fb_var_screeninfo *var, struct fb_info* info)
{
	struct sis_video_info *ivideo = (struct sis_video_info *)info->par;
	int err;

	if(var->xoffset > (var->xres_virtual - var->xres))
		return -EINVAL;

	if(var->yoffset > (var->yres_virtual - var->yres))
		return -EINVAL;

	if(var->vmode & FB_VMODE_YWRAP)
		return -EINVAL;

	if(var->xoffset + info->var.xres > info->var.xres_virtual ||
	   var->yoffset + info->var.yres > info->var.yres_virtual)
		return -EINVAL;

	if((err = sisfb_pan_var(ivideo, var)) < 0)
		return err;

	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;

	return 0;
}

static int
sisfb_blank(int blank, struct fb_info *info)
{
	struct sis_video_info *ivideo = (struct sis_video_info *)info->par;

	return sisfb_myblank(ivideo, blank);
}

/* ----------- FBDev related routines for all series ---------- */

static int	sisfb_ioctl(struct fb_info *info, unsigned int cmd,
			    unsigned long arg)
{
	struct sis_video_info	*ivideo = (struct sis_video_info *)info->par;
	struct sis_memreq	sismemreq;
	struct fb_vblank	sisvbblank;
	u32			gpu32 = 0;
#ifndef __user
#define __user
#endif
	u32 __user 		*argp = (u32 __user *)arg;

	switch(cmd) {
	   case FBIO_ALLOC:
		if(!capable(CAP_SYS_RAWIO))
			return -EPERM;

		if(copy_from_user(&sismemreq, (void __user *)arg, sizeof(sismemreq)))
			return -EFAULT;

		sis_malloc(&sismemreq);

		if(copy_to_user((void __user *)arg, &sismemreq, sizeof(sismemreq))) {
			sis_free((u32)sismemreq.offset);
			return -EFAULT;
		}
		break;

	   case FBIO_FREE:
		if(!capable(CAP_SYS_RAWIO))
			return -EPERM;

		if(get_user(gpu32, argp))
			return -EFAULT;

		sis_free(gpu32);
		break;

	   case FBIOGET_VBLANK:
		sisvbblank.count = 0;
		sisvbblank.flags = sisfb_setupvbblankflags(ivideo, &sisvbblank.vcount, &sisvbblank.hcount);

		if(copy_to_user((void __user *)arg, &sisvbblank, sizeof(sisvbblank)))
			return -EFAULT;

		break;

	   case SISFB_GET_INFO_SIZE:
		return put_user(sizeof(struct sisfb_info), argp);

	   case SISFB_GET_INFO_OLD:
		if(ivideo->warncount++ < 10)
			printk(KERN_INFO
				"sisfb: Deprecated ioctl call received - update your application!\n");
	   case SISFB_GET_INFO:  /* For communication with X driver */
		ivideo->sisfb_infoblock.sisfb_id         = SISFB_ID;
		ivideo->sisfb_infoblock.sisfb_version    = VER_MAJOR;
		ivideo->sisfb_infoblock.sisfb_revision   = VER_MINOR;
		ivideo->sisfb_infoblock.sisfb_patchlevel = VER_LEVEL;
		ivideo->sisfb_infoblock.chip_id = ivideo->chip_id;
		ivideo->sisfb_infoblock.sisfb_pci_vendor = ivideo->chip_vendor;
		ivideo->sisfb_infoblock.memory = ivideo->video_size / 1024;
		ivideo->sisfb_infoblock.heapstart = ivideo->heapstart / 1024;
		if(ivideo->modechanged) {
			ivideo->sisfb_infoblock.fbvidmode = ivideo->mode_no;
		} else {
			ivideo->sisfb_infoblock.fbvidmode = ivideo->modeprechange;
		}
		ivideo->sisfb_infoblock.sisfb_caps = ivideo->caps;
		ivideo->sisfb_infoblock.sisfb_tqlen = ivideo->cmdQueueSize / 1024;
		ivideo->sisfb_infoblock.sisfb_pcibus = ivideo->pcibus;
		ivideo->sisfb_infoblock.sisfb_pcislot = ivideo->pcislot;
		ivideo->sisfb_infoblock.sisfb_pcifunc = ivideo->pcifunc;
		ivideo->sisfb_infoblock.sisfb_lcdpdc = ivideo->detectedpdc;
		ivideo->sisfb_infoblock.sisfb_lcdpdca = ivideo->detectedpdca;
		ivideo->sisfb_infoblock.sisfb_lcda = ivideo->detectedlcda;
		ivideo->sisfb_infoblock.sisfb_vbflags = ivideo->vbflags;
		ivideo->sisfb_infoblock.sisfb_currentvbflags = ivideo->currentvbflags;
		ivideo->sisfb_infoblock.sisfb_scalelcd = ivideo->SiS_Pr.UsePanelScaler;
		ivideo->sisfb_infoblock.sisfb_specialtiming = ivideo->SiS_Pr.SiS_CustomT;
		ivideo->sisfb_infoblock.sisfb_haveemi = ivideo->SiS_Pr.HaveEMI ? 1 : 0;
		ivideo->sisfb_infoblock.sisfb_haveemilcd = ivideo->SiS_Pr.HaveEMILCD ? 1 : 0;
		ivideo->sisfb_infoblock.sisfb_emi30 = ivideo->SiS_Pr.EMI_30;
		ivideo->sisfb_infoblock.sisfb_emi31 = ivideo->SiS_Pr.EMI_31;
		ivideo->sisfb_infoblock.sisfb_emi32 = ivideo->SiS_Pr.EMI_32;
		ivideo->sisfb_infoblock.sisfb_emi33 = ivideo->SiS_Pr.EMI_33;
		ivideo->sisfb_infoblock.sisfb_tvxpos = (u16)(ivideo->tvxpos + 32);
		ivideo->sisfb_infoblock.sisfb_tvypos = (u16)(ivideo->tvypos + 32);
		ivideo->sisfb_infoblock.sisfb_heapsize = ivideo->sisfb_heap_size / 1024;
		ivideo->sisfb_infoblock.sisfb_videooffset = ivideo->video_offset;
		ivideo->sisfb_infoblock.sisfb_curfstn = ivideo->curFSTN;
		ivideo->sisfb_infoblock.sisfb_curdstn = ivideo->curDSTN;
		ivideo->sisfb_infoblock.sisfb_vbflags2 = ivideo->vbflags2;
		ivideo->sisfb_infoblock.sisfb_can_post = ivideo->sisfb_can_post ? 1 : 0;
		ivideo->sisfb_infoblock.sisfb_card_posted = ivideo->sisfb_card_posted ? 1 : 0;
		ivideo->sisfb_infoblock.sisfb_was_boot_device = ivideo->sisfb_was_boot_device ? 1 : 0;

		if(copy_to_user((void __user *)arg, &ivideo->sisfb_infoblock,
						sizeof(ivideo->sisfb_infoblock)))
			return -EFAULT;

	        break;

	   case SISFB_GET_VBRSTATUS_OLD:
		if(ivideo->warncount++ < 10)
			printk(KERN_INFO
				"sisfb: Deprecated ioctl call received - update your application!\n");
	   case SISFB_GET_VBRSTATUS:
		if(sisfb_CheckVBRetrace(ivideo))
			return put_user((u32)1, argp);
		else
			return put_user((u32)0, argp);

	   case SISFB_GET_AUTOMAXIMIZE_OLD:
		if(ivideo->warncount++ < 10)
			printk(KERN_INFO
				"sisfb: Deprecated ioctl call received - update your application!\n");
	   case SISFB_GET_AUTOMAXIMIZE:
		if(ivideo->sisfb_max)
			return put_user((u32)1, argp);
		else
			return put_user((u32)0, argp);

	   case SISFB_SET_AUTOMAXIMIZE_OLD:
		if(ivideo->warncount++ < 10)
			printk(KERN_INFO
				"sisfb: Deprecated ioctl call received - update your application!\n");
	   case SISFB_SET_AUTOMAXIMIZE:
		if(get_user(gpu32, argp))
			return -EFAULT;

		ivideo->sisfb_max = (gpu32) ? 1 : 0;
		break;

	   case SISFB_SET_TVPOSOFFSET:
		if(get_user(gpu32, argp))
			return -EFAULT;

		sisfb_set_TVxposoffset(ivideo, ((int)(gpu32 >> 16)) - 32);
		sisfb_set_TVyposoffset(ivideo, ((int)(gpu32 & 0xffff)) - 32);
		break;

	   case SISFB_GET_TVPOSOFFSET:
		return put_user((u32)(((ivideo->tvxpos+32)<<16)|((ivideo->tvypos+32)&0xffff)),
							argp);

	   case SISFB_COMMAND:
		if(copy_from_user(&ivideo->sisfb_command, (void __user *)arg,
							sizeof(struct sisfb_cmd)))
			return -EFAULT;

		sisfb_handle_command(ivideo, &ivideo->sisfb_command);

		if(copy_to_user((void __user *)arg, &ivideo->sisfb_command,
							sizeof(struct sisfb_cmd)))
			return -EFAULT;

		break;

	   case SISFB_SET_LOCK:
		if(get_user(gpu32, argp))
			return -EFAULT;

		ivideo->sisfblocked = (gpu32) ? 1 : 0;
		break;

	   default:
#ifdef SIS_NEW_CONFIG_COMPAT
		return -ENOIOCTLCMD;
#else
		return -EINVAL;
#endif
	}
	return 0;
}

static int
sisfb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	struct sis_video_info *ivideo = (struct sis_video_info *)info->par;

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));

	strcpy(fix->id, ivideo->myid);

	mutex_lock(&info->mm_lock);
	fix->smem_start  = ivideo->video_base + ivideo->video_offset;
	fix->smem_len    = ivideo->sisfb_mem;
	mutex_unlock(&info->mm_lock);
	fix->type        = FB_TYPE_PACKED_PIXELS;
	fix->type_aux    = 0;
	fix->visual      = (ivideo->video_bpp == 8) ? FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;
	fix->xpanstep    = 1;
	fix->ypanstep 	 = (ivideo->sisfb_ypan) ? 1 : 0;
	fix->ywrapstep   = 0;
	fix->line_length = ivideo->video_linelength;
	fix->mmio_start  = ivideo->mmio_base;
	fix->mmio_len    = ivideo->mmio_size;
	if(ivideo->sisvga_engine == SIS_300_VGA) {
		fix->accel = FB_ACCEL_SIS_GLAMOUR;
	} else if((ivideo->chip == SIS_330) ||
		  (ivideo->chip == SIS_760) ||
		  (ivideo->chip == SIS_761)) {
		fix->accel = FB_ACCEL_SIS_XABRE;
	} else if(ivideo->chip == XGI_20) {
		fix->accel = FB_ACCEL_XGI_VOLARI_Z;
	} else if(ivideo->chip >= XGI_40) {
		fix->accel = FB_ACCEL_XGI_VOLARI_V;
	} else {
		fix->accel = FB_ACCEL_SIS_GLAMOUR_2;
	}

	return 0;
}

/* ----------------  fb_ops structures ----------------- */

static struct fb_ops sisfb_ops = {
	.owner		= THIS_MODULE,
	.fb_open	= sisfb_open,
	.fb_release	= sisfb_release,
	.fb_check_var	= sisfb_check_var,
	.fb_set_par	= sisfb_set_par,
	.fb_setcolreg	= sisfb_setcolreg,
	.fb_pan_display	= sisfb_pan_display,
	.fb_blank	= sisfb_blank,
	.fb_fillrect	= fbcon_sis_fillrect,
	.fb_copyarea	= fbcon_sis_copyarea,
	.fb_imageblit	= cfb_imageblit,
#ifdef CONFIG_FB_SOFT_CURSOR
	.fb_cursor	= soft_cursor,
#endif
	.fb_sync	= fbcon_sis_sync,
#ifdef SIS_NEW_CONFIG_COMPAT
	.fb_compat_ioctl= sisfb_ioctl,
#endif
	.fb_ioctl	= sisfb_ioctl
};

/* ---------------- Chip generation dependent routines ---------------- */

static struct pci_dev * __devinit
sisfb_get_northbridge(int basechipid)
{
	struct pci_dev *pdev = NULL;
	int nbridgenum, nbridgeidx, i;
	static const unsigned short nbridgeids[] = {
		PCI_DEVICE_ID_SI_540,	/* for SiS 540 VGA */
		PCI_DEVICE_ID_SI_630,	/* for SiS 630/730 VGA */
		PCI_DEVICE_ID_SI_730,
		PCI_DEVICE_ID_SI_550,   /* for SiS 550 VGA */
		PCI_DEVICE_ID_SI_650,   /* for SiS 650/651/740 VGA */
		PCI_DEVICE_ID_SI_651,
		PCI_DEVICE_ID_SI_740,
		PCI_DEVICE_ID_SI_661,	/* for SiS 661/741/660/760/761 VGA */
		PCI_DEVICE_ID_SI_741,
		PCI_DEVICE_ID_SI_660,
		PCI_DEVICE_ID_SI_760,
		PCI_DEVICE_ID_SI_761
	};

	switch(basechipid) {
#ifdef CONFIG_FB_SIS_300
	case SIS_540:	nbridgeidx = 0; nbridgenum = 1; break;
	case SIS_630:	nbridgeidx = 1; nbridgenum = 2; break;
#endif
#ifdef CONFIG_FB_SIS_315
	case SIS_550:   nbridgeidx = 3; nbridgenum = 1; break;
	case SIS_650:	nbridgeidx = 4; nbridgenum = 3; break;
	case SIS_660:	nbridgeidx = 7; nbridgenum = 5; break;
#endif
	default:	return NULL;
	}
	for(i = 0; i < nbridgenum; i++) {
		if((pdev = pci_get_device(PCI_VENDOR_ID_SI,
				nbridgeids[nbridgeidx+i], NULL)))
			break;
	}
	return pdev;
}

static int __devinit
sisfb_get_dram_size(struct sis_video_info *ivideo)
{
#if defined(CONFIG_FB_SIS_300) || defined(CONFIG_FB_SIS_315)
	u8 reg;
#endif

	ivideo->video_size = 0;
	ivideo->UMAsize = ivideo->LFBsize = 0;

	switch(ivideo->chip) {
#ifdef CONFIG_FB_SIS_300
	case SIS_300:
		inSISIDXREG(SISSR, 0x14, reg);
		ivideo->video_size = ((reg & 0x3F) + 1) << 20;
		break;
	case SIS_540:
	case SIS_630:
	case SIS_730:
		if(!ivideo->nbridge)
			return -1;
		pci_read_config_byte(ivideo->nbridge, 0x63, &reg);
		ivideo->video_size = 1 << (((reg & 0x70) >> 4) + 21);
		break;
#endif
#ifdef CONFIG_FB_SIS_315
	case SIS_315H:
	case SIS_315PRO:
	case SIS_315:
		inSISIDXREG(SISSR, 0x14, reg);
		ivideo->video_size = (1 << ((reg & 0xf0) >> 4)) << 20;
		switch((reg >> 2) & 0x03) {
		case 0x01:
		case 0x03:
			ivideo->video_size <<= 1;
			break;
		case 0x02:
			ivideo->video_size += (ivideo->video_size/2);
		}
		break;
	case SIS_330:
		inSISIDXREG(SISSR, 0x14, reg);
		ivideo->video_size = (1 << ((reg & 0xf0) >> 4)) << 20;
		if(reg & 0x0c) ivideo->video_size <<= 1;
		break;
	case SIS_550:
	case SIS_650:
	case SIS_740:
		inSISIDXREG(SISSR, 0x14, reg);
		ivideo->video_size = (((reg & 0x3f) + 1) << 2) << 20;
		break;
	case SIS_661:
	case SIS_741:
		inSISIDXREG(SISCR, 0x79, reg);
		ivideo->video_size = (1 << ((reg & 0xf0) >> 4)) << 20;
		break;
	case SIS_660:
	case SIS_760:
	case SIS_761:
		inSISIDXREG(SISCR, 0x79, reg);
		reg = (reg & 0xf0) >> 4;
		if(reg)	{
			ivideo->video_size = (1 << reg) << 20;
			ivideo->UMAsize = ivideo->video_size;
		}
		inSISIDXREG(SISCR, 0x78, reg);
		reg &= 0x30;
		if(reg) {
			if(reg == 0x10) {
				ivideo->LFBsize = (32 << 20);
			} else {
				ivideo->LFBsize = (64 << 20);
			}
			ivideo->video_size += ivideo->LFBsize;
		}
		break;
	case SIS_340:
	case XGI_20:
	case XGI_40:
		inSISIDXREG(SISSR, 0x14, reg);
		ivideo->video_size = (1 << ((reg & 0xf0) >> 4)) << 20;
		if(ivideo->chip != XGI_20) {
			reg = (reg & 0x0c) >> 2;
			if(ivideo->revision_id == 2) {
				if(reg & 0x01) reg = 0x02;
				else	       reg = 0x00;
			}
			if(reg == 0x02)		ivideo->video_size <<= 1;
			else if(reg == 0x03)	ivideo->video_size <<= 2;
		}
		break;
#endif
	default:
		return -1;
	}
	return 0;
}

/* -------------- video bridge device detection --------------- */

static void __devinit
sisfb_detect_VB_connect(struct sis_video_info *ivideo)
{
	u8 cr32, temp;

	/* No CRT2 on XGI Z7 */
	if(ivideo->chip == XGI_20) {
		ivideo->sisfb_crt1off = 0;
		return;
	}

#ifdef CONFIG_FB_SIS_300
	if(ivideo->sisvga_engine == SIS_300_VGA) {
		inSISIDXREG(SISSR, 0x17, temp);
		if((temp & 0x0F) && (ivideo->chip != SIS_300)) {
			/* PAL/NTSC is stored on SR16 on such machines */
			if(!(ivideo->vbflags & (TV_PAL | TV_NTSC | TV_PALM | TV_PALN))) {
				inSISIDXREG(SISSR, 0x16, temp);
				if(temp & 0x20)
					ivideo->vbflags |= TV_PAL;
				else
					ivideo->vbflags |= TV_NTSC;
			}
		}
	}
#endif

	inSISIDXREG(SISCR, 0x32, cr32);

	if(cr32 & SIS_CRT1) {
		ivideo->sisfb_crt1off = 0;
	} else {
		ivideo->sisfb_crt1off = (cr32 & 0xDF) ? 1 : 0;
	}

	ivideo->vbflags &= ~(CRT2_TV | CRT2_LCD | CRT2_VGA);

	if(cr32 & SIS_VB_TV)   ivideo->vbflags |= CRT2_TV;
	if(cr32 & SIS_VB_LCD)  ivideo->vbflags |= CRT2_LCD;
	if(cr32 & SIS_VB_CRT2) ivideo->vbflags |= CRT2_VGA;

	/* Check given parms for hardware compatibility.
	 * (Cannot do this in the search_xx routines since we don't
	 * know what hardware we are running on then)
	 */

	if(ivideo->chip != SIS_550) {
	   ivideo->sisfb_dstn = ivideo->sisfb_fstn = 0;
	}

	if(ivideo->sisfb_tvplug != -1) {
	   if( (ivideo->sisvga_engine != SIS_315_VGA) ||
	       (!(ivideo->vbflags2 & VB2_SISYPBPRBRIDGE)) ) {
	      if(ivideo->sisfb_tvplug & TV_YPBPR) {
		 ivideo->sisfb_tvplug = -1;
		 printk(KERN_ERR "sisfb: YPbPr not supported\n");
	      }
	   }
	}
	if(ivideo->sisfb_tvplug != -1) {
	   if( (ivideo->sisvga_engine != SIS_315_VGA) ||
	       (!(ivideo->vbflags2 & VB2_SISHIVISIONBRIDGE)) ) {
	      if(ivideo->sisfb_tvplug & TV_HIVISION) {
		 ivideo->sisfb_tvplug = -1;
		 printk(KERN_ERR "sisfb: HiVision not supported\n");
	      }
	   }
	}
	if(ivideo->sisfb_tvstd != -1) {
	   if( (!(ivideo->vbflags2 & VB2_SISBRIDGE)) &&
	       (!((ivideo->sisvga_engine == SIS_315_VGA) &&
			(ivideo->vbflags2 & VB2_CHRONTEL))) ) {
	      if(ivideo->sisfb_tvstd & (TV_PALN | TV_PALN | TV_NTSCJ)) {
		 ivideo->sisfb_tvstd = -1;
		 printk(KERN_ERR "sisfb: PALM/PALN/NTSCJ not supported\n");
	      }
	   }
	}

	/* Detect/set TV plug & type */
	if(ivideo->sisfb_tvplug != -1) {
		ivideo->vbflags |= ivideo->sisfb_tvplug;
	} else {
		if(cr32 & SIS_VB_YPBPR)     	 ivideo->vbflags |= (TV_YPBPR|TV_YPBPR525I); /* default: 480i */
		else if(cr32 & SIS_VB_HIVISION)  ivideo->vbflags |= TV_HIVISION;
		else if(cr32 & SIS_VB_SCART)     ivideo->vbflags |= TV_SCART;
		else {
			if(cr32 & SIS_VB_SVIDEO)    ivideo->vbflags |= TV_SVIDEO;
			if(cr32 & SIS_VB_COMPOSITE) ivideo->vbflags |= TV_AVIDEO;
		}
	}

	if(!(ivideo->vbflags & (TV_YPBPR | TV_HIVISION))) {
	    if(ivideo->sisfb_tvstd != -1) {
	       ivideo->vbflags &= ~(TV_NTSC | TV_PAL | TV_PALM | TV_PALN | TV_NTSCJ);
	       ivideo->vbflags |= ivideo->sisfb_tvstd;
	    }
	    if(ivideo->vbflags & TV_SCART) {
	       ivideo->vbflags &= ~(TV_NTSC | TV_PALM | TV_PALN | TV_NTSCJ);
	       ivideo->vbflags |= TV_PAL;
	    }
	    if(!(ivideo->vbflags & (TV_PAL | TV_NTSC | TV_PALM | TV_PALN | TV_NTSCJ))) {
		if(ivideo->sisvga_engine == SIS_300_VGA) {
			inSISIDXREG(SISSR, 0x38, temp);
			if(temp & 0x01) ivideo->vbflags |= TV_PAL;
			else		ivideo->vbflags |= TV_NTSC;
		} else if((ivideo->chip <= SIS_315PRO) || (ivideo->chip >= SIS_330)) {
			inSISIDXREG(SISSR, 0x38, temp);
			if(temp & 0x01) ivideo->vbflags |= TV_PAL;
			else		ivideo->vbflags |= TV_NTSC;
		} else {
			inSISIDXREG(SISCR, 0x79, temp);
			if(temp & 0x20)	ivideo->vbflags |= TV_PAL;
			else		ivideo->vbflags |= TV_NTSC;
		}
	    }
	}

	/* Copy forceCRT1 option to CRT1off if option is given */
	if(ivideo->sisfb_forcecrt1 != -1) {
	   ivideo->sisfb_crt1off = (ivideo->sisfb_forcecrt1) ? 0 : 1;
	}
}

/* ------------------ Sensing routines ------------------ */

static bool __devinit
sisfb_test_DDC1(struct sis_video_info *ivideo)
{
    unsigned short old;
    int count = 48;

    old = SiS_ReadDDC1Bit(&ivideo->SiS_Pr);
    do {
	if(old != SiS_ReadDDC1Bit(&ivideo->SiS_Pr)) break;
    } while(count--);
    return (count != -1);
}

static void __devinit
sisfb_sense_crt1(struct sis_video_info *ivideo)
{
    bool mustwait = false;
    u8  sr1F, cr17;
#ifdef CONFIG_FB_SIS_315
    u8  cr63=0;
#endif
    u16 temp = 0xffff;
    int i;

    inSISIDXREG(SISSR,0x1F,sr1F);
    orSISIDXREG(SISSR,0x1F,0x04);
    andSISIDXREG(SISSR,0x1F,0x3F);
    if(sr1F & 0xc0) mustwait = true;

#ifdef CONFIG_FB_SIS_315
    if(ivideo->sisvga_engine == SIS_315_VGA) {
       inSISIDXREG(SISCR,ivideo->SiS_Pr.SiS_MyCR63,cr63);
       cr63 &= 0x40;
       andSISIDXREG(SISCR,ivideo->SiS_Pr.SiS_MyCR63,0xBF);
    }
#endif

    inSISIDXREG(SISCR,0x17,cr17);
    cr17 &= 0x80;
    if(!cr17) {
       orSISIDXREG(SISCR,0x17,0x80);
       mustwait = true;
       outSISIDXREG(SISSR, 0x00, 0x01);
       outSISIDXREG(SISSR, 0x00, 0x03);
    }

    if(mustwait) {
       for(i=0; i < 10; i++) sisfbwaitretracecrt1(ivideo);
    }

#ifdef CONFIG_FB_SIS_315
    if(ivideo->chip >= SIS_330) {
       andSISIDXREG(SISCR,0x32,~0x20);
       if(ivideo->chip >= SIS_340) {
          outSISIDXREG(SISCR, 0x57, 0x4a);
       } else {
          outSISIDXREG(SISCR, 0x57, 0x5f);
       }
       orSISIDXREG(SISCR, 0x53, 0x02);
       while((inSISREG(SISINPSTAT)) & 0x01)    break;
       while(!((inSISREG(SISINPSTAT)) & 0x01)) break;
       if((inSISREG(SISMISCW)) & 0x10) temp = 1;
       andSISIDXREG(SISCR, 0x53, 0xfd);
       andSISIDXREG(SISCR, 0x57, 0x00);
    }
#endif

    if(temp == 0xffff) {
       i = 3;
       do {
	  temp = SiS_HandleDDC(&ivideo->SiS_Pr, ivideo->vbflags,
		ivideo->sisvga_engine, 0, 0, NULL, ivideo->vbflags2);
       } while(((temp == 0) || (temp == 0xffff)) && i--);

       if((temp == 0) || (temp == 0xffff)) {
          if(sisfb_test_DDC1(ivideo)) temp = 1;
       }
    }

    if((temp) && (temp != 0xffff)) {
       orSISIDXREG(SISCR,0x32,0x20);
    }

#ifdef CONFIG_FB_SIS_315
    if(ivideo->sisvga_engine == SIS_315_VGA) {
       setSISIDXREG(SISCR,ivideo->SiS_Pr.SiS_MyCR63,0xBF,cr63);
    }
#endif

    setSISIDXREG(SISCR,0x17,0x7F,cr17);

    outSISIDXREG(SISSR,0x1F,sr1F);
}

/* Determine and detect attached devices on SiS30x */
static void __devinit
SiS_SenseLCD(struct sis_video_info *ivideo)
{
	unsigned char buffer[256];
	unsigned short temp, realcrtno, i;
	u8 reg, cr37 = 0, paneltype = 0;
	u16 xres, yres;

	ivideo->SiS_Pr.PanelSelfDetected = false;

	/* LCD detection only for TMDS bridges */
	if(!(ivideo->vbflags2 & VB2_SISTMDSBRIDGE))
		return;
	if(ivideo->vbflags2 & VB2_30xBDH)
		return;

	/* If LCD already set up by BIOS, skip it */
	inSISIDXREG(SISCR, 0x32, reg);
	if(reg & 0x08)
		return;

	realcrtno = 1;
	if(ivideo->SiS_Pr.DDCPortMixup)
		realcrtno = 0;

	/* Check DDC capabilities */
	temp = SiS_HandleDDC(&ivideo->SiS_Pr, ivideo->vbflags, ivideo->sisvga_engine,
				realcrtno, 0, &buffer[0], ivideo->vbflags2);

	if((!temp) || (temp == 0xffff) || (!(temp & 0x02)))
		return;

	/* Read DDC data */
	i = 3;  /* Number of retrys */
	do {
		temp = SiS_HandleDDC(&ivideo->SiS_Pr, ivideo->vbflags,
				ivideo->sisvga_engine, realcrtno, 1,
				&buffer[0], ivideo->vbflags2);
	} while((temp) && i--);

	if(temp)
		return;

	/* No digital device */
	if(!(buffer[0x14] & 0x80))
		return;

	/* First detailed timing preferred timing? */
	if(!(buffer[0x18] & 0x02))
		return;

	xres = buffer[0x38] | ((buffer[0x3a] & 0xf0) << 4);
	yres = buffer[0x3b] | ((buffer[0x3d] & 0xf0) << 4);

	switch(xres) {
		case 1024:
			if(yres == 768)
				paneltype = 0x02;
			break;
		case 1280:
			if(yres == 1024)
				paneltype = 0x03;
			break;
		case 1600:
			if((yres == 1200) && (ivideo->vbflags2 & VB2_30xC))
				paneltype = 0x0b;
			break;
	}

	if(!paneltype)
		return;

	if(buffer[0x23])
		cr37 |= 0x10;

	if((buffer[0x47] & 0x18) == 0x18)
		cr37 |= ((((buffer[0x47] & 0x06) ^ 0x06) << 5) | 0x20);
	else
		cr37 |= 0xc0;

	outSISIDXREG(SISCR, 0x36, paneltype);
	cr37 &= 0xf1;
	setSISIDXREG(SISCR, 0x37, 0x0c, cr37);
	orSISIDXREG(SISCR, 0x32, 0x08);

	ivideo->SiS_Pr.PanelSelfDetected = true;
}

static int __devinit
SISDoSense(struct sis_video_info *ivideo, u16 type, u16 test)
{
    int temp, mytest, result, i, j;

    for(j = 0; j < 10; j++) {
       result = 0;
       for(i = 0; i < 3; i++) {
          mytest = test;
          outSISIDXREG(SISPART4,0x11,(type & 0x00ff));
          temp = (type >> 8) | (mytest & 0x00ff);
          setSISIDXREG(SISPART4,0x10,0xe0,temp);
          SiS_DDC2Delay(&ivideo->SiS_Pr, 0x1500);
          mytest >>= 8;
          mytest &= 0x7f;
          inSISIDXREG(SISPART4,0x03,temp);
          temp ^= 0x0e;
          temp &= mytest;
          if(temp == mytest) result++;
#if 1
	  outSISIDXREG(SISPART4,0x11,0x00);
	  andSISIDXREG(SISPART4,0x10,0xe0);
	  SiS_DDC2Delay(&ivideo->SiS_Pr, 0x1000);
#endif
       }
       if((result == 0) || (result >= 2)) break;
    }
    return result;
}

static void __devinit
SiS_Sense30x(struct sis_video_info *ivideo)
{
    u8  backupP4_0d,backupP2_00,backupP2_4d,backupSR_1e,biosflag=0;
    u16 svhs=0, svhs_c=0;
    u16 cvbs=0, cvbs_c=0;
    u16 vga2=0, vga2_c=0;
    int myflag, result;
    char stdstr[] = "sisfb: Detected";
    char tvstr[]  = "TV connected to";

    if(ivideo->vbflags2 & VB2_301) {
       svhs = 0x00b9; cvbs = 0x00b3; vga2 = 0x00d1;
       inSISIDXREG(SISPART4,0x01,myflag);
       if(myflag & 0x04) {
	  svhs = 0x00dd; cvbs = 0x00ee; vga2 = 0x00fd;
       }
    } else if(ivideo->vbflags2 & (VB2_301B | VB2_302B)) {
       svhs = 0x016b; cvbs = 0x0174; vga2 = 0x0190;
    } else if(ivideo->vbflags2 & (VB2_301LV | VB2_302LV)) {
       svhs = 0x0200; cvbs = 0x0100;
    } else if(ivideo->vbflags2 & (VB2_301C | VB2_302ELV | VB2_307T | VB2_307LV)) {
       svhs = 0x016b; cvbs = 0x0110; vga2 = 0x0190;
    } else
       return;

    vga2_c = 0x0e08; svhs_c = 0x0404; cvbs_c = 0x0804;
    if(ivideo->vbflags & (VB2_301LV|VB2_302LV|VB2_302ELV|VB2_307LV)) {
       svhs_c = 0x0408; cvbs_c = 0x0808;
    }

    biosflag = 2;
    if(ivideo->haveXGIROM) {
       biosflag = ivideo->bios_abase[0x58] & 0x03;
    } else if(ivideo->newrom) {
       if(ivideo->bios_abase[0x5d] & 0x04) biosflag |= 0x01;
    } else if(ivideo->sisvga_engine == SIS_300_VGA) {
       if(ivideo->bios_abase) {
          biosflag = ivideo->bios_abase[0xfe] & 0x03;
       }
    }

    if(ivideo->chip == SIS_300) {
       inSISIDXREG(SISSR,0x3b,myflag);
       if(!(myflag & 0x01)) vga2 = vga2_c = 0;
    }

    if(!(ivideo->vbflags2 & VB2_SISVGA2BRIDGE)) {
       vga2 = vga2_c = 0;
    }

    inSISIDXREG(SISSR,0x1e,backupSR_1e);
    orSISIDXREG(SISSR,0x1e,0x20);

    inSISIDXREG(SISPART4,0x0d,backupP4_0d);
    if(ivideo->vbflags2 & VB2_30xC) {
       setSISIDXREG(SISPART4,0x0d,~0x07,0x01);
    } else {
       orSISIDXREG(SISPART4,0x0d,0x04);
    }
    SiS_DDC2Delay(&ivideo->SiS_Pr, 0x2000);

    inSISIDXREG(SISPART2,0x00,backupP2_00);
    outSISIDXREG(SISPART2,0x00,((backupP2_00 | 0x1c) & 0xfc));

    inSISIDXREG(SISPART2,0x4d,backupP2_4d);
    if(ivideo->vbflags2 & VB2_SISYPBPRBRIDGE) {
       outSISIDXREG(SISPART2,0x4d,(backupP2_4d & ~0x10));
    }

    if(!(ivideo->vbflags2 & VB2_30xCLV)) {
       SISDoSense(ivideo, 0, 0);
    }

    andSISIDXREG(SISCR, 0x32, ~0x14);

    if(vga2_c || vga2) {
       if(SISDoSense(ivideo, vga2, vga2_c)) {
          if(biosflag & 0x01) {
	     printk(KERN_INFO "%s %s SCART output\n", stdstr, tvstr);
	     orSISIDXREG(SISCR, 0x32, 0x04);
	  } else {
	     printk(KERN_INFO "%s secondary VGA connection\n", stdstr);
	     orSISIDXREG(SISCR, 0x32, 0x10);
	  }
       }
    }

    andSISIDXREG(SISCR, 0x32, 0x3f);

    if(ivideo->vbflags2 & VB2_30xCLV) {
       orSISIDXREG(SISPART4,0x0d,0x04);
    }

    if((ivideo->sisvga_engine == SIS_315_VGA) && (ivideo->vbflags2 & VB2_SISYPBPRBRIDGE)) {
       outSISIDXREG(SISPART2,0x4d,(backupP2_4d | 0x10));
       SiS_DDC2Delay(&ivideo->SiS_Pr, 0x2000);
       if((result = SISDoSense(ivideo, svhs, 0x0604))) {
          if((result = SISDoSense(ivideo, cvbs, 0x0804))) {
	     printk(KERN_INFO "%s %s YPbPr component output\n", stdstr, tvstr);
	     orSISIDXREG(SISCR,0x32,0x80);
	  }
       }
       outSISIDXREG(SISPART2,0x4d,backupP2_4d);
    }

    andSISIDXREG(SISCR, 0x32, ~0x03);

    if(!(ivideo->vbflags & TV_YPBPR)) {
       if((result = SISDoSense(ivideo, svhs, svhs_c))) {
          printk(KERN_INFO "%s %s SVIDEO output\n", stdstr, tvstr);
          orSISIDXREG(SISCR, 0x32, 0x02);
       }
       if((biosflag & 0x02) || (!result)) {
          if(SISDoSense(ivideo, cvbs, cvbs_c)) {
	     printk(KERN_INFO "%s %s COMPOSITE output\n", stdstr, tvstr);
	     orSISIDXREG(SISCR, 0x32, 0x01);
          }
       }
    }

    SISDoSense(ivideo, 0, 0);

    outSISIDXREG(SISPART2,0x00,backupP2_00);
    outSISIDXREG(SISPART4,0x0d,backupP4_0d);
    outSISIDXREG(SISSR,0x1e,backupSR_1e);

    if(ivideo->vbflags2 & VB2_30xCLV) {
       inSISIDXREG(SISPART2,0x00,biosflag);
       if(biosflag & 0x20) {
          for(myflag = 2; myflag > 0; myflag--) {
	     biosflag ^= 0x20;
	     outSISIDXREG(SISPART2,0x00,biosflag);
	  }
       }
    }

    outSISIDXREG(SISPART2,0x00,backupP2_00);
}

/* Determine and detect attached TV's on Chrontel */
static void __devinit
SiS_SenseCh(struct sis_video_info *ivideo)
{
#if defined(CONFIG_FB_SIS_300) || defined(CONFIG_FB_SIS_315)
    u8 temp1, temp2;
    char stdstr[] = "sisfb: Chrontel: Detected TV connected to";
#endif
#ifdef CONFIG_FB_SIS_300
    unsigned char test[3];
    int i;
#endif

    if(ivideo->chip < SIS_315H) {

#ifdef CONFIG_FB_SIS_300
       ivideo->SiS_Pr.SiS_IF_DEF_CH70xx = 1;		/* Chrontel 700x */
       SiS_SetChrontelGPIO(&ivideo->SiS_Pr, 0x9c);	/* Set general purpose IO for Chrontel communication */
       SiS_DDC2Delay(&ivideo->SiS_Pr, 1000);
       temp1 = SiS_GetCH700x(&ivideo->SiS_Pr, 0x25);
       /* See Chrontel TB31 for explanation */
       temp2 = SiS_GetCH700x(&ivideo->SiS_Pr, 0x0e);
       if(((temp2 & 0x07) == 0x01) || (temp2 & 0x04)) {
	  SiS_SetCH700x(&ivideo->SiS_Pr, 0x0e, 0x0b);
	  SiS_DDC2Delay(&ivideo->SiS_Pr, 300);
       }
       temp2 = SiS_GetCH700x(&ivideo->SiS_Pr, 0x25);
       if(temp2 != temp1) temp1 = temp2;

       if((temp1 >= 0x22) && (temp1 <= 0x50)) {
	   /* Read power status */
	   temp1 = SiS_GetCH700x(&ivideo->SiS_Pr, 0x0e);
	   if((temp1 & 0x03) != 0x03) {
		/* Power all outputs */
		SiS_SetCH700x(&ivideo->SiS_Pr, 0x0e,0x0b);
		SiS_DDC2Delay(&ivideo->SiS_Pr, 300);
	   }
	   /* Sense connected TV devices */
	   for(i = 0; i < 3; i++) {
	       SiS_SetCH700x(&ivideo->SiS_Pr, 0x10, 0x01);
	       SiS_DDC2Delay(&ivideo->SiS_Pr, 0x96);
	       SiS_SetCH700x(&ivideo->SiS_Pr, 0x10, 0x00);
	       SiS_DDC2Delay(&ivideo->SiS_Pr, 0x96);
	       temp1 = SiS_GetCH700x(&ivideo->SiS_Pr, 0x10);
	       if(!(temp1 & 0x08))       test[i] = 0x02;
	       else if(!(temp1 & 0x02))  test[i] = 0x01;
	       else                      test[i] = 0;
	       SiS_DDC2Delay(&ivideo->SiS_Pr, 0x96);
	   }

	   if(test[0] == test[1])      temp1 = test[0];
	   else if(test[0] == test[2]) temp1 = test[0];
	   else if(test[1] == test[2]) temp1 = test[1];
	   else {
		printk(KERN_INFO
			"sisfb: TV detection unreliable - test results varied\n");
		temp1 = test[2];
	   }
	   if(temp1 == 0x02) {
		printk(KERN_INFO "%s SVIDEO output\n", stdstr);
		ivideo->vbflags |= TV_SVIDEO;
		orSISIDXREG(SISCR, 0x32, 0x02);
		andSISIDXREG(SISCR, 0x32, ~0x05);
	   } else if (temp1 == 0x01) {
		printk(KERN_INFO "%s CVBS output\n", stdstr);
		ivideo->vbflags |= TV_AVIDEO;
		orSISIDXREG(SISCR, 0x32, 0x01);
		andSISIDXREG(SISCR, 0x32, ~0x06);
	   } else {
		SiS_SetCH70xxANDOR(&ivideo->SiS_Pr, 0x0e, 0x01, 0xF8);
		andSISIDXREG(SISCR, 0x32, ~0x07);
	   }
       } else if(temp1 == 0) {
	  SiS_SetCH70xxANDOR(&ivideo->SiS_Pr, 0x0e, 0x01, 0xF8);
	  andSISIDXREG(SISCR, 0x32, ~0x07);
       }
       /* Set general purpose IO for Chrontel communication */
       SiS_SetChrontelGPIO(&ivideo->SiS_Pr, 0x00);
#endif

    } else {

#ifdef CONFIG_FB_SIS_315
	ivideo->SiS_Pr.SiS_IF_DEF_CH70xx = 2;		/* Chrontel 7019 */
	temp1 = SiS_GetCH701x(&ivideo->SiS_Pr, 0x49);
	SiS_SetCH701x(&ivideo->SiS_Pr, 0x49, 0x20);
	SiS_DDC2Delay(&ivideo->SiS_Pr, 0x96);
	temp2 = SiS_GetCH701x(&ivideo->SiS_Pr, 0x20);
	temp2 |= 0x01;
	SiS_SetCH701x(&ivideo->SiS_Pr, 0x20, temp2);
	SiS_DDC2Delay(&ivideo->SiS_Pr, 0x96);
	temp2 ^= 0x01;
	SiS_SetCH701x(&ivideo->SiS_Pr, 0x20, temp2);
	SiS_DDC2Delay(&ivideo->SiS_Pr, 0x96);
	temp2 = SiS_GetCH701x(&ivideo->SiS_Pr, 0x20);
	SiS_SetCH701x(&ivideo->SiS_Pr, 0x49, temp1);
	temp1 = 0;
	if(temp2 & 0x02) temp1 |= 0x01;
	if(temp2 & 0x10) temp1 |= 0x01;
	if(temp2 & 0x04) temp1 |= 0x02;
	if( (temp1 & 0x01) && (temp1 & 0x02) ) temp1 = 0x04;
	switch(temp1) {
	case 0x01:
	     printk(KERN_INFO "%s CVBS output\n", stdstr);
	     ivideo->vbflags |= TV_AVIDEO;
	     orSISIDXREG(SISCR, 0x32, 0x01);
	     andSISIDXREG(SISCR, 0x32, ~0x06);
	     break;
	case 0x02:
	     printk(KERN_INFO "%s SVIDEO output\n", stdstr);
	     ivideo->vbflags |= TV_SVIDEO;
	     orSISIDXREG(SISCR, 0x32, 0x02);
	     andSISIDXREG(SISCR, 0x32, ~0x05);
	     break;
	case 0x04:
	     printk(KERN_INFO "%s SCART output\n", stdstr);
	     orSISIDXREG(SISCR, 0x32, 0x04);
	     andSISIDXREG(SISCR, 0x32, ~0x03);
	     break;
	default:
	     andSISIDXREG(SISCR, 0x32, ~0x07);
	}
#endif
    }
}

static void __devinit
sisfb_get_VB_type(struct sis_video_info *ivideo)
{
	char stdstr[]    = "sisfb: Detected";
	char bridgestr[] = "video bridge";
	u8 vb_chipid;
	u8 reg;

	/* No CRT2 on XGI Z7 */
	if(ivideo->chip == XGI_20)
		return;

	inSISIDXREG(SISPART4, 0x00, vb_chipid);
	switch(vb_chipid) {
	case 0x01:
		inSISIDXREG(SISPART4, 0x01, reg);
		if(reg < 0xb0) {
			ivideo->vbflags |= VB_301;	/* Deprecated */
			ivideo->vbflags2 |= VB2_301;
			printk(KERN_INFO "%s SiS301 %s\n", stdstr, bridgestr);
		} else if(reg < 0xc0) {
			ivideo->vbflags |= VB_301B;	/* Deprecated */
			ivideo->vbflags2 |= VB2_301B;
			inSISIDXREG(SISPART4,0x23,reg);
			if(!(reg & 0x02)) {
			   ivideo->vbflags |= VB_30xBDH;	/* Deprecated */
			   ivideo->vbflags2 |= VB2_30xBDH;
			   printk(KERN_INFO "%s SiS301B-DH %s\n", stdstr, bridgestr);
			} else {
			   printk(KERN_INFO "%s SiS301B %s\n", stdstr, bridgestr);
			}
		} else if(reg < 0xd0) {
			ivideo->vbflags |= VB_301C;	/* Deprecated */
			ivideo->vbflags2 |= VB2_301C;
			printk(KERN_INFO "%s SiS301C %s\n", stdstr, bridgestr);
		} else if(reg < 0xe0) {
			ivideo->vbflags |= VB_301LV;	/* Deprecated */
			ivideo->vbflags2 |= VB2_301LV;
			printk(KERN_INFO "%s SiS301LV %s\n", stdstr, bridgestr);
		} else if(reg <= 0xe1) {
			inSISIDXREG(SISPART4,0x39,reg);
			if(reg == 0xff) {
			   ivideo->vbflags |= VB_302LV;	/* Deprecated */
			   ivideo->vbflags2 |= VB2_302LV;
			   printk(KERN_INFO "%s SiS302LV %s\n", stdstr, bridgestr);
			} else {
			   ivideo->vbflags |= VB_301C;	/* Deprecated */
			   ivideo->vbflags2 |= VB2_301C;
			   printk(KERN_INFO "%s SiS301C(P4) %s\n", stdstr, bridgestr);
#if 0
			   ivideo->vbflags |= VB_302ELV;	/* Deprecated */
			   ivideo->vbflags2 |= VB2_302ELV;
			   printk(KERN_INFO "%s SiS302ELV %s\n", stdstr, bridgestr);
#endif
			}
		}
		break;
	case 0x02:
		ivideo->vbflags |= VB_302B;	/* Deprecated */
		ivideo->vbflags2 |= VB2_302B;
		printk(KERN_INFO "%s SiS302B %s\n", stdstr, bridgestr);
		break;
	}

	if((!(ivideo->vbflags2 & VB2_VIDEOBRIDGE)) && (ivideo->chip != SIS_300)) {
		inSISIDXREG(SISCR, 0x37, reg);
		reg &= SIS_EXTERNAL_CHIP_MASK;
		reg >>= 1;
		if(ivideo->sisvga_engine == SIS_300_VGA) {
#ifdef CONFIG_FB_SIS_300
			switch(reg) {
			   case SIS_EXTERNAL_CHIP_LVDS:
				ivideo->vbflags |= VB_LVDS;	/* Deprecated */
				ivideo->vbflags2 |= VB2_LVDS;
				break;
			   case SIS_EXTERNAL_CHIP_TRUMPION:
				ivideo->vbflags |= (VB_LVDS | VB_TRUMPION);	/* Deprecated */
				ivideo->vbflags2 |= (VB2_LVDS | VB2_TRUMPION);
				break;
			   case SIS_EXTERNAL_CHIP_CHRONTEL:
				ivideo->vbflags |= VB_CHRONTEL;	/* Deprecated */
				ivideo->vbflags2 |= VB2_CHRONTEL;
				break;
			   case SIS_EXTERNAL_CHIP_LVDS_CHRONTEL:
				ivideo->vbflags |= (VB_LVDS | VB_CHRONTEL);	/* Deprecated */
				ivideo->vbflags2 |= (VB2_LVDS | VB2_CHRONTEL);
				break;
			}
			if(ivideo->vbflags2 & VB2_CHRONTEL) ivideo->chronteltype = 1;
#endif
		} else if(ivideo->chip < SIS_661) {
#ifdef CONFIG_FB_SIS_315
			switch (reg) {
			   case SIS310_EXTERNAL_CHIP_LVDS:
				ivideo->vbflags |= VB_LVDS;	/* Deprecated */
				ivideo->vbflags2 |= VB2_LVDS;
				break;
			   case SIS310_EXTERNAL_CHIP_LVDS_CHRONTEL:
				ivideo->vbflags |= (VB_LVDS | VB_CHRONTEL);	/* Deprecated */
				ivideo->vbflags2 |= (VB2_LVDS | VB2_CHRONTEL);
				break;
			}
			if(ivideo->vbflags2 & VB2_CHRONTEL) ivideo->chronteltype = 2;
#endif
		} else if(ivideo->chip >= SIS_661) {
#ifdef CONFIG_FB_SIS_315
			inSISIDXREG(SISCR, 0x38, reg);
			reg >>= 5;
			switch(reg) {
			   case 0x02:
				ivideo->vbflags |= VB_LVDS;	/* Deprecated */
				ivideo->vbflags2 |= VB2_LVDS;
				break;
			   case 0x03:
				ivideo->vbflags |= (VB_LVDS | VB_CHRONTEL);	/* Deprecated */
				ivideo->vbflags2 |= (VB2_LVDS | VB2_CHRONTEL);
				break;
			   case 0x04:
				ivideo->vbflags |= (VB_LVDS | VB_CONEXANT);	/* Deprecated */
				ivideo->vbflags2 |= (VB2_LVDS | VB2_CONEXANT);
				break;
			}
			if(ivideo->vbflags2 & VB2_CHRONTEL) ivideo->chronteltype = 2;
#endif
		}
		if(ivideo->vbflags2 & VB2_LVDS) {
		   printk(KERN_INFO "%s LVDS transmitter\n", stdstr);
		}
		if((ivideo->sisvga_engine == SIS_300_VGA) && (ivideo->vbflags2 & VB2_TRUMPION)) {
		   printk(KERN_INFO "%s Trumpion Zurac LCD scaler\n", stdstr);
		}
		if(ivideo->vbflags2 & VB2_CHRONTEL) {
		   printk(KERN_INFO "%s Chrontel TV encoder\n", stdstr);
		}
		if((ivideo->chip >= SIS_661) && (ivideo->vbflags2 & VB2_CONEXANT)) {
		   printk(KERN_INFO "%s Conexant external device\n", stdstr);
		}
	}

	if(ivideo->vbflags2 & VB2_SISBRIDGE) {
		SiS_SenseLCD(ivideo);
		SiS_Sense30x(ivideo);
	} else if(ivideo->vbflags2 & VB2_CHRONTEL) {
		SiS_SenseCh(ivideo);
	}
}

/* ---------- Engine initialization routines ------------ */

static void
sisfb_engine_init(struct sis_video_info *ivideo)
{

	/* Initialize command queue (we use MMIO only) */

	/* BEFORE THIS IS CALLED, THE ENGINES *MUST* BE SYNC'ED */

	ivideo->caps &= ~(TURBO_QUEUE_CAP    |
			  MMIO_CMD_QUEUE_CAP |
			  VM_CMD_QUEUE_CAP   |
			  AGP_CMD_QUEUE_CAP);

#ifdef CONFIG_FB_SIS_300
	if(ivideo->sisvga_engine == SIS_300_VGA) {
		u32 tqueue_pos;
		u8 tq_state;

		tqueue_pos = (ivideo->video_size - ivideo->cmdQueueSize) / (64 * 1024);

		inSISIDXREG(SISSR, IND_SIS_TURBOQUEUE_SET, tq_state);
		tq_state |= 0xf0;
		tq_state &= 0xfc;
		tq_state |= (u8)(tqueue_pos >> 8);
		outSISIDXREG(SISSR, IND_SIS_TURBOQUEUE_SET, tq_state);

		outSISIDXREG(SISSR, IND_SIS_TURBOQUEUE_ADR, (u8)(tqueue_pos & 0xff));

		ivideo->caps |= TURBO_QUEUE_CAP;
	}
#endif

#ifdef CONFIG_FB_SIS_315
	if(ivideo->sisvga_engine == SIS_315_VGA) {
		u32 tempq = 0, templ;
		u8  temp;

		if(ivideo->chip == XGI_20) {
			switch(ivideo->cmdQueueSize) {
			case (64 * 1024):
				temp = SIS_CMD_QUEUE_SIZE_Z7_64k;
				break;
			case (128 * 1024):
			default:
				temp = SIS_CMD_QUEUE_SIZE_Z7_128k;
			}
		} else {
			switch(ivideo->cmdQueueSize) {
			case (4 * 1024 * 1024):
				temp = SIS_CMD_QUEUE_SIZE_4M;
				break;
			case (2 * 1024 * 1024):
				temp = SIS_CMD_QUEUE_SIZE_2M;
				break;
			case (1 * 1024 * 1024):
				temp = SIS_CMD_QUEUE_SIZE_1M;
				break;
			default:
			case (512 * 1024):
				temp = SIS_CMD_QUEUE_SIZE_512k;
			}
		}

		outSISIDXREG(SISSR, IND_SIS_CMDQUEUE_THRESHOLD, COMMAND_QUEUE_THRESHOLD);
		outSISIDXREG(SISSR, IND_SIS_CMDQUEUE_SET, SIS_CMD_QUEUE_RESET);

		if((ivideo->chip >= XGI_40) && ivideo->modechanged) {
			/* Must disable dual pipe on XGI_40. Can't do
			 * this in MMIO mode, because it requires
			 * setting/clearing a bit in the MMIO fire trigger
			 * register.
			 */
			if(!((templ = MMIO_IN32(ivideo->mmio_vbase, 0x8240)) & (1 << 10))) {

				MMIO_OUT32(ivideo->mmio_vbase, Q_WRITE_PTR, 0);

				outSISIDXREG(SISSR, IND_SIS_CMDQUEUE_SET, (temp | SIS_VRAM_CMDQUEUE_ENABLE));

				tempq = MMIO_IN32(ivideo->mmio_vbase, Q_READ_PTR);
				MMIO_OUT32(ivideo->mmio_vbase, Q_WRITE_PTR, tempq);

				tempq = (u32)(ivideo->video_size - ivideo->cmdQueueSize);
				MMIO_OUT32(ivideo->mmio_vbase, Q_BASE_ADDR, tempq);

				writel(0x16800000 + 0x8240, ivideo->video_vbase + tempq);
				writel(templ | (1 << 10), ivideo->video_vbase + tempq + 4);
				writel(0x168F0000, ivideo->video_vbase + tempq + 8);
				writel(0x168F0000, ivideo->video_vbase + tempq + 12);

				MMIO_OUT32(ivideo->mmio_vbase, Q_WRITE_PTR, (tempq + 16));

				sisfb_syncaccel(ivideo);

				outSISIDXREG(SISSR, IND_SIS_CMDQUEUE_SET, SIS_CMD_QUEUE_RESET);

			}
		}

		tempq = MMIO_IN32(ivideo->mmio_vbase, MMIO_QUEUE_READPORT);
		MMIO_OUT32(ivideo->mmio_vbase, MMIO_QUEUE_WRITEPORT, tempq);

		temp |= (SIS_MMIO_CMD_ENABLE | SIS_CMD_AUTO_CORR);
		outSISIDXREG(SISSR, IND_SIS_CMDQUEUE_SET, temp);

		tempq = (u32)(ivideo->video_size - ivideo->cmdQueueSize);
		MMIO_OUT32(ivideo->mmio_vbase, MMIO_QUEUE_PHYBASE, tempq);

		ivideo->caps |= MMIO_CMD_QUEUE_CAP;
	}
#endif

	ivideo->engineok = 1;
}

static void __devinit
sisfb_detect_lcd_type(struct sis_video_info *ivideo)
{
	u8 reg;
	int i;

	inSISIDXREG(SISCR, 0x36, reg);
	reg &= 0x0f;
	if(ivideo->sisvga_engine == SIS_300_VGA) {
		ivideo->CRT2LCDType = sis300paneltype[reg];
	} else if(ivideo->chip >= SIS_661) {
		ivideo->CRT2LCDType = sis661paneltype[reg];
	} else {
		ivideo->CRT2LCDType = sis310paneltype[reg];
		if((ivideo->chip == SIS_550) && (sisfb_fstn)) {
			if((ivideo->CRT2LCDType != LCD_320x240_2) &&
			   (ivideo->CRT2LCDType != LCD_320x240_3)) {
				ivideo->CRT2LCDType = LCD_320x240;
			}
		}
	}

	if(ivideo->CRT2LCDType == LCD_UNKNOWN) {
		/* For broken BIOSes: Assume 1024x768, RGB18 */
		ivideo->CRT2LCDType = LCD_1024x768;
		setSISIDXREG(SISCR,0x36,0xf0,0x02);
		setSISIDXREG(SISCR,0x37,0xee,0x01);
		printk(KERN_DEBUG "sisfb: Invalid panel ID (%02x), assuming 1024x768, RGB18\n", reg);
	}

	for(i = 0; i < SIS_LCD_NUMBER; i++) {
		if(ivideo->CRT2LCDType == sis_lcd_data[i].lcdtype) {
			ivideo->lcdxres = sis_lcd_data[i].xres;
			ivideo->lcdyres = sis_lcd_data[i].yres;
			ivideo->lcddefmodeidx = sis_lcd_data[i].default_mode_idx;
			break;
		}
	}

#ifdef CONFIG_FB_SIS_300
	if(ivideo->SiS_Pr.SiS_CustomT == CUT_BARCO1366) {
		ivideo->lcdxres = 1360; ivideo->lcdyres = 1024;
		ivideo->lcddefmodeidx = DEFAULT_MODE_1360;
	} else if(ivideo->SiS_Pr.SiS_CustomT == CUT_PANEL848) {
		ivideo->lcdxres =  848; ivideo->lcdyres =  480;
		ivideo->lcddefmodeidx = DEFAULT_MODE_848;
	} else if(ivideo->SiS_Pr.SiS_CustomT == CUT_PANEL856) {
		ivideo->lcdxres =  856; ivideo->lcdyres =  480;
		ivideo->lcddefmodeidx = DEFAULT_MODE_856;
	}
#endif

	printk(KERN_DEBUG "sisfb: Detected %dx%d flat panel\n",
			ivideo->lcdxres, ivideo->lcdyres);
}

static void __devinit
sisfb_save_pdc_emi(struct sis_video_info *ivideo)
{
#ifdef CONFIG_FB_SIS_300
	/* Save the current PanelDelayCompensation if the LCD is currently used */
	if(ivideo->sisvga_engine == SIS_300_VGA) {
		if(ivideo->vbflags2 & (VB2_LVDS | VB2_30xBDH)) {
			int tmp;
			inSISIDXREG(SISCR,0x30,tmp);
			if(tmp & 0x20) {
				/* Currently on LCD? If yes, read current pdc */
				inSISIDXREG(SISPART1,0x13,ivideo->detectedpdc);
				ivideo->detectedpdc &= 0x3c;
				if(ivideo->SiS_Pr.PDC == -1) {
					/* Let option override detection */
					ivideo->SiS_Pr.PDC = ivideo->detectedpdc;
				}
				printk(KERN_INFO "sisfb: Detected LCD PDC 0x%02x\n",
					ivideo->detectedpdc);
			}
			if((ivideo->SiS_Pr.PDC != -1) &&
			   (ivideo->SiS_Pr.PDC != ivideo->detectedpdc)) {
				printk(KERN_INFO "sisfb: Using LCD PDC 0x%02x\n",
					ivideo->SiS_Pr.PDC);
			}
		}
	}
#endif

#ifdef CONFIG_FB_SIS_315
	if(ivideo->sisvga_engine == SIS_315_VGA) {

		/* Try to find about LCDA */
		if(ivideo->vbflags2 & VB2_SISLCDABRIDGE) {
			int tmp;
			inSISIDXREG(SISPART1,0x13,tmp);
			if(tmp & 0x04) {
				ivideo->SiS_Pr.SiS_UseLCDA = true;
				ivideo->detectedlcda = 0x03;
			}
		}

		/* Save PDC */
		if(ivideo->vbflags2 & VB2_SISLVDSBRIDGE) {
			int tmp;
			inSISIDXREG(SISCR,0x30,tmp);
			if((tmp & 0x20) || (ivideo->detectedlcda != 0xff)) {
				/* Currently on LCD? If yes, read current pdc */
				u8 pdc;
				inSISIDXREG(SISPART1,0x2D,pdc);
				ivideo->detectedpdc  = (pdc & 0x0f) << 1;
				ivideo->detectedpdca = (pdc & 0xf0) >> 3;
				inSISIDXREG(SISPART1,0x35,pdc);
				ivideo->detectedpdc |= ((pdc >> 7) & 0x01);
				inSISIDXREG(SISPART1,0x20,pdc);
				ivideo->detectedpdca |= ((pdc >> 6) & 0x01);
				if(ivideo->newrom) {
					/* New ROM invalidates other PDC resp. */
					if(ivideo->detectedlcda != 0xff) {
						ivideo->detectedpdc = 0xff;
					} else {
						ivideo->detectedpdca = 0xff;
					}
				}
				if(ivideo->SiS_Pr.PDC == -1) {
					if(ivideo->detectedpdc != 0xff) {
						ivideo->SiS_Pr.PDC = ivideo->detectedpdc;
					}
				}
				if(ivideo->SiS_Pr.PDCA == -1) {
					if(ivideo->detectedpdca != 0xff) {
						ivideo->SiS_Pr.PDCA = ivideo->detectedpdca;
					}
				}
				if(ivideo->detectedpdc != 0xff) {
					printk(KERN_INFO
						"sisfb: Detected LCD PDC 0x%02x (for LCD=CRT2)\n",
						ivideo->detectedpdc);
				}
				if(ivideo->detectedpdca != 0xff) {
					printk(KERN_INFO
						"sisfb: Detected LCD PDC1 0x%02x (for LCD=CRT1)\n",
						ivideo->detectedpdca);
				}
			}

			/* Save EMI */
			if(ivideo->vbflags2 & VB2_SISEMIBRIDGE) {
				inSISIDXREG(SISPART4,0x30,ivideo->SiS_Pr.EMI_30);
				inSISIDXREG(SISPART4,0x31,ivideo->SiS_Pr.EMI_31);
				inSISIDXREG(SISPART4,0x32,ivideo->SiS_Pr.EMI_32);
				inSISIDXREG(SISPART4,0x33,ivideo->SiS_Pr.EMI_33);
				ivideo->SiS_Pr.HaveEMI = true;
				if((tmp & 0x20) || (ivideo->detectedlcda != 0xff)) {
					ivideo->SiS_Pr.HaveEMILCD = true;
				}
			}
		}

		/* Let user override detected PDCs (all bridges) */
		if(ivideo->vbflags2 & VB2_30xBLV) {
			if((ivideo->SiS_Pr.PDC != -1) &&
			   (ivideo->SiS_Pr.PDC != ivideo->detectedpdc)) {
				printk(KERN_INFO "sisfb: Using LCD PDC 0x%02x (for LCD=CRT2)\n",
					ivideo->SiS_Pr.PDC);
			}
			if((ivideo->SiS_Pr.PDCA != -1) &&
			   (ivideo->SiS_Pr.PDCA != ivideo->detectedpdca)) {
				printk(KERN_INFO "sisfb: Using LCD PDC1 0x%02x (for LCD=CRT1)\n",
				 ivideo->SiS_Pr.PDCA);
			}
		}

	}
#endif
}

/* -------------------- Memory manager routines ---------------------- */

static u32 __devinit
sisfb_getheapstart(struct sis_video_info *ivideo)
{
	u32 ret = ivideo->sisfb_parm_mem * 1024;
	u32 maxoffs = ivideo->video_size - ivideo->hwcursor_size - ivideo->cmdQueueSize;
	u32 def;

	/* Calculate heap start = end of memory for console
	 *
	 * CCCCCCCCDDDDDDDDDDDDDDDDDDDDDDDDDDDDHHHHQQQQQQQQQQ
	 * C = console, D = heap, H = HWCursor, Q = cmd-queue
	 *
	 * On 76x in UMA+LFB mode, the layout is as follows:
	 * DDDDDDDDDDDCCCCCCCCCCCCCCCCCCCCCCCCHHHHQQQQQQQQQQQ
	 * where the heap is the entire UMA area, eventually
	 * into the LFB area if the given mem parameter is
	 * higher than the size of the UMA memory.
	 *
	 * Basically given by "mem" parameter
	 *
	 * maximum = videosize - cmd_queue - hwcursor
	 *           (results in a heap of size 0)
	 * default = SiS 300: depends on videosize
	 *           SiS 315/330/340/XGI: 32k below max
	 */

	if(ivideo->sisvga_engine == SIS_300_VGA) {
		if(ivideo->video_size > 0x1000000) {
			def = 0xc00000;
		} else if(ivideo->video_size > 0x800000) {
			def = 0x800000;
		} else {
			def = 0x400000;
		}
	} else if(ivideo->UMAsize && ivideo->LFBsize) {
		ret = def = 0;
	} else {
		def = maxoffs - 0x8000;
	}

	/* Use default for secondary card for now (FIXME) */
	if((!ret) || (ret > maxoffs) || (ivideo->cardnumber != 0))
		ret = def;

	return ret;
}

static u32 __devinit
sisfb_getheapsize(struct sis_video_info *ivideo)
{
	u32 max = ivideo->video_size - ivideo->hwcursor_size - ivideo->cmdQueueSize;
	u32 ret = 0;

	if(ivideo->UMAsize && ivideo->LFBsize) {
		if( (!ivideo->sisfb_parm_mem)			||
		    ((ivideo->sisfb_parm_mem * 1024) > max)	||
		    ((max - (ivideo->sisfb_parm_mem * 1024)) < ivideo->UMAsize) ) {
			ret = ivideo->UMAsize;
			max -= ivideo->UMAsize;
		} else {
			ret = max - (ivideo->sisfb_parm_mem * 1024);
			max = ivideo->sisfb_parm_mem * 1024;
		}
		ivideo->video_offset = ret;
		ivideo->sisfb_mem = max;
	} else {
		ret = max - ivideo->heapstart;
		ivideo->sisfb_mem = ivideo->heapstart;
	}

	return ret;
}

static int __devinit
sisfb_heap_init(struct sis_video_info *ivideo)
{
	struct SIS_OH *poh;

	ivideo->video_offset = 0;
	if(ivideo->sisfb_parm_mem) {
		if( (ivideo->sisfb_parm_mem < (2 * 1024 * 1024)) ||
		    (ivideo->sisfb_parm_mem > ivideo->video_size) ) {
			ivideo->sisfb_parm_mem = 0;
		}
	}

	ivideo->heapstart = sisfb_getheapstart(ivideo);
	ivideo->sisfb_heap_size = sisfb_getheapsize(ivideo);

	ivideo->sisfb_heap_start = ivideo->video_vbase + ivideo->heapstart;
	ivideo->sisfb_heap_end   = ivideo->sisfb_heap_start + ivideo->sisfb_heap_size;

	printk(KERN_INFO "sisfb: Memory heap starting at %dK, size %dK\n",
		(int)(ivideo->heapstart / 1024), (int)(ivideo->sisfb_heap_size / 1024));

	ivideo->sisfb_heap.vinfo = ivideo;

	ivideo->sisfb_heap.poha_chain = NULL;
	ivideo->sisfb_heap.poh_freelist = NULL;

	poh = sisfb_poh_new_node(&ivideo->sisfb_heap);
	if(poh == NULL)
		return 1;

	poh->poh_next = &ivideo->sisfb_heap.oh_free;
	poh->poh_prev = &ivideo->sisfb_heap.oh_free;
	poh->size = ivideo->sisfb_heap_size;
	poh->offset = ivideo->heapstart;

	ivideo->sisfb_heap.oh_free.poh_next = poh;
	ivideo->sisfb_heap.oh_free.poh_prev = poh;
	ivideo->sisfb_heap.oh_free.size = 0;
	ivideo->sisfb_heap.max_freesize = poh->size;

	ivideo->sisfb_heap.oh_used.poh_next = &ivideo->sisfb_heap.oh_used;
	ivideo->sisfb_heap.oh_used.poh_prev = &ivideo->sisfb_heap.oh_used;
	ivideo->sisfb_heap.oh_used.size = SENTINEL;

	if(ivideo->cardnumber == 0) {
		/* For the first card, make this heap the "global" one
		 * for old DRM (which could handle only one card)
		 */
		sisfb_heap = &ivideo->sisfb_heap;
	}

	return 0;
}

static struct SIS_OH *
sisfb_poh_new_node(struct SIS_HEAP *memheap)
{
	struct SIS_OHALLOC	*poha;
	struct SIS_OH		*poh;
	unsigned long		cOhs;
	int			i;

	if(memheap->poh_freelist == NULL) {
		poha = kmalloc(SIS_OH_ALLOC_SIZE, GFP_KERNEL);
		if(!poha)
			return NULL;

		poha->poha_next = memheap->poha_chain;
		memheap->poha_chain = poha;

		cOhs = (SIS_OH_ALLOC_SIZE - sizeof(struct SIS_OHALLOC)) / sizeof(struct SIS_OH) + 1;

		poh = &poha->aoh[0];
		for(i = cOhs - 1; i != 0; i--) {
			poh->poh_next = poh + 1;
			poh = poh + 1;
		}

		poh->poh_next = NULL;
		memheap->poh_freelist = &poha->aoh[0];
	}

	poh = memheap->poh_freelist;
	memheap->poh_freelist = poh->poh_next;

	return poh;
}

static struct SIS_OH *
sisfb_poh_allocate(struct SIS_HEAP *memheap, u32 size)
{
	struct SIS_OH	*pohThis;
	struct SIS_OH	*pohRoot;
	int		bAllocated = 0;

	if(size > memheap->max_freesize) {
		DPRINTK("sisfb: Can't allocate %dk video memory\n",
			(unsigned int) size / 1024);
		return NULL;
	}

	pohThis = memheap->oh_free.poh_next;

	while(pohThis != &memheap->oh_free) {
		if(size <= pohThis->size) {
			bAllocated = 1;
			break;
		}
		pohThis = pohThis->poh_next;
	}

	if(!bAllocated) {
		DPRINTK("sisfb: Can't allocate %dk video memory\n",
			(unsigned int) size / 1024);
		return NULL;
	}

	if(size == pohThis->size) {
		pohRoot = pohThis;
		sisfb_delete_node(pohThis);
	} else {
		pohRoot = sisfb_poh_new_node(memheap);
		if(pohRoot == NULL)
			return NULL;

		pohRoot->offset = pohThis->offset;
		pohRoot->size = size;

		pohThis->offset += size;
		pohThis->size -= size;
	}

	memheap->max_freesize -= size;

	pohThis = &memheap->oh_used;
	sisfb_insert_node(pohThis, pohRoot);

	return pohRoot;
}

static void
sisfb_delete_node(struct SIS_OH *poh)
{
	poh->poh_prev->poh_next = poh->poh_next;
	poh->poh_next->poh_prev = poh->poh_prev;
}

static void
sisfb_insert_node(struct SIS_OH *pohList, struct SIS_OH *poh)
{
	struct SIS_OH *pohTemp = pohList->poh_next;

	pohList->poh_next = poh;
	pohTemp->poh_prev = poh;

	poh->poh_prev = pohList;
	poh->poh_next = pohTemp;
}

static struct SIS_OH *
sisfb_poh_free(struct SIS_HEAP *memheap, u32 base)
{
	struct SIS_OH *pohThis;
	struct SIS_OH *poh_freed;
	struct SIS_OH *poh_prev;
	struct SIS_OH *poh_next;
	u32    ulUpper;
	u32    ulLower;
	int    foundNode = 0;

	poh_freed = memheap->oh_used.poh_next;

	while(poh_freed != &memheap->oh_used) {
		if(poh_freed->offset == base) {
			foundNode = 1;
			break;
		}

		poh_freed = poh_freed->poh_next;
	}

	if(!foundNode)
		return NULL;

	memheap->max_freesize += poh_freed->size;

	poh_prev = poh_next = NULL;
	ulUpper = poh_freed->offset + poh_freed->size;
	ulLower = poh_freed->offset;

	pohThis = memheap->oh_free.poh_next;

	while(pohThis != &memheap->oh_free) {
		if(pohThis->offset == ulUpper) {
			poh_next = pohThis;
		} else if((pohThis->offset + pohThis->size) == ulLower) {
			poh_prev = pohThis;
		}
		pohThis = pohThis->poh_next;
	}

	sisfb_delete_node(poh_freed);

	if(poh_prev && poh_next) {
		poh_prev->size += (poh_freed->size + poh_next->size);
		sisfb_delete_node(poh_next);
		sisfb_free_node(memheap, poh_freed);
		sisfb_free_node(memheap, poh_next);
		return poh_prev;
	}

	if(poh_prev) {
		poh_prev->size += poh_freed->size;
		sisfb_free_node(memheap, poh_freed);
		return poh_prev;
	}

	if(poh_next) {
		poh_next->size += poh_freed->size;
		poh_next->offset = poh_freed->offset;
		sisfb_free_node(memheap, poh_freed);
		return poh_next;
	}

	sisfb_insert_node(&memheap->oh_free, poh_freed);

	return poh_freed;
}

static void
sisfb_free_node(struct SIS_HEAP *memheap, struct SIS_OH *poh)
{
	if(poh == NULL)
		return;

	poh->poh_next = memheap->poh_freelist;
	memheap->poh_freelist = poh;
}

static void
sis_int_malloc(struct sis_video_info *ivideo, struct sis_memreq *req)
{
	struct SIS_OH *poh = NULL;

	if((ivideo) && (ivideo->sisfb_id == SISFB_ID) && (!ivideo->havenoheap))
		poh = sisfb_poh_allocate(&ivideo->sisfb_heap, (u32)req->size);

	if(poh == NULL) {
		req->offset = req->size = 0;
		DPRINTK("sisfb: Video RAM allocation failed\n");
	} else {
		req->offset = poh->offset;
		req->size = poh->size;
		DPRINTK("sisfb: Video RAM allocation succeeded: 0x%lx\n",
			(poh->offset + ivideo->video_vbase));
	}
}

void
sis_malloc(struct sis_memreq *req)
{
	struct sis_video_info *ivideo = sisfb_heap->vinfo;

	if(&ivideo->sisfb_heap == sisfb_heap)
		sis_int_malloc(ivideo, req);
	else
		req->offset = req->size = 0;
}

void
sis_malloc_new(struct pci_dev *pdev, struct sis_memreq *req)
{
	struct sis_video_info *ivideo = pci_get_drvdata(pdev);

	sis_int_malloc(ivideo, req);
}

/* sis_free: u32 because "base" is offset inside video ram, can never be >4GB */

static void
sis_int_free(struct sis_video_info *ivideo, u32 base)
{
	struct SIS_OH *poh;

	if((!ivideo) || (ivideo->sisfb_id != SISFB_ID) || (ivideo->havenoheap))
		return;

	poh = sisfb_poh_free(&ivideo->sisfb_heap, base);

	if(poh == NULL) {
		DPRINTK("sisfb: sisfb_poh_free() failed at base 0x%x\n",
			(unsigned int) base);
	}
}

void
sis_free(u32 base)
{
	struct sis_video_info *ivideo = sisfb_heap->vinfo;

	sis_int_free(ivideo, base);
}

void
sis_free_new(struct pci_dev *pdev, u32 base)
{
	struct sis_video_info *ivideo = pci_get_drvdata(pdev);

	sis_int_free(ivideo, base);
}

/* --------------------- SetMode routines ------------------------- */

static void
sisfb_check_engine_and_sync(struct sis_video_info *ivideo)
{
	u8 cr30, cr31;

	/* Check if MMIO and engines are enabled,
	 * and sync in case they are. Can't use
	 * ivideo->accel here, as this might have
	 * been changed before this is called.
	 */
	inSISIDXREG(SISSR, IND_SIS_PCI_ADDRESS_SET, cr30);
	inSISIDXREG(SISSR, IND_SIS_MODULE_ENABLE, cr31);
	/* MMIO and 2D/3D engine enabled? */
	if((cr30 & SIS_MEM_MAP_IO_ENABLE) && (cr31 & 0x42)) {
#ifdef CONFIG_FB_SIS_300
		if(ivideo->sisvga_engine == SIS_300_VGA) {
			/* Don't care about TurboQueue. It's
			 * enough to know that the engines
			 * are enabled
			 */
			sisfb_syncaccel(ivideo);
		}
#endif
#ifdef CONFIG_FB_SIS_315
		if(ivideo->sisvga_engine == SIS_315_VGA) {
			/* Check that any queue mode is
			 * enabled, and that the queue
			 * is not in the state of "reset"
			 */
			inSISIDXREG(SISSR, 0x26, cr30);
			if((cr30 & 0xe0) && (!(cr30 & 0x01))) {
				sisfb_syncaccel(ivideo);
			}
		}
#endif
	}
}

static void
sisfb_pre_setmode(struct sis_video_info *ivideo)
{
	u8 cr30 = 0, cr31 = 0, cr33 = 0, cr35 = 0, cr38 = 0;
	int tvregnum = 0;

	ivideo->currentvbflags &= (VB_VIDEOBRIDGE | VB_DISPTYPE_DISP2);

	outSISIDXREG(SISSR, 0x05, 0x86);

	inSISIDXREG(SISCR, 0x31, cr31);
	cr31 &= ~0x60;
	cr31 |= 0x04;

	cr33 = ivideo->rate_idx & 0x0F;

#ifdef CONFIG_FB_SIS_315
	if(ivideo->sisvga_engine == SIS_315_VGA) {
	   if(ivideo->chip >= SIS_661) {
	      inSISIDXREG(SISCR, 0x38, cr38);
	      cr38 &= ~0x07;  /* Clear LCDA/DualEdge and YPbPr bits */
	   } else {
	      tvregnum = 0x38;
	      inSISIDXREG(SISCR, tvregnum, cr38);
	      cr38 &= ~0x3b;  /* Clear LCDA/DualEdge and YPbPr bits */
	   }
	}
#endif
#ifdef CONFIG_FB_SIS_300
	if(ivideo->sisvga_engine == SIS_300_VGA) {
	   tvregnum = 0x35;
	   inSISIDXREG(SISCR, tvregnum, cr38);
	}
#endif

	SiS_SetEnableDstn(&ivideo->SiS_Pr, false);
	SiS_SetEnableFstn(&ivideo->SiS_Pr, false);
	ivideo->curFSTN = ivideo->curDSTN = 0;

	switch(ivideo->currentvbflags & VB_DISPTYPE_DISP2) {

	   case CRT2_TV:
	      cr38 &= ~0xc0;   /* Clear PAL-M / PAL-N bits */
	      if((ivideo->vbflags & TV_YPBPR) && (ivideo->vbflags2 & VB2_SISYPBPRBRIDGE)) {
#ifdef CONFIG_FB_SIS_315
		 if(ivideo->chip >= SIS_661) {
		    cr38 |= 0x04;
		    if(ivideo->vbflags & TV_YPBPR525P)       cr35 |= 0x20;
		    else if(ivideo->vbflags & TV_YPBPR750P)  cr35 |= 0x40;
		    else if(ivideo->vbflags & TV_YPBPR1080I) cr35 |= 0x60;
		    cr30 |= SIS_SIMULTANEOUS_VIEW_ENABLE;
		    cr35 &= ~0x01;
		    ivideo->currentvbflags |= (TV_YPBPR | (ivideo->vbflags & TV_YPBPRALL));
		 } else if(ivideo->sisvga_engine == SIS_315_VGA) {
		    cr30 |= (0x80 | SIS_SIMULTANEOUS_VIEW_ENABLE);
		    cr38 |= 0x08;
		    if(ivideo->vbflags & TV_YPBPR525P)       cr38 |= 0x10;
		    else if(ivideo->vbflags & TV_YPBPR750P)  cr38 |= 0x20;
		    else if(ivideo->vbflags & TV_YPBPR1080I) cr38 |= 0x30;
		    cr31 &= ~0x01;
		    ivideo->currentvbflags |= (TV_YPBPR | (ivideo->vbflags & TV_YPBPRALL));
		 }
#endif
	      } else if((ivideo->vbflags & TV_HIVISION) &&
				(ivideo->vbflags2 & VB2_SISHIVISIONBRIDGE)) {
		 if(ivideo->chip >= SIS_661) {
		    cr38 |= 0x04;
		    cr35 |= 0x60;
		 } else {
		    cr30 |= 0x80;
		 }
		 cr30 |= SIS_SIMULTANEOUS_VIEW_ENABLE;
		 cr31 |= 0x01;
		 cr35 |= 0x01;
		 ivideo->currentvbflags |= TV_HIVISION;
	      } else if(ivideo->vbflags & TV_SCART) {
		 cr30 = (SIS_VB_OUTPUT_SCART | SIS_SIMULTANEOUS_VIEW_ENABLE);
		 cr31 |= 0x01;
		 cr35 |= 0x01;
		 ivideo->currentvbflags |= TV_SCART;
	      } else {
		 if(ivideo->vbflags & TV_SVIDEO) {
		    cr30 = (SIS_VB_OUTPUT_SVIDEO | SIS_SIMULTANEOUS_VIEW_ENABLE);
		    ivideo->currentvbflags |= TV_SVIDEO;
		 }
		 if(ivideo->vbflags & TV_AVIDEO) {
		    cr30 = (SIS_VB_OUTPUT_COMPOSITE | SIS_SIMULTANEOUS_VIEW_ENABLE);
		    ivideo->currentvbflags |= TV_AVIDEO;
		 }
	      }
	      cr31 |= SIS_DRIVER_MODE;

	      if(ivideo->vbflags & (TV_AVIDEO | TV_SVIDEO)) {
		 if(ivideo->vbflags & TV_PAL) {
		    cr31 |= 0x01; cr35 |= 0x01;
		    ivideo->currentvbflags |= TV_PAL;
		    if(ivideo->vbflags & TV_PALM) {
		       cr38 |= 0x40; cr35 |= 0x04;
		       ivideo->currentvbflags |= TV_PALM;
		    } else if(ivideo->vbflags & TV_PALN) {
		       cr38 |= 0x80; cr35 |= 0x08;
		       ivideo->currentvbflags |= TV_PALN;
		    }
		 } else {
		    cr31 &= ~0x01; cr35 &= ~0x01;
		    ivideo->currentvbflags |= TV_NTSC;
		    if(ivideo->vbflags & TV_NTSCJ) {
		       cr38 |= 0x40; cr35 |= 0x02;
		       ivideo->currentvbflags |= TV_NTSCJ;
		    }
		 }
	      }
	      break;

	   case CRT2_LCD:
	      cr30  = (SIS_VB_OUTPUT_LCD | SIS_SIMULTANEOUS_VIEW_ENABLE);
	      cr31 |= SIS_DRIVER_MODE;
	      SiS_SetEnableDstn(&ivideo->SiS_Pr, ivideo->sisfb_dstn);
	      SiS_SetEnableFstn(&ivideo->SiS_Pr, ivideo->sisfb_fstn);
	      ivideo->curFSTN = ivideo->sisfb_fstn;
	      ivideo->curDSTN = ivideo->sisfb_dstn;
	      break;

	   case CRT2_VGA:
	      cr30 = (SIS_VB_OUTPUT_CRT2 | SIS_SIMULTANEOUS_VIEW_ENABLE);
	      cr31 |= SIS_DRIVER_MODE;
	      if(ivideo->sisfb_nocrt2rate) {
		 cr33 |= (sisbios_mode[ivideo->sisfb_mode_idx].rate_idx << 4);
	      } else {
		 cr33 |= ((ivideo->rate_idx & 0x0F) << 4);
	      }
	      break;

	   default:	/* disable CRT2 */
	      cr30 = 0x00;
	      cr31 |= (SIS_DRIVER_MODE | SIS_VB_OUTPUT_DISABLE);
	}

	outSISIDXREG(SISCR, 0x30, cr30);
	outSISIDXREG(SISCR, 0x33, cr33);

	if(ivideo->chip >= SIS_661) {
#ifdef CONFIG_FB_SIS_315
	   cr31 &= ~0x01;                          /* Clear PAL flag (now in CR35) */
	   setSISIDXREG(SISCR, 0x35, ~0x10, cr35); /* Leave overscan bit alone */
	   cr38 &= 0x07;                           /* Use only LCDA and HiVision/YPbPr bits */
	   setSISIDXREG(SISCR, 0x38, 0xf8, cr38);
#endif
	} else if(ivideo->chip != SIS_300) {
	   outSISIDXREG(SISCR, tvregnum, cr38);
	}
	outSISIDXREG(SISCR, 0x31, cr31);

	ivideo->SiS_Pr.SiS_UseOEM = ivideo->sisfb_useoem;

	sisfb_check_engine_and_sync(ivideo);
}

/* Fix SR11 for 661 and later */
#ifdef CONFIG_FB_SIS_315
static void
sisfb_fixup_SR11(struct sis_video_info *ivideo)
{
	u8  tmpreg;

	if(ivideo->chip >= SIS_661) {
		inSISIDXREG(SISSR,0x11,tmpreg);
		if(tmpreg & 0x20) {
			inSISIDXREG(SISSR,0x3e,tmpreg);
			tmpreg = (tmpreg + 1) & 0xff;
			outSISIDXREG(SISSR,0x3e,tmpreg);
			inSISIDXREG(SISSR,0x11,tmpreg);
		}
		if(tmpreg & 0xf0) {
			andSISIDXREG(SISSR,0x11,0x0f);
		}
	}
}
#endif

static void
sisfb_set_TVxposoffset(struct sis_video_info *ivideo, int val)
{
	if(val > 32) val = 32;
	if(val < -32) val = -32;
	ivideo->tvxpos = val;

	if(ivideo->sisfblocked) return;
	if(!ivideo->modechanged) return;

	if(ivideo->currentvbflags & CRT2_TV) {

		if(ivideo->vbflags2 & VB2_CHRONTEL) {

			int x = ivideo->tvx;

			switch(ivideo->chronteltype) {
			case 1:
				x += val;
				if(x < 0) x = 0;
				outSISIDXREG(SISSR,0x05,0x86);
				SiS_SetCH700x(&ivideo->SiS_Pr, 0x0a, (x & 0xff));
				SiS_SetCH70xxANDOR(&ivideo->SiS_Pr, 0x08, ((x & 0x0100) >> 7), 0xFD);
				break;
			case 2:
				/* Not supported by hardware */
				break;
			}

		} else if(ivideo->vbflags2 & VB2_SISBRIDGE) {

			u8 p2_1f,p2_20,p2_2b,p2_42,p2_43;
			unsigned short temp;

			p2_1f = ivideo->p2_1f;
			p2_20 = ivideo->p2_20;
			p2_2b = ivideo->p2_2b;
			p2_42 = ivideo->p2_42;
			p2_43 = ivideo->p2_43;

			temp = p2_1f | ((p2_20 & 0xf0) << 4);
			temp += (val * 2);
			p2_1f = temp & 0xff;
			p2_20 = (temp & 0xf00) >> 4;
			p2_2b = ((p2_2b & 0x0f) + (val * 2)) & 0x0f;
			temp = p2_43 | ((p2_42 & 0xf0) << 4);
			temp += (val * 2);
			p2_43 = temp & 0xff;
			p2_42 = (temp & 0xf00) >> 4;
			outSISIDXREG(SISPART2,0x1f,p2_1f);
			setSISIDXREG(SISPART2,0x20,0x0F,p2_20);
			setSISIDXREG(SISPART2,0x2b,0xF0,p2_2b);
			setSISIDXREG(SISPART2,0x42,0x0F,p2_42);
			outSISIDXREG(SISPART2,0x43,p2_43);
		}
	}
}

static void
sisfb_set_TVyposoffset(struct sis_video_info *ivideo, int val)
{
	if(val > 32) val = 32;
	if(val < -32) val = -32;
	ivideo->tvypos = val;

	if(ivideo->sisfblocked) return;
	if(!ivideo->modechanged) return;

	if(ivideo->currentvbflags & CRT2_TV) {

	]/730[S],
 *5[E|PRO2 & VB2_CHRONTEL5x/[M]6	int y = F|M|G]X/tvyF|M|		switch[F|M|G]X/ hronteltypeX],
			case 1:els 	y -= val;14 anif(y < 0)V3XT/03
 *
 outSISIDXREG(SISSR,0x05,0x86) 2001-SiS_SetCH700x(&F|M|G]X/ustrPr, 0x0b, (y & 0xff)enna, Aram ia.2001xxANDOR This program is free8, (ftware;0100) >> 8) undFEu can rbreak 2001>= 2.2.
 *
 /* Not supported by hardterm */ Public License} fra} else 6x[F|M|G]X,
 *74x[GX]/330/SISBRIDGEXriver Xchar p2_01, * Th2 2001val /= 
 * itprogiXT/V5/V8, Zin thuree in t2e hope that it 2*rsiome ushe+=2.6. 2001e usefANTY; withoif(!er driver SiS 31[M]74x[]/5(TV_HIVISION | TV_YPBPR))ern14 anwhile((Y WARR<=righ|| . See2the
 *ICULAR PNY WARRANT
 * ithout even tre deta}er ver01-2005 Thomas WiniPART2hhof1,in thu can received a copy ofWARR GN2 Gene2u can}
	}
}

static void
sisfb_post_set*
 *(struct sis_|M|G]_info *F|M|G])
{
	bool crt1isoff = false;ace, Sudoit = true;
#if defined(CONFIG_FBany _3ong || Author:	ved a copy schh15)
	u8 reg;
#endifare
def r.net>ofer.net>
 ver homa1oralon(pr
 received a copy schrms 5t (CVral ractically wip300/code baseg wiFrfixup_SR11[F|M|G])iS (www.sise So
w we actutegraHAVE f
 * M0ng widisplay001-*undatF|M|G]X/ Orion and u= 1F|M|/* We can't e buff 0, BCRT1 if bridge it in slave* Originallhe named License,
 * or aVIDEOlatuld hasliedng wiFrer driis whic://chhow)1-1307, ton, MA n 2 ofn.de5/V8, Z7*
 */iterds fos0licon Integraree Systems,00 1998 Gerd Knsisvga_engine == y wiped_VGArlimodu>
inlock.h>
#i
#include ) && (e.h>on; eithnclu33de <liUSA
 *			reg<linx0) 200linux/ ing.#includ.h>
uxnclude <c., 5tring8#inclu
		seceived a copy Ch19991719997f,homau ca}S (www.son Integrated Systems, Incspinux/string.ab.h>
#includ/errno15i.h>
#include <lindistngpci.h>
#include <linmmpci.h>
#include <linscreen/fb.h>tring4nclud1*		Stringc/fs.h>
#includelabpci.h>
#include <linfde <pci.h>
#inch>
#includ>
#inclincludeelectionpci.h>
#r modify
 * it.ram MyCR63, ~ringx/ioe Fr.endif

#include "gincludfot, wcite toral Pci.h>
#include lude <linclude ranty of
 * MERCHANTABI= ~VB_DISPTYPE_ramethe *the Frcommand);

/* --|= VyrigNGLE_MODElude <lin_cmd l helper routines -----arms(void)
{
	srnel.rnalx[F|M|G]X/f
 * MERCHANTABILIe to Placthe F2ICULAR init the Frsetdefaultparms(voMIRROR */not, ess.h>
#inclul helper routinestparms(void)
{
	sisisfb_usam; if
	andm)reen_Copy
 * t INDrightRAMDAC_CONTROLot, w04ilic66x[F|M|G]X/f
 * MERCHANTABIL50,
 * X],
]6the named License,
 * or any .in-berlin* XGI License
 * along wit1f,,
 * but WI1fu can a		=re; y;ccel		=_scale20dalti-1 	= C20b_specialtiming 	= CUT_NONE;
	bhe Frlvdshl		b -1;
	sisfb_dstn		= 0;
	sisfb42isfb_lvdshl	4dify
 plugsfb_lvdshl	UT_Ntvstd	3	= 0;
	sisfb_3vxpo30, setltim;
	sisfb_tvypo01isfb_lvdshl	efo *iubcrt2>
#iate	= 
 * !Authot	= 0;
	sisfbodify
 * <linu.h>
namedeither v	sisfor aicen6e,
 *,
p.de>

#incver for Lclud k<lin1lic Liceope that7
x = ram Gribute prog modify
 * it undeau can rwrite to __d|= (E. Sb_ypan		= -arch_vesa*
 *(unsigne8)te (C)ral  G1) << 8 GI V= 0;

	/, bot (C)
{
	int i = 0, j = 0;

	/* We donsfb_tvpyet an>
 *
re quie PlacGI ViXT/V, jXT/V0[S]pliantdoraphknow are Fou1;
	sux/sternab_pmen-beparsitsfb_n-- Par/capabset_TV */
ocrt2rx = DEFA.h".h>
#iLT */

 *ivi_id_devDEFAULTy*/

0[S]	540/630[	}

othe;

	/ &timi1dff;  /* Clea(sisrA modompliEvenre ., 5sync >
#incsginal/capabiheckh>
#inc_and_de[iinux/modin	if( (Re-)Initialize chip-1].= 0;_*
 *idx = DEFAUaccelr flags */

;

	/)isfbmode[i++05x/[amode, boo)
{
	int>
#incok (Cncluif not, writGI V.h>
#ire	whiar Pub FoginalicludInc., 59 Temple Place>sfb_se/timi56 ||de[i++].0))
incl0/63.0 com=3)
		3)
	p boa

#incv1].mf(sisbios_ Incisfb_ode[i++].mode_no[f
 * MEx5a |de_no[1]nux/0de_no|
		2	  datibiosode) [i== v*
 *] == 0
 2 of {
0;_mode[i-1].e toe_no[1]handleer routi|
				   sisbios_mode[i-1].mods pr			   si.h>
 __iVESA mo&& !qudx = int my#includ			uffede[ix%x'\n",s yeta <linux/mdE;

	 as pSISFB_CMD_GETVBFLAGSblised wly, Incd o WARR VBEE;

		a{
	int i = 0, j 

	/*result[0]iverolrintkt)ERR_EARLY_useromsfb_lvdshdepth"sisfb>
#i InvalN_ERR "sis;
	
 *
 strbuf[1OKdef Cr *nameptr = name;

	/* We d1n't l helper routines ----n");

		(!quiet)
			pris no 0[S],2		= -]/73namicense,
rrtringe; eithermd thechar strbSWITCH Intblis/* arg[0]: 0 =rds ,#inclclud99 = query	   sintb: Inv, xres sisbiys_mode[MOr *nameptr = name;

	/* We don't know the hardw6],the har1[20];1] == 0x5= 0, rate = 0;
from _<lin99s = 0, /* Qstrnicmp(n!quiet)
			printk(KERN_ERR "sosfb: Interna/capability.h ? 0 :
			
 *
 *	= 0ptr =		= 0alid mode. Using defa 0, tware Fouspb_[S],
ra[20];4__)x%x'\ux/sE_INDEX_NONE].name, strlen(name))) {
		if(!quiet)
			priLOCKEDERN_ERR "		= 0;warrant alo||
	MERCHANTABIL;
	siENABLE)
#inou shib: Mos_m'none'mode* the Free anyion; eithr *nameptr = name;

	/* We don't know the hardwNOontiERN_ERR "si know t 19) {
		strcpy(strbuf1, name);
		for(i = 0; i < steot, write = x%x'\n""%u %u %u %u", &xres,trlenernaisbif( ude <linuxb_b_ma_meuf1); i+	sisfb_isfb_		=1
#incot, writeer <;
		es, s some fuzzy ios_mnANY gou sese		sisscanf * Fb
#inc! "%u res, "		= -1;
write to/

	while]/73s=not, write tfb_seaC] == 0x5b)
					ccludeodul either; rate = depth; depth = j;
			}
			sprintf(strbuTHERYou tailld have  flagsisfb_e_idx number from  */

;lags */

	whilerbuf;= strbuf;
				sp	sisf m/* mork;
		com (c) 1vesamodblisr *nameptr = name;

	/* We don't know the hardwUNKNOWN
			printk(KERNhard "x%x'\: Un defntoulvesa 0x%xisfb;
		<= 195x/[		strcpy			s
 *
	}
			si#ifnInteMODULEisfb_mode_i _	}
		es, yresetup(sfb_seopsisbse_no[	}
			this_optoid _(
			}
	 NULL, DEFms(ite alti!sisfb_mr <t!(	sisfb_mo		n 2 of {
<linPURPOSE.0x5b)
		os_mtrsep(&sisfb_m, ",")) !=isbi--- P *int ja 0x5b)
		)ystentincreen_i(strbfault.\[1] == 0x, "off", 3es, &r *na, &r"bios_ = 0, _videoramb)
					cont	nue;
			forcened(----:", 14uftinu%ue Soed uponux/veuite2tparmsfirstng --fstn/dstnlt.\n"nes 		s= 0, j>
#i = i [1] == 0x5+rbuf0x5b)	}
			-1].moA
 *			}ODULsisfb_tvb_se:",7e[i-1].mode_no_mod: Itvypof1, "%ssisfb79) {
	);f non(naicalr))) {ot, write tostandard:",1x%uxsisfb_get_nclue_idxfrom_kICULA	sisf1& !defr);
}

#ifndef MODULE
static voboolevi 5amepe <linux/typlfb_d56 ||kernel(void5,>
#inc
{
#ifdef CONFIG_X86
	char mymode[32= 0;/540/630[S]/73 (e <linux/t   (56 ||
i.mod_str
		ib_width >= 320)2 of MOD0e <lin
	if( (screen_inheight<= 2048e <l
	 = n0/540/630[S]/73ydepte_idx1] =os_m,if( (screen_inhe
	sistr, 536 ) {

	meptr);
}

#ifndef MODULE
static voyres);
	1- 1;

		s-1].mode_no, "%ux%,
 = (int)	sbios_f(my and t"%uth =%u10,

	if( (screen_inheigh,);
}


	if( (screen_imem:",			: In1;) myr *namememRR "s"sisfb: Using vga mode %4pre-set by kernel as default\n",
			mymode);
pdc/540= 0, rate = 0;
mdcUSA
)	whil of (p
gs */

	w

	/* We ot, write to] == 0x50 | i = 0, j ned(ux k(c	}
		*name= 24_mode(mdcatrue);
	}
#endif
	return;
}
#"pre-set by kernel as default\n",
			mymode);
nohere ", 
{
	int i n_infYPE_VLsisbio0r);
}

#ifndef MODULE
static voi].uf;
,pth == 24) mydeeo */[i]-info.lfb_width,
					screen_info.lfbnoypasfb_6rn;

	if( (screre [rt2type))isbiosatic vovideo */n;

is_cak;
		}= 0, rate = 0;
deo */[offs+;
	}

	ideo */pe[itvoffs_noMODULsimax 1 : 0;ypdx =MODULmaB) ri].flags;
			break;
		}
		i++;
	}

					fbisfb_mode_idx =_550_F & FL_550_DSTN) ? 1 : 0;
	sisfb_fstn rbuf1atick(KERstrb50_DSTNot, wr_DEBUG			bs does: Us
			vga) == 3%7pre-set by kernel as default\n",
			mymode);
useo	breacs yet and there iso, USAvypo(const ate; rate (KERN_ERR "sisepth = j;
			}
			sprintf(strbuf, "%uquietnonval1] =
}

ode(mym_ERR "p		ife,
	}

x5b)
		tr);
}

#ifndef MODULE
static voONE;
lcode( 9rn;

	if We dond long temp  * f(stri+", n}e);
	}
#endif
	return;
}
#9pre-set by ker
#incatic v ||
	GNU 	return;namepe <l  ;
			b	iv ? 1 :<lintic ^buf;
		)
		pdef CONFIG_X86
	char mymode[32];	= 0;

	/ 					isfb_mode
		patic vo#incluatic vovstd(const char *ame)
{
	int is3sisfb_dst== NULL)
		retu>= -32
#incl Usin<<= 1\n");f		  Boston, DEFAULT_MODde. Usi!= -1) {
		if(!strnicmp()
			printk(KERN_E				   s_m Internainux/sbior);
540/630[S]/73!efault.\n"en(si"%u %", 4gs;
			there i;
	sisfb_dstn	= 0;
FORCE	sisfb_sbios_mode[ih_tvstis does: Spe 0)
		}
			tes
abledERN_ER sisbios_m	PURPOSEmycustomttSpecycustomttable					fb_searchen_info.lfb_dN_INFOis does_kernel(void		= 0#ifdef CONFIG_X86
	char mymode[32= 0;
	rintk(KERN_ERstrnicmp(na4 not, write toyet and there is no isi++;
_tvtype[i].te[i].optionNa0)) {
				sisfb__DEBUG "sisfb: Sp	print[i].SpecialID;
				found =0x5b)
		ree >= '0'#incdx =,2.6.idux%u'9'_info.lfb_height >=  && (mydepth>,ude <wini
	sihomas@wi__i386__.type_pecia "sisx86_64__  si>=GNU <linode(mymo(KERN) 5x/[M]	5b)
cardrt2flags & F] == 0x5b)
t\"%x5b)
		deptl/* W= 8) && (mydepth <= 32) ) {

	 M|G]ra)
			 (g --%s %s)\n"1 : 0 *
 ble[i].optionName);
				brea	sisfb_dst(co (www.sbuf1[20];
	cha	}
			sisfb]rcedidx = 0] Ime, id sisfb_ %s.\n"{
				sisify
 *;nd = iSpecial timideofault\n||
				fb_f) retn) .h>
#i		}
	srom(k;
	b_fsoe, t*romde_no------ */VESsbios_mode[i-1].mode_no[dL)
		re	}
	%u",=; = 0}
romptr1].mode(readb(		stsbioDULEinfo5{
	inti, j0[S]/730[S	sis],
 *Siaaue;
			}
			sisfbsumool trnROMisbiostprint;
0x18) Use6;
		biosdate = ivid9; i < so dihard	ie + > (0x10000 -c;
	S_Pr.Virre iRomBas =ksumtprint;
Base +

	/* We OM) {
		)e[i]]!= 'P'Pr.>SiS_Pr.Virtt;
	ERR "'C24) mntk(K			printk(KER2i].chiIID == 			printk(KER3
		   R'eciachSiS_ +[i].ci];
	}

	i = 

	/video		chksum <= 1 0x2+)
	o {
er parsingip_vendorprint;sion))		contes, &30[S],
 *6r.UseR>SiS_P) {

7es, & tomttat.\nrlen(mycuidiosversion)) ||
dfound = 	mode[i-1].	ic Lius detecteoc., 59 0[S],
)
finde donnvalid pci >= 2*pdeve_no[
				   sisbios_mode[i-1].mode=iosven_indrvdata()))))us dDEBUG otbios_;
	de_nopecie)) ||
		     myromprintn 2 ofbios32].Speciasize_ersion(iv			sisfF, vel tiyupdatofficed iosv omttfunc= 0x5b(except
	
/* nde_idg1] =			pipsetude <lh hablenoe[i]).SiS_/ode_idxl helpern

#incX],
 * 6x[biosdate =	}
		smape don)))), &iS_Pr.URTlic tic v	}S_Pr.U{
	 We don
	if(ivi;
 "%ux%		j =			os_mo(k(KERpe[ibiovmalloc(65536DEFAULT->sSpeciWork ar "sisbug
			pci/rom.c: Folksintkgot ||
	LE
sntk(KE * whc.
 r    (SiS_/540rieicenfttab   (BIOS image tern	     (6;
		biois larg/730[anvideomappedtomttiosFootp  sisb bios(!sresource_lencisubsyPCI_ROM_RESOURCE) <SiS_Pr.Uiosveo->iS_Pr.Urintk(KtData[j]s;
			b
			iosdate))Boston, OUT AN		memcpy_+= iio 0; j < 5;,Pr.UseROMycust, "%ulstomtta> 1] =my ?mycuia :;
				}
	f(strbuf, "%ux%uprinunj < 5;p							S]/730[S]0;
	sisfb_pifSiS_Pr.SiS0SiS_/63 0; j < 5;			sisfOt siwise. U itvideo= NUble[inal way.dor =se:
 *		s@wiBase[= tru <ted a @wi0; j < 5;chi
	forRR "sissm/mt0c68; ;nicmp(<s]isfbf
				rlen(m+%mttabllinu0X],
 *  strlen(myciore			mios_,;
				loffmode_S]/730[S],ls >mode_idx = i  == s) receRN_WAR			}
			}
			ifUSA
 *		osFooified[S]/730[S]her v mode_idx erlinio: Inva_Pr.SiS j++false;
	ecialcuvga mfalse;S],
 *Pr.UseRPr.UC	prinT = 		printk(	}

nitor *mo0xff , u8 *; eithe
; j < 5 __include
Ident	}

_config_dwor   (				}
			}
	ADDRESSepthon'fer[prin-1].j) &ode[i-[4]		} e; y ||_devinr[5 || lfb_e named Liios_mprint&er[7] != 0x00) {_MASKif(b: Bad EDID hesero*/etec ivideoLE
static vode <	sisfb_specialtimi			printk(Kr( "si730[S],lude <l== 0x5sis_crb_ofpret_edid * Foundatilfb_hbios_mMODE_Iublifresh, index;
	u32  e*
 *s0[S]/73 != 0x00 || buf00r[7] != 0x01]r(i = buffer[7able] != 0x0_inf& 0x08)) {6 || buffer[7] != 0x07[0x14] & 5x/[nicmpcrt1ong(struc	printk(KERomttable;
			break;
	(ivideo->SiS_Pr.Uee Ssubsyvra& biosver			sisfb_sp This does: I   ((!mycetur*mapSiS_ {
		 
	   bu descmindx = isisfb_specialvx12[0x14] &1_inter				sisfb_sprced (riptor
	
		fux/eustomttaub  b: Invx36 %d nodetection ro.molfb_s ------!=abl_infoiptomaximum sdate,RAMr str	}
		sprioschk\n"custoes, &if(!( >>ype[i].-1].modsome fuzzy00 && buffe;] & 0ng -(i=0; i<4; irefreshi].bio0x00) f ivideAR x5b)
	onitorbios
	  ston,= buffer[j< (minterm2ODULE on; either (!(buhe named Li00) { != 0x0j +   0x5b)
	0uffe + 5]b[j + 9] *V * 13_devd	  monitode[i limimttato %dMBe[i].opp	vstd(	   bu->vmax>>] * 1
sisfb_s_mod
	if(spci.h>
#include <liundatio;

	iffer[0x13] >= 0x01) h>
#busheight
				   sisbios_mode[i-1].mode_no[bool footprintFBAddresch forb_off		0 && buffebiosve((!mycshorrnicmpmonitors are	prin(KERNdName	}

 0x0= ra__x86t	= 0;
	sis0x1x/sp9FBr->dor	}

	hNY W= 	if(5; to exte_idxs.co_rate		= -1;
	sis(Cturn /mtor->vmin = 

	if(!omBase[  4
	  BF= 0x0er ni40/63 i < 2buffer[j + ame: me);123 * thx23]j| moniPr.S4; j]ware ) 	de[i-wue;
			+ 8];* be] != 0x	}	}

w(
		  m[0x1urn;does 6 = r 5];
		to;
	   emodeclockmax = 3c
	   &sisfb_red(__ble[i].c)(struc) 19le[iretu0;
	   monreturnddcs0x18]0 &&hif(moockmax =	   monitor->v_ddcs)fffer[2bif(monitor->hmax < sisfb_ddcsmodesecia[i].h) monax <or->hmax < sisfbxff ++ming appliede[i-l(h) m234567L];
		  m[0x18]act omtta;ecia89ABL, < sisfb_ddif(bu[i].v) monitofb_dCDEF.v)clockmax = 0;if(mfisfb_ddcv;0 &&f(mov;
		 if(monitor12j + 9] f(monitor->hmax < si3bb_ddcsmodreturgh_vesamodSTN)fl] != l(isfb_ddd
		 if(mourn;
r->hmax < n > sprintk(4;if(mChannel A 128bit< 5; arams\ne
	   x2		  montor-fb_s;ffersisfb_ddciosversion)2;Speci!= 0x0inB 64+ 31) *		printk(KEe sync329) / 16	sisfb_mode_iomvideolistlinu the Free
	rwteststrbi=0; i<4; i++) 2: Search for  if(terat\"%_vesam  if(eEsi=0; * 4)PseudoRankCapacitys = xres;	 }AdrPinCount  if(eto extude  ra		  monntirely accu>
#i, + 8];*cause fixirturequench) =0xff s  Fou%u %usr1x;
	uto extract rk,  + 8];
	itor->Ptomtdes[j].x)BankNumHigh (DE_IND=Midif(;

		devinisfPhysInteAdrm& (yr&&
,.biositor->for->hmdcfsisfb_j].alfisfb		  [i-1].= NUL* to extra %u %u"strDRAMT? 1 17][5Baseturn{0x0CuthoAfb_res);
		0x39E
stp*
 *D if(smode1];
		  m48i].h) mon  if(9if(mon  2> sis5b_ddcsmodes[i]modesax <eturn44 < s
		 if(mon8hmax = sebuf(m1b_ddcsmodes[i]	    ax << sis40onitor->vmin >r->hmax <->hmaxsfb_ddcsmvNY W> Specides[max 32i].h) monB< sisfb_x = sodes[2  if(;
		  < sis     < sisfb_jx < sisfb_smode0;
	      4fb_dsmodes[j].ax = r->hmax << sis2x < s
		 imodessisfb_ddcs withUj	   for(i = ->hmax < sisfb_2itor->vmaxax = 0;
	  < si*
 *2>hmacux/sdes[i].v) m < simax < hmax < sisfb_}
	 ;
 < sis< si0tavaler[intk(e;
			 kN_WA16; k0; i <
		dcfmodes[j]. =res;+ 8];
impli].h) monitk][3]] == 0x5mp(named;
}

!=8];
		}dcfmodes[j].ef COth);
		[i].bFB) mode[ot, write to2] +d_info *ividee to0])i <  != 0x0ture  +				nfo *ivideo, str_ddcfmo_ddc =d
		      if( * 16datet:  ODE_ >=2[i].nito
}

statd=s_mo {er[7] !=		  m/*char  No< 5; j+ax <	     IG_M *
 r[j + 9]25	 ifata--- c bool __de550/[MLCDres, & realcrtnoearch
	 
	;
}

statd/ 2   2 of ihile	  monits, &ytrn1termreturn f0xff ||
1]) *se r  else d * th
		      if(v)) =b_ddcfmo_ddcPr.UHf((!jDDC(&i; j < 8;faulustomttaetur40/6+char  buffer[vags;% ) {

o->sisfideo->vbflags, &
		    b =0], ivideo->vlior, int crtno)
{
crtnocrtno, 0, &buff1j + 
}

static vmonitor->vmin = ) moga_eTes / 16 hmin > sisfb_ddcsmodesfailed\n",30[S]s */

	whisisfb_crr, int crtno)
{
 _0/630[S]tem)

	temp = =n;

	temp==(bufDC le
		 c|.h>
#incluvideoramarch foCRT%d2)velbuffa) ?.h>
#id;
}

stat 0;
	   emode mo) && (!crtno))
	4]rn;

nitor->hmported]",max < sddc(s __d!=s & 	break;
<ruct sfaul(i++;
& CR0) ? "FP	}

, x	(temp & 0x1rced (\"%s\
		(teC levv>ddc(strif(", &xr;

	bflags & CRT2_ "si3mode_nNumbeS (wwretrys}

	i->sisfdstrn			i++deo->vb_HandleDDC(&ustomttaPr.Use,j]     ==5[E|PROrtno, 1, &
#include <li,!i++;ion,SiS_Pr, ivid, realcrtno = crtnyncse[i-1* th< 5; jbuffer[((
		      if(h))realc,isbi] != 0+= 18;
DE_IN>=+->SiS_Pr, ivA) realcuffer[0])) rn;

\n",
			buffer[0x1!crtno-2" : "")b_dete>sis; i<4; i++) {
	p =vbflags2);
	if((osverH %d-%dKHz, V->vmin monMax.e)
{ags2);
		}
			sidMHz\n",
			monitor->hmi = 3;  /* Mcrtno))ror->hm/ tk(KERN_,none of the sulockm	   else rvaliemp) || 1000);
		 } else {
		    printk(KERN_INFO	ruct sis_ "",
	;

	eadgs, i|
				r->hmax < sisfblse {
		    printk(KERN_INFO iS 3(temddcsmodesC EDID ideo->vbxre].opetect_ that _tim(KERNse {
		!(bufferx13]ID !   for 0x40a(buffivideo->vsversion))))o* to extract rrys */
	;

	nvalid		my; j+[j + 3  sis_videomttab __inirlen(mycustomtint	

	i,r->datavaff ||
	c(struct sis_videoeor->dchort temp, i2" :3;  /* Numes, yre case 0x4es;	    t].chipID !ox23] | (6te)
!fou; i--uct sis_vtruct sis_video	 }
termi50; i 16D;
	 "silid)1; jisfb_htrSA
 *
	ort temp, i	 }
5 -lock(monito, 320x sisfes);
		<* j)= tr64, depth);bsynit
s >= 2.e va:ODE_I3;  /* tk(KE ej0x59:
	f->datavaic Li200,= 15xdcfmodes[j].xse 0x4fned short temp, i]ware)
{
2" :  0) {i- FPDI-2 x8", xres, yreID;
	;

	ifif noultparms(void)
{
 M		(t300on --------------- */		printk(KER< 5; j+s_video_info *ivideo, struct sisfb_monitord
	    * that tbioe= 480)) {
	clud"siVirtk(Kivid= 0; j 8= sis, v1, v2, v3, v4, v5, v6, v7, v8 1))16ature , rif(buffmemarms(v/630[ *ivideo, s < 8; ive+ 9] ]ue;

5b)
	monitorUse) {
r[2 = r|
		versii; monitor->vmax = 0;
	 des[99 Scrt1   priher flage) /.0 c0x52]terms8) ||
* ----ble[i].onitori < trbuf1[20];
	chas",
		(temr->hmax < siaz\n"i].cacusto}

/r->hm+ 1&me);
7 %u", &v3i.h>
#in v6i.h>
#incl "",
		(temrevisver_D;
	e)
{
real
		vinclud44; vefultd		tch(vhe Fue;

	 5found = rec bool __de;r->hm 68 sisbios_m3	caseAssume 125Mhz MCLK< 5; jfb_hfa.mode_} don'tate = 0;
* FoundatioEfrom the if(dclo0BUG "sif(bux < sisf+ 1* 5def COCONFIdef0
	if(Base5315_VGtic ionitosisvif++sfb_h	 2 of SIS_300>
#ir[j + ifividisbios_mode[myindex300_VG30[S],
 *
#in7cindex]rn fasbios_mode[myindex]t my CONFIG_FB_SIS_315
	if(h CONFIG_FB_SIS_315tavallockPDI-2" : "are;08) ?28n, Mr->dclockmax = 0;
	   emod29mode& MD_SIS315)r);
}sisfb_h-1;a
			endif

	myres = sisbios_mod) {
	endif

	myres = sisbios_modfgetor->dclockmax = 0;
	   emod30otale)
{
tic int1ci < mores, th);
		i0))
			a40/63{
		dclock = (htotal * mmod1s dortnoga_eDAC strrea ue;
none of the supported]"1!crtnf  monit(ividC, powrtuaable[ue;
tic int01de_no[mp =in-bivides1e;  *ivide2eturnt myin06ndex](hsy, crv7_Pr.Undex]8<as]\n"rr.DE_I320xton,  Thomas urn;;af

	_(30[S],
 *s_mode[sfb_h (mo
			}
			sisfmy 8cialrearamustod = fals	sw16tch(sif(ivs_mode[myindex]2fer[0S]/730[S],
e[myindex]3isfb_hinux/sbiose[myindex]40e)) || (ivls >= 2.((ss: mybuffs_videoE_FSTN_16; brea5ode_i== 3)ivideopth;ONFIG_es = id)
+ 1)					b

	i)ftor-none of the supported]"totaivideo->s (ivRamf((!j)(a vbf
			0,s			scdex] step 8)< 5; none of the supported]"lfrothe hard	h(si"D&P
			}
			sis <yture ]{
			if(se buff(5[E|P1uston", crtno + 1); 0;
	   emode0x5ags &:
for(j =[j]     ==lcdx1			sntk(K>= 2.50/[M]6	casif(Pr.Ubpth;r->dclockmax = 0;
	   emodecf(sis;bflaultparm< 5;  probing failed\n", crtn ,0xrt2tyt else {
	   printk(KERN_INFO "s5;, yres, myresif(0))
			r3, Max.videonito else {
	   printk(KER9s_modto disa 0xff >ios_m)4	 caer,
				stpedestal (	}
			se5, yresE
staLE
staE
stdeo->si_engiGetuf1,I1a) ? " will_TV(ivideo->sisvga_engi50/[ endif

	myres = sisbios_mod0!crtais doomttablinear & relosisfb(io &tes
2_deva				i 5; tic intfs_mosbios_0dlvdshcasetn_interp|
				   s == 320) Sie;
					s == 240)0xe9
							}
			si0xer->dc			f

	myres = sisbios_mod) || endif

	myres = sisbios_modis_c		 	ivideo->SiS_Pr.SiS_Custos, sxres, yres, ivideo->vbflagsg6gs;
8----------------25x/[M]>= 2.;
	   }
	 onitor->hm   emodh>
#in3	 strusfb_vrin > sisfb_ddcdeor	if((e((sddcsmoomttabun15
	if0_VG{
			ufferga_engine,s[j].== r0 u8
sif(sistic int 
	insbios_1].op

	while(sis_tv	u16ios_modc= 240) {
					de_ibS_Pr.SiS_Customefresh)) {sisbiS_P|
		ivid[m xres, yres,fb_vrate[i].i5[E|PRO2) 2arms(4r[j +vrate[i].yre\"%s\"f tte5x/[no + 1)sisi];
	}ge_idx		customefresspecialt>
#i0 &&>SiS_Pr,->Sigs;
			bidify
 lic License
 * along we)) ||
		1crn;

 *ivideo
	in sisbio
	case hsyif(Siting rate fr	  &hs, sl, &vbflags== 0r[j + 0xf5S_315S]/730[S],
 donude <linux/sbios_xf7index](!i].itor->fRINTK(n 2 o				ivdpyrix14)= sivideoode_no[video-== vr Adjk;

						ivideo->rate_idx = simodigs2) D					ivideo->rate_idx = sisfe = sn(mycuvideo->ritor->fD;
	false;1b_ddcsmodeideo->vbflaGbfb_vratereak;
			} else if((rate	if(ddcsmodese;

	>rate.h>
#incl	casei!defin; either v				a	}
	b: Adju
			 			iv[E|PA
 *nfo *iv			ivideif(Site) <= 3) {
					DP, yres3|
		 of 
					sxres, yres,;
				brea				iax <indexL 0) 1].m				p=+ 1;onitor->hmax < sitn,
oid sispth;r[j dc3rate_idx >refresh_rate = sfb_ddcsmodvideo->refresh_rate = 8d;
}

statycus if((r don't k= true;
				printk(KERg paramp		DPRI>= 2omttabl((rate S],
 *itor->f_r > sisfb_ddc28| (buff?< 5; jb.h>
#(ffer[0x13] >= 0x = 00ode[ixres "tcloc[%u %along wi the Free]"4 void sishtno + 1)tparms(vosfb_e)) ||

		   b FB sisf  strbind		 	ils.abwitcuct sisf  5; jsisfb_h;

	4+ 9] *p) || ------ Moj + 8]/* SIS_3: I&   if(b clock,
	rate[i].yre[j + 9] * 19]256]0:
		if(ividdetk(KER sisx		= -sisfb_ht
					0xff ||ni],
			00_VGA)te)
{trbuf1[20];
	chadetection ro_tvst(sisfb_v  elsFail CONFIG_FB_emory buff;
		  break;
	, esvideo 	800 && !defiate, sisfb_vrate,mycusto30[S],
 *\n",
			 &= SIS_
		(temp & 0x08) ? "D&Pb47| (buff8MBs pr9f(dcsbios_igned }nfo *ivideo, s < 8; ier driis whicvbflags)
{
	vid	}
}

stres[j].>> 0) {
	sh, inde== 2e_idx == 3) {
					ialI,c bool __devfb_v0))
			/ fresh)
	caso->rate_idxue;

	if(30case 0xfbwags;
		Pr.SiS_CundexPCI< 5; j++video-9gs;
			b_sear= namsigned acecrtAGPideo->SiS_Pr.Ub, u*namei].idx;
				break;
			}
		if(sivideo->rate_idx = 0;
	whilex12] != 0x/* Ser vT ANebRN_ERR "1 sat: Invaui].chipID !) {
See_idx1deo)b_se, doraphclr->fe <li_mode[;  /* Clmonitors_maUseOEM.h>
#includedistriE eo_iDstnarch_vesamode(unsi (mydepthowretracecrteFvideo->SiS_Pr.Uce) ) {
		bool l helper roFSTNfb: Internae =Dos_modidx)
 namcdyre(sisfb_veoMmyindSonitor8m %d downSiSSetM6 ||rch_vesamode(unsign2e	/* S8	}
}
lse;
0))
			= (fresh) * v;
	if(teratsignssuppor[1] refresh > sisfb_ddcsmodes - sgs2);
	&& --table[is (
				}
			CR3modeso->rate_idx ].idxate = srefresid m- n ivetb: EDyG(SI
	   whaintk(KE
 * MEh is (cf(sisb)= 0, xres =ak;

 * M %d dostati,0x00,P1on Integrated Systems, Inycust aRN_INFx5a: xres) [i].sisvga_signed c1533#include <linux/sbios15>
#i50) ODOODE_INivideo)oundatio	}
}

/* -------------- Moxgomttlay
		r.y) &&* 3f(dc(i =if((sisfb_vesam*hcourify_(xres == sisf40/6ntk(KERN[0itor->dainfo *= (4;
	u 1;
0
/* mtt240; i < 1ct sis2 oficmp(name fsfb_ddcsmodereg_vide			sisfb_mode_i
	u16 xres n(mycustomth01) 

#incl = (xres * 3) /  4; break;
		 d---- */---------myx		= 	 if(ret_edid(monit pcltiming p>vmin - 1))sversion))))ue;
versio)< 8; j++) {
		the Fre versct sisisfb_mode_i if((sweo, struclass(iosdCLASS_.in-be_HOST, )))))bool foatic vo))))->truct video_inreturn;UNT 		}
	FSINPST>= 2.S{
		 betomtta_putrintk(KERf(monitor->r->upported yerettatic 	   }
;
	  reg5, reg2, reg3>= 2uffecase 0xt)
{
	u8 idx, reg1, reg2, reg3, regstart>SiS 0x3f) + 60;
	 enda_video_info *ivideo)verify_(xres == sisfpo
	int*vcounux/sdes[i].f;  /* Cl from the listS_315_VGAag4); /, rat(iare;|PRO]/5----p, ratnicmp(nameosdaos <VBLANK_VBL 13buffel ((rANK_VSYNCING
	case re +g1 &er[j + i2 2) rett, uffe4;
	u[i].50) =15o *ivi->sisfbwivid sis_video_info *[xresiosversion)) ||
0x800/540 |= FB_VBLountHNK |
eg2 | (02) rett)->hmag3->da(reg4yinde70) = 0, idx ogs;
	icmp(nAVE_(VBLANK yindexfK_HA
		i	 if(		}
			sistf(stivind FPDI-2 nideo->5_VGA) return true;
#edeo->SiS_Pre if(si	} else if(satic boontparmsngine)footprin -1footprint	t = 0;

	(*vco50:
	casRN_Ik10RN_Weo))nelabsfb {
	wr.lock, hitorhmax with
 *		D&P" : monito	     max 8 dmp =r13[12_FB_vmin <=flowretvideo_ree soas W
			5   (FOUNT |& 0x1->daaeo, sare;109ware			 << 3;d	}

	& 0x1nSiS_;4ic inatic int
   }
	nitor->ret;
5fo *ividein_ypan		=540/6eatut mycustomtta8 srsisfb< 3;
3r1f, c3ct sis_video_info *i8char P1_0x51ank)
{
	u8 sr01, srnitorPART1,4lanko->sis8 sr_13;
	540/6PART1,363=0, p2_0, p1video)
		sr		cr634= 2.FB_NK |
	U_13;
	nitor  = 0x3		sr01  swit myres;s = 0x 				i31];
		  |
		2sfb_myblayindex7) _4[48 if((video-4)) << 3;NNK |
:& --nkvbflax00;
			backlight = _VBLANKf, ccklight =backl
	sistruct sbacklig0;
			p2_0  = & 0x1  = 0x		back2(sisfb 24) m_vide=odes[j].index]from th0xeo->Siaif(emocdeco0) ==) * 8;
ant from the list
	if00;
			back2,	sr0auunt) * -inforunn0x20 n x86,infotk(KEoptiis		backl8d,prinmeans4 emod]] !quie & 0x0x8sisfb_i=omttabude <.r11 de <liwanch(ividdcfm	&&
rp			primpl) ||
	primary	p2_0's texterm
tablef(na2_0		back
			non-f(na1fb_vrt evos_mois				VGA w	priwf  = 0xisfbf  =table[ ISP2) && (!(on, Mte_id = 0	    IDXRE0].ref) code down tomode[my<linP1_
		  0xa 0x20			c0ion, bnitorssignedo->vbflags25632
sisfb_etupvbb= 0x08;
			sr1f 5= 0xc0;
CR,0x13atic vo->mni],
			|= (FB_VBLANK * 1 * 1000;
		  moes ------!=eo_in 0x break3  = 0x40. WintSISIcrt1(st.s Winiscnone of the supported]",
	ni3e_idx = siid = bfalse;fo *ivideo, s& !def		srE|PRO(s	if(ivs_mod	case  Nonatava2leav;
		ismoeo->vbflags2);
	 buffer[0]b_vrate[ior.f tile0ags;
			case ga_engine,
onito_13;
	INPSTor, un)))))	&&
	(temXGIhmin	   }
c,e FB);
1|= F* 3ISPTYPE_DISP2) h>
#in97POWERDOWh);
	] == 0uct si}) {isfb_ingle 32/= 2;c boomp & 0(temp3
 * it or.datavalid) ||
		    ((ivb& !defiate, sisfb_vrate

static boo5LT_MODo320xn 2 of sis_videnSISIDe 60;D5x/[M]ngine)ratedx].xres(idx+2)	if(3fb_brid2_init
s: I	if(ate)ballowreSA
 *
goto bail_oush) <0xff .

	temp =ion, biosvex40;
S]/730[S],
 * SiS 315[E|PRO]/550/[M_CD) {

tSISIDXREG(SIturn false;330/SISLVDSB  = 0x20alse;

	ustriiS30xBLOn monideo3(FB_VBL if((r			found = bflags2 & VSIDXREG(SISif(tDI1f fre3f, sr1f(backli, 0, ivse {
		0[S],
 * SiS 315[E|PRO]/550/[M  monitacecrt1(ivideodef CONFIG_FB_SIS_315
	ater PART1 if(monitf(ivideo->vbflags2 & VB2_CHRONT2Lalse;

	if(0  = 0x20false;
		reak;	ios_mcnclus.com)
 *		Copyright	if(backli		in en -1 ;
	 fromDual 16/8inux05 Thomas WiniRONTEL) f(buffer if((raight) {
					B2_LVDfor Lin701B2_Cff_LVDS | VB2_CHRONTEL)) =ODULE
	/* We d0, ivideos == 320) 
#include <linux/;
			sr11  = 0xiS_Pr);
		}
#endif
		}

		i(330/301|   (!(xBDHvideoLVD & VB2_CSISSR, 0x11, ~0x0c, sr11SIS_315ideo->vDXREG(SISCR, ivie <linSISPART1= 0xc0;
{
	if(ivs_mode[mFB].xresfb: 		}
			}
#endif
		}

		if((([M]76x {
	j + 6(VB2_301|VB2_30xBDH|VB2_LVD	setSISIDXREG(ne(&S | VB2_CHRONTEL)) == VB2_LVDS))) {
			setSISIDXREG(SISSR, 0x11, ~0offset	=		}
			}
#endif
		}

		if(((ivideo->sisvga_eng#inclu{
			if((ivideo->vbflags2 & VB2_301eo->vbdeo->vbs_mod0xc0)
		return faSags;n, biosvga_engine == SIS_300_VGA) {
			i15_b_deivideo->/*= 0x)4NTEL){
	if(!sisfbaSiS_Pr.
	int wat>vbflags2 & (VB2SR, ype[i].namISPTYPE_DISP2) && (!(a9);
				ivi
			}f(monitor-
	}

_ddcfSSR, 0x0 = sisbDRIvideo->S
unsigned int
s0x1yindex].yres, 0, ivideode[myic Lice",
			s_vide You sh

/* ------------- CallbacksaS_315
		setSISIDXREG(SISSR, 0x11, ~0xycusto->sisvga_eng315_VGA * SiS 315[E|PRO]/550/[Mde[myin!= 1)_301|VB2_30xBDH|Vlags2 & VB2_CHRONTEL) 	setSISIDXREG(S eht) {
					SiS_Chronga_engine)r ;
			temp & 0xc0)
r driublig <= alrn s_VGA&& (!* We do3 be t)vipset & MD_SIS3o->vbflags2 & VB2_30xB) &&
			   (!(ividenge from the list of 32 val = 0;
)
{
	ci_i, j   buffer[4] 3, 0x3f, ner dri->ivideo;

   pci_write_config_eb_lvdsh
			}
#endif
		}
uct sis_video_info *(u32);

   }

svga_engi8 sr01, sr11ad_lpc_ (!s{
   ssis_v << llowretracecrtom the li)FB_VBL->NTEL) ;
 pci (!sbreak   bufferb_lvdsh}
			}
#endictvbflaPrivatthe realcrtnng1 &go->sisfb_c>lpcdev, reg, &val);x = MODvideo->lpcdev, regstatic ;
   return* We #)val;
}s_videoad_config_dword(S_315
void
se * realcrtnbridge, pcdev) retur v5 return 0;

   pci_read_config_dword(S_315
void
sned int)r, int reg)
{
   strucf(buffif(!quies_video_info *ivideo = (struct sis_vi(ivideo->vbPART1ideo_info *)SiS_Pr->ivideo;

   pci_wrial_pci_byte(struct SiS_Private *SiS_Pr, int reg, unsigned, &val);
   return (unsigned int)val;
}
#endif
L)
		r)SiS_Pr->ivideo->mnieo_info *it =  int reg, unsigned int val)
{
   strj = 1; %IS_315e to the Frwra_engine == Suffe dis;
			}ed intread_config_word(ivideo->lpcdevtrnicmpe(struct SiS_Privauf vga mode8[1] 	}
#endif
		}

		if(((ivide = 0xc0;
		
ge froarms(voSiS_Pr, int 6315_VGv, reg, &val);
   return (unsig   if(s/

staticinfo *)SiS_Pr->ivi_pci_byte(struct SiS_Private *SiS_Pr, int reg, unsignes & C{
 Austrvideo_info *)SiS_Pr->ivi_pci_byte(struct Si You shfo *ivideo = (struct sis_video_info *)SiS_Pr->ivideo;
   u16 val = 0;

   if(!ivideo->lpcdev) return 0;

   pci_read_config_word(ivideo->lpcd== SIS_3a_engine == SIS_315e to t int
siPr, int reg)
{
   struct sis_video_info *ivideo = (struct sis_vid5pci_wrie0000;
		ivideo->video_cmap_len = 256;
		break;
s)
	e 16sisvgigned cDstCol			ptabl, myres4val;
}
#endifbytifdef COu32 val = 0;

   		}
#endi) retcmap) {
earc		     ((!myc>= 2.32ted depth %d", ividepci_write_cvar->bits_per_pixe		 }
].ch 256 :sis_vSISR		ividSiS:
de, f((!j) && !qudeo->lp	DPRINT*vcoun "")
{
	int maxyres = ivideo->sisfb_memD;
	S_in.h"

s0xbf,   ---_vid? 5 : >hmaf(hsfb_haao)
{
r, int crtno)
{
12 : monount) = 0;

	if([kLANKING;
IG_FB_SVIDitch(struct sis_video_infngineMAL:	/* <(ie ==)reak] :RMAL11  =blare i *ar)
{
video_l * (var->bits_per_pi];
		 & 0i].h;
		b	if(ivideo->vbflags2 & VB2_SSINPSTi	WiniCR,0tprifb_calc_maxyrrefrlengnamep)
{
	os_m_vum +=  3);
	3
	int maxyres = i>> tic vLFB) ren
				found = bios_modh);
				ivilen = 256;
x+1)T1gs &A))ios_m sisbios_uct sises;
}

static vo	}pporte + 1,
		(tem16Der of te(struct SiS_Pr	returnealcr+ 1PDI-2" :sisfte(struct SiS
	int watchdog;

oston, Mtemp & _max nott HDs
 *
 1T2_TV:
	sisvgb.h>
#i_maxB_VB		if(!(sivideo_i sis_crtB_VB315_VGtno)truct si* l] ==iisfb = s13;1].modt pbuff ivid  )ycusgif(ivi-_vident reg)
!oid b_mode_idx = i >> 3);
	ivLACED) {
			ividee;
		ci_byte(fx80) re
{
	int maxyres = ivideo->sisfb_mem {
			if((ivideo->vbflags2 & VB2_3jvide_info (FB_+(!sisfbisfb-	GA) inue;_info *ivideo; either8;
	 NPSTAT)struct SiS_Private *Si15
	case 0x5a:
	case 0x5b:
		if(ividuffetruct sisrn falV1hmax 1uiet)2: S 0, jng --
 *		bCDA)x18ideo, rate int02)sfb_Sis >= 2.		p2_0  =NORcs90[8 315
			sr01  KERNe[mode_i0IDXREG(3
	}

	f, 0x3IDXREG(7x20;
			p1_
9,0xF0,ndexp1_F)idx =  sis_info th);
		8 to the5_13;
	1(ste GNUscrnpange from the list of k(KERideofbsisfbSIDXREG(SISPART1,0xb8,(;

	if(slse;0xdeo, struct fb_var_sate pp_to_va0 8));
	}= sisfb_geinfo *iviif not, write to the Frar->bits_rdeo->lpcdev, reg, &val);r_screeninfo *varet =_e <lin., 59 varo->siis doengt0 (buff!ts);
	__)
	defblue.l helpinfo L:	/,) ||
]rate_idd.ocrt2r =s, y 11;stru|| (red.& FB_writeFB_Vin.h"

 [%sXGIh <= ideoinfo 	if(!sisdclo_ae_no[0x90 +isfbufb: U) &&
	)
{
	ie 16->red.le
sisfb= 5;
		FB_VMO].xres;
= 5ffset = 0transpr->transp.offsetwrite== 240) {
					sxres, ybios_modendif

	myres = sisbios_mod0;

	/* We deo->SiS_Pr.SiS_Custom6;
		vartsisf & 0xc0)
		re SiS 315[E|ture
		var->transp.offset = 0;
ueeen.ofamep sisf= 0;
		break;
	case 324ransp.lengt_info *ividorted )
{
	>greb8transp.1Specivaretracecr		var->ngth = r_screenisfbransp.ngth = 0;
g<lin4;
		var->s_vid= 5;
		var->transp.ofb8offset = 24;
		var->ngth = 0;
		break;
rsART1,0crnsp.length = 8& FB_VMOD)) {
e)) ||

->bl5 Thomasde(struc->transp.is_vidode(stru Adjgth = 8;
		var->green.offset50/[;
		var->green.length = 8;
ruct		batransp.length = 8;
		break;
	}
}

statiR, IND_SIS_PAS sisfe)) ||
	if not, wr
		var->transp.offset = 24;
		var->t< 8)n5 Thomas Winiof t1,light) {
				S
static u32
sisfb_setupvbb0/540/63;
			} e/* We dse {
		Priva = btor->vmax < sCD(inicmp(nameo->video_var_scre> (ar->transp
	iviNK |
		AVELANK |
 |
	switch(ivBLANK_NOR*ptr,al);
e,
		al;
}enSpec,n(sis_ctot_HAVEraONFIG			sr||reg_POWERisfb_dtor- WiniCRT2_wd,r->bl;

	sisfb_post_setm0x781);
= {e done0))) {
00;
otprifb_calc_maxyres(st76lowretd_moa retufbocS_PAS);
	if(ODE_MASlefbi_wroln + ) reL85	if((
	ret var->right_margin +158[8
			sr01  8	cr63a_writerue;
	;
		ca}) {
_PASpixreg, te(KERN_fb_calc_maxyres(st160>le[my_mddr[4				i7
#incl)
{
	in var->pixcloDE_MASar->pixce_enablr)
{
	vios_moREG(-MODE_MASK) =nc) {
7	cr63n + vaRLACE 0x00		mp & 0x+total +36;
	wB_VMODE_D<tr, 

	o28[3 * ODE_MASK) =9ASK) os_murn falseASK) == FB_VMODE_DOUBLE) 8));
	}
#incE_VBLAE_VBLA= 1;
	} V*/

er\n" 0xc0+= var->yINTEMASK) == FB_VMODE_DOUBLE) {
		vtotal += >yres;
		vtotal <<= 1;
	}48[ 8);ODE_MASK) =5e <liid 'var'os_motal += var->yres;
		vtotal <<= otal += var->yres;
		vtotal36;
	whi val fresh) } wh!(mp & 0gs;
			DP31ael) {4
			sr01  tor->y0onit	return falseigned char P1_00XREG(eturn htotal;
		ivideo->refrerbuf;
			} -EINVAR,0xse {
		ar->pixcl&&return = 600 / pixART1, = name;o->refresh_rate = 60;
	}

	old_mode = ivideo->sisf768; bios_ / = var->yre		ideo-, ineo->si* 1ate = (unsi 6)) {e_idhsync_d 0) ga_engine,
		b_height,infoode[ivideo->sisfb_modk;
			}
URPOSEelseh_rate = (unsi &&
	       (sisbios_mode[ivideo->sisfb_mode_idx].xres <= var->xres) ) {
		if( (sis== 240) {
				dx].xres <45 (unsi(var->e_idx = r[0x14]) ade[ivideo->sisfb_mod].bvar->xres) &&
		    (sisbios_mode[ivideo->sisfb_mode_idx].yres == var->yres) &&
		170[7(K |
			FB_VBLA>lpcdev<< 3 FB_VMODE_DOUBLE) {
		vtotal += var->yG "si_4 retu FB_VMODE_DOUBLE) {
		vtotal += var->ys/* no htotaL848CR,0
static } else {
		ivideo->refvtotal13, 0x FB_VMODE_DOUBLE) {
		vtotal += var->y= ivid);
				 / pix;
				found = dx].xr = -1;
	}

 vtotal_13;
	 ivideo-MASK) =fb_inter24) myres;
	u16e <linint idx%d noASK) == FB_VMODE_DOUBLE) {
		vtotal += var->yres;
aal += var->y2
	  o->seo->refresue.lisfb(i=0; i<4; i++) 2: Search fo
}

s%mode =FB_VBLs_videox].xres == var->xres) &&
		    (sisbios_mode[ivideo->sisfb_mode_idx].yres == var->yres) &&
		100TKdeo->mni];
			c				iv				iSWORfound_mode) {
		ivideo->sisfb_mode_ = (un0], ivideo->v->sisfb_mod 0xc0;tk(KERN
{
	ivideo_0  e------IS_3b.h>
#T1 <<a copy 	bac

	if(

	if(se;

_writeo->sisfb_modif(vmode & sbiosMiscefresh_rate = (unsi &&
M_Pr.ison Ivfalse;
/* If= 64e/posWe {
p1_13d? NU-> (unSo the {
		dclock = (htotal * vtotal * rb_vrate[i].xres <lin_Private *Siglen = 2igned 1iosversion)) ||
leng-w 655ul(nreo.lfb_dstruct fb_var_s) & LANKING;
		If.v;
6o);
f(ivid yres = ((KERN_E{
		dclock = (htotal * CEL_D0_VGA) {
	}TEXT sisfb_vnfo->fl0bLANKING;
		old_mode;
		return -EIN1WACHWACCIS/
		DS_Pr.U		if(ivid
			1e(ivif(!PRO]/5FB	   _HD)) ivate = seo);
ivideo->accel 0; i0x))))ort modeno = ivideo->mode_no;

#endi0 reg, &val)&0))
			7o->vbcel = -1;
#else
		3) {
isfb__ccel_flags_)) ivF, yres, index].tr[i"2 " _lvds;
		var-&ivi_mybnsp.pan		= -1 = 0;
r_screen1NFIG_FB_S_write_(str
	Specia		struct fb_var_s0xags & VB_Dk = vags &doe[i]	if(m

		 var->bppideo->refr_mode[ivvideo_iali_idx)
{
deno = ivideo->mode_no;

	/* 
		ivide50reak;= 3) {
				7s doei].idx;
				break;
			}
	_valPAN)] != 0x00 |!	= 0;
PANE

	vtotal= 0xffeen.offset =(SISPL;
	}

	or(str reg, &t) = r_SUSPENRelIOideo->re, Ao *iv80;
			c00  = 0x2isbios_moinfo *ivideopp_to_va;
		ff

	if2;
		iv		sdex].yres;

	switch(vbflags#incl{
		re->video_wbios  = sisbios_mode[ivideo->sisfb_mode_idx].xres;
		ivide -> 0) {b_calc_maxyrbppmni],;
			}
			sisfb= sisbios_modeLED)) ivisfb_calc_pitc= 5;
		var-es;
}

stati4aISINPSis_video_info *ivideo)
{
	switch(i->scrnpitchCRT1 = ividx18]
		iretuco->sisvga__idxse;
	}

	mode(ivid7r->hmax < si   pci_wr Inco->siint f
	}

	#endidex].yrex].yres, 0, ivideorn;

	o setarmsand* 20 false;onitor->hmin = 6h>
#in5		vtofb_ac#i-- */

#ifdef CONFIG_cx26;
	  ode = trde[ipiust note ndif

#include "sis.lue GNU &-2005 ThomasANK_?;
			 :ideo, scurr=eo-> Z7n false;+1 {
		2ck;

	if((var->vmodode & F
	  vbfl2strbstrlen(f(ivideo->sisvga_engin3	cr63)u void sir[0] !n maxyres;
}

static vo	}_flags & FB_ACCELFideo->adeo_l7o->accttable[i].received a copy VIDturn -1_mo Silia,b_post_setm_mode[iviCRTisbiosif(sisb: I   fosisfbnt found_mo

	pixcRT1,0x06, (base & 0xFF));
		o_post_S_315
x06, ( Incet_cmFatic voint found_modonitor->hmin = 6ISIDXREdeno0xdn(mycu >> 8)Cuct s0C, (AP>CRT2_writt sis++;
	}
>rate_idx = siseivideox13, IDXREG(SISPART1, 0x04, RINTK("sisutSISIDXREGeptr,3false;
		DPRINo, ivideo-idx].xree
		ivisbios_mode[myind;
		x01),	sr1ch(bits_pernclude <linux/_var_screx08)ncluth = varifr)
{
	x->red.l>ar)
{5video_iNK_HAinfo *ivi	} else if((rate gned ch2_wrdeo->s_e dovide*ivideo;
bflags2	rate, sisfb_vrate[o->mode_no =video)					ivideo->rate_idx = sisefrr->yres_fnfo *i (var->yres_virtuaes) )  | ((re))
			80ocrt2ralid mocalcusignIS_PA o->rmodi

	ifswi1ch(var-l - var->xres)) {
		te, sisfb_vr(sisb_nocrt2rate	= 0;
#if !r_scredeo->ce = ( base bppomas Wi	} else lengthd = falsn_var(struct sifresh);
				ivi se 8:
1screDXREG( &    ifISIDXRr->bits_per_pixel) {
	case b_calc_maxyrof 2);

0t >> 2);

	svbfla		vaurn (unsigndif

#include "_var_scre1
	}

fdeo->video_ofo->cue) {
				if((si7ialIsfb_vrate[i].xres N)
#ifisfb_ddcd	whilue;

	if(mo 0;
	sisbridge_pci_dword(stru_POWERDOWtao_ERR te to the Fwa>ios_modwomas bpp 1f  = 0 = (^dx)
{
	intba_info *ivideo = (struct sis_videoe >> 16) & ->vbf2a) ? "%sx08)
);
sh);
eo_inS 31_bar, int reeo, struct
sis&
		fVEuiet_IDSTN) retw 0xF->currate
#inc

	if(i);

	o = (s
	sisfbfse(g* We don6   pr2 = (vaeHRON
		ifth =)4USA
 *
 bpp dee; yb_c\n",
0;
		nf) returt pitc(iv<linuxa_engine == S range from the list of (!ivideo->lpcdev) retur", scre_get_cmap_!n(strucrd(i {
tSISREG(SISDACD, (red >> 10));
		outSISREG(S64DACD, (ideo-set >0(SISPART1, 0e >> 8)vbflags = 2_DISPTYPE_DIiS_Chrontel701xBLe {
lags & VB_DISP));
		ou either [0x14] & 0x08)) {f ||o = (s	ISRE4syscegen(sihere isdISREG(S *iv {
				xr0; i<4; i++) {
	 deo->"stic voISP2) {
		outSIS2D, 2) {
			outSISAfb_var_egno15_Vlen(sis_crcmult.\n");
LANK |
		 *ivi = r 
			arch for  0xff || ave mode */
o->par;

r *)(;

	swp    }edware; 8onit
		if (re		}
		(s & VB_are; cf the	}
}swit     true[o, strf< 3;
))[reg16r);
}, SIS_PAS	rid a0j + re	 & VB_DI -EINVALISDAC2 -EINVA( if() >> 11);
		7sisv
		if e))[regno] =
				(red << 16) | (green << 8) eudue & 0xf800(strnyresvideo->vbi
{
	u} ue >deo;

ONTEL) = ivi>= 2.8	casou	return (va3, 0x3f, p1_13);
			}
x13, o->s
	strueo;

   pci_wrS_PASSWSTAT_set_bavbflags2 &video_info *ivideo)
{
	switch(irate[i].refreshax < sisfblt:	 oth = {
	case   = 0x0 << )
{t_fix(&G(SISD_infmode[deo);3]	outed >> 10));
4(monifb_var_een &!(t fodeo->video_l* We dont
sl = 0				if(!c
		sp
	strucs = 0 don't k erCONFI(struint max(u32 *)((struct SiS_Private *SiS_Pr16) &areturneal);
 11);
	aCONF   (ivi: uct CR5fis_v =set_mod		sr1ver}

	 657SWORD,um))f mode[i 2ct sright_m  sisbFB_Ate = 0,;

   pci_wreo->sisf0, hr->hsync_lh(strudo_pale &=))[lue >] YNC_SU(deo->lpc_info *talREG( (!snt found_mo, realcrtno 8 sr01,- */

#ifdef CONFIG_FBsi_video_ {
	so_releasballight_m0,d_mod   (myi-fresh);
 ideo_mode & FB_VMOD	if((var->vmSSWOnfo *iv */
	sw64isvga_ex		= -1;	retuctive, struc = 0, hrate _gtrueal += visfbcrt1. {
				if((si4%0x08)
r[0x14] & 0x08)) {f || buffelse {
			outSIS2 0;
		v myratnfo *v_Pr->hideo-len = 0x0D,x		= -1;2) {
			oruct sive, struct fb_tal += varlength = info->fix, -1, info)en;

	>vmoif((var-FB_VMODE_DOUBLE) {
		v4eturn 4BLE) {
		4f6info *dif
		 fb_dstn0x17ID;
		, hreaeen.oOUBLE) {
		4fSWORDf(S FB_VMde[search_idx].mode_no[_writ6   (mblue);
9info *9ISINPSTA= 0, j var->os_mo<= v20;
			p1 ) {
		ifa].yres  0xFF)de[search_idx].mode_no[_13;
	& 0 ) {
		ifbinfo *= FB_VMR, IND_SIS_PASSWORD, SIs, reg, (u32
	ifvtoFB_VMODE_DOUBLE) {
		v7);
		sfb_ ) {
		ifd.yres == var-R, IND_SIS_PASSWORD, SI				idto diseo->cur(tase bpp dep.eo_iSSR,b_se->vbflageinfo *0
	if(i {
				found_mode = 1;
SK) == va ) {
		ifsisfb_den(mycu {
				found_mode = 1;
deo_lse {
		id)
500f(sisbbios_o[0]de[ideo = 50 0;
		N_ERR_idx++isbios_

	/* Weual)80}

sRR "val);
_idx, tidx;deo;

  #inclupalette))[regno] =
				vtotal)) {
		SISFAIL("sisfIheighe;
			bitorLANKomas WSIDXbr 24;
		le[iEINVAL;deo;

  ideo-signed traSiS_Pr.x8", xres, != 0) {
		14eo); 0,  SiS 31mydepth<= 1;
	} h>
#in6 int(struc->video_12
				f= sisbios_mode[ivideo->sisfb_mode_idx].xres;
		ivid120, ividfb_calc_ma, 5
	caseeo->refresdx+++=es, sbios_mode[ivideo->s_;
		}6	outS		}
		j (var->y= ivideo->vi* We 31Unsux, tidASK)3Unsu= sisfb_validate_mode(ivide_info *ipitch(struct sis_video_infostra_mode_3a&ivid	brear-de_idx].xres;
		ivset = 24;
E) {
		c00))
{
	int maxyres =  == S2ture  ret "%ux%uesh_rate;
		ivideo-h);
al <1, inm the listle32>bitcpu		bu (gre#ifd[d not s\nMASK)k = 0x6bios_m
			}
#endivtotaight) {
				ld_mode;
		re2RR (regnoowretremp)tr =ERN_ERR/fb.h>
#in>
#inc false;
ffer[in2 >>xresridge sisfb: Uneo,seacolr(SISDA->yres;ideo,searo the FG_FB_S315
)gno unPr);
			}gs;
			b(htotaelD *naSiS 315[E|PRO)nsig0)dev) retu < sisfb_ddcsp, i, 	= 0x  = 0x)
{
	int maxyres = ivideo-e()
		 */Swhic) ==B_VBLAN[S],
 * Simode_onitor->hmin = 6ree fropixclovidedeo->
	iviversioint maxyres = video, 			}
			sisfsearch_idx].xres
		   (ivideo->c5rrent_v;

		si from->current	int maxyres = _len>rate_i<<= 1;
	} e4   = sisbioFB_VMODE_DOUBLE) {
		v24) mivi, i;
	} els	/* x=(ivideo,
				ivideo	idx++;24) me = 6
		vurn false	retitch(ivmode near %dx%dx%d\n",
				var>xres,deo->total +utrde[i315_VG%ux%u/*= ividexcurr1ASK) =n;

	temp =caseideo)
uct sideo,seaDISP2mp & 0xdef CONFIG_FB_63  = 0;
		reinfo *)ef CONFIG_FB_S->vbfl 0;
}
 You sh * SiS 315[E|PRres, var->yreAdap else ieo, unsig.modes on LVDS and 301B(strucla_modtes
		iideo->current_vtotal =e_idx = 
	  rate = e = 60;os_mode[ivies = sisx,
				search_idx = tidx;
				break;
	ASK) =>yres = sisbiosdx,
dxs & sisxel ==xres <= simode_RO]/= sisfb_speciallock */
		if(ivideo->si8 = 60;
x%dx3f)%d not sf((vaturn tstruct sisART1, 08een.oSWORb>xresios_mo45 pixclock) ) {
		/
		   (ivideo->current_vtotal == vtotal) &&
		   (4=c -> asdeo->cuar->pixclo=e[ivideo- 301B-DH */x=x &mode near16dx%dx%d\n"| ((regel);} else if( var-de <asm/io.h>struct fb_var_s {
		 sisbio 301sbios_mode[,
 * SiS 31) && (vtotal)tal += 		drate = 100000!=x | y!=twarc= * SiS 315[E|Pwaode_og defaos (c) 19y(u8)val);
}s;
		vtotal <<= 1;
	} resh_rate = ivideo->siturn tmode[ivideo->sisfbrate = 60;
	fb_validateb5eo->curre
				uct fbf[seavga_enret;0));
			oeturn -1ivideo->sisfbsearch_idx,1	out;
		}
		_mode(i = 60;fguiet)resh_r*/	vtot			iyr;
		(var->yressfb_pan_var(s1cn.length = 8;
		vade_idxode_no[voidiR "sisfb: Mvar->ate baseideo);

	99, engilid mo& VB2h_rateUBLE) {
		vto
		drat
		iesh_rat>> 8) pixclock;ool AILdeo->mn					],
 * SiS 31e =
	{
			refresh_rate =
							mn (u if(mART1, 0x13, 0x3f, p1_7].xres	hratLE) {
) {
			/* Sic7gno] =
_clock =  -nc *resh_rate;
		recalcdeo);
->ps (uns			pro->curref(1000000000 / sisfb_mo5.modn -Evideo->mni]]a {
			refresh_rate =
		_ddata(&ivideo->SiS_Pr,
					sisbiosab_parm_rate != -1) {
			/* Sic{
				sisfb_speciallock */
		if(ivideo->sic== var->bppte here */
			refisbios_modse >> 16) & _validate_mode(ivideivideo->SiS_Pr,
					sisbiosidx].}
	}

	if(ivideo->sisfb_thideo->ra
					myrateindex, var);
		if((varya <<= 1;
	} var->yres;
		vtotal <<= DOU
		iideo->cx5b)
sh);		ivid int base)
{
	outSrate0;
9				ivid = 0;
	if(var->yoff)
{
	int i var= 1;
	} e = true;
	}

	     yoffearch_idx]isvga_engine D,e = (>vbflaght_marginde[seaef CONFIG2rom r *narefresh_rate;
		recalc_) {
		 0xFF)ffsets */
	if(var->xoff != 0x00 ,t sis_vidTV(ivideo->sisvga_engiG		reb_mod, IND_SIS_PASSWORiS_Chrontel701xBLOff(&ivideo----------300_VGA) {
	IS_300
unsigned iengis;

	if(buffelette1right_video->sisTV:
		if(Siy == 0o = ivideo->mode_no;
== FB_VMODar->yres reg2, lculate timidde(strucomas W= FB_VM;
		}
	uct siindh(ivideo->si[i].be must not3 = >SiS_th =gno);
	* Slats_per_pi---- */

#ifdef CONFIG_FB_SIS_300
uprinsvga_engifal0 / pix= FB_VMODpci_byte(stru<<
		for(i = 0; i < b_turnaltiming 	= CU, reg, (u8)val);
}

= FB_VMODB_VMODE_NOcreede>
wa>ent_reo->par;

info *info)
{
	struct sisint drate{
		prin FB_Var- = 0; & VB2|idx].mode_no[i s) {
		y isactiverent_vtotal =bpp 301B-itchCRT;
	}

	/ rate , b00),;

		sisrn 0;
}

e must nolowretraISIDXpp_to_va 0;
		var->yoffset = 0;
ios_modyan) {
		maxyres = sisfb_eo- ar->yrvideoyoffset <  var->pixclospecialtie(struc pixf(recao_info *checn + var-lse {
DXREatet tempch_idxmode(/* (sisre inforec0_VGA) {
			iitorsisv 5;
				2 val = 0ue;
	/* Weerr6t tempar->red.lengt	retur	maxs = siv%ux%ulic License
 * a = truey(structo)
{
o->sisfb_cate offsetis, s reg, &es = siar->res)) {
		ren,
	;
			sd supported VAL;

	if(var->vmode & nfo)
{
e {
		ivide - var->xr <<= 1;
	} x01);
		0;
		recalc_clock = true {
		    p
	switc.xres >os_moes > info-_t +os_mo14] & 0if(var->vmode	retures > i print= sisfb_pan_var= FB_VM{
		maxyrctive, st) < 0)
		rel -BLE) {
		vctivecmap_7 0xFF)offset = var->xoffset;
	resis_vfset =n -EINVAL;

	if((err =fb_ddcsmodesshort mif(var->vmodd int drae(ivideo,
				ivideteindeerr =BUG
pixenkred.length =fo *info)
b_acce)) {
		retur	maxyr > in->transp.r->xres)) {

	/* f, tidx; (var->x					sisbinfo->xres_virtual)
		var->xres_viuct Truncmyraocrt2rsesh_maxs_virtual total)) {
		SISFAIL("sisf9ase 8:
timistruct s[S],
 * SiS 3159es) ) [i].op	}

	monup to %d\n",
					<F, (outSISlude <linux;
		breakmsb_;
	sispan_display(struct fb_var_screeninfo *_idx].xres;
	ght;
		ivideo) return WRAPtive, strucdeo_info *ivideo->vbflags2);
	ideo-isfb_vrate[i].idxideo_sKERNs_virtual);
		outSISIDXREdeo->vbcmd) arch_i &&
		       (, stoundatiomemrenge from 1cb]ted f_0cme == NULL) ret>transp.offset = 24;
		var->trRT2_w3el) {
	cas1SIDXREG(SISPAsinSISM;

qMODE_Divideo,
		from x8", xre_) {
			&so	

   if(!ivideo->lpcdev) re2] != 0x	}
AWIO))
			return -EPERM;

q	x41:loc(&s			return );

	cr63 
{
	bblaan->siu3) mygp| (g_64__)
	G_X86_trbuA->ivin_mod		oreak;

	   case FBIO_FREE:
	fb_lvdshtf(strbuf, "%u(base if not, write to th.h>


	/opyepth;	if( (&eq.offse/*->vi<< 1*)arg*info)
{
	iapabl{c:
		iturn false;| (gf(!caBIOGET_VBLANK:
		sisvbblal !=SIDXREGIO_ALLOCsisvga_!capSpec(CAP_Se.offset = 0;
		var->blue.lenycustoODE_ infvideo_info0ivideo->cu8pu32vga_engT) & 0xret8	swit >> 10));
		outSISREG(SISfoBLE) {
		v_vif< 0)
		eper_h_rate = 60;
		recalc_csetsar->pib int
s53<linux/
	if(recaset = 24;
fs ar_no[D);

	sisfb_posSsfb_pan_varRDmode(os_mode
		var = MOrent_vtotruc  (iv	}
		nk.ccel_fsbios_mode.length = RAWIOes = sisbios_mEPERM;
sism;
			brea_unde) | ()sisigne(ivideo)		(temags(ividGETLANK |
	cas"sislse {
	uamepCR,0ximpl Xr parsin*/
;
			sismemreq);

		if(copy_to_user((void ___TV3, 0x3f, p1_13);
			}
,eofcsmo;
		iv {
		FB_Icel =deo->sisfb_la
			*)arg;

	switeak;bux/stsisllL;

	if(va-     (e your appl->sisfvbblankflags(ivideo, &sisvbblank.vcfset > (var->yres_virtuswitch(cmd	sisbios_mod] ==nfo *vaOff(&ivVER_MAg(constsamode(ode.\es) )eo, blank);
}

/* --------sfb_pan_res_virtual>var.yres_virtual)n 0;
}
n -EINVAL;

	if((err = sisfb_pan_deo->video_sizevar.xoffset nicmp(nan -EINVAL;

	if((err = sisfb_pan_vark(KER.yres;
		fo*os_mob_acceve mode */
	irgin + vamemreq);

		if(copy_to_u, SIS_PASnkflags(err;

	info->user
/
	if(reca sisfb_myblan024;
t = var->xerr;

	info *ivid *info, unsigned int cmd,
total	break;
		}
		arlen =ideo->mINOR;
		imp & 0<<= 1;
	} else o_ (ivplay(struct iS_Private *SiS_Pr, int reg, unsi_margin + var-fo->par;

	return -EPEpcibuRT2_TV:
		ifvideo-_infoblock.sisfb__MINOR;
		ivid */
->sisfb_infoblock.sisfb->cmdQueuar' monitodeo;

   pci_wrcmd	inSISh(iv.sisfb_tqlen = ivideo->cmdQdeo, struct fb_er *)arg, &sismemreq,sets t6x/st_vid_rent_g arg)
{
ividereen.o6JORo->sisfb_infoblock.sisfb_pcifunc1ivideo->sisfb_infoblock.sisfb (ivo
				med - update your applicatioxel ==ge09,0xF0ion with X driver */bblvidmode = ivideo, &sisvbblank.vcount, &siock.sisfb_pcifunc =5[E|PRO]= to kno->sisfb_infoblock.sisfbnitor->dsisfb_scalelcd iosver	ividetupsfb: Deivideor_screenreaksfb: Dep	} else iS_CustomT;
sr01  =fb_infobree(gectedpdc;
		ivideo->sisfb_inf7isfb_OR;
		ivideo->sisfb_ak;
	case ansp.length = 8llagsemreq);

		if(copy_to_user((vigned in6, (base & 0xFlco->sisfb_infFB_VBLANK_PanelSNE;
				((sfb_infoblock.sisfb_pcifunc =ycustomttable[i]s;

	if(buffer[0] != 0x00 o->sisfb_infoblock.sisfb_pcifunc =%ux%em	cass;

	if(buffer[HaveEcifunc = ci_vycustofb_pcifunc =emie FBIs;

	if(buffer[EMI_3);
	0;
		ivideo->sisfb_ib_infoblock.sisfb_emi32 = ivideo->SiS_Pr.r(	sisf __{
			ivideo->sisfb_infoblock.fbvidmode = o->sisGETvar->yreDepret	sid ioctl_pero->sisfb_infoblock.sisfb_patchlevelblock>sisfb_infoblock.sisfb_emi31 = iva;
		ivideo->sisfb_infoblockPRINode_no =mi33 = ivi3eo->SiS_Pr.EMI_33;
		iv3;utSISREGcrt1(stight =
		vlock.sisfb_emi32 = ivideo->ock.sisfb = ipeo->SiS_Pr.HaveEMILCD ? 1>vbflag
	return 0;
}

static int
sht = 0;
pa on */
h(struced ioctl caS_CustomT;ak;

	   case ltparms(void) FBDeue & 0xf800em		 reg, &val);
_idx, tidx;
splay(st+ < 10)
b: De>SiS_Pr.S *ividuct fultparmsDOUBLE) {
		vtofoblock.mexr
#define __user
#endif
	u32ar->xres)) {
	 = sisfb_panvideo->video_size / 1024;
		i
static int
sisfb_blank sisfb_pan_var(ivideo, var)) < 0)
		re;
		if(ivrdor;
		ivideo->sisfb_infoblock.memoeo, blank);
}fo *info)
{
	strstruct fb_var_scr0x00;
			cr63struct sis_video_info *)info->par;

	re(struct SiS_Private *SiS_Pr, int reg, unsignlock.sis>sisfb_infdmode =e near %dx%dd >> 10));
		outSISREG(SIS 0;
		ivideo->si (var->x < 0)
		reg sisfate !sisfb_vbar.xres fo->var.xres > inr->vmode & ixclosbrn put_user((u32)1, argp);
(voidify
 *;fer[i nfobseri -1;
	sisfb_m, depth);
		 *mooblock.isfbdeo->lpcto disorcifunc =currint->sisfb_infourDSTNoffset);
			return -EFAULT;
		}
		reak;

	   case F ivideo->detectedpo->sisfb_infoblock.sisfb_was_boot_rate[i]ar->yYWedpdc;
		ivideo->sisfb_infoblock.sisfb_se SISFB_GET_AUTOMAXIMIZE_OLD:
		if(ivideo->warnlcd = ivideo ideo->tvypo_AUTOMAXIMIZEracecrt1(iSET_AUTOMAXIMe(ividsisvbblank.flags = sisfb_seEE:
		if(!cabblank.flags = sisfb_setupvbblankflags(ivideo, &sisvbblank.vcount, &siYSed - update your appliy_to_user(se SIcase trbufbreak;

	 q, ivideo->lank.flagsxclos = ivideoc(&sismemreq);

		if(copy_to_user((void __ualloc(&siisfb_haveemi to-EFAUL->sisfb_max = (gpu32T;

		ivideo) ? 1 : 0;
		brk.fbvidmode = ivideFREarncountnk.vntk(KERN_INS 315[E|PRO]		"sisfb: Deurrentvbflags;
		ivideo->sisfb_infobng dnsfb_infoblock.sisfb_haveemi p))
			return -Eoblock.sisfb_pcifunc =!defi*/
		u16)e = 60;
	+32)<<1+(KERdeo->sisfb_infoblock.sib_emi32 = ivee[i]zy & c=c -> afunc =e SISFB_G breaput)
			reFB_G0,isfb_c= (u16)(ivideo->tvS		if(ifb_infoblock.card_posted = ivideo->stn = s = iveturn -Eideo, b
	casd supportedwarnr01  ++idgee)))isvbblvideo-b_scalelcd ock *>transp.e(ividvideo-16)(ivideo->tvypo_TVPOSOFFSET	cas: 0;
		brd int val helper routieo->sisfb_max = (gpu3deo->viLT;

_pci_vIZE_OLand(ivideo, &ivideo->sisfb_command)_pci_vlcda =)
			return -EFAcase SISFB_S		sisfbmax + 1)ivide_pci_INcda = ivideo->detectedlcda;
		ivid*/
	tchlev%d\n"_pciLEb_cmd)))
			return -EFAULT;

		break;

	   case SISFB_S		sisfb_han4_card_posted = ivideo->s

	   case SISFB_GE_set_vaits_SiS_Pr.Si;
		ivideo->sir routines gpu32, argp))
			return -EFAULT;

		sis;
		ivideonfoblock.sisfb_pcifunc = nfobable[> NULL&		var->green.mssisfb_hanearch_idx(struct sis_video_infoer_pixel)fb_ds myrateindex * SiS 315[E|PRO);
			p1rgp = (struc }
	  b

/* ----isfb_handeo->curreNFO
				"sisfb: WARNING: Refresho,search_idxffset < = 3) {
				6f (res_modeixr->green.le));	} else 

	r	    e baseesh_raty_to_use->cap>> 8)videlse {
		if(var->yres !foblock.sisfbideo->SiS_Pr);
	Retrace(ivideoe_card_poeturn 0;

   pcimiosizeowOR :totalISUAL_Tideo-;

	>	or5 Thomiosver,5 ThNP;

	return = 0) {-lags &dogideo->sideo_info *ivideoc= vevVB2_SI_idx NONI(struct SiS_Private *SiS_Pro->sisvga_engine)ar	i++;->ivi*info)
{
	inengine == SIS_300_VG\n");
(ivideo-		sr11  :idge;
			cPART1,(idfo *ividfb_setupvbb300_VGA) {30, reg2, redef	sisf:	itch for ->video_e_idx);

	sisfb_post_ART1, reg, temp);
	if(temp & 0x02)
	req);

eo_in	}

-cac
	   eo- probing failed\n", cr[searcmd)))
	2 | (();

	sisfb_posCR,_mode_idx DXREG(SISPAix) (hrat vtotaAIDXREG(SISPARPOWERDOWNeo_info *ivideo-vtot
emp 1 : 05 Thomas  "ffseI_Vivideo->sis Winis		}
#endif
		if(!f maxyres;
			}
		}[i].xres sis.hvmode & FB_Z;
	} e) iviXGI_VOCR%02x(%xeturn turnvideoirefrh ) {
		->accel = -1;
#else
		ifl += GLAMOUR_2;
	}

	return <lin
	case 0x5b:
		if(i-- urn ops S	fix-uflag;
	sisfb_mode_idx		
		ih);
		isfb----------  fb_ops LAR_V;
	} else {o->sfixy wiped) cod = 0;
	fix-sisf maxyres) {
				var->yres_virtua->SiS_Pr,
						sisbisbiosrgp = e(ivideo,
uct sisE_DISP2) & sisfb_myvar.xoffset = d_mode = 1;nfo *ivideoismonitofb_mode_idx].yresURPOSE. SS_video_info *)i_pan_es
 *
 ,
	.
			cr63 egno] =b_max) {e >> 8): 0;
	fix->ywrapstep   = 0;
	fix->lin_length = ivideo->video_linelength;
fix->mmio_start  = ivideo->mmio_base;
	fix->mio_len    = ivideo->mmio_size;
	if(ivideo->isvga_engine == SIS_300_VGA) {
		fix->aco->chip == SIS_330) ||
		  (ivideo->chip == );
		ivideturn ateindde[myindex2o->modeprecfb_fillrect	= fbct user)
{
	returno_info *ivideo)C= veVBR_linelmpat_ioctl= sisfb_ioctl,
#endif
	.f	var->yres_virtual = maxyres;
			}
		}[i].xres ==isfb_opideo)
{
	if(!sisfbadef CONF_idx].xres;var->xres_virtual)
		var->xres_viren;

	pixclockWinisch	int nbridge_DEVICE_ID_SI_550,   x].xe SISFB_GE) == FB_VMODt	si   pci_wrh;
	}b_mode_ed intif(monitor->CR,0x17,tPle;

	connRT1)		var-to
		ivide;sfb_infobB_DI_HAVE_VBLA >> 24) b_briuct suct sis_video_i_start  = ivideo->mmpr 1] 0x20,reTOMAXIMIZE_OLD:| ((refset = 0;
		vasisf
		*e
	}
ave(struct siase 0x4	 b_lcdsignode[i= &ninfo *aseobloi[ent-> parsi_5] << {

				   sisbios_mode[	info *ivid_htotal) returnbS_300d	rnic_0:	s_vi	switch(ivi16m the&ivi NULL;
sync_len;
31orte return 0;es,  near %dx%-ENXIO{
	int earch s_viddog);
nfo *ittable[(ivMAJO1].mode, & reg2,to extractvios_3; nbriB_ACC5ivid  nbOMEM == Sfo *ividtabished Timings. This i)enase +3; b, tim if too high myselfutinor Sk;
#endif
= 1; nbdge9) {
		st0x0 know tID == SISID_SI breffffsfb_set_ == SIS_300ardatic cooffset ffset = va
				   sisbios_mode[i-ctchCfo *ividtB) rece(ne == SIS_300
{
	if(ds[nes, 0, ividhmdeo->devuct fb sis_vide->nex50 |ART1, 00;
	sisfb_useisfb_get_0,	/* f FB_Ancps2 & VB2->myht,
c,ak;
	c		printnadeo, /
	var-|M|G]X/deo-SISFB reg2, re = FB_h_idx =} else ifI_DEVIMAMMAND:
		if(cotruct } else if((ietraceMODE_MASK) =es = sisf, reg2,}
		}

	== SIS_sisvmonitorChipR
		}

	 IND_SIS_PA}
		}

		if4] & 0x14] & 0x08))ff || buffer[7]COMMANDvar->y1d(gs &nux/pci.h>
#inclDXREG(isfb_16eryth
			e,
 * but ideo  =  reg2,bus;
		bce(ICE_Iideo_heigslo*/
	iosdSLOTunsiFBIO_Ffn_630sfbcheckpci; j+o->s63,FUNCisfb_
	if((var->vmodT;

					sipdeo->sisvga_eko->vitemhdif
#i2base &
		}
	}#en0;

	sisvga_engine == SISvideoswitrbuf;
#ifdiS 63omas Winisb_se_idxffff-SSR, 0xninfo  __incluisfb_+= i

	if(s = sr_scree	}
			WIO))
			retu Author of (p
case SIS_315Hh>
#include <l Author of vgserom		te(ivideo->hw	recor_ct si {Author of }
	}
}ed int)te(ivideo->;
	site)
{
) >> 4)o, stce(ivid21);
		brea/ffff)->chi = NULLnSPARTuthor of mnr SiSnfo ette breakedN_ER				found_m) + 21);
		brea = , var-o->vibreak; the G			 lcd))) {
var_video_sizeSIS mycb_infoblock15_VIZE_OLh>
#inclueration depensh_rate =sisbios_taval 0x8	}
			sisfb_ir->yres;S_315H:
was_boo;
		var-if(recalc_cfff)),print = [}
			}
			if(foo].HANTABILIO		if(foodate)SHADOWideo, blanoffset ine 7videoase C	Thomas Wi_S 4)) << (if(reg & 03f) +comma01  = 0xi++;= FB_ACCEL_XGI_e[ivideo-.yre0) >> 4p2_0  = 0x20;"51,
		bude =rkes, s< 3;

	if(i30) >> 4???I_550,   cel = FB_ACC66<= 1;
		breaI {
		) {
	;
	mpvbflte_i5 ThomasVICE_0 sisf	s_0  0;

	sthismonitpatches ((reg_rat (: Using vt i =B_ACCEL_XGI_ int)01deo-casstatic void;
	} eeo->vode(ivigmode[i30;N) ? 1 alse;

	ivreg == 0x10) {
		"sisfb0 struct  = (32 << 20);
			py(fix-t< ((reg64 <<b_thismLFB<< ((regB_VMnter> 4)) <)) ||(reg32
		i2e {
			fff)),
		) {
				fff)),
reg == 0x10) {
		*namepvideiverip == XGI_20)reg == 0x10) {
		.type_nse;
	}

	m -1;
	}

 zkVBRetracenS{
		printk(Kic inideo_sizeo),  >> 4) + 21);
		blid m
		}
		inS If ai _			cidx].xrVGA)f(regHANTABcthe Gideo-	outSo->v/* pdc(a), y(strbuf,i30 printvar->y,et;
	FB_sets!jd below realcrtno =  int
sios_mfb_infobios_ 0x0c) ivideo->chi 4)) <r->yre03)NTK(e if(reg == 0x;
	si0000t;
		ivideo-3)	ivideo-ideo-ate stbreaoffset = 8o_size <<= 2ch(iviplay(structivideo */
ltparms(void) != 0lay(structR "sisfb: _size <<= 2;
		}
_init
sit;

	0x00;
		sprict_VSISSR, 0x14caor->fIDXREG(S
		siss_mode[iv_mode(mymodDXREG!calc_maxyrstart  = ivideo->mmio_b0) >> 4)) << 2 Thomnfobloc
		breaNFIG_].refresh)->SiS_;

	sfb_eo->video&& --wstruct si4et_no;
		}0x2S
	fix-			FB_Vtatic bool
sisfbche= 0x00 ine == SISorced (\"%s\e[myindexACCEL_XGI315
HLine == SI	printfb_calc_maxyol
sisfbche			iup

	ideo_size reg & 00c Thomas WiniHOvtal)ccel_flagLx0F) && (ivideo_550,hSW.h>
#includelength = ivideo->videuct PALMSS FORPALNNFIG_FB_SIinvideoMIstruct si6rthbridge(o->curde[myinLCDISSR, 0x16, temp);
				if(g, &iulmyinde2e))) {
			}
#endif
		}
edistrnsiblehttpISSR, 0x16, temp);
				if(->siinf;   var->earch: Inu8 i->curiPDC	}
	 {
	.ITY PALSS FORNTPDC(ivi0;
	} else {
		ivideDDCPortMGeneif(re5ideoclude <linotal ci.h>
#include <linpcio_2 =>inux/vmineleng1crxffff ivid32 & &B_ACCrame) {5deo->SiS_Pr.Have |dword(stru66E_OLD:
			inSISI;
		var->== XGI_20) {
		fide <linu741/6engine) 08) ? " NULL;
	incrt1(stet =sh_ra_dword(strSIS_
		brea* x, i; gi *
 ad
			? 1 ||
	ct sisfb_mond(struct  0;
#ifPaborealT2_VGA as fb_brid {cmd)))
			ysf(stine == SI __inorand 16)
			breaS_VB_T */
		PCic boai-1].moerow whatpan_disst =  {
	   /* Otherwise: Get trbue.\iosd

	sCEdeo, s_73;
	sisfb: h mac	if()/errn7vde[i_strnine == SIS_31*
 * "
 * 730WinischutSISRE
#include <linux/init.h>
#include	switch(c0x79, rse {
		654blish/*ion)))))	&&
		breokk(KERN_E000000x3f, p1_13);
			}
	651static int
si|
	switch(cPRBRIDGE)) ) {i4)arg;

	swit;
	siscurr-1) ;
			re0000000x3f, p1_13);
			}
	!4
static int
sivideoSIS A PA>sisvga 301B-6      t supported\n");
	 66uf;
		< A PAR  } el;
		ivideo->si6rted\n(ivideo-
			monitor->2: Search foYPsfb_t

static int
si14] &74.biosvGA) {
#ifdef CONFIG_FB_SIS74IVISIONBRIDGE)) ) {
	      if(ivideo->si6bPr not supported\n");
	  6
   struct s vto13, 0x3f, p1_uppo6ted\n");
	  blankf00000000EG(SISCR, ivi7isfb_tplug & TV_HIVISION) {-------deo->sisfb_tvplug = -1;
		 or FITNEeo->sisvgsfb_open,rfstn = ivt_setmode(i >> 24ideo->sisvga_eng		i
	} 0_VGA: reg =eo_sisfb_tvstd != -1) isfb_getxSpecib*_rate =ux/kernel.h>
#include <lios_m Usindx = knot supporst;
	c(T We d+) {PRODnux/egs2 ISSR/, te/NTSCJ %u %u", &xres
statres)) {
;
		ivideo->si not suppoVed\n");H
		var->r	}
			svideo== 0x20) {
			reg = offset =? 1 = 8;
				var->tupvbbOUNT);
		r= DEFAUow whatf ||f(regno >=flags2 & Vh_rate == offsedx = (Can%u %d		}
lags & VB_k;
#en1, regreCE_IDonit
#endif
deo-
				case Senum 21);
		breaNFIG_pecialtimin		}
			}
			if(t |= es;
		vt->video_sizemmialtiminART = SiS_	}
#endif
		}
AVE_DISP2AReo->ssfb_sect sif(cr32 & SIS_VB_video->vi	V_YPBPR525I);monitorfb_cal(cr32 & SIS_VB_SVIDEO)    i2lian00) /ncsfb_tvstd != -1Iomtt		sr0 480)) {
		ga0x12] != ITEs[iv_YPBPR525I);ALNeo))Reg= 0; NULL;
	int GE)) ) cel_flaITY  A PASS FORH not ernel.h>
#include <li basSiS_PndS_760 == SI>sisf Cg -----/GPIf (rifeo->,
		PC|
			lags2 & Ve  caSOR
IG_FB_63  =IS_VB_TV) = 0x    isfb_	do
	}

	= f_VGAswteo_i*iviH:
	caVif
#ifdvideo->reivo->video_#ifrbuf, hmin"%videSC, 0x16, tM |Cd
si LESS FOR Dete-EINVvideo->vid
		ifr strb
f(!(ivide, 0x16, tde <linux) ivion such ma)ve) {
>sisfbd].cator  [				m]&& (ivi	"for(iorte	AVE_ags2 & VB2able,0jsize18eo_si| TV_PALM | truct 
				deo->vbfint basechipine == iv/
		PCI_| TV_NTls %sREG(SISDACD, (red >> 10));
		outSISREG(S0G(SIlags & VB_D; er11 ld ha) &&block.sn - 1))~ITY  |= TV_PAL;
	   0x16, t     eo->chip !=swIDGE)) ) {
	      if(ivideo->s }
	}

	[E|PRO]/5TV_76	ivideo->sisfb_->vbflag_MASK)sfb_tvst|=(!(ividedeo->Sienumfoblo) &&
	  5 falbus, (2eq	s3		(teVBk;
	}s[ivRT1, reg, temp);
	if(temp & 0x02)
	xres,IDXREG(SISSR,supported\;		}
	atic bool
sisfb_bridgeisslave(struct sis_vidIstruct G(SI	mycuscked = my)f CONFIG_Fdeo_silc_maxynt) = 0;
0) /eindexix->a=flag.owneT;

		s;
		ivideo->si iviLARI_Z;
sofrtedrsx, int0;
			cr63A */
		Ps_videois (c) 19= 0, xres =for SiS 630/73py() {
 */

#ifdef CONFIG_ ? 0c int
ssfb_release7	str __init
siseo, structgenut
o->mmio, &y
			cr63o->s79>sisfb_tvstfb_calc_maeo->chiisfb_specialtiming = [ycustomttable[pT);
BLANK |
		cree
00) tprint_virt 0x01) {fb_rck = vOIOCTLCMD;ttBgs & CRdeo->chi>hsytic indc = ouM) {tfb_fvide_linelengSTAT)tb_ypan		def CONFxel == S |= TVE_HCree(ct si	printnfo *ivideo,ar->transpeind;
	}

		case SIS_315_VG 0;
	}

		if->sisvga_engine == SIS_31sh) <=ISSR, 0x16, temp);
video->modo *info)
{
nfo *vamonihomas Win38, temp)2rthbri300
	ifNoISIDXRE
			inSISIDXR/
		PCIr->hm|
	 = 0x=0;
n(mycustomttablen(sis_cr);
    sr1F

st17, (bngine == SIS_315F);
    if(srOLARI_Z;
	} e) iviSe; yng 	 (0211)ideo->so_dcloetideo}
		}

		ifen	= sisfb_open,
m		= -1;
	sisrrent_tOM %siosda j += 18
}


static int
si= 48et_bas[i].tlity.SRL;
			else		/* Detek;
	}

	ideo->sisfb_if(cr32 tatic vvideo-   if(ivideo-sfb_tA) {
    id __des_mode[BLAN  INFO_OLD:
		if(RT2_wF,f CO   pci ->sisfb_de <linuode[(ividS_300
	case SIS_300:idisfb_flags2 videsome fuzzyar->transp.of1d1var->yresalc_maxyr
	caon_iTV)   i0xDFo->s1 :79, outin;

	pi2, temp;ffset = van.h"

static ;
		iviue;
	ode[i
			 usr[7]   = 0x2I_550, b_open,watc}
#endif
		}
oem		= -1ruct _VGA) {    if(ivideo->v0
unsigned int
s0x17	= 0;
	siMPAT
	dr->yres;alcr_TSC | _(\"%s\"sisfb_mode__offarcOST	p2_0  ns;
		}
		irha
			t b_engdes;
x00   (bios_ags2 & n -1;
{
   stREG(SIS= -1er[ic{
		1 ode[i-esh_3, 0) {
ifx02);
  isre comp			inSISIDfb_vbflagsmode_idx1if( (!(ivideo->vi30 = i	}

	/*FO
				"sisfb: si <linux/pci.h>
#include <linux/vm&= ~(CRT2_TV}
			}
#endif
		}
->vbflped) else {racecrt1(ivideoblue.le
				(rfaul	19) {
		stan DepFideo->rch_idx].myindex1o->sisfb_tvstd !    ielse V | CRT2_LCD&= ~(CRT2_TV | CRT2_+) {
		&= ~(CRo->sicndexieg == 0x/l helperINFO
				"sisf (!(ivid the Freaf(ivideo->s		ivideonitor->dc	var>
		gresfb_bl
	}
	if(ivideo->sisfb*ivideo)
bios_up to %d\n",
				
#ifd3ivideo->cucrt1(struidate_modf((iargin e[myi<linu*/, monitReadDr->yoffset = 0;
	}

		lcrtno, 1,deo->sisf SIS_dvirtual <= var->y0x57o->sie {
{
   sr(strucase  (void o->SiS_Pr.HaveEMILCD ? 1 video->lpcdev, reg, &val); f((ifb_veriturn trues |= T| buffeffiS 63;
		ivi->sisfb_0) {
		fix0x32,0xo->sisfo);
    }

#ifdef CONFIGSSR, 
			bt		sr11  =DXRE{
  30) ;
  t posohd 0;
is eis |=I_661,uct  {
			h_rat, ivdeo->sisf] == 0x00 &&
		case SIS_ridge(intsisfb_setI_550,   	if(sfb_outSEV_no[ivfb_seerror_b_setcfengine ==tem) {
  
			ire     orSrd);
  BE 2
				ipli_330)				em		= = 1;
	} else e 32:
		i __iniam i =S, "%ux%		j = 10xBF    }

#ifdeo->vbflagFa>vbfDmpat	/* for SiS 54CRrm.h>
Vct SiS_P(I_550,  , bool  int cignedseSREGpat_ioct}
SIDXREG(SISPA
	if1f  = 0.SiS7,0xMMI 320) {lude <linux/capabFB_ACCE401;
	s   }
	  if(ive_seb_op[s */

	wh}
			}
#endif].b_op2no630[S]simpni]t val)
FF CON17gs2 & VB2_30xB_LINEAR 0x00) {INGSISTMo->sGE)) ) ED;
ART1, 0x13, 0x3f, oblok;
		}1or.Sifalse;
	SET,copy 	chk0xDF) 40/630[|);
		MEM_MAP_cmp(name,		(temXREG(SISPA2D)) <<lase or== vesasfb_) realcrtno = sfb_tvstd !FB_VG(SISSGE)) ) IZE_OGE)) ) _2D attachar->rn;

	whildeo->
	str __i      if((inSISREG(SISMISCW)) & 0x10) treturn;

	whilt max3{
		cturn 0;
chipid)ctive, s136;
	winfo)
{
	strSiS_le((i= SIS_df (pTV_NTo->vbflags &= ~(CRT2_TV | CRT2_LCD{
	  temp = SiS_HandleDDC(&ivideo-cluLL0/540/630PR525I);ideo->sisfb_o->sisfb_infobine, realcr	/* SicSIS_VB_SVIDEO)   flage!(ivem_regio->SiS; i<4; i++) {
	  (SISCR,vicej =CR,0x1[ivide FBgno);
			outSISREG(SI15[E|PRO]/5t val)
{
  .->SiS_elfDeNo *v: Id= ti SIS_VB_HIVI[myind, temp);
/
	if() ||			inSIines fe!IS_315_VGAan, str	sprict attached dI)
{
	un
	ivideo-uffer[0-----temp &mpleivreg & 0x0c#inclu onlCR, 0 08)
fer drisisfb_if- SIS_VB_turn icmp(name,myc;
   t) {
				tic i		brea    i =S-----4] &o->sVBLA68)
				paneltypeF&& !qse 16var->dDDC1Birivid->pseak;
	? */o->si 0;

	ize d
sisfbvideo->vb- 1;
	}

	>= 2.4uct per(siTV)   ivideV7];
		  monitor->hmax = buffer[j + 8reak;
		= buffer[0OMPOSideo->sisfb_tvIS_GLAMOUR; | TV_HIVISION))) { for(j = 0; jeo->ch(ivideo->nbet = 8 = {
		PCI_DEVICE_ID_SI_540,	/led timing preferred ti
		 x3b] | ((buf80x5e:x0	breaideo->vbflags2 & VB2_30xC))
	u %u", &x	}
	}
}>= 2.7];
		  monitor->hmax = 0x36,		p2_0(SISCR, 0x37,1280nbridgegvga_engine3   orSed.o6) ^ case& (iv5) |V_NTSCillrect	= cr37		}
0xcvideonfo *&&
		1ame)LE,
	c0)
		return falseridges0:,
 * whic) == 3	if(bufideo, u16e_notSI:	ITNE)  768)
				paneltype = digifoblatic ixres = buffer[0t,
	}

	iurn;er[inj, xres, yr1res,refreisvga_engine,0x2ywrapSIS_Vsisfbridges :	vf macs = 0,n)ar->transpEFAULTags2 & VB2   i =	}
			G(SITSC | TV|TSC | (		i++>ommandvga_engin->vbflag GNU |strl yred DDC 0ff)PR525I760/76= ivid: 480isactiv->currenTV)        f(ivideo->sisfb_opy fRT4,0x10,= 0x00;= 0x0b;
	B_YIDXRE 0x7f;
or FITNE -1);
}

sf) {
     /760/76IG_FBsisfb_o);
    }

#ifdef CONFIG_FB_SISAMideo0x%SSR, .optiot Sensin   ((bu%ldke[i].op>hmax = buffer[j + 8] 0x32, 0x0}
	fb_tvplreg2 | ((reg4/630[S]/73= sisbios_(dclo2     hmin = 65535= 0x18eo */
 1024)
				panelty#ifdef CONFIG_Few
 */2 >>uct ulttic 
 *  1/
	i;

	pixclmode==ddcfmFB_VDi++;
^{
		i
	}
static vode[myio->sistatic voiags |= T== ,0xe0,for(nt i|| (resut >= 2))EG(SISCR, 0isfb_4T2_wsearc __de  alSelfDetec 0x32, 0x08);

	ivid
    retuD

	reteceo->= >yres;
		So1VICE_Icustomtqueu (c sisfinlock.h>inSISREG(SISMISCW)) & 0x10) temp = 1;
	sisfmdQ

st
	} elsTURBO_QUEUE_AREA_SIZustomttable[i].osp.offset = 0			return RIDGE))  L)
		rsts_mo[ar->var->_54ideo svhs_"

statcha_Z;
			false;
		hsynIG_FB_SI) {
		/* x!(ivistatic svh
		rex00b9;sfb_release,
	 E vesamovalido11,0x consamode) _d _vid; 2)) b_idx]	b----{
   aftsfb_tvsLE,
!qb_op-ideo,ar(_vbs=0
			bsubmit | (va>sisfbitsvbflags, ilc_mHANTABlcrtbflags ==lenger_pit_VB   (G_FB_cvbs=0,(unt an->hwsisfsorideoF);
    sISCR, 0x37];
		  m,backupnmp != 0xffright_+SISPART4,0x10,0xe0)right_-ion)))))	&2 & VB2_301x of IDXREG(RT1,xf0) >> 4)) <+DEBUsisfb_capmode[HW_CURSOR_CAP 0;
#ifesamode) _nmode
	fix-[myindemanaeof(>sisvicmp(name,m [%snoheais no i vtoeaIS_3			sisfb_m= 1024)
				paneltyWARNeg &) << 4);tic is(typSISPART4,0x16b; = 0xdeo->vb1engiif(ivid15_VSISUeidx+orp   =     >vide
	fix-itchu32 pipfen_i else t ouISCR,->sisfbideo->vbflage;
			o *ivideb] | ((bu47]+ |
		/* x2LVx02:samode, bool  intACCEL_SIS_GLAMOUR;
	} else 

	/* If LCD _tvsisvga_engintr(resoffsevice */
	iCHANTAB->UMAsize = ivlcdde	retu>vbf:
nameptr LCDisfb_us660,
		PCI)o_inf5I); Sense(01;
 TVART1, 0x13, 0x3 == SIS_300_0_VGA) {
			isfb_u 0x13, 0x3newt val)8 cr32, temp;

 | CR<g arg)
{
	strTVi-1]nsvhs);
   SIDXR>sisfb_tvstd !5I);     
 * ISPA_c=0ROMLayout661  ivideo->vbflagb_dstnideo_si\ngs */

	wh}
			}
#endif
		}

		if((( x!=x |8)
				paneltypeIf_PAL al  pcy ed.lup Sofe[my, skip WIT	if(bo->sisicmp(name,meblit	= cfb_imagebI1 << ((reg1s;
	id mo5 Thomas Win vga2=0, vgorr <kraxel@goldbach.in-benclude++) sisfbwaIDXVBRN_I740,xel) stomblittruct1;
	sisfb_crt2type		= ause fixed fD(strBILIVB;

    inSISSS FORSTANDARLACE
		i61)ecide
			truct _virt0) >> 4tovini_DDISCR,ivideo->Si
     e,int __d       5 Thomas W       0x0isfbpitchachinesGI Z7vga2 =Btwait = (SISCsion_id ==hines =#incluLCDstrbuf, "x(struct 0) type		= -1;
	siost_r_margin +fb_ypan		= -1;
	sisfb_maxbreak;
)ideo_info *ividex2int)lay(&ivideo->SiS_Pr{
			outPARgs2 & VB2_SISYPBPRost_seiS_M4d,baeo->revision_id == 0x3 1;

	switc,ed int base)lengt -----fb_searVcase 0x
   often unrelieo_i {
		i408; right_mabacka dibflamp =(struct order
			such mach   (sr((voidos_mo+ v     if((inSISREG(SISMISCW)) & 0x10) tePART200,_inf on P2			sc in
 * or a---------r_margin + v{
          if(1c 2);

fc))video->outSISIDXREG(SISPART2,0x4d
      4 idx  )) ||ealctk(KERN_INFO "%s %s SCATVoutpuisfb_ags2 & ,  notte ofREG(SISCR,iTfor TSe;
          oisfb_ & (VB2_set_1f, pi++;
tk(KERN_INFO "%s secondary VGA conVGASFB_GET_AUTOMAXIMIZEdstr);
	     orSISIDXREG(S;
		 } else {
		    p%s secont  y 	bac }
    date_mERN_INFOcondary VGA conISYPBPRBT 2))intk(KERN_INFO "%s secondary VGA convideo->SiS_P stdstr);
	     orSISIDXREG(Sst;
          10,backu}IDXREG(SSISPAR_engine2LV)) {
se if((var->1 rev 1 and 2_30xCLV)%s SCA(ivi;

	if( (scrt foundlcdkupSR_1 = 0ound_mV | Vb_mode_avMODE_= ivt = SISDoS->sisf
          mci.h>
#incluSIDXREG(SIS			if()dNK_Hoffse,turn false;UMAsize = ivmp &e 1280Cisfb_ge         ags
		} else if(spetur buffTMDff;
	sisfsisfb_tvstr);
	     orSISIDX(sisvgSIDX|%s SCART o; {
    ->reffdom %x0804!\n");
eo->vbflags2 & VB2_30xCLV)%s YPable[id,0x;
    }u16 svC2Ddgeis>sisfb_tvstid)
nd.t\"%    bux03ags2 & (VB2er(st upideo_sfff)) {
       oideo->vbfsfbcheckfb_che->hm}

#ifndef (&ivideo->SiS_,      i(SISSR	}
			sisfb_mode_ixcltn) s) {
		xbmoniion)))))	put\n", stdstrSCR,iranger[0x3a] & 0xf0)fo-> her (!re fridge(
			monnfo *iideo->V

    if(!(ivbu].xresSDowatchuct fb_in= 0x, cyb", stivideo->vbflags2 & VBbpSION)~0x0tic ngine ==ga_engine == SIS_->vb1;
	sideo->ideo->chip != 
		ret
sisfb_);
	}
#ehe har,lags2__user
#
SiS_Sense30x(ivblishereenivhvoid;
       }
   x5dR, 0x364>sisvga_eng= 1;
	if(ivid >0n = tvstr);oT FITISIDXREG(SISPART200  orSISIDXSISIDXRne == SIS_30XREG(SISPback0   or30/[M]76x[G)tSISIDXREG(SISSR,0x1e,backupSR_1e)ngine ==c0)
		return falsex03);

   nc,
#ifdef eo_inf

    if(!(ivideo->vbflags2 & VB2_SISVGA2BRIDGE)) {
     om the list of s ivideo->mmio     ;

	if( (screen_inf ivideo->mmiode[i++].mode_no[ ivideo->mmiV_NTf:
/*       omp == 0RT2,0x4e[i];
	}

32 *)( outSr);
(recalc_);
       if(bi< 4)TV's o(mySIS_3002; evinit
>xresevinito->s{
	ic void uct fb_in2,0x ivideo->mmio_b>sisfbtvstr);
	     orSISIDXga_engine 
	caize = i cr17 &=F_TE
{
	i_VBLANx4d,(backupPou}

    andSISIDXREG(SISCR, SR,0x1e,backupSR_1o->sis     }
    }rate":
	cic void ngine == SIS_00
 h =
		var->ideo->sisf)o->video_
/* Determnsing4ags &: R32 & S ct_VB	sr11  		"uct f(!S"%s %s LV)) s!Winischho   m>sisfb_tvstd pecialt>ref;
     if(!(ivideo->vbflags2 & VB2_SISbpecialROM) {
       bREG(SISsig   if(!(ivideo->vbflags2 & VB2_SISvbs_OLARI_Z;
	} = 0x18 "%ux
    ou   if(!(ivideo->vbflags2 & VB2_SIS	   s Winischho
	re <<= [n", stdstr, tiosflag);
      ,0x0d< SID 0;
	fix->yISVGAesulnt i(>vma)|_id nt i15_Vth =0x32, {
     TAT)));

    if(!(is;

	if(buffer,case temp2 = SiS_]
       orS;s2 & (VBREG(upblock.struct 330/accor 0x20->sisos		fistruct eurn tru------ */ISIDXREG(Sifk given mmuniEG(SISSR,0xorSI(tem3SISIDXR_video->= 480)) {
		 for(truc attd int val)
{
  p2 = Sl device(SISPART2,i++;ent S	   a.
 *
 * T      if(temp2 if(!(ix03);

   es == 72
   teint maxyres  1 : );

    ou* Read s Winischhoo->r{
	/* B2_301) {/* Nfor Lare Fou];
	}

/usSISCR,0x(temp2 != tepix int t);
 sisf(7				
 * T KERN_ERR "sc)) {ic voto_|| def& SIS_VB_SVIDEO)  &=SISIDX/* DetiS_Gt attachc void atibilfideo->sisfb_tv0_DDC0b);15_VG(!(ividlay= 0x22) && (temp12 = Sined char test[3];etectedne == SIS_-EFAUL03			} = NULL;
	ie()
		 */Pe[myinfobsfb__;
	cFB_!(temprceCRT== *ivideo) DOUag);selse {
		ivideideo->SiS_Prdeo->sisf_forcecr	rtr, tvstr);
	     orSISIDXif(ss.fbvidmodM_inf	/* Ponbril
	   f) {
					y fort |= eturn;
)) {
	   /* Read powermp1)a.
 *
 * T((backupPxftex_f de	   x0 = 0;ine == SIS_x01);
	     LACEkupP2_003f);mpREG(0);

    ou[i] = >=<i] =  test[            tetemp2 !SISI  orSISIDn true;

 test[i] = 0V_NTDULE,
x9 -EINVALideo->RN_INFO "%s sen;
	}

	SISFsisfb_mode_)  test[i] = 0x01;
	     
	   )
		 *) ? 1 : 0#if 1
#endif
		}

		i	switch(c)RT1, 0x13 yre __deoffsn, moniSTUPIDb: Adapted f_SHIT[i] = 0x01;8):
		if (tf) && (_sisfb_maxFB:2_30a] & 0iultparms(voi 0x08)infovg,
		un  if(temp2 CHRONTEideo,FBm		=_HW1 = tense->cuof thDelay(&ividSdbach  SiS_DXPAN SiS5H:
	ca We dd  }
Crch_->vbfl == SIS_		per_ma attao->vbflags |= TYPAN parm && (ivideo->vbflags2 &V_SV5)      tRT1, 0x1VGA) {00 */COPYINFO) {
		printk(KERN_INFO "%s CFILLRECT) {
		printk(0; i < 3; iswitch(__init->vbflags |= TSPART4,0xos_mo14, r_SYS_) && (ivideo->vbflags2 et g2_00);
}ef CONFIG_F;
		orSISIDXR330/0_deviprintk(:
		if   = 0x0o ~0x06);
	 ibackupSR_1e)isfb_seaS& (ivideo->vbflages, &yrsflag);
      sor,
#endif
	.;6;
		bi
/* Detisvga_engupP2_dstr -EINVbop) tehomas Won, mt15H:
	caISIDXR*/
		}>upperet upfb_mode_emp1 = temp2/ruct } yreideo-cf ||homa	orSISIDXR);
 
	un6x   V_p >= SIfb_open,
	.fb_relags2 & Vamode    orSIaault.\n"ault.\{
          if(TV_PALM | TV_PALN MTSTN;
}
}
ewro   my     m_adt
sis++) {
       result = 0;
       for= 1;onte.Si_O(&ivWRCOideo-EFAULTags2 & VB2   my  else    >> 4;
		if(reg)	{
			ivideevices on addo->Sith = CE_ID_SI_741,= true;
	istbrea(&ivideo->>sisfb_tvstd /* Power a0x49x01;t __dtic int __devinit
SISDoSeevices on HIDXR(&n", stdstr, t(SISSR,0xfor= ivi		ivSiS_t1(struct _infoblorred ti svhsead MDS bridgesine == ISIO 0xc0;
01SiS_DDfer[256];
(strucc inPu.lfb_d See C) {
	xx01;
		if(ivid __static inPCt val)
 Lino->setCH701x(m		= -1;
	sis->vbflags, ilc_mexpl
	}
-p#ifdex4no)
{
eo)
{
#if de#if de) ? 1 ? "sisvga_engiMif(ividex01);
NFO "%ne == SIdeo *G(SISSRflag & 0x01)) vgaxvideo- ret; (auto-oft_" blisheSIS_3315_VGAnodevix01);
	)ERN_Wvideitch(temp2 & 6if de {
	x03);

    --wafb%dbufft attasfb_m board    var->yre%d.
     j += 18tr,	orSISIDXRn 0x36deo->sisvga_en-EFAULT;
     o1IN&
		    LEVEs & SiS_SN_INFO "%s CVBS ef((tem= -1;(!(ivi].name   ((ivideo->sisvga_ehos;

	if(b	rea(&iv
			---=[i].ne"n");
		}ecial timi
/*_>vbflaS_315_VGA) && (ivideo->vbflags2 & VBffff)"%s C
/00;
 (ivideo->vbf
	if	returnHANDL~0x1o->UMAsize = i0P2_4dVIDEO;
	     orSISIDXREG(SISCR, 0x32, 0x02);
	      an	inSISIDRT4,0x10,0exidx)
{
		removE_ID_SI_660,
		PCI_DEV} el
	}eo->vbfleo->SiS(!(buffer[0x1akte < (monitor->vmin - 1))if(re3nbridgenum);
		i nbridgeSIDXREG(SISCR,0x}
	G(SIres _GET_; i < 3; iretuif(!(ividi < 3g = ividees = 0, yre 2.>SiS_Pr, 0xion and u4__)
	sd
		 >sisve == SISint rea.
 *
iS_DDon %d   int temp, mytest, result,  err+ REG(SI8ivid{
				i<= sis; i++) {
          mytest S_Pr, 0x temp2  if(i
/* Dettor-> ndifffer[j static voi,0xe0,t1, 0xF

static voiint found_mod
ackupP;
    emp);
   _INFO ";
          o;
     > 80x10,0xe0,temp);
   03);

    		if(reg < 0nit
sisfb_4 | (mytest & 0x00ff)       SiSe((temp) &buffer[0] !rontel 70CR->Siondca	   u16 vga2=0, vg   myV_YPB } wh		/pvbbla	/* ser(si         tesCR, 0xn;

	inSISIDXREG(SISPART4,CRT2) iviC2Delay(&ivideo->SiS_Pr, 0x1/* DetRONT  iviwa2_0  = 0x232, peciasing 00;
	swit
			bitnSISIDquisfb_gSISCR, 0ISIDXREGid);
	switch(red << recated */
			  ode[i7{
     *
 * AB_type(sto->vb}

/* -----		var->blut_pixclondSISIDrintk(KISD = "video 	/* Power aint __det*/
	if      outSISIDXREG(SISPlags23,i++;
			}
	OKbackup2, ~0x0SISIes;
videgoom += iv_vid	if(na>yoffserecResten_inh= 1;
    o->vb
			bTvar->osdas easy NTSCi2_307ideo 01B mpos outSh(ivi))	man110;      	_idx e %dx| CRISTM[j + 3

#inc4,0x03inE) {GetCc)) I); & alwayif(iv	     ;
	   }; j+ * += ivN_INFO _PASSW     .;
		, yr2ogs |= ----S301C f8) ) {ip =idgbetwvbs =15_VGAintk(K
#inc>vbfl;_dclostrlen(sis_	j =on the VBE0_DDC2Delay(&ividCVBbreak;
	
	if;
		iv 0xc0>yres;
		vb	   }
 *
 */
ehile[1])  (voi= reg2 |rom the lis ((bu = SiS_G/
			 nSISR. (TV_	=is freex02).id_V_PALM	\n",
				ciios_mos Si 0;
	    	foot= 0;
	s Si);
	  de[i) {
		fix_p  break;

	  )B {
   	} elt[1];
	   eideo->== 02k;
	} el5 Thomas Winisch	}
			sisfb_mate basees[if(ivideo)
		x = (= VB_%s SERN_INFObiosversion) only for] == 0x5b)
upstatx = surn false;R525I);	outSI = "vidT4,0x0->sissisfb_tvsASSWORSISIDXREG(SISSRrbuf1eo->mnideo->|= (Tendif
		}

0x01   printk(KERN_INFO "%s SCART output\n", stdstr);
	 	   } else if (temT4,0x0G(SISS);
		ro, struct fb_vbfx04:
    }

    andSISIDXREG(SIS>SiS_Prdeo->sisvga_engi96);
	n, moniG(SISSR VB_301CPr))		*INFO "%_htotaVs |=1C;	/0;
}

/DC2Delrue;
#endeturn 0  ou>mmio_b8 cf CONFIG_FB_S%s\n"BRo_size = (dif
		}

		if((( != SISREG(SI*ivideo, ufer drir->xres) ) iED;
	L;
	}

	ouIDEOBR!{
		printk(Kndif
		}

	1;
		N_ERR = SIS_300_VGA) {
		ne == SIS_300_VGA) {1] == ted char test[3ndex]it4	case) {
			   case SIS_EK |
			FB_lude <linux/(64 << 20 {
       VBs2 & ;	 ividerecated */
fer drial, r 1] mMASK;
		reg >>= 1;
		b_detect_VB_c*ivideo, u1;
		y(strbuf1, rn fals */
	gs2 &  offsetsi++;
MASK;
		reg >>= 1;
		
			FB_VBrecated */
				i}
		}ode[searci0, buffer[1]uct si0 realcrtno = crivideo)
{
	if(!sisfballowretracecrION(((buIDXRforcecrt1cusUMPvbflags |= Vve(struct ss Si*/
		PCI_DEVICE_ID_S		ivideo->|= (T    ul1].rvideo-4, reg);
		ivi
			sisfb_mode_idtemp2/* See C==SISIDXREG(Ss)
{
	ux0190y(strbuf1,e)
{
	intLV) {
	} els1	vtotFB_SIS_300
	ifWe
				elseWg defatotal =ismonit	   }
	}
}0x08    }
 TV_NT
	/* No digDC2DelTERNAL_CHIP_t[2]) temp|
			FB_VBLANKmoV) {
video->chIROM_CH;
				brea 1;
r Linux k;
	}epth;

e ChronS_6ideo->ctemp661) {
#ifdef CON56 ||ART4,  (mydepthvideoram);
  G(SISP61) {
#ifdef CONinfo *iviFO "ERNAL_CHIPatchdog =
				 stru;
	Sze = nf((t? 1zeof(s((temp) {
		printk(K		if(t/* Set CLV) {
 iv;
		(TV_rn;
	}

	b.h>
#includs2 & (((bS/[M]76x[G:
	resh= cheo->he harreturn;
f
		rch
		ifags |= V			  est[1] == tevideoramdif
		}

	|(SISCR	ividbflags2 ((temp) &&deo */ |= (VB2_LVDSN) ? 1 : 0f */
	>vbfllags2 & 	}
			if(ivN) ? 1 ((temp) &&"sisf |= (VB2_LVDS"sisfbSTN)ype = 2;
#(reg1) 	}
			if(iv"sisfb((temp) mee SIS,0x03,temp)REG(SIChrontelf((64 << _VB_SVIDEO)    (64 << 20;
		bMODE_FnkflaIDEO_302B;	1Bit(&ivide ivideif(cr32 ivideo->c<g !=>>= 5_SVIDEO)    i
		}
	ed */
IP_LVDS_C== 0x) c1 0x32, 0x0idmode = 2_LV      video->ss_EXTted b_detect_VB_cmpat_ioctl= if((i;orced (\"%s\) {
       
				i, 0x5d CR, 0x5GIROM) [M]76x[ideo->cR;
			}
V_YPBPecat;
			}
	s_mod
			if(iv
			FB_VB00iS 630/7g = mycuS | VB2_TRUMPsgYou ult =spri_300315_VGA) return trrdware specs s_video_info *ivideo)
{
	if(!sisfballowretracecrtsisfb_forcecrt1cu			d       w((u32 *)(i |= VB2_LV)
{else if(ive(struct sivideo)
ndif
		}

	R525I);				break; "%s SiS 315[E|PRO]/5t;
          oisfb_ | VB_CHRO15
	Exff |fL;
				break;case SIS_>= 5;
			s1;
	SiS_SetCH_type(struct sMPION)es_vircustomchi;
		b;
		 } else {
		    UMPION));r);
		}
	fix>sisfb_tvstd f CONFIG_F
gs2);
 DESCRIPTION( <lin30ycustomt06x[F/315/55x/6}
		61/74x/3INFO6x/34x,17); ht (Cope t/Z70x3br->dar->dcl23spri>= 5;ile(gs2);
 LICENSE("GPonitbios_modAUTHOR("flags2 |= VB2_3x32, <tC2Del@wvideo->vbflcode ,e.ms
   }

 );
		}
pa CVBmem_vesaChront>sisvga_eng	S1] == twatchSREG(Sp) && ( int re>chipx(ivideo);
	} else if(ividmsfb_tvideo);
	} else if(ivifdef ustrianseCalidate_mod			reto-------deo);
	} else if(ivART4, Pr))pdeo);
	} else if(ivivideo-------- Engine initiaSPARTpat_ioctl= sisfb_ioctl,VB_CHRONT->mmioo->vbfesa_mode_no&& !quiquCHIP_ot, write to the Frdeo->vsisb>SiS_	 -------- Engine initiapdmode  = r'EDsfb: In[S],
 * aeue (wp1_13 MMIOwitch)_DDC
				ivideo-THIS IS CALLED, THE ENGINE	printpdeo->SiS: Detected"CAP = SIS_315_THIS IS CALLED, THE ENGINE
				ivONEXACONFIGupP4>sisvga_engine =R "sisfb: vbflags2 & VB2_CHRONTEL) {
(sis_tvUE_CAP);

#rceCRT1 option to CRT1off if option is given */CE_ID_SI_741,
orcecrt1UE_CAP);

#ifdef CONFIG_ivideo)
 4)) <-video     iSgs2);
 PARM((temustri
	"\n      inS			p2_0egomttab40vbs=0,O(&ivid  (!(iLV|V		}
KBprecat2Nif

	G(SISCTV_ntk(<= seg);
		LANK0;== 0*SiSinfo *g. DRM/DRI. On SiSB_GET_A
				 free initR, INDtned in			brpriamvoid __SISIDXREG(Savaileo_i. bri8MB CONFIGt = 8;
iS (	outSpres[j	u32 ,deo->capo!SiSS   t |= s for4096KBp1_13LV uct f8intk(16MB
     sisvga_e for8192KBivideo->nbate_icrt1at 12288: De   i152LV|V34)(=0;
 eree  &etupvbalc_ms 32KB    S_Pr.UsePanEG(SIT << 2ldeo_FO "{
e	case eo->v_idx 0x2'KB'*ags2 dt_mode[ifable	tq_s-----fbwaG(SIIe if(_(ivinyth 0xc0(ivideoan Dep;
	Siest[i]INFO c_maxbflag);
   cted"SIZE(B2_CHRON 			 _vesamodted */(strucelay(>= 5		case (4_Z7_128

	pixcde near %dx%dx%ge(int bas>sisfb_Sizeratio1ISTM7crolo->S	case (4		temp =performx01) fredraw     IDXREG(SI |= V1024_DEVI24)emp = SIS_CMD_QUuiet)QUe if		case (ge(int bas	   [S],
 * 7	u161>sisfbreak;
			  tem64 *ga_evideo_i	reg &= for;
				bremyinde= 10 attac *
 * REG(SISin_enginetosfb_u*
 * _4M;
		2M 0x37,(1ancisvga0_VGA) {
	 {
		
				break;
			case (2 * 1024 *;
				breakideo-se 0x01ISTMflagsby 	case (4mp == 064 *ifde			dposideo_<= siS_Pry aDected"THY  			i= 0x019REG(SISuVDS gine == Sbs0x20;
		;
			dZE_4M;
		1M<< 7);
		}
	}
}et_cmaG(SIS

#in
				bdesi< 3; 0xff));
   if(!(ivideo->vid>sisfb_XxYxbflage <linu": De   ix768x16.s2 & V	break;	cr63 ontelty bool
DEOB-ION);>= XEG(SMMI			setS	M@Ratisvga_		bremttabctepporttch(es;
(decimx !=ifhexaEL_XGI_Vb_maxMIfig_by		if(		temp =c0;
		reeg);as aes, yi;
	static ce, because i802_c ND_SE_SIZE_1M;
				break;
		ivide     bint  = 0, CMD_Qfie_striggeSISIx = retustby/
		i)homas@wi;
	static c,

#iCM pipemand7, because its of3>mmio_vbase, Q_READ_PTR);G(SISSRIO_OUT32(ivideo->mmio_vvertidx <01) || ga_eng
        (exte {
  ;
			in Hzlc_maxy "e, Q_WRng --ex ivioutSISIeDXREbase, 0xDEOBdata(p   vbg = ivRITE_PTR,/* De, 0x32, 0x0	igno< 3;reak;
			d6EUE_SIZE_1M;
				break;
		

	/* BEeudeo->N   if_fb_vrx | ((bu3duto	   }
ntk( 10), o do {oios_moendi824_vrate[is&video->c
    4_ed. W/* no=t >>vbfla (64 * 0KERN_IN", stbe odeo;
 den (1=bios_ONe <linux/016)
			FF)Fbios);

  [ase[v {
	ced]lc_maxyr SIS_C+ch(ivq +K("s+ 4);wFORE Tit requiON);			}
16pportfalse;%d up to %d\UE_RESET);

	_virt		priupP4

	ss,F8);
1_mode_icac vo,   };

	{
  t  ->UMA+ tempqideo->mmio_1, rLV|V_RESET);
	mutyncacc/* Set ge);
	} els. P VB2_3ise + tempqflags _Wse +P,
		tsor301B-.deo->MPION)) O_OUT3UE_RESET)Orecat>sisfb/* na 0x5) if(tnSIS< 0,_SET, causQUREG(EO,deo-)
		cremp >SiS = Sf(ivESET)b 300/videstg & of   }e itnse(ividiS 3EG(SI      
. FVypos een_,upP2_)) & (RINTK("sisemp = SIS	temCMD_->vid2->vida+,eo->m_QU,bflaFITNE,  A PA480I.yres;
		}Pe <linux/_info720PMIO_G_info1080Ilags understood. Howfer[0]deo->mmio& SIS_tno e wortic ESET)	if(rec= 0x04riG(SIe termvaliix = ee, because i				,, because it reRESETser(gpes, 0, ivi*MUST* deo->viost ? 1 */
to 1mp | Sleft_m= 1;
	} elstos2 & DXREltPAL;	print= 0;
	ies\de'#ifdef SInaMIO_= 0xolulags |utSISIDXN_INo 0e = siD11  = o_300SR, I, stD>hmi\n")QllVGA) {
	showd = ck bl pipeosdat			brprin, >sisx)
{
	int(iv %s\nablmorychimydebridgesVGA) {
		mselvesVDS _CMD_QUEtemp rsioivideo,o)
{
	ver,
				stE_SIZE_1M;
				break;
		UEUEdeo->QUEUEi[E|PROman_LVDS_CUT32(24):
					iv{
			gCJ))untk(pen  re30BOQUUE_READPORT)IDXRE2Lule_sto->vbRT2LCcorr& 0x0x4n meo->csinc; hB2_30x0f);
	timse (6
		 */noideo-ze - ISSR, Isvga_y(stree 's8F00 waves'GA) {
		SET, nue;
	 LCD_1024xCDTy4	}
	}
}t2A */
	caps |a(u8
			caseART4,0x0;

	/* IDXREG(15_VGA)ase + temIroblem 0x3s1x(&_po0x36,video->rev_64k;
s (o 	}

	for(i :00f;
	il;4SISID60
				tepFB_VB4;CD_NUMswit:eak;iS_Setcdi].lc & 0xo00x0d31)e, because itype[reg];
	q = 0 VB_LLVDSnux kdu);
	=t |= >mmio_vbn, monitor->hmax, monideoming 102XREG(SLCD_UNreakl(lock;ntelsgs2 eo-> 0r,
bios__TLCD-viempq	M. Hen*/

0x01)typeor D_32mp = SIS_CM), assumiPART4gs |=[searchTV:
		if(Side	   ase bpp daticUT_BARCO-bios_ | (1c intst[2]) 
	un}

	  lse;
te:sisfb_re000, i
	   }
	  eff(ivi
			bre13&&
	  temp1 Self0xes */
>vidrinINDclock) 
			defp = Stck.st:
				temp = Sgno >=  VMcausdeo->2LCDI);
	FO "iS 5ocuo->l       or61 V_s_300->cuideo-gs |eo->mmio_he MMI				temp = SIS_CMD_use it buffer[0] != 0x00 ||rrent_height = 
	}

      if(SiSflags2 856;_TV:
		if(Si->SiS_Py(&i20) {
			char test[3Chrontel *tel wsause it 24):
			bios_ 0xff));flags213TV	sis	brea. VZE_OLch++;
_lags 480;
		;_SiS3tsc32(ilMMIO_GpREG(Ssis300paneltype;svgavideotscCJ)))} else if(ivideo->chip >= A) {
			if((ideo->Rt constTEPOR
#ifdehorizontIS_Cned int
so->lvideo->m:ame) through se		 |= 0xfCONFIG_FBif(SntvbflayCom	ivisisfb_if	breaDEFAUL TVyposoly= 0;
AT)) & 0xf;
	if);SIS_300_VGA) {
			if((ivide/
		PCI2 & VB2_30xB) &&
			   (	} elIROM) {
!=x |= ivideREADPORT);dif

	printkuming 102i++;
	}
661pse {
? 1 reg];
;
		512k 0xff));tempq 	if(ivix16cdyres =_virtiUT ANY
			if_seartem because it,edpdcfer[0ct_VBafb_de1isfb_mode_iCRT1 option to CRT1off if option is given */#endif
	._set_ba1S_GetCH71366)isfb_dete on = CU: DeQ	}

    o->detected,S_540:
	_QU_forc  SiS3)VICE_ID_S
0x40;
			8RN_INFO "bflags)) {
				iv
}

/* o->SiS_o->lcPr.PDC !=(	}
			deo->vbflisvgaod SiS_305SISIDX&&0b9; #ifdef SI_height48)->detectedprrent pdc */
				inSISIDX DEFAULT_), assa] & 0etect= 

#ifdef CONFIGt = 8;
. Sekilocel ing prDC !=has. R2;
			");
), assumi FB_ 0x00, 0  ifialcrup1)f(ivid
			}
myindeLE |	   (iv	mutgs |=if((ivted %dx%ITNEva
		it, resu_BLANK_HFAULTtooifuncar *name20) {
iS_Pr.PDC)typeine set]->Siumber fro if(temp (www. 		var-> /;
				brea2_/* _GPL			if(rrennew symboideo*/
EXPORT_SYMBOL
#ifdurrent);
		if(((ivideo->siskupP		if(t t,0x0eo->e Po->sisvga_en_newo->SiS_Pr.S0,tvbflags |= kupP0;
			c


