/* $XFree86$ */
/* $XdotOrg$ */
/*
 * Mode initializing code (CRT1 section) for
 * for SiS 300/305/540/630/730,
 *     SiS 315/550/[M]650/651/[M]661[FGM]X/[M]74x[GX]/330/[M]76x[GX],
 *     XGI Volari V3XT/V5/V8, Z7
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "init.h"

#ifdef SIS300
#include "300vtbl.h"
#endif

#ifdef SIS315H
#include "310vtbl.h"
#endif

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE,SiSSetMode)
#endif

/*********************************************/
/*         POINTER INITIALIZATION            */
/*********************************************/

#if defined(SIS300) || defined(SIS315H)
static void
InitCommonPointer(struct SiS_Private *SiS_Pr)
{
   SiS_Pr->SiS_SModeIDTable  = SiS_SModeIDTable;
   SiS_Pr->SiS_StResInfo     = SiS_StResInfo;
   SiS_Pr->SiS_ModeResInfo   = SiS_ModeResInfo;
   SiS_Pr->SiS_StandTable    = SiS_StandTable;

   SiS_Pr->SiS_NTSCTiming     = SiS_NTSCTiming;
   SiS_Pr->SiS_PALTiming      = SiS_PALTiming;
   SiS_Pr->SiS_HiTVSt1Timing  = SiS_HiTVSt1Timing;
   SiS_Pr->SiS_HiTVSt2Timing  = SiS_HiTVSt2Timing;

   SiS_Pr->SiS_HiTVExtTiming  = SiS_HiTVExtTiming;
   SiS_Pr->SiS_HiTVGroup3Data = SiS_HiTVGroup3Data;
   SiS_Pr->SiS_HiTVGroup3Simu = SiS_HiTVGroup3Simu;
#if 0
   SiS_Pr->SiS_HiTVTextTiming = SiS_HiTVTextTiming;
   SiS_Pr->SiS_HiTVGroup3Text = SiS_HiTVGroup3Text;
#endif

   SiS_Pr->SiS_StPALData   = SiS_StPALData;
   SiS_Pr->SiS_ExtPALData  = SiS_ExtPALData;
   SiS_Pr->SiS_StNTSCData  = SiS_StNTSCData;
   SiS_Pr->SiS_ExtNTSCData = SiS_ExtNTSCData;
   SiS_Pr->SiS_St1HiTVData = SiS_StHiTVData;
   SiS_Pr->SiS_St2HiTVData = SiS_St2HiTVData;
   SiS_Pr->SiS_ExtHiTVData = SiS_ExtHiTVData;
   SiS_Pr->SiS_St525iData  = SiS_StNTSCData;
   SiS_Pr->SiS_St525pData  = SiS_St525pData;
   SiS_Pr->SiS_St750pData  = SiS_St750pData;
   SiS_Pr->SiS_Ext525iData = SiS_ExtNTSCData;
   SiS_Pr->SiS_Ext525pData = SiS_ExtNTSCData;
   SiS_Pr->SiS_Ext750pData = SiS_Ext750pData;

   SiS_Pr->pSiS_OutputSelect = &SiS_OutputSelect;
   SiS_Pr->pSiS_SoftSetting  = &SiS_SoftSetting;

   SiS_Pr->SiS_LCD1280x720Data      = SiS_LCD1280x720Data;
   SiS_Pr->SiS_StLCD1280x768_2Data  = SiS_StLCD1280x768_2Data;
   SiS_Pr->SiS_ExtLCD1280x768_2Data = SiS_ExtLCD1280x768_2Data;
   SiS_Pr->SiS_LCD1280x800Data      = SiS_LCD1280x800Data;
   SiS_Pr->SiS_LCD1280x800_2Data    = SiS_LCD1280x800_2Data;
   SiS_Pr->SiS_LCD1280x854Data      = SiS_LCD1280x854Data;
   SiS_Pr->SiS_LCD1280x960Data      = SiS_LCD1280x960Data;
   SiS_Pr->SiS_StLCD1400x1050Data   = SiS_StLCD1400x1050Data;
   SiS_Pr->SiS_ExtLCD1400x1050Data  = SiS_ExtLCD1400x1050Data;
   SiS_Pr->SiS_LCD1680x1050Data     = SiS_LCD1680x1050Data;
   SiS_Pr->SiS_StLCD1600x1200Data   = SiS_StLCD1600x1200Data;
   SiS_Pr->SiS_ExtLCD1600x1200Data  = SiS_ExtLCD1600x1200Data;
   SiS_Pr->SiS_NoScaleData          = SiS_NoScaleData;

   SiS_Pr->SiS_LVDS320x240Data_1   = SiS_LVDS320x240Data_1;
   SiS_Pr->SiS_LVDS320x240Data_2   = SiS_LVDS320x240Data_2;
   SiS_Pr->SiS_LVDS640x480Data_1   = SiS_LVDS640x480Data_1;
   SiS_Pr->SiS_LVDS800x600Data_1   = SiS_LVDS800x600Data_1;
   SiS_Pr->SiS_LVDS1024x600Data_1  = SiS_LVDS1024x600Data_1;
   SiS_Pr->SiS_LVDS1024x768Data_1  = SiS_LVDS1024x768Data_1;

   SiS_Pr->SiS_LVDSCRT1320x240_1     = SiS_LVDSCRT1320x240_1;
   SiS_Pr->SiS_LVDSCRT1320x240_2     = SiS_LVDSCRT1320x240_2;
   SiS_Pr->SiS_LVDSCRT1320x240_2_H   = SiS_LVDSCRT1320x240_2_H;
   SiS_Pr->SiS_LVDSCRT1320x240_3     = SiS_LVDSCRT1320x240_3;
   SiS_Pr->SiS_LVDSCRT1320x240_3_H   = SiS_LVDSCRT1320x240_3_H;
   SiS_Pr->SiS_LVDSCRT1640x480_1     = SiS_LVDSCRT1640x480_1;
   SiS_Pr->SiS_LVDSCRT1640x480_1_H   = SiS_LVDSCRT1640x480_1_H;
#if 0
   SiS_Pr->SiS_LVDSCRT11024x600_1    = SiS_LVDSCRT11024x600_1;
   SiS_Pr->SiS_LVDSCRT11024x600_1_H  = SiS_LVDSCRT11024x600_1_H;
   SiS_Pr->SiS_LVDSCRT11024x600_2    = SiS_LVDSCRT11024x600_2;
   SiS_Pr->SiS_LVDSCRT11024x600_2_H  = SiS_LVDSCRT11024x600_2_H;
#endif

   SiS_Pr->SiS_CHTVUNTSCData = SiS_CHTVUNTSCData;
   SiS_Pr->SiS_CHTVONTSCData = SiS_CHTVONTSCData;

   SiS_Pr->SiS_PanelMinLVDS   = Panel_800x600;    /* lowest value LVDS/LCDA */
   SiS_Pr->SiS_PanelMin301    = Panel_1024x768;   /* lowest value 301 */
}
#endif

#ifdef SIS300
static void
InitTo300Pointer(struct SiS_Private *SiS_Pr)
{
   InitCommonPointer(SiS_Pr);

   SiS_Pr->SiS_VBModeIDTable = SiS300_VBModeIDTable;
   SiS_Pr->SiS_EModeIDTable  = SiS300_EModeIDTable;
   SiS_Pr->SiS_RefIndex      = SiS300_RefIndex;
   SiS_Pr->SiS_CRT1Table     = SiS300_CRT1Table;
   if(SiS_Pr->ChipType == SIS_300) {
      SiS_Pr->SiS_MCLKData_0 = SiS300_MCLKData_300; /* 300 */
   } else {
      SiS_Pr->SiS_MCLKData_0 = SiS300_MCLKData_630; /* 630, 730 */
   }
   SiS_Pr->SiS_VCLKData      = SiS300_VCLKData;
   SiS_Pr->SiS_VBVCLKData    = (struct SiS_VBVCLKData *)SiS300_VCLKData;

   SiS_Pr->SiS_SR15  = SiS300_SR15;

   SiS_Pr->SiS_PanelDelayTbl     = SiS300_PanelDelayTbl;
   SiS_Pr->SiS_PanelDelayTblLVDS = SiS300_PanelDelayTbl;

   SiS_Pr->SiS_ExtLCD1024x768Data   = SiS300_ExtLCD1024x768Data;
   SiS_Pr->SiS_St2LCD1024x768Data   = SiS300_St2LCD1024x768Data;
   SiS_Pr->SiS_ExtLCD1280x1024Data  = SiS300_ExtLCD1280x1024Data;
   SiS_Pr->SiS_St2LCD1280x1024Data  = SiS300_St2LCD1280x1024Data;

   SiS_Pr->SiS_CRT2Part2_1024x768_1  = SiS300_CRT2Part2_1024x768_1;
   SiS_Pr->SiS_CRT2Part2_1024x768_2  = SiS300_CRT2Part2_1024x768_2;
   SiS_Pr->SiS_CRT2Part2_1024x768_3  = SiS300_CRT2Part2_1024x768_3;

   SiS_Pr->SiS_CHTVUPALData  = SiS300_CHTVUPALData;
   SiS_Pr->SiS_CHTVOPALData  = SiS300_CHTVOPALData;
   SiS_Pr->SiS_CHTVUPALMData = SiS_CHTVUNTSCData;    /* not supported on 300 series */
   SiS_Pr->SiS_CHTVOPALMData = SiS_CHTVONTSCData;    /* not supported on 300 series */
   SiS_Pr->SiS_CHTVUPALNData = SiS300_CHTVUPALData;  /* not supported on 300 series */
   SiS_Pr->SiS_CHTVOPALNData = SiS300_CHTVOPALData;  /* not supported on 300 series */
   SiS_Pr->SiS_CHTVSOPALData = SiS300_CHTVSOPALData;

   SiS_Pr->SiS_LVDS848x480Data_1   = SiS300_LVDS848x480Data_1;
   SiS_Pr->SiS_LVDS848x480Data_2   = SiS300_LVDS848x480Data_2;
   SiS_Pr->SiS_LVDSBARCO1024Data_1 = SiS300_LVDSBARCO1024Data_1;
   SiS_Pr->SiS_LVDSBARCO1366Data_1 = SiS300_LVDSBARCO1366Data_1;
   SiS_Pr->SiS_LVDSBARCO1366Data_2 = SiS300_LVDSBARCO1366Data_2;

   SiS_Pr->SiS_PanelType04_1a = SiS300_PanelType04_1a;
   SiS_Pr->SiS_PanelType04_2a = SiS300_PanelType04_2a;
   SiS_Pr->SiS_PanelType04_1b = SiS300_PanelType04_1b;
   SiS_Pr->SiS_PanelType04_2b = SiS300_PanelType04_2b;

   SiS_Pr->SiS_CHTVCRT1UNTSC = SiS300_CHTVCRT1UNTSC;
   SiS_Pr->SiS_CHTVCRT1ONTSC = SiS300_CHTVCRT1ONTSC;
   SiS_Pr->SiS_CHTVCRT1UPAL  = SiS300_CHTVCRT1UPAL;
   SiS_Pr->SiS_CHTVCRT1OPAL  = SiS300_CHTVCRT1OPAL;
   SiS_Pr->SiS_CHTVCRT1SOPAL = SiS300_CHTVCRT1SOPAL;
   SiS_Pr->SiS_CHTVReg_UNTSC = SiS300_CHTVReg_UNTSC;
   SiS_Pr->SiS_CHTVReg_ONTSC = SiS300_CHTVReg_ONTSC;
   SiS_Pr->SiS_CHTVReg_UPAL  = SiS300_CHTVReg_UPAL;
   SiS_Pr->SiS_CHTVReg_OPAL  = SiS300_CHTVReg_OPAL;
   SiS_Pr->SiS_CHTVReg_UPALM = SiS300_CHTVReg_UNTSC;  /* not supported on 300 series */
   SiS_Pr->SiS_CHTVReg_OPALM = SiS300_CHTVReg_ONTSC;  /* not supported on 300 series */
   SiS_Pr->SiS_CHTVReg_UPALN = SiS300_CHTVReg_UPAL;   /* not supported on 300 series */
   SiS_Pr->SiS_CHTVReg_OPALN = SiS300_CHTVReg_OPAL;   /* not supported on 300 series */
   SiS_Pr->SiS_CHTVReg_SOPAL = SiS300_CHTVReg_SOPAL;
   SiS_Pr->SiS_CHTVVCLKUNTSC = SiS300_CHTVVCLKUNTSC;
   SiS_Pr->SiS_CHTVVCLKONTSC = SiS300_CHTVVCLKONTSC;
   SiS_Pr->SiS_CHTVVCLKUPAL  = SiS300_CHTVVCLKUPAL;
   SiS_Pr->SiS_CHTVVCLKOPAL  = SiS300_CHTVVCLKOPAL;
   SiS_Pr->SiS_CHTVVCLKUPALM = SiS300_CHTVVCLKUNTSC;  /* not supported on 300 series */
   SiS_Pr->SiS_CHTVVCLKOPALM = SiS300_CHTVVCLKONTSC;  /* not supported on 300 series */
   SiS_Pr->SiS_CHTVVCLKUPALN = SiS300_CHTVVCLKUPAL;   /* not supported on 300 series */
   SiS_Pr->SiS_CHTVVCLKOPALN = SiS300_CHTVVCLKOPAL;   /* not supported on 300 series */
   SiS_Pr->SiS_CHTVVCLKSOPAL = SiS300_CHTVVCLKSOPAL;
}
#endif

#ifdef SIS315H
static void
InitTo310Pointer(struct SiS_Private *SiS_Pr)
{
   InitCommonPointer(SiS_Pr);

   SiS_Pr->SiS_EModeIDTable  = SiS310_EModeIDTable;
   SiS_Pr->SiS_RefIndex      = SiS310_RefIndex;
   SiS_Pr->SiS_CRT1Table     = SiS310_CRT1Table;
   if(SiS_Pr->ChipType >= SIS_340) {
      SiS_Pr->SiS_MCLKData_0 = SiS310_MCLKData_0_340;  /* 340 + XGI */
   } else if(SiS_Pr->ChipType >= SIS_761) {
      SiS_Pr->SiS_MCLKData_0 = SiS310_MCLKData_0_761;  /* 761 - preliminary */
   } else if(SiS_Pr->ChipType >= SIS_760) {
      SiS_Pr->SiS_MCLKData_0 = SiS310_MCLKData_0_760;  /* 760 */
   } else if(SiS_Pr->ChipType >= SIS_661) {
      SiS_Pr->SiS_MCLKData_0 = SiS310_MCLKData_0_660;  /* 661/741 */
   } else if(SiS_Pr->ChipType == SIS_330) {
      SiS_Pr->SiS_MCLKData_0 = SiS310_MCLKData_0_330;  /* 330 */
   } else if(SiS_Pr->ChipType > SIS_315PRO) {
      SiS_Pr->SiS_MCLKData_0 = SiS310_MCLKData_0_650;  /* 550, 650, 740 */
   } else {
      SiS_Pr->SiS_MCLKData_0 = SiS310_MCLKData_0_315;  /* 315 */
   }
   if(SiS_Pr->ChipType >= SIS_340) {
      SiS_Pr->SiS_MCLKData_1 = SiS310_MCLKData_1_340;
   } else {
      SiS_Pr->SiS_MCLKData_1 = SiS310_MCLKData_1;
   }
   SiS_Pr->SiS_VCLKData      = SiS310_VCLKData;
   SiS_Pr->SiS_VBVCLKData    = SiS310_VBVCLKData;

   SiS_Pr->SiS_SR15  = SiS310_SR15;

   SiS_Pr->SiS_PanelDelayTbl     = SiS310_PanelDelayTbl;
   SiS_Pr->SiS_PanelDelayTblLVDS = SiS310_PanelDelayTblLVDS;

   SiS_Pr->SiS_St2LCD1024x768Data   = SiS310_St2LCD1024x768Data;
   SiS_Pr->SiS_ExtLCD1024x768Data   = SiS310_ExtLCD1024x768Data;
   SiS_Pr->SiS_St2LCD1280x1024Data  = SiS310_St2LCD1280x1024Data;
   SiS_Pr->SiS_ExtLCD1280x1024Data  = SiS310_ExtLCD1280x1024Data;

   SiS_Pr->SiS_CRT2Part2_1024x768_1  = SiS310_CRT2Part2_1024x768_1;

   SiS_Pr->SiS_CHTVUPALData  = SiS310_CHTVUPALData;
   SiS_Pr->SiS_CHTVOPALData  = SiS310_CHTVOPALData;
   SiS_Pr->SiS_CHTVUPALMData = SiS310_CHTVUPALMData;
   SiS_Pr->SiS_CHTVOPALMData = SiS310_CHTVOPALMData;
   SiS_Pr->SiS_CHTVUPALNData = SiS310_CHTVUPALNData;
   SiS_Pr->SiS_CHTVOPALNData = SiS310_CHTVOPALNData;
   SiS_Pr->SiS_CHTVSOPALData = SiS310_CHTVSOPALData;

   SiS_Pr->SiS_CHTVCRT1UNTSC = SiS310_CHTVCRT1UNTSC;
   SiS_Pr->SiS_CHTVCRT1ONTSC = SiS310_CHTVCRT1ONTSC;
   SiS_Pr->SiS_CHTVCRT1UPAL  = SiS310_CHTVCRT1UPAL;
   SiS_Pr->SiS_CHTVCRT1OPAL  = SiS310_CHTVCRT1OPAL;
   SiS_Pr->SiS_CHTVCRT1SOPAL = SiS310_CHTVCRT1OPAL;

   SiS_Pr->SiS_CHTVReg_UNTSC = SiS310_CHTVReg_UNTSC;
   SiS_Pr->SiS_CHTVReg_ONTSC = SiS310_CHTVReg_ONTSC;
   SiS_Pr->SiS_CHTVReg_UPAL  = SiS310_CHTVReg_UPAL;
   SiS_Pr->SiS_CHTVReg_OPAL  = SiS310_CHTVReg_OPAL;
   SiS_Pr->SiS_CHTVReg_UPALM = SiS310_CHTVReg_UPALM;
   SiS_Pr->SiS_CHTVReg_OPALM = SiS310_CHTVReg_OPALM;
   SiS_Pr->SiS_CHTVReg_UPALN = SiS310_CHTVReg_UPALN;
   SiS_Pr->SiS_CHTVReg_OPALN = SiS310_CHTVReg_OPALN;
   SiS_Pr->SiS_CHTVReg_SOPAL = SiS310_CHTVReg_OPAL;

   SiS_Pr->SiS_CHTVVCLKUNTSC = SiS310_CHTVVCLKUNTSC;
   SiS_Pr->SiS_CHTVVCLKONTSC = SiS310_CHTVVCLKONTSC;
   SiS_Pr->SiS_CHTVVCLKUPAL  = SiS310_CHTVVCLKUPAL;
   SiS_Pr->SiS_CHTVVCLKOPAL  = SiS310_CHTVVCLKOPAL;
   SiS_Pr->SiS_CHTVVCLKUPALM = SiS310_CHTVVCLKUPALM;
   SiS_Pr->SiS_CHTVVCLKOPALM = SiS310_CHTVVCLKOPALM;
   SiS_Pr->SiS_CHTVVCLKUPALN = SiS310_CHTVVCLKUPALN;
   SiS_Pr->SiS_CHTVVCLKOPALN = SiS310_CHTVVCLKOPALN;
   SiS_Pr->SiS_CHTVVCLKSOPAL = SiS310_CHTVVCLKOPAL;
}
#endif

bool
SiSInitPtr(struct SiS_Private *SiS_Pr)
{
   if(SiS_Pr->ChipType < SIS_315H) {
#ifdef SIS300
      InitTo300Pointer(SiS_Pr);
#else
      return false;
#endif
   } else {
#ifdef SIS315H
      InitTo310Pointer(SiS_Pr);
#else
      return false;
#endif
   }
   return true;
}

/*********************************************/
/*            HELPER: Get ModeID             */
/*********************************************/

#ifndef SIS_XORG_XF86
static
#endif
unsigned short
SiS_GetModeID(int VGAEngine, unsigned int VBFlags, int HDisplay, int VDisplay,
		int Depth, bool FSTN, int LCDwidth, int LCDheight)
{
   unsigned short ModeIndex = 0;

   switch(HDisplay)
   {
	case 320:
		if(VDisplay == 200) ModeIndex = ModeIndex_320x200[Depth];
		else if(VDisplay == 240) {
			if((VBFlags & CRT2_LCD) && (FSTN))
				ModeIndex = ModeIndex_320x240_FSTN[Depth];
			else
				ModeIndex = ModeIndex_320x240[Depth];
		}
		break;
	case 400:
		if((!(VBFlags & CRT1_LCDA)) || ((LCDwidth >= 800) && (LCDwidth >= 600))) {
			if(VDisplay == 300) ModeIndex = ModeIndex_400x300[Depth];
		}
		break;
	case 512:
		if((!(VBFlags & CRT1_LCDA)) || ((LCDwidth >= 1024) && (LCDwidth >= 768))) {
			if(VDisplay == 384) ModeIndex = ModeIndex_512x384[Depth];
		}
		break;
	case 640:
		if(VDisplay == 480)      ModeIndex = ModeIndex_640x480[Depth];
		else if(VDisplay == 400) ModeIndex = ModeIndex_640x400[Depth];
		break;
	case 720:
		if(VDisplay == 480)      ModeIndex = ModeIndex_720x480[Depth];
		else if(VDisplay == 576) ModeIndex = ModeIndex_720x576[Depth];
		break;
	case 768:
		if(VDisplay == 576) ModeIndex = ModeIndex_768x576[Depth];
		break;
	case 800:
		if(VDisplay == 600)      ModeIndex = ModeIndex_800x600[Depth];
		else if(VDisplay == 480) ModeIndex = ModeIndex_800x480[Depth];
		break;
	case 848:
		if(VDisplay == 480) ModeIndex = ModeIndex_848x480[Depth];
		break;
	case 856:
		if(VDisplay == 480) ModeIndex = ModeIndex_856x480[Depth];
		break;
	case 960:
		if(VGAEngine == SIS_315_VGA) {
			if(VDisplay == 540)      ModeIndex = ModeIndex_960x540[Depth];
			else if(VDisplay == 600) ModeIndex = ModeIndex_960x600[Depth];
		}
		break;
	case 1024:
		if(VDisplay == 576)      ModeIndex = ModeIndex_1024x576[Depth];
		else if(VDisplay == 768) ModeIndex = ModeIndex_1024x768[Depth];
		else if(VGAEngine == SIS_300_VGA) {
			if(VDisplay == 600) ModeIndex = ModeIndex_1024x600[Depth];
		}
		break;
	case 1152:
		if(VDisplay == 864) ModeIndex = ModeIndex_1152x864[Depth];
		if(VGAEngine == SIS_300_VGA) {
			if(VDisplay == 768) ModeIndex = ModeIndex_1152x768[Depth];
		}
		break;
	case 1280:
		switch(VDisplay) {
			case 720:
				ModeIndex = ModeIndex_1280x720[Depth];
				break;
			case 768:
				if(VGAEngine == SIS_300_VGA) {
					ModeIndex = ModeIndex_300_1280x768[Depth];
				} else {
					ModeIndex = ModeIndex_310_1280x768[Depth];
				}
				break;
			case 800:
				if(VGAEngine == SIS_315_VGA) {
					ModeIndex = ModeIndex_1280x800[Depth];
				}
				break;
			case 854:
				if(VGAEngine == SIS_315_VGA) {
					ModeIndex = ModeIndex_1280x854[Depth];
				}
				break;
			case 960:
				ModeIndex = ModeIndex_1280x960[Depth];
				break;
			case 1024:
				ModeIndex = ModeIndex_1280x1024[Depth];
				break;
		}
		break;
	case 1360:
		if(VDisplay == 768) ModeIndex = ModeIndex_1360x768[Depth];
		if(VGAEngine == SIS_300_VGA) {
			if(VDisplay == 1024) ModeIndex = ModeIndex_300_1360x1024[Depth];
		}
		break;
	case 1400:
		if(VGAEngine == SIS_315_VGA) {
			if(VDisplay == 1050) {
				ModeIndex = ModeIndex_1400x1050[Depth];
			}
		}
		break;
	case 1600:
		if(VDisplay == 1200) ModeIndex = ModeIndex_1600x1200[Depth];
		break;
	case 1680:
		if(VGAEngine == SIS_315_VGA) {
			if(VDisplay == 1050) ModeIndex = ModeIndex_1680x1050[Depth];
		}
		break;
	case 1920:
		if(VDisplay == 1440) ModeIndex = ModeIndex_1920x1440[Depth];
		else if(VGAEngine == SIS_315_VGA) {
			if(VDisplay == 1080) ModeIndex = ModeIndex_1920x1080[Depth];
		}
		break;
	case 2048:
		if(VDisplay == 1536) {
			if(VGAEngine == SIS_300_VGA) {
				ModeIndex = ModeIndex_300_2048x1536[Depth];
			} else {
				ModeIndex = ModeIndex_310_2048x1536[Depth];
			}
		}
		break;
   }

   return ModeIndex;
}

unsigned short
SiS_GetModeID_LCD(int VGAEngine, unsigned int VBFlags, int HDisplay, int VDisplay,
		int Depth, bool FSTN, unsigned short CustomT, int LCDwidth, int LCDheight,
		unsigned int VBFlags2)
{
   unsigned short ModeIndex = 0;

   if(VBFlags2 & (VB2_LVDS | VB2_30xBDH)) {

      switch(HDisplay)
      {
	case 320:
	     if((CustomT != CUT_PANEL848) && (CustomT != CUT_PANEL856)) {
		if(VDisplay == 200) {
		   if(!FSTN) ModeIndex = ModeIndex_320x200[Depth];
		} else if(VDisplay == 240) {
		   if(!FSTN) ModeIndex = ModeIndex_320x240[Depth];
		   else if(VGAEngine == SIS_315_VGA) {
		      ModeIndex = ModeIndex_320x240_FSTN[Depth];
		   }
		}
	     }
	     break;
	case 400:
	     if((CustomT != CUT_PANEL848) && (CustomT != CUT_PANEL856)) {
		if(!((VGAEngine == SIS_300_VGA) && (VBFlags2 & VB2_TRUMPION))) {
		   if(VDisplay == 300) ModeIndex = ModeIndex_400x300[Depth];
		}
	     }
	     break;
	case 512:
	     if((CustomT != CUT_PANEL848) && (CustomT != CUT_PANEL856)) {
		if(!((VGAEngine == SIS_300_VGA) && (VBFlags2 & VB2_TRUMPION))) {
		   if(LCDwidth >= 1024 && LCDwidth != 1152 && LCDheight >= 768) {
		      if(VDisplay == 384) {
		         ModeIndex = ModeIndex_512x384[Depth];
		      }
		   }
		}
	     }
	     break;
	case 640:
	     if(VDisplay == 480) ModeIndex = ModeIndex_640x480[Depth];
	     else if(VDisplay == 400) {
		if((CustomT != CUT_PANEL848) && (CustomT != CUT_PANEL856))
		   ModeIndex = ModeIndex_640x400[Depth];
	     }
	     break;
	case 800:
	     if(VDisplay == 600) ModeIndex = ModeIndex_800x600[Depth];
	     break;
	case 848:
	     if(CustomT == CUT_PANEL848) {
	        if(VDisplay == 480) ModeIndex = ModeIndex_848x480[Depth];
	     }
	     break;
	case 856:
	     if(CustomT == CUT_PANEL856) {
	        if(VDisplay == 480) ModeIndex = ModeIndex_856x480[Depth];
	     }
	     break;
	case 1024:
	     if(VDisplay == 768) ModeIndex = ModeIndex_1024x768[Depth];
	     else if(VGAEngine == SIS_300_VGA) {
		if((VDisplay == 600) && (LCDheight == 600)) {
		   ModeIndex = ModeIndex_1024x600[Depth];
		}
	     }
	     break;
	case 1152:
	     if(VGAEngine == SIS_300_VGA) {
		if((VDisplay == 768) && (LCDheight == 768)) {
		   ModeIndex = ModeIndex_1152x768[Depth];
		}
	     }
	     break;
        case 1280:
	     if(VDisplay == 1024) ModeIndex = ModeIndex_1280x1024[Depth];
	     else if(VGAEngine == SIS_315_VGA) {
		if((VDisplay == 768) && (LCDheight == 768)) {
		   ModeIndex = ModeIndex_310_1280x768[Depth];
		}
	     }
	     break;
	case 1360:
	     if(VGAEngine == SIS_300_VGA) {
		if(CustomT == CUT_BARCO1366) {
		   if(VDisplay == 1024) ModeIndex = ModeIndex_300_1360x1024[Depth];
		}
	     }
	     if(CustomT == CUT_PANEL848) {
		if(VDisplay == 768) ModeIndex = ModeIndex_1360x768[Depth];
	     }
	     break;
	case 1400:
	     if(VGAEngine == SIS_315_VGA) {
		if(VDisplay == 1050) ModeIndex = ModeIndex_1400x1050[Depth];
	     }
	     break;
	case 1600:
	     if(VGAEngine == SIS_315_VGA) {
		if(VDisplay == 1200) ModeIndex = ModeIndex_1600x1200[Depth];
	     }
	     break;
      }

   } else if(VBFlags2 & VB2_SISBRIDGE) {

      switch(HDisplay)
      {
	case 320:
	     if(VDisplay == 200)      ModeIndex = ModeIndex_320x200[Depth];
	     else if(VDisplay == 240) ModeIndex = ModeIndex_320x240[Depth];
	     break;
	case 400:
	     if(LCDwidth >= 800 && LCDheight >= 600) {
		if(VDisplay == 300) ModeIndex = ModeIndex_400x300[Depth];
	     }
	     break;
	case 512:
	     if(LCDwidth >= 1024 && LCDheight >= 768 && LCDwidth != 1152) {
		if(VDisplay == 384) ModeIndex = ModeIndex_512x384[Depth];
	     }
	     break;
	case 640:
	     if(VDisplay == 480)      ModeIndex = ModeIndex_640x480[Depth];
	     else if(VDisplay == 400) ModeIndex = ModeIndex_640x400[Depth];
	     break;
	case 720:
	     if(VGAEngine == SIS_315_VGA) {
		if(VDisplay == 480)      ModeIndex = ModeIndex_720x480[Depth];
		else if(VDisplay == 576) ModeIndex = ModeIndex_720x576[Depth];
	     }
	     break;
	case 768:
	     if(VGAEngine == SIS_315_VGA) {
		if(VDisplay == 576) ModeIndex = ModeIndex_768x576[Depth];
	     }
	     break;
	case 800:
	     if(VDisplay == 600) ModeIndex = ModeIndex_800x600[Depth];
	     if(VGAEngine == SIS_315_VGA) {
		if(VDisplay == 480) ModeIndex = ModeIndex_800x480[Depth];
	     }
	     break;
	case 848:
	     if(VGAEngine == SIS_315_VGA) {
		if(VDisplay == 480) ModeIndex = ModeIndex_848x480[Depth];
	     }
	     break;
	case 856:
	     if(VGAEngine == SIS_315_VGA) {
		if(VDisplay == 480) ModeIndex = ModeIndex_856x480[Depth];
	     }
	     break;
	case 960:
	     if(VGAEngine == SIS_315_VGA) {
		if(VDisplay == 540)      ModeIndex = ModeIndex_960x540[Depth];
		else if(VDisplay == 600) ModeIndex = ModeIndex_960x600[Depth];
	     }
	     break;
	case 1024:
	     if(VDisplay == 768) ModeIndex = ModeIndex_1024x768[Depth];
	     if(VGAEngine == SIS_315_VGA) {
		if(VDisplay == 576) ModeIndex = ModeIndex_1024x576[Depth];
	     }
	     break;
	case 1152:
	     if(VGAEngine == SIS_315_VGA) {
		if(VDisplay == 864) ModeIndex = ModeIndex_1152x864[Depth];
	     }
	     break;
	case 1280:
	     switch(VDisplay) {
	     case 720:
		ModeIndex = ModeIndex_1280x720[Depth];
	     case 768:
		if(VGAEngine == SIS_300_VGA) {
		   ModeIndex = ModeIndex_300_1280x768[Depth];
		} else {
		   ModeIndex = ModeIndex_310_1280x768[Depth];
		}
		break;
	     case 800:
		if(VGAEngine == SIS_315_VGA) {
		   ModeIndex = ModeIndex_1280x800[Depth];
		}
		break;
	     case 854:
		if(VGAEngine == SIS_315_VGA) {
		   ModeIndex = ModeIndex_1280x854[Depth];
		}
		break;
	     case 960:
		ModeIndex = ModeIndex_1280x960[Depth];
		break;
	     case 1024:
		ModeIndex = ModeIndex_1280x1024[Depth];
		break;
	     }
	     break;
	case 1360:
	     if(VGAEngine == SIS_315_VGA) {  /* OVER1280 only? */
		if(VDisplay == 768) ModeIndex = ModeIndex_1360x768[Depth];
	     }
	     break;
	case 1400:
	     if(VGAEngine == SIS_315_VGA) {
		if(VBFlags2 & VB2_LCDOVER1280BRIDGE) {
		   if(VDisplay == 1050) ModeIndex = ModeIndex_1400x1050[Depth];
		}
	     }
	     break;
	case 1600:
	     if(VGAEngine == SIS_315_VGA) {
		if(VBFlags2 & VB2_LCDOVER1280BRIDGE) {
		   if(VDisplay == 1200) ModeIndex = ModeIndex_1600x1200[Depth];
		}
	     }
	     break;
#ifndef VB_FORBID_CRT2LCD_OVER_1600
	case 1680:
	     if(VGAEngine == SIS_315_VGA) {
		if(VBFlags2 & VB2_LCDOVER1280BRIDGE) {
		   if(VDisplay == 1050) ModeIndex = ModeIndex_1680x1050[Depth];
		}
	     }
	     break;
	case 1920:
	     if(VGAEngine == SIS_315_VGA) {
		if(VBFlags2 & VB2_LCDOVER1600BRIDGE) {
		   if(VDisplay == 1440) ModeIndex = ModeIndex_1920x1440[Depth];
		}
	     }
	     break;
	case 2048:
	     if(VGAEngine == SIS_315_VGA) {
		if(VBFlags2 & VB2_LCDOVER1600BRIDGE) {
		   if(VDisplay == 1536) ModeIndex = ModeIndex_310_2048x1536[Depth];
		}
	     }
	     break;
#endif
      }
   }

   return ModeIndex;
}

unsigned short
SiS_GetModeID_TV(int VGAEngine, unsigned int VBFlags, int HDisplay, int VDisplay, int Depth,
			unsigned int VBFlags2)
{
   unsigned short ModeIndex = 0;

   if(VBFlags2 & VB2_CHRONTEL) {

      switch(HDisplay)
      {
	case 512:
	     if(VGAEngine == SIS_315_VGA) {
		if(VDisplay == 384) ModeIndex = ModeIndex_512x384[Depth];
	     }
	     break;
	case 640:
	     if(VDisplay == 480)      ModeIndex = ModeIndex_640x480[Depth];
	     else if(VDisplay == 400) ModeIndex = ModeIndex_640x400[Depth];
	     break;
	case 800:
	     if(VDisplay == 600) ModeIndex = ModeIndex_800x600[Depth];
	     break;
	case 1024:
	     if(VGAEngine == SIS_315_VGA) {
		if(VDisplay == 768) ModeIndex = ModeIndex_1024x768[Depth];
	     }
	     break;
      }

   } else if(VBFlags2 & VB2_SISTVBRIDGE) {

      switch(HDisplay)
      {
	case 320:
	     if(VDisplay == 200)      ModeIndex = ModeIndex_320x200[Depth];
	     else if(VDisplay == 240) ModeIndex = ModeIndex_320x240[Depth];
	     break;
	case 400:
	     if(VDisplay == 300) ModeIndex = ModeIndex_400x300[Depth];
	     break;
	case 512:
	     if( ((VBFlags & TV_YPBPR) && (VBFlags & (TV_YPBPR750P | TV_YPBPR1080I))) ||
		 (VBFlags & TV_HIVISION) 					      ||
		 ((!(VBFlags & (TV_YPBPR | TV_PALM))) && (VBFlags & TV_PAL)) ) {
		if(VDisplay == 384) ModeIndex = ModeIndex_512x384[Depth];
	     }
	     break;
	case 640:
	     if(VDisplay == 480)      ModeIndex = ModeIndex_640x480[Depth];
	     else if(VDisplay == 400) ModeIndex = ModeIndex_640x400[Depth];
	     break;
	case 720:
	     if((!(VBFlags & TV_HIVISION)) && (!((VBFlags & TV_YPBPR) && (VBFlags & TV_YPBPR1080I)))) {
		if(VDisplay == 480) {
		   ModeIndex = ModeIndex_720x480[Depth];
		} else if(VDisplay == 576) {
		   if( ((VBFlags & TV_YPBPR) && (VBFlags & TV_YPBPR750P)) ||
		       ((!(VBFlags & (TV_YPBPR | TV_PALM))) && (VBFlags & TV_PAL)) )
		      ModeIndex = ModeIndex_720x576[Depth];
		}
	     }
             break;
	case 768:
	     if((!(VBFlags & TV_HIVISION)) && (!((VBFlags & TV_YPBPR) && (VBFlags & TV_YPBPR1080I)))) {
		if( ((VBFlags & TV_YPBPR) && (VBFlags & TV_YPBPR750P)) ||
		    ((!(VBFlags & (TV_YPBPR | TV_PALM))) && (VBFlags & TV_PAL)) ) {
		   if(VDisplay == 576) ModeIndex = ModeIndex_768x576[Depth];
		}
             }
	     break;
	case 800:
	     if(VDisplay == 600) ModeIndex = ModeIndex_800x600[Depth];
	     else if(VDisplay == 480) {
		if(!((VBFlags & TV_YPBPR) && (VBFlags & TV_YPBPR750P))) {
		   ModeIndex = ModeIndex_800x480[Depth];
		}
	     }
	     break;
	case 960:
	     if(VGAEngine == SIS_315_VGA) {
		if(VDisplay == 600) {
		   if((VBFlags & TV_HIVISION) || ((VBFlags & TV_YPBPR) && (VBFlags & TV_YPBPR1080I))) {
		      ModeIndex = ModeIndex_960x600[Depth];
		   }
		}
	     }
	     break;
	case 1024:
	     if(VDisplay == 768) {
		if(VBFlags2 & VB2_30xBLV) {
		   ModeIndex = ModeIndex_1024x768[Depth];
		}
	     } else if(VDisplay == 576) {
		if( (VBFlags & TV_HIVISION) ||
		    ((VBFlags & TV_YPBPR) && (VBFlags & TV_YPBPR1080I)) ||
		    ((VBFlags2 & VB2_30xBLV) &&
		     ((!(VBFlags & (TV_YPBPR | TV_PALM))) && (VBFlags & TV_PAL))) ) {
		   ModeIndex = ModeIndex_1024x576[Depth];
		}
	     }
	     break;
	case 1280:
	     if(VDisplay == 720) {
		if((VBFlags & TV_HIVISION) ||
		   ((VBFlags & TV_YPBPR) && (VBFlags & (TV_YPBPR1080I | TV_YPBPR750P)))) {
		   ModeIndex = ModeIndex_1280x720[Depth];
		}
	     } else if(VDisplay == 1024) {
		if((VBFlags & TV_HIVISION) ||
		   ((VBFlags & TV_YPBPR) && (VBFlags & TV_YPBPR1080I))) {
		   ModeIndex = ModeIndex_1280x1024[Depth];
		}
	     }
	     break;
      }
   }
   return ModeIndex;
}

unsigned short
SiS_GetModeID_VGA2(int VGAEngine, unsigned int VBFlags, int HDisplay, int VDisplay, int Depth,
			unsigned int VBFlags2)
{
   if(!(VBFlags2 & VB2_SISVGA2BRIDGE)) return 0;

   if(HDisplay >= 1920) return 0;

   switch(HDisplay)
   {
	case 1600:
		if(VDisplay == 1200) {
			if(VGAEngine != SIS_315_VGA) return 0;
			if(!(VBFlags2 & VB2_30xB)) return 0;
		}
		break;
	case 1680:
		if(VDisplay == 1050) {
			if(VGAEngine != SIS_315_VGA) return 0;
			if(!(VBFlags2 & VB2_30xB)) return 0;
		}
		break;
   }

   return SiS_GetModeID(VGAEngine, 0, HDisplay, VDisplay, Depth, false, 0, 0);
}


/*********************************************/
/*          HELPER: SetReg, GetReg           */
/*********************************************/

void
SiS_SetReg(SISIOADDRESS port, unsigned short index, unsigned short data)
{
   OutPortByte(port, index);
   OutPortByte(port + 1, data);
}

void
SiS_SetRegByte(SISIOADDRESS port, unsigned short data)
{
   OutPortByte(port, data);
}

void
SiS_SetRegShort(SISIOADDRESS port, unsigned short data)
{
   OutPortWord(port, data);
}

void
SiS_SetRegLong(SISIOADDRESS port, unsigned int data)
{
   OutPortLong(port, data);
}

unsigned char
SiS_GetReg(SISIOADDRESS port, unsigned short index)
{
   OutPortByte(port, index);
   return(InPortByte(port + 1));
}

unsigned char
SiS_GetRegByte(SISIOADDRESS port)
{
   return(InPortByte(port));
}

unsigned short
SiS_GetRegShort(SISIOADDRESS port)
{
   return(InPortWord(port));
}

unsigned int
SiS_GetRegLong(SISIOADDRESS port)
{
   return(InPortLong(port));
}

void
SiS_SetRegANDOR(SISIOADDRESS Port, unsigned short Index, unsigned short DataAND, unsigned short DataOR)
{
   unsigned short temp;

   temp = SiS_GetReg(Port, Index);
   temp = (temp & (DataAND)) | DataOR;
   SiS_SetReg(Port, Index, temp);
}

void
SiS_SetRegAND(SISIOADDRESS Port, unsigned short Index, unsigned short DataAND)
{
   unsigned short temp;

   temp = SiS_GetReg(Port, Index);
   temp &= DataAND;
   SiS_SetReg(Port, Index, temp);
}

void
SiS_SetRegOR(SISIOADDRESS Port, unsigned short Index, unsigned short DataOR)
{
   unsigned short temp;

   temp = SiS_GetReg(Port, Index);
   temp |= DataOR;
   SiS_SetReg(Port, Index, temp);
}

/*********************************************/
/*      HELPER: DisplayOn, DisplayOff        */
/*********************************************/

void
SiS_DisplayOn(struct SiS_Private *SiS_Pr)
{
   SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x01,0xDF);
}

void
SiS_DisplayOff(struct SiS_Private *SiS_Pr)
{
   SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x01,0x20);
}


/*********************************************/
/*        HELPER: Init Port Addresses        */
/*********************************************/

void
SiSRegInit(struct SiS_Private *SiS_Pr, SISIOADDRESS BaseAddr)
{
   SiS_Pr->SiS_P3c4 = BaseAddr + 0x14;
   SiS_Pr->SiS_P3d4 = BaseAddr + 0x24;
   SiS_Pr->SiS_P3c0 = BaseAddr + 0x10;
   SiS_Pr->SiS_P3ce = BaseAddr + 0x1e;
   SiS_Pr->SiS_P3c2 = BaseAddr + 0x12;
   SiS_Pr->SiS_P3ca = BaseAddr + 0x1a;
   SiS_Pr->SiS_P3c6 = BaseAddr + 0x16;
   SiS_Pr->SiS_P3c7 = BaseAddr + 0x17;
   SiS_Pr->SiS_P3c8 = BaseAddr + 0x18;
   SiS_Pr->SiS_P3c9 = BaseAddr + 0x19;
   SiS_Pr->SiS_P3cb = BaseAddr + 0x1b;
   SiS_Pr->SiS_P3cc = BaseAddr + 0x1c;
   SiS_Pr->SiS_P3cd = BaseAddr + 0x1d;
   SiS_Pr->SiS_P3da = BaseAddr + 0x2a;
   SiS_Pr->SiS_Part1Port = BaseAddr + SIS_CRT2_PORT_04;
   SiS_Pr->SiS_Part2Port = BaseAddr + SIS_CRT2_PORT_10;
   SiS_Pr->SiS_Part3Port = BaseAddr + SIS_CRT2_PORT_12;
   SiS_Pr->SiS_Part4Port = BaseAddr + SIS_CRT2_PORT_14;
   SiS_Pr->SiS_Part5Port = BaseAddr + SIS_CRT2_PORT_14 + 2;
   SiS_Pr->SiS_DDC_Port  = BaseAddr + 0x14;
   SiS_Pr->SiS_VidCapt   = BaseAddr + SIS_VIDEO_CAPTURE;
   SiS_Pr->SiS_VidPlay   = BaseAddr + SIS_VIDEO_PLAYBACK;
}

/*********************************************/
/*             HELPER: GetSysFlags           */
/*********************************************/

static void
SiS_GetSysFlags(struct SiS_Private *SiS_Pr)
{
   unsigned char cr5f, temp1, temp2;

   /* 661 and newer: NEVER write non-zero to SR11[7:4] */
   /* (SR11 is used for DDC and in enable/disablebridge) */
   SiS_Pr->SiS_SensibleSR11 = false;
   SiS_Pr->SiS_MyCR63 = 0x63;
   if(SiS_Pr->ChipType >= SIS_330) {
      SiS_Pr->SiS_MyCR63 = 0x53;
      if(SiS_Pr->ChipType >= SIS_661) {
         SiS_Pr->SiS_SensibleSR11 = true;
      }
   }

   /* You should use the macros, not these flags directly */

   SiS_Pr->SiS_SysFlags = 0;
   if(SiS_Pr->ChipType == SIS_650) {
      cr5f = SiS_GetReg(SiS_Pr->SiS_P3d4,0x5f) & 0xf0;
      SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x5c,0x07);
      temp1 = SiS_GetReg(SiS_Pr->SiS_P3d4,0x5c) & 0xf8;
      SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x5c,0xf8);
      temp2 = SiS_GetReg(SiS_Pr->SiS_P3d4,0x5c) & 0xf8;
      if((!temp1) || (temp2)) {
	 switch(cr5f) {
	    case 0x80:
	    case 0x90:
	    case 0xc0:
	       SiS_Pr->SiS_SysFlags |= SF_IsM650;
	       break;
	    case 0xa0:
	    case 0xb0:
	    case 0xe0:
	       SiS_Pr->SiS_SysFlags |= SF_Is651;
	       break;
	 }
      } else {
	 switch(cr5f) {
	    case 0x90:
	       temp1 = SiS_GetReg(SiS_Pr->SiS_P3d4,0x5c) & 0xf8;
	       switch(temp1) {
		  case 0x00: SiS_Pr->SiS_SysFlags |= SF_IsM652; break;
		  case 0x40: SiS_Pr->SiS_SysFlags |= SF_IsM653; break;
		  default:   SiS_Pr->SiS_SysFlags |= SF_IsM650; break;
	       }
	       break;
	    case 0xb0:
	       SiS_Pr->SiS_SysFlags |= SF_Is652;
	       break;
	    default:
	       SiS_Pr->SiS_SysFlags |= SF_IsM650;
	       break;
	 }
      }
   }

   if(SiS_Pr->ChipType >= SIS_760 && SiS_Pr->ChipType <= SIS_761) {
      if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x78) & 0x30) {
         SiS_Pr->SiS_SysFlags |= SF_760LFB;
      }
      if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x79) & 0xf0) {
         SiS_Pr->SiS_SysFlags |= SF_760UMA;
      }
   }
}

/*********************************************/
/*         HELPER: Init PCI & Engines        */
/*********************************************/

static void
SiSInitPCIetc(struct SiS_Private *SiS_Pr)
{
   switch(SiS_Pr->ChipType) {
#ifdef SIS300
   case SIS_300:
   case SIS_540:
   case SIS_630:
   case SIS_730:
      /* Set - PCI LINEAR ADDRESSING ENABLE (0x80)
       *     - RELOCATED VGA IO ENABLED (0x20)
       *     - MMIO ENABLED (0x01)
       * Leave other bits untouched.
       */
      SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x20,0xa1);
      /*  - Enable 2D (0x40)
       *  - Enable 3D (0x02)
       *  - Enable 3D Vertex command fetch (0x10) ?
       *  - Enable 3D command parser (0x08) ?
       */
      SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x1E,0x5A);
      break;
#endif
#ifdef SIS315H
   case SIS_315H:
   case SIS_315:
   case SIS_315PRO:
   case SIS_650:
   case SIS_740:
   case SIS_330:
   case SIS_661:
   case SIS_741:
   case SIS_660:
   case SIS_760:
   case SIS_761:
   case SIS_340:
   case XGI_40:
      /* See above */
      SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x20,0xa1);
      /*  - Enable 3D G/L transformation engine (0x80)
       *  - Enable 2D (0x40)
       *  - Enable 3D vertex command fetch (0x10)
       *  - Enable 3D command parser (0x08)
       *  - Enable 3D (0x02)
       */
      SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x1E,0xDA);
      break;
   case XGI_20:
   case SIS_550:
      /* See above */
      SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x20,0xa1);
      /* No 3D engine ! */
      /*  - Enable 2D (0x40)
       *  - disable 3D
       */
      SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x1E,0x60,0x40);
      break;
#endif
   default:
      break;
   }
}

/*********************************************/
/*             HELPER: SetLVDSetc            */
/*********************************************/

#ifdef SIS_LINUX_KERNEL
static
#endif
void
SiSSetLVDSetc(struct SiS_Private *SiS_Pr)
{
   unsigned short temp;

   SiS_Pr->SiS_IF_DEF_LVDS = 0;
   SiS_Pr->SiS_IF_DEF_TRUMPION = 0;
   SiS_Pr->SiS_IF_DEF_CH70xx = 0;
   SiS_Pr->SiS_IF_DEF_CONEX = 0;

   SiS_Pr->SiS_ChrontelInit = 0;

   if(SiS_Pr->ChipType == XGI_20) return;

   /* Check for SiS30x first */
   temp = SiS_GetReg(SiS_Pr->SiS_Part4Port,0x00);
   if((temp == 1) || (temp == 2)) return;

   switch(SiS_Pr->ChipType) {
#ifdef SIS300
   case SIS_540:
   case SIS_630:
   case SIS_730:
	temp = (SiS_GetReg(SiS_Pr->SiS_P3d4,0x37) & 0x0e) >> 1;
	if((temp >= 2) && (temp <= 5))	SiS_Pr->SiS_IF_DEF_LVDS = 1;
	if(temp == 3)			SiS_Pr->SiS_IF_DEF_TRUMPION = 1;
	if((temp == 4) || (temp == 5)) {
		/* Save power status (and error check) - UNUSED */
		SiS_Pr->SiS_Backup70xx = SiS_GetCH700x(SiS_Pr, 0x0e);
		SiS_Pr->SiS_IF_DEF_CH70xx = 1;
	}
	break;
#endif
#ifdef SIS315H
   case SIS_550:
   case SIS_650:
   case SIS_740:
   case SIS_330:
	temp = (SiS_GetReg(SiS_Pr->SiS_P3d4,0x37) & 0x0e) >> 1;
	if((temp >= 2) && (temp <= 3))	SiS_Pr->SiS_IF_DEF_LVDS = 1;
	if(temp == 3)			SiS_Pr->SiS_IF_DEF_CH70xx = 2;
	break;
   case SIS_661:
   case SIS_741:
   case SIS_660:
   case SIS_760:
   case SIS_761:
   case SIS_340:
   case XGI_20:
   case XGI_40:
	temp = (SiS_GetReg(SiS_Pr->SiS_P3d4,0x38) & 0xe0) >> 5;
	if((temp >= 2) && (temp <= 3)) 	SiS_Pr->SiS_IF_DEF_LVDS = 1;
	if(temp == 3)			SiS_Pr->SiS_IF_DEF_CH70xx = 2;
	if(temp == 4)			SiS_Pr->SiS_IF_DEF_CONEX = 1;  /* Not yet supported */
	break;
#endif
   default:
	break;
   }
}

/*********************************************/
/*          HELPER: Enable DSTN/FSTN         */
/*********************************************/

void
SiS_SetEnableDstn(struct SiS_Private *SiS_Pr, int enable)
{
   SiS_Pr->SiS_IF_DEF_DSTN = enable ? 1 : 0;
}

void
SiS_SetEnableFstn(struct SiS_Private *SiS_Pr, int enable)
{
   SiS_Pr->SiS_IF_DEF_FSTN = enable ? 1 : 0;
}

/*********************************************/
/*            HELPER: Get modeflag           */
/*********************************************/

unsigned short
SiS_GetModeFlag(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
		unsigned short ModeIdIndex)
{
   if(SiS_Pr->UseCustomMode) {
      return SiS_Pr->CModeFlag;
   } else if(ModeNo <= 0x13) {
      return SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
   } else {
      return SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
   }
}

/*********************************************/
/*        HELPER: Determine ROM usage        */
/*********************************************/

bool
SiSDetermineROMLayout661(struct SiS_Private *SiS_Pr)
{
   unsigned char  *ROMAddr  = SiS_Pr->VirtualRomBase;
   unsigned short romversoffs, romvmaj = 1, romvmin = 0;

   if(SiS_Pr->ChipType >= XGI_20) {
      /* XGI ROMs don't qualify */
      return false;
   } else if(SiS_Pr->ChipType >= SIS_761) {
      /* I very much assume 761, 340 and newer will use new layout */
      return true;
   } else if(SiS_Pr->ChipType >= SIS_661) {
      if((ROMAddr[0x1a] == 'N') &&
	 (ROMAddr[0x1b] == 'e') &&
	 (ROMAddr[0x1c] == 'w') &&
	 (ROMAddr[0x1d] == 'V')) {
	 return true;
      }
      romversoffs = ROMAddr[0x16] | (ROMAddr[0x17] << 8);
      if(romversoffs) {
	 if((ROMAddr[romversoffs+1] == '.') || (ROMAddr[romversoffs+4] == '.')) {
	    romvmaj = ROMAddr[romversoffs] - '0';
	    romvmin = ((ROMAddr[romversoffs+2] -'0') * 10) + (ROMAddr[romversoffs+3] - '0');
	 }
      }
      if((romvmaj != 0) || (romvmin >= 92)) {
	 return true;
      }
   } else if(IS_SIS650740) {
      if((ROMAddr[0x1a] == 'N') &&
	 (ROMAddr[0x1b] == 'e') &&
	 (ROMAddr[0x1c] == 'w') &&
	 (ROMAddr[0x1d] == 'V')) {
	 return true;
      }
   }
   return false;
}

static void
SiSDetermineROMUsage(struct SiS_Private *SiS_Pr)
{
   unsigned char  *ROMAddr  = SiS_Pr->VirtualRomBase;
   unsigned short romptr = 0;

   SiS_Pr->SiS_UseROM = false;
   SiS_Pr->SiS_ROMNew = false;
   SiS_Pr->SiS_PWDOffset = 0;

   if(SiS_Pr->ChipType >= XGI_20) return;

   if((ROMAddr) && (SiS_Pr->UseROM)) {
      if(SiS_Pr->ChipType == SIS_300) {
	 /* 300: We check if the code starts below 0x220 by
	  * checking the jmp instruction at the beginning
	  * of the BIOS image.
	  */
	 if((ROMAddr[3] == 0xe9) && ((ROMAddr[5] << 8) | ROMAddr[4]) > 0x21a)
	    SiS_Pr->SiS_UseROM = true;
      } else if(SiS_Pr->ChipType < SIS_315H) {
	 /* Sony's VAIO BIOS 1.09 follows the standard, so perhaps
	  * the others do as well
	  */
	 SiS_Pr->SiS_UseROM = true;
      } else {
	 /* 315/330 series stick to the standard(s) */
	 SiS_Pr->SiS_UseROM = true;
	 if((SiS_Pr->SiS_ROMNew = SiSDetermineROMLayout661(SiS_Pr))) {
	    SiS_Pr->SiS_EMIOffset = 14;
	    SiS_Pr->SiS_PWDOffset = 17;
	    SiS_Pr->SiS661LCD2TableSize = 36;
	    /* Find out about LCD data table entry size */
	    if((romptr = SISGETROMW(0x0102))) {
	       if(ROMAddr[romptr + (32 * 16)] == 0xff)
		  SiS_Pr->SiS661LCD2TableSize = 32;
	       else if(ROMAddr[romptr + (34 * 16)] == 0xff)
		  SiS_Pr->SiS661LCD2TableSize = 34;
	       else if(ROMAddr[romptr + (36 * 16)] == 0xff)	   /* 0.94, 2.05.00+ */
		  SiS_Pr->SiS661LCD2TableSize = 36;
	       else if( (ROMAddr[romptr + (38 * 16)] == 0xff) ||   /* 2.00.00 - 2.02.00 */
		 	(ROMAddr[0x6F] & 0x01) ) {		   /* 2.03.00 - <2.05.00 */
		  SiS_Pr->SiS661LCD2TableSize = 38;		   /* UMC data layout abandoned at 2.05.00 */
		  SiS_Pr->SiS_EMIOffset = 16;
		  SiS_Pr->SiS_PWDOffset = 19;
	       }
	    }
	 }
      }
   }
}

/*********************************************/
/*        HELPER: SET SEGMENT REGISTERS      */
/*********************************************/

static void
SiS_SetSegRegLower(struct SiS_Private *SiS_Pr, unsigned short value)
{
   unsigned short temp;

   value &= 0x00ff;
   temp = SiS_GetRegByte(SiS_Pr->SiS_P3cb) & 0xf0;
   temp |= (value >> 4);
   SiS_SetRegByte(SiS_Pr->SiS_P3cb, temp);
   temp = SiS_GetRegByte(SiS_Pr->SiS_P3cd) & 0xf0;
   temp |= (value & 0x0f);
   SiS_SetRegByte(SiS_Pr->SiS_P3cd, temp);
}

static void
SiS_SetSegRegUpper(struct SiS_Private *SiS_Pr, unsigned short value)
{
   unsigned short temp;

   value &= 0x00ff;
   temp = SiS_GetRegByte(SiS_Pr->SiS_P3cb) & 0x0f;
   temp |= (value & 0xf0);
   SiS_SetRegByte(SiS_Pr->SiS_P3cb, temp);
   temp = SiS_GetRegByte(SiS_Pr->SiS_P3cd) & 0x0f;
   temp |= (value << 4);
   SiS_SetRegByte(SiS_Pr->SiS_P3cd, temp);
}

static void
SiS_SetSegmentReg(struct SiS_Private *SiS_Pr, unsigned short value)
{
   SiS_SetSegRegLower(SiS_Pr, value);
   SiS_SetSegRegUpper(SiS_Pr, value);
}

static void
SiS_ResetSegmentReg(struct SiS_Private *SiS_Pr)
{
   SiS_SetSegmentReg(SiS_Pr, 0);
}

static void
SiS_SetSegmentRegOver(struct SiS_Private *SiS_Pr, unsigned short value)
{
   unsigned short temp = value >> 8;

   temp &= 0x07;
   temp |= (temp << 4);
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x1d,temp);
   SiS_SetSegmentReg(SiS_Pr, value);
}

static void
SiS_ResetSegmentRegOver(struct SiS_Private *SiS_Pr)
{
   SiS_SetSegmentRegOver(SiS_Pr, 0);
}

static void
SiS_ResetSegmentRegisters(struct SiS_Private *SiS_Pr)
{
   if((IS_SIS65x) || (SiS_Pr->ChipType >= SIS_661)) {
      SiS_ResetSegmentReg(SiS_Pr);
      SiS_ResetSegmentRegOver(SiS_Pr);
   }
}

/*********************************************/
/*             HELPER: GetVBType             */
/*********************************************/

#ifdef SIS_LINUX_KERNEL
static
#endif
void
SiS_GetVBType(struct SiS_Private *SiS_Pr)
{
   unsigned short flag = 0, rev = 0, nolcd = 0;
   unsigned short p4_0f, p4_25, p4_27;

   SiS_Pr->SiS_VBType = 0;

   if((SiS_Pr->SiS_IF_DEF_LVDS) || (SiS_Pr->SiS_IF_DEF_CONEX))
      return;

   if(SiS_Pr->ChipType == XGI_20)
      return;

   flag = SiS_GetReg(SiS_Pr->SiS_Part4Port,0x00);

   if(flag > 3)
      return;

   rev = SiS_GetReg(SiS_Pr->SiS_Part4Port,0x01);

   if(flag >= 2) {
      SiS_Pr->SiS_VBType = VB_SIS302B;
   } else if(flag == 1) {
      if(rev >= 0xC0) {
	 SiS_Pr->SiS_VBType = VB_SIS301C;
      } else if(rev >= 0xB0) {
	 SiS_Pr->SiS_VBType = VB_SIS301B;
	 /* Check if 30xB DH version (no LCD support, use Panel Link instead) */
	 nolcd = SiS_GetReg(SiS_Pr->SiS_Part4Port,0x23);
	 if(!(nolcd & 0x02)) SiS_Pr->SiS_VBType |= VB_NoLCD;
      } else {
	 SiS_Pr->SiS_VBType = VB_SIS301;
      }
   }
   if(SiS_Pr->SiS_VBType & (VB_SIS301B | VB_SIS301C | VB_SIS302B)) {
      if(rev >= 0xE0) {
	 flag = SiS_GetReg(SiS_Pr->SiS_Part4Port,0x39);
	 if(flag == 0xff) SiS_Pr->SiS_VBType = VB_SIS302LV;
	 else 	 	  SiS_Pr->SiS_VBType = VB_SIS301C;  /* VB_SIS302ELV; */
      } else if(rev >= 0xD0) {
	 SiS_Pr->SiS_VBType = VB_SIS301LV;
      }
   }
   if(SiS_Pr->SiS_VBType & (VB_SIS301C | VB_SIS301LV | VB_SIS302LV | VB_SIS302ELV)) {
      p4_0f = SiS_GetReg(SiS_Pr->SiS_Part4Port,0x0f);
      p4_25 = SiS_GetReg(SiS_Pr->SiS_Part4Port,0x25);
      p4_27 = SiS_GetReg(SiS_Pr->SiS_Part4Port,0x27);
      SiS_SetRegAND(SiS_Pr->SiS_Part4Port,0x0f,0x7f);
      SiS_SetRegOR(SiS_Pr->SiS_Part4Port,0x25,0x08);
      SiS_SetRegAND(SiS_Pr->SiS_Part4Port,0x27,0xfd);
      if(SiS_GetReg(SiS_Pr->SiS_Part4Port,0x26) & 0x08) {
         SiS_Pr->SiS_VBType |= VB_UMC;
      }
      SiS_SetReg(SiS_Pr->SiS_Part4Port,0x27,p4_27);
      SiS_SetReg(SiS_Pr->SiS_Part4Port,0x25,p4_25);
      SiS_SetReg(SiS_Pr->SiS_Part4Port,0x0f,p4_0f);
   }
}

/*********************************************/
/*           HELPER: Check RAM size          */
/*********************************************/

#ifdef SIS_LINUX_KERNEL
static bool
SiS_CheckMemorySize(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
		unsigned short ModeIdIndex)
{
   unsigned short AdapterMemSize = SiS_Pr->VideoMemorySize / (1024*1024);
   unsigned short modeflag = SiS_GetModeFlag(SiS_Pr, ModeNo, ModeIdIndex);
   unsigned short memorysize = ((modeflag & MemoryInfoFlag) >> MemorySizeShift) + 1;

   if(!AdapterMemSize) return true;

   if(AdapterMemSize < memorysize) return false;
   return true;
}
#endif

/*********************************************/
/*           HELPER: Get DRAM type           */
/*********************************************/

#ifdef SIS315H
static unsigned char
SiS_Get310DRAMType(struct SiS_Private *SiS_Pr)
{
   unsigned char data;

   if((*SiS_Pr->pSiS_SoftSetting) & SoftDRAMType) {
      data = (*SiS_Pr->pSiS_SoftSetting) & 0x03;
   } else {
      if(SiS_Pr->ChipType >= XGI_20) {
         /* Do I need this? SR17 seems to be zero anyway... */
	 data = 0;
      } else if(SiS_Pr->ChipType >= SIS_340) {
	 /* TODO */
	 data = 0;
      } if(SiS_Pr->ChipType >= SIS_661) {
	 if(SiS_Pr->SiS_ROMNew) {
	    data = ((SiS_GetReg(SiS_Pr->SiS_P3d4,0x78) & 0xc0) >> 6);
	 } else {
	    data = SiS_GetReg(SiS_Pr->SiS_P3d4,0x78) & 0x07;
	 }
      } else if(IS_SIS550650740) {
	 data = SiS_GetReg(SiS_Pr->SiS_P3c4,0x13) & 0x07;
      } else {	/* 315, 330 */
	 data = SiS_GetReg(SiS_Pr->SiS_P3c4,0x3a) & 0x03;
	 if(SiS_Pr->ChipType == SIS_330) {
	    if(data > 1) {
	       switch(SiS_GetReg(SiS_Pr->SiS_P3d4,0x5f) & 0x30) {
	       case 0x00: data = 1; break;
	       case 0x10: data = 3; break;
	       case 0x20: data = 3; break;
	       case 0x30: data = 2; break;
	       }
	    } else {
	       data = 0;
	    }
	 }
      }
   }

   return data;
}

static unsigned short
SiS_GetMCLK(struct SiS_Private *SiS_Pr)
{
   unsigned char  *ROMAddr = SiS_Pr->VirtualRomBase;
   unsigned short index;

   index = SiS_Get310DRAMType(SiS_Pr);
   if(SiS_Pr->ChipType >= SIS_661) {
      if(SiS_Pr->SiS_ROMNew) {
	 return((unsigned short)(SISGETROMW((0x90 + (index * 5) + 3))));
      }
      return(SiS_Pr->SiS_MCLKData_0[index].CLOCK);
   } else if(index >= 4) {
      return(SiS_Pr->SiS_MCLKData_1[index - 4].CLOCK);
   } else {
      return(SiS_Pr->SiS_MCLKData_0[index].CLOCK);
   }
}
#endif

/*********************************************/
/*           HELPER: ClearBuffer             */
/*********************************************/

#ifdef SIS_LINUX_KERNEL
static void
SiS_ClearBuffer(struct SiS_Private *SiS_Pr, unsigned short ModeNo)
{
   unsigned char  SISIOMEMTYPE *memaddr = SiS_Pr->VideoMemoryAddress;
   unsigned int   memsize = SiS_Pr->VideoMemorySize;
   unsigned short SISIOMEMTYPE *pBuffer;
   int i;

   if(!memaddr || !memsize) return;

   if(SiS_Pr->SiS_ModeType >= ModeEGA) {
      if(ModeNo > 0x13) {
	 SiS_SetMemory(memaddr, memsize, 0);
      } else {
	 pBuffer = (unsigned short SISIOMEMTYPE *)memaddr;
	 for(i = 0; i < 0x4000; i++) writew(0x0000, &pBuffer[i]);
      }
   } else if(SiS_Pr->SiS_ModeType < ModeCGA) {
      pBuffer = (unsigned short SISIOMEMTYPE *)memaddr;
      for(i = 0; i < 0x4000; i++) writew(0x0720, &pBuffer[i]);
   } else {
      SiS_SetMemory(memaddr, 0x8000, 0);
   }
}
#endif

/*********************************************/
/*           HELPER: SearchModeID            */
/*********************************************/

bool
SiS_SearchModeID(struct SiS_Private *SiS_Pr, unsigned short *ModeNo,
		unsigned short *ModeIdIndex)
{
   unsigned char VGAINFO = SiS_Pr->SiS_VGAINFO;

   if((*ModeNo) <= 0x13) {

      if((*ModeNo) <= 0x05) (*ModeNo) |= 0x01;

      for((*ModeIdIndex) = 0; ;(*ModeIdIndex)++) {
	 if(SiS_Pr->SiS_SModeIDTable[(*ModeIdIndex)].St_ModeID == (*ModeNo)) break;
	 if(SiS_Pr->SiS_SModeIDTable[(*ModeIdIndex)].St_ModeID == 0xFF) return false;
      }

      if((*ModeNo) == 0x07) {
	  if(VGAINFO & 0x10) (*ModeIdIndex)++;   /* 400 lines */
	  /* else 350 lines */
      }
      if((*ModeNo) <= 0x03) {
	 if(!(VGAINFO & 0x80)) (*ModeIdIndex)++;
	 if(VGAINFO & 0x10)    (*ModeIdIndex)++; /* 400 lines  */
	 /* else 350 lines  */
      }
      /* else 200 lines  */

   } else {

      for((*ModeIdIndex) = 0; ;(*ModeIdIndex)++) {
	 if(SiS_Pr->SiS_EModeIDTable[(*ModeIdIndex)].Ext_ModeID == (*ModeNo)) break;
	 if(SiS_Pr->SiS_EModeIDTable[(*ModeIdIndex)].Ext_ModeID == 0xFF) return false;
      }

   }
   return true;
}

/*********************************************/
/*            HELPER: GetModePtr             */
/*********************************************/

unsigned short
SiS_GetModePtr(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex)
{
   unsigned short index;

   if(ModeNo <= 0x13) {
      index = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_StTableIndex;
   } else {
      if(SiS_Pr->SiS_ModeType <= ModeEGA) index = 0x1B;
      else index = 0x0F;
   }
   return index;
}

/*********************************************/
/*         HELPERS: Get some indices         */
/*********************************************/

unsigned short
SiS_GetRefCRTVCLK(struct SiS_Private *SiS_Pr, unsigned short Index, int UseWide)
{
   if(SiS_Pr->SiS_RefIndex[Index].Ext_InfoFlag & HaveWideTiming) {
      if(UseWide == 1) {
         return SiS_Pr->SiS_RefIndex[Index].Ext_CRTVCLK_WIDE;
      } else {
         return SiS_Pr->SiS_RefIndex[Index].Ext_CRTVCLK_NORM;
      }
   } else {
      return SiS_Pr->SiS_RefIndex[Index].Ext_CRTVCLK;
   }
}

unsigned short
SiS_GetRefCRT1CRTC(struct SiS_Private *SiS_Pr, unsigned short Index, int UseWide)
{
   if(SiS_Pr->SiS_RefIndex[Index].Ext_InfoFlag & HaveWideTiming) {
      if(UseWide == 1) {
         return SiS_Pr->SiS_RefIndex[Index].Ext_CRT1CRTC_WIDE;
      } else {
         return SiS_Pr->SiS_RefIndex[Index].Ext_CRT1CRTC_NORM;
      }
   } else {
      return SiS_Pr->SiS_RefIndex[Index].Ext_CRT1CRTC;
   }
}

/*********************************************/
/*           HELPER: LowModeTests            */
/*********************************************/

static bool
SiS_DoLowModeTest(struct SiS_Private *SiS_Pr, unsigned short ModeNo)
{
   unsigned short temp, temp1, temp2;

   if((ModeNo != 0x03) && (ModeNo != 0x10) && (ModeNo != 0x12))
      return true;
   temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x11);
   SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x11,0x80);
   temp1 = SiS_GetReg(SiS_Pr->SiS_P3d4,0x00);
   SiS_SetReg(SiS_Pr->SiS_P3d4,0x00,0x55);
   temp2 = SiS_GetReg(SiS_Pr->SiS_P3d4,0x00);
   SiS_SetReg(SiS_Pr->SiS_P3d4,0x00,temp1);
   SiS_SetReg(SiS_Pr->SiS_P3d4,0x11,temp);
   if((SiS_Pr->ChipType >= SIS_315H) ||
      (SiS_Pr->ChipType == SIS_300)) {
      if(temp2 == 0x55) return false;
      else return true;
   } else {
      if(temp2 != 0x55) return true;
      else {
	 SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x35,0x01);
	 return false;
      }
   }
}

static void
SiS_SetLowModeTest(struct SiS_Private *SiS_Pr, unsigned short ModeNo)
{
   if(SiS_DoLowModeTest(SiS_Pr, ModeNo)) {
      SiS_Pr->SiS_SetFlag |= LowModeTests;
   }
}

/*********************************************/
/*        HELPER: OPEN/CLOSE CRT1 CRTC       */
/*********************************************/

static void
SiS_OpenCRTC(struct SiS_Private *SiS_Pr)
{
   if(IS_SIS650) {
      SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x51,0x1f);
      if(IS_SIS651) SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x51,0x20);
      SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x56,0xe7);
   } else if(IS_SIS661741660760) {
      SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x61,0xf7);
      SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x51,0x1f);
      SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x56,0xe7);
      if(!SiS_Pr->SiS_ROMNew) {
	 SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x3a,0xef);
      }
   }
}

static void
SiS_CloseCRTC(struct SiS_Private *SiS_Pr)
{
#if 0 /* This locks some CRTC registers. We don't want that. */
   unsigned short temp1 = 0, temp2 = 0;

   if(IS_SIS661741660760) {
      if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA) {
         temp1 = 0xa0; temp2 = 0x08;
      }
      SiS_SetRegANDOR(SiS_Pr->SiS_P3d4,0x51,0x1f,temp1);
      SiS_SetRegANDOR(SiS_Pr->SiS_P3d4,0x56,0xe7,temp2);
   }
#endif
}

static void
SiS_HandleCRT1(struct SiS_Private *SiS_Pr)
{
   /* Enable CRT1 gating */
   SiS_SetRegAND(SiS_Pr->SiS_P3d4,SiS_Pr->SiS_MyCR63,0xbf);
#if 0
   if(!(SiS_GetReg(SiS_Pr->SiS_P3c4,0x15) & 0x01)) {
      if((SiS_GetReg(SiS_Pr->SiS_P3c4,0x15) & 0x0a) ||
         (SiS_GetReg(SiS_Pr->SiS_P3c4,0x16) & 0x01)) {
         SiS_SetRegOR(SiS_Pr->SiS_P3d4,SiS_Pr->SiS_MyCR63,0x40);
      }
   }
#endif
}

/*********************************************/
/*           HELPER: GetColorDepth           */
/*********************************************/

unsigned short
SiS_GetColorDepth(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
		unsigned short ModeIdIndex)
{
   static const unsigned short ColorDepth[6] = { 1, 2, 4, 4, 6, 8 };
   unsigned short modeflag;
   short index;

   /* Do NOT check UseCustomMode, will skrew up FIFO */
   if(ModeNo == 0xfe) {
      modeflag = SiS_Pr->CModeFlag;
   } else if(ModeNo <= 0x13) {
      modeflag = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
   } else {
      modeflag = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
   }

   index = (modeflag & ModeTypeMask) - ModeEGA;
   if(index < 0) index = 0;
   return ColorDepth[index];
}

/*********************************************/
/*             HELPER: GetOffset             */
/*********************************************/

unsigned short
SiS_GetOffset(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
		unsigned short ModeIdIndex, unsigned short RRTI)
{
   unsigned short xres, temp, colordepth, infoflag;

   if(SiS_Pr->UseCustomMode) {
      infoflag = SiS_Pr->CInfoFlag;
      xres = SiS_Pr->CHDisplay;
   } else {
      infoflag = SiS_Pr->SiS_RefIndex[RRTI].Ext_InfoFlag;
      xres = SiS_Pr->SiS_RefIndex[RRTI].XRes;
   }

   colordepth = SiS_GetColorDepth(SiS_Pr, ModeNo, ModeIdIndex);

   temp = xres / 16;
   if(infoflag & InterlaceMode) temp <<= 1;
   temp *= colordepth;
   if(xres % 16) temp += (colordepth >> 1);

   return temp;
}

/*********************************************/
/*                   SEQ                     */
/*********************************************/

static void
SiS_SetSeqRegs(struct SiS_Private *SiS_Pr, unsigned short StandTableIndex)
{
   unsigned char SRdata;
   int i;

   SiS_SetReg(SiS_Pr->SiS_P3c4,0x00,0x03);

   /* or "display off"  */
   SRdata = SiS_Pr->SiS_StandTable[StandTableIndex].SR[0] | 0x20;

   /* determine whether to force x8 dotclock */
   if((SiS_Pr->SiS_VBType & VB_SISVB) || (SiS_Pr->SiS_IF_DEF_LVDS)) {

      if(SiS_Pr->SiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToTV)) {
         if(SiS_Pr->SiS_VBInfo & SetInSlaveMode)    SRdata |= 0x01;
      } else if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA) SRdata |= 0x01;

   }

   SiS_SetReg(SiS_Pr->SiS_P3c4,0x01,SRdata);

   for(i = 2; i <= 4; i++) {
      SRdata = SiS_Pr->SiS_StandTable[StandTableIndex].SR[i - 1];
      SiS_SetReg(SiS_Pr->SiS_P3c4,i,SRdata);
   }
}

/*********************************************/
/*                  MISC                     */
/*********************************************/

static void
SiS_SetMiscRegs(struct SiS_Private *SiS_Pr, unsigned short StandTableIndex)
{
   unsigned char Miscdata;

   Miscdata = SiS_Pr->SiS_StandTable[StandTableIndex].MISC;

   if(SiS_Pr->ChipType < SIS_661) {
      if(SiS_Pr->SiS_VBType & VB_SIS30xBLV) {
	 if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA) {
	   Miscdata |= 0x0C;
	 }
      }
   }

   SiS_SetRegByte(SiS_Pr->SiS_P3c2,Miscdata);
}

/*********************************************/
/*                  CRTC                     */
/*********************************************/

static void
SiS_SetCRTCRegs(struct SiS_Private *SiS_Pr, unsigned short StandTableIndex)
{
   unsigned char  CRTCdata;
   unsigned short i;

   /* Unlock CRTC */
   SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x11,0x7f);

   for(i = 0; i <= 0x18; i++) {
      CRTCdata = SiS_Pr->SiS_StandTable[StandTableIndex].CRTC[i];
      SiS_SetReg(SiS_Pr->SiS_P3d4,i,CRTCdata);
   }

   if(SiS_Pr->ChipType >= SIS_661) {
      SiS_OpenCRTC(SiS_Pr);
      for(i = 0x13; i <= 0x14; i++) {
	 CRTCdata = SiS_Pr->SiS_StandTable[StandTableIndex].CRTC[i];
	 SiS_SetReg(SiS_Pr->SiS_P3d4,i,CRTCdata);
      }
   } else if( ( (SiS_Pr->ChipType == SIS_630) ||
	        (SiS_Pr->ChipType == SIS_730) )  &&
	      (SiS_Pr->ChipRevision >= 0x30) ) {
      if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
	 if(SiS_Pr->SiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToTV)) {
	    SiS_SetReg(SiS_Pr->SiS_P3d4,0x18,0xFE);
	 }
      }
   }
}

/*********************************************/
/*                   ATT                     */
/*********************************************/

static void
SiS_SetATTRegs(struct SiS_Private *SiS_Pr, unsigned short StandTableIndex)
{
   unsigned char  ARdata;
   unsigned short i;

   for(i = 0; i <= 0x13; i++) {
      ARdata = SiS_Pr->SiS_StandTable[StandTableIndex].ATTR[i];

      if(i == 0x13) {
	 /* Pixel shift. If screen on LCD or TV is shifted left or right,
	  * this might be the cause.
	  */
	 if(SiS_Pr->SiS_VBType & VB_SIS30xBLV) {
	    if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA) ARdata = 0;
	 }
	 if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
	    if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
	       if(SiS_Pr->SiS_VBInfo & SetCRT2ToTV) {
		  if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) ARdata = 0;
	       }
	    }
	 }
	 if(SiS_Pr->ChipType >= SIS_661) {
	    if(SiS_Pr->SiS_VBInfo & (SetCRT2ToTV | SetCRT2ToLCD)) {
	       if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) ARdata = 0;
	    }
	 } else if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) {
	    if(SiS_Pr->ChipType >= SIS_315H) {
	       if(IS_SIS550650740660) {
		  /* 315, 330 don't do this */
		  if(SiS_Pr->SiS_VBType & VB_SIS30xB) {
		     if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) ARdata = 0;
		  } else {
		     ARdata = 0;
		  }
	       }
	    } else {
	       if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) ARdata = 0;
	    }
	 }
      }
      SiS_GetRegByte(SiS_Pr->SiS_P3da);		/* reset 3da  */
      SiS_SetRegByte(SiS_Pr->SiS_P3c0,i);	/* set index  */
      SiS_SetRegByte(SiS_Pr->SiS_P3c0,ARdata);	/* set data   */
   }

   SiS_GetRegByte(SiS_Pr->SiS_P3da);		/* reset 3da  */
   SiS_SetRegByte(SiS_Pr->SiS_P3c0,0x14);	/* set index  */
   SiS_SetRegByte(SiS_Pr->SiS_P3c0,0x00);	/* set data   */

   SiS_GetRegByte(SiS_Pr->SiS_P3da);
   SiS_SetRegByte(SiS_Pr->SiS_P3c0,0x20);	/* Enable Attribute  */
   SiS_GetRegByte(SiS_Pr->SiS_P3da);
}

/*********************************************/
/*                   GRC                     */
/*********************************************/

static void
SiS_SetGRCRegs(struct SiS_Private *SiS_Pr, unsigned short StandTableIndex)
{
   unsigned char  GRdata;
   unsigned short i;

   for(i = 0; i <= 0x08; i++) {
      GRdata = SiS_Pr->SiS_StandTable[StandTableIndex].GRC[i];
      SiS_SetReg(SiS_Pr->SiS_P3ce,i,GRdata);
   }

   if(SiS_Pr->SiS_ModeType > ModeVGA) {
      /* 256 color disable */
      SiS_SetRegAND(SiS_Pr->SiS_P3ce,0x05,0xBF);
   }
}

/*********************************************/
/*          CLEAR EXTENDED REGISTERS         */
/*********************************************/

static void
SiS_ClearExt1Regs(struct SiS_Private *SiS_Pr, unsigned short ModeNo)
{
   unsigned short i;

   for(i = 0x0A; i <= 0x0E; i++) {
      SiS_SetReg(SiS_Pr->SiS_P3c4,i,0x00);
   }

   if(SiS_Pr->ChipType >= SIS_315H) {
      SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x37,0xFE);
      if(ModeNo <= 0x13) {
	 if(ModeNo == 0x06 || ModeNo >= 0x0e) {
	    SiS_SetReg(SiS_Pr->SiS_P3c4,0x0e,0x20);
	 }
      }
   }
}

/*********************************************/
/*                 RESET VCLK                */
/*********************************************/

static void
SiS_ResetCRT1VCLK(struct SiS_Private *SiS_Pr)
{
   if(SiS_Pr->ChipType >= SIS_315H) {
      if(SiS_Pr->ChipType < SIS_661) {
	 if(SiS_Pr->SiS_IF_DEF_LVDS == 0) return;
      }
   } else {
      if((SiS_Pr->SiS_IF_DEF_LVDS == 0) &&
	 (!(SiS_Pr->SiS_VBType & VB_SIS30xBLV)) ) {
	 return;
      }
   }

   SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x31,0xcf,0x20);
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x2B,SiS_Pr->SiS_VCLKData[1].SR2B);
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x2C,SiS_Pr->SiS_VCLKData[1].SR2C);
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x2D,0x80);
   SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x31,0xcf,0x10);
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x2B,SiS_Pr->SiS_VCLKData[0].SR2B);
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x2C,SiS_Pr->SiS_VCLKData[0].SR2C);
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x2D,0x80);
}

/*********************************************/
/*                  SYNC                     */
/*********************************************/

static void
SiS_SetCRT1Sync(struct SiS_Private *SiS_Pr, unsigned short RRTI)
{
   unsigned short sync;

   if(SiS_Pr->UseCustomMode) {
      sync = SiS_Pr->CInfoFlag >> 8;
   } else {
      sync = SiS_Pr->SiS_RefIndex[RRTI].Ext_InfoFlag >> 8;
   }

   sync &= 0xC0;
   sync |= 0x2f;
   SiS_SetRegByte(SiS_Pr->SiS_P3c2,sync);
}

/*********************************************/
/*                  CRTC/2                   */
/*********************************************/

static void
SiS_SetCRT1CRTC(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
		unsigned short ModeIdIndex, unsigned short RRTI)
{
   unsigned short temp, i, j, modeflag;
   unsigned char  *crt1data = NULL;

   modeflag = SiS_GetModeFlag(SiS_Pr, ModeNo, ModeIdIndex);

   if(SiS_Pr->UseCustomMode) {

      crt1data = &SiS_Pr->CCRT1CRTC[0];

   } else {

      temp = SiS_GetRefCRT1CRTC(SiS_Pr, RRTI, SiS_Pr->SiS_UseWide);

      /* Alternate for 1600x1200 LCDA */
      if((temp == 0x20) && (SiS_Pr->Alternate1600x1200)) temp = 0x57;

      crt1data = (unsigned char *)&SiS_Pr->SiS_CRT1Table[temp].CR[0];

   }

   /* unlock cr0-7 */
   SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x11,0x7f);

   for(i = 0, j = 0; i <= 7; i++, j++) {
      SiS_SetReg(SiS_Pr->SiS_P3d4,j,crt1data[i]);
   }
   for(j = 0x10; i <= 10; i++, j++) {
      SiS_SetReg(SiS_Pr->SiS_P3d4,j,crt1data[i]);
   }
   for(j = 0x15; i <= 12; i++, j++) {
      SiS_SetReg(SiS_Pr->SiS_P3d4,j,crt1data[i]);
   }
   for(j = 0x0A; i <= 15; i++, j++) {
      SiS_SetReg(SiS_Pr->SiS_P3c4,j,crt1data[i]);
   }

   SiS_SetReg(SiS_Pr->SiS_P3c4,0x0E,crt1data[16] & 0xE0);

   temp = (crt1data[16] & 0x01) << 5;
   if(modeflag & DoubleScanMode) temp |= 0x80;
   SiS_SetRegANDOR(SiS_Pr->SiS_P3d4,0x09,0x5F,temp);

   if(SiS_Pr->SiS_ModeType > ModeVGA) {
      SiS_SetReg(SiS_Pr->SiS_P3d4,0x14,0x4F);
   }

#ifdef SIS315H
   if(SiS_Pr->ChipType == XGI_20) {
      SiS_SetReg(SiS_Pr->SiS_P3d4,0x04,crt1data[4] - 1);
      if(!(temp = crt1data[5] & 0x1f)) {
         SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x0c,0xfb);
      }
      SiS_SetRegANDOR(SiS_Pr->SiS_P3d4,0x05,0xe0,((temp - 1) & 0x1f));
      temp = (crt1data[16] >> 5) + 3;
      if(temp > 7) temp -= 7;
      SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x0e,0x1f,(temp << 5));
   }
#endif
}

/*********************************************/
/*               OFFSET & PITCH              */
/*********************************************/
/*  (partly overruled by SetPitch() in XF86) */
/*********************************************/

static void
SiS_SetCRT1Offset(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
		unsigned short ModeIdIndex, unsigned short RRTI)
{
   unsigned short temp, DisplayUnit, infoflag;

   if(SiS_Pr->UseCustomMode) {
      infoflag = SiS_Pr->CInfoFlag;
   } else {
      infoflag = SiS_Pr->SiS_RefIndex[RRTI].Ext_InfoFlag;
   }

   DisplayUnit = SiS_GetOffset(SiS_Pr, ModeNo, ModeIdIndex, RRTI);

   temp = (DisplayUnit >> 8) & 0x0f;
   SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x0E,0xF0,temp);

   SiS_SetReg(SiS_Pr->SiS_P3d4,0x13,DisplayUnit & 0xFF);

   if(infoflag & InterlaceMode) DisplayUnit >>= 1;

   DisplayUnit <<= 5;
   temp = (DisplayUnit >> 8) + 1;
   if(DisplayUnit & 0xff) temp++;
   if(SiS_Pr->ChipType == XGI_20) {
      if(ModeNo == 0x4a || ModeNo == 0x49) temp--;
   }
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x10,temp);
}

/*********************************************/
/*                  VCLK                     */
/*********************************************/

static void
SiS_SetCRT1VCLK(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
		unsigned short ModeIdIndex, unsigned short RRTI)
{
   unsigned short index = 0, clka, clkb;

   if(SiS_Pr->UseCustomMode) {
      clka = SiS_Pr->CSR2B;
      clkb = SiS_Pr->CSR2C;
   } else {
      index = SiS_GetVCLK2Ptr(SiS_Pr, ModeNo, ModeIdIndex, RRTI);
      if((SiS_Pr->SiS_VBType & VB_SIS30xBLV) &&
	 (SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA)) {
	 /* Alternate for 1600x1200 LCDA */
	 if((index == 0x21) && (SiS_Pr->Alternate1600x1200)) index = 0x72;
	 clka = SiS_Pr->SiS_VBVCLKData[index].Part4_A;
	 clkb = SiS_Pr->SiS_VBVCLKData[index].Part4_B;
      } else {
	 clka = SiS_Pr->SiS_VCLKData[index].SR2B;
	 clkb = SiS_Pr->SiS_VCLKData[index].SR2C;
      }
   }

   SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x31,0xCF);

   SiS_SetReg(SiS_Pr->SiS_P3c4,0x2b,clka);
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x2c,clkb);

   if(SiS_Pr->ChipType >= SIS_315H) {
#ifdef SIS315H
      SiS_SetReg(SiS_Pr->SiS_P3c4,0x2D,0x01);
      if(SiS_Pr->ChipType == XGI_20) {
         unsigned short mf = SiS_GetModeFlag(SiS_Pr, ModeNo, ModeIdIndex);
	 if(mf & HalfDCLK) {
	    SiS_SetReg(SiS_Pr->SiS_P3c4,0x2b,SiS_GetReg(SiS_Pr->SiS_P3c4,0x2b));
	    clkb = SiS_GetReg(SiS_Pr->SiS_P3c4,0x2c);
	    clkb = (((clkb & 0x1f) << 1) + 1) | (clkb & 0xe0);
	    SiS_SetReg(SiS_Pr->SiS_P3c4,0x2c,clkb);
	 }
      }
#endif
   } else {
      SiS_SetReg(SiS_Pr->SiS_P3c4,0x2D,0x80);
   }
}

/*********************************************/
/*                  FIFO                     */
/*********************************************/

#ifdef SIS300
void
SiS_GetFIFOThresholdIndex300(struct SiS_Private *SiS_Pr, unsigned short *idx1,
		unsigned short *idx2)
{
   unsigned short temp1, temp2;
   static const unsigned char ThTiming[8] = {
		1, 2, 2, 3, 0, 1, 1, 2
   };

   temp1 = temp2 = (SiS_GetReg(SiS_Pr->SiS_P3c4,0x18) & 0x62) >> 1;
   (*idx2) = (unsigned short)(ThTiming[((temp2 >> 3) | temp1) & 0x07]);
   (*idx1) = (unsigned short)(SiS_GetReg(SiS_Pr->SiS_P3c4,0x16) >> 6) & 0x03;
   (*idx1) |= (unsigned short)(((SiS_GetReg(SiS_Pr->SiS_P3c4,0x14) >> 4) & 0x0c));
   (*idx1) <<= 1;
}

static unsigned short
SiS_GetFIFOThresholdA300(unsigned short idx1, unsigned short idx2)
{
   static const unsigned char ThLowA[8 * 3] = {
		61, 3,52, 5,68, 7,100,11,
		43, 3,42, 5,54, 7, 78,11,
		34, 3,37, 5,47, 7, 67,11
   };

   return (unsigned short)((ThLowA[idx1 + 1] * idx2) + ThLowA[idx1]);
}

unsigned short
SiS_GetFIFOThresholdB300(unsigned short idx1, unsigned short idx2)
{
   static const unsigned char ThLowB[8 * 3] = {
		81, 4,72, 6,88, 8,120,12,
		55, 4,54, 6,66, 8, 90,12,
		42, 4,45, 6,55, 8, 75,12
   };

   return (unsigned short)((ThLowB[idx1 + 1] * idx2) + ThLowB[idx1]);
}

static unsigned short
SiS_DoCalcDelay(struct SiS_Private *SiS_Pr, unsigned short MCLK, unsigned short VCLK,
		unsigned short colordepth, unsigned short key)
{
   unsigned short idx1, idx2;
   unsigned int   longtemp = VCLK * colordepth;

   SiS_GetFIFOThresholdIndex300(SiS_Pr, &idx1, &idx2);

   if(key == 0) {
      longtemp *= SiS_GetFIFOThresholdA300(idx1, idx2);
   } else {
      longtemp *= SiS_GetFIFOThresholdB300(idx1, idx2);
   }
   idx1 = longtemp % (MCLK * 16);
   longtemp /= (MCLK * 16);
   if(idx1) longtemp++;
   return (unsigned short)longtemp;
}

static unsigned short
SiS_CalcDelay(struct SiS_Private *SiS_Pr, unsigned short VCLK,
		unsigned short colordepth, unsigned short MCLK)
{
   unsigned short temp1, temp2;

   temp2 = SiS_DoCalcDelay(SiS_Pr, MCLK, VCLK, colordepth, 0);
   temp1 = SiS_DoCalcDelay(SiS_Pr, MCLK, VCLK, colordepth, 1);
   if(temp1 < 4) temp1 = 4;
   temp1 -= 4;
   if(temp2 < temp1) temp2 = temp1;
   return temp2;
}

static void
SiS_SetCRT1FIFO_300(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
		unsigned short RefreshRateTableIndex)
{
   unsigned short ThresholdLow = 0;
   unsigned short temp, index, VCLK, MCLK, colorth;
   static const unsigned short colortharray[6] = { 1, 1, 2, 2, 3, 4 };

   if(ModeNo > 0x13) {

      /* Get VCLK  */
      if(SiS_Pr->UseCustomMode) {
	 VCLK = SiS_Pr->CSRClock;
      } else {
	 index = SiS_GetRefCRTVCLK(SiS_Pr, RefreshRateTableIndex, SiS_Pr->SiS_UseWide);
	 VCLK = SiS_Pr->SiS_VCLKData[index].CLOCK;
      }

      /* Get half colordepth */
      colorth = colortharray[(SiS_Pr->SiS_ModeType - ModeEGA)];

      /* Get MCLK  */
      index = SiS_GetReg(SiS_Pr->SiS_P3c4,0x3A) & 0x07;
      MCLK = SiS_Pr->SiS_MCLKData_0[index].CLOCK;

      temp = SiS_GetReg(SiS_Pr->SiS_P3d4,0x35) & 0xc3;
      SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x16,0x3c,temp);

      do {
	 ThresholdLow = SiS_CalcDelay(SiS_Pr, VCLK, colorth, MCLK) + 1;
	 if(ThresholdLow < 0x13) break;
	 SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x16,0xfc);
	 ThresholdLow = 0x13;
	 temp = SiS_GetReg(SiS_Pr->SiS_P3c4,0x16) >> 6;
	 if(!temp) break;
	 SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x16,0x3f,((temp - 1) << 6));
      } while(0);

   } else ThresholdLow = 2;

   /* Write CRT/CPU threshold low, CRT/Engine threshold high */
   temp = (ThresholdLow << 4) | 0x0f;
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x08,temp);

   temp = (ThresholdLow & 0x10) << 1;
   if(ModeNo > 0x13) temp |= 0x40;
   SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x0f,0x9f,temp);

   /* What is this? */
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x3B,0x09);

   /* Write CRT/CPU threshold high */
   temp = ThresholdLow + 3;
   if(temp > 0x0f) temp = 0x0f;
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x09,temp);
}

unsigned short
SiS_GetLatencyFactor630(struct SiS_Private *SiS_Pr, unsigned short index)
{
   static const unsigned char LatencyFactor[] = {
		97, 88, 86, 79, 77,  0,       /* 64  bit    BQ=2   */
		 0, 87, 85, 78, 76, 54,       /* 64  bit    BQ=1   */
		97, 88, 86, 79, 77,  0,       /* 128 bit    BQ=2   */
		 0, 79, 77, 70, 68, 48,       /* 128 bit    BQ=1   */
		80, 72, 69, 63, 61,  0,       /* 64  bit    BQ=2   */
		 0, 70, 68, 61, 59, 37,       /* 64  bit    BQ=1   */
		86, 77, 75, 68, 66,  0,       /* 128 bit    BQ=2   */
		 0, 68, 66, 59, 57, 37        /* 128 bit    BQ=1   */
   };
   static const unsigned char LatencyFactor730[] = {
		 69, 63, 61,
		 86, 79, 77,
		103, 96, 94,
		120,113,111,
		137,130,128
   };

   if(SiS_Pr->ChipType == SIS_730) {
      return (unsigned short)LatencyFactor730[index];
   } else {
      return (unsigned short)LatencyFactor[index];
   }
}

static unsigned short
SiS_CalcDelay2(struct SiS_Private *SiS_Pr, unsigned char key)
{
   unsigned short index;

   if(SiS_Pr->ChipType == SIS_730) {
      index = ((key & 0x0f) * 3) + ((key & 0xc0) >> 6);
   } else {
      index = (key & 0xe0) >> 5;
      if(key & 0x10)    index +=  6;
      if(!(key & 0x01)) index += 24;
      if(SiS_GetReg(SiS_Pr->SiS_P3c4,0x14) & 0x80) index += 12;
   }
   return SiS_GetLatencyFactor630(SiS_Pr, index);
}

static void
SiS_SetCRT1FIFO_630(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
                    unsigned short RefreshRateTableIndex)
{
   unsigned short  ThresholdLow = 0;
   unsigned short  i, data, VCLK, MCLK16, colorth = 0;
   unsigned int    templ, datal;
   const unsigned char *queuedata = NULL;
   static const unsigned char FQBQData[21] = {
		0x01,0x21,0x41,0x61,0x81,
		0x31,0x51,0x71,0x91,0xb1,
		0x00,0x20,0x40,0x60,0x80,
		0x30,0x50,0x70,0x90,0xb0,
		0xff
   };
   static const unsigned char FQBQData730[16] = {
		0x34,0x74,0xb4,
		0x23,0x63,0xa3,
		0x12,0x52,0x92,
		0x01,0x41,0x81,
		0x00,0x40,0x80,
		0xff
   };
   static const unsigned short colortharray[6] = {
		1, 1, 2, 2, 3, 4
   };

   i = 0;

   if(ModeNo > 0x13) {

      /* Get VCLK  */
      if(SiS_Pr->UseCustomMode) {
	 VCLK = SiS_Pr->CSRClock;
      } else {
	 data = SiS_GetRefCRTVCLK(SiS_Pr, RefreshRateTableIndex, SiS_Pr->SiS_UseWide);
	 VCLK = SiS_Pr->SiS_VCLKData[data].CLOCK;
      }

      /* Get MCLK * 16 */
      data = SiS_GetReg(SiS_Pr->SiS_P3c4,0x1A) & 0x07;
      MCLK16 = SiS_Pr->SiS_MCLKData_0[data].CLOCK * 16;

      /* Get half colordepth */
      colorth = colortharray[(SiS_Pr->SiS_ModeType - ModeEGA)];

      if(SiS_Pr->ChipType == SIS_730) {
	 queuedata = &FQBQData730[0];
      } else {
	 queuedata = &FQBQData[0];
      }

      do {
	 templ = SiS_CalcDelay2(SiS_Pr, queuedata[i]) * VCLK * colorth;

	 datal = templ % MCLK16;
	 templ = (templ / MCLK16) + 1;
	 if(datal) templ++;

	 if(templ > 0x13) {
	    if(queuedata[i + 1] == 0xFF) {
	       ThresholdLow = 0x13;
	       break;
	    }
	    i++;
	 } else {
	    ThresholdLow = templ;
	    break;
	 }
      } while(queuedata[i] != 0xFF);

   } else {

      if(SiS_Pr->ChipType != SIS_730) i = 9;
      ThresholdLow = 0x02;

   }

   /* Write CRT/CPU threshold low, CRT/Engine threshold high */
   data = ((ThresholdLow & 0x0f) << 4) | 0x0f;
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x08,data);

   data = (ThresholdLow & 0x10) << 1;
   SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x0F,0xDF,data);

   /* What is this? */
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x3B,0x09);

   /* Write CRT/CPU threshold high (gap = 3) */
   data = ThresholdLow + 3;
   if(data > 0x0f) data = 0x0f;
   SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x09,0x80,data);

  /* Write foreground and background queue */
#ifdef SIS_LINUX_KERNEL
   templ = sisfb_read_nbridge_pci_dword(SiS_Pr, 0x50);
#else
   templ = pciReadLong(0x00000000, 0x50);
#endif

   if(SiS_Pr->ChipType == SIS_730) {

      templ &= 0xfffff9ff;
      templ |= ((queuedata[i] & 0xc0) << 3);

   } else {

      templ &= 0xf0ffffff;
      if( (ModeNo <= 0x13) &&
          (SiS_Pr->ChipType == SIS_630) &&
	  (SiS_Pr->ChipRevision >= 0x30) ) {
	 templ |= 0x0b000000;
      } else {
         templ |= ((queuedata[i] & 0xf0) << 20);
      }

   }

#ifdef SIS_LINUX_KERNEL
   sisfb_write_nbridge_pci_dword(SiS_Pr, 0x50, templ);
   templ = sisfb_read_nbridge_pci_dword(SiS_Pr, 0xA0);
#else
   pciWriteLong(0x00000000, 0x50, templ);
   templ = pciReadLong(0x00000000, 0xA0);
#endif

   /* GUI grant timer (PCI config 0xA3) */
   if(SiS_Pr->ChipType == SIS_730) {

      templ &= 0x00ffffff;
      datal = queuedata[i] << 8;
      templ |= (((datal & 0x0f00) | ((datal & 0x3000) >> 8)) << 20);

   } else {

      templ &= 0xf0ffffff;
      templ |= ((queuedata[i] & 0x0f) << 24);

   }

#ifdef SIS_LINUX_KERNEL
   sisfb_write_nbridge_pci_dword(SiS_Pr, 0xA0, templ);
#else
   pciWriteLong(0x00000000, 0xA0, templ);
#endif
}
#endif /* SIS300 */

#ifdef SIS315H
static void
SiS_SetCRT1FIFO_310(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex)
{
   unsigned short modeflag;

   /* disable auto-threshold */
   SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x3D,0xFE);

   modeflag = SiS_GetModeFlag(SiS_Pr, ModeNo, ModeIdIndex);

   SiS_SetReg(SiS_Pr->SiS_P3c4,0x08,0xAE);
   SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x09,0xF0);
   if(ModeNo > 0x13) {
      if(SiS_Pr->ChipType >= XGI_20) {
	 SiS_SetReg(SiS_Pr->SiS_P3c4,0x08,0x34);
	 SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x3D,0x01);
      } else if(SiS_Pr->ChipType >= SIS_661) {
	 if(!(modeflag & HalfDCLK)) {
	    SiS_SetReg(SiS_Pr->SiS_P3c4,0x08,0x34);
	    SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x3D,0x01);
	 }
      } else {
	 if((!(modeflag & DoubleScanMode)) || (!(modeflag & HalfDCLK))) {
	    SiS_SetReg(SiS_Pr->SiS_P3c4,0x08,0x34);
	    SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x3D,0x01);
	 }
      }
   }
}
#endif

/*********************************************/
/*              MODE REGISTERS               */
/*********************************************/

static void
SiS_SetVCLKState(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
		unsigned short RefreshRateTableIndex, unsigned short ModeIdIndex)
{
   unsigned short data = 0, VCLK = 0, index = 0;

   if(ModeNo > 0x13) {
      if(SiS_Pr->UseCustomMode) {
         VCLK = SiS_Pr->CSRClock;
      } else {
         index = SiS_GetVCLK2Ptr(SiS_Pr, ModeNo, ModeIdIndex, RefreshRateTableIndex);
         VCLK = SiS_Pr->SiS_VCLKData[index].CLOCK;
      }
   }

   if(SiS_Pr->ChipType < SIS_315H) {
#ifdef SIS300
      if(VCLK > 150) data |= 0x80;
      SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x07,0x7B,data);

      data = 0x00;
      if(VCLK >= 150) data |= 0x08;
      SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x32,0xF7,data);
#endif
   } else if(SiS_Pr->ChipType < XGI_20) {
#ifdef SIS315H
      if(VCLK >= 166) data |= 0x0c;
      SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x32,0xf3,data);

      if(VCLK >= 166) {
         SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x1f,0xe7);
      }
#endif
   } else {
#ifdef SIS315H
      if(VCLK >= 200) data |= 0x0c;
      if(SiS_Pr->ChipType == XGI_20) data &= ~0x04;
      SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x32,0xf3,data);
      if(SiS_Pr->ChipType != XGI_20) {
         data = SiS_GetReg(SiS_Pr->SiS_P3c4,0x1f) & 0xe7;
	 if(VCLK < 200) data |= 0x10;
	 SiS_SetReg(SiS_Pr->SiS_P3c4,0x1f,data);
      }
#endif
   }

   /* DAC speed */
   if(SiS_Pr->ChipType >= SIS_661) {

      SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x07,0xE8,0x10);

   } else {

      data = 0x03;
      if(VCLK >= 260)      data = 0x00;
      else if(VCLK >= 160) data = 0x01;
      else if(VCLK >= 135) data = 0x02;

      if(SiS_Pr->ChipType == SIS_540) {
         if((VCLK == 203) || (VCLK < 234)) data = 0x02;
      }

      if(SiS_Pr->ChipType < SIS_315H) {
         SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x07,0xFC,data);
      } else {
         if(SiS_Pr->ChipType > SIS_315PRO) {
            if(ModeNo > 0x13) data &= 0xfc;
         }
         SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x07,0xF8,data);
      }

   }
}

static void
SiS_SetCRT1ModeRegs(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
		unsigned short ModeIdIndex, unsigned short RRTI)
{
   unsigned short data, infoflag = 0, modeflag, resindex;
#ifdef SIS315H
   unsigned char  *ROMAddr  = SiS_Pr->VirtualRomBase;
   unsigned short data2, data3;
#endif

   modeflag = SiS_GetModeFlag(SiS_Pr, ModeNo, ModeIdIndex);

   if(SiS_Pr->UseCustomMode) {
      infoflag = SiS_Pr->CInfoFlag;
   } else {
      resindex = SiS_GetResInfo(SiS_Pr, ModeNo, ModeIdIndex);
      if(ModeNo > 0x13) {
	 infoflag = SiS_Pr->SiS_RefIndex[RRTI].Ext_InfoFlag;
      }
   }

   /* Disable DPMS */
   SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x1F,0x3F);

   data = 0;
   if(ModeNo > 0x13) {
      if(SiS_Pr->SiS_ModeType > ModeEGA) {
         data |= 0x02;
         data |= ((SiS_Pr->SiS_ModeType - ModeVGA) << 2);
      }
      if(infoflag & InterlaceMode) data |= 0x20;
   }
   SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x06,0xC0,data);

   if(SiS_Pr->ChipType != SIS_300) {
      data = 0;
      if(infoflag & InterlaceMode) {
	 /* data = (Hsync / 8) - ((Htotal / 8) / 2) + 3 */
	 int hrs = (SiS_GetReg(SiS_Pr->SiS_P3d4,0x04) |
		    ((SiS_GetReg(SiS_Pr->SiS_P3c4,0x0b) & 0xc0) << 2)) - 3;
	 int hto = (SiS_GetReg(SiS_Pr->SiS_P3d4,0x00) |
		    ((SiS_GetReg(SiS_Pr->SiS_P3c4,0x0b) & 0x03) << 8)) + 5;
	 data = hrs - (hto >> 1) + 3;
      }
      SiS_SetReg(SiS_Pr->SiS_P3d4,0x19,data);
      SiS_SetRegANDOR(SiS_Pr->SiS_P3d4,0x1a,0xFC,((data >> 8) & 0x03));
   }

   if(modeflag & HalfDCLK) {
      SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x01,0x08);
   }

   data = 0;
   if(modeflag & LineCompareOff) data = 0x08;
   if(SiS_Pr->ChipType == SIS_300) {
      SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x0F,0xF7,data);
   } else {
      if(SiS_Pr->ChipType >= XGI_20) data |= 0x20;
      if(SiS_Pr->SiS_ModeType == ModeEGA) {
	 if(ModeNo > 0x13) {
	    data |= 0x40;
	 }
      }
      SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x0F,0xB7,data);
   }

#ifdef SIS315H
   if(SiS_Pr->ChipType >= SIS_315H) {
      SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x31,0xfb);
   }

   if(SiS_Pr->ChipType == SIS_315PRO) {

      data = SiS_Pr->SiS_SR15[(2 * 4) + SiS_Get310DRAMType(SiS_Pr)];
      if(SiS_Pr->SiS_ModeType == ModeText) {
	 data &= 0xc7;
      } else {
	 data2 = SiS_GetOffset(SiS_Pr, ModeNo, ModeIdIndex, RRTI) >> 1;
	 if(infoflag & InterlaceMode) data2 >>= 1;
	 data3 = SiS_GetColorDepth(SiS_Pr, ModeNo, ModeIdIndex) >> 1;
	 if(data3) data2 /= data3;
	 if(data2 >= 0x50) {
	    data &= 0x0f;
	    data |= 0x50;
	 }
      }
      SiS_SetReg(SiS_Pr->SiS_P3c4,0x17,data);

   } else if((SiS_Pr->ChipType == SIS_330) || (SiS_Pr->SiS_SysFlags & SF_760LFB)) {

      data = SiS_Get310DRAMType(SiS_Pr);
      if(SiS_Pr->ChipType == SIS_330) {
	 data = SiS_Pr->SiS_SR15[(2 * 4) + data];
      } else {
	 if(SiS_Pr->SiS_ROMNew)	     data = ROMAddr[0xf6];
	 else if(SiS_Pr->SiS_UseROM) data = ROMAddr[0x100 + data];
	 else			     data = 0xba;
      }
      if(SiS_Pr->SiS_ModeType <= ModeEGA) {
	 data &= 0xc7;
      } else {
	 if(SiS_Pr->UseCustomMode) {
	    data2 = SiS_Pr->CSRClock;
	 } else {
	    data2 = SiS_GetVCLK2Ptr(SiS_Pr, ModeNo, ModeIdIndex, RRTI);
	    data2 = SiS_Pr->SiS_VCLKData[data2].CLOCK;
	 }

	 data3 = SiS_GetColorDepth(SiS_Pr, ModeNo, ModeIdIndex) >> 1;
	 if(data3) data2 *= data3;

	 data2 = ((unsigned int)(SiS_GetMCLK(SiS_Pr) * 1024)) / data2;

	 if(SiS_Pr->ChipType == SIS_330) {
	    if(SiS_Pr->SiS_ModeType != Mode16Bpp) {
	       if     (data2 >= 0x19c) data = 0xba;
	       else if(data2 >= 0x140) data = 0x7a;
	       else if(data2 >= 0x101) data = 0x3a;
	       else if(data2 >= 0xf5)  data = 0x32;
	       else if(data2 >= 0xe2)  data = 0x2a;
	       else if(data2 >= 0xc4)  data = 0x22;
	       else if(data2 >= 0xac)  data = 0x1a;
	       else if(data2 >= 0x9e)  data = 0x12;
	       else if(data2 >= 0x8e)  data = 0x0a;
	       else                    data = 0x02;
	    } else {
	       if(data2 >= 0x127)      data = 0xba;
	       else                    data = 0x7a;
	    }
	 } else {  /* 76x+LFB */
	    if     (data2 >= 0x190) data = 0xba;
	    else if(data2 >= 0xff)  data = 0x7a;
	    else if(data2 >= 0xd3)  data = 0x3a;
	    else if(data2 >= 0xa9)  data = 0x1a;
	    else if(data2 >= 0x93)  data = 0x0a;
	    else                    data = 0x02;
	 }
      }
      SiS_SetReg(SiS_Pr->SiS_P3c4,0x17,data);

   }
      /* XGI: Nothing. */
      /* TODO: Check SiS340 */
#endif

   data = 0x60;
   if(SiS_Pr->SiS_ModeType != ModeText) {
      data ^= 0x60;
      if(SiS_Pr->SiS_ModeType != ModeEGA) {
         data ^= 0xA0;
      }
   }
   SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x21,0x1F,data);

   SiS_SetVCLKState(SiS_Pr, ModeNo, RRTI, ModeIdIndex);

#ifdef SIS315H
   if(((SiS_Pr->ChipType >= SIS_315H) && (SiS_Pr->ChipType < SIS_661)) ||
       (SiS_Pr->ChipType == XGI_40)) {
      if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x31) & 0x40) {
         SiS_SetReg(SiS_Pr->SiS_P3d4,0x52,0x2c);
      } else {
         SiS_SetReg(SiS_Pr->SiS_P3d4,0x52,0x6c);
      }
   } else if(SiS_Pr->ChipType == XGI_20) {
      if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x31) & 0x40) {
         SiS_SetReg(SiS_Pr->SiS_P3d4,0x52,0x33);
      } else {
         SiS_SetReg(SiS_Pr->SiS_P3d4,0x52,0x73);
      }
      SiS_SetReg(SiS_Pr->SiS_P3d4,0x51,0x02);
   }
#endif
}

#ifdef SIS315H
static void
SiS_SetupDualChip(struct SiS_Private *SiS_Pr)
{
#if 0
   /* TODO: Find out about IOAddress2 */
   SISIOADDRESS P2_3c2 = SiS_Pr->IOAddress2 + 0x12;
   SISIOADDRESS P2_3c4 = SiS_Pr->IOAddress2 + 0x14;
   SISIOADDRESS P2_3ce = SiS_Pr->IOAddress2 + 0x1e;
   int i;

   if((SiS_Pr->ChipRevision != 0) ||
      (!(SiS_GetReg(SiS_Pr->SiS_P3c4,0x3a) & 0x04)))
      return;

   for(i = 0; i <= 4; i++) {					/* SR00 - SR04 */
      SiS_SetReg(P2_3c4,i,SiS_GetReg(SiS_Pr->SiS_P3c4,i));
   }
   for(i = 0; i <= 8; i++) {					/* GR00 - GR08 */
      SiS_SetReg(P2_3ce,i,SiS_GetReg(SiS_Pr->SiS_P3ce,i));
   }
   SiS_SetReg(P2_3c4,0x05,0x86);
   SiS_SetReg(P2_3c4,0x06,SiS_GetReg(SiS_Pr->SiS_P3c4,0x06));	/* SR06 */
   SiS_SetReg(P2_3c4,0x21,SiS_GetReg(SiS_Pr->SiS_P3c4,0x21));	/* SR21 */
   SiS_SetRegByte(P2_3c2,SiS_GetRegByte(SiS_Pr->SiS_P3cc));	/* MISC */
   SiS_SetReg(P2_3c4,0x05,0x00);
#endif
}
#endif

/*********************************************/
/*                 LOAD DAC                  */
/*********************************************/

static void
SiS_WriteDAC(struct SiS_Private *SiS_Pr, SISIOADDRESS DACData, unsigned short shiftflag,
             unsigned short dl, unsigned short ah, unsigned short al, unsigned short dh)
{
   unsigned short d1, d2, d3;

   switch(dl) {
   case  0: d1 = dh; d2 = ah; d3 = al; break;
   case  1: d1 = ah; d2 = al; d3 = dh; break;
   default: d1 = al; d2 = dh; d3 = ah;
   }
   SiS_SetRegByte(DACData, (d1 << shiftflag));
   SiS_SetRegByte(DACData, (d2 << shiftflag));
   SiS_SetRegByte(DACData, (d3 << shiftflag));
}

void
SiS_LoadDAC(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex)
{
   unsigned short data, data2, time, i, j, k, m, n, o;
   unsigned short si, di, bx, sf;
   SISIOADDRESS DACAddr, DACData;
   const unsigned char *table = NULL;

   data = SiS_GetModeFlag(SiS_Pr, ModeNo, ModeIdIndex) & DACInfoFlag;

   j = time = 64;
   if(data == 0x00)      table = SiS_MDA_DAC;
   else if(data == 0x08) table = SiS_CGA_DAC;
   else if(data == 0x10) table = SiS_EGA_DAC;
   else if(data == 0x18) {
      j = 16;
      time = 256;
      table = SiS_VGA_DAC;
   }

   if( ( (SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) &&        /* 301B-DH LCD */
         (SiS_Pr->SiS_VBType & VB_NoLCD) )        ||
       (SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA)       ||   /* LCDA */
       (!(SiS_Pr->SiS_SetFlag & ProgrammingCRT2)) ) {  /* Programming CRT1 */
      SiS_SetRegByte(SiS_Pr->SiS_P3c6,0xFF);
      DACAddr = SiS_Pr->SiS_P3c8;
      DACData = SiS_Pr->SiS_P3c9;
      sf = 0;
   } else {
      DACAddr = SiS_Pr->SiS_Part5Port;
      DACData = SiS_Pr->SiS_Part5Port + 1;
      sf = 2;
   }

   SiS_SetRegByte(DACAddr,0x00);

   for(i = 0; i < j; i++) {
      data = table[i];
      for(k = 0; k < 3; k++) {
	data2 = 0;
	if(data & 0x01) data2 += 0x2A;
	if(data & 0x02) data2 += 0x15;
	SiS_SetRegByte(DACData, (data2 << sf));
	data >>= 2;
      }
   }

   if(time == 256) {
      for(i = 16; i < 32; i++) {
	 data = table[i] << sf;
	 for(k = 0; k < 3; k++) SiS_SetRegByte(DACData, data);
      }
      si = 32;
      for(m = 0; m < 9; m++) {
	 di = si;
	 bx = si + 4;
	 for(n = 0; n < 3; n++) {
	    for(o = 0; o < 5; o++) {
	       SiS_WriteDAC(SiS_Pr, DACData, sf, n, table[di], table[bx], table[si]);
	       si++;
	    }
	    si -= 2;
	    for(o = 0; o < 3; o++) {
	       SiS_WriteDAC(SiS_Pr, DACData, sf, n, table[di], table[si], table[bx]);
	       si--;
	    }
	 }            /* for n < 3 */
	 si += 5;
      }               /* for m < 9 */
   }
}

/*********************************************/
/*         SET CRT1 REGISTER GROUP           */
/*********************************************/

static void
SiS_SetCRT1Group(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex)
{
   unsigned short StandTableIndex, RefreshRateTableIndex;

   SiS_Pr->SiS_CRT1Mode = ModeNo;

   StandTableIndex = SiS_GetModePtr(SiS_Pr, ModeNo, ModeIdIndex);

   if(SiS_Pr->SiS_SetFlag & LowModeTests) {
      if(SiS_Pr->SiS_VBInfo & (SetSimuScanMode | SwitchCRT2)) {
         SiS_DisableBridge(SiS_Pr);
      }
   }

   SiS_ResetSegmentRegisters(SiS_Pr);

   SiS_SetSeqRegs(SiS_Pr, StandTableIndex);
   SiS_SetMiscRegs(SiS_Pr, StandTableIndex);
   SiS_SetCRTCRegs(SiS_Pr, StandTableIndex);
   SiS_SetATTRegs(SiS_Pr, StandTableIndex);
   SiS_SetGRCRegs(SiS_Pr, StandTableIndex);
   SiS_ClearExt1Regs(SiS_Pr, ModeNo);
   SiS_ResetCRT1VCLK(SiS_Pr);

   SiS_Pr->SiS_SelectCRT2Rate = 0;
   SiS_Pr->SiS_SetFlag &= (~ProgrammingCRT2);

#ifdef SIS_XORG_XF86
   xf86DrvMsgVerb(0, X_PROBED, 4, "(init: VBType=0x%04x, VBInfo=0x%04x)\n",
                    SiS_Pr->SiS_VBType, SiS_Pr->SiS_VBInfo);
#endif

   if(SiS_Pr->SiS_VBInfo & SetSimuScanMode) {
      if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
         SiS_Pr->SiS_SetFlag |= ProgrammingCRT2;
      }
   }

   if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA) {
      SiS_Pr->SiS_SetFlag |= ProgrammingCRT2;
   }

   RefreshRateTableIndex = SiS_GetRatePtr(SiS_Pr, ModeNo, ModeIdIndex);

   if(!(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA)) {
      SiS_Pr->SiS_SetFlag &= ~ProgrammingCRT2;
   }

   if(RefreshRateTableIndex != 0xFFFF) {
      SiS_SetCRT1Sync(SiS_Pr, RefreshRateTableIndex);
      SiS_SetCRT1CRTC(SiS_Pr, ModeNo, ModeIdIndex, RefreshRateTableIndex);
      SiS_SetCRT1Offset(SiS_Pr, ModeNo, ModeIdIndex, RefreshRateTableIndex);
      SiS_SetCRT1VCLK(SiS_Pr, ModeNo, ModeIdIndex, RefreshRateTableIndex);
   }

   switch(SiS_Pr->ChipType) {
#ifdef SIS300
   case SIS_300:
      SiS_SetCRT1FIFO_300(SiS_Pr, ModeNo, RefreshRateTableIndex);
      break;
   case SIS_540:
   case SIS_630:
   case SIS_730:
      SiS_SetCRT1FIFO_630(SiS_Pr, ModeNo, RefreshRateTableIndex);
      break;
#endif
   default:
#ifdef SIS315H
      if(SiS_Pr->ChipType == XGI_20) {
         unsigned char sr2b = 0, sr2c = 0;
         switch(ModeNo) {
	 case 0x00:
	 case 0x01: sr2b = 0x4e; sr2c = 0xe9; break;
	 case 0x04:
	 case 0x05:
	 case 0x0d: sr2b = 0x1b; sr2c = 0xe3; break;
	 }
	 if(sr2b) {
            SiS_SetReg(SiS_Pr->SiS_P3c4,0x2b,sr2b);
	    SiS_SetReg(SiS_Pr->SiS_P3c4,0x2c,sr2c);
	    SiS_SetRegByte(SiS_Pr->SiS_P3c2,(SiS_GetRegByte(SiS_Pr->SiS_P3cc) | 0x0c));
	 }
      }
      SiS_SetCRT1FIFO_310(SiS_Pr, ModeNo, ModeIdIndex);
#endif
      break;
   }

   SiS_SetCRT1ModeRegs(SiS_Pr, ModeNo, ModeIdIndex, RefreshRateTableIndex);

#ifdef SIS315H
   if(SiS_Pr->ChipType == XGI_40) {
      SiS_SetupDualChip(SiS_Pr);
   }
#endif

   SiS_LoadDAC(SiS_Pr, ModeNo, ModeIdIndex);

#ifdef SIS_LINUX_KERNEL
   if(SiS_Pr->SiS_flag_clearbuffer) {
      SiS_ClearBuffer(SiS_Pr, ModeNo);
   }
#endif

   if(!(SiS_Pr->SiS_VBInfo & (SetSimuScanMode | SwitchCRT2 | SetCRT2ToLCDA))) {
      SiS_WaitRetrace1(SiS_Pr);
      SiS_DisplayOn(SiS_Pr);
   }
}

/*********************************************/
/*       HELPER: VIDEO BRIDGE PROG CLK       */
/*********************************************/

static void
SiS_InitVB(struct SiS_Private *SiS_Pr)
{
   unsigned char *ROMAddr = SiS_Pr->VirtualRomBase;

   SiS_Pr->Init_P4_0E = 0;
   if(SiS_Pr->SiS_ROMNew) {
      SiS_Pr->Init_P4_0E = ROMAddr[0x82];
   } else if(SiS_Pr->ChipType >= XGI_40) {
      if(SiS_Pr->SiS_XGIROM) {
         SiS_Pr->Init_P4_0E = ROMAddr[0x80];
      }
   }
}

static void
SiS_ResetVB(struct SiS_Private *SiS_Pr)
{
#ifdef SIS315H
   unsigned char  *ROMAddr = SiS_Pr->VirtualRomBase;
   unsigned short temp;

   /* VB programming clock */
   if(SiS_Pr->SiS_UseROM) {
      if(SiS_Pr->ChipType < SIS_330) {
	 temp = ROMAddr[VB310Data_1_2_Offset] | 0x40;
	 if(SiS_Pr->SiS_ROMNew) temp = ROMAddr[0x80] | 0x40;
	 SiS_SetReg(SiS_Pr->SiS_Part1Port,0x02,temp);
      } else if(SiS_Pr->ChipType >= SIS_661 && SiS_Pr->ChipType < XGI_20) {
	 temp = ROMAddr[0x7e] | 0x40;
	 if(SiS_Pr->SiS_ROMNew) temp = ROMAddr[0x80] | 0x40;
	 SiS_SetReg(SiS_Pr->SiS_Part1Port,0x02,temp);
      }
   } else if(SiS_Pr->ChipType >= XGI_40) {
      temp = 0x40;
      if(SiS_Pr->SiS_XGIROM) temp |= ROMAddr[0x7e];
      /* Can we do this on any chipset? */
      SiS_SetReg(SiS_Pr->SiS_Part1Port,0x02,temp);
   }
#endif
}

/*********************************************/
/*    HELPER: SET VIDEO/CAPTURE REGISTERS    */
/*********************************************/

static void
SiS_StrangeStuff(struct SiS_Private *SiS_Pr)
{
   /* SiS65x and XGI set up some sort of "lock mode" for text
    * which locks CRT2 in some way to CRT1 timing. Disable
    * this here.
    */
#ifdef SIS315H
   if((IS_SIS651) || (IS_SISM650) ||
      SiS_Pr->ChipType == SIS_340 ||
      SiS_Pr->ChipType == XGI_40) {
      SiS_SetReg(SiS_Pr->SiS_VidCapt, 0x3f, 0x00);   /* Fiddle with capture regs */
      SiS_SetReg(SiS_Pr->SiS_VidCapt, 0x00, 0x00);
      SiS_SetReg(SiS_Pr->SiS_VidPlay, 0x00, 0x86);   /* (BIOS does NOT unlock) */
      SiS_SetRegAND(SiS_Pr->SiS_VidPlay, 0x30, 0xfe); /* Fiddle with video regs */
      SiS_SetRegAND(SiS_Pr->SiS_VidPlay, 0x3f, 0xef);
   }
   /* !!! This does not support modes < 0x13 !!! */
#endif
}

/*********************************************/
/*     HELPER: SET AGP TIMING FOR SiS760     */
/*********************************************/

static void
SiS_Handle760(struct SiS_Private *SiS_Pr)
{
#ifdef SIS315H
   unsigned int somebase;
   unsigned char temp1, temp2, temp3;

   if( (SiS_Pr->ChipType != SIS_760)                         ||
       ((SiS_GetReg(SiS_Pr->SiS_P3d4, 0x5c) & 0xf8) != 0x80) ||
       (!(SiS_Pr->SiS_SysFlags & SF_760LFB))                 ||
       (!(SiS_Pr->SiS_SysFlags & SF_760UMA)) )
      return;

#ifdef SIS_LINUX_KERNEL
   somebase = sisfb_read_mio_pci_word(SiS_Pr, 0x74);
#else
   somebase = pciReadWord(0x00001000, 0x74);
#endif
   somebase &= 0xffff;

   if(somebase == 0) return;

   temp3 = SiS_GetRegByte((somebase + 0x85)) & 0xb7;

   if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x31) & 0x40) {
      temp1 = 0x21;
      temp2 = 0x03;
      temp3 |= 0x08;
   } else {
      temp1 = 0x25;
      temp2 = 0x0b;
   }

#ifdef SIS_LINUX_KERNEL
   sisfb_write_nbridge_pci_byte(SiS_Pr, 0x7e, temp1);
   sisfb_write_nbridge_pci_byte(SiS_Pr, 0x8d, temp2);
#else
   pciWriteByte(0x00000000, 0x7e, temp1);
   pciWriteByte(0x00000000, 0x8d, temp2);
#endif

   SiS_SetRegByte((somebase + 0x85), temp3);
#endif
}

/*********************************************/
/*      X.org/XFree86: SET SCREEN PITCH      */
/*********************************************/

#ifdef SIS_XORG_XF86
static void
SiS_SetPitchCRT1(struct SiS_Private *SiS_Pr, ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
   unsigned short HDisplay = pSiS->scrnPitch >> 3;

   SiS_SetReg(SiS_Pr->SiS_P3d4,0x13,(HDisplay & 0xFF));
   SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x0E,0xF0,(HDisplay >> 8));
}

static void
SiS_SetPitchCRT2(struct SiS_Private *SiS_Pr, ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
   unsigned short HDisplay = pSiS->scrnPitch2 >> 3;

    /* Unlock CRT2 */
   if(pSiS->VGAEngine == SIS_315_VGA)
      SiS_SetRegOR(SiS_Pr->SiS_Part1Port,0x2F, 0x01);
   else
      SiS_SetRegOR(SiS_Pr->SiS_Part1Port,0x24, 0x01);

   SiS_SetReg(SiS_Pr->SiS_Part1Port,0x07,(HDisplay & 0xFF));
   SiS_SetRegANDOR(SiS_Pr->SiS_Part1Port,0x09,0xF0,(HDisplay >> 8));
}

static void
SiS_SetPitch(struct SiS_Private *SiS_Pr, ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
   bool isslavemode = false;

   if( (pSiS->VBFlags2 & VB2_VIDEOBRIDGE) &&
       ( ((pSiS->VGAEngine == SIS_300_VGA) &&
	  (SiS_GetReg(SiS_Pr->SiS_Part1Port,0x00) & 0xa0) == 0x20) ||
	 ((pSiS->VGAEngine == SIS_315_VGA) &&
	  (SiS_GetReg(SiS_Pr->SiS_Part1Port,0x00) & 0x50) == 0x10) ) ) {
      isslavemode = true;
   }

   /* We need to set pitch for CRT1 if bridge is in slave mode, too */
   if((pSiS->VBFlags & DISPTYPE_DISP1) || (isslavemode)) {
      SiS_SetPitchCRT1(SiS_Pr, pScrn);
   }
   /* We must not set the pitch for CRT2 if bridge is in slave mode */
   if((pSiS->VBFlags & DISPTYPE_DISP2) && (!isslavemode)) {
      SiS_SetPitchCRT2(SiS_Pr, pScrn);
   }
}
#endif

/*********************************************/
/*                 SiSSetMode()              */
/*********************************************/

#ifdef SIS_XORG_XF86
/* We need pScrn for setting the pitch correctly */
bool
SiSSetMode(struct SiS_Private *SiS_Pr, ScrnInfoPtr pScrn, unsigned short ModeNo, bool dosetpitch)
#else
bool
SiSSetMode(struct SiS_Private *SiS_Pr, unsigned short ModeNo)
#endif
{
   SISIOADDRESS BaseAddr = SiS_Pr->IOAddress;
   unsigned short RealModeNo, ModeIdIndex;
   unsigned char  backupreg = 0;
#ifdef SIS_LINUX_KERNEL
   unsigned short KeepLockReg;

   SiS_Pr->UseCustomMode = false;
   SiS_Pr->CRT1UsesCustomMode = false;
#endif

   SiS_Pr->SiS_flag_clearbuffer = 0;

   if(SiS_Pr->UseCustomMode) {
      ModeNo = 0xfe;
   } else {
#ifdef SIS_LINUX_KERNEL
      if(!(ModeNo & 0x80)) SiS_Pr->SiS_flag_clearbuffer = 1;
#endif
      ModeNo &= 0x7f;
   }

   /* Don't use FSTN mode for CRT1 */
   RealModeNo = ModeNo;
   if(ModeNo == 0x5b) ModeNo = 0x56;

   SiSInitPtr(SiS_Pr);
   SiSRegInit(SiS_Pr, BaseAddr);
   SiS_GetSysFlags(SiS_Pr);

   SiS_Pr->SiS_VGAINFO = 0x11;
#if defined(SIS_XORG_XF86) && (defined(i386) || defined(__i386) || defined(__i386__) || defined(__AMD64__) || defined(__amd64__) || defined(__x86_64__))
   if(pScrn) SiS_Pr->SiS_VGAINFO = SiS_GetSetBIOSScratch(pScrn, 0x489, 0xff);
#endif

#ifdef SIS_LINUX_KERNEL
   KeepLockReg = SiS_GetReg(SiS_Pr->SiS_P3c4,0x05);
#endif
   SiS_SetReg(SiS_Pr->SiS_P3c4,0x05,0x86);

   SiSInitPCIetc(SiS_Pr);
   SiSSetLVDSetc(SiS_Pr);
   SiSDetermineROMUsage(SiS_Pr);

   SiS_UnLockCRT2(SiS_Pr);

   if(!SiS_Pr->UseCustomMode) {
      if(!(SiS_SearchModeID(SiS_Pr, &ModeNo, &ModeIdIndex))) return false;
   } else {
      ModeIdIndex = 0;
   }

   SiS_GetVBType(SiS_Pr);

   /* Init/restore some VB registers */
   SiS_InitVB(SiS_Pr);
   if(SiS_Pr->SiS_VBType & VB_SIS30xBLV) {
      if(SiS_Pr->ChipType >= SIS_315H) {
         SiS_ResetVB(SiS_Pr);
	 SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x32,0x10);
	 SiS_SetRegOR(SiS_Pr->SiS_Part2Port,0x00,0x0c);
         backupreg = SiS_GetReg(SiS_Pr->SiS_P3d4,0x38);
      } else {
         backupreg = SiS_GetReg(SiS_Pr->SiS_P3d4,0x35);
      }
   }

   /* Get VB information (connectors, connected devices) */
   SiS_GetVBInfo(SiS_Pr, ModeNo, ModeIdIndex, (SiS_Pr->UseCustomMode) ? 0 : 1);
   SiS_SetYPbPr(SiS_Pr);
   SiS_SetTVMode(SiS_Pr, ModeNo, ModeIdIndex);
   SiS_GetLCDResInfo(SiS_Pr, ModeNo, ModeIdIndex);
   SiS_SetLowModeTest(SiS_Pr, ModeNo);

#ifdef SIS_LINUX_KERNEL
   /* Check memory size (kernel framebuffer driver only) */
   if(!SiS_CheckMemorySize(SiS_Pr, ModeNo, ModeIdIndex)) {
      return false;
   }
#endif

   SiS_OpenCRTC(SiS_Pr);

   if(SiS_Pr->UseCustomMode) {
      SiS_Pr->CRT1UsesCustomMode = true;
      SiS_Pr->CSRClock_CRT1 = SiS_Pr->CSRClock;
      SiS_Pr->CModeFlag_CRT1 = SiS_Pr->CModeFlag;
   } else {
      SiS_Pr->CRT1UsesCustomMode = false;
   }

   /* Set mode on CRT1 */
   if( (SiS_Pr->SiS_VBInfo & (SetSimuScanMode | SetCRT2ToLCDA)) ||
       (!(SiS_Pr->SiS_VBInfo & SwitchCRT2)) ) {
      SiS_SetCRT1Group(SiS_Pr, ModeNo, ModeIdIndex);
   }

   /* Set mode on CRT2 */
   if(SiS_Pr->SiS_VBInfo & (SetSimuScanMode | SwitchCRT2 | SetCRT2ToLCDA)) {
      if( (SiS_Pr->SiS_VBType & VB_SISVB)    ||
	  (SiS_Pr->SiS_IF_DEF_LVDS     == 1) ||
	  (SiS_Pr->SiS_IF_DEF_CH70xx   != 0) ||
	  (SiS_Pr->SiS_IF_DEF_TRUMPION != 0) ) {
	 SiS_SetCRT2Group(SiS_Pr, RealModeNo);
      }
   }

   SiS_HandleCRT1(SiS_Pr);

   SiS_StrangeStuff(SiS_Pr);

   SiS_DisplayOn(SiS_Pr);
   SiS_SetRegByte(SiS_Pr->SiS_P3c6,0xFF);

#ifdef SIS315H
   if(SiS_Pr->ChipType >= SIS_315H) {
      if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
	 if(!(SiS_IsDualEdge(SiS_Pr))) {
	    SiS_SetRegAND(SiS_Pr->SiS_Part1Port,0x13,0xfb);
	 }
      }
   }
#endif

   if(SiS_Pr->SiS_VBType & VB_SIS30xBLV) {
      if(SiS_Pr->ChipType >= SIS_315H) {
#ifdef SIS315H
	 if(!SiS_Pr->SiS_ROMNew) {
	    if(SiS_IsVAMode(SiS_Pr)) {
	       SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x35,0x01);
	    } else {
	       SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x35,0xFE);
	    }
	 }

	 SiS_SetReg(SiS_Pr->SiS_P3d4,0x38,backupreg);

	 if((IS_SIS650) && (SiS_GetReg(SiS_Pr->SiS_P3d4,0x30) & 0xfc)) {
	    if((ModeNo == 0x03) || (ModeNo == 0x10)) {
	       SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x51,0x80);
	       SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x56,0x08);
	    }
	 }

	 if(SiS_GetReg(SiS_Pr->SiS_P3d4,0x30) & SetCRT2ToLCD) {
	    SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x38,0xfc);
	 }
#endif
      } else if((SiS_Pr->ChipType == SIS_630) ||
	        (SiS_Pr->ChipType == SIS_730)) {
	 SiS_SetReg(SiS_Pr->SiS_P3d4,0x35,backupreg);
      }
   }

#ifdef SIS_XORG_XF86
   if(pScrn) {
      /* SetPitch: Adapt to virtual size & position */
      if((ModeNo > 0x13) && (dosetpitch)) {
	 SiS_SetPitch(SiS_Pr, pScrn);
      }

      /* Backup/Set ModeNo in BIOS scratch area */
      SiS_GetSetModeID(pScrn, ModeNo);
   }
#endif

   SiS_CloseCRTC(SiS_Pr);

   SiS_Handle760(SiS_Pr);

#ifdef SIS_LINUX_KERNEL
   /* We never lock registers in XF86 */
   if(KeepLockReg != 0xA1) SiS_SetReg(SiS_Pr->SiS_P3c4,0x05,0x00);
#endif

   return true;
}

/*********************************************/
/*       X.org/XFree86: SiSBIOSSetMode()     */
/*           for non-Dual-Head mode          */
/*********************************************/

#ifdef SIS_XORG_XF86
bool
SiSBIOSSetMode(struct SiS_Private *SiS_Pr, ScrnInfoPtr pScrn,
               DisplayModePtr mode, bool IsCustom)
{
   SISPtr pSiS = SISPTR(pScrn);
   unsigned short ModeNo = 0;

   SiS_Pr->UseCustomMode = false;

   if((IsCustom) && (SiS_CheckBuildCustomMode(pScrn, mode, pSiS->VBFlags))) {

      xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3, "Setting custom mode %dx%d\n",
		SiS_Pr->CHDisplay,
		(mode->Flags & V_INTERLACE ? SiS_Pr->CVDisplay * 2 :
		   (mode->Flags & V_DBLSCAN ? SiS_Pr->CVDisplay / 2 :
		      SiS_Pr->CVDisplay)));

   } else {

      /* Don't need vbflags here; checks done earlier */
      ModeNo = SiS_GetModeNumber(pScrn, mode, pSiS->VBFlags);
      if(!ModeNo) return false;

      xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3, "Setting standard mode 0x%x\n", ModeNo);

   }

   return(SiSSetMode(SiS_Pr, pScrn, ModeNo, true));
}

/*********************************************/
/*    X.org/XFree86: SiSBIOSSetModeCRT2()    */
/*           for Dual-Head modes             */
/*********************************************/

bool
SiSBIOSSetModeCRT2(struct SiS_Private *SiS_Pr, ScrnInfoPtr pScrn,
               DisplayModePtr mode, bool IsCustom)
{
   SISIOADDRESS BaseAddr = SiS_Pr->IOAddress;
   SISPtr  pSiS =$ */
TR(pScrn);
#ifdef$ */DUALHEADe86$ */Ent
/* XdotEnt =n) fo->entityPrivate;
#endife86$unsigned short ModeIdIndexee86$    SiS 315/550/[M]No = 0/[M]661[FGM]X/char  backupreg30/[M]e86$ iS_Pr->UseCustom/[M] = false/V5/V8/* Remember: iversa modes for CRT2 are ONLY supportede86$ *ight -) on the 30x/B/C, anyright (C) 2001if/XFreeis LCD or VGA,*
 *CRT1stria
 Aright /V5/V8if((Isiversa) && (, Z7CheckBuildiversal mo*
 * M, and ,for SiSVBFlags))) {

	x[GX]/330/[xfr Linux} elseis free softwar, Z7Get[GX]/u fra* apply:
 *
 * * This progra;
	Linu![GX]/3) returnle for Linux}V5/V8, ZRegInitfolloPr, Base/* $)ee86$ iSe FrPtree Softation; eimodiSysprogrrsion 2 of#if defined(i386) || later ve__rsion.
 * *
 * * This p__n.
 * *
 * * ThAMD64ed in the hope thamdit will be useful,
x86_it wi5/V8, Z7
 * e naVGAINFOnd/or modiSetBIOSScratch* apply:0x489LITYffor aediswithout even the implied w0x11630/730,
5/V8, Z7SetRegee Soften theP3c4,0x05,0x86)/V5/V8, Zther CIetcrsion 2 of the nSetLVDSld have received a DetermineROMUsagersion 2 ofinux keSave and  info so we can set it from within Sdify
 .org/XFr1of t initializing code (CRif(or SiSDualHead/[M] is (C) 2n) for
 ->XFre[GX]/330/[GX]/3ee86$07, USA
 *
 * OtDl moduland llowing license terms kernel, =  kernel,llowing license termsCR30nd/or modiic License for mord det30ation;ry forms, with or w1thout
 * * modification, are permi1ted provided that the fol5thout
 * * modification, are permi5ted provided that the fol8thout
 * * modification, are permi8or any 01-1307,/* Wite t'to theXFreeam; ibeforetributam; ifso the- says who...?of t proviite 330
 *
 * O1[GX]/330= -1 is 	 xf86DrvMsgVerb*
 * M->scrn0/651, X_plie, 3,
		"Settingibutions indelayed until aftero thdocumentrm mus\n" Geneicense tru * * Redi}30/730,
 * 07, USA
 *
 * OtherwSe * f.
 * * 3)e name of, Suite, Z7
 * (Universal mo is f the a    SiS 315/550temp spend/or mse fCVDisplayith the products
 * C/[M]prog & DoubleScanved frTHE  specific>>= 1ed proviedist* *
 * * THIInfoFTWARE Interlaceved fr specific<<IS''  this stions and the following disclaimer in the
 *       documcuffer and  %dx%d1-20XFre\n"ABILI
 * * THIHen perm,UDING, BUwith th redistribITED TO, THE IMPLIED WARRANTIES
 * * OF MERCHANTABILITY AND FIstandardm must0x%xCULAR PURPOS the foENT SHALGeneral PUnLockXFre along with thral ucts
 * *    derived fromion.
 * *!ductsSearch/[M]6Dee Softwa&[GX]/3ATA, OR650/651am iicense as publSHALL THE AUion.
 /[M]650/65130/[M]76xLUDING, BUGetVBTyp* along with the nae FrVBrsion 2 of throducts
 *  the , WHE & VB_SIS30xBLVGOODS OR SERV
 * * THIhipUDING>rg$ *_315H condie naResITY,rsion 2 of OF
 *ublic ORLicense for more det32,0x1tted EVEN IF ADVISED OF THE POSSart2Portdeta0detacCH DAI Volari V3Xst of conditions and the following  3) Theredistris@winischhofer.net>
 *
 * Formerly based on nyright
 * }HEORY OF L/* Get VBif normation (connectors, includeed devices)copyrigROCUREMENT OF SUBSTITUTE GOODS OR SIABILITY, IMPee Softwa, OR PRO/[M]650/651, * 1) ReOWEVER CAUSED A/* If thist reaITNESS FOR A,writdoRedicing 005 band fTWARorg/XFre* Fouopyright
0vtbl.h"
#endif

#ifdef SIS315H
#include "3tted prssionVEN IF YPbPersion 2 of the naSetTVms
 *f

#ifdef SIS315H
#include  of the namedLCDRes#endif

#ifdef SIS315H
#include       */
/***Low/[M]Tesree Softwa DAMAGES (INCF
 * * THSegmen* moistere,
 * * or h this petm mustULAR PUclude "ini follo * * (INCLUDING NEGLIGEVB THE ||functioS_Pr->SiS_StReIF_DEF_y ofTHE A== 1n.
 o;
   SiS_Pr->SiS_ModeResInfCH70xx   != 0eResInfo;
   SiS_Pr->SiS_StandTaTRUMPION SiS_Stnclude "300vtbSetXFreGroup***************  POINTEER INITIAtrangeStuf ARISING* *
 * * Y_en permOnN            */
/***RegByt******** for more6,0xFFD TO, PROCRISING IN ANY WAY OUT OF THE USEion.
 * *
 * * TH_ModeResInfo   =_ModeRe-frERVICES; IsBostEd * along wm is 	Timing;
   RegAN,
 * * D 	Thomas W1nischho13,0xfbCH DAssionrmission.
 */
R TORT
 * * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OROCUREMENT OF
 * OMNewup3Data;
St2TimiIsVA***********oup3Data;
a;
   SiS_Pr-ISED OF THE POSSI * Useddeta* 1)ata;
nal code-fr;
   SiS_Pr->SiS>SiS_HiTVGroup3SiiS_ExtPALDaFE;
   SiS_imu;
 DAMAGE.
 *
 itions and the followi,I Volari deIDGrouput
 * * modification, are permitt &  = XFreToLCDup3Data;
   SiS_Pr->SiS_HiTVGroup3SiS_St1HiTV0xfhomas@;
#if 0
 NY EXPRES2Timing;

   SiS_P=OUT OF6a = ||_StNTSCDaS_Pr->SiS__StNTSCData;
   730m is iS_St525ral Public License for morS_ExtPALData = SiS_S permission.
 */

#iSetPitch: Adapt to virtual sizNG NposiIG_H
opyrigng;
   Data; NOT LIMITE * ** ModeNTSCDataHandle760r->SiS_HiTVSt1ibution.
 * *}

/* = &SiS_OutputSelect;
   SiS_Pr->pSiS_SoftSe/
/ (C) X.org/XFree86:CDat
 * *dify
 ed w( THE tting  = ta     org/Bost-on,  and X.ta;
   SiS_Ptting= &SiS_OutputSelect;
   SiS_Pr->pSiS_SoftSetti
bool
g;

   SiS_Pr->SiS_structRE DISC/540/ *SiS_Ext70pDa IMPtion) pplyPr->SiS_StS_LCD1280xen perm/[M]
/* pragma;
  ource and)
_Pr->SISIOADDRESSare Foundc prior wriIO/* $XFree86$ */
/* $XdotOrg$ */
/*
 * Mode *     SiS 315/550/[M]650/651truct Si30/[M]76x[GX],
 *     XGI Volari V3XT/V initializing code (CRT1 section) for
 * for SiS 300/305/540/6376x[GX],
 *     XGI Volacr30,a;
   SiS_1r->SiS_ExtL8r->SiS_ExtL5r->SiS_Ep40d=[M]76x1280x;
   Sie and b* GNU General P
 * (Universal module for Linuxinux kernel, the following license terms
 * apply:
 *
 * * This program is freoftware without specific prior written permissHiTVData;* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' 	NY EXPRESS OR
 * * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMditions and the following disclaimer in the
 *  *    documTNESS FOR A PARTICULAR P1RPOSE A	E DISCLAIMED.
 * * IN NO EVENTree software; you can redistribute it and/or modify
 * * it under the ter0); defloc_tegive s progrr->Sneral Public License as publiSiS_LVDS320x240Data_2   = SiS_LVDS320x240Data_2;
   SiS_PrAL, EXEMPLARY, OR CONSEQU  = Sruct SiS_PEORY OF LIABther version 2 of the n the Free Software Foundation; eiamed License,
 * * or any later version.
 * *
 * * This program is distributed in the hope that it will be useful,
 * * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * * GNU General Public License for more details.
 * *
 * * You should have received a copy of the GNU General Public License
 * * along with th, BUT
 * * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * * THEORY OF L/* blic Lice CLUDINGxtNTSCDataLITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USES_LCD1280winischhofer.net>
 *
 * Formerly based on non-functional code-x600;    /* lowest value LVDS/LCDA */
   SiS_Pr->SiSd by permission.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "/* (Walloc_tex86 4ined(e curren SiS_SMOC_PRbutions include "*********************************/
/*         POINNITIALIZATION            */
/*********************************************/

#if defined(SIS300) || defined(SIS315H)
static void
InitCommonPointer(struct SiS_Private *OpenCRTCr->SiS_HiTVSt1le  = SiS_SModeIDT, Inciming;
   SiS1Pr->SiS_PALTiming   ned(SIS315H)
staticR TORT
 * * (INCL IMP SiS_St2HiTVDatAS_NTSCTiming;
   SiS_Pr->SiS_PALTiming      = SiS_PALxtNTSCData;
   SiS_Pr->SiS_Ext525pData = SiS_ExtNTSCData;
   SiS_Pr1>SiS_Ext750pData = SiS_Ext750pD  SiS_Pr->Sta = SiS_Exng;
   SiS_Pr->SiS_HiTVSt1TimiClos

  ta_0 = SiS30., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * O this lis the following license term1 apply:
 *
 * * R or promote products
 * *    derived from;
   SiS_P  = SRT1Usekernel,l modul.
 * * 3) ThData  = SSRClock_ 300  prior writ00_ExtLx1024Data  = SiS3S SOFTWACD1280x1024Data;
S SOFTWAON) HOWEVER CAUSED A68Data;
   SiS_Pr->SiS_ExtLCDRUPTION) HOLinux kernstributioif    ngocum_300; /* 300 */
., 59 Temple Place, Suite 330, Boston, MA 02111-1307, * *    notice,therwise!t of conditions and the following disclaimer in the
 * *		"(Re-)   documOR A org/XFrehe distrS_Pr->SiS_LC0x1024Data;(Universal mo_Pr->SiS_CHrwithout
 * * modification, are permittedta;
   SiS_lowing conditions
 * * are met:
 * * 1)ta;
   SiS_urce code must retain the above copyrigta;
   SiS_s list of conditions and the following iS_Pr->SiS_NoStResInfo     = SiS_StR3Data;
/* B Vola LUT-enablSiS_Pr->SiS_Suthor may not be useALData;
   SCD1400x105fer.net>
 *
 * Formerly basart4nischhofd= Si0x08TSCData = SiSLNData = SiS300_CHT, 730 */
   }
   SiS_Pr-ata;
   SiS_Pr-dification, are permit,forms, with or wi;
   SiS750pData  = SiS_St750pData;
   1480Data_1;
   SiSa;
   SiS750pData  = SiS_St750pData;
   Sitributions of sou SiS_Pr->SiS_LVDSBARCO1024Data_1 = SiS380_LVDSBARCO1024DaVUPALNiS_ExtNT
   SiS_Pr->Si->SiS_Ext750   noti;
   _1ALDating license terms applO1366Data_2n and use inS_StHi750pData  = SiS_St750pData;
   0iS_Pr->iS_PCH DAMAGE.
 *
 0Data_2   = SiS300_LVDS;    /* nolType04_2a = SiS300_PanelType04_2a;
SiS_Pr->TVOPlType04_2a = SiS300_PanelType04_2a;
TVData =ed oUPALNData = SiS300_CHTVUPALData;  /* not suppota  = SiS_StN* Author: 	Thomas Ws */
   Si, ~->SitLCD1400x1051366Dat = SiS
 * (Universal modulS_Pr->SiS_LCD1#if 0
   SiS_r promote p * *arning: Fe Sohere,
   SiSESS FOR A entries re
 r->Sie86 right (possibly overwrittenart of the LTiming  = SiS_HiTVSt1Timing;
   SiS_Pr->SiS_HiTVSt2Timing  = SiS_HiTVSt2Timing;
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *_PanelType04_2b = SiS300_PanelType0SiS_Ext525iDa_St525iData  = SiS_StNTSCData;
   SiS_Pr->SiS_LCD1280x8525pData  = SiS_St525pData;
   SiS_Pr->SiS_St750pData  = SiS_St750pData;
   SiS_Pr->SiS_Ext525iData = SiS_ES_Ext750pData;

   SiS_Pr->rted on 3/ = S[GX]/33in 
 *  s MERCHe86 aiS_Pr->SiS_CHTSiS_Pr-ID* apply DAMAGES (INCpSiS_OutputSele0/730,TVReLinux_XF86of th#ifnitiaGETBITSTR
#later  BITMASK(h,l THE 	(((    SiS )(1U << ((h)-(l)+1))-1)<<(l))on 300 seGENs */
mask THE	ries */
1?S_CH,0SiS30PAL;
   SiS_porte(var,S_CHTVVCL((Pr->= SiS_Pr->SiS_CHT) >> (_CHTVVCCLKUNTSC;
   SiS_TR(val,ee S,to)  ((   SiS_Pr-L  = S))OPAL 0?toSiS_ promotvoid SiS_CalcCR SiS_Pr->S280x768_2Data = SiS_ExtLCD12int depth
   SiSM =  * *1Data_Fix syncSiS_Pr->SiS_  = S   S
   [0]  =0_CHE DISCLAIMTotalKONT3) - 5S_Pr->ff;		/* CR0iS_Pr->SiS_ series */
   1iS_Pr->E DISCLAIMED.
 * M = SiS301;	ONTSC; 0 */
   } el series */
   2ries */
   SiS_PrBlankStartHTVVCLKUPALN = SiS3able;
   300 series */
   3iS_Pr->Sed on 300 seriesEndHTVVCLKUPAS_Pr->1F) | 0x80;NTSC; 30_CHTVVCLKUPAL;   /* not 4ries */
   SiS_PrSync */
   SiS_P+ 3LN = SiS340_CHTVVCLKUPAL;   /* not 5PAL;   /** not supported on 300 series */
  20S_Pr-2) |NTSC; 5/
   LVDSBA *SiS_Pr)
{
 #ifdon 300 ser+ SiS/
   Si/V5/V8, Z7
 * ies */
   6ries */
   SiS_PVKOPALM->SiS_-   SPr->FFLN = SiS3AL;  ALN = SiS300_CHTVVCLK7PAL;   /* not suppSiS_Pr->SiSRT1Table 100LKONT8)ONTSC; 7iS_EModeI|ype >= SIS_340->SiS_CH  Sis */
   S_MCLKD7)0_MCLKData_0_340;  /*#ifdef SIS- x
   } else if(6iS_Pr->ChipType >= SISeries */
  */
   } else if(5iS_Pr->Ch} el0_MCLKData_0_340;  /*) {
      SiS_Pr->S2S_MCLKD4iS_Pr->ChipType >= SIS 340 + XGI */
   }       Si3iS_Pr->ChipType >= SIS_761) {
      SiS_P      Si2->SiS_RefIndex      = SiS16] =te *SiS_Pr)
{
KData_0_761LKData_0_760;  /* 4prelimi; NTSC; 9of the LinuSiS30 = S8ExtTiming  = SiS_HiTVPr->SiS_CHT= 16S_MC024x768Data;
 S_MCLKData_0 | the60KONTSCSRE**********Y EXPRESS OR
 * *LKData_0 = S6410_Mata_0_330;  /* 330 */
   } 4 THEORY OF LIABIdex      = SiS80 = fIndex;
   S#ifdef SIS     SiS_P    = SiS311 /* not supported on 300 s9
   }  else {
      Son 3->SiS_MCLKD0SiS_Pr->SiS = SiS300_CHTVVCLKUPAL;   /* not 100 = Srior written perm    Sis */
  ata_0 = SiS3CLKOPALN = SiS300_CHTVVCLK11MCLKData_1_340;a_0_660;  /* 661/741ata_0 = SiS3SiS_Eta_1 = SiS310_MCLKData21;
   }
   SiS_Pr->S>ChipTypata      = SiS310_VAL;   ta_1 = SiS310_MCLKData3] =DataN = SiSRAiS_EMod_CHTVVCLKUiS_Pr->ChipType >= SIS2), 10:10, 0:S_Prbl     = SiS310_PanelDela 340 + XGI 1_Pr->SiS_P1:deRebl     = SiS310_PanelDelaData_0_761;S;

   SiS_2:  Sibl     = SiS310_PanelDela_761) {
   x_Pr->SiS_P3:3>SiS_St2LCD1024x768Data   = SiS31>ChipLCD10  8:8, 4:4;
   SiS_Pr->SiS_ExtLCD1024x768D>ChipT  1024D4:4, 5:5)      SiS_Pr->SiS_MCLKData4Pr->SiS_PanelDeBayTbl     = SiS310_PanelDelLKOPALM = Si = SiS300, 9ta  1nelDelayTblLVDS = SiS310_Paner->SiS_CHSiS310_CRT21art2_103ta;
   SiS_Pr->SiS_ExtLCD1024series */
   SiS_Pr->art2_105 SiS310_St2LCD1280x1024Data;

#ifdef SISIDTable;
 art2_107:61024D = SiS310_SR15;

   SiS5Pr->SiS_PanelDeCSiS_Pr->SiS_CRT2Part2_1024x7rted on 300 seriesiS_Pr1024x768_1;

   SiS_Pr->SiS_CHTVU  SiS_Pr-PALMData;
 5:58Data;
SelecL  = SiS300_C SiS   STiming;
   SiS_Pr->SiS_CHTVVCLKUPAL61[FGM]X/[M]74x[GX]/3 * *    SiS 315/550/[M]650/651
   SiS    SiS 315/550SSetMode* IN Nax* IN Nb * * , remaiOPAL30/[M]76x[GX],
 * 15/550VGAHDE0x1024Data; the imHDE0_MCLKnt i, jeIDTable 1:1 data: useOPAL;o thebyo thcrt1crtc(nclude "iniSiS_Pr->SiS_LCD, 730 *LCDPass11ESS INTEa = SiSSetMode)d/or modify
 prog*******************************_MCLKDaSSetMode)& HalfDCLK)1UPAL;
 S IS''  = SiS310_SR1r->SiS_CH=1UPAL;
SiS300_EMPALData  = SiS310PAL;
   Si = SiS310_SR1lDelayTbl  SiS_Pr->SiS_CHTV  SiS_Pr->SiS_CH_Pr->SiS_VCL->SiS_CHTVReg_UPALM = _MCLKData_630; N ANY WAY<UT OF THE USE initializ30laimer.
  SiS_Pr-SiS_Pr->SiS_CHTVTRT1UPAL;_CHTVCRT1OPAL;

   SiS_PrDontExpandData;
 S_LCD1280= SiS310_CHTVReg_Panel   SiS_Pr-;
#if 0
TVReg_ONTSC;
   SiS_Pr->  SiS_PS IS'' AND ANS_CHTVCRT1UP  SiS_P% 8630/730,
 * nal code-->SiS_CHTVR15Hf

#if defOKLData SiS, o   =opyright
r->SiS_CHTVReg_SOPAL = S -HTVReg_SOPAL =XReree86$PAL  = a310_CHTVReg_UPALN;
 DE;HTVRenot /2 !copyright
 * S_CHTVReg_OPALN = SiS310_CHTVReg_OPALN;
   SiS_Pr->SSiS310_CHTVVC>SiS_CHTVVCLKOPAL;
#if 0
  SiS_P+SC;
  a1/[M]6_OPAL;

   SiS_Pr->SiS_CHTVVCLKUNT-PAL;
   Si>SiS_CHTVVCAL  = SiS310_CKOPALM prior writ = SiS310_SC;
   S_OPALM = SiS310_CHTVReg_OPALM;
   SiS_Pr->SiS_CHTVReg_UPALN LM = SiS310_CHTUPAL;
  SiS310_CHTVVCLKOPAL USE OF
 *ta = SiS310_CHTV310_CHTVVCLKOPAL;
   +  }
   if(SPAL = RS +ta;

 ~SiS_PanelTta = SiS310_CHiS_Pr->SiS_C#ifdef SIS+HTVReg_SOPAL = REPALNDateg_ONTSC;
   SiS_Pr->S_LVDS848x4ndif

#ifdef SIS31iS_Pr->L  = SiS310_C0_EModeIDTiS_Pr->S_Pr->SiS_St525iDaS_CHTVReg_OPALN = SiS310_CHTVReg_OPALN;	OPALM = Si_Pr);
#else
  CHTV
   SiS_Pr-)
{
   if(LKONT_Pr->  SiS_Pr-_Pr);
#else
      return faiS_Pr);
#else
      return false;
#PALM =  true;
}

/*VCLKUNTSC = Si  SiS_Pr->SiS_ef SIS300
    (ointer(SiPALM = , boolTVVC 7urn f7/

#ifnd = SiS310_CHTVVCLKHRr(Si int AL;

   SiS_Pr->SiS_CHTVVCLKne, unsigned#endif
   } else {
#ALMData = SiS310_CHTV, bool Fidth, inunctional code-fr) {
#ifdef SIS300
      InitTo300PointeriS_Pr);
#else
      return false;
#endif
   }
   return true;
}

/*     */
/*SiS_CHTVVCLKOPALM   SiS_Pr-S315H
     ) / SiS<<ue;
}

/*******************
#ifdef SIS315H
      InUPALN = 	S_Pr->SiS_StNTScase 320:
		if(VDisplay == 200) ModeIndex =SiS_CHTVVCLKOPALM/ 10le;
width, int && (LCDwidth >= 6eIndex =_Pr-mu;
#if 0
  >SiS_CHTVVCLKONTSC = SiS310_CHTVVCLKONTSLCDheightCHTVCRT1OPAPr->SiS_CHTVReg_OPALN = SiS310_CHTVReg_OPALN;
#ifndef SIS310_CHTVVCLKOPALM;ModeIndex = 0;

   switch(HDisFlags, int Hbool FSTlse
r->SiS_C= ModeInd****_Ext525iData =4) && (LCD+ht)
{
   unsignedSx1024Data  = SiS3f SIS300
    UPALN = SiS310isplay == 400) ModeIndex = 768))) 	case 400:
		if((!(VUPALN = LN = SiS310_0x20ex_512x384[Depth];
		}V;
   SiS_Pr->SiS_YHTVVCLKO  = SiS310_CHTVVCLKOPAL;LM = SiSLM = SiS310_CHTVVCLKUPALM;
   SiS_Pr->SiS_CHTVVCLCDheight)
{
   unsign20x576[De*********/
/*     HTVReg_OPALM;
   SiS_Pr->SiS_CHTVReg_UPALN nelDtupid hackLData64LKDa0/32SIS_6HTVVCLKUPALM = SiS310_CHTVVCefined(SiS_PAL =_1024x76 {
  ModeI Mode FSTN, int )SiS_438TVVCLKUNT+ SiS20x200[Depth];
iData  = SiS[Depth];
		break;
	case 80} elS_Pr->S	x_800x480[Depth];
		break;
	case 848:
eInd           */
/0/

#ifndef SIreak;
	case 768:SiS310_CHTVLN = SiS310_CHTVVCLKOPALpType > prior writtHTVVCLKSOPAL = Si) || ((LCDw6[Depth];
		break;
	case 768:
		if(VDisplay == 576) ModeIndex = ModeIndex_768x576[Depth];
	ex =reak;
	case 800:
dex_640x480[Dep POINTER Iisplay == 400) ModeIndeV = ModextLCD1024x768Data   	break;
	casee if(VDisplay == 768) Mod = SiS310_CHTVRedeIndex_720x480[DeALM = SiS310_CHTVReg_OPALM;
   SiS_Pr_MCLKData_0_330x768Data  --    ModeIndex = M(VDispla
		if(ViS_PALTimi00_CHTVVCLKOPAL;SiS_Ext7on-funcVOPALMData = SiS310_CH&=S300F8Engine == SIS_300_VGA) {
		|= (S_CHTVCRT1<< 4240_2;
   r->SiS_MCLKData_0 	if(VDET/V5/V8, Z7>SiS_ExtHiTVData = SiS_ExtHiT11,0x7ESS ;
		for(iPr->Sijngine i <= 7; i++, j++S_NTSCTiming;
    SiS300_PanelType04_2j,efIndex      = SiSi]  POINTER Iex_1[Deptx1th];
			768[Dak;
			case 768:
				if(VGAEngine == SIS_300_VGA) {
					ModeIndex = ModeIndex_300_1280x758[Depth]2
				} else {
					ModeIndex = ModeIndex_310_1280x768[Depth];
				}
				break;
			case 800:0A8[Depth]				ak;
			case 768:
				if(VGAEngine == SIS_3c0_VGA) {
					ModeIndex = ModeIn6[Depth];
		break;
	creak;
	case 128 		switTVCRT1UNTSC;
   SiS_Pr->SiS_CHTVre detaEdeInF, ModeIn600) ModeIndex Data_1_340 960:
				ModeInd01		}
	5 ModeIndeg_ONTSC;
 IS PROVIDED BY T == 480   } 8x = ModeIndex_1280x960[Depth];
				b perm09,0x5se 1024:
			 initializ_XORGg_OPA== SIS_3TWDEBUG= Motions and (0mer in the"%d _300_136000_1360x10x102(_300_1360x1)RPOSE E DISCLAIMED.
 * * deIndex_320x240[Dep == SIS_315_VGA)End == SIS_315_KOPAL1400:
		if(VlDelayTb == SIS_315x768Data  050[Depth];
			}1050) {
				MopType 1400:
		if(VGeries */
  == SIS_315__St2LCD1:
		if(VDispndex = ModeIndex_1600h];
			el240_1;) ModeIndex = ModeIndex {{0x%02x,== 1050) ModeIndex = ModeIndex_1680x1050[Depth];
se 1400:
		if(Ves */
   Si == SIS_315 on 300 serk;
	case 1920:
		if(V2isplay == 1440) ModeI3dex = ModeIndex_1920x14isplay == 1440) ModeI5dex = ModeIndex_1920x16isplay == 1440) ModeI7 = Mode315_VGA) {
			if(VDispla , OR1050) ModeIndex = ModeIndex_1680x1050[Depth];
		}
		break;
	case 1920:
		if(V8isplay == 1440) ModeI9dex = ModeIndex_1920x11Display == 1440) ModeInndex = ModeIndex_1920x11440[Depth];
		else if(1VGAEngine == SIS_315_VG1A) {
			if(VDisplay ==15
		}
		break;
	case 2048:
		if(VDisplay}}breakSiS_MCLKData_0 = SiS31SS FO730,
th, booSiS_CHTVSOPAGeneric_ConvertCRDataVSOPALData;

   SiS_Pr->SiS_CHTVCRT1UN    X*crPAL;ALDatCLKUNresALM = yshor== SIS_300_VGA) {
			i			00Data      = SiS_Pr->pth, bool SIS_300_VLINUX_KERNEL2 & 280x768fb_var_screenf not*r->S
   Si1SOPeresol FSTN,_CHTVCRT1ONTSC = SiS31HRE, HB) {
RS {
	DispD) {
  SiS_SiS310_CHTVCRT1U)) {V		ifVVDisVlay V= 20isplay [GX],
 *     XGsr_{
    cVDisplay == 2402T1OPAL  = ModeIndex A, B, C, D, E, F* IN NodeInd(VDispl = )
{
  horteIDTable HorizonPALMtOPALMKData;HT   }f(VGAEn0]KData    SiS 315/55)(	   elsePr->S
		}
	on-funcA =eInd+brea== SIS_315_VGA) {
d   } elries */end   ModeI;
   f(VGAEngdex_320x240_FSTN[Depth];
		   }
		}C		}
	 * *   
   ter(Si_UPAL  IS_315_VGA) {
retrace (=supp)DSCRrt   ModeIRtOrgf(VGAEn4dex_320x240_FSTN[Depth];
		   }
		Cr);

   300_VFA) &RS - E - 3case 400:
	     if((briesDisplay == 300B ModeIndex 2dex_320x240_FSTN[Depth];
		   }
		a = 68[Dept];
		   else if(VGAEng5]300_V == 240ModeIndex _TRU    if((CustomT != CUT_PPANEL848) &B
   (f(VGAEn3odeInd1f;
   ModeInde320x240_FSTN[DepthON))) {
eInd8661) {
      if(VDisplay == 384) {
		 ];
		   }
		}
	     * *
 * & VB2_TRUMPION))) {
		   if(VDPANEL848) &RCDheigh      Mode		    (    }
		   }
	4SIS_33
				ModeInA) &B512:((512:s */
25yright
BDheiR ``ASS_St?se if(:T != CU+ 25 * *
 * e if(VDiRplay == + Fe;
   Si6       ComT != CUT_PANEL848) && (Custom600_VGA) D = B - F - Ce == SIS_300_VGA) {
			iMPIOS_Pr->->UPALData  == (E *  }
	   dex = ModeDisplay,
		in[Depth300)FDepth];
	     break;
	ca>ChipT:
	     if(CustomT 300)CDepth];
	     break;68_1  = Si   if(VDisplay == 480) ModeIn300)DDepth];f(VDisplay == 1024) ModeIndex = ModeInd* * H: A;
		B;
		C;
		D;
		E;
		F;
		}HT;
		&& (ndexRS56x48B[Depth]856x480856xRPOSE 	 ModeIndex_320x240HTy == 200VDisplay =		if(VtNTSFOR A PAR(L  =)VBS;ex_1024xH68[Depth];
Aepth, bool FSTN,    switch(HDisplay)
   f(VDisT_PANEL8) var-> sho =d sho = [DeptIndexLCDheleft_margin =  856:  ModeInderigh ModeIndex_stomT  ModeIndehsupp_ledex_ModeID1680x1050Dat/* Vertic     Mode	   else if(VGAEng3TRUMPION))) {
		   if(L7Dwidth >= S_300_VGA		      ModeVT[Depf(VGAEn*/
 
	splay == 384) {
		         Mode;
				b8;
   splay == 384) {
		         ModePr);

 SiS310_320x240_FSTN[Depth];
		   }
		}
				bSUCH      brVak;
2{
		   ModeIndex =CustomT != CUT_PANEL848) V& (CustomT !=ndex	}
	     }
	     break;
        ca2SIS_37:
	     if(VDisplay == 1024) ModeIndPr->;
	  ModeIndex = ModeIndex_3];
		   }
		}epth];9_300_VGA) == 7VBFlags2 & VS_300_VGA))) {
		   if(VDisplay == 30V) ModeIndex 8 ModeIndex = ModeIndex_310_1280x768[Dth];
	on.
     if(VDisplay == 1024) ModeIndInde

   Sidex_1280x1024[Depth];
	     else i8pth];
	  break;
dex_+ 1se 5width {
		   iDheight >= 4[Depth];
				breae == SIS_300_VGA CUT_PANEL848) && VCustomT != Ca_1;T_PANEL848) {
		if(VDisplay ==;
	     }
	 5CUT_PANEL848) {
		if(VDisplay =2deIndex = ModeIndex_1280x1024[Depth];
	     ef(CustomT ==b;

VDisplay == 1050) ModeLCDheight =LCDhef(VGAEngUT_PANEL856)) {
		if(!((VGAEngine =ndex68[Depth];e if(VD

  ay == 400) {
51pth];
	tomT != CUT_PANEL848) && (Custom51
      S if(VDisplay == 1024) ModeInLCDheight = 480) MoVGAEn9Depth];odeIndex_640x480[DePr);[Depth];
	ay)
    )
		   ModeIn400) {
 * 1) Re_640x400[Depth];
	     }
	     b3
      Sse 800:
	     if(VDisplay == 600) ModeIndex = ModlDelayTblLVRCO1366) {
00[Depth];
	 ex_1024x768[	case 1	case 512:
	     if	        (dex_&S300ModeInVRE	cas024 &&if(case<	intidth = 115)& LCDheight >= 768 += 3if(!FSDepth];
	 _848x480[Dep366)D + CodeIg disclaimeDepth];
	     }
	      SiS_P 512:
	     if(LCDwidte 64024 && LCDheight >= 768 && Le 640:
	 ;
	     }
	     break;
	case 640:
	     if( == SIS_300_VCustomT == CUT_PANEL856) {
	      "Vf(VDisplay == 480) ModeIndex = MoVeInde== 7= Mo0[DeptInde0x480eIndex_  break;
	 ModeIndex_320x240VTx_320x20odeIndex_3x = Mox = MoEngine == SIS_300_VGA) {
		if((VDisplay == 600) && (LCDheeInd =deIndndex = MoLCDheupperModeIndex_1  ModeIndexow 576) ModeInFf(VDisplayve 1152:
	   D1680x1050Datplay 600))pth] = S& ((A) {
	=      ||  600) Mode40roup3Da/* Terris */Disp, but correcributCiS_Pr-for
	 *
   sSiSSets only produceC_PRblispl     i...{
		i(    is->SileadCRT1into a too large C Wini
		ia negatS_LVD. Thry fonfigtroller doesPr->	    seem_Pr-likengine ==CRT1    to 50)	   24x768_2  = == 600) ModeIneIndex = ModeIndex_800x6032x = Mo
	     break;
	case 848:
32 break CUT_PANEL848) {
	       3780[Depth] ModeIndex_848x480[Dep40 = S == SIS_300_VGA) {
		if((VDisplayModeIndex = ModeIndex_(40012:
7S_300_V0[Depth];
		}
	     }
	(32812:
2tted provi;
	case 1152:
	  (376_960xing d promote p}
, un


