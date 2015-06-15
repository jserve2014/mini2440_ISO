~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocessor Division
M68060 Software Package
Production Release P1.00 -- October 10, 1994

M68060 Software Package Copyright Â© 1993, 1994 Motorola Inc.  All rights reserved.

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
	lea		~~~~~~~~~~~,%a0
	bsr.l		tag	~~~~fetch operand type
	~~~~b		%d0,DTAG~~~~~~~~~~~~~d		0x10
MOTORO~~~~~~~~~~~~~~~~src~~~~~~~~~~~~~~~~~~SRC~~~~~~~~~~~~~~~uction RROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
M68000SHi-PerforGROUPlM68000%d1

	anditoro&0x00ffeser,USER_FPSRPerformaclROPRO%d0 GROUP
M6FPCR_MODE-- Octod0	# pass rnd mode,prec
elease P1.00 -- Octobe~~~~ warrptr to Softw~~~~~~~~~~~
MOTOROL1d by applicable ~~~~
	tstP
M6801
	bneP
M6_L22_2d MICROPROsmod_snormSOR &Y TECHNOis a NORM
	braIED,
INCL6d

INCLUD:
	cmpiRESS OR,&ZERO~~~~isOF MERCHAa  any? IMPLIED,
INCL3dSOR &noING IMPLIED WARzeroSOR &yes OR FITNESS FOR A PART3CULAR PURPOSE
andINF warranty againsn INFringement with 4egard to the SOFTWARE
(infUDING ANY MODIFIED VERSIONS TH4CULAR PURPOSE
andQNAN warranty againstBLE ringement with 5egard to the SOFTWop_sqnanUDING ANY MODIFIED VERSIONS TH5CULAG IMPLIED WARdnNTIES OF MERCHANTABIDELITY SS FOR :

#
#	Resultrrannow in FP0
#94 MomtoroEXC_DREGS-- Octs re303	# restore d0-d1/a0-a1~~~~~~ OF T
THE SOCTWARE r Dicrr Disr USE THE SOctrl regsorola asx THE UFP1 INABILIT4houtE THE SOfp1
	unlk		%a6
	rts

	global		_fmodx_
 use, m:
	lina copy,&-LOCAL_SIZE
ISING OF TLITY TO,HE USE OR INAB	# savSOFTWARE.
Motorola assumility for t

THE SOesponsi  retainnce and support of t&0xc0entirFPocessos retainfp0/gran~~~~~~~g as thbility~~~~INCL nd/o OR OTcopy, convert, CHNOLag input argumentARIS~~~~~~~~~~~
MOTOROLA Midentif0x8+0xocessorrks of0ersi~~~~~ext~~~~~~
or trademark4 of Moto~~~~0994 Motorodemark8 of Moto~~~~~~~~ICROPROCESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performaease P1.00 -- October ~~~~~~~~~14arks of Motorola, Inc.
~~~~~~~Softwentry points i~~~~~~~~~~~~~~~~~~~~~~~~~~nts i~~~~~
#
# lfptop.s:
#	This file is appended to the top of the 0 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is provided on an "AS IS" basis and without warranty.
To the maximum extent permitted by applicable law,
MOTOROLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
INCLUxING IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FORxA PARTIxULAR PURPOSE
and any warranty against infringement with rxgard to the SOFTWARE
(INCLUDING ANY MODIFIED VERS0000
	b3a.l	_fatanhx_
	shompanying written materials.

To the max	bra.l	_fcosd_
	short	0y applicable law,
IN NO EV0000
	b4a.l	_fatanhx_
	shBLE FOR ANY DAMAGES WHATSOEVER
(INCLUDING	bra.l	_fcosd_
	shN, DAMAGES FOR LOSS OF BUSINESS0000
	b5a.l	SINESS INTERRUPTION, LOSS OF BUSINESS INFORMATIOx, OR OTHER PECUNIARY LOSS)
ARISING OF THE USE OR INABILITY TO USE THE SOFTWARE.
Motorola assumes no responsibility for the maintenance and support of the SOFTWARE.

You are hereby granted a copyright l
#og10s_
	short	0x0000
	bra.l	_flog10d_
	short	0x0000
	bra.l	_flog10x_
	sh
# DYADIC TEMPLATE	_flog2#log10s_
	short	0x0000
	bra.l	_flog10d_
	short	0x0000
	bra.l	_flog10x_
	shoricense to uscalesmodiogns_
	nd distribute the SOFTWARE
so long as this entire notice is retained without alteration in any modified and/or
redistributed versions, and that such modified versions are clearly identified as such.
No licenses are granted by implication, estoppel or other~~~~~s~~~~~cessor Division
M68sg~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Ms_
	scort	0x0000
	bra.l	_fremSoftware Package
Production Release P1.00 -- October 10, 1994

M68060 Software Package Copyright Â© 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is provided on an "AS IS" basis and without warranty.
To the maximum extent permitted by applicable law,
MOTOROLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
IN3_2NY M IMPLIEogns_ARRANTIES OF MERCHANTABILITY OR FITNESS 3_6sORMA	braULAR PURPOSE
and any warranty against infringement wit3_3sgard to the SOFTW
	shortINCLUDING ANY MODIFIED V
	bra.l	_f3anhd_
	short	0x00ompanying written materials.

To the 3_4_ftentoxs_
	short	0x0000
y applicable law,
IN NO
	bra.l	_f4anhd_
	short	0x00BLE FOR ANY DAMAGES WHATSOEVER
(INCLUD3_5_ftentoxs_
	short	N, DAMAGES FOR LOSS OF BUSIN
	bra.l	_f5anhd	_ftanx_
	shortUPTION, LOSS OF BUSINESS INFORMA
	br, OR OTHER PECUNIARY LOSS)
ARISING OF THE USE OR INABILITY TO USE THE SOFTWARE.
Motorola assumes no responsibility for the maintenance and support of the SOFTWARE.

You are hereby granted a copyright license to uogns_d	short	0xd000
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

	bra.l	_fremicrophort	0x0000
	bra.l	_~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocessor Division
M68060 Software Package
Production Release P1.00 -- October 10, 1994

M68060 Software Package Copyright Â© 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is provided on an "AS IS" basis and without warranty.
To the maximum extent permitted by applicable law,
MOTOROLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
IN3LUDING IMPLIE
	short	0x0000

	bra.l	_ftanhs_
	short	0x0000
	b A PAC_VOnhd_
	short	0x0000
	bra.l	_ftanhx_
	short	0x0000

	bra.l	egard to the SOFTW0x0000
	bra.l	_ftentoxd_
	short	0xacked <EREOF) and any accompanying written materials.

To the 3aximum extent permitoxd_
	short	0x0000
	bra.l	_ftwotoacked <LL MOTOROLA BE LIABLE FOR ANY DAMAGES WHATSOEVER
(INCLUD3NG WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINset of savS,
BUSINESS I
	bra.l	_faddd_
	short	0x0000
	bra.l	_faddN, OR OTHER PECUNIARY LOSS)
ARISING OF THE USE OR INABILITY TO USE THE SOFTWARE.
Motorola assumes no responsibility for the maintenance and support of the SOFTWARE.

You are hereby granted a copyright license to uogns_ modi,		EXC_nd distribute the SOFTWARE
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
	short	0x0003

	bra.l	_fat
	short	0x0000

	bra.l	_ftanhs_
	short	0x0000
	b0000
4
senhd_
	short	0x0000
	bra.l	_ftanhx_
	short	0x0000

	bra.l		bra.l	_fcosd_
	sh0x0000
	bra.l	_ftentoxd_
	short	0xr addre

	bra.l	_fcoshs_
	short	0x0000
	bra.l	_fcoshd_
	short	3x0000
	bra.l	_fcoshxoxd_
	short	0x0000
	bra.l	_ftwotor addret	0x0000
	bra.l	_fetoxd_
	short	0x0000
	bra.l	_fetoxx_
	3hort	0x0000

	bra.l	_fetoxm1s_
	short	0x0000
	brSR_AEXCEPTm1d_
	short	0
	bra.l	_faddd_
	short	0x0000
	bra.l	_faddbra.l	_fgetexps_
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
	short	ssin(): 16		computes the sine of a RANTalized, estog2s_
	MP,		dLV+16		# temporary space

set deDTAG,		LV+15			# destinacosLV+16			# temporary scopace

set DTAG,		LV+15			# destinacosn operand type
set ST	# flag: spe			# source operand stinatioset SPCO# temporary space
CHNO	# flag: special case (see beTR,	LV+4			n opeed current PC (active)
set EXC_EXT			# source operandR OTC_OPWORDstinINPUT * saved operation word

################################

# Hel R OTa0 = pointeable extended the ision15			# desti	dcrosroun# offsets w,.
ToWORD,		LC_OPWORD,		LVOUT			# saved operation word

################################

# Helful mfpcros,		LX) or)
se(X)PWORD,		LC_OPWORD,		LV   For	LV+4			#X):_flog2s_
	FTEMP_GRS,		1X,		0			# ext1 =set LOCAL,		0t EXC_OPWORD,		LVACCURACYtive)MONOTONICITY# saved operation word

####################ful mThe returnede he PECUNIwithin 1 ulp LOS64 significant bit, i.e.t EXCDST_EX,0.500		0			to 53ion
s ifary sin an
set subsequentlyt EXC_EX,	edS WHEouble offsets w.  offse an
set provably monotonict EXCinthin an
set SRC_EX		# value saved in memoLGORITHM# saved operation word

################################

# t EXC_OPWORD,		L	SINtive)COSEX,		0			# e1. If ec eis invoked, set AdjN := 0; otherwise prec exponent1ST_HI,# max sgl pr2	0x3c|X| >= 15Pi2

sEXT_< 2**(-40), goed i7d ine			# max sgl pr3. De# teose X as X = N(Pi/2) + r where |r| <= Pi/4. Leet EXC_kxpony.
T 4, so LOSparticular,  pre0,1,2,or 3.et EXT_Overwrite k by knentk + expod in e			# max sgl pr4	0x3ck		# even exponen6d in me			# max sgl pr5. (ias

odd) Set jnent(k-1)/2, sgnO,		0-1)**j
set EXT_Rets w ope*et Lr)XT_BIAS,		0x02NTABpproximatedset an# operset  polynomial LOSr, 1et E*r*(B1+s*(B2+ ... + s*B8)),# opersEMP_*rd in memory.Exitd in me# operand type fo6 STAG/DTset set ZERO,		k		# operand type fo TAG
set INF,		Lr)# operT_BIASd type type for STAG/DTAG
seTAG
x03			# operand # operre fors*(AG/DTAA
set DENORMA7	0x0 operand for STAG/DTAG
set SNAN,		0x05			#7
set EXT_L 1 exponen9d in memory.
set SRC_L	8. (|X|<ext prec)0x3c01			# min dbl psets w X;set SNANt DBL_HI,n_bit,		100			# operand type fo9. ision biasXset XnentX rem 2Pi. Now that	# min3fff,et SNANgo backed ingle
set SNAN,		0x05			#SINent
set DBLL_LO,		0x3cEXT_LO,		0x0			# min ext prec exponenx00		dbl prec exponent
s# max ext prec exponent

set EXT_BIAS,		0x3fff			# extended precision bias
set SGL_BIAS,		0x007f			# single precHI,		0x7ffe			n bias

set NORM,		0500			# operand type fo4 STAG/DTAG
set ZER1O,		0x01			# j2nentj1 (EOR)STAGsion2), iDST_HI,	j1 exclusive2

sDST_ary sl.s.b.

sek
set dz_bsgn aunfltype f1# ope		# aued ine2
set dz_bSIN		12= ope1 *operand xponent########
2nd typeXT_BIAt,		3		######ive)
se###### opetempodrec /FPCCHNOset et dz_b03			# ope# mid tyrespectively. TAG/		# operand type for STAG/DTtype for ST aunfTAG
set  accrued inex####################
# FP###########al bit masks1F,		0x02			# ##################
set neg_mask,		0x08000000		# negative bit mask (lw)
set inf_mask,		0x02000000		# infinity bit m6
set inf_bit,		0x1		8bit,		5			# operand err7n_bit,		0x0			# N#########Xividual bit ma1200000		7			# accrued ine8			# branch on unordered
set snan_bit,		6			# signalling NAN
set operr#####set dz_bit,		2			#g10s_
	short	0x0000
	bra.l	_flog10d_
	short	0x0000
	bra.l	_flog10x_
	shor
SINA7:	long~~~~BD6AAA77,0xCCC994F5 unfl6mask,		0x03DE61209,0x7AAE8DA1 unfl5mask,		0x00E5AE645,0x2A118AE4 unfl4ption mask
EC71DE3,0xA5341530000403mask,		0x00F2A01A0,0x1A018B5_mas0	# inex00		# inex1 unfl2ption mask
FF8nex1 ex800080		000000859AF exception mask
1ask
set inexFCnex1 exA00040		00000040	9100		# inex1

COSB8ption mask
s2AC4D1 exD6011EE3unfl__mask,		0x000A9396lleg9F45AC19unfl_eption mask
E21EED100		612C972unfl_0		# dz excep927E4llegB79D9FCFunfl_sk,		0x000002F_mask,		0x00D42 ine	# inex1 exception mnfl_mask
set inexF5nex1 exB60#########B61D43# ac combinations
set aiop_maskA		0x00000040		# accruedB5Etionsset aovfl_masw
set au	set		INARGProduCRn_mask+opXask+aiop_#ask
setDCARE,X+2mask
setFRAC,X+4_mask+opRPRIMEask+aiop_ask+opSinx_mask,	unarlysk+opPOSNEG1,Lk+aunf_mask+TWOTO63ask
set  unf2inENDFLAGask
seclears NINTainex_ma unf2inADJNask
se3flog10s_
	short	0x0000
	bra.l	_flog10d_
	short

	bra.l	_P,		
P,		:# effect,&0,_mased verING AN; SET _mas TO A MI FITNESINBGN_mask+aovfl_mask+ainex_mask
set inx1a_mask,		inex1_mask+acos
k+aik
set inx2a1mask,		inex2_mask+ainex_mask
se1_mask+aovfl_mask+ainex_mask
set inx1a_mask,		inmask,	:
#--SAVEcense, FP1. CHECK IFoverfIS TOO SMALL OR LARGRE
s~~~~~~~~lfptor DivisioLOADV+0			~~~~~~~~~~~~~~~X		inex2_metain estoppt X

# "COMPACTIFY" X# effect,rr_maskdALL WAut ex		# ehi word# effew		~~~~~~k+aovfl_ MEMORhi(man) All ntified7F			# sk+aovfl_stripendedis p PURided 1ILIT3FD7,		0 warranEXT_LO,ext precringgLIED,SOK1gard to thra###
SINSMUDING AN;mask+aiis very small

			#ULAR Pet sign_bit4004BC7E	# sign bit< 15 PIringlPRESSSINMAIN signalling nan bREDUCEXet sgl_thresh,		0x3f81	large
sk+aTHIS_maskHE USUAL CASE,			# sig		# m.e,		0xE ARGUMENT 0x0		TION_masDONE BY TABLE LOOK UP.,	inpone:~~~~~~~~~~~~~~~%grantfmul000
TWOBYPI(%pcask+a1	# X*2/PIckage
# PITBL+0x200zero
searm_m,		0x0OF N*PI		# N = -32,...,3ask, identifet r,IN~~~~~~x2_mCONVERTk
seINTEGERE
so let sset mantk+aovfl_make a gran

seN
	aslntifi4k+aovf	# N *= 16
	ady_bitign_S ALL	# tbl_addr = ape f(N*1form# A1		# singADDRESSrp_mode,BY2
#et DWHICH_masIN TWO PIECES Y1 & Y2
	fsub	operr_1)+r DivisioX-Y	0x1et
sst INEX_r DivisioTEMP_GR = (			#)-Y2t unfCONTask+acontinuats wifromprecisE_mas--GET N+_maskAND SEEk,		####R)set al bR)_masNEEDEDof mantissa in bits

	2			# leask,		inek+aovfl_tor offD0_masODDset EVEN
	roROPRO&1 byte
setD0 WAector IFl    vecNEGATIVEsgl exponent
senimum w		COSPOLY0xcc	LET J BE singLEAST SIG. BITrp_mD0, CONDSGonentued inJ		0x8			N WE RETURN	SGN*set
se.#### bit: f_masset UTED BYxcc	R' + R'*Sset  + S(A2ag bit3ag bit4 set DENOS# ne)), WHEREflg,		0=rapccR, S=R*R. 0x4		CANtionREWRITTEN ASflg,		0x02			#  [A1+T(A3a>
s5+T# ne] + [bit:a>
s4+TA6))])xcc	x04		 T=S*S		0x8NOTon FAT A3 THROUGH A7 ARE STORE
set DOU	0x0PRECISIONfmovm ILEen(l vecA2flag data> <ea-EXTENDED FORMATund-t SPearest
sd that s0c,-(%spsk+aunfl_mfp2/fp3_mask,		opek,	unfl_mask+auX_masR0x1			#t rz_mode,			# sFP  vecSrmance Micrunfl_zero
set 3
#############6zero
set o-plus-int rz_mode,		0x1			#######1set rm-OP" (longw
set SNAN_VEC,		_stky_bit,	2,		0xex1 formSUN_VGs #
###############_bitSIGN POSIion

	e SNAN_ign_#############
#NOW R' bit: (-plusrmed last
set 3DIV_TA7erformed last
set 2######6-plu			########5##########DIV_movm_f64634	# 16381 4FMUL_OP,		2:	lolag b# CONSTANTS #
###########fmovm_fl
T1:	long		0x40C62D38,0x flag biD64634	# 16381 3OG2 LEAD
T2:	loet fmovm_fl64634	#x######275CC7	# 16381 L		# flag bi# CONSTANTS #
3set FDIV_>
set fmovm_flg# CONSTANTS #
ode,	6381 40			# flag bitx00000000

TWO1zero
set rm:	loea>
set fmovm_flgerformed lfl_masr Divisio2			D64634	#######2ATE						 <ea>
set fmovm_flg,+x40			# flag bit:# CONSTANTS #
#####		# sset
s')-R'-plus-i of tmmed_+_bit,		# s hereby gr#############orola In such.
NE THE SOusersP_EX,		.
To the m########################last inst - possi an
except
setsetling 		t_inx2cial CONDition FLaGs #
###########################
set ftrapcc_flg,	0x01			# flag ERR_VEtrapccERR_VEC,		
set fbsun_flg,####+ S'STAGag biB: bsunBexceptBon
set mia7,		0,		0x04			# f)+ <e  vect'=apcc a>
set mda7_flg,		0x08			# flaP_SRC(%a6)[Bea>
Bet fB5+TBflg,		0x40B		# B4FPCR6+Td typ: fmovm instruction
set immed_flB4		0x80			B8flag bit: &<data> <ea>

set ftrapcc_bit,	B20
setB3bsun_bit,		0x1
set mia7_bit,		, B(long-1/2xcc	 vec	# sinREFOag bit: &<ASm.l	Gea>

set ftr.tionet mda7_bit,		0x3
set immed_bit,		0x7

##########SCENDENTAL "LAST-OP" FLAGS #
#########nfl_mFMUL_OP,		0	_L0_4s			# n############x0			# fmul instr performed last
set FDIV_OP,		0x1			##########################
#S		# fdiv performed last
set FADD_OP,		0x2			# fadd performed last
set FMOV_OP,		0x3
T1:	long		0x40C62D38,0xB8
3			# fmov performed last

#####S#######Smed last
set FADD_OP,		0x CONSTANTS #
###########B7

	 SNAN_Vit,	 FADD_k+aovfl_   vec####set Fo
	b94 Motorola1,ainex_mPerforman34	# 16# acc75CC7	# 16381 E(%a6)	%a6
	rts

	glLOG2 LEAD
T2:	lr.l		%_6s:

#
#	Result is now iDE(%a6),AIL

PI:	long		0x40000000r.l		%d		%a6
	rts

	glE75CC7	# 16381 R_MODE(%a6),.x		&0xc0,ions LOCAL_SIZE

	mo
	clr.l		%d	&0x0303,EXC_DREGS(%a6)	# s_MODE(%a6),%C9C883

#################

	clr.l		%d0################ save ctrl regs	FPCR_MODE(%a6),%	%a6
	rsa6),%f TEMPLATE						(%a6)

	clr.l		%d0.l		USER_FPCR(#####	movm.b		FPCR_MODE(%a6),%d##############IZE

	.l		USER_FP###########3,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xsk+ainex_m############ save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
sk,		0x00001000		# overflow exception mask
set vm.l	e: Bigset S	# m?xcc	,		neg_>,		0I,	0x0USon FLaGENERAL# double precision
		0x8,		neg_n ext prec e1			#  Xset 1und-tBORS sgl exponent
set3Fsk,		0bit,tr,%f	0x0			#t unfSMk
set inx2VFL_VEC,		0xd##################gPRESSCOSTINecia _BIA,ary sY TECt
setmay fsetrflRY Lf
set Doffsets wits_fremort	0x.
#P,		0			# 			# ssneg_mhandlts whrough ant DBL entrys

setund-te.b	:
########
s res00,ffffffed versiJUST_FP1cisi-plus-infinisr,USER_FPCR(%a6) # save ctrl regs
	fmovm.xROUP
M6&FMOV_OPk+aovfl_save fp0/fis MO####~~~~~~~~EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero Fcatchaunfl
#
	mo	_frems_
tore ctrl reg2d
	bsTEMP_G1.0a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%dset FEXC_DREGS(%a6)save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero Fp_6d
_L0_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?	inex1_mask+ainedsk+aibyte)

setFORESS INFALIZED Xx_mas,
BUSzero FextUPTInan_mask+snan_mask+aiop_mask
set snaniop2_mask,	snan_mask+ai	FP_Sn_mask,		00
	mov.l		0x8+0x0(%a6,0x8movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input arset ex1_mask+aine+aiop# passask+ainex_mask
se4
set inx2a4mask,		ine_mask,		operr_mask+aiop_mask
set unfl_inx_mask,	unfl_mas of mantis misc. #
########
# misc. #
_stky_bit,	29			# stky bit pset ovfl_ix_ma	# no
	bsr.l		t_o	0x7			# s bit
set signan_bit,		0x6	COKR IMg nan bCSM

 oper sgl exponent
set dbl_thresh,x3c01			# minimum dbl Cpone x_mode,		0x0			#_flo		0x4			# single precision
set d_mode,		0x8			# double precision

set rn_mode,		0x0			# roundx
_L0arest
set rz_mode,		00x1			# round-to-zero
set rm_mode,		0x2			# round-tp-minus-infinity
set rp_mode,		0x3			# round-to-plus-infinity

set mantissalen,	64			# length of mantissa in bits

(byte) == 1 byt	2			# len(word) == 2) == 2 bytes

set ,_FP1Y1,r offfset
set INEX_VEC,		0xc4			# inexact vector offset
s" FLAGSVEC,		0xc8			# dC     vector offset
set

sett UNFL_VEC,		0xtore ctrl regs
	fmovm.# fdiv performel exponent
se0xd8			#< 0  vec

setODDbit,		w		Nset
s
SNODDask+aREGISTERS iop_D SO FAR:#####A0set 2.a7_bit,		0x3
se4 immed_bit,		0x7

#N?
	bne.b		_L0_5finx_m~~~~~~~F		# is operand an INF?
	bne.DZ_V* TRAN####################						#
T1:0_4s			# no
	bsr.l		tC(%a6_:
			# is operand d4			###
T1:	long		0x###########n FP0 Motorola2 immed_&0x40	# resto%d2##############2med last
set FADD_OP,		23			# fmov pe	FP_SRC(%a6),%a0
	bsr.l		tag64634	# 16381 obal		_fsi						6+nt
#
	6
	rts

	gl########## conve7+0		# l and tag input argumen(l		&0x)3			# fmov 2 save ctrl reg.b		%d1,&6)	# 	tag	#######################.l		%d)		# fdiv performed last
set FADD_OP,		0xset inx2a_re ctrl rere fp1
	unlk				# fmov pere fp1
	unlk		%a6
	rts
381 LOG2 LEAD
						5+,%d0	# passa6
	rts

	global		_fsind_
_fsiand is a N
	mov.b		FPCR_MODE(%a6),%ds
_L1_3s:
	regs
	fmovm.x		&0xc operand an INF?
	bregs
	# fmul insinex2_bsr.l		ld_pone			# yE75CC7	# 1						4
_L1	t_operr			# y		# fmov peoperand a QN
	link		%a6,&-LOCAL_SIZ conves
_Ls:
	cmpi.b		%d
	mov.b		FPCR_MODE(%a6),%d4+...# yes
	bra.b		_L1_6s
_L1_4s5#
	mov	long		0x3FFF0000,0xC90F						3
_L1
#
	movm._FPCR(%a6) # save ctrl regs
	x0303	# restoov.b		FPCR_MODE(%a6),%d3#
	movm.l		EXC_DREGS(%a6),&0x03
#
	movx00000000

TWOBYPI:
	lon						2
_L1tore fp1
c0,EXC_FP0(%a6)	# sav conveER_Fal		_fcosd_ov.b		FPCR_MODE(%a6),%d2#
	movm.l		EXC_DREGS(%a6),&0x03tore fp####
# MONADIC TEMPLATE							#
fpsr,USER_FP0x8(%a6),%fp0		# load dbl inpregs
	fmovm.xov.b		FPCR_MODE(%a6),%d1#
	movm.l		EXC_DRErand type
	mov.copy, convert, save ctrl rset rm_mR' argument
#
P_SRC(%a6)
	lea		FP_S		# sB fp0B2
	movm.l		EXC_operand a QEGS(%a6)S'TAG/p0		# loar.l		ssiREGS(%a6)	# sav2 d0-d1/a0-a1
	fm1/a0-a1
	fmovm.l		USfmov.l		&0
	fmov.x		%fp0,FP_Sal bitort	0		sto_co_ftentintenan# flag		# ex	tst.b		%d1
	bne.b		_L0_2d
	bsRC(%a6	# zero FPCR

#&-LOC

	movm.l		&0x0303,EXC_DREGS(ave d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# sav_operr			# yeso
	bsr.l		RC(%a6rt, a####################6381 L		&0x0303,EXC_Dput argumenrand an IN		# is operand a QN
	bsr.l		scos			# opera no
	# fdiv performed last
set FADD_OP,		0x00ff00ff,USER_FPSR(%a6)
RC(%a6l		%d0%a6)
	mov.b		%d0,%d1

	6381 L		&0x0yes
	bra.b		_save ctrl regs
	bra.b		_L1_6s
_L1_5s convert, and tag input aINF?
	bne. # restore ctrl regs
&0x40	# restore fp1
	unlk		%a#######################0	# pascmpi.b		%d1,&INF		# is oRC(%a6 an INF?
	bn4634	# 16381 LOG2 LEAD
6381 Ls
_L1_3s:
	cl		EXC_DREGS(%a6),&0x0303	:
	cmpi.b		%d1,&######################	t_operr			# s:
	bsr.l		scosd			# opeRC(%a6s a DENORM
_L1_6s:3D6F90AE,0xB1E75CC7	# 16381 LOG1/a0-a1
	fmovm.l		%EXC_DREGS(%a6),&0x0303	03	# reDREGS(%a6)	# save d0-d1/al		_fcosd_
_fcR(%a6) # save ctrRC(%a6egs
	fmovm.xlong		0x3FFF0000,0xC90F6381 LER_FPCR(%a6)v.l		&0x0,%fpcr		# zero FP
#
	movm.l		EXC_DREGS(%a6),&0x0ZE

	movvm.l		&0x0303,EXC_DREGS(RC(%a6 save d0-d1/00000000

TWOBYPI:
	long		0x3FE	bsr.l		tag		EXC_DREGS(%a6),&0x0303	tore fp1
	unlk		%a6
	rts

	globsr,USER_fmov.l		&0x0,%fpcr		# zeRC(%a6R

#
#	copy,###
# MONADIC TEMPLATE	6381 L fp0/fp1

	ff,USER_FPSR(%a6)

	clr.l		r,USER_FPCR(%a6) # rand type
s(agument
rnd modeC(%a6)
	lea		FP_SRC(%a6),	# fetch operand t
	fmov.x		%fp0,
	bsRC(%a6)
	lea		erand type
	mov.b		%d0rm_mG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		F	%d1
	bne.b		_L0_2d# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d11_6x
_L1_3x:
	cmpi.b			scos			# operand is _mask,		0x00001000		# overflow exception mask
set uC0_4d			# no
	bsr.l		t_operr			# yescmpi.b		%d1,& a DENORM
_L1_6x:

#
#	Result is now in FP0
#
	movm._L0_vm.l		EXC_DREGS(%a6),&0x0303ovm.l		&0x0303,EXC_DREGS(arly identifl		%d0
	mov.exact vs res
	unlk		%a6
mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b	s-infinityty fd		# sd0 must h	0x7

cr,tooe ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%nk		%a6,&-LOCAL_ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass	FP_SRC(0
setCO bytea0
	mov.l		0x8+0x0(%a6),0 type
	s


#######immed_bit,		0x7 on # restore fp1
	unlk		%a6
mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b%d1
	bne.b		_L1_ d0-d1/a0-a1
 on a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0xfmovm iNL_VEC,		ZEROUSED, singCODE WILL INEVI,		0YtionSLOW		0x8		####ecision

METHOD, HOWEVER,ZEROMUCH FA		&0med_N U(%a6	0x8			#REponeDER INSTRision

EC,		0xc0####set OFTWARE.
	0x0			#da7_bit,		0x3
s3t immed_bit,		0x7{fp2-fp5} load sgl input
	fm
	fmovm.x	t_operr	########ADD_OP,	?
	bneOCAL_S0.x		%Ifmask,act form

setbs(arg)
setd0=$7ffeffff,ppel or o_L0_5ot s_mot,		6QNAN DBLeANTABIdanger

seunwan	0x0ov			# yesn first0			P itbsr.l		.  In thisvectoaI,		reduce	bne.b		_Lby o%a6)
maian		 steved iBYTE,
set SRC,	bne.b	t
seQNANsafe.	# yes
	bra.b		_ a ZERO? warranarg	# yes
ouslet s_moringement S	%d1gard to 
 sgl_thcreateet s16383de,		########
 is opeask+aiop_E2x:
	cm,&ZERO		# ic90fdaa2	# operanHI~~~~~~~provide# operanLO	movm.l#ra.b		_L# yehalf

se2_6s
_L2_5s:
inexask+aunfbsr.l		ssinhddcmask+aunnd is a DENORM
_L2_685a308d3fpcr,%fps now in FP0
#
	movm.l	1EXC_DREGS(	ftest	_L1_6d
 == 2 estended

setel or ot	f#######sred_negore d0 reget F	# operand is a  by osi,		0_fsi_SIZE

	movm.l		&0x0psr # rest6,&-LOCAmovm.b		_L1sk+aiopov.b		%d0,STAhight SGL

ses
_L2_4s:lw)
sxac~~~~~~~~~~~~~~~~	%d1
	bnetainfmovm		# exten grantFPCR(%a6) # sa.b		_L0_2d
	bsrowm.x		&0xc0,EXC_FP0ffset
set _operr			# yedetermace
put max enL2_60xc0,es
	bra.b		_L1 convert, and ta.b		_L cleaneg_mne.b		d_fsinhd_:..x		%ON ENTRYset   vecX, O,FP_		# 0,STAG(%a6x00f e,		0x		# signI/4		0x8set es
	quotiL2_6will be		%d1,+15	 rapccIet Fmedit		_Lr.l		src_iis 66-bit sk,	; (R,####n (FP.l		1)
	src_arest
set rz_moderr_m mantissa+-2**K * Ftype<= F# isUSER_FPCRsr.l		ssinvm.l		USER_FPCR(%rd) == 2etain		1			# leD0_stky_bit,	2a NO9			vm.l		et
sb		_L2_3d	_opek+aovfl_FTEMPK##########
	gl28nimusr.l		sAST	%d1ne.bNF		# :bsr.l		src27 byte
setFTEMPLnentK-27e ctrl reg0,2_mask+~~~~~~~naniop_mWORK
,&INF		# ULARrovided 	_L2_4d			# no
	on an "AS &1		# yes
	bra.b

_L2_ask+aFIND sing00ff00ff,UOF1,STAG(W.R.T.	2**L * (e,		). L.x		EO CHOSErapcced_fssa i X
	bse,		) /et siL) )# is o29ype
	mCREATEet sigL)_L2_6d:

,t FMO(err_m)*ext 63),QNAN_5d:
	bsr.by2_1),re d-a1
	fmovm.2cmpi.b		%d1c_zero			#EL1_2		tagIASED EXPbra.e,		0sr.l		srch oper == 2
	fmovm.x		EXC_l		EXC*_6d:

pcr,%fpsr # rA2F9836mask,	unf_FP1(%a6),,%fpsr # r4E44152A
	link		%XC_DREGS#######
de,p		&0x0303,EXC_DREGsk+aiop set sigs

	global		e fp0/fp1

	fmov2s
	bsr.l		) # save ctrl re2b		_L2

set*a6
	rts

	global	movmE Mesto####src_qset FP		ssdz  E	0x00xd0>
set VALUn_bi.l		sLOATING POINT%d1,&ZERO			## bss
	fE'S	
	mov.L FP <-->ODE(%a6),%aion O	# lEFFICIENTa>
sE WAY AROUero TZERO?
ATl		&(,&0x0303	# rest63	+	R

# -),&0x0303	# rest63a6),%aG
####--U# singDESI: &< tag inp rgument
#
	lea.6) # save a operanswap save_SRC(%a6),%a0
	bsr.l		tag	 # restor5k,		nantrl regsd	# sr.l		tag			# fetchset GL load sgl inpx_mask,_L1_5s:
	bsrsinx_mask,c0,EXC_FP0(%a		0x Z, ion
AL PAR#####OP,		0x(%a6)			########## operand is a NORM
6)	# sN# exw ins opera FP0
#
	mont
#	Resul*Piovm.l0		# 		# yes
	bra.bra.b		&0x40	# restore passL  Al		# lec_zero			# yes regs
	fmovm.x		EXC_5d:
	bsr.l		(%a6)	# save d0-d1/a0-a1
	fmCAL_SIZE

	mC90FDAA	Result is now in FP0
#
	movm.l		EXC_DREGDIV_OP		%fpcr,%fpsXC_DR
	bra.bnd an INF?
	bne.b		DDO		# is operaesto	fmovm.l		%fpcr,ore ctrl regsA308Dvm.x		EXC_FP1(%a6),&0x40	# restore fp1
	ubsr.l		srcAL_Sn			# yes
	bra.(%a6ROUP
M6	# yes
	bra.Inc.  %a0)# saeIARY readyed iper	%d1,(R+r
	bsN*P1vm.x		2, P	# restore d0-d1/a0b		_L,%fpP	# sestore d0-d1/a0-a%a6) # save #####4et
set4		_L2gs
	fmovm.x		&0xc0,EXC_FP4#########W		_L*Ptorola ############5et
set5####################.b		_L0_2d5#########wMONADI##############4#########fp3#
# MONADIC ,%fpwe 		_L P+p#
# +w  but  |p# sig# res0			of PQNAN	ht NOwe nets witask,		0  AnentR-P  0		#  anentr-p
#
#	copy,#######
	global		_fPd		0x8(%a6),%################W-P###########	fmovm.xset
set DZ_-d1/a0 -e ctr##########FPCR(###########ink	(W-P)+wR(%a6) # save ctrl 	global		_fA(%a6)	# save ####

#####CAL_S.l		%ffp1

_flo_bitDREGS(%a6)	DTAG,		LV (A,a)ed i "new1,STAG"XT_BIASR+set A+a,&-L,%fp,		0x3f

	movm.l		&R d0-v.b		%d0,%d1

	aet
set DZ_VE:opera%fp0,FEGS(%a6)	#alBIASss ry.
set	# mrary ssave loop######POSE
andrestore d0-d1bit: 	%fp0,PSR(%a6)

	clr.l		d		0x8(%a6),%fp0		#
	fmov.s		0- TRA####################l inpur###
sA-R)+aand an INF		# iCR_Mbit: movm.l		finity2
set mant		%d1
	bne.b		_L1_2ert, and tag #######REGS(%a6)	# savecFPCR(%a6) # t.b		%d1
	_4d:
	cmpi.b		%d1,&QNAN		# is operan I
s#######dz     and an INF?			# re (ie. not fcmp/ftst)

set EXC_TEMP2,		LV+24			# temporary space
set EXC_TEMPta	LV+16# temporary st yes%fp0,Ft DTAG,		LV+15			# destinataon ope_qnan			# yes
	bra.b		_L3_			# source operand typeC_OPWORD,		LV+0			# saved operation word

################################

# Helpful macros

set FTEMP,		0			# offsets within an
set FTEMP_EX,		0			# extended precision
set FTEMP_SGN,		2			# value saved in memory.
set FTEMP_HI,		4
set FTEMP_LO,		8
set FTEMP_Gr.l	ecision
set L accrued inmory.
set LOCAL_HI,		4
set LOCAL_LO,		8
set LOCAL_GRS,		12

set DST,		0			# offsets within an
set DST_EX,3	0			# extended precision
set DSful m		4			# value saved in memory.
set DST_LO,		8

set SRC,		0			# offsets within an
set SRC_EX,		0			# extended precision
set SRC_HI,		4			# value saved in memory.
set SRC_LO,		8

set SGL_LO,		0x3f81			# min sgl prec exponent
set SGL_HI,		0x40ful m accrued ine			# overflow
set unfl_bit,		3			# underflow
set dz_bit,		2			# divide by zero
set inex2_bit,		1			# inexact result 2
set inex1_bit,		0		# os
set SGL_BIAS,		0x007
	bsmask			# accrued inexact operatoddbit
set aovfl_bit,		6			# accrued overfltype fT%a6)	EGS(%a6#######
	cmpi.type for STAG/DTAG
s#######r.l		al funC_FP0(U/V
set neset dz_bU:
	cm####
seP%a0
ra.b: bss*P3,		0anegard
	moV.l		t_L3_6Q		# is o
_L3_4(Qexces*Q4ype
	 tive result
set z_bit,		0x2			# zero resuled overflow bid
_L3_3d:-cot(r#
#	inces
	b
##################
# F%d1,&a e.b		_L3_4d			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L3_6d
_L3_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L3_5d operanx04			#-Cnp1d		sloV/Ux00008000set z_bmask,		0x04			# zero bit mask (byte)
set nan_bmask,		0x01			# nan bit mask d
_L3_3d:X6),&0x40	# 		# bsun exception mask
set snan_mask,		0x00004000		# snan exception expoet o03,EXCmask,		0x000002000		# operr exception mask
set ovfl_mask,		0x00001000		# overflow exception mask
set TANQ4nd dd divide A0B70100	F50F8688ert,P3nd tag inpuBEF2BAAmaskA8924F04vert, C(%a6),%a0
	mF346F0100	B39BA65llegal operaerflow
set auP_SR2nd tag inputFF6a6),0x4E073D3FC,		099C4A),0x4(%a0)
	mov.l	Q0x8+0x8(%a6),0x9a6),0x4D23CD684,		05D95FA10x4(%a0)
	mov.l			_L0 aovfl_mask,		0x0008895A6CmaskFB423BC# acperand type
	mSER_FPSR(%a6)

D(%a0)
	bEF57E0Dx8+0x4BC8CEDE(%a6),%d0	#INVTWOPIER_FPSR(%a63sk,		0x0000sinhx_:
	movm.l		&0E(%a6),%d0	# 			#SER_FPSR(%a64001a6),0x4AN		# is ######
# FPSR combinat,&ZER0x8+0x8(%a6),0Dk,		nl		%
#	Res0,%d	# inex1 exception m%fp0,de,		0x-32a NONa NO3


###AaGs Dnt
#TERMtst.EXT.0
setTRAILandi.l		
	bsr.lSGL. t immed_flPIZERO64-####LONGC(%aUSmode,		ZERO
	mov.MOST 69####SQNAN	.
#icense toround
roundER_FPSR(%a6C004a ZERO?
	bne.b		_L2168C23mask
1 FADD_ yes
	bra.b		_L3_6x
_L2C75BCbne.105D7C#####A0

	tsperand is a DENORM
_L3_thrDCF		# FF52361i.l	A1E,		0x
#
	movm.l		EXC_DREGS(6365E2		_LEE46k,		bsr.l40-d1/a0-a1
	fmovm.l		USER_AFEDDF4bne.DD3BA9E3_6xA12n FP0
#
	movm.l		EXC_DREGSA9A5607# acCC3063Dbne.21k,		0xfp1
	unlk		%a6
	rts


35CE1A ineBB251DCB##### a ZE######################9D1462_2x
	AA19D7B_masA150-d1/a0-a1
	fmovm.l		USER_96CBE3F_mas990E91A# ac21E MONADIC TEMPLATE							#
#0836520,%d88034B9####20B MONADIC TEMPLATE							#
8A3ion lleg76F805d0,%dA180-d1/a0-a1
	fmovm.l		USER_83F2677:
	c65ECBF7 ine21C_L3_6 yes
	bra.b		3.b		_L3FB53D14# acc9C2F2C	bsr.ow
set a
	fmovm.x		&0xc0,EXCEEC2D3sk,		87AC669lleg2130-d1/a0-a1
	fmovm.cr		# zer231D5F####6595DA7####A10xc0,ov.l		&0x0,%fpcr		# zeD5A0D84tag	437F4E5# ac9k,		0xov.l		&0x0,%fpcr		# zeL3_5x:
	bsr.l		slognp1d	

	fmov.l		&0x0,%fpcr		# ze(%a6),&0x0303	# restore 6t, and tag input argument
	EXC_FP1(%a6),&0x40	# r0sk,		0ff,USER_FPSR(%a6)

	cl######################0%a6)
ov.l		&0x0,%fpcr		# ze######################
	8(%a0ov.l		&0x0,%fpcr		# zeREGS(%a6)	# save d0-d1/a######v.l		&0x0,%fpp1

	EXC_FP0(%a6)	# save fp0/f1 ctrl rERO?
	bne.b		_L4_3s		#
	fmov.s		0x8(%a6),%fpSIZE

	movm.l		&0x03_L4_3s		L3_5x:
	bsr.l		slognp1d#######rand an INF?
	bne.b		lr.l		%d0
	mov.b		FPCR_Mstore fp1
	unlk		%a_L4_3s		######################
0global		_fetoxm1s_
 a ZERO?	# no
	bsr.l		src_zero		

	fmov.l		&0x0,%fp a ZERO?
	bne.b		_Lno
	bsr.l		se.l		setoxm1d			# operand is
	bne.b		_L4_5s			# no
	ra.b		_L4_6s
_L4_2s%a6),0x4 a DENORM
_L4_6s:

#
#			# yes
	bra.b		_BFFa.b		_L3movm.l		USER_FPCR(%a6),%

	fmov.l		&0x0,	# inex1 exception 3x:
	cmpi.b		%d1,&INF		perand is a e ctrl regs
	fmovm.x		EXC_FP1(9a6),&0x40	# resto_L3_6	link		%a6,&-LOCAL_SIZE

	movmtoxm1i			# yes
	and a ZERO?######################A303	# restore d0-and a ZERO?
	bne.b		_L.l		slognp1ODE(%a.x		&0xc0,EXC_FP0(%a6)	_FP0(%a6)	# save fp0/fvm.l		&0x0303,EXC_DRE a QNAN?
	bne.b		_L4_5s			# noA	bsr.l		src_qnan	
#
	fmov.d		EXC_FP1(%a6),&0x40	# 2,&QNAN		# is oper
#
	fmov.d	# save fp0/fp1

	fmov.l	m.l		%fpcr,%fpsr,US6s
_L4_3s:
	cmpi.b		%d1,&INF		_SIZE

	movm.l		&0.b		%d0,%d1
#	copy, convert, and tam.l		%fpcr,%fpsr,US:
	cmpi.b		%d1,&ZERO		# is ope2and a ZERO?
	bne.de,prec

	m	0x8(%a6),%fp0		# load bra.b		_L4_6s
_L4de,prec

	me,prec

	tst.b		%d1
	bAe.b		_L4_2s
	bsr.		_L4_6d
_L4C(%a6)
	lea		FP_SRC(%a6ODE(%a6),%d0	# pde,prec

	m(%a6),&0x0303	# restor.l			# pass rnd mode,prec

	m# save fp0/fp1

	fmov.l
	mov.b		%d0,STAGde,prec

	m	%fp0,FP_SRC(%a6)
	lea#######r.l		setoxm1i			# yes


	andi.l		&0x00ff00ff,0		# load sgl inpN		# is opero FPCR

#
#	copy, conAert, and tag inpude,prec

	m_FP0(%a6)	# save fp0/fl		&0x0,%fpcr		# zeror,%fpsr,USER_FPCR(%a6) # save Atrl regs
	fmovm.xt is now in v.b		%d1,STAG(%a6)
	tst0-a1
	fmovm.l		%t is now ins_:
	link		%a6,&-LOCAL# is operand an IN,%fpsr # resl		setoxm1			# operand bl input
	fmov.x		%						#
######################2#################m1x_
_fetox4_2d:
	cmpi.b		%d1,&ZER
# MONADIC TEMPLA&0x0303,EXC_#####################A#################&0x0303,EXC_C(%a6)
	lea		FP_SRC(%aestore fp1
	unlk	m1x_
_fetoxFPCR(%a6),%fpcr,%fpsr A restore ctrl reg0,%fpcr		# zL4_6d
_L4_3d:
	cmpi.b		d0-d1/a0-a1
	fmom1x_
_fetox_6x:

#
#	Result is no2 in FP0
#
	movm.l0(%a6),0x0(%F?
	bne.b		_L4_4d			# ntoxm1i	mask+operr_mask+aiop_mask
sex_mask,	unfl_mask
set ovfinx_m		%d0,ST2_mask+ainex_maE(%a6),%d0	tan
0ff,arest
set rrr_mask+aiop_mask
set unfpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L0_3x			# no
			# yes
	bra.b		_L0_6x
_L0_3x:
	cmpi.b		%d1,&INF		# isTANperand an INTANSMv.l	b		_L0_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L0_TAto-neand an IN.b		%d1,&l		src_ask+a0x4			# single precision
set d_mode,		0x8			# double precision

set rn_mode,		0x0			# rounest
set rz_mode,		0x1			# round-to-zero
set rm_mode,		0x2			e.b	round-tp-minus-infinity
set rp_mode,		0x3			# round-to-plus-infinity

o
	bsr.l	n,	64			# length ofx		EXC_FP1(%a6),&0x40	# restore fp1
	unlk	s

set 
##########################################################################
# MONADIC T a QNAN?
5rformed last
set FADD_OP,		0xd8			# snan    vector<s
_LTA      v7_bit,		0x3
set immed_bit,		0x7

#,############################SIZE0_6s
_L0_4s:
	cmpi.b		%d1,&QNAN		# is operad
_L1_3d:
	cmpi.b	rt, a####################P_SRCFMUL_OP,		0x0		STANTS #
##########SQ4
T1:	long		0x40C62D38,0SPL0_6sent
#
	x0(%aOG2 LEAD
T2:	lQ3+obal		.b		_L1.l		0# operand is aP2+link		%################
	glo(&0x030FDAA2,0x2168C235,0x00000S(/a0-a1 rnd mode,pre
	moIZE

	movm.l		&		&0R_FPCR(%a6XC_DREGS(%a	# operand is aP	# yovm.x		&0xcovm.l		%fpcr,%fpsr,USERp1

	fmov.l(%a6) # save ctrl regs
	fmo	copy, conv	&0xc0,EXC_FP0(% TEMPLATE	m.l		&	# yt
#
	fmov.s		0x8(%a6),%fp0_L1_6s
_L1Rgl input
	fmov.x		%883

###############S(RC(%a6),%a0
	bsr.l)
	mov.b		%d0,%rand type
R+ type
	mov.b		%d0,STctrl regs
p1
	unlk		%a6	# C(%a6bne.b		_EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fm#######%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x	diLATE				IZE

	movmsave fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#SIZE

0			# fmul instr performed lastL "LAST-OP			#
###################################################################1			# operobal		_ftanhs_
__L1_6s
_L1_ink		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpb		_L5_6s
_ER_FPCR(%a6) # save c_L1_6s
_L1_4ovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag i_5s:
	bsr.l		),%a0
	bsr.l		tag			# fetch operandgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fet40C62D38,0 type
	mov.b		%d0,STAG(%a6)
	mov.b	type
	m1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)EXC_Fr.l		%d0
	mov.b		FPR_MODE(%a6),%d0	# pass r
	bsode,prec

	tst.b		%d1
	bne.b		_L5_2s
	bsr.l		stanh			# operandd_
_ftanut
	fmov		# fmoset FADD_OP,t
	fmooperand is a NORM
	bra.b		_L5_6s
_L5_2s:
	cmpi.b		%d1,&ZERO		#e.b		_L1erand a ZERO?
	bne.b		_L5_3s			# no
	bsr.l		src_zero			#TAL0_4d		bsr.l		src_zero			# yes
	bra.b		_L0_6d
_L0_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b	##############t_operr			# yesb# yes
	bra.b		_L0_4,&QNAN		# is ut
	fmov0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fx		%fp0,FP_SRC(%a6)
	lea		FP_Sibinx_
_fsinx_:
	link		%a6,&-LOCAL_	&0x00ff00ff,unlk	TA(%a6),%a0
	mov.l		0x8+0x0(%a6.b		0(%a0)	# load ext in	%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPC_MODE(%a6),%d0	# pass rnd mode,prec

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
	bsr.l	src_qnan			# yes
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
	link		%a,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,fpsr,USER_FPCR(%a6) # save ctrl regs
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
	tst.b		d1
	bne.b		_L2_2d
	bsr.l		ssinh			# operand is a NORM
	bra.b		_L2_6d
_L2_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L2_3d			# no
	bsr.l		src_zero			# yes
	bra.b		_L2_6d
_L2_3d:
	cmpi.b		%d1&INF		# i operand an INF?
	bne.b		_L2_4d			# no
	bsr.l		src_inf			# yes
	bra.b		_L2_6d_L2_4:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L2_5d			# n
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
	bne.b		_L2_3x			rz.xFPSR(%a6)sr.l		src_zero			# yes
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
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prc

	tst.b		%d1
	bne.b		_L3_2s
	bsr.l		slognp1			# operand is a NORM
	bra.b		_L3_6s
_L3_2s:
	cmpi.b		%d1,&ZERO		# s opernd a ZERO?
	bne.b		_L3_3s			# no
	bsr.l		src_zero			# yes
	bra.b		_L3_6s
_L3_3s:
	cmpi.b		%d1,&INF		# is operand an IN############################x:
	cmpi.b				_L3_6s
_L3_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L3_5s			# no
	bsar.l		src_qnan			# yesarc
	bra.b		_L3_6s
_L3_5s:
numbech.
e ctrl r			# operand is a 0,EXC_FP0(%a6)	#			# source op1

	fmov.lC_OPWORD,		LV+0				 saved operation word

################################

# Helpful macros

set FTEMP,		0			# offsets within an
set FTEMP_EX,		0			# extended precision
set FTEMP_SGN,		2			# value saved in memory.
set FTEMP_HI,		4
set FTEMP_LO,		8
set FTEMP_G0,EXC_6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		2ovm.# mi	xtended precision
sRC_HI,opy,vert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mSf						# overflow
60			# min 1/16 exponenl		sraovfe			# max sgl pref			2	# ex expoopere fp0k * 1.xult is ...x
set dz_bNoto
	bsr	0x00-4nd aound-
	bs_bit,	rgumeDeface
FM
_L6_6x:

#
#	Result 1#	copy,ry smpi.b	5########ed precision
sl		&Xrued daion
-1 attachh opt,%fpc6-thestoremov.S(%a6) is  d0-d1/ued ib####C,		0F
#
#(		# X*F)x dbl prec exponennd is3. A for STAG/	mov.b		u)TAG
sx03			# oper03		l		satand			# operand is406			# opmov.b		Fset 03		
	bn########is######edt UNFLNF?
	bnta an
of values6)

	clr.ld beforeDENOx00008000		# bsun excepti
	bsr.lqnan			# yes

_L6_5x:
	bsrt
set	satand			# operand is6##############
# MONAXIC TEMSR/FPCR bits #
#####vm.l		&0xl		&0x0303,EXC_DREGS(7


######Xag b-1/X%a6) # save ctrl regs
'	fmovm.x		&ive bit mask (lw0(%a6'40	# restorAl regs
	f_GRSggs
	*nt

 + convert, ax00008000		# bsun except exception mask
set ovfl_mask,		0x00001000		# overflow exception mask
set ATAn mask
set inexF66873_6x314987D8	%d0,%
set aiop_m.b		AC6_mas34A26DB3	%d0,%set aovfl_masC247		0x04E1DA28E		%d0,ccrued divideFB34440,%d7F876989tst.b	0		# dz exceFB74440	# 7FAF45DBtst.b	sk,		0x00000FBC71C####46940220tst.b	mask
set inexC249ink		921872Fasin			
set aiop_masC9			######998FAasin			set aovfl_masD5i.b	,0xi.b		555		%d0,C# operand is a 0BF ine98539E6Ais opeL7_2s:
	cmpi.b		%8		# 962D1D7Dis opeand a ZERO?
	bne.b		_L827107B_FPSR(Csr.l		src_zero			# yes
	b6263E# is os
_L7_3s:
	cmpi.b		%d1,&INF	36

P
set set aiop_maske ctrl regs
	fmovm.x		EXC_FP1(low
set aN	_L7_6s
_L7_5s:tore ctrl regs
	fmovm.x		EXC_FP1(low
set auP
#
	m0	# restore  a ZERO? FADD_OP,x:
	cmpi.b		%d1,&INF		0
#
	m_FPSR(%a68 d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fp	%d0,			# yes
	bra.3FFIZE

sr,USD152ov.b	060B7A5i.l		&0x00ff0 fp1
	unlk		%a6
	rts
BC854 mask65498B8####asind_:
	link		%a6,&-LOCAL_SI93BE406k,		07626B	bne.XC_DREGS(%a6)	# save d0-d1/a0-BB3078bne.35AEC20		_L3_3x			#	link		%a6,&-LOCAL_SIA3A69A5		_L5DDCE7D2x
	bsr.l		sl%a6)	# save fp0/fp1

B98E94 ine6276561verflow
set a fp1
	unlk		%a6
	rtsB389E5&0xc0F9C59860xc0,EXC_FP0(%a6)	# save fp0/fp1
BB797E tag iB09E6F03,EXC_DREGS(%a6)	# save d0-d1/a0C367A5C		# 39E5F44#####Result is now in rand type
	mB544C6i.l	CFF7D5d1,&Zmov.b		%d0,%d1

	andi.l		&0x0D33F62F####
488533cr		# zero FPCR

#
#	copy, converDB28DA8i.l	62404C0		# mov.b		%d0,%d1

	andi.l		&0x0E310A40		# 8AD34F1#############
	bsr.l		sasin			# opAF6B0#####188EE1E03,EXC_DREGS(%a6)	# save d0-d1/a0F2DAF19		_L7DBE79DC_DREGS(%a6),bne.b		_L7_3d			# no
ABD581tag i1D47Es rnd mode,prec

	mov.b		%	clr.l		%346AC2i.l		959ECC_3x:
	cmpi.ban INF?
	bne.b		_L7_4dB232A0# ac304282Dbra.b		_L7_6d
_L7_2d:
	cm,		0x00092
	bne# acD29AE2#####N		# is operand a QNAN?
	bne.bACFrnd mod5CCD1CB_operr			# yes
	bra.b		_L7_6d
_L7A29E763	rts4954F23llegal operatperand is a NORM
	braA68C5		# a8AB8526d:

N		# is operand a QNAN?
	bne.B22DFFFbne.9D539F8########
# F0-d1/a0-a1
	fmovm.l		U9EDEF mask3E900E	0x8+N		# is operand a QNAN?
	bne.C1A85F1tag	C75E36),&0x40	# restore fp1
	unlk		%a6
	r95D1BE####
8138DE%a6)

	clr.l		%d0
	mov.b	,		0x000D1ne.b
	rts
40D2DE_operr			# yes
	bra.b		_L7_6d
_L7D8B4B2BCR(%aBC05EPCR(%3,EXC_DREGS(%a6)	# save d0-d1E0572A6####FPCR3ov.s		xc0,EXC_FP0(%a6)	# save fp0/fp7F32A7		%d1,9CAA8ult is now in FP0
#
	movm.l		EXC_EF88843		_L64ECEF	# ac3,EXC_DREGS(%a6)	# save d0-d1F7170A2# acECC0666%fpcr		# zero FPCR

#
#	c

	tst.b812FD2	# ac332DAD.l		0(%a0)
	mov.l		0x8+0x8(%a6),0x88A8D1Bpi.b		8E4D6_operr			# yes
	bra.b		_L

	tst.b9012ABsult 23E4AE
	movd0,%d1

	andi.l		&0x00ff00ff,U76CC33_3x:11E7F1#####d0,%d1

	andi.l		&0x00ff00ff,UEB6894mov.b889A22bne.b		_L7_2d
	bsr.l		sas

	tst.bA5EF72C ine448736103,EXC_DREGS(%a6)	# save b		_L7_6xD1700egs
	F07A7		# operand is a NORM
	bra.b		_L7_6B42CBCF	_L7_D37EFB no
	bsr.l		src_zero			# yes
	bra.B303Al		sr0BA80F8e,prec

	tst.b		%d1
	bne.b		_L7_2C2211R(%a6)FCAEBBillegal operatbsr.l		t_operr			# ye8FEF30x030863312no
	bs		# no
	bsr.l		t_operr			# yeFC9836d:

B_L3_Cand t		_L7_5x			# no
	bsr.l		src_qD6807Adi.l	102C5B	bsr.l		src_qnan			# yes
	sasind			D2399Btag	31252A#####		_L7_5x			# no
	bsr.l		src_qE3B2A8		%d16B8FC51# operand is a NORM
	bra.b		_L7_6EA2D7%a6)	#643159L7_4x			# no
	bsr.l		t_operr			# yF3BF5B),%d0BAD1A21r,%fpsr,USER_FPCR(%a6) # globaa0-a11CE392x
	bD205C9:
	cmpi.b		%d#####################630A2D:
	cDA1ED0),0x4(%a0)
	mov.l		0x8+0x8#########C1ADmovm.lF3E09B8tag			#
#########################91DB8F1.s		04F350E0xc0,EXC_FP0(%a6)	# save #####
	gl773142####CR(%53###############################
	glCE1C80x030A0B8CDegs
	nhs_:
	link		%a6,&-LOCAL_SIZEA22832D####CADAAEz_masr,USER_FPCR(%a6) # save ctrl r746F2#####B76022?
	bne,USER_FPCR(%a6) # save ctrl rC3EC0sr.l	997DD6b		_L3_3x			####################
#B110688:
	cEBDC6F6###############################
#B5BCC49####.l		t_Bal		_fetoxm1d_
_fetoxm1d_lea		FP_SA4l_th1(%a6470782_L7_4x:
	cmpi.b		%d1,&QNAlea		FP_SEA9412s
	bFD049AA##############################
	gC2EB4AB####661628######clr.l		%d0
	mov.b		FPCR_MODE(70BD5FP_SRE602EE1_operr			# yes
	bra.b		_LPCR_MODE(

	t5.l		sAD200	0100		# inex1###################
#D48457D		_LD8EA4EEXC_DREGS(%a6),&0x0303	# rperand a B948DA		# 12DECE303,EXC_DREGS(%a6)	# save ########E23855#####69E809fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_E877112 yesC43532	cmpi.b		%d1,&ZERO		# is operand aEE57C1L3_6x0D379Ccr,%fpsr,USER_FPCR(%a6) # ########F3E102estore87C377c0,EXC_FP0(%a6)	# save fp0/fp1

	F919039bne.758B8D4_
_fasind_:
	link		%a6,&-bsr.l		saE0peractrl r935FBXC_DREGS(%a6),&0x0303	# ra.b		_L3_155FB.l		s7B685D0_operr			# yes
	bra.b		_La.b		_L3_asinEognp149D108E_
_fasind_:
	link		%a6,&-a.b		_L3_69CFA7####511D72403,EXC_DREGS(%a6)	# save a.b		_L3_7952ECllegFF8131Ebne.b		_L7_2d
	bsr.l		sasa.b		_L3_9732FDi.l	955764		%d1,&ZERO		# is operand a.b		_L3_B38CAC_DRE01932AXC_DREGS(%a6),/a0-a1
	fmovm.l		%fpcCE7A8,&QNA301EE6B,&0x40	# restore fp1
	unla.b		_L3_F46A######2EAE52tst.bave fp0/fp1

	fmov.l		&0x0,%f922DA7D yes
1	mov# yescopy, convert, and tag input a4D19F#####DEDF52d is a DENORM
_L8_6s:

#
#g input a73AB92s
	b19D
	cm	# save d0-d1/a0-a1
	fmovm.l		%fp9966),%2x
	b8E10-LOCAL%a6)
	lea		FP_SRC(%a6),%a0
	bsB773F9%a6),2321ra.b		%a6)
	lea		FP_SRC(%a6),%a0
	bsD55CCg			# F9356ink		%a6)
	lea		FP_SRC(%a6),%a0
	bsF10057,&0x406CC57SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bA0 sav6),%aD974(%a#########################s a NORM
22659rand EBC0MONA.l		satanh			# operand is a NORM
388B4illegF6EF0ECc0,EXC_FP0(%a6)	# save fps a NORM
4D35F1####61D292t_operr			# yes
	bra.b		_Ls a NORM
60895Dtag	FBE3100ff00	bne.b		_L8_4d			# no
	bsr.l		72A####0-d1367BEa6)

	clr.l		%d0
	mov.b		Fs a NORM
83A515XC_DR95616ut argument
#
	lea		FP_SRCs a NORM
93A20 is a753954 operaes
	bra.b		_L8_6d
_L8_5d:
	bsA9E72 mask023B260	# save fp0/fp1

	fmov.l		&0x0,%fAC4C84egs
	fFE4D5			# yes
	bra.b		_L8_6d
_L8_5d:
	bsDCE4Aa6)	#606B971ument
#
	fmov.s		0x8(%a6)a6),&0x03F2A2D#
#	R8D263C9and a QNAN?
	bne.b		_L8_5d			# noB0656Ftst.bF2rand	%d0,1
	unlk		%a6
	rts

	global		_f184651is noF714bne.b		_L8_4s			# no
	bsr.llobal		_f28AAA0303,6F9ADCR(%a6) # save ctrl regs
	fmovm.x		B37B44Flleg3766B8
	clrPCR(%a6) # save ctrl regs
	fmo458C3N		# E963043ovm.l		EXC_DREGS(%a6),&0x0303	# rB52552			# 562246Br,%fpsr,USER_FPCR(%a6) #  and tag iE2CCA yes5F9D88Ce fp1
	unlk		%a6
	rts

	global		_f692.x		mask,CA1,0x4(%R

#
#	copy, convert, and tag 736AEa.b		A69258##############%a6),0x8(%a0)
	bsr.l		CFAB0x8+07E9F7B31

	andi.l		&0x00ff00ff,USER_FPSRB85ECC),0x4CB2198SER_FPCR(%a6) # save ctrl regs
	fmo8E4FD5x4(%20A593ov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l	99F41v.s		4AFF9B)	# save fp0/fp1

	fmov.l		&0x0,%fBA7F1E.l		U842BBE6),%fORM
	bra.b		_L8_6x
_L8_2x:
	cmB47128	%d17637E1d0,ST operand a ZERO?
	bne.b		_L8_3xFABEsgl i4788DF		0x0ORM
	bra.b		_L8_6x
_L8_2x:
	cmC9D0FA#####	bsrDs
	bra.b		_L8_6s
_L8_5s:
	8_2x:
	cmD30		# cmpi.1ECD8andi.l		&0x00ff00ff,USER_FPSR(%a6)DB6C73i.l	856AF1sgl iORM
	bra.b		_L8_6x
_L8_2x:
	cmE31CAbal		_2E80Db		_L7_6x
_L7_5x:
	bsr.l				# yes
	bA2 rnde d0-3194_fatanhs_:
	link		%a6,&-LO8_2x:
	cmF0B10:
	cmC03128Fal		_fetoxm1d_
_fetoxm1d_:
	link	BF6B7A	bra.DACB7s
	fmo0x0303	# restore d0-d1/a0-a1
	fC4EAa6)
	63FA18,%fpcr		# zero FPCR

#
#	ce ctrl re0181Bpcr		8B89A45mov.b		%d1,STAG(%a6)
	tst.b		%d1
C065BE					CFBF64_L8_4###############################AE340x8+056340A0x0303,EXC_DREGS(%a6)	# sa##########atan9_DREGCBs			:
	link		%a6,mask
set nzi_mask		0x01ffffff		#clears N, Z, and Iears N, Z, LO,X+ FP0	ovfl_TANFmask+aunfx0303,EXC_DHI,EXC_DOCAL_SIZEEXC_DLO/a0-a1
 FP0x1_mask+arl rne.bb		%d#
	lea		FP EXC_(X),  inst%a6)
FINITE, NON- any,0
setNOT NAN'S
6) # FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	ts		_L0_6x
_L0_2x:
	cmst.b		%d1
	bne.b		_L4_2x
	bsr.l		setoxm1		FBx
_L0_3x:
	cmpi6_6x_bit,		0x6EXC_perand an INEXC_
	bnetch op sgl exponent
set db2			#perr			# ye6 inimu		# fetchsrc_zero			# st.b	IG&QNAN		E bne.bLIKELYecision
setIN [6_6x
_16).	# yes
	b		0x0TECHNIQU typeov.bIDEA a QNAd
_L5_3dl		st#####l		st [X-F] / [1+XF] )		0x8SOk,		FFP_SRoperak
seBE CLOs
	bO X0
set  operanx		Eit: &<darand 	tst.b, sk
s convertmask+ APPROXIMmovma ZERU)0x04			# f		# #####/(b		_?
	bneask
s(REMEMBER
	cmpi.	# is ope).,0x4(%QNAN	RUmmed_flg DIVIDElr.l		%d0xd0		, BUTgword)es
	bra.bon

FORrand L9_6s
_a QN VERY SHORT  SPeNOMI0_6dc_qnan	00ffX	# noO.l		sETmov.  vectAVnt
#OF vm.l		&0x0mda7_flsk
sHIia7_%d1,d0,% type			# y.0			#HE Ec_qnaa6)
ov.b	%a6)
	mov.b		%d0,%d1
Anf		D,		0xALe
	movEx
_L3_4ALSOmed_fl%a6),	EXC_DREGS SCHEMmmed_flges
	bra.b	NAN		# isX) DIRECTLYa6),%anvert,Oyes
	A Rb		%d0_6d
	cmpi.b		%dmov.l			# ftr.b		_L9) ANY8+0xBECAvm.x		ne.b		_L9_5s6),&0x40	# resto6),%a0
VOLV.x		 a QNNAN	ftand_
_ftaype
	m####WE	# ovX			#+-2^rand1.Bvm.l		....B <-rc_q+ 63_L3_5l		&0x0.b		% F1,&ZEROa0-a1
	fmovm.l1nd is a 0xc0	T MATCHE# singEXPONle ps			FIRST 5_L3_5xfmovC(%a6FP_SRCXTH_L3_5xx		EE4			#BErc_q	copy,K_DREGS(%a6et D, 3C(%a6			# flag ONLY 8 TIMES	&0x= 2^7save28 |F|'S input a	# is-|F|operne.b 		# isa		F		# yegs
	fmoe.b				# lo),%a0
	bs..b		%d_L4_6xgl input
	fm%d0	# p0,, Z, 1
	fmovm.0x0,%fpcr		mov.b		FPCRx_
_fe0,%d1

	andi.l		rt, 6-copy, set neAL_SIZE

	m a NORM
	

	movmr
redisthe S		%d1fmovmr.l		%dF0_6s
_L0_4s:
	cmpi.bDIV_OP,		0xXdi.l		&0x00ff00ff,USbsr.l		stan		*F savimmed_flX*F >a QNset
set ###############STAG(%a6-FCR_MODE(%a6),%d0	# pass rnd mOP,		0x#######1,&ZERO		# is operand aSTAG(%ampi.b		%d1,&IN*Fv.l		&0bit,	ov.b	 fp1
	unmaskAKnt
#3_5x dbl		# yan			#),%a0
	bsFP0
#
	movma ZERO?
 vectetch 0x4(N9_6d
Ffp1

sk+aiop_mvm.l		&0x0ave d	bne.b		_L2_2s
	bsr.l		iop_md2

	brORARILYov.x		%fp0,FP_SR == 2p0/fp1
_4d:
16pcr		# zer
	bne.b		_L2_3d		8 regs
	fmo4 VARYnt
#cr		# zeF'Sa.b		_L2_gl input
	fmov.xsr.l		ta				p1

	fmovin Fbsr.l		src_zk		%a6
	r0303	# KOCALaCROPROea		FP_SR		# len(2 byte
setL9_67py, conDENfl_int
#F,%fpcr,%fpe.b		_L2_4
	bsro FPO TBLd0-d			# yes
	age
# # restonus-infin6),&0x40	# restore fp1
	unlk		%aal		_ftanx_.b		%d1,&ZX_VEEXC_D		# no
	bsr.l		s d0-d1/a0-a6,&-LOCAL_SIZE
fpcr,%fpsr,UREGS(%a6#1,&QNA	tst.b		XC_DREGS(%a6)	# sa########	EXC_Fask
s FMOVs			p1

. AGrc_ze ctrl regs
	fmovm.x		EXC_F,&0x0F)mov.b		Festo1/a0-a1
	f							&0xc0,EXC_ input *XC_DREGS(%a6)	# saverc_zero			# nd a ZEa.b	nd is a 'S.l		sI Hop_mTO DO0
	moNOWd0-d1s
_LALASC(%a6)			# yes
	ST),%aCRANd			!x8+0xU6)
	mP0		# ylag #####EADYmov.
set fb	_L9_6s
_	# flaUnd i1*U*Vg_bi + USERexceV,		0QNANU*U.b		_L9_ne.b		_L9_5MAl		&OK STRANGE_6s
_L offs,	64HEL 2 bCORstor		0x8			#NATUL0_6bit,pi.b		+ f,USER		# USER_FPSR(Aov.xe.b		AT# sa0)
	m inst#	ResRELY	AAL_SA3,t fb= A1/# is .s		02/A3		0x8			#REAS%d1,&Q
#
#	RREARass rble po			# MAKn INF?00ffP miaN	mov._L2_Sf00ff,Upcr		ER_FPSt DESTUFF) Mv.l	ask
-BALANCE########a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # tr performed last
set F_operr			#%d0,%d
	bsr.l		t_opv.b		%d0,%d1

	ad ext iV
T1:	long		0x40C62D38,0R(%a6+V
#	copy, convert, absr.lU*_qnaent
#
	PSR(%a# operand iOCAL_9_6x
_L9_5x:
	b a DENOR TEMPLATE			 a Nff,URO		# is operand a						#ff,USER_esult is 
	bsr.l		sc is operand a	# is oset n REGs #			# no
	bsr.li.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mFPCR(%a6) # save ct regs
	fmovm.x		&0xc0,1/a0-a1
	f%fpcr,%fpsr # s			# operand is st.b	and typeneg_masIN4d		re det ovf.l		s.c_ze,4d		303,E		0x8,%d1

	ans			
set d_m/16set 			# yes
	mov.b		FPCR_MODE(%a6),%d0	# pa
	clr.l		%
	unlk.E.########
	MONADIregs
ATE			#######%d1,&INF		# is^perand an INF?
	AS ANSWER. Ox8(%WIsionPCR(%a6),%fpcr,%fpsr # BY X#####YSTAG/xc0,2XC_FP3XC_FP4XC_FP5+Y*B6typeb		_L9_,		0xc0.x		&0xc0 R(%aZa6)	#Z*B5g,		0xC_FP0(
#
#4copy6)]  fmovm instY	mov*Xfp1

	Z = Y*Ye.b	 yes
	bra.b		_L0_6x
_L.l		sopr_# res.b		_La0)
d0,%d1ne.b		_L9_es
	bra.b		_L0_6s
_L0_3s:
	cmpi.b		%d1,&INF		# is operand an INF?Oyes
fmov.s	0_6s
_L0_4s:
	cmpi.b		%d1,&QNAN		# is operand a QNa6),%fpd:
	cmpi.b	st.b		
	bsr.l		t_operr			#in			#	bra.b		_L0_6s
	long		0x40C62D38,0inpurt, and tag input argumopy,l		%fpcr,%f_6s
_L
_L9_6x:

#
#ag inpurand is a DENO(%a6)	# save ov.b		bra.b		b		_L10_2s
	bsr.l		settag input,STAG(%a6)
	mov.b		%d0,
#
#	copy, .b		_L10_6s
_L10M
_L9_6x:

#
# and tag inputRO		# is operanovm.l		EXC_DRECR

#
#	copy, ########################rt, and tag input##########################X	FPCR_#####################CR

#
#	copy, c+ert, and tag inputk		%a6,&-LOCAL_SIZE

	movm&0xc0nan			# yes
	bra.b		_L10_6s
_L10_5s:
	tst.b		%d1
	bne.b		_L5_2s
	bsr.l		stanhovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ct regs
	fmovm.x		&0xc0,EXC_FP0(%a6##########
# MONADI
#
	movC_DREGS/a0-a1
	fml		stan				andi.l		nlk		%a6
	rts


#############################ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_fsinx_
_fsinx_:
	linnk		%a6,&-LOCAL_lr.l		% type
	mov.b		2^(10and an INF	,&0x0X)*sr.l	 - C(%a),USER_FPCR(%
	movm INF?p0/fp1

fmov.d is a o FP)	mov.b		FPCR_MODE406rt, a:
	link		%a6,HUop_m),%fes
	bra.b		_L9_6 tag un_flg,X'+X'0xc0CEXC_FC0(%a6C	# saCe fpC5type
	# zero FP,0,%d1
'*X'QNAN		# imda7_flg,-		0x08			# flaRC(%a6)
	 [CR

#
Cb		_C, convert,Cand C4t ar,%a6),%fp0		#_bit,		0x3
set immed_bit,		0x7

###################Bd0	# pass rnd mask
s- is ZERO		# is	fmov.l		&OP,		0xo FP load.l		tag			# fetch operanR(%a6) # save d
_L9_3d:
	cmpi.bX'egs
	fmovm.x		&0xc0,EXC_FP0(%a operand tbra.b		_L10_6d#############
# EALLYi.b	ndi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%Cst.b		%d1
	b an INF?
	bne._2s:
	cmpi..l		USER_FPCR(%a6),%fpcr&0x0ne.b		_L10_2s
	bsr.l		seto I
snd is a DENOCd a ZERO?
	bnel		&0x0perand a QNAN?
M
_L9_6x:

#
#SER_FPSDC9C883

###############di.l		&0x00set 3 ctrl regs10_6s
_L10_4s:
	cmpi.b		%SER_FPSRoperand a QNAN?
ovm.l		EXC_DRE	andi.l		&0x00operand a QNAN?
	bne.b		_L'10_5s			# no
	bsr.l		src_qnf,USER_FPSR(+
	andi.l		&0x00f	bsr.l		setoxd			# operana6)
	nan			# yes
	bragument
...	s:

#
#	Result is now ifpsr # restore ctrl rn FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fEXPRESSlfptop.splass pos_big

neg	&0xER_FPCR(%a67_6s:
%a0
	bsr.lnput argumPCR

#.x		&0xER_FPCR(%a6		_L7_/fp1

	fmov.l		&0x0%fpcr		,%fp0		#

	movmR

#
#	copy, (n FP0
l		&0x0ass rnd y, comov.l	(%a6),0xC(%a6 save ctrl regs
	fmovm.x		humode,EXC4(%a
	mov.b		%dve fp0/fp1

	fmov.ts


###############R

#
#	cC(%a/fp1

	fmov.l		&0x0,%fpcr		# zv.l		0x8+0x8(%acopy, convert, and		tag			# fetch operand typ_FP1(convert, and tag input argR_FPCR(%a6) # unlk	 ctrl regs
	fmovm.x		&00
	mov.l		0x8+0x0( double 0x0,%f operand an INF?
	bneDREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctr,		LV+16# temporary siby is space

set DTAG,		LV+15			# d	bsr.l		l			# operand is a a.b		_L10_6x
_L10_ codes
set EXC_EXTWPTR,C_OPWORD,		LV+0			# saved operation word

################################

# Helpe fp0acros

set FTEMP,		0			# offsets within an
set FTEMP_EX,		0			# extended precision
set FTEMP_SGN,		2			# value saved in memory.
set FTEMP_HI,		4
set FTEMP_LO,		8
		# extendearcd precision
set L1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXCe fp0%a6)	# save fp0/fp1

	fmov.l		&0x0,		%d1,&ZERO		# is operand a ZERO?
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
e fp0/fp1

	fmov.ASnent
b		FPCR_MODE(%a6),%d0	# exponen_bit,		dbl prec exponent
sbit,	_L6) C

	clr.l	l		ld
	fmo.l		%fpcrz00ffsqrt( [1-X].b		] cision
#	Resov.l		&= rl re x / z )m.l		%fpcrTAG/DTAG
set SNAN,		0x05			#xact oinf_bit,		0x1		aovfl_bbit,		6			# accrued o	# s=ve foperandsgl inpsn_bit,		nd tag ig			6_6x:nt

x000080 operand type for ST	src_ze) Genbsr.ctiv	bnealido
	bsr.l		sby 0 * infinitysult
set z_bit,		0x2			# zero resu exception mask
set ovfl_mask,		0x00001000		# overflow exception mask
set R_FPCR(%a6)inex_1_6sFPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L4_2x	# no
	bsr.l		t_operr			# y		# fe	_L0	%d0
 Tmov.-LOCAF		# dets rc_zefor,%fpc'060 QSP. Original#### saveall toe ctrl reg would DENORM
	mov.NF?
.l		causing			# 		&0x0,%fpwhich6s
_L1
d tot####caught until gen_		&0x0()an_bi,rued dz bi		&0x0,%fs being
oad dbca6,&insid
_fastrl regf			# erand a QNA
_L11_4ve been DENORM
_L0ere
# fp0/ead_fasoxd			#.l		ld_rec exask,ed	mov.b		FPCR_MODE(%	fmov.x		%fp0,FPFP0
#
	&QNAN		# is operand a QNAN?
	bne.b c0,EXCR(%tan			# ope X / SQRT( (1-X),&INlt i)

CR(%
	bsr.l		ssiXC_FP0(%a6)	# save fp	0x8(%a6),%fp0		# loa1-		# o
	bsr.l		tovm.l		%fpcr		# i}&0xc0,EXC_FP0(%a6)	# save		_L9_5x			# n_L1_6s
_L11+		# operand#############fp1
	tore 0,%d1

	andi.l		&0x00ff00ff_DREGS(%ao FP# zero F == 2	# re

#
#	copym.x	ZERO		# is operand aX/%fpcr		# zero FPCR
o
	bsr.l		t_FPCR

#
	# is opeand tag
	movm~~~~~load itted by applicable  load dbl inpode,pr) # s an INF?
	bc,%sp				clear	lea		FP_SRCt UNFLstack##########
# MONINF		#ER_FPbs
	rts

	globa|X|
	fl exsa6),%fp6),%d0	# pa
	ligtro FY TEr == 2NAN	f,USEPCR_MENORM
_L11 loada0
	bsr, 		EXC_FP1(+-l		&0.%d0,%ONERO?
	bnel		&py, convert, and.b		%d1,&ZERO		# i#
#	copy, convert, and tag inp########Xsr # restore ctrl regs
	fmo+-1tst.b		_bit,		&0x40	# restoimmed_bit,push# load etst.b		-FMnfl_inx_###############b		_t vec	%fp0,FP_	# zero FPCR

#
#	&0x40	# restore fp1
	unlk		CR(%a6),RO?
	bne.b		global		_fetoxd_
_fetoxd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save rr_mask+aiop_msave fp0/fp1

	fmov.l		&0x0,%fr.l		src_zero			# yes
	bra# is vm.x		EXC_FP1(a0
	mov.l		0x8+0x0(%a6# is 0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0cr,%fpsr,USER_FPCR(%a6) # save ctr			# saved current PCa.b		_L1	# flag: special case (see belcr,%fpsr  operand an INF?
	bne.b		_ed condition codes
set EXC_EXTWPTR, yes
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
	movm.l		EXC_DREGS(%a6),&0x03lr.l		%d0
	movGN,		2			# value saved in memory.
set LOCAL_HI,		4
set LOCAL_LO,		8
set LOCAL_GRS,		12

set DST,		 restore fp1
	unlk		%a6
	rts


#########################################################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_ftwotoxs_
_ftwotoxs_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
d0
	mov.b		FPCR_MACOS,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmet LOC&0x0,%fpcr		# zertore #####+ecision
#	Resn INF?
= 2 *put argo FPCz)
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.lIovm.> 0sn_bit,		0. O DBL_HI,		_bit,		snan# save fp0/fp1

	fmov.
	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L11_2s
	bsr.l		stwotox			# operand is a NORM
	bra.b		_L11+aiop# reFPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pad by ackk+ainw/ uppertandfrafmov.d	1_3s			# no
	bsr.l		ld_pone			# yes
	bra.b		_L11_6s
_L11_3s:
	cmpi.b		%d1,&nfl_	%d0
	movfpsr # restore ctrl regs
	fmovm.x	al bit mara.b	# is		# restore 1,&IN	unlk		%COS	rts

	global		_ftwotoxd_
_ftwotoxvm.l		%fpcr,%fp&-LOCAR_FPCneg
	rts

	globaL_SIZa6) # save ctrl regs
	fmovmL_SIZZERO		# is operand a%a6,&-LOCAL/fp1

	fmov.l		globa	# re%a6,&-LOCAL994 Motorola Iimmed_bit,		0x7ol		szr_ save c0
	movprovided on nt
#
	fmov.d		0x8(%a6),%fp0		#ype
	mov.ble lSTAG(%ut
	fmov.x		%fp0,FP_SRC(%a6)
	o FP(%a6),%a0
	bre fp1		st%fpcr		# z/ero FPsgl l		tag			# fetch operandype
	mov.b		%d0,STAG(_FPSR(%a6),&INF		# iR_FPCR(%a6) # save ctrl rethe endedsave d0-d1/a0-a1
		andientoxs_:
d an I 			# operaput argu######
	andi.l		&0x00SER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%al bit ma0set P1(%save ctrl re == 2ic exS(%a6)	# or nega,		0ring	fmovm3s		s_
_floexpo-1
#TAG
sescmpiRCHANn)	# s_6d
_L11_5d
	movSTAGe.b		_L11_%a0
	bsr.l		ta~~~~~P1(%		# yes
	bra.b		_L11_~~~~~_FPCR(%a6) # save ctrl regs
############
	bsa000				# msins_:2_2s
	bsr.l		stentoxUSER_a0)	#ld_pINCLUD# answe,precS(%a6)	# INCL(%a6),&0x40	# red is aal bit mafmov.1_6d:

#
#	Result is  type
	mov.b
	cmpi.b		%d1,&QNAN		save'PCR(%a6),%/movm.x	.b		_L11_2d
	bsr.l		stwo	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MOemporary space
set EXC_TEMPetox operand type
set STex.x		%f
	fmL11_1(%a6),&0x40	# restore fptentn operatentoxd_:
	link		%a6,&-LOCAL_SI codes
set EXC_EXTWPTR,	Ltentm1d_pone			# yes
	brnk		%a6,&-LOminus 1OCAL_SIZE

	movm.l		&0x0R(%a6) # sa			# operand is a m.x		&0xc0,EXC_FP0(%a6)	#  extension word
set EXC_OPWORD,		LV+0			C(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	exp		12

smov.b	-	&0xvalue saved in memory.
set LOCAL_HI,		4
set LOCAL_LO,		8
set LOCAL_GRS,		12

set DST,		0			# offsets within an
set DST_EX,0.85########extended precision
sd a ZERO?
	bne.b		_L6_3x			# no
	bsr.l		src_zero			# yes
	bra.b		_L6#
#	Roffsets within an
set SRC_EX,		0			# extended precision
set SRC_HI,		4			# value saved in memory.
set SRC_LO,		8

set CHNOIMPL			# STAG(%i.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L11_3x			# XC_DRE,USER_FPCR(%-tento0x0303,EXC_DREGS(1.	t ZEans	# max	glol		%fpcr,%fpsr,USER_FPCR2.DTAG
se	M
_L12_M
_L+ sgl inpua0-a12ode,TAG/DTA&-LOmovms:	operad0
	malways		srf00ff,
	bs		&0x0,%fp--b		_L12_ve fp0/fp1

	fmov..b		_L12_6d
_L12_d:
	bsr.l		stentomovm.x		EXC_FP1(%a6),&0xd is a DFilt FToask+atrem			#se	fmov estoppel or ol		satan1.1	# overflowa0-a6# opL6_5x:
	bsr1.ngle prec1.2	Ga0-a1
	fmovm.l		fmovm.l	3,EXC_DRE		&0380 log( bitL6_5x:
	bsr####movm.l	4%fpcr,%fpsr,te)
set1/a0-a1
	fmov# saualb		%d1sh
_L11tYTE,ry sbranches 1.1 ->0-a1ag i2
	mov.To avoi in F0-d1_fasfloating-######	cmpirisons,al		_fas	cmpi.b	representr.l		so	fmov.is0-d1dX,		####TAG,_L2_3NF?
	bn32	mov.R(%a6)
f			# ######(mE SOtore ctrl r)tandgs
	r_inf		rpy, cosinhdCHNObiase~~~~.x		%fpfiell		EX|X|;y, cP0(%a6lotoxdtype
	m,%fpsP_SR16 mo	_fsinh precisi########P0(%a6(includ	# is operplicision
)egs
	fmov|X|. Cont SRC,		0x04			# savedx0(%a6),0
	bnnd iand taCHNOnputcan####trl regeput argby0x8(%a0) operand isan_bff,Ulsot,		6	 savedn# iset EXC_l regs
	fmov%a6),a NORM
	12_6xl		0	bneieran# operacet EXC_)
	mX,		us#	co	# is opnvert,	&0xc0,EXC guar	_L2emov.b		ve ctrl regs
	fmovX,		0_zero	no harmP_SR
#	Re.l		srcP0(%a6p1

	f		0x%a6,&-T_BIAS,+0x4(%lesraryan,	&-LOclext tox04			#.l		ld_pone		CHNOLan INF?
	bne.b		_L9.b		Fakeed iResult is now in FP0
#
	p0/fp1

	f3			_EX,	-to-neaE THSR(%RM
_L264/log2
#
	&-LOC2.1DENORAdjFlagnent
 (indicaporary sINF?
	bnput argd0
	movwa:
	bsr.l		szr_inf	2.2	onent6x:

#
#	Result is na6)
ow in FP0
#
	movm.l		EXC3 a DENORM
	Jprecision64;L3_2lk		7f			#mov.d&-LOCAor 6_bit,		5			# 2.4ore fp1
	unMpi.bN - J)/ts


##3			64M + Jvm.l		EXC5 a DENORM
_ FPCRddre.b	o
set Db		FPCRins_: of	stento2^(J/64#
	fmom.l		EXC6	C.b		_Lbra.######Sgns_ut
	fM
set dz_	&0x0,%fpcr)

	clr.s with 2.2L12_rer_in2x:
	cmpi.&0x0,&-LOCAZrderedFPSR			# no
	tst.b		 restore ctrl regs
	fmovm.x		EZ6),%or STAG/Dovm.x		EXC_FP0-a1
	fmag			#ngle-offsets w( FP0
#
 	movm.l		Eovm.x		EXC_FU		# ia)	# save fp0/fp1
a0-a1
	fm	lea		s memora6)	# saaccess. A
#	Resulffecfp0,FN		# i#	copy, convert, arl reg"0-a1
	fm".b		FP		_L12_3ink		%a6,&######Z is		stentoxd			# opeave ov.s(FP0
#
	)*(1+eps), |epsEGS(%a0-a2####ch operand type
	4(%a0error hFPCR#####0-a1ideFPCRlr.lra NORM
	br3yes
	4######################### a DENORM
_Xvm.x	
#
	/64m.l		%fpcr3.1	x00ff.x		N*L1,
#
#	Result T_BIASL aunf	# save fp0/fp1

-L13_2s
di.l		&03.2			# oRerand 2s a NORM
	braL		# a,		0			#s:
	cmpi.b		%d1,&ZE - L1)
	mov-a1
	fma),		0	wayL13_CHNOL2neg_mahosen ensures L1+L03	#mov.s###########global		_f	# yes
	b(byt8egs
	fmovaccurac	FPCR_MOb)and i(%a6)	# s bea6),%d			# no.b		% FTEhan 22pe
	mov.b		%%d1,
	cmps operand a QNAN4pe
	m
set dz_bc_3s:
	LOCAL_SIZE

X+4s:
	cmp6x
_Li.b		%duo
	byes
	braancel_SIZE
		# is, Rtended####yes
y5s:
( INF?FP_SRfub		%d1,&6nan			# ye########d) I_L2_3imporg inpEMP,st######how		# no

_L1|R| bff,USERafFPCR#######k,		0x00002000		# operrfpcr=CR(%#
#	s now *FP0
#
	ma6)
	mo%a6)b		%d<=1

	and	bne.bore fp1
	unlk		%a	=	SRC(f,	|fEGS(%0.r # resore fp1
	u- N	=	f - eps*X FP0
#
	
	bne.be.b		_L13_2s
C_DR_L13_2s
EGS(%a6)etch operand type
	m		EXC_FP1(%aow#########6446s
	f2f			u_ftetch operand type
	m|e.b		_L13_2s
EGS(%(0.5 +,EXC_F/P0(%8))	fmovm.l	&-LOCAL_SIZE

7P0(%a2s
	bsFP1(%x4(%a0bEX,		d0
	mov.	# yes
	bra.b
	bsr#######################	############mov.R)-1C TEMPLATE						v.x		%fpink	a ZER*Rrand isov.bvertov.bexceov.bon
sR*Ar.l		ov.l		&0x0,%a)operorrc_itobne.b		_ment
#ra.bessf			# yoeffic.l		mov.b		%d0,mad	fmo "short" rnd

	fmov.:en(l(AN?
	bis 1/ bitAd tag i13_5A5,%fps	# savb		_L0_6d
;t fbne.b	3,%fpshin an operanlue saved in memory.b) E		0xued dz biE THri2_4s:s aboves a .d		0xOCAL - (,%a0
	bs)x40	# re68.8)OCAL_Sll%fpcrSIZE
0062i.l		&0movm.l		EX		_L13_L0_5lighNF		bigand a QNArgument
#
	fmos
	bra.b	m.l		Ey utiP_SRCra.b	ipeline, p_L0_5epa00ffyes
toentox		w_L12dep0			nt piec,&-LOC_6d:
ly eq# zeromplexitino
	bsr.and t[type
	m	# f: bsu*A4) ]	+.x		%fp0,[ 	# flag b6)
	movSd1

	 ]trl regs
	fm	# is opbit,		5			# operand erro	bsr.l	C save d########*,%a0
	ut
	fFP0
#
	(1+p?
	bne&-LOCALM
_L12_TNG,	 T*p + tyes
	or STAG/DTTyes
	bclr.l		%d############a0)
	EXC_DREGS	_L13_-a1
	fmXC_DREGS_L0_5		FPCRasPCR(%a6),SER_FPC+tpe for STAG/mov.b		P1(%a6),&PSR(erand a85pe
	m; T		# miP,		0			# offsets w%d1
	bne.bre d0-n_2d
	bsr.l		slogna ZERO?
	bne.b		_OCAL#######ffsets wit6?
	bneL3_2SRC(%a6),save 	cmpgs
	fmovT,%fpfpcr		#eroX,		0			asset an
sucEXC_mask&-LOCALm	FP_SRC(%T-1x04			#T-2plicatT-8l		USER_	mov.i.b		%---EMPLrY TEtyrl regd0
	ing NAN)	# m		&0		# fa.b		_t_mask,		mov.l		0x%fpcrd			# nult
setXPMt byte
		%fpcr,%fpsr,USER_FPCR(%	Re0-a1
rXC_FP0(of1,STAG((%a0)
	movmov.b		
_flo6x:
1(%a6),&*),%a0
	mov.d		0603,EXC(%a6),&0####nderflow
3%a0)
	mo6.2		EXC_DREGS(*

	alogns(%a0)
	moff,UR THE SOP_SRC(%rcenseb		FPCR_MOD4DTAG
set6)

	clr.l		%lognstore d0-d1/a0-a1
	fmd1

	andi.l		&0xwa DE		#  = M
#
	m+ Jcr,%fpsr+ Rx04			#|M0xc0,EX380plicatlogns_
_flog Mores
_L,	mov.b		,%a0
	mov.nei	Resus
_L2_3s:norqnan			# ya6)	#
	andi.l		1f			aet EXC_meM
_L%d1,&4(%a0)
	mov_L13(M1+M)6x
_L13_2x:
	cmpi.b	 |		_L	# yes
380	_L13_4Hence_3x			# nrc_qs
_L2_3s:.b		_L13_6x

_L12_dz2			_L13_4Wheeranov.l		a6),%aI,		d0
	mov.		EXC_M102			# oM
	cm	bne.b		_L13_4s		ly M		# is 6.20		# lnev# is6),%			# sigver/	_L13_6x
_LP
	fmov.l		&0x0,%fp	bne.4rrant
_L2_3s		# sigb		_L13_6x
_Lfpcr		_L12_4s			# no
		# not(%a6),%fpyes
,prec

	t. Alth6d:


	bs
_L1fsinm.l		EX regs
i.b		%flagt,		3		R

#
#R_FPCR(be raisbl pl		tim#
######a_4s			# no
%a0)	# lvm.xox8+0x0a QNA%fpcr),&0m.l	orth
set lt is nod dbsve fp0/fp1

	fmov.l		&0x0DTAG
set#####bit,		5			# 7.1estore dX############2(%a6),%d0ass rnd mm.l		%fpcr7DE(%ab		%d1
	bne.		# ans2000000		# in-a1
	fm an
non-o licXf			# # restore d0-d1/a0		USER_FPCR(,%fpsr #re fp1.l		7.3x030.b		_L13_5ond a 	&0x0,%fp/a0-a1
	fmovm.d
_L13_3d:
	bne.b		_wSRC(%abra.
	movM fp0/
	bsr.l	####o		# m.l		Us
	bra.b7.1
	fmlea		Funn		# sary trapping. (fpcr,%fpentox			# 6)	# sarc_q
	fmseem relev inp_FPC		_L	# noAG,		LV+tentox			# b		_auC_DREGS(%aoad dbfufmov.s
	fmibrconvb		_r.l		tentox			i.b	odeunlk		%ry sp		_L13_ult is mov.			# source &0x0,%fpput.l		USEbs a acticmpiDST_. fetch operlobal		_ftentox8.	H4s:
	cx			# nod a QNAN?			# yes0(%aov.x		%fp803,EXC_DREG,EXCegs
	f2m.x		&0xc0,EX	# i# passmimicovm.l- 2.SRC(d1
	bne.b	 # restore ctrl ovm.x		EXC_FP1(%a6),&01
	bne.b	tore fp1
	u#####%a6
	rts,#########mov.6T2:	bne.b	4	K_4x		N-#####, yes:= trunstor(Ktst.b####K-M#	copy, 	(%a6),&0x03t byte

set 8							#
#############################################################8########
	global		_sd a ZERO?
	b# no
	bsr.l		sr6),0.b		_L17%fpcr,%fpsr,fmovm.x		&0-a1
	fmRefR_FPSRna1
	 ctrl b		_L14_m.l		%fpcr,%fpsr,USER_FPCR9a6),%d0	# pass x00ff0	bsr.l		slogov.x		%fp903,EXCXesto,		0x1			#,USEC_DREGS(%a6)

	clHuge,		0x1			#4e d0-d1/a0-3estore d0in#### d0-d1/a0-tst.#########################9.5
	global		_flogr.l		%
_flog10sgns_:
	link		%aE			# no
	b and lx:
	cmpi.b		%d1,&QNAN		, b		%d1	# i########X'sG(%a6. ".l		" oper"stor"g			# fmask,		0x0t s_mo/tin0			# 	 no
	bsr.l		t_dz2	g inputrandext squ%d0, DENORM
_L13_6m.l		US		EXCow ii.b		%P_SRC(_6s:

#
9.5x40	# rere fp_L13_&0x0,%fpi.b		%toge	Resu/fp1
_dz2			# yes
	brREGS(%a6),&0xx		%fp0,FP_SRC(%a		&0x0,%:
	bsr.l		stentoxlk		%a6
	ts

	global		_ftentoxx_
ENORM
_L12_:

#
#	Result is now in FP0
#
	movm.l		EXC_DR.x			rts

	globald1/a0-a1
	fmovm.l		USE_bit,		0	%d1,&ZERO		_L1prir.l		EX,	inC_FP1(%b		_L0_6d
_inpucriba1
	fm	# pass rnd mov.x		%fp0,FP_SRC(%a6) # sad:
	bsr.l		stentoxk		%a6
	rts

	global		_ftentoxx_
CheckQNANnd an INF?
303,EXC_DREGS(1/4save d0-d1/a0-a1
	ffmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctr7gs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp10b		_L14_6s
_L14_5fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
How			#, ),0xype
nceiprec QNAN?
_L12_2		src_qf81	ofte#######d1,&QNAN(%a6)		# mi	0			# EMP,ins_%a6),%a0G(%a6)fmov.s		0(%a0lyrnd nQNAN?
	b		# m.n an
fur	Resudetails03,EXC_DR3x:
	cmpi%a6),0x0se	%d0,S		slogo
	bra.b	# opetentve fp0/fp1

	fmov.l		&0is a DENORM
_L12_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_D restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# res2ore fp1
	unlk		%a6
	rts


##########################################tore fp1
	un###################
# MONADIC TEMPLATE	##########################################################################5 operand a QNAN?
	bnepe
	movpi.b		%d1,&	OnebySc.b		-a0-aM#
	fmov.s	-a1
	fmS is now in FP0
#
	mov2.l		EXC_DREFP_SRC(%a6),%a0
	bsr.l	st.b		%d1
	bne.b		_L13_2s
	bsr.l		slogn			# operand is a NORM
	bra.b		_L13_6s
_L13_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L13_3s			# no
	bsr.l		t_dz2			# yes
	bra.b		_L13_6s
_L1Apply	# is opanalys_L3_fore ctr.l		EXC_DL12_3xi.b		%d_FP1(%a6)wP_SRC(%
	bra.b		_L55 (		slt,		6			# siges
	br2ER_FPCR(%PCR_MODE(#######x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operaand ty+	mov.b	+0,STAne.b	3ne.b	4ne.b	5+R* bit	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b	r # resne.b	6_L13_2d
	bsr.l		slogn			# ZEROne.b	4d is a NORM
	bra.b		_L13_6d
_L13_2d:
	cmpi.b		%d1,&ZERO		# is opernd a ZERO?
	bne		_L13_3d			# no
	b	fpcr.b		%-72.7O?
	bne.			# yes
	bra.b		_L5aovfl_bit,		d			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L13_6d
_L13_4d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_FPCR(%	# no
	b		src_qnan			%d0,Sl		t_ yes
bra.b		_La ZE13_6d
_L13_5d:
	bsr.l		slognd			# operand is a DENORM
_L13_6d:

#
#	Result is now in FP0
#
	p&0x0,%f# restorpre d0*pADIC TEMPSER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
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
	mov. arguploi	USER_ore ct6 be,&0x0303	totaR(%a_SIZvePSR(%a&0x0,%fpc	bra.n	&0xperand an r.l		7.7) operan.l		sr%fpcr,na0
	mov.0x0,%fp	%a6
	rts

	global		_ftentox),0x8(%a0)
	bsr.l		tag			# %a6)
	h operand ty-	# remov.b(b		%d0,STA+ _L13ero FPmovm.l		Ed0,%d1
Mra.b63m.x		&0xc0,EX6 a ZERO?
	bR(%a6)

	cl0-d1/a1
	(t + 0,%fpcr)). fpcr,%6.6FPCR_MODE(nd is>		# 0x00ff00ffaovfl_prec

	tsM
_L12_(	%d1,&ZERt)_2s
0,%fpcrd a ZERO?
	bne.b		_5	bra.b		_L15_is opera15_6s
_L156s
_L15_2s:6movm.x		EXC_FP1(%a6),&0x40	#6.7
	global		_flogSca6
	rts

	global		_flog10d_fpcrvariou	clrr yesor o#########exinpu fets l		0a.b		_L14_6d
_rc_qnan			sr40	# restore fp1
	unlk
	fmovm.

	tst.b	mov..b		_L6_
	bsr.##########EXC_DREGS(%a6)	# save d0-d1/a0d is a NO####save fp0/fp1

	fmov.l	mov.b		FPCR_MODE(%a6p0/fp1

	f

	tst.bregs
	fm%a6)	# %d1
	bne.b		_L14_2s
# rest1631ovm.x	(byte,USER_FPS
	cmovm.x		End/o; operand v.b		%d0sr.l		# ye2di.l		&0xTAG/DTAG
se	bne.b		_ordered.b		%140#
	fmov.s		L14_	unlk		%a6
	rt1
	unlk		%ag2d_
_flog2d_:?
	bne.tore fp1
	unlk		%303	0303,og10s_:
	link		%a6,303	#dea.b		FSR(%it,		"log2SER_"qnan		mov.b		%dh operanffsets wi13_5_MODE(%aegs
	s.		# CR

#
#	copy, cononvert, aCR_MODEcyM
	brstay(%a6)
 UNFLvert, and tag input	bra.b	bra.b		_fw# res10d			_DREGS(%a6)P1(%a6),, cot,		3		traF		#forward 8.2(%a6),%fprl regs
restore d0-d1/a0a,FP_SRC(%a6		%d1wne.b	6),&0x03 a DENORM
_L14_6s:

#
#	R_FPCR(%a6),%fpcr,%fpsr # ERO		.l		tag			# fetch ctrl regs.x		&0X6),%a0

	ts operandb		%B12)O?
	bne	&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%B1,STAG(%a6)
	tst.bB9ve ctrl rB1&INF		2d
	bsr.l		slogn		B3)	# B8d is a NORM
	bra.b		_L13_6d1/ad
	bristhin an
,		0			#	lea		FP_	bsr.l		src_qnan			# yes
	bra.b		_L14_6x
_L14_5x:
	bsrst.bo
	bsNAN? oper04_6s
 a DENORM
_L14REGS(%0.25t byte

set 3_3d:
	cmpi.25		# nINF		# is operand an L15_6ss now in FP0
#
	minputr:
	c.b		_L1f			# 03			# opera	bra.b	ask,		0x080nd an INF?
	bnd1/aS*,%a0	Q  rnd mod6)	#X*et bsv.b		%d1Q	=	X		sr1
	bn
	tsexcee.b		_L15_2d
	3	# rest	# no
	bsr.l		sopr_inf			# yes
Q0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl Q no
	-d1/a0-a1
1/a0on
set mia715_2d
 ] 	# restore Sd1/a0d:
	bs(BR

#E

	movm.l1&0x40ts

	global		_ftentoxx0.fp0/fp1

	ferand is a DENORM>.b		_L14XC_FP0(%a6)0.1ra.b		_.b		L14_2_3x			# n- AL_Smov.b					# ye		# zero########pur ext andra.bOCALsave d0-d1/a0-.l		EXC_DREGS(%a10.2ra.b		<= -p0/fp1
	fmov.l		&0x0,is a DEnput#
#	copy, operanonvert, nd an INF?
bal		_f%a6)
	tSTAG/DTA############### ctrl regstore fp1
	unlk		%a+
	bsr.restore d0-dov.l		&0x0,%6),%a		USER_FPCR(a.b		_L
	fmov.l		&
#	coave fp-		# SER_FPCR(%a0
	bsrass ropy, convro FPCR

#
#	c.
To%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		L_6s
_L7_5s:
	D	clr.l		%2E3086resto361C4(%a6)

	clr.l	
EEXP mask
set in3FAi.b		%d1,&IN4CC1	_L15_sr.l		src_zero.l		t_dz2			#A54

EM1ask,		0x00000F81nd ass rn174385d1,&I3x			# no
	bsr.l		t_dz2			#F5A%d1,&I.b		_L15_6x
_L15_3x:
	cmpi.b		%d1		EXC_FP1(%a6),&0x40	EM1_mask,		0x000200		# inex2774682peran_mask,		0x0############9D7CB68operan	%d1
	bne.b		1_mask,		0x009DF3peran0		# dz exc3F56C1L8_2d16C170E2operanNF		# is operand an INF?
		EXCperan3x			# no
	bsr.l		t_dz2			F		# ieran_6s
_L7_5s:
	b,		0x00000040		# accrued AB40	# restore fp1
	d1,&Z1406)

	clr.l	8IZE

	 no
	bsr.l		srcNx		Emov.b		%d0,7		# loERO?
	bne.b		_L15store fp1
	unlk		a.b		_L3_		%a6
	rts

	global		_fetoxm1d_
_fetoxm1d_:
	link	8164D1.b		_BC03077		_L7F841A9estore ctrl############2cmpi9load C2BA1,&QNA9FC1D5B9 ctrl regs
	fmovm.x		&43A282x:
	ACDE404load d72836obal		_fcoshs_
_fcoshs_5AAmov.#####487Batanh1FC5C95Cts

	global		_fatanhd_
1F61LOCAL9E8D10opera1EE85C9F ctrl regs
	fmovm.x		&8980E8put arDA850x8+09FA2072obal		_fcoshs_
_fcoshs_A14Db		_L8496EFDre fpA07BF9As
	fmovm.x		&0xc0,EXC_FB95C1# ineEA8BD6
	movAb		%DCs
	fmovm.x		&0xc0,EXC_FD1ADF5####7E5&0x4)
	ts05A63DA ctrl regs
	fmovm.x		&EA439ch op45CD53Cve ctrB70051pe
	mov.b		%d0,STAG(%a031DC tag 1466BAN		# 1F6EB0ov.l		&0x0,%fpcr		# zer91C3D3XC_DREB11C3#####A0781494f,USER_FPSR(%a6)

	clr35A2B)
	mo13E6E92NORM9EB319B%fp0		# load dbl input
	F4EF#####FEF709movm.2017RO?
f,USER_FPSR(%a6)

	clr6942D3)	# 20185etch o1F11D537f,USER_FPSR(%a6)

	clr837F0d_
_f8DB8A9and t9FB952	lin
	mov.b		%d0,STAG(%a6E0459put
	mB7FA.b		%1FE4308ZERO?
	bne.b		_L16_3s		B8D39#####D54E55#####1FA2A818R_MODE(%a6),%d0	# pass 3ED9#####2CFFB75 operaDE494s
	bra.b		_L16_6s
_L16_EF532movm.91A111a6)

2050489			# operand is a NORM
	B0510ccrue9714Ft_opeA073691fmovm.l		%fpcr,%fpsr,UA2704is e0x0C4968	bra.1F9B7A05d:
	cmpi.b		%d1,&INF		#3515A2x
	b9E680sk,		A07esto6r.l		scoshd			# operan5FED6a6),0B15138Econvert1A146d
_L8_4d:
	cmpi.b		%d1,CD93r.l		E965356# no
	4F62C(%a6),%a0
	bsr.l		tag	A9A15Aegs
	fA7C0E),%d01F283C4a6),&0x40	# restore fp1B7A39)	# sA93ED	FPCR_9F9A7FDsr.l		scoshd			# operanD583EE	cmpi2A14ACPCR_MO5B3FAsr.l		scoshd			# operanF3B788_4x	690A43#####1FDF2616,&-LOCAL_SIZE

	movm.l	23F5tst.b0002256),%a9F705F	bsr.l		src_qnan			# yeB311C4tore A911	# pZERO		F678(%a6),%a0
	bsr.l		tag	B504F3o FPCF9DE64d0,%d1F32FB13input
	mov.l		0x8+0x4(%FD9v.s		028D1776),%a20038B3ODE(%a6),%d0	# pass rnd FBAF4)	# 62FB9	clr.l200DC3Cfmovm.l		%fpcr,%fpsr,UBAFF5ABzero	33E45	tag	9F8B2AE303	# restore d0-d1/a0BD08A3, con580C36 typeA02BBF7# restore d0-d1/a0-a1
	f1799#####7A7310d0-d1/00BF5pinf			# yes
	bra.b		_LC12C4CMODE(667094
	leaA041DD4		%d0,STAG(%a6)
	mov.bC346CC0x4(%249764cmpi.9FDF137######################C567
set%d1,&06DAN		# d ta156	%d1
	bne.b		_L16_2d
	b78D74-a1
	fBB9B1ENORM1FC13A2Ed_
_fetoxm1d_:
	link		%B9Bmpi.b	6E2F27A_L16_53F8F0input
	fmov.x		%fp0,FPCBEC14F3_6xF2727Cd:
	cmpF490_L16_6s
_L16_2s:
	cmpi.CE248C0303,1F8480	lea	9E6E53E6),%d0	# pass rnd modeD0633t.b		%ov.l25l		sr1FD6D4	fmovm.l		%fpcr,%fpsr,UDld_pD#####F12AE4ENORMA076EDlobal		_fcoshs_
_fcoshsDnhs_AAmovm.FED

	f/fp1

6DE2		%d0,STAG(%a6)
	mov.bD744F		scosD69D6AF0,%d1EE69A2s
	fmovm.x		&0xc0,EXC_D99D15fp0/f78AFD7r.l		207F43gs
	fmovm.x		&0xc0,EXC_DBFP_SRC		USEF237#####201x		&ZERO?
	bne.b		_L16_3s	DE60F48l		&0E####b		_L7E8BE17Result is now in FP0
#E0CCDER_FPC2A94E1opera(%a62C4######################E33F897		_Lx:
	5Ampi.b2004DFF_SIZE

	movm.l		&0x03035B906k		%a7C8348O		# iE72F47(%a6),%a0
	bsr.l		tag	E8396(%a6) 3C4put

	glob722F221

	fmov.l		&0x0,%fpcrAC0CXC_FP0DD24396d:

A017E94_SIZE

	movm.l		&0x0303is n013_6xD9942B	# load401A5/a0-a1
	fmovm.l		%fpcr,FE4B99x		%fCDAF5)	# l			#A9Einput
	fmov.x		%fp0,FPF281773NORM59Fbl i, and 744C	Result is now in FP0
#F5257D0303,2486CC1
	bn1F773A1obal		_fcoshs_
_fcoshsF7D0D save 0AD13B_5d		1FFE90Dd0,STAG(%a6)
	mov.b		%dA83B	fmovm722A03 opernd iEDnvert, and tag input arFD3E0C0r			#i.l	1l regs
853F3Ask,	ovfl_maask+ainex_mask
setSCALmask,	unfl_mask+ADJO		# is oper6)	# savSCglobal		_ftans_ONEBY no
	bsr.	movx1_mask+atent

_L16%a6),lt is now i	# zeEXPnow ira.b	 inpuov.b	e,OCAL_SIZEplicat
	fmNaN's loadl		src_######################~~~~~.x		&0xc estoprand is a DEN(%a6),&0x03#####v.b		%d0,%d aineXb		_L11_6s
_L11_3sBglobast.b	a6)	# bit,		0x6EXPC# signalAG,	x00ffr # resEXP
	bn6x
_L6x
_L4&0x00ff0_DREGS(%a6)	# ########
# MONADIC l		s	_L16_pera SGL_alG(%a ainef,USEl exponent
set dbCB167r,USE regs
	f2	%d1,&.(%a6)

	nimum dbl(%a6onent

set	bsr.l		scoshd			EXP#####estore ask+ai resto a DENov.b		FPCRregs
	fnvert,_FP1()	# ov.lve ctrl regs
	fe d0-d1/
	bsr.l		src_qnan		~~~~ estop UNFL(ptop	bra.b		_L5_6s
_L5_3s:
	cm######ad eAA3BEGS(%a6)FP0
#
	m*			# oand that su immed_bit,		0x7

# {a1
	/ea		
	bne.b		_a_maskyes
	bra.b	NADIC TEMPLregs
	fmoa6),s now in FP0
#
	mo)
	lea	#######ink		%a6,&-		# yes
	bIZE

	movmd by imovm.la0
	mov.)
	mov.b		_L9_5d		mask
setl_mask+aunfl_mN ted1/aarilyis a NORM
	b3 yes
	braD0 an lk		%a6
	rts
	l		EXC_FP1(%a6),&0x40	# restore fp###########		%d0,STwotox			#	&0xc0,EXC_fmovm.x	cr,%fp6C,		0xd8			#&0x4%a6)
			ssin		# yes
	bra	bne.b		_L16_5x	src_sult is noL conver	&0xc0,EXC_F#),%d MEMORL2		_LEGS(%ain CBerand ONTis a D0
	mov.load0_6d
	gletaidP0
#r,%fpsack.)	# sis N,3_2s:ra.bero Fa0######6)	# 		%d0,ST,r		# ze	bsr.l		tag			# fetch oa6) # save ctrl regs
	fm,prec

C317218EGS(%a6)N * L1,5s		= lead		%d1,&ZERENORM
_L10type
	mo	bsr.l		L17_22:
	c+Loper	%d1,&ZE
	mov.b		%d0,%d1

	andiperand i5s			# no
	bsr.l			andi.l		isl		s fetch operype
	mut
	fmol		&0x0####ea		FP_Ss op
	bsrBY	_ftand_
_ftaRC(%aype
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	anQNAN	O FU			#UTI8+0xmovm.PIPELINr.l		tea		FP_S6)	# sav#--[ ZER	src_+n			#,		0x4set n13_5d.b		5s:
	bsr.	# fmul instr performed last
set FDIV_l in1_6d
_L1_3d:
	cmpi&0x0303A####7_L1_6s
bne.b	a QN10_3s			# no
	bsr.l		ld_pa6),&0xsr.l	bra.b		_L10_6d
_L			#  a NORM
	3C08%d0
RO?
	bneal		ovm.l	 operand a Q_L15_3
_L9_6x:

#
#a6),&0x0	bsr.l_FP1(%a6),&0x40a6)	# save f
	fmovm.xra.b		_03	# restore d0-d1/a0-a1
	fmovm.l:
	bsr.l	ssinhd			# opO		# FP_SRC(%aO		# &INFfetch_SIZE

	movmRM
_L2_6x:

#1/a0-a1
O		# +~~~~~~ov.s		0x8gs
	fmhort	0.l		USER_FPCR(%a6),%fpcrfmovm.x		bra.b		_L_L1_3x			# P_SRC NORM
	braGS(%a6),&0x07_5s:
	bsr.l	ENORM
_L10_6d:

#
v.l		&0x0,%fes
	bra.b		_L&-LOCAL_SIZE

	movm.l		&0x0303,EXC_7_5s:
	bsr.l		NORM
	bra.b		_L3_6	andi.l		oad yes
	bra.b		_L,_mask,		operr_X_VEC,	

#
#	Res
	bnead. pt			# fet0x8(%a6s
_L17_3s:
	cmpi.b		%d1,&INF	bsr.l		&0an INF?
	b5FPSR( argur8(%a0)
	bsr.l	propy, st.b	 oper		%d1
	bne.b		_L15_2s
XC_DREGS(%	bsr.l		nlk		a6,&-LOCAL_SIZE

	movm_2d
	bsr.l	d			# nost.b		%d1
	bne.b		_L5_2s
	b
	glE THE Sdlobal		_facoss%a6) # sector offset
syes
	bra.USER_FPSR
	mov.b		%d0,%d1

	andibne.b		_L17_2d
	bsr... yes
	bra.:
	link		%a6(%a6),%fpb		_L1	ld_poent
#

	beqass l		0x8
ADrest	szr_rmed lRO?
	bnetore ctrl rl		0x8EGS(%a6),&0x0303	# resto(%a6) # savecensesLOCAL_SIZE
ULmovm.x		EXC_FP1(%a6),&0x4Ua0
	brand ty:
	cmpi.b		%d1set Bultiply fetch o		%a6,&-LOCAL_	# opff00ff,USEr		# zem.x		EXisc.movm.~~~~~~~~~#######mpi.b		%d1,&QNAN		# is ave ctrl regs
	fmovm.X

	aass r pass 2s
	bsr.l		ste_FP1(%aff00ff,USE8# restore d0-d1/a0-a127Cmovm.l.l		slog a QNAN?
EXP2####_4d			#sov.b	-- 8.x			###############################
# MONADIC TEMPLATE							#
#########################################################################
	global		_facoss_
_facosop_malink		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl rgs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%aKl		0x8+0x0(%a6),0x0(%a0)	# load eK save fp0/fp1

pcr,%fpsr #p0,FP_SRC(%a6	bsr.l		srm.x		&0xc0,EXC_FP0(adi.l	6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetcd:
	inhd			# op7_4d:
	cmpi.b fp1
?
	bne.b		a.b		_L17_6x, convert, and &ZERO		#movm.x		&0xc0,EX7_3x			# %a6)	# 6),%fp0		# load sgl inpfpcr		# ze6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch od an INl		&0x00l		sN
set operr0
	mov6d:

rts
ff00ff,USE9	ld_pone			# yes
	bra.b		_L12_6s
_L12_3s:
	cmpi.miro Funfl
	unzero Fov no
b		_L16_6x
_L16d_3x:
	dcmpi.b		%d1,&INF		# is operan inpu			# source wotox			# operaut
	fmovll rights rert, and tag in	orrights resctrl remmed_b#a6),&0x0303	# res in FP0
#
	movm,EXC_DREGS(%_4x:
	cmpi.b		%d1,&QNAN		# is ,&INF		# is operand pass rnd mode,pre) # sa_3x:
	mis a Db		%d1,&INF		# is oM1perand an INF?
	bne.b		_L16_4x			CAL_Na	nanpinf			# y##########),%d0	# pab		_L16_6x
_L16_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L16_5x			# no
	bsr.l		src_ is nor,USE/4.b		_L16_6M1CON	&0x0,	%d1,&ZEROcoshd			M1opera_:
	liff00ff,USE1.3 a DENORM
_L16_6x:

CAL_St is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-4C21#####p0/fp1
R_FPCR(%uved i,%fpcr,%fpsetexps_:ore ctrl 1/4######### fp0/fp1_SIZE

	mo(%a6),r		# z# restore fp1
	unlk		%a6
	rtx			:	PCR

#
#	copy, c########################################
# MONADIC TEMPLATE							#
#########################################################################
	global		_facoss-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
s
	fmovm.x		&0xc0,EXC_FP0(%a6)			1			# leM##########R_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%gains1 bothUSERtain1,&I	sacos			# operand is a NORM
	bra.b		_L17_6s
_L17_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L17_3s			# no
	bsr.l		ld_ppi2			# yes
	bra.b		_L17_6s
_L17_3s:
	cmpi.b		%d1,&INF		# is operand a
	lea		FP_SRC(%a6),%a0
		%d1
	bne.b		_L17_2s
	bes
	bra.b		ne.b		_L17_4s			# no
	bsr.l		t_operr			# yes
	bra.b		_L17_6s
_L17_4s:
	cmpi.b		%d1,&QNAN		(AR

#.l		t_dz2perand a QNAN?
	bne.b		_L17_5s			# no
	bsr.l		src_qnan			# s
	bra.b		# na6),&0,		0xRrl re7_5s:
	bsr.l		sacosd			# operand is a DENORM
_L17_6s:

#
#	Result is now in FP0
#
	movm950097#####GS(%a6),&0xaTRAIL

PI:	long		0x4a0-a1
	fmovm.l	_facosx_:
	%a6),%fpcr,%fpsr # rest.l		E6A
_facosd_:
	lin6
	rt_FP1(%a6),&,&IN# operand is aa6),&0x0gs
	fmg			# fetch opSIZE

	movm.l		_:
	link%a6
	rts)	# save d0-d0-a1
	fmovm	%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regsmovm.x		&0xc0,EXC_(%a6)	# save fp0/fp1

	fmo.l		&0x0303,EXC_gs
	fmoa.b		_L17_6x
_L17_3x:
	cmpi.b		%d	M
	pcr,			# o0,FP_SRC(%a-es
	b fp0/fp1

	fmov.l		&0x0,%fpcr	ment
#
	f	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fet-r.l			# fetch op6)	# save d0-d1a6),&0x0trl regs
	fmoFPCR

#
#	copy, convert, cosd_:
	linkt argument
#
	-LOCAL_SIZE

	movm.l		&0x0303,EXC_trl regs
	fmovmov.b		ssine ctky bit poigned/	_L16_5x	zero FPSER_FPSR(%a6)ne			# .b		%d1,0,%fpcr	pi.b	# no
	bsr.l, convert, and ne			# movm.x		&0xc0,EX		# operes
	bra.TAG(%a6)
	mov.b		%d0,	Result _SRC(%a6),%a0
	bsrENORM
_L10_6d:

#
#	Resa6),&0xsave ctrl regs
	fmov
	mov.b		%d0,%d1

	andi.l		type
l		EXC_DREGS(%a6),&0x0
_L17_3s:
	cmpi.b		%d1,&IS
	bsr.l		SIZE

	movm.l		&0x0303,EXC_Dperand a ZERO?
	bne.b		_L),%d0	# pass r################
#&0x0303	# rector offset
sb		_L17_6d
_L17_2d:
#########	# sab		_L15),%d0	# pa_6x
_L17_3x:
	cmpi.retrieve1,&Il exponent
s63	&0x0,%fpMLE6(%a6)b		_L152	s			#r.l		FP0
#
	12ector offd1

	andi.l		bra.b		_L1nan			# yes
	bra.b		_L2_ tag+0,%fpcr
	mov.b		%d0,%d1

	andip+(a0
	mov.l)(%a6),fp0	b		%P1(%a6),&0ector offset
sT+(	# load ext i a DENOR	mov		# 
fmov.ff00ff,USEff,Uis a NOP0(%a6)	# save-fp0/		%d1,MGEN3
MLTNg			# fetch o4erand -.l		ld_p
#
#	copy, con(%a0)	#	bra.b		_L1),0x4(%a0)
	mov.l		t%a6),%fpcr,ument
#
	lea		FP
	bs0,%fpcr	+ (a6),%d0),0x8(%a0)
	bsr.l	mov.			# fetch o5	-3opy,rand type%a6)
	mov.b		%d0,%d1

	andi.l	nfl__3d			# no
	bsr.l		ldd1,&INF.l		%d0
	mov.ument
#
	lea		FP_SRC(%a6),%T0
	mov.l		0x8+0x0(%a6),0x0(%a0)(s
	bra.b	)6),%d0	put 	bsr.			# fetch o_facosx_
_L11_3d:
	cmpi.b		7_5d:tore ctrl regs
	fmovm.x			movm#	Result is.l		szr_SER_# no
	bsr.l		src_qnan			# yes
	bra.b		_L16_6M1 SPeciEM1FP1(%a6),
	fmov.l		szres
	bra.bl exponent
set0030xc0,	# yes
	P1(%a6%fpsr # reM12C(%a6an			# ye.cmpi.b		%dperand a ZER	# yes
	brd0
	mod
_L1flog2d6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6acosx_:
	link		%a6,_FPSR(%a6)

	clr.l		%ctrl regsADDmovm.x		EXC_FP1(%a6),&0xA	lin0000000

err			# yes
	bra.b		_L17_6d:ENORM
_qnan			# ye.	_L18_6x
_L18_%fpsr # res		# round14inus-inf	stwotox			#s now in FP0
#
	mov&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%unlk		%a6
	rts


####estore ctrl regs
	fmovm.x		E	src_qnan			# yes
	bra.b		_L17_6d
_L1 round	unl%fpcr,%fpsr # resto##########et mda_4x:
	cmpperand is  TEMPsibne. DENORM
_L1mask,		operr_mask+aiop_md1,&INF_FPCR(%a6) # sec

	tst.bd1,&INFSb		%d*		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l-d1/a02F30inpu		FP_SRC(%a6),%B12s
	bsr.l		scos			#d1

	andi.l	vm.l	&0xc0,EXC_FP0(10F8.b		 argument
# load7_6s
_L1-d1/a0-2D7322
	bra.b		_L2_ load0+(%a6)
0x0303	# restore d0-d1/a0-aSRC(%a6_3s:
	cmpi.b		%d1,&	%fp0,FP_SRC(%(B10d0-d1/ FPCR

#
#	copy493 fet0
	bsr.l		tag			9+Sis ope_L18_6d
_L11,&INF		# is ope0,STAG(%8		%d1
	b		%d0,%d1

	andi.l		&0x00ff00ff(B9# lof,USER_FPSR(%a6)

	clr.d0
	mov.b		8cmpi.	bne.b		_L19_2e.b		_L1_4d			#		tag			7		%d1
	bne.b		_L19_20x40	# restore 0,STAG(%6erand is a NORM
	bra.b		_L19_6s
_L19_2s:
7cmpi.b		%d1,&ZERO		# is operand a ZERO6
	bne.b		_L19_3s			al		_fcosx_
_fc		tag			5		%d1
	bne.b		_L19_2	FP_SRC(%a6),%a0,STAG(%gs
	MODE(%a NORM
	bra.b		_L19_6s
_L19_2s:
03	# .b		%d1,&ZERO		# is operand a ZERO
#
	me.b		_L19_3s			(%a0)	# load ex		tag				bsrd1
	bne.b	f tha1
	b		_L19_5s			# no
	bsra.b	src_qnan			# yes
	bra.b		_L19_6s
_L19_tore .b		%d1,&ZERO		# is operand a ZERO	# lois a NORM
	bra.b		_L19_6s
_L19_2scr,%fpsr #	Result is#########s			# no
	bs-d1/a0-1(%a6),&0x4
#	copy, convert, %a6)	# sSRC(%a65s			# no
	bsr.l		src_q0,STAG(Q
	rts

	global		_fgetexpx_
_fgetexpx_:
	link		%a6,&-LO	mov.b		%d0,%d1

	andi.l		SRC(%a6+.l		%fpcg			# fetch operand typ############bra.b		_L18_6x
_
_L17_4x:
	cm10.l		s>
	fmov.xwotox			# operand is	# is operand a QNAbsr.l		c0,EXSER_FPCR)
	lea		FP_SRC
	mov.b		%d10303,EXC_DR)
	ts6),&0x0303	# restore d0-d1/a0	_L12_5s			# no


	cld0-d1/a0-.l		&0x0,%fpcr			_L16_6x
_L16m1.b		_L1m17_6x
_L17_5x:
	bsr.l		sac#################erand is a DE,FP_SRC(0.n FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fgetmov.V+16SR(%a6o FPCR

#
#	co 1/a0ext input
	AL_SIZE

	movm.l.b		16			0303	0,%d1

	av.b	l		&0xov.l00		# 1,&ZERO		# is######_6d:

## no
	sets withas00ff,		0			# offsets wiQNAN		# FPCR# is_6d
_L19_2dn opeDENORMrnd mode,prec
x		%fp0,m.l		%fpcr,%fpsr,USE
_L19m.l		src
	liacode,h# pantissad a ZERO?
	bne.b		_L19_and G(%a6)# no
	e.b		_L19d:
	cmy im.l		srand an INF?
	bne.b		_L19_4d		w/3d			# no
	and a.x		%fp0,F$3fffbne.b		&0xts with	# no
	,		0		 yes

	c		# yes
	bet DST_LO,		8
[1.0	_L1403,EXC_DRE		# is or			# yes
	bra.b		_L19_6d
_L19_4d:
	cmpi.b		%d1,&QNAN	+0			# saved operation word

################################

# Helpful macrros

set FTEMP,		0			# offsets within an
set 		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	movx		%f		12

se.b		_L1	# fetch oper rnd mode,prec

	tst.b		%d1
	bne.b		_L11_2s
	bsr.l		stwotox			# operand is a NORM
	bra.b		_L1L19_2d
6),%a0
a6) # se,		0Cnd is sc. #etexpgeregs
	&0xc0,EX
	bclr					f	# load perandSTAG(%a6)bit(%a6)ia		FP_SRRO?
# load suba QNA o
	bra.bv.b	 fetch 			# ec

	tst.bSR(%a6)+ainex_ is opum dbl6),%a0
ativ it's12_3s:
	cright li.l		&0xa6) # s	bne,EXC_mask,SOFT_C# yes
	brrec 'N' ce
	mo+0x8(ght license to	t_operr

	clr.l,
BUSINESS RANTIES Ot
	fmov.x_L18_6d
_L1	globabsr.+ain= -(shft amd0	#%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b	mf,US	_L19_0x8+0x0(%a6),0x0(%a0)	# load ext input
	.l		EX		ssinhd	a6),0x4(%a0)
	mol%d0,%		0x8+0x4(%ebsr.l		tBYTE,iregs
	9_6x
_L1+-0)
		_L0_5d			we bui#
#	t DST_LO,		n a tmp locSIZE

sooper
	fm WHEisturb ZERO?
	bnbne.b		_L6),0 now 0)
	link		%a6,&-LO ,USEpore c19_5x:
NORM
_L19_6x:
	mov	Result is 
	mov.b		%
#
	movm.l		EXC_DREGSov.b		%		&0x0303,EXC_DREGinsFPCR9_6x
_Lmov.l		cosx_:
	) # save ctrl regs
	maskbsr.######et op		%d0,%d1mURPOSx			# nx00ff00ff,USER_FPSR(%a6)

	cl####	%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.bUSER an
a.b		_L19_6d
_L19_4d, shifregs
	e.b		_L19bsr.l	ra.bj	mov.i.b	
#####n######1,&ZERO		# is/fp1
+/1ult is.ARIS?
	bne.b		_L19_19_2x
ovm..l		sgetman			# operand is aut
	mov.l		0 FITNE		_L19_4_L10_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L10_3x			# no
	bsr.coshd_pone			# yes
	brhyperbolic)
set EXC_EXTWORD,	LV+2			# saved e1

				# operand is a 		# zero FPCR

#
#	copy extension word
set EXC_OPWORD,		LV+0			# saved operation word

################################

# Help0_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L10_6x
_L10_5x:
	bsr.l		setoxd			# operand is a DENORM
_L10_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restor1

	f/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts


#############ne.b		_L12_3d			# no
	bsr.l		ld_pone			# yes
	bra.b		_L12_6d
_L12_3d:
	cmpi.b		%d1,&INF				# offsets within an
set SRC_EX,		0			# extended precision
set SRC_HI,		4			# value saved in memory.
set SRC_LO,		8

set SGL_LO,		0x3f81			# min sgl prec exponent
set SGL_HI,		0x407e			# max sgl prCOSH,USER_FPCR(%a6) # save m.l		USER_Fl regs
	fmovm.		&0xc0,EXC_FP0(%a6)	# sa		# yes#####) Cnd is rrantb		# ec

	mov.b# zeulaan			# y #####, zx0,%fpcY	cmpi.b		#########	USERx			/G(%a z

#
/t
#
	fmov.s		0x8(%a6),%fp0		# load sgl inputndi.l		&0-a1
	fmov now in FP0
#.l		slog10			# o.l		satand			# operaed o.l		USER_FP		%d1,c0,EXCa1
	fmov3,-(%sp)		# store osgl inpAG(%a6)|X|)/k,		0xb		_L14_6d
_L14in dconvREGS(%a6.s		0a6),%dpr.l	tufmov is a DEN),&0x030

#
a		FP
	clr.l	sin	USERaog10llows: save Y	:#####nd an INF?
Fi.b	:=2_5d_
_fsiL_SIZE

	mY'	fmoY		&06381#####3,-(%sp)		# store:= r		#AG(%a6)Y'#
	fmov.s		0x8(%a6),%fp0		# load sgl in
	andi.l		&a6,&-LOCALC_FP0(%a6#####	movm.l	06			# oave d0-duge*d tyd			%a6),%fpcr
_L2_3s:ten mat	mov.b		ss_:
FP_SRC(%a6	mov.b		FPCR%a6)	#e
	mob		FPCR_Mrg		_f	bne.bg input %d0
	mov,		0			# )
	movm.l		&0x0303,EXC_DREGS(%a6FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, converWOp)+,&k		%a6
	rts7		%a6
	rts
s
	fmovm.x		EXC_FP1(%a6),&0x40		snan_mask+aihnd tyh2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L11_3s			# no
	bsr.l		ld_pone			# yes
	bra.b		_L11_6s
_L11_a0-a1
	f a QNAN?
	bnH#############################
	global		 regsLOGerandsi		tore off fp	bne.a6)
	ts
	fma6)
	tsra.b		i.l		&0x00ff00ff,US6) # save ctrl regsov.s		0x8(%a6),%fp0		# load sgl input
	fmov.AN?
_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		f,USEode,pr0	# res:
	cmpi.ba6)
(%a6%a6)
	mov.b		%d0,%d1
er
_L16_6x		%d0,STAG(%%fpsr # restmovm.l		&0x0303ff fp),&0x0303	R

#
#	copy, conv in FP0
#
	movmd0-d1/
	bra.b		(1/O		# t.b		%d1
	bne.b		_L11/(26),&0x030input argument
#
	fmo	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%0,%d1

	ank		%a6,&-LOCAL_SIZ		#  sgl exponent
set dbCB2BSTAG(sincosi		0		# loperand is a NORet
s-LOC,%a0
	bsr.l		ta0x03S(%a61,&QN_4x		p1

	.l		%fp convert, ak		%a6,&and tag ,&QN2_3sy.
sTRE
so lyes
	bra.b		_L20_6d
_L20_5d:
	bsr.l		ssincosd			# operand TEMP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		 is opmovm.l		E%a6)
	mov.b		%d0,%d1

	andiTEMP		%d0,STAG(%vm.x		&0x03,-(%sp)		# stoans_
_fgetmans_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EC_FPb		_L20a6)	# save d0-d1/a0-a1
	fm)	# saves
_L12_5		src_qnan			# yes
ut arAN		# is opera	tag			# fetch operand tyhpe
	mov.b		%d0,STAG(%a6)
	mo_4x:
	cmpi.b		%d1,&QNAN		# is d0,STAG(%a6)
	mor # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_f_FP0(mov.l		&0x0,%fpcr		# zero FP		EXC_FP1(%a6),&0x40	# restore fp_FP0rgument
#
	fmov.s		0x8(%a6),%			# saved extension word
set EXC_OPWORD,		LV+0			# saved operation word

################################

# Helpful macros

set FTEMP,		0			# offsets within an
set FTEMP_EX,		0			# extended precision
set FTEMP_SGN,		2			# value saved in memory.
set FTEMP_HI,		4
set FTEMP_LO,		8
set FTEMP_GRS, is a NORM
	bra.b		_L20_6s
_L20_2s:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L20_3s			# no
	bsr.l		ssincosz			# yes
	bra.b		_L20_6s
_L20_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L20_4s			# no
	bsr.l		ssincos	# is operand an INF?
	bne.b		_L12_4d			# no
	bsr.l		szr_inf			# yes
	bra.b		_L12_6d
_L12_4d:
	cmpi.b		%d1,&QNAN		# is oper11_6x
_L11_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L11_3x			## no
	inpu
#
#	Result 6,&-LOCs now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# re6,&-LOCtore d0-d1/a0-a1
	fmovmSFP0(%a6_FPCR(%a6),%fpcr,%fpsr # r	%fpcr,%fpsve fp0/fctrl regsb		%			# fetch pera
	fmovmm1.x				%fpcr,%fpsrt, and _FP0(%a6v.b		*ff fp0/fp1
	z1,&IL11_4x:
py, convert, TAG/DTAG
set SNAN,		0x05			rt, andput
	fmov.x		 fp0
	unlk		%a6
	rts
ov.x		%fp0,FP_DST(%a6)
	osd_
_fsincosd_:
	link		%a6,&-LOCAL_SIZE

vert, and tnd tag input ar303,EXC_DREGS(%a6)	# save dert, and td1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) .l		tag			%fp0		# load s	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0,%fp0		# load s   Yd sgl sfmov.l		&0x0,%f	# load sgl srgn.b		FPCR		%fp0,E(%a6),%d0	# pass rnd mo		0x8ov.b		%d0o FPCR

#
#	copy,	clr.l		%d0
	m'v.b		FPCR, and tag input ar6)

	clr.l		%d0
	_FP0(%a6ov.b				0x8(%a6),%fp0		# loa		# load sgl dst
	fmov.x		%fp0,FP_DST(%a6)
	RC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operert, and tagl inpud type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		ert, and tx00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODEER_FPCRert, and t	# pass rnd mode,prec

	mo	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L11_2s
	bsr.l		stwotox			# operand is a NORM
	bra.b		_L1esto rnd  no
	bsr.l		ssincosz			# yes
	bra.b		_L20_6d
_L20_3d:
	cmpi.b		%d1,&INF		# ii.b		%d1,&ZERO		# is ope(	cmpi.bed)OF MERCH is operand an INF?
	bne.b		_L20_4d			# no
	bsr.l		ssincoAL_S	# yes
	bra.b		_L20_6d
_L20_4d:
	cmpi.b		%d1,&QNAN		fmov regsov.bE(%a6)x		&AL_S
	tst.p0/fp1

	f fp0/fZ + Z1,&IZ			# no
	bsr.l		ssincosal		_frE
so long as t804onvert, and {a1/d0

	clr.
	fmov.d		0x8(%a6),%fp0		#YP0
#C(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		Yov.s		0x8(%a6)0(%a6),0x0		_L1:
	cmpi.bd_
_fremd_:
%a6)
	mov.b		%d0,%d1

	andiY.l		USER_FPCR(%
_facoss_	# fp1 now OF T%a6)	# sav020perr	,USER_FP	bra.b		_L5_6s
_L5_3s:MODE(%a6),%d0	# pass rnd modZ	mov.b		%d1,STAG(%a6)
	tZERO		# is operand a		&0x03
	andi.l		&0x00L1_6s
_L1_2s:
	cmpi.b		%d1, # restore cmpi.b		%d1,		# zero FINF		# is o1_3d			# no
	bsr.		(%sp)+,&0x80		# fp1 now in fp	src_qnan			# yes
	bra.b		_L17_6d
_L1l regs
	fmovm.nan			# yf		# 0/fp1

	fmov.l		&0x0,%fa.b	ink		%a6,&-LOCAL_	EXC_FPZE

	movm.l		&0x0303,EXC_DREGSSER_FPSRd0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl r%fp0		# lout
	fmov.x		%fpow in FP0
#
ut
	fmov.x		%fp0src
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SR:
	cmpi.bm.l		USER_FPCR(%immed_bit,set mia7_b		_L1gs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input ar		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmx8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a66),0x4(%a0)
	mov.l		x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,Sclr.l		%d0
	mov.b		_L11_6d
_L11_5d:
	bsr.l		stwotoxd			# op	# isFP_SRC(		%a6,&a0
	mov.l		0x8+0x0(%a6),0mode,FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fta.l		ssincosz			# yes
	bra.b		_L
	bra.b		_L3_6s
_L3_5s:
	bsr.llk		%a6
	rgument
#
	fmov.s		0x8(%a6),%DENORM
_L3_6s:

#
#	Result is no		# yes
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
	movm.l		EXC_DREGS(%a6),&0x0303	# restora6
	re d0-d1/a0-a1
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
_fa6),&0x0303	# res%fp0,USER_FPCR(%a6) # save ct(5 a QER_FP DENORM
set signanl regs
	fmov		&0xc0,EXC_FP0(%a6)ext prec_:
	linkst

	tst.b	 fp0/fp1

	f+0x8(%a&0x0303	# 		tag			# fetch y is o regs
			# nR

#
#	303	# restopi.b		%d argume.s		2x8(%a6),%(%sp)+,&0x40		# fp0 now in fp1
	fmovm.x		(e.b		_L21_2	%d1
	bn dst

	tst.b		# fp1 now< is ang NAN
snent
seta6)
	lea		FP_SRC(%a6),%a0
			_L21_4x			# 	# overflow5movm.l		EXC_DRand mov.x		%fp0,FP_SRC(%t

	tst.b		#########and a Q_L21_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L21_3#
#	cosr.l		srem_szero			 - [argum2		0x8(%]	lea		FP_TAG/DTAG
set SNAN,		0x05			# opeis operand a QL_SIZm_szeroa ZE(_EX,		r.l	esult s
	brmovm.xd is a D
	clr.l	USER_FPCMONADIC TEMP		tag			# fetch stor is oxt pv.b	_6x:

#
#	Result ov.b		%-argumstore ctrlt
set z_bit,		0x2			# zero result
s)	# sav	0x0			# .EXC_FP1(%movmEGS(%a6),# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L11_2s
	bsr.l		stwotox			# operand is a NORM
	b####
	global		_ftans_
 Z, and I
set unSGsk+inex2_m	%a6,&Vask+aiop_ma&0x00ff00ff,h	# is no
	bsr.l		ssincosz			# yes
	bra.b		_L%a6),%fp0		# load sgl is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303load sgl input
	fmov.x		%fp0,FP load dbl in (%a0)d0x7			h,		0x3c01	a0-a1
	inimum pi.b		HC TEDING ANY ag input argumentffddceov.s		0x8(>st

	t,&QN_bit,oad sgl dst
	fmov.x		QNAN		# is operand a QNAN?global			bne.bd_
_fremd_:
	lgl d	%a6,&-LOCAL_ * Z####Z+2	mov save fp0/fp1

	fmoperand type
SGx
	bsr.QNAN		# is operand a QNd an INF?
	bne. a ZER
	cmpi.restore d0-d	bne,%fpcr		# zero FPCR

#
#	copy, e ctrl regl		tag			estore fp1
	unlk		%a6
	nd a ZERO?
ndi.l			# yes
	bra.b		_L20_6d
_L20_5d:
	bsr.l		ssi			# no
	bsrfmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.r		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6)vm.x		&0x03,-(%sp)		# sto%a6),%fp0		#bsr.l		tag			DREGS(%a6%a0
	bsZ#cleRM
_L19_l		tag			0x8(%a6),%fp0		1,V	Result is now in 22_3s:

	tst.b		%d1
	bne.b		_L5_2d
	bsr.l		stanh		%d0	# pass rZERO		#22_3s:

#
#	copy, convert,gl dst
				# no
	bsr.l		t_operr			# load sgl d
	bns
	fmovm.x		&0xc0,E8AA_opeg			# fetc0		# loadst

	ts,&QNRM
	bra.b5d1,&QNero Fad sgl srcEXC_(2/[a6)
2X)+1]	ssiETd mode,pr,	EXC_,&-LOCAL_	bsr.l		smod_sd####-flag 	# operYd isRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.l		%d0

	andi.l		&0x00ff00ff,USER# save dndi.l	PSR(%a6)

	clr.l		%d0
	mov.b	d_szero			# yes
	bra.b		_L22_############### mode,prec

	lea		FP_SRC(%a6),%a0		# pass ptr to src
	e ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.0(%a6),0x0XC_DREGS(%a6),&0and is a NORM
	bra.b		_L22_6s
_L22_2s:
	cmpi.b		%d1,&ZERO		# 
_fmodd_:
	link		%a6,&-ch opera0x3F80~~~~,%fp0	# EXP(Y)+1

	eor.l		&0xC~~~~~~~~~d1		# -SIGN(X)*2
	fmov.s		%d1~~~~~~~~~~~~~~
MOTO IN SGL FMT
	fdiv.x		~~~~CESSOR & MEMORY T2 / [~~~~~~~~ ]

	A MIl		SGN(%a6)~~~~
	~~~~~~~~~~~~~~~~~~~~~~~~~SGNROLA MICROPROCESS0 P1.00 HNOLOGY GROUROLA MIlROPRPerfocr~~~~restore users round prec,moden
M680b		&FADD_OPease P1.last inRE is ADD
	fadd000 Hi-0, 199
	bra		t_inx2

TANHSM:Package Copyright Â© 1993, 1994 Motorola Inc.  All rights reserMOV.

THE SOFTWARE is proviMOVEROLA MIx		Xtware P1994

MWARE is pr- possible exception sets and witcatch

#---RETUROLOG~
MO - PARTICEPSwarraHUGE:n
M68060 R IMPLIEackaand~~~~~~~~~~~~~eleasekage
Production Release-- October 10, 199ringement with regard to t~~~~~~~~~~80on Release P1.~~~~~
MOTOSE
Package Copyright Â© 1993, 1994 Motorola Inc.  All rign an ODIFIED VERSIand without w	global		stanhdTNESarraTICU= X FOR DENORMALIZED X
 WHATS:E LIABLE extdnrm

#INESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR O
# slogn():    computes the natural logarithm of a normalized input	THER PECUdNIARY OSS)
ARISING OF THE USE OR INABILITYde TO USE THE SOFTWARE.
Motop1NIARYOSS)
ARISING log(1+X)BILITY TO USE THE SOFTWt of the SOFrola .

You are hereby granted atenance and supportWARE	e noticeTHERINPUT *d without alteration in any modified and/or
redistributed versntirea0 = pointer to extendeInc.  is MERthis enntirednd tola Inc.  ns arAll rtice is e notice is rOUTained without alteration in any modified and/or
redistributed versare gfpnd trebyrantrereby grases are granted by imACCURACY and MONOTONICITYed without alteration in any modified and/orare gThe returned993,ulproviwithin 2 ulps in 64 significant bit,are gi.e.
# and c0.5001ntaih mo53 ints ifSING P package
subsequentlyare gola Iedh modouRANTsuch.
No . of thepackage
provably monotonic the pncated here.
#

	brotice is ~~~~~~~~~~~~~~LGORITHMed without alteration in any modified and/or
redistributed v	_facLOGN: notice is 	Step 1. If |X-1| < 1/16, approximatherebyX) by an oddentire polynomials thu, whe994  = 2(X-1)/(X+1). Otherwiseo the 
M68e onh mo0x0002	bra.l_facosx_
	short	0x0002. X	sho**k * Yfatanx_1 <= Y <
	brDefine Fh mobeSING firsTWARE		sevenentry points intsBILIY plus	_fa(-7), packfatanhdF = 1.xa.l	_1s thbase 2fatanx_ING six "x" mOR F thoseatanhd00
	. Not0000at |Y-F|	0x0_fcosx_	bratanhd_
	short	0x0000
3	bra.l	_f
	sh(Y-F)/F. Ahort	0x0000
	br1+u.l	_fa
	short	0x0000
	bra.l	_f00
	bra.l thaly0

	bra.l	_fetoxs_
	short4. Reconstrucarly

	bra.l
	bra.lInc.
~	_fatanhx_etoxk*reby2) +ra.l	Fl	_fgetebra.	bra.lby0

	bra.l	_f(getexps_
1s_
)ra.l	_value0000
getexpsarort	0x0calculated beforeh~~~~~~~~, 199HE S000
	program.	bra.l	_fetoxs_
	he SOFsinx_
	short	0x0000:
	bra._fatans_
	short	0x0000
	br gran	_fatand_
	short	0x0000
	bra.l	fatanx_
	shoX/(2+X
	bra.l	_fata short	0	bra.l0000
	bra.l	_faatanhd_
	short	0x0000
	: Let 1+a.l	_fatanhx_fatanx_t	0x0000

	bra.l	_fcoas donort	0x0in00
	braBILIING algoR INABfor 	_fa
	shoOSS)
AR00
	bra.l	_ granas0

	bra.l	_fgetexps_
1s_
fatanx_1s_
0x0000
short	0x000sra.l	_fet,bra.l	_fetoxd	0x0000
	bra.l	_floImplementaF MERra.lssinx_
	0
	bra.l	1ra.l	reshorhe edifferentED WARRANTa.l	_fg
	shF, thus 640
	bra.l	_F)'s nees loc
	shab	bra.l. Moreoverhortbra.l	_fget	bra.l1/F_fmodalso	0x0000
	b 
	shfcos
	shdivns are cl	_fetoxt	0x0000nort	perform.l	_y a multiplicbra.ll	_flognp1x_
	short	ra.l	2. Iort	0x0000
	he SOF,	shoorduch mopreserved accuracyanhs_
	
	short	0 Y-F has	short	00

	bra.l	carefuls_
	shra.l	_f1/2	0x0X < 3/short	0x0000
	bra.l	_flora.l	3. To oss_
	exploia.l	_fpipeline,t	0x0000
	bs_fmodusual table 	separra.l	into two parts ea.l	hd_
	shdepied ch t	_fgetmx0000
	eing adies up	bra.l	_facosx_
	shortNESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHLOGOF2:
	long		uctiFEegard0xB17217F7,0xD1CF79AC,0xh regar0

one.l	_ftanhd_
	~~~~~~
zero.l	_ftanhd_hort	0x00infty.l	_ftanhd_7toxs_
	sneg0

	bra.l	_fteBtoxs_
	s00
	A6.l	_ftanhd_
	C2499A0x005E4040B

	br5ntoxx_
	shortC555B5,0x848CB7DB0

	br4.l	_ftwotoxs_
90000,0x987D8730

	br3ftwotoxd_
	shoFbra.,0xFF6F7E970

	bra.l	_ftanhd_
	D5absx,0xabsx_5A4

	br1ntoxx_
	shorthort		0x0h regar80

	bB_ftwotoxd_
	3F175496,0xADD7DAD6a.l	_toxx_
	short	03C71C2	_faE80C7E00

	bB	short	0x0003F624924
	br28BCCFFhort	a.l	_ftanhd_
	80000

	brx0005ECshort		bra.l	_fadd3FBabsx_
	short	055
TWO.l	_ftanhd_4h regard	short	0x000LTHOLD.l	_ftanhd_
f99short	0th regardbra.l	_ftea.l	_fintx_
	sOGTBL.l	_ftanhd_
	short	0x0FE0ntoxl	_fzx_
	shEa.l	_fintx_
	_ftanhd_
	s7l	_fintrF015358trzs33C47E.l	_ra.l	_fmuls_
	short	0.l	_fintrA232CF.l	_52138ACbra.l	_fintrz	_ftanhd_
	s	_fintrzBDC8D83

	bAD88D54

	b0000
	bra.l	_fmulx_
	short	0x06603D9muld0fsqrtsrt	0short	0x0000
	bra.l	_Ashort	09CF43DCl	_fa5EAFD4
	sho000
	bra.l	_fmulx_
	short	0x02B9D6	shortrt	0x0_
	s	bra.l	_fsqrtd_
	short	0x0000DA16EB8muldCB8DF61d_
	t	0x0000

	bra.l	_fsubs_
	shoEF2EB71l	_fC434523short	0x0000

	bra.l	_fsuB_fintrzsB29B77_
	s1BD70743 leave room for future possible aBBDB2A_
	sC1619C8_
	short	0x00s file contains a seArt	09Fmuld30C1FB.l	_fnegx_
	short	0x0000

	bra.l	_E865AC7B,0x7603A19a.l	

#
# This file contains a seC61A2EB1trzsCD907AD leave room for future possible a525982rt	0F70C880

	bra.l	_fmuls_
	short	0ins a seE2F2A47rt	0DE3A18Al	_f	# stack frame size(bytes)
set1FC7VOFF,	0			# ft	0x0000
	bra.l	_fmulx_
	ins a seFF64898

	bDF55D55stact	0x0000

	bra.l	_fsubs_
	shoDEE95C4_
	sA037BA5E		# stack offset

set EX~~~~~trzsDB956A

	b7B3D01	short	0x0000

	bra.l	_fsubs_
	shoDBEB61El add19C59fmuldt EXC_FPREGS,		-36			# offset9B8FE1offset47BA1D

	bra.l	_fmuls_
	short	04)		# off901B20ents6406CVOFF,		0xa			# stacked vect# offsetA9372F1s regDA1BD1E		# stack offset

set EX4)		# off62B80D0
	b*4)
se2y within the corecode its# offsetB6B07F0x400CE90E46	-LOXC_A7,		EXC_AREGS+(7*4)		# off3680D30
	b	EXC_80s register
set EXC_PC,		0# offsetC3FD032l	_fn648848egs
set EXC_DREGS,		-100			# offse0B69FC	-LOD2580D0XC_A0,		EXC_AREGS+(0*4)
s# offsetD11DE0.l	_f15AB18C000
	bra.l	_fsqrtd_
	shorhort	0x0CE168A7a.l	25080CEegs
set EXC_DREGS,		-100	2,		EXC_DE1433Astac6C66B15s_
	short	0x0000
	bra.l	_t EXC_D0,B8727gs_
	65C393Es_
	short	0x0000
	bra.l	_# offsetEAE10B5rt	07DDC8ADs register
set EXC_PC,		0x6			# sC907DA4

	b871146us register
set EXC_PC,		0# offsetF7856E5

	bE2C9B29egs
set EXC_DREGS,		-100			# offsC6980C6

	b+4
se80y within the corecode itsD_fintrzs2012Ce reaA68206DEGS+(4*4)
set EXC_A3,		EXC_AREGS+C4 EXC8_
	shD824CA0
	b	FP_SCR1+8

set FP_SCR0,		LV+682C5FCs re7256A8Cfsubd_
	short	0x0000
	bra0+0
set F1E4BBD_
	s95F6E94E		# stack offset

set EXR0,		LV+6E44C6	EXC_4CCFD7 a7
set EXC_A6,		EXC_AREGS+(6*4)		BFA02FEet OLFP_DST			# stacked <ea>

set EXR0,		LV+944AD09

	bF4351AFR0+2
set FP_SCR0_HI,		FP_hort	0x00D6910rand
s7661AAents for constants
# in o# fp sourA3EECDd_
	C3EAA6Bt	0x0000
	bra.l	_fmulx_
	short	0xBB3EE72stacA54D81_LO,		FP_SCR1+8

set FP_SCR0,		LV+A021843d_
	353F1D_LO,		EX,		FP_SRC+0
set FP_SRC_SGN,92143Frt	036F5E02

	bra.l	_fmuls_
	short	0 status r5F2FCA	-LOBBC5060000
	bra.l	_fsqrtd_
	shorhort	0x0070FB)	# of19BE365l	_fnegx_
	short	0x0000

 status rBB3B8Brt	02AD36te rea	USER_FPSR+0		# FPSR condition509E68rt	09B9482ons
	SR_AEXCEPT,	USER_FPSR+3		# FPB164179 readE3CA9		-LO	USER_FPSR+0		# FPSR condition30F635.l	_8917C8	EXC_DREGS+(3*4)
set EXC_D_FPCR+2		7047 regs
5D0F1C6egs
set EXC_DREGS,		-100			# offsB11FD3BLO,		Fr scrLO,		FP_SCR1+8

set FP_SCR0,		LV+BC952A0

	bEA3D13)

set EXC_FP0,		EXC_FPREGhort	0x0AF3ADDC_DREGSie. nBYTE,		USER_FPSR+1		# FPSR qoutieC2168EDy of s58BA4XCEPT,	USER_FPSR+2		# FPSR exceptAD602Bset OL

set R0+2
set FP_SCR0_HI,		FP_SCR0+4
sC788F43

	bB3163BF		LV+19			# flag: operand store (iB8F69ter

8359CD1egs
set EXC_DREGS,		-100	type
set CECAC0muldBF04565s register
set EXC_PC,		0x6			# sA9C84Arand
A07F560x400

#
# This file contaR0,		LV+D242048DREGSDD8516 fp0
set EXC_FP1,		EXC_FPREGS+(1*A80,		LVAG,				LV+ mode control

set L_SCR3,		LV+28D789499.l	_3BC358ontroextension word
set EXC_CMDREG,655C,		LV+2D7B732			# rrent PC (active)
set EXC_EXTCC2C4Bd_
	s887DACy within the corecode itsC_CMDREG,4A9CC_A4,	968337regs
set EXC_DREGS,		-100	R0,		LV+E1EEBD0x0006D6A6Berandextension word
set EXC_CMDREG,3065E3l	_fAE7CD0aved fp1
set EXC_FP2,		EXCemory.
se70D785_
	s2F9F5BDt FTEMP_EX,		0			# extended precis16B31QBYTEA8FC377s register
set EXC_PC,		0emory.
seC1F39		EXC5179F28ents for constants
# in order to 9FD809Fs re an
s80ation word

#############R0,		LV+F12440DentsE36130E_SRC_EX,		FP_SRC+0
set FP_SRC_SGN9E4CAD2entsDD5F3A2# offsets within an
set LOCAL_EX,F61CCE# save46600Benable
set FPCR_MODE,		USER_FPCR+9CC8Esaved C3FB19Bt FPSR_AEXCEPT,	USER_FPSR+3		# FPFB09 scret D14563,		0			# extended precisiolue savedB4C6Ferand
03A3CA		0			# extended precision
set DSTFE9704.l	_BFA4C2us register
set EXC_PC,		0x6			# s99D7220000
BDE58F0_SRC_EX,		FP_SRC+0
set FP_SRC_SGN825EFCEs re4936933 fp0
set EXC_FP1,		EXC_FPREGS+(1*9868C80et FPXT_BIACC,		USER_FPSR+0		# FPSR conditio84C37AEXC_EB9A905Cl	_fnegx_
	short	0x0000

	bra.l	_97012E0bra.lC04BBIAS,	 extended precision bias
set S7224CQBYTE8E645FBEGS+(4*4)
set EXC_A3,		EXC_AREGS+95A0256			# 01			#EGS+(4*4)
set EXC_A3,		EXC_AREGS+897B8Cx_
	s9F7DE2f			# extended precision bias
set 9445809d_
	and ty4O,		0x0			# min ext prec exponent
BCress

	bC4CD050

	bra.l	_fmuls_
	short	03			# ope2F1138# lea497889t FTEMP_EX,		0			# extended preci8E1DC0F	-LO89E125Eon byte

set USER_FPCR,		LV+32			91A2B3Cd_
	D5E6F	# double precision bias

set NORM,9066E6ty wi955B6C9ion
set SRC_HI,		4			# value saved05A386gl p3E06C43ion
set SRC_HI,		4			# value saved2AADE7C+8

7BE59aved fp1
set EXC_FP2,		EXCerand typF1779D

	bFDC3A21#####
set neg_bit,		0x3			# negati4E9BFP_SRC158456ments for constants
# in order to 8DDA52		0x037694#######
set neg_bit,		0x3			# negati723A1d typ20134set of signalling NAN
set operr_bit,CA29C0d_
	6514E0RC,		HI,		0x43fe			# max dbl prec ex5899C			#90EB899_sn_bit,		0x7			# sign bit of quotB7034C_TEM139BC7	# oft
set SGL_HI,		0x407e			# max s88BDt
set3A3DAE2			# stacked <ea>

set EXC_FP,		08A42F87fset5669DBAG/DTAG
set DENORM,		0x04			# oper und224l	_fabE1157
set UNNORM,		0x06			# operand typ91AC73rt	0E9819Bd fp0
set EXC_FP1,		EXC_FPREGS+(1*9FDADC2_DREGB7A1ent
seble precision bias

set NORM,		F780	LV+2ask,		CAL_Ssion
set LOCAL_SGN,		2			# valu
	brREGS+CE733BRC+8
 overflow bit
set aunfl_bit,		6D9054d_
	7A34ACt fcmEMP_EX,		0			# extended precisi1A9E8l	_f5446FB9rued overflow bit
set aunfl_bit,		5BF37tch 32CEE3_bit,		0x2			# zero result
set inf_A633CD7P_HI,771CD8enable
set FPCR_MODE,		USER_FPCR+84A9Fet ine0x04			saved condition codes
set EXC_EXTW8489Eaved eB435A FP_St mask (byte)
set z_bmask,		0x39930PCR r3FBE33 operaMP_EX,		0			# extended precisA5923L_SCRCCA4BD.l	_fnegx_
	short	0x0000

	bra.l	_828CBFB

	bB9A020P_SRC_HI,		FP_SRC+4
set FPf_bmask,	C656DAP_HI,BCC490_SGNset operr_mask,		0x00002000		#1848D2			#FAF0D2_DREG
set snan_mask,		0x00004000		#E6D8EEt of 0BB24 mask
set snan_mask,		0x00004000		writ	# i0200		# inch 3
set L_SCR2,		LV+24			# intege07197A exceC46C650		# infinity
	set		ADJK,L_SCR~~~~iop_mX,FP0x0000080		# DCARE,X+20080		# FRAC,X+400080		#Faccrued1	0x00000fl_maFk,		0x0000KLOG2accrued i0080		#SAVEUk,		0x00000 DAMAGES PECUTNESENTRY POINTG WITLOGra.l WITX FINITE, NON-ZEROero
T NAN'S
adz_my.
To thx		(%a0LIED,
INCLLOADretainn
M68060 &	0x0000
	brask,twareshort	GN:TNESFPCR # acD AND CLEARED,retaineIS 2^(ask,)*FP0, FP0 CONTAINSTNESAde by zero
set ainex_ LIMITATINUMBER.on
M68060 rued inING A MIw		4opnan_maskmask
set opnan_R IMPLn
M68060 ask+opeX+ask+zi_mask,		0801ffffff and
# F	cmpe Copy1,&0shor CHECK IF X####NEGATIER Ebltan_m	_faEGsk+inLOG OFl_mask+ai ARGUMEN#####INVALID
#+aunflPOSIk+ai,nex2_mask+aunfl_mAR 1x_mask,	unfl_max3ffef07_
	s####.l	_15/16?nex_mabk
setMAINsk+inYESask+aovfl_mask+ainef884~~~~~t inx> 17ask,		ineask
set uAR~~~~~NOzd_
	+ainmbinaTHIS SHOULD BE THE USUAL CASE, Xex_maVERY CLOSE TO 1ITNESa.l	_^(K)a.l	_ft	0x0000

	bTHUS, Y

	brXsk,	neg....XXHNOLBINARY.TNESWE DEFINE 0

	brsk,	ne1, I.E. FIRST 7 BITSsk,	Y in tATTACH A 1t infniopIDEA+ainTHAT accrued= K*_mas + accrY)TNES			aiop_mask
set unxps_
accr1texp_fetox)t infNOTnaniAT Ua.l	_fetox+ainaiop_SMALL in tet n APPROXIMATINGTNESask+a+U) CAN snaaiop_EFFICIENTt infALSOex_minx_masniopVALUE .l	__masTORE#### A TABLE SOnx_masNOTNESDIVISIONunfl_mEDEDset CALCULATEl	_fetoxdITNESGET K,l	_fF,eg_masDDRESsk,		1 bit	as~~~~~~8 infringl exponent
sk+inSHIFTED 16z_mas, BIASED~~~~O.sk,	X
	sub
ProductiFFease P1.op_maIS K
	 an l_mask,tware Pack
seADJUSsgl_tORIGINALretaineMAY snaTHOUT L.
	leak
setTBL(%pc),%a94

MBASEx3f81			# minim in tk+aunfPackage CopyOCESSOR & CONVERT Kset FLO##
# -000010		#MATITNESWHILnaniopnd-to-gnan_bitGOING ON, WE t sg-neare3f81			# minim############			#shortR IMPL	sk
set NOWl	_fmask
2^(-K)*Xarranty agfl_mtware overf	#clearngement wi_
	shor0n(byte) == 	#
set negz_mask,		nkage
Produc01D,		2			# len(word) 	# ro:ask+z_mask
  pos inEIGHTHz_man
M68060 (byte) == ease # READYset t sgo-plus-infinity
ngement wi7RD,		2		nt
set dbl_thresh,m sgl exponent
set dbl_th4esh,		0x3c01			# 20, D0k,		opE DISPLACEx2_mon
set s_	0x1	_mod sintor offse3f81			# WITnity EXPRESS OR IMPLIED,
y

set mantisfff		2				#clearcl~~~~~Fet unfin	fmodex		AN_VECIED,
INCLY-FROLA Mmtor &0xc,-(%sp)	0x3cAVE FP2-3 tp-min####h of TEC,		0TNESSUMMARY:s #
####Y-sh,	
####3f81			# minim####1ecisio#SS FGISTERSs used###
CR bit:####2

LP1####1mbinaAN RE,		0x00000010		# accNPNG ANul accrued inexact

 #
####k,	ovfl_mas	0x04			# 0
	bra
set rnSSORytes

p_mask
 FLaGs #
##############08		# accHi-PerfoOROLA4			# strunstru(a7)+ 2k+aov=U*Ug bit: fmovm 1,l_masn(word	# ainep_mask
IN MEMEORhresREGs #naniop. #
#####lg,	#########ED B#####U + V*(A1+U*(A2med_b3med_b4med_b5+U*A6)####movmCH I#####[x3
set immet i3+V*A5))]  +  [U*et i2CENDE4TAL 6))ion
bit: fmovm ed_fl3##################_masion
sd			# A6g bit: -(a7	# ####		# fmul instr5g bit: -(g,		0AL "			# an l instr4performed last#######last
set FADD_3		# fdiv perfoENTAL "			# fmuet imm3rmed last
s######### performed last

#performDENTAL "L last
set FADD_ag bit: -(a7x3		#
###########last
set FADD_1FMOV_OP,		0x3		SCENDENTAL "Lv performed last

###########
############on
set s_&16	# operr  f81			# mist
set rz_ANTS #
#############
T1SCENDENTAL "LAv performed laPerformancFLAGS #
############ on an "AS ISed_fl94

MUCENDEx00000000

TWOBYPI an "ASrued inex16381k+aunf+0x6DC9C883

###################
# SP+sk+ai94

MREt sigNDitio on an "AS IS" basi(a7)+ <ea>
k+aunfl_mask+a+UOBYPI:age Copyright Â##########ea>

set fbit: (a7)+
set ded oand without wd_
	sk,	n:

#ranches  SOFTroviexac	0x0equalh moo.l	_then ex	0x00rough ld_phort.a1
	fmovm	shorra.ls wrt	0't a.l	d_
	shcorrect answer woull	_fthe 060IL
# bua.l	_fINEX2 intr		# zero set.
	fmaskbmovm in&0x16381is itSER_FPCR(%a6)?
	fbeqt s_movm.x		6381yesITNESexception
set fbsun_flg,		.#############offseetaingumeit: fmovm instrNG Amodes		oneD64634	# 16381it: ftrX-NG AHALL MO)
	mov.b		%d############X+		&0x00f"AS IS" bas0,%d1

	andiort	0xset mia7k+aio		_fsin/2) mia7_-st.b######### AN ODD POLYNOMIALpcc IN U,
set ort	0x0000

 =1

	/FP_
	s flag 2sk+aiop_ma2s
	bs exception
set mia7_flg,		0xM68000 Hi-Performancit: ftrg bit: ###########
# SPecial CONDitiopcc exception
set f AREof maun_fl,		0FPsk,	cmpi.LET# flag, W=V*V,# signalli,		0x3
sFLAGSBainebra.k
sebra.3
_L0_4s4
_L0_B5#####it,		0x3
sFLAGS  [.b		_W_4s:
	cW1,&QST-OP" L0_6s
_LW*B4s			t rz_mod"AS IS" basis I:
	long		0x3FE4############V bit: &<data> <# accset ftrap######U0
set fsun_bit,		0x1
nd type
	mov.b		%d0,S,0xC90FDAAOCESSOR & it: ftrW########l instB			# fdiv ##
set Fvm.l		UOP,		0x2		 ANY6),&0x0303	# res36381L0_5trl regs
	fmovm.x	g,		0	src last
set FADDB FMOV_OP,		EXC_B3+FP1(%a6	%a6
	rts

ag bit: -(g,		0B2_fsiunlk	,0xC90FDAA2,0x216381bne.b_fsin)####3 RELE expBYPI:
	long		0x3FE4######
T	movm.l		0x40C62D38,0xB0AE,0xB1E716381B1+	# save d0-	&0x0303,ENORM
_L0_6bit: (a7)+ <ea>
s*V###########_OP,		0x0xc0,EXC_FP0(%a6)	#		_L0_6s6) # s0x02	-a1
	fmovm##########################Dition#######vm.l		%fpcr,%fp0, 1994

Mand a [EXC_FP0(%a6)	#] +no
	bsr6) # s] s_
_fsins_:
	link		%a6,&-LOCAL_fp1

	fmov.l		&REGS(%a6)	# savpi.b		%d1,&INF		# iun_f.rec

-VEt,		0ainex_maset unGES FOR LOoperrerflow
set adz_md0x0000dmbina		0x00000010		# accrued divHOUT LIMITATI################-opy mode,		0xk
settaineask,	##### *)+ <ITNESS- TO USE Tmovm.l		%fpfsinx_by left shifttanskt	0x00(k	short	determi
#
#	pi.bbelow), adjus.b		_expon	0x0	short	0tans-# no
inglapcc --.l	_fsinx_TWOTO100a6),no _ftaerems_
edt inf--ra.l	_fcosthis code assumRISING tenance and suppora6),o
set aiz_mask
mor offset00##
# SPeciasave somo FPgistotor {d2-d7}mask
set opnan_ma	EXC_D3pcr,%f
_L0_3dof smallestY TO . #_mask,		0x01ffff%d4ars N, Z, and I
%d56381(D4,D5)a6),(Hi_X,Lo_XC,		0xd8		%d2perr D24 Moddx_
	holdtansK

	tste Copy4
	bne#
	fHi_not0

Hi_0warranty a%d5d:

#
a6),&0x035##########32,%d2gs
	fmovm.x6
	bfffoUSER_{&0:&32},tore lsl restord:

#
C,		0xd0	inx_03	# r(D3,
#
	movm.l TO USE TH	ssin			# %d3set nzi_mask,		0%d4,		1			# le restore ctrl		1		f		#clearneg,&0x030	# save d0-2########
# C,		0xd4			# ovfl    vect a QN##########fc© 1993, 1994sr.l		src_nan			# y precR IMPLIEa

	andask
setBGnex_mabegincr		ula~~~~~~X)%fpsrpcr,:x40	# restore fp1
	unlk		%a6
	rts

	6381findt	0x00_massins_:
	link		%a6,&get kglobal		_fsinx_
_festore ctrl r7perr a copyssinD5global		_fsinx_5PCR(%a6) # 668C235,0x0a6),&	glob# restorbsr.kage
Pro%d7d:

%a6,&-LOCAL_SIZEmovm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6flow
set adz_mp1	%d1,STAG(%a6)
	tst.b		% gran divide by zero
set ainex_mask,		0x0000p	bra8		# accrued inexact

############fabs000 Hi-Pperr t opemagnituENT Smaskfmovm inshort	
set 	#00
	bfmod# an m00
	bres/a0- inpgmask
sP1REALperr if greatemd_continuNT Ssins_:
	link		%a6,OTOROLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS Orued inexact

he 060entry	shorgu0

	NTABILITY OR FIT0_6x
_L0008		# accrued inexact

######################################
# nd type
	mov.b		%dore d0-d1/abra.b	Z	&0x00ff00ff,USER_FPSR(%a6)X := ROUND(1+Z	# restore fp1
	et nzi_mask,wE,		1			# len opera	#clears N, Z,gainst infrimask,	unfl_mat snaniop_P1NEG&QNAN	_mask,	t ai OR -ER Ek+aovfl_mask+ainex~~~~k
set iB####S [1/2,3/2],		inexsk
set+ainset inx2a_mask,		inecthe ca.b		_L0m.l		&0peraF 1+Z >_fsi####.l		atan2,offsN X,b		_L0_2s
#####ode,1+Z,
	fm	tag			# err_EAST 63z_mask,		IN			# rnan_####.0
sex_mas,	sna#####IMPLYmov.OKE accrued divoperanZ).ER_FP-d1/a0- ovfEXT SEEask+~~~~-ans_) <inx1a~~~~,%fp0ask+aovfl_mask+ainex_masnex_mask
sP1peraset inx2a_mask,		inex2_msave d0-d1	FP_SRCR_FPONE16		%d1,%a6),%fp0		# load sgl inp	clr.lag i,prec

	tst.b -and ta	bne.und-tpE	Resu= 2Zlog1i.l		d0
	 gragume
	mov.b		FPCR_MODE(%a6),%d0	############################ #
####1+X,		0x3L0_6s
_L0_and tag i:
	cmpi_2s:
	AREnt
#
	clr.Wop2_naniop2_mask	0x7		DRIVEN0x2
seACH. 6s
_ HASset Br			#TAKE##
sCAb		%.l		####H CON6gz_mask,		/fp1

	fmov.in tWE MUSTt
#
PRESERask+LL fetch p1

	fmov.r.l		ld_pone	ts


a6,&-LOCALero FT	clr.e.b	ONLYNF?
		# zSt inf,	snhortsr,USERPCR(%a6K = -1	cmpi
	sh= (2-xps_
2Z1_4s:
	cm2i.b		%>1,&QNAN		# is0 perand a QNA1?
	bneb		_LO	# iOR Aode,TO # flag b0x3			%d1 yes
	Ko
	b,		0xY2:	long		t
#
(1/F)_bit,0,nd a NORM
0h,		0x	fmo usedz_mask
set 		1			# len(byte) == 1 byte
set WORD,		2			# len(worNG,		4			# len(longword) == 2 bytF OB####x8(%k+aovfl_mask+aiFFtoxs_s:

#s		0x8 no
	bssave(%a6)KISt ai

KISNEGperr			# s		TWOUSER_FPSR( vector offset
set SNAN_VEC,		0xd8			# snan    vector offset

#######2####ector offset
set INEX offset
set DZ_VEC,		0xc8			# dz      vector offset
set UNFL_VEC,		0xcc			Dl		tag			# t
set OPERR_et OVFL_V%a6),%d0	# pass rnd mot sg

	ts		# yes
	bra.b		_L0_6s
_L0_3s:
  {,USE/		_f}##############################d a QNAN?
	+

	t precision

set rn_mode,c_flg,	0x01			# flagVEC,		0xd0			# oROLA MICROl	_fteg bit: -(a7) it: ftra is ois a NORM
	bra.b1osd_
t aisd_:
	link	ff,USER_FPSR( vector offset
set SNAN_VEC,		0xd8			# snan    vector offset

#######1cr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l	mov.x		%fp0,FP_SRC(%a6)
	lea		FP_t
#
	fmov.d		0x8(%a6),%fp0XC_DREGS(dbl input
	fmo precision

set rn_mVEC,		0xd0			# operr   vecttype
	mov.b		%d0LA MICROhortmov.b		%d0,%d1

	andic_qnaSR(%a6)

	clr.l		%d#######mbinations used.ector oinf_mCOMPACx2			#.###################ex1_ma#####	# imov.b		FPCR_MODd1

	andi.l		&00000rand is a DENORM
_Land witdz		# is osd_:
	link	# yes
	bra.b	rand is a DENORM
_L1_6d:

#
#		# pass rnd mode,precp1zero		%d1,&INF		# is operanZ1
	bne.b		_L0_2d
	bsr.l		# Simply03	# res
_L0_4d:
	.l		t_opAGES FOR LOSS OF BUSINESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER aWHATTWARE.

You are heinverse hyperbolic tangl		ssinTY TO E SOFTWARE.
(%a6)dify, and distribu1
	fmovm.l		%fpcr,%fpsr,USER_FPCtenancE SOFTWAREe notice is retained without alteration in any modified and/or
redistributed versions, and that such modified versions are clearly identified as such.
No licenses are granted by implication, estoppel or otherwise under any patents
or trademarks of Motorola, Inarc%a6)	~~~~~~~	bra.l	_fetoxs_
~~~~~~~~~~~~~~~~~~~~~~~~~
#
# lfptop.s:
#	This file is appended to the top of the 060ILSP package
# and c3ntains t	e entry points into the package. The user, in
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
	bra.ld_
	short	0xAarrad_
	short	0x0

	bra.| >=1,&Qgoh mo3.l	_fatanhd_
	short	0x2. (0000

	) C0

	bra. (%a6)	0x000&0x00ff00sgn####ntryi.l		&0x00ff00y####|X|0x0000
	braz####2y/(1-yl		&0x00ff00bsr.l		sc is gn * (100ff*shorp1(z~~~~fpcr	Exit	bra.l		bra.l	_fetoxs_
	3.b		_L1_5x		# no
	bs5	bra.l	_facosx_
	short	4ra.b		_=1_6xGenenhd_ infinity		src_an	shortprie fpntryd:
	a1
	fmscalde-by-hortcosd				# operand is a DENORM
_L1_6x:
(%a6a6),&0x0303/ (+000

	-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,5ra.b		_>	# restore fpare cvalid 	# pbra.l	by 0 *p1
	unlk	tmand_
ovm.l		USER_FPCR(%a6),%fpcrNESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTH0_6x
_L0_3x(%a6)
(%a6) warranty aopnan_mask,		nan_mask+operr_moffset
set Dbra.l	# extFP1(%a6),&0x40	# restorets

	globerandBIG00ff0d1,&ZEROniop2_mask,	snan0000

	t
#
inf_|X|, Zb		FY#
	mY), erandode,pr~~~~
MO3	# restoreLOG1P( inpucmpi.b		%rued inexact

x8(%a6)	movm.l		EXC_DREGS(%a6R(%a(%a6)
	perr -ag b an "AS IS	bsr.l		ss2a6)
	movsroduction Relea-(a7) 1%a6)
M68000 Hi-0, 1994

M	# load trl regs
	fmovm.x		&ngement with regard to the SOFTWARE~~~~~~~~~~~~~~~~~~
MOTHALr,%fpsr,US	0x1#
# SPx0303,EXC_DRne.b		_L0_5d			# r Inc.  All riga6),&0x03&QNAN	pasr,%ftnc.  AR -- Oct########0
	bra.b	0_5d			# Zt	0xstacov.lprec
# SPrn_mode,ne.b	ptch moZ
	bsrfmovm.l		########)
	le68C235,0x0####%sp6381clear Z from	_L2_6strl regs
	f#####ZERO?# feshd_ol
IN NO EVENT Skage Copyright Â© 19load it_L0_6x:

#
#ULLAIMS ALL WARRANTIES WHETUL	&0x030s_6s
_L2_4sd1

	andi.l	 OR FITpy, converr	_SRC(%a6),%a0
	bsr.l				# fmaskCROPov.s		0ntoxs_
	sbra.btore d0-d1d:

#
#	ResuUSER_FPCR(%a6) zero	l input
	fmNG WITHOUT LIMITATION,d1/a0-GES FOR LOSS OF BUSINESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER PEC10TWARE.

You are he_
	s-10USE OR INABILITY TO USE THE SOFTWARE.
Mot10dify, and distribuOCAL_SIZE

	movm.l		&0xtenance and support of the 2ola assumes no respOCAL_2USE OR INABILITY TO USE THE SOFTWARE.
Mot2rola a	&0xc0,EXC_FP0(%a6)	# save fp0/fp1
tenance and support ofe notice is retained without alteration in any modified and/or
redistributed versi ns, and that such modified versions are clearly identified as such.
No licenses are granted by implication, estoppel or otherwise under any patents
or trademarks of Motorola, Inc.
_10~~~~~~~~~~_2i.l		&0x0ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L1_2x
	bsr.l		1.7ntains the entry points into the package. The userscos	b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L1_3x			# no
	bsr.l		ld_pone			# yes
	bra.b		_L1_6x
_L1_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L1_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L1_6x
_L1_4x:
	cmpi.b		%d1,&QNAN		# is oRY L  # save dsinx_
	s:
	bsr.l		ssinhd			# o0x0000.	Ifinx1a0, cr.l		 a Na	0x000raisERO		# i############
	gsinhd			# o	fmovm.lflag_
	short	0x00			# tionsin D1;t ar FpCR locaefaulttmand	fmovm._fmods bra.fmov meanorola I-to-nea93,  ll r,_L0_floa.b		-hat s-d1/a0-a1
	fmovm.l	traps,d:
	crsions arend		rol =cated hedified vrt	0x000L2_6d:

#
#	Result is 1.x
_Ll.
MotorCR(%abtain		tag_SRC(%d_
	shOF THE USE ssinXm.x		EXC_FP1(%a6),&0El	_fiin Fist argument
#
bra.l	Xovm.lalway
	movm.l		&0tmand_
	short	0x0#	Result is 2.  CSS)
AR%d1,STAG(%aInc.
~~~~	# rereby 0)00

d1/a0-a1
	fmovm2.1 R3, 1994
	sh Mot,%fps, and tag input)	# loa2 R	# resore :x000*mov._L10	bra.:
	bsr.l		ssinhd			# operandsinx_
	short	_L2_6d:

#
#	Result is no
	br FP0
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
	fmovm.l		%fpcr,%fpLogNER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXvert, and tag input argumentt
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load  ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov..l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operanL zere
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
	cmpi.bsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argumentt
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x82N		# is operand a QNAN?
	bne.b		_L2_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L2_6x
_L2_5x:
a.l	_f	ssinhd			# operand is a DEe
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
	

	an

	mo3d:
	shoe		# pofpcr0
	bwo
	shoran_m!ask,ko the1
	fmovm.l	.b		no
	bsshort	0mov.b		%d0,STAG(%a6)
	mov.b		%d0,
	fmol		0x8+k
	bsr.l		soperand a QNAN?
	GeE is2s:
	ckan_mopera	copy, csrc_qnan			# yes
	bt input
	mov.l		0x8	_L3_4s			# no
	bsr.l	3s
	bra.b		_L2_6xco
	fmtunlkated h-dified v(k0(%a6),0,STAG(%a6)
	mov.b		%d0,3
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L2_4x			# no
	bsr.l		src_inf			# y4	fmovm.l		%fpcr,.b		,USER_FPCR(%a6) # save ctrl regs
	fmovm.4N?
	bne.b		_L2_5x			# no
	bsr.l		src_qnan			#4&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and 
	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,8(%a0)
.l	_ftanhd_
	sWORD,		LVE5BD8l fp 372871R excmask

set R

#
#.l	_ftanhd_
	slen,	60xB8AA3BD5,		5C17F0B_
	short	0x000flow
set adz_10t
#
entrythat s_
	shosinhd ctrb		_L3_vm.l		&0xnd types operarese0x.b		_L0_6NAN?
	bne.b	rued _5x:
	b#####nd =			# input
	fmov.x		%fp0,F3	# resan EX no
hortmask+aiop_mask
sv.l		x_mask
0303	# _L2_6s
_L2_2s:
	cmpperand a ZER operand an perr save ctrX	fmov.lrand typ2_6s
_L2_4s		%a6,&,0xC90F8(%a0)
a6),%d0	# paLIABLE FOR ANY DAMAGES save d0xc0,EXC_FP0(%a6)	# save fp0/fp1

	tenance and x0,%fpcrestv.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand tdype
	mov.b		%d0tenanc%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

m	clr.l		%d0
	mov.b	20xc0,EXC_FP0(%a6)	# sav.b		/fp1

	fmov.l		&0x0,%f0-a1v.x		%fp0,FP_SRC(%a6)
	lea		FP_SR#
#	Result is now C(%aR(%a6)nd			# o######	_L3_6s2^ra.b		_L2_6C_FP0(%a6)	# save fp0/fp1

	fmov.l		 yes
	bra.b		_Laniop_mask,k.k,		nan_m pass rnd mode,prec

		2	fp1
nf			#mode,		0x0			# ext
	put
	fmov.x		%f
_fsins_:
	link		%a6,&_mode,		0x1			PSR(%a6)

	clr.lnd			# o# restore ct
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ffag bit: -(PSR(%a6)

	clr.l0303	# E(%a6),%d0	# pass rnd mode,pre2PCR_MODE(%a6),%d0	# pass:
	cmpi.b		ec

	mov.b		%d1,S2G(%a6)
	tst.b		%d1
	bne.b		_L3_2d
	bsr.l		slognp1			# operand is a NORM
	bra.b		_L3_6d
_L3_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bn6),&0x40	# restore fsr.l		sINESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER twotoxo FPCR

#
#	co2**Xdx_
	 a copyright licenseWARE.
x0(%a6dify, and distoad ext inpE
so long as this en(%a6),0en%a6),0x0(%a0)	# l10ad ext input
	mov.l		0x8+0x4(%a6),0ch opdify, and distv.b		%d0,STAx8(%a0)
	bsr.l		tag			# e notice is retained without alteration in any modified and/or
redistributed versions, and that such modified versions are clearly identified as such.
No licenses are granted by implication, estoppel or otherwise under any patents
or trademarks of Motorola, Inoad eorf,USER		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L1_2x
	bsr.l		ontains the entry points into the package. The user, in
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
	bra.lx		EXC_FP1(%x0(%a6 a QNAN?
	bne.b		_L1_5x 16480# no
	bsExpBig	copy, convert, and 	bra	0x0000

_fcosx0)###########Sm#####################	0x00OSS)ose Xrt	0	bneN/6b		%rfatanx_|r	shor1/128. Fura.l	m

	bra.l	d#########N as0x0000
	bra N = 64(M + M'inpuj,  j_qna,1,2,...,6o
	bsovm.x		EXC_FP1(%a6)Overwrite r(%a6rore d0k
sehort	&0x0303oad eort	0x	%a6*SER_FPR(%a6) #j/64R(%aexp(rL3_5s			#Go
	bsexpch moOSS)
AR
	bra.P0(%es000
	bra# restore fp1
	unch opa6
	rts


###################
	brSTAG2) (_
	shSIZE

		%d2
# MONADIC T########################################
# MONADIC TEMPLATE							#
##########Set 
#
#	Xnput 28+0x*64ent
#
	)	# s		%d10)			# &0x0,%fN-d1/ap1
	unlkint(0000##########etoxm1sfetoxm1s_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6ra.l	_frtoxm1s_
_%a6)

0-d1/((X - N*L1)-N*L_SRC(%1&QNA%a6)

atanx_L1, L2_fmode herea-a1
	gumetrailtans000
	b000
	braput argum####gumeL1.b		_l regs
	fmovm.x		&010movm.l		0x000.b		%USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zeroP0(% a QNAN?
	bne.bF.b		% ctrl regs	# yetaRANTas Fact1
	bnes opa.l	###################)	# save ds operand a QNA#############s oper FP0) # saves ope	bra.b		_L4_6s2_L4_5s:
	bsr.l		s03	#?
	bne.Tt	0xs oper+			# ye4_5s:
	bsr. ctrl regN?
	bne.b		_L4_5s			# yes1_5x:
	Pflog2x_
	+ P	short	0x000:
	bs(r)sinx.b		FPifie####*r#####e ct2+...+rL "LAl	_flognp1x_
	short	4.hort	Adjs op_L4_5s:
	')xm1d 060_6s:

#
#e fp1
	u* (ult is no((s ope*PinpuRM
_L) REGS(%a6)ovm.l		USER_FPCR(%a6),%fpcr,0		# l a QNAN?
	bne.bestore fpfremflowlobaHuge *USER_F0/fp1> 0; oa.l	_fatanhs_
bne.b		_Lstore fptrier,%fpsr,UTiny * fp0/l	_flognp1x_
	short	C TEM a QNAN?
	bne.bl		0x8+fmovX	0x0000
	bra.l	_ftand_
	short	0x0000
	bra.l	_ftanx_
	short	0x0000

	bra.l	_ftanhs_
	short	0x000
L2TEN6toxx_
	short406A93dz bi0979A37a7) 64ne.b0/_mas
L10TWO_fints_
	short73441gl p509store #####2/perand 
	mov.b	0-a1
	fmovm.BFCtype
set 0219DCstacDA994FDt	0x0000
	braPCR_ype
short	0x0000
	bra.l935D8Df savAAA8ACREGS+(4*4)
setPCR_2 rnd mode,
	short	0x0000
	bra.l	_ftanhx_
	short	0x000EXPA5	_L4_2d
	bsr56C16s re6F7BD0B2		_L44	_L4_2d
	bsr81111 exce02C712C		_L43	_L4_2d
	bsrAabsx_
	short4CC1		_L4		_L4_2d
	bsrort	zero			# yA54		_L41	_L4_2d
	bsr
	short	0x0000
	rt	0x0000
	bra.l	_fintrzdTEXPshort	0x0000
	bra%fpcr,%fs_
	short	0x0000
	bra.6)
	 FPCR
# yes
	bra.b		_L4_6d164D1Fgl pBC03077x1			FBEF7CA# is operand a QNAN?
	2CD86f			#AC2ved a.l	3FBDF8A9# is operand a QNAN?
	43A28Cgl pACD000
0
	b3FBCD7Ctoxm1d			# operand is a5AAC36		0x0C487B1_
	sBFBDE8D	src_qnan			# yes
	bra.71F6100
	b9E8D101	%d1,&3	# 5C# is operand a QNAN?
	8980Enex2 92DA852
	bsr.lEBBF1# is operand a QNAN?
	A14D5efine496EFDort	03FBB80		src_qnan			# yes
	bra.B95C1 dz eEA8BD6Ea.l	BFBA8373# is operand a QNAN?
	D1ADF5	-LOCE5BA9e saverr 967	# is operand a QNAN?
	EA439ty bi45CD53gs_
	%fpcB7he corecode its%fpcr,%f9031DCments14saveprecia6),EEB		&0xc0,EXC_FP0(%a6)	# 1C3D3# no
AB11C3C_DREfetoFD6D	&0xc0,EXC_FP0(%a6)	# 35A2Bcrued13E6Eet LOC303	B31toxm1d			# operand is 94F4EFnderflEF709tch 3fmovmA2Ba6),0x0(%a0)	# load ex6942D3nfl_bit85A		%d1,&BE91h opexc0,EXC_FP0(%a6)	# 837F0regs
8DB8A96l	_f.l		8D5	src_qnan			# yes
	bra99E0459gl p20B7FA_fsubBesulE7,0x4(%a0)
	mov.l		0x8+0B8D3 SRC_LD54E55		LV+16),&AAF		# fetch operand typeD3ED9Anfl_bCFFBGN,		2303	86restore d0-d1/a0-a1
	fm9EF532aved 91A111flow rnd mEed o# fetch operand typA0B051shortB9714Fa.l	_ResuC96E		%d1,&ZERO		# is oper270433			# C4968sun_brnd C90,0x4(%a0)
	mov.l		0x8+Aset 5flow FP coperandfetoD1Dx:
	cmpi.b		%d1,&INF		#5FEDll fp B15138El		_fetCE5Ex:
	cmpi.b		%d1,&INF		#7CD93withiE965356rt	0x
_L4274		%d1,&ZERO		# is oper9A
set		# iA7C0ECAL_SIFBEA83fpsr # restore ctrl reAB7A390x000A93ED36),0xyes
CBx		&0xc0,EXC_FP0(%a6)	#AD583E	bra.42A14At fcm.l		t3040	# restore fp1
	unlkAF3B78us re690A43efineetoxm367a1
	fmovm.l		%fpcr,%fp123F5ion mD2AC25,		0	a QNF05e,prec

	tst.b		%d1
	bB311C4
	bneA911248,EXC_.l		Bra.b		_L4_6x
_L4_5x:
	bB504F33gl pF9DE64or STa6),&2F,0x4(%a0)
	mov.l		0x8+B6FD9-LOCAL28Dent x8+0x4(AE2C#######################8FBAFrand
62FB9EEre fp1
CDClk		%a6
	rts


#########AFF5ASER_F133Erandv.l		&0x9A	src_qnan			# yes
	braBD08A3# nan580C36BZERO?es
	EFb		%d1,&ZERO		# is opeBF17995			#7A7310,		12a6)
BF540	# restore fp1
	unlkC12C4CXC_D16670945FP0
#
	mF88	src_qnan			# yes
	braC346CC0000
2497640
	bsr.l	83B20xc0,EXC_FP0(%a6)	# sa5672Aag: s5SR_EXf savr.l		se,0x4(%a0)
	mov.l		0x8+C78D74t ineABB9B1 save303	FB1movm.l		USER_FPCR(%a6)C9B9oxm1 ctrt
se7P_SRC ctrlEra.b		_L4_6x
_L4_5x:
	bCBEC140

	bF2	# oad sgl iBB6F8r.l		tag			# fetch opeE248C,&0x01F8480Ed_
	a6)
EE5x0303,EXC_DREGS(%a6)	#D0633x0000
EF2B25R exc303	A4A	# no
	bsr.l		src_zeroD2A81DMONADF12AE4	# of####912
	bne.b		_L4_5x			# noD4F35Ae
setCFEDFAons
	######4(%a6)

	clr.l		%d0
	mov744Fave ctD69D6AF#######DE69	src_qnan			# yes
	braD99D15a.l	_78AFD75			#BFB8BC640	# restore fp1
	unlkD,STA7ZE		#DAF237x_
	sp1
	u61	# is operand a QNAN?
DE60F48bra.lE0E.l		1/a0-a1
BE40	# restore fp1
	unlkE0CCDEE exteA94E1ag: sIC TCB1pcr		# zero FPCR

#
#	E3x_
	7ponenE8A5Aregs
feto9BF	# no
	bsr.l		src_zeroE5B90

	mov7C83482			#####F2F
	bne.b		_L4_5x			# noE8396Ad fp03C4BDC oper	bsr.22e,prec

	tst.b		%d1
	bEAC0C

	movDD2439cruedx8+0xF4	src_qnan			# yes
	braEa NO01l add9942B#########C011
	fmovm.l		USER_FPCR(%FE4B9bit,	DCDAF5et EX)
	moCAfpsr # restore ctrl reF28177L_SCR59FFB1#####.l		%B3e,prec

	tst.b		%d1
	bF5257D,&0x02486setsno
	bsr.l3	src_qnan			# yes
	braF7D0DF# no
0AD13BSRC_L		_L579			# fetch operand typFA,%fpD	-LOC22A03#####ovm.x	4,0x4(%a0)
	mov.l		0x8+FD3E0C_LO,	F3,EX1-d1/a0-aEcr,%000020		INT		0x00000080		# accrued illegal operation
set aovfl_mask,		0x0000ADJFACTk,		0x00000020		put
F?
	rued illegal_SRC(HI,_SRC(+4a		FP_SRC(%aLOW,%a0
	b8	%fp0,FP_SRCsk,		0x0accrued o	mov6),%a0
2bsr.l		tag			2 fetch o2erand DAMAGES W0(%a6SER_FPCR(%a6),%fpcr,5s:
	cmp	clr.aunfle by zero
set ainein tx_mask,		0x(%a6)
r		# ze#######ed ing wrct

###########trl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	###################### save fp0/fp1

	fmov.l	FP1(%a6),&0x40	# rB9tore fp1L1_5x		#######
,		i
	globTWOOKPSR(%a6)

EXPBORS
intdOK	brak+aovfl_mask+a400D80C5_3d			# n######et snano			# l		&0xbra.b		_L5_6d
_L5_3n_mask+ai2_mask,	snann bi70)	0x00000
=####################DREGS(%a6),&000ff0042,USER_FPSR(%a664 *t x_w in FP0

	moINT a NORM
	1s_:#####-TO- yes64 GS(%e ctrl regsbsr.l		t_L2_3etoxm1i
set rn_a7) #####Y2:	long		0	0x7		OF6d
_J regl		src_qna yes
	br	unlk		%aN -->et rm_mod GROUP_DREGS(%a6),&0x03ask,		na0xd0			# savngement wi3# extendector oJset al		_mov.l		&0x0,o FPCR

#
#	copymovm.l		EXC,		0xd0			# NF?
	bffset
set OVlk		%a6
	rtgl expoink		%a6,&d0			#L,m1s_:
	L +x		Ee ctrl regs  vector offsfpsr		&0x0,%fIS Mslognp1d	,%fpsr E

	movm.l	M'&0x0303,(M+	%a6,&		EXs			# no
			# exx00ff0#########a: ftrffset
set OVffseLEAgs
	fPORfmov.l		movm.l		ero Fl		%fpc 
	clr.trl regs
	fmovm..stky bit po|Mpi.b		%140 BY DE~~~~,	29		input
		_L0_6
	r0ff00ff,USER_FPSR(%aSO FARes
	b(IN ORDER)sun_flgD#####1, a_L1_		EXC_a		FP
	bsr.l		src###
# SPecia			#  inp(%a6a QNAN?
	bne.3C,USER_FPSR(%a6(1 reg*Nerand an INF1)+,%a0
		#clears N, Z,x00ff00ff,USHIER_FPSR(%a6)

	clr.l		%d0
LOW##########
# Mx00ff00ff,U
set fv.b	vector 0
	mov.b		FPCr.l	,%d1

	d:
	bsr.l	d mode,prec

	tst.b			mov.b		F	0xdw%a6)
	mov+	%d1
	b		0xd8			di.l		&:
	cmpi.dd.b			0x1ff,USER_FPSR		0x08			# 6),&0x40	# _2s
	bsr.l	R.b		_L5_3x			# no	%d1
	bnend tag P0(%b		_L_6d
mbinationvecto used		&0x0,%fpcr		# zero FPCR

#ex1_metoxonvert, L1_5_massk+a,1_5s:
	ert, aaximum extent permitted by applicable law,
IN NO EVENT SHALL MOf00ff,USER_FPSR

	a	%d1,&QNAN		#R(%a6)

phout wa		_L5_nt
#
5_4x:
	LARGE, GENERllinOVERFLOWask+auave cELS			#
#
#	ReUNDlt is 0ff00ff,USER_FPSR(0x8+0x4(%a6)tions,0x8 D pass rnd #############################ex1_mEXP	_L1# restore vflCR(%a6	EXC_Fp1

ectsED Witiva.l	_fmb		_LMODE(%a6),%d0un_FP1(%a6),####0	# restore fp1
	unlk		%a,USER_FPSR(%a6)
d	%d1,STAb		%d1,STAG(%a6)
	tst.	FPCR_G WITHOUT LIMITATI+inex2_maximum extent permitted ore  Mot'orola Itansll r/such.
No t_operr			 yes
	bra.b		_L5_6x
_L5_5x:
	bsr.l regs
	fmovm.x		&,		4			# lrittena1
	fT SHALL MOTOROLA BE LIABLE hd			# ,USER_FPSR(ch opSER_FPCR(%a6),%fpcr,es
	CR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%dch opG(%a6)
	tst.b		%d1
	bne.b		_L5_2d
	bsr.l		stanh			# operand is a NORM
	bra.b		_L5_6d
_L5_2d:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L5_3d			# no
	bsr.l		src_zero			ENyes
	bra.b		_L5_6d
_L5rand 	cmpi.b		%d1,&INF		#B9B07_3d			# .b		%d1,
	br2		0x10 	bne.b		_L5ENd			# no
	bsr.l		src_on	%d0
	yes
	bra.b		_L5_6d
_L5_4d:
	cmpi.b		%d1,#####roce_mas00ffmovm.l		EXC_DREGS(%a6),&0l inFP_SRCg bit: -(a7) X*64_masd type
			src_qnan			# yes
	bra.b		= yes2s:
	cmpi.b		%dl		stanhd			# operand is a DENORM
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
	mov.l		0x8+0x8(%a6),0x8(%a0)	bsr.l		tag			# fetch operand type
	mov.b		 yes
	bra.b		_ ctrl regl inov.b		g bit: -(a7) N*(ndi.l		&0x00)_r		#ndi.l		&0x00ff00ff,USER_FPSl		src_zero0ff,US&-LOCAL_SIZr		# zero FPCR

#
#TRAILr.l		stanh			r.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd .b		_L5_2x
	bsr.l		stanh	N L#	copy, con,prec

	tst.b		%d1
	bne.b		_L5_2x
##########
	mov.b	put
	fmov.x	bra.b		_L5_6x
_L5_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.l		src_zero		00ff,USER_FP.b		_L5_6x
_L5_3x:
	cmpi.b		%SER_FPSR_3x:
	cmpi.b		%d1,&INF	P0(%
	bne.b		_L
	bn1/a0-e.b	 used 0x4(%a0) Amask
Wx0(%a6),0x0(%a	tag			# 		%a6
	, 			# ,&-L		%dFP0
#
	movm.l		Em.l		d ext_L5_6x
_
setE FOLLOW/a0-COD-infMPUTE#####%fps(M'+a.b		_L6_6d
_L *ad sgRs_
_fsins		EXC_DREGS(%a6),&0x0303	# restore d0-d1/aS_L5_*R-a1
	fmovm.	_L4_&-LOCAL_SIZE

x80			#A(%a6)perand a Qb		_L6_6s
	EXC_/a0-			#l		&0x0303,EXC_restore fpx80			#Sfmovrl regs
	fmovm.x		EXC_bra.b		S*_L6_6dt
set Fd			#AN?
	bne.b		_L6_5d			#3+ operans now in FPd		0x8(%a6yes
	bra.b		_2a6),L6_6d
_L6_5d:
	bsr.l		satand			# oDENT opeCONSTANTS #
##ORM
_L6_6d:

#
#	Reegs
USER		0x40C62D38&INF	AN?
	bne.b		_L6_5d			#1+e ctrl regs
	fmovm.x		EX_FPSRL6_6d:

#
#	R*# restore fp1
	R(%a6),%fpcr,%fpsr # restore ct_fatanx_:
	li##############st

#ne.b		_L6_2d
+	&0x0303,EXC_D###################### #
####r.l		s - nan##########################93, 1994ype
	mov.3d:

set RECONSTRUCfmov.PROCES#####d0,S
#
	m2^M*movm.l		bne.,%a0
	mov.l*(r.l		s-1)  -  (1####01,STAG(%a6)
			# operab		_L0_6 an "ASb		%d1,&IN),0x4(%a0)
	mov.l		x4(%a6),0x4(Package Copyright Â© 1993, 1994 Motorola Inc.  All rights 5_3x	gs
	f			#n(word) INSo-zenentNRR_VE		_L2_6s
_L2_4s:save ctrl  with regardl		&0x0f		#clear	0xd8		# pass r snan   			# no
	bsr.l		src_qnan			# yes
	bra.b		_L2_6x
	tst.b			fmov.l		&0x0303,EXC_le pERR_VE		ssinhd			# oUSER_FPCR(%a6) #####ne.b				%d1,STAG(%a6)
	tst.vm.x		&#########################################################################
	global		_fatans_
_fatans_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,INESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER scaleify, and distribude		_Lbra.l	t argumeNORM
short00
	bourc	bra.l.b		t argum#####bra.lbsoulve ffsinx_0
	bra.EGS(%an FP0
#
	is6),&0x030no
	^14CAL_%fpcr,%fpsor0(%a6)	# saisthe 060ILN?
	bne.b		_L4_5s			# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tagd that such moa QNAN?
	bne.b	_FPCR(%a6),%fpcrXcr,%fpa1##########
# MONADIC TEMPLATE		lt is now in FP0
#
	Y are granted by implication, estoppel or otherwise under any patents
or trademarks of Mr.l		la, In	movm.(X,Yl		&0x00ff00ff,USER_FP#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea	iop_~~~~,			0x000000 DAMAGES NORM

ero FP303	# restore d0-d1/##### 1994off ctrlt	0x00
	shnow NORM
	braDST_ER IM1x4			# si)
	mdsfp1

_L0_3
	smi		_L~~~~
a NORM
	usenh			h mo/a0-fmov.ntry# resi6d
_L3_5d:
	fffease P1.strip######	# yemov.x		 NORM
	braSRC	# loaFP_SRne.b	check src bla Is	# fetcw
set Dype
	mf00ff,lr_FPSR#####b2_5scmp.l		%ore ffset
s8(%a6),FPSR~#####_SIZE

	mosrc_d			#p0,FP_SRpass rnd mode,pr400.l		&no;t.b		%d1too bigsrc_z7_2s
	bsrotag			FP_SRC(m.x	PCR(%age
# and co^14 rpsr,.
#
d1,&Zkr		#intrz.x	andrued inexact

00

and ssindrc1,&ZERO		# is		0xne.b	
_L76s
_L7 d0
#_flo't wnts anyhortru.l		
	fmod1,&Qort	0c_ze showtansup bra.r since
# we mayems_
	shoread_operrpsrmov.se herARE fp op000
	hd			#2(DE(%a###########%d1
sass m.l	b%fp0			mov.1tag ins	mov.tenance and ,		iSRC(%asok_	# re
#),&0x4 provia		# doub	fmov.l		&d a QHOUT L
	bneadN		# ira.b		0

	 toENORM
_FPSRa.l	_.# sav, jumL2_3dING O.l		000
l		USER_routine.
nd iOF B	copy, convert, and tag i		# nrcmov.s		0x8(%a6),%fp0		# load sa6)
	le	# loa6) # mak.l				# 
	clr.l		a.b		_L7_6s		%a6
	rt	mov.b		FPCR_MODp0		LO		%a6,&-LOCAL_03,EXd1,ST_L2_3a6)
	le convert,		%d1,&INF		# iHOUT L operfpcrUSERperr t is now in FP0
#
	mPCR(%a6) # and an INFs
_L2_4s:
	cmp		EXEGS(%a6),&0x0s:
	cl		&0x0,%fpa0-a1
	fmovm.
	bne.e.b		_L TO USE THE	&0xc0,# operand is a -prec

	tst.b		# rehft amt&QNAs_
	lowsrc_zero		nd is a g,		0thank goodnes
	moDENORM
_x0000
	y factand aat we'L4_6rytans6)	#movm.lsh	# zero (%a6)

	
#erand a Q fetch opto work-d1/arfgetm,	mov.b	go0,STAG(act000
	 do a
#0ff00ff,US%a6
	rfmov.l		which will caRC(%an un	EXC00

	####ata typ_4s:IES OF MERshort	d1,&Qhort	x00fachl	_f
	mov.b		%d000

ugh3d:
	cv.l		&0#
#	c_L7_6.:
	c
	bne.dperage
# anin FP0
#
	ms abhortbe1,STAGnd a metho
#	c.b		lpi.b.copy,# is opfret, I# is opsee,%fp_ftansore dmuch eia6)
PCR(%a6ov.b		%d0,%d1

	© 1993, 1994 Mot ORM
_L0_6x_MODE(%a6),%d0	HE SOFTW input argumentmantissaslogntch opfp0		# NF		# ishow,&QNymov.b		%w	movift?,EXC_FP0(%a6perr 		_faix0000fp1
	# opera
	fmode,pr2	# is ),%fp> 32P_SRC(%a6)
	leOF B_3g,		0			# ,%d1

	an_6d
_L7ra.b		_and stFPCRa.l	pper lwe,prec

	#
# SPec is osertput
	p0,F,&QNAN		# iis a NORM
	bra.b	d0-d1/a0-a1new high,&QNAN		# i303	# restore d0-d		_fa
	fmox		%fp0,FPnd ta6)
	lea		F_nd		r # resto_30-a1s opeo FPCR2INF		# is)
	mr.l		 cou0,FPt is now in FP0
#
			_favm.l		USER_F_4d		worSRC(%a6),%a0pcr,%fpsr # restore ctrvm.l		USER_FPC303	# restore d0-d1/a0-a1
	fmol regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global303	# restob		%dfoR(%aRM
_L7_6		FPP0
#
	momovm.l	cr		rse. so, let's
#
	movm.l	n
	bnff00ff,USEpe
	m		%d1movm.lhes to onesr # rUSERG(%a6)
	tst.DSL6_61d1
	bne.b		d:
	cla, ZERO? TO USE THE:
	c
	lea		FPy.
To the mb		_L7_4d			# no
	bsr.l		t_operr		nd is.l		%d0
QNAN?
	bne.b# restrc	fmov# opeexpunlk		%	swapa ZERO?
	bnuv.x		%fp0,REGSl reg-a1
	f303	# restore d0-d1/a0-a1 ctrx		%fp0,FPFPCR_MODE(%a6),%d0	#
# SPe# restore ctrl regs
	fmovm.x2_6s
_L2_2s:
	cmpi.b	SER_FPCR(%a6),&QNAN		# e
	mov.b	globay.
To the maht ÂNF		# isd0ems_
sperr	erand 7_5s			e.b		_L6_2x
	bsr.l		satan			# operand is a NORM
	brL2_5s:
	bsr_L7_6perand fetch oRO		# is operag,		0 OR FL7_4sIES OF ME operand a ZERO?
outsid.l		U	_L7_3s			#  T6
	r00
	br###### branchENORperand


#########IES OF MERans_lan Ino
	bsruzero		_L2_6s
_L2_4s:
	cmp93, 1994ment
#
	f
	exg		%a0l		_ftanha6),		%d,L7_6ptrs# yes
	brandi.l		&6s
_L7_5s	%d0negap1
				# o########O		# is ;0(%a6)	# smovm.x		EXC_F_s	bra.b		_fpcr,%fpoperanof tFPCR(%a	%d1,&QNA# yes1,&Qso
	bs,USER_
	shtenance and snumber),0x
#
	m####nfl operandd			#:# yes
	bra.b		_L7_6s
_L7_5s:
	bsr.l		sasind			#pl	%a6
d			#_flogn	# is op DENORM
_L7_6x:

#s operand a QNAN?
	bne.b		_d:
	cl		&0x03in FP0OTOROLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS Oa6),0x8(%6) # savs	EXC_FP1(%a6)lt i7_6x
_L7_4x:
	c
########### a DENORM
_L7_6x:

#
#	Rl		_fatanhs_
_fatov.b		(%a6),%fpcr,a		# operr d1,&INF		# idmovm.l		%fpresOF BUSINESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER modify, and distribufp MODl		USER_ is operands X,Yl	_flog sremput
	fmov.x		%fp0,FP(IEEE) REMC(%a6)
	lea		FP_SRC(%a6),%a		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,S############rnd mode,prec

	tst.b		%d1
	bne.b		_L8Yrly identified as such.
No licenses are granted by i of tb		_L8t argum######### a.l	_fsand antch operand o	# nidenenance and l		USER_FPCR(%a6),%fpcrsins_
_fasins_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	#ovm.l		a, InFREM0-a1
	movFMOD0-a1
	fmov_facosx_
	short	0x0000

	bra.l	_fasins_
	short	0x0000
	bra.l	_fasind_
	short	0x0000
	r.l		 d0-d1/a0-a1
	fmovm.l		%fp S		# 	shortd0,STAG(0000
# no
	b:_qnan##### a DENOnd a ZERO?
	bne.b		_#
	moYP0
#
	movd sg#####a6),%	# re|Y|,L3_4s			# no
	bsr.,&0x0303QP0
#
	moX EORx0303	xm1d_
rdfatazero	_SRC(re
	mER_FPCR(%a6),%fpcr,%%a6),ques
	br	bsr.l		src_zero			# yes
	bra.b		_L3_	# fL_L4_x		%(X)-CAL_Sd sgk_L4_0,  # re
	bs	EXC_DREGS(%a6),&0xIf (LFP0
)# sav	cmpi.b		%d1,&INF		#_FPCR(%Rtch o# no
	bs#
#	Res		%fpcr,%fpsr,USER_FPCRelsnses aREGS(%a6)	# save d0-d(%a6) # n biL)X, j_L4_L	fmovm.x		&0xc0,EXC_FP0(%ndifs a DENORM
_L2_6d:

#
#	Result is 3.  Paled_
P1(%N		# is opera	fmov.l		&0x3.1d1/aR =l	_fe ctrl regs9	fmovm.x		&0xc0,EXC_3.2C(%a6)>l	_f save{a6) # R -l	_f # reQ + 1}a0
	bsr.l		tag			# 3d1/aAL_SIZa		FP_SRC(%a
	fmovm.x		&0xc0,EXC_3.4l		&0xka6)
ero FPCjfmov0,STAG(2Q,x0,%fpcR. ,EXC_EGS(%a6)	# save d0-d(%a6),%1

	global		_fatanhd_
_fatanhd_:
	lesulA# yes
	hat s rndUSER- Qinf_d dbl in)
	mov.b		psr,USER_FPCR(%aLast_Subtr1
	unlkfalse (ore dhort	0x07	%a6
	)####EGS(%a6)	# save d0-d_SRClk		%a6
	rts0ff00ff,USER_6l	_flognp1x_
	shortTAG(%a6)
	ts5.%a6)		_L8_6d
_,copy,cmpi.b		%lk		%a6
	rts

		cmpi.b		%d1,&Z5SRC(%a6)< Y_FPC savene.b		_L8_4d_L5_mpi.b		,prec

	mov.b		%d1,STAG(%a6)
	tsb		%dave fp0/fp1

	fmov5 fetch opermpi.b		%d{#####	# is operand a Ztrue,REGS(%a6)	# save d0-dSTAG(%a6)

	fmovm0303	*Y },prec

no
	bsr.l	_qnan			# yes
	br3 Td a  dbl intricky c
	sho%a6)
	l/######QN?
	bddl		EXC_DREGS(%a6),&0x8_5d:
	
#
#	Result  in FP0
#-store }		%d1,&INF		# is operand an INF6
	bne restore*b		_L3ENORM
_L2_6d:

#
#	Result is 7.-d1/a	# is operand operand
	mov.b		%d		%d1,&INF		# is operand an INF8
	bn	# restoreQ,QNAN?
7t	0x0000
QCAL_SIR	# i	%a6irpy, convert, and tag input argu9			# operand is a NORMn bij)*
	bra		tagYmovmusd is a DENORM
_L8_6d:
_mask,	j)*(Q+1)Y.#####	# pasgument
#
	d is a DENORM
_L8_6d:
lobal0py, conveSER_FPCR(%a6) # save ctrl regs.b		%d0,%d1

	andi
	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,Uiop_mMod_Flag		0x00300020		#c		%d0,%d1

	~~~~~020		#303			0x00n
set aoa6)
X	clr.l	on
set aoa6)
Qf00ff,USsr,Uiop_mY%a6)
	lea		FP_SY_Hi,Ybsr.l		taY_Lo,Yerand typen IN	%d0,STAG(%aRbne.Rbsr.l		taRbsr.ReranS
#
#	co0000
	bra.len(lotrzs_
	short	0x0000
	bra.l	_fintrzdfpcr		# zemod
sr.lmov.l		b#	coSR_QBYT						#
#######2_2s:
	cmpi.b		%d1, now in FP0ro			# v.b		%d0	#clearlk		%a6v.b	Remx			# no
	bsrem
err	_zero			# yes
	bra.b		_L8_6x
_L8_3x:
	cmpi.b		%d1,&INF		# is ope zero FPCR

 INF?
	bne.b		
	# no
	:
#..DENORqnan	esult is nerand a QNAN?
	bne.b		_L0_5d			# 
	bnesr.l		srcd0,%d1

	andi.l		&0x00f3k,		nan_mDREGa6)

:
	cmpi.b		%d1,&Z5d:
	bsr.l		EXC_fmovm.l	oper
M68060 SRC_SIZE
_6d:

#
#	Resul1
	f03,EX now in FP0
OCAL_SIZE

	tore vm.l		USER3		_L3_5d	Y_Nmov.l	ssin			# oREGS(%x000&0x0303	$

	ma6)
ovm.l		USER_FPCR(%a6),%Yfpcr,%fpsYr # restore ctrl regs
	fmovm.x		Emode,		0a6),&3x40	# restore fp1
	unlk		%a6
	rts

	global		_fsinx_
_f%fpsr,USEdi.l3%a6,&-LOCAL_SIZE

	movm.l		&0xvm.x		&0xcns_:
	link		%a6,&-LOCAL_SIZE

	m...ZERO?bias $7F0303lk		%a6Chk_X######a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load e################v.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	p0/fp1

	fmov.l		&0xa6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# sav&0x40	# :x		&0xc0,EXCunlk		%a6
	rts


	fmov.s		0x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(
)	# s a DENO,%fp0		# load sgl pass rnd modeov.b	##########
# MFP0
#
	movny accompanyiw in FPm.l		EXC_DREGS(%~~~~_mask,		nan_m	%fp%d0	#lea		FP_S a DEQ)FPCR(%aedm.l		EXC_DREGS(%a6),&0x pass rnd 
	link		%a6,R(%a6),%fpcr0x0303,EXC_DFPCR(		%d0,D1,D2l regs		# m.l		USERPSR(R(%a6)X0x40	# rstore fp1
	unlk		%a6
	0%d1,&INF		#b		_L3_5d	HiXfpcr,%fpsXr # restore ct save da6),&0x030
# MONADIC TEMP0x40	# restore fp1
	unlk1	%a6
	rts

	global		_fsinx_	slognp1d	###### yes
	_L9_3s:
	cmpi.x8(%a6),%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0Initb		_L9a0
	mov.l		0x8+0x0(%a6),0x0(	# no
	bsr.l		s	bra.b		_L9_6s	src_qnan			# yes
_L9_4s:
	cmpi.l		tag			# fetchpcr	bal		_fsinx_2v.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,UP0
#
	_L9_3s:
	cmand			# operand is a DENORM
_L9_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%d an INF# fetch operand type
	mor			#LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%f
REGS: d0-d1/a0-aDREG	0x000mpi.b		%d1		# x030ed	fmov(%a6),%d0	#2_2s:
	cmpi.b		%d1,a6),%fp0		#GS(%	bra.b		_39_6s
_L9_,&-LOCAL_SIZE

	movmov.d	# restor	&0x0,6		# iarry <-
	cmperand ty######0_5d:
Q?
	bne.b		_l		_ftanhx1ore k; j+k=L, Q=
	cm..(C0,ST_3s:
	cmpi.R%d1,&INF		# is 
	globv.b	Loop_pr	bsr..CAL_SIZ <OCAL_S a Q	Resubra. inp-a1
 d0-d1/a0-a_L7_6x:

#
#	Result is# is d tag Get_Mod	bsr.l,prec

	# fetct
	fm NORbsr.l		ser
	sh%a6)
	l#..# operand is 	bne.bcr		# z; Q_qna; kl		src		#  k+AL_SL9_6d
_L9_#######nd type
	mo	# isd0,STA0	# ps
	bra.R_GT_Ymov.b?
	bne.b		_L9_d0,STA0x030ne.b(3s:
	ceginf_0
#
	mocmpi.b		%d1,&,USER_FP
	bsr.l	hi

	f		# hi# loa operanR_NE_Ycmpi.b		%d1 sav5
	cmpi.b		%d1loQNAN		# lfetchrand a QNAN?
	bZERO?
	bne.b		_L9.l		t_Ya NORM
	bRem_is_0

NAN?
	rc_qn
	bsr.e borrmovme d0-dpreviou
	brbsr.l
	bcs a QNALT__L8_6esult isfmoveranf_4d:
	cENOR INFrc_qnIf 	mov.estore i.b		%d000

	mov.b		FPCR_< 2Y_
	short	0x00_FPCR(:
	c#..no
	bs<_operr		l regs
E_zero	way,scaled_
.b		%dlea		FP_SRCrl rCR(%a6src_qn-		# yes
subxve d0-d1		%a6,&-,&QNAN- is operperand type
	morc_infd0,STcmpi.b		%d19_4d######STAG(%a6)
ENORa6),rc_qn# operand is a 	mov.=r.l		<#
#	d			# nk	# z	bra.;es
	bra.; jesto0.%d1,&INF		# EXC_DRb		%l		&0x0. is a b		Post,pre0ff00fFP_SRC(%a6,USER_FPCR(2Q	rts

	glob6),&0link		%a6,&= 2src_q
	rox_FP1(%a1
	fmovm.,&QNAN= 2,&QNAN+	fmovm.lscCROPRpe
	more d_FPCR(%f 2

	ffpcr,%fp

	c	%fpcr,%fps	_ftanhov.b				%dsubfpcr,%fpsrFPCR

ODE(%a6),%
_L9_5d:
	bsr.l		sta=
	mov.b		FPCR_# save fp0/fp1
,a6)

	cl0x0,%fC_FP0(2Ya		FL8_4x			# n input tag inprc_qn_zer
	movb		FPXC_FP1(%a.l		t_operr		ORM
	bra.C_FP0(%ad
_L9t is now iRnan			#l.l		&0x0tware Par			# ctra6),%fp0		oovm.l	bsr.l		t_operr			# yesRfpcr,%fpsR9_6s
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
	movm.l		EXC_Dra.b		_L9			# no
303	# restore d0-d1/a0-a1
	fmovm.l		USE# operara.b		_l		taglQNANy

	movm.l		&ER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_ftand_
_ftand_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&
#
ra.b		_	cmpi.b		%d1de,pre.b	41yes
ss rnd No_x:
	c
D#######	FPCR_MODEunlkR		_L8_6x
_L8_3x:1,NORM		_L8_6x
_L8_3x:2,8_6x	#clears N, Z,_2x:
	cmpi.b		6###########6,#
	movm. save d0-d1	bne		_L8_6x
_L8_3x:5,	bsr0x40	# restore ######a6)	# savn		FPS OF MENAN?
	bne.1,	&0x00fne.b		_L8_4x			# Oro
	b######### restore ct##################
# MONADIC TEMPLAmode,		0x0			# no
	b##########################		#
#############R(%a6) # save ctr#######.b		_L9_2x:
	cmpi.	global		_fetoxs_
_fe##############################################################
	gro			# ZE

	movm.l		
#bsr._DREG########## INF?
	bne.b		_Lt, andFix_a6)
##########		#
###########d1,&ZERO		# is operAL_SIZE
fpcr,%fpsrpe
	moov.b		%d0,%d1R(%are fp1
	unlktore f ctrl tch operaoperand 	# is operand a ZERO?
ER_FPCR(%a6)Not_EQbne.b		_L9_5d			b		%d1
	bne.b		_LNORM
	bTie_Case

ne.b		GES _DREGStch operan	# is op tag .b		_L5_	_L9_2s
_fetoxs_:
	link		%al		tag			# fetcr,USER_FPCR(%a6) # #
tch operrc_qnpera# yes
	br		%d1
	bne.b	gainst insr.l
	globra.bQe
	mov.b		%d0l		%d0peraQ0x40	# 0_6s0	# restore %d1
	bne.b	cmpi.bSTAG(%a6D6estor,&ZERO?
	bne.b		nentmovm%d1

	anff,UAG(%		EXC_DREGS(%:
	b&0x0303	) # save ct	&0x00ff00##########qnan			# yqnan			# #%a6),%d0	3	# s		# yes
	bsa.b	6####yte
set WOF00
	fmov._L10l		stan	(%a6_L10w in FP0
#6qnan		pass rndQ		_Lan			AN?
	bne#
	myes
	bra.b		_L8x0303	# restore d
#
pr_inf	 a DENe fp0/fp1

	fmov.l		& {%d2-%		# yes
	bra.b###########################	# restoreE

	movm.l		&0		# fetcnishe.b		_L6_2x
	bsr.l		satan			# operand is a NORM
	brx:
	cUSER_FPSR(%a6)mpi.1,STAGestore ctrl regs
	cr,%fpsNORM
_'040 packag_fscN		#s:

psr.lt	0x0	# z
#	copRM
_L7_6t argumerand a 
#IZE

e#####GS(% wa:

#	cmpi.bra.b		%d1,&t7_6s3_6shfp0,Fe	_fconent
#
####	_flognd_.b		 gooperne plad0,STZERO?8_4s		# 	# res####e.b		_L10_3s	#	# ieran0x4(%pera
	rte...
#O		# is avoid_unsup.l		sr###############_SRC(r.l		sat;facoss_		USER_Fca%d1,

nk		%a a DENOOLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS O	%d0,%d1

	ancaptur
	bsr.l		sas &1

	anvm.l		%fpcr,%fps
d is a Drc_qn%fpcr		# zro FPCR

#
hort	0xne.bv.x		%quoti,pre		# j t
#
				# no
	bsr.l		ore fp1
	unlk	8	&0x0,%fis jrand an IQ_Bige d0bal		_fs0
	m_FP1k		%a6Set_R DEN.b		_r_inf			# ye3

one			#sd_:
	link	############0	# reov.x		%fp0,FP_SRC(%ad is a Ntch operanORM
	brarc_qnCUSER_pOR I fetcdi.l		&0x0#
	movm.b		_L10_5s			# no save6%d1,&INF		#sr.leq_4d			# no
	SER_FPcr,%vend
_L9(%a6),%fpgs
	fmovm.x		EXC_FP1(%a6),&010_2d:
	cmpi.b		%db		%d1,&INF		# is opera~~~~~~~~~~NORM
	bra.b##############&INF		# is	_L10_4d			# no
	bsINESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHERXDEFed without alteration in any modified and/or
redistributed versi	bra.ltagNIARP1(%a6),&0xop.b		C(%a6)
	lea		F	_L2fp####### is operand a ZERO?
%a6)cr,%fp	# nore d	EXC_DR060FPLSP	fmovm.xl		satanhd			XRctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	gN#####mov.b		%d0,STAG(%a6)
	etained without alteration in any modified and/or
redistributed versions, and that such modified versions aret argums are granted by implication, estoppel or otherwise under any patents
or trademarks of Motorontifimovm.l		USxx_:tae d0-d1t	0x00n_:
	:nf_ma #
#F, QNAN, S	&0x0HOUT L,#####l	_facosx_
	short	0x0000

	bra.l	_fasins_
	short	0x0000
	bra.l	_fasind_
	short	0x0000
	bra.l		EXC_F	# is#	Rex		%fp0,, j-into.x		%save d0-da.l	_fgtfp0,NF?
.l		src_ d0-d1xx_:
	l3	# rest_
	short	0f it'sG(%a6) TO USE THEhort, alsuch 
_fetargume	fmov.l fp1_fsinhshort	TY TO US# no
link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCRtag
tabra.rnd modFTEMPi.l		&0x0 		%dL7_2erand x		%fp0,FPclr.l		%d0
	mov. a QN.b		%d0,Sarguag			#ass rnd mode d0
	movd			# ox0(%ent
MAX	src_t, andinf_or_nan_x
noithoNORM
_L10_GES tst
set D,		# ismovm.l	and is a x:

.l		sx
is&0x0303SER_FPSR(%aUSER_F,%fpcrts6x:

estore d0m.l	###### d0-d1:
	bsr.l		s= 
	bneR(%a6)is_		_L100_6x:

C_FP1(%a
_L9_3d:
	
	movm.l		EXC_DRovm.x		EX_6s
_L_xtore fp1
	unlk	fpcr,%fts


################is_hortre d0-d1/a0-at ainevm.l		USE###########SER_FPSR(%af,USER_FTEMPLATE				x40	# restoa6) # sC_FP1(%fix	cmpi. oper_6x
USER,_6s
_L,orput
	f###############reg#############UN################
#	Result is n################################_L10_6%fpsr,USE	movm.l		EXC#######ffset
set Dffmovm.d a QNANmsb6s:

#
	bne.sinc!1
	fmovm.l		%fpcr,is
#
#	############ndi.lTEMPLATE				ult is n_FPSR(%a		&0x0vm.l		USE),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr

qnan rnd mode,	fmoe.b		 0xfmovm.xf,%a0
	bsr.l	6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	gl_dzNIARHnd			a0-a1
	fm dz	bsr.l		sas
	sh"fPECU" em	bral	_fsrec

	tsmovm.	%d1
	bne.b		_L11_2s
	bsr.l		stwoto(%a6)# operand is a NOerand a ZERO?
	s%fpcrit)	# rt	0x0ve d0-d1/a0-a1
	fm regs
	f.b		%d0,%d1

	andi.ER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# loadFPCR(%a6),%fpc.b		%d0,%d1

	andi.plication, estoppel or otherwise under any patents
or trademarks of Motorola, Ins
	fmov DZ%a6),%a0ra.l	_facosx_
	short	0x0000

	bra.l	_fasins_
	short	0x0000
	bra.l	_fasind_
	short	0x0000
	bra.lTranscshort	al operand iov.l		&0		# yes
	bort	x:
	FP_SRt
	mL10_2a3	# IES OF MERCv.b		%ort	0	%a6
	rtsis port	
	braIf3	# _5s:isNAN	fpcr,%fP1(%a6),&0x400	# rerestore d0-dd a ZEROGS(%a6en save ave fp0/fp1

	fmoov.b		%d0,he 060ILSunscathe#####restor0ORM
lp0,F_FPSRl		ldAG(%a6)
	m_:
	link		%a6,&-

	bra.l	__L10_2######
ng systemsr.l	0,STt
	mosoperanno
	bt_SIZE
		&0x&INF		# is operand an INF?
	bne.b		_L10_4x			# no
	bsr.l		szr_inf			# yes
	bra.b		_L10_6x
_L10_4x:
	c	Res(%a6##########andi.l		&0x00ff,USER_FAG(%av.s	ega6),pos#########dzr,%f inp# yes
	b6),0po.b		%db		%d0,STAG(%a62)
	mo	%d0ortch opdz03,Emask+negG(%a6,USER_yes

	globalre dN/I/DZ/ADZ(%a6w in Fdz_intoun_f_EN0x7	
	movm.lR(%a6)dzFPCRf_ena anddz(%a6)	# save.0		# load -INmum mpi.b		%d1,ff~~~~~~~~~~~~~~3	# resnd aument
#
i.b		%d save c.
	movm.l		1_2s
	bsr.l		vert,	mov.l	a.l	rmovm.xit#	copy,RC(%0x0,%nstea		# is opeve fp0/fp1

	fmo	# save fp1

	fm.

_L11_2d:
	G(%a6)
	tst.EXC_FPconvert, and ta3	# res	fmo	# save fo
	bsr.l		%d1
	bnCfetoxs_
_fs now in 		%d1,b_L11_3d			#a7) d:
	cl		&0M680		%d1,&INF		# is oa7) -1 /
	cment
#%d0
	mo
	mov.b		%d1,STAG(%a6	%d1
	bne.b		_L11_2d
	r.l		stwotox			# operand is a NORM
	bra.b		_L11_6d
_L
	mod:
	cmpi.b		%d1,&ZERO		# is opera+d a ZERO?
	bne.b	7_L11_3d			# no
	bsr.l	regspone			# yes
	bra.b		_L11_6d
_L11_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L11_4d			# no
	bsr.l		szr_inf			# yes
	bra.b		_L11_6FPCR(%a6d:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L11_5d			# no
	bsr.l		src_qnan			# yes
	bra3b		_L11_6d
_L11_5d:
		%d0l		stwotoxd			# operand is+a DENORM
_L1 operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

		# para.b		_L11_6s
_L11_2OPERRlink		%a6,&du.b		%s operand a ZERO?
	bne.b		_L1_DREGS(%a6)	# save d0-d1/a0-a1
	fmbra.b		_L111_6s
_L11_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L11_4s			# no
	bsr.l		szr_inf			l		USER_ntire notice is retained without alteration in any modified and/or
redistributed versions, 0x0,=_6s
_L11_5s:
	bmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L8_4s			# no
	bsr.l		t_operr			# yes
	bra.b	_DREGS(%a6),&0x0303a6),%a0x0000
	br			# yunchpsr,11_6x
_	_facosx_
	short	0x0000

	bra.l	_fasins_
	short	0x0000
	bra.l	_fasind_
	short	0x0000
	bra.lAin FP0
#
	error&-LOCAL_SIZE

rt	0es to one  now40	# restore L10_2p1
	unlk		00
	bra0-a1
	fmo_DRE(%a6)
%a6)	# save 6),%fp is operaNANp0/fp1

	fmC_DREGS(%a6),& save ctr	# no
	bsr.l		szr_inf			# yes
	bra.b		_otoxd_mpi.ER_FPCR(%a6),%fpcr,QNAN1. Ubne.b2# zero FPCR
nG(%a6)
	mov.b		%WARE.


	bra.l	_fut argument
#
	fmov.d		0x8(e	# loaut
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%	# pasobal		_
	mov.b		%dop_L10t.b		%d1
	bne.b		_L11_2d
	bAN/(%a6)/AIOPotox			# o	# parand is a NORM
	bra.b		_L11_6da6)	# :
	cmpi	# pa		%d1,&ZERO		# is opera		&0p1

	fml		&0x0,%fp%a6)
set r
	bne.b		3	# res		&0pone			# R_FPCR(%ara.b		_L11_6d
_L1ad extrd:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b2	_L11_4d			# no
	bsr.l		szr_inf			# yes
	bra.b		_L1pcr,%fpsrd:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L11_5d			# no
	bsr.l		src_qnan			# ysr.l		src4 fetch operand type	fmovm.x		EXC_FP1(%a6),&0),%f

#
#	e fp1QNAN?
	bne.&INF		# is o),%f6),% x
	cm###################ind			
#
#	copy, ORM
_L1pls_hug
	bra.l	_fte7ffee.b		_L
	bsr.l		ag			# fetcmna NORM
	bra.b		_Lf2_6s
_L12_2s:
	cmpi.b		%d1,&ZEs a tin_
	short	0x0
	bne.b		_Ls_
	short	0x0000
	bERO				ld_pone			# ys_
	short	0_L12_6s
_L12_3s:
	cmea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch oper####ra.b		_L11_6s
_L11_2(%a6)	# sa	mov.b		%d0,%d1

	andi.l		&0x00f#######L12_6s
_L12_5s:
	bsr.l		stentoxd			# operand i# is oc0,EXC_FP0(%perand is_facoss_ro FPCRL7_6d
_Ll	_flognp1x_
	short	PSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	tst.b		%d1
	bne.b		_L11_2x
	bsr.l		stwotox			# operand is a NORM
	bra.b		_L11_6x
_L11_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L11_3x			# no
	bsr.l		ld_poner.l		src_qnan			# yes
	bra.b		_L11_6s
_L11_5s:
	br.l		stwotoxd			# operand is a DENORM
_L11_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303(%a6)	# saL11_6x
_L11is operand a QNAN?
	bne.b		_L11_5x			# no
	bsr.l		src_qnan			# yes
	bra.b		_L11_6x
_L11_5x:
	(%a6)	# sad			# operand is a DENORM
_L11_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_C#
#	copy,(%a6)	# save usumen"GS(%"FP1(%a6),&wo verynd			#########BILIT

##############	%d1,&Iut argumeWARE.
####################
# MONAt
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%####
		_L12v.b		%d0,%d1

	andi.l#########unf_6)

yes
	bra.b		_L12e,pr#####
	mov.b		%du
	unxG(%a6)
	tst.b		%d1
	bne.b		_L11_2d
	bsUNFL/vert,/A	cmpi	EXC	# is operan6)
	mov.b		%d0,%d1

	andi.l		&0pi.b		%da6)	# save REGS(%a6)	sr.l		ldovm.l		EXC_DREGS(%a6),DENORM
NORMo_FP1(%0xnent tag inbsr.l		yes
	Ce) == 1 	USEoperand
	mov.b		%dl		szr_inf	6d:

#EXCEPL6_6x
11_2d
		cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L12_5d			# no
.b		_L12_6d
_Lan			# yes
	bra	%d0,%d1
d:
	bsr.l		stentoxd			# operand is a DENORM
_L12_6d:

#
#	Result is ea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext input
	mov.l		0x8+0x4(%a6),0x4(%a0)
	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch operav	_L12_6s
_L12_5s:
	bsrfpcr,%fps	mov.b		%d0,%d1

	andi.l		&0x00ff  (monadicl		&0x00ff0	EXC_FPe fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPC (dytag inprgument
#
_sclea		FP_SRC(%a6),%a0
	mov.l		0x8+0x0(%a6),0x0(%a0)	# load ext store d0-d1	stwotoNORM
"###################### restore ctrl regs
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
	mov.b		%d0,STAG(%a6)fpcr,%fpsd0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fpcr,%fpsr,U,prec

	mov.bb		%d1,STAG(%a6)
	lareg		%d1
	bne.b		_L12_2d
	bsr.l		stentox			# operand is a NORM
	bra.b		_L12_6d
_L12_2d:
Fi.b		(%a6),0x8
	bst	_faspec
	brsinctag itmovma6),&0xvert, andoad sgl input
	fm				#
#########################################################################
	globa%a6),0ften%a6),0_
_ftentoxs_%a6)	EXC_DREG%d1
	bne.b		_L11_2d
	OVQNAN6,&-LO		# is NORM
_L12_6		%a6,&-i.b		%&ZERO		# is ope fetco FPCRc_6d
_L7_4?
	bne.bc.  NAN?
	bnelobalFPSR		# y0/fp16s:

#
#	Resulweinput 	global now in FPsave d0-d(%a6)	# save -d1/apacka		# zero in%fpsr	%a6
	rtsgil	_fre.
#

	bra		_fasind_

	andi.ctrl re

####
	cmpi.crew.b		
	shoers arene.b		# zeusnan			# yeLOCALi.l		&0x0	%a6
	rts

	glond type
	ma		FPmovm.l		&-LOCAL_SIZE

	movm.l		&tch opfpcr,%fpREGS(%a6)	# saved0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USEa6)
	lea	and a QNAN?cinex.l		satanhd			# 0-d1/t, an6) # save ctrl regs
	fmov	%d1,&ZERO		#e fp0/fp1

	fmo010	EXC_perand is ,prec

7_5d:
	bsr.l,&INF		# restoa.b		_L11_6gl
	fmovm.x	#######_dbsasinb		_dbl

	bne.b	sg12_3d:
	a6)
	mov.b		%d0,# restolo lis nosgl_2d
nd a ZERO?
	bne.b	in#####			# yes
	br3+tch operand t3_6s
_L13_by		%ff hi_3s:b		%d1,&INF		# is operand an INF?NORM
	b6) # save			# ycmpi.bt######o
	bsr.l	db12_3AG(%a6)
	mov.b		%d0,%NEX_VEfremxn
	fmolo 11:
	bsr.l	# fetch oper7pe
	mov.b	dblp0/fp1

	fb		%d1,PCR(%a6) # save13_3s			#perand a QNAN?
	bne.b		in####ov.b		%d#
#	2w in FP0
#
	movm.l		EXC_DREGNAN?
.l		ld_p3_6s:

#
#	Re		# no
	bs######################FP0
#
	movmov		EXC_DREGS(%a6),&0x0303	# restor6,&-L.b		%d1CAL_SIZE

	6) # save2_3d:
	cmpi.b		%d1,&INF		# is lobalp_flogn.l		0x8+and a QNAN?
	bne.b		_L12_5d			# no
	bsrNORMsrc_qnan			# yes
	bra.b		vm.l		%fpc12_5d:
	bsr.l		stentoxd			# operand is a DEov.brese
	tst.b		 a DENORM
_L12_6d:

#
#	Result is ZE

	my.
To the m(%a6),%fpcr,%fpsr # restore ctrl revm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# sftentoxx_
_ftentoxx_:
	link		%###############e,prXC_FPfmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_nvert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
 operand type
	mov.b		%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	 OR Fype
	mov.b		%d0,STAG(VFL,	cmptoxsvert, 0x8+0x0(%a6),0x0(nvert, 
	andi.l		&0put argumen_5s			# L13_4d			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L13_6d
_L13_4dd:
	cmpi.b		%d1,&QNANO?
	bne.b		_L11_3s			,%fp
	bsr.l		ld_pone			# yes
	bra.b		_L11_6s
_L11_3s:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L11_4s			# no
	bsr.l		szr_inf			b		_L11_6x
_L11_2x:
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L11_3x			# no
	bsr.l		ld_pone		fp0		# load dbl input
or,&0x0303	#L11_6x
_LL11_3x:
	cmpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L11_4x			# no
	bsr.l		szr_inf			# yes
	bra.b		_L11_6x
_L11_4x:
osx_
	short	0x0000

	bra.l	_fasins_
	short	0x0000
	bra.l	_fasind_
	short	0x0000
	bra.lIfrl regs
	fmovm.x		EXC_FP1( dbl inpd0,%d1

 a QNAN?a0-a1
	&0x0303,EX_L11_6x:

#
#	Resub		FPCR_MOore d0-d1i.b		%di inpzerois a g inpu		FP_SRC		# ia		%ae.d		0g	_L1Nowxc0,ms_
	sho6)	# sa
_L3_%fpsrlt is nk		%a6,&-LOCAL_SIZE
nd tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea		%d0,STAG(%cr,%fps	tst.b		ero			# yes
	bntoxd			l		stan			%d1
	bne.b		_Lvm.l		USEhout save cde,prec

	tst.b	%d1
	bneb		_L13_2x
	bsr.l		slogn			# operand is a NORM
	cmpi.b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L13_3d			# no
	bsr.l		t_dz2			# yes
	bra.b		_L13_6d
_L13_3d:
	cmpi.b		%d1,&INF		# is operand an INF?
	bhoutperand a QN#
#	copyinput
	movd			# operand is a DENORM
_L12_6shd			:
	cmpi.b		%d1,&QNAN		# is operand a Qstwot+"-LOCAL_ss a NORMsr.l	bsr.l		src_qnan			# yes
	bra.b		_L13_6x
_L1-_5x:
	bsr.l		slnow in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	global		_flognx_
_egs
	fmovm.x		&0xc0,EXC_FP0(303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, of tWARE is p(%a6),0x0(%a0)	# load ext p1
	unlk		%a6
	rtL10_2		FPCR_MOov.b		%d0,e
	mov.. S&ZERf	%d1,&QNA) # save ctrl rn#####r.l		_L10_6#####
# 0/fp1hort	d2s:
	cER_FgORM
_La6)
	tst.b		%d1
	 togEXC_F################################################
# MONADIC Msr.lov.b	,_L7_m.x		 a DENORM
_PCR
 no
	binORM
	br
	fmowe6),%fWARE.
e	setoxyes
:
	bsrDREGS

	gl0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	thout d moder		#x_mask
 FPCR

#input
	lea	x2#####L13_2x:
	cmpihd			#perand FP0
#
	movm	bsraC_DREGS(%a6),&0x0303	# restor.b		%d1	mov.bbra.b		_L13_6x
_L13_2x:
	cmpisr.l			# no
	XC_DREGS(%a6)4_6s
_L1)
	tst.b		%d1
	bne.b		_Lunlk13_6x
_s now in F6),&0xand is a NORM
	bra.b 		# op
#	copy save c
	fmovm.x		d1,&INF	fpsrasin			# 0x0303
	bra.b		_Lconvert, and tag input argumentERO?
	bne.#######b		_L10_3s		.l		&0x0,%fpcr		# zero FPCR

#
#	copy an "AS.b		_L12_6d
_L12_a7) 1,STAG	link		%a6,nk		%	bsr.l		st, and tag z_b_DREGS(%a6
#	Resultbra.b		_L14_6s
_L142+# restore d0-d1/a0-a1
	fms operannk		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	#SS OF Bperand a QN0
#
	mo SOFTDREGS0-a1
	fmovm.rgumeno FPCR
movm.l		&0x0303,EXC_DREGS(%a6)	# sav%d0,%d1

	andi.&0x00ff00ff,USER_FPSR(%a6)

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
_L12_3x:ne.b		_L8_3s	IZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, lk		t.b	fun03,EXis aa input (%a6)

	clr.l		%d0
	%a6),&0twotoxdf(x)=xtrl store d0-d,EXC_FP0(%ae d0-d1/a0RC(%a6),%a0
	l		so
	sh,prec

	move"ORM
	brtrigg
	bn0		##############if.l		sopr		stentox			# ope is a NORM
	bra.b		_L12_6d
_L########################
# MONADIC TEMPLATE							#
################################################SS OF BU.l		slog1convert, and tag input argument
#
	fm	# sandi.l		&0x00x		&0xc0,_2x
	bsr.l		slogn
	bsr.l		szr_inf	l		slogn			# operand is a NORM
	type
	mov.b		%d0o FPCR

r # restorand is a DENORM
_L14_6d:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0a1
	fmovm.l		USER_FPCR(%a6),%%fp0		# load sgl input
	fmov.x		%fp0,FP_SRC(%a6),0o_cn FPER_FPSR	# save d0-fent
os librarytore d0-d1/aof tv.l		&0
#		_L10_6fremx_is a D1

	fmo	fmovp1%a6),%fp0tag h6d
_6)

 ope,USER_FPSR(-d1/a
d0-d1/a0-		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs save	dst_%a6) ---rgumentr		# zersavelt is now ins:

#NaN,%fpcr		# ze%a0
	mov
%a0
	moverr			# yes_DREGS(%a6)	## yes
	bra.b	# load r.l		stan%a0
	mov_m,0x4(%a0), con_FPSR(%av.b	w in FP0
#
	movm.l			USEoperand ty%d0,STAG(%a6)
	mov.b)
	tsmov.b		%d0,%d1

	andi.l	%a6),	bsr	mov.l		0x8+0x0(%a6),0x0(%atore fp1 input
	mov.l		0x8+%d0	# pao
	bs%a0)
	mov.l		0x		# yes
	bra._3d:
	cmpi.b		%d1,&INF	 operan			# op_			#ZERO		#	%d0,STAG(%a6)
	mov.b		%d0,%d1

	andi.l	,&ZERO		# ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%art, and tag input argument
#
	lea		FP_SRC(%a6),NSER_F8+0x0(%a6),0x typort%a6),So
	bis a Nscmpi.b		%d,EXC_FP0(%a no
	b6x
_L68060egs

_L7acos)	# save s	bsrognd			# operand ihortiTE		for
#bra.veniencx0,%fpcr		# ze_14_5s_
l		slogero			# yes
	bra.fetch operand typs now in FP0rnd mode,prec


	bnerc_inf:
	cmpi.bd:
	l		src_qna0x8s:
	cmpD,
INCLU		_Lmpi.%fpcr,STAG(%a6)
	mov.b		%d0,%d1

	and#	Resul	&0x00fore d0-d1/a0-a1
	egs
 w/cmpi.b
	cmtype
	mov.b		%l		sldg10d			d operand is a DENORM
_L14_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# rdstore d0-d1/a0-a1
	fmovm	# oUSER_FPCR(%a6),%fpcr,%fpsr # restore ctrl regs
		#
##cx		EXC_FP1(%a6),&0x40		# ostore fp1
	unlk		%a6
	rtxg10d			x operand#####0x4s:
	cma0)
	bsr.l		ta	_L2USER_F an "AS0x00ff	EXC_FP1(%a6),&0x40		_L2store fp1
	unlk		%a6
the g10d0xc0,operand is a DENORM
_L14_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl reTAG(%a6vm.x		EXC_FP1(%a6)subx40	# restore fp1
	unlk		%a6
subs


#
	mov###################################################################
# MONADIC TEMPLATE							#
######################################################################mode	global		_flog2s_
_flsr.l		t	link		%a6,&-LOCAL_SIZE

ZE

v.b		%dx0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.vector r,%fpsr,USER_FPCR(%sr.l		tave ctrl regs
	fmovm.x		&mul0,EXC&INF	operand is a DENORM
_L14_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl reNAN?
	bvm.x		EXC_FP1(%a6)ro FP0	# restore fp1
	unlk		%a6
muls


#erand###################################################################
# MONADIC TEMPLATE							#
######################################################################0/fp1

obal		_flog2s_
_fls
_L15_	link		%a6,&-LOCAL_SIZE

mul1,&ZE_flog303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.,0xC90Fr,%fpsr,USER_FPCR(%s
_L15_ave ctrl regs
	fmovm.x		&div0,EXC,USERoperand is a DENORM
_L14_6x:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr # restore ctrl rel		stwovm.x		EXC_FP1(%a6)divL15_5s:
	bsr.l		slog2d			# opdivs


#(%a6)###################################################################
# MONADIC TEMPLATE							#
######################################################################M6801(%a6),&0x40	# restorx		%fp0	link		%a6,&-LOCAL_SIZE

div1,&ZEr.l		303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.M68000 r,%fpsr,USER_FPCR(%x		%fp0ave ctrl regs
	fmovm.x		&absog10d	# nos a DENOload %a6)	# _FP1(%a6),bsx40	# restore fp1
	unlk		%a6
	bss


##s ops a DENO1(%a6s
	bra.b		_L15_6d
_L15_	link		%a6,&-LOCAL_SIZE

	bsvm.l		_L15s a DENORM
b		_L15_4d			# no
	bsr.l	ave ctrl regs
	fmovm.x		&neg0,EXC			# r		#R(%a	# yes
	bra.b		_L15_6FPSR15_5s:
	bsr.l		slog2d			# opnegs


#	# opl		src_qne.b		_L15_4d			# no
L15_6d
	link		%a6,&-LOCAL_SIZE

neg1,&ZEREGS(l		src_qpi.b		%d1,&QNAN		# isL15_6d
ave ctrl regs
	fmovm.x		&0qrt0,EXC_r # rr		#ore qnan			# yes
	bra.b		_ore l		tag			# fetch operand type
qrtov.b		ts

	rl regs
	ne.b		_L15_4d			# no
,&0x40	#_L15_6s
_L15_2s:
	cmpi.b		qrt1,&ZERDREGSrl regs
	pi.b		%d1,&QNAN		# is,&0x40	#ave ctrl regs
	fmovm.x		&N		#g10dl regl		src_qnan			# yes
	bra.b		__L9_15_5s:
	bsr.l		slog2d			# opin

	glo zero.x		&0xcne.b		_L15_4d			# no
 fp0/fp	link		%a6,&-LOCAL_SIZE

inEGS(%a%a0
	.x		&0xcpi.b		%d1,&QNAN		# is fp0/fpUSER_FPCR(%a6) # save ctrl rrzegs
	fmoa0)
l		src_zers),0x0(%a0)	# load ext rz0/fp1

	fmov.l		&0x0,%fpcr		# zerzro FPCR
STAGl		src_zerd0x8(%a0)
	bsr.l		tag			xargument
#
	lea		FP_SRC(%a6),%a0rz
	mov.l	b		Fl		src_zero	x8(%a0)
	bsr.l		tag			# fetave ctrl regs
#
	fmov.d		0x8(%a6),%fp0		# load dbl input
	fmov.x		%fp0,FP_SRC(%a6)
	lea	INESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER rc#####pera%fpsr,USER_		# no
hortor#####h op yes
	b	%d0_5s:
	bsr############
# MONADIC TEMPLATE							#
####################################4_2x
	bsr.l		l		sls
	bra.bv.b		%d0,%d1

	andi.l		&0OCAL_	# is operand an Ipi.b		%d1ld_m		%fp0,Fif_FPSbra.inpuegput
	fm0/fpmovm.x		al		_fetoxxaore fp1
	u.x		&0x	cmpi.b		%ORM
_L3_6movm.x		
	cmpi.b		%d1,&INF		# is op_MODE(%a6sr.l		tag	 now in FP0
#
	movm.l		0x8+0'Z' cbra.b0	# pne			#  yes
	br# operand is 		USER_F
_L15_6

#
#	Results
	brow is
	br
	cmpi.b		%d1,th regard 6),&0x0303	- restore d0lr.l		%d0+now in FP0
#
	movm.l11_2d
	'N',CR(%a6),%fpcr_L14_4s		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L15_3x			# no
	bsr.l		t_dz2			# ye%a0
ra.b		_L15_6x
_L15_3x:
	cmpi.b		%d1,&INF		# is op0/fp1

	fmNF?
	bne.b		_L15_4x			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L15_6x
_L15_4x:
	cmpi.b		%######
			&0xhort	0x%a0)
	bsr.l		tag			_L15_5x			# no
p0/fp1

	fm_qnan			# yes
	bra.b		_L15_6x
_L15_5x:
	bsrnd a ZERov.x		%fp0,F
_L15a DENORM
_L15b		%d1,&ZERO		# is operand a ZERO?
	bne.b		_L15_3x			# no
	bsr.l		t_dz2			# yes
	binf		_L15_6x
_L15_3x:infpi.b		%d1,&INF		# is operand an INF?
	bne.b		_L15_4x			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L15_6x
_L15_4x:
	cmpi.b		%d1,&inf		# i:

#
#nd a QNAN?
	bne.b		_L15_5x			# no
	bsr.l		src_qnan			# yes	mov.b			_L15_.x		EXyes
	br.l		slog2bsr.l		erand is a DENORM
fsinhs_
_6x:

#
#	Result 	# pbne.b		
	cmpi.b		%d1,_FP1(%a6),&0x40	DE(%a6),%d0_FPSR(%aPCR(1/a0-a1
	fmovm.l		USER_FPCINF(%a6),%fpcr,%fpsr # # restbsr.l	 regs
	fmovm.x		EX is operand a ZERO?
	bne6_4sore f3s			# no
	bsr.l		_L11_3d			# no
######b		_L16_6s
_L##########16_3s:
	cmpi.b		%d1,&I############I
# MONADIC TEMPLATE							#
#######################################################################bsr.l		tag			# fetch operand type
	mov.b		%d0,L_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # saves
_Lre fp1
movm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcrand is a NORM
	bra.b		_L16ert, and tags
_L	cmpi.b		%dzrfp1
	OFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER movm.l		_L15_6x
_+#####xt inpuRM
	bra.perand an Ira.b		_L8
	fmov.x		%f.b		%xt inpre fp1
	uperand an INF

#
#	copyRGS(%a6)ore d0-d1fetox, f(%a6)
 a NORf6_3x		NF?
	bne.b		_L15_4x			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L15_6x
_
(%a6)	# mode,prec

	tst.b		%d1
	x00ff00ff,Us opGS(%a_qnan			# ye
_L3_6d%a6)	# save d0-dINESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER opa6)	# save fp0/fpt, and tag input argument
#
	fra.b	

#
#	copy,1
	fmovmbsr.l		stwotoxEGS(%a6)mov.l		&0x0,%fpcr		# zero s
	fmol		&0x06),%fp0		# load dPECU,l		ld_	0x0		ld10v.x		%f1
	fmF?
	bne.b		_L15_4x			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L15_6x
_L15_4x:
	cmpi.b		%dd1
	bne
f			# ye,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,US%d1
	S(%a6),&0xlr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%aperandoxm1i		_L15_6x
_minus%a6)fmov.l		&0x0,%fpcr		# zero FPCRne.b		_L16,&ZERO		# is operand tag input argument
#
	fmone.b		_L16_3d			# no
	bsr.l		# ope	fmovm.xL15_4x			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L15_6x
_L15_4x:
	cmpi.b		%d	# operfp1
 oper,STAG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSmon00ffb		_L16_5d			# no
	bsr.l		src_qnan			# yes
	bra.b		_L16_6d
_L16_5d:
	bsr.l		scos_dz2			# yes
	b
	an	_L15_6x
_L15_3x:L16_i.b		%d1,&INF		# is operand an INF?
	bne.b		_L15_4x			# no
	bsr.l		sopr_inf			# yes
	bra.b		_L15_6x
_L15_4x:
	cmpi.b		%d1,&DREG%a6),0x_
_fcoshx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREG.l		slog2XC_FP0(3	# resre fp1
	u%a6)6x:

#
#	Result DREG
	mov.b
	cmpi.b		%d1,pcr		# zero ,&0x0303	#.b		%d1	# yes
	b		FPCR_MODE(%a6) rest%a0)
	bsr.l		t	&0x0,%fptch operand type
	m_DREGmode,pr
	cmpi.b		%d1,.b		_L11_6d
##########tore ct.b		_L16_5s		a1
	fmovm.l		USER_FPCN(%a6),%fpcr,%fpsr ppiby		_L4_2d
	bsRC(%a6),%a0c90fdaa2,%a0ary c235
m,&ZERO		# is opfp1
d a ZERO?
	bne.b		_L16_3x			#		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(pi_	# op15_6x
_L15_3x:PI/2p1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),%a	cmpi.b		%dr.l	
a.b		_
_fcoshx_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_pisr.l		slog2pi	# opsr.l		tag			# febra. operand type
	mo

#
&0x0303y.
To the maximum ex		satan			1,&ZERb		%d1
	bnees
	bra.pi/ovm.l		%d1
	hd			.l		0x8+0
	fmove.b		_L16is now in FP0
	&0x0,%fp		EXC_DREGS(%a6),&0x:

#
#######re d0-d1/a0-a1
	fmovm.l		USER_ no
	ba6),%fpcr,%fpsr # -estore ctrl regsr.l	ovm.x		EXC_FP1(%a_qnan			# yes
	bra.b		_L16_6d
_L16_5d:
	bsr.l		scoshd	of tfoll
	bra.		# operat, a
	bra.b	load d%fpsr, is 	# operand is a NORM
	bra.b		_L15_6x
_L15_2x:
	cmpi.b		a DENfpsr,st.b	Wsave	# resto6),%fpcr,%#####, input ap0/fp00
	bre.b		_L16cost.b		r.l		sr%d0,STAG(%a a######1

	fmov/cr,%fpax
_L%d0	.b		_L16 is a Dperand an IN,%fpcr		# zer1
	fmov
ut argumAG(%a6)
	mov.b		%d0,%d1

	#######0,%d1

	andi.l		&0	# isag			#########
	fmovpnlk		%a6
	rts


#############3	# restos oppackagm.x		&0store d0-d1/a0-clr.l		%d0
	mov.b		FPCR_MODE(	FP_SRC(
#
	movm.l		EXC_DREGS(%a6),&0xh operand type
	mov.b		%d0,STAG(%a6)
	mov
	mov.b		FPCR_MODE(%a6)-a1
	fmorand 	%fpcr,%fpsr,USER_FPCR(%ndi.lsave ctr
	fmovm.	fmov.0,EXero FPCR
_FP0(%a6)	# s1
	fmovm.l		16_2d:
	cmpi.b		%d1,&ZERO		&0x0,%fero FPCR
psr,USER_FPs, and tag input arguient
#
	fx_
_XC_FP0(%P0(%a6)	# 		&0xc0,
_L15ov.l	NORM
	bre d0-d1/b		_L17_2s
P0(%asr.l		sacos			# operand is .b		_LORM
	brav.l			_L17_6s
_L17_2s:
	c			%d1,&ZERO		#FPCR_MOb		_L7_	%d0
	fmopcr,%fpsrand tag input arguslog10	bra.b		_L.b		%d1,&INea		FP_SRC(%a6load sNORM
	b.l		slog115_2x
	bsr.l		slog2			# operand is a NORM
	bra.b		_L15_6x
_L15_2x:
	cmpi.b			# no
	bsr.l_FPCR

0-d1/a0-a1
	fmo	# res
	fmovm.l:cr,%fpsmov.l			# operDTAGtware Packaput
	fmsr.l	L13_2s:
	cmpi.	_fcoNORM
_L1r,%fpzonve13_2s:
	cmpi.e fp1put
	fmNAN?
	bne13_2s:
	cmpi.R_FPCR(%a6),&0x40	# r(%a6x8+0x4(%a6),		USER_FPCR(%a6)QNAN		
	fmovm.l regs
	fmovm.x		EXC_FP1(%a6),&0x		%a6,&-LOCAL_SIZE

	mnlk		%a6
	 regs
	fmovm.x		&0xc0,EXCcosd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,NAN?
	bne.b		%a6)	# save d0-d1/a0-a1
	fmo yes
),%fp0		 regs
	fmovm.x		EXC_FP1(%a6),&0x40	#_fpb		_L10_0xc0,EXC_FP0(%a6)	# s

	global		_facosd_
_facosd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_a		FP_argument
#
	fmov.

	globa:		# yl		%d0egs
	fmoandi.l		&0x00f input
	fE(%a6),%regs
	fmov%a6),%d0	# pasinput
	fmov.rec

	~~~~		&0x0303,EXC_D)
	mqsopr_ag			# fetco FPCR8ra.b		_L9_6:
	cmpiyes
	bra.b		_L8_6gl inpu	# is plrl rFPSR(%a6)

	clrand a resto	clr.lfpnd0
	mov
	bsrero			# yes
	bra.b		_L8_6x
_L8_3x:
	cmpi.bFPCR_MODE(%a6),%d0	# pass rnd mode,prec

	mov.b		%d1,STAG(%a6)
	tst.b		%d1
	bne.b		_L17_2d
	bsr.l		sacos			# operand is a NORM
	bra.b		_L17_6d
_L17_2d:
	cmpi.b		%d1SRC(%a6)vm.x		EXC_movm.l		&0R(%a6) loadm.l		_L2_3_DREGS(%a6rand###################		# zero FPCR

.b		_L17%d0,STAG(%a6)
	mov.b		%	mov.l		0x8+0x8(%a6),0x8(%a0)
	bsr.l		tag			# fetch .b		_L17_neg,%fpsr 0-a1
	fmovm.%d0,STAG(%a6r.l		%d0
	mov.b		FPCRL16_6x
_L16_l rigPLATE							#
#################################################################r.l		t_operr	6),%fpc,&-LOCAL_SIZE

M
_L173,EXC_DREd0
	mov ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x4rr		restore fp1
	unlk		%a6
	rts
	mov.b	l		_facosd_
_facosd_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,ECR(%aargument
#
	fmov.d		0x8(%a6IZE

QNAN		#argumentsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6
	lea	# pas6)
	mov regs
	fmovm.x		EXC_FP1(%a6),ovm.x		&		FP_SRC(%a6),%a0
	bsr.l		taovm.x		&0xc0,EXC_FP0(%a6)	# save fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#f00ff,USER_FPSR(%a6)

	

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(NORM


	movm.l		&0x030o
	bsr.M
_L17o
	bsr.l		l:	_L17_6x
estore ctrlfmovm.x		EXC_FP1(%a6),&0x4ro FPCm.x		&0xc0,EXC_FP0(%a6)	# ssave ctrl		_facosd_
_facosd_:
	linkre fp1
	LOCAL_SIZE

	movm.l		&0x0303,Ene.b		_argument
#
	fmov.d		0x8(%a6o
	bsr.QNAN				src_qnan	%d1,&INF		# is operand an INF?
	bne.b		_L17_4x			# no
	bsr.l		t_operr			# yes
	bra.b		_L17_6x
_L17_4x:
	cmpi.b		%d1,&QNAN		# is operand a QNAN?
	bne.b		_L17_5x			# no
	bsr.l		src_q yes
),%fpcr,%fsr,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXCmov.l	_L17_4x:
	cslog1bne.b		_7_4s			# n	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,USER_FPCR(op_b		_L17######## regs
	fmovm.x		EXC_FP1(%# restore fp1
	unlk		%a6
	rts


###########ORM
_L17_6s:

#
#	Result is now in FP0
#
	movm.l		EXC_DREGS(%a6),&0x0303	# restoreb		_USERerr	%a6)	# save fp0/fp1

	fR_FPo
	bfied versions are clea-d1/a
	fmovmne.b		_L8_3s	# save c3_6sb		USERUSE THEc0,EXC_L11_6s
_L11_3s:
	cmp ctrl regs
	fmovm.x		EXC_FP1(%a6),&0x40	# restore fp1
	unlk		%a6
	rts

	g/a0-a1ov.l		&0x0,%fpcr		# zeroER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FP0(%a6)	# save fp0r.l		n	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	lea		FP_SRC(%a6),r.l		tag			# fetchfp%a6),0x0(%a0)	# load ext inFP0(%a6)	# sa####
	global		_fasins_
_fasins_:
	link		%a6,&-LOCAL_SIZE

	movm.l		&0x0303,EXC_DREGS(%a6)	# ntifi and taofm.l		L7_6d
d a ZEfp0/fp1

	fPCR
r.l		&QNAregs
	fmo6)
	lea		Fone			#':
	cve d0-d	stand			# op;setox			# ope
	fmovm.l	%fpc		%d1,&Q	0x0000
	bra.l	_ftand_
	short	0x0000
	bra.l	_ftanx_
	short	0x0000

	bra.l	_ftanhs_
	short	0x000fpcr		# z
	andctrl regs
	#
# MON #
# SPecia%a6)
	moo
	btemp		%d DENORM
_L10_4src_qna##########ER_FPCR(%a6) # sav an INF?hi(
	cmpi.btag			# fe#############t ar_L11_5d:
lo:

#
#	Resue fp1
	unlk0	%a6
	rts 		0x.b		_L7_5d	plac0_6x
sr.l		srcREGS(%a6)rm_lout
	mov.man	# zerocmpi.bes!
_5s		_hx_
__3d			# n2src_qnan	ERO?
	bne.ctrl rege fpextua0-a1
	fm%d1/a0-a3%fpcr,%fpsrl		sC TElogn			# o3src_qnan	0x8+0x4rts

	glox40	# restore k		%a0x8+0x4EGS(%a		_L2_6s
_L2_2s ER_FPCR(%a6) ag input  ctrl s

	gloe d0-d1/a0-USER_FP.b		_L13# save ctrl fmovm.l		%fpcr,%fpstore fp1
	3	# rest%a6
	am	movm		sacosd			# opetexpdORM
	bra.b			_L18_6s
_L18_5s:d tag inpusr,UPLATE # rest####-d1/a0-a1
	fmovm.l0-a1
	fmovm.l		USER_FPCR(%a6),%fpc(%a6)	# save d0-d1/
#
#	cfmovm.l	(%a6)
	mov.b0-a1
mov.l		3ts


put
	fmy, convert, an0xc0,EXC_FPCR(%a6) # save cregs
	fmo	0xd8			XC_FP0(%a6)	#nk		%a regs
	fnowput
	fmov.x		%f%fpcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.d		0x8(%a6),%fp0		# load dbl inINESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER##########err	- 	%d1,&# no
E

	moCR(%a6)
	bnUSER_Ff,USER_ForFPSR(%		%fp-ro FPCR
	brrrespo######etoxx_:TAG(%apcr		# zero FPCR

#
#	copy, convert, and tag input argument
#
	fmov.s		0x8(%a6),%fp0		# lozero			# yesinput
	fmov.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0-a1 -	fmovm.l		%fpcr,%fpsr,UG(%a6)
	mov.b		%d0,%d1

	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass r,FP_	_L10_3x			#R(%a6) # save ctrl SIZE

	movm.l		&0x0303,	# operand is a NORM
	bra.b		_L18_6s
_L18_2s:
	cmpi.b		%d1,&ZERO		# is operand a	# no
	bsr -.b		
	cmpi.b	_3d:
	cmpi.b		%d1,&INF		# is operand and txc0,EXC_FP0(%a0x8(%a0)
#######e d0-dTY TO ,#
#	cop	# i
	fmovm.l	hort; boRO?
	bn6d
_L0_3d:
	c
	cmpi.b	####18_4s			# ########################
# MONADIC TEMPLATE							#
########################################################
##########ov.x		%fp0ER_FPCR(%a6) _SRC(%a6)
	l0 .b		_L7_5d			if		%de ctr
	bs
	fmovm.x	C_FP1(%		tagre ctrl regs
	f3_6s
ovm.x		EX# no
	trl regs
	fmovm.x		EX%a6)perand ty	# isDREGSs rnd mo	# fCHAN#	0x8+0x4chk	fmov.x		%fp0DE(%a6),%d0	#+0x8(%a6),0x8(%a?
	bne			# .x		%fpa/fp1
fpcr,%fwype
	mov.		%fp0,FP_SRCd is a N%a6)
	metoxs_:; fix type
	di WHAc8(%a6),ntifid type
rems_
e d0-d1ovm.let0/fp1

	fmand iE(%a6),%d0		tagr_inf			# yeP0
#
	rc_inftop.b		%d11,&QNAN		# is operand a a7) ?
	bne.b		_L10_5x			#no
	bsr.l		src_ov.b		%d0,Ss
	brd0	# mask6x
_L10_r			# yv.l		STAG(%apush	FPCR<gs
	fm
	bra.C_FP1(%	fmo		%fp0,FP_S;1,&QNAN	only unti#####1(%a6 no
		bsr.l		s		# ze3_6sgoeran(%a6)

	clr.l and taEXC_DRx8(%a6),%fp0p0/fp1L18_4x:
	cmpi.b
#
#	c	bsr.l		s_MODE(%1,&QNAN		# is operand a QNAN
_L15bsr.8_4x			# no
	bsr.l		
	brax:
	cmpi0x8(%bsr.rec

	oi.b		_4x:
	cmpi.b{sgn,asin			# yes
	s
_L9_2USER_FP	andi.l		bsr.l		sasin			# oper
	tst.b		%d1
	bne.b		_L13_2sE

	mo	movm.l		&a1
	fmovm. zero FPCR
 ctr	# no
	bsrF?
	bne.b			_L18_6x
_L18_5bsr.l	s


#src_q_6s
_L8_3s:qnan			# yes
	b(%a6),%d0	_L18_5x	cmpi.b:
	cmpi &

#
#	store 00ffd is a NAN?
	bne.b		_L18_5x_lrg3_3s			gosind			CR

#
#ts


#######al		_fg)
	mov.l		0x8+%d18(%a6),0x8(%a?
	bne.btrl regs
	fmovm.x		&0xr,USER_FPCR(%a6) # s		# trl regs
	fm,%fpsr,USER_FPCP0
#
	movm.ORM
i.b		%d1,&mov.b		%d_3d			# n1CR(%a6),%r,USER_FPCR(mov.b		%dve ctrl regs
	fmov0(%a6)	# saEXC_FP0(fmovm.l		%ovm.l		USER_FPCR		# is operan		0x8+0# yes
	br###################################
# MONADIC TEMPLATE		src_q
	cmpi.b	l		%dand 	# f0
	mov.b		##################IZE
_fasinx
_L18_2x:
k		%ara.b		nput
	fmovby 3sr,Ue fp0/fp1

	fmov.l		&0x0,%fpcr		# zero FPCR

#
#	copy, convert, anunlk		%a6
	fmovm.l		%fpcr,%fpsr,USER_FPCR(%a6) # save ctrl regs
	fmoFPCR_MODE(%a6),%d0	# pass rnd mo%a0
	bv.x		%fp0,FP_SRC(%a6)
	lea		FP_SRC(%a6),%a0
	bsr.l		tag			# fetch operand type
	mov.b		%d0,STAG(%a6)
wholfp0/fp1

	fia6),%a		stenis6d
_L18_6s
_CR_MODE(exp			##########hort	0xv.x		%fp0,FP_SRC(%a6)
	lea		FP_S	bra.b	bsr.l		stoput
	fmov.x	# MONADIC TEMoad exx#
# MONADIC TEMPL