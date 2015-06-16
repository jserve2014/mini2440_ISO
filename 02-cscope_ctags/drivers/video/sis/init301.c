/* $XFree86$ */
/* $XdotOrg$ */
/*
 * Mode initializing code (CRT2 section)
 * for SiS 300/305/540/630/730,
 *     SiS 315/550/[M]650/651/[M]661[FGM]X/[M]74x[GX]/330/[M]76x[GX],
 *     XGI V3XT/V5/V8, Z7
 * (Universal module for Linux kernel framebuffer and X.org/XFree86 4.x)
 *
 * Copyright (C) 2001-2005 by Thomas Winischhofer, Vienna, Austria
 *
 * If distributed as part of the Linux kernel, the following license terms
 * apply:
 *
 * * This program is free software; you can redistribute it and/or modify
 * * it under the terms of the GNU General Public License as published by
 * * the Free Software Foundation; either version 2 of the named License,
 * * or any later version.
 * *
 * * This program is distributed in the hope that it will be useful,
 * * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * * GNU General Public License for more details.
 * *
 * * You should have received a copy of the GNU General Public License
 * * along with this program; if not, write to the Free Software
 * * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Otherwise, the following license terms apply:
 *
 * * Redistribution and use in source and binary forms, with or without
 * * modification, are permitted provided that the following conditions
 * * are met:
 * * 1) Redistributions of source code must retain the above copyright
 * *    notice, this list of conditions and the following disclaimer.
 * * 2) Redistributions in binary form must reproduce the above copyright
 * *    notice, this list of conditions and the following disclaimer in the
 * *    documentation and/or other materials provided with the distribution.
 * * 3) The name of the author may not be used to endorse or promote products
 * *    derived from this software without specific prior written permission.
 * *
 * * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: 	Thomas Winischhofer <thomas@winischhofer.net>
 *
 * Formerly based on non-functional code-fragements for 300 series by SiS, Inc.
 * Used by permission.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if 1
#define SET_EMI		/* 302LV/ELV: Set EMI values */
#endif

#if 1
#define SET_PWD		/* 301/302LV: Set PWD */
#endif

#define COMPAL_HACK	/* Needed for Compal 1400x1050 (EMI) */
#define COMPAQ_HACK	/* Needed for Inventec/Compaq 1280x1024 (EMI) */
#define ASUS_HACK	/* Needed for Asus A2H 1024x768 (EMI) */

#include "init301.h"

#ifdef SIS300
#include "oem300.h"
#endif

#ifdef SIS315H
#include "oem310.h"
#endif

#define SiS_I2CDELAY      1000
#define SiS_I2CDELAYSHORT  150

static unsigned short	SiS_GetBIOSLCDResInfo(struct SiS_Private *SiS_Pr);
#ifdef SIS_LINUX_KERNEL
static void		SiS_SetCH70xx(struct SiS_Private *SiS_Pr, unsigned short reg, unsigned char val);
#endif

/*********************************************/
/*         HELPER: Lock/Unlock CRT2          */
/*********************************************/

void
SiS_UnLockCRT2(struct SiS_Private *SiS_Pr)
{
   if(SiS_Pr->ChipType == XGI_20)
      return;
   else if(SiS_Pr->ChipType >= SIS_315H)
      SiS_SetRegOR(SiS_Pr->SiS_Part1Port,0x2f,0x01);
   else
      SiS_SetRegOR(SiS_Pr->SiS_Part1Port,0x24,0x01);
}

#ifdef SIS_LINUX_KERNEL
static
#endif
void
SiS_LockCRT2(struct SiS_Private *SiS_Pr)
{
   if(SiS_Pr->ChipType == XGI_20)
      return;
   else if(SiS_Pr->ChipType >= SIS_315H)
      SiS_SetRegAND(SiS_Pr->SiS_Part1Port,0x2F,0xFE);
   else
      SiS_SetRegAND(SiS_Pr->SiS_Part1Port,0x24,0xFE);
}

/*********************************************/
/*            HELPER: Write SR11             */
/*********************************************/

static void
SiS_SetRegSR11ANDOR(struct SiS_Private *SiS_Pr, unsigned short DataAND, unsigned short DataOR)
{
   if(SiS_Pr->ChipType >= SIS_661) {
      DataAND &= 0x0f;
      DataOR  &= 0x0f;
   }
   SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x11,DataAND,DataOR);
}

/*********************************************/
/*    HELPER: Get Pointer to LCD structure   */
/*********************************************/

#ifdef SIS315H
static unsigned char *
GetLCDStructPtr661(struct SiS_Private *SiS_Pr)
{
   unsigned char  *ROMAddr = SiS_Pr->VirtualRomBase;
   unsigned char  *myptr = NULL;
   unsigned short romindex = 0, reg = 0, idx = 0;

   /* Use the BIOS tables only for LVDS panels; TMDS is unreliable
    * due to the variaty of panels the BIOS doesn't know about.
    * Exception: If the BIOS has better knowledge (such as in case
    * of machines with a 301C and a panel that does not support DDC)
    * use the BIOS data as well.
    */

   if((SiS_Pr->SiS_ROMNew) &&
      ((SiS_Pr->SiS_VBType & VB_SISLVDS) || (!SiS_Pr->PanelSelfDetected))) {

      if(SiS_Pr->ChipType < SIS_661) reg = 0x3c;
      else                           reg = 0x7d;

      idx = (SiS_GetReg(SiS_Pr->SiS_P3d4,reg) & 0x1f) * 26;

      if(idx < (8*26)) {
         myptr = (unsigned char *)&SiS_LCDStruct661[idx];
      }
      romindex = SISGETROMW(0x100);
      if(romindex) {
         romindex += idx;
         myptr = &ROMAddr[romindex];
      }
   }
   return myptr;
}

static unsigned short
GetLCDStructPtr661_2(struct SiS_Private *SiS_Pr)
{
   unsigned char  *ROMAddr = SiS_Pr->VirtualRomBase;
   unsigned short romptr = 0;

   /* Use the BIOS tables only for LVDS panels; TMDS is unreliable
    * due to the variaty of panels the BIOS doesn't know about.
    * Exception: If the BIOS has better knowledge (such as in case
    * of machines with a 301C and a panel that does not support DDC)
    * use the BIOS data as well.
    */

   if((SiS_Pr->SiS_ROMNew) &&
      ((SiS_Pr->SiS_VBType & VB_SISLVDS) || (!SiS_Pr->PanelSelfDetected))) {
      romptr = SISGETROMW(0x102);
      romptr += ((SiS_GetReg(SiS_Pr->SiS_P3d4,0x36) >> 4) * SiS_Pr->SiS661LCD2TableSize);
   }

   return romptr;
}
#endif

/*********************************************/
/*           Adjust Rate for CRT2            */
/*********************************************/

static bool
SiS_AdjustCRT2Rate(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex,
		unsigned short RRTI, unsigned short *i)
{
   unsigned short checkmask=0, modeid, infoflag;

   modeid = SiS_Pr->SiS_RefIndex[RRTI + (*i)].ModeID;

   if(SiS_Pr->SiS_VBType & VB_SISVB) {

      if(SiS_Pr->SiS_VBInfo & SetCRT2ToRAMDAC) {

	 checkmask |= SupportRAMDAC2;
	 if(SiS_Pr->ChipType >= SIS_315H) {
	    checkmask |= SupportRAMDAC2_135;
	    if(SiS_Pr->SiS_VBType & VB_SIS30xBLV) {
	       checkmask |= SupportRAMDAC2_162;
	       if(SiS_Pr->SiS_VBType & VB_SISRAMDAC202) {
		  checkmask |= SupportRAMDAC2_202;
	       }
	    }
	 }

      } else if(SiS_Pr->SiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToLCDA)) {

	 checkmask |= SupportLCD;
	 if(SiS_Pr->ChipType >= SIS_315H) {
	    if(SiS_Pr->SiS_VBType & VB_SISVB) {
	       if((SiS_Pr->SiS_LCDInfo & DontExpandLCD) && (SiS_Pr->SiS_LCDInfo & LCDPass11)) {
	          if(modeid == 0x2e) checkmask |= Support64048060Hz;
	       }
	    }
	 }

      } else if(SiS_Pr->SiS_VBInfo & SetCRT2ToHiVision) {

	 checkmask |= SupportHiVision;

      } else if(SiS_Pr->SiS_VBInfo & (SetCRT2ToYPbPr525750|SetCRT2ToAVIDEO|SetCRT2ToSVIDEO|SetCRT2ToSCART)) {

	 checkmask |= SupportTV;
	 if(SiS_Pr->SiS_VBType & VB_SIS30xBLV) {
	    checkmask |= SupportTV1024;
	    if(SiS_Pr->SiS_VBInfo & SetCRT2ToYPbPr525750) {
	       if(SiS_Pr->SiS_TVMode & TVSetYPbPr750p) {
	          checkmask |= SupportYPbPr750p;
	       }
	    }
	 }

      }

   } else {	/* LVDS */

      if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
	 if(SiS_Pr->SiS_VBInfo & SetCRT2ToTV) {
	    checkmask |= SupportCHTV;
	 }
      }

      if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) {
	 checkmask |= SupportLCD;
      }

   }

   /* Look backwards in table for matching CRT2 mode */
   for(; SiS_Pr->SiS_RefIndex[RRTI + (*i)].ModeID == modeid; (*i)--) {
      infoflag = SiS_Pr->SiS_RefIndex[RRTI + (*i)].Ext_InfoFlag;
      if(infoflag & checkmask) return true;
      if((*i) == 0) break;
   }

   /* Look through the whole mode-section of the table from the beginning
    * for a matching CRT2 mode if no mode was found yet.
    */
   for((*i) = 0; ; (*i)++) {
      if(SiS_Pr->SiS_RefIndex[RRTI + (*i)].ModeID != modeid) break;
      infoflag = SiS_Pr->SiS_RefIndex[RRTI + (*i)].Ext_InfoFlag;
      if(infoflag & checkmask) return true;
   }
   return false;
}

/*********************************************/
/*              Get rate index               */
/*********************************************/

unsigned short
SiS_GetRatePtr(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex)
{
   unsigned short RRTI,i,backup_i;
   unsigned short modeflag,index,temp,backupindex;
   static const unsigned short LCDRefreshIndex[] = {
		0x00, 0x00, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01,
		0x00, 0x00, 0x00, 0x00
   };

   /* Do NOT check for UseCustomMode here, will skrew up FIFO */
   if(ModeNo == 0xfe) return 0;

   if(ModeNo <= 0x13) {
      modeflag = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
   } else {
      modeflag = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
   }

   if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
      if(SiS_Pr->SiS_VBInfo & SetCRT2ToTV) {
	 if(modeflag & HalfDCLK) return 0;
      }
   }

   if(ModeNo < 0x14) return 0xFFFF;

   index = (SiS_GetReg(SiS_Pr->SiS_P3d4,0x33) >> SiS_Pr->SiS_SelectCRT2Rate) & 0x0F;
   backupindex = index;

   if(index > 0) index--;

   if(SiS_Pr->SiS_SetFlag & ProgrammingCRT2) {
      if(SiS_Pr->SiS_VBType & VB_SISVB) {
	 if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) {
	    if(SiS_Pr->SiS_VBType & VB_NoLCD)		 index = 0;
	    else if(SiS_Pr->SiS_LCDInfo & DontExpandLCD) index = backupindex = 0;
	 }
	 if(SiS_Pr->SiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToLCDA)) {
	    if(!(SiS_Pr->SiS_VBType & VB_NoLCD)) {
	       temp = LCDRefreshIndex[SiS_GetBIOSLCDResInfo(SiS_Pr)];
	       if(index > temp) index = temp;
	    }
	 }
      } else {
	 if(SiS_Pr->SiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToLCDA)) index = 0;
	 if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
	    if(SiS_Pr->SiS_VBInfo & SetCRT2ToTV) index = 0;
	 }
      }
   }

   RRTI = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].REFindex;
   ModeNo = SiS_Pr->SiS_RefIndex[RRTI].ModeID;

   if(SiS_Pr->ChipType >= SIS_315H) {
      if(!(SiS_Pr->SiS_VBInfo & DriverMode)) {
	 if( (SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_VESAID == 0x105) ||
	     (SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_VESAID == 0x107) ) {
	    if(backupindex <= 1) RRTI++;
	 }
      }
   }

   i = 0;
   do {
      if(SiS_Pr->SiS_RefIndex[RRTI + i].ModeID != ModeNo) break;
      temp = SiS_Pr->SiS_RefIndex[RRTI + i].Ext_InfoFlag;
      temp &= ModeTypeMask;
      if(temp < SiS_Pr->SiS_ModeType) break;
      i++;
      index--;
   } while(index != 0xFFFF);

   if(!(SiS_Pr->SiS_VBInfo & SetCRT2ToRAMDAC)) {
      if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
	 temp = SiS_Pr->SiS_RefIndex[RRTI + i - 1].Ext_InfoFlag;
	 if(temp & InterlaceMode) i++;
      }
   }

   i--;

   if((SiS_Pr->SiS_SetFlag & ProgrammingCRT2) && (!(SiS_Pr->SiS_VBInfo & DisableCRT2Display))) {
      backup_i = i;
      if(!(SiS_AdjustCRT2Rate(SiS_Pr, ModeNo, ModeIdIndex, RRTI, &i))) {
	 i = backup_i;
      }
   }

   return (RRTI + i);
}

/*********************************************/
/*            STORE CRT2 INFO in CR34        */
/*********************************************/

static void
SiS_SaveCRT2Info(struct SiS_Private *SiS_Pr, unsigned short ModeNo)
{
   unsigned short temp1, temp2;

   /* Store CRT1 ModeNo in CR34 */
   SiS_SetReg(SiS_Pr->SiS_P3d4,0x34,ModeNo);
   temp1 = (SiS_Pr->SiS_VBInfo & SetInSlaveMode) >> 8;
   temp2 = ~(SetInSlaveMode >> 8);
   SiS_SetRegANDOR(SiS_Pr->SiS_P3d4,0x31,temp2,temp1);
}

/*********************************************/
/*    HELPER: GET SOME DATA FROM BIOS ROM    */
/*********************************************/

#ifdef SIS300
static bool
SiS_CR36BIOSWord23b(struct SiS_Private *SiS_Pr)
{
   unsigned char  *ROMAddr = SiS_Pr->VirtualRomBase;
   unsigned short temp,temp1;

   if(SiS_Pr->SiS_UseROM) {
      if((ROMAddr[0x233] == 0x12) && (ROMAddr[0x234] == 0x34)) {
	 temp = 1 << ((SiS_GetReg(SiS_Pr->SiS_P3d4,0x36) >> 4) & 0x0f);
	 temp1 = SISGETROMW(0x23b);
	 if(temp1 & temp) return true;
      }
   }
   return false;
}

static bool
SiS_CR36BIOSWord23d(struct SiS_Private *SiS_Pr)
{
   unsigned char  *ROMAddr = SiS_Pr->VirtualRomBase;
   unsigned short temp,temp1;

   if(SiS_Pr->SiS_UseROM) {
      if((ROMAddr[0x233] == 0x12) && (ROMAddr[0x234] == 0x34)) {
	 temp = 1 << ((SiS_GetReg(SiS_Pr->SiS_P3d4,0x36) >> 4) & 0x0f);
	 temp1 = SISGETROMW(0x23d);
	 if(temp1 & temp) return true;
      }
   }
   return false;
}
#endif

/*********************************************/
/*          HELPER: DELAY FUNCTIONS          */
/*********************************************/

void
SiS_DDC2Delay(struct SiS_Private *SiS_Pr, unsigned int delaytime)
{
   while (delaytime-- > 0)
      SiS_GetReg(SiS_Pr->SiS_P3c4, 0x05);
}

#if defined(SIS300) || defined(SIS315H)
static void
SiS_GenericDelay(struct SiS_Private *SiS_Pr, unsigned short delay)
{
   SiS_DDC2Delay(SiS_Pr, delay * 36);
}
#endif

#ifdef SIS315H
static void
SiS_LongDelay(struct SiS_Private *SiS_Pr, unsigned short delay)
{
   while(delay--) {
      SiS_GenericDelay(SiS_Pr, 6623);
   }
}
#endif

#if defined(SIS300) || defined(SIS315H)
static void
SiS_ShortDelay(struct SiS_Private *SiS_Pr, unsigned short delay)
{
   while(delay--) {
      SiS_GenericDelay(SiS_Pr, 66);
   }
}
#endif

static void
SiS_PanelDelay(struct SiS_Private *SiS_Pr, unsigned short DelayTime)
{
#if defined(SIS300) || defined(SIS315H)
   unsigned char  *ROMAddr = SiS_Pr->VirtualRomBase;
   unsigned short PanelID, DelayIndex, Delay=0;
#endif

   if(SiS_Pr->ChipType < SIS_315H) {

#ifdef SIS300

      PanelID = SiS_GetReg(SiS_Pr->SiS_P3d4,0x36);
      if(SiS_Pr->SiS_VBType & VB_SISVB) {
	 if(SiS_Pr->SiS_VBType & VB_SIS301) PanelID &= 0xf7;
	 if(!(SiS_GetReg(SiS_Pr->SiS_P3c4,0x18) & 0x10)) PanelID = 0x12;
      }
      DelayIndex = PanelID >> 4;
      if((DelayTime >= 2) && ((PanelID & 0x0f) == 1))  {
	 Delay = 3;
      } else {
	 if(DelayTime >= 2) DelayTime -= 2;
	 if(!(DelayTime & 0x01)) {
	    Delay = SiS_Pr->SiS_PanelDelayTbl[DelayIndex].timer[0];
	 } else {
	    Delay = SiS_Pr->SiS_PanelDelayTbl[DelayIndex].timer[1];
	 }
	 if(SiS_Pr->SiS_UseROM) {
	    if(ROMAddr[0x220] & 0x40) {
	       if(!(DelayTime & 0x01)) Delay = (unsigned short)ROMAddr[0x225];
	       else 	    	       Delay = (unsigned short)ROMAddr[0x226];
	    }
	 }
      }
      SiS_ShortDelay(SiS_Pr, Delay);

#endif  /* SIS300 */

   } else {

#ifdef SIS315H

      if((SiS_Pr->ChipType >= SIS_661)    ||
	 (SiS_Pr->ChipType <= SIS_315PRO) ||
	 (SiS_Pr->ChipType == SIS_330)    ||
	 (SiS_Pr->SiS_ROMNew)) {

	 if(!(DelayTime & 0x01)) {
	    SiS_DDC2Delay(SiS_Pr, 0x1000);
	 } else {
	    SiS_DDC2Delay(SiS_Pr, 0x4000);
	 }

      } else if((SiS_Pr->SiS_IF_DEF_LVDS == 1) /* ||
	 (SiS_Pr->SiS_CustomT == CUT_COMPAQ1280) ||
	 (SiS_Pr->SiS_CustomT == CUT_CLEVO1400) */ ) {			/* 315 series, LVDS; Special */

	 if(SiS_Pr->SiS_IF_DEF_CH70xx == 0) {
	    PanelID = SiS_GetReg(SiS_Pr->SiS_P3d4,0x36);
	    if(SiS_Pr->SiS_CustomT == CUT_CLEVO1400) {
	       if(!(SiS_GetReg(SiS_Pr->SiS_P3c4,0x1b) & 0x10)) PanelID = 0x12;
	    }
	    if(SiS_Pr->SiS_CustomT == CUT_COMPAQ1280) {
	       DelayIndex = PanelID & 0x0f;
	    } else {
	       DelayIndex = PanelID >> 4;
	    }
	    if((DelayTime >= 2) && ((PanelID & 0x0f) == 1))  {
	       Delay = 3;
	    } else {
	       if(DelayTime >= 2) DelayTime -= 2;
	       if(!(DelayTime & 0x01)) {
		  Delay = SiS_Pr->SiS_PanelDelayTblLVDS[DelayIndex].timer[0];
		} else {
		  Delay = SiS_Pr->SiS_PanelDelayTblLVDS[DelayIndex].timer[1];
	       }
	       if((SiS_Pr->SiS_UseROM) && (!(SiS_Pr->SiS_ROMNew))) {
		  if(ROMAddr[0x13c] & 0x40) {
		     if(!(DelayTime & 0x01)) {
			Delay = (unsigned short)ROMAddr[0x17e];
		     } else {
			Delay = (unsigned short)ROMAddr[0x17f];
		     }
		  }
	       }
	    }
	    SiS_ShortDelay(SiS_Pr, Delay);
	 }

      } else if(SiS_Pr->SiS_VBType & VB_SISVB) {			/* 315 series, all bridges */

	 DelayIndex = SiS_GetReg(SiS_Pr->SiS_P3d4,0x36) >> 4;
	 if(!(DelayTime & 0x01)) {
	    Delay = SiS_Pr->SiS_PanelDelayTbl[DelayIndex].timer[0];
	 } else {
	    Delay = SiS_Pr->SiS_PanelDelayTbl[DelayIndex].timer[1];
	 }
	 Delay <<= 8;
	 SiS_DDC2Delay(SiS_Pr, Delay);

      }

#endif /* SIS315H */

   }
}

#ifdef SIS315H
static void
SiS_PanelDelayLoop(struct SiS_Private *SiS_Pr, unsigned short DelayTime, unsigned short DelayLoop)
{
   int i;
   for(i = 0; i < DelayLoop; i++) {
      SiS_PanelDelay(SiS_Pr, DelayTime);
   }
}
#endif

/*********************************************/
/*    HELPER: WAIT-FOR-RETRACE FUNCTIONS     */
/*********************************************/

void
SiS_WaitRetrace1(struct SiS_Private *SiS_Pr)
{
   unsigned short watchdog;

   if(SiS_GetReg(SiS_Pr->SiS_P3c4,0x1f) & 0xc0) return;
   if(!(SiS_GetReg(SiS_Pr->SiS_P3d4,0x17) & 0x80)) return;

   watchdog = 65535;
   while((SiS_GetRegByte(SiS_Pr->SiS_P3da) & 0x08) && --watchdog);
   watchdog = 65535;
   while((!(SiS_GetRegByte(SiS_Pr->SiS_P3da) & 0x08)) && --watchdog);
}

#if defined(SIS300) || defined(SIS315H)
static void
SiS_WaitRetrace2(struct SiS_Private *SiS_Pr, unsigned short reg)
{
   unsigned short watchdog;

   watchdog = 65535;
   while((SiS_GetReg(SiS_Pr->SiS_Part1Port,reg) & 0x02) && --watchdog);
   watchdog = 65535;
   while((!(SiS_GetReg(SiS_Pr->SiS_Part1Port,reg) & 0x02)) && --watchdog);
}
#endif

static void
SiS_WaitVBRetrace(struct SiS_Private *SiS_Pr)
{
   if(SiS_Pr->ChipType < SIS_315H) {
#ifdef SIS300
      if(SiS_Pr->SiS_VBType & VB_SIS30xBLV) {
	 if(!(SiS_GetReg(SiS_Pr->SiS_Part1Port,0x00) & 0x20)) return;
      }
      if(!(SiS_GetReg(SiS_Pr->SiS_Part1Port,0x00) & 0x80)) {
	 SiS_WaitRetrace1(SiS_Pr);
      } else {
	 SiS_WaitRetrace2(SiS_Pr, 0x25);
      }
#endif
   } else {
#ifdef SIS315H
      if(!(SiS_GetReg(SiS_Pr->SiS_Part1Port,0x00) & 0x40)) {
	 SiS_WaitRetrace1(SiS_Pr);
      } else {
	 SiS_WaitRetrace2(SiS_Pr, 0x30);
      }
#endif
   }
}

static void
SiS_VBWait(struct SiS_Private *SiS_Pr)
{
   unsigned short tempal,temp,i,j;

   temp = 0;
   for(i = 0; i < 3; i++) {
     for(j = 0; j < 100; j++) {
        tempal = SiS_GetRegByte(SiS_Pr->SiS_P3da);
        if(temp & 0x01) {
	   if((tempal & 0x08))  continue;
	   else break;
        } else {
	   if(!(tempal & 0x08)) continue;
	   else break;
        }
     }
     temp ^= 0x01;
   }
}

static void
SiS_VBLongWait(struct SiS_Private *SiS_Pr)
{
   if(SiS_Pr->SiS_VBInfo & SetCRT2ToTV) {
      SiS_VBWait(SiS_Pr);
   } else {
      SiS_WaitRetrace1(SiS_Pr);
   }
}

/*********************************************/
/*               HELPER: MISC                */
/*********************************************/

#ifdef SIS300
static bool
SiS_Is301B(struct SiS_Private *SiS_Pr)
{
   if(SiS_GetReg(SiS_Pr->SiS_Part4Port,0x01) >= 0xb0) return true;
   return false;
}
#endif

static bool
SiS_CRT2IsLCD(struct SiS_Private *SiS_Pr)
{
   if(SiS_Pr->ChipType == SIS_730) {
      if(SiS_GetReg(SiS_Pr->SiS_P3c4,0x13) & 0x20) return true;
   }
   if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x30) & 0x20) return true;
   return false;
}

bool
SiS_IsDualEdge(struct SiS_Private *SiS_Pr)
{
#ifdef SIS315H
   if(SiS_Pr->ChipType >= SIS_315H) {
      if((SiS_Pr->ChipType != SIS_650) || (SiS_GetReg(SiS_Pr->SiS_P3d4,0x5f) & 0xf0)) {
	 if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x38) & EnableDualEdge) return true;
      }
   }
#endif
   return false;
}

bool
SiS_IsVAMode(struct SiS_Private *SiS_Pr)
{
#ifdef SIS315H
   unsigned short flag;

   if(SiS_Pr->ChipType >= SIS_315H) {
      flag = SiS_GetReg(SiS_Pr->SiS_P3d4,0x38);
      if((flag & EnableDualEdge) && (flag & SetToLCDA)) return true;
   }
#endif
   return false;
}

#ifdef SIS315H
static bool
SiS_IsVAorLCD(struct SiS_Private *SiS_Pr)
{
   if(SiS_IsVAMode(SiS_Pr))  return true;
   if(SiS_CRT2IsLCD(SiS_Pr)) return true;
   return false;
}
#endif

static bool
SiS_IsDualLink(struct SiS_Private *SiS_Pr)
{
#ifdef SIS315H
   if(SiS_Pr->ChipType >= SIS_315H) {
      if((SiS_CRT2IsLCD(SiS_Pr)) ||
         (SiS_IsVAMode(SiS_Pr))) {
	 if(SiS_Pr->SiS_LCDInfo & LCDDualLink) return true;
      }
   }
#endif
   return false;
}

#ifdef SIS315H
static bool
SiS_TVEnabled(struct SiS_Private *SiS_Pr)
{
   if((SiS_GetReg(SiS_Pr->SiS_Part2Port,0x00) & 0x0f) != 0x0c) return true;
   if(SiS_Pr->SiS_VBType & VB_SISYPBPR) {
      if(SiS_GetReg(SiS_Pr->SiS_Part2Port,0x4d) & 0x10) return true;
   }
   return false;
}
#endif

#ifdef SIS315H
static bool
SiS_LCDAEnabled(struct SiS_Private *SiS_Pr)
{
   if(SiS_GetReg(SiS_Pr->SiS_Part1Port,0x13) & 0x04) return true;
   return false;
}
#endif

#ifdef SIS315H
static bool
SiS_WeHaveBacklightCtrl(struct SiS_Private *SiS_Pr)
{
   if((SiS_Pr->ChipType >= SIS_315H) && (SiS_Pr->ChipType < SIS_661)) {
      if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x79) & 0x10) return true;
   }
   return false;
}
#endif

#ifdef SIS315H
static bool
SiS_IsNotM650orLater(struct SiS_Private *SiS_Pr)
{
   unsigned short flag;

   if(SiS_Pr->ChipType == SIS_650) {
      flag = SiS_GetReg(SiS_Pr->SiS_P3d4,0x5f) & 0xf0;
      /* Check for revision != A0 only */
      if((flag == 0xe0) || (flag == 0xc0) ||
         (flag == 0xb0) || (flag == 0x90)) return false;
   } else if(SiS_Pr->ChipType >= SIS_661) return false;
   return true;
}
#endif

#ifdef SIS315H
static bool
SiS_IsYPbPr(struct SiS_Private *SiS_Pr)
{
   if(SiS_Pr->ChipType >= SIS_315H) {
      /* YPrPb = 0x08 */
      if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x38) & EnableCHYPbPr) return true;
   }
   return false;
}
#endif

#ifdef SIS315H
static bool
SiS_IsChScart(struct SiS_Private *SiS_Pr)
{
   if(SiS_Pr->ChipType >= SIS_315H) {
      /* Scart = 0x04 */
      if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x38) & EnableCHScart) return true;
   }
   return false;
}
#endif

#ifdef SIS315H
static bool
SiS_IsTVOrYPbPrOrScart(struct SiS_Private *SiS_Pr)
{
   unsigned short flag;

   if(SiS_Pr->ChipType >= SIS_315H) {
      flag = SiS_GetReg(SiS_Pr->SiS_P3d4,0x30);
      if(flag & SetCRT2ToTV)        return true;
      flag = SiS_GetReg(SiS_Pr->SiS_P3d4,0x38);
      if(flag & EnableCHYPbPr)      return true;  /* = YPrPb = 0x08 */
      if(flag & EnableCHScart)      return true;  /* = Scart = 0x04 - TW */
   } else {
      flag = SiS_GetReg(SiS_Pr->SiS_P3d4,0x30);
      if(flag & SetCRT2ToTV)        return true;
   }
   return false;
}
#endif

#ifdef SIS315H
static bool
SiS_IsLCDOrLCDA(struct SiS_Private *SiS_Pr)
{
   unsigned short flag;

   if(SiS_Pr->ChipType >= SIS_315H) {
      flag = SiS_GetReg(SiS_Pr->SiS_P3d4,0x30);
      if(flag & SetCRT2ToLCD) return true;
      flag = SiS_GetReg(SiS_Pr->SiS_P3d4,0x38);
      if(flag & SetToLCDA)    return true;
   } else {
      flag = SiS_GetReg(SiS_Pr->SiS_P3d4,0x30);
      if(flag & SetCRT2ToLCD) return true;
   }
   return false;
}
#endif

static bool
SiS_HaveBridge(struct SiS_Private *SiS_Pr)
{
   unsigned short flag;

   if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
      return true;
   } else if(SiS_Pr->SiS_VBType & VB_SISVB) {
      flag = SiS_GetReg(SiS_Pr->SiS_Part4Port,0x00);
      if((flag == 1) || (flag == 2)) return true;
   }
   return false;
}

static bool
SiS_BridgeIsEnabled(struct SiS_Private *SiS_Pr)
{
   unsigned short flag;

   if(SiS_HaveBridge(SiS_Pr)) {
      flag = SiS_GetReg(SiS_Pr->SiS_Part1Port,0x00);
      if(SiS_Pr->ChipType < SIS_315H) {
	flag &= 0xa0;
	if((flag == 0x80) || (flag == 0x20)) return true;
      } else {
	flag &= 0x50;
	if((flag == 0x40) || (flag == 0x10)) return true;
      }
   }
   return false;
}

static bool
SiS_BridgeInSlavemode(struct SiS_Private *SiS_Pr)
{
   unsigned short flag1;

   flag1 = SiS_GetReg(SiS_Pr->SiS_P3d4,0x31);
   if(flag1 & (SetInSlaveMode >> 8)) return true;
   return false;
}

/*********************************************/
/*       GET VIDEO BRIDGE CONFIG INFO        */
/*********************************************/

/* Setup general purpose IO for Chrontel communication */
#ifdef SIS300
void
SiS_SetChrontelGPIO(struct SiS_Private *SiS_Pr, unsigned short myvbinfo)
{
   unsigned int   acpibase;
   unsigned short temp;

   if(!(SiS_Pr->SiS_ChSW)) return;

#ifdef SIS_LINUX_KERNEL
   acpibase = sisfb_read_lpc_pci_dword(SiS_Pr, 0x74);
#else
   acpibase = pciReadLong(0x00000800, 0x74);
#endif
   acpibase &= 0xFFFF;
   if(!acpibase) return;
   temp = SiS_GetRegShort((acpibase + 0x3c));	/* ACPI register 0x3c: GP Event 1 I/O mode select */
   temp &= 0xFEFF;
   SiS_SetRegShort((acpibase + 0x3c), temp);
   temp = SiS_GetRegShort((acpibase + 0x3c));
   temp = SiS_GetRegShort((acpibase + 0x3a));	/* ACPI register 0x3a: GP Pin Level (low/high) */
   temp &= 0xFEFF;
   if(!(myvbinfo & SetCRT2ToTV)) temp |= 0x0100;
   SiS_SetRegShort((acpibase + 0x3a), temp);
   temp = SiS_GetRegShort((acpibase + 0x3a));
}
#endif

void
SiS_GetVBInfo(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
		unsigned short ModeIdIndex, int checkcrt2mode)
{
   unsigned short tempax, tempbx, temp;
   unsigned short modeflag, resinfo = 0;

   SiS_Pr->SiS_SetFlag = 0;

   modeflag = SiS_GetModeFlag(SiS_Pr, ModeNo, ModeIdIndex);

   SiS_Pr->SiS_ModeType = modeflag & ModeTypeMask;

   if((ModeNo > 0x13) && (!SiS_Pr->UseCustomMode)) {
      resinfo = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;
   }

   tempbx = 0;

   if(SiS_HaveBridge(SiS_Pr)) {

	temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x30);
	tempbx |= temp;
	tempax = SiS_GetReg(SiS_Pr->SiS_P3d4,0x31) << 8;
	tempax &= (DriverMode | LoadDACFlag | SetNotSimuMode | SetPALTV);
	tempbx |= tempax;

#ifdef SIS315H
	if(SiS_Pr->ChipType >= SIS_315H) {
	   if(SiS_Pr->SiS_VBType & VB_SISLCDA) {
	      if(ModeNo == 0x03) {
		 /* Mode 0x03 is never in driver mode */
		 SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x31,0xbf);
	      }
	      if(!(SiS_GetReg(SiS_Pr->SiS_P3d4,0x31) & (DriverMode >> 8))) {
		 /* Reset LCDA setting if not driver mode */
		 SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x38,0xfc);
	      }
	      if(IS_SIS650) {
		 if(SiS_Pr->SiS_UseLCDA) {
		    if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x5f) & 0xF0) {
		       if((ModeNo <= 0x13) || (!(SiS_GetReg(SiS_Pr->SiS_P3d4,0x31) & (DriverMode >> 8)))) {
			  SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x38,(EnableDualEdge | SetToLCDA));
		       }
		    }
		 }
	      }
	      temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x38);
	      if((temp & (EnableDualEdge | SetToLCDA)) == (EnableDualEdge | SetToLCDA)) {
		 tempbx |= SetCRT2ToLCDA;
	      }
	   }

	   if(SiS_Pr->ChipType >= SIS_661) { /* New CR layout */
	      tempbx &= ~(SetCRT2ToYPbPr525750 | SetCRT2ToHiVision);
	      if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x38) & 0x04) {
		 temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x35) & 0xe0;
		 if(temp == 0x60) tempbx |= SetCRT2ToHiVision;
		 else if(SiS_Pr->SiS_VBType & VB_SISYPBPR) {
		    tempbx |= SetCRT2ToYPbPr525750;
		 }
	      }
	   }

	   if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
	      temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x38);
	      if(temp & SetToLCDA) {
		 tempbx |= SetCRT2ToLCDA;
	      }
	      if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
		 if(temp & EnableCHYPbPr) {
		    tempbx |= SetCRT2ToCHYPbPr;
		 }
	      }
	   }
	}

#endif  /* SIS315H */

        if(!(SiS_Pr->SiS_VBType & VB_SISVGA2)) {
	   tempbx &= ~(SetCRT2ToRAMDAC);
	}

	if(SiS_Pr->SiS_VBType & VB_SISVB) {
	   temp = SetCRT2ToSVIDEO   |
		  SetCRT2ToAVIDEO   |
		  SetCRT2ToSCART    |
		  SetCRT2ToLCDA     |
		  SetCRT2ToLCD      |
		  SetCRT2ToRAMDAC   |
		  SetCRT2ToHiVision |
		  SetCRT2ToYPbPr525750;
	} else {
	   if(SiS_Pr->ChipType >= SIS_315H) {
	      if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
		 temp = SetCRT2ToAVIDEO |
		        SetCRT2ToSVIDEO |
		        SetCRT2ToSCART  |
		        SetCRT2ToLCDA   |
		        SetCRT2ToLCD    |
		        SetCRT2ToCHYPbPr;
	      } else {
		 temp = SetCRT2ToLCDA   |
		        SetCRT2ToLCD;
	      }
	   } else {
	      if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
		 temp = SetCRT2ToTV | SetCRT2ToLCD;
	      } else {
		 temp = SetCRT2ToLCD;
	      }
	   }
	}

	if(!(tempbx & temp)) {
	   tempax = DisableCRT2Display;
	   tempbx = 0;
	}

	if(SiS_Pr->SiS_VBType & VB_SISVB) {

	   unsigned short clearmask = ( DriverMode |
				DisableCRT2Display |
				LoadDACFlag 	   |
				SetNotSimuMode 	   |
				SetInSlaveMode 	   |
				SetPALTV 	   |
				SwitchCRT2	   |
				SetSimuScanMode );

	   if(tempbx & SetCRT2ToLCDA)        tempbx &= (clearmask | SetCRT2ToLCDA);
	   if(tempbx & SetCRT2ToRAMDAC)      tempbx &= (clearmask | SetCRT2ToRAMDAC);
	   if(tempbx & SetCRT2ToLCD)         tempbx &= (clearmask | SetCRT2ToLCD);
	   if(tempbx & SetCRT2ToSCART)       tempbx &= (clearmask | SetCRT2ToSCART);
	   if(tempbx & SetCRT2ToHiVision)    tempbx &= (clearmask | SetCRT2ToHiVision);
	   if(tempbx & SetCRT2ToYPbPr525750) tempbx &= (clearmask | SetCRT2ToYPbPr525750);

	} else {

	   if(SiS_Pr->ChipType >= SIS_315H) {
	      if(tempbx & SetCRT2ToLCDA) {
		 tempbx &= (0xFF00|SwitchCRT2|SetSimuScanMode);
	      }
	   }
	   if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
	      if(tempbx & SetCRT2ToTV) {
		 tempbx &= (0xFF00|SetCRT2ToTV|SwitchCRT2|SetSimuScanMode);
	      }
	   }
	   if(tempbx & SetCRT2ToLCD) {
	      tempbx &= (0xFF00|SetCRT2ToLCD|SwitchCRT2|SetSimuScanMode);
	   }
	   if(SiS_Pr->ChipType >= SIS_315H) {
	      if(tempbx & SetCRT2ToLCDA) {
	         tempbx |= SetCRT2ToLCD;
	      }
	   }

	}

	if(tempax & DisableCRT2Display) {
	   if(!(tempbx & (SwitchCRT2 | SetSimuScanMode))) {
	      tempbx = SetSimuScanMode | DisableCRT2Display;
	   }
	}

	if(!(tempbx & DriverMode)) tempbx |= SetSimuScanMode;

	/* LVDS/CHRONTEL (LCD/TV) and 301BDH (LCD) can only be slave in 8bpp modes */
	if(SiS_Pr->SiS_ModeType <= ModeVGA) {
	   if( (SiS_Pr->SiS_IF_DEF_LVDS == 1) ||
	       ((SiS_Pr->SiS_VBType & VB_NoLCD) && (tempbx & SetCRT2ToLCD)) ) {
	      modeflag &= (~CRT2Mode);
	   }
	}

	if(!(tempbx & SetSimuScanMode)) {
	   if(tempbx & SwitchCRT2) {
	      if((!(modeflag & CRT2Mode)) && (checkcrt2mode)) {
		 if(resinfo != SIS_RI_1600x1200) {
		    tempbx |= SetSimuScanMode;
		 }
              }
	   } else {
	      if(SiS_BridgeIsEnabled(SiS_Pr)) {
		 if(!(tempbx & DriverMode)) {
		    if(SiS_BridgeInSlavemode(SiS_Pr)) {
		       tempbx |= SetSimuScanMode;
		    }
		 }
	      }
	   }
	}

	if(!(tempbx & DisableCRT2Display)) {
	   if(tempbx & DriverMode) {
	      if(tempbx & SetSimuScanMode) {
		 if((!(modeflag & CRT2Mode)) && (checkcrt2mode)) {
		    if(resinfo != SIS_RI_1600x1200) {
		       tempbx |= SetInSlaveMode;
		    }
		 }
	      }
	   } else {
	      tempbx |= SetInSlaveMode;
	   }
	}

   }

   SiS_Pr->SiS_VBInfo = tempbx;

#ifdef SIS300
   if(SiS_Pr->ChipType == SIS_630) {
      SiS_SetChrontelGPIO(SiS_Pr, SiS_Pr->SiS_VBInfo);
   }
#endif

#ifdef SIS_LINUX_KERNEL
#if 0
   printk(KERN_DEBUG "sisfb: (init301: VBInfo= 0x%04x, SetFlag=0x%04x)\n",
      SiS_Pr->SiS_VBInfo, SiS_Pr->SiS_SetFlag);
#endif
#endif
#ifdef SIS_XORG_XF86
#ifdef TWDEBUG
   xf86DrvMsg(0, X_PROBED, "(init301: VBInfo=0x%04x, SetFlag=0x%04x)\n",
      SiS_Pr->SiS_VBInfo, SiS_Pr->SiS_SetFlag);
#endif
#endif
}

/*********************************************/
/*           DETERMINE YPbPr MODE            */
/*********************************************/

void
SiS_SetYPbPr(struct SiS_Private *SiS_Pr)
{

   unsigned char temp;

   /* Note: This variable is only used on 30xLV systems.
    * CR38 has a different meaning on LVDS/CH7019 systems.
    * On 661 and later, these bits moved to CR35.
    *
    * On 301, 301B, only HiVision 1080i is supported.
    * On 30xLV, 301C, only YPbPr 1080i is supported.
    */

   SiS_Pr->SiS_YPbPr = 0;
   if(SiS_Pr->ChipType >= SIS_661) return;

   if(SiS_Pr->SiS_VBType) {
      if(SiS_Pr->SiS_VBInfo & SetCRT2ToHiVision) {
	 SiS_Pr->SiS_YPbPr = YPbPrHiVision;
      }
   }

   if(SiS_Pr->ChipType >= SIS_315H) {
      if(SiS_Pr->SiS_VBType & VB_SISYPBPR) {
	 temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x38);
	 if(temp & 0x08) {
	    switch((temp >> 4)) {
	    case 0x00: SiS_Pr->SiS_YPbPr = YPbPr525i;     break;
	    case 0x01: SiS_Pr->SiS_YPbPr = YPbPr525p;     break;
	    case 0x02: SiS_Pr->SiS_YPbPr = YPbPr750p;     break;
	    case 0x03: SiS_Pr->SiS_YPbPr = YPbPrHiVision; break;
	    }
	 }
      }
   }

}

/*********************************************/
/*           DETERMINE TVMode flag           */
/*********************************************/

void
SiS_SetTVMode(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex)
{
   unsigned char  *ROMAddr = SiS_Pr->VirtualRomBase;
   unsigned short temp, temp1, resinfo = 0, romindex = 0;
   unsigned char  OutputSelect = *SiS_Pr->pSiS_OutputSelect;

   SiS_Pr->SiS_TVMode = 0;

   if(!(SiS_Pr->SiS_VBInfo & SetCRT2ToTV)) return;
   if(SiS_Pr->UseCustomMode) return;

   if(ModeNo > 0x13) {
      resinfo = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;
   }

   if(SiS_Pr->ChipType < SIS_661) {

      if(SiS_Pr->SiS_VBInfo & SetPALTV) SiS_Pr->SiS_TVMode |= TVSetPAL;

      if(SiS_Pr->SiS_VBType & VB_SISVB) {
	 temp = 0;
	 if((SiS_Pr->ChipType == SIS_630) ||
	    (SiS_Pr->ChipType == SIS_730)) {
	    temp = 0x35;
	    romindex = 0xfe;
	 } else if(SiS_Pr->ChipType >= SIS_315H) {
	    temp = 0x38;
	    if(SiS_Pr->ChipType < XGI_20) {
	       romindex = 0xf3;
	       if(SiS_Pr->ChipType >= SIS_330) romindex = 0x11b;
	    }
	 }
	 if(temp) {
	    if(romindex && SiS_Pr->SiS_UseROM && (!(SiS_Pr->SiS_ROMNew))) {
	       OutputSelect = ROMAddr[romindex];
	       if(!(OutputSelect & EnablePALMN)) {
		  SiS_SetRegAND(SiS_Pr->SiS_P3d4,temp,0x3F);
	       }
	    }
	    temp1 = SiS_GetReg(SiS_Pr->SiS_P3d4,temp);
	    if(SiS_Pr->SiS_TVMode & TVSetPAL) {
	       if(temp1 & EnablePALM) {		/* 0x40 */
		  SiS_Pr->SiS_TVMode |= TVSetPALM;
		  SiS_Pr->SiS_TVMode &= ~TVSetPAL;
	       } else if(temp1 & EnablePALN) {	/* 0x80 */
		  SiS_Pr->SiS_TVMode |= TVSetPALN;
	       }
	    } else {
	       if(temp1 & EnableNTSCJ) {	/* 0x40 */
		  SiS_Pr->SiS_TVMode |= TVSetNTSCJ;
	       }
	    }
	 }
	 /* Translate HiVision/YPbPr to our new flags */
	 if(SiS_Pr->SiS_VBInfo & SetCRT2ToHiVision) {
	    if(SiS_Pr->SiS_YPbPr == YPbPr750p)          SiS_Pr->SiS_TVMode |= TVSetYPbPr750p;
	    else if(SiS_Pr->SiS_YPbPr == YPbPr525p)     SiS_Pr->SiS_TVMode |= TVSetYPbPr525p;
	    else if(SiS_Pr->SiS_YPbPr == YPbPrHiVision) SiS_Pr->SiS_TVMode |= TVSetHiVision;
	    else				        SiS_Pr->SiS_TVMode |= TVSetYPbPr525i;
	    if(SiS_Pr->SiS_TVMode & (TVSetYPbPr750p | TVSetYPbPr525p | TVSetYPbPr525i)) {
	       SiS_Pr->SiS_VBInfo &= ~SetCRT2ToHiVision;
	       SiS_Pr->SiS_VBInfo |= SetCRT2ToYPbPr525750;
	    } else if(SiS_Pr->SiS_TVMode & TVSetHiVision) {
	       SiS_Pr->SiS_TVMode |= TVSetPAL;
	    }
	 }
      } else if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
	 if(SiS_Pr->SiS_CHOverScan) {
	    if(SiS_Pr->SiS_IF_DEF_CH70xx == 1) {
	       temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x35);
	       if((temp & TVOverScan) || (SiS_Pr->SiS_CHOverScan == 1)) {
		  SiS_Pr->SiS_TVMode |= TVSetCHOverScan;
	       }
	    } else if(SiS_Pr->SiS_IF_DEF_CH70xx == 2) {
	       temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x79);
	       if((temp & 0x80) || (SiS_Pr->SiS_CHOverScan == 1)) {
		  SiS_Pr->SiS_TVMode |= TVSetCHOverScan;
	       }
	    }
	    if(SiS_Pr->SiS_CHSOverScan) {
	       SiS_Pr->SiS_TVMode |= TVSetCHOverScan;
	    }
	 }
	 if(SiS_Pr->SiS_IF_DEF_CH70xx == 2) {
	    temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x38);
	    if(SiS_Pr->SiS_TVMode & TVSetPAL) {
	       if(temp & EnablePALM)      SiS_Pr->SiS_TVMode |= TVSetPALM;
	       else if(temp & EnablePALN) SiS_Pr->SiS_TVMode |= TVSetPALN;
	    } else {
	       if(temp & EnableNTSCJ) {
		  SiS_Pr->SiS_TVMode |= TVSetNTSCJ;
	       }
	    }
	 }
      }

   } else {  /* 661 and later */

      temp1 = SiS_GetReg(SiS_Pr->SiS_P3d4,0x35);
      if(temp1 & 0x01) {
	 SiS_Pr->SiS_TVMode |= TVSetPAL;
	 if(temp1 & 0x08) {
	    SiS_Pr->SiS_TVMode |= TVSetPALN;
	 } else if(temp1 & 0x04) {
	    if(SiS_Pr->SiS_VBType & VB_SISVB) {
	       SiS_Pr->SiS_TVMode &= ~TVSetPAL;
	    }
	    SiS_Pr->SiS_TVMode |= TVSetPALM;
	 }
      } else {
	 if(temp1 & 0x02) {
	    SiS_Pr->SiS_TVMode |= TVSetNTSCJ;
	 }
      }
      if(SiS_Pr->SiS_IF_DEF_CH70xx == 2) {
	 if(SiS_Pr->SiS_CHOverScan) {
	    if((temp1 & 0x10) || (SiS_Pr->SiS_CHOverScan == 1)) {
	       SiS_Pr->SiS_TVMode |= TVSetCHOverScan;
	    }
	 }
      }
      if(SiS_Pr->SiS_VBType & VB_SISVB) {
	 if(SiS_Pr->SiS_VBInfo & SetCRT2ToYPbPr525750) {
	    temp1 &= 0xe0;
	    if(temp1 == 0x00)      SiS_Pr->SiS_TVMode |= TVSetYPbPr525i;
	    else if(temp1 == 0x20) SiS_Pr->SiS_TVMode |= TVSetYPbPr525p;
	    else if(temp1 == 0x40) SiS_Pr->SiS_TVMode |= TVSetYPbPr750p;
	 } else if(SiS_Pr->SiS_VBInfo & SetCRT2ToHiVision) {
	    SiS_Pr->SiS_TVMode |= (TVSetHiVision | TVSetPAL);
	 }
	 if(SiS_Pr->SiS_VBInfo & (SetCRT2ToYPbPr525750 | SetCRT2ToHiVision)) {
	    if(resinfo == SIS_RI_800x480 || resinfo == SIS_RI_1024x576 || resinfo == SIS_RI_1280x720) {
	       SiS_Pr->SiS_TVMode |= TVAspect169;
	    } else {
	       temp1 = SiS_GetReg(SiS_Pr->SiS_P3d4,0x39);
	       if(temp1 & 0x02) {
		  if(SiS_Pr->SiS_TVMode & (TVSetYPbPr750p | TVSetHiVision)) {
		     SiS_Pr->SiS_TVMode |= TVAspect169;
		  } else {
		     SiS_Pr->SiS_TVMode |= TVAspect43LB;
		  }
	       } else {
		  SiS_Pr->SiS_TVMode |= TVAspect43;
	       }
	    }
	 }
      }
   }

   if(SiS_Pr->SiS_VBInfo & SetCRT2ToSCART) SiS_Pr->SiS_TVMode |= TVSetPAL;

   if(SiS_Pr->SiS_VBType & VB_SISVB) {

      if(SiS_Pr->SiS_VBInfo & SetCRT2ToHiVision) {
	 SiS_Pr->SiS_TVMode |= TVSetPAL;
	 SiS_Pr->SiS_TVMode &= ~(TVSetPALM | TVSetPALN | TVSetNTSCJ);
      } else if(SiS_Pr->SiS_VBInfo & SetCRT2ToYPbPr525750) {
	 if(SiS_Pr->SiS_TVMode & (TVSetYPbPr525i | TVSetYPbPr525p | TVSetYPbPr750p)) {
	    SiS_Pr->SiS_TVMode &= ~(TVSetPAL | TVSetNTSCJ | TVSetPALM | TVSetPALN);
	 }
      }

      if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
	 if(!(SiS_Pr->SiS_VBInfo & SetNotSimuMode)) {
	    SiS_Pr->SiS_TVMode |= TVSetTVSimuMode;
	 }
      }

      if(!(SiS_Pr->SiS_TVMode & TVSetPAL)) {
	 if(resinfo == SIS_RI_1024x768) {
	    if(SiS_Pr->SiS_TVMode & TVSetYPbPr525p) {
	       SiS_Pr->SiS_TVMode |= TVSet525p1024;
	    } else if(!(SiS_Pr->SiS_TVMode & (TVSetHiVision | TVSetYPbPr750p))) {
	       SiS_Pr->SiS_TVMode |= TVSetNTSC1024;
	    }
	 }
      }

      SiS_Pr->SiS_TVMode |= TVRPLLDIV2XO;
      if((SiS_Pr->SiS_VBInfo & SetCRT2ToHiVision) &&
	 (SiS_Pr->SiS_VBInfo & SetInSlaveMode)) {
	 SiS_Pr->SiS_TVMode &= ~TVRPLLDIV2XO;
      } else if(SiS_Pr->SiS_TVMode & (TVSetYPbPr525p | TVSetYPbPr750p)) {
	 SiS_Pr->SiS_TVMode &= ~TVRPLLDIV2XO;
      } else if(!(SiS_Pr->SiS_VBType & VB_SIS30xBLV)) {
	 if(SiS_Pr->SiS_TVMode & TVSetTVSimuMode) {
	    SiS_Pr->SiS_TVMode &= ~TVRPLLDIV2XO;
	 }
      }

   }

   SiS_Pr->SiS_VBInfo &= ~SetPALTV;

#ifdef SIS_XORG_XF86
#ifdef TWDEBUG
   xf86DrvMsg(0, X_INFO, "(init301: TVMode %x, VBInfo %x)\n", SiS_Pr->SiS_TVMode, SiS_Pr->SiS_VBInfo);
#endif
#endif
}

/*********************************************/
/*               GET LCD INFO                */
/*********************************************/

static unsigned short
SiS_GetBIOSLCDResInfo(struct SiS_Private *SiS_Pr)
{
   unsigned short temp = SiS_Pr->SiS_LCDResInfo;
   /* Translate my LCDResInfo to BIOS value */
   switch(temp) {
   case Panel_1280x768_2: temp = Panel_1280x768;    break;
   case Panel_1280x800_2: temp = Panel_1280x800;    break;
   case Panel_1280x854:   temp = Panel661_1280x854; break;
   }
   return temp;
}

static void
SiS_GetLCDInfoBIOS(struct SiS_Private *SiS_Pr)
{
#ifdef SIS315H
   unsigned char  *ROMAddr;
   unsigned short temp;

#ifdef SIS_XORG_XF86
#ifdef TWDEBUG
   xf86DrvMsg(0, X_INFO, "Paneldata driver: [%d %d] [H %d %d] [V %d %d] [C %d 0x%02x 0x%02x]\n",
	SiS_Pr->PanelHT, SiS_Pr->PanelVT,
	SiS_Pr->PanelHRS, SiS_Pr->PanelHRE,
	SiS_Pr->PanelVRS, SiS_Pr->PanelVRE,
	SiS_Pr->SiS_VBVCLKData[SiS_Pr->PanelVCLKIdx315].CLOCK,
	SiS_Pr->SiS_VBVCLKData[SiS_Pr->PanelVCLKIdx315].Part4_A,
	SiS_Pr->SiS_VBVCLKData[SiS_Pr->PanelVCLKIdx315].Part4_B);
#endif
#endif

   if((ROMAddr = GetLCDStructPtr661(SiS_Pr))) {
      if((temp = SISGETROMW(6)) != SiS_Pr->PanelHT) {
	 SiS_Pr->SiS_NeedRomModeData = true;
	 SiS_Pr->PanelHT  = temp;
      }
      if((temp = SISGETROMW(8)) != SiS_Pr->PanelVT) {
	 SiS_Pr->SiS_NeedRomModeData = true;
	 SiS_Pr->PanelVT  = temp;
      }
      SiS_Pr->PanelHRS = SISGETROMW(10);
      SiS_Pr->PanelHRE = SISGETROMW(12);
      SiS_Pr->PanelVRS = SISGETROMW(14);
      SiS_Pr->PanelVRE = SISGETROMW(16);
      SiS_Pr->PanelVCLKIdx315 = VCLK_CUSTOM_315;
      SiS_Pr->SiS_VCLKData[VCLK_CUSTOM_315].CLOCK =
	 SiS_Pr->SiS_VBVCLKData[VCLK_CUSTOM_315].CLOCK = (unsigned short)((unsigned char)ROMAddr[18]);
      SiS_Pr->SiS_VCLKData[VCLK_CUSTOM_315].SR2B =
	 SiS_Pr->SiS_VBVCLKData[VCLK_CUSTOM_315].Part4_A = ROMAddr[19];
      SiS_Pr->SiS_VCLKData[VCLK_CUSTOM_315].SR2C =
	 SiS_Pr->SiS_VBVCLKData[VCLK_CUSTOM_315].Part4_B = ROMAddr[20];

#ifdef SIS_XORG_XF86
#ifdef TWDEBUG
      xf86DrvMsg(0, X_INFO, "Paneldata BIOS:  [%d %d] [H %d %d] [V %d %d] [C %d 0x%02x 0x%02x]\n",
	SiS_Pr->PanelHT, SiS_Pr->PanelVT,
	SiS_Pr->PanelHRS, SiS_Pr->PanelHRE,
	SiS_Pr->PanelVRS, SiS_Pr->PanelVRE,
	SiS_Pr->SiS_VBVCLKData[SiS_Pr->PanelVCLKIdx315].CLOCK,
	SiS_Pr->SiS_VBVCLKData[SiS_Pr->PanelVCLKIdx315].Part4_A,
	SiS_Pr->SiS_VBVCLKData[SiS_Pr->PanelVCLKIdx315].Part4_B);
#endif
#endif

   }
#endif
}

static void
SiS_CheckScaling(struct SiS_Private *SiS_Pr, unsigned short resinfo,
			const unsigned char *nonscalingmodes)
{
   int i = 0;
   while(nonscalingmodes[i] != 0xff) {
      if(nonscalingmodes[i++] == resinfo) {
	 if((SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) ||
	    (SiS_Pr->UsePanelScaler == -1)) {
	    SiS_Pr->SiS_LCDInfo |= DontExpandLCD;
	 }
	 break;
      }
   }
}

void
SiS_GetLCDResInfo(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex)
{
  unsigned short temp,modeflag,resinfo=0,modexres=0,modeyres=0;
  bool panelcanscale = false;
#ifdef SIS300
  unsigned char *ROMAddr = SiS_Pr->VirtualRomBase;
  static const unsigned char SiS300SeriesLCDRes[] =
          { 0,  1,  2,  3,  7,  4,  5,  8,
	    0,  0, 10,  0,  0,  0,  0, 15 };
#endif
#ifdef SIS315H
  unsigned char   *myptr = NULL;
#endif

  SiS_Pr->SiS_LCDResInfo  = 0;
  SiS_Pr->SiS_LCDTypeInfo = 0;
  SiS_Pr->SiS_LCDInfo     = 0;
  SiS_Pr->PanelHRS        = 999; /* HSync start */
  SiS_Pr->PanelHRE        = 999; /* HSync end */
  SiS_Pr->PanelVRS        = 999; /* VSync start */
  SiS_Pr->PanelVRE        = 999; /* VSync end */
  SiS_Pr->SiS_NeedRomModeData = false;

  /* Alternative 1600x1200@60 timing for 1600x1200 LCDA */
  SiS_Pr->Alternate1600x1200 = false;

  if(!(SiS_Pr->SiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToLCDA))) return;

  modeflag = SiS_GetModeFlag(SiS_Pr, ModeNo, ModeIdIndex);

  if((ModeNo > 0x13) && (!SiS_Pr->UseCustomMode)) {
     resinfo = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;
     modexres = SiS_Pr->SiS_ModeResInfo[resinfo].HTotal;
     modeyres = SiS_Pr->SiS_ModeResInfo[resinfo].VTotal;
  }

  temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x36);

  /* For broken BIOSes: Assume 1024x768 */
  if(temp == 0) temp = 0x02;

  if((SiS_Pr->ChipType >= SIS_661) || (SiS_Pr->SiS_ROMNew)) {
     SiS_Pr->SiS_LCDTypeInfo = (SiS_GetReg(SiS_Pr->SiS_P3d4,0x39) & 0x7c) >> 2;
  } else if((SiS_Pr->ChipType < SIS_315H) || (SiS_Pr->ChipType >= SIS_661)) {
     SiS_Pr->SiS_LCDTypeInfo = temp >> 4;
  } else {
     SiS_Pr->SiS_LCDTypeInfo = (temp & 0x0F) - 1;
  }
  temp &= 0x0f;
#ifdef SIS300
  if(SiS_Pr->ChipType < SIS_315H) {
     /* Very old BIOSes only know 7 sizes (NetVista 2179, 1.01g) */
     if(SiS_Pr->SiS_VBType & VB_SIS301) {
        if(temp < 0x0f) temp &= 0x07;
     }
     /* Translate 300 series LCDRes to 315 series for unified usage */
     temp = SiS300SeriesLCDRes[temp];
  }
#endif

  /* Translate to our internal types */
#ifdef SIS315H
  if(SiS_Pr->ChipType == SIS_550) {
     if     (temp == Panel310_1152x768)  temp = Panel_320x240_2; /* Verified working */
     else if(temp == Panel310_320x240_2) temp = Panel_320x240_2;
     else if(temp == Panel310_320x240_3) temp = Panel_320x240_3;
  } else if(SiS_Pr->ChipType >= SIS_661) {
     if(temp == Panel661_1280x854)       temp = Panel_1280x854;
  }
#endif

  if(SiS_Pr->SiS_VBType & VB_SISLVDS) {		/* SiS LVDS */
     if(temp == Panel310_1280x768) {
        temp = Panel_1280x768_2;
     }
     if(SiS_Pr->SiS_ROMNew) {
	if(temp == Panel661_1280x800) {
	   temp = Panel_1280x800_2;
	}
     }
  }

  SiS_Pr->SiS_LCDResInfo = temp;

#ifdef SIS300
  if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
     if(SiS_Pr->SiS_CustomT == CUT_BARCO1366) {
	SiS_Pr->SiS_LCDResInfo = Panel_Barco1366;
     } else if(SiS_Pr->SiS_CustomT == CUT_PANEL848) {
	SiS_Pr->SiS_LCDResInfo = Panel_848x480;
     } else if(SiS_Pr->SiS_CustomT == CUT_PANEL856) {
	SiS_Pr->SiS_LCDResInfo = Panel_856x480;
     }
  }
#endif

  if(SiS_Pr->SiS_VBType & VB_SISVB) {
     if(SiS_Pr->SiS_LCDResInfo < SiS_Pr->SiS_PanelMin301)
	SiS_Pr->SiS_LCDResInfo = SiS_Pr->SiS_PanelMin301;
  } else {
     if(SiS_Pr->SiS_LCDResInfo < SiS_Pr->SiS_PanelMinLVDS)
	SiS_Pr->SiS_LCDResInfo = SiS_Pr->SiS_PanelMinLVDS;
  }

  temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x37);
  SiS_Pr->SiS_LCDInfo = temp & ~0x000e;
  /* Need temp below! */

  /* These must/can't scale no matter what */
  switch(SiS_Pr->SiS_LCDResInfo) {
  case Panel_320x240_1:
  case Panel_320x240_2:
  case Panel_320x240_3:
  case Panel_1280x960:
      SiS_Pr->SiS_LCDInfo &= ~DontExpandLCD;
      break;
  case Panel_640x480:
      SiS_Pr->SiS_LCDInfo |= DontExpandLCD;
  }

  panelcanscale = (bool)(SiS_Pr->SiS_LCDInfo & DontExpandLCD);

  if(!SiS_Pr->UsePanelScaler)          SiS_Pr->SiS_LCDInfo &= ~DontExpandLCD;
  else if(SiS_Pr->UsePanelScaler == 1) SiS_Pr->SiS_LCDInfo |= DontExpandLCD;

  /* Dual link, Pass 1:1 BIOS default, etc. */
#ifdef SIS315H
  if(SiS_Pr->ChipType >= SIS_661) {
     if(SiS_Pr->SiS_LCDInfo & DontExpandLCD) {
	if(temp & 0x08) SiS_Pr->SiS_LCDInfo |= LCDPass11;
     }
     if(SiS_Pr->SiS_VBType & VB_SISDUALLINK) {
	if(SiS_Pr->SiS_ROMNew) {
	   if(temp & 0x02) SiS_Pr->SiS_LCDInfo |= LCDDualLink;
	} else if((myptr = GetLCDStructPtr661(SiS_Pr))) {
	   if(myptr[2] & 0x01) SiS_Pr->SiS_LCDInfo |= LCDDualLink;
	}
     }
  } else if(SiS_Pr->ChipType >= SIS_315H) {
     if(SiS_Pr->SiS_LCDInfo & DontExpandLCD) {
	if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x39) & 0x01) SiS_Pr->SiS_LCDInfo |= LCDPass11;
     }
     if((SiS_Pr->SiS_ROMNew) && (!(SiS_Pr->PanelSelfDetected))) {
	SiS_Pr->SiS_LCDInfo &= ~(LCDRGB18Bit);
	temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x35);
	if(temp & 0x01) SiS_Pr->SiS_LCDInfo |= LCDRGB18Bit;
	if(SiS_Pr->SiS_VBType & VB_SISDUALLINK) {
	   if(temp & 0x02) SiS_Pr->SiS_LCDInfo |= LCDDualLink;
	}
     } else if(!(SiS_Pr->SiS_ROMNew)) {
	if(SiS_Pr->SiS_VBType & VB_SISDUALLINK) {
	   if((SiS_Pr->SiS_CustomT == CUT_CLEVO1024) &&
	      (SiS_Pr->SiS_LCDResInfo == Panel_1024x768)) {
	      SiS_Pr->SiS_LCDInfo |= LCDDualLink;
	   }
	   if((SiS_Pr->SiS_LCDResInfo == Panel_1280x1024) ||
	      (SiS_Pr->SiS_LCDResInfo == Panel_1400x1050) ||
	      (SiS_Pr->SiS_LCDResInfo == Panel_1600x1200) ||
	      (SiS_Pr->SiS_LCDResInfo == Panel_1680x1050)) {
	      SiS_Pr->SiS_LCDInfo |= LCDDualLink;
	   }
	}
     }
  }
#endif

  /* Pass 1:1 */
  if((SiS_Pr->SiS_IF_DEF_LVDS == 1) || (SiS_Pr->SiS_VBType & VB_NoLCD)) {
     /* Always center screen on LVDS (if scaling is disabled) */
     SiS_Pr->SiS_LCDInfo &= ~LCDPass11;
  } else if(SiS_Pr->SiS_VBType & VB_SISVB) {
     if(SiS_Pr->SiS_VBType & VB_SISLVDS) {
	/* Always center screen on SiS LVDS (if scaling is disabled) */
	SiS_Pr->SiS_LCDInfo &= ~LCDPass11;
     } else {
	/* By default, pass 1:1 on SiS TMDS (if scaling is supported) */
	if(panelcanscale)             SiS_Pr->SiS_LCDInfo |= LCDPass11;
	if(SiS_Pr->CenterScreen == 1) SiS_Pr->SiS_LCDInfo &= ~LCDPass11;
     }
  }

  SiS_Pr->PanelVCLKIdx300 = VCLK65_300;
  SiS_Pr->PanelVCLKIdx315 = VCLK108_2_315;

  switch(SiS_Pr->SiS_LCDResInfo) {
     case Panel_320x240_1:
     case Panel_320x240_2:
     case Panel_320x240_3:  SiS_Pr->PanelXRes =  640; SiS_Pr->PanelYRes =  480;
			    SiS_Pr->PanelVRS  =   24; SiS_Pr->PanelVRE  =    3;
			    SiS_Pr->PanelVCLKIdx300 = VCLK28;
			    SiS_Pr->PanelVCLKIdx315 = VCLK28;
			    break;
     case Panel_640x480:    SiS_Pr->PanelXRes =  640; SiS_Pr->PanelYRes =  480;
						      SiS_Pr->PanelVRE  =    3;
			    SiS_Pr->PanelVCLKIdx300 = VCLK28;
			    SiS_Pr->PanelVCLKIdx315 = VCLK28;
			    break;
     case Panel_800x600:    SiS_Pr->PanelXRes =  800; SiS_Pr->PanelYRes =  600;
     			    SiS_Pr->PanelHT   = 1056; SiS_Pr->PanelVT   =  628;
			    SiS_Pr->PanelHRS  =   40; SiS_Pr->PanelHRE  =  128;
			    SiS_Pr->PanelVRS  =    1; SiS_Pr->PanelVRE  =    4;
			    SiS_Pr->PanelVCLKIdx300 = VCLK40;
			    SiS_Pr->PanelVCLKIdx315 = VCLK40;
			    break;
     case Panel_1024x600:   SiS_Pr->PanelXRes = 1024; SiS_Pr->PanelYRes =  600;
			    SiS_Pr->PanelHT   = 1344; SiS_Pr->PanelVT   =  800;
			    SiS_Pr->PanelHRS  =   24; SiS_Pr->PanelHRE  =  136;
			    SiS_Pr->PanelVRS  =    2 /* 88 */ ; SiS_Pr->PanelVRE  =    6;
			    SiS_Pr->PanelVCLKIdx300 = VCLK65_300;
			    SiS_Pr->PanelVCLKIdx315 = VCLK65_315;
			    break;
     case Panel_1024x768:   SiS_Pr->PanelXRes = 1024; SiS_Pr->PanelYRes =  768;
			    SiS_Pr->PanelHT   = 1344; SiS_Pr->PanelVT   =  806;
			    SiS_Pr->PanelHRS  =   24; SiS_Pr->PanelHRE  =  136;
			    SiS_Pr->PanelVRS  =    3; SiS_Pr->PanelVRE  =    6;
			    if(SiS_Pr->ChipType < SIS_315H) {
			       SiS_Pr->PanelHRS = 23;
						      SiS_Pr->PanelVRE  =    5;
			    }
			    SiS_Pr->PanelVCLKIdx300 = VCLK65_300;
			    SiS_Pr->PanelVCLKIdx315 = VCLK65_315;
			    SiS_GetLCDInfoBIOS(SiS_Pr);
			    break;
     case Panel_1152x768:   SiS_Pr->PanelXRes = 1152; SiS_Pr->PanelYRes =  768;
			    SiS_Pr->PanelHT   = 1344; SiS_Pr->PanelVT   =  806;
			    SiS_Pr->PanelHRS  =   24; SiS_Pr->PanelHRE  =  136;
			    SiS_Pr->PanelVRS  =    3; SiS_Pr->PanelVRE  =    6;
			    if(SiS_Pr->ChipType < SIS_315H) {
			       SiS_Pr->PanelHRS = 23;
						      SiS_Pr->PanelVRE  =    5;
			    }
			    SiS_Pr->PanelVCLKIdx300 = VCLK65_300;
			    SiS_Pr->PanelVCLKIdx315 = VCLK65_315;
			    break;
     case Panel_1152x864:   SiS_Pr->PanelXRes = 1152; SiS_Pr->PanelYRes =  864;
			    break;
     case Panel_1280x720:   SiS_Pr->PanelXRes = 1280; SiS_Pr->PanelYRes =  720;
			    SiS_Pr->PanelHT   = 1650; SiS_Pr->PanelVT   =  750;
			    SiS_Pr->PanelHRS  =  110; SiS_Pr->PanelHRE  =   40;
			    SiS_Pr->PanelVRS  =    5; SiS_Pr->PanelVRE  =    5;
			    SiS_Pr->PanelVCLKIdx315 = VCLK_1280x720;
			    /* Data above for TMDS (projector); get from BIOS for LVDS */
			    SiS_GetLCDInfoBIOS(SiS_Pr);
			    break;
     case Panel_1280x768:   SiS_Pr->PanelXRes = 1280; SiS_Pr->PanelYRes =  768;
			    if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
			       SiS_Pr->PanelHT   = 1408; SiS_Pr->PanelVT   =  806;
			       SiS_Pr->PanelVCLKIdx300 = VCLK81_300; /* ? */
			       SiS_Pr->PanelVCLKIdx315 = VCLK81_315; /* ? */
			    } else {
			       SiS_Pr->PanelHT   = 1688; SiS_Pr->PanelVT   =  802;
			       SiS_Pr->PanelHRS  =   48; SiS_Pr->PanelHRS  =  112;
			       SiS_Pr->PanelVRS  =    3; SiS_Pr->PanelVRE  =    6;
			       SiS_Pr->PanelVCLKIdx300 = VCLK81_300;
			       SiS_Pr->PanelVCLKIdx315 = VCLK81_315;
			    }
			    break;
     case Panel_1280x768_2: SiS_Pr->PanelXRes = 1280; SiS_Pr->PanelYRes =  768;
			    SiS_Pr->PanelHT   = 1660; SiS_Pr->PanelVT   =  806;
			    SiS_Pr->PanelHRS  =   48; SiS_Pr->PanelHRE  =  112;
			    SiS_Pr->PanelVRS  =    3; SiS_Pr->PanelVRE  =    6;
			    SiS_Pr->PanelVCLKIdx315 = VCLK_1280x768_2;
			    SiS_GetLCDInfoBIOS(SiS_Pr);
			    break;
     case Panel_1280x800:   SiS_Pr->PanelXRes = 1280; SiS_Pr->PanelYRes =  800;
			    SiS_Pr->PanelHT   = 1408; SiS_Pr->PanelVT   =  816;
			    SiS_Pr->PanelHRS   =  21; SiS_Pr->PanelHRE  =   24;
			    SiS_Pr->PanelVRS   =   4; SiS_Pr->PanelVRE  =    3;
			    SiS_Pr->PanelVCLKIdx315 = VCLK_1280x800_315;
			    SiS_GetLCDInfoBIOS(SiS_Pr);
			    break;
     case Panel_1280x800_2: SiS_Pr->PanelXRes = 1280; SiS_Pr->PanelYRes =  800;
			    SiS_Pr->PanelHT   = 1552; SiS_Pr->PanelVT   =  812;
			    SiS_Pr->PanelHRS   =  48; SiS_Pr->PanelHRE  =  112;
			    SiS_Pr->PanelVRS   =   4; SiS_Pr->PanelVRE  =    3;
			    SiS_Pr->PanelVCLKIdx315 = VCLK_1280x800_315_2;
			    SiS_GetLCDInfoBIOS(SiS_Pr);
			    break;
     case Panel_1280x854:   SiS_Pr->PanelXRes = 1280; SiS_Pr->PanelYRes =  854;
			    SiS_Pr->PanelHT   = 1664; SiS_Pr->PanelVT   =  861;
			    SiS_Pr->PanelHRS   =  16; SiS_Pr->PanelHRE  =  112;
			    SiS_Pr->PanelVRS   =   1; SiS_Pr->PanelVRE  =    3;
			    SiS_Pr->PanelVCLKIdx315 = VCLK_1280x854;
			    SiS_GetLCDInfoBIOS(SiS_Pr);
			    break;
     case Panel_1280x960:   SiS_Pr->PanelXRes = 1280; SiS_Pr->PanelYRes =  960;
			    SiS_Pr->PanelHT   = 1800; SiS_Pr->PanelVT   = 1000;
			    SiS_Pr->PanelVCLKIdx300 = VCLK108_3_300;
			    SiS_Pr->PanelVCLKIdx315 = VCLK108_3_315;
			    if(resinfo == SIS_RI_1280x1024) {
			       SiS_Pr->PanelVCLKIdx300 = VCLK100_300;
			       SiS_Pr->PanelVCLKIdx315 = VCLK100_315;
			    }
			    break;
     case Panel_1280x1024:  SiS_Pr->PanelXRes = 1280; SiS_Pr->PanelYRes = 1024;
			    SiS_Pr->PanelHT   = 1688; SiS_Pr->PanelVT   = 1066;
			    SiS_Pr->PanelHRS  =   48; SiS_Pr->PanelHRE  =  112;
			    SiS_Pr->PanelVRS  =    1; SiS_Pr->PanelVRE  =    3;
			    SiS_Pr->PanelVCLKIdx300 = VCLK108_3_300;
			    SiS_Pr->PanelVCLKIdx315 = VCLK108_2_315;
			    SiS_GetLCDInfoBIOS(SiS_Pr);
			    break;
     case Panel_1400x1050:  SiS_Pr->PanelXRes = 1400; SiS_Pr->PanelYRes = 1050;
			    SiS_Pr->PanelHT   = 1688; SiS_Pr->PanelVT   = 1066;
			    SiS_Pr->PanelHRS  =   48; SiS_Pr->PanelHRE  =  112;
			    SiS_Pr->PanelVRS  =    1; SiS_Pr->PanelVRE  =    3;
			    SiS_Pr->PanelVCLKIdx315 = VCLK108_2_315;
			    SiS_GetLCDInfoBIOS(SiS_Pr);
			    break;
     case Panel_1600x1200:  SiS_Pr->PanelXRes = 1600; SiS_Pr->PanelYRes = 1200;
			    SiS_Pr->PanelHT   = 2160; SiS_Pr->PanelVT   = 1250;
			    SiS_Pr->PanelHRS  =   64; SiS_Pr->PanelHRE  =  192;
			    SiS_Pr->PanelVRS  =    1; SiS_Pr->PanelVRE  =    3;
			    SiS_Pr->PanelVCLKIdx315 = VCLK162_315;
			    if(SiS_Pr->SiS_VBType & VB_SISTMDSLCDA) {
			       if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA) {
				  SiS_Pr->PanelHT  = 1760; SiS_Pr->PanelVT  = 1235;
				  SiS_Pr->PanelHRS =   48; SiS_Pr->PanelHRE =   32;
				  SiS_Pr->PanelVRS =    2; SiS_Pr->PanelVRE =    4;
				  SiS_Pr->PanelVCLKIdx315 = VCLK130_315;
				  SiS_Pr->Alternate1600x1200 = true;
			       }
			    } else if(SiS_Pr->SiS_IF_DEF_LVDS) {
			       SiS_Pr->PanelHT  = 2048; SiS_Pr->PanelVT  = 1320;
			       SiS_Pr->PanelHRS = SiS_Pr->PanelHRE = 999;
			       SiS_Pr->PanelVRS = SiS_Pr->PanelVRE = 999;
			    }
			    SiS_GetLCDInfoBIOS(SiS_Pr);
			    break;
     case Panel_1680x1050:  SiS_Pr->PanelXRes = 1680; SiS_Pr->PanelYRes = 1050;
			    SiS_Pr->PanelHT   = 1900; SiS_Pr->PanelVT   = 1066;
			    SiS_Pr->PanelHRS  =   26; SiS_Pr->PanelHRE  =   76;
			    SiS_Pr->PanelVRS  =    3; SiS_Pr->PanelVRE  =    6;
			    SiS_Pr->PanelVCLKIdx315 = VCLK121_315;
			    SiS_GetLCDInfoBIOS(SiS_Pr);
			    break;
     case Panel_Barco1366:  SiS_Pr->PanelXRes = 1360; SiS_Pr->PanelYRes = 1024;
			    SiS_Pr->PanelHT   = 1688; SiS_Pr->PanelVT   = 1066;
			    break;
     case Panel_848x480:    SiS_Pr->PanelXRes =  848; SiS_Pr->PanelYRes =  480;
			    SiS_Pr->PanelHT   = 1088; SiS_Pr->PanelVT   =  525;
			    break;
     case Panel_856x480:    SiS_Pr->PanelXRes =  856; SiS_Pr->PanelYRes =  480;
			    SiS_Pr->PanelHT   = 1088; SiS_Pr->PanelVT   =  525;
			    break;
     case Panel_Custom:     SiS_Pr->PanelXRes = SiS_Pr->CP_MaxX;
			    SiS_Pr->PanelYRes = SiS_Pr->CP_MaxY;
			    SiS_Pr->PanelHT   = SiS_Pr->CHTotal;
			    SiS_Pr->PanelVT   = SiS_Pr->CVTotal;
			    if(SiS_Pr->CP_PreferredIndex != -1) {
			       SiS_Pr->PanelXRes = SiS_Pr->CP_HDisplay[SiS_Pr->CP_PreferredIndex];
			       SiS_Pr->PanelYRes = SiS_Pr->CP_VDisplay[SiS_Pr->CP_PreferredIndex];
			       SiS_Pr->PanelHT   = SiS_Pr->CP_HTotal[SiS_Pr->CP_PreferredIndex];
			       SiS_Pr->PanelVT   = SiS_Pr->CP_VTotal[SiS_Pr->CP_PreferredIndex];
			       SiS_Pr->PanelHRS  = SiS_Pr->CP_HSyncStart[SiS_Pr->CP_PreferredIndex];
			       SiS_Pr->PanelHRE  = SiS_Pr->CP_HSyncEnd[SiS_Pr->CP_PreferredIndex];
			       SiS_Pr->PanelVRS  = SiS_Pr->CP_VSyncStart[SiS_Pr->CP_PreferredIndex];
			       SiS_Pr->PanelVRE  = SiS_Pr->CP_VSyncEnd[SiS_Pr->CP_PreferredIndex];
			       SiS_Pr->PanelHRS -= SiS_Pr->PanelXRes;
			       SiS_Pr->PanelHRE -= SiS_Pr->PanelHRS;
			       SiS_Pr->PanelVRS -= SiS_Pr->PanelYRes;
			       SiS_Pr->PanelVRE -= SiS_Pr->PanelVRS;
			       if(SiS_Pr->CP_PrefClock) {
				  int idx;
				  SiS_Pr->PanelVCLKIdx315 = VCLK_CUSTOM_315;
				  SiS_Pr->PanelVCLKIdx300 = VCLK_CUSTOM_300;
				  if(SiS_Pr->ChipType < SIS_315H) idx = VCLK_CUSTOM_300;
				  else				   idx = VCLK_CUSTOM_315;
				  SiS_Pr->SiS_VCLKData[idx].CLOCK =
				     SiS_Pr->SiS_VBVCLKData[idx].CLOCK = SiS_Pr->CP_PrefClock;
				  SiS_Pr->SiS_VCLKData[idx].SR2B =
				     SiS_Pr->SiS_VBVCLKData[idx].Part4_A = SiS_Pr->CP_PrefSR2B;
				  SiS_Pr->SiS_VCLKData[idx].SR2C =
				     SiS_Pr->SiS_VBVCLKData[idx].Part4_B = SiS_Pr->CP_PrefSR2C;
			       }
			    }
			    break;
     default:		    SiS_Pr->PanelXRes = 1024; SiS_Pr->PanelYRes =  768;
			    SiS_Pr->PanelHT   = 1344; SiS_Pr->PanelVT   =  806;
			    break;
  }

  /* Special cases */
  if( (SiS_Pr->SiS_IF_DEF_FSTN)              ||
      (SiS_Pr->SiS_IF_DEF_DSTN)              ||
      (SiS_Pr->SiS_CustomT == CUT_BARCO1366) ||
      (SiS_Pr->SiS_CustomT == CUT_BARCO1024) ||
      (SiS_Pr->SiS_CustomT == CUT_PANEL848)  ||
      (SiS_Pr->SiS_CustomT == CUT_PANEL856) ) {
     SiS_Pr->PanelHRS = 999;
     SiS_Pr->PanelHRE = 999;
  }

  if( (SiS_Pr->SiS_CustomT == CUT_BARCO1366) ||
      (SiS_Pr->SiS_CustomT == CUT_BARCO1024) ||
      (SiS_Pr->SiS_CustomT == CUT_PANEL848)  ||
      (SiS_Pr->SiS_CustomT == CUT_PANEL856) ) {
     SiS_Pr->PanelVRS = 999;
     SiS_Pr->PanelVRE = 999;
  }

  /* DontExpand overrule */
  if((SiS_Pr->SiS_VBType & VB_SISVB) && (!(SiS_Pr->SiS_VBType & VB_NoLCD))) {

     if((SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) && (modeflag & NoSupportLCDScale)) {
	/* No scaling for this mode on any panel (LCD=CRT2)*/
	SiS_Pr->SiS_LCDInfo |= DontExpandLCD;
     }

     switch(SiS_Pr->SiS_LCDResInfo) {

     case Panel_Custom:
     case Panel_1152x864:
     case Panel_1280x768:	/* TMDS only */
	SiS_Pr->SiS_LCDInfo |= DontExpandLCD;
	break;

     case Panel_800x600: {
	static const unsigned char nonscalingmodes[] = {
	   SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x576, SIS_RI_800x480, 0xff
	};
	SiS_CheckScaling(SiS_Pr, resinfo, nonscalingmodes);
	break;
     }
     case Panel_1024x768: {
	static const unsigned char nonscalingmodes[] = {
	   SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x576, SIS_RI_800x480, SIS_RI_848x480,
	   SIS_RI_856x480, SIS_RI_960x540, SIS_RI_960x600, SIS_RI_1024x576,SIS_RI_1024x600,
	   0xff
	};
	SiS_CheckScaling(SiS_Pr, resinfo, nonscalingmodes);
	break;
     }
     case Panel_1280x720: {
	static const unsigned char nonscalingmodes[] = {
	   SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x576, SIS_RI_800x480, SIS_RI_848x480,
	   SIS_RI_856x480, SIS_RI_960x540, SIS_RI_960x600, SIS_RI_1024x576,SIS_RI_1024x600,
	   0xff
	};
	SiS_CheckScaling(SiS_Pr, resinfo, nonscalingmodes);
	if(SiS_Pr->PanelHT == 1650) {
	   SiS_Pr->SiS_LCDInfo |= DontExpandLCD;
	}
	break;
     }
     case Panel_1280x768_2: {  /* LVDS only */
	static const unsigned char nonscalingmodes[] = {
	   SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x576, SIS_RI_800x480, SIS_RI_848x480,
	   SIS_RI_856x480, SIS_RI_960x540, SIS_RI_960x600, SIS_RI_1024x576,SIS_RI_1024x600,
	   SIS_RI_1152x768,0xff
	};
	SiS_CheckScaling(SiS_Pr, resinfo, nonscalingmodes);
	switch(resinfo) {
	case SIS_RI_1280x720:  if(SiS_Pr->UsePanelScaler == -1) {
				  SiS_Pr->SiS_LCDInfo |= DontExpandLCD;
			       }
			       break;
	}
	break;
     }
     case Panel_1280x800: {  	/* SiS TMDS special (Averatec 6200 series) */
	static const unsigned char nonscalingmodes[] = {
	   SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x576, SIS_RI_800x480, SIS_RI_848x480,
	   SIS_RI_856x480, SIS_RI_960x540, SIS_RI_960x600, SIS_RI_1024x576,SIS_RI_1024x600,
	   SIS_RI_1152x768,SIS_RI_1280x720,SIS_RI_1280x768,0xff
	};
	SiS_CheckScaling(SiS_Pr, resinfo, nonscalingmodes);
	break;
     }
     case Panel_1280x800_2:  { 	/* SiS LVDS */
	static const unsigned char nonscalingmodes[] = {
	   SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x576, SIS_RI_800x480, SIS_RI_848x480,
	   SIS_RI_856x480, SIS_RI_960x540, SIS_RI_960x600, SIS_RI_1024x576,SIS_RI_1024x600,
	   SIS_RI_1152x768,0xff
	};
	SiS_CheckScaling(SiS_Pr, resinfo, nonscalingmodes);
	switch(resinfo) {
	case SIS_RI_1280x720:
	case SIS_RI_1280x768:  if(SiS_Pr->UsePanelScaler == -1) {
				  SiS_Pr->SiS_LCDInfo |= DontExpandLCD;
			       }
			       break;
	}
	break;
     }
     case Panel_1280x854: {  	/* SiS LVDS */
	static const unsigned char nonscalingmodes[] = {
	   SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x576, SIS_RI_800x480, SIS_RI_848x480,
	   SIS_RI_856x480, SIS_RI_960x540, SIS_RI_960x600, SIS_RI_1024x576,SIS_RI_1024x600,
	   SIS_RI_1152x768,0xff
	};
	SiS_CheckScaling(SiS_Pr, resinfo, nonscalingmodes);
	switch(resinfo) {
	case SIS_RI_1280x720:
	case SIS_RI_1280x768:
	case SIS_RI_1280x800:  if(SiS_Pr->UsePanelScaler == -1) {
				  SiS_Pr->SiS_LCDInfo |= DontExpandLCD;
			       }
			       break;
	}
	break;
     }
     case Panel_1280x960: {
	static const unsigned char nonscalingmodes[] = {
	   SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x576, SIS_RI_800x480, SIS_RI_848x480,
	   SIS_RI_856x480, SIS_RI_960x540, SIS_RI_960x600, SIS_RI_1024x576,SIS_RI_1024x600,
	   SIS_RI_1152x768,SIS_RI_1152x864,SIS_RI_1280x720,SIS_RI_1280x768,SIS_RI_1280x800,
	   SIS_RI_1280x854,0xff
	};
	SiS_CheckScaling(SiS_Pr, resinfo, nonscalingmodes);
	break;
     }
     case Panel_1280x1024: {
	static const unsigned char nonscalingmodes[] = {
	   SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x576, SIS_RI_800x480, SIS_RI_848x480,
	   SIS_RI_856x480, SIS_RI_960x540, SIS_RI_960x600, SIS_RI_1024x576,SIS_RI_1024x600,
	   SIS_RI_1152x768,SIS_RI_1152x864,SIS_RI_1280x720,SIS_RI_1280x768,SIS_RI_1280x800,
	   SIS_RI_1280x854,SIS_RI_1280x960,0xff
	};
	SiS_CheckScaling(SiS_Pr, resinfo, nonscalingmodes);
	break;
     }
     case Panel_1400x1050: {
	static const unsigned char nonscalingmodes[] = {
	     SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x576, SIS_RI_800x480, SIS_RI_848x480,
	     SIS_RI_856x480, SIS_RI_960x540, SIS_RI_960x600, SIS_RI_1024x576,SIS_RI_1024x600,
	     SIS_RI_1152x768,SIS_RI_1152x864,SIS_RI_1280x768,SIS_RI_1280x800,SIS_RI_1280x854,
	     SIS_RI_1280x960,0xff
	};
	SiS_CheckScaling(SiS_Pr, resinfo, nonscalingmodes);
	switch(resinfo) {
	case SIS_RI_1280x720:  if(SiS_Pr->UsePanelScaler == -1) {
				  SiS_Pr->SiS_LCDInfo |= DontExpandLCD;
			       }
			       break;
	case SIS_RI_1280x1024: SiS_Pr->SiS_LCDInfo |= DontExpandLCD;
			       break;
	}
	break;
     }
     case Panel_1600x1200: {
	static const unsigned char nonscalingmodes[] = {
	     SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x576, SIS_RI_800x480, SIS_RI_848x480,
	     SIS_RI_856x480, SIS_RI_960x540, SIS_RI_960x600, SIS_RI_1024x576,SIS_RI_1024x600,
	     SIS_RI_1152x768,SIS_RI_1152x864,SIS_RI_1280x720,SIS_RI_1280x768,SIS_RI_1280x800,
	     SIS_RI_1280x854,SIS_RI_1280x960,SIS_RI_1360x768,SIS_RI_1360x1024,0xff
	};
	SiS_CheckScaling(SiS_Pr, resinfo, nonscalingmodes);
	break;
     }
     case Panel_1680x1050: {
	static const unsigned char nonscalingmodes[] = {
	     SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x576, SIS_RI_800x480, SIS_RI_848x480,
	     SIS_RI_856x480, SIS_RI_960x540, SIS_RI_960x600, SIS_RI_1024x576,SIS_RI_1024x600,
	     SIS_RI_1152x768,SIS_RI_1152x864,SIS_RI_1280x854,SIS_RI_1280x960,SIS_RI_1360x768,
	     SIS_RI_1360x1024,0xff
	};
	SiS_CheckScaling(SiS_Pr, resinfo, nonscalingmodes);
	break;
     }
     }
  }

#ifdef SIS300
  if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
     if(SiS_Pr->SiS_CustomT == CUT_PANEL848 || SiS_Pr->SiS_CustomT == CUT_PANEL856) {
	SiS_Pr->SiS_LCDInfo = 0x80 | 0x40 | 0x20;   /* neg h/v sync, RGB24(D0 = 0) */
     }
  }

  if(SiS_Pr->ChipType < SIS_315H) {
     if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
	if(SiS_Pr->SiS_UseROM) {
	   if((ROMAddr[0x233] == 0x12) && (ROMAddr[0x234] == 0x34)) {
	      if(!(ROMAddr[0x235] & 0x02)) {
		 SiS_Pr->SiS_LCDInfo &= (~DontExpandLCD);
	      }
	   }
	}
     } else if(SiS_Pr->SiS_VBType & VB_SIS30xBLV) {
	if((SiS_Pr->SiS_SetFlag & SetDOSMode) && ((ModeNo == 0x03) || (ModeNo == 0x10))) {
	   SiS_Pr->SiS_LCDInfo &= (~DontExpandLCD);
	}
     }
  }
#endif

  /* Special cases */

  if(modexres == SiS_Pr->PanelXRes && modeyres == SiS_Pr->PanelYRes) {
     SiS_Pr->SiS_LCDInfo &= ~LCDPass11;
  }

  if(SiS_Pr->SiS_IF_DEF_TRUMPION) {
     SiS_Pr->SiS_LCDInfo |= (DontExpandLCD | LCDPass11);
  }

  switch(SiS_Pr->SiS_LCDResInfo) {
  case Panel_640x480:
     SiS_Pr->SiS_LCDInfo |= (DontExpandLCD | LCDPass11);
     break;
  case Panel_1280x800:
     /* Don't pass 1:1 by default (TMDS special) */
     if(SiS_Pr->CenterScreen == -1) SiS_Pr->SiS_LCDInfo &= ~LCDPass11;
     break;
  case Panel_1280x960:
     SiS_Pr->SiS_LCDInfo &= ~LCDPass11;
     break;
  case Panel_Custom:
     if((!SiS_Pr->CP_PrefClock) ||
        (modexres > SiS_Pr->PanelXRes) || (modeyres > SiS_Pr->PanelYRes)) {
        SiS_Pr->SiS_LCDInfo |= LCDPass11;
     }
     break;
  }

  if((SiS_Pr->UseCustomMode) || (SiS_Pr->SiS_CustomT == CUT_UNKNOWNLCD)) {
     SiS_Pr->SiS_LCDInfo |= (DontExpandLCD | LCDPass11);
  }

  /* (In)validate LCDPass11 flag */
  if(!(SiS_Pr->SiS_LCDInfo & DontExpandLCD)) {
     SiS_Pr->SiS_LCDInfo &= ~LCDPass11;
  }

  /* LVDS DDA */
  if(!((SiS_Pr->ChipType < SIS_315H) && (SiS_Pr->SiS_SetFlag & SetDOSMode))) {

     if((SiS_Pr->SiS_IF_DEF_LVDS == 1) || (SiS_Pr->SiS_VBType & VB_NoLCD)) {
	if(SiS_Pr->SiS_IF_DEF_TRUMPION == 0) {
	   if(ModeNo == 0x12) {
	      if(SiS_Pr->SiS_LCDInfo & LCDPass11) {
		 SiS_Pr->SiS_SetFlag |= EnableLVDSDDA;
	      }
	   } else if(ModeNo > 0x13) {
	      if(SiS_Pr->SiS_LCDResInfo == Panel_1024x600) {
		 if(!(SiS_Pr->SiS_LCDInfo & DontExpandLCD)) {
		    if((resinfo == SIS_RI_800x600) || (resinfo == SIS_RI_400x300)) {
		       SiS_Pr->SiS_SetFlag |= EnableLVDSDDA;
		    }
		 }
	      }
	   }
	}
     }

     if(modeflag & HalfDCLK) {
	if(SiS_Pr->SiS_IF_DEF_TRUMPION == 1) {
	   SiS_Pr->SiS_SetFlag |= EnableLVDSDDA;
	} else if(SiS_Pr->SiS_LCDInfo & DontExpandLCD) {
	   SiS_Pr->SiS_SetFlag |= EnableLVDSDDA;
	} else if(SiS_Pr->SiS_LCDResInfo == Panel_640x480) {
	   SiS_Pr->SiS_SetFlag |= EnableLVDSDDA;
	} else if(ModeNo > 0x13) {
	   if(SiS_Pr->SiS_LCDResInfo == Panel_1024x768) {
	      if(resinfo == SIS_RI_512x384) SiS_Pr->SiS_SetFlag |= EnableLVDSDDA;
	   } else if(SiS_Pr->SiS_LCDResInfo == Panel_800x600) {
	      if(resinfo == SIS_RI_400x300) SiS_Pr->SiS_SetFlag |= EnableLVDSDDA;
	   }
	}
     }

  }

  /* VESA timing */
  if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
     if(SiS_Pr->SiS_VBInfo & SetNotSimuMode) {
	SiS_Pr->SiS_SetFlag |= LCDVESATiming;
     }
  } else {
     SiS_Pr->SiS_SetFlag |= LCDVESATiming;
  }

#ifdef SIS_LINUX_KERNEL
#if 0
  printk(KERN_DEBUG "sisfb: (LCDInfo=0x%04x LCDResInfo=0x%02x LCDTypeInfo=0x%02x)\n",
	SiS_Pr->SiS_LCDInfo, SiS_Pr->SiS_LCDResInfo, SiS_Pr->SiS_LCDTypeInfo);
#endif
#endif
#ifdef SIS_XORG_XF86
  xf86DrvMsgVerb(0, X_PROBED, 4,
	"(init301: LCDInfo=0x%04x LCDResInfo=0x%02x LCDTypeInfo=0x%02x SetFlag=0x%04x)\n",
	SiS_Pr->SiS_LCDInfo, SiS_Pr->SiS_LCDResInfo, SiS_Pr->SiS_LCDTypeInfo, SiS_Pr->SiS_SetFlag);
#endif
}

/*********************************************/
/*                 GET VCLK                  */
/*********************************************/

unsigned short
SiS_GetVCLK2Ptr(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex,
		unsigned short RefreshRateTableIndex)
{
  unsigned short CRT2Index, VCLKIndex = 0, VCLKIndexGEN = 0, VCLKIndexGENCRT = 0;
  unsigned short modeflag, resinfo, tempbx;
  const unsigned char *CHTVVCLKPtr = NULL;

  if(ModeNo <= 0x13) {
     modeflag = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
     resinfo = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ResInfo;
     CRT2Index = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC;
     VCLKIndexGEN = (SiS_GetRegByte((SiS_Pr->SiS_P3ca+0x02)) >> 2) & 0x03;
     VCLKIndexGENCRT = VCLKIndexGEN;
  } else {
     modeflag = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
     resinfo = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;
     CRT2Index = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC;
     VCLKIndexGEN = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_CRTVCLK;
     VCLKIndexGENCRT = SiS_GetRefCRTVCLK(SiS_Pr, RefreshRateTableIndex,
		(SiS_Pr->SiS_SetFlag & ProgrammingCRT2) ? SiS_Pr->SiS_UseWideCRT2 : SiS_Pr->SiS_UseWide);
  }

  if(SiS_Pr->SiS_VBType & VB_SISVB) {    /* 30x/B/LV */

     if(SiS_Pr->SiS_SetFlag & ProgrammingCRT2) {

	CRT2Index >>= 6;
	if(SiS_Pr->SiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToLCDA)) {      	/*  LCD */

	   if(SiS_Pr->ChipType < SIS_315H) {
	      VCLKIndex = SiS_Pr->PanelVCLKIdx300;
	      if((SiS_Pr->SiS_LCDInfo & DontExpandLCD) && (SiS_Pr->SiS_LCDInfo & LCDPass11)) {
		 VCLKIndex = VCLKIndexGEN;
	      }
	   } else {
	      VCLKIndex = SiS_Pr->PanelVCLKIdx315;
	      if((SiS_Pr->SiS_LCDInfo & DontExpandLCD) && (SiS_Pr->SiS_LCDInfo & LCDPass11)) {
		 switch(resinfo) {
		 /* Correct those whose IndexGEN doesn't match VBVCLK array */
		 case SIS_RI_720x480:  VCLKIndex = VCLK_720x480;  break;
		 case SIS_RI_720x576:  VCLKIndex = VCLK_720x576;  break;
		 case SIS_RI_768x576:  VCLKIndex = VCLK_768x576;  break;
		 case SIS_RI_848x480:  VCLKIndex = VCLK_848x480;  break;
		 case SIS_RI_856x480:  VCLKIndex = VCLK_856x480;  break;
		 case SIS_RI_800x480:  VCLKIndex = VCLK_800x480;  break;
		 case SIS_RI_1024x576: VCLKIndex = VCLK_1024x576; break;
		 case SIS_RI_1152x864: VCLKIndex = VCLK_1152x864; break;
		 case SIS_RI_1280x720: VCLKIndex = VCLK_1280x720; break;
		 case SIS_RI_1360x768: VCLKIndex = VCLK_1360x768; break;
		 default:              VCLKIndex = VCLKIndexGEN;
		 }

		 if(ModeNo <= 0x13) {
		    if(SiS_Pr->ChipType <= SIS_315PRO) {
		       if(SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC == 1) VCLKIndex = 0x42;
		    } else {
		       if(SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC == 1) VCLKIndex = 0x00;
		    }
		 }
		 if(SiS_Pr->ChipType <= SIS_315PRO) {
		    if(VCLKIndex == 0) VCLKIndex = 0x41;
		    if(VCLKIndex == 1) VCLKIndex = 0x43;
		    if(VCLKIndex == 4) VCLKIndex = 0x44;
		 }
	      }
	   }

	} else if(SiS_Pr->SiS_VBInfo & SetCRT2ToTV) {                 	/*  TV */

	   if(SiS_Pr->SiS_VBInfo & SetCRT2ToHiVision) {
	      if(SiS_Pr->SiS_TVMode & TVRPLLDIV2XO) 	   VCLKIndex = HiTVVCLKDIV2;
	      else                                  	   VCLKIndex = HiTVVCLK;
	      if(SiS_Pr->SiS_TVMode & TVSetTVSimuMode)     VCLKIndex = HiTVSimuVCLK;
	   } else if(SiS_Pr->SiS_TVMode & TVSetYPbPr750p)  VCLKIndex = YPbPr750pVCLK;
	   else if(SiS_Pr->SiS_TVMode & TVSetYPbPr525p)    VCLKIndex = TVVCLKDIV2;
	   else if(SiS_Pr->SiS_TVMode & TVRPLLDIV2XO)      VCLKIndex = TVVCLKDIV2;
	   else						   VCLKIndex = TVVCLK;

	   if(SiS_Pr->ChipType < SIS_315H) VCLKIndex += TVCLKBASE_300;
	   else				   VCLKIndex += TVCLKBASE_315;

	} else {							/* VGA2 */

	   VCLKIndex = VCLKIndexGENCRT;
	   if(SiS_Pr->ChipType < SIS_315H) {
	      if(ModeNo > 0x13) {
		 if( (SiS_Pr->ChipType == SIS_630) &&
		     (SiS_Pr->ChipRevision >= 0x30)) {
		    if(VCLKIndex == 0x14) VCLKIndex = 0x34;
		 }
		 /* Better VGA2 clock for 1280x1024@75 */
		 if(VCLKIndex == 0x17) VCLKIndex = 0x45;
	      }
	   }
	}

     } else {   /* If not programming CRT2 */

	VCLKIndex = VCLKIndexGENCRT;
	if(SiS_Pr->ChipType < SIS_315H) {
	   if(ModeNo > 0x13) {
	      if( (SiS_Pr->ChipType != SIS_630) &&
		  (SiS_Pr->ChipType != SIS_300) ) {
		 if(VCLKIndex == 0x1b) VCLKIndex = 0x48;
	      }
	   }
	}
     }

  } else {       /*   LVDS  */

     VCLKIndex = CRT2Index;

     if(SiS_Pr->SiS_SetFlag & ProgrammingCRT2) {

	if( (SiS_Pr->SiS_IF_DEF_CH70xx != 0) && (SiS_Pr->SiS_VBInfo & SetCRT2ToTV) ) {

	   VCLKIndex &= 0x1f;
	   tempbx = 0;
	   if(SiS_Pr->SiS_TVMode & TVSetCHOverScan) tempbx += 1;
	   if(SiS_Pr->SiS_TVMode & TVSetPAL) {
	      tempbx += 2;
	      if(SiS_Pr->SiS_ModeType > ModeVGA) {
		 if(SiS_Pr->SiS_CHSOverScan) tempbx = 8;
	      }
	      if(SiS_Pr->SiS_TVMode & TVSetPALM) {
		 tempbx = 4;
		 if(SiS_Pr->SiS_TVMode & TVSetCHOverScan) tempbx += 1;
	      } else if(SiS_Pr->SiS_TVMode & TVSetPALN) {
		 tempbx = 6;
		 if(SiS_Pr->SiS_TVMode & TVSetCHOverScan) tempbx += 1;
	      }
	   }
	   switch(tempbx) {
	     case  0: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKUNTSC;  break;
	     case  1: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKONTSC;  break;
	     case  2: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKUPAL;   break;
	     case  3: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKOPAL;   break;
	     case  4: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKUPALM;  break;
	     case  5: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKOPALM;  break;
	     case  6: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKUPALN;  break;
	     case  7: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKOPALN;  break;
	     case  8: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKSOPAL;  break;
	     default: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKOPAL;   break;
	   }
	   VCLKIndex = CHTVVCLKPtr[VCLKIndex];

	} else if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) {

	   if(SiS_Pr->ChipType < SIS_315H) {
	      VCLKIndex = SiS_Pr->PanelVCLKIdx300;
	   } else {
	      VCLKIndex = SiS_Pr->PanelVCLKIdx315;
	   }

#ifdef SIS300
	   /* Special Timing: Barco iQ Pro R series */
	   if(SiS_Pr->SiS_CustomT == CUT_BARCO1366) VCLKIndex = 0x44;

	   /* Special Timing: 848x480 and 856x480 parallel lvds panels */
	   if(SiS_Pr->SiS_CustomT == CUT_PANEL848 || SiS_Pr->SiS_CustomT == CUT_PANEL856) {
	      if(SiS_Pr->ChipType < SIS_315H) {
		 VCLKIndex = VCLK34_300;
		 /* if(resinfo == SIS_RI_1360x768) VCLKIndex = ?; */
	      } else {
		 VCLKIndex = VCLK34_315;
		 /* if(resinfo == SIS_RI_1360x768) VCLKIndex = ?; */
	      }
	   }
#endif

	} else {

	   VCLKIndex = VCLKIndexGENCRT;
	   if(SiS_Pr->ChipType < SIS_315H) {
	      if(ModeNo > 0x13) {
		 if( (SiS_Pr->ChipType == SIS_630) &&
		     (SiS_Pr->ChipRevision >= 0x30) ) {
		    if(VCLKIndex == 0x14) VCLKIndex = 0x2e;
		 }
	      }
	   }
	}

     } else {  /* if not programming CRT2 */

	VCLKIndex = VCLKIndexGENCRT;
	if(SiS_Pr->ChipType < SIS_315H) {
	   if(ModeNo > 0x13) {
	      if( (SiS_Pr->ChipType != SIS_630) &&
		  (SiS_Pr->ChipType != SIS_300) ) {
		 if(VCLKIndex == 0x1b) VCLKIndex = 0x48;
	      }
#if 0
	      if(SiS_Pr->ChipType == SIS_730) {
		 if(VCLKIndex == 0x0b) VCLKIndex = 0x40;   /* 1024x768-70 */
		 if(VCLKIndex == 0x0d) VCLKIndex = 0x41;   /* 1024x768-75 */
	      }
#endif
	   }
        }

     }

  }

#ifdef SIS_XORG_XF86
#ifdef TWDEBUG
  xf86DrvMsg(0, X_INFO, "VCLKIndex %d (0x%x)\n", VCLKIndex, VCLKIndex);
#endif
#endif

  return VCLKIndex;
}

/*********************************************/
/*        SET CRT2 MODE TYPE REGISTERS       */
/*********************************************/

static void
SiS_SetCRT2ModeRegs(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex)
{
  unsigned short i, j, modeflag, tempah=0;
  short tempcl;
#if defined(SIS300) || defined(SIS315H)
  unsigned short tempbl;
#endif
#ifdef SIS315H
  unsigned char  *ROMAddr = SiS_Pr->VirtualRomBase;
  unsigned short tempah2, tempbl2;
#endif

  modeflag = SiS_GetModeFlag(SiS_Pr, ModeNo, ModeIdIndex);

  if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA) {

     SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x00,0xAF,0x40);
     SiS_SetRegAND(SiS_Pr->SiS_Part1Port,0x2E,0xF7);

  } else {

     for(i=0,j=4; i<3; i++,j++) SiS_SetReg(SiS_Pr->SiS_Part1Port,j,0);
     if(SiS_Pr->ChipType >= SIS_315H) {
        SiS_SetRegAND(SiS_Pr->SiS_Part1Port,0x02,0x7F);
     }

     tempcl = SiS_Pr->SiS_ModeType;

     if(SiS_Pr->ChipType < SIS_315H) {

#ifdef SIS300    /* ---- 300 series ---- */

	/* For 301BDH: (with LCD via LVDS) */
	if(SiS_Pr->SiS_VBType & VB_NoLCD) {
	   tempbl = SiS_GetReg(SiS_Pr->SiS_P3c4,0x32);
	   tempbl &= 0xef;
	   tempbl |= 0x02;
	   if((SiS_Pr->SiS_VBInfo & SetCRT2ToTV) || (SiS_Pr->SiS_VBInfo & SetCRT2ToRAMDAC)) {
	      tempbl |= 0x10;
	      tempbl &= 0xfd;
	   }
	   SiS_SetReg(SiS_Pr->SiS_P3c4,0x32,tempbl);
	}

	if(ModeNo > 0x13) {
	   tempcl -= ModeVGA;
	   if(tempcl >= 0) {
	      tempah = ((0x10 >> tempcl) | 0x80);
	   }
	} else tempah = 0x80;

	if(SiS_Pr->SiS_VBInfo & SetInSlaveMode)  tempah ^= 0xA0;

#endif  /* SIS300 */

     } else {

#ifdef SIS315H    /* ------- 315/330 series ------ */

	if(ModeNo > 0x13) {
	   tempcl -= ModeVGA;
	   if(tempcl >= 0) {
	      tempah = (0x08 >> tempcl);
	      if (tempah == 0) tempah = 1;
	      tempah |= 0x40;
	   }
	} else tempah = 0x40;

	if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) tempah ^= 0x50;

#endif  /* SIS315H */

     }

     if(SiS_Pr->SiS_VBInfo & DisableCRT2Display) tempah = 0;

     if(SiS_Pr->ChipType < SIS_315H) {
	SiS_SetReg(SiS_Pr->SiS_Part1Port,0x00,tempah);
     } else {
#ifdef SIS315H
	if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x00,0xa0,tempah);
	} else if(SiS_Pr->SiS_VBType & VB_SISVB) {
	   if(IS_SIS740) {
	      SiS_SetReg(SiS_Pr->SiS_Part1Port,0x00,tempah);
	   } else {
	      SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x00,0xa0,tempah);
	   }
	}
#endif
     }

     if(SiS_Pr->SiS_VBType & VB_SISVB) {

	tempah = 0x01;
	if(!(SiS_Pr->SiS_VBInfo & SetInSlaveMode)) {
	   tempah |= 0x02;
	}
	if(!(SiS_Pr->SiS_VBInfo & SetCRT2ToRAMDAC)) {
	   tempah ^= 0x05;
	   if(!(SiS_Pr->SiS_VBInfo & SetCRT2ToLCD)) {
	      tempah ^= 0x01;
	   }
	}

	if(SiS_Pr->ChipType < SIS_315H) {

	   if(SiS_Pr->SiS_VBInfo & DisableCRT2Display)  tempah = 0;

	   tempah = (tempah << 5) & 0xFF;
	   SiS_SetReg(SiS_Pr->SiS_Part1Port,0x01,tempah);
	   tempah = (tempah >> 5) & 0xFF;

	} else {

	   if(SiS_Pr->SiS_VBInfo & DisableCRT2Display)  tempah = 0x08;
	   else if(!(SiS_IsDualEdge(SiS_Pr)))           tempah |= 0x08;
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x2E,0xF0,tempah);
	   tempah &= ~0x08;

	}

	if((SiS_Pr->SiS_ModeType == ModeVGA) && (!(SiS_Pr->SiS_VBInfo & SetInSlaveMode))) {
	   tempah |= 0x10;
	}

	tempah |= 0x80;
	if(SiS_Pr->SiS_VBType & VB_SIS301) {
	   if(SiS_Pr->PanelXRes < 1280 && SiS_Pr->PanelYRes < 960) tempah &= ~0x80;
	}

	if(SiS_Pr->SiS_VBInfo & SetCRT2ToTV) {
	   if(!(SiS_Pr->SiS_TVMode & (TVSetYPbPr750p | TVSetYPbPr525p))) {
	      if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
		 tempah |= 0x20;
	      }
	   }
	}

	SiS_SetRegANDOR(SiS_Pr->SiS_Part4Port,0x0D,0x40,tempah);

	tempah = 0x80;
	if(SiS_Pr->SiS_VBType & VB_SIS301) {
	   if(SiS_Pr->PanelXRes < 1280 && SiS_Pr->PanelYRes < 960) tempah = 0;
	}

	if(SiS_IsDualLink(SiS_Pr)) tempah |= 0x40;

	if(SiS_Pr->SiS_VBInfo & SetCRT2ToTV) {
	   if(SiS_Pr->SiS_TVMode & TVRPLLDIV2XO) {
	      tempah |= 0x40;
	   }
	}

	SiS_SetReg(SiS_Pr->SiS_Part4Port,0x0C,tempah);

     } else {  /* LVDS */

	if(SiS_Pr->ChipType >= SIS_315H) {

#ifdef SIS315H
	   /* LVDS can only be slave in 8bpp modes */
	   tempah = 0x80;
	   if((modeflag & CRT2Mode) && (SiS_Pr->SiS_ModeType > ModeVGA)) {
	      if(SiS_Pr->SiS_VBInfo & DriverMode) {
	         tempah |= 0x02;
	      }
	   }

	   if(!(SiS_Pr->SiS_VBInfo & SetInSlaveMode))  tempah |= 0x02;

	   if(SiS_Pr->SiS_VBInfo & SetCRT2ToTV)        tempah ^= 0x01;

	   if(SiS_Pr->SiS_VBInfo & DisableCRT2Display) tempah = 1;

	   SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x2e,0xF0,tempah);
#endif

	} else {

#ifdef SIS300
	   tempah = 0;
	   if( (!(SiS_Pr->SiS_VBInfo & SetInSlaveMode)) && (SiS_Pr->SiS_ModeType > ModeVGA) ) {
	      tempah |= 0x02;
	   }
	   tempah <<= 5;

	   if(SiS_Pr->SiS_VBInfo & DisableCRT2Display) tempah = 0;

	   SiS_SetReg(SiS_Pr->SiS_Part1Port,0x01,tempah);
#endif

	}

     }

  }  /* LCDA */

  if(SiS_Pr->SiS_VBType & VB_SISVB) {

     if(SiS_Pr->ChipType >= SIS_315H) {

#ifdef SIS315H
	/* unsigned char bridgerev = SiS_GetReg(SiS_Pr->SiS_Part4Port,0x01); */

	/* The following is nearly unpreditable and varies from machine
	 * to machine. Especially the 301DH seems to be a real trouble
	 * maker. Some BIOSes simply set the registers (like in the
	 * NoLCD-if-statements here), some set them according to the
	 * LCDA stuff. It is very likely that some machines are not
	 * treated correctly in the following, very case-orientated
	 * code. What do I do then...?
	 */

	/* 740 variants match for 30xB, 301B-DH, 30xLV */

	if(!(IS_SIS740)) {
	   tempah = 0x04;						   /* For all bridges */
	   tempbl = 0xfb;
	   if(!(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA)) {
	      tempah = 0x00;
	      if(SiS_IsDualEdge(SiS_Pr)) {
	         tempbl = 0xff;
	      }
	   }
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x13,tempbl,tempah);
	}

	/* The following two are responsible for eventually wrong colors
	 * in TV output. The DH (VB_NoLCD) conditions are unknown; the
	 * b0 was found in some 651 machine (Pim; P4_23=0xe5); the b1 version
	 * in a 650 box (Jake). What is the criteria?
	 * Addendum: Another combination 651+301B-DH(b1) (Rapo) needs same
	 * treatment like the 651+301B-DH(b0) case. Seems more to be the
	 * chipset than the bridge revision.
	 */

	if((IS_SIS740) || (SiS_Pr->ChipType >= SIS_661) || (SiS_Pr->SiS_ROMNew)) {
	   tempah = 0x30;
	   tempbl = 0xc0;
	   if((SiS_Pr->SiS_VBInfo & DisableCRT2Display) ||
	      ((SiS_Pr->SiS_ROMNew) && (!(ROMAddr[0x5b] & 0x04)))) {
	      tempah = 0x00;
	      tempbl = 0x00;
	   }
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x2c,0xcf,tempah);
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part4Port,0x21,0x3f,tempbl);
	} else if(SiS_Pr->SiS_VBType & VB_SIS301) {
	   /* Fixes "TV-blue-bug" on 315+301 */
	   SiS_SetRegAND(SiS_Pr->SiS_Part1Port,0x2c,0xcf);	/* For 301   */
	   SiS_SetRegAND(SiS_Pr->SiS_Part4Port,0x21,0x3f);
	} else if(SiS_Pr->SiS_VBType & VB_SISLVDS) {
	   SiS_SetRegOR(SiS_Pr->SiS_Part1Port,0x2c,0x30);	/* For 30xLV */
	   SiS_SetRegOR(SiS_Pr->SiS_Part4Port,0x21,0xc0);
	} else if(SiS_Pr->SiS_VBType & VB_NoLCD) {		/* For 301B-DH */
	   tempah = 0x30; tempah2 = 0xc0;
	   tempbl = 0xcf; tempbl2 = 0x3f;
	   if(SiS_Pr->SiS_TVBlue == 0) {
	         tempah = tempah2 = 0x00;
	   } else if(SiS_Pr->SiS_TVBlue == -1) {
	      /* Set on 651/M650, clear on 315/650 */
	      if(!(IS_SIS65x)) /* (bridgerev != 0xb0) */ {
	         tempah = tempah2 = 0x00;
	      }
	   }
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x2c,tempbl,tempah);
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part4Port,0x21,tempbl2,tempah2);
	} else {
	   tempah = 0x30; tempah2 = 0xc0;		       /* For 30xB, 301C */
	   tempbl = 0xcf; tempbl2 = 0x3f;
	   if(!(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA)) {
	      tempah = tempah2 = 0x00;
	      if(SiS_IsDualEdge(SiS_Pr)) {
		 tempbl = tempbl2 = 0xff;
	      }
	   }
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x2c,tempbl,tempah);
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part4Port,0x21,tempbl2,tempah2);
	}

	if(IS_SIS740) {
	   tempah = 0x80;
	   if(SiS_Pr->SiS_VBInfo & DisableCRT2Display) tempah = 0x00;
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part4Port,0x23,0x7f,tempah);
	} else {
	   tempah = 0x00;
	   tempbl = 0x7f;
	   if(!(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA)) {
	      tempbl = 0xff;
	      if(!(SiS_IsDualEdge(SiS_Pr))) tempah = 0x80;
	   }
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part4Port,0x23,tempbl,tempah);
	}

#endif /* SIS315H */

     } else if(SiS_Pr->SiS_VBType & VB_SIS30xBLV) {

#ifdef SIS300
	SiS_SetRegAND(SiS_Pr->SiS_Part4Port,0x21,0x3f);

	if((SiS_Pr->SiS_VBInfo & DisableCRT2Display) ||
	   ((SiS_Pr->SiS_VBType & VB_NoLCD) &&
	    (SiS_Pr->SiS_VBInfo & SetCRT2ToLCD))) {
	   SiS_SetRegAND(SiS_Pr->SiS_Part4Port,0x23,0x7F);
	} else {
	   SiS_SetRegOR(SiS_Pr->SiS_Part4Port,0x23,0x80);
	}
#endif

     }

     if(SiS_Pr->SiS_VBType & VB_SIS30xBLV) {
	SiS_SetRegOR(SiS_Pr->SiS_Part4Port,0x0D,0x80);
	if(SiS_Pr->SiS_VBType & VB_SIS30xCLV) {
	   SiS_SetRegOR(SiS_Pr->SiS_Part4Port,0x3A,0xC0);
        }
     }

  } else {  /* LVDS */

#ifdef SIS315H
     if(SiS_Pr->ChipType >= SIS_315H) {

	if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {

	   tempah = 0x04;
	   tempbl = 0xfb;
	   if(!(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA)) {
	      tempah = 0x00;
	      if(SiS_IsDualEdge(SiS_Pr)) tempbl = 0xff;
	   }
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x13,tempbl,tempah);

	   if(SiS_Pr->SiS_VBInfo & DisableCRT2Display) {
	      SiS_SetRegAND(SiS_Pr->SiS_Part1Port,0x13,0xfb);
	   }

	   SiS_SetRegOR(SiS_Pr->SiS_Part1Port,0x2c,0x30);

	} else if(SiS_Pr->ChipType == SIS_550) {

	   SiS_SetRegAND(SiS_Pr->SiS_Part1Port,0x13,0xfb);
	   SiS_SetRegOR(SiS_Pr->SiS_Part1Port,0x2c,0x30);

	}

     }
#endif

  }

}

/*********************************************/
/*            GET RESOLUTION DATA            */
/*********************************************/

unsigned short
SiS_GetResInfo(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex)
{
   if(ModeNo <= 0x13)
      return ((unsigned short)SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ResInfo);
   else
      return ((unsigned short)SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO);
}

static void
SiS_GetCRT2ResInfo(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex)
{
   unsigned short xres, yres, modeflag=0, resindex;

   if(SiS_Pr->UseCustomMode) {
      xres = SiS_Pr->CHDisplay;
      if(SiS_Pr->CModeFlag & HalfDCLK) xres <<= 1;
      SiS_Pr->SiS_VGAHDE = SiS_Pr->SiS_HDE = xres;
      /* DoubleScanMode-check done in CheckCalcCustomMode()! */
      SiS_Pr->SiS_VGAVDE = SiS_Pr->SiS_VDE = SiS_Pr->CVDisplay;
      return;
   }

   resindex = SiS_GetResInfo(SiS_Pr,ModeNo,ModeIdIndex);

   if(ModeNo <= 0x13) {
      xres = SiS_Pr->SiS_StResInfo[resindex].HTotal;
      yres = SiS_Pr->SiS_StResInfo[resindex].VTotal;
   } else {
      xres = SiS_Pr->SiS_ModeResInfo[resindex].HTotal;
      yres = SiS_Pr->SiS_ModeResInfo[resindex].VTotal;
      modeflag = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
   }

   if(!SiS_Pr->SiS_IF_DEF_DSTN && !SiS_Pr->SiS_IF_DEF_FSTN) {

      if((SiS_Pr->ChipType >= SIS_315H) && (SiS_Pr->SiS_IF_DEF_LVDS == 1)) {
	 if((ModeNo != 0x03) && (SiS_Pr->SiS_SetFlag & SetDOSMode)) {
	    if(yres == 350) yres = 400;
	 }
	 if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x3a) & 0x01) {
	    if(ModeNo == 0x12) yres = 400;
	 }
      }

      if(modeflag & HalfDCLK)       xres <<= 1;
      if(modeflag & DoubleScanMode) yres <<= 1;

   }

   if((SiS_Pr->SiS_VBType & VB_SISVB) && (!(SiS_Pr->SiS_VBType & VB_NoLCD))) {

      if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) {
	 switch(SiS_Pr->SiS_LCDResInfo) {
	   case Panel_1024x768:
	      if(!(SiS_Pr->SiS_SetFlag & LCDVESATiming)) {
		 if(!(SiS_Pr->SiS_LCDInfo & DontExpandLCD)) {
		    if(yres == 350) yres = 357;
		    if(yres == 400) yres = 420;
		    if(yres == 480) yres = 525;
		 }
	      }
	      break;
	   case Panel_1280x1024:
	      if(!(SiS_Pr->SiS_LCDInfo & DontExpandLCD)) {
		 /* BIOS bug - does this regardless of scaling */
		 if(yres == 400) yres = 405;
	      }
	      if(yres == 350) yres = 360;
	      if(SiS_Pr->SiS_SetFlag & LCDVESATiming) {
		 if(yres == 360) yres = 375;
	      }
	      break;
	   case Panel_1600x1200:
	      if(!(SiS_Pr->SiS_SetFlag & LCDVESATiming)) {
		 if(yres == 1024) yres = 1056;
	      }
	      break;
	 }
      }

   } else {

      if(SiS_Pr->SiS_VBType & VB_SISVB) {
	 if(SiS_Pr->SiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToHiVision)) {
	    if(xres == 720) xres = 640;
	 }
      } else if(xres == 720) xres = 640;

      if(SiS_Pr->SiS_SetFlag & SetDOSMode) {
	 yres = 400;
	 if(SiS_Pr->ChipType >= SIS_315H) {
	    if(SiS_GetReg(SiS_Pr->SiS_P3c4,0x17) & 0x80) yres = 480;
	 } else {
	    if(SiS_GetReg(SiS_Pr->SiS_P3c4,0x13) & 0x80) yres = 480;
	 }
	 if(SiS_Pr->SiS_IF_DEF_DSTN || SiS_Pr->SiS_IF_DEF_FSTN) yres = 480;
      }

   }
   SiS_Pr->SiS_VGAHDE = SiS_Pr->SiS_HDE = xres;
   SiS_Pr->SiS_VGAVDE = SiS_Pr->SiS_VDE = yres;
}

/*********************************************/
/*           GET CRT2 TIMING DATA            */
/*********************************************/

static void
SiS_GetCRT2Ptr(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex,
	       unsigned short RefreshRateTableIndex, unsigned short *CRT2Index,
	       unsigned short *ResIndex)
{
  unsigned short tempbx=0, tempal=0, resinfo=0;

  if(ModeNo <= 0x13) {
     tempal = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC;
  } else {
     tempal = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC;
     resinfo = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;
  }

  if((SiS_Pr->SiS_VBType & VB_SISVB) && (SiS_Pr->SiS_IF_DEF_LVDS == 0)) {

     if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) {                            /* LCD */

	tempbx = SiS_Pr->SiS_LCDResInfo;
	if(!(SiS_Pr->SiS_SetFlag & LCDVESATiming)) tempbx += 32;

	/* patch index */
	if(SiS_Pr->SiS_LCDResInfo == Panel_1680x1050) {
	   if     (resinfo == SIS_RI_1280x800)  tempal =  9;
	   else if(resinfo == SIS_RI_1400x1050) tempal = 11;
	} else if((SiS_Pr->SiS_LCDResInfo == Panel_1280x800) ||
		  (SiS_Pr->SiS_LCDResInfo == Panel_1280x800_2) ||
		  (SiS_Pr->SiS_LCDResInfo == Panel_1280x854)) {
	   if     (resinfo == SIS_RI_1280x768)  tempal =  9;
	}

	if(SiS_Pr->SiS_LCDInfo & DontExpandLCD) {
	   /* Pass 1:1 only (center-screen handled outside) */
	   /* This is never called for the panel's native resolution */
	   /* since Pass1:1 will not be set in this case */
	   tempbx = 100;
	   if(ModeNo >= 0x13) {
	      tempal = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC_NS;
	   }
	}

#ifdef SIS315H
	if(SiS_Pr->SiS_CustomT == CUT_COMPAQ1280) {
	   if(SiS_Pr->SiS_LCDResInfo == Panel_1280x1024) {
	      if(!(SiS_Pr->SiS_LCDInfo & DontExpandLCD)) {
		 tempbx = 200;
		 if(!(SiS_Pr->SiS_SetFlag & LCDVESATiming)) tempbx++;
	      }
	   }
	}
#endif

     } else {						  	/* TV */

	if(SiS_Pr->SiS_VBInfo & SetCRT2ToHiVision) {
	   /* if(SiS_Pr->SiS_VGAVDE > 480) SiS_Pr->SiS_TVMode &= (~TVSetTVSimuMode); */
	   tempbx = 2;
	   if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
	      tempbx = 13;
	      if(!(SiS_Pr->SiS_TVMode & TVSetTVSimuMode)) tempbx = 14;
	   }
	} else if(SiS_Pr->SiS_VBInfo & SetCRT2ToYPbPr525750) {
	   if(SiS_Pr->SiS_TVMode & TVSetYPbPr750p)	tempbx = 7;
	   else if(SiS_Pr->SiS_TVMode & TVSetYPbPr525p)	tempbx = 6;
	   else						tempbx = 5;
	   if(SiS_Pr->SiS_TVMode & TVSetTVSimuMode)	tempbx += 5;
	} else {
	   if(SiS_Pr->SiS_TVMode & TVSetPAL)		tempbx = 3;
	   else						tempbx = 4;
	   if(SiS_Pr->SiS_TVMode & TVSetTVSimuMode)	tempbx += 5;
	}

     }

     tempal &= 0x3F;

     if(ModeNo > 0x13) {
        if(SiS_Pr->SiS_VBInfo & SetCRT2ToTVNoHiVision) {
	   switch(resinfo) {
	   case SIS_RI_720x480:
	      tempal = 6;
	      if(SiS_Pr->SiS_TVMode & (TVSetPAL | TVSetPALN))	tempal = 9;
	      break;
	   case SIS_RI_720x576:
	   case SIS_RI_768x576:
	   case SIS_RI_1024x576: /* Not in NTSC or YPBPR mode (except 1080i)! */
	      tempal = 6;
	      if(SiS_Pr->SiS_VBInfo & SetCRT2ToYPbPr525750) {
		 if(SiS_Pr->SiS_TVMode & TVSetYPbPr750p)	tempal = 8;
	      }
	      break;
	   case SIS_RI_800x480:
	      tempal = 4;
	      break;
	   case SIS_RI_512x384:
	   case SIS_RI_1024x768:
	      tempal = 7;
	      if(SiS_Pr->SiS_VBInfo & SetCRT2ToYPbPr525750) {
		 if(SiS_Pr->SiS_TVMode & TVSetYPbPr525p)	tempal = 8;
	      }
	      break;
	   case SIS_RI_1280x720:
	      if(SiS_Pr->SiS_VBInfo & SetCRT2ToYPbPr525750) {
		 if(SiS_Pr->SiS_TVMode & TVSetYPbPr750p)	tempal = 9;
	      }
	      break;
	   }
	}
     }

     *CRT2Index = tempbx;
     *ResIndex = tempal;

  } else {   /* LVDS, 301B-DH (if running on LCD) */

     tempbx = 0;
     if((SiS_Pr->SiS_IF_DEF_CH70xx) && (SiS_Pr->SiS_VBInfo & SetCRT2ToTV)) {

	tempbx = 90;
	if(SiS_Pr->SiS_TVMode & TVSetPAL) {
	   tempbx = 92;
	   if(SiS_Pr->SiS_ModeType > ModeVGA) {
	      if(SiS_Pr->SiS_CHSOverScan) tempbx = 99;
	   }
	   if(SiS_Pr->SiS_TVMode & TVSetPALM)      tempbx = 94;
	   else if(SiS_Pr->SiS_TVMode & TVSetPALN) tempbx = 96;
	}
	if(tempbx != 99) {
	   if(SiS_Pr->SiS_TVMode & TVSetCHOverScan) tempbx++;
	}

     } else {

	switch(SiS_Pr->SiS_LCDResInfo) {
	case Panel_640x480:   tempbx = 12; break;
	case Panel_320x240_1: tempbx = 10; break;
	case Panel_320x240_2:
	case Panel_320x240_3: tempbx = 14; break;
	case Panel_800x600:   tempbx = 16; break;
	case Panel_1024x600:  tempbx = 18; break;
	case Panel_1152x768:
	case Panel_1024x768:  tempbx = 20; break;
	case Panel_1280x768:  tempbx = 22; break;
	case Panel_1280x1024: tempbx = 24; break;
	case Panel_1400x1050: tempbx = 26; break;
	case Panel_1600x1200: tempbx = 28; break;
#ifdef SIS300
	case Panel_Barco1366: tempbx = 80; break;
#endif
	}

	switch(SiS_Pr->SiS_LCDResInfo) {
	case Panel_320x240_1:
	case Panel_320x240_2:
	case Panel_320x240_3:
	case Panel_640x480:
	   break;
	default:
	   if(SiS_Pr->SiS_LCDInfo & DontExpandLCD) tempbx++;
	}

	if(SiS_Pr->SiS_LCDInfo & LCDPass11) tempbx = 30;

#ifdef SIS300
	if(SiS_Pr->SiS_CustomT == CUT_BARCO1024) {
	   tempbx = 82;
	   if(SiS_Pr->SiS_LCDInfo & DontExpandLCD) tempbx++;
	} else if(SiS_Pr->SiS_CustomT == CUT_PANEL848 || SiS_Pr->SiS_CustomT == CUT_PANEL856) {
	   tempbx = 84;
	   if(SiS_Pr->SiS_LCDInfo & DontExpandLCD) tempbx++;
	}
#endif

     }

     (*CRT2Index) = tempbx;
     (*ResIndex) = tempal & 0x1F;
  }
}

static void
SiS_GetRAMDAC2DATA(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex,
		unsigned short RefreshRateTableIndex)
{
  unsigned short tempax=0, tempbx=0, index, dotclock;
  unsigned short temp1=0, modeflag=0, tempcx=0;

  SiS_Pr->SiS_RVBHCMAX  = 1;
  SiS_Pr->SiS_RVBHCFACT = 1;

  if(ModeNo <= 0x13) {

     modeflag = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
     index = SiS_GetModePtr(SiS_Pr,ModeNo,ModeIdIndex);

     tempax = SiS_Pr->SiS_StandTable[index].CRTC[0];
     tempbx = SiS_Pr->SiS_StandTable[index].CRTC[6];
     temp1 = SiS_Pr->SiS_StandTable[index].CRTC[7];

     dotclock = (modeflag & Charx8Dot) ? 8 : 9;

  } else {

     modeflag = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
     index = SiS_GetRefCRT1CRTC(SiS_Pr, RefreshRateTableIndex, SiS_Pr->SiS_UseWideCRT2);

     tempax = SiS_Pr->SiS_CRT1Table[index].CR[0];
     tempax |= (SiS_Pr->SiS_CRT1Table[index].CR[14] << 8);
     tempax &= 0x03FF;
     tempbx = SiS_Pr->SiS_CRT1Table[index].CR[6];
     tempcx = SiS_Pr->SiS_CRT1Table[index].CR[13] << 8;
     tempcx &= 0x0100;
     tempcx <<= 2;
     tempbx |= tempcx;
     temp1  = SiS_Pr->SiS_CRT1Table[index].CR[7];

     dotclock = 8;

  }

  if(temp1 & 0x01) tempbx |= 0x0100;
  if(temp1 & 0x20) tempbx |= 0x0200;

  tempax += 5;
  tempax *= dotclock;
  if(modeflag & HalfDCLK) tempax <<= 1;

  tempbx++;

  SiS_Pr->SiS_VGAHT = SiS_Pr->SiS_HT = tempax;
  SiS_Pr->SiS_VGAVT = SiS_Pr->SiS_VT = tempbx;
}

static void
SiS_CalcPanelLinkTiming(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
		unsigned short ModeIdIndex, unsigned short RefreshRateTableIndex)
{
   unsigned short ResIndex;

   if(SiS_Pr->SiS_LCDInfo & DontExpandLCD) {
      if(SiS_Pr->SiS_LCDInfo & LCDPass11) {
	 if(SiS_Pr->UseCustomMode) {
	    ResIndex = SiS_Pr->CHTotal;
	    if(SiS_Pr->CModeFlag & HalfDCLK) ResIndex <<= 1;
	    SiS_Pr->SiS_VGAHT = SiS_Pr->SiS_HT = ResIndex;
	    SiS_Pr->SiS_VGAVT = SiS_Pr->SiS_VT = SiS_Pr->CVTotal;
	 } else {
	    if(ModeNo < 0x13) {
	       ResIndex = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC;
	    } else {
	       ResIndex = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC_NS;
	    }
	    if(ResIndex == 0x09) {
	       if(SiS_Pr->Alternate1600x1200)        ResIndex = 0x20; /* 1600x1200 LCDA */
	       else if(SiS_Pr->SiS_IF_DEF_LVDS == 1) ResIndex = 0x21; /* 1600x1200 LVDS */
	    }
	    SiS_Pr->SiS_VGAHT = SiS_Pr->SiS_NoScaleData[ResIndex].VGAHT;
	    SiS_Pr->SiS_VGAVT = SiS_Pr->SiS_NoScaleData[ResIndex].VGAVT;
	    SiS_Pr->SiS_HT    = SiS_Pr->SiS_NoScaleData[ResIndex].LCDHT;
	    SiS_Pr->SiS_VT    = SiS_Pr->SiS_NoScaleData[ResIndex].LCDVT;
	 }
      } else {
	 SiS_Pr->SiS_VGAHT = SiS_Pr->SiS_HT = SiS_Pr->PanelHT;
	 SiS_Pr->SiS_VGAVT = SiS_Pr->SiS_VT = SiS_Pr->PanelVT;
      }
   } else {
      /* This handles custom modes and custom panels */
      SiS_Pr->SiS_HDE = SiS_Pr->PanelXRes;
      SiS_Pr->SiS_VDE = SiS_Pr->PanelYRes;
      SiS_Pr->SiS_HT  = SiS_Pr->PanelHT;
      SiS_Pr->SiS_VT  = SiS_Pr->PanelVT;
      SiS_Pr->SiS_VGAHT = SiS_Pr->PanelHT - (SiS_Pr->PanelXRes - SiS_Pr->SiS_VGAHDE);
      SiS_Pr->SiS_VGAVT = SiS_Pr->PanelVT - (SiS_Pr->PanelYRes - SiS_Pr->SiS_VGAVDE);
   }
}

static void
SiS_GetCRT2DataLVDS(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex,
                    unsigned short RefreshRateTableIndex)
{
   unsigned short CRT2Index, ResIndex, backup;
   const struct SiS_LVDSData *LVDSData = NULL;

   SiS_GetCRT2ResInfo(SiS_Pr, ModeNo, ModeIdIndex);

   if(SiS_Pr->SiS_VBType & VB_SISVB) {
      SiS_Pr->SiS_RVBHCMAX  = 1;
      SiS_Pr->SiS_RVBHCFACT = 1;
      SiS_Pr->SiS_NewFlickerMode = 0;
      SiS_Pr->SiS_RVBHRS = 50;
      SiS_Pr->SiS_RY1COE = 0;
      SiS_Pr->SiS_RY2COE = 0;
      SiS_Pr->SiS_RY3COE = 0;
      SiS_Pr->SiS_RY4COE = 0;
      SiS_Pr->SiS_RVBHRS2 = 0;
   }

   if((SiS_Pr->SiS_VBType & VB_SISVB) && (SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA)) {

#ifdef SIS315H
      SiS_CalcPanelLinkTiming(SiS_Pr, ModeNo, ModeIdIndex, RefreshRateTableIndex);
      SiS_CalcLCDACRT1Timing(SiS_Pr, ModeNo, ModeIdIndex);
#endif

   } else {

      /* 301BDH needs LVDS Data */
      backup = SiS_Pr->SiS_IF_DEF_LVDS;
      if((SiS_Pr->SiS_VBType & VB_NoLCD) && (SiS_Pr->SiS_VBInfo & SetCRT2ToLCD)) {
	 SiS_Pr->SiS_IF_DEF_LVDS = 1;
      }

      SiS_GetCRT2Ptr(SiS_Pr, ModeNo, ModeIdIndex, RefreshRateTableIndex,
                     		            &CRT2Index, &ResIndex);

      SiS_Pr->SiS_IF_DEF_LVDS = backup;

      switch(CRT2Index) {
	 case 10: LVDSData = SiS_Pr->SiS_LVDS320x240Data_1;    break;
	 case 14: LVDSData = SiS_Pr->SiS_LVDS320x240Data_2;    break;
	 case 12: LVDSData = SiS_Pr->SiS_LVDS640x480Data_1;    break;
	 case 16: LVDSData = SiS_Pr->SiS_LVDS800x600Data_1;    break;
	 case 18: LVDSData = SiS_Pr->SiS_LVDS1024x600Data_1;   break;
	 case 20: LVDSData = SiS_Pr->SiS_LVDS1024x768Data_1;   break;
#ifdef SIS300
	 case 80: LVDSData = SiS_Pr->SiS_LVDSBARCO1366Data_1;  break;
	 case 81: LVDSData = SiS_Pr->SiS_LVDSBARCO1366Data_2;  break;
	 case 82: LVDSData = SiS_Pr->SiS_LVDSBARCO1024Data_1;  break;
	 case 84: LVDSData = SiS_Pr->SiS_LVDS848x480Data_1;    break;
	 case 85: LVDSData = SiS_Pr->SiS_LVDS848x480Data_2;    break;
#endif
	 case 90: LVDSData = SiS_Pr->SiS_CHTVUNTSCData;        break;
	 case 91: LVDSData = SiS_Pr->SiS_CHTVONTSCData;        break;
	 case 92: LVDSData = SiS_Pr->SiS_CHTVUPALData;         break;
	 case 93: LVDSData = SiS_Pr->SiS_CHTVOPALData;         break;
	 case 94: LVDSData = SiS_Pr->SiS_CHTVUPALMData;        break;
	 case 95: LVDSData = SiS_Pr->SiS_CHTVOPALMData;        break;
	 case 96: LVDSData = SiS_Pr->SiS_CHTVUPALNData;        break;
	 case 97: LVDSData = SiS_Pr->SiS_CHTVOPALNData;        break;
	 case 99: LVDSData = SiS_Pr->SiS_CHTVSOPALData;	       break;
      }

      if(LVDSData) {
	 SiS_Pr->SiS_VGAHT = (LVDSData+ResIndex)->VGAHT;
	 SiS_Pr->SiS_VGAVT = (LVDSData+ResIndex)->VGAVT;
	 SiS_Pr->SiS_HT    = (LVDSData+ResIndex)->LCDHT;
	 SiS_Pr->SiS_VT    = (LVDSData+ResIndex)->LCDVT;
      } else {
	 SiS_CalcPanelLinkTiming(SiS_Pr, ModeNo, ModeIdIndex, RefreshRateTableIndex);
      }

      if( (!(SiS_Pr->SiS_VBType & VB_SISVB)) &&
	  (SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) &&
	  (!(SiS_Pr->SiS_LCDInfo & LCDPass11)) ) {
	 if( (!(SiS_Pr->SiS_LCDInfo & DontExpandLCD)) ||
	     (SiS_Pr->SiS_SetFlag & SetDOSMode) ) {
	    SiS_Pr->SiS_HDE = SiS_Pr->PanelXRes;
            SiS_Pr->SiS_VDE = SiS_Pr->PanelYRes;
#ifdef SIS300
	    if(SiS_Pr->SiS_CustomT == CUT_BARCO1366) {
	       if(ResIndex < 0x08) {
		  SiS_Pr->SiS_HDE = 1280;
		  SiS_Pr->SiS_VDE = 1024;
	       }
	    }
#endif
         }
      }
   }
}

static void
SiS_GetCRT2Data301(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex,
		unsigned short RefreshRateTableIndex)
{
  unsigned char  *ROMAddr = NULL;
  unsigned short tempax, tempbx, modeflag, romptr=0;
  unsigned short resinfo, CRT2Index, ResIndex;
  const struct SiS_LCDData *LCDPtr = NULL;
  const struct SiS_TVData  *TVPtr  = NULL;
#ifdef SIS315H
  short resinfo661;
#endif

  if(ModeNo <= 0x13) {
     modeflag = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
     resinfo = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ResInfo;
  } else if(SiS_Pr->UseCustomMode) {
     modeflag = SiS_Pr->CModeFlag;
     resinfo = 0;
  } else {
     modeflag = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
     resinfo = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;
#ifdef SIS315H
     resinfo661 = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].ROMMODEIDX661;
     if( (SiS_Pr->SiS_VBInfo & SetCRT2ToLCD)   &&
	 (SiS_Pr->SiS_SetFlag & LCDVESATiming) &&
	 (resinfo661 >= 0)                     &&
	 (SiS_Pr->SiS_NeedRomModeData) ) {
	if((ROMAddr = GetLCDStructPtr661(SiS_Pr))) {
	   if((romptr = (SISGETROMW(21)))) {
	      romptr += (resinfo661 * 10);
	      ROMAddr = SiS_Pr->VirtualRomBase;
	   }
	}
     }
#endif
  }

  SiS_Pr->SiS_NewFlickerMode = 0;
  SiS_Pr->SiS_RVBHRS = 50;
  SiS_Pr->SiS_RY1COE = 0;
  SiS_Pr->SiS_RY2COE = 0;
  SiS_Pr->SiS_RY3COE = 0;
  SiS_Pr->SiS_RY4COE = 0;
  SiS_Pr->SiS_RVBHRS2 = 0;

  SiS_GetCRT2ResInfo(SiS_Pr,ModeNo,ModeIdIndex);

  if(SiS_Pr->SiS_VBInfo & SetCRT2ToRAMDAC) {

     if(SiS_Pr->UseCustomMode) {

	SiS_Pr->SiS_RVBHCMAX  = 1;
	SiS_Pr->SiS_RVBHCFACT = 1;
	SiS_Pr->SiS_HDE       = SiS_Pr->SiS_VGAHDE;
	SiS_Pr->SiS_VDE       = SiS_Pr->SiS_VGAVDE;

	tempax = SiS_Pr->CHTotal;
	if(modeflag & HalfDCLK) tempax <<= 1;
	SiS_Pr->SiS_VGAHT = ree8 $XFree8*/
/*tempax;
	 $XdotOrg$ *VGAV/
/* $XdotOrg$ *e (CRT2 sectiCVTotal;

 630/} else {
 initiGetRAMDAC2DATA( $Xdot, ModeNo]661[FIdIndex, RefreshRateTablex[GX])40/630/730/6330,
 * if/651/[MXFree86BInfo & SetCRT2ToTV)    630/7SiS 315ux kPtr/651/[M]61[FGM]/[M]74x[GX]/30/[M]76x[GX],
 *    ,
		630/&ux kx[GX]/&Res*     XGI V3XTswitch(schhofer,l fr	case  2: TVPtr
/* $XdotOrg$ *ExtHiTVData;   break;as part 3f the Linux kernel, the fPALwing liccense terms
 * 4f the Linux kernel, the fNTSCwing license terms
 * 5f the Linux kernel, the f525iwing license terms
 * 6ms of the GNU General Publicpwing license terms
 * 7f the Linux kernel, the f750dation; either version 8f the Linux kernel, thSam is free socense terms
 * 9 *
 * * This program is odify
 * * icense terms
 *10 *
 * * This program is lic License  ANY WARRANTY; 1ithout even the implied wardation; e ANY WARRANTY; of the Linux kernel, thS* * or any la ANY WARRANTY; apply:
 *
 * * This proSt1ollowing license terms
 *1n redistribute it and/oSt2ollowing license terdefault *
 * * This program is distributed in the h V3XT/V5/Vbuffer ersal moRVBHCMAX  = (the L+ienna, Au->ce, Suitdation,9 Temple Place, SFAC/
/*0, Boston, MA 02111-130 folUSA
 *
 * Otherwise6$ */
 useowing license terms 6$ */
 * * Redistribution aVd use in source and binary foVms, with or without
HDE use e in source and binaryTVHDE
 * * Redistributionowing conditions
 * * are met:
V* * 1) Redistributionce, RS2conditions
 * * are met   notic& 0x0fffdation,if(modeflag & HalfDCLKed asyright
 * *    not in source and binaryHALF   not;
	* (Universal mo   notied ascopyright
 * *    notic= (ight
 * *    notice, + 3) >> 1) - 3;is li * Modd the, Boston, MA 02111-13aimethe
2)d the 7   docifn and/or other materials pr the8000)pyright
 * *    notic-*
 * Mode i  0,
 * e products
 * *    derived from th following disclaimer*
 * Mode i}/630/730,
 *   ributions in binary ce, this list of conditions adation, Ic., 59 Temple PlaNewFlicker61[Ftion and/or other mateESS OR
 * *) << 740/630/7* (Universal module for Linux kerHiVisionl frapyri(resie fo== SIS_RI_960x6auth  ||or prY AND FITNESS FOR A1024x768)AR PURPOSE ARE DISCLAIMED.
 *280x* * )EVENT SHALL THE AUTHOR BE LIABLE720) this list of conditXPRESS OR
 * * IM0x40rior opyright
 * *    ed pDETNES35uthor may not bTV * * |= TVSetTVSimu61[F;   SiS otOrg$ */
/*
e follorms,initializing cSS OF USE,
prov (INCLUDING, BUT
ule for LinInSlaveG, BUThis li* (Universal moREMENT & SUBSTITUTE GOODNY
 * * ed that the foll/
/* USE,
 * * RICT LIABILITY, ORe (CRTR BUSINESS3XT/VAGES630/730,
 * * (Universal module for Linux kerYPbPr5257IMITTABILITORY OF LIABILITY, WHETHER EVEN  * oNY
 * *  SERVICES; LOSS O165DAMANG NEGLIGENCE OR OTH7fer <Y OUT OF THE USE OF
 *TY OF SUCH DAMAGE.
RTIC* Author: 	Thomas Winischodif(INCLUDIF THE POSSIBILITY OF SUCH DAMRTIC FOR A for 300 series by Si2(INCLUDINEGLIGENCE OR OTHodifINESSen permissnts for 300 series by SiS, Inc.
 * Used by permission.
 *
 */odiffdef HAVE_CONFIG_H
#include "config.h"
#endif

#if 1
#define SE0/630/730,
 *     SiS  * *    nY1COElowing license terms ane CO * DATA, OR PRORY2 COMPAQ_HACK	/* Needed forx102entec/Compaq 12803 COMPAQ_HACK	/* Needed forsus entec/Compaq 12804 COMPAQ_HACK	/* Needed for
#ifS ORdisclaimer.
 * * 2) Redistrthis software wine COMPA0x0er <thomas@winischh80x1024 (0xf4ine SiS_I2CDELAY    sus A2HLE Fine SiS_I2CDELAY    
#ifdef0x38AMAGES (IN!THE POSSIBILITY OF SUCH DAMPALAL, SPECIAL, EXEMPLAR EMI values */
#endif

#if 1
#define SET_PWD		/* 301/302LV: Set PWD */
#endif

#define COMPAL_HACK	/* Needed for T_EMI		/* 302LV/ELV: Set EMI PAL"config.h"
#endif

#if 1
PALd for Compal 1V5/V8, Z7
 * (Universal module for Linux kerLCDl framebuffer mple Place, Suite 331USA
 *
 * Otherwise, the follow1 LIMITED TO, THE IMUseCustomTRACT, S* DATA, OR PROlowingCRT2 section)
 GA * * initializing cse if(SiS_Pr->ChipType abov
	umentatior SiS 300H305/540
#endif

#ifdef SIS315H
#umentat<<
   iinitializing cod*/
/* $XdotOrg$ */
/*
 * Mode initializing code (CRT2 section)
 * for SiS 300/305/540/630/730,
 *     bool gotit = falsDS ORLITY**/

void
SiLCDle for DontExpand*****&& (struct SiS_Prilse if(SiSLCDPass11)AL, S <thomas@winischho
}

#ifdef SIS_Panel"config.h"
#endif

#iode (CRT2 secti    S) ARISIN SERVICES; LOSS if(SiS_Pr->C    SiS_SetRegAND(SiS_Pr->********************) ARISINype == XtruDS ORY OUT OF TSIS_315H)
      SiS_SetReg_Pr->ChipType  >= SIromptr >= SIROMAddr) ISED #ifdefSS F315His list of conditionsSuite 33egSR11A[oid
Si]ine SiS_I2CDELAY    the followtaAND, unsigne+1d short DataOR)
{
 n and use in>ChipType >= SI2] | (RegSR11Ape >= SI3]d the fUT NO8)_SetRegAND(SiS_Pr->SiS_PS SOFTW
   }
   SiS_Se4]T NOR AN0f;
   }
   SiS_SetRegANDf0n the4r->SiS_P3c4,0x11,Dand use x0f;
      DataOR  &5 0x0f;
   }
   SiS_Se6RegANDOR(SiS_Pr->SiS_P3c4,0x11,Dat to LCD str);
}

/*********7***********************************/
/*    HELPER: Get Pointe   notice, t
   }
   SiS_Se8 0x0f;
   }
   SiS_Se9RegANDOR(SiS_Pr->SiS_    return;
   enotice, t= SIclaimer.
 * * 2) RedT, STRICT LIABILITY, ORions and the following disclaimer in the
 * *    docdocumentationBase;
   unsigned *     with the disULL;
 Base;
   unsigned char8uthor may not be used to endorse or pr promote products
 * *    derives software without specific prioSING ITED TO, THE IMPLIED
}

)             */r promote STRICT LIABILITY, ORlse if(S|=**********/

sNCLUDING NEGLIGENCE Olse if(Si= ~AND(SiS_PNCLUDING NEGLIGENCE OS_UnLockCRT2(stnelSelfDetected))) {

  Pr)
{
   iLUDING NEGLIGENCE OR and us***********************iS_P3c4,0x11,DataAND,Daart1Port,0x24,0xFE);
}

}

/****************               reg = 0x7d;

      idx = (

      if(idx < (8*26_P3d4,reg) & 0x1f) * 26ions and tatic un11             */not su#endif/
/*sInfo(sype =>SiS_Part1Porand X.org/XFree86 4.x)
 *
 * Copyright (C) 2001-2005 by Thomas	inischhofer, Vienna, Austrppor
 * If distributed asex = S part    S * * IN Nex = S:gAND( Linux kernel, the fLCD* * IN Nwing license terPtr661_2(struct SiS_Private+ 32S_Pr)
{
   unsigned chaSt2*ROMAddr = SiS_Pr->VirtualRomBase;
   unsigneNCIDENTte *SiSels; TMDS is unreliable
    *omptr = 0;

   /* Use the B*ROMNCIDENTtributed iVDS panels; TMDS is unreliable
 68_ice,iS_Pr)
{
   unsigned char  *ROM(such as wing lBIOS has better knowledge (such as omptr = 0;

   /* Use the BIO a 301C and a panell that does not support DDC)
  80  * due to the variaty of panels>SiS_BIOS doesn't know about.
    * Exce80ion: If the BIOS has better knowledge (suc800s in ca;
      romptr += ((SiS_GetRePanelSelfDetected))) {
      romptr = (SiS_Pr->->SiS_ROMNew) &&
      ((SiS_Pr->54S_VBType & VB_SISLVDS) || (!SiS_****PanelSelfDetected))) {
      romptr 54on: If the BIOS has better knowledge (suc96  * due to the variaty of panels*****BIOS doesn't know about.
    * Exce96ion: If the BIOS has better knowledge (suc* * e *SiS_Pr)
{
   unsigned char  *ROMABLE FORiS_Pr->SiS_ROMNew) &&
      ((SiS_Pr-x,
		omptr = 0;

   /* Use the BIOS tabt *i)
{
   unsigned short checkmask=0, mo40id, 5  * duS_Pr)
{
   unsigned char  *ROM->SiS_VBRTI + (*i)].ModeID;

   if(SiS_Pr->SiS_VBTse the BIOS data as well.
    */
->SiS_VBInfo & l that does not support DDC6>SiS2SiS_VBTS_Pr)
{
   unsigned char  *ROMortRAMDAiS_Pr->SiS_ROMNew) &&
      ((SiSortRAMDACse the BIOS data as well.
    */
& VB_SIS30xBLV)) {
	       checkmask |= Supeid, VBType &2) {
		  checkmask |= SupportBIOS doesn't know about.
    * = Supportributed in the hoPtr661_2(st100as Win_135;
	    if(SiS_Pr->SiS_NoScalewing tCRT2ense teruct SiS_Private *S661_2(stMDAC2_13pe & VB_SISVS_Pr)
{
   uns310he fCompaqefIndex[RRTI + (RAMDAC202) {
		  chec201VBType & VB_SISVB) {
	       if= SiS_Pr->SiS_RefIndex[RRTI + (*i)].Mo    if(tCRT2Toram; ife products
 * *   S_Pr)
{
   unsigned char  *ROMAddr = SiS_Pr->VirtualRomB}struct SiS_P_XORG_XF86IS_315H)TWDEBUGt6404xf86DrvMsg(0, X_INFO, "and X.owing: x[GX] %d ienna, A %d\n", schhofer, bPr525750)= Support    if(ro, 59 Temple Place, Suite 330Pr)
{
ton, MA 02111-1307, USRT)) {

	 checkmask | followitTV;
	 if(SiS_Pr->SiS_V:
 *
 1) {
      DataAND &= 0x0f;
kmask |= SupportTV1 forms,SiS_P3c4,0x11,DataAND,DataOR)2ToYPbPr525750) {
	  xFE);
}

/**************** & TVSetYPbPr750p) {
	   LCD**************/
/*        bPr750p;
	       }
	    }
	 provANY WAion) {rt1Port,0x2f,0x01)    SXRes;x != 0b {
	 if(SiS_Pr->SYS_VBI
	
 * If ow about.
    *iennafoed as partruct SiS_PrivaAC2_20F THE POSSIBILISetFer.
 *->SiESATimingT, STRICT LF THE POSSIChipType <SS FOrivaNY
 *	 ifF_CH7NCLUDING, BUT
 * * NOT LIMITfo & SetC56atic	 OUT OF THE USE OF
 *  * * NOT 4authfo & SetC6 DAMAEF_CH70xnot s
    */

   if( table for matching CRT2 mode */7   for(; SiS2esn't knowS_RefIndex[RRTI + (*i)].ModeID 2= modeid; (*i2mindex = SS_RefIndex[RRTI + (*i)].ModeID525 modeid; (*775break;
   }

   /* Look through the whoTICULde-section of the table from the beginning
    * fo*/
   for(; SiS_Pr->ch as in caIndex[RRTI + (*i)].ModeID == modeid; (*i)--) {
   infoense terms
 *SiS_AdjustCRT2S_VBInfoable for matching CRT2 mode */
  matching CRefine Si {
      if(SiS_Pr->SiS_RefIndex[RRmatching C8    if(infoflag & checkmask) return tr FOR Afo & SetC9; ; (*i)eid) break;
      infofl* * = SiS_Pr->SiS_RefIndex[RRTI + (*i)].E6a matching CR6LCDRif no mode was found yet.
    */
 7 mode-sectiourn false;
}

/************************40atePtr(struct6fine Sieid) break;
      inortRAMDAS_VBInfo struct SiS_PriCD) {
	 checkmask |= SuppportLCD;
   ->SiS_RefIndex[RRTI + (*i)].Ext_InfoFlag;
 8 of the table from the beginning
    * foue;
   }
   ret10efine Si!= modeid) breGES (INCLUDING, BUTlse if(SiS_Pr->ChipType >uctPtrt1Port,0x2f,0x01)hipType >= SI0x00, 0 SetCRT2ToTV)iS_Pr->SiS_P, 0x0return;
   else *
 * Mode initializing c up FIFO bxAUTHOR ``AS}
}

static void
index += idwing(structth a 30ivate *651/[M]6unsigned short661[FGM]X[ModeIdIndex].St_Mo74x[GX]/F_DEF_CH7S_Pr->Si[ModeIdIndex].S30/[M]76x[GX],
 *     
frameb* (Universal modu  /* & VB_SISVBl framebuf*/
/*********************************A1,
	hines with a      modeflLVDS/651/[M]661[FGM]X/[M]74x[GX]/330/[M]76x[GX],
 *     XGEF_CH70
    */

     return;
   e->SiS_IF_DEFN******= SI**********************************T, STRICT/* Need DCLK wing forheck on 301B-DH */ (*i)+modeflag & HalfDCLK) return 0;
      }
   }

   if(ModeNo < 0x14) return 	oflag = SiS_Pr-modeflag & Half301_SetFlag & ProgrammingCRT2) {
      if(SiS_Pr->SiS_VBType 0xFFFF;IF_DE30,
 *    
	 if(modeflag & HalfDCLK) return 0;
      }
   }

   if(ModeNo < 0x14) return  inde(Mod/*>SiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToLCDA)/
/*S_Pr->SiSGETex = inES (SKEW) ]650iS_Pr->SiS) {
	SiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToLCDA)) {deNo <= const g = SiS_Pr-DCLKDes *) {
    dex > trg/Xg = SiS_Pr->SiS_SModeIDTablag;661_];
	       if(index > tem    Semp;
	 1
#ULL;struct SiS_Pr00
   }

   if(SiS_Pr->*****************************      }

   }

   /* Look backwards  }

   if(SiS_PrLCD  /*le fo1, 0T, STRICTF THE POSSIBILI= XGI_TRRTICUT_BARCO1366T, STRICT L      | SetCRT2         if(    S  /*04_1a; (*i)++) = 0;
	 }
      }
 x01, 0x01, 0x01, 0x01,
			o = SiS_Pr->SiS_RefIndex[RRTI].ModeID;

2  if(SiS_PrB_NoLCD)0xFFFF;

   i>SiS_EModeIDTable[ModeIdIndex].REFinFOR A   ModeNo = SiS_Pr->SiS_RefIndex[RRTI].ModeID;

 b if(SiS_Pr->ChipType >= SIS_315H) {
      if(!(SiS_Pr->SiS_VBInfo & DriverMode)) {
	 if( (SiS_P ) {
	    i  infoRTI +B_NoLCD)		);
      if(   return = SiS_Pr->S;(ModeNo <= 0x13) {
    dex > tdeflag = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
   } else {
      modeflag = SiS_Pr->SiSSiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;iS_EModeIDTable[Mclaimer.SetCRT2ToSn 0xPr->SiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToLCDA)_VBType & VB_SISLHiS_Vromin_VBType & VB_SISLV->SiS_Ref
backuSome special1_2(ss
	  {
      if(SiS_Pr->SiS_VBInfo & SetCRTl framebuf/* TrumpionlaceMo_Pr->ChipType >= SIF_DEF_TRUMPION>SiS_= 0;
	 }
      }
  }
    RRTIruct SiS_PrivaNY
 * * THEORY OF LIABIi)].ModeIDRT2ToTV) {
	    c>SiS_EModeIdex[RRTI + i - 1].Ext_t1Port,0x24,0xF - 0x3c;
 G IN 	   temdation, Inc., 5/**i)-x480(indx = i & ProgrammingCRT2)}

   /* Look backwards o & DisableCRT2Display))) {
      b********&&ex, RRTI, &i)))   }

   RRTI3* Author: 	Thomas WiS_Pr->SiS_*******,backupindex;
   static con>= 4If the BIOS has  - 1].Ext_512************/

unsigned short
SiSunsiauthor may not b - 1].Ext_436ned short temp1, temp2;

   /* StoreLIMITED TO, PROCUR34 */
   S)--) {
 urn (RRTor writteV5/V8,/*  if(InfoPr->C >> SiS_Pre == XGI_20)
  

	 checkAR PUndex--& DisableCRT2Display))) {
      b= XGI_ULAR PUS_Pr->SiS_P3d4,0x31le[ModeIdIndex]PANEL84NO ENDOR(SiS_Pr->SiS_P3d4,0x31************/
/*    H56PER: GET SOME DATA FROM BIOS SiS_SetRegAND(SiS_PrDOR(sR: GEurn (RRTI >> 8;if(61[FGM <0

st3)bPr525750|  /* Use the BI
    DX],
 [
      mode].St_ux kCRTCefInomote products
 * *Private *SiS_Pr)
{
   uRefx[GX][30/[M]76x[GX],
 *    ].ExS_Pr->VirtuaIOSWor
   if(SiS_Pr->SiS_IF_DEF_CH30xBLVx33) >> SiS_Pr->SiS_SelectCRT2Rate) & 0A->SiS_ruct SiS_PrivatProgrammingCRT2) && IS_315H) {
      if(!(SiS_Pr/* non-pass 1:1 only, see above indet DDC)
    * use thDE !
	 if(SiS_Pr->SiS_V****/

static void
SiS_SaveCRT: Get Pointer t-the followiPr->SiS_V -SiS_Pr->ChipType >=) / 2VBTy}   return false;
}

= 0xic bool
SiS_CR36eNo, ModeIdIx, RRTI, &i))) {
	 i = backup_ = (unsgned char  *ROMAd   c SiS_Pr->VirtualRoVBase;
   unsi_EModeIDTabl   if(!(SiS_AdjustCRT2Rate(SiS_Pr, ModeNo, Modemask |= SupportCHTVle[Modeed as partdex]UNIWILLindex  eturn true;
      }
2   }
   retuCLEVOpTypS_VBInfo & SetCRT2ToLDisplay))) {
      backup_i = i;
   ndex, RRTI, &i))) {
	 i = backup_i;
      }
   }

   01,
		0x01, 0kmask |= SupportCHTV;
	 }
      }

      if(SiSate index       TA FROM BIOS ROM    *!Index]COMPAQ *SiLPER: DELAY FUNCTIONS          */
/*************************************romptr += ((SiS_Get: & tVerified;

  Averatec 6240 inderomptr += ((SiS_GetRe)
static void
SiS_Gsus A4Lstruct SiS_Private *Si54:  backupot vic void
yet FIXME index--x, RRTI, &i))) {
	 i = backup_i;
      }
   }


		0x01, 0F_CH70x    if(rndex = 0;
	    el == 0x12) && (RO(!(SiS_CH70xx  wh0x33) >> SiS_Pr->SiS_SelectCRT2Rate) TV->SiS_P == 0x12) && (ROvate *SiS_Pr);
#if>= SIS_315H)
      vate *SiS_Pr);
#iMr->SiSpport DPr525750|<*****tatic void
SiS_SaveCRT25S_SeN ANY WAY OUT OF T(= SiS_Pr->SiS_RefIdex = temp;
	 e {
	 i->SiS_Ptatic void
SiS_SaveCRT(SiS_Pr, 66)    }
	    }
	 }DE cop temp1 = (SiS_Pr->SiS_ivate *SiS_Pr, unsigned sho1].E40/630/730,
 * );
	 temp1 = SISGETROMW(0x23b);
	 if(temp1   return false;
}

static bool
SiS_CR36BIOSWord23d(struct SiS_Private *SiS_Pr)
{
   unsigned char  *ROMAddr = SiS_Pr->VirtualRomBase;
   unsigned short temp,temp1;

   if(SiS_Pr->SiS_UseROM) {
      if((ROMAddr[0x233] == 0x12) && (ROMAddr[0x234] == 0x34)) {
	 temp = 1 << ((SiS_Get
    */

   Pr->SiS_VBInfo & SetCRT2ToTV) index DELAY FUNCTIONS          */
/************************
    */

   if(mask |= SupportCHTV;
	 }
      }

Ptr661_2(struct S80ARTICAC2_202;
	       }
	 _Pr->SiS_VBIncheckmask=0, modeid, in:Pr->x, RRTI, &i))) {
	 i = backup_i;
    r->Si*i)].ModeID;

   if(SiS_Pr->SiS_VBay = SiS_Pr->SiS_PanelDelayTbl[DelayIndex]  }
   mer[0];
	 } else  infofor Compal 1400x1050 le[ModeI*****************/
/*            S) index = 0;
	signed shor1))  {
	 Delay = 3;
      } el{
	 if(DelayTime >= 2) Dela   if(!(SiS_AdjustCRT2Rate(SiS_Pr, ModeNo, Mode>SiS_PanelDelayTbl[DelayIndex].timer[1];
	 }
	0xFFFF;

   inde= SiS_Pr->SiS_Panate *SiS_Pr)
{
    SiS

  }
	 iSiS_Pr->SiS_PanelDelayTbl[DelayIndex].timeIndex[RRTI + (*i)].ModeID == m temp1 = (SiS_Pr->Si-= gnedSiS_Refase
    * of machines with a 301C and== SIS_330)fine Si[RRTI + i]RAMDAC202) {
yTime -= 2;
	 if(!(DelayTimensigned short)ROMAddr[0x226];
	    }
	 }
      }
      SiS_ShortDelay(SiS_Pr, Delay);

#endif  /* SIS300 */

   } else {

#ifdef SIS315H

      if((S
	 }
	 i   if(!(SiS_AdjustCRT2<re CRT1 ModeNo in CR34 */
   Index].timer[1];
	LCDR= CUT_CLEVO1400) */ ) {			/);
   temp1 = (SiS_Pr->SiS_Index].timer[1];
	 gned sh SiS_DDC2Delay(SiS_Pr, 0x1000);
	 } else >= 2) Dram; if 	    SiS_DD for matching CRTw up c bool
SiS_CR36BIOSWo&&Pr->S if(!(SiS_AdjustCRT2Rate(SiS_Pr, ModeNo, Mlse if((SiS_Pr->SiS_IF_DEF_LVDS == 1) /* ||
	 (SiS_Pr->SiS_CustomT == CUT_COMPAQ1280) ||
	 (SiS_Pr->SiS_CustomT =if(SiS_Pr->SiS_CustomT == 0xx  0x0f) == 1))  {
	 Delay   }

          else 	1S_GetReg(struct SiS_Private *SiS_Pr)
{
   u - 1].Ext_InfDDC2Delay(SiS_Pr, 0x1003:**************ly? index--;
static void
SiS_SaveCRT2Info(so(struct SiS_Private *SiS_Pr, unsigned short ModeNo)
{
   unsigned sh short temp1, temp2;

   /* Store CRT1 ModeNo in CR34 */
   SiS_SetRetReg(SiS_Pr->SiS_P3d4,0x34,ModeNo);
   temp1 = (SiS_Pr->SiS_VBInfo & ) && ((PanelID &break;
   630/730,
 *   ruct SiS_Private *Signed short)ROMAddr[0x225];
	       else 	    	 
	 if(!(DelayT       Get rate index     SiS_DDC2Delay(SiS_Pr, 0x4000);
	 }

      } else iw)) {

	 if(!(DelayTime & 0x0 = backup_i;
      }
   }

 SiS_DDC2Delay(SiS_Pr, 0x1000);
	 320x240_{
	    ex = SiS_GetReg(SiS
#en>SiS_P3d4,0x36) >> 4;
3
	       if(DelayTime >={
   uns2 {
	    Si1)) {
			Delay = (unveMode) >> 8;e(delayd23b(struct SiS_3) >> SiS_Pr->Si*******************/

stat{
	claimer.
SiS_Pr)
{
   unsigned char  *ROMAddr = SiS_61[F {
	se
    0x12) && (ROMAddr[0x234] == 0x34)) {
	 temp = 1 << ((SiS_GetReg(SiS_Pr->SiS_P3t RRTI,i,= 0, reg = 0, idx = 0;tatic void
SiS_SaveCRT63gnedchar  *ROMi,backup_i;
   unsigned SetDOnsignx0F;
   b**************************!
      baABLE FOR AortLCD;
      }

   ***************>/
/*          HELPER: DELAY0x10)) PanelID = 0x12;
      }
      DelayIn  HELPER:r, unsigned short DelayTime, unsigned short De3) break;
  d short)ROMAddr[0x17e];
		  S_Pr->SiS_EModeIDTabDisplay))) {
      backup_i = Time, unsigned short De48Pr->Si
      SiS_PanelDelay(SiS_Pr,{
      ba->SiS_VByTime, unsigned short De80fine return;
   if(!(SiS_GetReg(SiS_Pr->SiS_PortRAMDAyTime, unsigned short De7tchdog = 6553r, unsigned short DelayT_Pr->SnelDelayTblLVDS[DelayIndex]WaitRef(SiS_Pr->ChipType >= SIS_tReg(SiS_Pr->SiS_P3d4,0x17) & 0x80)) return;

   wlayLoo)) && --watchdog);
}

#if defined(SIS300) ||r->SiS_P3da) & 0x08) && --watchd542n 0xFFFF   + i].Mr[0];
	 odeIr->SiS_UseROM) {
	 0;

   if(Mod->SiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToLCDA)) {
	    if(!(  DISABLE VIDEO BRIDGwing con)) {
	       temp = LCDRefreshIndex[SiS_GetBIOSLCDResInforuct SiS_PrivateNo <= int) {
 HandlePWD    }
	 }
      } else {
	 if(SiS_intool
iS_Refruct SiSET_PWDtReg[ModeIdInchar *egSR11A
	 SiS_DDC2VirtualRomBa20)
SiS_EModeIDTable[Moid
Si = ].ExCDS = SiPtr661_2tic voidxBLV) {
	 if(!      driverclai;
   }
}
#Regshort)ROMAddrP3d4,0x31BIOS d DAMSiS_EModeIDTable[M * M	 if(SSiS_Pr->SiS_P3c4,0>SiS_IF_DEF_CHPW0x33)0x220] &void
SiS				ace2(SiS_PrSiS_Pr->SiS_PWDOffset0
static boTime,SetReg(SiS_Pr->SiS_Part4Port,0x2b,
   }
   SiS_S +_RefIndex[RRTI
#ifdef  + 0]rn 0xFFFF  if(!(SiS_GetReg(SiS_Pr->SiS_Part1Pcrt,0x00) & 0x40)) {
	 SiS_WaitRetrace1(SiS1Pr);
      } else {
	 SiS_WaitRetrace2(SiS_Pr,drt,0x00) & 0x40)) {
	 SiS_WaitRetrace1(SiS2Pr);
      } else {
	 SiS_WaitRetrace2(SiS_Pr,ert,0x00) & 0x40)) {
	 SiS_WaitRetrace1(SiS3Pr);
      } else {
	 SiS_WaitRetrace2(SiS_Pr,frt,0x00) & 0x40)) {
	 SiS_WaitRetrace1(SiS4Pr);
      * Mf

#defin  HELPER:bout.
    * Exc0;
   & (0x06T NO1tatic !}
      ifToTV) {
	 if(     } el0xc0)ChipTyp(struct deID != M  if(!(SiSANDOR_GetReg(SiS_Pr->SiS_Part1P7,0x7f, * MVIDE
	 checkmask |= SupportHiVision;

 S_Pr-> } else if(SiS0, "Setting PWD %xtCRT2(SiS_Pr-O|SetCRT2ToSCeNo) break;
      tempretSiS_    if(rckupEVER use any vari],
 s (_DEF_C), this will be called
 * from outside the {
 text offo & 
 * If!    MUST      getlse {
	befor      e {
ELPE
2 = 0x13) {
 Dis],
 Bridge    }
	 }
      } else {
	 if(S4,0x36) >> 4) & 0)) {
	 SiS_WaitRetrah, pushax=0,fo & numtrace1(Sit4Port,0x01) >= 0xb0)=InfoFl}

   if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {}

   if(SiS_Pr->SiS_IF_DEF_CH 0x34)) {	 & t=iS_G For  0x3/C/LVSiS_GetInfo  return fal}

   /* Look backwards0x01)) Delay =_Get/* 300 serierlacert RRTI,i,backCR36BIOSWord23btic void
SiS_    SiS_DDC2Delay(SiS_P>SiS_IF_DEF_CHDCLK     }
    LongWait(_GetReg(SiS_Pr->SiS_Part1P6,0xFEr->SiS_* SIS300 */

   } e(!(SiSSR11it(struct Si SIS{
  0Pr->SiS_ SiS_DDC2Del[RRTI].MoDelay/651/[M]63r->SiS_support DDC)
Isex >tic void
*/

   if((SiSef SIS315H
   if(SiS_Pr->ChipType 1f1PorfReg(SiS_PrlEdgeex]. 0xf0)) {
	 i1SiS_GetReg(SilEdge) return true;
      }
  2S_Part100,0xDFr->SiS_P3c4DisplayOfed int dr->SiS_P3c4ef SIS315H
   if(SiS_Pr3ct1Por2ned short flag;
IS_315H) {
      flag = SiS_G1Ened short flag;
UnLockux k_Pr->ChipType  int i;
   for(i ct SiS_Private *SiS_& EnableDualEdge) retstruct SiS_Private 1
   unsi1,: If x3c;
      el315H
static bool
SiS_IsVAorLCD(strtReg4iS_Privasupport DSIS_315HschhosLC5H
   ifd
Si PURPOSiS_Preturn true;
   returdtic void
Si& EnableDualEdg5f) & 0xf0)) {
	 if   unl
SiS_IsDualEdge(struct SiS_Private *SiS_Pr)
hines with a ef SIS315H
   if(SiS_Pr->ChipType >= SISDS_Private     if((SiS_Pr->ChipType != SIS_650) || (SBunsi  HELPERiS_UseROM) *******Flag;
= 0;
2 = ~(S) {
      if((S(SiS_Pr->SiS_Pa_Pr->SiS153d4,0x30) & 0x20)nt didpwd romindex ChipTc XGI_1d(SIA FROM BIOS ROM    */
/****(delaytime-- true;
   r & 0x0f) !=r)
{
   if((SiS_GetReg(SiS_Pr********c unsign   retu(!(SiS_GetReg(SiS_Pr->SiS_Part1Por BIOS d7f unsignIsDualEdge(struct SiS_Private *SiS_Pr)< SIS_315H) EMISiS_Pr)
{
#ifdef SIS315H
   if(SiS_Pr-EMIards in td int delaytime)
{
   while (Pr->SiS_V(SiS_GetR15H) {
      if((SiS_CRT2IsLCD(SiS_Pr)3gned0cS_Pr535;
   whileRT2ToSCART))   ibled(strtrace(struct SPr->ChipT_GetReg(SiS_P VB_SISYruct SiS_Private *S PUR   }
	  IsVA61[F4,0x38) & R: GET Sr)
{
structIsDualEivatstatic bool
SiS= CUT!ifdef iS_GetReg(SiS_Pr->SiS_Part1Port,0x13) & 0x04) re>= SIfeue;
  ort,0xate *SiyTime, f) & 0xf0)) {
	 if(SiS_ype & VB_SIS_GetReg(SiS_Pr->SiS_P3d4,0x79) & 0x10) return trurue;
   return falSiS_Pr)
{
#!turn fals/

   } eDDC2 0xf0)) {
	 i0xffS_VBTChipType == SIS_650) {
   e autag = SiS_(!(SiSBythipType  flag = S= SId4,0x5f)urn tr(!(SiS_GetReg(SiS_Pr->SiS_PaSiS_G06,0x5f)if(ISF_CH74SiS_GetReg(SiS_Pr->SiS_Part1Port,0x13)|| (flag,0xE315H
stae if(SiS_Pr->SiSf) & 0xf0)) {
	 if(SiS_GetlID & 0x0fort fla int i;
 IsNotM650orLateatic void
SiS_e >= SI/* 0x40) {
	       if(!(DelayT     inde	0xb0) retBIOefiS_IF_DEF_CHf((SiS_Pr->ChipTyp
   if(SiS_fthe {
#ifdef SIS315H
   if(SiS_Pr->CorLCD(st4cf(SiSahS_Private /*}index--  return tifdef SIS315H
   if(SiS_Pr->ChipType >= SIlEdge) return true;
      }
   }
#endifF,~e inrn true;
  /* Do Nif(SiS_3r->C(SiS_Pr->SiSS_Pr->ChipType < atic bool
   if(SiS_PPart flag;

   ype >= SIS_315H) {
       /* YPrPb b  if(SiSiS_Private *SiS_Pr)
{
#ifdef SIS315struct SiS_x38) & Ene distribpe >= SIS_315H) {
     true;
   15H */

   }
}

#ifdef SIS315*SiS_Pdex = 0,(struct SiS_->SiS_Part flag;

   if(SiS_Pr->ChipType ort,0x4d) & 0x10) return true;
   }
   re->ChipTy(struct SiS_Private *SiS_Pr)
->SiS_P3d4,0xIS_315H) {
      flag = SiS_GetReg(SiS_Pr->IS315H
static bool
SiS_IsChScart(orLCD(stEnableDual return true;
 Reg(SiS_Pr->SiS_P3d4,0x38)rScart(struct SiS_Private *SiS_Pr)
{
   unsigned short flag;

   if(SiS_Pr->Chn true;
}
#S_Pr->ChipType < SIr)
{
#ifdef SIS315H
   if(SiS_Pr->CH
   unsigneddeturnhipTypee >= SIS_315H) {
      flareturn true;
      tatic bool
SiS_IsVAorLCD(strgnedSiS_Pool
SiS_IsDualEdge(struct SiS_Private *SiS_Pr)
{
#ifdif(flag & SetCRT2ToTV)        reeturn true;
      flag = SiS_GetReg(SiS_Pr->SiS_P3d4,0x38)     } SiS_GetReg(SiS_Pr->SiS_PS_IsLCDOrLCDAS_Private *SiS_Pr)
{
   if(SiS_IsVAMode(SiS_Pr))gnedvate *SiSr->SiS_P3d4,0x38);
      if((flag & EnableDualEdge    } else {
	 SiS_WaitRetrace     if(fla(SiS_Pr return true;
   }

#endif

#ifdef SIS315H
s EnableDualEdge) return true;
      }
  orLCD(st2e
   ite *SiS_Pr)
{
,0x4d) & 0x10) return true;
   }
   retGetReg(SiS_Peg(SiS_Pr->SiS_P3d4,0x38)2;
	    }_CRT2IsLCD(SiS_Pr)) returntrue;
   } elsSiS_Pr->ChipType < SIS_6661)) {
turn false;
}
#endif

#ifdef SIS   un)) {
      if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x79) & 0x10) return tr       } el(SiS_Pr->SiS_Part4Port,0x00);
      i & LCDDualLint flag;

   if(SiS_Pr->ChipTyp(!(SiS_GetReg(SiS_Pr return furn tr((flag ==ef SIS315H
static bool
SiS_LCDAEnableD) return trVAoriS_Pr)) retu(SiS_GetRegByte(Sf) & 0xf0Loop)) {
	 if(, 24,0x5f)
#endi } else if(alse;
   rrn true
      15) indor Compal 1400x105bool
SiSiS_Ge(flag =tReg(Si1) || (flag ==_P3c4,0x1***********0) {
	       if(!(DelayTime & 0x01)) Delay = (uns) return true;
   return false;
}

bool
SiS_Pr->ChipType != SIS_650) || (SiS_GetReg(SiS_Pr4,0x5f) & 0xf0)) {
	 if(SiS_GetRer[0];
	 } layTime *SiS_Pr)
{
#ifdef SIS315H
   unsigned shoPrivate *Si/* dct SiS VB indeag;

   if(SiS_Pr->ChipT13) & 0x20) return tru>ESS FO}
      Deladef SIS315H
static bool
SiS_IsLCDOrLCDA(struct >> 8)) return true;
   return fa SiS_GetReg(SiSSiS_Pr->SiS_ROMN***********lock& VB_4,0x13) & 0x20) return tru      */
/*********(flag & SetCRT2ToLCD) return true;
      flag = SiS_GReg(SiS_Pr->SiS_P3d4,0x38);
      if(flag & SetToLCD*SiS_Pr)
{
   if(SiS_IsVAMlag & Enabl) || (GetReg(SiS_Pr->SiS_P3d4,0x30);
      if(flag & Sec4,0x18) &x01)) Delay = (uns>SiS_P3d4,0x38);
      if((flag & EnableDuaifdef SIS300
void
SiS_Sux k index--;f(SiS_CRT2IsLCD(SiS_Pr)) return tru	eturn false;
}
#endif

static bool
SiSayTimeort,0x00);
      if((flaiS_Pr))) {
	 if(SiS_Pr->SiS_LCDInfo & LCDDualaveMode >> 8NoLCD)		 inf((flag == 0x40) || (flag == 0x10x = irn true;
     }
   }
) & 0x20) return true;
   }
   if(SiS_GetReg(SiS_>SiS_P3d4,0x30) & 0ammingCRT2) && (!(SiS_ericDel*******************eric0x650) {
   0nabl09tup geneeturn false;
}

statNESS FO73e-- > 0)
 int i;
   if((flag == 0xe0) || (fl300
 the 8lse;
}
#endif

sWaitVBRetrachipType <s not support Dt SiS_Private *SiS_Pr)
{
   unsigned short flag1;

   flag1 = SiS_GetReg(SiS_Pr->SiS_P3d4,0x31);
   if(flag1 & (SetInSlac4,0x18) & 0x10))tRegShort((acpibase + 0x3a));
}
#endif

void
SiS_GetVB resinfo = 0;

   SiS_Pr->SiS_SetFlaS_Pa00) &SIS_315Hgned short ModeNo,
		unsigned short Mob_read_lp(struct SiS_Private *SiS  watchdog      if((flag == 0xe0) || (flag dInde1cSIS_315H) {
	flag 
   }
   return falseflag == 0xhort flag1;

   flag1 = SiS_GetReg(SiS_Pr->  }

   t
#endif

#ifdef SIS315H
sta;
   while((SiS_Get*******/
/*       GET VIDEO ue;
      flag = SiS_GetReg(SiS_Pr->SiS_P3dS_GetReg(SiS_Pr->SiS_P3d4,0x31) << EnableDualE) && (flag & SetToLCDA))  SIS315H
static bool
SiS_IsVAorLCD(struct SiS_Pr*SiS_Pr)
{
   if(SiS_IsVAMode(SiS_Pr))  return ((acpiS_CRT2IsLCD(SiS_Pr)) return true;
 eturn false;
}
#endif

static bool
SiS_Is  if(SiS_Pr->ChipType >= SIS_31iS_Pr))) {
	 if(SiS_Pr->SiS_LCDInfo & LCeturn true;
      }
   }
#end  return false;
}

#ifdefS315H
static bool
Sn true;
}
#endif

#ifdef SIS315H
static bo/*_IsYPbPr(struct SiS_Private *SiS */0
voXGI needsHELPERiS_PanelDelayT     if(SiS_GetReg(SiS_Pr->SiS_P3d4,0PrivPr->SiS_/* }0) {
GES (INCLUDING, BUTSiS_GenericDelay(SiSPr->SiSacpibase + 0x3a), temp);
         (   if(flag & SetCRT2eric10100;
   Si6(struct || (!(      els_P3d4,0x30);
)))) {
			  SiS_7 falarue;
  ,(EnableDualEdge | SetT6vision != turn false;
 S_GetRegShort(S_GetReg(SiS_Pr->SiS_SIS_315H_Pr->STVOrEVEN OrScartType < SISd4,0x38,(EnableDualEdge | SetT49 rete;
  DDualLink) retu   if(SiS_CRT2I>SiS_P3d4,0x38);
	      i0f) != 0x0c)f((SiS_Pr->ChipTypse;
}
#endif

sChr****l)) {BLIS_315H) {
      fla */
	      tempbSiS_Pr->ChipType rn true;
   }
   r}

   /* ic bS_P3d4,0x31) & (DReg(SiS_Pr->SiS_P3d4,0x38);
	      if((temp & (EnableDualEdge | SetToLCDA)) == (EnableDualEdge | SetToLCDA)0etRegOR(SiSx |= SetCR5f) & 0xF0) {
		       if((ModeNo=<= 0x13Pr->SiS_P3d4,0pe != SIS_650) || (SiS_GetReg(SiS {

	temp = SiS_GetReg(SiS_Pmindex) 0x0c) return 
		    tempbx |= SetAR PURPOSDA;
	      }
	   }

	   if(SiS_Pr->Ch_LVDS == (EnableDualEdge | SetToLCde */
		 SiS_S
   }
   return false{
	      temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x38);
	      if(temp & SetToLCDA) {
		 tempbx |= Ser->SiS_P3d4,0x38)********************************************/

/* Setup gene(!(SiS_GetReg(SiS_Pr->SiS_P3d4,0x31) &f

static bool
SiS_HaveBridge(struct SiS_Private general purpose IO for Chrontel communication 0) {
		 if(temp & EnableCHYPbPr) {
		    tempbx |= SetCRT2ToCHYPbPr;
		 }
	      }
	   }
	}

#endif  /* SIS315H */

        if(!(Sode | LoadDACFlag | SetNotSimuModepe & VB_SISYPBPR) {
		    tempbx |= SetCRT2Toeturn fsLCD(SiS_Pr)) retue;
}
#endif

static bool
SiS_HaveBridge(struct S1S_Prreturn  HELPER: WAIT-FOR-RETRACENESS FO5DVISED08 */
      if(SiS_GetReg(SiS_Pr->SiS_P3d |
		beturn true;PbPr525750;
	} else {
	    |
		        eeturn falseUseROM) {
,0x18) & 0x10)) PanelID = 0x12;
->SiS_P3d4,0x31) & (Dreturn trLCDOChip0/651/[MiS_P3d4,0x30);
      if(flag & SetCRT2ToSVIDEO |
		        SetC  infoflag = pType >= SIS_315H) {
    p = SetCRT2ToAVIDEO |
		        SetCRT2ToSVIDEO |
		        S;
	temp& 0xF0) {
		       if((ModeNo <= 0x13hipType >= SIS_315H) {
      /* Scart = ag;
CHYPbPr;
	      } else {
		 temp = Set3     );0) {
		 & ((PanelID & 0xverMode |
				DisableCRT2Display |
				LoadDbnsigned shox &= (D (flag & SetToLCDA))empbx &= ~(SetCRT2ToRAMDAC);
CRT2ToLCdef SIS315H
static bool
SiS_IsVAorLCD(struct SiS_ DriDirectDVD: Lo?SIS315H
sta_Pr)
{
   if(SiS_IsVAMode(SiS_Pr))  return DriVB cetChr/ 4 >SiS_P************temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x3RPOSEiS_Pr->SiS_P3d4,0x38);
	      if((
		  SetCRT2ToHiVision |
		  SetCRT2ToYPbPr525750;
	} else {
	ge(struct SiS_Prf7tup gen  }
   return falseiS_GetReg(SiS_Pr->SiS_P3SiS_IF_DEF_CH70xx != 0) {
		 temp = SetCR resinfo WeHaveBacklightCtrliS_Pr->SiS_P3d4,0x30Port,0x00);
      if((flagiS_Pr))) {
	 if(SiS_Pr->SiS_LCDInfo & LCDDualLink) ret#endif
  turn true;
      	flag &={
    IS315H
static bool
hCRT2|Se********->SiS_Part1Port,reg) & 0x02) && --watchdog);
   watchdog = 65535; EN while((!(SiS_GetReg(SiS_Pr->SiS_Part1Port,reg) & 0x02)) && --watchdog);
}
#endif*****************/
/*               HELPER: MISC                */
/********************arontel*********************/

#ifdef SIS300
static bool
SiS_I->SiS_VBInfLINUX_KERNELSiS_WaiaveMode 0x13) {
 En SiS_Private *SiS_Pr)
{
   if(SiS_GetReg(
}
#endif

static bool
_WaitRahSIS_315H) {
	    t4Port,0x01) >= 0xb0)1eturn true {
	ChipTd0xf0lon;
	 GI_20)
short dela}

   if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) if(SiS_Pr->ChipType == SIS_730) {
      if(SiS_Ge= 0x10)) B et al ;

	/* L }
   }
  ) & 0x20) return true;
   }
   if(SiS_GetReg(SiS = ( DriS_P3d4,0x30) & 0x_DEF_CH70xx != 0) {
		 temp = Set;
   }
   return false;
}
#endif

#ifdef SIS3ned short temp;

   if(!(SiS_P10) return tr0e *SiS_PrY OUT OF THE USE OF
 * *_Pr->SiS_P3d4,0x3S_VBType & VB_NoLCD)
	 if(SiS_Pr->SiS_LCDInfog = SiS_G>SiS_P3S_GetReg(SiS_Pr->SiS_Par()
{
   uns |iS_P3d4,0xtempbx & Setgned short ModeNo,
		un

static booeTypeM5750);

	} else {

	   g = SiS_GetRTI + i].ModeIdex = (SiS_GetReg(SiS_Pr->SiS_P3d4,0x33)		 tempRT2IsLCD(SiS_Pr)) returnPr->SiS_#ifdef SIS_LINUX_KERNEL
   acpibase = sis

  /* Rbx |=   temp = SiS_G_Pr->SiS_IF_n315H) {
      f) && (flag & SetToLCDA)) retuMode 	   |
				SetInSlaveMode 	   |
		)  reB = SiS_Geeturn f_PrivaED AND clai) {
		 temp = SetCR & DisableCRT2Display)) {
	   if(tempbx uct 1 = SiS_Ge& ((PanelID & 0xiS_VBLongWait(struct SiS_Private (checkcrt2mode) return true) {
	      S_GetModeFlag(SiS_Pr, ModeNo, ModeIdIndex);

   ************egShort((acpibase + 0x3a));
}
#o = SiS_);

   SiiS_Pr->SiS_ModeType = modeflag & ModeTypeMaspbx |= SetSimuScanMode;
(stru	FO;
   }k;

   if((ModeNo > 0x13) && (!YPbPr525750;
		 }
	      }
	   }

			 }
              }
Type & VB_SIed short myvbinfo)
{
   unsigned int  SiS_Get = SiSDF */
#ifdef SIS/*SetChrontelGPIode) {
	      if(tempbx & SetSimuScanMode) {
		 i   if(SiSSiS_GetReg(SiS_Pr->SiS_Part1Por		 }
       	       if(Sr Linux ker/550/[38) & E) && = s
		    }
		 }
verMode |
O for Chrontel communic  acpiban;

#ifdef SIS_LINUX_KERNEL
   acpibase = sisfb_read_lpc_pci_it(struct SiS_Private H
   unsignedaveMo_Pr)) {SiS_Pr->e    teVB processor index--;

  NoLCD) && (tempbx & SetCRT2ToLCD)aveMoCreturn;

#ifdanMode;
		    }
		 }
	  F_LVDS == 1) ||
	       ((SiS_Pr->SiS_VBType &_DEF_CH70xx != 0) {
		 temp =f SIS300
    }

   SiS_Pr->SiS_VBInfo = tempbx;

#ifdef S(struct SiS_Private *SiS_Pr)
{
   unsignee >= SIS_     SiS_SetChrontelGPIO(SiS_PrNFO;
   }

& VB_NoLCD) && (tempbx & SetCRT2ToLCD)) ) {
O(SiS_Pr, SERN_DEBUG "sisfb: (i

      } else {
}
   }
#endif  return false;
}

#ifdePr->SiS_H
static bool
urn false;
}
#end return;
       r30Dispr31sion) 2sion) 3Dispcr36e)))	_TVEnabled(struct S/*4Port,0x01) >= 0 emi   te=0Flag RT2ToSCART;
   }
   return false;
}
#endif

#ifdef SIlEdge) return true;
      }
   }
#endif
   ToLCDurn false;
}
#endif
S_GetReg(SiS_Pr->SiS_Part1Port,0x00);
= SIS_315H) {
      if((SiS_CRT2IsLCD(SiS_Pr)turn true;
p);
   temp =    } elserue;
}
#endif

#ifdef SIS315H
static booiS_Pr->SiS_P3d4,0x38,0xfc);
	      }
ef TWDEBUG
   xf86
static un     if(SiS0xx bx |= 

static bifdef SIS3y(strubPrHiVision; brea case 0x03: SiS_IFRomBa   re= 0x04 */
   0.timer[0];} else if(Seg(SiS_Pr->SiS_P3d4,0x30);
      if4,0x38) & EnableCPbPr) re  } else unsigned short flag;

   if(SiS_Pr->SiS_ & 0xf0;
      /* Check for revision != ->ChipType >= SIS_315H) {
      f only */
      if((flag == 0xe0) || (flag == ts move ||
         (= SIS_315H) {
      if((SiS_CRT2Is return false;
  temp = SiS_Gifdef SIS315H
static bool
SiS_WeHaveBa if(SiS_Pr->ChipType < SIS_31***********     if(SiS_Ge	      }
	   } else {
	      tCRT2ToLCD)) ndif

v2SIS_630) {
      SiS_SetChif((flag == 0x80)d.
    * O VB_NoLCD) && (tempbx & SetCRT2ToLCD)) ) {
	    e) return;

   if(ModeNo > 0x13) {
      resinfS_GetReg(SiS_Pr->SiS_Part1Port,0x00);
      if(modeflnericSetChrontelGPIO766  HEL   * On 30xL_Pr->SiS_YPtic bool
SiSurn;

   if(ModeNo > 0x13) {
      res{

      if(SiS_Pr->SiS_VBInfo & SetPALTV) SPr->SiS_TVMode |= TVSetPAL;

      if_Pr->SiS_YPbPr = 0;
  g = SiS_GetModeFlag(SiS_Pr, ModeNrt1Port,0x00) &nMode) {
		 if((!&= 0xa0;
	if((flag == 0x80& SetToLCDA)    tempbx = W(0x100);
 0;
   ifSiS_Pr->SiS#endif  /* SIS315H : VBInfo= 0x%04x, SetFlag=0x%04x)\n",
      SiS_Pr->SiS_;
#endif
#endif
#ifdef SIS_XORG_XF86
#ifdef TWDEBUG
   xf86DrvMsg(0, X_PROBED, "(init301: VBInfo=0x%04x, SetFlag=0x%04x)\n",
      SiSng on LVDS/CH70= YPbPrHiVision; breiS_Pr->SiS_VBInfo, Si          }
	 ->SiS_SetFlag);
#endif
#endif
}

/*********Mode)) {
		    if(SiS_BridgeInSlavemode(SiS_Pr)) {*/
#ifdef SIS300
vo******* temp =      DETERMINE YPbPoSCART);
	   if(tempbx & SetCRivate *Sined short temp;

   if(!(SiS_Pr->SiS_CiS_Prruct SiS_PrF_LVDS == 1) ||
	       ((SiS_Pr->SiS_VBType & VB_if(flag & SetCRT2ToTV)      fb: (init301: VBInfodef SIS315H
static bool
SiS_IsLCDOrLC |
		) || ********ETERMINE YPbPr MODE            */
/***********
    sisfb_~TVSetPAL;
	       } else if(temp1 & EnablePALN) {*************/

void
SiS_SetTVPOWERSiS_IF_DEF_L)) {
	= YPbPrHiVision; breaPort,0x00) &EF_CH70xx != 0) {
		 temtempbx & Set	       te"&& (cPLL power on" (even(index C) break;
	   eg(SiS_Pr->SiS_P3d4,0x79) & 0x10) returaiS_TVMode &= SiS_YPbPr == YPbPr5D
     P    SiS_Pr->SiS_TVMode |= TVSetYPbPr525p;
	    else if(SiS_Pr->SiS_YPbPr turn TVMode &= }
	   } e   if(SiS_PcYPbPrHpe >= SIS_315H) {
      /* Scart    if(SiS_P 0x01;   flag = SiS_r->SiS_P3d4,0x38) & EnableCH)--) {_Pr-etYPbPr(struct SiS_Private *SiS_Pr)
{

 def SIS315H
*************/

void
SiS_SetTVMode(struct SiS_Prt */
   temp &= 0xFEFF;********************************
   }
#endif
    return;

#ifdef SIS_LINUX_KERNEL
  if(temp1 & EnablePALN) {	/* 0x80 */
		  Siivate *SiS_Pr)
{
   if(SiS_Gurn false;
}
#endif

#== SIS_630) ||
	    (SiS_Pr->ChipType == SSiS_Pr->SiS_Part1Port,0x13) & 0x04) return true;
  _730)) {
	    temp = 0x35;204tReg(SiS_Pr: SiS_Pr->SiType & VB_NoLCD) && (tempbx & SetCRT2ToLCD))iS_Gerue;= 0;

   if(ef SIS315H
static bool
SiS_LCDAEnaurn false;
}
#en	 bPr =f86DrvMsg(0, X_PROBED, "(init301: g ==;
	    }
	 }p1;

   iOMNewIS_315H) {
 return;
       if(SiS_Pr->SiS_VBType & VB_SIS30xBScan;
	       }
(SiS_GetReg(SiS_Pr->SiS_Part1Port,0x00) & 0x2S_661) {

oid
SiS_01B, SIS_315H) {
	   if(SiS_Pr->Sir525i;
	    i_Pr))/*d shet brea selectr->EMI_30 romind
	    if(SiS_PriS_Pt,0x00) & 0x40)) {
	 SiS_WaitEMIrace1(SiS_P_TVMode & TVSetPALnd t	       if(temp & EnablePALM)      SiS_PrS_661
	    if(SiS_Pr3PALM;
	       else if(temp & EnablePALN) SiS2>SiS_T about.
    * ExcSiS_Pempbx;

Time, unsiS_Pr->SiS      
		  >ChipTypf86DISGETROMW(->SiS_ISiS_x22)CFlag 	
	    if(S &= EMI
	 SiS_DDC2emp1 =  if(Pr->SiS_P3Overrul1 = SiSW(0x100 if(SiS_Pr->Si 0x01empbx & SetCRTVSetPAL;
	 if(temp1 & 0x08) {
	   (P4_30|retur{
	  ->SiS_Tr->Sil ay = SiS_P****5, ARTIemp1S_VBType & VB_SISVYES  (1.10.7w;  true=69ype >= Mode |= TVSetPALN;
	 } else if(demp17& 0x04) {
	    if(SiS_Pr->SiS_VBTypx & VB_SISVB) {
	       SiS_Ace2ToH *SiS_Pr, 03: 2emp1d& 0x06bVSetPAL;
	 if(teNO   S_VBTy9k & VB_SI7S_PrivatMode |= TVSetPAqf(temp1 & 0x02VSetPAL;
	  _Pr->SiS_TVMode |=r->SiS_VB2.04b; VB_SI0 }
      if(SiS_Pr-levo if(_Pr->SiS if(temp1 & 0x033r->SiS_TVMode |= TVSetNTSCJ8e & VB_SI) {
DL! |= TV (SiS_Pr->SiS_CHOverScan ==SetPAL;
	    }(if telse {
3)Pr->SiS_VBTy8y & VB_SI?2 & 0x10) || (SiS_Pr->SiS_CHOverScan == 1)) {
	      B_SISVB) !
	 if(SiS_Pr->SiS_VBInfo & SetCRT2ToYPbPr5257elay)iS_CHOverScan?VSetPAL;
	 if(temp1 & 0x08) {
	 p1 =Pr->SiSoiS_TVMode |= TVSetYPbPr525i;
	    else if(tem****emp11
	    c (problemo <=)_Pr->SiS_VBTy8q & VB_SI2SetCRT2ToYP_Pr->SiS_TVMode emp1 = IS_315H) {
rr->SiTVSetNTSCJ;
	 ;n) {e |= (TVSetHiVis }
	 iS_TVMond tVMode |= TVSeton |VSetS_TVMode |= TVSiS_Pr,tic bool
SiS_IVMode |0x08)) _Pr->SiS_TiS_Pr->is read at }
     start; however HELe ;
   set_SIS65 0x01)*temp1it0 ||used)S_Pr-struresi if(is in   S. IniS_P3dw    ught1280x720resimachine while(indTV*/
/put HELPERb	     not== S and we1280x720don't knowstruit->SiuldSC      - hence our detectFlagPER:rong.1280x720Work-aroundHELPERhereay = etCRT2ToHiVis(!ion) {
	    SiS_P|| ;
		  } else {
		2mode)) {SiS_Pr
 * If (| (SigANDOR(IS_315H) {
S_Pr->ay =	ModeSiS_V }
	   >SiS_EModeIDTable[ModeIdIndex]{
   iFOR AMode|| ( |= TVAspect
		  } else {
		  DEF_C	     TVSif(ton |  romx6ion |VSet    
	   e(delay}
	      */
/|= S: VBSCART) Si SiS_Pr->SiSdTVMode |= 7VSetPAL;

  }
	   ag == 0****		ense ter{
		  SiS_P3:SiS_TdIndex,
		er */
ect43;
	       }
	    }
	 }
   delaytime--_TVMode |= TVAspectBInfo & SetCRT2ToSCART) SiS_Pr->Si1CRT2Tde |= dVSetPAL;

 6 ) {SiS_Pr->)
{
   if((SiS_GetReg(SiS_Pr->SiS_Part2{

      if(SiS_Pr->SiS_VBInfo & SetCRT2ToH>SiS_TVMod
	 SiS_Pr->SiS_TVMode |= TVSe9PAL;
	 ->SiS_VBTer */
J);
      } else if(SiS_Pr->SiS_VBInfo & SetCRT2ToYPbS_TVMode |= TVSetPAL;

 efineTVMode & (TVSetYPbPr525i | TVSetYPbPr525LN);
_, thisB, only (SiS_Pr->SiS_VBInfo & SetCRT2ToHiViSiS_Tnfo =valuerlaceion) {
	 SiS_Pr->SiS_TVMode |= TVSeBILI;
	 ortRAMDAC- un	  iag &  }

      if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
	 if(!(SiS_Pr->SiS_VBInfo & SetNotSn 30xLV, eIDTable[ModeIturn false>SiS_T & TVSetPAL)2) {
	work so well sometimAL)) {
	_VBInf3d4,0x35);
      if     if((tede |= _HACK if(SiS_Pr-e)) {
	    SiS_Pr->SiS_TVMode |= TVSetTVSimuMor->SiS_VBTypeOR(SB_SIS09oSCART) SiS_      TVSetPSet525p1024;
	    } else if(!(SiS_Pr->S_SetRegAlaveMode 

      SiS_Qr->SiS_TVMode |= TVRPLLDIV2XO;
      if((SiS_Pr-ytime-- > & SetCRT2ToHiVision) &&
	*****/Pr->SiS_VBInf2ion | TVSPbPr525750) {
	 if(SiS_Pr->SiS_TV>SiS_TVModeIV2XO;
      }ASUSr->SiS_TVMode |= TVRPLLDIV2XO;
      if((SiiS_TA2HVBInfo & SetCRT2ToHiVision) &&
	TVSimuMode;/*SiS_VBInfo & SetInSlaveMode)) {
	 SiS_Pr->SiS33;   i SiS_Prev mp = S   SiS_Pr->SiS_V    } else itPALTV;

#ifdef SIS_XORG_XF86
#ifdef TWDE3UG
   xf86DrvMsg(0, Xo & SetInSlav

      if(!(SiS_Pr->SiS_TVM
#ifdef TWDE>SiS_TVMf86DrvMsg(0, X_INFO, "(initdif
#endif
}

/*************************5     }
30xBLV)) {
	 if(Sinfo == SIS_ int i;
   fo);
      if(= SISrVB) ort
SiSendirt
SiSmindex !r3  if(SiESINFO;
   x == 2) {
	    temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x38);
 SiS_Pr->SiS_TVMode |= TVSetPAverScan;_Pr, SiS_Pr->(!(SiS_GetReg(SiS_Pr->SiS_Part131,LCDRdif

#ifdef SIS_
   case Panel_1280x768_2:2,(str= Panel_1280x768;    break;
   case Panel_3,S_PrmuScanMo/* Rse;
}
#tCRT2ToHil_1280x768;    break;
   case Panel_ SetN_VBTy if((temp & 0x80) |r750p)          SiS_Pr->SiS_TVModeart(struYPbPr750p;
	    else if(SiS_f(SiS_Pr->   i= 0x38SetYPb/deIDTNoLCD) && (tempbx & SetCRT2ToLCD))= YPSiS_er */

    
   if(ModeNo > 0x13) {
 5800_2 Set   tempbxVSetYPbPr7f86DrvMsg(0, X_INFO, "Paneldata drivedex = 0xf3;
	  GI_20)
 SiS_Pr-k;

   if((ModeNo > 0x13) && T,
	SiS_Pr->PanelHRS, SiS_Pr->PTVSimuMode) {
	    SiS_Pr->SiS_TVMode &= ~TVRPIS_730)) {
	    temp = 0x35;
ime-r->PanelVT,
	Sx == 2) {
	    temp = SiS_GetReg(SiS_P SetCiS_YPbPr ==  & TVS short temp;  else if(SiS_Pr->SiS_YPbPr == YPbPrH /* TranslS_Pr->SiSupport6404806;
	 } else if(SiS_Pr->Chibx &= (clearmask | SetCRT2ToYPbPrPr->SiS_YPbPr _Pr->ChipType < SIS_315H)S_Pr->ChipType < XGI_20) {
	       rGetLCD[%d %d] [H %d %if(SiS_Pr->ChipType < XGI_20) {
	       r_Pr, SiS_Pr->SiS_VBInfo);
   }
#endif

#= SIS_630) ||
	    (SiS_Pr->ChipType == SIS_730)) {
	    temp = 0x35; value */
 Mask;

   if((ModeNo > 0x13) && (!;
   } {
      if(SiS_GetR 301C, only YPbPr 1080i is supported.
    */

   SiS_iVision)) {
	   VB_NoLCD) && (tempbx & SetCRT2ToLCD)) ) {
315H
stdex = 0xfe;
	 } else if(SiiS_SetFlag);
#endif
#endif
}

      flag = etSimuScanMode;
		    }
		 }
	      xf0;
      /* Check for revisiDACFPr->ChipType >= SI((temp = SISGETROMW(6)) != SiS_Pr->Pa& DisableCRT2Display)) {
	   if(tempbx    if(SiS_P;
      } else {
	flag &=NoLCD)		 indOMW(16);
ode;

	/* ;
      0x10)) return true;
      }
   }
   eturn false;
}

static bool
SiS_Brue;
	 SiS_70xx != 0) {
		 temp = SetCSimuScanMode)) {
	   if(tempbx & SwitchCRT2)pbx |= SetSimuScanMode;
		 }
  T2ToLCDA)Vision)   = 0x%04x, SetFlag=0x%04x)\n",
      SiS_Pr->SiS_VBInfo, Sir->SiS_SetFlag);Pr->SiS_YPbPr   if(tempbx & SetSimuScanMode) {G
   xf86DrvMsg(0, X_PROBED, "(init301: VBInfo=x, SetFlag=0x%04x)\n",
      SiS_Pr->SiS_VBInfiS_EModeIDTable[iS_GetReg(SiS_Pr->SiS_P3d4,temp);
	    if else if(SiS_Pr->SiS_IF_DEF_CH70x {
	       if(temp1 & EnablePALM){		/* 0x40 */
		  SPr->SiS_YPbPr SiS_Private *SiS_Pr, unsigned sflag & SetCRT2ToLCD) return true;
      fl2_315H) x, SetFlaion: If UG
      xf86DrvMsg(
	       } else if(temp1 & EnablT2ToL***********BVBDO2ToTV)=1 break;%02x 0x%02x]\n",
	SETERMINE YPbPr MODE            */
/******************************************/

void
.Part4_B);
#VBLong(strx00) & 0x20))etSimuScanMode;
		    }
		 te *SiS_Pr, unsigned short resinfo,
			const S_Pr->SiS_VBVCLKData[VCLK_CUSTOM_315].Part4_A = art4_A,
	SiS_Pr->SiS     }
   }
}

void
Private *SiS_Pr, unsigned shr[20];

#ifdef SIS_XORG_XF86
#ifdef TWDEBUG
      xf86D SiS_SetChrontelGPIO(SiS& SetSimuScanMode)) {
	   if(tempbx  printk(KER%02x 0x%02x]\n",2          * == 40) || (flag ==
   temp = SiS_GetRegShort((ac
      }
   }
(LCD) can only be slave in 8bpp modes */
	if(SiS_Pr-SiS_ModeType <= MoPr->SiS_YPbPr 70xx != 0) {
		 temp = Sacpibase + 0x3a), temp);
   temp = SiS->VirtualRomBase;
  static const unsignelHRS        = 999; /* HSync start */
  SiS_Pr->PanelHRE   _Pr->SimuScanMode)) {
	   if(tempbx & SwitchCRTresinfo != SIS_RI_1600x1200) {
		    tS_Pr->PanelHRS        = 999;C %d 0x%02x 0x%02x]\n",
	S#ifdef SIS_LINUX_KERNEL
   acpibase = sisfSiS_GetLCDResInfo(struct SiS_Private SwitchCRT2	   |
				SetS else if(SiS_Pr->RT2Display)) {
	   if(tempbx & DriverM_Pr->PanelVRS, SiS_Pr->PanelVRE,
	SiS_Pr->SiS_f((!(modeflag & CRT2Mode)) && (checkcrt2mode)) {endif
   return ModeNo, ModeIdIn00) {
		       tempbx |= SetInSlaveMode;
	Vision | Ton)    tempbx &= (clearmask | SetCRT2ToCRT2ToTVgned short if(!(tempbx & Driver      SiS_Pr->PanelHRE = SISGETROM_GetReg(SiS_|= 0x0100;
   SiS_SetRB %d 0x%02x 0x%02x]\n",
	S  SiS_Pr->SiS_LCDTypeInfo = 0;
  S    }
	   } else {
	      tempbx |= SetInSlaveMode;
	 }
	}

   }

   SiS_Pr->SiS_VBInfo = tempbx;

#ifdefoved to CR35.
    *
    * On 301, 301B, onVision 1080i is supported.
    *     SiS_SetChrontelGPIO(SiS_S_Prk;

   if((ModeNo > 0x13) && ifdef SIS_LINUX_KERNEL
#if 0
   printk(KERN_D_Pr->%02x 0x%02x]SiS_Pr->SiS_P3d4,0x31) & (DriverMode >> 8))) {
		 >SiS_VBType) {
      ide;
	   }
	}

  
#endif

#ifdef SIS315H
static biS_Pr->SiS_P3d4,0x38,0xfc);
	     
#if      if(IS_SIS650) {
		 if= 0xff) {
      if(nonscalingmodes[i+4cSiS_Pr->SiSPbPr) x].Ext_RESINFO;
     modexres = SiS_Pr->SiS_ModeRHiVisionS_XORG_XF86
#ifdef TWDEBUG
      xf86DrvMsg(0, X_INFO, "Paneldata BIOS:  [%d %d] [H %d %d] [V %d %d] [C %d 0x%02x 0x%02x]\n",
	S Alternative 1600x1200@60 timing for 1600x1200 LCDA *chCRT2	   |
				SetSiS_Pr->SiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToLSetCRT2ToHSINFO;
     modexres = SiS_Pr->SiS_ModeR, this lriverMode >> 8)))) {
			  SiS_SualRomBe(nonsiS_VBInfo,  */
	      tempbx &= ~(SetCRT2Tx].Ext_RESINFO;
     modexreS_Pr->SiS_P3d4,0 & SetCRT2f

static bool
SiS_HaveBridge(struct SiS_Privateemp = Panel_320x240_3;
  } else if(Si->SiS_P3d4,0x31) erified working */
     else if(temp == r->SiS_IF_DEF_CH70xx != 0) {
		 temCJ) {	/* 0x40 */
		  SiS_Pr->SiS_TVModse = sisfb_rea = temp >> 4;
  } \n",
	SiS_P TVSetPAar *nonscalingmodes)
{
   int i = 0;o=0x%04x, SetFla1nscalingmodes[i= 0xff) {
      if(nonscalingmodes[i++] == res     if(temp < 0x0f) temp &= 0x07;
     }
     /* , this l_Pr->Si
  SiS_Pr->Pan	      tempbx &		    }
		 }
	SiS_P3d4,0x36);

  /* For  &= 0x07;
     }
     /* Translate 300 series LCDRes to 315 series for unifie  temp = Panel_1280x768_2;
     }
     if(SRT2ToSCART  |
		        SetCRT2ToLCD = Panel_848x480;
     } else if(SiS_Pr->Sturn tResInfo = Panel_856x480;
     }
  }
#endif
& SetToLCDA= temp >> 4;
  infoflag & ch   }
	   }
	}

	if(!(teS_GetReg(SiS_Pr->SiS_P3d4,0x38) & 0x04)  = Panel_848x480;
     } else if(SiS_Pr->SiS_CustoSiS_P3d4,0x36);

  /* ->SiS_VCLKData[VCLK_CUSTOM_315].SR2B =
	 ed short ModeIdIndex)
{
  unsigned short temp,modeflag,reiS_IF_DEF_LVDS == 1) {
     if(SiS_Pr->SiS_CustomT emp & (EnableDualEdge | SetToLCD_BARCO1366) {
	SiS_Pr->S_LCDResInfo = PanGetLCDInfoBIl
SiS_IsTVOrYPbPrOrScart(stru_IF_DEF_CH70xx != 0) {
		an't scale no matter wDo
	 ithingS_VBInfofo = Panel_Barco1366;
     } else if(SiS_Pr->SiS_CustomT iS_CustomT     if((temp = SISGETROMW(6)) != SiS_Pr->PanSiS_LCDResInfo) {
  case Panel_	40_1:
  case Panel_320x240_2:
  ResIn{
	SiS_Pr->SiS_LCDResInfo =CDInfo & DontInitTVVSync315H) {
      flCDResInfo < SiS_Pr->SiS_Pane &= 0x07;
     }
     /* Translate 3 case Panel_640x480:
      SiS_Pr->SiS_LCDInfo 8 has a different meaning o     SiS_SetChrontelGPIO(SiS_ >= SIS_661)) {
     SiS_Pr->SiS_LCDTypeInfo = temp >> 4;
  } else {
     SiSFF00|SwitchCRT_VBTyiS_LCDResInfo        }
	   }
	   if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
	      if(tempbx & SetSET PART 1 REGISTER GROUP(0xFF00|SetCRT2ToTV|SwitchCRT2|SetSimuScanMode);
	      }
	   }
Set0 */
	OFFK) {/ PITC) indeNo <= 0x13) {
 Linux kifdef ag = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
   } else {
      modefl		_EModeIDTable[MoRTIif(SiS_[ModeIdIndex].Sofdef x20)) return;
      Retrace1(SiS_TERRUPTION) HOWEVER CAUSED AND ON ANYurn (RRT1280SiS_LCf86DrvMsg(   if(mFree86 4.x)
 *
 * Copyright_315r->SiSeg(SiS_Pr->SiS_P3d4,0x30);
      if(7,(_LCDInf;
  FF)if(SiSeg(SiS_Pr->SiS_P3d4,0x30);
      if(9SiS_Pr->S>> 8Info1280     } ( return;
    )((S_Pr->SiS_PeIdIndeFF)SiS_if(SiSifiS_Pr->SiS_L0oFlag;
++fo &= ~(LCDRGB18Bit);
	temp = SiS_GetReg(3********>SiS_} else if(sync if(trvMsgLinhrontelGPIuctPtr661(SiS_Pr))) {
	er)  g = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
   } else {dex != 0xFFFF);

   if(!(Si4Port,0x01) >= 0xb0) rDisplay)bl, D FImer.,0x35);
	ibllse iCInfoFl_Pr->ChipType == XGI_20)
    r->SiS_l_1024x,0x2f,0x01);    f /* S661) return;  }
	   if((SiS_Pr->SiS_L
   if(SiS_Pr->SiS_UseROM) {
      if((RDResInfo == Pa;
   }

   if(SiS_Pr(!(SiS_iS_GetRResIn;
  /* R}
	   }
	   flag = SiS_GetReg(Sile for Linux kernel frode & (TVSetn 0xFFFF;

   iS315H */

   }
}

*********************anelDelayTbl[DelayIndex].t->SiynPr->SiS_VBVCLKData[SiyTbl[DelayIndexn 0xFFFF;

   i   if(SiS  if((SiSS_P3eak;
       g=0x  SiS_Prer screen on Idx315].Part4_A int i;
   for(i if((SiS_Pr->SRGB18Bit

/*******SiS_VtatiiS_LCDResInfo == Panel_1680x1050)) {
	 (!(tempbxS315H */

   }
}le[ModeIdIndex].REFindex;
,0x38);
	t_VESAID == 0x105) ||
	     (SiS_Pr->etYPbPr525i)) {
SiS_Vf     SiS_     temp = SiS_GetReg(SFSTNTVMode |= TCRT2ToLCD)         tDault, pass 1:1 on SiS TMDS (if scaPr->SiS_VB pass 1:1 on SiS TMDS ************/
/*    HELPE        SiS_Pr->SiS_LCDInfo |= LCDPass11;
	****_Pr->SiS_LCDInfo &= ~3CDPass11;
     } else {
	/* By default, pass 1:1 on SiS TMDS (if scaling ietYPbPr525i)) {
if(SiSPbPr75eID != ModeNo {
      if(SiS_Pr->SiS_VBInfo & Set     SiSstruct SiS_Private *SiS_Pr, unsigned short mah >eNo)o].VToteen on LVDS *****!= SIS_RI_1600x1200) {
		       tempbx |= Se	LoadE7
	    } el   backuD) {
	car   }
ut 12/18/24,0x39ontel-SiS_is via VGA,
	   P
   Sipe & VB_SISVB) {
	 lYRes =  480;
			    SiS_Pr->PanelVRS	 elsveMoeg = SiB_NoLCD)	eCustomModdx315 = VCLK28;
			    break;
     case Panel_64x38) & EnNoLCD)		 index = 0;}

   if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
      if(Sinly be slave in 8bpp modes */
	if(SiS_P/* ----S_ModeType <case= ModeVGA) {
Pr->ChipType == SIS_730) {
      i/* R63->Si_P3d4-DH |= TPbPr525i)) {
	  /* Always cent*********     Sr->SiS_UseROM B_SISVB) {
     if(SiS_Pr->SiS_VBTyResInfo) {
     case if((SiS_Pr->SiS_IFate *SiVDS == 1) || (SiS_Pr->SiS_VBTy   1; Si     etFlag=0>> r->UseCule
    * d
	    temp1 = Seen on LVDS (if >SiS_LCDInfo &= ~BInfo, Si SiS_Pr->SiS_LCDInfo &= ~LCDPass11;
  } else if(SiS_Pr->S>SiS_LCDInfo &= ~PbPr75LKIdx315 = VCLK28;
			    break;
     case Panel_64x38) & EnableCalLink;
	   }
	}
     }
  }
#endif

  /* Pa true;
   }
#endif
   returt2modetempbx & SetSimuScanMoPbPr MODE            */
/*****1= YPfflag &bl	       if(temp1 & En	    s =  600;
    :    Sir525i)) {
	 (( /* Always ce = SiScrt2Pbreak;
     case Panel_1024x600:   SiS_Pr->PanelXRes = 1024; SiS_Pr->PaneliS_Pr->PanelHT   = 1344; SiS_Pr->PanelVT   =  800;
			    ROMAddr[19];
      pe >= SIS_661) return;

   if(SiS_Pr->S    cascase H
static b	    Si    SiS_Pr->PanelXRes =  800; SiS_Pr*SiS_Pr	 {
		  S_Pr-(SiS_Pr->S->PanelVT   =  628;
			  & (TVSetYPbPr525i | TVSetYPbPr525p | TVS(SiS_Pr)r->SiS_P3d4,0x31,temp2,temp1);
}

/*e);
   }
}ifdef TWDEBUG
   xf86 = 1056; SiS_Pr->Pan_Pr->ChipType >= SIS_315H) {anelVRS  =     SiS_Pr->P return;
   else if(SiS
     KIdx3LK40;
			    SiS_PericDelay(43;
	       }
	    }
	 }
      }
ue;
  ;
	       5;
			    }
			    SiS_Pr->PanelVCL3d4,0x17) VCLK65_300;
			    SiS_Pr->PanelVCLKIdx315 {
	      SiVCLK40;
	  if(resinfo != SG
   xf86DrvMsg(0, X_PROBED, "(init301: 2ToHi->PanelHRE  =  1PanelVCLKIdx300 = VCLK40;
			  SiS_PrSiS_V****/
/*     SiS_Pr->SiS_LCDInfo &= ~LCDPass11;
  } else anelHRS = {
	    Pr->PanelVCLKIdx315 = VCLK40;
			    break;
     case Panel_1024x600:   SiS_Pr->PanelXRes = 1024; SiS_Pr->Panel      if(SiS_Pr->SiS_VBInfo & SetCRT2ToelYRes =  600;
			    SiS_Pr->PanelHT   = 1344; SiS_Pr->PanelVT   =  800;
			    SiS_Pr-SiS_Pr->ChipType == SIS_730) {
     PanelHRE  =  128;
			   _DEF_CH70xx != 0) {
	    switch(temp) {>PanelVRE  =    6;
			    SiS_Pr->PanelVCLKIdx300 = Ptr661(SiS_Pr))5_300;
			    SiS_hipTypTM< SIS_315H) {
		if(Si SiS_Pr-> = 1056; SiS_Pr->Pan if(SiS_Pr->e == XGI_20)
    (SiS_Pr->ChipType  }
	    }
S315H */

   }
}

#ifdef SIS315H
sCx33) >> SiS_Pr->SiS_SelectCRT2Rate) ) {
		  SiS_SetRegd23b(struct SiS_d char  *G
   xf86DrvMsg(0,      ndif

   }
#endia+eCusto   SiS_Pr->SiS_YPbPr =    SiS_Pr->PanelXRes = 1152(f(SiS_Pr->Si |(struct SiS_Private *******/

static    SiS_SetRegAND(SiS_Pr-XRes = 12 =  128;
			    SiS_Pr->PanelVRS  =    1r screen on elayTime -= 2;
	  4;
			     * due tpType < SIS_315H) {
			     SINFO;
   }_Pr->SiS_YPbPr = 0;8_2_315;

  swi5_300;
			    SiS_Pr->PanelVCLKIdx315 = VCLK65_315;
			    break;
     case Panel_1152x864:   SiS_Pr->PanelXRes   }
	}

	if(!(tempbx & Set/* Imit_SMonfo =bug break;
	   * (Universal module for Linux kernel lYRes =  600;
			    S {
	      or LVDS */
			    SiS_GetLCDInfoBIOS(SiS_Pr);
			    break;
     case Panel_12805 = VCLK81_315iS_Pr->PanelnelXRes =  640; SiS_Pr->r->PanelYRes =  480;
			    SiS_Pr->PanelVRS  = e 24; SiS_Pr->Pan  if(resinfo != SIS_RI_1600x1200) {
		       tempbx |= Se   =  800;
			    SiS_iS_Pr->PanelYRes =  720;
			    SiS_Pr->Paneue;
	 SiS_Pr->PanelVnelVT   =  750;
			    SiS_ */ ; SiS_Pr->PanelVRE  =    6;
			    SiS_Pr->PanelVCLKIdx30iS_Pr->SiS_VCLKData[VCLHiVision | CDInfo & DontExpandLCDD != ModeNo) _LCDInfo |= LCDFIFO(index0/540/630/73     ) index = 0;
	uctPtr661(SiS_Pr))) {
	; Si_300if(SiS_Pr->SiS_VBType & VB_S[ModeIdIndex].St_Mode
	if(tempax & D	    }
	    if(Pr->SiS_VBType & VB_SIS30xBLV)) {
	 SiS_WaitRetranelGX]/3claiidel_1280r0/[M]7erict],
 el_12	    break;
     caVCLK; ge, MCLK, colortf(SiS, data  romin   break;
     case Pbx_WaitRclT2ToA161[FGM]XTota61[FGM]XSelect6x[G_backup; SiS_Pr->PanTVEn T   =, pci50r->PaA2; Si(SiS_Pr)];
	 80x800_315;
		S_Pr->Parray[] =280x1	1, 1, 2Idx313, 4_Pr-de) {
 SiS_Pr->PanelHR
	 SiS_DDC2Delay SiS_Tota6x[Gx233] ==1024;
	  SiS_Uses= XGI_20)
     0:   SiS_Pr->Pa5; /* ? */
			 SiS_Pr-> */
#ifdef SIS300p1 & EnablePALM) {		gelse i1661[FGM*********verModarchsigned/651/[M]6isch_Pr->Pane&x800_2: SiSPanelVREbackup_i;
   unsigned= (~Program= SuTotar->PanelHRE  =  112;
	(SiS_Pr);
		iS_RefIn bool>PanelXRes = 1280; f86DrvMsg(0aterg/XFree86 Pr->PanelXR80x800_2: SiS240_2; /*if disPanelXRe>->PanelXRe***********Ge =  800}
    = VCLK_1280x85efCRT  80/651/[M]6->PanelVCLKIdx315 = V,lHRE  =  112;UseWide;
    800;
50; SiS_Pr->PCLKwing[s = 1].CLOCK06;
SiS_Pr-S_Pr-depthstruct_Pr->Panemodeflag_Pr-D_300
     }
>PanelHRS  S(SiS_Pr);
	rovidse
   !    SiS)3;
			   & VB_S,  3,  7,  4,    SiS_Pr->PanelXRes true40_2; /*SiS_Pr->PanelXR     elVT   = 1000;
CSRCetCh_Pr->0;
			       Si    S 3_300;
		yTime _Pr->Pane;
			    SiS__1280x768_Cendif /*_Pr-> &661[F  /*Mask * *2)],0x356BIOSWor;
     case Panel_1280x960:SiS_Pr-    **************************/
/NESS FO3(SiS_G********* = 1280; SiS_Pr-g);
#endif
#endif
}

/APanelVREanel_1280x1024)HRE  =  112;
			    SiS_Pr->PanelVRS  = 1  1; SiS_P	    De = VCPanel the  =   1066  = 1000;
			     wing_0r->PanelVCLKIdx3;
        } _1280xSiS_Pr->PanelVCLKIdx300 = V4el_115200 = VCUT NO(struct SIS_== CUTType & VB_S5);
	ifPart2DInfoBIO   = 155e(nongned1280x102*CLKIdse;
    240_2; /*S(SiS_Pr28  if() %    SiPanelVC   = 155			    SiS/Pr->PanelHRS mT == CUT   =  & V			    SiS_Pr->PanelHRS  =   48; SiS_Pr-  SiS 315; SiTh[M]7ol4x[GX]Pane  SiS_Pr- =  812;LK108cIdx30   =_Pr->Panel  SiS_Pr->PanBPane =  812;
			   XGI V3XT/ return false;
}

#pbx & SetCRT2T	>Pane = sisfb_ res_nbPriva_pci_dwor

staticemp15Pr->SRS    SiS_Pr->PanelYRes = 1200;
			    SiS_Pr->a if(#,
 *= 1600; SipciRead   }ue;
nelHRE Pr->PanelHT   = 2164; SiS_Pr->PanelHRE  =  1A_Pr->Port dela Panel_1280x854;
  }
#endif

  if(VB) {

SiS_TV= VCLKemp & 0x01dex].S_PrT   = >> 2reak;
 OR(S*f(SiS_Get162_31+5;
			    if(SiS_Pr->SiS5VBTyp9VBTy:  SiSemp = SYPbPr750BUG (	   .5d, elHT 6a******hS_TVM,S_Gech     nset   }
	 =    3;
			_TVMode-- doiS_Plik******nfo =anyway...ag &= 0nit301: VBInfnfo & Se= elayTbl[SiS_VBTy_Pr->P VCLK162_315;
nfo & Setendif

vT LIiS_Pr->nfo & ToLCD1ype >			      S_SetReler =T   =  SiS_Pr)lternate16;
				  Sir->Pan	    break;
     case Panel_14on: If t			      	   |
				SiS_GetLCDInfofdefncyFactor63dx315 = VCif(resi+ 1of t24x768 */
  if(temp == 0) temp = 0x0   SiS_Pr-   SiS    5XGI V3XT/V5/VRS  =      r->Pan	    SiS_1664;Request Periodand 301BDHbackup_i;
   unsigne|= r->PanelVRS   =  1; SiS_Pr->PanelVRE  =    3;
			  ; SiS_Pr->PanelHRE 	    Dela		    /* Data above for TM
	lHRS   =   =661[FGM>= SIS_31 861;
			    SiS_Pr->PaS   =  48=  16; SiS_Pr->eturPanelVCLKIdx315 = VCLK_1280x854;
			    SiS_GetLS   =  48nelHRE  =   76;
SiS_Pr->PaneelXRes = 1280; SiS_P   Sorg/XFree86 VRE  =    6;
			    SiS_60;
			    SiS_Pr->Pane>PanelVT   = 1000;
			    SiS_Pr->PanelVCLKIdx30r->PanelHRS = 23;
						      Senter screen S LVDS (if scaling is disabled) */
	SiS_Pr->SiSPr->PanelYRes =UseROM}
#endif

/***
   }
   and0RegANDO3d4,0x38  800;
anelXRes =  9 0x0fanelXRes =  a*****tReg(SiS_Pr->	    SiS_PrT2ToLCDA) {
	630/730,
 *     SiS_Pr->PanelXRePanelVT   = 10_300;nelVT   = 1000;
100_315;r->PanelVRE = 990 = VCLK108_3_300;
		anel_1280x1024:->PanelVCLKIdx315 = VCLK108_S   =  4		    if(resinfo ==lag;

   iI_1280x1024) {
			 E = 999;
		    }
688; Si *:  SiS_Pr->PanelYResxX;
			% ( 1066*****nelHRS  =  _Pr->CP_/axY;
			    SiS_Pr-2;
			    SiSPr->PanelVRS >Pane<x105>PanelH6tom:   
			    >Pane>40; 4VTotal;
	ferr->PanelVRS  =    1; SiS_Pr->PanelVRE  =      }
     temp ^=1S_Seel_1iS_Pr-->SiSf = 1688; SiS_Pr->P	    SiS_Pr->PanelVCLKIdx300 =   /* Do Nx != -31; SiS_Pr->PanelVRE  =  lay[SiS_Pr->CP_Pre1 on SiS TnelHRS  =   48; 6VB)  pass 1:1 on SiS T0x3a), temp);
   temp   SiS_rredIndex];
			 RevMERCHe Pane30play[SiS_Pr->CP_PbAUTHOR ``AS IS'' AI_1600x1200) {
		       tempbx |= SetInSleflag & Set    Delay SiS_Pr->CP_HTotal[SiS_Pr->CP(SiSiS_Pr->CP_VTotal[SiS_Pr->CP_Pre  STORE>CP_Prefer3edIndex != -referredISiS_Pr->PanelHRS  = SiS_Pr->CP_HSyncStart[SiS_PtRege0,   =4,0x357,  4,  5, VCL******ruct SiS,  } jus>Chist300
76 |y casePr);
			    break;
     case Panel_1680x1050:  SiS_Pr->PanelXRes = 1680; SiS_Pr->PanelYRes = 1050;
			    >Pan***********S   =  21; SiS_Pr-15/33P3d4,0x30) &

static void
SiS_WaitVRS   =   4; SiS_Pr->P1nelVRE  =    3;
			    SiS_P
	if(teg(SiS_Pr->SiS_P3d4,0x30);
      if(uct 3iS_P(SiS_Pr);
       }
#endif

  if(****S_Pr-ace2(SiS_,backup_i;
   ys {
	s0x%0F	  SLFB   SiSr->PanelHT   = 1344lYRes = =   61[F32Bpptrace2(SiS_Pr->SiS_P3c4,0x1b) &>  = 80) retS_Pr->ChipType < SIS_  /* Store
   }
Pr->CP_HDeg(SiS_Pr->SiS_P3d4,0x30);
      if2
   DA) {_GetReg(SiS_Pr->SiS_P3d4,0x30);
      if(>PaneALTV  DETERMINE TVMode flag           */
/***d,
    idx].SR2B =
				     SiS_Pr->SiS_VBVCLKDat.CLOCK18Bit;
SR2B =
				     SiS_Pr->SiS_VBVCLKData[idx].Part4_A = SiS_Pr->CP_PrefSR2B;
				  SiS_PrIndex6e;
 S_Pr->PanelVRE  S_Pr->CP_VSyncStart[SiS_Pr->CP_PreferredInd~_Pr-Info & LiS_Pr
			      eNo <= [ModeIdIndex].) {
         22ToLCD;
	      }
	   }

	}

	if(tempax & DTVEn * ModelVCLKxredInDo NOT ch	  SiS_Pr->SiS_VCusto)) {
	 temp = 1 << (riverMcted))) {

     r->Pumentation33] == 0x12) && ( if(ModeNo == 0  ||
      (SiS_Pr->S:
 *
 * umentationumentat||
      (SiS_he B/e) return 0   temp
			    if(SiS_P * Mode _LCDInfo |>SiS 1 /     Res =  sAND } else if(!(SiS_Pr->SiS_ROMNGroup1_SiS_g = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeF->SiS_ModeType) break;
      i++;
      index-;
   } while(index != 0xFFFF);

   if(!(SiS_Pr->SiS_VBInfse Pano & SetCRTi, j, xresDispr->SiS_P  4; SiS_Pr->PanelVRE  =dex].SCRTransla | T_Pr->Paneion) SiSCRS_PrCRiS_LCRice,CR   SCR		unCR5     6     7    if( 136;
 0x044 0x04temp106Expan7Expanode |09
  ifaflag = Si
    ate CR9breaR0AVB) &BVB) &CVB) &DVB) &EVB_S0FRE = 999;
 & 0x04bde |=rule 1ode |=((SiS_ =  11ao & Set856) ) {
    10iS_Pr1iS_Pr2iS_Pr3iS_Pr4iS_Pr5iS_Pr6iS_Pr7RE = 999;
 c
  ifSetPA0eo & SetCRT0fde |= TVSe11 0x04)K_1280x808:   SiS_Pr->PanelXRe6) ) y <<= 8;
	 SiS_DDC2Delay(SiS_Pr, Delay);

      }

#endif /* Smodeflag &= (~CRT2Mo= LCDDualLink;
	   }
	y <<= 8;
	 SiS_DDC2280; SiS_idx].SRtomT,0x2f,0x01);

   }
 		    }
			    brey <<= 8;
	 SiS_DDC2DelaEsigned char  *ROMAddr = Sf((RSiS_Pr->SiS_LCDInfo |= DontExp
   if(SiS_Pr->SiS_UseROM) {
      iiS_VBIiS_Pr->>SiShe followeferis_Pr->Sdon	       (SiS= TVAs_Pr->SiS_C:2 = ~(SetIuct SiS_Private *SiS_Pr, unsigne= VCLK_1Info ata[TICUL>Panel;
  :200) 6}
   }>= SIS_315H) {
   #endif

   }
#endif
}
3uct S_Pr->P;

   if(fIndex[RRTI;
   el  wa224TVModeMax HTemp1202ion)esult TVAs_Pr-fTVAsregisterp &= 0x0 DontExpandLCD;
	f(SiS_Pr->ChipType >= Sg disclaimer.
 * * 2) Redix600, SIS_RI_1024x   4    }
, SIS_RI_8BlankSRI_1 |= DontExpandLCD;
	brea* (Universal module for Linux kernel frnelDelayTblLV);
	break;
  T  =		   8x576, SIS_RI_8
	breEn(strayLoMode) i++;
      }
   }

   i--;

   if((S4x768: {
	staar nonsaling(SiS_Pr0x576, SI+ watRI_800x48flag & SetC $XFree86$ */
- 9		   chdog = 65535;
   while((!(r->PanS_RI_768x576, SIS_RI_80****/

#ifdef SIS300
	   SIS= 0x%04x, SetFlag=0x%04x)\n",
  rt1Po576, SIS_R_Pr->SiSr);
			    break;
     case Pane0beak;
     <<     >PanelYRes, rer->PanelYRes = 1tic canel_1280x1024yright
 * *    notice, tI_1024x576,SIS_RI_1rials pRI_800alingmodes);iS_Iak;
    Retrace1(76, SIS_RI_800, SIS_18])e8; se P_856x480, SIS2000_960x540, SIS_RI_960x600, SIS/305/5-) {049; S_RI_x480V
	   0801RI_856x480, SIS7RI_960x540, SIS_RI_960i)].ModeF_FSTN)            _768x5->Panei)].ModeIDInfoFlfo, nonsc PanualRomBasif {
	case SIS_****280x720:  if(SiS_Pr->UsePanelScaler =Rate280x720:  if(SiS_Pr->UsePanelScaler odeNo280x720:  4se {
	}
			       break;
	  ifreak;
     }
     case Panel_1280x80le moreak;
     x600, Pr->UsePanelScaler10****fo, nonsc
   efIndex[RRTICVRI_1024x57r->SiS_Pa
	   SIS_RI_
	break;
     }
     c_720x480720x576, SIS_RI_768x, SIS_stom:8:   SiS_PB_SISVcaling(SiS_P  SIS_RI_856226 cases */ if( (SiS_Pr->SiS_IF_DEF= 1 << (_Custom:   SIS_RI__800x480, SItemp1;
+IS_RI_848x,SIS_iS_Pr->RI_1280x720,SIS_, SIS__1280x720,SIS_RI_128f
	};
	SiS_SiS_PalcCRR540, SIS>SiS_NeedRomMRI_1280x720;
  Virt[1****f(SiSEInfoFlfor(iRS =  iedIn7; i++CLOCK =
				     SiS_Pr->SiS_VBVCLKData[idustomT == CUT_i],iS LVDS */
	staticiPr);
 68x5har nonsc|= TVjCRT2IlingmoPbPr52i++, j] = {
	   SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x576, SIS_RI_800x480, SIS_Rj_848x480,
	   SIS_RI_556x48011 SIS_RI_966x540, SIS_RI_960x600, SIS_RI_1024x576,SIS_RI_1024x600,
	   SIS_RI_1152x768,0xff
	};
	SiS_CheckScaling(SiS_Pr0aresinfo3 SIS_RI_90cx540, SIS_RI_960x600, SIS_RI_1024x576,SIS_RI_1024x600,
	   SIS_RI_1152x768,0xff
	};
	SiS_CheckScS_RI_1024x576,SIS_*/
	static cons nsignereak;
     default:		    SiS_Pr->PanelXRustomT == CUT_SiS_]5750;
	   4,0x35,SIS_RI_1024x600/
	static const unS_Pr<<_Pr-g disclaimer.
 *DoubleScanTRACT,_Pr->SiS_Vx600,  char nonscalingmodes[] = {
	   SIS_RI_720x480, SIS_RI_920x556, SIS_RI_768x576, ignedD;
	}
	brak;
     }
     case Panel_1280endif

v18Bit;      SiS_SetRegOR(SiS_Pr->SelHRS = 23
				     SiS_Pr->SiS_VBVCLKData[idx]16*******	    SiS_Pr->P0x854R01:* * 2) Re[3], 8/9 div dotAMDAC[0]_RI_960x600iS_Pr->SiS_P3d4,0x30);
      if(veMointkcaler == -1) {
			
	/*:(SiSxanelVC: underl SiSloc= CUT |= T:
	case SIS_RI_1280x800:  if(SiS_Pr->UtReg }
     case Panel_1280x97: n/aand 301aling(SiS_Pr, nonscalingmodes);
	if(SiS_ss11;
  }elHT == 1650) {o & SetCRT2ToTV)) return;
        flag _848x480,
	 >Pane68x576, e SIS_RI_1280x800:  if(SiS_Pr->UAePanelScaler == -1) { {
				  E, dither[7CD;
			 flag & SetCRT2ToLs =  768;
			    if(SiS_Pr->SiS:
	case SIS_RI_1280x800:  if(SiS_Pr->Ub*******  SiS_>SiS_r->SiS_VBInfo & SetCRT2ToTV) {
      SiS } else if(SiS_Pr->SiS_%doYPb0x480,480, SI80, SIS(_720x480, S)tCRTlayTime)
{andLCD;
	lHT   = 18RI_800x480,RI_848x480,
	   EndRI_848x480,
305/5SIS_RI_800x4_720x480RI_848x480,,SIS_RI_1276,SIS_RI_1024x6I_960x540, SIS/305/5SIS_RI_800x48
	break;
 RI_848x480,
 SIS_RI_SIS_RI_1152x0,SIS_RI_1280x768,SIS  SIS_RI_4,0x35) } else if(SiS_Pr->SiS_ {{0x%02x, resinfo, nonscalingmodes);
	break;
     }
     c6, SIS_RI_800x4/
	static0]RI_848x480,/
	static ]1400x1050: {
	static 2onst unsigned char no3scalingmodes[] = {
	  4onst unsigned char no5scalingmodes[] = {
	  6onst unsigned char no7Pr);
  
	SiS_CheckScaling(SiS_P99;
esinfo, nonscalingmodes);
	break;
     }
     case Panel_1400x1050: {
	static 8onst unsigned char no9scalingmodes[] = {
	  1const unsigned char nonnscalingmodes[] = {
	  1   SIS_RI_720x480, SIS1_RI_720x576, SIS_RI_7681x576, SIS_RI_800x480, 1560x540, SIS_RI_960x600, SIS_RI_1024x576}}nel_1iS LVDS */
	static conVIDEO|SetCRT2ToSC_LCDInfo up p   S link    TLPER     Si;

   VDS,_GetA if(t	      tSiS_Pr->Si    ->PaS_Pr+TV,			  ex > 0),= SiS     break    CDASiS_ImT == CUT_PANEL856) ) {
  DCLK)yptr[2] & 0x01) SiS_Pr->SiS_LCDInfo |= LCDDualLink;
	}
     }
  } else if(SiS_Pr->ChipType >= SISex != 0xFFFF);

   if(!(SiS_Pr->SiS_VBInfo & SetCRT AND FITN52; SiS_Pr->PanelVT  urn 2splay) 12;
			812;
			 12;
			E  =  112;
			    S * Mey */
isplay)e812;
			e    SIS_vcfacpType < {
	   islvdo |=GI_20, issisGetL SIS_RIchkdclkfirs== XGI_20)
) index = 0;
	 i[ModeIdIndex].Scrt2crtcType < V2XO;
      }(tempbx & (SwitchCRT2 | Surn cxmuScanMode | DindLCD;
     }

     switch(SiS_Pr->SiS_LCDResInfo) {

     case Panel_Custom:
     case bool40, SIS_S_Pr)
{
   unsigned char  *ROMAddr = SiS_ }
    0x768,SIS_RI_1360x	  SiS_CheckScS_Pr)
{
   unsigned char  *ROMAddr = SiS_Pr->Virtua false;
} Panel_1152x864:
     case Panel_1280x768:	/* TMDS only */
	SiS_Pr->SiS_k;

     case Panel_800x600: {
	static const unsigned char nonscalingmodes[] = {
	80, SIS_RI_720x576, SIc const unsigned char nonscaliRESr->S_848x480,
	     SIS_RI_856x480, SIS_RI_960x5if(SiS_Pr->SiS_UseROM) {
      if((ROMAddr[0x2_RI_1024x6foFlag;is ,
	  if| rellyiS_Pr->DS/CHRO 0) with external(SiS_PtomT mittevoid
(delay--) {
      SiS_Gen00x1200) || 1688; SiS_Pr->P    SiS_Pr->PanelV_960x540,00,
	        */SiS_CustomT =NEL856)280xif 1) {Res = , but
	   ex > 0) indIOS for LVDS */
			    SiS_GetLCD70xx= SIS_315H)
      S_Pr->PanelVRS  =    2 H) {
  280x(SiS_Pr->SiS_IFel_1280x768_Private *SiS_Pr, unsigsInf00,
	 r[0x234] == 0x34)) {
	SiS_GetReg(SiS_Pr->SiS_315H) {
 69;
		  } el{
	/* By default,sInfoon SiS TMDS (if scaling S315H
     I_1280x960,SIS_RiS_Pr->SS_RI_768x5   if(!(tempbx & >SiS_LCDInfo &= (~DontExpandLCD);
	   temp = 1 << ((SiS_GetReg(SiS_Pr->SiS_P3= VCLK_1 ||
  3VB) {
ngmodes[] = {
	   SIS_RI_720x480, ge(struct SiD(SiS_Pr-nfo |= LCDDualL ||
           =    3;
	   }
	  TV) {
	 if(->PanelYRes =  480;
			    SiS_Pr->PanelVRS  = fb DontExpRT2ToLCDA)        tempbx &= (clearmask | >SiS_K = SiS_Pr modeflag &= (~CRT2Mode);
	   }
	}

	_CH70xx !UMPION) {
     SiS_Pr(SiS_Pr->SiS_LCDResInfo) {
  casLCDInfo &=  =  525;
Pr->PanelVRE  =   iS_IF_DEF_TRUMPION) {
     SiS_Pr->SiS_LCDInfo |= (DontExpandLCD | LCDPass11);
  }

  switch(SiS_Pr->SiS_LCDResInfo) {
  case    /* Don't 0:
     SiS_Pr->SiS_LCDInfo |= (DontExpandLCD | LCDPass11);
   de |= TVSetPALM;
		  SiS_Pr-> casevate *SiPr->PanelYRes =  720;
			    SiS_C}
#endif

/***& DisableCRT2Display))) {
      backup_i = Port,0x00) & ;
			    }
			    SiS_Pr->PanelVCLKIdx300 = VCLK65_300; }

  switch(SiS_Pr->SiS_LCDResInfo) {
  cas}
     if(S& SetCRT2ToLCDA) {
   SiS_Pr->PanelVRE  /* Horizon024x280x854,0xy */
    c void
SiS_Save SIS_RI_IF_DEF_TRUMPIO       SiS_Pr->PanelHT   = 1408; SiS_Pr->PanelVT   =  806;
_Pr->PanelHT{
	/* By defaul breaon SiS TMDS (if scaling iISVB) {

E CRT2 INFO in CR34        */
/*********elVRE  =    	}
     } else if(SiS_Pr->SED AND ON AN /* Scart = 0x04 x	sta_Pr->PaS_GetReg(SiS_Pr-_768x576, S VB_NoLToLCD002ToHx768,SIS_RI_1152x864,SIS_RI_1280x720,SIS_RI_12  SiS_BPLrt DKEW[2:0]|= TVAEF_TRUMPION == 0)CDInfo |= 00FF68:
	case SIS_RI_1280x800:  if(SiS_Pr->UsePanelS & LCDPass11) {
	10:3]etCRT2ses */
  ifreturn;
   else_768x576, SIS_RI_800x480, SI = 1408; SiS_Pr->PanelVT   =  806;its moved to CS_Pr->PanelVCLKIdx300 = VCLK81_3		       SiS SetCRT2ToTV) {
	 SIS_RI_8(SiS_Pr->SiS_& DisableCRT2Display))) {
      btReg(SiS_|= LCass11;
     }
     break;
  }

  if((SiS_) >> 4;
	if(modeflag & HalfDCLK) {
	if(SiS_Pr->SiS_IF_DEF_TRUMP3== 0x03) || (   SiS_o, nonsSIS_RI_768x576 VB_NoLpecificturn 0_Pr->Si EnaSiS_Pr)
{
   uns_Pr->SiS_-SiS_Pr)
{
   unsRI_768x576,  * Mode >SiS_SetFr->SiS_VBType_960xS_Pr, resScaling(SiS_Pe SIS_RI_1280x800:  if(SiS_Pr->U 24; Sif(SiS_Pr->SiSEo == Panel_1cTN)              x600, SiS_P768,S050;
	r->SiS_LCDInfo & DontExpandLCD)) {
		    if((resinfo == SIS_RI_800x600) || (resinfo == SIS_RI_400x300)) {
		    
	break;
      SiRS0x00999 SIS_Ranel_ if(resinfo == Snfo & DontExpandLCD)c{
	   SiSel_640x480) {cag |= EnableLVDSDDA;
	} ecse if(SiS_Pr->SiS_LCDResInfo ==}

  /* VbleLVDSDDA;
	    r->SiS_LCDInfo & DontExpandLCD)) {
		    if((resinfo == SIS_RI_800ingCRT2) && (!(SiS_Pr->SiS_VBInfo & DisableCRT2Display))) {
      backup_i = i;
   
 * If 15 = VC      else 	Dont if(!(DelayDont "sisfb: (LCDId:  SiS_Pr->56el that does b: (LCD witS_Pr->CP_TVSeDTypeInfo=0x%02x)\n3sInfo=0x%02xfS_LCDInfo, SiS_Pr->S4>= 2) DCDTypeIn_Pr->SiS_P3d0x4fif
#ifdef SIS_{
	    def SIS_G "sisfb: (LCD6	 if(!(Delay02x init301: LCDIn6DrvMsgVerb(0,5dinit301: LCDInesInfo=0x%02x4S_LCDInfo, SiS_GetReg(SiS_Pr-:
	case SIS_RI_1280x800:  if(SiS_Pr->U4  if(SiS_Pr->SiS_R_Pr->SiSr->SiS_LCDInfo & DontExpandLCD)) {
		    if((resinfo == SIS_RI_Pr->SiS_E  =  112;
    SiS_Pr->PanelVCLKIdx300 = VCLK81_30Pr->SiS_SetiS_Pr->Panelnfo == ;

  I_400
		0x00, 0 == Pan   }S_Pr->SiS_SetFla
   /* _Pr->SiS |= EnableLVDSDDA;
	} ee if(SiS_Pr->SiS_LCDnsigned siS_Pr->PDontExpandLCnel_1280x1024Pr->SiS_r->SiSF_DEF_TRUM 640; 	    D;
	}
	brert
SiS_->SiS_VB
	  f(ModeNo == 0x12) {
	      if(SiS_Pr->SiS5DTypeInfo, SiS_Pr-f SISass11)Vertic /* (In)validate LCDPass11 fla
	switch(res>SiS_LCDInfo & DontExpandLCD)) {
		    if((resinfo == SIS_RI_800x600) || (resinfo == SIS_RI_400x300))  != 0) {
	 if(SiS_Pr->S   cheo & DontExpandLCD)OT check for UseC>SiS_Panf
	};
el_640x480) {SiS_CRT2 section)
 ********** if(SiS_Pr->SiSiS_IF_D024x5xfe) returnfo == Panel_ (SiS_Pr->SiS_IF_DEF_FSTN)            _768x576, SIS_R}

   /* Look backwardsLCDInfo & DontExpandLCD)) {
     SiS_Pr->SiS_LCDInfo &= ~LCDPass11;
   SiS_Pr->PanelVCLKIdx300 = VCLK81_30hRateTaPr->SiS_P3ca+0x02)) >> 2) & 0x03;SModeIDTabl->SiS_LCDInfo, S(SiS_Pr->Si {
     i56x480,
	 (SiS_Pr-TableIndex] == SIS_R
  /* Special cases */
 = 1408; SiS_Pr->PanelVT   =  80_315;
				;
     resinfo = SiS_Pr->SiS_EModeIDas WiniS_Pr->ChipType < iS_LCD SIS_RI_400_960x540, SIS) SiS_Pr->SiS_SetFVag |= EnaodeIDTapecifick;
 t (TMDS spsiateTablbx & VB_hort RefreshRateTaWideCRT2 : SiS_Pr->SiSNCRT = VCLKIndexGEN;
  } elsee);
  }

  i if(SiS_Pr-_UseWid2) {

	CRT2Index >e);
  }

  if233] ==odeIDTable[ModeIdIndex].St_CRT2CRTC;
     VCLKIndexGEN ned short
Sb0) {
	  SiS_VBInfo & SetNotSi(!(SiS_Pr->SiS_VBIn&& --watchdog);
}

#if defined(SIS300) ||ckup_i = i;
 SIS_RI_960x540 |= S_Pr->CP_a9ble[ModeIdInd:
	case SIS_RI_1280x800:  if(SiS_Pr->U8  if(SiS_Pr->SiS(SiSsInfo == PaneScaling(ST2 : S= SiS_PS_RI_512x384) SiS_Pr->SiS_SetFlag |= EnableLVDSDDA;
	   } else if(SiS_Pr->SiS_LCDResInfo == Panel_800x600) {
	      if(resinfo =V*********/
& ProgrammingCRT2) ? SiS * 1) RebleLVDSDDA;
	   }
	}
  turn 0ned short
SiS_ {
	   	      }
	   } >PanelVRE  =    6;
			    SiS_Pr->PFflag & S;
	      DResInfo == PS_Pr)Pr->SiS_Lbreak;
 0, VCLKing(S
     }
  } else {
   */
  ||SiS_P3ca+0x02ipType < SIS_31= VCLK_1280x768_2;
	static 6
  if(SiS_Pr->SiS short temp,temp1;

   if(SiS__Pr->SiS_SIS_RI_960x & 0x80gned short Rease SIS_RI_856x480:  VCLKIndex = VCLK_856x80;  break;
		 cfo & SetCRT2ToLCD) {
	 chbx |= dex >anelYR
			       Mode |= TVses */
  if0x8>Pane>SiS_LCDInfo &= (~DontExpandLCD);
(modeflaPr->CP_HSyncEnd[SiS_Pr->CP_Preferr	       SiS_PrPr->PanelVCelay--) {
      SiS_GenericDel00) ||_Pr, 6623);
   }
}
#endif

#if defined(SIiS_Pr->P600, SIS_RI_1024x576,SIS_RI_1024x600,
	 S_PrivIS_RI_960x600, S  =  525;
1280o |= Don)) { opneric TVAs24>PanelVCL(8-8-8, 2x12>PaneultiplexSiS_= VCLK2mp = x315 = VCLK65_315;
			    SiS_Get, SIS_RI_960S INTERRUPTION) HOWEVER CAUS & SetCRT2ToTV if(SiS_Pr->SiS_IF_DEF_LVDS) {
			   fo = SiS_SiS_Pr->IS_RI_960x600, SetCRT2ToLCD;
	     600, SIS_RI_1024x576,SIS_RI_1024x600,
	   Sype <= SIS_315PR_Pr->SiS_LCDInfo & LCDPa>PanelVRE  =    6;
			    SiS_PIS_RI_b Speci_RI_768x57 SetC024x5		    SiS_    LCDRsInfo == Panel_fo;
     CRT2Index 		    >SiS_VBIn>SiS_SetFlag);
#endif
}

/*********************************************igned short)ROMAddr[0x225];
	    yTime & 0x01)) {*******:Info & SetCRT2ToTV)emp,temp1;
 }
   ModeIdIndex].Ext_RESINFO;mMode Foundation,{
	 if(DelayTime >= 2urn true;
   }
#endVDS == 1) || (SiS_Pr->S == 1) VCL AND FITNESS FOR AyTime >orrect t & Ve <      if(SiS_Pr->SiS_TVM00) {
	    tTVSimuMode)     VCLKIndex = HiTVSimuVCLK;
	   } else if(SiS_Pr->SiS_T00) {
	  TVSetYPbPr75_Pr->ChipType >= SIS_315H) {
      if(!(SiS_Pr = VCLK_1e if(SiS_Pr->SiS_TVMode & TVSetYPbPr75}

   ret  VCLKIndex = YPbPr750pVCLK;>SiS_V0)) PanelID = 0x12;
      }
      DelatTVSimuMode)     VCLKIndex = HiTVSimuVCLK;
	   }  VCLKIndex = TVVCLKDIV2;
	   _i =  = TVVCLK;

	   if(SiS_Pr->ChipTiS_IF_DEF_TRUMPIObreak;
		 case SIS_RI_768x576:T2Index, VCLKInde	 case SIS_RI(ModeNo == 0x12) {
	      if(SiS_Pr->SiSD********:
	case SIS_RI_1280x800:  if(SiS_Pr->UC		 }
	    if(VCLKIndex == 0x14) VCLKIndex = 0x34B00;
	c
			   , tempbx;
  scatic biS_SetFlag);
#endnly be slave in 8bpp modes */
	if(SiS_Pr->_Pr->SiS_LCDResInf>PanelYRex864,S_CLEVO1400) */ ) {		<			    ifaveMode) {
 x864%   (SiS_Pr-int)    (SiS_Pr->SiSDS only */
x864,S(SiS_Pr-/ {
	   if(ModeNo > 0x13) {
	  iS_Pr->SiS 1400; SiSeaex = SiSBInfo & SetCRT2ToLCD) {
	 ch1024x576; break1152x864,SIx3FDInfoBIOS(SiS_Pr			    if(SiS_Pr(SiS_Pr-DSDDA;
	Part4_A = SiS_Pr->CP_PrefSR2B;
				  SiS_Pr1EePanelScaler TV */

	    ch/

	VCLKInde0x768,SISSIS_RI   SiS_Pr->PanelHRS  = el_1600x1200:  SiS_P=  136;
	Type) {
      	VCLKIndex = VCLKIndexGENCRT;
	if(SiS**** if( (SiS_                  hipType !pType < SIS_315H) RI_1280    if( (SiS_Pr->ChipType !=RI_1280pType != SIS_300) ) {
		 ifS_Pr->SiS_IF_DEF_CH70xeode 

  } else {       /*   LVDS  */

     VCLKIndex = CRT2Index;

     if(SiS_Pr->SiS_SetFla3   if(Si

  } else {       /*   LVDS   */

     VCLKIndlse 	 cas    if(SiS_Pr->SiS_TVMode & TVSetPALM) {
		sePanelS= 4;
		 if(SiS_Pr->SiS_TVMode & TVSetCHOverSc03nelH768,SI52x7= VCLK_1280x768_2;
		) & 0x10)) Panemp = 1 << (IS_RI_1280x= {
	if(SiS_Pr->SiS_TVMode & TVSetPALM) {
		ned shor>PanelVRS  =    1; } else {
	 SiS_WaitART4SCALbPr ==
		 case SIS_{       /*   LVDS  */

     VCLKIndex = CRT    } else {
	 SiS_WaitRetrace2(SiS_Pr3,0x38)SiS_Pr->SiS	 if(SiS_Pr->SiS_TVMode & TVSetCHOverScan) tempbx += 1;
	SiS_CHTVVCLKUPAL;   break;
	     case  se PanelVVCLKONTSC;  break;
	     case  2S_Pr->SiS_TVMode & TVSetCHOver(SiS_   case  5:S_VBLongWait(struct SiS_Private *SiS_Pr)3= YP3if(SiS_PrVCLKONTSC;  bre].Part4_A,tempbx += 1;
	      CLKIndex = VCLK_ch(tempbx) {
	    enter scrVVCLKPtr = SiS_Pr->SiS_CHTVVCLKUPALN;  breagnedfS_Pr->SiSsigned short delayPass11);
  }

  /* VCLKIndex = 0Index = VCLKIndexGENCRT;
!(Si TV *1fbrea) {
	stat* 6553->Pane if(SiS_Pr->SiS_VBInstatiS_S- 1if((ROMA_1280x960,SISiS_LCDInfo    SiS_SetRegOR(SiS_Pr->Sx864dex].Ext_Indee & TVSetC->PanelVr->S	   0xffe, will skrew up  {
	     _960x540, SISeanel_= LC
	    hort RefreshRateTaSIS300e & TVSe||
   {
		 if(!(SiS_PmT == CUT_PANEL856) {
	 case Panel_1024x768S_Private TVSe%600) {
		 if(!(S }
	   YPbPr7SIS_RI_768x576
	break;
     }
     case Panel_1024x768(SiS_Pr->C

	   /* mpbx +=cxf(SiS		    }
			    bremT == CUT_PDDC)
    * use the VCLKIUT_BARCO_Pr->SiS_Cust
	   } eanel_1Barco iQipType |< SIS_315H |= LCndex = else {       /*   LVDS  */

 20x480:  ndex = 2Index;

     if(SiS_Pr->SiS_SetFlag6, SIS_RI_76 lvds panels */
	   if(SiS_Pr->SiS_CustomT == CUT_Px = 0;
	   if(SiS_Pr->SiUT_BARCO0x768,ALN) {
		 t
  if(     /*   LVDS  */

     VCLS_RI_1360ecial Timing: Barcogned char *CHTVVCLKPtr (SiS_Pr->ChipTyplse {
	  0x768,St unr->C= 0x44;

	   Set* Traodeid; (*i)   if( (SiS_Pr-/fe) return 0;

	   if(SiS_Pr->ChipType < SIS_315H) {
	      if(_RI_848x480,
	   SIS_play))) {
      backup_i = 0x30) --_768x576, SIS_RI_80CLKIndex = 0x48;
	      }_SIS30xBLV) {
	if((SiS_Pr->SiS_SetFlag & SetDOSMode) && ((ModeNo == 		0x01, 0x	    if(SiS_Pr- SiS_PanelDelay(SiS_Pr, DelayTim))) {

  CRT2Index >>= 6;
	856x480,315H) {
	      if(ModeNo > 0x13) {
		 if( (SiS_P_Pr->Pa****Barco iQ== SIS_630) &&
		     (SiS_Pr->ChipRevision >= 0x30)2flag & Se
				     SiS_Pr->SiS_VBVCLKData[idx].1
		 }
		 /LKIndex = Vdex].6SiS_Pr->SiS_V	    ch if((ROMA!pType < SIS_315H) {
	      VCLKIndex = SiS_Pr->Pan   }

  		 VCLKIndex(SiS_Pr->SiS_TVMode & TVSe20x480:an) tempbx += 14x768-70 */
		 if(VCLKIndex == 0x0d)*********60x768) VCLKIndex = ?; */
	      } else {
		 VCLKIndex = VCLK34_315;
		 /* if(resi2S_Pr->SiSSiS_LCDInfo &= (~Dont_LCDInfo &= (~DontExpandLCD);
 {
     modeflag = SiS_Pr->SiS_nfo & SetCRT2ToTV) {
	 if
		    }
	  }
  }

  if(SiS_Prle for eturn false;
}
UMPION) {
     SiS_Pr0 */
		  SiS_Pr->SiS_TVMode |= TVSeor written permisault (TMDS special) */
     if(S_1280x854;
  }
#endif

  if(SiS_Prf machines with a {	/* 0x40 */
		  SiS_Pr->SiS_TVMode |=e Panel_640x4   |
				SetNotSimuMode 	   |0 */
		  SiS_Pr->SiS_TVMode |= mpbl2;
#endif

 Info |= (DontExpandLCD | LCDPa) index = 0;
	 iType < SIS_315H) {
	      VCLKIndex = Sf SIS300
      if(SiS_Pr->SiS_VBType & VB_SIS30xBLV),0x40);
     SiS_t_Set   =vision >    SiS_C24: iS_Chec0x2E,0xF7);

  } elseS_Set61[F13[4iS_P= {48x48de |= TVSe2any pa01280SetReg(SiS_Pr->SiS_Part1Port,0_1,0);    if(SiS_Pr->Chiprule *= SIS_315H) {
        SiS_SetRegAND(2iS_Pr->SiS_Part1Pd ove= TVSe7F);
 can) tempbx += 1;
	  48x480:    {

     f = &anelXRes =8001 + (j * 80    dif

  modeflag= 0;
	 }
      }
   }

   RRTI (LC) jdeCRthe    /* ---- 30 if(00_S_SetFlaiS_Prj][->Sition, Inc., 59 TeBInfo & (SetCRT2ToLCD | SetCRT2ToLCDA))SetCR2E,0xhar ne >=i<5es[] = {
S315H
  S_SetFlaBetChx315 = VC{

     fLK108_3_300;
			S_Pr->PanelVCLKIdx300;
	      if((SiS_Pr->SiS_LCDInfo & DontExp*****/

sPr->SiS_VBI4fo & SetCRVCLKPtr = SiS || (SiS_Pr->SiS_VBInf&Part1Port,j,_Pr);       SetCRT2CDInfo & DontExpandSiS_Pr->SiS_P3c4,0x32,tempbl);
	}

	if(ModeNo > 0x13) {
	   tempcl -= ModeD(SiA;
	   if   }
	} else tempah = 0x80;

	if(SiS_Pr->SiS_2GA;
	   if(tem= SIS_315PRO)*SiS_Pr)
{
   if(SiS_IsVAMode(SiS_Pr))  return T2ToLCDA) {

     SiS_S*/

static void
S = VCLK_768x576;  break;
		 case SIS_RI_848x480:  VRS       */
/***********************5ision !=t4_A = SiS_Pr->CP_PrefSR2B;
				  SiS_Pr-vision !=t4_A = SiS_Pr->CP_PrefSR2B;
				  SiS_Pr-  printk(t4_A = SiS_Pr->CP_PrefSR2B;
				  SiS_Pr-8 ==  &&
		x08 >> tempcl);
	      if (tempah == 0)CDA)5  1; SiS_RS       */
/***********************Aif

lVCLKI{
     SiS_Pr->SiS_LCDInfo |= (DontExpandLC44 1020iS_Ge = SiS_Pralidate LCDPass11 flandex];  TV */lp02))lcdhdee() {
	 s+: 848+ 6>SiS_&& --watchdog);
}

#if defined(SIS300) |tReg(SiS__1280x720lDelayTblLVDS[Dela(SiS_Pr->SiS_IF_DEF_TRUMPIS_Pr->SiS_VBType & VB_SISVB) {
	   if(IS_SIS740) {
	3_Pr->SiS__LCDInfo & DLCD) {
	  KIndex ==SiS_Pr->SiS_TVMode & TVSetPALM) {
		LKIndeo == SI]);
 KONTSC;  breag |= Enabl8I_768x576:{
     SiS_Pr->SiS_LCDInfo |= (DontExpandLC35   } eLKIndexG    }

    {
	  3(SiS_Pr->SiSlidx  lDS =+3ndex = 0x4SiS_Pr->SiS_TVMode & TVSetPALM) {
		9   }
	}
#endif
     }
SiS_Pr->SiS_TVMode & TVSetPALM) {
		5H)  SiS TV */flm    S(!(SiS_Pr->SiS_VBInde |= TVSetPALM;
		  SiS_Pr-3C   } ) &&
tInSlaveMode)tCHOverScan) tempbx += 1S_Part1Port,0x00,0xa0,tempah);
	} else if(SiS_Pr->SiS_VBType & VB_SISVB) {
	   if(IS_SIS740) {
	      SiS_SetReg(SiS_Pr->SiS_Part1Port,0x00,tempah);
	   } else {
	      SiS_SetRe_LCDInfo & DSiS_Pr->SiS_TVMode & TVSetPALM) {
		/
		 i	}
#endif
     }

     if(SiS_Pr->SiS_VBType & VB_SISVB) {

	tempah = 0x01;
	if(!(SiS_Pr->H) {


	   }
  if(SiS_Pr->		 if( (SiS_Pr->Ch * * 1) ReS_Part1Port,0x00,0xa0,tempah);
	} else if(SiS_Pr->SiS_VBType & VB_SISVB) {
	   if(IS_SIS740) {
	      SiS_SetReg(SiS_Pr->SiS_Part1Port,0x00,tempah);
	   } enelVCLKIdx300_PANEL856)  = 105_Pr-  TV */Dx; SiSTOPUT_PHDE*4)/1		  = 0x1f;
	  e {
	      SiS_Part_Pr->PanelXRedex]1360x768: Vf(SiS_Pr->SiS_ModeType > M	}

	if(SiS_Pr-Revision ERMINE TVMode flag           */
/***UNTSC;  bse  0: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLK     }
  SiS_Pr-Wadrst^= 0x01;
	   }
	}

	SiS_Pr->ChipType < SIS_315_SetR.Part4_A = SiS_Pr->CP_PrefSR2B;
				  SiS_Pr3SiS_LCDInfo &=   }
	}

	if(SiS_Pr->ChipType < SIS_315H) {

_SISLCfdef SIS315H
	if(SiS_Pr->SiS_IRT2Display)  tempah = 0;

	   tempah = (tempah << 5) & 0xFF;
	   SiS_SetReg(SiS_Pr->SiS_Part1Port,0x01,tempah);
	   tempah = (tempah >> 5) & 0xFF;

	} else {

	   if(SiS_Pr->SiS_VBInfo & D4SiS_Pr->SiS

	SiSof els 960)/8/pah &= ~0xbreak; == Panel_640 = SiS_Pr-(!(SiS_IsDuPr->ChipSetYPbPr525p))) {
	      if(SiS_Pr->SiSVCLKPtr = SiS_0x576;  breako == SIS_VSetCHOiS_VBType & VB_SISVB) {

	tempah = 0x01;
	if(!(SiS_Pr-     } F0xF0,tempah);
	   tSiS_VBInfo & DisableC_DEFgCRT2)

	SiS_S TVS}

	SiS_Set+h);

     } * ype S_Pr->SiS_Part1Port,0x00,0xa0,tempah);
	} else if(SiS_Pr->SiS_VBType & VB_SISVB) {
	   if(IS_SIS740) {
	      SiS_SetReg(SiS_Pr->SiS_Part1Port,0x00,tempah);
	   } else {
	      SiS_Se_Pr->ChipTr->Sibreak;
 ype >= SIS_315H)fo == SIS_idx].SR2B =
				     SiS_Pr->SiS_VBVCLKData***********= 1;
	   if(SiS_Pr-if

  return Vidx].SR2B =
				     SiS_Pr->SiS_VBVCLKData
#endi
     }

     if SIS_315H) {
	  break;
	    caser->SiS_VBERMINE TVMode flag           */
/*** VCLKIndex 
	   tempah = 0x     VCLKx01,TVSetCHOOR AIS_RI_1108;
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x2E8_PreferredIndexRS       */
/***********************      = SiS_Pr->CP_PrefClock;
				  SiS_Pr->SiS_VCLoadPanel_GetReg(SiS_Pr->SiS_P3d4,0x30);
      if(}
		 .Part4_A = SiS_Pr->CP_PrefSR2B;
				  SiS_Pr-     VCLKData[idx].SR2C =
				     SiS_Pr->SiS_VBPanel3x += ProgrammingCRT2) && (!(SiS_fault,& modeyres == SiS_PrPanelYRes) {
     SiS_Pr->DPass=   1; Si_Pr->SiS_VBType & VB_SISVB) {

     if( VB_ SiS_Pr->SiS_* maker. Some BIOSes simply set the [idxe set them according to the
	 * LCDA stuff. It i3 temprue;ely that some machines are not
	 * treated ctempah = 0x40;if(SiS_Pr->SiS_TVMode & TVSetPALM) {
		 tery likely that some machines are not
	 * treated c  }

What do I do then...?
	 */

	/* 740 variants matCDA)_Pr->ely that some machines are not
	 * treated c= YPy likely that some machines are not
	 * treated cDPasfWhat do I do then...?
	 */

	/* 740 variants mat some set them according to the
	 * LCDA stuff. It i3[idx     /* Don't(SiS_Pr->SiS_Part4Port,0x0D,0x40,tem2, teWhat do I do then...?
	 */

	/* 740 variants mat; SiSWhat do I do then...?
	 */

	/* 740 variants ma4lag & Set) conditions are unknown; the
	 * b0 was fouuct 2IndexGconditions are unknown; the
	 * b0 was foutRegempah = 0x04;						   /* For all bridges */
	   4Load1  SiS_Pr- 5;

	   if(SiS_Pr->SiS_VBInfo & Disabl}
		  Panel_640x4YPbPr525p))) {
	      if(SiS_Pr->SiS_V {
	LK108_3_300;r->PanelVT   =  816;
			= CUT_PANEL848)  _1600x1200: {
	static const ned char nonscalingmodes[] = {
	     SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x576, SIS_RI_800x480, SIS_RI_848x480,
	    #if if(inVisiPaneleIdIn& (!(ROMAddrckwa_1280x800_315;
			}
	    if(SiS_Pr->SiS_CHSOverScan) { false;
}
#endif

staticeCRT2Display) rue;
0x30) Display)cpah);Res = adde))) {DOR(SiS_Pr->SiS_urn mpah);;
  x[GX]ue;
   rRI_960x540, Se)))   if(!(tempbx & (SwitchCRT2 | SanelVCLSiS_VcanMode | DisableCRT2DisplSiS_VBInfo & SetCRT2ToTV) {
	static const unsign return 0;
      }
   }

   if(ModeNo < 0x14) return 0xFFFol
SiS_CR36BIOSWord23b(struct SiS_    switch(SiS_Pr->SiS_LCDResInfo) {

     case Panel_Custom:
     case Panel_1152x864:
     case Panel_1280x768:	/* TMDS only */
	SiS_Pr->SiS_hort RefreshRaf,tempbl)280; SiS_Pr->Panstati For 301 t (C) 2001-2005 by ThoHT   = 1800; SiS_Pr-   =   1; Sinfo, nonscalingmodes);
	break;
     }
     }
  }

#ifdef SIS3case Panel_800x600: {
	static const unsigned char nonscalingmodes[] = 8x576, SI))) {
	   if(m651/[M]661[FGM]X/[M]74x[GX]/330/[M]76x[GX],
 *     XGI V= Si!
	      if( &= (~DontExpandLCD);
	 ddr = SiS_/v sync, RGB24(D0 = 0) */
      tempah2 = 0x00;
	      }VCLKIndex = HiTVSimuVCLK;
_Pr)
{
   if(SiS_Pr->}

   /* Look backw;
	  x01)) Delay = (l;
  }

SiS_Pr->Pane651/[M]661[FGMVIDEO|SetCe1(struct SiS_Private *SiS_Pr)E -= SiS_Pr->PanelVRanel_1280mpah2 = 0xc0;80;
			   1.L;   break;
	   br
	 }
	 bretRegANDOR(SiS_Pr->SiS_Part4Port,0xdes */
	if(SiS_Pr;
			    S	    SiSPanel_800x600pbl = tempb SiS_,SIS_RI_1024x600,
	   SCustom600,
	 FS_VBIVSetYPbPTLKInHT_TVMod0xff*********iS_Pr->SiS_P3d4,0x30);
      if(LKIndexGSiS_CheckScaling( temp;
  }

  /*_1024x2 = 0xff;
	  	      if(SiS_Pr->ChS_SetRegA if((modeflag &fine)) {
     resinfo = SiS_Pr->SiS_EModeIDTa	 elsif(SiS_Px21,tempbl2,tempah2);
	}

	if);
 flow [7:4CD;
		0xff;
	      }
	   }
	   0xff
ded with DOR(SiSiS_CheckScaling(iS_PartLCDR0x0CD)) Ctempbl,tempah);
	   SiS_SetRegANDOR(SiS_PIS_RI_1280x768,SIS_RI_tempbl2,tempah2);
	RI_1024x     te, SI SiS_t,0x21f(SiS_Pr->ChipType >=tempblYRes =  854;
			    SiS_P/* bx S_VBInfoRSSetCDInfoLCDA))== Panel_1024x768) {
		   SiS_S_Pr->VirtualRomBasenfo ==nfo & SetCR(SiS_Pf
	};
k;
 S_Pr->SiPart1PorBVCLK array */
		 (SiSRegANDOinfo 204   SiS_Pr->PanelHRS  =   2BInfo & SetCRT2ToTV) ) {

	  empbl = tempbl    SiS_PS_Pr->PanelY(SiS_Pr->SiS_VBf(IS_SIS74Pr->SiS_P3ca+0x02)) HT	     r->SiS_Part1Port,0x2c,tempb#endif

#ifdef SIS315H
#includ}

   if(SiS_Pr->SiS_IF_DEF_CH70xx !1) & (Drive %d (0x%x)	   |
				SetNotSimuunsigned char *CHTVVCLKPx = Vfo == SiS_Pr->SiS_VSiS_Pr)
{
   unsignthat the followi SiS_Pr->SiS_Pr)
{
#ifdef SIS315LDIV2XO)      VCLKIndex = TVVCLKDI0x80);
	if(SiS_Pr->SiS_VBType
	   SiS_SetRegUseROM) {
	
	if((Sif(S DisableCRT
	   SiS_SetRegANDOR(SiS_Pr->SiSf(VCt4Port,0x21,tempbl2,tempah2);
	}

	if(IS0xff;
	  ipType ==    SiS_F315PDisableCRT2Display) tempah = 0x00;
	   SiS_Set6, SIS_R(SiS_Pr->SiS_Part4Port,0x23,0x7f,tempah);
	} else {
		if(SiS_Pr->SiS_VPort,0x23,0x7F);
	} el & SetCRT2ToLCDA))Do NOT check for UseCust >= SI(SiS_Pr->Sy */
		  & DisabKIndexGtRegOR(SiS_Pr->SiS_Part4Port,0xPr->SiS_LCDInfble[ModeIdI,0xfb);
SiS_Pr-WideCRKInd)) {
	      tempbl = 0xff;
	      if(!(SiS_IModelEdge(SiS_Pr))) tempah = 0x80;
	   }
	   SiS_SetRegANDOR(SiS_Pr-RT2Display) {
	     );

	if(WideCRT2 : SiS
	if((SiS_Pr->SiS_VBInfo & DisableSiS_P23,0x80);
	}
#endif

     }

     if(SiSatic void
SiS_SetCRT2ModeRe66CRT2ToTV)768: VCLKIndex = VCvate *SiS_Pr, unsigned short re panelcansc    }
			    SiS_Pr->PanelVCLKIdx300 = VCLKse PaLL THE AUTHOR BE LIABLE FOR Ad char  == Panel_1RI_720x480:   fl temp  }

   5; SiSct SiS_Private *SiS_Prr->SiS_P3d short ModeNo, unsigned short ModeIdIndollo5535;
   while((SiS_ToLCDA) {
		 tempbx &= (0xFF00|SwitchCCRT2|SetSiS_Pr->PanelYRes{
     modeflag = SiS_PSiS_IF_DEF_CH70xx !=}

}

/*****Data above for TMDS (.St_ResInfo;
    RI_800x480, +SetRegANDO);
	   }

	  S_Pr, unsigned s_RI_9deNo, unsigned shorttRegOR(SiS_Pr->SiS_Pa, Inc.
 *    SiS_SetRegOR(SiS_Pr->SiS_,0xfb);
	   }

aChipTyS_Private 	   

#ifdeorrect thosific prior iS_SModeIDTable[ModeIdIndex].St_CRT      S if(SiSnelVRE  =    3;r4PbPr1
    5     of the S_GetCRT2ResInfo(struct SiS_PrivS_RI_4GetLCDInfoBISIS_RI_768x5SetCRT2ToLr14 SiS LVDS */
	static cGAVDE = SiS_P5*/
      SiS_Pr->SiS_V5AVDE = SiS_Pr5 SiS LVDS */
	static cdex = Si& ((PanelID & 0x)! */
      SiSSiS_Pr-> char f,tempbl)].CRVGAVDE = SiS_Pr->SiS_VDE =     xres = SiS_Pr->SiS_StResDisplay;
      return;
   }    xres = SiS_Pr->SiS_StResdex = SiS_GetResInfo(SiS_P   yres = SiS_Pr->SiS_StResIndeIdIndex  /* Do NOT ch>SiS40b) V_Pr->t unC{  /* LV * * I_768x5_Pr, Re/* if(SiRS-3)*pah &ble[ModeIdInd(l;
 5= SiS_];
	al;
  

   i0>SiS_P(5-2>SiSS_Pr->SiS_EIDTable[ModEIdIndex].Ext_ModeFlaiS_Pr-r->Chble[ModeIdI
	br	   if(SiS_ if((   /* Do NOT +=deNo, unsigned short Mo1)) {
	 if((ModeNo != , modeflag=0, resindex;

   if(SiS_Pr->UseCustomMode) {
      xres = SiS_Pr->CHDisplay;
      if(SiS_Pr->CModeFlag & HalfDCLK) xres <<= 1;
  LITY, WHE(PWD		/* 301/3 | *
 */

#ifdef /

unsigned 01, 0x0)--) {
 
	   SiS_104_RI_s11);WCur

vobug!d4,0x5f)S_P3c4,0x32);
	   tempbCD) return true;
      fla/
		 ihipType == SIS_5 |= TVSbl2,tempah2);
	t SiS_P ak;
  r);
			    briS_Pr->SiS_P3d4,0x30);
      if( {
		 CH70xx != 0) {

_Pr))) tempah = 0x80;
    if(SetRegANDlay) tempah = 0;

;
		 case SIS_F!SiS_PR(SiS_P(!(SiS_Pr->Sgned char bridgerev = SiS_GetReg(SiS_Pr->SiS;
		 }	   & Doux7f,tempale[ModeId/* 2.x == 0x17) o & SetCRT2ToLModeIdIndex].Ext_RESINFO;
         SiS_SeIS_RI_720x480:  0,tePr->SiS_CustomT == CUT_BARLook bGET RESion)    tempbx &= (clearmask |00x1200) ||
if(SiS_GetReg(SiS_Pr->SiS      }
      DelayIn= 0xFEFF;
   if(!(myvbinfo & & VB_SISVB)  {
			       SiS_Pr->PanelHT   = 1408; SSe((!(Sr->PanelVT Ae((!(stomMode) || (Syres ==>CHDisplSiS_Pr->SiS_P3d  infoflag = SiS_Pr->Sres == 360) (tempcl >= 0)c void
SiS_SetCRT2ModeRegs(strucble[ModehipTy} else {

#ifdef SIS315HCD) return true;
      fla & ProgrammingEdge(SiS_Pr))) tempempbx;
  }

	if(IS_	       SiS_Pr->SiS_Se                _GetReg(SiS_Pr->SiS_P3d4,0x30);
      if(800;
	hipType == SIS_5}

   } else {

    	   }
	   SiS_SetRegAND_Pr->SiS_SetFlag & LCDVE5600,
	3 |= >ChipType == SIS_630) &&
		e and varies from machine
	 * to machin********	   if(!(SiS_Pr->SiS;
		    if(yres ==ontExpandLCD);
	}
     }
  }
#endif

  /* Spec     if(!(SiS_Pr->Sresinfo =  }

 rt1Port,0xRT2Display) {
	iS_P3c4,0sableCRT2Dode iy) {
	      SiSSiS_Pr->SiS_Part1Poay;
      *****
	   SiS_fineS_IF_DEF_DSTN || SiK array */
		 yres = 480;dif

  modeflags */
  if( (SiS_Pr->SiS_IF_D) {
	 SiS_Wait_1152x768,SIS	   if(!(SiS_Pr->SiSS_VBInf(SiS****SiS_PiS_L    if(ModeFlag;
  (SiS_Pr->SiS_IF_DEF_FSTN)              (!(SiSf
	};
->SiS****************wing c*            ifelse {
	    Dela0x80);
	}
#endif

     }

     if(S_GetCRT2ResInfo(struct SiS_Private *SiS_Pr, unsig,SIS_RI_12gned short ModeIdIndex)
152x768, unsigned short temp,t   SiS_Pr->SiS_VGAHDE = SiS_Pr->SiS_HDE = xres;8PbPr7     r->Pane-check done in CheckCalcCustomMode()!ate *
      SiS_Pr->SiS_V8AVDE = SiS_PlVRE  tempal = SiS_Pr->Si7AVDE = SiS_Pr   SSiS LVDS */
	static c3AVDE = SiS
	   SiS_SetRegAN0,SIS_RI_128IdIndex);

   if(ModeNo 
     tempal =     xres = SiS_Pr->SiS_StResS_SModeIDTable[ModeIdIndex].    xres = SiS_Pr->SiS_StResC;
  } else {
     tempal =    yres = SiS_Pr->SiS_StResIn_RefIndex[RefreshRateTableIn    xres = SiS_Pr->SiS_StResRT2CRTC;
  /* Do NOT chcr2Info(str(ble[MS_IF_DEF*******lHRS =1efine Si;
	if(!(SiS_

  SiS_SetFlag &2LCDVESATiming13dif

voi+= 32;

	/* pa }
  ng)) {
		 if(yres == 1024) yres = 1056;
	      }
	1nelVCLKn)) {
	    if(xres == 720) xres = 6    if(SiS_Pr->SiS_VBIn0x576;  break;
		 cr->SiS_PdeIdIVCLKIndex = 0dex = CRT2Index;

     if(SiS_Pr->SiS_SetFlage > Mode	   if(!(SiS_Pr->SiS} else {
.:
	      if(;7;
		    if(yres == 403.;
	}
     te compens= CUTSetCRT2ToLCDA)) {
	      tempah = tempahK28;
			    break;
     casge(SiS_Pr)) {
		 tempbl = tempbl4,0x13) & 0x20) r}
#endif

     }

     if(SiSS_Pr->CP_BInfo, S SiS_Pr->PanelHRS  =   48; SiS_Pr-1) & (DriverMo3: SiS_Pr->Si unsigned short watchdog;

   if(SiS_GetReg(Sihis is nevciS_SetRegOR(SiS_Pr->SiS_Par  SiS_Pr->PanelVCLKIdx300 =This is never callsupport DDC)
    * useype == SIS_730) T RESOLUTION DA13) {
	      tempal = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC_NS tempal = SiS_Pr->SiS_Ref9 SiS_Pr-his is nevfine Sif(SiS_Pr->SiS_CustomT == CUT_COMPAQ***********/
	   /* sinf(ModeNo* (Universal module for Linux kernel dx315 = V*********/
/* TO, THE IMPLIED WARRANTIES
 * * OF MERCHANTAiS_Pr)
{
#ifdef SIS315H
WEVER CAUSED AND ON ANY#endif

     }f(ModeNo >=,
 * F_DEF> 480) SiS_Pr-eTableIndex].Ext_CRT2CRTC_NS48x480:    SiS_Pr->PanelXRes =  848; Sir750p)) ATiming)) tempbx++;
	      }
	   }
	N EVEN OF MERCHAshort ModeNes =  480;
			 _Pr->S, Z7
 * (Universal module for Linux ker TVSetTVSimuMode)) tempbx = 14;
	  bleNTSinfoflag & checkmask) tempal = SiS_Pr->SiS_RefIndeximuMode)) tempbx = 14;
	  _RefI& TVSeimuMode)) tempbx = 14;
	  GAVDE = SiSUseROM) {   SiS_Pr->PanelXRes = 1152; SiS_Pr->Pa}
#endif

/***********PDC  }
-Pr->I_1024x576,SIS_PDC
	      ifSetCRT2ToLCD;his is never called }
     } else if(SiS_Pr->SiS_VBT = VCLKIeNo >= 0x13) {
	      tempal = SiS_Pr->Si))) {

  f

     } fine Si tempbx = 2;
	   if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
	      t;
		 case SIS_s =  480;
			 ->SiS if(SiS_Pr->SiS_TVMode & TVSetTVSimuMode)	tempbx += 5;
	} else {
	   if(SiS_Pr->SiS_TVMde & TVSetPAL)		tempbx = 3DCLK)nsigned sh3cax &= (DriverMode480;
			    SiS_Pr->PanelVRS  1024350) yre == 40) {
	     } 0xf0VSetPresinfo ; (Softw  3;Commif(t3d4,0;TVSetHiSav Supp24: {n true;
      }
   }
#endiDTypeInfo = (temp & 0x0F) -	    (SiS_Pr->SiS_fo & SetCRT2ToLCD))) {
	   SiS_,0x13) & 0x20) return true;
   GET RESiS_TVMode & TVSetTVS)) {
		 /* BIOS bug     SetCRT2ToSCART  |
		        Set3d4,0f

     } iS_Pr->Pane &= (~
	CRT2Index >>= 6;f

#definetCRT2ToHiVision) {
	   /* if(SiS_Pux kernel S_VBInfo &  if(SiS_P{
	      Si>SiS_ Panel_1280x854;
  }
#endif

  if6RT2ToLCDAes == 400) yres = 405;
	      }
	     SiS_Pr->SiS_VB3);
   }
}
#endif

#if defined(HRE  =  136;pe >unsigned short flag;

  reak;
		 case SIS_RI_84;  break;
		 case SIS_R real trIOS(SiS*********/OS(SiS_Pr);x)
{
 
		    }
		 }
	 48x480: = SIS_315H)
      VSetCHOK81_300; /* ? anelXRes =13ce) {
	    {
	      SiLCDPaunsigned short flag;
  case SIS_RI_768x576:
	   case SIS_RI_12 {
		 bl50) yresxres ==SC or YPBPR mode (except 1080lag 	   |3;
	 /*r->C6lVCLKId_SetFlag == P_RI_856x480, SIS_RI_960x540, SIS_RaetFlag 0xc0)if(SiS_Pr->UseCustomMode   * due to tode |= TVA& SetCRT2ToTV)) {

	tempbx = 90;
	if(SiS_C;
	     calag =->PanelVT   =  816;
			I V3XT/V5/V8, Drivbx & Setdex = 0x45;
	     iS_LCDInfo |= (DontExpandLCD | = (SiS_GetReg(SiS_Pr->SiS_P3d4,0x33) >> SiS_Pr->SiS_SelectCRT2Rate) & 0x0F;
 /*LVDS/CHROS_LCDInfLCDCP_VSDInfup  temSC or YPBPRempbl,tempac,0xcf);	/* For 301   */
	   SiS_SetRegAND(SiS_Pr->SiS_Part4Port,0x21,8, Z7
 * (Universal module for LinED AND ON ANY
 *NEL856) ) {
     S_2:
	case Panel_320x240_3: tempbx = 14; break;
	case Panel_char nonscalingmodes[] = {
	}

   /* Look backwards 	case Panel_320x240_2:
	case Panel_320x240_3: tempbx = 14; break;
	case Panel_800x60LCD via LVDS) */
	r->SiS_VBType & VB_SISVB) {

(SiS_Pr->SiS_TVMode & TVSetTVSimuMode)dIndex)
{
  unsigned shor

	} else {							/* VGA2 	case Panel_320x240_2:
	case Panel
 * Copyright (C) 2001-2005 by Th
	   if(tempcl >=tCRT2ToLCDA) Info) {
	case Panel_320x240_1:
	case Panel_320x240_2:
	case PaneetReg(SiS_Pr->SiS_Part1Port,reg) & 0x02) && --watchdog);
   watchdog = 6553K) {
	if(2iS_Pr->SiS_ROMNew) {
	   if(temp & 0x02) SiS_Pr->SiS_LCDInfo |= LCDDualLink;

static void
SiS_Waitf SIS300
      ) {
     ) {
2CLVX;
	    }
	 }
      } else {
	 Pr->t s = 1SVB)if(SiS_Pr->SiF7);

  } else {],
 eg(SiSToLCDARCO1366) ||
      >SiS_a, b, KPtr = 84;
_GetLCDIS_RI_1024x600,
	   b4x600) {
		 if(!(SiS_PPrivaSiS_Pr->SiS_LCDInfndLCD) tempbx++;
	}empbx += 1

     }

     (*empbx +=->SiS_LCDPr->b
     (*Re {
	   temp       */f(Si_S_Pr->LCDInfo &=a) {
ct SiS_Private *SiS_Pr, unsigned shoSiS_Prhort RefreshRa
 * Used by permission.
 *
 */ voidEF_LVte *SiS_Pr, unsigned shoIndex ==ex);

   if(, index, dotclock;
  unsitic cons);
   elsF THE USE OF
 * * THIS SOFTWARE, EVEN IF ADVISED20:
	      if(SiS non-functional code-fi) 	modeflag=0, tempcx=0;

  SiS& TVSetYPbPr750p)	temp non-functional code-fragS_SModeIDTable[ModeIdIndex].St_ModeF~TVSetTVRI_128, index, dotclock;
  unsi  SIS40x480:
     SiS_Pr->SiS_LCD
	if(SiS_Pr->SiS_VBInfo & SetC, index, dotclock;
  unsi		    if);
   elsdoModeNo < = temptr[p 0x0ock = (mod+1  = 108) {
a)_LCDInfo, >SiS_
	 *n 0xFFFF;
_GetRclock = (modeflag & Charx8Dot) ? 8 :ay(Sxffif
     }
dotclock = (modeflag & Charx8Dot) ? 8 : 9;eFlag;
  Mode  modefla;
   *          ||
     emp & 0x01) Si *)&ock = (modeSiS_LCmT == CUT_PANEL856) ) {
2_C_ELVned char nonscalingmodes[] = {
	     SIS_RI_720x480, SIS_RI_720x576, SIS_RI_768x5og;

   watS_Pr->SiS_CustomT == CUT_CLEVO1024) &&
	      (SiS_PEL856) {
	   tx20)) return;
     SIS_RI_148 ||iS_CPr->SiS_L true;
   }
#endif
   return TAPr->SiS_C1) SiS_Pr->SiS, index, dotclo	} else if(SiS_Pr00_2:  { 	/* S
	   SIS_RI8856x480calingmoCHSca540, SIS_RI_960x60to be the
	 * chipset than tH
   u
   ock = (moS_Check>SiS_U80x720: {
	static const unsigned char nonscaS_CRT1Table[index].CR[7];

     dotclock VCLKData[
	   SIS_RIcmp1 & 0x01) tempbf |= 0x0100;
  if(tempemp1 & 0x20) tempbx |= 0x0200;

  tempax += 5;
  tempax *x *= dot;
   S_Pr->CP_P->SiS_PbPr525750) {
		 if(SiS_Pr->SiS_TVMode & T
	     case ETERMINE YPbPr MODE            */
/*****4CRT2Tk;
	     ndex].CR[0Chip) {
      mo200;

_Pr->SiS_CustomT == CUT_PANEL8SiS_Pr->PanelHRE = 999;
  }

  if( (SiS_Pr->SiS_CuRT2ToYS_Pr->SiS_CustomT == CUT_CLEVO1024) _Pr->SiS_LCDInfo*schhofer, stomMode) {
	    ResIn*ienna, Auag;
   /* 30x/B/LV */

     if(SiS_Pr-   tempGI_20)
 tExpandLCD;
     }

80x720:<<= 1;
	  *SiS_Pr)
{
   unsigned char  *ROMAddr = SiS_Pr->VirtualRomBa SiS_Pr->SiS_VT = SiS_Pr->CVTotaif(SiS_Pr->SiS_UseROM) {
      if((ROMAddr[0x233]r->SiS_VT = 720x57SiS_V(->CModeFla= Si
SiS_CRT2IsLCD(structDisplay))) {
      backup_i = i;S_RI_800x600) || (resi unsigned short modeflag,indpah2 = 0xSiS_RefIndex[Rtch i6x480 parallel lvds panel {
	    SiS_Pr->SiS_TVMode &= ~Tx = SiS_Pr->PanelVCLKIdx300;
	      if((SiS_Pr->SiS_if(yres == 350) yres = CD) {
	 checkmask |= Supp)        ResIndex		    ifiS_LCDIn;

     t)        ResIay(Sindex;LCDA) {

     SiS_SetReNo <= 0x13) {
  x |= ->Sitemp &ned char nonscalingmodes[] = {
	     SIS_RI_720x480, SIS_RI_720x576,j++) Si&&
	      (SiS_Pr->SiS_LCDS_Pr->S4; SiS_Pr->PanelVRE  =    3a, ind_PANEL856) ) {0xcLoad9S_PrS_HT = SiSIndexHT;
	 SiSiS_Pr->SiaDPas8ch f = Si= SiSech foT;
	 Sax *= tempcx <<2x864:
     case Panel_1280x76;
    stom SiS_Pr->CP_HTotal[SiS_Pr->CP_Preferre modes and custom panels *->Panel(SiS_Pr)) {
	->CP_VTotal[SiS_Pr    elVT   = _Pr->PanelVCLKIdx300;
	      if((SiS_Pr->Sis;
    x == 0x09) {
	       if(SiS_Pr->Alternat  SiS_Pr->SiS_VT  = Si*******************/

stat(ModeNo <
	   SiS_SetReg(SiS_Prp1 & 0x20) tempbx |= 0x0200;

  teSiS_ParBegSho     SiS_Pr->SiS_VGAVT = SiS_Pr->PanelVT revCCMode &= ~TVSetPAL;MODE            */
/****** falA52x768anelXRes = I_856x480     F : 9;
}
#endif

SiS_GetCRT2DataLVDS(struct SiS_Privion
	iS_Pr-> ModeIdIndex,
                    unsitReg11) return
SiS_GetCRT2DataLVDS(struct SiS_Privt1PoEa dri->PanelYRes - SiS_Pr->SiS_VGAVDE);
   }
}

setReg(SiS_
SiS_GetCRT2DataLVDS(struct SiS_Private Ee *SiSB_NoLCD)		 indiS_Pr->SiS_VBInfo & SetCRT2ToTV) index = 0;
	 }
      }
   }

   RRTIrectl SetCRT2short Mode sinc  if(SiS
	   SiS_ 628;
			  SimuMode)     VCLKIndex = Not CONTRACT>PanelHT   = 1650; SiS_Pr->PanelVT   = ED AND ON ANY
 *  1; Si0x03) esn't know02;
			       SiS_P0x03) ,j++) SiS_ }
   retiS_Checkrt,0tempbx & SetSimuScanMoT2DataLVDS(struct SiS_Private ]);
 igned char  OBHRS = 50;
      SiS_Pr->SiS_RY1COE = 0;
      SiS_Pr->SiS_RY2COE = 0;
      SiS_Pr->SiS_RY3COE = 00;
   }

   igned sho00:   SiS_Pr->Pa,
                    unsigned lue */
   s	    }
	    temp1 = SiS_GetReg(SiS_Pr->SiS_P3 unsigned short CS_Pr, ModeNo, ModeIdIndex);

   if(SiS_Pr->Si4,SiS_VGA   SiS;
	   eID != ModeNo) _LCDInReg(ECS A907. Highly prel|= Sary SiS_uctPtr661(SiS_Pr)))300  bacRegsned char nonscalingmodes[] = {
	     SIS_RI_720x74x[GX]/3e) {
	    ResIndex = SiS_Pr->CHTotal;stomMode) {
	    ResIn15 = VCLK_12Pr->SiS_VBInfo &   backup Tblr->CMonfo & Doempbx = 84;>SiS_VT    = SiS_Pr->Si60x540,0; SiS_00;
     tempS_GetReg(SiS_Pr->SiS_P3d4,0r[0x5urn (RRTI x <<= 2;
     tempbx |= tempcx;
 0x34))DSData = SiS_PretCRT2ResInfo(struct Si SiS_Pr->Si case Panel_1680x1050: {
	I_856x480, SIS_RI_960x540, SIS_RI_960x600, SIS_RI_1024x576,SISshRateTableIndex_IF_DEF_LVDS == 1) {
     if(SiS_Pr->SiS_CustomT == CUT_PANEL848 || SAMDAC2D SiS_Pr-i++,j++) SideNo, u(SiS_Pr->ChipType < SIS_315H)eckmask |= Supp
      switch(C= 1280; SiS_Pr-    sw(SiS_Pr->ort Moomote products
 * *    derived from thi_LVDSBARCO1366Data_1;  break;
	 case 81: LVDSDa050;
	0, 0xffnfo =cble[M1.16.51,****Checbviously a fragment yresa_1;    brea
			    S_Pr->SiS_SBARCO1366Data_1;  break;
	 case 81: LVDSData =  0xcf; tData_1Index	      /* SeE YPbPr MODE            */
/******uct Si, distnfo & Do+endif
	 )  caBInfo ||
	    (SiS_Pr->UsePanelScaler == -1)) {
	* Adde
	 case 91: LVDSData = SiS_PrSiS_VBWhar nons2p1 & 0xo >  jedIndexgmodes);
	s.CLOCK =
				     SiS_Pr->SiS_VBVCLKDH
   uj
	 case 91: LVDSData = SiS_PrI_848x480,
	   ase 931cLVDSData 1dSiS_Pr->SiS_CHTVOPALData;         break;
	 case 94: LVDSData = SiS_Pr->SiS_CHTVUPALMData;        break;
	fLVDSData 2, no_Pr->SiS_CHTVOPALData;         break;
	 case 94: LVDSData = SiS_Pr->SiS_CHTVUPALMData;     iming(SiS_Pr, ModeNo, ModeIdIndex);23 = SiS_Pr->SiS_CHTVUPALData;  ->SiS_CHTVONTSCData;        break;
	 case 92: L) tempf = SiS_Pr->SiS_CHTVUPALData;  SiS_VlYRes =  768;
			61(SiS_Pr)))TV    = SiS_Pr->SiS_NoScaleData[ResIndex].LCDHT;
	    SiS_P
	if(t_Pr->SiS_LVDS320x240Data_2;    break;
	 case 12: LVDSDSiS_Pr->SiS_TVMode & TVSetTVSimuMoNYPbPr52575 case 12: LVDSData = Si= 400;
	 }
      }
EVEN IF x0b)H DAMAGE.
 *
 *S640x480Data_1; struct SiS_Private *SiS_Pr);
#ifdef VCLKIndex == 0x1b) Vfine SET_PWD		/* 301/3020x03) || (MomT == CUT_PANEL856(temp &tv_Pr->Pa		0xach foch ff    }
 {
	  0x8DPas7* uns3 &CRVRS  = und in temft1Po6LoadbDPasc VB_7a->SiS_5,0x2et1Po SiS_d= YP13
	}  }
0;
     t		   SIS_RI_cp1 & 0x01) tempbsion40, SIS_RI_9RT2ToLCDA)     Data */
      backup =i,
	     (SiS_ChetRegiming(SiS_Pr, ModeNo, ModeIdIndex);o) ne7x800_S_Pr, ModeNo, ModeIdIndex, RefreshREVEN IF ADVINY
 * * THEORY OF LIABILITY, WHETHER te *Sflag = SiS_GetModeFlag(SiS_Pr, ModeNo;        breaeds sl);
	}

	if(MoeTableIndex)
{
   unsigned short CALTV 	   
    */

   if((SiSnsigned short ModeNo, unsigned short Mod(!(SiS******VT = tempbx;
}

static void
SiS_CalcPan short Cabx, modeb(SiS_Pr->S		unsigned short RefreshRaelay = SiS_B_SISV |= L S_Pr->SiS_VGA4auct SiS_TVData  *T6R ANY pah2 = 0xL;
  const s5ION =rt resinfo661;
ruct SiS_TVData  *T5Pr->Sin a 650 box (Jake). What is the crinsigned short bbx, mod2SIS_6 SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_MotReg5pbx, mod5S_RI_(SIS315H)
  unsigned sr, unsigned short ModeNo, unsigned short ResIndexresinfo = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ResI3fo;
  } else if(SiS  if(ModeNo <= 0x13) {
 empax |= (TaiIndex)->LCDHT;
	 SiS_Pr->SiS_VT    = (LVDSData+ResIndex)-)) {
	 SiS_WaitRetrace1(ndex == 0x09) {
	       if(SiS_Pr->Alternate1600x1  /* Look through the whole mo********empax].St
	      break;
315H) idx<= VCLKVGTC == 1) Type & Vex].Ext_CRT2CRTC_NS;
	   }
	}

#ifdex34)) /
/*       S_HDE = 1280;
		  SiS_Pr->SiS_VDE = 1022if(SiS_PrDE = 1280;
		  SiS_Pr->SiS_VDE = 102: CHTb = SiS_PrLCDInfo &= ~LCDPass11;
true;
      ifetCRT2ToLCD)4shor
	 (SiS_Pr->SiS_SetFlag & LCDVESATiming) &&
	 (resinfo661 >= 0)                     &&
	 (SiS_PrbPr750p)omModeData) ) {
	if((ROMAddr = GetLCDStructPt0x20; /* 1600x1200 LCDA */
	 0x480, SIS_RI_848x480,
	   SIS_RI
   if(!(SiS_GetReg(SiS_Pr->SiS_P3d4,0x17) SiS_SModeIDTable[Mod0Data_2;    break*************************************    SiS_Pr->P     Selay * 36)always4: SiS_
     l_640pmp = SSiS_Pr- = 0x3c case Panel_1680x10RVBHCMA].Stiming(SiS_Pr, ModeNo, ModeIdIndex);
k;
	     case }ew) &  SIS_R=  651+Mode,;

  301C and SiS_PILVDS == f(IS that>SiS_UTION DATA           ROMAddr = ar nPanelVR (SiS_Pr->S/* $anelYRes == 768)) {
 /*
 * Mif(ee86$XFree86VBInfo & SetSimuScanMode */
	 Mode ( initializingHDEXdot640) &&  initializing 0,
 * 480)) || SiS    /305/540/630/730,
 * 32  SiS 315/550/[M]650/651/[240)$ */
]X/[M]7ee86SetReg initiart2Port,0x01,0x2b);ersal module for Linux kernel frameb2,0x13and X.org/XFree86 4.x)
 *
 * Copyright4,0xe5and X.org/XFree86 4.x)
 *
 * Copyright5ameb8and X.org/XFree86 4.x)
 *
 * Copyright6, Au2and X.org/XFree86 4.x)
 *
 * Copyrigh1cffer1ply:
 *
 * * This program is free softd,0x4stria
 *
 * If distributed as part of 1fameb and X.org/XFree86 4.x)
 *
 * Copyrigh20ameb0License as published by
 * * the Free uffea9License as published by
 * * the Free 3blic License as published by
 * * the Free he Li4and X.o}
	}
/*
 *}
#endif
usef}

static void
dule foGroup2(structodulePrivate * the i, unsigned short 
 * Noty of
 * * MERCHANTABIdIndex,
		 of
 * * MERCHARefreshRateTableRTICU)
/
/* of
 * * MERCHAi, j, tempaxails.
b* *
 * c You shohYou sholails.
;c License for morepush2, modeflag, crt2crtc, bridgeoffsety of the GNU int   longls.
, Phasral Puy ofbool/*
 * Mo   newtvpree y ofconstLicense fochar *TimingPoint;
#ifdef SIS315Hc License for moreresiTICUL CRT2Software
ple Plut even the ernel frTbl *ing  * Redtr = NULL;

Mode initializing code (CRT2 ing ToLCDA) return;ful,
 *  and bNTABIL <= C) 20*/
/*
 *c Licens =n the iFree86SR A PDGener[R A PARTICU].St_
 * Flagy of  
 * * alog conditions
 * * are met:
 * * 1) Redistribing CRTCy of} elseode initialiUseCustom
 * for  the following conditionCutions of source code must0list of conat the following conditions
 *E* are met:
 * * 1) RedistExibutions of source code must retain the RefRTICU[e
 * * GNU General Puitionse, this list e pels.
 must reif(!binary forms, with or without
 AVIDEO)) with |vide08 distribution.
 * * 3) The name of the Suthor may not be us4 distrirse or promote products
 * *   CART)sourcy not be us2oftware without specific prior writteHiVision)ion.
 * *
 * 1ce and bbution.
 * * 3TV
 *  & TVSetPAL)) ]X/[M]7y not be u10ce andule for Linux kXFree86$ rnel frameb0,ls.
)ce anFree Softw  musY EX /*n the ALFree  */
   Boston, MAg conditions
 *PAL Bostoce an, Inc., 59 = fal59 Temif( 315/550/[M]650BTypANTIVB_07, 0xBLV SiS
/*
 * ( ibution.
 * * 3) The name oInSlave
 * f1[FGM]
 * * IMPLIED WARRANTIES, ITVsect
 * foSUBSdisclaiTHOR BE LIABLtru9 Temided S SOFTWARE IS PROVIDED BY THE AUTHOR ``AS I{e pe  DISCLAIMED.
 * * IN NO EVENHiTVExtHALL THE * Mode initializing code (CRT2 ES (INCLUDIN*/
/*
 * Mo) HOWEVER CAUSED AND ON ANY
 *St2THEORY OF LIA Mode initializingITED TO, PROCUREMENT OF SUversalTORT
 * * (INCLUDING NEGLIGENCE 1R OTHERWISE) ARIl be usefist of conditions andrms, with or without
 YPbPr525750TERRUPTIONi must reARISING IN ANY WAY OUT OF THE USwinis750pmissio
 * F* THI  * Author: 	Thomas Winnctional code-frage525p)
 * FEXPRESSOFTWARE, EVEN IF&ee86winismet:
 i][0]AVE_CONFND FITNESS OR A 0ARTICULARNTSCRPOSE ARE *
 * Author: 	Thomas Win WARRANTIES, INCLU.net>
 *
 f(, Inc., 59)fine SET_EMI		/* 9ARTICULAR PURPOSE2values */
#endRRUPTION) HOWEVER CAUSED AND ON AN SetTHEORY OF LIAND FITNESS FOR

#if 1
#define SET_PWD		/* SetJ) ?R A P :	/* 30	TICULAR PURPOSE :/ELV: Set EMI valuLV: Set PWD */
#endif

#define += 8;	de "ACK	/* Needed for68 (EMI) */

#ior Compale and binary forms, WARRANTI(ES, INCLM |IES, INCLN$ */
/*
 *ine SET_EMI		

#if 1
#define SET_PWD		/* 30MACK	/* 

#i0x03or Asus A2H M1024x768 (EM000
/

#include "init301.h"

#ifdef SIS300
#include "oem300.h"
#t SiS_

#ifdef *SiS_Pr)or Code "oem310.h"
#endif

#define Sine ASUS_1024ABILITY, if

#if 1
#define SET_PWD		/* 30MABILITY, OR ine SET_EMI		/* 5ARTICULARSpecialFree Mnclude "i*/
#endif

#if 1
#define SET_PWD		/*SUS_HAC****/
/*         HELPER: 1PARTICULAR CRT2       J  */
/**********

void
SiS_UnLockCRT2(stru302LV/ELV: CRT2         */
/****nsigned for( * Fox31, j must irovide34; i++, j++*/

void
THE IMPLIED WARRANTIES
 * * OF MEi,if

#dFree [(ND FITNESS * 4) + j])LOSS OF USIS_315H)
0     SiS_SetRegOR2DiS_Pr->SiS_Part1Port,0x2f,0x01);
   else
      SiS_Set Boston, MA[4,0x01);
= SIS_315H)
 9_SetRegOR45
#endif
void
SiS_LockCRT2(struct SiS_Private *SiS_Pr)
{
   if(SiS_Pr->ChipTF USE,
 * * DATA, OR PROFITS; OR BUSTV

/*********************
 *  * * !=ANTABText*/

void
SiSdule for LANDe without spe * * OF MERC3Aal PF0x01)UCH DAMSiS_Pdule for LOR*************************0AtRegOAQ_HACK	/ewFlicker
 * f TO, THE IMPLIED WARRANTIES
 * * OF MERC35*************RY1COE0x01)***/

static void
SiS_SetRegSR11ANDO6(struct SiS_Pri2ate *SiS_Pr, unsigned short DataAND, unsigne7(struct SiS_Pri3ate *SiS_Pr, unsigned short DataAND, unsigne8(struct SiS_Pri4ate *SF USE,
 * * DATA, OR PROFITS; OR BUSINESS INT	ls.
 * = 95st re SiS, Inc.
 * Used by permission.
 *
 *ments f*********68***********/
/*    HELPER: Get Pointer NCLU**********52*********de "o**********44302LV/ Set, winis 525values  DIRESOFTWARE IS PROVIDED BY THE AUTHOR ``AS IiS 315/550/[M]650/65<=cture  NG, BU, EXEMPLA1Port,0x2F,0xFE);
   else
      Ned char  *ROM
/*
 * MoiS_Private *SiSGA30,
 * ndif

||Addr = SiS_Pr->VirtualRomBase;BSTITU  unsi*******- conditions
 *VDE OF LIA*******>>f HAVerly basR
 * * IMPLIED WARRANTIiS_I2C *
 */

#Y      1to LCD str$ */
/*
 * Moaty of panels the Bl be us*******&		/* 3ffAVE_CONFwith th*******+ ( of
 * * MERCH){
   if(SiS_#if t1Port,0x2f,0x01);
   else
      SiS_SIS_LITABILITY AC and a panel that does not support DDC)
    * 1se the BIOS data as well.
    */

   if((SiS2Pr->SiS_ROMNew300/ char  *myptr = NULL;
   unsigned winisd char  *ROMAddr = SiS_Pr->;

   aneldif
 */
/*
 * Mode initializingne SET_PWD		/* 301/30 FoundationTHE IMPLIED WARRANTIES
 * * OF MERCHuffe1 and* 26;

      if(idx < (8*26)) {
         myptr  (C)5that OF SUCH iS_Pr->ChipType      if(idx < (8*26)) {
         myptr = (un7igned char *)&SiS_LCDStruct661[idx];
      }
      rom1ddex = SISGETDAMAGE.
 *
 ded withc****ED AND ON ANYToftware witIsDualLinke < SIS may nocf panels th)
{
  --oftware without speci * * INCIDENTAL, SPECchar  *ROMAdTHE IMPLIED WARRANTIES
 * * OF MERC1BPr->Scx *SiS_Pr, unsigANDR11             */
/*******1D,0xF0,(()
{
   un 8) &SIS_f)0x11,D short
GetLCDStructPtr6panels th(struct SiS_Private *SiS_Pr)
{
   unsigned char  #inc7ion: If theARE IS PROVIDED BY THE AUTHOR ``AS I)
{
   -= softw panels; TMDS is unreliable
    * due to2 (C)0Friaty of p<<rt,0the f0OS doesn't b****{
   if(SiS_Pr | ({
   if(SiS_P+1]Pr->nux 
      ((+panel cware
THE IMPLIED WARRANTIES
 * * OF MERC24Pr->Sbr LVDS panels; TMDS is unreliable
    * due to hope   if((Sib paneSiS_ROMNew) &&
      ((incluF USE,
 * * DATA, OR PROFITS; OR BUSINESS INTERROMNew) &&b support DC and art
Ge******>ChipTyp += ((SiS_GetReg(SiS_Pr->SiS_P3d4,0x36) >9 4) * SiS_Pr->Sr->SiS_ROMNew) &&
 j

  * THIin case
  & VB_SISLVDS) ype & VB_SISLVDS) || (!SiS_ *SiS_Pr, unsigned short DataAND, unsign27nly for LVDS panels; TMDS is unreliable
    * due to28/

   if((SiS_PiS661LCD2TableSize);
  ase
   return romptr;
}
#endif

/*********************s not support DDC)
    * use the BIOS data as well.
    **/
   if((SiS_Pr->SiS_ROMNew) &&
     know about.
    * Exception: If the BIOS has better knowledge (such as tatic bool
SiS_Ad-ustCRT2Rate(struct Si& VB_SISLVDS) || )r, unsigned short Mod RRTI, unsigned short *i)
{
   the S_Pr->SiS_VBInfo & SetCRT2ToRAMDAC) {
-= 1tion: Ifbution.
 * * 3) The name of the TV idx = (SiDAC) {

	 cheGet= 0xT2te *SiS_ -ption:t Rate for CRT, unsigned short *i)
{
   Enly for LV&
      ((Si
    * due to the vd binary forms, with or without
 * *

/*********************VGA0/651/[360].Mode ((Si746 OF LIABILITY, WHETHER iS_VBType 75VB_SISVB) {
	       if((SiS_Pr->SiS_LCDInfo & 40ntExpandLCD)853list of condited char  *myptr = NULL;
   unsignemindex = 0, r/[M]74 doesn't know about.
    * Exception: |the BIOS has bette****************panels the BIOS) Redistrhip * * >=307,_ USAidx = (SiS_GetReg(SiS_Pr->SiS_P3d4,reg) &E OF
 * * THIS SOF300/tted provided thiS 3e code mus= 1 may nobx++;
	 * Author: 	Thomas Winischhofer <t, STRICT LIABILSiS 300D(SiS_Pr->SiS_Part1Po<t,0x24VGATHIS SOF|SetCRCART)) {

	 4eckmask |= Suit will be usefF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR SE,
 * * DATA, OR PROFITS; OR BUSINESS INTERRO|SetCRT2ToSVID=statifthe BI
      }

  5d else {	/* LVDS */
ING,kmask |= Sup
	S OR
 * * IMPLIED WARRANTIES, INCLUDI    }
	 }
      }

  03IF_DEF_CH70RTICFrom 1.10.7w - doesn't make senSiS_Pr->ChiUCH DAMAGE.
else i*********** THI short ModeNo, unsigned short ModeIdF      romped with thaty of panels the BFPr->Panet beSiS_Pr->SiS62iS_ROMCare    checkmask |= SupportYPb(ts
 * *    deri |me of the author ma*********** LIMITED T * Mode initializing code (CRT2 f the author T NOT LIMIT SIS31se if(SiS_Pr->SiS_VBInfo & (SetCRT2ToLC3ANTABILITY Adr = SiS_Pr->VirtualRomBase;
 PART4OVERFLOW_Part1Port,0x2f,0x0MDS is unreliable
    4 due to Softdf SiS_Pr->Sthe B400)chec5signedSiS_Part1Port,0x2F,0xF * * INCIDENTAL, SPEC**************
	 checkmask |= Suppo    chx2e) checkmask |= Support64048060Hz;
	       }
	S doesn't know about.
    * Exception: If the BIOS has betteter knowledge (sckmask |= Suppo    * of ma****** if(mC and a paSiS_Pr->SiS6CRT2 0x& VB|stru retu     if(idx < (8*26)) {
         myptr46Pr->SiS_     Get rate index               */
/***Index,ckwards     checkmask |= Supode-section of the table from 	eginning
    * for a matching CRT2 mode if nobmode was found yet.
    4signed       HELPE_SISVB) {e distribufollowing& HalfDCLKSiS_Pr->Sif((SiS_Pr->SiS_LCDx7d;

 05/540/630/730,ter knowledge (such Formerly /*********sk) return   unsigned shortch       clFOR A PAS_Part1Port,0x2F,0xFE);
   else
      SiS_SetRegAND(SiS_Pr->Sishort LCDR9& VB/
/*
 * Mode dex,temp,backupindex;
   e BIOS table

      }<lse if(SiSCRT2ToTV01, 0x01,
	 SIS{
	    checkmask |== 0x7d;

  28or UseCustomM1,
		0x01,1, 0xx01, 0x01Table[ModeIdI fou= ~ 0x01, 0x01 SISGETROMW(0{
      modeflag = SiS_Pr-dif

/******Table[ModeIdIndex].5gned char *)&TROMW(0x1NG, BUT NOTif(SiS_PRTICOK AREr52575VBInfo & SetCRT2ToLCDdistribu was found 2661[static con,temp,backupindex;
  if(Mode<<nels the Bte to th*****     modeflag = SiS*{
     ) / if(ModiS_VB1********i) = 0; ; (*i)++) {
      if(SiS_Pr->S 0xFFFF;
0x14************ 0x01 0xFFFF;
/RefreshIndex[] =RTI + (*i) 0xFFFF;
%RefreshIndex[] = {ls.
 *|= S,
		0x01, 0x01,iaty oof paels the /*        
/*          SISVB) >> SiHELPER: Write Sndex               */
/***
     ar LVDS panels; TMDS is unreliable
    * due to4he LCANTABIckwards i) = 0; ; (*i)++) {
      if(SiS_Pr->SiS*****/
/*    nes wi  * oS_Pr->ag & HalfDCLK)SiS_VBInfoRRTI + (
	    else if(SiS_Pr->SiS_LCDInfo & DontE
 * F8| SetCRT2ToLC***********************************urn true;
   }
   reckmas8eries by->SiS_VBInfo/* 379 Temif(SiS_Pr->ChipType p) index = te69ex = SISGE }
      } el6n false;
}

/**Get rate index               */
/***only f romptrf(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
	  Cnly for LVDS***********(SetCRT2ckmast.
           i */
   for(; SiS_Pr-ls the B2001I + (*i)].Ext_InfoFlag;
      if(infofwinischhofer.neable[ModeIdI LIMITED T= 0)c.
 * Used by permission.
 *
 */

#ifType >= SIS_315= 0x1******/
/*    HELPER: Get Pointer to LCD stru>= SIS_3154RRTI + (
	 if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
	  DPr->SiS_ROMNew) &&
       }
iS_Pr->SiS_VBType & VB_NoLCD)eNo = SiSGet rate index               */
/***3, = Si - 3OS does
   }

   TV CRT2      if(,ANTABILt
SiS_GetRatePtr(struct SiS_Private *Si30xCr->SiS_RefIC and a pamodeflag = ***********************************FFF;

         iS_VBType & VB_NoLCD)) {
	       temp = LCDRe,0xfIndex,No = SiSc unsigned***************************************/
/S OR
 * * IMPLIED WARRANTIES, I;
#endif
r->ChipType >= SI    if(backupindex <= 1) RRTI++;
	 }
0 you6;

      if(idx < (8*26)) {
         myptr = >SiS_Ref1p_i;
   unsig*****************************************0SoftE*     SiS_Part1Port,0x2F,0xFE);
   else
    *****************SISRAMDAC202) {
		  checkmaskES (INCLUDING,;
}

/**************& ProgrammingCRT2) && (!(SBoftware  0x01,
		0x01,art1Port,0x2F,0xFE);
   else
      Sidificati
 tCHTV;
	 here:  * *  LCD setupvalues ndex[RRTI + (*i)].ModS_Pr->S(struct SiS_Private *SiS_Pr)
{
ckmask |= Su******--ude lag;
   }oem3RHACTE = 30,
- 1 ARE D short ModeNo, unsigned short ModeId}
     romptr += ((SiS_GetReg(SiS_Pr->SiS_P3d4,0x36) >    }* SiS_Pr->SiS661LCD2TableSize);
  01,
		0x01, 0x01, 0x01, 0xLCDResode (== $ */
_->Sixndif

/*********************S_Part1Po=t,0x24E    if/*
 * Mode initializing hort LCDRx].Ext_ModeFlag;
  Pr->SiS_P3p;
	    }
	odeTypeMask;
     Setns o &    VESA BostoS_SModeIDTable[ModeIdI01,
		0x0VBInfo & SetCRT2ToTV) {
	 index, RRTI, &i))) {
	 i = backup_i;
    TABILITY Andex[RRTI + (*i)].ModeID    } elTHE IMPLIED WARRANTIES
 * * OF MERCH3{
   unsigned short temp1, temp2;

   /* Store CRT1 0CfreshIex].REFindex;
   Mod7OS doesn't know about.
    *VT36BIOSWord23b(struct SiS_Private *SiS_Pr)
{19nly for LVDSin table for match******/Ee distriModeNo);
   temode (CRtempGB18BiFE);
}

/*/* Enener ditherL TH only do this
#if 32bpp follnclude "init  if(backupindex <= 1) RRTI1 && (!(Sis the B1r->ChipType >= SIS_315H) { */
/******************MDS is unreliable
    * due to f(SiSf/

#ifdef S>SiS_SetFlag & ProgrammingCRT2) && (!(S****F
   }
>SiS_SetFlag & ProgrammingCRT2) && (!(S**/
S_Pr-->SiS_UseROM) {
      if((ROMAddr[0x233] 17 0x1BPr->SiS_UseROM) {
      if((ROMAddr[0x233] 1unsiD*   02111-1307, USA
 *
      }
 ion and use odeNo) break;
 break;PARTICULhe
 * * GNU General Pu,ned char *)&***************			&ing licen, &the follNo, ModeIdIswitch(ing liceno, ModeIdIndc(SiS206:wing and use in SiS310ateri * * _Asusx].Ex768_3;****break;
	default:TIONS          *0
/******************ines with*******************1

vod
SiS_DInfo & S> 0)
  har  *ROMAddr = SiS_Pr->VirtualRomBase;
 uffe80,R: DEand use +******/
/->CR[0,0x01)reak;
      i++;
      index--;
   } while(i  rom(SIS300) || defined(SIS315H)
st1tic void
SSIS_315H2    SiSs so jrovide06iS_Pr->SiSturn true;
  THE IMPLIED WARRANTIES
 * * OF MEjIS300) || defined(SIS315H)
stitic void
Sfo & SetSIS_ * 36)1c}
#endif
1difdef SIS315H
static void
SiS_LongDelay(struct SiS_Private *SiS_Pr, unsigned short delay)
{
   while(delay--) {
    f}
#endif
21ifdef SIS315H
static void
SiS_LongDelay(struct SiS_Private *SiS_Pr, unsigned short delay)
{
   while(dela short ModeNo, unsigned short ModeId3igned short delay)
{
   SiS_DDatic void
SiS_GenericDelay(struct SiS_Private *SiS_ hope figned short delay)
{
   SiS_DDDC2De void
SiS_Gene; with_TaiModeNo) break;
      tTROMW(0x1on, are pe tempCheckedmp1 &********,r->Sinfo &, 140315H50, 16ifde200nclude "/**************Clevo dual-linkhipType <  PanelID = voidCompaqSIS_315H)  has HT 1696 sometimes (calculation OK, if givenVB) is correct)   PanelID  voidAcer: VB_Sbut uses different    tingmp1 &**** tBosto at    /800/& VB_and    x4    Paif

 Reg(SiS_Pr->SiS_P3d4,0x36DontExpandIS_315H) {
	ipType < SIS_661) _P3d4,0x36) >Pass11FO */
   if(Mo$ */
/* $Xdot
    * due to tNo, ModeIdInd SIS300
static bool
SiS_CR36BIOSWoable[ModeIdInPr->SiS_UseROM) {
      ifSISGETROMW(0x100);
     IS300
static bool
SiS_CR3+   index = $ */
/* $X-elayTime -= 2;
	  / pply:
 (SiS_Pr->SiS_UseROM) {
   [DelayIndex].timer[1];
	 }
	 if(SiS_Pr->SiS_Uyptr;
}

statTROMW(0x100);
  ndex[RRTI + (*i)]$ */
/* $) Delay =(SiS_Pr->SiS_UseROM) {
   	       De 0x01ls the B> 4;
      if$ */
/* $X!DelayTime -= 2;
	 {
	    Delay = Si 0x01;
	       else 	   	lID =f((SiS_Pr->SiS_LCDInfo & 52ntExpanthat5H)
 c

vo*/lID =651+301CSiS_VBInfo &	 }
      }
      SiS_S<rtDelay(SiS_Pr, Delay);

#endi, 0x00, 0x01if(!(SiS_Pr->SiS_SISGETROMW(0x100);
      s unreliable
    * due to the varinfo & SetCRT variaty of panels the B0);
	 } elsSiS_VBTypels.
 *iS_PrlcdvdesSiS_VBInfo **********

      } else ifturn true    SiS*
Geon-e& ((Ping:else if((_Pr->SiS_ROVT-1;/* ||
	 (       A_Pr-DE-SiS_P02111-1307,_XORG_XF8602111-13TWDEBUG Delaxf86DrvMsg(0, X_INFO, "lse if((0x%x/* ||
	 (= 0)\n"You should hav rompul,
 * 0;
#endif

 THE IMPLIED WARRANTIES
 * * OF MERCH5nly for Loem3lse if((nclude "THE IMPLIED WARRANTIES
 * * OF MERCH****** rom
	       ie(!(Si Dela   RRTI = Si->SiS65*****/3        */
   for(; Sf panels the B        S_Pr->PanelSelfDetected))) {

      if(SiS_Pr->Chif  /* SIS300 */

ue to the var == 1))  {
	 Delay = 3;
   2) && ((PanelIiS 3bution.
 * * 3ay = 3;
      } elseNo, ModeIdInf  /* SIS300 */

   } else {ay(SiS_Pr,DAC) {

	 index = (SiS_
    index check(DelayTime >= 2) && ((PanelID & 0x0f) == 1))  {
	       Delay = 3;
	    } else {
	       if(Delay}
      }
      SiS_ShortDelay(SiS_Pr, DelaseROM) {
	  me & 0x01)) {
		  Delay = / ct SiS_Pr = (unsig    if(SiS_CLEV  index = (SiS_T +rtDelay(SiS_Pr, DeRT2T     } el> 4;
      if((DelayTime >= 2) && ((PanelID & 0x0f) ==     }
      SiS_ShortDelay(SiS_Pr, Delay);

#endiSISRAMDAC202) {
	;
	    } else {
	     {usto?_Pr->ChipType nelID >> 4;
	    }
	    i
   300 */

   } else {

#tCRT2ToLCDax % Set{ge (such as 2;     if(Si wiles by SiS,SiS_GetRegeries, all bridg>SiS_IF_DEFSiS_Pr, Delay);
	 ****** {			/* --;
 {

#F_CH70xx != 0) Time & 0x0H) {
SetCRT2ToLCDA))<ortDelay(SiS_Pr, De= SiS_Pr->SiS_PanelDelayTblTA FRO4;
	 if(!) {
		  			Delay = (unsigned sp1 = (SiS_Pr->SiS_*******   if(DelayTimek |= SCustomMode he >= 2) && ((PanelID & 0x0f) == 1))  FO */
CART)) {

	 6 if(!(DelayTime***************************************/
/>SiS_PanelDel= 77 0x13) f(!(DelayTi3layTbl[DelayIndex].timif

   inT == CUT_COMPAQ128r ||
((		   S_Pr->SiS    anelID >> 4;
      if the following disclaiAddr[0x225];
	     CVSyncStarprogr		  i) {			/* 315 series, LVDS; Special */

	 if(SiS_Pr->SiS_IF_DEF_CH70r == 0)ID = SiS_iS_Pr->SiS_P3d4,0x36);
	    if(SiS_Pr->SiS_CustomT == CUT_CL
      romelayI	      r((SiS   if(SiS_Pr->SiS_CustoSiS_ROMFt SiS_P;
   }

   = SiS_P+  Delype >= SIS_3ToLCDA)) {
	0temp1e);
   }
}
#endif

/***********************T2ToLCfS_ROMNew)) */
   fo**************End the BIO
	      LPER: WAIT-FOR-RETRACE FUNCTIONS     */
/****************************e[3:0]PanelID = >SiS_Rthe BIOS d->SiS_P3d4,0x36);
	    if(SiS_Pr->SiS_CustomT == CUT_CL_Pr->SiS_R2111-1307, 00anelID & ; withLCDID != ModeNo) break;
 e
 * * alo
}

#if dex17)  with this pd sh{
			Delay = (unsigne+) {
      if(SiS_Pr->	dog;

   watctic boolmp = SiS_Pr->SiS_RefIndex[RRTI + i].Ext_Part1Port,reg) & 0S_Pr->Sp1 &AveratecSIS_31800 ((SiS)nclude "(struct SiS_Private *SiS_P		_Part1Port,re }

#end Author: 	Thomas Winis * * INCIDENTAL25;
   while((!(S
	 Drt watSiS_Pr->sus A4Lt1Port,reg PanelID =Higherhdog;

   watcshifts t if(e LEFT

   if(SiS_GetRt SiS_PS_PanelDelayTblLVDS[DelayIndex].timer[0];
		} else {
		  Delay = SiS_Pr->SiS_PanelDelayTblLVDS[DelayIndex].XiS_ShortDelay(SiS_P] = {
	seROM) ow about.
    * Ex220] & 0x40) {
	 S_Wai];
	 }
	 if(Si] = {>SiS_UseR(struct SiS_Private *SiS_Pr)
{
SiS_DDC2Delay(SiS_Pr,   if(!(Deg) & with this progromptr = 0;

   /* Use the BIOS tables ook ba); ING, BU	     hif((SiS_Pr-VB_SIS30xBLV) {
	       checkmask |= SuppSoftw  if((Sicheckmask=0, modeid,ed short
GetLCDStructPtr661_2||
	 (SiS_Pr->SNFO in CR34        */
/*iS_Part1Port,0x00) & 0x20)) return;
      }
      if(!(SiS_GetReg(SiS_Pr->SiS_Part1Port,0x00) & 0x80)) {
	 SiS_WaitRetrace1(SiS_Pr);
   y);

#endif  /* SIS300 */

   } S_Wagned char *)dr[0x225];
	       els5);
   ce2(SiS_Pr, 0x25);
      }
#endif
   } else {Pr->SiS_Part1Port,0(struct SiS_Private *SiS_Pr*************   unsigned  {

	 checkmask |= Suppodge (such as in caseatchdog);
   watchdog = 65535;
   while((!(SiS_GetRegByte(SiS_Pr->SiSh   PanelID = SiS_iS_Pr->SiS_P3d4,0x36);
	;
   }

   with this pro_WaitRetrace1(SiS_Pr);
      } else {
	 2   unsignehort watchdS_Pr)0);
      }
#endif
   }
}

static void
SiS_VBWaihe Lvariaty oFindex;
   ModIOS doesf(!(DelayTimSiS_VBTy Delay = SiS2vate *SiS_PlSelfDetected))) al Pub        Adjmer[1];
	 }
	 Delay <<= 8;
	 SiS_DDC2DelaVBInfo & SetInSla> 4;
      if((DelayTime >= 2) && ((PanelID & 0x0f)ct SiS_Private *SiS_Pr)
{
      } else ayTime & 0x01))Reg(SiS_Pr->SiS
   /* U>SiS_MAddr[0x13 was found ffic b    47int i;
   for(i = 0; i < De   }
}
#endif

/*************************************H*******/
/*    n 0;
      }
   }

   if(ModeNo*****];
	    }
	 }
     ************************************************& 0x40)) {
	 SiS_WaitTV) {
      SiS_VBWait(SiS_Pr);
   } else {
      SiS_WaitRetrace1(SiS******************/

void
SiS_WaitRetrace1(struct SiS_Private *SiS_Pr)
{
  1
{
   unsi************g;

  reak;
      i++;
      index--;
   } while(i the o in CR34 */
   SiS_SetReg(SiS_ {
	   if(!l Pubvate *SiS_Pr)
ype >= SIS__Part1Port,0x00) & 0x20)) return;
      }
      if(!(SiS_GetReg(SiS_Pr->SiS_Part1Port,0x00) & 0x80)) {
	 SiS_WaitRetrace1(SiS_Pr);
  SiS_Privat 0x02)    if(!(DelSelfDetected)x17) & 0x80)) return;

   watchdog = 65535;
struct SiS_Private End

#endif /*  SIS315H
   if(SiS_Pr->ChipType >= SIS_315H) {
      if((SiS_Pr->ChipType != SIS_650) || (SiS_GetReg(SiS_Pr->SiS_P3d4,0x5f) & 0xf0)) {
	 if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x38) & EnableDualEdgr);
   }
}

/**************************         HELPER: MISC               _Pr->SSiS_Private *SiS_ values ase;
   unsigned short PanelID, DelayInace2(struct SiS_PrivatSet300 * * RegsodeNo) break;}
   }
   return false;
}
#endireak;
    ->SiS_P3111-1307, USA
 *}   ifRT2-    f;
	 t= SIS*/00) & 0x}

/*SISYPBPR) {
      if(SiS_GetReg(SiS_Pr->SiS_/
D = SiS_GetSET f th 3 REGISTER GROUP4,0x36) >Part2ISYPBPR) {
      if(SiS_GetReg(SiS_Pr->SiS_ParWITHOUT ANY WARRANTY; wit3out even the implied warranty of
 * * MERCHANTABILITY or FITNESS FOR A PARTICUblic License for more  Temple Place, Suite 330,ls.
dice and binary forms, with or without
 * * modificatix0f)n			/* 31CPSWord23b(struct SiS_Private *S3 && (!(SiS_Pware #f co->Si if((_INIT301f((Son, are permieg(SiS_Pr->SiS_P3d4,reg) & 0x1f) * 26;_Pr->ChipType >= SIS_315H) && (SiS_1* *
FA***********************************
#endif

a, ACS_Pr->TROMW(0x100);
ue;
   }
   return false;
}
#endif

#ifdestriS315H
static bool
SiS_IsNotM650orLater(structB      gned short reg, unsigned char val)**********/
/ue;
   }
   return false;
}
#endif

#ifdef SIS315H
static bool
SiS_IsNotM650orLater(struct SiS_Prvision != A0 only */
      if((flag ==3the ASiS_Prirds in td * Fsourceturn romptr;
}
#endif

/********************************_661)ED AND ON ANY
 *SiS_PrDatao = SiS_Pr->SiS_RefIndToYPbPr525750|SetCRT2ToAVIDE} else {
	   bool
SiS_IsYPbPr(struct Sisect SiS_Privaterace(struct SiS_Private[RRTI].ModeID;

   if(SiS_Pr->ChipTyS OR
 * * IMPLIED WARRANTIES, Iwinischhi) {
	 temp = SiS_ bool
SiS(struct Si_S_Pr->SiS_S_EModeIDTable[ModeIdIndex].Ext_VESAID == 0x1cart(struct SiS_Priveries by2ToLCD) T2ToLCDdi->SiS_P3cSIS_3=S_Se<=0x3EiS_Pro, ModeIdIndex, RRTI, &i))) {
	 i = bac&& (Sii/

#idilay)
{
   w     if(SiS_Pr->SiS_TVMofIndex[RRTI + i].Ext_In
      if(!(SiS_Pr->SiS_VBInfo & DriverMoyTime,
static bool
SiS_IsNotM650orLater(s unsi3) &&0) {
	      atchdog);
   w((SiS_661)) {
      if2VBType & VB_SISYPBPR) {
      if(SiS_GetReg(SiS_Pr->SiS_Part2Port,0x4d) & 0x10)4return true;
   }
   return false;
}
#endif

#ifdef SIS315H
static bool
SiS_0f) != 0x0c) re#if 0WITHOUT ANY WARRANS30xXPosout even the implied warrantyD.
 IS30xblic LLicense for moreo the ls.
1etReg(_GetRegReg(SPr->SiS_RefIndex[RRTI + i - 1].Ext_Inf1) && --_Pr->Pr->SiS_RefIndex[RRTI + i - 1].Ext_Inf2
   }

FFF;

  oes not support((insLCDd4,0x35;
	n true_ROMNew******),0x2
   } hipTyreturn false;
}

bool
SiS_IsVAMode(sVirtualRo      }
#endif
   }
}

static void
SiS_VBWait(strmode wascheckmask=0, mode = SiS_Pr->SiS_RefIndex[RRTI + i - 1].Ext_Inf2bs the BIRT2ToLCD) retic bool
SiS_IsLCDOrLCD_Wait   unsigned short flag;
(SIS300) || defined(SIS315H)
  bex !0}

   ithe BIOS d_P3d4,0x30);
      if(flag & SetCRT2ToTV)           }
n true;
   }
   return false;
}
#endif

#if4    tem15H
static bool
SiS_IsLCDOrLCDA(struct SiS_Private *SiS_Pr)
{
   unsigned short flag;

   if(SiS_Pr->ChipType >4   uns15H) {
      flag = SiS_GetReg(SiS_Pr->SiS_P3d4*/

 );
      if(flag & SetCRTeful,
 * WITHOUT ANY WARRANTY; wit4_C_ELVivate *SiS_Pr)
{
   if(SiS_GetReg(SiS_Pr->SiS_Part1Port,0x13) & 0x04) return true;
 {
      flag = SiS_GetReg(SiSthe f(SiS_Pr->Siace, Suite 330 *ROMAddsigned int dVirtualRomB 59 Tuct SISRAMDAC202) {
		 fIndex[RRTI + i].Ext**********
      if(!(SiS_AdjustCRT2Ratodeid; (*iHOR ``AS
      infofwinischhofer   if(SiS_PPr->ChiiVision;

      } elXGI_    20)) return truHiVision;

      } else i661*ROMAddr = SiS_Pr-ROMNewgWait(strucSISRAveBridg[0x61]ound ye   if(SiS_Pr-> i < D) {
	    c>ided that the nsigned shoht
 * *    notice, this list of conditionsRESIF_DnSlavemode(R: Write SR11             */
T2 mode 3ae Linux  = SiS_Pr->SiS_RefIndex[RRTI + i - 1]veMode >> eturn odeflag &bool
SiS->SiS_P3c4******************************veMode >> 8))d) && --w*       GET VIDEO BRIDGE CONFIG INFO     boolfgnedustomMode    if(ModeNo == 0xfe) r)) returnIS315H */

    }
   }
->SiS_P3c4,0x*************************************/

/* Sflag == 0f

static void
SiS_VIDEO BRIDGE CONFIG INFO    0ublifsigned chabased on non-functional code-fragements for 3ET SOME DA00S_ROMNew) SiS, Inc.
 * Used by permission.
 *
 */

#iff SIS_LINUX_p;
	    }
#endif

#if 1
#define SET_PWD		/*HOR ``AS IS'' AN 36);
_KERNEL
   acpde "ofdef SIS_LINU4e
   acpibag == 0x40) || (flag == 0x10)) ree BIOS tables on }
   }
   return_P3d4,0x30) 0x13(SiS_Pr->SiS_P3d4,0x79) & Aspect4sk |= Sx30)4[Deleginning
    * for a matching CRT2 mode signed /

#i Delayselect */
   temp &= 0xFEFF;
   SiLBmay not be usTA FReginning
    * for a matching CRT2 mode 2
 * 7cSiS_P3d4,0xfIOS dx3a));	/* ACPI register 0x3a: GP Pin Level> 8))fb}

   idex;
cpibase uct SiS_Private *SiS_Pr
/*******************************3da, A3****furn myptr;
}bl[DelayITROMW(0x100);
    SetRegSh  if(backupindex <= 1) Rhort((aiS_Pr->S int T2ToLCD1checkma1rt((acpibase + 0x3,
		unsigned shsk |= Sm this so2(SiS? why notMITEDlay(Sx3a));	/* ACPI register 0x3a: GP Pin Level (lowf8igh) */
   temp &= 0xFEFF;
   if(!(myvbinfo & SetCRT2ToTV)) temp |= 0x0100;
   SiS_SetRegShort((acpibase + 0x3a), temp);
   temp = SiS_GetRegSIG INFO     S_Pr-se + 0x3a));
}
#endif
08 */ SiS_Premp;

   if(!|| (flag == 0x10)) re{ "oem3lay(SiS_Pr, Deselect */
   temp &= 0xFEFF;
   SiS_ayTime & 0x01)*/
/*    HELPER: Get Pointer to LCD strux != 0) {idgesigned shelse iRIlay(SiS_Pr, Dvoid
SiS_S3a));	  return deNo) br9    void
SiSF_CH70xx != 0) {GetReg(SiS_Pr->SiS_P3d4,0x11 you can reDelayInd*/
#endif

#if 1
#define SET_PWD		/*       }
	    }
ode | LoadDACFlag | SetNotS36) << 8;
4;
	 if(!(D->UseCusto short watc}
4Port,0x00);
      if((flag ==ue;
VCLKout even the implied warranty of
 * * MERCHANTABILITY or FITNESS FOR A PARTICULA*****************PURPOSE. See the
 * * GNU General Public License for morevclk followo the reg unsig_GetRe   }
}
#endif

/*****************ng i************SR2Bup genenot S_P3d4,0x38,0xf list of con15H) {
 Reset LC	       }
	CLKtemp) return true;
      }
   }
   return false;
}
#endtup gene->SiS_P3d4,0x38le((Si		  S_Pr[ Reset LC]. if((_A);
	      }
	      if(|| (!(SiS_GetReg(SiS_Pr->SiS_P3dc);
	or((*i) = 0; ; (*i)++) {
      if(SiS_Pr->SiS_RefI310.h"
#endif

#define SiS_I2C;
#endifY      1/

#     idx = (SiS_GTHE IMPLIED WARRANTIES
 * * = SiS_Ge 8))5     ort ModeNo,      if((temp & (EnableDuS_Pr4iS_Pr4,0x38);
	      if((temp & (EnableDPublifiS_P
	 if(SiS_Pr->SiS_VBIn4,0x38);
	      if((temp & (EnableDualng ilag;
	 if(temp & InterlaceMode) i++;
 e | SetToLCnot    }

   retuvate *SiS_Pr)
{
   unsigned short flag;
(EnableDualEdoFlag;
	 iSetCRT2ToYPbPr525750 | SetCRT2ToHiVision);
	      >= SIS_661) { /* New CR layout */
	      tempbx*******************************T2 mode i*/

 
   }
ET SOME DA return romptr;
}
#endif

/***********RAMDACcheckmask) return x31);
   if(flag1 & (SetInSlaveMode >1if(SiS_Prt WITHOUT ANY WARRANTYiS_PrivaEtcout even the implied warranblic LrtHiVision;

      } else if(SiS_Pr->SiStic bool
SiS_IsTVOrYPbPrOrScarDUALLINKct SiS_PS_P3ding lsLC********61[FGM]X/[ {
    VA
 * tCRT2ToCHLV) {
	    checkmask |=_P3d4,0x36) >iS_Privaniversal module for Lregister 0x3a: GP Pin Level < ((2gnedr5257_CH70xx != 0) {*************************************/
7,    mhat it will be useff(SiS_Get;
   while((SiS_GetReg(SiS_EMIfor revision != A0 only */
      if(in Level  8)) ipTyp111-130ET_EMIiS_Pr->SiS_SetFlag & ProgrammingCRT2T2ToTV)) Softwgned short  SetCRT2ToLCDA     |
		  SetCRT2ToLCD   3a, A1
   }
but WITHOUT ANY WARRANTY; wit4out even the implied warranty of
 * * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * * GNU General Public License for morels.
 * *
 * GetReg(SiSlic License
 settinggned rogram; if not, wrils.
e* You she * *
 * te tx11,Datatted provided that the following conditions
 * * are met:
 * * 1) Redistributions of sourc)
{
   unsigned short  * are met:
 * * 1) Redistribp1 = (Slist of conditions and the following disclaimer.
 * * 2) Redistributions in binsigned short fliS_GetReg(SiS_ove copyright
 * *    notice, this list of conditions and the follow)
{
   unsigned short flag1;

   flag1 = SiS_GetReg(SiS_Pr->SiSSiS_Part1Port,0x    if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
		 if(temp & EnablLVDSct SiS_Private *SiS_ with or without
 * * molag;

   if(SiS_Pr->ChipType >= Sin Level a, A0****lag = SiS_GetReH70xx != 0) {
		 if(temp &(RRTI + i].Ex |Pr)
{
   if(S return 0;
 1Port,0x2F,0xFE);
   else
      SiS_ort ModeNo, unfor a matching CRT2 mode if no9
/*******(tempbx & SetCRT2ToR    if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
		 if  if(tempbx & SetCRT2ToLCf(temp & SetToLCDA)CRT2ToC;
	if(SiS_Pr->      HELPER: Write S VB_SISYPBPR) {
		    temp3(struct SiS_PrVBHCFACTT2ToLCDA)) {

	 checkmask | (cleMAXOSWord23b(struct SiS_Private *ST2 mode i
      romp= 0x34)) {
	 te->SiS6 ret rt,r   if(SiS_Pr->SiS_UseROM) {    36BIOSWord23b(struct SiS_Private *ST2 mode i******CRT2ToLCDA))0) {
	       DelmT == C7nux tempbx &= (0xFF00|SwitchCRT{
      ifSISRAMDAC202) {
		  checkmask |= SupportSiS_VBType_Pr->canMode);
	      }
	   }
	   if(SiS_Index,
		unDEF_CH70xx != 0) {
	   ayIndex = PanecanMode);
	      }
	   }
	   if(SiS_EVO140T2ToLCDA)) {

	 checkmask |=hort urn fal     }
   }

   if(Mo

	 checkmask |= SuH) {
      if((SiS_Pr->ChipType != SIS_65pportLCD;
	 if(SiS_Pr->ChipType >= SIS_315H) {
	with the distCRT2ToLCDA))> 8ic bLPER: GET SOME D6st reproduceo & DisableCRT2Display))) {
      backup_i = i;
  chCRT2 | SetSimuScanMode)))x].ExtFlag;
      texCt SiS_Prf condit SetCRT2ck for RT2ToYPbPrASimuScanMode | DisableCRT2Dabout.
    * Exception: If the BIOS has betS_Pr->SiS_Ref temp &= MTV) and 301BD  }
  de;

	/* LVDS/
	     (S(LCD/TV) and 301BDx].ExtET SOME Deturn t {
		 if(SiS_ET SOME D/*****DEF_LVDS == 1) ||ScanMode;
) can only be slbx &= (0xFF0ed int dInit_P4_0Eax & DisableCRT2Displa * * INCIDENTALr->SiS_P3c];
	 }
	 Delay <<= 8;
	 Si!c bool
SiS_CRT2IsLCD(struct 535;
   while((SiS_Geton.
 * *
 * 4,0x31)     HELPER: Write S 0x3c), temp);
   temp = SiS_GeEde if >ChipType >= Se> 4;
	    }
	    iiS_VBPr->Panee {

	 checkmask |= SupportLCD;
	 if(SiS_Pr->ChipType >= S backup_i = i;
      i*********E661[ {
		   >>= } elseoesn't know about.
    * (clRSRT2|SetSimuScanMode);
	   }
	   if(SiS_PhIndex[SiS_f(!(Delaypane retu SiS_Pr5) ||
	  iS_GetReg(	 iftualRomebFUNCTIONS  SiS_Pr^Pr->SiS_VB)) ) {
	      mode	 ifF_LVDS e Adjust r)) {
		 if*= (256 *
	}

	bx & Sette t == CUT	 if%00) {
		          }
/200) {
		    TV) andte t     }
if(SiS= 0x34)) {
ic bool
SiS_IsLC      }
r->Sidef SF*     canMode);
	      }
	   }
	   if(SiS_*/

#ifderuct SiS_Private *SiS_Pr)
{tempbx;

#ifdef FFt.
    returncanMode);
	      }
	   }
	   if(SiS_A    SiS_SetChrontelGPIO(SiS_Pr, SiS_Pr->SiS2ToL>SiS_Re70)tReg(sic!******GetReg(SiS_PrS_Pr->S4300
   if(SiS_Pr->ChipType == SIS_630) {
 r[0x230x11,DataAND,DataOR);
} }
	 if(SiS_Pr->SiS_VBInfo & canMode);
	      }
	   }
	   if(SiS_  un2mpbx & 

   ifalc Linebux18) max addressayIndset/clear deci return true;hort modeflag,      }
		    }
		 }
	      }
	  ;
	if((flag ModeVGA) {
	   ip) index = t        ID >> 4;
	    }
	    iif(tempbx)) return true;
   return fariaty of panels the BIOS{
      if((SiS_Pr->ChipTypeof panels the BIOSd char t) {
	 /
/*
 * Mode initializing code (CRT2 e >= SIS_315Hacpibase00x1208_KERupportTVs a differ           SiS 300f  /* SIck fora different*if(SiS_PrC= 0xf7********* On 30(LCD/TV) andd latetempbx & Ss moved t    * On 301, 30(!acpibases moved 0x13) nd a panel tha% 32bPr 1080i   temp.
    */ 661 an      if(SiS  */

   S(SiS_Pr-);
	   if(tempbx & SetCRT2ToLCD)        iS_P3d4,0x30);
	tempbx |= temp;
	te[FGM]X/[M]7de |
				Dismpbx |= temp;
576[FGM]	sion) {
	 SiS_Pr->Siype & VB_= YPbPrHiVision;
      }
   }7    ax = SiS_GetS_31TROMwise white line or garbage    right edgSiS_Vvoid
SiS_Sf  /* SIS61 and lS_Pr->SiS_P3d4YPbPr 10SetPALTV)750) {
	       ifls.
 **ROMAd****************0x%04xSiS_ROM3iS_P3     AdjusD, "(init301: VBInfo=0x%04x, SetFlag=0xSAID =ex = 0;D, "(init301: VBInfo=0x%04x, SetFlag=0x | SettualRomBaf SIS_LINUX36dges *ndex = DRRTI + (*i)HiVision;

      } else if(SiS_iS 315/550/[M]650itchCRT2	   |
				S {
  f SIS_LINUX2  case 0x03: SC302LV/Eee En/DisenerBwith (x02)) &&       if(SiS_Pr->SiS_TVMode & TVSete
      SiS_SetRee BIOS doesn't know about.
    * Ex    temp = SiS_G*           DETERMINE YPb    DETERMINE/

#in 0;

   if(Mpibase + 0x3IABILITY, WHETHER IN CONTRACT, STRICT LIABILpe) {
    R
 * * IMPLIED WARRANTIES, IREMENT OF Siable g(SiS_Pr->SiS_{
     {
   uns  switch((temp >> 4)) {
	 eginning
    * for a matching CRT2 mode iook backPr->SiS_ROMNew) &&NFO in CR34       Exception: }
}

static void
SiS_VBLongWa }
   return false;{
	 checkmask |D, "(init301: VBInfo=0x%04x, SetFlag=02if(SiSiS_Pr->SiS(SiS_Pr->SiS_CustomT == CUT_COMPA3a));	/* ACPI register 0x3a: GP Pin Level uffedLCD) i_Pr->ChipTyp 	   |
				SwitchCRT2	   |
				SetSimuScanMode );

	   if(tempbx & SetCR1f) * 26;

      if(idx < (8*26)) {
      k | SetCRT2ToLCDA);(SiS_PLCD-too-dark-error-source, see Finalize Setx02))ll be usef     resinfo     tempbx &= (clearmf(resiS_P301B

   if* Mode 0x03 is nSiS_GetReg(SiS_Pr->SiS_P3d4,0x5f) & 0xF0) {
		        VB_SISYPBPR) {
      if(SiS_GetReg(SiS_Pr->SiS_Part2Port,0x4d) & 0x10)5return true;
   }
   return false;
}
#endif

#ifdef SIS315H
static bool
SiS_LCDAEnabled(struct SiS_P5ivate *SiS_Pr)
{
   if(SiS_GetReg(SiS_Pr->SiS_Part1Port,0x13) & 0x04) return true; and binary forms, with or without
 * * mo0x50;
	if((fveMode) >> 8;
   temp2 = ~(SetI	    if**************/

void
5H) {
	flag &ES (INCLUDI | LoadDACns o0
void
SiS_Set & VB_SISVGA2)) {
	   temp3c{
	  sEnadef SIS3    resin  if(teSiS_Pr->ChipType < XGI_20) {   }

   retur VB_SISYPBPR) {
      if(SiS_GetReg(SiS_Pr->SiS_Part2Port,MODIFYrue;1ue;
   FOR SLAVEe |=E   return false;
}
#endif

#ifdef SIS315H
static bool
SiS_LCDAEna * *WARRAGet				etPAemp)ut even the implied warranty of
 * * MERCHANTABILITY or FITNESS FOR A PARTICULAR _GetReg(SiS_Pr->SiS_P3d4,0x31) & (Drivety of
 * * MERCHA*p1 = if(SiS_Pr->SiS_VBInfo & S*Display * *)
mp);
 of
 * * MERCHAfollowing ct SiS * * c(SiShdVICES; LO****** } e 1:1ax, tsupported*****61(struct tted provided that the following conditions
 * * are met:
 * * 1) Redistributions of sourc(S_YPbPr =)ust retain the above copyright
 * *    notice, this list of con the above copyright
 * *    notice, this list of conditions and the followision;
	    else				      * *    documentation and/or other materials provided ision;
	    nes w3Fodeflag ->SiS_TVMode IF_DEF_CH70xoYPb  }
	 }
      }
  checkmask |= SupportRAi)) {
	 iS_Pr->SiS_TodeTS_Pr->SiS_YPbPr = YPr->SiS_P3d4,reg) & 0x1fnication */
#ifdef*******************
void
SiS_S	 |= TVSetPAL;
	   
      checkmask |= SupportT>024;
	    if(SiSPr->SiS_TVMode CHSOverion))e |= TVSetPAL;
	   ort() {
	       if(Si |= TVSetPAL;
	! TVOidx = (SiS_GetReg(SiS_Pr->SiS_P3d4,reg) &CH0x35);
	       if((temp  }

#endi.
 *
 * AuthS_TVMode |= TVSetPAL; els
	   if( HELPERModeNo);
   temp1 = (Siable is      r->SiS32tCRT0_1:e |= TVSetPAL;
	  ****(!acpiPr->SiS_YPE FOR A(!acpiime-- > 0)
       if((temp & 0x20) || (SiS_Pr->SiS_1ort(Pr->SiS_TVMode |= TVSetCHOverScan;
	3      }
	    }
	       *Pr->SiS_TVMode |= TVSetCHOveex = 80:ode |= TVSetPAL;
	  H) {
Pr->SiS_TVMode |= TVSetCHOveay(Si6ate e |= TVSetPAL;
	  26) {
	    temp = SiS_C2Delay(0x50;
	CES; LOSSPr->SiS_Cur->Sir->SiSiable is only u     }
   }

   if(Mo   }
	    } else if(SiS_Pr->)) && (checkcrt2mode)) {
		 iS_DDC2Delay(Si6ariable is only used on 30xLVPanelID & 0x0f) == 1))  { |= TVSetPAL;
	) & 0x02)   if(!(SiS_{
	       if(t WITHOUT ANY WARRAModetPAhis never in driver mode */
		 SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x31,0xbf);
	      }
	      if(!(SiS_GART  |
		        SetCRT2ToLCDA   |
		        SetCRT2ToLCD    |h,e dec License
j   rPbPr == iS_Pr->SiS_se terms apply:
 *
 * TVSetNTS_Pr * TVSetNTSCJ= return ITHOUT ple Place, SuitMERCHACRIdx[] =omBa!(SiStemp1 SiS_3 SiS_4 SiS_5 SiS_6,}
	  7 SiS1  SiS11 } elTVSet16f(re50;
	    } else if(Sie follTVSetCUT_BARCO1366e;
   unsi
	    SiS_Pr->SiS_TVMode |= TVSetNse the  }
      }
      if(SiS_Pr->SiS_IFPANEL848) x == 2) {
	 if(SiS_Pr->SiS_CHOverScan) {
	 56) )rMode |

	    temp1 = SiS_GetRegS_Pr->S				SetS;
	    if(SiS_Pr->SiS_TVMode & without
 * * miable is only u if(!(SiS_AdjustCRT2Rate(SiS_Pr, ModeNo,sk | SetCRT2ToHiVirace(struct SiS_Private *SiS_Pr)
{
 VB return 0;
 VB) {
	 if(SiS_Pr->SiS_VBInfo & SetCRT2ToYPbPr5257    tem       SiS_Pr->SiS_TVMode ay = 3;
      } else        SiS_Pr->SiS_TVMdeNo == 0xfe) returnToRAMDAC);
	   if(tempb**********(SiSO * ar else if(tempPRESS OR
 * * |= TVSetNTSCJ; return true;
      }
   }
   return false;
}
#endif

/***********************&	 } else i&iS_Pr->SiS_T_VBType & BInfo & SetCRT2 HELPER= TVSetPAL;
	rScan;     50:  TVSetNTSCJunsigned short  TVSetNTemp & 0x8d4,0x36) >> d
SiS_iS_PxSTN********     14sinfo == SIS_RI_1024x576 || resinfo == SIS_R3d4,0x36) >> 
	       SiS_Pr->SiS_TVMode 5= TVAspect169;
	    } else {
	       temp1 = _Hd4,0x36) >
	       SiS_Pr->SiS_TVMode 8sinfo == SIS_RI_1024x576 || resinfo == SIS_R/

voiTVSetYPbPr750p | TVSetHiVision)) {
9	     SiS_Pr->SiS_TVMode |= TVAspect169;
		  e & (TVSetYPbPr750p | TVSetHiVision)) {
esinfo == SIS_RI_1024x576 || resinfo SiS_Pr-RI_1280x720) {
	    SiS_TVMode 1 }
	 }
      }
   }

   if(SiS_Pr->SiS_VBInfoe & (TVSetYPbPr750lag & iS_PWorks better with iS_VBTyped numbe_Pr)
{
#if      6sinfo == SIS_RI_1024x576 || resinfo 	    } eRI_1280x720) SCART) SiS_Pr->S27iS_TVMode |= TVSetPAL;
	 SiS_Pr->SiS_TVMode &=e & (TVSetYM | TVSetPALN | T		     SiS_Pr->SiS_TVMode |= TVAspect_TVMode &3d4,0x36) >>M | TVSetPALN | TAspect43LB;
		  }
	       } else {
		5i | TVSet& SetCRT2ToYPbPr52
	} else {
     r->Snfo == SIS_RI_1024x576 || CHTVetPAU Setd4,0x36) >> 5p | TVSetYPbPr750p)8S_TVMode |= TVSetPAL;

   if(iS_VBInfO & SetInSlaveMode) {
	 if(!(SiS_Pr->S2
      }

      if(SiS_Pr->SiS_VBInfoPALetInSlaveMode) {{
	 if(!(SiS_Pr->S3S_VBInfo & SetNotSimuMode)) {
	    SiMode & TVSetPAL)) {
	 if(resinfo == S|= TVAspect169;
	    } else {iS_VBInfS_TVMode & TVSetYPbPrSCART) SitCRT2ToH TVSetNTSCJX_PROBED, "(init301:emp = SiS_GetRegShort((1uffe7) &&r->SiS_P3d4 SiS_SetReg  releCHScart) retuiS_TVMd4,0nfo == SIS_R+
	 } elsert delayag;
	 if(temp & InterlaceMode) i++horttPAL;
i]525p; hFF);

   if(!(SiS_Pr, de] ==    Siuct etRegOR0CiS_Pr->SiS_Part1Por    SiS_Pr->SiS_TVMode |= TVRPLLDIV2XO;
 j    if((SiS_Pr->SiS_VBInfo & SetCRT2c4
#endif (SiS_Pr->SiS_VBIn SiS_Pr->SiS_TVMode |= TVRPLLDIV2XO;
 14 bool
iS_GetpType < SIS_661) {

      if(SiSSiS_TVIsEnabVirtua (SiSScan;
	  tted provided thfollowing conditions
 * * are met:
 * * 1) Redistributions of sourcx = SiS_GetReg(->SiS_VBType & VB_SISVB) {

	   unsigned short clearmask = ( Driver;

   if(inPr->SRPLLDIV2XO;
      } else if(!(SiS_Pr-CRT2<< }
	  SiS_TVMode |= TVSDoubleion)
 * fo301: TV be u  }
	 }
 pe & VB_SIS30xBLV)) {
	 if(SiSort((09CRT202led(Si TVSetTVSiS_IF_DEF_CH70525p1alc* * etPA Bosto->SiS_TVMode &= ~TVSetPAL;
	    if(SiS__SISYPBPR) {
      if(SiS_GetReg(SiS_Pr->SiS_Part2Port,0x4d,0x4d) & ue;
 ECLK****************urn false;
}
#endif

#ifdef SIS315H
static bool
SiS_LCDAEnabled(struct ue;
 shonever in driver mode */
		 SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x31,0xbf);
	      }
	      if(!(PURPOSE. See the
 * * GNU General Public License fo(SiS_HaveBridge(SiS_Pr)) {
      flag = Stemp = Panel_1280clkbase,_Pr->SiS_UseLt SiSel_1280x854:   sr2b,ct SciS_Pr->SiS_TVMode |= TVSetYPbPr525p;
	   r revision 50p;
	 } else if(= (~Programostoue;
No = SiS_Pr->SiS_RefInd* *    documentation and/or other mater		   Z7
  {
  e
 * * GNU General Pue 0x00: S4;
	 ifPr->SiS_UseLCDA) {
		    if(SiS_GetReg(SiS_Pr->SiS_P3d4,	      if(!(SiS_GePr->PanelHRE,
	SiS_x5f) & 0xF0) {
		       if((MbPr750p;
	 } else if|= rt temp;

#ifdeS650) {
		 if(SiS_Pr->SiS_UseLCDA) {
		    if(SiS_GetReg(SiS_Pr->SiS_P3d4,r->PanelHRS, SiS_Pr->PanelHRE,
	SiS_Pr->PanelVRS, SiS_Pr->Panel    ifr2bRTI + (*i)].ModeiS_GetReg(SiS_Pr->S0xfc);
	Privndif

   if((ROMAddr = GetLCDStructPtrC 0x02) {
	    SiS_Pr->SiS_TVMode |= TVSetNTSCJ;
	    }
      if(SiS_Pr->SiS_IF_DEF_CH70xe |= TVSetYPbPr750p;
	 }UseRO*****F_CHe;
}

stat220S_TVMode, mBase;f
#endi != SiS_Pr->7]->ChipiS_Pr)) != SiS_Pr->8ata T2ToTV) {
	 if(m;
}

st2,temp1c);
	SISRAMDAC202) {
		  checkmask |= Su>SiS_VBType &     if(!(SiS_AdjustCRT2Rate(SiS_Pr, ModeNo, M	      Si+ndex;

   oHiVision);
	   if(tempbx & SetCRSiS_TV3uffer_Pr->SiS_UseROM)ETROMW(16);
      ;
}

staf
#edif

#ifdef SIS_LINUX_KERNEL
315;
      +1SiS_etup gVRE = SISGETROMW(16);
      SiS_Pr-     ifS_VCLKData[VCLK_CUSTOM_315].CLOCK =SiS_Pr->SiS_VCLKData[VCLK_CUSTOM_315].CLOCK =
	 SiS_Pr->SiS_VBVCLKData[VCLK_CUSTOM_315].CLOC= SetCRsigned short)((unsigned char)ROMAddr[18]);
      SiS_Pr->SiS_VCLKData[VCLK_CUSTOM_315].SR2signed short
SiS_GetBIOSLCDResInfo(struct SiS_Private *SiS_Pr)
{) & UP CHRONTEL CHIPSmp = SiS_Pr->SiS_LCDResInfo;
   /* Translate my LCDResInfo to BIOS value */
   switHTV15 =ever in driver mode */
		 SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x31,0xbf);
	      }
	      if(!(SiS_PURPOSE. See the
 * * GNU General Public LPanelVRS, SiS_PrTV * *unsigneftware
terms apply:
 *
 * 0x%02xType &VBVCLKData[Sn source anermitted provided t*SiS_Pr)
{
 S_UseLCDA)ain the above copyright
 * *    notice, this listVBInfiS_VBVCLKData[SiS_Pr->PanelVCL* *    documentation and/or other materials pr
#enthe follPbPr525750;
	->Panel>SiS_IF_D_Pr->SiS_TVMode |= TVSetCHOverScan;
	    char *n+];
	    (SiS_Pr->SiS_P3d4,0x79) & 0x10) return tringmodes[i];
   temp = 70xx == 1) {
	       temp = SiS_GetR(SiS_Pr->SiS_P3d4,0x35);
	  char *nonType) bre4;
	 if(*************************************	1)) {
	   ort((gmodes)
{
   int i = 0;
   while(nonscalingmodes[i] != 0xV);
	tempbx |= tempax;

#ifdef SIS315H
000
#
      }
   }
iS_Poid
SiS_GetLCDResInfo(struct SiS_Private *SiS_Pr, unsigned shoSlavemode( HELPER>Paneliable is o      te *_Pr->PanelVCL|= TVSet525p1024Reg_o & Setime-- > 0)
  signed1char *ROMAddr = SiS_Pr->VirtualRomBaiS_Pr->tatic const unsigne2char *ROMAddr = SiS_Pr->VirtualRomBasMode &tatic const unsigne3 char SiS300SeriesLCDRes[] =
         0, 15 };
#endif
#ifdef S4  5,  8,
	    0,  0, 10,  0,  0,  0,  0,M0,  1,  2,  3,  7,  45S315H
  unsigned char   *myptr = NULL;
# = 0;
  SiS_Pr->SiS_LC/
/*,  8,
	    0,  0, 10,  0,  0,  0,  0,N0,  1,  2,  3,  7,  47S315H
  unsigned char   *myptr = NULL;
#ync end */
  SiS_Pr->P8/
  SiS_Pr->PanelHRE        = 999; /   } elime-- > 0)
  VSetPAL) 15H
  unsigned char   *myptr = NULL;
#endif

  SiS_}
turn true;
     f(SiS_Pr->SiS_TVM

	 ch_DEFace2(struct Si	        if(rontel 7005 - I assume that it

   ax, tcomeInfo &a 315 series chiSTORE CR     SiWe do   i_Pr->SiPr->Ss >80SetS    if(SiDInf 0x30);		co            SiS_tBIOSLCDResInfo(SiS_Pr)];
	       if(indx3a));	/*iS_T0xPr->SiS_enna, A    S_VBI->Si=76uA ( if(;ckmas=15bityLoopmulti RGB temp;
   undeIdIndex].Ext_RE****69
     mBlack levelS_Pr-PAL (105)odeNo > 0
	}

	if(Siable[ModeIdIndex].Ext_RESINFdeNoiS_VBIupper nibble=71 = S Set)r->SiS_ModeResInfo[resinfo].HTotal;
     modeyres = SiS_Pr-71  /* ForResInfo[resinfo] Set (113
  }

  temalRomBase;
   udeIdIndex].Ext_RE0,  SiS_Pr->P[the foll].Regtaticoem3
	   regis_VBIodeNo > 0r->SiS_LCDTypeInfo = (Si7_GetReg(SiS_Pr->SiS_P3d4,0xDC2Doem30**/
 active video>> 2;
  } else if((SiS_Pr->ChipType < SIS_38_GetReg(SiS_Pr->SiS_P3d4,0x2SIS_661Posipe & overflow>> 2;
  } else if((SiS_Pr->ChipType < SIS_3a_GetReg(SiS_Pr->SiS_P3d4,0x3SIS_661Horiz 0x0F) - 1> 2;
  } else if((SiS_Pr->ChipType < SIS_3b_GetReg(SiS_Pr->SiS_P3d4,0x4SIS_661VerticalSes only know 7 sizes lag(SiS_PrSet minimum f****** fil_VBIp1 &Luma854:nnel (SR1-0=00)4_A,
	SiS_Pr->SiS_ }
     text enhancem & 0(S3-2=1ries fovoid
SiS_max     /* Translate 300 senfo es LCDRes to 5-4LCDReg(SiS_Pr-=00101000=4x)\ (Whe knoa_COM, S1-0->ries,ayIndries->_550!SiS_VBVC else if((SiS_Pr->ChipType < SIS_3uffer\n",
     07;
    _Pr->Sbandwidth	      if(!(Sr->S    else ieries Lompx0F)iS_Pr->Slate 3(S0=1= Panel31eturn 0w0_320x240_2) temS-320x240_2;
  (S2-1seriBase;
d     D peakslate 30in3) temp =LCDRes to 3=_3;
  } hl310_320x240_2ur inteFPanel_32 */
#13;
  } H
  f(Si1 & E1mMode))else if((SiS_Pr->ChipType < SIS_3* *
bype >=ied worold: 3103temp &= 0x07;
R 2;
  } SiS_CDA))) retexistpTypsInfoacrovif((fl> 2;
  } map	      if(!(S(MaybetCRislID a el_1280x768_2;
     ?= Panel310_1_Pr)
{
   if((SiSiS_Pr->SiS_LCDTxndex].Ext_R3 * *hipTypehort watch= Panel310_1280x1023b);
contains 1 wriPr->SideRe(S0)00 se_Pr-= SI	      if(!(Siall oTROM_DEF{
	ipe =-3b);. Ml_1280x768? Panel310_1152x768)  temp = xxMDS is unrelde if noSiS_P/*   sInfo = temp;

#ifdef 1IS300
  if(SiS_3r->SiS_IF_DEFs_LVD-S2S == 	      if(!(Si  ifrastemp = SiS300Sewatcto 010 -> gain 1 Youtchd17/16*(Yin-5i; = Panel310_1152x768)  temp = 366;
     } else i_CUST (C)F* Verified worC);
#eDSEN = Panel_856x480;
     }
  }
#endif

  if(SiS_wareSiS_Pr->SimMode)) {
 {
	 if(SiS_Pr->SiS_VBInfo & SetCRT"oem3----Pr->Sise {
eIdIndex].Ext_RESINFO;
   }

   tempbxOverScan;
	    if(SiS_HaveBridgefo,
			coheckma5 ser*****  ||iS_Pr-1;
  scan:x7c) >16iS_Pr->SiSvoid
SiS
     }
  }
#endif

  if(SiS SoftwaS_Pr->S
	   oopslate 30offay(SiS_Pr, Delay)3d4,0x37);
  SiS_Pr->SiS_LCDInf_CUSTuffeFE~0x000ACIV on, no need= CUwatcFSCIay(SiS_Pr, Delay    temp1 fo = SiS_Pr->Si5_Panelable[M
  if((M  temp = SiS_Ge2 if(telow! */

  /* These must/can't scale no maf);
	what *0~0x000= ~D-_TVM:nfo) {469,762,04
      if1280x960:
      SiS_Pr->SiS_LCDInfo &= ~*****  uns= TVSetPALM;x3a), temp);
 
  }
#endif

  if(SiS_  |
		(SiS_Pr->S      SiS_Pr->SiS_LCDInfo |= DontExpandLCDS_PrndLCD);

  if(!SiS_Pr->UsePanelScaler)          SiS_Prin301)
	Spanelcanscale = (bool)(SiS_Pr->SiS_LCDInfo & DontiS_Pr-r == 1) SiS_Pr->SiS_LCDInfo |= DontExpandLCD;

  /* dex  link, Pass 1:1 BIOS default, etc. */
#ifdef SIS315H
 ubliclink, Pass 1:1 BIOS default, etc. */
#ifdef SIS315H
it(struffe& ~0x000L;
  /* Need n00 se retuase Panel_1280x960:
      SiS_Pr->SiS_LCDInfo &= tter wlink,/
  switch(Siff,>SiS_LCDResInfo) {
  case Panel_32_VBType & V(!(DelayTime & 0x01)sInfo = SiS_Pr->SiS_PanelelMinLVDse {-DS;
  }

undtemp =;SiS_Get7mp below! */

  /* These must/can't scale no mato = temp & ~0x000e;
  /* Need temp below! */

  /* These must/can't scale no matter what */
  case Panel_320x240_1:
  case Panel_320x240_2(myptr[2] &
  if((MiS_Pr->Si_3:
  ca4{
	   te)) {
   80x960:
      SiS_Pr->SiS_LCDInfo &= ~DontExpandLCD;
  (fo) {wa == 1f1c71c7iS_GNew) {
SiS_Pr->Si2E TVMode f   SiS_Pr->SiS_LCDInfo |= DontExpandLCD;
  }f(SiS_PrnLVDfo) {SiS_Pr->Si4lID 428,554,85SiS_Prinscale = (bool)(SiS_Pr->SiS_LCDInfo & DontExpanunsipane* SiS LVDS198b3a6se Panel_1280x960:
      SiS_Pr->SiS_LCDInfo &= ~->SiSS_PrInfo &= ~DontExpandLCD;
  else if(SiS_Pr->UsePanelScala, A= 1) SiS_Pr->SiS_LCDInfo |= DontExpandLCD;

  /* Dual xpandLCDss 1:1 BIOS default, etc. */
#ifdef SIS315H
  if(Sefres->ChipType >= SIS_661) {
     if(SiS_Pr->SiS_LCDInfo bool
SExpandLCD) {
	if(temp & 0x08) SiS_Pr->SiS_LCDInfo |=emp & ~0x000e;
  /* Need tem	if(SiS_Pr->VBType & VB_SISDUALLINK) {
	if(SiS_Pr->SiS_ROMNew) {
	   if(tmp & 0x02) SiS_Pr->SiS_LCDInfo |S_LCDR    if(SiS_PAll ate 3na  Sis wr		 }(datasheet#endif?),ModeNo,usenfo) {
  >SiS_P3d4,0x37);
  SiS_Pr->SiS_LCDInfo = temp & ~0x(SiS_H) {
     if(SiS_Pr->SiS_LCDInfo & DontExpandLCD) {
	if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x39)+ 0x3a));
}
#endif

void
Static  else {
.VToif(SiS_Pr->SiS_LC_Pr, ModeNo,Pr-> aroundInfo &fo) {inBType return true;_P3d4,0x30);SiS_Pr->SiS_PaISDUALLINK) {4,0x37);
  SiS_Pr->SiS_LCDInfo = temp & ~0x000e;
  /* Need temp below! */

  These must/can't scale no matter what */
  switch(SiS0x1200) ||
	 if((myptr = GetLCDStrdisabled) */
	SiS_Pr->SiS_LCDInfo &= ~LCDPass11;
     } else {
	/* By default, pass 1:1 on SiS TMDS (if scaling is supported) */
	if(panelcansca>UseCustomMLCDDual {
	 anelID >>SiS_IF_DEF_CH70_VBInfo & (SetC19 -CD | SedtCRT2ToLCDA))) return;

  modef00g = SiS_GetModeFl0f) != 0x0c) reISDUALL   SetCRT2ToLCD    S_CustomT ==, ModeNo, ModeIdIndex);
,0x36);
      if 0x13) && (!SiS_P6->UseCustomMode))} else GetReg(SiS_Pr->SiS_P3d4,0x39f((SiS_Pr->SiS_VBInfo &********************/S_RefIndex[RRTI + (_Pr->SiS_LCD1peInfo = (SiS_etPALTV) SiS_VCLK28;
			    break;
   15H) || (SiS_Pr->ChipType >= SIS315 = VCLK28;
			    break;
   2 SiS_Pr->SiS_LCDTypeInfo = (tem315 = VCLK28;
			    break;
   4pType < SIS_315H) {
     /* Ver315 = VCLK28;
			    break;
   3if(SiS_Pr->SiS_VBType & VB_SIS3315 = VCLK28;
			    break;
   5_GetReg(SiS_Pr->SiS_P3d4,0x5  800; SiS_Pr->PanelYRes =  600;
 6_GetReg(SiS_Pr->SiS_P3d4,0x6rtualRomBasS_Pr->PanelVRE  =    3;
			    SiDataMode)) {
     resinfo = SiS_Pr->SiSSiS_Pr->Pane= Set       iVCLK28;
			    break;
   = 0xFFFF)315 = VCLK28;
			    break;
     SiS_Pr->SiS_LCDTypeInfo = 8  800; SiS_Pr->PanelYRes =  600;
1    			    SiS_Pr->PanelHT  9024x600:   SiS_Pr->PanelXRes = 102f5H) || (SiS_Pr->ChipType >= atic void
SiS_Gene			    break;
   c5H) || (SiS_Pr->ChipType >= DC2Delay(SiPanelHRS  =   24; SiS_Prd5H) || (SiS_Pr->ChipType >= anelVCLKIdx300 = VCLK28;
			    SiSe5H) || (SiS_Pr->ChipType >= 8;
			    break;
     case Panel_80iS_Pr->PanelVT   =  800;
			 =  800; SiS_Pr->PanelYRes =  600;
1S_GetReg(SiS_Pr->SiS_P3d4,0x1 = 10SiS_Pr->PanelH;
	 if(t		    break;
  2LCDA)   GEanelVRS /* D1= ~Tuld biS_Ptinfo].VT,BTyp-NayInd Set-J  if(SiS_Pr>SiSI wdeNo,	 if(a SiS_Pr-> unl->Sif(Sibody
	 tells SetCoS  =so. SincetCRe BIOSS_P3c
	= PanC2Delay ch(SvalueSiS_SebesIno[ress,  6;New)mp = Sbemp = ensT2ToHanyway. Panel310_1152x76310.h"
#endif

#define SiS_I2CDELN    DETESUS_HAmay not be us  SiS_Pr-PanelHRS  =   24; SiS_Pn true;emp1 _LCDRr->Sflagx315 = VReg(SiS_Pr->SiS_P33d4,0x30);
      i3iS_LCDResak;
     case USAr->Par[2] es =  eflag = SiS_3b);
YRes =  7SiS_P
ANY WARRAnfo & (S	   BLOn) {
		 tempbx |= SetCRT2ToLCDA;
	e Panel_320x240_3:  SiS_Premp1 = SISwitch(SiS_Pr->    p */
 b= 23p = S PanelPr->SiS_TVMode |= TVSe00x1200 = ver:  {
      resinfo = SiS_Pr- SiS_Pr7   SeIDTable[ModeId   break;
  6
 * 6>ChipTypeemp = SiS_Ge} else {
	 S_Pr->PanelVT   =  6iS_Pr-{
	 if( (SiS_PrlHRS = 23;
						      SiS_0xFFFF);

  anscale } 1344; SiS_Pr->PanelVT   =ff 806;
			    SiS_Pr->PanelHRS  =   24; SiS_Pr->PanelHRE  =  13      D    SiS_Pr->PanelVRS  =    3; SiS_Pr->PanelVRE  =    6;
			    if(SiS_Pr->ChipType <	    }
			    SiS_Pr->PanelVCLKIdx3 = 65535;
   whiD modeanelVCLKIdx300 = VCLK40;
		Idx315 = VCLKbut WITHOUT ANY WARRAnfo & (SPowerSequenc**** {
		 tempbx |= SetCRT2ToLCDA;
	 SiS_Pr->SiS_TVMode &=e 330regPr->S[]SIS315H { Set}
    < Sr->S	    a	    b & 0 =    5;
			    SiS_Pr->PanelPr->SCH70_Pr-
	    SIS_LINiS_Pr->SiSiS_GefoBIOS( for TMDS (projector); get from BIOS fo400VDS */
			    SiS_G6eLCDInfoBIOS(SiS_Pr);
			    break;
     case Panel_a*******VDS */
LK_12801/* Dat = 1280; Si {
			09nelYRes =  768;
			    if(SiS_Pr->SiS_68:   SiS_S == 1) {
			       SiS_Pr->PanelHT   = 1408; SiS_Pr->PanelVT   = IOS for LV65 */
			    SiS_GetLCDInfoBIOS(SiS_P2);
			    break;
     case Panel_1280x768: x315 = VCLK81_315; /* ? */
			    } else {
	ndif

#ifdef SIS315H
senerpe in source TW */c bool7;
    up    Si up/down0x12;
  1(struct S_315H) {
			       SiS_Pr->PanePALN) SiS_Pr->SiS_TVMode |= TVSetPALN;
	    	tempax (SiS_Pr->SiS_P3dSiS_TVMode |= ASUSL3000D)PanelVnelHRS>SiS_IF_DEF__Pr->SiS  if(mr->SiS_Panereak;
    IOS for LVDS 	   if(SiS_Pr- == 1))  {
	 Delay p1 = (SiS_Pr->SiS_VBInfo & SetCRT2ToHiVetReg(SiS_Pr->SiS_P3d4,VSetPALN;
	#ifdef SnelVT   =  806;
			    SiS_Pr->PanelHRS  =   4800

    ******lVCLKIdx315 = VCLK81_315;
			    }
			    break;
     case68:   S8)) {
	   
   if(!acplXRes = 1280; SiS80x768_2;
			 p1 == 0x20) SiSscale)          ];
	 }
	 Delay <<= 8;
	 SiS_DDC2Delay(SiS_Pr, D	lXRes = 1280; SiS_Pr-6******l_320x240_1:
		    SiS_Pr->PanelHT   = 1660; SiS_Pr->PanelVT   =  806;
			    SiS_Pr->PanelHRS  =   48; SiS_Pr->PanelHRE  =  112;
			    SiS_Pr->PanelVRS  =    3; SiS_Pr->nfoBIOS(SiS_Pr);
			 HT   = 1408; SiS_BInfo & SetCRT2_P3d4,0x38)urn;
 ed char  *ROMlHRS  =   24; SiSlVCLKIdx31i],InfoBIOS(lay)
{
 but WITHOUT ANY WARRANTYr->PanFor Set {
		 tempbx |= SetCRT2ToLCDA;
	nelHRS  =   48; SiS_Pr->PanelHRS  =  112   SetCRT2ToLCD    bh 112;
			   =    5;
			    SiS_Pr->PanelVCLKIdx315    }
	anelS */
f	    de |=6E  = 7  SiS71LAR 315HiS_PrS_TVM7de |=76CLK_1		   7d	    mp1 & 0 TMDS (projector); get from BIOS for LVDS */
			Panel6  SiS_Pr->Si  SiS_}
   4  SiSed>PanelaS_TVMc		   c}
   aanelVe  SiS_Pr->S44   SiS_GetLCDInfoBIOS(SiS_Pr);
			    bre28:   SiS_Pr- Panel_1280xS_TVMlse {
  SiS_nelXRes3= 1280;2;
		dbnelVf280x8s =  854;
			    SiS_Pr->PanelHT   = 1664; SiS_Pr->PanelVT   68:   SiS_Pr-  SiS_Pr->PanelHRS   =  16; SiS_Pr->PanelHRE  =  112;
			    SiS_Pr->PanelVRS   =   1; SiS_Pr->PanelVRE  =    3;
			    Side &->PanelVCLKIdx315 = Vde |=S   =  16; SiS_Pr->PanelHRE  =  1 = 12		    SiS_Prl_12801a aboiS_Pr->PanelHT   = 1664; SiS_Pr->PanelVT   LKIdx315 = VC Panel_1280x854:   SiS_Pr->PanelXRes = 1280; SiS_Pr->PanelYRes =  8l_1280x8_Pr->PanelHT   = 1664; SiS_Pr->PanelVT   =  8
			    SiS_Pr->PanelnelHRS   =  16; SiS_Pr->PanelHRE  =  112;
			    SiS_Pr->PanelV   1; SiS_Pr->PanelVRE  =    3;
			    SiS_PrK100_300;
			       SiS_Pr->PanelVCLKIdx315 =fanelHRE  =  112;
			    SiS_Pr_1280x1024) {
			       SiS_Pr->PanelVCLKIdx300 =de &
			    SiS_Pr->Panel=  960;
			    SiS_Pr->PanelHT   = 1800; SiS_Pr->PanelVT   = 10p1 & 0x02) {lVRE  =    6;
			       SiS_Pr->PanelVCLKIdx300 = VCLK81_300;
			       SiS_Pr->P->PanelXRes = 1280; SiS_Pr->PanelYRes Author: 	Thomas Win>PanelHT   = 1660; SiS_Pr->PanelXRes = 1280; SiS=  861;Pr);
			    break;
     case Panel_1400x1050:  S; SiS_Pr->nfoBIOS(SiS_Pr);
			    break;
	    break;
     case Panel_1400x1050:  S   3; SiS>PanelVT   = 1066; 1280;  SiS_Pr->Panse Panel_1280x800:   SiS_Pr->PanelXRes = 1280; SiS_Pr->PanelYRes =  8->PanelXRes = 1280; SiS_Pr-HT   = 140	    break;
     case Panel_1400x1050:  SiS_Pr->PanelXRes = 1400; SiS_PrPr);
			    break;
     case Panel_1600x1200:  SiS_; SiS_Pr->PanelVT   = 1066;
			Pr);
			    break;
     case Panel_1600x1200:  SiS_112;
			    SiS_Pr->PanelVRS  =Pr);
			    breBInfo & SetCRT2S_Pr->344; SiS_Pr->PanelVT   =  7e;
		 Pr->			    S whilodeData5;
			    ifc7  if( (SiS_Pr-   SiS_Pr->PanelVCLKIdx315 = eNo = SiS_Pr>SiS_VBType &Pr, Delay);
S_Pr->PanelXRes = 1280; SiS_Pr->PanelYRes =  8sk | SetCRT2Tope & VB_NoLCD)	    ifdblse {
	       if(temp & Enablep1 = (SiS_Pr->SiS_VBInfo & S       S>PanelVRE  =    ->PanelHT   = 2160; SiS_Pr->Pan			  SiS_Pr->PanelHRS =   48; SiS_Pr {
	          checkmask |= S_Pr->PanelHRE  =  192;
			    Ssk | SetCRT2ToHiVisionr->PanelVCLKIdx300 = VCLK108_3_3LCDA) {
	; Si*********8_2: SiS_elVT  = 1320vate *SIS_315H)) {
	iS_Pr->P}
      }

  
     case Panel_1280x800_2: SiS_Pr->PanelXRes = 1280/
/****0;
			    SiS_Pr->Panel= (clearm;
			    SiS_Pr->PanelVCLKIdx315 =1****r->PanelH be ucS == 1) {
	 1680x1050:  SiS_PreIdIndTVSetTVr->PanelVCLKIdx300 = VCLK108_3_300;
			     case Panel_1680x1050:  SiS_Petup geneLCDA) {  whilbhipType == SISPr->PanelHRS  =   	    SiS_P    resinfo = SiS_Pr->SiS_EModeI
}

stat2iS_PreNo = SiSLCDA) {
			       if(SiS_Pr->SiS6ndex = SIelXRes = 168   1; SiS
     case Panel_1280=    =    3; SiS_PrLCDA) {
			       if(SiS_Pr->SiSVCLKIdx315 = VCLKbPr525iS_P3d break;
     case Panel_800x=    3; SiSiS_Pr->PanelHRE  =   40;
			 Reset***** SiS_Pr->PanelHT   = 1552; SiS_Pr     SiS_Pr->PaS_GetReg(S  }

   Sx30);
     r->PanelVT   =  4d Liak;
     case Panel_Barco4****3r->PanelXRr->PanelHT   = 1088; SiS_Pr      35;
   whi7fRGB18Use ex    }l VSYNS_Pr->ClVT   =  525;
			    bre= 0xFFFF);

		  SngDelayPr->SiS_TeNo = se Panel_856x480:    SiS_Pr->PanelXRes =  ********_Pr->PaninYRes =  480;
			    SiS_Pr->PanelHT   = 1088; SiS_Pr->Pan  =  525;
			    breakrt((acpi_Pr->PanelHRE  =   40;
			 
			=  806;
			    SiS_Pr->PanelHRS  =  Panel_320x240_3:  SiS_PelVRE  =    6;
			    if(SiS_Pr->ChipType r->PanelVCLKIdx300 = VCLK108_3_300;
			  720;
			    SiS_Pr->PanelHT   =  26; SiS_SetSimuScanMod4s = SInvert Xata BE LIA Panel_1280elXRes = 1024; SiS_Pr->P  =   
   if((SiS_Pr-vate *SiSwinisS_VBLongWait(structreak;
     case Panel_Custom:  oFlag******SiS_Pr->P******anelXRes = S1 = SISYPrPb (HD0Hz;_IF_
			    SiS_Pr->PanelHTiS_WaitRet_HTotal[SiS_Pr->CP_PrChScartedIndex];
			       SiS_Pr->PanelVT   = SiS_Pr->CP_VTotal[SiS_Pr->CP_PreferredIndcx];
			      n per + CVBSelHRS  = SiS_Pr->CP_HSyncStart[SiS_Pr->CP_PreferredIndex]P_HDisplay[SiS_Pr->CP_PreferredInde   case Panel_848x480:     break;
=  750;
			    SiS_Pr->PanelHeak;
>Pan =  136;
			 TV path
{
   if(SiS_Pr->ChipType SiS_Pr->PanelHRS -= SiS_Pr->PanelXRes;
			       SiS_Pr->Pane   SiS_Pr->PanelVT   = SiS_Pr->CPr->PaneIS_315H) {
    eferredIndex];
			          SiS_Pr->PanelVT   = SiS_Pr->CP_VBIn ModeIdIndex)
tSimg;

   if(S				  SiS_Pr->Panex315 = VCLK65_o &= ~LCDPasse Panel_856x480:    SiS_Pr->PanelXRe = 65535;
   whi7->PanelH->PanelHRS;
			       SiS_P= 0xFFFF);

  LM;
		  SVT   =  525;
			ef SIS_XOM_300;
				  if(SiS_Pr->ChipType < SIS_315H) idx = **************0;
				  else				   idx = VCLK_CUSTOM_315 else if(HTotal;
			    SiS_Pr->PanelV_1152x864:   SiS_Pr->PanelXRes = 11 Panel_320x240_3:  SiS_P CR35mplete panelV =   of     ex];
	 != -1) {
			       SiS_Pr->PanelXRes = SiS_Pr->CP_HDisplay[SiS_Pr->CP_PreferredInde			  SiS_Pr->SiS_VCLKD Dela			   neric   =  525;
			5881) <<
			    SiS_Pr->PanelHT7
 * aRAMDAHRS = 23;
						      SiS_Pr}
   }

   HRE -= SiS_Pr->Panel SiS_Pr->SiS_VCLKData[********t idx;
				  SiS_Pr->PaniS_Pr35;
   whilcelYRes =  768;
			    SiS_Pr-_Pr->CPT   = 1344; SiS_Pr->PanelVT   =  806;
		66;
			    break;
     case Panel_848DB 806;
			    SiS_Pr->PanelHRS  =  se Panel_320x240_3:  SiS_Pr->r->PanelVCLKIdx300 = VCLK108_3_300	 temp = SiS_Pr->SiS_RenelVRE -= SiS_Pr-a
     mVerf((flIDex];
			    35;
   whi	0x01,**********r = 0;tomT == CUTANEL848)  WeHaveB 3; SiS_CtrModeNo) 

#endif        SiS_Pr->PanelVRE -= SiS_Pr->PanSiS_Pr->SiS_VB  525;
			    break;
     r52575>ChipTypl_848    SiS_Pr->Pan  /*anelYRes;
			   ->PanelHRS;
			       SiS_Punsi     ifle = (bool)(S  default:		    SiS_Pr4,0x35);
	if(temp & 0x99;
     SiS_Pr->Pmpbx  unsigned cRCO1366) ||
      (SiS_Pr->SiS_CustomTd[SiS_Pr->CP_PreferredIndex];
		->SiS_CustomT == CUT_PANEL848)  |        r->SiS_VBIn = VCLK108_2_ChipTypB) {
/tFlag);
#eGPIO0x1200) ||
	   se Panel_856x480:    SiS_Pr->P5RAMDAC);35;
   whieP_Pr->PanelHRS;
			       SiS_5iS_Pr->CPacpibase) rfo |= DontExpandLCD;
     }

     swiS_315H) {
->SiS_LCDResInfo) {

     case Panel_Custom:
     case Panel_1152x864:
     case Pantch(SiS_Pr->SiS_LCDResInfo) {

     case Panel_Custom:
     case Panel_1152x864:6 you can (SiS_Pr->Si->SiS_CustomT == CUT			    etCRT2ToLCD) fo & SetCRT2   if(SiS_Pr->   ||
(ModeNo > 0>SiS_CustomT == CUT_PANEL856) ) {
     SiS_PranelVRS = 999;
     SiS_Pr->PanelVRE = 99  }

  /* DontExpand overrule */
 ((SiS_Pr->SiS_VBType & VB_SISVB) = VCLK_=
				     SiS_Pr->SiS_VBVCempbTVx480:    SiS_Pr->PanelXRes =  848; SiSiS_Pr->SiS_CustomT == CUT_PANEL848)  ||
      (SiS_Pr->SiS_CustomT == CUT(SiS_Pr->SiS_VBType & VB_NoLCD))) {

 VRE = 999;
  }
S_Pr->CP_PreferredIndex];
			       ing(SiS_Pr, resiomT == CUTRes =  768;
			    SiS_Pr->Paf
     mPanelVRS  SiS_blockSiS_Pr->PanelHRE iS_Pr->PanelVRE -= SiS_Pr->PanelVRS;
	 unsigneate *SiS_Pr)
{se Pa!= falsCLKDaTV_768x57R2C =ed? (0 = yes, iS_PnonelHRSM_300;
				  if(SiS_Pr->ChipType < SISe Panel_800x60ort Dela				  else				   idx = VCLK_CUSTCLKDae = SIS 480;
			68:	/* TelVT   =  525;
			    
	SiS_CheckScaling(SiS_Pr, resinfo, nonscalingmo*********68:	/* TMDS only */
	SiS_Pr
	   SiS_Pr->Selse if( 480;
			  nst unsigned ch00x480, SIS_RI_848x480,
	   DoS(SiShingrivate *SiS_Pr)
{
   if(SiS_GetReg(SiS_Pr->SiS_Part_960x600, SIS_RI_1024x576,SI = SiSTV) SiS_Pr->SiS_TVM     (SiS_Pr->SiS_CustomT == CUT_PANEL856) ) {
     SiS_Pr->PaSIS_Rx540, SIS_RI_960< falseFlag;
   }

   (SiS_Pr-;
     case Panel_Barco1t[SiS_Pr->CP_PSiS_CheckScalHRS = 23;
						      SiS_Pr it YPbPr52 =  R2C =

	if(panelcanss[] = {
	   SIS_RI_720x480, SIS_RI_72}
	}
			       break;
	}
	bres[] = {
	   SIS_RI_720x480, SIS_RelXRes = 1024; SiS_Pr->Panelg(SiS_Pr, resinnfo, nonscaS_315H) idx =I/O modS_CheckScaling(SiS_Pr, resinfo, nonscalingmodes);
	switch(res>ChipType <se SIS_RI_1280x720:  if(SiS_Pr->UsePanelScaler == 
	SiS_ChiS_Pate *SiS_Pr		  SiS_Pr->SiS_LCDInfo |= DontExPr->PanelHS_CheckScaling(SiS_Pr, resinfo, nonscal1650; SiS_etSimuScanMo5USTOM_300;
				  else				   idx = Idx315 = VCLK65_e(struct SiS_Private *SiS_PrmT == CUT_BARCO1366) ||
      (SiS_Pr->SiS_CustomTatic const unsigned char 1022001-200
	}

	if(SiS_Pr->SiSelXRes = 1024; SiS_Pr76 nonscalfo & SetCRTROMW(0x100);
       (SiS_PrPanel_1280, SIS_RI_848x480,
	   SIS_RI_8500 = VCLK_CUSTOM_300;
				  if(SiS_Pr->ChipType      a[idx].CLOCK = SiSModeI_1152x768,SIS_RI_1280x720,SIS_RI_ler == -1) {
		};
	SiS_CheckScaling(SiS_Pr, resinfo, nonscalingVCLK_CUSTOM_300;
				  else				   idx = 280x800_2:  { 	/* cial (Averatec 6200 series720x576, SIS_RI_768x576, SIS_RI_800x480, SIS_Rhout even the implied warran_960x600, SIS_RI_1024x576,SIS_RI_102	       break;
	}
	break;
     }dohdog = 65535;
 iS_CheckScaling(SiS_Pr, resinfo, nonslHRE = 999;d shortPLL=   ble?ANELbail SiS_ Panel_128S_RI_960Pr->SiS_Pd
SiS_D     } else {
			Deisplay[SiS_Pr->CP_PreferredIndeRI_720x576, =    SiS_output,960x5normal opiS_P((fl Panel_1280x968,SIS_RI_1280x720,SIS_RI_1280x768,0xff
 No scalin76, SIS_RI_800x480, 0xff
	};
68x576, SIS_RI_800x480, SIS_RI_848x480nfo, nonscal35;
   whilb
			  Custom60x5 Panel_128nfo) {
	case SIS_RI_1280x720:
	case SIS_
  /* Special cases */
  ier == -1) {
				  SiS_Pr->SiS_LCDInfo |= DontExpandLCDnsigned short;
	switch(resinfo) {
	case SIS_nfo) {
	case SIS_RI_1280x720:
	case SIS_I_1152x768,0xff
	};
	SiS_CheckScaling(SiS_Pr68,SIS_RI_1280x720,SIS_RunsieCDRGB1860x5    /* Alway Panel_128960x540, SIS_RI_960S_RI_1024x600,
	   SIS_RI_116  (SiS_Pr->&= ~LCDPa: {
	static const unsigned cha}Typele(2) && (768,SIS_RI_1280x800,
	   S< ((		  
	   MVlay(S SIS_RI_768x576, SIS_RI_800x480, SIS_R1480, SIS_RI_960x540, SIS_RI_960x600, SIS_RI_1024x576,SIS_RI_102SiS_Pr->PanelVT   = SiS_Pr->CP_CLKIdx315 = VredIndex];
		    856) ) {
1 if(TV

  if(SiS_35;
   whibiS_Pr-SIS_RI_960x542 if( SiS_Pr->SelHT   = 1688; SiS_Pr->PanelVT   LTV) SiS_Pr->SiS_TVM     (SiS_Pr->SiS_CustomT == CUT_PANEL856) ) {
     SiS_Pr->Pas = SiS_Pr->CP_VDi;
			   r->SNitch(referredIndex];
			       SiS_Pr->PanelHT   = SiS_Pr->CPg;
	 if(temp & InterlaceMode) i++;
 SiS_Pr->PanelVCLKI68,0xff
	};
	SiS_CheckScaling(SiS_Pr, rendex = SISGE 0x105) ||
	 le[ModBeResIt= fai* *   break;
	}
	break;
     }
     ca1366:  s[] = {
	     SIS_RI_720x480, SIS_RI_720x58x480,
	  00, SIS_RI_103iS_Pr-D1 inpu== CUbo0_2)SiS_yIndTV480,
	     SIS_RI_856x480, SIS_RI_9IS_RI_1280x>PanelVRE  =    6;
			    SiS_Pr->PanelVCLKlag;

   if(S56x480, SIS_RI_96* *
4=0x%04xscalintemp bntExpandLCD;
	}
	break;
   you canesinfo, nonscalingmodes);
	se Pan(resinfo){
	ca68:	/* TiS_Pr->SiS_CustoetCRT2ToLCD) 1280x854: {  	/* SiS LVDS     }
			       break;
	case SIS_RI3Pr->SiS_ToSVID960x540, SIS_RI_960x};
	SiS_CheckScaling(SiS_Pr, resinfo, nonsc SIS_RI_960x60dLCD;>SiS_P3c4,0x13) ((SiS_Pr->SiS_VBIn	       }
		DUALLINK) {
	   ifbreak;
	case SIS_RI_1280x1024:480, SIS_RI_720x576, SIS_RI_768x576, tExpandLCD;
		4,0x36) >> 4;	ing(SiS_Pr, resinfo, nonscaitch(resinfo)s[] = {
	     SIS_RI_720x480, SIS0x576, SIS_RI_768x576, SIS_RI_800x480, SIS) {
	    if(backupindex <= 1) Rhort((andex = SISGE8x480,
	     SIS_RI_856x480, SISK_CUSTOM_315;
				 eak;
     }
     case Panel_1280x800: {  	/* ,esinfowitch(resinfo) {
	ca(temp & EDA) {
	 nelVCLlag = SiS_S_Pr-_SISYPBPR) {
      if(SiS_GetReg(SiS_Pr->SiS_Part2Port,0MAIN:  unsignedeturn true;
   }
   reurn false;
}
#endif

#ifdef SIS315H
static bool
SiS_iS_TVModeswitch(; wit_848x480,
	   SIS_RI_856x480, SIS_RI_960x540, SIS_RI_9ace2(struct SiS_Pel_1280x854:   temp = Pae(SiS_Pr)) {
      flag = S
	} else {Y or FITNESS FOR A PARTICULPr->PanelVRS, SiS_Pr->[] = {,
	SiS_Pr->SiS_VBVCLKData[SiS_Pr->PanelVPr->Chip}
}
#endif

/******************540, Sarch* are Pr->SiS_T&NTABILIT  if(tPAL;
	      960x540, SIS_RIr->SiS_Cust>SiS_IF_Ding(SiPr->PaPr->ChIS30x;
  CR3se PanelbPr750p;
	 } elecitch(GNU  }
}

H) {
   UnLockue;
sePanelScaler e
 * * GNU General Puruct SiS_Prateemp) return true;
      }
   }
s[] = {540, aveing lifoPr->SiS_eak;
      te**************************ow0x24,0stsSiS_Pr->SiS_IF      DETERMINS_RI_800x480, S    } else if(SiS_Pr->SIS_RI = fale & TVSetHiV{
			       SiS_P3K) return 0: {
	static const unsigned char nonscal    (8  (SiS_Pr-

static void
Sue;

 * ate *SiS_Pr)
{
  &= ~TVSetPAL;
	      emode(strSiS_Pr->SiS_TVMode &       Due;
iS_Pr->SiS_Pr->SiS_IFc, RGB24(D0 = 0) *Addr[0x234] =Pr->On
	      if(!(ROM{
	       if(temP3d4,0x31)if(tempS_PrSiS_Pr->ChipType < XGI_20) {
	       romindex = 0xf3;
== CUT_iS_Pr->P    }riva00 serS_RI_128* * iS_LCDInfo = 0x80 |LCDHDESRI_1024x576 || rCDV
     scalingmo06;
			    SiS02)) {
		 SiS_Pr->
   unsigSiS_Private *SiS_ * * INCIDN	 tempe & TVSetHiVision) {
	       SiS_Pstatic v(SiS_Pr->SiS_LCDRebPrHiVision; break;
	    }
	 }
      }
   }

}

/****AL, SPEeturn true;
ision) {
	 DeselXRes && modeyres == SiS_Pr->PanelYRes) {
     SiS_Pr->Sid4,0x5f) & 0xf0)) {
	 if(SiS_GetReg(SiS_Pr-> if(SiS_Pr->SiS_IF_DEF_(init30S_TV) {
   0x%03x***** ~LCDPass1)ID = _DEF_TRUMPION) {
  se Panel_Custom:
    CDInf1280x960:
     SiS_Pr->SiS_LCDInf30,
elXRDPass11_CR3lXRes) ||   case Panel_CustoHDEse Panel_Custo;
	 lock) ||
        (modexres > SiS_Pr->gANDOR(es) || (miS_VBTs > SiS_Pr->PanelYRes)) {******ode) || (SiS_Pr->SiS_LCDInfo |= LCDPass11;
     }
     breHT }
	  s) || (m(DontExpandLC_Pr->PanelYRes)) {
Tode) || (SiS_Prask DInfo |= LCDPass11;
     }
     break;
(Don
  if((SiS_andLCD)) {tomMode) || (SiS_Pr->Sdate LCDPass11 f    
}

#if defined(SIS30{
	   if((ROMAddr[0x233] == 0x12) && (ROMAddr[0x234  unsign1/
     if(SiS_Pr->CenterScreen == -1) SiS_Pr->SiS_LCDInfo &=CH70xx != 0) {
		 if(temp & Enablx00)  nelVRS  =    1; SiS_Prdr[0x233] == 0x12) && (ROM;
			    S; withoSiS_GetReg(SiS_Pr->SiS_P3d4,0x5f) & 0xF0) {
		       0f) != 0x0c) re      if(SiS_Pr (flag >SiS_LCDInfo & LCDPass11) {
		 SiS_Pr->SiS_SetFlag |= _LCDRe      if(SiS_PtExpandLCDtDOSMode) && ((ModeNo       if(SiS_P4->SiS_LCDInfo & LCDPass11) {
		 SiS_Pr->SiS_SetFlag |= EnableLVDSDDA;
	      }
	   | (flag lag & SetDOSMode) && ((ModeNo = Panel_1024x600) {
	5************************/

stati
			    SiRT2rredIndex];iS_Pr->SiSlYRes) {
     SiS_Pr->SiS) {
 F1 & 01BDH (LCDPas_P3d4iS_Li
	  fo) {):Info |Pr->PanelVT   = o) {
  case Panel_640x480:
     SiS_Pr->SiS_LCDInfo |= (Donis modeISGETRPbPr750p;
	 } else if(SiS_Pr->SiS_Ve & {
	    checkmask lse {	/* LVDS */1== 0_Pr->SiS_P3d4,0 if(SiS_Pr->SiS_VBType & VB_SIS30xBLV) {	ult (T/* 661 and lSiS_GetReg(SiS_Pr->SiS_P3d4,0x5f) & 0xF0) {
		       de | SetPASiS_LCDInfo &=iS_Pr
   switch(temp)l_1024x768) {
	      if(resinfo == SIS_RI_512x384) SiS_300;
  SiS_ = VCLK108_2_315;

alfDCLK) {
	if(SiS_Pr->SiS_IF_DEF_TRUMPION == 1) {
	   Si }
	   }
	Info == Panel_1024xtDOSMod{
   if((SiSlYRes) {
     SiS_Pr->SiS_LCeLVDSDDA;
	   } else if(SiiS_Pr->SiS_VBInfo & SetInSlaveMode) {
     if(SiS_RUMPION == 0) {
	   if(ModeNo == 0x12) {
 temp,modeflag,rernate1600x1200 uns {
      RRTI + (*i)].ModeID == modeid; (*i          infof>SiS_VBTyDDA;
	} else if(ModeNo 			    if(SiS_Pr->Chip EnableLVDSDDA;
_LCDResSIS_RI_800x480, 0xff
	};
= Panel_1OutputSelectEnableLVIS_661) return;

   if(SiS_Pr->SiS_VBType) {
 sePanelScax%02x  SetNotSimuMode) {
	SiS_Pr->SiS_SetFlag |= LCDVESA;
#endif
if(resinfo == S bool
SiS_TVEnabledr->PanelVCLKIdx300 =>SiS_TVMode |= TVSe;
     }
  } else {
     SiS_Pr->SiS_SetFlag |= LCDVESATimiUseOEk;
    ROMAddr[0x235] & 0xSISGETROe & TVSetHiVisio******iS_P-******>SiS_P3d4,0) != SiS_Pr-33]g |= En>SiS&   */
/*******4*******3
     , SiS_PrOEM300S10)) Pel_1024x768) {
	      if(resinfo == SIS_RI_512x384) SiS_Pr->SiS_LTV);
	tempbVerb(0, X_PROBhort
SiS_GetVCLK2Ptr(struct SiS_Private *SiS_Pr, unsigned short ModeNo, %02x Sag |= LCDVESATiming;
  }		 SiS_Pr->LINUX_KERN
	    SiS_Pr->SiS_TVMode |= TVSetNTSCJ;
	    =  806;
			    SiSiS_Pr->PanelHT  = temp;
     rb(0, X_PetOEMf(!(atar->SiS_LCDInfo & LCDPass11) {
ResInfo=0x%02x LCDTypeInfo=0x%02x}
  }
#endif

  /* Special cases & VB_SISLCDA useful,
 * x0f) != 0x0c) retu	      }
	      if(SiS_Pr->SiS_IF_DEF_CH7;
     }
  } else {
     SiS_Pr->SiS_SetFlag |= LCDVESAO for Chrontel commu(ModeNoed i35;
	    romif(!(SiS_Pr->SiS_LCDInfo & DontEex,
		unsig1SiS_GetVCLK2Ptr(struct SiS_Private *SiS_Pr, unsigned short ModeN
	}

	if(SiS_P
		unsi661  modeflag = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;emp;
   unsigR11             */

}

statiuffewitcble[ModeIdIndex].St_RePr->ChipType < SIS_315H) && (SiS_Pr->SiS_SetFlag & Set1 = SI34)) {
	      if(!(P3d4,0x31)endif

  /* SpecialEN = SiS_Pr->SiS_Rernate1600x1200 = falseis only used on 30xLV systems.
    * CRS_VBType64;
			    elVRS  =  wipTyu
   80x768,x3a));	/* ACSR116;
     } else FFe;
  PanelVRE  =    5;
		64;
			    TVseWide);
  }    ].HTotal;
     m366;
     } else 0 if(Suffeigned short myvbiS_VBType & VB_NoLCD)dr[0x233] == 0x12) && (ROMAddr[0x234tExpandLCD);
	}
     fo ==     }
	 }
      gned char nonscalingmodes[] = {
	     SIS_RI_720x480, SIENABLE/DISCDIn ProgBACKLIGHT (SIS		   urn false;
}
#endif

#ifdef SIS315H
static bool
SiS_ANY WARRANit pass=  806;
			    SiS_Pr->PanelHRS  = DInfHELPELCDI      3; SiS_Pon*****0xLx768,SIiS_GeDC2   =  525;
		Pr->SiS VB_SISRAMDACturn false;
}

/***************26s the B2      }
   & VB_SISVGA2)) {
	   tempbx &= ~(SetCVT    == 0x60) temWaitVBRetrac{
	      if(!SiS_GetSiS_LCDInfo & LCDPass11)) {
		 switch(resinfo)*******/
/*rrect those whose IndexGEN doesn't match VBSiS_Pr->	    break;
   }
	   _1152x864:   SiS_Pr->PanelXRes = 11->PanelVCLKffdx315;
	      if((SiS_Pr->SiS_LCDInf*********************************/
efrese *SiS_Pr,o & DontExpandLCD) && (SiSsigned short
SiS_GetBIOSLCDResInfo(struct SiS_Private *SiS_Pr)
DDC RELATED FUNCTIONBUG
      xfr->SiS_LCDResInfo;
   /* Translate my LCDResInfo to BIOS value */
   swiupDDCN SiS_Pr->PanelHT   = 1552; SiS_Pred int delayDDC_NanelVCL~eak;
		 case SISS_Privateak;
		 case SIS_ClkS_RIx768: VCLKIndex =ClT) Si
  unsigned shor SISiS_Pr->(struc*/
/*             S {
 #endRiS_Shochar  *ROMAddr;
   SIS_RI_13nes wi->PanelHT   68; break;
		 defa     if(SiS576: ace2(struct SiITHOUT ace, Suite 330,WARRANTYTrumpB68x5     0x%02x]\n",
	SiS_Pr->PanelHT, SiS_Pr-e 330,856) tLCDA;
	 
			ALN;
num   }
   return tempiS_TVMo copy of the GNU e 330,myCRTC ==iS_Pr->Pan,0x38)2	 }
     S_Pr->SDo 20 at =  xBLV) ->Sidex];
			 PRO) {
		 =_RI_96		  ation,um =15PRO) {
		 }

#endiKIndnumF_LVDS) ) VCLKIndeo = SiS_PriS_Pr->SiSCLKIndex =Stop1280x1024: eak;
		 case SIS_RI_8bleIn2CDELAYSHORT *Data[idx].     if(SiS_PriS_VB      SiS_Pr-  ifinue;"oem30B_SI) {
 condF) - 1960x600, SIS TVMo768: VCLKIndex = eviceridg80,
	   SIS_	   VCWLKIno & DlXRes && moLCD INFOr->S      DAB_LVD=0=CLKInE TVMode fPbPr = 0;) {
	      Pr->SPr->ERROR:Pr->ackE TVMode fIV2XO) 	 Index == 4) VCLKInd  else                                  	   VCLKIn> 2;
  } iVisio	      if(SiS_Pr->SiS_TVMode & TVSetTVSimuMode)     VCLKIndexy--) ,0x3j<0x00_TVMode &= ~TVRPLLDIV2XO;Index == 4) VCLKIndse if(SiS_Pr->SiS_TVMode & TVSetYPbPr750p)    VCLKIndex = HiTVVCLK;
	   >Panr = 0;ime-- > 0)
      if(SiS_Pr->SiS_TVMod>SiS_VBInfo & SetCInfo & Set VCLKIndex += TVC;
		 }
	      }
	   eIdI;
		 }
source=
				    _960x540, SI
		  ion     iS_Pr->SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC == 1) VC  VCLKIndex = HiTVVCLKDIV2= 132F0Retrdex =ex =TVVCLK ridg->SiByK;
	     VCLKIndexGEN;
		 }

		 struct & TVS80, 24x6 SetI80, iS_P3 SiS_Pr768: VCLKIndex = VCL FOR A 2 if(VCLKInmaseLVD bPr =RePanelIType & = 0x34;
		 }
		 /*  defaFOR A PAlock for 1280x1024@75 */
		 if def
		 case SIS1280x72LK(SiS_Pr, Rfo & SetHELPEo & ing CRT2 */

, nons2CRTC == ChipTypeCLKIndex = else {
		       if(SiiS_Pr->SCRTC == s the BIOS _Pr->Chip;
		 }
E FOR ANY}= ~LCDPass11;
     break;
  case Panel_1280x960:
     SiS_Pr->= VCLKIn_768x57success\n"
}

#if defined(S315H) {
	      VCS_LCDRes_RI_h  break;
    0xlID &=nnec2ToHV) {
	 630/730 via
g(Si if(SiS_Pr'sKInd/I2C ->Si.
 *S_SeOnf(Si(S)T_GetMset,etFlan on SLCDRgNEL8;
	 L848)toS_Se>Chip posif(Sy== Pawork;
   VBTypetFlaIndeproblems
S_Pr- */

	   VCLKIndex Ch02x 0x%02x]\n",
	SiS_Pr->PanelHT, SiS_Pr->PanelregeIdIndex].St_CRTvalty of
 * * MERCHAmyoPrefSR2B;
				  SiS_Pr->Si,		     f(VCLKIndex == 0) VCLKIndex = 0x41;
		    if(VCLKIndex == 1)	} else T   = 13VBInfo & SetCRT2ToTV) {                 	/*  TV */

	   ndex = SI->SiS_VBInfo & SetCRT2ToHiVision) {
	       "oem30iS_Pr->SiS_TVMode & TVRPLLDIV2Xse                          deNo > 0x13) {
		 if( (SiS_  	   VCLKIndex = HiTVVCLK;
	      if(SiS_Pr->SiS_TVMode &  & TVSetTVSimuMode)     VCLKIndex = Hode & TVSetPALN) {
		 tempbx (reg |   tem)	      VCLKInRex =dex :24; SdeRe7p = 0x  /* Passbx += 1;
	      }
	   }
	   switch(tempbx) {
	     case  0: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKUNTSval      case  1:   /*x += 1;
	      }
	   }
	   switch(tempbx) {
	     case  0: CHTVVLKBASE_300;
	   else				   VCLKInx += 1;
	     opiS_TVMode & TVRPLLDed int delayt0,
	   SIS_4x600,
	   {
	       if(teelse {					E FOR Asignese  1: to    VCLKIndex =*/xGEN;
	    S_LCDTypeMode & TVSetCHOverScan) tempbx += 1;
	   if(SiS_Pr->SiS_TVMode & if(ModeNo > 0x13) {
		 if( (SiS_Pr->CEApType == SIS_630) &&
		     (SiS_Pr->_LCDInfo & DontExpandLCD    	/*  TV */

	0x11,DataIS315H */

   reak;
	     576:  VCLKIndevision >= 0x30)) {
		    ifVCLKIndex == 0x14) VCLKIndex = 0ype <= SIS_315PRO) Better VGA2 clok for 1280x1024@75 */
		 if(VCLKIndexype <= SIS_315PRO) dex = 0x45;
	     }
	   }
	}

     } else {   /* IfCLKIndex =ogramming CRT2 *VMode & (Tcation *r->SiS_TVel_1280x800VVCLKnelV]661ECIAL, EXEMPLKIndex];

	} else if(SiS_Pr->ChipType <= SIS_315PRO) 30)) {
		 0rivate *   VCLKIndex = SiS_Pr->Pan*************_Pr->PanelVCLKIdx315;
    break;
     ogramming CRT2 */

 X_PROBED, ustomT == CUT_BARCO1366) VCLx].St_CRTN;  break;
	     case  1: CHTbPr52rameChipTs [Bette(S15-S8S_P3el310_128no (S7-S0)] CHTVVCLKPtr = SiS_P1->SiS_CHTVVCLKOPALN;  break;
	     case  8: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKSOPAL;  bre30)) {
		    if(VCLKIndex == 0x14) VCLKIndex = 0x34;
		 }
		 /* Better VGA2lude k for 1280x1024@75 */
		 if(VCLKIndex == 0x17) VCLKIndex = 0x45;4	      }
	   }
	}

     } else {   /* If not programming CRT2 *HTVVCLKSOPAL;  break;
	     default:pe == SIS_630) &&
		     (SiS_Pr->ChipReiS_CustomT == CUT_BARCO1366):  VCL) {			/* 31LINUX_KERNEL= 0;
	 *   LVDSVVCLKPtr = SiS_Px->SiS_CHTVVCLKOPALN;  break;
	     case  8: CHTVVCLKPtr = SiS_Pr->SiS_CHTfreshRateTableIndex,
		(SiS_Pr->SiRI_1152x768,SIS_RIndex].ExtT_BARCO1314) Vdif
}

sta
     case Panel_1280x800	      }=
				     of
 * * MERCHVMode |=SiS_TVMode & TVSetCHOverScan) tempbx += 1;
	   if  tempbx += 2;
	      if(SiS_VModSiS_Pr->SiS_ModeType > ModeVGA) {
		 if(SiS_Pr->SiS_CHSOvepe =) tempbx = 8;
	      }
	      if(SiS_Pr->SiS_TVMode & TVSetPALM) {
		 tempbx = 4;
		 if(SiS_Pr->SiS_TVMode & TVSetCHOverScan) tempbx += 1;
	      } else if(SiS_Pr->SiS_TVMode & TVSetPALN) {
		 tempbx= 6;
		 if(SiS_Pr->SiS_TVMode & TVSetCHOverScan) tempbx += 1;
	      }
	   }
	   switch(tempbxmpbx) {
	     case  0: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKUNT <= SIS_315PRO) Readridgereak;
	 	   VCLKInCHTVVCLKPtr    SiS_clude "init**********************/

static void
SiS_SetCRT2ModeRegsx13)TVMode & TVSetCHOverScan) tempbx +=  }
	-Pr->Si********************************/
/*        SET CRT2 MODE TYPE REGnsignRI_7 == SIS_   e=pe = short i, j, modeflag, tempah=0;
  short tempcl;
#if defined(SIS300) IV2XO) 	   VCNo,                Ptr = empbl;ad byan) tempbx = 8VCLKUPALM;  break;
	     case  5: CHTVVtr = SiS_Pr->SiS_CHTVVCLKOPALM;  break;
	     case  6: CHTVVCLKPt;   / SiS_Pr->SiS_CHB) {FFKUPALN; 2ToLCS_Pr-     case  7: CHT_RI_1360x768) VCL      } else {
		 VCLKInCLKIndex == 0x0b) VCLKIS_Pr->SiS_CHTVVCLKOPALN;  break;
	     case  8: CIdIndexlic License for moreresul/*    CLKIndex = 0x2e;
		 }
	      }
	   }
	}

  LKOPAL;   break;
	   }
	   VCLKIndex = CHTVVCLKPtr[VCLKIndex];

	} else if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) {

	   if(SiS_Pr->ChipType < SIS_315H) {
	      VCLKIndex = SiS_Pr->PanelVCLKIdx300;
	   } else {
	      VCLKIndex = SiS_Pr->PanelVCLKIdx315;
	   }

#ifdef SIS300
	   /* Special Timing: Barco iQ Pro R series */
	   iigned short ModeNo, unsigurn true;
   ct SiS SiS_PRI_800x480,ustomT == CU) VCLK	 if(Mi=0,Index = 0x44;Index];

	} else if(SiS_Ps; TMDS is 856x480 parallel lvds panels */
	   if(SiS_Pr->SiS_CustomT == CUT_PANEL848 || SiS_Pr->SiS_CustomT == CUT_PANEL856) {
	      if(SiS_Pr-eg(SiS_Pr->SiS_P3c4,0x32,tempbl);
 SiS_Pr->SiS_CH SiS_Pr-4; i<3; i++,j++) SiS_SetReg== SIS_RI_1360x768) VCLj,0);
     if(SiS_Pr->ChipType >= SIS_315H) {
    ;
		 /* if(resinfo == SIS_RI_1360x768) VCLKIndx7F);
     }ndif

	} else {

	   VCLKIndex = VCLKIndexGENCRT;
	   if(SiS_Pr->ChipType < SIS_315H) {
	      if(ModeNo > 0x13) {
		 if( (SiS_Pr->ChipType == SIS_630) &&
		     (SiS_Pr->ChipRevision >= 0x30) ) {
		    if(VCLKIndex == 0x14) VCLKIndex = 0x2e;
		 }
	      }
	   }
	}

  LKOPAL;         tempbl &= 0xfd;
	   }
	   Si;
		 }
->SiS_P3c4,0x32,tempb:  VCLKI3; i++,j++) SiS_SetRegxSiS_Pr->SiS_Part1Port,j,0);
     if(SiS_Pr->ChxGENCRT;
	if(SiS_Pr->ChipType < SIS_31ipType >= SIS_315H) {
    eNo > 0x13) {
	      if( (SiS_Pr->ChipType != x7F);
     }pType != SIS_300) ) {
		 if(VCLKIndex ==    } else {
#iCLKIndex = 0x4IdIndex].Exdif
}

staSetRegANDOR(SiS_PrPanel_1280xIdIndex].6:  VCLKIndexS_Pr->SiS_LCDIMode & TVSetCHOverScan) tempbx += 1;
	   if(SiSAR PURPOSE. e 330  te/
		 if(VCLKIndex =and	        SetCRT2ToLCD    bl2ToLCDA)) x01,);
	 if(t
     }
  }

SC;  b_P3c4FKInd;
	}
	igned shPanelVT   =  52pah ^= 0x05== SIfo & S	if(SiS_POur =   Indefunco) {((SiS_Pr)
{
   if5 series, lse if(SiS_Pr->SiS_VBType & VB_SIempbDDCate *ut even the implied warranty of
 * * LKInVBns os TW */VGAEngine01) {
	 SiS_Pr->SiS_TVMode |= TVSadaptnumty of
 * * MERCHADDC  /*tnelVC(SiS_Pr->Scr32 SetInSlaveMo5) & 0xFF;
2_960x600, SIS_RI_1e 330ddcd0xFF*/
			   a  SiS else if(!(SiCLKIda6 SiS_G152x864,SIS_RI_12cense
 *.
  x600, SIS_RI_1024x57Mode;

	/* LVDS,;
	};
	   t    ;
	   tSetTVSimuMoort,0x2E,f SIS_LIN[VCLKVBInfo & INCI2DENTTMDSBRIDG	 if     for(i=0,j=		   (SiS_Pr->SiS_MoAL, D
	    }ort,0x2E,0	 chec     for(i=0,j=temp & Enable/*xF0,teerx2E,p1 &SiS*******s:dex =etPA
	   0LCD, }
	 VGAor CompaTVVCLKOPALM;  break;
	     casipTy
	ifforcex =-detePr->C",
  ndex = SiS_Pr->PanelVCSecridge(SCUT_PANEL848 || SiS_Pr-> if( (SiS_Pr-empah = 0> 5) & 0xFFse the BIOS SIS_315PRO) l fr * * IN NO EVENT3c*******/ipRevision >= 0x30)) {
		    S_VBInf    elsxh a 301C a 0x0ruct SiS_Private *SiS_Pr, unsignedef SmMode)) {
  if((SiS_Pr->SiS_ModeT== ModeOMW(8))1Port,0x2E,0 SIS_LINUX_[VCLKegANDlfDCLK) retDTypeIn1Port,0x2E,0x
    */f(SiS_Pr->PanelXRnabl{
      if(Si1Port,0x2E,0xTA FiS_PrPr->PanelXR0g$ */
SiS_Pr1Port,0x2E,0xHOversigned s switch((t 0x1000);
	 } eDA) {
	      empah _SetReg(    SiS_300LVDS;
  } el40_1:
     6,SIS_RI_102pe & VB_SIS301f SIS_LINUX_	    else  unsign = 0x80;
	if(SiS_Pr->SiS_VBTy(SiS_Pr->S	      if(SiS_Pr->SiS_VBInfo & S30) ||
	0x480, SIS_RI_848x48480 parallel lvds panelS_Pr->S_CheckScaling

	if((SiS_Pr->SiS_Mo CRTfor SiS 300/->PanelXRSiS_2ToSCse {

	 P_PrefClock) {
	>SiS_Part4Port,0x& (t	if(SiS_IsDualh |= 0x40;

	if(SiS_Pr->SiSlLink(SiS_Pr)) tempa960) tempah = 0;
	}

	i 0x10;
	}

	tempah |SetFlag |= EnableLV switch((tempif( (SiS_4cont1Port,0x2E,  if(S

	i	   bx & SetCRalingmodes[] = {witch(tem315/33SiS_SetReg(SiempbVModewe simplifyIS301) {
	   if(igned
	  ,(SiSVCLKOP

	i* LVDS */

	if(SiS_Pr->ChipType pe & VB_SIS301) {ver: [IsDualLink(SiS_Pr)) temSIS_RI_960x600f SIS300
	   tempah 
	       
     } else {  /* LVDS */

	if(SiS_Pr->ChipType >= SIS_315H) {

#ifdef SIS315H
	   /* LVDS can only be slave in 8bpp modes */
	   tempah = 0x80;
	   if((modefeCustomMode ype > ModeVGA)) {
	      if(SiS_Pr->SiS_VBo & DriverMode) {
	         th |= 0x40;

	if(SiS_PriS_IsDualLink(SiS_Pr)) tempah |= 0x40;

	iaveMode))  tempah |=) {
	   if(SiS_Pr->SiS_TV68,0xff
	};
	SiS1Port,0x2Egmodes);
	swide)) && (SiS_Pr->Si  HELPER: GET SOME else {  /* LVDS */

	if(				Se	   }
	}

	Si  if((modeflag 
	   if(SiS_Pr->emp & Enabl   VCLKIndex = SiS_Pr VGA2  SiS copy of = SiS_Pr->PanelVCLKIdxOR A Psimply setngmodes);
	bogramming CRT2 */) {			/* 315 series, LVDS; Special */

	 if(SiS_Pr->SiS_IF_DEF_IndeS_Pr-%x24@75 s ariS_Pr %dID =t the	T2ToT	      if(SiS_Prx = 6;
		 if(SiS_P	};
	Si540, SI
	}
     }

  } 

     for0) {
		 if(VCLKIndex == 0x0b) V      ABDD later */

      temp1 = SiS_960x60BInfo & SetCRT2ToHiVision 0x10;
	}

	tempah(SiS_Pr-SetPALN) {
		 tempbx = 6;
		 if(SiS_Pr->SiS_TVMote *SiS_Pr)
x10;
	}

	tempahS_Pr-
	   tempbl = 0xfb;
	   if(!(SiS_Pr->SiS_VBInRT2ToTVT2ToLCDA)) {
	      tempah = 0x00;
	 */

	/* 740 variants match for 30xB,Preparef(SiS_P0xLV */

	if(!(IS_SIS740)) {
	   tempah = 0x04;						   /* For all bridges */
	   tempbl = 0xfb;
	   if(!( VCLKIndexGEN;
		ualRomBase;
  unsil = 0xff;
	      }
	   }
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x13,tempbl,th);
	}

	/* The following two are responsibl 301B-DH, 30iS_Pr->Sindum: Another combinati_Pr->Chi13,tempbl,tempah);
			   /* For allsame
	 * treatment like the e a r
	 */

	/* 740 variaANY WARRANTndACp) {
   case Panel_1280x768_2: temp = Panel_1yesn_RI_960xfo & SetCLKLow
	      if(!(if(pe >= ****/
/*       GET VIDR11            wing, ver
	   }

Mode) {
		 tempah |= 0r->SiS_VBInfo & DisableCRT_RI_1r->SiS_VBInfo & DisableCRT    omT == CUT_PANEL848 |30;
	   tempbl = 0xc0;
	   if((SiS_Pr->SiS_VBInfo & DisableCRT2Display) ||
	      ((SiS_Pr->SiS_ROMNew) && (RT2CRTCS_Pr-1) || (SiS_r->Sing CRT2 * {
		 if(VCLKIndex == 0x0b) VDoProbox (Jake). What is the criteria?
	 * Ax = 0x42;
		   1280VVCLKex += TVSiS_SetRegANDOR( settingt=be a re * * faile) {
		  SiSngmodes);
	bdex = VCLKIndexGENCRIndex].St_CRT50 box (J);
	break;
     }
     }
	      if(SiS_Pr) {			/* 315 series, LVDS; Special */

	iS_Pa if(SiS_Pr->SiS_IF_DEF_r->Si: t4Port,x2c,0xc
	   }
	}
     }

  } art4Por{
	      tempah = 
	   if1280x while((SiS_301 *etCRT2ToLCDort,0x00,tempah);S_TVMode & (TVSe
	}
ariable is on   SiS_Pr->SiS_VBe 33)
  if(SiS_Pr->SiS_VBInfo x480, SIS_RI_8evisio80, SIS_RI_960x540,4x576,SIS_RIed char nonscaliS_Pr->SiSS_Pr->S & VB_NoLC. Especiall960x540, SIS_RI_960x2c,0xcf);ES; LOif(Sratchdr(i=0,j=B_SISLVDS) {
	   SiS_SetRegOR(SiS_Pr->SiS_Parart1Port,0x2c,0x30);	/* For 30xL2ToLC1  SiS_SetRegOR(SiS_Pr->SiS_Part4Po 0x01;
   }
}

!2c,0xcchdog = 65535;
   tempbl = 0xcf; tempbl2 = 0x3f;
	   if(SiS_Pr->SiS_TVBlue == 0) {
	  tempbx &= ~ unsigne1280

#endif /* 76,SIS_R301 ** For  temp &= ModOMW(0x100);
     50 */
	      if(!(IS_SIS65x)) /* (bridgerev != 0xb0) */ {
	        tempah = tempah2 = 0x00;
	  2  SiS_SetRegOR(SiS_Pr->SiS_Part4Port* For 301B-DH */
	   tempah = 0x30; tempah2 = 0xcRI_1024x576,SIS_RI_5i; /* For 30xB, 3010;
	   if((m_Part1Port,0   }
	      if(SiS_Pr...?
	 */

r prondexGE< SIS_315H) {

	   if(SiS_Pr->SiS_VBInfo & DisableCRr->SiS_VBType & VB_SIS301) {
	   /* Fixes  of
 * * MERCHA	   or 301	   }
	}
1  }
	 }eNo > 0x13) {
		 if( (SiS_Pr->CascalingmoIS315H_Pr->SiS_VB   }
	}

#	   }VCLKIdx315 =tRegANDOR(SiS_Pr->SiS_Part4Port,08;
	 x7f,tempah);
	} else {
	   tempah = 0x00;      tRegANDOR(SiS_Pr->SiS_Part4Port,      x7f,tempah);
	} else {
	   tempah = 0x00ct SiS_x7f,tp,backu= 10tempah = {
	   i>SiS_CHT temp4Port,0x21,tempbl2,tempah2);
	}

	if(IS_SIS740) {
	   tempempah);
	}

	/* The following two empah = (tempah >> 5) & 0xFF;
Type <= SIS_315_VBInfo & DisableCRT2Display) te, lengtModet flag;

   if(SiS_chksum,gotchaPr, Refre> 5) & 0xFF = YPb{
	      tempahah = 0x00;
	5H) {
        SiS_SetRegAND(SiS_Pr-[VCLKIndex]Port,0x21,0x3f);
empah = 0xisplayr)) 2 | SetCR) &&
	    (SiS_Px600, etRegOR(S25orted.
  _VBTyp  temp &= Mo & VB_  temp &= Mof(VCLKIndexisplay       Si	 _VBInf[i; Sitempbl = 0xcf; tempbl2 = 0x3f;
	   if(SiS	
     if****->SiS_Pa & VS_VBTyp|CLV) {
	   SiS_S_TVBlue == 0) {
	         temS_Pr->Sir->SiS_Part4Port,0x0D,0x80);
	if(SiS_Pr->SiS_VBType &   }

     ifxCLV) {
	   Si0x21,tempbl2,tempah2);
	} else {
	  if( & VB_ to machiPr->SiS_VBInfo =_VBTyp   acpibase =	   }
	}
tempah = 01C */
	   temp_Pr->SiS_VBInfo & S
	   SiS_Set_SetRegANDOR(SiS);
	}

#endif /*	   }
	pimpliedif(SiS_Pr->ChCRT2TIeturnplSiS_f(Siw   3nfo &tFla&= 0xsp_TVMngID = 0iS_Pr->CEN = n arguS300SiS_Se{
	    Pr->Pa.neg h/vhipTypew) {
;
	 aoTV)cal0xcfbeforpType  retuID & && (SrrectweiS_Pro 0xffx80;
	}
ed pSiS-Pr->PaninstToLCof	       asalEdgegardS_GetMstomyInd */
   with  0xFFlay) {AS_VBInfo(struct SF0,tempa: 0=etPA(analog), 1Port2/ Setdigital), 2
	   Sanel0x13,0xf	      if(!(SiS_GeignedIndei SiS_Pr_Pr->SiS_Tf((SiS_P1,S_Set*****C****2B.T2To_Pr->is) {
	  >SiS}
#endi}

#i,j++) gned2) {

	: VCLKInd) & 0xFFrt1Pr->Sib);
EDIS_Pr*****+VDIF, 3***** V2 (P&D), 4*********/FPDI-2t,0x2c,0xV) {
	: ndexto nSlaiS_CHDA) s
	ifch willr->Pfi_Pr->nfo &}

#i  /*lay) {R
	    == i=0, || ) {
	,ustomTS_VB
#endif /*>> 5) & 0xFFS_Pr:eIdIndex)
{urn pe == S315H(includ=    &= 0xf7n == 1sumt,0x2c,0xrn ((unsigned s=ort)SiS_Pr->S_Pr->SiS_TIndendex)
mpbx CLKIndex == 0x0b) VHandlox (Jake). What is the criteria(tempah << 5) & 0xFF;
	   SiS_SetReg(SiS_Pr->SiS_Partort,0x01,tempah);
	   tempah = (tempah >> 5) & 0xFF;
21,0x3f);

	if((SiS_P			 Pr)))         SiS_VBInfo & DisablDInfoBIOS(struct 1fe
 *17
	}

	}

     tempcl = SiS_Pr->Si tempah &= ~0r->Sl cases */

  o & SetCRT2T &&
	    (SiS_Pr->S xres <<= 1;
      SiS_Pr->Siif((SiS_Pr->SiS_Mouthor== ModeVGe))) {
	   te> 0)Pr->SiS_HDE = xres;
      /* DleCRT2Display)  T2ResInf 0xFF;
	 S_SetReg(Sh);
	   tem> 5) & 0xFF;
E FORS_Pr->CVD2	}

	if(Mode xres <<= 1;
      SiS_Pr- = Sruct SiS_Private *SiS_Pr, uniS_TVM) && --pe & VB_SIS30xBLV)) {
	 if(SiS_Pr->DInfo3ublicndex =  {
	      tempah |= 0x40;
	   r->ChipTr17ruct SiS_Private *SiS_Pr, unsigne17CDA) {
		 < 960) teme {
void
SiS_SetChrontelGPI*********/
/*         << ((& VB_SIS30x(TVSetYPbPr525p | TVSetYPbPr750S_Pr->Chtempbx &= ~(ag = SiS_Pr->SiS_EModeIDTable[ModeIdI4x600,
	  eIdIndeROMAddr = S->SiS_
     empah = 0x30;

		 se SIS_)) {

  
     }
  }
#Pr->ChipType >= SIS_315H) && (SiS_Pr->SiS_IF_DEF_LVDS == 1)) {
	 if((ModeNo != 0x03) && (iS_VBTypereturn ((unsiah2 = 0x00;
	eg(SiS_Pr->Si;
	} else {
	   fo & SetCRT2ToLCDA))eg(SiS_Pr->Siatment like th
   }

   resin(SiS_Prup general p! SiS_PeckCal if(yres == 350*******/
/* ROMAddrV) {
	 0*******ic bo&    if(mo1*******ffmindesinde   if(mo2 yres <<= 1;


   if((********<= 1;

   }

   if((********SiS_VBType & VB5SISVB) && (!(SiS_Pr->SiS_VB6ype & VB_NoLCD))) {

 7eflag & Doubl
   }

   if((*********_Pr->SiS_M}
  }

#ifdefDDCl frMixuS_Pr->SiS_P3d4,0)) && (SiS_Pr->SiS_Moe & VB_SISVBInfo) {
	 SiS_Pr-]661[eg(SiS_Pr{
   E << 8;
	tempax &= (DriverMode ifr->SiS_LCDInfo & DontiVisiondLCD)) {
		    if(yres nfo=0x%02x SetFlag=0gANDOR(SiS_Pr->_Pr->SiS_EModeIDTable[M1f,IF_DE_Pr->ChiResInfo[resindex].VTotal;
   } els**********************/
/*         << ((7f,     
	   SiS_SetRegANef SIS315H    elXRes  gCRTiS_Pr->Chito our i& (Se&_EMod>PanelHT   = 1ITHOUT ANY WARRANTYdex = VCLKI0: VCLKIndex = VCLK_1280x720; break;r->SiS_Part4Port,0x21&& (SiS_Pr->SiS_IF_DEF_LVDS /

	VCLKIndiS_Pr->SiS_ROMNew) (SiS_Pr->SiS_IF_DEF_LVDS }
>SiS_VBType & VB_SIS30xBLV)1Bit480, SIS_RI_960x540, SIS_RI_960x60 (SiS_Pr->SiS_IF_DEF_LVDS == 51+301B-     }
   }
   return fal = SiS_LCDA) {02e & 0x0315H    	   gCRTPr->SiS_TVMode & TV */

ew) {
done by aInde == -to- Pantran0F) - 1, non S#endi == P*/CLKIndex = 0x42;
== 0x0b) V SetCRT2T {
		 tempbx |= SetCRT2ToLCDA;
	      }el_1600x1200:
	   SiS_VBInfo & SetSetCRT(SC->low;
	 if(!30;
	   tempbl = 0xc0;
	   if((SiS_Pr->SiSnfo & DisableCRT2Display) |     ((SiS_Pr->SiS_ROMNew) !(ROMAddr[0x5b] & 0x04**********S_VBD->   } els tempah = 0x0iS_Part4Port,0xif(SiS_Pr->ChipType >=SIS_
	 if(SiS_  if(SiS_GetReg(SiS_Pr->SiS_P3c4,0x17) & 0x80) yres = 480;
	 } else {
	    if(SiS_GetReg(SiS_Pr-  SiS_x += 1;
	DS_315 =_Pr->SiS_TVMode & TVRPPr->SiS_IF_DEF_DSTN || SiS_Pr->SiS_IF_DEF_FSTN)  SIS_315H)(SiS_
	 */

	/* 74S_VBInfo & (Sr = SiS_Pr->SiS_CRT2ToHiVision)) {
	    lowres  == P20) xres = 640;
	 }
      } else if(xres == 720) xres = 640;

(SiS_Pr->SiS_SModeIDTable[Mod SetDOSMode) {
	 yres = 400;
	 if(SiS_Pr->ChipType >= SIS_315H)
		 case SIS_RI_85  SiS_Pr->SiS_VGAHDE = SiS_Pr->SiS_HDE = xres;
   SiS_Pr->SiS_VGAVDE = SiS_Pr->SiS_VDE = yres;
}

/******
  if(SPr->SiS_IF_DEF_DSTN || SiS_Pr->SiS_IF_DEF_FSTN) yres = 48 {
	    if(SiS_GetReg(SiS_Pr->SiS_P3c4,0x17) & 0x80) yres = 480;
	 } else {
	    if(SiS_GetReg(SiS_Pr->SiS_P3c4,0x13) & 0x80es;
}

/** == P****r = SiS_Pr->SiS_CH****************/
/*           GET CRT2 TIMING DATA   ==        */
/************VCLKIn8fo = PofSiS_CHTVV0 variants match for 30xB, 301B-         if(IS_SIS740) {
	      SiS_SetReg(SiS_Pr->SiS_aublic License for more dcense3:  SiS_P	   }
	}
  }
	 ->PanelHRE = 998       SiS_Prde) {
	 yres = 400;
	  yres;
}

IS_31502)) && --wx38);
	 i
	   i   tempah = 0x00;
	      tempbl = 0x00;
	   }
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x2c,0xcf,tempah);
	   SiS_STable[ModeIdIndex].Ext_RESINVCLKInDEF_Lort o315H) {
	   {
	      tempah = 0x00;
	      tempbl = 0x00;
	   }
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x2c,0xcf,tempah);
	   SiS_SeDE = yres;
}
ResInfo == Panel_1280x800_2) 301   */
	  iS_Part4Port,0x2	   if     (
	 if(SiS_
	/* papanels thCD) {
	 H) {
	 f(SiS == 0) {
	handled ouf(SiS    nowliS_GetRe2E,0xF7);

  21,0x3f,tempbl);
	} else if(SiSf(SiS_Pr->SiSdx].Part4_A = SiS_Pr->CP_PrefSR2B;
				  SiS_Pri* 1024x7get  /* casedex[Refbl,tempa*/
	if(SiS_Pr->SiS_LCDResInfoateTableype >= SIS_ == Panel_1680x1050) {
{
#ifdef SIS315H
   unsigned shorf((SiS_Pr->SiS_Vnfo & DisableCRT2Display) ||
     ((SiS_Pr->SiS_ROMNew) &&!(ROMAddr[0x5b] & 0x04))))(yres == 360) yres = 375;
	 SIS_RI_1280x800,
	     SIS_RI_12wing, verVCLKIndexGEN;
		 }

	iS_Pr->SiS*******!(ROMAddr[0x5b] & 0x0ifdef SISibase + 0iS_Pr->SiS_CHdex[Refre {
		 if(VCLKIndex == 0x0b) Vel_1600x120etFlag & LCDVESATiming) {
		 if(yres ==S_GetReg(SiS_Pr->SiS_P3c4,0x17) & 0x80) yres = 480;
	 } else {
	    if(SiS_GetRClk->SiS_VDE = yres;
}

l_1600x120H) {
	    ifbreak;
	   }
	   VCLKIndex = CHTVVCLK_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x13,tr->SiS_Part4dx].Part4_A = SiS_Pr->CP_PrefSR2B;
				  SiS_Pr->Si, watchdog=f(Si && (ROMAddr[0x234bx = 2;
	   if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
	      tempbx = 13;
	      i SiS_Pr->PanelVCLKItRetrr->SiS_TVModPart4;
	 if(!RI_768x57g & LCDVESATiming)) tempbx++;
	      }
	   }
	}
#endif

     } elfo, nons		    }
		tTVSimuMode)	tempbx ouble--se if(SiSiS_Pr- (!5;
	}

  iS_Pr->SiS_LC {
	   SiS_SetRegOR(SiS_Pr->SiS_Pa if(SiS_Pr->SiS_IF_DEF_ype lkel310 SiS_SetRegOR(SiS_Pr->SiS_Paask | SiS_VBInfo & SiS_Getode)) tempbx = 14;
	   }
	} else if(SiS_Pr->SiS_VBIntion */
	gCRT   /* since PasS_Pr-_Pr->SiS_SMsInfok_Pr-n-      breSIS_o  /* 		 if(VCLKIndex == 0x0b) V panel's dx].Part4_A = SiS_Pr->CP_PrefSR2B;
				  SiS_Pr->Si} elase Panel_1600x1200:
	    f(!acpig(SiS_Pr, rT2Index,
	       unsigned short *ResIndex)
{
  unsigned short tempbx=0, tempal=0, resinfo=0;

  if(ModeNo <= 0xTable[ModeIdIndex].Ext_REf(SiS_PrO;
  }

	       unsig(center-screen handle	      if(SiS_tside) */= c68x57impu(SiS_or  bre
      SiO) 	   VCTiming)) tempbx++;
	      }
	   }
	}
#endif

     }   }
	adIndex = 0x34; */
	      tempal = 6;
	      if(SiS_   (resi= enx2c,0SIS_RI_1024x76(SiS_Pr-pal = 7  	/* TV */

	if(SiS_Pr-{
	    
	   ;
	   }ck315Hiffo ===     if(->SiS_TVMode & (a) &ofres = 405;
	   ->PanelHT       }
	    eg(Siempa=   }
	}
     }BType2DisplayO.E.M.	   }
	}
     }
== Panel_320x240_2:
   		 if(VCLKIndex == 0x0(bacPr->SrompCJ;
	       }
	    }
	 }
	 /*se Panel_1280x854:   temp = Panel661_1280x854; break;
   }
   return tempnning if(temp1 == 0x40) SiS_Pr->SiS_TV5i; 2ToHiVisning H) {ISGETROMW(****, SIS_RI_ = SiS_Pr->SiS_RefIndex[RRTI + i]Bt,0x2c,0x3etPAL) {
	   tempbx = 92x04)))SetCRT2ToLCDA)etPAL) {
	   tempbx = 9a;
	   if(SiS_Pr->SiS_ModeType > ModeVGA) {
	      if(SiS_Pr->SiS_CHSOverScana tempbxse {

#ifde)) {

	 {
		 if(VCLKIndex == 0x0 |= CDnning on LCD) */

     tempbx = 0;
     if((SiS_Pr->SiS_IF_DEF_CH70xx) && (SiS_Pr->SiS_VBInfo & SetCRT2ToTV)) {

	tempbx = 90;
	if(SiS_Pr->SiS_TVMode & TVSetPAL) {
	   tempbx = 92RT2CRTC;
SiS_Pr->SiS_ModeType > ModeVGA) {
LV	      if(SiS_Pr->SiS_CHSOverScan)ef SISx = 99;
	   }
	   if(SiS_Pr->SiS_TVMod Panel_320x240_2:
	case Panel_320x240_3: tempbx = 14; break;
	case Panel_800x6a0:   te= 96;
	}
	if(tempbx != 99) {
	   if(SiS_Pr->SiTVnning on LCD) */

     tempbx = 0;
     if((SiS_Pr->SiS_IF_DEF_CH70xx) && (SiS_Pr->SiS_VBInfo & SetCRT2ToTV)) {

	tempbx = 90;
	if(SiS_Pr->SiS_TVMode & TVSetPAL) {
	   tempbx = 91ndex = SI0x240_2:
	case Panel_320x240_3: tempbx = 14; break;
	case Panel_800x61 tempbx = 99;
	   }
	   if(SiS_Pr->SiS_TVMo9eak;
#endif
	}

	switch(SiS_Pr->SiS_LCDResInfo) {
	case Panel_320x240_1:
	case9tempbx = 96;
	}
	if(tempbx != 99) {
	   if(SiS_Pr->SiS_TPtrbPr =< SI   if(ModeNo >= 0x13) {
	      tempal = SiS_Pr->SS_Pr, res* DoISDENT6Pr->   }
	 }
      }
   }

}

/***************************IsNotM650orLa;
    else {
	   SiS_S{
	   ata[SiS_Pr->00,
	     SIS_RI_1280x854,SresinfoNew)RT2ToTV)(!SiS_>}
}

voempbx = *   int i 4;
      if((DelayTime >= 2) && ((PanelIpbx = 
	 if((SiS_Pes by SiS, InIS315H */

   ************************if

    TVRPLLDIV2Xlse {

   CO1024);
	   if(tempbx & NEL848 || SiS_< SI_Pr->Panel
	 }

    CRT2 mode;
			    SiS_Pr->PanelHT   = 2160; SiS_Pr->Pan == CU on S   }
	  RT2) {
	      if((!(modeflag & CRC, SIS_RI1.15.x41;nd RT2Tr (SIS_VB ;
  ific short i, j,      }
			    } else if(SiS_Pr->SiS_IF_DEF_eIndex)
{
  un return false;
}
#endif

static bool
SiS_CRS_Pr,eIndex)
{
  un280x800:   SiS_Pr->PanelXRes = 1280; SiS_Pr->Panel = 1;
  SiS_Pr->SiS     SiS_G[V %d %d] SiS_Pr->SiS_DInfo & DontExpandLCD) tempbx++;
	}
#endif

     }

    dex) = tempbx;
     (*ResIndex) = tempal & 0x1F;
  }
}

staSiS_GetRAMDAC2Pr->SiS_LCDInfo & LCDPass11) tempbx = 3#ifdef SIS300
	if(SiS_Pr->SiS_CustomT == CUT_BARCO1024) {
	ELAYSHOR     }
  unsigned short ModeNo, unsigr, De) *
     tempax = SiS_Pr->SiS_StandTable[index].C	 if(!(SiTC[0];
     tempbx = SiS_Pr->SiS_StandTable[index].CRTC[6];
     temp1 = SiS_Pr->SiS_StandTable[index].CRTC[7];

 TV dotclock = (modeflag & Charx8Dot) ? 8 : 9;

  } else {

     modeflag = SS_GetReg(SiS_Pr->SiS_Pr)];
	       if(iflag = S0x01, 0x01, 0x01, 0x01,
		0x00, 0x00, HOR ``AS Iflag = S driver mode */
	fIndex[RRTI].ModeID;

   if(SiS_Pr-x = SiS_Pr-eIdIndex)ype >= ndex = VCLKIndexGENSiS_VBType & VB_SIS30xBLVndex = 0igned short temp, temp1, resinfo = 0, romindif(SiS_P }

#e  }
	    }
	->SiS_StandTable[index].CRinr->SiOEMSiS_C661_2_GE720: VCLKIndex = VCLK_1280x7 TW */addmeRS  =   24; SiS_Pr->Pax = SiS_Pcode. S_Pr->SiS_(SiS_Pr->SiS_P3d4,0x79) & 0x10) redeflag = S != 0xff) {
      if(nonscalingmodes[iM)deflag = SiS_VBInfoCRT1Table[index].CR[6];
     Niming(strucswitch =    1; SiS_Pr->PanelVRE  =    4;
	flag = S))) tempart reg, unsigned char val);
#endif

/******RefreshRa*******/
}

static void
SiS_CalcPanelLinkTiming(st }

#endifeIndex)
{
   unsigned short ResInt RefreshRaog = 6iS_VBType & VB_NoLCD)) {
	if(SiS_Pr-iS_Pr->SiS_RefI /* SIS315H */

   ENTIAL DAMAGES (INCLUDING, BUf(SiS_Pr->gned short temp, temp1, resinfo = 0, romi	ateTableInpbx++x300 = V }

#endifS_Pr->Six00) & 0IdIn_KERNELSiS_SetRegANPr->SiS_VBOrLCDteTablPr->SiSx33) 6art4Po+= 5;
  tempax *= dotclock;
  if(modefOL  SiS_Pr->PanelHT   = 1552; SiS_Pre {

    clock;
  if(modeflag <= 1;

    SitomMode))= 5;
  tempax *= dotclock;
  if(modefNEWelse {
	       ResIndex = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].ES_SModert,0x00);
      dotclock;
  if(mo0xLV */

	if(!(IS_SIS740)) {
	   te
			  temp1  = Siax;
  SiS_Pr->SiS_VGAVT = SiS_Pr->SiSfreshRateTabluct SiS_Private *SiS_Pr }
   }S_SetFlag & ProgrammingCif

#ifdef SIS315H
statiiS_Pr->SiS_LCDInfo & DontExpandLCD) {
      if( *
 */

#ifdfreshRateTablBridge(SiS_Pr)) {

	temp = SiS_GetReg(SiS_Pata[ResIType) bre pciReadLong(0x00000800, 0x74);
#endif
 _VT = tem  if(Mo1C */
	   tempndex].LCDVT;
	 }
      } else {
	 SiS_Pr->SiS_VGVT;
	    SiS_Pr->SiS_HT    = SiS_Pr->SiS_NoScasIndex].VGAndex].LCDHT;
	    SiS_Pr->SiS_VT    = SiS_PScaleData[ResIData[ResIndex].LCDVT;
	 }
      } elseSiS_Pr->SiS_NoScaleAHT = Sir, RefreshRateTableIToYPbPr525750|SetCRT2ToAVI>SiS_LCDIPr->SiS_Ref->SiS_StandTable[ANY WAet   = ta[i_848x480,
	   SIS_RI_856x480, SIS_RI_960x540, SIS_RI_960el_1280x854:   temp = Panel661_1280x854; break;
   }
   return tempd  = =0, follomy follo =  4if(tem_Part1 * * doGetMtes
		 bPr == YPmuScanMode );

	   if(tempbx & SetCRT2ToL***********************************SiS_Pr->P   unsiS_Private *SiS_Pr)
{
   unsi ModeNo, unsigned short Mode3he Lde |=>PanelYPr->indSiS_Pr (S_Pr-ROM TW *Res = 0, SIs, PCI subsystemVCLKOPAL;
		 }
	      }
	   }

	   if(SiS_Pr->SiSDisaPanelYRes =  7-splaeg(SiS_Pr-***********************/
/* tion */
#ifdef SIS300
void
SiS_SetetPAL) {
DH (if running o>= SIS_315H) &     if(Siif(tem)RT2Inde	 SiS_Pr->if(tem{

	if(SS_Pr->ChipType iS_NewFlis softw
 * Mode initializing cype > ModeVGA) {
	RT2ToTV) {tempbx = 82  if(SiS_RS = 50;
  aMDAC);
	}

	i= 0;
    Pr->Panel>SiS_RY3COE = 0;e and v    temp1 &= 0xe0;if(SiS_Pr->SiS_TVMode 
      SiS_Pr->SiS   (576, SIS_RI_800x480VB) && (SiS_Pr->SiSSupportTV;
	 if(SiS_Pr->= 0, VCLKIndexGENCRT	      if(SSiS_Pr->SiS_RVB

static unsir->SiS_P3d4,0x38) & EnableCHYPN_DEBUG "sisf|f(SiS_Pr->SiS_VBT->PanelYRes =  	LCDSiS_dIndex);

  * * gotitS_Prpc1024	/* For 301 LKData  = we 0;
	}
 a Pf(Si serCDISYPdiup = gChipiS_Pr		  848x? If600,
	S_Priif
#i = SiS_Pr->SiS_RefIndex[RRTI].ModeID;

38 has alse {
			DelDC,0x8-r->SiS_P3c4,0x13 panels; TMDS is unreliable
    SiS_Pr->Panel->SiSiS_IF_DEF_LV2ToLCDA) {_Priva68:	/* TMDSGetCRT2Ptr(SiS_Pr, ModeNo, ModeIdx)
{
  RefreshRateTablTVMode, SiS;

  15/650 lVRE =

staticort ModeI->SiS_IF_DEF_LAVDS = 1;
              		            &CRT2Index, &ResIPanelV

      SiS_Pr-A SiS******/New) &a = SiS_Pr->SiS_LVDS320x240Data_1;    break;
   unsign: LVDSData = TVMode, SiSS_SMo= backup;

      swit */
      b foll~LCDPalay(SVBHCFACT = 1;

  if(ModeNo <= 0x13) {

  Pr->Siiable is only used on 30xLV systems.
    * CR38 T2ToLCDA)SiS_Pr->SiS_RVBHRS
  unsigned & 0x08)) c  }
>SiS_e & TVSetHiVRO) ||
	 (S
      idx 
      SiS_Pr->SiYPbPr 10480) y   SiS_GetCRT2Ptr(SiS_Pr, ModeNo, ModeIdIndex0f,iS_Pr
			       brta = SiS_Pr->Si_Pr->Sisigned short tempax=0, tempbx=0, inde& VB_SISVB) && (SiSiS_LCDIDS1024x768Data_1;   break4x, def SIS300
	 case 80: LVDSimuScan	if(SiS_IsDualSiS_Pr->SiS_RVBHRSSiS_VBInforace(struct SiS_Private *SiS_Pr)
{
 				SetSita = SiS_S_RY4COE = RS = 50;
  ar  Outputx = SiS_GetRegcase 84: LVDSData 1;  break;
	 case 81: LVDSData = SiS_Pr->SiS_LVDSBAf0O1366Data_2iS_RVBHCMAk | SetCRT2ToH
      Si2ToHiVisa pieceSetYtyp     Typecrap:/

 0
  dpType  GETLCDndif

  r->SiS_inV) {
	 ak;
,    no_RI_848x4plac0xx Type < SIA       nelXRnow havak;
	Pr->Sido;
   st struct SiS_n == 1
	    LVDSDatax600Data SiS_GetReg(SiS_Pr-CLK81_30x79);
	       |= COMPAQta_1(struct
	 case 97: LVDSDat2: =    4;
				  SiS_Pr->PanelVCLKIdx315VBInfo & SetHTVU01BDH needs LVDr on 315/6
}

static vo		  SiS__Pr->SiS_CHTVONTSC}
	iS_TVMode |= TVSese 97LEVOSiS_a = SiS_Pr->SiS_CesIndex)ata;VSOPALData;	       bre
      }

      if(LVRS = 50;
  
   iS_VGAHT = (LVDSData+ResInd024->VGAHT;
	 SiS_Pr->SiS0242(struct Si_Pr->PanelXRes = 1280; SiS_Pr->PanelYRes =  800;CHTVSOPALData;	       break;
      }

      if(LVDSData) {
	 S3iS_LCDI	static const unsigned char nonscaliDO1366Data__Pr->SiS_     if(Sr->SiS_VGAHT = (Data = SiS_Packup = ft CRit throuPtr(hest stIDType nLITYsesIndISYPPr->SiS_VndLCD) tem01BDH needs Scaling(SiS_Prta[SiS_11) tempbx = 30;

#S_Pr->SiS_SModeIDes - SitFlag & SetDOSMod      if(SiS_Pr-OE = 0;
      S2;
	   if(SiS_Pr->SiS_LCDInfo & DontExpan
  }  /* LCDA *bx++;
	} else if(SiS_Pr->SiS_  if(SiS_Pr->Si

   if(SiS_Pr->SiS_VBType & VB_SISVB) {
      SiS_Pr->Si     if(SiS_PAlwaysiS_Pr1)) seS_TV p, MA	     HT  >SiS_ < SIf((SiS_Pr->Si      if(SiS_stshorcarryS */S_Se *SiS_a& LCLCDIrst locfo) {
iS_Pr-     if(SiS_Pempbx = 10; break;
	case PaS_Pr->PanelHRE,
	SiS_Pt ModeNo, unsignetruct SiS_Private *SiS_Pr)
{
   if(St RefreshRateTableIndex)
{
reak;
	case Panel_800x600:        tempah |iS_Pr->Sckup;

   = SiS_Pr->SiS_CHe;
}

stSiS_Pr- +SiS_Se)ata = t	 if(!(DelayTime & 0x01))S_Pr->SiS_CH*******f(!(     SiSVRE  ion  1;301LV[es - Sitr = NULL;
if(SiS_Pr->Shar nonscalingmodeVData  *TVPtr  = NULL;
#ifdef SIS315H
  sho1t resinfo661;
#endif

 SetCRT2ToRAMDAC)      tempbx &= 2learmask | SetEs 1: ResIndex;= SiS_Pr->SiS_SModeIDTable[ModeIdIndex].S2esinfo661;
#ef(ModeNo <= >CModeFlagk;
#endif
	 case 90: LSISGET: SiS_Pr->iS_L	x44;

	   /* Spec }
   }
g = SiS_Pr->if(SiS_Pr->(checkcrt2mode)) {
		 if(resinfo != SIS_RI_Pr->SiS_(checkcrt2mode)) {
		 if(resinfo != S;
			 dex].Ext_RESINFO;
#ifdef SIS315H
     resinfoH (LCDdex].Ext_RESINFO;
#ifdef SIS315H
     res112;
			  1;
     iiS_LCDDaFlag & S_Pr->SiS_RVBHC {
	   mode onBette->Chiype & VB_endif
in
	    < SIS ModeNoinfo661 >= 0)00

                 C&
	 (SiS_Pr->  const struct SiS_LCDData *LCDPtr x = SISGETROMW(0
      SiS_CalcPanelLinkTiming(SiS_2COE = 0;
         break;
	 caiS_LCDISData = SiS_Pr->SiS_CHTVO  = 	       br_Pr, ModeNo, ModeIdI  = NULL;
#ifdef SIS315H
  s301info661;
#endifif
	 case 90: LVDSData = SiS_Pr->SiS_CHTVUNTSCData;        break;
	 case 91: LVDSDataLCDInfo, SiS_Pr->SiS_else if(SPRO  break;
	wFlickerMode = 0;
  SiS_Pr->Sxxt resinfo661;
#endif

 SData = SiS_Pr->SiS_CH  = NULL;
#ifdef SIS315H
  short resinfo661;
#endifk;
#endif
	 case 90: LVDSData = SiS_Pa_1;  break;
	 CData;        break;
	 caseshort t ModeNo, uSData = SiS_Pr->SiS_CHTVONTSCDatble[ModeIdIndex].St_ModeFlag;
     index = SiS_Ge
	   }
	}
   r->Sixp= SihipTiS_VBInftempbx = SiS_Pr->/* $VBType & VB_SIS30xB) {
	 * Modif(IStOrg740) delay = 0x01;
 * Modeelse* Mode 300code (CRSiS310_LCDDode Compensation_3xx301B[myindex]ection}

 SiS 300X/[M]74x}  /* got it from PCI *//[M]74xif(/* $XFree86$ *Info & SetCRT2ToLCDA
/*
 /* $SetRegANDORZ7
 * (UniverPart1Port,0x2D,0x0F,((code (<< 4) & 0xf0));
	dochiptest = false;330/[M]/[M]} * for, Z7
 * (Universal module for LinTV) {			/* -, the follo TVV3XT/V5/V8,[M]6 = GetTVPtrI[M]6Z7
 * ();T/V5/V8, Zitiali650 && Z7
 * (Universa/
/* $XdotOrgLVDS)
/*
[M]74x[GX, Z7
 *IsNotM650orLater* This podify
 * * it it unZ7
 * (UniverUseROM)are; !Z7
 * (UniverROMNew)neral * Mode/* Always use the second pointer on 650; some BIOSesV3XTPublic Liceher vestill carry old 301 data at the first loc5/550* * rsion.
 * *
 * * Thiromptr40/6ISGETROMW(0x114);		 * M*/; either ve, Z7
 * (Universa/
/* $XdotOrg$ 2LV)on.
 * *
 * * n the implWITHOUT ANY WARRANTY; wiaThomc License!WITHOU) returnU Generalcode (CRROMAddr[WITHOUT+terms
];
[FGM]X * for{ recei00/305/540/630/730TV*     SiS 315/550/[50/6ld have receiv/[M]74x[GX]ed a copy
 * *
 * * switchZ7
 * (UniverCustomT
/*
 * Mcase CUT_COMPAQ1280:, 59 Temple Place, Suite2 330, Boston, MALEVO140e 330, Boston, MAerwise, 07, USA details.
 0x02r more dets Winischhofer, VieGeneralbreakce andllowing license024the following license024rms apply:
 *
 * * Re3istribution and use in source    and binary forms,default:ion.
 * *
 * *  GNU General Public License
 * * al651301LVwith thisGeneral Purranty of
 * * MERCHANTABILITY or n; eitherce code must retain the above copyright
 * 2    notice, this l}[FGM]X/M]74x[GX]/330/[M]7
 *
 * I as published by
 * * the Free Software Foundation;yright
 *WITHOUT AapplyWITHOU* This pro	 Public License for mtails.
 * *
 * * You should have r   notice, this/* $XFree86$IF_DEF_/or  == 1r in the
 * * GNU General Public License
 * * al/or with this f not, write toith the deral Public License
 * * alongwith thislic License/* $XFree86$ */
/* $XdotOrg$ */ng disclai initializing n; eitherr written permission.
 * *
 * * THI740ong with this either veLV:2 of tributed?later bug?A PARTIC, write tons of source code must retain the above copyrigh IMPLIED WARRANTIES
ED BY THE AUTHOR ``AS IS'' AND A1Cg code (CRT2 distrib}
	]/330/[M]T/V5/V8, Z7
 *ux kEnabled GNU Generalith the& * Refhomas Winischhofer, Vienna, Austria
 *
 nse for 
er veWriteIES, IN3XT/V5ED BY THE AUTHOR ``AS IS'' ANDV/
/*
gram is free software; you can redistribute it and/or mare;on and user in the
 * *temp =; you Gebuffd X.org/XFree83d4,0x36-2005 by  >> 4 IS PROVIDED T, STR= 8ux ke/* se, x1050ITY AN(ace, L)A PARTIC BUT
 * * NOT LITHIS SOFT| * Rb0 IS PROVIDtice, thisE) ARISI6ED W of source code mWARE, EVEN IF ADVISED OFcTHE POSSIBILITY OF SUCH DA> 7ux kANY 28Y OU24OF THE which one?
 * * THIS SOFT * R35HE POSSIBIL*
 * Auth framebuffd X.org/XFree86 4.x)
 *
 * Ccode program i, write to the Fre framebuffer and X.org/XFree86 4.x)
 *
 * CopF0VE_CONFIG_H
#incustria
 *
 {S
 * * DSV3XT/V5/V8, Z7
 * (Universal module for Lininux nfig.h"
#endif

#if 1
#define SET_EMI		/* 302LV/ELV: Set EMI valuH
#include "colic Licenseree software; you can redie used CH70xx != 0neral of source code m<<=NCE OR OTHER"
#endif

#if 1
#define SET_EMI		/* 302LV/ELV: S0Fefine COMPAQ_H FOR A PARTICULAR PURPOHACK	/* Needed for Compal 1400x1050 (EMI) */
#define COMPAQ_HSiS, Inc.
#endif

}

static void
SetAntiFlicker(struct315H
Private */* $XF, unsigned short ModeNonfo(struct SiS_PrivatId*
 * )
ACK	o(struct char  ** *
 * tten p$XFreVirtualRomBa Vienno(struct SiS_Prerms
,T, Sg, un1,WITHOU=0R SERED BY THE AUTHOTVivat & (TVSetYPbPr750p|**********525p)cense for 
#endiivate <=0x13)ense terms
 * /* $XFree86$SSIS_LDTAMAG[SIS_LINUX_K].dotOtTV unsign*
 * OMPA* fo CRT2          */
/*******E****************************Ex********/

void
S
ACT, STRIapply:
 *
 * * This proACT, ST>>= 1;are m/* 0: NTSC/*****, 1: PAL, 2: Hing licACT, S1 =CT, S  HELPERas published by
 ** the Free Software Foundation;ROVIDED BY THE AChip/
/* > ANY _66se or CONTRACT, S0x2fGetOEMly:
 661SiS_Pr->Chip
#endif
void>= SIS the
 * *    documNY WARRANTY; 260     1000
#);
}

#ifdef SIS_LINUX_KERN76IED WARRAWITHOUT ANY WARRANTY; 3     	define S the author may nf SIS_LINUX_KERN33IED Wif(SiS_Pr->ChipType == XGI_20)192COMPAQ_HACK	/* Needed forWITHOUT ANY WARRANTY; wix24,0xFE);S_I2CDn;
  lic Lice_SetRegf
voidsus    if(SiT, STRI* *
 * * You shouf
voidould have FOR A PARTICULART, STRIeral Publatic unsi1[T, S]OFTWARE IS */
/T, STsus A2H
S315H
#include "oem310.h"
#endif

2x)
 *
 0A,0x8fg, un);plied rms
 0A D[6:4] USEELAYSHORT  150

stEdgeEnhanceed short	SiS_GetBIOSLCDResInfo(struct SiS_Private *iS_Pr);
#ifdef SIS_LINUX_KERNEL
static void		SiS_SetCH70xx(struct SiS_Private *SiS_Pr, unsigned short reg, unsigned char val);
#eT, STRIf
void
SiS_ly:
 *
 * * This pIGEN1; base     SiS_SetRegOR(SiS_Pr->SiS_PartHELPER: Lock < * Rock CRT2          */
/**************************************** &= 
void
SiS_UnLockCRT2(struct SiS_Private *SiS_Pr)
{
   if(SiS_Pr->ChipTypeMAddr = SiS
#endif

/********S_SetRegOR(SiS_Pr->SiS_Part1Port,0x24,0x01);
}

#ifdef SIS_LINUX_KERNEL
static
#endir->ChipType == XGI_20)
 c    return;
   else if(SiS_Pr->ChipType >= SIS_315H)
      SiS_SetRegAND(els 	}
	f
void
SiS_LockCRT2(struct SiS_Private *SiS_Pr)
{
   if(SiPart1Port,0x2F,0xFE);
   else
      SiS_SetRegAND(SiS_Pr->SiS_Part1Port,a424,0xFE);
}

/**********************************2if((SiS_Pr/
/*            HELPER: Write SR11             */
/*********************************************/

static void
SiS_S &= ANDOR(struct SiS_Private *SiS_Pries igned short DataAND, unsigned short Data3R)
{1F  if(SiS_Pr->ChipType >7:5IS_661) {
      DataANDYFilthe     DataOR  &= 0x0f;
   }
   SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x11,DataAND,DataOR);
}

/***ed short reg*****, i, j  HELPER: LocktLCDStrucx24,0x01)        */
/****************************************gned ch
void
SiSACK	/* Needed 2(struct SiS_Private *SiS_Pr)
{
   if(SiS_Pr->ChipTypesigned short
GetL     return;
   else if(SiS_Pr->*******H)
      SiS_SetRegOR(SiS_Pr->SiS_Part
#endif

/**********************  SiJ)GeneraT, STRImptr =   Si-J2 ofs(SiS be us
 *
 * If distributeTMDS is unreliPALMr FITT, STRI3ptr = PAL-Mhe BIOS doesn't know about.
    * ExceptionN If the BIOS4has betterN be us, Z7
 * (Universal module for LinHiVision)due to the varihe BIOS  panels the B,
 * * DATA, OR PROFITS; OR BUSIN ANY EXPREivatefor(i= ser, j=0; i/Unl38; i++, j++fine COMPAL_HACK	/* NeaAND, unsigned short Dai,eral Publgned ch2NDOR(struct S[j]pe & VB_SISLB_SISLVDS) 4nelS/Unl4AelSelfDetected))) {
      romptr = SISGETROMW(0x102);
      romptr += ((SiS_GetReg(SiS_Pr->SiS_P3d4,0xACK	/* Needed ISLVDS) || (!SiS_Pr->PanelSelfDetected))) {
      romptr = SISGETROMW(0x102);
      romptr += ((SANDOR(struct S*************/
/61) {
      DataANDPhaseIncned short	SiS_GetBIOSLCDResInfo(struct SiS_Private *SiS_Pr);
#ifdef SIS_LINUX_KERNEL
static void		SiS_SetCH70xx(struct SiS_Private *SiS_Pr, unsigned short reg, unsi,j,resinfo char val);_Pr, unsignint  lrominindex =  Free Softwaret PWD */
#endif

#def*         HELariaty of uted n],
 nlater, and already secheckSetGroup2 that does not supporTMDS is unreliable
 GOODS OR SERhis list of f SIS_LINUX_KERNEL
st||  */
/******* Foundae & VB_S>SiS_Vd
SiS_LockCRT2(st_2_OLDned short005 bfff  if(Si  checksus 2  if(SiISLVj=0, DS) |1S_Pr->Pa4elSelfDetected))) {
      romptr = SISGETROMW(0x102);
      _TVgned [  check+ r->SiS_P3d4,0x36)nse for r->Virt better , panel 	 checkmask |= SupportRAMDAC2;
	 if(SiS_Pr->ChipType >= SIS_315H) {
	  ******ion: |f machines *         HELPER: Lock/Unlocke & VB_SRTI + (    */
/*********************************St_Resl mo**************/

s & DontExpandLCD) && ( *SiS_Pr)
{
   if(SiS_Pr->Ext_RESINFO SetCRT2T return;
   else if(SiS_Pr->Chip******/

# GraphicsegOR(CRT2TText,x36)2R(SiSToHiVision,0x3* 3 Suppoeckmask |  4->SiS_PoHiVisio 5->SiS_Peckmn;

 hat does not suppor by
 * *if(modeiITHOUT ANY WARRANTY; wi6COMPAQ_Hrt,0x2F,0xFE);
   else
      SiS_SetRegAND(SiS_Pr->SiS_Part1Port,0 checkmas, Inc.
_ROMNew) &&
      ((SiS_Pr->SiS_VBType & VB_S***************************nowlert,0x2F,0xFE);
   else
      SiS_SS_315H)
      SiS_SetRegAN19nowledgehis list of condsal module fInSlaveivat * the Free SoftwareTMDS is unreliTVSimu}

  tion; eitEO|SetCRT2ToSCART)) {

	 cheSS OR
 0x2F,0xFE);
   else
      SiS_SetRegANDype & VB_SIS30xBLV) {
	    checkmask |=the abovedefine Si|= Sup        HELPER: Wr You sho= UCH DA<< x24,0xFE)SRAMDAC202) {
		  checkmask |= SupportRAMDAC2_202;
	       }
	    }
	 }

      } * *
 * * You shou************/
/*           AromindexT, ST%e & VB_SIType >= SIS_3Index[RR*****  SiegORSiS_Pr-SiS_Part1P  }

   /* Look backwards in table for matching C & VB_SISVB) {

   ``AS IS'' AND ANY EX)S_VBI    romptr = SISGETROMW(0x102);
      romptrgned shorS_AdjustCRT2Rate(struct S)
 * forhis VB_SISVB) {

      if(SiS_
      }

  _VBTye {	/* LVDS */

      if(SiS_Pr->SiS_IF
#ifdef SIS315H
#inclu-section of the table from the beginning
  iS_GetReg(SiS_Pr->SiS_P3d)
 * fox[RRTI + (*i)].ModeID != modeid) break;
      infoflag = SiS_Pr->S_AdjustCRT2Rate(struct SiS_PrirtRAMDAC2_135;
UTHOR ``AS IS'' AND ANY EXP the Free Softwaret DDC)
    * use the BIOS d,0x24,0x01);
lse {	/* LVDS */

      Pr->SiS_VType & V*******/
d short
SiS_G********re;       m>DStruc
      if((*i) ( & DontExX_KERNRI_64 * S0_VBTS_VBIo, unsigned short M800x60US_HACthe whole mode-section of the table f {
	*
 *1SiS_VBI unsigned short modeflag,index,temp,2,5 by ndex;
   static const unsigned short LCDRe3resh5ndex[] = {
		0x00, 0x00, 0x01, 0x01,
		0x01(INC7fowledOS doesn', unsigned short Mnon-x76NG Ii;
   unsigned short modeflag,index,temp,back1eindex;
   static const unsigned short LCDRefres8bndex[] = {
		0x00, 0x00, 0x01, 0x01,
		0x01, 0x0turn 0;

   if(ModeNo <= 0x13) {
      modefx01, turn fo & SetCRT2TELAYSHORT  150

st*     SiS(strd short	SiS_GetBIOSLCDResInfo(struct SiS_Private *ICULAR PURPOSE _Pr, unsigned shoSIS_LINUX_Knfo(struct SiS_PrRTIERNEL
Pr, unsigned shocode (CRT,70xx != 0)C202(SiS_PlcdpdcSiS_VBT2ToTV) {
	 iid		SiS_SetCH70xx(struct SiS_Private *SiS  if & VB_SISVB) {

      if(S(#endif

#de |le for Linux _SelectCRT2RateA_SelectCRT2RRAMDAC)dex[RRTI         HELTI +1. New/***: VGA2|= SuLCD/ bac-Pass1:1 be use/* (If a cion,  m****i 2 ofd,  Programis asion 2set; hence we do this:
 * *tReg(Sipe & VB_SIS30xBLV) {
	     *******************l module for Linndex;

 			||CK	/* Need50p;
	       }
	    }
	 SiS_Pr->Sate) & 0x0F;
   bacrivati;
 ToLCD) {
	   LCDl modulLCD Prog1***********)--) {
     2ries by SiSrtTV1024;
	 Usetion, }

   n; eithromindex];
     CSRClock if(!(SiS_P 0x01, 0x0*SiS_Pr, unsigRTICULAR PURPOSromindex];
 GetVCLK2Pand/or ot,ivate *     if(SiS_ Set2H 1024x768 (61_2(struct SiS_Priva = tDatawith th.CLOCKex[SiS_GetBI
	g(SiSiS_VBT 25)DA)) {
	    if(!(SiS_Prcheckma(S_Pr->S/S_IF_-rse <<
   if(SiS_Pno mo* *
 * *0x5b]     Index); you can redistpandLCD) index = ndex;
_SelectCRT2Rateation; eith   if++iS_V if(EO|SetCRT2ToSCART)) {

0if((SiS_Pe details.
 * *
 * * You should have f(!(SiS_Pr->SiS_VBTyiS_Pr->SiS_EModeIDTable[ModeIdIndex].REFindex;RTICULAR PURPOS framebuffer and X.org/XFree86 4.x)
 *
 *dreshIight (C) ****      0f Thole[ModeIdIndex].Ext_VESAID == 0x105) ||
	     (SiS_P3501, 0ight (C) deIdI & Set7dex].Ext_VESAR A PARTICULAR PURPOSex].Ext_VESAID == 0x105) ||
	     (SiS_Pr->Si0;
	 }
    << 3-2005 by Thom68 (EMI) */

#include "init301.h"

#ifdef SIS0,0xb;
	 }
      }
   }

 6 i = 0;
   do ].Ext_VESAoLCD | Set00
#definY, OR /* 2. Oldif(SiS_Pr->SiS_SetFlag & Pro gramminetCRT2ToLCD) {
pe & VB_NoLCD))code (CRT2 CE OR SLCDResInfo(SiSyptr = &Ry:
 *
 * * Re {
      iftemp = SiS_Pr->SiS_Recode (CRToLCD) {
	    ef*
 * [RTI      PDCIGENCex].Ex ADVISED ht (C) 2008programnfo & SetCRT2ToTV) {
	 XGI_2IED W;
     code (CRT2 606x].Ext_V* 301/302LV: Set PWD */
#endif

#define
	T2ToRAMDAC)) eMode) PROVIDED BY THE AUTHOXGIetCRT2TGeneraerms
 * apply:
 *
 * * This proe(SiS_PeNo,H)
      SiS_SetRegAND5_IF_DEF_CH7S_RefIndex[RRT* *
 * * You should havodeIdInd SetCRT	ith the) i++;
      }
  Genera if(}))) does not support DDC)
    * use the BIOS da  ModeNofo & SetCRT2ToTV) {=r->SiS4ware;C2_135;
	   Rev/

   = * Red disclaimer.code (-  backup_iRefInRE CRT74x[GX]/330/Part1Port,0x2F,0xFE);
   else
     LIED Wag & ProgrammingCRT2) && (!(SiS_Pr->SiS_VBInfo & DisableCRT2Display)->SiS_RefIndex[RRbackup_i = i;
)
{
   u/* TODO (even_Prilyo & SetCRia
 *
 * If distributed as part of the Linux ModeNo i/* 3.ing license t_Pr, ModeNo,LockCRT2(struct SiS_PrivateT2ToLCD) {
	    if(SiS_Pr->SiS_CH70xx != 0) {
	 if(SiS_Pr0>SiS_VED BY THE AUTHOR ``AS IS'' UMC) |= Support1 & VB_SIS_Pr->ChipType >= SIS_315H) {
      if(!(SiR A PARTICULAR PUR SetInSlaveModf(SiS_Pr->S>= Sicode (CRTo & SetInSetInSlaveMode >> 8);
   SiS_SetRegA) index = backupindex = 0;
	 }
	SiS_P3d4,0x34.tCRT,tCRTA (for n  if(S only LV|= SunonFFFF);

 o & SetCR******iS_Pr->SiS_VBInfo11)) {
e ASPanelation, 
	 iftemp = SiS	 i = backuGetLCDS shorpportRAM GNU Gene) lay)))    }
   }

[RRTI + i - 1].E*****************? 14 :**/


	,0x3Forfine S(= Suany times TMDS), the TY ANmust know aboun the correct valueA PARTtails.
 * *
 * * You shoup) return tr+ 1];baseate)ddr = SiS_Pr) i+VirtualRomBase;
   unsigned ]    }
 temp,teAx34)) {
	 t, write to the Freede) MDS:	 if our own, siS_PrTY ANhas no ideaA PARTT2) ThisB_SIdone
 * >=6612) &&< ((SiS_<
	 tis calling_VBIn2) && 233]OF LIArsion.
 * *
 & VB_SISVB) {

 nfo & (SetCRT2ToLCD |********oftware
 * * FoundaGetReg(SiS
/*******Templ>SiS_Px00, 0x0: *SiS_Pr)
{
 008;inary forms,*/
/*         on 720R: DELAY FUNCTIO4S          */
/****************68ms apploid
SiS_DDC2Delay(s_2:************************/

void
SiS_DDC2Dela8, the fowhile (delaytime-- > unsigned int delaytime)
d4,0Verified& temtime-- >A PARTICUhile (delaytime--54*************************ool
SIXMEned(SIS315H)
static void
ut
 **SiS_Pr)
{
1e************/

void
SiS_DDC2WAY OUT 
{
   SiS_DD**************/

void
SiS_DDC26AY O20

#ifdef SIS31400static void
SiS_LongDelay(s delaf

#ifdef SIS312Delay(SiS_Pr, deistributions of source RAMDAC2_135;
>SiS_XResSiS_non-ivate 
#endif

#if Yefined(0x00 disclaimer.
  DELAY FUNCTIONSsclaimer.tice, this list of 
#if defindors2Indere; you can (SIS315H)
dorsIS30void
SiS_ShortDelay(strucC2DeliS_Pr, 6623);
  *SiS_Pr, unsigned short delayed(S40 while(delay--) {
      Sed(SI5ckup_i;
   = (SiS_Pr->SiS_VB****_Private *SiS_Pr, unsigned short delayed(SbackSiS_Pr, unsigned short Dela2ackup_i;
   = (SiS_Pr->SiS_VBInTHE POSSIBItic void
SiS_d
SiS_ShortDelay(struct}
#enclaimer.         */
iS_ModeTypeRT2 I/* Override by detected or2 ofr-DAC2*ROMAversi3d4,0xbut2) && if,& temany lreason,r->Scan't) {
d *ROMAd    Xatereturn true;
     Z7
 * (Universal module for Linux iS_Pr->VirtualRDCe AS-f

/**s of source code must r & 0x10))005 1(SiS_Pr-00
#define SiS_VBType & VB_NoLCD)		 index = 0;
	 ux ker4,0x18) & 0x10)A) PanelID = 0x12;
      }
     {
	 Delay = 3;= Pane*****8< SiS_Pr->SiS
{
   unsignedSetCRT2ToLCD) {
	   sal module for Linux kernedef SIS300
s>= Sif(!(DelaiS_RefIndex[RRTI + i].ModeID != ModeNo) break;
      temp = SiS_Pr->SiS_x768 (EMI) */

#include "init301.h"

#ifdef SIS ModeTypeMask;
      if(temp < SiSACK	/* Needed fex].Ext_VESAID == 0x105) ||
	     (SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_V== 0x107) ) {
	    if(backupindex <= 1) RRTI++;
	 }
      }
   }

   i = 0;     modeflag = SiS_or LSyncDithe2(strd short	SiS_GetBIOSLCDResInfo(struct SiS_Private *SiS_Pr);
#ifdef  SetCRT2ToTV) {
	 if(mode + (flag   if(ModeNo < 0x14),0x01);
 se;
   unsigned short temp,temp1;

   if(SiS_Pr->SiS_UseROM) {
            myptr = &ROMAddr[1);
}

	 (Siemp) indexRegByteY, OR TORT
 * *ca+x24,0xFE)nSlaveMode >> 8);
 pe & VB_NoLCD)) {x1000);
	 } else {
	    CDRefl moF(SiS_Pr-(struct SiS_Private * /* ||
	 (SiS_Pr->S1].Ext_InfoFlag;
	 if(S_CustomT == CUT_
	    SiS_D }
   }
   return false;
}
#endif

/**80) ||
	 (SiS_Pr->SiS_CuABILITY, OR TORT
 * * (INCL7)_PrivNo longer check D5turn true;ries, LVDS} else {* * Nwini&& (!(SiS_Pr->SiS_VBInfo & DisableCRT2DiS_P3cCH70xx == 0T, STRICUT_CLEVO1>>GE.
|IS31c if(!(SiS_Pr->SiS_VBInfo &nfo & (SetCRTRGB18BiHER IRefInT, ST^)
{
   uns   ||
	 (SiS_Pr->SivatS_LINUX_ivat24Bppdata asED OF10_Pr->SSiS_Pr->SiS_RefIndex[RRTI + i].ModeID != ort Data1a,0xe0  if(Sid23b(struct SiS_Private *T, STRI0x3, Delay=0;
#_CustomT == CUT_COMPAQ1280) {
	       e {
	    2, Delay=0;
#PanelID ||
	 (SiS_Pr-x768 (EMI) */

#include "init301.h"

#ifdef SI19eak;
 {
	       Delelse {
	   SiS_Pr->SiS_CustomT == CUT_COMPAQ1280) {
	       DelayInf;
	    } else {
	       DelayIndex = PanelID >>84;
	    }
	    if((DelayTime >= 2) && ((PanelID 0];
		} e= 1)7
		  Delay = Si)) {
	  vate *SiS_Pr, unsigSiS_Parmsifdef SIS315H

      if((SiS_PtCRT2ToTV) {
	 iid		SiS_SetCH70xx(struct SiS_Private *SiS_PPr, unsigned shoi = baromind1romindtic portTV1024;
	    if(SiS_Pr-(it and/or  |XdotOrg$ *CIDTable[Mod framebuffer d X.org/XFree86 44x)
 *
 *(INC00x01Delay = (ED BY THE AUTHOR ``AS IS'' ANDOF LIAPr->SiS_VBT DelayInine HL) Panel & 0x10)) Pa framebuffer and X.org/XFree86 4ayIndex = SiSfc else	    Delay {
			Delayeg(SiS_Pr->SiS_P3d4,0x360xBLV) {
	r->SiS_VBTy;
	 temp1 = SISGETROMW(0x23b);
	 if(tempCH70xx == 0)>SiS_P3d4,0x36) >> 4;
	 if(!(DelayTime & 0x
#endif
void
SPr->SiS_UseROM)*******3	    if(SilayIndex 2	    fte *SiS_01)) {
	    Delay = SiS_Pr-Generalf
void* * Ned short S_Pr, unsigne the folove coelDelayTbl[DelayIndex].timer[0];
	 } else {
Pr, usigned < SiS_Pr->SiS INFO in CR34        */
/**********x1b) & 0x10)) Pa void
SiS_PanelDelayLoop(sshort  }
   }
*****].Ext_VESAID == 0x107) ) {
	    if(backupindeayIndex 0->Sib
		  Dpindex)
{
   unsignednsigned short)ROMiS_OEM310Settinged short	SiS_GetBIOSLCDResInfo(struct SiS_Private *SiS_Pr);
#ifdef SIS_LINUX_K    ||
	 (SiS_Pr-> SetCRT2To0xf7;
	 if(!(SiS0xBLV) {re; you can redistribute it and/or modifx768 (ES_Pr->SiS_EModeCDResInfivate *SiS_GetReg(SiSx17)  && (!(SiS_Pr->SiS_VBInfo & Disap,temp1;

   if(SiS_Pr->SiS_UseROM>SiS_Panel

   } else {

#ifde --watchdog);
  5535;
   whil

#if dr[0x17e];
		 uct SiS_PrivateelDela) Delay = (unsiS_Pr->SiS_E
	    }
	 }
  etReg(SiS_Pr->S* DATA, OR PROFITS; OR BUSINESS re; you can redist   if(SiS_Pr->SiS_VBIPr->SiS_P3datic unsigne	    }
	 }
      } else {SiS_Pr->Sisigned short   watchdog = 65535;
   while((!(SiS_gned cha   watchdog = 65535;
   while((!(SABLE FOR ANY DIRECT, INDIRECT,
 *tchdog);
}

#if  &= 0x0f;
     watchdog = 65535;
   while((!(SelDelayr)
{
   unsigned short661tchdog;

   if(SiS_GetReg(SiS_Pr->SiS_P3c4,0x1f) & 0xc0) r
			turn;
   if(!(SiS_GetReg(SiS_Pr->SiS_P3d4,0x17) & 0x80))  DATA, OR PROFITS; OR BUSINESS INTERRUP_P3da) & 0x08) && --watchdog);
   watchdog = 65535;
    while((!(SiS_GetRegByte(SiS_Pr->SiS_P3da) & 0x08)) && --watchdog);
}

#if defined(SIS300) || defined(SIS315H)
static void
SiS_WaitRetrace2(struct SiS_Private S_SetReg(SiS_Pr->SiS_P3d4,0x34,ModeNo);
   temp1 = (SiSiS_GetReg(SiS_Pr->Sihdog);
   watchdog =ex].Ext_VESA& --watchdog);
}
#ruct SiS_Private *SiS_Pr)
{
   unsi-watchdog);
   watcruct SiS_Private *SiS_Pr)
{
   uVBRetrace(struct SiS_Private *SiS_Pr)
{
   if(if(SiS_Pr->ChipType < SISruct SiS_Private *SiS_Pr)
{
   u)
{
   uns
#endif ol
SinalizeLCD
 * 36) >fak;
   sSISVB)or L registers& temthe very pSiS_SiS_V.  } Ifr->Shave a backup ifue;
E GOO08)) cof(SiS of it; o{

#wiInfo*r->SDAC2the ;
   }
} accord;
	 io mostlater v. Howevey(SihisgWaifuncit wilooks qu; LOSifferencheck_VBIykmask |so you bettergWaipray that }
     }
     te..    /
{
   unsigned sheak;
      ed short	SiS_GetBIOSLCDResInfo(struct SiS_Private *SiS_Pr);
#ifdef SIS_LINUX_KERNEL
static voSiS_PrT, Scl;
   ch;
   b***/

bifdef Seg, unaeg, unID;

   if(SiS_ShortTI + (*iS_P (SiS_ype & VB_SISVB) {

   ribute it and/or modoLCD | SetT2ToLCD) {
	    if(SiS_        HELPER!(SiS_GetRegByte(SiS_Pr->SiS_P3da) & 0x08)) && --watchdog);
01)) {
	    Delay = SiS_Pr->SiS_PanlDelayTbl[DelayIndex].timer[0];
	 } else {
	    Delay = SiS_Pr->SiS_PaSISLVDS) ||  1 << ((SiS_GetReg(SiS_=r->SiS_P3d4,0x36 0xb0) return true;
 pe & VB_NoLCD))        HELoftware
 * * Foundation, Inc.,9 Temple Place, Suite 3, Boston, MA 02111-1307,SA
 *
 * Otherwise, thfollowing license termsCRT2ToLCD | SetCRT2T         myptr = &ROMAddr[ & DontExpandLCD) && (SiS_Pr->SiS_LCDInfo & LCDPass11)) {
	     SiS_Plse {
	pandLCD) && (SiS_Pr->SiS_LCDInfo & LCDPassivatstomT ==       if(modeid == 0x2e) checkmask |= Support64048060Hz;
	       }
	    }
	 urn true;
      }
   }
#end|= Support64048060Hz;
	       iS_IsVAMode(sf(SiS_Gree softwr)
{
   if(S Free ABILITY, OR TORT
 * * (I 0x5f-2005 by T Needed for In
 * * Foundation, I0) &ation, are pe0
   };

   /* Do [DelayIndex].timer[0];
		} eeaitRx24,, 0x01, 

#ifdef SIS315H
#incluIsVAorLCD(struct SiS_Private *SiS3iS_Pr->SiS_Pf(SiS_GetReg(SiS_Pr->Si false;
}

#ifdef SIS315H
st->SiS_CustomT == CUT_CO3d4,0x30) & 0x20)x00, 0x00
  34] == 0x34Maybe alllse brsITNES= i;
      if(!(SiS Delay = SiS_Pr->SiS_Pane      if(SiS_GetReg(SiS_Pr->SiS_P3c4,0x13) & 0x20)}
   wledge) break;
    turn true;
   return false;
}
#endif

static boo_Priv while((!(SiS_GetRegByte(SiS_Pr->SiS_P3da) & 0x08)) && --watchdog);
}

ualLink(struct SiS_Private *SiS_Pr)
{
#ifdef SI_VBInfo & SetCRf((SiS_CRT2IsLCD either ve(SiS_Pr->ChipType >= SIS_315HPr)) ||
         (SiS_IsVAMode(SiS_Pr))) {
	 if(SiS_Pr->SiS_LCLoop; i++, Z7
 * (Universal module for Linux kernelPER: WAIT-ch PanelID = SiS_GetReg(SiS_Pr->SiS_6LIGENCE General Pu5H
stati== Siisclaimer.
 Used by permission.
 *
 */

#ifdef 18*SiS_Pr)
RTI + (*i)].ModeID != modeid) break;4) returnb*
 *1, 0x01,SiS_Pr->SiS_Part1Port,0x13) & 0x04) returnPr->SIndex[] SiS_Pr->SiS_Part1Port,0x13) & 0x04) return->Si1turn 0;
 above copy_CH70se for mPr)) return true;
   return falSiS_CRT2IsLCD(struct SiS_Private *SiS_Pr)
{
   if(SiS_Pr->C(SiS_GetReg(SiS_Pr->SiS_Part2Port,0x0art1Port,0x00) & 0x80)) {
	 SiEMI0
   };

   /* Do NOT check for UseayIndex == 1)
   i#ifdef SET_EMIi;
   unsigned ll bridges */

	 DelayIndex 3 Mod0els #endifnsigned short flag;

   if(SiS_Pr->ChipTodeFl1SiS_Pr->SiS_Part1Port,0x2F,0xFuct SiS_Private *SiS_Pr)
 on non-true;
   }
#endif
   ref((SiS_CRT2IsLCD(SiS_Pr)) |f(SiS_Pr-ACER2) &&e & VB_SISYPBPR)|
         (SiS_IsVAMode(SiS_Pr))) {
	 if(SiS_Pr->SiS_LCDI 4;
      if5H
static bool
SiS_LCDAEnabled(struct SiS_PrivateV5/V8, Z7
 * (Universal module for Linux kernelualLink(struct SiS_Private *SiS_Pr)
WAY OUT 0
   };

   /* Do NOT check for Use4) returnfe & >SiS_ia
 *
 * If distribute_GetReg(SiS_Pr->SiS_Part2Port,0x00) & 0{
   if(Sit SiS_def SIS31->SiS_Part1Port,0x13) & 0x04) return true;
   returnlse;
}
#endif

#ifdef SIS315H
static bool
SiS_WeHaveklightCtrl(struct SiS_Private *SiS_Pr)
{
   if((SiS_>ChipType >= SIS_315H) && (SiS_Pr->ChipType < SISurn true;
   }
  B   temre; you can _Pr)
{iS_Prf(Signed shS_Pr->ChipType >= SIS_315H) {
      /* Scart = 4 Delay = short fitho
#ifdef SIS315H
static bool
SiS_IsTVOrYPbPrO5tReg(SiS_Pr->SiS_leCHScart) return true;
   }
   return false;
}6tReg(SiS_Pr->SiS_>SiS_VBIef SIS315H
static bool
SiS_IsTVOrYPbPrO7tReg(SiS_Pr->SiS_73d4,0x30);
      if(flag & SetCRT2ToTV)       8tReg(SiS_Pr->SiS_       STO SIS315H
static bool
SiS_IsTVOrYPbPrO9tReg(SiS_Pr->SiS_93d4,0x30);
      if(flag & SetCRT2ToTV)       atReg(SiS_Pr->SiS_GNU Generallse;
}
#endif

#ifdef SIS315H
static bReg(SiS_Pr->SiS_pe < SIS_66klightCtrl(struct SiS_Private *SiS_Pr)Reg(SiS_Pr->SiS_nowleiS_Pr->ChipType >= SIS_315H) && (SiS_Pr->ChiReg(SiS_Pr->SiS_d(struct     if(Si }
   }
   return false;DontExpandodeIDTabased.10.8wA PARTICULApe >= SIS_315H) {
      flag = SiS_Get0x9   if((SiS_         myptr = &ROMReg(SiS_Pr->SiS_Part1Port,0x13) & 0x04) return tru1iS_LC*****01,
		0x00,  }

   , unsigned 2 >= 0xb0) r		eg(SiS_Pr->SiS_P3d4,0x30);
      if(flag &     RT2ToLCD) return S_VBTy   }
   retuignense for more dep; i++)pType >= SIS_315H) {
      /* Scart = 0x04 */
      if2ToLnfo(SiS_Pr)];
	re; 1,
		0x00, 0x00, 0x00, 0x00_GetReg(SiS_Pr->SiS_Part1Port,0x13) & 0x04) return true;
 dex--;
10.7u****#if 0= SiS_GetRetic bo = 806ptr = 0x326 */ut e/*
SiS_Vs dierlater versi		 return--Priva */
/******bx      i}
#endif

static bool
SiS_HaveBridge(strub		  Dela*SiS_Pr)
{_Pr)
bx****8ModeIdI theendif

statiVDS[DelayIndex].timer[0];
		} e->SiS8		  Dela  /* Check  above cop    if(SiS_Pr->SiS_VBInfo*
 * Mode in      myptn trueendif

static bool
SiS_HaveBridge(struct S7Indexendif

static bool
SiS_HaveBridge(strulse f0x01,rt flag;

   if(SiS_HaveBridge(SiS_Pr))  * Sn false;
}

static bool
SiS_BridgeInSlavemhipTy;
   return>SiS_IF_DCRT2T true;
  & HalfDCLKReg(Siendif

static bool
SiS_HaveBridge(struretu2flag == 0x10)) return true;
      }
   }
  RTI+#endifendif

static bool
SiS_HaveBridge(stru6 rettruct SiS_Private *SiS_Pr)
{
   unsigned sh7e == SISlse {
	flag &= 0x50;
	if((flag == 0x40) || 4nowle== 0x10)) return true;
      }
   }
   retdpurposern true;
   if(1_Priva************5H
statelGPIO(stS315H)
se 33PIO(->SiS_Part1Port,0x13) & 0x04) return tru91, 0xshort myvbinfo)
{
   unsigned int   acpifdef SIS300hort myvbinfo)
{
   unsigned int   acpi= 1)g == 0turn;

#ifdef SIS_LINUX_KERNEL
   acpibaode(;
   unsigned short temp;

   if(!(SiS_Pr->SiSPr->struct hort myvbinfo)
{
   unsigned int   acpiort flag1;basenary for return Templ07, base &= 0xFFFF;
   if(!acpibase) return;base;
   unsigned short temp;

   if(!(SiS_Pr->SiSode(struct SiS_GetRegShort((acpibas3 + 0x3c));	/* ACPI register 0x3c: GP Event 
   acpibase =_GetRegShort(}gSho>SiS_IF_D1)) {
   r->SiS_Part1Poatic
#endif
vocl)
{
   uatic bool
SiS_LCDAEnabled(st short DataOiS_LCPin Lev* * NOT LI (low/h* * N74;
	 (low/h>= Sate def SIhigh) */
   temp &= 0xFEFF;
   if(!(myvP3d4,return faS_GetRh    }
 | returliS_Pr->SiS_TVMbool
SiS_IsChScart(struct SiS_Private *SiS_01,
		0x00, 0x00, 0x00, 0x00
nsig2ToLCD) return true;
      flag = SiS_G0x20)) return  */
/********etstomSetCRTVESATiming0x20)) retuSiS_Priv = 700;
 == CUT_COMPAQ12 return faiS_Pr)
Reg(o = !(Delasinfo = 0;
******************GAVDE < = SiSlGPIO(stl
SiS_nfo 68 - (SetCRT2ToLCD ModeegShort(S_Pr->SetRegS  if(
SiS_= 2)) w;0x13) 6s:S hasuct Si true;
   }
   retu ModeId= dIndeMask;

   if((M> 0x13) && (!SiS_Pr-><_Pr-;   i1;CustomMode= SiS_G-   infaxeFlagibase + 0x3a));	TUTE GOODS OR edge (su)
{
   unsigned sho unsigned short modeflag,index,temp0);
   RegShort(SiS_
   unsigned  SiS & In);
}
#ecdif
igned short DataAND, unsigned short DataOback8  {
	       De {
        /* Che br =5H
	if(SiS_Pr->Cort m 300 O.E.M.15H
	if(SiS_Pr->Ch= USE,_650) {
SiS_0LAYSHORT  150

stOEM,
 *ata2ar *)&SiS_LCDStruct661[idx];
      }
      romindex = SISGETROMW(0x100);
     0x2  ||
	 (SiS_Pr->efTab**************************crt2crtcAC20 true;
 , 51/[M]6deID;

   if(Si == SIS_330) CUTt iidx;
         myptr = &ROMAddr[ true;
     }
   }
#endif
   return false;
}

bool
SiS_IsVAMode((acpeg(SiS_iS_SetRegAND(SiS_Pr->SiS_P3d4,0x38,0xfc);
	 or LCRTC**************/

s */
		 SiS_SetRegAND(Si) {
      flag = SiS_GetReg(SiS_Pr->SiS_P3  if(IS_SIS650) {
		 if(Sixt_InfoFla);
	            (SiS_GetReg(Sf(Sif(IS_SIS6* * N3findex = 0, reg = 0,  false;
}

#ifdBARCc bool
SiS_IsD series, all bridges */

	 Del4) return, 0xdGetReg true;
   return false;
}
#endif

s	     36E.
 *
 * AiftReg(SiS_Pr->SiS_P3d40x31) & o theT/V5/V8, Z7
 * (Univerbx, temp;
 ow{
	  eststrue;
   }
#ISLVDS)iS_P7elSelDTable[ModeIdInif(barco_p1651/[M]66[f(IS_SIS][i][0]S_Pr->ChipType >= SISer and X.org/XFree86 4.x)
 *d
SiS_ShortD_GetReg(SiS_ New CR layout */
	      tempbx &       = SiS_GS_Pr->SiS_P3d4,0x38) & 0x04) {
		 2emp Short((a New CR layout */
	      tempbx1->Sie copyright
 * *bPr(struct SiS_Phigh) */
   temp &= 0xFEFF;
  .x)
 *
 
   iOTHERWISE) AR }
   }
atic
#endif
voB_SISYPBPR) {
		    tempbx |= SetCRT2TiS_Prtatic void
SiS_Pg(SiS_Pr->SiS_Part1Port,0x13) & 0x04) return t
	tempbx |= tempax;

YSHORT o(struct SiS_P
******CRT2thar *)&SiS_LCDStruct661[idx];
esetstomERNEL
static void		SiS_SetCH70xx(struct SiS_Private *SiS_Pr, unsigned sho= SiS_=0*i)].ModeID;
YSHORT const
static void		S   if(tAMAG300[] =);

eturreturSVGA2)) {
	   tempbx &= ~(SetCR VB_SISVGA2)) {
	   tempbx &= ~(SetCRT2ToSiS_f  /* SIS315H */

        if(!(SiS_Pr->SiS63BType & VB_SISVGA2)) {
	   tempbx &= ~(SetCRT2ToRAMDAC);
	}

	if(SiS_Pr->SiS_VBType & VB_SISVB) return true;
 *******/

st     dex);
(SiS_= SiS_GetT LIABILITY, OR TORT
 * * (INCLUDING NOT LMode)) {
      resin SiS_Private *SiS_P
   unsi if(S7hipTyp= 0;

   i & VB_SCRT2ToLCD) returnbx, temp;
   unsigned sh->SiS_IF_+s A2H 102ualLink(struct SiS_Private *SiS_Pr)
{
#ifdef SIS315H
 tReg(SiS_Pr->SiS_Pue;
      flag = SiS_ |
		      3hipTypPr->Si  else
      SiS_SetR        SetCRT= 0;
	 }
 235   */
/*** = 0x12;
    SetCRT2ToYPbtic bool
Si/
/*dge) returnf((Delay
		 iSiS_SetFlr->ChipType == XGI_20)
5
SiS_WeHav        HEL= SiS_Get* *
 * * You shou(SiS_Pr->SiS_IF_DEF_CH7ARRANTIES* for SiS 3= SiS_GetSiS_Pr->SiS_VBT   }
	}

	if(!(tempbx & tePr, 6623);
   }
= SiS_Ge if(FFg;

   iB) {
FFiS_Pr->SiS_V     = SiS_G11     	 SetCRT2ToAVIDEO |
		        SetCRT2ToSVIDEO |
		  Reg(SiS_Pr-Pr->Si#endif

#if 1 |
		 		 temp = S i;
      if(!(SiS_AdjLCDA   |
		      RT2ToTV | SetCRT2ToLCD;
	      } e {
		 temp = SetCRT2ToLCD;
	      }
	   }
	}

	if(!(tempbx & temp)* for   * due tsinfoned sh;
   do {
      if(SiS_Pr- = DisableCRT2DisplaCRT2   tempbx = 0;
	}

	if(SiS_Pr->Si   }
	   pe & VB_SISVB) {

	   unsigned short clea( DriverMod/

#ifdef , Z7
 * (Universal module f
      }

   |
		       SetCRT2ToSCART)       te		        SetCRT2ToCHYPbPr;
	  Reg(SiS_Pr-   unsibx & S else {
		 t	      if(SiS_Pr->SiS_IF_DEF_CH7     hipType >= SIS_315H) {
bx &= (clearmask | SetCRT2ToSCART);
	   tempbx & SetCRT2ToHiVision)    tempbx &= (clearmasSiS_P3d4CRT2ToHiVision)ELAYSHORT  150

st if(Modode ar *)&SiS_LCDStruct661[idx];
      }
      romindex = SISGETROMW(0x100);
      if(romindex) id		SiS_SetCH70xx(struct SiS_Private *SiS_Pr, unsigned short reg, unschar val);
#endif

/********SiS_Private *SiS_Pr) return true;
  750|SetCRT2ToAVIDEO|SetCRT2ToSVIDCRT2TetCRT2ToLCD;7*******f

/) break;
    |SetSimuScanMode);
	   }
rn false;
}

				SetSimuScanMode );

	  4turnetCRT2ToLCTh*        SiS 315/550 tSimu_Priuld b(struc *SiS_Pr)
{
 ->SiSsn;

  here. Unfortunatep1 =various_GetReverIOS  4) &'tgrame
   unn;

  a uniform way us;
	 eg. 0x12byteime 20, SiS_ of VBWait(Siy) {
	 ard coded SiS_Ps (S_Getime &|= S18F_DE	 if(SiS_1().n;

  Thusr->SiSe)))truct ) >> ^= 04,0x3 sel>SiS_P     if(SpdcP3d4iSISVWait(sSiS_VBLon_Pr->SiS_Pa    idbpp S/CHROhat does not su10)) PanelIsDualEdge(s return;
       if(SCDResInfInde  return false;
}

bool
SiS_*i)--) {
     Panel_UnLockCRT2(struct SiS_PrivateVB****************************,
 *    t romindex = 0, reg =f SIS_LIN!T2ToHiVision f((Delay     HELPE	|= SupportLCD;
 *   }
2Mode)) Type == XGI_2lic Licmode)) {
	+ay =S_VBT_Pr->SiSnelDelayLoop(stOMPAQ_HACK	/* Neer(struct SiS_Private *SiS_Pr)
ESS INdeflag
static void
00hort,
 *    iS_GetReg(SiS_r)
{
   if(SiS_IsVAModesEnabled(SiS_Pr)) {
		 if3NDOR(struct SiS_P1000
#define SietLCDStructPtr661 else
      SiS_SetRegOR(etCRT2ToLCD;
	      }
if(SiS_Gflag & CRT2sk |= SupportLCD;
 crt2modRT2ToTV | SetCRT2ToLCD;!= SIS_RI_sk |= Support {
		         */
/**************riverMode)) {dgeIsEnabled(SiS_Pr)) {
		 if5!(tempbx & DriverMPAQ_HACK	/* Needed for InV 	   |
				SwitchCRT2{
	      if(tetCRT2ToLCD49] |

	if(!(tempb4aOMAddr[0x00) & 0lay)) {
	   if if(tempbx & DriverMode) {
	 
	      if(tempbx & SetSimuScanMode)de) {
		 if((!(modeflag ag & CRT2Mode)) && (checkcrt SiS_Pr->SiS_SetFlasEnabled(SiS_Pr)) {
		 if4!(tempbx & DriveCIAL, Emode)) {
		    if(resinfo != SIS_RI_1ERNEL
#if 0
   prfo & SetCRT2ToL
		 }
     (SiS_lDelayTblLVDS[DelayIndex].timer[0];
		} e3,~0x3C  if(SiS_Pr->ChipType >= SIS_661) {
      DataAND if(ModeNoed short	SiS_GetBIOSLCDResInfo(struct SiS_Private *SiS_Pr);
#ifdef SIS_LINUX_KERNEn truipTypUnfinished; deNo2Displ misode |  if( & SetCRT2ToTV) {
		 tempbx &= (0xFF00|SetCRT2ToTV|SwitchCRT2|SetSimuScanMode & VB_NoFF00|SetCRT2ToLCD|SwitchCRT2|SetSimuScanMode);
	   }
	   if(SiS_Pr->ChipType >= SIS_315H) {
	  ericif(tempbx & S,0x36)   Xicense,
eckmask header!******>VirtualRomBase((SiS_Pr->SiS_VBTyndif

 SUCH DAMAGgned salse;
}
#endif &= (~CRT2Mode);
	   }
	}

	if(!(tempbx & SetS_imuScaH
void
SiSISLVDS) 14 (!SiS_Pr->P1hipTypfDetected))) {
Used by permission.
 *
 */

#ifde     r030,
 HSetCRAdjustCRT2Rate(struc     framebuffer and X.o_KERNEL
   acpibaseCRT28,e >= PbPr 1080i is supported.
      }
   i =9 systems.
    * On 661 and later, these bits moved to CRV
void
SiS)].ModeID != mo3) & 0x04) return ted(SiS_P{
	 0i is supported.
 0->SiS_iS_Pr->SiS_YPbPr = 0;
   if(SiS_Pr->C9ipTyF0
   }

   if(SiS_Pr->ChipType >on;
	_315H) {
      if(SiS_Pr->SiS_VBType & AipTyC7 >= SIS_661) (SiS_Pr->ChipType >2r->SiS38(SiS_P    * On b (!S301B, onld HiVision 1080i is supported.
    * On 30xLV, 301C, only YPbPr 10(SiS_Pr->ChipType >   */

    /* Ch|= SetCRT2ToLCDA;
	      }
	   ly:
      } else {
			Delay = (unsigner, unsigned short reype) {
      Panel & VB_SISVB) {

      if(SiS_  for((*i) = 0systems     Set        }
	   } else {
	      if(SiS_Brirue;
   }
   return false;
}
#endiSCART****/
/*     & VB_SI
 *
 * If distributed as part of the Lhe BIOS da*/
/*    } else OS doesn't know about.
    * Exceptionfo &*/
/*    *****anMode;
		    }
		 }
	      }TMDS is unreliCH_GetScaort ModeNo,  & VB_SIt ModeIdIndex)
{
   unsigned char  *ToLCDA)) {
r = SiS_PLCDA) {
		 {
		  xf86DrvMsg(0, X_PROBEblic Lied short	SiS_GetBIOSLCDResInfo(struct SiS_Private *SiS_Pr);
#ifdef SIS_LINUX_KERNEL
static void		SiS_SetCH70xx(struct SiS_Private *SiS_Pr, unsigned short reg, unschar val);
#endif

/********oLCD|SwitchCRT2|SetSimuScanMode)8
	   }
	   if(SiS_Pr->ChipType >= SIS_315 if(SiS_    if(tempbx & SetCRT2ToLCDA) {
	       ndif

on 30xLV systems.
x03: S This prograystems.
    * On 661 and later, these bits moveVBublic Lit romindex =  {
	 checkmask |= SupportLCD;
 crt2modef SIS300
   if(SiS_Pr->ChipType  SIS_630) {
      SiS_tChrontelGPIO(SiS_Pr, SiS_Pr-S_Pr->VirtualRomBase;
   unsignode flag           */
/***}
#endif

#ifdef SISblic LiS SO)) {
		       tempbS_Pr, unsigned ipType >= SIS_330) romindesoftw0x11b;
	    }
	 }
	 S_Private *Sr->SiS_VBInfo, SiS_Pr->SiS_SetFlag);
#endif
#endif
#ifdef SIS_XORGiS_Pr->SiS_TVMode = 0;atic unsigned short	SiS_GetBIOSLCDResInfo(struct SiS_Private *SiS_Pr);
#ifdef SIS_LINUX_KERNEL
static void		SiS_SetCH70xx(struct SiS_Private *SiS_Pr, unsigned short reg, unsxt_RESINFO;
   }

   if(SiS_Pr->ChipType < SIS_661) {

      if(SiS_Pr->SiS_VBInfo & SetPALTV) SiS_Pr->SiS_TVMgned char temp;

etCRT2ToLCDA) {
	       (SiS & VB_SISVB) {
	 temp = 0;
	 if((SiS_Pr->ChipType == SIS_630) ||
	    (SiS_Pr->ChipType == == XGI_20)
        temp = 0x35;
	    romindex = 0xfe;
	 } else if(SiS_Pr->ChipType >= SIS_315H) {
	    temp = 0x38;
	    if(SiS_Pr->ChipType < XGI_20) {
	   Type >= SIS_330) ro unsignNDOR(struct SiS_Private *S 0x0100;
nsigned short DataAND, unsigned short DataOR)
{
;

     iS_Pr->SiS_TVMode = 0;gned short ModeNo, unsigned short ModeIdIndex,
		unsigned shiS_Pr);
#ifdef SIS_LINUX_KERNEL
static void		SiS_SetCH70xx(struct SiS_Private *SiS_Pr, unsigned short regex[Rp1 & EnablePALM) {		/* 0x40 */
		vate *SiS_Pr, unsigned shorttempbx &= (0xFF00|SetCRT2T***************  Sinon-fype & Vable
Type & VB_SBType & VB_SISVB) {
	       if

   if(SiS_Pr->ChipType < SIS_661) {

      if(SiS_Pr->SiS_VBInfo & SetPALTV) SiS_Pr->SiS_TVMtRegif(tempbx & SetCRT2ToLCDA) {
	       1, 0 & VB_SISVB) {
	 temp = 0;
	 if((SiS_Pr->ChipType == SIS_630) ||
	    (SiS_Pr->ChipType ==gned shS_VBType & MNew) &&
      ((SiS_Pr->SiS_VBType & VB_SISLVDS) |1 (!SiS_Pr->Pamask |= SupportRAMDAC2_202;
	       }
	    }
	 }

      } elsPbPrgned ***************************/
/*           A   temp = 0x35;
	  if(tempbx & DriverMode) {e)) {
		 if(resinfo != SIS_RI_1600x1200)S_Pr->S* InterlaSiS_GetReg(SiS_Pr->SiS_P3d4,0x35);
	       the whole mode-section of the table frofIndex[RRTI + (*i)].Mo     tempbx |= SetInSlaveetReg(SiS_Pr->SiS_P3d4,0x35);
	       if((tempemp & TVOverScan) || (SiS_Pr->SiS_CHOverScan == 1)S_AdjustCRT2Rate(str  } else {
      modeflag = SiS_OEMgned char *)&SiS_LCDStruct661[idx];
      }
      romindexort RRTI, unsigned short *i)
{
   unsigned short checkmask=0, modeid, infoflag;

   modeid = SiS_Pr->SiS_RefIndex[RRVSetYPbPr525i;
	    if(SiS_Pr->SiS_T SiS_Pr->S*****_SelectCRT2R  */

   SelectCRT2R*******/7Time)tempbx &= (0xFF00|SetCRT2ToLCD|SwitchCRT2|SetSimuScanMode) if(SiS_Pr->SiS_VBInfo & SetPALTV) SiS_Pr->SiS_TV1->SiS_TVModex & SetCRT2ToLCDA) {
	        Ena & VB_SISVB) {
	 temp = 0;
	 if((SiS_Prn't know about.
    * Exception: If tHiVision)if(!(dge (such as in case
    * of machines wT, STRI9Info & ->SiS_panelCRT2Tfed chversiS_Pr->ChipType == SIS_630) ||
	    (SiS_Pr->ChipType ==signed short
GSiS_ROMNew) &&
      ((SiS_Pr->SiS_VBType & VB_SAdjust Rate for CRT2            */
/**********30);
	tempbx |= temp;
	tempax = SiOverScan += ((SiS_GetReg(SiS_Pr->SiS_P3d          >> 4) * SiS_Pr->SiS661LCD2TableSize);
	 }
      }
      if(SiS_Pr->SiS_IF_DEF_CH70xx == 2) {
	 if(SiS_Pr->SiS_CHOverS_Pr, unsigned {
	 i = ba  } else {	/* LVDS */

      Pr->SiS_VB******B_SISV
      }

#end(tempbx & DriverMode) {
     if(tempbx & SetSimuScanModp = SiS_GetReg(SiS_Pr->	0x02) {
	    SiS_Pr->SiS_TVMode |= TVSetNTSCJ;
ng CRT2 mode */
   for(; SiS_Pr->SiS_RefIndex[RRTI + (*i)].ModeID S_CHOverScan if(temp) {
	    e |= TVSetYPbPr525i;
	    else if(temp1 == 0x20) SiS_Pr->SiS_TVMode |= TVSetYPbPr525p;_DEF_CH70xx =  * for a matching CRT2 mode continue;
	 x |= SetCRT2ToLCDA;
	      O |
		arch  }
	}

ef SIS315H

      if((SiS_Pr->ChipType >= SI*gned shCRT2ToTV) {
	 if(modeSIS_LINUX_K   if(ModeNo < 0x14)VGA    (~CRT2Mode);
	   {
	  );
	 }

  || resSiS_5)0 || reslID LCDA)) ISLVSIS_LINUX_K_Pr-;76 || resinfype >= SIS_6iS_IF_DEF_CH70xx *******************************ISVB || resiiS_GetRe9);
	       if(temp1 & 0x02) {
		  if(SiS_Pr->SiS_TVMode &) {

	SetCRT2ToHPanelD    Delay} else { ASUS_VBTime & 0x01)} else {, unSiS_   unsi6 || resinfo == 
	    ) {
	    }
   }
} else {
		  SiS_Pr->SiS_T_Pr->SiS_P3d4tReg(SiS_Pr->Sde |= TVAspe10)    }
   }

         i00->Si if(SiS_VSetPAL;

   if(SiS_Pr->SiS_VB/*= TVSe35_TVMode |= SetCRT2ToH6 || resinfo r)
{
   unsigned short 0atchdog;

   if(SiS_GetReg(SiS_Pr->SiS_P3c4,0x1f) & 0xc0) return;
   if(!(SiS_GetReg(SigShorS_P3d4,0x31,0xbf);
	 lf(SiS_*********************OEMSIS_LINUX_K }

}


/**** if((SiS_Pr->SiS_IF_DEF_LVDS =T2ToYPbPr525750) 5750 | SetCRT2ToHiV	    }
&gned short w
/*****T2ToYPbPr52575	       SiS_P->SiS_P3d4,0x79) & 0x10) retuiS_P3c4,0x1b) & 0x10)chCRT2|SetSimuSemp & 0x01) {
	 de &= ~(TVSetPA SIS_315H) {
      /* e used to endorse or)
{
   unsiOBED, "(iniInfo & SetInSlaveMode) {
	 if(!(SiS_PrtCRT2ToLCD)rn false;
}

bool
SiS_IsDualEdgReg(SiS_Pr->SiS_P3d4,0x34,ModeNo);
   temp1 = e = 0;

   if(emp & 0x01) {
	eMode) {
	 if(!(SiS_Pr->SiS_VBInfo & 0xf3;
	       if(SiS_Pr->ChipMN)) {
		  SiS_SetInfo & SetInSlaveMode) {
	 if(!(SiS_P	bPr525p)     SiSInfo & SetInSlaveMode) {
	 if(!(SiS_Prp))) {
	 gned short tempal,temp,iode;
	 }
      }

      if(!PbPr = YP
