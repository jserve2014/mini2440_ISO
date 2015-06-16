/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Kevin E. Martin <martin@xfree86.org>
 *   Rickard E. Faith <faith@valinux.com>
 *   Alan Hourihane <alanh@fairlite.demon.co.uk>
 *
 * References:
 *
 * !!!! FIXME !!!!
 *   RAGE 128 VR/ RAGE 128 GL Register Reference Manual (Technical
 *   Reference Manual P/N RRG-G04100-C Rev. 0.04), ATI Technologies: April
 *   1999.
 *
 * !!!! FIXME !!!!
 *   RAGE 128 Software Development Manual (Technical Reference Manual P/N
 *   SDK-G04000 Rev. 0.01), ATI Technologies: June 1999.
 *
 */

/* !!!! FIXME !!!!  NOTE: THIS FILE HAS BEEN CONVERTED FROM r128_reg.h
 * AND CONTAINS REGISTERS AND REGISTER DEFINITIONS THAT ARE NOT CORRECT
 * ON THE RADEON.  A FULL AUDIT OF THIS CODE IS NEEDED!  */
#ifndef _RADEON_REG_H_
#define _RADEON_REG_H_

#include "r300_reg.h"
#include "r500_reg.h"
#include "r600_reg.h"


#define RADEON_MC_AGP_LOCATION		0x014c
#define		RADEON_MC_AGP_START_MASK	0x0000FFFF
#define		RADEON_MC_AGP_START_SHIFT	0
#define		RADEON_MC_AGP_TOP_MASK		0xFFFF0000
#define		RADEON_MC_AGP_TOP_SHIFT		16
#define RADEON_MC_FB_LOCATION		0x0148
#define		RADEON_MC_FB_START_MASK		0x0000FFFF
#define		RADEON_MC_FB_START_SHIFT	0
#define		RADEON_MC_FB_TOP_MASK		0xFFFF0000
#define		RADEON_MC_FB_TOP_SHIFT		16
#define RADEON_AGP_BASE_2		0x015c /* r200+ only */
#define RADEON_AGP_BASE			0x0170

#define ATI_DATATYPE_VQ				0
#define ATI_DATATYPE_CI4			1
#define ATI_DATATYPE_CI8			2
#define ATI_DATATYPE_ARGB1555			3
#define ATI_DATATYPE_RGB565			4
#define ATI_DATATYPE_RGB888			5
#define ATI_DATATYPE_ARGB8888			6
#define ATI_DATATYPE_RGB332			7
#define ATI_DATATYPE_Y8				8
#define ATI_DATATYPE_RGB8			9
#define ATI_DATATYPE_CI16			10
#define ATI_DATATYPE_VYUY_422			11
#define ATI_DATATYPE_YVYU_422			12
#define ATI_DATATYPE_AYUV_444			14
#define ATI_DATATYPE_ARGB4444			15

				/* Registers for 2D/Video/Overlay */
#define RADEON_ADAPTER_ID                   0x0f2c /* PCI */
#define RADEON_AGP_BASE                     0x0170
#define RADEON_AGP_CNTL                     0x0174
#       define RADEON_AGP_APER_SIZE_256MB   (0x00 << 0)
#       define RADEON_AGP_APER_SIZE_128MB   (0x20 << 0)
#       define RADEON_AGP_APER_SIZE_64MB    (0x30 << 0)
#       define RADEON_AGP_APER_SIZE_32MB    (0x38 << 0)
#       define RADEON_AGP_APER_SIZE_16MB    (0x3c << 0)
#       define RADEON_AGP_APER_SIZE_8MB     (0x3e << 0)
#       define RADEON_AGP_APER_SIZE_4MB     (0x3f << 0)
#       define RADEON_AGP_APER_SIZE_MASK    (0x3f << 0)
#define RADEON_STATUS_PCI_CONFIG            0x06
#       define RADEON_CAP_LIST              0x100000
#define RADEON_CAPABILITIES_PTR_PCI_CONFIG  0x34 /* offset in PCI config*/
#       define RADEON_CAP_PTR_MASK          0xfc /* mask off reserved bits of CAP_PTR */
#       define RADEON_CAP_ID_NULL           0x00 /* End of capability list */
#       define RADEON_CAP_ID_AGP            0x02 /* AGP capability ID */
#       define RADEON_CAP_ID_EXP            0x10 /* PCI Express */
#define RADEON_AGP_COMMAND                  0x0f60 /* PCI */
#define RADEON_AGP_COMMAND_PCI_CONFIG       0x0060 /* offset in PCI config*/
#       define RADEON_AGP_ENABLE            (1<<8)
#define RADEON_AGP_PLL_CNTL                 0x000b /* PLL */
#define RADEON_AGP_STATUS                   0x0f5c /* PCI */
#       define RADEON_AGP_1X_MODE           0x01
#       define RADEON_AGP_2X_MODE           0x02
#       define RADEON_AGP_4X_MODE           0x04
#       define RADEON_AGP_FW_MODE           0x10
#       define RADEON_AGP_MODE_MASK         0x17
#       define RADEON_AGPv3_MODE            0x08
#       define RADEON_AGPv3_4X_MODE         0x01
#       define RADEON_AGPv3_8X_MODE         0x02
#define RADEON_ATTRDR                       0x03c1 /* VGA */
#define RADEON_ATTRDW                       0x03c0 /* VGA */
#define RADEON_ATTRX                        0x03c0 /* VGA */
#define RADEON_AUX_SC_CNTL                  0x1660
#       define RADEON_AUX1_SC_EN            (1 << 0)
#       define RADEON_AUX1_SC_MODE_OR       (0 << 1)
#       define RADEON_AUX1_SC_MODE_NAND     (1 << 1)
#       define RADEON_AUX2_SC_EN            (1 << 2)
#       define RADEON_AUX2_SC_MODE_OR       (0 << 3)
#       define RADEON_AUX2_SC_MODE_NAND     (1 << 3)
#       define RADEON_AUX3_SC_EN            (1 << 4)
#       define RADEON_AUX3_SC_MODE_OR       (0 << 5)
#       define RADEON_AUX3_SC_MODE_NAND     (1 << 5)
#define RADEON_AUX1_SC_BOTTOM               0x1670
#define RADEON_AUX1_SC_LEFT                 0x1664
#define RADEON_AUX1_SC_RIGHT                0x1668
#define RADEON_AUX1_SC_TOP                  0x166c
#define RADEON_AUX2_SC_BOTTOM               0x1680
#define RADEON_AUX2_SC_LEFT                 0x1674
#define RADEON_AUX2_SC_RIGHT                0x1678
#define RADEON_AUX2_SC_TOP                  0x167c
#define RADEON_AUX3_SC_BOTTOM               0x1690
#define RADEON_AUX3_SC_LEFT                 0x1684
#define RADEON_AUX3_SC_RIGHT                0x1688
#define RADEON_AUX3_SC_TOP                  0x168c
#define RADEON_AUX_WINDOW_HORZ_CNTL         0x02d8
#define RADEON_AUX_WINDOW_VERT_CNTL         0x02dc

#define RADEON_BASE_CODE                    0x0f0b
#define RADEON_BIOS_0_SCRATCH               0x0010
#       define RADEON_FP_PANEL_SCALABLE     (1 << 16)
#       define RADEON_FP_PANEL_SCALE_EN     (1 << 17)
#       define RADEON_FP_CHIP_SCALE_EN      (1 << 18)
#       define RADEON_DRIVER_BRIGHTNESS_EN  (1 << 26)
#       define RADEON_DISPLAY_ROT_MASK      (3 << 28)
#       define RADEON_DISPLAY_ROT_00        (0 << 28)
#       define RADEON_DISPLAY_ROT_90        (1 << 28)
#       define RADEON_DISPLAY_ROT_180       (2 << 28)
#       define RADEON_DISPLAY_ROT_270       (3 << 28)
#define RADEON_BIOS_1_SCRATCH               0x0014
#define RADEON_BIOS_2_SCRATCH               0x0018
#define RADEON_BIOS_3_SCRATCH               0x001c
#define RADEON_BIOS_4_SCRATCH               0x0020
#       define RADEON_CRT1_ATTACHED_MASK    (3 << 0)
#       define RADEON_CRT1_ATTACHED_MONO    (1 << 0)
#       define RADEON_CRT1_ATTACHED_COLOR   (2 << 0)
#       define RADEON_LCD1_ATTACHED         (1 << 2)
#       define RADEON_DFP1_ATTACHED         (1 << 3)
#       define RADEON_TV1_ATTACHED_MASK     (3 << 4)
#       define RADEON_TV1_ATTACHED_COMP     (1 << 4)
#       define RADEON_TV1_ATTACHED_SVIDEO   (2 << 4)
#       define RADEON_CRT2_ATTACHED_MASK    (3 << 8)
#       define RADEON_CRT2_ATTACHED_MONO    (1 << 8)
#       define RADEON_CRT2_ATTACHED_COLOR   (2 << 8)
#       define RADEON_DFP2_ATTACHED         (1 << 11)
#define RADEON_BIOS_5_SCRATCH               0x0024
#       define RADEON_LCD1_ON               (1 << 0)
#       define RADEON_CRT1_ON               (1 << 1)
#       define RADEON_TV1_ON                (1 << 2)
#       define RADEON_DFP1_ON               (1 << 3)
#       define RADEON_CRT2_ON               (1 << 5)
#       define RADEON_CV1_ON                (1 << 6)
#       define RADEON_DFP2_ON               (1 << 7)
#       define RADEON_LCD1_CRTC_MASK        (1 << 8)
#       define RADEON_LCD1_CRTC_SHIFT       8
#       define RADEON_CRT1_CRTC_MASK        (1 << 9)
#       define RADEON_CRT1_CRTC_SHIFT       9
#       define RADEON_TV1_CRTC_MASK         (1 << 10)
#       define RADEON_TV1_CRTC_SHIFT        10
#       define RADEON_DFP1_CRTC_MASK        (1 << 11)
#       define RADEON_DFP1_CRTC_SHIFT       11
#       define RADEON_CRT2_CRTC_MASK        (1 << 12)
#       define RADEON_CRT2_CRTC_SHIFT       12
#       define RADEON_CV1_CRTC_MASK         (1 << 13)
#       define RADEON_CV1_CRTC_SHIFT        13
#       define RADEON_DFP2_CRTC_MASK        (1 << 14)
#       define RADEON_DFP2_CRTC_SHIFT       14
#       define RADEON_ACC_REQ_LCD1          (1 << 16)
#       define RADEON_ACC_REQ_CRT1          (1 << 17)
#       define RADEON_ACC_REQ_TV1           (1 << 18)
#       define RADEON_ACC_REQ_DFP1          (1 << 19)
#       define RADEON_ACC_REQ_CRT2          (1 << 21)
#       define RADEON_ACC_REQ_TV2           (1 << 22)
#       define RADEON_ACC_REQ_DFP2          (1 << 23)
#define RADEON_BIOS_6_SCRATCH               0x0028
#       define RADEON_ACC_MODE_CHANGE       (1 << 2)
#       define RADEON_EXT_DESKTOP_MODE      (1 << 3)
#       define RADEON_LCD_DPMS_ON           (1 << 20)
#       define RADEON_CRT_DPMS_ON           (1 << 21)
#       define RADEON_TV_DPMS_ON            (1 << 22)
#       define RADEON_DFP_DPMS_ON           (1 << 23)
#       define RADEON_DPMS_MASK             (3 << 24)
#       define RADEON_DPMS_ON               (0 << 24)
#       define RADEON_DPMS_STANDBY          (1 << 24)
#       define RADEON_DPMS_SUSPEND          (2 << 24)
#       define RADEON_DPMS_OFF              (3 << 24)
#       define RADEON_SCREEN_BLANKING       (1 << 26)
#       define RADEON_DRIVER_CRITICAL       (1 << 27)
#       define RADEON_DISPLAY_SWITCHING_DIS (1 << 30)
#define RADEON_BIOS_7_SCRATCH               0x002c
#       define RADEON_SYS_HOTKEY            (1 << 10)
#       define RADEON_DRV_LOADED            (1 << 12)
#define RADEON_BIOS_ROM                     0x0f30 /* PCI */
#define RADEON_BIST                         0x0f0f /* PCI */
#define RADEON_BRUSH_DATA0                  0x1480
#define RADEON_BRUSH_DATA1                  0x1484
#define RADEON_BRUSH_DATA10                 0x14a8
#define RADEON_BRUSH_DATA11                 0x14ac
#define RADEON_BRUSH_DATA12                 0x14b0
#define RADEON_BRUSH_DATA13                 0x14b4
#define RADEON_BRUSH_DATA14                 0x14b8
#define RADEON_BRUSH_DATA15                 0x14bc
#define RADEON_BRUSH_DATA16                 0x14c0
#define RADEON_BRUSH_DATA17                 0x14c4
#define RADEON_BRUSH_DATA18                 0x14c8
#define RADEON_BRUSH_DATA19                 0x14cc
#define RADEON_BRUSH_DATA2                  0x1488
#define RADEON_BRUSH_DATA20                 0x14d0
#define RADEON_BRUSH_DATA21                 0x14d4
#define RADEON_BRUSH_DATA22                 0x14d8
#define RADEON_BRUSH_DATA23                 0x14dc
#define RADEON_BRUSH_DATA24                 0x14e0
#define RADEON_BRUSH_DATA25                 0x14e4
#define RADEON_BRUSH_DATA26                 0x14e8
#define RADEON_BRUSH_DATA27                 0x14ec
#define RADEON_BRUSH_DATA28                 0x14f0
#define RADEON_BRUSH_DATA29                 0x14f4
#define RADEON_BRUSH_DATA3                  0x148c
#define RADEON_BRUSH_DATA30                 0x14f8
#define RADEON_BRUSH_DATA31                 0x14fc
#define RADEON_BRUSH_DATA32                 0x1500
#define RADEON_BRUSH_DATA33                 0x1504
#define RADEON_BRUSH_DATA34                 0x1508
#define RADEON_BRUSH_DATA35                 0x150c
#define RADEON_BRUSH_DATA36                 0x1510
#define RADEON_BRUSH_DATA37                 0x1514
#define RADEON_BRUSH_DATA38                 0x1518
#define RADEON_BRUSH_DATA39                 0x151c
#define RADEON_BRUSH_DATA4                  0x1490
#define RADEON_BRUSH_DATA40                 0x1520
#define RADEON_BRUSH_DATA41                 0x1524
#define RADEON_BRUSH_DATA42                 0x1528
#define RADEON_BRUSH_DATA43                 0x152c
#define RADEON_BRUSH_DATA44                 0x1530
#define RADEON_BRUSH_DATA45                 0x1534
#define RADEON_BRUSH_DATA46                 0x1538
#define RADEON_BRUSH_DATA47                 0x153c
#define RADEON_BRUSH_DATA48                 0x1540
#define RADEON_BRUSH_DATA49                 0x1544
#define RADEON_BRUSH_DATA5                  0x1494
#define RADEON_BRUSH_DATA50                 0x1548
#define RADEON_BRUSH_DATA51                 0x154c
#define RADEON_BRUSH_DATA52                 0x1550
#define RADEON_BRUSH_DATA53                 0x1554
#define RADEON_BRUSH_DATA54                 0x1558
#define RADEON_BRUSH_DATA55                 0x155c
#define RADEON_BRUSH_DATA56                 0x1560
#define RADEON_BRUSH_DATA57                 0x1564
#define RADEON_BRUSH_DATA58                 0x1568
#define RADEON_BRUSH_DATA59                 0x156c
#define RADEON_BRUSH_DATA6                  0x1498
#define RADEON_BRUSH_DATA60                 0x1570
#define RADEON_BRUSH_DATA61                 0x1574
#define RADEON_BRUSH_DATA62                 0x1578
#define RADEON_BRUSH_DATA63                 0x157c
#define RADEON_BRUSH_DATA7                  0x149c
#define RADEON_BRUSH_DATA8                  0x14a0
#define RADEON_BRUSH_DATA9                  0x14a4
#define RADEON_BRUSH_SCALE                  0x1470
#define RADEON_BRUSH_Y_X                    0x1474
#define RADEON_BUS_CNTL                     0x0030
#       define RADEON_BUS_MASTER_DIS         (1 << 6)
#       define RADEON_BUS_BIOS_DIS_ROM       (1 << 12)
#	define RS600_BUS_MASTER_DIS	     (1 << 14)
#	define RS600_MSI_REARM		     (1 << 20) /* rs600/rs690/rs740 */
#       define RADEON_BUS_RD_DISCARD_EN      (1 << 24)
#       define RADEON_BUS_RD_ABORT_EN        (1 << 25)
#       define RADEON_BUS_MSTR_DISCONNECT_EN (1 << 28)
#       define RADEON_BUS_WRT_BURST          (1 << 29)
#       define RADEON_BUS_READ_BURST         (1 << 30)
#define RADEON_BUS_CNTL1                    0x0034
#       define RADEON_BUS_WAIT_ON_LOCK_EN    (1 << 4)
/* rv370/rv380, rv410, r423/r430/r480, r5xx */
#define RADEON_MSI_REARM_EN		    0x0160
#	define RV370_MSI_REARM_EN	     (1 << 0)

/* #define RADEON_PCIE_INDEX                   0x0030 */
/* #define RADEON_PCIE_DATA                    0x0034 */
#define RADEON_PCIE_LC_LINK_WIDTH_CNTL             0xa2 /* PCIE */
#       define RADEON_PCIE_LC_LINK_WIDTH_SHIFT     0
#       define RADEON_PCIE_LC_LINK_WIDTH_MASK      0x7
#       define RADEON_PCIE_LC_LINK_WIDTH_X0        0
#       define RADEON_PCIE_LC_LINK_WIDTH_X1        1
#       define RADEON_PCIE_LC_LINK_WIDTH_X2        2
#       define RADEON_PCIE_LC_LINK_WIDTH_X4        3
#       define RADEON_PCIE_LC_LINK_WIDTH_X8        4
#       define RADEON_PCIE_LC_LINK_WIDTH_X12       5
#       define RADEON_PCIE_LC_LINK_WIDTH_X16       6
#       define RADEON_PCIE_LC_LINK_WIDTH_RD_SHIFT  4
#       define RADEON_PCIE_LC_LINK_WIDTH_RD_MASK   0x70
#       define RADEON_PCIE_LC_RECONFIG_NOW         (1 << 8)
#       define RADEON_PCIE_LC_RECONFIG_LATER       (1 << 9)
#       define RADEON_PCIE_LC_SHORT_RECONFIG_EN    (1 << 10)

#define RADEON_CACHE_CNTL                   0x1724
#define RADEON_CACHE_LINE                   0x0f0c /* PCI */
#define RADEON_CAPABILITIES_ID              0x0f50 /* PCI */
#define RADEON_CAPABILITIES_PTR             0x0f34 /* PCI */
#define RADEON_CLK_PIN_CNTL                 0x0001 /* PLL */
#       define RADEON_DONT_USE_XTALIN       (1 << 4)
#       define RADEON_SCLK_DYN_START_CNTL   (1 << 15)
#define RADEON_CLOCK_CNTL_DATA              0x000c
#define RADEON_CLOCK_CNTL_INDEX             0x0008
#       define RADEON_PLL_WR_EN             (1 << 7)
#       define RADEON_PLL_DIV_SEL           (3 << 8)
#       define RADEON_PLL2_DIV_SEL_MASK     (~(3 << 8))
#define RADEON_CLK_PWRMGT_CNTL              0x0014
#       define RADEON_ENGIN_DYNCLK_MODE     (1 << 12)
#       define RADEON_ACTIVE_HILO_LAT_MASK  (3 << 13)
#       define RADEON_ACTIVE_HILO_LAT_SHIFT 13
#       define RADEON_DISP_DYN_STOP_LAT_MASK (1 << 12)
#       define RADEON_MC_BUSY               (1 << 16)
#       define RADEON_DLL_READY             (1 << 19)
#       define RADEON_CG_NO1_DEBUG_0        (1 << 24)
#       define RADEON_CG_NO1_DEBUG_MASK     (0x1f << 24)
#       define RADEON_DYN_STOP_MODE_MASK    (7 << 21)
#       define RADEON_TVPLL_PWRMGT_OFF      (1 << 30)
#       define RADEON_TVCLK_TURNOFF         (1 << 31)
#define RADEON_PLL_PWRMGT_CNTL              0x0015 /* PLL */
#       define RADEON_TCL_BYPASS_DISABLE    (1 << 20)
#define RADEON_CLR_CMP_CLR_3D               0x1a24
#define RADEON_CLR_CMP_CLR_DST              0x15c8
#define RADEON_CLR_CMP_CLR_SRC              0x15c4
#define RADEON_CLR_CMP_CNTL                 0x15c0
#       define RADEON_SRC_CMP_EQ_COLOR      (4 <<  0)
#       define RADEON_SRC_CMP_NEQ_COLOR     (5 <<  0)
#       define RADEON_CLR_CMP_SRC_SOURCE    (1 << 24)
#define RADEON_CLR_CMP_MASK                 0x15cc
#       define RADEON_CLR_CMP_MSK           0xffffffff
#define RADEON_CLR_CMP_MASK_3D              0x1A28
#define RADEON_COMMAND                      0x0f04 /* PCI */
#define RADEON_COMPOSITE_SHADOW_ID          0x1a0c
#define RADEON_CONFIG_APER_0_BASE           0x0100
#define RADEON_CONFIG_APER_1_BASE           0x0104
#define RADEON_CONFIG_APER_SIZE             0x0108
#define RADEON_CONFIG_BONDS                 0x00e8
#define RADEON_CONFIG_CNTL                  0x00e0
#       define RADEON_CFG_ATI_REV_A11       (0   << 16)
#       define RADEON_CFG_ATI_REV_A12       (1   << 16)
#       define RADEON_CFG_ATI_REV_A13       (2   << 16)
#       define RADEON_CFG_ATI_REV_ID_MASK   (0xf << 16)
#define RADEON_CONFIG_MEMSIZE               0x00f8
#define RADEON_CONFIG_MEMSIZE_EMBEDDED      0x0114
#define RADEON_CONFIG_REG_1_BASE            0x010c
#define RADEON_CONFIG_REG_APER_SIZE         0x0110
#define RADEON_CONFIG_XSTRAP                0x00e4
#define RADEON_CONSTANT_COLOR_C             0x1d34
#       define RADEON_CONSTANT_COLOR_MASK   0x00ffffff
#       define RADEON_CONSTANT_COLOR_ONE    0x00ffffff
#       define RADEON_CONSTANT_COLOR_ZERO   0x00000000
#define RADEON_CRC_CMDFIFO_ADDR             0x0740
#define RADEON_CRC_CMDFIFO_DOUT             0x0744
#define RADEON_GRPH_BUFFER_CNTL             0x02f0
#       define RADEON_GRPH_START_REQ_MASK          (0x7f)
#       define RADEON_GRPH_START_REQ_SHIFT         0
#       define RADEON_GRPH_STOP_REQ_MASK           (0x7f<<8)
#       define RADEON_GRPH_STOP_REQ_SHIFT          8
#       define RADEON_GRPH_CRITICAL_POINT_MASK     (0x7f<<16)
#       define RADEON_GRPH_CRITICAL_POINT_SHIFT    16
#       define RADEON_GRPH_CRITICAL_CNTL           (1<<28)
#       define RADEON_GRPH_BUFFER_SIZE             (1<<29)
#       define RADEON_GRPH_CRITICAL_AT_SOF         (1<<30)
#       define RADEON_GRPH_STOP_CNTL               (1<<31)
#define RADEON_GRPH2_BUFFER_CNTL            0x03f0
#       define RADEON_GRPH2_START_REQ_MASK         (0x7f)
#       define RADEON_GRPH2_START_REQ_SHIFT         0
#       define RADEON_GRPH2_STOP_REQ_MASK          (0x7f<<8)
#       define RADEON_GRPH2_STOP_REQ_SHIFT         8
#       define RADEON_GRPH2_CRITICAL_POINT_MASK    (0x7f<<16)
#       define RADEON_GRPH2_CRITICAL_POINT_SHIFT   16
#       define RADEON_GRPH2_CRITICAL_CNTL          (1<<28)
#       define RADEON_GRPH2_BUFFER_SIZE            (1<<29)
#       define RADEON_GRPH2_CRITICAL_AT_SOF        (1<<30)
#       define RADEON_GRPH2_STOP_CNTL              (1<<31)
#define RADEON_CRTC_CRNT_FRAME              0x0214
#define RADEON_CRTC_EXT_CNTL                0x0054
#       define RADEON_CRTC_VGA_XOVERSCAN    (1 <<  0)
#       define RADEON_VGA_ATI_LINEAR        (1 <<  3)
#       define RADEON_XCRT_CNT_EN           (1 <<  6)
#       define RADEON_CRTC_HSYNC_DIS        (1 <<  8)
#       define RADEON_CRTC_VSYNC_DIS        (1 <<  9)
#       define RADEON_CRTC_DISPLAY_DIS      (1 << 10)
#       define RADEON_CRTC_SYNC_TRISTAT     (1 << 11)
#       define RADEON_CRTC_CRT_ON           (1 << 15)
#define RADEON_CRTC_EXT_CNTL_DPMS_BYTE      0x0055
#       define RADEON_CRTC_HSYNC_DIS_BYTE   (1 <<  0)
#       define RADEON_CRTC_VSYNC_DIS_BYTE   (1 <<  1)
#       define RADEON_CRTC_DISPLAY_DIS_BYTE (1 <<  2)
#define RADEON_CRTC_GEN_CNTL                0x0050
#       define RADEON_CRTC_DBL_SCAN_EN      (1 <<  0)
#       define RADEON_CRTC_INTERLACE_EN     (1 <<  1)
#       define RADEON_CRTC_CSYNC_EN         (1 <<  4)
#       define RADEON_CRTC_ICON_EN          (1 << 15)
#       define RADEON_CRTC_CUR_EN           (1 << 16)
#       define RADEON_CRTC_CUR_MODE_MASK    (7 << 20)
#       define RADEON_CRTC_CUR_MODE_SHIFT   20
#       define RADEON_CRTC_CUR_MODE_MONO    0
#       define RADEON_CRTC_CUR_MODE_24BPP   2
#       define RADEON_CRTC_EXT_DISP_EN      (1 << 24)
#       define RADEON_CRTC_EN               (1 << 25)
#       define RADEON_CRTC_DISP_REQ_EN_B    (1 << 26)
#define RADEON_CRTC2_GEN_CNTL               0x03f8
#       define RADEON_CRTC2_DBL_SCAN_EN     (1 <<  0)
#       define RADEON_CRTC2_INTERLACE_EN    (1 <<  1)
#       define RADEON_CRTC2_SYNC_TRISTAT    (1 <<  4)
#       define RADEON_CRTC2_HSYNC_TRISTAT   (1 <<  5)
#       define RADEON_CRTC2_VSYNC_TRISTAT   (1 <<  6)
#       define RADEON_CRTC2_CRT2_ON         (1 <<  7)
#       define RADEON_CRTC2_PIX_WIDTH_SHIFT 8
#       define RADEON_CRTC2_PIX_WIDTH_MASK  (0xf << 8)
#       define RADEON_CRTC2_ICON_EN         (1 << 15)
#       define RADEON_CRTC2_CUR_EN          (1 << 16)
#       define RADEON_CRTC2_CUR_MODE_MASK   (7 << 20)
#       define RADEON_CRTC2_DISP_DIS        (1 << 23)
#       define RADEON_CRTC2_EN              (1 << 25)
#       define RADEON_CRTC2_DISP_REQ_EN_B   (1 << 26)
#       define RADEON_CRTC2_CSYNC_EN        (1 << 27)
#       define RADEON_CRTC2_HSYNC_DIS       (1 << 28)
#       define RADEON_CRTC2_VSYNC_DIS       (1 << 29)
#define RADEON_CRTC_MORE_CNTL               0x27c
#       define RADEON_CRTC_AUTO_HORZ_CENTER_EN (1<<2)
#       define RADEON_CRTC_AUTO_VERT_CENTER_EN (1<<3)
#       define RADEON_CRTC_H_CUTOFF_ACTIVE_EN (1<<4)
#       define RADEON_CRTC_V_CUTOFF_ACTIVE_EN (1<<5)
#define RADEON_CRTC_GUI_TRIG_VLINE          0x0218
#define RADEON_CRTC_H_SYNC_STRT_WID         0x0204
#       define RADEON_CRTC_H_SYNC_STRT_PIX        (0x07  <<  0)
#       define RADEON_CRTC_H_SYNC_STRT_CHAR       (0x3ff <<  3)
#       define RADEON_CRTC_H_SYNC_STRT_CHAR_SHIFT 3
#       define RADEON_CRTC_H_SYNC_WID             (0x3f  << 16)
#       define RADEON_CRTC_H_SYNC_WID_SHIFT       16
#       define RADEON_CRTC_H_SYNC_POL             (1     << 23)
#define RADEON_CRTC2_H_SYNC_STRT_WID        0x0304
#       define RADEON_CRTC2_H_SYNC_STRT_PIX        (0x07  <<  0)
#       define RADEON_CRTC2_H_SYNC_STRT_CHAR       (0x3ff <<  3)
#       define RADEON_CRTC2_H_SYNC_STRT_CHAR_SHIFT 3
#       define RADEON_CRTC2_H_SYNC_WID             (0x3f  << 16)
#       define RADEON_CRTC2_H_SYNC_WID_SHIFT       16
#       define RADEON_CRTC2_H_SYNC_POL             (1     << 23)
#define RADEON_CRTC_H_TOTAL_DISP            0x0200
#       define RADEON_CRTC_H_TOTAL          (0x03ff << 0)
#       define RADEON_CRTC_H_TOTAL_SHIFT    0
#       define RADEON_CRTC_H_DISP           (0x01ff << 16)
#       define RADEON_CRTC_H_DISP_SHIFT     16
#define RADEON_CRTC2_H_TOTAL_DISP           0x0300
#       define RADEON_CRTC2_H_TOTAL         (0x03ff << 0)
#       define RADEON_CRTC2_H_TOTAL_SHIFT   0
#       define RADEON_CRTC2_H_DISP          (0x01ff << 16)
#       define RADEON_CRTC2_H_DISP_SHIFT    16

#define RADEON_CRTC_OFFSET_RIGHT	    0x0220
#define RADEON_CRTC_OFFSET                  0x0224
#	define RADEON_CRTC_OFFSET__GUI_TRIG_OFFSET (1<<30)
#	define RADEON_CRTC_OFFSET__OFFSET_LOCK	   (1<<31)

#define RADEON_CRTC2_OFFSET                 0x0324
#	define RADEON_CRTC2_OFFSET__GUI_TRIG_OFFSET (1<<30)
#	define RADEON_CRTC2_OFFSET__OFFSET_LOCK	    (1<<31)
#define RADEON_CRTC_OFFSET_CNTL             0x0228
#       define RADEON_CRTC_TILE_LINE_SHIFT              0
#       define RADEON_CRTC_TILE_LINE_RIGHT_SHIFT        4
#	define R300_CRTC_X_Y_MODE_EN_RIGHT		(1 << 6)
#	define R300_CRTC_MICRO_TILE_BUFFER_RIGHT_MASK   (3 << 7)
#	define R300_CRTC_MICRO_TILE_BUFFER_RIGHT_AUTO   (0 << 7)
#	define R300_CRTC_MICRO_TILE_BUFFER_RIGHT_SINGLE (1 << 7)
#	define R300_CRTC_MICRO_TILE_BUFFER_RIGHT_DOUBLE (2 << 7)
#	define R300_CRTC_MICRO_TILE_BUFFER_RIGHT_DIS    (3 << 7)
#	define R300_CRTC_X_Y_MODE_EN			(1 << 9)
#	define R300_CRTC_MICRO_TILE_BUFFER_MASK		(3 << 10)
#	define R300_CRTC_MICRO_TILE_BUFFER_AUTO		(0 << 10)
#	define R300_CRTC_MICRO_TILE_BUFFER_SINGLE	(1 << 10)
#	define R300_CRTC_MICRO_TILE_BUFFER_DOUBLE	(2 << 10)
#	define R300_CRTC_MICRO_TILE_BUFFER_DIS		(3 << 10)
#	define R300_CRTC_MICRO_TILE_EN_RIGHT		(1 << 12)
#	define R300_CRTC_MICRO_TILE_EN			(1 << 13)
#	define R300_CRTC_MACRO_TILE_EN_RIGHT		(1 << 14)
#       define R300_CRTC_MACRO_TILE_EN                  (1 << 15)
#       define RADEON_CRTC_TILE_EN_RIGHT                (1 << 14)
#       define RADEON_CRTC_TILE_EN                      (1 << 15)
#       define RADEON_CRTC_OFFSET_FLIP_CNTL             (1 << 16)
#       define RADEON_CRTC_STEREO_OFFSET_EN             (1 << 17)

#define R300_CRTC_TILE_X0_Y0	            0x0350
#define R300_CRTC2_TILE_X0_Y0	            0x0358

#define RADEON_CRTC2_OFFSET_CNTL            0x0328
#       define RADEON_CRTC2_OFFSET_FLIP_CNTL (1 << 16)
#       define RADEON_CRTC2_TILE_EN         (1 << 15)
#define RADEON_CRTC_PITCH                   0x022c
#	define RADEON_CRTC_PITCH__SHIFT		 0
#	define RADEON_CRTC_PITCH__RIGHT_SHIFT	16

#define RADEON_CRTC2_PITCH                  0x032c
#define RADEON_CRTC_STATUS                  0x005c
#       define RADEON_CRTC_VBLANK_SAVE      (1 <<  1)
#       define RADEON_CRTC_VBLANK_SAVE_CLEAR  (1 <<  1)
#define RADEON_CRTC2_STATUS                  0x03fc
#       define RADEON_CRTC2_VBLANK_SAVE      (1 <<  1)
#       define RADEON_CRTC2_VBLANK_SAVE_CLEAR  (1 <<  1)
#define RADEON_CRTC_V_SYNC_STRT_WID         0x020c
#       define RADEON_CRTC_V_SYNC_STRT        (0x7ff <<  0)
#       define RADEON_CRTC_V_SYNC_STRT_SHIFT  0
#       define RADEON_CRTC_V_SYNC_WID         (0x1f  << 16)
#       define RADEON_CRTC_V_SYNC_WID_SHIFT   16
#       define RADEON_CRTC_V_SYNC_POL         (1     << 23)
#define RADEON_CRTC2_V_SYNC_STRT_WID        0x030c
#       define RADEON_CRTC2_V_SYNC_STRT       (0x7ff <<  0)
#       define RADEON_CRTC2_V_SYNC_STRT_SHIFT 0
#       define RADEON_CRTC2_V_SYNC_WID        (0x1f  << 16)
#       define RADEON_CRTC2_V_SYNC_WID_SHIFT  16
#       define RADEON_CRTC2_V_SYNC_POL        (1     << 23)
#define RADEON_CRTC_V_TOTAL_DISP            0x0208
#       define RADEON_CRTC_V_TOTAL          (0x07ff << 0)
#       define RADEON_CRTC_V_TOTAL_SHIFT    0
#       define RADEON_CRTC_V_DISP           (0x07ff << 16)
#       define RADEON_CRTC_V_DISP_SHIFT     16
#define RADEON_CRTC2_V_TOTAL_DISP           0x0308
#       define RADEON_CRTC2_V_TOTAL         (0x07ff << 0)
#       define RADEON_CRTC2_V_TOTAL_SHIFT   0
#       define RADEON_CRTC2_V_DISP          (0x07ff << 16)
#       define RADEON_CRTC2_V_DISP_SHIFT    16
#define RADEON_CRTC_VLINE_CRNT_VLINE        0x0210
#       define RADEON_CRTC_CRNT_VLINE_MASK  (0x7ff << 16)
#define RADEON_CRTC2_CRNT_FRAME             0x0314
#define RADEON_CRTC2_GUI_TRIG_VLINE         0x0318
#define RADEON_CRTC2_STATUS                 0x03fc
#define RADEON_CRTC2_VLINE_CRNT_VLINE       0x0310
#define RADEON_CRTC8_DATA                   0x03d5 /* VGA, 0x3b5 */
#define RADEON_CRTC8_IDX                    0x03d4 /* VGA, 0x3b4 */
#define RADEON_CUR_CLR0                     0x026c
#define RADEON_CUR_CLR1                     0x0270
#define RADEON_CUR_HORZ_VERT_OFF            0x0268
#define RADEON_CUR_HORZ_VERT_POSN           0x0264
#define RADEON_CUR_OFFSET                   0x0260
#       define RADEON_CUR_LOCK              (1 << 31)
#define RADEON_CUR2_CLR0                    0x036c
#define RADEON_CUR2_CLR1                    0x0370
#define RADEON_CUR2_HORZ_VERT_OFF           0x0368
#define RADEON_CUR2_HORZ_VERT_POSN          0x0364
#define RADEON_CUR2_OFFSET                  0x0360
#       define RADEON_CUR2_LOCK             (1 << 31)

#define RADEON_DAC_CNTL                     0x0058
#       define RADEON_DAC_RANGE_CNTL        (3 <<  0)
#       define RADEON_DAC_RANGE_CNTL_PS2    (2 <<  0)
#       define RADEON_DAC_RANGE_CNTL_MASK   0x03
#       define RADEON_DAC_BLANKING          (1 <<  2)
#       define RADEON_DAC_CMP_EN            (1 <<  3)
#       define RADEON_DAC_CMP_OUTPUT        (1 <<  7)
#       define RADEON_DAC_8BIT_EN           (1 <<  8)
#       define RADEON_DAC_TVO_EN            (1 << 10)
#       define RADEON_DAC_VGA_ADR_EN        (1 << 13)
#       define RADEON_DAC_PDWN              (1 << 15)
#       define RADEON_DAC_MASK_ALL          (0xff << 24)
#define RADEON_DAC_CNTL2                    0x007c
#       define RADEON_DAC2_TV_CLK_SEL       (0 <<  1)
#       define RADEON_DAC2_DAC_CLK_SEL      (1 <<  0)
#       define RADEON_DAC2_DAC2_CLK_SEL     (1 <<  1)
#       define RADEON_DAC2_PALETTE_ACC_CTL  (1 <<  5)
#       define RADEON_DAC2_CMP_EN           (1 <<  7)
#       define RADEON_DAC2_CMP_OUT_R        (1 <<  8)
#       define RADEON_DAC2_CMP_OUT_G        (1 <<  9)
#       define RADEON_DAC2_CMP_OUT_B        (1 << 10)
#       define RADEON_DAC2_CMP_OUTPUT       (1 << 11)
#define RADEON_DAC_EXT_CNTL                 0x0280
#       define RADEON_DAC2_FORCE_BLANK_OFF_EN (1 << 0)
#       define RADEON_DAC2_FORCE_DATA_EN      (1 << 1)
#       define RADEON_DAC_FORCE_BLANK_OFF_EN  (1 << 4)
#       define RADEON_DAC_FORCE_DATA_EN       (1 << 5)
#       define RADEON_DAC_FORCE_DATA_SEL_MASK (3 << 6)
#       define RADEON_DAC_FORCE_DATA_SEL_R    (0 << 6)
#       define RADEON_DAC_FORCE_DATA_SEL_G    (1 << 6)
#       define RADEON_DAC_FORCE_DATA_SEL_B    (2 << 6)
#       define RADEON_DAC_FORCE_DATA_SEL_RGB  (3 << 6)
#       define RADEON_DAC_FORCE_DATA_MASK   0x0003ff00
#       define RADEON_DAC_FORCE_DATA_SHIFT  8
#define RADEON_DAC_MACRO_CNTL               0x0d04
#       define RADEON_DAC_PDWN_R            (1 << 16)
#       define RADEON_DAC_PDWN_G            (1 << 17)
#       define RADEON_DAC_PDWN_B            (1 << 18)
#define RADEON_DISP_PWR_MAN                 0x0d08
#       define RADEON_DISP_PWR_MAN_D3_CRTC_EN      (1 << 0)
#       define RADEON_DISP_PWR_MAN_D3_CRTC2_EN     (1 << 4)
#       define RADEON_DISP_PWR_MAN_DPMS_ON  (0 << 8)
#       define RADEON_DISP_PWR_MAN_DPMS_STANDBY    (1 << 8)
#       define RADEON_DISP_PWR_MAN_DPMS_SUSPEND    (2 << 8)
#       define RADEON_DISP_PWR_MAN_DPMS_OFF (3 << 8)
#       define RADEON_DISP_D3_RST           (1 << 16)
#       define RADEON_DISP_D3_REG_RST       (1 << 17)
#       define RADEON_DISP_D3_GRPH_RST      (1 << 18)
#       define RADEON_DISP_D3_SUBPIC_RST    (1 << 19)
#       define RADEON_DISP_D3_OV0_RST       (1 << 20)
#       define RADEON_DISP_D1D2_GRPH_RST    (1 << 21)
#       define RADEON_DISP_D1D2_SUBPIC_RST  (1 << 22)
#       define RADEON_DISP_D1D2_OV0_RST     (1 << 23)
#       define RADEON_DIG_TMDS_ENABLE_RST   (1 << 24)
#       define RADEON_TV_ENABLE_RST         (1 << 25)
#       define RADEON_AUTO_PWRUP_EN         (1 << 26)
#define RADEON_TV_DAC_CNTL                  0x088c
#       define RADEON_TV_DAC_NBLANK         (1 << 0)
#       define RADEON_TV_DAC_NHOLD          (1 << 1)
#       define RADEON_TV_DAC_PEDESTAL       (1 <<  2)
#       define RADEON_TV_MONITOR_DETECT_EN  (1 <<  4)
#       define RADEON_TV_DAC_CMPOUT         (1 <<  5)
#       define RADEON_TV_DAC_STD_MASK       (3 <<  8)
#       define RADEON_TV_DAC_STD_PAL        (0 <<  8)
#       define RADEON_TV_DAC_STD_NTSC       (1 <<  8)
#       define RADEON_TV_DAC_STD_PS2        (2 <<  8)
#       define RADEON_TV_DAC_STD_RS343      (3 <<  8)
#       define RADEON_TV_DAC_BGSLEEP        (1 <<  6)
#       define RADEON_TV_DAC_BGADJ_MASK     (0xf <<  16)
#       define RADEON_TV_DAC_BGADJ_SHIFT    16
#       define RADEON_TV_DAC_DACADJ_MASK    (0xf <<  20)
#       define RADEON_TV_DAC_DACADJ_SHIFT   20
#       define RADEON_TV_DAC_RDACPD         (1 <<  24)
#       define RADEON_TV_DAC_GDACPD         (1 <<  25)
#       define RADEON_TV_DAC_BDACPD         (1 <<  26)
#       define RADEON_TV_DAC_RDACDET        (1 << 29)
#       define RADEON_TV_DAC_GDACDET        (1 << 30)
#       define RADEON_TV_DAC_BDACDET        (1 << 31)
#       define R420_TV_DAC_DACADJ_MASK      (0x1f <<  20)
#       define R420_TV_DAC_RDACPD           (1 <<  25)
#       define R420_TV_DAC_GDACPD           (1 <<  26)
#       define R420_TV_DAC_BDACPD           (1 <<  27)
#       define R420_TV_DAC_TVENABLE         (1 <<  28)
#define RADEON_DISP_HW_DEBUG                0x0d14
#       define RADEON_CRT2_DISP1_SEL        (1 <<  5)
#define RADEON_DISP_OUTPUT_CNTL             0x0d64
#       define RADEON_DISP_DAC_SOURCE_MASK  0x03
#       define RADEON_DISP_DAC2_SOURCE_MASK  0x0c
#       define RADEON_DISP_DAC_SOURCE_CRTC2 0x01
#       define RADEON_DISP_DAC_SOURCE_RMX   0x02
#       define RADEON_DISP_DAC_SOURCE_LTU   0x03
#       define RADEON_DISP_DAC2_SOURCE_CRTC2 0x04
#       define RADEON_DISP_TVDAC_SOURCE_MASK  (0x03 << 2)
#       define RADEON_DISP_TVDAC_SOURCE_CRTC  0x0
#       define RADEON_DISP_TVDAC_SOURCE_CRTC2 (0x01 << 2)
#       define RADEON_DISP_TVDAC_SOURCE_RMX   (0x02 << 2)
#       define RADEON_DISP_TVDAC_SOURCE_LTU   (0x03 << 2)
#       define RADEON_DISP_TRANS_MATRIX_MASK  (0x03 << 4)
#       define RADEON_DISP_TRANS_MATRIX_ALPHA_MSB (0x00 << 4)
#       define RADEON_DISP_TRANS_MATRIX_GRAPHICS  (0x01 << 4)
#       define RADEON_DISP_TRANS_MATRIX_VIDEO     (0x02 << 4)
#       define RADEON_DISP_TV_SOURCE_CRTC   (1 << 16) /* crtc1 or crtc2 */
#       define RADEON_DISP_TV_SOURCE_LTU    (0 << 16) /* linear transform unit */
#define RADEON_DISP_TV_OUT_CNTL             0x0d6c
#       define RADEON_DISP_TV_PATH_SRC_CRTC2 (1 << 16)
#       define RADEON_DISP_TV_PATH_SRC_CRTC1 (0 << 16)
#define RADEON_DAC_CRC_SIG                  0x02cc
#define RADEON_DAC_DATA                     0x03c9 /* VGA */
#define RADEON_DAC_MASK                     0x03c6 /* VGA */
#define RADEON_DAC_R_INDEX                  0x03c7 /* VGA */
#define RADEON_DAC_W_INDEX                  0x03c8 /* VGA */
#define RADEON_DDA_CONFIG                   0x02e0
#define RADEON_DDA_ON_OFF                   0x02e4
#define RADEON_DEFAULT_OFFSET               0x16e0
#define RADEON_DEFAULT_PITCH                0x16e4
#define RADEON_DEFAULT_SC_BOTTOM_RIGHT      0x16e8
#       define RADEON_DEFAULT_SC_RIGHT_MAX  (0x1fff <<  0)
#       define RADEON_DEFAULT_SC_BOTTOM_MAX (0x1fff << 16)
#define RADEON_DESTINATION_3D_CLR_CMP_VAL   0x1820
#define RADEON_DESTINATION_3D_CLR_CMP_MSK   0x1824
#define RADEON_DEVICE_ID                    0x0f02 /* PCI */
#define RADEON_DISP_MISC_CNTL               0x0d00
#       define RADEON_SOFT_RESET_GRPH_PP    (1 << 0)
#define RADEON_DISP_MERGE_CNTL		  0x0d60
#       define RADEON_DISP_ALPHA_MODE_MASK  0x03
#       define RADEON_DISP_ALPHA_MODE_KEY   0
#       define RADEON_DISP_ALPHA_MODE_PER_PIXEL 1
#       define RADEON_DISP_ALPHA_MODE_GLOBAL 2
#       define RADEON_DISP_RGB_OFFSET_EN    (1 << 8)
#       define RADEON_DISP_GRPH_ALPHA_MASK  (0xff << 16)
#       define RADEON_DISP_OV0_ALPHA_MASK   (0xff << 24)
#	define RADEON_DISP_LIN_TRANS_BYPASS (0x01 << 9)
#define RADEON_DISP2_MERGE_CNTL		    0x0d68
#       define RADEON_DISP2_RGB_OFFSET_EN   (1 << 8)
#define RADEON_DISP_LIN_TRANS_GRPH_A        0x0d80
#define RADEON_DISP_LIN_TRANS_GRPH_B        0x0d84
#define RADEON_DISP_LIN_TRANS_GRPH_C        0x0d88
#define RADEON_DISP_LIN_TRANS_GRPH_D        0x0d8c
#define RADEON_DISP_LIN_TRANS_GRPH_E        0x0d90
#define RADEON_DISP_LIN_TRANS_GRPH_F        0x0d98
#define RADEON_DP_BRUSH_BKGD_CLR            0x1478
#define RADEON_DP_BRUSH_FRGD_CLR            0x147c
#define RADEON_DP_CNTL                      0x16c0
#       define RADEON_DST_X_LEFT_TO_RIGHT   (1 <<  0)
#       define RADEON_DST_Y_TOP_TO_BOTTOM   (1 <<  1)
#       define RADEON_DP_DST_TILE_LINEAR    (0 <<  3)
#       define RADEON_DP_DST_TILE_MACRO     (1 <<  3)
#       define RADEON_DP_DST_TILE_MICRO     (2 <<  3)
#       define RADEON_DP_DST_TILE_BOTH      (3 <<  3)
#define RADEON_DP_CNTL_XDIR_YDIR_YMAJOR     0x16d0
#       define RADEON_DST_Y_MAJOR             (1 <<  2)
#       define RADEON_DST_Y_DIR_TOP_TO_BOTTOM (1 << 15)
#       define RADEON_DST_X_DIR_LEFT_TO_RIGHT (1 << 31)
#define RADEON_DP_DATATYPE                  0x16c4
#       define RADEON_HOST_BIG_ENDIAN_EN    (1 << 29)
#define RADEON_DP_GUI_MASTER_CNTL           0x146c
#       define RADEON_GMC_SRC_PITCH_OFFSET_CNTL   (1    <<  0)
#       define RADEON_GMC_DST_PITCH_OFFSET_CNTL   (1    <<  1)
#       define RADEON_GMC_SRC_CLIPPING            (1    <<  2)
#       define RADEON_GMC_DST_CLIPPING            (1    <<  3)
#       define RADEON_GMC_BRUSH_DATATYPE_MASK     (0x0f <<  4)
#       define RADEON_GMC_BRUSH_8X8_MONO_FG_BG    (0    <<  4)
#       define RADEON_GMC_BRUSH_8X8_MONO_FG_LA    (1    <<  4)
#       define RADEON_GMC_BRUSH_1X8_MONO_FG_BG    (4    <<  4)
#       define RADEON_GMC_BRUSH_1X8_MONO_FG_LA    (5    <<  4)
#       define RADEON_GMC_BRUSH_32x1_MONO_FG_BG   (6    <<  4)
#       define RADEON_GMC_BRUSH_32x1_MONO_FG_LA   (7    <<  4)
#       define RADEON_GMC_BRUSH_32x32_MONO_FG_BG  (8    <<  4)
#       define RADEON_GMC_BRUSH_32x32_MONO_FG_LA  (9    <<  4)
#       define RADEON_GMC_BRUSH_8x8_COLOR         (10   <<  4)
#       define RADEON_GMC_BRUSH_1X8_COLOR         (12   <<  4)
#       define RADEON_GMC_BRUSH_SOLID_COLOR       (13   <<  4)
#       define RADEON_GMC_BRUSH_NONE              (15   <<  4)
#       define RADEON_GMC_DST_8BPP_CI             (2    <<  8)
#       define RADEON_GMC_DST_15BPP               (3    <<  8)
#       define RADEON_GMC_DST_16BPP               (4    <<  8)
#       define RADEON_GMC_DST_24BPP               (5    <<  8)
#       define RADEON_GMC_DST_32BPP               (6    <<  8)
#       define RADEON_GMC_DST_8BPP_RGB            (7    <<  8)
#       define RADEON_GMC_DST_Y8                  (8    <<  8)
#       define RADEON_GMC_DST_RGB8                (9    <<  8)
#       define RADEON_GMC_DST_VYUY                (11   <<  8)
#       define RADEON_GMC_DST_YVYU                (12   <<  8)
#       define RADEON_GMC_DST_AYUV444             (14   <<  8)
#       define RADEON_GMC_DST_ARGB4444            (15   <<  8)
#       define RADEON_GMC_DST_DATATYPE_MASK       (0x0f <<  8)
#       define RADEON_GMC_DST_DATATYPE_SHIFT      8
#       define RADEON_GMC_SRC_DATATYPE_MASK       (3    << 12)
#       define RADEON_GMC_SRC_DATATYPE_MONO_FG_BG (0    << 12)
#       define RADEON_GMC_SRC_DATATYPE_MONO_FG_LA (1    << 12)
#       define RADEON_GMC_SRC_DATATYPE_COLOR      (3    << 12)
#       define RADEON_GMC_BYTE_PIX_ORDER          (1    << 14)
#       define RADEON_GMC_BYTE_MSB_TO_LSB         (0    << 14)
#       define RADEON_GMC_BYTE_LSB_TO_MSB         (1    << 14)
#       define RADEON_GMC_CONVERSION_TEMP         (1    << 15)
#       define RADEON_GMC_CONVERSION_TEMP_6500    (0    << 15)
#       define RADEON_GMC_CONVERSION_TEMP_9300    (1    << 15)
#       define RADEON_GMC_ROP3_MASK               (0xff << 16)
#       define RADEON_DP_SRC_SOURCE_MASK          (7    << 24)
#       define RADEON_DP_SRC_SOURCE_MEMORY        (2    << 24)
#       define RADEON_DP_SRC_SOURCE_HOST_DATA     (3    << 24)
#       define RADEON_GMC_3D_FCN_EN               (1    << 27)
#       define RADEON_GMC_CLR_CMP_CNTL_DIS        (1    << 28)
#       define RADEON_GMC_AUX_CLIP_DIS            (1    << 29)
#       define RADEON_GMC_WR_MSK_DIS              (1    << 30)
#       define RADEON_GMC_LD_BRUSH_Y_X            (1    << 31)
#       define RADEON_ROP3_ZERO             0x00000000
#       define RADEON_ROP3_DSa              0x00880000
#       define RADEON_ROP3_SDna             0x00440000
#       define RADEON_ROP3_S                0x00cc0000
#       define RADEON_ROP3_DSna             0x00220000
#       define RADEON_ROP3_D                0x00aa0000
#       define RADEON_ROP3_DSx              0x00660000
#       define RADEON_ROP3_DSo              0x00ee0000
#       define RADEON_ROP3_DSon             0x00110000
#       define RADEON_ROP3_DSxn             0x00990000
#       define RADEON_ROP3_Dn               0x00550000
#       define RADEON_ROP3_SDno             0x00dd0000
#       define RADEON_ROP3_Sn               0x00330000
#       define RADEON_ROP3_DSno             0x00bb0000
#       define RADEON_ROP3_DSan             0x00770000
#       define RADEON_ROP3_ONE              0x00ff0000
#       define RADEON_ROP3_DPa              0x00a00000
#       define RADEON_ROP3_PDna             0x00500000
#       define RADEON_ROP3_P                0x00f00000
#       define RADEON_ROP3_DPna             0x000a0000
#       define RADEON_ROP3_D                0x00aa0000
#       define RADEON_ROP3_DPx              0x005a0000
#       define RADEON_ROP3_DPo              0x00fa0000
#       define RADEON_ROP3_DPon             0x00050000
#       define RADEON_ROP3_PDxn             0x00a50000
#       define RADEON_ROP3_PDno             0x00f50000
#       define RADEON_ROP3_Pn               0x000f0000
#       define RADEON_ROP3_DPno             0x00af0000
#       define RADEON_ROP3_DPan             0x005f0000
#define RADEON_DP_GUI_MASTER_CNTL_C         0x1c84
#define RADEON_DP_MIX                       0x16c8
#define RADEON_DP_SRC_BKGD_CLR              0x15dc
#define RADEON_DP_SRC_FRGD_CLR              0x15d8
#define RADEON_DP_WRITE_MASK                0x16cc
#define RADEON_DST_BRES_DEC                 0x1630
#define RADEON_DST_BRES_ERR                 0x1628
#define RADEON_DST_BRES_INC                 0x162c
#define RADEON_DST_BRES_LNTH                0x1634
#define RADEON_DST_BRES_LNTH_SUB            0x1638
#define RADEON_DST_HEIGHT                   0x1410
#define RADEON_DST_HEIGHT_WIDTH             0x143c
#define RADEON_DST_HEIGHT_WIDTH_8           0x158c
#define RADEON_DST_HEIGHT_WIDTH_BW          0x15b4
#define RADEON_DST_HEIGHT_Y                 0x15a0
#define RADEON_DST_LINE_START               0x1600
#define RADEON_DST_LINE_END                 0x1604
#define RADEON_DST_LINE_PATCOUNT            0x1608
#       define RADEON_BRES_CNTL_SHIFT       8
#define RADEON_DST_OFFSET                   0x1404
#define RADEON_DST_PITCH                    0x1408
#define RADEON_DST_PITCH_OFFSET             0x142c
#define RADEON_DST_PITCH_OFFSET_C           0x1c80
#       define RADEON_PITCH_SHIFT           21
#       define RADEON_DST_TILE_LINEAR       (0 << 30)
#       define RADEON_DST_TILE_MACRO        (1 << 30)
#       define RADEON_DST_TILE_MICRO        (2 << 30)
#       define RADEON_DST_TILE_BOTH         (3 << 30)
#define RADEON_DST_WIDTH                    0x140c
#define RADEON_DST_WIDTH_HEIGHT             0x1598
#define RADEON_DST_WIDTH_X                  0x1588
#define RADEON_DST_WIDTH_X_INCY             0x159c
#define RADEON_DST_X                        0x141c
#define RADEON_DST_X_SUB                    0x15a4
#define RADEON_DST_X_Y                      0x1594
#define RADEON_DST_Y                        0x1420
#define RADEON_DST_Y_SUB                    0x15a8
#define RADEON_DST_Y_X                      0x1438

#define RADEON_FCP_CNTL                     0x0910
#      define RADEON_FCP0_SRC_PCICLK             0
#      define RADEON_FCP0_SRC_PCLK               1
#      define RADEON_FCP0_SRC_PCLKb              2
#      define RADEON_FCP0_SRC_HREF               3
#      define RADEON_FCP0_SRC_GND                4
#      define RADEON_FCP0_SRC_HREFb              5
#define RADEON_FLUSH_1                      0x1704
#define RADEON_FLUSH_2                      0x1708
#define RADEON_FLUSH_3                      0x170c
#define RADEON_FLUSH_4                      0x1710
#define RADEON_FLUSH_5                      0x1714
#define RADEON_FLUSH_6                      0x1718
#define RADEON_FLUSH_7                      0x171c
#define RADEON_FOG_3D_TABLE_START           0x1810
#define RADEON_FOG_3D_TABLE_END             0x1814
#define RADEON_FOG_3D_TABLE_DENSITY         0x181c
#define RADEON_FOG_TABLE_INDEX              0x1a14
#define RADEON_FOG_TABLE_DATA               0x1a18
#define RADEON_FP_CRTC_H_TOTAL_DISP         0x0250
#define RADEON_FP_CRTC_V_TOTAL_DISP         0x0254
#       define RADEON_FP_CRTC_H_TOTAL_MASK      0x000003ff
#       define RADEON_FP_CRTC_H_DISP_MASK       0x01ff0000
#       define RADEON_FP_CRTC_V_TOTAL_MASK      0x00000fff
#       define RADEON_FP_CRTC_V_DISP_MASK       0x0fff0000
#       define RADEON_FP_H_SYNC_STRT_CHAR_MASK  0x00001ff8
#       define RADEON_FP_H_SYNC_WID_MASK        0x003f0000
#       define RADEON_FP_V_SYNC_STRT_MASK       0x00000fff
#       define RADEON_FP_V_SYNC_WID_MASK        0x001f0000
#       define RADEON_FP_CRTC_H_TOTAL_SHIFT     0x00000000
#       define RADEON_FP_CRTC_H_DISP_SHIFT      0x00000010
#       define RADEON_FP_CRTC_V_TOTAL_SHIFT     0x00000000
#       define RADEON_FP_CRTC_V_DISP_SHIFT      0x00000010
#       define RADEON_FP_H_SYNC_STRT_CHAR_SHIFT 0x00000003
#       define RADEON_FP_H_SYNC_WID_SHIFT       0x00000010
#       define RADEON_FP_V_SYNC_STRT_SHIFT      0x00000000
#       define RADEON_FP_V_SYNC_WID_SHIFT       0x00000010
#define RADEON_FP_GEN_CNTL                  0x0284
#       define RADEON_FP_FPON                  (1 <<  0)
#       define RADEON_FP_BLANK_EN              (1 <<  1)
#       define RADEON_FP_TMDS_EN               (1 <<  2)
#       define RADEON_FP_PANEL_FORMAT          (1 <<  3)
#       define RADEON_FP_EN_TMDS               (1 <<  7)
#       define RADEON_FP_DETECT_SENSE          (1 <<  8)
#       define R200_FP_SOURCE_SEL_MASK         (3 <<  10)
#       define R200_FP_SOURCE_SEL_CRTC1        (0 <<  10)
#       define R200_FP_SOURCE_SEL_CRTC2        (1 <<  10)
#       define R200_FP_SOURCE_SEL_RMX          (2 <<  10)
#       define R200_FP_SOURCE_SEL_TRANS        (3 <<  10)
#       define RADEON_FP_SEL_CRTC1             (0 << 13)
#       define RADEON_FP_SEL_CRTC2             (1 << 13)
#       define RADEON_FP_CRTC_DONT_SHADOW_HPAR (1 << 15)
#       define RADEON_FP_CRTC_DONT_SHADOW_VPAR (1 << 16)
#       define RADEON_FP_CRTC_DONT_SHADOW_HEND (1 << 17)
#       define RADEON_FP_CRTC_USE_SHADOW_VEND  (1 << 18)
#       define RADEON_FP_RMX_HVSYNC_CONTROL_EN (1 << 20)
#       define RADEON_FP_DFP_SYNC_SEL          (1 << 21)
#       define RADEON_FP_CRTC_LOCK_8DOT        (1 << 22)
#       define RADEON_FP_CRT_SYNC_SEL          (1 << 23)
#       define RADEON_FP_USE_SHADOW_EN         (1 << 24)
#       define RADEON_FP_CRT_SYNC_ALT          (1 << 26)
#define RADEON_FP2_GEN_CNTL                 0x0288
#       define RADEON_FP2_BLANK_EN             (1 <<  1)
#       define RADEON_FP2_ON                   (1 <<  2)
#       define RADEON_FP2_PANEL_FORMAT         (1 <<  3)
#       define RADEON_FP2_DETECT_SENSE         (1 <<  8)
#       define R200_FP2_SOURCE_SEL_MASK        (3 << 10)
#       define R200_FP2_SOURCE_SEL_CRTC1       (0 << 10)
#       define R200_FP2_SOURCE_SEL_CRTC2       (1 << 10)
#       define R200_FP2_SOURCE_SEL_RMX         (2 << 10)
#       define R200_FP2_SOURCE_SEL_TRANS_UNIT  (3 << 10)
#       define RADEON_FP2_SRC_SEL_MASK         (3 << 13)
#       define RADEON_FP2_SRC_SEL_CRTC2        (1 << 13)
#       define RADEON_FP2_FP_POL               (1 << 16)
#       define RADEON_FP2_LP_POL               (1 << 17)
#       define RADEON_FP2_SCK_POL              (1 << 18)
#       define RADEON_FP2_LCD_CNTL_MASK        (7 << 19)
#       define RADEON_FP2_PAD_FLOP_EN          (1 << 22)
#       define RADEON_FP2_CRC_EN               (1 << 23)
#       define RADEON_FP2_CRC_READ_EN          (1 << 24)
#       define RADEON_FP2_DVO_EN               (1 << 25)
#       define RADEON_FP2_DVO_RATE_SEL_SDR     (1 << 26)
#       define R200_FP2_DVO_RATE_SEL_SDR       (1 << 27)
#       define R300_FP2_DVO_CLOCK_MODE_SINGLE  (1 << 28)
#       define R300_FP2_DVO_DUAL_CHANNEL_EN    (1 << 29)
#define RADEON_FP_H_SYNC_STRT_WID           0x02c4
#define RADEON_FP_H2_SYNC_STRT_WID          0x03c4
#define RADEON_FP_HORZ_STRETCH              0x028c
#define RADEON_FP_HORZ2_STRETCH             0x038c
#       define RADEON_HORZ_STRETCH_RATIO_MASK 0xffff
#       define RADEON_HORZ_STRETCH_RATIO_MAX  4096
#       define RADEON_HORZ_PANEL_SIZE         (0x1ff   << 16)
#       define RADEON_HORZ_PANEL_SHIFT        16
#       define RADEON_HORZ_STRETCH_PIXREP     (0      << 25)
#       define RADEON_HORZ_STRETCH_BLEND      (1      << 26)
#       define RADEON_HORZ_STRETCH_ENABLE     (1      << 25)
#       define RADEON_HORZ_AUTO_RATIO         (1      << 27)
#       define RADEON_HORZ_FP_LOOP_STRETCH    (0x7    << 28)
#       define RADEON_HORZ_AUTO_RATIO_INC     (1      << 31)
#define RADEON_FP_HORZ_VERT_ACTIVE          0x0278
#define RADEON_FP_V_SYNC_STRT_WID           0x02c8
#define RADEON_FP_VERT_STRETCH              0x0290
#define RADEON_FP_V2_SYNC_STRT_WID          0x03c8
#define RADEON_FP_VERT2_STRETCH             0x0390
#       define RADEON_VERT_PANEL_SIZE          (0xfff << 12)
#       define RADEON_VERT_PANEL_SHIFT         12
#       define RADEON_VERT_STRETCH_RATIO_MASK  0xfff
#       define RADEON_VERT_STRETCH_RATIO_SHIFT 0
#       define RADEON_VERT_STRETCH_RATIO_MAX   4096
#       define RADEON_VERT_STRETCH_ENABLE      (1     << 25)
#       define RADEON_VERT_STRETCH_LINEREP     (0     << 26)
#       define RADEON_VERT_STRETCH_BLEND       (1     << 26)
#       define RADEON_VERT_AUTO_RATIO_EN       (1     << 27)
#	define RADEON_VERT_AUTO_RATIO_INC      (1     << 31)
#       define RADEON_VERT_STRETCH_RESERVED    0x71000000
#define RS400_FP_2ND_GEN_CNTL               0x0384
#       define RS400_FP_2ND_ON              (1 << 0)
#       define RS400_FP_2ND_BLANK_EN        (1 << 1)
#       define RS400_TMDS_2ND_EN            (1 << 2)
#       define RS400_PANEL_FORMAT_2ND       (1 << 3)
#       define RS400_FP_2ND_EN_TMDS         (1 << 7)
#       define RS400_FP_2ND_DETECT_SENSE    (1 << 8)
#       define RS400_FP_2ND_SOURCE_SEL_MASK        (3 << 10)
#       define RS400_FP_2ND_SOURCE_SEL_CRTC1       (0 << 10)
#       define RS400_FP_2ND_SOURCE_SEL_CRTC2       (1 << 10)
#       define RS400_FP_2ND_SOURCE_SEL_RMX         (2 << 10)
#       define RS400_FP_2ND_DETECT_EN       (1 << 12)
#       define RS400_HPD_2ND_SEL            (1 << 13)
#define RS400_FP2_2_GEN_CNTL                0x0388
#       define RS400_FP2_2_BLANK_EN         (1 << 1)
#       define RS400_FP2_2_ON               (1 << 2)
#       define RS400_FP2_2_PANEL_FORMAT     (1 << 3)
#       define RS400_FP2_2_DETECT_SENSE     (1 << 8)
#       define RS400_FP2_2_SOURCE_SEL_MASK        (3 << 10)
#       define RS400_FP2_2_SOURCE_SEL_CRTC1       (0 << 10)
#       define RS400_FP2_2_SOURCE_SEL_CRTC2       (1 << 10)
#       define RS400_FP2_2_SOURCE_SEL_RMX         (2 << 10)
#       define RS400_FP2_2_DVO2_EN          (1 << 25)
#define RS400_TMDS2_CNTL                    0x0394
#define RS400_TMDS2_TRANSMITTER_CNTL        0x03a4
#       define RS400_TMDS2_PLLEN            (1 << 0)
#       define RS400_TMDS2_PLLRST           (1 << 1)

#define RADEON_GEN_INT_CNTL                 0x0040
#	define RADEON_CRTC_VBLANK_MASK		(1 << 0)
#	define RADEON_CRTC2_VBLANK_MASK		(1 << 9)
#	define RADEON_SW_INT_ENABLE		(1 << 25)
#define RADEON_GEN_INT_STATUS               0x0044
#	define AVIVO_DISPLAY_INT_STATUS		(1 << 0)
#	define RADEON_CRTC_VBLANK_STAT		(1 << 0)
#	define RADEON_CRTC_VBLANK_STAT_ACK	(1 << 0)
#	define RADEON_CRTC2_VBLANK_STAT		(1 << 9)
#	define RADEON_CRTC2_VBLANK_STAT_ACK	(1 << 9)
#	define RADEON_SW_INT_FIRE		(1 << 26)
#	define RADEON_SW_INT_TEST		(1 << 25)
#	define RADEON_SW_INT_TEST_ACK		(1 << 25)
#define RADEON_GENENB                       0x03c3 /* VGA */
#define RADEON_GENFC_RD                     0x03ca /* VGA */
#define RADEON_GENFC_WT                     0x03da /* VGA, 0x03ba */
#define RADEON_GENMO_RD                     0x03cc /* VGA */
#define RADEON_GENMO_WT                     0x03c2 /* VGA */
#define RADEON_GENS0                        0x03c2 /* VGA */
#define RADEON_GENS1                        0x03da /* VGA, 0x03ba */
#define RADEON_GPIO_MONID                   0x0068 /* DDC interface via I2C */ /* DDC3 */
#define RADEON_GPIO_MONIDB                  0x006c
#define RADEON_GPIO_CRT2_DDC                0x006c
#define RADEON_GPIO_DVI_DDC                 0x0064 /* DDC2 */
#define RADEON_GPIO_VGA_DDC                 0x0060 /* DDC1 */
#       define RADEON_GPIO_A_0              (1 <<  0)
#       define RADEON_GPIO_A_1              (1 <<  1)
#       define RADEON_GPIO_Y_0              (1 <<  8)
#       define RADEON_GPIO_Y_1              (1 <<  9)
#       define RADEON_GPIO_Y_SHIFT_0        8
#       define RADEON_GPIO_Y_SHIFT_1        9
#       define RADEON_GPIO_EN_0             (1 << 16)
#       define RADEON_GPIO_EN_1             (1 << 17)
#       define RADEON_GPIO_MASK_0           (1 << 24) /*??*/
#       define RADEON_GPIO_MASK_1           (1 << 25) /*??*/
#define RADEON_GRPH8_DATA                   0x03cf /* VGA */
#define RADEON_GRPH8_IDX                    0x03ce /* VGA */
#define RADEON_GUI_SCRATCH_REG0             0x15e0
#define RADEON_GUI_SCRATCH_REG1             0x15e4
#define RADEON_GUI_SCRATCH_REG2             0x15e8
#define RADEON_GUI_SCRATCH_REG3             0x15ec
#define RADEON_GUI_SCRATCH_REG4             0x15f0
#define RADEON_GUI_SCRATCH_REG5             0x15f4

#define RADEON_HEADER                       0x0f0e /* PCI */
#define RADEON_HOST_DATA0                   0x17c0
#define RADEON_HOST_DATA1                   0x17c4
#define RADEON_HOST_DATA2                   0x17c8
#define RADEON_HOST_DATA3                   0x17cc
#define RADEON_HOST_DATA4                   0x17d0
#define RADEON_HOST_DATA5                   0x17d4
#define RADEON_HOST_DATA6                   0x17d8
#define RADEON_HOST_DATA7                   0x17dc
#define RADEON_HOST_DATA_LAST               0x17e0
#define RADEON_HOST_PATH_CNTL               0x0130
#	define RADEON_HP_LIN_RD_CACHE_DIS   (1 << 24)
#	define RADEON_HDP_READ_BUFFER_INVALIDATE   (1 << 27)
#       define RADEON_HDP_SOFT_RESET        (1 << 26)
#       define RADEON_HDP_APER_CNTL         (1 << 23)
#define RADEON_HTOTAL_CNTL                  0x0009 /* PLL */
#       define RADEON_HTOT_CNTL_VGA_EN      (1 << 28)
#define RADEON_HTOTAL2_CNTL                 0x002e /* PLL */

       /* Multimedia I2C bus */
#define RADEON_I2C_CNTL_0		    0x0090
#define RADEON_I2C_DONE (1<<0)
#define RADEON_I2C_NACK (1<<1)
#define RADEON_I2C_HALT (1<<2)
#define RADEON_I2C_SOFT_RST (1<<5)
#define RADEON_I2C_DRIVE_EN (1<<6)
#define RADEON_I2C_DRIVE_SEL (1<<7)
#define RADEON_I2C_START (1<<8)
#define RADEON_I2C_STOP (1<<9)
#define RADEON_I2C_RECEIVE (1<<10)
#define RADEON_I2C_ABORT (1<<11)
#define RADEON_I2C_GO (1<<12)
#define RADEON_I2C_CNTL_1                   0x0094
#define RADEON_I2C_SEL         (1<<16)
#define RADEON_I2C_EN          (1<<17)
#define RADEON_I2C_DATA			    0x0098

#define RADEON_DVI_I2C_CNTL_0		    0x02e0
#       define R200_DVI_I2C_PIN_SEL(x)      ((x) << 3)
#       define R200_SEL_DDC1                0 /* 0x60 - VGA_DDC */
#       define R200_SEL_DDC2                1 /* 0x64 - DVI_DDC */
#       define R200_SEL_DDC3                2 /* 0x68 - MONID_DDC */
#define RADEON_DVI_I2C_CNTL_1               0x02e4 /* ? */
#define RADEON_DVI_I2C_DATA		    0x02e8

#define RADEON_INTERRUPT_LINE               0x0f3c /* PCI */
#define RADEON_INTERRUPT_PIN                0x0f3d /* PCI */
#define RADEON_IO_BASE                      0x0f14 /* PCI */

#define RADEON_LATENCY                      0x0f0d /* PCI */
#define RADEON_LEAD_BRES_DEC                0x1608
#define RADEON_LEAD_BRES_LNTH               0x161c
#define RADEON_LEAD_BRES_LNTH_SUB           0x1624
#define RADEON_LVDS_GEN_CNTL                0x02d0
#       define RADEON_LVDS_ON               (1   <<  0)
#       define RADEON_LVDS_DISPLAY_DIS      (1   <<  1)
#       define RADEON_LVDS_PANEL_TYPE       (1   <<  2)
#       define RADEON_LVDS_PANEL_FORMAT     (1   <<  3)
#       define RADEON_LVDS_NO_FM            (0   <<  4)
#       define RADEON_LVDS_2_GREY           (1   <<  4)
#       define RADEON_LVDS_4_GREY           (2   <<  4)
#       define RADEON_LVDS_RST_FM           (1   <<  6)
#       define RADEON_LVDS_EN               (1   <<  7)
#       define RADEON_LVDS_BL_MOD_LEVEL_SHIFT 8
#       define RADEON_LVDS_BL_MOD_LEVEL_MASK (0xff << 8)
#       define RADEON_LVDS_BL_MOD_EN        (1   << 16)
#       define RADEON_LVDS_BL_CLK_SEL       (1   << 17)
#       define RADEON_LVDS_DIGON            (1   << 18)
#       define RADEON_LVDS_BLON             (1   << 19)
#       define RADEON_LVDS_FP_POL_LOW       (1   << 20)
#       define RADEON_LVDS_LP_POL_LOW       (1   << 21)
#       define RADEON_LVDS_DTM_POL_LOW      (1   << 22)
#       define RADEON_LVDS_SEL_CRTC2        (1   << 23)
#       define RADEON_LVDS_FPDI_EN          (1   << 27)
#       define RADEON_LVDS_HSYNC_DELAY_SHIFT        28
#define RADEON_LVDS_PLL_CNTL                0x02d4
#       define RADEON_HSYNC_DELAY_SHIFT     28
#       define RADEON_HSYNC_DELAY_MASK      (0xf << 28)
#       define RADEON_LVDS_PLL_EN           (1   << 16)
#       define RADEON_LVDS_PLL_RESET        (1   << 17)
#       define R300_LVDS_SRC_SEL_MASK       (3   << 18)
#       define R300_LVDS_SRC_SEL_CRTC1      (0   << 18)
#       define R300_LVDS_SRC_SEL_CRTC2      (1   << 18)
#       define R300_LVDS_SRC_SEL_RMX        (2   << 18)
#define RADEON_LVDS_SS_GEN_CNTL             0x02ec
#       define RADEON_LVDS_PWRSEQ_DELAY1_SHIFT     16
#       define RADEON_LVDS_PWRSEQ_DELAY2_SHIFT     20

#define RADEON_MAX_LATENCY                  0x0f3f /* PCI */
#define RADEON_DISPLAY_BASE_ADDR            0x23c
#define RADEON_DISPLAY2_BASE_ADDR           0x33c
#define RADEON_OV0_BASE_ADDR                0x43c
#define RADEON_NB_TOM                       0x15c
#define R300_MC_INIT_MISC_LAT_TIMER         0x180
#       define R300_MC_DISP0R_INIT_LAT_SHIFT 8
#       define R300_MC_DISP0R_INIT_LAT_MASK  0xf
#       define R300_MC_DISP1R_INIT_LAT_SHIFT 12
#       define R300_MC_DISP1R_INIT_LAT_MASK  0xf
#define RADEON_MCLK_CNTL                    0x0012 /* PLL */
#       define RADEON_MCLKA_SRC_SEL_MASK    0x7
#       define RADEON_FORCEON_MCLKA         (1 << 16)
#       define RADEON_FORCEON_MCLKB         (1 << 17)
#       define RADEON_FORCEON_YCLKA         (1 << 18)
#       define RADEON_FORCEON_YCLKB         (1 << 19)
#       define RADEON_FORCEON_MC            (1 << 20)
#       define RADEON_FORCEON_AIC           (1 << 21)
#       define R300_DISABLE_MC_MCLKA        (1 << 21)
#       define R300_DISABLE_MC_MCLKB        (1 << 21)
#define RADEON_MCLK_MISC                    0x001f /* PLL */
#       define RADEON_MC_MCLK_MAX_DYN_STOP_LAT (1 << 12)
#       define RADEON_IO_MCLK_MAX_DYN_STOP_LAT (1 << 13)
#       define RADEON_MC_MCLK_DYN_ENABLE    (1 << 14)
#       define RADEON_IO_MCLK_DYN_ENABLE    (1 << 15)
#define RADEON_LCD_GPIO_MASK                0x01a0
#define RADEON_GPIOPAD_EN                   0x01a0
#define RADEON_LCD_GPIO_Y_REG               0x01a4
#define RADEON_MDGPIO_A_REG                 0x01ac
#define RADEON_MDGPIO_EN_REG                0x01b0
#define RADEON_MDGPIO_MASK                  0x0198
#define RADEON_GPIOPAD_MASK                 0x0198
#define RADEON_GPIOPAD_A		    0x019c
#define RADEON_MDGPIO_Y_REG                 0x01b4
#define RADEON_MEM_ADDR_CONFIG              0x0148
#define RADEON_MEM_BASE                     0x0f10 /* PCI */
#define RADEON_MEM_CNTL                     0x0140
#       define RADEON_MEM_NUM_CHANNELS_MASK 0x01
#       define RADEON_MEM_USE_B_CH_ONLY     (1 <<  1)
#       define RV100_HALF_MODE              (1 <<  3)
#       define R300_MEM_NUM_CHANNELS_MASK   0x03
#       define R300_MEM_USE_CD_CH_ONLY      (1 <<  2)
#define RADEON_MEM_TIMING_CNTL              0x0144 /* EXT_MEM_CNTL */
#define RADEON_MEM_INIT_LAT_TIMER           0x0154
#define RADEON_MEM_INTF_CNTL                0x014c
#define RADEON_MEM_SDRAM_MODE_REG           0x0158
#       define RADEON_SDRAM_MODE_MASK       0xffff0000
#       define RADEON_B3MEM_RESET_MASK      0x6fffffff
#       define RADEON_MEM_CFG_TYPE_DDR      (1 << 30)
#define RADEON_MEM_STR_CNTL                 0x0150
#       define RADEON_MEM_PWRUP_COMPL_A     (1 <<  0)
#       define RADEON_MEM_PWRUP_COMPL_B     (1 <<  1)
#       define R300_MEM_PWRUP_COMPL_C       (1 <<  2)
#       define R300_MEM_PWRUP_COMPL_D       (1 <<  3)
#       define RADEON_MEM_PWRUP_COMPLETE    0x03
#       define R300_MEM_PWRUP_COMPLETE      0x0f
#define RADEON_MC_STATUS                    0x0150
#       define RADEON_MC_IDLE               (1 << 2)
#       define R300_MC_IDLE                 (1 << 4)
#define RADEON_MEM_VGA_RP_SEL               0x003c
#define RADEON_MEM_VGA_WP_SEL               0x0038
#define RADEON_MIN_GRANT                    0x0f3e /* PCI */
#define RADEON_MM_DATA                      0x0004
#define RADEON_MM_INDEX                     0x0000
#	define RADEON_MM_APER		(1 << 31)
#define RADEON_MPLL_CNTL                    0x000e /* PLL */
#define RADEON_MPP_TB_CONFIG                0x01c0 /* ? */
#define RADEON_MPP_GP_CONFIG                0x01c8 /* ? */
#define RADEON_SEPROM_CNTL1                 0x01c0
#       define RADEON_SCK_PRESCALE_SHIFT    24
#       define RADEON_SCK_PRESCALE_MASK     (0xff << 24)
#define R300_MC_IND_INDEX                   0x01f8
#       define R300_MC_IND_ADDR_MASK        0x3f
#       define R300_MC_IND_WR_EN            (1 << 8)
#define R300_MC_IND_DATA                    0x01fc
#define R300_MC_READ_CNTL_AB                0x017c
#       define R300_MEM_RBS_POSITION_A_MASK 0x03
#define R300_MC_READ_CNTL_CD_mcind	    0x24
#       define R300_MEM_RBS_POSITION_C_MASK 0x03

#define RADEON_N_VIF_COUNT                  0x0248

#define RADEON_OV0_AUTO_FLIP_CNTL           0x0470
#       define  RADEON_OV0_AUTO_FLIP_CNTL_SOFT_BUF_NUM        0x00000007
#       define  RADEON_OV0_AUTO_FLIP_CNTL_SOFT_REPEAT_FIELD   0x00000008
#       define  RADEON_OV0_AUTO_FLIP_CNTL_SOFT_BUF_ODD        0x00000010
#       define  RADEON_OV0_AUTO_FLIP_CNTL_IGNORE_REPEAT_FIELD 0x00000020
#       define  RADEON_OV0_AUTO_FLIP_CNTL_SOFT_EOF_TOGGLE     0x00000040
#       define  RADEON_OV0_AUTO_FLIP_CNTL_VID_PORT_SELECT     0x00000300
#       define  RADEON_OV0_AUTO_FLIP_CNTL_P1_FIRST_LINE_EVEN  0x00010000
#       define  RADEON_OV0_AUTO_FLIP_CNTL_SHIFT_EVEN_DOWN     0x00040000
#       define  RADEON_OV0_AUTO_FLIP_CNTL_SHIFT_ODD_DOWN      0x00080000
#       define  RADEON_OV0_AUTO_FLIP_CNTL_FIELD_POL_SOURCE    0x00800000

#define RADEON_OV0_COLOUR_CNTL              0x04E0
#define RADEON_OV0_DEINTERLACE_PATTERN      0x0474
#define RADEON_OV0_EXCLUSIVE_HORZ           0x0408
#       define  RADEON_EXCL_HORZ_START_MASK        0x000000ff
#       define  RADEON_EXCL_HORZ_END_MASK          0x0000ff00
#       define  RADEON_EXCL_HORZ_BACK_PORCH_MASK   0x00ff0000
#       define  RADEON_EXCL_HORZ_EXCLUSIVE_EN      0x80000000
#define RADEON_OV0_EXCLUSIVE_VERT           0x040C
#       define  RADEON_EXCL_VERT_START_MASK        0x000003ff
#       define  RADEON_EXCL_VERT_END_MASK          0x03ff0000
#define RADEON_OV0_FILTER_CNTL              0x04A0
#       define RADEON_FILTER_PROGRAMMABLE_COEF            0x0
#       define RADEON_FILTER_HC_COEF_HORZ_Y               0x1
#       define RADEON_FILTER_HC_COEF_HORZ_UV              0x2
#       define RADEON_FILTER_HC_COEF_VERT_Y               0x4
#       define RADEON_FILTER_HC_COEF_VERT_UV              0x8
#       define RADEON_FILTER_HARDCODED_COEF               0xf
#       define RADEON_FILTER_COEF_MASK                    0xf

#define RADEON_OV0_FOUR_TAP_COEF_0          0x04B0
#define RADEON_OV0_FOUR_TAP_COEF_1          0x04B4
#define RADEON_OV0_FOUR_TAP_COEF_2          0x04B8
#define RADEON_OV0_FOUR_TAP_COEF_3          0x04BC
#define RADEON_OV0_FOUR_TAP_COEF_4          0x04C0
#define RADEON_OV0_FLAG_CNTL                0x04DC
#define RADEON_OV0_GAMMA_000_00F            0x0d40
#define RADEON_OV0_GAMMA_010_01F            0x0d44
#define RADEON_OV0_GAMMA_020_03F            0x0d48
#define RADEON_OV0_GAMMA_040_07F            0x0d4c
#define RADEON_OV0_GAMMA_080_0BF            0x0e00
#define RADEON_OV0_GAMMA_0C0_0FF            0x0e04
#define RADEON_OV0_GAMMA_100_13F            0x0e08
#define RADEON_OV0_GAMMA_140_17F            0x0e0c
#define RADEON_OV0_GAMMA_180_1BF            0x0e10
#define RADEON_OV0_GAMMA_1C0_1FF            0x0e14
#define RADEON_OV0_GAMMA_200_23F            0x0e18
#define RADEON_OV0_GAMMA_240_27F            0x0e1c
#define RADEON_OV0_GAMMA_280_2BF            0x0e20
#define RADEON_OV0_GAMMA_2C0_2FF            0x0e24
#define RADEON_OV0_GAMMA_300_33F            0x0e28
#define RADEON_OV0_GAMMA_340_37F            0x0e2c
#define RADEON_OV0_GAMMA_380_3BF            0x0d50
#define RADEON_OV0_GAMMA_3C0_3FF            0x0d54
#define RADEON_OV0_GRAPHICS_KEY_CLR_LOW     0x04EC
#define RADEON_OV0_GRAPHICS_KEY_CLR_HIGH    0x04F0
#define RADEON_OV0_H_INC                    0x0480
#define RADEON_OV0_KEY_CNTL                 0x04F4
#       define  RADEON_VIDEO_KEY_FN_MASK    0x00000003L
#       define  RADEON_VIDEO_KEY_FN_FALSE   0x00000000L
#       define  RADEON_VIDEO_KEY_FN_TRUE    0x00000001L
#       define  RADEON_VIDEO_KEY_FN_EQ      0x00000002L
#       define  RADEON_VIDEO_KEY_FN_NE      0x00000003L
#       define  RADEON_GRAPHIC_KEY_FN_MASK  0x00000030L
#       define  RADEON_GRAPHIC_KEY_FN_FALSE 0x00000000L
#       define  RADEON_GRAPHIC_KEY_FN_TRUE  0x00000010L
#       define  RADEON_GRAPHIC_KEY_FN_EQ    0x00000020L
#       define  RADEON_GRAPHIC_KEY_FN_NE    0x00000030L
#       define  RADEON_CMP_MIX_MASK         0x00000100L
#       define  RADEON_CMP_MIX_OR           0x00000000L
#       define  RADEON_CMP_MIX_AND          0x00000100L
#define RADEON_OV0_LIN_TRANS_A              0x0d20
#define RADEON_OV0_LIN_TRANS_B              0x0d24
#define RADEON_OV0_LIN_TRANS_C              0x0d28
#define RADEON_OV0_LIN_TRANS_D              0x0d2c
#define RADEON_OV0_LIN_TRANS_E              0x0d30
#define RADEON_OV0_LIN_TRANS_F              0x0d34
#define RADEON_OV0_P1_BLANK_LINES_AT_TOP    0x0430
#       define  RADEON_P1_BLNK_LN_AT_TOP_M1_MASK   0x00000fffL
#       define  RADEON_P1_ACTIVE_LINES_M1          0x0fff0000L
#define RADEON_OV0_P1_H_ACCUM_INIT          0x0488
#define RADEON_OV0_P1_V_ACCUM_INIT          0x0428
#       define  RADEON_OV0_P1_MAX_LN_IN_PER_LN_OUT 0x00000003L
#       define  RADEON_OV0_P1_V_ACCUM_INIT_MASK    0x01ff8000L
#define RADEON_OV0_P1_X_START_END           0x0494
#define RADEON_OV0_P2_X_START_END           0x0498
#define RADEON_OV0_P23_BLANK_LINES_AT_TOP   0x0434
#       define  RADEON_P23_BLNK_LN_AT_TOP_M1_MASK  0x000007ffL
#       define  RADEON_P23_ACTIVE_LINES_M1         0x07ff0000L
#define RADEON_OV0_P23_H_ACCUM_INIT         0x048C
#define RADEON_OV0_P23_V_ACCUM_INIT         0x042C
#define RADEON_OV0_P3_X_START_END           0x049C
#define RADEON_OV0_REG_LOAD_CNTL            0x0410
#       define  RADEON_REG_LD_CTL_LOCK                 0x00000001L
#       define  RADEON_REG_LD_CTL_VBLANK_DURING_LOCK   0x00000002L
#       define  RADEON_REG_LD_CTL_STALL_GUI_UNTIL_FLIP 0x00000004L
#       define  RADEON_REG_LD_CTL_LOCK_READBACK        0x00000008L
#       define  RADEON_REG_LD_CTL_FLIP_READBACK        0x00000010L
#define RADEON_OV0_SCALE_CNTL               0x0420
#       define  RADEON_SCALER_HORZ_PICK_NEAREST    0x00000004L
#       define  RADEON_SCALER_VERT_PICK_NEAREST    0x00000008L
#       define  RADEON_SCALER_SIGNED_UV            0x00000010L
#       define  RADEON_SCALER_GAMMA_SEL_MASK       0x00000060L
#       define  RADEON_SCALER_GAMMA_SEL_BRIGHT     0x00000000L
#       define  RADEON_SCALER_GAMMA_SEL_G22        0x00000020L
#       define  RADEON_SCALER_GAMMA_SEL_G18        0x00000040L
#       define  RADEON_SCALER_GAMMA_SEL_G14        0x00000060L
#       define  RADEON_SCALER_COMCORE_SHIFT_UP_ONE 0x00000080L
#       define  RADEON_SCALER_SURFAC_FORMAT        0x00000f00L
#       define  RADEON_SCALER_SOURCE_15BPP         0x00000300L
#       define  RADEON_SCALER_SOURCE_16BPP         0x00000400L
#       define  RADEON_SCALER_SOURCE_32BPP         0x00000600L
#       define  RADEON_SCALER_SOURCE_YUV9          0x00000900L
#       define  RADEON_SCALER_SOURCE_YUV12         0x00000A00L
#       define  RADEON_SCALER_SOURCE_VYUY422       0x00000B00L
#       define  RADEON_SCALER_SOURCE_YVYU422       0x00000C00L
#       define  RADEON_SCALER_ADAPTIVE_DEINT       0x00001000L
#       define  RADEON_SCALER_TEMPORAL_DEINT       0x00002000L
#       define  RADEON_SCALER_CRTC_SEL             0x00004000L
#       define  RADEON_SCALER_SMART_SWITCH         0x00008000L
#       define  RADEON_SCALER_BURST_PER_PLANE      0x007F0000L
#       define  RADEON_SCALER_DOUBLE_BUFFER        0x01000000L
#       define  RADEON_SCALER_DIS_LIMIT            0x08000000L
#       define  RADEON_SCALER_LIN_TRANS_BYPASS     0x10000000L
#       define  RADEON_SCALER_INT_EMU              0x20000000L
#       define  RADEON_SCALER_ENABLE               0x40000000L
#       define  RADEON_SCALER_SOFT_RESET           0x80000000L
#define RADEON_OV0_STEP_BY                  0x0484
#define RADEON_OV0_TEST                     0x04F8
#define RADEON_OV0_V_INC                    0x0424
#define RADEON_OV0_VID_BUF_PITCH0_VALUE     0x0460
#define RADEON_OV0_VID_BUF_PITCH1_VALUE     0x0464
#define RADEON_OV0_VID_BUF0_BASE_ADRS       0x0440
#       define  RADEON_VIF_BUF0_PITCH_SEL          0x00000001L
#       define  RADEON_VIF_BUF0_TILE_ADRS          0x00000002L
#       define  RADEON_VIF_BUF0_BASE_ADRS_MASK     0x03fffff0L
#       define  RADEON_VIF_BUF0_1ST_LINE_LSBS_MASK 0x48000000L
#define RADEON_OV0_VID_BUF1_BASE_ADRS       0x0444
#       define  RADEON_VIF_BUF1_PITCH_SEL          0x00000001L
#       define  RADEON_VIF_BUF1_TILE_ADRS          0x00000002L
#       define  RADEON_VIF_BUF1_BASE_ADRS_MASK     0x03fffff0L
#       define  RADEON_VIF_BUF1_1ST_LINE_LSBS_MASK 0x48000000L
#define RADEON_OV0_VID_BUF2_BASE_ADRS       0x0448
#       define  RADEON_VIF_BUF2_PITCH_SEL          0x00000001L
#       define  RADEON_VIF_BUF2_TILE_ADRS          0x00000002L
#       define  RADEON_VIF_BUF2_BASE_ADRS_MASK     0x03fffff0L
#       define  RADEON_VIF_BUF2_1ST_LINE_LSBS_MASK 0x48000000L
#define RADEON_OV0_VID_BUF3_BASE_ADRS       0x044C
#define RADEON_OV0_VID_BUF4_BASE_ADRS       0x0450
#define RADEON_OV0_VID_BUF5_BASE_ADRS       0x0454
#define RADEON_OV0_VIDEO_KEY_CLR_HIGH       0x04E8
#define RADEON_OV0_VIDEO_KEY_CLR_LOW        0x04E4
#define RADEON_OV0_Y_X_START                0x0400
#define RADEON_OV0_Y_X_END                  0x0404
#define RADEON_OV1_Y_X_START                0x0600
#define RADEON_OV1_Y_X_END                  0x0604
#define RADEON_OVR_CLR                      0x0230
#define RADEON_OVR_WID_LEFT_RIGHT           0x0234
#define RADEON_OVR_WID_TOP_BOTTOM           0x0238

/* first capture unit */

#define RADEON_CAP0_BUF0_OFFSET           0x0920
#define RADEON_CAP0_BUF1_OFFSET           0x0924
#define RADEON_CAP0_BUF0_EVEN_OFFSET      0x0928
#define RADEON_CAP0_BUF1_EVEN_OFFSET      0x092C

#define RADEON_CAP0_BUF_PITCH             0x0930
#define RADEON_CAP0_V_WINDOW              0x0934
#define RADEON_CAP0_H_WINDOW              0x0938
#define RADEON_CAP0_VBI0_OFFSET           0x093C
#define RADEON_CAP0_VBI1_OFFSET           0x0940
#define RADEON_CAP0_VBI_V_WINDOW          0x0944
#define RADEON_CAP0_VBI_H_WINDOW          0x0948
#define RADEON_CAP0_PORT_MODE_CNTL        0x094C
#define RADEON_CAP0_TRIG_CNTL             0x0950
#define RADEON_CAP0_DEBUG                 0x0954
#define RADEON_CAP0_CONFIG                0x0958
#       define RADEON_CAP0_CONFIG_CONTINUOS          0x00000001
#       define RADEON_CAP0_CONFIG_START_FIELD_EVEN   0x00000002
#       define RADEON_CAP0_CONFIG_START_BUF_GET      0x00000004
#       define RADEON_CAP0_CONFIG_START_BUF_SET      0x00000008
#       define RADEON_CAP0_CONFIG_BUF_TYPE_ALT       0x00000010
#       define RADEON_CAP0_CONFIG_BUF_TYPE_FRAME     0x00000020
#       define RADEON_CAP0_CONFIG_ONESHOT_MODE_FRAME 0x00000040
#       define RADEON_CAP0_CONFIG_BUF_MODE_DOUBLE    0x00000080
#       define RADEON_CAP0_CONFIG_BUF_MODE_TRIPLE    0x00000100
#       define RADEON_CAP0_CONFIG_MIRROR_EN          0x00000200
#       define RADEON_CAP0_CONFIG_ONESHOT_MIRROR_EN  0x00000400
#       define RADEON_CAP0_CONFIG_VIDEO_SIGNED_UV    0x00000800
#       define RADEON_CAP0_CONFIG_ANC_DECODE_EN      0x00001000
#       define RADEON_CAP0_CONFIG_VBI_EN             0x00002000
#       define RADEON_CAP0_CONFIG_SOFT_PULL_DOWN_EN  0x00004000
#       define RADEON_CAP0_CONFIG_VIP_EXTEND_FLAG_EN 0x00008000
#       define RADEON_CAP0_CONFIG_FAKE_FIELD_EN      0x00010000
#       define RADEON_CAP0_CONFIG_ODD_ONE_MORE_LINE  0x00020000
#       define RADEON_CAP0_CONFIG_EVEN_ONE_MORE_LINE 0x00040000
#       define RADEON_CAP0_CONFIG_HORZ_DIVIDE_2      0x00080000
#       define RADEON_CAP0_CONFIG_HORZ_DIVIDE_4      0x00100000
#       define RADEON_CAP0_CONFIG_VERT_DIVIDE_2      0x00200000
#       define RADEON_CAP0_CONFIG_VERT_DIVIDE_4      0x00400000
#       define RADEON_CAP0_CONFIG_FORMAT_BROOKTREE   0x00000000
#       define RADEON_CAP0_CONFIG_FORMAT_CCIR656     0x00800000
#       define RADEON_CAP0_CONFIG_FORMAT_ZV          0x01000000
#       define RADEON_CAP0_CONFIG_FORMAT_VIP         0x01800000
#       define RADEON_CAP0_CONFIG_FORMAT_TRANSPORT   0x02000000
#       define RADEON_CAP0_CONFIG_HORZ_DECIMATOR     0x04000000
#       define RADEON_CAP0_CONFIG_VIDEO_IN_YVYU422   0x00000000
#       define RADEON_CAP0_CONFIG_VIDEO_IN_VYUY422   0x20000000
#       define RADEON_CAP0_CONFIG_VBI_DIVIDE_2       0x40000000
#       define RADEON_CAP0_CONFIG_VBI_DIVIDE_4       0x80000000
#define RADEON_CAP0_ANC_ODD_OFFSET        0x095C
#define RADEON_CAP0_ANC_EVEN_OFFSET       0x0960
#define RADEON_CAP0_ANC_H_WINDOW          0x0964
#define RADEON_CAP0_VIDEO_SYNC_TEST       0x0968
#define RADEON_CAP0_ONESHOT_BUF_OFFSET    0x096C
#define RADEON_CAP0_BUF_STATUS            0x0970
/* #define RADEON_CAP0_DWNSC_XRATIO       0x0978 */
/* #define RADEON_CAP0_XSHARPNESS                 0x097C */
#define RADEON_CAP0_VBI2_OFFSET           0x0980
#define RADEON_CAP0_VBI3_OFFSET           0x0984
#define RADEON_CAP0_ANC2_OFFSET           0x0988
#define RADEON_CAP0_ANC3_OFFSET           0x098C
#define RADEON_VID_BUFFER_CONTROL         0x0900

/* second capture unit */

#define RADEON_CAP1_BUF0_OFFSET           0x0990
#define RADEON_CAP1_BUF1_OFFSET           0x0994
#define RADEON_CAP1_BUF0_EVEN_OFFSET      0x0998
#define RADEON_CAP1_BUF1_EVEN_OFFSET      0x099C

#define RADEON_CAP1_BUF_PITCH             0x09A0
#define RADEON_CAP1_V_WINDOW              0x09A4
#define RADEON_CAP1_H_WINDOW              0x09A8
#define RADEON_CAP1_VBI_ODD_OFFSET        0x09AC
#define RADEON_CAP1_VBI_EVEN_OFFSET       0x09B0
#define RADEON_CAP1_VBI_V_WINDOW                  0x09B4
#define RADEON_CAP1_VBI_H_WINDOW                  0x09B8
#define RADEON_CAP1_PORT_MODE_CNTL        0x09BC
#define RADEON_CAP1_TRIG_CNTL             0x09C0
#define RADEON_CAP1_DEBUG                         0x09C4
#define RADEON_CAP1_CONFIG                0x09C8
#define RADEON_CAP1_ANC_ODD_OFFSET        0x09CC
#define RADEON_CAP1_ANC_EVEN_OFFSET       0x09D0
#define RADEON_CAP1_ANC_H_WINDOW                  0x09D4
#define RADEON_CAP1_VIDEO_SYNC_TEST       0x09D8
#define RADEON_CAP1_ONESHOT_BUF_OFFSET    0x09DC
#define RADEON_CAP1_BUF_STATUS            0x09E0
#define RADEON_CAP1_DWNSC_XRATIO                  0x09E8
#define RADEON_CAP1_XSHARPNESS            0x09EC

/* misc multimedia registers */

#define RADEON_IDCT_RUNS                  0x1F80
#define RADEON_IDCT_LEVELS                0x1F84
#define RADEON_IDCT_CONTROL               0x1FBC
#define RADEON_IDCT_AUTH_CONTROL          0x1F88
#define RADEON_IDCT_AUTH                  0x1F8C

#define RADEON_P2PLL_CNTL                   0x002a /* P2PLL */
#       define RADEON_P2PLL_RESET                (1 <<  0)
#       define RADEON_P2PLL_SLEEP                (1 <<  1)
#       define RADEON_P2PLL_PVG_MASK             (7 << 11)
#       define RADEON_P2PLL_PVG_SHIFT            11
#       define RADEON_P2PLL_ATOMIC_UPDATE_EN     (1 << 16)
#       define RADEON_P2PLL_VGA_ATOMIC_UPDATE_EN (1 << 17)
#       define RADEON_P2PLL_ATOMIC_UPDATE_VSYNC  (1 << 18)
#define RADEON_P2PLL_DIV_0                  0x002c
#       define RADEON_P2PLL_FB0_DIV_MASK    0x07ff
#       define RADEON_P2PLL_POST0_DIV_MASK  0x00070000
#define RADEON_P2PLL_REF_DIV                0x002B /* PLL */
#       define RADEON_P2PLL_REF_DIV_MASK    0x03ff
#       define RADEON_P2PLL_ATOMIC_UPDATE_R (1 << 15) /* same as _W */
#       define RADEON_P2PLL_ATOMIC_UPDATE_W (1 << 15) /* same as _R */
#       define R300_PPLL_REF_DIV_ACC_MASK   (0x3ff << 18)
#       define R300_PPLL_REF_DIV_ACC_SHIFT  18
#define RADEON_PALETTE_DATA                 0x00b4
#define RADEON_PALETTE_30_DATA              0x00b8
#define RADEON_PALETTE_INDEX                0x00b0
#define RADEON_PCI_GART_PAGE                0x017c
#define RADEON_PIXCLKS_CNTL                 0x002d
#       define RADEON_PIX2CLK_SRC_SEL_MASK     0x03
#       define RADEON_PIX2CLK_SRC_SEL_CPUCLK   0x00
#       define RADEON_PIX2CLK_SRC_SEL_PSCANCLK 0x01
#       define RADEON_PIX2CLK_SRC_SEL_BYTECLK  0x02
#       define RADEON_PIX2CLK_SRC_SEL_P2PLLCLK 0x03
#       define RADEON_PIX2CLK_ALWAYS_ONb       (1<<6)
#       define RADEON_PIX2CLK_DAC_ALWAYS_ONb   (1<<7)
#       define RADEON_PIXCLK_TV_SRC_SEL        (1 << 8)
#       define RADEON_DISP_TVOUT_PIXCLK_TV_ALWAYS_ONb (1 << 9)
#       define R300_DVOCLK_ALWAYS_ONb          (1 << 10)
#       define RADEON_PIXCLK_BLEND_ALWAYS_ONb  (1 << 11)
#       define RADEON_PIXCLK_GV_ALWAYS_ONb     (1 << 12)
#       define RADEON_PIXCLK_DIG_TMDS_ALWAYS_ONb (1 << 13)
#       define R300_PIXCLK_DVO_ALWAYS_ONb      (1 << 13)
#       define RADEON_PIXCLK_LVDS_ALWAYS_ONb   (1 << 14)
#       define RADEON_PIXCLK_TMDS_ALWAYS_ONb   (1 << 15)
#       define R300_PIXCLK_TRANS_ALWAYS_ONb    (1 << 16)
#       define R300_PIXCLK_TVO_ALWAYS_ONb      (1 << 17)
#       define R300_P2G2CLK_ALWAYS_ONb         (1 << 18)
#       define R300_P2G2CLK_DAC_ALWAYS_ONb     (1 << 19)
#       define R300_DISP_DAC_PIXCLK_DAC2_BLANK_OFF (1 << 23)
#define RADEON_PLANE_3D_MASK_C              0x1d44
#define RADEON_PLL_TEST_CNTL                0x0013 /* PLL */
#       define RADEON_PLL_MASK_READ_B          (1 << 9)
#define RADEON_PMI_CAP_ID                   0x0f5c /* PCI */
#define RADEON_PMI_DATA                     0x0f63 /* PCI */
#define RADEON_PMI_NXT_CAP_PTR              0x0f5d /* PCI */
#define RADEON_PMI_PMC_REG                  0x0f5e /* PCI */
#define RADEON_PMI_PMCSR_REG                0x0f60 /* PCI */
#define RADEON_PMI_REGISTER                 0x0f5c /* PCI */
#define RADEON_PPLL_CNTL                    0x0002 /* PLL */
#       define RADEON_PPLL_RESET                (1 <<  0)
#       define RADEON_PPLL_SLEEP                (1 <<  1)
#       define RADEON_PPLL_PVG_MASK             (7 << 11)
#       define RADEON_PPLL_PVG_SHIFT            11
#       define RADEON_PPLL_ATOMIC_UPDATE_EN     (1 << 16)
#       define RADEON_PPLL_VGA_ATOMIC_UPDATE_EN (1 << 17)
#       define RADEON_PPLL_ATOMIC_UPDATE_VSYNC  (1 << 18)
#define RADEON_PPLL_DIV_0                   0x0004 /* PLL */
#define RADEON_PPLL_DIV_1                   0x0005 /* PLL */
#define RADEON_PPLL_DIV_2                   0x0006 /* PLL */
#define RADEON_PPLL_DIV_3                   0x0007 /* PLL */
#       define RADEON_PPLL_FB3_DIV_MASK     0x07ff
#       define RADEON_PPLL_POST3_DIV_MASK   0x00070000
#define RADEON_PPLL_REF_DIV                 0x0003 /* PLL */
#       define RADEON_PPLL_REF_DIV_MASK     0x03ff
#       define RADEON_PPLL_ATOMIC_UPDATE_R  (1 << 15) /* same as _W */
#       define RADEON_PPLL_ATOMIC_UPDATE_W  (1 << 15) /* same as _R */
#define RADEON_PWR_MNGMT_CNTL_STATUS        0x0f60 /* PCI */

#define RADEON_RBBM_GUICNTL                 0x172c
#       define RADEON_HOST_DATA_SWAP_NONE   (0 << 0)
#       define RADEON_HOST_DATA_SWAP_16BIT  (1 << 0)
#       define RADEON_HOST_DATA_SWAP_32BIT  (2 << 0)
#       define RADEON_HOST_DATA_SWAP_HDW    (3 << 0)
#define RADEON_RBBM_SOFT_RESET              0x00f0
#       define RADEON_SOFT_RESET_CP         (1 <<  0)
#       define RADEON_SOFT_RESET_HI         (1 <<  1)
#       define RADEON_SOFT_RESET_SE         (1 <<  2)
#       define RADEON_SOFT_RESET_RE         (1 <<  3)
#       define RADEON_SOFT_RESET_PP         (1 <<  4)
#       define RADEON_SOFT_RESET_E2         (1 <<  5)
#       define RADEON_SOFT_RESET_RB         (1 <<  6)
#       define RADEON_SOFT_RESET_HDP        (1 <<  7)
#define RADEON_RBBM_STATUS                  0x0e40
#       define RADEON_RBBM_FIFOCNT_MASK     0x007f
#       define RADEON_RBBM_ACTIVE           (1 << 31)
#define RADEON_RB2D_DSTCACHE_CTLSTAT        0x342c
#       define RADEON_RB2D_DC_FLUSH         (3 << 0)
#       define RADEON_RB2D_DC_FREE          (3 << 2)
#       define RADEON_RB2D_DC_FLUSH_ALL     0xf
#       define RADEON_RB2D_DC_BUSY          (1 << 31)
#define RADEON_RB2D_DSTCACHE_MODE           0x3428
#define RADEON_DSTCACHE_CTLSTAT             0x1714

#define RADEON_RB3D_ZCACHE_MODE             0x3250
#define RADEON_RB3D_ZCACHE_CTLSTAT          0x3254
#       define RADEON_RB3D_ZC_FLUSH_ALL     0x5
#define RADEON_RB3D_DSTCACHE_MODE           0x3258
# define RADEON_RB3D_DC_CACHE_ENABLE            (0)
# define RADEON_RB3D_DC_2D_CACHE_DISABLE        (1)
# define RADEON_RB3D_DC_3D_CACHE_DISABLE        (2)
# define RADEON_RB3D_DC_CACHE_DISABLE           (3)
# define RADEON_RB3D_DC_2D_CACHE_LINESIZE_128   (1 << 2)
# define RADEON_RB3D_DC_3D_CACHE_LINESIZE_128   (2 << 2)
# define RADEON_RB3D_DC_2D_CACHE_AUTOFLUSH      (1 << 8)
# define RADEON_RB3D_DC_3D_CACHE_AUTOFLUSH      (2 << 8)
# define R200_RB3D_DC_2D_CACHE_AUTOFREE         (1 << 10)
# define R200_RB3D_DC_3D_CACHE_AUTOFREE         (2 << 10)
# define RADEON_RB3D_DC_FORCE_RMW               (1 << 16)
# define RADEON_RB3D_DC_DISABLE_RI_FILL         (1 << 24)
# define RADEON_RB3D_DC_DISABLE_RI_READ         (1 << 25)

#define RADEON_RB3D_DSTCACHE_CTLSTAT            0x325C
# define RADEON_RB3D_DC_FLUSH                   (3 << 0)
# define RADEON_RB3D_DC_FREE                    (3 << 2)
# define RADEON_RB3D_DC_FLUSH_ALL               0xf
# define RADEON_RB3D_DC_BUSY                    (1 << 31)

#define RADEON_REG_BASE                     0x0f18 /* PCI */
#define RADEON_REGPROG_INF                  0x0f09 /* PCI */
#define RADEON_REVISION_ID                  0x0f08 /* PCI */

#define RADEON_SC_BOTTOM                    0x164c
#define RADEON_SC_BOTTOM_RIGHT              0x16f0
#define RADEON_SC_BOTTOM_RIGHT_C            0x1c8c
#define RADEON_SC_LEFT                      0x1640
#define RADEON_SC_RIGHT                     0x1644
#define RADEON_SC_TOP                       0x1648
#define RADEON_SC_TOP_LEFT                  0x16ec
#define RADEON_SC_TOP_LEFT_C                0x1c88
#       define RADEON_SC_SIGN_MASK_LO       0x8000
#       define RADEON_SC_SIGN_MASK_HI       0x80000000
#define RADEON_M_SPLL_REF_FB_DIV            0x000a /* PLL */
#	define RADEON_M_SPLL_REF_DIV_SHIFT  0
#	define RADEON_M_SPLL_REF_DIV_MASK   0xff
#	define RADEON_MPLL_FB_DIV_SHIFT     8
#	define RADEON_MPLL_FB_DIV_MASK      0xff
#	define RADEON_SPLL_FB_DIV_SHIFT     16
#	define RADEON_SPLL_FB_DIV_MASK      0xff
#define RADEON_SPLL_CNTL                    0x000c /* PLL */
#       define RADEON_SPLL_SLEEP            (1 << 0)
#       define RADEON_SPLL_RESET            (1 << 1)
#       define RADEON_SPLL_PCP_MASK         0x7
#       define RADEON_SPLL_PCP_SHIFT        8
#       define RADEON_SPLL_PVG_MASK         0x7
#       define RADEON_SPLL_PVG_SHIFT        11
#       define RADEON_SPLL_PDC_MASK         0x3
#       define RADEON_SPLL_PDC_SHIFT        14
#define RADEON_SCLK_CNTL                    0x000d /* PLL */
#       define RADEON_SCLK_SRC_SEL_MASK     0x0007
#       define RADEON_DYN_STOP_LAT_MASK     0x00007ff8
#       define RADEON_CP_MAX_DYN_STOP_LAT   0x0008
#       define RADEON_SCLK_FORCEON_MASK     0xffff8000
#       define RADEON_SCLK_FORCE_DISP2      (1<<15)
#       define RADEON_SCLK_FORCE_CP         (1<<16)
#       define RADEON_SCLK_FORCE_HDP        (1<<17)
#       define RADEON_SCLK_FORCE_DISP1      (1<<18)
#       define RADEON_SCLK_FORCE_TOP        (1<<19)
#       define RADEON_SCLK_FORCE_E2         (1<<20)
#       define RADEON_SCLK_FORCE_SE         (1<<21)
#       define RADEON_SCLK_FORCE_IDCT       (1<<22)
#       define RADEON_SCLK_FORCE_VIP        (1<<23)
#       define RADEON_SCLK_FORCE_RE         (1<<24)
#       define RADEON_SCLK_FORCE_PB         (1<<25)
#       define RADEON_SCLK_FORCE_TAM        (1<<26)
#       define RADEON_SCLK_FORCE_TDM        (1<<27)
#       define RADEON_SCLK_FORCE_RB         (1<<28)
#       define RADEON_SCLK_FORCE_TV_SCLK    (1<<29)
#       define RADEON_SCLK_FORCE_SUBPIC     (1<<30)
#       define RADEON_SCLK_FORCE_OV0        (1<<31)
#       define R300_SCLK_FORCE_VAP          (1<<21)
#       define R300_SCLK_FORCE_SR           (1<<25)
#       define R300_SCLK_FORCE_PX           (1<<26)
#       define R300_SCLK_FORCE_TX           (1<<27)
#       define R300_SCLK_FORCE_US           (1<<28)
#       define R300_SCLK_FORCE_SU           (1<<30)
#define R300_SCLK_CNTL2                     0x1e   /* PLL */
#       define R300_SCLK_TCL_MAX_DYN_STOP_LAT (1<<10)
#       define R300_SCLK_GA_MAX_DYN_STOP_LAT  (1<<11)
#       define R300_SCLK_CBA_MAX_DYN_STOP_LAT (1<<12)
#       define R300_SCLK_FORCE_TCL          (1<<13)
#       define R300_SCLK_FORCE_CBA          (1<<14)
#       define R300_SCLK_FORCE_GA           (1<<15)
#define RADEON_SCLK_MORE_CNTL               0x0035 /* PLL */
#       define RADEON_SCLK_MORE_MAX_DYN_STOP_LAT 0x0007
#       define RADEON_SCLK_MORE_FORCEON     0x0700
#define RADEON_SDRAM_MODE_REG               0x0158
#define RADEON_SEQ8_DATA                    0x03c5 /* VGA */
#define RADEON_SEQ8_IDX                     0x03c4 /* VGA */
#define RADEON_SNAPSHOT_F_COUNT             0x0244
#define RADEON_SNAPSHOT_VH_COUNTS           0x0240
#define RADEON_SNAPSHOT_VIF_COUNT           0x024c
#define RADEON_SRC_OFFSET                   0x15ac
#define RADEON_SRC_PITCH                    0x15b0
#define RADEON_SRC_PITCH_OFFSET             0x1428
#define RADEON_SRC_SC_BOTTOM                0x165c
#define RADEON_SRC_SC_BOTTOM_RIGHT          0x16f4
#define RADEON_SRC_SC_RIGHT                 0x1654
#define RADEON_SRC_X                        0x1414
#define RADEON_SRC_X_Y                      0x1590
#define RADEON_SRC_Y                        0x1418
#define RADEON_SRC_Y_X                      0x1434
#define RADEON_STATUS                       0x0f06 /* PCI */
#define RADEON_SUBPIC_CNTL                  0x0540 /* ? */
#define RADEON_SUB_CLASS                    0x0f0a /* PCI */
#define RADEON_SURFACE_CNTL                 0x0b00
#       define RADEON_SURF_TRANSLATION_DIS  (1 << 8)
#       define RADEON_NONSURF_AP0_SWP_16BPP (1 << 20)
#       define RADEON_NONSURF_AP0_SWP_32BPP (1 << 21)
#       define RADEON_NONSURF_AP1_SWP_16BPP (1 << 22)
#       define RADEON_NONSURF_AP1_SWP_32BPP (1 << 23)
#define RADEON_SURFACE0_INFO                0x0b0c
#       define RADEON_SURF_TILE_COLOR_MACRO (0 << 16)
#       define RADEON_SURF_TILE_COLOR_BOTH  (1 << 16)
#       define RADEON_SURF_TILE_DEPTH_32BPP (2 << 16)
#       define RADEON_SURF_TILE_DEPTH_16BPP (3 << 16)
#       define R200_SURF_TILE_NONE          (0 << 16)
#       define R200_SURF_TILE_COLOR_MACRO   (1 << 16)
#       define R200_SURF_TILE_COLOR_MICRO   (2 << 16)
#       define R200_SURF_TILE_COLOR_BOTH    (3 << 16)
#       define R200_SURF_TILE_DEPTH_32BPP   (4 << 16)
#       define R200_SURF_TILE_DEPTH_16BPP   (5 << 16)
#       define R300_SURF_TILE_NONE          (0 << 16)
#       define R300_SURF_TILE_COLOR_MACRO   (1 << 16)
#       define R300_SURF_TILE_DEPTH_32BPP   (2 << 16)
#       define RADEON_SURF_AP0_SWP_16BPP    (1 << 20)
#       define RADEON_SURF_AP0_SWP_32BPP    (1 << 21)
#       define RADEON_SURF_AP1_SWP_16BPP    (1 << 22)
#       define RADEON_SURF_AP1_SWP_32BPP    (1 << 23)
#define RADEON_SURFACE0_LOWER_BOUND         0x0b04
#define RADEON_SURFACE0_UPPER_BOUND         0x0b08
#define RADEON_SURFACE1_INFO                0x0b1c
#define RADEON_SURFACE1_LOWER_BOUND         0x0b14
#define RADEON_SURFACE1_UPPER_BOUND         0x0b18
#define RADEON_SURFACE2_INFO                0x0b2c
#define RADEON_SURFACE2_LOWER_BOUND         0x0b24
#define RADEON_SURFACE2_UPPER_BOUND         0x0b28
#define RADEON_SURFACE3_INFO                0x0b3c
#define RADEON_SURFACE3_LOWER_BOUND         0x0b34
#define RADEON_SURFACE3_UPPER_BOUND         0x0b38
#define RADEON_SURFACE4_INFO                0x0b4c
#define RADEON_SURFACE4_LOWER_BOUND         0x0b44
#define RADEON_SURFACE4_UPPER_BOUND         0x0b48
#define RADEON_SURFACE5_INFO                0x0b5c
#define RADEON_SURFACE5_LOWER_BOUND         0x0b54
#define RADEON_SURFACE5_UPPER_BOUND         0x0b58
#define RADEON_SURFACE6_INFO                0x0b6c
#define RADEON_SURFACE6_LOWER_BOUND         0x0b64
#define RADEON_SURFACE6_UPPER_BOUND         0x0b68
#define RADEON_SURFACE7_INFO                0x0b7c
#define RADEON_SURFACE7_LOWER_BOUND         0x0b74
#define RADEON_SURFACE7_UPPER_BOUND         0x0b78
#define RADEON_SW_SEMAPHORE                 0x013c

#define RADEON_TEST_DEBUG_CNTL              0x0120
#define RADEON_TEST_DEBUG_CNTL__TEST_DEBUG_OUT_EN 0x00000001

#define RADEON_TEST_DEBUG_MUX               0x0124
#define RADEON_TEST_DEBUG_OUT               0x012c
#define RADEON_TMDS_PLL_CNTL                0x02a8
#define RADEON_TMDS_TRANSMITTER_CNTL        0x02a4
#       define RADEON_TMDS_TRANSMITTER_PLLEN  1
#       define RADEON_TMDS_TRANSMITTER_PLLRST 2
#define RADEON_TRAIL_BRES_DEC               0x1614
#define RADEON_TRAIL_BRES_ERR               0x160c
#define RADEON_TRAIL_BRES_INC               0x1610
#define RADEON_TRAIL_X                      0x1618
#define RADEON_TRAIL_X_SUB                  0x1620

#define RADEON_VCLK_ECP_CNTL                0x0008 /* PLL */
#       define RADEON_VCLK_SRC_SEL_MASK     0x03
#       define RADEON_VCLK_SRC_SEL_CPUCLK   0x00
#       define RADEON_VCLK_SRC_SEL_PSCANCLK 0x01
#       define RADEON_VCLK_SRC_SEL_BYTECLK  0x02
#       define RADEON_VCLK_SRC_SEL_PPLLCLK  0x03
#       define RADEON_PIXCLK_ALWAYS_ONb     (1<<6)
#       define RADEON_PIXCLK_DAC_ALWAYS_ONb (1<<7)
#       define R300_DISP_DAC_PIXCLK_DAC_BLANK_OFF (1<<23)

#define RADEON_VENDOR_ID                    0x0f00 /* PCI */
#define RADEON_VGA_DDA_CONFIG               0x02e8
#define RADEON_VGA_DDA_ON_OFF               0x02ec
#define RADEON_VID_BUFFER_CONTROL           0x0900
#define RADEON_VIDEOMUX_CNTL                0x0190

/* VIP bus */
#define RADEON_VIPH_CH0_DATA                0x0c00
#define RADEON_VIPH_CH1_DATA                0x0c04
#define RADEON_VIPH_CH2_DATA                0x0c08
#define RADEON_VIPH_CH3_DATA                0x0c0c
#define RADEON_VIPH_CH0_ADDR                0x0c10
#define RADEON_VIPH_CH1_ADDR                0x0c14
#define RADEON_VIPH_CH2_ADDR                0x0c18
#define RADEON_VIPH_CH3_ADDR                0x0c1c
#define RADEON_VIPH_CH0_SBCNT               0x0c20
#define RADEON_VIPH_CH1_SBCNT               0x0c24
#define RADEON_VIPH_CH2_SBCNT               0x0c28
#define RADEON_VIPH_CH3_SBCNT               0x0c2c
#define RADEON_VIPH_CH0_ABCNT               0x0c30
#define RADEON_VIPH_CH1_ABCNT               0x0c34
#define RADEON_VIPH_CH2_ABCNT               0x0c38
#define RADEON_VIPH_CH3_ABCNT               0x0c3c
#define RADEON_VIPH_CONTROL                 0x0c40
#       define RADEON_VIP_BUSY 0
#       define RADEON_VIP_IDLE 1
#       define RADEON_VIP_RESET 2
#       define RADEON_VIPH_EN               (1 << 21)
#define RADEON_VIPH_DV_LAT                  0x0c44
#define RADEON_VIPH_BM_CHUNK                0x0c48
#define RADEON_VIPH_DV_INT                  0x0c4c
#define RADEON_VIPH_TIMEOUT_STAT            0x0c50
#define RADEON_VIPH_TIMEOUT_STAT__VIPH_REG_STAT 0x00000010
#define RADEON_VIPH_TIMEOUT_STAT__VIPH_REG_AK   0x00000010
#define RADEON_VIPH_TIMEOUT_STAT__VIPH_REGR_DIS 0x01000000

#define RADEON_VIPH_REG_DATA                0x0084
#define RADEON_VIPH_REG_ADDR                0x0080


#define RADEON_WAIT_UNTIL                   0x1720
#       define RADEON_WAIT_CRTC_PFLIP       (1 << 0)
#       define RADEON_WAIT_RE_CRTC_VLINE    (1 << 1)
#       define RADEON_WAIT_FE_CRTC_VLINE    (1 << 2)
#       define RADEON_WAIT_CRTC_VLINE       (1 << 3)
#       define RADEON_WAIT_DMA_VID_IDLE     (1 << 8)
#       define RADEON_WAIT_DMA_GUI_IDLE     (1 << 9)
#       define RADEON_WAIT_CMDFIFO          (1 << 10) /* wait for CMDFIFO_ENTRIES */
#       define RADEON_WAIT_OV0_FLIP         (1 << 11)
#       define RADEON_WAIT_AGP_FLUSH        (1 << 13)
#       define RADEON_WAIT_2D_IDLE          (1 << 14)
#       define RADEON_WAIT_3D_IDLE          (1 << 15)
#       define RADEON_WAIT_2D_IDLECLEAN     (1 << 16)
#       define RADEON_WAIT_3D_IDLECLEAN     (1 << 17)
#       define RADEON_WAIT_HOST_IDLECLEAN   (1 << 18)
#       define RADEON_CMDFIFO_ENTRIES_SHIFT 10
#       define RADEON_CMDFIFO_ENTRIES_MASK  0x7f
#       define RADEON_WAIT_VAP_IDLE         (1 << 28)
#       define RADEON_WAIT_BOTH_CRTC_PFLIP  (1 << 30)
#       define RADEON_ENG_DISPLAY_SELECT_CRTC0    (0 << 31)
#       define RADEON_ENG_DISPLAY_SELECT_CRTC1    (1 << 31)

#define RADEON_X_MPLL_REF_FB_DIV            0x000a /* PLL */
#define RADEON_XCLK_CNTL                    0x000d /* PLL */
#define RADEON_XDLL_CNTL                    0x000c /* PLL */
#define RADEON_XPLL_CNTL                    0x000b /* PLL */



				/* Registers for 3D/TCL */
#define RADEON_PP_BORDER_COLOR_0            0x1d40
#define RADEON_PP_BORDER_COLOR_1            0x1d44
#define RADEON_PP_BORDER_COLOR_2            0x1d48
#define RADEON_PP_CNTL                      0x1c38
#       define RADEON_STIPPLE_ENABLE        (1 <<  0)
#       define RADEON_SCISSOR_ENABLE        (1 <<  1)
#       define RADEON_PATTERN_ENABLE        (1 <<  2)
#       define RADEON_SHADOW_ENABLE         (1 <<  3)
#       define RADEON_TEX_ENABLE_MASK       (0xf << 4)
#       define RADEON_TEX_0_ENABLE          (1 <<  4)
#       define RADEON_TEX_1_ENABLE          (1 <<  5)
#       define RADEON_TEX_2_ENABLE          (1 <<  6)
#       define RADEON_TEX_3_ENABLE          (1 <<  7)
#       define RADEON_TEX_BLEND_ENABLE_MASK (0xf << 12)
#       define RADEON_TEX_BLEND_0_ENABLE    (1 << 12)
#       define RADEON_TEX_BLEND_1_ENABLE    (1 << 13)
#       define RADEON_TEX_BLEND_2_ENABLE    (1 << 14)
#       define RADEON_TEX_BLEND_3_ENABLE    (1 << 15)
#       define RADEON_PLANAR_YUV_ENABLE     (1 << 20)
#       define RADEON_SPECULAR_ENABLE       (1 << 21)
#       define RADEON_FOG_ENABLE            (1 << 22)
#       define RADEON_ALPHA_TEST_ENABLE     (1 << 23)
#       define RADEON_ANTI_ALIAS_NONE       (0 << 24)
#       define RADEON_ANTI_ALIAS_LINE       (1 << 24)
#       define RADEON_ANTI_ALIAS_POLY       (2 << 24)
#       define RADEON_ANTI_ALIAS_LINE_POLY  (3 << 24)
#       define RADEON_BUMP_MAP_ENABLE       (1 << 26)
#       define RADEON_BUMPED_MAP_T0         (0 << 27)
#       define RADEON_BUMPED_MAP_T1         (1 << 27)
#       define RADEON_BUMPED_MAP_T2         (2 << 27)
#       define RADEON_TEX_3D_ENABLE_0       (1 << 29)
#       define RADEON_TEX_3D_ENABLE_1       (1 << 30)
#       define RADEON_MC_ENABLE             (1 << 31)
#define RADEON_PP_FOG_COLOR                 0x1c18
#       define RADEON_FOG_COLOR_MASK        0x00ffffff
#       define RADEON_FOG_VERTEX            (0 << 24)
#       define RADEON_FOG_TABLE             (1 << 24)
#       define RADEON_FOG_USE_DEPTH         (0 << 25)
#       define RADEON_FOG_USE_DIFFUSE_ALPHA (2 << 25)
#       define RADEON_FOG_USE_SPEC_ALPHA    (3 << 25)
#define RADEON_PP_LUM_MATRIX                0x1d00
#define RADEON_PP_MISC                      0x1c14
#       define RADEON_REF_ALPHA_MASK        0x000000ff
#       define RADEON_ALPHA_TEST_FAIL       (0 << 8)
#       define RADEON_ALPHA_TEST_LESS       (1 << 8)
#       define RADEON_ALPHA_TEST_LEQUAL     (2 << 8)
#       define RADEON_ALPHA_TEST_EQUAL      (3 << 8)
#       define RADEON_ALPHA_TEST_GEQUAL     (4 << 8)
#       define RADEON_ALPHA_TEST_GREATER    (5 << 8)
#       define RADEON_ALPHA_TEST_NEQUAL     (6 << 8)
#       define RADEON_ALPHA_TEST_PASS       (7 << 8)
#       define RADEON_ALPHA_TEST_OP_MASK    (7 << 8)
#       define RADEON_CHROMA_FUNC_FAIL      (0 << 16)
#       define RADEON_CHROMA_FUNC_PASS      (1 << 16)
#       define RADEON_CHROMA_FUNC_NEQUAL    (2 << 16)
#       define RADEON_CHROMA_FUNC_EQUAL     (3 << 16)
#       define RADEON_CHROMA_KEY_NEAREST    (0 << 18)
#       define RADEON_CHROMA_KEY_ZERO       (1 << 18)
#       define RADEON_SHADOW_ID_AUTO_INC    (1 << 20)
#       define RADEON_SHADOW_FUNC_EQUAL     (0 << 21)
#       define RADEON_SHADOW_FUNC_NEQUAL    (1 << 21)
#       define RADEON_SHADOW_PASS_1         (0 << 22)
#       define RADEON_SHADOW_PASS_2         (1 << 22)
#       define RADEON_RIGHT_HAND_CUBE_D3D   (0 << 24)
#       define RADEON_RIGHT_HAND_CUBE_OGL   (1 << 24)
#define RADEON_PP_ROT_MATRIX_0              0x1d58
#define RADEON_PP_ROT_MATRIX_1              0x1d5c
#define RADEON_PP_TXFILTER_0                0x1c54
#define RADEON_PP_TXFILTER_1                0x1c6c
#define RADEON_PP_TXFILTER_2                0x1c84
#       define RADEON_MAG_FILTER_NEAREST                   (0  <<  0)
#       define RADEON_MAG_FILTER_LINEAR                    (1  <<  0)
#       define RADEON_MAG_FILTER_MASK                      (1  <<  0)
#       define RADEON_MIN_FILTER_NEAREST                   (0  <<  1)
#       define RADEON_MIN_FILTER_LINEAR                    (1  <<  1)
#       define RADEON_MIN_FILTER_NEAREST_MIP_NEAREST       (2  <<  1)
#       define RADEON_MIN_FILTER_NEAREST_MIP_LINEAR        (3  <<  1)
#       define RADEON_MIN_FILTER_LINEAR_MIP_NEAREST        (6  <<  1)
#       define RADEON_MIN_FILTER_LINEAR_MIP_LINEAR         (7  <<  1)
#       define RADEON_MIN_FILTER_ANISO_NEAREST             (8  <<  1)
#       define RADEON_MIN_FILTER_ANISO_LINEAR              (9  <<  1)
#       define RADEON_MIN_FILTER_ANISO_NEAREST_MIP_NEAREST (10 <<  1)
#       define RADEON_MIN_FILTER_ANISO_NEAREST_MIP_LINEAR  (11 <<  1)
#       define RADEON_MIN_FILTER_MASK                      (15 <<  1)
#       define RADEON_MAX_ANISO_1_TO_1                     (0  <<  5)
#       define RADEON_MAX_ANISO_2_TO_1                     (1  <<  5)
#       define RADEON_MAX_ANISO_4_TO_1                     (2  <<  5)
#       define RADEON_MAX_ANISO_8_TO_1                     (3  <<  5)
#       define RADEON_MAX_ANISO_16_TO_1                    (4  <<  5)
#       define RADEON_MAX_ANISO_MASK                       (7  <<  5)
#       define RADEON_LOD_BIAS_MASK                        (0xff <<  8)
#       define RADEON_LOD_BIAS_SHIFT                       8
#       define RADEON_MAX_MIP_LEVEL_MASK                   (0x0f << 16)
#       define RADEON_MAX_MIP_LEVEL_SHIFT                  16
#       define RADEON_YUV_TO_RGB                           (1  << 20)
#       define RADEON_YUV_TEMPERATURE_COOL                 (0  << 21)
#       define RADEON_YUV_TEMPERATURE_HOT                  (1  << 21)
#       define RADEON_YUV_TEMPERATURE_MASK                 (1  << 21)
#       define RADEON_WRAPEN_S                             (1  << 22)
#       define RADEON_CLAMP_S_WRAP                         (0  << 23)
#       define RADEON_CLAMP_S_MIRROR                       (1  << 23)
#       define RADEON_CLAMP_S_CLAMP_LAST                   (2  << 23)
#       define RADEON_CLAMP_S_MIRROR_CLAMP_LAST            (3  << 23)
#       define RADEON_CLAMP_S_CLAMP_BORDER                 (4  << 23)
#       define RADEON_CLAMP_S_MIRROR_CLAMP_BORDER          (5  << 23)
#       define RADEON_CLAMP_S_CLAMP_GL                     (6  << 23)
#       define RADEON_CLAMP_S_MIRROR_CLAMP_GL              (7  << 23)
#       define RADEON_CLAMP_S_MASK                         (7  << 23)
#       define RADEON_WRAPEN_T                             (1  << 26)
#       define RADEON_CLAMP_T_WRAP                         (0  << 27)
#       define RADEON_CLAMP_T_MIRROR                       (1  << 27)
#       define RADEON_CLAMP_T_CLAMP_LAST                   (2  << 27)
#       define RADEON_CLAMP_T_MIRROR_CLAMP_LAST            (3  << 27)
#       define RADEON_CLAMP_T_CLAMP_BORDER                 (4  << 27)
#       define RADEON_CLAMP_T_MIRROR_CLAMP_BORDER          (5  << 27)
#       define RADEON_CLAMP_T_CLAMP_GL                     (6  << 27)
#       define RADEON_CLAMP_T_MIRROR_CLAMP_GL              (7  << 27)
#       define RADEON_CLAMP_T_MASK                         (7  << 27)
#       define RADEON_BORDER_MODE_OGL                      (0  << 31)
#       define RADEON_BORDER_MODE_D3D                      (1  << 31)
#define RADEON_PP_TXFORMAT_0                0x1c58
#define RADEON_PP_TXFORMAT_1                0x1c70
#define RADEON_PP_TXFORMAT_2                0x1c88
#       define RADEON_TXFORMAT_I8                 (0  <<  0)
#       define RADEON_TXFORMAT_AI88               (1  <<  0)
#       define RADEON_TXFORMAT_RGB332             (2  <<  0)
#       define RADEON_TXFORMAT_ARGB1555           (3  <<  0)
#       define RADEON_TXFORMAT_RGB565             (4  <<  0)
#       define RADEON_TXFORMAT_ARGB4444           (5  <<  0)
#       define RADEON_TXFORMAT_ARGB8888           (6  <<  0)
#       define RADEON_TXFORMAT_RGBA8888           (7  <<  0)
#       define RADEON_TXFORMAT_Y8                 (8  <<  0)
#       define RADEON_TXFORMAT_VYUY422            (10 <<  0)
#       define RADEON_TXFORMAT_YVYU422            (11 <<  0)
#       define RADEON_TXFORMAT_DXT1               (12 <<  0)
#  /*
 *define RADEON_TXFORMAT_DXT23/*
 * nc., Mar(14 <<  0)
#nc., MaCopyright 2000 ATI Technol45 Inc., Markarkha5, Ontario	Copyright 2000 ATI TechSHADOW16 Inc., Markha6nt, California.
 *
 * All Rights Reserv32 Inc., Markha7, Ontario, and
 *                VA LinuUDV88tems Inc., Frem8, Ontario, and
 *                VA LinL ass65ystems Inc.,(19ion files (the
 * "Software"), to deal in tUV88ociated doc(20, Ontario, and
 *                VA LinTI TechMASKthe right31to use, copy, modify, merge,
 * publish, distrSHIFTnc., Ma0io, and
 *                VA LinAPPLE_YUV_MODE., Frem , Ont5rio, and
 *                VA LinALPHA_IN_MAPare withosubjec6rio, and
 *                VA LinNON_POWERto any perght noti7rio, and
 *                VA LinWIDTHribute, subliremont, C8) shall be included in all copies or su to permit p 8io, and
 *                VA LinHEIGHtribute, sublicmont, 12rio, and
 *                VA Lin,
 * EX to permit p12 copy, modify, merge,
 * publish,5s or substantial
LIED, INce and this permission notice (incAR PURPOS to permit16EMENT.  IN NO EVENT SHALL ATI, VA L,
 * EXPRESS OR LIED, I2e, copy, modify, merge,
 * publish,R ANY CLA to permi2ersons to whom the Software is fuST_ROUTE_STQ0c., Fre0subje24rio, and
 *                VA Lin OUT OF OE AND
 * NO3ION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALSTQ1 next paragraARE.
 */

/*
 * Authors:
 *   Kevin E. Martin <to any (2ree86.org>
 *   Rickard E. Faith <faith@ENDIAN_NO_SWve copyCTION WIce and this permission notice (inc
 *
 * 16BPPferencexfree86.!! FIXME !!!!
 *   RAGE 128 VR/ RAGE 128 32 Register Ran Hour!! FIXME !!!!
 *   RAGE 128 VR/ RAGE 128 HALFDWferencE SOFTWAce and this permission notice (inc * Theibut_ENABLEr Referenc of the Software.
 *
 * THE SOFTWACHROMA_KEYual (Technical Re9erence Manual P/N
 *   SDK-G04000 UBICboveual (Tech Referen3ario, and
 *                VA LinPERSPECTIVEual (Tec FILE HA1)
#Copyright 2000PPXME !!!FACES_N CONNEermit pex1d24REGISTER DEFINITIONS THAT ARE Nmartin@ECT
 * ON TH8REGISTER DEFINITIONS THAT ARE Nto any perso* ON THcio, and
 *              AREs or su1 PROVIDED "AS  persons to whom the Softwude ",
 * EXeg.h"
#include "4lude "r300_reg.h"
#include "r500_regbstantial
 * :
 *xf, Onario, and
 *             EON_MC_AGP_LOC000FFFF
#defin		RADEONTH THE SOFTWARE OR THE USude "r500_r2g.h"
#include "rIS", WITHOUT WARRANTY OFEON_MC_AGP_LT		16
#define RAANTABILITY, FITNESS FOR AGP_TOP_SHIFT	000FFFF
#define		RADEON of the Software.
 *
 * T48
#define		RAC_AGP_TOP_MASK		0xFFFFNCLUDING BUT NOT LIMITED Tude "r500_r3g.h"
#include "r/OR
 * THEIR SUPPLIERS BEEON_MC_AGP_L2		0x015c /* r20ORT OR OTHERWISE, ARISINGN_AGP_BASE_2	000FFFF
#define		RADEONINGEMENT.  IN NO EVENT SHAAGP_BASE			0x0C_AGP_TOP_MASK		0xFFFFTHER LIABILITY,
 * WHETHERude "r500_r4g.h"
#include "rHE RYPE_CI8			2
#define ATI_DATATYPE_TYPE_RGB888			5
/
#iTYPE_RGB565			4
#define ATI_DATATY000FFFF
#define		RADEONITH THE SOFTWARE OR THE US888			6
#definGB1555			3
#define ATI_8)
REGISTER DEFINITIONTXOFFSETNOT CORRECT
 * _H_

#c5c_422			11
#define ATI_DATATYCODE IS NEEDED12
#def7E RADEON.  A FULL AUTI_DATATY_RADEON_REG_H_12
#def8clude "r300_reg.h"
#inclTXO>
 *
 * References:
 *
EON_MC_AGP_START_SHIFT	0
#deTER_ID     BYrtinrences(nse, 2c /* PCI */
#define RADEON_AGP_BASEWORD         20x0f2c /* PCI */
#define RADEON_AGP_BASE99.
 *
 * !!!!       0x0170
#define RADEON_AGP_MACRO_LINEAROP_MASK		TYPE_0 << 0)
#       define RADEON_AGP_TI THIS next par  (0x20 << 0)
#       define RADEIAGP_APER_SIZE_128MB   (3(0x30 << 0)
#       define RADEON_P_AP_X
 *   AlMB      (0x38 << 0)
#       define RADEON_AGPOPpermit         (0x38 << 0)
#       define_DATATYibute, subli0xfSIZE_eersons to whom the Softwardefine RA to permit p5Y_422			11
#define AME !!!_DATATYT0NOT CORRECTON Td0  /* bits [31:5] */SIZE_MASK    (0x3f << 0)
#define RACODE IS NES_PCIE RADEON.  A FULL AUDIT OF#define RAto any perS_PCI
#ifndef _RADEON_REG_H_
#d#define RAgies Inc.,S_PCIne ATI_DATATYPE_AYUVI config*/
#    4IS NEEDED!  *MB  4 /* offset in PCI config*/
#  1ADEON_STATUS_Pe0its of CAP_PTR */
#       define RAD             e0100000
#define RADEON_CAPABILITIES1PTR_PCI_CONFIGe00x34 /* offset in PCI config*/
#  1    define RADe0N_CAP_PTR_MASK          0xfc /* ma1k off reservede1its of CAP_PTR */
#       define R2DEON_CAP_ID_NUL1100000
#define RADEON_CAPABILITIES2y list */
#    10x34 /* offset in PCI config*/
#  2   0x02 /* AGP 1N_CAP_PTR_MASK          0xfc /* ma2ON_CAP_ID_EXP  ORT        (1<<8)
#define RADEON_AGP_ine RADEON_AGP24Y_422			11
#define ATEX_SIZEYPE_YVYU_422			12
#ded04CONFINPOT  0x06
#       define x0f5c /* 
#define ATI_DATATYPd          0x10 /* PCIx0f5c /* rs for 2D/Video/OverdRADE, and
 *              EX_Uc /* ibute, sublic0x7fADEON_MC_AGP_START_SHIFT	0
#de  define R to permit persons to whom the SoftwaEX_Vine RADEON_AGP_FW_MODE     INGEMENT.  IN NO EVENT SHALne RADEON OF
 * MERCHAOR
 * THEIR SUPPLIERS BESIGNED_RGBRADEON_AGP_FZE_16MBario, and
 *             EON_AGPv3_8 to permit 3ersons to whom the SoftwEON_AGPopment Man         0x0D RE1 /* VGA */
#define RADEON_ATTRDW  to permi31ODE           0x02
#   PITCHYPE_YVYU_422			12P_2X_8ADEON_AGP_1X_MODE           0x01
# 0 /* V
#define ATI_DAT4X_MO_CONFI_CNTL                  0x1660
#     rs for 2D/Video/4X_MOAUX_SC_CNTL   /* note:IG    13-5: 32 byte aligned stride of texture map  0xRGB4444			15

				/* CBLENDYPE_YVYU_422			12
#def6          0x000b /* 2_SC_EN  
#define ATI_DATATYPE_
#ifndef _RADEON_REG2_SC_EN  rs for 2D/Video/Overl9ersons to whom the SoftwCOLOR_ARG_N_ATTRX    rmit persons to whom the SoftwUX3_SC_EN   000FFFF
#define		R1ADEON_MC_AGP_START_SHIFT	0
#deUX3_SC_EN   ZERO Inc., MarkhN CON       define RADEON_AUX3_SC_MODE_NAND  CURRENTC_MODE04100- RADEON_AUX1_SC_BOTTOM               0x1670
#defin * ThTHE SO RADEON_AUX1_SC_BOTTOM               0x167DIFFUSEne RADEON off                0x1668
#define RADEON_AUX1_SC_TOPUX1_SC_Rystem       define RADEON_AUX3_SC_MODE_NAND  ND CULARne RADE(d.
 *0
#define RADEON_AUX2_SC_LEFT                 0x1UX1_SC(7GHT                0x1668
#define RADEON_AUXTFACTOx1674
#d (ciate2_SC_TOP                  0x167c
#define RADEON_UX1_SC_R9_AUX2_SC_TOP                  0x167c
#define 0ne RADEO., FremN CO      0x1684
#define RADEON_AUX3_SC_RIGHT  UX1_SC_., Fremmart2_SC_TOP                  0x167c
#define 1               _AUX RADEON_AUX_WINDOW_HORZ_CNTL         0x02d8    0x168c
#deIGHT2_SC_TOP                  0x167c
#define 2                off                  0x0f0b
#define RADEON_BIO    0x168c
#deyste2_SC_TOP                  0x167c
#define 3               d.
  << 16)
#       define RADEON_FP_PANEL_SCAL    0x168c
#deN_AU       define RADEON_AUX3_SC_MODE_NAND            0_APER_ne RADEON_DRIVER_BRIGHTNESS_EN  (1E_OR       (0 << 5)
#   t to the following conditTNESS_EN  (1   (1 << 5)
#define RADEe RADEON_DISPLAY_ROT_00        (0 << 0
#define RADEON_AUX1_SCN_DISPLAY_ROT_90        (1 << 28)
#       defUX1_SC_RIGHT    e RADEON_DISPLAY_ROT_00        (0 << 1_SC_TOP                270       (3 << 28)
#define RADEON_BIOS_1_SCR        0x1680
#e RADEON_DISPLAY_ROT_00        (0 <<       0x1674
#define RAD#define RADEON_BIOS_3_SCRATCH               0xe RADEON_AUX2_Se RADEON_DISPLAY_ROT_00        (0 <<  RADEON_AUX3_SC_BOTTOM  N_CRT1_ATTACHED_MASK    (3 << 0)
#       defiFT              N_CRT1_ATTACHED_MASK    (3 << 0)
#                    0x1688
COLOR   (2 << 0)
#       define RADEON_L    0x168c
#define RAN_CRT1_ATTACHED_MASK    (3 << 0)
#    d8
#define RADEON_AUX_W
#       define RADEON_TV1_ATTACHED_MASKRADEON_BASE_CODE     N_CRT1_ATTACHED_MASK    (3 << 0)
#    IOS_0_SCRATCH          RADEON_TV1_ATTACHED_SVIDEO   (2 << 4)
# EL_SCALABLE     (1 <<N_CRT1_ATTACHED_MASK    (3 << 0)
#    ALE_EN     (1 << 17)
# CHED_MONO    (1 << 8)
#       define RAD  (1 << 18)
#       de RADEON_DISPLAY_ROT_00        (0 <C		0x015c /* r200+4)
#       define RADEON_AUX3_SC_M!!! OR       (0 << 5)
#   1    define RADEON_AUX3_SC_MODE_NANDC 28)
#       define RADEO      define RADEON_CRT1_ON           0
#define RADEON_AUX1_SCine RADEON_TV1_ON                (1 << 2)
#   UX1_SC_RIGHT          define RADEON_CRT1_ON           1_SC_TOP                               (1 << 5)
#       define RADEON_        0x1680
#      define RADEON_CRT1_ON                 0x1674
#define RAD<< 7)
#       define RADEON_LCD1_CRTC_MASK     e RADEON_AUX2_S      define RADEON_CRT1_ON            RADEON_AUX3_SC_BOTTOM  ON_CRT1_CRTC_MASK        (1 << 9)
#       defiFT              ON_CRT1_CRTC_MASK        (1 << 9)
#                    0x1688
(1 << 10)
#       define RADEON_TV1_CRTC_    0x168c
#define RAON_CRT1_CRTC_MASK        (1 << 9)
#    d8
#define RADEON_AUX_We RADEON_DFP1_CRTC_SHIFT       11
#      RADEON_BASE_CODE     ON_CRT1_CRTC_MASK        (1 << 9)
#    IOS_0_SCRATCH          T       12
#       define RADEON_CV1_CRTCEL_SCALABLE     (1 <<ON_CRT1_CRTC_MASK        (1 << 9)
#    ALE_EN     (1 << 17)
# ne RADEON_DFP2_CRTC_MASK        (1 << 14)  (1 << 18)
#       d      define RADEON_CRT1_ON MPC_EN  s for 2D/Video/O     0x1e RADEON_DISPLAY_ROT_00    efine RA1 << 26)
#       1 define RADEON_DISPLAY_ROTefine RBDEON_ACC_REQ_CRT1       ce and this permission nofine RADEOACC_REQ_TV1       OR
 * THEIR SUPPLIERS BEfine RADECDEON_ACC_REQ_CRT1       h) shall be included in ae RADEON_AACC_REQ_TV1       7define RADEON_DISPLAY_RC_EN  CTL              (1 EON_     of the Software.
 *
 * TIOS_6_SCRAADDstems Inc., FreB   (8
#       define RADEON_ACC_MODE_CHASUBTRAC26)
#            
#       define RADEON_ACC_MODE_CHANGEEON_AGthe rightsx0028
#       define RADEON_ACC_MODE_CHAC_EN                (0xfine RADEON_LCD_DPMS_ON           (1DOTgies Inc., Mar(m, Onfine RADEON_LCD_DPMS_ON   SCAL4X_MODE       _DATATYPE_1MS_ON           (1 << 23)
#  TCH              fine RADEO2      0x03c0 /* VGA */
#de)
#  1XDEON_ACC_REQ_CRT1 2)
#   N               (0 << 24)
#     2 define RADEON_DPMS_SMB                   (0 << 24)
#     4 define RADEON_DPMS_Sfine       (2 << 24)
#       deCLAMON_Ae RADEON_DPMS_SUSPEND        (0x38 << 0)
#       def0_EQ_TCU        DPMS_SUSPEND     TH THE SOFTWARE OR THE USE1DRIVER_CRITICAL       (1 << 27)
t to the following conditi2DRIVER_CRITICAL       (1 << 27)
ce and this permission not3DRIVER_CRITICAL       (1 << 27)
<< 22)
#       define RADEO3_SC_EN TCH               5)
 22)
#       define RADEON_ACC_R     define RADEON        define RADEON_A (TeN            (1 << 2)
#  ARGB4444			15

				/*        
#define ATI_DATATYPE_ne ATI_DATATYPE_AYUV_4       MODE_NAND     (1 << 3)DE           0x04
#      * The_EN            (1 << 4)
#       define RADEONA10         000FFFF
#define		RADEON_MC_AGP_START_SHIFT	0
#deA10            (1 << 5)
#define ne RADEON_BRUSH_DATA12                 0   define RADEONfine RADEON_AUX_WINDOW_HORZ_CNTLA10         FP2_ON          N_AUX_WINDOW_VERT_CNTL         0A10            8
#       defDE                    0x0f0b
#deA10         N_TV1_CRTC_MASK             0x0010
#       definSH_DATA17    (1 << 11)
#      (1 << 16)
#       define RADEOSH_DATA17    e RADEON_CRT2_C 17)
#       define RADEON_FP_CHSH_DATA17            13
#         define RADEON_DRIVER_BRIGHSH_DATA17    ne RADEON_ACC_Rciat0                 0x14d0
#define RADEOC_REQ_CRT2       #define RADEON_BRUSH_DATA10          (3 << 28)
#      xFFFF0000
#define		RADEON_MC_AON_BRUSH_DATx14b0
#define RADEON_BR4dc
#define RADEON_BRUSH_DATA24      fine RADEON_BRUSH_DATA14dc
#define RADEON_BRUSH_DATA24      ON_BRUSH_DATA15        4dc
#define RADEON_BRUSH_DATA24      DATA16                 4dc
#define RADEON_BRUSH_DATA24                    0x14c4
#d            0x14f0
#define RADEON_BRUS    0x14c8
#define RAD            0x14f0
#define RADEON_BRUSc
#define RADEON_BRUSH            0x14f0
#define RADEON_BRUS RADEON_BRUSH_DATA20              0x14f0
#define RADEON_BRUSRUSH_DATA21           4dc
#define RADEON_BRUSH_DATA24    EQ_DFP2          RGB332			7
#define ATI_D#define RADE_FB_START_SHIFT	0
#define		RADEON_MC_FB_TOP_MASK#define RADEx14b0
#define RADEON_BR         0x150c
#define RADEON_BRUSH_fine RADEON_BRUSH_DATA1         0x150c
#define RADEON_BRUSH_ON_BRUSH_DATA15                 0x150c
#define RADEON_BRUSH_DATA16                          0x150c
#define RADEON_BRUSH_              0x14c4
#d_DATA4                  0x1490
#define    0x14c8
#define RAD_DATA4                  0x1490
#definec
#define RADEON_BRUSH_DATA4                  0x1490
#define RADEON_BRUSH_DATA20  _DATA4                  0x1490
#defineRUSH_DATA21            of the Software.
 *
 * TDOelopmentDONT_REPLICATTHIS FIL    es: June 1999.
 *
 */

/*A10       
#define RADEON_BIOf   define RADEON_AUX2RADEON_PE_YVYU_422			12
2)
#         define RADEON_RADEON_
#define ATI_DATAOverla     define RADEON_AURADEON_rs for 2D/Video/O << 3)8   define RADEON_RB3D21)
# CNTL for 2D/Video/OverlORT OR OTHERWISE, ARISINGCOMB_FCabov     define RADEON_ THE SOFTWNCLUDING BUT NOT LIMITED Tdefine RAADD RADEOne RADEON_DPMS_STA       0x154c
#define RADEON_BRUSH_DATA52  NO               0x1ght not   0x154c
#define RADEON_BRUSH_DATASUB                0x1550an Hou_DATA54                 0x1558
#define 0x1554
#define RADEON         0x154c
#define RADEON_BSRCCC_MODEGL_DATA36                fine Rce and this permission no57           ONPER_SIZE_6
#define RRADEONBRUSH_DATA58                 0x1568
#d57                    N_DFP_BRUSH_DATA58                 0x1568
#defi_MINUSRUSH_DATA6 (3-INFRINGEMENT.  IN NO EVENT SHA57           DSine RADEON_BRUSH_DATssion0x1498
#define RADEON_BRUSH_DATA60               574
#defin(30x0028              0x156c
#define RADEON_BRUSH_    0x168c
#      tion 0x1498
#define RADEON_BRUSH_DATA60                 0x        ut reRADEON_BRUSH_DATA61                 0x1574
              0x14
#    A62                 0x1578
#define RADEON_BRUSH_DATAe RADEO4      (1 << 19)
#       define N_BRUSH_DATA7         _SATURADEON(4ADEON_BRUSH_DATA58                 0x1568DEON_BRUSH_DATA51      (6A59                 0x156c
#def574
               0x1564
#define RADEON
#       define RADEON_DIS< 12)
#	definefine RADEON_BRUSH_DATA59   (1 << 14)
#	define RS600_MSI_REARM		  USH_DATA6                   (1 << 14)
#	define RS600_MSI_REARM		              0x1570
#define  (1 << 14)
#	define RS600_MSI_REARM		  574
#define RADEON_BRUSH_DA       define RADEON_BUS_RD_ABORT_EN        (1 <<DATA63           /rs740 */
#       define RADEON_BUS_RD_DISC              0x149c
#d       define RADEON_BUS_RD_ABORT_EN        (1 << 25)ine RADEON_BRne RADEON_BUS_MSTR_DISCONNECT_EN (1 << 28)
ADEON_BRUSH_SCALE      US_WRT_BURST          (1 << 29)
#       define RADEON          0x1 (1 << 14)
#	define RS600_MSI_REARM	 << 6)
#       define RADEON_B
#                0x1494ine RADEON_BRUSH_DAdeo/Overl3clude "r300_reg.h"
#incl * The      OTE: THISpyright notiario, and
 *             PLA    Manual (Tech/* PCIE */
#        0x03c0 /* VGA */
#dDITHER   0xa2 /* PCI* PCIE */
#  0x20 << 0)
#       defineROU     0xa2 /* PCIx7
#       def   (0x38 << 0)
#       de#     K_WIDTH_MASK      0ght noti(1 << 14)
#	define RS600_M_WIDTHINI define RADEON_* subject to the following conditRO NOTE: THISAL       (1 <<  notice and this permission noSTENCILX0        0
#     paragraph) shall be included in aZX0        0
#       ON_PCIE_LC_L_BRUSH_DATA45             EPTHXYfig*/
# H_SHIFT   CIE_LC_Les: June 1999.
 *
 */

/*      (1 <<
 * and to per10
define RADEON_DISPLAY_ROT_MASe DeveloRGB15 Softw3SK   0x70
#       define RADEON_PCIERGB56ystems DE           0x04
#     CIE_LC_LINK_W_LC_n on the (1 << 21)
#       define     define RAD3 to any  << 23)
#define RADEON_BNFIG_EN    (1Yciated docuRGB332			7
#define ATI_DNFIG_EN    (1 <<ciated do9ine RADEON_CACHE_CNTL                UV422_VYUY 1S_MASK             (3 <<N_CAPABILITIES_ID   YVYUART_MASK		0x0000FFFF
#defiNFIG_EN    (1aS_ID4 off rODE           0x04
#     (1 << 9)
#       4RADEON_1R_SI< 24)
#       define RARCMP_FLI#       define N_BRUSH_PCIE_DATA               FIG__DATATADEON_BRUSH_DATA544)
#       define RADEON_AUX3fine RADEON_AGP_AR_SIZE_4f          0x000b N_START_CN0 /* RADEON_BRUSH_DATA54define RADEON_CACHE_LINE     0 /* VDEON_BRUSH_DA0x0fine1ffdefine RADEON_CACHE_LINE      N_AGP (1 << 4)
#         (1 << 19)
#       define RA3_SCADEON< 8)
#           (1 << 22)
#       define RADEO3_SC
 *
 * References:)
#       define RADEON_EXT_DESKx0014
#                     define RADEON_LCD_DPMS_ON   x0014
#      D          efine RADEOLOCK_CNTL_INDEX    e RADTL   (1 << 15)
#define HE RADEON.  A FULL#       de    0x0008
#       defi_RGB332			7
#define ATI_D       def<< 7)
#       defineRADEON_PLL_DIV_SEL         e RAD
#       define RADEON_ENGIN_DYNCLK_MODE     (1 <<      define Refine RADEON_ACTIVE_HILO_LAT_MASK  (3 << 13     define Rine RADEON_ACTIVE_HILO_LAT_SHIFT 13
#    C_LINDEON_BRUSH_DATA51  4X_M8P_LAT_MASK (1 << 12)
#ROP      0x0034 */
#defiPWRMGersons to whom the Softw3
# DEON_BRUSH_DATA51 LIED, I of the Software.
 *
 * T3
# CLR_SIZE_128 CONNECTION WL              0x0015 /* PLL Nfine RADEON_BON_PCIE_LC_L              0x0015 /* PLL AND_INVERT
#             L              0x0015 /* PLL *OPYfine RADEON_CL 0x1560_CLR_3D               0x1a24
#defREVERS< 4)
#   4CLR_SRC              0x15c4
#definine RAAL       (1 <5CLR_SRC              0x15c4
#definX)
#define RADEON_C6DEON_TCL_BYPASS_DISABLE    (1 << 20A#       defineH_DATAR_SRC              0x15c4
#define Rdefine RADEON_PC8_SRC_SOURCE    (1 << 24)
#define REQUIVfine RADEON_PC9DEON_TCL_BYPASS_DISABLE    (1 << 20)O           0x15  0xR_SRC              0x15c4
#definO2
# e RADEON_CLR#defi_CLR_DST              0x15c8
#defineefine RADEON_PCIfine         0x1A28
#define RADEON_CODEON_CLR_CMP_C_CODEOW_ID          0x1a0c
#define RADEEON_ACC_REQ_CRT1  m, OnL              0x0015 /* PLL   (1 << 15)
#defWRMGT_CNTL   AT_MASK (1 << 12)
#     deREFDEON_BRUSH_DAT4X_M
#deWIDTH_X8        4
#       defREFGP_MODE_MASK         0x17
#       defCONFIG_CNTL ADEON_AGP_FW_MOE           0x10
#       define     def_WIDT to permit   0x01
#       define RADE    defVALUALINGS IN THE       (            0x1470
#define     defWRITdefinATI_REVdefine ATI_DATATYPE_ARGB8fine RADEON_CN_CFG_ATI_REV_A13   _PCIE_DATA              Zfine RAine RADEON_BRUSHBUSY   0x00e8
#define RADEON_     d, distribute, subline		RADEON_MC_AGP_START_SHIFT	0
#deEON_CONFIG_RE16BIT_INT_Zs:
 *
 * !x010c
#define RADEON_CONFIG_REG_APER_SI24         0x01an Houfine RADEON_CONFIG_XSTRAP                0x00FLOA   0 0x1560x010c
#define RADEON_CONFIG_REG_APER_SI32         0x01TL    _CONSTANT_COLOR_MASK   0x00ffffff
#       def  define P_EQ_Cx010c
#define RADEON_CONFIG_REG_APER_SIZE      defiWCMP_SRC_ON_CONSTANT_COLOR_C             0x1d34
#       defiW_MSK    TANT_COLOR_ONE    0x00ffffff
#       define RADEON_W              define RADEON_PCIE_LZ_TEST_NEON_define RADEON_PC10
#defTH THE SOFTWARE OR THE USART_REQLESSEON_ACC_REQ_CRT1          define RADEON_GRPH_START_REQ_SHIQUA RADEON_BRUSH_Dne RADEO  define RADEON_GRPH_START_REQ_S     (0x7f<<8)
#   e RADEON_  define RADEON_GRPH_START_REQ_SG     (0x7f<<8)
#    N_CONSTON_GRPH_CRITICAL_POINT_MASK     (REATSK          (0xNSTANT_C  define RADEON_GRPH_START_REQ_SN     (0x7f<<8)
#    P_NEQ_GRPH_CRITICAL_CNTL           (1<<ALWAYT         0
#  DR        define RADEON_GRPH_START_REQ_SDEON_BRUSH_DATA51   DEON_GRPH_CRITICAL_AT_SOF        fine RADT_REQ_MASK         0
#define RADEON_BRUSH_DATA53    (1<<31)
#defiHIFT         0N_BRUSH_DATA54                 0x       define R    (0x7f<<8                0x155c
#define RA(1<<31)
#defi     (0x7f<<8) 0x1560
#define RADEON_BRUSH_DATA51<<31)
#defi(0x7f<<16)
#  efine R        (0x7f<<8)
#       define RADEON_ 16
#       dNSTANT__SHIFT         0
#       define RADEON_28)
#       deADEON_G_SHIFT         0
#       define RADEON_9)
#       defMP_SRC__SHIFT         0
#       define RADEON_G_1_BASE         0x0028SHIFT         0
#       define RAFAIL_KEE           0
#definINK_WIDTH_X8        4
#       defITICA   (1 << 5)
#dN_BRUSH_)
#       define RADEON_GRPH2_STOP_CNTfineAC< 4)
#          )
#       define RADEON_GRPH2_STOP_CNTINACC_REQ_TV2  0x1560
)
#       define RADEON_GRPH2_STOP_CNTDE54
#       de_SHIFT RADEON_CRTC_EXT_CNTL                0x00ine RADEON_S_MASK   )
#       define RADEON_GRPH2_STOP_CNTR_SIZE            (1<<2INK_WIDTH_X8        4
#       defZPASSAL_AT_SOF     CTION WI0   << 16)
#       define RADEON_RADEON   (1 << 5)
#xfree86.  (1 <<  9)
#       define RADEON_CRTC_       0x0214100-C Re  (1 <<  9)
#       define RADEON_CRTC_0054
#       E SOFTWA  (1 <<  9)
#       define RADEON_CRTC_       defineRADEON_
#       define RADEON_CRTC_CRT_ON        ine RADEON_NSTANT_ne RADEON_CRTC_EXT_CNTL_DPMS_BYTE      0bstantial
 * p   (1<<ne RADEON_CRTC_EXT_CNTL_DPMS_BYTE  ITICAL_AT_SOF     CTION WITH THE SOFTWARE OR THE US
#define RADEODISPLAY_DIS      (1 <          0x0050
#       define RADEON_RISTAT     (1 << 11)
          0x0050
#       define RADEON_     (1 << 15)
#defin          0x0050
#       define RADEON_x0055
#       define   1)
#       define RADEON_CRTC_CSYNC_EN       define RADEON          0x0050
#       define RADEON_  define RADEON_CRTC_D  define RADEON_GRPH_START_fineRESSION   0xa2 /* PCIE */
# eference Manual P/N
 *   SFORCE_Z_DIRTDEON_COMPOSITE_SHnologies: June 1999.
 *
 */

/*ZDEON_CO0        0
#       define 02
#d    0x0015 /* PE_APER_PATTERNRADEON_BRUSH_DATA5dersons to whom the SoftwN           e RADEON_BRUSH_DATA4fineADEO  define RADEON_CRTC_DISP_REREPE000 OUNES OF
 * MERCHAOR
 * THEIR SUPPLIERS BESP_REQ_EN_B  STARRADEON_AGP_APdefine ATI_DATATYPE_ARGB8SP_REQ_EN_B  LITTLE_    ORDER8MB   (0 of the Software.
 *
 * TSP_REQ_EN_B  BIGdefine RADE       deC2_SYNC_TRISTAT    (1 <<  4)
#       deAUTO_RE  (1 << 15ND     es: ine RADEON_CRTC_EN    STADEON_   (1 << 25)
#    ON_CRTC2_INTERLACE_EN    (1 <0
#definPTRATI_REV_A   define RADEON_CRTC_DISP_RE0
#define        def
#ifndef _RADEON_R   (SACC_REQ_TV2       deo/Ove26cD_MASK   (0xf << 16)
#defiInisheCO    DEON_BRUSH_IOS_ROM                        defiXefine RADEON_AGP     0x00e0
#       define    define RADEDEON_BRU< 5)
#       define RADEON_AUX3_SC0)
#    ON_PCIE_L to permRGB332			7
#define ATI_D      define RADE RADEON_CRTC2_DISDPMS_ON           (1 << 23)
#          define RADEON_CRTS        (1 <<  8)
#       d  defifine RADEON_CRTC2_HSYNCS    K  (0xf << 8)
#  SOLITART_CNN_CRTC2_CRT2_ON               (1<<8)
RE_TOP_LE  define RADEON_DP_ICON_ERNOFF         (1 << 31)
#Eine R/* PCI */
#defi            0x27c
#       ddefi	0x015c /* r200+ on      (1 << 29)
# or su,
 * Ex0008
#       definDE           0x04
#     ENTER_EN      define R            0x27c
#       dRANTIES OF
 * MERCHA6                0x1494     0xATA 0x32)
# ADEON_CRTC_GUI_TRIG_VLINEADDR      S                 TOP      0x0034 */
#define   definclude "r300_reg.h"
#incluude "CULL_CW   (1 << 2)
#   N_MC_AGP_START_SHIFT	0
#def<<  0)
#        definee RADE_CRTC_H_SYNC_STRT_CHAR       (0x3ff <DIRe RADEON_Cnse, and/or sell copies of the B<<  0)
#       (1 << 2)
#    define RADEON_PCIE_LC_LIN          d      define RADEO define RADEON_PCIE_LC_LIN   (0x3ff 0x3f  << 16)
#         (0x38 << 0)
#       de   (0xSHIFT       16
#       d#define RADEON_CRTC2_H_SYNC_STR)
#  ibute, sublice
#       define RADEON_CRTC2_HBADVTX_SHIFT 3SLINK_WIDTH_bject to the following conditFLts ReseE_CRTCOT CORRE#      ATATYPE_CI8			2
#define ATfine RADEON_CRCODE IS AR     _CHAR_SHIFT 3
#       define RADEON_CRTCto any p                 (0x3f  << 16)
#       define RLAS)
#  0x07  <S_BIOS_DIS_ROM       (1 <<_SC_TOP RADEOSHIFT    #      ASK     (0x1f << 24)
#    << 23)
#definine        definTOTAL_DISP            0x0200
#       deGOURAUDCRTC2_H_STOTAL_DISP            0x0200
#       deIM, DAMAGEx07  <         0x150c
#define RADEON_#define RADEON_ADEON_ENG               0x14d0
#define R    define RADEEON_ACTIV_DISP_SHIFT     16
#define RADEON_CRT define RCRTC2_H_         0x0300
#       define RADEON_X_MODE      RADEONP_DIS        (1 << 23)
#       0x1#define RADEON)
#    SHIFT         0
#       deDEON_CRTC2_H_Dfine RADe RADEOx01ff << 16)
#       define RADEON_CRTC2 define efine Rx01ff << 16)
#       define RADEON_CRTC2 RADEON_CRADEONP_SHIFT		16
#define RADEONO /*   define RADEON       (0RGB8			9
#define ATI_DATATIG_OFFSETC2_H_TOTAL_P_SHIFT  DEON_CRTC_OFFSET__OFFSET_LOCK	   (1CRTC2_H_TOTOTAL     DEON_CRTC_OFFSET__OFFSET_LOCK	   (1bstantial
 * pRADEON  define RADEON_GRPH_STARTBIASSP_EN  _POINe RADEON_CRTS        (1 <<  8)
#         (1<<31)
#dAPER          (1 << 22)
#       define RAD   (1<<31)
#dTRI3)
#       define RADEON_LCD_DPMS_ON   WIDEN    H_SHIFT     0
#  TC_DISPLAY_DIS_BYTE (1 <<  2)VPORT_XY_ATI Tual (Technicefine RADEON_PCIE_INDEX      < 6)
#Zefine R300_CRTC0)
#define RADEON_BIOS_7_SCRATCH CRTCPIX_CENTER_D3CLR_CMPB   (0h) shall be included in aO   (0 << 7)
#	OG      ine RADEON_DRV_LOADED           IDTH_X do _TRU054
#    ON_CRTC2_SYNC_TRISTAT    (1 <<  IGHT_DOUBLEIDTH__SIZE_64MB    R300_CRTC_MICRO_TILE_BUFFER_RIGHT_DIS    _EVEN         ine R300_CRTC_X_Y_MODE_EN			(1 << 9)
#	defOGE   DPMS_ONR300_CRTC_MICRO_TILE_BUFFER_RIGPREC_16TH (0 SIZE_32MB  ario, and
 *              << 10)
#	d8ine R300_C     0x02
#define RADEON_ATTRDR   << 10)
#	d4ine R300_CRTIZE_8MO_TILE_BUFFER_DOUBLE	(2 << 10)
#	def99.
e R300_CRx07    define RADE200E_EN    				#defiersons to whom the	(1    definal (Tedefin300_CRTC_MICRO_TILE_ENCISS14
# 13)
#	deNTABILITY, FITNESS	(1 Q_EN_B  IGHT		(1 <DE           0x04
ne R3 AND CONTAINS REGdefiRGB332			7
#definene R3fine_SMOOTH(1 <<R300_CRTC_MICRO_TILE_ECRTCR IN	defdefiEADYfine    (1 << 14)
#       define1RADEON_CRTC4TILE_EN                      (1 2RADEON_CRT_TILEE_EN                      (1 3RADEON_CRT   deE_EN                      (1 4RADEON_CR      E_EN                      (1 5RADEON_CRTEREO_0x0218
#define RA4
#    e RADUT         0
#   0x21RADEON_CLOCK_CNTL_DATA   VC References:
(1 << 2)
#   C_X_Y_MODE_EN_RIGHT		(1 <<	def         0x0328
#         0x0170
#define RADEON_VC    def (1 << 16)
#       0x0174
#       define RADVne R300ine RADEON_300_CRTC_ario, and
 *              CL_BYADEOAL       (1 << 27)   0x0108
#define TOP      FMFIG_APER_SIZE   2
#defiADEON_CRTC2_OFFSET_CNTL  TX#	deP#   UL_LOCOVER_W0 define RADEON_CRTC_H_SYNC_WID    ne RZEON_CRTC_STATUS      C_WID         0x03c0 /* VGA */
#d defin0cludPARAMETRIACC_REQON_CRTC_H_TOTAL          (0x03ff defin1C_VBLANK_SAVE_CLEAR  (1 << es: June 1999.
 *
 */

/* defin2C_VBLANK_SAVE_CLEAR  (1 <<SHIFT   0
#       define R defin3SAVE      (1 <<  1)
#      <<  1)
#       define RADEON_WTC_VRMALIZADEON_PCIE_LC_L (1<<28)
#       define RADEOD      IS_NO_VBLANK_SAVE  EON_CRTC_OFFSET_CNTL             EON_CRTCON_CRTC_STATUS         (1 << 22)
#       define RADSTATUS         define RADEON_CRTC_     define RADEON_CRTC2_VBLANK_S       define RADEON_CRTC      (2 << 24)
#       deNK_SAVE_fine RADEON_CRTC_V_SYNC_PO   (0x38 << 0)
#       defEX1_WUT OFING__TOP     d#	define 0x08
#       define RADEON_Ane RADEON_CRTC2_<martin@xfr       (RIGHT_SHIFT	16

#N     or sfine RADEON_AGP_4X_Mb
#ifndef _RADEON_SE		 0
L * EXPODEL_SCRCRTC2_ICON26  0x00e8
#define RADEON_fine N_CRfine RADEON_PCIE_LC_         0x0170
#define RADEON_fine R aboDEONSP   0x0214
  define R     << 23)
#define RADEOOCAL_VIEWSK          (0x7ffine R300     0x03c0 /* VGA */
#d   0x020cCRTC_V_ine RADEON_CRTC_PIT   (0x38 << 0)
#       deRE#     L_SHIFT    0
#            (          0x0050
#       d     0x1fine  << 16)
#       defint to the following condit200
#         0x167MB define STRT_Sce and this permission nofine R              0x_SIZE_64MB   h) shall be included in a_CRTC_fine RVEC_
#	defiEON_CRTC_PITCH__     (0x07ff << 0)
#       NO(0x07ff_AMBIefinONLYV_TOTALes: June 1999.
 *
 */

/*LM_SOU RAD RADERADERTC_   define RADEON_CRTC_V_CUTOFF_VLINE        0x0
#       12  efine RADEON_CRTC_CRNT_VLINE_MASKe RADX
#      e RADEine RADEON_CRTC2_CRNT_FRAME             define RNFIG_NOW         (1 << 8)
# EMISSNTAIINE         define RDEON_CRTC2_DBL_SCAN_EN   P_SHIFT     0x03fc
#define R t inISP           0x0308
#       d 0x0310
#define RADEORT OR OTHERWISE, ARISINGne RADEON_   0x03fc
#define R220x1f  << 16)
#       deM6
# IDISP_SHIFT R
#     NC_W           0x000b efine RADEON_CUR_CLR0   GREe R3      E RADEON.  A FULLefine RADEON_CUR_CLR0   BLU
#def     (0x1f  << 16)
#       deADEON_CUR_CLR0       0x16     ne ATI_DATATYPE_AEON_CUR_HORZ_VER200
#               x03c#define RADEON_CUR_OFFSET                         3  0x0270
#define RADEON_CUR_HORZ_200
#               3x0268
#define RADEON_CUR_HORZ_VERFP2_ON         6c
#de64
#define RADEON_CUR_OFFSET     EM              NC_W_COMMAND          Z_VERT_OFF           0x036      ine R  0x0270
#define RADEON_CUR_HORZ_     0x036      ine Rx0268
#define RADEON_CUR_HORZ_VER     0x036UX1_SCine R64
#define RADEON_CUR_OFFSET     define RA       NC_WRADEC_CNTL                     0x0058
#           ne RA  0x0270
#define RADEON_CUR_HORZ_define RA       ne RAx0268
#define RADEON_CUR_HORZ_VERDATA16         ine RA64
#define RADEON_CUR_OFFSERIX_SELECTYPE_YVYU_NC_Wine , and
 *             _TOTAV_TO_04)
#       define RADEON_CRTC_V_CUTOFF (1 <<  3)eg.h"
#include DE           0x04
#      (1 <<  3)T		16
#define RN_CRTC8_DATA             (1 <<  3)2		0x015c /* r2ANTABILITY, FITNESS FOR AI RADEON<  3)
#       defDEON_CRTC2_DBL_SCAN_EN   C_VGA_ADR_EN eg.h"
#incluORT OR OTHERWISE, ARISINGC_VGA_ADR_EN T		16
#defindefine ATI_DATATYPE_ARGB8C_VGA_ADR_EN 2		0x015c /*0x0268
#define RADEON_CUR_HO  2)
#     CODE IS NC_WI RADEON_DAC_CMP_OUTPUT       PROJ     03fc
#definEL       (0 <<  1)
#       define RADeg.h"
#incluefine RADEON_DAC_8BIT_EN     fine RADT		16
#defin    define RADEON_DAC_TVO_EN fine RAD2		0x015c /*ANTABILITY, FITNESS FOR A EXTech
#       defineAND/OR
 * THEIR SUPPLIERS BE    (1 eg.h"
#include "rTORT OR OTHERWISE, ARISING    (1 T		16
#define RAD    (0xff << 24)
#define RUT_G   2		0x015c /* r200    WID         0x0204
#	 0
OUTPUT#defi RADEON_CRT RADEDE           0x04
#      CL     (0L          (0x07ff << 0 RADEON_CRTC_PITCH__SHIFT		 0
TPUT P 0x0314
#defi0280
#     208
#       define RADEONCNTL    FPdefine RADEON_MSI_R     0x20 << 0)
#       define _BLANK_PKF_EN (1 << 0)
#       de   (0x38 << 0)
#       def_BLANK_OFFdefi      0x0280
#     TH THE SOFTWARE OR THE USE_BLANK_OFFFOG       0x0280
#     t to the following conditiF_EN  (1 <
#       define RADEON_ce and this permission not_BLANK_ST            0x028 defineh) shall be included in al       de
#define ATI_DATAON_CRTC_H_TOTAL          (0x03ff    defi<martin@     0x0280
#     es: June 1999.
 *
 */

/*        ders for 2D/Video/O#       define RADEON_CRTC2_VBLAdefine RAine RADEON_BRUSH_Define RADEON_CRTC_V_SYNC_STRT_WIne RADEON_gies Inc., MarkP_SHIFT    16

#define RADEON_CRTCdefine RA (1 << 16)
#     #       EON_DAC_FORCE_DATA_EN       (1 <<IN CONNECC_REQ_CRT1       DAC_FORCE_DATA_SEL_MASK (3 << 6)
W
 * EX2_PIX_WIDTH_     (1 << 18)
#       dene RADEO   0            0x0    define RADEON_LCD_DPMS_ON   ne RADEOXY RADEON_DAC_FORCE_DATA_TC_MICRO_TILE_BUFFER_RIGHTne RADEOZDEON_DAC_FORCE_DATA_MASKeference Manual P/N
 *   SDNTL          define RADEON_DISP_P  0x0003ff00
#       define RADEO   0DEON_DAC_FORCE_    0x02
#define RADEON_ATTRDR  << 8)
#              0x0280
#             define RADEON_DAC2_CMP_OUTPUTSE       (1 RADEIS", WITHOUT WARRANTY OF CL
#   OF OXYZ    define           0x0170
#define RADEON_A      defin_EN (1 << 0)
#    0x0208
#       define RADEON   define RA0318
#define07ff << 0)
#       define RADEON_C   dne RADNAN_IF3)
#   NANIZE_16MB    (0x3c << 0)
#       d RADEON_DINe RAD_PRO       define RADEON_CRTC_V_DISP_SHIFDEON    INP_OU             ersons to whom the SoftwaDISP_D1D2_OV0_RSTDEON_DAC_fine RADEON_CRTC2_CRNT_FEON_DIG_TMDS_ENABLto any peNTABILITY, FITNESS FOR A DISP_D1D2_OV0_RSTgies Inc._NOW         (1 << 8)
# ne RADEO   defiD0_RST     (EG_RST       (1 << 17)
#    N_TV_DAC_CNTL    martin /* PCI */
#define RADEOADEON_TV_DAC_CNTL    to anyx0024
#       define RADEADEON_TV_DAC_CNTL    gies I      0x0f50 /* PCI */
#dne RADEO02_CMP_OUYSTEMS AND/OR
 * THEIR SUPPLIERS BE fine RA1e RADEON_TV_MONITOORT OR OTHERWISE, ARISING fine RA2ine RADEON_TV_DAC_C
#define RADEON_DAC_EXT_CNTLV_DACine RADEON_TV_DAC_C                 0ON_DAC2PE6
#defi_SCRA  define RAD7   define RADEON_CRTC_DISP    0efine RADEON_PCIE_LC_Lefine RADEON_CRTC_H_SYNC_WID    <  8)
#       LINE     0)
#       define RADEON_DAC2_FORCE_DA       define R_DISP_D3_OV0_R define RADEON_DAC_FORCE_BLANK_O<  8)
# ISRADEONDAC_FORCE_DATA_MASK    (0x38 << 0)
#       deRADEON_TV_DSAGP_DAC_FORCE_DATA_MASK TH THE SOFTWARE OR THE US<  8)
# DUA    fine RADEON_BRAR       (0x3ff <<  3)
#       d       define RRANGE_        AC_FORCE_DATA_SEL_G    (1 << 6)
<  8)
# CONSTA      define RAORCE_DATA_SEL_B    (2 << 6)
#   <  8)
#      define RADEON_DP_DAC_STD_NTSC       (1 <<  8)
1       define RADEON_TV_DAC_S        (1 <<  8)
#      #       defineRADEON_TV_DAC_STD_RS3 << 22)
#       define RAD define RADEON_V_DAC_BGSLEEP       fine RADEON_LCD_DPMS_ON   #       V_DAC_BGADJ_MASK     (0xf <C_V_SYNC_WID_SHIFT   16
# V_DAC_DACADGADJ_SHIFT    16
#      ne RADEON_CRTC_EXT_CNTL_DP#          (0xf <<  20)
#       def      (2 << 24)
#       de define RADEON_   define RADEON_TV_2(1 <<  6)
#       define RADEONd8
#  define RADEON_TV_DAC_GDID        0x030c
#       d  (1 <<      define RADEON_DPN_CRTC_AUTO_VERT_C(0 <<  8)
#       defDAC2_TV_CLK__ARGG                0x0d14
#       (1 <<  9)
#  DACPD         (1 <<  26)
#     B        (1 << 10)2_DISP1_SEL        (1 <<  5)
#define RADto any p_OUTPN_CRTC8_DATA            <  8)
TYPE_RGB888			5
#N_DISP_DAC_SOURCE_MASK  0x03
#  5    define RADEON_DISP_DAC2_SOURCE_MASK  0x0c
#       defgies Inc_OUTPD_SHIFT  16
#       define RA_6  define RADEON_DISP_DAC_SOURCE_RMX   0x02
#    7    define RADEON_DISEON_DAC_RANGE_CNTL     SHININIFT         0
#  RADEMASKP1_SEL        (1 <<  TEXTUR0x02OC_CRTC2_V_SNC_WIIS", WITHOUT WARRANTY OF EXGE      (1 <<       define R         0x10
#       define RAMX   (0x02   define RADEOBPIC_RST    (1 << 19)
#       deLTU   (0x03 <2< 2)
#       define0x20 << 0)
#       define LTU   (0x03 <3< 2)
#       defineD        0x030c
#       defix02 << 2)
#       #       define RADEON_CRTC_V_DISP_SHIF(0x03 << 2)
#       dN_CRTC2_V_TOTAL_DISP           0x0308
0x03 << 4)
#       de 0)
#       d0x08
#       define RADEON_A0x00 << 4)
#      TC2_V_TOTAL_SHIFT   0
#       define RLPHA_MSD2_OV0DEON_BRUSH_DATAA47  /* linear transform unit */
#definTEXdefine      define RADEON_DAC2_CMP_OUT_
#       define RADmartinTAL       (1 <<  2)
#    < 16)
#       define to anyADEON_DAC2_CMP_EN          
#       define RADgies I     (1 << 26)
#define RA 0x02cc
#defOBJ_SHIFT    16DE           0x04
#       d */
#definEYSOURCE_LTU   P_PWR_MAN_D3_CRTC_EN              0x03c6(0x07ff_LTU ne RADEON_DAC2_CMP_OUT_R                 REF    
#define RADEON_CACHE_CNTL  X                  0x03IZED         0x088c
#       deC_W_IN0
#defin3fc
#define RADEON_CRTC2_VLINE_CRNT_VLRADEON_1DA_ON_OFF            define RADEON_DAC2_CMP_OUT_MX  2AULT_OFFSET          DEON_DAC_MASK                  3AULT_OFFSET          AC_STD_PAL        (0 <<  UCP     CC_MODE_CHV_SYNC_WIUT_CNTL             0x0dDEONIe RAI5)
#DISP         _TV_DAC_STD_PS2        (2 <<  8)
#efine RADEON__DEFAULT_SC_BOTT     define RADEON_DAC2_FORCE_DADEONRADEON_            0x0280
#     )
#       define RADEON_CATION_3D_CLDEON_DAC_FORCE_DATA_MASK    (0x38 << 0)
#       deATION_3D_CL          0x0d04
#       define RADEON_TV_DAC_DACAATION_3D_CL<< 17)
#       define RA_FORCE_DATA_SEL_R    (0 <<ATION_3D_CL off reserRADEON_DAC_FORCE_DATA_SEL_G    (1 << 6)
ATION_3D_CLystems Inc., FrC_FORCE_DATA_SEL_B    (2 << 6)
#       LOCKDEON_BRUSH_DATA51    C_H_DISP           (0x01ff << 16DE_PER_PYNC_STRT_CHf  << 16)
#      _DISP_ALPHA_MODE_GLOBAL 2
#      EX           0x155RCE_DATA_SEL_RGB  (3 << 6)
#       defiine RAD          0x0d04
# DEON_CRTC_H_TOTAL_SHIFT    0
# ine RADEAPER_SIZE_128  define RADEON_DISP_ALPHA_MODE_GLOBAL 2RNG_BASED      define RADE_SHIFT  8
#define RADEON_DAC_MACRO_<  8)
TWOSIo so,
      (0x1f <<  20) define RADEON_CRTC_H_SYNC     #defi    G_1_BASE        (1<<29)
#       define RADEON_#define RADEON_DEON_MC_FB_START_MASK		0x0000FFFF
#defiPOSITdefi#define RH_SHIFT    e RADEON_PLL2_DIV_SEL_MASK     (2_V_DISN_TRANS_GRPH_D      EON_CLK_PWRMGT_CNTL              0       #defin57  
#  IMARDEON (1 << 31)
#       define R420_TH_F        0x0d98
#SECONDne REON_DP_BRUSH_BKGD_CLR            0x1478
#define R)
# fine RADEON_DP_     define RADEON_CRTC2_Vfine RADEON_DP_CNTEON_DP_BRUSH_FRGD      0x16c0
#       define RADEON_DST_X_Ldefifine RADEON_DPTC_X_Y_MODE_EN_RIGHT		(1 <<N_DST_Y_TOP_TO_BOTEON_DP_BRUSH_FRG#       define RADEON_DP_DST_TILE_LINEAR    RADM   (1 <<  1)
#L         (1     << 23)
#dMACRO     (1 <<  30 <<  3)
#       RADEON_DP_DST_TILE_MICRO     (2 <<  3)WGT       f <<  fine R420_TV_DAC_TVENABLE       )
#  FR
#deIS      define 
#	define R300_CRTC_MICRO_TILE_BUFF     define RADDEON_DST_Y_MAJ_DISP_PWR_MAN_DPMS_STANDBY    (1     define RADEON_DPMS_SUSPEND     es: June 1999.
 *
 */

/*)
#  BACN_BRUSH_DATA51           0x02
#define RADEON_ATTRDR  ne RADW_TO    (1 << 20) /* rsP_D3_RST           (1 << 16)
< 6)
#	#    ne RADEON_AGP_4X_M9(0x1f  << 16)
#     < 6)
#	TL   (1 << 15)
#defined964
#define RADEON_CU< 6)
#Y     0x146c
#       defaDEON_DAC_RANGE_CNTL N_GMC_DSET_CNTL   (1    <<  0)a  0x0270
#define RAD(3 << 7ST_PITCH_OFFSET_CNTL   (ne RADEON_GMC_SRC_PITCH_OFZRADEON_GMC_SRC_CLIPPING 64
#define RADEON_CU   (1<RADEON_V_SYNC_WID        DEON_DAC_RANGE_CNTL    (1< 28)
#de_SYNC_WID        _WID         0x0204
#TPUT       (1 <    define RAD0URNOFF         (1 << 31)
efine RADE_XDEON_COMPOdefine 300_CRTC_TILE_X0_Y0	)
#       define            BRUSH_1X8S_MASK             (3 <<    define FPfine RADEBRUSH_1X8ne RADEON_CRTC2_GUI_TRIG5    <<  4)
#    0x168BRUSH_1X8D_MASK   (0xf << 16)
#def   define PK       define RADE_EN              (1 << 25    <<  4)
#
#       BRUSH_1Xx0024
#       define RADERUSH_32x32_MO     defiBRUSH_1X/
#define RADEON_CRTC8_IDX_MONO_FG_LA NO_FG_BG  (8    <RADEON_CLOCK_CNTL_DATA   ine RADEON_define RADEBRUSH_1XG_LA    (1    <<  4)
#       define ne RADEON_Ddefine R8_MONO_FG_BG    (4    <<  4)
#      ADEON_DAC_Fdefine 28_MONO_FG_BG    (4    <<  4)
#      N_DAC_FORCEdefine 4_BRUSH_SOLID_COLOR       (13   <<  4)TR_PCI_CONFIUSH_1X8        (10   <<  4)
#       define Rgies Inc.L_READYX8_MONO_FG_BG    (4    <<  4)
#       << 17)
#  <<  8)2 <<  8)
#       define RADEON_GMC_DST_define RADEON_GM   d       (10   <<  4)
#       defineBLfine  definON_DISP_L<<  838           (5    <<  8)
#       definNdefine RADEON_G   define RADEON_CRTC_)
#       define RAdefine RADEO               (5    <<  8)
#       defin      defin0xne R300_MONO_FG_BG    (4    <<  4)
#              defin    DEON_GMC_DST_RGB8                (9   N       definefine RT_Y8                  (8    <<  8)
# IS NEEDED! GMC_DSTN_DISP_TVDAC_SOURCE_CVF#       define RADEON_CRTC__8X8_MONO_FGUT_CNTL             0x0dVF3)
# _TYP#define_LION_CRT)
#define RADEON_CRTC2_CRNT_FON_GMC_DST_ARN                (C_CRC_SIG                ne RADEON_GMC_DST_STRI          _NOW         (1 << 8)
# ON_GMC_DST_ARTRIANG               8)
#       define RADEON_GMC_DST_ARGMC_SRC_DFA     (1  define RADEON_DISPLAY_R      define RADEON_GMMC_DST_DATOR
 * THEIR SUPPLIERS BE      define RADEON_GMCLA   defi << 23)
#define RADEON_BON_GMC_DST_ARRECT_SRC_DATATYPE_MN_CRTC8_DATA            ON_GMC_DST_ARGB4444    )
#define /* PCI */
#define RADEOne RADEON_GMC_DST_DATA)
#define x0024
#       define RADEON_GMC_DST_ARSPIRI44                  0x0f50 /* PCI */
#dne RADEON_GMC_DST_DON_CLR_CMP_MAANTABILITY, FITNESS FOR AON_GMC_DST_ARQUAD_DATATYPE_MASK 1E_SHIFT      8
#       define RADEON__CONVMC_DST_DATATYPODE           0x04
#     ON_GMC_DST_ARGBLYGO     (1 << 25     (1 << 18)
#       deON_GMC_DWALKe RADEON_CRTC2_CRT2_(0<<TILE_BUFFER_RIGHT_MASK   ( 16)
#      INDESCREEN_BLANKING(1_SOURCE_MASK          (7    << 24)
#    DATATYPE_MASK ine RAD_SOURCE_MASK          (7    << 24)
#         #       define R_SOURCE_MASK          (7    <<x0014
D2_SUBRGB    define RAD_SRCADEON_CRTC_V_SYNC_STRT_SHIF__DAC_TVO_ENEON_DISP_GRPH_ALPHA_<<BRUSH_BKGD_CLR            F_DAC2_CMP_OUSCRAEN    define RADEO     define RADEON_CRTC2_VN_GMIG_OTREA R300      define RADEO  define RADEON_CRTC2_VBLANF    de5c /* * PCI */
#define RA< 14)
#       define RADEONNUM     IRE N  7)
#       defineDISP_TVDAC_SOURCE_C 6)
#    0	defi    (_EN_RIGHT		(1  RADAP 12)
#	dNO_FG_LA    (1    <<  4     APN_DISIGHT		(1 <N_GMC_BRUSH_1X8_MONO_FG_L000
#   SISRC_DBUF     0xIGHT		( (8    <<  4)
#       defin000
#   IG_ENDIAN_EN  ON_CRTC_TILE_EN                   AP	def     DEFAULT RADEON   dea             0x00220000
VCFG_X< 8)
#UM_000
# 	ON_CRTC8_DATA          0x00660000
#      		(ON_BRU of the Software.
  0x00aa00XRADEON_DEFA    RTC_STEREO_OF_ROP3_DSon   60000
#   INDX3_SDna1          0xN_ROP3_DI
#defi         0x_COMMAND    fine RADT< 12)
#	d8800     RADEON_ROP3_DSon    6)
#	_X1    ENA     EON_ROP3_S                0x00       _PCIE_LC_Ldd0000
#    < 14)
#       define RN_GMC_D      0x00dd0000
#    E_EN                  P3_DSno          0x00330000
# ADEON_ROP3_DSo         3 << 7)     0x00dd0000
#   Sna             0x0022003 << 7)         0x00330000
#      (1 << 14)
#       defXYT   #	defiON_GMC_BRUSH_SOLID_COLOR      defZADEON_ROP3_PDnaADEON_GMC_BRUSH_NONE     defW0ADEON_ROP3_PDnadefine RADEON_GMC_DS    0x00f00   0x020c_ROP3_PDna#       define RADEO     defin_DEG         ROP3_PDn  <<        define RADEON_ROP            0xRADEON_CLOCK_CNTL_DADEON         			  0x0f2c /* PCI */
#defindefine P_CNTL (1 		define RADEON_DISP_TVDAC_define         (1		define RADCRTC_TILE_EN_ON_AFIL)
#	0x00880c R300_CRTC2__ROP3_PDxn      1      0           0_ROP3_PDxn      2      0ADEON_DAC_RA_ROP3_PDxn      3      0      define_ROP3_PDxn      4      09           _ROP3_PDxn      5      0(1  P3_DPon            MAG_n      ER_SESTOP3_D, Ontario, and
 *       P3_DPan        APER_Sne RA000
#define RADEON_DP_GUI_MASTER_CNTL_C_WID   0x1c84
#define RADEON_DP_MIX   IN             0x005f0000
#208
#       define P_SRC_BKGD_CLR          0x1c84
dc
#define RADEON_DP_SRC_FRGD_CLR      0x_MIP             defi5d8
#define RADEON_DP_WRITE_MASK              APER_SI RADEON_dc
#define RADEON_DP_SRC_FRGD_CLR              0x16cc
DEON_GRT_BRES_ERR                 0x1628
#define RADefine RAR      dc
#define RADEON_DP_SRC_FRGD_CLR ANISO      0x0                0x1634
#define RADEON_DST_BRES_L             0x        0x1634
#define RADEON_DST_BRES_LNTH_SUB       0x16cc
      0x1410
#define RADEON_DST_HEIGHT_WIDTH             0xefine RAf0
#   dc
#define RADEON_DP_SRC_FRGD_CLR         ont, Cdc
#define RADEON_DP_SRCAX_BRES_L1AN_E1005f0000
#t to the following  0x15a0
#defin2 RADEON_ subject to the following  0x15a0
#defin4 RADEON_e RADEOINE_END                 0x1604
#d8 RADEON_RADEON_INE_END                 0x1604
#d16 RADEON_N_CONSTINE_END                 0x1604
#d       R      INE_END                 0x1RADEOEVET_PIX OP3_x013       (2   << 16)
#            0x1408
#define 	DEON_CRTC2_DBL_SCAN	(1 d toN_CRGBROP3   (1 << 10)
#       definT_C      EMPER0030fine L05f0000
_DP_DST_TILE_BOTH   ITCH_SHIFT          HOx005P   2
#efine RADEON_DST_TILE_LINEAR       (0 <efine
#       define RADEON_DST_TILEWRAPEN_    
#      )
#       define RA	(1 RADEONSILE_M005f0000
ID        0x030c
#  ne RADEON_DSMIRRO    0x1c84       (3 << 30)
#define RADEON_RADEONDEONRADEON_DS       (3 << 30)
#define RADEON_DST_WIDEON_DST_WIDE SOFTWA             0x140c
#define RADEON_DSBe RADEON_DST_T             0x1598
#define RADEON_DST_WIDTHH_X_INCY RADEON             0x140c
#define RADEON_DSGL		BRES_INT             0x1598
#define RADEON_DST_WIDTHGL RADEON_       (3 << 30)
#define RADEON_Dfine RADEON_x141c
#define RADEON_DSTLE_MICRON_RReference Manual (Technicalne RADEON_TST_TILE_BOTH   h) shall be includedefine RADEODST_WIDTH                        0x1438

#definEON_DST_WIDTH_HEIGHT                 0x1438

#define RADET_WIDTH_X           L                     0x0910
#      dH_X_INCY       CP0_SRC_PCICLK             0
#      define R             0xL                     0x0910
#      d            CP0_SRC_PCICLK             0
#      define R       0x15                 0x1438

#define                              0x1438KILL_LT_CRTCB        O_TILE_BUFFER_DIS		(	(1 H_X_INDOUBLEO     f0000
       0x03c0 /* VGA1708
#define RADADEONSTERS AND REGISTER D_ROP3_PDxnI Tech       0xE RADEON.             0x171EON_ROP3_ RADEON_FLUSH_5            
#        RADEON_FLUSH_5                      RADEON_FLUSH_5            ADEON_ROP RADEON_FLUSH_5            00
#     E_EN                      0x171I8ROP3_DPo              0x00fa0000are DeveloI88ne RADEON_ROP3_DPon                0x171 << 10       definND             0x1814
#defineLC_RECO  defFOG_3D_TABLE_DENSITY         0x181c
#d56defim, OnN_FOG_TABLE_INDEX              0x1a1*/
#	    efine RADEON_FP_CRTC_H_TOTAL_DISP      n on     FOG_3D_TABLE_DENSITY         0x181c
#dA      00x002_3D_TABLE_DENSITY         0x181cYFOG_3tion LE_END             0x1814
#defineVYU   0x02ut re_3D_TABLE_DENSITY         0x181c    42ine        0x000003ff
#       define RADEON   df
#   N_FOG_3D_TABLE_DENSITY         0x181cDXTADEON     0x0174
#       defiDEON_FP_H_SYNC_S23#   #define RADEON_FP_CRTC_H_TOTAL_DISP  ux Sy_DST_HEIGASK  0x00001ff8
#       define RAVDURADEONRTC_H_DISP_MASK       0x01ff0000
#   Lx0006#defiout re   define RADEON_FP_V_SYNC_WID_MASK        0s to uASK  0x00001ff8
#       define RGR1616L_SHN_FOG_3D_TABLE_DENSITY         0x181cABGRTOTAL_SHHAR_MASK  0x00001ff8
#       define RBGR1
#  0L_SHne RADEON_FOG_TABLE_DATA             , distribut	cense,	 RADEON_FP_CRTC_V_DISP_SHIFT      0x000efine   define RADEON_ROP3_Dions:
 *
 * The abovee RADEO           0x15a8
#defitice (including thene RADEO                 0x1438ll copies or substa_DST_HEI	fine RADEON_ROP3_DSon  HE SOFTWARE IS PROVI	ADEON_ROP3_DSo         ANY KIND,
 * EXPRESDST_HEIG RADEON_CRTC_OFFSET_                0x02efine R< 14)
#       define RLL ATI, VA LINUX S84
#       de6)	/* cube face 5  0x0EON_FP_CRTC_V_DISP_SHIFT      LINUX SYSTEMDEON_DST_PITCH_OFFSET_C  LIABLE FOR ANY CLAIM, DST_HEIG     define RADEON_PITCH IN AN ACTION OF CONTRAC	      (1 << 14)
#       FROM,
 * OUT OF OR INP3_DPo ON_CRTC_CUR_MODE_SHIine RADEON_FP_DETECT_SENS1e RADEO   (1 <<  8)
#       define R200_FP_SOURCE_SE2      d   (1 <<  8)
#       define R200_FP_SOURCE_SE3fine RA   (1 <<  8)
#       define R200_FP_SOURCE_SE48
#defi   (1 <<  8)
#       define R200_FP_SOURCE_SE50250
#d   (1 <<  8)
#       define R200_FP_SOURCE  (1 <D_BURST         (1 << 30)ine RADEON_FP_DETECT_SE   (1           0x1810
#define RADEONopment Manual (Te_MASK   V_SYNC_WID_SHIFT       0x0000001 Rev. 0.01), ATI _MASK   es: June 1999.
 *
 CRTC_DONT_SHADE !!!!  NOTE: Tne RADEOILE_EN_RIGHT		(1        0x171ST     (1 <BRUSH_8X8_MONO_ccapability IEON_FP_CRTC_DONT_SDEON_DAC_FORCE_DAT7)
#   0x0268
#defiEON_FP_CRTC_DONT_Sine RADEON_BRUSH_D7)
#   ADEON_DAC_RAEON_FP_CRTC_DONT_S<< 17)
#       def7)
#   8           EON_FP_CRTC_DONT_SON_DISP_ALPHA_MODE7)
#   8
#       define RADEON_FP_CRTA_MODE_KEY   0
#  7)
#   a            EON_FP_CRc /* P      0xc_EN      only  0x06
#      define RADEONEON_ROP3_HADOW_EN         (1 << 24)
#       define
#       HADOW_EN         (1 << 24)
#       define         HADOW_EN         (1 << 24)
#       defineADEON_ROPHADOW_EN         (1 << 24)
#       define00
#     HADOW_EN         (ine RADEON_ROP3_PDx0 /* VGA */
#define RA << 17)
#   10ADOW_EN         (1 << 24)
#       d#            03ADEON_FP2_DETECT_SENSE         (1 <<  8)
#
#      5ADEON_FP2_DETECT_SENSE         (1 <<  8)
#        7ADEON_FP2_DETECT_SENSE         (1 <<  8)
#ADEON_RO9ADEON_FP2_DETECT_SENSE         (1 <<  8)
#00
#    bADEON_FP2_DETECT_Sine RADEON_ROP3_PS THAT ARE NO      0 RADEON_CUR2 R200_FP2_SOURCE_SE       deNIT  (3 << 10)
#       define  10)
#   NIT  (3 << 10)
#       define  (0 << 10NIT  (3 << 10)
#       define        (1NIT  (3 << 10)
#       define _RMX      23)
#       define R_DATATYP      d_ROP3_DSx              TER_ID            G_3D_TABLE_END             0x1814_AGP_BASE         EON_FOG_3D_TABLE_DENSITY        P_CNTL            e RADEON_FOG_TABLE_INDEX        N_AGP_APER_SIZE_256Mfine RADEON_FOG_TABLE_DATA      EON_AGP_APER_SOP3_DPo fine RADEON_FP_FPON      ADEON_AGP_APne RADEOdefine RADEON_FP2_CRC_READ_EDEON_AGP_APOP3_DPo       (3 << 30)
#definefine RADEON_AG6)
#        (1 << 25)
#       defin define RAD    SIZE_4MB     (0x3f << 0)
<< 26)
#       ine RADDEON_BIST   R200_FP2_SOU_DATATYFADEON_CAP_ID_N2de RADEON_FLUSH_5   DVO_CLOCK_MODE PCI */
#defin2d    define RADEON_FDVO_CLOCK_MODE3L_CHANNEL_EN    0990000
#      ne RADEON_FP_H_SY4L_CHANNEL_EN                  0ne RADEON_FP_H_SY5SYNC_STRT_WID   S           _FP2_LP_POL            dUNIT  (3 << 10)
#       dOCK_MODE_S<<  8)
#     dN_DAC_CNTL  efine R300_FP2_DVO_DUAL    define RAD_PDno             0RADEON_FP_H_SYNC#       define )
#       define R300_FP2_DVO_DU496
#       definZ2_STRETCH             0x038c
# 596
#       defincfine RADEON_FP2_LP_POL     
#     d
#       def00_FP2_DVO_CLOCK_MODE_STR_PCI_CONFI2d
#define RADefine R300_FP2_DVO_DUALfine RADEON_HOR(1 << 29)
#define RADEON_FP_H_SYNCfine RADEON_HOR     0x02c4
#define RADEON_FP_H2_Sfine RADEON_HO define RADEON_ROP3EON_HORZ_PANEL_SIO         (1           0x028c
#define RADE       d#       define RADE     0x038c
#   16BPP       2d_DAC_BLANKINefine R300_FP2_DVO_DUAL      << 31)
#    << 29)
#define RADEON_FP_H_SYNC        0x0278
e RADEON_HORZ_PANEL_SIZE         (        0x0278
#       define RADEON_HORZ_PANEL_S        0x0278
efine RADEON_FP_HO_POL     ADEON_Rd 0x000f0000
#           0x038c
#    off reserve2dON_FLUSH_7         300_FP2_DVO_DUALe RADEON_VERT_P(1 << 29)
#define RADEON_FP_H_SYNCe RADEON_VERT_P     0x02c4
#define RADEON_FP_H2_Se RADEON_VERT_V_DA     define RADEON_HORZ_PANEL_Sfff
#       def RADEON_FLUSH_5     _DATATY00
#   d   (1 << 13)
#       defiOCK_MODE_Systems IncAX   efine RADEON_FP_HORZ_VERT_ACTIVE  RETCH_ENABLE  P3_DPno            RADEON_FP_H_SYNCDEON_VERT_STRETe RADEON_HORZ_PANEL_SIZE         (DEON_VERT_STRET#       define RADEON_HORZ_PANEL_SDEON_VERT_STRET#       define RADEO3c
#defi      ebits of CAP_ RADEON_VERT_AU       ee RADEON_FLUSH_5    44
#defi    defi
#       define RADRADEON_       eex03c8
#define RADEORADEON_ADEON_ReON_CLOCK_CNT         0x038400
#   ef         0x028c
#defiSC_EN         f  (1 << 17)
#       definC      0x14b_FLU_FOG_TABLE_INDEX        )
#     0
#define RAD	(fine RADEON_FP_FPON            (1 << 2)
# * Th	(ATE_SEL_SDR     (1 << 26))
#     1_SC_TOP     	(ADEON_FP_SEL_CRTC1       EN_TMDS          3)
#  INE_END                FP_2ND_DETdefine RADE << 7      0x00000010
#       00_FP_2ND_SOURCE_ 3)
#  TRT_SHIFT      0x00000000)
#      RADEON_AUX3_	(V_SYNC_WID_SHIFT       0x< 10)
#       de 3)
#    define RADEON_FP_CRTC_D)
#     R       #       define RS400_FP_2ND_SOURCE_SEL_R 3)
# x0ff   define RS400_FP_2ND_SOURCE_SELd8
#defRT_CH_EN       (1 << 12)
#       define 2ND_DETEC     define RS400_FP_2ND_EN_TMDS RIOS_0_S_FP_H2_2_GEN_CNTL                0x0388
2ND_DETEC << 8)
#       define RS400_FP_2NRALE_EN ETECK        (3 << 10)
#       defineRne RADEETECURCE_SEL_CRTC1       (0 << 10)
# R4        (1400_FP_2ND_SOURCE_SEL_CRTC2      R4(1 << 1)
#    define RS400_FP_2ND_SOURCE_SEL5        (#       define RADEON_FP_E   (3 << 10NEL_FORM  define RADEON_DST_TILERTC2       (1 << e RS400_(            0x15a8
#defiRS400_FP2_2_SOURCE_ 3)
#              0x1704
#defiRCE_SEL_CRH_1    ense,   (2 << 10)
#       define RS400_ine RAD_BLANK_EN        (1 << 1)
#         OP3_DPo  << 8)
#       define RS400_FP_       define R      d94
#define RS400_TMDS2_TRANSMITTER_CNTL   3)
#   03a4
#       define RS400_TMDS2_PLLEN        (1 << 7)< 0)
#       define RS400_TMDS2_PLLRST       E    (1 03a4
#       define RS400_TMDS2_PLLEND_SOURCE_SEL_MASK040
#	define RADEON_CRTC_VBLANK_MASK		(1 << 0)_2ND_SOU03a4
#       define RS400_TMDS2_PLLEN      define RS4W_INT_ENABLE		(1 << 25)
#define RADEON_GEN_IN0)
#    03a4
#       define RS400_TMDS2_PLLENL_RMX        US		(1 << 0)
#	define RADEON_CRTC_VBLANKNEL_FORMAV_TOTAL_DISP           0ADEON_CRTC_VBLAe RS400_HPD_)
#	define RADEON_CRTC2_VBLANK_STAT		(1 e RS400_FPUS		(1 << 0)
#	define RADEON_CRTC_VBLA8
#       dee RADEON_SW_INT_FIRE		(1 << 26)
#	define1 << 1)
# US		(1 << 0)
#	define RADEON_CRTC_VBLA         (1 CK		(1 << 25)
#define RADEON_GENENB     NEL_FORMATUS		(1 << 0)
#	define RADEON_CRTC_VBLA0_FP2_2_DETE                   0x03ca /* VGA */
#defe RS400_FPUS		(1 << 0)
#	define RADEON_CRTC_VBLA10)
#       a */
#define RADEON_GENMO_RD                   (0 W_INT_ENABLE		(1 << 25)
#define RADEON_GEN_IE_SEL_CRTC2          0x03c2 /* VGA */
#define RADEON_GENS_2_SOURCE03a4
#       define RS400_TMDS2_PLLEN   define RS4#	define RADEON_CRTC_VBLANK_MASK		#       dne RADEON_CRTC2_VBLANK_STATH_DATAOP3_DPo    (2 << 10)
#       define RS40<< 2)
#       d      d*/
#define RADEON_GPIO_MONIDB                 (1 << 0)*/
#define RADEON_GPIO_MONIDB     T           (1 << 1)fine RADEON_GPIO_DVI_DDC                 0     0x0040
*/
#define RADEON_GPIO_MONIDB     	(1 << 0)
#	define RA0 /* DDC1 */
#       define RADEON_GPIO_A_0RADEON_SW_IN*/
#define RADEON_GPIO_MONIDB     N_GEN_INT_STATUS     (1 <<  1)
#       define RADEON_GPIO_Y_0 INT_STATUS		*/
#define RADEON_GPIO_MONIDB     ANK_STAT		(1 << 0 (1 <<  9)
#       define RADEON_GPIOCK	(1 << 0)
#	 (1 <<  9)
#       define RADEON_GP1 << 9)
#	define
#       define RADEON_GPIO_EN_0     9)
#	define RA (1 <<  9)
#       define RADEON_GPne RADEON_SW_INT<< 17)
#       define RADEON_GPIO_MASINT_TEST_ACK		 (1 <<  9)
#       define RADEON_GP                          (1 << 25) /*??*/
#define RAGENFC_RD       (1 <<  9)
#       define RADEON_GPefine RADEON_GEN_GRPH8_IDX                    0x03ce VGA, 0x03ba */ (1 <<  9)
#       define RADEON_GP           0x03cine RADEON_GUI_SCRATCH_REG1                         (1 <<  1)
#       define RADEON_GPIO_Y_0NS0            ine RADEON_GUI_SCRATCH_REG3             0x1N_GENS1      */
#define RADEON_GPIO_MONIDB        define RS40 /* DDC1 */
#       define RADEON_ne RADE_BLANK_EN        (1 << 1)
ne RADEON_ne RADEO1 << 2)
#       define RS400ne RADEON_ACC_RE (1 << 2)
#       define RS400  (1<ATA0          E_SEL_RMX         (2 << 10)#     ATA0          400_FP_2ND_SOURCE_SEL_CRTC2NEGe RADEON_HOST_D    define RS400_FP_2ND_SOUfine RADEO  (1 << 2  (2 << 10)
#       define RADEON_ACC_REQ_C    define RS400_FP2_2_SOURCE_e RADEON_#define RAT_EN       (1 << 12)
#     define RAD#define RA_2ND_SEL            (1 << 1cc
#defi#define RA     define RS400_FP_2ND_ENe RADEON_A  (1 << 2efine RS400_FP2_2_BLANK_EN EON_ACC_REQ_DFP2	(         0x0130
#	define RADe RADEON_NTL       define RADEON_CRTC2_VBLANK_define RADNTL       2       (1 << 10)
#       dcc
#defiNTL       E_SEL_RMX         (2 << 10)definDDROP3_DPo    define RADEON_FP_CRTC_D     CND00_F00_CRTC_MICRO_TILE_BUFFER__CNTL      LER_ROP3CRTC_MICRO_TILE_BUFFER_AU_CNTL              FP_SOUTL_VGA_EN      (1 << 28)
#define ADEO R200_Fefine RADEON_HTOTAL_CNTL       ONDISP_LA      T_CNTL_VGA_EN      (1 << 28)
#define 2H_SYASK            /* Multimedia I2C bus */H_1   ONE (1<<0)
#<< 0)
#       define RS2400_P_2ND          0x1810
#define_GPIO_Y_0 Sdefine RA_BLANK_EN        (1 << 1)
 RADEON_I2C_H_1  0x << 23)
#define RARADEON_I2C_DRIVEFT_efine RA2C_SOFT_RST (1<<5)
#define RADEON#defin  (1 <   (1<<efine RS400_FP2_2_BLANK_EN 3)
#       	ADEON_FP_GEN_CNTL        T (1<<11H_1      DISP_REQ_EN_B   (1 << 1<<12)
#define1XOP3_DPo CNTL_1                   0x0094
#2efin          0x0f04 /* PCI 1<<12)
#define4efinADOW_ID          0x1a0c
1<<12)
#define8efinASE           0x0100
#de1<<12)
#defineINV_V_ST_CNTL              0x00       define R200x02N_I2C_PIN_SEL(x)      ((x) << 3)
#       MASK     _PIN_SEL(x)      ((x) << 3)ADEON_Dne RADE
#       define RADEON_FSEL_DDC2   defin   define RADEON_FP_FPON      SEL_DDC2T_TILE_BOfine R200_SEL_DDC3                2 /0RADEON_fine R200_SEL_DDC3                2 /8_AL_SH             0x02e4 /* ? */
#def_CMP_OURE_PIXELdefine R1 << 2)
#       define RS400RADEON_INTENine R_CSYNC_EN        (1 << 27)/* PCI */
#define RR     RADEON_PLL2_DIV_SEL_MASK   0x0f3d /* PCI */
 RADEOERRUPT_PIN                0x0f3d /* PCI */
_V_SA59                 0x156   0x0f3d /* PCI */
N_FP     0x1498
#define RADEO   0x0f3d /* PCI */
0x0250
#dUPT_PIN                0x0f3d /* PCI */
 defSH_DATA62                /* PCI */
#defi_WIDT    define R  (2 << 10)
#       define e RADEON_LVD   0x300_CRT              0x02d0
#       define RADEOdefine RADEON_HOST_DATA5              define RADEN_LVSEL_CRDS_DISPLAY_DIS      (1   <<  1)
#       N_HTOT_CNDS_DISPLAY_DIS      (1   <<  1)
#      ON_LVFP_SOUine RADEON_LVDS_PANEL_FORMAT     (1   <<PLL */

 DS_DISPLAY_DIS      (1   <<  1)
#      N_LVN_I2C_DS_DISPLAY_DIS      (1   <<  1)
#      ADEON_CNTL                0x02d0
#     fine    0x0efine RS400_TMDS2_CNTL      e RADR00aafine R300_CRTC_MACRO_T <<  6)
#     		        1 /* 0x64 - DVI_DDe RAD    		_NOW         (1 <<DS_EN        OST_DATA1   2    define RADEON_FP_PAN       define       definN_HDP_SOFT_RESET        (1 <     defi        _RGB332			7
#define_EN        (1   <<        define_PIN_SEL(x)      ((x) << 3)     defi   0x0f0x03c1 /* VGA */
#deADEON_LVDS_DIGON          defin   define RADEON_FP_CR        0_FP_2ND     define RADEON_LVDS_       0x14bfine RS400_TMDS_2ND_EN      14b4
#define RADEON_B  deONFIguessne RADEON_FP_TMDS_EN      14b4
#define RADON_LV(3<< 21)
#       define RADEON_LVDS_DTM_POL_LO               efine RS400_FP2_2_BLANK_ERTC2        (1  1   <<define RADEON_CRTC2_VBLANN_BRUSH_DATA16        ASK        (3 << 10)
#     N_BRUSH_DATA16   1   <<E_SEL_RMX         (2 << 1ATA17                RS400_FP_2ND_SOURCE_SEL_CRTATA17           1   <<  define RADEON_FP_CRTC_D        _SHIFT_1    #       define RADEON_LVDS_LP_POLR0EON_LVDECT_EN       (1 << 12)
#    (1   <<            _2ND_SEL            (1 <<RESET      )
#     ATE_SEL_SDR     (1 << 26) (1   << EON_GPIO_MA
#       define RADEON_LVDS_FPDI_R2EL_MASK     << 27)
#       define RADEON_L*/
#define RSHIFT        28
#define RADEON_LVR3EL_MASK              0x02d4
#       define      0x15e0ELAY_SHIFT     28
#       define R4EL_MASK  ELAY_MASK      (0xf << 28)
#           0x15e8EON_LVDS_PLL_EN           (1   << 5EL_MASK 
#define RADEON_HOST_DATA7ATA17          
#define RSHIFT        28
#define RADEON_LV0x0f3f /*1   <<ine RADEON_HDP_APER_CNTL              efine RS400_FP2_2_DVO2_EN                      efine RS400_TMDS2_CNTL    24              0x0394
#define RS400_TMDS2_TR          0x14e4
#def 0x03a4
#< 21)
#       define RADEON_LVDS_DTM_POL_    0x14e41   << T_TIMER         0x180
#       define R300_MC_DISP    (1   << 23)
                   0x15c
#define R300EN          (1                    0x15c
#define R300VDS_HSYNC_DELAY_SR300_MC_DISP1R_INIT_LAT_MASK  0xf
#define RADETL                        0x15c
#define R300 RADEON_HSYNC_DEne RADEON_MCLKA_SRC_SEL_MASK    0x7
#       dSYNC_DE                   0x15c
#define R300  define RADERADEON_FORCEON_MCLKB         (1 << 17)
#)
#      RADEON_FORCEON_MCLKB         (1 << 17)    (1   <<   define RADEON_FORCEON_YCLKB         (1L_MASK   RADEON_FORCEON_MCLKB         (1 << 17)00_LVDS_SRC_0)
#       define RADEON_FORCEON_AIC    define R3RADEON_FORCEON_MCLKB         (1 << 17)< 18)
#       (1 << 21)
#       define R300_DISABLE_   (2   <RADEON_FORCEON_MCLKB         (1 << 17)CNTL                   0x001f /* PLL */
#       defiVDS_PWRSERADEON_FORCEON_MCLKB         (1 << 17)ine RADEON_L RADEON_IO_MCLK_MAX_DYN_STOP_LAT (1 << 1fine RADEne RADEON_MCLKA_SRC_SEL_MASK    0x7
#       /* PCI */
#      define RADEON_IO_MCLK_DYN_ENABLE    (1 <  0x23c
                   0x15c
#define R3000x03ba */
#define RADEON_GPIO_MONID   RUSH_DATA22  	   0x0068 /* DDC interfaceN_BRUSH_DATA/* DDC3 */
#define RADEON_GPIO_MONATA37                0x006c
#de< 21)
#       define RADEON_LVDS_DTM_POL_          LAT_SHIFT 8  0x01b0
#define RADEON_MDGPIO_MASK            0xf
#       define DEON_LVDS_PLL_EN           (1   8
#define define R300AD_A		    0x019c
#define RADEON_MDfine RADEON_MCLK_CNTL        0x01b4
#define RADEON_MEM_ADDR_CONF   define RAD_A		    0x019c
#define RADEON_MD       define RADEON 0x0f10 /* PCI */
#define RADEON_MEM_CNTL define RADEAD_A		    0x019c
#define RADEON_MD)
#       define _MASK 0x01
#       define RADEON_MEM_8)
#       de_MASK 0x01
#       define RADEON_ME(1 << 19)
#             (1 <<  3)
#       define R300  (1 << 20)
#_MASK 0x01
#       define RADEON_ME         (1 << 2ONLY      (1 <<  2)
#define RADEON_MELKA        (1_MASK 0x01
#       define RADEON_MEE_MC_MCLKB      EON_MEM_INIT_LAT_TIMER           0x01             _MASK 0x01
#       define RADEON_MEfine RADEON_MC_MDEON_MEM_SDRAM_MODE_REG           0x0   define RAD_MASK 0x01
#       define RADEON_ME 13)
#       def   define RADEON_B3MEM_RESET_MASK    << 14)
#      0x0f10 /* PCI */
#define RADEON_MEM_CNTL << 15)
#define0)
#define RADEON_MEM_STR_CNTL             0
#define RAAD_A		    0x019c
#define RADEON_MDf4

#define RADEON_HEADER              ine RADEON_BR0e /* PCI */
#define RADEONAHOST_DATA0                   0x17c0
#define Rdefine R300_ATA1                   0x17c4
#defAne RADEON_HOST_DATA2                   0x17c8A#define RADEON_HOST_DATA3                   0xA7cc
#define RADEON_HOST_DATA4               define R300#define RADEON_HOST_DATA5                   (1 7d4
#define RADEON_HOST_DATA6     03
#          0x17d8
#define RADEON_HOST_DATA7 0x0f
#defin       0x17dc
#define RADEON_HOST_D   0x0150
             0x17e0
#define RADEON_define R300NTL               0x0130
#	define R */
#define _RD_CACHE_DIS   (1 << 24)
#	define03
#       _READ_BUFFER_INVALIDATE   (1 << 27) 0x0f
#definine RADEON_HDP_SOFT_RESET        (1   0x0150
      define RADEON_HDP_APER_CNTL  A      (1 << 23)
#define RADEON_HTOTAL_CNTL                  0x0009 /* PLL */
#       de     RADEON_HTOT_CNTL_VGA_EN      (1 << 28)
N_MPP_#define RADEON_I2C_CNTL_0		    0x0090
#defin       )
#define RADEON_I2C_HALT (1<<2)
      e RADDEON_ION_DAC_CMP_EN     SHIFT    RADEON_I2C_DRIVE_EN (1<<6)
#define RADEOND_INDEX        (1<<7)
#define RADEON_I2C_STARR          efine RADEON_I2C_STOP (1<<9)
#defi define R300_MCECEIVE (1<<10)
#define RADEON_I2C_ABOR 0x0f
#d)
#define RADEON_I2C_GO (1<<12) 0x0f
#d RADEON_I2C_CNTL_1                    0x0f
#ddefine RADEON_I2C_SEL         (1<<1 0x0f
#dne RADEON_I2C_EN          (1<<17)
# 0x0f
#dADEON_I2C_DATA			    0x0098

#defin 0x0f
#d_DVI_I2C_CNTL_0		    0x02e0
#       0x0f
#dR200_DVI_I2C_PIN_SEL(x)      ((x) << EON_OV0_AUTdefine R200_SEL_DDC1              EON_OV0_AUT - VGA_DDC */
#       define R200_AEL_DDC2                1 /* 0x64 - DVI__OV0_AUT       define R200_SEL_DDC3           _OV0_AUT* 0x68 - MONID_DDC */
#define RADEON_D_OV0_AUTNTL_1               0x02e4 /* ? */
#d_OV0_AUTDEON_DVI_I2C_DATA		    0x02e8

#definA RADEON_INTERRUPT_LINE               0x0f3c /* PCI0_AUTO_FLIP_CADEON_INTERRUPT_PIN                0x00_AUTO_FLIP_C
#define RADEON_IO_BASE             ECT     0x00004 /* PCI */

#define RADEON_LATENCYECT     0x0000        0x0f0d /* PCI */
#define RAECT     0x0000_DEC                0x1608
#define ECT     0x0000ES_LNTH               0x161c
#definECT     0x0000BRES_LNTH_SUB           0x1624
#def534
    0x00< 2)
#       define R300_MC_IDLE   e RADEON_LVDS_RST_FM           (1   <NTL           define RADEON_LVDS_EN NTL           (1   <<  7)
#       defiNTL    define RADEON_LVDS_BL_MOD_LEVEL_MASK       0x0408
       define RADEON_LVDS_BL_MOD_EN         0x040< 16)
#       define RADEON_LVDS_ON_EXCL_HORZ_    (1   << 17)
#       define RADEON_      0x040           (1   << 18)
#       de00ff0000
#   VDS_BLON                     0x0055efine 0x008800 def00a00000
#       define R (0xONFIalways have x    (1        0x00500000
#         1<<  (2 << 10)
#       def0x00f0     deT_EN       (1 << 12)
#        define RADEON_D  define RS400_PANEL_FORMN  (1V define RAD    dedefine RADEON_CRTC2_VBL 8)
#ASK     SUB           0x1624
#dN  (1RIGHT 00a00  de                 0x1438EXCLDISCRETE RADHORZ_Yfine RADEON_ROP3_DSon   ne R  0x0
# VE_V RADEON_GMC_WR_MSK_DIS              0x2
#ADEON RADEON_GMC_LD_BRUSH_ TER_HC_COE2_GRPH_OTRADESENE_EN (1<<6)
#defineefine RADEOPADEONA    define RADEON_Lefine RADEOFPDEON_Lne RADEON_CRTC2_GARDCODED_COEF    AVDS_BL_MOD_LEVEL_SHefine RADEON_FILT
#    S_BL_MOD_LEVEL_SHIFT 8ADEON_FILTdefine 		ine RADEON_ROP3_ZERFOUR_TAP_COEF_     de		00    (0    << 15)
FOUR_TAP_COEF_T		16
#		     (1 << 18)
#   FOUR_TAP_COEF_2		0x01		1 << 23)
#define RAFOUR_TAP_COEF_TYPE_RG		1 /* PCI */
#defineFOUR_TAP_COEF_   defi		MS_MASK            FOUR_TAP_COEF_ define		2          0x04B4
#define RADEON_)
#    		2 0x0068 /* DDC interfa1 << 4)     de17)
#       define RADEO)
#   ADEON_OVdefine RADEON_FILTER_HC_COEWx0d44
#d                  0x1708 8)
#F        4               ine RADEON_       0ay */
#define RADEORADEON_OTEX    MON_ROIFT 0x00000003
#       define       d8
#0x0e00
#defin          0x04B4
#define RTEXIOS_0x0e00
#definON_DST_PITCH_OFFSET_C A_100_1ALE_0x0e00
#defin RADEON_OV0_FOUR_TAP_COEF_TEX0_FP0x0e00
#defin         1 /* 0x64 - DVN_OV0_G10)
_180_1BF      R_SIZE_MASK       0DAC2_CMP_OUTPUT    RADEON0 0x0218
#def0e14
#define RADEON_OV0_GA1MA_200_Z_STRETCH_BLEND
#define RADEON_OV_GAMMDEON_RADEON_MASK        0x000003_CMP_OUe RA     define  RADEON_EXCL_VERT_    << 2COEF_0N_FILTER_HC_COEF_HORZ_UV                 0       define RADEON_FILTER_HC_CMP_OUdefin       SUB           0x1624
#dV0_GAMMA_34                         0x1438V0_GAMMA_34_V_SY<<       0x02ec
#       deV0_GAMMA_34N_FP_<<SEQ_DELAY1_SHIFT     16
V0_GAMMA_340x02N_OVADEON_OV0_GAMMA_2C0_2FF            defi<<
#define RADEON_HOST_DATPHICS_KEY_CH_1    0x3f            0x0e2c
#define RADEONdefine RADEON_FILT         0x0130
#	define_CMP_OUP_COEF_HORZ_YFER_INVALIDATE   (1 << 2_DISP_D1D2_SUBPIC_GAMMA_040_07F          (0  DPx         0EN       0x028c
#defiRTC_I  defi_TRANS_Ux03c8
#define RADEONEON_VIDEO_       deUE    0x00000001L
#       defi 10)
#   UE    0x00000001L
#       defi (0 << 10UE    0x00000001L
#       defi       (1UE    0x00000001L
#       defi_RMX     efine RADEON_FPine RAD    0x00
#      1            0x00440000
UPD  0xUSEx1674
# << 2)_DISP_7F      
0000/* Registers for CP and Microcode Engyrig 0x06
#       defiCP_M  27MH_SYNCEND (1 << 17)
# 07x100000
#define RA    defineR  RADEON_GRAPHIC_KY_FN_
#ifndef _RADEON_    define    0x0008
#       de07ON_CAP_PTR_MASK   0100L
#        RADEON_BRUSH_DCMP_e0
#       de     0x00RBTL		 _SYNC_SEL          (107 R300_CRTC2_TILE_X   0x0       define RADEON_CR_OV0_4ifornia.
 *
 * Al0x00UFSZ25)
#define_TRANS_B              0x           0254
#  _TRANS_B           LK 0x0d24
#d80d28
#define RADEON_OV0_LIC               of ornia.
 *
 * Al     WAPx00050OST_DATA2    ornia.
 *
 * Al5a0
FE def        _D              0xF                   x0028
#  _TRANS_B          NO  RADEO     define R_TRANS_B          RRTC2WTH_MA6)
#     D REGISTER DEFINIT   0x0 0x00fine  RADEON_CMP_MIX_M          0x10 /* _ACTIVE_LI0d20
#define RADEON_OV0_COMMAND             0x0W1_H_ACCUM_INIT          0x0NE    0x00000030L
# K   0x0000CUM_INIT          0x0#       defiWIDTH_X1RA defUM    DEON_OV0fine RADEON_03L
#       de_SYN  RADEON_OVS           6e RA  0x00000100L
#define RADEONON_OcC_BRUe RADEON_OV0_P1_X_       define RADEON_CRTC4
#deUT_CNTL           _OV0       0(x)e RADEON_DISP_S_ATRS400_FP2_2_DVO2_EN   3_BLANK_V0_LS_AT_TOP   0x0434
#      of the Software.
 3_BLANKfine  RAD     define RADEON_DISP_PWR_MAN_DPMS_O3_BLANK 0x00000fffRADEON_DISP_D3_RST  ne RADEON_OV0_P2_X_EON_OV0_P1_MAX_LN_IN_PE498
#de
#ifndef _Rine RADEON_OV0_  RADEON_GRAPHIC_KE98
#dene ATI_DATAine RADEON_OV0_P3_X_H   defin 0x0498
#d_COMMAND    _OV0_P1_X_P1_V_ACCUM_INIT               
#       de    define  RA_P3_X_START_END           RADEON_CUR20x00000001L
#      LOAD_CNTL            0x049C
#define RADEON_OV0UV444             (14  80_LIN_TRANS_A0x00000001L
#  DELADEON_COMPOSITE_000000S                 finex00000100L
#define RADEON_OV0efine RADEON_CUR2_L
#       0efine  RADEON_REG_LD_CTOUT 0x00000003L
# CP_CSQ_START_END           0x_OV0RADEON_CLOCK_CNTL_DATA          3)
#    RADEON_CRTC       (0   << 16)
#       defineEAREPRIDI    DDICNTL      ine RADEOM (1 << 15)
#       defineK_NEARPIO    0x00000008L
marti   define  RADEON_SCALER_SIGNED_UV  BM    0x00000008LON_AUX1_SCdefine  RADEON_SCALER_SIGNED_UV         BM          0x1#       define  RADEON_SCALER_GAMMA_SEL_MASKIGHT     0x          define  RADEON_SCALER_SIGNED_UV         PI(1 << 5)
    (1 <<VYUY_422			11
3OV0_P1_ESYNC_P3_X_START_END       0x7   (1 << 13) define  RADEO       define RADEOG14   V0_SCALE_CNTL             RADx0420
#       define  DEON_PLL_DIV_SEL          SQN_OV0_  0x00000004L
#      defiine  RADEON_SCALER_VERT_PICK_NL
#  000f00L
#       define  RAefine  RADEON_SCALER_SIGNED_U 0x00INDI< 12_DISP_LIEV_A13       (2   << 16)
#       de        0  0x00000400L
#       def_PCIE_DATA               20080L
#       define  RADEON00L
#define RADEON_O    N_SCALER_GAMMA_SEL_G1RADEON488
#define RADEON_O    define  RADEON_SCALERRADEONNE    0x00000030L
# EON_S8)
#)
#           define RAUY422       0x00000B00L
#     0x0000ne  RADEON_S3  8)
#       defineine  RADEON_REG_LD_CTL_LOCK_R  0x0N_CRTC8_DATA            ON_CEON_COTIMEC2_PIX_WIDISP_TV_PATH_SRC_CRTC2 (1 ER_TEMPORALIM    TRACT, TO3_SCALE_CNTL            _TOTROP3_7    ornia.
 *
 * Al  0x0000RCE_Y    defidefine RADEON_OV0_L_SMART_SWITCH   ECEIVE (1     0x0d28
#define RADE  0x00001ITCH         _D              0x7F0000L
#       _SCALER_BURST_YUY_422			11
#definAIC_START_END           0x0498
01    define RADEON_CRTC_DIPCIGH   TRANSLefine d90
#define010c
#define RADEON_CONFIGIS def_OFne  _RADEOACCIFT        0x0208
ornia.
 *S43_DPSI_REARM RADE    0
#       defineONFIrs400/rs480   0x00000020L
#   DEONLO_SCALER_SOURCE_YUV12      1ON_CAP_PTR_MASK   DEONPTTL		 #definASK         0x0000DEONHI  0x01fFT_R_CMP010L
#   Constant     /* ISP_TVDAC_SOURCDEON_FNK_S_INTefine  RADEON_SUM_INITGUI       deREG     /*                   */
# 04F8
#define RADEON_OV0_V_INC            2#    484
#definP packet type      DEON_SCALER_ADAPTPACKEefine RADEON_DAC_F< 26)
#define RADne R300_CRTC2_TILE_Xine RADEOSHADOW_VEND  (1 << 1811   <<  8)
#            define  RADEON_VIYNC_CONTROL_EN (1 <<        (12   <<  8       define  RADEON_VIYNC_SEL          (1 <         C  define RADEON_GMC_DST_VYUY  ine RADEOPIXEL 1
#       defin03fffff0L
#       define  RADEON_VIF_BUF0_1ADEON_DISP_LIN_TRANS    fff0L
#       define  RADEON_VIF_BUF0_1STX     0DEON_GRPH2_STA            0x155c
#define RADne RADEON_INTERRUPSE_ADRS       0x047N_CRTC2_GEN_CNTL    defineADRS          0x00000002L
#DLL_READY  OUT_CNTL          _OV0_P1E_ADRS_MASK     0x03fffff0L
#      EON_CRTC2_GEN_CNTL         RADEON_VIF           0x0444
#        define  RADEON_VIF_BBASE_ADRS       0x04_MC_AGP_TOP_MASKe  R3ff#        0x0464
#define RADEON     INTEV0_P1_MAX_LN_IN_PER_L      _VIF_BUF2_TILE_ADRS        E_CLDEON_DISP_GRPH_ALPHSK 0x480000SCALER_SOURCE_YVYU422  SK     0xEXHADOANS_BYPASS (0x01 e  RADEON9L
#       define  RADEON_VIF_PLY0x480SC_SRC_DATAdefine  RADEONDBUF3_BASE_ADRS       0x044C
# 27)
LE_EN_T         0
#   RADEONEBUF3_BASE_ADRS       0x044C
#)
# NDR_ */
#dDX     DEON_OV0_VI2ne  _BUF2_BASE_ADRS_MASK     0LOADFP2_DVC 28)
#       de#definedefinBUF2_BASE_ADRS_MASK     0WA_CMD_COMD  0x146c
#     #define6454
#define RADEON_OV0_VIDEO_KEYDRAW_VBUFF5_BASE_ADRS       02#            0x0404
#define RADEON_OV1IMMEON_CLR_CMP_MAS       _BUF3_BASE_ADRS       0x044C
#_END     NDSCREEN_BLANKING       ARADEON_OV0_VIDEO_KEY_CLR_LOW      PALETVE_LINES_M1    RADEON_OCx00a50000
#     EON_OV1_Y_X_END        SYNC_CONTROL_EN 0xc    5ne RADEON_OVR_CLR                    VBPNTALL_GUI_UNTIL       FRADEON_OV0_VIDEO_KEY_CLR_LOW E   0PAine RADEADEON_CAP0_BUF1_O9define RADEON  0x0924
#define RADEOBITB << 16)
#deN_OFFSET     ADEON          0x0924
#define RADEOSMALL (0xF0_EVEN_OFFSET      RADEON_OV0_VIDEO_KEY_CLR_LOW E   0HOST    _OFFSET      ET     Y_X_START                0x040RADEONOLY  define R 0x092C

#defiN_CAP0_BUF0_OFFSET           0_CAP0_VBIADEOAPERine RADEOET      0x0600
#define RADEON_OV1_Y_XRADEON_CAP7ff <AD_CNTL    ET     VR_WID_LEFT_RIGHT           0xF1_EVEN_OFFfine RADEON_CAPET     BINDOW          0x0948
#define RADEN_SCAVEN_OFFSET      ET     OTTOVIF_BUF2_TILE_ADRS  VC_FRne RADEON_GMC_0_BASE_ADRS       0x0440
#       define  RAD                     0x0280ine RADEON_GMC_BRUS                0x0958
# 
#       de0_BASE_ADRS       0x0440 VGA, 0x3b4 */
#dx00000001
#  define RADEON_CRTC2_V_T       define_FIELD_EVEN   0x00000002
A   (7    <0_BASE_ADRS       0x0440L_FLIP_READBACK     000001
#  
#       define N_CAP0_CONFIG_CO0488
#define RADEON_O000001
#   0x0d68
#       def2_MONO_FG_LA  (9 T_BUF_GET      0x00000004
##       define RADEON_CAP0_CONFIADEON_DAC_RANGE_CN   0x000000define RADEON_DAC_FEON_GMC_BRUSH_1X8_CONESHOT_MODE_FRAME 0x000000F_BUF0_PITCH_SEL        P3_PDna     ONESHOT_MODE_FRAME 0x0000ADEON_DAC_FORCE_DAT      define RADEONONESHOT_MODE_FRAME 0x000000F_BUF0_TILE_ADRS             define G_BUF_MODE_TRIPLE    0x000F_BUF0_TILE_ADRS         <<  8)
#    ONESHOT_MODE_FRAME 0x000000F_BUF0_BASE_ADRS_MASK   0000
#       define DE_TRIPLE    0x000F_BUF0_BASE_ADRS_MASK              (4G_BUF_MODE_TRIPLE    0x000     define RADEON_CAP0_CONFIG_      ONESHOT_MODE_FRAME 0x0000e RADEON_GMC_DST_32BPPCAP0_CONF       ONESHOT_MODE_FRAME 0x0000ADEON_GMC_D
#       define  RAD   defiONESHOT_MODE_FRAME 0x0000 4)
#       define ne RADEON_GMC_DST_YONESHOT_MODE_FRAME 0x0000      define RADEON    define RADEON_G                0x0958
#  NE  0x00020000
#       defi   define 0_CONFIG_VIP_EXTEND_FLAG_ENE  0x00020000
#       defi000001L
#       define  RADE_MORE_LIUF0_TILE_ADRS          0x00000002L
#ON_CAP0_CONFIG_HORZ_DIRADEONMC_DST_ARADEOefine RADEON_CAP0_CONFIG                0x09IVIDE_2      0xefine RADECAP0_CONFIG_CONTINUOS          0x00000IVIDE_2      0x0_OFFSET        CONFIG_START_FIELD_EVEN   0x0000AT_BROOKTREE   0x00MC_CONVERSIO000
#                0x000040FORMAT_CCIR656    TRI      define _CONFIG_START_BUF_GET      0x000RMAT_ZV          0xC_SRC_DATA       defiDEON_BIST        FIG_FORMAT_ZV          0xMC_DST_DATAT000
#    _CRTC_AUTO_VERT_CFIG_FORMAT_ZV          0xST_ARto any 000
#    7e RADEON_CAP0_CONFIG_FORMAT_VIP      00000           0_CONFIG_START_BUF_SET      0x00IVIDE_2      0x3VRTRGB4444     000
#    9efine RADEON_CAP0_CONFIG_VIDEO_IN_VYUY422 _DST_DATATY000
#    aefine RADEON_CAP0_CONFIG_VIDEO_I#       ine RADEON_CAP0_CONFIG_BUF_TYPE_ALT       0x0NFIG_VBI_DIVIDE     define RADEAP0_CONFIG_BUF_TYPE_FRAME     0xNFIG_VBI_DIVIDERIN8
#define RAD000
#   
#       define RAFIG_FORMAT_    << 27)
#BGRACCUM_INIT BRUSH_1X8_MO      0x0964
#define RADEON_CAP0_VIDE       define CAP0_CONFIG_ONESHOT_MODE_FRAME 0E   0MAO1<<31)
#0_BASE_ADRS       0x0449                 FIG_FORMAT_             1    << 28)
#    0x0968
#define RADEON_CAP0_ONESHOEXCLUSIVEON_CAP0_XSHARADEON_CAP0_CONFIG_BUF_MODE_TRIPLE   RIG_CNC_SYNC_STRT_CH000
#       define RADEON_CAP0_CONFIG_VERT_DIVID    defineefine RADEON_CAP0_CONFI_CONFIG_MIRROR_EN        E   0         define RADEON_DPfine RADEON_CRTC_GUIV    fine 0_SCALER_SOURCE_YUV12   ISP_TROL         0x0900

/* 1econd capture unit */

#TART_BUF_GET     x0900

/* _I2C_ALL_GUI_UNTIL_FLIP 0
#ifndef _RADEON_x0900

/* 3_SCALER_SOURCE_YUV12       DEON_CAP1_BUF0_EVEN_OFFS4T      0x0998
#define RA_CRTC_AUTO_VERT_Cx0900

/* 5_SCALER_SOURCE_YUV12   IG_BUF_TYPE_FRAME x0900

/* 6  0x09A0
#define RADEON_ine RADEON_CAP1_BUF1_OFFSET7  0x09A0
#define RADEON_ RADEON_CAP1_BUF0_EVEN_OFFS8_SCALER_SOURCE_YUV12   3DEON_CAP1_BUF1_EVEN_OFFSET 9#define RADEON_CAP1_VBI_ON_CAP1_BUF_PITCH          1second capture unit */
ADEON_DAC_RANGE_CN      0x09BSET           0x0990
#dADEON_DAC_RANGE_CN      0x09B           0x0994
#defiADEON_DAC_RANGE_CN      0x09BET      0x0998
#define 5DEON_CAP1_BUF1_EVEN_OFFSET 1     0x099C

#define RA5INDOW                  0x09B   0x09A0
#define RADEO      define RADEOVS       d_POSN           0x0994
#
#define RADEON_BR    0x09C                 0x0994
#7DEON_CAP1_BUF1_EVEN_<  8)
DATA16            0x0994
9                  0x09D0
#deRPO_H_SYNCx0994
#define TRIG_CNTL           <  8)
HWVGADJRADEON_CAP1_ANC_H_9ON_CAP1_BUF_PITCH         dne RUASP_LIne RADEON_CAdefinG_CNTL             0x09CEYE2ADEONne RADEON_CAP11ADEON_CAP1_BUF1_EVEN_DEON         0x0994
#defineSHARPON_CAP1_BUF_PITCH   GLOBVERT_POSN           0x09941* VGA, 0x3b4 */
#dVS RADELANK_
/* misc multimedia r1             0x000VS     VE_Y_1          0x0994
#de1US                 S0x09D0
#dCD_SCALER_SOURCE_YUV12    e RADEON_IDCT_AUTH_CONTROne RAEXPON                (0x1f  << 16)
#                CUTOFF           0x1_DISP1_SEL        (     0x09D4
#defiTHRES   0x0)
#      RADEON_IDCT_AUTH_CONTROe RADE   0x002a /* P2PLL _EVEN_OFFSET      SSN_DEFAGUAR     RADEJ           0ADEON_DAC_RANGE_CNT     define Rdefine R_P2PLL_PVG_M4 define RADEON_CASS_HORZefine RADEON_P2PLL_PVG_MASK
#define RAD        11
#       define RADEON_P2PLL_P5TINUOS          0S0000
0x0
#       define EON_CAP1_ANC<<  0)
#       dTdefiS)
#	       define RADEON_CRTON_GMCUT_CNTL             0x0dV_ARADEOR   define RADEON_Dnse, and/or sell copies of the CSCAL  0x002c
#       defin     define RADEON_DAC2_FORCE_DA  0xH   PHASE_FR300_CRCI */
#define RAornia.
 *
 * AllV_FIFOf
#       		define RADEOornia.
 *
 * AlVIN/
#       define RADt toornia.
 *
 * AlAUD/
#       define RADce aornia.
 *
 * AlDVS/
#       define RAD<< 22)
#       define RADERTPLL */C_LIN_TRANORCE_DATA_MASK   0x0003ff00
#       defin PLL */ same as _R */
#  HIFT  8
#define RADEON_DAC_MACRO_    ADEONOW300_MC_INfine RADEON_CRTC2_OFFSET__OFFSET_LOCK	 TVCLKTICAL_C_ONb RADEON_DISP_D3_RSalifornia.
 *
 * AllV_ON	define RA  define  RADEON_P1TVne  RDAC_MUXSYNC  (1 << 18)
#defON_G         0x040C
#  t 2000YINTE0x3ff << 18)
#   _TRANS_BYPASS     0x10000000L
#     C_GRTC_M                0x16c4
# (3 << 24)
#       define RN_FCLU  define RADEON_PIX24MB    (0x30 << 0)
#       defi    K_WIDTH_Mfine  RADEON_SCALER_ENe R300_PPLL_REF_DIV_ACC_SD_MX003L
# e RAD      defK_CNTLTH THE SOFTWARE OR THE US    IX2CLK_SRC_SEL_BYTECLK  0x02

#       define RADEON_ACC_UK_SRC_SEL_P2PLLCLK 0x03
#     NCLUDING BUT NOT LIMITED TOV2CLK_SRC_SEL_BYdefine RADs */

#define RADETVPv3_8N_GMC_DST_AYUV444             (ON_GMD_MASK   (0xf << 16)
#defW /* VTOR       define RADEON_SOFT_RESET_GRPH_PP v3_8K_WIDTH_Mb (1 << 9)
_LC_LINK_WIDTH_X4        3    RC300_MC_IND (1 < 9)
#define RADEON_DISP2_MERGE_EON_PIXCLK_CRTC1D_ALWB_OFFSET_EN    (1 << 8)
#      ne RADEON_PIRMXb (1 << 9)
b  (1 << 11)
#       define RADEON_PIXCLK2D_ALWDEON_CRTC_H_TOTAL_SHIFT    0
# V_SRCO<< 16_BY_ADEO (1 << 9)SHIFT   0
#       define RUV   deE    ARGIN      000+ only */
#define RADEON_AIFOLS  FFN_AGP_
#       define RADEORT OR OTHERWISE, ARISINGne Rne RAADEOS_AT_TOP   0x044
#     
#       define RADEON_DISPVfine     0x0 0x002c
#       define RADEON_SYS_HOTKEY      1 << 16)
#VA     define R300_PIXCLK_T   0x0108
#define TVHIFT  N_GMC_DST_AYUV444             ON_GM_EN              (1 << 25ADEOORT_END           0x049 RADEON_P2PLL_FB0_DIV_MASK    0xfine RUDST_X_DIR_LEFT_TO_RI_P2PLL_POST0_DIV_MASK  0x00070000fine Idefine RADEON_PIX2CLP        (1 <<  6)
#       define fine PUON_ACC_REQ_DFP1    */
#define RADEON_DISP_MISC_CNTL  PMI_CAEON_CLR_CMP_MASKefine RADEON_DAC_FORCE_DATA_SEL_MASK (3)
#    IO_DRIVE_LINES_M1         1 << 
#define RADEON_PHTO       define  RADEON_GRAPHIC_KEY_F8          0x10 /* ne RDIS3fffff0L
#       define         0x0_COMMAND          ne Rdefin_OV0_VID_BUF0_BASE_ADRS        0x0
#ifndef _RADEON_ne RADEONON_PMI_REGISTER                 0xC /* PCI */
#define VADEON_PMI_PMC_REG                  0x0IG_BUF_TYPE_FRAME     RADEON_PMI_PMCSR_REG                0xP1_H_WINDOW           e RADEON_PPLL_CNTL                    VBI_ODD_OFFSET    WAYS   define RADEON_PPLL_RESET            5e /* PCI */
#defineFe RADEON_PPLL_CNTL                    
#       define RAADEO
#definATOMIC_UPDATE_EN     (1 << 16)
#  (1 <<  1)
#           L_VGA_ATOMIC_UPDATE_EN (1 << 17)
#  (7 << 11)
#       deVLL_ATOMIC_UPDATE_VSYNC  (1 << 18)
#defi5e /* PCI */
#define O OUT#   L_BYTECLK                 0x0ADEON_DAC_RANGE_CNIV_1    EON_CO          0x0005 /* PLL /
#de     define RADEON_PP     D_WMC_DS  (1 << 18)
#define RADEOADEOornia.
 *
 * AlV_3  LL */RDdefine RA< 17)    define RADEON_PPLL_FB3__ACND_Aine RADEON_    define RADEON_PPLL_FBW define RAC_SELSK   0x00070000
#define RAD_PPLL_POST3_DI5d /* PCI */
#define V#    VSYNC NE  0x00020000
#        */
#ON_DAC_CMP_EN           UVEN  1ST_LINE_LSBS_MASK 0x48EON_CRTC2_GEN_CNTL         E_R  (1     define RADEON_D_P2PLL_DIV_0            Y_WNb   _POST3_DON_CRTC_CUR_MODE_SHIFT   2LL_ATOM_FIELE       (1 << 2)#       ABLE estart on field      0/* same as _R */
#definEON_WRADEYPE_RGB888			5
#dfine RADEON_PIXCLK_TTIMN_CRN_GMC_DST_AYUV444            0x0             0x032c
#defiHR  (1 << 15) /* same as __W */
#       define RADEON_HOST_DAfine RADEON_TV_DAC_BDACPD         (1 <<  26)
REQ_Y_FI02c
#       defineT   (1 <<  0)
#       define RADEne RADBUR<<29)
#       defiND          (2 << 24)
#       deUVON_D#       #	define RADEISP_HW_DEBUG                0x0dUV define      (1 << I_REV_ID_Mefine RADEON_PPLL_REF_DIV_MASF_BUF0_TILE_ADRS       << 0)UT_CNTL             0x0d     2
VBI2_OFFSET   0x0280
#       define RADEON_DAC2_FORCYADEON_OV0_ON_PIX2CLK_SRC_SL   0x1820
#define RADEON_DESTINA  (1 <<     define RADEON_
#define RADEON_DEVICE_ID                 ine RADEON_SOFT_RESET_HD 0x0e RADEON_PPLL_DIVY_FA#   N_HOST_DATA_SWAP_16BIT  (1 << 0)RT_PAGE                0x0ne RAPN_CRPODEON_CAP0_ANCe RADEON_PLL2_DIV_SEL_MASK     (Y_COEF  define RADEON_PIX2CT_DATA2    0x0e40
#       defiRI4
#       define RADEON_CRTC_ RADEONON_DAC_CMP_EN           USH    IVE           (1 << 31)
#define0x0e40
#       defiSAIAN_OTH   0x0d20
#define RADEON_OV8      define RADEOTV_UPSDEON#defGAe RA0
#define RADEON_PCI_GON_Fornia.
 *
 * AlYfine RAb     RADEON_P2Pornia.
 *
 * AlUVEON_DSTCACHE_CTLSTA_2ND    (1 << 31)
#de_RB2DSCALER_ETADEOT         0
#    SY   RT_PAGE                0x0     0x3250
RADEON_CRTC_AUTO_HORZ_CENTER_EN (1<<2U       0x3250
fc
#define RADEO    (1 << 31)
#de#define_RB2D
#define RADEON_RB3D_ZCSY   ON_RB2D_DC_FREE          ( RADEOC_UPDATE_W  (1 << 15) /* same as _R */
#define RADE_DC_2D_CACHE_DISABLefine RADEON_PIXCLK_TMODULAEON_AMASK     0x03ff
#       SY  V_DA3428
#define RADFL_LC_DEON_PWR_M3D_ZC          0x1714
_RB3D_DC_2D_CACHE 0x01
#       define RADEALne RADEO define RADEON_PIAC_FORCE_DATA_SEL_G    (1 << 6)
fine T0x1408
# RADEON_DAC_FORCE_DATA_SEL_B    (2 << 6)
#   BLANK2c
#define R  0x1724
#define RADEON_CACHE_LINETATYU42c
#define RHE_DISABLE   ornia.
 *
 * AlSLEW_0
#  0x325EON_PWR_MN 0x01
#       define RADECSET LFAULT_SCTOM_RIGHT      0      (2)
# define RADEON_RB3D_DYNC_CONTROL_EN (1 << 2008PUT_CNTL             0x0d#def       408
#definITCH    #       define RADEON_HOSPLL_RB3D_DC_DISABLE_RI_READ         (1 << 25)

#define RADEON_RB3D_DSTC#       define RADEON_PIXCLK_TCSH_D_SEL        (1 << 8)
#       defin 0x0218
#define RA#defV_A misc multimedia registe_FILL       RAD
#define RADEON_OV0
# def_BLEND_A000
#   #         0xf
# define RADEON_       15) ornia.
 *
 * All (Te1_B RADEN_RB3D_ine RADEON_P2N_REG_BASE                    ine RAD       define RADE     3#         0x0f18 /*  define    0x0f09 /* PCI */
#define Rine RADine R200_RB3D_DC_3DH4E4
PCI */300_MC_INDC_BU6x0440
# e RADEON_SC_BOTTOM             ine RAD0F  P2PLL_REF_DIV_MAOM                    01GMC_DST_Y16f0
#define RADEON_SC_BOTTOM_R        7         0x002B /* PF    L */
0x01ff0x1a            0x1640
#define RADEON_    ER RADAD      TTE_INDEX            d RADE2)
#	dFFSET    20#   PLL   0x00000020L
#          # define RADEON_RB3D_DC_FREE      021C_TOP_LEFT     (1 << 25)

#define RAM0L#defin5) /* same as _W */x1c88
#       define RADEON_HIPIXEL 1
#       defi7)
#define RADEON_I2 RADEON_SC_SIG_DC_3D_CACHE_DISABLEIS", WITHOUT WARRANTY OF V_N_SC_SIGN_MASK_LO       0x        (1 << 25)

#define RAEON_M_DC_2D_CACHE_DISABL000a /* PLL */
#	define RADEOSIGN_MASK_HI       0x800     (1 << 26)
#define RA     8
_DC_2D_CACHE_DISABLMS_MASK             (3 <<        0x0030 */
/* #definV_OUT_CNTL             0x0dN_SP_DC_2D_CACHE_DISABLE      0x16e4
#define RADEON_V_S     (EEN_BLANKING       (1 << 26)
#       define RADEONV_DTR_ID       0x342c
#       deAYS_ONb     (1 << 19)
x16ec
#dNE  0x00020000
#       deC      2         0x1c88
#       define RADE    RTC2_VSYNC_T<< 0)
#       define RADEON_DAC2_FORCE_DAT     SL_AT_SOF             0x0f5c /* PCI */
#define RADEON      deF
#deRADEON_DISP     0x0f63 /* PCI */
#define RADEON_PC                  0x000000a /* PLL */
#	define RA    IXEL 1
#       define _I2C_CNTL_1             ON_SPLL_PV /* PCI */
#define REDESTAL       (1 <<  2)
#    K    000d /* PLL */
#       defi define RADEON_DAC_PDWN_R  VPDTA_SWAP_HDW    (3 << 0)ODE           0x04
#       0x00IXEL 1
#       define RADEO_DATA                 0x00b4    T_REQ       0x000                 0x03c0 /* VGA */
#db4
#de_PIXCLK_<15)
ALETTE_30_DATA              00000RADE2FT_RE3D_DC00000ex03c1 /* VGA */
#de7)
#       defineT_REB3D_DC_FLUSH  LK_FORCE_DISP1      (1<<18)
#       define RADEON_BRU     K_FORCE_DISP1      (1<<18)
#  One  efine RADEON_SCL (1    << 15)
#     20)
#       define RADEON_DEON_BRUS_FORCE_E2         (1<<20)
#      ALLOW_FIDfine RADEON_SCL* VG    (1<<22)
#       define RADEON_SCLK_FOREON_SCLK_FORCE_   (1<<17)
#       define RERVED e             (1<<21)
#       dCRITIRTC_R_HC_CO      define RE_SE         (1<<21)
#       d define RADEON_SCLK_FLK_FORCE_IDCT       (1<<22)
#       define define RADEON_SC      define       (1<<23)
#       define R_FORCE_RB         (1x000c
#defin  (1<<24)
#       dMIF_MEMine RADEON_SC        0x040C
#    (1<<18)
#       PCI */

1 << 15) /* same as _R *ON_SCLK_FORCE_OV0    EON_CRTC2_CURORCE_E2         (1<<20)
#    1  define RADEON_SCLK_FORCE_TAM        (1<<26)
#       definCE_SR           (1<<25)LK_FORCE_IDCT       (1<<22)
#       defiCE_SR           (1< (1<<28)
#       define RADEON_SCLK_FORCE<27)
#       define R3      define RADEON_SCLK_FORCE_#    define RADEON_SCON_DAC_CMP_EN     <<28)
#           define RADEON_SCLK_FORCE_TOP        (1<<19)
* PLL */
#       dEON_SCLK_FORCE_E2         (1<<20)
#    * PLefine RADEON_SCLK_FORCE_SE         (1<<21)
#      1<<11)
#       de define R300_SCLK_FORCE_TX           (1<<2 RADEON_SCLK_FORCE_VIP        (1<<23)
#       define)
#       define REON_SCLK_FORCEEON_BIST         CI0_AU define RADEON_DPne  RA          0x000b /5)
#          0x0005 /* e  RAE RADEON.  A FULL 5)
#TXON_SCA8
#deine   0x1c8c
#define RE_MAX_DYN_STD_DCdefine RAD07
#       define RADEON_SCLUNMAPPN_ATER_INfine     U      (0EON     0x0700
#define RADEON_SDRAM_MODE_REG   EON_DSTOMU              0x20000
#define RADEON_SDRAM_MODE_REG   efine Rne RADEON_EON     0x0700
#define RADEON_SOUBLE32_128_CACHE            RADEON_SNAPSHOT_F_COUNT         8_4  0x0244
#dedefine RA07
#       define RADEON_SCLCHK_RWdefiIc
#define _V_TOTAL_D07
#       define RADEON_SCLINefine_AUTTLBRADEON_I2C_7
#       define RADEOefine RA RADEDR /* 0xine                 0x15b0
#define RADEON_HIC_PIDEON_CAP1_BUF1_EVRE_MAX_DYN_STL
#deefine RADEON_SCLK_MORE_MAX_DYN_STSCLK_FLO#defne RADEON_SCLK_MORE_MAX_DYN_STSCLK_FHI#defDEON_BIST         e RADEON_SCLK_0920#defE           0x325fine RADEON_SRC_X      efine RADEON_CAP0fine RADEON_SRT_WID    RADEON_ROP3_DSa              defin5bits of CAP_PTR */           OV0_G15ine RADEON_V#define RADEON_SR2_Y_X   (0x1f  << 16)
#             3_Y_X   64
#define RADEON_          4_Y_X    0x00000A00L
#   efine RADEO5_SUBPICS           V530_GB_PI_TO_#    to any perso0x4efine endif
