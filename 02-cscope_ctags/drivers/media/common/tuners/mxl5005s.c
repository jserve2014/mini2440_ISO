/*
    MaxLinear MXL5005S VSB/QAM/DVBT tuner driver

    Copyright (C) 2008 MaxLinear
    Copyright (C) 2006 Steven Toth <stoth@linuxtv.org>
      Functions:
	mxl5005s_reset()
	mxl5005s_writereg()
	mxl5005s_writeregs()
	mxl5005s_init()
	mxl5005s_reconfigure()
	mxl5005s_AssignTunerMode()
	mxl5005s_set_params()
	mxl5005s_get_frequency()
	mxl5005s_get_bandwidth()
	mxl5005s_release()
	mxl5005s_attach()

    Copyright (C) 2008 Realtek
    Copyright (C) 2008 Jan Hoogenraad
      Functions:
	mxl5005s_SetRfFreqHz()

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/*
    History of this driver (Steven Toth):
      I was given a public release of a linux driver that included
      support for the MaxLinear MXL5005S silicon tuner. Analysis of
      the tuner driver showed clearly three things.

      1. The tuner driver didn't support the LinuxTV tuner API
	 so the code Realtek added had to be removed.

      2. A significant amount of the driver is reference driver code
	 from MaxLinear, I felt it was important to identify and
	 preserve this.

      3. New code has to be added to interface correctly with the
	 LinuxTV API, as a regular kernel module.

      Other than the reference driver enum's, I've clearly marked
      sections of the code and retained the copyright of the
      respective owners.
*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "dvb_frontend.h"
#include "mxl5005s.h"

static int debug;

#define dprintk(level, arg...) do {    \
	if (level <= debug)            \
		printk(arg);    \
	} while (0)

#define TUNER_REGS_NUM          104
#define INITCTRL_NUM            40

#ifdef _MXL_PRODUCTION
#define CHCTRL_NUM              39
#else
#define CHCTRL_NUM              36
#endif

#define MXLCTRL_NUM             189
#define MASTER_CONTROL_ADDR     9

/* Enumeration of Master Control Register State */
enum master_control_state {
	MC_LOAD_START = 1,
	MC_POWER_DOWN,
	MC_SYNTH_RESET,
	MC_SEQ_OFF
};

/* Enumeration of MXL5005 Tuner Modulation Type */
enum {
	MXL_DEFAULT_MODULATION = 0,
	MXL_DVBT,
	MXL_ATSC,
	MXL_QAM,
	MXL_ANALOG_CABLE,
	MXL_ANALOG_OTA
};

/* MXL5005 Tuner Register Struct */
struct TunerReg {
	u16 Reg_Num;	/* Tuner Register Address */
	u16 Reg_Val;	/* Current sw programmed value waiting to be writen */
};

enum {
	/* Initialization Control Names */
	DN_IQTN_AMP_CUT = 1,       /* 1 */
	BB_MODE,                   /* 2 */
	BB_BUF,                    /* 3 */
	BB_BUF_OA,                 /* 4 */
	BB_ALPF_BANDSELECT,        /* 5 */
	BB_IQSWAP,                 /* 6 */
	BB_DLPF_BANDSEL,           /* 7 */
	RFSYN_CHP_GAIN,            /* 8 */
	RFSYN_EN_CHP_HIGAIN,       /* 9 */
	AGC_IF,                    /* 10 */
	AGC_RF,                    /* 11 */
	IF_DIVVAL,                 /* 12 */
	IF_VCO_BIAS,               /* 13 */
	CHCAL_INT_MOD_IF,          /* 14 */
	CHCAL_FRAC_MOD_IF,         /* 15 */
	DRV_RES_SEL,               /* 16 */
	I_DRIVER,                  /* 17 */
	EN_AAF,                    /* 18 */
	EN_3P,                     /* 19 */
	EN_AUX_3P,                 /* 20 */
	SEL_AAF_BAND,              /* 21 */
	SEQ_ENCLK16_CLK_OUT,       /* 22 */
	SEQ_SEL4_16B,              /* 23 */
	XTAL_CAPSELECT,            /* 24 */
	IF_SEL_DBL,                /* 25 */
	RFSYN_R_DIV,               /* 26 */
	SEQ_EXTSYNTHCALIF,         /* 27 */
	SEQ_EXTDCCAL,              /* 28 */
	AGC_EN_RSSI,               /* 29 */
	RFA_ENCLKRFAGC,            /* 30 */
	RFA_RSSI_REFH,             /* 31 */
	RFA_RSSI_REF,              /* 32 */
	RFA_RSSI_REFL,             /* 33 */
	RFA_FLR,                   /* 34 */
	RFA_CEIL,                  /* 35 */
	SEQ_EXTIQFSMPULSE,         /* 36 */
	OVERRIDE_1,                /* 37 */
	BB_INITSTATE_DLPF_TUNE,    /* 38 */
	TG_R_DIV,                  /* 39 */
	EN_CHP_LIN_B,              /* 40 */

	/* Channel Change Control Names */
	DN_POLY = 51,              /* 51 */
	DN_RFGAIN,                 /* 52 */
	DN_CAP_RFLPF,              /* 53 */
	DN_EN_VHFUHFBAR,           /* 54 */
	DN_GAIN_ADJUST,            /* 55 */
	DN_IQTNBUF_AMP,            /* 56 */
	DN_IQTNGNBFBIAS_BST,       /* 57 */
	RFSYN_EN_OUTMUX,           /* 58 */
	RFSYN_SEL_VCO_OUT,         /* 59 */
	RFSYN_SEL_VCO_HI,          /* 60 */
	RFSYN_SEL_DIVM,            /* 61 */
	RFSYN_RF_DIV_BIAS,         /* 62 */
	DN_SEL_FREQ,               /* 63 */
	RFSYN_VCO_BIAS,            /* 64 */
	CHCAL_INT_MOD_RF,          /* 65 */
	CHCAL_FRAC_MOD_RF,         /* 66 */
	RFSYN_LPF_R,               /* 67 */
	CHCAL_EN_INT_RF,           /* 68 */
	TG_LO_DIVVAL,              /* 69 */
	TG_LO_SELVAL,              /* 70 */
	TG_DIV_VAL,                /* 71 */
	TG_VCO_BIAS,               /* 72 */
	SEQ_EXTPOWERUP,            /* 73 */
	OVERRIDE_2,                /* 74 */
	OVERRIDE_3,                /* 75 */
	OVERRIDE_4,                /* 76 */
	SEQ_FSM_PULSE,             /* 77 */
	GPIO_4B,                   /* 78 */
	GPIO_3B,                   /* 79 */
	GPIO_4,                    /* 80 */
	GPIO_3,                    /* 81 */
	GPIO_1B,                   /* 82 */
	DAC_A_ENABLE,              /* 83 */
	DAC_B_ENABLE,              /* 84 */
	DAC_DIN_A,                 /* 85 */
	DAC_DIN_B,                 /* 86 */
#ifdef _MXL_PRODUCTION
	RFSYN_EN_DIV,              /* 87 */
	RFSYN_DIVM,                /* 88 */
	DN_BYPASS_AGC_I2C          /* 89 */
#endif
};

/*
 * The following context is source code provided by MaxLinear.
 * MaxLinear source code - Common_MXL.h (?)
 */

/* Constants */
#define MXL5005S_REG_WRITING_TABLE_LEN_MAX	104
#define MXL5005S_LATCH_BYTE			0xfe

/* Register address, MSB, and LSB */
#define MXL5005S_BB_IQSWAP_ADDR			59
#define MXL5005S_BB_IQSWAP_MSB			0
#define MXL5005S_BB_IQSWAP_LSB			0

#define MXL5005S_BB_DLPF_BANDSEL_ADDR		53
#define MXL5005S_BB_DLPF_BANDSEL_MSB		4
#define MXL5005S_BB_DLPF_BANDSEL_LSB		3

/* Standard modes */
enum {
	MXL5005S_STANDARD_DVBT,
	MXL5005S_STANDARD_ATSC,
};
#define MXL5005S_STANDARD_MODE_NUM		2

/* Bandwidth modes */
enum {
	MXL5005S_BANDWIDTH_6MHZ = 6000000,
	MXL5005S_BANDWIDTH_7MHZ = 7000000,
	MXL5005S_BANDWIDTH_8MHZ = 8000000,
};
#define MXL5005S_BANDWIDTH_MODE_NUM		3

/* MXL5005 Tuner Control Struct */
struct TunerControl {
	u16 Ctrl_Num;	/* Control Number */
	u16 size;	/* Number of bits to represent Value */
	u16 addr[25];	/* Array of Tuner Register Address for each bit pos */
	u16 bit[25];	/* Array of bit pos in Reg Addr for each bit pos */
	u16 val[25];	/* Binary representation of Value */
};

/* MXL5005 Tuner Struct */
struct mxl5005s_state {
	u8	Mode;		/* 0: Analog Mode ; 1: Digital Mode */
	u8	IF_Mode;	/* for Analog Mode, 0: zero IF; 1: low IF */
	u32	Chan_Bandwidth;	/* filter  channel bandwidth (6, 7, 8) */
	u32	IF_OUT;		/* Desired IF Out Frequency */
	u16	IF_OUT_LOAD;	/* IF Out Load Resistor (200/300 Ohms) */
	u32	RF_IN;		/* RF Input Frequency */
	u32	Fxtal;		/* XTAL Frequency */
	u8	AGC_Mode;	/* AGC Mode 0: Dual AGC; 1: Single AGC */
	u16	TOP;		/* Value: take over point */
	u8	CLOCK_OUT;	/* 0: turn off clk out; 1: turn on clock out */
	u8	DIV_OUT;	/* 4MHz or 16MHz */
	u8	CAPSELECT;	/* 0: disable On-Chip pulling cap; 1: enable */
	u8	EN_RSSI;	/* 0: disable RSSI; 1: enable RSSI */

	/* Modulation Type; */
	/* 0 - Default;	1 - DVB-T; 2 - ATSC; 3 - QAM; 4 - Analog Cable */
	u8	Mod_Type;

	/* Tracking Filter Type */
	/* 0 - Default; 1 - Off; 2 - Type C; 3 - Type C-H */
	u8	TF_Type;

	/* Calculated Settings */
	u32	RF_LO;		/* Synth RF LO Frequency */
	u32	IF_LO;		/* Synth IF LO Frequency */
	u32	TG_LO;		/* Synth TG_LO Frequency */

	/* Pointers to ControlName Arrays */
	u16	Init_Ctrl_Num;		/* Number of INIT Control Names */
	struct TunerControl
		Init_Ctrl[INITCTRL_NUM]; /* INIT Control Names Array Pointer */

	u16	CH_Ctrl_Num;		/* Number of CH Control Names */
	struct TunerControl
		CH_Ctrl[CHCTRL_NUM];	/* CH Control Name Array Pointer */

	u16	MXL_Ctrl_Num;		/* Number of MXL Control Names */
	struct TunerControl
		MXL_Ctrl[MXLCTRL_NUM];	/* MXL Control Name Array Pointer */

	/* Pointer to Tuner Register Array */
	u16	TunerRegs_Num;		/* Number of Tuner Registers */
	struct TunerReg
		TunerRegs[TUNER_REGS_NUM]; /* Tuner Register Array Pointer */

	/* Linux driver framework specific */
	struct mxl5005s_config *config;
	struct dvb_frontend *frontend;
	struct i2c_adapter *i2c;

	/* Cache values */
	u32 current_mode;

};

static u16 MXL_GetMasterControl(u8 *MasterReg, int state);
static u16 MXL_ControlWrite(struct dvb_frontend *fe, u16 ControlNum, u32 value);
static u16 MXL_ControlRead(struct dvb_frontend *fe, u16 controlNum, u32 *value);
static void MXL_RegWriteBit(struct dvb_frontend *fe, u8 address, u8 bit,
	u8 bitVal);
static u16 MXL_GetCHRegister(struct dvb_frontend *fe, u8 *RegNum,
	u8 *RegVal, int *count);
static u32 MXL_Ceiling(u32 value, u32 resolution);
static u16 MXL_RegRead(struct dvb_frontend *fe, u8 RegNum, u8 *RegVal);
static u16 MXL_ControlWrite_Group(struct dvb_frontend *fe, u16 controlNum,
	u32 value, u16 controlGroup);
static u16 MXL_SetGPIO(struct dvb_frontend *fe, u8 GPIO_Num, u8 GPIO_Val);
static u16 MXL_GetInitRegister(struct dvb_frontend *fe, u8 *RegNum,
	u8 *RegVal, int *count);
static u32 MXL_GetXtalInt(u32 Xtal_Freq);
static u16 MXL_TuneRF(struct dvb_frontend *fe, u32 RF_Freq);
static void MXL_SynthIFLO_Calc(struct dvb_frontend *fe);
static void MXL_SynthRFTGLO_Calc(struct dvb_frontend *fe);
static u16 MXL_GetCHRegister_ZeroIF(struct dvb_frontend *fe, u8 *RegNum,
	u8 *RegVal, int *count);
static int mxl5005s_writeregs(struct dvb_frontend *fe, u8 *addrtable,
	u8 *datatable, u8 len);
static u16 MXL_IFSynthInit(struct dvb_frontend *fe);
static int mxl5005s_AssignTunerMode(struct dvb_frontend *fe, u32 mod_type,
	u32 bandwidth);
static int mxl5005s_reconfigure(struct dvb_frontend *fe, u32 mod_type,
	u32 bandwidth);

/* ----------------------------------------------------------------
 * Begin: Custom code salvaged from the Realtek driver.
 * Copyright (C) 2008 Realtek
 * Copyright (C) 2008 Jan Hoogenraad
 * This code is placed under the terms of the GNU General Public License
 *
 * Released by Realtek under GPLv2.
 * Thanks to Realtek for a lot of support we received !
 *
 *  Revision: 080314 - original version
 */

static int mxl5005s_SetRfFreqHz(struct dvb_frontend *fe, unsigned long RfFreqHz)
{
	struct mxl5005s_state *state = fe->tuner_priv;
	unsigned char AddrTable[MXL5005S_REG_WRITING_TABLE_LEN_MAX];
	unsigned char ByteTable[MXL5005S_REG_WRITING_TABLE_LEN_MAX];
	int TableLen;

	u32 IfDivval = 0;
	unsigned char MasterControlByte;

	dprintk(1, "%s() freq=%ld\n", __func__, RfFreqHz);

	/* Set MxL5005S tuner RF frequency according to example code. */

	/* Tuner RF frequency setting stage 0 */
	MXL_GetMasterControl(ByteTable, MC_SYNTH_RESET);
	AddrTable[0] = MASTER_CONTROL_ADDR;
	ByteTable[0] |= state->config->AgcMasterByte;

	mxl5005s_writeregs(fe, AddrTable, ByteTable, 1);

	/* Tuner RF frequency setting stage 1 */
	MXL_TuneRF(fe, RfFreqHz);

	MXL_ControlRead(fe, IF_DIVVAL, &IfDivval);

	MXL_ControlWrite(fe, SEQ_FSM_PULSE, 0);
	MXL_ControlWrite(fe, SEQ_EXTPOWERUP, 1);
	MXL_ControlWrite(fe, IF_DIVVAL, 8);
	MXL_GetCHRegister(fe, AddrTable, ByteTable, &TableLen);

	MXL_GetMasterControl(&MasterControlByte, MC_LOAD_START);
	AddrTable[TableLen] = MASTER_CONTROL_ADDR ;
	ByteTable[TableLen] = MasterControlByte |
		state->config->AgcMasterByte;
	TableLen += 1;

	mxl5005s_writeregs(fe, AddrTable, ByteTable, TableLen);

	/* Wait 30 ms. */
	msleep(150);

	/* Tuner RF frequency setting stage 2 */
	MXL_ControlWrite(fe, SEQ_FSM_PULSE, 1);
	MXL_ControlWrite(fe, IF_DIVVAL, IfDivval);
	MXL_GetCHRegister_ZeroIF(fe, AddrTable, ByteTable, &TableLen);

	MXL_GetMasterControl(&MasterControlByte, MC_LOAD_START);
	AddrTable[TableLen] = MASTER_CONTROL_ADDR ;
	ByteTable[TableLen] = MasterControlByte |
		state->config->AgcMasterByte ;
	TableLen += 1;

	mxl5005s_writeregs(fe, AddrTable, ByteTable, TableLen);

	msleep(100);

	return 0;
}
/* End: Custom code taken from the Realtek driver */

/* ----------------------------------------------------------------
 * Begin: Reference driver code found in the Realtek driver.
 * Copyright (C) 2008 MaxLinear
 */
static u16 MXL5005_RegisterInit(struct dvb_frontend *fe)
{
	struct mxl5005s_state *state = fe->tuner_priv;
	state->TunerRegs_Num = TUNER_REGS_NUM ;

	state->TunerRegs[0].Reg_Num = 9 ;
	state->TunerRegs[0].Reg_Val = 0x40 ;

	state->TunerRegs[1].Reg_Num = 11 ;
	state->TunerRegs[1].Reg_Val = 0x19 ;

	state->TunerRegs[2].Reg_Num = 12 ;
	state->TunerRegs[2].Reg_Val = 0x60 ;

	state->TunerRegs[3].Reg_Num = 13 ;
	state->TunerRegs[3].Reg_Val = 0x00 ;

	state->TunerRegs[4].Reg_Num = 14 ;
	state->TunerRegs[4].Reg_Val = 0x00 ;

	state->TunerRegs[5].Reg_Num = 15 ;
	state->TunerRegs[5].Reg_Val = 0xC0 ;

	state->TunerRegs[6].Reg_Num = 16 ;
	state->TunerRegs[6].Reg_Val = 0x00 ;

	state->TunerRegs[7].Reg_Num = 17 ;
	state->TunerRegs[7].Reg_Val = 0x00 ;

	state->TunerRegs[8].Reg_Num = 18 ;
	state->TunerRegs[8].Reg_Val = 0x00 ;

	state->TunerRegs[9].Reg_Num = 19 ;
	state->TunerRegs[9].Reg_Val = 0x34 ;

	state->TunerRegs[10].Reg_Num = 21 ;
	state->TunerRegs[10].Reg_Val = 0x00 ;

	state->TunerRegs[11].Reg_Num = 22 ;
	state->TunerRegs[11].Reg_Val = 0x6B ;

	state->TunerRegs[12].Reg_Num = 23 ;
	state->TunerRegs[12].Reg_Val = 0x35 ;

	state->TunerRegs[13].Reg_Num = 24 ;
	state->TunerRegs[13].Reg_Val = 0x70 ;

	state->TunerRegs[14].Reg_Num = 25 ;
	state->TunerRegs[14].Reg_Val = 0x3E ;

	state->TunerRegs[15].Reg_Num = 26 ;
	state->TunerRegs[15].Reg_Val = 0x82 ;

	state->TunerRegs[16].Reg_Num = 31 ;
	state->TunerRegs[16].Reg_Val = 0x00 ;

	state->TunerRegs[17].Reg_Num = 32 ;
	state->TunerRegs[17].Reg_Val = 0x40 ;

	state->TunerRegs[18].Reg_Num = 33 ;
	state->TunerRegs[18].Reg_Val = 0x53 ;

	state->TunerRegs[19].Reg_Num = 34 ;
	state->TunerRegs[19].Reg_Val = 0x81 ;

	state->TunerRegs[20].Reg_Num = 35 ;
	state->TunerRegs[20].Reg_Val = 0xC9 ;

	state->TunerRegs[21].Reg_Num = 36 ;
	state->TunerRegs[21].Reg_Val = 0x01 ;

	state->TunerRegs[22].Reg_Num = 37 ;
	state->TunerRegs[22].Reg_Val = 0x00 ;

	state->TunerRegs[23].Reg_Num = 41 ;
	state->TunerRegs[23].Reg_Val = 0x00 ;

	state->TunerRegs[24].Reg_Num = 42 ;
	state->TunerRegs[24].Reg_Val = 0xF8 ;

	state->TunerRegs[25].Reg_Num = 43 ;
	state->TunerRegs[25].Reg_Val = 0x43 ;

	state->TunerRegs[26].Reg_Num = 44 ;
	state->TunerRegs[26].Reg_Val = 0x20 ;

	state->TunerRegs[27].Reg_Num = 45 ;
	state->TunerRegs[27].Reg_Val = 0x80 ;

	state->TunerRegs[28].Reg_Num = 46 ;
	state->TunerRegs[28].Reg_Val = 0x88 ;

	state->TunerRegs[29].Reg_Num = 47 ;
	state->TunerRegs[29].Reg_Val = 0x86 ;

	state->TunerRegs[30].Reg_Num = 48 ;
	state->TunerRegs[30].Reg_Val = 0x00 ;

	state->TunerRegs[31].Reg_Num = 49 ;
	state->TunerRegs[31].Reg_Val = 0x00 ;

	state->TunerRegs[32].Reg_Num = 53 ;
	state->TunerRegs[32].Reg_Val = 0x94 ;

	state->TunerRegs[33].Reg_Num = 54 ;
	state->TunerRegs[33].Reg_Val = 0xFA ;

	state->TunerRegs[34].Reg_Num = 55 ;
	state->TunerRegs[34].Reg_Val = 0x92 ;

	state->TunerRegs[35].Reg_Num = 56 ;
	state->TunerRegs[35].Reg_Val = 0x80 ;

	state->TunerRegs[36].Reg_Num = 57 ;
	state->TunerRegs[36].Reg_Val = 0x41 ;

	state->TunerRegs[37].Reg_Num = 58 ;
	state->TunerRegs[37].Reg_Val = 0xDB ;

	state->TunerRegs[38].Reg_Num = 59 ;
	state->TunerRegs[38].Reg_Val = 0x00 ;

	state->TunerRegs[39].Reg_Num = 60 ;
	state->TunerRegs[39].Reg_Val = 0x00 ;

	state->TunerRegs[40].Reg_Num = 61 ;
	state->TunerRegs[40].Reg_Val = 0x00 ;

	state->TunerRegs[41].Reg_Num = 62 ;
	state->TunerRegs[41].Reg_Val = 0x00 ;

	state->TunerRegs[42].Reg_Num = 65 ;
	state->TunerRegs[42].Reg_Val = 0xF8 ;

	state->TunerRegs[43].Reg_Num = 66 ;
	state->TunerRegs[43].Reg_Val = 0xE4 ;

	state->TunerRegs[44].Reg_Num = 67 ;
	state->TunerRegs[44].Reg_Val = 0x90 ;

	state->TunerRegs[45].Reg_Num = 68 ;
	state->TunerRegs[45].Reg_Val = 0xC0 ;

	state->TunerRegs[46].Reg_Num = 69 ;
	state->TunerRegs[46].Reg_Val = 0x01 ;

	state->TunerRegs[47].Reg_Num = 70 ;
	state->TunerRegs[47].Reg_Val = 0x50 ;

	state->TunerRegs[48].Reg_Num = 71 ;
	state->TunerRegs[48].Reg_Val = 0x06 ;

	state->TunerRegs[49].Reg_Num = 72 ;
	state->TunerRegs[49].Reg_Val = 0x00 ;

	state->TunerRegs[50].Reg_Num = 73 ;
	state->TunerRegs[50].Reg_Val = 0x20 ;

	state->TunerRegs[51].Reg_Num = 76 ;
	state->TunerRegs[51].Reg_Val = 0xBB ;

	state->TunerRegs[52].Reg_Num = 77 ;
	state->TunerRegs[52].Reg_Val = 0x13 ;

	state->TunerRegs[53].Reg_Num = 81 ;
	state->TunerRegs[53].Reg_Val = 0x04 ;

	state->TunerRegs[54].Reg_Num = 82 ;
	state->TunerRegs[54].Reg_Val = 0x75 ;

	state->TunerRegs[55].Reg_Num = 83 ;
	state->TunerRegs[55].Reg_Val = 0x00 ;

	state->TunerRegs[56].Reg_Num = 84 ;
	state->TunerRegs[56].Reg_Val = 0x00 ;

	state->TunerRegs[57].Reg_Num = 85 ;
	state->TunerRegs[57].Reg_Val = 0x00 ;

	state->TunerRegs[58].Reg_Num = 91 ;
	state->TunerRegs[58].Reg_Val = 0x70 ;

	state->TunerRegs[59].Reg_Num = 92 ;
	state->TunerRegs[59].Reg_Val = 0x00 ;

	state->TunerRegs[60].Reg_Num = 93 ;
	state->TunerRegs[60].Reg_Val = 0x00 ;

	state->TunerRegs[61].Reg_Num = 94 ;
	state->TunerRegs[61].Reg_Val = 0x00 ;

	state->TunerRegs[62].Reg_Num = 95 ;
	state->TunerRegs[62].Reg_Val = 0x0C ;

	state->TunerRegs[63].Reg_Num = 96 ;
	state->TunerRegs[63].Reg_Val = 0x00 ;

	state->TunerRegs[64].Reg_Num = 97 ;
	state->TunerRegs[64].Reg_Val = 0x00 ;

	state->TunerRegs[65].Reg_Num = 98 ;
	state->TunerRegs[65].Reg_Val = 0xE2 ;

	state->TunerRegs[66].Reg_Num = 99 ;
	state->TunerRegs[66].Reg_Val = 0x00 ;

	state->TunerRegs[67].Reg_Num = 100 ;
	state->TunerRegs[67].Reg_Val = 0x00 ;

	state->TunerRegs[68].Reg_Num = 101 ;
	state->TunerRegs[68].Reg_Val = 0x12 ;

	state->TunerRegs[69].Reg_Num = 102 ;
	state->TunerRegs[69].Reg_Val = 0x80 ;

	state->TunerRegs[70].Reg_Num = 103 ;
	state->TunerRegs[70].Reg_Val = 0x32 ;

	state->TunerRegs[71].Reg_Num = 104 ;
	state->TunerRegs[71].Reg_Val = 0xB4 ;

	state->TunerRegs[72].Reg_Num = 105 ;
	state->TunerRegs[72].Reg_Val = 0x60 ;

	state->TunerRegs[73].Reg_Num = 106 ;
	state->TunerRegs[73].Reg_Val = 0x83 ;

	state->TunerRegs[74].Reg_Num = 107 ;
	state->TunerRegs[74].Reg_Val = 0x84 ;

	state->TunerRegs[75].Reg_Num = 108 ;
	state->TunerRegs[75].Reg_Val = 0x9C ;

	state->TunerRegs[76].Reg_Num = 109 ;
	state->TunerRegs[76].Reg_Val = 0x02 ;

	state->TunerRegs[77].Reg_Num = 110 ;
	state->TunerRegs[77].Reg_Val = 0x81 ;

	state->TunerRegs[78].Reg_Num = 111 ;
	state->TunerRegs[78].Reg_Val = 0xC0 ;

	state->TunerRegs[79].Reg_Num = 112 ;
	state->TunerRegs[79].Reg_Val = 0x10 ;

	state->TunerRegs[80].Reg_Num = 131 ;
	state->TunerRegs[80].Reg_Val = 0x8A ;

	state->TunerRegs[81].Reg_Num = 132 ;
	state->TunerRegs[81].Reg_Val = 0x10 ;

	state->TunerRegs[82].Reg_Num = 133 ;
	state->TunerRegs[82].Reg_Val = 0x24 ;

	state->TunerRegs[83].Reg_Num = 134 ;
	state->TunerRegs[83].Reg_Val = 0x00 ;

	state->TunerRegs[84].Reg_Num = 135 ;
	state->TunerRegs[84].Reg_Val = 0x00 ;

	state->TunerRegs[85].Reg_Num = 136 ;
	state->TunerRegs[85].Reg_Val = 0x7E ;

	state->TunerRegs[86].Reg_Num = 137 ;
	state->TunerRegs[86].Reg_Val = 0x40 ;

	state->TunerRegs[87].Reg_Num = 138 ;
	state->TunerRegs[87].Reg_Val = 0x38 ;

	state->TunerRegs[88].Reg_Num = 146 ;
	state->TunerRegs[88].Reg_Val = 0xF6 ;

	state->TunerRegs[89].Reg_Num = 147 ;
	state->TunerRegs[89].Reg_Val = 0x1A ;

	state->TunerRegs[90].Reg_Num = 148 ;
	state->TunerRegs[90].Reg_Val = 0x62 ;

	state->TunerRegs[91].Reg_Num = 149 ;
	state->TunerRegs[91].Reg_Val = 0x33 ;

	state->TunerRegs[92].Reg_Num = 150 ;
	state->TunerRegs[92].Reg_Val = 0x80 ;

	state->TunerRegs[93].Reg_Num = 156 ;
	state->TunerRegs[93].Reg_Val = 0x56 ;

	state->TunerRegs[94].Reg_Num = 157 ;
	state->TunerRegs[94].Reg_Val = 0x17 ;

	state->TunerRegs[95].Reg_Num = 158 ;
	state->TunerRegs[95].Reg_Val = 0xA9 ;

	state->TunerRegs[96].Reg_Num = 159 ;
	state->TunerRegs[96].Reg_Val = 0x00 ;

	state->TunerRegs[97].Reg_Num = 160 ;
	state->TunerRegs[97].Reg_Val = 0x00 ;

	state->TunerRegs[98].Reg_Num = 161 ;
	state->TunerRegs[98].Reg_Val = 0x00 ;

	state->TunerRegs[99].Reg_Num = 162 ;
	state->TunerRegs[99].Reg_Val = 0x40 ;

	state->TunerRegs[100].Reg_Num = 166 ;
	state->TunerRegs[100].Reg_Val = 0xAE ;

	state->TunerRegs[101].Reg_Num = 167 ;
	state->TunerRegs[101].Reg_Val = 0x1B ;

	state->TunerRegs[102].Reg_Num = 168 ;
	state->TunerRegs[102].Reg_Val = 0xF2 ;

	state->TunerRegs[103].Reg_Num = 195 ;
	state->TunerRegs[103].Reg_Val = 0x00 ;

	return 0 ;
}

static u16 MXL5005_ControlInit(struct dvb_frontend *fe)
{
	struct mxl5005s_state *state = fe->tuner_priv;
	state->Init_Ctrl_Num = INITCTRL_NUM;

	state->Init_Ctrl[0].Ctrl_Num = DN_IQTN_AMP_CUT ;
	state->Init_Ctrl[0].size = 1 ;
	state->Init_Ctrl[0].addr[0] = 73;
	state->Init_Ctrl[0].bit[0] = 7;
	state->Init_Ctrl[0].val[0] = 0;

	state->Init_Ctrl[1].Ctrl_Num = BB_MODE ;
	state->Init_Ctrl[1].size = 1 ;
	state->Init_Ctrl[1].addr[0] = 53;
	state->Init_Ctrl[1].bit[0] = 2;
	state->Init_Ctrl[1].val[0] = 1;

	state->Init_Ctrl[2].Ctrl_Num = BB_BUF ;
	state->Init_Ctrl[2].size = 2 ;
	state->Init_Ctrl[2].addr[0] = 53;
	state->Init_Ctrl[2].bit[0] = 1;
	state->Init_Ctrl[2].val[0] = 0;
	state->Init_Ctrl[2].addr[1] = 57;
	state->Init_Ctrl[2].bit[1] = 0;
	state->Init_Ctrl[2].val[1] = 1;

	state->Init_Ctrl[3].Ctrl_Num = BB_BUF_OA ;
	state->Init_Ctrl[3].size = 1 ;
	state->Init_Ctrl[3].addr[0] = 53;
	state->Init_Ctrl[3].bit[0] = 0;
	state->Init_Ctrl[3].val[0] = 0;

	state->Init_Ctrl[4].Ctrl_Num = BB_ALPF_BANDSELECT ;
	state->Init_Ctrl[4].size = 3 ;
	state->Init_Ctrl[4].addr[0] = 53;
	state->Init_Ctrl[4].bit[0] = 5;
	state->Init_Ctrl[4].val[0] = 0;
	state->Init_Ctrl[4].addr[1] = 53;
	state->Init_Ctrl[4].bit[1] = 6;
	state->Init_Ctrl[4].val[1] = 0;
	state->Init_Ctrl[4].addr[2] = 53;
	state->Init_Ctrl[4].bit[2] = 7;
	state->Init_Ctrl[4].val[2] = 1;

	state->Init_Ctrl[5].Ctrl_Num = BB_IQSWAP ;
	state->Init_Ctrl[5].size = 1 ;
	state->Init_Ctrl[5].addr[0] = 59;
	state->Init_Ctrl[5].bit[0] = 0;
	state->Init_Ctrl[5].val[0] = 0;

	state->Init_Ctrl[6].Ctrl_Num = BB_DLPF_BANDSEL ;
	state->Init_Ctrl[6].size = 2 ;
	state->Init_Ctrl[6].addr[0] = 53;
	state->Init_Ctrl[6].bit[0] = 3;
	state->Init_Ctrl[6].val[0] = 0;
	state->Init_Ctrl[6].addr[1] = 53;
	state->Init_Ctrl[6].bit[1] = 4;
	state->Init_Ctrl[6].val[1] = 1;

	state->Init_Ctrl[7].Ctrl_Num = RFSYN_CHP_GAIN ;
	state->Init_Ctrl[7].size = 4 ;
	state->Init_Ctrl[7].addr[0] = 22;
	state->Init_Ctrl[7].bit[0] = 4;
	state->Init_Ctrl[7].val[0] = 0;
	state->Init_Ctrl[7].addr[1] = 22;
	state->Init_Ctrl[7].bit[1] = 5;
	state->Init_Ctrl[7].val[1] = 1;
	state->Init_Ctrl[7].addr[2] = 22;
	state->Init_Ctrl[7].bit[2] = 6;
	state->Init_Ctrl[7].val[2] = 1;
	state->Init_Ctrl[7].addr[3] = 22;
	state->Init_Ctrl[7].bit[3] = 7;
	state->Init_Ctrl[7].val[3] = 0;

	state->Init_Ctrl[8].Ctrl_Num = RFSYN_EN_CHP_HIGAIN ;
	state->Init_Ctrl[8].size = 1 ;
	state->Init_Ctrl[8].addr[0] = 22;
	state->Init_Ctrl[8].bit[0] = 2;
	state->Init_Ctrl[8].val[0] = 0;

	state->Init_Ctrl[9].Ctrl_Num = AGC_IF ;
	state->Init_Ctrl[9].size = 4 ;
	state->Init_Ctrl[9].addr[0] = 76;
	state->Init_Ctrl[9].bit[0] = 0;
	state->Init_Ctrl[9].val[0] = 1;
	state->Init_Ctrl[9].addr[1] = 76;
	state->Init_Ctrl[9].bit[1] = 1;
	state->Init_Ctrl[9].val[1] = 1;
	state->Init_Ctrl[9].addr[2] = 76;
	state->Init_Ctrl[9].bit[2] = 2;
	state->Init_Ctrl[9].val[2] = 0;
	state->Init_Ctrl[9].addr[3] = 76;
	state->Init_Ctrl[9].bit[3] = 3;
	state->Init_Ctrl[9].val[3] = 1;

	state->Init_Ctrl[10].Ctrl_Num = AGC_RF ;
	state->Init_Ctrl[10].size = 4 ;
	state->Init_Ctrl[10].addr[0] = 76;
	state->Init_Ctrl[10].bit[0] = 4;
	state->Init_Ctrl[10].val[0] = 1;
	state->Init_Ctrl[10].addr[1] = 76;
	state->Init_Ctrl[10].bit[1] = 5;
	state->Init_Ctrl[10].val[1] = 1;
	state->Init_Ctrl[10].addr[2] = 76;
	state->Init_Ctrl[10].bit[2] = 6;
	state->Init_Ctrl[10].val[2] = 0;
	state->Init_Ctrl[10].addr[3] = 76;
	state->Init_Ctrl[10].bit[3] = 7;
	state->Init_Ctrl[10].val[3] = 1;

	state->Init_Ctrl[11].Ctrl_Num = IF_DIVVAL ;
	state->Init_Ctrl[11].size = 5 ;
	state->Init_Ctrl[11].addr[0] = 43;
	state->Init_Ctrl[11].bit[0] = 3;
	state->Init_Ctrl[11].val[0] = 0;
	state->Init_Ctrl[11].addr[1] = 43;
	state->Init_Ctrl[11].bit[1] = 4;
	state->Init_Ctrl[11].val[1] = 0;
	state->Init_Ctrl[11].addr[2] = 43;
	state->Init_Ctrl[11].bit[2] = 5;
	state->Init_Ctrl[11].val[2] = 0;
	state->Init_Ctrl[11].addr[3] = 43;
	state->Init_Ctrl[11].bit[3] = 6;
	state->Init_Ctrl[11].val[3] = 1;
	state->Init_Ctrl[11].addr[4] = 43;
	state->Init_Ctrl[11].bit[4] = 7;
	state->Init_Ctrl[11].val[4] = 0;

	state->Init_Ctrl[12].Ctrl_Num = IF_VCO_BIAS ;
	state->Init_Ctrl[12].size = 6 ;
	state->Init_Ctrl[12].addr[0] = 44;
	state->Init_Ctrl[12].bit[0] = 2;
	state->Init_Ctrl[12].val[0] = 0;
	state->Init_Ctrl[12].addr[1] = 44;
	state->Init_Ctrl[12].bit[1] = 3;
	state->Init_Ctrl[12].val[1] = 0;
	state->Init_Ctrl[12].addr[2] = 44;
	state->Init_Ctrl[12].bit[2] = 4;
	state->Init_Ctrl[12].val[2] = 0;
	state->Init_Ctrl[12].addr[3] = 44;
	state->Init_Ctrl[12].bit[3] = 5;
	state->Init_Ctrl[12].val[3] = 1;
	state->Init_Ctrl[12].addr[4] = 44;
	state->Init_Ctrl[12].bit[4] = 6;
	state->Init_Ctrl[12].val[4] = 0;
	state->Init_Ctrl[12].addr[5] = 44;
	state->Init_Ctrl[12].bit[5] = 7;
	state->Init_Ctrl[12].val[5] = 0;

	state->Init_Ctrl[13].Ctrl_Num = CHCAL_INT_MOD_IF ;
	state->Init_Ctrl[13].size = 7 ;
	state->Init_Ctrl[13].addr[0] = 11;
	state->Init_Ctrl[13].bit[0] = 0;
	state->Init_Ctrl[13].val[0] = 1;
	state->Init_Ctrl[13].addr[1] = 11;
	state->Init_Ctrl[13].bit[1] = 1;
	state->Init_Ctrl[13].val[1] = 0;
	state->Init_Ctrl[13].addr[2] = 11;
	state->Init_Ctrl[13].bit[2] = 2;
	state->Init_Ctrl[13].val[2] = 0;
	state->Init_Ctrl[13].addr[3] = 11;
	state->Init_Ctrl[13].bit[3] = 3;
	state->Init_Ctrl[13].val[3] = 1;
	state->Init_Ctrl[13].addr[4] = 11;
	state->Init_Ctrl[13].bit[4] = 4;
	state->Init_Ctrl[13].val[4] = 1;
	state->Init_Ctrl[13].addr[5] = 11;
	state->Init_Ctrl[13].bit[5] = 5;
	state->Init_Ctrl[13].val[5] = 0;
	state->Init_Ctrl[13].addr[6] = 11;
	state->Init_Ctrl[13].bit[6] = 6;
	state->Init_Ctrl[13].val[6] = 0;

	state->Init_Ctrl[14].Ctrl_Num = CHCAL_FRAC_MOD_IF ;
	state->Init_Ctrl[14].size = 16 ;
	state->Init_Ctrl[14].addr[0] = 13;
	state->Init_Ctrl[14].bit[0] = 0;
	state->Init_Ctrl[14].val[0] = 0;
	state->Init_Ctrl[14].addr[1] = 13;
	state->Init_Ctrl[14].bit[1] = 1;
	state->Init_Ctrl[14].val[1] = 0;
	state->Init_Ctrl[14].addr[2] = 13;
	state->Init_Ctrl[14].bit[2] = 2;
	state->Init_Ctrl[14].val[2] = 0;
	state->Init_Ctrl[14].addr[3] = 13;
	state->Init_Ctrl[14].bit[3] = 3;
	state->Init_Ctrl[14].val[3] = 0;
	state->Init_Ctrl[14].addr[4] = 13;
	state->Init_Ctrl[14].bit[4] = 4;
	state->Init_Ctrl[14].val[4] = 0;
	state->Init_Ctrl[14].addr[5] = 13;
	state->Init_Ctrl[14].bit[5] = 5;
	state->Init_Ctrl[14].val[5] = 0;
	state->Init_Ctrl[14].addr[6] = 13;
	state->Init_Ctrl[14].bit[6] = 6;
	state->Init_Ctrl[14].val[6] = 0;
	state->Init_Ctrl[14].addr[7] = 13;
	state->Init_Ctrl[14].bit[7] = 7;
	state->Init_Ctrl[14].val[7] = 0;
	state->Init_Ctrl[14].addr[8] = 12;
	state->Init_Ctrl[14].bit[8] = 0;
	state->Init_Ctrl[14].val[8] = 0;
	state->Init_Ctrl[14].addr[9] = 12;
	state->Init_Ctrl[14].bit[9] = 1;
	state->Init_Ctrl[14].val[9] = 0;
	state->Init_Ctrl[14].addr[10] = 12;
	state->Init_Ctrl[14].bit[10] = 2;
	state->Init_Ctrl[14].val[10] = 0;
	state->Init_Ctrl[14].addr[11] = 12;
	state->Init_Ctrl[14].bit[11] = 3;
	state->Init_Ctrl[14].val[11] = 0;
	state->Init_Ctrl[14].addr[12] = 12;
	state->Init_Ctrl[14].bit[12] = 4;
	state->Init_Ctrl[14].val[12] = 0;
	state->Init_Ctrl[14].addr[13] = 12;
	state->Init_Ctrl[14].bit[13] = 5;
	state->Init_Ctrl[14].val[13] = 1;
	state->Init_Ctrl[14].addr[14] = 12;
	state->Init_Ctrl[14].bit[14] = 6;
	state->Init_Ctrl[14].val[14] = 1;
	state->Init_Ctrl[14].addr[15] = 12;
	state->Init_Ctrl[14].bit[15] = 7;
	state->Init_Ctrl[14].val[15] = 0;

	state->Init_Ctrl[15].Ctrl_Num = DRV_RES_SEL ;
	state->Init_Ctrl[15].size = 3 ;
	state->Init_Ctrl[15].addr[0] = 147;
	state->Init_Ctrl[15].bit[0] = 2;
	state->Init_Ctrl[15].val[0] = 0;
	state->Init_Ctrl[15].addr[1] = 147;
	state->Init_Ctrl[15].bit[1] = 3;
	state->Init_Ctrl[15].val[1] = 1;
	state->Init_Ctrl[15].addr[2] = 147;
	state->Init_Ctrl[15].bit[2] = 4;
	state->Init_Ctrl[15].val[2] = 1;

	state->Init_Ctrl[16].Ctrl_Num = I_DRIVER ;
	state->Init_Ctrl[16].size = 2 ;
	state->Init_Ctrl[16].addr[0] = 147;
	state->Init_Ctrl[16].bit[0] = 0;
	state->Init_Ctrl[16].val[0] = 0;
	state->Init_Ctrl[16].addr[1] = 147;
	state->Init_Ctrl[16].bit[1] = 1;
	state->Init_Ctrl[16].val[1] = 1;

	state->Init_Ctrl[17].Ctrl_Num = EN_AAF ;
	state->Init_Ctrl[17].size = 1 ;
	state->Init_Ctrl[17].addr[0] = 147;
	state->Init_Ctrl[17].bit[0] = 7;
	state->Init_Ctrl[17].val[0] = 0;

	state->Init_Ctrl[18].Ctrl_Num = EN_3P ;
	state->Init_Ctrl[18].size = 1 ;
	state->Init_Ctrl[18].addr[0] = 147;
	state->Init_Ctrl[18].bit[0] = 6;
	state->Init_Ctrl[18].val[0] = 0;

	state->Init_Ctrl[19].Ctrl_Num = EN_AUX_3P ;
	state->Init_Ctrl[19].size = 1 ;
	state->Init_Ctrl[19].addr[0] = 156;
	state->Init_Ctrl[19].bit[0] = 0;
	state->Init_Ctrl[19].val[0] = 0;

	state->Init_Ctrl[20].Ctrl_Num = SEL_AAF_BAND ;
	state->Init_Ctrl[20].size = 1 ;
	state->Init_Ctrl[20].addr[0] = 147;
	state->Init_Ctrl[20].bit[0] = 5;
	state->Init_Ctrl[20].val[0] = 0;

	state->Init_Ctrl[21].Ctrl_Num = SEQ_ENCLK16_CLK_OUT ;
	state->Init_Ctrl[21].size = 1 ;
	state->Init_Ctrl[21].addr[0] = 137;
	state->Init_Ctrl[21].bit[0] = 4;
	state->Init_Ctrl[21].val[0] = 0;

	state->Init_Ctrl[22].Ctrl_Num = SEQ_SEL4_16B ;
	state->Init_Ctrl[22].size = 1 ;
	state->Init_Ctrl[22].addr[0] = 137;
	state->Init_Ctrl[22].bit[0] = 7;
	state->Init_Ctrl[22].val[0] = 0;

	state->Init_Ctrl[23].Ctrl_Num = XTAL_CAPSELECT ;
	state->Init_Ctrl[23].size = 1 ;
	state->Init_Ctrl[23].addr[0] = 91;
	state->Init_Ctrl[23].bit[0] = 5;
	state->Init_Ctrl[23].val[0] = 1;

	state->Init_Ctrl[24].Ctrl_Num = IF_SEL_DBL ;
	state->Init_Ctrl[24].size = 1 ;
	state->Init_Ctrl[24].addr[0] = 43;
	state->Init_Ctrl[24].bit[0] = 0;
	state->Init_Ctrl[24].val[0] = 1;

	state->Init_Ctrl[25].Ctrl_Num = RFSYN_R_DIV ;
	state->Init_Ctrl[25].size = 2 ;
	state->Init_Ctrl[25].addr[0] = 22;
	state->Init_Ctrl[25].bit[0] = 0;
	state->Init_Ctrl[25].val[0] = 1;
	state->Init_Ctrl[25].addr[1] = 22;
	state->Init_Ctrl[25].bit[1] = 1;
	state->Init_Ctrl[25].val[1] = 1;

	state->Init_Ctrl[26].Ctrl_Num = SEQ_EXTSYNTHCALIF ;
	state->Init_Ctrl[26].size = 1 ;
	state->Init_Ctrl[26].addr[0] = 134;
	state->Init_Ctrl[26].bit[0] = 2;
	state->Init_Ctrl[26].val[0] = 0;

	state->Init_Ctrl[27].Ctrl_Num = SEQ_EXTDCCAL ;
	state->Init_Ctrl[27].size = 1 ;
	state->Init_Ctrl[27].addr[0] = 137;
	state->Init_Ctrl[27].bit[0] = 3;
	state->Init_Ctrl[27].val[0] = 0;

	state->Init_Ctrl[28].Ctrl_Num = AGC_EN_RSSI ;
	state->Init_Ctrl[28].size = 1 ;
	state->Init_Ctrl[28].addr[0] = 77;
	state->Init_Ctrl[28].bit[0] = 7;
	state->Init_Ctrl[28].val[0] = 0;

	state->Init_Ctrl[29].Ctrl_Num = RFA_ENCLKRFAGC ;
	state->Init_Ctrl[29].size = 1 ;
	state->Init_Ctrl[29].addr[0] = 166;
	state->Init_Ctrl[29].bit[0] = 7;
	state->Init_Ctrl[29].val[0] = 1;

	state->Init_Ctrl[30].Ctrl_Num = RFA_RSSI_REFH ;
	state->Init_Ctrl[30].size = 3 ;
	state->Init_Ctrl[30].addr[0] = 166;
	state->Init_Ctrl[30].bit[0] = 0;
	state->Init_Ctrl[30].val[0] = 0;
	state->Init_Ctrl[30].addr[1] = 166;
	state->Init_Ctrl[30].bit[1] = 1;
	state->Init_Ctrl[30].val[1] = 1;
	state->Init_Ctrl[30].addr[2] = 166;
	state->Init_Ctrl[30].bit[2] = 2;
	state->Init_Ctrl[30].val[2] = 1;

	state->Init_Ctrl[31].Ctrl_Num = RFA_RSSI_REF ;
	state->Init_Ctrl[31].size = 3 ;
	state->Init_Ctrl[31].addr[0] = 166;
	state->Init_Ctrl[31].bit[0] = 3;
	state->Init_Ctrl[31].val[0] = 1;
	state->Init_Ctrl[31].addr[1] = 166;
	state->Init_Ctrl[31].bit[1] = 4;
	state->Init_Ctrl[31].val[1] = 0;
	state->Init_Ctrl[31].addr[2] = 166;
	state->Init_Ctrl[31].bit[2] = 5;
	state->Init_Ctrl[31].val[2] = 1;

	state->Init_Ctrl[32].Ctrl_Num = RFA_RSSI_REFL ;
	state->Init_Ctrl[32].size = 3 ;
	state->Init_Ctrl[32].addr[0] = 167;
	state->Init_Ctrl[32].bit[0] = 0;
	state->Init_Ctrl[32].val[0] = 1;
	state->Init_Ctrl[32].addr[1] = 167;
	state->Init_Ctrl[32].bit[1] = 1;
	state->Init_Ctrl[32].val[1] = 1;
	state->Init_Ctrl[32].addr[2] = 167;
	state->Init_Ctrl[32].bit[2] = 2;
	state->Init_Ctrl[32].val[2] = 0;

	state->Init_Ctrl[33].Ctrl_Num = RFA_FLR ;
	state->Init_Ctrl[33].size = 4 ;
	state->Init_Ctrl[33].addr[0] = 168;
	state->Init_Ctrl[33].bit[0] = 0;
	state->Init_Ctrl[33].val[0] = 0;
	state->Init_Ctrl[33].addr[1] = 168;
	state->Init_Ctrl[33].bit[1] = 1;
	state->Init_Ctrl[33].val[1] = 1;
	state->Init_Ctrl[33].addr[2] = 168;
	state->Init_Ctrl[33].bit[2] = 2;
	state->Init_Ctrl[33].val[2] = 0;
	state->Init_Ctrl[33].addr[3] = 168;
	state->Init_Ctrl[33].bit[3] = 3;
	state->Init_Ctrl[33].val[3] = 0;

	state->Init_Ctrl[34].Ctrl_Num = RFA_CEIL ;
	state->Init_Ctrl[34].size = 4 ;
	state->Init_Ctrl[34].addr[0] = 168;
	state->Init_Ctrl[34].bit[0] = 4;
	state->Init_Ctrl[34].val[0] = 1;
	state->Init_Ctrl[34].addr[1] = 168;
	state->Init_Ctrl[34].bit[1] = 5;
	state->Init_Ctrl[34].val[1] = 1;
	state->Init_Ctrl[34].addr[2] = 168;
	state->Init_Ctrl[34].bit[2] = 6;
	state->Init_Ctrl[34].val[2] = 1;
	state->Init_Ctrl[34].addr[3] = 168;
	state->Init_Ctrl[34].bit[3] = 7;
	state->Init_Ctrl[34].val[3] = 1;

	state->Init_Ctrl[35].Ctrl_Num = SEQ_EXTIQFSMPULSE ;
	state->Init_Ctrl[35].size = 1 ;
	state->Init_Ctrl[35].addr[0] = 135;
	state->Init_Ctrl[35].bit[0] = 0;
	state->Init_Ctrl[35].val[0] = 0;

	state->Init_Ctrl[36].Ctrl_Num = OVERRIDE_1 ;
	state->Init_Ctrl[36].size = 1 ;
	state->Init_Ctrl[36].addr[0] = 56;
	state->Init_Ctrl[36].bit[0] = 3;
	state->Init_Ctrl[36].val[0] = 0;

	state->Init_Ctrl[37].Ctrl_Num = BB_INITSTATE_DLPF_TUNE ;
	state->Init_Ctrl[37].size = 7 ;
	state->Init_Ctrl[37].addr[0] = 59;
	state->Init_Ctrl[37].bit[0] = 1;
	state->Init_Ctrl[37].val[0] = 0;
	state->Init_Ctrl[37].addr[1] = 59;
	state->Init_Ctrl[37].bit[1] = 2;
	state->Init_Ctrl[37].val[1] = 0;
	state->Init_Ctrl[37].addr[2] = 59;
	state->Init_Ctrl[37].bit[2] = 3;
	state->Init_Ctrl[37].val[2] = 0;
	state->Init_Ctrl[37].addr[3] = 59;
	state->Init_Ctrl[37].bit[3] = 4;
	state->Init_Ctrl[37].val[3] = 0;
	state->Init_Ctrl[37].addr[4] = 59;
	state->Init_Ctrl[37].bit[4] = 5;
	state->Init_Ctrl[37].val[4] = 0;
	state->Init_Ctrl[37].addr[5] = 59;
	state->Init_Ctrl[37].bit[5] = 6;
	state->Init_Ctrl[37].val[5] = 0;
	state->Init_Ctrl[37].addr[6] = 59;
	state->Init_Ctrl[37].bit[6] = 7;
	state->Init_Ctrl[37].val[6] = 0;

	state->Init_Ctrl[38].Ctrl_Num = TG_R_DIV ;
	state->Init_Ctrl[38].size = 6 ;
	state->Init_Ctrl[38].addr[0] = 32;
	state->Init_Ctrl[38].bit[0] = 2;
	state->Init_Ctrl[38].val[0] = 0;
	state->Init_Ctrl[38].addr[1] = 32;
	state->Init_Ctrl[38].bit[1] = 3;
	state->Init_Ctrl[38].val[1] = 0;
	state->Init_Ctrl[38].addr[2] = 32;
	state->Init_Ctrl[38].bit[2] = 4;
	state->Init_Ctrl[38].val[2] = 0;
	state->Init_Ctrl[38].addr[3] = 32;
	state->Init_Ctrl[38].bit[3] = 5;
	state->Init_Ctrl[38].val[3] = 0;
	state->Init_Ctrl[38].addr[4] = 32;
	state->Init_Ctrl[38].bit[4] = 6;
	state->Init_Ctrl[38].val[4] = 1;
	state->Init_Ctrl[38].addr[5] = 32;
	state->Init_Ctrl[38].bit[5] = 7;
	state->Init_Ctrl[38].val[5] = 0;

	state->Init_Ctrl[39].Ctrl_Num = EN_CHP_LIN_B ;
	state->Init_Ctrl[39].size = 1 ;
	state->Init_Ctrl[39].addr[0] = 25;
	state->Init_Ctrl[39].bit[0] = 3;
	state->Init_Ctrl[39].val[0] = 1;


	state->CH_Ctrl_Num = CHCTRL_NUM ;

	state->CH_Ctrl[0].Ctrl_Num = DN_POLY ;
	state->CH_Ctrl[0].size = 2 ;
	state->CH_Ctrl[0].addr[0] = 68;
	state->CH_Ctrl[0].bit[0] = 6;
	state->CH_Ctrl[0].val[0] = 1;
	state->CH_Ctrl[0].addr[1] = 68;
	state->CH_Ctrl[0].bit[1] = 7;
	state->CH_Ctrl[0].val[1] = 1;

	state->CH_Ctrl[1].Ctrl_Num = DN_RFGAIN ;
	state->CH_Ctrl[1].size = 2 ;
	state->CH_Ctrl[1].addr[0] = 70;
	state->CH_Ctrl[1].bit[0] = 6;
	state->CH_Ctrl[1].val[0] = 1;
	state->CH_Ctrl[1].addr[1] = 70;
	state->CH_Ctrl[1].bit[1] = 7;
	state->CH_Ctrl[1].val[1] = 0;

	state->CH_Ctrl[2].Ctrl_Num = DN_CAP_RFLPF ;
	state->CH_Ctrl[2].size = 9 ;
	state->CH_Ctrl[2].addr[0] = 69;
	state->CH_Ctrl[2].bit[0] = 5;
	state->CH_Ctrl[2].val[0] = 0;
	state->CH_Ctrl[2].addr[1] = 69;
	state->CH_Ctrl[2].bit[1] = 6;
	state->CH_Ctrl[2].val[1] = 0;
	state->CH_Ctrl[2].addr[2] = 69;
	state->CH_Ctrl[2].bit[2] = 7;
	state->CH_Ctrl[2].val[2] = 0;
	state->CH_Ctrl[2].addr[3] = 68;
	state->CH_Ctrl[2].bit[3] = 0;
	state->CH_Ctrl[2].val[3] = 0;
	state->CH_Ctrl[2].addr[4] = 68;
	state->CH_Ctrl[2].bit[4] = 1;
	state->CH_Ctrl[2].val[4] = 0;
	state->CH_Ctrl[2].addr[5] = 68;
	state->CH_Ctrl[2].bit[5] = 2;
	state->CH_Ctrl[2].val[5] = 0;
	state->CH_Ctrl[2].addr[6] = 68;
	state->CH_Ctrl[2].bit[6] = 3;
	state->CH_Ctrl[2].val[6] = 0;
	state->CH_Ctrl[2].addr[7] = 68;
	state->CH_Ctrl[2].bit[7] = 4;
	state->CH_Ctrl[2].val[7] = 0;
	state->CH_Ctrl[2].addr[8] = 68;
	state->CH_Ctrl[2].bit[8] = 5;
	state->CH_Ctrl[2].val[8] = 0;

	state->CH_Ctrl[3].Ctrl_Num = DN_EN_VHFUHFBAR ;
	state->CH_Ctrl[3].size = 1 ;
	state->CH_Ctrl[3].addr[0] = 70;
	state->CH_Ctrl[3].bit[0] = 5;
	state->CH_Ctrl[3].val[0] = 0;

	state->CH_Ctrl[4].Ctrl_Num = DN_GAIN_ADJUST ;
	state->CH_Ctrl[4].size = 3 ;
	state->CH_Ctrl[4].addr[0] = 73;
	state->CH_Ctrl[4].bit[0] = 4;
	state->CH_Ctrl[4].val[0] = 0;
	state->CH_Ctrl[4].addr[1] = 73;
	state->CH_Ctrl[4].bit[1] = 5;
	state->CH_Ctrl[4].val[1] = 1;
	state->CH_Ctrl[4].addr[2] = 73;
	state->CH_Ctrl[4].bit[2] = 6;
	state->CH_Ctrl[4].val[2] = 0;

	state->CH_Ctrl[5].Ctrl_Num = DN_IQTNBUF_AMP ;
	state->CH_Ctrl[5].size = 4 ;
	state->CH_Ctrl[5].addr[0] = 70;
	state->CH_Ctrl[5].bit[0] = 0;
	state->CH_Ctrl[5].val[0] = 0;
	state->CH_Ctrl[5].addr[1] = 70;
	state->CH_Ctrl[5].bit[1] = 1;
	state->CH_Ctrl[5].val[1] = 0;
	state->CH_Ctrl[5].addr[2] = 70;
	state->CH_Ctrl[5].bit[2] = 2;
	state->CH_Ctrl[5].val[2] = 0;
	state->CH_Ctrl[5].addr[3] = 70;
	state->CH_Ctrl[5].bit[3] = 3;
	state->CH_Ctrl[5].val[3] = 0;

	state->CH_Ctrl[6].Ctrl_Num = DN_IQTNGNBFBIAS_BST ;
	state->CH_Ctrl[6].size = 1 ;
	state->CH_Ctrl[6].addr[0] = 70;
	state->CH_Ctrl[6].bit[0] = 4;
	state->CH_Ctrl[6].val[0] = 1;

	state->CH_Ctrl[7].Ctrl_Num = RFSYN_EN_OUTMUX ;
	state->CH_Ctrl[7].size = 1 ;
	state->CH_Ctrl[7].addr[0] = 111;
	state->CH_Ctrl[7].bit[0] = 4;
	state->CH_Ctrl[7].val[0] = 0;

	state->CH_Ctrl[8].Ctrl_Num = RFSYN_SEL_VCO_OUT ;
	state->CH_Ctrl[8].size = 1 ;
	state->CH_Ctrl[8].addr[0] = 111;
	state->CH_Ctrl[8].bit[0] = 7;
	state->CH_Ctrl[8].val[0] = 1;

	state->CH_Ctrl[9].Ctrl_Num = RFSYN_SEL_VCO_HI ;
	state->CH_Ctrl[9].size = 1 ;
	state->CH_Ctrl[9].addr[0] = 111;
	state->CH_Ctrl[9].bit[0] = 6;
	state->CH_Ctrl[9].val[0] = 1;

	state->CH_Ctrl[10].Ctrl_Num = RFSYN_SEL_DIVM ;
	state->CH_Ctrl[10].size = 1 ;
	state->CH_Ctrl[10].addr[0] = 111;
	state->CH_Ctrl[10].bit[0] = 5;
	state->CH_Ctrl[10].val[0] = 0;

	state->CH_Ctrl[11].Ctrl_Num = RFSYN_RF_DIV_BIAS ;
	state->CH_Ctrl[11].size = 2 ;
	state->CH_Ctrl[11].addr[0] = 110;
	state->CH_Ctrl[11].bit[0] = 0;
	state->CH_Ctrl[11].val[0] = 1;
	state->CH_Ctrl[11].addr[1] = 110;
	state->CH_Ctrl[11].bit[1] = 1;
	state->CH_Ctrl[11].val[1] = 0;

	state->CH_Ctrl[12].Ctrl_Num = DN_SEL_FREQ ;
	state->CH_Ctrl[12].size = 3 ;
	state->CH_Ctrl[12].addr[0] = 69;
	state->CH_Ctrl[12].bit[0] = 2;
	state->CH_Ctrl[12].val[0] = 0;
	state->CH_Ctrl[12].addr[1] = 69;
	state->CH_Ctrl[12].bit[1] = 3;
	state->CH_Ctrl[12].val[1] = 0;
	state->CH_Ctrl[12].addr[2] = 69;
	state->CH_Ctrl[12].bit[2] = 4;
	state->CH_Ctrl[12].val[2] = 0;

	state->CH_Ctrl[13].Ctrl_Num = RFSYN_VCO_BIAS ;
	state->CH_Ctrl[13].size = 6 ;
	state->CH_Ctrl[13].addr[0] = 110;
	state->CH_Ctrl[13].bit[0] = 2;
	state->CH_Ctrl[13].val[0] = 0;
	state->CH_Ctrl[13].addr[1] = 110;
	state->CH_Ctrl[13].bit[1] = 3;
	state->CH_Ctrl[13].val[1] = 0;
	state->CH_Ctrl[13].addr[2] = 110;
	state->CH_Ctrl[13].bit[2] = 4;
	state->CH_Ctrl[13].val[2] = 0;
	state->CH_Ctrl[13].addr[3] = 110;
	state->CH_Ctrl[13].bit[3] = 5;
	state->CH_Ctrl[13].val[3] = 0;
	state->CH_Ctrl[13].addr[4] = 110;
	state->CH_Ctrl[13].bit[4] = 6;
	state->CH_Ctrl[13].val[4] = 0;
	state->CH_Ctrl[13].addr[5] = 110;
	state->CH_Ctrl[13].bit[5] = 7;
	state->CH_Ctrl[13].val[5] = 1;

	state->CH_Ctrl[14].Ctrl_Num = CHCAL_INT_MOD_RF ;
	state->CH_Ctrl[14].size = 7 ;
	state->CH_Ctrl[14].addr[0] = 14;
	state->CH_Ctrl[14].bit[0] = 0;
	state->CH_Ctrl[14].val[0] = 0;
	state->CH_Ctrl[14].addr[1] = 14;
	state->CH_Ctrl[14].bit[1] = 1;
	state->CH_Ctrl[14].val[1] = 0;
	state->CH_Ctrl[14].addr[2] = 14;
	state->CH_Ctrl[14].bit[2] = 2;
	state->CH_Ctrl[14].val[2] = 0;
	state->CH_Ctrl[14].addr[3] = 14;
	state->CH_Ctrl[14].bit[3] = 3;
	state->CH_Ctrl[14].val[3] = 0;
	state->CH_Ctrl[14].addr[4] = 14;
	state->CH_Ctrl[14].bit[4] = 4;
	state->CH_Ctrl[14].val[4] = 0;
	state->CH_Ctrl[14].addr[5] = 14;
	state->CH_Ctrl[14].bit[5] = 5;
	state->CH_Ctrl[14].val[5] = 0;
	state->CH_Ctrl[14].addr[6] = 14;
	state->CH_Ctrl[14].bit[6] = 6;
	state->CH_Ctrl[14].val[6] = 0;

	state->CH_Ctrl[15].Ctrl_Num = CHCAL_FRAC_MOD_RF ;
	state->CH_Ctrl[15].size = 18 ;
	state->CH_Ctrl[15].addr[0] = 17;
	state->CH_Ctrl[15].bit[0] = 6;
	state->CH_Ctrl[15].val[0] = 0;
	state->CH_Ctrl[15].addr[1] = 17;
	state->CH_Ctrl[15].bit[1] = 7;
	state->CH_Ctrl[15].val[1] = 0;
	state->CH_Ctrl[15].addr[2] = 16;
	state->CH_Ctrl[15].bit[2] = 0;
	state->CH_Ctrl[15].val[2] = 0;
	state->CH_Ctrl[15].addr[3] = 16;
	state->CH_Ctrl[15].bit[3] = 1;
	state->CH_Ctrl[15].val[3] = 0;
	state->CH_Ctrl[15].addr[4] = 16;
	state->CH_Ctrl[15].bit[4] = 2;
	state->CH_Ctrl[15].val[4] = 0;
	state->CH_Ctrl[15].addr[5] = 16;
	state->CH_Ctrl[15].bit[5] = 3;
	state->CH_Ctrl[15].val[5] = 0;
	state->CH_Ctrl[15].addr[6] = 16;
	state->CH_Ctrl[15].bit[6] = 4;
	state->CH_Ctrl[15].val[6] = 0;
	state->CH_Ctrl[15].addr[7] = 16;
	state->CH_Ctrl[15].bit[7] = 5;
	state->CH_Ctrl[15].val[7] = 0;
	state->CH_Ctrl[15].addr[8] = 16;
	state->CH_Ctrl[15].bit[8] = 6;
	state->CH_Ctrl[15].val[8] = 0;
	state->CH_Ctrl[15].addr[9] = 16;
	state->CH_Ctrl[15].bit[9] = 7;
	state->CH_Ctrl[15].val[9] = 0;
	state->CH_Ctrl[15].addr[10] = 15;
	state->CH_Ctrl[15].bit[10] = 0;
	state->CH_Ctrl[15].val[10] = 0;
	state->CH_Ctrl[15].addr[11] = 15;
	state->CH_Ctrl[15].bit[11] = 1;
	state->CH_Ctrl[15].val[11] = 0;
	state->CH_Ctrl[15].addr[12] = 15;
	state->CH_Ctrl[15].bit[12] = 2;
	state->CH_Ctrl[15].val[12] = 0;
	state->CH_Ctrl[15].addr[13] = 15;
	state->CH_Ctrl[15].bit[13] = 3;
	state->CH_Ctrl[15].val[13] = 0;
	state->CH_Ctrl[15].addr[14] = 15;
	state->CH_Ctrl[15].bit[14] = 4;
	state->CH_Ctrl[15].val[14] = 0;
	state->CH_Ctrl[15].addr[15] = 15;
	state->CH_Ctrl[15].bit[15] = 5;
	state->CH_Ctrl[15].val[15] = 0;
	state->CH_Ctrl[15].addr[16] = 15;
	state->CH_Ctrl[15].bit[16] = 6;
	state->CH_Ctrl[15].val[16] = 1;
	state->CH_Ctrl[15].addr[17] = 15;
	state->CH_Ctrl[15].bit[17] = 7;
	state->CH_Ctrl[15].val[17] = 1;

	state->CH_Ctrl[16].Ctrl_Num = RFSYN_LPF_R ;
	state->CH_Ctrl[16].size = 5 ;
	state->CH_Ctrl[16].addr[0] = 112;
	state->CH_Ctrl[16].bit[0] = 0;
	state->CH_Ctrl[16].val[0] = 0;
	state->CH_Ctrl[16].addr[1] = 112;
	state->CH_Ctrl[16].bit[1] = 1;
	state->CH_Ctrl[16].val[1] = 0;
	state->CH_Ctrl[16].addr[2] = 112;
	state->CH_Ctrl[16].bit[2] = 2;
	state->CH_Ctrl[16].val[2] = 0;
	state->CH_Ctrl[16].addr[3] = 112;
	state->CH_Ctrl[16].bit[3] = 3;
	state->CH_Ctrl[16].val[3] = 0;
	state->CH_Ctrl[16].addr[4] = 112;
	state->CH_Ctrl[16].bit[4] = 4;
	state->CH_Ctrl[16].val[4] = 1;

	state->CH_Ctrl[17].Ctrl_Num = CHCAL_EN_INT_RF ;
	state->CH_Ctrl[17].size = 1 ;
	state->CH_Ctrl[17].addr[0] = 14;
	state->CH_Ctrl[17].bit[0] = 7;
	state->CH_Ctrl[17].val[0] = 0;

	state->CH_Ctrl[18].Ctrl_Num = TG_LO_DIVVAL ;
	state->CH_Ctrl[18].size = 4 ;
	state->CH_Ctrl[18].addr[0] = 107;
	state->CH_Ctrl[18].bit[0] = 3;
	state->CH_Ctrl[18].val[0] = 0;
	state->CH_Ctrl[18].addr[1] = 107;
	state->CH_Ctrl[18].bit[1] = 4;
	state->CH_Ctrl[18].val[1] = 0;
	state->CH_Ctrl[18].addr[2] = 107;
	state->CH_Ctrl[18].bit[2] = 5;
	state->CH_Ctrl[18].val[2] = 0;
	state->CH_Ctrl[18].addr[3] = 107;
	state->CH_Ctrl[18].bit[3] = 6;
	state->CH_Ctrl[18].val[3] = 0;

	state->CH_Ctrl[19].Ctrl_Num = TG_LO_SELVAL ;
	state->CH_Ctrl[19].size = 3 ;
	state->CH_Ctrl[19].addr[0] = 107;
	state->CH_Ctrl[19].bit[0] = 7;
	state->CH_Ctrl[19].val[0] = 1;
	state->CH_Ctrl[19].addr[1] = 106;
	state->CH_Ctrl[19].bit[1] = 0;
	state->CH_Ctrl[19].val[1] = 1;
	state->CH_Ctrl[19].addr[2] = 106;
	state->CH_Ctrl[19].bit[2] = 1;
	state->CH_Ctrl[19].val[2] = 1;

	state->CH_Ctrl[20].Ctrl_Num = TG_DIV_VAL ;
	state->CH_Ctrl[20].size = 11 ;
	state->CH_Ctrl[20].addr[0] = 109;
	state->CH_Ctrl[20].bit[0] = 2;
	state->CH_Ctrl[20].val[0] = 0;
	state->CH_Ctrl[20].addr[1] = 109;
	state->CH_Ctrl[20].bit[1] = 3;
	state->CH_Ctrl[20].val[1] = 0;
	state->CH_Ctrl[20].addr[2] = 109;
	state->CH_Ctrl[20].bit[2] = 4;
	state->CH_Ctrl[20].val[2] = 0;
	state->CH_Ctrl[20].addr[3] = 109;
	state->CH_Ctrl[20].bit[3] = 5;
	state->CH_Ctrl[20].val[3] = 0;
	state->CH_Ctrl[20].addr[4] = 109;
	state->CH_Ctrl[20].bit[4] = 6;
	state->CH_Ctrl[20].val[4] = 0;
	state->CH_Ctrl[20].addr[5] = 109;
	state->CH_Ctrl[20].bit[5] = 7;
	state->CH_Ctrl[20].val[5] = 0;
	state->CH_Ctrl[20].addr[6] = 108;
	state->CH_Ctrl[20].bit[6] = 0;
	state->CH_Ctrl[20].val[6] = 0;
	state->CH_Ctrl[20].addr[7] = 108;
	state->CH_Ctrl[20].bit[7] = 1;
	state->CH_Ctrl[20].val[7] = 0;
	state->CH_Ctrl[20].addr[8] = 108;
	state->CH_Ctrl[20].bit[8] = 2;
	state->CH_Ctrl[20].val[8] = 1;
	state->CH_Ctrl[20].addr[9] = 108;
	state->CH_Ctrl[20].bit[9] = 3;
	state->CH_Ctrl[20].val[9] = 1;
	state->CH_Ctrl[20].addr[10] = 108;
	state->CH_Ctrl[20].bit[10] = 4;
	state->CH_Ctrl[20].val[10] = 1;

	state->CH_Ctrl[21].Ctrl_Num = TG_VCO_BIAS ;
	state->CH_Ctrl[21].size = 6 ;
	state->CH_Ctrl[21].addr[0] = 106;
	state->CH_Ctrl[21].bit[0] = 2;
	state->CH_Ctrl[21].val[0] = 0;
	state->CH_Ctrl[21].addr[1] = 106;
	state->CH_Ctrl[21].bit[1] = 3;
	state->CH_Ctrl[21].val[1] = 0;
	state->CH_Ctrl[21].addr[2] = 106;
	state->CH_Ctrl[21].bit[2] = 4;
	state->CH_Ctrl[21].val[2] = 0;
	state->CH_Ctrl[21].addr[3] = 106;
	state->CH_Ctrl[21].bit[3] = 5;
	state->CH_Ctrl[21].val[3] = 0;
	state->CH_Ctrl[21].addr[4] = 106;
	state->CH_Ctrl[21].bit[4] = 6;
	state->CH_Ctrl[21].val[4] = 0;
	state->CH_Ctrl[21].addr[5] = 106;
	state->CH_Ctrl[21].bit[5] = 7;
	state->CH_Ctrl[21].val[5] = 1;

	state->CH_Ctrl[22].Ctrl_Num = SEQ_EXTPOWERUP ;
	state->CH_Ctrl[22].size = 1 ;
	state->CH_Ctrl[22].addr[0] = 138;
	state->CH_Ctrl[22].bit[0] = 4;
	state->CH_Ctrl[22].val[0] = 1;

	state->CH_Ctrl[23].Ctrl_Num = OVERRIDE_2 ;
	state->CH_Ctrl[23].size = 1 ;
	state->CH_Ctrl[23].addr[0] = 17;
	state->CH_Ctrl[23].bit[0] = 5;
	state->CH_Ctrl[23].val[0] = 0;

	state->CH_Ctrl[24].Ctrl_Num = OVERRIDE_3 ;
	state->CH_Ctrl[24].size = 1 ;
	state->CH_Ctrl[24].addr[0] = 111;
	state->CH_Ctrl[24].bit[0] = 3;
	state->CH_Ctrl[24].val[0] = 0;

	state->CH_Ctrl[25].Ctrl_Num = OVERRIDE_4 ;
	state->CH_Ctrl[25].size = 1 ;
	state->CH_Ctrl[25].addr[0] = 112;
	state->CH_Ctrl[25].bit[0] = 7;
	state->CH_Ctrl[25].val[0] = 0;

	state->CH_Ctrl[26].Ctrl_Num = SEQ_FSM_PULSE ;
	state->CH_Ctrl[26].size = 1 ;
	state->CH_Ctrl[26].addr[0] = 136;
	state->CH_Ctrl[26].bit[0] = 7;
	state->CH_Ctrl[26].val[0] = 0;

	state->CH_Ctrl[27].Ctrl_Num = GPIO_4B ;
	state->CH_Ctrl[27].size = 1 ;
	state->CH_Ctrl[27].addr[0] = 149;
	state->CH_Ctrl[27].bit[0] = 7;
	state->CH_Ctrl[27].val[0] = 0;

	state->CH_Ctrl[28].Ctrl_Num = GPIO_3B ;
	state->CH_Ctrl[28].size = 1 ;
	state->CH_Ctrl[28].addr[0] = 149;
	state->CH_Ctrl[28].bit[0] = 6;
	state->CH_Ctrl[28].val[0] = 0;

	state->CH_Ctrl[29].Ctrl_Num = GPIO_4 ;
	state->CH_Ctrl[29].size = 1 ;
	state->CH_Ctrl[29].addr[0] = 149;
	state->CH_Ctrl[29].bit[0] = 5;
	state->CH_Ctrl[29].val[0] = 1;

	state->CH_Ctrl[30].Ctrl_Num = GPIO_3 ;
	state->CH_Ctrl[30].size = 1 ;
	state->CH_Ctrl[30].addr[0] = 149;
	state->CH_Ctrl[30].bit[0] = 4;
	state->CH_Ctrl[30].val[0] = 1;

	state->CH_Ctrl[31].Ctrl_Num = GPIO_1B ;
	state->CH_Ctrl[31].size = 1 ;
	state->CH_Ctrl[31].addr[0] = 149;
	state->CH_Ctrl[31].bit[0] = 3;
	state->CH_Ctrl[31].val[0] = 0;

	state->CH_Ctrl[32].Ctrl_Num = DAC_A_ENABLE ;
	state->CH_Ctrl[32].size = 1 ;
	state->CH_Ctrl[32].addr[0] = 93;
	state->CH_Ctrl[32].bit[0] = 1;
	state->CH_Ctrl[32].val[0] = 0;

	state->CH_Ctrl[33].Ctrl_Num = DAC_B_ENABLE ;
	state->CH_Ctrl[33].size = 1 ;
	state->CH_Ctrl[33].addr[0] = 93;
	state->CH_Ctrl[33].bit[0] = 0;
	state->CH_Ctrl[33].val[0] = 0;

	state->CH_Ctrl[34].Ctrl_Num = DAC_DIN_A ;
	state->CH_Ctrl[34].size = 6 ;
	state->CH_Ctrl[34].addr[0] = 92;
	state->CH_Ctrl[34].bit[0] = 2;
	state->CH_Ctrl[34].val[0] = 0;
	state->CH_Ctrl[34].addr[1] = 92;
	state->CH_Ctrl[34].bit[1] = 3;
	state->CH_Ctrl[34].val[1] = 0;
	state->CH_Ctrl[34].addr[2] = 92;
	state->CH_Ctrl[34].bit[2] = 4;
	state->CH_Ctrl[34].val[2] = 0;
	state->CH_Ctrl[34].addr[3] = 92;
	state->CH_Ctrl[34].bit[3] = 5;
	state->CH_Ctrl[34].val[3] = 0;
	state->CH_Ctrl[34].addr[4] = 92;
	state->CH_Ctrl[34].bit[4] = 6;
	state->CH_Ctrl[34].val[4] = 0;
	state->CH_Ctrl[34].addr[5] = 92;
	state->CH_Ctrl[34].bit[5] = 7;
	state->CH_Ctrl[34].val[5] = 0;

	state->CH_Ctrl[35].Ctrl_Num = DAC_DIN_B ;
	state->CH_Ctrl[35].size = 6 ;
	state->CH_Ctrl[35].addr[0] = 93;
	state->CH_Ctrl[35].bit[0] = 2;
	state->CH_Ctrl[35].val[0] = 0;
	state->CH_Ctrl[35].addr[1] = 93;
	state->CH_Ctrl[35].bit[1] = 3;
	state->CH_Ctrl[35].val[1] = 0;
	state->CH_Ctrl[35].addr[2] = 93;
	state->CH_Ctrl[35].bit[2] = 4;
	state->CH_Ctrl[35].val[2] = 0;
	state->CH_Ctrl[35].addr[3] = 93;
	state->CH_Ctrl[35].bit[3] = 5;
	state->CH_Ctrl[35].val[3] = 0;
	state->CH_Ctrl[35].addr[4] = 93;
	state->CH_Ctrl[35].bit[4] = 6;
	state->CH_Ctrl[35].val[4] = 0;
	state->CH_Ctrl[35].addr[5] = 93;
	state->CH_Ctrl[35].bit[5] = 7;
	state->CH_Ctrl[35].val[5] = 0;

#ifdef _MXL_PRODUCTION
	state->CH_Ctrl[36].Ctrl_Num = RFSYN_EN_DIV ;
	state->CH_Ctrl[36].size = 1 ;
	state->CH_Ctrl[36].addr[0] = 109;
	state->CH_Ctrl[36].bit[0] = 1;
	state->CH_Ctrl[36].val[0] = 1;

	state->CH_Ctrl[37].Ctrl_Num = RFSYN_DIVM ;
	state->CH_Ctrl[37].size = 2 ;
	state->CH_Ctrl[37].addr[0] = 112;
	state->CH_Ctrl[37].bit[0] = 5;
	state->CH_Ctrl[37].val[0] = 0;
	state->CH_Ctrl[37].addr[1] = 112;
	state->CH_Ctrl[37].bit[1] = 6;
	state->CH_Ctrl[37].val[1] = 0;

	state->CH_Ctrl[38].Ctrl_Num = DN_BYPASS_AGC_I2C ;
	state->CH_Ctrl[38].size = 1 ;
	state->CH_Ctrl[38].addr[0] = 65;
	state->CH_Ctrl[38].bit[0] = 1;
	state->CH_Ctrl[38].val[0] = 0;
#endif

	return 0 ;
}

static void InitTunerControls(struct dvb_frontend *fe)
{
	MXL5005_RegisterInit(fe);
	MXL5005_ControlInit(fe);
#ifdef _MXL_INTERNAL
	MXL5005_MXLControlInit(fe);
#endif
}

static u16 MXL5005_TunerConfig(struct dvb_frontend *fe,
	u8	Mode,		/* 0: Analog Mode ; 1: Digital Mode */
	u8	IF_mode,	/* for Analog Mode, 0: zero IF; 1: low IF */
	u32	Bandwidth,	/* filter  channel bandwidth (6, 7, 8) */
	u32	IF_out,		/* Desired IF Out Frequency */
	u32	Fxtal,		/* XTAL Frequency */
	u8	AGC_Mode,	/* AGC Mode - Dual AGC: 0, Single AGC: 1 */
	u16	TOP,		/* 0: Dual AGC; Value: take over point */
	u16	IF_OUT_LOAD,	/* IF Out Load Resistor (200 / 300 Ohms) */
	u8	CLOCK_OUT, 	/* 0: turn off clk out; 1: turn on clock out */
	u8	DIV_OUT,	/* 0: Div-1; 1: Div-4 */
	u8	CAPSELECT, 	/* 0: disable On-Chip pulling cap; 1: enable */
	u8	EN_RSSI, 	/* 0: disable RSSI; 1: enable RSSI */

	/* Modulation Type; */
	/* 0 - Default;	1 - DVB-T; 2 - ATSC; 3 - QAM; 4 - Analog Cable */
	u8	Mod_Type,

	/* Tracking Filter */
	/* 0 - Default; 1 - Off; 2 - Type C; 3 - Type C-H */
	u8	TF_Type
	)
{
	struct mxl5005s_state *state = fe->tuner_priv;
	u16 status = 0;

	state->Mode = Mode;
	state->IF_Mode = IF_mode;
	state->Chan_Bandwidth = Bandwidth;
	state->IF_OUT = IF_out;
	state->Fxtal = Fxtal;
	state->AGC_Mode = AGC_Mode;
	state->TOP = TOP;
	state->IF_OUT_LOAD = IF_OUT_LOAD;
	state->CLOCK_OUT = CLOCK_OUT;
	state->DIV_OUT = DIV_OUT;
	state->CAPSELECT = CAPSELECT;
	state->EN_RSSI = EN_RSSI;
	state->Mod_Type = Mod_Type;
	state->TF_Type = TF_Type;

	/* Initialize all the controls and registers */
	InitTunerControls(fe);

	/* Synthesizer LO frequency calculation */
	MXL_SynthIFLO_Calc(fe);

	return status;
}

static void MXL_SynthIFLO_Calc(struct dvb_frontend *fe)
{
	struct mxl5005s_state *state = fe->tuner_priv;
	if (state->Mode == 1) /* Digital Mode */
		state->IF_LO = state->IF_OUT;
	else /* Analog Mode */ {
		if (state->IF_Mode == 0) /* Analog Zero IF mode */
			state->IF_LO = state->IF_OUT + 400000;
		else /* Analog Low IF mode */
			state->IF_LO = state->IF_OUT + state->Chan_Bandwidth/2;
	}
}

static void MXL_SynthRFTGLO_Calc(struct dvb_frontend *fe)
{
	struct mxl5005s_state *state = fe->tuner_priv;

	if (state->Mode == 1) /* Digital Mode */ {
			/* remove 20.48MHz setting for 2.6.10 */
			state->RF_LO = state->RF_IN;
			/* change for 2.6.6 */
			state->TG_LO = state->RF_IN - 750000;
	} else /* Analog Mode */ {
		if (state->IF_Mode == 0) /* Analog Zero IF mode */ {
			state->RF_LO = state->RF_IN - 400000;
			state->TG_LO = state->RF_IN - 1750000;
		} else /* Analog Low IF mode */ {
			state->RF_LO = state->RF_IN - state->Chan_Bandwidth/2;
			state->TG_LO = state->RF_IN -
				state->Chan_Bandwidth + 500000;
		}
	}
}

static u16 MXL_OverwriteICDefault(struct dvb_frontend *fe)
{
	u16 status = 0;

	status += MXL_ControlWrite(fe, OVERRIDE_1, 1);
	status += MXL_ControlWrite(fe, OVERRIDE_2, 1);
	status += MXL_ControlWrite(fe, OVERRIDE_3, 1);
	status += MXL_ControlWrite(fe, OVERRIDE_4, 1);

	return status;
}

static u16 MXL_BlockInit(struct dvb_frontend *fe)
{
	struct mxl5005s_state *state = fe->tuner_priv;
	u16 status = 0;

	status += MXL_OverwriteICDefault(fe);

	/* Downconverter Control Dig Ana */
	status += MXL_ControlWrite(fe, DN_IQTN_AMP_CUT, state->Mode ? 1 : 0);

	/* Filter Control  Dig  Ana */
	status += MXL_ControlWrite(fe, BB_MODE, state->Mode ? 0 : 1);
	status += MXL_ControlWrite(fe, BB_BUF, state->Mode ? 3 : 2);
	status += MXL_ControlWrite(fe, BB_BUF_OA, state->Mode ? 1 : 0);
	status += MXL_ControlWrite(fe, BB_IQSWAP, state->Mode ? 0 : 1);
	status += MXL_ControlWrite(fe, BB_INITSTATE_DLPF_TUNE, 0);

	/* Initialize Low-Pass Filter */
	if (state->Mode) { /* Digital Mode */
		switch (state->Chan_Bandwidth) {
		case 8000000:
			status += MXL_ControlWrite(fe, BB_DLPF_BANDSEL, 0);
			break;
		case 7000000:
			status += MXL_ControlWrite(fe, BB_DLPF_BANDSEL, 2);
			break;
		case 6000000:
			status += MXL_ControlWrite(fe,
					BB_DLPF_BANDSEL, 3);
			break;
		}
	} else { /* Analog Mode */
		switch (state->Chan_Bandwidth) {
		case 8000000:	/* Low Zero */
			status += MXL_ControlWrite(fe, BB_ALPF_BANDSELECT,
					(state->IF_Mode ? 0 : 3));
			break;
		case 7000000:
			status += MXL_ControlWrite(fe, BB_ALPF_BANDSELECT,
					(state->IF_Mode ? 1 : 4));
			break;
		case 6000000:
			status += MXL_ControlWrite(fe, BB_ALPF_BANDSELECT,
					(state->IF_Mode ? 2 : 5));
			break;
		}
	}

	/* Charge Pump Control Dig  Ana */
	status += MXL_ControlWrite(fe, RFSYN_CHP_GAIN, state->Mode ? 5 : 8);
	status += MXL_ControlWrite(fe,
		RFSYN_EN_CHP_HIGAIN, state->Mode ? 1 : 1);
	status += MXL_ControlWrite(fe, EN_CHP_LIN_B, state->Mode ? 0 : 0);

	/* AGC TOP Control */
	if (state->AGC_Mode == 0) /* Dual AGC */ {
		status += MXL_ControlWrite(fe, AGC_IF, 15);
		status += MXL_ControlWrite(fe, AGC_RF, 15);
	} else /*  Single AGC Mode Dig  Ana */
		status += MXL_ControlWrite(fe, AGC_RF, state->Mode ? 15 : 12);

	if (state->TOP == 55) /* TOP == 5.5 */
		status += MXL_ControlWrite(fe, AGC_IF, 0x0);

	if (state->TOP == 72) /* TOP == 7.2 */
		status += MXL_ControlWrite(fe, AGC_IF, 0x1);

	if (state->TOP == 92) /* TOP == 9.2 */
		status += MXL_ControlWrite(fe, AGC_IF, 0x2);

	if (state->TOP == 110) /* TOP == 11.0 */
		status += MXL_ControlWrite(fe, AGC_IF, 0x3);

	if (state->TOP == 129) /* TOP == 12.9 */
		status += MXL_ControlWrite(fe, AGC_IF, 0x4);

	if (state->TOP == 147) /* TOP == 14.7 */
		status += MXL_ControlWrite(fe, AGC_IF, 0x5);

	if (state->TOP == 168) /* TOP == 16.8 */
		status += MXL_ControlWrite(fe, AGC_IF, 0x6);

	if (state->TOP == 194) /* TOP == 19.4 */
		status += MXL_ControlWrite(fe, AGC_IF, 0x7);

	if (state->TOP == 212) /* TOP == 21.2 */
		status += MXL_ControlWrite(fe, AGC_IF, 0x9);

	if (state->TOP == 232) /* TOP == 23.2 */
		status += MXL_ControlWrite(fe, AGC_IF, 0xA);

	if (state->TOP == 252) /* TOP == 25.2 */
		status += MXL_ControlWrite(fe, AGC_IF, 0xB);

	if (state->TOP == 271) /* TOP == 27.1 */
		status += MXL_ControlWrite(fe, AGC_IF, 0xC);

	if (state->TOP == 292) /* TOP == 29.2 */
		status += MXL_ControlWrite(fe, AGC_IF, 0xD);

	if (state->TOP == 317) /* TOP == 31.7 */
		status += MXL_ControlWrite(fe, AGC_IF, 0xE);

	if (state->TOP == 349) /* TOP == 34.9 */
		status += MXL_ControlWrite(fe, AGC_IF, 0xF);

	/* IF Synthesizer Control */
	status += MXL_IFSynthInit(fe);

	/* IF UpConverter Control */
	if (state->IF_OUT_LOAD == 200) {
		status += MXL_ControlWrite(fe, DRV_RES_SEL, 6);
		status += MXL_ControlWrite(fe, I_DRIVER, 2);
	}
	if (state->IF_OUT_LOAD == 300) {
		status += MXL_ControlWrite(fe, DRV_RES_SEL, 4);
		status += MXL_ControlWrite(fe, I_DRIVER, 1);
	}

	/* Anti-Alias Filtering Control
	 * initialise Anti-Aliasing Filter
	 */
	if (state->Mode) { /* Digital Mode */
		if (state->IF_OUT >= 4000000UL && state->IF_OUT <= 6280000UL) {
			status += MXL_ControlWrite(fe, EN_AAF, 1);
			status += MXL_ControlWrite(fe, EN_3P, 1);
			status += MXL_ControlWrite(fe, EN_AUX_3P, 1);
			status += MXL_ControlWrite(fe, SEL_AAF_BAND, 0);
		}
		if ((state->IF_OUT == 36125000UL) ||
			(state->IF_OUT == 36150000UL)) {
			status += MXL_ControlWrite(fe, EN_AAF, 1);
			status += MXL_ControlWrite(fe, EN_3P, 1);
			status += MXL_ControlWrite(fe, EN_AUX_3P, 1);
			status += MXL_ControlWrite(fe, SEL_AAF_BAND, 1);
		}
		if (state->IF_OUT > 36150000UL) {
			status += MXL_ControlWrite(fe, EN_AAF, 0);
			status += MXL_ControlWrite(fe, EN_3P, 1);
			status += MXL_ControlWrite(fe, EN_AUX_3P, 1);
			status += MXL_ControlWrite(fe, SEL_AAF_BAND, 1);
		}
	} else { /* Analog Mode */
		if (state->IF_OUT >= 4000000UL && state->IF_OUT <= 5000000UL) {
			status += MXL_ControlWrite(fe, EN_AAF, 1);
			status += MXL_ControlWrite(fe, EN_3P, 1);
			status += MXL_ControlWrite(fe, EN_AUX_3P, 1);
			status += MXL_ControlWrite(fe, SEL_AAF_BAND, 0);
		}
		if (state->IF_OUT > 5000000UL) {
			status += MXL_ControlWrite(fe, EN_AAF, 0);
			status += MXL_ControlWrite(fe, EN_3P, 0);
			status += MXL_ControlWrite(fe, EN_AUX_3P, 0);
			status += MXL_ControlWrite(fe, SEL_AAF_BAND, 0);
		}
	}

	/* Demod Clock Out */
	if (state->CLOCK_OUT)
		status += MXL_ControlWrite(fe, SEQ_ENCLK16_CLK_OUT, 1);
	else
		status += MXL_ControlWrite(fe, SEQ_ENCLK16_CLK_OUT, 0);

	if (state->DIV_OUT == 1)
		status += MXL_ControlWrite(fe, SEQ_SEL4_16B, 1);
	if (state->DIV_OUT == 0)
		status += MXL_ControlWrite(fe, SEQ_SEL4_16B, 0);

	/* Crystal Control */
	if (state->CAPSELECT)
		status += MXL_ControlWrite(fe, XTAL_CAPSELECT, 1);
	else
		status += MXL_ControlWrite(fe, XTAL_CAPSELECT, 0);

	if (state->Fxtal >= 12000000UL && state->Fxtal <= 16000000UL)
		status += MXL_ControlWrite(fe, IF_SEL_DBL, 1);
	if (state->Fxtal > 16000000UL && state->Fxtal <= 32000000UL)
		status += MXL_ControlWrite(fe, IF_SEL_DBL, 0);

	if (state->Fxtal >= 12000000UL && state->Fxtal <= 22000000UL)
		status += MXL_ControlWrite(fe, RFSYN_R_DIV, 3);
	if (state->Fxtal > 22000000UL && state->Fxtal <= 32000000UL)
		status += MXL_ControlWrite(fe, RFSYN_R_DIV, 0);

	/* Misc Controls */
	if (state->Mode == 0 && state->IF_Mode == 1) /* Analog LowIF mode */
		status += MXL_ControlWrite(fe, SEQ_EXTIQFSMPULSE, 0);
	else
		status += MXL_ControlWrite(fe, SEQ_EXTIQFSMPULSE, 1);

	/* status += MXL_ControlRead(fe, IF_DIVVAL, &IF_DIVVAL_Val); */

	/* Set TG_R_DIV */
	status += MXL_ControlWrite(fe, TG_R_DIV,
		MXL_Ceiling(state->Fxtal, 1000000));

	/* Apply Default value to BB_INITSTATE_DLPF_TUNE */

	/* RSSI Control */
	if (state->EN_RSSI) {
		status += MXL_ControlWrite(fe, SEQ_EXTSYNTHCALIF, 1);
		status += MXL_ControlWrite(fe, SEQ_EXTDCCAL, 1);
		status += MXL_ControlWrite(fe, AGC_EN_RSSI, 1);
		status += MXL_ControlWrite(fe, RFA_ENCLKRFAGC, 1);

		/* RSSI reference point */
		status += MXL_ControlWrite(fe, RFA_RSSI_REF, 2);
		status += MXL_ControlWrite(fe, RFA_RSSI_REFH, 3);
		status += MXL_ControlWrite(fe, RFA_RSSI_REFL, 1);

		/* TOP point */
		status += MXL_ControlWrite(fe, RFA_FLR, 0);
		status += MXL_ControlWrite(fe, RFA_CEIL, 12);
	}

	/* Modulation type bit settings
	 * Override the control values preset
	 */
	if (state->Mod_Type == MXL_DVBT) /* DVB-T Mode */ {
		state->AGC_Mode = 1; /* Single AGC Mode */

		/* Enable RSSI */
		status += MXL_ControlWrite(fe, SEQ_EXTSYNTHCALIF, 1);
		status += MXL_ControlWrite(fe, SEQ_EXTDCCAL, 1);
		status += MXL_ControlWrite(fe, AGC_EN_RSSI, 1);
		status += MXL_ControlWrite(fe, RFA_ENCLKRFAGC, 1);

		/* RSSI reference point */
		status += MXL_ControlWrite(fe, RFA_RSSI_REF, 3);
		status += MXL_ControlWrite(fe, RFA_RSSI_REFH, 5);
		status += MXL_ControlWrite(fe, RFA_RSSI_REFL, 1);

		/* TOP point */
		status += MXL_ControlWrite(fe, RFA_FLR, 2);
		status += MXL_ControlWrite(fe, RFA_CEIL, 13);
		if (state->IF_OUT <= 6280000UL)	/* Low IF */
			status += MXL_ControlWrite(fe, BB_IQSWAP, 0);
		else /* High IF */
			status += MXL_ControlWrite(fe, BB_IQSWAP, 1);

	}
	if (state->Mod_Type == MXL_ATSC) /* ATSC Mode */ {
		state->AGC_Mode = 1;	/* Single AGC Mode */

		/* Enable RSSI */
		status += MXL_ControlWrite(fe, SEQ_EXTSYNTHCALIF, 1);
		status += MXL_ControlWrite(fe, SEQ_EXTDCCAL, 1);
		status += MXL_ControlWrite(fe, AGC_EN_RSSI, 1);
		status += MXL_ControlWrite(fe, RFA_ENCLKRFAGC, 1);

		/* RSSI reference point */
		status += MXL_ControlWrite(fe, RFA_RSSI_REF, 2);
		status += MXL_ControlWrite(fe, RFA_RSSI_REFH, 4);
		status += MXL_ControlWrite(fe, RFA_RSSI_REFL, 1);

		/* TOP point */
		status += MXL_ControlWrite(fe, RFA_FLR, 2);
		status += MXL_ControlWrite(fe, RFA_CEIL, 13);
		status += MXL_ControlWrite(fe, BB_INITSTATE_DLPF_TUNE, 1);
		/* Low Zero */
		status += MXL_ControlWrite(fe, RFSYN_CHP_GAIN, 5);

		if (state->IF_OUT <= 6280000UL)	/* Low IF */
			status += MXL_ControlWrite(fe, BB_IQSWAP, 0);
		else /* High IF */
			status += MXL_ControlWrite(fe, BB_IQSWAP, 1);
	}
	if (state->Mod_Type == MXL_QAM) /* QAM Mode */ {
		state->Mode = MXL_DIGITAL_MODE;

		/* state->AGC_Mode = 1; */ /* Single AGC Mode */

		/* Disable RSSI */	/* change here for v2.6.5 */
		status += MXL_ControlWrite(fe, SEQ_EXTSYNTHCALIF, 1);
		status += MXL_ControlWrite(fe, SEQ_EXTDCCAL, 1);
		status += MXL_ControlWrite(fe, AGC_EN_RSSI, 0);
		status += MXL_ControlWrite(fe, RFA_ENCLKRFAGC, 1);

		/* RSSI reference point */
		status += MXL_ControlWrite(fe, RFA_RSSI_REFH, 5);
		status += MXL_ControlWrite(fe, RFA_RSSI_REF, 3);
		status += MXL_ControlWrite(fe, RFA_RSSI_REFL, 2);
		/* change here for v2.6.5 */
		status += MXL_ControlWrite(fe, RFSYN_CHP_GAIN, 3);

		if (state->IF_OUT <= 6280000UL)	/* Low IF */
			status += MXL_ControlWrite(fe, BB_IQSWAP, 0);
		else /* High IF */
			status += MXL_ControlWrite(fe, BB_IQSWAP, 1);
		status += MXL_ControlWrite(fe, RFSYN_CHP_GAIN, 2);

	}
	if (state->Mod_Type == MXL_ANALOG_CABLE) {
		/* Analog Cable Mode */
		/* state->Mode = MXL_DIGITAL_MODE; */

		state->AGC_Mode = 1; /* Single AGC Mode */

		/* Disable RSSI */
		status += MXL_ControlWrite(fe, SEQ_EXTSYNTHCALIF, 1);
		status += MXL_ControlWrite(fe, SEQ_EXTDCCAL, 1);
		status += MXL_ControlWrite(fe, AGC_EN_RSSI, 0);
		status += MXL_ControlWrite(fe, RFA_ENCLKRFAGC, 1);
		/* change for 2.6.3 */
		status += MXL_ControlWrite(fe, AGC_IF, 1);
		status += MXL_ControlWrite(fe, AGC_RF, 15);
		status += MXL_ControlWrite(fe, BB_IQSWAP, 1);
	}

	if (state->Mod_Type == MXL_ANALOG_OTA) {
		/* Analog OTA Terrestrial mode add for 2.6.7 */
		/* state->Mode = MXL_ANALOG_MODE; */

		/* Enable RSSI */
		status += MXL_ControlWrite(fe, SEQ_EXTSYNTHCALIF, 1);
		status += MXL_ControlWrite(fe, SEQ_EXTDCCAL, 1);
		status += MXL_ControlWrite(fe, AGC_EN_RSSI, 1);
		status += MXL_ControlWrite(fe, RFA_ENCLKRFAGC, 1);

		/* RSSI reference point */
		status += MXL_ControlWrite(fe, RFA_RSSI_REFH, 5);
		status += MXL_ControlWrite(fe, RFA_RSSI_REF, 3);
		status += MXL_ControlWrite(fe, RFA_RSSI_REFL, 2);
		status += MXL_ControlWrite(fe, RFSYN_CHP_GAIN, 3);
		status += MXL_ControlWrite(fe, BB_IQSWAP, 1);
	}

	/* RSSI disable */
	if (state->EN_RSSI == 0) {
		status += MXL_ControlWrite(fe, SEQ_EXTSYNTHCALIF, 1);
		status += MXL_ControlWrite(fe, SEQ_EXTDCCAL, 1);
		status += MXL_ControlWrite(fe, AGC_EN_RSSI, 0);
		status += MXL_ControlWrite(fe, RFA_ENCLKRFAGC, 1);
	}

	return status;
}

static u16 MXL_IFSynthInit(struct dvb_frontend *fe)
{
	struct mxl5005s_state *state = fe->tuner_priv;
	u16 status = 0 ;
	u32	Fref = 0 ;
	u32	Kdbl, intModVal ;
	u32	fracModVal ;
	Kdbl = 2 ;

	if (state->Fxtal >= 12000000UL && state->Fxtal <= 16000000UL)
		Kdbl = 2 ;
	if (state->Fxtal > 16000000UL && state->Fxtal <= 32000000UL)
		Kdbl = 1 ;

	/* IF Synthesizer Control */
	if (state->Mode == 0 && state->IF_Mode == 1) /* Analog Low IF mode */ {
		if (state->IF_LO == 41000000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x08);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x0C);
			Fref = 328000000UL ;
		}
		if (state->IF_LO == 47000000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x08);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 376000000UL ;
		}
		if (state->IF_LO == 54000000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x10);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x0C);
			Fref = 324000000UL ;
		}
		if (state->IF_LO == 60000000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x10);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 360000000UL ;
		}
		if (state->IF_LO == 39250000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x08);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x0C);
			Fref = 314000000UL ;
		}
		if (state->IF_LO == 39650000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x08);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x0C);
			Fref = 317200000UL ;
		}
		if (state->IF_LO == 40150000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x08);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x0C);
			Fref = 321200000UL ;
		}
		if (state->IF_LO == 40650000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x08);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x0C);
			Fref = 325200000UL ;
		}
	}

	if (state->Mode || (state->Mode == 0 && state->IF_Mode == 0)) {
		if (state->IF_LO == 57000000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x10);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 342000000UL ;
		}
		if (state->IF_LO == 44000000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x08);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 352000000UL ;
		}
		if (state->IF_LO == 43750000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x08);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 350000000UL ;
		}
		if (state->IF_LO == 36650000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x04);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 366500000UL ;
		}
		if (state->IF_LO == 36150000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x04);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 361500000UL ;
		}
		if (state->IF_LO == 36000000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x04);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 360000000UL ;
		}
		if (state->IF_LO == 35250000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x04);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 352500000UL ;
		}
		if (state->IF_LO == 34750000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x04);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 347500000UL ;
		}
		if (state->IF_LO == 6280000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x07);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 376800000UL ;
		}
		if (state->IF_LO == 5000000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x09);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 360000000UL ;
		}
		if (state->IF_LO == 4500000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x06);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 360000000UL ;
		}
		if (state->IF_LO == 4570000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x06);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 365600000UL ;
		}
		if (state->IF_LO == 4000000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x05);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 360000000UL ;
		}
		if (state->IF_LO == 57400000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x10);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 344400000UL ;
		}
		if (state->IF_LO == 44400000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x08);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 355200000UL ;
		}
		if (state->IF_LO == 44150000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x08);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 353200000UL ;
		}
		if (state->IF_LO == 37050000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x04);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 370500000UL ;
		}
		if (state->IF_LO == 36550000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x04);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 365500000UL ;
		}
		if (state->IF_LO == 36125000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x04);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 361250000UL ;
		}
		if (state->IF_LO == 6000000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x07);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 360000000UL ;
		}
		if (state->IF_LO == 5400000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x07);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x0C);
			Fref = 324000000UL ;
		}
		if (state->IF_LO == 5380000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x07);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x0C);
			Fref = 322800000UL ;
		}
		if (state->IF_LO == 5200000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x09);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 374400000UL ;
		}
		if (state->IF_LO == 4900000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x09);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 352800000UL ;
		}
		if (state->IF_LO == 4400000UL) {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x06);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 352000000UL ;
		}
		if (state->IF_LO == 4063000UL)  /* add for 2.6.8 */ {
			status += MXL_ControlWrite(fe, IF_DIVVAL,   0x05);
			status += MXL_ControlWrite(fe, IF_VCO_BIAS, 0x08);
			Fref = 365670000UL ;
		}
	}
	/* CHCAL_INT_MOD_IF */
	/* CHCAL_FRAC_MOD_IF */
	intModVal = Fref / (state->Fxtal * Kdbl/2);
	status += MXL_ControlWrite(fe, CHCAL_INT_MOD_IF, intModVal);

	fracModVal = (2<<15)*(Fref/1000 - (state->Fxtal/1000 * Kdbl/2) *
		intModVal);

	fracModVal = fracModVal / ((state->Fxtal * Kdbl/2)/1000);
	status += MXL_ControlWrite(fe, CHCAL_FRAC_MOD_IF, fracModVal);

	return status ;
}

static u32 MXL_GetXtalInt(u32 Xtal_Freq)
{
	if ((Xtal_Freq % 1000000) == 0)
		return (Xtal_Freq / 10000);
	else
		return (((Xtal_Freq / 1000000) + 1)*100);
}

static u16 MXL_TuneRF(struct dvb_frontend *fe, u32 RF_Freq)
{
	struct mxl5005s_state *state = fe->tuner_priv;
	u16 status = 0;
	u32 divider_val, E3, E4, E5, E5A;
	u32 Fmax, Fmin, FmaxBin, FminBin;
	u32 Kdbl_RF = 2;
	u32 tg_divval;
	u32 tg_lo;
	u32 Xtal_Int;

	u32 Fref_TG;
	u32 Fvco;

	Xtal_Int = MXL_GetXtalInt(state->Fxtal);

	state->RF_IN = RF_Freq;

	MXL_SynthRFTGLO_Calc(fe);

	if (state->Fxtal >= 12000000UL && state->Fxtal <= 22000000UL)
		Kdbl_RF = 2;
	if (state->Fxtal > 22000000 && state->Fxtal <= 32000000)
		Kdbl_RF = 1;

	/* Downconverter Controls
	 * Look-Up Table Implementation for:
	 *	DN_POLY
	 *	DN_RFGAIN
	 *	DN_CAP_RFLPF
	 *	DN_EN_VHFUHFBAR
	 *	DN_GAIN_ADJUST
	 *  Change the boundary reference from RF_IN to RF_LO
	 */
	if (state->RF_LO < 40000000UL)
		return -1;

	if (state->RF_LO >= 40000000UL && state->RF_LO <= 75000000UL) {
		status += MXL_ControlWrite(fe, DN_POLY,              2);
		status += MXL_ControlWrite(fe, DN_RFGAIN,            3);
		status += MXL_ControlWrite(fe, DN_CAP_RFLPF,         423);
		status += MXL_ControlWrite(fe, DN_EN_VHFUHFBAR,      1);
		status += MXL_ControlWrite(fe, DN_GAIN_ADJUST,       1);
	}
	if (state->RF_LO > 75000000UL && state->RF_LO <= 100000000UL) {
		status += MXL_ControlWrite(fe, DN_POLY,              3);
		status += MXL_ControlWrite(fe, DN_RFGAIN,            3);
		status += MXL_ControlWrite(fe, DN_CAP_RFLPF,         222);
		status += MXL_ControlWrite(fe, DN_EN_VHFUHFBAR,      1);
		status += MXL_ControlWrite(fe, DN_GAIN_ADJUST,       1);
	}
	if (state->RF_LO > 100000000UL && state->RF_LO <= 150000000UL) {
		status += MXL_ControlWrite(fe, DN_POLY,              3);
		status += MXL_ControlWrite(fe, DN_RFGAIN,            3);
		status += MXL_ControlWrite(fe, DN_CAP_RFLPF,         147);
		status += MXL_ControlWrite(fe, DN_EN_VHFUHFBAR,      1);
		status += MXL_ControlWrite(fe, DN_GAIN_ADJUST,       2);
	}
	if (state->RF_LO > 150000000UL && state->RF_LO <= 200000000UL) {
		status += MXL_ControlWrite(fe, DN_POLY,              3);
		status += MXL_ControlWrite(fe, DN_RFGAIN,            3);
		status += MXL_ControlWrite(fe, DN_CAP_RFLPF,         9);
		status += MXL_ControlWrite(fe, DN_EN_VHFUHFBAR,      1);
		status += MXL_ControlWrite(fe, DN_GAIN_ADJUST,       2);
	}
	if (state->RF_LO > 200000000UL && state->RF_LO <= 300000000UL) {
		status += MXL_ControlWrite(fe, DN_POLY,              3);
		status += MXL_ControlWrite(fe, DN_RFGAIN,            3);
		status += MXL_ControlWrite(fe, DN_CAP_RFLPF,         0);
		status += MXL_ControlWrite(fe, DN_EN_VHFUHFBAR,      1);
		status += MXL_ControlWrite(fe, DN_GAIN_ADJUST,       3);
	}
	if (state->RF_LO > 300000000UL && state->RF_LO <= 650000000UL) {
		status += MXL_ControlWrite(fe, DN_POLY,              3);
		status += MXL_ControlWrite(fe, DN_RFGAIN,            1);
		status += MXL_ControlWrite(fe, DN_CAP_RFLPF,         0);
		status += MXL_ControlWrite(fe, DN_EN_VHFUHFBAR,      0);
		status += MXL_ControlWrite(fe, DN_GAIN_ADJUST,       3);
	}
	if (state->RF_LO > 650000000UL && state->RF_LO <= 900000000UL) {
		status += MXL_ControlWrite(fe, DN_POLY,              3);
		status += MXL_ControlWrite(fe, DN_RFGAIN,            2);
		status += MXL_ControlWrite(fe, DN_CAP_RFLPF,         0);
		status += MXL_ControlWrite(fe, DN_EN_VHFUHFBAR,      0);
		status += MXL_ControlWrite(fe, DN_GAIN_ADJUST,       3);
	}
	if (state->RF_LO > 900000000UL)
		return -1;

	/*	DN_IQTNBUF_AMP */
	/*	DN_IQTNGNBFBIAS_BST */
	if (state->RF_LO >= 40000000UL && state->RF_LO <= 75000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       1);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  0);
	}
	if (state->RF_LO > 75000000UL && state->RF_LO <= 100000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       1);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  0);
	}
	if (state->RF_LO > 100000000UL && state->RF_LO <= 150000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       1);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  0);
	}
	if (state->RF_LO > 150000000UL && state->RF_LO <= 200000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       1);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  0);
	}
	if (state->RF_LO > 200000000UL && state->RF_LO <= 300000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       1);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  0);
	}
	if (state->RF_LO > 300000000UL && state->RF_LO <= 400000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       1);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  0);
	}
	if (state->RF_LO > 400000000UL && state->RF_LO <= 450000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       1);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  0);
	}
	if (state->RF_LO > 450000000UL && state->RF_LO <= 500000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       1);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  0);
	}
	if (state->RF_LO > 500000000UL && state->RF_LO <= 550000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       1);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  0);
	}
	if (state->RF_LO > 550000000UL && state->RF_LO <= 600000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       1);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  0);
	}
	if (state->RF_LO > 600000000UL && state->RF_LO <= 650000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       1);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  0);
	}
	if (state->RF_LO > 650000000UL && state->RF_LO <= 700000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       1);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  0);
	}
	if (state->RF_LO > 700000000UL && state->RF_LO <= 750000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       1);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  0);
	}
	if (state->RF_LO > 750000000UL && state->RF_LO <= 800000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       1);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  0);
	}
	if (state->RF_LO > 800000000UL && state->RF_LO <= 850000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       10);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  1);
	}
	if (state->RF_LO > 850000000UL && state->RF_LO <= 900000000UL) {
		status += MXL_ControlWrite(fe, DN_IQTNBUF_AMP,       10);
		status += MXL_ControlWrite(fe, DN_IQTNGNBFBIAS_BST,  1);
	}

	/*
	 * Set RF Synth and LO Path Control
	 *
	 * Look-Up table implementation for:
	 *	RFSYN_EN_OUTMUX
	 *	RFSYN_SEL_VCO_OUT
	 *	RFSYN_SEL_VCO_HI
	 *  RFSYN_SEL_DIVM
	 *	RFSYN_RF_DIV_BIAS
	 *	DN_SEL_FREQ
	 *
	 * Set divider_val, Fmax, Fmix to use in Equations
	 */
	FminBin = 28000000UL ;
	FmaxBin = 42500000UL ;
	if (state->RF_LO >= 40000000UL && state->RF_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, RFSYN_EN_OUTMUX,     1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_OUT,   0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_HI,    0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_DIVM,      0);
		status += MXL_ControlWrite(fe, RFSYN_RF_DIV_BIAS,   1);
		status += MXL_ControlWrite(fe, DN_SEL_FREQ,         1);
		divider_val = 64 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 42500000UL ;
	FmaxBin = 56000000UL ;
	if (state->RF_LO > FminBin && state->RF_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, RFSYN_EN_OUTMUX,     1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_OUT,   0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_HI,    1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_DIVM,      0);
		status += MXL_ControlWrite(fe, RFSYN_RF_DIV_BIAS,   1);
		status += MXL_ControlWrite(fe, DN_SEL_FREQ,         1);
		divider_val = 64 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 56000000UL ;
	FmaxBin = 85000000UL ;
	if (state->RF_LO > FminBin && state->RF_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, RFSYN_EN_OUTMUX,     0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_OUT,   1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_HI,    0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_DIVM,      0);
		status += MXL_ControlWrite(fe, RFSYN_RF_DIV_BIAS,   1);
		status += MXL_ControlWrite(fe, DN_SEL_FREQ,         1);
		divider_val = 32 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 85000000UL ;
	FmaxBin = 112000000UL ;
	if (state->RF_LO > FminBin && state->RF_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, RFSYN_EN_OUTMUX,     0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_OUT,   1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_HI,    1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_DIVM,      0);
		status += MXL_ControlWrite(fe, RFSYN_RF_DIV_BIAS,   1);
		status += MXL_ControlWrite(fe, DN_SEL_FREQ,         1);
		divider_val = 32 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 112000000UL ;
	FmaxBin = 170000000UL ;
	if (state->RF_LO > FminBin && state->RF_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, RFSYN_EN_OUTMUX,     0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_OUT,   1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_HI,    0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_DIVM,      0);
		status += MXL_ControlWrite(fe, RFSYN_RF_DIV_BIAS,   1);
		status += MXL_ControlWrite(fe, DN_SEL_FREQ,         2);
		divider_val = 16 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 170000000UL ;
	FmaxBin = 225000000UL ;
	if (state->RF_LO > FminBin && state->RF_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, RFSYN_EN_OUTMUX,     0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_OUT,   1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_HI,    1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_DIVM,      0);
		status += MXL_ControlWrite(fe, RFSYN_RF_DIV_BIAS,   1);
		status += MXL_ControlWrite(fe, DN_SEL_FREQ,         2);
		divider_val = 16 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 225000000UL ;
	FmaxBin = 300000000UL ;
	if (state->RF_LO > FminBin && state->RF_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, RFSYN_EN_OUTMUX,     0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_OUT,   1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_HI,    0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_DIVM,      0);
		status += MXL_ControlWrite(fe, RFSYN_RF_DIV_BIAS,   1);
		status += MXL_ControlWrite(fe, DN_SEL_FREQ,         4);
		divider_val = 8 ;
		Fmax = 340000000UL ;
		Fmin = FminBin ;
	}
	FminBin = 300000000UL ;
	FmaxBin = 340000000UL ;
	if (state->RF_LO > FminBin && state->RF_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, RFSYN_EN_OUTMUX,     1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_OUT,   0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_HI,    0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_DIVM,      0);
		status += MXL_ControlWrite(fe, RFSYN_RF_DIV_BIAS,   1);
		status += MXL_ControlWrite(fe, DN_SEL_FREQ,         0);
		divider_val = 8 ;
		Fmax = FmaxBin ;
		Fmin = 225000000UL ;
	}
	FminBin = 340000000UL ;
	FmaxBin = 450000000UL ;
	if (state->RF_LO > FminBin && state->RF_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, RFSYN_EN_OUTMUX,     1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_OUT,   0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_HI,    1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_DIVM,      0);
		status += MXL_ControlWrite(fe, RFSYN_RF_DIV_BIAS,   2);
		status += MXL_ControlWrite(fe, DN_SEL_FREQ,         0);
		divider_val = 8 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 450000000UL ;
	FmaxBin = 680000000UL ;
	if (state->RF_LO > FminBin && state->RF_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, RFSYN_EN_OUTMUX,     0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_OUT,   1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_HI,    0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_DIVM,      1);
		status += MXL_ControlWrite(fe, RFSYN_RF_DIV_BIAS,   1);
		status += MXL_ControlWrite(fe, DN_SEL_FREQ,         0);
		divider_val = 4 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 680000000UL ;
	FmaxBin = 900000000UL ;
	if (state->RF_LO > FminBin && state->RF_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, RFSYN_EN_OUTMUX,     0);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_OUT,   1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_VCO_HI,    1);
		status += MXL_ControlWrite(fe, RFSYN_SEL_DIVM,      1);
		status += MXL_ControlWrite(fe, RFSYN_RF_DIV_BIAS,   1);
		status += MXL_ControlWrite(fe, DN_SEL_FREQ,         0);
		divider_val = 4 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}

	/*	CHCAL_INT_MOD_RF
	 *	CHCAL_FRAC_MOD_RF
	 *	RFSYN_LPF_R
	 *	CHCAL_EN_INT_RF
	 */
	/* Equation E3 RFSYN_VCO_BIAS */
	E3 = (((Fmax-state->RF_LO)/1000)*32)/((Fmax-Fmin)/1000) + 8 ;
	status += MXL_ControlWrite(fe, RFSYN_VCO_BIAS, E3);

	/* Equation E4 CHCAL_INT_MOD_RF */
	E4 = (state->RF_LO*divider_val/1000)/(2*state->Fxtal*Kdbl_RF/1000);
	MXL_ControlWrite(fe, CHCAL_INT_MOD_RF, E4);

	/* Equation E5 CHCAL_FRAC_MOD_RF CHCAL_EN_INT_RF */
	E5 = ((2<<17)*(state->RF_LO/10000*divider_val -
		(E4*(2*state->Fxtal*Kdbl_RF)/10000))) /
		(2*state->Fxtal*Kdbl_RF/10000);

	status += MXL_ControlWrite(fe, CHCAL_FRAC_MOD_RF, E5);

	/* Equation E5A RFSYN_LPF_R */
	E5A = (((Fmax - state->RF_LO)/1000)*4/((Fmax-Fmin)/1000)) + 1 ;
	status += MXL_ControlWrite(fe, RFSYN_LPF_R, E5A);

	/* Euqation E5B CHCAL_EN_INIT_RF */
	status += MXL_ControlWrite(fe, CHCAL_EN_INT_RF, ((E5 == 0) ? 1 : 0));
	/*if (E5 == 0)
	 *	status += MXL_ControlWrite(fe, CHCAL_EN_INT_RF, 1);
	 *else
	 *	status += MXL_ControlWrite(fe, CHCAL_FRAC_MOD_RF, E5);
	 */

	/*
	 * Set TG Synth
	 *
	 * Look-Up table implementation for:
	 *	TG_LO_DIVVAL
	 *	TG_LO_SELVAL
	 *
	 * Set divider_val, Fmax, Fmix to use in Equations
	 */
	if (state->TG_LO < 33000000UL)
		return -1;

	FminBin = 33000000UL ;
	FmaxBin = 50000000UL ;
	if (state->TG_LO >= FminBin && state->TG_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, TG_LO_DIVVAL,	0x6);
		status += MXL_ControlWrite(fe, TG_LO_SELVAL,	0x0);
		divider_val = 36 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 50000000UL ;
	FmaxBin = 67000000UL ;
	if (state->TG_LO > FminBin && state->TG_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, TG_LO_DIVVAL,	0x1);
		status += MXL_ControlWrite(fe, TG_LO_SELVAL,	0x0);
		divider_val = 24 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 67000000UL ;
	FmaxBin = 100000000UL ;
	if (state->TG_LO > FminBin && state->TG_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, TG_LO_DIVVAL,	0xC);
		status += MXL_ControlWrite(fe, TG_LO_SELVAL,	0x2);
		divider_val = 18 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 100000000UL ;
	FmaxBin = 150000000UL ;
	if (state->TG_LO > FminBin && state->TG_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, TG_LO_DIVVAL,	0x8);
		status += MXL_ControlWrite(fe, TG_LO_SELVAL,	0x2);
		divider_val = 12 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 150000000UL ;
	FmaxBin = 200000000UL ;
	if (state->TG_LO > FminBin && state->TG_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, TG_LO_DIVVAL,	0x0);
		status += MXL_ControlWrite(fe, TG_LO_SELVAL,	0x2);
		divider_val = 8 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 200000000UL ;
	FmaxBin = 300000000UL ;
	if (state->TG_LO > FminBin && state->TG_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, TG_LO_DIVVAL,	0x8);
		status += MXL_ControlWrite(fe, TG_LO_SELVAL,	0x3);
		divider_val = 6 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 300000000UL ;
	FmaxBin = 400000000UL ;
	if (state->TG_LO > FminBin && state->TG_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, TG_LO_DIVVAL,	0x0);
		status += MXL_ControlWrite(fe, TG_LO_SELVAL,	0x3);
		divider_val = 4 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 400000000UL ;
	FmaxBin = 600000000UL ;
	if (state->TG_LO > FminBin && state->TG_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, TG_LO_DIVVAL,	0x8);
		status += MXL_ControlWrite(fe, TG_LO_SELVAL,	0x7);
		divider_val = 3 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}
	FminBin = 600000000UL ;
	FmaxBin = 900000000UL ;
	if (state->TG_LO > FminBin && state->TG_LO <= FmaxBin) {
		status += MXL_ControlWrite(fe, TG_LO_DIVVAL,	0x0);
		status += MXL_ControlWrite(fe, TG_LO_SELVAL,	0x7);
		divider_val = 2 ;
		Fmax = FmaxBin ;
		Fmin = FminBin ;
	}

	/* TG_DIV_VAL */
	tg_divval = (state->TG_LO*divider_val/100000) *
		(MXL_Ceiling(state->Fxtal, 1000000) * 100) /
		(state->Fxtal/1000);

	status += MXL_ControlWrite(fe, TG_DIV_VAL, tg_divval);

	if (state->TG_LO > 600000000UL)
		status += MXL_ControlWrite(fe, TG_DIV_VAL, tg_divval + 1);

	Fmax = 1800000000UL ;
	Fmin = 1200000000UL ;

	/* prevent overflow of 32 bit unsigned integer, use
	 * following equation. Edit for v2.6.4
	 */
	/* Fref_TF = Fref_TG * 1000 */
	Fref_TG = (state->Fxtal/1000) / MXL_Ceiling(state->Fxtal, 1000000);

	/* Fvco = Fvco/10 */
	Fvco = (state->TG_LO/10000) * divider_val * Fref_TG;

	tg_lo = (((Fmax/10 - Fvco)/100)*32) / ((Fmax-Fmin)/1000)+8;

	/* below equation is same as above but much harder to debug.
	 * tg_lo = ( ((Fmax/10000 * Xtal_Int)/100) -
	 * ((state->TG_LO/10000)*divider_val *
	 * (state->Fxtal/10000)/100) )*32/((Fmax-Fmin)/10000 *
	 * Xtal_Int/100) + 8;
	 */

	status += MXL_ControlWrite(fe, TG_VCO_BIAS , tg_lo);

	/* add for 2.6.5 Special setting for QAM */
	if (state->Mod_Type == MXL_QAM) {
		if (state->RF_IN < 680000000)
			status += MXL_ControlWrite(fe, RFSYN_CHP_GAIN, 3);
		else
			status += MXL_ControlWrite(fe, RFSYN_CHP_GAIN, 2);
	}

	/* Off Chip Tracking Filter Control */
	if (state->TF_Type == MXL_TF_OFF) {
		/* Tracking Filter Off State; turn off all the banks */
		status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
		status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
		status += MXL_SetGPIO(fe, 3, 1); /* Bank1 Off */
		status += MXL_SetGPIO(fe, 1, 1); /* Bank2 Off */
		status += MXL_SetGPIO(fe, 4, 1); /* Bank3 Off */
	}

	if (state->TF_Type == MXL_TF_C) /* Tracking Filter type C */ {
		status += MXL_ControlWrite(fe, DAC_B_ENABLE, 1);
		status += MXL_ControlWrite(fe, DAC_DIN_A, 0);

		if (state->RF_IN >= 43000000 && state->RF_IN < 150000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
			status += MXL_ControlWrite(fe, DAC_DIN_B, 0);
			status += MXL_SetGPIO(fe, 3, 0);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 4, 1);
		}
		if (state->RF_IN >= 150000000 && state->RF_IN < 280000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
			status += MXL_ControlWrite(fe, DAC_DIN_B, 0);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 0);
			status += MXL_SetGPIO(fe, 4, 1);
		}
		if (state->RF_IN >= 280000000 && state->RF_IN < 360000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
			status += MXL_ControlWrite(fe, DAC_DIN_B, 0);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 0);
			status += MXL_SetGPIO(fe, 4, 0);
		}
		if (state->RF_IN >= 360000000 && state->RF_IN < 560000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
			status += MXL_ControlWrite(fe, DAC_DIN_B, 0);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 4, 0);
		}
		if (state->RF_IN >= 560000000 && state->RF_IN < 580000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 1);
			status += MXL_ControlWrite(fe, DAC_DIN_B, 29);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 4, 0);
		}
		if (state->RF_IN >= 580000000 && state->RF_IN < 630000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 1);
			status += MXL_ControlWrite(fe, DAC_DIN_B, 0);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 4, 0);
		}
		if (state->RF_IN >= 630000000 && state->RF_IN < 700000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 1);
			status += MXL_ControlWrite(fe, DAC_DIN_B, 16);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 4, 1);
		}
		if (state->RF_IN >= 700000000 && state->RF_IN < 760000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 1);
			status += MXL_ControlWrite(fe, DAC_DIN_B, 7);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 4, 1);
		}
		if (state->RF_IN >= 760000000 && state->RF_IN <= 900000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 1);
			status += MXL_ControlWrite(fe, DAC_DIN_B, 0);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 4, 1);
		}
	}

	if (state->TF_Type == MXL_TF_C_H) {

		/* Tracking Filter type C-H for Hauppauge only */
		status += MXL_ControlWrite(fe, DAC_DIN_A, 0);

		if (state->RF_IN >= 43000000 && state->RF_IN < 150000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 0);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 1);
		}
		if (state->RF_IN >= 150000000 && state->RF_IN < 280000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 3, 0);
			status += MXL_SetGPIO(fe, 1, 1);
		}
		if (state->RF_IN >= 280000000 && state->RF_IN < 360000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 3, 0);
			status += MXL_SetGPIO(fe, 1, 0);
		}
		if (state->RF_IN >= 360000000 && state->RF_IN < 560000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 0);
		}
		if (state->RF_IN >= 560000000 && state->RF_IN < 580000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 1);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 0);
		}
		if (state->RF_IN >= 580000000 && state->RF_IN < 630000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 1);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 0);
		}
		if (state->RF_IN >= 630000000 && state->RF_IN < 700000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 1);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 1);
		}
		if (state->RF_IN >= 700000000 && state->RF_IN < 760000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 1);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 1);
		}
		if (state->RF_IN >= 760000000 && state->RF_IN <= 900000000) {
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 1);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 1);
		}
	}

	if (state->TF_Type == MXL_TF_D) { /* Tracking Filter type D */

		status += MXL_ControlWrite(fe, DAC_DIN_B, 0);

		if (state->RF_IN >= 43000000 && state->RF_IN < 174000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 0);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 3, 1);
		}
		if (state->RF_IN >= 174000000 && state->RF_IN < 250000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 0);
			status += MXL_SetGPIO(fe, 1, 0);
			status += MXL_SetGPIO(fe, 3, 1);
		}
		if (state->RF_IN >= 250000000 && state->RF_IN < 310000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 0);
			status += MXL_SetGPIO(fe, 3, 1);
		}
		if (state->RF_IN >= 310000000 && state->RF_IN < 360000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 0);
			status += MXL_SetGPIO(fe, 3, 0);
		}
		if (state->RF_IN >= 360000000 && state->RF_IN < 470000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 3, 0);
		}
		if (state->RF_IN >= 470000000 && state->RF_IN < 640000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 1);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 3, 0);
		}
		if (state->RF_IN >= 640000000 && state->RF_IN <= 900000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 1);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 3, 1);
		}
	}

	if (state->TF_Type == MXL_TF_D_L) {

		/* Tracking Filter type D-L for Lumanate ONLY change 2.6.3 */
		status += MXL_ControlWrite(fe, DAC_DIN_A, 0);

		/* if UHF and terrestrial => Turn off Tracking Filter */
		if (state->RF_IN >= 471000000 &&
			(state->RF_IN - 471000000)%6000000 != 0) {
			/* Turn off all the banks */
			status += MXL_SetGPIO(fe, 3, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
			status += MXL_ControlWrite(fe, AGC_IF, 10);
		} else {
			/* if VHF or cable => Turn on Tracking Filter */
			if (state->RF_IN >= 43000000 &&
				state->RF_IN < 140000000) {

				status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
				status += MXL_SetGPIO(fe, 4, 1);
				status += MXL_SetGPIO(fe, 1, 1);
				status += MXL_SetGPIO(fe, 3, 0);
			}
			if (state->RF_IN >= 140000000 &&
				state->RF_IN < 240000000) {
				status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
				status += MXL_SetGPIO(fe, 4, 1);
				status += MXL_SetGPIO(fe, 1, 0);
				status += MXL_SetGPIO(fe, 3, 0);
			}
			if (state->RF_IN >= 240000000 &&
				state->RF_IN < 340000000) {
				status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
				status += MXL_SetGPIO(fe, 4, 0);
				status += MXL_SetGPIO(fe, 1, 1);
				status += MXL_SetGPIO(fe, 3, 0);
			}
			if (state->RF_IN >= 340000000 &&
				state->RF_IN < 430000000) {
				status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
				status += MXL_SetGPIO(fe, 4, 0);
				status += MXL_SetGPIO(fe, 1, 0);
				status += MXL_SetGPIO(fe, 3, 1);
			}
			if (state->RF_IN >= 430000000 &&
				state->RF_IN < 470000000) {
				status += MXL_ControlWrite(fe, DAC_A_ENABLE, 1);
				status += MXL_SetGPIO(fe, 4, 1);
				status += MXL_SetGPIO(fe, 1, 0);
				status += MXL_SetGPIO(fe, 3, 1);
			}
			if (state->RF_IN >= 470000000 &&
				state->RF_IN < 570000000) {
				status += MXL_ControlWrite(fe, DAC_A_ENABLE, 1);
				status += MXL_SetGPIO(fe, 4, 0);
				status += MXL_SetGPIO(fe, 1, 0);
				status += MXL_SetGPIO(fe, 3, 1);
			}
			if (state->RF_IN >= 570000000 &&
				state->RF_IN < 620000000) {
				status += MXL_ControlWrite(fe, DAC_A_ENABLE, 0);
				status += MXL_SetGPIO(fe, 4, 0);
				status += MXL_SetGPIO(fe, 1, 1);
				status += MXL_SetGPIO(fe, 3, 1);
			}
			if (state->RF_IN >= 620000000 &&
				state->RF_IN < 760000000) {
				status += MXL_ControlWrite(fe, DAC_A_ENABLE, 1);
				status += MXL_SetGPIO(fe, 4, 0);
				status += MXL_SetGPIO(fe, 1, 1);
				status += MXL_SetGPIO(fe, 3, 1);
			}
			if (state->RF_IN >= 760000000 &&
				state->RF_IN <= 900000000) {
				status += MXL_ControlWrite(fe, DAC_A_ENABLE, 1);
				status += MXL_SetGPIO(fe, 4, 1);
				status += MXL_SetGPIO(fe, 1, 1);
				status += MXL_SetGPIO(fe, 3, 1);
			}
		}
	}

	if (state->TF_Type == MXL_TF_E) /* Tracking Filter type E */ {

		status += MXL_ControlWrite(fe, DAC_DIN_B, 0);

		if (state->RF_IN >= 43000000 && state->RF_IN < 174000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 0);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 3, 1);
		}
		if (state->RF_IN >= 174000000 && state->RF_IN < 250000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 0);
			status += MXL_SetGPIO(fe, 1, 0);
			status += MXL_SetGPIO(fe, 3, 1);
		}
		if (state->RF_IN >= 250000000 && state->RF_IN < 310000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 0);
			status += MXL_SetGPIO(fe, 3, 1);
		}
		if (state->RF_IN >= 310000000 && state->RF_IN < 360000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 0);
			status += MXL_SetGPIO(fe, 3, 0);
		}
		if (state->RF_IN >= 360000000 && state->RF_IN < 470000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 3, 0);
		}
		if (state->RF_IN >= 470000000 && state->RF_IN < 640000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 1);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 3, 0);
		}
		if (state->RF_IN >= 640000000 && state->RF_IN <= 900000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 1);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 3, 1);
		}
	}

	if (state->TF_Type == MXL_TF_F) {

		/* Tracking Filter type F */
		status += MXL_ControlWrite(fe, DAC_DIN_B, 0);

		if (state->RF_IN >= 43000000 && state->RF_IN < 160000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 0);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 3, 1);
		}
		if (state->RF_IN >= 160000000 && state->RF_IN < 210000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 0);
			status += MXL_SetGPIO(fe, 1, 0);
			status += MXL_SetGPIO(fe, 3, 1);
		}
		if (state->RF_IN >= 210000000 && state->RF_IN < 300000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 0);
			status += MXL_SetGPIO(fe, 3, 1);
		}
		if (state->RF_IN >= 300000000 && state->RF_IN < 390000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 0);
			status += MXL_SetGPIO(fe, 3, 0);
		}
		if (state->RF_IN >= 390000000 && state->RF_IN < 515000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 3, 0);
		}
		if (state->RF_IN >= 515000000 && state->RF_IN < 650000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 1);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 3, 0);
		}
		if (state->RF_IN >= 650000000 && state->RF_IN <= 900000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 1);
			status += MXL_SetGPIO(fe, 4, 1);
			status += MXL_SetGPIO(fe, 1, 1);
			status += MXL_SetGPIO(fe, 3, 1);
		}
	}

	if (state->TF_Type == MXL_TF_E_2) {

		/* Tracking Filter type E_2 */
		status += MXL_ControlWrite(fe, DAC_DIN_B, 0);

		if (state->RF_IN >= 43000000 && state->RF_IN < 174000000) {
			status += MXL_ControlWrite(fe, DAC_B_ENABLE, 0);
			status += MXL_SetGPIO(fe, 4, 0);
			status += /*
    MaxLinear1, 15005S VSB/QAM/DVBT tuner driver
3    Copy}
		if (VSB/e->RF_IN >= 1740oth@l && 006 Steven To< 25oth@li0) {05S VSB/QAM/DVBT tControlWriteinearDAC_B_ENABLEMXL5005S VSB/QAM/DVBT tuner driver
 MXL5005S VSB/QAM/DVBT tuner driver

  L5005S VSB/QAM/DVBT tuner driver
  Copyright (C) 2006 Steven Toth nctions:
nuxtv.org>
      Fu3ctions:
	mxl5005s_reset()
	mxl5005s_writereg()
	mxl5005s_writeregs()
	mxl5005s_init()
	mxl5005s_  Copyright (C) 2008 MaxLinear
  erMode()
	mxl5005s_set_params()
	mxl5005s_get_frequency()
	mxl5005s_gease()
	mnuxtv.org>
      Futoth@li	mxl5005s_attach()

    Copyright (C) 2008 Realtek
    Copyright (C) 2008 Jan Hoogenraad
      Functions:
	mxl5005s_SetRfFreqHz()

    This program is free softwaL5005Sht (C) 2006 Steven Toth  terms ofnuxtv.org>
      Fu57erms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (a  Copyright (C) 2008 MaxLinear
    C   This program is distributed useful,
nuxtv.org>
      Fu7useful,
    but WITHOUT ANY WARRANTY; without even the impli  Copyright (C) 2008 MaxLinear
  Y or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You of the Gnuxtv.org>
      F= 9terms of the GNU General Public License as published by
    m; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/*
opyright }

(C) 2006 SteTF_Type =DVBT tTF_G	mxl
		/* Tracking Filter tdidnG add for v2.6.8 */
005s_reset()
	mxl5005s_writereg()
	mxDIN_Bcense  more details.

    You hould have received a copy1 a publithe Lin05s_reset()
	mxl5005s_writereg()
	mxl5005s_writeregs()
	mxl5005s_init()
	mxl5005s_reconfigure()
	mxl5005s_AssignTunerM  Copyright (C) 2008 MaxLinear
    Copyright (C) 2006 Steven Toth < a publinuxtv.org>
      Fun8tions:
	mxl5005s_reset()
	mxl5005s_writereg()
	mxl5005s_writeregs()
	mxl5005s_init()
	mxl5005s_reconfigure()
	mxl5005s_AssignTunerMode()
	mxl5005s_set_params()
	mxl5005s_get_frequency()
	mxl5005s_geearly madth()
	mxl5005s_release()
	mxl5005s_attach()

    Copyright (C) 2008 Realtek
    Copyright (C) 2008 Jan Hoogenraad
      Functions:
	mxl5005s_SetRfFreqHz()

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be4of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Aveode()
	mxl5005s_set_params()
	mxl5005s_get_frequency()
	mxl5005s_g/* Enumernuxtv.org>
      Fu6 terms o    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    YouON = 0,
	nuxtv.org>
      Fu82f the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/*
    History of this driver (Stg to be w):
      I was given a public release of a linux driver that included
      support for the MaxLinear MXL5005S silicon tuner. Analysis of
      the tuner driver showed clearly three things.

      1. The tuner driver didn't supportE_NAthe LinuxTV tuner API
	 so the E-NARealtEmpia ONLY changeRealtk added had to be removed.

      2. A significant amount of/* if UHF and terrestrial=> Turn offTV tuner API
	 sod had Modulation Type */
en1oth@linuxt it2006 Steven To-16 */
	I_D)%6oth@lin!= I felt ituxTV        all the banksES_SEL	mxl5005s_set_params()
	mxl5005s_get VSB/QAM/DVBT tuner driver

    Copyright (C) 2008 MaxLinear
  d
      Functions:
	mxl5l5005s_writereg()
	mxl5005s_writere       k ad12     /*n RSSI                   /*l5005s_writereg()SEQ_EXTSYNTHCALIF_ENCLK16_CLK_OUT,       /* 22 */
	SEQ_SE       DCCAL_ENCLK16_CLK_OUT,       /* 22 */
	SEQ_SEAGC_EN_ELEC_ENCLK16_CLK_OUT,       /* 22 */
	SEQ_SERFA_ENCLKRFAGC	 Linu       ELECTreference pointT,            /* 24 */
	IF_SEL_DBL,    /* ELEC_REFH, 5 /* 28 */
	AGC_EN_RSSI,               /* RSSI_REF, 3           /* 32 */
	RFA_RSSI_REFL,             /L, 2CLKRFAGC, follower Aparame	 sois from analog OTA mode,     * can be 12 */
	to seek bet	 soperforma/* 3RFA_RSSI_REFH,             /* 31 */
	RFSYN_CHP_GAIN* 33 */
} elsemxl50/* 14 VHF or Cable => XTAL_CAPS/* 15 */
	DRV_RES_S9 */
	3 */
	XTAL_CAffSELECT,             /* 27 */
	SEQ_EXTDCCAL,                   /* 12 */
	backXTIQFSMbove conditioned had to be removed.

      2. A sigG_R_DIV,            t of the driver is refer43ce driver code
	 from MaxLstoth@li felt it was important to identify and
	 preserve this.

      3. New code has to be added to interface correctly with the
	 LinuxTV API, as a regular kernel module.

      Other than the refestoth@linuxtv.org>
      Functions:
	mxl5005s_reset()
	mxl5005s_writereg()
	mxl5005s_writeregs()
	mxl5005s_init()
	mxl5005s_reconfigure()
	mxl5005s_AssignTunerMode()
	mxl5005s_set_params()
	mxl5005s_get_frequency()
	mxl5005s_get_bandwidth()
	mxl5005s_release()
	mxl5005s_attach()

    Copyright (C) 2008 Realtek
    Copyright (C) 2008 Jan Hoogenraad
      Functions:
	mxl5005s_SetRfFreqHz()

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/*
    History of this driver (Steven Toth):
      I was given a public release of a linux driver that included
      support for the MaxLinear MXL5005S silicon tuner. Analysis of
      the tuner driver showed clearly three things.

      1   1. 	ret    VSB/QAM;
}

VSB/ic u16VBT tuner dristruct dvb_frontend *nearu8 MaxL_Num5S_BB_IQSWVal)
{
	L500VSB/QAM= 0LKRFC) 2_IQSWAP_n't 1)_POLY = 51,              /* 51 */
_IQSW1BB		4
#dVal ? 0 :ENCLKRF/*		4
#2Q_EXnot availB,   nnel NDSEL_ADDR		53
#3	mxl50NDSEL_ADD50053
#demxl5005s_reset()
	mxl5005s_writereg()_IQSWicense foSTANDARD_ATSC,
};
#define MXL5005S_STA amounthis prograRD_DVBT,
	MX	MXL_DVBT,
	MXL_ATSC,
	MXL_QAM,
	MXL_AN5S_STANDNCLK16_CLK_OUT,       /* 22 */
	SEQ_SE
enum {
	opyright (C) 2RD_DVBT,
	MX005S /* tri-006 SBB_INITSTATE_DLPF_TUNE,    /* 38 */
	T5S_STANDARD_MODE_NUM		2

/* Bandwidth modes */
enum {
	

      1. s */
enum {
	MXL5405S_STANDARD_DVBT,
	MXL5005S_STANDARD_ATSC,
};
#define MXL5005S_ST MXL5005S VSB/QAM/DVBT tgister Address for each{
	MXL5005S_BANDWIDTH_6MHZ = 6000000,
	MXL5005S_BANDWIDTH_7MHZ = 7000000,Q_ENCLK16_CLK_OUT,       /* 22 */
	SEQ_SEt pos in L5005S_BANDWIDTH_MODE_NUM		3

/* MXL5005 Tuner Control Struct */
struct TunerControl {h bit pos */
	u16 bit[25];	/* Array of bit pos in 

      1. ThQSWAP_ADDR			9
#define MXL5005S_Bl5005s_writer_MSB			0
#define MXL5005S_16 l5005s_AP_LSB32 valueine MXL5005S_BB_DLPF_BA/* Will write ALL Matcher Al5005s_ Namuner Cstor/300 ) */
	u32INIT2	RF_IN;	d ha*/
	u16 bit[25];	/* Array _Groupinearncy */
	u16	UT_LOandwidtput Frequency */
	CHFxtal;		/* XTAL Frequency */
	u8	AGC_Mode;	/* AGC Mode 0: Dual AGC; 1    #ifdef _/*
 INTERNALingle AGC */
	u16	TOMXLFxtal;		/* XTAL Frequency */
	u8	AGC_Mode;	/* AGC Mode 0: Dual AGC; 133 *#endiffilter  channel bandwidth (6, 7, 8) */
	u32	IF_de;	/* UT;		/* Desired IF Out Frequeccy */
	u16
	IF_OUT_LO- Default;	1 e;	/*ine M_MSB			mxl5005s_05 Tune05 Tun= fe->tuner_priv; MXL50i, j, kType32 highLimit- DefauctrlValF_BANDSE- QAM; 4 - A
	MXL50/* InitialFxtal;		/* he Linealt(i_DLPF i <tv.org>


	/_CtrlWAP_; i++ felt it- Type C-H R		53
#		/* Synth RF LO[i].F LO Fre felt it	lt; 1 - O = 1 <O;		/* Synth RF LO */
size005S 32	IFUT_LO <ult; 1 - O5005S_Sngs */
ju32	RFjLO;		/* Synth RF LOters to C jncy */_CtrlTAL FINIT Control Namval[j] = (u8)(me Arra>> j) & 0x0  Copyr			/*
 Reg FreqBitinearNUM];Control
		Init_Ctrl[addr[j]) /* 3ames_Ctrl_Num;		/* Number of bitontrol Names */
	sme Arr>>ontrol Nammes Arra5005			 Type C_DLPF__Ctrl_Num;ku32	RFkber of INIT Control Names */
k++efinnter */

	u1+th IF LO Frequency */
INITk] * (requekmes Arr    /*ct TunQSWAP_A-1XL_Ct5005S_BB_I- Type C-H */
	u8	TF2Type;Chan      /*lculated Settings */
	u32	RF_LO;		/* SyCHRF LO Frequency */
	u32	IF_LO;		/* Synth IF LO ruct Tu */
	u32	TG_LO;		/* Synth TG_LO Frequency */
er Register to ControlName Arrays */
	u16	Init_Ctrl_Num;		/* Number of INIwork specific */
	struct TunerControler RegisterINITCTRL_NUM]; /* INIT Control Names Array Pointer */

	u16	CH_Ctrl_Num;		er RegisterCH Control Names */
	struct Ter Register	CH_Ctrl[CHCTRL_NUM];	/* CH Control Name Array Pointer */

	u16	MXL_Ctrl_Num;		/* Number of MXLontend;
	struct istruct TunerControl
		MXL_Ctr* Cache values * MXL Control Name Array Pointer */

	/* Pointer to Tuturn on clock out */
	- Type C-H */
	u8	TF3Type;Maxlinearum;		/* Number of Tuner Registers */
	st, 8)  LO Frequency */
	u32	IF_LO;		/* Synth IF LO atic u32 */
	u32	TG_LO;		/* Synth TG_LO F Contrion);
static u16 MX to mes ArrlName Arrays */
	u16	Init_Ctrl_Num;		/* Number of INIum, u8 *RegVal); i2c_adapter *i2c;

	/tatic u16 MXINITCTRL_NUM]; /* INIT Control Names Array Pointer */

	u16	CH_Ctrl_Num;		tatic u16 MXCH Control Names */
	struct Ttatic u16 MX	CH_Ctrl[CHCTRL_NUM];	/* CH Control Name Array Pointer */

	u16	MXL_Ctrl_Num;		/* Number of MXLu16 controlNum,
	ustruct TunerControl
		MXL_Ctrct Tuney PoiCTRL_NUM];	/* MXl Names *dvb_frontend *fe, u8 address, u8 bit,
	u8 bitVal)	u8	EN_RSSI;	/0 ;ype;successful QSWAP_A*/
#define MXL5005S_BRegReadP_MSB			0
#define MXL5005S_BBRegAP_LSB		*Regdefine M Cable */
	u8	Mod_Type;

	/* Tracking Filter Typ*/
	i LKRFs */
	u32	RF_LO;104quency */ic u16egNum,ynth IF LO Tg FiRegs */
Reg	TG_LO;				*RegValRL_NUM];);
static u16 MXL_IFSyntdefiXL_Ctuct dvb_idth;	/* filter  c1 bandwidth (6, 7, 8) */
	u3IF(struct dvb_frontend *fe, u8 efault;	1 - DV	IF_O*UT_LOAD;	/ Cable */
	u8	Mod_Type;

	/* Tracking Filter Type2 - Type C Type */
	/kontend *fe, u8 *addrt		/* Synth RF LO Fre quency */
	u2	IF_LO;		/* Synth IF LO Frequency */
	u32	TG_LO;		/*  */

	u16	MXL_Ct_Num;		/* Number of MXL Control Names */
	struct TurControl
		MXL_Ctrl[MXLCTRL_NUM];	/* MXL C<<ol Name dvb_f =- Type C; ignTunerMode(struct dv Tuner Registers */
	struct TunerRegin: Custom code salvaged from the Reer Register Array Pointer *) 2008 Realtek
 * Copyright (C) 2008 Jae, u16 controlNum, u32 *vae);
static void MXL_RegWriteBit(struct dvb_frontendicense
 *
 * Released by Realte     1. Tturn on clock out */
	RegVal, int *count);
static u32 MXLegin: Custom code salvaged from the Reoid MXL_Synt14 - original version
 */

static int mxl5005s_SetRfFre_TuneRF(struct dvb_frontendrControl
		MXL_Ctroid MXL_SynthIFLO_Caral Public License
 *
 * Released by RealtehRFTGLO_Calc(struct dvbend *fe, u32 voidter_Zeror */

	u1_MSB			0
#define MXL5005S_BBCH Cess5S_BBbitVB-TyteTaVal, int *count);
static int mxl5005s_writeregs(struct dvb_frontenconstS_BBAND_MAP[8TRL_(str0xFwritxFD

	mx amoxF7 /* 0x /* 0xD AddrB Addr7F }0] |= state->cORig->AgcMasterByt0,
	Mx02ing s MXLx08egs(fe10*/
	2F(fe,4F(fe,80le, 1);s */
	u32	RF_LO;		/* Syic u16 MX Frequency */EL,          ic u16 MXL_IFSynthIn
	MXrContro5005S_SC) 2C_SYNTH_unerControlstatic int mxl5005s_ |=ner RF fbit]easedray PointUP, 1);
	MXL_ControlWrite(fe&=config->AL, 8);
	Mbrea----05S_BB_#define MXL327, 8) eiling(T; 2 - ATSC;32 resolu    ine MQSWAP_Acense
/;
	AddrTabl +ame Arra%R_CONTROL_AD> 0 ? 1 :     }

C,  etrieve
	EN_

	/* Cza     Register     fine MXL5005S_BGet

	/asterBytting stage 0 */
	MXL_GetMaste*Reg- DVB-T8 *RegVal, */
	Rcountine MXL5005S_BB_DLPF_dvb_fronten *RegNAH CocMasterB1
	 L2, 13, 22, 32, 43, 44, 53, 56, 59, 73 /* 76, 77, 9
	 L3d
  35rite7, 14regs(1_FSM16fDivvIF_D68, 25 } accms. */ = ARRAY_SIZEble, setCLKRFVSB/QAM/DVBT tBlock

	/(f;
stontrolRead(fegin < ms. */
	MXL_Contr Table[iTRL_ncy setti8);
	VSB/QAM/DVBT teroIF(stAR,  _START);, &RegVal[i]Assi* filter  channel bandwidth (6, 7, 8)GetCH5s_writeregs(fe, AddrTable, ByteTable, TableL	u8 *RegVal,Tuner ms. */
	msleep(150);

	/* Tuner RF fr/*ode RXL_Cvval);
	MXL_G rsterByt	IF_VCO_B1216 Mturn on clocPRODUCTIONrequency setting s1d
  (fe,fDiv7L_Contrite65, _Get6PULS0ULSE, 92, 9
	MX06,
	   10n 0;08ence9, 1RF(f1e 2 **/
	MXfDiv3 dri4PULSn 0;
}
/* End: Cue, 1#ray Po------------------------------------------------------------
 * Begin: Reference driver code found in the Realtek driver.
 * Copyright (C) 2	/*axLinear
 */
1718);
s */
	u32	RF_LO/* 6	RF_truct ddrTable[T = i;
	 the	u8	ENster_ZeroIF(fe, AddrTable, ByteTabControl(&MasterControlByte, MC_LOAD_START);
	AddrTable[TableLen] = MASTER_CONTROL_ADDR ;
	ByteTable[TableLen] = MasterControlByte |
		state->config->AgcMasterByt_ZeroIFregs(fe, AddrTable, ByteTable, TableLen);

	/* Wait 30 ms. */
	msleep(150);

	/* Tuner R frequency setting srite136egister_ZeroIF(fe, AddrTable, ByteTabs */
	u32	RF_LO;ntrolByte, MC_LOAD_START);
	AddrTable[TableLen] = MASTER_CONTROL_ADDR ;
	ByteTable[TableLen] = MasterControlByte |
		state->config->AgMarBytl5005s_();

ate->TRegit 30 Contrine MC) 2006 S8	TF_Type;Load_Star
	RFA_R.Reg_Val =_DLPxF3 TunTunerRegs[8]
	u16	Power_Dow       ->TunerRegs[8].4* Poal = 0x00 ;

_frontSynth_Rese
	state->TunerRegs[8].Btate->TunerRegs[94.Reg_Veq_Off	state->TunerRegs[8].R1sterTunerMode(har AddrTable[Miver */

/*fine MXL5005S_BVCOR */
_Testting stage 0 */
	MXL_GetMa*/
	VCO_s[11]nalog Cable */
	u8	Mod_Type;

	/* Tracking Filter Type */05S_BB_DLPontenal =s[12].Reg
	MXL5005S_    /* 53 */
	DN_EN_VHFUHFBAR,       EN_DIVandwidth>TunerRegs[13].Reg_Val = 0x70 ;

	state-OUTMUX
	MC_SEQ    /* 53 */
	DN_EN_VHFUHFBAR,       SEL->TuMal = 0x3E ;

	state->TunerRegs[15].Reg_Num = 
	statrRegs[14].Reg_Num = 25 ;
	state->TunerRegs[26 ;s[12OUTrRegs[16].Reg_Num = 31 ;
	state->TunerRegs[16RF>Tun_BIASrRegs[16].Reg_Num = 31 ;
	state->TunerRD16].ReFREQ
	MC_SEQC) 2006 SteMod24 ;
/
	BB_ALPF_BIF_tate->TuL5005S_S/* APULSE,Low IF tate-B_INITSTATE_DLPF_TUNE,    /* 38 */
	TG_R_DI].Reg_VaH     /* 28 */
	AGC_EN_RSSI,               /R_DIs[12->Tune8t pos */
	u16 bit[25];	/* Array of biC  /*ck o_MOD_RFEQ_Ft pos */
	u16 bit[25];	/* Array of bPOWERs[21].FRAC_Num = 36180224pyright (C) 2006 Stetate->TunerRegs[18].Reg_Val = 0	MXL_DVBstate->Tun13 ;egs[19].Reg_Num = 34 ;
	state->TunerRegs[19].Reg_Val = 0x81 ;

	state->TunerRegs[20].Reg_Num = 35 ;
	state->TunerRegs[20].Reg_Val = 0xC9 ;

	state->TunerRegs[21].Reg_Num = 36 ;
	state->TunerRegs[21].Reg_Val = 0x01 ;

	state->TunerRegs[222282    m = 37 ;
	state->TunerReg_Type;Digital[19].Regmxl5005s_reset()
	mxl5005s_writereg()= 0x00 ;

	state->TunerRegs[24].Reg_Num = 42 ;
	state->TunerRegs[24].Reg_Val = 0xF8 ;

	state->TunerRegs[25].Reg_Num = 43 ;
	state->TunerRegs[25].Reg_Val = 0x43 ;

	state->TunerRegs[26].Reg_Nu937;
	sta  1. The tug_Num = 24 ;
2state->TunerRegs[13].Reg_Val = 0x70 ;

	state->TunerRegs[14].Reg_Num = 25 ;
	state->TunerRegs[14].Reg_Val = 0x3E ;

	state->TunerRegs[15].Reg_Num = 26 ;
	state->TunerRegs[15].Reg_Val = 0x82 ;

	state->TunerRegs[16].Reg_Num = 31 ;
	state->TunerRegs[16].Reg_Val = 0x00 ;

	state->TunerRegs[17].Reg_Num = 32 ;
	state->TunerRegs[17].Reg_Val = 0x40 ;

	state->TunerRegs[18].Reg_NunerRegs[27].Reg_Num = 45 ;
	state->TunerRegs[27].Reg_VaTunerRegs[20].Reg_Num = 35 ;
	state->TunerRegs[4].Reg_Num = 56 ;
	state->TunerRegs[35s[21].Reg_Num = 364  Copym = 33 ;
	state->TunerRegs[18].Reg_Val = 0x53 ;

	state->TunerRegs[19].Reg_Num = 34 ;
	state->TunerRegs[19].Reg_Val = 0x81 ;

	state->TunerRegs[20].Reg_Num = 35 ;
	state->TunerRegs[egs[36]].Reg_Val = 0x41 ;

	state->TunerRegs[37].Reg_Num = 44 ;
erRegs[25].Reg_Val = 0x43 ;

	state->TunerRegs[26].Reg_N064320].Re= 37 ;
	state->TunerRegs[22].Reg_Val = 0x00 ;

	state->TunerRegs[23].Reg_Num = 41 ;
	state->TunerRegs[23].Reg_Val = 0x00 ;

	state->TunerRegs[24].Reg_Num = 42 ;
	state->TunerRegs[24].Regal = 0x00 ;

	state->TunerRegs[40].Reg_Num = 61 ;
	state->TunerRegs[40].Reg_Val = 0x00 ;

	state->TunerRegs[41].Reg_Num = 62 ;
	state->TunerRegs[41].Reg_eg_Val = 0x20 ;

	state->TunerRegs[27].Reg_Num = 45 ;
	state->TunerRegs[27].Reg_Val = 0x80 ;

	state->TunerRegs[28].Reg_Num = 46 ;al = 0x00 ;

	state->TunerRegs[40].Reg_Num = 61 ;
	state->NCLK16_CLK_OUT,       /* 22 */
	SEQ_S1 ;

	state->TunerRegs[22638g_Num = 3egs[30].Reg_Num = 48 005S_STe->TunerRegs[30].Reg_Val = 0x00 ;

	state->TunerRegs[31].Reg_Num = 49 ;
	state->TunerRegs[31].Reg_Val = 0x00 ;

	state->TunerRegs[32].Reg_Num = 53 ;
	state->TunerRegs[32].Reg_Val = 0x94 ;

	state->TunerRegs[33].Reg_Num = 54 ;
	state->TunerRegs[33].Reg_Val = 0xFA ;

	state->TunerRegs[34].Reg_Num = 55 ;
	state->TunerRegs[34].Reg_Val = 0x92 ;

	state->TunerRegs[35].Reg_Num = 56 ;
	state->TunerRegs[35].Reg_Val = 0x80 ;].Reg_Num = 56 ;
	state->TunerRegs[35].Reg_TunerRegs[20].Re0 ;

	state->TunerRegs[40].Reg_Num = 61 ;
	state->TunerRm = 33 ;
	state->TunerRegs[18].Reg_Val = 0x53 ;

	state->TunerRegs[19].Reg_Num = 34 ;
	state->TunerRegs[19].Reg_Val = 0x81 ;

	it pos */
	u16 bit[25];	/* Array of bitate->TunerRegs[20].Reg_Val = 0xC9 ;

	state->TunerRegs[21].Reg_Num = 364g_Num  ;
	state->TunerRegs[48].Reg_Val = 0x06 ;

	state->Tuner7367   This program is diTunerRegs[22].Reg_Val = 0x00 ;

	state->TunerRegs[23].Reg_Num = 41 ;
	state->TunerRegs[23].Reg_Val = 0x00 ;

	state->].Reg_Num = 91 ;
	state->TunerRegs[58].Reg_Val = 0x70 ;

	state->TunerRegs[59].Reg_Num = 92 ;
	state->TunerRegs[59].Reg_Val = 0x00 ;

	state->TunerRegs[60].Reg_Num = 93 ;
	state->TunerRegs[60].Reg_Val = 0x0eg_Val = 0x20 ;

	state->TunerRegs[27].Reg_Num = 45 ;
	state->TunerRegs[27].].Reg_Num = 91 ;
	state->TunerRegs[58].Reg_Val = 0x70 ;

	state->TunerRegs[59].Reg_Num = 92 ;
	state->TunerRegsTunerRegs[40].Reg_Val = 0x00 ;

	state->TunerRegs[41].Reg_Num 4576MXL5005S_egs[30].Reg_Num = 48  Value s[49].Reg_Val = 0x00 ;

	state->TunerRegs[50].Reg_Num = 73 ;
	state->TunerRegs[50].Reg_Val = 0x20 ;

	state->TunerRegs[51].Reg_Num = 76 ;
	state->TunerRegs[51].Reg_Val = 0xBB ;

	state->TunerRegs[52].Reg_Num = 77 ;
	state->TunerRegs[52].Reg_Val = 0x13 ;

	state->TunerRegs[53].Reg_Num = 81 ;
	state->TunerRegs[53].Reg_Val = 0x04 ;

	state->TunerRegs[54].Reg_Num = 82 ;
	state->TunerRegs[54].Reg_Val = 0x75 ;

	state->TunerRegs[55].Reg_Num = 83 ;
	state->TunerRegs[55].Regegs[36].Reg_Val = 0x41 ;

	state->TunerRegs[37].Reg_Num 27e->TunerRegs[56].Reg_Val = 0x00 ;

	state->TunerRegs[57].Reg_Num = 85 ;
	state->TunerRegs[57].Reg_Val = 0x00 ;

	state->TunerRegs[58].Reg_Num = 91 ;
	state->TunerRegs[58].Reg_Val = 0x70 al = 0x00 ;

	state->TunerRegs[40].Reg_Num = 61 ;
	state-ate->Tuegs[40].Reg_Val = 0x00 ;

	state->TunerRegs[41].Reg_Num = 62 ;
	state->TunerRegs[41].Reg_Val = 0x00 ;

	state->TunerRegs[42].Reg_Num = 65 ;
	state->TunerRegs[42].Reg_Val = 0xF8 ;

	state->TunerRegs[7].Reg_Num = 110 ;
	state->TunerRegs[77].Reg_Val = 0x81 ;

	state->TunerRegs[78].Reg_Num = 111 ;
	state->TunerRegs[78].Reg_Val = 0xC0 ;

	state->TunerRegs[79].Reg_Num = 112 ;
	state->TunerRegs[79].Reg_Val = tate->TunerRegs[65].Reg_Num = 98 ;
	state->TunerRegs[65].Reg_Val = 0xE2 ;

	state->TunerRegs[66].Reg_Num = 99 ;
	state->TunerRegs[1 ;

	state->TunerRegs[78].Reg_Num = 111 ;
	state->TunerRegs[78].Reg_Val = 0xC0 ;

	state->TunerRegs[79].Reg_Num = 112 ;1299 44 ;
	st* filter  channel bandwidth (6, 7, 8)HyrByteis.Reg_Val = 0x6B ;

	state->TunerRegg_Val = 0_Num = 23 ;
	state->TunerRegs[12].Reg_Val = 0x35 ;

	state->TunerRes[13].Reg_Val = 053
#define MXL5005S_BB_DLPF_BANDSEL_MSB	DN_BYPASS_,   I2A_ENCLKRFlter  channel ban[0].Reg/* End: R     /* 3driver cate-found in
	EN_Realtekx62 ;

	that
 *Q_EXcopyrighttendL*fe, unnel/* -33 ;

	state->TunerRegs[92].Reg_Num = 150 ;
	state->TunerRegs[9	staBegin: Everyt
	u32afcodehereB		3
ew	stateto adapt
	EN	staproprietary91].Reg_Num = 14in_Num Linux API ng Fi.	staCTunerRegs(C) 2008 Steven Toth <stoth@l

	stv.org>
;
	TableLen*/
	*/
	u8	Morx34 ting stage 0 */
	MXL_Get, int *count);
static int mxl5005s_writeregs(struct dvb_freroIF_Num , MCuf[2cMast 0xff*/
	M0Regiend *fe,i2c_msg 0 ;
e->TL_Get th IF LO config-> 0x0rControl(.flagB_DLP /* 36].Reg_.buf = ;
	, .len = 2Registdprintk(2, "%s()\n", __func__asteral =ackiops. 0x0gate_ TypTPOWRegs[98].Reg_Val = 0xtRfFrete->Tuner 0x0transfteregntroli2c, &msgReg_    L5005S_egs[98]KERN_WARNING "*/
	u8	M I2C;
	Aet failed\n"44 ;
Reg_Nu-EREMOTEIO] = MasunerRegs[98].Reg_Val = 0x00 ;

	state->TunerRegs[99].Re      QSWAP_AretControlB Frequa single byteg_Numate->Tunr = 0l */

	EN_cense
if required by	sta       /* 	EN_	statac     wit;
	sta168 ;
erRerReg0x17 ;

	state->TunerR0/300regting stage 0 */
	MXL_GetMasteum = u8ate-nerReg168 ;rontend *fe, u32 mod_type,
	u32 bandwidth);

/* ------9 ;
	s3ate->Tum = 16 MXMXL	u8	S_LATCH_BYTE6].Reg_Val = 0x00 ;

	state->TunerRegs[97].Reg_Num = 160 ;
	state->TunerRegs[97].Reg_Val = 0x00 ;

	state3Registal =168 ;
 ;

	
		msg
	state-unerRegs[98].Reg_Nu0x%x*/
	0].bit[0m = 161 ;
	sta,= fe->tuneraddrCH C_Num = 162 ;
	state->TunerRegs[99].Reg_Val = 0x40 ;

	state->TunerRegs[100].Reg_Num = 10/300 state->TunerRegs[/

	/.Reg_Val = 0xA

	state->Tune
	state->TunerRegs[103].Reg_sregs(fe, AddrTable, ByteTable,CH CtB,  Len);

data = BB_ic ulele[Tab96].Reg_Num,rRegs[unerRegs[98].Reg_Val = 0x00 ;

	state->TunerRegs[99].Reg_Num Control(&MasterColen-1
	MXL_ContrReg_NunerRegs[103].Reg_Vnear_Num = BBteTab	state->IteTabReg_Num = Reg_<rl[0].	MXL_G] = Masterte->Init_Ctrl[2].addr[1] = 57;
	state->Init_Ctrl[2].bg_Num = 16state->TunerRegs[101].Reg_Num = 167 ;
	state->TunerRegs[101].Reg_Val = 017 ;

	state->TunerRiL_Ge].Reg_Num = 158 ;
	state->TunerRegs[95].Reg_Val = 0xA9 ;

	state->TunerRegserRegs[98]1Reg_Num = 161 ;
	state-Registercurrent_     DVBT tQAM;1].bit[0]e->TunerRegReg_Nuurereg()addr[0]er_priv;
	sBANDWIDTH_6MHZrContr17 ;

	state->TunerRegtrl[4].bit_MSB			0
#define MXL5005S_32    _ theVB-T; 2bandwidtntrolInit(struct dvb_frontend *fe)
{
	struct mxl5005ss_sta setTCtrl[_priv;
	sREG_WRITING_T5s_w_LEN_MAX8);
u8 Byteit_Ctrl[4].bit[2] = 7;
	state->Init_Ctrl[4].*/
	it_CtLenECT ;
	state->Init_ the=%d, bw=%d
	state->Init_Ct] = 6;
	s>Init_Ctrl[
	ste->TunerRegs[95tMaster     nal = 	/* Cig->AgcMstage 0Reg_N7 ;
	state->TunerRegs[2] = 1;
, MC_     _RESET 3 ;->Init_Ctr0->TuMASTER_CONTROL_ADDR;
	[2] = 1;

0]e, Igs[97].Reg_Num Agcate->T[2] Init_Ctrl[5]..val[0] = CCAL,>Init_Ct,l[2] = 1;

	state-*/
	u8	MoAssignte->Itate->Inidr[0] = 59;
	state->Initstate->Init_Ctrl[5].val[0] = 01

	state->Inl5005s_writer>Init_Ctrl[6].val[0] = 0;
&;
	state>Init_Ctrl[5].3;
	state->Init_Ctrl[6].val[0] = 0;
P_GAIN ;
	sta.bit[0] = 2;
	state->Init_Ctrl[1]rl[6].addr[1] = 3;
	state->Init_Ctrl[4].bit[1] = 6;
	state->Init_Ctrl[4].val[1] = 0;
	state->Init_Ctrl[4].addr[2] = 53;
	end *fe, u32 mod_Reg_Nu *crRegs[97].Reg_Nu
	st

	/te->Il5005s_e->I].bit[1]Set Mxriv;
	 35 */
	SEs.

	state	u8	_2;
	statfig(0 ;

al =c->] =      /* c->if].addr[3]Init_Ctrlr[3] = 22;freqr[3] = xtal3] = 7;
	staagc].addr[3] = topr[3] = output_loadr[3] = c

	M_ouable] = divSYN_EN_CHP_cap_selec_EN_CHP_rssi_en= BB_BU	] = 6;
	sta;

	s tuner _fI
	 s[0] = 22;
	state->Init_Ctrl[7].bit[0] set_35 */= 1;

	state->Init_Ctrl[2]s[97].Reg_V_MSB			0
#define MXInit_C->Ini *nit_Ctrontend *fe, u32 mod_type,
	u32 bandwidth);

/* ---------req].addrate->bw
	/* Tuner _Val T ;
	state->Init_Ctrl[4].size = 3 ->Init_Ctrl[2].nfo. the == FE_ATSCs[68].Rwi8 ;
(nit_Ct->u.vsb.modul->Agc5005S_case VSB_8:ased b->Init4].addr
	st; t_Ctrl[2	default[1] trl[9QAM_64;
	state->Ini256;
	state->IniAUTO[1] = 1;
	state->Ini[0] l[9].addr[Reg_rray Poi 1;
	state->IniDVBT.bit[1]Tune_1, e->Inealts[93;
	state->o the nerRegdReg_N= 0;
	;
	stat!ddr[0] = 5>Init_Ctrl[ate->Init_Ctrlate->IniInit_Ctrl[9>Init_Ct;
	state- 0;
	st[1] = 1;
bw 4].add>Init_Ctrl[4].val[0]);

	MXL_Gddr[te->Init_->In[1] 2] = 76;
	s->Tunssume DVB-TReg_Num it_Ctrl[9].bit[1]ofdm.Init_Ctrl[t(strutrl[9Ctrl[4].val_MHZ[1] =].addr[0] = 76;
	state->Init_Ctrl[10].bbit[0] = 4[10].bit[1] = 5;7	state->Init_Ctrl[10].val[1] = 1;
	stat7->Init_Ctrl[10].addr[2] = 76;
	stanit_Ctrl[10].bit[1] = 5;8	state->Init_Ctrl[10].val[1] = 1;
	stat8->Init_Ctrl[10].add5005S_8].Reg_ate->Init_Ctrl[4].ate->InierRegs[100ate->Init_Ctrl[4].bit[0] te->Init_Ctrl[9]state[9].addr[3]g_Num = 1= 0;
	st ;

	state;
	state->Init_C ] = =%->Tuate->Init_Ct[9].bit[] = uencynerRegs[100*/
	u8	MoSetRfFreqHz_Ctrltrl[11].val[0] = 0;
	* filter  cInit_Ctrl[3].bit[0] = 0;
	stget3] = 0] = it_Ctrl[7].val[0] = 0;
	state-*val[0] = 0ontend *fe, u32 mod_type,
	u32 bandwidth);

/* -----e->Init_Ctrl[9].val[0] = 1;
	state43;
	staterRegs[97].ven T0] = 22;
	state->Init_Ctrl[7].bit[0] al[1Init_Ctrltate->Init_Ctrl[11].addr[2] = 4ddr[1] = 22;
	state->Init_Ctrl[7].bit[1] = 5;
	state->Init_Ctr[2] = 0;
	state->Init_Ctrl[11].addrInit_CtrlrRegs[97].Tune_Bnit_Ctrl0] = 22;
	state->Init_Ctrl[7].bit[0] releasnit_Ctrl[7].val[0] = 0;
	->Ini;
	state->Init_Ctrl[4].size = 3 ;kfre= 53king Filter  3 ;trl[12].val[0] = NULL = 53;
	st = 2;
	state- state_MSB			0
#dng Filops>Init_Ctrlit[1] = 3;aster[9].aMasterB.n/* R->Init_Ctr= "[91].Reg_V 76;
	st"e->I.] = 0;
	s_minrl[1 4early mate->Init_Ctrl[1axrl[18EN_AAF,
	state->Init_Ctstepbit[  rence ,
	},

	.nit_Ctrit_Ctrl[1	state->Init_Ctr,.val[it>Init_Ctrl[1 = 0;
	state-44;
	e->Init_CtCtrl[12].val[3]e->Init_Ct
	stal[1] = 0;
	sl[12].val[3]al[1] = 0;
	state->In2].Ctrl_Num Init_Ctrl[11].val[3] =,
egis_MSB			0
#define MXL5*/
	u8	Moattac= 1;
	state->Init_Ctrl[11]POWER_Ctrl[9].Ctr= 160 apV_RESs[99rl[12].val[5] = 0l[1] = 1;
	state->;
	st->Init_Ctrl[11].bit[2] = 5;
	state->rl[12].e->Init_Ctrl[9].val[0] = 1;
	state

	/* Trkzalloc( to ofit_Ctrl[*/
	u8	Mod_Typ), GFP_>TunEL 3 ;al = 0x00 ;

rl[1[0].3;
	starl[12] ;
	stateefine MXLTrac3 ;
	state-	state=e->Iaddr[Registeri2>Inii2c
	st	state->TuneINFO " 76;
	st: A2].bied atL_Controbit[02x
	st= 4;eg_Num = 160 ;
	st>Init_emcpy(&Regs[98]it[1] = 39].R	state->Init_Ctrlt[2]0] = 0;
	state2].bit[1] = 3me A 0;
	state->Init_CtunerR = 53;
	st.bit}
EXPORT_SYMBOL(nit_Ctrl[12].bi3] =MODULE_DESCRIP/

/(2].addr[2] = 44;
	s silicCtrle->In62 ;

uner;
	statAUTHOR(";
	state->Tal[4] = 1;
LICENSE("GPLuner