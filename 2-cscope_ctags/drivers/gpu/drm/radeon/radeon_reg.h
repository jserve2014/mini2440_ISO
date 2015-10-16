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
 *define RADEON_TXFORMAT_DXT23*
 *  nc., Mar(14 <<  0)
#c., MarCopyright 2000 ATI Technol45 Ic., Markkarkha5, Ontario	*                VA LinSHADOW16stems Inc.,ha6nt, California.
 es InAll R    s Reserv32.
 *
 * Permi7nt, Calif, ands Iny of this softwVA LinuUDV88temsstems IFrem8btaining
 * a copy of this software andL ass65ysiated docu(19ion files (thes In"Software"), to deal in tUV88ociated doc(20btaining
 * a copy of this software and VA LinMASKthe      31to use, copy, modifsellerge,s Inpublish, distrSHIFTc., Mar0g
 * a copy of this software andAPPLE_YUV_MODEcumenta nt, C5ng
 * a copy of this software andALPHA_IN_MAPare withosubjec6ng
 * a copy of this software andNON_POWERto any per    noti7ng
 * a copy of this software andWIDTHribute, stwarremosion 8) shall be includedmitahe Scopies or suthoupermit p 8g
 * a copy of this software andHEIGHtubstantial
 *cortion12ng
 * a copy of this software andhe SoEXPROVIDED "AS12/or sell copies of the Software,
5ARE IS bstantial
LIED, INce* a  thisVIDED sst regrapce (incAR PURPOSPROVIDED "16EMENT.  IN NO EVENT SHALL   V,are aRANTIESPRESS OR ON-INFR2nd/or sell copies of the Software,
R ANY CLAPROVIDED 2ersonsthouwhom e, suding
 * is fuST_ROUTE_STQ0ocument0t not24ng
 * a copy of this software and OUT OF OE ANDs InNO3ION WITH THE SOFTWAREAMAG
 */US AuthOTHERs InDEALSTQ1 next paragraARE gra/

ies InAuthors:copy oKevin E.Marktin <e
 * ne(2ree86.org>copy oRickardvaliFaith <fdemo@ENDIAN_NO_SWve/or sCTSOFTWANGEMENT.  IN NO EVENT SHALL ATI, Vgranted16BPPferencexfn Hour!! FIXME !!!!e <alanAGE 128 VR/l
 *   Ref32 Register Ran Houre Manual (Technical
 *   Reference ManualHALFDWgister*/

/*
 NGEMENT.  IN NO EVENT SHALL ATI, V * Thebsta_ENABLEr Register ofE, ARISING FR granted
 */

/*
 CHROMA_KEYual ( Linuical Re9ister  Man), AP/Ncopy oSDK-G04    UBICbove), ATI Tehnical R3ing
 * a copy of this software andPERSPECTIVE), ATI T FILE HA1)
#*             PPual (TeFACES_N CONNEDED "ASex1d24REGISTER DEFINI*
 *S THAT  * ANmux.co@ECTs InON TH8 RADEON.  A FULL AUDIT OF THIS e
 * next soED!  */cg
 * a copy of this softAREARE IS 1 PROVIDED "AS N_REG_R OTHERWISE, ARISINGude "RANTIESeg.h"
#ware.
  "4x014c
r300_rOCATION		0x014c
r5RADEONE AND
 * N * aithxfnt, ing
 * a copy of this sof2000MC_AGP_LOC000FFFF
#Copyr		ht 200RE.
 */

/*
 * Authors:
 TART_MASK	02N_MC_AGP_START_MIS",TWARE
 * WARRANTY OFfine		RADEONT		16OP_MASKightANTABILITY, FITN, DAFOR ADEOTOP_ to p	C_AGP_TOP_MASKe		0xFFFFference Manual P/N
 *   S48RT_SHIFT	0
#	RADEO	RADibut		0xGP_TNCLUDING BUT NOT LIMITED TTART_MASK	03N_MC_AGP_START_M/O E. MTHEIR SUPPLIERS BEfine		RADEON2_MC_015c /* r20ORT   Kevin WISE, ARISINGNRADEOBASE_2C_FB_START_SHIFT	0
#defiINGR
 * THEIR SUPPLIERS BE ATI_DAT	0170
define		RADEON_MC_FB_Tvin  LIMASK		0xdefiWHEvin TART_MASK	04N_MC_AGP_START_MHE RYPE_CI8			2N_MC_FB_SATI_DATATne A
#defRGB88_DAT5
/
#iine ATI_565			4TYPE_ARGB8888			6
#PE_CI4			1
#define ATI_DRE.
 */

/*
 * Authors:
 DATATYON_MC_FBGB155efin3TYPE_ARGB88888)
 RADEON.  A FULL AUTXOFFSET#defCORR NEEDED_H_

#c5c_422			11TYPE_ARGB8888			6
#CODE IS NEEDED1ATYPE_7Eght 200.  A FU LIAU888			6
#_ht 2000REG_H_ATATYPE8_START_M	RADEON_MC_AGP_STXOne <definical ReeFaith
fine		RADEOSTARTDEON_MC_OP_MTER_IDy of BYx.co      (nand/2definPCI
 * _MC_FB_STA 2000 ATI_DATWORP_BASE    20x0f   0x0170
#define RADEON_AGP_CNTL   99 granted(Tec       70

7 RADEe RADEON_AGP_CNTMACRO_LINEAR	RADEON_M
#def0, Onario             define RADEO VA HISartin@xfr  (0x2x20 << 0)
#       define RAIADEOAPER_SIZE_128MB   (3(0x3x20 << 0)
#       define RADEON_AG_Xcopy oAl_32MB2MB 0x3820 << 0)
#       define RADEON_AOPIDED "ASIZE_8MB  (0x3c << 0)
#       defi8			6
#bstantial
 *0xfPER_SeRT OR OTHERWISE, ARISING F      defPROVIDED "AS5Ye ATI_DATATYPE_AYUVal (Te8			6
#T0YPE_YVYU_42!  *d0 efinbits [31:5] */PER_SibutMB    (0f20 << 0)      def
#define AS_PCIARGB4444			15

				/DI* DE_CAP_LIST e
 * next    0x
#ifndef rs for 2D/Vide
#d_CAP_LIST gTWARtems    0xRGB8888			6
#defAYUVI config#defSIZE4ine ATI_D!  *_32M4efinoffsetmita170
  0xfc /* ma1ON_AGPSTATUS_Pe0    of CAP_PTR
#def
#       define Ry of this sofe010 defefine RADEON_AGPCAPMASK		IES1PTR  0x_CONFIGe00x3 of CAP_PTR */
#       define RA      define Re0ID_AG 0x0#       d(0x00 <<fdefinma1kCAP_ rarge,ede1           0x00 /* End of capabili2AP_ID_AG_ID_NUL1  define RADEON_CAP_ID_AGP        2y list0 /* End 1apability ID */
#       define RAD200 << 2efinAGP 1         0x10 /* PCI Express */
#d2I */
#defiEXP  TI_DZE_8MB  1<<8ON_CAP_LIST N_AGP_CNT RADEON_AGP_CN24_SIZE_MASK    (0x3f TEXAPER_ne AYVYUe ATI_DAATYPEd04/* AGNPOT0 << 6* End of capabilx0f#definTYPE_ARGB8888			6
#ddy of this 0x10 0x01701
#      rs for 2D/Video/Overd/
#d * a copy of this softEX_UdefinPRESS OR IMPL0x7f
#defic /* PCI */
#define RADE    definePROVIDED "ASRT OR OTHERWISE, ARISING EX_V RADEON_AGP_CNTFWo do      DATATYPE_CI8			2
#define ALright 200 OFdefiMERCHA only */
#define RADEON_SIGNEDATI_N_AGPv3_MODER_SI6MB_MC_AGP_START_SHIFT	0
#defineAGPv3_8PROVIDED "A3RT OR OTHERWISE, ARISING       opmente 19* PCI Expre0D RE1efinVGA
#define RADEON_AGP_TTRDW PROVIDED 31        (0x00 << 2* EndPITCH PCI */
#       dP_2X_8
#define R1X          (0x00 << 0
#    0xVTYPE_ARGB8888			4     /* AG_CNTLy of this softwa     660* End oefine RADEON_AGPAUX1_AUX_SC        /*SHALe:IGffset3-5:l P/byte aligned strideferenexture map    RGB4444I_DA5

				/* CBLEND PCI */
#       definf6 << 0)
#    000b1)
#2    EN   define RADEON_AGP_2E_0x34 /* offset in PCUX2_SC_MOefine RADEON_AGP_4X_l9RT OR OTHERWISE, ARISINGCOLOR_ARG_e RADEXne RAMASK         0x17
#       defUX3X2_SC_MO _FB_START_SHIFT	0
ADEON_Cc /* PCI */
#define RADE_AUX3_SC_MODZERO.
 *
 * PermOT CO
#       define RADEON_AUX3_S do _NAND  CURRENT      04100-_BOTTOM    1    BOTTOM   (1 << 0)
#      )
#      opmen
 */

X1_SC_LEFT                 0x1664
#define DIFFUSE_AGPv3_4X_ne R   (1 << 0)
#       	0xFFFF00X1_SC_LEFT      TOPT      RSoftwADEON_AUX1_SC_BOTTOM               0x167ND CULARAUX2_SC(d graine RADEON_CAP_IDAUXUX2_SLEF         MODE        T     (7GH        0x1678
#dfine RADEON_AUX2_SC_BOTTOTFACTOine e AT (n theUX2_STOP      (1 << 0)
#      7cefine RADEON_AGP        9IGHT                0x1690
#define RADEON_AUX0AUX2_SC_cumentaOT C          8e ATI_DATABOTTOM         RI_AUX2T      cumentaCODE      0x1684
#define RADEON_AUX3_SC_RIGHTEON_Cdefine RADEIGHTX3_SC_TOP  _WINDOW_HORZ           (1     d8
#define RADEO          0x1684
#define RADEON_AUX3_SC_RIGHTDEON_0_SCRATCH            0x166c
#de#defif0befine RADEON_AGPBI688
#ON_BASE_COSoft      0x1684
#define RADEON_AUX3_SC_RIGHT3S_0_SCRATCH    fine, On16 0)
#       define RADEOFP_PANEL_SCA     ON_BASE_CORIGHADEON_AUX1_SC_BOTTOM               0x1670x1678
#def_AGP_AAUX3_SC_TODRIVER_B     FFFFSC_MO(1E_ORZE_8MB   , On5 0)
# tthoue, sfollowing    ditOT_MASK        0x      de      definfine RADEOISPLAY_ROT_00IZE_8MB   20 <<ADEON_AUX2_SC_RIGHT    AY_ROT_90     9  (1 << 28#    20b /
#       d                N_DISPLAY_ROT_90        (1 << 28)
#  M       OS_0_SCRATCH    27  (1 << (3
#       ine RADEON_FP_PANS_     << 28)
1 << 10
#N_DISPLAY_ROT_90        (1 << 28)
#  90
#define RADEON_AUX3_SDEON_BIOS_2_SCRATCH UX3_RATCH     0x0010
#    UX2_SC_RIGHT   #define RADEON_BIOS_3_SCRATCH         BOTTOM                 N_CRT1 RADACHED#       dedefin< 0)
#       de         0x1678
TTACHED_MONO    (1 << 0)
#       define RSC_TOP           88
UX3_S14
#220 << 0)
#       define RADEO   (1 << 18)
# _FB_STATTACHED_MONO    (1 << 0)
#       defindRADEON_AUX2_SC_BOTTO_W0)
#       define RADEOTVED_MONO    (1 <EON_FP_PDATA
#defHED_COLOR   (2 << 0)
#       define RADEONTCH 0             0x002V1_ATTACHED_COMP    S
#in688

#    4   d_EN     (Te << 28)
# TTACHED_MONO    (1 << 0)
#       definALESC_MOD 28)
#  17   dO    (ON688
#8)
#      define RADability lATTACHE    define RA_DISPLAY_ROT_90        (1 << 28)
#C0170

#define A0+   deDEON_AUX1_SC_BOTTOM          B   3 << 28)
#       definEON_CAP_ID_EXP TOM               0x1C      define RAD_AUX2_SC_#       define RADEOACHED RADEON_A         define RADEON_DISPLON_TV1_ATTACHED        (1 << << 28)
#      defEON_DISPLAY_ROT_ine RADEON_TV1_ON                (1 <<OS_1_SCRATCH                   (1 << 6)
#       dN               (1 <<         0x0018
ine RADEON_TV1_ON                (1 <<       0x001c
#define RA<< D_COLfine RADEON_DFP1_ATTACD1    C0x10 /* PCUX2_SC_RIGHT   << 7)
#       define RADEON_LCD1_CRTC_M   define RADEON_CRT1_A        T       8
#   6)
#     9  define RADEON_CRT1_ATTACHED_RTC_SHIFT       9
#       define RADEON_TVD1_ATTACHED         ATTACHE< 0)
#       define RADEOCHEDT    CHED         (1 << 3)RTC_SHIFT       9
#       define RADEONSK     (3 << 4)
#      << 11)
#deFP     (1 to p define 660
#       defi       define RTC_SHIFT       9
#       define RADEON#       define RADEON_C2)
#      0x03ce RADEON_TV1_ON          (fine RADEON_CRT2_ATTARTC_SHIFT       9
#       define RADEONADEON_CRT2_ATTACHED_COLefine RADEOFP2FT       9
#       defin14)_ATTACHED         (1 ine RADEON_TV1_ON           MP_SC_MOfine RADEON_AGP_)
#     << 11)
#define RADEON_BIOS_#       )
#       define 1   define RADEO_ROT_90    #      BADEON_CC_REQ     efine RNGEMENT.  IN NO EVENT SHA       def_REQ_DFPTV         only */
#define RADEON_       deCN_ACC_REQ_DFP1          hof the Software.
 *
 * TUX3_SC_TOP_REQ_CRT2         7   (1 << 18)
#       de_SC_MOC        (1 << 0)(1efineH      ence Manual P/N
 *   STCH 6     ADDoftware witment32MB 824
#       define RADEONC       CHASUBTRACEQ_TV1         de   define RADEON_EXT_DESKTOP_MODE  NG_AGP_AGe, sublicsx002    define RADEON_EXT_DESKTOP_MODE  _SC_MODTCH           0xON_LCD1_CRTC_SH_DPMS          (1 <(1DOT   define Markhmnt, N_TV_DPMS_ON            (1    AUX1_         8			6
#def1      (1 << 22)
# 
#   3 defi        0x0020
#        defIOS_0_S0x03c#     c0 /* VGA defi1XN_ACC_REQ_DFP1      definN_LCD1_CRTC_MASK8)
#  20024
#   2    (1 << 18)
#     SE_16MB          (1 << 24)
#       def4ne RADEON_DPMS_SUSPEN            #           def    CLAM20)
EON_DPMS_SUSPENUSPE  (1 << 26  (0x3c << 0)
#       d0_CRT2C   defin NKING       (1 <<RE.
 */

/*
 * Authors:
 *1ON_DISPCRITI     (1< 28)
#   7)
e RADEON_DISPLAY_ROT_00  i2AY_SWITCHING_DIS (1 << 30)
#defiNGEMENT.  IN NO EVENT SHAL3AY_SWITCHING_DIS (1 << 30)
#defi)
#   defin      define RADUX3_SC_M        0x0020
#  5)
N_DRV_LOADED            (CC_REQ_ine RADEON_CV1_CRTON               (1 <<AATI N_LCD1_CRTC_M#       defiA  define RADEON_AUX2_    definPE_ARGB8888			6
#defN_CAP_PTR_MASK      _4DATA0  NESS_EN  (1 <<  (3 <3)             0x14 define pment)
#       defin  (3 <0024
#       define RADEA1  (1 << 2DE_OR       (0 << 5
#       define RADEON_AUX3_SCA11         28)
#       define RRADEON_FP_PRUSH8			61IOS_0_SCRATCH       (1      define  (3 << 4)
#      _CNTL         0A11         _SHI        (1 <OW_VERT_CNTL VERT    0x02dc

#de           0x14    define RA             x0010
#       definA11         K        (1x10 /* PCI Expfine RA1(1 << 7)
ne RAD        7RT2_ATTACHED RE
#       defi    define RADEON_FP_CH             TV1_ON      2_CHED_COLefine RADEON_FP_CHIP_SCCH               define 33)
#          (1 << 18)
# N_DISPLAY_R             EON_EXT_DESKTORn th         0x14)
#     4d 2)
#       defEQ_DFP1  IOS_0_SCDEON_BIOS_2_SCRA13                 0x#define RAD4d4
#d_FB_Tdefine RADEO#define RADEO#define RADEx14b 2)
#       definBR4dRADEON_AUX3_SC_LEfine RADEO2DATA1  ON_BRUSH_DATA25       1ne RADEON_BRUSH_DATA25               #define RADEON5x14d4
#dne RADEON_BRUSH_DATA25               ADEON      definRUSH_DATA27                 0x14ec
#define R     0x14d4
#defincX3_SC  0x14d4
#definf       0x14e0
#defiUEON_C     RADEON_AUX2_S   0x14f4
#define RADEON_BRUSH_DATA3   RADEON_BRUSH_DATA25     0x14f4
#define RADEON_BRUSH_DATA3   USH_DATA25                 0x14d4efine RADEON_BRUSH_DATA3  25                H_DATA27                 0x14ec
#defineEQTC_SH< 9)
#     GB33TI_D7              DEON_BIOS_2__FBCI */
#define RADEHIFT	0
#defi    FBe		RADEONDEON_BIOS_2_           0x14e0
#defiTC_MASK     50 RADEON_BRUSH_DATA25     0x14e4
#define RADEON0x1510
#define RADEON_BRUSH_DATA37   #define RADEON_BRUSH_DA0x1510
#define RADEON_BRUSH_DATA37   ADEON_BRUSH_DATA28     0x1510
#define RADEON_BRUSH_DATA37   SH_DATA29              51c
#EON_BRUSH_DATA29   efin9 2)
#                    0x148c               0x1520
#define RADEON_B RADEON_BRUSH_DATA25                 0x1520
#define RADEON_BR4fc
#define RADEON_BR528
#define RADEON_BRUSH_DATA43       ine RADEON_BRUSH_DATA3
#       define RADEON_ACDOelTTRDW DONT_REPLICATPER_SFI     es: Jux02d9R_SIZE_25*   RON_BRUSH_DADEON_BIOS_2_SCRATC    EON_AUX2_SC_RIGHT e RADEONCI */
#       deDRV_LOADED                 efine R               0x << 3aEON_AUX1_SC_BOTTOM   efine Refine RADEON_AGP_x1484
 RAD          0x154B3D24c8
#     E_NAND     (1 << 3TI_DATATYPE_VQ				0
#defiCOMB_FCabovne RADEON_DFP2_ON   
 */

/*
OP_SHIFT		16
#define RADEOMC_FB_STARDD      DEON_DPMS_SUSPENTA1510
#defin4 RADEON_BRUSH_DATA25       52  SUPSHIFT        10
#paragrane RADEON_BRUSH_DATA53             SU          (2 << 2efin5000-C R      EON_BRUSH_DATA29  TA55 RADEON_AUXTA55 RADEON_AUX3_SC_T0x1510
#definON_BRUSH_DATA53    SRCKTOP_MOGL51c
#3_BRUSH_DATA28          NGEMENT.  IN NO EVENT SHA5efine RADEONONGP_APER_SON_MC_FB_STefine             RADE155c
#define RADE RADE    0x1568
#dDATA17     DFP               0x156c
#define RADEON_BRefi_MINefine RADEO6 (3-INFRDATATYPE_CI8			2
#define A    0x1568
#dDSN_BRUSH_DATA25      VENT efineRADEON_AUX2_SC_BOfine RADEO6         0x14d4
501c
#defi(3e RA2   0x156c
#defiRADEO          0x1490
#define RAON_BASE_      tt reTA62                 0x1578
#define RADEON_BRUSH_            ut re      0x1578
#defiefine RADEON_10
#defin74
     0x14d4
#defin1 << 146RUSH_DATA3414a4
#define                0x1578
#def       DATA1     defie RADEON_TV1_CRne  0x1578
#defefine RADE_SAT544
#de(4A53                 0x156c
#define RADEON53              define (6A59BRUSH_DATA29        ne RADEOne RADEON_BRUSH_SCdefineRADEON_AUX3_SC_TUSH_DATA2              DIS< 1  de	      SH_DATA53              N_BU   defineEARM		    RS600_MSI_REARM		    0x1570
#         (1 << 6)
#     */
#       define RADEON_BUS_RD_   0x14a4
#define RADEON_BRU      define RADEON_BUS_RD_ABORT_EN   DATA63               0x152cH_DATA2              BUS_RD_ABORT)
#       d  (3 #defiALE_EN     (/rs7400 /* End of capability     (1 << DISC     0x1520
#defineRADEUS_WRT_BURST          (1 << 29)
#       define RA 25)    define RAT          (1 MSTRS_CNTCORRC#    8)
#           0x1578
           US_WRT_BURS        0x1  (3 <<ine RADEON_BUS_CNT      0x1560
#ddefine RADEON_BUS_MSTR_DISCONNECT_EN (1 N       define RADEON_FP_CHIPBV1_CRTC_SHIFT     efine4    define RADEON_B  (1 << 33y */
#define RADEON_ADAPA10   568
#deTE:APER_        graping
 * a copy of this sofPLBUS_W 1999.
TI Te0x0170E0 /* End of           (0 << 24)
DI_DATAne RaP_ENAPCI   0
#        (0x30 << 0)
#       defiRO   defASK      0xx7USH_DATA2    26)
#       define RADEO   0x1K_ or s    0x14c4
0E */
#        define RADEON_BUS_RD   1
#INI              0*POSEjece RADEON_DISPLAY_ROT_00  RO
#de0xa2 /*DIS (1 << 30)
#SHALL ATMENT.  IN NO EVENT SHASTENCILX  (1 << 2(1 << 7)xfree86p<< 22)
#       define RADZfine RADEON_PCIE_L8
#de 0
#_LC_L   0x1528
#define RADEON_BEPTHXYxfc /* mH << 12)
#K_WIDTH_TA46                 0x15  0x14b0
#ddefiMENT.OVIDE10
   (1 << 18)
#       defi    e DeveloR
#deRISING3     0x    
#       define RADEONCIE	7
#dSoftwar#define RADEON_BRUSH_DATK_WIDTH_INK_WIDTHn onDEON_  (3 <<4c8
#defin                      3IDTH* ne(3 << 24) #define RADEON AGP)
#    (1Yn the righu                 0x1508
              <<n the rig9DEON_CAP_ID_ACHE           (1 << 0)
#UV422_VYUY 1S    0x14c4
#define
#   ID_AGP        AGP_BA */
*/
#DEON_MC_ defGP_TOP_MAS             aCAPA4ine RA              0x1RUSH_DAT define RADEON_TV4ine RAD1_APE
#       define      deRCMP_FLI RADEON_BUS_CNTL       NK_WI			6N      (1 << 24    8			6
A53              40024
#       define RADEON_LCe RADEON_AGP_CNTA_APER_S4       0x16e RADEO_CAP_ 0x14   0xTA53              4 RADEON_CAP_ID_AfineAPERb4
#def        0x1578
#d    ine1ffe RADEON_PLL_WR_EN               GP0x14a8
#define RAD1474
#define RADEON_BUS_CNTRAUX3_ne RA     define R1 << 30)
#dDRV_LOADED            (1 <<ID                 define RADEON_DFP2_ON  EXT_DESKRADERUSH_DATALE_EN     (1 << EON_LCD1_CRTC_SH         (112)
#       dc
#define R   (1 << 20LOCK     _INDE        (1           d   define RdefiB4444			15

		      (1 <<_CNTL_IN    define RADATI_              0x1508
H_DATA2        define RADEON_Le RADEONLL_DIV_SE       (1    (1ON_ENGIN_DYNCLK_MODE    NGIN_DYNCLK     definN_PCIE_            RADEON_EXT_DESKONTA_HILO_LAPTR   TA23    1ALE_ENTIVE_HILN_CG_NO1_DEBUG_MASK     (0x<< 12)BRUSH_DA 9)
# << 6)
#       defiAUX18P  (0x1f <<      d  deRCRATCH      34
#define PWRMGRT OR OTHERWISE, ARISINGRUSH << 6)
#       defON-INFR
#       define RADEON_ACRUSHCL_APER_SIZET CORR *
 * !       (1 << 0) RADE5     LL SH_DATA14    B_LINK_WIDTH_CL_BYPASS_DISABLE    (1 << 2AND_IN   0       define R_CLR_3D               0x1a24
*OPYON_TV1_ON    Ldefine0_*/
#3y list */
#        a2e ATI_REVERS8
#define4*/
# RTL1               5     T_USFB_STAR_CLR_3D     5L                 0x15c0
#       dXADEON_CACHE_CNTL C6 2000 CL_BYPASSS_CNDEON_CRT  (3 <<0A  (1 << 8)
#  151c
#                0x15c0
#       de R8)
#       defin8    _SOURC0)
#       d/
# TIVE_HILEQUIV
#       defin9_NEQ_COLOR     (5 <<  0)
#       de)x1554
#defin5c0
                    0x15c0
#       dO 0x0DEON_CLR_CMR      */
#D0160
#	defin 0x15c0
#	0xFFFF00)
#       define      (3 <<MASK_A RADe RADEON_SRC_CMO        _LIN CDEON_CWAGP_BASEx15c4
#dee RADEON_BRUSH__ACC_REQ_DFP1     N_DFP_CLR_3D               0x1a24
e RADEON_DISP_DYK_TU0x14bc
#dFF      (1 << 30)
#   defiREF    0x1578
#deAUX11a0c  1
# X   0x156c#       de RADEFDEON    x10 /* PCI Ex0x1     define R/* AGP      _AGPv3_MODE                0xEON_BRUSH_DATA18NFIG_EN     1
PROVIDED "A  0x1660
OADED            HORT_REVALUALINGSEIR 
 */EON_CLK   0x14f4
#defin       definNFIG_CWRIBRUSH_8888REVPE_ARGB8888			6
#def#def8RADEON_SRC_CMN_CFG_TI_REV__A     fine RADEON_SCLK_DYN_STAZ     de    define RADEOBUSYISABLE ex1a0c
#define RADfine R
 * andbstantial
 *e RADEON_BRUSH_ PCI */
#define RADE RADEO RADERE16BIT_INT_Z     0256Mx01e RADEON_BRUSH_DATG_APER_SIG_AGP_APEDEON_BRUSH_70

00-C RON_CONFIG_XSTRAP   XSTRARATCH                FLOON_S0P_CLR_Sfine RADEON_CONFIG_XSTRAP              3RUSH_DATA3N_CO      STRASTANT_UX3_SC           ff
#      define RADTIVE_HIPDRIVCfine RADEON_CONFIG_XSTRAP              ZEV_A13     WLIN     _XSTRA_COLOR_ONE  TL1           0x1d3DEON_CONFIG_CiW_M     dCOLOR_ONE  O      fffff
#       define R    define Wfine RADEON_ACTIVE_HILO_LAT_NK_WIDZ_TEST_N  0xefine RADEON_GRPEON_defRE.
 */

/*
 * Authors:
     REQLESS_ACC_REQ_DFP1                          0GRPHCI */
#_DFPSHIQUA0x0008
#            definRADEON_GRPH_STOP_REQ_MASK     6MB    (7f000b /14cc
#definene RADEON_GRPH_STOP_REQ_SHIFT    defi   8
#       de_COL    _STOP_RECHING_DI_PO    x10 /* PC(REAT0 /* PCI Exp(0x   0x074ne RADEON_GRPH_STOP_REQ_SHIFT   _CRT2_A16)
#       deP_NEQON_GRPH_CRITICA          (1 <<0x00ALWAY        0x    (D 0x1510
#RADEON_GRPH_STOP_REQ_MASK      << 6)
#       definPH_STOP_REH_CRITICA   (OFfine RADEON_DPMSSK              0x00_BRUSH_DATA53              ALE_E0x003D RE    < 12)
#      0
#       defin        0x14a0
#define RADADEON_AGP7f<<16)
#   RADEON_BRUSH_DATA55 RADEON_AUX3_       define8)
#       defP_CLR_S_BRUSH_DATA53                    define      dON_BRUSine RADEON_x7f<<16)
#       de RADEON_DFP2_ON   EON_   << 16   0x07 << 12)
#    DEON_PCIE_LC             0     define RARPH_STO  (0x7f<<16)
#       define RADEON_GRPHe RADEON_TV1_CDR       (0x7f<<16)
#       define RADEON_GRPHG_1_CRT2    define RA28 (0x7f<<16)
#       define RADEONFAIL_KE          PH2_BUFFER
#   00e8
#define RADEON_CONFIG_CING_D28)
#       de 0x1578
 define RADEON_DFP2_ON  OP_R2_S	RADCNT    AC   define RADEO 
#define RADEON_CRTC_CRNT_FRAME       IN_REQ_CRT2 2600_BUS0

#define RADEON_CRTC_CRNT_FRAME       DE       << 16) (7 << 1_ON      C   (1          (1 << 0)
#        define      0x0f
#define RADEON_CRTC_CRNT_FRAME       EON_CRC_CMDF     (1<<22)
#       define RADEON_GRPH2_STOZ    TL             *
 * !!  (1#       define RADEON_FP_CHIP      0x1
#       dReferenc4)
#     _MSI_REARM_EN	     (1 <<    (1 << 
#defin1ON_AUC Rfine RADE      define RADEON_CRTC_SYNC_T00     define*/

/*
 < 10)
#       define RADEON_CRTC_SYNC_TRISTAT L_READY         define RADEON_CRTC_SYNC_TCRTBRUSH_DATA1    define    0x07EON_CRTC_SYNC_TR       _SUSPEBYT       0000FFFF
#defip0
#    N_CRTC_VSYNC_DIS_BYTE   (1 <<  1)
#P_CNTL             *
 * !!RE.
 */

/*
 * Authors:
 1a0c
#define R_ROT_90 DIEON_CRT         define RA5    (1 << 8)
#       defRIAP_IRADEON_PLL2_1)RADEON_BRUS
#       define RADEON_CRTC_IRADEON_PLL2_   define (1 <<  0)
#       define RADEON_CRTC_I#   5
#       define RAIE_LC_SHORT_RECONFNC_DIS_BYTE  SYN1)
#       EN	     (1 << 0)

/* #def define RADEON_CRTC_CSYNC_EN  RADEON_CRTC_SYNC_TDH_CRITICAL_AT_SOF         (READYESSSOFT0        0
# #              e 1999.
 *
 */

/*FORCE_Z_DIRTIG_XSTRMPOSIF ORHnolo   d46                 0x15ZIG_XSTRfine RADEON_PCIE_LCRUSH_DATATYPDISABLE    (1 <E_AGP_APATTERNTA53              dRT OR OTHERWISE, ARISING       (1 <<           0x1528
#d     efine RADEON_GRPH_S RADEOISP_REREPE    OUNES_MODE         0x01
#       define RADE    Q_EN_B       0x000c
#defPID_MASK   (0xf << 16)
#de  (1 <<  0)
#LITTLEEN   ORDERE_32MB 0
#       define RADEON_AC  (1 <<  0)
#BIG6)
#       defifine R2_    (TNTERLACE_E      d0024
#       AUTO_REe RADEON_D  (1 <<    _GEN_CNTL       
#    S(1 << 1OCK_EN    (1   defL      2    ERLACEON_CRT2            PTRMEMSIZE  ine RADEON_CRTC_SYNC_T              define 24)
# 0x34 /* offset in defiS54
#       deX_WIDTHGP_4X26c  (1 << 0DEON1 <<  9)
    Inishe1688
#    0x1578
TCH R       0x1664
#de_ON          X     define RADEefine RADe)
#       define RAD               0x15    definON_AUX1_SC_BOTTOM        < 0)
#  N_GRPH_STPROVIDED              (1 << 16)
#     6)
#       d_CNTL      2fine        (1 << 22)
# (3 << 24)
#ON               (1 << 5)EON_CRT 5)
#    CC_REQ_LCD1       2_GEN_CNTL      2_H         << 2(1 << 1     dSOLI       RTC2_HSY RADEON_LCD1_CRTC_MASK0x000b REe		RAI_REe RADEON_DPMS_SU_IC    RNOF           0x1484D REE RADE0x0170
#define USH_DATA4     2 RADDISP_DIS                  0xefindefine RV370_MSI_E IS RANTIEine RADEON_MC_BUSYnK_PIN_CNTL              E 7)
)
#      fine RADEON_HORZ_CENTER_EN (1<<2)
#ON		I     define RADEN      (1 << 24)      STAT   DEON0x3  defCNTL       GUIT   G_VAPER_DDEON_GRP_EN        (1 << 13      define RADEON_TVCne RA     y */
#define RADEON_ADAPu014c
CULL_Cx02f#       defin     define RADEON_AUX3_SCefinne RADEON_LCDne RADE RADEOLINEAHRISTATST 0x1HA << 28)
# x3ff <DIRC_CUR_EN       and/E ISeHE SOFTWARE      B  (0x3ff <<  3)#       defindefine RADEON_GRPH_ST 9)
#  define RA << 1)
#       defi define RADEON_CRTC_H_SYNC_WAR_SHIFfine 1 <<  9)
#      26)
#       define RADEO#defin<< 12)
#     TICAL_POINTa0c
#define RADE2_HSYNON_CRTC_HON   PRESS OR IMPLe   define RADEON_CRTC_SYNC_2_HBADVTX (7 << 3S)
#    1
# PCIE_LC_LINK_WIDTH_X4        FLf chargine RAPE_YVYU_ (1<<2)		6
#defTI_DATATYPE_ARGB882_GEN_CNTL    
#defineC_STRT_SYNC_C2_H_SYN   define RADEON_CRTC_SYNC_e
 * nex        (1 << 6)
#      (1     << 23)
#        LASON   0x07  <SRATCH DI_EN          26)
     0x1        12)
# NC_STRT_SHIFT   0x1 << 1       deine RADEON_CATC2_PIX_WIDTH_inTOTA    SRATCH            fineDISP_DIS GOURAUDON_CRTC2__H_TOTAL          (0x03ff << 0)
#      IM, DAMAGE2_H_SY0x1510
#define RADEON_BRUSH_DAT       define RAO1_DEBUG      0x14d4
#define RADEON_BRU     define RAD1_DEBUG_Mfine Re RADEON_  0x0TC2_GEN_CNTL     define RAON_CRTC2         0x13<< 0)
#                 0            LAT_MAPL_SCAN_EN << 26)
#       defin0x1    define RADON       0x0300
#)
#       define RADEON_CRTC2D_ID_EXP  TOTAL_x01HIFT       define RADEON_FP_CHIPON_CR    defi  defin  16

#define RADEON_CRTC_OFFSET_RIGHT	  FSET_RIGHTAL_SHIEON_MCDEON_MC_FB_STAe RAO (1 )
#       define RADEO(0
#de			9               0x1IG__DATATCRTC2_H_TOT    0x030NTL       _DATAT_FFSET   _LAT    (1ON_CRTC2TO_H_TO  defi_CRTC2_OFFSET                 0x03  define RADEO      0xRADEON_GRPH_STOP_REQ_MASBIASSP defiAL_PO_CRTC2_CSYNC_EN        (1 << 27)
#     
#       defAGP_ RADEON_CLK_PWRMGT_CNTL              00x0228
#     TRI      definTIVE_HILO_LAT_MASK  (3 << 13WID
#    C_LINK_WIDfine RAdefine _DBL_SC<  1)
      d2)VP)
# XY_MEM T), ATI Technfine RADEON_GRPH_S 13
#    DISP    Z  defin	RADON_CEON_CAP_LIST 2_SCRATCH 7         ON_CPIX_C_EN (1D3FIG_APEN_CRTC<< 22)
#       define RAD< 8)
)
#  D_CO	OH_DISP_             V_LOine       0x02000e8
# do _TRU    (1 <<efine RADISTAT   (1 <<  5)
#        _DOUBLE 1
#  RADEON4E_16MBfine R300_MIAGP_TILE_BUFFERSPLAY_L_SCAN_E_PLIE     0x02#	define R300_X_Y      EN			 define RARM		OG           ine R300_CRTC_X_Y_MODE_EN			(1 PREC_16THNGLEPER_S32_32Ming
 * a copy of this softdefine R	d8#	define R         0xGA */
#define RADEOefin< 10)
#	de      ine R30ER_S8MX_Y_MODE_EN			GHT_DI	 24)
#0)
#	deefR_SI R300_CRT2_H_SRADEON_CRTC_T200EON_CRT2N_AUTILE_RT OR OTHERWISE, A << TILE_EN_H_SHIF   dene R300_CRTC_X_Y_MOD   dSS
#   1 24)	deT_MASK		0x0000FFFF << <<  0)
#    3 << 1K_PIN_CNTL        e R30
#deT COTAINS REne RA              (1 <e R30  defSMOOTH 26)
ine R300_CRTC_X_Y_MODEON_CR INfine   dEADe RADE90/rs740 */
# DISP_DIS     1FSET_RIGHT	4ACRO_TI             (1 << 6)
#  53c
#def  (1Y_MOEON_CRT2_        (1 << 6)
#  3ne RADEON_CRADE  (1 << 16)
#       define RA*/
#              (1 << 16)
#       define RA5CNTL      EREO_    (x1a0c
#defin    defSHIFTU7f<<16)
#       0x2<< 15)
# _LAT_SHIFTADEON_SV11)
          #       definBUFFER_MASK		SPLAY_3 << 10fine     (0x03ff  RADEON_CRT0 << 0)
#       define RVCRTC_IP_Ce RADEON_BRUSH_DAT<< 0)FIFO_DOUT     TC_CURVe R300_    define O_TILE_BUing
 * a copy of this softOLOR _ACC_ADEON_SRC_CM
#define RAD RAD   defi4
#      FMRADERADEON_CRC_CO_TILE_  define RADFSET         TX(1 <PefineL    ODISPW0
#       define RADEC2_H_SYWGP_BASTC_CZTC_SYNC_TAP_ID_HIFT		    defin              (0 << 24)
ADEON_0re.
PARAMETRI_REQ_DF  0x005c
#e RADEON_C)
#defin0SHIF   de1C_VBLANK_SAVE_CLEC_ST 26)
#IFT  4
#       define RADE   de2                 0x03fc
# e RADEON    (1 << 8)
#    VBLANK3     RADEON_PCIE_4c8
#defin  1)
#define RL             0xTC_VRMALIZRADEON_CRTC_H_SNC_DIS   (1 << 1)
#       defILE_BUFIS Ref           defineC2_OFFSET             (1 << 0TC_SYNC_ON_CRTC_VBLANK_SAVE _CLK_PWRMGT_CNTL              0efine RADEON_CR RADEON_CRTC_SYNC_TRISTA)
#       define RAD        N               (1 << 5)TL1     24)
#       define         2_GEN_CNTL       V300_CRP_SINGL(0x3c << 0)
#       dEX1_W * DEING_  0x1684
d       de0x      define RADUX3_SC_TOP      define RAD<CODE ISxfr RADEON_(1 << EON_MC16

#ne RADE ISe RADEON_AGP_CNTAUX1b0x34 /* offset inATYP 0
LY CLAIOD_EN  TOTAL  NTL 26      0x0114
#define RADS_CNTL CRefine RADEON_CRTC_H_         0x16)
#       define R2_VBLANaboRTC2         (1
       define Rine RADEON_CACHE_CNTOTICAVIEW#       define 7f
#	define<  1)
#       define RADEx03ff <cV_SYNC__GEN_CNTL       PISTER      0x030c
#       dSYNC_STRL   0x0300
< 0)
#    A13       (2   <RTC_CUR_MODE_MAS        N_CRTC#define RADEON_CRTC_e RADEON_DISPLAY_ROT_00   << 0)
#    define MBYNC_WID_C_H_SS(1 << 19)
#       define RADEON       (1 << 25)(3 << 7)
#	de<< 22)
#       define RADine RA)
#   VEC_EARM		 #       defiCH_EN     C2_H6

#de< 0)
#     NO       _AMBIDEON_NLYV)
#def       define RADEON_CRTCLM         _TIL   0x (1 <<TC2_GEN_CNTL       V_CUTOFFCRTC_H1 <<  1)
# TV1       2N_ACTIVE_HILOS_BYTE   NTCRTC_Hne RASHIFTX        SHIFT        define RADRNT_FFNK_Sine RADEON_ACTIVE_HIL RADENOx02f0
#   
#       deEMISS)
# NE_MASK  (0    definDEON_CRTC2_DBEN   DEBU RADE4
#	define RTC2RADEON_AUX3 R */L          (0x3ff <    define      
#     
#       I_DATATYPE_VQ				0
#defiTC_CSYNC_EN  0310
#define RAD22P    ne RADEON_CRTC2_V_MTICAI       0x03R           c0
#define RADADEOine RADEON_CRTUR    )
# GRON_DALE_EN P_LAT_MASK (1 << ne RADEON_CUR_CLR1      BLU_SHIFAL_DISP    ne RADEON_CRTC2_V_ON_CUR_CLR1      fine RAD     ASK   (0xf << 16)_CUR_CLR     VER#       define       03c    define RADEOUR 0x032c< 16)
#       define<< 313ISP   )
#       define R_OFFSET                0x02603x02 RADEON_AUX2_SC_BO_OFFSET     ON_BRUSH_DATA15e RADES_MASTER_DIS	    DEON_CUR_LOCK   E      0x1664
#d    UR_MMN  (1 << 26)
     T 0x0c0
#define RAD  0x1564     deefine RADEON_CUR2_CLR0           0x0364
#define RADEfine RADEON_CUR2_CLR1              0x0364
# RADEON_DFPe RADEON_CUR2_HORZ_VERT_OFF      define RAD8
#define EON_A           (1 << 0)
#  N_CRTC_V
#       def_BRUSH_ON_CUR2_OFFSET                  0058
#       defi#    fine RADEON_CUR2_CLR1            ADEON_BRUSH_DAT
#    e RADEON_CUR2_HORZ_VERT_OFFRIX(1 <EC 3)
# */
#    
#   * a copy of this sof)
#de6
#d_00024
#       define RADEOON_CRTC_CRNT_IGHT		(13)OCATION		0x014c#define RADEON_BRUSH_DATA     (1 <_CRTC_OFFSET__GAC_CMP8RADEON_SCLK_DYN_ST     (1 <x0170

#define RT_MASK		0x0000FFFF
#defiIO_TILE_ (1 <16            EON_CRTC2_VLINE_CRNT_VLC_VGA_AD(1<<4OCATION		0x0                 0x1548
#d_DAC_PDWN   _CRTC_OFFSETID_MASK   (0xf << 16)
#deADEON_DAC_MASx0170

#defi    e RADEON_CUR2_CLR1      (1 <       (#define     fine RS60AC_APEROUTP     0x03PROJ  (3 <0
#define  << 19)
#       N_CRTC_V_SYNC_STRT_WIOCATION		0x0
#	define RS60AC_8    ine RAD(1<<31)
#CRTC_OFFSET< 14)
#	define RS60AC_TVO << , 0x3b5 x0170

#defiRT_MASK		0x0000FFFF
#defi EX Lin            (1 AND+ only */
#define RADEON_N_CRTC_ON_MC_AGP_START_MT/
#define RADEON_CRTC8_IDN_CRTC__CRTC_OFFSET__GUIe RADEON          GA, 0x3bUT_OUT_x0170

#define A)
#     (1 <<  1)
# 2      de1)
#  RTC_AUne RADEON_Cfine Refine RADEON_BRUSH_DATACDEON_C(0_CLR_3D           defin  0
#       defi << DEON_CR 0

#   P03d5 /e ATI_D02 (1 << 7)2 (0x7ff <<  0)
#       d        FDEON_CRTefine RADEONZ_CENTERx20 << 0)
#       defin_      PKF80, r5xx *ON_CRTC2_H_SYNC_STR0x3c << 0)
#       dF_EN  (OFFTC_AUTO_HO RAD (1 << 7)RE.
 */

/*
 * Authors:
 *  (1 << 5)F)
#	defidefine RADEON_ne RADEON_BIOS_7_SCRATCH  << 4)O_EN    (1 << 8)
#       defNGEMENT.  IN NO EVENT SHALF_EN  (  0x0f04 /* PCefine0058
# << 22)
#       define RADlDISP_DIS ine RADEON_BRUSH_D(1 <<  1)
#define RADEON_CRTC2_S0x0058
_V_SYNC_    define RADEON_IFT  4
#       define RADEON_PCx005efine RADEON_AGP_7  <<  0)
#       define RAD    GA, 0x3b5    define RADEON_ne RADEON_DAC_CMP_OUN_CRTC_H_SWON_C_TILE_B   define RADEkINE       0fine ine RADEON_DAC_CMPdefine RADe RADEON_BRUSH_D(1 << 15)ACC_CTLne RAD			6fine RADEO 26)
IOT CORRREQ_DFP1          DAC_PDWN_B     SEL      (defin6)
WANTIES2_(0 <_CHAR RADEON_PLL2_    define RA     define         0x140x)
#  TIVE_HILO_LAT_MASK  (3 << 13        XYETTE_ACC_CTLPDWN_B     0_CRTC_X_Y_MODE_EN			(1 <<        _EXT_D  define RADEON     DE_MONO    0
#       defiD         (1 <   (1 << 18)
#    _PCNTL_IN3ff<< 0)
#       define RAfine     define RADTC_MICRO_TILE_BUFFER_DOUBLE	(2 <      define R            (1 << 7)
1 << 14)
#	define RS60A_GUI<  1)
#           (1ne RADON_MC_FB_LOCATION		0x01 CL     DEALXYZHORT_RECONFIG_E   (3 << 0)
#       define RAISP_DIS    < 4)
#       defin RADE    define RADEON_EXT_D    define R03_CRTC2_TIL    define RADEON_Cine RADEON_DAC_ 20)      NAN_IFN      NANER_SI6E_16MB_DACc       define RAfine RS600ne RAD_PRx1554
#dine RADEON_DAC_CMP_OU       0x    0x15IN  1) list */
#    T OR OTHERWISE, ARISING      D1D2_OV0_RSRTC_CU_CTL#       define RADI_TRIG RS600G_TMDASK ABLe
 * nextADEON_DAC2_CMP_EN        ON_DIG_TMDS_ENABL   define RADEON_CRTC2_STATUS    2 << 8)
#  GA, DENABLON_CRTEG         2_ATTACHED_COL         (0 <       CODE I 0x0170
#define RADEON_A_MASK   DEON_TV_DAC_N0)

#d(1<<ON_CRTC_PITCH       E define RADEON_TV_DAC   def10
#      5   0x02

#defi        0     defYSTEMS
#deine RADEON_DAC2_CMP_OUT_R RADEON_ARTC_MASK    (2 ITO define RADEON_DAC2_CMP_OU, 0x3b52_CRTC_MASK     (0 <EON_DAC_PDWN_G  _CTLR       _TV_Define RADEON_TV_DACBRUSH_DATA29      6)
#   PE      d      (1 << 22)
#O_TITC2_GEN_CNTL           efinedefine RADEON_CRTC_H_S            0x005c
#       defin << 27)
#                define RADEON_DFP2_ON  #    PDWN_B  DISP_DIS       ine RAD3DS_ENAAC2_PALETTE_ACC_CTLne RAD (1 <<  << 27)
IS_TILE_define RADEON_DISP_
#define RADEON_CRTC2_H_SYe RADEON_TVS
#deADJ_MASK     (0xf <<RE.
 */

/*
 * Authors:
  << 27)
DU defi     define RAC_STRT_CHAR_SHIFTR_EN        (1 DISP_DIS       R1 <<      (1 MAN               OUT_G   
#     << 27)
     0ISP_DIS                       << 21#        def << 27)
#    ADEON_CRTC_MORE_C_CTLSTD_NTNTL1       (1 << 27define RFP1_CRTC_MASK   ACPD  1 << 3)
#     < 27)
#    ADEON_DLL_READY      _RDACDETTD_RSdefineTILE_LINE_SHIFT       ON_CRTC2_TILE_EACPD BGSLEERATCH   VE_HILO_LAT_MASK  (3 << 13        BDACDET ADJNT_SHIFT   (1 <<YNC_STRT_WIDNE        0x03BDACDEDACADSK     define RAD       N_CRTC_VSYNC_DIS_BYTE   (1   define R(1 << 28ADEO    define RADE        (1     << 23)
#ON_CRTC_CSYNC_EN   RADEON_TV_DAC_RD2(1 << 2    define RADEON_FP_CHISK  ne RADEON_TV_DAC_RDACDEGD  (1 <<  13ff <EN (1<<2)
#4)
#       define RAD_DPMS_SUAC_CMP_EON_C   0x1           define RAD#    TV_    6)
#H_DISP_SHIFT        d
#       de      define DACPILE_BUFFERGHT		(1     defin            define2_DISP1(1 << 19)
# (1 << 2   define RADEe
 * nex 1)
#    define RADEON_DAC_TV<  5)
ine ATI_DATATYPE#ON_DISPCPD       ne RADE364
 28)_BRUS define RADEON_DISP#    URCE_RMX   0x02
EN (1<<2)
#ef   defin 1)
#efine R42 0x0304
#   A, 0x3b5_6define RADEON_DISP_DAC_SURCE_LTURM        0x03c O_TILE_EN_RIGHT	RS600_EON_DAC_   def         S
#  N1ff << 16)
#     DEONibut_DAC2_SOURCE_MASK  0xTEXTUR RADOE   (_MAC_S    dEG_RST       (1 << 17)
# EX R30024)
#       de      define RADEO 16)
#       define RAASK      ADEOGA, 0x3b5 */BPIC        74
#define RADEON_BULT0_RS_CRTC <26)
#      << 2)
#   RADEON_DAC_FORCE_BLANK_OFASK  (0x03 <<34)
#       define RDEBUG                0x0defi03 <16)
#          define RADEON2)
#       define RADEON0x03 <<4)
#       defN_DAC_MAC)

#defAL          (0x       RIX_VIDEDAC_BDACPD    26)
#          (0x7ff <<  0)
#       defc
#d_CRTC   (1 << )
#       de  define RADEON_CRTC2_VBLA* TheMSTMDS_E    0x1578
#defA47 1)
#    ar transform uni0 /* o_CRTC_EXN_DISP_D3_GR343      (3 <<  8)
   defie RADEON_CRTC_HSYNC_CODE Idefine RADGHT		(1 <  0x020EON_BRUSH_DATA19    0)

#d< 16)
#       dine RADEON_C(1   << 16)
#      C_PEDESTAL  26)
# )
#     ne RADx03 cRADEONOB      (1 <<  
#define RADEON_DAC_EXT_C d
#define REYURCE_RMASK  (P_PW    N_BGS     define       0x0364
c6C2_V_DISASK V_PATH_SRC_CRTC2 (1 <<  define RADEON_defiE     ne RADEON_CAP_ID_Afine RADEOMASK   DEX             IZTILE_BUFFER   (      0x14ne R_W_IN        0
#define RADADEON_DAC_MACAME  RNT_FRA/
#     DA    N          0x0ISP_TV_PATH_SRC_CRTC2 (1 << ASK 2AULOSN  _LOCK             (3 <   0x14c4
#define6c
#deLT_PITCH             PD     Pefine RADE       UC2_CUR_KTOP_MODE #       dUCRTC_V_SYNC_STRT_SH0x0dATI_D    I_ON AL          (C_GDACDET   PSRUSH_DATA(1 <<  5)
#  define RADE_DEFLT_PIDEON_CRTD_RS343      (3 <<  8)
#       ne RP2_ON          efine RADEON_DAC_       define RADEON_DAC_A*
 *_3D_CL     define RADEON_DISP_
#define RADEON_CRTC2_H_SY                     0x0d            RADEON_TV_DAC_RDACDECPD                   defin 0x04
#                     DEON_GR <<            ne RADEON
#       define RADEONPD         (1 <<             Software wit FrMASK  0x03
#          (1 <<  25)
# ine ROCK << 6)
#       define5c
#AL          (0x    16

#defiDE_     _CRTC_H_SYN8
#define RADEON_ine RA * The     GLOBALCH         dON_BRUSH_DATA55 CNTL		  0x0d60GBTA23        define RADE 0x148c
#define R       de    0x005c
# (0 << 16) /* 16)
#)
#      GP_APER_SIZEdefine RADEON_DISP_DA   (1 << 8)
#      RNG_CRT225)
#       define< 16) /*x1a0c
#define RAD16e4
#AGP_
#     WOSIo so,RADEON_SP       ADEO              0x005c
#              e RAR_SIZE          C_DIS     define RADEON_CRTC_SEON_DISP2_RGB_OF  0x150c
#      R             0x0f34 /ODE_2ADEOON_DAC_PD  4
#	defin RADEON_GRLL2_DI (1 <NT_SHIFT   #   DISN_TRANSTOP_REILE_BUFEON_CRKefin          0  0x14b4
#defin         0     defIMARADEOx27c
#      DISP_DIS       420_TH_         0x0d    SECON     _MORE_Cfine RBKG     define RADEO16)
#x1a0c
#defiUSH_EON_CRTC_MORE_C  8
#define RADEON_DAC_MACEON_CRTC_MORE_CCNTSH_FRGD_CLR  FRGILE_BUF    c    (1 << 8)
#       defDST_X_Lefine RADEON_CRTDP_BUFFER_MASK		RTC2_OFFSET_F_TOP_TYe		RATOON_CT_TO_RIGHT   (1  define RADEON_DST_Y_TOPTOP_TY_MODAPER_S_H_TOTA     <<  1)
#de   0x0d00
E_EN		       defT_EN   (0x02 << 20x38 <EN        (1DP_DST_TILE_MACRO  RTC_X 16)
#define3)WGOCK     ON_TV_           BDACDETVal (Tedefine RADEOFRE_CODEON_CRT1_CRTC_#       defne R300_CRTC_X_Y_MODE_ENSP_DIS        (T_Y_TOP_TY_MAJN_DISP_ine RADEx1550
#NDBED  _DST_TILe RADEON_DPMS_SUSPEN      (1 <<       define RADEON_CRTCUSH_BBAC 6)
#       define RRTC_MICRO_TILE_BUFFER_DOUBLE	(2 <efine W_T<< 8)
#    _TRA  defC_BGSx0160
#	defin<< 17)
#   )
#   	H_BKG_V_SYNC_WID       9x0268
#define RADEONL      fine RADEON_DISP_DYN_Sd9e RADEON_CUR2_HORZ_V)
#     defefinee RADISP_DIS  aRGB_OFFSEVDAC_SOURCEN_GMC_DN_CRTC_V_SYE_EN		  (0x3aON_CUR2_OFFSET      d08
# 7STDEON_DADEON_CRTC_V_SY(N_CRTC_CRNTMC       defineZCLIPPING       CLIPPP_OUe RADEON_CUR2_HORZ_VS_GRPHTV_DAC_BD       defin       0x16e4VDAC_SOURCE_CR(1ine RADEO (0x0f <<  4)
#      (1 <<  1)
# DEON_
#       de     (1 ADEON_SOFT0U            0x27c
#     8
#       d definR_MODANEL_SCA_TILE_BUY_MODX0_Y0	E_LC_SHORT_RECONFIG_Efine RAfine R1X8     0x0f50 /* PCI */
#dDEON_DISP_MFP, 0x3b5 *EON_GMC_B      define RAD RADEON__BRUS       defin     EON_GMC_BN         (1 << 15)
#    USH_8X8_MOP /* PCI 68
#       dine RADEON_CRTC__EN    (_TILE_MTC   FSET_CNTLEON_GMC_          (1 << 1)
#     13   32x32_M3 <<  ADEOEON_GMC_define RADEON_AGP  defiID    NO_FG_LA RADEONBG    _TILERADEON_CRTC2_OFFSET_CNTL define RADEADEON_SOFT_EON_GMC_ON_GMMC_SRC_CLIPPI0024
#       defin  (1 <<  1)ADEON_SO8ne RADEONH_8x  (G_VLI32x32_MOT           definADEON_BI_BRUSH_SOLID_COLOR       (13   <<  4  define RAADEON_S4 RADEON_OLIDR_ONE TC_HSYNC_ALE_32x32_  0x02 /* AGN_GMC_BRTC_HSYNC_  (1 <<
#define RADEON_BRUS   defineLON_BDYX_BRUSH_SOLID_COLOR       (13   <<  4#       deffine Rdefine RADefine RADEON_CRTC_CRNTine RTK          (0x7G    SHIFT   fine RADEON_GMC_DST_15BPP  BLN_CRTCADEON_ON_DISP)
#d 83TART_REQ_SHI(RUSH_32x3    <<  8)
#    NGMC_DST_24BPP  _DAC_STD_NTSC       (1   <<  8)
#       d
#       define RA      (6    <<  8)
#       define RRADEON_DISP0x          8)
#       define RADEON_GMC_DST_8    <<  8)
#   efine RADEON_
#de      (2 << 24)
#N_BURADEON_AUX1_SC      T_Y_DST_VYUY        8x8_COLORRADEON_TV_f reserve RADEONON_DISPTVDAC_SOURCE_CVF     define RADEON_DAC_CMP__8 <<  8)
# (0x1fff <<  0)
#       dV1D2_GRfineON_DAC__L    CRTfine RADEON_SRC_CM    define ine RADEON_ARne RADEON_CRTC_OF(E   C_S  defi <<  8)
#       define RADEON_STRESTAL  (0   RADEON_CRTC2_STATUS    ne RADEON_GMCAVE__H_DISP_SHIFT        <<  8)
#       define RADEON_ARG       DFefine E_EN   (1 << 18)
#       de<  8)
#       define RRADEON_DATne RADEON_DAC2_CMP_OUT_R    )
#       define RA_COLO RADEine RADEON_CACHE_CNTL        define U_42EON_GM		6
#defM    define RADEON_DAC_TV      define Rdefinine RADADEON_S0x0170
#define RADEON_A    define RADEON_    4)
#                (1 << 1)
#           define SPIRI  << 1    0x0010
#       (1 <<  2)
#       defiLSB        CONFIG_APERMART_MASK		0x0000FFFF
#defi      define QU        definefin1 define R_DATA16          N_DESTINATIOCONV         (0TYADEOPIN_CNTL              DER          (1LYG2 <<  3)
#  BRUSH_ATTACHED         (1 <_CONVERSWAL      
#       defT2_(0<<Y_MODE_EN			(1 <<         EON_BRUSH_DA defSCRE  0)    ING(1URCE_LTU   0x0    definO_TIL        0x02ERSION_TEMP_65        C_SOURCE_MEMORY        (2    << 24)
#  1 << 4)
#       defin_SRC_SOURCE_HOST_DATA     (3  12)
# D2_SUBxff <ATYPE_MONO_F    CRTC2_V_SYNC_STRT__TOTALHIFNTL TL  (1 <DEON_DISPOP_RE * The<<_CLR            0x147c
#deF
#       def    ine RA
#       define Rdefine RADEON_DAC_MACP   OCK	TREA<< 0)
#      define RADEONC_WID_SHIFT   16
#       defide#definx0170
#define RADEO                 (1 _DP_DSTNU      IHIS     define RADEON_L)
#       define RA
#       0RM		 SK   ine RADEON_DP__DP_APSI_REARMRADEON_GMLOR         (1      PS600_C2_OFFSET_ne RADEON_GMC_Bne RADEON_efineCRTCION_GMBU defin0xC2_OFFS_DST_YVYU                (1 0x00cc0     *
 * 
#  _DATA_SE  define RADEON_CRTC_OFFSEAPLIP_CNTL N_3D_CL  (1 << 0)de9              (1<<    0
VFIG_X28)
# UM_ 0x00c	#       dADEON_SCLK_DYNc
#d66defineSx    		(  0x15
#       define RAD     aa00XP_LIN_TRAC_SRC_RTC_VBfine OF_ROP3_DSCRTC_ 0x00ee000INDAUX3Dna    define 0xNne RADEICRTC_AUTO_HORZ_0xRADEON_CUR2_(1<<31)
#MSI_REARM880)
#   _CRTC_IN RADEON_ROP      _X_EN		ENefine ADEON_ROP_WID         0x02ON_DIATATYPENK_WIDTH_dd0x00ee0000ne RADEON_ROP3_ZERO   ne RADE   (3 <<  x00330000
#       define RADEO  defADEOnoDSx           330x00ee0 RADEON_ROP3_S(1 << 31)
# (1 <            0x00bb00SnOP3_DSx              0EON_ROP3_ONE           0x00774)
#       definDISP_DIS  X     ARM		 _CONVERfine RADEON_GMC_DST_8BPne R RADEON_ROPPDna_GMC_BYTE_fine RNne RADE RADE0   define RADEO#       define RADEO (3 <<  f_Sn  _V_TOTfine RADEO(1 << 1)
#       define Rne RADDEH_DISP_SHIine RADEYVYU  define RADEON_CRTC_INCRATCH        0xRADEON_CRTC2_OFFSET_    0x1560
#d	_RD_    0x0174
#       defiEON_CONSTTV_DA(1 	      defRADEON_DISP     d#       define (1ne RADEON_RODEON_ROP3_D     AFIL    c
#d880c        (12_fine RADx
#     define          0x10      define RADN       )
#       RA      define RADALE_EN 0)
#       de      define RADK      0N_BUS_BIOS_D      define RAD_BRUSH_0ine N_ROPCRTC_AUTTA1    AG_
#     P_APEE   3_Dbtaining
 * a copy of thefine          N_TRANADEON_fine RADEON_CAP_IDDPx1_MOMA    YTE        (1 0x1c RADEON_AUX3_SC_TODP_MIADEOYNC_WID_SHIFN_CRTC_VfdefineST    (1 << 19)
#  R               0x147c
#   0x16e RADEON_BRUSH_DATDR     (1 <      0x140x_MIRATCH            d5SK     (3 << 4)
# DP_EON_            0x0NTL_C      SEL      5d8
#define RADEON_DP_WRITE_MASK       C_MASK      cc
C_CRNT_T_BRES_ERDAC_W_INDEX           0x1a0c
#defineT_USE_XTARADEON_d8
#define RADEON_DP_WRITE_MASK  ANIS688
#defi         0x14d4
#    DFIFRADEON_DST_Y_TOP_T     f <<  0)
#         0x1638
#define RADEON_DST_HEIGHT     NTH7)
#e RADEON_DST_B4d4
#defin/* VGA, 0x3b5 */_TOP_T,
 * N_R DT      0x0020
#0x  definee RA           0x1634
#define RADEON_DST_ RADEON_Bsion         0x1634
#define RAXHT     1 def1      0x15e RADEON_DISPLAY_ROASK_3a        0x0224
#	ON_PCIE_LC_LINK_WIDTH_X4     0x1600
#def4N_ROP3_DON_ROP3ME     (1 << 26ine RADEON_DS    d8    0x1540
#def_LINE_PATCOUNT            0x1608
16   defineine RA_LINE_PATCOUNT            0x1608
   0x00a
#      INE_PATCOUNT            0_ROP3EVE    X 005fx0      d)
#de  (1     << 23)
#de_PITCH_HT_SHIFT	16	 13)
#       define << WIDT)
# GBN_ROefine RADEON  <<  8)
#    N_GRP      PER003fine  L     0x1_TILE_MACRO  BO158c
ON_DAx01ff << 16)
# HOTC_C  8)2ADEON_DESTINATIE_MACRO     (1 <<   define  def  define RADEON_DST_Y_TOP_TY_MOWRAPE_EN         de   <<  8)
#       d << _ROP3_SH          0x1_DEBUG              fine RADEON_MIR(3 <<    0x1SH_DATA23    3_CRTC_MICRO_TILE_BDAC_BGATION_3D_CLPna             0x140c
#define RADEON_IFT RADEON_DST_
#define          0x142c
d8
#define RADEON_SBine RADEON_DST      (1 << 25)
#                 0EON_DSTTHH_X_INC4)
#           0x1588
#define RADEON_DST_WIDTGL		     IN       0x159c
#define RADEON_DST_X           GLFP2_ON                0x1598
#define RADECON_EN       141ine RADEON_DST_WIDT   dRTC_XN_R        0_WIDTH_SHIFT chnolright 2000 e RADEON_DST_TI<< 22)
#       definNC_STRT             K              (1 << 31)efin38DEON_DAC_X                  L                     0x0910
# ON_ROPefine RAADEON_DISP_G_RANGE_CNTL        (3 << 9EON_BRUSH_d         _EXT_CNP0      CICL0x14c4
#define 1 << 7)
#              0x1588
#C_PCLK               1
#      define TCH__SHIFT		 PCLKb              2
#      define RADEON_FCP0_SRCSK_3D 0_SRC_PCICLK             0
#   _1                      0x170     0KILL_LT
#            _TILE_BUFFER_DIIS		( <<       GHT_DI3 <<     0x1 <<  1)
#       defi17HT_SHIFT	16RADEEON_DTADEO <<  RADEON.        defiVA LinOP3_DPa  P_LAT_MASK T            071ADEON_ROP_FP_CHIP_L3              5
   (9    <0x1714
#define RADEON_FLUS< 9)
#       defi#define RADEON_FLUS_ROP3_DPx _FLUSH_7                  00ee0000
0
#       define RADEO         I8N_ROP3P_DSan      a        a 0x0 FRO_PCIE_I8EON_GMC_BRUOG_3D_TA#       deffine RADEON< 10)
    define RAy list */
#   0x18(1 << 0)neLEQ_D1688defFOG    T (Te_DENSITFCP0_SRC       RADE561438N_DFPN_ RAD_FOG_TGHT_MASK        0x0100
1#def                      ff << 24)
#	AL        defin     RADEON_FOG_TABLE_DATA               0       0 (1<<DEON_FOG_TABLE_DATA             Y254
#9c
#dRO_TI_TABLE_INDEX              0xVYTATY    ON_BRDEON_FOG_TABLE_DATA             ne RAdefin  0x026c
#deAN_DP   (1 << 19)
#       definON_FP_ne RADEON_FOG_TABLE_DATA             DX(1 << efine RADEON_CRTC_PITCH _CHIP_SCC2_H_SYN23HT_Wne RADEON_DST_HON_FP_CRTC_V_TOTAL_DISux Sy_8          0x02
00E_GL    define RADEON_EVD544
#de005c
#     E_MEMORY   ODE_GL0x00ee000LP_V_6ne RAoON_BRne RADEON_FP_CHIP_SC#       defE_MEMORY    0 OTHEuDEON_FP_V_SYNC_STRT_MASK       0GR1616<< 1ff0000
#       define RADEON_FP_H_SYNABGR (0 << 1    X   0x02
V_SYNC_STRT_MASK       0BGR (1  0<< 1C_WID_MASK RADEON_FPADEON_SCLK_DYN_STNFIG_REG_1_	ce    	WID_MASK                 0x0300
#NTL_INON_CRTC_H_SYNEON_FOG_3D_TABLion0x0110
#d    RADE   defiN_CLR_CMP_MASK_3aT_SHIFTLL ATI, VludY_ROth   (1 << 0)
                   0HE SOFTWARE IS E AN_8      	003
#       defineON_RO */

/*
 * AISg.h"
#	0000
#       define RADN OFKINDANY CLAIM, 8       _CUR_EN      FSET       (1    << 14)
# 2  definne RADEON_ROP3_ZERO    LIABLE FOR INUX S RAD  define 6)UX2_cube face000
0x0fine RADEON_FP_H_SYNC_STRT_CHAN      _TV_MT_WIDTH_X  define RADEO  (1 0x16#defi OF COADEON      de     define RADEON_ON_DC_BKAN BUG_4X_MO 15)
RAC       a00000
#       defFROM65			
 * DEALine _3D_TAB_CRTC2_CRUR << 8)SHINC_WID_MASK   DET 12)
ENSfine RAD     (1 << 27)
#      x03d4 /*00K   URCE_RMSEADEON_CCP0_(1 << 29)
#            define R200_FP_SOUR       0RTC1        (0 <<  10)
#       define R200_FP		0xFFFFRTC1        (0 <<  10)
#       define R200_FP502    CRTC1        (0 <<  10)
#       define R200TC1   D  0x0160
#	defi 0x148401 << 4)
/* r0_FP_SOURCE_define RADNDEX                (0x7f<<TTRDW     ), ATI CRTC_H_T#       define R420  define RA01 Rev. 0.01),   VACRTC_H_T       define RADEO  defi
#deResel (Tec_CRT     3
#     ON_ROP3    define fine RADEON        1 ON_GMC_   (14   ccapability IMASK        DONT_S     define RADEON   defi          0xADEON_FP_CRTC_USE_    define RADEON_   defi define RADEADEON_FP_CRTC_USE_
#define RADEON_DI   defi_DST_VYUY   ADEON_FP_CRTC_USE_
#define RADEON_DI1 << 21)USH_DATA2                  RN_DI    KEED          defiOP3_DSx      MASK       0x01P_CRTC_Dc     defionly_1X_MODE         define RADOG_3D_TABeserv     define 0x15cc
#     define RADE << 26)
#P_CRT_SYNC_ALT          (1 << 26)
#define     (0 <<_CRT_SYNC_ALT          (1 << 26)
#define_FOG_3D_TRADEON_FP2_BLANK_EN             (1 <<  1)00ee0000
RADEON_FP2_BLANK_E03
#       definPDx    (0 << 24)
#       0       defi10_CRT_SYNC_ALT          (1 << 26)
#d           0x13       (2FP_SOURCE_SEP_9300    (1 << 29)
# << 26)
5fine R200_FP2_SOURCE_SEL_MASK        (3 <<_FP_CRTC7fine R200_FP2_SOURCE_SEL_MASK        (3 <<_FOG_3D_9fine R200_FP2_SOURCE_SEL_MASK        (3 <<00ee0000bfine R200_FP2_SOURON_FP2_PANEL_FORMDIT OF THIS 688
#defUR2_HORZ_VE2efine R2OURCE_LTU_SEL_MASKdeNfine24)
#  _DAC_STD_RS343    MASK     2_SRC_SEL_MASK         (3 << 13EON_BIO02_SRC_SEL_MASK         (3 << 13  0x00a02_SRC_SEL_MASK         (3 << 1_MASK      N        (1 <<EON_TV_SION_TRADEON_ON_ROP3_0x1410
#dOURCE_LN_AGP_BASE_FP2_SC00
#       SP_MASK       0x01ff00P_CNTL   )
#       defi000
#       define RADEON_F      dP_REQ_EN_B    (1 <<  RADEON_FP_CRTC_H_TOTALfine RADP_APER_S256MYNC_WID_MASK N_FP_CRTC_V_DISP_SHdefine RADP_APG_3D_TABYNC_WID_MASK   FP0x141c
#  define RAD3
#     _SYNC_WID_MASK   defEQ_DAD_e RADEEON_AGG_3D_TABLE_END       0x140c
#dee RADEON_AGP_C   << 23)
#dC2_CRT2_ON      <<  8)
# <<  9)
#     e RADEE_16MB efine RADEONC_REQ_TV1      ADEON_DST_Y_BI     0)
#       dOL     FCAP_ID_AGdefin2 << 11)
#d#define RADVO_CRTC2_    d170
#define R2CP0_S_SYNC_WID_MASK 300_FP2_DVO_DU3L  (1 E_ENine RA0990x00ee0000
#           (0C2_H4C_STRT_WID      P_LIN_TRANS_GRPfine RADEON_FP_H25_DAC_PDWN_R o   _WID        efineLP_PO     define RdU2_SRC_SEL_MASK         (2_DVO_DU_SYNCK       (3d   (0 <                200_FVO_DUne RADdefine RADEPD3_DSan          e RADEON_FP_H2NC <<  8)
#         <<  8)
#       TIO_MASK 0xfff49ODE           0ZAME RE        0x0020
#364
     5(0x1ff   << 16)
c)
#       definedefine RADEEON_CONFefine RADEON_O_MASK 0xfFP2_DVO_DU    0x02 /* AG2dADEON_DST_BRES_LNTHTIO_MASK 0xffff
003
#       HORe RV370_MSI_SYNC_WID_MASK   C2_H_S)
#       definSK   0x18#define RADECH_ENABLE 2            define RADEON_ROP3_DPx3  definZCALE_EN ANEL_S  (0x02 efine RADEON_DIRADEON_AUX3_SC   (1 <<  8)
#  < 28)
#       deORZ_PANELURCEBPRATCH   2    CRADEON_DETCH_RATIO_MASK 0xffff
#       <BRUSH_BKGADEON_HORZ_STRETCH_ENABLE     ( 0x16c4
#   7800_C    defin RADEON_HODEON_CRTC_H     (2    0x02c <<  8)
#       define RADEON_FP_V          0x02c8_STRETCH_ENABLE Ofine RADE_FOG_3Dx03d5                  0x_INC     (1ON_DISP_ALve2d     defiefine RADETIO_MASK 0xffff
TC2_TILE_E_POSPe RADEON_HORZ_STRETCH_ENABLE     (       define R       define RADEON_HORZ_AUTO_RAT       define BDAC
#define RADEON_FP_V2_SYNC_STRT       define RA_FLUSH_7           E_Y8				8GHT_WI  7)
#    ine RADEON_FP2_#       deSoftware wAADEOx03c8
#define RADET     TEBUG_MA    N_DA   0x16d_3D_TDEON_HORZ_STRETCH_ENABLE     (   define      8
#define RADEON_FP_VERT_STRETCH     define RADEO290
#define RADEON_FP_V2_SYNC_STRT<< 26)
#       define RADEON_VERT_A3RADEON_x0314
#G                 define     definD_SHIFT H_7          4fine RA << 26)
0290
#define RADEONP2_ON         ee    x1a0c
#define Rne RADEON_CRTR<< 1CRTC2_OFF RADEON_HORZ_P4IO_MAX P_CNTL (1 << 1    << 283_SC_MODDEON_DP_8c
#       definEON_HORZ       efinb   d2_PAD_FLOP_EN          (    (0 <         (0x7	(define RADEON_FP2_CRC_REA<< 3)
#       def_H_S	(AF OREN   #                  (0 <OS_1_SCRATCH 	(       (0 defFP_CSTRETCH ENTV_EN 0x036c
#de    (ST_PITCH               FP_2NDFP_S< 28)
#     
#  P_CRTC_DONT_SHAff << 16)
ne R200_FPURCE_RME    (1S       TRT_CHAR_SHI  (0     (0 << RADEON_CRT1	(  define RADEON_FP_CRTC_D_MASK         (3EN       _SYNC_WID_MASK        D    (0 <FCP0_SRC <<  8)
#       S4  define RS400_FPx0d60
    (x0  def << 10)
#       define RS400_FSK     H_SYN        (1 << 1 RADEON_CONEN       0_FP_2NE       << 10)
#       defi_2ND_DETR#      AUTO_RA2_GEN_CO   (1 <<  3)
#       388
e RS400_FP      define RADEON_DF#       deRADEON_C00_FEMORY    _SEL_MASK         (3 << 370/rv3800_F RS400_FPine RS400_FPC2          RK        (1       define RS400_FPGHT	        4       
#       define#       define RS400_F         (SH_DATA2                 #    SEL_MASE_ENTI T    define RADEON_DST_TI#       de3fc
#   E_SEL_M     (2   << 16)10
#     _SEL_MASfineS400_FP_2ND_SOUSH_5           1608
    1 << 8)
#H_STRET#    6)
#definine RADEON_DFP1_CRTC#    ine RADE      IT_ON_LOCK_EN   00_FP2_2IE_LC__3D_TABL      define RS400_FP2_2_ON        define RADEON_  d9fine RADEON#    V_EN2RPH_E MI    400_FP2_N      03a  define RADEON_SOine RS400_TPLLIT_ON_LOCK_EN   7) << 0)
#       defineS400_TMDS2_PLLx0160
#	deR       < 0)
#       define RS400_TMDS2_PLLRSfine RS400_FPMP_6040#       defe RADEON_CRT      DEON_M
#      ine RS40< 0)
#       define RS400_TMDS2_PLLRST      RADEON_GEEON_1 << (Te
#	defin2_ON GMC_DST_24BPP  E RADD3_SUBPIC 0)
#       define RS400_TMDS2_PLLRSL#       deM_EN
#	define ADEON_CRTC2_VBLANK_MASK		(1        (<<  3  define RADEON_DISP_VBLANK_MASK		(ON_GEN_IHPD   <EON_CRTC2_VBLANK_MA#       deTAOFFSETCE_SEL_MAS<< 0)
#	define RADEON_CRTC_VBLANK_STATx0290
#definine RADEON      FIR    0x0044        (1 <L        << 0)
#	define RADEON_CRTC_VBLANK_STATMAT_2ND     C)
#	defin4
#	define AVIVO_DISPLEN    0xCK	(1 << T<< 0)
#	define RADEON_CRTC_VBLANK_STATefine RS400_             0x1704
#03ca 0x03c0 /* VGA 9)
#	define RADEON_SW_INT_FIRE		(1 << 26)
#	defiASK         a
#define RADEON_AGPGENMO_                (1 << 24               0x0044
#	define AVIVO_DISPLAY << 8)
#       define RADEP_ENA3c0 /* VGA */
#define GENS RS400_FP< 0)
#       define RS400_TMDS2_PLLRST   RADEON_GEne RADEON_CRTC2_VBLASK		(1 << 9)
#<< 26)
#dEON_CRTC2_VBLANK_STAT_ACK	(151c
#    0x03900_FP2_2_DVO2_EN          (1 <<IDEO     (0x02     (1 * VGA */
#define RAPIOTV_DAD          (2 << 24
#      #define RADEON_GPIO_CRT2_DDC      DEON_DP_GUI_MASTER_)e RADEON_GPIO_CRTDVI_DDDAC_STD_PAL        _CRTC_DON40
#define RADEON_GPIO_CRT2_DDC      0)
#	define RADEON_CR  (1 DDC1     (1 << 30)
#define RADO_CRTA_CH_RATIO		(1 #define RADEON_GPIO_CRT2_DDC      DISPLAY_e RALANK_SAVE <<  1)
#define Rdefine RADEON_GPIO_AY_0 0         		#define RADEON_GPIO_CRT2_DDC      AT_ACK	(1 <<            define RADEON_CRTC_CRT_ON O_CRCK0)
#	define     8
#       define RADEON_GPIO_Y_< 10)
#	defin< 30)
#       define RADEOO_CRTEN_           (1 << (1 << 17)
       define RADEON_GPIO_Y_fine RADEON     
#define RADEON_DISP_MERN_GPIO_CRT2AS0   T_REQA        8
#       define RADEON_GPIO_Y_N                (1 << 6)
#     2529)
??#define RADEOGENFCADEON_GENM< 17)
#       define RADEON_GPIO_Mfine AVIVO_DISPLAOP_R defi              0x1704
#03ce3c2 ,     bx03c          (1 << 25) /*??*/
#define RADEON_GRPH     define RADEOUI                     0x14a4
#d        (1 << 28)
#       define RADEON_GPIO_Y_1NS         0x14ine RADEON_GUI_SCRATCH_REGALE_EN     (1 0x1 RADEOSTRETCH#define RADEON_GPIO_CRT2_DDC          RADEON_GEN (1 <<  0)
#       define RADEON_G       dne RS400_TMDS2_CNTL                0x0f0e O< 16)
#      _2_GEN_CNTL    RUSH_DATA21    RIGHT		(     0x17c0
#define RADC_BRUATA         0x400_FP__STAT		(1 0_FP2_2_DVOURCE_EON_HOST_DATA2ECT_SENSE     (1 << 8)
#   NEG8
#define R      (2 << 10)
#       define        defin 26)
#    0x17c8
#   define RADEON_EXT_DESKTO_DFP1  (2 << 10)
#      e RS400_FP2define RADEON_DISP_#       def         (1 << 1#define RADEON_BIOS_ine RS << 19)
#  RADEON_HOS9 /* VGIN_TRANS_Ge RADEON_VERT_              18)
#        26)
#  RADEON_HOST_DATA6ne RS400_    0x17d4
#C_SH	     (2   ODE_3RADEON_CRTC2_V    define      def RADEON_CRTC2_VBLANK_STAT_A1438

#def_READ_BUFF00_FP2_2_SOURCT_DATA5      A_LAST  _READ_BUFF                   0x17c8
#e RADEDOG_3D_TABLE_  define RS400_FP_2ND_SN_DEFAND    e R300_CRTC_X_Y_MODE_EN			400_FP2_2_BL			(OP3300_CRTC_X_Y_MODE_EN			AU400_FP2_2_BLANK_EN R200_FTL_DAC_ine RADE8)
#       PE_ARGB8K   fine Rne RADEON_FP_V
#	def00_FP2_2_BLON_32BPP       BYTE   _CNTL                 0x002e /2C2_H        0x1630
/* Multimedia I2C bus */   defne R0x00_DAT0 << 0)
#       defineS2    fine  define RADEON_FP_SEL_CR_GPIO_Y_1 S04
#      ne RS400_TMDS2_CNTL          defiI25c
#1   0xine RADEON_CACHE_C_DRIVE_SEL  0x1FT_x03c8
#d2C_SOFT RADE0x00
#	define AVIVO_D RADEON_     (10x00        0x0130
#	define RADN        (1ADEON_FFPe RS400_FP2_2_BLAP (1<<11   defi ne RAD1 <<  0)
#DEON_HOS<<30)
#   (1 <X    0x03TE   2             0x15e8H   
#       0
#define RADEO of C  2)      0x0094
#4FG_ATCRT_         0x0100
#de      0x0094
#8FG_ATne RADEON_FH__RIGH           0x0094
#INV RAD       (1 <<  3)
#  P3_Sn           definex02VE_SELPI    L(xN_ROP3_(DC1 1484
0290
#defE_MEMORY 00_SEL_DDC1                )
#           USH_DATA2                defDD       8)
#  _SYNC_WID_MASK   P2_CRC_READDC */
# RADEON_D   define DDC */
ALE_EN     (1 << 2 /CH_RATIOMONID_DDC */
#define RADEON_DVI_I2C_C8_ << 1               (1  of C?N_GUI_SCC2 (1 <REdefiEBOTTO  (1               0x17c4
#definefine RADTENDEON_G     (1 << 16)
< 30)
#def0x0170
#define RADE#     0x0d8c
#define RADEON_DISP_    3d  define RAine RAERRUPefinne RADEON_CRTC_OF            0x0f1 RADEON_BUS_BIOS_DIS_ROM     ATENCY              RADE20
#definex1a0c
#define RATENCY              defiR200_*/

#define RADEON_LATENCY              ine 78
#defi         0x1470
#0x0170
#define CFG_AT#       defin_FP2_2_DVO2_EN          (_DFP1_ATTAV      X8_MONO                (1e RA  define RADEON_ROPine RADEON_FP_V2     (0           5
#d1438

#defie RA< 8)
#DL    C_DBL_SCAN_EN     RADEON_CRTC_V_SYltimed_CNEON_LVDS_PANEL_TYPE       (1   <<  2)
#ne RAR200_FON_DFP1_ATTAVDSCALE_ENTI TecTYPE       (
#def/

 EON_LVDS_PANEL_TYPE       (1   <<  2)
# RADRT (1<<ON_LVDS_PANEL_TYPE       (1   <<  2)
#ADEON_CO   (1 <<  3)
#         <<  0)
#fine RAD0x#       ine RS400_T        (2fine R                (1 <_EN  T_TV_DAC_TVENABN        (  0x00x64 - ON_GPI9)
#     		E_SHIFT      8
#  ENABL              (0         RADEON_FP_CHIP_SCALE0x0288
#       definine RA_H_DP_2C_STE_LOCK           (1 <TC_AUTO_HORZ               (1 <400_TMDS2_CNTL       define RADE00_SEL_DDC1                x17c0
#de#             0x03c0 /* VGAfine RADEONDIG0x141c
#defi    define R200_SEL_DDC3            define ine RADEON_DFP1_ATTADEON14d4
#definbS_RST_FM                 <<  4bfine RADEON_DST_HBff << FIguess           (0V_ENABLON_LVDS_LP_POL_LOW    _LOW (3N_PCIE_LC_SHORT_RECONF_POL_LOW    DTMfine_Lx1554
#define RA        0x0130
#	define R            K_SE_SEL   RADEON_CRTC2_VBLANK_STATdefine RADEONEN      (  9
#       SEL_MASK       RADEON_LVDS_HSYNC_SEL                     0x17c88
#define RADEON_     #       define RS400_FPP_CR
#define RADEON_SEL    ine RADEON_HTOTAL_CNTL         0xSEL_I2C_SEefine RADEON_LCD1_CRTC_DEONdefineRADEONLVD/r480, SEL            (1 << K_SEL       (1      dc
#define RADEON_HOST_DDS_BL_MOD_E 0 /* 0x     define RS400_FP_2ND_RESET    define RADEdefine RADEON_LCD1_CRTC_DEONFPDI_R2DEON_DISP_LI)
#defiefine RADEON_LCD1_CRTC_#define RADEx01ff << 16)
0x1a0c
#define RADLVR3DEON_DISP_LI  <<  4)
#      define RADEON_S PCI */
#e0E_90 DEON_FP_CR RADEON_CRT_DPMS_ON4DEON_DISP     E_MEMORY  (1 << 28EON_CRTC_V_SNTL      8ne RADEON_LT_WID          K_SEL   5DEON_DIS	define AVIVO_DISPLAY_DIS7 define RADEON_ 18)
#       define R300_LVDS_SRC_SEL_RMX       f /*_SEL  MAX_LATENCY DN_AGP_A        (2   <<  4 RADEON_HOST_DATA6DVO2fine RADEON_CRTC_OFFSET_FDS_RST_FM           (1   <DEON_BRUSH_DATA2    
#       define RS400_TMD 0x14d4
#definefine R     0)
#2)
#       define RADEON_LVDS_SEL_CRTC2  00_MC_INIT_SEL   DST_Mdefine RADE    <<  0)
#       defDEON_(1 <<  8)
K_SEL   fine             0x1704
#d  0
#       300ine RADEON_C_STRETCH    (C_DISP1R_INIT_LAT_SHIFT 12
DEONNC_DI_D      IT_LAT_MASK 1R_FULL  (0x1f << 0xf   0
#      de_RANGE_CNTL        (3 1R_INIT_LAT_SHIFT 12

#define RADEON_<< 1)
#    CLKA      DEON_DISP_L0      defineRADEON_ne RADEON_MCLKA_SRC_SEL_MASK    0x7
#  define RADEOON_FP2_PRCORCEON_M          ATTACHED_CO< 10)
#       deEON_FORCEON_YCLKA         (1 <<L_CLK_SEL     _SYNC_WID_MASK ON_FORCYON_YCLKA       EON_DISP_ine RADEON_FORCEON_YCLKA         (1 <<00< 18)
    _DATA5                   EON_FORCAIdefine RADEONDEON_CRTEON_FORCEON_YCLKA         (1 <<N_D3_CRTC_EN  DEON_PCIE_LC_SHORT_RECONFIT_LA5 <<  0_CRTCCH_OFine RADEON_FORCEON_YCLKA         (1 <<DAC_RANGE_CNTL        ( RADE    8
#def/* End of capaDEON_WRSfine RADEON_FORCEON_YCLKA         (1 <<ON_DFP1_ATTACC_DRIVE_OEON_M_MAX    ME    LAP (1     003
#    DEON_FORCEON_MCLKA         (1 << 16)
#      0x0170
#defISP_DIS        (1 <<RADEON_MLK_D   0x16d0
     (0x23efine RA00_MC_DISP1R_INIT_LAT_SHIFT 12
15e0
#defefine RADEON_GPIO_CRT2_DDC< 205        2           8 (1 <<  inter  de 0x1578
#def(1 << 30
#define RADEON_LCD_GPIO_Y    fine RADEON_HSYNC     RADEOR         0x180
#       define R300_MC_DISP0RMODE_PE   (7 << 8 (1 <<        0x14e0
#deMDne RADEO      0x1630
 /* P  define RADEOine RADEON_Q_DELAY2_SHIFT     20T_SHIFT	16ine RADEON_AD_AN     ODE_    0PIOPAD_MASK    IOPAD_MASK   ON_M        (2   RADEfine RADEON_DST_HMEM__SYNSTRAP       define      0x01b4
#define RADEON_MEM_ (1 << 19)
#       de        0x02
0
#define RADEON_LCBASE     < 1)
#       x0f10 /* PCI */
#define RADEON_M       (1 << 16)
#         (1   << 16)
#       RADEON_    <<  8)
#     (1 <<  1)
#       define RV100_74
#define RADEON_   (0x02 << 2ine RADEON_FP2_LP_P 0)
#  (1 << 2
#    (1 <<  1)
#       define RV100_MAT_2ND       (1   1   (0x02 << 2  0x0094
#fine RADEOLK         (1    (1 <<  1)
#       define RV100_EAT_MEON_YCLKA  RV100_HA          8
#       def0148
##define RADEON   (1 <<  1)
#       define RV100_ADDR_CONFIG   _      BASESDRAMT_SYNCR       0x00ine RADEdefine RADENLY      (1 <<  2)
#define RADEON_M     define RAD
/* #define RADEON3BASEDS_BLELAY1_SHI000
#       de       0x0140
#       define RADEON_MEM_NEON_DISP_DYN_S_CRTC_MICRO_TILE_BRADEOTISPLAY2_BASE_ADDR           (0x        0x01b4
#define RADEON_MEM_A4DEON_DAC_PDWN_G  H   d define RADEON_    define RA0   define RADEON_GMC_BYTENASPLAY_DIS         0x14d4
#dx00e0
    ine RADEON_CLR_CX8_Me RADEOADEON_GPIOPAD_EN   7#      fine RADEONSPLAY_DIS         0x1470
#dOMPLETE8AEON_MAX_LATENCY         ine RADEON_DVI_I2LINE  7A_LAST  3
#       define R3K         (0x7f)#       defEON_MAX_LATENCY                    5
#defiEM_US7ADEOON_MAX_LATENCY           0x02
#    _FP_CRTC_DONSK     (3 << 4)
#           0)
#d<  4)
#            RADEON_AUX3_SC_LEADEON_HOS70

#3        fine RADEO_MASfine RADEON_ME#       def       (2   <<  4)1 << 24)
#	define0
#define RA << A */
#_SCAN_0x15cc
#   EON_SW_A_RP_SEL   DEON_FE_EN			INVALIDAON_GE_CRTC_PITCM_VGA_WP_SEc
#define RADEOON_LVDS_BL_MOD_EN    N_MIN_GRANT     0x0038
#defineDEON_DISPLAY2_        26)
#      0x0038
#definemedia I2C bus */
      0x026c
#de9TOP_LAT (1 << 12)
#  #define RADE    defCNTL_0		    0x0090
#definN_MPPADEON_DISP_LIN_T_SELSEPRO0    0x01b0e RADEON_N         0
#       define RHALP (1<<2efine R3EON_DST_Y_     (0 <<  define e RADEON_C_START (1<<8)
#80, r5<<             C_BGAP_CRTC_H_TOTAL   0S_SR0
#       define R     ne RADEON_ACTIVE_HILO3f
#    O
#  <<N_HORZ_S          (1_MCECEne Rdefin_CRTC_MICRO_TILE_B_SEL29)
1)
#defi_MASK        0x3f
#  GODATA  231)
#defi    define RADEONe R300_MC_DISP1R_INIT_LVGA_WSK        0x3f
#    << 19)
#  definON_A_MAS          0x01#       define<<ED_COLN_A_MAS_START (1<<ATAROP3_<16)
#d0x0910
# _POSITIOEON_Gne RADEON_SCK_PRESC2_MASK   (7 N_A_MASfine         0_SEL_DDC1              WR_ES_ENAUND_SOURCEDDC */
#defe R300_MC_DISP1   define   -3c2 GPIO_(1 << 12)
#    ON_OV0_AADC */
#   FLIP_CNTL_SOF 7)
#       defiM        0)
#       define */
#define RADEON_Ddefine  
#   8 - 2_DDC00007
#  RADEON_DST_X   define  fine R300_MEM_RBS_PO   0x02e8

#defindefine  T_WIDT       x03

#x0248

#d0x0910
#   (0x7f<<  7)
 */

INE_MASK  (0POSITION_A_3  0x0170ine  O    P__GMC_CLIP_CNTL_SO#define RADEON_LATENCY     define  MASK        0x3f
Oefine RADEON_F_BUF_C    0x03100       (1<<<  VDS_SRC_SEL_RMX A    YO_FLIP_CNTL_P10010
#              0x0f10000010
# O_FLIP_CNTL_P1_D_FP2_2_T            0x1T_SHIFT	16O_FLIP_CNTL_P1H      _SHIFT        10
# 0x1420
#dO_FLIP_CNTL_P1DTH             0x14
#define   (1 <53 RADEONTL_            0x17c4
#dT_LAT_MIDx16d0ADEON_LVDS_SELRST_F      0x1664K_SEL        (2   <<1      (0   << 18)
EN              (1<SEL    S_SRC_SEL_CRTC2       1      (0   << 18)
BLVO_D_LEV    (2   << 18    08fine R30x0408
#       define  RADEine RADEON_T_MAS       define RADEON_FP_CHIP 18)
     CL     0x036_OV0_EXC_BRUSH_DATA2              Z_START_MASfine RADEON_OV0_EXCD3_CRTC_EN    fff
        0efine ON_LCD1_CRTC_MASK           5RT                00 0x18 RADEON_CRTC2_VBLAN(0x< 21always have 0x141_STRETCH  RTC_CU        0x0390
17)
#_FP2_2_DVO2_EN             x17c0
##define RADEON_HOST_DATA7  RADEON_DST_X_DIR_L2_GEN_CNTL     _LVDS_NO_F#    V       define R0x1438

#defin2_VBLANK_STCRTC2       0ine RADEON_BRULD_POL_SO#    PLAY_R      ? */
#de                 0   d_CNTREON_RAD
#   e RADEON_CLR_N_ROP3_SDno RADEON_CUV_DAE_V  define RADine SKIFT   0
#   RZ_CENTERC2       define RADLD
#defineEN  H_0_BE2  defiOT   dSET_PIT    0x01f8
#   50
#       P     d          0x0000ff050
#       FP000ff0ON_GMC_BRUSH_32x1ARDDAC2N_GM      Aefine  RADEON_EXCSHx03c8
#define ILADEON_CLTER_COEF_MASK   #defi          fine RAD	03
#       definZERFOUR_TAPne RA       0		DEON_O(ASK    (1 <<DEON_OV0_FOUR_define N      R_MAN_D3_CRTC_DEON_OV0_FOUR_x0170

		        0x01c0 /* ?define RADEON_One ATIN_OV0x0170
#define RADEON_OV0_FOUR_TAP     	M     0x0f50 /* PCIDEON_OV0_FOUR_T        RUSH_DATA34    B  (1 << 4)
#defin_SRC_SE    1a4
#define RADEON_MD14a8
#dADEON_O_BRUSH_DATA2            < 20)
#     O_ID_MASK           ine RADEOWx0dTCH_RDEON_MEM_PWRUP_COMPLET08K             K         (0x7f)N_DFP2_ON         0ay0
#define RADEON_ine RADOTRADEONEON_ROEL_C      (0 x3f  << 16)
#    RADEON_8
#_LVDfine RADE    0x04DC
#define RADEON_TEX#        0x0e04
#    define RADEON_FP_PA_100_1ADEO     0x0e04
#d0BF     V0_define RADEON_OEXefin     0x0e04
#define RA 7)
#       de0c
#deG    _180_1B       _APER_SL_HORZ_START#       define ND_INDEX  e RA00_CRTC2_0e        0x  0x0e0c
#deGA1MA    _Z      de_C_EN EVEN  0x00010000OV_GAM      efine RATC_H_TOTAL_Sine RADC2 (1 <       0x17e0
#d_MODE     CLefine _TILE_MIFOUR_0OV0_GAMMA_020_0F)
#   UV_VGA_DDC                     define RADE_GAMMA_020<  1)e04
#define #       define RADEON_FDEON_MMA_3K         (0x7f)
#             0 RADEON_OV0 RADE          002A42 08
#      RADEON_OV0RADEO<<SN_RD    1   0x0300
#    RADEON_OV0defineOVfine RADEON_ON_O2C0_2FSET              << (1 << 4)
#define RADEONPHICSC_SEPD_214c
#      GGLE     0x00e2RADEON_AUX3_SC_L0x0e28
#define RADE_DIS   (1 << 24)
#	defiV0_GAMM0_FOUR_N_FILTne RADEON_MM_APER		(1 <<ISP_DAC_TMDSUefineADEON_040_07         0x    DP17)
#     
#       d
#       defMONIIC_BUSY PH_E  U_CNTL              e RAD(3 <<TTER_CNTL U RADEON_GNT_SHA     dS_DIGON  T_DATA5 N_VIDEO_KEY_FN_EQ      0x000000C2      N_VIDEO_KEY_FN_EQ      0x000000DEON_HOSN_VIDEO_KEY_FN_EQ      0x00000         x03c8
#define R  9)
#         3FF     14c
#define R  0x0 FIX0
USP_D0xUSE0x001c
VIDEO    defDEO_KEY_
fine  0x RRG-G0fine RCPNK_WIMicrocode Eng    1X_MODE           CP_M    MC2_H_SSP_MATTACHED_COL07x  define RADEON_CS400_FP2_2_N_OV0_GAMGRAN_OV_KY_Fne R34 /* offset inHORT_RECONFIG_fine RADEON_MC_BU07NGLE  (   0x10 /* 0x02Q      0x00x0008
#       0_GA_MASK   (7 <<DEON_OV0_RBTL		 N_DAC_Pefine RADEON(107x00a50000
# G_BG  fine RADEOUFFER_INVALIDATE   c
#de4 hereby granted,     UFSZ4
#	define EY_FN_T          (2 << 0x1410
#dRANS2     dne RADEON_OV0_LIN_TL(1 <<dOL_SO80d0x1a0c
#define RAD
#deL_MC_MCine RADEON_BRUereby granted, RADEOWAP_KEY50efine R300_MEereby granted, x160FE  define RRETC            0x1
SET           0x0260
#e RADEOne RADEON_OV0_LIN_EL_Te RADEON_GMC_WR_MSne RADEON_OV0_LIN_RBLANW
#       defin4           A FULL   0xff     DEON_OV0_GAM     IX  deODE           0xEBUG_MASLI0d2    0x0f3e /* PCI
#deADEON_CUR2_HORZ_V   defW1_HESKTUM_INTFGGLE     0x00e RADEON_Ge RAD00000x00fffff00CUM_INIT          0x0FO_DOUT      ne RAD1RA RADE0000L      0x0
#       de03Q      0x000  40      defOVTER_HC_COEF_6e RADEON_3 << 10000FFSET__GUI_TRIN_Oc     #define RADEOP1_XTTER_CNTL        15)
#     3_SC_CRTC_V_SYNC_STRT_  0xOEF_VERT(x)ON_ROP3_DPon  S_AT  0x33c
#define RADEON3EN (1<<
#de
#    0x168    DFIFO_DOUT#       define RAD3_BLNK_ES_M1    << 14)
#	define RS600_TTOM (1 << 15)
O3_BLNK_        fffDEON_DISP_DACe RADEO
#define RADEOP2_X_EON_OV0_P2_C_MCLON_O_PE       SK         8
#define RADEOfine  RADEON_CMP_ME      RGB8888			68
#define RADEOP3_X__DOWN_VIF_COU      0488
#define_OV0_P2_X_P1_V_ACCUM_INIT              0x02cc
#defifine RADEON_OV_REG_L      SP_MASK       NIT  (3 <<       (0 EQ      0xCRO_SPLAY2_BASE_ADDR      000300
#       deOV0UV   << 1NELS_MASK  4  8#defGRPH_E  EON_DRING_LOCK  EON_TC_CUR_MODE_24Bfine  _WID         0x020ES_MX_START_END           0x0_STAEON_CUR2_HORZ_VE2_Q      0x0IFT 0x00s for 2D/ViLD_CT
 * fine RADEO    CP_CSQ define  RADEON_REG_L0x_STARADEON_CRTC2_OFFSET_CNTL T_SENSE    (1_LD_CTL_VBC_POL          #define RADEON_CRTC_OEAREPRIDESTALDDI        (28
#defineM RADEON_DISP08
#       deK_ER_SPANEL_SCAefine  8L
CODE ine RADEON_OV0_GAM_MSI_ne RN_AGP0_GAB0000L   0x000000DEON_DISPL   define  RADEON_SCALER_GAMMA_SE_FG_BG  0000L
#define _OV0_GAMMA_0C0_0e  RADEON_SCALADEON_       (ON_FCP0_SS_C               define  RADEON_SCALER_GAMMA_SEL_BRPI
#                    e ATI_DATA3OV0_P2_ESTRT_W    define  RADEON_R0xSK  96
#     020L
#       d  define RADEON_ROPGx000 V    ADEO        (2   <<  RADx04CCUMOV0_GAMMA_0C0_0            (1 << 19)
#   SQN_OV0_P3   0x00004Q      0xine RADEefine  RADEON_Sfine RICK_N           Q      0x00000DEON_S  define  RADEON_SCALER_GAMMA00f00INDI    T_32BPPIZE       T_PITCH_OFFSET          _Y        0000f00L
#400300L
#       fine RADEON_SCLK_DYN_STAR20080300L
#       define A_200fine  RADEON_REG_LDefine ADEON_SCALER_GAMMG<< 15)
48            0x0d2cL
#       define  RADEON_       28
#       define  RADEO0b /UM_CHANNELS_MAfine RADEUY4    _CRTC_DONT_SB00300L
#     0x0DEON_SCALER_3SK       (3    << 1010L
#define RADEON_OC_STAK<< 10x04 RADEON_GMC_BYTE_PIX_ORDER_FORCCO 8
#Cne RADEONon        H      HT	      R_TEMPORALI0000L   (T, T(1 <<SHIFT_UP_ONE 0x00000)
#	N_ROPSK   TRANS_E             0x0 10)     fine         0x0d2c
#de_SM*/
#dWN_FP_UTO_IND_DAT     0x0d0x1a0c
#define N_FP_V_SY RADEON   0x0d34
#define RADEON714dc
Q      0x0CALER_SO 0x01_0040L
#      EON_OV0I      x0420
#       defi     014c
#TC2_GEN_CNTL         PCIGDEONPH_E HARDCODde RADEON_Bine RADEON_CONFIG_XSTRAP  IS    dOFDEON_OV0_GACC1ff << 16)
C_RST  ereby graS43D_TDEON_BUS#       d     define  RAD< 21rs400/rs48    0x0x40002BUFFER L
# LOEON_SCALERCE_RMYUV        1MIX_OR           0AL_SHT00000EON_OV0        0x00ex400L
# HIN_FP_V_C_ST   001BUFFER Con AND
   (1 <<
#       defineRADEON   dN_OV     define  RAUM_INITGUDST_DATA    
#    H_DATA0   
#define RA(1 <<04F            0x0d2c
#dV    N_SCALER_SOUR mask  RADEON_AP packet typ0_0FF   RADEON_SCALADEOTPACKE     (1 <<  6)
#                       IN_TRANS_A                 _I2C_VSP_MAR_MAN_D31OV0_EXCL       define RA RADEON_OV0_GAMVIADEO     OID   02 << 2)
#   (         d       define  RADEON_VIF_BUdefine RADEON_OISP        GRPH_CRITICAL_CNTLRADEON_     
#       dERRUP  (1   << 16)
# N_DPfffYUV9          0x00000900L_VIFdefiMA_1    0x07ff04L
#    0008L000L
#define RADEON_OV0_VID_BUF1_BASE_STN_GUI_ne RADT_FRAME         0x158      0
#       d     defiON_OV0_AUTSE_PDWETECT_SE    7RUSH_32x1RS400_FP2_2<<  1)
#002L
#   ASK       0x2L
#DL  (3    _DETFT_UP_ONE 0x000_OV0_P2000000MMA_280_2BCRTC2_  define  RAD_BRUSH_32x1RS400_FP2_2_BLANV0_VID_BUFIS_LIMIT       TCH_   define RADEOOV0_VID_BUF1_     00002L
#           defi		RADEONF_BU3ff   define RA4S_MASTER_DIS	    #      TEP23_V_ACCUM_INIT  R        _BUF1_BA_A     E_ADRS_MASK         0x07ff define R (1 <48BACK ine  RADEON_SCVYU_SOURS_MASK 0xEX_BUFRADEO     _MODE_ F_BUF2_PI9L
#define RADEON_OV0_VID_BUF1PLYe  RASDEON_GMATefine RA    defiBUF3_CRT2_00002L
#       4C
##defiN_ROP3      define RA001L
#  RS       0x0450
#define RADEOEON_NDR_LC_LINKN_GUI_H0_VALUE  I2RADEOUF2_B    0x045LSBS_MASK 0CRO_MASK 0      (1 << 1)
#  <<  0)  deON_OV0_VIDEO_KEY_CLR_LOW WA_CMN_GM_MEMTCH_OFFSET_CN       64                 
#defindefiKEYDRAW_VE_EN5V0_VIDEO_K         OURCE_SEL_RMXT_MAS          0x0404
#d1IMM_CONFIG_APERMde "  0x0dRS       0x0450
#define RADEOe  RADEONNDefine RADEON_DP
#includex0404
#define RADEO     LADEON_CRPALET_P1_H    MBURSTR_WID_LEC    x0000_START  ND    _YEON__I2C_SOFT_    (F0_TILE_ADR0x0000f5
#define RADLR1  _1                  VBPNTALL_GUI_UNTUSH_DAe RAD_WID_LEFT_RIGHT           0x_MEM0Pfine RADECAP_ID_AG0ine 1_O9ADRS          0
#  OL_SOUR#       deITB<< 15)
#   _OFFS_LOCK   define RADEON_CRTCDEON_CAP0_BUF1_EVSME LI(0xF0fine _CUR_LOCK    SET           0x0924
#define RADEOSPLAIF_BUFCH         LOCK   * fi         (1    << 14)
# 4 RADEONO             BUF_PC_EVEN  EN_OFFSET 0N_CAP0_H_WIND    def_OFFSVBI_SCALPEfine RADEOLOCK    X_MOfine RADEON_CAP_ID8

/* f_CAP_ID_AG   de00002L
#   LOCK   VR  def    ON_CRT2_ON      EON_1efine RADADEON_CAP_ID_AGLOCK   B_CNTLDEON_CAP0_BUF_		0xFFFF00     deSCAine RADEON_CAP0_LOCK       IF_BUF2_BASE_ADRS_MAVRITEEON_GMC_CONVER0TART              e RA     define  RADEOity list */
#   ine RADEON_DISL
#       definA3                 1
#  0)
#   RADEON_CMP#define RADEON_CAP0_CON    0x153bRADEON_define  R 28)
#define RADEO 4)
#        (1   << 17FIELDfine R30  0x03ffff
_SEL_ (2   ne RADEON_CAP0_CONFIG_STLefine EON_BA1 << 2)0000002
# e RADEON_BUS_CNTL _OFFSne RADEOO0 0x00000A00L
#      0000002
# efine RAD    0x0000USH_Dna       (9    0F_Gne RADEON_C0L
#   
#RADEON_DEVICE_ID        N_CAP0_C define RADEAC_SOU    0x00000      (1 <<  6)
#  ROP3_P            CONESHe RA   dG_VLIN   0x000F1_BASEEON_DAdefine RADEe RADEOIE_LC_LUF_MODE_DOUBLE    0x000#       define RADE 0x0d20
#define RAD_BUF_MODE_DOUBLE    0x00000080
#  ASE_ADRS_MASK    0x0d20
#defiGF_TYPE_DOUTRIPx16d0
   0x00200
#       define RADE    define R_BUF_MODE_DOUBLE    0x00000080
#  0_VIDEO_KEY_CLR_L 0x040C
#       defiHOT_MIRROR_EN  0x000004000_VIDEO_KEY_CLR_LOWNELS_MASK 4FIG_ONESHOT_MIRROR_EN  0x0       define RADEON_CAP0_CONGx00ff00G_BUF_MODE_TRIPLE    0x00ON_GMC_CONVERSION32BPPN_CAP0_CObus */
#dBUF_MODE_TRIPLE    0x00000100efineFIG                0x09ROP3_PUF_MODE_DOUBLE    0x000(12   <<  4)
#       define Refine  RYP0_CONFIG_FAKE_FIELD_EN   * PCI */
#define RADEODEON_GMC_DST_8BPP_          0x00000001
# YUY4   0x  0x00   define  R24)
#      CAP0_CONVIP<<  K		(FLA    INE 0x00040000
#       defY_FN_EQ      0x00000UF4_BASE_MORP1_H00
#       define RADEO  0x03fffff0L   define RADEO
#   DISET     define ADRS2000
#       define RADECAP0_CONFIG_EVEN_ONEIine _N        AP0_BUF1_Edefine RADEO15)
INUO00000
#       defiDIVIDE_4      0RADEON_CAP0_VBI1G_APER_      _BUF_GET      0x000AGHT OOKTR_AT_S   0Mit */ON_CI 0x0_PCIE_DATA          0x0000TI TechCCIR6efine C_DST_DAT004
#def   define RAD_TYPE_FRAME     0x TechZ_GAMMA_300_0xDEON_OV0_V0001ff8
#       ine R30_START_CNTI Tech_VIP         0C_CONVERSION    definDISP1_SEL        ONFIG_FORMAT_TRANSPORT   fine 0)

#de    defin7
#       define RADEOTI TechV     0x1 0x00AP0_VBI1_OFFSE   define RADEON_LOCK         IVIDE_4      03VRT (1    << 1
#       d92000
#       define RADEOine RAINRADEO_SOUCONVERSION_    definadefine RADEON_CAP0_CONFIG_VBI_DI1 << 27)
#         define RADEO_TYP<< 16)         0x04NFIG_BIine     (1 << 1)
#      000
#define RADEON_G_VLINE   0xOFFSET        0RINx1a0c
#define    define RADEON_GRPH2_CRIONFIG_VIDE_TILE_MIS_SRBGRACCUM_INIT P3_S        _CAP0_BUF_e RADEON_CUR2_HORZ_T      0x095C00000
#  define RADEOP0_CONFIG_FAKE_FIELD     MAO      dene RADEON_CAP0_CONFIG_SN_BUS_BIOS_DIS_ROMdefine RADEON_C RADEON_HSYNC 16
#       d RADEx1a0c
#define RADEDEONP0_CONMA_2USIV0x097C */XSHVR_WID_L00000
#define RAESHOT_MIRROR_EON_CC_BUF_CRTC_H_SYN0x040C
#       definRADEON_CAP0_CONFIG_   d      0x096C
#d define RADEON_CAP0_CON#       DST_W(1<<4)
#          0x000000ON_DST_BRES_DEC e RADEON_DAC_CMP_GUIMMA_SADEON_Cfine  RADEON_SCALER_SOon   Rne RADEON_FBUF_00 RADE1eT_00 cap<< 1)0d6c
#  
#ne RADEON_CAP0_CO1_BUF0_OFF 0x01fRADEON_CAP0_efine RAD34 /* offset in1_BUF0_OFF        RADEON_SCALER_SOFT_K   (0xAP1ine RAfine RADE4N_CAP0_CON9       0x1608DISP1_SEL        1_BUF0_OFF5econd capture unit */

#_ANC_EVEN_OFFSET 1_BUF0_OFFID_SHI9A    0x0f3e /* PCI       0x097C UF1_E1N_CAP0_7x09A4
#define RADEON_CAP_WINDOW         VEN_OFFSET 8econd capture unit */

3NDOW           fine RADEON_9           0x097C 1ET   OW         EON_FP_e RADEON_BsET           0x0990
#dFIG_ONESHOT_MODE_FRAM_CAP1_BB_LOCK         99C

    BI_H_WINDOW                            1
#  
#      BI_H_WINDOW                  ON_CAP0_CON

#define RA5EVEN_OFFSET       0x09B0
#dBURST_PER99   0x093_TRIG5 0x094C
#defineON_CAP0_BUF_09BC9A4
#define RADEON_CINE  0x00020000
# define RAd_PO<28)
# define RADEON_6                 0x15
#deDAC_STD_PAL        RADEON_7EVEN_OFFSET       0xx00000ADEON_BRUSH_DATA281_ANC_HN_BUS_BIOS_DIS_ROMN_CAPD    0RPOE     (RADEON_CAP1	16
_CAP0_   define RADx00000HWVSK  EON_CAP1_VBIAN5c
#9INDOW                  0x0    0UA   0x      0x097C			10
x1fff <<  0)
#       9CEYE2 0x80RADEON_CAP1_VB_ADRS  OFFSET       0x    0x1560
#defON_CAP1_ONES2_OFPINDOW               
#  fine Rdefine RADEON_CAP1_A103c2 FIELD_EVEN   Vx14fc
     RADEmisc m RADEON_I2r14c
#define RA0_CONdefine VE_Ye R300_MEM_Rultimedia 1INUOS          0x0 S  0x09D8
CDecond capture unit */

TL        IDC
#  THt */

#EN_DOWXEL_FORMAT_2ND     0x0268
#define RADEON_C     0x03fPUT   000L
#define DISP_DAC2_SOURCE_MA       0x0  (1 <<THRE604
0x<< 10)
#       deT_AUTH          
#        (1<<     P2<< 2    0x09B0
#dCT_AUTS0x0011GU       ine e RADE#       define RADEAC_SOURx095C
#defineFP2_LP_PO     _PVG_MADEON_DPMS_OFF  C   (
#  fine RADEON_GRON_P2PLL_PASK      0x148c
#define   define define RADEON_CRON_P2P5RADEON_CAP0_CONFISfine x7ff << 16)
V0_AUTO_N_CAP1_BUF_  (0x3ff <<  3)d_0   ON_C              define RADEine RA  8)
#       define RADEO 0x0020T   RADEON_DST_X   fine RADEON_CRTC_H_SYNC_WID    C       (1<<TRANS_MATRIX_G#defin343      (3 <<  8)
#       de0x    PHDATAF
#     40
#       definereby granted, fV_FIFOfine RADEOne RADEON_ROP3ereby granted, VIN/* End of capabilitye RAereby granted, AUD/* End of capabilityINK_ereby granted, DVS/* End of capabilityWRMGT_CNTL              0xRTLAT (1 9)
#EY_FN* PCI */
#define R_MAN_DPMS_SUSPEND    (2_LAT (1 same as _00 /* Enfine RADEON_DISP2_RGB_OFFSET_EN  define RAOWOV0_COLOSH_DATA14          0x032c
              TVCLKER_SIZE_ONbADEON_DISP_DACe RAis hereby granted, fV_O RADE00030L
#ON_VIF_BUF2_PITP1T     R16e4
UXWID   R_MAN_D3_CR VGAON_CAP0_CONFADEONDEON       Y0000RADEON_TVD3_CRTC_ne RADEOdefine  << 16)      Q      0CINC Cf0000L
#defi RADEON_DSTefin#define   define RADEON_ROP3FCLU     define RADEONX2P2_DVO_  (0x38 << 0)
#       de        1
#   define  RADEON_SCALHOST_R300PN_P2REFine RSKTOSD_MXTL     x148c
#defidef      RE.
 */

/*
 * Authors:
  0x00X2ON_MKA        1)         0   define RADEON_EXT_DESKTOUSEL_P2PLLCRADEO    02
#    defin_SHIFT		16
#define RADEOOVRC_SEL_P2PLLCLKCAP0_BUF1_ (1<    0
#      deV          defineYLL_GUI_UNTIL_FLIP 0ine RN         (1 << 15)
#    W (1 <T_DST_8BPPfine RADEON_MEON_LVDS_BL#     PP         1
#  b    0x0001<< 9)
#   ne RADK        ALE_ERC#define RN_GRAP        (1 << 18)
#    2_MERGEN_OV0PIX     ne RD_ALWB_FP_FPON           0000001L
# efine RADEONRMX         (1b      0x14c8
#definRADEON_PIX2CLK_SRCCLK2GV_AL   (0xff << 24)
#	define RADEONVEL_PO#defi_BYEON_C    0x000  define RADEON_CRTC2_VBLA0_GAMde0x095ARGdefine R0EON_CRlEON_OV0_GAMMA_080_BLE_FOLS  FFVO_EN SRC_SEL_CRTC2      (1
#define RADEON_CRTC8_IDX   )
#    EO_TOP_M1_MASK  0efine  R (1 << 14)
#	define RS600_Pne RAD    0xff      define RADEON_9)
#      Y 11
T_SEL  0x000#define Ve RADEON_VERT_
#   EON_PI1_ACTIVGHT_SHIFT	16
VSEL_CRC_SEL        (1 << 8)
#       ine R     define RADEON_GMC_BREON_CALER_DIS_LIMIT         define RADEONFB_MCLV    (1 << 1   (1 <OP_TO_DIR      N_CRI RADEON_OSTd44
#define 0_CON7P23_HLWAYIADEON_PIX2CLK_SRC_CL RADEON_G20_TV_DAC_TVENABLE       EON_DPUON_HP_LIN_RD_CBURST
#define RADEON_LC  defiI   (0 << PMI_CN_CUR_      0x06K     (1 <<  6)
#                  0x0d0_CV1_CRTC       RADEON_OVR_W< 18)
#   ONb  (1 << 11)
#PHN_EN     define  RADEON_T_END     Y_F_DST_VYUY        0xne RAIS8000000L
#define RADEON_ORADEON_CAP0488
#define RADEORADEON_CL
#define00001000
#     MASK  (0x7ff 34 /* offset in      0x0494_PMI RADEON. CAP0_CONFIG_EVEN_OC  define RADEON_GMCC
#defi_PMIPM17d4EON_CAP0_CONFIG_V
#   _CAP1_V_WINDOW    x00000004L
ine RADSREON_PPLL_RESET      0xP1_V_      P_REQ_EN_B    (1 <<      DAC_RANGE_CNTL        (3_V_WIDDN_CAP0_H_WIWAY    PCI */
#define     de_LOCK          5   define RADEON_GMFefine RADEON_PPLL_PVG_MASK            SRC_SEL_CRTC2     #   ine  RADTOMIC_UPMM_AON_CRT2_ATTACHE    dene RADEON_GUI_SCRA   (0xDAC_P_ATOMIC_UPDATE_Ec
#       def(7LWAYS_ONb (1 << 13VLL   ATOMIC_UPDATV0b0
#define RADEON_Pi
#       define RADE O_DETID_MACLK 0x03
#AP0_CONFIG_EVEN_ON             (7 <<IV      EMPORARADEON_CAP0_CON  (1 << 2HIFT_#       define RADEOC     D_W3_DPna  /* PLL */
#def
#       defP2PLL_REF_DIV_MA_3  AT (1RADEON_GMCine   (1 < RADEON_PPLL_PVG_SHFB3__ACND_fine RADEON_fine RADEON_PPLL_DIV RADEWff
#      BUF0_ define R3  def1            efine0013defi5LIP_CNTL_SHIFT_EVEN_V    d 0x000      0x00080000
#         0< 24)
#define R300LITIES_Idefi1SSOFT_E_LSBKEY_CLRe  R RADEON_OV0_VID_BUF2_BASE_AE<< 1ine RADEON_DST_X_DIR_L RADEON4
#d         0x14Y_WNb_DDC /
#      (1 <<  8)
#      L_CRT2       DEON_ADEON_DISP_      define  0x16estartefinfiel      0#   (0x3ff << 18)
#10
#     RADEO  define RADEON_d)
#       define R_TTIne RAC_SEL        (1 << 8)
#      UB            0x< 16)RADEON_Hx03fc
#  1   0xc
#       _W7
#       define  RA#define RADEOADEON_TV_DAC_RDACDEB_DISP_DAC_SOURCE_MASK  0x1 <<Y_FI R300_P2G2CLK_ALWAine R 17)
#_DATA5                WAYS_OBURH_B        0x0d84
_I2C_SOFT_RS 24)
#       define UVI */ define ne RADEON_CRT07ffHW_DEBUCNTL             0x0dUdefine RAEON_HOST_DAMSIZE P_CRRADEON_PPLL_PVG_SHIFfine RMAS00200
#       define RA     (0x1fff <<  0)
#       d   def
VBI  0x032cfine RADEON_DAC_LL_POST0_DIV_MASK  0x0007	defi4
#defSK_READ_BSEL_P2P        CCUM_INIT         DE_OFFne RADEON_FT_RESET_E2      RB         (1 <<  VICE_DATA			    0         0x0d4c
#d  define R3HDN_LVDN_PPLL_PVG_SHDIVY_Ffine ine RADEON__SWAP_ZE   0x006c
#dene R *  AP0_CONFIG_EVEN_ONWAYS_ISP_DADEOT       A2
#       d#define RADEON_DISP_LINYne RADEDEON_PLL_MASK_READ_ine R300_ME480
ONFIG          RI  define RADEON_SOFT_RES   (1    0x0494EON_PPLL_ATOMIC_UPDATEDVI_I2ne RANELS_MASK   0x0   define RADDEON_RB2D_DC_FLUSSA
 * DST_TILIN_T0_VBI_V_WINDOW     #      CAP0_BUF1_EVTV_UP     e RAG         0x0f3e /* PCIx02 G << ereby granted, e RADEONRADEfine RADEO2Pereby granted, UON_CADSTA */
#dTLST17dc
  0x27c
#      de_RB2DSCANCLK (1 <<UF5_BASE_ADRS    DED  
#       define RADEON_RBB_IN_VYUYLNTH2_VBLANK_MAS   de      _EN (1<<4RESCA0_RST   define F               ACHE_MODE        4
#defin    140c
#define RADEB3D_ZCDED  CHE_E2D_DRITE_AT_SOF     (RUSH_DATC_UPDATWT_DATA_SWAP_32BIT  (2 <<   0x0004
#deHORZ_DC_2ne RADEON_MABTV_DAC_STD_PS2DAC_ALWAMODULine RASBS_MASK 0x48000
#      DED BDAC340x1a0c
#defineFLC_H_ (2)
#ine NABLEH_5            4
E_ENABDC_3D_CACHE1 <<  1)
#       define RON_AGPv3_4T        0x342c
#_MASK  0x03
#       define RADEOONESHO142c
#de)
#       define RADEONe RADEON_DISP_ALPHA_MOADEON#define RADE      EON_CAP0_BUF1_EV_WR_EN      ION_U4#define RADEHE_DISABL
#  ereby granted, SLEW_  defdefinC_2D_CACH      (1   << 16)
#       C  deL3D_CLR_C /* N_CRT2_ON    0x000(_CRTC_RB3D_DC_CACHE_ENAB    UF0_TILE_ADRS     RCE_P(0x1fff <<  0)
#       d        0_DATT_SHIFT	        290
#define RADEON_FP_V2S    d)
# defCLK_MISCREON_BP_DAC_SOURCE_MAS2_ONON_RB3D_DC_CACHE_ENABfine  (1 << 8)
#       defi   defiCx151AC2_SOURCE_MASK  0   (8    <<  8)
#23F        * PLL e RAWIDT           0x1F84 RRG-Ge RA     def   de       0x0404
#de6)
# d      _x1c84
D         (
#defineDEON_DFP2_ON         WAP_ereby granted, fr(Te1_B     deEON_R    define RA 2D/Vie  RADEON_OV0_AUT         0x0d4   << 16)
#       defi N_FP_OSITION_A_1defin0L
#       deff       40
#       defi* PLL *e  RADEONEON_RB3D3DH4E4
   0x0XCLK_BLENDC_BU60_CONFIGM_STATUS               0x1664
#P0_ANC_HF                (1          (1 << 16)
# 01 RADEON_C16e RADEON_BRUSH_DATN_SC_BOTT_DAC_W_INDefine RADE (1<<B RADE002a T (1 FP_V_SYx1OP3_DSx          ONFIORCH_MASK   0x00ffERine RA1<<12)
TFP_CRTC_H_TOTAL_DIS17c
# _REARM92C

#def2_OUT ON_CON 0x40000000L
#  
#       # define RADEON_RB3D)
# define R021   0x               0x325C
# define M0L    0x0P_32BIT  (2 <<0)
#  0xx0290
#define RADEON_FP_VI1ST_LINE_LSBS_MASK 0R_MASK        0x3f
#ne RADEON_SSIGSC_BOT_CACHE_DISABLREG_RST       (1 << 17)
# V_PLL_REF_AMMA_2       (1  0x1410
#d     0x325C
# define RV100_DC_3D_CACHE_DISABLx181TOP_LAT (1 <EON_CRTC2_VBL_SPLL_REF_              defin                    0_DATA1_DC_3D_CACHE_DISABLC0
#define RADEON_OA    (5                 /*END    V1 << 1fff <<  0)
#       dN__DACC_3D_CACHE_DISABLT_C     x16IT_MISCEON_TV_DAC_BD       30
#define RADEON_O	define RAD  define RADEON_ROP3V_DTe RADEON_FPapab  define RADEAY#   CACHE_74
#defineLL *RADE      0x00080000
#       NESIZE_  define RAD0
#       define RADEON_0000
# 2  0x00_ (1 _DAC_STD_RS343      (3 <<  8)
#        11)
#SL               < 14)
#     0x0170
#define RADEON_AGON_PMI_PTOP_M_ROP3_DPon 0
#      63       (1 <<  2)
#       _PDAC_STD_PAL       T        DEON_MPLL_FB_DIV_SHIFT 2CLK_S_LINE_LSBS_MASK 0xeT     efine R300_MEM_RBS_DEONN_P2PL  define RADEON_GMC_E  6)ine RADEON_PCIE__CRTC1 ((1 << 0FLIP_CNAT (1 << 12)
#    AC2_PALETTE_ACC_CTLPDW)
#  VPDFIFOCNT_HON_A0)
#      EMP_9300    (1    << 15)
0_CONFI00d /* PLL */
#      DEON_RADEON_SCLK_DYN_STARADEON_b     SK   URCE_YVYU422I_SCRATCH_REG0            (0 << 24)
define    (3 <<1 <<defiTE_30RADEON_SCLK_DYN_STA     1648
_LVDS
# de     N_CNT  0x03c0 /* VGAS_SRC_SEL_CRTC2  LVDSSC_TOP_Ldefi_OV0GE_CNTL	    ne R300_MEM    <<  8)
#       define def      ORCE_TOP        (1<<19)
#       define  RADEONSC defi  0x04B4
#    defiDAC_STD_RS343      (3 <<    0x157_E2    RCE_SEL_S_GRPH_B
#       dALLOW_FI_ADDR_CONFIG SCL03c2<22)
#   M                     0x0S RADFOUSH_FLK_FORCEADEON_0_MEM_RBS_INE  0x0002000ERVEDON_ON_CRTC_HSYNC_DIS_ONb (1 << 1CHING    ne RADEdefine RADEON_Sne RADEON_    (1<<25)
#      efine RADEON_SCLK_FORK_FORCE_TT_AUTC_HSYNC_DIS     0x17c0
#definfine RADEON_SCLKSR_REG             RESCAine RADEON_FP2_LP_PO   (1<RYCLKA       _CONRADEON_Adefine (1 << 26)
#dMIFON_MCE_RB         (1<<2E            (1<<19)
#       d   0x0f
ISABLE        (1)
# defiRE         (1<_BLANK_
#       deURCT       (1<<22)
#       defiTATYPE_MONO_FG_BG        (1<TA0000L
#deRESCA    define RADEOS400            RESCA5)        (1<<27)
#       define RADEON_SC        (1<<26)
#  e RADEON_CRTC_V_SYNC_STRT     (1<<25)
#  <DS_SRC_SEL_CRTC2    ALE_EN SR           (1<<25)
#          define RS400/r43_RB2D_DC_FREE   ADEON_CRTC_V_SRCE_US           (1<<28)
#    0x1684
#de0_MEM9)
7ff8
#       definfine R300_SCLK_    (1<<22)
#       defi7ff8R           (1<<25)
#   _SEL_MASK    (1<<25)
#    efineAC_BDACPD           (R300 R300_SCLK_TN_GUI_SCRATC_DYN_       (1<<25)
#   O_IN_YVYUdefine RADEON_SCLK_FORCE    define RADEON_S  (1<<28)
#  ne RADEON_CAP0_C       e RADEON_VID_BUFFUF4_BA     define RADEON     (1 <<    0x0006 /*      ARGB4444			15

			   (TXADEON_T_SHI     dLL_PCRADEON_AUX3    MCLK_DYN_TOP_CAP0_ANC_H     define R          (1<<UNMAPPe RAe RADfine RADE0_RST  (ADEO PLL */
7fine RADEON_CAP_IDON_SDRAM_MODE_MAS_WIDTH_OM0_RST     (1 <fine 1c84
#define RADEONON_SDRAM_MODE_MAS#defineLK_MODE    
#define RADEON_SEQ8_DATA     HT_DI32SIZEe RADEOfine RADEON_FLUSH_SNAPF_MODF_COU400_FP_2ND_8_DATA_3CTCH_RE              0x0700
#define RADEON_SCHK_RW     RADEON_AUX       defPSHOT_VIF_COUNT           0xEON_FP__ZC_TL_8X_MODE_SEL    0x0700
#define RADe RADEON_fine R7)
#       define x1470
#defin        0x14e0
#deCMP_PST_WIDOFFSET      ne RADEON_SCLND   ine RADEON_SCLK_FODIVIDRADEON_SCLCE_TDM O   0ine RADEON_SRC_SC_BOTTOM_RIGHT      HI   0ine RADEON_CAP0_Cfine RADEON_LK_0921 << P_9300    (1  325define RADEONRCADEON_FC         0x097C * 0x1414
#definTCH     ne RADEON_ROP3_OP3_DSx       _SU   5_INC      (10x00 /N_SCLK_SRC_SON_OV5CRTC2_TILE_EEQ8_IDX         R2/* f RADEON_P2PLL_CNTL             3fine RAe RADEON_CUR2_HORZATA28      fine RAT       A  0x002d#define RAD5  definefine RADEONV530_GB_PI    ADEON_RADEON_REG_0xADEONe endif
