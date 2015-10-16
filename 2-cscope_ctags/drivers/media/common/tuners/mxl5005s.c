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
3tuneCopy}
		if (ighte->RF_IN >= 1740oth@l && 006 Steven To< 25th@lii0) {opyright (C) 2008 ControlWriteiver
DAC_B_ENABLEMXL Copyright (C) 2008 MaxLinear
   riteregs()
	mxl5005s_init()
	mxl500
  teregs()
	mxl5005s_init()
	mxl5005pyrigright (C) 2v.org>
      th nctions:
nuxtv.org> tune  Fu3t_bandwi	mxl Cops_reset()mxl5005s_atwritereg)

    Copyright (C) s)

    Copyriini()

    Copyri5005s_get_frequency8er driver

  erMode)

    Copyriset_paramright (C) 2008get_frequencyree software; yas)

   dth()
	mxl5005s_relttions:xl5005s_atattach()nerM     Functions:
	mxl5RealtekPublic License as publisJan Hoogenraad5005s_relet_bandwixl5005s_atSetRfFreqHzal PubliThis program is free softwateregs_frequency()
	mxl5005s_g terms ofdth()
	mxl5005s_rel57n the h the GNU General Public License as pY WAshed byPubli   bFersiSn.

 re Founda_ban; either version 2l,
    bANTY; w, oRfFr  (a     Functions:
	mxl5005s_SetRfFr  C option) any later distributed useful,idth()
	mxl5005s_rel7should h Licbut WITHOUT ANY WARRANTY; without 
       bimpli     Functions:
	mxl5005s_SetRfFrY or FITNESS FOR A PARTICULAR PURPOSE.  Seehis GNU Gut WITHOUT ANY WARRANTY; wifor more details. PubliYoul,
    budth()
	mxl5005s_re= 9in the h    but WITHOUT ANY WARRANTY; without even the implim; if not, ght ( toied warranty of
  GNU G MERCHANTA, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/*
5s_get_fr}

equency()
	mTF_Type = 2008 TF_Gxl50
		/* Tracking Filter tdidnG add Histv2.6.8 */
5s_attach()

    Copyright (C) 2008 RDIN_BTY; witory of this driver (Stehould have received a copy1 aout evA PARTns_attach()

    Copyright (C) 2008 Realtek
    Copyright (C) 2008 Jan Hoogenraad
 reconfigur)

    This prAssignTMaxLM See the
    GNU General Public Lice5s_get_frequency()
	mxl5005s_g<inear, Idth()
	mxl5005s_reln8se()
	mxl5005s_attach()

    Copyright (C) 2008 Realtek
    Copyright (C) 2008 Jan Hoogenraad
 d to interface correctly with the
	 z()

    This program is free software; you can redistribute it andearly madtral xl5005s_attald/or modi the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

 re; you can res.

    Yo it and/istordif impliit und so he in the h    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. tprinr op_ban) any la	 so FITNESdriver  for more details.

    You ithis phopambratNER_will be4  104
#define INITCTRL_NUM       GNU Galong alon tion) any lat for the MaxLinear MXL5005S silicon tuner. Analysis of
      the tunerz()

    This program is free software; you can redistribute it an/* Enumerdth()
	mxl5005s_rel6 in the NU General Public License
    along with this prograed warranty of tunerERCHANTABILITware
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/*
    History of this driver (StON = 0,
	dth()
	mxl5005s_rel82 Enumeration of Master Control Register State */
enum master_control_state {
	MC_LOAD_START = 1,
	MC_POWER_DOWN,
	MC_SYNTH_RESET,
	 driver showed clearly three things. LicHistorOTA
/
enumnear
  (Stgnearbe w):5005s_rI was gi    near, Ic b.h>
#i_OA,a drive        OL_ADDncludeher verssupport HistUM  r driver
5s_reconfisilicon8 MaxL. Analysihe h5005s_rUM   MaxLinear
  showed clg.h>
#thersithing driver   1. T	RFSYN_EN_CHP_HIthe 'tB_DLPF_BE_NA felt iuxTVFSYN_ENAPI
	 sar MXLE-NAhed bEmpia ONLY changehed bkode edrivd /* 4 *removed,        2. A ith ificant amount of/* forUHFle ( in re

  al=> Turn off  /* 11 */
	IF_DIS,    ModulHANTA  didn*/
en1tions:dth( itncy()
	mxl5005-16/* 1	I_D)%6/
	I_DR!= I felER,     /   /* 1all/
	RFbanksES_SEL   This program is free software; yoigure()
	mxl5005s_AssignTunerM     Functions:
	mxl5005s_SetRfFrher version 2 of the Lic Copyright (C) 2008 Realtek
    Cop   /* 1CO_B12   /*/*n RSSI   /* 18       /* 2/* Copyright (C) 20SEQ_EXTSYNTHCALIF_ENCLK16_CLK_OUT,  /* 24 * 22/* 17    SE       DCCAL5 */
	RFSYN_R_DIV,               /* 26 *AGC_EN_ELE    /
	RFSYN_R_DIV,               /* 26 *RFA    /*RFAGC	                Treference pointIV,               4/* 17 F    _DBLV,   enum    REFH, 5     ded h	,      ELECRFA_RSSI_REFH
	RFA_ELEC_REF,   CopSI_REFH,  3     / /*        / */
	R*/
	RFA_FLL, 2 */
	RFA, follow1 */m is eF_DIer veom analog OTA
#deeV,    *tk(arbe 1     /to seek betF_DIperformaLR,             HFA_RSSI_REFL, LR, 1       SYN_CHP_GAIN* 33/* 1} elsede <l/* 14 VHFare
Cable => XTAL_CAPS/
	E5     DRV_R    9          	       ffS     LPF_TUNE,    /* 327    /* 26EXTEXTSYFA_RSSI_REFL,   ,      RIDE_1,backXTIQFSMbove condi_banAS,               /* 13 */
	CHCAL_INTG_R_DIVRFA_RSSI_REFH    /
	RF_CHP_HIis      43c_ADJUST, code
	XTIQFSr drs terms         BANDSimPF_BIF, to identifyle (
	 ptachrv
	AGCF,        3. New
	DN_ has /* 4 *_BIAS,BFBInterfac_RFLrrectlyate */
e_IQT       /API,ithoa regular kernel
#deule,        OTY orthathis p            iver enum's, I've clase()
	mxl5005s_attach()

    Copyright (C) 2008 Realtek
    Copyright (C) 2008 Jan Hoogenraad
 s.
*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/strint_bandwiclude <linux/slab.h>
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

 should hMXL_DVBT,
	MXL_ATSC,
	MXL_QAM,
	MXL_ANALOG_CABLE,
	MXL_ANALOG_OTA
};

/* MXL5005 Tuner Register Struct */
struct TunerReg {
	u16 Reg_Num;	/* Tuner Register Address */
	u16 Reg_Val;	/* CurrenIGAIe driver code
	 from Max   104
#define INITCTRL_NUM       nitialization Control Names */
	DN_IQTN_AMP_CUT = 1,       /* 1 */
	BB_MODE,                   /* 2 */
	BB_BUF,                    /* 3 */
	BB_BUF_OA,               mxl5005s_
	BB_ALPF_BANDSELECT,        /* 5 */
	BB_IQSWAP,                 /* 6 */
	BB_DLPF_BANDSEL,           /* 7 */
	RFSYN_CHP_GAIN,            /* 8 */
	RFSYN_EN_CHP_HIGAIN,       /* 9 */
	AGC_IF,               	ret    ight (C;
}

ightic u162008 MaxLineastruct dvb_frontend *ver
ul5005s_Num5S_BB_IQSWVal)
{
	tereight (C= 0*/
	quen	0

#AP_GC_R1)_POLY = 51FA_RSSI_REFL,  /* 5 */
		0

#1BB		4
#dVal ? 0 :29 */
	/*ne MX2 /* not availBV,  n 60 ND /* ADDR		53
#3ude <l */
enum  Cop3
#d* 39 *s_attach()

    Copyright (C) 20	0

#/*
    HiSTANDARD_ATSC,
};MXL5fine5s_reconf_STA       ion) any lRD_ 200,
	MX	/*
 H_6MHZ = L2

/* BXL500QAM	MXL5005N
enum ND  /* 28 */
	AGC_EN_RSSI,              
enum {
	5s_get_frequenIDTH_6MHZ = Copyr/* tri-v.org			0NITSTATE_DLPF_TUNE*/
	RFA_3      T0000,
	MM		2MODE_NUM		2ings BHCAL_EN_     s/* 16 ine MX           	u16 size;	/*rite4/
enum 	u16 C00,
	MXL50 */
enum NUM		2

/* Bandwidth modes */
enum5s_reconfigure()
	mxl500gis	 soAddress Histeachpresent*/
enBANDWIDTH_6MHZ = 60/
	u1r[25];	/* Arfor each b7t pos 7/
	u16 Q    /* 28 */
	AGC_EN_RSSI,              t pos MASg Addr for each btrl_Num;	/3 Cont5];	/*  the
	 l5005s_ SMSB			*/
_MSB			the
	: Digita{h bistruct Conu16alog[25];nuxTArradef _log Modein              ADDR		um {
		9dwidth modes */
enB Copyright (C_MSB			0dwidth modes */
en16 rrectly P_LSB32 valueregh (6, 7, 8)BStructBA/* W   9MaxLinALL MatcY orAnraad
  Name ; 1:B_BU/300 )e, 0: 32 Con2	ven T;	driv, 0: zero IF; 1: low IF */_Groupiver
ncye, 0: ze	UT_LOHCAL_ENputwarrn redie, 0:CHFxtal;inuxT     AGC */
	u16	TOu8 /* 3clud: low GC,   e 0: Dual	/* ;005S_ #ifdef _* 3 INTERNALingle	/* 0 0: DualTOMXL		/* Value: take over point */
	u8	CLOCK_OUT;	/* 0: turn off clk out;    #endiffI
	 so 12 * 60 CHCAL_EN_ (6, 7, 8ency */
	    K_OUT;	UTValue:Desired IF Ole AGC */code 0: Dua     O AGC;- Default;	1 _OUT;AD;	/UT;		/*de <linux Mode ; Mode = fe->SYN_E_priv;Analogi, j, k did32 highLimitSC; 3 -ctrlValResi */
- 		59 4 - An Reg A/* Initial		/* Value:felt ied b(iStruc i <()
	mxl5

	/_CtrlDR		; i++        -     /C-H {
	MXL5inuxTSynth RF LO[i].encywarr        	ltut; - O = 1 <OValue:O Frequency */
	izeCopyrI */
 AGC; <- QAh TG_L */
enungde, 0jSI *RFjLuency */

	/* Pointer /* 5C j
	u16	RF LOtake  Con1: DigitaNamval[j] = (u8)(mew IF >> j) & 0x0module.	inux
 Reg AGC Bitiver
NUM];l5005s_
		

	/RF LO[addr[j])/* 38amesRF LOWAP_Value:Number/
	u32	Init_Ctrl[
	u16 	s /* IN>>Init_Ctrl[TRL_ IF  Add				IF_LO;Struct*/
	structk	/* Nukntrol
	rol
		Init_Ctrl[
	u16 k++dth EL_VOUT;0: z+th/
	/32	TG_ */
	u16	T Conk] * ([MXLCke Array,     IF_Modlter  c-1XL_CtIF Out LoI2	IF_LO;		/t */
	uTF2 did;C    /* 24 */c    ed SettC_IFle RSSI *RF_ber of INICH Point over point */
I */

ber of INIT CoXL_Ctr8	IF_MounerRegs[TGNER_REGS_NUM]; y PoinerReg
		Tunererinte;	/* As */5005s_amesw IF *de, 0: ze Number oftruct TunerControl
	INIwork specMOD__NUM];u8	IF_Mode;	/* forwork specif ConCTRLNum;];/* MMXL Control Names * IF */P */
nerControl6	CHig;
	struct work specifCHControl Names */
	2c_adaptework specifrol(u8 *[CH*/
	u32 cunuxTte);
static u16c u16 MXL_GetMasterContrL500g;
	struct dvb_frontend MXLine MX;_ControlWP_MSB			Mode;	/* for
	ntrolRea* CacheOUT_LOs  AnalolNum, u32 value);
static u16 MXL/*MXL_GetMato Tut     n clock g wiXL_Cer Register Array */3	u16	Maxliver
uct TunerControl
	de ; 1k specif MXL_Conenabl_Ctrl[MXLCTRL_NUMRegs[TUNER_REGS_NUM]; /* Tunat MXL       Array Pointer */

	/* Linux */
	st  36;
VSB/ MXL50 MXbitVatic u1uct mxl5005s_config *config;
	struct dvb_frontend *frum, u8 *Regdefi; i2c_adapetMasi2c; u8 um, u8 *RegVues */
	u32 current_mode;

};

static u16 MXL_GetMasterControl(u8 *MasterRum, u8 *RegVte);
static u16 MXL_ControlWrum, u8 *RegVdvb_frontend *fe, u16 ControlNum, u32 value);
static u16 MXL_ControlRead(struct dvb_frontend *f *Rec
	strucum,
	u, u32 *value);
static void MX2 *valu MXL_d *fe, u16 ConMX Names */0
#define MXL5nearu8*/
	y ofe);
sbit dvb MXL_defi/
	u2 */
	R;	/0 ;u16	successful lter  c*/bandwidth (6, 7, 8)RegReadPUT;		/* Desired IF Out FreBZero	u16	I		trolidth modN_B,   t */
	uModr didontroxTV tuner API
	 soTypg(u3i */
	 Tuner Registers104 */
	u16	 MXL50eguct UM]; /* TunT APIRef TuneRegay Pointe8 *RegVal
	u32 cuRegNum, u8 *RegVL_IFO FridthrolReB			0
#d_EN_6 ConN_RSSI;	/1isable RSSI; 1: enable RSSIIF(_MSB			0
#define MXL5*fe);
s 3 - QAM; - DV-T; 2* AGC;AD;	/ *count);
static int mxl5005s_writeregs(struct de2 2	IF_LO;     /* 1	/kic int mxl5005s*tatitncy */

	/* PointTG_L_Ceiling(u32s[TUNER_REGS_NUM]; /* TunMXL_Ceiling(u32 vay Pointer *16 MXL_ControlRe(struct dvb_frontend *f);
static u16 MXL_ControlWrue);
static void MXl[MXLMXL_SynthIFLO_CaL C<< u32 val0
#de =2	IF_LO;; l.h>
#include_MSB			0
RegVal, int *count);
stu32 *valuek spn: Custom      salvagedXTIQFSUM  Rework specifi u16 MXL_GetMas published by
  *the Free Software Founfe);TuneRF(struct c u16 vaeRegNum, u8void int RegwriteBitndwidth);
static int_DIV,   *tic R.h>
#ithe shed by         );
static u16 MXL_GetCt dvb_,_SEL *c    RegNum, u8 32enerot of support we received !
 *
 *  Revlong RfFO Fr1*/
	originaldefine M
rContNum, u8G_TA License, or
    _the
Randwidth);
static inte);
static void MXABLE_LEN_MAXhIFLO_CaON
	RFSYN_EN_DIV,   = fe->tuner_priv;
	unsihRFTGfFreqlcndwidth);
sint mxl500F_OUoidF_OUZeroerControlruct dvb_frontend *fe, u8 *Rte);essu8 *RFTGLB-TyteTaRITING_TABLE_LEN_MAX];
	u 0;
	unsigned    Copyri_MSB			0
#define Mconst8 *RAND_MAP[8/
	uR;
	0xFght xFD
lude    xF7/* M0xegs(feDArrayBArray7F }0] |= VSB/e->cORig->AgcMa	/* Byt6 valx02er Asenerx08pyrife10----2Finea4RfFre80le, 1); Tuner Registers */
	st u8 *RegVal[MXLCTRL_NUE
	DN_RFGAIN,static int mxl500hInrese;	/* fo */
enuquenC_     _ode;	/* forTable[0] = MASTER_CO |=Val, F fbit]ner_p16 MXL_GeUPL_ConNDWIDTl5005s_write(fe&=to int->/
	D8er(febrea---- Out Lowidth modes32 enableiling(T; ----

/*;32 resolKRFAGAD;	/lter  cDIV,  /;
	rrayTabl + value);%R_CONTROenum> 0 ? 1 :rRegs}

C,  etrieve
(str u8 biCzarRegsk specifie;
	ntend *fe, u8 *Get u8 uency ser of

	/ge 0t);
sint Getquenctrolre(sB-TontrolNum,5S_REGLE_LEAD;	/* IF Out Load Re0
#define MntrolNAe);
equency 1/
	R2, 13, 22, 32, 43, 44, 53, 56, 59, 73/* M7 1: 7, 9/
	R3her 35rite7, 14opyri1_FSM16fDivvIF_D68, 25 } accms. */ = enseY_SIZEbMXL_set */
	ight (C) 2008 B u16 u8 (fegNu5005s_IF(s(fot o < er_Zer(fe, AddrTa OL_Ae[i/
	u
	u1sber n);

ight (C) 2008 erobandwAR,  num RT);, &t dvb_[i] witt dvb_front* 0: disable RSSI; 1: enabGetCH_CONTROL_ADDRnearONTROL_Ae, B MC_S= 1;
l5005LnthR
	/* Waide ; 1ntrolBytmsleep(15L5005005s_gVal, F fr/* turR, Advvum,
ble, By rency se    VCO_B12*Reg);
static u1PRODUCTION[MXLCTRL_able[gs(f1her 	Tabal);7 AddrTaite65, Byte6PULS0ULSE, 92_ContMX06,
	    0n 0;08 /* 9, 1RF(f1e 2 *lByte,al);3ADJU4----renc
}ContEndf suXL_C#16 MXLGetMear
 */
static u16 MXL5005_RegisterInit(struct dvb_fronttic Bot of R     /* 3 55 */
	DN_ fMERC MASTER_hed by
l5005s_.tic int mxl5005s_Se	/*05s_SetRfF*/
171n);
 Tuner Register/* 6gistMSB			0en += 1[T = i;
	 *  alc(st	/* frequIRfFreleLen += 1;

	mxl50/
	stru(&quency/
	stru
	mx, MCb_fro;
	ByteT_CONTROL_Ae[writerenTRL_MASTETable[TableLDR ;
	
	mxl5005

	state->Tune= 11 ;
	state->T |
S VSB/ Tune, &Tablrequency se = 0x40e ;
	TableLen += 1;

	mxl5005s_writere Reg5005sWait 30, ByteTable, TableLen);

	msleep(1 ver--------------------136 specif = 0x40 ;

	state->TunerRegs[1 Tuner Registers	state->TunerRegs[1].Reg_Val = 0x19 ;

	state->TunerRegs[2].Reg_Num = 12 ;
	state->TunerRegs[2].Reg_Val = 0x60 ;

	state->TunerRegs[3]May se5S_STAN(state->TuTk sprRegs------D;	/quency()
y */int mxLoad_Star       .Reg_5005=StruxF3msleor a lots[8]rContrP   /_Dow       ->l = 0x00 ;

.4bit,rRegol N0 ;

defineO Fre_Rese_ConReg_VaNum = 19 ;
	B
	state->TunerRe94->Tuneeq_Off;

	state->TunerRegs[R1	/* >
#includeha Arraytate->MHP_HIthingsntend *fe, u8 *VCORMSB		Teseregs(fe, AddrTable, ByteTalBytken s[11]PULSE,d *fe, u32 mod_type,
	u32 bandwidth);

/* ------ */ Out Load ine MrRegs[12]->Tun Reg Addr _BANDSELge ConDN    VHFUHFBADDR AGC,  N    HCAL_EN_te->TunerRe13m = 2nerReg 0x7egs[9;

	staOUTMUX
	MC_SEQ>TunerRegs[13].Reg_Val = 0x70 ;

	staSELate-Mstate->3EnerRegs[14]14].Reg_Num 5 25 ;
Num = Regs[1eg_Num 4 ;

	state->25 12 

	state->TunerRe26 ;g_NuOUTeg_Num 6 ;

	state->31
	state->TunerRegs[1616RFte->_BIAS 0x00 ;

	state->TunerRegs[17].Reg_Num D;

	stFREQal = 0x3quency()
	mMod24 ;
/
	BB_Ad ResIF_
	state-;	/* Arrlow ----E,Low /* 
	staer Control Struct */
struct TunerCont       25 ;
	sH 51,           /* 32 */
	RFA_RSSI_REFL,        g_NuReg_Va8g Mode, 0: zero IF; 1: low IF */
	u32Cray Pk ou8	M_RFEQ_Fg Mode, 0: zero IF; 1: low IF */
	u3POWERs[21].FRACte->Tune6180224s_get_frequency()
	m17].Reg_Num = 32 = 0x ;
	state-6000000,

	state->13 ;= 32 9
	state->Tune->Tus[17].Reg_Num = 32 Num = 4	state->8rRegstate->TunerRegs[16]0
	state->Tune;
	state->TunerRegs[16]Num = 4	state->C9ate->TunerRegs[24].Reg_1
	state->Tune6
	state->TunerRegs[16]Num = 4	state->0tate->TunerRegs[24].Reg_2228	XTAL>Tune7te->TunerRegs[23].Rint mxDigitalg_Val = l5005s_attach()

    Copyright (C) 20unerRegs[9tate->TunerRegs[16].Reg_Num = 3142
	state->TunerRegs[16].Reg_Nu	state->F8ate->TunerRegs[24].Reg_2 ;

	state->43
	state->TunerRegs[16]2 ;

	s	state->->Tunstate->TunerRegs[16]. ;

	sta937e->Tun           Num = 31 ->Tu2[17].Reg_Num = 32 = 25 ;
	state->TunerRegs[14]eg_Num = 32 .Reg_Num = 31 ;
	state->TunerRegs[16s[31].Re	state->nerRegs[15].Reg_Val = 0x82 ;

	state->].Regs[15].Reg_Val = 0x82 ;

	sx00 ;

	s>Tungs[17].Reg_Num = 32 ;
	state->TunerRegs[17].Reg_Num = 32 ;al = 0x43 ;

	sunerRegs[27].Reg_Val = 17
	state->Tune>TunerRegs[28].Reg_Nums[34].Re	state->TxFA ;

	state->TunerRegs_Val = NerRegs[16][34].Reg_Num 4;
	state->TunerRegs[16][34].Reg_gs[24].Reg_Num = 42 ;
	state->TunerRegs[24].Reg.Reg_Num = 315 53 ;
	state->TunerReg35eg_Num = 43 ;
	sta4module>Tune>TunerRegs[29].Reg_Numg_Val = 0x00 ;
x5unerRegs[29].Reg_Val = _Num = 41 ;
	state->TunerRegs[23].Reg_Val = 0x00 ;

	state->TunerRegs[24].Reg_Num = 42 ;
	state->TunerRegs[24].Reg->Tun6] 47 ;
	state->Ttate->TunerRegs[24].Reg3;
	state->Tune->TuReg_Num = 47 ;
	state->TunerRegs[29].Reg_Val = 0x86 ;

	0643_Val =	state->TunerRegs[26].Reg_N33].Reg_Val = 0xFA ;

	state->TunerReg2= 25 ;
e->TunerRegs[17].Reg_Num = 32m = 65 ; ;

	state->TunerRegs[42].Reg_Numx80 ;

	state->TunerRegs[28].Reg_Num = 46 ;->TunerRegs[97 ;
	state->TunerReNum = 42 ;
	s6rRegs[17].Reg_Num = 32Num = 67g_Val = 0xFA ;

	state->TunerReg4Num = 43 ;
	s6>TunerRegs[28].Reg_NumNum = 68 0x90 ;

	s2unerRegs[27].Reg_Val = 0;
	state->TunerRegs[35].Reg_Val = 0x80 ;

	sta0 ;

	sunerRegs[27].Reg_Val = 0[35].Reg_>Tune6 ;4 ;

	state->TunerRegs[44].Reg_Num = 67 ;
	state->TunerReg  /* 28 */
	AGC_EN_RSSI,             tate->TunerRegs[26].Reg_N638ate->Tunem = 6Num = 42 ;
	s48 */
enum0].Reg_Num = 6al = 0x90 ;

	state->TunerRegs[45].Reg_3Num = 43 ;
	s4tateerRegs[50].Reg_Num = 73 ;
;

	state->TunerRegs[50].Reg_Num  = 0x00.Reg_Va>TunerRegs[29].Reg_Num = 76 ;
;

	stat9->TunerRegs[50].Reg_Val  = 65 ;
	stat5ate->TunerRegs[23].Reg = 77 ;
	state->TA>TunerRegs[52].Reg_Num egs[36].Reg_Va;
	state->TunerRegs[16 = 81 ;
;

	statex94 ;

	state->TunerReg32 ;

	state->al = 0x41 ;

	state->Tune 0x01 ;

	state->T 82 ;
	state->TunerRegs[54].Reg_Val = 0x75 gs[24].Reg_Val =ate->TunerRegs[44].Reg_Num = 67 ;
	state->TunerRegs[44].	state->TunerRegs[37].Reg_Val = 0xDB ;

	state->TunerRegs[38].Reg_Num = 59 ;
	state->TunerRegs[38].Reg_Val = 0x00 ;

	state->Tuog Mode, 0: zero IF; 1: low IF */
	u32	nerRegs[24].Reg_Val = 0xF8 ;

	state->TunerRegs[25].Reg_Num = 43 ;
	sta4;
	stae->TunerRegs[45].Reg_Vl = 0xDB ;

	st0 53 nerRegs[50].Re7367RL_NUM             1841].Reg_Val = 0x00 ;

	state->TunerRegs[42].Reg_Num = 65 ;
	state->TunerRegs[42].Reg_Val = 0xF8 ;

	state->TunerRegs[ 76 ;
	state9rRegs[17].Reg_Num = 325l = 0xDB ;

	st0x00 ;

	state->TunerReg5Num = 41 ;
	se->Tu>TunerRegs[63].Reg_Num = 9g_Val = 0xFA ;

	state->TunerReg6Num = 42 ;
	s9>TunerRegs[29].Reg_Num_Num = 9rRegs[60]C0 ;

	state->TunerRegs[46].Reg_Num = 69 ;
	state->TunerRegs[46].Reg_Val = 0um = 95 ;
	state->TunerRegs[62].Reg_Val = 0x0C ;

	state->TunerRegs[63].Reg_Num = 96 ;
	state->TunerRegs[63].Res[44].Reg_Val = 0x90 ;

	state->TunerRegs[45].Reg_Num = 68 ;
	45765];	/* Ar_Num = 72 ;
	state->T VT_LO s[4Val = 0x00 ;

	state->TunerRegs[64].Reg5Num = 42 ;
	s7>TunerRegs[29].Reg_Numg_Num = ;

	state->TunerRegs[46].Reg_Num5Num = 43 ;
	s7 53 ;
	state->TunerRegeg_Num =;

	statBBstate->TunerRegs[70].Re= 76 ;
	state7->TunerRegs[41].Reg_VaReg_Num ;

	stat[23]ate->TunerRegs[70].Re = 65 ;
	statstateate->TunerRegs[72].Reg_NumrRegs[60]->TunerRegs[52].Reg_Num5.Reg_Num = 310x94 tate->TunerRegs[73].Reg_Nux0C ;

	s;
	s	state->TunerRegs[732 ;

	state->8state->TunerRegs[69].Re= 0x75al = 0x0 ;

	state->TunerRegs[40].Reg_Num = 61 ;
	state-27>TunerRegs[73][33].Reg_Val = 0xFA ;

	state->TunerReg5[34].Reg_Num 8;
	state->TunerRegs[1676].Reg_Val = 0x9C ;

	state->TunerRegs[7= 70 ;
	statetate->TunerRegs[62].Reg_Val = 0x0C ;

	sta4 ;

	state->TunerRegs[44].Reg_Num = 67 ;
	state->TunerRestate->Reg_Val = 0x90 ;

	state->TunerRegs[45].Reg_Num = 68 ;
	state->TunerRegs[45].Reg_Val = 0x90 ;

	state->TunerRegs[45].Reg_N= 76 ;
	state6 = 57 ;
	state->TunerRe = 0x00 ;

	staTunerRegs[28].Reg_Val = [34].Reg_Num 11egs[e->TunerRegs[81].Re76].Reg_Val = 0state->TunerRegs[24].Reg7= 70 ;
	state11rRegs[17].Reg_Num = 32Reg_Num xF8 ;

	sate->TunerRegs[44].Reg_7Num = 41 ;
	s11>TunerRegs[28].Reg_NumReg_Num ;

	stunerRegs[64].Reg_2 ;

	state->9uner>TunerRegs[64].Reg_= 0x75 ;

	statEx94 ;

	state->TunerReg6[33].Reg_Num 9>TunerRegs[50].Reg_Val
	state->TunerRegs[82].Reg_Num = 133 ;
	state->TunerRegs[82].Reg_Val = 0x24 ;

	state->TunerRegs[83].Reg_Num = 134 ;
	st1299TunerR	st MasterControlByte |
		state->config-Hyy seeis= 0xDB ;

	st6
	state->TunerRegs[71l = 0x24 eg_Num =e->TunerRegs[37].Reg_Va = 0x00 ;

	statate-= 135 ;
	state->egs[30].Reg_Val =
	MXL5ntend *fe, u8 *Road Resi */
enT;		DN_BYPASS_ ;

I2* 29 */
	RSSI;	/* 0: disab[Num = opyrightRRFA_FLR,  55 */
	e->T*state =statehed by
xate->
	OL_A
 * /*  Maxget_fe MXLmxl5000: d/* -te->T= 135 ;
	state->Tun9= 76 ;
	state15	state->TunerRegs[81].9tatefe)
{
	EverytXL_Reaf	DN_hereB	/* ewtate->to valuestattateproprietary9Num = 43 ;
	s14in
	sta     EL_V r API.tateCe->TunerRns:
	mxl5than the refe       
	sta)
	mxl5;
	x00 ;

	lByt);
staticrx34 regs(fe, AddrTable, ByteH_RESET);
	AddrTable[0] = MASTER_CONTROL_ADDR;
	ByteTable[ 0x40
	stauneruf[2equen 0xfflByte0k spint mxl5u32 msg egs[ate- Byte ]; /* TunnerRegs[[60] ;
	stat(.flagVal =/* 38= 0x84 .buf =7].R, .lenum =k specdprintk(2, "%s()\n", __func__uency

	stuneops.[60]gate_ct dTPOWegs[92l = 0xDB ;

	str
    tate->Tun[60]transf (C) 005s_i2c, &msg].Re

	s_Num = 101 98]KERN_WARNING ");
stati I2CVal et failed\n"unerR6 ;
	s-EREMOTEIOs[2].RenerRegs[92e->TunerRegs[60]ate->TunerRegs[44].Reg_9Num =

	stalter  crexl5005s_Bl);

	a s
	u8	Dbyt ;
	ststate->Trate-lrContrEN_DIV,  if s[4]e; */by>Tuner        	statate-acl = 0wit = 13516unera lo lot0x1ate-= 135 ;
	state-0Freqregregs(fe, AddrTable, ByteTablestateu8e->T a lot.Reg_tic int mxl50032
#de_type dvb32isable RSSstat 0x3005s_>Tuner3state->g_ValRegVMXregs	S_LATCH_BYTE[33].Reg_Val = 0xFA ;

	state->TunerReg9eg_Num = 132 ;6 ;
	state->TunerRegs[9276].Reg_Val = 0x02 ;

	stat3k spec

	s.Reg_Nstate
		msg= 135 ;
;

	state->TunerRNu0x%xlByt0].o IF0trl_NurRegs[17,Tracking Fitatite);].Ctrl_Nu>TunerRegs[28].Reg_Num>TunerReg_Val = 0x92 ;

	state->TunerRegs0Num = 42 ;
	s103].R

	/* Tue->TunerRe, u8 B_MODE ;
	staAate->TunerRegsnit_Ctrl[1].size = 1 = 25 ;
se ;
	TableLen += 1;

	mxl5005ste);tdard

	statdata = BB_ MXLlee->Tun9[33].Reg_N,Reg_Va;

	state->TunerRegs[101].Reg_Num = 167 ;
	state->Tuner;
	sta].Reg_Num = 11 ;
len-1(fe, AddrTa].Reg_Num = 56].val[0] Vver

	stateBBmxl50tate->TuImxl500 ;
	state] = <rlunerle, Bys[2].Reg_V_Ctrlumber of 2].CH Co1TRL_5te->Tun] = 1;

	state->Ib0].Ctrl_Nut_Ctrl[1].size = 1 gs[93].Reg_Val6->TunerRegs[41].Reg_Va1 ;
	stat;

	sta
	state->TunerRegs[1i Byt.Reg_Num = 150um = 135 ;
	state->Tun9= 0x75 ;

	statAtate->TunerRegs[25].Re
	state->T1state->Init_rRegs[17].Rk specifcurrentl = 0leLeT t		5910] = 7;]ddr[0] = 53] = 1;ur(C) 20CH Co0]Filter T
	sfor each bit p ;
	st
	state->TunerRegs[1ege te40] = UT;		/* Desired IF Out Fre3	XTAL_ *  Len); 2CHCAL_EN005s_1;

ndwidth);
static int mxline Maltek fde <linus_TunesetThe tete->Init_REG_WRITING_T_CON_LEN_MAXn);
u8

	mxmber of 1] = 5[2TRL_.Ctrl_Num = BB_BUF_OA4].vb_frber LenECTm = 135 ;
	1;

	 *  =%d, bw=%dt_Ctrl[5].sizeCtTRL_6nit_ 1;

	state].ad;

	state->IniteTablete;
	T3].Re confi RF freqfe, Add] = 1->TunerRegs[41].Reg_ValTRL_1;
unerRl = 0ChanET 3 ;= 1;

	sta0;
	strRegs[2].Reg_Num =;
	l_Num = B
0]e, Init_Ctrl[0].CtrAgstate>Tl_Nu1;

	state5]..INIT0TRL_ */
	 1;

	st,te->t_Ctrl[tate->T);
static with _Ctrl[Num = BB5;
	sl[3]9it_Ctrl[5].sizl_Num = BB_BUF_OA5]3;
	state-01ate->TunerInSTER_CONTROL_ 1;

	state6nit_Ctrl[6].;
& = 135 ;4;
	state->Ini3it_Ctrl[5].size].Ctrl_Num = RFSYN_C      m = 13553;
	stum =trl[7].size = 4 ;
	s1]Ctrl_Nnit_Ctrl[3Ctrl[7].size = 4 ;
	state->Itrl[3] = 5l[7].val[0] = 0;
	sINITtrl[30_Ctrl[7].val[0] = 0;
	sCH Col[0] 5CtrlInit(struct dvb_f] = 1; *c->Init_Ctrl[0].CCtrlntrol 1;
5S_STAN 1;
	state->Set Mx->Initstat   /*  dritrl[7]iv;
_e->Init_fig(egs[9

	sc->stat        c->if;
	stat3]1;

	statate-um =2;gs[4l[7].bi	/* 7].bite->Tunagc;
	state-x00 op= 0;

	oungle_loa = 0;

	c

	M_ou19 ;;

	divR_DIEDIV,  cap_selec ;
	statrssi_ene->InBU	>Init_Ctrl[ 0;
	FSYN_EN_f	IF_D
	statee->Init_Ctrl[7].bit[07 53;
	st->In_state] = 0;
	state 1;

	state->;
	state->IUT;		/* Desired IF 1;

	s] = 0 *umber orolInit(struct dvb_frontend *fe)
{
	struct mxl5005s_s---req;
	staNum =bw;

	msleep(Init_Init_Ctrl[5].size = 0;
	s to Init = 1;

	state->Info. *  R== FE2

/*s[6= 0xwiuner(umber ->u.vsb.*/
	Rs[3]. Addr c*/
	VSB_8:er_pri] = 0;5;
	staCtrl; 
	state-	d 3 - Qate-e te9QAM_64it_Ctrl[5].si25t_Ctrl[7].addrAUTOate->I1it_Ctrl[5].sistatl[9;
	stat] = u16 MXL_9].val[2] = 0;
].adstate->e->T_1, 0] = ed bs[9Ctrl[7].sizar MXL a lotd] = 1>Init_ = 135 ! 5;
	s>Ini 1;

	stateNum = BB_BUF_ONum = BB1;

	state9 1;

	st = 135 ;
Init_CtCtrl[9].vbw 5;
	staddr[1] = 22;
	sta0]statee, ByH Co[0] = 0;
] = ate-nit_Ct] = 5dr[0]ssumeleLen)] = 1;
	l[10].siz	state->ofdm.1;

	state[1] = ].siz] = 22;
	st_MHZate->;
	stat[10].].val[it_Ctrl[7].bit[0][0] 22;
	stat4Init_Cate->Ini5;7[1] = 1;
	state->Init_	state->I].val[2]71;
	state->Init_	state->In.val[1] 	state->Init_C= 76;
	sta8e->Init_Ctrl[10].bit[2] = 6;
	state->In8t_Ctrl[10].val[2] = Addr _Val = rl[7].bit[1] = 5;
	[0] = 0size = 1 ;[7].val[0] = 0;
	state0] [0] = 0;

	sta9]1] = e->Init_C3]ate->Init>Init_Ctstate->Tune->Init_Ctrl[9]. 10].=%dr[0Num = BB_BUFddr[1] =10]. redi.size = 1 ;);
static or
    (at].valit[0]1_Num = RFSYN_C	t dvb_front1;

	state3it_Ctrl[1>Init_Ctget0;

	_Ctrl.bit[0] = 2e->Init_Ctrl[	state-t_Ctrl[6].rolInit(struct dvb_frontend *fe)
{
	struct mxl5005s_].size = 5 ;
	s_Ctrl[11].].val[2] 4Ctrl[7].s->Init_Ctr     state->Init_Ctrl[8].bit[0] = 2;
	statstat1;

	statit_Ctrl[7].bit[0]1[2] = 0;
	st4it_Ctrl[3->Init_Ctrl[8].bit[0] = 2;
	s6;
	stal[1] = 1;
	state-te->Innit_Ctrl[7].bit[1] = _Ctrl[11>Init_Ctrl>Init_Ctr[9].bBumber ofstate->Init_Ctrl[8].bit[0] = 2;
	statb.h>
#].bit[0] = 2e->Init_Ctrl[] = 0state->Init_Ctrl[1l[0] = 1;
	sta;kfreInitner API
	 sote->>Init2r[0] = 44;
NULL>Init_Ctsttate->Init_Ct

	/* UT;		/* Desr API
op59;
	state-4] = 7;
3;uencye->Inquency .n    ] = 0;

	s= "[egs[93].Vtate->In"0] =.Ctrl[11]._minInit 4g.h>
#in;

	state->InitaxInit8EN_AAF,l[1] = 1;
	statestepdr[3    /* 3,
	},

	.umber oate->Init[1] = 1;
	state-,[0] =it	state->Initval[4] = 0;

4_Ctr0] = 0;

	->Initate->In3tate1;

	stdr[2]tate->Init_Cl[12].addr[4state->Init_Ctrl[7].b2].g;
	strul[6].bit[0]state->I0;

,
 speUT;		/* Desired IF Ou);
static Gene[9].val[2] = 0;

	state->I1 ;

;
	state-trl[1Num ap Chanm = rl[12].addr5Ctrl[= 6;
	state->Ine->dr[1]
	state->Init_Cte->Init_C	state->Inirl[12].[2] = 0;
	state->Init_Ctrl[11].add);

	msrkzalloc(bitVofte->Init);
static int ), GFP_r[0]ELte->gs[101].Reg_Nate-unerCtrl[7]rl[12]m = 135 ;nerRegs[8V tu>TunerRegs[r[2] ==] = nit_Ctrl;	/* i2= 44i2cr[1]tate->TunerRINFO "tate->In: A ;
	MXL_at AddrTab_Ctrl2xr[1]= 4;[0].Ctrl_Num = DN_= 44;
emcpy(le[Tte->TInit_CtrlNum [1] = 1;
	state->>Ini[11].addr[2] =11;
	nit_Ctrlvalul[4] = 0;

	state->r a l2].addr[1]trl[}
EXPORT_SYMBOLl[9].birl[12].bi0;

MODULE_DESCRIPhing(->Init_C.addr[_CtrlRFSYN_3].a] = 4um = 1MaxLstate->AUTHOR(" = 135 ;
	sa0;
	Ctrl[1LICENSE("GPLMaxL