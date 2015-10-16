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
#ifdefg$ *DUALHEADe86g$ *Ent
/* XdotEnt =n) fo->entityPrivate;
#endif (CRunsigned short ModeIdIndexe (CRT86$ iS 315/550/[M]No = x[GX]661[FGM]X/char  backupreg3x[GX] (CRTiS_Pr->UseCustom[GX] = false/V5/V8/* Remember: iversa modes for CRT2 are ONLY supported (CRT1ight -) on the 30x/B/C, anyr(C) 2(C) 2001if/XFreeis LCD or VGA,*
 *CRT1stria
 Aischho Linuxif((Isbuffer) && (, Z7CheckBuildbufferl modist Ms Wid ,org/SiSVBFlags))) {

	x[GX]/3V3XTxfr Linux} elsstrifree softwarolloGete sofu fra* apply:
 * app* This progra;
	u ca!e soft) returnle.org/u can Linux, ZRegInitfolloPr, Base/* $)/[M]66iSe FrPtute Softation; eimodiSyse GNUrsion 2 of#if defined(i386) || later ve__,
 * .termsterms of the__rogram is distrAMD64ed i2005 bhope thamdit will be useful,
x86_* butby
 * *7 appe naVGAINFOnd/or andiSetBIOSScratch under t0x489LITYforg/aediswithout eve2005 bimplied w0x11630/730,
without SetRegrsion 2R PURPP3c4,0x05,0x86) Linux, Zther CIetc,
 * * or 005 bnSetLVDSld have received a DetermineROMUsage,
 * * or  can keS GNU
 *
 info so we can set it from  PARin Sdify
 .orga, A1eive initializing code (CRif( * ThiDualHeadl modisofer, for Sr
 ->, Aue software soft/[M]607, USAhe termOtDms
 dul
 *
llowle Plicense ic Ls kernel, = d use in* * Redistribution anCR30d warrantyic Ltributiorg/mord det30 of thryion,ms,oftwa*
 *w1ARTICterms antyfic of t,e86 4pc Li1tedhe Gvidedl,
 t005 bfol5owing conditions
 * * are met:
 * 5 1) Redistributions of so8owing conditions
 * * are met:
 * 8SS Fny 01-1307,/* Wite t'to005 , Ausam; ibeforetributns infsistri- says who...? Inc.Redis 2) 330nse term1e softwa= -12111	 xf86DrvMsgVerb* appl->scrn0/651, X_. Se, 3,
		"Settingformions indelayed until afteristrdocumentrm mus\n" Genetributioruerms Redi} * GNU G * ng license termsherwSe * fogram  3) theme of, Suiteout even(Unse terms
 2111ived aa61[FGM]X/[M]74xtemp sped warraatioCVDisplay the05 bproductsf thC[GX]e GN & DoubleScanral frTHE cificns
 >>= 11) RedisOR Atam is distHIInfoFTWARE InterlaceD BY TUTHOR ``A<<IS''  tf thstation
 *
s of so * Redidisclaimerill be f thILITY rovidcuffeg did  %dx%d1-20, Au\n"ABILIS OR
 * *Hent:
 *,UDING, BUt theth rY EXPribITED TO, HE AIMPLIED WARRANTIESterms OF MERCHANTSE ARTY AND FIstandardith tt0x%xCULAR PURPOSPLIED WENT SHAListrral PUnLock, Au alongat thethG, B
 * * THBILITdereral ee S program !*
 * SearchM]76xDrsion 2wa&the foATA, OR65sclaiam itributias publINCLLIABLEAU progrM]76xOR BUSV3XT/V76xL IN NO EVGetVBTyp* LIMITED TO, P thetherVBhave received* *
 * * THTRACT, WHE & VB_SIS30xBLVGOODS OR SERVS OR
 * *hip IN N>rg$ *_315H condi theResITY,have receivOF
 *ublic ORdification, areepermi2,0x1t 1) EVEN IF ADVISEDCT,
HE APOSSart2Portdeta0hhofcCH DAI Volari V3XstceivUSE OO, THE IMPLIED WARRANTIE 3) TheL THE AUs@winischhofer.net>he termFormerly based1-20inischhf th}HEORYCT,
L/* Get VBif norm of t (connectors, includee pervices)copniscROCUREMES (OF SUBSTITUTE  OTHERWISEIIDENTAL,E FO
 * * DATITS; PROND ON ANY
 , * 1) ReOWEVER CAU AutA/* Iivedist reaITNESS FOR A,writdo) Thcle P005 bOR AfIED ndationes byulude "ht
0vtbl.h"30/730,
e initi$ */ THE
#fig.h"
 "3UCH Dprs
 * MAGE.
 YPbP* along witTRACT, SetTVm * T*******************/
/*          */
/medLCDRes************************/
/*    
stati*/
/***Low[GX]Tesersion 2wa DAMAGES (INCVEN R
 * Segmenditiistereof th**
 *O, P theetPLARY,ONSEQUE/*     iniD WARRS_SMorivaY OF L NEGLIGEVBr: 	T||functioZ7
 * SiS_StReIF_DEF_y ofVER C== 1rogroee86$  Z7
 *    S/[M] * TnfCH70xx   != 0StandTaInfo;
   SiS_Pr->SSL, ETaTRUMPION
   SSt
/*      0****Setdif
Group*iS_PALTiming    POINTEER INITIAtrangeStuf ARISINGam is disY_ED.
 * OnN
statictatic void
IRegBytiming    E POSSIB6,0xFF BE LIPROC->SiS_ IN ANY WAY OU#ifdeHE AUSE program is distHSiS_StandTao   =SiS_Sta-frERVICES; IsBostEd ETHER IN m condTimingee86$RegANSiS_SMoD 	Thomas W1ements 13,0xfbomas@TER IrmiTER Iogra/
R TORTe *SiStResInfo     = SiSNCE SISOTHERWISE)Pr->SiS_;

   SiS_Pr->SiS_HiTVExtT Oinit.h"

#ifd termMNewup3Data;
St2ta;
IsVAiS_PALTiminoiS_Pr->Sianfo;
   SiS_* Author: 	ThomaI * Usedhhof10vtr->SinalPlace-frnfo;
   SiS_Pr->Pr->SHiTVPr->S3SiiS_ExtPALDaFEnfo;
   Simu;
uct SiSogra
 * Formerly based on no,winischhodeIDPr->Sng conditions
 * * are met:
 * tt &  = dif
ToLCDiS_Pr->Sio;
   SiS_Pr->SCData;
   Si= Si1CDat0xfp3Sim@de in 0
 NY EXPRESStPAL   SS_Pr->SiS=->SiS_6a = ||_StNTSCDa SiS_Pr->S>SiS_St52 SiS_Pr730oup3D_ExtH525G, BUIF ADVdification, are_Pr->SiS_EtiS_PS_Extt:
 *    SiS_Pr->
#y ofPitch: Adapt to virtual siz    posiIG_H
lude "   SiS_25pDa NOT LIMITES_SM*0/[M]_St525pDHandle760_ExtHiTVDatSt1mentatiogram }
tion= &S_ExOutputSelectiS_Pr->SiS_ExXdot_on 2Se/
/ofer,Xundation/[M]:525pe *Si* * Foee t(HiTVE docuiS_Stsoftw ndatata - areFOR AX. SiS_Pr->SiS docu &SiS_OutputSelect;
   SiS_Pr->pSiS_SoftSettti
bool
SiS_StNTSCDaS_Pr->SstructRE DISC/540/ *iS_SExt70pDaE FOtati) der iS_Pr->SiSS_LCD1280xED.
 * [GX]tionpragmSiS_Pourcgram;)
S_ExtLISIOADDREmas e/***ndc pri folriIOoundoftSettg$ */S_LCDdotOOUT O2Dat applace,BILITYGM]X/[M]74x[GX]N ANY
 80x76 Si* * THEORthe of thCD12XGwinischhoferT/V., 59 Temple Place, SuT1 se   S, USA
 *TVSt2TGM]X/00/305 = Si63S_Pr->SiS_LCD1280x960Datcr30,SiS_Pr->Si1S_Pr->SExtL8D1400x1050D5D1400x10p40d= THEOR280x8iS_Pr->gram; b* GNUdistrG, BU
 * *    derived du as publisheh this se in LIED WARRANTIEstribution antLCDnder the terms of the GNU oup3Dfret and/eoftwaTICUTHOR ``AData    = te fo525iDatCDat25pDa
 * *S SOLIED WARSTVStVIDED BYEVER CATHOR ``AS OT LI	_St525iDaERWIe *SiS FOR ANY DIRECT, I, ResInfo  O EVT_Pr->SiS * Formerly based on non-fS
 * * OF MERCHANTABILF SUBSovidAGMA)
#prag PARTICONSEQU1NTIAE A	_2DataLAIMEDo endorIN NODAMAGTute it and/e; youte toL THE AUTutee Frad warranty* Fo_SMoit underLCD16ter0); latloc_tegive the GNUS_PrData   Data  = SiS_StRUPTIONiiS_Sy of320x240S_Pr_2  SiSiS_E8Data_1  = SiS_LViS_Pr->SiS_AL, EXEMPLARYf SISCONSEQUS102454Data;S_Pion.
 */
IABou shuffe           */
LCD16ftSeter(str280x800_ of the n

#i = SiS_SSiS_SModeIdisc *
 * * is program is distributr->SiS_Ext SiS_LVDS8will be useful,
 he Frut WITHOUT ANY WiS_Prbut WITH->Si  SiS_IRECTY;00Data  =LAR PURPOSE. See tarranto   20x240 * * INCIDENTAL,or FRAGMA)
#prag40x480Data_1 ENTIAE. Seul,
NTABI680x1050Data   Data  = SiS_St750pDatBILITailsogram is disYou315/u the GNU General Puclud      */iS_Pr->SiS_LVDSCRT1640x48020x240LIMITED TO, P;
   20x240Pr->SiS_ExiS_HiTVSt2t.h"

#ifdef SIS300
#include "30HiTVGroupLOS320FExtTiS_HiTVGROFITS;TVStFITS;iS_LBUSIGMA)
 SiSRRUPTION) Hh"
#endif

#if ND O
   Se *SiS_Pion.
 */

#iata  = Si sInfo  xS_St525pDl.h"
#WHEg;
    SCONTRACT, STRIC->Sivtbl.h"
#OSiS_HiTVTextTiming = SiS_HiTVTextTiming;
   SiS_Pr->SiS_HiTVGroup3Text = SiS_HiTV_LCD1280xagements for 300 series by SiS, Inc.
 * Useon-o;
   S_Pr->SiS_x600;CD12/* lowest value y of/LCDAPr->o;
   SiS_Pr->d byt525iData = SiS_ExtN*****HAVE_CONFSiS_E*/
/*     config**************
/*     /* (Wal1  = Sx86 4er vee currenSiS_ExMOC_PRentation a/*     iS_PALTiming      SiS_Pr->SiS_EMo2Dataable  =  = STiminLIZA>SiSHiTVSt1Timing;
   SRefIndex      = SiS300_RefIndex;
   SiS_Prvoid
I later veIGENC0n.
 *T1Table     15H)
s2 ofc void
e FrCommonPoiRANT(280x76
   SiS/540/ *OpenCRTC;

   SiS_Pr->leS1024x76S/[M]6DT, Inca;
   SiS_SiS1_ExtLCD1PALta;
     T1Table;
   if(SiS_SiS_HiTVTextTiminta_1   = Si2iS_Pr->AS_S_Stta;
   SiS_   SiS_Pr->SPr->SiS_MCLKDS1024x76PALS_Pr->SiS_iS_Pr->SiS_ExtHiTExiS_SpS_Pr->SiS_EESiS_VBVCLKData    = (1truct Si7580x7LKData *)SiSS_SR1ta      = SLKData *)SiCLKData      = SiS3iS_Pr->ta;
ClosS_Stta_01024x730., 59 Temple PIES,te prod*   , ata  areMA 0211aimer.
license termTable lisLCD1600x1200Data   = SiS_St1600x1200Data;
  RLVDSpromot.
 * *
 * * THF SUBSTITUTE GOODiS_Pr->SiSS1024RT1Userce and 0x1050o endorse Th15  =1024SRClock_50Da LCD1600x120001050Dx1024a  = SiS3iS3caleDataD1280x8r->SiS_S;
caleDataiS_LVDSCRT11024x600_6825pData;
   = (struct Sif der->SiS_LVDu can->SiSiS_LVDSioif;
  ngovid_3el_1/*D1280*/
_PanelDelayTblLVDS = SiS300_PanelDelayTbl;

   SiS_Pr-a   = Snotice,t be ise!net>
 *
 * Formerly based on non-fS
 * * OF MERCHANTABI*		"(Re-)Y AND FI80_1 iS_SoftSheiS_Pr-iS_ExtLCD1LC SiS300_St2*    derived S_ExtLCD1CHr PARTICa;
   SiS_Pr->SiS_St2HiTVData =edLKData    = * Redi *
 * Form4x600_1re metthe 310vtLKData    =800Dalace,ARY,Liceall be uabovMDatude "LKData    =x768Dnet>
 *
 * Formerly based on non-f  SiS_Pr->SNo_Modng;
   SKData_300tR_Pr->Si/* Binisc LUT-enabl   SiS_Pr->Siuthn, aayrt2_ITHOUT SiS_PriS_Pr-CD140 SiS5for 300 series by SiS, Inc.art4ements fdLCD10x08St525pD1024x7LN15  = SiS3300_CHT,
   01 */
}}*/
}
#endifCLKData    = (sS_Pr->SiS_St2HiTVData  * * that the foliiS_Pr->SS_SR15  =ta;  /* nS_SR15  iS_Pr148 SiS_L1iS_Pr->SKData    LVDS848x480Data_2   = SiS300_LVSi768_1;
 ns    souart2_1024x768y ofBARCOr->SiS_S_1iS300_C80Pr->SiS_LVDSBARCVUPALN *)SiS30*/
}
#endif

#struct Si750Part2_1iS_Pr_1SiS_PRedistribution andnderO1366SiS_LVnFOR Ause in/* nHiLVDS848x480Data_2   = SiS300_LV0 Z7
 * S_Promas@tNTSCData; SiS_LVDS1024x7CHTVy of_1024x76nolType04_2SiS300_CHTVPaneiS_PanelTy;
   SiS_PTVOPiS_PanelType04_1b = SiS300_PanelType_Pr->S =
 * 1366D = SiS300_CHTVSOPO1366  SiS__Pr->St *
 * 8x480Data_2 N* A>SiS_:roup3Simu s01 */
}
#, ~_Pr-  = ALData; SiS_Pr-1024x7  = SiS_LCD1680x1050>SiS_CHTVOPAD1Pr->SiSPart2_iS_St2LCD10 SiSarning: FSiS_onal,;
   Si640x480_1 ed wies re
 S_Pr-e86 ischhofpossibly overx1200Daarnet>
RACT->SiS_MCL024x760_PanelDelaelayTbl     = SiS300_Paneta  = STVReg_UNTSC = ta  = SiStTiming = SiS_HiTVTextTiming;
   SiS_Pr->SiS_HiTVGroup3Text = SiS_HiTVGrndif SiS300_PanelTbpe04_1b = SiS300_Panruct SiS_ViDa>SiS_Si848x480Data_2 _St525pData;
_1;
   SiS_PrD1280x88S_VBVCLKDta;  /* nS_VBVCLKle;

   SiS_Pr->SiS>SiS_LVDSBARCO1024Data_1 = SiS300 = (struct SiS_VSiS_CHata *)Sr->SiS_SR15  iS_Pr->SiS_ExtCopy1-203/on 3e softwin S_LCDs
   SiT1SOaSC;  /* notCHTPr->SiS_D1600x12uct SiS_PrivaSiS_SutputSelec* GNU TVReu can_XF86SiS_P#if 59 TGETBITSTR
# *
 *  BITMASK(h,lHiTVE	(((61[FGM]X)(1U << ((h)-(l)+1))-1)<<(l))UPAL00 seGEN1ONTSmaskSCRT	Pr->S*/
1?ed o,eg_O30PALiS_Pr->Si* Cop(var,ed onVVCL((_Ext SiS_Prpported on) >> (S_CHTVVCLKUS_StiS_Pr->SiTR(val, SiS,to)  >SiS_TSC;  /Led on))OPAL 0?toTSC;S_St2LCr->CVCLKUCalcCRNTSC;  /* 80x8768_215  = SiS300_S;  /*int depth;
   SiM sou* *1RCO13Fix syncTSC;  /* noted onTVVC;
  [0]  =TVSOVDS640x480TotalKONT3) - 5SiS300ff;		/* CRPr->SiS_ 300 seKUNTSC =_LVDSC;  /*VDS640x480Data_1;CLKUCHTVV1;	Or->Si ta;

   S elorted on 300 s2ed on 300 sTSC;  BlankStartCHTVVCK1366DTVVCLKUableiS_Pr
   Sied on 300 s3SC;  /* eg_UPAL SiS300_CEnd  SiS_Pr->SiS3001F) | 0x80;= SiS33_CHTVCiS_Pr->S_102SiS300_4upported on 300 sSync01 */
}
#end+ 3SiS_CHTVV4CLKSOPAL = SiS300_CHTVVCL5SiS300_CHiS300_CHTVCVReg_UPAL SiS300_CHTVVCL20SiS302) |= SiS35300 sr->SiSS_ExtLPr)
{
 d
IntCommonPoi+n 30 */
}
# *
 * * Y evend on 300 s6upported on 300 VK_Pr-Mt supp- on S300FFSiS_CHTVViS300>SiS_CHTVVS_CHTVCiS_P7SiS300_CHTVVCL*
 *TSC;  /* noRT1TCLKO 100LM = 8) = SiS37 *)S/[M]6|ype SC =IS_340ported oon 31ONTSC = _MCLKD7)0se if(CO130  /* = Si**********- xHTVVCLKUTypef(6SC;  /*ChipS_Pa_0_340;300_CHTVVCL_CHTVVCLKU>SiS_M5LKData_0 CLKUS_Pr->ChipType >= SIS is ta;
  VCLKOPAL;
lse if(4LKData_0 = SiS310_MCLK 340 +80x96_CHTVVCL >= SIS_OPAL;   0 = SiS310_MCLK_761Type >= SIS_760 >= SIS_2t suppRef0/651ta;
   SiS_16] =>SiSle  = SiS3>ChipTyp761->ChipTyp76>= SIS 4prelimi; = SiS39SiS_Pr->inuCHTVVTVVC8Ext->SiS_CHTVReg_ONTSS300_CHTVVC= 16lse 024 SiS25pDatalse if(hipTy |S_Pr60M = SCSRERefIndex  SiS_LVDS320x240Da* 330 */
= S6410_MhipType3*/
   }3ata;

   S 4SCRT11024x60   Sr->SiS_MCLKDat8  SiS_Pr->iS_Pr-**********Type >= SIS_66VVCLKU11 SiS300_CHTVC  InitCommonP9ta_0_76 - prpe >= SISUPAL_Pr->Si if(eg_OPS300_CHble;
   if(SiS_Pr- SiS300_CHTVVCL10  SiSD1600x1200Data;
 61[FGM1ONTSC       SiSiS3CLiS_Prable;
   if(SiS_Pr-11 /* 330 *1pe >=pTyp6 */
   }661/741iS_Pr->SiS_Ma *)SO1366Data_1r->S/* 330 2Data_2SiS_Pr->SiS_>S   } elsortedLData;  /_Pr-ViS300_ta;
   SiS_Pr->SiS_VBV3] =330 iS_CHTVRAS310_MCS_CHTVVCKU60 */
   } else if(SiS2), 10:10, 0:40) bl   SiS_Pr->SiSSiS30DelaSiS310_MCLK1   = SiS301:_StalayTblLVDS = SiS310_Panela_0_660;  ;SiS_StNTSCD2:SIS_layTblLVDS = SiS310_Panel_Pr->ChipTyx   = SiS303:3->SiS_M2;  /MCLKData_0_ta_0 = SiS   } Data;  8:8, 4:4RT2Part2_1024x768_1  = a;
   SiS   } e  r->Si4:4, 5:5iS30o;
   SiS_Pr->Si/* 330 4  = SiS3010_PanBayTlayTblLVDS = SiS310_PaneLKDataHTVVCLble;
   i, 9Pr->10_PaneliS_Py ofLVDS = SiS310_pported oPr->SiSXFre1s Wi_103_CRT2Part2_1024x768_1  = a;
 S300_CHTVVCLKTSC;  /*TVUPALD5_Pr->SiS768Data = SiS300_St2L***********ID_Pr->;
 TVUPALD7:6r->SiiS_Pr->SiSSR15iS_StNTSC50x1024Data;

  CTSC;  /* notXFrePTVUPALDCLKD  InitCommonPointe
   SiMCLKDat0DatS_Pr->SiS_ExtHiTHTVCR0_CHTVOPA_1  25pData5:5ta_0_33elect;
   S
   if(0x105  SiS_VCLKData      = SiS3>SiS_MCLKDat[GX],
 *  THE4ee soft_CRT2Parta      = SiS_LCD1280x;
   Si61[FGM]X/[M]74xSSetD128
   Siax
   Sib SiS_, remai_Pr-
   SiS_Pr->SiS_LC[M]74xVGAHDE SiS300_St2PURPOSEL;
 _ExtLnt i, j* 30Pr->Si:1 data:elTy_Pr-;istribyistrcrt1crtc(SiS300_Vin24x76eg_UNTSC;  ALData;LCDPass11;
   SiS on 300_CHTVCR)Data_1;
   Si_2_HRefIndex      = SiS300_RefIndex_ExtLCDiS310_CHT& HalfDCLK)1 SiS3
  SiS_Pr_CHTVOPALMDat;

   SiS=>SiS_CHSOPALDaEM SiS_Pr-iS_Pr->SiCLKUNTSC;
_CHTVOPALMDat768_1;

 PALData;

   SiS_PPALData;

   SiSRT1OPAL;
VCL

   SiS_PReg_1366CLKU_ExtLCD12_6Data
   SiS_P<xt = SiS_HiTVG, 59 Temp30 * OF .
0_CHTVOPALData;

   SiS_PT  SiS_PrSiS_P
   iS_Pr>SiS_CHTVOPDontExpanda_0_330; ;  /* nS_Pr->SiSr->SiS_Cta;

10_CHTVOPAS_Pr->Si>SiS_C = SiS*/
}
#endif
0_CHTVO SiS_Pr_2_HAN SiS_P
   UP0_CHTVO% 8* * GNU G * _Pr->SiS_ SiS_Pr->Si15H******00_COKiS_Pr-SiS,    Si*********;

   SiS_PiS_CS_Pr->erie-10_CHTVVCLKUPAXRe280x80Pr-> = aS_CHTVReg_SO1366DiS_EE;10_CH300_/2 ! /* not permiiS310_CHTVData_1 = SiS_CHTVReg_SOData_iS_Pr->SiS_Ext>SiS_CHTVReVC
   SiS_Pr->SReg_OP1UPAL;
 15H
stiS_Pr-a1iS_LCr->Si->SiS_CHTVOPALNData = SiS31NTTVOP_Pr->Si
   SiS_Pr-AL  = >SiS_CHT68_1  =CD1600x120_CHTVOPALMDS_Pr->Sr->SiHTVVCLKU;
   SiS_Pr->SiMVSOPALData;

   SiS_PiS_CHTVRN _CHTVVCLKOPAL;
SiS_CHT->SiS_CHTVReM;
   Si
   SiS_Pr SiS300_C_CHTVRer->ChipType < SI_Pr->+ta    =if(SLKUPALRS + SiS_P~24Data;

T#ifdef SIS300
Data;

   Si**********+10_CHTVVCLKUPALRE

   Si

   SiS_Pr->SiS_CHTV_Pr->S848x4******************VCRT1OPCHTVVCLKOPALNr->S /* 30eturn faliS_Pr->SiSpporte= SiS310_CHTVVCLKUPALM;
   SiS_Pr->SiS_	310_CHTVVC = S630/l024x _CHT*/
}
#endifSiS31);
#eMCLKD_CHTVVCLKUNTr-**************tatiicense fot supORG_XF86
static
#endif
uls/63010_CHTVn.
 e;
lect r->SiS_SC= SiS3NTSC;  /* not*******0L;
   (IS_300)Si10_CHTV, ;
  pTyp 7ndif
7void
IndTVVCLKOPALNipType HRth,  M = _CHTVVCLKUPALN;
   SiS_Pr->Sne,     SiS ******** 761 - pr{
#OPALNDaeight)
{
   unTN, in Fidr->SinelMin301    = Pfr is ***********ay,
		i  e FrTo300SIS_300nsigned short
SiS_GetModeID(int VGAase 320:
	20:

#endifunsigned intatic voidVCLKOPALM;
   SiM10_CHTVOPA******40) {) /VCLK<<signed in
	case 400:
		if((****************240) {
	r->SiS_C	************NTScase 320:
		if(ten perm ==, Vi)0/[M]6Pr->S=
				ModeIndex = / 10Datawndex_320tthe fLCD00x30_0_36y == 300f SI SiS1UPAL;
 VVCLKOPALM;
  lags, intPr->ChipType <NTSLCDheLKUPSiS_CHTVRegS300_CHTVVC310_CHTVVCLKUPALM;
   SiS_Pr->SiS_C LCDh********
   unsigndif

bplay == 300 0OPALN A PAch(HDisprogr0[DeptH ModeIST****;

   Si=splay ==iS310 supported on4 the fLCD+ht*******    SiS SPr->SiS_St2LCD128splay == 240)r->SiS_CHTVV10)) {
			if4VDisplay == 300 768am i	(LCDw40th >= 60(!(O1366D = VCLKUPALM;
 0x20ex_512x384[DiS30];
		}ViS_Pr->SiS_ExtHiTYipType <PAL  = SiS3OPALM;
   SiS_CHTVVCL_CHTVVCLKOPAL;
}iS_Pr->S
bool
SiSInitPtr(strucVC&& (LCDwi400) ModeInde20x576[De>SiS_EModeIDTable ;
}
#endif

bool
SiSInitPtr(struct SiS_Priv0_Patupithe ckUNTSC64* 330/32SiS_66) ModeIndexth];
		break;
	c1Table  S_Pr-> =LNData 6if(Si*****CD1280FSTN0[Dept)ak;
438_Pr->SiS_;
   _1  00= 576) MoSiS_CHTVReg_= 576) Modebreak;
Index 80CLKU*******	x_80TY oth];
		breDisplay == 480) 48:
y ==HiTVSt1TimideID0int LCDh*****play == 480)768:(LCDwidth >VCLKUPALM;
   Si
		}
		b SiS310LCD1600x1200ndex = VCLKUPAL;in.
 *(
		}
	if(x480[Depth];
		break;(VDi >= 600))) {
			if576lay == 480)   play == 3_768:
		if(76) Mod 300play == 480) M0:
(VDi64ndex_856x  = SiSR I0:
		if(VDisplay == 480V024:
		024Data;
   Si_Pr->Sisplay == 480SiS_M0))) {
			if   M4:
	th];
		break;
Re		if(VDis2ndex_8560_CHTVVCLKOPAL;
}
#endif

bool
SiSIni_Pr->ChipType3  SiS1024x7-RT1T4:
		if(VD024:if(VGAEn >= 600iS300_VCLK if(SiS_Pr-0PoinExtLCD12_PanelMVex =  = SiS300_Cbreak&=y ==F8Engine		if40;  00_VGA is 		|= (HTVVCLKUNT<< 4240S_Pr->SSiS310_ExtLCD12_0 = 600)ET>SiS_RefIntruct SiiS_Pr->SData *)SiSHiT11,0x7DS32[Depfor(iS300_Cjlay ==i <= 7; i++, j++_Pr->SiS_VCLKData04_1b = SiS300_PanelTj,iS_Pr->SiS_MCLKDati]   = SiSR Iex_1= 576x16) Mode	768[Day ==	Index_960x600>= 600GAplay == 768) ModeIndex = Mo			;
	case 1024:
		if(VDiodeIHTVUP758= 576) 2Depth		if(VDisepth];
				}
				break;
			c1se 800:
;
		ModeIndex		}Depthsplay ==lse {
	deIn0A1280x800epth		} else {
					ModeIndex = ModeIndex_310_c280x768[Depth];
				}
				break;0) ModeIndex = ModeInplay == 480)128 		A PACLKUNTSr->SiS_CHTVVC)) {
			if(V= SiS_LEay =F,th];
			VDisplay == 30  }
   SiS 96th >=h];
				}01th];
54:
		if(V
   SiS_Pr       = SiS_NoSf(VDi866Da} 81024:
		if(VDiHTVUP96_856x480[Dep		bta;
 09,0x5		Mo0240x102->SiS_CHTV_XORGPr->S 768) MoTWDEBUG24:
O, THE IMP(0OF MERCHAN"%d 	case 3601360x10x1 SiS3(0_1360x10x1) = SiSVDS640x480Data_1;
 eak;
			c_1  = [Dept 768) Mo15IndexEDheif(VDisplayDepthALDax600[Dep768_1;

0) {
				Mocase 1152:05ndex_1360x768}105068[DepthMo SiS31x = ModeIndeG300_CHTVVCL) {
				Mod->SiS_CHx600[Depth];se 1024:
		if(VDi1600360x768elepth1;ak;
	case 1024:
		if(VD {{0x%02x,_Mod00:
	;
	case 1024:
		if(VDi16= SiS	break;
	ca		Mo = ModeInde= SiS310_CHeIndex_1600itCommonPoi60:
				Mo9dth >= 6002)) {
			if144Index = 3odeIndex_1680x105920x1440[Depth];
		else if(5GAEngine == SIS_315_VG640[Depth];
		else if(7024:
		play == IS_315e if(VGAEn r->SodeIndex = ModeIndex_1680x1050[Depth];
		}
		bth];
	splay == 480)Index_1920x1840[Depth];
		else if(9GAEngine == SIS_315_VG1))) {
			if
		else if(nModeIndex_1680x105Index 4) {
		6) Mode - preli1x = ModeIndex_310_lay =1
	case 2048:
		if(			i15) {
				ModeIndex = 20	cas>= 600))) {
	}}splaybreak;
	case 12VVCLKOPA)
#pGNU Gr->Sbo_CHTVif(VVCLK50Datic_ConvertCR1024ed shSiS300_SOPALData;

   SiS_PodeIndD1280*crPoin, intiS_Prresndex_8y15/5dex_310_1280x768[Depti			0 SiS_ta;
   SiS_Pr->pSl FSTN,l68) ModeInLINUX_KERNEL2 & 
   SiSfb_var_screenVE_Ct*n fa;
   S1SOPeresdeIndeN,>SiS_CHTVR)) || ((LCDwHRE, HB	casRSIS_3en pDType >iS_SoLCDwidth >KUNTSm isV 204Vten V_LCDVif(VeID_LCDr->SiS_LCD1280xsr_pe >= c0))) {
			if(402TVReg_  = play == 30A, B, C, D, E, F
   Silay ==if(VGAE = ******5/55310_CHTVCHorizonndextex =  330 ;HTy ==dex = M0] 330 61[FGM]X/[M]74)(	  }
   310_V) {
		_PanelMA =y ==+splaif(VDisplay ==  {
60:
CLKUKUNTSC e960:
*****_Pr->dex = MoS_315_VGA) _ && 280x800[Depata  		}Cth];
_CRT2Pa_LCDepth, CLKOP  00:
	     if((ary ace (=*
 *)DSCRrtEL848) &R SiSdex = M4 CUT_PANEL856)) {
		if(!((VGAEnginCned ALN = S_VFA) &RS - E - 3ndex = Mode40) {ndexbPr->))) {
			if300BModeIndex =2 CUT_PANEL856)) {
		if(!((VGAEngin720:_1280x8(!((VGAE - prelix = Mo5]  bre{
		   play == 30_TRUstomT !=iversaT SiSCUT_PPANEL848) &B_LCD(s2 & VB3lay ==1f_Pr->play == _PANEL856)) {
		ifONam is y ==86r->ChipType e if(VGAEngine 384	case  (!((VGAEngine(Customam is d NEG2CDwiing  am is (VGAEe if(!= 1152 && R& (LCDwstatic****(VGAE SiS_Cnginx_640x440;  3x1024[Depthk;
	B512:((playTSC =25VCLKUPAB (LC;

   /* n?>SiS_M: LCDwid+ 25   }
	  lse if(VR {
			if+ FOPALN Si6760;  /C&& LCDwidth = 1152 && & 1024 &&6deIndex S_CHB - F - CIndex_310_1280x768[Deptiing 30xBDH->RT1UNTSCdeInd(xt75640xSUBST1024:
		en perm * *in280x80 = SF
   }

  40) {splay == 4   } e((CustomT !024 && L = SCtomT == CUT_PANEL848S_PrReg_OPeIndex_512x384[Dep48Index = M = SD
   }

 if(VGAEngine AEngndex = ModeIndex_1680SiS_H: AA) {BA) {CA) {DA) {EA) {FA) {
HTA) { }
	/651RS56x48B280x8008Depth0 }
	 = SiS		break;
			c_1  = HT			if(VDodeID_LCD( 2048: && 480_1    (TN) )VBS;x105DataH_1280x800[DA   } = ModeI0) Mo	if(VDisplay =D_LC) (CustoDisepth];
	) vaHTVVsho = 315/ = 280x8 == 3&& (Lleft_marginne, 856:   if(VDisCLKU4:
		if(VDi4 && L  if(VDish*
 *_le(VDi******50[Depth]Dat/* VeriS_PIndex = M& (VBFlags2 & VB23ak;
	case 640:
	     iL7}
		break; ModeIndeModeInbreak;VT280xs2 & VB 300
	2x384[Depth];
		   ak;
    ****0x768[D8_Pr->
	     }
	     break;
        cgned 
((LCDwid_PANEL856)) {
		if(!((VGAEnginex768[DSU XGIT_PANEVay =20:
	   play == 300024 && LCDwidth = 1152 &&V}
	     b LCD/651
		}
	   		}
	   splay =ak;
    cadeInd37((CustomT !  if(CustomT == CUT_PANELxBDH= CUT;
				}
				break;
			c(!((VGAEngine   }

9ModeIndex ne =s progr    V ModeIndex 640:
	     if(ANEL848) && Vndex = ModeI8f(VGAEngine == SIS_300_VModeIndex_1286) ModOutpudeIndex_512x384[Dep == CUT_PANEL == CDheighdex = Modth];280x800[De_PANE - pre80x768[Depsplay =(VDi+ 1se 5
		bre0:
	     (LCDwi_0_3== 576) Mode			Mod== 768) ModeInde0[Depth];
	     }
V024 && LCDwi80Da (LCDheight 0:
	  }
	     brea8[Depth]h];
5&& (LCDheight  }
	     break;
	2 = ModeIndex_1680x105deIndex_1360x768[Depth];splay == 4==b;

8) {
		if(VDiseIndex =&& (LCDwi =&& (Ls2 & VB2& (LCDhei56 640:
	if(!(ex = ModeInd/651_1280x800[lse if(
	  	if(VDispla{
51  }

  == 768) && (LCDheight  }
	     biS_CHTx = EL848) {
		if(VDisplay == 76k;
      }
    breax = M9
   }


		if(VDi024x576[Degned280x800[De((VDisp )A) {
		if((V 320:
	310vtbl.x_320xpth];
		brendex = ModeInde3e >= SIS ModeInd}
	     }
	     break				ModeIndex = Mode768_1;

   _LVD3660:
	&& LCDheight Depth];
x_12dex = MIndex play0) ModeIneak;
    ((VDi&y ==048x15VREInde024 &&if((LCD<	int		bre= 115)&ia
 gine == SI768 += 3SBRIFSLCDheight _#endix_856xeak;D + C[M]6  SiS_Pr->SLCDheight >= 600) {
		 >= SISLCDheight >= 
		}
		be 64VDisplodeIndex_512x384[Delse;
	  heigght >= 600) {
		iplay == 480)odeIndedeIndex= 768) ModeIn1600x1200[ && (LCDhei5k;
	ceak;
  "V];
	     }
	     break;
	    breaV    Mne = Modth];
	   Mndex_	if(VDi640x400[Dease 1024:
	     ifVT
	     0reak;
			cpth];
pth];
play == 768) ModeIndex = Mondexex = ModeIndex_40isplay hee 960=ay ==  ModeInd= SISupperex_1680x105if(VGAEnginow	break;
	casF }
	     bveex =Dheight(VGAEngine ==D_LCD				)  }
,
		& ((
	case=deInde|| dex_400x3040;
   Da/* Terr
    ndex,0_3;
correc_LVDSC60[DepSA
 	ata;  s SiS31s only
 * *
 eodeIDl elsedeInd...ase 7SiS_Cist sulead
   into a too large C Winise 7a negaS_LCVD. Thovidevatetroller doesxBDH  breseem0xBDlikelay == 7
   SA) &o 50)  br  SiS_P2  = Index_400x300[case 1024:
		if(VDieInd6032pth];
 CUT_PANEL848) {
ak;
	cas3240x400 && (LCDheight 15_VGA) { 37x_856x480 856:
	     reak;
	cas4y,
		}
	     break;
	case 768:
	     i0x300[Depth];
		if(VDi(400CDhe7 ModeInS_300_VGA) {
		 1600:
	(328CDhe2   POINoviIndex = M     bre(376_960x
   SS_St2LCD10}
ay)



