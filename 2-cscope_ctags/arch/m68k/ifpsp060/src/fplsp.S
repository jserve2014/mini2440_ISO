~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocessor Division
M68060 Software Package
Production Release P1.00 -- October 10, 1994

M68060 Software Package Copyright © 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is provided on an "AS IS" basis and without warranty.
To the maximum extent permitted by applicable law,
MOTOROLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
INCLUDING IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
and any warranty against infringement with regard to the SOFTWARE
(INCLUDING ANY MODIFIED VERSIONS THEREOF) and any accompanying written materials.

To the maximum extent permitted by applicable law,
IN NO EVENT SHALL MOTOROLA BE LIABLE FOR ANY DAMAGES WHATSOEVER
(INCLUDING WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER PECUNIARY LOSS)
ARISING OF THE USE OR INABILITY TO USE THE SOFTWARE.
Motorola assumes no responsibility for the maintenance and support of the SOFTWARE.

You are hereby granted a copyright license to use, modify, and distribute the SOFTWARE
so long as this entire notice is retained without alteration in any modified and/or
redistributed versions, and that such modified versions are clearly identified as such.
No licenses are granted by implication, estoppel or otherwise under any patents
or trademarks of Motorola, Inc.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
# lfptop.s:
#	This file is appended to the top of the 060ILSP package
# and contains the entry points into the package. The user, in
# effect, branches to one of the branch table entries located here.
#

	bra.l	_facoss_
	short	0x0000
	bra.l	_facosd_
	short	0x0000
	bra.l	_facosx_
	short	0x0000

	bra.l	_fasins_
	short	0x0000
	bra.l	_fasind_
	short	0x0000
	bra.l	_fasinx_
	short	0x0000

	bra.l	_fatans_
	short	0x0000
	bra.l	_fatand_
	short	0x0000
	bra.l	_fatanx_
	short	0x0000

	bra.l	_fatanhs_
	short	0x0000
	bra.l	_fatanhd_
	short	0x0000
	bra.l	_fatanhx_
	short	0x0000

	bra.l	_fcoss_
	short	0x0000
	bra.l	_fcosd_
	short	0x0000
	bra.l	_fcosx_
	short	0x0000

	bra.l	_fcoshs_
	short	0x0000
	bra.l	_fcoshd_
	short	0x0000
	bra.l	_fcoshx_
	short	0x0000

	bra.l	_fetoxs_
	short	0x0000
	bra.l	_fetoxd_
	short	0x0000
	bra.l	_fetoxx_
	short	0x0000

	bra.l	_fetoxm1s_
	short	0x0000
	bra.l	_fetoxm1d_
	short	0x0000
	bra.l	_fetoxm1x_
	short	0x0000

	bra.l	_fgetexps_
	short	0x0000
	bra.l	_fgetexpd_
	short	0x0000
	bra.l	_fgetexpx_
	short	0x0000

	bra.l	_fgetmans_
	short	0x0000
	bra.l	_fgetmand_
	short	0x0000
	bra.l	_fgetmanx_
	short	0x0000

	bra.l	_flog10s_
	short	0x0000
	bra.l	_flog10d_
	short	0x0000
	bra.l	_flog10x_
	short	0x0000

	bra.l	_flog2s_
	short	0x0000
	bra.l	_flog2d_
	short	0x0000
	bra.l	_flog2x_
	short	0x0000

	bra.l	_flogns_
	short	0x0000
	bra.l	_flognd_
	short	0x0000
	bra.l	_flognx_
	short	0x0000

	bra.l	_flognp1s_
	short	0x0000
	bra.l	_flognp1d_
	short	0x0000
	bra.l	_flognp1x_
	short	0x0000

	bra.l	_fmods_
	short	0x0000
	bra.l	_fmodd_
	short	0x0000
	bra.l	_fmodx_
	short	0x0000

	bra.l	_frems_
	short	0x0000
	bra.l	_fremd_
	short	0x0000
	bra.l	_fremx_
	short	0x0000

	bra.l	_fscales_
	short	0x0000
	bra.l	_fscaled_
	short	0x0000
	bra.l	_fscalex_
	short	0x0000

	bra.l	_fsins_
	short	0x0000
	bra.l	_fsind_
	short	0x0000
	bra.l	_fsinx_
	short	0x0000

	bra.l	_fsincoss_
	short	0x0000
	bra.l	_fsincosd_
	short	0x0000
	bra.l	_fsincosx_
	short	0x0000

	bra.l	_fsinhs_
	short	0x0000
	bra.l	_fsinhd_
	short	0x0000
	bra.l	_fsinhx_
	short	0x0000

	bra.l	_ftans_
	short	0x0000
	bra.l	_ftand_
	short	0x0000
	bra.l	_ftanx_
	short	0x0000

	bra.l	_ftanhs_
	short	0x0000
	bra.l	_ftanhd_
	short	0x0000
	bra.l	_ftanhx_
	short	0x0000

	bra.l	_ftentoxs_
	short	0x0000
	bra.l	_ftentoxd_
	short	0x0000
	bra.l	_ftentoxx_
	short	0x0000

	bra.l	_ftwotoxs_
	short	0x0000
	bra.l	_ftwotoxd_
	short	0x0000
	bra.l	_ftwotoxx_
	short	0x0000

	bra.l	_fabss_
	short	0x0000
	bra.l	_fabsd_
	short	0x0000
	bra.l	_fabsx_
	short	0x0000

	bra.l	_fadds_
	short	0x0000
	bra.l	_faddd_
	short	0x0000
	bra.l	_faddx_
	short	0x0000

	bra.l	_fdivs_
	short	0x0000
	bra.l	_fdivd_
	short	0x0000
	bra.l	_fdivx_
	short	0x0000

	bra.l	_fints_
	short	0x0000
	bra.l	_fintd_
	short	0x0000
	bra.l	_fintx_
	short	0x0000

	bra.l	_fintrzs_
	short	0x0000
	bra.l	_fintrzd_
	short	0x0000
	bra.l	_fintrzx_
	short	0x0000

	bra.l	_fmuls_
	short	0x0000
	bra.l	_fmuld_
	short	0x0000
	bra.l	_fmulx_
	short	0x0000

	bra.l	_fnegs_
	short	0x0000
	bra.l	_fnegd_
	short	0x0000
	bra.l	_fnegx_
	short	0x0000

	bra.l	_fsqrts_
	short	0x0000
	bra.l	_fsqrtd_
	short	0x0000
	bra.l	_fsqrtx_
	short	0x0000

	bra.l	_fsubs_
	short	0x0000
	bra.l	_fsubd_
	short	0x0000
	bra.l	_fsubx_
	short	0x0000

# leave room for future possible additions
	align	0x400

#
# This file contains a set of define statements for constants
# in order to promote readability within the corecode itself.
#

set LOCAL_SIZE,		192			# stack frame size(bytes)
set LV,			-LOCAL_SIZE		# stack offset

set EXC_SR,		0x4			# stack status register
set EXC_PC,		0x6			# stack pc
set EXC_VOFF,		0xa			# stacked vector offset
set EXC_EA,		0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_AREGS,		-68			# offset of all address regs
set EXC_DREGS,		-100			# offset of all data regs
set EXC_FPREGS,		-36			# offset of all fp regs

set EXC_A7,		EXC_AREGS+(7*4)		# offset of saved a7
set OLD_A7,		EXC_AREGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS+(6*4)		# offset of saved a6
set EXC_A5,		EXC_AREGS+(5*4)
set EXC_A4,		EXC_AREGS+(4*4)
set EXC_A3,		EXC_AREGS+(3*4)
set EXC_A2,		EXC_AREGS+(2*4)
set EXC_A1,		EXC_AREGS+(1*4)
set EXC_A0,		EXC_AREGS+(0*4)
set EXC_D7,		EXC_DREGS+(7*4)
set EXC_D6,		EXC_DREGS+(6*4)
set EXC_D5,		EXC_DREGS+(5*4)
set EXC_D4,		EXC_DREGS+(4*4)
set EXC_D3,		EXC_DREGS+(3*4)
set EXC_D2,		EXC_DREGS+(2*4)
set EXC_D1,		EXC_DREGS+(1*4)
set EXC_D0,		EXC_DREGS+(0*4)

set EXC_FP0,		EXC_FPREGS+(0*12)	# offset of saved fp0
set EXC_FP1,		EXC_FPREGS+(1*12)	# offset of saved fp1
set EXC_FP2,		EXC_FPREGS+(2*12)	# offset of saved fp2 (not used)

set FP_SCR1,		LV+80			# fp scratch 1
set FP_SCR1_EX,		FP_SCR1+0
set FP_SCR1_SGN,	FP_SCR1+2
set FP_SCR1_HI,		FP_SCR1+4
set FP_SCR1_LO,		FP_SCR1+8

set FP_SCR0,		LV+68			# fp scratch 0
set FP_SCR0_EX,		FP_SCR0+0
set FP_SCR0_SGN,	FP_SCR0+2
set FP_SCR0_HI,		FP_SCR0+4
set FP_SCR0_LO,		FP_SCR0+8

set FP_DST,		LV+56			# fp destination operand
set FP_DST_EX,		FP_DST+0
set FP_DST_SGN,		FP_DST+2
set FP_DST_HI,		FP_DST+4
set FP_DST_LO,		FP_DST+8

set FP_SRC,		LV+44			# fp source operand
set FP_SRC_EX,		FP_SRC+0
set FP_SRC_SGN,		FP_SRC+2
set FP_SRC_HI,		FP_SRC+4
set FP_SRC_LO,		FP_SRC+8

set USER_FPIAR,		LV+40			# FP instr address register

set USER_FPSR,		LV+36			# FP status register
set FPSR_CC,		USER_FPSR+0		# FPSR condition codes
set FPSR_QBYTE,		USER_FPSR+1		# FPSR qoutient byte
set FPSR_EXCEPT,	USER_FPSR+2		# FPSR exception status byte
set FPSR_AEXCEPT,	USER_FPSR+3		# FPSR accrued exception byte

set USER_FPCR,		LV+32			# FP control register
set FPCR_ENABLE,	USER_FPCR+2		# FPCR exception enable
set FPCR_MODE,		USER_FPCR+3		# FPCR rounding mode control

set L_SCR3,		LV+28			# integer scratch 3
set L_SCR2,		LV+24			# integer scratch 2
set L_SCR1,		LV+20			# integer scratch 1

set STORE_FLG,		LV+19			# flag: operand store (ie. not fcmp/ftst)

set EXC_TEMP2,		LV+24			# temporary space
set EXC_TEMP,		LV+16			# temporary space

set DTAG,		LV+15			# destination operand type
set STAG,		LV+14			# source operand type

set SPCOND_FLG,		LV+10			# flag: special case (see below)

set EXC_CC,		LV+8			# saved condition codes
set EXC_EXTWPTR,	LV+4			# saved current PC (active)
set EXC_EXTWORD,	LV+2			# saved extension word
set EXC_CMDREG,		LV+2			# saved extension word
set EXC_OPWORD,		LV+0			# saved operation word

################################

# Helpful macros

set FTEMP,		0			# offsets within an
set FTEMP_EX,		0			# extended precision
set FTEMP_SGN,		2			# value saved in memory.
set FTEMP_HI,		4
set FTEMP_LO,		8
set FTEMP_GRS,		12

set LOCAL,		0			# offsets within an
set LOCAL_EX,		0			# extended precision
set LOCAL_SGN,		2			# value saved in memory.
set LOCAL_HI,		4
set LOCAL_LO,		8
set LOCAL_GRS,		12

set DST,		0			# offsets within an
set DST_EX,		0			# extended precision
set DST_HI,		4			# value saved in memory.
set DST_LO,		8

set SRC,		0			# offsets within an
set SRC_EX,		0			# extended precision
set SRC_HI,		4			# value saved in memory.
set SRC_LO,		8

set SGL_LO,		0x3f81			# min sgl prec exponent
set SGL_HI,		0x407e			# max sgl prec exponent
set DBL_LO,		0x3c01			# min dbl prec exponent
set DBL_HI,		0x43fe			# max dbl prec exponent
set EXT_LO,		0x0			# min ext prec exponent
set EXT_HI,		0x7ffe			# max ext prec exponent

set EXT_BIAS,		0x3fff			# extended precision bias
set SGL_BIAS,		0x007f			# single precision bias
set DBL_BIAS,		0x03ff			# double precision bias

set NORM,		0x00			# operand type for STAG/DTAG
set ZERO,		0x01			# operand type for STAG/DTAG
set INF,		0x02			# operand type for STAG/DTAG
set QNAN,		0x03			# operand type for STAG/DTAG
set DENORM,		0x04			# operand type for STAG/DTAG
set SNAN,		0x05			# operand type for STAG/DTAG
set UNNORM,		0x06			# operand type for STAG/DTAG

##################
# FPSR/FPCR bits #
##################
set neg_bit,		0x3			# negative result
set z_bit,		0x2			# zero result
set inf_bit,		0x1			# infinite result
set nan_bit,		0x0			# NAN result

set q_sn_bit,		0x7			# sign bit of quotient byte

set bsun_bit,		7			# branch on unordered
set snan_bit,		6			# signalling NAN
set operr_bit,		5			# operand error
set ovfl_bit,		4			# overflow
set unfl_bit,		3			# underflow
set dz_bit,		2			# divide by zero
set inex2_bit,		1			# inexact result 2
set inex1_bit,		0			# inexact result 1

set aiop_bit,		7			# accrued inexact operation bit
set aovfl_bit,		6			# accrued overflow bit
set aunfl_bit,		5			# accrued underflow bit
set adz_bit,		4			# accrued dz bit
set ainex_bit,		3			# accrued inexact bit

#############################
# FPSR individual bit masks #
#############################
set neg_mask,		0x08000000		# negative bit mask (lw)
set inf_mask,		0x02000000		# infinity bit mask (lw)
set z_mask,		0x04000000		# zero bit mask (lw)
set nan_mask,		0x01000000		# nan bit mask (lw)

set neg_bmask,		0x08			# negative bit mask (byte)
set inf_bmask,		0x02			# infinity bit mask (byte)
set z_bmask,		0x04			# zero bit mask (byte)
set nan_bmask,		0x01			# nan bit mask (byte)

set bsun_mask,		0x00008000		# bsun exception mask
set snan_mask,		0x00004000		# snan exception mask
set operr_mask,		0x00002000		# operr exception mask
set ovfl_mask,		0x00001000		# overflow exception mask
set unfl_mask,		0x00000800		# underflow exception mask
set dz_mask,		0x00000400		# dz exception mask
set inex2_mask,		0x00000200		# inex2 exception mask
set inex1_mask,		0x00000100		# inex1 exception mask

set aiop_mask,		0x00000080		# accrued illegal operation
set aovfl_mask,		0x00000040		# accrued overflow
set aunfl_mask,		0x00000020		# accrued underflow
set adz_mask,		0x00000010		# accrued divide by zero
set ainex_mask,		0x00000008		# accrued inexact

######################################
# FPSR combinations used in the FPSP #
######################################
set dzinf_mask,		inf_mask+dz_mask+adz_mask
set opnan_mask,		nan_mask+operr_mask+aiop_mask
set nzi_mask,		0x01ffffff		#clears N, Z, and I
set unfinx_mask,	unfl_mask+inex2_mask+aunfl_mask+ainex_mask
set unf2inx_mask,	unfl_mask+inex2_mask+ainex_mask
set ovfinx_mask,	ovfl_mask+inex2_mask+aovfl_mask+ainex_mask
set inx1a_mask,		inex1_mask+ainex_mask
set inx2a_mask,		inex2_mask+ainex_mask
set snaniop_mask,	nan_mask+snan_mask+aiop_mask
set snaniop2_mask,	snan_mask+aiop_mask
set naniop_mask,	nan_mask+aiop_mask
set neginf_mask,	neg_mask+inf_mask
set infaiop_mask,	inf_mask+aiop_mask
set negz_mask,		neg_mask+z_mask
set opaop_mask,		operr_mask+aiop_mask
set unfl_inx_mask,	unfl_mask+aunfl_mask+ainex_mask
set ovfl_inx_mask,	ovfl_mask+aovfl_mask+ainex_mask

#########
# misc. #
#########
set rnd_stky_bit,	29			# stky bit pos in longword

set sign_bit,		0x7			# sign bit
set signan_bit,		0x6			# signalling nan bit

set sgl_thresh,		0x3f81			# minimum sgl exponent
set dbl_thresh,		0x3c01			# minimum dbl exponent

set x_mode,		0x0			# extended precision
set s_mode,		0x4			# single precision
set d_mode,		0x8			# double precision

set rn_mode,		0x0			# round-to-nearest
set rz_mode,		0x1			# round-to-zero
set rm_mode,		0x2			# round-tp-minus-infinity
set rp_mode,		0x3			# round-to-plus-infinity

set mantissalen,	64			# length of mantissa in bits

set BYTE,		1			# len(byte) == 1 byte
set WORD,		2			# len(word) == 2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,		0xc0			# bsun    vector offset
set INEX_VEC,		0xc4			# inexact vector offset
set DZ_VEC,		0xc8			# dz      vector offset
set UNFL_VEC,		0xcc			# unfl    vector offset
set OPERR_VEC,		0xd0			# operr   vector offset
set OVFL_VEC,		0xd4			# ovfl    vector offset
set SNAN_VEC,		0xd8			# snan    vector offset

###########################
# SPecial CONDition FLaGs #
###########################
set ftrapcc_flg,	0x01			# flag bit: ftrapcc exception
set fbsun_flg,		0x02			# flag bit: bsun exception
set mia7_flg,		0x04			# flag bit: (a7)+ <ea>
set mda7_flg,		0x08			# flag bit: -(a7) <ea>
set fmovm_flg,		0x40			# flag bit: fmovm instruction
set immed_flg,		0x80			# flag bit: &<data> <ea>

set ftrapcc_bit,	0x0
set fbsun_bit,		0x1
set mia7_bit,		0x2
set mda7_bit,		0x3
set immed_bit,		0x7

##################################
# TRANSCENDENTAL "LAST-OP" FLAGS #
##################################
set FMUL_OP,		0x0			# fmul instr performed last
set FDIV_OP,		0x1			# fdiv performed last
set FADD_OP,		0x2			# fadd performed last
set FMOV_OP,		0x3			# fmov performed last

#############
# CONSTANTS #
#############
T1:	long		0x40C62D38,0xD3D64634	# 16381 LOG2 LEAD
T2:	long		0x3D6F90AE,0xB1E75CC7	# 16381 LOG2 TRAIL

PI:	long		0x40000000,0xC90FDAA2,0x2168C235,0x00000000
PIBY2:	long		0x3FFF0000,0xC90FDAA2,0x2168C235,0x00000000

TWOBYPI:
	long		0x3FE45F30,0x6DC9C883

#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_fsins_
_fsins_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L0_2s
	bsr.l		ssin			# operand is a NORM
	bra.b		_L0_6s
_L0_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L0_3s			# no
	bsr.l		src_zero			# yes
	bra.b		_L0_6s
_L0_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L0_4s			# no
	bsr.l		t_operr			# yes
	bra.b		_L0_6s
_L0_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L0_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L0_6s
_L0_5s:
	bsr.l		ssind			# operand is a DENORM
_L0_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fsind_
_fsind_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L0_2d
	bsr.l		ssin			# operand is a NORM
	bra.b		_L0_6d
_L0_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L0_3d			# no
	bsr.l		src_zero			# yes
	bra.b		_L0_6d
_L0_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L0_4d			# no
	bsr.l		t_operr			# yes
	bra.b		_L0_6d
_L0_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L0_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L0_6d
_L0_5d:
	bsr.l		ssind			# operand is a DENORM
_L0_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fsinx_
_fsinx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L0_2x
	bsr.l		ssin			# operand is a NORM
	bra.b		_L0_6x
_L0_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L0_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L0_6x
_L0_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L0_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L0_6x
_L0_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L0_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L0_6x
_L0_5x:
	bsr.l		ssind			# operand is a DENORM
_L0_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_fcoss_
_fcoss_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L1_2s
	bsr.l		scos			# operand is a NORM
	bra.b		_L1_6s
_L1_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L1_3s			# no
	bsr.l		ld_pone			# yes
	bra.b		_L1_6s
_L1_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L1_4s			# no
	bsr.l		t_operr			# yes
	bra.b		_L1_6s
_L1_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L1_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L1_6s
_L1_5s:
	bsr.l		scosd			# operand is a DENORM
_L1_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fcosd_
_fcosd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L1_2d
	bsr.l		scos			# operand is a NORM
	bra.b		_L1_6d
_L1_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L1_3d			# no
	bsr.l		ld_pone			# yes
	bra.b		_L1_6d
_L1_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L1_4d			# no
	bsr.l		t_operr			# yes
	bra.b		_L1_6d
_L1_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L1_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L1_6d
_L1_5d:
	bsr.l		scosd			# operand is a DENORM
_L1_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fcosx_
_fcosx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L1_2x
	bsr.l		scos			# operand is a NORM
	bra.b		_L1_6x
_L1_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L1_3x			# no
	bsr.l		ld_pone			# yes
	bra.b		_L1_6x
_L1_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L1_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L1_6x
_L1_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L1_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L1_6x
_L1_5x:
	bsr.l		scosd			# operand is a DENORM
_L1_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_fsinhs_
_fsinhs_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L2_2s
	bsr.l		ssinh			# operand is a NORM
	bra.b		_L2_6s
_L2_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L2_3s			# no
	bsr.l		src_zero			# yes
	bra.b		_L2_6s
_L2_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L2_4s			# no
	bsr.l		src_inf			# yes
	bra.b		_L2_6s
_L2_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L2_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L2_6s
_L2_5s:
	bsr.l		ssinhd			# operand is a DENORM
_L2_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fsinhd_
_fsinhd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L2_2d
	bsr.l		ssinh			# operand is a NORM
	bra.b		_L2_6d
_L2_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L2_3d			# no
	bsr.l		src_zero			# yes
	bra.b		_L2_6d
_L2_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L2_4d			# no
	bsr.l		src_inf			# yes
	bra.b		_L2_6d
_L2_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L2_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L2_6d
_L2_5d:
	bsr.l		ssinhd			# operand is a DENORM
_L2_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fsinhx_
_fsinhx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L2_2x
	bsr.l		ssinh			# operand is a NORM
	bra.b		_L2_6x
_L2_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L2_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L2_6x
_L2_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L2_4x			# no
	bsr.l		src_inf			# yes
	bra.b		_L2_6x
_L2_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L2_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L2_6x
_L2_5x:
	bsr.l		ssinhd			# operand is a DENORM
_L2_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_flognp1s_
_flognp1s_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L3_2s
	bsr.l		slognp1			# operand is a NORM
	bra.b		_L3_6s
_L3_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L3_3s			# no
	bsr.l		src_zero			# yes
	bra.b		_L3_6s
_L3_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L3_4s			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L3_6s
_L3_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L3_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L3_6s
_L3_5s:
	bsr.l		slognp1d			# operand is a DENORM
_L3_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_flognp1d_
_flognp1d_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L3_2d
	bsr.l		slognp1			# operand is a NORM
	bra.b		_L3_6d
_L3_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L3_3d			# no
	bsr.l		src_zero			# yes
	bra.b		_L3_6d
_L3_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L3_4d			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L3_6d
_L3_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L3_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L3_6d
_L3_5d:
	bsr.l		slognp1d			# operand is a DENORM
_L3_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_flognp1x_
_flognp1x_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L3_2x
	bsr.l		slognp1			# operand is a NORM
	bra.b		_L3_6x
_L3_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L3_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L3_6x
_L3_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L3_4x			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L3_6x
_L3_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L3_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L3_6x
_L3_5x:
	bsr.l		slognp1d			# operand is a DENORM
_L3_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_fetoxm1s_
_fetoxm1s_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L4_2s
	bsr.l		setoxm1			# operand is a NORM
	bra.b		_L4_6s
_L4_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L4_3s			# no
	bsr.l		src_zero			# yes
	bra.b		_L4_6s
_L4_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L4_4s			# no
	bsr.l		setoxm1i			# yes
	bra.b		_L4_6s
_L4_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L4_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L4_6s
_L4_5s:
	bsr.l		setoxm1d			# operand is a DENORM
_L4_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fetoxm1d_
_fetoxm1d_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L4_2d
	bsr.l		setoxm1			# operand is a NORM
	bra.b		_L4_6d
_L4_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L4_3d			# no
	bsr.l		src_zero			# yes
	bra.b		_L4_6d
_L4_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L4_4d			# no
	bsr.l		setoxm1i			# yes
	bra.b		_L4_6d
_L4_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L4_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L4_6d
_L4_5d:
	bsr.l		setoxm1d			# operand is a DENORM
_L4_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fetoxm1x_
_fetoxm1x_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L4_2x
	bsr.l		setoxm1			# operand is a NORM
	bra.b		_L4_6x
_L4_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L4_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L4_6x
_L4_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L4_4x			# no
	bsr.l		setoxm1i			# yes
	bra.b		_L4_6x
_L4_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L4_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L4_6x
_L4_5x:
	bsr.l		setoxm1d			# operand is a DENORM
_L4_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_ftanhs_
_ftanhs_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L5_2s
	bsr.l		stanh			# operand is a NORM
	bra.b		_L5_6s
_L5_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L5_3s			# no
	bsr.l		src_zero			# yes
	bra.b		_L5_6s
_L5_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L5_4s			# no
	bsr.l		src_one			# yes
	bra.b		_L5_6s
_L5_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L5_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L5_6s
_L5_5s:
	bsr.l		stanhd			# operand is a DENORM
_L5_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_ftanhd_
_ftanhd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L5_2d
	bsr.l		stanh			# operand is a NORM
	bra.b		_L5_6d
_L5_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L5_3d			# no
	bsr.l		src_zero			# yes
	bra.b		_L5_6d
_L5_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L5_4d			# no
	bsr.l		src_one			# yes
	bra.b		_L5_6d
_L5_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L5_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L5_6d
_L5_5d:
	bsr.l		stanhd			# operand is a DENORM
_L5_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_ftanhx_
_ftanhx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L5_2x
	bsr.l		stanh			# operand is a NORM
	bra.b		_L5_6x
_L5_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L5_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L5_6x
_L5_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L5_4x			# no
	bsr.l		src_one			# yes
	bra.b		_L5_6x
_L5_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L5_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L5_6x
_L5_5x:
	bsr.l		stanhd			# operand is a DENORM
_L5_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_fatans_
_fatans_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L6_2s
	bsr.l		satan			# operand is a NORM
	bra.b		_L6_6s
_L6_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L6_3s			# no
	bsr.l		src_zero			# yes
	bra.b		_L6_6s
_L6_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L6_4s			# no
	bsr.l		spi_2			# yes
	bra.b		_L6_6s
_L6_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L6_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L6_6s
_L6_5s:
	bsr.l		satand			# operand is a DENORM
_L6_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fatand_
_fatand_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L6_2d
	bsr.l		satan			# operand is a NORM
	bra.b		_L6_6d
_L6_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L6_3d			# no
	bsr.l		src_zero			# yes
	bra.b		_L6_6d
_L6_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L6_4d			# no
	bsr.l		spi_2			# yes
	bra.b		_L6_6d
_L6_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L6_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L6_6d
_L6_5d:
	bsr.l		satand			# operand is a DENORM
_L6_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fatanx_
_fatanx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L6_2x
	bsr.l		satan			# operand is a NORM
	bra.b		_L6_6x
_L6_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L6_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L6_6x
_L6_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L6_4x			# no
	bsr.l		spi_2			# yes
	bra.b		_L6_6x
_L6_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L6_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L6_6x
_L6_5x:
	bsr.l		satand			# operand is a DENORM
_L6_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_fasins_
_fasins_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L7_2s
	bsr.l		sasin			# operand is a NORM
	bra.b		_L7_6s
_L7_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L7_3s			# no
	bsr.l		src_zero			# yes
	bra.b		_L7_6s
_L7_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L7_4s			# no
	bsr.l		t_operr			# yes
	bra.b		_L7_6s
_L7_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L7_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L7_6s
_L7_5s:
	bsr.l		sasind			# operand is a DENORM
_L7_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fasind_
_fasind_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L7_2d
	bsr.l		sasin			# operand is a NORM
	bra.b		_L7_6d
_L7_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L7_3d			# no
	bsr.l		src_zero			# yes
	bra.b		_L7_6d
_L7_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L7_4d			# no
	bsr.l		t_operr			# yes
	bra.b		_L7_6d
_L7_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L7_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L7_6d
_L7_5d:
	bsr.l		sasind			# operand is a DENORM
_L7_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fasinx_
_fasinx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L7_2x
	bsr.l		sasin			# operand is a NORM
	bra.b		_L7_6x
_L7_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L7_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L7_6x
_L7_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L7_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L7_6x
_L7_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L7_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L7_6x
_L7_5x:
	bsr.l		sasind			# operand is a DENORM
_L7_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_fatanhs_
_fatanhs_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L8_2s
	bsr.l		satanh			# operand is a NORM
	bra.b		_L8_6s
_L8_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L8_3s			# no
	bsr.l		src_zero			# yes
	bra.b		_L8_6s
_L8_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L8_4s			# no
	bsr.l		t_operr			# yes
	bra.b		_L8_6s
_L8_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L8_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L8_6s
_L8_5s:
	bsr.l		satanhd			# operand is a DENORM
_L8_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fatanhd_
_fatanhd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L8_2d
	bsr.l		satanh			# operand is a NORM
	bra.b		_L8_6d
_L8_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L8_3d			# no
	bsr.l		src_zero			# yes
	bra.b		_L8_6d
_L8_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L8_4d			# no
	bsr.l		t_operr			# yes
	bra.b		_L8_6d
_L8_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L8_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L8_6d
_L8_5d:
	bsr.l		satanhd			# operand is a DENORM
_L8_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fatanhx_
_fatanhx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L8_2x
	bsr.l		satanh			# operand is a NORM
	bra.b		_L8_6x
_L8_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L8_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L8_6x
_L8_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L8_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L8_6x
_L8_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L8_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L8_6x
_L8_5x:
	bsr.l		satanhd			# operand is a DENORM
_L8_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_ftans_
_ftans_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L9_2s
	bsr.l		stan			# operand is a NORM
	bra.b		_L9_6s
_L9_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L9_3s			# no
	bsr.l		src_zero			# yes
	bra.b		_L9_6s
_L9_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L9_4s			# no
	bsr.l		t_operr			# yes
	bra.b		_L9_6s
_L9_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L9_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L9_6s
_L9_5s:
	bsr.l		stand			# operand is a DENORM
_L9_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_ftand_
_ftand_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L9_2d
	bsr.l		stan			# operand is a NORM
	bra.b		_L9_6d
_L9_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L9_3d			# no
	bsr.l		src_zero			# yes
	bra.b		_L9_6d
_L9_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L9_4d			# no
	bsr.l		t_operr			# yes
	bra.b		_L9_6d
_L9_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L9_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L9_6d
_L9_5d:
	bsr.l		stand			# operand is a DENORM
_L9_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_ftanx_
_ftanx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L9_2x
	bsr.l		stan			# operand is a NORM
	bra.b		_L9_6x
_L9_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L9_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L9_6x
_L9_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L9_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L9_6x
_L9_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L9_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L9_6x
_L9_5x:
	bsr.l		stand			# operand is a DENORM
_L9_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_fetoxs_
_fetoxs_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L10_2s
	bsr.l		setox			# operand is a NORM
	bra.b		_L10_6s
_L10_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L10_3s			# no
	bsr.l		ld_pone			# yes
	bra.b		_L10_6s
_L10_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L10_4s			# no
	bsr.l		szr_inf			# yes
	bra.b		_L10_6s
_L10_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L10_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L10_6s
_L10_5s:
	bsr.l		setoxd			# operand is a DENORM
_L10_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fetoxd_
_fetoxd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L10_2d
	bsr.l		setox			# operand is a NORM
	bra.b		_L10_6d
_L10_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L10_3d			# no
	bsr.l		ld_pone			# yes
	bra.b		_L10_6d
_L10_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L10_4d			# no
	bsr.l		szr_inf			# yes
	bra.b		_L10_6d
_L10_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L10_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L10_6d
_L10_5d:
	bsr.l		setoxd			# operand is a DENORM
_L10_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fetoxx_
_fetoxx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L10_2x
	bsr.l		setox			# operand is a NORM
	bra.b		_L10_6x
_L10_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L10_3x			# no
	bsr.l		ld_pone			# yes
	bra.b		_L10_6x
_L10_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L10_4x			# no
	bsr.l		szr_inf			# yes
	bra.b		_L10_6x
_L10_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L10_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L10_6x
_L10_5x:
	bsr.l		setoxd			# operand is a DENORM
_L10_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_ftwotoxs_
_ftwotoxs_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L11_2s
	bsr.l		stwotox			# operand is a NORM
	bra.b		_L11_6s
_L11_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L11_3s			# no
	bsr.l		ld_pone			# yes
	bra.b		_L11_6s
_L11_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L11_4s			# no
	bsr.l		szr_inf			# yes
	bra.b		_L11_6s
_L11_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L11_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L11_6s
_L11_5s:
	bsr.l		stwotoxd			# operand is a DENORM
_L11_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_ftwotoxd_
_ftwotoxd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L11_2d
	bsr.l		stwotox			# operand is a NORM
	bra.b		_L11_6d
_L11_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L11_3d			# no
	bsr.l		ld_pone			# yes
	bra.b		_L11_6d
_L11_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L11_4d			# no
	bsr.l		szr_inf			# yes
	bra.b		_L11_6d
_L11_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L11_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L11_6d
_L11_5d:
	bsr.l		stwotoxd			# operand is a DENORM
_L11_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_ftwotoxx_
_ftwotoxx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L11_2x
	bsr.l		stwotox			# operand is a NORM
	bra.b		_L11_6x
_L11_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L11_3x			# no
	bsr.l		ld_pone			# yes
	bra.b		_L11_6x
_L11_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L11_4x			# no
	bsr.l		szr_inf			# yes
	bra.b		_L11_6x
_L11_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L11_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L11_6x
_L11_5x:
	bsr.l		stwotoxd			# operand is a DENORM
_L11_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_ftentoxs_
_ftentoxs_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L12_2s
	bsr.l		stentox			# operand is a NORM
	bra.b		_L12_6s
_L12_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L12_3s			# no
	bsr.l		ld_pone			# yes
	bra.b		_L12_6s
_L12_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L12_4s			# no
	bsr.l		szr_inf			# yes
	bra.b		_L12_6s
_L12_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L12_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L12_6s
_L12_5s:
	bsr.l		stentoxd			# operand is a DENORM
_L12_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_ftentoxd_
_ftentoxd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L12_2d
	bsr.l		stentox			# operand is a NORM
	bra.b		_L12_6d
_L12_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L12_3d			# no
	bsr.l		ld_pone			# yes
	bra.b		_L12_6d
_L12_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L12_4d			# no
	bsr.l		szr_inf			# yes
	bra.b		_L12_6d
_L12_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L12_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L12_6d
_L12_5d:
	bsr.l		stentoxd			# operand is a DENORM
_L12_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_ftentoxx_
_ftentoxx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L12_2x
	bsr.l		stentox			# operand is a NORM
	bra.b		_L12_6x
_L12_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L12_3x			# no
	bsr.l		ld_pone			# yes
	bra.b		_L12_6x
_L12_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L12_4x			# no
	bsr.l		szr_inf			# yes
	bra.b		_L12_6x
_L12_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L12_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L12_6x
_L12_5x:
	bsr.l		stentoxd			# operand is a DENORM
_L12_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_flogns_
_flogns_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L13_2s
	bsr.l		slogn			# operand is a NORM
	bra.b		_L13_6s
_L13_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L13_3s			# no
	bsr.l		t_dz2			# yes
	bra.b		_L13_6s
_L13_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L13_4s			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L13_6s
_L13_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L13_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L13_6s
_L13_5s:
	bsr.l		slognd			# operand is a DENORM
_L13_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_flognd_
_flognd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L13_2d
	bsr.l		slogn			# operand is a NORM
	bra.b		_L13_6d
_L13_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L13_3d			# no
	bsr.l		t_dz2			# yes
	bra.b		_L13_6d
_L13_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L13_4d			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L13_6d
_L13_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L13_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L13_6d
_L13_5d:
	bsr.l		slognd			# operand is a DENORM
_L13_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_flognx_
_flognx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L13_2x
	bsr.l		slogn			# operand is a NORM
	bra.b		_L13_6x
_L13_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L13_3x			# no
	bsr.l		t_dz2			# yes
	bra.b		_L13_6x
_L13_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L13_4x			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L13_6x
_L13_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L13_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L13_6x
_L13_5x:
	bsr.l		slognd			# operand is a DENORM
_L13_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_flog10s_
_flog10s_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L14_2s
	bsr.l		slog10			# operand is a NORM
	bra.b		_L14_6s
_L14_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L14_3s			# no
	bsr.l		t_dz2			# yes
	bra.b		_L14_6s
_L14_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L14_4s			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L14_6s
_L14_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L14_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L14_6s
_L14_5s:
	bsr.l		slog10d			# operand is a DENORM
_L14_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_flog10d_
_flog10d_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L14_2d
	bsr.l		slog10			# operand is a NORM
	bra.b		_L14_6d
_L14_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L14_3d			# no
	bsr.l		t_dz2			# yes
	bra.b		_L14_6d
_L14_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L14_4d			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L14_6d
_L14_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L14_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L14_6d
_L14_5d:
	bsr.l		slog10d			# operand is a DENORM
_L14_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_flog10x_
_flog10x_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L14_2x
	bsr.l		slog10			# operand is a NORM
	bra.b		_L14_6x
_L14_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L14_3x			# no
	bsr.l		t_dz2			# yes
	bra.b		_L14_6x
_L14_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L14_4x			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L14_6x
_L14_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L14_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L14_6x
_L14_5x:
	bsr.l		slog10d			# operand is a DENORM
_L14_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_flog2s_
_flog2s_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L15_2s
	bsr.l		slog2			# operand is a NORM
	bra.b		_L15_6s
_L15_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L15_3s			# no
	bsr.l		t_dz2			# yes
	bra.b		_L15_6s
_L15_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L15_4s			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L15_6s
_L15_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L15_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L15_6s
_L15_5s:
	bsr.l		slog2d			# operand is a DENORM
_L15_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_flog2d_
_flog2d_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L15_2d
	bsr.l		slog2			# operand is a NORM
	bra.b		_L15_6d
_L15_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L15_3d			# no
	bsr.l		t_dz2			# yes
	bra.b		_L15_6d
_L15_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L15_4d			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L15_6d
_L15_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L15_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L15_6d
_L15_5d:
	bsr.l		slog2d			# operand is a DENORM
_L15_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_flog2x_
_flog2x_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L15_2x
	bsr.l		slog2			# operand is a NORM
	bra.b		_L15_6x
_L15_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L15_3x			# no
	bsr.l		t_dz2			# yes
	bra.b		_L15_6x
_L15_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L15_4x			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L15_6x
_L15_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L15_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L15_6x
_L15_5x:
	bsr.l		slog2d			# operand is a DENORM
_L15_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_fcoshs_
_fcoshs_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L16_2s
	bsr.l		scosh			# operand is a NORM
	bra.b		_L16_6s
_L16_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L16_3s			# no
	bsr.l		ld_pone			# yes
	bra.b		_L16_6s
_L16_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L16_4s			# no
	bsr.l		ld_pinf			# yes
	bra.b		_L16_6s
_L16_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L16_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L16_6s
_L16_5s:
	bsr.l		scoshd			# operand is a DENORM
_L16_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fcoshd_
_fcoshd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L16_2d
	bsr.l		scosh			# operand is a NORM
	bra.b		_L16_6d
_L16_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L16_3d			# no
	bsr.l		ld_pone			# yes
	bra.b		_L16_6d
_L16_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L16_4d			# no
	bsr.l		ld_pinf			# yes
	bra.b		_L16_6d
_L16_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L16_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L16_6d
_L16_5d:
	bsr.l		scoshd			# operand is a DENORM
_L16_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fcoshx_
_fcoshx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L16_2x
	bsr.l		scosh			# operand is a NORM
	bra.b		_L16_6x
_L16_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L16_3x			# no
	bsr.l		ld_pone			# yes
	bra.b		_L16_6x
_L16_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L16_4x			# no
	bsr.l		ld_pinf			# yes
	bra.b		_L16_6x
_L16_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L16_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L16_6x
_L16_5x:
	bsr.l		scoshd			# operand is a DENORM
_L16_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_facoss_
_facoss_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L17_2s
	bsr.l		sacos			# operand is a NORM
	bra.b		_L17_6s
_L17_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L17_3s			# no
	bsr.l		ld_ppi2			# yes
	bra.b		_L17_6s
_L17_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L17_4s			# no
	bsr.l		t_operr			# yes
	bra.b		_L17_6s
_L17_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L17_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L17_6s
_L17_5s:
	bsr.l		sacosd			# operand is a DENORM
_L17_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_facosd_
_facosd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L17_2d
	bsr.l		sacos			# operand is a NORM
	bra.b		_L17_6d
_L17_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L17_3d			# no
	bsr.l		ld_ppi2			# yes
	bra.b		_L17_6d
_L17_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L17_4d			# no
	bsr.l		t_operr			# yes
	bra.b		_L17_6d
_L17_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L17_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L17_6d
_L17_5d:
	bsr.l		sacosd			# operand is a DENORM
_L17_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_facosx_
_facosx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L17_2x
	bsr.l		sacos			# operand is a NORM
	bra.b		_L17_6x
_L17_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L17_3x			# no
	bsr.l		ld_ppi2			# yes
	bra.b		_L17_6x
_L17_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L17_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L17_6x
_L17_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L17_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L17_6x
_L17_5x:
	bsr.l		sacosd			# operand is a DENORM
_L17_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_fgetexps_
_fgetexps_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L18_2s
	bsr.l		sgetexp			# operand is a NORM
	bra.b		_L18_6s
_L18_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L18_3s			# no
	bsr.l		src_zero			# yes
	bra.b		_L18_6s
_L18_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L18_4s			# no
	bsr.l		t_operr			# yes
	bra.b		_L18_6s
_L18_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L18_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L18_6s
_L18_5s:
	bsr.l		sgetexpd			# operand is a DENORM
_L18_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fgetexpd_
_fgetexpd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L18_2d
	bsr.l		sgetexp			# operand is a NORM
	bra.b		_L18_6d
_L18_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L18_3d			# no
	bsr.l		src_zero			# yes
	bra.b		_L18_6d
_L18_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L18_4d			# no
	bsr.l		t_operr			# yes
	bra.b		_L18_6d
_L18_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L18_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L18_6d
_L18_5d:
	bsr.l		sgetexpd			# operand is a DENORM
_L18_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fgetexpx_
_fgetexpx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L18_2x
	bsr.l		sgetexp			# operand is a NORM
	bra.b		_L18_6x
_L18_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L18_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L18_6x
_L18_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L18_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L18_6x
_L18_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L18_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L18_6x
_L18_5x:
	bsr.l		sgetexpd			# operand is a DENORM
_L18_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_fgetmans_
_fgetmans_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L19_2s
	bsr.l		sgetman			# operand is a NORM
	bra.b		_L19_6s
_L19_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L19_3s			# no
	bsr.l		src_zero			# yes
	bra.b		_L19_6s
_L19_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L19_4s			# no
	bsr.l		t_operr			# yes
	bra.b		_L19_6s
_L19_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L19_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L19_6s
_L19_5s:
	bsr.l		sgetmand			# operand is a DENORM
_L19_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fgetmand_
_fgetmand_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L19_2d
	bsr.l		sgetman			# operand is a NORM
	bra.b		_L19_6d
_L19_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L19_3d			# no
	bsr.l		src_zero			# yes
	bra.b		_L19_6d
_L19_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L19_4d			# no
	bsr.l		t_operr			# yes
	bra.b		_L19_6d
_L19_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L19_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L19_6d
_L19_5d:
	bsr.l		sgetmand			# operand is a DENORM
_L19_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fgetmanx_
_fgetmanx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L19_2x
	bsr.l		sgetman			# operand is a NORM
	bra.b		_L19_6x
_L19_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L19_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L19_6x
_L19_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L19_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L19_6x
_L19_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L19_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L19_6x
_L19_5x:
	bsr.l		sgetmand			# operand is a DENORM
_L19_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_fsincoss_
_fsincoss_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L20_2s
	bsr.l		ssincos			# operand is a NORM
	bra.b		_L20_6s
_L20_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L20_3s			# no
	bsr.l		ssincosz			# yes
	bra.b		_L20_6s
_L20_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L20_4s			# no
	bsr.l		ssincosi			# yes
	bra.b		_L20_6s
_L20_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L20_5s			# no
	bsr.l		ssincosqnan			# yes
	bra.b		_L20_6s
_L20_5s:
	bsr.l		ssincosd			# operand is a DENORM
_L20_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		&0x03,-(%sp)		# store off fp0/fp1
	fmovm.x		(%sp)+,&0x40		# fp0 now in fp1
	fmovm.x		(%sp)+,&0x80		# fp1 now in fp0
	unlk		%a6
	rts

	global		_fsincosd_
_fsincosd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L20_2d
	bsr.l		ssincos			# operand is a NORM
	bra.b		_L20_6d
_L20_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L20_3d			# no
	bsr.l		ssincosz			# yes
	bra.b		_L20_6d
_L20_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L20_4d			# no
	bsr.l		ssincosi			# yes
	bra.b		_L20_6d
_L20_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L20_5d			# no
	bsr.l		ssincosqnan			# yes
	bra.b		_L20_6d
_L20_5d:
	bsr.l		ssincosd			# operand is a DENORM
_L20_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		&0x03,-(%sp)		# store off fp0/fp1
	fmovm.x		(%sp)+,&0x40		# fp0 now in fp1
	fmovm.x		(%sp)+,&0x80		# fp1 now in fp0
	unlk		%a6
	rts

	global		_fsincosx_
_fsincosx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L20_2x
	bsr.l		ssincos			# operand is a NORM
	bra.b		_L20_6x
_L20_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L20_3x			# no
	bsr.l		ssincosz			# yes
	bra.b		_L20_6x
_L20_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L20_4x			# no
	bsr.l		ssincosi			# yes
	bra.b		_L20_6x
_L20_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L20_5x			# no
	bsr.l		ssincosqnan			# yes
	bra.b		_L20_6x
_L20_5x:
	bsr.l		ssincosd			# operand is a DENORM
_L20_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		&0x03,-(%sp)		# store off fp0/fp1
	fmovm.x		(%sp)+,&0x40		# fp0 now in fp1
	fmovm.x		(%sp)+,&0x80		# fp1 now in fp0
	unlk		%a6
	rts


#########################################################################
# DYADIC TEMPLATE							#
#########################################################################
	global		_frems_
_frems_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl dst
	fmov.x		%fp0,FP_DST(%a6)
	lea		FP_DST(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,DTAG(%a6)

	fmov.s		0xc(%a6),%fp0		# load sgl src
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.l		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	lea		FP_SRC(%a6),%a0		# pass ptr to src
	lea		FP_DST(%a6),%a1		# pass ptr to dst

	tst.b		%d1
	bne.b		_L21_2s
	bsr.l		srem_snorm			# operand is a NORM
	bra.b		_L21_6s
_L21_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L21_3s			# no
	bsr.l		srem_szero			# yes
	bra.b		_L21_6s
_L21_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L21_4s			# no
	bsr.l		srem_sinf			# yes
	bra.b		_L21_6s
_L21_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L21_5s			# no
	bsr.l		sop_sqnan			# yes
	bra.b		_L21_6s
_L21_5s:
	bsr.l		srem_sdnrm			# operand is a DENORM
_L21_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fremd_
_fremd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl dst
	fmov.x		%fp0,FP_DST(%a6)
	lea		FP_DST(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,DTAG(%a6)

	fmov.d		0x10(%a6),%fp0		# load dbl src
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.l		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	lea		FP_SRC(%a6),%a0		# pass ptr to src
	lea		FP_DST(%a6),%a1		# pass ptr to dst

	tst.b		%d1
	bne.b		_L21_2d
	bsr.l		srem_snorm			# operand is a NORM
	bra.b		_L21_6d
_L21_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L21_3d			# no
	bsr.l		srem_szero			# yes
	bra.b		_L21_6d
_L21_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L21_4d			# no
	bsr.l		srem_sinf			# yes
	bra.b		_L21_6d
_L21_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L21_5d			# no
	bsr.l		sop_sqnan			# yes
	bra.b		_L21_6d
_L21_5d:
	bsr.l		srem_sdnrm			# operand is a DENORM
_L21_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fremx_
_fremx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_DST(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext dst
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,DTAG(%a6)

	lea		FP_SRC(%a6),%a0
	mov.l		0x14+0x0(%a6),0x0(%a0)	# load ext src
	mov.l		0x14+0x4(%a6),0x4(%a0)
	mov.l		0x14+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.l		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	lea		FP_SRC(%a6),%a0		# pass ptr to src
	lea		FP_DST(%a6),%a1		# pass ptr to dst

	tst.b		%d1
	bne.b		_L21_2x
	bsr.l		srem_snorm			# operand is a NORM
	bra.b		_L21_6x
_L21_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L21_3x			# no
	bsr.l		srem_szero			# yes
	bra.b		_L21_6x
_L21_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L21_4x			# no
	bsr.l		srem_sinf			# yes
	bra.b		_L21_6x
_L21_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L21_5x			# no
	bsr.l		sop_sqnan			# yes
	bra.b		_L21_6x
_L21_5x:
	bsr.l		srem_sdnrm			# operand is a DENORM
_L21_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# DYADIC TEMPLATE							#
#########################################################################
	global		_fmods_
_fmods_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl dst
	fmov.x		%fp0,FP_DST(%a6)
	lea		FP_DST(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,DTAG(%a6)

	fmov.s		0xc(%a6),%fp0		# load sgl src
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.l		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	lea		FP_SRC(%a6),%a0		# pass ptr to src
	lea		FP_DST(%a6),%a1		# pass ptr to dst

	tst.b		%d1
	bne.b		_L22_2s
	bsr.l		smod_snorm			# operand is a NORM
	bra.b		_L22_6s
_L22_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L22_3s			# no
	bsr.l		smod_szero			# yes
	bra.b		_L22_6s
_L22_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L22_4s			# no
	bsr.l		smod_sinf			# yes
	bra.b		_L22_6s
_L22_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L22_5s			# no
	bsr.l		sop_sqnan			# yes
	bra.b		_L22_6s
_L22_5s:
	bsr.l		smod_sdnrm			# operand is a DENORM
_L22_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fmodd_
_fmodd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl dst
	fmov.x		~~~~,FP_DST(%a6)
	lea~~~~~~~~~
MOT,%a0
	bsr.l		tag~~~~~fetch operand type
~~~~~b		%d0,DTAG~~~~~
MOTO~~~d		0x10
MOTOROPerformance M~~~srcDivision
M68060 ~~SRCre Package
Production RROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
M68000SHi-PerforCopyrlght ©%d1

	anditoro&0x00ffeser,USER_FPSR3, 199macl, 199%d0 CopyrighFPCR_MODE-- Octod0	# pass rnd mode,prec
elease P1.00  and wibeProd warrptr to Softwackage
Prodcessor L1d by applicable A DIS	tstright 1
	bnerigh_L22_2dr 10, 199smod_snormM6806ware Pacis a NORM
	braIED,
INCL6d
SS FOUD:
	cmpiRESS OR,&ZEr DiviisOF MERCHAa  any? IMPLITNESS FO3dM6806noINGingement WARzeroM6806yes OR FITNPOSEFOR A PART3CULAR PURPOSE
andINF applanty againsn INFringement with 4egardble the SOFTWARE
(infUD theANY MODIFTWARVERSIONS TH4REOF) and any accQNANanying written mtBLE als.

To the ma5imum extent permitop_sqnanpplicable law,
IN NO EVENT SHA5REOFhe SOFTWARE
(dnNTIES nty againNTABIDELITY D VERSI:

#
#	Resulting now in FP0
#94 MomightEXC_DREGS and ws re303	# restore d0-d1/a0-a168060  LOST
THE SOCitted r Dicrbilisr USE s no rctrl regsorola asxainteUFP1 INABILIT4houtmaintenafp1
	unlk		%a6
	rts

	global		_fmodx_
 use, m:
	lina copy,&-LOCAL_SIZE
IS thessum INFOTO,he SSEY MOARE.	# savermitted.
Moightort osumility for t
es no responsi  retainnce CHNOsupport of t&0xc0entirFPocessoLITYibutfp0/gran~~~
MOTg as thbin anProdS FO nd/oY MOOTibute convert,  Packag input argu
To ARISMOTOROLA DISCLAIMS A Midh mof0x8+0xied verrks of0ersi
ProdextOLA DISmodirademark4d thut aProd09ISINGight~~~~~~8~~~~~~~~~~~~h.
No0, 1994

M68060 Software Package Copyright © 1993, 199maimum extent permitterHETHEry po14atorola~~~~~~~la, Inc.
ge
Produaw,
entry points c.
~~~o one of the branch tranches toOR O lfptop.s:
#	This file NTABppende extent ptopd thahe 0 1993,acos~~~~~~~~lae. Th  All righILITYserved.fied andmitted_facprovi	shoon an "AS IS" basNTABndthe moutanying wr.
Ttent pmaximum extTo tpermitteLL WARRANTIES WHlaw,any patentDISCLAIMS ALLRE
(RATION, WHETHER EXPRPOSE
aingement with Ux the SOFTWARE
(rt	0x000LOSS OF BUSINE INFO MODIFIED VERSxONS THIxEOF) and any acc infFOR ANY DAMAGES W infals.

To the marxmum extent permitted bPARTIClicable law,
IN NO EV000A MI3a.l	_fatanhx_
	shompanying wr
	brn materials0000l	_fasinxOR F	bra.cosdshs_
	rt	0	_fatans_
	short	0IN NO EV	0x00004
	bra.l	_fcoshs_
HATSERSIONY DAMAG0000
ATSOEVER	0x0000
	br000
	bra.l	_fcoshxN,rt	0x0000ERSILOOSE
F BUSIIED 	0x00005
	br	bra.l INTERRUPTION,ort	0x0000
	bra.lteriORMATIOx,s are ra.lPECUNIARYort	0)
thero long antire notice i	_fataTOhe maintenaed without alteration es no	shor
redas sucy modifasinintenaed versions, and that permitted.

You are hereby learbra.tributcosx_ l
#og10scoshx_
	shx	0x00000
	bra.lg10sfcoshx_
	sh0000
	bra.l	_flog10doshs_

# DYADIC TEMPLATE_flog12#og10d_
	short	0x0000
	bra.l	_flog10d_
	short	0x0000
	bra.l	_flog10x_
	shooricens0000 uscaleED Wiogn_
	snd distributeent permitted so lontified is h mode notic	_facions ae_
	short	0altera -- Oi0x00yy.
Tifix000enser
re	bra.l	_fld v Incons,ind_
that such0
	bra.l	_00
	bra.getmaclearly 
or tra.l	_fsx_
	s.
No l
	bra..l	_fmort	0x00by imANTIE -- ,  THEppel or other8060 So one of Mo Divi	braightsg to one of the branch ta.l	_fremx_
	short	0xany patentss:
#	This file is appended to the top of the 060ILSP packaed vM_
	shcrt	0x0000
	bra.l	_flremin
# etmaPackage
Prod00 -- Ocaximum extent permitter 10_
	sho
ight 60 law,
sins_
	shor C
	bra.l	_©acosd_
	short	0x0000
	bra.l	_facosx_
	short	0x0000

	bra.l	_fasins_
	short	0x0000
	bra.l	_fasind_
	short	0x0000
	bra.l	_fasinx_
	short	0x0000

	bra.l	_fatans_
	short	0x0000
	bra.l	_fatand_
	short	0x0000
	bra.l	_fatanx_
	short	0x0003_2le lingemenrt	0xs_
	short	0x0000
	bra.l	_fatanhd_
	short3_6s0000bra..l	_fatanhx_
	short	0x0000

	bra.l	_fcoss_
	short	0x003_3smum extent permitoshx_
	x0000
	bra.l	_fcosx_
	sh	bra.l	_fl3anh_
	short	0x000	short	0x0000
	bra.l	_fcoshd_
	short	3_4_ft	0xox_
	short	0x0000
	ort	0x0000

	bra.l	_fet	bra.l	_fl4a.l	_ftentoxx_
	setoxd_
	short	0x0000
	bra.l	_fetoxx_
	3_50000
	bra.l	_ftwot_fetoxm1s_
	short	0x0000
	br	bra.l	_fl5a.l		_ftan	0x0000
t
	bra.l	_fetoxm1x_
	short	0x0000	brara.l	_fgetexps_
	short	0x0000
	bra.l	_fgetexpd_
	short	0x0000
	bra.l	_fgetexpx_
	short	0x0000

	bra.l	_fgetmans_
	short	0x0000
	bra.l	_fgetmand_
	short	0x0000
	bra.l	_fgetmanx_
	short	0x0000

	bra.l	_f
	bra.l	_flrt	0xdshort	0x0d00
	bra.l	_flog1n_
	short	0x0000
	bra.l	_flog1000
	bra.l0x0000
		_fintrzx_
	shp1_
	short	0x0000
	bra.l	_flog1np1rt	0x0000
	bra.l	_fintrzx_
	shp1ort	0x0000

	bra.l	_fmuls_
mod_
	short	0x0000
	bra.l	_flmod_
	short	0x0000
	bra.l	_flse, mo	0x0000

	bra.l	_fmuls_
remicroport	0x0000
	bra.l	_femd_
	short	0x0000
	bra.l	_fremx_
	short	0x0x0000

	bra.l	_fscales_
	short	0x0000
	bra.l	_fscaled_
	short	0x0000
	bra.l	00
	brs of Mo0000
	bra.l	_x_
	short	0x0000

	bt	0x0000
	bra.l	_fsind_
	short	0x0000
	bra.l	_fsinx_
	short	0x0000

	bra.l	_fsincoss_
	short	0x0000
	bra.l	_fsincosd_
	short	0x0000
	bra.l	_fsincosx_
	short	0x0000

	bra.l	_fsinhs_
	short	0x0000
	bra.l	_fsinhd_
	short	0x0000
	bra.l	_fsinhx_
	short	0x0000

	bra.l	_ftans_
	short	0x0000
	bra.l	_ftand_
	short	0x0000
	00
	brangemen
	bra.l	_fsqrts_
	short	0	_fc_
	short	0x0000
	brIONS C_VO.l	_ftentoxx_
	s0
	bra.l	_fl	_fcoshs_
	000

	bra.l	_fmulsimum extent permitP,		0x0			# frame0
	br_
	short	0x0acked <EREOF)ind_
0000acchort	0x0000

	bra.l	_ftwotoxs_
	short	nx_
	short	0x0000

	XC_DREGS,		-10		0x0			# framewoto0			# oLL ny patentBE LIA	_fabss_
	short	0x0000
	bra.l	_fabsd_
	NG WITHOUT LIMIT0

	_fetoxm1s_
	short	0x0000
	brsend thetai,
x_
	short		bra.l	_flad00
	bra.l	_fnegx_
	short	0xaddN_
	short	0x0000

	bra.l	_fdivs_
	short	0x0000
	bra.l	_fdivd_
	short	0x0000
	bra.l	_fdivx_
	short	0x0000

	bra.l	_fints_
	short	0x0000
	bra.l	_fintd_
	short	0x0000
	bra.l	_fintx_
	short	0x0000

	bra.l	_fintrzs_
	short	0
	br,		HE U00
	bra.l	_flognd_
	short	0x0000
	bra.l	_flognx_
	short	0x0000

	bra.l	_flognp1s_
	short	0x0000
	bra.l	_flognp1d_
	short	0x0000
	bra.l	_flognp1x_
	short	0x0000

	bra.l	_fmods_
	short	0x0000
	bra.l	_fmodd_
	short	0x0000
	bra.l	_fmodx_
	short	0x0000

	bra.l	wise underll dapaet Es~~~~~~~~~~~~~nto the package. The user, fp scratch 1
set FP_SCR1_EX,		FP_SCR1+0
set FP_SCR1_Ss located here.
#

	bra.l	_facoss_
	short	0x0000
	bra.l	_f60ILSP p
	short#ind_
conibutied elognect, branchnxtent p8

set . Thedifyr, in
# effect, branchesl	_foneshort	0xN,	FP_ tES WHfp sies loca0x00nx_
.
#.l	_fmuls_
acos_
	short	0x0000
	bra.l	_flt FPset EXC_A4,		EXC_AREGS+(4*4cos0

	bra.l	_fsqrts_
	short	0asi	0x00r

set EXC_ARN,		FP_DST+2
set EXC_A4,		EXC_AREGS+(4*4sihort	0x0000

	bra.l	_fmuls_
l	_f_DST,		LV+56			# fp destinataFP_DST_LO,		FP_DST+8

set FPx0000
	bra.l_AREGS3 source opera	# stacked vector offset
set EXC_EA,		0xc			# st		0x04
se>

set EXC_FP,		0x0			# frame pointer

set EXC_AREGS,		-6000
	bra.l	_fcoshxl address regs
set EXC_DREGS,		-10r addre.l	_fmuls_
cos EXC_EA,		0xc			# st,		USER_FPSfcoshx_
	s3REGS+(7*4)		# o_FPSxXC_A7,		EXC_AREGS+(7*4)		# offset FPSR_Q	0x0000
	bra.l	_fleEXC_A7,		EXC_AREGS+(7*4)		# oPSR 0

	3

set EXC_AREGS,		-6set USmt	0x0000
	bra.l	_fmulSR_AEXCEPTmx0000
	bra.l_AREGS+(5*4)
set EXC_A4,		EXC_AREGS+(4*4)
nt byte
getexp_
	short	0x0000
	bra.l	_flPCR ro_
	short	0x0000
	bra.l	_flPCR ro0

	bra.l	_fsqrts_
	short	0getmnd
set FP_SRC_EX,		FP_SRC+0
r scra		LV+28			# integer scratch 3m_SRC_HI,		FP_SRC+a.l	_fmuls_
	shhort	0x0000
	bra.l	_flog2d_
	short	0x0000
	bra.l	_flog2x_
	short	0x0000
t	ssin(): 16		 regutSCR0he siset FPa rt	0alizedort	0xg2_
	sMP,		dLV+6			# temporary space

ved de Hi-,		 ope5	and destinFP_D operannd type
set STcoAG,		LV+14	# source operand type

setnRY TECHNOLOGY ved ST	# flag: speND_FLsourceRY TECHNOype

tio	# saPCOd type
set STAG,		 Paced conditioncial cmum (see beTR,rce 4			EXC_Ced currTo tPC (active)		# sHE UEXT codes
set EXC_EXTWare C_OPWORDype
INPUT *
setort	 TEC -- Oword

################################

# Hel are a0 =t, braeP_SCR0rt	0	shorh	_faionoperand type	dcrosroun# offsets w,ra.lORD,ourc_OPWORD,ourceOUd extenved operation word

################################

# Helpful mfpTEMPourcX) orV+2	(X)set FTEMPon
set FTEMP_   For extensi#X):flog2s_
	sF
	br_GRS,		1X,		0ND_FLext1 =ved he SOsion		# sa
set FTEMP_ACCURACY,		LVMONOTONICITY2			# value saved in memory.
set FTEMP_HI,		set FP_SCreturnedmanxexps_
	he min 1 ulport	64 significant bit, i.e.		# sDST_EX,0.500ion
seto 53bra.s ifet ST	0x00		# ssubsequently		# save,	ed000
	ouS WH0			# ext. 
set S_LO,		8
s_
	abl00
	notonic		# sinT_EX,LO,		8
SRsaveD_FLvalue			# vain memoLGORITHM2			# value saved in memory.
set FTEMP_HI,		4
set FTEMP_LO,	alue saved in me
	sh,		LVCOSffseon
set L1. If ec ei0
sevokV+15ved AdjN := 0;S+(2*12)	# the  exponent1ST_HI,#sinx sgl pr2	0x3c|X| >= 15Pi2

sEXT_< 2**(-40), goet S7t SRn codeec exponen3. Ded tyose X
	brX = N(Pi/2) + r wnx_
 |r| <= Pi/4. Le			# sake			bra. 4, soort	particular, 		0x0,1,2,or 3.			# T_Over000
e ka.l	k	# mk +3fe		t SRC EXT_HI,		0x7ffe4
set kset Lven3fe			# 6t SRC_LEXT_HI,		0x7ffe5. (ias

odd) Set j	# m(k-1)/2, sgnOet D-1)**j+2			# T_R# extoper*GN,	r)XT_BIA pre0x02USINpproxi.l	_dved a,		0perved  polynomORD,LOSr, 1			#*r*(B1+s*(B2+ ... + s*B8)),t QNAN,tend*rt SRC_LO,ry.Exitt SRC_Lt QNANCHNOLOGY fo6 STAG/DT		8

et  any,		ias

SNAN,		0x05			# TAG		# sINFourcr)
set U			# o	0x05		x05			#roperand 		# o
# Fx03ND_FLY TECHNOs #
##r#####s*(####
#A: specELITYA7rand #
################
# FPSt SNANperandperan7or STAG/DL 13fe			# 9d type for Svalue savL	8. (|X|<exndedec)set 01EXT_HIin~~~~~p	# ext X;	0x2			#t DBLdbl n_on
s		1lue G
set UNNORM,		0x09.sets w biasX	0x2X	# mX rem 2Pi. Nowognp1lt

se3fff,0x2			#go b0			# ingl			# sa		# zero resulSINent: specBLL_Lrand x3cinf_b	4			#byte


set	0x0			#3fe			# xlue  q_sn0x43fe			# m
s_HI,				3			# underflot	LV+14AG/D	# operanigna
set LOC0			# bit,	# branch 		0x2	GL		# operand07ult 2
sr_bit1_bitHIperan7ff ope			# in		0x2LITYperaalue G
set UNNORM,		0x04set z_bit,		0x2ZER1ow
set esult
j2x2_bj1 (EOR)perats w2), i		4	accrjt,		clusive0					4	et STl.s.b.atiokLV+14	z_bsgn aunflx05			1s #
#l_biauperr_e2_bit,		3	SIN		12= #
#1 *Y TECHNOe			# m#######

2HNOLOGY		# intien3		######		LV+2	###### #
#type
d	# u/FPC Pac	0x2it,		3	R bits #
#l_biNOLOl	_feG,		Lly.6			/l_bit,		6			# accru########
y bit mask  accr			# opeata r#######x###################

# FP###########alion
 masks1nd tand fl_bi#################

	0x2neg_it m (lw)
t ©0lue #gatia,		Lnan bit m (lwV+2			infve bit mask2(byte)
setinfin_
	san bi6infinity uotien0x1		8uotienperandY TECHNOerr7 quotienet unfl_N#########Xividu# nan bit1byte)
		7fl_bi# zero bit 8fl_bi0_HI,		on unordered,		8

na quotienOND_FLndedall	0x0NAN# neg##########bit,		3	otien
set  (ie. not fcmp/ftst)

set EXC_TEMP2,		LV+24			# temporary space
set EXC_T
SINA7:	00
	fp sBD6AAA77,0xCCC994F5 ccru6e bit mask3DE61209,0x7AAE8DA1w exc5e bit mask0E5AE645,0x2A118AE4w exc4p -- Oit m
EC71DE3,0xA534153yte)403		# dz exceF2A01A0,0x1A018B5ve bhoutit m
set z_bex0000402k,		0x00000FF8ion mext ©8ue s accr859AFt,		ek,		0x0000010000nfinitexFC		0x000Ation 	# accru40	9t byteption 

COSB8k,		0x00000s2AC4Dx000D6011EE3ccru_ve bit mask00A9396lleg9F45AC19nderfl operation
E21EEDt byt612C972nderfrflowdzlegal 927E4	0x0B79D9FCFnderf
set adz_m002Flow
set adz_D42set Eeption megal operatderf0x0000vfl_mask5
# FPSRB60#########B61D43 bsu comb,	LV+4ninexacaiops useA#########40		# bsun excB5E#####
set ovons usw#
set u	set		INARG	0x00CRns use+opXask
 dzin#et aovfDCARE,X+2 used inFRAC,X+4_mask
seRPRIME nzi_maskask
seSSRC,e bit un_
	ssk
sePOSNEG1,Lk+accr_mask
TWOTO63et aovflw ex2inENDFLAGet aovods_
s NINT
	brx2_msk+inexADJNet aov3ore (ie. not fcmp/ftst)

set EXC_TEMP2,		LV+24.l	_fmuls_atio
atio:FP_SCR0_S&0,2_ma0000

licabl; SET 2_ma000
a.l	ODIFIEDINBGN_mask
pnan_maskzi_mnx_mast aovfl_mx1ave bit mtion an_maskcos
zi_msk
set sn2a1op2_mask,	s2an_mask+aiop_mask
snan_maskk+snan_mask+aiop_mask
set snaniop2_mask,e bit :
#--SAVE	bra., FP1. CHECK IFoverfIS TOO SMd_
	shorARGt	0xfp scratated ve room fLOADV+vfl_fp scratch 1
seXk,	nan_maons art	0x00t X
# H"COMPACTIFY" XFP_SCR0_Srrp_masdd_
	shut exset Lhiword
FP_SCRwunfl_inxf_mask,	0 Softhi(man)l	_fa	0x00007F excepf_mask,	ra.ls_
	shsins andshort1	_fa3FD7an banying verflow	0x0			#als.gment SOK1mum extentra##

SINSM0
	bra.l;n_mask+is00

y small

fl_bEOF) a	8

ig quot4004BC7Exceptio	0x0< 15 PIals.lfatanSINMAINeption mask
nan bREDUCEX	8

gl_threshxact re81	large
tky THISp_masntireUAL CASE, exceptinfl_b.ean biE ARGUMENT et unEXC_p_maDONE BY T		EXCLOOK UP.,	in			#:fp scratch 1
se%ort	0fmulg: oTWOBYPI(%pc_mask1	# X*2/PI
set FP_PITBL+0x200INCL
searm_man bitOF N*PImask  = -32,...,,	un,hort	0x0et r,INfp scrn_maCONVERTed i0x00GEt	0x000	8

 in mantf_mask,	neke a0000
atioN
	asl	0x004f_mask	0x3	*= 16
	ady000	ent
and_
nd tbl_PSR_ = a5			(N*1pack# A1op_bit,	ADDRPOSrp_.
To BY2
#specWHICHp_maIN TWO PIECES Y1 & Y2
	fsub	#####_1)+ve room fX-Ysk (et
ssperaEX_ve room fxtended = (fl_b)-Y2tw exCONT_mask+antinua extifrom_bit,	Ep_ma--GET N+nf_masND SEEit m####R)
set l bR)p_maNEEDEDofin biissa SRCbiht li
set nximu_mask,	f_mask,	maskoffD0p_maODD,		1	VEN
	ro, 199&1 byt			# D0 WAecmaskIFl    vecNEGATIVExpon	2			# diven_
	shw		COSPOLY0xcc	LET JLD_Ait,	LEAST SIG. BITbyteD0, CONDSG		# m######Jn bion mN WE RETURN	SGN*sexace. neg_bit:  unf2 in UTED BYial R' + R'*Sl_mas+ S(A2agc ex3 bsun 4e for0x3	Set i)),00
	REflgan b=rapccR, S=R*R. 0x4		CAN####REWRITTEN AS	# flag)

set n [A1+T(A3a>
s5+Tet i] + [ exct fm4+TA6))])ial x04		 T=S*Sapcc_NOTon FAT A3 THROUGH A7 	_faSTOt	0xset OU bitPRECIVENT~~~~m ILEen(lr ofA2cond data> <ea-EXTENDED
	shMATund-	# seaE TH
slognp1x_0c,-(%sptky nderfmfp2/fp3ve bit mopesk+ausnan_maskuXp_maRk (by	#t rztes

s excepFPor ofS	short	0x00#####nus-inft 3##############6#########o-plus-inNDENTAL "LASTRANSCEed las1####rm-OP" (00
	,		nan		# _VEC,		_stk	# le,	2r peron mpackSUN_VGs #################000	SIGN POSIbra.
	e fdiv ent
)
set nan_mask,NOW R'c exce(0x0		rmed la_bit####DIV_TA7P packANTS #
#####2##
set 0x0	ormed last#5##############pcc__f64634	# 16381 4FMUL_Oatio2masksun_b#####STANTSadd performed apcc__fl
T1mask,	 per40C62D38,0x cond biDx3D6F90AE,0xB13OG2 LEAD
T381 Let 0,0xC90Fx3D6F90mask (l275CC790AE,0xB1L+dz_0
PIBY2RAIL

PI:	long3####F####>#####0,0xC90FgRAIL

PI:	longL "LA,0xB1Evfl_bi0
PIBY2t	inf_ma: opTWO1#########rmA2,0xt fm############
T1:	long	ons usve room f
set:	long		0

TWOB.l	_#####	0x1###############,+x###############:RAIL

PI:	long		0x4excepbit: ')-R'0x0			#d thammed_+000		# # sanx_
	shor#############x0000
	bra.l	_fmmaintenaR0+0sPoffse	ra.l	_fasi########################S #
 l	_fc- possi	# vegal o####setmask
		t_ nanWORD,####i -- OFLa fadd performed lastsk,		0x08			# negftbit: _fsinfl_bit,		50
PIBERR_VE8(%a6)put
	frfor######bsun_fsinumen+ S'peraPIBY2B: )
	lBegal oBemora inia0x7		an bit LOCA f)+ <eor oft'=it:  ##
	glomda7,%fp0	mask oad sgl P_SRC~~~~~[B###
B		0xB5+TBandi.l		40Bb		%B4IS" 6+TNOLOG:###### fp0/r00 -- d in t%a6)	flB4apcc_vfl_B8_:
	link	 &<bit,		0x1>ation x8(%a6),st
seB20ch oB3)
	leit mask (ch operans
_L0_2, B,		0x-1/2ial r ofp_bit,REFO_2s
	bsr.lASm.l	G		# operand .####0,%d1

	# nan bi3rec

	tst.b# nan bi7
###########SC miaNTAL "LASTIV_OPask+long		0x40000#####75CC7	# 160	_L0_4sfl_binument
#
	fmot unfl_1			s rnd 00001:	long		0x40C62#####.l		t_rformed lastsk (lw)
set nan_mask,Sb		%ddiv.b		%d1,&QNAN		# is oADDand a QN
set n*4)
.b		%d1,&QNAN		# is oMOrand a QN3DAA2,0x2168C235,0x000000B8
 bits ~~~~.b		%d1,&QNAN		#######S1,&INF		es
	bra.b		_L0_6s
_L0_5s:AIL

PI:	long		0x4000000B7

	 fdiv pt
seL0_6s
. #
#####or of,		0x000Fo
	bhort	0x00001,k+aiop_x0000
	br6F90AE, bsunYPI:
	long		0xE~~~~~copyright liceL00,0xC90FDAA2,ROPRO%_6s, OR OTHER PE_facARY LDfsind_,AIL

PI2,0x2168C235		%fpcovm.l	d copyright liceEYPI:
	long		0x basisave d0~~~~t suc,bra.lhe SOFTWARE
	mo
	cmovm.l	dc0,E0303,HE USE ORsind_:# s
	fmovm.x		%C9C883
##################
fp1

	fmov.##########%a6),%f			#  nce and s	IS" basiscopy, ccopyrigspy, cf

	bra.l	_SRC(%~~~~~~ment
#
	fmovr
se
THE SOCR(IZE

	pcc_.b	nput
	fmov.x		%fpd		_L0_5s			# nave fp			# fetch 		_L0_5s			,%fpcr		# zero FPCR
	# lFTWARE.
Mot~~~~~~# no	%TEMP~~~~s.

THE SOCR~~~~~ p0		# load dbl iCR_MODE(0xc0,Emask+aiop_0x8(%a6),%fp0		# lre cgran~~~~~~~lxc0,E0~~~~crb		%INCL mode OR ###########1te)
set	neg_lowPSR combinatmask
setDE(%ae: Big	0x2	fl_b?ial ,		ativ>		t_I0		# USonvert,ENERAL# din an
_bit,		0	
apcc_.l		srct,		3			# unesult
 on un10x2
sBORSexpon###########t3Fbit mast
st0	#  bit mas# dz SMsk
set nanVFLp0,FP_S0x)
	mov.b		%d0,%nd agfatanCOSTINTWOR # in,et STware 0,%fpmay 		# rflhortf&<data0			# extits	0x00		FP_S.
#l		t_ob		% excepsativehandl exthrough cisiDBL fp scration0x2
se.bmask0x08			# 	sho00,fa6),&0000

	bJUST_FP1t,		0x0			# maskass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	topyrigh&a DENORky bit po.l		ssin	is MOvm.lfp scratHE UFP0

	clr.l		%d0ssin			# operand is a NORM
	bra.b		_L0catcht,		0
#fp0/t	0x00s_
HE SOnce and 2d MICxtende1.0G(%a6)hout warranty.
To the mfp0/v	mov%d1,pera~~~~~~~tstSER_FPFP1(%fpcr		# zero Fr.l		ssin			# operand is a NORM
	bra.b		_L0p_6d
perr2dULAR PUSER_FPCRd anyflow
sbmask,		0ar STA?sk,	snan_mask	bramask+,		0)ationFORPOSEINFALIZED Xx2_maEXC_Ab		_L0exl	_fa000	n_mas4000	n_mask+zinf_max00004000iop_mask+,	ov.l		0x8+0x	FP_Sl		0x81,&Q0psr,USlapcc_+0xlk		%,0x8MODE(%a6		&0x0,%fpcr		# zero FPCR
	%d0
	m is a NORM
	bra.b		_L0_6d
_L0#	granted by implCHNOLon, estoppenegatment
#
	lea	+0x4(ut war_mask+aiop_mask
sresset nani4op2_mask,	###########l_mask++0x4(%a6),0x4(%#####nex2_mask+auons usto toperr  misc.add perform
l_biERO		#med last
set9 exceptk		0x04n_bi nan_mix2_m yesa6),CROPROC_ompi. excep####,		8

tion6s
_L0_2s:6	COK
	shmode,		CSM

rr_ma			# no
	bsr.l		t~~~~ded precAN result

se_
	sh~~~~C			# x instr per			# flog	mov.iop_bit,		7			# 
	bra.	bsr. instr per0x00ff
_L0_3d:
	cmpi.b	LATE		nx:
	cmpi.b		%d1 _EX,dxr		#da7_bitTE		l instr peQNAN?
	_L0_5x-to-TEMPLATE			 instr per
set nENORM
_p-min0-a1
	fmotyssind	ytes

s			# s a DENORM
_L0/a0-a1
	fmoty		# yeoperr   len,	6# is olengthmpi.b		%d1  vector off(C(%a6 == EC,		set
set On(ord
EXC_F2ore fpC,		0ration ,ore Y1,ovfl 		# # operaEX		%d1,&QNc# is o+aioact(%a6) ovfl #####F?
	bne########## no
	bCctor################ebit,		t UN.b		%d1,&QN0303,EXC_DREGAG(%a6)
	rc_qnan			# yes# no
	bsr.l		0xd0x00f< 0TE					# yODD00		# w		aved
s
SNODDerandREGISTERS x4(%D SO FAR:vm.l	A00C62D.es
	bra.b		_L0_4s
_L0_3s:
	cmpi.b		N? IMPL	movperr5fnex2_fp scraFonvert, and tag ateriER_FPCRDZ_V* TRA (byte)

s	fmov.l		&0SRC(%a#DAA2rr			# yess
	bra.b		_ER_FP_:nimumert, and tagd# is #r		# ,0x2168C2	fmov.l		&0OSS)
rt	0x00002
	tst.b&ov.b USE THE%dD38,0xD)
	lea		2es
	bra.b		_L0_6s
_L0_52P0
#
	movm.l	8(%a0SER_FPSROLA MICROPROCESx3D6F90AE,0xB1nse to usiSRC(%a6+ntE

	yright lice8(%a6),%fp0d by 7et u# l%d0
	mov.b		FPCR_l or (v.b		%)P0
#
	movm.20		# load dbl 

#
#	cop FPCROCESS#######################	fmov.)	src_qnan			# yes
	bra.b		_L0_6s
_L0_5s:
	bne.b		_03,EXC_DRE####ranted a c
#
	movm.l		a ZERO?
	bne.opyright 		0x300,0xC90FSRC(%a5+1/a0-a1
	fmpyright license to ut FP_Dis o	shoNTABILpsr,USER_.b		%d0,STAG(%a6)s
_L1_3s:
	,STAG(%a6)
	tst.b	cm.x		&0xc0,EXC_FP0(######L0_4s:
	cm	nan_mbra.b		ld_			#.b		_y# save ctrSRC(%a4t_op	_L0		# .b		_L
#
	movm.l	 and tag iQNd disl		ld_e the SOFTWA
	clr.	t_or			FPCR

#
#	e.b		_L1_4s			# no
	bsr.l	4+...# yesient b(%a6)1_6	t_ope4s5

	mov,0x2168C23FFF		%f	# u90			#			3t_opE

	movm. mode,prec

	mov.b		%d1,STAG(&0x0, USE THE		_L1_4s			# no
	bsr.l	3R(%a6),%lS+(4*4		# zero F,		&0xCR(%a6)####
# MONADIC-to-nd don.l		US2t_opHE SOZEROc0%fpcrunlk		%a6
	rts
	clr.HE Se to ution 		_L1_4s			# no
	bsr.l	2ore fp1
	unlk		%a6
	rts

	globaE

	movoperandMON0000

	bra.l	_0,%fpcr	 pass rnd mo0x8type
	m~~~~~~~~~~~~~~~~inp,STAG(%a6)
	t		_L1_4s			# no
	bsr.l	1ore fp1
	unlk		%a6ECHNOLOGY Gr,USgranted by impl		# load db
#
#	ResR'ppel or o
#
rand type
~~~~~~~(%a0 d0-dB
	glB2re fp1
	unlk		L1_6s
_L1_5# zero FS'0000~~~~~~~~~ROPROssi6)
	mov.b		%d0,20
	mov.b		FPCR_Mv.b		FPCR_MODE(%a6USperand is ~~~~~~~~~~,%fp~~~~S# nan 		FP_		sto_co
set E0
	bra. sgl iset LOve ctrl reR IMPLR(%a6) #S(%a6)SER_FPra.b		_L0_6d
_L the fpsr,Umov.b		%d0,STAG(%a6)
	m	%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b	l		&0x0303,EXC_DREGS(_qnan			# yeess
	bra.b		SER_FPl		%d		_L0_5s			# no
	bsrg		0x3FE		&0x0,%fpcr	stoppel or 	&0xc0,EXConvert, and tag i5s:
bra.b		sco		# yeperatR

#
rc_qnan			# yes
	bra.b		_L0_6s
_L0_5s:esereser

THE SOFT~~~~~~SER_FP
	fmov~~~~~~r,USER_FP0 arg

###### no
	bsm.l		EXC_DREGmov.b		%d1,STAG(EXC_DREGS(%a6),&0x5s
	clr.l		%d0
	mov.b		FPCRXC_FP0(%a6 USE THE SO##########ov.x		%fp0,FP	# no
	bsr.l		ld#######################hout waFPCR

#
#	copINs
	fmovm.SER_FPc0,EXC_FP0(%3D6F90AE,0xB1es
	bra.b		globa	t_operr			c	unlk		%a6
	rts

	globa03	ENORM
_L1_6s:1,&.b		_L0_5s			# no
	bsrrc_qnan			# yr					%d1,&QNANdfl_bit,	SER_FPTABI0x3			t_ope6s:3D6F90AEw in1# save ctrl regLOGv.b		FPCR_MODE(%a6),&-LOCAL_SIZE

	movm.l	fmovm.x(%a6)

	clr.l		%d0
	mov.b d0-d1/a0-
_fce,prec

	mov.b		%SER_FPSTAG(%a6)
	ttore d0-d1/a0-a1
	fmovma6)	# nd mode,precand is a NORM
	bra.b		_L0_CR(%a6),%	unlk		%a6
	rts

	globve fp0/va.b		_L1_6d
_L1_2d:
	cmptore fp		%d0
	mov.d_
_fcosd_:
	link		%a6,e d0-d1EMICROPROCESS6,&-LOCAL_SIZE

	movm.l	(%a6),%fpcr,%fpsr yright licensass rnd perand is a NORM
	bra.b	SER_FPR_FPSR(%a6)
.x		&0xc0,EXC_FP0(%a6)	a6)	# 
	global		_fn			# yes
	bra.b	fp1

	fmoss rnd mode,prec

	C,		LV+8			(a6)
	leaanty.
ToRC(%a6),%a0
	bsr.d type
	 a QMEMORY TECHNOL_MODE(%a6),%d0	r.l		scos	6),%a0
	TECHNOLOGY Gsr.l		scos	Res6) # savsr.l		scosd			# oll rnd is a rc_qnan			# yes
	bra.b	ment
#
	fmove.b		_L1_4
	bne.b		_L1_2d
	bsa1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl re11_6:
	b1_3xENORM
_L1_6&QNAN		# is ope INF?
low
set adz_m.b		%d1,&ZERO		# is operand a ZERO?
	uC0_4SER_FPes
	bra.b		_L0	cmpi.b		%d1FPCR

#
#	copsave ctrl regs
xx0303,EXC_DREGS(%a6)	#OSS)
AR(%a6),%fL0_p1
	unlk		%a6
	rts

	globa03ra.b		_L1_6d
_L1_2d:
	cmpi
	short	0x0b		%d1,&QNAN#######_DREGnted a copyr		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl1
	fmovm.l
	shd d0-dd0 must hmpi.b	cr,too.b		%d1,STAG(%a6)
	tst&0x0301rts

	gloSER_FPCR(%a6),%fpcr,%fpsrsr.l		scosd			# 		_L1_6x
_L1_2x:
	cmpi.b		%%d1,&QNAN		# b		%d0,STAG(%a6)hout war		ld_ponra.b	COC,		0LA M			# fetch operand),0OLOGY Gs

########
_L0_3s:
	cmpi.snanUSE THE SOgranted a copyr		%fpcr,%fpsr,USER_FPCR(%a6) # save ctr	bne.b		_L1_2d1_0
	mov.b		FPCt	0x00FPCR~~~~~	0x0 esto.l		%fpcr,%fpsr4USER_FPp0		#0			# no fetc# pass Nb		%d1,& anyUSED,n FLaCODE WILL####VccrueY####SLOWx			# n)
	lc_qnan			METHOD, HOWl	_f, anyMUCH FAno
	a6)	N Ucos				# no
RE			#DEticeSTRqnan			#############sindgetexpx_
i.b		%d1yes
	bra.b		_L036s
_L0_3s:
	cmpi.{fp2-fp5}gumentxponfmov.s	fmG(%a6)
	tsx0303	# )
	lea		_6s
_L0_ER_FPCe SOFT0%a6),If	bsr.####pack		# ybs(argV+2		d0=$ inea6),,0000

	br6) # ot s_mo snanBLE FDBLeBUSINEdanger		# unwanl		sovi.b		%d1n first			#P itbra.b		. r,USl	_f#####accrureduce.b		_L1_2dby o,&INFmaian		 steset SBYTE,value sa,.b		_L1####BLE safe.b		%d1is now in g input anying arg	%d1,&Qousantis_moals.

To tS.b		mum exte
xtendedcreatextesE,0x30303	vm.l		EXCert, anerand is E2_5x:
	opy, converc90fdaa2n_bmask,	HIfp scras_
	sho_bmask,	LO.l		0x8#XC_DREGS		%dhalf		# 2%a6),&2_5s:
#####_bit,				%d1,&Qsinhddc#######n INF?
	be ctrl re2_685a308d3,%d0	# pafpsr # restore ctrl l	1fpcr		# ze	ftestEGS(%ad

	unlkest inex		# y00

	bra	f)
	lea	sred_negE SOFTand P1(%n_bmask,		0NTABIa.l	osEGS+0is o save fp0/a.b		_L1_6psr0xc0,EXscosd			ype
	mov_L1rand isr.l		scosdSTAha.l	_SGL		# 0-d1/a4s:	# inxaftware Package
Pr
	bne.b		ns ara0-at 2
set i0000
	mode,prec

	mo_L1_2d
	bsr.l	row		# yes
	bra.b		_L##########0303	# restordeteis pe
stope by nl res
	br.l		EXC_DREGS(
	clr.l		%d0
	mo_DREGSmods_ative		_L1_dis ophd_:.%a6),ON ENTRYl_masr ofX, O~~~~%d0
egs
	6) # s
	b 303	# exceptioI/d1
	bnnegats
	quotil rewill beR_FPCR ope bit: IP1(%meditR(%a%d1,&Qrc_		0x66-an bsr.l; (RFP_SRn (FP	fmo1)
mode,r.l		ssind			# opl_maR_FPCR(%a+-2**K * FLOGY<= Fd ta rnd modeER_FPCR(%a		%d0
	mnd mode,pstore fp0x0,%		esult
leD0med last
setBILI_3x		0x8+0LOCACR(%a2_3d	0303f_mask,	extenK input argu	gl28a.b		%d1,&QAST.b				_L0x40	#:		%d1,&Qrc27C,		0xd8	extenLx2_bK-27-a1
	fmovm0,_mask+aanch tab)
	mo_mWORK
),&0x40	#EOF)_
	shortrc_ze	EXC_DREGS(	0x0000
	b&(long1,&QNAN		#
trl rovm.lFINDn FLa
	bra.b		_OFCR(%a6)W.R.T.	2**L * (303	). L#####O CHOSEbit: t.b	rl re X%fpcr.l		 /i.b		L) )d tag 29d an ICREATEi.b		%L)rl red:

,s a D(sr.l	)*	0x063),BLE _5ero bra.by2_1), SOF	FPCR_MODE(2FPCR

#
#	cc#####.b		EL1_2ROCESIASEDl	_fnt b303	#n INF?
	bORY TEre fp###############	unlk	*EGS(%a%d0	# pas0xc0A2F983eption et u#########_fsinhx_
_4E44152A:
	bsr.l	k		%a6
	is operaTo t.b		%d0,STAG(%a6)
rand ise forsigt license to

	global		_fsin2&QNACROPROc

	mov.b		%d1,S2	src_z		# y*pyright license ta0-aE M THE		%a6,c_qFP1(%PPCR(dz  El		srxd0######VALU6s
_d1,&QLOAT thePOINT#	copy, con	pcc sAG(%E'S	.l		%fL FP <-->mov.x		%fpa bra)
selEFFICIENT##
	E WAY AROU		_LTnput 
ATv.b	(
	movm.l	%fpcr,%3	+	d
_L -

	movm.l	(%a6),0xpe
	moGovm.l--U_bit,	DESIsr.l	mov.b		 a6)
	lea		~~~~.ec

	mov.babmask,	swapb		%dand type
	mov.b		%d0,STAG	0xc0,EXC_5r.l	nance and sdm.l	00ff00ff,U yes
	brsindGL	bne.b		_L2_2x2_mask	movm.-a1
	f_SRC,	bsr.l		&0x0303,EXovm.lZ, i.b	ALNS T)
	lend a QNsind_:?
	bne.b		_L	&0x0303,EXC_DLITY  FPCR
Nt LOr # t, and restore ctlea	THER P*Pi0-a1
	%d0
b		_L2_5d			# XC_DRE################ warL.l	_ a ZER# restore _L2_####################a0-a1
	fml%a6),%ar.l		%d0
	mov.b		FPCR_M)	# save fp0movmDAA6),%fpcr,%fpsr # restore ctrl 	unlk		%a6
peranda6),%d0	# pak		%ais now 0xc0,EXC_FP0(%a6b		DDconvert, and  THER_MODE(%a6),%d0	a0-a1
	fmovm.A308D######################################
	gan INF?
	bSOFT,&-L	_L2_5d			#	bne.pyrighore d0-d1/a0	bra.lgl i	%d0e	shorreadyperrperXC_DR(R+rgs
	N*P1)
	tst2, P USE THE SOFTWARE.
CR(%a0,%f6),&s THE SOFTWARE.
Moprec

	mov.b)
	le4######4src_zld_pone			# yes
	bra.b		_4)
	lea		FWR(%a*P0x0000
	fmov.l		&0x5######LOG2 LEAD
T		# operandL1_2d
	bsLOG2 LEAD
wxc0,EX

	fmov.l		&0x########
#fp3		&0xc0,EXC_0,%fwe bsr.lP+prand+w  but  |pceptiUSE T			#of PBLE 	hn biwe n0_6d
_Lbsr.l		  Ax2_bR-P  	%d0
 ax2_br-p		_L1_2x
	bsrL2_3d:
	nse to uPicropl		&0x0,		_L0_5s			# no
W-00ff00ff,USE(%a6)
	t########DZ_WARE.
 --a1
	##########h operand ve ctrlnk	(W-P)+we,prec

	mov.b		%d1%a6) # saveA
	bra.b		_L2_###

# d sg SOFT(%a6),			# flog3s:
		# zero FP	# source (A,a)perr "newCR(%a6"		# ineR+rec e+ae th0,%fxact red1/a0-a1
	fmR0
	mno
	bsr.l		t_ope1

	fmov.lVE:perat,%d0	## zero FPCal	# oarra resulfl_bset STd0,%dloovm.x	## any accE THE SOFTWAR exce),%d0	
_L1_4x:
	cmpi.b		ctrl regs
	fm~~~~~~~~~~~~~snd tave 0xc0,EXC_FP0(%a6)	# sL2_2s		# 
sA-R)+a&0xc0,EXC_and t" ba excea0-a1
	fmask,	######n bi1
	bne.b		_L1_2d1_2.l		%d0
	mov.opy, co6)
	mov.b		%d0,%cmode,prec

		%d1
	bne._4ero FPCR

#
#	copx0303nd tag input I
sopy, co	cop vm.lxc0,EXC_Fw in Fe (ie.	sho fcmp/ftst6),%a0	# sa
	brt FALV+2# is otype
set STAG,		,&QNAN		# istarce o6NAN?
	bne.b		t_L2_,%d0	#pecial case (see below)

staoEXC_C_AMAGstore d0-d1/a0CR(%a3_ codes
set EXC_EXTWPLOGY saved in memet un2			# value saved in memory.
set FTEMP_HI,		4
set FTEMP_LO,		8
pset FaTEMP		# yeextenet DBL_LO		_L0_6d
_L4			# valueextend
set DBL_LO,et inex1_bit,		0	C_FP1(%a6),SG# ze
set nmemory.
set SRC_LO,e resultextendaccrud1
	bnextendlow
s8E

	movm.l	G# yeQNAN?
	bne.bLbsun except	link		%a6he SOFAL_SIZE

	mhe SOF	&0x0303,EXhe SOFed prec	# sav DSTore ctrl regs
	fmovm.x		EXC_FP1		4			#30	# restore fp1
	unlk		%a6
	rtDSset F		# is op1d_
_flognp1d_:
	link		%a6		4		&0x030ra.b		_L2_e ctrl regs
	fmovm.x		EXC_FP1 savedx40	# restore fp1
	unlk		%a6
	rt savAL_SIZt, and tag input argument
#
	fmo
set 0x8(%a6),%fpGt,		4			# 
setnfl_bit,xponent,		2			# divmov.b		accrued40set Fbsun exceptifl_bitERO		# RM
	bra.b	00		# restoroffse%d0	# pas02000		# operr dt bsea.l	#########operandotient#############rC_DREG######k,	sna# nan bl_bitinexact result 1

set gs
	a ZE		# bsun excepti#####peratiodd
	cmpi.bmask,		# snan exce# zero ba6),%dx05			T	bra.# zero ER_FPCR(%FPCR
###############
# FPopy, co# yesal funx0303,U/V# negat1,STAG(%UENORM8			# nPOLA ORM
r.l	s*P3	# lag		ar:

#
V.b		_LL3_6Q_4s			# 
# is4(Qegals*Q4OGY G _bmasnd is SR(%a2000		#  now in b		_Lnd iss
	bra.b	ow bicr		3_3d:-cot(rPSR(inc,&QNA###################k,		C_DREa 	_L1_2d3		EXC_DREGS(%a6),&0sopr_inult 2
 a DENORM
_L3_6spcr		ow i an INF?
	bne.b		_L3_4s			# no
	b
	cmpiASER_FPCR(%a6)3_5 operatnmov.b		-C	0x0		sloV/UREGS+t ©l		src_e bit mask		# feb		_Lk,		0x02		C(%a6) nega1,&Ie bit maskgnp1		de,		,		0x02	bsr.l		slX########### masksut,		al operation
00004000	a DENORM
_L1_l		%XC_DRde,	GS(%a6)	# fe				sr0,%fpca DENORM
_L1_(bytel_bit,		rEGS(%a6)	# save d0-dk+snan_maNORM
_L1_6x:

#
#	Result is now in FP0
#
	movTANQ4t ar0
	bd1
	bA0B7.b			F50F8688 impP30
	mov.b		FBEF2BAAf_mas8924F04y implitype
	mov.bmF346Fment
B39BA65	0x0alnhd			#.b		%d1,STauld_p #
##on, estoFF6# load E073D3F		# l99C4Aload sgl input
	fmQtch opl		&0x00x9# load D23CD684set aD95FA1ad sgl input
	fmoa6) opnan_maskov.l		&0x8895A6rl reFB423BC			# TECHNOLOGY GmL1_6x
_L1_4x:
	Dsgl inpbEF57E0D),%fp0BC8CEEXC_DREGS(%a6INVTWOPI# yes
	bra.3mov.l		&0x0eranxrt, a0-a1
	fmoXC_DREGS(%a6) erro# yes
	bra.4001# load L3_4s			#an_mask,		0SR#########opy, ov.b		%d0,STAGDMODE(IZE
o			# sd		####
# FPSR combinat,%d0	0303	# -32_3d	NRO?
3# saveAt, aDlea	TERMe ctEXT.ra.b	TRAILerr			# gs
	fmoSGL. 

	tst.b		PI any64-%d0	LONGa0)	US0x0303	 any.l		%fMOST 69,&0x0x0303.
#
	bra.l	_L0_5x
L0_5x# yes
	bra.C004 input e.b		_L1_2d2168C2mask

1L0_6s
a6),&0x0303	# rest:
	b2C75BCb		_105D7C(%a6)	#
ave 0x0303,EXC_De ctrl re3ded DCis opFF52361put A1ion
0xmov.l		0x8+0x8(%a6),0x6365E2USEREE46ov.l
	fmo4	mov.b		FPCR_MODE(%a6i.b		AFEDDF4(%a6)D3BA9ERM
_A12?
	bne.b		_L2_5x			# no
	SA9A5607			#CC3063Db		_21ov.l		granted a copyright l
35CE1A
	brBB251DCB# is o inp.b		_L0_5s			# no
	bsr9D1462_2x
	AA19D7B-a1
A15estore ctrl regs
	fmovm.x	96CBE3#####990E91A			#21E0xc0,EXC_FP0(%a6)	# save f#083652sd		88034B	_L3_20B0xc0,EXC_FP0(%a6)	# save f8A mem 	0x076F805osd		A18estore ctrl regs
	fmovm.x	83F2677nk		65ECBF7#####1C# is a6),&0x0303	#3M
_L3_6FB53D14			# 9C2F2Cpsr #%a0)
	mo_pone			# yes
	bra.bEEC2D a NOR87AC669	0x0213	mov.b		FPCR_MODE(
	bra.b		231D5FL1_4d595DA7is a 1s
	brrand is a NORM
	bra.b	D5A0D84
	mo437F4E5			#9ov.l		rand is a NORM
	bra.b	re c_5x:movm.l		ort	0x0	# operand is a NORM
	bra.b	SIZE

	movm.l	USE THE SO6		%d0
	mov.b		FPCR_6)
	lea#######################0

	clr		_L1_6x
_L1_2x:
	cmpi restore ctrl regs
	fmo,&INFrand is a NORM
	bra.b	.b		_L0_5s			# no
	bsr.	l		&0 mode,prec

	tst.b		%d1a6)

	clr.l		%d0
	mov.bopy, cand is a NORM		# op1
	unlk		%a6
	rts

	glob1a1
	fmo6x
_L3_5x:
	bsr4_3s	# operand is .l		&0x0,%f d0-d1/a0-a1
	fmovm36s
_L4_3			# fetch operand typeF?
	bne.&0xc0,EXC_FP0(%a6b		movm.l	
	movm.l		&0x0303R(%a6),%fpcr,%fpsr 6s
_L4_3sb		_L0_5s			# no
	bsr.0a6) # saverol regisg input aP0
#
	movm.l		r# restor# operand is a NORMg input e.b		_L1_2d
#
	movm.l		e
#
#	Rol reSER_FPCR(erand ia.b		_L4_6s
_5o FPCR

#
ORM
_L3_4%a6),&4_2s	# load .l		EXC_DREGtore, OR OTd is a DENORM
_L3BFFRM
_L3_6egs
	fmovm.x	mode,prec,%# operand is a N####
# FPSR combina1_5x:
	bsr.l	%a6),&0x400x0303,EXC_D##############################%a6)
############## is 
	bsr.l		scosd			# ope-d1/a0-aol re	and%a6),&0 tag input 
	bne.b		_L4_5s			# noA TO USE THE SOFTW tag input e.b		_L1_2doperand typmov.x		# yes
	bra.b		_L1_6d
_	unlk		%a6
	rts

	globa.b		_L1_6d
_L1_2d:
	fpcr,%fpsr # restorGS(%a6),&0xAL4_6s
_L4_5speran%d0,~~~~~d#######################2b		_L3_4s			# no
		%fp0,FP_S
	rts

	global		_fsinx_
E(%a6),%d0	# pass rre d0-dlink		XC_FP1(%a6),&0x40e d0-d1/a0-a1
	fmol		scosd			#R(%a6)

	clr.l		%d0
	moE(%a6),%d0	# pass rro FPCR

#
#	copy, convert, an2C_FP0(%a6)	# save%fpcr,%fpsrr.l		slognp1			# gumentcmpi.b	estore d0-%fpcr,%fpsrfpcr,%fpsb		%d1
	bne.bA(%a6),%fpregs
	fm NORM
	cr		4	# no
	bsr.l		ld_pone		,EXC_DREGS(%a6)	%fpcr,%fpsrb		%d0,%d1

	andi.l		&ff,USa1
	fmovm.l		%fpcr,%fpsr
	rts

	global		_fsinx_		# no
	bsr.l,%d1_L4_2d
	bsr.,%d0	# pa%d1,&INF		# F?
	bne.b		_t is no%fpcr,%fps_operr			# yes
	bra.b		~~~~~~~~~~		_L2_23_4s			# no
f,USER_FPSR(%a6)

	clA.l		%d0
	mov.b		F%fpcr,%fpsr	unlk		%a6
	rts

	globd is a NORM
	bra.b		_0	# pass rnd mode,prec

	mov.bA	%d1,STAG(%a6)
	tcr,%fpsr # rUSER_FPCR(%a6) # save c		FPCR_MODE(%a6)cr,%fpsr # srt, 	bsr.l		scosd			#fmovm.x		&0xc0,EXC_fsinhx_
_es	%d1,&QNANfl_bit,		6			 FPCR
s
	bsrE(%a6),_fetoxm1sm1x_:
	link		%a6,&-LORC(%a6)
	lea		F###m00

rc_qna4 zero FPCR

#
#	copy, 	&0xc0,EXC_FP0(%a		&0x0,%fpcra6) # save ctrl regs
	m1x_:
	link		%a6,		&0x0,%fpcr	# no
	bsr.l		ld_pone	,EXC_FP0(%a6)	# s&0x0303,EXC	EXC_FP1(%a,%d0	# pas A0-d1/a0-a1
	fmovm NORM
	bra.bRO?
	bne.		sl	andi.l		&
	mov.b		FPCR_MO&0x0303,EXCR_FPCR(%a6),%fpcr,%fps####
	bne.b		_L2_5,USER_FPperabra.b		_L4_a		F	EXC_DRE&QNAN		mask
se	# operand is a NORM
L0_6x
_L0_2x:
	ce fp0/fp1		# o.l		seto_mask+aiop_maskXC_DREGS(%atan
a6),r.l		ssind	# operand is a NORM
	bra.CR

#
#	copy, convert, and tag input e.b		_L1_2d
	3xXC_DREGS(d is a DENORM
_L3_0M
_L3_	bnes

	global		_fetoxm1dd taTANx		&0xc0,EXCTANSM
	fm(%a6) #4e.b		_L4_2%a6),&0x0303	# restorel		setoxm1			TAto-ne yes
	brabal		_fet
_L4_5sovm.l		# is operand a QNAN?
	bne.b		_L0_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L0_6x
_L0_5l		ssind			# operan is a DENORM
_L0_6x:

#
#	Result is now i	_L4FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		nd a ZERO,%fpcr,%fpsr # rest###################################
	globa%fpsr #################################
03	# restore d0-d1/a0-a1
	fmovm.l		USER_FP	&0xc0,EXC_Ffpcr,%fps5	# yes
	bra.b		_L0_6s
_L0_5s:l		_fcfpsr,UATE						<	t_oTAf			# vs
	bra.b		_L0_6s
_L0_3s:
	cmpi.b		SER_FPCR#####################a0-a0%a6),&rr		 an INF?
	bne.b		_L3_4s			# no
	cr		1FP_SRC(%a6),%and an INF?
	bne.b		_L1_4	_L4_
	bsr.l		t_b		%
PI:	long		0x400000SQ4DAA2,0x2168C235,0x00000SP			#sb		%d0,peran00,0xC90FDAA2,Q3+nse to_DREGS(	fmov operand a ZERP2+	bsr.l	erand is a NORM
	bglo(		&0x0	# i2ask
l		slomask		%fpS(E.
Motranty.
To the_6x
d0-d1/a0-a1
	fmb		%x		EXC_FP1k		%a6
	rtsl		&0x0303,EXC_6),&ye			# yes
	ODE(%a6),%d0	# pass rnd		# operand,prec

	mov.b		%d1,STAG(%a6(%a6)

	clrc0,EXC_&0x0303,E

	bra.l	_ fp0/fp# y0x00f	cmpi.b		%d1,&INF		#0GS(%a6),&0R	_L2_2s
	bsrE(%a6),ert, and tag input ape
	mov.
	mov.b		%d0			# no
	bsr.l	ECHNOLOGY R+and an INF?
	bne.,ST##########ranted a copy	#%a0)	#b		_L1_2FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_M#######),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tsdi%a6)	# s0-a1
	fmovr.l		ssin			# operand is a NORM
	bra.b		_L0_6d
_La0-a1
6s
_L0_4s:
	cmpi.b		%d1,&QNAN		rand an INfetoxm1x_:
	link		%a6,&-LOCb		_L5_4s			# no
	bsr.l		src_one			# yes
	branlk		%a6
	nse to uset EXC_GS(%a6),&0xa6)	# save d0-d1/a0-a1
	fmov an INF?
,STAG(%a6)
	mov.b		%d0,%d1

ov.b		FPCR_MODE(%a6),%0x8+05%a6),nd mode,prec

	mov.b	GS(%a6),&0x0e			# yes
	bra.b		_L1_6d
_L1_3d		ssin			# operand is a NORM
	bra.b		_L0_6d
_L0R(%a6)

	clr.l		%d0
	mov.b.l		ssinhmovm.ROLA MICROPROCESS,prec

	tbmask,		type
	mov.b		%d0,STra.b		_L4_6d
_L4_4d:l		ld_pone			#r,%fpsr # restore ctrl 35,0x00000l		%d0
	mov.b		FPCR_a6) # savr,USER0	# past_operr			# yes
	bra.b		_L1_6x
_L1_4x:&0x03_SIZE

	movm.l		&0x303,EXC_DREGS(%a6)	# s movm.	%fpcr,%fpsb		%d1
	bne.b		_L1_2d5operand a	%d1	_fcfl_bit,		6		umenttanmov.b		%
#
	mov		_L0_6s
_L06x
_L1perand a ZERO?
	bns now in F	bsr.lfp0/fro FPCR

#
#	copy, conv	src_zer mode,prec

	tst.b		%d1
5_L4_3a.b		_L4_6s
_L4_5s:
	bsr	#TA####dfpsr # tag			# fetch bsr.l		setoxm1			#cr		# P_SRC(%a6),%a%a6),&0x40	# resx		&0xc0,EXC_FP0(%a6b?
	bne.b		_L4_5x0303	# restorebAG(%a6)
	mov.b		%d4b		_L3_4s			#mov.b		% ctrl regs
	fmovm.x	zero FPCR

#
#	copy, cd0-d1/a0-a1
	fmovm.l########################################a6),%d0	# paSRC(%a6),%a0
	bsr.ibSRC,	is op_L3_6	bsr.l		scosd			#  yes
	bra.b		ed a T0x8(%aload ex		# fetch operand_L4_lk		t argument
#
	fm,&0x40	# restore fp1
	unlk		%a6
	rts

	global		_ftanhd_
_ftMORY TECHNOLOGY G,&-LOCAL_SIZE

	movm.l		&0x0bsr.l		t_operr			# yes
	bra.b		_L1_6x
_L1_4x:
	cmpi.b		%d1,&QNAN		# PC%a6) # save ctrl regs
	m.l		%fpcr,%fpsb		%d1
	bne.b		_L1_2d20/fp1

	fmov.eranfl_bit,		6			NTABILITY OR F	Result d0-d1/a		# load dbl input
	fmov.ss rnd mode,prec

	tst.b		%d1
2%a6),%a0
	bsr.l		tag			# fetch a6),&0x0303	# r restore 1

	andi.l		&0x00ff00ffER_FPSR(%a6)

	clr.l		%d0
	m		# is6),%a0
	bsr.l		tag			DREGS(%a6),&0x0303	# r restore ###########
# MONADIC TEMPLATE				,%fpcr,%fpsr # restor/a0-),%a0
	bsr.l		tt
	fmov.xP1(%a6),&0x40	# restore fp1FPCR(%a6),%fR(%a6)	movm.l		EXC_DREGStore ctrl re0x0303,EXC_DREGS(%a6)	#tore fp1
	unlk		%a6
	rts

b		%d0,%d1

	andi.l		&0x	# yes
	bra.b		_L5_6s
vm.x		EXC_FP1(%al		stanh			# operand is a NORM
	bra.b		_L5_6d
_L5_2d:
	cmpi.b		%d1,&ZERranted a copyright license to uerand 3d			#nd t
	bsr.l		serand a QNAN?
	bne.b		_L5_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		_L5_6s
_L5cr,RO?
	bne.b		_L1_3d			# no
	bsr.l		ld_pone			# yes
	bra.b		_L1_6d
_L1_3dP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USERestoppel or o		%fp0,FP_SRv.l		&0x0,%fpcr		# zero FPCR
XC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_ftanhd_
_ft5_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L5_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L5_6d
_L5_5d(%a6) # save ctrl regs
	m.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl d:

#
#	Result iag inpin FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# recr		ORM
ro FPCR

#
#	copy, convert, and tag input fpsr # restorein FP0
#
	movm.l		x		EXC_FP1(%a6),&0x40	# restoREGS(%	andi.l		&0x00ff,&0x40	# _ftanhx_
_ftanhx_:
	link		%a6,,%fpcr,%fpsr # restor		&0x0303,EXC_DREGS(%a6)	dsave 0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save cEXC_DREfpsr # restorperand is a DENORM
_L3_XC_FP1(%a_inf			# yes0x0,%fpcr		# zero FPCR

#
#	copy, coS(%at, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)5_3d			#	_L3_6%a6)	# save d0-d1/a0-a1
	fmovR_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rndERO?
	bne.b		_L1_3d			# no
	bsr.l		ld_pone			# yes
	bra.b		_L1_6d
_L1_3dORM
	bra.b		_L5_6x
_L5_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L5_3x			# no
1
	unlk		%a6
	rts

	gl		%fpcr,%fpsr,USER_FPperant argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov..b		%d0,STAG(ra.b	t.b	ne.b		_L5_4x			# no
	bsr.l		src_one			# yes
	bra.b		_L5_6x
_L5_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L5_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L5_6x
_RM
_L5_6d:

#
#	Result ix now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# re_L3_6_ is a D0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6)e.b	rz.xes
	bra.br # restore ctrl regs
	fmovm.x		EXC_o
	bsrnd is a NORM
	bra.b		_L4_6x_ftanhx_
_ftanhx_:
	link		%a6,s operand a ZERO?#############################o
	bsr4d is a NORM
	bra.m.l		%fpcr,%fpsr,USER_FPCR(%a6) # save cfmovm.l		USER_FPCR(%a##########################o
	bsr# fetch operax0,%fpcr		# zero FPCR

#
#	copy, coFPCR(%a6),%fpcr,%fpsr # restore ctrl %a6,&-LOCAL_SIZE

	movm.l	USE THE SOFTWARE.
Mot# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FP	&0xc0,EXC_FP0(%a6)	# save fe.b		_L5_4s			# no
	bsr.l		src_one			# yes
	bra.)
	lea		FP_SRC(%a6),%a0
	b%a6) # save	short	0xclr.l		%d0ent
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd 0
	bsr.l		tag			# fet_5d			# no
	bsr.6x
_L5_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L5_4x			# no
	bsr.l		src_one			# yes
	bra.b		_L5_6x
_L5_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L5_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L5b		_L6_6s
_L6_4s:
	cmpi.b30/fp1

	fmov.ort	0x	movm.l		EXC_DREGS(%a6),&0x0303	#
	brREGS(d0-d1/a0-a1
	fmovm.l		USER&0x030	fmovm.l		USER_FPCR(%.l	_ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# rere d0-d1/
	unlk		%a6
	rts

	global		_ftanhx_
_ftan	andi.l		&0x00ff00ff,USER_FP_5x:
	bsr.l		tore fp1
	un d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # sav3 ctrl regs
	fm	%d0,%dtand_
_fatand_:arc40	# restore fp1
	unl		snumbel	_f-a1
	fmo	movm.l		EXC_DREGS		&0x0303,EXC_DR codes
set EXC	# operandFP0
#
	movm.l		EXC	DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_flognp1d_
_flognp1d_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGSfp0,FPra.b		_L2_6x
_L2_4x:
	cmp_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff02######mi	ore fp1
	unlk		%a6
.l		tabuter.l		%d0
	mov.b		FPCR_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L5_6x
_L5_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L5_4x			# no
	bsr.l		src_one			# yes
	bra.b		_L5_6x
_L5_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L5_5x	Sult ODE(%a6),%d0	#6 unfl_bit,1/16 no
	bsrFPCR(mask operand type foeult 	Reseby zpnput 

	glk * 1.xDREGS(%...xd1,STAG(%Notnd a Zl		sr-and aNORM
nd al		src
	bsrDefinpuFl re60-d1/a0-a1
	fmovm1R(%a6)
81			CR

#LOG2 LEADp1
	unlk		%a6
v.b	Xero bdload
-1 attachegs
t0x8+06-th THE Smpi.),%a0
),&06),0x0(#####b sgl 0-d1FE

	(obalX*F)xt q_sn00ff,USER_FXC_DR3. At mask (lwd1,&QNANu)
# FPCR bits #
##R biFPCRset F	movm.l		EXC_DR40OND_FLopb		_L1_4R(%aR bi IMPcopy, conNF?
	bned######C_FP0(%ta	# vofnd tagsx:
	cmpi.bd before0x3	6),&0x40	3,EXC_DREGS(%a6)nd a ZEperand is a D-a1
	# fetch r.l		######################mpi.b		%),%a0
	bsr.l		X00

	bSR0000Rtor oadd perf# load sgv.b		%d0,STAG(%a6)
	m7a6),%fp0	XPIBY-1/Xprec

	mov.b		%d1,STAG'(%a6)
	tst.bmask,		0x02			#lk		%'SER_FPCR(%aA#########s
	fgRM
	*_bit +
	clr.l		%dE

	movm.l		&0x0303,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, conveATAd a ZERO?
	_mask6687RM
_314987D8scosd	#
set dzinfink	AC6-a1
34A26DB3scosd	 no
	bsr.lmasC2471
	unlE1DA28EN?
	bn zero b inputFB3444sd		7F876989e ctrl,		0x0000000FB77_2s	# 7FAF45DB
# MONAegs
	fmovm.FBC71 is n46940220e ctrld1

	andi.l		C249bsr.l921872FP_SR	_SRC(%a0x4(%a6)C_3x					#
#98FA# no
	ba6),%d0	# pasD5R

#,0xR

#
555N?
	bnCpcr		# zero FPCRBFi.l	98539E6A,&0x03L7/a0-a1
	fmovm.lh op 962D1D7Dinhd				fmovm.l		USER_FPCR(%827107Byes
	bCr # restore ctrl regs
	fm6263EER_FPCd0-d7nlk		%a6
	rts

	global	36

P d0-d1set dzinf_mad is a NORM
	bra.b		_L5_6d
_L5(%a0)
	moN	bsr. d0-d7fp0/rand is a NORM
	bra.b		_L5_6d
_L5(%a0)
	mov.ore cER_FPCR(%a6) operand 0_6s
_L0d is a NORM
	bra.b		_Ltore cyes
	bra.86),0x0(%a0)	# load ext input
	mov.l		0x8+scosd,EXC_FP0(%a6)	3FF0-a1
ass rD152,USER060B7A5			# yes
	brach operand type
	mov.BC854 a ZE65498B800		#t FP_ent
#
	fmov.s		0x8(%a6)93BE40r,%fp07626B.b		_k		%a6
	rts

r.l		%d0
	mov.b		BB3078b		_35AEC20ore ctre.b		t
#
	fmov.s		0x8(%a6)A3A69A5_SRC(DDCE7D%d1,&QNAN		#l.b		%d0,%d1

	andi.l	B98E94i.l	62765616),%d0	# pasach operand type
	movB389E5es
	bF9C598p0,FP_p1
	unlk		%a6
	rts

	global	BB797E+0x8(%B09E6F		# no
	bsr.l		src_qnan			# yes
	C367A5C yes39E5F4######nd tag input argu	cmpi.b		%d1B544C6put CFF7D5	copy# no
	bsr.l		t_operr			# yes
D33F62v.s		
488533,&0x0303	# restore d0-d1/a0-a1
	fDB28DA8put 62404C.l		&# no
	bsr.l		t_operr			# yes
E310Ask+dz_8AD34F1f00ff,USER_FPSR5_6x:

## no
	b####AF6B######188EE1E		# no
	bsr.l		src_qnan			# yes
	F2DAF1903,E7DBE79D		%a6
	rts

	b		_L1_2d76),%fpcr,%fABD5810x8(%1D47Es
	bra.b		_L5_6x
_L5_5x:
	p1

	fmov346AC2put a959ECC
#
	movm.l		_ftanhx_:
	link		%7_4dB232A0			#304282D&0x0303	#7C_FP1(7%a6),&0xet adz_m92 IMPL			#D29AED38,0xSER_FPCR(%a6),%fpcr,%fpsr # reACF	bra.b	5CCD1CB	bne.b		_L4_3x			# no
	bsis operaA29E763ight4954F23x8+0x4(%a6),tl		EXC_DREGS(%a6),&0xA68C5+dz_m8AB852_SIZESER_FPCR(%a6),%fpcr,%fpsr # rB22DFFFb		_9D539F303,E3_6d:

#,0x0(%a0)	# load ext i9EDEF a ZE3E900y, c8+SER_FPCR(%a6),%fpcr,%fpsr # rC1A85F	%d1	C75E3.l		&0x0303,EXC_	# no
	bsr.l		ld_pon95D1BNADIC
8138DE_4x:
	cmpi.b		%d1,&QNAN		et adz_mD is oright 40D2DE.l		sasind			# operand is a DENORD8B4B2Bde,prBC05Eode,pR_FPSR(%a6)

	clr.l		%d0
	movE0572Ampi.b	&0x3and isAG(%a6)
	mov.b		%d0,%d1

	andi7F32A7x00ff09CAA8tag input argument
#
	lea		FP_SRCEF8884303,E64ECE.l		acR_FPSR(%a6)

	clr.l		%d0
	movF7170A2%a6)ECC0666a6),&0x0303	# restore d0-		_L6_6s812FD	Resac332DAD	fmov_2s:
	cmpi.b		%d1,&ZERO		# is 8A8D1BCR

#
8E4D6	bne.b		_L4_3x			# no
	bs		_L6_6s9012AB is a 3E4AEd1
	br.l		t_operr			# yes
	bra.b		_76CC3,EXC:11E7
	bra.br.l		t_operr			# yes
	bra.b		_EB6894r,USE889A22		_L7_6d
_LM
_L5_6x:

#erat_L6_6sA5EF72Ci.l	4487361,STAG(%a6)
	mov.b		%d0,%dnd is a xD1700#####F07 and operand a ZERO?
	bnoperand is a B42CBCF is aD37EFB regs
	fmovm.x		EXC_FP1(%a6),&0x40B303 an sr0BA80F8	bra.b		_L6_6s
_L6_4s:
	cmpi.b7_2C2211	bra.bFCAEBBiult is now in%a6),&0x0303	# restor8FEF3	&0x0863312

#
#	,%a0
	bsr.l		tax0303	# restorFCnhx_S(%aBCR(%CCHNOLr			# 
	rts

	global		_fatandD6807Ar			#102C5Bobal		_fatand_
_fatand_:
		cmpi####D2399Bloba31252e ctrl7_6x
_L7_5x:
	bsr.l		sasind		E3B2A8x00ff6B8FC5xact berand isGS(%a6),&0x0303	#7_6EA2D7FPCR(%643159
_L7s operand a ZERO?
	bne.b		_L4_F3BF5BEGS(%BAD1A21a0
	bsr.l		tag			# fetch a6) #.
Mot1CE39%d1,&D205C9ENORM
_L1_6s:ndi.l		&0x00ff00ff,US630A2CULARDA1ED0s
_L6_2s:
	cmpi.b		%d1,&ZE# save ctC1ADss rndF3E09B8store cd type
	mov.b		%d0,STAG(%a91DB8F1d is 4F350ETAG(%a6)
	mov.b		%d0,%d1
_FPCR(%a677314D38,0de,p531

	andi.l		&0x00ff00ff,USER_FPSR(%CE1C8	&0x0A0B8CD#####t EXent
#
	fmov.s		0x8(%a6),%A22832D sgl iDAAEz#####s rnd mode,prec

	mov.b		%d1,746FD38,0xB76022	USER_s rnd mode,prec

	mov.b		%d1,C3EC(%a6),997DD6tore ctre.b				FP_SRC(%a6),%a0
	bsB110688SRC(EBDC6F(%a6) # save cts		0x8(%a6),%fp0		# 5BCC4	_L3_ no
	bBl		src_qnan	
	mov			# fet1
	unlk		A4		t_#####470782d
_L7gs
	fmovm.x		EXC_FP11
	unlk		EA941regs
FD049ARM
	bra.b		_L3_6s
_L3ff,USER_FPSR(C2EB4A#####66162#######AL_SIZE

	movm.l		&0x0303,EXC70BD5operaE602EE1	bne.b		_L4_3x			# no
	bs	bsr.l		sb		_5%d1,&ID200	ment
####
# FFP_SRC(%a6)
	lea		FP_D48457D03,ED8EA4E_SRC(%a6),%a0
	mov.l		0x8+%a6),%fpcB948DAi.b	12DECE			# no
	bsr.l		src_qnan		v.b		FPCE2385LOG2 L69E809P1(%a6),&0x40	# restore fp1
	unlk	E877112a6),C43532src_qnan			# yes
	bra.b		_L6_6s
_LEE57C1ORM
_0D379C%a0
	bsr.l		tag			# fetch  zero FPC3E102 THE S87C377mov.x		%fp0,FP_SRC(%a6)
	lea		F
	F919039b		_758B8D40
	mXC_DREGS(%a6)	# save NORM
	braE0USERnce an935FB%a6) # save ctrl regs
	fmRM
_L3_6s155FB
	mov7B685D0	bne.b		_L4_3x			# no
	bsRM
_L3_6sXC_D_
	sp149D108E is a DENORM
_L8_6s:

#
# restore f9CFa6),%f511D724,STAG(%a6)
	mov.b		%d0,%dRM
_L3_6s7952EC	0x0FF8131E# operand is a NORM
	bra.RM
_L3_6s9732FDput 9557pcr,#	copy, convert, and tag M
_L3_6sB38CA		%a601932Ak		%a6
	rts

	.b		FPCR_MODE(%a6),%dCE7A8C_FP1301EE6Binx_
_fasinx_:
	link		%a6m.l		%fpcF46e ctrl 2EAE52e ctr%d1

	andi.l		&0x00ff00ff,USE922DA7Da6),&11
	bstore a6)

	clr.l		%d0
	mov.b		FPCR4D19v.s		#DEDF52 FPCR

#
#	copy,8convert, ne.b		_L573AB9regs
19D	andr.l		%d0
	mov.b		FPCR_MODE(%a6),%9965_3d%d1,&8E10the SOore fp1
	unlk		%a6
	rts

	globB773F9	andi2321x0303	ore fp1
	unlk		%a6
	rts

	globD55CCore ctF9356bsr.l		s fp1
	unlk		%a6
	rts

	globF10057		&0x06CC57rand a ZERO?
	bne.b1

	andi.l		&0A0b		%5_3d:D970		#ndi.l		&0x00ff00ff,USER_FR(%a6),%f22659sr,USEBC0xc0,M
	bral		&0x0,%fpcr		#_DREGS(%a6)388B4
_L7_F6EF0ECmov.x		%fp0,FP_SRC(%a6)
	R(%a6),%f4D35
	bra.61D292
	bne.b		_L4_3x			# no
	bsZERO?
	bne0895DlobaFBperarc_qnmpi.b		%d1,###################7		EXC_# ye367BE4x:
	cmpi.b		%d1,&QNAN		# R(%a6),%f83A515k		%a95616%d0	# pass rnd mode,prec

R(%a6),%f93A20_DREG753954nhd			# o			# yes
	8C_FP1(8#
	globaA9E72 a ZE023B26M
	b,%d1

	andi.l		&0x00ff00ff,USEAC4C84######FE4Dperand_3x			# no
	bsM
_L8_6d:

#
#	RDCE4A ctrl 06B971x			# no
	bsr..b		%d1,&INs

	globaF2A2DR OTH8D263C9r,USER_FPCR(%a6) # savd:

),%a0
	B0656Fe ctrF2sr,Uscosdanted a copyright license to u184651S(%a6F71P1(%a8_6d
_L8l regs
	fmovm.x6) # save28AAAs			#6F9ADde,prec

	mov.b		%d1,STAG(%a6)
	tstB37B44F	0x03766B8fp1

ode,prec

	mov.b		%d1,STAG(%a6458C33_4s	E9630440	# res_SRC(%a6),%a0
	mov.l		0x8+B5255
set n562246Ba0
	bsr.l		tag			# fetch %d0
	mov.bE2CCA0-a15F9D88Ctch operand type
	mov.b		%d0,STAG(692	tst6)

	CA1oad sgtore d0-d1/a0-a1
	fmovm.l		USE736AEm.l		A6925##########	# ope		# is operand a ZEROCFAB%d1,&7E9F7B3t_operr			# yes
	bra.b		_L1_6x
_LB85ECCload CB2198rnd mode,prec

	mov.b		%d1,STAG(%a68E4FD5p0		20A59x0,% fetch opERO		# is operand a ZER99F41s
	fm4AFF9B	%d0,%d1

	andi.l		&0x00ff00ff,USEBA7F1Eext i842BBE		0x8%a6),&0x0303	#8&-LOCA8r.l		srcB4712_FPS17637E1FPCR_a0-a1
	fmovm.l		USER_FPCR(%8_3xFAB#####i4788D%fpcx0 operand a ZERO?
	bne.b		_L8_3C9D0F	EXC_DRbsrDd is a DENORM
_d0-d81

	fm.b		_L8_3D3pi.b	rc_qn1ECD8err			# yes
	bra.b		_L1_6x
_L1_4x:DB6C73put 856AF1
	cmp operand a ZERO?
	bne.b		_L8_3E31CAse to 2E80Da ZERO?
	
#	Res fetch operP1(%a6),&0A2
	bra6),03194a.l	_fcUSER_FPCR(%a6) # sa.b		_L8_3F0B10	bne.b03128ve d0-d			# fetch operand ent
#
	fBF6B7	_fta.DACB7M
	bral regs
	fmovm.x		&0xc0,EXC_FP0(C4E,%fp opeFA18%a6),&0x0303	# restore d0--a1
	fmov0181BM
	br8B89A45bsr.l		src_qnan			# yes
	bra.b		
C065B)	# saCFBF64d
_L8b		_L5_4s			# no
	bsr.l		src_onAE34%d1,&56340A	%d0,STAG(%a6)
	mov.b		%d001000000		#tan9	%a6
CBl reent
#
	fmov.s		%d0,STAnzi-a1
	1x_:
	a6),&0		#mask
set,.b		sr.lIlink		%a6,&LO,X+SS)
	nan_mTANFfpcr,%fpf5s			# no
	HI%fpcr	e SOFTWARHE USLOand tag mov.nan_maske anEGS(######d mode,pr	# sa(X),  fp0/b		%dFINITE,INF?-ll d,ra.b	NOT
set'S
ec

	k		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# soperand is a DENORMxm1			# opera is a D		%d1
	bne.b		_L1_2d4	%d1,&QNAN		#p1
	unlkFB operand is a N
	fm&INF		# isHE Ux		&0xc0,EXCHE Ul		&0xregs
_L0_4x			# no
	bsr.l
set 	cmpi.b		%d6 ra.b	,prec

	tsx		EXC_FP1(% ctrlIG_FP1(%aE b		_LLIKELYQNAN?
	bne.IN [
	fm
_16)		%d1,&QNAd1,&Ire PaIQUw in ,USEIDEAfpcr,%cr		5_3dmov.l6)	# sov.l [X-F] / [1+XF] )		srcSOov.lF oper	USERe d0BE CLOd isO Xra.b	 rl regs
		E	bsr.l		sr,US_L6_6s, %d0,ed by imn_mas APPROXIMss r inpuU)	mov.b		%det neg_bm/(nk			USER_	%d0,(REMEMBER	andi.lbal		_fta).oad sgx0303RUtst.b		g DIVIDE6s
_L4_4sxd0ZERO	UTgrestond is a Dn			FORsr,USL9err		cmpiNO EY SHORT et mNOMI%d0,and_
_f
	brXa.b		O
	movETmpi.TE				AV	# nOF	bne.bo
	bsd1

	an%d0,HIb		%C_DRosd	w in  opera.			# HE Eand_
b		_L4_bov.b		Fsr.l		scosd			#AREGSFTEM0xAL	%d1,&Esr.l3_4ALSOst.b		e			# HE USE OR SCHEM		t_opernd is a DE_L3_4s			X) DIRECTLYL5_3d:by impO_3x		A RAN?
	bC_FPsrc_qnan			f00ff,US#src_znk		%9)	shoh opBECA)
	tstEGS(%a6)9_5s.l		&0x0303,EXC_5_3d:
	VOLV	tst	cmpi0303o FP zero Fb		%d1,EXC_reshovXe fp+-2^sr,U1.BR_FPCR....B <-tand+ 63CR(%a
_L9_5sNAN?
 Fcopy, cEXC_FP0(%a6)	#1zero FPCRxc0	T MATCHE_bit,	EXPON_3d:l reFIRST 5CR(%a.x		%a0)	# operaXTHpcr		#	bneE		# fBEtand(%a6)
K	%a6
	rts
fmov, 3scos		ad sgl inONLY 8 TIMES		%d= 2^7,%d128 |F|'Se.b		_L5bal		-|F|6),%fe.b03,EXis	unlP1(%a6RM
	braGS(%a# operts

	glob.NAN?
	ER_FPx	_L2_2s
	bsrS(%a6)	0,%a6,&FP0(%a6)	 a NORM
	brb		_L1_4s		x0303,.l		t_operr			# impl6-8(%a6) negatd1/a0-a1
	f(%a6),%fpd1/a0-ap1d_
	sht pe)	# sass r_SIZE

F####################perand a QNXr			# yes
	bra.b		_L

	fmov.l				*F
#
		tst.b		X*F >cmpi########6)
	mov.b		%d0,%E

	mov-Fbsr.l		src_qnan			# yes
	bra.#######ed last
opy, convert, and tag ZERO?
	XC_FP1(%a6),&0*Fand is l		slsult ch operaf_masK	# nr		#_L0_P1(%ac0,EXC.b		%d1
	bstore ctrlovm.l		U(%a6)		%d0ad sN9C_FPFr.l		rand is a# load sgl	%d0

#
#	Result is now in F2_6d
d2.l	_fORARILYE(%a6),%d0	# paRre fpsin			#0-d1/16M
	bra.b		USER_FPCR(%a6),%f8	fmov.l		&0 VARY	# n
	bra.b	F'Sx0303	# rtype
	mov.b		%d0s rnd moch o input arrgumsr # restorel		ld_ponregs
	fKe SOa0, 199
	unlk		%&0x40	# lk		%a d0-# is7anted bDEN.b		_t
#F0x8+0x4(%a) # save 4g inpu FPO TBL	# yfpcr,%fpsr,t FP_USE THE.l		EXC_Dasinx_
_fasinx_:
	link		%a6,&-LO5_4s:
	cmx_

#
#	copy####HE US,%a0
	bsr.l		tag6),0x0(%a0)ave d0-d1/a0-a1),%a0
	bsr.lb		%d1,&
	brQNA_L6_6s
_##########################3s			#	%d0, a DEl re.l		. AG_5s:
 is a NORM
	bra.b		_L5_6d
	globF)b		_L1_4 THE0,EXC_FP0(	# savees
	bra.b	e.b		_L*psr,USER_FPCR(%a6) #x		EXC_FP1(%tag inpm.l	r.l		src'ut
	fsI H_6d
TO DO%d1
	NOW	# yed0-dALASa0)	#  operand iST_3d:CRANbal	!d1,&ZU now Ppi.b	y
PIB is opADYmpi.RC(%a6)
_ftarr		d sgl Ur.l	1*U*Vg_bi +ire RGS(%VC_DRr,%fU*Und_
_ftatand_
_ftanM an &OK Sve fGEerr			 regs%fpcHELnlk	CORx_:
x			# no
NATU			#l		sCR

#
+ n			# mpi.bO		# is oA~~~~GS(%aAT0
#
 inpu fp0/			# RELY	ASOFTA3,%a6)= A1/ER_FP
	fmo2/A3x			# no
REAS	EXC_FE

	moREARgumen0_3d:_FP1(%MAK,EXC_F
	brPperaN,USER	bsrS_qnan		M
	brHE SOFet mSTUFF) d1,&Z	%d0-BALANCNADIC TEMb		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	pi.b		%d1,&QNAN		# is o0303	# rescosd		d a ZERO?
	bnno
	bsr.l		t_opent
#
	fVDAA2,0x2168C235,0x00000	bra.+V d0-d1/a0-a1
	fmovm a ZEU*nd_
		# no
s
	bra operand a e SOF9	bsr.l9	satanhR

#
#	c_FP0(%a6)	# (%a6		_L convert, and tag 	src_qn		_L1_6xC_DREGS(%%fpcr,%fpsr0303	# restore		# no

	gl ovm. nvert,

#
#	cop			# yes
	bra.b		_L1_6x
_L1_4x:
	cmpi.b		%d1,&Qmode,prec

	mov.b		l		ld_pone			# yes
	br0,EXC_FP0(x8+0x4(%a6),0xsd			# operand is ctrlCHNOLOGYative bIN####%a6)p0/fp1
	mov.# re,####			# 	%d1,&nc.  Alll rebne.b		_/16stor operand ivm.l		&0x0303,EXC_DREGS(%a6)	#fp1

	fmovnted a.EtrapcR_FPSRxc0,EX#####a6)	# type
	moa6),&0x40	# r^x		&0xc0,EXC_FP0AS ANSWER. Oe.b	WIts w	mov.l		0x8+0x4(%a6),0xBY Xmovm.Yperan
	br20x0303######0x0305+Y*B6LOGYd_
_fta
	clr.l	# yes
	b 	braZPCR(%Z*B5di.l		x0303,E

	48(%a6)] 	# pass rndYglob*Xr.l		sZ = Y*YGS(%bsr.l		setoxm1			# opem.l		EXC_restoR(%a6),0)
osd			tand_
_ftar.l		setoxm1			#######
	unlk		%a6
	rts

	global		_ftanhx_
_ftanhxmovm.regs
	f####################
# MONADIC TEMPLATE				,%fpcr,l		0x8+6),&0x0303	 ctrl d a ZERO?
	bne.b		_Lpi.b		bsr.l		tag			# ,0x2168C235,0x00000s:
	ce.b		_L6_3x			# no
	bsbute%a6),%d0	# 		# fe is n-d1/a0-aon, estero FPCR

#
#	_FPCR(%a6) # ,USER_&0x0303CR(%a600/fp1

	fmov.et0x8(%a6), is operand a QNAN?
	bnre d0-d1/a0_DREGS(			# fe10l re
	cmpi.b		%d0
	mov.b		FP convert, and t FPCR

#
#	copstore d0-d1/a0ndi.l		&0x00ff00ff,USER_			# operand is andi.l		&0x00ff00ff,USER_FPX&0x030ndi.l		&0x00ff00ff,USstore d0-d1/a0-+.l		%d0
	mov.b		FP)	# save d0-d1/a0-a1
	fmoves
	berand is a DENORM
_L3_INF		# is 1

	fmEXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&ODE(%a6),%d0	# pass rnd mode,prec

	mov.b		l		ld_pone			# yes
	bra.b		_L1_6d # save ctrl regsDIore ctr		%a6
	,EXC_FP0(%a.b		_L9_	perr			# fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov####################################################
	global	pe
	mov.b		%d0,STAG(%a65_3d			# no
	bsrsr.l		scosd			# movm.l	i.b		%d1,&QNAN2^(10&ZERO		# is	globX)*_6x:
 -%a0)	)s rnd mode,p_6x
_LEXC_Fsin			# p0,FP__DREGS6
	r)global		_fetoxs_
40rand aent
#
	fmov.sHU_6d
	0x8nd is a DENOR9_6+0x8(	lea		FX'+X'0(%aC&0x03Clk		%CP0
#
)	# lC5LOGY Ga.b		_L0_,sd			#'*X'	_L3_4s		d1

	andi-.l		&0x00ff00f%d1,&INF	 [6d
_L0Cnk		Cted by imp.b		_C4oppe,slognp1			# 
	bra.b		_L0_6s
_L0_3s:
	cmpi.b		%d1,&INF	6),%fpcr,%a0-a1
	fmovm.l		%d0,-),&0y, convertoperand is#######6
	rgumen# restore ctrl regs
	fmoe,prec

	mov.bcr		9a6),&0x40	# X'	ld_pone			# yes
	bra.b		_L1_6set UNNORM0_6s:

#
#	Re##############
# EALLYR

#		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L5_5x			# no
	bsr.l		src_qnaC		%d1
	bne.bc0,EXC_FP0(%a6/a0-a1
	fmoext input
	mov.l		0x8+0xs a 		src_zero
	bsr.l		ld_ponosr.lo FPCR

#
#	Cfmovm.l		USER_
	fmovm.psr,USER_FPCR(operand an INFO		# isDonvert, and tag input ar			# yes
	#####a1
	fmovm.#	Result is###########
# O		# is (%a6),%fpcr,%fps FPCR

#
#	copperr			# yes
	(%a6),%fpcr,%fpsr # restor' is nrts

	global		_fatand_n			# yes
	b+operr			# yes
	bSRC(%a6)
	lea############	fmoverand is a DENOR3x			# ...	0x0303,EXC_DREGS(%a6)	#40	# resto/a0-a1
	fmor,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&	_fatanated herplgumepo03	#g

neg,%a0		_L5_2d
	b:

#:
OLA MICROPestoppel o_6d
_L	tst.b			_L5_2d
	bpsr # n			# operand is a ORM
	brnp1			# o_6x
_Ltore d0-d1/a0(v.l		0 QNAN?
	ument
#nted mpi.b	s
	bsr.l	mov.b		%d0		%d1,STAG(%a6)
	tsthu.
To EXC0		#ow in FP0
#d1

	andi.l		&0x00(%a6),%fp0		# load dtore d0-a0)	n			# operand is a NORM
	bra.bb		%d1
	bne.b		8(%a6),%fp0		# loarestore ctrl regs
	fmovw in%a6,&	clr.l		%d0
	mov.b		FPCR_gd mode,prec

	ed a ov.l		0x8+0x4(%a6),0x&	tag			# fetch oper	bsr.l		 a NORPSR(%a6)

	clr.l		%d0(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%ource oc_qnan			# yesi.l	_sSTAG,		LV+14	# source operand _L7_2d:
re fp1	USER_FPCR(%a6s:

#
#	Re_6x
_0_ codeinexac# savedWPTR,FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a

	glpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_flognp1d_
_flognp1d_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x030 restore fparc1
	unlk		%a6
	rtL,&ZERO		# is operand a ZERO?
	bne.b		_L1_3d			# no
	bsr.l		ld_pone			# yes
	bra.b

	gl.b		%d0,%d1

	andi.l		&0x00ff00ff,U	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%6bne.b		_L4_2sr # restore ctrl regs
	fmovm.x		Es rnd #####unlk		%a6
	rts

	global		_ftanhx_
_ftanhx_:
	link		%6
	fmovm.l		USER_FPCRpil re TEMPLATE							#
########egs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a0-a),%a0
	b

	global		_fsinxASx2_bi		&0x0303,EXC_DREGS(%a6)fe			# l		src_ dz_bit,		2			# divl		sl_L6) RM
	1

	fmQNAN?3d:
	(%a6),%d0z
	brsqrt( [1-X]nlk	] NAN?
	b			# rand is= e and x / z )E(%a6),%d0t z_bit,		0x2			# zero resul	bne.bro bit mask (by	bsr.l			src_zero			# yes
	b
	cl=d1

	USER_F		_L2_2s	fmovm.xx8+0x8(%ore 
	fmo_bit6),&0x4# infinity bit mask estore ) Gen a ZG,		# is lid##########
by 0 *z_bmask,		bsr.l		src_qnan			# yes
	bra.b		_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, conve	_L5_2d
	bs+aiopgs
	 is operand an INF?
	bne.b		_L6_4s			# no
	bsr.l		spi_2			# yes
	bra.b		_L6_6s
_L6_4s:
	cmpi.b0,FP_DREGS(%a6),&0x0303	# resto,prec
m1		4_4s: 			# the Ssr.l	d# ex_5s:
fis nowc'x_
	QSP. Originae,pre
#
	mall t########### would
#
#	copglobaC_FPs
_Lcauit,			# opes a NORMwhicha6),&0
 exts opecau.l	_until gen_erand ()1,&IN,%d1
	bz bierand a Qs being
~~~~~~cSER_is odis a ce and ult 2
ssr,USER_FP# is1_4nd aeen
#
#	copy,0er FP_re ceads a %a6),&0 QNAN?
00ff,U)

	edglobal		_fetoxs_
_fFP1(%a6),&0x40	#	bra.b	_FP1(%a6),&0x40	# restore fp1
	unl mov.x	R(%
	unlkerand X / SQRT( (1-X)),&0REGS)

# zero5_6x:

#
#1
	unlk		%a6
	rts

	g	src_zero			# yes
	br1-1(%a6nd a ZERO?ODE(%a6),%d0i.b		}STAG(%a6)
	mov.b		%d0,%d1_
_ftand1/a0-aGS(%a6),&01+	# no
	bsr.2

set DST,		0rant_:
	l.l		t_operr			# yes
	bra.b		%a6
	rts6
	ra.b		_L0re fp
	fmoore d0-d1/
	tsy, convert, and tag X/ORM
	bra.b		_L0_6d
nd a ZERO?
	0_6d
_L0bal		_ftad0
	mov_6x
_L		oper~~~~
	bra.l	_fatans_
	sh	# zero FPCR

To thc

	mc0,EXC_FP0(c,%spnlk	ods_
 mode,prec

######stack # save ctrl reg# is o		_L5bsright license|X|
	f# noC(%a6)
ptoxs_:
	link	ligta6
	wareestore	&0x0

TH	bsr. ctrl reg1gumenLA MICR, ##########+-6)

	ccosd	ONb		_L10_5d			6)

	clr.l		%d0


#
#	copy, converPSR(%a6)

	clr.l		%d0
	mov.b		bne.b		_L			# operand is a NORM
	bra+-1e ctrl l		src_#############
_L0_3s:
	pushrgument
e ctrl -FMa.b		_L0P_SRC(%a6)
	lea
	bn#####,&0x40	# x0303	# restore d0###################
	globalXC_FP1(%l		USER_FPCRbsr.l		src_qnaetch operREGS(%a6)	# save d0-d1/a0,%fp0		# load sgl input
	fmov.x		%fp0,FP_SR# operand is a,%d1

	andi.l		&0x00ff00ff,USEe
	mov.b		%d0,STAG(%a6)
	mER_FP##############	%d1
	bne.b		_L6_2s
	bER_FP		satan			# operand is a NORM
	bra.b		_L6_6s
_L6_2s:
	cmpi.b		%%a0
	bsr.l		tag			# fetch operand EXC_DREGS(%set EXC_CMC_DREGS(
set EXC_EXTWORD,	LV+2			# sal#	copy, c_ftanhx_
_ftanhx_:
	link		toreol riS(%ano
	bsr.l		szr_inf			#RM
_L10_6s:

#
#	Re_4x			#egs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		% is 	rts

	global		_fatand_
_fatand_:
	link		%a6m.l		&0x030# fetch opera1(%a6),&0x40	# r FPCR

#
#	copy,m.l		IZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fm6s
_L4_4s:
	cmbal		_flognp1d_
_flognp1d_:
	link		%a6cr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXCc0,EXC_FP0(%a6)	# save fx8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clffsetra.lstwotox			ent
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea	
	movm.l		&0x0303ACOS.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0ext ins a NORM
	bra.b		_:
	lmode,+QNAN?
	b			# ,EXC_FPunlk*stoppel_L0_6z)6_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L6_4d			# no
	bsr.l		spi_2			# yes
	bra.b		I	# l> 0h operand0. On bit of	bne.b		_psr,0
#
	movm.l		EXC_DREGSL6_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L6_4s			# no
	bsr.l		spi_2			# yes
	bra.b		_L6_6s
_L6_4s:
	cmpi.b110/fp1

	fmov.lotox		movm.l		EXC_DREGS(%a6),&0x0303	#11ass rndrek		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	#LL WARck
	leaw/ upper303,frap0,FP_Sperr),%a0
	bsr.l		taN?
	bne.b		_L.l		EXC_DREGS(%fpsr,USEch operand type
	mo_2x:4_4s:
	cm
	link		%a6,&-LOCAL_SSTAG(%a6)
	ts# nan bitx0303d1
	b
	fmovm.x		6),&0ted a coCOSight license to u ctrl  zero otox	DE(%a6),%d0	# p the S	_L5_negright licenseOFTWArec

	mov.b		%d1,STAG(%a6)
OFTWAy, convert, and tag 		scosd			#n			# operand ia6) #
	fmo		scosd			#short	0x0000
	
_L0_3s:
	cmpi.o0x4(zr_t
	fmov%d1
	bs_
	short	0x	# no
	bsr.l		src_zero			# yesb		%d1,&QN	shoZERO?
XC_FP1(%a6),&0x40	# restore fp6
	r
	andi.l		&0_FP0(%ov.lORM
	bra.b/n			# xpon restore ctrl regs
	fmovb		%d1,&QNAN		# is opyes
	bra.b),&0x40	# d mode,prec

	mov.b		%d1,S		# fpded		%d0
	mov.b		FPCRperr	0
	bra.:
xc0,EX######	USERstoppel 	movm.l	err			# yes
	O		# is operero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0xr,USER_F# nan bit oper####t
	mov.l		0xre fpi undzero FPCRort infL11_als.pass r####d0
	movRM
_-1
#
# FPSe d0-OF BUnFPCR

_FP1(11_5:

#
#ZERO,%fpcr,%fpOLA MICROPROCEfp sc####				#
###############fp sc mode,prec

	mov.b		%d1,STAGra.b		_L7_6d
_L7avm.l	bne.b+2
se:t is now in FPet EXC a NOatan	N?
	PARTICF?
	swo the zero FPCRS FO################rt, an# nan bitb		%dts

IZE

	movm.l		&0xi.b		%d1,&QN
	fmovm.x		EXC_FP1(%a,%d1'EXC_FP1(%a/a6)
	ts%fpcr,%fpsAN?
	bne.be ct.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L5_5x			# no
	bsr.l?
	bne.b		_L3_5s			# no
	bs is XC_CC,		LV+8			# savex%a6),%3d:
# no#########################et EEXC_CC,et EXC_Dent
#
	fmov.s		0x8(%a6)wotoxx_:
	link		%a6,&-L	Let E fetTE							#
#####sr.l		scosd	vm.l	 1x8(%a6),%fp0		# load sgle,prec

	mo1

	fmov.l		&0x0,%		# yes
	bra.b		_L1_6d
_L1store # braord

,&QNAN		0
#
	movm.l		EXC
	tst.b		%d1
	bne.b		_L6_2s
	bsr.l		satan			# operand is a NORM
	bra.b		_L6_6s
_L6_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L6_3s			# no
	bsr.l		src_zero			# yes
	bra.b		_L6_6s
_L6_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bneexpovm.x		r,USER-	bnex0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x_FP0(%a6)	# save fp0/fp1

	fmov.l		0.8r # restorore fp1
	unlk		%a6
########################################
# MONADIC TEMPLATE							#
R OTHbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov Pacngeml		srZERO?
303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%#### regs
k		%a6s rnd mode,p-et EX	&0x0,%fpcr		# ze1.	t
se####_HI,	x0,%%a6),%d0	# pass rnd mode2.#
# FPS	l reg2_l re+b		_L2_2s.
Mot2
To #####
#c0,EXd a :set Iad1,&Qalway&INFr_qnan	e.b		_s a NORM--CR(%a62_d1

	andi.l		&0x00_DREGS(XC_FP1(12_	global		_faet EX######################## FPCR

#Fil	movoovm.ltreml		ssepass rt	0x0000

	br########1.m.x	a6),%d0	r,USE2s
	/a0-a1
	fmo1.,		7			# 1.2	GEXC_FP0(%a6)	# sass rnd ,%fpcr		#psr 380 log(6,&-/a0-a1
	fmo.l		&	# loa4),%d0	# pass_flognp0,EXC_FP0(%a6			#ual0x00ffsh		# nts
	b81		N,	FP_SCR1.1 ->
Moton, fetch .To avoiargum# yes a floatingd1,&Q##src_qrisra.linput as#########epshorntvm.l		Ed:
	cmis# yedRC(%	0x8# sostoreC_FP0(%32b		_L	bra.b	ult 2
mode,p(mno r/a0-a1
	fmo)303,RM
	C_DREGSranted .b		% Pacnch ed by%a6),%dfielR

#
|X|;ntednlk		%lREGS(.b		%d10
	bs6d
_16 movm. argd:
	cmpi#########lk		%(inclu:
	cm&0x030ANTI
	bra.)#########|X|. Co.l		0		# lunlk		%a		# vr,USER_FP IMP		&0d0
	mo Pacn INca#####ce and estoppelbyis operarand an INF1,&I		_Llso
	bsrC(%a6),nde,p.l		szr#
	global		_ov.b	BILITY O6),&e fp.b		_iNORM2s
	bsrc.l		szrnow RC(%usR(%abal		_ftby imp,%a0
	mov. guar####nlk		b			mov.l		0x8+0x4(%aRC(%a restono harm6d
__inf	
	mov.b	# pas input		st		scos	# inex%fp0		lesset an,	 thecl	0x0tounlk		% TEMPLATE				icatioftanhx_:
	link		%9	mov.akord
ind tag input argument
#
sin			# oprest_SRC(
_L0neamain
	br ctrl 64/og2sa6),%the 2.10x3		AdjFlagx2_bi (indicae
set STXC_FP0(%stoppeld1,&QNAw		0x5_6x:

#zC_DREG2.2			# m FPCR

#
#	copy, conb		_L argument
#
	lea		FP_SR3R

#
#	cop	Ja QNAN?
	64;d1/a a c aiop_0,FP_ the Sor 6l		src_perand2.4:
	link		%aMCR

N - J)/(%a6),%rest64M + JR_FPCR(%a5R

#
#	copyL0_6dSR_Q.b	######L8_5	&0xresto of1
	unlk2^(J/64_3d:
	_FPCR(%a6	C4s			# raE

	movSt	0xs
	bsMd1,STAG(is a NORM
	:
	cmpi.fmovm. 2.20	# reC_DR.l		src_qns a N the SZsk,		0# no),%a0
	bse ctrl  operand is a NORM
	bra.b		_L5Z5_3dmask (lw)#############XC_FP0(%tore c_bit-0			# ext(restore e fp1
	unl############Ui.b		a	%d0,%d1

	andi.lEXC_FP0(% mode,s_:
	liPCR(%a6)aced v. Ao			# yeSCR0%d0	#3_4s		R(%a6)

	clr.l		%de and "XC_FP0(%"	mov.b1(%a6),3bsr.l		scomode,pZ_:
	lb		_L12#########?
	bne.s(	bra.b	)*(1+eps), |epsa6),%a0-aD38,0MORY TECHNOLOGY G sgl error h	&0xs
	fmo-a1.b		PCRmovmrBILITY OR 3#
####################
# MONADIR

#
#	copyX)
	ts%a6)/64E(%a6),%d03.1	s
	br	tstN*L13s:
	cXC_DREG	# ineL accrP0
#
	movm.l		EXC-L1 now r			# ye3.wotoxsoRNORM
_2R(%a6),%fpcr,3FE45s:
	c		%a6ro FPCR

#
#	copy, - Lb		%movXC_FP0(%)12_2dway%d1, Pack2ative host NOnsures L1+Lgs
	egs
	 operand a Q6) # save/a0-a1
	fl		_8#########accurac&0x0303,b)l		&0_FPCR(%a6 beG(%a6)),%a0
	save ctTEhan 22		%d1,&QNAN	# sa6),&0CR(%a6),%fpcr,%f4		%d1d1,STAG(%cch oped0-d1/a0-a1
X+########		&0xR

#
#	uE

	#
######bra.l is opobal		, Rt inex	0x8#
##yl		s(EXC_F_6d
_fud1
	bne.berand is a	bsr.l		s) Istoreipe
sne.b	estoperand #how,%a0
	bx40	|R| ERO?
SERaf

	clr.l	##ov.l		&0x0x		&0xc0,EXC_s rn=		EXe d0_FP1(%*	bra.b		s now i.b		AN?
	<=.  All .b		_L:
	link		%a6,&-LO	=	eranf,	|fa6),%0.		# ope:
	link		%- N	=	f - eps*Xrestore e.b		_L
	bra.b	 now 		%a	fmovm.la6),%a0
# no
	bsr.l		src_one############ow######
# M44e.b	f2RC(%u
set no
	bsr.l		src_one|0-a1
	fmovm.la6),%(0.5 +v.x		%/	# p8))FPCR(%a6)e d0-d1/a0-a1
7	# paregs
	#####d sgl bSRC(%d1,&QNAN		_L2_5d			# n0,EXC_FP%a6)	# save d0-d1/a0E

	movm.l		&),0x)-1C_FP0(%a6)	# sav(%a6),%dnver inpu*QNAN		ind tby im0,STGS(%0,STerand*A6x:

rand is a NOa)_6s
_r####tob		_L1_2			# nx030essEGS(%a6oeffics
_Lsr.l		scosdmadFPCR "r

se"
	br# operan:),%d(%fpsr is 1/6,&-A8+0x8(%13_5A50
	bsx			# v.b		%d0,%;%a6)EGS(%30
	bs4			# rand and_
_flognp1d_:
	linkb) #####bra.b		_Lmainrive d0s abovort	 .l		sre SO - (s

	glob)########68.8)e SOFTllss rnis op006no
	bs&0 fp1
	unlk1
	fmov) # ligh is bigr,USER_FP
	bsr.l		src_zd is a DE_FPCR(y utioperax0303ipeline, pd1,&Iepa
	br#
##to0
	br		w40	#de		# 	0x00iec)	# sain FPly eq%a6
	rmplexiti

#
#	co0
	mo[ve ctrl # fr.l		*A4) ]	+%a6),%d0	[ ######### now inS		# o ]############bal		_fte)
set nan_bmask,		0x01oR(%a6),Cb		%d0,fp0,FP_S*s

	gls
	bs	bra.b	(1+p	USER_ the SO	EXC_DRTNG,	 T*p .l		nd imask (lw)
T#
####1

	fmov.fp0,FP_SRC(%9_2x:HE USE OR
	fmovC_FP0(%k		%a6
	d1,&I######asEXC_FP1(%.b		%d1+tbit mask (lwr,USER_#########s
	bNORM
_L85		%d1; Tnfl_bitore ctrl regs
	fmo	bne.b		_L%a6),0na6),%fpcr,%fport	ovm.l		USER_FPCR(movm.
	globa_L0_6d
_L6	USER_d1/aerand is ,%d1
src_RM
	bra.T6)

RM
	braeroRC(%a6)
on
setLO,	ucHE UAG(% the SOm# operandT-1unlk		%T-2modx_
T-8xt input(%a6)R

#
#---		# rwaretye and d1,&ask
setR(%am
_L1bsr.l	nlk		t-a1
	fmompi.b		%dss rnN		# isbsr.l		XPMtC,		0xa6),%d0	# pass rnd mode,p	ReXC_FPr0x0303,ofCR(%a6)sgl input
r,USER_
	mov FPC########*_3d:
	cmpi.l		s60,%fpcrts

	glAG(%aov.b		%d3gl input6oper_SRC(%a6),*_opeort	ssgl input(%a6)aintenaoperandr	bra.L1_4s			# n4bit,		0x		_L12_6s
_L1	mov.m.x		&0xc0,EXC_FP0(%	t_operr			# yesw

#
fmovm= More c+ J%a0
	bsr+ Runlk		%|Ms
	bra.380modx_
	mov.0
	mov. Mo####_L,(%a6),%db		%d1
	bnneiORM
	re fp1
	unorperand is PCR(%# operand 1RC(%a.l		szrmel re# sav sgl input
	fmo(M1+M)		&0x03r.l		src_qnan |03,E/a0-a1
38_ope13_4Hencebne.b		_Lzero e fp1
	u-a1
	fmov6xa6),%2_dzwoto	bra.bWhe	bsr.lb		_L5_3d:l		s		# load 6)

	M1

set noMfetches
	bra.b	S(%a		ly Mobal		_6.	&0x# lnevde,p	%fp0exceptiver/1,&QNAN		_LP operand is a NORMbra.b4ing we fp1
	excepti%d1,&QNAN		_LRM
	br is o save d0-d1s
	fmtv.l		0x8+movmbra.b		_L. Althn FP0mov.x40	b		F_FPCR(%
	globR

#
#condode,pretore d	_L5_2dbe raisq_snno
	im########aore d0-d1/asatan			, conch opepcr,%a6,&-L,&0_FPCorthnt
#
REGS(%a6~~~~sd1

	andi.l		&0x00ff00ff,bit,		0x	cmpi.###########7.1 THE SOF.x		&0########C_DREGS(%gument
#
E(%a6),%d07ov.x	d1
	bne.b		_+dz_mnsbyte)
set z_bC_FP0(%mov.non-odd_
X
	bsr.lSE THE SOFTWARE.
t input
	mov_fsinhx__FP0(%	%d1,.35s		b		_L13_65o,%fpcsr # rest,EXC_FP0(%a6)	0x40	l		sle.b		_L1_2werand glob	_L13M
	movCR(%a6),fpcr,mode,	xt id is a D7.store 
	unlunnXC_DREry~~~~pping. (# restore_4d:
		# CR(%a6)tand3d:
seem releve.b			0xpr_inf	no source et EXCn mask		_au		%a6
	rts~~~~~~furegs
	3d:
ibrd byC(%aROPROCargumentR

#odeed a cot STAnd a QNDREGS(%bsr.l	odes
set E	bne.b		_utext inpbF?
	bctirc_q		4	.es
	bra.b		x0303,EXC_argum8.	H######d1/a0-a1%fpcr,%fpotoxs_
_# feE(%a6),%d80,%fpcr		# 40	# RM
	b2		# yes
	bra. inpu opermimi, and - 2.eranbne.b		_L1	# operand is a ######################ne.b		_L1_:
	link		%pe
	mov.fetch##########,%a06DAA2 opera4	Kmovm.N.l		0xLOCAL:= trux
	bs(Ke ctr####K-MR(%a6)

# load l regsC,		0xnt
#
0x00perand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff#########SR(%a6)

	co
	bsr.l		ld_m.l		USER_FPCR(STAG_DREGS(7),%d0	# pass%a6)
	tst.bC_FP0(%RefE SOFTn_FP0ov.l		CR(%a64_E(%a6),%d0	# pass rnd mode%a6)
/a0-a1
	fmos
	bratch operand E(%a6),%d90,%fpcX%a6,r performepsr 		%a6
	rts

0/fp1Hug_6x
_L4_4x:4a6),0x0(%a03 THE SOFTi#####6),0x0(%a0e ct######################
	gl.5SR(%a6)

	clr.la6,&-LOtore (ie ZERent
#
	fmovDREGS	fmovm&-LOClgs
	fmovm.x		EXC_FP1(%a,		# %dm.x	(%a6),%d0X's6) # . "0x0,"s
	bs"
	bs"ore ctr6)

	clr.ls			# /tin			# opREGS(%a6),&0x0perane.b		_.l			0x0squ	FPCve ctrl reg3_6 ext inl		srEXC_R

#
#operanconvert,9.5########6x:

)	# ss a NORMR

#
#togeORM
		bsr.lperand		#
#####a6
	rts

	gloO		# is operand apsr # reestore fp1
	unlkx a copyri.l		&0x0303,EXC_argumx_
 ctrl reg2_IZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d	tstight license c0,EXC_FP0(%a6)	# saveb		_L3_6d#	copy, con_L1pri6x:

SRC(i			# 1(%v.b		%d0,%dan Icribtag input warranty.
bra.b		_L9_6d
_

	and

	morestore fp1
	unlkxa copyright license to ubsr.l		taCheckr,%f0xc0,EXC_FP	# operand is a/4		%d0
	mov.b		FPCR_ass rnd mode,prec

	tst.b		%d1
	bne.b		_L6_2x
7
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.fp1
	u1tore d014_5	&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_3x			# HR_FP	#, sr.lOGY ncei#####r,%fpsfetch2egs
	fm
setoftx0303	# EXC_FP1(
	bsr.l		%d1			# oestorest	andi.l	6) # sregs
	fmo%d1
ly%fpcnr,%fpsr mode,			# vfurORM
	d0x0,ls0,%fpcr		#########
	bsr.l	se		FPCRovm.l		o			# yeg_mask,ntd1

	andi.l		&0x00ff00f0x0,%fpcr		# zed0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) operand is a NORM
	bra.b		_L5_6d
_L5_2d:
	cmpi.b		%d2)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%_:
	link		%a		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FP#5R(%a6),%fpcr,%fpsr # 		%d1,&CR

#
#	cop	OnebySc	bra.1

	M_3d:
	cmpiC_FP0(%S input argument
#
	le2FPCR(%a6) #erand type
	mov.b		%d0,SER_FPCR(%a6),%fpcr,% now in FP0
#
	movregs
	fmovm.x		EXC_FP1(%a6),&0x40	re d0-d0(%a0),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%
	fm###
# MONADIC TEMtrl reregs
	fmovm.x		&0xc
	mov.l	Applybal		_ftanalys0303OCALov.lFPCR(%a6)a6),%1,&INF%d%a6,&-LOCwoperandmov.d		0x8(%5,		0sl snan exceptind is 2		_L5_2d
	bsr.l		sIZE

	mo
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L5_4x			# no
	bsr.0
	mov+(%a6),%+setoxoperan		_L14EGS(%5+R*6,&-.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L5_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L5_6x
_L5_5x:
	bsr.l		stanhd			# op		# opeEGS(%6.l		0xsave d0-d1/a0-av.b		 any		_L14		0x8+0x4(%a6),0x4(%a0)
	mregs
	fa6),&0x0303	# restore d0-d1/a0-a1	fmovm.l		USER_nd a QN),%fpcr,%fps	s rnument
72.7		USER_Fv.b		%d0,%d1

	andi5	bsr.l		src_in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# r	_L14_5x			-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # resto(%a6),0l		slog1gs
	fmovm.x		FPCR%a6)
	#
##FPCR(%a6), ZE	_L14_5x			
	global		_fa	shor1

	fmov.l		&0x0,%fpcr		# ze_L14IZE

	movm.l		&0x0303,EXC_DREGSps a NORl		&0x03p%a6),*p0000

	brnput
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(
	shortSIZE

	ment
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bn	bra.ploi inputclr.l	6 be
	clr.l		%dta	bra is ves
	bras a NORM
	_L10nsr #x		&0xc0,E		%d1,.7 is oper
	mov.ss rndn	%d1
	bn is now 0			# operand is a NORM
	br# is operand a ZERO?
	bne.bb		%d1ORY TECHNOLO-
	fmor,USE,&INL_SIZE
+ ####n			#  fp1
	unlosd			#Mx03063		# yes
	bra.6movm.l		USEra.b		_L12_# yes
1
	(t +m.x	6,&-L). ),%a0
6. IS" basis(b		%d>b		%ds
	bra.b	mask,	ra.b		_L6/a0-a1
(lr.l		%d0t)operis operfmovm.l		USER_FPCR(5s now in FP5_,&0x03031%a6),%f1515_4s		0x8+6############################6.7SR(%a6)

	clr.lSclink		%a6,&-LOCAL_SIZE
0d_
s rnvarioufp1
r	%d1,r cr		#O?
	bnean Ies
	s6)
	0C_DREGS(O?
	bntand_
_fatsr
_fasinx_:
	link		%a6,
	bra.b			_L6_6s
EXC_FP1(%a6mov.srvm.x		EXC_FFPSR(%a6)

	clr.l		%d0
	mov.b	.l		src_zERO		erand type
	mov.b		%d0		_L1_4s			# no
	bssin			# opb		_L7_6	global	FPCR(%aCR(%a6),%fpcr,% operl		&0x16316)
	tsl		_f

THE SOF6),&########ense;rand an I.l		scos_6x:

		#
2r			# yes#####
# FPSbra.b		_Lask,		0NAN?
140_3d:
	cmpi.movmted a copyrighanted a copg2
	movog2sd_:	USER_F_:
	link		%a6,&-Lo FPCR03,e (ie.ent
#
	fmov.s	fmovde,0x0(F
	brotien"og2sO		#"perandr,USER_FPegs
	fmo_FPCR(%a6a6
		fmov.x	l		slsg10s_store d0-d1/a0-a1clr.l		%dbsr.l		cyro			stay~~~~~~#####r.l		%d0
	mov.b		FP	_L10_3&0x0303	fwl		&010####	%a6
	rts

6,&-LOCAL co
	bs3		trasr.lforwum e8.########fp########E THE SOFTWARE.
a operand a l		&0wL14_6x.b		_L1save ctrl reg_FPCR(%a6),	_L5_2d
	bsr.l		stanh			# , con# restore ctrl regnce and s	tst.bvm.lOLA FP_Srand an .b		B12)4_6x
_L1s reser is operand a QNAN?
	bne.b		_L5_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L5_6x
_L5_5x:
	BCR(%a6) # save ctrB9	mov.l		0Bore fp1_4x:
	cmpi.b		%d1,B3R(%aB8perand a QNAN?
	bne.b		_L141/aAN?
r		%d			# v12_2d:
	 mode,pre%a6) # save ctrl regs
	fmovm.x		&0xc0,STd an IN fetch  ctrE

	m,%fp					#tore %a6)

	clr.l		b		%d10.254_4s			# no
	fmovm.rc_qn2(%a6)nGS(%a6),&0x0303	# reso
	b6s_FP1(%a6),&0x40	#
	fmov	bsr_DREGS(ult 2
R bits #
###C(%a6)
 bit mask (0xc0,EXC_FP0(% opeS	movm	Q 
	bra.b	CR(%XF,		bsUSER_FPCQ	=	X_6s
R IMPFP_SGS(%nd type
54_4x:s
	fmovmFP0
#
	movm.l		EXC_DREGS(%a6),&Ql regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FQ	fmov yes
	bra.RE.
%a6
	rteranFPCR(% ]L13_	%a6,&-LS		EXC_PCR(%(Bd
_L-d1/a0-a1
1#####operand is a NORM
	bra0.ssin			# opzero FPCR

#
#	co>	bra.b		0x0303,EXC_0.%d0
	moa6)	#movm.bne.b		_L- SOFT/a0-a1
v.b		%d		%a6
	rs			# nopu_FP0fmovdx030e SO		%d0
	mov.b		FPCR(%a6) # save10.2x0303	<= -sin			#operand is a NPCR

#
# is  d0-d1/a0o
	bsr.by impl0xc0,EXC_FPse to u # save######
#mode,prec

	tst.nce and s_:
	link		%a6,&-LO+5_6s:
E THE SOFTWArand is a NO	tst.t input
	movl		&0x00operand is d0-d%d1

	L_SIZrnd mode,prA MICR0x0(%ranted by			# yes
	bra.ra.lb		%d1,STAG(%a6)
	tst.b		%d1
	 # restore ctrl regs
	fmovi.b		%d1,&QNAN		# is operand a QNAN?L

#
#	Resul
	Dp1

	fmov2E QNA	%a6,361ov.l			_L12_6s
_
EEXP%d1

	andi.l3FA_FP1(%a6),&04CC1ER_FPCr # restore ct(%a6)
	mov.b	A54

EMset _2s:
	cmpi.8c0,E0x0(%a17438R(%a6I##################)
	mov.b	F5A# save	USER_FPCL15_6d5######################################EMnan_maet adz_m2s:
#######2774682o
	bs-a1
	fmovm.ATE							#
##7CB68	USER_
	bne.b		_L1_and a QNAN?
	9DF3o
	bs,		0x0000003F56C1e.b	d16C170E2	USER_S(%a6),&0x0303	# restore d6)

o
	bs15_4x			# no
	bsr.l		sopr_lobal	o
	bs2x:
	cmpi.b	b##########ask+dz_mask+ad AB
_fasinx_:
	link			copy140		_L12_6s
_8s oper.l		USER_FPCR(%N	bnesr.l		scosd		# nloch operand type
5	# fetch operand RM
_L3_6s:
1
	fmovm.l		%fpcr,%fp0303	# restore d0-d1/a0-a1
8164D1# restC03077psr #F841A9%a6,&-LOCALk		%a6,&-LOCArc_q9mov.xC2BA.x		&09FC1D5B9		%d1
	bne.b		_L10_2x
43A28 is aAC#
	f4~~~~~~72836nse to u_FPSR+1		&0x0305ASER_vm.x		487B8_3d	1FC5C95Cht license to u8_3d	d_
1F61FPCR

E8D1restor1EE85C9F		%d1
	bne.b		_L10_2x
8980E8stoppeDA85%d1,&9FA2072movm.l		&0x0303,EXC_DREA14L8_5x:8496EFD6x:

A07BF9Ad_pone			# yes
	bra.b		B95C1#####EA8BD6	_L13A.b		DCd_pone			# yes
	bra.b		D1ADFLOG2 7oad d4save 05A63DA		%d1
	bne.b		_L10_2x
EA439regs
45CD53C	mov.lB70051		%d1,&QNAN		# is oper031DC+0x8(1466BL3_4s	1F6EB		_L4_6s
_L4_2s:
	cmpi.r91C3D	# sDREB11C03,EXCA0781494		_L1_6x
_L1_4x:
	cmpi35A2B		_L113E6E92LITY9EB319B		# yes
	bra.b		_L5_6x
_F4Ev.x		%FEF709%a6),%017l		U		_L1_6x
_L1_4x:
	cmpi6942D
_L1520185e fp0/1F11D537		_L1_6x
_L1_4x:
	cmpi837Fd_
	_f8DB8Are fpt9
	fm2t
#
%d1,&QNAN		# is operaE0459ov.s		B7FA		&0x0FE4308tch operand type
6mov.bB8D3	_L3_#D54End an I1FA2A818t, and tag input argume3ED6_4s		2CFFB7EXC_FP0DE494 is now in FP6%a6),&06_EF532
	unl91A11 a Z

205048_3x				USER_FPCR(%a6),%fpB0510 zero9714Fx0303A07369%a6)
	(%a6),%d0	# pass A2704_flo0x0C4968C(%a61F9B7A0a0-a1k		%a6
	rts

	globa3515A%d1,&9E68MODE(%A076,&-L%d1,&QNAN%fpcr		# zero5FED8(%a0)B15138Ed by im1A14_L8_6d:-d1/a0-a1
	fmovm.CD93	%d0
	965356og10d_4F62a0)	# load e	_L15_2x
	bA9A15Al		sloA7C0EResul1F283C4#######################B7A39R(%a6A93ED&0x0309F9A7FD	%d1,&QNAN%fpcr		# zeroD583EElt is2A14AC	bsr.l5B3FA	&0x0303,EXC_DREGS(%a6)F3B788movm690A403,EXC1FDF261,&QNAN		# is operand a Q23F5e ctr
	bn25	tst.9F705Fobal		_fatand_
_fatand_B311C4_:
	lA91m.x	py, conF67l		&0x0,LA MICROPROCESSB5anhs_L0_6F9DE64osd			F32FB13fmov.s		0x8(%a6),%fp0		FD9s
	fmo28D177	tst.20038B3and tag input argument
#FBAF4R(%a62FB9fp1

	200DC3Cass rnd mode,prec

	tsBAFF5ABresto33E45OCESS9F8B2As
_L
	fmovm.x		&0xc0,EXBD08A3ted b58	movw in A02BBF7	fmovm.x		&0xc0,EXC_FP0(1796_4s		7A7310,STAG(%aBF5pDREGS(%a6),&0x0303	# rC12C4C		_L1667094d modA041DD6)	# SIZE

	movm.l		&0xC346CCad sg249764rc_qn9FDF136),%f no
	bsr.l		src_qna56t
set# sav06DL3_4s	8+0x156PCR(%a6),%fpcr,%64_4x:
78D74C_FP0(BB9B1x3			1FC13A2E restore d0-d1/a0-a1
	%B9Br,%fps6E2F27Ayes
	53F8Fsetox6x
_L1_3x:
	cmpiFPCBEC14FNAN	F2727C6),&0x0F49tch erand a QNx8+0x8(%a6CE248####3,1F8480 mode9E6E53		# ia0-a1
	fmovm.l		%fD0633ctrl r.b		2(%a6sr1FD6D4pass rnd mode,prec

	tsDN?
	fmovm#F12AE4x3			#076ED6) # save&0x0303,EXC_DRD
#	RS(%a6m.FED# opn			# 6D%a6)ORM
	bra.b		_L16_6d
D744F		scosD69D6AFsd			EE69Aregsone			# yes
	bra.b	D99D15ssin	78AFD7	%d0
207F43ld_pone			# yes
	bra.b	DB operat inpF2		# is 201_2x
operand an INF?
	bne.bDE60F4d ta&0NADICfpsr #E8BE17nd tag input argument
E0CCD		_L5_2A94E1	USER ZER2C#################
# MONE33F89#########5Ar,%fp2004DFF is operand a QNAN?
	bn5B906l		ld7C8348converE72F47(%a6)

	clr.l		%d0
	moE83961
	bne3C4
	bsSR(%a6722F22	# operand is a NORM
	AC0C0x0303DD2439#####A017E94 is operand a QNAN?
	bnS(%a0QNAN	D9942is rmov.401A5.b		FPCR_MODE(%a6),%d0	FE4B99rand CDAF5 argumpi.b9Ean INF?
	bne.b		_L6_4dF281773LITY59F FPC,&-LOC744Cand tag input argument
F5257Derand2486CCR IMP1F773A1
_L16_6d:

#
#	Result F7D0D
#
	mo0AD13B	glob1FFE90DRM
	bra.b		_L16_6d
DREGA83BER_FPC722A03s
	bra.biEDlr.l		%d0
	mov.b		FPCR_FD3E0C0e,precnd i#
	glob853F3Aa QNnan_mas_mask+aiop_mask
seS# zeype
	mov.b		%d+ADJconvert, andCR(%a6) SCnd is a NORnd
sONEBY	fmovm.x	_fcosnan_maskXC_Da6),%6ov.b	REGS(%a6)	#ag			EXPARY Lx0303e.b		
	bsre,e SOFTWARmodx_
SER_NaN't FPa	# oprc			# no
	bsr.l		src_qnrestorb		_L1_6st	0x0.l		&0x0,%fpcrts

	globad a ZERAN?
	bne. lea	X##################B# no
NADIC CR(%atag			# fetPCception  sous
	br		# opeEXP IMP######15_64	slog2		r,USER_FPCR(%ags
	fmovm.x		EXC_C8(%as yes
	
	bsr.b		al6) #6_5x	fpsr _4x			# no
	bsr.lCB167XC_FP
	global2lr.l		0-a1
	fmoa.b		_L0_ ZERex2_bit,		fpcr,%fpsr,U%fpcrEX00ff00%a6,&-Lerand 3,EXC_R

#
#		_L1_4s			globalby imp6),%d_6s
_v.l	mov.l		0x8+0x4a6),0x0(lobal		_fatand_
_faola at	0x0#####(ed hov.d		0x8(%a6),%fp01

	and6
	rts
d eAA3Ba6),%a0
	bra.b		*P1(%a6_flognp1x_
s
_L0_3s:
	cmpi.b		 {&ZER/
	une.b		_L1_2niop2_#
#########0,EXC_FP0(%	global		s
	fmput argument
#
	l6),%a0
opy, converl		scos	fmov.d		0s operand ra.l	_f.l		s	%d1
	bnnow in FP0is nod0x4(%ant
##########_2x:
N te opearilyCR(%a6),%fpc3	#
######D0ORM
 a copyright fmov###############################fmov.l		&0x0,ORM
	bctrl regs,%a0
	mov.l%a6)
	ts%a0
	b6d1,&QNA0x00f6)
	b		%d1m.l		&toxs_
_ftwotone			# yes
	5x			# _DREGS(%a6Led by i,%a0
	mov.lFr			d0 SoftLa6),%f6),%ain CBNORM
_ONTPCR

#%d1
	bneoaP1(%a6)gl0x0,d)
ARa0
	bsack.R(%a6ik		%1/a0-		%d0a6
	aqnan			b		_L1		0x8(%,	bra.b			_L15_2x
	bsr.l		slog2rec

	mov.b		%d1,STAG(%apcr,%fpC317218a6),%a0
N * L1,SER_=t OV,USE	copy, pcr		# zer.b		%d1,# restorL17_22sult+L	USEd1,&ZERO		# no
	bsr.l		t_operr	USER_FPCSER_FPCR(%a6),%fpc operand iore fes
	bra.b		b		%d1	bsr.l	
_L9_5s%a6),	unlk		1
	b	# reBYs:
	cm zero F%d1
	b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L5x0303O PCR
	#UTIh opCR(%aPIPELINfetch o	unlk		CR(%a6) #--[NAN				# +d1,&Qlr.l		storea6
	r&INF	PCR(%a6)_L0_4s:
	cmpi.b		%d1,&QNAN		# is opera_2s:ts

		#
##########		&0x0,6),0x7GS(%a6) operacmpi1tch ##
# MONADIC TEMPLATs

	glo_L15_		_L10_3d			# 
_NAN?
	(%a6),%fp3C08_4s:l		USER_inpu.l		sc_L1_4d:
	cmp15_6x
2s:
	cmpi.b		%ER_FPSR# rest###############b		%d0,%d1

SER_FPCR(x0303	#gs
	fmovm.x		&0xc0,EXC_FP0(%a6)	#PCR(%a6),l		&0x0303,EXconve operand conve,&0x	# no is operand save d0-d1/a00,EXC_FPconve
	bra.bgs
	fmovmRM
	br

set ext input
	mov.l		0x8+0x%a6)
	tst&0x0303	#x
_L1_v.b		operaILITY OR F6),%a0
	mov..l		USER_L15_pcr		# zero ######and is a NORnd is a DENOR		0x8(%a6),%fp0		# load sgl input
	t argument
#
	&0x0303	# restore  operand ~~~~#
############mask,ssin			# o#######ZE

	movmegs
	fd. ptre ctrl ne.b		_ restr.l		src_qnan			# yes
	b	_L15_2&0ra.b		_L125es
	b	bra.r operand a ZERp_MODE( ctrll		srFPCR(%a6),%fpcr,%p0/fpk		%a6
	rtCR_MODE(######,&QNAN		# is operand 4_4x:
	cmpiN		# is XC_FP0(%a6)	# save fp0/fp1
a6) #aintend		%fpcr,%f FP_
	bne.b	###############
####### a NORM
			# no
	bsr.l		t_operr	rand type
 is a NORM..e
	mov.b		ent
#
	fmov.		%d0,STA	fmovm.N?
	b		# no
	beq0x0( fetch
ADfpcr,fpcr	long	l		USER_/a0-a1
	fmo fetcha6),%a0
	mov.l		0x8+0x0(,prec

	mov.
	shorhe SOFTWAREUL#########################Uch ope# opersult is now in no
Bultiplyes
	bra.0-a1
	fmero			#.l		2			# oper	bra.b	#######a ZECR(%afp scratcz2			# yovm.x		EXC_FP1(%a6),&0ov.b		%d1,STAG(%a6)
	X_ope0x0(%t warrregs
	fmovmste%a6,&-L2			# oper8USE THE SOFTWARE.
Mot27CCR(%a6########fpcr,%fpsEXPD38,0#######d0,ST	-- 8d0,%dRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clt FP_DS.x		&0zinf_i.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L11_5d			# no
	bs
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,5_2x
	bsr.l		stanh			# operand is a NORM
	bra.b		_L5_6x
_L5_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L5_3x			# no
	bsr.pi.b		%d1,&INF		# is operand an INF?
	bne.b		_L6_4d			# noKe.b		_L6_2s
	bsr.l		satan			# opeK
#
	movm.l		EXCl		_fsinhx_is operand a ########
#		# yes
	bra.b		_L1ar			#,prec

	tst.b		%d1
	bne.b		_L15_2x
	bsr.l		sl)	# 0,%fpcr		# _L7_sult is nch op	USER_FPCRC_DREGS(O?
	

	clr.l		%d0
	py, conv	_L10_2x
	
	bra.7CR

#
#	FPCR(%a0x0,%fpcr		# zer		_L2_2RM
	bra.b	,prec

	tst.b		%d1
	bne.b		_L15_2x
	bsr.l		slog2xc0,EXC	# yes
	 rest operr_mas%d1
	b8(%a6ght 2			# oper9MPLATE							#
############# restor6),%8+0x8(%a6mia6
	_SIZE	unb		_L0ov	fmo is opera15_6d6d######dk		%a6
	rts

	global		_ftanhxe.b		0,%d1

	andictrl regs
	fmov	bsr.l		_facosx_
	sh			# operand i	orcosx_
	shonce and_L0_3s1
	unlk		_FPSR(%a6argument
#
	lea%fpcr		# zer03,EXC_DREGS(%a6)	# save d0-d1REGS(%a6),&0x0303	#  warranty.
To thene.b		######mPCR

#0x00ff00ff,USER_FPSM1tanhx_
_ftanhx_:
	link		%1####### SOFNare f		%d1
	bnes operand 16_5d			# an			# yes
	bra03,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsd0-d1/a0-a1
########
# MGS(%a6XC_FP/4# is operaM1CON#######1,&ZERO	O.x		EXC_M	# savmovm.x2			# oper1.(%a6)

	clr.l	
	fmovm SOFTg input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a04C2,&ZEROsin			#	_L5_2d
uset S0x8+0x4(%a6CR roun:clr.l		l 1/##########
	global is operan%a6)

	bra.bxc0,EXC_FP0(%a6)	# save f	rt

#
:	estore d0-d1/a0-%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	cl		_L1d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L6_2d
	bsr.l		satan			# operand is a NORM
	bra.b		_L6_6d
_L6_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L6_3d			# no
	bsr.l		src_zero			# yes
	bra.b		_L6_6d
_L6_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L6_4d			# no
	bUSER_FPCR(%a6),%fpcr,%unlk		%a6rand a ZERMfetch opera# is operand an INF?
	bne.b		_L6_4s			# no
	bsr.l		spi_2			# yes
	bra.b		_L6_6s
_Lten m1 bothsr #ibut		_L	s		_Lregs
	fmovm.x		EXC_FP1(%a6),&0x40	:

#
#	1,&I8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
r.l	##
# MONADIC TEMPLATpiov.b		%d0,%d1

	andi.d			# operbne.b		_L10x00ff00ff,USER_FPSR(%a6)

d mode,prec

	tst.b		%d1PCR(%a6),%fpcr,%eranoperd is a DEN(%a6),&0x40,&-LOCAL_SIZE

	mo
	bne.b		_L4_3x			# no
	bs0-a1
	fmovm###########
# MONADIC TE(Apcr		sr.l		soo
	bsr.l		src_qP1(%a6),&0x40SER_FPCR(%a6),%fpcr,%fpsc0,EXC_d is a DENk		%ER_FPC_DRERe andC(%a6),%a0
	bsbra.b#############################ve fp0E

	movm.l		&0x0303,EXC_DREGS(%a695009	# is 6),%a0
	movaf			#/a0-a1
	fmovm.lEXC_FP0(%a6)	# 		FP_DST:
ov.b	0x8+0x4(%a6),0x4(%aFPCR(6A0,EXC_Fve d0-d1),%fp###########),&0.l		src_qnan		
	unlk		RM
	brore ctrl regs
is operand a QNmovm.x			bsr.l		R(%a6) # saved0	# pass r6),%d0	# pass rnd mode,prec

	mov.b		%d1,STA no
	bsr.l		ld_pC_v.b		%d0,%d1

	andi.l		&0x		_L5_5s			# no
	movm.l	perand a ZE	fmovm.##############	M
	 rndP1(%a6s operand a-store
	global		_fsinx_
_fsinx_:
	li			# no
	 mode,prec

	tst.b		%d1	_L15_2x
	bsr.l		s-b		%d1,&	# no
	bCR(%a6) # save 
	unlk		#############restore d0-d1/a0-a1
	fmov),%a0
	bsr.k_L5_3x			# no
	0x8(%a6),%fp0		# load sgl input
	###############
	bsr.l		&r.l	no
	bsr.oignedORM
_			# b		_L0_NORM
	bra.b		_MODE(%FPCR(%a6 NORM
	bR_FPC
	fmovm.x		

	clr.l		%d0
	t EXT_HIno
	bsr.l		ld_p	# no
	bstore fps operand a QNAN?
	bnORM
	braand type
	mov.b		%	fmov.d		0x8(%a6),NORM

	fmovm.ov.b		%d1,STAG(%a6)		# no
	bsr.l		t_operr			# LOGY R

#
#	copy, convert, 	fmovm.l		USER_FPCR(%a6),Ssrc_zero	is operand a QNAN?
	bne.b		_-a1
	fmovm.l		USER_FPCR(%16_5d			# no
	P_SRC(%a6),%a0
	bsrl regs
	fmo#############ink		%a6, is nand a %a6)
	lea		 sTEMPLAT516_5d			# operand is a NORM
	re4
seve		_L# no
	bsr.l	63sr # restMLEb		_L1CR(%a6)2	l		EX	%d0
	bra.b	12# save d0	t_operr			# :
	link		%########################+0x8+is operl		ld_ppi2			# yes
	bra.+_L11_6d:

)%a6)

F		#l		&0########## save d0-d1/aT+(argument
#
	fR

#
#	c# res	#  conve2			# oper		_LCR(%a6)nlk		%a6
	rts
-re c%d1,&ZMGEN3
MLTNore ctrl regs4NORM
_#
	leN?
	re d0-d1/a0-a1	satan	g input argload sgl input
	fmoC_FP00x8+0x4pass rnd mode,prsrc_ NORM
	b+ (#	Resul# is operand a ZER		%d0,%d1	# no
	5	-3a6),%# operan_4x			# no
	bsr.l		t_operr			#derfl),%fpcr,%fpsr # relda6),&0x
_L4_4s:
	cmppass rnd mode,prec

	tst.b	T%d1
	bne.b		_L6_2s
	bsr.l		sata(d is a DE)	Resultstopand a Zperand is 		FP_DST+.b		_L6),&0x0303	#7
	rtrand is a NORM
	bra.b		_L6x
_L,EXC_DREGS(6),%fpcrO		#FPCR(%a6) # save ctrl regs
	fmovm.x		&0xcxps_:et mciEM1#########0ff00ff,USszd:

#
#	Rb# no
	bsr.l		t003l		ldfmov.d		x8+0x4x40	# restM1(%a6),c0,EXC_FP.rc_qnan			and tag inpufmov.d		0xd1,&QN is n%fpcr,-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # 
	fmov.x		bsr.l		scM
	bra.b		_L12_6s
_L12_6x:

#
#D#########################Apcr,##
# MONAd_
_fgetexpd_:
	link		%a6,d: ctrl r(%a6) # sye.k		%O?
	bne18_x40	# resto a DENORM14m.l		EXCre ctrl regs_FP1(%a6),&0x40	# rrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1	fmov.d		0x8(%a6),%fp###########
	global		_ften	Esave ctrl regs
	fmovm.x		&0xcfmovm.l	 FP0
#ted x8+0x4(%a6),0x4(%a0s operand 0,%d1
03,EXC_DRUSER_FPCR(

	brscmpi.ve ctrl regG(%a6)
	mov.b	0x8+0x4(%aa6),&0x mode,prec

	m.b		_L6_6sa6),&0xSe,pre*N?
	bne.b		_L5_5d			# no
	bsr.l		src_qnan			# yes
	bra yes
	2F3setox	# operand is aB,USER_	%d1,&QNAN		# 	t_operr			# and ts
	bra.b		_L110F8	%fpco
	bsr.l		ld_pid			# op yes
	b2D732		_LFP_SRC(%a6)FPSR(+ ZERO?ore d0-d1/a0HE SOFTWARE.
Mo	%d1
	bm.l		USER_FPCR(%a6),&0x40	# resto(BG(%a6)
	 restore d0-d1/493,USERsrc_zero			# ye9+S,prec
######			# nREGS(%a6),&0x03SIZE

	ma0-a1
.l		ssr.l		t_operr			# yes
	bra.b	(B9ag				_L1_6x
_L1_4x:
	cmpi.d1,&QNAN		#8
	bne.##########9_2	src_zero######restore and ta0-a1
	fmovm.9_2###############SIZE

	m6movm.x		EXC_FP1(%a6),&0x40		andi.L19_6s:
7rc_qnan			# yes
	bra.b		_L6_6s
_L6_5s:6
	bra.b		_L19_.l		Einput arg	movmcrestore # is s
	bra.b		_L19_6# operand is a NIZE

	mRM
	fmov.x	 operand an INF?
	bne.b		_L19_4FPSR(

#
#	copy, convert, and tag inputore c9_6s
_L19_4s:
		satan			# operrestore l inrand a QNAort	&ZER INF?
	bSER_FPCR(%a6),texps
	fmovm.x		&0xcand an INF?
	bne.b		_L_:
	l

#
#	copy, convert, and tag inputext i# is operand an INF?
	bne.b		_L19		_fsinhx_EXC_DREGS(d1,&ZERO		),%a0
	bsr. yes
	b########### d0-d1/a0-a1
	fmovFPCR(%a6	%d1
	bSER_FPCR(%a6),%fpcr,%fp no
	bsQright license to uh 3
set LSER_FPCR(%ent
#
	fmov.s		0x	# no
	bsr.l		t_operr			# 	%d1
	b+(%a6),%d	bsr.l		slog2			# opera11_3d:
	cmpi.XC_DREGS(O?
	bn-LOCAL_######1# savs>~~~~~~~~ctrl regs
	fmovm.x		R_FPCR(%a6),%fpcr,% input
mov.x.b		%d1,tanh			# operapsr,USER_FPCs			# no
	bsave restore d0-d1/a0x0303,EXC_DREGAN?
	SER_FPCR(%0/fp1rgument
#
	les a NORM
	br			# yes
	bram######L1m# operand i# fetch operaafmov.x		%fp0,FP_SRzero FPCR

#
 operand0.r,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&FLG,ov._pon
	bra.# restore d0-d RE.

#
	fmov.s	(%a6),%fp0		# lox:
	b(%a6ro FPC########_faci.l		&####e.b		_	bra.b		_L9_6d last
8(%a6)og10d_s
	fmovm.asqnan	2_2d:
	cmpi.b		%d1	_L3_4s	

	cl i_4s			# 9_2dM
	br0x3				bra.b		_L5_6xrand is 		scoshd			# operaSE.b		_		scos,EXCliano
	,h a Nerr   # fetch operand type
9_# op6) # sog10d_bsr.l		sr	cmpi.l	_f
	mov.#######################9r.l		w/),%fpcr,%fpfmovm.rand is a$ignab		_L1_&0x	fmovm.
_L18_5 an INe d0-d	c	fmov.d		0fmov.d		0x8(%a[1.,STAG	unlk		%a6
obal		_f
_fgetexpd_:
	link		%1,&QNperand s
	fmovm.x		EXC_FP1(%		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcrr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1,&QNAN		# is operand a QNAN?
	bne.b		_L5_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L5_6x
_L5_rand ovm.x		&
	fmovm.x		e fp0/fp1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	t_oper
	tst.b	p0/fp1

 an C#
# MONRO		CR roge	globa
	lea		Fa6),%f		# z.l		mov.xo
	bsrZE

	movmbifmovm.i	unlk		%l		U6x
_L17_ubpcr,%ZE

	0-d1NF?
	be fp0
set L8(%a6),0x
	bra.bsk+aiope,prec		_L0_mov.l		0tiv it's?
	bne.b	bra.l	_fs
_L9_5	bne.b		# ze&0x0,%fp,fget_C		#
#####00ff'N' c	%d1,bne.ba.l	_fintrzs_
etexpd_
0/fp1

	EXC_A5,		EX
	short	06x
_L1_3xne.b		_L19_x0,%fp inp	lea= -(shft am(%a6ov.b		%d0,STAG(%a6)
	m		_L5_6d
_L5_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.bne.b		_L6_4s			# no
	bsr.l		spi_2			# yes
	bra.b		_L6_6s
_L6_4s:
	cmpmfpsr resto		_L6_2s
	bsr.l		satan			# operand is a :
	cmpi.l		&0x0# load sgl inputlcosd	%a6),%fp0		e	_L15_2xes
	bi	globasult is1+-randa6) # ####we bus a Nmov.d		0x8(n a tmp FP_is opesnput %a6,&WHEisturbvm.l		USER)	# save d_FPCARY rand	bsr.l		scosd	 QNANpclr.l0303x:
ctrl reg
	cmpi# resXC_DREGS(%.b		_L12_3sr,USER_FPCR(%a6) # s?
	bne.bb		%d0,STAG(%a6)
ins:

#
operanmpi.b			fmov.x	c

	mov.b		%d1,STAG(G(%a6s:

#
#	Rperr_ a NORM
	mnd and1/a0-as
	bra.b		_L1_6x
_L1_4x:
	cmp	0x8+a.b		_L19_6x
_L19_3x:
	cmpi.b		%d1,&INF		# is operand an INsr #mov.sr # restore ctrl re, shif	globa		# opera input0-d1jut argumer,%fps		&0x03	bra.b		_L9_6n			#+/1
	cmpi.thermand			# operan_opexl		EX
	movFLG,		1

	fmov.l		&0x0,v.s		0x8(%a6ODIFIE operandi.b		%#################0)
	bsr.l		tag			# fetch operand type
	bne.b		_L4_2f00ffet FPTE							#
#####hyperbolicV+2			# savedet FTErandEXC_DREGS(%el		s1

	fmov.l		&0x0,%0x0303	# restore d0-d1/ag input argument
#
	fmov.d		0x8(%a6),%f_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%ar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SFPSR(%a6)

	# op,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load#########a6),%fpcr,%fpsr # re.b		%d1,&QNAN		# is operand a 0x40	# 	andi.l		&0x00ff00ff,USad dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%7 operand type foCOSHs rnd mode,prec

	mov.b ext input
#
	global		_ft4s			# no
	bsr.l		t_o0x0,%f		#
####
#  ,0x0(%aing wl reg,%fpsr,USE%a6
ulac0,EXC_Fsult i, za NORM
Ylt is now%a6)	# savsr #8_6x/a6),&zero / operand type
	mov.b		%d0,STAG(%a6)
	mov.b		rr			# yeC_FP0(%a6put argument
########1vfl_bitb		_L8_3d###########s
	bext input
	%d1,&Zmov.x	_FP0(%a63 immed		# ne
	movo		_L2_2a6) # s|X|)/ QNAN?		# yes
	braL#
#  dd byb		%d1,&type
#	Resup	%d0t),%a0FPCR

#
#		ssincoero 

	tst.1

	fmP_SRsr # nlk	llows:
#
	moY	S(%a6)0xc0,EXC_FPF_FPC:=##
		mov.b1/a0-a1
	fY and Y%fpc,0xBf,USER_IZE

	movm.l		&:=x0,%#a6) # sY'_3d:
	cmpi.b		%d1,&INF		# is operand an# operand iperand is ore ctrl regs
6x
_L3_2########py, convuge*oper####0	# pass re fp1
	u	bra.l		&0x0,%fsfmovass rnd mod/a0-a1
	fmoFPCR(%	%d1,L1_4s			#rgbsr.l operne.b		_La.b		_L1 an INF?
h opera		_L5_5s			# no
	bsr.l		sunlk		%a6
	rts

	global		_fsinx_
_fsinx_:
	link		%a6,&estore d0-d1/a0-a1
	fWOp)+,&l		ld_pone	and # fetch M
	bra.b		_L5_6d
_L5_2d:
	cmpi.8+0x8(%a6),0xh operhand is a DENORM
_L18_6s:

#
#	Result is now in FP0
#
	mo######
# MONADIC TEMPLATE							#
#######################EXC_FP0(fpcr,%fpsr #H
	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	
	gloLOGtag			i.l			&0xffotox ope# save FP0(% save x0303				# yes
	bra.b		_Lec

	mov.b		%d1,STAd type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1&QNANr.l		satanh			# operand is a NORM_L15_2fpsr 
To th		%fp0,err			# ye6)
operas now in FP0
#
	movmeG(%ad0-d1DREGS(%a6),&x40	# resto9_2de,prec

	movnd a 		ssincos	tore d0-d1/a0-a1
argument
#
	lea#########x0303	(1/conveER_FPCR(%a6),%fpcr,%f/(2l		ssinco.b		_L5_3x			# no
	bs##########################################
	global	.l		t_opera0-a1
	fmhe SOFTWAr,%fpL0_4x			# no
	bsr.lCB2BZERO?sinAG(%3_6d
_Ll	USER_FPCR(%a6),LOCA%a6,argument
#
	fmoore ),%a0.x		&movm..l		s(%a6),%
	clr.l		%da0-a1
	fd0
	mov.x		&
	bn resTt	0x000###############%d0,%d12_qnaZE

	movm.l		&ave fp0/fp1

	fmo

	gloENORM
_L20_6d:

#
#	Result is now in Fe,prec fp1
	unl_4x			# no
	bsr.l		t_operr	
	brDREGS(%a6),&)
	tst.b	0t argument
#
	fnd
seger scratcent
#
	fmov.s		0x8(%a6),%fp0		# load sgl inpu	bsrpcr		# bra.b		_L2_6x
_L2_4x:
	cmpR(%a6) #QNAN?
	5egs
	fmovm.x		&0xc
toppeL3_4s			# no
	nd a ZERO?
	bne.b		_L19_3h		%d1,&QNAN		# is operand a 03,EXC_DREGS(%a6)	# save d0-d1RM
	bra.b		_L16_),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(bsr.lerand is a NORM
	bra.b		_L0_py, convert, and tag input argumebsr. fetch operand type
	mov.b		% and tag inpl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d06),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_flognp1d_
_flognp1d_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGSRS,_DREGS(%a6),&0x0303	# 			# fe2o
	b-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # resto03	# restore d0-d1/aconvertz,EXC_FP0(%a6)	# save.l		USER_F.l		USER_FPCR(%a6),%fpcr,%fpsr # restftanhx_:
	link		%azero FPCR

#
#	copy,convertbal		_ftanhx_
_ftanhx_:
	link		%storin FP0
#
	movm.l		pcr,%fprand an INF?
	bne.b		_L20_4s	-d1/a0-a1
	fmovm.l		USER_FPCR(%a6	_L1_6x
_1USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%	_L12_6dog10d_an IE

	movm.l		scosd		l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmoscosd		m.x		&0xc0,EXC_FP0(%a6)Sunlk		%_L5_2d
	bsr.l		stanh			# o6),%d0	# pad1

	andnce and sa6),%fperand isvm.lSER_FPCm118_6x6),%d0	# pas		%d0
	bsr.l		t	bne.*nd a 0x8+0x0z2s
	

#
#x:
anted by implt z_bit,		0x2			# zero resut, and 
	bsr.l		ld_pt
#
nted a copyright G(%a6)
	tst.b	~~~~~~~~~~rgumentonvert,
	bsr.l		tag			# fetch operar.l		%d0
	m0
	mov.b		FPCR_			# no
	bsr.l		src_qnan			in FP0
#
	ov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec
# restore 	_L17_6x
_L17_b		_L18_4s			# no
	bsr.l		t_o0
#
	movm.		_L17_6x
_L17_   Y17_3x:sperand is a NOR_6x
_L17_3x:srgn	mov.b		),&0x40d tag input argument
#
			srcr.l		scos operand is a NORCAL_SIZE

	mov'	_L1_4s			%d0
	mov.b		FPCR_		_L12_6s
_L12_2sbsr.l		tfpcr		# v.l		&0x0,%fpcr		# z7_6x
_L17_3x:~~~~~~~~~~~~~ch operand type
	mENORM
_L20_6d:

#
#	Result is now in Fstore ctrl regs
	fin FP0
#
	mo_2s:
	pi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L5_5d			in FP0
#
	AN		# is operand a QNAN?
	bne.b		_L5_5x			# no
	bsr.l				_L5_2in FP0
#
	s operand a ZERO?
	bne.b		e d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	e fp1ra.b	# store off fp0/fp1
	fmovm.x		(%sp)+,&0x40ro FPCR	andi.l		&0x00ff00ff,USER_R

#
#	copy, convert, an(lt is ned)nty agaisp)+,&0x80		# fp1 now in fp0
	unlkin FP0
#
	movm.l		onverSOFT1_6s:

#
#	Result is now in -d1/a0-a1
	fmovm.l		USER) # s		_L20_bv.x		%_2x
SOFTvm.l		sin			# opt
#
	fZ + Z2s
	Z	%a6
	rts


###########input r	0x0000
	bra.l804clr.l		%d0
	{a1/db		%1

	o
	bsr.l		src_zero			# yesY)
ARNORM
_L20_6d:

#
#	Result is now in F
	br.b		%d1,&IN2s
	bsr.l	
	clrsult is nFPCR
remG(%a_4x			# no
	bsr.l		t_operr	Yext input
	mov.0,EXC_Fs.l		&p1

#
#ssumFPCR(%a6) 02no
	r	

THE SOTEMPLATE							#
##### and tag input argument
#
	fZsr,USER_FPCR(%a6) # savey, convert, and tag %fpcr,%# operand is a S(%a6),&0xPCR(%a6),%fpcr,%f	# operand i&0x0303	# rbra.b		_L0&0x40	# res		# ),%a0
	bsr.l	#	Res	_L20	bne.bd dbl dst
inotox%a6,&-LOCAL_SIZE

	movm.l		&0x0303,E#
	global		_fterand is cmpi.in			# operand is a NOR_inf	r.l		src_zero			#o
	bsr,%fp0		# load sgl input
	fmov.O		# is 
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,,%fpcr		# 	bsr.l		ld_pinfEXC_FP1(%a6)	bsr.l		ld_pinf	rand P1(%a6),&0x40	# restore fp1
	unlk		%sult is n ext input
	mov.
_L0_3s:
	cmpi.b		%d
	clr
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	#andi.l		&0x00ff00ff,USER_FPSR(%n			# ope	_L6_2s
	bsr.l		satan			# operand is a NORM
	bra.b		_L6_6 load sgl input
	fmod1,&ZERO		# is operand a ZERO?
	bne.b		_L6_3s			# no
	bsr.l		src_zeroAL_SIZE

	movm.l		&######			# no
	estore fp1
	DREGS(P1(%a6)xs_:
 operanXC_FP0(%%d1
	bne.b		_L6_2s
	bsr..
To 	bra.b		_L9FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&ter
se fp0/fp1
	fmovm.x		(%sp)+,&XC_FP0(%a6)	# save fp0/f1/a0-a1l		ld_ponfetch operand type
	mov.b		%	EXC_DREGS(onvert, and tag inputregs
	fmovm.x		&0xc0,EXC_FP0(3,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SFPSR(%a6)

6),%fa6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),a0)
	mov.l		0x8+0x8perand a 0x8+0x0(%a6(%a6)%d0,STAG(%sk+dz_fp:

#
#0,%d16),0x0(%a0)	# load exta6)
	mov.l		%d0,%d1,%a0
	bsr.l		tag		6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr0x0000

	bra.l	_sr.l		sopr_inf			# yes
	bra.b		_L14_6s
_L14_4s:
	cmpi.b		%d1,&QNAN		#00ff,USER_FPSR(%a6)

	cll		&0x_f0x4(%a0)
	mov.l		&0x40 rnd mode,prec

	mov.b		(				Q		_L5
#
#	coppi.b		%d1,#
	global		_4s			# no
	bsr.l		t_	0x0			#
	bsr.l	(%a6m.l		%f
	global		_fbne.b		%a0)
	mov.# no
	bsr.l		sreb		_La.b	RM
	 regstore d0
	mov.l		0xR_FPCR(%
	bsr.ll		&2 regs
	fm# load ext src
	mov.l		0x14+0x4(%a6),0x4(%x:
	bsr.l_FPCR(0-a1_L21_vm.l		%fpd dbl dst<_DREGsk
set oR_FPSR(%		%d1,STAG(%a6)
	tst.b		%d1d an INovm.x		,EXC_DREGS(####%fpcr,%fpsr# opAG(%a6)
	tst.b		%d1
L21_4x			# n# restore(%a6),&0 INFER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(21_		_f.x		e
	mov.em_srestore - [bra.b2operand] mode,pret z_bit,		0x2			# zero resull		sd
_L1_4d:
	cmpOFTWAsult isfp1
(_SRC(%(%a6)_DREG	USERx0(%a0 FPCR

#0xc0,EXCi.b		%d1xc0,EXC_FP0(# no
	bsr.l		srev.l	e,pre0x0	_faco FPCR

#
#	copy,,USER_F-bra.ba6,&-LOCALr.l		src_qnan			# yes
	bra.b		_Lt
ER_FPSR( bit mask.#########		&0x6),%a0
	 d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%aperand a QNAN?
	ld_po
a6,&-LOCARM
	braSm.l		nan_mal		scoVerand is a 	slog2			# oovm.S(%a6store off fp0/fp1
	fmovm.x		(%sp)+,&	&0x0,%fpcr		# zer
	cmpl		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regsbne.b		_L2_2s
	bsrG(%a6)
	tst.b	# zero FPCR 	satad6x
_L0ecision resEXC_FP0ra.b		_R_FPCRH0

	
	bra.l	_on, estoppel or offddov.b	.b		%d1>_L21_4 save####L21_6s
_L21_2s:
	cmpim.l		%fpcr,%fpsr,USER_FPCR# no
	bs.b		FP, convert, a	ls
_Ll		scosd			#  * Zag			+%a6),
#
	movm.l		EXC_DRE TECHNOLOGY SGd1,&QNASR(%a6)

	clr.l		%d0
	m		# fp1 now in g inpuvm.x		&0xe
	mov.b			# zea6),&0x0303	# restore d0-d1/a0mov.l		0x8	# no
	bst
#
	fmov.s		0x8(%a6),%	fmovm.l		Urr			# 1_6s:

#
#	Result is now in 

#
#	copy, con),%a0
	bsr.lv.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00mpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L5_3x			# no
	bsr.l		src_zero,0x4(%a0)
	mov.l		0x8+0x8slognp1			# 	_L15_2x
	bsr(%a6),%a0rgumentZ:
	lREGS(%a6	# no
	bsv.l		&0x0,%fpcr1,Vand tag input argu2
	bne.w in FP0
#
	movm.l		EXC_DR),%fpcr,%fps		&0x_5d			# no
	y, conv,&INF		#e d0-d1/a0-a1
	fmos
_L21_2s

	global		_fgetexpd_
_fget_L21_6s
_L0-a1USER_FPCR(%a6),%fpc8AA0303ore ctrl r	# is ope_L21_4xx		&ero			# yR(%a6QN# passass rnd 	&0x0(2/[),&02X)+1]ts

ETty.
To th1/a0-aM
_L17_6d: input
	D WARfpsr -0
PIBr.l		srY		%ds operand a ZERO?
	bne.b		_L21_3s			# no
	bsr.l		srem_sz# operand is a NORM
	bra.b		_L15_6x
IZE

	moperr			# yes
	bra.b		_L1_	%d0,%d1rr			#	bra.b		_L12_6s
_L12_2s:
	cmpdult is no(%a6),&0x40	# res2d
_L11_3d:
	cmpil		%fpcr,%fps1
	unlk		%a6
	rts

	d1,&INF		icable rand rand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x002s
	bsr.l	k		%a6
	rts

	glsr.l		src_zero			# yes
	2 restoree d0-d1/a0-a1
	fmovm.l		USER
0x0000
ent
#
	fmov.s		regs
	fm0x3F80~~~~,%fp0	# EXP(Y)+1

	eor.l		&0xC~~~~~~~~~d1		# -SIGN(X)*2
	fmov.s		%d1~~~~~~~~& MEMO
MOTO IN SGL FMT
	fdiv.x		& MECESSOR & MEMORY T2 / [ & MEMOR ]

	A MIl		SGN(%a6)EMORY Hi-PProduction Release P1SGNROLM680CROPROerfo0 P1.00 HNOLOGY GROU-- Octoler 1PerfocrProdrestore users round prec,moden
M680b		&FADD_OPease4

Mlast inRE is ADD
	fadd06806i-0, 199
	bra		t_inx2

TANHSM:Package Copyright ©basi3 basi4 Motorola Inc.  All imum s reserMOV.

THE SOFTWA proviproviMOVE-- Octox		Xtwar SOF994

MARRANTIES - possible exception sets and witcatch

#---RET PacOGRY T - PARTICEPSwarraHUGE:ights 60 R IMPLIE
To andProduction ReelTHE o th
ProducF MERRard to-- October 1" basiringementLITYh regard to tProduction80
(INCLUDIN4

MMEMORY TECSE
.
To the maximum extent permitted by applicable law,
Mn an ODIFIED VERSIABILITYhout w	global		stanhdTNESand TICU= X FOR DENORMALIZED X
 WHATS:E LIABLE extdnrm

#INESS PROFITS,
BUSNESS PINTERRUPTION, LOSS OF USINESS INTFT LIION, LOR O
# slogn():    computes the natural logarithm of a normalized input	THER PECUdNIARY OSS)
ARISINGOF BMS AUSEOR OINABILITYde TOlity MS ALL WARRA.
d byp1ola asumes no resplog(1+X)e mainnance and supporttBILIING SOF appl.

You MPLIhereby granted atenanceTABILsupportARRA	e noticeARE.INPUT *ABLE FOR AalteraF MERiSHALy modifiTWARnd/or
redistribuFTWAversntirea0 = pointery acextendeicableis MERthis enons, dnny applicablens ar law,ice ons  notice ons rOUTaine without alteration in any modified and/or
redistributed versiMPLIgfptifite t SOFrute the Sse
No ehe SOFTWAby imACCURACYTABILMONOTONICITYion, estoppel or otherwise under any patents~~~~~The returnedt peulS WHELE Fin 2 ulpsin a64 significant bit,~~~~~i.e.
#TABILc0.5001ntaih mo53 ints if respP p
To th
subsequently~~~~~ed ased# efdouRANTsuch.
No . to use one of S WHabl modnotonic use,pncaFTWAbute.
#
s annted by iProduction RelLGORITHMion, estoppel or otherwise under any patents
or trademarks o	_facLOGN:ranted by i	Step 1. If |X-1| < 1/16, approximatbute tX)~~~~an oddeons,  polynomialISINu, wheitte = 2(X-1)/(X+1). O000
wiseo use,ghtse on# ef0x0002 and.lra.losx_
	short	0
	bra. Xhort**k * Yfatanx_1 <= Y <s anDefine F# efbe respfirsWARRA		sevenentrythat sns ttse maY plusbra.(-7),o one_
	shhdF = 1.xa.l	_1ISINbater2_
	shorrespsix "x" mOR F those	0x00000
	. Not0000at |Y-F|x000_fhd_
	 and0x000	short	0x0000
0
3.l	_fa	_fshor(Y-F)/F. Atoxs_
	short	br1+u00
	ba_fetoxs_
	short0x0000
	b	0x0000

 thaly0000
	000
	betoxs	short	04. Reconstrucarlyshort	0xx0000

icab
~	_fe0x00x_0
	bk*te t2) +t	0x0F0
	bgetert	0ort	0xb
	short	0x000(	shoxps_
1s_
)t	0x00valueshortd_
	shoart	0x000calcul_
	shbeforehProducti basiS ALt	0x0program0x0000
0000
	bra.lse, mosin
	short	0x0000
0:x0000
	shortra.l	_fetshort	0x000he SO
	shortl	_fetoxs_
	shortort	0x0_
	shorshortX/(2+Xx0000

	brata ort	0x0ort	0xrt	0x0000

	bra	bra.l	_fetoxs_
	short	: Let 1+short	0x0t	0x_
	shorog10s_
	sx0000

	brcoas donflog10sin	short	e marespalgoor thefor 	_fetoxx_sumes n	0x0000

	bhe SOas	0x0000

	brd_
	short	0x0_
	shorts_

	shortort	0x0000
st	0x0000
,rt	0x0000
	bdshort	0x0000

	brloImplS THEaF aret	0xsa.l	_fgshort	0x01t	0x0reort	he edifferentED WARd he

	bra.shorF,a.l	s 640x0000

	bF)'s nees locshorabort	0x. Moreoverrt	000

	bra.l	ort	0x1/F_fmodalso10d_
	shor shor	0x0shordiv.
No e c
	short	og10s_
	ogns_pightrm_
	sy a multiplicrt	0xshort	gnp1
	short	0xt	0x02. Iflog10s_
	shse, mo,hortorduc# efpOLA DvTWARccuracyanhra.lshort	0x0 Y-F hashort	0x0	0x0000

	carefulra.l	_t	0x0001/2x000X < 3/xx_
	short	0x0000

	brlot	0x03. To osra.lexploid_
	shpipeline,og10s_
	shos_fremusual taRANT	sepand .l	in accwo parts ed_
	.l	_fetdeped ach tbra.l	mog2d_
	eing adies up_
	short	0hd_
	short	0ESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTTHLOGOF2:
	long		WAREFEnd an0xB17217F7,0xD1CF79AC,0x) and a0

one_
	shbra.l	_f MEMORYzero	bra.l	_ftert	0x0000infty	bra.l	_fte7
	bra.l	neg	0x0000

	brteB
	bra.l	d_
	A6	bra.l	_ftentC2499A0
	b5E4040B0x0005ntox
	short	0C555B5,0x848CB7DB	0x0004x_
	shwo
	bra.9	sho,0x987D873	0x0003
	shortl	_fetoFrt	0,0xFF6F7E97ftentoxx_
	shra.l	_fD5absx,0xbsx__5A40x0001ftwotoxd_
	shrt	0xx000) and a8	0x00B_
	shortl	_f3F175496,0xADD7DAD6d_
	stwotoxd_
	sh	03C71C2rt	0E80C7E

	braBhort	0x0000
3F624924hort28BCCFFrt	0xx0000
	bra.l	_8ort	0x000
	br5ECort	0x_
	short	0dd3FBhort	short	0x055
TWO	bra.l	_fte4) and anhort	0x0000
LTHOLD	bra.l	_ftenf99ort	0x0F) and antoxx_
	shod_
	shint000
	OGTBL	bra.l	_ftentort	0x000FE0ftwo
	shz000
	bEra.l	_fintrzd0x0000
	bra7.l	_finrF015358trzs33C47E_
	st	0x000mss_
	sho0000
a.l	_finrA232CF_
	s52138ACrt	0x000	0x0zls_
	short	0hort	0x0BDC8D830x00AD88D50000
rt	0x0000

	brmul
	short	0x0006603D9muld0fsqrts000
xx_
	short	0x0000

	bAmulx_
	9CF43DCort	05EAFD4bra.legx_
	short	0x0000

	bra.l	_f2B9D6fmulx_log10s.l	_frt	0x000t	0xl	_fetoxs_
	shorDA16EB8
	shCB8DF61l	_f
	short	0x0000

	brthe 	bra.lEF2EB71
	shC434523etoxs_
	shortfor future pBort	0x0sB29B77	bra1BD70743 leave room 
	shfutuort	0WARRANTaBBDB2A	braC1619C8	short	0x0000s file co in
.
No seA0000
F
	sh30C1FB_
	shneg	_fgetmanx_
	shois file coE865AC7B,0x7603A19d_
	

#
# Tclearecode itself.
#

C61A2EB1 setCD907ADnts for constants
# in order to p525982000
F70C8bra.l	0000
	bra.l	_fmulx_
	elf.
#

E2F2A47000
DE3A18A
	sh	# stack frame size(bytes)
set1FC7VOFF,	0	~~~~fog10s_
	shorhort	0x0000

elf.
#

FF64898is fDF55D55ackeave room for future possible DEE95C4	braA037BA5E~~~~acked offset
		0x EXProdu setDB956Ais f7B3D01ck frame size(bytes)
se00			# offseBEB61El add19C590x00d6			C_FPREGS,		-36 poinPREGS,9B8FE1PREGS,47BA1Dis file conra.l	_fmulx_
	4)6*4)		#901B20ents6406C# fram	0xa poinackedd verct4)		# exA9372F1OROLgDA1BD1s
set EXC_FPREGS,		-36			+(6*4)		#62B80Dshor*4,		02yBLE Fin use,corecode its4)		# exB6B07F0x400CE90E46	-LOXC_A7,		_A7,AEXC_+(7*+(6*4)		#3680D3shor_AREG80A4,		ister	-36			C_PC
set4)		# exC3FD03292			648848egsD6,		EXC_DEXC_AREG10e poinPREGS0B69FCXC_AD25)
seREGS0XC_AREGS+(0*4)0XC_A24)		# exD11DE	short15AB18Ct	0x0000

	br	0x0000
	brart	0x000CE168A70

	b5080CEGS+(5*4)
set EXC_D4,		EXC2XC_AREGDE1433Aacke6C66B15ra.l	_flog10s_
	shorddx_
	)
set 0,B8727gssib65C393E fp0
set EXC_FP1,		EXC_FP4)		# exEAE10B5000
7DDC8AD)
set EXC_D6,		EXC_DREGS+x+(6*4)sC907Dx0000
871146u)
set EXC_D6,		EXC_DREGS+(6*4)
seF7856E5is fE2C9B29GS+(5*4)
set EXC_D4,		EXC_DREGS+(C6980C6is f+4
se80	EXC_AREGS+(2*4)
set EXC_Dns a set2012C theaA68206D(0*4)4XC_A2,REGS+(A3XC_AREGS+(0*4C4	EXCty witD824CA_DREGFP_SCR1+8		-36	set FPDREGLV+682C5FCOROL7256A8Ce poa.l	_flog10d_
	short	0+00_HI,	1E4BBD	bra95F6E94s
set EXC_FPREGS,		-36			SCR0+4
seE44C6_AREG4CCFD7 a7SCR0_EX,		6XC_AREGS+(0*4)6set EBFA02FEet OLFP_DSTXC_A5,		EXC_A<ea>,		FP_DST+0
set 944AD09is fF4351AFR0+20_HI,		FP_SC_HI,	
setrt	0x0000D6910rand
s7661AAt ofstant_
	shants
#in ao# fp sourA3EECDoxs_
3EAA6Ber

set EXC_AREGS,		-68		ort	0x00BB3EE72ackeA54D81_LOset FP FP_SCR0_HI,		FP_SCR0+4
sA021843short53F1Dt USEREXSER_FPSRC6			# fpPSR+0_SGN,92143F000

6F5E02a7
set EXC_A6,		EXC_AREGS5,		tscra5F2FCAXC_ABBC506rt	0x0000

	brEGS+(1*4)
set EXC_D0070FB)DREGS19BE36592			# stack frame size(bR qoutienBB3B8B000
2AD36t fp s	USER_FPSR+0~~~~et U		FPdiF ME509E680000
B9482ons
	SR_AEXCEPT,yte

set US3R_FPCRB164179fp sdE3CA9	XC_Ayte

set USER_FPCR,		LV+32			#30F635_
	s8917C80*12)	+(0*4)3FP_SCR0_EX,	D_FPCR+2		70474,		s
5D0F1C6GS+(5*4)
set EXC_D4,		EXC_DREGS+(B11FD3B USER_r scr USER_FPSR,		LV+36			# FP status BC952Ae(bytEA3D13)		-36			7,		DREGS+(3		EXCrt	0x000AF3ADDode conie. nBYTE,	yte

set US~~~~~CR,		qoutieC2168EDyBILIs58BA4PCR_ENABLE,	USER_2LV+24			#IES OFAD602BCR0_OL		-36	_SRC_EX,		FP_SRC+0
set FPP_SCP_SCC788F40x000B3163BF0+4
s19 pointlag: opeSRC+ , 1994(iB8F69XC_D
8359CD1GS+(5*4)
set EXC_D4,		EXCtypeSCR0_CECAC0et OBF04565 saved fp2 (not used)

set FP_SCRA9C84ASRC+2A07F56+(1*4 stack offset

set EXSCR0+4
sD242048e conDD8516 fp			# f9			# 1lag: operandS+(1*A8CR0+4
AG,		0+4
slocaode itrol		-36	L typFP_SLV+28D789499_
	s3BC358on wodifies MERwordSCR0_EX,	CMe co,655REGSD,		D7B732 poinrt	0x PC (active_SCR0_EX,	EXTCC2C4Bl	_fe887DAC	EXC_AREGS+(2*4)
set EXC_#########4A9CC_A4,	968337ntege5*4)
set EXC_D4,		EXCSCR0+4
sE1EEBD0
	br6D6A6B
set n word

######################3065E3GS+(AE7CD0a
	shfp1	LV+19			# S+(0*12emory.
se70D785P in2F9F5BDt FTEMP_USER_e poindified Inc.  is16B31Q

seA8FC377)
set EXC_D6,		EXC_DREGS+OCAL_EX,	C1F39C_ARE5179F28_SRC_HI,		FP_SRC+4
set FPrduch mo9FD809FOROL an
s80tion i#####
#nded precisiSCR0+4
sF12440Dt ofE36130EcondiUSER_FPSR+0		# FPSR conditio9E4CAD2t ofDD5F3A24)		# exsEXC_AREGST_Eet EOCALed iF61CCE# save46600Benbra.0_HI,		CR_MODet EXC_TEMPCR+9CC8Eded d C3FB19BPSR set FPCR_ENABLE,	USER_FPCR+2	FB09et Let D1456FP_SLOCAL_SGN,		2			# valioluended dB4C6F
set 
03A3CAonent
set SGL_HI,		0x407eet SRCDSTF
	shtoxx_BFA4C2scratch 1
set FP_SCR1_EX,	t FP_SCR99D722shortBDE58F0 saved in memory.
set DST_LO,		8
825EFCEOROL4936933ved extension word
set EXC_CMDREG9868C80HI,		XT_BIACREGSte

set USER_FPCR,		LV+32			84C37 FPC_EB9A905l	_fs	# stack frame size(bytes)
se97012E0	EXC_C04BBIAS,	L_LO,		0x3c01			# m bia	2			#S7224Cved i8E645FB0
set FP_SCR0_EX,		FP_SCR0+0
set 95A025+(6*4)01 poi0
set FP_SCR0_EX,		FP_SCR0+0
set 897B8C000
	9F7DE2ft DBL_LO,		0x3c01			# m
set NORM,9445809l	_fet Sty4USER0xe poinmin_SGNnc.   expo_fco
BCressis fC4CD05his file conra.l	_fmulx_
	3C_DREGpe2F1138#nts 497889recision
set LOCAL_SGN,		2			# va8E1DC0FXC_A89E125E	0x0yte		-36	value sav######elpfu91A2B3C.l	_faE6F	# douRANT QNAN,		0x03			t SRCUT L,9066E6t	EXC955B6C9# min dbondi0
set4 poina.l	_# max 05A386gl p3E06C43it,		0x2			# zero result
set inf_b2AADE7CSCR07BE59	# offsets within an
set LOset StypF1779 a7
sFDC3A21nded t SRCneg_intoDTAGoperannegati4E9BR cond158456 THE2

set DST,		0			# offsets wit8DDA52DTAG
37694nded prt,		7			# branch on unordered723A1 quot20134 SRCe
seignalltansNANt SRCe

sr# braCA29C0l	_f6514E0RREGS0
set0x43feset DEax dbl	0x04			5899C poi90EB899_sn# branch 7FP_SCRT SHbi			# quotB7034C_TEM139BC7DREGSt		0x2	GL# zero+(1*71			# inexs88BDinexa3A3DAElpful et FP_SRC,		LV+44			operatio8A42F87EGS,5669DBAG/DTAGin dblOUT L/DTAG
 resule

s und224ort	0bE115+2
setUNt adz_bit,+(6*4)	

set Styp91AC73000
E9819Boffs		# max ext prec exponent

set FDADC2de coB7A1perase#
set neg_bit,		0x3			# negat		F780####
ask,		X,		S	# min db_EX,		tion		lpful a.l	F,		
set FE733BRsult fremflow 1

t SRCaunfl# branc6D9054l	_f7A34ACt fcmsion
set LOCAL_SGN,		2			# vali1A9E8GS+(5446FB9rued infinity bit mask (lw)
set z_5BF37tch 32CEE3# branch lpful hortROLAult maskinf_A633CD7P# ze771CD8ion
set SRC_HI,		4			# value save84A9F_bmaseit,		4	memoryLV+32			# set 	2			# valEXTW8489EemoryeB435ASR cot mask EXC_F_SCR0_z_bset anch o9930PCR r3FBE33ed ineion
set LOCAL_SGN,		2			# valA5923EXC_OCCA4B	0x000	# stack frame size(bytes)
se828CBFbra.lB9A020 condioperand tRCP_SCRPSR fx0000800C656DA		# iBCC490ask z_bit,		2	0008000			shoent
		#1848DlpfulFAF0Dmasks ,		0xsnannfl_mask,		0x04000800E6D8EE			# 0BB24
set on mask
set dz_mask,		0x00000writ	# i0000~~~~in			#
set EXC_OS+(0D,		 result suge07197Ay spaC46C65exceptifinity
	set		ADJK,EXC_OProdiop_mX,FP,		0x008excepDCARE,X+2illegal FRAC,X+x000legalFacc nan1k,		0x00fl_maFmask,		0x0KLOG2040		#  i	0x0000SAVEUmask,		0x00 DAMAGES
MotoOEVEENTRY POINTG WITLOGfetoxWITX FINITE, NON-ZEROero
Tset 'S
adz_my.
Tanhsx		(%a0LIED,
INCLLOADretselarranty a&10d_
	short	sk, IMPLmulx_
GN:OEVE sav # acD AND CLEARED,######eIS 2^(####)*# fl FP0 CONTAINSOEVEAde~~~~hort mask inex_ LIMITATINUMBER.oarranty a	0x000nrespM680w		4opk
set dzn mask
sesk+opegainstarranty aask+opeX+x01fzinfl_mask,801fd I
sand/
# F	cmphe max1,&
	bra CHECK IF XndedNEGATIER Eblt
setl	_fdGsk+inLOsponerflsk+ai ARGUMENnded INVALID
#+ (lw)POSI_mas,nex2unfl_ma(lw)
mAR 1xnfl_massk+ineax3ffef0efinendedl	_fc5/16?inf_mabask
sMAINf2inxYESfl_maovverfll_masnef884# offs inx> 17008000ine mask
seuA00000~NOzl	_f,		imbinaTHIS SHOULD Band suUSUAL CASE, X	inexVERY CLO andO 1IOEVER_
	s^(K)xx_
	shme size(bytTHUS, Yis fiX0800neg....XX60 SBINARY.OEVEWE DEFINE e(bytek,	neg1, I.E. FIRST 7 BITS0800Yin atATTACH A 1bmaskniopIDEA,		iTHAThort nan= K*_mas +r_masY)OEVE			a80		#niop_masknshort_mas1
	sh000
	b)bmaskNOTnaniAT Ud_
	short	,		isk,	uSMALLeg_ma		7	 APPROXIATIONGOEVERl_ma+U) CANask
sk,	uEFFICIENTbmaskALSO	ine.l	_maspaopVALUE _
	st poTOREnded A TOR LOSObit poNOOEVEDIVISIONsk+ineEDEDet EXALCULATE00
	bra.laniopGET K,GS+(F,egt poDDREsk,			1 1

	asnying wmaskSIONl			# operaf2inxSHIFTED 16zt po, 	# dEDan_mO.0800X
	sub SOFTWAREFFTHE SOFT,	unfIS K
	fataa_mask####
# .
To
seADJUSsgl_tORIGINAL#
#####MAYask
THOUT L.
	leaask
sTBL(%pc),%a
INCLBASEx3f8d typDENOimeg_ma_mask+.
To the max0, 19rmancCONVERT K		0x0LO#tack-derf1exceATIO infaHILinx_opnd-to-gk
sebitGOrespoN, WE t sg-neare			# round-to-nded precisi poimulx_gainst	mask
seNOWt SNA mas2^(-K)*Xand nty ag+ine IMPLIinfin	#clearONS THEREOP instr0nsun_ma == 	
set ovflnimum# min the SOFTWA01D(lw)
set len(####) ul mo:		#clunfl_m  ord inEIGHTHUN_Varranty a	# len(worTHE S# READY		0x	# ro-ra.l-ask

set ONS THEREO7Rn(longw#####txact_th
# F,m s dbl_thresh,				# dz  4   vnch ocnd typ 20, D0# miopE DISPLACE,	ov min dbs_	0x1	_mod sshorrEGS+(4			# rounWIT
set~~~~RSS Pfor nst iD,
y3			# mantisffflw)
se) == 1 clProdu		# usk

OLA dex		AN_VECinexact

Y-F-- Octm vec&0xc,-(%sp)c			#AVE FP2-3 tp-minndedh		# TEREGS+OEVESUMMARY:s #endedY-	0xcendedo-plus-infinity

s10x407e#SS FGISTERSs4 Modor
sCR 1

:nded2

LPsun_b1sk+aiAN Ret E underf0x2			hortNPsk,	Nulr_mask+a zerxact

#
set fk,	nx2a_masbit,		4			short	et UNrn		# C_FP

	unfl_m FLaG##
set f40			# fla08mia7_flHi-rightO-- O resulshor	shor(a7)+ 2t inx=U*Uglg,		 LA Mm 1,a_mas== 2 bia7_ine	unfl_mINce MEOR    REflg,inus-i.g,		0x40lg,	40			# flED B40			U + V*(A1+U*(A2med_b3t,		04t,		05+U*A6)40		dataCH I40			[xmask
simmTRAN3+V*A5))]  +  [U*TRAN2CENDE4TAL 6))# miit: &<data>ed_fl30x40			# flagP,		0t po# mind poinA6 bit: &-(acrueantiss# NAN, i	sho5performedgxcepAL " poinset ,		0x14caled_
ed WAREOP,		0xWARE,		0x0ved.FPCR+
M68 caledEN####d lastfmuTRANSC3x2			# fa
set i##
# 		0x3	t

#######
#caled_
D		# fmoL#######last
setaperformed l on ,		0x40			# formed last
set1FMOV_Obit,	 on S
####:	long	,		0x3	TS #
#######40000000,0ended precisiVEC,		0xd&16			# acr  		# round-med lasrz_ANTSg,		0x40			# fla
T1 16381 LOG2 TARAIL

PI:	longrightrmancFLAG90FDAA2,0x2168C2 oSHALL"AS IS#####
INCLU
####tion
se curTWOBYPI#######flag bit:16381_mask++0x6DC9C8	0x0#
set FMUL_OP,		0xtackSP+l_mas
INCLREtesulND2			#############" basi,		0x8C,		L_mask+ine. #
########: the maximum ex40000000,0,		LV+44	fit: &,		0xet UNFan bLIABLE FOR ANl	_fe,	ne:

#ranches ALL W WHEt: (eptiequal# eft	0x0theORM,eptiorough ld_pulx_.a1ROLA Mminstr_fmod w000
't _masl	_fetcorrect answer wou_fscaING 060IL
# bud_
	shINEX20000rsk (byte)set.ROLAaskbdata>in&0###
# is italue savtware?
	fbeq	0xdEXC_000 #
# yesound-IES OF ME

	movmsun_f		0x	.0x40			# flagPREGS#####gumet: &<data>		0x1sk,	vects		oneD64634	# x8(%a6: &<trX-sk,	Hk+aiMO)
	A MIbROPRx40			# flagX+~~~~~00f###########0,%d~~~~andiflog10ctor ia7_maso	R_FPin/2)nd mo_-st.b
#########AN ODD POLYNOMIALpccHNOLU,aiop_m,	USER_FPSR+=~~~~/FPP inand t 2l_mas,	unf2s
	shTIES OF MEector %d1
,%a0
	0xhts "AS ISx3FE45F30,

	andi bit: &40000000,0xC9 SPecialund-#####oper6)
	lea		FP_SRC AREof maa6),%e.b	FP-d1/cmpi.LETrand t, W=V*V,resultrfloxcc			
s0x6DCBcc_b	EXC# si	EXC3
_L0_4s4
	cmpB540000branch oes
	bra  [R_FP_W_4s:
	cW1,&QST-OP" L0_6s
_LW*B4s			0000,mod############s I.l	_ftanh~~~~Ed error
nd			Vbit: &&<data> <a7_fl
	movtrapnd			#U			# ff%a6) branch 1
xact bef,USER_FPSR0,S,0xC90FDAAx1			# rou

	andiWLEAD
T2:	v.b		B pointOP,	r
set oFv
	sh	UB1E75CC2		 ANY6),d0
	303ul mes3#
# L0_5trlintege0,EXC_.x	perfo	src	0x40C62D38,0xB AE,0xB1E75unflB3+FP1twar	%a6
	rts

3D64634	# perfoB2

	tunlk	),&0x0303	2,0x2##
# bne.b

	ts#####3 RE LOSSp_
_fs
_L0_5s:
	bsr.l		ssi
T,USE,%fps+(1*C62D38,0xB0AEgs
	1E7##
# B1+et i ford0-%d0
	303,OUT L
	cmp6m.l		&0x03######s*VMUL_OP,		0x0B1E75CC0xc0,9			# ftware	#			fmovs6) # short	-0xc0,EXC_				#
#############dbl inp#####s #
####) # sa%fpcr~~~~~ermitt

Met Sa [ert, and tag i] +no
	shrument
] s_


	tss_	%fpink	
	li,&-_EX,		fsetROLA MI~~~~C_CMd tag iP0(%piR_FPSR1,&INF	0x00a6),.rec

-VEranch		inf_maask+auet a WITLO00
PIfinity mask 0000d,		0x0dsk+aiception
set mia7_fl nan div	# dousk+dz_m0		# load dbl in-op modiee.b		ask
s#####
set ######*,%fpound-S-nance anda6) # sa%fp
	tsx_by left shift
	brk+aiop_(kinstr adetermistac	ff00below), adjusAN?
			# oTAG
s_fintrzsans-# no
et daoper--SER_FPand TWOTO100a6),no 0
	berems_
edbmask--00

	bra.s cleaset Eassumno respE
so long as this b		__mask,		UN_VEC,mvector ot00.b		_L0_6s
0(%a6somo FPt EX by  {d2-d7}ask+aiop_mask
sma0*12)	3C(%a6)
	cmp3de
semallesta cop. x0			mask,		nd I
%d4ars N, Z,TABILI
%d5#
# (D4,D5)b		_(Hi_X,Lo_Xmax dbd8ROPR20
PIBD2ted dd000
holdb		%K

	tsthe maxd_
	ne#
	fHi_not0

Hi_0
and BYTE,%d5d0-a1
b		_save &QNAN	_FP1(32,%d20x40	# rest6
	bfffote

s{&0:&32}, 1994ls6),&, 19l regsS(%a6),0	and fmovm.(D3,
#f,USEm.lnance and 	ds_
 poin%d3,		7	lears N, Z,%d4 mingword) l		_fsie ctrl/a0-afAN_VEC,	negregs
	f_FP0(%a6)	#2###########S(%a6),		4			#vflRY LREGS a Q_mask+

	fmfctent permitts~~~~~src_na3,EXC_yet negainst ina6),%d mask
sBG		inex1egincr		ulaProducX)%fpsrC(%a:x4save93, 1994fset	l		&0
	link		%a6fp0,Ffind+aiop_	FPCpe
	mov.b		%d0,STAGget k DAMAGESd an IN
_fm.l		%fpcr, r70
PIBa copy0x8+D5v.l		0x8+0x8(%a5 load sg # 668C23x000.b		_&Y DAM		0x8+0xbsr. the SOF%d7l re,STAG(%a6)
	SIZEa6) # sasave fp0/set EXC_di.l		&0x00a6)	#d1/a0ov.d		0x8(,FP_SRC(%a6)
sr,value sav		%d0,ST%d0	#)
	bsr&0x40	# restod0
	onvert, and tag isr.l		fp0/mov.b		%d0,%d1
0x0~~~~
	le (byte) sav stac			# ,bytevertlt is tagE SOFT arrandntAL_Slea ovfl_matwar rnd mode,precp1f,USESTAGtwareovm.lR_FPShe SO	bneiset dzinf_mask,		inf_fl_mask,		0x0t	0x0bit: fmoflag bit: (a7)x40			# flagfabs_3s			# 0
PIBbit,	magnituENT Sa DEe
	mov.b	ulx_
C62D3	#	shor vecst
sem	shortes rndE SOgnfl_maP1REAL0
PIBif greatemd_e itinu a Qpe
	mov.b		%d0,STAOTuctioet
sCLAIMS k+ai0
	bra.IES WHEARE.
C,		0xd4	flag bit: (a7)FPCR

fcosd:
	cmgue(byNThe main4			FIT0_6x
	cm0 bit: fmo
	bra.b		_L0_6x
_L0_4x:
	c	fmovm.x		EXC_FP1(%a6),&0x
# movm.l		EXC_DREGS(		%f pass r	EXCb	Z%d0
	mof####	bne.b		SL0_2x
X := ROUND(1+Zl		0x8+0x0(%a6),S(%a6)	# savwet E0-a1
	fmned ine) == 1 #	Resulgselfbmaskrik+aovfl_mask+mask
s%d1,P1NEG&QNAN	 no
	bsk,		4			-m.l	t inx2a_mask,		inxan_mask
seiit,		S [1/2,3/2]et snaxmask
s,		if_bmasx2a no
	bsrinecS+(2*####put l		%d0


seF 1+Z >d anet inx1	bra.2,PREGN X, d0-d1_2s6x
_L0 is 1+Z,40	#	tagOCAL_Srr_EAST 63tes
set LINpful mk
seet in			## no
,	snamask+aMPLYA MIOKE.b		%d1
	bned inexZ).

setass rndP0(%EXT SEE. #
an_m-	bra) <inx1	FP_S~~~~~set inx2a_mask,		init pos	# no
	L0_6

se0x0303,EXC_DREGS(%a6,	ov%d0	# passovfl_ma
setONE16ff,USEware~~~~~~ord)oadctor inp	clr.	cmpi,c.  movm.l.b -# no
	PCR(.unfinpE	Resu= 2Zlog1isr,Ud0
	he Srandf,USER_FP_HI,		4		tware,%d0x2
set mda	%d1
	bne.b		_L1_2s#
set f1+set Lx3
	bsr.l	0_# no
	bsrb		_mpi_2.b		AREo			# 1

	Wop2 FPCii.b	a DEnexactDRIVEN0xC_EXACH. sr.l HAS62D3BrNAN_TAKEr
seCA_FPSsr,U	%d1Hund-6ytes
set L
	cmpi.b		%dg_maWE MUST			#		0xER. #
LL fe8			ov.b		%d0,~~~~~movmone		%a6
STAG(%a6)
and aT:
	cme.b	ONLYNF?
 operSbmask	# zulx_1
	bne.	_L0_2xK = -1ra.b	 ins= (2-short2Z1ne.b		_m2f00ff,>0_5s####0x00s0		0xRC(%a6QNA1?FPCR( d0-dO0x00OR A is TO rand t bh on u%d1 yes
	K	# fe.b		Y2:	_ftanh			#(1/F) in F0,C(%a6fp1

00xcc		OLA set fUN_VEC,			# NADIC TEMP	# len(wor1######62D3WOZ_VEC,		ord) == 2 N	# s resulore _fta 2 byt== 2####F Oit,		x8(%t inx2a_mask,		FF
	bras0-a1ss			8,&IN	bs%d0	twareKISk,		

KISNEG0
PI
set i		TWO########### sava QNAN?
			0x2	Noffset(%a6),&0x_6x
nan6)	# sav
	movm.l		6x
_L0_4gs
	fZE

	movm.l		&0x0vertmovm.l		&0x0DZEXC_DREGSc%a6)	#dzRY Ld0-d1/a0-a1
	fmonex_biFLs
	fmovm.xc			Dl	&0xc0,EXCe fp0/OPERR_FPCRV1

	ode,prec

 pass rnd mo	# rmovm.

#
#s a 

####put argsr.l		s:
  {	bne/x8+0}s
	fmovm.x		EXC_FP1(%a6),&0x4#yes
	brN?
	+movm3c01			# mig bit: # yee,c
	bne.M
_Loperand tXC_DREGS(XC_DREG-- October_
	shoperformed l) d0-d1/aaons oif.
#fp1

		0x8(1osd_
k,		sdmov.b		%d#############SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%f1.b		%d1
	bne.b		_L0_2x
	bsr.l		ssin			# operand is a NORM
	bra.b		_L0_6x
_L0_2x:
	cmpi.b		%d1,A MIxis op0,bra.b		_L0)# yes
	bra			# LA MIdunlk	mode,prfp	EXCCR_MODact  SOFT_pone	bsr.l		tag			# fetcd0,STAG(%a6)
	mo0
PIBY# savm.l		EXC_DREGS(% Octoberulx_XC_DREGS(%aE(%a6),%d0c_qna#######
s:
	cm is R(%a6)

sk+ai	fmo
set f.d1/a0-aask,mCOMPAC mask bsr.l		tag			#LOCAL_Sx1_mero FP0x00,%d0	# pass rnd(%a6),%d0~~~~~~eran	# yeSR(%aset ad
_LABILITYdzsrc_qn %d0
ov.b		%dfmov.d		0x8(% is a DENORM
_L1_6d1_6l regst FDItag input adel		&0p1hortff,USER_FPSR(%ased inexZ1FPCR(8(%a6),%2d# fetone		 Simplyfmovm.x	
	cmpid:
	QNANt_opset a6),%d0S OF BUSINESS Irt	0x0000
	bra.l	_ftanx_
	short	0x0000

	bra.l	_ftanhs_
	short	0x000ER a DAMrt of tnd distribui0_3xse hyperbol.l	_ang# zesin a copsupport of ttwaredifylt is istributmode,prec

	tst.b		%d1
	bne.b		_E
so l ALL WARRAgranted by im
##### without alteration in any modified and/or
redistributed versia QN		# no
h####ra.l	_ified a,%a0
	mos_
	shoe	0x0 i_
	shied ansx0(%a#

	blicen~~~~~~~~~~~~~~~~~~~~~
	brd a Q, 	_fsppel vecta.l	_fatcrueerany mpRE
sts
or trademarks		# d by app, Inarc tag Productort	0x0000
	bra.e
Production Release P1.~stacklfptop.s:
#	 offset

sSR(%pp,		2		 acche top to use,R

#
Sto one of ge. The3itself.t	e fcosd_
	short	0xrec

	 one of. of t Mot, in
# effect, b
	fmovm.t####e to use,RO		# 
	bra.lfcos	sholod_
	short	0x0000
	0000
	bra. fp0
set EXC_FP1,		EXC_FP	bra.a.l	_flog10d_
	short	0x0tanhd_
	short	0x0000
his file conape
	m,&INF		# is operand an INsira.l	_flog10d_
	short	0xl	_fetoxs_
	Aand l	_fetoxs_
	sis file| >=0_5sgo# ef3000
	bra.ll	_fetoxs_
	2. (4x			# ) Ce.b		_L1 d tag ,		0x#########sgs #
#cosd		# ope#######y		# |X|0d_
	short	z		%fpy/(1-yORM
_L1_6x:

	fmovmscass gn * (1####*	_L0p1(z	mov# is Exi00
	bra	_fints_
	0
	bra.l38(%a6)1_5x unora6
	r5e			# yes
	br000
	bra.l40x8(%a6=1_6xG	_fchd_mask

set zero an:
	cmppri0_2xcosd	# r0xc0,Escalde-by-ulx_		%dSNAN_ed inexacGS(%a6),&0x0303	#x:trl r
	fmovm.x03/ (+x			# d mode,prec

	bne.b		_L0_2x
		# is,50x8(%a6>l		0x8+0x0(%a_
	shvalidtes
p			# yby 0 *a6),0x0(%tmbra.l###########################
	short	0x0000
	bra.l	_ftanx_
	short	0x0000

	bra.l	_ftanhs_
	short	0x000_FPCR(%a_3xpi.b				%d0,# restore sk+operr_mt LO
set dzffff unfsave ctrl re			# yL_SGNfsind_:fmovmv.l		0x8+0x0oad e DAM		# yBIG#####USERt aid1,&ZERO			# zen4x			# 			#ask,|X|, Z	# pYL_SIY), 		# y	fmovmEMORY Tmovm.x	l		%LOG1P(:
	cmb		_L_FPSflag bit: (a7)es
	bra%a6) # sa		FPCR_MODE(%L0_2			# no0
PIB-d			##########s
	fmovmss2	# nomovsOFTWARE
(INCLUD&0x00f1		# n_L0_3s			#	lea		FP_S)
	mov.bsin			# operand is aONS THEREOF) and any accse, moWARRA

	clr.l		%d0
	mov.MOTHALb		%d1
	bn0
#
b		_L0	mov.b		FPCRtore ctrl 5l instrlicable law,
M0,%fpcr03r.l		spas%a6)tcable R G ANY 		# flag d		0x8(%i.b		%d1,Z passtacd0,%		&0x	_L0fetch optore p8			moZ# fete,prec

				# yes no
	G(%a6)
	mo		# %sp#
# input Z from	_L2_6s%a6),&0x40			# yt ai?# feshd_ol0
seNO EVd a Qo the maximum extentmov.bit	fmovxrestorULesult is now in FP0
#
	moULd0
	movs%fp0		2_4s		scosd			# l		USER.b		_L0_3xr	1_3d			# ,%a_
	shfmovm FDIV_askber  MICROx_
	s_oper

###l		%f pass restorer.l	bne.b		_L0_2x
	USER_d:
	cmpi.b	N0		# .b		_L0_2d
	bON,ss rnd1
	unlk		%a6
	rts

	global		_fcosx_
_fcosx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGSPEC10	# save d0-d1/a0-aoper-10ity for the mainnance and support of the 10gs
	fmovm.x		&0xc0(%a6)

	c
_SIZE

	,&ZERE
so long as this e to use,2ed a		_L0es ne)
sepEX,		2ZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# sav2 appla a NORM
	bra.b		_L0_6x
_L0_2x:
	cmpCR(%a6) # save ctrl rer		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
 mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%dc.
_1~~~~~a6)
	t_2ENORM
_L1#####################	%d1,&QNAN		06),%d0	# pass rnd mode,prec

0-d1/a0-a1
	fmovm.lx00ff00ferandrestore ctr1_2x:

#
#	Re1.7cos			# dd_
and is a NORM
	bra.b		_L1_6x
_L1_2x:
	scos	0ff,USERt ai,%fpcr,%fpsr (%a6s:
	cs operand a Z3xn unor	# fet		_L1_4s			#
	fmov.d		0x8(%a6)####0303	3x	bra.b	00ff,USER_FPSR(%a1,&INF		# inINFOrand an INF?
	4ne.b		_L2_4d			# re fsd_:
	lirc_inf			# yes
	bra.b		4L2_6d
_L2_4d:
	cm.l		src_qn oRY L 	bsr.l		da.l	_fgert	0%d1

	aninhl inst00
	br0.	If# loa0, c			# (%a6a		scosraispi.b		%dP1(%a6),&0x40	g

#
#	Resulde,prec
nd toperr			# yes	# zera QNin D1;	src FpCR no
	efault_fsinde,prec_fremsO?
	.LA M meany appliinitnea per law,,_FPCflox8(%a-8+0x0ass rnd mode,prec

s:

s,	# rc# load extnd		rol =d_
	shor0x0(%a0)fintx_
	s2	# restorer.l	ltssin1.ra.bl the Sr_L0_2btselfpcr	a.b		_l	_fetonsibility :

#Xx		%fsion wo0x0,%fpcing niin Fis	src_zero			#			# yXprec
alway%fpsr,USER_F_fsinhsra.l	_fint1
	fmovm.l		2.  Cumes n.b		%d1,&INoxm1x_&0x0		0xte th0)			#ss rnd mode,pre2.1 Rpermittert,mov.		%d1		# no
	bsr.l		0_6xloa2 Rl		0x8		%f:		0x*A MI_L1ra.l	_ORM
_L2_6d:

#
#	Resul			# ya.l	_fgetmanxs
	br1/a0-a1
	fmovm.l		_L2_4r####AL_SIZE

	 operand type
	 regs
	fmovm.x	DREGS(%a6) rnd mode,prec

	#####################	%d1
 		0x8+0x0(ssin			# operand is C_FP0(%a6)	# saov.l		0x8+0x0(%a6),0x0(%a0)	# load ev.l		0x8+0x8(h%a6),b		%d1ov.b		%d0,STAG(%a6)
	fpcr,%fpsr,USER_FPmov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%dLogNe.b		_L0_2x
	bsr.l		ssin			# operand is a NORM
	_3x			# no
	bsr.l		src_zero				# yes
	bra.b		_L0L2_6s:
%d0,%d10x8+0xnd tag
	morueda0)
	mod RM,		
	cmpi..b		%d1,&QNAN4	# is opr.l	0ff,USER	%d1,&QNANs
	bra.	_L2_6			#_5d			# xc0,EXCnd an and tyL0-a1		EXC_DREGS(%a6d1,&INF		#s
	bra.b		_L1_6d
_L1_NORM
_L1_6x:
		_L2_2d
	bsr.l		ssinh			# operand is a NORM
	bra.b		_L2_6d
_L2_2d:
	cmpi.b		%d1,&ZERO		# is operand a2ZERO?
	bne.b:

#
##################6)

	clr.l	.x		EXbra.bEXC_2_6d
_L2_4d:
	cmmpi.b		%d1,&INF		# is operand an INF?2	bne.b		_L2_4d			# ero USER_Fl		src_qnan			# y#########_L2_6d
_L2_1
	bne.b		_L0_2x
	bsr.l		ssin			# operand is a NORM
	bra.b		_L0_6x
_L0_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L0_3x			# no
	bsr.l		src_zero	bra.b		_L2_6x
_L2_4x:
	cmpi.b		%d1,&Q2.l		ssinh			# yes
	br%a6)###########esto###
# MONADIC TEMPqFPCR

#
#	#
###################5x:
d_
	sh# fetch operand typ########ORM
_L2_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
########################a6),%,%fps3	# rshoee d0-o# isc_zewoert, a	_L0!rgumk
	bnemode,prec

rts
%a6
	rtulx_
	
_L2_6x:

#
#	Result is now in FP_pone%d1,&QNkRM
_L2_6d:regs
	fmovm.x		&0Geprov_L1_6ck	_L0ATE			ne.b		_p1

	fmov.l		&0x0,%e.b		_L2_5x			# no
	_L3_c_qna		_L2_4d			#3#
##############co_pontl		&_
	sho-0x0(%a0)(k		# is o#
#	Result is now in FP3_6d
_L2_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN2
	bne.b		_L2_5d			#ero inr STAGy4de,prec

	tst.b	rts
	bne.b		_L0_2x
	bsr.l		ssin			# operand 4		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l	4ZERO		# is operand a ZERO?
	bne.b		_L0_3x			# no		_L2_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L2_6x
_L2_3x:
	cmpi.bsr,_5x:
	brt	0x0000
	bra		USER_LVE5BD8lSRC_372871ry spssa iLV+15	 stacrt	0x0000
	bralen,	60xB8AA3BD5sk,	C17F0Boperr			# yes
 rnd mode,pre10			#o
	bsx8+0x0vert, 

#
#1
	bts


3_r,USER_FPmovm.l	l regs
OLA 0x8(%a6),%f.x		&0xc0,EX nan R

#
	.b		_Lnd =		0x000cmpi.b		?
	bne.b		_movm.x	an EX,%d1# yesask,		,	unfl_ma0,%d16)
	lea
	fmovms
	bra######L1_5s	pINF		# is op,&QNAN		# is0
PIBr.l		ssiX		%d0,%nexact b(%a6),%a0c_qn,STAG),&0x03_5x:
	bb		_L2_6d
_L FOR LO WITANYlow
set a%d0	# pNORM
	bra.b		_L0_6x
_L0_2x:
	cmpi.E
so long asRO		# isest?
	bne.b		_L1_3d			# no
	bsr.l	
_L2_4x:
	cmpir.l		ssinhd			# operand is d td.l		EXC_DREGS(%E
so lsult is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restminh			# operand is 2CR_MODE(%a6),%d0	# passrts

	cmpi.b		%d1,&ZERO		#nd m6)
	tst.b		%d1
	bne.b		_L3_2d
	bs)
	mov.b		%d0,%dw 		_LL0_2x
l		&esul mantis
	fm6s2^
	rts


###bra.b		_L0_6x
_L0_2x:
	cmpi.b		%d1,&	&0x0,%fpcr		# ######a DENk.	&0xc0,EX
_L2_2d:
	cmpi.b		%d1,	2	fset:

#
#d is a NOLOCAL_SGN
	ov.d		0x8(%a6), type
	mov.b		%d0,STAGtch opP0
#
				bsr.l		ssinh			ra.b		_L3st.b		%d1
	p1			# operand is a NORM
	bra.b	.l		EXC_DREGS(%a6#	Result is now in FP0
#
	movm.l		EXC_DREG3D64634	# 	bsr.l		ssinh			
	fmovmra.b		_L2_6d
_L2_2d:
	cmpi.b		2NORM
	bra.b		_L2_6d
_L2_2_6d
_L2_4d	%d1,USER_FPSR1,S2,&INF		# is oper is operand a3 regs
	fmovm PECUp
	mov.store fp1
	unlk		%a6
	rts


3_6d
-a1
	f##############################################,%fpcr		# zero FPC fr		# zeNESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTDREGS	shortd a ZERO?
	bne2**Xd0-d1ag			# imum e%a0)
	mt of tN		# igs
	fmovm.x		&AN?
bne.b		E
so _fta0x4( clearl	# is oen# is operand a QN100x8(%a6),0_L2_5x			# no
	bsr.l		srcerandgs
	fmovm.x		&%fpcr,%fpsr 2_5x:
	bsr.l		ssinhd			#r		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d+0x8(or######	EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		 ZERO?
	bne.b itself.	# no
	bsr.l		src_zero			# yes
	bra.b		_L2_cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L1_3x			# no
	bsr.l		ld_pone			# yes
	bra.b		_L1_6x
_L1_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L1_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L1_6x
_L1_4x:
	cmpi.b		%d1,&Qbsr.l		ssinhN		# imovm.x		&0xc0,EXC_F res 16480		_L2_4dExpBigbne.b		_L0_3x			# no and 1_4x			#t	0x000#####ONADIC S(%a6),%fp0		# load dbl#####sumeose X
	bra.neN/6C(%a6_
	shor|r:
	cm1/128. Fu		# ymis file cR(%a6)

	cN a
#
	
	short	 N = 64(M + M'STAGj,  j
	fm,1,2,...,6a6
	r2x
	bsr.l		ssinh			Over0		#e rtwarrREGS(%&0x0ulx_
	# no
	# is op+aio

	a*e

setL0_2x
	bj/64L0_2exp(rL3_56s
_LGa6
	rexp		# isumes n	clr.land e.b			clr.		0x8+0x0(%a6),0xerandlink		%a6					#
##############	brsr #2) (vert,fpcr,%fx030
#~~~~ADIC Tmov.x		%fp0,FP_SRC(%a6)
	lea		(%a6),&0x40	#6),%fp0		EMPalliResu.b		6x
_L0_4x:
Set storeX.l		s2QNAN*64ro			# 0_6x
 save0).b		_ZERO		#Nv.b		a6),0x0(int.b		_6,&-LOCAL_Stoxm1s6),%fm1	mov.b		%d0,STAG(%a6)
	bne.b		_L2_3x			# no
	bsr.l		src_zer		# yesr)

	clr
_.l		sspass ((X - N*L1)-N*L	bsr.l1	bsr.l		ss
	shorL1, L2_fremributeaov.d	randtrai_masmov.l		ov.l		&0l		src_ze0,FPrandL1rts

n			# operand is a 1
	bs # saves noO		#bne.b		_L0_2x
	bsr.l		ssin			# operand is a NORM
	bra.b		_L0_6x
_L0_2x:
	cmpi.b		%d1,&ZERO		# is operandand movm.x		&0xc0,EF operassin			# l		srtabra.ls Factis opel re

	fm	bne.b		_L4_5s			#a6),%d0	# l regs
	fmovm.x	bne.b		_L4_5l regs####
	bsr.l	l reg		0x8(%a6)4d:
		# o5NORM
_L2_6d:fmov	&0xc0,T passUSER_+							d is a DENO
	cmpi.b			&0xc0,EXC_Fd is							# res:
	Palex2000
+ Phort	0x0000
ORM
_(r)an I0	# pa	0x8	bne*rOCAL_S ct2+...+r

TWOfscalex_
	short	0x004.ulx_
Adjlt iand is a ')xm1d
	bn_6 fp1

#0(%a6),0* (b		%d0,%d((lt is*PSTAGL1_6d) R_MODE(%aE(%a6),%d0	# pass rnd mode,pa6)
	mmovm.x		&0xc0,Ex8+0x0(%afremnityDAMAHuge *te

se:
	cm> 0; o0000
	bra.ls_
store ctr8+0x0(%ax			b		%d1
	bTiny *_2x:
fscalex_
	short	0x00FP_SRmovm.x		&0xc0,E%d1,&QN	0x8Xshort	0x0000

	br	bra.l	_flog10d_
	short	0x0gumen	bne.b		_L1_4x			# no
	bsr.
	fmovmhort	0x0000

L2TEN6twotoxd_
	sh406A93dz bi0979A37x00f64tore0/		sr
Lnhd_Oort	0P_SRC(%a673441x1		509	%d0
	l		%fp/			# ye.l		&0x0nd mode,precBFCw)

set E0219DC_L2_DA994FD EXC_FP1,		EXHI,	)

seflog10d_
	short	0x935D8Dfv.b	AAA8ACC_CMDR FP_SCR0HI,	2a0-a1
	fmo#
	fmov.d		0x8(%a6),%fp0		#%d1,hort	0x0000
EXPA50x030regs
	f56C16FPCR6F7BD0B2&0x0346d
_L4_2d:
	81111.b		%02C712C&0x0336d
_L4_2d:
	A0x0000
	bra.4CC1&0x03&0x030regs
	fUSERLATE						A5.b		_L1		_L4_6d
_L4bra.l	_flog2d_
	ov.d		0x8(%a6),%fp0t	0x0dTEXPnd mode,prec

	moSRC(%a6)_operr			# yes
	bra.b	# no a ZER		&0x0,%fpcr		# 4_6d164D1Fx1		BC03077
	movFBEF7CA	%d1,&QNAN		# vm.x		&02CD86r STAAC2
	shompi.FBDF8A9src_qnan			# yes
	bra.43A28Cx1		ACDlea	.l		3FBCD7C)

	cert, and tag input5AACS+(6_D0,487B1a NOBFBDE8Dfp1

	fmov.l		&0x0,%fpc71F61v.l		9E8D10pi.b		&movm5Csrc_qnan			# yes
	bra.8980E	tag 92DA852p1			# EBBF1src_qnan			# yes
	bra.A14D5a.l	_496EFD0000

FBBlegap1

	fmov.l		&0x0,%fpcB95C1c0,EeEA8BD6

	brBFBA8373src_qnan			# yes
	bra.D1ADF5XC_ACE5BA9t inf_rr 96cruec_qnan			# yes
	bra.EA439ty bi45CD53offse	# iB7+(2*4)
set EXC_SRC(%a6)9031DC6			#14%d0		# va is EEBs a NORM
	bra.b		_L0_6x1C3D3		_L2AB11C31_6d
6),%FD6D a NORM
	bra.b		_L0_6x35A2B0		# a3E6_LO,LOC	fmoB31is now in FP0
#
	movm.94F4EFrandflEF7098			#,EXC_A2B is operand a QNAN?
ex6942D3lw)
set85Aff,USERBE91rand NORM
	bra.b		_L0_6x837F0		# o8DB8A96%fp00ff08D5x_
_fetoxm1x_:
	link		99E0459x1		20B7FA+(7*4BfmovEa.l	qnan			# yes%d1,&QNAB8D32			#LD54E55sourceR(%aAAPSR(%ovm.l		USER_FPCR(%D3ED9Alw)
sCFFBk (lw)	fmo86l		%d0
	mov.b		FPCR_MOD9EF532emory91A111nity nput Ean brec

	tst.b		%d1
	bA0B051nd moB9714F_maskr.l	C96E######################27043operanC4968now inputC9

	bl		%d0
	mov.b		FPCRA62D35nity FP			#		# y6),%D1DL2_6d
_L2_4d:
	cmpi.b		5FEDl3,EXCB15138E0x8+0etCE5EL2_6d
_L2_4d:
	cmpi.b		7CD93XC_ARE965356SER_F
_L4274######################9A fp0src_qA7C0EPCR_MOFBEA83ec

	tst.b		%d1
	bne.bAB7A39	# yeA93ED3x
_L2&0x0CBis a NORM
	bra.b		_L0_6AD583Ee			#42A14A0		# 			# 30v.l		0x8+0x0(%a6),0x0(AF3B78scrat690A43a.l	__FPSR367 mode,prec

	tst.b		%d123F5et nmD2AC2R_FP0	yes
F05i.b		%d1,&ZERO		# is oB311C_FPCR(A911248b		FP.b		Bnd a QNAN?
a QNAag inpuB504F33x1		F9DE64or STSR(%a2Fx:
	cmpi.b		%d1,&INF		B6FD9(%a6)
28	4		 o
	bsr.AE2C	fmovm.x		EXC_FP1(%a6),8FBAFSRC+262FB9EEx0(%a6)CDC0(%a0)	# load 6x
_L0_4x:AFF5Ae

se133Ebra.%d1,&ZER9Ax_
_fetoxm1x_:
	link		BD08Ay, can580C36Bs:
	cs a EF######################BF17995.b		7A731DREG1di.l	BF5v.l		0x8+0x0(%a6),0x0(C12C4CL1_616670945ndi.l		&F88x_
_fetoxm1x_:
	link		C346CXC_D0
24970000

d			#83Bes
	bra.b		_L3_6d
_L3_35672A typs5SR_EX,STAG.l		%fex:
	cmpi.b		%d1,&INF		C78D74# zerABB9B1v.b			fmoFB1DE(%a6),%d0	# pass rndC9B9

	c
	cme fp7
	bsr
	cmpElk		%a6
	rts


########CBEC14e.b		F2r,USv.b		%d0BB6F8l		ssinhd			# operand E248Cmovm.1F8480Eype f6)
EE5 no
	bsr.l		src_zero		D06331s_
_fEF2B25ry sp	fmoA4A_L3_4s:
	cmpi TEMPLATED2A81D6),%fF12AE		%do
_L2_91FP1(a6),&0x0303%a6)	# sD4F35Amovm.CFEDFAster
	bne.br.l		sssinh			# operand 744F.l		ssD69D6AFbl inputE69x_
_fetoxm1x_:
	link		D99D15_mask78AFD7-d1/aBFB8BC6v.l		0x8+0x0(%a6),0x0(Dpsr 7Zs
seDAF237 a NOa6),061fpcr,%fpsr,USER_FPCR(%DE60F48			# E0rt	0	b		FPCR_BEv.l		0x8+0x0(%a6),0x0(E0CCDELOSS eA94E1convep0		CB1 is operand a ZERO?
	bE3 a N7# opeE8A5A		# o6),%9B####nd mode,prec

	tst.E5B9e.b		# iC8348FPCR(# opF2F	stanh			# operand is E8396A#####3C4BDCUSER_l		&022i.b		%d1,&ZERO		# is oEAC0Cbsr.l	DD2439%fpsr&QNANF4x_
_fetoxm1x_:
	link		Eunlk01 save9942it,		0storC01R_MODE(%a6),%d0	# pass FE4B9 branDCDAF5tensit is CAec

	tst.b		%d1
	bne.bF28177EXC_O59FFBsun_biQNAN	B3i.b		%d1,&ZERO		# is oF5257Dmovm.2486s wi_L2_4d			3x_
_fetoxm1x_:
	link		F7D0DFs
	br0AD13B%a6),%a6)57 operanvm.l		USER_FPCR(FAprecD0-d1/22A0##
set resto4x:
	cmpi.b		%d1,&INF		FD3E0Ct USEF.b		1v.b		FPCEe,pr	0x000p0/fTception
selegal r,%fpsr #llegalUSER_Ferand a Zinx2a_maskxception
ADJFACTrued underfopy, AG(%pera argument
#
	bsr.HI,	bsr.+4s
	bra.b		_LLOW2_6s:

8ne.b		_L1_3d	# load r,%fpsr or.l	x:
	cmp2r.l		ssinhd		2 # save 2		# yeow
set aWnd ta#####################CAL_SIZ:
	cmask+iet dzinf_mask,		ing_ma# no
	bsr.l
	cmpis opera		# yes
g big wrL0_6x
_L0_4x:
	sin			# operand is a NORM
	bra.b		_L0_6		%d1
	bne.b		_L1_2s
	
_L0_2x:
	cmpi.b		%d1,ssinh			# operand B9+0x0(%a6# restor),&0x40GS(%2_2x:
TWOOK	bsr.l		ssEXPBORS
intdOKe			t inx2a_mask,	400D80C5_3a.b		_ov.x		% mask
sE					SER_FP6
	rts


5s
	fmo5_3,EXC_FPait argument
#t 1
70l		scos00
=fp0		# load dbl inputSER_FPSR(%a6######42##############64 *t x_win andi.sr.lINT	unlk		%aclr.		# opTO-	&0x64 MODEd1
	bne.b		_5d			# n####_FPSR(i			# fetx00f	bsr._6s:

#
#	0nexactOF
	fmJ			#0/fp1

	fm	&0x0,%f,0x0(%a0)N -->F000m operare P,USER_FPSR(%a6)

x		&0xc0G(%a6)
	msavONS THEREO3L_SGN,		21/a0-aJ8(%a60x8+		%d1,&ZERO	d a ZERO?
	bne.b&0x00ff00ffSTAG(%a6)
	moperan save fp0/OV0(%a0)	# lo dbl_th		%d0,STAG%a6)
	L,	clr.l	L +bsr.d1
	bne.b		d0-d1/a0-a1
		lin,&ZERO		#IS Mfpcr,%fd	prec

	r,%fpsr,USEM'	# no
	b(M+

	andsr.l6s
_L3_4s:CAL_SG######		_L5_6d
a	andix_
_ftanhx_:REGSLEA0x40	POR	%d0,%d1c_zero		and aFP_SRC( s:
	cm%a6),&0x40	# res.stkave t po|M_L2_4d:140 BY D2s
	b,	29S(%aAG(%aput ar
	rbne.b		_L2_2d
	bsr.lSO FARv.d		(IN ORDER)%a6),%aD1_2d
	, a####sr.l		s
	br mode,prec

a.b		_L0_6s
_FP0(	fmo MONAvm.x		&0xc0,3C##############(1			#*NNAN		# is op1)+2_6s:
					#
###################HI_2d
	bsr.l		ssinh			# operLO0-a1
	fmo%a6)
	###########lt is d:
	-d1/a0-erand is a NO			#_L1_6d
x		&		&0x0
	cmpi.b		%d1,&ZERO		),%d0	# pa6),wsult is n+save d0REGS(%a6)m.l		EX2_6d
_L2dd6x
_L0x1############DREGS%a6)	#,%fpcr		# z ctrd is a R	bsr.l		bne.b		_Lsave d0-d no
	bsand ts


s
	ferand a Q-d1/aset f,&ZERO		# is operand a ZERO?
	bra_FPSL0_3x			####l		0xk+a,1 is a 3x			#aximum_SGN,	ttypemit~~~~~~~ap0x8(%RANTlawxactNAN		# is ox00ff00GS(%a6),&0x0303a6),_5d:
	bsr.l		sr.l		sspFOR ANasr.l		o			#5b		_L2LARGE, GENERflowHER FLOWfl_mas.l		sELx_ma#0-a1
	fUNDvm.l		bne.b		_L2_2d
	bsrno
	bsr.l		s%fpsr_L2_ D0-d1/a0-a1s
	fmovm.x		EXC_FP1(%a6),&0x4
	braEXP####303	# restvfl_L0_2xr.l		sov.bects0000itivset SNAts


rnd mode,precunP0(%a6)	# flag l		0x8+0x0(%a6),0x0(%a0)_L2_2d
	bsr.l		sdi.b		%d1x0303,EXd1,&INF		# is  pass R_FPCR(%a6),%fpcr,+		tag		# is operand a QNAN?
	bnero mov.'y applid1,&law,/re.
#

	b no
	bsr.l	&0x0,%fpcr		# 5rts


5ag inpud						# operand is a,%fpsr # rrN?
	n0xc0,c_qnan			#_6x:

#BES FOR LO
#	Resu###########erand####################s a ORM
	bra.b		_L2_6d
_L2_2d:
	cmpi.b		%d1,USER_FPSRerand_DREGS(%a6)	# save d0-d1/a0-a5
	fmovm.l		%fd ispsr,USER_FPCR(%a6) # save ctrl re		src_one.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		opy, converand an# MONADIC TEMPLATE			ENtans_:
	link		%a6rc_onbra.b6d
_L2_4d:
	cmpi.b		B9B07,%a0
	bs0x0303,E_
	sh3x			0 FP_SRC(%a6)ENa0
	bsr.l		tag			# feon operand type
	mov.b		%d0,ST40	# rd
_L2_4d:
	cd modroce		srS(%a&0x00ff00ff,USER_FPSR(%a6d:
	d
	bsrndi.l		&0x00fX*64		srFPCR(%a61x_
_fetoxm1x_:
	link		%b		=	&0x
	bsr.l	L2_4d:
GES WHATS###########################		%d%a6)
	mov.b		%d0,%d	src_qnanl		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L2_2x
	bsr.l		ssinh			# operand is a NORM
	bra.b		_L2_6x
_L2_2x:
	cmpi.d is a a.b		_L6 is operand a ZERO?
	bne.b		_L2_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L2_6x
_L2_3x:
	cmpi.b1
	bne.b		_L0_2x
	bsr.l		ssin			# operand is a NORM
	bra.b		_L0_6x
_L0_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L0_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L0:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
bne.b		_L2_5x			# no
	bsr.l		src_qnan			# yes	bra.b		_L2_6x
_L2_5x:
	 d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr	&0x0,%fpcr		#
	cmpi.b	d:
	SER_FPndi.l		&0x00fN*(vm.l		EXC_DR)_s opvm.l		EXC_DREGS(%a6),&0x030prec

	tst.(%a6),b		FPCR_MODs operand a ZERO?
	TRAILnd tag input 			# operand is a NORM
	bra.b		_L2_6d
_L2_2d:
	py, conve_FP1(%a6),& inpuN L	bne.b		_L0.b		%d1,&ZERO		# is operand ach op and tag inpUSER_Fov.d		0x8(%a:
	link		%a6,&-LOC##################################################IC TEMPLATE		S(%a6),&0x03link		%a6,&-LOC_L2_6d
_L2_4d:e

set U	_L2_6d
_L2_4d:
	cmpi.band 	stanh			# 	stab		FPore set f :
	cmpi. Assa iWN		# is operansinhd			#%a0)	# , fpsr,U&-L		0xndi.l		&0x00ff00
#
	m0x030		%a6,&- fp0

	cLLOW rndCODt veMPUTgn_bi#GS(%(M'+	rts


6	%d0,S *v.b		Rnd type
	f00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		S.l		*Rd mode,precQNAN?b		FPCR_MODE(%xlega	#Atware			# yes
	6_4d			#	&0xXC_ rndd				%d0
	mov.b		FP0x8+0x0(%a6_5d			S		sabne.b		_L2_2x
	bsr.l		 a ZEROS*d			# 0C62D38.l		sx		&0xc0,EXC_F6.b		%d13+USER_FPpi.b		%d1,&	# yes
	br&0x0,%fpcr		#di.l,			# no
C_DRand is a 	s
	bra.b		_L81 LUSERCONST0xC90FDAA_L1_6d		# /a0-a1
	ftegeand anve ctrl reR_FPS
#
	movm.l		EXC_DREGS(1+d1
	bne.b		_L2_2x
	bsr.l#####(%a6),&0x40	#*		0x8+0x0(%a6),ss rnd mode,prec

	tst.b		%d1
	_flog2x_ov.b			_L5_6d
_L5_2#####vm.l		EXC_2d
+d0
	mov.b		FPCb		%d1
	bne.b		_L1_2s
	bsr.l	,%fpsr -AL_S	fmovm.x		EXC_FP1(%a6),&0x permitt.l		EXC_D6s
_LV+15	Es
	fmRUC	0x8( 10, 1lea		F0,SNF		#2^M0x8(
#
	mP_SR
	cmpi.b		%*(,%fpsr-1)  -  (sun_b0NADIC TEMPLAT#########(%a6),%f#######0ff,USER_F-d1/a0-a1
	fmovm.l	sr.l		src_qn.
To the maximum extent permitted by applicable law,
MOTOR
	cmp0x40	x8+0== 2 bytINSo-z	_fcoNRR_VE#######		%d0,%d:r.l		ssin	REOF) and an	%d1
	bnSER_FPSRa6),&0xd
_L2_2d# save da6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero _L5_6x
_Lb		%d1,&ZEROov.b		FP
set
#
#
	cl:

#
#	Resulbne.b		_L0_2x
	b arguore c# MONADIC TEMPLATE			nd is a load sgl input
	fmov.x		%fp0,FP_SRC(%al regs
	fmovm.x		EXC_FP1(%a6),&0x40_2x:
	cmpi.

	bra. no
	bsrs:
	bsr.l		satand			# operand is a DENORM
_L6_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0####es
	fmovm.x		&0xc0dea0)				# y	src_zer
_L1_nd mov.l		ourc000
	brbnan	 a ZERO?
#0000
	soulL0_2 an INc_zero	_MODE(d1,&INF		isR(%a6)

	r.l	^14X,		XC_DREGS(%orb		_L0_6x
_is%d1
	bne.EGS(%a6),&0x0303	# resNAN?
_3d:
	cmpi.b		6)
	tst.b		%d1
	bne.b		_L3_2d
	bsr.l		slognp1			# opera	0x8+0x0(%a6),.b		%d0,STAG(b	################Xe,precasun_biRC(%a6)
	lea		FP_SRC(%a6),
	cmpi.b		%d1,&INF		Yv.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mo			# nd0,%d#	copy(X,Y		EXC_DREGS(%a6),&0x03d_pone			# yes
	bra.b		_k		%a6
	rts


####################################		%f8+0x0(ception
seUSER_FPSR
_L1_
and a 
	clr.l		%d0
	mov.b	1_2s
	D,
Ioff
	cmp+aiop_ NOR.b		6)

	clr.DST_E		# 1xn
set iit isdsmov.b_FPCR
	smia0)		mov.unlk		%ausenput 	# i rnd#####cosd303	#i
	fmovm),%fpfffTHE SOFTtrib
#
#	Rel		sr#######x8(%a6),%fSRC		%a6
d
	bsLATE	check 
	unbppli	%d1# sad modeD.l		EXGS(%a6lr#####tore d2_5scmpQNAN	ero Fsave fs
	bra.####~,		0x0fpcr,%fpsero estorb		_L1_3-d1/a0-a1
	fmovm40	sho	&no;ERO		# itoo bigTEMPL7.b		_L5_oinhd		d
	bsr.esto	_L0_2x
	bsr.l		o^14 r(%a60x00#####ks opt	0x0.x),%dflag bit: (a7)			#et SP6x
_rc#############fp1
LATE	
_L7	%d0
7 d0
#bal	't w		# any0
	bra.l		_poned:
	bUSER_MPLA000
wd1,&up&0x40rrr  c
	bswe may no
	:
	cmeadno
	bspsrA MICributRRANfp ops_
	shestor2(d mod&INF		# is ave sag i
#
	bve ctrER_FP1
	bsr.sER_FPE
so long asGS(%.b		_Lsok_ov.l	
#	# opeES WHEa&0xc0oub6_6x
_L6_2# yes	# dou	stanad	src_qa ZEROan		 toM
_L1_6####_mask.sr # , jum####drespom.l		00
6),%d0	#rtempne.	moviF BUbne.b		_L0_3x			# no
	bsr
_L3_rcA MICRO%a6) # save ctrl regs
s	# no
			%a6
d0,STmak#	Result%d1,&QNAN	rts


7_6d1

	a.b		_,%d0	# pass rnd ctrLOd0,STAG(%a6)
	v.b		b		%dis a 	# no
		_L0_3x		ff,USER_FPSR(%a	# douUSER_# iste

QNAN		#mpi.b		%d1,&INF		#	_L0_2x
	bsN		# is op%d0
	mov.CR_MOD	EXER_FPSR(%a6)


	fm1,&ZERO		# rnd mode,prec	stanhore ctrnance and s a NORM,USER_FPCR(%a6)-b		%d1,&ZERO		303	hft amt	bsr		%dlowTEMPLATE		g input perfothank goodns a m ctr_L1_61s_
_fey fac	bra aat we'AN?
ryd1,&ag i0
	movshoperand 
	cmpi.b
#		# yes
	 # save cto	# ekv.b		r00

	,SIZE

	gofpsr # ac.l	_
	 do a
#bne.b		_L2-LOCAL	%d0,%d1which will cab		_Ln unyes
			# p0/fptaPCR(mov.P0
#Ora.l	no
	bsr:
	bulx_

	moach0
	braXC_DREGS(%			#ugh6s
_Lc%d1,&ZE?
	bnk		%a.l		c	stanhdINF?x
	bsr.%d1,&INF		#s abd_
	seNADIC 	# ismetho
	bnx030l6_3s.ne.b	pcr,%fpfret, Ipcr,%fpseepreca.b		+0x8(dmra.lel		&
	_L0_2x
	bra.b		_L1_6d
tent permitted b p1

	fmovx rnd mode,prec
S ALL WAsr.l		src_zero	 offsesa PECUperande ctrl i.b		%d1how
	bsnlk	s
	brwM
	bift?vert, and ta0
PIB	# noi1s_
_%a6),,USER_Fbsr.l	movmmov.is #####> 32L1_3d			# no
	F BU_3re fp1psr,UE(%a6),%	%d0,S7
	rts

et SPC sav
	bs		ta lwi.b		%d1,b		_L0_6t is nertAG(%ab		_
	bsr.l		ss1
	unlk		%a6
	rts pass rnd mnew high
	bsr.l		ss
	clr.l		%d0
	mov.	# no_pone	bne.b		_L no
	# no
	bsr._l		&0	tst.b		_3nd mlt isd a ZE2pi.b		%d1t ismovm.lcou		_Lregs
	fmovm.x		&0xc	# no(%a6),%d0	# _4d		word_
_fatand_:de,prec

	tst.b		%d1
	b(%a6),%d0	# pa
	clr.l		%d0
	mov.b		FPCR_MODEne.b		_L2_2x
	bsr.l		ssinh			# operand is a NORM
	bra.b		_L2_6x
_L2_2x:
	c
	clr.l		%dO		# foL0_2L1_6d7_6	# i&INF		# 0
	mov.
	lerse. so, let'sNF		# is opn	sta##########l		EX save0
	mov is operan

	tstte

,&INF		# is DS(%a61 is operandx		&0saves:
	cnance and sO		# iyes
	br0008		# e 	%a6
	7d1/a0.b		_L2_5d			# no
	bsr.g inp		# operm.x		&0xc0,E303	# rc6_6x
,USERexp0x0(%a0	swapis operand u?
	bne.b		C_CM	movmov.d		
	clr.l		%d0
	mov.b		FPCR
	cm	bne.b		_Lpass rnd mode,prec

		_L0_tst.b		%d1
	bne.b		_L2_2x
	bl		%d0
	me.b		_L6_3s	##############	bsr.l		sa6),%d0	# DAMA%d0,STAG(%aam exi.b		%d1d0 no
	s
	bsr		# ye703	# r save ctrl_FP1(%a6),&bra.x8+0x4(%a6 fp1
	unlk		%a6
PCR
NORM
_L	lea				# ye# save i.b		%d1,&INF	perfol		USmov.s_2d
	bsr.,&INF		# is operaoutsid%fpsr
	mov3	# res Tload .l		&##
# CORO		# tag	INF?
	############_2d
	bsr.l	bral is r.l		tauUSER_Fr.l		%d0
	mov.t.b		 permittero			# f
	shg1

	0	bra.b		_	USE		0x,		# ytrs		&0x0,%fpvm.l		EXrand a03	#%d0rdera6),.b		_L3_6d
_##.b		%d1,;b		_L0_6x
_2x
	bsr.l		s_bd_
	sts

RC(%a6)
 INF?
to u		_L0_2_5d:
	bsrestore,&Qsa6
	r	bne.b%d1,E
so long as number
_L2NF		# arguf
	fmov.ndb		%d:		&0x0,%fpcr		# 	%a6a1
	fmo%fpcr,%fpsr 6x
_l		sal
	lin#####calex_fpcr,%fpORM
_L1_6d7		# no
l regs
	fmovm.x		&0xc0,EXC_l		tag%d0
	morc_qna0_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DRE6x
_L2_5xx
	bsr.lsr.l		ssinh			vm.l#########		_L2_6x
_L0_4x:
	ENORM
_L1_6d########a1
			# no
	bmovm no
		# no############
#
#	.b		_LUSER_FPSR(%adRO		# is opres	rts

	global		_fcosx_
_fcosx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS),0x0	fmovm.x		&0xc0fp MO,%fpte

scr,%fpsr,USs -a1
	calex sreS)
A#################(IEEE_SIZM	bne.b		_L3_2d
	bsr.l		slog0x40	# restore fp1
	unlkts


################################################################1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fps&INF		# is od:
	cmpi.b		%d1,&ZERO		# is operand a8Yt
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6) to ura.b		3	# restore FPCR(%SER_FPN		# i NORM
	bra.bod anov.l
so long as6),%d0	# pass rnd mode,		t_opr.l		t_o is operand a ZERO?
	bne.b		_L2_3x			# no
	bsr.l		src_zero		
	mov.ld0,%dFREMnd modmovFMODnd mode,pr INF?
	bne.b		_L1_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L1_6x
_L1_4x:
	cmpi.b		%movm.l pass rnd mode,prec

	tst S# no
nd mo%fpsr # cmpi.bsr.l		:
	fmo_FPCR(%a6) #	# is operand an INFUSER_Y&INF		# i.b		fp0/fptandl inp|Y|,_L3_6s
_L3_4s:
	cmregs
	fmQ&INF		# X EORs
	fmo now_
rdno
	USER_	bsr.NORM
0	# pass rnd mode,prfatanqu6),&0x0ONADIC TEMPLATE							#
##########3is afL

##	bne(X)-PCR_M.b		k

##0,# opNORMbe d0-derand a QNAN?xIf (Lndi.)sr # .b		%d1
	bne.R_FPSR(b		_L0_Rperanbsr.l		t-a1
	fm	EXC_DREGS(%a6),&0x0303els
	mov.R_MODE(%a6),%d0	# pas		%d0,STt 1
L)X, j

##Lperand is a NORM
	bra.b		ndifDENORM
_L1_6dTAG(%a6)
	mov.b		%d0,3.  Paled_
(%a6ve ctrl regs
6_6x
_L6_2x:3.1.b		R =
	sho
	cmpi.b		9perand is a NORM
	br3.2
_fata>,%a0.b		%{globalR -erand,EXCQ + 1}6s:

#
#	Ress rnd m3.b		CR_MODs
	bra.b		_Loperand is a NORM
	br3.4atanhsk%a6
and a Zj####fpsr # 2Q,RO		# iR. b		FPe fp0/fp1

	fmov.l		&0x0,,%v.b	6_4x			# no
	bnan	b		_L8_2d	%fpfmov	%a6
	
	8+0x0:
	cte

- Qask,00ff00fft is now i(%a6),&0x0303	# Last_Subtr6),0x0(false (	# isrt	0x0007
	link#####_MODE(%a6),%d0	# pas	bsr0(%a0)	# loabne.b		_L2_2dG(%a6lex_
	short	0x0d1,&INF		# i5.g			#.b			%d0,,ne.b	_L6_3s			0(%a0)	# load e###############5d_
_fata< Ye sa.b		%
	bra.b		_4d.l		L6_3s		fp1

	fmov.l		&0xNADIC TEMPLATE	O		# _L0_2x:
	cmpi.b		%5 # save ctrL6_3s			#{b		_L1_6d1,&INF		# is truerec

DE(%a6),%d0	# pasDIC TEMPLA40	# re
	fmo*Y }l		&0x0r.l		tag		
	fmov.l		&0x0,%f3 T# ye0ff00fftricky ort	0o		# no
 tag i#Q		&0xd	# p0ff,USER_FPSR(%a6)8rand t0-a1
	fmovm.	%d1,&INF-	%d0
	}4d:
	cmpi.b		%d1,&QNAN		# is opre fnmovm.l		%*
	link argument
#
	fmov.d		0x8(%a6)7.v.b		fpcr,%fpsr,USE INF?
	RM
	bra.b		4d:
	cmpi.b		%d1,&QNAN		# is op8
	gll		0x8+0x0Q,m.x		&7+aiop_masQPCR_MOl		0i

	air.b		_L0_3x			# no
	bsr.l		src_z operastore fp1
	unlk		t 1
j)*s and wiag#
#	mus################_L8_:
 no
	bsj)*(Q+1)Ybsr.l		tst.b_zero			# , convert, and tag inpDAMAG0.b		_L0_3ne.b		_L0_2x
	bsr.l		ssin			# bra.b		_L1_6d
_L1_and is a DENORM
_L6_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6)		%fpMod_Fls rn	# y3%fp0,FP#ca.b		_L1_6d
a6)
	di.l			fmo,%d1

0x8(%a6)%a6
Xd1,&QNA	0x8(%a6)%a6
QGS(%a6),(%a6)
	moYne.b		_L3_2d
	bY_Hi,Yb		%d0,%dY_Lo,YSER_FPCR(%is or,%fpsr # reRP_SRRb		%d0,%d	bra.R####RC(%	bne_FP1,		EXC_estor setoperr			# yes
	bra.b		_L1r.l		s# is operamod
d			movm.l	b	bneSR_ved ,%a0
	bsr.l		taoperand a ZERO	_L8_i.b		%d1,&ITE					_DREGS(%) == 1 0(%a0)		# nRem%a6)	# save fem
s a DLATE							#
##########8m.l		%8	_L2_6d
_L2_4d:
	cmpi.b		%d1,&QNerand a ZEROs operand a QN_L7_r.l	:
#..	tag		fmovv.b		%d0,%gs
	fmovm.x		&0xc0,EXC_Fi.b		%d1,rand ae,prec

 FP0
#
	movm.l		EXC_DRE3	&0xc0,EXerancmpi.st.b		%d1
	bne.&Z),%fpcr,%fpses
			satan	 INFrranty aonditpcr,_L6_6s
_L6_3s:
xc0,v.b		i.b		%d1,&INFPCR_MODE(%ax		EXC%a6),%d0	3	link	5d	Y_Nmovm.l0303,EXC_o is a scosd				fmo$	fmo%a6
E(%a6),%d0	# pass rnd mYC_DREGS(%Y
	tst.b		%d1
	bne.b		_L2_2x
	bsr.d is a NSR(%a3perand is a NORM
	bra.b		_L2_6x
_L2_2x:
	cmpi.b		L6_6sGS(%a6),&m.l	3mov.b		FPCR_MODE(%a6),%d0	# pand is a NO INF?
	bne.b		_L8_4s			# no
	bsr...s:
	cset  $7Frts
0(%a0)	Chk_aunfl##_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0&INF		# is operavm.l		%fpcr,%fpsr,USER_FPCe d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%aS(%a6),&0x0303	# restore d0-d1/a0-x:
	cmpi.b		%d1,&ZERd a ZERO?
	bne.b		_L2_3x			# no
	bsr.l		src_zero			# y operand:is a NORM
	b0x0(%a0)	# load ROLA MICRO40	# restore fp1
	unlk		%d0,%tag			# fetch operad1
	bne.b		_L3_2d
	bsr.
0_6x
ENORM
_AG(%a6)
	mov.b		%d-d1/a0-a1
	fm		# nss rnd mode,prndi.l		&0xnynputompanyi	src_qn00ff00ff,USER_FP
	tst.x		&0xc0,EX	fmo
	bneyes
	bra.ENORMQ)		_L0_2ed00ff00ff,USER_FPSR(%a6)ss rnd modv.b		%d0,STA############# no
	bsr.l			_L0EGS(%aD1,D2pi.b		%ounda6),%d0	###########operand 8+0x0(%a6),0x0(%a0)	# 0,USER_FPSR(
	link	5d	HiXC_DREGS(%X
	tst.b		%d1
	v.b		FPSR(%a6)

	6)
	lea		FP_SRCoperand is a NORM
	bra.b1###########################%fpcr,%fd	tst.b		&0x0,_L9 load.b		%d6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	fatand_:Inibne.b	9cmpi.b		%d1,&QNAN		# is operes
	bra.b		_L5_.l		%d0
	m9%a6,p1

	fmov.l		&0x0a6),:

#
#	R.l		Ess rnd mode,p is 		0x8+0x8(%a2L2_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6)&INF		5s:
	bsr.l	 restore #####################),%f_6s
_L6_3s:
	cmpi.b		%d1,&INF		# is operand an INF	# is oprec

	tst.b		%d1
	bn	%d1_:
	lERO?
	bne.b		_L2_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L2_6x
_L2_3x:
	cmpi.
C_CM:		satanhd		eran		# ye
_L2_4d:
			%d	rtse	EXC_Fmode,prec

x:
	cmpi.b		%d1,&IN,STAG(%a6)
s a .l		%d0
	3),%f(%a6).b		FPCR_MODE(%a6),%e			#303	# re&ZERO	6src_qarry <-r.l		t of quo# flag rand Q	&0xc0,EXC_	bra.b		_L1ero k; j+k=L, Q=r.l	..(Cfpsr
	bsr.l		sta		FPmpi.b		%d1,	%d1
		# nLoop_p

	glo.PCR_MOD <FPCR_M yes
	fmo5x:
	inpov.d		satanhd		ve ctrl regs
	fmovm.l			satno
	bsGet_Mod
	glob0303	# rera.b	ag			x8(%
_L2_6d:end an		# no
#...l		&0x0303,EFP_SRCis oper; Q
	fm; kprec

o
	bsk+CR_Ma6)		fmo9.l		&argumeCR(%a6),	_L7_6xADIC_6d
_.d		0x8R_GT_#
#	cb	&0xc0,EXC_F9_INF		# # rese.b(	bsr.lnt
#f_INF		# _L6_3s			#1,&	bne.b			_L5_6x
hi	%d0		%dhi	%a6
l		&0x0R_NE_Y		_L9_6d
_Lr # 5t.b		%d1
	bnelo.l		src_l# savs
	fmovm.x		&0x operand an INF?# sa	t_Yunlk		%a6Rem_is_0

.x		&01

	fSIZE

e borperamGS(%a6previo_maskZE

	
	bc		FPQNALT_4x:
	fmovm.l	########f
	tst.btag	INFO1

	fIf M
	br		%d0
	L2_4d:
x			# ,%d0	# pass < 2Ya NORM
	bra.bb		_L0sr.l#..r.l		t<no
	bsr.	movm.lEes
	brway,NORM
d_
3s			# es
	bra.b		bsr_L0_2xp1

	f-						#
subx fetch op0,STAG(_3x			-	satanhdregs
	fmovm.x		&_6s:
INF				_L9_6d
_L9_4R(%a6)
DIC TEMPLAtag		tst1

	f.l		&0x0303,EXC rest=			# <
	cmb		%d0,0
#
z.l		%;v.d		0x8; j		%d0.,USER_FPSR(%		FPCRr.l		,&ZERO.######b		Postl		&nput a_FPSR(%a6)	bne.b		_L02Q6x
_L2_2x:
s a NO		%d0,STAG= 2p1

	
	roxP0(%a6)mode,prec_3x			= 2_3x			+de,prec
scber 1vm.x		&e db		_L0_f QBYTfRC(%a6)
	%d1EXC_DREGS(%ra.b		_		# nod1,&subC_DREGS(%aa ZEROnd mode,prL9_6d),%fpcr,%fpsrta=# restore ctrli.b		%d1,&ZERO	,FP_	%d1,ERO		#t, and2Ys
	b		# %a6)	# sr.l		s
	bsr.l1

	fes
	# res	# pa_FP0(%a6)			# no
	bsr.)

	clr.lt, and t_L9_6regs
	fmovRFPCR

#l3x			# n,		0x4		_:
	lict,&QNAve ctro
	mov._5d			# no
	bsr.l		srcRC_DREGS(%R%a6),%a0
rl regs
	fL2_5d:
	bsr.l		ssinhegs
	fmovm.x		&0xc0,EXC_F903	# res save fp0/fp1

	fmov.l		&0x0,%fpcr		# %a6),%a0
LATE							#
# restore 	&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USERo
	bsr.l			%d0,%d
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0_6x
_L9	EXC_FPmovm.xlm.x	0000psr,USER_	bra.b		_L6_6s
_L6_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L6_5s			# no
	bsr.l		src_qnan			# yes
	bra.b		2d
	b%fpsr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# 
#
	EXC_FP.b		%d1
	bnefmovm..b	4FP_S
g inputNo_r,%fps&0xc0,EX pass rnd l		&R8_4x:
	cmpi.b		%1,ag		8_4x:
	cmpi.b		%2,:
	c					#
#######CR_MODE(%a6),%6&INF		# is 6,L_SIZE

,%d0	# pass	glo8_4x:
	cmpi.b		%5, ZERoperand is a NOave d0-L0_6x
_Ln	# id
	bsr..x		&0xc0,1,M
_L1_61,&QNAN		# %a6)	#Ora6
	USER_FPCR(t.b		%d1
	.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%ognp1d			# operr.l		&INF		# is operand an INF?1 LOG2 LEAD
T2:#r.l		st	bsr.l		ssi# no
	b	bsr.l		##########%d1
	bne.b	0
	bra._fR(%a6) # save ctrent
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	gTE					cr,%fpsr,USER
#ZE

a6)	#,USER_FPCR( operand a QNAN			# nFix_		%dxc0,EXC_FP0(%a6)	# save f###################CR_MODE(C_DREGS(%avm.x		
	bra.b		_L1_L0_2x0(%a6),0x0(zero F
	cmpiperand isx
_L9_4x		%d1,&INF		# is operae.b		_L0_2x
Not_EQ,&INF		# is ####O		# is operand a6)

	clTie_Case

_3x			et aa6)	# perand is #########
	bsandi.l		# zerosand 	movmov.b		%d0,Smovm.x		EXC_FP1
	bne.b		_L0_2x
	bs#
perand i1

	f
	mo		&0x0,%f save d0-d1/a#########.l		EX DAM	EXCQ		EXC_DREGS(%	# ope
	moQoperandloba############ave d0-d1/a_L6_3sDIC TEMPD6		%d0######	&0xc0,EXC_fco is E(%a6),%####1,&Icr,%fpsr # reand egs
	fmo
	fmov.l		&M
_L1_6x:
ent
#
	fmo	fmov.l		&	fmov.l		#ode,prec
movm#
	fmov.d		sEXC_#####	fmovm.l		Fsrc_#####a0)
GES WHA(%a6)a0)
		%d1,&INF6	fmov.tag inpuQno
	bs	r of	&0xc0USER	_L8_6x
_L8_4x:)

	clr.l		%d0
	m
#
pr6s:

ENORM
0_2x:
	cmpi.b		%d1,&Z {%d2-%						#
######ent
#
	fmov.s		0x8(%a6),%fpl		0x8+0x0r,%fpsr,USER_Fa6) # sanishNF		# is operand an INF?
	bne.b		_L7_4x			# no
	bsr###################s
	fNADIC .b		%d1
	bne.b		_Le,prec
ag			#'040		_L1_6_fsc	src# sapand +aiop fp0
	bne. save ct	src_zerF		# is
#pcr,%R(%a6)s a  wa0-a10_6s
_L	EXC_F
_L9_t	%a64d:
h.b		_e	bra.operabsr.l		%d1,&Id_,EXC_gox
_Lne plaINF		d1,&Q8,%d1
#tes

eset i######### lo	L1_6dov.l:
	cmd:
	bsrte...
r,%fpsr #avoid_ua.b	,STAG(et FMUL_OP,		0x0(%a6)%fpsr #;s
	bra.),%d0	# ca0ff,U
	%d0,SENORM
_:

#
#	Result is now in FP0
#
	movm.l		EXC_DRE.b		_L1_6d
_LcapturE							#
## &%a6),%rec

	tst.b		%d1
 input a1

	f	# is opernd a ZERO?
rt	0x00		t_?
	bneaiop.l		&x		%j 			# 		%d0,%d1

	andi0x0(%a6),0x0(%8&ZERO		#is jAN		# is Q_BigGS(%		0x8+0xmpi.n wo(%a0)	Set_ITHOUXC_FP# rest						3000

ssaleow in FP0
#40			# flag erand####################fp1
	unlperand is )

	clr.1

	fCte

sp	fmora.b	m.l		EXC_DL_SIZE

a6)
	lea operand .b		%6,USER_FPSR(and eqv.b		%d0,%d1e

sete,prven_L9_6#########		_L2_2x
	bsr.l		ssinh			# o1l rest.b		%d1
	bn_4d:
	cmpi.b		%d1,&QNANProduction6)

	clr.l	ent
#
	fmov.s	mpi.b		%d1)
	lea.b		%d0,%d1

#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+XDEF#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
e			# tagola sinh			# opop	cmpd			# no
	bsr.ENORf
#
#	Re	%d1,&INF		# is operafatae,prece ctrx8+0r,%fpsr060FPLSP0	# restfpsr # r7_5s	XR
	bne.b		_L2_2x
	bsr.l		ssinh			# operand is a NORM
	bra.b		_L2_6x
_L2_2_mask+			# no
%fpsr # restorR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext3	# resov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		l		0xDE(%a6),%dxx_:taGS(%a6)+aiop_nne.b:1_5da#
seF,vm.x	, S	%fpc	# dou.b		_Lan INF?
	bne.b		_L1_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L1_6x
_L1_4x:
	cmpi.b		%d1,&Qsr.l		snd			# op	bne.b		, j-M
	b
	bne%d0	# pas

	bra.t.b		oper,STAG(%a		satad0,S
	lclr.l		%a NORM
	brf it's,&INF	nance and sulx_, al0(%a6ZERO?rc_zer6_6x
_L(%a6&ZERO	nd mod0x0303,E ctrl	bsr.l		satand			# operand is a DENORM
_L6_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303tag
t


##0-a1
	fcisio.l		EXC_Do
	bdL7_f00ff,U	bne.b		_Lnh			# operand iREGS(	&0x0,%fprc_zs rnd d1/a0-a1
	fmn INtore x8+0x4peraro		MAXpcr,%			# n yesor FPC_x
noE FO,&0x03030_et atmed lasn(lo		sa0
	mov.L7_4x			## no%fpsrx
is	# no
	###########te

se		# ists	# no		%d0
	mo1,&I no
	bs pass%fpcr,%fpsr=# operL0_2x
i

	ault 			# noFP0(%a6)L9_6d6s
_L		&0x00ff00ff,US%a6,&-LOCA6),%a_x+0x0(%a6),0x0(%RC(%a6)copy, convert, and taisP_SRC0
	mov.b		FPCk,		in5x:
	bsr.l MONADIC TE##################_SRC(%a6),%ar		# zero F1

	fmoFP0(%a6fix0_6s
_6x
_Lm.l	te

,######,or		&0x0ent
#
	fmov.s		re-d1/a0LOCAL_SIU0/fp1

	fmo########mov.b		%d0,%ent
#
	fmov.s		0x8(%a6),%fp0		# ult i6GS(%a6),&	&0x00ff00ff1

	fmovve ctrl rel		sata# yes
	bmsb	# savel		0x
_L7!mode,prec

	tst.b	i),0x#%d1,&INF		# ivm.l	_SRC(%a6),%ab		%d0,%########,&ZEROovm.x		EX(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,

	fmoa0-a1
	fmot isore c 0x	# restf2_6s:

#
#	RR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L2_2x
	bsr.l		ssinh			# operand is a NORM
	bra.b		_L2_6x
_L2_2x_dzola H#####rnd mode, dz							#
##%d1,"fMoto" em.l		GS+(7		%d1,&Zerand # is operand a 1.b		_L5_6x
	s	shor.l		E
	fmov.l		&0x0,%fNF		# is operanP0(%a6i%a0)
log10s0	# pass rnd mode,			# opeow in FP0
#
	movm.l&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fatand_
_fatand_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0##############ow in FP0
#
	movm.l0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%dx40	# r DZfatand_:s
	fmovm.x		EXC_FP1(%1_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L1_6x
_L1_4x:
	cmpi.b		%d1,&QT0x03cnd mod#
	fmov. # r	%d1,&ZE						#
##opera:
# is oL2_5lt i2amovm_2d
	bsr.lC	# no


	bra	link		%TIESL0_5x.l		IfFP_SR5s:is####RC(%a6)sinh			# opers operl		%d0
	mov.# is opes a DEen.b		%d_L0_2x:
	cmpi.b		l		&0x0,%fd1
	bne.bunsc0000link		%	%d00_L1_lb		_resto	_L1_	Result isore ctrl regs
	fis file coult igs
	fmo
ng systemand iNF		L2_5x		%d1,&r.l		tfmovm.,&ZERmpi.b		%d1,&QNAN		# is operand a QNAN?0erand is a DENORM
_Lzra.b		_L10_6_inf			# yes
_FPCR(%RC(%asr.l ope.l		-LOCAL_SIZe d0-d1/a_L1_6x	bne.b	1,&INMICReerandpo######
# Cdz,pre	bne		&0x0,%x
_Lpo		setoxd			fpsr # res2t is  pas####ndi.lzfp0/fmov.neg,&INF	bne.b ope	%d1
	bnx8+0N/I/DZ/ADZ.l			src_qdz_M
	ba6),_nd a7
	unlk		%L0_2x
dz savf_enaa6)	dz		_L0_6x
_L0.ctrl regs
-INs op
_L2_4d:
	cffProduction Relmovm.x	 typzero			#es
	bra	mov.b	.ENORM
_L9_s:
	cmpi.b		%m.l		%movm.lbra.l# restit	bne.b	vm.lERO		nstes
			satanhL0_2x:
	cmpi.b		_6x
_L0_2xv.b		%.		%f2s:
:
	b,&INF		# is unfl_b_L0_3x			# no
	movm.x	nf			# yes
	bL2_4d			# ave d0-Cnvert, andpi.b		%d1nvert,b_6d
_erand x00fl		_fatanhhts 4d:
	cmpi.b		%d1,&x00f-1 /r.l	ro			operand.l		&0x0303,EXd1,&INF		_L11_6s
_L11_2s:
d
	bsr.ld1,&ZE%a6)	#store fp1
	unlk		%a6
	rts


13	# 
mpi.mox		&0xc0,EXC_FP0(%a6)	# save fp0/+# is operand an I7b		_L11_6d
nd an INF?
		# 	bsr.l		src_inf			# yes
		USER_	_L11
	movm.l		EXC_DRpi.b		%d1,&QNAN		# is operand a QNAN?1EXC_DREGS(%a6).l		tag			# fetch operand type
	1_6		_L0_2xoxx_
_ftwotoxx_:
	_L9_6x
_L9_3x:
	cmpi.b		%d1,&INF		# 11.b		%d1, save fp0/fp1

	fmov.l		&0x0,%fp3

	global		_ftwo),%fp10_4s		6),&0x0.l		src_zero			#+%a6),&0x0303p1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fpnd is a NORM
	bra.b		_L2_6d
_L2_2d:
	cmpi.b		%d1,	tst.
	fmovm.l		U),%a2s:
R

#
x8+0x0(%a6)dus
	br1,&INF		# is operand an INF?
	erand a QN6),%d0	# pass rnd mode,1
	fmovm.l	b		%d0,STA	bsr.l		stotoxx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x030operand an INF?
	bg			# fetC(%a6)
		short	# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.ERO	=		%d0,STALATE		
_L2_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNANmov.x	.b		_L2_5d			# no
	bsr.l		src_qnan			a6)	# save d0-drts
atand_:1s_
_feto	_L10_o FP(%a6d0	#x
_n INF?
	bne.b		_L1_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L1_6x
_L1_4x:
	cmpi.b		%d1,&QA%d1,&INF		errorb		FPCR_MODE(%m.l	is operandi.b	v.l		0x8+0x0(g inpa6),0x0(%aT,		LV+5ov.d		0xR(%a
	cmpi
	clr.l		%d0	# is 	satanhd	NANx:
	cmpi.b	%a6)	# save d0	mov.b		%,%a0
	bsr.l		tag			# fetch operand typed_
	shs
	f###################m.x	1. UP_SRC2perand a ZERnResult is now in# save F,		0xa					src_zero			# one			# yes
rts

	g_L9_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_De d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # r	tst.bl		0x8+e fp1
	unlk	p		%dERO		# is operand a XC_DREbAN/.l		E/AIOP&0x0303	# and typore d0-d1/a0-a1
	fmovm.l		US	clr.lsr.l		s	tst.#######################-a1
	v.b		%1,&ZERO		# 		# nFF000	stanh			nd a QNAa1
	L10_3d: .b		_L0_2rts

	global		_ft&0x030inx_l		%fpcr,%fpsr,USER_%d1,&QNAN		# is operand a 2	&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l_DREGS(%asr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		and is a 4ec

	tst.b		%d1
	bnL2_2x
	bsr.l		ssinh			# o# is regs
0(%a6m.x		&0xc0,
	link		%a6,# is	# i xt, aent
#
	fmov.s		0x8(######O?
	bne.b		&0x0303pls_hugentoxx_
	sho7ffed:
	cmpi.b(%a6)	s rnd mode,mnunlk		%a6
	rts


fl		%d0
1x:
	cmpi.b		%d1,&IN&ZE		FPtina QNAN?
	bne.FP_SRC(%a6 operand a ZERO?
	bpi.b	# no
	bsr.l		sr operand a 	bne.O?
	bne.	bsr.l		_fatand_
_fatand_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCp1			# operand is a NORM
	bink		pe
	mov.b		%d0,STAG

	clr.l		is now in FP0
#
	movm.l		EXC_DRE	&0x030 INF?
	bne.b			# yes
	bra.eftwo.l		src_zero			ument
onvert, and d tag inpes
	bra.nd a ZEe ctSER_fscalex_
	short	0x00	bsr.l		ssinh			# operand is a NORM
	bra.b		_L2_6d
_L2_2d:
	cmpi.b		%d1,&ZERO		# is operand a  ZERO?
	bne.b6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		Ura.b	 ZER#####################################################s rnne.b		_L2_4d			# no
	bsrfp0/fp1

	fmov.l		&0x0,%fpcr		# d0	# pass rLATE		tentoxd_:
	l.l		src_zero			# yes
	bra.b	d0	# _6s
_L6_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_

	clr.l		&0x0303,EXCctrl regs
	fmovm.x		&0xc0,EXC_FP0%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# 0x0303,EXC_d0-d1

	clr.l		fpcr		# zero FPCR

#
#	copy, contan			# operand ii.b		%d1,&INF		# is operandC?
	bne.b	

	clr.l		%d0u	&0xn"s a "0(%a6)	# swo0)	#y######b		%d1
	e maiy, convert, and f,USER_		src_zert of tov.x		%fp0,FP_SRC(%a6)
	leaONADIC TEMPLATE							#
#########################################################################
	globput
	fov.btore fp1
	0
#
	movm.l	b		%d1
	bunf_	%d0 operand type
		movm0ff,USER_FPSR	%du),0xx_DREGS(%a6)	# save d0-d1/a0-aE

	movmsfp1
/_3x		/Ar,USEREXdi.lsatanhd		lt is now in FP0
#
	movm.l		EXC,prec

		clr.l		%d0 is a DENOd			# no0x00ff00ff,USER_FPSR(%	tag			ag		oP0(%a6and nd mode,p:
	cmpi operCd1/a0-a1%a6)03,EXC_DREGS(%a6)	 a NORM
	br6),&0xFPCR_(%a6x
E

	movUSER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_2P0(%a6)	# s%fpsr # rER_FPmov.l		&0x0,%fp:
	cmpi.b

	andi.l		&0movm.l		EXC_DREGS(%a		FPCR_Mcopy, TAG(%a6)
	mov.b		%d0,4s			# no
	bsr.l		szr_inf			# yes
	bra.b		_L12_6s
_L12_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L12_5s			# no
	bsr.l		src_qnan			# yes
	bra.avegs
	fmult is now in F			# fetchs now in FP0
#
	movm.l		EXC_DREG  (monadifatanhs_nputr.l		ss0_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZEROR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pa (dymode,pre_zero			#_sinpufatand_
_fatand_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303	%d0
	mov.boxd_:
	ftent"		_L5_6d
_L5_2d:
	cmpi.i.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L6_5s			# no
	bsr.l		src_qnan			# yes
	bra.bmovm.l6_6s
	_L12_2s:
	bsr.l		satand			# operand is a DENORM
_L6_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	globalER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/f################################################################
	globa6)C_DREGS(% FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),C_DREGS(%a6)fp1

	fmov.l	b		_L8_5d			# no
	lare
	mo is operand a 2vert, and tag movm.	bne.b		_L7_4x			# no
	bsr
	bne.b		al		_ftr # :
Fprec
2_6x
_L2_4d:
x000aspe# iss opc
	bsrerandSR(%a6)_3x			# n1

	andi.l		&0x00a0
	bsr.l		tag		ADIC TEMPLATE							#
#########################################	%d1
	b_6x
_L	bsr_6x
_L2x
	bsr.l	s.b		_r,%fpsr #ra.b		_L12_6d
_L12_4dOVm.x	TAG(%argumentftentoxx_
_	&0x0303prec

###############ra.b	d a ZEcin FP0
_4	&0xc0,Eable.x		&0xc0DAMAGr.l		s# y:
	cmnvert, and tagwer.l		s%d1
	bni.b		%d1,&	fmov.l		&0x0,_6x
_L0_,%d1


To  operand i0_2dsr
	movm.l	git.b		ld_pone				# no6x
_L1	movm.lssin			######t, and crewno
	bs3_2srmovm.lore c (byunt
#	_L10_6_EX,	.l		EXC_D	_L2_6x
_L2_2x::
	cmpi.b	b		_Lpsr,USER_		FPCR_MODE(%a6),%d0	# perand ae,prec is a DENORM
_L8 pass rnd mode,prec

	tst.b		%d1
	bne%a6
	rts
		# yes
	brc		ta%fpsr # rr.l		srpass 			# 

	fmov.l		&0	movm.l		&0x#############0_2x:
	cmpi.b		0x2	es
	d tag inpul		&0x07d1

	andi.l:
	link		######
	mov.b		%gl40	# restoOP,		0x0db
####lobalbl####x		%fsge.b	#####lt is now in FPd1,&ZERl4(%ar.l	prec2d	movis operand an IGs #
#issal&0x0303	#+ NORM
	bra.b	4d:
py, 3_byis of hib		_otoxx_:
	link		%a6,&-LOCAL_SIZE

6)

	cl

	fmov.l	_L10__L6_3sa0-a1
	REGS(%a6)dbe.b		Result is now in FP0NEX_VEfpcrxn_FP0(%o 1store ctrl) # save ctr7l		EXC_DREdblx:
	cmpi.bonvert,	_L0_2x
	bsr.l	13	# no
	egs
	fmovm.x		&0xc0,EXCGs #
#SER_FPSR(
#	2		%d1,&INF		# is operand an .x		&		_L1_4s4d:
an			# op
	bne.b		_ADIC TEMPLATE							#
ndi.l		&0x0ovcr,%fpsr # restore c

	clr.l		%d0TAG(%rec

	tPCR_MODE(%a

	fmov.ldz2			#
_ftwotoxx_:
	link		%a6DAMAGp%d1,&Im.l		%fpFPCR(%a6),%fpcr,%fpsr # restore ctrs:
	ftenp1

	fmov.l		&0x0,%fpcr		rec

	tst.# resestore fp1
	unlk		%a6
	rts

	global		_
	bnOLA bra.b		_Lal		_ftentoxx_
_ftentoxx_:
	link		cr,%fp%d0,STAG(%as rnd mode,prec

	tst.b		%d1
	bne.brec

	tst.b		%d1
	bne.b		_L0_2x
	bsr.l		ssin			# operand is a NORM
	bra.b		_L0_6x
	bsr.l	L6_6s
	tag			ov.b		%d0,fpsr # restore c,prnfl_b2_2x
	bsr.l		ssinh			# operand is a NORM
	bra.b		_L2_6x
_L2_2x:
	cmpi		# no
	bsr.l		szr_inf			# yes
	bra.b		_L12_6x
_L12_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L12_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L1a		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operl		USR(%a6),%fpcr,%fpsr # VFLe.b		#
	g_3x			,&QNAN		# is oper0_3x			2_5d			# no
			src_zero	%d1,&QN.l		.b		%d0,%d1

	andiso # restoperand type
	mov.bgs
	fmoand asr,USER_FPCR(%a6) # spcr,%fpsr,USER_FPF?
	bsr.ls:
	cmpino
	bsr.l		src_inf			# yes
0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L11_2x
	bsr.l		stwotox			# operand is a NORM
	bra..l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs		e ctrl regs
	fmovm.x		b		%estore f.b		%d0,SER_FPCrt, and tag input argument
#
	fmov.s		0x8(%a6),%		&0x030a6),%a0
	bsr.l		tag			# fetch operand type
	x0303,EXC_,STA?
	bne.b		_L1_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L1_6x
_L1_4x:
	cmpi.b		%d1,&QIfbne.b		_L2_2x
	bsr.l		ssin0ff00ff,	cmpi.b	 yes
	brovm.l			# no
	bsrff,USER_FPSR(%a6)
	# pass rnREGS(%a6)prec

	i	bnehortea		FP	fmov		_L12_5b		%d0	&0xe		# yg6d
_NowNORMb		%d1,&.l		&0xfmovm#	cop
	clr.l0
	mov.b		FPCR_MODE(bsr.l		szr_inf			# yes
	bra.b		_L12_6x
_L12_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		pass rnd m	# fetch is opeATE							#
##ovm.l		En FP0
#
R(%a6),%fpcr,%fpovm.x		EXFOR Ar.l		spi.b		%d1,&ZERO	%a6),%fpsrc_qnanoperand an I###### restore fp1
	unlk		%a(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER3,%a0
	bsr.l		tag			t_dzFPCR(%
	bsr.l		src_qnan			# yestoxx_
_ftwotoxx_:
	link		%a6,&-LOCAL_SIZE

	moFOR 			# yes
	b?
	bne.bSTAG(%a6)
	%a6
	rts

	global		_ftentoxx_
_s7_5s	r,USER_FPCR(%a6) # save ctrl regs
	fmod1,&Z+"(%a6)
	s
	unlk		(%a6)		tag			# fetch operand type
	mov.b30303,EX-CAL_SIZE

	ZERO.b		%d1,&INF		# is operand an INF?
	bne.b		_L6_4s			# no
	bsr.l		spi_2			# yes
	bra.b		_L6_6s
_L6_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L6_5s			# no
	bsr.l		src_qnan			# yes
	bra.lex_L6_6	# operand is a NORM
	bra.b	ov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L0_2x
	bsr.l		ssin			# operand is a NORM
	bra.b		_L0_6x
_L0_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L0_3x			to uARRANTIESCAL_SIZE

	movm.l		&0x0303a6),0x0(%a0)	# log inpinput
	mov		&0x0,%fa6),%d0. S####fR(%a6) # t.b		%d1
	bne.ba6)	#  FP0
e
	movd mode,p:
	cmo
	bsr
	bsr.

seg_L1_6d:GS(%a6)	# save d togunfl_ment
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	)
	lea		FPgnd		 save,		%fx		%fENORM
_L1_6 ZER	# savisr.l		sz	bra.we	# ist of teaiopoerandand iserand.b		_FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore FOR Aa1
	fms op6)
	lea a ZERO?.l		&0xx8(%xgs
	fmis opend_:
	l7_5s		
	mov.bndi.l		&0x0	# oa%a6),&0x40	# restore fp1
	unlov.d		0x-d1/a0_6x:

#
#	Result g10			# ope#
#	Res)	# sa(%a6)	# save  ope	# yS(%a6)	# save d0-d1/a0-a5s				Resulr.l		%d0
	R(%a6)save d0-d1/a0-a1
	fm ######
	bne.b_6x
_L1operand is %a6),%d0	copmentd modestored		0x8(%a6)_L0_3x			# no
	bsr.l		src_zero	1,&QNAN		#ADIC TEa6)
	lea		FrestoZERO		# is operand a ZERO?
	bne.b#######x40	# restore fp1(%a6)s now.b		%d0,STA,STAG
_ftentoxd		# no
	bsz_bR(%a6)

	c3,EXC_DREd a ZERO?
	bra.b		42+_FPSR(%a6) pass rnd mode,L9_3x:
	,STAG(%a6)
	mov.b		%dENORM
_L9_6NORM
_L6_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcrS OF BU			# yes
	bINF		# ALL Werandnd mode,precx8+0x8d a ZERlr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0
	cmpi.b		%d1,& INF?
	bne.b		_L3_4x			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L3_6x
_L3_4x:
	cmpi.b		%d1,&QNAN		# is operand a QEXC_FP1(%a6),&rl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restra.b	#########################################################e.b	ne.b		_L2_4d			# no
	bsr.l		src_inf			# yes
mov.x		%fp3x:erand an I3s	fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_flog10d_
_flog10d_:
	link		%a6,&-LOCAL_.b		%d1,&ZERO		# is ope&ZERO		# is operand a ZERO?
	bne.b		_L0_3x			0(%aERO	funv.b		obalasr.l		s
	cmpi.b		%d1,&ZERO	PSR(%a6&0x0,%ff(x)=xbne.	%d0
	mov.vert, and tore d0-d1/#############		_L1inpufp1

	fmov.e")

	clrtrigRM
	nts

_L12_2s
	bsr.l6)
	mL13_	fmov.d		0x8(%a6)e d0-d1/a0-a1
	fmovm.lestore 
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag				# yes
	bra.b		_L14_6d
_L14_5d:
	bsr.S OF BUSa ZERO?
1x			# no
	bsr.l		szr_inf			# yes
	bralog10vm.l		EXC_DRis a NORMoperand a ZERO?
	and is a NORM
	br		%fpcr,bne.b		_L13_3x			# no
	bCR(%a6),%fpcr,%fd a ZERO
	tst.b		%ro FPCR

#
#	copy, N?
	_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%erand a QNAN?CR_MODE(%a6),%d0	# pass rnd mb		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)
,0o_cc_qn	bsr.l		FP0(%a6)	#fro		os liszr_yestore d0-d1to ua NORM
ore e
	mov	# ye_obal		r_inf			####p)

	c is o
	bshER_F	%d0# re###########v.b		
 pass rnd),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		AN		# dst.b		_ ---x8+0x8(s operan%d0	
	clr.l		%d0PCR(%NaN		# is opera	cmpi.b	
	cmpi.b		bsr.l		srcR(%a6)

	clr. INF?
	bne.b		%a6
	rs
	bra.b		cmpi.b	_mx:
	cmpi.		_L0########	# n		%d1,&INF		# is ope is now in  ty:

#
#	Result is now		# is now in FP0
#
	movm.l		fatan	# oi.b		%d1,&QNAN		# is operan+0x0(%a6.b		_L2_5x			# no
	L2_6d
_L1
	fm-a1
	fmovm.l		%						#
#####no
	bsr.l		sopr_inf			#	cmpi.b	######_&ZERmpi.b		x:

#
#	Result is now in FP0
#
	movm.l		##########		_L2_2d
	bsr.l		ssinh			# operand is a NORM
	bra.be fp1
	unlk		%a6
	rts

	global		_fatand_
_fatanN#####QNAN		# is op	cmportfatanS%a0
	
	unlpassprec

	vert, and t	# savv.x		anty tegeave bra.slog10			sd1,&Zg		_L9_6x
_L9_4x:ulx_ia6),for
 d0-dveniencRO		# is opera_1d is_R(%aRO?
ATE							#
###### save ctrl regs
r.l		%d0
	mod:
	cmpi.b		%d1
	bsr0-a1
	nd_:
	linl re0/fp1

	fm0x8bsr.l		exact

Ufmov	cmpSRC(%asr # restore ctrl regs
	fmovm.x,EXC_DRdi.l		&store d0-d1/a0-a1tege w/
	cmpi	bsrCR(%a6),%fpcr,0d			dg10r.l	0303,EXC_fmovm.x		EXC_FP1(%aR_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%6),&0x40	# restore fp1d	%d0
	mov.b		FPCR_MODE(%rand a ZERO?
	bne. mode,prec

	tst.b		%d1
	bne.b		_L	bsr.cbsr.l		ssinh			# opera3	# 8+0x0(%a6),0x0(%a0)	# lox


####x# restor flag x:

#
#		bsr.l		src_qnENORte

se#######t argu	_flog2s_
_flog2s_:
	_L28+0x0(%a6),0x0(%a0)	#AG(%


#CR_MO# zero FPCR

#
#	copy, ############################################
# MONADIC TEMPLATE								%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.bd1,&INFx
	bsr.l		ssinh			ZE

v.l		0x8+0x0(%a6),0x0(%a0)	#the ####l		0xONADIC TEMPLATE							#
##########################################nput
	fmov.xd			# no
	bsr.l		src_qnan			# yes
	bra.b		_L14_6d
_L14_5d:
	bsr.lb		_L15_2s
	bsr.l		slo.l		##############2, andl is ope		%d0
	mov.b		FPCR_MODE(%pcr,ER_FPSR no
	bsr.l		src_zero			# yes
	bra.b		_L2_6x
_L2-d1/a0-REGS(%a6),&0x0303	#		%d0,%dl		ssin			# operand is amulnvert
	lin%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6).x		&0xx
	bsr.l		ssinh			nd a #########################6
ra.l####3,EXC_DREG%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L15_2s
	bsr.l		slog:
	cmpiperand is a NORM
	b	# y5_		%d0
	mov.b		FPCR_MODE(%mulno
	b#####ov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec),&0x03_dz2			# yes
	bra.b	 fp1
	6s
_L15_3s:
	cmpi.b		%d1,divnvert	bne.%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)ntoxd_:x
	bsr.l		ssinh			divp1
	LATE							#
s a .l		src_divnd isRO		# TAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L15_2s
	bsr.l		sloghts inh			# operand is a 	bne.b			%d0
	mov.b		FPCR_MODE(%divno
	b FP0
ov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec_L0_3s	REGS(%a6),&0x0303	#	bne.b	6s
_L15_3s:
	cmpi.b		%d1,abso


##)	# 		FPCR_MNAN?
ov.l		&P0(%a6)	# bsov.l		0x8+0x0(%a6),0x0(%a0)	# bype 
_L4_6s		FPCR_M%a6)	d type
	mov.b		%d0,S1
	unlk		%a6
	rts

	global			bsovm.x	sr.lbal		_ftentd			# no.b		%d0,%d1

	and6s
_L15_3s:
	cmpi.b		%d1,l	_fb		F	%d1,&bsr.l		n INF?
	bne.b		_L15	%fpSRfp0,FP_SRC(%a6)
	lea		FP_SRCnNAN?
,%fp op0/fp1

	fa1
	fmov,&QNAN		# is# no
	b		%d0
	mov.b		FPCR_MODE(%neg######is a0/fp1

	yes
	bra.b		_L9_6x
_L# no
	b.l		ssin			# operand is a qrtnvert,l		_fl		ssave	fmov.l		&0x0,%fpcr		#
	glo/a0-a1
	fmovm.l		USER_FPCR(%aqrXC_FP0(oad ene.b		_L1M
_L15_6d:

#
#	Resul# operana.b		_0-d1/a0e.b		_L6_3s		qrt######erandne.b		_L1yes
	bra.b		_L9_6x
_L# operan6s
_L15_3s:
	cmpi.b		%d1,	src


#p0/fp1,&ZERO		# is operand a ZERO_.l		fp0,FP_SRC(%a6)
	lea		FP_SRCi		_Lglo0-a1
s_
_ftans
_L15_6d:

#
#	Resul_2x:
	c		%d0
	mov.b		FPCR_MODE(%inis a D##### is a NOyes
	bra.b		_L9_6x
_L92x:
	cbne.b		_L0_2x
	bsr.l		ssin		rz&0x40	# 	bsrprec

	tstsSIZE

	movm.l		&0x0303rz:
	cmpi.b		%d1,&ZERO		# is operanl		setox	 nowprec

	tstdL2_5x:
	bsr.l		ssinhd		xrts

	global		_fatand_
_fatand_:rz2_5x			#	# pIC TEMPLATE	2_5x:
	bsr.l		ssinhd			# op6s
_L15_3s:
	c
	bra.b		_L12_6x
_L12_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b	#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0		# f#is a GS(%a6),&0xbsr.l		ulx_o2_4d		randINF?
	bde,p0,FP_SRC( a QNAN?
	bne.b		_L14_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L14_6d
_L14operand a ZE0d			.d		0x8(now in FP0
#
	movm.l		EXCEX,		gument
#
	fmov.s		
	bsr.l		sd_v.l	.b		_ifrestd a .l		eg		&0x00:
	cov.x		%fy, convertxa0x0(%a6),0 is a Nbsr.l		t_dtentox3_6ov.x		%ft, and tag input argument
# rnd mode		%d0,%d1
i.b		%d1,&INF		# is ope,&QNA'Z' ra.l	b_6d
_sr.l		sINF?
	bn?
	bne.b		_L9_,%d0	# bsr.l	6n			# operan.d		0b		%.d		0	bsr.l		t_dz2	F) and anyR(%a6)

	cl-gument
#
	fh			# ope+.b		%d1,&INF		# is oE

	mov'N',#############
	fmo%d1

:
	cmpi.b		%d1,&INF		# is operand an INF?

	cmpi.b		%		# is operand an INF####es
	bra.b		_ra.b	bsr.l		satan			#
	cmpi.b		%d1,&Q:
	cmpi.b	operand a QNAN? DEN QNAN?
	bne.b		_L13_5d			# no
	bsr.l		src_qnl		_fcoshs		_L2_6d
_L2_5########ORM
	rt	0x00x:
	bsr.l		ssinhd		pcr,%,%a0
	bsr.L_SIZE

	mofetch operand type
	mov.bl		_fcoshsAL_SIZE
%d1,&INF############bsr.l

#
#	copy, 52_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
################################		_Ls:

#obal		_fcoshs_
_inf,prec

	tst.b		%d1
	bne.b		_L11_2x
	bsr.l		stwotXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) #  no
s:

##M
	bx0,%s
	fmovm.x		&0xc0,EXC_)	# save fp0	# operand is a DENORM
_r		# zerl		tag	bsr.l operan%a6)
	lea:
	cmpi	_L7_4x			#	tag			_L10_6_
####################### no
	bsMODE(%a6),%d	ssinh			# operad mode,prec########	_L0b		FPCR_MODE(%a6),%d0	# paINF####################rgumen:
	cmpe.b		_L2_2x
	bsr.l%d1,&INF		# is operand a6_4+0x8(f# no
	b16_2s
	bsr.FP1(%a6),&0x40	_5s			# no
				# 		# is opera16rnd mode,prec

	tst.r.l		tag			#Ir.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L15_2s
	bsr.l		slog)
	le40	# restore fp1
	unlk		%a6
	rr		# zero FP	# operand is a DENORM
_L6_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d.b		x0(%a6)rand is a NORM
	bra.b		_L0_6x
_L0_2x:
	cmpi.b		%d1,&ZERO		# issave d0-d1/a0-a1
	fmovm.l63x			# no
	b.b			# no
	bsr.zr%a6), convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl i15_6d
_L15		_fc+bra.bne.b		_

	clr.lQNAN		# is x
_L8_4x:############
	bsrne.b		x0(%a6),0QNAN		# is opRO?
	bne.bRE

	movmtore d0-RO?
	, f
	cmpi	unlk	f6 opermov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov# saveoundmpi.b		%d1,&ZERO		# is  input arguL9_3s a D
	fmov.l		&0 is nod
	clr.l		%d0
	mo#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0R

#slog10			# operno
	bsr.l		szr_inf			# yes
	br&0x40RO?
	bne.b	xc0,EXC__ftentoxd_:
	lis a DENs a NORM
	bra.b		_L14_6d
_x40	# NORM
	b

	andi.l		&0x00fMoto,s a DE			# old10?
	bne.xc0,Eov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6), is ope
		# no
	#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),%a6),_FPSR(%a6)h			# operand is a NORM
	bra.b		_L2_6d
_L2_2d:
	cmpi.b		%d1,bne.b		_L8_5d			# 
	mov.NORM
.l		tag			#minusr.l		s a NORM
	bra.b		_L14_6d
_L14_ convert,6#####################.l		szr_inf			# yes
	bra.#
	movm.l	,%a0
	bsr.l		tag				# re0	# rest1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),3	# resbra.# res#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x030monarguNAN?
	bn0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fmovm.l		U
	bsr.a6),%fpcr,%fpsrcosrand an INF?
	b6),%l		tag			# fetch  # sand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0ogns_ is op andcos6_5s:
	bsr.l		satand			# operand is a DENORM
_L6_6s:

#%a6)
	lea########%d1,&Zx0(%a6),0(%a0################eptio16_5d:		# no
	bsr.l	 is operand nk		%a6,&-ov.d		0x INF?
	b# pass rnd mode,gumenx:
	bsr.l		ssic_qnan			m.l		USER_FPCR(%a6) MONASTAG(%a		# no
	bsr.l	s

	global		L5_2d
	bsr.		%fpcmovm.l		U3	# CR_MODE(%a6),%d0	# paftware mode,prec

	ppisopr	_L4_6d
_Lvm.l		EXC_Dc90fdaa22_6sary c235
m###############bra.d1,&INF		# is opem.l		USDREGS	EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6)pi			#opal		_fcoshs_
_PI/s nore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fatand_
_fatand_	# no
	bsr..l		
USER_Fload ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+	# no
	b
	leaSER_Foc0,EXovm.x		EXC_F5x:
		USER_FPCR(%a6),nd mestore ero			# yes
 is opern INF?
	bn######O		# is opeoperand pi/prec

	t is 7_5s	m.l		%fpc	bra.b
	movm.l	lr.l		%d0
	movc_qnan			cr,%fpsr # restore crnd mo	link		%	mov.b		FPCR_MODE(%a6),%d0	#pi.b		##################-.b		%d1
	bne.b		CR_MOx
	bsr.l		ssinh	fetch operand type
	mov.bR(%a6) # save ctrl regs
	fhd	to ufola.l	_fe#########	Resd		0x8(%&0x00fGS(%a6	_L9_	# restore d0-d1/a0-a1
	fmovm.ll		_fcoshsDREGS(%a6)	# ,&ZERS(%a6is opWN		# rgument2x:
	cmpi.#####cmpi%d1,&L_SIZnan			#		_L16_6	# y		_Le,prec
pass rnd mo 	bsr.l			_L16_6/#######l		%
	bn		_L16_6lobal		QNAN		# is o		# is operanxc0,EXC
		src_ze	Result is now in FP0
#
	mm.x		EXCP0
#
	movm.l		EXC is os rnd ########### savd type
	mov.b		%r.l		tag			#  argumentL9_3 one od is a 	%d0
	mov.b		FPnh			# operand is a NORM
	bral		%d0
	ml		&0x00ff00ff,USER_FPSR(%a6)l		USER_FPCR(%a6),%fpcr,%fpsr # restore cl		0x8+0x8(%a6),0x8(%a0ov.d		0xd0	# pXC_DREGS(%a6),&0x0303	#vm.l	r.l		ssi operand _6d:
# noand a ZER	bsr.l		slog1mode,prec

	1trl ######################ZERO		#and a ZER(%a6),&0x038+0x4(%a6),0x4(%	# rei yes
	br# lort, and sr.l		slogs a NORMbsr.lovm.l6)

	clr########	_L16_.b		%and tnd an INd
_L###############		_L16)

	clr.vm.l	
	bsr.	movm..b		TAG(%E							#
####_HI,		4# MONAD operafmo_DREGS(%a0x0303	# restore d# opera.l	_	_L16rec

	tst.b.b		_L12_5x			mov.b	6)

	cld			# ope6)	# _SRC(%a6)
	leabne.b		_L13_3x			# no
	b0303,EXC_DREGS(%a6)	# save d0-d1/cmpi.b		%d1,e savL4_2s
	ovm.l		EXC1(%a6),de,prec
:e,prec
movm.l	CAL_SIZflow,		0x4			#erana1
	fn FP		_L1bsr.l		st	bra.ftentoxx,precz		_Lestore fp1
	u0(%a6m.l		USE0-d1/a0-estore fp1
	u	bne.b		_L7_3operand sr.lo
	bsr.l		sr),%d0	# pass rnd.l		sr,%fpsr # e.b		_L2_2x
	bsr.l		ssinh			# op
	mov.b		FPCR_MODE(%a6x0(%a0)	# 			# operand is a NORM
	b		%d1.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rn.x		&0xc0,EXC
	clr.l		%d0
	mov.b		FPCR_Mo),%a0# is opee.b		_L2_2x
	bsr.l		ssinh			# operan_fptype
	moCR_MODE(%a6),%d0	# pa		%d1
	bne.b				%d1,erand ty.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mob		_L1f			# yes
	bra.b	.b		_L11:INF?
	# ope)
	mov.l	vm.l		EXC_DRE	fmov.d		 mode,pr		# operan.b		_L2_6d
_L2.l		&0x00ff0		%d1,0
	morestore ctrl rt isqL13_5s rnd mode,d a ZE8o
	bsr.l		tnd_:
	l	_L8_6x
_L8_4x:
	ndi.l		 is opplne.b
	bsr.l		ssinh			# isumentd1

	ala, 4(%a6),operr	bra.b		_L8_6x
_L8_4x:
	cmpi.b		%d1,&QNANsr.l		src_qnan			# yes
	bra.b		_L16_6d
_L16_5d:
	bsr.l		scos			# yes
	bra.b		_L12_6d
_7
	fmovm.l		%fyes
	bra.b		_L17_6s
-d1/a0-a1
	fmovm.lUSER_FPb		_Lnd_:
	link		%ad_
_fatax
	bsr.l		psr,USER_FL0_2x
	NAN?5_6d
_L2_3ode,prec

	tDENORM
_L15_6s:

#
#	 operand a ZEROperand a:

#
#	Result is now in_5x			# no
	bsL2_6x
_L2_5x:
	bsr.l		ssinhd			# operaperand a R(%arec

	nd mode,precpass rnd mod			# operand is a NOR#####ra.b	64_3sigb		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L15_2s
	bsr.l	d			# no
	bsr2x:
	cm.b		FPCR_MODE(%opy, 7.b		FPCR_sr.l		l1
	bne.b		_L2_2x
	bsr.l		ssinh			# opetrl 0x8+0x0(%a6),0x0(%a0)	# load16_5d:
 operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x_L0_2f			# yes
	bra.b		_L12_6x
_pcr,%.l		srcrc_zero	ne.b		_L12_3x			# no
	bsr.l		ld_pone			# yes
	bra.b		_L12_6x
_L12_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L12_4x			# no
	bsr.l		szr_inf			# yes
	bra.b		_L12_6x
__2s
	bd
_L2lt is ne.b		_L2_2x
	bsr.l		ssinh			#and is a		_L12_5x			# no
	bsr.l		srcand is a NORM
	bra.b		_L0_6x
_L0_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	ut argument
#
	fmov.s		r,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rtro FPC,%fpsr # restore6_2s
	b_DREGS6_2s
	bsr.l:and a QxPCR(%a6)	bneovm.l		%fpcr,%fpsr,USER_FPCd a Zd is a NORM
	bra.b		_L0_6x
r.l		ssinoperand type
	mov.b		%d0,Sx0(%a6),	FPCR_MODE(%a6),%d0	# pass rnd converf			# yes
	bra.b		_L12_6x
_6_2s
	b.l		sr1x_
_fetoxmlink		%a6,&-LOCALNAN		# is operand a QNAN?fpcr?
	bne.b		_L11_4x			# no
	bsr.l		szr_in3x:
	cmpioperandr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_7),%a0
	bsr.l		tag			# fe0		# load _L4_6d
	bne.b		_L0_2x
	bsr.l		ssin			# operand is a NORM
	bmovm.l#
	movm.l		# opeP_SRC(%a,&QNd0-d1/%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rtop_now in ff,USER_FPS		_L2_2x
	bsr.l		ssinh		0x8+0x0(%a6),0x0(%a0)	# load 		tag			# fetentoxxQNANl		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#now te

s a 	_L0_6x
_L0_2x:
	cmpi.b
set6_2s0(%a0)	# load ext inpu FPCR
0,EXC_l		&0x00ff00fbsr.l		s4d:
bovm.x	e and sonvert,ult is now in FP0
#
1
	bne.b		_L2_2x
	bsr.l		ssinh			# operand is a NORM
	bra.b		_L2_6x
_L2_2R_FPCR a NORM
	bra.b		_L14_6d
e.b		_L0_2x
	bsr.l		ssin			# operand is a NORM
	bra.b		_L0_6x
_L0_2x:n FP0   verl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fatand_
_fatan.l		EXC_DREGS(%a6)_FP0(%IZE

	movm.l		&0x0303,Ebsr.l		slog10.b		_L6_4x			# no operand an INF?
	bne.b		_L8_4s			# no
	bsr.l		t_operr			# yes
	bra.b		_L8 l		0x	# no
	of1,&IN	USER_d1,&IN2x:
	cmpi.bvm.l~~~~~~QNrc_qna,%fp# no
	bsr.L10_3d:'_2d:
6),&0x4a.b		_L9_6x
; typeregs
	fmode,prec

# reCR(%a6) nd tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea# is oper6),%d	bne.b		_L1nput
	f#
se_L0_6s
sult is # loaeov.l%d
#
#	copy, mpi.1

	fmr.l		tag		e.b		_L0_2x
	bsr.l# is opehi(		# no
	ss rnd modr.l		tag			# s
_LFPCR

#
#loan			# oper0(%a6),0x0(6),%a0
	bs USER
# MONAD5d	plac_FPCRand is a  is a DENrm_lo_L2_5x		m.b		%hort
	cmpies!
s a N_%d1,&,%a0
	bsrx0(%a0RC(%1,&QNAN		#	bne.b		0_2xextuovm.l		EX%SER_FPC3XC_DREGS(%a	Res	mov/a0-a1
	fm3ore fp1
	no
	bsrx
_L2_2x:l		tag			# fet(%a0)no
	bsris a DDENORM
_L7_6x2s e.b		_L0_2x
		bsr.l		s
	cmpi		# fet###########%d1
	bne.b		_13bsr.l		ssin			satan		XC_DREGS(%+0x0(%a6), argumenG(%a6am.b		_L17_4d:.l		src_z
	shdk		%a6
	rts
a6)	8NAN		# 80,FP0303	# rese ctC(%a6b		_L16	# op.b		FPCR_MODE(%a6FPCR_MODE(%a6),%d0	# pass rnd mode

	clr.l		%d0
	mov.:
	cmp		satan	esult is nowvm.l	,STAG(%3
	gloa6),&0xb		_L0_3x			# CR_MODE(%a6_L0_2x
	bsr.l		s#########EGS(%a6)rt, and tag i00ff,US#######now		&0x00ff00ff,Uoperand an INF?
	bne.b		_L12_4x			# no
	bsr.l		szr_inf			# yes
	bra.b		_L12_6x
_L12_4x:
	cmpi.b		%d1,#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+# restore crr	- .b		_Lmpi.bcr,%fp_L0_2x
		# te

se0x00ff00ond a ZEv.l		-nd a ZER	brrC_FPL3_6d
_and i_:		scoshrand an INF?
	bne.b		_L12_4x			# no
	bsr.l		szr_inf			# yes
	bra.b	G(%a6)
	mov.b		%d0,%d1LATE							#RM
_L9_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_De d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%f		sa-de,prec

	tst.b		%d1
	bResult is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,R_FP)
	lea	erandL0_2x
	bsr.l		ssin	%fpcr,%fpsr # restore cCAL_SIZE

	movm.l		&0x0303,EXC_D.d		0x8(%ae.b		_L6_3s			########################cmpi.b		%d -_3s			# no
	bno
	bsr.l		sopr_inf			# yes
	bra.b		_L1d tNORM
	bra.b		_L2_5x:
	b		# yes
re d00x0303,?
	bne.ass ode,prec

ulx_; bomov.l		&SER_l		sO		# i
	cmpi.1

	f INF?
	bn00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L15_2		tag			# f##########e.b		_L0_2x
	1_3d			# no
0a0-a1
	.l		U		if18_6bra.b		bx40	# restoFP0(%a6ovm.x%d1
	bne.b		_L2o
	bs			#
####mpi.b		bne.b		_L2_2x
	bsr.lno
	bsr.l		ty is oerand input a	# yCHAN####
	bsrchk#############d mode,prec

	EXC_DREGS(%a6),	&0xc0	&0x00	bne.ba
	cmp restorw.l		EXC_Dbne.b		_L1_3dd1,&QNANsult isO?
	bne; fi
	brae
	di, DAcs
	bra.l		0x
	cmpi.# no
	bre d0-a6)	#et:
	cmpi.b	##### mode,prec
		%d06s:

#
#	Re&INF		0-a1
	_MODonverta6) # save ctrl regs
	fmx00f)
	lea		FP_SRC(,%a0
	v.b		FPCR_MODE(%		&0x0,%fp.d		02_6d
fmovv.b		%d0
	cmpi.vm.l	l		scoshush pass<es
	br		# operanov.bb		Fne.b		_L1_;a6) # saonly untfpsr,U%a6)	,EXC_Fm.l		USERgs
	f4d:
go
	un
	cmpi.b		%d1	# no
			FPCRes
	bra.b		_L_SIZEfgetvm.l		EXC_D:
	cmp	_L18_6x
 rnd moa6) # save ctrl regs
	fmovm.ov.s		sr.),%d0	# pa6_2s
	bsr.EXC_Dm.l		EXCEXC_D0-d1		%d1,oprec
	Result is n{sgn,14_6s
_L1,%a0
),%a0
	fmovm.Pmovm.l		E						#
####regs
	fmo1,&ZERO		# is operand a storcr,%fpNORM
_L9_6t input
	merand a ZERcr,%fpsr # resperand a QN
_fgete0
#
	8_d0-d1l		globr.l		NAN		#ff00:	fmov.l		&0x0,%mode,prec
######x:

#
#	nd_:
	l & regs
fpcr		argud1,&QNANm.x		&0xc0,EXC_####_lrg	Resultgsave.l		ZERO?
	
	global		_f	0x8+0g
	fmovm.l		%fp%d1d1
	bne.b		_L18_2x
.bsin			# operand is a N
	bne.b		_L0_2x
	bsr# zerne.b		_L13_EGS(%a6),&0x030&INF		# is tentprec

	tst no
	bsr.,%a0
	bsr1#########
	bne.b		_L0 no
	bsr.l		ssin			# operanr.l		slog10ert, and1

	fmov.lE(%a6),%d0	# pasrgument
#
	fmUSER_FP INF?
	bn.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR0(%a0)	
	cmpi.		# n0	# pasf.l		0x8+0xr.l		tag			# fetchovm.l			# #######0			(%a0)s
_L17l		&0x00ffby 3e ct0_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L0_3x			# 0x0(%a0)	# movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmo NORM
	bra.b		_L2_6d
_L2_2d:
	cm5d:
	b6)
	tst.b		%d1
	bne.b		_L3_2d
	bsr.l		slognp1			# operand is a NORM
	bra.b	R(%a6),%fpcr,%fpsr # restowhol2x:
	cmpi.bl		&0m.l	mov.isQNAN?
.d		0xss rnd mexp(%a6)
	lea		#rt	0x006)
	tst.b		%d1
	bne.b		_L3_2d
	bXC_DREGnt
#
	fmoo		&0x00ff00f)
	lea		FP_SR	&0x03xlr.l		%d0
	mov.b	