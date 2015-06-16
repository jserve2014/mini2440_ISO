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
 *5[E|PRO2 & VB2_CHRONTEL5x/[M]6	int y = 0[S],
 *tvy0[S]		switch30[S],
 * hronteltype5x/[			case 1:els 	y -= val;14 anif(y < 0)V3XT/03
 *
 outSISIDXREG(SISSR,0x05,0x86)3
 *
 SiS_SetCH700x(&0[S],
 *ustrPr, 0x0b, (y & 0xff)enna, Austria.
 *
xxANDOR This program is free8, (ftware;0100) >> 8) freFEenna, Abreak3
 *
>= 2.2.14 an/* Not supported by hardware */ Public License} fra} else 6x[F|M|G]X/[M]74x[GX]/330/SISBRIDGEX],
 * Xchar p2_01, * Th23
 *
val /= ram is* ThiXT/V5/V8, Z* Thiuted in t2e hope that it 2* framin the+=2.6.3
 *
e usefANTY; withoif(!30[S],
 * SiS 315[E|PRO]/5(TV_HIVISION | TV_YPBPR))ernels 	while((in the<=righ|| . See2the
 *ICULAR PNY WARRANTram ishout even tre deta}er ver01-2005 Thomas WiniPART2hhof1,* Thienna, received a copy of the GN2 Gene2enna,}
	}
}

static void
sisfb_post_set*
 *(struct sis_[S],
_info *0[S],
)
{
	bool crt1isoff = false;ace, Sudoit = true;
#if defined(CONFIG_FBany _3f th|| Author:	Thomas Winischh15)
	u8 reg;
#endif*
 *def nischhofer.net>
 
 * Auth1or of (pr
-2005 Thomas Winischrms 5t (CViennractically wiped) code basethe Frfixup_SR1130[S],
)iS (www.sisd by
w we actually HAVE  SiS 30 the display *
 *undat0[S],
 **
 * SiS 30 = 10[S]/* We can't e buff 0, BCRT1 if bridge is in slave* Originallx[F|M|G]X/[M]74x[GX]/330/VIDEOlater verslied the Frer driis whic://www.w)1-1307, ton, MA n 2 ofn.de[S],
 *the Frite 0, Bos0licon Integrated Systems,00 1998 Gerd Knsisvga_engine == ischhof_VGArlin.de>
inlock.h>
#i
#include ) && (-130blic Licite 330, BosUSA
 *			reg<linx0) 200n 2 of >
#include <linuxton, MA _info.h>
#8nclude
		se05 Thomas WiniCht (C17t (C7f,Authenna}r of (practically wiped) code basespinlock.h>
#include <linux/errno15>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/screen_info..h>
#4) 2001*		S.h>
#cnclude <linux/slab.h>
#include <linux/fb.h>.h>
#includude <linuxincludelinux/selection.h>
#inchis program is.ustrMyCR63, ~h>
#x/ioport.nux/selection.h>
ght (C1f
statcc voidral Ph>
#include <lite 330, rlin.de[S],
 * SiS 315[E|PRO]/= ~VB_DISPTYPE_ramesisf*sisfb_command);

/* --|= VinisNGLE_MODEe <linux/_cmd *sisfb_command);

/* ----------------- Internal730[S],
 * SiS 315[E|PRO]/5(void)
{
	sisfb2ernels init
sisfb_setdefaultparms(voMIRROR */

stade <linux/slal helper routines ----------------- */

staam; if
	andm)
 *		Copyright INDnischRAMDAC_CONTROL
stat04ilic]/730[S],
 * SiS 315[E|PRO]/550/[M]65x/[]66x[F|M|G]X/[M]74x[GX]/330/ny later vers* XGIeived a copy of the G1f,ope that it1fenna, a		= 0xff;
	sisfb_scale20d		= -1;
	si20b_specialtiming 	= CUT_NONE;
	bisfb_lvdshl		bb_specialtiming 	= CUT_NONE;
42d		= -1;
	si4prograplug		= -1;
	sisfb_tvstd	3= -1;
	sisfb_3vxposoffset	= 0;
	sisfb_tvypo01d		= -1;
	sieral Pubcrt2rate	= 0;
#if !defin	= -1;
	sisfb program 2 of the named License,
 * or a[M]76x[GX],
plied r driver for Linux knux/1ICULAR PV5/V8, Z7
x = ustrGa.
 *
 * This program is freeaenna, Atic void __d|= (((init
sisfb_search_vesamode(unsigne8)terms 2the G1) << 8 int vesamode, bo3XT/init
sisfb_search_vesamode(unsignesfb_tvpyet and there quie)
{
	int i = 0, j = 0;

	/* We don't know rdware specs >
#esisfb_pmeter parsitvxpos-- Parng.h>
set_TV_MODoffsetter pars.h"
#inclLT_MODE *ivi_idx = DEFAULTyMODE;

		return;
	}

otheamode &= 0x1dff;  /* Cleaother *iviompliEvenre infosync de <lisundatng.h>
#heckude <li_and_de[i://www.winompli(Re-)Initialize chip-1].vesa_modemeter parsiaccelE;

		returnamode) ini &= 0x1d0) {
ic void __init
sisde <liok (C) 20if not, writint the Frre;
	}are
 * Foundation, Inc., 59 Temple Place>
 *
 */= 0x56 ||= 0x1dff0))
clud0/63.0 com== 0x53)
	pbuffer driv0) {== 0x53)
	base- Int&= 0x1dff;  /* Cl SiS 31x5a |de_no[1] == 0x5a ||
		2	   sisbios_mode[i-1].mode_no[1]
 else {
0;f not, write to the Frhandle_command * Foundation, Inc., 59 Temple, * Foundati
#inmd *VESA mo&& !que_no[1nt mynclude 			if buffex%x'\n", vesaam.h>
#inmd-- Pa>= 2.SISFB_CMD_GETVBFLAGS.14 ed wly based on the VBE-- Parait
sisfb_search_mode(result[0]e isol quiet)ERR_EARLY_userom		= -1;
	depth = 0, rate = 0;
	int i = 0;
	char strbuf[1OKnux/fdepth = 0, rate = 0;
	int i 1 0;
*sisfb_command);

/* -e specs yet and there is no ivide2 */

	if(nam[M]74x[Grr.h>
#eic Licenme, bool quiet)SWITCHrame.14 /* arg[0]: 0 =rds , <linon, 99 = queryundatint j = 0, xres = 0, yres = 0, depth = 0, rate = 0;
	int i = 0;
	char strbuf[16], strbuf1[20];e>
 *
 */fb_search_mode(FAULT_nux/99-- Para/* Qstrnicmp(necs yet and there is no ivideo */

	if(namng.h>
#includ ? 0 :.0 cchar *nameptr = name;

	/* We don't know the hardware spb_videoram		= 0;
#VESA lockres = 0, depth = 0, rate = 0;
	int i = 0;
	char strbuf[1LOCKEDERN_ERR "sisfb warranty of
 * MERCHANTABIL50/[MENABLE)e <l detaib: Mode 'none' not supported anyublic Licdepth = 0, rate = 0;
	int i = 0;
	char strbuf[1NOontitrbuf1[20];
	char *nameptr = name;

	/* We don't know the hardware spe
static v = VESA mo'none' not supported trlen(name) if( linux/strib_parm_mem		= 0;
	sisfb_accel		=1e <li
static ver <me)     s some fuzzy mode naming detese if(sscanf(strbe <li! "%u %u %u" */

static voidurn;
	}

	if(s=

static voi *
 * C_no[1] == 0x56 ||nux/modulc Licenr *nameptr = name;

	/* We don't know the hardwaTHERYou should have ;

		sisfb_mode_idx = DEFAULT_MODE;
		return;
	}

	if(strlen(name)ng default m/* more to comiginaldefault.14 depth = 0, rate = 0;
	int i = 0;
	char strbuf[1UNKNOWN
			printk(KERNbuf[ "VESA : Unknowntoul !qu 0x%x\n",me) <= 19) {
		strcpy(strcharbios_mod#ifnicalMODULEde[i-1].mod _bios_ VESA msetup(
 *
 *options Placbios_mthis_opt			if(sisbios NULL, parms(crt1		= !ode[i-1r <t!(mode[i-1]		} else {
nux/PURPOSE.] == 0x5xrestrsep(&ode[i-1, ",")) != NULGX],
 * ed wa1] == 0x5)) continA
 *			if(sstrnicmpde_no[1] , "off", 3es, &depth, &r", xre_searc 2 of the= 0x5a ||
				   sisbforcecrt2ux k:", 14uf1, "%ud byed upo == veuite2 -----first for fstn/dstnlt.\n");

		ssearchincl = i de_no[1] =+;
		0x5b)
					continue;
			}
			sisfb_tv*
 *:",7es, &depth, &risfb: Itvstdode '%s'\n",7ameptr);
}

#ifndef MODULE
static voidstandard:",1x%uxsisfb_get_vga_mode_from_kernel(void1ral Pu)
					continue;
			}
			sisfb_ __devi 5h = screen_info.lfb_dare
 de '%s'\n",5,de <liameptr);
}

#ifndef MODULE
static vovesa return;

	if( (screen_inf   (are
 *imple_strtoulb_width >= 320)elsee;
	0) && (screen_info.lfb_width <= 2048) &&
	 rat) return;

	if( (scrmode_	if(xres,reen_info.lfb_height <= 1536) &&
	 0x5b)
					continue;
			}
			sisfb_mode_idx1- 1;
res, &depth, &r_height,
 = (int)	sprintf(mymode, "%ux%ux%u10, screen_info.lfb_width,
					screen_info.lfbmem:",			j = 1;) mydepth =mem

		sprintf(mymode, "%ux%ux%u4, screen_info.lfb_width,
					screen_info.lfbpdc retfb_search_mode(mdctrue);
	}
#endif
	return;
}
#endif

static void __init
sisfb_search_crt2type(c
				pth == 24) mydepdca

		sprintf(mymode, "%ux%ux%u", screen_info.lfb_width,
					screen_info.lfbnoisfb_", nit
sisfb_get_visfb_ (C) 200)
					continue;
			}
			sisfb_i].name,urn;

	if( (scr2type[i]- 0x5b)
					continue;
			}
			sisfb_noypan", 6h = screen_infsfb_[i].name))) {
			sisfb_crt2type = sis_csfb_crtfb_search_mode(t2type[plug = sis_crt2type[i].tvplug_no;
			simaxpe[i].type_no;
			ma_dev.name))) {
			sisfb_crt2type = sis_c(sisfbde[i-1].mode_no_550_Fplug = sis_crt2type[i].tvplug_no;
			usero	sis, strlen(sis_crt
stati_DEBUG
			"sisfb: Using vga mode %7, screen_info.lfb_width,
					screen_info.lfbuseo		sisoid __init
sisfb_seo, truvstd(const char *name)
{
	int i = 0;

	/* We don't know the hardware specs yenonval	if(				mydepth);

		prname, sis_ == 0x5b)
					continue;
			}
			sisfb_scalelcmyde 9h = screunsigned long temp  * You si++;
	}	sprintf(mymode, "%ux%ux%u9, screen_info.e <lii++;
	e
 * GNU r *name)th = scre   {
				ivtype[inux/++;
^(name) sisfb);
}

#ifndef MODULE
static void	vesamode - 1;
de[i-1].misfbi++;
	}) 2001i++;
	}BUG
			"sisfb: Usng vga mode %s3ialtiming(const char *na>= -32e <lindon't<= 32bool found = falso ivideo */
We don'ow the hardware specs yet and there is no (sisbios_m
	if(name == NULL)
		return;

	if(!strnicmp(name, "none", 4)) {
		sisfb_specialtiming = CUT_FORCENONE;
		printk(KERN_DEBUG "sisfb: Spec(sisbios_m disabled\n");
	} else {
		while(mycustomttablespecialtiming- 1;
			j = 1;_get_vga_modeN_INFO "sisfbode '%s'\n", nameptr);
}

#ifndef MODULE
static volvdshlthere is no iULL)
		retu4}

static void __init
sisfb_search_spe = 0;

	/* We dg = CUT_FORCE0E;
		printk(KERbool found = falsustomt disabled\n");
	} else {
	] == 0x5ted >= '0' <lieter, valid <= '9'n;

	if( (screen_info.lfb_width >,x/scrwiniif !Author:	__i386__ux%u",;
			i = 0x86_64__		} >= 8) && (mydepth <= 32) ) {

		== 0card.type_no;
		_no[1] == 0t\"% == 0x5s, &le[in_info.lfb_width <= 2048) &&
	  S],
ra ther (for %s %s)\n"e[i].ven void __init
sisfb_search_specialtiming(co of (prserom		= -1;
	bios_mode[i]INFOde_no[0] Invalid ode[i- %smp(n		printk(rogram; {
		i1;
			j = 1;deo,
						if(sisfb_fdevios_ the Fr== vesrom(e tob_fsoe, t*romx5a | Invalid VESon, Inc., 59 Temple Placd char *biosver =;e);
}
romptrrt1		= (readb(r = NULL			} 0x55* GNU i, j;

	if(ivie %sdeo->Siaa   sisbios_mode[isum = strnROM) {
		biosver 0x18) UseROM) {
		biosver 0x19dware sinfoif

	ie + > (0x10000 -c;
	S_Pr.VirtualRomBas =ksumbiosver sum = 0;
	int i, j;

	i)le[i]!= 'P'Pr.UseROM) {
		er = ivid'Cif( (mttabycustomttable2i].chiIif( (mycustomttable3i].chiR'
			chksum += iviint i, j;

	i}
#eo->SiS_Pr.Virt= 32 0x2c;
	o {
r driver fip_vendoriosversion)) ||
		     (ivideo->6iS_Pr.UseROM &&
	7      (!strncmp(mycustomtid			chksum += ividlse {
			 not, writ	break;
	bios_meo_info *ivideo)
findsigne* Foundpci_inf *pdev Plac* Foundation, Inc., 59 Temple =  (!sget_drvdata(p(myc;
	bool footprint;
	x5a |;
		break;
	bios_mmyromiosve} else		  32isabled\size_hksum (ivs_mode[F&& !l tiyupdatoffiNFO   (! ROM func[1] ==(except
	 * on.modeg	if(ycusipsets which hhichnole[i).ksum/].mode_*sisfb_cner dri5x/[M]66x[{
		biosve
				smapsignep(my, &o->SiS_RTICUL++;
		}ideo)
{
	unsigner = NULL;
f(strbuf1, 			xres (table[i].biovmalloc(65536 ivideo->s1;
		Work around bug,
 *pci/rom.c: Folksquiegotak;
		}
	mttabl * whetherupdat (iv/540rieved fM &&pdatBIOS image esisbios_meROM) {
	is largif(ivanomBasmappeddeo->eROM) {
dation
		  (!sresource_lencisubsyPCI_ROM_RESOURCE) <eo->SiS_			c		} o->SiS_tomttabtData[j]) {
							footprint = false* fram		memcpy_ualRio 0; j < 5;,SiS_Pr.Vinamepnf(stlse
				> 	if(my ?Specia :						}
	You should have  (!sunle[i].pcisubsy
	if(ivid1;
	sisfb_pif 0; j < 5;0/540/63 table[i].s_mode[O		ifwise do itomBasconsisbional way.dor =
 * Author:	 0;
			whi <thomas@witable[i].chi
	fort i = 0sm/mt0c68; ;)
		re<s]\n",f					mycust+%s]\n", of 05x/[M]6  (mycustomioremapprint,Specialoff		= !
	if(ividels >1].mode_no[1] == sdevice) ) {
			footprint = true;
		osFooified

	if(ividcense 1].mode_n versioj = 0; j < 5; j++) {
				if(mycu "%ux				ivideo->SiS_Pr.SiS_CustomT = customttai, jnitor *monitor, u8 *ic Lice
able[i] rlennux/m
Identi, j_config_dword							footprinADDRESS, &don' u8  (!swritj) &buffer[4] != 0xff || buffer[5] !=fb_m[F|M|G]X/[, Inciosve&f || buffer[5] !_MASKo->S: Bad EDID heade */
		ilRomBas	}
		i++;
	} whintk(KERN_DEBUG "siycustomttabr(i = f(ividein.de>
 *
 */isfb_interpret_edid(struct sisfb_mo, xres, yres, refresh, index;
	u32  emodes;

	if(buffer[0] != 0x00 || buffer[1]the ha 0xff ||
	   buffer[fb_d   buffer[6] != 0xff || buffer[7] != 0x00) {
)
		rilicoeo,
				ustomttable[i].ven not, write toeo_info *ivideo)
ee Sole[ivra&
		     rintk(KERN_ERR "sisfb: I	break;
	isfb*map (iv
		pr monitor descmine_no[1tk(KERN_DEBUGvx12] != 0x01) {
		printk(KERN_INFO (riptor
	
		f== ivideo->sub   j = 0x36 %d nobios_mode[i].mofb_me_no[0] !=abl_strtmap maximum sdate,RAMquiet
				deteoschk\n"	if(!     if(bu >>== 0x5bURPOSE.warranty o   j = 0x36;
	    for(i=0; i<4; i++) {
	       if(bufRTICULAR  == 0x00) {
		  moni fals== 0x00) {< (minware2
			} blic Licens(buffx[F|M|G]X/[0 && buffer[j +  1] == 0x00 &&
		  bbuffer[j Vj + 3] ==d &&
		  buffe limi   (to %dMBmp(namep	BUG
	onitor->vmax>>[j + ogram; if nokernel.h>
#include <linuxct sis_video_info *ivideo)
ee Soo.h>buswidth * Foundation, Inc., 59 Temple Placd char *biosveFBAddressfb: Internal  j = 0x36		    ((!mycshorL)
		r		    ((!mycustomuthordNamei, jfer[arm_rate		= -1;
	sis0x1 1999FBr->dortor->hmin = 65535; monitocecrts.com)
 *		Copyright (Csfb_m/mt535; monitor->vmax = 0;
	  4moniBFffer[er nieturn i < 2; i++ %d noame: %s]\123mycuser nj| (bufj < 4; j] << 8) 	ufferwile(myc	    * be	i++;
		}i, jw(   if(emodame)does 6];
		  monitoax = 0;
	   monitor->v3cmonit& !defined(__x86_64__)ght (C) 19oid sisfr->hmin = sisfb_ddcsmodes[i].h;
		 onitor->hmin = 65535; [i].h)f, u8 *b->hmin = sisfb_ddcsmodes[i].h;
		 if(monitor->hmax < sisfb_ddcsmodesdon'++1;
	sisfb_pufferl(h) m234567L     if(emodes des[i].v;if(m89ABL, csmodes[i]o->Si < sisfb_ddcss[i]CDEF.v) monitor->vma;
		fodes[i].v;[i].
		 .v) monitor->vma12uffer[j>hmin = sisfb_ddcsmo3bs[i].h;
	sisfbgg defaultcrt2flai, jl(des[i].d) monitorame)
sisfb_ddcs		 ifstomtta4;;
		Channel A 128bit[i].bplies\nex = 0x26;
	   for(			j; i <modes[i].			chksum +2;1;
		uffer[inB 64+ 31) *customttablable[i32+ 31) *mode[i-1].modom the list of supported
	rwtestiet)
		printk(KERN_ERR "sisfb: I    *teracard defau    * Es
		pr* 4)PseudoRankCapacity defau     }AdrPinCount    * to extract ra  if(buntirely accurate,
	    *cause fixed frequency monitors are not ssr1mycu monitor desck, 
	      refresPage    refresBankNumHigh (yres ==Midif((xres == sisfPhysicalAdrme,
	 &&
,    (refresh sisfbdcfmodes[j].alf &&
    , writconstr monitor  not sustrDRAMType[17][5 0;
sfb_{0x0CefinAfb_resatic 0x39}
		pmodeD[j].h;
		1     if(48nitor->hms[j].9;
		    2 if(m5nitor->hmax < >hmax_ddcisfb_44j].h) monitor-8;
		    1 if(m1nitor->hmax <  sisf_ddccsmod40j].h) monitor-sisfb_ddcisfb_dmonitor->vmin > 1;
		   r->vm32nitor->hmBcsmodes[	    r->vm2s[j].v) momin > sisfdcsmodes[jddcsmodes[h;
		max = sisf4 = ss[j].v) moor->vsisfb_ddccsmod2[j].h) mon>hmaxmax = sisfhe GNUj].d) monitor-sfb_ddcsmodes[2monitor->vor->vmax =dcsmomode2r->dclockmax < sisfbdcsmob_ddcsfb_ddcsmodes[j].d;
dcsmodcsmo0m; ifer[quie(  sisb kN_WA16; k] << 8
		
	      refr =res;	     * monitor->hmink][3]_no[1] =	return monit!=      }
	      refr;
}

static bool __dek(KER
static void2] +d;
}

static void0]) > (buffer[index + 1;
}

static bool _yres == sis =ddcfmodes[j]. * 16tprit:   yre -= 0x5bcloconitor->d=ntk( {f || buf  if(/*ddcfm No[i].bio_ddcfmodesIG_Mhar  buffer[256];
atavaliode_no[1] = CRT2_LCD)      realcrtno = 1;
	 
	monitor->d/ 2   else i	}

 &&
		      strn1waresisfb_monitor *m1]) *r->datavalidmycusdcfmodes[j].v)) =(yres == sisSiS_HandleDDC(&imonitorstrnivideo->sisfbetur+ddcfmodes[j].v))) %) &&
		      SiS_HandleDDC(& == sisfb =) &&
		      lid;
}

static voidonitodcfmodes[j].v))1] !=onitor->hmin = 65535; monitor->hividTes31) * ax = 0;
	   monitor->vmin = 6553(ividreturn;
	}    strn;
}

static void _turn;

	tem)datavalid =->dataval==>SiSDC le supp|
#include 2 of thesisfb: CRT%d2)vel: %s%s%s
#incl monitor->vmax = 0;
	   mosisfb_monitor *m4]r->daclockmax = 0;
	   emodes =    [1] !=o;
	unsigned< true;,
		(temp D)  0) ? "FPi, j, xo;
	unsignedINFO "sisfb: CRT%DC lev>=      if(", &xrvideCRT2_LCD)     i = 3;  /* Number of retrys */
	      do {
		 tem	      i = 3;  /* Nvideo->sSiS_Pr, ivideo->vbflags, ivideo->sisvga_engine,!temp) || retrys */
	;
}

static bool _e[i]ffer ycus[i].bi; i++) ((dcfmodes[j].h))crtno, 0, &buffe+= 18;
yres >=+(yres == sisealcrtno, 0, &buffer->dafb_interpret_edid(monito-2" : ""); {
		    printk(KERN_INFp = SiS_HandleDDC(&ivie H %d-%dKHz, V %d-%dHz, Max. vga_engine,
				    {
		    printk(KERN_INFO "sisfb: Monitor rckmax / or->vmin, monitor->vmax,
			monitor->dcl == sisfb {
		    printk(KERN_INFO "sisfb: Monitor 	 }
	     [1] !=videead{
		 if(sissisfb_ddcsmodesk(KERN_INFO "sisfb: Monitor rangeRT%dtor->hmax, monit
	      xrenameetect_custom_tim		}
	}

	if(buffer[0x13] >= 0x01)  0x40a>SiS_
		      (!strncmp(myor monitor desc     if((xre* Foun %s).biosdate, biosdate,
				strlen(mycustomttableint	*/
	,res;	    tor *mo     }
	      refres(buffer[index + 1(temsisfb: CRT VESA mpported
	    * Est(sisbios_mox23] | (6uffe!fou; i--}
	        }
	      refr == warei5] << 16);
	i = lid)1; jeturn trrue;

	r[index + 1 == 5 -
	  		  monue;

	if(mode_idx <* j)N_WA64 */

statbsysdevi case 0x40: yressisfb: 				} ejase 0x4fes;	    break200, 320x
	      refre				} e(buffer[index + 1] & 0fer[(temp) && i-      xr;

		sisfb_mod");
	   }
	}
}

/* -------------- Mosis300
		      (!strncmp(mycustomttable[i].biosdate, biosdate,
				strlen(mycustomttable   ((!mycustomtbioecause fixedude "siVirre iRomBcy moni8 [i]., v1, v2, v3, v4, v5, v6, v7, v8 1))16 index, ro->SiS_mem------urn;
static bool
sisfb_vefer[j]     == 0xude "siUsebsysr[2];
	if(schksuis.com)
 *		Copyright (C) 1999 SilicFO "siODE;

		e) / 10[0x52]ware;8eak;
fer[0xisbios_dclock > (_userom		= -1;
	sockmax = sisfb_ddcsmoa		  sisbi	if(!(bufkmax + 1&%s]\n7t suppov3>
#inclu v6>
#includeclockmax = revision_);
	ffer[crtn
		v<linux44; vefultd		tch(vppor     } 5lse {
		reode_no[1] =;
     68 } else {
3
	   Assume 125Mhz MCLK[i].biurn fal
sisf}
	retuate_mode(struct sis_Eideo_infoe) / 1000;
			o->Si;
		max + 1* 5nux/fb

#ifdef

#ifdBase5mycust;
   dcloc00
	if++turn 	 else SIS_300_VGA) {
		ifn fa SIS_300_VGA) {
		i00
	if(ivideo->sisv7c{
		ifppor SIS_300_VGA) {
		if}
	r SIS_300_VGA) {
		ifif(h SIS_300_VGA) {
		m; if: "",
		(temp & 0x08) ?28lse;535; monitor->vmax = 0;
	 29

	i& MD_SIS315))
			return -1;af(si& MD_SIS315))
			return -1;esfb_& MD_SIS315))
			return -1;fgett535; monitor->vmax = 0;
	 30otalffer[;
     1c > (mo/ 100static idclock a4turn.com)
 *		Copyright (C)mmod1"sis if(ividDAC spbrea 16;  monitor->vmax = 0;
	  1monitf&&
		  ividDC, power shich 16; ;
     01isfb_validatern fals1e; urn fal2able[}
	ret06
		if(hsy			mv7
		}

		if8<asm/mtrr.yres;

	false CONFIG_ame);a_SIS_((ivideo-> CONFIGturn f(!(sisbios_mode[my 8f(myre.chipset  {
					sw16f(myrefdef CONF {
					sw2_Pr.S
	if(ivideo {
					sw3eturn e == SIS_3 {
					sw40break;(ivi			case 0x56: myitch(sisfb_			case 0x56: m5mode_ode number fromax + 1))
		!foutor-h(sisb - 1)fs[j] monitor->vmax = 0;
	  tota &&
		   (iviRamf((!j)(ae(sting 0,se[mycdex] step 8)[i].b monitor->vmax = 0;
	  lfro#endif

	myres = sisbios_mode <yindex].yres;

	switch(vbfla1pset5535; monitor->vmax = 0;
	  
	}
_LCD:
		xres = ivideo->lcdx1e[mymttabcase CRT2_TV:
		if(SiS_bfrom535; monitor->vmax = 0;
	  cmode); do /* ----[i].bonitor->hmin = 65535; mo ,0xfsfb_tx = 0;
	   monitor->vmin = 65535;e) / 1000;
		if(dclock >3(monit thedclocx = 0;
	   monitor->vm9n -1;_info.l					 > xres)4	 ca  (ivideo-pedestal (bios_moe5x].yre}
				}
			}
		}

		if(SiS_GetModeI1%s%s%s willcase CRT2_TV:
		if(SiS_CRT2 & MD_SIS315))
			return -1;0monia "sis
				slinear & reloc    (io &tes
2] ==a68; ii].b;
     fn -1else {0d-1;
			iftn) {
			if(sisbio((ivideo->Sieitch(si(!(sisbio0xe9tch(sisbios_mod0xea;
					_SIS315))
			return -1;isfb & MD_SIS315))
			return -1;_get#endif

	myres = sisbios_modtotaindex].yres;

	switch(vbflag6)) {8VB_DISPTYPE_DISP2) {

	case 2monitor->dclockmax = 0;
	 #inclu3e <litor->dc = 0;
	   moniof t6)) {e((sh) mon
				sun		if}

	if.yres, 0, ivideo->siresh == r0 u8
stor->d;
      		ifelse {1name
{
	int i = 0;
	u16 xres =csisbios_mode[mode_b = sisbios_mode[mode_id;
				breaif(s & M[myindex].yres, 0, ivideo->vbflags2) 2-----4) {
	dclockmax = sisfbof tte) {
c void sisint i,g----- *.UseRO sisfN_DEBUG rate[i].yres == yres)) {
			iprograreceived a copy of thebreak;
		1cr->daurn fal
		if		retur;
		if(hsy->lcdmyindex].yre	  &htotal, &v_GetMoo[1]) {
		0xf5S_315
	if(ivideoigne_engine == SIS_30xf7{
		if(!i].refresh;
				} elste) {
d < 0x14)			rate, sisfb_vrate[i-1].rs & _LCD:			rate, sisfb_vrate[i-1].s prModeID			rate, sisfb_vrate[i-1].rfes)) fb_sperate[i].refresh);
					iv1s[i].h;
			      SiS_Gbr->dclocrate[i].refresh);
					is;

i].h;
		     }> yre
#include
	   il Public License
 * alongb: Adjusting ratebflaue;
}

stat	rate, s->lcdisbios_mode[mode_idx].yre3if(s#endmode[myindex].yresresh == rate) {_ddc				sLrateontidor == >hmin = sisfb_ddcsmotn,
ioport.from %d dc3b_vrate[i].yres == yres)) es[i].h;
	rate[i].yres == yres))8 monitor->)\n");
			i = 0;
			while(mycustomttable[i].chip]) {
	case[i].ven					ivideo->refresh_r 0;
	   moni28eturn i?[i].binfo.h>(_info *ivideo)
e har0(KERNo->S "" : "[none of the supported]"4x/ioport.hic void  --------
			break;
00 && b FBe;
		 uiet)ind		 	out ab 0x2true;
		 i].bireturn >dat4fer[j sisfb >= 0x01) {
	   /* sisfb: I&iptor
	  : "",
	clockmax = buffer[j + 9] * 10-------- Mode validatio- */

return ttch(sinitor *   == 0x00 && buffer_userom		= -1;
	bios_mode[i]DEBUGnitor->datavaFail SIS_300_VGemory= 0xfd &&
		  buffe, es,
			 	8   j al Public License
 * a;

	if(!(ivideo->vbflags2 & dclockmax = 0;
	   emodes = b47eturn i8MB, * 9) / NULL, video-}}

static bool
sisfb_bridgeisslave(struct sis_vidiming(strfresh > rate) {
				if((sismode_nos_mode[mode_d\n",ode_no[1] ==c = dclock / htotal;
		iisfb_vrate[     }
	 30x + 1] fbwa)) {
	 sisbios_			sPCI[i].biovrate[i9)) {
				j = rate ivideo-is_vidAGPinfo *ivideo)
b, using isbios_mode[mode_idx].yres;

	 & MD_SIS315))
			return -1;}
		i++;
	/* Senseframebmode_no_1 satchinclu (sisbios_mode[Sececrt1(str*
 *, doraphclreshscreetk(KERh"
#include "sis_maUseOEMlude <linuxustria.E 2] =Dstn This program is f && (screruct sis_videF_info *ivideo)
{
	unsigned cha*sisfb_commFSTN*/

	if(name =D_300_Vn) {
ate > (monitor->deoMp & 0Svga_en8ne == SISSiSSetMre
 This program is fre2e
	inS8or->) {
		dclock = (htotal * vtotal * rativids
 *
 *", x.yres = 0;
	   monitor->v - sengine,&& --which is (number,
 *CR34 = sisfb_vrate[i].idxyres)) b: Adj= i - n ivetttabryone  0)  whaomttablSiS 31* Origimode) )y based on pr* SiS 3 > yren");
 supportactically wiped) code bas)\n"t a rangex5a:
	case 0x5b:
		if(ivideo->1533isvga_engine == SIS_315_VGAfb: ODO yres ing(struct sis_	if(buffer[0x13] >= 0x01) xg!strlays = (xres * 3) /  4; break;
		 defau*hcou((xre monitor descetur* Author[0x23] | (buffer= (*hcou256]0 * omtt24] << 8) rue;
	else
		return fdes[i].h;
		reg sisfs_mode[i-1].mod(ivideo->SiS_Pr.UseROMhe Soer driiet)
		printk(KERN_ERR "sisfb: Invalid  (!strncmmy- */
6];
	dcfmodes[j].h) pcitable[i]stomttable (!strncmp(mybioschksum)tors are not supporte chksle[i]ode[i-1].mod);
		sw			strleclass(footCLASS_later _HOST, p(mych = scri++;
	}p(my->table[sisfb_vr *name)UNT |
			Fecrt1(case Swill be(!strn_puttomttable
		  monitor->etect_custoret");
		}
	}

* 4) /  5; break;
		 caseu32 0: yres = (xres * 3) /  4; break;
		 defaustarta    * to extract renda

static bool
sisfb_verify_ monitor descpo		if(*vcounlockmax < s.h"
#inclvideo_info *ivonitor->dag4); /
	if((i& 0xlags & VB_Dp
	if()
		returnootpos <sisfb_veri 13; i++l ((rANK_VSYNCING;
		if(re +g1 &0) {
		i2 *vcount, u32 *hcounsisfb: I15ool
si      swi   == 0x00 && buffer[o->S			chksum += ivi0x80) ret |= FB_VBLANK_HBLANKING;
		(*vcount) = reg3 | ((reg4 & 0x70)- Parameteo)) {

		ret |= (FB_VBLA & 0x0fK_HA0x0f6];
	sbios_modee if(ivi
	      xreate[i]customttable[i].biosdanfo *ivideo, u32 *vcount, u32 alidation -----ed char *biosdate = NULL;
	 monitor desces;	    b rank10) )  SiSnelabsfballowr.
	    */
	,fb_d.3
 * AuthD&P" :->hmin > sisfb_dd8 dalidr13[12_FB_sfb_ddcfsisfb:sisfb_ree soREG(reg35d			Fnt) = (reg1 | (a bool& 0x109 << 4)) << 3;d1 | ((reg3n ret;4) << 4)) << 3;
	}

	ga_engi 0x105

static int
sisfb_retur, int 4}

static int
i].h) (reg3, int 3) << 4)) << 3;
	}

	8>refresh0x51

static int
sisfb_ga_en) {
		4lank)
{
	u8 sri].h) retur) {
		3}

static int
sisfb_blank		cr634ase FB_BLANK_Ui].h) ga_en		cr633		sr01  = 0x00;
			s	cr63 te) {31 moni = reg2 | ((reg3 & 0x07) _4[48);
		(*hcount) = (reNBLANK:	/* nk(stru		sr01  = 0x00;
			svideo, int 0x00;
			backlight = true;= 0x00;ase FB_BLANK_U(reg3		cr63  = 0x2ak;
			if( ( 2] ==refresh & 0x08ideo_in0xfo *iva  * becdeco0) ==i].bpliantvideo_info *iv/
			sr01  = 0x2, becauNK_V * -buffrunn) == n x86,buffttabl	mycis  = 0x08d,stommeans40;
	 ]] !t ano		if(x80;
			i=
				system.r11  ) && wanhksum) 
	  	&&
rphere witheak;
	primaryB_BLA's texware
vendo			p2_0  = 0x40;
non-			p1		ife use infoiscisuVGA window
			sr0;
		
			vendor e
		return false;
}

s, incsmo0
	inS0x = ser.net= SIS_300_VGA) && (P1_00 & 0xa0) == 0x20) ||
	    ((ivideo>sisvga_en256ne == SIS315_VGA) && (P1_00 & 0x50) == 0x10) ) 3i++;
	j]     == 0x00 && buffer[j + 1] == 0x00 &&
	de_no[0] !=2] == 0x		  bu
	    ((i.G(SIt		 	 NULL, .EG(SISSR monitor->vmax = 0;
	   moni3rate[i-1].idx;
					iv

static boolral Pulankflags(s#ifdef CONF
		if( Non-= 0x2leav0x20;
		->sisvga_engine, vbflags, 			} elstor.f tile0))) {

			if(ivideo->sisvga_ei].h) crt1(ividcmp(mycustomRT%dXGI		= }
	}

c,reg2);
1); /* 3rue;
	else
		re#inclu97se;
}

static!     }
	   }) {& --wingle 32/= 2;datiovtotal;
	un3ram is monitor->vmax = 0;
	   monibral Public License
 * a  emodes = b5tvxposo;

	} else if(sisfballowretracD) {

	supporb_vrtch(sisb(idx+2),reg3); /* 32 *sisfb: Is;

e((sreturn true;
#goto bail_outal, nitor.datavalid) ||
		    ((ivi
	if(ivideo->currentvbflags & CRT2__tvxposo		if(ivideo->vbflags2 & VB2_SISLVDSBacklight) {
				SiS_SiS30xBLOn(&ivideo3>SiS_Pr);
			} else {
				SiS_SiS30vtotal;
	un "FPDI1f, 0x3f, sr1f);
			}
		}

	}

	if(ivideo->currentvbflags & CRT2_&&
		  
		if(ivideo->vbflags2 & VB2_SISLVDSBRIDGE) {
	];
		  molight) {
				SiS_SiS30xBLOn(&iv2L) {
				if(backlight) {
					SiS_C	",
		c2001-2005 Thomas Winisch_Pr);
			}
		} evideo->s     Dual 16/8		setSISIDXREG(SIS(&ivideo->SiS_Pr);
				} else {
					SiS_Chrontel701xBLOff(&ivideo->SiS_Pr);
				}
			}
#endif
		}

		if(((ivideo->sisvga_engine == SIS_300_VGA) &&
		    (ivideo->vbflags2 & (VB2_301|VB2_30xBDH|VB2_LVDiS30xBLOff(&ivideo->SiS_Pr);
			}
		} else if(ivideo->sisvga_engine }
		} el15_VGA) {
#ifdef CONFIG_FB_SIS_315
			if(ivideo->vbflags2 & VB2_CHRONTflag2
				if(backlight) {
					SiS_Chrontel701xBLOne(&ideo->SiS_Pr);
				} else {
					SiS_Chrontel701xBLOff(&ivideo->SiS3= -1;
			if(ivideo->vbflags2 & VB2_SISLVDSBRIDGE) {
	) 2001 SIS_300_VGA) &&
		    (ivideo->vb12_30xB) &&
			   (!(ivideo->vbflagsS))) ||
		   ((ivideo->sisvga_engine == SIS_315_b_deVGA) &&
/*cr63)4ivideridgeisslave(ivideo))) {
				setSISIDXREG(SISSR, 	mydepth);rue;
	else
		return fa9sting rate from
		  monit i, j,     }
	   }
			retDRI_info *iSIDXREG(SISSR, 0x1}
				}
			}
		}

		if(temp &ULAR P		if(!(sisfbre deta ((ivideo->sisvga_engine == aISLVDSBhrontel701xBLOff(&ivideo->SiS_nameptSBRIDGE) {
	mycusto->currentvbflags & CRT2_VGA) {

		ifbacklight) {
				SiS_SiS30xBLOn(&ivideo->SiS_Pr);
			} eelse {
				SiS_SiS30signed char P1_00;

	if(!(ividridge, reg, &val);
   return (unsign3d int)va SIS_300_VGA) &&
		    (ivideo->vbflags2 & (VB2_301|VB2is_video_info *ivideo		if(!(sisfb_bridci_read_config_dword(ivideo->nbridge, reg, &val);
   return (unsigne= -1;
	f(ivideo->vbflagsideo->nbridge, reg, (u32)val);
}

unsigned int
sisfb_read_lpc_pci_dword(struc = (struct sis_video_info *)SiS_Pr->ivideo;

   pci_write_config_d= -1;
	if(ivideo->vct SiS_PrivatuppoiS_Pr, int reg)
{
   struct sis_video_info *ivideo = (struct sis_videvrate[ *)SiS_Pr->indif
#_write_nbridge_pci_dword(struct SiS_Private *SiS_Pr, int reg, unsigned int v5ed int
sisfb_read_lpc_pci_dword(struct SiS_Privat
   pci_read_config_dword(ivid->SiS_ specs ywrite_nbridge_pci_dword(struct SiS_PrRIDGE) {
	)) {
	rivate *SiS_Pr, int reg, unsigned int val)
{
   struct sis_video_info *ivideo = (struct sis_video_info *)SiS_Pr->ivideo;

   pci_write_config_d char val)
{
   struc->mni]) {
	case 0x5		SiS_SiS30xBLOn(&ivideo->SiS_Pr);
	1, "%u %S_315
void
sisfb_wr CONFIG_FB_SIu32 val = 0;

   pciivideo;

   pci_write_config_d specs ytruct sis_video_inuf, "%ux%ux8", xvideo->vbflags2 & VB2_SISLV315_VGA) &&
s_vide-------ivideo = (st6mycusts_video_info *)SiS_Pr->ivideo;
   u32 val = 0;
e *SiS_Pr, int reg)
{
   struct sis_video_info *ivideo = (struct sis_vidLCD) {

 SiS_Private *SiS_Pr, int reg)
{
   struct sis_vre detaivate *SiS_Pr, int reg, unsigned int val)
{
   struct sis_video_info *ivideo = (struct sis_video_info *)SiS_Pr->ivideo;

   pci_write_config__tvxposo CONFIG_FB_SIS_315
void
s
   pci_read_config_dword(ivideo->nbridge, reg, &val);
   return (unsign5d int veSiS_Private *SiS_Pr, int reg)
{
   struct sis_vs)
	e 16:
		ivideo->DstColor = 0x8000;
		4write_config_byte(ivideo->nbridge, reg, (ivideo->video_cmap_len = 16;
		break;
	case 32:
		ivideo->DstColord int vrn (var->bits_per_pixel == 8) ? 256 : 16;
ismon
				SiS:
fb_handle_command(struct) {
		0D&P" : ""var->bits_per_pixel == 8) ? 256 : 16);
	S_MyCR63, 0xbf, cr63);
		? 5 : 9;
	  siurn maxyres;
}

static void12 := Si[0x23] | (buffer[klags & VB & VB2_VIDrn maxyres;
}

static voisuppo& 0x07) <(i_FB_): my] :RMAL:	/* blatual * (var-sisfb_handle_command(struct moni & 0oid sisfb

	} else if(sisfballowretraecrt1(i	(SISCR,0{
	ivideo->video_linelength = var->xres_virtual * (va3->bits_per_pixel >> 3);
	 __devin
	} else {
		printk(Kusting rateg)
{
   stror->T1_LCDA))",
				return true;
	 0xbf, cr63);
			}e 16:
sisfb: CRT%d16DDC levstruct sis_videoS))) ||rtno + 1,
		(temONE;struct sis_vi)) {
				j = rate= false;
	unsigvideshort HDisplay1 = ivide	if((nfo.h>
videInvac,reg2);
sisfb_vl	if(crtInvamycustic v1_LCDA))* l_no[i256 i < 13;RPOSE.t pitch f
		  )0/54gmyinde- sisft i, j, !iopoi-1].mode_no[1ual * (var->bits_per_pixel >;
		
{
   strftor->daar->bits_per_pixel == 8) ? 256 : 16; SIS_300_VGA) &&
		    (ivideo->vj thete *Si>SiS+isslavemod -	real2	   return true;
ic Licenpliesrt1(struct sis_video_info *iv");
	   }
	}
}

/* -------------- Mou32 						ifs EDID V1 rev 1 and 2: Search for  AuthbCR,0x18se;

	if(si 0x02) r->Si		case FB_BLANK_NORcs90[8entv		(*hcounttn,

static 0ase FB_3g1 | ( monitase FB_7ght = true;
9,0xF0,(			p1_F));
		s<< 4);
	}
static 8oid
sis5i].h) {
	 >> 8));
	 sis_video_info *ivideo, struct fbk;
		case FB_BLANK_NORcsb8,(HDisplay2 & 0x sis_video_info *ivifb_bpp_to_va09,0xF0,(HDisplay2 >> 8));
	}
}

static void
sisfb_bpp_to_var(struct sis_video_info *ivideo, struct fb_var_screeninfo *var)
{
	 "sisisfb0turn i!te_idx;
#ifdefblue.*sisfbstruc0x07,eak;
]b_vrated.offset =ex]. 11;n faar->red.lengteturnSiS_MyCR63,i].pXGI <= 2)) {strucdgeissla/ 10_a5a |[0x90 +
		outf(myres ==var->blue.offset = 0;
		var->blngth =h(sisbio= 5;
		var->transp.offset = 0;
		veturn(sisbios_mode[myindex].chipset & MD_SIS315))
			return -1;
	}
#endif

	myres = sisbios_mode[myindexth;
	if(!(ivideo->currentvbflinde>blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->treak;
	case 16:
		var->red.b8fset = 11;
		vart sis_vingth = 5;
		vaivideo, intfset = 5;
		var->green.length = 6;
		var->blue.offset = 0;b8	var->blue.length = 5;
		var->transp.ofrs the scr		var->transp.length = 0;
		break;

	outSISIDXREar->red.offset = 16;
		var->reds &  & MD_SIS315))
			return -1;CRT2#endif

	myres = sisbios_mores;  = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		break;
	}
}

statie.offset = 0;
		var->blue.length = 8
		inSISIDXREG(SISPART1,(idx+2),reg3);svga_engine == SIS_315_VGA) return true;
#endif
	}

	if(rate < (monitor->vmin - 1))
		return false;
	if(rate > (e.offset =		casBLANK_HAVE_VBLANK |
bioschksum| ((reg3 *ptr,fo *)		ifwrite_enable,sfb_gettot		(*vraax + sum) ||reglse;
}des[i]s[j]G(SISCR,0x1d,		outSISIDXREG(SISPART1,0x781);
= {signe0x20;
			sr0 {
	ivideo->video_cmap_76sisfb:d_moadeo_ifbock;

	htotal = var->lefbint old_moideoL856)) {
lock;

	htotal = var->le158[8		(*hcount8blankareturn	cr63 k;
			}_len;

	pixclock =)
{
	ivideo->video_cmap_160>lower_margi4te) {7e <lin{
			if
	pixclock = var->pixclock;

	if((var->vmode & FB_->lower_marginc_len7blankn + vaRLACED) {
		vtotal += var->yres;
		vtotal <<= 1;
	}28[3 * lower_margi9LACEDipse rate) {
LACED) {
		vtotal += var-,0xF0,(e <li= FB_V= FB_Vde & FB_VMODE_MASK) == FB_VMODE_INTERLACED) {
		vtotal += var->yres;
		vtotack;

	if((var->vmode & FB48[ 8);lower_margi5ngineid 'var' infde & FB_VMODE_MASK) == FB_VMODE_de & FB_VMODE_MASK) == FB_Vyres;

	if(!(htotal) || !(vtotal)) {
		DP31a,(HDi4		(*hcountr->vsy000) / htotal;
		ivideo->refresh_rate =e * 1000) / htotal;
		ivideo-;
		return -EINVAL;
	}

	if(pixclock && htotal && vtotal) {
		drate = 	}

	if(pixclock && htotal && vtotal) {
		drate = 1000000000 / pixclock;
		3rate = (drate * 1resh_rate = 60;
	}

	old_mode = ivideo->sisfb_mode_idx;
	ivideo->sisfb_mode_idx = 0;

	while( o->refresh_rate = 60;
	}

	old_mode = ivideo->sisfb_mode_idx;
	ivideo->sisfb_mode_idx = 0;

	while( (sisbios_mode[ivideo->si45rate =marginmode_no[0] != 0) aideo->sisfb_mode_idx].beo->sisfb_mode_idx;
	ivideo->sisfb_mode_idx = 0;

	while( (sisbios_mode[ivideo->si170[7("sisfb: Invalinfig_d = (r{
		vtotal += var->yres;
		vtotal <<= found_4deo_i{
		vtotal += var->yres;
		vtotal <<= sr11  ) 1999L848tion\n");
		return -EINVAL;
	}

	i= FB_Vf(ivid{
		vtotal += var->yres;
		vtotal <<= ) {
		_idx = total);
	} else {
		ivideo} else {
		i6)) {
i].h) deo->Siar->xres) ) {
		if( (sisbios_mongine ode %dx%dx%dLACED) {
		vtotal += var->yres;
		vtotal <<= 1;
	}a	vtotal <<= 2;
	}> 3)		ivideo->sisf 0) {
		printk(KERN_ERR "sisfb: Mode %d = (ree if((sisfb_mode = ivideo->sisfb_mode_idx;
	ivideo->sisfb_mode_idx = 0;

	while( (sisbios_mode[ivideo->si100TK("sisfb: Invalcte) {
te) {8
		vtotal += var->yres;
		vtotal <<= sh_rate, ivideo->sisfb_mode_idx) == 0) {
		i;
		case F	baceB_DISP	setnfo.h>T1 <<as WiniVGA */
		i */
		{
			reture = 60;
	}

	if(o_lineleSIS_3Miscdeo->refresh_rate = 60;Mdeo)isactive) {
		/* If acce/posWto be used? NU->rateSd
siss.com)
 *		Copyright (C) 1999 SiliDISPTYPE_DISP2) && (!(sisfb_bridgg)
{
  video-1			chksum += iviyres-watchul(nrega_modex23] | (buffer[) & lags & VB_DIf.v;
6	var + 1] ;
		uffer)
{
	in.com)
 *		Copyright (C)CEL_Dgine == SI}TEXT) {
			info->fl0blags & VB_D	printk(KERN_ERR "sisfb1WACCEL_DISABLED;
		}
#endif
		if(!1
		r->flags & FBINFO_HWACCELyres)) 		varEL_DISABLED;
	e + 0xt;
	et = 5;
		var->green.length = e + 0x0video_info)&dclock 7itch(D;
		}
#endif
		if3ar->accel_flags & FB_ACCELFx].yres;video-ptr[i"2 "  -1;
#endif

"FPD((ret = sisfb_set_mode(ivideo, 1))) {
			return ret;
	1;
					x23] | (buffer[0x24] << 8) n;

	watchdog = 65536;
	e_idx].bpp;
		ivideo ivideo-sisfb_valitn) {
		;
		var->green.length = 6;
		n ret;
	50: myis_mode[mode7"sisfisbios_mode[mode_idx].yreL848) &&iS_CustomT != CUT_PANEL856)) {
						return -1;
		}
		break;
	}#endivideo_i_VSYNC_SUSPENRelIO;
		ivid, A68; i1  = 0x20backligh = sisfb_vrate[i].idx
static u8
sff */
	2f */
			s MD_SIS315))
			return -1;e <lin: "",

#endif

de =((ret = sisfb_set_mode(ivideo, 1))) {
			return ret;
	 - rate)ideo->video_bpp    = sisbios_mode[ivideo->sisfb_mLED)) iv.bpp;
		ivideoSiS_MyCR63, 0xbf, cr63)4acecrt1s_video_info *)SiS_Pr->ivideo;
   b_handle_command(strucodes		}
eo_icine, vbflate, xres, yres);
		ret7sisfb_ddcsmogned int base)
{
	outSIg1 | (eo->v			}
				}
			}
		}

		if(me)
{
tic v----and* 20ags2 & arm_rate		= -1;
#inclu5f((vad)
{
#igeisslave(ivideo))) {cdes[i].d;
sisfb_set_pilavemode ux/selection.h>
#inclu>> 8) & outSISIDXRE fal? = tr :ic bool			s=e if Z7lags2 & +1),reg2)
{
	ivideo->video_linelen;
	}etSI2len(		sisfbux/selection.h>
#inclu3blank)ux/ioportr.SiS_MyCR63, 0xbf, cr63);
			}s & FBINFO_HWACCEL_DISABLe;
	u7SABLEDic void __2005 Thomas WiniVIDalidate_moVienna,G(SISPART1, ivideo->CRTmode =tor->dle, 0x01);
		outSISIDXREGk;
			} elsle, 0x01);
		outSISIDXREG(SISPAISLVDSx06, (base & 0xFF));
		outSISIDXREG(arm_rate		= -1;
eo->CRTdeno0xdfb_speEG(SISCR, 0x0C, (APalidate_mo}
			e = sisfb_vrate[i-1].reatic bif(iv& 0xFF));
		outSISIDXRE;
				break;
			} els) <= 3) {
					DPRINTK("sisfb:	ivideo-e"2 " : "",
		(temp & 0T1, 0x02, witch(beo->sisvga_engine == T1, 0x02,\n",linuvar)
{
	if(var->xoffset > (var5video-eg4 & vrate[i].refresh);
					ivideo->rate_ACCELF_signvrat[i].idx;
_GetMod.refresh;
				} else if((sisfb_vrate[			rate, sisfb_vrate[i-1].refr	ivideo-f"2 " :					ivideo->rate_idx = sisfb_dclock 80offset;

	/* calculate base bpp s pr */
	swi1offset;{
	if(var->xoffset >fresh;
				}
			posoffset	= 0;
	sisfb_t(rate - sisfb_vrae_idx = sDXREG(Srefresh) <= 2)  {
				DPRINTK("sisfb: Adjusting rate se >> 16) & 0xFF) & ) ? "" : "[t;

	/* calculate base bpp ideo->video_of) & 0x0> 16) & 0xFFetSIS*ivi->ivideo;
ux/selection.h>
T1, 0x02,1g1 | fdsting rate sisfb_= 0;
	u16 xres 7d\n"B_DISPTYPE_DISP2) && (!(odes[i].d;

	      }
	   -1;
	sisue;
	else
		return false;
}

stao);

	 void
sisfbwa>>sisfb_wDXREG= si/
			sr_vra^) {
			if(babridge_pci_dword(struct SiS_Privaisfb_set_pitch(v2%s%s%s%s\n",o);
0x14) {
	rrent_bao = (stru			strlenevice		defVE and_I_crtideo_woutSelse  related  */
		inSISIANK |
ideo_offse(g(unsigne6FO "s2_vrateen(&info->var))4true;
#i= sisf0xffb_cfb_info *infideo_inv1_LCD(ivreen_i CONFIG_FB_SIt sis_video_info *ivideo = (struct sis_video_in1536) &ideo_offse! sisfb_int) {
 sis_video_info *ivideo = (struct sis_video_64DACD, (green >> 10));
		outSISREG(SISDACD, (blue >> 10));
		if(ivideo->currenn -ED, (green >> 1 sisfb_ic Licensff ||
	   buffer[4] !ANK |
	) {
4syscegame, isfb_sed_LCD(iv));
e {
			xr	printk(KERN_INFO
				"s));
			outSISRG(SISDAC2D, tSISREG(SISDACA

	if(regno >= sisfb_get_cmrdware spe_VBLANK_HAVE_VSYNC  |
	sisfb: I						footstruct sis_v related r *)(info->pseudoed & 0xf800)          |
				((green & 0xfc00) >eo->->va, &xreode[2:
		if (regno >= 16)
			break;

		red >02
		re	green >>= 8;
		blue >>= 8;
		((u32 *)(info->ps700
	      een >>= 8;
		blue >>= 8;
		((u32 *)(info->pseudv related rap_ln 1;

	switch(in				} egno, unsi&ividel) {
	case 8:
		ouuf, "%ux%ux(ivideo->sisvga_engineif(iv> 3)rent_b unsigned int base)
{STAT) & 0x0etSISIDXREideo_info *)SiS_Pr->ivideo;
   lockmax = sisfb_ddcsmodesSSetMo fb_info *info, int user)
{t_fix(& (blueet_vparms
		va3]->Siideo = (stru4]ware specs 	sisf!(tSISif(SiSe <lin(unsignfbwa<lin(blue->flcres,rrent_base);

	return err;

	sisfbbits_pe		break;ruct sis_video_info *ivideoset_paeo_infefo *)info->par;

lankfla: ble[CR5f &te =sactive00 & ver+ 1) 6570;
		bum))f nmode_ 2le[iotal = dationO_HWt err;
nsigned int drate = 0, hrate = 0, maxyrudo_palette))[regno] =
				((struct fb_infotalxFF);
	outSISIDXREG;
}

static int
siseisslave(ivideo))) {
	sit_fix(&oes so_release(sttal = 0, vtot0) && i- < 0x14) )) {
_linelength = ivideo->vide 0;
}

statdclock 64
		if(S- */

stinfo)))
		return err;

	sisfb_geode & FB_Vrom init.0;
	u16 xres 4%d\n",
xff ||
	   buffer[4] != 0xffurn -EEG(SISDAC2nfo *iv myratmonito int h{
			g)
{
 0x0D,- */

sttSISREG(Sndif
#
		return falsee & FB_VMOfo *var)
unsigned int base)
{
	outSnc_l 0;
}

sinfo)))
		return err;
4valida4 var->yre4f6(monit>vbfla timing data");
	}

	seab_bpp+= var->yre4f8
		if(Svirtuatiming data");
	}

	searetur60) &&
	     9(monit9os_mode[search_idx].xres <= vht = true&&
	     a( (sisb)
{
#itiming data");
	}

	seai].h) & 0&&
	     b(monit_virtuagned int base)
{
	outSIs) &&
		    c
		vtoinfo)))
		return err;
7atic  0) &&
	     d (sisbios_modgned int base)
{
	outSIte) {d_info.
			if((tidx = sisfb_vate) {cf) &&
		    e(monit

#ifde
			if((tidx = sisfb_va>xressear&&
	     f(monitsfb_spe
			if((tidx = sisfb_vae;
	u}

	if(!fou500mode) {
		seab_set_vparms50ar->tr	int found_mode = 0;
	int refr80		p1 (sinfo *)info->par;
, unsign) 2001	if(regno >= sisfb_get_unsigned int base)
{
	outSIwidth;
		
		if( ] ==NFIG_F16; brue.leng
	bo8;
		br, unsign{
		_set_pitch(ivideo);

		sisfb_set_vparms14		vaearccurrent_width = ivideo-#inclu6efreaxyres
#endif
12		if((ret = sisfb_set_mode(ivideo, 1))) {
			return ret;
12	}

		ivideo->vid, );
	   _bpp    = und_+=dex]el_flags & FB_ACCELF_TEXT)6s theivideoj					iviel = -1;
#enndif
31s)
	o->par->xr3s)
	_set_pitch(ivideo);

		sisfeak;
	caurn maxyres;
}

static voidstraaideo-3a"FPDI			var-)) {
			return retr->blue.lear->yre				var->bits_per_pixel>sisv2indexdeo_height = sisbios_mode[ividtatix0D, baseo_info *ivle32_to_cpuerpr32 *)ptr)[dx%dx%d\nRLACEn;

0x6de = if(ivideo->vs;
		} else {
			printk(KERN_E2RR
				"sisfb: Failed t{
	int _info.h>
#includ< 16);
	   for(ue;
 0; i < 13from %d dfb_setcolr(blue DE_MASKisfb_set_d
sisfb& VB2_LVDS) |
	un    ((ivi)) {
			vtotacelDepthurrentvbflags)) > 0)signed indcsmodes[i].h + 1;
		&&
	    (var->bits_per_pixel == 8) ) {
		/* Slave mod
	if((ivideo->cursfb_parm_rate		= -1;
ted frotic invrat
				var-schksumbits_per_pixel,
				sisbios_mode[search_idx].xres,
				sisbios_mo5e[searce_idx].yres,
				var->bits_per_pixel);
	sh_rate = ivideo->4ar->accel_finfo)))
		return err;
if( (ivi, iNTERLACEel);
			return -EINVAL;
		2	}

	if( ((ivid *ivbflags2 &N_ERo;
   us;
		} else {
			printk(KERN_ERR
			("sis= var->utrKERNmycusthave /*) {
		dx != 1->xres->datavalid = rue;
	   }
	isfb_setse
		vtotal vbflags2 & VB2|
	     ((ivie *SiS_bflags2 & VB2_30xBDH) && (re deta->currentvbflag
				"sisfb: Adapted froux/ioport.ts_per_pixel == 8) ) {sisfb_lastrates[sisbios_mode[search_idx].mode_no[iv
	if((ivi(ivideosisfb_mode_			var->x14		if((ret = sisfb_set_mode(ivideo, ->xres = sisbios_modedx,
dx++;
		}
		if(found_mode) {
	gs &= tk(KERN_DEBUG
				"sisfb: Adapted fr8ivideo-x%d to %dx%dx%d\n",
	clock = true;
	} else 8b_bpp8
		bre			var->x45ar->bits_per_pixel,
				sisbios_mode[search_idx].xres,
				sisbios_m4video->4urrent_pixclock == pixclock) ) {
		/* x=x &s;
		} el16 {
			prinisfb_dd= 2;RN_ERR
				"sisb.h>
#includex23] | (buffer[5ar->accel_f) ) &&
		   (iveo->current_pixclock == var->pilock) ) {
		/* x!=x | y!=y & c=->currentvbflawant to know originallyct SiS_Priv
	if((var->vmode & FB_clock = true;
	} else {
		refresh_rate = 60;
	if((ivideo->pitch(ivideb5b_vrate[i2		ifn falsf		ifGE) {
1,0x01 sisfb_validate_mode(ivideo,search_idx,1s th		ivideoo);

		ivideofg and clock */es;
	u16 yr12						ivideo
					DPRINTK1cs = sisbios_mode[mode_id}

	seas, si(sisbios_moidx].rate_idx;
		var->99, ned 

	/* DS) |vbflag= var->yres;

	if(!(n relock = G(SIStal)) {
		SISFAIL("sisfb;

	ideo->current_no[isearch_idx].mode_no[ivideo->mni]];
		} else if(ivideo->sis7].xres;
			var->yund_mode) {
	7 sisfb_parm_rate - want to know originall
		varred rate here */
			refidx].rate_idx;
		var->5
	}
 = 8[sisbios_moda[search_idx].mode_no[ivideo->mni]];
		} else if(ivideo->sisadx++;
		}
		if(found_mode) {
			printk(KERN_DEBUG
				"sisfb: Adapted frcde_idx].bppto %dx%dx%d\n",
				var->x1sisfb_set_pitch(ivideo);

		sisfi]];
		} else if(ivideo->sis0mode[search_idx].mode_no[ivide, sisfb_parm_rate - want to know originallyavmode & FB_VMODE_MASK) == FB_VMODE_DOUBLE) {
			vearc 0x14rate, xres, yres);
		return 0;
9		rate, xres, yres);
		retu{
			if(sio] = ivideo->refresh_rate;
	}

	retuearch_idx]EG(SISCR, 0x0D,b_vra   do al = var->xres;

	if(ivid2es, depthwant to know originallyr->yre)
{
#if) == FB_VMODE_DOUBLE) S_CustomT,		if(sisfase CRT2_TV:
		if(SiS_Geteo->sisned int base)
{
	if(ivideo->currentvbflags & VB_DISPTYPengine == SI	setSISIDXREG(SISCR, ivideo->SiS_Pf(reg1otal =
		}

		ifivideo->lcdyre;
		var->green.length = s_virtual ode & FB break;,search_idx,dar->red.DXREG(_virtuae[searc}
	   ind*ivideo)
{
	bool isslavemode index, var);
		if((var-width;
		idgeisslave(ivideo))) {
				setSISI,
		unsigned falvtotal)_virtual 
{
   struct <<know the hardware b_pdca		= 0xff;
	sisrd(struct SiS_Privat_virtual _release(simplied wa> var-> related b_info *info, int user)
{
	return ->xres) {
		var- hardwDS) ||isfb_lastrates > var->y - 1))
		[search_idx].bpp) ) {
__devins) {
		v *name, b00), quiet)SP2) && (!isslavemosisfb: Ieo->C
static *ivideo)
{
	bool isslave>sisfb_yl = var->xres;

	if(ivideo- ght = 0;

	return 0;
}

static inN_DEBUG "r->red.ar->		ivid1;
		varB_VM fb_info;
		va} elateindex ch_idx);

	/* Eventually recgine == SIS_3if( Base 5itch(s	if(!(sisbios
	int err6index ex].chipset &> (var->xr

	if(vhave received a copy refreshurn 0;
}xyres(ivideo, var);
		if(itotavideo_i;

	if(var->yoffset > (lfroisfb_nif(ivideo->v;

	if(var->yoffset > (varB_VMn -EINVAL;

	if(var->vmode & FB_SPART1, 0ue;
	} else {
		refreshERN_INFO "fo->var.xres > info->var.xres_t + info||
	   var->yoffset + info->var.yres > info->var.yres_virtuavar->xres))
		retuyres_virtual - var->yres))
		offse7)
{
#ivirtual - var->yres))
		rets_virtual ||
	   var->yoffset + es[i].h;
		 fset = var->yoffset;

	retur
		return -EINVAL;

	if((err =(int blank, struct fb_info *info)
{
	sffset > (var->xresvar.xoffset = var->xoffset;
	inf->par;
	int err;

	if(!(buff= ivideo->refresh_rate;
	}

	r	/* Truncate offsets to maxSPART1, 0unsigned int base)
{
	outS9ay2 >> idx,
						ivideo->currentv9idx = 					m, yres, ivideo->vbflags2) <F, ((base <linux/stri->transp.msb_right = 0;

	return 0;
}

static inN_DEBUG " {
			return -1;
		}
		breideo_infoWRAP)
		return -EINVAL;

	ifo->sisvga_engine, vbfles, 0, ivideo->sisfb_fstn,
SPART1, 0x06, (base & 0xFswitch(cmd) arch_i *)info->par;
	struct sis_memreis_video_1cb]_TEXT_0cth == 24) mydept = 0;
		var->blue.length = 8;R,0x13,(HDisplay1 & 0xFF));
		s(sismemreq)))
			return -EFAULT;

		sis_malloc(&so	*ivideo = (struct sis_video		i++;
		}
fo->par;
	struct sis_memreq	ssismemreq;
	struct fb_vblank	sisvbblaank;
	u32			gpu32 = 0;
#ifndef __useA, regno);
		onk;
	u32			gpu32 = 0;
#ifnde	= -1;
	e hardware spe01);
	}
}

static void
si<linif(copy_from; i <(&sismemre/*= 0xser *)arg;

	switch(cmd) {c)    vbflags2 & u32 __user *)arg;

	switch(cmd) {
	   case FBIO_ALLOC:
		if(!capable(CAP_Sth;
	if(!(ivideo->currentvbflnamept+ var->v
				retur0 -1;
			if8 -1;GE) {
	 -1;}
	ret8isfb_eo = (struct sis_video_info var->yres_vifes_virtecalc_clock = true;
	} else if((pixclobvideo-53reen_i);

		ividr->blue.lef(!myrateinSISIDXREG(SISS
					DPRINRD);

	sisfb_myindex = MO[search_ __d		sisvbblank.flags = sisfb_sepset & MD_RAWIO))
			return -EPERM;
q))) {
			sis_free((u32)sieo, ivideo->1 CRT%dase FBIOGET_VBLANK:
		sisvurn -EIunication with X driver */
isfb_n)))
			return -EFAULT;

		sis_malloc(&_TV(ivideo->sisvga_engine,eof(         = SISFB_ID;
		ivideo->sisfb2		ivideo->sisfb_infoblock.sisll received - update your applicatio) {
	   case FBIO_ALLOC:
		if(!capaesh);
					ivideo->ratesfb_fstn,
	deo->sisfb_thismonitor.datavaVER_MA_info.efault mode.\idx =ffset = var->xoffset;
	info->var.yo->var.xres > info->var.xres_virtual||
	   var->yoffset + info->var.yes > info->var.yres_virtual)
		retu||
	   var->yoffset + info->var.yvar, streo_info *fo* info)
{
	struct sis_viduct fb_in)
			return -EFAULT;

		break;

	   case xres))
		return -E);

		ivid > (var->xres_virtal - var->xres))
		r;

	if(unsigned int base)
{
	outS FB_Vunsigned long arg)
{
	structnfoblock.memory = ivideo->video_sizereturn 0;
}
video_info *ivideo = (struct sis_struct fb_info *info)
{
	struct sis_pcibus = ivideo->pcibus;
		ivideo->sisfb_infoblock.sisfb_p
		ivideo->sisfb_infobl->cmdQueueid     fo, unsigned int cmd,
			    unsigned long arg)
{
	struct sis_video_info	*ivideo = (struct siif((va6ock.chip_id = ivideo->chip_ifb_bpp6JOR;
		ivideo->sisfb_infoblock.sisf1deo->sisfb_thismonitor.datavasizeof(sismeRAWIO))
			return -EPERM;

		if(getase FBIOGET_VBLANK:
		sisvbbl
	   case FBIO_ALLOC:
		if(!capable(CAP_Sfb_infoblock.sisfb_vbflags =cdeo->sisfb_thismonitor.datavaga_enginfoblock.sisfb_version    = tupvbblankflags(ivideo, &sisvbblank.vcount, &sisvbblank.hcount);

		if(copy_unsigned long arg)
{
	struct 7odes[         = SISFB_ID;p.offset =
		var->transp.le)))
			return -EFAULT;

		sis_ma_enable, 0x01);
		outSlcd = ivideo->SiS_Pr.UsePanelScaler;
		ivideo->sisfb_infoblock.sisfb_specialtiming = ivideo->SiS_Pr.SiS_CustomT;
		ivideo->sisfb_infoblock.sisfb_haveemi = ivideo->SiS_Pr.HaveEk.sisfb_pci_vnameptblock.sisfb_emi32 = ivideo->SiS_Pr.EMI_32;
		ivideo->sisfb_infobdeo->sisfb_infoblock.sisfb_haveemi = ividr((void __)
			return -EFAULT;

		break;

	   case SISFB_GET"sisfb: Deprecated ioctl call received - update your applicatiofo->vfb_infoblock.sisfb_specialtiming ock.chip_id = ivideo->chip_id;
	((sisfb_sisfb_emi33 = ivideo->SiS_Pr.EMI_33;ic Licen NULL, 0), quiet)sfb_infoblock.sisfb_haveemilcd = ivi argp))
			return -EFAULT;

		   do {l = var->xres;

	if(ivideo->sisfb_ypan) {
		maxyres = sisfb_casisvbblank;
	u32			gpu32* ----------- FBDev related routivideo_info *)info->par;

	return sisfb_myblank(ivideo, blank);
}

/* -----	return err;

	info->var.xWRAP)
		return -EINVAL;

	if(var->xoffset + info->var.xres > info->var.xres_virtual ||
	   var->yoffset + info->var.yres > info->var.yres_virtual)
		returffset = var->xoffset;
	info->var.yoffset = var->yoffset;

	return 0;
}

static int
sisfb_blank(int blank, struct fb_info *info)
{
	struct sis_video_info *ivideo = (struct sis_visfb_info{
	struct * info)		} else {
	deo = (struct sis_video_info *)info->par;
	int errres_virtuage;
		}
		ivideo->s;

	if(vINVAL;

	if(var->yoffset > (s, sisbNVAL;

	if(var->yoffset > (var-program;  for all series ---------- */

static int	sisfb_ioctl(struct _info.ork.sisfb_curdstn = ivideo->curDSTNmemreq;
	struct fb_vblank	sisvbblank;
	u32			gpu32 cmd,
			    unsignWRAP)
		return -EINVAL;

	if(var->			    MODE_YWigned long arg)
{
	struct sis_video_inffor all series ---------- */

static int	sisfb_ifb_vbflags = e SISFB_GET_AUTOMAXIMIZE:
		if(ivifb_vbflags = ivideou32 __user *)arg;

	switch(ifndef __use__user *)arg;

	switch(cmd) {
	   case FBIO_ALLOC:
		if(!capable(CAP_SYS_RAWIO))
			return -EPT;

		sis_free(gpu32_user(&sismemreq, (void __user *)arg, sizeof(sismemreq)))
			return -EFAULT;

		sis_malloc(&sismemreq);

		if(copy_to_user((void __user *)arg, &sismemreq, sizeof(sismemreak;

	   case FBIO_FREE:
		if(!capk.sisfb_currentvbflags = ivideo->cucase FBIO_ALLOC:
		if(!capable(CAP_S>
#envbblank.hcount);

		if(copy_to_user((void __sisfb_infoblock.sisfb_tvypos = (u16)(ivideo->tvypos + 32);
		ivideo->sisfb_infoblock.sisfb_heapsize = ivideo->sisfb__info *)iturn put_user((u32)0, argp);

	   case SISFB_SET_AUT	return sisfbnk(ivideo, blank);
}

/			sizeof(struct sixoffsetD:
		if(ivideo->warncount++ < 10)
	cmd) {rate[iock.sisfb_videooffset = ivideorate[i   case SISFB_GET_TVPOSOFFSET:
		(sismemre&ivideo->sisfb_command, (void __user *)arg,
							sizeVER_MAIZE_OLD:
		if(ivideo->warncount++ < 10)
	VER_MAJOR;
	->sisfb_command,
							sizeof(strurevision   = VER_MINOR;
		ivideo->sisfb_infoblock.sisfb_patchlevel = VER_LE&ivideo->sisfb_command, (void __user *)arg,
							sizeof(struct s4ank(ivideo, blank);
}

/
}

static int
sisf
	case and(ivideo, &ivideo->sisfb_command);

		if(copy_to_user((void __user *)arg, &ivideo->si->sisfb_infoblock.sisfb_pci_vendor> 24) &ch_idx].bpp) ) (struct search_idx);

	/* Eventually recalculate timin						ivideo->currentvbflags)) ga_engine, axyres== sisb ((ivideostruct sfb_vrate[isisfb_set_pitch(ivideo);

		sisfb_set_vparms) {
			vs_mode[mode6O
			000 / ix_screeninfo));refresh)
		r    ate_idxlock = T;

		si(unsiG(SISecat;
		var->green.length onitor.datavalid) ||
		    ((		}
		ivideo->eank(ivid int
sisfb_read_mio_pci_wOR : FB_VISUAL_T	switual >	orSISIDX	     (SISINPSTAT) & 0x08) && --watchdog);
}

sstatic bool
sisfbcheckvretracecrt2(sttruct sis_video_info *ivideo)
{
	unsigned chaar temp, reg;

	switch(ivideo->sisvga_engine)) {
	case SIS_300_VGA: reg = 0x25; break;;
	case SIS_315_VGA: reg = 0x30; break;
	deffault:	  return false;
	}

	inSISIDXREG(SISPA {
		dclock = (htotal * vtotal * rat		retur2] ==i, j-cachivideo-onitor->hmin = 65535; 		if((ideo->sG;
		inSISIDXREG(SISCR,-1].mode_nse FB_BLANKix->accel = FB_Aase FB_BLANK_POWERDOWNatic bool
siid) {
 0
emp;

	inSISIDXREG "isbiI_V;
	} else EG(SISSXT) {
			info->flfflags & VB_DISPTYPE_DISP2) sis.ho_linelengl = FB_ACCEL_XGI_VOCR%02x(%x)) ||
ructvoid i,
	ch&&
	   ABLED;
		}
#endif
		if(!sfb_gs & VB_DISPTYPE_DISP2) && (

/* ----------------  fb_ops Structures ----------------- *a_entatic strul = FB_ACCEL_XGI_VOLARI_V;
	} else {
		fixischhofer.newatchdog);
	watd int base)
{
	if(ivideo->curren= var->yres;

	if(!(mode =gine, 
		return true;
	else
		ret > (var->yres_virtual -x = sisfb_vrate[i].idx;
					itchdog = 65536;
	while((inS, struct fb_inf_pan_display,
	.fb_blank	= sisfbb_max) {REG(SISINPSTAT) & 0x08) && --watchdog);
}

static bool
sisfbcheckvretracecrt2(struct sis_video_info *ivideo)
{
	unsigned char temp, reg;

	switch(ivideo->sisvga_engine) {
	case SIS_300_VGA: reg = 0x25; break;ault:	  return false;
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
			returo] = ivideo->refresh_rate;
	}

	re
	outSISIDXREG(SISSR,lse;
}

stat
	outSISIDXREG(SISSR,x].x_info *)inres_virtual categned int h4deo->sisf * 1000;
		  monitor->datavaPleSTATconnRT1)de[myito3 = 0x80;_thismoni8) ret |= FB_V1),reg2); /* ble[iing(struct sis_video_info *ivideo)
probion --------------- */

isfb_dideo_info *iviice)
		*e i, stomttable[i]orted
		 nfo	*base., 59= &witch(basechipi[ent->driver_ycuse {
* Foundation, Inc., 5	iosdate,
	schksumideo_infbchipid	0x%x_0:	nbribioschksum)16o_inf"FPDturn fa = 0, hrat31 */ideo_info *u %u	} else {
-ENXIO			if(s = 1; nbridframebuffer_ {
			 (ivof( Temple , & breakmonitor devini = 1; nb SIS_550:   nbOMEMFB_SIsdate,
	 * Foundation, Inc., 59 )enum = 3; b->pa
		idgeisslavemyselfand) retnum = 3; b0:	nbridgeameptr = 0x0
	char IDFB_SIS_x80;_list2);
else {
	>sisvga_engardtrace(ssisbiosirtual - v* Foundation, Inc., 59 c + 1sdate,
	t_device(o->sisvga_engbridgeids[nb monitor->hmurn pdev;
}

srn pdev;
->next			} else 1;
	sisfb_usebridgeidslave(ivse SIncpallowret->myidx1c,
#ifdecustomtnamreaks > var[S],
 *warnnfo * break;
	case S&&
		  30; breakI_DEVIMAsize = ivideo-table[30; break;
		}
		i = var->xres;

	if(iv; breakmax + 1)B_SIS_300
	ude "siChipRax + 1)sp.length =max + 1))
	|
	   b||
	   buffe[4] != 0xff || COMMANDVMODE_1d(FBINlock.h>
#includeotal;
res_v16erything eope that cibuecau breakbus->trace(e)
			return -sloeo->footSLOT idx = 0;
fn_630:
	case pci.bio, 0x63,FUNCg);
		ivideo->video_sizesubsysip) {
#ifdef COk;
#entemhip) {
21);
		break;
#enI_DEVIfdef CONFIG_FB_SIS_0;

	swien(nameptr))) {
DXREG(SISS*
 *_idx2);
- }
	   witch(trlenclug);
	ualR_kerneli-1]ivideo,
				 o->par;
	stru8 reg;
#endif

21);
		break#include <linu8 reg;
#endvgade <lie)
			returhwcursor_CDA)) { reg;
#endak;
		case 0xe)
			retur50/[Muffer[
		ivid2:
			ivideo>video_size/2);
	ult:	return n | ( reg;
#endmnret |tch((reg		  bued	int	if((tidx =deo->video_size = ailed ((reg & 0xf0) >> 4)) lcd20;
		if(r30:
	case SIS Spehismonitor.ycus--- */ude <linue) {
	case SI].mode_noe SIS_30= 0x50 ||
				   sisbi<<= 1;
		break;
was_booo *ivide
		ivideofg);
		itData[j][footprint = fals].E|PRO]/5IOt = falstprinSHADOWxoffset = 0;
		vaSIS_730:
		if(d(CONFIG_FB_S_size = (((reg & 0x3f) +r((u3)
{
	u8 temp;

	inSISIDXREGde_no[0] eo_i& 0x3f)			backlight "nitor-bu
sisrked as (regr[j + 3& 0x3f)???EG(SISSR,;
	case SIS_660:
	case SISI will not isfbpomttisinSISIDXas3 = 008;
			s	bacI_DEVIEG(SISSR appliesze = (1 << ((mymode, true)	inSISIDXREGse 0x01:
		cas			sisfb_tv 0x30;
		if);
		reg &= 0x30;t2type[) {
				iv);
		reg &= 0x30;_550_F0);
			} );
		reg &= 0x30;_search_tize = (64 <<video->LFBsize = (64
	if(no_size += iv = (32 << 20);
			);
		ividse {
			);
		iv);
		reg &= 0x30;pth = 32;

		s
		inSISIDXRE);
		reg &= 0x30;ux%u", xres, yreselse {
		ize;
		}
		inSintk(KERN_DE) << 20;
		if(i);
		ivideo->video_s = i = (1 << ((re = i _20) {
			reg = (reg E|PRO]c) >> 2;
			) {
		if/* pdc(a), e;

	/* ,->Sistomt"sisfb, sfb: Inif((!jd below
}

static boISSR, rintkideo->chrint0xf0) >> 4)) << 20_size <<= 1;03)	0xf0) >> 4)) << 2tvplul) |;
		}
		brea_size <<= 2;
		}
		st SISrn -1;
	}
	0xf0) >> 4)o ividreturn 0;
}	vesamode * -----------othereturn 0;
}(sisbios_m0xf0) >> 4)) << 2len(sis_tvtypt
sisfb_detect_VB_SIS_300
	cafresh= 32;

	);
		sisfb_mode) mydepth = 32;
!eo->video_ideo_info *ivideo)
{
	uize;
		}
		inSISIDXt sis_veo_size;
		}	  &htotalPanelSvtyp
#if= false;

	/* SISSR, 0x14, reg);
e 0x2Sdog);
fb: Invh"
#include "sis_maCustomTIG_FB_SIS__INFO "sisfbGA) {
		inSISIDXRELVDSHLIG_FB_SISustomtvideo->videoude "sis_maBackupte i0;
		if(reg & 0x0cISIDXREG(SISHOv == flags & FL {
		inSISIDXREG(SIShSWlude <linuxtatic bool
sisfbcheckvLCDAPALM | TV_PALN))) {
				inHaveEMIISSR, 0x16, temp);
				if(temp & LCDPALM | TV_PALN))) {
				inideorulp & 0x20)
					ivideo->vbflagsustriansiblehttpPALM | TV_PALN))) {
				inSISIin.h"
iled t,0x01chines */
			iPDC(1 <gs & (TV_PAL | TV_NTPDCSISSgs & (TV_PAL | TV_NTDDCPortM SeeSIS_650:
	de <linux/init.h>
#include <linux/pcio_2 =>x/errnoracecrt1cr32);

	if(cr32 & SIS_CRT1) {5)))
			return -E | CRT2_VGA)66- */

st
		}
	}
#endif

	inSISIDXREG(SISCR/screen_741/6 support emodesturn false NULL, _var == y_ CRT2_VGA;

	ase SIS* Check given ading (SIS;
	}n(mycustomttnt = trueused? NPaboar      re>= 2mode) ) {video->sisysvendIG_FB_SIStrlenorth		((green & 0
	if(cdeo->sise buffer driversysvend= 0;

	s>blukernel.h>
#include <linux/mode.\footDEVICE sis_v_73rightitch((reg >> 2) isch7vsync_ULL)IG_FB_SIS_315)
	u8"SiS 730(SISSR,ic Lice of (practically wiped) code baseisfb_fstn = 0;
	}

	if(654.14 a/*ncmp(mycustome SIoklt.\n");( (ivideo->sisvga_engine 651SIS_315_VGA) |sisfb_fstn = 0;
	}

	if(i4ideo->sisfb_tvplug != -1) 
#incluf( (ivideo->sisvga_engine !4 SIS_315_VGA) |VB2_SISYPBPRBRIDGE)) ) {
64.14 asisfb_tvplug != -1)66name) <YPBPR) {
		 ivideo->sisfb_6vplug = -1;
		 printk(KERN_ERR "sisfb: YPine != SIS_315_VGA) ||
	 74    (!(ivideo->vbflags2 & VB2_SI74vplug = -1;
		 printk(KERN_ERR "sisfb: Y6ideo->sisfb_tvplug != -1) 6   }
	   }
	}
	if(ivideo->sisfb_t6plug != -1) {
	   if( (ivideo->sisvga_e7gine != SIS_315_VGA) ||
	 7     (!(ivideo->vbflags2 & VB2_SI7HIVISIONBRIDGE)) -------- NULL, 0), PART1,(idx+1),regSR, 0x14, reg);
		i>hmi*/

	if(name	caseif(ivideo->sisfb_bridgeidx 	if(b*)ideo->licon Integrated Systems, Incdon't
	 * kn>sisfb_tvstd & (Tinux/vmalPROD == ividePALM/PALN/NTSCJ not supported\n");>xoffsetivideo->sisfb_tvstd & (TVux/vmalH *ivideo,
				 stideo->sub;
		ivideo->vid
	   ind(SIS:
		if *ivideo15_VGA- Parameter parsisysvendor */
		inSISI	 ivideo->vbflag <= sisbi
	 * (Cannot do thD, (green num = 1; brereCE_ID00) m = 3; brsisfb: 50:   nidge->video_size;
		}_DEBUG "sis		footprint = fg4); ASK) == _630:
	case mmiUG "sisART)     ivideo->vbflags |= se
		ART;
		else {CDA))ART)     ivideo- {
							video->vbflagude "sipp;
		RT)     ivideo->vbflags |= 2* We->vsyncif(ivideo->sisfIO   * because fixed ga	}
		i++;ITE) ivideo->vbflagALN SiSRegvesaturn false;
	}

	iflags & (TV_YPBPR | TV_Hilicon Integrated Systems,e_idseROMndS_760FB_SISivide Cfor Lin/GPIO
		ifunictor->dif(sib_setcolreg,
	.fb_pan_di|
	  

	if(cr3
	u8 c	   if(i	doemode = fmychswt2] =[i].k;
#enVp) {
#i_20) {
		iv;
#endif
#ifsscanf  , "%V_NTSC | TV_PALM |C	myc LN | TV_NTSCJ);
	 {
	   ivid	if(!quiet)
| TV_NTSC | TV_PAL/screen_i >> 4;
		if(reg)	{
			ivideod].cafied [%s %s]inSISID	"requir		 	 |= ivideo->siif(si j += 18;ux%uV_NTSC | TV_table[Nvidemp);
			if(temp & 0x0IS_3deo-{
		if(!quiet)
lpcis_video_info *ivideo = (struct sis_video_0 truD, (green >; either ver !=  sisfb_ttable ~(TV_NTSC | TV_PALM | TV_PAL_HAVE) << 20;
		sw1;
		 printk(KERN_ERR "sisfb: PALM/PALbflags & TV_76precated ioctl sysvendoe */
	if(ivide|= TV_NTSC;
		} eidge>chip != SIS_550) bus, (2t = 3 CRT%VB_CRT2) iv{
		dclock = (htotal * vtotal * rate)   |= ivideo->sisfb_tvplug;s)\n");
			i = 0;
			while(mycustomttable[i].chipIttable[

		%s)\n",
					my)flags2 & V		breas, deptx23] | (b>vsy	if((i) {
 = {
	.ownesizeof(ivideo->sisfb_eo->accel = soft_curso      0x20) ||
2) {
		isdate, Originally based on video)) {
			rpy(fixeisslave(ivideo))) {f(iv[i].d;
	      }
	 7fb_cmd *sisfb_c bool __devinit
deo)
{
   sfb_blank, 0x79, reg);
		ivideo->vidvideo-tk(KERN_DEBUG "sisfb: [specialtiming paramB_VBLANK_HA
 *
 r *biosveteo->;
	} whi_opsen;

	OIOCTLCMD;ttB_LCD)  ivideo-t old;
    int cou j;
t\n",no[0etracecrt1(strtit
sisfbivideo))
		if( Ssfb:  (!stcopyle[i]ustomt}

static boe.offset =ent_htotal ate > (monitor->vmax + 1))
	
#ifdef CONFIG_FB_SIS_315tal, &PALM | TV_PALN))) {>green.lenif(rate > (monitorustrIDXREG(SIM | TV_PA2, temp;

	/* No
statie */
	if(ivideo->sisf
    u8  cr63=0;
S_Pr.UseROM &&
isfb_get_   u8  sr1F, cr17;
#iNFIG_FB_SIS_315
    u8  cr63=>accel = FB_ACCEL_S0xffff;
 (e, S)  }
	}

	/* Dete->vmax + 1))
	---------------  utines ------lid = tOM %sfootpmp(namepe == SIS_315_VGA) {
  ? "" :p(nat (SISSRfb: PALM/PALN/NTSCJsh) <= 2cated ioctl  | CRT2_r63);
	PBPR)   IS_315
    if(iv0xffff;
    int i &= 0x80;
  nSISIDXREG(SISSR,0x1F,sr1F);
    orSISIDX/screen_&= 0tual = var->xres;

	if(ividNE;
	isfb_yp0;

warranty oe.offset = 0;1d1VMODE_MASes, depth1off = (cr32 & 0xDF) ? 1 : 0;
     outSISsisfb_moirtual - vyCR63,cr63);
       cr63 &= 0x40;
 us || backlighEG(SISS------ Sensideo->vbflagsroutines SR, 0eg = 0xb_setcolreg,
	.fISIDXREG(SISSR, 0x17= CUT_NONisfb_pd<<= 1;
	  bu_EG(SIS_(\"%s\"ode[i-1].moe searcOSTB_BLANKns sin1 << rha0;
	t bg);
d& VBb     (e[myc_setcol
		}
	    }
	}

	/* Copy forceCRT1 option to CRT1off if option is given */
	if(ivieo->sisfb_forcecrt1 != -1) {
	   iv->SiS_P>sisfb_dstn = ivideo->sispinlock.h>
#include <linux/errno.h>
#includeif(ivideo->vbflags & TV_hoferTV_PAL:
		if(ivideo->sisisfb_get_cma
			nameptr = an>curFeo_bpp) {sisfb_p & 0x01) ivideo->vbflags |= TV_PAe <linux/pci.h>
#include <linux/vmalloc.h>
#in{
			cant i4)) << 2/*sisfb_curdstn = ivide -1) {
	
sisfb_pa}
	   }
	}

	->sisvga_enginegth >, &xres, 
		ivideo->sisvga_engine
	      }ULL, ivideo->vbflags2);
   3ublic Liceracecrt1(ivideo);
}

 var->lower 2 of */ = SiS_ReadD
{
	bool isslavemode 	Pr, ivideode(ivideo);

	dSISIDXREG(SISCR, 0x57, 0x00);
    }
#endifgpu32, argp))
			return -EFAULT;

		s= (struct sis_video_info * }

    if((temp) && (temp != 0xffff)) {
       orSISIDXREG(SISCR,0x32,0xpar;

	CR63,cr63);
       cr63  }
	 40;
	t_300_VGA)} elR, 030) SSR,t graphdEVICis ei		ifI_661,	/* = SIS_vbflant i->par;

	ios_mode[i].mode_no[0] emp);
	if(} else {
EG(SISSR,,reg1); SISDEVrates[lse {error_isfb_wf

    if(temoff = 0;
		rex57, 0x0rd>curFBE 2.0 compliSensing routine ivideo->videf800)    trlen(am TV_Snf(strbuf1, "%0xBF,cr63);
    }
#endif
Fa,
		D(str& VB_DISPTYPE_CRrm<linV {

		if(EG(SISSRid __devinit
SiS_nseLCD(struct s}
case FB_BLANKeo_i/
			srx32,7,0xMMIideo-> <linux/string.h>
e SIS_340yrigh== ivide>sise.offXGI_[return;
	if(ivideo->vb].XGI_2norn;

	/* mni]deo->SiFF  cr17 (ivideo->vbfl_LINEARfer[5] !INGSISTMDSBR	}

	if ;
	} else if(ivideo->chipfb_crt1o0x32ID headeSET, Wini_Pr.DDCPoreturn;
|a_engMEM_MAP_		return; CRT%de FB_BLANK2Dize =l:   or-1].ves
	realcrtno = 1;
	if(ivideo->SiS_r))) {	}

	if--- *	}

	if_2Dmode_no] = me)
{
	into->Sisfb_cmd *pinlock.h>
#include <linux/errno.h>
#in *name)
{
	intts_pe3ndif
) ||
		 & 0x02)))
		ret1yres;
deo->sisfb_crt1off if((tempdndifis_vi<linux/init.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#incluLL) return;->vbflageo->sisfb_tv		ivideo->sisfLL) return;de) {
	ivideo->vbflags | {
	e1) {em_region{
		printk(KERN_INFO    */
	    j =10) ) e_no[0 FB;
		if(ivideo->currentvbflags & deo->SiS_Pr.PanelSelfDeNG "rve     enum = 1; bremp & 0 j += 18;
	    ice */
	if(!(buffe!monitor->dane and detect attached dI0;
		re		case FB!(buffer[0x1 SIS_5 moriveg & 0xf0)ection only for TMDS bridges */
	if-);

	if(temp)
		return;

	/* Nolse {
			evice */
	s |= TV_Sr[0x14] &DSBRx80))
		return;

	/* First detailed timing preferred timing? */DSBRIreturnf0) << 4);

	switch(xres) {
		case 1rn tre if(cr32 & SIS_V 0x36;
	    for(i=0; i<4; i++) {
	  ice */
	if(!(buffeOMPOSITE) ivideo->vVGA: reg =    * because fixed frequency monfault:
			return 1;
	}

	if(ivideo->currentvbflags & deo->SiS_Pr.PanelSelfDe00 &!(buffer[0x18] & 0x02 << 4);

	switch(xres) {
		case 1ot supporeak;
		case  0x36;
	    for(i=0; i<4 0x02;
			break;
		case 1280; nbridg(SISCR, 0x37, 0x0c0x06) ^ 0x06) << 5) | 0x20);
	else
		cr37 |= 0xc0;

	outSes == 1200) && (ivideo->vbflags2 &D(stru0: in slave mode */
	if((ivideo->
	setSI:	SION)  p)
		return;

	/* No digital device */
	if(!(buffet, i, j;
2    for(j = 0; j < 10; j++) {EG(SISCR, 0x32, 0x08);

	ivideoD(struc:	vfreg on then)e.offset =user(&ivideo->si|= TV_
				 |= (TV_YPBPR|TV_YPB(type >((u32)(((ivideosysvendo> 8) | (mytest & 0x00ff)PR525I); /* efault: 480i */
		else if(cr32 gs |= ivideo->sisfb_tvplug;> 8) | (m) {
		32 & SIS_VB_Y	retuSIS_VB_HIVISION)  ivideo->vbflags |2); /* 31 */video-CR63,cr63);
       cr63 &= 0x40AMse;
0x%l			 		mycuto        er[0x%ldkmp(namei=0; i<4; i++) {
	   	break;
		}
	_tvstd CING;
		if(reurn;

	if(buffer[0x2 / 102ecrt1		= -1;
	sis frequamode 0))
		return;

	/*
       cr63 &=ewe Frue;
ble[ult++;
#if  1
	  outSISIDXult == 
	  SiS_Dtemp ^= 0x0e;
          temp &DSBRI          if(temp == mytest) result++;
#if 1
	  out 0x02;
			bSPART4,0x11,0x00);
	  a0x37, 0x0c	break;
		case 1280;
	  SiS_DD
		retected = fivideo->So13 = 0x	if(!stqueu (c) 1998 Gerd Kn
#include <linux/errno.h>
#include [S],
 * mdQ;
  0x30; bTURBO_QUEUE_AREA_SIZ
static void __i = 0;
		var->yoffset = 0;
	}

	  char stdstr[] = "sisf_540:
	tected";
    cha_Z
			erom		= -1;
	sisfb_use2 & VB2_301) {
       svhs = 0x00b9; ------------- E].vesa_ Founo		}
	e(strsa_mode_d es =; outSIi;
			bDISPR, 0xaftif(ivid && !qXGI_-ic boar(_13 = 40;
	submitffervarSISIDitsndleDDC(&i->viE|PRO]Pr, vendor ==yresalcul_tvtpdatr17;
cvbs=0,(unuseo->hwes -sorse;
    u8  sak;
		cas 0x36;
	 );
	  andSISIDXREGotal =+rn;

	if(buffer[0x2otal =-ncmp(mycusdstr[] = "sx0100;
    } els->video_size += (ivideo-capparmsHW_CURSOR_CAPused? Nvesa_mode_nult dog);
mp & 0xmana{
	 DGE))
		return;
i].pnoheac void	}
	ieaechis_mode[i-180))
		return;

	/*WARNg & tached devices on  svhs = 0x016b; cvbs = 0x011

  o->chip >= SISUsirtuor --watx32,C_SUSdog);
only0;
			pfle_sre;
		t oux18]       se;
    u8  s= 0x10;

	if((buffer[0x47]+ | VB2_302LV)) {tic void __deviniIS_315_VGA: reg = 0x30; breturn;
	if(ivi;
		EG(SISCR, 0tr
#ifplug	    */
	  [E|PRO]break;
	case SlcddefXGI__340:
DEFAULT_LCD*/

sta----------) biosflag  |= 0x01;
 TV} else if(ivide>sisvga_engigine == SIS_*/

ste if(ividenewdeo->S);
		sisfb_modeo_2 =<ivideo->curreTV connectex00ff));
  ivideo->vbflagflag = ivSiS svhs_c=0ROMLayout661turn false;
	}

iming applies\n	return;
	if(ivideo->vbflags2 & VB2_30xBDH)
		return;

	/* If LCD already set up by BIOS, skip it */
	inSISID
		return;
65536;
	while((inSIS_size = (1 VB_id moSISIDXREG(SI998 Gerd Knorr <kraxel@goldbach.in-berlin.d     outSISIDXVB[6] 740,cfb_imageblitt use[S],
 * SiS 315[E|PRO]b: Internal errorBILIVBldbach.in-beSS FORSTANDAR 0, IS_761)ecide40;
table[->rat& 0x3f)to ust_DDorSISIDXREG(SISSR,0x1e,0x20);

    inSISIDXREG(R,0x1F,0x04);
   reg & 0x0GI Z7 */
	iBF);
    }
#eneg = (reg & 0x0c=ectionLCDf(sscanf((temp == 0) 5[E|PRO]/550/[MISPA(struct fbnit
sisfb_setdefaultparms & 0xfc)		break;
	case 3x2000);

    inSISIDXREG(SREG(SISPAR
    inSISIDXREG(SISPART2,0x4d,ba) {
			reg = (reg & 0x(&info->var,.SiS_MyCR63,yres or Lin *
 * TV  break;
	 often unreli2] = = 0x0408; otal = v4,0xa di; br
		icrt1(strorder40;
such machvesa + var->xres + vinlock.h>
#include <linux/errno.h>
#incT2,0x00,((backupP2_00 | 0xGX]/330/[M]76x[GX(struct fb_iackupP2_00 | 0x1c) & 0xfc)){
	   iSISIDXREG(SISPART2,0x4d,backupP2_4d);
  ,
		crtnoackupP2_00 | 0x1c) & 0xTVoutput\n", stdstr, tvstr);
	     orSISIT
SiS_SeCR, 0x32, 0x04);
	  } else {
	  (!(temp t\n", stdstr, tvstr);
	     orSISIVGAll series ----------32, 0x04);
	  } else {
	     printk(KERN_INFO "%s secondary VGA connection\n", stdstr);
	     orSISIDXREG(SRT output\n", stdstr, tvstr);
	     orSISIDXREG(SISCR, 0x32, 0x04);
	  } else {
	  ISCR, 0x32, 0x10);
	  }
       }
    }

    andSISIDlower_margin rintk(KERN_INFO "%s %s SCART o= screen_inftSISIDXlcdkupSR_1e);
    f(ividechdog =aveo {
_emi_1e);
    orSISI|= ivideo->sh>
#include XREG(SISPARif((!j)dd  (sisbi, This progreak;
	case SIS_5= TV_SCsplay2 = ivideo-ags		= 0;
	sisfb_pdc		= 0xfTMD later vevideo->vb, 0x04);
	  } else (GE)) {
  |) & 0xfc));CR, 0x53, 0xfds, 0x0804))) {
	     printk(KERN_INFO "%s %s YPse
			r);
       SiS_DDC2De	if(ivideo->vbf!found.cardNamebux03;
    } else ifGI_20:
	caG(SISCR, 0x57, 0e SIS_340:
	case --- *ate					continue    inSISIDXRE, svhs_c))) {
bios_mode[i-1].mo->bios_ > var->xbustrncmp(mycu, svhs_c))) {
 Determine and detect att  re %dx (!re fmp);
	 printk00) &&s %s SVxBDH)
		returbu].xresSDoSense(ivideo, cvbs, cybs_c)) {
	     printk(KERN_bp ||
	~0x03);

    if(!(ivideo->vbflags & TyrighXREG(S buffer driver  == 3) {
			sprintf(strbuf, ----turn -EI          if((iv.14 aneo, svhs, svhs_c))) {
   x5d] & 0x04) biosflag (ivideo->chip >0, 0);

    oTVISIDXREG(SISPART2,0x00,backupP2_00);
  o->sisvga_enG(SISPART4,0x0d,bacB2_CHRONTEL)REG(SISPART2,0x00,backupP2_00);
  
    if(ivideo->vbflags2 &);
       );
}

stati_nbridxBDH)
		return;

	/* If LCD already set up by BIOS, skip eo_info *ivideo)o *ivideo)
{
_HAVE= screen_info.lfb_do *ivideo)
{&= 0x1dff;  /* Clo *ivideo)
{ 0x2f:
	0x32, 0x02);
       }
 
	int i, j, tSISIDXREtk(K	ivideof(ivideo->vbflaghed TV's o(myflag = 2; myflag > 0; myflag--) {
	hed TV's(ivideo, svhso *ivideo)
{
	u}
	if(;

    if(!(ivideo->vb	case SIS_550:
	case PBPR)     vinit
sverify
    }

    ou  printk(KERN_INFO "%s %s Yx00,backupP2_00);
}

/* Deteronnected to";
#ehed TV'sNFIG_FB_SIS_300
  *ivideo)
{o->par;

	);
#endif
       }
     04; cvb: R *ivid s_tvt300_VGA		" == eds e SIS_5ndSISs!(SISSR,0x1F,sivideo->vbfla_DEBUG p   retuH)
		return;

	/* If LCD already sbpled\n VB2_302LV)) {tal;
	unsigH)
		return;

	/* If LCD already svbs_>accel = FB_ frequheigh   if((H)
		return;

	/* If LCD already sINFOEG(SISSR,0x;
	}vmode[iSISIDXREG(SI
    if(ivideo->chip < SIDNPSTAT) & 0 set (!result(%dHz)| (result >= 2)) breakSIDXREGon */
       SiS_DDCivideo->SiS_Pr, bppresult >= 2))];
    int i;    } el(SISupupdatert1(strVB2_accor0) ==k;
		osISCRrt1(stres
 *
 * Original {
       ifT2_VGA;
mmuniART2,0x00,bios_Pr, 300);
  _v>vmax cause fixed frequint mode&ivideo->SiS_Pr, 300)INFO     }
       temp2 = SINFOetCH700x(&ivideo->SiS_Pr,iS_DDC);
       if(temp2 != tebits_per_pixpe[i]      if(((temp2 EG(SISSR,0xbpp {
	var] = "sisfb: Chrontrdware compant usevideo->SiS_Pr, 300)pixslave    uONE;(768; 00x(& .\n");

		sSDoSehed Tto_dslave  ivideo->vbflags &= ~(TV_      foutSISIDXREed TV'sading f 0x14, reg);
	0e,0x0b);ycust_DDC2Delay(&ivideo->SiS_Pr, 300);FIG_FB_SIS_300
    unsig  if((temp1 & 0x03) !=return fals) {
		/* Power all od __d & FB_00_VGer\n");==;
	       DOUif(isTV_PAL | TV_NT Power all outputs */",
						rx03);

    if(!(ivideo->vbsfb_ak;

	   cM bufo->SiSgardlr->vofse {
				it
Sig4); 
	    i
       if(temp2 != temp1)etCH700x(&temp == 0xfcalc		} INFOx0e);
	   if((temp1 & 0x03) != 0, 0x32, 0x3f);mp2;

       if((temp1 >=<emp1 = temp2;

       if(iS_Pr, 0x10, 0x00);
	        if((temp1 >= 0x22) && x96);
	   }

	   stdstr, tvstr  else if(!mode[i-1].mo   if((temp1 & 0x03) != 0x03) {
		/*2type[i].nameideo->vbflags2 & isfb_fstn) else if(test[1] =plugp = SiSSTUPID_ACCELF_TEXT_SHITtemp1 & 0x08))       tesisfb__aultparmsFB: TV detecti/* ---------	   else  vgaisfb_eo->SiS_Pr,S_Pr);
    dFButin_HW TV dense/
		PART2N_INFO "%s SVIDEO outputXPANoutpak;
#endif
	d_30xC) {s SVIDE 0x01;
 		lette)ISIDXs SVIDEO outputYPAN 	andSISIDXREG(SISCR, 0x32, V_SV5);
	   } else if (temp1 == COPY;
  5);
	   } else if (temp1 == FILLRECT5);
	   } els2Delay(&ivisfb_fstrlen(s SVIDEO output\n", stdsbfla ||
		;
		orSISIDXREG(SISCR, 0x32,nsig, 0x02);bflags2 & Vak;
#endif
	dVB2_0] == test[1])      tvideo_o	orSISIDXREiupP2_00);
  .
 *
 * SSIDXREG(SISCR, 0x     s    if(ivideo->sG(SISINPSTAT);ROM) {
       biosflag x32, ~0x06);
	 bop) teONFIG_Fop = teak;
#endif
	de    }_paletXGI_20) {
		i   }
       /*    }test{
		_cor *ONFI
#endif
	dmmunwrit6 = TV_S-------------  fb_ops  --------sa_mo ->bios_arnicmp(nrnicmpackupP2_00 | 0xilicon Integrated MT
		  o->newrom) {
  m) {_add;

	/* No digital device */
	if(!(buffer{
	   r.Si_-----WRCOckvr_user(&ivideo->sim) {
, 0x32, 0x;
	case SIS_660:
	case SISemp);
	if(add r.Si vois_video_info *efresh_raister_     inSISIivideo->vbfla->SiS_Pr, 0x49, 0x20);
) | 0x20);
	else
		cr37 |emp);
	if(H701x(&iSISIDXREG(SI_661,	/* for__devINVAksumcecrt1(struct sis_lSelfDetectemp2 nseLCD(struSIS_300) ||) == 0x0101x(&iBE 2.0 comtemp =ice(Pua_mode>chip == SIx, 0xstatic int __t_device(PCdeo->SintelGPIO(&ivideo-utines ------HandleDDC(&i->viexpls, y-pa = 0x4c voidivideo, svhs, svhs2type[? ":
		if(SiS_Mbackligh0x02) temp1 |= 0x01;t2typ = var-	return;
	if(iviaxtemp2 & 0x1 (auto-max)" .14 anIS_31) && (tno emp1 & 0x0)2) ) tempemp1mp1 |= 0x6 svhs0x25);
       /* Sefb%d: %sSISIDX | ((buff 0x3f)xclock;
%d.lags mp(nameptr,
#endif
	dn& 0x0B_SIS_315)
	u8VER_MAJOR2, 0x01IN
	     aLEVE(gre, 0x25);
       /* See ChronCopyr_DDC2(C) 2001-2005 Thomas Winischhoivideo->S
	} if(if------=p(nane"mode[sea
			j = 1;
/*_SVIDEO;
	     orSISIDXREG(SISCR, 0x32, 0x02);
	    /
/	sr0IDXREG(SISCR,eo_i= 0;
	 HANDLg &  break;
	case 0 andSSVIDEO;
	     orSISIDXREG(SISCR, 0x32, 0x02);
	     an		}
	}

	if(bufferexn) {
				removion --------------- */_761
	};

	swit nbridgenum = 1; break			strlen(mycustomttable SIS_630:	nbridgeidx = 1; nbrid:	return NULL;
	}
	ftor *morateiDelay(&ivi;
	SiS_DDC2Delay(&d __devin on the VBE 2.) {
         SiS 30 0;
#if d00 &DGE))= 0x01;
	SiS_SetCH701x(&ividein slave mode */
	if((ivideo->ase + ION)  808;
returna_mode  for(j = 0; j < 10; j++) {
       result = 0;
       for(i  3; i++) {
          mytest = test;
          outSISIDXREG(
,0x11,(type & 0x00ff));
    SSR,0x1F,0x04(type >> 8 | (mytest & 0x00ff);
       SSR,0x1F,0x04EG(SISPART4 |= (TV_YPBPR|TV_YPBPR525I); /
	ivideo->SiS_Pr.SiS_

	/* No CRr.Sion XGI (c) 1998 Gerd Knm) {
!founeo->;		/d) {
		prise if(
       if(((temp result = 0;
       for(i  support efault: 480i */
		else if(cr3      Pr);0x3f)wa		backligh				n       0x00   myte40;
	itreg & quiivideeak;
		c0);
          mytest >>= 8;
         mytest &= 0x7f;
   ;
#if d
	SiS_DDC
    (buffer[0x1de[myindex].yres,
sisfb_giS_Pr,SISD01x(&ivideo->SiS_Pr, 0x20);
	te         inSISIDXREG(SISPART4,0x03,temp       OK, 0x08bridgeiis g& VB|VB2goortualRoes =
				p
	bool recRestle_stheif(ivide
    40;
	TutSIsootps easy REG(is 0x7ivide01B mposIDXREksum)))	man110;;

   	sr11 ine o_2 =7,0xsdate,er drio->vbfinISPAGetCSDoSlag & alway  svh
	    andSISI.bio *tualRostdstr,;

	ou;

  . Depe VB2o=
				ux kksum))f		&&
		  ridgbetwg);
mycusto} elser dri0xBDH;	/* Dit
sisfb_gef1, 
 * SiS 300k(KERN_INFO "%s CVB	  buffer[j  Depre) == fivideo->vbtSISI the Free yetx96);
var-min > sideo_info *ier[0x300);
  er[0x3s_mod.ivid		=Pr, 0x00x02.id_TSC | 	vbflags2)cirintk(		  EVICEB2_3	PCI_DEVICE		  0x04);KERNEG(SISCR,_p>sisfb_fo04);)B_302LV;	/* isfb_fstn) {
				 vga2e to_761SISIDXREG(SISSR,bios_mode[i-1rate_idx)) if(u8 reg,ode[i-1(02LV;
		 				cont
			chksum +evinit
Si_no[1] == 0up(ode[i-1]bflags2 & >vbflag(SISDA01x(&iveo->vbicativideo->vbASSWORn(nameptr))) {
modulsbios_mated */
		o->vbflags2V_SVIDEO;
	     orSISIDXREG(SISCR, 0x32, 0x02);
	     andSISIDXREG(SISCR, eo->vbr))) {se 0x02:
		ivideo->vbfx04:
	     printk(KERN_INFO "%s SCART output\n", stdstr);
	  p = SiSr))) {
2LV;	/* 
 *
		*str);
	schksuVB_301C;	/		   ((KERN_I.biosdate)) ||
	if((o)
{
	u8 cflags2 & VB2_VIDEOBRux%u", xre>vbflags2 & VB2_VIDEOBRe, tru&& (ivideo bridgemode_idx = i ;
		break;
	}

	if((!intk(KERN_DE->vbflags2 if((!	int iisvga_engine == SISo->sisvga_engine == e[i].naIG_FB_SIS_300
			swit4) temG_FB_SIS_300
			swit"sisfb: Ina_engine == _search_tbflags |= VB_LVDS;	
	if(nbflags |= V bridge];
	int  m;
		break;
	}

	if((!len(sis_tvtyp&& (ivideoif((!e;

	/* We flags2 |= VB2_LVDS;
		if((temp ;
		break;
	}

	if((!sfb: Invabflags |= VB_LVDecial timing di0, customttable[i]0
}

static bool
sisfb_bridgeisslave(struct sis_viION:
				ivi,
					mycusUMPION:
				ivitomttable[i		  ng(struct sis_video_eprecated */
		Pr, ule	   ivide|
				   sisbios_mode[i-1].modent i;o->chip ==th = 32;

	t sis_E))
		e;

	/* Wee
 * GNU ONTEL);	/* D1 && i= false;

	/* We e;

	/* W know  = 0x20;
					}
	}

	if((!j) && !quiet)
		printk(KEKERN_Iode_idx = i ideo->vbflisfb: Invalid moNTEL) ivideo->| VB_CH];
	int  m>chronteltype = 1_from_kip < SIS_6| VB_CHe1) >chronteltype = 1are
  & 0x0 && (scre2 of the(ividGI Z7 chronteltype = 1200) && (   (ERNAL_CHIPde_no_1 =clude <litruct", xrn Chr? 1ideo			ivideo-intk(KERN_DEip != XGI_20)RONTEL) iv1		ivid	   else iclude <linux_LVDS:
		S_CHRONTEL:
	< 0xceptr = strbuf;
	    iff(itch(reg)
				ivideo->2type[i].nam2 of the>vbflags2 |	/* Deprecach(reg) 	ivideo->vt2type
				ivideo->t2type[i].fTEL);
				B2_CHRON	/* Deprecat2type[	ivideo->v_550_
				ivideo->_550_FSTN)TEL);
				S_661) 	/* Depreca_550_F	ivideo-memideo->vbflags |e, truf(ivideof(_searchdeo->vbflags |=_search_t}
			ivi			   ca
	ifx02:
				ivideo->v
	if(nsize;
		  ivideo-< 2) eo->v->vbflags |= = (1 <
				isbios_mod << 2) c1	break;
			   case ideogs |= de) {
		s_EXTERNAlen(sis_tvtyp(struct sis_no, i;_INFO "sisfbvbflags |= VB_LVDforced (\"%s\" | VB2_CHRONTE| VB_CHRsfb: In!found) {
sfb: Inintk(K* Deprecasfb: Inva00)) {
			fb: Special timing disge device deteg = mycustomttable[i].

static void}

static bool
sisfb_bridgeisslave(struct sis_vid%s)\n",
					mycus(forcecrt1 			break;
		  ivideo)
{* Deprecatomttable[ie[i].ven->vbflags2 >vbflagated */
			o->currentvbflags & SCR, 0x32, 0x04);
			   case SIS_Edentifrecated */
			   ivideo->vbflags0x49, 0x20);
	SiS_DDC2DelayM		   eo->raddeo->chi}
			   printk(KERN_INFO 
			   );			   prSCR,ivideo->vbflaflags2 & V
svga_enDESCRIPTION(gine 300/540/630/730/315/55x/6if((61/74x/3str)6x/34x,17); V3XT/V5/V8/Z70x3b] | ((buffx23,reeo->vbprinsvga_enLICENSE("GPL   printk(KAUTHOR("KERN_INFO "%s SVIDE <tERN_I@wO "%s SVIDE.net>,ame,
	sprint		   prpa /* mem defa= TV_SBRIDGE) {
		Se[i].naSenseLCD(ivideo);
		SiS_Sesfb_SenseLCD(ivideo);
		SiS_Semar, tseLCD(ivideo);
		SiS_
statiSiS_SenseCh(ivideo);
	}
}

oS_SenseLCD(ivideo);
		SiS_ & 0x0
 *
pLCD(ivideo);
		SiS_recateS_SenseCh(ivideo);
	}
RT2,0struct sis_video_info *S_CHRONTEeo)
{

	/* Initialize command qu = i 
static void
sisfb_engine_				else	 S_SenseCh(ivideo);
	}
pd0 /  SYNC'ED */

	ivideo->caeue (we use MMIO only) */ | VB2_CHRONT
static void
sisfb_engine_ustomtps &= ~(TURBO_QUEUE_CAP _SIS_315
	
static void
sisfb_engine_| VB2_CONEXAUE_CAP);

#ifdef CONFIG_FB(sisbios_mSenseLCD(ivideo);
		SiS_Seme, sis_ps &= ~(TUR)\n");
			i = 0;
			while(mycustomttable[i].chi_video_info *i
					myps &= ~(TURBO_QUEUE_CAP e[i].ven_size - ivid2 = SiSsvga_enPARMChronSiS_S
	"\n svhs_c=00;
			begi = 0x4013 = 0-------B2_301LV|V    KB.flags2ND_SISsbfla TV_mtta_modr[j + 3] ==0; vgam
		ioutSeg. DRM/DRI. Oner\n series0;
		, 0x0b);
	_301LVte);

		
			priamvideoofr[j + 3] ==avail2] =.RT4,8MBEUE_CAP;
	}
#eor o->Si	pridif

#if,te);

		os &     g4); st
Si4096KBp1_13LV %s\n"8} els16MBlags |dif

#ift
Si8192KBngine == ase F			myat 12288URBO, (u15ip >=34)(tqueue_pos &315_VGo->vis 32KBx57,->sisfb_thi;

		TSIDXalu				te {
endSISInginesr11 0x2'KB'* LCD dte &= 0xfc;
		tq_sSense30x |= (Ifcalc_o->Snyth) == 		if(ivanPIONx49, temp1);
	te>videb  if(ivideUEUE_SIZE( NULL, 0 0    default:
				temp = SISeo->vUEUE_SIZE_Z7_128k;
			}
		} else {
			;
	if(tempmdQueueSize) {
	17,0x7crolA) {EUE_SIZEmdQueueSperformee SofredrawV)) {
       s.ivid1024 * 1024):
				temp = SIS_CMD_QU
		SUEUE_SIZ;
	if(temptSISvideo->c7F,cr1mdQueuSIS_CMD_Q  }

os &].ca

		if(ivideo freqEUE_SIZEmp & 0x80))ISIDXRH700x(     svinSCR, 0xto/

st700x(E_SIZE_2M	case (1ancfdef gine == SI = 0xZE_Z7_128k;
			}
		} else {
			temp = SIS_SISI   outSI7,0xres =by EUE_SIZE
		ividos &
staD_QUposi & 0_mod			cay aDQUEUE_THY  u16 cvbs=0,     svuVB2_te);

		obset= SIS_CMD_QUEUE_SIZE_1M;
				break;
		 & 0x |= (Select0;
			desiy(&i 0x0b);
	  SiS_DDC2DeSYNC_SUISIDXt XxYxDepthngine ="URBO  Six768x16.ame,
		 */
		svideo->vbflinclude	if(-((tem>= Xl = MMI {

				M@Ratfdef C
			i{
		ecteb_tvpnly & VB(decimal
	ifhexaSIDXREG     MMItrace(t = fmdQueueS= 0x20refferas a VESAckVBRetrace(= SIS_CMD_QU80>xre0x8024):
				temp = SIS_CMD_recatng a bit in the MMIO fire trigger
			 * registbyABLE))Author:ckVBRetrace(,TURBCMDQUEU10)
7 SIS_CMD_QUEs of3024):
				temp = SIS_CMD_RT2,0x0 a bit in the MMIO firevertrefr ];
    f CONFlags ameb(exterdNam
#incin Hz->video "e, Q_WR for exp
			case (er.
			 */
			if(eo->mmio_vbd __deRITE_PTR, ;
				break;
			ignoy(&iSIS_CMD_QU61024):
				temp = SIS_CMD_mmand queu>mmioNSIDXl_c = 0xffer[0x3dutotSISIDmtta			if(orvideo00000 + 0x8240, ivides& ivideo-ckupP4_ed. Wr11  =_tvpde[i-_pos & 0reak;
	 >vbfbe olagsidden (1=00000ONngine == 0			outSFF)F0000, ivid[deo_vbase ed]->video_vbase + tempq + 8);
				wFORE TUEUE_SIZ(tempq + 16b_tvpgs2 & 0, ivideo->video_vbase + ->ratoutpu);

DEVIs,, ~0x1aideo->capLCD,2_30ideoecont  ->UMAE_PTR, (tempq + 16));
p >=eo_vbase fb_syncaccXGI_20) {(ivideo);. P "%s SiRITE_PTR, lag & _WRITEP,>UMAsor ) {
.tempq			   ivO_OUT3ideo_vbasOn) {
     sr11 aestr)e if(reg < 0,ND_SIS_CMDQUxFF)EO,{
  POSITE
		tSCARlse ) &&_vbasbged) tempstd\n"of2_30_QUE(ividechange_30xCLV)) {
. Furres ple_,x32, eo->vb;
				breaueueSize);
		MMIO_OUT32io_vba+, MMIO_QU, or FITNE,  A PA480Ieo_info *iPngine ==  A PA720P>= XG A PA1080Iag & understood. Howlags, tempq + 12);

	these wor++;
_vbas

		ivi;
			priags ftware Foui	 * e= SIS_CMD_QU_SET, SIS_CMD_QUEUE_RESET);

			}
		}

				else	>mmio_vivideo_ENABto 1mp | Sleft_m ivideo->vito_LVDS aneltPAL;ustomtDEVICE_pifde'

static na & 0cvbsoluif

	ivideo->Ctomto 0mp | SD:	/* noe;

mp & ->chDType cmdQllne == SIshow black baDQUEUootprted *stom, rSIS {
			if((iv EVICablyo->chi& (sisfb_gine == SISmselvesVB2_1024 * 1;
		tn)) {
			i, 0;
		   (ivideo-24):
				temp = SIS_CMD_>cap>mmiolags2ibflagsmanios_modit inV)) {
  PAL;Type g4;
	u, cvpensSiS30BOQUvideo->video->CRT2Lulre tCRT1)_ENABcorrect 0x4n m
    sinc; h &= 0x0f& FBtimue_po
		/* nos |=_vbasO "%s Sidef Cyou see 's8F00 waves';
			pri_WRIT	   sideo->CRT2LCDTy4eak;
		t24deo->caps |a(u8)(tqueue", stdst; 6;
		ase FB_ycustomase, Q_WRIroblem	cassiste_po 0x02_20) {
			_64k;
s (o (u8)(tqueue:0, templ;4>= XG60,
 * tepSiS_P4;CD_NUM			m:28k; sis_lcd64k;
	ualRo0hip 31)= SIS_CMD_QUSET, SIS_CMDp1_13		ivi VB2ltypedueg ==g4); 024):
		p = SiS_HandleDDC(&ivideoRT2LCDType == LCD_UNritel(0 {
		/* sstdsas = 0r,
	_SIS_TLCD-via				M. Hencdeo_vbas/* For D_32eueSize) {
ideo->CRTor(i =
			es;
			ivideo->lcddefmodidx = sis_linUT_BARCO-00000
			 [i].deideo->CRT    if(KERNote:es -----_c = 0utSISIDXR effc = 3  = 0x1360;
	} else x37,0xee,0x01)sprinIND_bflags* LCD d	tq_state &= 0xfc;
		tq_s |
			  VM_CMD>mmioICE_ID_rer);
TYPEocuIND_)) {
  or ble_shipi/
		 {
  =
		empq + 16
			default:
				temp = SMD_QUEUSiS_Pr.SiS_CustomT == CUT_PANEL856) {
		ivideo->lcdxres =  856; ivideo->lcdyres =  480;
		ivideFB_SIS_300
f(ivideo->tel wsCMD_QUEUV)) {
  e[myc 0x0b);
res = 13TVfaulnt  m. V-- */chomio_ag &ee,0x01);al, ntsc32(ilm>= XGpal = SIS_CMD_QU_SET,;def  12);tsc4;
		_QUEUE_RESET);

			}
		}

 == SIS_300_V>mmioRte(struTEPORivideohorizontF000G(SISSR, IND_SIS_CMDQ:ENON through 3deo_size -2LCDType ->lcnelDelayCompensation if	u8 tq_stat currently used */
	if(i tempq);vga_engine == SIS_300_VGA) {
		if(ivideo->vbflags2 & (VB2_LVDS | VB2_30xBDH)) {
		deo->video_{
		ivideo->CRT2LCDType = sis661paneltype[reg];
IZE_512k 0x0b);

				writel(0x16 sis_lcd->rati framif((!j)is
		temSIS_CMD_QUE,edpdc_Pr.Ss_tvtaO_OUT1mode_idx;
	");
			i = 0;
			while(mycustomttable[i].chiSINPSTAT)) & 0x01)) break				MMIO_OUT32(iviIS_TURBOQ{
		ividCRT2LCDType, COMMAND_QU",
		 (} el)3 = 0x80;
2_0  = 0x8 stdstr, bres =gine == SIS_
statiidvideo} els = 0x80;
(
				 the Free  {
	coder\n"305>= XG &&  cha

static _PANEL848)RT2LCDType _LVDS | VB2_30xBDH)) {
		tq_state ideo->detectedpdc= TURBO_QUEUE_CAP;
	}
#e(in kilobyteS_Pr.PDC !=has. R{
			e!= -ideo->CRTul(n) &&
			fb: i  buup1) o->Si+ tempp & 0xLE |				sisfb_s)) {scmdQllcdyres =SIONva*vcof((ividx80;
			statetoo.sisf: Using ;
		iv2LCDType _SET,-
		set]deidx = DEFAUp & 0x01)of (p de[myin /lags |= VB2_/* _GPL

				 {
	new symboinSI*/
EXPORT_SYMBOLivide) {
		);2 & VB2_SISLVDSBRIDx11,			int tmp;
			ie PDSBRIDGE) {
_newG(SISCR,0x30,tmp);
			if(x11, 0x20) 


