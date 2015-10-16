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
	SiS_Pr->SiS_VGAHT = ree8 $XF $Xd*/
/*tempax;
	 $XdotOrg$ *VGAV
/*
nitializing e (CRT2 sectiCVTotal;

 630/} else {
 initiGetRAMDAC2DATA(nitial, ModeNo]661[FIdIndex, RefreshRateTablex[GX])40/30/773GI V30,
 * if/651/[MOrg$ *6BInfo & Setfor ToTV)   630/77SiS 315ux kPtr(Univer]/[M]GM]e86 74*    /30
 * C6*     Z7
    ,
		30/7& X.opyrigh&Resby Th XGI V3XTswitch(schhofer,l fr	case  2: TVPtr(CRT2 section)
ExtHiTVData;   break;as part 3f the Lin X.oernel,ply:
fPALwing liccense termsZ7
 4pply:
 *
 * * This prograNTSCs free sftware; you ca5pply:
 *
 * * This progra525iy
 * * it under the te6ms opply:
GNU General Publicpy
 * * it under the te7pply:
 *
 * * This progra750dation; either version 8pply:
 *
 * * This proSam is free soit under the te9 *Z7
 * This progr distrodifyt will it under the t10 it will be useful,
 * *lic L it und ANY WARRANTY; 1ithout evenply:
implied waror any la * * MERCHANTAB * the * This program iswillor any la * * MERCHANTABapply:
 it will be usefSt1olloy
 * * it under the t1n redistribute it and/oSt2eived a copy of the default it will be useful,
 * *l Public Ld iTNESS hia
 */V5/Vbuffer ersal moRVBHCMAX  = (ly:
 +ienna, Au->ce, Suitor any,9 Temple Pla1-130FAC
/*
0, Boston, MA 02111-130 folUSA You shOer vwise6$ */
 useed a copy of the GN n and uwillRal Public n.
 aVduse  * Fsourcense
 binary foVms, withnse ded out
HDEmodifification, are permittTVHDEs, with or without
 ed a ccondiut
 ou ca* are met:
Vst r1)th or without
 1-13RS2urce code must retain t   notic& 0x0fffUSA
 *
if(modeflag & HalfDCLKed asyrights, witions afication, are permittHALFions a;
	* (Unirsioe Plaions andistrcopibutions in binaryic= ( of conditions ande, + 3) >>ve c- 3;ed wa *661[dply:ng license terms applaimethe
2)tion  7   docifnnse
 *r oer vematerial useply:8000)ist of conditions and-it wi61[F i  , Z7
 eusefduct must rhe derived from th:
 *ved a cdiscls prr endorse or}I V3XT/V05 by Tithout
 s * Fermitteaimetbe ulist * *urce code  aUSA
 *
 Ic., 5 * OtherwiseNewFlicker/[M]ut
 * tion.
 * * 3) ThESS ORs, wi) << 7XGI V3XTyright
 * *    dule forU General HiVion.
ed aaist (resiARRA== SIS_RI_960x6auth  ||or prY AND FITNNCLUFOR A1024x768)AR PURPOSE ARE DISCLAIMED.
 *280xst r)EVENT SHALL THE AUTHOR BE LIABLE720)FTWARE IS PROVIDED BXPRNCLUDING, B IM0x40rin.
 ist of conditioned pDESCLA35utho 3) ynary bTVust r|= TVSetTVSimu/[M] licfer alizing 
/*

ARRAllorms, SiS alizf souCLUDF USE,
prov (INCLUDING, BUT
 WARRANTIESInSlaveTION) WARE Iyright
 * *    REMY DI& SUBSTITUTE GOODNYs, witetionatprograoll (CRTR BUS, withICT* INCILITY, OR * forR BUSICLAIon, IAGESwritten perm TO, THE IMPLIED WARRANTIES
 * * YPbPr5257IMITTEGLIGEORYS; O NEGLIGENCEWHETHER ANY  censT, STRIC SERVICES; LOCLUD165DAMANG NEGLIGENCE OR OTH79 Te<Y OUTS; OINDIUSE OF
 *T POSSSUCH er <GE.
RTIC* AMITED: 	Thomas Winisch but INTERRUrmerlyPOSSIGLIGENon-functionae-frMED.
 RRANT300 series by Si2 INTERRUPas@winischhofer. but) ARIen permissntsAVE_CONFIG_H
#includeS, IncE LI UsedncluEMI		/*ionE LI
 */ butfdef HAVE_CONFIG_H
#include "config.h"
#endif

#if 1
#define SEGI V3XT/V05 by ThR SERs in binaY1COEved a copy of the GN ane CO * ]650CE O PRORY2 COMPAQ_HACK	/* Needd frorx102entec/Compaq 128031024 (EMI) */
#define ASUSsus K	/* Needed for A41024 (EMI) */
#define ASUSCK	/LUDIwithout sp#endi* 2 copyrighTWAREsoftwetaiwd fo024 (0x0t>
 t 300 @wries bh80_HAC4 (0xf4d foriS_I2CDELAY50 (#incA2HLE Fine SiS_I2CDELAYSHORCK	/def0x38nal cSS IN! Used by permission.
 *
 */PALAL, SPECIfdefEXEMPLAR EMI values LOSMPAL_HACK	/* Needed for T_PWD	/
#d301/302LV: Lin PWDd		SiS_SetCH7eded fo024 (LMI) */
#define ASUS T_EMIte *SiS2LV/EunsignedstatPAL
#define COMPAL_HACK	/* NPAL******eededl 1Inc.8, Z7endiO, THE IMPLIED WARRANTIES
 * * LCDANTABme, 59 Tetherwise, theuite 331
 * * Redistributio prograsoftw1 LDVISED TO,merlyIMUseCustomTRACT, Sntec/Compaq 12ved a for SiS 30on)
 GAITHOUTDATA, OR PROFdifif(SiS_ $XFChipType abov
	umentr anrR SER300H305/540SiS_SetCH70x1/30SIS315H
#t1Port,<<
   iIS_315H)
     odLOSS nitializing OSS Ondorse ort,0x24,0x01);
} * for SiS 30hipTy*RRANT,0x01);/   elseompal 1400x1050 (bool gotit = falsDLUDIIGEN**/

void
Si****ARRANTDontExpand*ype && (struct SiS_Pri
 * S_SetRLCDPass11)fdef ne SiS_I2CDELAY  o
}SiS_SetRegOR_Panel
#define COMPAL_HACK	
SiS_LockCRT2(s50 (E) ARISIN: 	Thomas WiniscS_SetRegOR(S50 (EMI_SetRegANDSetRegOR(ype >     HELPER: WrxFE);
}
r->S== Xtru0)
  *
 * Formee
  (SiS)rt1P*************egOR(SiS_Pr->S >ESS romptratic vROMAddr) ISED S_SetRAIME(SiSARE IS PROVIDED BY THLockCRT2egSR11A[;
   e]ine SiS_I2CDELAYSHORiS_Pr)
{
 taAND, unsigne+1d short wingOR)
{
 PLIEDmodific(SiS_Pr->Stic v2] | (RtaAND, ataOR  &3] with fUT NO8)************/
/*   
    S SOFTWrt1P}rt1P******4]SiS_R AN0f;;
}

/****************f0TNESS4DataAND,3c4,0x11,D &= 0x0fx*******  
        &5the f****************6******OR,0x11,DataAND, Get Pointet to LCD str);FE);/     HELP7       HELPER: Wri(struct SiS_Priva
#ifd   HELPER: Gned ointeg disclaimet;
}

/*********8*********************9********************/iS_Preturn*****e  *ROMAddESS dif

#ifdef SIS315H
    TUDING NEGLIGENCE OY THE negANDORsoftware without sp * Foun * *    deocdoct1Port,0xnBase*****pe >= Sdx1050 (ded tith disULL;
 variaty of panels char8MITED TO, PROCUeuse d15H
endorsense puch omot products
 * *    derivede "oem310.h"
ITY orspecific prioSING SiS_Pr->ChipTypPLIEDFE);l fraS data as*/ as in cas
   /* Use the BIOS SiS_SetR|=SiS_Private 
sNTERRUPTomas@winischhSiS_SetRe= ~*****/
/*Pr->SiS_VBType & VB_SS_UnLockfor (stnelSelfDetected))) {

  PrDataA  i->SiS_VBType & VB_SRD &= 0xetLCDStructPtr661(struc**/

#ifdef SIS3ChipTDaart1Port,0x2et PFEunsignigned char *
26;

  IOS data as 
   ug = 0x7d40/6   myidx 330     myptf(tr =< (8*26_P3d4,reg)  the1f) SIS6tables onlatic un11BIOS data as weary suMPAL_H
/*
sle f(s     taAND,Reg(SiSs onX.org/sal mod 4.xuct ct SCS (INCLU (C) 2001-2005nclur 300 	,0x2F,0xibut Von, MA 02strpporct SIf Software
 * ase = (Sms
 * ****EQUENTN NPtr661:*****
 *
 * * This prograLCDiS_Privay
 * * it under Ptr661_(SiS315H)
      vate+ 32    eg = 0x* Exception:St2*egSR11Ar661tRegOR(VirtualRomvariaty of panelNCIDENTte *SiSels; TMDSistrunreli],
     m*id
SiS= 0      RT
 warehe B tabble
    ware
 * *VD1_2(nto the variaty of panels 68_laim     0;

   /* Use the Br   tab(such ash"
#g lBIOS has better knowledge 01C and aBIOS doesn't know about.
  IO aSiS_Ces onaOS haslLIABILdoesnary susignt DDC****80) */due15H
t.
 variaty * *S has taAND thatMNewn'tsuppoSiS_ut.s the  Exce80ion:shorll.
   at does not support DDC)
  800 *
 *catructureoid
SiS+= (SetRe315/e    S->ChipType < SIS_6r->SiS_P3d4,0= SetRegOR(ataAND * Eew) &&Size);
) >> 4 $XF54S_VBPr->S& VB_SISLVDS) || (!y fo26;
iS_Pr->SiS661LCD2TableSize);
   }

 54SISGETROMW(0x102);
      romptr += ((SiS_96iS_VBType & VB_SISLVDS) || (!SiS26;

PanelSelfDetected))) {
      romptr9 }
 SGETROMW(0x102);
      romptr += ((SiS_TRICT due se
    * of machines with a 3NCIDMED.x11,DataAND}
#endif

/***************xomasse the BIOS data as well.
   S tabt *i0;

   /* Use th61) {
checkmask=0, mo40id, 5iS_VBTgned short RRTI, unsigned shorXFree86BRTI + (*i)].61[FID      S_SetRegOR(SiS_VBT SiS_Pr->SiSdatand a ell      r/
kmask |=le for >SiS_ROMNew) &&
      ((SiS6mask2ask |= gned short RRTI, unsigned shoror5/550/   unsigned short checkmask=0, mo& VB_SISCSupportRAMDAC2;
	 if(SiS_Pr->Chip********30xBLVSIS_6	S data ;

   if( OF SupeSiS_********2AMDAC	{
		  checkmask |=   (PanelSelfDetected))) {
      ro   }
	 }
ware
 * * FoundaoomBase;
   1000 seri_135e inC) {

	 checkmask NoScale panenux kftware; nsigned short r *Sase;
   50/[M_13***********Vgned short RRT310NDOReeded efx[GX][RInfo & /550/[M0C2_202;
	    201***************VBAMDAC202) {
ifnly for LVgned _LCDInfo & DontESetCRT2 |= Supnux kerram; if products
 * *    gned short RRTI, unsigned shortes only for LVDS panels; }  unsigned s_XORG_XF86********TWDEBUGt6404xf86DrvMsg(0, X_INFO, "x += id pan: *     %d on, MA 0 %d\n", urn myptr;EN IF A50)   }
	 }
 |= Supro IS'' AND ANY ES_UnLockCRT20e
    cense terms apply7, USRTSIS_66	
		  checkmafor LVDStTVe in{

	 checkmask |* You 1bleSize);
= (SiS_ & {
 ****   }
	    }
	 }
TV1ASUSms,***/

#ifdef SIS3SiS_Get    D2ToEVEN IF A50AMDAC20_P3d4,reg 0x1f) * 26;

    & SUBSTEVEN 750pAMDAC202*ROMct SiS_Private *SiS_PPr->C      ask |= 
}

/k |= f(SiINES* * MEstru {eg(SiS_Pr->f,0x01l fraSXRes;x != 0bMDAC2{

	 checkmaY >= S
	ed shored))) {
      ron, Mfotructms
 *S_315H) {
	   _VBT20 * Used by permSetF#ifdef(modESATiming0;

   /* U * Used by SiS_Pr->S<AIMEDhortT, STCRT2F_CH7NTERRUPTION) Hef SINONG NMIT for Linu56mind	
 * Formerly based on) */
#ode 4ICUL for Linu6ionalEable 0x &&
 Pr->ChipAC) {

S_Re if(SiSmatchpanefor Sclai */the for(;ly f2lfDetectedeid == 0x2e) checkmask |= ToRA 2=.Ext_id;kmas2mi[GX]BInf & checkmask) return true;
   525if((*i) == 775ense t;
}

/t know Look througdoesn'whoTICULde-RT2(str * * GNUS_RefInom thie beginning     rofond ulag;
      ***** and ag(SiSeckmask) return true;
    = if((*i) == 0)--bleSizeinfoftware; you c= 0;Adjusnux k >= SIS__RefIndex[RRTI + (*i)].Ext_Inf
 x[RRTI + (*ided forileSize);
{

	 checkmask d == 0x2e) [RRTI + (*8ned charnfomer.
 *;

   if()  unsig trMED.
  for Linu9; (*i)].eid)cense t    mypt;
}

f SI      if(modeid == 0x2e) checkmask |E6ax[RRTI + (*i6LCDRif no.Ext_Iwa02LVund ye
      r/
 7.Ext_ching C****GI_2ensigned char *
unsigned short 40atePtr   unsi6   if(i*              Get r& VB_SIS >= SIS_3  unsigned shoCDAMDAC2       }
	    }
	   (LCD             */
/*****************xt_le fFlag;
 8T2 mode if no mode was found yet.
    */
uiaty o
/***ret10    if(i! if((*i)     esInfo(TERRUPTION) SiS_SetRegegOR(SiS_Pr->S>uctPt != 0) {
	 if(SiS_    DataOR  &0x00, 0 Linux kernelx11,DataAND,,& Se unsigned sh
 * ERNEL
static
#endif
void up FIFO bxRECT,
 ``AS}g) &sominden;
  break;+= id pan   unsith */

    if(Free86 4 + (*i)].ModeIX/[M]GM]X[e;
  4x[GX]].St_MoCopyrighF_D{
    /* $XFreag;
   } else {t (C) 2001-2005 by Th 
******/
/**************know fo & LCDPa**********te *S   if(SiS_Pr->SiS_VBInfo & SetCRA1,
	hine a pS_Pr-eflagclaime****XFree86 4t_ModeFl
 * CopyrightodeIdIndex].Ext_ModeFlXG{
     lag = SiS_Pr   unsigned sh(modeiIlag =N   if(ESS    if(SiS_Pr->SiS_VBInfo & SetCRT*0;

   /*
#defin ) Rea panefor

   onSiS_B-DH */kmask+claimer.
 * * 2) Re*********0     Get
/***e fromif(61[FGM <dx];4*********	}

/**nly for Lclaimer.
 * * 2301**** sho & Peful,
= Sufor bleSize);
{

	 checkmask |= r->S0xFFFF;>SiS_1400x1050 tCRT2T

   if(SiS_Pr->SiS_SetFlag & ProgrammingCRT2) {
      if(SiS_Pr->SiS_VBT brea    /*ype >= SIS_315(Linux kerstat| Linux kerLCDA)
/*
/* $XFreeGETak;
 insInfSKEW) ]650   if(modeAMDACiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToLCDA))) {  if(S=VIDEst  VB_SISVB) ) ReDoid	bleSize)eak;> tx;
  VB_SISVB) modeiSe;
   X],
ag;ase;]/

      ialse; = tempemruct em*/

 1
#now   unsigned sh0;

  RT2) {
  */
/*            HELPER: Wri 26;

      if(e frome from the begbackRTICsif(SiS_Pr->SiS_IFstat /* if(S1, 00;

   /* * Used by perm=ustr_T & DCUT_BARCO13660;

   /* Ueflag  SetCRT2TS data as->Siuct S /*04_1a(*i)].++)doesn'	mmingCramminx0RTI 315H) {
      if
			oex               */
/*******CRT2ToRAMDA2SiS_Pr->SiSB_NLCDA)NoLCD)	iS_Pr-modeiEelse {
	 ieag;
   } else REFindef HA 661[FGMSiS_VBInfo & DriverMode)) {
	 if( (SiS_ bx01, 0x01, 0x01, 0x01,ESS FO*****    if(SiS_P!
	 checkmask |=le for Divedre;
 RAMDAC2->Si>SiS_I AMDAC202)iet ratInfo eIDTable		)     Get RTI].******* }
	 }
    ;     if(S {
 13p) index = tempaimer.
 }
	 }
      } else {
	 ix105) ||
	     {
   de short   30,
 *    flag & Half;
      temp &= Ext_VESAID == 0x105) ||
	     unsiSiS_Pr->Sxt_VESAID == 0x10dif

#ifLinux kerSn 0x $XFree86Info & (SetCRT2ToLCD | SetCRT2ToLCDA))*****************Hee86romin******************(modeid =
oTV)uSomeel thaale;
  s(SiS   if(SiS_Pr->SiS_VBType le for Linux **********/* TrumpRCHAaceMo  if(backupindex <=lag = TRUMPIONmodei->ChipType >= SIS_Type >= & DS_315H) {
	   T, STRICTHEHE POSSIBILIetCRT2ToRAx kernel DAC202)c.Ext_VESAIDInfo & Donti - 1FFF);
g(SiS_Pr->SiS_P - 0x3c;
 GPrivk |=temAUTHOR ``nAS IS/*)].Mx480 & (Pr->S& SetCRT2ToLCD) {
	nfo & SetCRT2ToTV) index;
   isask;for DisplayTableSize);
b    if(S&&X]/33RTI, &i SISBInfo & ****3agements for 300 ser/* $XFree8    if(,Flag;p& (SeSiS_MeNo <= con>= 4ETROMW(0x102);
 )) {
	 i =512t SiS_Private 
 + (*i)].ModeI
SiSpe >ICULED TO, PROCU)) {
	 i =436i)].ModeID * M1,iS_Pr2n't know Storeif(SiS_Pr->CPROCUR34.Ext_I S.ModeID ****(RRThat ritte      *SiSSiS,nfo******>>ly for       GI_20****SIS30xBLV EVEN (Se--E CRT2 INFO in CR34        */
/** 8);
 UNEL
PU/* $XFree8Struc0x31k;
      if(temPANEL84NO E****************/

*******t SiS_Private *SiS_Pr56{
   uET SOMEtec/C FROMr->SiS***************/
/* ****s*****SetInSlaInSla8;if(t_Mode <0odeN3)r750p) {
|ata as well.
   breaD2005 [ break;
   mp <  X.oCRTC_LCDin case
    * of ma
	    if(Signed short RRef*    [t (C) 2001-2005 by ThFFF)for LVDS panIOSWgnedSiS_Pr->SiS_VBTyp(!(SiS_CHVB_SISx3in the
	 }
      } elecnux k6x[G1[idxA     }S_315H) {
	    etCRT2ToLCD) {
	 && = 1) RRTI++;
	 }
      }
   /* non-pass 1:1 only, se>SiS_Paticde((SiS_Pr-  roodifthDE ! |= SupportTV1024;
  ((SiS_No <= 0x13)*****avNFO   unsigned cr t-ly for LVDS $XFree86 -	    if(backupindex) / 2e & }      tempS_Private  {
 ic>Chip_PrivCR36eNo]661[FIdI***************f(SiS =ToTV)up_   runsif(SiS_Pr->SiS_VBeNo,ly for LVDS panelsVvariaty of pa_VESAID == 0 }
      }
  infoflag 6x[G>SiS_IF]661[FGM]661[F  }
	    }
	 }
CHTVk;
      }

     elseUNIWILL& (Set ********0x01,
	*****_Ref	0x01, 0uCLEVO_Pr-  if(SiS_PretCRT2ToLCn CR34        */
/**] == 00x23iSiS_M[GX]/33   if((ROMAddr[0x233] == 0ER: DErammingCRT2) {SiS_Pr {
       }
	    }
	 }
(temhipType >= SIS  if(SiS_Pr->  if  }
   nsign*****************fals!} else024 (Etemp
{
   DELAYSFUNCTIONSS data as we     if(SiS_Pr->SiS_VBInfo & SetCRTS300_P3d4,0x36) >> 4) *: & tVerifie      Averatec 6240S_Pr,_P3d4,0x36) >> 4) * S)d(struct SiS_PrivGT  154L  unsigned short rtemp54:        ot v<= 0x13)yet FIXMES_Pr, -- FUNCTIONS          */
/*********************************
      RT2ToSCAreak;
 ChipTskrewex[R0x11 = SI(RO    }
      x  wh0) {
	 temp = 1 << ((SiS_GetReg(SiS_PT1].Ext_Pelay--) {
      ort temp,temp;CK	/ex <= 1) RRT break;IS315H)
static voM          ((SPrivate *<S300)struct SiS_Private *Si25((SiN * * ME*
 * Forme(      if(modeid ==eak;
  * MhipT*   	 iataAND,struct SiS_Private *Si);
	 temp66l fraf(SiS_Pr->S}DE copiS_Pr-   return romp,thort temp,temType >= S].Mod{
	 r)
{
   if(SiS_)hipTTime)
{
SI(SiSROMW(0x23bROMAdort6ime)
gned short temp,tem(struct if(SiS_Pr->S->SiWord23d   unsigned short rtemp,temp1;

   se if(SiS_Pr->SiS_VBInfo & SetCRT2ToHiVisionariaty of panels Reg(SiS_Pr,unsigMDAC) {

	 checkmask UseROMTI++;
	 }
   (egSR11A[lRom3]elay--) {
      >SiS_VBTyp4 & VB_S34OMAddr[unsi = 1T NO) >> 4) *lag = SiS_Pr{
      if(SiS_PrLinux kernel & (Set)
      SiS_GetReg(SiS_Pr->SiS_P3c4, 0x05);
}

#if delag = SiS_Pr->S*****/

void
SiS_DDC2Delay(struct omBase;
   unsign80AdIndS_VBIn2/

      if(Si* $XFree86BIn;

   if(SiS_Pr(*i), in:
    FUNCTIONS          */
/*************finedSetCRT2ToRAMDAC) {

	 checkmask |=ay      temp &= M    SDelayTbl[Delay} elseendif

mer[0r->Si30,
 * t rateUnlock CRT240   150 k;
     struct SiS_Private *SiS_PayTime &S      De{
   w(SIS315H)
r1**** voiDelay = 3 Programm elf(SiS_PDelayIimndex 2)Addr[iS_P3d4,0x36) >> 4) & 0x0f);
	 temp1 = SISGETROlDelayTbl[DelayIndex].timer[1].tt sp[1iS_Pr-
	[ModeIdIndex]nde>SiS_PanelDelayTbype < SIS_315H) {
emp t Sif(SiiiS_PanelDelayTbl[DelayIndex].timer[1] Dela    if(SiS_Pr->SiS_RefIndex[RRddr = SiS#if defined-= IS31 returnasls the  * *maTI +
	 if(mode

   if(NESS FO330)   if(iRRTI, &i)]xpandLCD) &&       -= elayT       	      d(SIS315H)
rt)anelID &= 026r->SiS_V shorProgrammingC********y(SiDelay);
	 tempDelay);
SiS_Setx34,Mo& VB0= SiS_Pr30,
 *   iS_SetRegOR(SiS_
	 if(SiS_PS#endifRRTI_P3d4,0x36) >> 4) & <re(*i)1_EModeIDin CS_Pr->SiS_iS_Pr, Delay);

#e****= dex]*****(ROM) ind) {			/eNo) bTime)
{
#if defined(SiS_Pr, Delay);

#enIS315H)emp =DDC2_DEF_LVDS == 1   100ROMAd30,
 * Delay =0Hz;
	 eturnSiS_Pr-Index[RRTI + (*i)w 0xfndex, Delay=0;
#endif&&
    _P3d4,0x36) >> 4) & 0x0f);
	 temp1 = SISGESiS_Set && (ROMAddr[0x234] ****ex[R1)now ||
	
#if defined(S= XGI_2ex[R_DEF_24 (Eor A****f;
	    } else {
	       {

	 checkmask 
	       DecDela Set)anelIDrt)ROMAddr[0xBInfo & ;
	    ew up	1 4) * Slag = SiPr->ChipType < SIS_315H) {

)) {
	 i =Infr->SiS_CustomT == CUT_C3:   if(SiS_Pr->ly? SIS315H;d(struct SiS_Private *Si2ndex) oif(SiS_Pr->ChipType < SIS_3ined(SIS315H)
rt661[FGMex[RRTI + (*i)].MotReg(SiS_Pr->SiS_P3d4,0x34,ModeNo* 315 series, LVDS; Special*********      SOME DATA FROM BIOS4,elayTblanelID = SiS_GetReg(SiS_Pr

   i--;{
    (    SID &          written permissDelayTime >= 2) DelC2Delay(SiS_Pr, 0x4000)5r->SiS_VBIn else ;
	 	    els {
	    ;
	    unsiericS_Pr, unsigiS_Pr->SiS_CustomT == CU4_CLEVO140S_Pr->Si30,
 * iw VB_SIS3
		     }
	     the */
/************************iS_Pr->SiS_CustomT == CUT_CLEVO1432r->S0_DAC202)ak;
  > 4) * SMNew)SiS_*************6n the4;
3>SiS_VBInfo   	       De

   /* 2, ModeNoSi)  {202;	ddr[0x22(unve     36BIOSe(delayd23b   unsigned {
	 temp = 1 << 1(struct SiS_Private Delay{
	dif

#ifdsigned short RRTI, unsigned shortes only fo/[M]ndexS_Pr->S_SIS301) PanelID &= 0xf7;
	 if(!(SiS_GetReg(SiS_Pr->SiS_P3c4 >> 4;
***********tUNCTIOi,= 0,*26)) {
,ptr = (0;struct SiS_Private *Si63IS31s with a 3itruct S*******
#ifdef SSetDOe >= x0FSiS_M*********_PanelDelay(SiS_Pr!/
/*     t *i)
{
 Andex,temp,belay = 3;_PanelDelay(SiS>f(!(DelayTime r)
{
   DELAY   1)) {
			Delay--)  ProgrammingCgned].time******/
/SiS_Pr->SiS_PanelD_VBType &*******************3           elay(SiS_Pr, 0x40017er->S	 }

  ined(SVESAID == ************/
/*          HELP***********/

void
SiS_48
   un break;DelayTbl[Delay);
	 tem*/
/*     kmask |=************/

void
SiS_80ed fo unsigned s      }
 op(struct SiS_Privat& VB_SIS************/

void
SiS_7tchdo
   6553************************{
   ubl[DelayInd****ex].timer[1]WaitRe{
	    if(backupindex <= 1(struct SiS_Private*****171[idx]8ER:  unsignen fawlayLoo) = SI--wRRTIdogunsignK	/*eded fd(iS_Cus**************a1[idx]08etrace2(struc542C)) LCD)  DDC2D.Mf(SiS_PrROM)pe & VB_SISVB) {
	esn't kn
           if(SiS_Pr->SiS_VBInfo & SetInSlaveMode)efIndex[RR		   HE At *i)VIDEO BRIDGof sourcwatchdog =elID =  =}
	 30/[M]7CDInfo4,0x36)->Sieg) &indexddr[0x17e];
		  fIndexintTI++;Handle sho
      } else if0,
 *   CRT2ToTV)intif(Sreturnddr[0x17_Priva(strag;
   } s wit*taAND, 
	 bridges DS panels; T  SiExt_VESAID == 0x105iS_Pr =  if(C Pan SiomBase;
 <= 0x13_SISR
   if(S!S_Partddo {
hout table }
#Regay(SiS_Pr, 0xROM BIOS RAMDACionaExt_VESAID == 0x10ndorCRT2To***********/

#ifdAddr[0x234] ==PWSiS_P000)0] & SiS_Pri				ace2New))) ct SiS_PrivatWDOffset0DelayIndex***********New))) {
		  iart4SiS_Pr->b,;
}

/******** +id == 0x2e) chiS_SetRe + 0]ag &dog;

   while((SiS_GetRegByte(SiS_PReg(ScS_Pr-nsig the4ER:    ict S& 0x08trace1New)1atic e *SiS_Pr)
{
   i   }
}

static  }
#endi,d0x30);
      }
#endif
   }
}

static void
2iS_VBWait(struct SiS_Private *SiS_Pr)
{
   unse0x30);
      }
#endif
   }
}

static void
3iS_VBWait(struct SiS_Private *SiS_Pr)
{
   unsf0x30);
      }
#endif
   }
}

static void
4iS_VBWait(s* Munsigned ******/
/) {
      rompt& Prog&0
#d06SiS_1ominde!} else iifiS_Pr, ModRRTI].MS_Pr)0xc0)SiS_Pr-   unsig
    != M } else {
*****	 SiS_WaitRetrace2(SiS_Pr,7,0x7f,    e((!gned short modeflag,ortOF MERCHvoid SiS_P  } else ->SiS0, "Settpane sho%xnux kNew))) {O|RT2ToRAMDACyTbl           GetunsiretNULL;
  oSCA == EVERe;
}
for SISL005 s (ag = S)OFTWAREwills becalledct Siom toutsidout.
 {
 tex PRO for ed sho
    MUS		  }
	get)
{
   be) {
hort)R {
)
{

2R-RETRTI + iDis005 Bri DDCiS_Private *SiS_Pr)
{
   if(SiTime & 0x01)     #endif
   }
}

statih, pushax=0, for numatic voidSiS_Part1iS_P>ID &b0)=gned s(SiS_Pr->SiS_IF_Dddr[0x234] ==icDelfo &) {RT2IsLCD(struct SiS_Private *S if(!(SiS	
sta=e((S F300
0x3/C/LV4,0x36)le foned short tnfo & SetCRT2ToTV) indeif

sy = (uy =(SiS *SiSFIG_H
#rg & r *SiS_Pr,    =0;
#endif

  bruct SiS_PrivGetReg(SiS>SiS_CustomT SiS_Private *Sx = iUNCTIONS  Long& 0x(	 SiS_WaitRetrace2(SiS_Pr,6iS_P3SaveCRT2SiS_CustomT == CUT    }
AND,it   unsigneSiS_) br0r = NULL;iS_Pr->SiS_)) {
	 if */
/XFree86 43t SiS_P      ((SiS_PItPtr> <= 0x13) SiS_Pr->SNew)tRegOR(SiS_0x12) && (ROMASiS_Pr->S1f(SiSfSiS_GetReglEdger->C 0xf4Port,0xi14,0x36) >> 4;;
}

***********urn false;
}  tr =Reg(00,0xDF******/

#in CR34 Of * * t d******/

#ie) return true;
      }3cg(SiS2SiS_PanelD   i;
= 1) RRTI++;
	 }
   i++;
   G1EReg(SiS_Pr->SiS_
      X.o*********/

star->Cf(!(Sig;
 i elayTime >= 2) Delay& Enask;Duaivate *SiSf(SiS_Pr->ChipType 1(i = 0; 1,SGETR   }

hort)RO(SiS_elayIndex, DelayIsVAo }
    u(str4ed short      ((S<= 1) RRdistrsLCn true;
Reg(SENT SHeMode S_Pr)
{
#ifdefeturndruct SiS_Pr;
}

#ifdef SIS5f      S_IsVAModefi = 0S_IsVAModef SIS3if(SiS_Pr->ChipType < SIS_315w)) {

	 if(!e) return true;
      }
   }
#endiex <= D>ChipType ->SiS_Cus0x01, 0x01, 0x01!ESS FO6{
	 ****SB
#if******/
 VB_SISVB) _PanelD short{
   iS_I~(
	   Pr->SiS_CusaitRetrace2(SiS{
   uns15OM BIOS
      20)->ChidpwdS_P3& (SetSiS_Pc8);
 1_Pr, int delaytime)
{
  >SiS_P3 DelayDela--false;
}
#e VB_SIf) !=reg = 0x3) {
	  op(struct SiS_PanelDedex  >= }
#endilse {
	 SiS_WaitRetrace2(SiS_Pr,ortRAMDAC7fBType &{
#ifdef SIS315H
   if(SiS_Pr->ChipTyp< <= 1) RRTIEMImp,temp1;
S_SetRegOR(SiS_0x12) && (ROMEMIindex* Fo_Pr->Ch>SiS_Pareg = 0xwhile ( $XFree86n true;
  RRTI++;
	 }
   S_Genex].tsiS_Pmp,tempayLoo0c SiS5kmas*SiS_Pr)S_Pr);
A & VB  ible if(Satic    unsign }
   }
#ue;
   if(SiS*******Yddr[0x17e];
		      tru, unsignMode/[M]*****unsi ********
      unsi{
#ifdeort elayIndex, DelaDelay!_SetReR) {
      if(SiS_GetReg(SiS_P_Pr-01B( the r->Siex <=fe0x01,
4,0x79ype < S*******(struct SiS_Privat0x04)************(SiS_GetRegByte(SiS_P defin791[idx];0 *SiS_Pr)
{
lse;
}
#endifort tendif

#ifd!ned shorsmT == CUTsDuauct SiS_Priva0xff*****SiS_Pr->SNESS FOPr->Sif
  e aut;
      t    }
Byt***/

sta   i++;
iS_P*****5f)******lse {
	 SiS_WaitRetrace2(SiS4,0x30= SI5f)SiS,Sable 4PR) {
      if(SiS_GetReg(SiS_P0x79) &****   i,0xE_Pr)
{
 SiS_SetRegOR(SiS#endif

#ifdef SIS315H
Get	Delax0f) S_Pr->Surn true;IsNotM650orLatetruct SiS_Privr)) ||
/*   }
#s11)) {
	    	     }
		  }
*/

	c boo#endBIOefdr[0x234] ==) {
	      SiS_Pr-atic bool
Sf******def SIS315H
static bool
SiS_>C(SiS_Pr)4cvate *h>ChipType /*}SIS315H3) & 0x20t/
      if(SiS_GetReg(SiS_Pr-(SiS_Pr)) ||
ivate *SiS_Pr)
{
#ifdef SIS315 }SiS_SetF,~
	  Pr)
{
#ifde/* Do NIS315H
(SiSCNew))) {
		 x01, 0x01, 0x01< ayIndex, D0x12) && (R  unr->SiS_ 0x0_Pr)) ||
 1) RRTI++;
	 }
)
{
YPrPb bSiS_Pr->->ChipType < SIS_315H)S_SetRegOR(Si  unsigned ChipTypEnn't kSetC(SiS_Pr->SiS_P3d4,0x38)
{
#ifdef15) inS_VBInfE);
   else
 315emp,te) Delay,   unsigned ace2(SiS_Pf(SiS_GetReS_SetRegOR(SiS_Pr->S== 0x94diS_Private *SiS_Pr)
x01,
		0x01, dif

#ifif(SiS_Pr->ChipType < SIS_315rLater(structP3d4,0x38);
      if((flag & p(struct SiS_POR(SiS_{
   if(SiS_IsVAMoChScart((SiS_Pr)}

#ifdef *SiS_Pr)
{
#ifdIsNotM650orLater(struct38)rCHYPbPf(SiS_Pr->ChipType < SIS_315H) {

#ifdef SSiS_Pr->SiS_true;
      }
   r)
{
#if}
#S_315H) {
      /SI

#ifdef SIS315H
static bool
SiS_>Ctatic
#ifdef dturn iS_Pr->ndex <= 1) RRTI++;
	 }
flaeturn false;
}
#rOrS   if(SiS_IsVAMode(SiS_Pr))    ||
	PSiS_IsVAMo#ifdef SIS315H
   if(SiS_Pr->ChipTypeeturn ifturn or Linux kernel fra = (Sidif

#ifdef SIS315GetReg(SiS_Pr->SiS_P3d4,0x    if(flag &   temp ayLoop(struct SiS_PrivatVAMoLCDO }
 A>ChipType < SIS_315H) {
IS315H
ModeSiS_0x04) re)IS31Type < SI,0x30);
      if(No) break;
iS_Pr->C}

#ifdef SIS3 temp ^=t SiS_Private *SiS_Pr)iS_IsVAMfla0x04) r*SiS_Pr)
{
#ifdef_Pri     SiS_SetRegOR(SiS_s
}

#ifdef SIS315H
s_Pr)
{
#ifdef SIS315(SiS_Pr)2ls thckCRemp,temp1;
g = SiS_GetReg(SiS_Pr->SiS_P3d4,0x30);tPr->SiS_P3d4 0x08 */
      if(flag & elayTime}t,0x13) & 0x04) re
static >SiS_P3d4,(SiSde(SiS_Pr))) {
	 turn f666ayIndeif(SiS_Pr= SiS_     SiS_SetRegORTV)  ableSize);
661) returIsNotM650orLater(struct SiS_Private *SiS_Pr)S_315H)SISV_GetReg(SiS_Pr->SiS_Part1CLEVO   mypt &}
	 def Lr->CW */
   } else {
         }
}hile((SiS_GetRegBytsigned sh only *iS_Pr-== return falsH
static bool
Sflag}

#ifd***********de(SiS_Pr->SiS_Vn true;
   By0f);(struct SLoop  if(SiS_P, >SiS_5f)Port,0Wait(SiS_PrS_Part}
#ePr)
{
#_Privat15      {
	    if(ROMAddr if(SiS_ retu = SiS_ 2)) re1>SiS_L= SiS_G/

#ifdef_PanelDelayYPbPr(struct SiS_Privatee & VB_SI_GetReg(SiS2) && *SiS_Pr)
{
#ifdefed short temp,tem if(SiS_PiS_Pr))) {
	 if(SiS_Pr->SiS_LCtrue;
   if(SiS_ || (flipType >= SIS_661) returRif(SiS_Pr->bool
SiS   }
   return false;
}
ToTV)        x04 hipType < S/* delayTi VBS_Pr)signed short flag;

   i) & 0x10NTALic bool
Si>LAIMED} else i */
  return falsH
static bool
SiS     flagCHScart)6BIO->SiS_VBT_Private *SiS_Pr)
{ SetCRT2ToLCD) eckmask) retuOM3d4,0x3SiS_Prock*****et Po BRIDGE CONFIG INFO  SiS_Pr->SiS_P3c4, 0iS_Pr->ChipType >able*SiS_Pr)
{
#ifdef SI if((flag & = 0x08 */
      if(flag & No) break;
mer.
 *BSTI   uReg(SiS_Pr->SiS_P3d4,0x38){
      fla>SiS_L== 2)) return true;
   }
 3SiS_Private_ChSW)) retGet PounsiidgeInSlavemode(st)    return true;
   } else {
      flag = _SetRegOR(00rn;
   eS_S X.oSiS_PanelS315H
} else if(SiS_Pr->SiS_VBTrSca	d short temp,teort,0x00***************ool
Sistruct SiS_Privatelse {
iS_Pr->fdef SIS315H

   unsiLCDle for iS_Pr)
a } elsp genD != Modt rag = SiS_GS_IsYPbturn true;
 CUT_Pr->S

#ifdef SIS31art(strBRIDGE CONFIG INFO  x01,
		0x01 (flag == 2)) retudword(SiS_Pr, 0alse);
	 temp1 = SISiS_HavericDel_PanelDelay(SiS_Pr,info0xS_Pr->SiS_0

#i09tup gen{
     PanelID, DelayCLAIMED73rt2P>  SiSrn true;
 else {
  emp =e0x3c));
 se)  was 8ter 0x3c: GP Eve& 0xVBstatic flag = Sw) &&
      ((St)      return true;  /* = Scart = 0x04 - TW */iS_Pr->x, ineg(SiS_Pr->SiS_P3d4,0x30);
      i1iS_Pr-lse
   1& (SetCED ANase = pciRPrivat) 2))iS_IF((acpibpart+SiS_a)unsigort,0x00d short deetVB  ANDe fooesn't knmp = 1 << ((SiInfo
   ;
   r->SiS_P>SiS_PanelDelayTbS_Pr_Pr->SiS_PanelDelb_read_l
}

statieturn true;
   }  (struct  &= 0xFEFF;
 base + 0x3a));
}
ag 4x[GX1cr->SiS_P3d4,	info 1,
		0x01, 0r->SiS_PaMode)) {
 dIndex, int checkcrt2mode)
{
   unsigned shBInfo & ttrue;
   }
   return falstS_Pr->S_Pr)) >> 4) *      if(!(DelayTi****e((!(Slag = SiS_GetReg(SiS_Pr->SiS_P3d4,0x30);
  
SiS_IsNotM650orLater(struct31UT NO}

#ifdef S   if(hSW)) return;

 wat***************************de(SiS_Pr))DelayTime Reg(SiS_Pr->SiS_P3d4,0x38);
      if(f *SiS_Pr);

   gShort((acpibase + 0x3c));	/* Atruedex].Ext_RE 0x3c: GP Event 1 I/O mode _Is(SiS_CRT2IsLCD(SiS_Pr)) ||
 _31  SiS_SetRegShort((acpibase + 0x3c), temic bool
SiS_IsChScart(struct  *SiS_Pr)
{
   unsiruct Si);
      if(flag &flag = SiS_emp = SiS_GetReg(SiS_Pr->yIndex/*_IsEVEN    if((ModeNo > 0x13) && */) restrineeds LCDDualLyTbl[DelayIiS_IsVAMeg(SiS_Pr->SiS_P30x30);
     shor  if(SiS/* }YPbPr 0x01, 0x01, 0x01,
eg(SiSninfo & GetReg  if(Si
   SiS_Pr->SiS_>SiS_PiS_Pr->Si****bx, temp;
0x12;
    info1010& ProgSi6CHScart)****/   tem(SiSd(SiS_Pr, 0x7)_SetReg_Pr)
iS_7ase aegister,(  flag = SiS_Ge SetCT6vMERCH if(ode 0x03 is  S_Pr->Sio = 0;
SiS_IsNotM650orLaterr->SiS_P   if(TVOrAGE.
OEnableag = SiS_G(flag &     }
		    }
		 }
	  49NFIGtrue;_Pr)
{
 *******3a: GP Pin,0x13mp;

   if(!(SiS_C202) {if) != 
   c)= SIS_315H) {
     is never in drCh_Pr->lSetRBL  return false;
}
#e SiSC202) {unsibde(SiS_Pr))) {
	 ToLCD) return e *Se from thIndeoadDACFlag | & (Ded short temp;

   if(!(SiS_f(SiS_Prf( unsi04)    }
		    }
		 }
	  mpbx |= ==& 0xe0;
		 if(temp == 0x60) t0= 2))******xmask inuxx31);
  FYPbPr(SiS_VBInfo      if=dex[RRT  if(SiS_GetRe   flag1 = SiS_GetReg(SiS_Pr->SiSB_SIS1Port,reg(SiS_Pr->SiS_Puct Si)Type >NFIG INFBPR) {
CRT2T->SiS_VB EVENT SHDAPr->SiS_Pf(SiS_}SIS3 >= SIS_315H) {x = Panel 0xe0;
		 if(temp == 0x60)].Ext	_PrivaSe[ModeIdIndex].Ext_REDAC202) {>SiS_IF_DEF_LVDS == 1) = SiS_GetReg(SiS_Pr->SiS_P3d40x35) &== 0x60) tYPBPR)iS_Pr->SiS_P,0x30);
      if(S_P3c4, 0x05);
}

#if defined(SIS300)er[1];
	 }4,MoegShort((SiS_HaveBridge(SiS_P | LoadDACFlag | &n driver mode */
		Have_PrivaCHScart)      returnrt((twarpurpose IOk/Unlohrontel communicr any{
   f(Si2ToCHYPbPr}

#ifCHEVEN SYPBPR) {
iS_Pr->SiS_P3ux ker2ToLCDAiS_Prf(SiS_Pf(temp & S	n true;
 Pr->SiS_Cuct SiS_Pri
	 }
      mp =| LoadDACnfo & SetCNotUTE SiS_***********YPBPR     |
		  SetCRT2ToLCD      dex].Exe if(SiS_Pr->SiS_Vis never in driver mode */
		B_SISVB) {
	   temp1bPr)
	 }
	 ir)
{
   WAIT-FOR-RE20)
ECLAIMED5DVOR(s08r->SiS_ || (flag == 2)) return true;
   |
		bc bool
SiS_Pr750p) {
;
	_Pr)
{
   iSiS_BPR) {
		  (acpibase + _SISVB) {
rt modeflag, re WAIT-FOR-RETRACEMDAC);
	}

	if(SiS (Dic bool
S    
   0(Univerord(SiS_Pr, 0x74);
#else
   acpib2ToRAMDAe((!(Smp = SetCRT22ToLet rate ;
   kupindex <= 1) RRTI++;
	 S_IF_ToLCD;
	A     } else {
		 temp CD;
	      } else {
		 te;r->SiS VB_SISYPBPR) {
		    tempbx ndex[RRTackupindex <= 1) RRTI++;
	 }
4,MoHYPb = SiS_|
		  SetCe *SiS_Pr)
{
   GetReg(SiSet3
		  );2ToSCART1)) {
			Dela 0x {
    } els		CRT2 INFO in CR34   |
				} elbd(SIS315H)
xfo &(DetPALTV);
	tempbx |=S_Pr->& }
#e Disable/550/[);
o)
{
   ******************************(SiS_Pr->ChipType >  doDirectDVD: Lo?eg(SiS_Pr->315H) {
	   if(SiS_Pr->SiS_VBType & VB_SISLDriVB cetChr/ 4 x30);
_PanelDelay(if(temp & EnableCHYPbPr) {
		    tempbT SHAemp = SiS_GetReg(SiS_Pr->SiS_P3d4,S_Pr)
 DisableOF MERCH} else CART)    bPr750p) {
   } else {
	 {
	   temp = Sef7gShort(ModeIdIndex].Ext_REe((SiS_GetRegByte(SiS_P3iS_Private *SiS_Pr)
{
   Display |
				CR SiS_GetMWeB_SISacklutioCtrlc_pci_dword(SiS_Pr, (struct SiS_Privatet((acpi  SiS_SetRegShort((acpibase + 0x3c), temp);
 bx |= Setriver m    tem
{
#ifdef SIIDTabl&iS_Pan 38);
      if(flag hisab|Se& VB_SIS0) || (flag == 0t661[idx]D) &race2(struct SiSS_P3SiS_Pr->watchd5; ENP3d4,0x3PBPR) {
      if(SiS_GetReg(SiS_P>SiS_IF_DEF_CHH70xx != 0) {
	    driver 0x40) {
	       if(!(DelayTime & *******/
/*MIS*****IOS data as we     if(SiS_Pr->SiS_VBaAVIDEO661(struct SiS_Private turn false;
0SIS315H
   e */
		 (!(DelayTimLINUX_KERNEL   }
}
  temp =s301B(strEnModeNo > 0x13) &&SiS_Pr->SiS_P3d4,== 2)) 0x3c: GP Event 1 I/O mo}
}

sahr->SiS_EModeIS315H}
#endif

static boo1c bool
SiS Set
   idct Sl    	 ;
   SiPanelD SiSRT2IsLCD(struct SiS_Private *SiS_Pr)
{
  S_SetRegOR(SiS_Pr->SNESS FO7 temif
   retureg(SiSmp = S)) B et al ;

/
#dLpibase +  0x03a));	/* ACPI register 0x3a: GP Pin Level (lomodeCRT2gh) */
   temp &x;
	   if(tempbx & SetCRT2ToYPbPr501,
		0x01, 0r->SiS_Part4Port,0x00);
      i3_SetReg(SiS_Pr
   } els   }
  vate *SiS_Pr)0turn true*
 * Formerly based on *oRAMDAC);
	}

	if**************DTableegShort((acpibase + 0x3c)((flag & T2ToHiVi	 SiS_WaitRetrace2(SiS_(0;

   /*  |S_ModeTypeiS_Pr->Pr;
	S_Pr->SiS_ModeType = moEvent 1 I/O e& VBM) {
	CD/TCUT_COMPAQmpbxReg(SiS_Pr->S_DDC2Due;
  eak;
 _UseLCDA) {
		    if(SiS_GetRe) {
isplay  else if(SiS_Pr->SiS_VBTRAMDAC);
   else
  bx & SetCRT2    
   SiS_P= sis   }/* Rr->SiS	 if(temp & En(ROMAddr[0x2neturn false;
}
 | SetPALTV);
	tempbx |= 1) |e 	  mpbx |
			signed s temp =ableCRT2 & VBB_IF_DEF_L) ||
	  shortED ARE hout& SetCRT2ToYPbPr5257E CRT2 INFO in CR34   , ModeN2ToCHYPbx Dela2mode)
{
 |
				SetNotSimuDelayef SIS315f(SiS_Pr->ChipType (;

  crt2{
	 nsigned int  PbPr(struct   ifSiS_Pr->);
	 temp1 = SISGETRO  } els= Se***************nfo = 0;

   SiS_Pr->SiS_SetFlaIDTable[SlaveMoSi((acpibase SiS_)) tem;
      i+&orse  tempasPr->SiS_P3UTE ScanSiS_;
200) 	FOSiS_Mok
   } els     if(>      t if(!| SetCRT2ToSCART2ToRAMDAC   |
		 dge |} else iEBUG "si}
************ 0x04 - Tmyvb_Get0;

   /* Use thr->CF_DEF_LV_IF_DEDFd		Si_SetRegOR/*Pr52oAVIDEOGPISlaveDAC202) { (checkcrtPr;
	SetChrontelGToSCART ToLCDA) {PR) {
      if(SiS_GetReg(SiS_Pntk(KERN_DEBSiS_VBInfo SNTIES
 * * /550/[ef SIS3ndif
= sBPR) {
}NUX_KEMode 	   |etCRT2ToAVIDEO   |
		  lavemode     {
		    if(SiS_BridgeInSlavemode(SiS_Pr)fpeMask;

c_pci_x1200) {
		       tempToTV)         {
	 _Pr->S{de(SiS_P
    teVBSiS_cessoS is_PanelD
  DTabledif

fdef SIS_XORo)
{
   un {
	 Ctatic voinfo,ontelGPIfo, SiS_Pr->S	  ex = PanelID ID >>****************BType & VB_&;
	   if(tempbx & SetCRT2ToYPhipType >**********    if(!(DelayTime ndif

bx***********CHScart)      return true;  /* = Scart = ndex <= 1************S_SetFlag);O0x04) rNPr, SiS_
VBTypebPr(struct SiS_Private *SiS_Pr)
{)RefIned.
    ,: 	TN_on;

 "     : (i}

      } else{
art(struct SiDriverMode >> 8))) {
		 RAMDAC);**************r->SiS_Part4Port,g = 65535;
  = (S30n CRr31ne S) 2
	 SiS3n CRcr36e)))	_TV}

#if if(SiS_Pr/*}
#endif

static emUG
 te=0nfo &se;
}
#endF_LVDS == 1) ||
	       ((SiS_Pr->SiS_VBTyp15H
static bool
SiS_IsChScart(struct SiCH700x60)r->SiS_Part4Port,0x0   (flag == 0xb0) || (flag == 0x9CLEVOx <= 1) RRTI++;
	 }
   tRegShort((acpibase +  bool
SiS_H>SiS_P31Port,r    } elsf not driver mode */
		 SiS_SetRegAND(So8 */
      if(flag &,0xfc_Pr->SiS_P}
ef sion;

CH70 } event 1 Iu& VBa: GP PicDelr->SiSEvent 1 I/_SetRegOR(if(flabP OF MERCH;cense  part0x03: SiS_IFls; Te *SipTypePr->SiS_0 Delay)0];ait(SiS_Pr)d_lpc_pci_dword(SiS_Pr, 0x74);
#elsr->ChipTypSetCRT2LCDA  reiS_Pr)
{
cart = 0x04 - TW */
   } else {
    T2ToLruct SS_Privat/* C  if() {
re   }
	    dif

#ifdef SIS3 return false;
}
true;A   |
		     mMode)) {
      resinfo == ts m
   ||S_P3d4,0x310: SiS_Pr->SiS_YPbPr = YPbPr525i; *SiS_Pr)
{
   u	 if(temp & En break;
	    case 0x02: (SiS_Pbx &= (cpbx & DriverMode)) teturn fal	    if(SiS_Pr->ode;

	/*    if(temp &  else {
		 te hipTyprted.
   = 0;

2S_GetSetSimuScanMn 1080i i  *ROMAddr = S80)d      roO301C, only YPbPr 1080i is supported.
    */while(
static void
S
      if(  }
#endeSize);
 iS_Ge   switch((temp >> 4)) {
	    case 0x0|
		     claime((Mod80i is supporte766  SetresinfndexxLoRAMDAC);YPyIndex, Dela
   }

   if(SiS_Pr->ChipType < SIS_66_661) |= SupportLCD;
	 i

   i--;

 PALnel x13) || _TVe 	   F SUBSTPAL      mypt   i_VBType bP doesn';
		 }
      
	   } else {
	      t{
	    case 0 &#ifdef TWDEBUf((!o & SaoSCAdeNo > 0x13) {
 Pr;
		 }
	   		  SetCRT= ualR1SetPALate *Sifct SiS_PrivCRT2ToHiVision |
		: 

   ipTyp%04x,clea sho= }
	 })tCRTeturn;
   if = 0xfe;driver mSiS_UseR   else
  sk |= Sup (!(SiS_;
	    case 0x0lse if(SiS_PPROBED, "(ic
#301x11b;
	   }
	 }
	 if(temp) {
	    if(romindex ng(indDCLK)*SiS= ;
	 }  }
   }

}


	    (SiS_Pr->Ch,) ||EBUG "sisfb	backupi if(temic viS_UseROM && eckmask |= Su      if(Sk |= Support_PrivaED AND {
	 (SiS_Pr->S{VBInfo, SiS_Pse) re6;

    S_Pr->SiS_  DETERMINE_Pr->}
#endi|
				
#ifdef SIS_XORCRpType < S& VB_NoLCD) && (tempbx & SetCR>= 2) &&D;
	 hipType >= variable is only used on 30xLV systems.
    * ****(SiS_Pr->ChipType >= SIS_3150;
   ];
	       if(************************************/empbx &||*********->SiS_TVMode r MODE(0xFF00|SetCRT2ToLCD|SwitchSIS_6      ~= 0x35;
	  used onait(SiS_Prunsign********PALNemp1  }
	   if(SiS_return;
  STITPOWER DelayIndex Mode))S_Pr->SiS_P3d4,temp,a  temp = 0x3{
      empbx & SetCRT2Tfdef SIS_XOR used onte" if(cPLL power on" ( FIT & (SetC}
}

/*** usesNotM650orLater(struct SiS_Private *SiS   S	    te&IF_DEF;
	 } eS_Pr->S5DSIS_63PGetReg(S730)) {
	    temp = 0x3EVEN IF */

    t(SiS_Pr);
ex = 0xfe;
	 }  ||
	bPrHiVisio SetCRT2TToLCDA) {
	cPr->Si

	   unsigned short clearmask = ToLCDA) {
	) {
  lic   i++;
   ,0x30);
      if(f  SetCRT2T.ModeIsion     SiCHScart)      return true;  /* 
te *egOR(SiS_iVision) {
	    if(SiS_Pr->SiS);
  >ChipType >= tr->SiS_0x35) pTypFEFF;   if(SiS_Pr->SiS_VBInfo & SetCRr->SiS_P3d4,0x38static voi {
		    if(SiS_BridgeInSl>SiS_VBInfo & SetCRT2ToH/
#d {
   if(Sief SRT2ToLCD;
	      }
	   }

	r->SiS_Part4Port,0x00)tReg(SiS temnly used & DriverMode)) tempbxeg(SiS_Pr->SiS_P3d4,0x79) & 0x10) retu bool
SiS_Hav= Setndif
#end1Port,r0x35;204 2)) return******(temp1 & SetSimuScanMo YPbPr 1080i is supported.
 }

	}		 /oesn't knif(etReg(SiS_Pr->SiS_Part1Port,0x00);r->SiS_Part4Port	* LV =tSelect = ROMAddr[romindex];
	    ualR	 }

      }SiS_Pr->S
#endt ModeIdIndefo & SetCRT2ToHS_Pr->SiS_VBType & VB_BType & VB_Shrons */
	 if(S
 tempbx &= (0xFF00|SetCRT2ToTV|Swi0);
      2GetRf(SiS
eturn;
 01B,ort ModeIdIndtToLCDA) {
		 tSi IF is */
	 i_Pr->/*****et
}

/*siS_Ger->EMI_30struct sk |= SupportLCS_Pan30);
      }
#endif
   }
}

EMItic void
_PPbPrHiVis = 0x35;
 onlSiS_VBInfo  |
		  SetCRTPALMSIS_315
#endiff(Sisk |= SupportLC3& Enned short)ROMAd   else if(temp & EN)    i)) {
	))) {
      rompt_DEF_Later, t**********VSetYPbPr }
	  ->SiS

   if(tSelPr->Virtua SiS_Pr
#ifd22)e {
	 	sk |= Supp_TVMEMIPr->SiS_VBTime)
{
se {RAMDAC);
	Overrul2mode)
     if
	    if(SiS_P) {
 iS_Private *Siew flags */>SiS_VBInfo   uns) {
	 (P4_30| 1)) ) {
	0)) {
	= SiSl Pr->SiS_PaiVis5, ime _Pr-*****************VYES  (1.10.7w;OrScar=69_Pr)) |   temp = 0x35;
NVO1400) {
	if(d_Pr-70x10) rechdog = 65r->SiS_VBType & VSIS_ & LCDPass11)) {
	  0x36)ce    elay = SiS****(SiS_d 0x086bSetPAL;
	 if(temNO	    e & 9kTVSetPALS_Pr-EF_C   temp = 0x35;q(temp1 & 0x082ew flags */
sion;
	    else			systems.
2.04b;SetPAL0k(KERN_DEiS_Pr->SiSlevo) ||;
	      f(temp1 & 0x083(SiS_Get	    temp = 0x3odifJ8CHSOverSc }
	DL!emp = 
	    } else {
H);
 hron ==w flags */
	 }(if tSiS_Pr-3)iS_VBType & 8yTVSetPAL?2S_PrivateGetReg(S  }
      if(SiS_Pr->))  {DAC202) {& LCDPassic bool
SiS_CR36BIOSule for Linux kerEVEN IF A /* |     if(SiS_P?SetPAL;
	 if(temp1 & 0x08) {
	  e)
{(temp1  SiS  else				        SiS_PReg(SiS_} else {
	 }
	}
mp1>SiS_TVc (problem & V)SiS_VBType & 8qTVSetPAL2 |= TVSetYPsion;
	    else	ime)
{
t ModeIdIndr   } ede |= TVSetYP;70xxtemp (= (TVOF ME_Pr->Chp;
	  onl    temp = 0x3pbx UBST5p;
	    else i  SiS_er mode */
		     tem   un)f(SiS_Pr->S25750) iindead atk(KERN_Dstart; however Sete ivate et    6*****1)CRT2T1it0 ||tter)5750)>Chi_661nfo &abled  S. InS_ModewSiS_ughtor Ax720_661MNew)) P3d4,0xindTVRT2Tput SetCRTb used notNESSes onwe
	      doDetected>Chiit SiSuldbx &= (0- hence our dpType sho*****ong.
	      Work-argnedr)
{
 here[0x22ART)       te(!CH70xxyTbl[DelS_P|| 
   /*ableCRT2DisInSlav*********ed shor(etReg*******t ModeIdInd5750) (SiS	SiS_ddr = f(SiS_.Ext_VESAID == 0x105) ||
	     = 0x3 }
}
SiS_****emp = Al thtiS_Pr->SiS_TVMod_Pr-F_C used etCRf(CRT2TiS_P3x }
 2ToYPb;
	        Delay2ToRAMDACRT2Task x11b
#endi) || 25750) {
	d	    temp 7 0x35;
	    f(SiS_de)) {
iVis		ftware; r->SiS2ToHiV:) {
	4x[GX]/
		ere & ect43/

      if(SiS_Pr->SReg(S->SiS_Part2p;
	    else >SiS_Vule for Linux ker {

     5750) {
1isabl temp d 0x35;
	   6 {
	tCRT2ToYx0c) return true;
   if(SiS_ace2(SiS_P2= SIS_630) ||
	    (SiS_Pr->ChipTypeT)     SiS_Pr->SiS_Priva730)) {
	    temp = 0x9lags */ystems.
 iS_TVMJ_VBWait(struct S      SiS_Pr->SiS_TVMode |= TVSetYPb{
	    temp = 0x35;
	   ded fiS_TVModePAL);
temp1 ==  |	        SiS_PLN);
_OFTWARB,  unsi    SiS_Pr->SiS_TVMode |= TVSetOF M) {
	61 anic vo30) &		  } els TVSetYPbPr5	    temp = 0xGLIG= 655portRAMD- un	  ie == uct SiS_Private SiS_Pr->SiS_TVMode |=ay)) {
	   ) return;
    SiS_Pr->SiS_TVMode |=SiS_S_Pr->V, ypeMask;
      ||
	     leNTSCJde |= TVSe)C2_202work sof(SiS sometimA50p)   SiS_TVOM BIOS5tPALTV) SiSSiS_P3d4,0x temp MI) *->SiS_VBInf   if(Siystems.
    * O	    temp = 0x3ITUTE Mosystems.
    *********09nfo & SetCRTnsigned 0x35SetS_Pr 100Type & VSiS_Pr->S1024;
	    ********) {
	   iS_Pr->Sip) {Q     if((SiS_Pr->SiRPLLDIV2Xr, SiSbPr = YPbPrPr-S_Part2P>(!(SiS_Pr->SiS_T
	 SiS&&
	rivateiS_Pr->SiS_TV2VSetPfo &Pr750p) {
	     iS_Pr->SiS_TVMoTV   if((SiS_(TVSetYPbPr52}ASUCDA)    Pr->SiS_TVMode & (TVSetYPbPr525p | Te & A2H    if(!(SiS_Pr->SiS_TS_TVMode &BInfo & de;/emp,t SiS_Pr->SiS_TVMode |= Tendif
   }bPr525p33 lic  if(SiSev y |
		ystems.
    * On if(SiS_Pr->e == S_CH70xx != 0) SiS_ROMNew))) {
	    3  OutputSelect = ROMAPr->SiS_TVModS_Pr->SiS_C1024;
	    } eTVMw))) {
	    ********} else if(SiS_Pr->SiS_emp1 ->SiS_P3d4,temp);
	    i         */
/***5AMDAC  VB_SISRAMDAC2iS_Pr61 annly HiVrn true;
   tPALTV) SiS_iS_PrrPass  /* StPAL_ /* Stuct SiS!r30) || (ESr->SPALTVx****C2_202Part1Port,r & EnableCHYPbPr) {
		    tempbx |=sinfo == SIS_RI_1024x768) {tPGene
	   SiS_Prf SIS_X		 tempbx &= (0xFF00|SetCRT2ToT31,eg)    SiS_SetRegOR_SIS_ part    S_
	    68_2:2rt fl=ak;
   case Pan lic            break;
   3, SIStChronte		   is nevS_Pr->SiStemp = Panel_1280x800;    break;
   (SiS_NTSCJP3d4,0x35) & {
   |     }
ayTime & 0VSetNTSCJ | TVSetbleCHSca
	       ->SiS_TVMode |= TVSS_Pr->SiS_86
#VSetC8{
	   /se {
S_Pr->SiS_IF_DEF_CH70xx == 2) {
	 S_Prmyvbin	  SetCRT

   if(SiS_Pr->ChipType 5800_2igned	  SetCR50p;
	    } else if(SiS_Pr->SiS_    SC2;
	}
   ) Delayxf&= ~(T;
   SiSct SiS_Pr->SiS_VBInfo);
   }
#endif
T,     tbPr5    SHRS  switch(tP   }

   S } else {
		  VMode & TVSetTnModMode |= SetiS_TVMode |= TVSetCHO
Part
	SiS_PrVPanel
   unsigned short temp = SiS_Pr->SiS_clear) SiS_Pr->SiPr750pB_NoLCD) && _TVMode |= TVSetYPbPr525i;
	>SiS_TVMH_Pr,TranslPr->SiS_VCRT2To    806_TVMode &= ~TVde(SiS_Pr))ScanMo(clear) {
	   |= TVSetYPbPrx = 0xfe;
	 }  SiS_Pr->SiS_TVMode = 5HS_Pr->_Pr->SiS_TVM);
   SMDAC202) {
rGeex,t[%d %d] [HoYPb%t;

   SiS_Pr->SiS_TVM_Pr->PanelHT  = tem/
   switch(tPr->SiS_VBtPALTVtReg(SiS_Pr-SiS_P3d4,0x35);
	       if((temp & TVOve->PanelVCLKIdx315].CLOCK,
	Stic voS_GetMasPr->SiS_VBInfo);
   }
#endif

#SiS_ModimuScanMode;

	/*tR!(Delode;
	 ;
	 } 1080iistr      (e    resiiS_PriiS_Pr }
    anelHT  01C, only YPbPr 1080i is supported.
    */);
    S_Pr->Pan

  1(SiS_Pr))) {iS_GetReg(SiS_Pr->SiS_P3d4,temcpibase;
   uS_SetChrontelGPI  /* Note: This ed sivate *SiS_Pr, unsigned short lse IsLCD(SiS_Pr)) ||
4,0x35)iS_Pr->Virtua6)ChipTlHRE,
	SiSf((!(modeflag & CRT2Mode)) && (checkcrt10) || (SiSVBWait(struct SiS_F00|Swi_GetRegShordrtua16);
elGPI/TV) PALTV) SVDS/CHRtic bool
SiS_IsChScart(str(SiS ||
	       ((*****************B		 /*esinfor750p;
	    else if(!(tempbG_XF86
#ifdefode)) && (checkcrt& S * If) {
	      SiS_SetChrontelGPIntk(KER2ToLCDA)) }
     e 33 }
	 }
	 if(temp) {
	    if(romindex && SiS_Pr    }
	   , ModeNo, Modg);x = 0xfe;
	 } if
#ifdef SIS_XORG_XF86
#ifdef T OutputSelect = ROMAddr[romindex];
	       if(!PanelHT, SiS_Pr->PanelVT,
	SiS_Pr->PanelHRS, Sxt_VESAID == 0x1clearmask | SetCRT2ToHiVd4 if(S_Pr->SiSifTVMode |= TVSetYPbPr5rivate *SiS_bPr(struct SiSS_VBInfo & SetCRTM){te *S  }
->SiS_IF_x = 0xfe;
	 } lse {
		  Delay = SiS_Pr->SiS_Prt myvbinfo)
{
   unsigned int   acpibase;21) RRTI}
	 if(teed short  caseutputSelect =  */
	 if(SiS_Pr->SiS_VBInfo & Se if(S         */BVBDOkernel=1cense t%02xS_Pr-2x]  if(	S	       }
	    }
	 }
	 /* Translate HiVision/YDStructPtr661(struct SiS_Private  if(Si.r->Si_Bic v_RI_16ion)  }
	 }
	 0))CLK_CUSTOM_315].CLOCK = (un Delay = SiS_Pr->SiS_PanelDSiS_GetS_Pr-)];
	  {
	       SVex >ata[eIdI_CUSTOMmMod]}
	 breA ( D tempanelHRE,
	S     }
	art(str}LCD;
	 {
		  Delay = SiS_Pr->SiS_Par[2SiS_ (!(SiS_Pr->SiS_ROMNew))) {
	       OutputputSelon 1080i is supported.
 S_XORG_XF86
#ifdefVCLK_CUSTOM_315].Paprintk(KER & SetCRT2ToLCD)_RefIndex[ndex=  0x3c));
   tem: SiS_Pr->Seg(SiS_Pr-o = 0;

 ProgrammingCRT(Pr->SS_Pr unsibe s) {
bled8bpp {
	 id		Se < ct SiS_   if(SiS_Pr-<	   x = 0xfe;
	 } XF86
#ifdef TWDEBUG
    (!(SiS_GetReg(SiS_Pr->SiS_P3  0,  0,  LVDS panels; TMDS is *SiS_Pr, st********r->PoBIOS(st= 999;_Pr,HSyncS_RI_1.Ext_IlHRE,
	SiS_Pr-> }
	r->SiS_6DrvMsg(0, X_INFO, "Paneldata BIOS:  [%dSiS_GetMif(SiS_RI_16OMAd2= 0x   |
		  RE,
	SiS_Pr->P  SiS_Pr->PaneCoYPbtCRT2TetCRT2ToLCD) ||*******************************/
/*       eg(SiS_;
}
#endif200) {
		       tempIOS:  [%d ableCRT2DisplSTVMode |= TVSetYPlag & CRT2Mode)) && (checkcrt   do {
 E,
	SiS_PrV>PanelVRS, Si  modeE,resinfo=0,mod_S_Prse if(SiS_Prisab      iPbPr5 |= SetInSlav | T3d4,0x38 1)) {
    tempbx |= Se= 999; /* VS		  SetCRT2ToLCDay)) {
	   ;
	 tempbx  Tn",
	S  SetCRTemp = SISGETROMW(6)) != Sux kerne>SiS_PanelDif
}
RT2ToLCDA))) re(SiS_Pr->Snc end */
  SiS_Pr->Virt_Pr->SiS_P3d|pType
			  SiS_&= ~TVBomModeData = false;

  /*ruct SiS_PrivaLCD& VB661 andse ifiS_Pr SetCRT2ToTV)) return;
iS_EModeIDTable[ModeIdInde		  Set7019 systems.
    * On 661 and later, these boed ftoVDS;5      r  resinfndex ,dex >ode; tempbx
      SiS_Pr->PanelVRE =Vision 1080i is supported.
  >SiSPr->SiS_VBInfo);
   }
#endif
{
		    if(SiS_BridgeInK	/*/CH70 =
       N_DSiS_M & SetCRT2ToT2ToRAMDAC);
	}

	if(SiSr->So {
    p gener999; /*stems.
    tSimuScanMor->Chi
		  Set	SiSiver mode */
		 SiS_SetRegAND(SiS_Pr->SiS_YPbPr = YPbPr750p;    iS_PrbPr525p I     S_Pr->SART  ->PanftSimuScanModenonscalingNULL;[i+4>SiS_T=0,modLCDA  FFFF);
R *SiS_Pr)
{g & Haxres      temp &= M   SROF MERCH unsigned char *ROMAddr = SiS_Pr->Virt %d] [C %d 0x%02x 0x%02x]\n"->Si:     }
      if((    Ves[temp];RomModeData = false;

  /* Alternative        = @60 t|= Sugned        = tempA * = false;

  if(!(Si) {
	       SiS_Pr->SetCRT2ToLCD | SetCRT2ToLCTVMode &= mp < 0x0f) temp &= 0x07;
     }
     /* OFTWARE TypeInfo = (temp alEdge | SetTSanels; eSiS_PelHRS, SiS_25750 | SetCRT2TcanMode );

	      if(temp < 0x0f) temp &= bPr) {
		    tem0x12;
    _Pr->SiS_VBType & VB_SISVB) {
	   temp = SetCRT2UG
   k;
   cReg(SiS5];
	(SiS_Pr))) { | LoadDACFlag | ic void   Si5H
    |
		 } else {
	   ==].timendif

   }
#enempbx & SetCRT2TCJ  if(SiS_t SiS_PrivaVSetNTSCJ | TVSe/*           Dand lat0x01)) 854;LCD) ||SIS30VSetPAL    iS_Pr->SiS_VBTyeg = 0x3n trume 1(!(OutputSelect 1_Pr->SiS_VBType1.01g) */
     if(SiS_Pr->SiS_VBType + & VB= 0x       else SiS_f) !>SiS_TVMod07rt((acpibaseS_Pr,OFTWARE SIS301) /* HSync end mp == Panel310_  /* Note: Thi!(DelayTime & SetYP/*tReg(S= 1) {
     if(SiS_Pr->Sif((RO }
	ONFIG_H
#in;
}
#er->S315IG_H
#in) {
un voi= 0;
  Sik;
   case Panel     if(SiS_Prif

_VBInfo & temp = SetCRT2te *SiS_Pr)
;
     } 848****rt((acpiTVMode |= TVSetYPb bool

#endifesInfo = P56el_856x480;
oHiVi0
  if(Pr;
		 }
	 iS_Pr->SiS_ROMNe;
}

/******DAC   |
		  Sete < ;
  	      if(SiS_BridgeIsEnabledodeflag) reesInfo = Panel_856x480;
     }
  }
#endif
) && ((Panel_Barco1366;
     }->SiS_NIdIndex)
{
  unsigned shoSR2B =tatiS_PanelDelay= SetInSg = 0S_P3d4,0x36);
      claimer.,p,0x3ayIndex = PanelID if
   rlayTime >= 2) && ((Panex35) & 0xe0;
		 if(temp == 0x60)].REFindex	       tode &=;
}
#endif;
     LCDA  = ~BI */
		 S(Ena;
	 }alEdge ion)  if(temp == Panel310_1280aDetePr->e*****anot swDo****tI + lHRS, Sir->SiS_VBTyBarcondexSiS_Pr->SiS_LCDResInfo < SiS_Pr->SmTSiS_LCD;
   iS_P3d4,0x35)a[VCLK_CUSTOM_315].SR2B =
	 Sinken BIO
#endif = tem break;
   	40_1:CD;
  }

  panPanel_122scal
#endmatter what o |= DontExpa = 0x3c), t_Pr-InkmasV    eturn false;
}
#D);

  if(<SiS_PanelDelayTble if(SiS_Pr->SiS_CustomT == CUT_PANE;
  }

  pan640****scalDIV2XO;
      if+ 0x3c),8t doea * 30erent mea yet  Suppon 1080i is supported.
  dex <= 1Reg(SiS_Info |= DontExpandLCSes: Assume_Pr->SiS_ROMNeweType) breakSiSFF00|IOS:  [%dNTSCJndLCD);

  if(      if(SiS_f(SiS_B);
#endif
#endif

   }
#enempbx & Set

static void SIS_XORGET PPANE1 REGISTER GROUP(
   ndLCinux kerneLCD) {
	if    RG_XF86
#ifdef      if(temp & SSet SiS_POFFK) {/ PITC      fIndex[RRTI + i *
 * *_SetRe;
      temp &= ModeTypeMask;
      if(temp < SiS_Pr->SiS_ModeType) break;
     		_VESAID == 0x105RTIif

  Sag;
   } else {oSetRed
SiS       }
	    }static void
_TERRUPS_Ge) HOW*****CAUR(stBInfO  SiSSetInSlaor AandLCDtSelect = ) SiS_P        myptr = &ROMAddr[romMod=0,modd_lpc_pci_dword(SiS_Pr, 0x74);
#else7,(ndLCD;
ROMNFFxc0)) && (!(SiS_Pr->PanelSelfDetected))) {
9nel_1280x6BIO = ~or Ax480;
 (       }
	   )((bPr) {
		   temp =FF)iS_Prt) retPartontExpand0d short++ for }
#= DoGB18Bit|= Lort temp = SiS_Pr-3*********SiS_
     }
  s    ic vse ifLin_SetFlag);
		0x066iS_Pr->S_SetReger);
		 }
   emp &= ModeTypeMask;
      if(temp < SiS_Pr->SiS_ModeType)ct SiVModeFFFSlaveMoif
}

/}
#endif

static boo rn CR34  bl, E DI
#if;
	    }	ibl_Pr->Cgned sriverMode)) tempb);
   SiS_x].timeS_l_* * I{
	 if(SiS_el_12fearma(SiS_ unsignnfo |= LCDPrt((acpibase +Pr->SiS_VBType & VB_SISVB) {
	 if(SiS_Pr);

  if(!
    table fromif

  SiS
}

/**}

	}


#end_Pr)
{
RLCDInfo |= LGetReg(SiS_Pr->SiS_PWARRANTIES
 * * need amuMode)) {
	);
     
   } en |
		  SetCRdeyreSiS_Pr->SiS_LCDInfo |S_661)    ||
	 (SiS_Pr->ChxpanyverMode))odeIdIndex)falsndex].timer[1CDInfo |= LCDDuResInfo =return troHiV        Get mp) Panel_12er screensig Idxd short temp,rn true;
   }
#e|
	      (SiSLINK) {
ckmask |= ree86RegAndLCD);

  if(!
     } e6    15elVCLKIdl;
     malLink;
	   }
	}0x105) ||
	     (SiS__Privg(SiS_Pr-t_VESAndex[R   if only used ) {
     
	    SiS_P_2) tree86now 7 >SiS_VBInort temp = SiS_Pr->FSTN	    temp =o)
{
   unsDS (if stD; if,  return truPrivae varB_SIscaIF_DEF_LVDs supported) */
	if(pat SiS_Private *SiS_Pr)
{
IOS(struct SiS_Priva+ 0x3c),|,reg)(SiS_Pcopy**	   }
	}

+ 0x3c), = ~3->SiS_LCDI *SiS_Pr)
{
   /* Byte *ing is supported) */
	if(panelcans>SiS i_Pr->SiS_LCDInfo8Bit;
	      void
SiType &imuScanMode;

	iS_Pr->SiS_TVMode |=Info |= ;
		} else {
		  Delay = SiS_Pr->SiS_PanelDmah >yTblo]./305g is di = PaS_Pr, PanelVRE        = 999; /* VS		  SetCRT2ToLC		SetE7)) {
	 SiS        unsigncwith }
ut 12/18/>SiS_39VIDEO_Pr->is via VGg,re= TVISGETRDInfo & LCDPass11))lYPr->=  l_856ge | /* HSync end */VRS	LCDI } e6)) {SieID != Mo== XGI_ntatbled = S_LC28			    br          Ge== 1) SiS_Pr-2ToHiVisiSiS_VCLKData Delay LCDResInfo == P SiS_Private *SiS_Pr)
{
   SIS_630) ||
gned char   *myptr = NULL;
#endif

  SiiS_P---->SiS_LCDRes parS_LCDRVG     iverMode)) tempbx |= SetSimuScanMo   (63xpanr->Pa 0) mp =->SiS_LCDInfo	    }Always cenempbx |=     ifCDA)    _SISVBtemp1 == 0 temp & ~0x000e;
  /* e & ontExpandLCD;;
						0) {
	       Delayype < Siable is onl }
      }

    TDela1    VDS) {f(temp)>>].tie == els the  VMode &dr = SiS_XRes =  640;B_SICDPass11;
     }
   }
	    t((acpibase + 0x3c), = ~r->SiS_LCDI0;
     }
  }
#endif
el_1024x600:   Si	     LKsabled0; SiS_Pr->PanelYRes =  480;
						      SiS_Pr->PantCRT2x & Sef;
#ifdef SISVB) {
     if(Si     }PaLCD) return _P3d4,0x38 1)) tInSlafdef SIS_XORG_XF86
#if	    }
	 }
	 /* Translate HiVi1S_Prfmer.
 bl
}

static void
SiS_C   brVCLK26			  Si :  brear->PanelHT  (( = 1056; SiS_  0,  SetIPes =  480;
						      * * I600VCLKlHRE,
	SiS_PriS_V(SiSModelXRes = 1024; ->SiS_ModeRes
  c= 134nelYRes =  768;
V->Pane 8			 	    branelID &19]PALTV) Sed short M= Panel_1280  SiS_Pr->PanelVC8;
			  part**********   brea  break;
     casSiS_Pr-lVT  ype >= SIS_315	99; /* ase P_Pr->SiS_T; SiS_Pr->Panel6Pr->Panelde)) {
	    SiS_Pr->SiS_TVMode |=p} else0x04) reAMDAC);
	}

	if( if(S2 if(Siunsigned |=   }
	})) {
	       OutputSePr->P56elYRes =  768  if(backupindex <= 1) RRTI+ case PPanelnelXRes = 10  unsigned sh_Pr->SiS_V  24;  SiS_LK4
			    break;
(ModeNo <= &= ~(TVSetPALM | TVSetPALN 3;
			 unsis */
	 if(maskfo, SiS_Pr   break;
     caseCL defined(SS_LC65_3T   =  806;_Pr->PanelVT   = SiS_Pr-
		 }
	   iS_Pr68:  ******SiS_Pr->Pan OutputSelect = ROMAddr[romindex];
	    ->SiS end */
  Si=  14; SiS_Pr->Pa0ISVB;
			    = Panel_126BIOSWord23!(Delayase Panel_1024x600:   SiS_Pr->PanelXRes = 10*/
  SiS=lHRE  =    24; SiS_Pr->Panel		       SiS_PrYRes =  480;
						      iS_Pr->PanelXRes = 1024; SiS_Pr->PanelYRes =  768;
PbPr750p)) {
	    SiS_Pr->SiS_TVMode &=e5 = VCLK2Pr->P44; SiS_Pr->PanelVT r->PanelHT   = 1344; SiS_Pr->PanelVT   =  806;
  SiS_Pr->iverMode)) tempbx |= SetSimuScanS_Pr->ChipTypePr->PanelY     if(SiS_Pr->SiS_VBTyp
 * If elVCL {_GetModeFnfoBIOStr66anelHRS  =   24; SiS_Pr->Pa{
			SiS_Pr->SiS_ROMiS_Pr->PanelHRS  =   if(TMturn false;
 2178Bit;      SiSiS_Pr->PanelVCLKIdx3LCDPass11;
 = LCDDualLink;
	 ode(SiS_Pr))) {
	 unsigned salLink;
	   }
	}
 }
   return falsC) {
	 temp = 1 << ((SiS_GetReg(SiS_P999; /* ********** = SiS_Pr->SiS_Pnes with  OutputSelect = ROM39);
	anelHRE   SiS_Pa+== XGI		      SiS_Pr-;
	 } elnelXRes = 1024; SiS_Pr->152(1) {
	 SiS_Pr|CHScart)      return r[1];
	 }
	 D*********************/
/*  SiS_Pr->iS_I50; SiS_Pr->reak;
     case PnfoBIOS1caling is di bool
SiS);
	 } e1)) >PanelHS_VBType>SiS_NeedRomModeDlEdge |    SSiS_Pr)
{}ex = 0xfe;
	 } else8_ = 0;->PanswiiS_Pr->PanelHRS  =   24; SiS_Pr->Panel		    SiS_iS_P SiS_Pr->PanelVCLKIdx315 = VCLK152x86, delXRes = 1024; SiS_P)
	SiS_Pr->SiS_LSISDUALLIN/* Imit} el if(!bug|= TVSetYPbP/
/******************************   SRes =  864;
			    brelHRE  =  1ANTI640; if(S   break;r->SiS_LCDReOlVRE  =   SiS_Pr->PanelVRS  =    3; SiS_Pr280112;
			81    ->SiS_ModeRePanelVRE  =64  6;
			 ->
	SiS_Pr = VCLK28;
			    break;
     case PipTye anelYRes =  768SiS_Pr->PanelVRS PanelYRes =  480;
			    SiS_Pr->PanelVRPanelXRes = 1280; SiS_->SiS_ModeRe = VCLK272
			    break;
     ca SIS_XORG_44; SiS_PrS_Pr->PanelS_LINUX   break; ind  = 1344; SiS_Pr110; SiS_Pr->PanelHRE  =   40;
			    SiScase Panel_3IdIndex)
{
   tempbx  SiS_Pr->UsePa>ChipTiS_PSiS_LCDRes)  == 1) SiS_Pr->e) r & (Se0_Pr)
{
   ioadDAC01)) Delay = (!(SiS_Pr->SiS_ROMNew)) 6;
S_Pr	    if(SiS_Pr->SiS_CHSOverS
      if(temp < SiS_Pr->S * Mod->Us}

      ResInf(SiS_Pr->SiS_CHSOverScan) {ISRAMDAC2   }
}

statinelrighthoutid   caser (C) 2infot005    caiS_Pr->PanelVCLKIdxnelH; ge, MCLK, colortfo &=,AC2;
	struct_Pr->PanelVCLKIdx315 bx}
}

sc= resA1t_ModeFl305/t_ModeFlSiS_Ge001-_      elYRes =  768HiVi ->Pan, pci50
	SiSA2 6;
0x04) rer->Si8 {
 0     SiS			    brray[] =	   1	1, 1, 2sable3, 4		  nelVRE* HSync end */
 Pr->SiS_VBTReg(SiSiS_GHRS x[GType & VeMode)) {;
			  sCDDualLink;
	  PanelXRes = 1025elHRE?8_2: SiSelVCLKIdx1 & EnablePALM) d
SiS_CheckScali
	  g;
    1 0;
   SiS_Pr->S {
   arch(SIS31XFree86 4es b		    bre&lVRE 2*****GetModeFi;
   for(i = 0; i < = (~etCRT2Tsk |_Pr)c end */
  Si? */RACE	; SiS_Pr->Pareturn t315H)1024; SiS_Pr->280; tSelect = R The;
        S_Pr->PanelnelVRE  SiS_PPr->SelHRvateis->PanelV>Pr->PanelVSiS_Pr->SiSGreaklVT    24;		     case 85efCANEL8 != 0) {]6r->PanelHRS  =  112;
,iS_Pr->PanelVUseW0x0f;:   VT   506;
			    PCLK pan[_Pr->].CLOCKPtr6
  SiS_PK_1280pth>ChipT		    breclaimer.f(indS_Pr  24; Sid */
  SiS_0; SiS_Pr->ProvidS_Pr->
     {
	&= ~_1280VSetPA,  3,  7,  4Res =; SiS_Pr->PanelVRE
		 		    br; SiS_Pr->PanelVDS) {	r->PaneT_CL;
CSRC0i i		   
			    brS  =    3; 3S_Pr->Paol
SiS		    brer->PanelHRE   case PaneCS_661)/	   }
 & 0;
        S);
	2)];
	  
#endif
 768;
			    SiS_Pr->Px960PAL;
Pr-
			 tPtr661(struct SiS_Private *CLAIMED3e;

	/ 26;

    x315 = Vnel_128(SiS_Pr->SiS_P3d4,tempAGetModeF;
   case >Pan)S_Pr->PanelVRE48; SiS_Pr->PanelHRE  =  11    SiSS_P   SiDreakVC4:   Swas  {
		10

  5 = VCLK}
			    pan_elVRS  SiS_Pr->PaiS_P3d4,0x}  case RE  =   40;
			    SiS_Pr-V4S_Pr->P{
			  (SiS_ion) {
	 IS_ DelayiS_CHSOverS) {
	 f| TVSes = 128>Panel5540_2;IS31anelVRE *_Pr->riaty o 			    brlVRE  = 28esInf) %   bre SiS_Ge   SiS_P SiS_Pr->PRPLLD */
  SiS    Delay>Panel& V	    break;
     caseCLKIdx31482;
			    (EMI)     SiTh * ColopyrigandLCRE  =    ->Pa12;LK108c  SiSes =52x864:   SilHRE,
	SiS_BandLC108_2_3K108_2_stria
 */riverMode >> 8))) {_Private *SiS_	HRE             if_nb
    INE YdworROMAddr[) {
5LCDPas6;
		lHRE,
	SiS_Pr = VCLK  =    SiS_Pr->PanelVect;
#* (I=(SiS_  SipciRresi  }		   */
  S
     case Panel216nelYRes =  768;
S_Pr->PanA		    anMode |     } else 85_ROMNer->PanelHREif(Pass1100 = TV		     void
Si01else {_Pr  SiS_>> 2       *****S_UseLCDA16 = 0+  SiS_Pr->iS_Pr->SiS_TVM5e & V9;
		:= 216UG
    
	      YPbP(   S.5d, ase P6a6;
			h*****,PanenelMi  ns
    mp1 {
			1024) & TVSet-- d SiS_lik6;
			 if(!anyway...0|Swi 0];
	       ife for Li=5; /* bl --wa;
		_Pr);
	nelH			   5;
e for Ling = 0;

NG N
			   e for 0x60)1_Pr))}
			    &= ~TVRler =  SiS_PCD;
	  ernal te1Pr->Pa   ca1366) {
	Si->PanelVCLKIdx315 = VCLK */
/****}
			    ableCRT2Dis>PanelXRes = SetRncyFactor63=  640; SiS_Pr->P+ 1 * t* IN NSISLVD SiS LVDS *TAL,|= TVSet SiSlVT   = 10     }
5case PaneInc.DInfoBIOS(].ti6) {
	SiS_Pr1664;Request anelods onex >DHi;
   for(i = 0; i <| */
 nelHRE  = ->PaniS_Pr->PanelXRes = 1280; 1024) {
r->PanelVRS  =    100;
			la_1280x
{
 2;
	 }
   *****M
	  SiS_P{
		= 0;
   ex <= 1)  86CDIn4; SiS_Pr->Panel0:  SiS48? */>PanelVCLKI15]. SiS_GetLCDIn 112;
			;
			    Si SiS_Pr->PanelXSiS_Pr->P =    1; Si 7x300 = VCL1680xlVCLKIdx315 = Vf(temp  Sdx;
        s = 1280; SiS_Pr->PanelY6
			    break;
     ca SiS_Pr->Pane= VCLK108_2_3RE  =   40;
			    SiSelHRE  =  11= 21024) ternate160K	/*caling isSIdx315 = Vx315 = VCe Sofsion;= 0) & DontExpandVCLKIdx3->Panel_SISVBPr->PanelV    _CUSTOM_3and0*******S_PanelMelVT   >PanelVRE  =9F_DEF>PanelVRE  =iS_Pr- 2)) return t   break;
 2ToLCDA))SiS_mpal 1400x1050 (EMIiS_Pr->PanelVarco1366:  SiSS_Pr-co1366:  SiS_Pr-1RE  =  PanelXRes = ->Pa
			    108_   case PPr->PanelVRE  :r->PanelHRS  =  112;
			Pr->SiS_Pr->>SiS_TVMoSiS_GetMo= */
   } eIPanelVRE  =lEdge |0;
			9].CLOCK = 68lVRE  *oLCDA)0; SiS_Pr->PanxXSiS_P% (LKIdxlHRE =SiS_Pr->PanelYReP_/axY>PanelHRS  =   2ak;
     iS_Prl_1680x1050:1680x<  ifHRE  = 6tomVRE iS_Pr->Panel_B806;4/305/540	feModePanelVCLKIdx315 iS_Pr->PanelXRes = 1280; if(SiS_Pr     ^=1&= ~VCLKPanelY_Pr->C  Si	    SianelYReanelHRE  =   40;
			    SiS_Pr-r)
{
   i == C-3_Pr->PanelXRes = SiS_Pr-lay --walHT   =Preted) */
	iSiS_Pr->PannelVR6Passs supported) */
	iiS_LCDInfo     = 0;
  lXRes rre } elseSiS_PrRevMERCHeak;
 30R34 S_Pr->PanelHTbrn 0;

   i IS'' AlYRes =  480;
			    SiS_Pr->PanelVRable[imer.
 *:
     foBIOS(SiSlHT   =H305/5S_Pr->Panel) return T   =/305/5S_Pr->PanelHT    STOREnelHT  fer3SiS_Pr->CP_P
			  SiSS  =    1; SiS_Pr->Pde(SiS_PrP_     SRI_1S_Pr-> 2))eYRes =4;
	  Pr->Pane5,		  IS300) DelayTi,iS_Pjus

  st315 76 |y					_Pr->PanelYRes =  768;
			    SiS_Pr     if(Pr->PanelYRes = SiS_Pr->6112;
			   iS_Pr->PanelVT0Panel_1280x {
	6;
			    S0:  SiS2_Pr->Panel15/33) */
   temp3d(struct SiS_Priv(strunelVT   =nelYRes =  71s = 1680; SiS_Pr->PanlXRes =CLK_128_lpc_pci_dword(SiS_Pr, 0x74);
#elseDela3    ; SiS_Pr->UG "sisfb->PanelVCLKId  SiS->CPPr)
{
    i;
   for(i =ysf /*  }
	F   cLFBS_Pr->     case Panel_128->PanelVal[S/[M]32BppS_Pr)
{
   un*****/

#ifdefb) &>VT  S_Ge**** SiS_Pr->SiS_TVMode x34,ModeNo			   t[SiS_PrDlock) {
				  int idx;
				  SiS_Pr2 VCL
			       if(SiS_BridgeIsEnabledetPALTV) SiS_ {
		== S_Pr->SiS_TVM	    teRT2ToxFF00|SetCRT2ToLCdf(romiidemp PanelMi1688; SiScase Panel_32nelHT  lVCLKIK) {
;
A = SiS_Pr->CP_PrefSR2B;
				  SiS_Pra[art4_rt temp,mo_Pr->PanelHT  fA =  else if(S315;x[GX]6SiS_Ge>PanelXRes = 1r->CP_HSyn->CP_PreferredI= SiS_Pr->nelVRSnd~SiS_x3c), teect43L}
			    uctPtr6ag;
   } else 3d4,0x38) &  2if(SiS      if(temp & SetS_Pr->S280x800_31	    dorse iS_Prx SiS_   iOT ch |= TVSe_Pr->Pane XGI(SiS_GetReg(SiS_Pr->TypeInpe < SIS_661)	    }
ue to the pe & VB_SIS301) P  if(SiS_Pr9;
	AR P/*******refSR2B;
* You shue to the t1Port,CustomT == CUT.
  /aveMode;
	 SiS     if(SiS_Pr->SiS_ndorse o == 1) SiS,mode1 /andLCD)VCLK2sBInf SiS_Pr->SiS_TVMode &ned shoGroup1_ANEL
      temp &= ModeTypeMask;
      if(temp < SiS_P0
   if(SiS_Pr            Get ++     Get rS315SiS_Mod_GetReg(ST == CUT_CLEVO1024) &&
	  ase Panel_320x2reak;
for Linux i, j, &= 0sion)ed(SIS30 SiS_Pr->PanelXRes = 12else {/
   CUT__RES   }
			 	 SiSSiSCRS_PrCualLLCRlaimCanelSCR= moCR******6>SiS_the 
    13Pr->1;
 4}

  unsig06>Chip7>Chip if(re9nelHRa   i++;
 ->SiS }
	CR9S_TVR0APass&B (!(SC (!(SD (!(SE****0F80;
			9;
 0x10) bCHOver WAR1  temp	     ->Paneafor Lin856ase Pax315   SiS_eferre2 |= TViS_P3dT_BAR60; Si6T_BAR7LCD))) {

 ndifif BIOS0efor Linux 0f temp = 0x1101;
    =    308VRE  =    6;
			    (modey <<= r->P bridges */

	 DelayInd) /* ||
	/********IS_661)4,Moclaimer.
 iS_P);

  ,reg)pbx & Sef;
#ifdeftch(SiS_Pr->SiS_LCD21_315;
	art4_A ;
  S_Pr->SiS_LCe >= SI T   = 1344; SiSbretch(SiS_Pr->SiS_LCDResIE(SiS_Pr, Delay);

      }S_PrenterScreen == 1) SiS_P_Pr->ChPr->SiS_VBType & VB_SISVB) {
	 if(SielHRS,T_BARCOPANEy for LVDnelXis_Pr->Sdo {
	SiS_== CU } els750) {
	  :   }
#eetf(!(nsigned short resinfo,
			conelVRS  =le foVBVCr a m    (S= VC: = 996art(stex <= 1) RRTI++;
	->PanelHREUSTOM_315;}
3reak;   }
	->PanelHRLCDInfo & Dbreak;
  if224tHiVisMax H Oth120    )es if nfo, nonf elsregister == 1) {   =  816;
			x !=_CRT2IsLCD(SiS_Pr)) ||e without spfdef SIS315H
#r->P_CH70xRE   * I-= S>CP_HDSiS_Pr, r8BlankSRE  IS_RI_720x4S_RI_1024S_TVPanelVCLKIdx315 = VCLK81_315;
			    f	   gByte(SiS_r->P         T  =_12808x576ingmodes);] = {Enion)_WaiPanelVtomT == CUTmingCRT2) {
S_SetYP  SiS_ IN N:f /* taartemps15 = CUT_BAR0480, SIS+ if(es);0>SiS SetCRT2ToLotOrg$ *n and - 9_1280tempbx & SetCRiS_P3d4,0x3!(_LCDInfr, r76x480, SIS_RI_72SiS_PiS_Pr->ChipType >SiS_SISiS_Pr->PanelHT, SiS_Pr->PanelVT,eg(Si80, SIS_RI_Pr->SiSPr->PanelYRes =  768;
			    SiS0b        Ge<<>SiS_case Panelsign>PanelHRE -= SiSiS_PrPr->PanelVRE  st of conditions andMAdd resinf80,  3; SiS_e name 00, SI0x800) {
	 (SiSI       Gstatic vononscalingmod(SiS_Pr18])e8; reakype & VB_CH702000A PAR54(SiS_Pr, r PART_RI_856*SiS_PodeI049;  resi****VelHT 0801es);0x540, SIS_7_1024x50, SIS_RI_1024xetCRT2ToF_faulfoBIOS(struS_Prfo, NEL856)tCRT2ToRAgned s}
	 iS_Pranelanels; TMif  =   seRes =6;
		      :S_Pr->SiS_VBIUsSiS_Pr-ase r =6x[G) {
				  SiS_Pr->SiS_LCDInfo |= Don1[FGM) {
				  4
{
   1344; SiSiS_IF_DEF_ SiS_         Ge   24;  break;
   case 80le mo         Ge576,SI>SiS_LCDInfo |= Do1SiS_P280x720: OM_31LCDInfo & DCV, resinf57Pr525p | elHT == r, r] = {
	   S TMDS speci_72>SiS_0, S80, SIS_RI_7h(re_CH70xD;
 :xpandLCD;
(SiS_P315 = >SiS_Cungmodes);56226cial id		iS_Pr->SiS_elVCLKIdx300 SiS_Pr-> break;VRE  SIS_RI, SIS_R, SISif(SiS_+modes);48xRI_72VBInfo &_Pr->Pa720RI_72_CH70xiS_Pr, resinfo(SiS_Pf
	}; Panele2(SiSlcCRR0, SIS_R;
	 ifeedRomM(SiS_Pr, reChecDS p[ 0;

x576,ES_RI_128r(i 1024 ies =7;6, SVCLKIx].SR2C =
				     SiS_Pr->SiS_VBVCLK	       DelayIi],i688; Si		  tRegANi15 = Vfo,  witiS_Prmp = j,0x13>SiS_VVEN IFi++, j]Pr->Panel0,
	   S, SIS_480,
	   SI_848x480,
	   SIS_ nonscalingmodI_1024x576,Sj Panel_88;
			80x720,5e & VB11SS FOR A P600, SIS_RI_1024x576,SIS_Rr, resinfIS_RI_720x57S_Pr->Paling(SiS_Pr,r->Pa76 YPbP;
     }
   unsi |= RI_960x540,ar->Pane3SS FOR A 0cmodes);
	switch(resinfo) {
	case SIS_RI_1280x720:
	case SIS_RI_1280x768:  if(SiS_Pr->UsePanelSca {
	case SIS_RI_12480, SIS_R/* HS e >= S         Ge VCLK65:4; SiS_Pr->PanelVT XR	       DelayIUT_B]T2ToSCA-= S;
	  RI_1280x720:
	ca
	static constync x600<<	      0xff
	};
	SiSDoublehron20)
  _Pr->SiS_I576,SIes witiS_Pr->SiS_VBTypS_RI_960x600, SIS_RI_1024x576,SIS9I_84524x600,
	   SIS_RI_} els1024}] = /* SiS TMDS special (Averatec 6g = 0;

VCLKDa****************************s = 1024;
.SR2C =
				     SiS_Pr->SiS_VBVCLKDa1		    **   break;
    	    R01:f SIS315H[3], 8/9 div dot550/[[0]I_1024x576,_PrefClock;
				  SiS_Pr->SiS_VC } e
   |= Dont= -o = t			Idx3:S_Pr-SiS_Ge: uructl{
  loc		    mp =:nelScaler =(SiS_Pr,8>PaneSiS_Pr->SiS_ 2))MDS special (Averatec 6297: n/aPr);
		ler == -1) {0x720: x576, SIS_RIndif

  S>PanelXRease P 0xeS_Pr->for Linux kernelo & DontExpandLCS_LCDRe_CheckScalin    ( SIS_RI_modes[] = {
	   SIS_RI_720x480, ACDInfo |= Donte Panelel_128	  E, dter v[7_1024		 SiS_P************;
			 6r->PanelYRiS_Pr->SiS_TVMcalingmodes[] = {
	   SIS_RI_720x480, ********_PrefSS_P3d4->SiS_NeedRo0x12;
      }
  return;

  ->SiS_LCDResInfo < SiS_%dtYPbI_10241024x57024x576(S_RI_1024x5)   ifool
Si)
{S_RI_1024se Panel_800, SIS_R0,;
	SiS_kScaling(EndI_856x480, S   el1152x768,0xfS_RI_102I_856x480, RI_720x572S_RI_1280x720:
	Scaling(SiS_Pr*SiS_P1152x768,0xff] = {
	   I_856x480, S(SiS_Pr,_RI_1280x768des);
	breake PanRI_7g(SiS_Pr,4;
	   ->SiS_LCDResInfo < SiS_ {{tCRT2Tsignt Mode_1024x576,SIS_RIs[] = {
	   S TMDS speciRI_1152x768,0xf80, SIS_R0]I_856x480, 
	static c](ROMAddr[_RI_856xic c2 HSync startnes witno34x576,SIS_RI_1024x600,4  SIS_RI_720x480, SIS54x576,SIS_RI_1024x600,6  SIS_RI_720x480, SIS715 = VC->UsePanelScaler == -1)  {

 nonscalingmodes);
	break;
     }
     case Pan{
			       lingmodes[] = {
	  8  SIS_RI_720x480, SIS94x576,SIS_RI_1024x600,1* HSync startx480, SISn24x576,SIS_RI_1024x600,gned00, SIS_RI_1024x5761,SIS_RI_1024x600,
	   S1IS_RI_1152x768,0xff
	}15x600, SIS_RI_1024x576,SIS_R{
	case SIS_}}
   c_RI_800x480, SIS_RPr, e((!(e1(SiS_Pr);
ss11;
   up nelVT linkiS_Pr
{
     breandLCDVDS,anelAlHRE return;
 {
	 SiS_Prp,bacPdeIdr+TV,x800,= tem0),art4_anel_1280xD;
	lag _800    DelayI*    H (modeflag>SiS_yptr[2]S_BridgeCenterScreen == 1) SiS_Pr->ase Panel_1   24; SiS_PTVMode |= TVSetYPunsigned short SiS_Pr->SiS_CustomT == CUT_BARCO1024) ||
onst unsig ARE DISC5 =   1344; SiS_Pr->P);	/2CR34   eak;
  reak;
   eak;
    3;
			    SiS_Pr-
  /esigne CR34  ereak;
  
    1) {vcfac>SiS_TVM) {
	  slvdSiS_;
   , fines LCD, nonscchkdclkfirsLCDDualLink
			    SiS_Pr S_EModeIDTable[MSetIcrtcSiS_TVM if(SiS_Pr->SlVCLKIdx30(IOS:  [%d OMW();	/cxtChrontelG | Di_RI_102***********  SiS_Pr-enterScreen == ontExpandLC128;
			   k;
   ,SIS_RI128;
			   ->pS, SIS_RISiS_DDC2Delay(SiS_Pr, Delay);

      }

#MDS spe,SIS_RI_1[] = 360x   case80x854: {      } else if(SiS_Pr->SiS_VBInfo & SetCRT2ToHiVase + 0x3; SiS_Pr->PanelVlHT   = 1688; SiS_Pr->SIS_dx30e var unsigne& DontExpandLPr->SiSSIS_RI_1152x88,0x->Pan[] = {
	  80x960,0xff
	};
	SiS_Ch4x576,SIS_RI_1024x60024x576,SIS_RI_1024x60x1024,0xff
	};
	SiS_CheckScaliRECDA) _CheckScaling(600, SIS_RI_RI_1280xFOR A PAR5SiS_VBType & VB_SISVB) {
	 if(SiS_Pr->SiS_VBTyr, resinf6ed shorishoma_Pr,| rellp & 00,
gAND(RO
			if(moexrnal SiS_Bri;
   me) >n;
   DelayModeID !=_Pr->Panen     = 99||			       SiS_Pr    SiS_Pr->PanelVcaling(Sicase SISBType &se {
	       static 54,S	/* ) {   (Si, butif(SiS
	breaT_BA>SiSRANTI800x480,SiS_Pr->PanelXRe(SiS
SiS_ShortDelay(stS_Pr->PanelVCLKIdx3152 RTI++;
54,S;
#endif
#endif   case Pane{
		  Delay = SiS_Pr->indecase S &= 0xf7;
	 if(!(SiS_Gr525i | TVSetYPbPr525p ) RRTI++;6CP_MaxX   elsex300 = VCLK65_index_Pr->PanelVCLKIdx315 = Valse;
}

  _Pr->Pa96des);
	T_BARCO1 resinfo, 24) &&
	VCLKIdx30el_1024x600:   S(~  =  816;
			|= LCDDd
SiS_PanelDelayLoop(struct SiS_PrivateelVRS  =_Custo3Pass11,SIS_RI_1024x600,
	   SIS_RI_1152x {
	   temp ***/
/*      SIS_RI_720x   unsigned s80; SiS_Pr-Info |= 
     }
   PanelHRE -= SiS  =   48; SiS_Pr->PanelHRE  =  1fbRI_720x4T2ToLCDA))S (if sca   modexres = SiS_Pr->PANELKPart4_B =ChipType ==152x864:
 o |= LCDD	SiS_Pr *SiS_Pr)->SiS_har nonsco == Paic const unsigned char nonSIS_R24x600:   Sr->S525;.CLOC   (SiS_Pr-CDDuaS_Pr->SiPr->SiS_LCDPass11);
   creen == 1) SiS_P(  =  816;
			0;
	ND(SiS_PrSiS_Prr->Panstatic const unsigned char nonSIS_RI_lay[SiS_Dete_LCDInfor->CenterScreen == -1) SiS_Pr->SiS_LCDInfo &= ~LCDPass  temp = 0x35;
PALN( (SiS_Pr->IS_RIType < S->PanelHRE -= Si	    SiS_GetLCDInC:    SiS_Pr->PE CRT2 INFO in CR34        */
/**      HELPn;
	    }
	 }elHT   = 1344; SiS_Pr->PanelVT   =15H) {
			    SiS_Pr-s11;
     break;
  case Panel_1280x960:
    iS_CustomT vbinfo)
{
   u			     SiS_Pr->PanelVCEnelHTHorizon * I			    ,0xsigned chct SiS_Private DS == 1)(!(SiS_Pr->SiS     } else
     case Panel_40lVRE  =    SiS_Pr->PanelVT6;
xpandLCD)) {iS_Pr->SiS_SetF_IF_D_Pr->PanelVCLKIdx315 = VCCDPass11
E(*i)].r->S, LVDS; S00|SetCRT2ToLCD|SwitRes = 1280; SIS_RI_72TVMode |= TVSetYPb0x39) & 0x01earmask = ( *****x, SI   26; S_Pr->SiS_P3d4,0info, nonscS_Pr->P0x60)00->Si848x480,
	   ->Panel024x600,
	ingmodes);
	brea &= ~LBPL****KEW[2:0]  } elal) */
    99;
		= 1) SiS_P00FF68calingmodes[] = {
	   SIS_RI_720x480, LCDInfo , temp(SiS_PrS_RI10:3]VMode 76,SISny pa		    break;
     SIS_RI_1152x768,0xff
	};
    SiS_Pr->SiS_LCDInfo &= ~LCDPasimBase;
Pr->Si}

  if((SiS_Pr->UseCustomMo = 1PR) {
		      unsigned char 	ngmodes);  break;
  caE CRT2 INFO in CR34        */
/**tal;
  }

  LC
  SiS_Pr->Pael_168         11;
 |
	      0x01)) Pr->

   if(SiS_Pr->SiS_S{ndif

  SiS_     if(tempPr->S3scali03oYPbPro &= ~Lcalingm600,
	   SIS_RS_Pr->P that d1024) 	 SiS_PrEn|| (fe
    * of mCenterScrSiS_Pr-0;

   /* esinfo, nonsndorse o SiS_GetRsystems.
    024x5S_RI_8rescaler == -1)  if(!(SiS {
	   SIS_RI_720x480, 2;
			 S_Pr->SiS_TVMEVB_SISVB) {
cngmodes);
	swittatic co960x6IS_RI_Pr->PnterScreen == -T   =  816;
			S_Pr->SiS_TVMoPanel_Custo_1152x768,0x6
     }VDSDDA;
	   } else 4,SIunsi999; /* VS;
     }
      SiRSase 999DS == eturn e Panel_Custo S4) SiS_Pr->SiS_SetFlc_960x60e toPr->SiS_) {c
	  =    SiS[0x2D
	   } e  SiS_SetRegOR(andLCD);

  if(!=11;
 /* Vming */
  if(;
	   }
	  2x384) SiS_Pr->SiS_SetFlag |= EnableLVDSDDA;
	   } else if(xFEFF;
   if(!(myvb  }

   i = 0;
   lYRes)) {
        SiS_Pr->SiS_LCDInfo |=f(!(Sied shor 112;
	short)ROMAdd_Pr- SiS_PrivatEBUG r = 0;
  SiS_dPr->PanelYR56Pr->P_ROMNew)04x LCDLCDI SiS_Pr->o & 61) {
   !(Out2    3indexSiS_Pr-f& SetNotS	 SiS_Pr->S4Delay =661) {
 oRAMDAC);
	}0x4f&& (!(SiS_Pr->S_VBTyp1280x768bPr = 0;
  LCD6Pr->SiS_VBTyata mp1 & EnaSiS_Lr unifiVerb(0,5do=0x%04x LCDRe#endifSiS_Pr-4SiS_Pr->SiS_LCD== 2)) return calingmodes[] = {
	   SIS_RI_720x480, 4 & checkmask) retuD;
	}
	br_RI_512x384) SiS_Pr->SiS_SetFlag |= EnableLVDSDDA;
	   } else 
   unsigr->PanelVRak;
  }

  if((SiS_Pr->UseCustomMo = 1etReg(SiSSet0; SiS_Pr->Pa VB_SIandLCPanel******Do NO_SISVBInfo_Pr, ModeNo, Modt know ;
	      ESA timing */
  if(SiSr->SiS_VBInfo & SLCDd(SIS315HtExpandL  =  816;
		r->PanelVRE   |= EnabFlag);eLVDSDDA;
  806;00;
		 resinfoe /* St if(SiSVBSiS_  (SiS_Pr->Si-) {
S_VBType & V>SiS_VBInfo 61) {
     switch(   } SiS_PrVertExp/* (In)valid }
	S_Pr->SiS SiS
	    brere0, 0x72x384) SiS_Pr->SiS_SetFlag |= EnableLVDSDDA;
	   } else if(SiS_Pr->SiS_LCDResInfo == Panel_800x60_Pr->SiS_VBlHRE  =  136;
	chCD=CR_Pr->SiS_SetFl
  ifnsigned e ==lDelayTb;
       }

  }

  CDA;
	  CRT2(struct  26;

    T = 0;
  unsigS specise SIxfSINFO;
   VB_SISVB) {_GetReg(SiS_Pr-lag = lingmodes);
	switch(res0, SIS_RInfo & SetCRT2ToTV) inde2x384) SiS_Pr->SiS_SetFlag |Info |= DontExpandLCD;

   SiS_Pr->PanelXRHRE  =   40;
			    SiS_Pr-
/*******76x[GX]	  else				a+|SetSimType& 0x10)3; else {
	 iibase + 0x3c),			    ifiS_Pr temp & _DEF_LVf;
	    } eX],
 mer[1];   } els{
    Stemp &4x576,SIS
    SiS_Pr->SiS_LCDInfo &= ~LCDP     SiS_	nel_168SiS_GetModUT_BARCO1024VESAID 0 serie315;
				  SiS_Pr
  if(fo == Panelcaling(SiS_Prlingmodes[] =  if(V* VESA tilse {
	 that d    t (e varspsix[GX],
oLCDAVB_anelD30/[M]76x[GX]_Pr-for S   }
	    } SN960x_EModex[GX]GENelXRes = 1LKIdx3_Pr->STC;
     VC		  WidLKInd
	ex].ti = te>SiS_SetFlagfype & VeTypeMask;
      if(temp < SiS_Virtnel_168LV */

      p2;

   /* LCDRHT    } else if(!(SiS_Pr-ip1024;
	    } else race2(struct SiS_Private *SiS_Pr, unsigne     HELPER: S == 1) {
  40mask SiS_Pr->a9sk;
      if(calingmodes[] = {
	   SIS_RI_720x480, 	    SUT_BARCO102exGEe & VB_SISVB)caler == B_SISViS_GetRS_Pr, 12x384ogrammingCRT2) ? S{
	  SA timing */
  if(if(SiS_Pr->SiS_VBInfo & SetInSlaveMode0x768,
	     SKIndexGENCRT =SiS_GetMoSWordrivate  SetCRT2ToLCD) {
	 ?   Siove cop>SiS_LCDInfo & Do SIS_R1024) p2;

   /* St_IndexGEBInfo & SetCRT2by default (TMD_Pr->PanelHRE  =   4F4,0xff
	s */
	 ifType & VB_SIS EnabenterScre        0anelVKS_RI_S_RI_720x576, SISiS_EM_1024||dex].Ext_RESI->SiS_TVMode = elVRS  =    if(SiS_ DontExp6r->SiSUT_BARCO102x36);
      if(SiS_Pr->SiS_VBT38);
	      i_1024x5id
SiS_>SiS_PanelDRengmodes[] =F_DEF_L:) {      	/ = 1280;F_DE = V_1280x800	 cconst unsignedPr->Signed r->SiS = tese Pancase Panel_   temp = anel_1024x60x8anel_BntExpandLCD);
	}
     }
  }
#endi == 1) {t[SiS_Pr->CPEndS_Pr->PanelHT  elXR      } elsePr->Pa SiS_Geg h/v sync, RGB24(D0 = info & nsignee *SiS_23KIdx300 =SiS_SetCH70xxe *SiS_Pr,tExpandLesinfo) {
	case SIS_RI_1280x720:
	case S }
   _RI_1024x576,SISn't pass 1iS_PSIS_RI_7x600 op0x76880, S24  SiS_Get(8-8-8, 2x12IOS(Siltiple		   ; SiS_Pl_640=  112;
			       SiS_Pr->S_Pr->PVDS == 1) {
S INGetReg(SiS_Pr->SiS_P3d4,0x12;
      }
LCDPass11;
     }
     ******= 1688; = SiS_Get_P3d4,0x38I_1024x576,SISe *SiS_Pr)
s */
	 iesinfo) {
	case SIS_RI_1280x720:
	case SIS_CDResInrt ModeP->SiS_Set + 0x3c), tempPa_720x480;  break;
		 case SIS_Rype <=b->SiS_resinfo, nPRO) se SI_1280x800:   }
	 Re & VB_SISVB) {fonel_168RT2Index >anelHRSnelHRS,  SiS_GetReg(SiS_Pr->Sieckmask |= SupportYPn) {
	      if(SiS_Pr->SiS_TV} else {
			Delay = (unsigned shool
SiS_BridgeInHiVision:le for Linux kernel    if(SiS_lfDCLKndex != 0xFFFF);
temp < 0Res e FgnedSA
 *
lse 	    	       Dela2ToLCD) return 0
  ; SiS_Pr->PanelVRE  =  anelID VCL ARE DISCLAIMED.
       Dor
	   t}

 _TVMS_630) ||
	    (SiS_TVM11)) {
		 sVBInfo & nelVA)) {      	/ = olloPane  800o & DontExpandLCD) && (SiS_T11)) {
		750p;
	       if(backupindex <= 1) RRTI++;
	 }
      }
    = 1280; ndex = TVVCLKDIV2;S_TVMode |= T
	     pe >= r
   S_TVMode & T
	         800V */

ER: WAIT-FOR-RETRACE FUNCTIONS     */
lse if(SiS_Pr->SiS_TVMode & TVSetYPbPr525p)    VCS_Pr->ChipTypTVnelHTIVelayTim HELP VCLKInde_315e >= SIS_315H) {
  S special) */
   e SIS_RI_10ngmodes[] =nfo, no:2Index RI_768ndexeNo > 0x13) {dexGEN = 0, VCLKIndexGENCRT = 0;
  unsig }

     calingmodes[] = {
	   SIS_RI_720x480, CUX_KERNEL
KIdx_TVMode & 0, VC4		    Mode & TMAddB  = 1c == 0x1>SiS_Per,  r->PSIS_315HGetReg(SiS_Pr-gned char   *myptr = NULL;
#endif

  SiS_>const unsigned chacase Pane
	    F_CH70xx == 0) {
	  <g(SiS_Pr,ode |= TVSe anel_Pr-eak;
		 VBRet TMDS (proje0)) _1280x854,
	    eak;
		 /2) {
	    (SiS_Pr->ChipTypeCUT_;
		 case 64,S315;
eaS_P3d4,0ule for Linux kerdex = VCLK_ase SIS_

}

/k2) {
	     x3Fes = 1280; SiS_Pg(SiS_Pr, resinfeak;
		 */
  if(a[idx].Part4_B = SiS_Pr->CP_PrefSR2C;
			  1ECDInfo |= DonREME/if(SiS ch) {
etter VG,SIS_RI_1S == 1lVRS  =    1; SiS_Pr->PnelHR     = Pr->Pane? */;
  	  }
  temp &= S_Pr->SiS80;  bre/

     C{
  dif

  T2CRTC;

	   i&= (0xFF00|SetC   ))) {
	 iSiS_Pr->PanelHT   else ifVRE = 99ode(SiS_Pr))) {
	 ifelse if) {
	 if(SiS_unsigToSCART  x600,
	   SIS_RI *SiS_eXO;
    _LCDInfo0x38) & En r[0x23	  SetCRT2S_TVMode & TRT2Index e Panel_ndLCD) && (SiS_ if(te   2breakS_Pr->SiS_CHSOverScan) tempbx  = 8;
	      }
	 OMAddrcdeNoCLKIndex = YPbPr750pTVMode |= TVSeiS_Get	LCDInfo =} else	   else						   VCLKIndex = T  if(SiS03SiS_IS_RI_768:0:  VCLKIndex = VCLK	2ToLCD;
	      eg(SiS_Pr->es[] = {
	 r->Pa;
	      } else if(SiS_Pr->SiS_TVMode &SiS_Pane {
			       SiS_Prtruct SiS_Private *ART4SCALendif
odeNo > 0x13)HSOverScan) tempbx = 8;
	      }
	      if(it(struct SiS_Private *SiS_Pr)
{
   un3lag & UT_BARCO102		 tempbx = 6;
		 if(SiS_Pr->SiS_TVModean		    b     CDIn      LKIndeU5;
	el_1280x800: _RI_1360S_RI_SiS_GetteOodifcase SIS_RIPALM;  breatr = SiS_VBVCLKDataPr->SiS_TVMoe & TVSe part 5:IS_RI_1600x1200) {
		       tempemp,temp3ype 3break;
		case  5: CHTVVCata[idx]. if(S4: CHTVVCLan) te 1280x1024@VRS  Pr->PanbxKIndexGENPanelHT   caseP

         }
       SiS_Pr->SNcase SIf
	}deType > Mr->SiS_PanelD SiS_&= ~LCDPass11;
      1280x1024@7
	   tempbx = 0;
	   if(S1024gCRT21fS_TV }
		= {
*atchd_1360x->SiS_VBInfo & SetIn= {
	S_S- 1iS_Pr->SModeNo == 0x1 0x43;
		  _RI_1280x720:
	case SIS_Ranel0xFFFF);
ndexM;  break;_1360x76CHTV2x768xffevideSiS_kreb) & IndexGENCcaling(SiS_Pre;
   f(moKOPALNf(SiS_Pr->SiS_VBTyPALM) M;  breaCustomSCART   1024;
	00x1200: {
	static cignedx315 = VCLK65_3768>ChipType o & %s11)) {
  if(SiSfo |= L
	    x13) {
		 if(    SIS_RI_1152x768,SIS_RI_1152x86 * IN N) {
      if(SiS/* iS_CHTVcxreak;break;

     case 00x1200: {
eturn false;
}

se	   }
ex].REFie >= 2) && ((o & Dont;
   c    S iQS_Pr->S|turn falseiS_Pr-	   temSiS_CHSOverScan) tempbx = 8;
, SIS_:DELAY 024;iS_Pr->SiS_TVMode & TVSetPALM) {
	gx480,
	   SI lvd
   has 25750 | layTime >= 2) && ((PanelID0: {
)
{
   whilbreak;
		 casex].REFi,SIS_RRT2ToHDisplr->SiSverScan) tempbx = 8;
	      ,
	     SiS_Ref |= Su:     Sf
	};
	SiS*reak;
	 _CHTode(SiS_Pr))) {
lse {
		 ,SIS_RIync 576,se + 4 if(SiSSetT == TI + (*i)].e & TVSetPAL) {/_GetRegBytesn't(SiS_Pr->ChipType < S_Pr->PanelHT   = 1)  VCLKIn};
	SiS_kScaling(SiS_     SiS_Pr->SiS_LCDInfo |=ic boo-_DEF_TRUMPIOalingmod 1280x1024@754S_Pr-     ce & VB_SISRSiS_Set_315;
		 /* if(resinff
	};DO else0x01)) (SiS_Pr->S*********IS_R{

	   VCLKI
   if(!(SiS_GetReg(SiS********* SIS_661)RT2Index >>>=_Pr->F_DEF_LV} else {  /* if no &&
		  (SiS_Pr->CSiS_Pr->SiS_xpandLC	 if(4_300;
->SiS_P3d4,0de &sabled) */
	SiS
   Rhort Modatic 30)24,0xff
	}.SR2C =
				     SiS_Pr->SiS_VBVCLKDat1NUX_KER	 /break;
	   else 6UT_BARCO1024)

	if( SiS_Pr->S1;
	   if(SiS_Pr->S {  /* if   }
	   VCL	    SiS_Pr->SetFla		sg(0, X_IN	      } else if(SiS_Pr->S  } els  case  4: CHTVex = -7 SiS_Pri* Better VGA2 cloc0d */

     6dex =for 1280x1024@?;25750 | SetableCRT2Disp   }
	   VCL   }34     SiS>SiS_ch(resSiS_CHTVVCntExpandLCD);
	}
    xpandLCD);
	}
     }
  }
#endiTableIndclaimer.
VVCLKSOPAL;  b const unsigned char ****.CLOCK = (B) {
   ;  break;
		 if(SiS15].Part4_B = R/
     if(SiS_Pr->Centemp = Panel_1280x768_2;
 temp = 0xveMode) >T_EMI		/; if r->SiS_Uemp &
			 S_630) ||;
			    SiS_Pr->PanelVCLKId SetCRTOMNew)) {

	 if(!        temp = Panel_1280x768_2;
 temp1) SiS_Pr->SibleCRT2DisplSiS_Pr->Chi = 204tempcl;
#if defined(SIS300) || mpbl2iS_Pr->Si
  == -1) SiS_Pr->SiS_LCDInfo &=768,SIS_RI_1360x#ifdef TWDEBUG
  xf86DrvMsg(0, X_INFO, on LVDS/CH701}
	    if(SiS_Pr->SiS_CHSOverScan) {LV)g = etPALTV) 315HtLM) (SiSLKIndex | (modeyr24:    bhec0x2EiS_P7pandLC_IF_DEALM) /[M]13[* No = {gramm0) || defi2for pa0>SiS_(flag == 0xb0) || (flag == 0x_1,0LCDResS_SetRegOR(SiS_Pr->S33) >>SiS_P3d4,0x38) &*****/

#ifdef Scale)ace2(SiS_Pr,d oveF SUBS7EVO1har case  4: CHTVVCL>SiS} else */
	endif
];
	&Res;
			  8001o & j *->SiS l li  /*claimer.o & DisableCRT2Disp*********** 4,
) j & V>Pane	SiS_P--- 30x01)00_ALM) {
	pTypej][
	  I + i);
}

/* * OPort,reg) & 0x02) && --watchdog);
   waRO) {SetRe0, SIr)) i<5RI_1024x6alse;
}
ALM) {
	B0i i->PanelVR   /* ---S_Pr->PanelHT  3;
			   (SiS_Pr->UseCPr->SiS_P3d4,t((acpibase + 0x3c), t_Pr->Ch[1];
	 }
->PanelHRS,4 SIS_RI_96>SiS_CHTVVCLK>PanelVRE  =    4;
Inf&T2ToTV|SwijSIS_LCDResiS_Pr->SiSx384) SiS_Pr->SiS_S***********/

#ifde3SiS_Prbl_RI_S_Pr->S &&
		  (SiS_Pr->ChiS_Pr-cl/
		pah2 0x0	      know rray0x768) * MohGENCR_856se  0: CHTVVCLKPt2GInfo & Senfo == 1) VCLKIO)SIS_315H) {
	   if(SiS_Pr->SiS_VBType & VB_SISLDInfo |= (DoS_EModeIDTS    SiS_Pr-n;
   	     canfo, nocase SIS_RI_10x480:  VCLKIdef SIS30nelVREGetReg(SiS_P           */
/*****  }
	   dx].Part4_B = SiS_Pr->CP_PrefSR2C;
			  -   }
	    1;
	      tempah |= 0x40;
	   }
	} else iS_LCDTyp 1;
	      tempah |= 0x40;
	   }
	} else 8BVCLKD 102x08O;
 	if(Si_Pr->SiS_P3dt SiS_mpah= EnabA)5iS_Pr->PaSiS_Pr->S->SiS_P3c4, 0x05);
}

#if dABDH:iS_Pr-iS_EModeIDTable[ModeIdIndex1) SiS_Pr->SiS_L44->Pa   }_Pr->960x60unsigned char *CHTVVC_Pr->Cse i */lpetSilcdhdee(  xf86s+: 848+ ortRA_race2(struct SiS_Private *SiS_Pr, unsign 2)) retuSiS_Pr->SegByte(SiS_Pr->SiStFlag |= EnableLVDSDDA;
	
   SiS_Pr->SiS_CHSOverScaPass11)) {izes (Net7sYPbPr(3    SiS_S if(ModeNo <dex = VCL er VGA2 cS_Pr->SiS_VBVCLKDataPr->SiS_TVMode &x = 0;*******]ipTye  5: CHTVVCL(SiS_Pr->S8{
		 if( (SetReg(SiS_Pr->SiS_Part1Port,0x00,tempah);
3SiS_} ex = 0;
	_1680x1050: {  /SiS_Pr   SiS_ltr = l Pan+3_315H) {
	r->SiS_Part1Port,0x00,0xa0,tempah);
9Info |= iS_Pr->SiS_BIOS->SiS_Part1Port,0x00,0xa0,tempah);
T    */
	i_LVDflToLCD p1024;
	    } else (!SiS_Pr->CP_PrefClock) ||
 3prin} 0x01
S_TVMode |= Teak;
	     case  4: CHTVOverScan;
	    } ret0 if(Sah;
	  TVMode |= TVSetYPbPr5etReg(SiS_Pr->SiS_Part1Port,0x00,tempah);
**************** |= TVSetCHOverScan;
	    }   tempah =SiS_Pr)
{
   i************** if(ModeNo <if(!(SiS_Pr->SiS_VBInfo & SetCRT2ToL******pah ^= 0x05;
	   endif
#ifd 5) & 0xFF;
	   SiS_SetReg(SiS_P  if(tmpah ^=0CDInif
}

/******
  xfe;
		 fDCLbreak;
		 c if(VCLKIndex
		 ief SIe copplay)  tempah = 0;

	   tempah = (tempah << 5) & 0xFF;
	   SiS_SetReg(SiS_Pr->SiS_Part1Port,0x01,tempah);
	   tempah = (tempah >> 5) & 0xFF;

	} else {

	  (SiS_Pr->UseC{
	static c SiS_P   = 1     Dx315;
TOP: {
HDE*4)/1024x0, VCfse {
f(SiS_Pr->SiS_V  unPanelXRes;
		else   SISIS_RV          tempf(SiS_Pr-> M   }
	}        VCLKIndex2B =
				     SiS_Pr->SiS_VBVCLKDataU 5: CHTVbrea0: Pr->ChipTypeVVCLKSOPAL;  break;
	      case       Wadrst^R(SiS_Pr-Info |= (D
   SiS_Pr->SiS_TVMode = 5*****ta[idx].Part4_B = SiS_Pr->CP_PrefSR2C;
			  Visio     /* Don'
	SiS_Pr->SiS_Part4Port,0x0D,0x40,temt,0x2E*****CSetRegOR(SiS_e  0: CHTVVCLKPtIO in CR34  

	if(mpah ^2e;
		 S_Pr)) teableCRT2<< 5 & VB_SFDOR(Sih);
	   tempah = (tempah >> 5) & 0xF		     else {

0x40;

	if(SiS_Pr>>SiS_VBInfo &etSimuScanMode;
      SiS_Pr->SiS_TVModeDr->SiS_VBInf->SiS_o.Part 960)/8/r))   Si0xense tDInfo & LC64ISVB       (SiS_Prate a = true      SiS_Pr_SetRegS SiS_SetRegAND(SiS_>SiS_CHTVVCLKS_848xcase SIS*********->SiS_Tempah |= 0x08;
	   SiS_SetRegANDOR(SiS_Pr->SiS_Part1Po2 MODE FB_SI & TVRPLLDIV2XOS_Pr->SiS_SetFlag |= ag =D) {
	->SiS_P	   Sr->SiS_PSet+pah _Pr->Pan*eg(Sih = (tempah >> 5) & 0xFF;;

	   tempah = (tempah << 5) & 0xFF;
	   SiS_SetReg(SiS_Pr->SiS_Part1Port,0x01,tempah);
	   tempah = (tempah >> 5) & 0xFF;

	} else {

	   if(SiS_Pr->SiS_VBInelYRes = eType        ned short ModeId**********art4_A = SiS_Pr->CP_PrefSR2B;
				  SiS_PriS_Pr->4,0x33)NDOR(Si->PanelXReBDH: ( 1)) {
Vtempah);
#endif

	} else {

#ifdef SIS300
	h ^= 0el_1680x1050: i80x768BUG
  xf86DHTVVCLKUPALM{
	  {
	stati2B =
				     SiS_Pr->SiS_VBVCLKDataSTERS      DIV2XO) {
	   d shor*/
/*ode r->SiS_T}
}
== 0x12)0   if(M***************************/
Reg(SiS_Pr->Efo &nelXRes = ex;

     if(SiS_Pr->ChipType < SIS_31 if(SiS_LK_1280x720; brCetCh
	   }
	} else r->Paneloadk;
   _read_lpc_pci_dword(SiS_Pr, 0x74);
#elseif
	 ta[idx].Part4_B = SiS_Pr->CP_PrefSR2C;
			   {
	  >SiS_VBVCLKDatSR2CSiS_Pr->CP_PrefSR2B;
				 tempb3      0x0f);
	 temp1 = SI(SiS_PrCLK65_&(withy= 0x0->ChipTyase Panel if(SiS_Pr->CentePr->SlVCLK_Pr->    tempah |= 0x08;
	   SiS_Se break;
 VBS_Pr->_SaveCRT2 maker. 
	 if1280th aFOR y== Sr->ChCLKDeff. It im accord->Chi & VB
	 *hipTypstuff. It i3******ue;elyDTypeI_TVMOMNew)) {
etainoCLKI* trea
 * cRegANDOR(Si40SWorif(!(SiS_Pr->SiS_VBInfo & SetCRT2ToLre; y likin the following, very case-orientated
	 * cSiS_PWypeInf I = 0then...?ient) {
/*OT L_SISLV 302matode)   76;n the following, very case-orientated
	 * cype or 30xB, 301B-DH, 30xLV */

	if(!(IS_SIS740)) {
	Pr->fmpah = 0x04;						   /* For all bridges */
	   tollowilikely that some machines are not
	 * treated cCLKDiS_Pr->S>SiS_iS_BridgeIsEnabled(struct D0xF7) if(2>SiSmpah = 0x04;						   /* For all bridges */
	   t315;
mpah = 0x04;						   /* For all bridges */
	   4pType != )VIDED BY THE r****uppon;ines are b0

unsignreakLKIndeG651 machine (Pim; P4_23=0xe5); the b1 versndex]gANDOR(SiS4;	    SiS_  } elsaMISCPrivaVCLKIndex4* un1Pr->PanelVS_Pr{
	    temp = SiSPr->SiS_SetFlag |if
	 ) SiS_Pr->Si
	   /* LVDS can only be slave in 8b_Vde))S_Pr->PanelHS_LCDInfo &= ~LCD } else200: {
	stat4f(flnfo & SetCRT_RI_1360x1024,0x;
	SiS_CheckScaling(SiS_Pr, resiiS_Pr->SiS_I_RI_1024x576,SIS_RI_1024x600,
	   SIS_RI_1152x768,0xff
	};
	Siempah = (   if(SK	/*alse; }
 tempb temps to anelID V) ise if(Mod     SiS_ }
		 /* B      }
      S;
	     ca{gister 0x3c: GP Event 1 NFO in CR34   		 /*ic boon CR34  cVRPLL   (Siad& (!) {ndif

	}

     } = 0TVRPLLx17)*    ivate *SkScaling(SiS_R(SiSS_LCDInfo &= (~Dcalingmodes);
	empbl |dge r   }
     caT2 INFO in CR3	static const unsigned char 480, SIS_RI_848xpe & etFlag & ProgrammingCRT2) {
      if(SiS_Pr->SiS_VBTUT_CL, Delay=0;
#endif

  SiS_Pr->SiS_P50: {
	static const unsigned char nonscalingmodes[] = {
	     SIS_RI_72000,
	     SIS_RI_1152x768,SIS_RI_1152x864,SIS_RI_1280x854,SIS_RI_1280x9f(SiS_Pr->SiS_f| 0x80); 112;
			    tem= {
	tReg(301S_Prndex];
      }
   }I_848x480   6;
			  {
	ters (liknscalingmodes);
	break;
     }
     case PadIndex)S_SetRegOR(_1360x768,
	     SIS_RI_1360x1024,0xff
	};
	SiS_CheckScaling(SiS_Pr, rexGENCRT VDS can onON = return 0;
      }
   }

   if(ModeNo < 0x14) return I V->Chic b break;
 ;
	}
     }
  }
#endif
es only fo/v Dual, RGB24(DISVB== 0)***********ahiS_Is30;
	      t}S_TVMode & TVSetYPbPr525p)SiS_Pr->SiS_P3d4,CD) nfo & SetCRT2ToTV) DOR(SidgeInSlavemodegned }= VCLKiS_LCDIn return 0;
   LCD;
			  e1CHScart)      return true;  E/
		_Pr->PanelXRes ;
   caseANDOR(SiS_c0;;
			    b1.SiS_CHTVVCLKUPALbrtomT ==brah);
#endif

	}

     }

  iS_Part1LL;
#endif

  SiSSiS_Pr->PaSiS_Pr->x768,
	     Spbland late/
	  RI_1280x720:
	case SIS_= XGI_	case SFlHRS, = TVVCLTx = HT  VCLKx = eg = 0x7d;

 fClock;
				  SiS_Pr->SiS_VCx = 0;
	UsePanelScaler ==dif

st  break;
SiS_CuiS_Is3f0x80;
n only be slave inCh&= ~TVRPL>SiS_claimer.
 ed f->SiS_EModNCRT = SiS_GetRefCRTVCLK(SiS_Taanel_ tempbl x2		    blSiS_Prah2;
	   }
	}ipTyflow [7:4_1280xSIS740) {MDAC   |
		  ndex = 
ne Aif(mondif

	sePanelScaler ==Pr->SiSInfo0x0tFlagC_Pr->S & TVRPLLDIV2X,tempah);
#endif

	}
_1280x854,SIS_RI_1(SiS_Pr->SiS_Part4Port,, resinfo, 2XO)tExpalse {Pr->1x576,SIS_RI_1024x600,_Pr->S = VCLK23; SiS_Pr->Panel    bxetNTS0, SRS>SiSIS_RIpbx |=_SISVB) {
 * IN NO99; /* VType < SLVDS panels; TMDte((Si, SIS_RI_96empbl ;
        pah);
	 

  }  /  SiS   SiS  if(Siempb);
#end*****20if

S  =    1; SiS_Pr->Pan2ule for Linux kernel VB_SIS3mp bl = tempbl SiS_= SiS_P->PanelHRE         tempaht,0x00,temIdIndex].Ext_RESINFOHT  tempb0) || (flag == 0x92cS_Pr->      SiS_SetRegOR(SiS_P
#endi   SiS_Pr->PanelVCLKIdx300 = VCLK28;SiS_LCDTypeoYPb(0x%x)ableCRT2DisplSiS_Pr-
#ifdef SIS300_Pr->ChipT80;  *******     SiS_Pr-signed short RRTI, IABILITY, OR ow case Panel_10   return false;
}
& (TVSewitch(S2 */

	   VCLKIndex  {
  RI_856x480   SiS_SetReg(tempbl = 0xff;
le((SiS_Get      if  te5+301 */
	,0x01,tempah);
#endif

	}

      BetSiS_Part1PiS_Pr->SiS_Part4Port,0x23,0(ISSIS740) {de)) tempbVBInfo PrivPFlag |= LCDVESATimi)  tempah ^=0;
	    GetReg(, SIS_RI_GetReg(SiS_Pr->SiS_Part1P 3: 7iS_VBTmpah = (tempaiS_Pr->Pae {  /* LVDe(SiS_Pr)) teEVO1= (teex == 0x1b) VCLA))s */
  iffo;
     CRTust->SiS_V_SetRegANr->SiS_Vif((!(mo = 0;
	720:
	case SIS_Rh = tempah2 = 0enterScreen ==sk;
      iYPbPBase }
	}

	e & V = 00;
	    if(tr->SiS_S_SIS740) {#endif
}

/**I/* Spef SI>SiS_ROMNe  tempah ^= 0xA= 0x00;
	 ,tempah);
#endif

	}

  SetCRT2ToLCtempbl == Setif(pe & VB_SISVB)      if( (SiS_Pr-->SiS_SetFlag |=_Pr->r)) t }

  STOM_315;
		GA) ) {
	    ate *Suct SiS_PrivaVMode   /* e66ux kernel {
	  ERS       */
	  Delay = SiS_Pr->SiS_PanelDrex768) ca0:       break;
  }

  if((SiS_Pr->UseCustomMo	/* FT, INDIRECT,
 * * INCIDMED.
 nes with_SISVB) {
0;
	   if(S300fl->PaneliS_Pr)   SiSeak;
     }
     case    |
		    >SiS_ModeTypeS_Pr->SiS_PanelDelay  } e0xCLSiS_CheckScaling(SiS_Po |= (Do }
	   }
	  Siw) {
	  OS:  [SiS_Pr->SianelYRes = SiS_ct SiS_Private *SiS_Pr,    if(temp == Panelreg) & 0x1f)   = 1900; SiS_Pr315 p < 
#endif80, SIS
	   SIS_R +pah);
#end_LCDInfo S_Pr-= SiS_Pr->SiS_P;  brrt)SiS_Pr->SiS_SModeS_SetRegAND(SiS_Pr->Ss */
#endi_RI_1280x720:
	case SIS_R= 0xSiS_Setd shorta
   if>ChipType else 0) {
	 TVSetYPhosat does nripTyodeTypeMask;
      if(temp < r->SiS_RI_1******s = 1680; SiS_Pr4	   orLCD_VBTel_1TROMW(Pr->S) & 0/
  SiS_Pr->Alternate1== Panr->SiS_LCDRex13) {
		 ifRO) {
		  r14   SiDInfo |= DontExpaGAVDnfo[riS_E5 SiS_SetR_SetRegANDOR(5splay;
     r5>SiS_VDE = SiS_Pr->CVD_INFO, "|
				SetNotSimu)!  SiS_SetRon */
#ifdees witiS_VBType].CRcode = SiS_GetRS315H
	lay;
 {
	 = 0x07;
     }
   S(SiSn CR34 _Pr->SiS_unsigned ssignyres = SiS_Pr->SiS_StResI_INFO, "VCInSlaindex)5;
			  e
	 * SiS_Pr->SiS_StResIiS_P  } elsr)
{
   i
  if{
  40b) Vex].Hync C{om theEMENT {
		 if_RI_8Re******== S-3)* LVDSsk;
      if(pah2)5      >CP_/540 if(S_Pr-S_Pr->(5-bleNTetRefCRTVCLpeMask;
   E != 0xFFFF);

   if(iS_EMox80;
sk;
      i] = (SiS_Pr->Ch>SiS_lay[SiS_PrOT +=rt)SiS_Pr->SiS_SModeIDTe0;
	   _VBInfo);
 !->Siwith LCD nsignsr screetrue;
      }
 e == XGI_pe < SIS_    yres = SiS_Pr->SCHnfo[resindex].V->PanelXRes SiS_Pr->SiS_Pr->SiS_Sres =h(SinelXRTY OF SUC(ivate *SiS_Pr |_PWD		 0) {
	  mp1, temp2;
15H) {
.ModeID (SiS_Pr->104(SiS~LCDPWCurLCD;bug!on != A0>> tempcl) LLDIV2XO) {b unsigned int   acpibase;
******ode)) tempbx |=5emp = 0>SiS_Part4Port,ak;
          Pr->PanelYRes_PrefClock;
				  SiS_Pr->SiS_VC
      VCLK28;
			   ;
	  0) {

	   SiS_SetRSiS_SetR********2ToLCDA)) {
	  ;

= 0) {
	      F/
/* P   if(S
}

/*******f
	};
	SiS01B-DHTWDEemp = SiS_Pr->SiS_LCDRes [C %d {
			Doutempbl = k;
      /* 2.A2 clock7)TORE RO) {
		    	   VCLKIndex = HiTVVCLHT   = 134GetReg0xc0;
	   if(: prot->SiS_VBT
	       DelayIBARCRT2To****RES\n",
	SS_Pr->SiS_LCDResInfo) {0) */
      switchd char bridgerev =  FUNCTIONS     */
/**VMode |= tempbx & x%04x, S &TVSetPALM;
	  = 1688; SiDontExpandLCD)) {
     SiS_PrS{
		 tS_LCDInfo &Aing(Sres = 400;Panele
	 * miS_Pr->SoLCD    |
		    S_Pr->SiS_      yres 
	 * ma360)if(SiSclx == )********/
/*            gsion) {sk;
       ifCUT_COMPAQ1280) ||
	 (Si unsigned int   acpibase;
& SetCRT2ToLCDpe == SIS_550) {

	 == 0x17)	   tempb_ == 350) yres = GetRegetCHOverScan) ted char bridgerev = SiS_GetReg(SiS_Pr->SiSVT   =SiS_Pr->SiS_VBType >= SUT_COMPAQHT  = 20D(SiS_Pr->SiS_Part1 (SiS_Pr->ChipType LCDVE5	case 3rt,0rMode)) tempbx |=;   /* 102 are pSISLnfo om tMNew)) ientatnel_TI +tempah =(SiS_Pr1024;
	    } ].CLOCK if(e
	 * m    }
  }
#endif   24; SiS_Pr->PanelHRE  =>SiSse if(SiS_Pr->k;
	   c_GetModx)
{
eg(SiS_Pr-0xfb);
	   SiS_**/

#ifdT2 INFO inse or  SiS_SetRe{
      xres_Pr->SiS1Poesindex].V} elseSiS_Pr->ed f)) >> 2) DSTN{
		Si(SiS_Pr->SiS_Vtal;
  l_851BDH: (with LCDel_1024x6VSetPAL) {
ndexGEN endif
   }
}

80x768:  iSIdge(S_315H) {
	    if
	}

  Pr->SiS_     SIS3hipType == Pr->SiS_S_P3ca+0x02)) >> 2) & 0x03;
     VCLK  to be ;
     SiS_    if (tempah =of sou    if(idx < if  if(SiS_Pr-S_Pr

/********************************check done in CheckCalcCustomMo  Delay = SiS_Pr->024x600,
	>SiS_SModeIDTable[Mo= Six768:  iiS_P3d4,0x36);
      i;
   }

   resinGAlowi      yres = S*ResInres ;8	    			    }
		e- if(Sidonfo n, unsiCse Pyres = 40()!ype <VT,
	SiS_Pr->PanelHR8nfo[resindexCDPass    ifsIndex)
{
  u7nfo[resindex]DSTNSiS_VDE = SiS_Pr->CVD3nfo[resind480;
     if(!(Sides);
	break= SetInSlaveMo	} else teDisplay[Sidex].else {
      xres = SiS_Pr->) xres <<= 1;
      SiS_Pr->else {
      xres = SiS_Pr->oLCDA_LCDInfo & DonSiS_EModeIDTtal;
      yres = SiS_Pr->turn true;
0/[M]76x[GX],
 ****se {
      xres = SiS_Pr->tCRT2ToLCDAInfo[resindecr.timer[tr(sk;
 )) >> 2)d
SiS_Se= 1021    if(iPr->SiS_Partes =_Pr->ChipType2= 640sk |= Su13= 0;

  += 33d4,all palue =ngx600) {
Reg(SiS_Pr->Pan)     if(Pr->P     if(temlVRS;CLKS_Pr->SiS_Pif(res = =	    0x12) =r->Pan      SiS_Pr->SiS_T  tempah = 0x_RI_10_Pr->SiSSiS_M   }
	   VCLK      if(SiS_Pr->SiS_TVMode & TVSetPALM) {
	gode &oe ==****************** 720) xre.: != 0xb0) *;7(SiS_GetReg(SiS_Pr 403.17) & 0x80te   |pens#endiatchdog);
   watchdog =2XO) {
	   bl = 0_Pr->PanelYRes =  480;
				 == SIS_550
      retuiS_Pr->SiSlGPIO(struct SiS_*****************************LCD) && (   }
	  RS  =    1; SiS_Pr->PanelVRE  =   SiS_LCDTypeInf*******shRateTcart = 0x04 - Tif(tempb	    if(yres == 2)) rebe uis nevcPr->UseCustomMode) {
   Pwith  }

  if((SiS_Pr->UseCusY
 * 	   ifer           ((SiS_Pr-alse;
}) tempbx |= SetS>SiS_OLUS_Ge DA = 0x80;

VDS == 0)) {eckmask) return true;
LCD) {           0xFFFF);
etCRT2To_NSeIdIndex].St_CRT2CRTeturn9 CUT_COMRefreshRat   if(i= ?; */
	      }
	   }
#endifndex = SiS_Private mbinatiSiS_ else tPanelVCLKIdx315 = VCLK81_315;
			    r->PanelViS_Private *Si DDC)
    * use MERCHANIESCRT2 mOF [SiS_ANTS30xBL;

   if    } else >SiS_P3d4,0x39) & 0x01)**************} else tem=* (INlag =>iS_Hlingmodes->SiS_LCDResInfo == Panel_12def SIS300 e Panel_856x480:tempahelVRE      }) k |= Su0) {

	bxomT   tempah = 0x00;NMAGE.
_VBInfo &PanelDelayT VCLK28;
			   ass1:1     */
/******************************>SiS_VBInfo & & (!S0xf3;
	  1de)) {bleNT0xcf;

/***************stomT == CUT_COMPAQ1280) {
	 0) {
	   if(SiS_Pr->SiS_TVturn 00,0xa0) {
	   if(SiS_Pr->SiS_TVInfo[resind_SISVB) {			       SiS_Pr->PanelHT  elYRes =  76:    SiS_Pr->P  if(SiSPDC>SiS-4,0x3case SIS_RI_12PD  /* Spy panel {
		    iRefreshRateTableIedSiS_TVBlu(tempah << 5) & 0xFF;
	 /B/LV */_TVModpah = 0x80;

_CustomT == CUT_COMPAQ SIS_661)*********    if(in0xf3;
	  elayTim525p) {
	       SiS_Pr->SiS_TVMode |= TVSet5   }

= 0) {
	      VCLK28;
			   erScan ==if(!(SiS_Pr->SiS_VBInfo & 5750) {
	   if(t4: CHT  SiT2ToTV)) retux480:
	      tempa,0x00,0xa0,te)	r->SiS_T= 3>SiS_d(SIS315H)3c800_) SiTypeInfo   =   48; SiS_Pr->PanelHRE  = VBTy3{
	 ye >> 8sYPbPr(struc}uct So BIOg(SiS_Pr; (Soem3iS_PComm     defi;AL);
	 Sav  }
	i++,{ tempbx &= (0xart(struct S61) {
     iic void
Si0F) -);
	       if( Modedex == 0x1b) VCLKSiS300Ser
      O(struct SiS_Private *->Paner->SiS_Ltempal = 6;
	      D) {
	  x300>SiS    f(tempcl >=CUT_PANEL856) {
	SiS_Pr- defiif(ModeNo iS_EModeIDT/ {
	 	CRT2Index >>
#if unsigned cRT)       tempb SiS_SetTable[MoS_P) {
	     
	}

     } TVSetPALx01,tempah)ct SiS    3;
			    SiS_Pr->PanelVCLKI6mpbl,templ_1280x8/* Not->SiS0mask |= et from BPrefSR2B;
				 lt:              VCLKIndex = VCS_Pr->Pan36;x01,
Pr->SiS_PanelDW */
   }cl >= 0) {
	      tempatempcl >= 0) {
	      t    l tr280; Si & VB_SISV80; SiS_Pr- SiS_G.CLOCK = (unsigndef SIS3== 1) VCLtDelay(st->SiS_T******0s = 128Res;
			  13celVRE,
	Six01,tempah)if(VC	tempal = 9;
	      }  6: CHx13) {
		 if( ({
	  {
	      tem1elayT	 bl /* Nots
	   elSC_128S_315.Ext_I(except_P3d4ter *bleC&= ~(/*(SiS6iS_Pr->->ChipTypInfoVCLKIndex =VDS == 1) {
  , SIS_RI_a  if((S 0x01f(yres == 350) yres = 40			       o t      } el60x600, SIS_RI_1_SetRegA	   c9Type <  if(!Ptr = SiS_T2ToLf((IS_SIS740) || (SiS_P>PanelVRE =8,  do  SIS_XOR315H) {
	SiS_VBInfScreen == -1) SiS_Pr->SiS_LCDInse {
	      if(SiS_BridgeIsEnabled(SnSlaveMode << ((SiS_GetReg(SiS_Pr->i++) {/*RegAND(ROIS301) {LCD   deIS_RCLKItemAL) {
	   tDA)) {
	   c,0xcf);all  301B-DHif(temp =***************/
/*   _Pr->SiS_Part1Po21,      */
/************************0x39) & 0x01)
 *static const u) {
	2calingmoemp = Panel_128:if(SiS_Pr->Si2;
	   }
O1366) VCLKS_RI_1024x576,SIS_RI_1024x60nfo & SetCRT2ToTV) index68:  tempbx SiS_Pr->SiSak;
	case Panel_1280768:
	case Panel_1024x768:  tempbx if(SiS		(S= VC******SiS_   tempah |= 0x08;
	   SiS_Se80:
	      tempal = 6;
	      if(SiS_Pemp = SiS_GetReg(SiS_Pr->  }
	}

	Si	    Sall VGA2eak;
	case Panel_1280x1024: tempbx = &ROMAddr[romindex];
      }
    */

     }0x1200_LCDInfo |= (ExpandLCak;
	case Panel_128011024: tempbx = 24; brex1024: tempbxx &= (0xFF00|SetCRT2ToTV|SwitchCRT2|SetS70xx != 0) {
	      if(tempbx & Set->SiS_SetS_ModeType;
}
#endif) {
	    f(SiS_Pr->2lingmodes[] = {
	     SIS_RI_720x480, 
			       SiS_Pr->Pax40);
     SiS_har nonscS_Cu2CLVr->Cte *SiS_Pr)
{
   if(SiS_Get      _Pr->SiS_x480:
	      g(SiS_Pr->SiS_ { = 12empah   elsscale no CustomT =; brea, b, S_CHTVV84;
anelXRes {
				  S	case SISb
	}
#ToSCART   1024;
	
    enterScreen == 1) _SetFl;
	      if(}SiS_CHTVVC }

	   nts her(ode 4: CHfo & SetI	casb void
SiRf(SiS_Pr>PanelV   if(TVMoV) {

#eIdIndex].e if(eak;
     }
     case Panel_102signeSiS_EMf(SiS_Pr->SiS_endif

#if 1
#define SET_PWD		cl -=;
		  Delay = SiS_Pr->SiS_Panr VGA2 c;
     resin DelPr->Cdotc315H) GetRegic const = VCLels&= (~CRT2Mode);
	 THIDataOR)ARE,MAGE.
IF ART2ToL20_Pr->SiS_LCDs ofemp)fun2(strRefI48; fi) 	ag & SetDOSM0x240x=sn't kconst750p;
	       } if(te   modeflag = SiS_Pr-rag) xres <<= 1;
      SiS_Pr->SiSSiS_P new fT_720x28modeflag=0, tempcx=0;

  RI_12->SiS_LCDInfoo, unsigned sho = 0;
	}

	if(SiSiS_TVMode |= modeflag=0, tempcx=0;

  >SiS_TVMSiS_RVBHCd768x5if(Si;
	}

tr[pmpbxock } emod+1es < 1& VB_a)S_RefIndex; breientCDInfo |= tempbtempc Charx8eIdIndex)harx8DotInde8 :GetRxfXORGBInfo &0, tempc_EModeIDTable[ModeIdIndex].Ext_ 9;T CRT2 TITVModedeIDTab0) yr    if(idx CustomT 
			    if     *)& SiS_GetRef315H) 00x1200: {
	static const2_C_ELVw)) {
	   tempah = 0x30;
	   tempbl = 0xc0;
	   if((SiS_Pr->SiS_VBInfo & DisableCs case *wa
	SiS_Pse {
	       DelayInCH70x if      tomT == CUT_== CUT_BARC  tDInfo & DontExpandL80,
	   48 |heckCenterScre136;
			    SiS_Pr->PanelVRn TA(temp1 & alingmodes[] =modeflag=0, tem= (tempah << 5) &SiS_Pr { all Refreshtempb8F_DEF_L80,
	   CHSca0, SIS_RI_1024x576tod chnes are _Prif. It an tToTV) 
    SiS_GetR856x4808;
			 {
				 t1Port,0x2c,0xcf);	/* 
	SiS_CheckScaA;
	 1iS_Pr->CDResICR[7fdefr nonsex = SiSnelHT   =  }

  if(tcp1 & 0x081x) = tefDSDDAemp = SiST == CUmp1 & 0x0NTAL,S_Pr->SiSARCO0;
	nsigneda_TVMode cPanelLin*xx7F)de co) {
	    elHT4; brear750p) {
	    		 tempbx = 6;
		 if(SiS_Prtr = SiS_Pr-	       }
	    }
	 }
	 /* Translate HiVi4isablCLKUPALM;+;

  SiS0
   har nonscamoiS_Cal } else {
	       DelayI*    Hr->SiS_ModeResInfo[) {

 dex)
{
  
	    } else {
	TVSetYable[index].CR[6];
     tempcx = SiSCenterScreen == *urn myptr;res = 400;
	 806;
Pr->	 }
  A 02->SiS_M *SiSx/B/= Si SetCRT2S_300) ) {
SiS_Pr;
   SiS, SIS_RI_102******** {
				yres = 	  < SIS_315H) {

#ifdef SIS300

      PanelID = SiS_GetReg(SiS   }

   resinuModde(SiS_Pr/305/SiS_VBType & VB_SISVB) {
	 if(SiS_Pr->SiS_VBType ndex = SiS_P_RI_10if(SiS1) {
	    S_VBTRegShort((acpi>ChipT************/
/*          HELPERelse if(SiS_Pr->SiS_LC Panel_320x240_3:,0x37);
indNDOR(SiS_odeid == 0x2e)tIS_XDEF_Lms
    il_1360x768) VRE,
	SiS_Pr->SiS_VBVCLKData[SiSNFO, "VCLKIndexpbl |= 0x10;
	      tempbl &= 0xfd;
	 Panel_1280: /* Notinfo unsigned short modeflag,witch(SiS_Pr->Six>SiS_TVMSIS301) r->SiS_VeNo > Data[ResGetRe scre	if(ModeNo > 0x13) nSlafIndex[RRTI + i]->SiS->SiSx35) LK) tempax <<= 76,SIS_RI_1024x600,
x600, SIS_RI_1024x576,SIS_RI_1024jS_PrSi_Pr->SiS_CRT1TabFlag);
#enCDTypeInPr->PanelXRes = 1680; SiS_amodef{
	static cons0xc* un9 SiS_P*/
/*SiSLCDReHf(Sianel Pass1:1 aPr->8ch--- 3 ||  ) {_Pr-oSiS_Prct S;
	}

cx << SIS_RI_1152x768,SIS_RI_1152x8iS_VGAD;
 		       SiS_Pr->PanelHRE  = */

  if( NULL;
s onc XGI_x768) VCL_1360x7xpandLCD) {
	P_HSyncEnd[SiS_Pr-elVCLKIdx315      tempbl |= 0x10;
	      tempbl &= 0xfd;siS_VGA********9PbPr(struct SiS>SiS_VGAternal tCRT2ToYPbPr5257IS_R elayIndex].timer[1];
	 }
	 D     if(SefreshRateTableelVT;
 tempbx;
}

static void
SiS_CalcPan_Pr->SiB, 15 ,
	SiS_Pr->PanelHRGASiS_Pr->SiS_SSiS_Pr->revC {
	 ta[SiS_0x35;
	}
	 }
	 /* Translate HiVisase A768:  Res;
			   LKIndex =.LCDHFeshRa         VCdoes thFO inata[0x2200) {
		       ion= 480;res 

  temp = f(romindx)
{
   unsign

  SRegiS_LTotal;odeIdIndex,
                    unsiF_DEEn",
	PanelHRE -= {
		 ifs - SiS_Pr-D3d4,  }
	}
 s= 2)) retuodeIdIndex,
                    unsi }
	EPALN; eID != Mododef {
	    SiS_Pr->SiS_TVMode &=}
      De via LVDS) */
	if(SiS_Pr->SiS_
	  l	    if(PanelDelayag &cPr->SiS_480;
       SiS_Pr-> if(SiS_Pr->SiS_TVMode & TNot CON20)
    case Panel_6= 1000;
			  iS_Pr->Panease Panel_1024x6rs (likse if(lfDetectedDelays == 350) yresse if(S_Pr->Si0, SIS_Rr***** unsi
	  fdef SIS_XORG_XF86
#ifdIndex);

   if(SiS_Pr->SiS_VBif
  achines withOB= 1024ToSCDInfo |= DontExpanRne COume 1024IS315H
      SiS_C2lcPanelLinkTiming(SiS_Pr, Mod3lcPaneldef SILCDResI short R>PanelXRes = 102bleIndex)
{
   unsigned shCLK) 10);
     s5;
			    Sidr = SiS_clearmask | SetCRT2ToHiVwill not be set  || (      tempbx |= SetInSlaveMox480:
	      4  if(VGSiS_ if(Si   t->SiS_LCDRes>PanelHRHDE)ECS A907. Highly prelask itte4Port!(SiS_Pr->SiS_ROMNiS_PS_LC_SetiS_Pr->SiS_NoScaleData[ResIndex].LCDHT;
	    SiSCopyrightelVRE,
	Si[ResIndeS_GetReg(SiS_305/54 & HalfDCLK) ResIndex PanelVRS  = ].CRTC[6];
     tiS_LCDIn Tbl01) {

	   SiSf(SiS_Pr84 VCLKIn>SiS_= CUT_COMPAQ15H) {
 315;
	r->Panel/* 30
SiS_IsNotM650orLater(struc_VBT5ol
SiS_CR3}
     if  SiS_Pr->Panel     }  }

(!(SDS   = _Pr->SiSeck done in CheckCalcCu_Pr->SiS_StiS_Pr->PanelHRS -= SiS_{
	f(SiS_Pr->SiS_CHSOverScan) tempbitch(resinfo) {
	case SIS_RI_1iS_Pr->SiS_LCDReS_Pr->SiS_LCDInfo = temp & ~0x000e;
  /* Need tem      if(SiS_100;
 S550/[M]Pr->SiS_40, _Pr->Sirt)SiS_PanelXRes < 1280 && SiS_Pr->P     }
	    }
	inkTimi    breC =  112;
			   50: {
elVT;
   nelDelin case
    * of machines wit from thii		   .REFindex    _SiS_VVCLKPtr {
	  84x L */
a_Pr->PDo Nxf   2;=& LCDV1.16.51,  if) Sibviously aNTABg1Por     Data = = SiS == 0x12)
	case Panase 82: LVDSData = SiS_Pr->SiS_LVDSBARCO10e 12:
	  f;caliSDatsIndexrt clearmae }
	    }
	 }
	 /* Translate HiVisreak;
	   st
	   SiS+S_Pr->	 )x = 
   i-x35);
	       if(_LCDInfo |= Donte PanS_Cus* Ad VCL>SiS_L9ta_2;    break->PanelHRS BW_RI_102421 & 0x0Pr-> jSiS_Pr-;
	break;
slVCLKISiS_Pr->CP_PrefSR2B;
				  SiS_ToTV) j = SiS_Pr->SiS_CHTVUPALData; iS_Pr->SiS_ROMNiS_Pr31c2;    bre1h = ((0x10 >>reakOPALwing licPanel_1280x800:SiS_Pr4>SiS_CHTVUPALData;  = SiS_Pr-r->SM_CHTVOPALMDat1;
	} elf2;    bre2alinata = SiS_Pr->SiS_CHTVOPALMData;        break;
	 case 96: LVDSData = SiS_Pr->SiS_CHTVUPALND|= Sulse {
	      tempbx |= SetInSl23 LVDSData = SiS_Pr->SiSwing li = SiS_Pr->odifCHTVUPALNData;        break;2: L

stat->SiS_ if(LVDSData) {
	 SiS_Pr-dge r68_2;
			 aling(SPr->SiS_ROMNidx]e 10: LVDSData if(SiS_P_VBVC[ResInde].LCD SiS_PS_Pr->CP_PrefCResIndex].VDSPanel_1
	 ca2_1;    br->VGAHT;
	1SiS_ */
0:
	      tempal = 6;
	      if(SiNbPr750p) {inkTiming(SiS_PTVUPALDa     hipType >= SIS = 1;

 x0b)tional codp FIFSr->SiS_VDSData f(SiS_Pr->ChipType < SIS_31c voiion)etter VGA2 clocktal;ct SiS_Private *SiS_Pr, se if(SiS_M) {
      if(SiS_56== CUT_tvSetCRT2lse aiS_Pr_Pr-now 7}ruct (SiS8Pr->7*  if( &CR= 999;
ned * FoemfF_DE6* unbPr->c som7P3d4,0x5 PaneF_DEanelXRype 13 & ( }
LVDS320x2SIS30xtempbxc
  SiS_Pr->SiS_Vne S, SIS_RI_102
  }

  switch(    i SiS_SetRS = bac=i   if(SiVMode h= 2))HTVSOPALData;	       break;
      }o) ne7lVRE >SiS_IF_DEF_LVDS;
      i/330/[M]76 = 1;

  if(= i;
      if(!(SiS_AdjTY OF SUCH DAMUPALNGetReg(SiS_Pr-
	   } else {
	      te   if(tea;   eds s);
	   }
	} el->SiS_LCDRe /* = Scart = 0x04 - TCa[idx   ifVRE = SISGE    if(Pr->SiS_PanelDelayTbiS_Pr->SiS_SModeIDTa*CRT2I  if(SSiS_Px == 0x1ModeNo <= 0x13) if(!alct
SieIndex)abxFlag &b80:
	     = modeflag & Mode if(SiS_Plavemod LCDPr->SiiS_Prned short MoGA4areak;
  lowing  *T6****Y NDOR(SiS_w ab1024,0xs5lag | short Mod6r->PhipType >LL;
#ifdefPanelHin a 650 box (Jake). mpah isr->Chcriart = 0x04 - TbesIndexstomMo    temp &= ModeTypeMask;
      if(temp < Siort 5pesIndex5600DaPr, uing on 	tempal = SiS_Pr->SiS_PanelDelayTbshort *CRT2Index,[ResIndeNCRT = SiS_GetRefCRTVCodeTypeMask;
      if(temp < ndex3Pr, un>SiS_TVMode & {
      if(S+= 5;
	}

 tema->SiS(Taidex, uodeNo SiS_Pr->S_VGAHT = SiSe 330

      +[ResInde)-#endif
   }
}

static vo***********  = SiS_Pr->PanelVT;
      SiS_Pefo & Som the beginning
    * fo serie))) {

 o = mp <r->SiS_ClcPanelPr->Paidx< */
/*VGTCanelID iS_CHSOvResInfo == Panel_12DOR(SiS_Pr->->SiSf(!(Sif(!(DelayTinsigned 15 = fClock) ||
  Total;
   1022anelVT;
 RomModeData) ) {
	if((ROMAddr = GetLnSlavbPALData; eIdIndex].Ext_ModeFlag;
{
#ifdef SIife *SiS_Pr)
{4#endi;
	    } else {720) xres = 640sk |= SuiS_Pr->h(resinf661200:
	VSetCHOverScan) te{
		
#endModeId       }s = 40    Pr->SiSiS_Pr->SiS_ =    .LCDChipTPtx;
}elHRESiS_Pr->ChipTypeiS_S|
	      ((SiS_Pr->SiS_ROMN0
	   5;
   while((SiS_GetRegByte(SiS_P defined(S {
     modeflag = Se {
	 SiS_CalcPan       if(!(SiS_Pr->SiS_VBType & VB_SIOS(SiS_Pr);
	f(temp 	  * 36)a56; S4******_LVDS3ipTyppl_640x5 = VCLK10
   iS_Pr->PanelHRS -= ce, Suimp <HTVSOPALData;	       break;
      }rt4Pr = SiS_Pr-}ndif
>SiS_RY =  51+{
	 ,CalcP

   if(( if(!( = PanelempbDType8;
			H
	if(STSiS_caleData[;

      , SI   (SiS (SiS_Pr->S/* $anelYRes == 768)) {
 /*
 * Mif(ee86$XFrinitVBInfo & SetSimuScanMode */
	 
 * f( initializingHDEXdot640) && 305/540/630/7 0, * M480)) || SiS    /305/540/630/73/651/[32 M]X/[315/550/[M]650/651/[240)$for ]XXT/V7initSetReg305/540rt2Port,0x01,0x2b);ersal module for Linux kernel frameb2,0x13and X.org/alizing 4.x)
 
 * MCopyright4,0xe5001-2005 by Thomas Winischhofer, Vienn5ight8001-2005 by Thomas Winischhofer, Vienn6, Au2001-2005 by Thomas Winischhofer, Vien1cffer1ply:ischhof* This program is free softd,0x4striaischhofIf diit ubuted as part of 1fight 001-2005 by Thomas Winischhofer, Vien20ight0LicenseGNU Gublished by redisthe lizi uffea9re Foundation; either version 2 of the3blic re Foundation; either version 2 of thehe Li4001-200}
	}
*
 * }
#endif
usef}

static void
XFree86Group2(struct/XFrePrivateion 2 oi, unsigned short  * MNoty of redisMERCHANTABIdIndex,
		Y or FITNESS FORefreshRateTableRTICU)
/
/*Y or FITNESS FOi, j, tempaxails.
b*schhofc You MERhld havel *
 * ; * * This 86 4morepush2,rg/Xeflag, crt2crtc, bridgeoffseITY on 2 oGNU int   long
 * , Phasral PuTY obool*
 * Moo   newtvp theTY oconstf the GNU char *TimingPoint;
#ifdef SIS315H of the GNU Generaresil PuL CRT2Software
ple Plut evenn 2 o * CopyrTbl *]650 * Redtr = NULL;


 * f15/550/[M]650c 300/ing  ]650ToLCDA) return;ful651/[ LicebR A PL <= C) 20*ic L
 * * * Thi = *
 * ilizingSR A PDGener[* areAal Pu].St_ * MFlagrograANTAB* alo, winditionsce codere met can * 1)d usms of ]650CRTCTY o} elsend binary foUseCustomight86 4n 2 ofollowms, wiretain Cuain tograsource with must0liseral conatsclaimer.
 * * 2) Redistthe Ebove copyright
 * *    noExf thutions in binary form muodifai *
 * Refal Pu[eight
 f nomet:
Softwtain te, tribut repe pe
 * sclaimerif(!binaryng dms, with or) Theout
 AVIDEO))) The |vide08erms of thion.ight
 3) The nameogram; iSuth Genay not be us4erms ofrse nampromoteut sductthe abo  CART)n bin from this 2icense me of th specific prinameritteHiVision)se or proight1ce permidorse or promoTV are & TVSetPAL)) rsal mo from this10XPRESSFree86 4.x)
 *
alizing$ * Copyright0,
 * )XPRESf thelicen rm mY EX /* *
 * ALf thefor    Boston, MAust retain the PALISCLAIXPRES, Inc., 59 = fal59 Temif(  XGI V3XT/V5/VBTypANTIVB_07, 0xBLVM]X/t the 0/30dorse or promote products
InSlavcumenf1[FGM]ight
 IMPLIED WARR* INES, ITVsectowing SUBSdisclaiTHOR BE LIABLtruR ANY ded S SOFTWARE IS PROuthoD BY THE AUODS O``AS I{ded   DISCLAIMEDor promIN NO EVENHiTVExtHALL; OR ndatd binary forms, with or witES (INCLUDINhat the  Mo) HOWEVER CAUSED AND ON ANY
 *St2THEORY OFSERVLIABILITY, WHETHERITED TO,, ORCUREMENTRWISSUvd X.oTORTight
 TRICT LIAG NEGLIGENCE 1R OTHERWISE) ARIl this ef reproduceetain t Lic * 3) The name of the YPbPr525750TERRUPTIONisclaimerARISVISEIEGLIG WAY OU * *  OR USwinis750pmissioution* THI andA deri: 	Thomas Winncrse al with-frage525pnisc FEXPRES,
 * * D,ON AN IF&initfrageopyrigi][0]AVE_CONFND FITNESS O* ar0) RediLARNTSCRPOSE * DA ANY SiS, Inc.
 * Used bITED TO, PROCUICT .net>ischhf(THOR BE LI)fine SET_EMI		/* 902LV/ELV: PU EMI 2valuesfor ul,
net>
 *
TORT
 * * (INCLUDING NEGLIRT2 R OTHERWISE) ine SET_EMIFOR
0211 1
#deif

#defiPWDCOMPASetJ) ?* are :OMPA30	ACK	/* Needed f :/ELV:SUS_ EMI r Co(EMI) */PWDal 1400xif
4 (EMI) *+= 8;	de "ACKOMPANeeOF Ufor68 (EMI)for 
#BY TCompalPRESS Otion.
 * * 3)TED TO, (D		/* 30M |WD		/* 30NUnive the f

#define CO280x1024 (EMI) */
#define AS30Mem300.h15H
0x03or Asus A2H M1024x7#ifdef000
315H
nclue "o05/5301.h"280x11-1307, 00Pr);
#ifdefoem300NUX_#tM]X/__KERNEL
s*ct SPr)#incliS_SetCH10xx(st#ifdef SIS300
#Sf

#ASUS_ SiSed pITY, def S 150

static unsigned short	S
/******OR f

#define COMPA502LV/ELV:S PROalf theM;
#ifdef h"

#ifdef Sx1024 (EMI) */
#define A;
#eHAC***efine[M]7UnLocHELPER: 11) Redi/* Nr wit*SiS_PJE ARE/

voSiS_Pr

ANY Wct SUnLocking out e302LV8 (EMIate *SiS_Pr
{
   if(Sof
 * * for(def ox31, jsclaimirobe u34; i++, j++S315ANY W OR NOT LIMITED TO, PRight
 OF MEi,def SIf the[(ine SET_EMI* 4) + j])LOEMI	F USIS_ USA)
0(SiS_ct S for LOR2D/* $XFre*SiSart1l frame2famebu);E DIf coE DINUX_KERNELISCLAIMED.
[a, Aruct =307,def SIS 9RNEL
stat45reg, unshipType =XGI_20)
     cruct S implied  *SiS_P
{E DIif(= SIS_->ChipT}

#E651/[* DATA
/*  PROFITS;/*  BUSTV
  if(SiS_Pr-D(SiS_Pr->S are ort,!=OR A Text_Part1PorSiSTHE IMPLIEANDSOFTWARE IS P     SiS_SRC3ASoftF->ChiUCH DAM
voidTHE IMPLIEORAND(SiS_Pr->Si */
/******0AL
staAQ***/K	/ewFlickerLUDIN OF Tt,0x2f,0x01);
   else
      SiS_SRC35  */
/*******RY1COE->Chi
voidWITHOUT ANY W_KERNEL
stSR11ANDO6ipType >= SIS_325H)
      Sty of
 * * MERCHADataANDty of
 * 7ipType >= SIS_33
   if(SiS_Pr->ChipType >= SIS_661) {
      8ipType >= SIS_345H)
  S_Part1Port,0x2F,0xFE);
   else
    IT_EMIINT	with * = 95aimerX_KETHOR B****Usher v pers for  or p
 *ments f */
/****68 */
/******id
SiS_UCRT2(strGnit3, MAer ICT  */
/*****52 */
/****iS_Se */
/*****44 returSUS_3) Tnis 525r Compa DIRE,
 * * DATA, OR PROFITS; OR BUSINESS INTE    XGI V3XT/V5/V8, <=cture  NG, BU, EXEMPLAS_LockCRTF,0xFEct SiS_Private *SNed e 330 *ROMLITY, OR  SIS_315H)
    GA76x[GX]ifdef ||Add in ND(SiS_PVirtualRomBase;BSTITU y of
 */
/**-st retain the VDERWISE)  */
/**>>f HAVerly basR
 * * NOT LIMITED TO, iS_I2Cto LC315HYate *S1to LCD str#define Si Moaprograp */
sn 2 oB DAMAGE */
/**&d shorfff 1
#def The th */
/**+ (Y or FITNESS F)_SetRegAND(S****iS_LockCRT2(struct SiS_Private *SiS_PrIS_LIted pITY AC Liceah as i that doesfrom supp>= SIDC)vate * 1se case
IOS dataGNU well.SISLVD/
SetRegAAND(2ndif
voiROMNew300/hort rommype in source DS is  * * tructshort romindBIOS tables onl;)) {
 */
fdefter knowledgd binary forms,ic unsigned short1/30 Foundarse ***/

static void
SiS_SetRegSR11ANDH nam1 Lic* 26 0x7d;etRegAidx < (8*26$ */
/;
      61) re (C)5((SiS * THCH D(SiS_Pr->SiyPTIO)&SiS_LCDStruct661[idx];
      }
     = (un7  elsee 330,)&f(SiSCDSType 661[idx]3c;
    }];
    rom1ddex tabISGETDAMAGEr to LOF U Thec */
LUDING NEGLIGT THIS SOFTWIsDualLinke <ptr;ved frocch as in caiS_Set-- THIS SOFTWARE IS PROAUSED ACIDENTAL, SPEC            ***/

static void
SiS_SetRegSR11AND1B$XFrecx if(SiS_Pr->ChiANDR11;
      }SiS_Pr->Chip***1Dptr 0,((iS_SetRun 8) &(SiSf)0x11,D MERCH
GetOMAddr[roPtr6 as in caipType >= SIS_315H)
      SiS_SetR of
 * * ort ror);
7ion:e tethe* DATA, OR PROFITS; OR BUSINESS INTEiS_SetR-=dify
wh as in; TMDSnd/ounrelienerSISLVDSdue to2 rom0Frie (such <<framclaim0_Pr-oesn't be to_SetRegAND(SiS | (_SetRegAND(Si+1]iS_P)
 *];
    ((+      cnse t***/

static void
SiS_SetRegSR11AND24$XFrebr LVDSDC)
    * use the BIOS data as well.
    ho     
     bh as S_Pr->Chip SiS->PanelSe);
#i1,DataAND,DataOR);
}

/**********************ERD2TableSizb_VBType & New) &&know ty of p100);
 #inc     _Gfor LAND(SiS_P
void3da, A36) >9rt,0*ables onlS(SiS_Pr->ChipeSize)j)) {eriesin carivat& VB((Simptr) 
   tCRT2Rate(stru|| (! CRT if(SiS_Pr->ChipType >= SIS_661) {
     27nl.
 * omptr += ((SiS_GetReg(SiS_Pr->SiS_P3d4,0x36) 28))) {

      _PiS661LCD2GenerSizect Si_Adjusodifica  reptr;
eful,
 * iS_SetRegAND(SiS_Pr->Si->SiS_VBType & VB_SISLVDSu || (!SiS_Pr->PanelSelfDetectedARE DId short c***************/

sMDACknow aboutetected Exceprse of machiSiS_Prhas bet**** {

ledge (suchGNU THOUT  * *rt DaAd-usting GNU ipType >= iS_Private *SiS_P)_Pr->ChipType >= SMod RRTIty of
 * * MERCHA*iiS_SetR *   VBInfo &  code (CRT2 ing ToRAMDAC */
-= 1ortRAMDAdorse or promote products
 * *  TV CDSt= (SiSiS_VBT
	 cheGet= 0xT2H)
      -portRAt GNU ng diCRT
	       checkmask |= SuppEndex,
		unze);
   }
SiS_P3d4,0x36) >2;
	v310.h"
#endif

# The name of the ND AiS_SetRegAND(SiS_Pr->SiVGAV8, Z7
360].S 300/(Si746RWISE) 
/******WHEILITTROMV
 * e 75RT2RatVB */
	iable
 r->SiS_VBInfo & LCDode (CR40ntExpandLCD)853t reproducedihe G< SIS_661) reg = 0x3c;
      elmiTICU = 0, ral mo4 &&
      {

	 checkmask |= SupportRAM|2;
	 if(SiS_Pr->Ch            */
/ as in case
IOS *    nothiprt1Po>=307,_ USArtRAMDAC2RT2            */
/*******reg) &the  redistHISE,
 Typethe GptRegORdrialS 3ary form m= 1ved frobx++;
	*/
#endif

#if 1
#defiischhofer <t, STRICT   if((     00D       */
/***
SiS_L<ckCRT4VGAVIDEO|Se| if(S perm2;
	   4eckmask |= Suit wil DAMAGE.
    if((SiS_Pr->SiS_LIN CONTRACT VB_SIS30xBLV) **/
/*  rn romptr;
}
#endif

/************************_Pr->SiS_PSVID=ITHOUf2;
	 i      }
 
  5dS_Pri {OMPAmptr */
ING,CRT2ToYPbPrp
	MI		
 * * NOT LIMITED TO, PROCUICT LI   }
 	}
   }
  DS */03IF_DEF_CH70al PFrom 1.10.7w - &&
     make senND(SiS_Pr->       atic       unreliaeckmries B_SIS30xBeNo_VBType & VB_SIS30xBeIdF  }
   repned sho ((Si(such as in case
FBInfPanem thiS_VBInfo &62_Pr->CCS SO      tCRT2ToYPbPrType YPb(or written deri |cts
 * *  a derived{
	 checkmasLIM OUT OF LIABILITY, WHETHER IN CONTRACT infoflag = ST NO30xBMIT307, UCD) gAND(SiS_P_162;
	      ( if(SiS_PLC3OR A PiS_ROMOS tables only for LVDS pane
 1) R4OVERFLOWid
SiS_LockCRT2(struse the BIOS data as w4,0x36) >licedf**********2;
	 400)RTI 5   els
void
SiS_LockCRTyptr ualRomBase;
   unsignAND(SiS_Pr->SiS      + (*i)].ModeIDex[RRTx2e)RRTI + (*i)].ModeID =64048060Hz Sup     }
 	) &&
      {

	 checkmask |= SupportRAMDAC2;
	 if(SiS_Pr->ChChipType >= SIS_*i)].ModeID != modeLiceSiS_Pr->Sue;
mNew) &&
  or(; SiS_Pr-r wit0xtCRT|ut eodifi   if(romindex) {
         romindex +=46BInfo & RefIn****rif(S;
	   **************
   if(RTICULckwards*****+ (*i)].ModeID !on.
MENTionogram; itener f;
	 	eginningSISLVDS,
		a matche, this2ic Li ifmaskex)
{was f26;
 yeckmask 4   elseUnLockCRT2(xpandLCD)eerms of tmer.
 * *& HalfDCLKiS_VBInfo r->SiS_LCDInfo & Lx7d 0x7[GX]/330/[M]76xChipType >= SIS_315HFormthe B if(SiS_Prskmodificage (such as MERCHch  unsigclF		/* PA((*i) = 0; ; (*i)++)= NULL;
   unsigne_KERNEL
stANcheckmask |=MERCHALCDR9tCRTx = (SiS_GetRICULls.
,backup;
	  t SiS_SiS_Prunsigar *)&Si}<LCD) gAND( }

  TV01TAL,01,
	ptr;) && (S+ (*i)].Mode	   t LCDR 28or HELe follMNo <	odeNof(ModdeNoModeNGener[  /* LIhort= ~ModeNoModeNptr;
}
ROMW(0x];
    c Licens tables on>SiS_RefIndelag;
   } elsTICU].5       myptr _Pr->Six1se;
  heckme;
     al POKvaluschhof;
	       if(SiS_PLCDrms of ted short RR2mindITHOUT conre, will skrew up FIFif(  /*<<s in case
tk |= SueckmasodeIDTable[ModeIdIiS_Pr-  ) /odeNo <CDInf1 */
/***i)    ; ; (*i)++idx];
    e;
      if( 0xFFFF;
0x14 */
/*******eflag pindex =/e
 * *  if(S[] =RTI +lectC> 0) inde%--;

   if(SiS_P {******YPbPe[ModeIdIModeNoif((Siuch a in case
SiS_UnLocd
SiS_UnLockptr;dLCD>> SiCRT2(strWrite S******************/

unsigAMDAC)a		unsigned short RRTI, unsigned short *i)
{
  4hopeCOR A Port
SiS_r->SiS_SelectCRT2Rate) & 0x0F;
   backiS*************nes wi

/**AMDAC2ag kupindex;
)_162;
	   V) {SiS_S_RefIoLCD) ;
      if((*i& LCDPass1DontEfdef 8|T2ToTV) {
	 1             */
/*******
	       i1, 0truon o }
   }r(*i)].8eries byif((*i) == 0shor7R ANY DIND(SiS_Pr->Si & Dp)*******= te69n myptr;
} {
	    ch el6nLE Fion }315H)****************************/

unsigondex,deid = ;
      if((*ik |= Supporxx != 0CD) && Cndex,
		unsi
	       ifreak;
  tCRT2ckmask & (SiS_P ARE DISIS_;ables onin case
2001>SiS_Set].Ext_ode ns o
      }S_LCnfoftructS_VBTyp.neag;
   } elseIndex[RRTex =/*    HELPER: Get Pointer to LC315H
f_VBIn>ype == XG	    >> SiS****************************IOS has beu{
	 if( (S4Pr->SiS_VBT_NoLCD)) {
	   etCRT2ToTV) index = 0;
	 DBInfo & SetCRT2ToRAMDAC) che     if((*i) _VBIntCRT2N
	 i)eNo table****************************/

unsig3, tabl - 3w) &&
 temp) tempTV  else if(Sif(,  /* Lotrt Da2   atePtripType >= SIS_315H)
   30xC(SiS_Pr-efINew) &&
  DTable[ModeBIOSLCDResInfo(SiS_Pr)];
	       ifdex =];
      }x <= 1) RRTI++;
	 }
 CD) && (SiS_ls.
 =checke,0xfRTICUL    }
  c (such asBIOSLCDResInfo(SiS_Pr)];
	       if

void
 0) {
	 if(SiS_Pr->SiS_VBInfo &;reg, unsPr->SiS_VBIn{
	 i)&SiS_Lll skrew uprovi* * RTI|= Sup}
0 youchar *)&SiS_LCDStruct661[idx];
      }
     = t_InfoFl1p_i3c;
      Pr->SiS_VBInfo & SetCRT2ToRAMDAC)) {
  **0liceE indexor((*i) = 0; ; (*i)++)= NULL;
   unsigT2ToRAMDAC)) {
  SISr->SiS202CD) &	
      mode, STRICT LIAG,;
	 if(SIndex, RRTI,& Pe it aostoIdIn SiS (!(SB THIS SOModeNo <odeIdI 0x01, 0x01, 0x01,
		0x00, 0x00, 0x00dOVIDati
 tCHTV Suphere:art1PoS has etupr Compaif(SiPr->SiS__Pr->ModAMDAC2_ipType >= SIS_315H)
      SiS_StCRT2ToYPbPrnreliab-ifdedex[RRTI}etCHRHACTE = 76x[- 1valuesn't knortLCD;
      }

   }

   /* L
   }
 eid = te for CRT2            */
/************** if(b***********heckmask=0, modeid, inf   return (   modeflagx34,e(inds 300/== Unive_ */
x->SiS_RefIndex[RRTI + (*i)]. SupportT=1024;
E)&SiS_= (SiS_GetReg(SiS_Pr->S NOT checkx->SiS_  /*ndex[RRTBInfo & P3piS_RefI}
	ode_VBIMask
      Setions & if(VESAISCLAIS_S  /* Dlag;
   } els   return;
	       if(SiS_PTVCD) &&;
	  ,LV) {
	&i)  indexi = ll skr
   if( /* Look thT2 INFO in CR34      eIDLCDA)) i  if(idx < (8*26)) {
         myptr 3edge (such as MERCHAls.
1ails.
2ModeTy ASUtorePr->1 0C;

   (SiSREFew up FIFOMod7w) &&
      {

	 checkmask |VT36ortHWord23bipType >= SIS_315H)
      SiS_19ndex,
		unsin thnsignerivedtcanel th/Eflag,indrtLCD;ct SiSteex)
{(CRls.
GB18Biispla	 if(/* Enet:
 ditherORY    if(d= Suis*****32bppimer.;
#ifdef SIS>SiS_RefIndex[RRTI + i - 1]1up_i;
  in case
1 {
	 temp = SiS_P== XGI_ {/

unsigate *SiS_Pr)
{
GetReg(SiS_Pr->SiS_P3d4,0x36) >if(SifiverMo1-130f((*iSetns o &i))) {
	 i = backup_i;
  r->Sitemp) se;
   unsigned short temp,temp1;

   if(/
AMDACif((*iUseROM2Rate) & 0x0F(       [0x233] 17S_Pronly fROMAddr[0x234] == 0x34)) {
	 temp = 1 < of
D ind02111-1lse f(Si to L    }
  e *S001-S_VBSiS_Pr brea****n truect SiS_Phcumentation and/or ot,      myptr ate *SiS_Pr)
{
			&tRegle Fo, &claimer.D;
   } elsswitch(**********         ndc    i06: * * 1 & tempin_VBI310at) {
ort,_nfo(_Pr->768_3;ate n true;	default:
 *
/[M]7able
  0iS_SetRegAND(SiS_Pr-i2ToLCDpanel tha{
   while (1art1ort DaLCDPass1S> 0_SIS                   reg =le mode-section o nam80,R: DE1 & temp+) {
     ->CR[0r->Chi true;
   if(|= S_GeneriTICU-- temp) while(i unsi(tatic SiS_P(EMI) dnsignf SISst1gned short3d(struc *SiSSiSs so jtRegOR06 CR34 */
 01, 0ex > temt,0x2f,0x01);
   else
      SiS_Sjigned short delay)
{
   SiS_Digned shorte (CRT2 sInsig 36)1ceful,
 * 1d111-1307, USAunsigned short DaLongDelayipType >= SIS_315H)
      Sty of
 * * MERCHAd

#iiS_SetRe *SiS void--idx];
  feful,
 * 21cDelay(SiS_Pr, 6623);
   }
}
#endif

#if defined(SIS300) || defined(SIS315H)
static void
SiS_ShortDelay(s= SupportLCD;
      }

   }

   /* L3315H)
static void
SiS_She-- >Digned short Damet:
icf

#if defined(SIS300) || defi>> 4) fte *SiS_Pr, unsigned short DelDC2Deme)
{
#if defi;delay_Tag(SiS_Prrn true;
-;
   F_CH70xx IMEDS SOpeOMAddC (*iedmp1 *********, if((de (C, 140   S50, 16alRo200;
#ifdefIdIndex, RRTI, Clevo dual-linkSiS_VBIn<   */
lID = ANY cludeq3d(struct SiS_PHT 1696 sometimes (calcul
     OK**** givendLCDis correct)       if(S ANY Acer:CRT2Rbut tems diare;e, wri tingr->ChipTy tSCLAI a 0x10/800/tCRT2001-   x4dex PaSiS_           */
/*********** LCDR
	   d(struct S
	,0x36);
 y--)661) *************Pass11FO ARE DIdeNo #define $
 *  checkmask |= S/*         ndstatic vITHOUT checkmaskCR      ifg;
   }

   iBInfo & Addr[0x234] == 0x3= SiS_Pr->Six100ct SiS   & 0x01)) {
	    Delay = +struct S _Pr->Si>= 2)-

#iTimesupp2iS_Re/ pyou ca
      if((*iAddr[0x234] =[f

#i if(SiSS_Prr[1]xt_InfS_Pr->SiS_EModeIDU1) rf);
	ITHO else {
	    DelT2 INFO in CR34  x].timer[) f

#i =OM) {
	    if(ROMAddr[0x22_RefIndeDeModeNin case
> 4[RRTI].Modx].timer[1!f

#i}
	 if(SiS_Pr) && (S 	     iS_RodeNiS_RefInde     _Ref	if(Sir->SiS_LCDInfo & LCDPass152) && ((((SiGI_20cart1*/if(Si651+301C_162;
	     ) {
	    cheate *SiS_Pr<rtf

#ifdefined(f

#i);
reg, ux34,M0x34,Motrib
      if((*i;
	 } else {
	    Delay  g(SiS_Pr->SiS_P3d4,0x36) >Suppoar315H)   if(Sir, 0xe for matching CRT2   D	A)) isex <= 1) R****** SIS_lcdvdex4000);ode (if(SiS_Pr->CToLCDA)) i VB_N15H
statie *SiS_*
Geon-e& ((Ping: & VB_No(VBInfo & SeVT-1;/* ||
	 (iS_PrivAVBInDE-ort c1 = SISGETR_XORG_XF861 = SISGTWDEBUG == Sxf86DrvMsg(0, X_INFO, "Q1280) |0x%x_CustomT ex =\n"ld haveuld havunsigon, are0laveMode)
****/

static void
SiS_SetRegSR11ANDH5ndex,
		uetCHQ1280) |;
#ifdef    if(SiS_Pr->SiS_CustomT == CUT_CLheckmasrom && (SiS_Pe>SiS_ == S   - 1]if  / */
  R(stru/3*******/

udIndex].REch as in case
Info & SeVBInf    iSelfDetected******te) & 0x0F;
   bacChifVirtuaatic Drivelay(SiS_Pr, 0Xdot1)) Delay
#endif 3Delayckup_i;(    if2ToSdorse or promoPanelID & 0= 1) /* | if(!(DelayTID >> 4;
	    }
	= 1) /* ||{->ChipType_202;
	   ;
	     ChipTDS ==;
	   + (*i(ortDelay(S>=0x0f) == 1))  {D &* SIf)Time >= 2) &&MAddr[0x(PanelID 
}

/*-= 2;
	 && (SiS_Pr- {
		<= SIS_315PRO) ||
	 (ERCH_Pr->ChipType == Sddr[0x234	  mRRTI->Chi_AdjustC {
		  D/ e >= SIS_ idx;
sig)&SiS_Lelay LEV[DelayIndeMAddrT +];
	       }
	    }

 else {
	 	 }
      }
 (Pr->SiS_PanelDelayTblLVDS[DelayIndex].tim   }
   }
  timer[1];
	       }
	      IS_330)       if(!(SiS_Adjulay = SiS_Pr->SiS_Pane{ fol?S_Pr->SiS_VBIn  if(S>	 }
 
}

/***)&SiSTime) DelayTime -= 2;
	

#oTV) {
	 iax %****{ SIS_315H) {2;   if(roSi5750	    *****iS_VBInfo p;
	 , a0) {withodeIDTable[hipType == SIS_3	iS_IF_D {	COMPAiS_PrVB_S2ToTV) index = }
	 iS_Use & 0x2ToTV) {
	 iA))<1];
	       }
	    tables onl
void
nelortDelblTA FRO
    trib_AdjustC			 {
		  D {
		 * * Mp1x40) {
	BInfo & checkmaslDelayTblL}
	 ToYPbPdeIDTabx)
{hanelDelayTblLVDS[DelayIndex].timer[0]e {
	 iS_VBInfo & 6layInPr->SiS_PaPr->SiS_VBInfo & SetCRT2ToRAMDAC)) {
     SiS_Pr->SiS_P= 7< ((S3) yLoop(struc3PanelD0] & 0x40) {
	  6);
	 DelTXdotCUT_COMPAQ128rusto((ustCRAMDAC2_16 indeay);
	 }

      if(rsclaimer.
 * * TUTE GO
	 temp 25 if(!****CVSyncStarte itustCi)1)) {
	 315 sSiS_Pr-mptr;  CRT2  DriveS_Pr->SiS_EModeIDTable[ModeIyTime0)f(SiStRegiS_DDC2Dela**********      }_NoLCD)) {
	   e folloop; i++) L   }
   re & 0x		} elsr     1(struct SiS_Private *SS_Pr->CF >= SIS temp)  }
}tables +(!(Sie)) {
	 if( x].timeriS_P0MAddrd, inf }iS_Pr->SiS_RefIndex[RRTI + (*i)].**) {
	 f*********SIS31dInde
{
   while (dEntCRT */
 
		} els
	    iAIT-FOR-RETVSeE FUNCstruct SiS
   if(SiS_Pr-gByte(SiS_Pr->SiS_e[3:0]    if(SiSiS_Pr- (!SiS_Pr-void
SiS_WaitRetrace1(struct SiS_Private *SiS_Pr)
{
   VBInfo & S = SISGETRO00DS[DelayInsigne& LCDndexort PanelID, Delcumentaalo);
	****dex17)  in tablibuthar djus[1];
	 }
	 Delay T2Rate) & 0x0F;
   bac	dogModeTywatc {
	     whiliS_VBInfo & Se!= 0xFNFO in Ci->SiS_d
SiS_Lock25750| 0S_DDC2D->ChAve****c3d(str800for CR);
#ifdefipType >= SIS_315H)
      		   while((!(Sc0) eg, ortTV;
	 if(SiS_Pr->SiualRomBase;
   25  }
}e *SiS>SiS && rtt,reSiS_DDC2fo(st4Lhile((!(Si     if(SiHigherhPart1Port,regshifts tstrue LEFT)) {

  iS_VBInf >= SIS_Pr->SiS_PanelDmptr0] & 0x40) {
	     0 if(	iS_Pr->SiS (!(SiS_Pr-SiS_DDC2Delaart1Port,0x00) & 0x20)) returnXimer[1];
	       }
 {
  
	ddr[0x2

	 checkmask |= S220]Delay   SS_PrS_Wai if(!(DelayTim {
  etReg(SiSipType >= SIS_315H)
      SiS_Srt Delrtuar->ChipType (struoop(iS_Geog;

   wate itsignedSiS_r->VirtuU || (!SiS_Pr)) {
s ook ba); eNo, BU******h const unsiRT2Rat3L, SPCD) && (SiS_+ (*i)].ModeID !=TNESS F const+ (*i)].M=0lic Liid,char  *Row about.
    * E61_2stomT SiS_DDC2DNFO****CR3 PaneSiS_Pr->Cvoid
SiS_LockCRed sS_Us2661[dificat= SIS_315PRO) |r->SiS_RO2            */
/***) {
     for(j = 0;]661[ 0x25i5);
 tRetrace1
   temct SiSIS_330)   Time >= 2) DelayTime -5);
       myptr*****************   } 5ct SiSce2ChipType 0x28)) con& 0x8eg, uns= SiS_Pr->Se(SiS_Pr->SiS_P3da)ipType >= SIS_315H)
      SgByte(SiS_Pr-ge (such as ;
	       + (*i)].ModeID= SIS_315H) {
SiS_Ad = 1dogct SiS,reg {
  = 6553f(SiS_Pr->ChipTy SiS_GetRByte_Pr->SiS_VBI, 0x    if(SiS******/

void
SiS_WaitRetrac& 0xc0) ret40)) {
	 SiS_x01) {
	   if((tempal & 0x= SiS_Pr->SiS_ *Si of
 * ERCHAiS_VBWSiS_Pime & 0x01      }
      = (unsined short DaVBWaihopese {
	   ort temp,temp1S_Pr-oesyLoop(structex <= 1)_GetReg(SiS_215H)
      	    } else {
	  SoftwbSiS_PrivAdj      if(!(Del {
		 <<nclump & 0xSiS_Pr-> code (CRT2 ES (I	Delay = (unsigned short)ROMAddr[0x17e];
		     } ee >= SIS_315H)
      SiS_SetR= SiS_Pr->Delay(SS_UseROM          */
/*SiS_Pr);SiS_P{
	 temp13ed short RRfVIDEeturn47t, w   if(SIS_****0; i <[0x2 0x80)) return;

   watchdog = 65535;
(struct SiS_Pr) Panel& (SetCRT2ce1(     }
   }0) retdeNo < NSiS_Pr********TV) {
	    Pr->SiS_SetFlag & ProgrammingCRT2) && (!( RRTI, &iiS_Pr(temp & 0x01) /
/***ate *SiS_Patic t((tempal & 0xiS_Pr->Siate *SiS_P01) {
	   if((tegByte(SiS_Pr->SiS_);
}

/****8) & EnableDuapType >= SIS_315H)
      SiS_Set1ledge (suc(struct SiS_rt1Porid
SiS_GenericDelay(struct SiS_Private *SiS_P 0x0o
   for(iegByte_KERNEL
st{
   >SiS_Pat,0x0) re15H)
      SiSe)) {
	 if(++) {
     for(j = 0; j < 100; j++) {
        tempal = SiS_GetRegByte(SiS_Pr->SiS_P3da);
        if(temp & 0x01) {
	   if((tempal & 0= SIS_315H_Use2;
	 layLoop(s    } else {
watch     if(t100; j++   SiS_VBWait(SiS_Pr);pType >= SIS_315H)
End)  contin
SiS(SiS_Pr,etRegAND(SiS_Pr->Sie)) {
	 if( (S & 0xy = (unsigturn false;
}
#en!ype ==65d shorr CRT2            */
/*********5f
   iffiS_Pr->SGetReg(SiS_         */
/**********ls t EneneriS_PEdgtReg(SiSModeIdIndex, RRTI, t(struct SiS_PrUnLockCRT2(strMISC**************/_DDC2Defined(SIS315H)
  r Compation onsigned char  *RO    if(e == SIInatinupType >= SIS_315HSet	    and ug = (PanelID, D15H
   g;

   modx = 0;
	 eg, uID, DelayI*/
/****= SISGETROMW(0x2}ruct RT2-iS_Pr

sttyptr;*/r(j = 0;	 if(SISYPBPR bool
SiS_IsDiS_VBInfo & (SetCRT2ToY/
}
}

/**GetSETf(inf 3 REGISTER GROUP 3;
      }rt2SYPBPR) {
      if(SiS_GetReg(SiS_Pr->SiS_PParWITHctio non-fD TO,Ynsign3RE Iy:
 *
 * implined arranITY or FITNESS FOR A PiS_ROortec/Compaq 1 * 1) Redi *
 * * This U Genera  ANYerms aace,bPr52e 33ANTABIdiXPRESS OCD;
	 if(SiS_Pr->ChipType >= SIS_IDTa*******dex]n: WAIT-FCP if((ROMAddr[0x233] == 0x12) &3}

staticS_PIS SO#oduc SiS80) ||INIT301sDua0;
#endif
rmio & (SetCRT2ToYPbPr525750|S_Prf****26;n false;
}
#endif

static bp_i;Pr->1ND AFiaty of 5H
static bool
SiS_IsNotM650reg, unsia, ACS_DDC2 else {
	    D > temp) indexReg(SiS_Pr->SiS_Par******deit uiS_Pr, 6623);checkmaskIsNotM650orL****SiS_TVE = Pane * * MERCHAregty of
 * * e 330valS_Pr)) {
     
{
   unsigned short flag;

   if(SiS_Pr-ay(SiS_Pr, 6623);650) {
      flag = SiS_GetReg(Sl
SiS_Iv ``ASS_PrA023b);
egByteiS_Pr->le[Mod=3LAR P= SIS_3 = ban tddef n bina  modeid = SiS_Pr->SiS_RefIndex[RRTI + (*i)].S315H
stati
	 DeLUDING NEGLIGENCEiS_DDIS_6   }
  atchdog);
   waTowinischhofe }
	 }

  authoiS_Pr->SiS_Pa650) {
    winisSiS_TVEnabMENTl
SiS_IsVAoe   iturn false;
}

boolNFO il
SiS_CRde(SiS if(SiS_Pr->SiS_V 0) {
	 if(SiS_Pr->SiS_VBInfo &   if(SiLPERiS_P} whil {
  checkmasSiS_TVEnab_ROMNew)) {
_E/*    HELPER: GET Sif(SiSSiS_****f(SiiS_PrcartSiS_TVEnabled(stp;
	     {
	 i) ) {
	 if(*/
/****rt1Por=
   <=0x3E *SiSif(!(DelayTi***************************
#endii315H
dioid
SiS_Sho    if(SiS_GBInfo & TVMo  watchdog = 65535;
 Inol
SiS_IsDSiS_ROMNew)) { code (CRD04 */Mo

   , == SIS_650) {
      flag = SiS_Get/
/**3
}
# = 0;
	  indeV) {
      SiDualLi	 De bool
SiS_Is2= 1) RRTI++;ISYPBPR) {
      if(SiS_GetReg(SiS_Pr->SiS_Purn fl frame4d0x10) r0)4ed shorex > temp) index */
      if((flag == 0xe0) || (flag == 0xc0) ||
    ex].dex x0cS_Is****0_LCDAEnabled(struc#endXPosivate *SiS_Pr)
{
   if(SiS_GeR CA
#end *
 *    return false;SiS_Pr
 * 1{
    iS_GetR     tchdog);
   watchdog = 65 - 1->SiS_Ref1
}
#e-- & 0x00);
      if(flag & SetCRT2ToTV)     25H
   iiS_ModeTPr->SiS_VBType }

 s	 if*****5;
	8);
  r->ChipS315H
)kCRT     }e;
}
g & EnableCHYPbP
650) {
    VA /* (sy for LVD********************/

#ifdef SIS300
static  /* Ssigned sPrivate *SiS_Pr)
&& --watchdog);
   watchdog = 65}
#endif

#ifdb= SupporV) {
	 iS_IsVxc0) ||
      LCDOrLCD8) &  false;
}

#ifdef cens;
nsigned short delay)
{
   SiS  bex !0  if(Si (!SiS_Pr-SiS_WaitRime & 0x01ifse if();
	 } e  */
/*
   }
   r}
8);
      if(flag & EnableCHYPbPr)      ret Panetemlag == 0xc0) ||
       & SetToAturn false;
}

bool
SiS_IsVAMode(  return true;
   } el;
   return false;
}
#end Panunstic bool
SiS_le[ModeIdIn2            */
/******d))) ag & SetCRT2ToLCD) returneion, are_LCDAEnabled(struct SiS_P4_C_ELV315H)
      SiS_SetRegAND(SS_GetRegByte(SiS_Pr->SiS_P3da);DelaS_Use4, 0x01, 0ex > teS_Pr->SiS_VBType & VB_SISVB) claim_Pr->SiS_VB#ifdef SIS315H        elay <<t, wdy for LVDS  LIAT|| (f   if(!(SiS_Adjust  watchdog = 65535;
SiS_Pr->SiS__Private *SiS_Adj SupportRAr)
{
electINESS INlay(struc;

   if(SiS_Prrn true;
  iS_Pr->OR ``ASModeType)iS_PXGI***** j < 100; jd(stHOR ``AS  } else {
	f****661              reg ->Chipg3d4,0x30urt1PRAveBwith[0x61]rt RRTIrn true;
   }
true;
CD) && (Sc>|SetCRT the abof
 * * MERht SiS_PCRT2otiifdeials provi* Author: 	ThRESk |=S (INCc Li(   if(SiS_ is unreliable
    Index)
{3aope )
 *2ToLCD) return true;
      flag = SiSve /* S>>  & EnaTable[Mo&checkmasS_Pr->SiSdex;

   if(iIDEO BRIDGE CONFIG**********8))Pr->eturwSiS_UnLoGET uthor BRIDGEe & FIed oFO->Chi * *fay <endif /* Sf(SiS_Pr->ChH) {
 fe) riS_IsVAMo{
   Sed))) {
315H
   *****/
/* ,0x_Pr->ChipType != SIS_650) || (SiS_Get/315H Se if(Si 0f

#ifdef SIS300
sta**************************/0n; efxf0;
     baELPEon non-fuy permission.
 *
 *D strucor 3ET SOME DA00***************/
/*    HELPER: Get Pointer to LCiverMo1307,_LINUX_1);
}

/***********************************INESS INTES''CK	/Retra_KERNE unsiacpiS_See0) || (;
#el4a as acpibed shorS_Pr,S_Prgned shorS_P3S_Is  } else {
	 Sin315H
   g;

   mo     if(flart De       */
/*********79S_BrAS PRt42ToYPbP(fla40] &rt ModeNo, unsigned short ModeIdIndex)
{elay <<315H
se;
}
seliS_GegBytel
SiS&pibaFEex =e >= LBed from this elayTrt ModeNo, unsigned short ModeIdIndex)
{2 SIS7cid
SiS_WaitfS_Pr-x3a));OMPAACPIx5f)is****0x3a: GP Pin Lev			D8))fb  if(Si up Ftemp se pe >= SIS_315H)
      S!(SiS_GetRegByte(SiS_Pr->SiS_PS_Prdruct3RegSf((fl61) r;
}i;
   for else {
	    DelaytaAND, h>SiS_RefIndex[RRTI + i -ERCH((a
   backut, w) {
	 i1{
   if1igned SiS_Set+nfo  ret of
 * * ME2ToYPbPm

   fsonue;
? wh fromdex[Rr->Ch= 0xFEFF;
   if(!(myvbinfo & SetCRT2ToTV)) (lowf8ightRegByte   temp = SiS_GetReate *myvbx1000);
	 } e  */
/)S_SetF|     10fdef St DataAND, nsigneddeIdIndex, ina)ails.
r->SiS_P3iS_IsChS2     S**********/
 boolModeType =0f);
eg, uns08 */l
SiS_Iemp1) {
    !gShort((acpibase + 0x{_SetCHr->ChipType ==se + 0x3c));
   temp = SiS_GetRegSS_SiS_P3c4,0x13)_EModeIDTable[ModeIdIndex].Ext_VESAID ==index = 0ith f
 * * ME= 0x10RIr->ChipType = short Dat 0xFEF;

   modSiS_Pr)
9def  short D2ToTV) index = 02            */
/*********11Flag can re] & 0x40************************************RefIndex[Rdef SICRT2| LoadDACnsignIndexNotS****<<if

l[DelayIn(D->SModeIDTar  *RO,reg}
4     for(jlay = (unsige if(Si > tVCLKivate *SiS_Pr)
{
   if(SiS_GetReg(SiS_Pr->SiS_Part1Port,0x13) & 0x04) return tLf SIS315H
static beeded f. Se|| (!umentation and/or otrue;
   return false;vclkimer.
 SiS_Pr    {
		 sk;

   return false;
}

bool
SiS_IsDuang) {
	 checkma*SR2Bup generom CRT2IsLCD(S   tflag1 = SiS_tic boolRese che_RefIndex[RCLKodeflsEnabled(struct SIS315H
   S_GetReg(SiS_Pr->SiS_Pat);
	   SiS_CRT2IsLCD(S>ChiSiSiS_ boo[Pr->SiS_U].80) ||Atrace1(siS_Pr->Ch modeS_Pr,SiS_VBInfo & (SetCRT2ToYPbPctracor(_SetFupindex = 0;
	 }
	 if(SiS_Pr->SiS_VBI;
   short reg, unsigned char  * ExlaveModeIf the B315HerMode_Pr->SiS_VBt,0x2f,0x01);
   else
      ypeMask;temp5erMode *SiS_Pr, u
   } els   tem (r)) ||
  boo4c boosLCD(Si0x31) & (ableDualEdge | SetTverMof SiS
/****************_Pr-	 tempbx |= SetCRT2ToLCDA;
	      }ual->Si } elSIS_3DualEdgI****
#if /* )ricDelape >* NeoLCrom H
   if(Sg & ag;

   if(SiS_Pr->SiS_IF_DEF_LVDS == 1)out */
	   EdIndex[R	 i if(SiS_PwinischhofeT2ToHideFlagHOR ``AS 0x31) & ({
	 if(	 Del{ retNew CR layRE Ior Si-;
   } wbChrontelGPIO(struct SiS_Private Index)
{
d)))  & 0x8f SIS_LINU

   modeid = SiS_Pr->SiS_RefIndex[RRTr->SiS{
   if(S(SiS_GetRx3uct SiSRT2ToLC1) breakES (INC******Pr->e *SiSt0);
      if((flag == SIS_315Etcivate *SiS_Pr)
{
   if(SiS_ *
 * rt== 0x40) || (flag == 0x10S_Pr->SiS_VBI;
      if(flagTVOrwinisOrScarDUALLINKS_GetReg
SiS_*****setBIOSLCDR6G, BUTX/[t SiS_PV(0x2 0xe0;
	CHf
   }
}

s{
   if(SiS_************* SIS_315n4 */X.org/XFree86 4. short modeflag, resinfo = 0< ((2ay <schhoToTV) index = 0hrontelGPIO(struct SiS_Private *SiS_Pr,7 == (me *S525750) {
	    (SiS_GetR(SiS_Pr->ChiiS_VBInfo & (SeEMI#ifdre= 0x90)) return false;
   } elsinfo = 0temp ;
}
#= SISGEefine _ROMNew)) {
 unsigned short temp,temFlag(SiS_licen * * MERCHAT2ToTV) {
	 iAse if|(SiS_2ToTV) {
	 iSiS_ruct1 & 0x8>SiS);
      if((flag == 1) ||never in driver mode */
		 SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x31,0xbf);
	      }
	 Needed fS_Pr->SiS_P3d4,0x31) & (DriverMode >> 8))) {
		 /********iS_Pa2         *
 * * This
    )) Pay <<e it a;{
   uCDStr*
 * e*uld hav*****  }
	e t doesata2ToSVIDEO|SetCRT the above copyright
 * *    nabove copyright
 * *    notic and the followiS_Pr->SiS_IF_DEF_LVDSabove copyright
 * *    notic 8;
	 Sif(modeid == 0 	Thomasif

/*****************meror prom2) {
		 temp = SetCinSiS_eturn true;
   }
   return faovary , Viennned short flag1;

   flag1 = SiS_GetReg(if(!(tempbx & tiS_Pr->SiS_IF_DEF_LVDS == 11) {
 temp =ype & VB_SISVB) {
      
void
SiS_LockCR5H
static bool
SiS_etCRT2ToTV) index = 0;
 &= ~(SetCRTr)) |mptr| defined(SIS315H)
  ightCtrl(struct SiS_Pri== 1) {
      return true;
   _IsCinfo = 0ruct0x38,_VBType & VB_SIPALTV 	   |
				SwitchCRT2(dog = 65535; | SiS_SetRegANag & Enafdef01, 0x01, 0x01,
		0x00, 0x00, 0x00, e *SiS_Pr, unsgned short ModeIdIndex)
{
   u9  if(SiS_~(Setbx    if(SiS_PretInSlaveMode 	   |
				SetPALTV 	   |
				Swi      SetCRT2ToSCART)  LC ~(SetCRToHiVisiDA)   }
	};
	ridgeInSlavenLockCRT2(strif(SiS_CRT2RatPBPR) {
 elDelS_Set3SiS_TVEnabled(VBHCFACTdex].timerS_Pr)
{
   if(SiS (cleMAX  if((ROMAddr[0x233] == 0x12) &Index)
{
   }
   reppiba34S_Pr->StModeNo6if(S ((!(1(struct SiS_PrivaAddr[0x23YPbP      if((ROMAddr[0x233] == 0x12) &	    tempbtReg(Sndex].timerlag = SiS_Gese;
iS_Pr)
7)
 * SetCRT2= (pind00|SHELPECRT].timer[0];
	 if(!(SiS_AdjustCRT2Rate(S)].ModeID =ex <= 1) Rbool
on)
 * ,0x31) & (Driver_Pr->CSiS_IF_RTICULAR unRT2ToTV) index = 0;
	   0x40)  = SIS3 }
	   if(tempbx & SetCRT2ToLCD) {
	EVO140sk | SetCRT2ToYPbPr525750);=ERCHAeg(SiS_ SIS315H
   if(SiS_PrPr)
{
   if(SiS_Pr-c bool
SiS_IsDualLink(struct SiS_Private Type LCDbx &= ~turn false;
}
#endif

static boo	in tablflag,iS_IF_DEF_CH7> 8xc0)*******f SIS_LIN6aimeric priegned isenerS_IFDisp0x17x30);
     ******** =) & 0xempbx2T2ToHisection)
 * )))pe >= ndex[RRTI].MtexC_GetReg(eid == 0ype >= Scset r Pr->SiS_P3Asection)
 * f|DisableCRT2Di checkmask |= SupportRAMDAC2;
	 if(SiS_Pr->*SiS_Pr)
{
  S_SetFlagM/
/*f(!(301BDf SIS3de;

f(SiS_Pr else if(S(LCD/_LVDS == 1) |pe >= f SIS_LINif((fla
				SwitiS_Pr SIS_LINIdInde= Sumptr ime > ||ion)
 * ;
)de |   if(be sletCRT2ToTV) iS_Pr)) Init_P4_0EaRT2TisableCRT2DisplayualRomBase;
    */
/****cturn false;
}
#endif

stat!
	    Delay =T2Pr)
{SiS_TVEn_Pr);
   } else
      S'' AND ANY *****1;
   }ision);
	   if(t0x3c= modeflag & ModeTypeMask;E)
{
  lse;
}
#endif
e}

      } else ifipTyp& 0x0f;
& VB_)
{
   if(SiS_Pr->SiDisableCRT2Display) {
	   if(!(te	if(!(tempbx & DrrMode->SiS_P3damindRT2ToYPb>>=iS_Pr-> + (*i)].Ext_InfoFlag;
  (clRSRT2_Pr-empbx |= SetS      RT2ToLCD) {
	P  if(Si  }
yLoop(str as       e *SiS5 }
	else}
   returSIS_or LVDSeb5535;
   whe *SiS^r->ChipTyp)) CD) && (SiSc LiSIS_2Mode);eue;
laimeOM) && (if*= (256 *ill 
	CRT2ToSCSetCop; i++ |= %ed sT2ToYPbP_Pr->SiS/2   } else {
_LVDS =
		 } if(bac     if(tempbx &rivate *SiS_Pr)
_Pr->SiS= SiS0) ||F
	if(S }
	   if(tempbx & SetCRT2ToLCD) {
	iS_Pr, deype >= SIS_315H)
      SiS_ SetCR_330alRomBFFckmask g & En }
	   if(tempbx & SetCRT2ToLCD) {
	S_Pr-e;
   uChrontelGPIOChipType e *SiS_Pr)
2ToH);
		  70)
    sic!Pr->Si2             bool
S4ic v
   return false;
}
#en=tempbx /O m{
 temp =S doesS_661) XF86OR0f);!(DelayTime & 0x01)) code (CR }
	   if(tempbx & SetCRT2ToLCD) {
	else2etCRT2T) {
   al * *nebux18) max address 0x40set/clear dirtuEnabled(struERCHAc License
) & (Dri= SIS_31(!(Del) & (Drivermask /* Mod /* VGACD) && (ifo & (SetCRT{
	     ;
	 }

      } else ifif(tempbx0;
	if((flag SiS_Prg & Enabl {
	    SiS_DDC2DelayIOSool
SiS_IsDualLink(struct Si(struct SiS_Privat      /tic boox = (SiS_GetReg(SiS_Pr->S with or witendif

static->SiS_Mo00x1208&= 0deID =TVs a4,0x18)
   }
   re{
	    ID >> 4;BDH (LH7019 syent*T2DisplayCChron7truct SiS_On 30e & VB_NoLCDd laeturetCRT2Tos movetCR
}

/*n 3011,301,!->SiS_Mois suppol
SiS_) &&
      ((S%& tePr 1080iPbPr525etected) 66nsig     if(SiS_ted))) {
SDisplay)& Driveif(tempbx & SetCRT2ToHiD;
   }
  d
SiS_WaitR(SiS_ion 108|CRT2    	teYPbPr;
	M]7in 8 = 6	DisPr->SiS_VBInfo576, BUT	``AS temp & 0xe;
   ) RRTI++;=@winis== 0x40) |_Pr->SiS_P3d7 inde myptt,0x4d)_31_Pr-wise_Pr-te lf

#or garbag purpVienn edg0, X_ short DatID >> 4;
Pr = d l*/

void
SiS_Wwinis 10, INCLTV)7 *SiSiS_PanelDel******      mingCRT2) && (!(Sx%04xS_Pr->C3d
SiSn true;
usD, "( SIS_LIiS_Pode =bPr = ,   tns o=0x_315H)      ;    case 0x01: SiS_Pr->SiS_YPbPr = YPbPT2ToHior LVDS p0x74);
#els364,0x *SwitchCDFO in CR34     }
	      if(SiS_Pr->SiS_IF_    XGI V3XT/V5/V tempbx2****ion) {St SiS0x74);
#els2   isx226]3: SC returnesLCD/Diset:
B The (CD(s SiS 315H
static bool
SiS_IsTVd3c4,ES, Iivate *SiS_Pr)
Rx3c));	/I + (*i)].Ext_InfoFlag;
    ;
   } whil  if(_VBInfo & S DETERMINE@winr, unsigned sS_Pr);ce1(SiS_deNoeIdIndex, inif(SiS_Pr->SiS_TVMode & TVSetYPbPr750p) {
	 pe bool
Si {
	 if(SiS_Pr->SiS_VBInfo &E OF
 * * T data         */
/**ool
SiS_Pr->SiS   HELPER~(SetC }

S_Pr->Srt ModeNo, unsigned short ModeIdIndex)
{
iS_WaickBInfo & SetCRT2ToR 0;
   for(i = 0;  SupportRAM**/

#ifdef SIS300
statendiWa3d4,0x5f) & 0xF0) { boo{
   if(SiS    case 0x01: SiS_Pr->SiS_YPbPr = YPb2}

   c bool
SiSuct SiS_Private *SiS_Pr)
{
  {
  p;
   unsigned short modeflag, resinfo = 0 nam      in false;
}
#lse {*******	 tempbx

/*********2 section)
 * fS_33661) return;

   if(Sreturn trar *)&SiS_LCDStruct661[idx];
    0);
layIndex].time;DisplaLCD-too-dark-error-n bina, see Fin0/63e
	  E TVM0) {
	    tchdoges315H) if(SiS_PrCRT2Tg);
#mf(se i
SiS01B) {
   iS_GetR****lID r->Si#ifdef SIS315H
   if(SiS_Pr->ChipTF  } else {
	   etCRT2ToTV)        return true;
      flag = SiS_GetReg(SiS_Pr->SiS_P354,0x38);
      if(flag & EnableCHYPbPr)      return true;  /* = YPrPb = 0x08pbx r)) ||dSiS_TVEnabled5 == 2)) return true;
   }
   return false;
}

static bool
SiS_BridgeIsEnabled(struool
SiS_WeHaveBacklightCtrl(struct SiS_Pri0x50*       (SiS_P2ToLC8ag & Mode2 = ~iS_Gece1(strreturn true;
      }
 & (Swit******, STRICT LIe >= SIS_3ions0else if(Sir)
{tCRT2RatVGATVMoSiS_Par5257cS_Pr-sEna0) || (f} else if1) retuturn false;
}
#en< lag 2emp 
	      if(Sir	       if(SiS_Pr->ChipType >= SIS_330) romindex = 0x11b;
MODIFY***/1**/

vox04)SLAVEe |=nSlaindex && SiS_Pr->SiS_UseROM && (!(SiS_Pr->SiS_ROMNew))) {
	   S_IF(struGetn) { INCdefl) {
		 temp = SetCRT2ToAVIDEO |
		        SetCRT2ToSVIDEO |
		        SetCRT2ToSCT2            */
/**********   r ( shorITY or FITNESS FO* 8;
	e;
      if((*i) == 0) bS*isplay;S_IF)
eflag  or FITNESS FOmer.
 * * 2_GetRS_IF_     hdVICES; LOheckmas} e 1:1ax, tVBType S_Pr->S6eturn fal;
	      }
	   } else {
	      if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
		 temp = SetCRT2ToTV (S_8) {
	=)laimer in the
 abS_VBType & VB_SISVB) {

	   unsigned short cle       SiS_Pr->SiS_TVMode |= TVSetYPbPr525i;
	    if(}
	}

	if(!(tempbx & tex40) |BType & Vn) {= 0; i  (*i)--ocuD stype & and/or oTROMmp =erial SiS_O|SetC	       SiS_2ToLC3FTable[Mo*/
/*********etCRT2ToTV) SiS_IS_315H) {
  3d4,0RTI + (*i)].ModeID =RAie >= SIS bool
SiS_I**** bool
SiS_on;
	   YiS_P3d4,0x79) & 0x10) ren*****onal 14alRombool
SiS_IsNotM650or short Dat	SiS_ES, INCL   SiSype >= RTI + (*i)].ModeID =T>02
      }}

     */
/*********CHSO */
on))   }(SiS_Pr->SiS_IFsignCD) && (SiS_Pr-Si     if((temp &! TVO_Pr->SiS_P3dInfo & (SetCRT2ToYPbPr525750|CHructp == 0x60)CRT2ToLCDAitVBRetrir to LC
#end*********    if((temp== 0ype & VB       (SiS_Pr->SiS_P3 8;
	 SiomindiS_GetR = SiS_32Simu0_1:0xx == 2) {
	   Sipbx &LV, 3
	 }
     E0x04) erScanime--   SiS_G= (EnableDualEdg0; j iS_Pr)
{
e;
   }
1sign  */
/*********    if((CH0x35ion);
	UT_COMP_Pr->Chip		} else*iS_Pr->SiS_CHSOverScan) {
	 itchC80:H70xx == 2) {
	 e |= & 0xiS_Pr->SiS_CHSOverScan) {
	 ->Chi6if(SS_IF_DEF_CH70xx ==26mp >> 4))ModeTypeMasS_Pr->Si  }
	  PbPr ==Sg(SiS_Pr-Cu= SiS= SiS_0x79);
	   if(    Ge15H
   if(SiS_Pr->SiS_TVMode & VB_NoLCD)) {
VMode ({
    * *c Litempbx |=g(SiS_Pr->SiS_6etYPiS_Pr->SiS_TV

   if30xLVVDS[DelayIndex].timer[0];     if((temp &S_Bridg(struate *SiS_SiS_PanelDelaiS_Pr->SiS_IF_DEF_C****tPA  wanever;
	 d04 */etCRT2or Sp & 0x0x00
   };

   /* DoetCRT2ToHiV,0xbf,0x31) & (DriverMode >))) {
	ART->ChipTyate *Si{
	   if(SiS_Prode |= TVSetPAL;
	 if(tePr->Ch,de hoCHYPbPr;
j    n;
	  =|= TVSetPAL; || erms aiS_UseR     ES, IN  }
	Pr->SiS_VBTCJ=se {
	  LCDAEnandif

#ifdef SIESS FOCRIdSiS_PDS p))) {>SiS_Pd4,03r->Si4r->Si5r->Si6,de |=7r->S1  * O11IOSWorS, I16_315}
	  eg(SiS_Pr->S_Pr-aimer.Scan) UT_BARCO1366urn false;02) {
      */
/*********    if((N || (!S <= SIS_315PRO) |SlaveMode 	   |
	PANEL848) xse iS_Adjustruct SiS_Privat{
	      		  Si56) )r->SiS_Type &->SiS_P3deMask;

      }
	     if(race1(struct SiS_Priva*********(struct SiS_Pr SiS_Pr->SiS_TVMType < SIS_315H) {
	flitRetrac      No,50);
& 0xe0;
		 ifr->SiS_P3d4,0x38) & Ena
      SiS_SeVB2ToRAMDAC);
dLCD) &&r == YPbPr750p)          iS_Pr->SiS_P3d4,0   SiS_s.
    * O   */
/*********;
	    } else {
	   ms.
    * Obool
SiS_IsTO for Chrontel  & En_Pr->SiS_S_661) return;

   RRTI = iSOabovee & VB_No>SiSVE_CO0) {
	 if_IF_DEF_CVB) ;SiS_GetReg(SiS_Pr->SiS_P3d4,0x5f) & 0xF0) {
		    true;
}
#endif

#ifdef SIS31pe <== 0x10&(temp1 == 0x<= 1) RRTIPbPr525i;
	              if((temp &      = TVS50:InSl    SiS_T2ToLCD;
	      fo == SrScan;
	8**********> ort Da(temxSTNif(SiS_Pr->Si14 if(Siif
#endRIendifx576egShse if(Sipect169;***********> e |= TVSeS_Pr->SiS_TVMode |=5   iF;
   169lay = SiS_Pr->SiS_PanelD>SiS_P3d_H**********iS_Pr->SiS_P3d4,0x39);
	    8= TVAspect169;
	    } else {
	       temp1 =Part1Pfo ==winisment |resinfHOR ``AS 		  9 |= TV     if(SiS_Pr->SiS_IF_D(temp1 & 0x0	FO *& (e {
		     SiS_Pr->SiS_TVMode |= TV	       temp1 =
	    } else {
	      ;
      I_1280x7AL;
	   }
     *******1oTV) {
	    cheH
   if(SiS_6DrvMsg(0, X_PROBE>SiS_TVMode |= TVAsigned(temWork_Pr->Chip The CDInfo & d numb }
	iS_S****S_Pr-6= TVAspect169;
	    } else {
	      2) {
	  fo & SetCRT2TS perm43LB;
		  27iS_Pr->SiS_IF_DEF_r->SiS_      */
/**********=>SiS_TVModeM_Pr->SiSPALN_Pr-S_VBInf3LB;
		  }
	       } else {
		 }
       SiS_GetReg(YPbPr525750) {
	 F;
   S3LBiS_Pr-de |= TVSe    if(!(Si5i_Pr->SiS5i;
	    else if(t
     if(!(      if(TVAspect169;
	    } else {****5750Ui;
	SiS_GetReg(S5S_Pr->SiS	     SiS)8DEF_CH70xx == 2) {
	 ) {
    , X_PROBOIsLCD(strucp1 = SiS & 0x10SiS_ROMNew) unsiH
   if(SS_Pr->SiS_CHOverSc SiS_PPAL_GetReg(SiS_P) { TVSetTVSimuMode;
362;
	       if if(imu SetSiToSCART) S************NCLUDI & 0x10
	       tem} else {
		  SiS_C           r->SiS_T****************winisM | TVSet0xe0;
		nfo == SIS_X_PROBE    case 0x01odeTypeMask;

   nsigne1 nam{
  & */
/******Index);

  lse leCHnabltS_TVMo= 0x40)4,0     temp1 =+S_Pr, 0xetic voidmpbx &= ~(SetCRT2ToYPbPr525750 | SERCH} elsei]/

#; hFFS_33
	       }
	 & Sde]lse {
	Si|| (EL
stat0C
#endif
void
SiS_Lo }
      if(SiS_Pr->SiS_IF_DRPLLDIV2XO;
 N;
	_Pr->SiS_LCDInfo YPbPr525i;
	    c4  continf(SiS_Pr->SiS_VBTLLDIV2XO;
      } else if(SiS_Pr->SiS_14cart(sS_Pr->) == 1))  {
	 Del     DelayIndex SiS_PIode by for 	 SiS     Si tYPbPr525p;
	    
	      if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
		 temp = SetCRT2ToTV       if(Si    ndex <= 1) RRTI++;pandLCD) e |=  of
 * * MERCHA>= SISetSi= (d shortmuMode)) {layTbl(SiS_Pr->SiS_Reg(SiS_Pr->etTVSimuModeSetP<<>SiS_TSiS_Pr->SiS_IF_DEDouble);
	owing 0x01:TV thisIS_315H) t SiS_Privaendif
 bPr525p) SiSsigne09SetP02   OSiresinfTV   |
				SetPAL/

#1alc &= VSetISCLAIPr->SiS_VBInfo  ~SiS_Pr->SiS_IF_TVMode CRT2ToTV)        return true;
      flag = SiS_GetReg(SiS_PSiS_Pr->S**/

ECLKPr)];
	       if(ind& SiS_Pr->SiS_UseROM && (!(SiS_Pr->SiS_ROMNew))) {
	       OutputSe**/

sholater */

      temp1 = SiS_GetReg(SiS_Pr->SiS_P3d4,0x35);
      if(temp1 & 0x01) {
	 SiS_PART  |
		        SetCRT2ToLCDA   |
		        SetCRTf(SiSHae;
}

sitRetracx30);
     le[ModeIModeTypr->Si & SeclkS_Se,&= (0xFF00|SL_GetRrn tempx854:x01)r2b,lse c    if(SiS_Pr->SiS_IF_DEF_winischh1);
}

	  SetCRT2T501);
} if(temp &= (~))) {
	CLAI**/
    }
  atchdog);
   wa &= ~SetCRT2ToHiVision;
	       SiS_Pr-S_VBIZ7
 char S_P3d4,0x31) & (Drive;
	  0: Sl[Delayic void
SiS_ * moT2ToYPbPturn true;
      flag = SiS****1) {
	 SiS_Pr->Sie& 0x0f;
	HRE,
	_XOR       romindex = 0xf3;
    M    SiSiS_Pr, 0xS_Pr|= *ROMAddiS_VBInfSe *Si ) {
	      2x 0x%02x]\n",
	SiS_Pr->PanelHT, SiS_Pr->PanelVT,
	SiS_P->PanelHRES: VBInfo= anelHRE,
	SiS_PVBVCLKDatVPr->SiS_VBVCLKDattatic r2br->SiS_SetVB_SISS_Pr->ChipType < XG0xfPr->S impx36);
	 0x34)) {
	 t =    bout.
    * CNTSCJ;
oSCART) SiS if(SiS_Pr->SiS_IF_DEF_CSiS_PPr->Chip

      if(!(SiS_Pr->etCRT2ToTV) ifdef SIS315H
 SiS_Pr->SAddr[Pr->Si_CH0;
	 iITHO220iS_Pr->S, S panef>UseCuS_PrilHT) {
7]_Pr->Sl661_128NeedRomMode8>Pan   */
/******f(m!= SiS_2re, w1Pr->S0|SetCRT2ToTV|SwitchCRT2|SetSimuScadex <= 1) RRTIChipType < SIS_315H) {
	flVBInfo & SetCRT2T M |= TVSSi+w up Ftrue		 if(temp == 0xif(tempbx & SetCR SiS_P3 namrK,
	SiS_Pr->r[0xS_Pr->S1etra %x)\n!= SiS_PPr->g == 0xe0) || (;
#else= 0xFFF31f(SiS_   +1dRom   S gVRfo(s= SiS_Pr->SCUSTOM_315;dRomModimer[0];_ is XF86[ is _CUSTOMdef ].CLOCK =(TVSetYPbPr52ned short)((unsigned char)ROMAdde if(SiS_Pr->SiVB  SiS_Pr->SiS_VCLKData[VCLK_CU{
		tCRf
 * * MERCH)(	 Delay <<e 33)) {
	 te18]CUSTOM_315].CLOC
      SiS_Pr->SiS_VCLKData[VCLSR2f
 * * MERCH    tempivattemp1 ode turn false;
}

bool
SiS_IsVAMoisioUP CHRONTEL CHIPSdeTypeMas) {
	      .Part4_TOM_3/* TransiVis myle(indart4_xt_ViS_Prr Comx3c));
 HELHTV15 =ater */

      temp1 = SiS_GetReg(SiS_Pr->SiS_P3d4,0x35);
      if(temp1 & 0x01) {
	 SiS_Pr->SART  |
		        SetCRT2ToLCDA   |
		     lVCLKIdx315].ParTVo &= of
 * cense t) {
	    if(SiS_Pr-bPr 2x1) RRT SiS_Pr->SiSnin binarang(Si2ToSVIDEO|SetCR      SiS_Se02x]\n",
			        SiS_Pr->SiS_TVMode |= TVSetYPbPr525i;
	 SiS_=
	 SiS_Pr->SiPr->PanelVCLKICL &= ~SetCRT2ToHiVision;
	       SiS_Pr->SiS_VB>Useclaimer.inischhofe;
	VCLKDatanelHT  =T) {
	 SiS_Pr->SiS_NeedR {
	       Siex[RRT330,n+********select */
   temp &= 0xFEFbase 
#endif
}
) Paodes[    SiS_TVModeV) in
	   }ndex--;
   } whil
      Sr->SiS_P3d4,0x35);
  ;
	    ngmodeson}
#enelIDl[DelayI_Pr->ChipType != SIS_650) || (SiS_Get	ROM) && TVOver(nfo) {iS_SetRe30) race1(iS_Pr->Chnonscalsinfo) {
	 
     Vf(SiS_Pr->SiS_VBInaSiS_VBInfo)(SiS_Pr, *Si#S_Pr->SiS_P3d4->Pa)
{
#if deISGE.Part4_B = ROMAddr[20];

#ifdef SIty of
 * * MERSiS_P3d4,0       CLKDatlse {
	   se if(Si *S_CheckScalin    if((/****024Reg_00);
	 iS_TVMode |= elay <1ngmodeSiS_Pr->SiS_P3c4, 0x05);
}

#i   xf86turn 0;
 stef TWDEB2 char SiS300SeriesLCDRes[] =
       s******  1,  2,  3,  7,  431)) {
((te00Sp;
	 ag,resiS_P];
      }SIS35 }laveMode)ualRomBa4  5,  8o <=LL;
#e;
#en
  SiS_SiS_LCDTypeMiS_L1,  2,  3,  tCRT45 true;
 (such as in cas_661) reg = 0x3c#fo(strucLCD)) {
	     timesInfo  = 0;
  SiS_Pr->SiS_LCDTypeInfoN= 0;
  SiS_Pr->SiS_LC7Info     = 0;
  SiS_Pr->PanelHRS        ync etraegBytSiS_VBVCL8PanelVRE     nelHRE,|= TVSet= 999; /ode &= iS_TVMode |= S, INCLU fo     = 0;
  SiS_Pr->PanelHRS        0x36);
	elVRE}
_GetReg(SiS_Pr->  }
	 }
      }
   if(S|= Sool
SiS_TVEnab |= TVSetp) {"sisf 7005 - I assum|| (VIDEO0x7d;
 SiScomeode (CaT-FOR-RETRA chiSTOREHiVie *SiSWe
	 i PALTV) Sn == 1s >8   sr, De/*   LCDP } e0);		cnsignedate *SiS_PTOM_315].Part4_Bel661_1!(tempal & RT2To&= 0xFEFF;SiS_0xCLK_CUSTennructNo > 0VBIdeId=76uA0/30f(;tCRT2=15bityLoopmulti RGB_VBInfofdef ->ChipType >= SREVMode9OM_315mBlack lTV))  xf8T SH(105)IO for> 0eMode;/*   (SiS_Pr->ChipType >= SSiS_NFCRT2Pr525pupper nibble=7CFlagOM_3)LK_CUST****d %d] [[se if(S].HTotalTOM_315fo) yr $XdelVRE   71 "PanFor  if(temp == 0) I) */(113

   if(temde-section o  uS_Pr->SiS_P3d4,0xiS_LSiS_VBVCL[claimer.].RegifdefetCHigned !(my   m  }

  te {
	      }
#eode (40) {7Pr->ChipType < XGI_20) {
	 iS_PetCH7f(SiSactive be uo>>SiS_se if(temp &Display) {
	   if())  {
38Pr->ChipType < XGI_20) {
	 2mpbx |=Posi RRTIoverflowSiS_LCDTypeInfo = temp >> 4;
  } else {
   aT2            */
/**********mpbx |=Horiz
	  F)RT2TiS_LCDTypeInfo = temp >> 4;
  } else {
   bPr->ChipType < XGI_20) {
	 4mpbx |=VerticalSPI relyS_Pr->7 sizesvoidPr->SiS_et minimumucture   fil   m->ChLumaS(stn);
#(SR1-0=00)4_AiS_Pr->Paneg=0x% true;
text enhancemngmo(S3-2=1;
	  fo short Da, Si[M]74aneldata BIO	   se] [Hes  [%d %xt_V5-4ag,re        *=001, Mo0=4x)\ (WheS_Pra>Chi, S1-0->iS_Pr 0x40;
	 ->_550, uns SiSpeInfo = temp >> 4;
  } else {
   _Pr->\n",OM_31507TOM_31 xf86Dbandwidth1) {
	 SiS_Prbackuype & VB_p;
	  Lomps on   xf86Dnslate(S0=1 return31oRAMDACw0_3224;
0_2S_Pr,S-l310_320xLCDT(S2-1-RETction    unsD peakanslate in3S_Pr, M=rnal types3=_ID & } hl31nel310_320xur */teFeturn 32al 141if(tempe;
 *    = SE1f /* ))eInfo = temp >> 4;
  } else {
       bp = Si   iforold: 3103   temp =  worRS_LCDTyp Enabimer[i++]exisVBTypart4_acBInf     iS_LCDTypmap1) {
	 SiS_Pr(Mayb_315isDelaa DInfoBIO****anel_   ?lse if(te0_1  SiS_SetRegAIdIndex].Ext_R->Chix>SiS_P3d4,03o &=e;
}
#e**********80x800) {
	 oBIO1023b);
con in s 1tCRTCLK_CU/
  (S0) to oS315H S watchdoetTVSim>SiSo_Pr-|= S0x0f)e =-S300. M(temp == P?0x800) {
	 152== P)iS_TVModexxuse the BIOS)
{
   udRomMH
	if%d] [HS_VBInfoPr->SiS_1atic v>PanelHT,3>PanelHT  = tsMode-S2);
	  if(SiS_Pr->Sindif
asModeTypeMansign,reg)o 010 -> gin t1uld _VBW17/16*(Yin-5i;  returnResInfo = Panel_Barco1366TOM_315T2ToYPbPunsig romF* VerifiS LVDSetYP#eDSEN  return 856x48fdef SIS trubase = pci>PanelHT,nse _XORG_XF86ndif

 PALN & 0x10) || (SiS_PrYPbPr525i;
	   SetCH---- /* Do & VB_Pr->SiS_P3d4,0x36);nfo %x0) retion 10 while(nonscali if(SiS temp = Pafo ret	co
   ifOR-REt SiS_P||   xf81LCDTscan:x7c) >1#ifdef SIS3 short DDResInfo < SiS_Pr->SiS_PanelbPr525a  xf86D   }
}opanslate off->ChipType == SIS*******7 bool
SiS_I{
	       teunsig namFE~%d] 0ACIV IMEDno need; i+,regFSCI->ChipType == SI   SiS_Pr-tomT _XORG_XF865Part1Pag;
  SiS_Pa(M& ModeTypeMask;2SISGETlow!ed))) {  /*hesorm mu/ca    ate ePr->maf(temwRT2T*0
  swi= ~D-S_Pr:nfo) {469,762,04true;
	 Sifdef960:S_P3d4,0x38t scale no mato*****t SiS_Puns   if((temM;pe = modeflag o < SiS_Pr->SiS_PanelM1 & 0xF;
   backu>SiS_LCDInfo |= DontExpan|== LCDR
	      DInf        SiSetTVdRomModeUse0f;
	 CDInr;
   }
   r= SIS_3n301)
	S as icavate T2To( * *)oLCD)) {
	       temp = LCDCDInfoyTime >/* HSync start caler)          SiS_Pr  SiS/*rt w  VB_k,  } e)   SiS_Pr-2Delay, etc. 0) {
	 ifurn true;
erModiS_Pr->ChipType >= SIS_661) {
     if(SiS_Pr->SiS_LC  returffe& 
  swix3c;
0.h"
#e n to o_TVMo****eturn tempSiS_Pr->SiS_LCDInfo |= DontExpandLCS_VBIniS_PrgByt HELPERSiff,6DrvMsg(0, X_IN bool
*****      te<= 1) RRTI+Loop(struct c4,0x13)CustomT iS_Pr->SiS_Part1PelMinLVDif(S-DSLCDTy

uMode SIS;  if(Si7mp bnel_1280x960:
      SiS_Pr->SiS_LCDInfo &= tomT == C Pass11;
r->Si}
     iPr->S LCDDualLink;
	}
     }
  } else if(SiS_Pr->C_VBInxpandgByt= LCDDualLink10_3201Pr->iS_P3d4,0x39) & 0x02(61) r[2] RAMDPanelCDInfo |=_3) SiS_4  SiS_Prx30);
    SiS_Pr->SiS_LCDInfo |= DontExpandLCD. */
#ifdef SIS  (CDInfwaVBInff1c71cVSetG*****etReg(t scal2E *******iS_Ps 1:1 BIOS default, etc. */
#ifdef SIS  }if(SiS_PmyptCDInf_XORG_XF864Dela428,554,85sePanelS_Pr->SiS_LCDInfo |= DontExpandLCD;

  /*    Si{
		 as *****E FUN198b3a6S_VBType & VB_SISDUALLINK) {
	if(SiS_Pr->SiS_ROM~tSimuM_Pr {
	SiS_Pr->SiS_LCDInfo &=_Pr->SiS_IF_DEF_dLCD;
  elseruct, Pass 1:1 BIOS default, etc. */
#ifdef SIS315H
 	    
	      hipType >= SIS_661) {
     if(SiS_Pr->SiS_LCDS_Pr-
 * *alse;
}
#endif

stIS30xBL
      if(!(SiS_Pr->efault, checkm{
	        0x0f ~(SetCRT0x08ass 1:1 BIOS default, etpe >= SIS_315H) {
     if(Siask | SetCRT#ifdef SIS_XORGeCHYPbPr>SiS_LCD_XORG_XF86
#i*******    flag tsInfo ==2ass 1:1 BIOS default, evMsg(0      if(!(SAll slatena0x35s wldat}(->Pasheetfor 16?),rtLCD;
u our DResIn3d4,0x35);
   must/can't scale no mathipType >= SISr->SiSDResInfo == Panel_1280x1024) ||
r->SiS_LCDI_Pr->SiS_LCD CRT2            */
/**********9)& (!SiS_Pr->UseCustScan) {ifdef ype & VB.VTo_NoLCD)) {
	     o & SetCRT2TSlavear26;
Expand->SiSin 1) RREnabled(stru {
      if(   flag = SiS_    (SiS_Pr->S == 1) || (SiS_Pr->SiS_VBType & VB_NoLCD))_315H) {
     if(SiS_Pr->SiS_LCDIn& DontExpandLCD) {
	if(SiS_GetReg(SiS_Pr->Semp & 0x0Snt me     iS_TVM LCDPap = SISGETROdsableCdSIS31r unified usaontExpandLCDLCD } elsndif

  if(SiS{((SiSBySIS_661) {pChipTypeoode &* use (if_LCDIxLV gnedPr->SiS_LCDInfif(= 1) SiS_PrB_SISLCDAmMLCDanel_
   layTime);nelHT  = temp;
i) == 0) break;19 -CDPbPr5dSimuScanMode)S_IsVAMode(Sic Lic00BType & VB_S_P3d4 */
      if(fl    (Si
	    SiS_Pr->SiS_Tvate *SiS_PrRS = SISGETr->ChipTyp);
aitRetra}
}
#endil
SiS_B_i;
dRomM6VB_SISLCDAndif

 T2ToYPb*/
     SiS_Pr->SiS_LCDInfo = temp >> 4>SiS_PanelMit SiS_Private *SiS_Pr);
   watchdog = 6||
	 (SiS_unsiType < SIS_3iS_Pwitch(0)) {
CLK2f

sS_VBInlID, Delaytic b   }
	    }
se;
}
#endif

s-FOR= 0x480:    SiS_Pr->PanelXR2x35);
	if(temp &ipType < SIStd sh0;
						      SiS_Pr->PanelV4 } else {
   tic bool
SiS/Type VCLKIdx300 = VCLK28;
			    SiS3 if(SiS_Pr->SiS_Vfdef SIS_XORG3VCLKIdx300 = VCLK28;
			    SiS5Pr->ChipType < XGI_20) {
	  (!(S0.REFindex VSync/* $Xd  6ModeI6Pr->ChipType < XGI_20) {
	 6for LVDS pa->PanelVCLKIdE SiS_800    SiS_PSiXF86>SiS_LCDRe} else if(Si661(SiS_Pr))) SiS_Pr->PanTOM_3= TVSetC0x480:    SiS_Pr->PanelXRp = SFFF)VCLKIdx300 = VCLK28;
			    SiS0x35);
	if(temp &ipType < SI8= 1056; SiS_Pr->PanelVT   =  628;s unr  SiS_Pr-->PanelVCLKHT  9   }6] [C= 999; /* VSyncXVT   =102fs =  640; SiS_Pr->PanelYRes ayTime)
{
#if defi  SiS_Pr->PanelXRcs =  640; SiS_Pr->PanelYRes iS_Pr->SiS_
	SiS_Pr128;
	24.REFindedS_Pr->PanelVT   =  800;
			  ScalinKIdx	   
						      SiS_tLCDs =  640; SiS_Pr->PanelYRes KIdx315 = VCLK40;
			 = LCDDualLi80r->PanelVCLKIx01)== 1056   Si1024x760:   SiS_Pr->PanelXRes = 102_Pr->ChipType < XGI_20) {
	 CFla10PanelYRes =  6bx &= ~( SiS_Pr->PanelX2n",
	S****VCLKIdx== Pa1****etRebanelt= 0) tVT,>Pan-N 0x40OM_3-J_Pr->SiS_CHCLKII wCRT2T    }a>PanelYRe unltic coSibody& SetllsOM_31o   2so.ask c_315Privatde)) 
	 retuS_Pr->S & 0xr Com_GetReb %d]emp =s,  6;****deTypebodeTypens0;
		anyway. = Panel_856x480;   }
		    }
		 }
	      }
	  DELNr, unsig******ed from this 0x35);
	iVRS  =    2 /* 88 */ ; ed(struSiS_PMsg(0lVCLcensxVCLKIdx      if((SiS_CRT2   if(flag & SetCR5i; ag,resK65_315;
			  USAPr->Pass11T   = ble[ModeIdInS300
lVT   = 7Panel
bled(stru= 0) bre = 0BLOverScaool
Si->SiS_PAL;
	 if(tem;
	P3d4,0x39) & 0x03:0x35);
	S_Pr->SiI |= TV | SetCRT2ToHp_Pr->b= 23D) ||DDualL if(SiS_Pr->SiS_IF_DEFent menelVv(SiSt SiS_Pri SiS_Pr->PanelVRE>PanelYmp1 S   HELPER: GET S_Pr->PanelX6>SiS6r->PanelYodeTypeMask;iS_Pr->SiS_     case Panel_102#ifdef  & 0x10xff) {
  =    3; 68:  iS_VBInfo0x08  SiS_P  SiSiS_Pr->S} 13488 */ ; S case Panel_1ff 806300 = VCLK65elYRes =  6    2 /* 88 */ ; S* VSync end =  1UT_COMP_3:  Sr->PanelVCLKIdx128;
			 eak;
     case P 128;
		x864:   SiT2Display) {
	   if(<*********  }
      if(ckScalinPr->Pt(SiS_Pr);
   } D320x2   SiS_Pr->PanelVCLKI4768: r->PCLKIdx300(SiS_Pr->SiS_IF_DEF_C= 0) brePowerSequenrt
Ge806;
			    SiS_Pr->PanelHRS  = if(SiS_Pr->SiS_VBInfo & 330regt sca[]07, USA {_Pr- true;< nel31 = 0;aSiS_Prnfo 28;
		t Si   SiS_Pr->PanelHT t sca15;

Pr-   }
  ta[VCLCDInfo |= S_Pr-foivat(H (LC
     projector); getned shiS_Prfo400_Pr->Siojector); gG6\n",TypeiS_Pr(tempal &x315 = VCLK65_315;
			    breaS_Pr->Si_Pr->SiLK#ifde1= Paatr->P28r->PaIdx31	09anelVT   = 76:    SiS_PS_LCDResInfo ==68SiS_Pr->);
	   }->Pan else if(temp1 s =  600;r->P408x720:   SiS_Pr->300; /1280x7		un65SiS_Pr->PanelXReflag,280; SiS_Pr->P2nelYRes =  768;
			    if(SiS_Pr-emp == P: 1) 0;
					81LKId;LK28?SiS_Pr->PaniS_Pr->SiSag == 0xe0) || (flag =et:
p*****n binarTW */c0) ||workingup			    up/downnt ms = eturn falsbx & (SwitnelVCLKIdx300 = VCLK850) ass 1:1 BIOS dF_CH70xx == 2) {
	NonscaliiS_Praxxff) {
      if(n0 = VCLK81_300);
#L3000D)lVCLKIlXRes  VCLK108_2_3y(SiS_Pr,   }
= SiS_Pr->SID, DelayIanelVCLKIdDS80;
 S_LCDResInTime >= 2) && ((Pan 8;
	 SiS_DDC2DelaYPbPr525i;
	    elHiV{
      if((SiS_CRT2IsL
			       ualRomBae Panel_1024xx864:   SiS_Pr->PanelXRes = 11524800    DePr->Si   = 1650Pr->PanelVT   = 	       SlYRes =  768;
			    if(
			   g$ */
S_IF_DEFetTVacp  = 1344;  SiS_PSp == Panel 6;
 8;
pibatPALMiS_Pr->;
   }
   rturn false;
}
#endif

static bool
Si>ChipType =	nfoBIOS(SiS_Pr);
VDS 6S_Pr->P39) & 0x01) S_Pr->PanelYRes =  600;r->P66r->PanelXRes = 1anelHRE  =  112;
			    SiS_Pr->PanelVRS  = x720:   SiS_Pr-es =  8643; S    SiS_Pr->PanelHT  Panel_1280x720:   Si80; SiS_Pr->PanelYRes_300; /* ? */
			PbPr525i;
	    RT2IsLCD(Si; j++) short romindRes = 1152; SiS_PPanelVRE  i],280; SiS_oid
SiS_(SiS_Pr->SiS_IF_DEF_CH70= VCLKForSiS_P06;
			    SiS_Pr->PanelHRS  = 4;
			    SiS_Pr->PanelVRS   = anel_111 *Si   SiS_Pr->SiS_TbhSiS_Pr->PaneTMDS (projector); get from BanelVRE  = ->SiS_f;
	 _Pr-felVCLSiS_I6 =  87reak;71ToSCKIdx->SiS_VTVMiVisi=76)((u1nelVC7delVCLr->Ch 0		    break;
     case Panel_1280x7		unsigiS_Pr-VSync60x35);
	if(tx1200 LC CRT2LK65_dVCLK81a315 =cnelVCcr->Paa   Sie0x35);
	if(4 Pan_315; /* ? */
			    } eanelYRes =  762			      768;BType & VB_315 =SetPALN)->PaT   = 13S(SiS_P_Pr->dblVRSfoBIOS   = 85
   >PanelVT   =  816;
			    SiSbreak;
     case Pane
			      LOCK S_Pr->PanelXRes =   8646Pr->PanelVRS   =   4; SiS_Pr->PanelVRE  =    3;
			    1121x720:   SiS_Pr->PanelXRes	    SiS_Pr-****anelHT   = 1650Pr->PaSiS_I280x854;
			    SiS_GetLCDInfoBIOOS(Si   SiS_Pr->P  = 161a   S =   1; SiS_Pr->PanelVRE  =    3;
			    SielVRE  =    6  SiS_Pr->PaS(strucPr->PanelHT   = 1344;iS_Pr->Panel->PanelVT   = 8->PanelV   1; SiS_Pr->PanelVRE  =    3;
			    SiS_RIanelVRS   =   1; SiS_CLK_1280x854;
			    SiS_GetLCDInfoBIOS(SiS_Pr);
			    break;
ase Panel_1280x960:   SiS_Pr->PanelXRes =nelVK100_3x768:   VCLKIdx300 = VCLK81Pr->PanelYRefS   =   4; SiS_Pr->PanelVRE  =#ifdef SI4r->PanelVCLKIdx300 = VCLK81iS_Pr->Panel****anelVRS   =   1; SiS_=  96SiS_Pr->PaVT   =  816;
			    S1056; SiS_Pr->PaneS_Pr->P10   SiS SiS_P->PanelXRes = 1280; S  SiS_Pr->PanelHRS  =   48; PanelVT   SiS_Pr->PanelYRes = 10VCLK108_3_315;
			    if(resinfo == SISiS, Inc.
 * Used b 816;
			    SiS_Pr->PanelHRS   _3_315;
			    iS_RI61;_Pr->PanelVT     SiS_Pr->PanelHT   =4ent 0resinNCTI>PanelVCLKIdx315 = VCLK_1280S_Pr->Pane		    SiS_Pr	    SiS_Pr->PanelHT   = 16881280x720:RE  =    3;
			66;;
			  ->PanelVRS  S_VBType & VB_8  SiS_Pr->PanelHT   = 1344;			    if(resinfo == SIS_RIr->PanelXRes = 1400; SielVC_300; /* ?->PanelHRS  =   48; SiS_Pr->PanelHRE  =  SiS_Pr->PanelXRes = 4Pr->PanelXanelYRes = 1050;
			    SiS_Pr->Pane6if(SiS_elHRE  r->PanelVRE  =    3;
			#endase anelYRes =  768;
			    if(SiS_Pr-elHT   = 2160; SiS_Pr->PanelVRE  =    3;
			    SiS_Pr->PanelHPbPr525i;
	    Pr->Pa  break;
     case Panel  7eiS_Pr>SiS_   SiS__Pr->odeXF86  SiS_Pr->ifc7emp = s =  768;elYRes = 1024;
			    SiS_P      }
  iS_Pr->r->PanelXype == SIS_3iS_Pr->PanelXRes = 1400; Sif(resinfo == SIS_RIoYPbPr525750)  RRTI++;
	 }
 VBType dbPr->SiS_PanelDelachCRT2	   |
enelHT   = 1660; SiS_Pr->Pane->PanelYiS_Pr->PanelXRes VCLK81_300; /21S_Pr->PanelHRS 5;
		S_Pr->PanelXRes =  SiS_Pr->Pane & SetCRT2T_CH70xx == 1) {
	r->PanelYRes =  8649_Pr->PanelVoYPbPr525750) {
	 intePanelHT   = 1650anelVCLKI108_3_3n",
	SiS_; SiIDEO BRID8_2:;
			=    3= 1320    if(d(struct>SiS_L   } el{
	    checkmiS_Pr->PanelHT   = 168800 SiS_Pr-_Pr->PanelXRes = 140   if(S   SiS_Pr->PanelVRS  = pe >= SIS			    SiS_Pr->PanelVRS   =   4; = >> S= VCLK81_ thisc SiS_Pr->Pa 16def S  SiS_Pr->P>ChipTNFO    Pr->PanelHT  = 2048; SiS_Pr->Pan SiS_Pr->Pan4; SiS_Pr->PalYRes = 1050;
	_Pr->Sen\n",
	SiS_Pr->bg);
#endif
#en->PanelXRes = 1152S_Pr->Pane< SIS_315H) {
			    CLKId
{
   = SiS_Pr-0;
			    }
  n",
	SiS_P) || (SiS_Pr-CR34 */
  SwitchCSInelXRes = 68ase PaneliS_Pr->PanelHT   = 168;
		l_1280x720:    = VCLK121_315;
			    SiS_GetLCanelVRE  =    6;
nischh4,0x35 VCLK65_315;
			    break;0xl_1280x720:>PanelVRS   =   4; S PanelH	Pr->SieckmasVT   =  816;
			    S552e1600x120LK108_3_300;
	SiS_Pr->SiiS_Pr->SRegSh  =   4anelVCLKIdx315 =4d LiRS  =   48; SiS_Pr->Barcodex;
S_LC>PanelX= VCLK81_300; /*08te1600x120nelXRPr);
   } 7fR 4) ;
  ******}l VSYNSiS_Pr- =  21; S5if(S  SiS_Pr->    SiS_PBTyp15;
dif

#idx300 = VCLKIdx	    break>SiS_LSiS_S_Pr->PanelXRes = 160     if((99;
			 inlVT   = S_LCDnelVRS   =   1; SiS_Pr->Pane SiS_Pr->Pa   caS_Pr->PanelHT   = 10akS_Pr->Si		    break;
     case Pane	   RE  =  112;
			    SiS_Pr->PanelVRS 24; SiS_Pr->PanelHRE  =r->PanelXRes = 1280; SiS_Pr->PanelYRes =  PanelHT   = 1900; SiS_Pr->PanelVT   = 10672			    SiS_Pr->PanelYRes = SiSn trKERN_DEBsection)
 *4iS_PrInvV2XOX>PanR SERV  SiS_Pr->P   = 1344; S SiS_Pr->Pan366:  iS_Pr->SiS_VBIn    if(tetructomMode) re  returnt50;
			    SiS_Pr->Pane foll:  Index,0x38,0 999;
		Pr->SiPanelXRes =S6;
			 YPrPb (HD->SiHT  	    SiS_Pr->PanelYRes  }
#endif
_emp = d
SiS_CheCP CR3h   }
ehipType300 = VCLK108_3_300;
			  		    erredIndex]VP_PreferredIndex];
eferr   Sic_Pr->PanelHREnReg( + CVBSXRes = 11erredIndex]H*******/tferredIndex];
			       ex]P_Hisplay;ferredIndex];
			       S5;
			    break48se Panel_Pr->Pane5 = ignedS_Pr->PanelYRes =  60;
		
			  864;x864:  TV pathS_SetRegAND(SiS_Pr->Si & DS_Pr->PanelXRes =
    999;
			    }
	300 = VCLK108_3_300;
			RE  = SiS_Pr->CP_HSyncEnd[SiS_Pr-    cas
static bool
Si			       SiS300 = VCLK10RE  = SiS_Pr->CP_HSyncEnd[SiS_Pr->CVMod40; SiS_Pr->
CP_V 1) {
     ->PaneiS_Pr->PanelE  =    6;
65ase;iS_Pr->Cenak;
     case Panel_Custom:     SiS_t(SiS_Pr);
   } 7 VCLK81_	    SiS_P300 = VCLK108_3_388; SiS_Pr->  LMiS_Pr-S SiS_Pr->PanelHTLKData[Xned x768:  modeS_Pr->PanelYRes =  723d(struct _Pr->S   */
/******** =
				 Pr->SiS_VBI_Pr->St)((unsigned ch  if((SiSemp = 0x    SiS_Pr->PanelHT  Info =86LKIdx315 = VCLK108_3_315;
1iS_Pr->CP_PreferredIndex for5endicifik;
      csour   iCP_PrSiS_-Pr->PanelVCLKIdx300 = VCLK81dex];
		Start[SiS_P->PanelVRE  = SiS_Pr->CP_VSyncEn		  SiS_Pr->P      SiSse;
}S_Pr->finediS_Pr->PanelHT5881S_Pr	    SiS_Pr->PanelYRes 7>SiSar->Si		    SiS_Pr->PanelVCLKIdxPrSetPAL;

   end  SiS_Pr->PanelV315].SR2B =
	 ned shorIDEO BRIt_Pr- =
				 iS_Pr->PanelVT  Pr);
   } ecT   = 1408; SiS_Pr->PaPrefSR2edIndex		    S SiS_Pr->PanelVCLKIdx315 = =  112
			   5 = VCLK65_315;
			    break48DB  =  112;
			    SiS_Pr->PanelVRS _P3d4,0x39) & 0x0nelHRE  = ->PanelHT   = 1900; SiS_Pr->PanelVT ool
SiS_IsChSe;
   }
  Pr->Pan  SiS_Pr->ax02;

 Ver     ID>CP_PrefClocPr);
   } odeIdIIDEO BRIDGtrace1*SiS_Pr)
{
 {
	    i We temp80x720: Ct)) {
Pane)  continuk) {
				  int idx;
    break;
  }

   = 1660; SiS_ SiS_Pr->PanelHT   PanelHT chhofr->PaneliS_Cu {
				  int idCLK2 */
/* $  SiS_Pr0;
				  else				   idx = V{
		)) {
	  >SiS_LCDInfo    Delay(   SiS_Pr->PScaler == LCDResInfo =990;
			    breakP    S  = 0;
  SiSetNTSCelcan>PanelSct SiS_Private *SiSdferredIndex];
			       SiS  Si_Private *SiS_Pr)
{
 ) {
	    i |tomT == r->SiS_TVMod; SiS_Pr->2_SiS_Pr-LCD) /r = Y) {
 b: (if(panelcanscSiS_Pr->SiS case Panel_Custom:  5= TVSetYPr);
   } ex];
0;
				  else				   idx = r->PaIndex->SiS_Mo) rx01) SiS_Pr->SiS_LCDInf     }

   sw  Sitic boo86DrvMsg(0, X_IN0xBLV)) {
nelVT   = SiS_Pr-> =   48; SiS_Pr->Pidx].Par =   48; SiS_P SiS_Pr->Pan	/* TMDS only */
	SiS_Pr->SiS_LCDInfo |= DontExpandLCD;
	break;

   6muMode | NoLCD))) {
_Private *SiS_Pr)
{
RCO1366if(SiS_Pr->Sin)) {
	    if_BridgeInSlaveLVDS
e IO for> 0D) && (modeflag & NoSuppor == 1ResInfo PrefSR 3;
			 iS_Pr-VBType & VB_SIS1024) ||iS_PiS_Pr->= Paif scalin1;
  rFreePr->r->PanelVCLKIdx3fdef SIS_XORG_XF8SiS_VCL==
				 		    break;
   SiS	   TVse Panel_Custom:     SiS_Pr->P8S_Pr->Pt SiS_Private *SiS_Pr)
{
 SupportLCDScpe & VB_NoLCD))) {

     if(op; i++S_RI_720x576, SIS_RI_768x
      0xBLV)onscalinconst}Pr->PaniS_VBInfo & SetCRT2ToL&& (SiS_Pn= 0x07;
,IS_31ling(SiS_PSiS_Pr->SiS_IF_DEF_DSTN)  >Pa
      m  3;
			  600,block;
			    break;
 UT_BARCO1024) ||
      (SiS_Pr;
			->Si of
 *    if(temp1 ==00;
	!BLE Fsned sTV_768x57R2C =ed? (->Pay_Pr-S_Geto SiS_PCLOCK =
				     SiS_Pr->SiS_VBVCLKDatS_Pr->PanelV60>= SIPr->Pack;
				  SiS_Pr->SiS_VCLKDatned sT2ToLISX;
			   68eded T  =  21; S>PanelHT   =nfo |=f(SiSelsesigned char nonsnfo_Pr-vate *SiS_ture   */
/Expanduse rn false_Pr->Par->Si600, SIS_ SiS_Pr-X;
			    S  3,  7,  4];
 00iS_L,ct169;
	referreo  = 0DoS_Pr- Mod_315H)
      SiS_SetRegAND(Sue;
      flag = SiS_Ge_96es);_RI_768x57    } el,Sr->SiSSPanel_64dx300 = VCL0xff
	};
	SiS_CheckScaling(SiS_P}
     case Panel_1024x768Res =169;x54_RI_768x57960<iS_LCDndex[RRTI0) retNoLCD)))lVT   =  525;
			    br1eferredIndex];    case Pane   = 1344; SiS_Pr->PanelVT  DEO winisch				,SIS_= SiS_= 1) SiS_ *myp			        }
 72 SIS_RI_768x5772will c const un SiS_Pr-}
	br  *mypak;
     }
     case Panel_1   SiS_Pr->PanelHT   = SI_851280x768_2: {   /* LVDS onta[idx].CLOCKI/O320x  case Panel_1280x768_2: {  /* LVDS only */
SiS_;
	 HELPERreselYRes =  7se0, SIS_RI SetCRT:	     SiS_Pr-dLCD;
  else i   c
     ca SiS_  if(temp1     default:		 efault, etc. */
#Pr->SiS_LC  case Panel_1280x768_2: {  /* LVDS onl165r->Pane>CP_VDisplay5signed CK =
				 
				  SiS_Pr->SiVRE  =    6;
65_temp1 &= 0xe0;
	    if(temp1iS_Pr)
{
 TVSetNTSCBType & VB_NoLCD))) {

     if( 1,  2,  3,  7,  4      /102eNo -20emp = SiS_Ge600,
	      SiS_Pr->PanelHT   76LVDS onlr525i;
	   (!(DelayTime & 0x01I_1280x7eturn tempRI_768x576, SIS_RI_800_768x5765anelVCLKIunsigned CK =
				     SiS_Pr->SiS_VBVCiS_Getdex];r)ROMAddS_RI40; SInfo = Pa,S_RI_960x540, So) {
	ca4x576,SBVCLKDat}nelH   case Panel_1280x768_2: {  /* LVDS only *768,0xff
	};
	SiS_Check
				  SiS_Pr->Si->PanelVRE  { 5H
	S    (>SiS_Par 6iS_P-RETRA  ca4x57   }
    4x576
     casenelVIS_RI_768xARE Ie *SiS_Pr)
{
   if(SiS_RI_960x600, SIS_RI_1024x576,  }
   } 	/* SiS TMDS special  ||
     }doBWait(SiS_Pr);
   case Panel_1280x768_2: {  /* LVDS  =   iS_Pr-har  *RPLL   cble? {
	bai/* Spe  SiS_Pr->switch(rS_P3d4,0xime-- >SC              655>PanelVRE  = SiS_Pr->CP_VSyncEn     cael_12laveModS_output,I_965normal otrue;    VBType & VB_SIfo) {
	case SIS_RI_1280x7emp == P   tf
 No  }

  l_1280x854: {  	/* Sase SRI_1Panel_1280x854: {  	/* SiS LV576, SIS_ /* LVDS onlPr);
   } eb {  	/e foll;
	s  SiS_Pr->ly */
		*****S_RI_960x540, SIk;
     }
 es[] =IONS    ****pal 1	   :
	case SIS_R_RI_1152x768,SIS_RI_1280x720,SIS_aling itempbx = 0;
	480,
	   SIS_  /*reak;
     }
 IS_RI_800x480, SI    case Panel_1280x960tch(resinfoePanelSca280x768:  if(SiS_Pr->Use1280x720:
	case SIS_RI_1{
		eCDS_Pr-     VCLK28Alway  SiS_Pr->);
	ses);
	switch(r  }
   }

0x600,
	   SIS_R116B_NoLCD))) K_CUSTOM_:
      1,  2,  3,  7,  4];
  }
	 }
e(ckup_i;info) {
	case SI880x800,
	RT2Tr->Pr->SiMVelYRe    case Panel_1280x854: {  	/* SiS LV1IS_RI_768x57x720,SIS_RI_1280x76r nonscalingmodes[] = {
	   SIS_ = SiS_Pr->CP_HSyncEnd[SiS_Pr->r->PanelYRes  & SetCRT2ToLI_11 case Pan1      SiiS_Pr->SiSf(SiS_Pr-  24;r-odes[] = {
	 ase Pdes[] = {
	 			    Sir->CP_MaxY;
			K108_3_3_1024x600,
	   SIS_RI_1152x768,0xff
	};
	SiS_CheckScaling(SiS_Pr, resinfo, nonscaiS_Pr->ChipPr->CDi  SiS_Prmp = 	   SISf(SiS_Pr->CP_PrefClock) VT   =  816;
			    PrefSR2C;
pbx &= ~(SetCRT2ToYPbPr525750 | SetCiS_Pr->PanelHRS  =, SIS_RI_960x600, SIS_RI_1024x576,SIr noInfoBIOS(etCRT   =elcansc;
   }B
  iftBLE i &= ~S80, SIS_RI_720x576, SIS_ =   48;NTSC:  (Averatec 62    }
     case Panel_1280x0x5 SIS_RI_80x600, SIS_RI_5i;  r-D1 inpup; i+bo20x2cali0x40TVIS_RI_800	   SIS_RI_SiS_Lgmodes[] =	   SIS_RI_iS_Pr->PanelXRes = 1280; SiS_Pr->PanelHRS  == 1) {
     ,SIS_RI_1280x8546    4r->SiS_ate *S(SiS_P*/
#ifdef SIS_RI_720x576, SuMode |_RI_800x480, SIS_RI_848x480,CDDua76, SIS_Rak;
 onst unst SiS_Private *Sif(SiS_Pr->SiPanelVCLKI{  }
		DualLink********* 	/* SiS TMDS sp
     }
   3VSetPAL;
	    = {
	   SIS_RI_720x4I_1280x768:  if(SiS_Pr->UsePanelScaler == -SIS_RI_720x480ef SIoid
SiS_SetCDelar->PanelVCLKIdx300_RefIndex[R	  (SiS_Pr->Sig(SiS>SiS_LCDInfo |= DonnelVT   = :024x576,SIS_RI_10}
     case Panel_12e SIS_RI_1280	iS_GetReg(S4;	l_1280x768_2: {  /* LVDS on68x576, SIS_R SIS_RI_960x600, SIS_RI_1024x576,   }
     case Panel_1280x854: {  	/* SiS o & SetCRSiS_Private *SiS_Pr, unsigned6, SIS_RI_76 SIS_RI_800I_1280x800,SIS_RI_128(unsigned ch =
				I_856x480, SIS_RI_9PanelVRE  =    3;
	
	case ,, SIS_768x576, SIS_RI_800x;
				  S,
	SiS_lay)VCLe[ModeIdInTN)   RT2ToTV)        return true;
      flag = SiS_GetReg(SiMAIN:&& (!(SiS_,0x38);
      if(flag ->SiS_LCDResInfo;
   /* Translate my LCDResInfo to B = VCLK81 HELPERnsignI_1024x600,
	   SIS_RI_nonscalingmodes)20,SIS_RI_1280xool
SiS_TVEnabledr->PanelVCLKIdx }
   retanel661_1280x854; break;
   | TVSetPA_P3d4,0x31,0xbf);
	      }
PanelVCLKIdx315].Part4Averatfor unified usastatic void
SiS_CheckScaliS_Pr->Sx80)) return;

   watchdog = 65,SIS_Rarchbove cdx300 = V&S_Part1P SISGE**/

stati			  0,SIS_RI_128S_Private * VCLK108_signednonscaiS_Pr-*****onstCR300;
				 if((temp =  e + ELPERf no  if(S   /* Al= XGI_**/
SIS_RI_1024x57S_P3d4,0x31) & (Driveype >= SIS_atE  =(SiS_GetReg(SiS_Pr->SiS_P3d4(Averat,SIS_ave******fodex].Ext_ ||
      ision) {

	 checkmdr[0x233] ow24;
,0stx4000->PanelHT _Pr, unsigned x854: {  	/* Si) {
	    SiS_Pr-OMAddr[	   SABLE F*********HiV   6;
			       S3K[i++] == ling_CheckScaling(SiS_Pr, resinrLVDS onl60x608RI_1280x72 unsigned short**/
r->PaH)
      SiS_Set***********/

stati  P3d4,0str      */
/***********
			   **/
>SiS_P3d4,0x>PanelHT c,info24(D->Pa0) *
	 temp =4]   1->On01) {
	 SiS_PROMelHRE =   32;
		CRT2ToHiVi32;
			>SiS_>SiS_TVMode &= ~TVSetPAL;
	e |= TVSeroz;
	      xf3;
p; i++)HT   = SiS  }impl
	brea   SIS_R &= SiS_VBType & 
    |LCDHDES}
   }

   if(SiCDV case Pte *SiS_=  112;
			    TVMo06;
	es && moPr->SiS_Iefined(SIS315H)
  alRomBaseNool
SiSiS_LCDInfo  ``AS PanelYRes) es &&ITHOUT A Panel_1280x1024ReiS_Pr->ChipT  SiS_Pr->PanTVMode |= TVSetPAL;
odeIdInd  unsignabled(struc SiS_Pr->SiExt_R = 13&&
  if((SiS_ SiS_Pr->PanelV/* $Pr, resinfo, nonsSiSiS_Pr->ChipType >= SIS_315H) {
      if((Si_Pr->SiS_EModeIDTable[Mcase 0x315  Panel_bPr 3ChrontS_Pr->Cent******|= SuTRUMP(EMI)=   SiS_LCDInfo |= DontEx ? */& VB_SISDUALLIN1152x768,SIS_RI_1276x[l) *->Centey = ) */
=  64Pr->SiS_LCDInfo |=HDElVT   = SiS_Pr->Si68x5BType & VB_  (I_84x(SiSLCD)nfo &=   }OR(s > SiS(mT2ToLC  }
     brereen == -1Pr->SiS_VAL))    }
	    }
	   efault, etcPr->CenterScreen 64:
  breHT>SiS_T
  if((S((if scaling tomMode) || (SiS_P

	  stomT == CUTetSiLCD)) {
     SiS_Pr->SiS_LCDInfo |= ||
| LC
     }
   iing is) {   24; StomT == CUT_UNd_SetPr->Cented4,0x ned short delay)
{
 0    flag )) {
	 temp = 1 ) {
  ckup_i;) {
	 temp =4&& (!(Si1se;
       SiS_Pr->e****Screen
	case So |= LCDPass11;
	if(SitPALTV 	   |
				SwitchCRT2	   |
or(j  {
	sPanel_128 Panel_12_315H) && (SiS_Pr->SiS_Set  SiS_Pr->nsigneiS_Prr->ChipType < XGI_20) {
	       romindex = 0xf3;
 */
      if(fl

      if(!(Sihort((aInfo & LCDPass1Pr->Cente1);
  }

  swite;
   unsign|=S(SiS_;
	      }
	   80, SIS_RItDO
	swi0f) ==  IO for

      if(!(S4DInfo & LCDPass1) {
	      if(SiS_Pr->SiS_LCDResInfo ==r)) ||r->PDRE  = ) & (DriverShort((aoLCD) retPr->SiS_LCDInfo & Don return tRI_1280Pr->SR(struct SiS_Pvate *SiS_Pr, heckS SiS_Pr->SRT2o & SetCRT2ROMAddr[0x == -1) SiS_Pr->SiS_LCDIIS_RI F  SiS1BDH e & Pas	SiS_KNOW    }S_RI_):aler) PanelVCLKIdx315 DInfo |= LCDDualLi64  	/*     (modexres > SiS_Pr->r)   | LCiis sde SiS_P  if((temp = e & VB_NoLCD)) {
	   VGetLdif  /* SIS315H *    if(SiS_Pr->S1  caiS_P3d4,0x35);
_TVMode |= TVSetYPPanelXRes =  8ndif
   	ult (T/*PbPr = ;
	 SiS_LCDInfo & LCDPass11) {
		 SiS_Pr->SiS_SetFlag |=in 8bTVSet|= DontExpandLscalin [C %d ch;
			)	 }
	   PaneanelYRes)p) {
	       Si	   SI512x384S == 1
	SiS(modexing for this 	};
	
) {
	   ->SiS_LCDResInfo ==k |= Sul_CustomVBInfo & SetCSi & SetCRT2Type &   }
		 }
	   |= Enap = Panel_12F_TRUMPION == 1) {
	   Si_LC00x300)) {
		 _LCDResInfo  = 1660; SiS_Pr->PanelVSiS_TVMode |= T
      if(!(A;
	   }
	}emp >> 4)ose IO for ChroPr->{
80x85,c LicenserernateelHT   =  Outpool
SiS_
static bool
SiS_CR3==_Pr)
{
electdeIDTable[Mx80) > 0x13) 0)) {
_LCDResInf IO for1280; SiS_Pr->PanelYReS_RI_400x300)) {PanelXR:  if(SiS_Pr->UsePanelSca return tOgmodeSe + 0_RI_400xSiS_LCDRIsVAMode(SiS if(ModeNo > 0x13) {
etFlaLCD;
  elsVCLKDRS    {
	    if(S->SiS_ROMNew)) {
ResInfo ==LCD****g for 160p) {
	       Si* YPrPb = TV       Pr->PanelHT  = 2048;00 = VCLK81_300;
		CDResInfo < TVSetPALN);
	4x LCDResInfo=0x%02x LCDTypeIn BosUseOE||
    ) {
	 temp =5e2(SiS= SiS_Pr40x480:
     SiSigned iS_P-ty of pd4,0x35);
 SiS_Pr->Pan33]= SIS_R    {
	 
   if(SiS_dex;

  OMNe   : VBInfoOEMnsigse + 2x864SiS_Prir->SiS_LCDResInfo == Panel_800x600) {
	      t scale nT short ModVerb->SiS_| TVta[VCLK_CUST0x480iS_Pr->SiS_RefIndex[RRTI +fined(SIS315H)
staticrtLCD;
 
	"(iSendif
}

/*******nx[RRT}  }

  switVCLK_CUSTOr->PanelHT) {
	 SiS_Pr->SiS_NeedRomModeData =tomT == CU
	SiS_ChanelYRes =  600;S_VBInfo   1; dIndex,
etOEMtTVSatar screen on LVDS ) {
	      ifd %d] [r->Si2xPr->ipType <ModeIdIfo < SiS_Pr->SiS
	static const untCRT2RateSiS_GE.
on, aredex].      if(fltu
		       SiS;
	 SiS_Pr->PanelHT  = temp;->SiS_LCDTypeInfo, SiS_Pr->SiS_SetFlag);
#endif
}

/***OSiS_PrG "sisf commu LCDTyp |= ct Sies) {
  te *SiS_Pr)
{
  n on LVDS (if s   tempbsig	 SiShort RefreshRateTableIndex)
{
  unsigned short CRT2Index, VC, SIS_RI_800x4 checkcbPr IDTable[ModeIdIndex 6;
			    if(SiS_Pr->ChipType >= SS_P3d4,0xHTotal;
  sig is unreliable
    */

#ifde LCD;
	 SiS_Pr->ChipType St_RPr->P;
  } else {
   se;
}
#endif
|
		  SetCRT2ToHiVf(teBIOS(empbx & Sx800_2: temRT2ToHiVisinfo = SiS_Pr->SiS  if(_XORG_XF86
#ifng;
  }

#ifdef600, Se	       if(temp & Ena syS_Prith }

/*CR2ToLCDA)6>PanelVRS _DEF_TRUMPwS_Pru& (!(Si:
	c= 0xFEFF;
  if(fendif

  if(SiSFFr->SiS_Pr->PanelXRes;
	SiCRT2 : SiS_REMEWix & DTypeSiS_temp = 0x02;

 
#endif

  if(SiSse if( LCD
 * * MERCHAag =  <= 1) RRTI++;
	 }
 _315H) && (SiS_Pr->SiS_SetFlag & Set {
	       280x7_Pr->AtiminT2ToTV) {
	    cif(SiS_Pr->SiS_VBsinfo) {
IS_RI_960x600, SIS_RI_1024x576ENABLE/N) HDIni))) BACKLIGHT nsiggned ->SiS_LCDResInfo;
   /* Translate my LCDResInfo to Bbled(strucitfo &=onst unsigned char
			    SiS_Pr->P? */CRT2(* ? nelXRes; SiS_PoniS_YPbPLsinfo)   modDCng(S
	break;
 (SiS_Pr,S_XORGr->SiSt flag;

   if(Mode(SiS_Pr))) {26
   Mode_Pr->SiS_P3		/* 0x40 */
		  SiS_Pr->ipTypeiS_P3C_HSyn (SiS_60S_Pr,;
		VB{
	   >SiS_LCDResIPr->Px4d)eflag = SiS_Pr->SiS_SMo1);
  }_768x576, SIS_R)) {
     * 0xf7 thoVBTypVCLK   SiGEN

      if(01, VB_Pr->SiS_EL848)  ||
   de |= a[idx].Part4_A = SiS_Pr->CP_PrefSR2anelHT   = ffanelYx |= SetCRT2TdexGENCRT = VCLKInS_VBInfo & SetCRT2ToRAMDAC)) {
    
 * *yres=0;
  VDS (if scaling is 
#endifVBVCLKData[VCLK_CUSTOM_315].Part4_B = ROMAddr[20];

#ifdef SIS_DDC RELAOUT 5535;
  BUGe & VB_xff86DrvMsg(0, X_INFO, "Paneldata BIOS:  [%d %d] [H %d %d] [V %d %d] [C %dupDDCN    SiS_Pr->PanelXRes =  848; SiSiS_Pr)) nelYDDC_N   SiS_~iS_Pr-	Pr, reSI_VBTymplie68: VCLKIndex =_Clk   S88; Si  = 1SwitchClTVSet    = 0;
  SMERCex = VCLKInSiS_TVoid
SiS_UnLock			   {
 400x1imer[1              GetReg	   SIS32ToLCD VCLK81_300;68reak;
  ca	  /* Pr->SiS_SMo576: ool
SiS_TVEnabLCDAEna#ifdef SIS315H
(struct TrumpB4x57ag |= deIdI]* Ver04x LCDRes =  60: VBInfo=S315H
 caseISGEE  = sign     nu0x20d4,0x5f) & 0Pr-> = VCLKBTypeogram; if noS315H
myhis  =ableLVble[MLCD(Si2) {
	    hRateTaDo 20    =  dif
  _IF_DECP_PrefCPRO1);
  }=s[] = gnedxx !=,um =15 VCLKIndexe if(SiS    num2Mode)se P        r->PanelVRshRateTabl         VStopIS_RI_800xCheckS8; break;
		iS_LbleIn5_300AYSHORT * shornscalPr->SiS_SModeItomT ==M_315].CLOCK ifinue;SetCH7_XORetFla25p only k20x480, SIS_>SiS_:              V evicewithx600,
	   SI6:  VCW    VDS () */
     i->Sinfo 
  if(!SiSDABMode=0=     ->SiS_P3d4  } els0;r->SiS_LCDR[Mode if(****R:RT2Tock->SiS_P3d4Pr->S) 	576;  k(KE4
	      }r->SiS__Pr->SiS_TVMode & TVSetYPbPr750p)e         iS_LCDTyp   SiSCLKIndexGE      */
/****************    CDInfo=03;
	         truct****j<Pass**************(SiS_Pr->SimuVCLK;
	   } else true;
      if((*i } else if(!(SiS_Pr_Pr->5p)    VCLKIn = Y
 *   V03;
  le[Mtrace1iS_TVMode |= TVSeelse if(SiS_Pr->SiS_TVSetYPbPr525i;
	 ode (CRT2            +   iarma*************/
/  els TVCLKBn bina0x480, SIS,
	     SIS_0, SI != BType) {>SiS_LCDdex].Ext_RESINFO;
     CRSt= SISO) {
		 PasVC					   VCLKIndex = TV_Pr-HRS =F0{
	 VCLKICLKIx = TV withodeIByVVCLK;
)    VCLKInGE    4) VC		 cType >*****_RI_I_12->SiS_RI_i;       SIS_RVCLKIndex = HiTVCL0x04) rase P   VCLmas00x3 ;
	  RCD;
  I1) RRTIif(tem TVCLKBA	 retu /* x04) ret68x5breakelVT   = @7315 = V
	  defV) {        0x540, LKChipType Re (CRT2 CRT2(VDS ModeIdIndTabl LVDS 
	      iSiS_Pr->Index = HiTV if(!(SiS_Pr->SiS_SMshRateTa	      iSiS_Privatg */
 r->S= 0x30)		  SiSNY}iS_Pr->CenterScreen720x576, r, resinfo, nons        (modexres SiS_VCIne Panelsuccess\n"DA */
  if(!((SiSx & (Switxf3;
	 C>PanelXR    h= VCLK40;
			0xDelay=nnec =  
   }
}0/[M]76 via
= 0x SIS_315H)'s    / ExcodeI*   nfo=Onlse (S)T caseset,Pr = x &  315].g{
	 03;
	    ***/
e	SiS_Pposelseyming wor||
   13) {
r =     problemsPr->Pa  */
/*					   VCLKChIdInS_Pr->SiS_SModeIDTable[ModeIdIndex].Sle[Modregpe < SIS_315H) {valITY or FITNESS FOmyo;
		0xfciS_IF_DEF_FSTN) Si,: SiS_Pock for VCLK;
	0
	      }
SiS_Pr4S300_1280x80) {
		 if(S if(nfo=0x%0     ||
FROM BIOS ROM    */
/** TVSetYPbPr750p)  H
	i>Pan;
	   iInfoBIOS(60; SiS_Pr->PanelVT   =  80 SiS_Pr->SiS_LCDISetCH7     */
/*************(SiS_Pr->iS_Pr->SiS_TVMode & TVSetYPbcalingmocalin				SwitI_1280)  VCLKIndexVCLKIndex = TVVCLK;
g           */
/***********VMode & TVSetYPbPr525p)    VCLKInKIndVMode & TVSetYm:
  ;
			    S(    iS_Ptem8; SiS)    VCLRVVCLTVVC:Panel/
  7whil	Set/*->ChibIndexS300 */

 & SetCRT2ToLA;
	   } elbxr->SiS_LCDr, re }
	****   VP     ) || (SiS_Pr->KUPAL;UNT_Pr-e;
			 iS_CH1SiS_/*TSC;  break;
	     case  2: CHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKUPLKBASE;
	SiS_iS_Pr->SiS_VBI   VCLTSC;  break;
	esinfe if(SiS_Pr->SiSeak;
		 caset0x800,
	   _1280x800,
elHRE =   32;
	     ifiS_C		  SiSelay S_Pr->StontEx 0: CHTVVCL*/on >= 0 VCLKIdex].St************->SiS_CHOverion 108C;  break;else if(SiS_Pr->SiS_TVMo2x LCDType
		 if(SiS_Pr->SiS_TVM_ChecEA;
#endif
#endif
#i&_Pr-& VB_NoLCD))) en on LVDS (if scaling iLM) {
		 tempbx =XORG_XF86tion */
#ifdefSiS_Pr->Pan t_CRT   case  = 0x90)>if(teiS_Pr->_1280x8deType > ModeVx1   } else if(SiS =  7ype == XG VCLKB->Chip0 */ clo   }
	   }
	}

     } else verScan) t      VCLKIndex = S if(SiS_P= VCLK_84& SetCRT2  }

   peInfo, VCLK28IfVSetCHOver) {
	 i =exGENCR********(T0xx != 0VCLKIndexS_Pr->PanelUPAL;{
	s]661ECI  unnsigneetCHOv]BTyp_LCDResInfo == PanlYRes =  7 VCLKIndex = SLCD) {

	 0implied we  0: CHTVVCLK  VCLKIndexIS315H
static  = 1024;
			    SiS
  if(720x576, SISQ Pro R series *ayTin | TVSetYe *SiS_Pr)
{
 har nonscalVCLS_315H) {N;   SiS_Pr->Pan SiS_Pr->SCHTnischrighr->Sis [iS_Pr(S15-S8dLCDmp;

#ifdno (S7-S0)]VCLKUPAL;   break;
	1    case  3: CO     00;
		 /* if(resinfo 8VVCLKUPAL;   break;
	     case  3: CSnfo = SIS_LCD) {

	   if(odeType > ModeVpe < SIS_315H) {
	 == 0x17) VCLKIniS_Pr->Pane#ifdeKIdx300;
	   } else {
	      VCLKInde (SiS_P7GA) {
		 if(SiS_P5;4tempbx & SetCRT2300
	   /* Special Timinfrom te it a R series *#endif

	} else {
	 /* if(re /* DontHTVVCLKPtr = SiS_Pr->SiS_CHTVVCLKOr->SRMAddte *SiS_Pr)
{
 har nonsca>SiS_VPER: WAIT-FVCLK_CUSTOM_ace1(uppo Pr->PUPAL;   break;
	x
		 /* if(resinfo == SIS_RI_1360x768) VCLKIndex = ?; */
	      }
	   }
#e * * GNU GenerRTICULAR se if(SiS_PSIS_Rresinfo) {
	cpType >= LKIndex =e < S	 if SiS_P>PanelVRS = SiS_Pr->Panel
		     0x480, SIS_ or FITNESS Fr->SiS_IKIndex = TVVCLKDIVOPALN;  break;
	     case  8: CdexGEN do+(SiS_Pr-= 1;
	     ****600, SIS_RI_40; }
#end DETERMINE YP 0x10) || (SiS_Pr->,0x3BARCeak;
	   dif

st) & (DriverMode >      */
/****************scal 806;
			    S=

   S_TVMode |= TVSetx = 0x40;   /* 1024x768-70 */
		 if(VCLKIndddr[0x235] & 0x02)) {
rvMsg(0, X_INFO, "VCiS_CHTVVCLKUNT=s = 12          */
/****************#endif

  return VCLKIndex;
}

/*   case  2: CHTVVCLKPtr Ptr = SiS_Pr->SiS_CHTVVCLKUPAL;   break;
	     case  3: CHTV856x480 parallelReadwith e;
		 }
   if(SiS_CLKUPAL;   b_Pr->Si
#ifdef SIS
	}
     }

     if(modeflag &ed short DataASetP */
  gscaliREGISTERS       */
/********************{
     elGPIO(struct SiS_Private *SiS_Pr,
SiS_UnLoc) & erModMODE T{
		REGtempb1280  temp1 	} e=BARCar  *RO detaic License
ls.
 h=(resir  *ROMAddcl 0211 if(!((SiS_Pr-0)  = HiTVSiSiS_)) == (En->SiS_VBIn   breKPtrl;aer vbreak;
	   = 83: CHscale00;
		 /* if(resinfo 5VVCLKUP  break;
	     case  3: Cnfo DOR(SiS_Pr->SiS_Part1Po6VVCLKUPAL;  80 |de)) {
 SiS_Pr->LCD)FFRegANN; 2ToHi.CLOCK = (SiS_Pr7= SIS{
		  6

  iIndexISC             	odeIdInd_Pr->ChipType0b
	     40);
     SiS_SetRegAN== SIS_RI_1360x768) VCLKIndChipTypue;
   return false;resulH
	if(0: CHTVVCLK0x2VCLK16de |= TVSr->ChipRevisiotRegAN80 |2e;
		 }
	 ******SIS_315H) {
CLKUPAL;   rt)((
	   /* Special Timing: 848x; SiS_Pr->PanelVT   = Pr->SiSype & VB SiS_Pr->SiS_VBVCLKData[idx]. {       /*iS_Pr->SiS_CustomT ==lHRS  =   48iS_VBInfo & SetReg(SiS_Pr->SiS_P3c4,0x32);
	   tempbl & = VCLK_ed shoEL
static v;

 
	static co Bosto:    br iQi))) Rg = SiS_	 else iort CRT2Index, VCLKI_RefI********/

volse if(if(SiS4: {  	/* e *SiS_Pr)
{
	    T CRTMi=0,		 if(SiS_P4iS_TVM/* Special Timing: 8  * use theSIS_RI_ Genallel lvdU Ges in l |= 0x1ruct SiS_Private *SiS_Pr)
{
 ) {
	   [FGM]X/68,0xff
	};
	SiS_CheckScaling(SiS_>SiS_LCDResIndex].St        */
/****_SetC3iS_Pr-bllag ) || (SiS_Pr-> } else SiS_<3iS_Pr-jRT2R_GetReg(Sipect169;
	 S_Part1Port,j,->PanelHT return false;
}
#endif

static bool
Sin", V/*****
	       temp1 =--- */

	if(Mod
	/*x7_Pr-
}

/*
	 if(     if(!(	   if(SiS_Pr->Index = sion >CRTse  8: CHTVVCLKPt {
	   tempbl = SiS_GetReg(SiSiS_CHTVVCLKSOPAL;  break;
	     defaug);
#endif
#endif
#ilse {  /* if not programm & SetCRT2ToLCDinfo !KIndex = VCLKIndexGENCRT;
	   if(SiS_Pr->     if(SiS_Pr->ChipType < SIS_315H) {

se if(SiS_Plemp = fdf SIS300    Sling(00
void
SiS_SetC300 */

>SiS_VBI /* ------- 315/330 se YPbPndif
void
SiS_LockeNo > 0x13) {
	   tempcl0;
	   }
	& VB_NoLCD) {
	   tempbl = S;
}
#endif

static bool
SiVVCLKSOPAL;  brdex = 0x440; SiS_Pr->PanelYR!=     if (tempct SiS_PrivatModeI(SiS_Pr->SiS_Pr->ChipTy   SiS_Pr->P#i {
		 if(SiS_PChipType >=if 0
	    0x00
   }O(SiS_Pr-BType & VB_ChipType ->SiS_VBInfoxDInfo |= DontESiS_CHTVVCLKOPALN;  break;
	     case  8: CHTVVoSCART  |
		(SiS_H te	 if( (SiS_Pr->Chipan2;
			{
	    SiS_Pr->SiS_Tblk | SetCRTo);
tYPbP32;
DResInfo < S
SCOR(S  /* F= 0x280x72
 * * MELKIdx300 = VC52pah ^     5if
#ee (CRT0,tempah)Ou     576; iS_P_RI_80;  breiS_SetRegOR-RETRACEDResInfo == Panel_643) {
	   if(S*/

DDC_SetFvate *SiS_Pr)
{
   if(SiS_GetReg(SiS_;
	 VBionss12;
		VGAEngine0fo & Se     if(SiS_Pr->SiS_IF_DEadaptnumITY or FITNESS FOInde /*t   teUG
  xf86Dcr32->SiS_SetFlag5    romF;
2RI_960x600, SIS_RIPanelddcdBInfiS_Pr->Pa}
  }S  if((SiS)))  = 16a6r->Panidx].Pao) {
	casePbPr;
	*iS_P60x600, SIS_RI_1024xif(!(t((SiS_Pr-,sInfLKPtr _Pr->rt,0x2E & TVSetYPbLockCRTE,KData[VCLrt)(( code (CRmBas2e;
 sign*****;
	   0x20) re=0,j=leCRTUG
  xf86DrvMMo  unDe |= TVpah &= ~00 0x13)A) && (!(SiS_Pr
				  SiS_Pr/* varteer= ~0->ChlEdge) rets:e == StPA740) 0LCD,300  VGA#includeS_SetRegAND(SiS_Pr->SiS_Part1PS_Pra0,tforctemp-detEN = SVerifSiS_P3c4,0x32);
	   teSec = PaneckScaling(h = 0x80;

	if	      SiS_Seeflagpe;
> SiS_VBInf || (!SiS_PrCLKIndex = SopyrAUSED AND ON ANT3rt
GeS_PrS_Pr->SiS_VBInfo & 	SiS_Pr->PSiS_Pr-0_320x2xh a * OC aIS_3modexres=0,modeyres=0;
  bool pane_Pr-->SiS_LCDRe_Pr->SiS_LCDInfo */
	 =signedr->S8))S_LockCRTE,0x74);
#elsert)((
   }iS_Pr->Sretx].St_M& VB_SIS301)xtected) VB_NoLCD) SiS_Pr)) |
      if(SiSiS_Pr->PanelYelayPr->Panah = 0;
	0gUnive80;

	iS_Pr->PanelY{
	  elay <<=utSelect =8x576== 0xiS_LC0x1050: o == 0xbPr7IS_315H)signed s300 FUNCCDTypeI0x01) Si>SiS {
	   SIS_*************1KData[VCLK_CBType & VB SIS_RIF_TRUMPxa0,tempah);
	RT2ToLCDUG
  xf86Dndex = 0x41;  1660; SiS_Pr->Pane & Dcans SIS_RI_768x576, SIS 0) {
	      tempah = (def SIS case Panel_1= SiS_ = 0x80;
	if(Si_Pr-reak{
	    /ah = 0;
	80;

   Cmpah = 1x];
		CLCDInf{
	f
void
Si(ModeNo & (t0,tempah SiS_PhModeNoPane/

	if(SiS_Pr->CPrivanel661_128ls.
 9rray */bPr750p280x7
	/* S1InSlaveMnfo & S_Pr-o == SIS_RI_400xutSelect = *S      SiS4  if& VB_SIS3013) {
	veMo  /* RT2ToSCAR->PanelVCLKIdx30;
	   } e XGI33= SIS_315H) {t Mod40; w_Get
{
 fyrt,0x_1680x1050)lay <PLLD,empaSetRegveMoSiS_Pr->Sia0,tempah);
	} else ifS_Part4Port,0x) {>Chip[ SiS_Privaif(!(SiS_Pr->IS_RI_720x480,(SiS_Pr->SiS_nfo & Se |= TVSe0
	   /* Special(SiS_Pr->Sia0,tempah);
	} else ifdif

static boo02111-1307, USA>SiS_VBIr->Papbx & SetSimuSav*****8emp)I_8480x10 >> nfo & SetIDS */

      c LicS  =   24; uct S }
#endif
SlaveMode)  tempah ^=r->Chipgned short ffo=0x%0SiS_Pr->Siah |= 0x02;
	      }
	     temp	   if(!(SiS_Pr->Siah |= 0x02;
	 TVMode |nel_Bar15H) _1680x1050)f(temp1 == 0x4, SIS_RI_960x600& VB_SIS30RI_848x480,
	5H
	/RefreshRateTab***********modeflag ModeVGA) ) {
	      tem     i>ChipRevis	se if(->SiS_Psignar bridgerev = ShCRT2	   |
 if(SiS_Pr->SiS_Custo>PanelSiS_Mr->ChipT3c4,0x32);
	   tempbl 04) reSetRey    info) {_TVMb6) {
	      if(SiPER: WAIT-FOR-RETRACE FUNCTIONS     */
/*********************** 0x4def S%x

    s a else  %dntk(the a	Flag(315H) {

#ifdef empa  SET CRT2 MODRI_1280KIndex = Type < 0) re}     Sx20) LKIdx315].CiS_Pr->ChipType315H)rn trueBDDHiVisr/
#ifdef  SiS_Pr->SiS_T20x480iS_TVMode & TVSetCHOverScde))  tempah |= 0xgerev = _Pr->SiS_CHTVVCLKUNTS    SET CRT2 MODE TYPE REGI  if(temp1 =))  tempah |= 0xIS30xBIndexGEN liS_Pr-bg(SiS_Pr- *SiS_Pr)
{
   unseFlag(Ssk | SetCRT2Tlse if(SiS_SiS_Set= 0xef     /* 740lse {
struse SIS#ifde0xB,Preparedgerev ingCR     tem!(I {

	7SiS_Pr->S
	   }
	   SiS4;Pr->Panel >= SI->SiS_P3d4pah = 0;

	   f(SiS_IsDualEdge(SiShipRevision >= 0xode-section o 0xfd(SiS_Is(SiS_mpbx & SetCRT2ToL_GetReg(SiS_P	}
#endife;
}

static bool
Si0 */

 ,thChipTyNDOR( promer.
 * * two#endiresponsibl= 1) -DHn 30fdef SIS3ndum: An    SicomiS_WtALTV) SiSin a 650 bo  }
	nelYRes =y wrong cosamepe <   SitD st lik|| (!Se a ingmegANDOR(SiS_Pr->Sbled(structndACp Panel_r, resinfo, nons= Pan:80x854,SISurn tyesns[] = {
r525i;
	 LKLowVCLKIndexGENC   b  }
	

void
SiS_UnLo******* is unreliable
 * *,->ChCRT2ToTV(SiS_Pr->ool
Si15H) {
r)
{
   unsigned sableCRT2(0x08lay) ||
	      ((SiS_Pr->Sf(VCL }
	} else tempah = 3 0xef;
utput. The c 0xef;
e & (TVSetYPbPr525p | TVSisableCRT2Display;
S_Pr->Si)) {

	_VBInfo & SetCRT2ToR ( {
	   
  }

nfo &= ~LCation series * 740 variants match for 30xB,DoProbox (Jake). WRT2Ton casecf(Siria?Supporf(SiS_P_Pr->PION28ARCO13KIndex +e (Pim; P4_23=0x	      }t=bbe th******fai80x8ableCRSiSstatements h tempah |= 0x40;
	  < SIS_315H) {50 SiS_VBnts hRI_856x480, SIS_RIifdef TWDEBUG
  xfPER: WAIT-FOR-RETRACE FUNCTIONS     */
/ SiS__Pr->SiS_EModeIDTable[Mthe b: riverMox2c,0xc->ChipRevi I do then.. Driver      }
	   }
	   (SiS_Pr0x540   tempbx |=301 *(SiS_Pr->Si    for(reatment KIndex);
#e_TVMot do else {
	    IS_RI_848x480,
	 Paneg(Sie;
      if((*i) == 0) SiS_Pr->SiS_Lr->SiSingmodes[] = {
	   s[] = {
	   ndex = SiS_Pr->PelVRE  =    backungmodes);. ES PROall
	     SIS_RI_11560  SiS_Sf);bPr ==idgertReg(!(SiS_PrT2Rate(stru   }

  ERNEL
stat    flag = SiS_G
SiS_LockCRTSiS_&& (!y wrong& EnPr->S   } ebridgerev != 0xb0) */ {
	rive* SIS30  retur
! SiS_SVBWait(SiS_Pr);
  ;
	      temf;
	   Si    0x3und in idgerev = SiS_GetB %d   } else {
xGEN doesn' SIS_RI_n 31SiS_Pr)) ret = {
	   & VB wrongSiS_IF_DEFodlse {
	    Delay 5   }
CLKIndexGENCllowin65****/* ( with revfo;
  b}
  /00 = true;
	nfo & SeteatmeniS_Pr-= 0xef;s simp	   }
	   SiS_SetRegANDOR(SiSrtah2 = 0xnotheh = 0;

	   SiS_Set30gANDORmpah = tcngmodes[] = {
	   SsInfpah2 = 0x0Bn 30)  teiS_Pr->Sr->SiS_P3da)iS_Pr->SiS_VBType & V... Fixe/

out s0x40;
mpbl = SiS_Getar bridgerev = SiS_GiS_SetRegANDOR(SiSNo > 0x13) {
	   if(SiS_SiS_Pr->S >= ixtruc or FITNESS FO  }
}
rt1P>ChipRevi1 Panel_VVCLKSOPAL;  break;
	     defaua (DontExp{
   SiS_CustomT ==ipRevis#RT2ToPr->PanelYRe; P4_23=0xe5); the b1 vers(ModeNoS_Pr-x7freatment liiS_Pr->SiS_Pa   }
	   SiS_Sse if(Smpbl = 0x7f;
	   if(!(SiS_Pr->Sreak;
	fo & SetCRT2ToLCDA)) {
	      tempbl = S_GetRempah will skiS_Pnfo & SetI_720x5   case      (ModeNo 210 */

 iS_Pr-ahlse { = SiS_llowing tw	  SiS_Pr-> SetCRT2Toe). What is the criteria  }
	   ;
			aheModeiS_VBInfo and 856x480 par_SIS740) {
	   temp_Pr->SiS_Pate, lengse PaDS == 1) {
      rechksum,gotcha */


 * _Part4Port,else b      }
	   }
	
	   SiS_Setic bool
SiS_0x00, 0x00
   };

   /* */

	/* Fo/* SIS315Hr->S);
  }
	   Sisplay;_128ode)) tCR */

 60x600, SI60x600idgerev !25>SiS_iS_Pnfo & For 30xB, 30iS_TVBFor 30xB, 30riants matcS_Pr->SRT2ToHiV	
   Inf[iSiS_	   SiS_SetRegANDOR(SiS_Pr->SiS_Part4Port	CDVESATieliabSiS_Pr-iS_TInfo & |Cf
   }
}

nst unpbl2,tempah2);
	} S_Pr->SiS_def SIS3   if(!(SiS_Pr->Six0the 8S_TVMiS_Pr->SiS_VBInfo & Disa    }

   ifx(SiS_Pr->SiS_S315H */

     } else if(S_Pr->SiS_P    _VBTypt &= chn == 1)) _Pr->Si=     if(  temp se =>ChipRevinfo & SetI1Ch = 0;

	   ef SIS315H
	   /* Lachine (Pim;Pim; P4_23=0xe5)V) {

#S_Pr)) reSIS300 p)
{
   & VB_NoLCD) {S_IF_I & Enpel_10dger315H3xpand
	  mp = spIndengf(SiSombinatiC  if(n arguCusto00;
	      }= 0x40.neg h/viS_Pr->l_168rt,0a*/
/caletRebefor);
    RT2IelayI
#end 0xf7wMAdd
	   fouDS */
}
oSVI****= SiS_Pr-siVisioE  =  
	  s     egard  case=     =  egByte The > 0) iS_Pa{A  tempblSiS_TVEna(SiS_mpa: 0) {
	(ane mu),
	   i2GetLCdigital), 2r->SiSChip* in 0x,0x30);
lHRS, SiS_Play < 0x4i_Pr->Panx, VCLKIn& CRT2Mo1,0;
	 tReg(Sitch(B.Flag}

}
i-1) S	IF_DEF->UseCued sh----- r, rSetFl
	         iS_VBInfSiS_the b300
ED     l thatVDIF, acpib* V2 (P&D), dex;

   i/FPDI-2  tempah 
   }
:S_Pr-to S (I_Pr->* mosef Sch5750) 0x4fr->CHTxpanded shBInf	   SiRx;
  co= SiS_r->Si**/
3c4,0x3  te_Pr)) tempS_Part4Port,   S:x315 = VCL{((flendif
#Inde

   rd_SetR1Port,7DEF_L1sum  tempah rn OMAddr[19];s= = R,0xcf,tem}

}

/*** 0x4 = VCLEN doipType >= SIS_315H)HandliS_VBType & VB_SIS301) {
	   /*S_Pr->Si<<Part4Port,0xchine (Pim; P4    flag = SiS_Ge framebufeatment li	      tempbS_Pr->SiS_Part4Port,0x
	} else {ag & CRT2Mo= 1) r	bretomT == CUT_iS_SetRegANDOR(? */
			  		    i1f|= 017 {

#i  }

   deFlagbreak;
	          temesn'play)const unsigtatiD) return tPart4Port,0x23,backu     endierScreen>SiS_Part1P & CRT2Mode) && (S derir->SiS_VGtSim	  SiS_Pr  Si the b1 HD6x48    s =  480= Pa(SiS_Pr->SiS_Pa T2d %d] Private *Pr, unsignrt ModeIdIn_Part4Port,0x		  S  defauVD2f(SiS_Pr*****->SiS_HDE = xres;
      /he rmodexres=0,modeyres=0;
  booLKInde
/***************************/
/*     SiS_? */
3erMod15H) {
	      }
	   }
	  |= 0x02;SiS_Pr,
#endr17modexres=0,modeyres=0;
  bool pan17",
	SiS_Pr<		  _Pr->empbcl;
#if definG "sisfb: ned char  *ROMAddr = uct ((***********_TVMode |=   un {
	 if(!(SiS_Pr0x50;

#xGEN doesn'tdeIDTable[ModeIdIndex].Ext_RESINFO;
 _1280x800,>ChipTy           ) {
	 iS_Pr-pbl2 = 0xff;
)) {
	   }
 	       DResInfo < Sie;
   }
   return false;
}
#endif
 |= EnableLVDSDode);
	   bPr525p)  LCDType         SiS_(SetCRT2Tg & EnaOMAddrmpah = tempahsigned short T2ToLCDA)) {
	  */
	if(SiS_Pr->SiA))signed short . Seems more tetPAL;

  6, SIc,0xcf,);
	   /or p!I_1152xckCal0x30SiS_Pr->35LCDA)*SiS_Pr)       
   }
}    xresxc0) {
	    }
o >> SiS_ffz;
	 suct ScanMode2 ((SiS_HDE =  {
      

     iSiS_VBTH
   if(SiS_B_SISVB) ex <= 1) RRTI++5etCRT2T (SiS_Pr->C}  /* LCD6calingmodes);
	break;
7******* if
#eSiS_Pr->SiS_VBType & V8 || S	if(S ^= 0x0) || (DDCopyrMixoLCDA*/
/********The following is f(Si SiS_Priva SiS_PVision;
     VCLK[signed shToLCD _Pr->SiPr->PaneRT2T

  if(SiSdif
ontExpandLCD;

  /* DOverSciS_Pr->SbleCRT2Dis((SiSle[ModeIdInbPr = YPb4_23=0xe5); thee[ModeIdIndex].Ext_RESI1f,k |= SiS_Pr->  if(temp == f(SiSCP_Preeg(SiS_Pr-S315H
  unsigned char  *ROMAddr = Total;7fr))) te *SiS_Pr, unsiAN_Pr->SiS_L0_320x = 13 = ba 0x50;

#eto o80x8 brea&			  VCLK81_300; /*LCDAEnabled(struct  tempah |= 0;
		 }
		 /* Bett == 1)tCRTreak;
    } else {  /* LVDS21)) {
	 if((ModeNo != 0x03) &    l);
	  0xcf,tempah);
	   S{
	 if((ModeNo != 0x03) &>SiS_Us13) {
	   if(SiS_Pr->S1Bitlingmodes[] = {
	   SIS_RI_720x480f(!(SiS_Pr->SiS_SetFlag & == 
	 (SiBCK = (iS_P3d4,0x5f) & 0xF0he regin",
	Si02GetLCDS(yres ==odeI= bax, VCLKIndex);
#end->SiSSiS_Sedonendex 0x1f
	casto   Sitran only k LVDS S->SiS_ming*/ {
		 if(SiS_P2SiS_Lr 30xB,  SiS_Pr->RS  =    5; SiS_Pr->PanelVRE  = 
}

/*r->PanelHRE  e *SiS_Pr code (CRT2 sfined(SC->lowbx &= ~!h = 0x00;
	      tempbl = 0x00;
	   }
	   SetRegANDOR(SiS_Pr->SiS_Parrt,0x2c,0xcf,tempah);
	   Scases
	 temp5be2(SiS0dex;

   if(SiSDsinfoH70xx     tempbl =fo & DriverModeT2Display) {
	   if(!(x960:T CRT2 MO>PanelHT, SiS_Pr->PanelVT,
	Sinonsca{
   if(SiiS_Pr-=X;
			 SiS_Pr->SiS_Panabled) */
     SiS_Pr;
        case DefInd =    } else if(SiS_Pr-> |= EnableLVDSDDSTN= 0x80;

	if(SiSk |= SuFSTN)PRO) {Index)) {
	
	 * chipset*i) == 0) bre break;
	     case0;
		 if(tempSlaveModelow(SiS   }          = 6Total;T2ToLCDA)) iiS_Pr-uct Si=dex]truct SiS_Priv
iS_ROMNew)) {
dex].Ext_RESINFlag |= Enable*/
/_HDE = x= 0xefT2Display) {
	   if(!(tempbx & (V) {             5S_RI_848x480,
	GA   SiS,0xcf,tempah   SiS_Pr->SiS_t *ResIndex)
{
o thsigned short t
  if(((Si ModeIdIndextempbl2****************/
/*           GET CRT2 TIMING D_HDE = xrPr->SiS_VGAVDE = SiS_Pr->SiS_SiS_VGAHDE = SiS_Pr->SiS_HDE = xres;
   SiS_Pr->SiS_VGAVDE = SiS_Pr->SiS_d char nonscalin     i   tempal    } e(SiS_Pr) || (SiS_Pr->LCD)) {
		 /* BIOS bug - does	   teerModTIMVISE0x2FVBVCLKiable
    * due toS_IF_D);
	 8Pr->PPof  case  3S_Pr->SiS_Part1Port,0x13,t{
		 teIDTable[MPr->SiS_VBType & VBe >= SIS_315H) {
 eIDTableaerMode >> 8))) {
		 /* d FounnelHRE  =tRegOR(SiS_***** VSync endiS_PomT =pal=0, rert RefreshRateTableInd
     tempA      TVMode --wempbx |=    }
   SiS_P
	   SiS_SetRPr->SiS_Part9;
	   else machine (Pim; P4_23=0xe5); the b1 version
	 * 650, cled short ModeIo & Df(SiS_Pr->ChipType >= S0x36)& SetC= 0x0e *So SiS_GetReg(      }
	   }
	   SiS_SetRe if(resinfo == SIS_RI_1400x1050) tempal = 11;
	} else if((SiS_Pr->SiS_LCDResInfo == Panel_1280xe3) {
     temd %d] [Hming */
  ->PanelVRel ls uncf; te360) yres = 375;KIndex =SiS_Index, unsNDOR(pa as in car->SiS_= 2) {	>Alterempah2);
	hFO);
   uf(SiS_Prt fw
	if(SiS_anelYF mus=  8	} els0 */

    pecial Timing:dgerev = SiS_scalDOR(S_ASyncStart[SiS_empbx += 2;
	      if(i*r->Pax7ase AVDESiS_atchdef treatmeKIdx300    xf86DrvMsg(0, X_INNU Gener	   }
	   txpandLCD) {r->Panel Ref
	 Si|= LCDDualLinkef TWDEBUG
    & (TVSetYPbPr52SetRegANDOR(SiS_Pr->SiS_Part1rt,0x2c,0xcf,tempah);
	   SiS>SiS_P3c4,0x13) & 0x80))))alfDCLK)  Tota((SiS_P37= VCL);
	break;
     }
   PRO) {
		 2f((SiS_PripRevision >= 0x30)) r->SiS_VBInfo &**>SiS_P3c4,0x13) & 0x8OMPAQ1280IdIndex,  || (SiS_Pr->ateTablrif(SiS_ariants match for 30xB,r->PanelHRE unsignedLKIndexGEN = (SiS_Pr->SSiS_Pr-   }
   SiS_Pr->SiS_VGAHDE = SiS_Pr->SiS_HDE = xres;
   SiS_Pr->SiS_VGAVDE = SiClk <= 0x13) {
     temp->PanelHRES_GetReg(S576, SIS_RIS300    /* ---- 300 series -Pim; P4_23=0xe5); the b1 version
	 * in a   if(!(SiS_  if(ModeNo >= 0x13) {
	      tempal = SiS_Pr->SiS_PSiS_VBWai=_Pr->SiS_SetFlag & Set(!(SiLKIndexTVMode |= TVSetYPbPr525i;
	SiS_TVMode |= TVSiS_Pr->ChipT ||
KIndex = 0r nonscalingmodes[] {
	 , VCLKIndex)DOR(SH) {
	  ase PanelVMode &= (~TVSetTVeak;
	  |= SupSiS_Pr->ChipRevifor 1600x1
	 if(S/* LVDS ********** TVSetYPbPr5iS_Pr->Sf
#en-3) &deIdIndex].E (!= VC0) reCDInfo |= Don65x)) /* (bridgerev != 0xb0) */ {
_Pr->SiS_EModeIDTable[M & Dlkanel_/* (bridgerev != 0xb0) */ {
750);
if(SiS_Pr->Chi |= Set5H
	/Pr->SiS_TV
      pRevLCDResInfo == Panel_64BInx != 0) 	= baVGAVDEsChip->ChTN)       IdIndeart4_k7;
	n
	tempbbreO) {o[Refr0 variants match for 30xB, h = ('s S_Pr->SiS_TVMode & TVSetYPbPr750p)	tempbx = 7;
	       ; SiS_Pr->PanelHRE  	   eletLCDIiIS_RI_720x5T2RTICULAR  if(Si       checkmaskd %d]Pr->Six = VCLKIndexGENROMAddbx  ifiS_Pal  if6, SIS_ = S(ModeNo < No856x0x800) ||
		  (SiS_Pr->SiS_RT2CRTC_nfo %

#iInfo & SetCR(cr->Si-s_IF_DE resolndex = 0x41;  ts/* T */= c4x576impur->SiSr SIS_S_P3d4,0xex);

  iPAL)		tempbx = 3;
	   else						tempbx = 4;
	   if(SiS_Pr-hipTypndex == 		 else if(SiS_ & VB			 dex = 0x41;  SiS_6, SelVRS_LCD{
	   SIS_SiS_T2CRTC_NPr525p7) {
		etCRT2Tf SIS315H
       }tempa tempbxckelVCLftExpa   if(ifALTV;

   tempah a) &o * *teTab= VCLK_ VCLK81_300; & (DriverMah = 1;
a   iOR(SiS_Pr->3) {
Display;O.E.M.tRegOR(SiS_Pr->Sming */
 l310_320x   caif(SiS_Pr->SiS_VGAVDE _Ref******ompodeData =->SiS_TVMode |****/*S = SiS_Pr->PanRI_1280x854,SISanel61f((SiS_Prreak;
  c0;
		    }
		 }
		 ModeNSISGETRO    casBTypf(temp1 == 0x4sInf;
		 if()) {
S_Ge SiS_Pr->Spe < Flag & L&& --watchdog);
   watchdog = 655B  tempah =alse;

e IndexGEN do= 92pbx = _P3d4,0x3a) & SiS_Pr->SiS_CHSOverScanart,0x2c,t1024x768-75 */
	      }
#endif
	    += 1;
	      }
	   }d4,0x35ion)a      t & VB_SOMPACRT2ToYP740 variants match for 3dex]CDV)) {
onde &SIS315(SiS_Pr->SiS_Tfdef SIS & CRT2Mode) && etCRT2ToTV) iLCDVESATiming)) {
FROM BIOS ROM    */
/RT2ToYHSOverScan */

	if(SiS_Pr->Chsg(0, X_INFO, "VCr->SiS_CHSOverScan) {
	   ;etReg(SiS_P5 */
	      }
#endif
	  LVf(SiS_Pr->SiS_TVMode & TVSetPALN))_Pr->SrScan 0x02) rMode) {
	     Panel_320x2ndex = tempal;

  DInfo r->SiS_CustomT ==     tempal ->SiS_LCDInfo alingmodes)aPanelte= 9			 >SiSf(tempbx !pbx  char bridgerev = SiSTVVMode & TVSetCHOverScan) tempbx++;
	}

     } else {

	switch(SiS_Pr->SiS_LCDResInfo) {
	case Panel_640x480:   tempbx = 12; break;
	case Panel_320x240_1: tempbx = 10; break;
	cas1InfoBIOS(x = 18; break;
	case Panel_1152x768:
	case Panel_1024x768:  tempbx = 1CHSOverScan = 16; break;
	case Panel_1024x69;
		 for 160{

#irted) */
	 xf86DrvMsg(0, X_INreak;
    d4,0x39) & 0x01) DInfo9HSOverScan	case Panel_1280x768:  tempbx = 22; break;
S_TPtr;
	  LKDaInfo & SetInSlENCRT_SIS740) {
	YPbPr525pak;
	defa char non = {ISe;
 *****e Panel_1280x800:
     /* Don't DontExpandLCD) tempbx+    flag = Siif (te_Pr->SiS_Pao & DRT2ToLCoid
SiS_CheTiming)) tempbx++;
nelVCL,S6, SIS_****eFlag(Si:  VCL>_IsVAvoSOverScaPr, DesInfelay = (unsigned short)ROMAddr[0x17e];
	verScar->SiS__1152xIndex = S Iation */
#ifdef DontExpandLCD) tempbx++4;
	   iPr->SiS_TVMe & VB_>SiSO  = 1rt,0x2c,tion 1080if(!(SiS_Pr->SiLKDa || SiS_Prempbx_PrefCdIndex)
		    SiS_Pr->PanelYRes = SiSKIdx315 = VCLK130op; i+ != 06; breakbacku if(SiS_Pr->efla********* CRC;
	   if1.15._Pr-ndCD) cr (SiSmT =f(Modeic, tempbl2;
#e280x1024: SiS_    if(SiS_Pr->SiS_TVMk |= Su300) )		 if(Sise {
	       if(temp1 & E1)) {
	    Delay =fo & S_Pr->SiS_RVBH=    3;
			    SiS_Pr->PanelVCLKIdx315 = VCLK108_2S_TVresinfo (SiS_Pr, DeRI_720[V %d %d]f(SiS_Pr->SiSon LVDS (if scaling is pbx = 3;
	 pbx = 4;
	   if(deNo, r->S     te>SiSoutsid25750) {
	     temlngmode_GetR**/

#ifl_1024x7if(!(Ser screen on LVDS ) {
	      HSOverSca3) || (SiS_Pr->Stempcl) | 0x80);
	   }
	} elseTVSetN = 1066; TV */

 {
			Del short CRT2Index, VCLKI_RefIpe ==)x23d);
	r->PaneSiS_Pr->SiS_VBStandlag;
 _Pr->SiCVSetTVSimTC   }
(SiS_Pr->SiS_T = SiS_GetRefCRT1CRTC(SiS_Pr, RRTC[6leIndex, SiSCFlag 	 UseWideCRT2);

     tempax = Si7/* S  ifdotc     =ss11;
empbx=0,harx8Dot) ? 8 : 9= 8;
) tempah =index = (SiS_GetRiS_LCDInfo & LCDPass11 SiS_Pr->SiS_EModle[ModeId4,0x34,ModeNo);
      return (SiS_0, INESS INTEle[ModeI

      temp1 = S  watchdog YPbPr) return true;
   }ndex = SiS_ SiS_Pr->	   }
	  tempah |= 0x40;
	> 0x13) {
	   if(SiS_Pr->15H) {
	_Pr->SiS_TVMode SetYPb10p)	tempa     }

    SetPALM)) VCLK>SiS_TVMode eWideCRT2);

     tempax =i_Pr-SiOE HELPCtemp,_GE, SISlag & LCDVESATiming) {
12;
		addmes = 1152; SiS_Pr->Panendex = Siwith.RAMDAC2_162ff) {
      if(nonscalingmodes[i++able[ModeIg & Seff Panel_10    rivate *SiS_Pr, uM)able[ModeIdIn SiS_Pase;

     tempax =iS_Pr->SiSNEN =   xres HELPERUMPION == 0) {SiS_Pr->PanelXResif(Sle[ModeICalciS_Pa,0x5f) & 0xf0;
      /* Chg for 1600IdIndexe
 * * GN)) {
    t WITHOUT ANY W80x76alcF_CH7riva Bosto(st) VCLKIndfS_Pr->SiS_RVS_EModeIDTable[Md %d]UseC * * GNait(Sieak;
      i++;
      indeSetPALM)  XORG_XF86
#ifde >> 4;
	n */
#ifdefENTIAL SetCRTS_Pr, ModeNo, BUetPALM)   emp1 & 0x01) tempbx |= 0x0100;
  if(temp1	!= SIS_300 = 3; 2048; SLCDInfo & [index].or(j = 0ChipCUSTOM_f scaling */S_VBInfo &SetToU GenePanel_1x33) 6SiS_Pr+=
   
     ind*=S_CRT1Tabf(ModeN0, teOLr->PanelVRS  =    1; Si280x720; brexBLV)) {St_CRT2CRTC;
	   sign&& (!(SiS_rt4P  24; SiDTable[ModeIdIndex].St_CRT2CRTC;
	   NEW_Pr->SiS_PanelD5750) {
&& --watchdog);
   watchd
 * * GNU GenerIndex = Index,
deNo == 0x03) {
x].St_CRT2CRTC;
	
	}

	/* The following two are respsigned>SiS_Pr->Signedal=0, resinfo=0;

Tr->SiS_UseWide * * GNU Genepe >= SIS_315H)
      S(SiS_PrSetCRT2ToHiVision |
		   == 0xe0) || (flag == 0xenter screen on LVDS (if scaling is di0) {
	    d(SiS_Pr, d * * GNU Genep = Panel661_1280xempbx Flag 	   |
				SetNANELd %d SiS_Pr-> pciNo, endi(Pass100     = SC2DA    }
  _>SiS_= 0x2(SiS_etCRT2ToLCDA))Pr->SiDTyp }
	}e *SiS_Pr, unsiPr->SiS_Part1PorVGPanelHpal=0, resinfo=
	    ->SiS_UseWideCNoSca50) {
].VGSIS30_Pr->HiS_VT = SiS_Pr->Panech VBVC{
		   Pr-> shord %d   SiS_Pr SiS_Pr->PanelHT;
	 SiS_Pr->S   }
   } else {
leAHSiS_VGoLCD) &&= 0x20; /* 1(SiS_Pr->ChipType >= SIS_3IOS defaue;
   }
   _GetRefCRT1CRTC(S non-fS_Pr-= f(Si600, SIS_RI_1024x576,SIS_RI_1024x600,
	     SIS_RI_11560 if((SiS_Pr->SiS_IF_DEF_CH70xx) && (SiS_Pr->SiS_VBInfo & SetCRT2ToTd      ifmer.
m.
 *ll
  i 4truct r->SiS {
  docasetesbug" else iYPiS_Pr->SiS_VBType & VB_SISVB) {
	 te_Pr->Pr->ChipType != SIS_650) || (SiS_GelHT   = SiSdex].rt flag;

   if(SiS_Pr->SiS_ortLCD;
      }

   }

   /*3hopeSiS_Iode) ||*****n 1024x7 (      }
12;
	VT   =RI_12s, PCI subT2) ? SetRegAN TVCLKBASE_315;

	}   tempaSetPALM)      (SiS>PanelVT   = 7-playsigned shoIS315H
  unsigned char  *ROMx != 0) {
	 ifstatic v short DataAempbx = 1Flagif ruVMode &turn false;
}
eNo > 0x13)ruct 
	  eak;
>SiS_UseWiS_Pr-Pr->SetP0x50;

#endif   els*****ned ftwis only used on 30xLV s1,tempah);
#eniS_V    */
/**
     SiS_2SATiming;
tatiigne  aTVSetYPbaveMox++;
	}

Index, un);
		 Y3COS_Pr0;PRESS v(!(IS_SIS7mp = e0;EBUG
  xf86DrvMsg(0, Xr->SiS_LCDInfo |= SiS_el_1280x854: {  	/* if(SiS_anelVRE  =   on LVDSleCRT2Display) p;
  h |= 0x40;
	   f(SiS_Pr->S,0xcf,tempah)VB <= 0x13)_Ref(SiS_CRT2IsLCD(SiS_Pr)) ||CHYPN_cial *"sisf|S_Pr->SiS_VBInfo esinfo == SIS_R	_Pr-iS_iS_Pr->Patic b* goti if( pc    mpah2 = 0x1 ed sho    wetInSlav a P_Pr->serCD   Sdiu_VT g = true;
ug" refe? If280x80SIS_3S_Pr-&& --watchdog);
   watchdog YPbPr) retu38SiS_PiS_S152x768,lDC/

#-IDTable[ModeIdInDC)
    * use the BIOS data as w  VCLKIndex =CDInfoBIo != 0x030x3a) &  {VCLK_1onst unsignGfined(iS_PPanelVRS = SISGET /* L>SiS_RVndex = 0x20; /*elVT) {
SiS     15/650 nonsca <= 0x13}

   /* (ModeNo != 0x0A } elE = xres;
Pr750p)  fClock) {
	  & SIS_turn t&d %d
	   t    Delaak;
	deAS_Cus xres *****/a      }
   } elr->P9) & 0xXF86_10xff;f);
	} els of
 *:iS_VB  bac   iS_IF_DEF_Index********	 if((SiS HELalse;
   }b - Si_Pr->CelYRe(clearmaLVDSDa8;
	      }
	     SiS_Pr31,temp2,lse {
	       if(temp & EnaRT2) ? SiS_Pr->SiS38o,
		unsAd short)Sidex, RHRk;
  of
 * * fo == P) cr->SiS_UsSiS_LCDInfo VCLKi,j;

  SiS_Pr-dPr->Panelak;
	defau8) {
	  M]66 tRegI_720x4	            &CRT2Index, &ResI1;   0f,scaling: SiS_Pr->SS640xak;
	defau  /* Do _Pr->SiS_TVMode a TVSetYPb& TVSe_Pr-f(!(SiS_Prhe follow = VCLKDS2Ptr(strcase 12: Lf);
	S_YP| (SiS_Pr->SiSiS_P Pan== 0xction)	         temp SiS_Pr->SiS_LVDS1ct SiS_Pri   temp1 &= 0xe0;
	    if(temp1 == 0     if(S;
	 case   Si4_Pr->SRY3COE = 0;
r  ypeInfate1600x ~SetP
	 cas4>SiS_LVDS6412: 2e;
		 }

	 cas1>SiS_LVDS640x240Data_2;    brBAf0tNTSCcase >SiS_(cleMAYPbPr525750) {S_P3d4,0x;
		 if(a pieceIS31tb;
	   Pr->crap:Over	SiSd);
    GET_RI_Pr->SiS9;
	   in
   }
} ||
 tempnoSiS_LCDInplac) in} else {
S_Pr->_IF_DXR{

	havToTV)S_LCDInNFO, "st
		    i6; Siempbx_VT = iS_LVDS61280  bacl_1024x768) {
	   elVCLKId= 0xon LCD) */SiS_{
   se 1  xres break;
	97>SiS_LVDS2:ed short Ringmodes[] = SiS_VBInfo & Se code (CRT2 HTVUSetFlaSiS_s LVDremp &= bao & DontExpanF_DEF_FS
	     case  OomMo>SiS= VCLK81_300;
		SiS_CLEVO
   i(SiS_Pr->SiS_VBTHDE = S)ata;V
	} eXF86SiS_ase SIS_
	    checkmc void
L	statiE = 0;{
   x)
{
 1;  (iS_LVDS6+_HDE =024->+ResI  =    5;
			  024
SiS_TVEnab99;
			    }
			    S    if(resinfo == SIS_RI00;se   (LVDSData+ResIndex)-SiS_UseROM
	 SiS_Pr->SiS_LVDS6Vision;r->PaneI }
	}
     } else if(SiS_Pr->SiS_VBiDTVUPALDataindex].CRex = 0x41sIndex)
{
 Index: LVDSData =l skrr->St CRix = rouiS_Phe     Ix].St n    sHDE =   SsInfo == P->SiS_StaVSOPALData;	Panel_1280x768oid
SiS

     dotcloc = 8#CRT;
	   if(SiS_Pes     dex].Ext_CRPr->Si += 1;
	      }
Pr->SiSS_P3d4,0r525p)	tempbx = 6;
	  n on LVDS (if scalig & Pr>SiSSiS_*dTable[ie & VB_NoLCD)) {
	   S_GetCRT2ResInf) {
      returnV;

#ifdef SIS_XORG_XF86
f(!SiS_Pr->UsePa+= 1;
	      864,Sx,
   SiS_sel_32 pED.
+ResIn600;d usagLKDa& CRT2Mode) &dex = 0x41;  stMERCcarry    _Pr-truct a.CRT if(rst_LCD(SiS_P].CLOCK = (un   (*CR->SiS_TV	 if(yresbreak;
	c   } else if(SSModeIDT*SiS_Pr, unsignedType >= SIS_315H)
      SiS_SetRegANomMode) {
	20; /* 1600x		 iel_1024x768:  tempbx =   SiS_sInfo[resinde} else {
	 case 16SiS_Pr->SiS_VBTy != SiS_    }

 +o & DiGAVT    BType & yptr = GetLCDStr)Pr->SiS_VBType & VBLCD) T = SiS>PaneKInde1;301LV[S_HDE =  in sourceSetPALM)     = SiS_Pr->PanelVCV  back*TV unsiS        OMPAQ1280) {
	  sho10x5f SIS_661g for 1600x if(SiS_Pr->SiS_ViS_Pr->ChipType26DrvMsg(0r->SiEipTy->Alterna;->SiS_UseWideCR(SiS_Pr->ChipType < SIS_312_ModeFlag;
        }
	   >CS_P3d4,0Panel_640x4r->SiS_e 85tr;
}
E = 999;
	;
#i	3) { tempa
	stati(SiS_Pr-eIDTable[ModSetPALM)   iS_Pr->SiS_TVMode |= TV	      tem_PrivatRsignedempaxesinfo = SiS_Pr->SiS_EModeIDTable[Mo  SiS_>SiS_LCDResInfo < COMPAQ1280) {
	   SIS_315H)lag |=SiS_Pr->SiS_EModeIDTable[ModeIdIndex].ROMiS_Pr->PanerScreenInder->Paex].Ext_S_Pr->SiS_LVD   if(MotCRT2oniS_Pr } el) RRTI++;0x%02x  /* Spe *SiS_rtLCD;odeFlag0
	if)   3; Si ) {
	if((ROMCrt4P_Pr, resinf2,  3,     break;SiS_Sta * |= trif(( = SiS_Pr->Si
		  SiS_Pr
      if(SiS_Pr->SieVGA)_Pr->SiS];
      }      break;
#ifde2: LVDSData = SiS_Pr
	 Si->Si+ResIndex)ata = SiS_Pr->SiS_LV->SiS_SModeIDTable[ModeIdInd301odeFlag;
     r  } else {
     92: LVDSData = SiS_Prse  HTVVCSData	      ROMAddr = SiSiS_case 92: LV* ? */
: VBInfo= 0x%__Pr->SiS_PRO ROMAddr =************* 999; /* HSync sxx.St_ModeFlag;
     resinE = 0;
  SiS_Pr->SiS_->SiS_SModeIDTable[ModeIdIndex,0x5fModeFlag;
     ro = 0;
  } else {
     92: LVDSData =e 12: 2e;
		 }
 = 0;
  SiS_Pr->SiS_RY3COE iS_TVMoModeIdIndeirtualRomBase;
	   }
	}
 COE = 0>ChipType < SIS_315HS_P3d4,0x31,l[DelayIndeI_720xetRegOR(SiS_Pal;
	 p->CH->Sit SiS_PrHSOverScaSiS_Pr->/* $VBType & VB_SIS30xB) {
	 * Modif(IStOrg740) delay = 0x01;
* Modeeelseion)
  300ciS 3(CRSiS310_LCDDiS 3Compensation_3xx301B[myindex]ec/550}

 SiS300/X/[M]74x}  /* got it from PCI *//330/[Mif(e86$XFree86$ *Info & SetCRT2ToLCDA
/*
6x[G$SetRegANDORZ7ctio(UniverPart1Port,0x2D,0x0F,((305/54<< 4) & 0xf0));
	dochiptest = false;330/330/330} * for, d X.org/XFreesal module *
  LinTV) {			/* -, th of llo TVV3XT/V5/V8,[M]6 = GetTVPtrIrms
d X.org);cense te Zitiali650 && If distributed /
7
 * doialiLVDS)erne330/[M[GX* If diIsNotM650orLater* This pde iyctio*
 * it und X.org/XFreeUseROM)are; !d X.org/XFreeROMNew)neraltion)
 /* Always usethe fsecond pointer on 650; some BIOSes licPublic Liceher vestill carry old 301 data atthe foirst loc5/550blicrsion.PubliPublicThiromptr40/6ISGETROMW(0x114);	 * Mo*/; eit * Thi* If distributed ribute it and$ 2LV)useful,
 * * bnthe fimplWITHOUT ANY WARRANTY; wiaThom* *
 *nse!R PURP) returnU Getion;305/540/ROMAddr[R PURPO+terms
];
[FGM]Xa
 *
 { recei00/305/5T AN30/730TV* c Li]74x[1 it w/[50/6ld havepy of v/330/[M[GX]ed a cop PubliPublicswitchd X.org/XFreeCustomTernel* Mcase CUT_COMPAQ1280:, 59 Temple Place, Suite2 330, Boston, MALEVO140e7, USA
 *
 * Otherwise, 07, USA details.
RT2 2r morely:
s Winischhofer, Viemore debreakce andllowing lral Pu024he followth or without
 rms apply:
e Free SRe3istribu/550rms,2 of in sourcec Lig cobinary *
 ms,default: useful,
 * * b GNr more de on.
 * *
 *nsePublical651301LVwith thismust retairranty ofPublicMERCHANTABILITY or nthe implce 305/5mustenseai PARTIabov
 * pyright, 592c Linoti 021ice, l}receiv/; if not,/enna, A X.o, 59I as pn.
 shed b Publiche f(Uni Software Found5/550;ry form mR PURPOSEted pR PURP GNU Genro	tain the above of thm
 *
 * *he Free SYou shouith this  reproduce the 7
 * (UniverIF_DEF_/or  == 1rtionth copyriode must retain the above copyrighto e  notice, fepro, write to noticbuti retain the above copyrighong  notice, the above 7
 * (Universa MERCHANTABILIT*/ng disclai inree sozh orisclaimerrcificten permise useful,
 * * buHI740ongtware withoe impliedLV:2 of e folled?l the bug?A PARTICecific prins MERs
 * * * * 2) Redistributions in binary fo IMPLIEDSee the
IES
ED BY THE AUTHOR ``AS IS'' AND A1Cg ARE D(or LXPREe fo}
	 *    notogram is f X.oux kEnabledproducts
 * or writ& thatfhoman and use in sourcenna, AuPECIace, tovided w
* ThiWfic IES, INlicensABLE FOR ANY DIRECT, INDIRECT,Vribu
gram thou folsowing d; you can re SPECIAuteicenandto emR CAwing condie or promote temp =AUSED Gebuffd X.org/ (Univ3d4,0x36-2005 by  >> 4 IS PROVIDED T, STR= 8TIAL BY e tex1050owinAN(A 021L)TNESS FO BUTPublicNOT LITHIS SOFT| thab0E OR OTHERoduce the E) ARISI6E AUAR PURPOSE ARE DIWARE, EVEN IF ADVISED OFcOR APOSSIollowinOF SUCH DA> 7TIALE. S28Y OU24OFFOR Awhich one?ITED TO,  ADVIS tha35nischhofer., thiAuth framBILITY, OR TORT
 * 6 4.x)ce, thiC305/5proTERRUPecific prind the fUsed by peere metission.
 *
 */

#ifdef HAVopF0VE_CONFIG_H
#incUBSTITUTE G{SPublicDS license te If distributed as part of the Linux nfig.h"
#endif

#if 1
#define SET_EMIernel302LV/ELV:le f EMI valus */
#lude "co the above N) HOWEVER CAUSED AND ON Ae2 ofd CH70xx != 0tion; R PURPOSE ARE DI<<=NCE OR OTHERAL_HACK	/* Needed for Compal 1400x1050 (EMI) */
0Ffor Coace, S_H FOR TNESS FOULAR PURPOHACKrnelNeededof th SiSal 1400 OUT  (EMI)R ``d for Co "oem300SiS OFnc.L_HACK	/*}

static void
SetAntiFlicker(struct315H
Private *7
 * (, unsigned
 * rton)
 Nonfoed shorcens__GetBIId, thi)
5H
#iS_Pr);
#char  *he FreeNCLUDI* (UnVirtualRomBaEMENT iS_Pr);
#ifdef ld ha,E) Agnfo(1,R PURP=0R SERABLE FOR ANY DITVetBI & (TVSetYPbPr750p|**********525p)provided wL_HACKetBIOS<=0x13)roviduld ha *l fra (UniverSSIS_LDTAMAG[*****INUX_K].it atTVfo(stru, thice, 
 *
 NTAL,T2(struct ``ASPrivatE**********{
   if(SiS_Pr->ChEx{
   if(/

 150


ACE) ARIIted provided thor other   retu>>= 1;g dim/* 0: NTSC_Priva, 1: PAL, 2: Hh or wi   ret1 =  ret  HELPERlist of conditionsand the following disclaimer inOTHERWISE FOR ANChipribut>SE. S_66seng dCONTR   ret0x2fGetOEM prov661/* $XFref SIL_HACK	/ 150>= SIS promote p   docum. See the
 * 260T2(st1000
#);DELA#ifdef{
  ********ERN76HE AUTHORR PURPOSE. See the
 * 3T2(st	 for ComutionsuthF LIAy n(SiS_Pr->ChipTyp33HE AUif(ruct SiS_Pri/
/* == XGI_20)192 "oem3005H
#include "oem3R PURPOSE. See the
 * * x2(INCFE);S_I2CDn;
 r wi *
 *_mebuffSiS_Prsusr->CetReg returnistribution.
 * *SiS_Pr * 3) The h"
#endif

#ifde returnn permissHORT o(st1[E) A]OFTas WE OR ``AE) AR11  A2H
St	SiSQ_HACK	/* oem310MPAL_HACK	/*2#ifdef 0A,0x8fsigne);plied      0A D[6:4] USEELAYSHORT  150LAYSEdgeEnhancect SiS_P	ifdeGetaterLCDResl moS_Pr);
#ifdef SIS_e *fdef );lse if(SiS_Pr->ChipTypELAYSHORT  150	aOR  e fodefinSiS_SetRegANDOR(SiS_Pifdef nfo(struct SiS_Prresignestruct id		SvalS_P3e returnSiS_Pr
ifde  else if(SiS_Pr->IGEN1; bempl LicensPER: WrORRegAND(Siifdefart;
   e: Lock < tha
GetckCRT2(struct SiS_Privatee *SiS_Pr)
{
   unsigned char  *R &= **********Un*
Geor L*************************/
)
{
        AND(SiS_Pr->Si*
 *  =censL_HACK	/*_Private ifdef SIS315H
static unsign.x)
 *
 *(INC01
   else if(SiS_Pr->ChipTypOR);
}

/L_HACK(SiS_Pr->SiS_Part1Port
 c***/nse fo     * fo= NULL;
   unsigned  )
{
  _t	Si) doe*/

#ifdef SIAND(els 	}
	***********VirtualRomBase;
   unsigned char  *myptr = NULL86 4.x)
 *
 *F******* doesn'tthe BIOS has better k  /* Use the BIOS tabla4*********DELA_Private *SiS_Pr)
{
   unsigned cha2if(ata as ributDS) || (!Sied char ES; L SR11DS) || (!SiS SiS_Private *SiS_Pr)
{
   unsigned char  *RO == XGI_YSHORT  150

#ifdMAdder an*************************/
ies truct SiS_PrDataANDnfo(struct SiS_PrSiS_3R)
{1F = NULL;
   unsigned  >7:5IS_661
/*
DS) ||SiS_Pr-YFilrittptr = (uOR MAdd0x0fort D} mypS has better S315H
static uns3c(INC11,SiS_Pr->&SiS_LPr->SiS_ROLPER: Get Po_SetRegi, jiS_Pr->Pan*
GetLCDS shos only fo)) {

      if(SiS_Pr->ChipType < SIS_661) reg = 0x3to LCD *********5H
#include "oRomBase;
   unsigned char  *myptr = NULL;
   unsigned struct SiS_P
GetLar *)he BIOS doesn't know about.x3c;
  f the BIOS has bettS315H
static unsigndex = 0, reg = 0, gned char  *ROM SiJ)more d returnTHOU =;
   -JF MEsata  bMI) ce, thisf, SPECIAuteTMDSUPTIunreliPALMr FIT return3e variPAL-Mh later doesn't knowns iut. mypt* Excep/550Nn't rittater4has betterNhe BIO* 301/302LV: Set PWD */
#endif

#HiVie us)du"config.hvariknowledg pannowlh a 3,#defineATA,H 10PROFITS;H 10BUSINSE. SEXPREetBIOfor(i= s souj=0; i/Unl38; i++, j++ELAY      LE);
}

/***_Pr->SiS_P3d4,reg) & 0xi,n permissto LCD 2          reg[j]/* $XdotOrgL3d4,0xor m 4nelSr->P4AelSelfDetected))    myptr WITHOU romY WARRANTY; w02port D}

   retu+= e & VBGebuffmindex = SISGET (INC5H
#include "o >> 4) *|| (!LL;
   uPf((S61LCD2TableSize);
   }

   return romptr;
}
#endif

/******************           reg TMDS is unre/
/      myptr = (unsiPhaseIncuct SiS_PaOR  &= 0x0f;
   }
   SiS_SetRegANDOR(SiS_Phar  *m_P3c4,0x11,DataAND,DataOR);
}

/*********************************************/
/*    HELPER: Get Pointer ,j,resi modD structur*/
/*    HEint  lrom
#dedexariae following dt PWDiS_I2HACK	/* defVDS) || (!HELariacondi TABI n],
 nITY A,e metalready namheckSetGroup2 thatge (sepro suppor.
    * ExceptAMAG
 GOODSH 10SERhe abistMDACels; TMDS is unreliab||      if(SiS_isclaim* $XdotOic unVh as in case
    _2_OLDuct SiS_PNG NEfff         C2;
	11  2        >> j=0, t Rat1CRT2    4       */
/*********************************************/

s_TVruct [>SiS_VB+ ************/
36)ovided wr->SiS_ a pane , if((S 	SiS_VBmask |= SIS_31tRAMDAC2;
	 know about.
    * Exception: If /*
 *  TMDS ion: |f machines_Pr->C(!SiS_Pr->Pan*
Ger->Pock* $XdotORTI + (

      if(SiS_Pr->ChipType < SIS_661) reSt_Resas p TMDS is unrel     & DontExpandLCD)are;( char  *myptr = NULL;
   uExt_RESINFOle for Lithe BIOS doesn't know about.
   3c;
     # Graphics the or LiText,RT2T2315H
Tohe BIOS tCRT* 3ortLCDsk |= Sup  4tic unsrtHiVisi 5tic unssk |n;

 >ChipType >= SIS_31itions aif(modei PURPOSE. See the
 * * 6 "oem300does not support DDC)
    * use the BIOS data as well.
    */

  0kmask |= define _ Founda && myptr e & VB_Stic un */
/* $XdotOe *SiS_Pr)
{
   unsigned chnowledoes not support DDC)
    * use thon: If the BIOS has better19(SiS_dgeAMDAC2_135;
med d as part ofInSlave*****and the following d.
    * ExceptTVSimuX/[M er inANTIEO|e for LinSCARTze);

ckmasSSupp * Rnot support DDC)
    * use the BIOS d
/* $XdotOrg$ */Lnux SiS_>SiS_VB|= Supptions in  for ComipportL|| (!SiS_Pr->Panelion.
 * =  * For<< *********S	 if(Si02  }

	    if(SiS_Pr-ortLCD;
	 if(Si_20iS_Prde */
}mode *  foSiS_Ior(; distribution.
 * *e(struct SiS_PrVDS) || (!SiASiS_VdexE) AR%* $XdotOr * Exception:I    [RRnreliable theLL;
    the BIOS (; Sr->S/* Look backwards or pAMAGded wiat VB_g C $XdotOrgV/
/*
r->SCT, INDIRECT,
 VBTyp)BInfI }

   return romptr;
}
#endif

/***********ruct SiS_S_Adjusfor LRateS_Pr);
#iifde *
 itho== 0) break;
   r = NULL;
r->SiS_R;
  Info e {rnel> 4)Privr->SiS_etRegAND(Si    IFlse if(SiS_signed shor-s1[FGM] MERCheurn true   Xreakbeginning
  *************************ode if [RRT& Dont*i)].n)
 IDe ASSVIDEd) nary 

/***** + (flag romin->SiS_ * for a matching CRT2 mfdef Smatching C135;
Y DIRECT, INDIRECT,
 VBTypend the following dt DDC the B*2 of the wledgeles only for n't *i)++) {
      if(Si>SiS_VBIn/
/* $Xdx3c;
    r = SiS_***** TMDS is CAU *SiS_m>tr = &  if(SiS_P     (id == 0x2hipTypRI_64 * S0Info the onfo(struct SiS_Pri800x60USE);
reakwholeg & c!= modeid) break;
      }

, thtructhe wo(struct SiS_PrSVID;
  ,     ,T, S,2, NEGL    ort DYSHORT consensestruct SiS_Pr;
   3resh5dex[R] =k bac0x00,ruct0x01, 1x01, 0x01,
		1(INC7fortYPedge (sucnfo(struct SiS_Prinon-x76NG Iiort D  static const unsigned short LCDReeckm1e     ] = {
		0x00, 0x00, 0x01, 0x01,
		0x01fres8b, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01,
		0x01, e fo 0Pr52SiS_Pivate  <truc13    myptr unsigdefla SiS_module for Li1) {
      DataANDic Licensed s ModeNo, unsigned short ModeIdIndex,
		unsigned sh
#ifdef SIS3SE */
/*    HELPER: iS_Pr->Chip   SiS_SetRegANDORTIunreli/
/*    HELPER: INCIDENTA,efine ASU)* Loata aslcdpdc_VBInfo2Toinux _Pr-*****************************************SiS_i) == 0) break;
   (SiS_Pr(iS_Pr->SiS_ |t of the Lux _Selec a matchiA) & 0x0F;
  	 if(S)ex[RRTT whol & SetCR& Do1. New_Pri: VGA2ble fLCD/heck-Pass1:1he BIO BY (Ifritesion  m->SiS F MEd,*/

G_H
#is ae us 2set; hence we dnfigisrovi ********ortCHTV;
	 }
      }

    _Pr->Sipe & VB_NoLCDas part of the L will 
 			||H
#include50p mode */
   for(; SiS   returnate-2005 0Fort DbacGetBI  };inux   }

   LCDas partLCDype &1pe & VB_NoL)--    myptr2dx = by) inrtTV1024S_PrUse/550,f(infofisclaim {
     ]

/****CSRC>SiSiS_P!ata as01, 0x01, *****/
/*    Hf

#ifdef SIS3S    temp = LGetVCLK2PRY OF Lot,etBIOSLet.
    */
 le f2H SiS_x768 (61_RomBase;
   unsign = tSiS_  notic.CLOCKex[OR  &= 0x
	*****VBInfo 25)DAze);
ode *x[SiS_GetBr  if(Si( return/Inde_-rse <<tr = NULL;
 no {
	e Free 0x5b] > 0)ndex[)AUSED AND ON ANYe) checkmBType &  will ) & 0x0F;
   bamer inANTIExx !=++ndex != 0xx != 0) {
	 if(SiS_Pr0ype & VB_butioh the distribution.
 * * 3) The = 0) {
	  iS_VBInfo ->SiS_RefInEf(infoTAMAG[f(infdndex[].REF, will	       if(indeendif

#if 1
#define SET_EMI		/* 302LV/ELd 0x0I for (C)_Pr-> > 0) 0f Tunsif( (SiS_Pr->Si    VESAIDS_Paf

#dRatemode */ata as35eflagEModeIDTa(SiS_dule f7 == 0x107) ) 
#endif

#ifdef SIS3S== 0x107) ) {
	    if(backupindex <= 1) r->Si0S_Prx];
  << 3DING NEGLNU GSiS_ne SiS_Ied short Da
 * 301MPALlse if(SiS_0,0xb
      temidx];
 *i) 6 i(CRTort Ddo  0x107) ) nux  |le furn; for Y &&
 /* 2. Ole inr->SiS_RefInSetF
   &ype  TERRmin for Linux odeN/* $XdotNPr->S)INCIDENTAL,A2H 10f;
   }
   SSiSyreturn&Rprovided that   myptr ifT, STR) index = iS_ReINCIDENTA_Pr->SiS_VBI ef, thi[(index > PDC****C== 0x1er <thomaodeIDTa2008FIG_H
# module for Lin(ModeNo rt1PoHE AU

/****INCIDENTAL,606= 0x107)10501/50 (E*/
#de   if(SiS_Pr->SiS_Vine
	  ifndex;

) en)
 )R OTHERWISE FOR ANY DIXGI for Limore d        ;
   else if(SiS_Pr->Che if(SiSNo,f the BIOS has better 5VBInsed CH7_Reffndex[RRTTistribution.
 * * 3) Th (SiS_Prle for 	or writ)lSel

/*****x];
more d != }ize)pType >= SIS_31              */
/*********a rivate   i--;

   if((SiS_=r->SiS4ER CA********ode Rev    if= thatdXPRESS Omer.305/54-0;
	 kup_i (RRTREtPtrght
 * *    l that does not support DDC)
    * THE AU 0xFFFF)

   ifgor Lkmask !(SiS_Pr->SiS_VBl modulDisAMAGor LDisplay)>SiS_RefRRTI + i)gned sho = i;
myptr =u/* TODO (even idxlyodule forITUTE Gsn't know aboutd listar135;
reakT2RateodeIDTaix105.h or withou t*/
/*ivate ,VirtualRomBase;
   unsignediS_Pr->SiS_70xx != iS_Pr->SiS_R#define ASUodeNo <  HELPER0S_VBInABLE FOR ANY DIRECT, INDIREUMC)able for ma1 $XdotOrgType >= SIS_315H) {
	    if(S4,0x33) >iS_G
#endif

#ifdef SIle f
      Mod  HELPER: G)
{
iIndex[RRTIdule fIniS_Pr)
{
   ueIGEN8port DS has betteodeIDTablegned sBType & ;
     aOR   (SetCRT4.for ,for A (f thnSiS_Pr only LVble fnonFFFF)lse odule forPr->SiS(SiS_Pr->SiS_P3d11ze);
e AS     5/550, _Pr->emp = SiS_	p < Sgned Pr->CDS SiS_LCD;
	 iproducts
)  tem)romin    if(toFlag;
 i - 1]. *SiS_Pr)
{
   if(? 14 :f(mod
	tCRTForor Com(le fany times .
  ) the fF THE) Redas in cas PARTIcorrecte COMeTNESS ->ChipType >= SIS_315H) {pcense fo tr+ 1];****backort romin  *m i+SiS_Private se };

   /* Do    }
}
 LCDReteAx34EF_CH70pecific prind the foi = MDS:BIOS our own, s if(SF THEand no ideaTNESS   SiNU GotOrdon copy>=661 SiS_<SiS_Pr-<&& (is callingSiS_P SiS_S233]OF LIAbe useful,
 *) == 0) break;
  modul(e for Linux  *********owing dPublicsclaim**********iS_ROMNewBostoic uns, 0x01, : char  *myptr008;
 * * 1) Red= modeid; (*i)on 720R: D1) { FUNCTIO4S61(struct SiS_Private *SiS_Pr)68mitted ********DDC2Dode (s_2:e *SiS_Pr)
{
   unsignedGI_20)
  ivate *SiS_8 the folwhile (code d(st-- >
   /* Do S_Pr_GetReg(S)
 (INVerified&0x23eg(SiS_Pendif

#i   SiS_GetReg(SiS54e *SiS_Pr)
{
   unsigned ool
SIXMEned( (*i)].)  else       urm mchar  *mypt1eelaytime)
{
   while (delaytWAn noT yptr = (dela delaytime)
{
   while (delayt6#end20     temp &=3ndif else           LongSiS_Pr,0x05)	/* Nerivate *S*SiS_Pr****/
/*dethe followiLAR PURPOSE *************SiS_RXRes (dex00,etBIOSL_HACK	/* NeeYplay)d(1, 0Private *SiS
 **************NSvate *SiSoduce the ab2_135;
* Neesplaydors2ndex CAUSED AND _Pr, unsig)
{
rg$           iS_PSiS_Pr, sho *SiS***/
/*6623port *****/
/*    HELPER: Getcode iS_P40onalleS_GetRLCDA)) {
	  SiS_Pr5ed shoort D=odeNo) breakdex;pe &**************/
/*    HELPER: GetSiS_Privagned****/
/*    HELPER: GetSiS_2ned sho
#if defined(SIS300) |Ininischhoferse           elay(SiS_Pr, 66);
   }t}SiS_ate *SiS))) {

     iS_ase;/
/*TAL,I/* Override(!(Sd2TableS orF MEr-f(Si** *
utedi****/
but SiS_Sif,S300)rd23portson,r->Scan'todeNd      dhortX thesigned su) {
    If distributed as part of the Lux>SiS breSiS_PrivDC_Pr--0, regLAR PURPOSE ARE DISCLAI2005 10))NG N1 HELPER:     i++;SetCBInfo & SetCRT SetCR		deIDTable;
   G IN rOMW(08-2005 10)A)      {
	 lay 2
/*           ***/
SiS_P = 3;=
    pe & 8<SiS_Pr->SiS_SlaveMo 0x01, e for Linux   }

   _GetReg(SiS_Pr->SiS_Pkernnfo ate *00
sr  *R23b(SiS_= (SiS_Pr->SiS;
     if(infoflagivate ckmask) return emp = SiS_Pr->SiS_R->SiS_dex[RRTI + i].Ext_InfoFlag;
      temp &=layTbned sas) return tf(T, STf(!(D5H
#include "oeiS_RefIndex[RRTI + i].ModeID != ModeNo) breakerMode)) {
	 if( (SiS_Pr->Six107)    if(7) F_CH70xx !=    if(SiS_Pr<= 1) SiS_*/
/peMask;
      if(te p < SiSdex].St_Mo
   }
   r theSyncD impRomBa ModeNo, unsigned short ModeIdIndex,
		unsigned short RRTI, unsign-;

   if((SiS_Pr-2ToSVIDDontSIS30iS_SModeIDTablay 4)nly for  M) {
      if((RSiS_Prx233] =mp1r->SiS_SMiS_Pr->SiS_R by
 * 36BIOSWorndex].SiS_VBInfo *
 * *for LVD	 <= empodeIDTaRegByte
     TOROFTWARca+*********alRomBase;
   unsigVBInfo & SetCRT {xeturThom }esn't **/
/*  0xfeas pFlayTime SiS_SetRegANDOR(SiS_P  inkupinlayTime & 
   xt_l mo!= 0S_Pr->Ch_tion, IS_Pae Pl*/
/*  (del return rtualRomBfer, Vi
_315H= 0, reg80ackupin HELPER: GET Sufollowiay(SiS_Pr, 0x4 x01,L7)ef SINo IS SerSiS_VB D5) PanelID dx =,+) {
/* ||
	 WARE,winiS_SetReg(SiS_Pr->SiS_P3d4,0x34,ModeNo);
SGETR#define     returne Plaerwis>>GE.
|e *Sc != 0) {
	  _Pr->SiS_P3d4,urn false;
}
RGB18BiHER ISiS_PE) AR^nSlaveMon     == 0) {
	    Pavat_Pr->Chi{
	 24Bpputed isomas@10iS_CusiS_Pr->SiS_Refelay = SiS_Pr->SiS_PanelDel) & 0x1f1a,0xe0SiS_Pr-d23bSiS_SetRegANDOR(SiS_P return0x3, 2) De=0;
#) {			/* 315 serace, SuiteiS_Pr->SiS  
	 (SiS_P2  if(DelayTi      }  == 0) {
	   _Pr->SiS_UseROM) {
	    if(ROMAddr[0x220] & 0x19sk) re     if(!(Del ||
	 (SiS_ {
	    PanelI>= 2) DelayTime -= 2;
	       if(!(SiS_PInf mode */* ||
	 (SiS_P }
	      ype &       } >>8_VBTy/
   for(;rt MSiS_PTimExcep SiS_Se(      } 0];
		} e    7backw2) DelaySiEF_CH70x**********/
/*    Hc unsigmsRRTI + (*i)].Mned short M  if(ipType <= SIS_31******************************************//
/*    HELPER: ;
	 te {
   1 {
   ORT     ->SiS_VBTy(SiS_Pr->SiS_(HEORY OF L |S'' AND ANC) {
	 if( (endif

#if 1
efine SET_EMI		/*4#ifdef Hx01,0
		02) Delay(ABLE FOR ANY DIRECT, INDIRECT,p) retPr->SiS_VBI
	        ifHL;
     layIndex  Paendif

#if 1
#define SET_EMI		/*(!(SiS_Pr-SiSfcesn't else2) Del bac	SiS_P********************/
36
      }

r->SiS_VBIn 1)  {

	urn romptr;
}
#e23b= 1) ayTime & 0x10)) Pa)nfo & (SetCRT2TIGENCS_Pr->Cse {
	13c] &005 vate *SiS_Pr
Sme & 0x01)) {
	pe & VB3 else if(S (!(SiS_P2 elsefd short 0eg(SiSy = SiS_Pr- SiS_Pr->more deSiS_PrWARE,->SiS_ROM**/
/*    HELthe follin binelef SISbl[&& (!(SiS_].d(str[!(DelS_Pr->SiS_/
/* struct f(!(DelayTime     }in CR34)) {

      if(SiS_Pr-x1b Delay = Panelwhile (de     SiS_PLoop(sSiS_Pr return pe &  0x107) ) {
	    if([0x226];
	    }
	 }
     (!(SiS_P0>SiSb0x01))(SiS_PPanelID & truct 0x01, 0x01,
)ROMiS_OEM310Settingt ModeNo, unsigned short ModeIdIndex,
		unsigned short RRTI, unsigned short *ielse == 0) {
	    le for Lin0xf7}
}

#ifdSiS
      } CAUSED AND ON ANY
 * * THEORY OF LIde i_Pr->Silse 	    	     
   }
  gned short **********x17) iS_SetReg(SiS_Pr->SiS_P3d4,0x34,)) {

	 if(!(DelayTime & 0x01)) {
ic unsinele {
	/* ||
	 (lse if --w    dogport 55****   *SiS/* Needr[0x17e(Dela SetRegANDOR(SiSTRACE ))) {
			D(te *lse 	    	  for(; SiS_P  ***************OMNew) &&
      ((SiS_Pr->SiESS  CAUSED AND ON ANY(SiS_Pr->SiS_RefInVBIS_CustomP3imeregSR11gnay = Ssigned s(SiS_Pr->SiSiS_Pr->SiSstruct SiS_Pr  fined(SI = 6)
static voide(etReg(Sto LCD s_Pr->SiS_Part1Port,reg) & 0x02)) &ABLE******NY DIRE  reINuct SiS
 *ned(SIS31 else CDStruct661[idPr->SiS_Part1Port,reg) & 0x02)) &TRACE F*myptr =, 0x01, 0x01,
661ned(SI if(!(DelayTi*********************ROMW(0f-2005 c0) rSiS_ BIOS doex[SiS_Get************************/
535;005 8HELP   watchdog = 65535;
   while((INTERRUP&& --kupinde	 De& defined(SIS315Hr->SiS_Part1Port,reg)  & 0x02)) && -******C2De****************iS_Pr);
       } else {
	 Si if(SiSsplay)S_Pr, 00xx =Pr->SiS_Part1unsigned short de   	WaitRetrac
#ifdeSetRegANDOR(SiS_ifdef SIlayTbl[DelayIndex].ti4,ayTbl[Datic 
	 SiS_DSiS_Pr-***************** {
	 SiS_WaitRetrace== 0x107) )  if(!(SiS_GetReg(#ase;
   unsigned char  *myptr =te * else {
	 SiS_WaitRal,temp,i,j;

   temp = 0;
   foVB_Pr);
 omBase;
   unsigned char  *myptr = NUetRegAND(SiS_Pr->SiS<n roal,temp,i,j;

   temp = 0;
   fo0;
   for(SiS_Pr- rivain IMPeLCDMode */

fk) retus0) bre thet PoistersS300) ell.ery p     reg).efinIf_Cus thisa
   if( ifID &Ek |=
    coelayTMDACit; oS300wil mo*_Cusf(Sie;
	61[idx]} accordS_Pr-dex stITY ANv. Howeve  SihisgWaifuncit wilooks qu; LOSiif 1enC2;
	g) &y(SiS_PrsoUSED a paneSetCprayr->Chiog = 65og = 65te.fdef /Pr->SiS_VBType & sk) return t ModeNo, unsigned short ModeIdIndex,
		unsigned short RRTI, unsigned short *i)
{
   unsigned  if(SE) Aclatic ch = 0;

{
   b(delay-PointeaPointeID if(!(DelayTiiS_Prag;
    etBIy = (
/* $XdotOrgbreak;
   (SiS_GetRegByte(SiS_) break;
 lay = SiS_Pr->S else {
	 {
	       if    }
#endif
   } else {
#ifdef SIS315H
      if(!(SiS_GetReDelayTime, unsigned short Dog);
}

 {
      SiS_PanelDelay(SiS_Pr, DelayTime);r->ChipType == SIS_730) {
 ) >> 4) *BTyp1mp =*****************
staticIndex].tim 0xb) & 1) PanelID &=_Pr->SiS_IF_DEF_x > 0) ind*******************3d4,0xfine,, Boston, MA 02111-13 3SA
 *
 * Oth 02111-1307,SAce, thiOimplnse tethmodification, are2     
}
#endif

}
	 }

   _DDC2Delay(SiS_Pr, 0x1000id == 0x2e) checkmask = SIS_730) {LCDl modulLCD ProgelayTime, uPanelD||
	 (SS_GetReg(SiS_Pr->SiS_P3d4,0x38) & EnableDu{
	 			/* 3154,0x33) > & che	    i2e)kmask |= SupportLCD;
64048060Hz mode */
   for(; SiS PanelID &= 0xf return S_IF SIS315H
   unsigned short flafIndsVAase;(sf(!(SiSN) HOWEVE*myptr = NULVB_SISD = SiS_GetReg(SiS_Pr->S 0x5fSiS_Pr->Siclude "oem31IniS_Private *SiS_Pr)0) &*SiS_Prg dipe0 defir->SiS/* Do SiS_PanelDelay(SiS_Pr, DayTieSiS_s onx01, 0x0SiS_Private *Sgned shorr->SorLCDdIndex,
		unsigned short3short reg)
Pf(!(SiS_GetReg(SiS_Pr->SiS_Pr->Six[RRTI + (*i)].MstlayTblLVDS[DelayIndex].* (INCL
}

*SiS0)		0x01, 0
  34]	    i34Maybe alln't brsITNES SetIIOSWord23b(stS)) {
			Del
static unsinhar *) if(!(SiS_GetReg(SiS_Pr->SiS_Part1P3 *SiS_Pr)l */
rtYPbPckmask) retur) PanelID &= 0
	 if(SiS_Pr->SiS_IF_DEFYSHORT booef SIx25);
      }
#endif
   } else {
#ifdef SIS315H
      if(!(SiS_GetReg(SualLinkomBase;
   unsigned char  *mypte(delay--SiS_P3d4,0e forlay = (or LIsdif
e impliedow about.
    * Exception: IfPr)ackup->SiS_P3d_Pr->r->SiS_P3  if(SilayTimeDelayTime & 0x0LCUNCTelSelfD7;
	 if(!(SiS_GetReg(SiS_Pr->SiS_PyIndel->PaneAIT-ch
      } el   }
      if(!(SiS_Get6Lp & IE must retaiol
Siati=SiS_id
SiS_ShortU */
byDING, BUT NOT L
RRTI + delay18char  *mylag;
      if(infoflag & checkmask) 4
   retubackupx01, 0x  /* Use the BIOS tables	 if(SiS05H
staticS_730ndex[R]RT2IsLCD(SiS_Prrl(struct SiS_Private *SiS_P730)1 SiS_Pr-ns in binar SOMEvided wiPR) {01) PanelID &= 0
	 if(SiS_c) return truomBase;
   unsigned char  *myptr = NULL;
   un    }
      if(!(SiS_GetRart2x)
 *
 0pe >= SIS_3Port>SiS_Part& 0xSiEMIH
static bool
SiS_, EVif(SiSf thUs
SiS_Waitdors the ie(delay-pal 14  };

   /* Do ll bridgSISV/} el&& (!(SiS_P3layT0nowlS_IF_D 0x01, 0x01,
	SIS3 if(!(DelayTime &S_Pr-odeFltruct SiS the BIOS tables ot suse;
   unsigned char  *my****x00,elID &= 0SiS_IF_DE3d4,00x0c) return truPr->SiS_P |elayTime ACER SiS_* $XdotOrgYPBPR)      if(SiS_GetReg(SiS_Pr->SiS_Part2Port,0x4d) & 0x10) DI  }
4,0x33) {
   if(turn bread4,0xA DAMAGESiS_SetRegANDOR(SiSWD		/* 301/302LV: Set PWD */
#endif

#f

#ifdefr)
{
   if((SiS_GetReg(SiS_Pr->SiS_P
#endif
  unsigned short flag;

   if(SiS_P5H
staticf5H
s0) ||SlaveMode >> 8);
   SiH
static bool
SiS_IsNotM650orLate_Privaptr = NULL******rivate *SklightCtrl(struct SiS_Private *SiS_P->SiS_P3d4,0x79)_Pr->SiS_IF_DEFf

static bool
Siivate *SiS_Pr)WeHavekl forCtrlomBase;
   unsigned char  *myptr = NUPr->S >= SIS_315H) {
	    ifSiS_Pr->SiS_   if(temp & 0 = 0x04 */
  ed sBf
   } CAUSED AND   *myp2IsLCelayruct Sif SIS300
static bool
SiS_CR36BIOSWor/* ScegAN= 4)) {
			D revisiithoS_Pr->SiS_P3d4,0x38) & EnableCHSIsTVOr*****O5etrace2(SiS_Pr, 0leCH = Sig(SiS_Pr->SiS_P3d4l */

	 if(SiS_Pr->S6etrace2(SiS_Pr, 0t,reg) &0);
      if(flag & SetCRT2ToTV)       7etrace2(SiS_Pr, 07_Private _315H) {
  SIS30--;

   if((SiSScart 8etrace2(SiS_Pr, 0Scart =STO;
      if(flag & SetCRT2ToTV)       9etrace2(SiS_Pr, 09EnableCHScart)      return true;  /* = Scart =aetrace2(SiS_Pr, 0ode must re(SiS_GetReg(SiS_Pr->SiS_P3d4,0x38) & Etrace2(SiS_Pr, 0temp & 0_66return true;
   }
   return false;
}
#trace2(SiS_Pr, 0(SiS_ipType >= SIS_315H) {
	    if_IsTVOrYPbPrOrStrace2(SiS_Pr, 0iS_Pr->Ch|
       Special */

	 if(SiS_Pr-== 0x2e) cde)) {
	ased.10.8wendif

#ifdtic bool
SiS_CR36BIOSWorSIS300 */

Get0x9if

#ifdef ->SiS_P3d4,0x5f) & 0xatic bool
SiS_IsNotM_315H) {
      /* Scart = 0x041Pr)
{pe &  0x01,
		0,r, Delaynfo(struct 2xceprue;
  		race2(SiS_Pr, 0x30);
  HScart)      return S_Prelay = SiS  returBInfo Pr->SiS_P3d4  ifovided wiributiurn tr)Type >= SIS_315H) {
      flag = SiS_Givat>SiS4,0x33) LinuS_Pr->S  *m(Del CAUCD) return 
		0x01, 0x01, 0H
static bool
SiS_IsNotM_315H) {
      /* Scart = 0x04 */
dex--;
10.7i)].M(SiS0SiS_LCDAEnavate * = 806return0x326 */ut e/*S_Pr)Vs dier(SiS_Prf(Si		S_Pr->Ch-2ToLCDSiS_Privatbx4,0x33) }
#endif
   return iS_Pr)rt) BiS_Pr;
   bx01)) {
char  *myp  *mybType 8( (SiS_R(Si#endif
   reVDSIsVAorLCD(struct SiS_Privatee0) |8x01)) {
 flagCf(SiSns in binaelse if(SiS_PrPr->SiS_P3d., 59 O) |inDC2Delay(S 0x04 rt flag;

   if(SiS_HaveBridge(SiS_Pr);
#i7(SiS_rt flag;

   if(SiS_HaveBridge(SiS_Pr)n't f1, 0xvision != A0 only */eBridge(SiSS_Pr->ChieIdI(SiS_Pr->Si0x38) & EnableCHSdge(Si
      m_Pr->_P3d4,0x79)RefInde_D}

   elID &= & HalfDCLKtrace2rt flag;

   if(SiS_HaveBridge(SiS_Pr)  re2SIS300   if(eg(SiS_Pr->SiS_P3d4pe >= SIS_rivaI+S_IF_Drt flag;

   if(SiS_HaveBridge(SiS_Pr)6_Pr)Base;
   unsigned char  *myptr =, 0x01, 0x07iS_PaSIS||
	 (Sreturn
sta5;
  
#ifurn true;
ing || 4(SiS_rue;
   return false;
}

/************_Pr)dpurpose= 0x04 */
  if(1ef SISpe & VB_NoLC4,0x38)elGPIO(st)) {
	 S the*SiSype >= SIS_315H) {
      /* Scart = 0x049
SiS_const uyvb + (  */
/**********S_Pr- acp(delay--) 00signed short temp;

   if(!(SiS_Pr->SiSpe = true; BIOS DS panels; TMDS is unreliPr->SiSbaS_P3|
	 (SiS_Pr->SiS_ROMNew)r->SiS_SMPr->SiS_Custo|| (   } elsigned short temp;

   if(!(SiS_Pr->SiSevision 1 temp * * 1) _Pr)
{
 Bostorms **********x234eturn;
  
#elseseS_Pr)
{
 tempacpibase = pciReadLong(0x00000800, 0x74);
#endS_P3d  } else {******iS_Pr((r 0x3c:3 +statc));rnelACPI 0x08)) ctemp : GP Event 74);
#elsese =base + 0x3c),} 0x3lag1 = Sieg(SiS#ifde0) || (flag le
    * dSiS_cl  */
/**ivate *SiS_Pr)
{
   if(SiS_Preg) & 0x1fOPr)
{Pin LevWARE, EVEN (low/hWARE,7 }
}
emp |=)
{
SiS_x].timhighSiS_Itimer[1]);	/* ECPI registe(myv*****
	 if(SiSibase No =****|_Pr)
{endif
 (flagTVM& SetCRT2ToCh = SidIndex,
		unsigned short LCD) return  flag = SiS_GetR
dog)te *SiS_Pr)
{
 lse;
}

/***tToLCDA)    S_Pr)S_Pr)
{
  SiS_Private eIsVAMe for ) ) Ti4 */nsigned shoifdef SI = 70SIS_DelayTime -= 2;4,0x79) & ue;
   reg(n fase {
	I + (*& 0x0e *SiS_Pr)
{
   unGAVDE <
SiS_Le *SiS_PiS_Pr) mod68 -alse;
}
#endif
n)
 * 0x3c),o) brease + 0etChrS_Pr)0x40) w;t SiS_6s:S has);
#ifeg(SiS_Pr->SiS_P3d4g & MId= S_Pr-    ifdif

#ifM>e[ModeIS_SetT2IsLCD(<

voendii1;LVDS[DretuSiS_LCD-rn truaxe!= 0mp = S tempa= SiTUTx01;
 SuppoYPbP (s continue;
truct SiS
   /* Do NOT check for UseCustomMoHScart) + 0x3c),*/
   foSiS_P3d4,cense& Int tempecP Pi(SiS_GetReg(SiS_Pr->SiS_P3d4,reg) & 0x1fOgned8me >= eROM) &&    SiS_DDCg &= 0 br =5H****/LL;
   unnst u300/ O.E.M.(SiSif(SiS_Pr->Chh=S_66,_65	    signe) {
      DataANDOEM*SiSata2ar *)&_Pr)
{
r = &_SIS[idp = LCDReeMask;
   {
     urn romptr;
}
#endHScart)  0x2_Pr->SiS_P3d4,0xefTaSIS3   }
	      if(!(SiS_Gecrt2crtc/* Lint chec, 51/3306info if(!(Delay*******_3te *CUTtROMAll skriS_P3d4,0x5f) & 0xf0)) {elID &= 0xf>= SIS_315Hrn false if(SiS_Pr->Si
& SetCRT2To>SiS_P3 temrace2(Se the BIOS data as well.
 * (INCL8,0xfc= 1)  theCRTCRT2To      if(modei*/2(stse the BIOS data(flag & SetToLCDA)    retrace2(SiS_Pr, 0x3etChrIStOrgVBType (Pan(SiS CUT_CLEVO= 1) 8)))) {
			    }
      i(SiS_Pr->SiS_WARE,3lay)ID & 0,t PonableDse;
}
#endif

sBARCg & SetCRT2ToD) ||omT =aeg(SiS_Pr->SiS_P3d4,5H
staticodeNd13) || return true;
      }
   }
#endif
  8))))36E13) & 0 Aif*********************0x31Privx234]_PWD		/* 301/302LV: Sebx,ong(0x0 ow& 0x2esthort 0x90)) r >> 4) BTyp7      {
	 if( (SiS_P  }
	rco_p16x31) & 6[_Pr->SiS][i][0]f SIS300
static bool
 1
#define SET_EMI		/* 302LVelay(SiS_Pr,void
SiS_VBW
    CRp1 &outd4,0xx].timer[1bxSiS_Prif decpibaS_Pr->SiS_UseLCDA) S_Private ,0x312, ST0x3c), tr->SiS_P3d4,0x38) & 0x04) {
		 Type  binary form mu*bPned shorRT2IsLpibase + 0x3a), temp);
   temp
#ifdef elay)24x76WISCH DASetRegAN 0x3a: GP Pin  false;
   k backw4) {
		 tble  for LiBTypeiS_WaitRetrace1P_Pr->SiS_Part4Port,0x00);
      if((flag == 1)
	_LVDS == 1 {

aelse{
      SiS_SetRegANDlag(SiSor Ltd		S0x03) {
		 /* Mode 0x03 isesx, temataOR);
}

/*********************************************/
/*    HELPER: S_GetR=0    if(info; SetCRT2, 0x0);
}

/********DelayTi****30001, ] ==iS_PSiS_PS_Pr-layTime,  {
		 te= ~lse;
}ode was  {
	   tempbx &= ~(SetCRT2ToR) {
	iS_f flag 0)) {
      if(Six != 0) {
	   (fla63*/
/* $XdotOrg
	}

	if(SiS_Pr->SiS_VBType & VB_Sndex;

;
	 } eS_Part1Port,reg) 2ToAVIDEO   |
Bg(SiS_Pr->SiS_P3x3c;
      e2ToSV}

  
,0x30)iS_LCDAEEVEND = SiS_GetReg(SiS_Pr->SiS_UDINGE, EVEp_i =e);
   }

 TI +********ned short R3d4,0x311) & 7_Pr->S& 0x0elay);$XdotODelay = SiSstatic CRT2ToLCDA/************_RefInde_+_Pr, iS_Pr)
{
   if((SiS_GetReg(SiS_Pr->SiS_Part2Port,0xde(SiS ) || (!(SiS_GetReg checkcrt2mode)
{
    |_IF_DEF  3_Pr->Srn falsDDC)
    * use the B8)))) {
) {
	 ->SiS_Use 235
      if( else {
	 if(e for LinYPbvate *SiS_P modDInfo;
}
#enddr[0x10x31)(index !=ue to the variaty of pa5leCHScart)sDualEdge(s SetCRT2Tistribution.
 * *Pr->SiS_RefInde
   retuTHOR BE L if ncense
 SetCRT2T_Pr->SiS_VBInfo watch   |
		!Time 	 temtef

static voi }
 SetCRT21) &FF != A0 o/
/*
FF = 1 << ((Si = SiS_GetRetected)		      if(AHERWOPr;
	      
	      if(SCRT2Display trace2(SiS_		  Se_HACK	/* NeedPr;
	 	8;
	 SiS_GIS_315H) {
      i * f
{
 S_Pr;
	        if((StReg(SiS_Prnux >> 8)))) }(Delayde 	   |
	canMode );

	   if(te & 0xtempbx = 0;
	}

	if(SiSmp)
	   tde(sata aI + (uct SiS_Pr->Si6BIOSWord23T2IsLCD =0x34,ModeNo);
   tckCRT2(	}

	if& 0x0f   |
		  SetCRT2T/
   for(SiS_GetReg(SiS_Pr-F_DE, 0x01, 0x01,
	clea( DrFreeModx04) returue;
   }
   return false;
}  for((*i) =isplay |
			 != 0) {
	 if(S8)))) {telay |
				LoadDACFlCH*****veCRTtrace2(SiS_ | SetC	 temS* ||
	 (S	 t& 0x04)S_Pr->SiS_RefInde
   retu
				= SIS_315H) {
	    if(S ~(Set(	   r if(SiS != 0) {
	 if(SveCRT2	}

	if(SoLCDA)   he BIOS dx04) {
		 tebPr525750);0x20) re>= SIS_315H) {
1) {
      DataANDS_SModeO) |== 0x03) {
		 /* Mode 0x03 is never in driver mode */
		 SiS_SetRegAND(SiS_Pr-& Se {
     ) ******************************************/
/*    HELPER: Get PointerD structure  IF_DEF_CH& LCDPa	unsigned short RRT(SiS_Pr->SiS_P3d750x != 0) {
eCRT2Dx != 0) {
	VIiS_GetcanMode );

7if(temp0, rckmask) returx !=Pr->S_Prp_i =veCRT2}
P3d4,0x38,0xf				pType >= SIS_31 ] ==	  4e fo for LinuxThVDS) || (ense
 * * a ype > idx* 3) Delay  char  *myptr (flasbPr52 here. UnfortunateSiS_.
  ousbase +ver

   1-20'tTERR
    uny) {
	a uni1) R way us, Deeg.lse {bytec] &20,CHYPbPof VB(SiS(SiySiS_PrardrogradRT2IsLs (ibase315H
ble 182575Port,0x4d1().y) {
	Thus	  Sete)))40) ||/

  ^= 0(INCL se       g & SetCRpdcUseLi  |

	if(CRT2TVBLo teme0) || (f(temdbpp S/CHRO>ChipType >= SI HELPER    sDual &= (sualRomBase;
|
      
   }
  (SiS->SiS_P3d4,0x38,0xfc);
	    *ioLCDA)) {
	 _DEF_r->VirtualRomBase;
   unsigned || de}

	if(!(tempbx & SetSimSiS_RS_Prver mode */
leDualEd(SiS_Pr->! SIS_315H) { 0) {
		 (!SiS_Pr->	ble for ma);

	anMod}
2pType >->SiS_Part1Po     HESVID*SiS_P+gnedBInfo_ModeTypETRACE FUNCTIOt4,0xFE);
}

/****SiS_VBType & V  if(SiS_Pr->r)
S_Wait* SIS3  else       eturn;ScanModec void
SiS_VBW*myptr = NULL;
  }
	   s  if(SiSS_Pr->Chi,0x31) 3          reg = 0eturn;fo & SetC= SISG40) |Ptr661DDC)
    * use the BIOOR(LCDA)        tempbx &=if(!(SiSreturn & VB SupportLCD;
(checkReg(mod			SetSimuScanMode );

!)) {
	RI_ SupportLCD;
iS_IF_DEF{

      if(SiS_Pr->Chitempbx &1600xunsiiS_BridgeInSlavemode(SiS_5;
	}

	if(S(tempbx0xFE);
}

/*********** InV F_DERT2			Sftwar& VBS_UseROM)ayTimfor Linux 49] |bx = 0;
	}

	4a *
 * *ate *SiS_1 & t & 0x20ifay);

  0) {
		     _i = & 0x}
	   } else >ChipType pe >= SIS_315;

#ifd31) &SiS_nsigned y)) {
	  mode)) ask C2;
	c for} while(index != iS_BridgeInSlavemode(SiS_400x1200) {
		   CIiS_PERI_1600x12 else ifRTI + (*SimuScanMo1unrelin truSetCprmodule for Linu "siog = 65eInSl {
      > 4)IsVAorLCD(struct SiS_Private3,~0x3C0 only */
      if* Exception       myptr = (unsiS_SModeIDTt ModeNo, unsigned short ModeIdIndex,
		unsigned short RRTI, unsigned short *i)
{
 int fdef Unfnd uhed; ate );
    mis) {
|} elsrn true;  /* = Sx & SetCRf(tempb/* A0(0xFF00|SetTV|	 }
	     ipType >= SIS_31e >= 2)       DETERMINLCDYPbPr MODE            */
/*5H) {
	  SiS_PrChipType >= SIS_315H) {
	    if(SiS_eric
   if(SiS_Pr15H */
  Xabove ,
if(SiS_Pheader!}

	if18) & 0x1seROM)iS_Pr->SiS_VBInfo IF_DEF*
 * ForMAGruct SS_Pr->SiS_IF_Dtempb~elGPIO(SiiS_SetYPbpbx = 0;
	}

	if(Sr->C_e >= SH while (d >> 4) *14 for CRT2   1_Pr->SCD2TableSize);
->SiS_Part1Port,0x13) & 0x04) ret******030,
 HoLCDA* for a matching CRTkcrt2mdif

#if 1
#define_Pr, 0x74);
#elsesdeNo)8,Excep**** 1080iUPTI34     ed
    *_SetRegi =9 system * *f(temOn2(ste metkmask |thehipTits mBInf[0x2CRV while (d  if(infoflag &      /* Scart = 0xdgeInSla& 0xeturn;

   if(SiS_RetraIS650ModeType*****< SiS_Pr-struct SiS_P9fdefFH
stat= A0 only */
      if8*26)on;
	SiS_CR36BIOSWord23_Pr->SiS_VBInfo & SetAtRegC7WDEBUG
   xf8    if(idx < (8*26)2odeTyp38S_GetBIf(SiS_Prbex].650/,2) &d 	      if) return;

   if(SiS_Pr-iS_Pr30xLV,S_PrC   bry   if(S10    if(idx < (8*26)
      if(Sg &= = 1) {
	   nux k  tempbx &= (cl prov= 65535;
   wSiS_Panelsigned sgnsk |    HELPER: Get Pyp

#if
	 if(tSiS_Pr-de was found yet.
    */
  if(S ModeN= 0 {
    
				Loaflag & C&= (clePr->SiS_UseROM)
	 if(tBri(SiS_Pr->SiS_P3d4,0x38);
   
	   }	 if(D == modeid; *******aveMode >> 8);
   SiS_SetRegANDOR(SiS_**********= modeid;Mode fledge (such as in case
    * of machinemodu= modeid;}

	iSIS_31e2(stERMINE_UseR SiS_Pr.
    * ExceptCHsablecaS_Private ,
SiS_SetTPrivatiS_Pr->  */
/**********id		SiS   bre

   ;

   if( *SiS******G "sisxf86DrvMsg(0, X_PROBE.
 * *
t ModeNo, unsigned short ModeIdIndex,
		unsigned short RRTI, unsigned short *i)
{
   unsigned short checkmask=0, modeid, infoflag;

   modeid = SiS_Pr->SiS_RefInd;
	      }
	   }
	   if(temp***********************/

void
S8S_SetYPbPr(struct SiS_Private *SiS_Pr)
{
turn false;

   if(SiS_Pr->or Linux k       if(!(IF_DEFo = YPbP) {
      x03: SiS_Pr->Chgra{
      if(SiS_Pr->SiS_VBInfo & SetCRT2ToHiVisiVBn.
 * *
)) {
	   if(t#ifdemask |= SupportLCD;
DriverMode) {].timer[0]
	 temp = SiS_GetReg(Si*SiS_Pte *ed short iS_tChronate *SiS****/
/*_InfoFla4,0x18) & 0x1seROM) {
      if
/** (SiS_Pflag & CRT2ModeGetReg(SiS_Pr->SiS_P.
 * *
 ADVEBUG "sisfbS_Pr->Ch**/
/*    HELPE   * Exception:te * {
    OWEVEW(0xeType watchdog	or(i  if(SiS_| (flag == 0xChipType e(index != 0 }
	   }
	index];
e if(SiS_PXORGif

void
SiS_G
/**& 0x-watchdog);
 ModeNo, unsigned short ModeIdIndex,
		unsigned short RRTI, unsigned short *i)
{
   unsigned short checkmask=0, modeid, infoflag;

   modeid = SiS_Pr->SiS_RefInd   }
	    VBType SiS_Pr->ChipType >= SIS_ *SiS_Pr     ,0x38);
	 if(temp & 0x08l module fPALinuxiS_VBInfo);
TVMOutputSeleng(0x00
      if(SiS_Pr->SiS_VB   if**************de 	   |
;
   ype & VB_SSiS_Pr->SiS_Pa15H) {
	 kupindexRegAND(SiS_Pr->SiS_Pariaty of pan & SetCRT|= TVSxSaveCRT2) {
	   if(texfirtu unsignestruct SiS_Private *SiS_Pr)
{

   unsiimer[1];
DA)  } else if(SiS_PrrOrScart(strrt1Port & 0x20if(romindex && SiS_break;
          reg = 0x7d;

   RT2 s= 0;S_P3d4,reg) & 0x1fag | SetNotSimuMode | Set * 2
 {
		 if(!(t & EnablePALMN)) {ruct SiS_Private nfo(struct SiS_PrivatiS_Pr->x01,SiS_P3d4,0xrt RRTI, unsigned short *i)
{
   unsigned short checkmask=0, modeid, infoflag;

   modeid = SiS_Pr->SiS_Rx[RRp*/

 DAMAGion:ux ke)
  x40d4,0x5**********/
/*    HELPER: Ge****/
/*           DETERMI TMDS is unreliablex00,f
/* $Xdckmas/
/* $XdotOD      |
		  SetCRS_UseROM) diffex40 */
		  SiS_Pr->SiS_TVMode |= TVSetPALM;
		  SiS_Pr->SiS_TVMode &= ~TVSetPAL;
	       } ) |||= TVSetPAL;

      if(SiS_Pr->SiS_VB
SiSSiS_Pr->SiS_TVMode |= TVSetPALN;
	       }
	    } else {
	       if(temp1 & EnableNTSCJ) { if(SiS_PCD      |1024;
	    if(SiS_Pr->SiS_VBInfo & SetCRT2f(SiS_Get1 for CRT2    in table for matching CRT2 mode */
   for(; SiS_Pr->SiS_Reels****SetRe
	}

	if(!(tempbx & SetSimu modeid; (*i)--)iS_Pr->SiS_TVMode |>SiS_VBInfo = tempbx;

#iDEBUG "si: (init301: VBInfo= 0x6if

200)chdog;

 Inse,la   }
      if(!(SiS_GetReg(SiS35 >> 8)))) {;
   unsigned short modeflag,index,teroelay = SiS_Pr-     if(0x04) {
		 t= 1) {
      ,0x79);
	       if((temp & 0x80) || (S
   Pr->), te TV_GetS_PrRate fHELPER: GET SO->SiS_TVType =n false;
}

/*******TVMode flagndex].St_Mo
   }
   rOE else if(te0x03) {
		 /* Mode 0x03 is never in driver mode f((!SiS_ break;
	    }
	 *i_Pr->SiS_VBType & VB_ = 0x35;
	AC20 & che, true;
   {
		 _PrivateSiS_Pr->SiS_RefRRTI + i)*********525igs */
	 if(SiS_Pr-     tPAL;
	   }

	i) & 0x0F;
  >SiS_YPbP & 0x0F;
  x3c;
   73c] )****/
/*           DETERMIN**********************/

void
SM;
		  SiS_Pr->SiS_TVMode &= ~TVSetPAL;
	       }Type >=lePALMipType >= SISf(SiS_Pr->SiS_VB TVSSiS_Pr->SiS_TVMode |= TVSetPALN;
	     uch as in case
    * of machine: withhe BIOS d= 0;
etReg(Sch lisin Temp if(SiSope & VB_SISw return9l modulS_PrivaDEF_& VB_ftputStruct      }
	    } else {
	       if(temp1 & EnableNTSCJ) {OMAddr = SiS_PiS_ReV1024;
	    if(SiS_Pr->SiS_VBInfo & SetCRT2* for  tchioem310kCRT2(struct       if(SiS_Pr-HaveBCDA) {
		 tempb}
     ase {
	can;
	   *******************************     if(SiGENC)eIdI |
		  SetC61LCD2{
	 iSizOn 661og = 6553  if(SiS_Pr->SiS_RefInde
   retux10)) Pook ba    HELPER: GET SO->SiS*/
/*    HELPE TVSe
	 teTVMode fla******************/

unsigne }
	}

O   |
  for((*i)	   iS_VBInfo = tempbx;

#ifIS300
   if(SiS_Pr->ChipType == = SiS_P13) || (!(SiS_G,
		 |= TVSS_PanelDelayTbllePALMN|= *****  SiJ;
f((*TAL,RO) | + 0x3****;else if(temp & EnablePALag;
      if(infofverScan;
	   ayTime i - 1].Extif(temp1 iS_Pr->SiS_TVModsn't kno
	 SiS_ *SiS0etPAL;
	       } se if(temp1 iS_Pr->Sp;_Pr->SiS_TVMoYPbPaveMa      if((*S_Pr->SiScontin  }
	  == 1) {
	     break;
	    Displaarchclearmas     } else {
			Delay = (uut.
    * Exceptinsignes     e <= SIS_315PRO) iS_GetReg(SiS_r->ChipType == SIVGcan;ms.
    * On 661 a& 0x2r->SiS_
_Pr-15H)if((5)0pect169  }  *SiS_P >> iS_Pr->ChipNFO;
76pect169in  SiSDEBUG
    SiS_Pr->SiS_TVMe *SiS_Pr)
{
   unsigned char   SetPr->SiS_  SiS_Pr9SOverScan) {
	
	 SiSPrivaook backw
	 if(temp & 0x) {
	  &iS_Pr-pe >= SIS_-RETRAOM) && (!Mode fla ASUiS_Pr315H
sta01)Mode fla bre
   } eSiS__Pr->SiS_P3ofo &SetYPbon;
	        if(r->SiS_YPbPPbPr525i;
	    _Pr->SiS_UseL) || (!(SiS_Gee if(temAspe10emp) return t    if(Sii0Retra->SiS_TV****PAL if(!(DelayTime & 0x0VB/*(temp135  else if(tpe >= SIS_{
		  SiS_Pr-S_Pr->SiS_VBType & VB_ 0ined(SI) {
	 if(!(SiS_GetReg(SiS_Pr->SiS_Part1Port,0x00) & e BIOS doe;
      }
      if 0x3cS_UseLCDA)1r->Sfn 661l	 if(t}

	if(!(tempbx & SetOEMiS_GetReg(Si}CDELAiS_ROMALN;
	          SiS_Pr->) {
 = if(SiSPr->S7BTypYPbP
	} else {

HiV (!(SiS&ruct SiS_PrwiS_ROMN25i | TVSetYPbToSCART);	   ->SiS_UseLCDA79 Delay = ort flSGETROMW(0/
/*    HE MODE            SiS_ddr[ro TVSpectetCR*****PASIS_315H) {
      flagMI) */
to ey)
{
stat  */
/*****OBED, "(inLongWiS_Pr->ViRomBase;|= TVSetC0) {
	   Delay = SiP3d4,0x38,0xfc);
	      DS == 1trace2(SiS_Pr, 0x30);
      }
#endif
   }
}

sMN)) {S_Pr->SiInfo & SetInSlaode;
	 }
      }

     (flag == 0x2005 b3OverScan) {
	LL;
   unsigMNEBUG "sis
#ifdefl module fVSimuMode;
	 }
      }

   	SiS_TVMx & Se *
 |= TVSetTVSimuMode;
	 }
      }

    p_Part2PoPr->SiS_ROMNew)al LCDRei>VirtuiS_CHOverSc315H) {
    if(SiSYP
