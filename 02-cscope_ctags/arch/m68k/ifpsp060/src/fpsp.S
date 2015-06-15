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
# freal.s:
#	This file is appended to the top of the 060FPSP package
# and contains the entry points into the package. The user, in
# effect, branches to one of the branch table entries located
# after _060FPSP_TABLE.
#	Also, subroutine stubs exist in this file (_fpsp_done for
# example) that are referenced by the FPSP package itself in order
# to call a given routine. The stub routine actually performs the
# callout. The FPSP code does a "bsr" to the stub routine. This
# extra layer of hierarchy adds a slight performance penalty but
# it makes the FPSP code easier to read and more mainatinable.
#

set	_off_bsun,	0x00
set	_off_snan,	0x04
set	_off_operr,	0x08
set	_off_ovfl,	0x0c
set	_off_unfl,	0x10
set	_off_dz,	0x14
set	_off_inex,	0x18
set	_off_fline,	0x1c
set	_off_fpu_dis,	0x20
set	_off_trap,	0x24
set	_off_trace,	0x28
set	_off_access,	0x2c
set	_off_done,	0x30

set	_off_imr,	0x40
set	_off_dmr,	0x44
set	_off_dmw,	0x48
set	_off_irw,	0x4c
set	_off_irl,	0x50
set	_off_drb,	0x54
set	_off_drw,	0x58
set	_off_drl,	0x5c
set	_off_dwb,	0x60
set	_off_dww,	0x64
set	_off_dwl,	0x68

_060FPSP_TABLE:

###############################################################

# Here's the table of ENTRY POINTS for those linking the package.
	bra.l		_fpsp_snan
	short		0x0000
	bra.l		_fpsp_operr
	short		0x0000
	bra.l		_fpsp_ovfl
	short		0x0000
	bra.l		_fpsp_unfl
	short		0x0000
	bra.l		_fpsp_dz
	short		0x0000
	bra.l		_fpsp_inex
	short		0x0000
	bra.l		_fpsp_fline
	short		0x0000
	bra.l		_fpsp_unsupp
	short		0x0000
	bra.l		_fpsp_effadd
	short		0x0000

	space		56

###############################################################
	global		_fpsp_done
_fpsp_done:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_ovfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_unfl
_real_unfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_unfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_inex
_real_inex:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_inex,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_bsun
_real_bsun:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_bsun,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_operr
_real_operr:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_operr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_snan
_real_snan:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dz,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fline
_real_fline:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fline,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_disabled:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fpu_dis,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trap
_real_trap:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trap,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trace
_real_trace:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trace,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_access
_real_access:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_access,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

#######################################

	global		_imem_read
_imem_read:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_imr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read
_dmem_read:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dmr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write
_dmem_write:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dmw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_word
_imem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_irw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_long
_imem_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_irl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_byte
_dmem_read_byte:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_word
_dmem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_long
_dmem_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_byte
_dmem_write_byte:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dwb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_word
_dmem_write_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dww,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_long
_dmem_write_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dwl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

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
# XDEF ****************************************************************	#
#	_fpsp_ovfl(): 060FPSP entry point for FP Overflow exception.	#
#									#
#	This handler should be the first code executed upon taking the	#
#	FP Overflow exception in an operating system.			#
#									#
# XREF ****************************************************************	#
#	_imem_read_long() - read instruction longword			#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tag_x() - determine optype of src/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result to FP regfile	#
#	unnorm_fix() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP regfile			#
#	fout() - emulate an opclass 3 instruction			#
#	tbl_unsupp - add of table of emulation routines for opclass 0,2	#
#	_fpsp_done() - "callout" for 060FPSP exit (all work done!)	#
#	_real_ovfl() - "callout" for Overflow exception enabled code	#
#	_real_inex() - "callout" for Inexact exception enabled code	#
#	_real_trace() - "callout" for Trace exception code		#
#									#
# INPUT ***************************************************************	#
#	- The system stack contains the FP Ovfl exception stack frame	#
#	- The fsave frame contains the source operand			#
#									#
# OUTPUT **************************************************************	#
#	Overflow Exception enabled:					#
#	- The system stack is unchanged					#
#	- The fsave frame contains the adjusted src op for opclass 0,2	#
#	Overflow Exception disabled:					#
#	- The system stack is unchanged					#
#	- The "exception present" flag in the fsave frame is cleared	#
#									#
# ALGORITHM ***********************************************************	#
#	On the 060, if an FP overflow is present as the result of any	#
# instruction, the 060 will take an overflow exception whether the	#
# exception is enabled or disabled in the FPCR. For the disabled case,	#
# This handler emulates the instruction to determine what the correct	#
# default result should be for the operation. This default result is	#
# then stored in either the FP regfile, data regfile, or memory.	#
# Finally, the handler exits through the "callout" _fpsp_done()		#
# denoting that no exceptional conditions exist within the machine.	#
#	If the exception is enabled, then this handler must create the	#
# exceptional operand and plave it in the fsave state frame, and store	#
# the default result (only if the instruction is opclass 3). For	#
# exceptions enabled, this handler must exit through the "callout"	#
# _real_ovfl() so that the operating system enabled overflow handler	#
# can handle this case.							#
#	Two other conditions exist. First, if overflow was disabled	#
# but the inexact exception was enabled, this handler must exit		#
# through the "callout" _real_inex() regardless of whether the result	#
# was inexact.								#
#	Also, in the case of an opclass three instruction where		#
# overflow was disabled and the trace exception was enabled, this	#
# handler must exit through the "callout" _real_trace().		#
#									#
#########################################################################

	global		_fpsp_ovfl
_fpsp_ovfl:

#$#	sub.l		&24,%sp			# make room for src/dst

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(%a6)		# grab the "busy" frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

# the FPIAR holds the "current PC" of the faulting instruction
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		# fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)

##############################################################################

	btst		&0x5,EXC_CMDREG(%a6)	# is instr an fmove out?
	bne.w		fovfl_out


	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		fix_skewed_ops		# fix src op

# since, I believe, only NORMs and DENORMs can come through here,
# maybe we can avoid the subroutine call.
	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		set_tag_x		# tag the operand type
	mov.b		%d0,STAG(%a6)		# maybe NORM,DENORM

# bit five of the fp extension word separates the monadic and dyadic operations
# that can pass through fpsp_ovfl(). remember that fcmp, ftst, and fsincos
# will never take this exception.
	btst		&0x5,1+EXC_CMDREG(%a6)	# is operation monadic or dyadic?
	beq.b		fovfl_extract		# monadic

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fovfl_op2_done		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO
fovfl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

fovfl_extract:

#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mov.l		FP_DST_HI(%a6),TRAP_DSTOP_HI(%a6)
#$#	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extension

	andi.l		&0x00ff01ff,USER_FPSR(%a6) # zero all but accured field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

# maybe we can make these entry points ONLY the OVFL entry points of each routine.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%d1 # fetch routine addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

# the operation has been emulated. the result is in fp0.
# the EXOP, if an exception occurred, is in fp1.
# we must save the default result regardless of whether
# traps are enabled or disabled.
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		store_fpreg

# the exceptional possibilities we have left ourselves with are ONLY overflow
# and inexact. and, the inexact is such that overflow occurred and was disabled
# but inexact was enabled.
	btst		&ovfl_bit,FPCR_ENABLE(%a6)
	bne.b		fovfl_ovfl_on

	btst		&inex2_bit,FPCR_ENABLE(%a6)
	bne.b		fovfl_inex_on

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp
	bra.l		_fpsp_done

# overflow is enabled AND overflow, of course, occurred. so, we have the EXOP
# in fp1. now, simply jump to _real_ovfl()!
fovfl_ovfl_on:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe005,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_ovfl

# overflow occurred but is disabled. meanwhile, inexact is enabled. therefore,
# we must jump to real_inex().
fovfl_inex_on:

	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.b		&0xc4,1+EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_inex

########################################################################
fovfl_out:


#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)

# the src operand is definitely a NORM(!), so tag it as such
	mov.b		&NORM,STAG(%a6)		# set src optype tag

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xffff00ff,USER_FPSR(%a6) # zero all but accured field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0		# pass ptr to src operand

	bsr.l		fout

	btst		&ovfl_bit,FPCR_ENABLE(%a6)
	bne.w		fovfl_ovfl_on

	btst		&inex2_bit,FPCR_ENABLE(%a6)
	bne.w		fovfl_inex_on

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_done		# no

	fmov.l		%fpiar,0x8(%sp)		# "Current PC" is in FPIAR
	mov.w		&0x2024,0x6(%sp)	# stk fmt = 0x2; voff = 0x024
	bra.l		_real_trace

#########################################################################
# XDEF ****************************************************************	#
#	_fpsp_unfl(): 060FPSP entry point for FP Underflow exception.	#
#									#
#	This handler should be the first code executed upon taking the	#
#	FP Underflow exception in an operating system.			#
#									#
# XREF ****************************************************************	#
#	_imem_read_long() - read instruction longword			#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tag_x() - determine optype of src/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result to FP regfile	#
#	unnorm_fix() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP regfile			#
#	fout() - emulate an opclass 3 instruction			#
#	tbl_unsupp - add of table of emulation routines for opclass 0,2	#
#	_fpsp_done() - "callout" for 060FPSP exit (all work done!)	#
#	_real_ovfl() - "callout" for Overflow exception enabled code	#
#	_real_inex() - "callout" for Inexact exception enabled code	#
#	_real_trace() - "callout" for Trace exception code		#
#									#
# INPUT ***************************************************************	#
#	- The system stack contains the FP Unfl exception stack frame	#
#	- The fsave frame contains the source operand			#
#									#
# OUTPUT **************************************************************	#
#	Underflow Exception enabled:					#
#	- The system stack is unchanged					#
#	- The fsave frame contains the adjusted src op for opclass 0,2	#
#	Underflow Exception disabled:					#
#	- The system stack is unchanged					#
#	- The "exception present" flag in the fsave frame is cleared	#
#									#
# ALGORITHM ***********************************************************	#
#	On the 060, if an FP underflow is present as the result of any	#
# instruction, the 060 will take an underflow exception whether the	#
# exception is enabled or disabled in the FPCR. For the disabled case,	#
# This handler emulates the instruction to determine what the correct	#
# default result should be for the operation. This default result is	#
# then stored in either the FP regfile, data regfile, or memory.	#
# Finally, the handler exits through the "callout" _fpsp_done()		#
# denoting that no exceptional conditions exist within the machine.	#
#	If the exception is enabled, then this handler must create the	#
# exceptional operand and plave it in the fsave state frame, and store	#
# the default result (only if the instruction is opclass 3). For	#
# exceptions enabled, this handler must exit through the "callout"	#
# _real_unfl() so that the operating system enabled overflow handler	#
# can handle this case.							#
#	Two other conditions exist. First, if underflow was disabled	#
# but the inexact exception was enabled and the result was inexact,	#
# this handler must exit through the "callout" _real_inex().		#
# was inexact.								#
#	Also, in the case of an opclass three instruction where		#
# underflow was disabled and the trace exception was enabled, this	#
# handler must exit through the "callout" _real_trace().		#
#									#
#########################################################################

	global		_fpsp_unfl
_fpsp_unfl:

#$#	sub.l		&24,%sp			# make room for src/dst

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(%a6)		# grab the "busy" frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

# the FPIAR holds the "current PC" of the faulting instruction
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		# fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)

##############################################################################

	btst		&0x5,EXC_CMDREG(%a6)	# is instr an fmove out?
	bne.w		funfl_out


	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		fix_skewed_ops		# fix src op

	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		set_tag_x		# tag the operand type
	mov.b		%d0,STAG(%a6)		# maybe NORM,DENORM

# bit five of the fp ext word separates the monadic and dyadic operations
# that can pass through fpsp_unfl(). remember that fcmp, and ftst
# will never take this exception.
	btst		&0x5,1+EXC_CMDREG(%a6)	# is op monadic or dyadic?
	beq.b		funfl_extract		# monadic

# now, what's left that's not dyadic is fsincos. we can distinguish it
# from all dyadics by the '0110xxx pattern
	btst		&0x4,1+EXC_CMDREG(%a6)	# is op an fsincos?
	bne.b		funfl_extract		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		funfl_op2_done		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO
funfl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mov.l		FP_DST_HI(%a6),TRAP_DSTOP_HI(%a6)
#$#	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extension

	andi.l		&0x00ff01ff,USER_FPSR(%a6)

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

# maybe we can make these entry points ONLY the OVFL entry points of each routine.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%d1 # fetch routine addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		store_fpreg

# The `060 FPU multiplier hardware is such that if the result of a
# multiply operation is the smallest possible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception. Since this is incorrect, we need to check
# if our emulation, after re-doing the operation, decided that
# no underflow was called for. We do these checks only in
# funfl_{unfl,inex}_on() because w/ both exceptions disabled, this
# special case will simply exit gracefully with the correct result.

# the exceptional possibilities we have left ourselves with are ONLY overflow
# and inexact. and, the inexact is such that overflow occurred and was disabled
# but inexact was enabled.
	btst		&unfl_bit,FPCR_ENABLE(%a6)
	bne.b		funfl_unfl_on

funfl_chkinex:
	btst		&inex2_bit,FPCR_ENABLE(%a6)
	bne.b		funfl_inex_on

funfl_exit:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp
	bra.l		_fpsp_done

# overflow is enabled AND overflow, of course, occurred. so, we have the EXOP
# in fp1 (don't forget to save fp0). what to do now?
# well, we simply have to get to go to _real_unfl()!
funfl_unfl_on:

# The `060 FPU multiplier hardware is such that if the result of a
# multiply operation is the smallest possible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception. Since this is incorrect, we check here to see
# if our emulation, after re-doing the operation, decided that
# no underflow was called for.
	btst		&unfl_bit,FPSR_EXCEPT(%a6)
	beq.w		funfl_chkinex

funfl_unfl_on2:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe003,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_unfl

# undeflow occurred but is disabled. meanwhile, inexact is enabled. therefore,
# we must jump to real_inex().
funfl_inex_on:

# The `060 FPU multiplier hardware is such that if the result of a
# multiply operation is the smallest possible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception.
# But, whether bogus or not, if inexact is enabled AND it occurred,
# then we have to branch to real_inex.

	btst		&inex2_bit,FPSR_EXCEPT(%a6)
	beq.w		funfl_exit

funfl_inex_on2:

	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to stack

	mov.b		&0xc4,1+EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_inex

#######################################################################
funfl_out:


#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)

# the src operand is definitely a NORM(!), so tag it as such
	mov.b		&NORM,STAG(%a6)		# set src optype tag

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xffff00ff,USER_FPSR(%a6) # zero all but accured field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0		# pass ptr to src operand

	bsr.l		fout

	btst		&unfl_bit,FPCR_ENABLE(%a6)
	bne.w		funfl_unfl_on2

	btst		&inex2_bit,FPCR_ENABLE(%a6)
	bne.w		funfl_inex_on2

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_done		# no

	fmov.l		%fpiar,0x8(%sp)		# "Current PC" is in FPIAR
	mov.w		&0x2024,0x6(%sp)	# stk fmt = 0x2; voff = 0x024
	bra.l		_real_trace

#########################################################################
# XDEF ****************************************************************	#
#	_fpsp_unsupp(): 060FPSP entry point for FP "Unimplemented	#
#		        Data Type" exception.				#
#									#
#	This handler should be the first code executed upon taking the	#
#	FP Unimplemented Data Type exception in an operating system.	#
#									#
# XREF ****************************************************************	#
#	_imem_read_{word,long}() - read instruction word/longword	#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tag_x() - determine optype of src/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result to FP regfile	#
#	unnorm_fix() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP regfile			#
#	load_fpn1() - load src operand from FP regfile			#
#	fout() - emulate an opclass 3 instruction			#
#	tbl_unsupp - add of table of emulation routines for opclass 0,2	#
#	_real_inex() - "callout" to operating system inexact handler	#
#	_fpsp_done() - "callout" for exit; work all done		#
#	_real_trace() - "callout" for Trace enabled exception		#
#	funimp_skew() - adjust fsave src ops to "incorrect" value	#
#	_real_snan() - "callout" for SNAN exception			#
#	_real_operr() - "callout" for OPERR exception			#
#	_real_ovfl() - "callout" for OVFL exception			#
#	_real_unfl() - "callout" for UNFL exception			#
#	get_packed() - fetch packed operand from memory			#
#									#
# INPUT ***************************************************************	#
#	- The system stack contains the "Unimp Data Type" stk frame	#
#	- The fsave frame contains the ssrc op (for UNNORM/DENORM)	#
#									#
# OUTPUT **************************************************************	#
#	If Inexact exception (opclass 3):				#
#	- The system stack is changed to an Inexact exception stk frame	#
#	If SNAN exception (opclass 3):					#
#	- The system stack is changed to an SNAN exception stk frame	#
#	If OPERR exception (opclass 3):					#
#	- The system stack is changed to an OPERR exception stk frame	#
#	If OVFL exception (opclass 3):					#
#	- The system stack is changed to an OVFL exception stk frame	#
#	If UNFL exception (opclass 3):					#
#	- The system stack is changed to an UNFL exception stack frame	#
#	If Trace exception enabled:					#
#	- The system stack is changed to a Trace exception stack frame	#
#	Else: (normal case)						#
#	- Correct result has been stored as appropriate			#
#									#
# ALGORITHM ***********************************************************	#
#	Two main instruction types can enter here: (1) DENORM or UNNORM	#
# unimplemented data types. These can be either opclass 0,2 or 3	#
# instructions, and (2) PACKED unimplemented data format instructions	#
# also of opclasses 0,2, or 3.						#
#	For UNNORM/DENORM opclass 0 and 2, the handler fetches the src	#
# operand from the fsave state frame and the dst operand (if dyadic)	#
# from the FP register file. The instruction is then emulated by	#
# choosing an emulation routine from a table of routines indexed by	#
# instruction type. Once the instruction has been emulated and result	#
# saved, then we check to see if any enabled exceptions resulted from	#
# instruction emulation. If none, then we exit through the "callout"	#
# _fpsp_done(). If there is an enabled FP exception, then we insert	#
# this exception into the FPU in the fsave state frame and then exit	#
# through _fpsp_done().							#
#	PACKED opclass 0 and 2 is similar in how the instruction is	#
# emulated and exceptions handled. The differences occur in how the	#
# handler loads the packed op (by calling get_packed() routine) and	#
# by the fact that a Trace exception could be pending for PACKED ops.	#
# If a Trace exception is pending, then the current exception stack	#
# frame is changed to a Trace exception stack frame and an exit is	#
# made through _real_trace().						#
#	For UNNORM/DENORM opclass 3, the actual move out to memory is	#
# performed by calling the routine fout(). If no exception should occur	#
# as the result of emulation, then an exit either occurs through	#
# _fpsp_done() or through _real_trace() if a Trace exception is pending	#
# (a Trace stack frame must be created here, too). If an FP exception	#
# should occur, then we must create an exception stack frame of that	#
# type and jump to either _real_snan(), _real_operr(), _real_inex(),	#
# _real_unfl(), or _real_ovfl() as appropriate. PACKED opclass 3	#
# emulation is performed in a similar manner.				#
#									#
#########################################################################

#
# (1) DENORM and UNNORM (unimplemented) data types:
#
#				post-instruction
#				*****************
#				*      EA	*
#	 pre-instruction	*		*
#	*****************	*****************
#	* 0x0 *  0x0dc  *	* 0x3 *  0x0dc  *
#	*****************	*****************
#	*     Next	*	*     Next	*
#	*      PC	*	*      PC	*
#	*****************	*****************
#	*      SR	*	*      SR	*
#	*****************	*****************
#
# (2) PACKED format (unsupported) opclasses two and three:
#	*****************
#	*      EA	*
#	*		*
#	*****************
#	* 0x2 *  0x0dc	*
#	*****************
#	*     Next	*
#	*      PC	*
#	*****************
#	*      SR	*
#	*****************
#
	global		_fpsp_unsupp
_fpsp_unsupp:

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(%a6)		# save fp state

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

	btst		&0x5,EXC_SR(%a6)	# user or supervisor mode?
	bne.b		fu_s
fu_u:
	mov.l		%usp,%a0		# fetch user stack pointer
	mov.l		%a0,EXC_A7(%a6)		# save on stack
	bra.b		fu_cont
# if the exception is an opclass zero or two unimplemented data type
# exception, then the a7' calculated here is wrong since it doesn't
# stack an ea. however, we don't need an a7' for this case anyways.
fu_s:
	lea		0x4+EXC_EA(%a6),%a0	# load old a7'
	mov.l		%a0,EXC_A7(%a6)		# save on stack

fu_cont:

# the FPIAR holds the "current PC" of the faulting instruction
# the FPIAR should be set correctly for ALL exceptions passing through
# this point.
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		# fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)	# store OPWORD and EXTWORD

############################

	clr.b		SPCOND_FLG(%a6)		# clear special condition flag

# Separate opclass three (fpn-to-mem) ops since they have a different
# stack frame and protocol.
	btst		&0x5,EXC_CMDREG(%a6)	# is it an fmove out?
	bne.w		fu_out			# yes

# Separate packed opclass two instructions.
	bfextu		EXC_CMDREG(%a6){&0:&6},%d0
	cmpi.b		%d0,&0x13
	beq.w		fu_in_pack


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-create them anyways, zero exception field
	andi.l		&0x00ff00ff,USER_FPSR(%a6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

# Opclass two w/ memory-to-fpn operation will have an incorrect extended
# precision format if the src format was single or double and the
# source data type was an INF, NAN, DENORM, or UNNORM
	lea		FP_SRC(%a6),%a0		# pass ptr to input
	bsr.l		fix_skewed_ops

# we don't know whether the src operand or the dst operand (or both) is the
# UNNORM or DENORM. call the function that tags the operand type. if the
# input is an UNNORM, then convert it to a NORM, DENORM, or ZERO.
	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fu_op2			# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO

fu_op2:
	mov.b		%d0,STAG(%a6)		# save src optype tag

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg

# bit five of the fp extension word separates the monadic and dyadic operations
# at this point
	btst		&0x5,1+EXC_CMDREG(%a6)	# is operation monadic or dyadic?
	beq.b		fu_extract		# monadic
	cmpi.b		1+EXC_CMDREG(%a6),&0x3a	# is operation an ftst?
	beq.b		fu_extract		# yes, so it's monadic, too

	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fu_op2_done		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO
fu_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

fu_extract:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	bfextu		1+EXC_CMDREG(%a6){&1:&7},%d1 # extract extension

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

	mov.l		(tbl_unsupp.l,%pc,%d1.l*4),%d1 # fetch routine addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

#
# Exceptions in order of precedence:
#	BSUN	: none
#	SNAN	: all dyadic ops
#	OPERR	: fsqrt(-NORM)
#	OVFL	: all except ftst,fcmp
#	UNFL	: all except ftst,fcmp
#	DZ	: fdiv
#	INEX2	: all except ftst,fcmp
#	INEX1	: none (packed doesn't go through here)
#

# we determine the highest priority exception(if any) set by the
# emulation routine that has also been enabled by the user.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceptions set
	bne.b		fu_in_ena		# some are enabled

fu_in_cont:
# fcmp and ftst do not store any result.
	mov.b		1+EXC_CMDREG(%a6),%d0	# fetch extension
	andi.b		&0x38,%d0		# extract bits 3-5
	cmpi.b		%d0,&0x38		# is instr fcmp or ftst?
	beq.b		fu_in_exit		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		store_fpreg		# store the result

fu_in_exit:

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6

	bra.l		_fpsp_done

fu_in_ena:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	bne.b		fu_in_exc		# there is at least one set

#
# No exceptions occurred that were also enabled. Now:
#
#	if (OVFL && ovfl_disabled && inexact_enabled) {
#	    branch to _real_inex() (even if the result was exact!);
#	} else {
#	    save the result in the proper fp reg (unless the op is fcmp or ftst);
#	    return;
#	}
#
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # was overflow set?
	beq.b		fu_in_cont		# no

fu_in_ovflchk:
	btst		&inex2_bit,FPCR_ENABLE(%a6) # was inexact enabled?
	beq.b		fu_in_cont		# no
	bra.w		fu_in_exc_ovfl		# go insert overflow frame

#
# An exception occurred and that exception was enabled:
#
#	shift enabled exception field into lo byte of d0;
#	if (((INEX2 || INEX1) && inex_enabled && OVFL && ovfl_disabled) ||
#	    ((INEX2 || INEX1) && inex_enabled && UNFL && unfl_disabled)) {
#		/*
#		 * this is the case where we must call _real_inex() now or else
#		 * there will be no other way to pass it the exceptional operand
#		 */
#		call _real_inex();
#	} else {
#		restore exc state (SNAN||OPERR||OVFL||UNFL||DZ||INEX) into the FPU;
#	}
#
fu_in_exc:
	subi.l		&24,%d0			# fix offset to be 0-8
	cmpi.b		%d0,&0x6		# is exception INEX? (6)
	bne.b		fu_in_exc_exit		# no

# the enabled exception was inexact
	btst		&unfl_bit,FPSR_EXCEPT(%a6) # did disabled underflow occur?
	bne.w		fu_in_exc_unfl		# yes
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # did disabled overflow occur?
	bne.w		fu_in_exc_ovfl		# yes

# here, we insert the correct fsave status value into the fsave frame for the
# corresponding exception. the operand in the fsave frame should be the original
# src operand.
fu_in_exc_exit:
	mov.l		%d0,-(%sp)		# save d0
	bsr.l		funimp_skew		# skew sgl or dbl inputs
	mov.l		(%sp)+,%d0		# restore d0

	mov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SRC(%a6) # create exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore src op

	unlk		%a6

	bra.l		_fpsp_done

tbl_except:
	short		0xe000,0xe006,0xe004,0xe005
	short		0xe003,0xe002,0xe001,0xe001

fu_in_exc_unfl:
	mov.w		&0x4,%d0
	bra.b		fu_in_exc_exit
fu_in_exc_ovfl:
	mov.w		&0x03,%d0
	bra.b		fu_in_exc_exit

# If the input operand to this operation was opclass two and a single
# or double precision denorm, inf, or nan, the operand needs to be
# "corrected" in order to have the proper equivalent extended precision
# number.
	global		fix_skewed_ops
fix_skewed_ops:
	bfextu		EXC_CMDREG(%a6){&0:&6},%d0 # extract opclass,src fmt
	cmpi.b		%d0,&0x11		# is class = 2 & fmt = sgl?
	beq.b		fso_sgl			# yes
	cmpi.b		%d0,&0x15		# is class = 2 & fmt = dbl?
	beq.b		fso_dbl			# yes
	rts					# no

fso_sgl:
	mov.w		LOCAL_EX(%a0),%d0	# fetch src exponent
	andi.w		&0x7fff,%d0		# strip sign
	cmpi.w		%d0,&0x3f80		# is |exp| == $3f80?
	beq.b		fso_sgl_dnrm_zero	# yes
	cmpi.w		%d0,&0x407f		# no; is |exp| == $407f?
	beq.b		fso_infnan		# yes
	rts					# no

fso_sgl_dnrm_zero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	beq.b		fso_zero		# it's a skewed zero
fso_sgl_dnrm:
# here, we count on norm not to alter a0...
	bsr.l		norm			# normalize mantissa
	neg.w		%d0			# -shft amt
	addi.w		&0x3f81,%d0		# adjust new exponent
	andi.w		&0x8000,LOCAL_EX(%a0)	# clear old exponent
	or.w		%d0,LOCAL_EX(%a0)	# insert new exponent
	rts

fso_zero:
	andi.w		&0x8000,LOCAL_EX(%a0)	# clear bogus exponent
	rts

fso_infnan:
	andi.b		&0x7f,LOCAL_HI(%a0)	# clear j-bit
	ori.w		&0x7fff,LOCAL_EX(%a0)	# make exponent = $7fff
	rts

fso_dbl:
	mov.w		LOCAL_EX(%a0),%d0	# fetch src exponent
	andi.w		&0x7fff,%d0		# strip sign
	cmpi.w		%d0,&0x3c00		# is |exp| == $3c00?
	beq.b		fso_dbl_dnrm_zero	# yes
	cmpi.w		%d0,&0x43ff		# no; is |exp| == $43ff?
	beq.b		fso_infnan		# yes
	rts					# no

fso_dbl_dnrm_zero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	bne.b		fso_dbl_dnrm		# it's a skewed denorm
	tst.l		LOCAL_LO(%a0)		# is it a zero?
	beq.b		fso_zero		# yes
fso_dbl_dnrm:
# here, we count on norm not to alter a0...
	bsr.l		norm			# normalize mantissa
	neg.w		%d0			# -shft amt
	addi.w		&0x3c01,%d0		# adjust new exponent
	andi.w		&0x8000,LOCAL_EX(%a0)	# clear old exponent
	or.w		%d0,LOCAL_EX(%a0)	# insert new exponent
	rts

#################################################################

# fmove out took an unimplemented data type exception.
# the src operand is in FP_SRC. Call _fout() to write out the result and
# to determine which exceptions, if any, to take.
fu_out:

# Separate packed move outs from the UNNORM and DENORM move outs.
	bfextu		EXC_CMDREG(%a6){&3:&3},%d0
	cmpi.b		%d0,&0x3
	beq.w		fu_out_pack
	cmpi.b		%d0,&0x7
	beq.w		fu_out_pack


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-create them anyways, zero exception field.
# fmove out doesn't affect ccodes.
	and.l		&0xffff00ff,USER_FPSR(%a6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

# the src can ONLY be a DENORM or an UNNORM! so, don't make any big subroutine
# call here. just figure out what it is...
	mov.w		FP_SRC_EX(%a6),%d0	# get exponent
	andi.w		&0x7fff,%d0		# strip sign
	beq.b		fu_out_denorm		# it's a DENORM

	lea		FP_SRC(%a6),%a0
	bsr.l		unnorm_fix		# yes; fix it

	mov.b		%d0,STAG(%a6)

	bra.b		fu_out_cont
fu_out_denorm:
	mov.b		&DENORM,STAG(%a6)
fu_out_cont:

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	lea		FP_SRC(%a6),%a0		# pass ptr to src operand

	mov.l		(%a6),EXC_A6(%a6)	# in case a6 changes
	bsr.l		fout			# call fmove out routine

# Exceptions in order of precedence:
#	BSUN	: none
#	SNAN	: none
#	OPERR	: fmove.{b,w,l} out of large UNNORM
#	OVFL	: fmove.{s,d}
#	UNFL	: fmove.{s,d,x}
#	DZ	: none
#	INEX2	: all
#	INEX1	: none (packed doesn't travel through here)

# determine the highest priority exception(if any) set by the
# emulation routine that has also been enabled by the user.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceptions enabled
	bne.w		fu_out_ena		# some are enabled

fu_out_done:

	mov.l		EXC_A6(%a6),(%a6)	# in case a6 changed

# on extended precision opclass three instructions using pre-decrement or
# post-increment addressing mode, the address register is not updated. is the
# address register was the stack pointer used from user mode, then let's update
# it here. if it was used from supervisor mode, then we have to handle this
# as a special case.
	btst		&0x5,EXC_SR(%a6)
	bne.b		fu_out_done_s

	mov.l		EXC_A7(%a6),%a0		# restore a7
	mov.l		%a0,%usp

fu_out_done_cont:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6

	btst		&0x7,(%sp)		# is trace on?
	bne.b		fu_out_trace		# yes

	bra.l		_fpsp_done

# is the ea mode pre-decrement of the stack pointer from supervisor mode?
# ("fmov.x fpm,-(a7)") if so,
fu_out_done_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.b		fu_out_done_cont

# the extended precision result is still in fp0. but, we need to save it
# somewhere on the stack until we can copy it to its final resting place.
# here, we're counting on the top of the stack to be the old place-holders
# for fp0/fp1 which have already been restored. that way, we can write
# over those destinations with the shifted stack frame.
	fmovm.x		&0x80,FP_SRC(%a6)	# put answer on stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)

# now, copy the result to the proper place on the stack
	mov.l		LOCAL_SIZE+FP_SRC_EX(%sp),LOCAL_SIZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_SRC_HI(%sp),LOCAL_SIZE+EXC_SR+0x4(%sp)
	mov.l		LOCAL_SIZE+FP_SRC_LO(%sp),LOCAL_SIZE+EXC_SR+0x8(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp

	btst		&0x7,(%sp)
	bne.b		fu_out_trace

	bra.l		_fpsp_done

fu_out_ena:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	bne.b		fu_out_exc		# there is at least one set

# no exceptions were set.
# if a disabled overflow occurred and inexact was enabled but the result
# was exact, then a branch to _real_inex() is made.
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # was overflow set?
	beq.w		fu_out_done		# no

fu_out_ovflchk:
	btst		&inex2_bit,FPCR_ENABLE(%a6) # was inexact enabled?
	beq.w		fu_out_done		# no
	bra.w		fu_inex			# yes

#
# The fp move out that took the "Unimplemented Data Type" exception was
# being traced. Since the stack frames are similar, get the "current" PC
# from FPIAR and put it in the trace stack frame then jump to _real_trace().
#
#		  UNSUPP FRAME		   TRACE FRAME
#		*****************	*****************
#		*      EA	*	*    Current	*
#		*		*	*      PC	*
#		*****************	*****************
#		* 0x3 *  0x0dc	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	*****************
#		*      SR	*	*      SR	*
#		*****************	*****************
#
fu_out_trace:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l		_real_trace

# an exception occurred and that exception was enabled.
fu_out_exc:
	subi.l		&24,%d0			# fix offset to be 0-8

# we don't mess with the existing fsave frame. just re-insert it and
# jump to the "_real_{}()" handler...
	mov.w		(tbl_fu_out.b,%pc,%d0.w*2),%d0
	jmp		(tbl_fu_out.b,%pc,%d0.w*1)

	swbeg		&0x8
tbl_fu_out:
	short		tbl_fu_out	- tbl_fu_out	# BSUN can't happen
	short		tbl_fu_out	- tbl_fu_out	# SNAN can't happen
	short		fu_operr	- tbl_fu_out	# OPERR
	short		fu_ovfl		- tbl_fu_out	# OVFL
	short		fu_unfl		- tbl_fu_out	# UNFL
	short		tbl_fu_out	- tbl_fu_out	# DZ can't happen
	short		fu_inex		- tbl_fu_out	# INEX2
	short		tbl_fu_out	- tbl_fu_out	# INEX1 won't make it here

# for snan,operr,ovfl,unfl, src op is still in FP_SRC so just
# frestore it.
fu_snan:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d8,EXC_VOFF(%a6)	# vector offset = 0xd8
	mov.w		&0xe006,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)

	unlk		%a6


	bra.l		_real_snan

fu_operr:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d0,EXC_VOFF(%a6)	# vector offset = 0xd0
	mov.w		&0xe004,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)

	unlk		%a6


	bra.l		_real_operr

fu_ovfl:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d4,EXC_VOFF(%a6)	# vector offset = 0xd4
	mov.w		&0xe005,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6

	bra.l		_real_ovfl

# underflow can happen for extended precision. extended precision opclass
# three instruction exceptions don't update the stack pointer. so, if the
# exception occurred from user mode, then simply update a7 and exit normally.
# if the exception occurred from supervisor mode, check if
fu_unfl:
	mov.l		EXC_A6(%a6),(%a6)	# restore a6

	btst		&0x5,EXC_SR(%a6)
	bne.w		fu_unfl_s

	mov.l		EXC_A7(%a6),%a0		# restore a7 whether we need
	mov.l		%a0,%usp		# to or not...

fu_unfl_cont:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(%a6)	# vector offset = 0xcc
	mov.w		&0xe003,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6

	bra.l		_real_unfl

fu_unfl_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg # was the <ea> mode -(sp)?
	bne.b		fu_unfl_cont

# the extended precision result is still in fp0. but, we need to save it
# somewhere on the stack until we can copy it to its final resting place
# (where the exc frame is currently). make sure it's not at the top of the
# frame or it will get overwritten when the exc stack frame is shifted "down".
	fmovm.x		&0x80,FP_SRC(%a6)	# put answer on stack
	fmovm.x		&0x40,FP_DST(%a6)	# put EXOP on stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(%a6)	# vector offset = 0xcc
	mov.w		&0xe003,2+FP_DST(%a6)

	frestore	FP_DST(%a6)		# restore EXOP

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, copy the result to the proper place on the stack
	mov.l		LOCAL_SIZE+FP_SRC_EX(%sp),LOCAL_SIZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_SRC_HI(%sp),LOCAL_SIZE+EXC_SR+0x4(%sp)
	mov.l		LOCAL_SIZE+FP_SRC_LO(%sp),LOCAL_SIZE+EXC_SR+0x8(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp

	bra.l		_real_unfl

# fmove in and out enter here.
fu_inex:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30c4,EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6


	bra.l		_real_inex

#########################################################################
#########################################################################
fu_in_pack:


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-create them anyways, zero exception field
	andi.l		&0x0ff00ff,USER_FPSR(%a6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	bsr.l		get_packed		# fetch packed src operand

	lea		FP_SRC(%a6),%a0		# pass ptr to src
	bsr.l		set_tag_x		# set src optype tag

	mov.b		%d0,STAG(%a6)		# save src optype tag

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg

# bit five of the fp extension word separates the monadic and dyadic operations
# at this point
	btst		&0x5,1+EXC_CMDREG(%a6)	# is operation monadic or dyadic?
	beq.b		fu_extract_p		# monadic
	cmpi.b		1+EXC_CMDREG(%a6),&0x3a	# is operation an ftst?
	beq.b		fu_extract_p		# yes, so it's monadic, too

	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fu_op2_done_p		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO
fu_op2_done_p:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

fu_extract_p:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	bfextu		1+EXC_CMDREG(%a6){&1:&7},%d1 # extract extension

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

	mov.l		(tbl_unsupp.l,%pc,%d1.l*4),%d1 # fetch routine addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

#
# Exceptions in order of precedence:
#	BSUN	: none
#	SNAN	: all dyadic ops
#	OPERR	: fsqrt(-NORM)
#	OVFL	: all except ftst,fcmp
#	UNFL	: all except ftst,fcmp
#	DZ	: fdiv
#	INEX2	: all except ftst,fcmp
#	INEX1	: all
#

# we determine the highest priority exception(if any) set by the
# emulation routine that has also been enabled by the user.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceptions enabled
	bne.w		fu_in_ena_p		# some are enabled

fu_in_cont_p:
# fcmp and ftst do not store any result.
	mov.b		1+EXC_CMDREG(%a6),%d0	# fetch extension
	andi.b		&0x38,%d0		# extract bits 3-5
	cmpi.b		%d0,&0x38		# is instr fcmp or ftst?
	beq.b		fu_in_exit_p		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		store_fpreg		# store the result

fu_in_exit_p:

	btst		&0x5,EXC_SR(%a6)	# user or supervisor?
	bne.w		fu_in_exit_s_p		# supervisor

	mov.l		EXC_A7(%a6),%a0		# update user a7
	mov.l		%a0,%usp

fu_in_exit_cont_p:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

	btst		&0x7,(%sp)		# is trace on?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done		# exit to os

# the exception occurred in supervisor mode. check to see if the
# addressing mode was (a7)+. if so, we'll need to shift the
# stack frame "up".
fu_in_exit_s_p:
	btst		&mia7_bit,SPCOND_FLG(%a6) # was ea mode (a7)+
	beq.b		fu_in_exit_cont_p	# no

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

# shift the stack frame "up". we don't really care about the <ea> field.
	mov.l		0x4(%sp),0x10(%sp)
	mov.l		0x0(%sp),0xc(%sp)
	add.l		&0xc,%sp

	btst		&0x7,(%sp)		# is trace on?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done		# exit to os

fu_in_ena_p:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled & set
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	bne.b		fu_in_exc_p		# at least one was set

#
# No exceptions occurred that were also enabled. Now:
#
#	if (OVFL && ovfl_disabled && inexact_enabled) {
#	    branch to _real_inex() (even if the result was exact!);
#	} else {
#	    save the result in the proper fp reg (unless the op is fcmp or ftst);
#	    return;
#	}
#
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # was overflow set?
	beq.w		fu_in_cont_p		# no

fu_in_ovflchk_p:
	btst		&inex2_bit,FPCR_ENABLE(%a6) # was inexact enabled?
	beq.w		fu_in_cont_p		# no
	bra.w		fu_in_exc_ovfl_p	# do _real_inex() now

#
# An exception occurred and that exception was enabled:
#
#	shift enabled exception field into lo byte of d0;
#	if (((INEX2 || INEX1) && inex_enabled && OVFL && ovfl_disabled) ||
#	    ((INEX2 || INEX1) && inex_enabled && UNFL && unfl_disabled)) {
#		/*
#		 * this is the case where we must call _real_inex() now or else
#		 * there will be no other way to pass it the exceptional operand
#		 */
#		call _real_inex();
#	} else {
#		restore exc state (SNAN||OPERR||OVFL||UNFL||DZ||INEX) into the FPU;
#	}
#
fu_in_exc_p:
	subi.l		&24,%d0			# fix offset to be 0-8
	cmpi.b		%d0,&0x6		# is exception INEX? (6 or 7)
	blt.b		fu_in_exc_exit_p	# no

# the enabled exception was inexact
	btst		&unfl_bit,FPSR_EXCEPT(%a6) # did disabled underflow occur?
	bne.w		fu_in_exc_unfl_p	# yes
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # did disabled overflow occur?
	bne.w		fu_in_exc_ovfl_p	# yes

# here, we insert the correct fsave status value into the fsave frame for the
# corresponding exception. the operand in the fsave frame should be the original
# src operand.
# as a reminder for future predicted pain and agony, we are passing in fsave the
# "non-skewed" operand for cases of sgl and dbl src INFs,NANs, and DENORMs.
# this is INCORRECT for enabled SNAN which would give to the user the skewed SNAN!!!
fu_in_exc_exit_p:
	btst		&0x5,EXC_SR(%a6)	# user or supervisor?
	bne.w		fu_in_exc_exit_s_p	# supervisor

	mov.l		EXC_A7(%a6),%a0		# update user a7
	mov.l		%a0,%usp

fu_in_exc_exit_cont_p:
	mov.w		(tbl_except_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore src op

	unlk		%a6

	btst		&0x7,(%sp)		# is trace enabled?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done

tbl_except_p:
	short		0xe000,0xe006,0xe004,0xe005
	short		0xe003,0xe002,0xe001,0xe001

fu_in_exc_ovfl_p:
	mov.w		&0x3,%d0
	bra.w		fu_in_exc_exit_p

fu_in_exc_unfl_p:
	mov.w		&0x4,%d0
	bra.w		fu_in_exc_exit_p

fu_in_exc_exit_s_p:
	btst		&mia7_bit,SPCOND_FLG(%a6)
	beq.b		fu_in_exc_exit_cont_p

	mov.w		(tbl_except_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore src op

	unlk		%a6			# unravel stack frame

# shift stack frame "up". who cares about <ea> field.
	mov.l		0x4(%sp),0x10(%sp)
	mov.l		0x0(%sp),0xc(%sp)
	add.l		&0xc,%sp

	btst		&0x7,(%sp)		# is trace on?
	bne.b		fu_trace_p		# yes

	bra.l		_fpsp_done		# exit to os

#
# The opclass two PACKED instruction that took an "Unimplemented Data Type"
# exception was being traced. Make the "current" PC the FPIAR and put it in the
# trace stack frame then jump to _real_trace().
#
#		  UNSUPP FRAME		   TRACE FRAME
#		*****************	*****************
#		*      EA	*	*    Current	*
#		*		*	*      PC	*
#		*****************	*****************
#		* 0x2 *	0x0dc	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	*****************
#		*      SR	*	*      SR	*
#		*****************	*****************
fu_trace_p:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)

	bra.l		_real_trace

#########################################################
#########################################################
fu_out_pack:


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-create them anyways, zero exception field.
# fmove out doesn't affect ccodes.
	and.l		&0xffff00ff,USER_FPSR(%a6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		load_fpn1

# unlike other opclass 3, unimplemented data type exceptions, packed must be
# able to detect all operand types.
	lea		FP_SRC(%a6),%a0
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fu_op2_p		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO

fu_op2_p:
	mov.b		%d0,STAG(%a6)		# save src optype tag

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	lea		FP_SRC(%a6),%a0		# pass ptr to src operand

	mov.l		(%a6),EXC_A6(%a6)	# in case a6 changes
	bsr.l		fout			# call fmove out routine

# Exceptions in order of precedence:
#	BSUN	: no
#	SNAN	: yes
#	OPERR	: if ((k_factor > +17) || (dec. exp exceeds 3 digits))
#	OVFL	: no
#	UNFL	: no
#	DZ	: no
#	INEX2	: yes
#	INEX1	: no

# determine the highest priority exception(if any) set by the
# emulation routine that has also been enabled by the user.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceptions enabled
	bne.w		fu_out_ena_p		# some are enabled

fu_out_exit_p:
	mov.l		EXC_A6(%a6),(%a6)	# restore a6

	btst		&0x5,EXC_SR(%a6)	# user or supervisor?
	bne.b		fu_out_exit_s_p		# supervisor

	mov.l		EXC_A7(%a6),%a0		# update user a7
	mov.l		%a0,%usp

fu_out_exit_cont_p:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

	btst		&0x7,(%sp)		# is trace on?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done		# exit to os

# the exception occurred in supervisor mode. check to see if the
# addressing mode was -(a7). if so, we'll need to shift the
# stack frame "down".
fu_out_exit_s_p:
	btst		&mda7_bit,SPCOND_FLG(%a6) # was ea mode -(a7)
	beq.b		fu_out_exit_cont_p	# no

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)

# now, copy the result to the proper place on the stack
	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),LOCAL_SIZE+EXC_SR+0x4(%sp)
	mov.l		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+EXC_SR+0x8(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp

	btst		&0x7,(%sp)
	bne.w		fu_trace_p

	bra.l		_fpsp_done

fu_out_ena_p:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	beq.w		fu_out_exit_p

	mov.l		EXC_A6(%a6),(%a6)	# restore a6

# an exception occurred and that exception was enabled.
# the only exception possible on packed move out are INEX, OPERR, and SNAN.
fu_out_exc_p:
	cmpi.b		%d0,&0x1a
	bgt.w		fu_inex_p2
	beq.w		fu_operr_p

fu_snan_p:
	btst		&0x5,EXC_SR(%a6)
	bne.b		fu_snan_s_p

	mov.l		EXC_A7(%a6),%a0
	mov.l		%a0,%usp
	bra.w		fu_snan

fu_snan_s_p:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.w		fu_snan

# the instruction was "fmove.p fpn,-(a7)" from supervisor mode.
# the strategy is to move the exception frame "down" 12 bytes. then, we
# can store the default result where the exception frame was.
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d8,EXC_VOFF(%a6)	# vector offset = 0xd0
	mov.w		&0xe006,2+FP_SRC(%a6)	# set fsave status

	frestore	FP_SRC(%a6)		# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, we copy the default result to its proper location
	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZE+0x4(%sp)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),LOCAL_SIZE+0x8(%sp)
	mov.l		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+0xc(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp


	bra.l		_real_snan

fu_operr_p:
	btst		&0x5,EXC_SR(%a6)
	bne.w		fu_operr_p_s

	mov.l		EXC_A7(%a6),%a0
	mov.l		%a0,%usp
	bra.w		fu_operr

fu_operr_p_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.w		fu_operr

# the instruction was "fmove.p fpn,-(a7)" from supervisor mode.
# the strategy is to move the exception frame "down" 12 bytes. then, we
# can store the default result where the exception frame was.
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d0,EXC_VOFF(%a6)	# vector offset = 0xd0
	mov.w		&0xe004,2+FP_SRC(%a6)	# set fsave status

	frestore	FP_SRC(%a6)		# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, we copy the default result to its proper location
	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZE+0x4(%sp)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),LOCAL_SIZE+0x8(%sp)
	mov.l		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+0xc(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp


	bra.l		_real_operr

fu_inex_p2:
	btst		&0x5,EXC_SR(%a6)
	bne.w		fu_inex_s_p2

	mov.l		EXC_A7(%a6),%a0
	mov.l		%a0,%usp
	bra.w		fu_inex

fu_inex_s_p2:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.w		fu_inex

# the instruction was "fmove.p fpn,-(a7)" from supervisor mode.
# the strategy is to move the exception frame "down" 12 bytes. then, we
# can store the default result where the exception frame was.
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30c4,EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# set fsave status

	frestore	FP_SRC(%a6)		# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, we copy the default result to its proper location
	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZE+0x4(%sp)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),LOCAL_SIZE+0x8(%sp)
	mov.l		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+0xc(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp


	bra.l		_real_inex

#########################################################################

#
# if we're stuffing a source operand back into an fsave frame then we
# have to make sure that for single or double source operands that the
# format stuffed is as weird as the hardware usually makes it.
#
	global		funimp_skew
funimp_skew:
	bfextu		EXC_EXTWORD(%a6){&3:&3},%d0 # extract src specifier
	cmpi.b		%d0,&0x1		# was src sgl?
	beq.b		funimp_skew_sgl		# yes
	cmpi.b		%d0,&0x5		# was src dbl?
	beq.b		funimp_skew_dbl		# yes
	rts

funimp_skew_sgl:
	mov.w		FP_SRC_EX(%a6),%d0	# fetch DENORM exponent
	andi.w		&0x7fff,%d0		# strip sign
	beq.b		funimp_skew_sgl_not
	cmpi.w		%d0,&0x3f80
	bgt.b		funimp_skew_sgl_not
	neg.w		%d0			# make exponent negative
	addi.w		&0x3f81,%d0		# find amt to shift
	mov.l		FP_SRC_HI(%a6),%d1	# fetch DENORM hi(man)
	lsr.l		%d0,%d1			# shift it
	bset		&31,%d1			# set j-bit
	mov.l		%d1,FP_SRC_HI(%a6)	# insert new hi(man)
	andi.w		&0x8000,FP_SRC_EX(%a6)	# clear old exponent
	ori.w		&0x3f80,FP_SRC_EX(%a6)	# insert new "skewed" exponent
funimp_skew_sgl_not:
	rts

funimp_skew_dbl:
	mov.w		FP_SRC_EX(%a6),%d0	# fetch DENORM exponent
	andi.w		&0x7fff,%d0		# strip sign
	beq.b		funimp_skew_dbl_not
	cmpi.w		%d0,&0x3c00
	bgt.b		funimp_skew_dbl_not

	tst.b		FP_SRC_EX(%a6)		# make "internal format"
	smi.b		0x2+FP_SRC(%a6)
	mov.w		%d0,FP_SRC_EX(%a6)	# insert exponent with cleared sign
	clr.l		%d0			# clear g,r,s
	lea		FP_SRC(%a6),%a0		# pass ptr to src op
	mov.w		&0x3c01,%d1		# pass denorm threshold
	bsr.l		dnrm_lp			# denorm it
	mov.w		&0x3c00,%d0		# new exponent
	tst.b		0x2+FP_SRC(%a6)		# is sign set?
	beq.b		fss_dbl_denorm_done	# no
	bset		&15,%d0			# set sign
fss_dbl_denorm_done:
	bset		&0x7,FP_SRC_HI(%a6)	# set j-bit
	mov.w		%d0,FP_SRC_EX(%a6)	# insert new exponent
funimp_skew_dbl_not:
	rts

#########################################################################
	global		_mem_write2
_mem_write2:
	btst		&0x5,EXC_SR(%a6)
	beq.l		_dmem_write
	mov.l		0x0(%a0),FP_DST_EX(%a6)
	mov.l		0x4(%a0),FP_DST_HI(%a6)
	mov.l		0x8(%a0),FP_DST_LO(%a6)
	clr.l		%d1
	rts

#########################################################################
# XDEF ****************************************************************	#
#	_fpsp_effadd(): 060FPSP entry point for FP "Unimplemented	#
#			effective address" exception.			#
#									#
#	This handler should be the first code executed upon taking the	#
#	FP Unimplemented Effective Address exception in an operating	#
#	system.								#
#									#
# XREF ****************************************************************	#
#	_imem_read_long() - read instruction longword			#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tag_x() - determine optype of src/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result to FP regfile	#
#	unnorm_fix() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP regfile			#
#	tbl_unsupp - add of table of emulation routines for opclass 0,2	#
#	decbin() - convert packed data to FP binary data		#
#	_real_fpu_disabled() - "callout" for "FPU disabled" exception	#
#	_real_access() - "callout" for access error exception		#
#	_mem_read() - read extended immediate operand from memory	#
#	_fpsp_done() - "callout" for exit; work all done		#
#	_real_trace() - "callout" for Trace enabled exception		#
#	fmovm_dynamic() - emulate dynamic fmovm instruction		#
#	fmovm_ctrl() - emulate fmovm control instruction		#
#									#
# INPUT ***************************************************************	#
#	- The system stack contains the "Unimplemented <ea>" stk frame	#
#									#
# OUTPUT **************************************************************	#
#	If access error:						#
#	- The system stack is changed to an access error stack frame	#
#	If FPU disabled:						#
#	- The system stack is changed to an FPU disabled stack frame	#
#	If Trace exception enabled:					#
#	- The system stack is changed to a Trace exception stack frame	#
#	Else: (normal case)						#
#	- None (correct result has been stored as appropriate)		#
#									#
# ALGORITHM ***********************************************************	#
#	This exception handles 3 types of operations:			#
# (1) FP Instructions using extended precision or packed immediate	#
#     addressing mode.							#
# (2) The "fmovm.x" instruction w/ dynamic register specification.	#
# (3) The "fmovm.l" instruction w/ 2 or 3 control registers.		#
#									#
#	For immediate data operations, the data is read in w/ a		#
# _mem_read() "callout", converted to FP binary (if packed), and used	#
# as the source operand to the instruction specified by the instruction	#
# word. If no FP exception should be reported ads a result of the	#
# emulation, then the result is stored to the destination register and	#
# the handler exits through _fpsp_done(). If an enabled exc has been	#
# signalled as a result of emulation, then an fsave state frame		#
# corresponding to the FP exception type must be entered into the 060	#
# FPU before exiting. In either the enabled or disabled cases, we	#
# must also check if a Trace exception is pending, in which case, we	#
# must create a Trace exception stack frame from the current exception	#
# stack frame. If no Trace is pending, we simply exit through		#
# _fpsp_done().								#
#	For "fmovm.x", call the routine fmovm_dynamic() which will	#
# decode and emulate the instruction. No FP exceptions can be pending	#
# as a result of this operation emulation. A Trace exception can be	#
# pending, though, which means the current stack frame must be changed	#
# to a Trace stack frame and an exit made through _real_trace().	#
# For the case of "fmovm.x Dn,-(a7)", where the offending instruction	#
# was executed from supervisor mode, this handler must store the FP	#
# register file values to the system stack by itself since		#
# fmovm_dynamic() can't handle this. A normal exit is made through	#
# fpsp_done().								#
#	For "fmovm.l", fmovm_ctrl() is used to emulate the instruction.	#
# Again, a Trace exception may be pending and an exit made through	#
# _real_trace(). Else, a normal exit is made through _fpsp_done().	#
#									#
#	Before any of the above is attempted, it must be checked to	#
# see if the FPU is disabled. Since the "Unimp <ea>" exception is taken	#
# before the "FPU disabled" exception, but the "FPU disabled" exception	#
# has higher priority, we check the disabled bit in the PCR. If set,	#
# then we must create an 8 word "FPU disabled" exception stack frame	#
# from the current 4 word exception stack frame. This includes		#
# reproducing the effective address of the instruction to put on the	#
# new stack frame.							#
#									#
#	In the process of all emulation work, if a _mem_read()		#
# "callout" returns a failing result indicating an access error, then	#
# we must create an access error stack frame from the current stack	#
# frame. This information includes a faulting address and a fault-	#
# status-longword. These are created within this handler.		#
#									#
#########################################################################

	global		_fpsp_effadd
_fpsp_effadd:

# This exception type takes priority over the "Line F Emulator"
# exception. Therefore, the FPU could be disabled when entering here.
# So, we must check to see if it's disabled and handle that case separately.
	mov.l		%d0,-(%sp)		# save d0
	movc		%pcr,%d0		# load proc cr
	btst		&0x1,%d0		# is FPU disabled?
	bne.w		iea_disabled		# yes
	mov.l		(%sp)+,%d0		# restore d0

	link		%a6,&-LOCAL_SIZE	# init stack frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

# PC of instruction that took the exception is the PC in the frame
	mov.l		EXC_PC(%a6),EXC_EXTWPTR(%a6)

	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		# fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)	# store OPWORD and EXTWORD

#########################################################################

	tst.w		%d0			# is operation fmovem?
	bmi.w		iea_fmovm		# yes

#
# here, we will have:
#	fabs	fdabs	fsabs		facos		fmod
#	fadd	fdadd	fsadd		fasin		frem
#	fcmp				fatan		fscale
#	fdiv	fddiv	fsdiv		fatanh		fsin
#	fint				fcos		fsincos
#	fintrz				fcosh		fsinh
#	fmove	fdmove	fsmove		fetox		ftan
#	fmul	fdmul	fsmul		fetoxm1		ftanh
#	fneg	fdneg	fsneg		fgetexp		ftentox
#	fsgldiv				fgetman		ftwotox
#	fsglmul				flog10
#	fsqrt				flog2
#	fsub	fdsub	fssub		flogn
#	ftst				flognp1
# which can all use f<op>.{x,p}
# so, now it's immediate data extended precision AND PACKED FORMAT!
#
iea_op:
	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	btst		&0xa,%d0		# is src fmt x or p?
	bne.b		iea_op_pack		# packed


	mov.l		EXC_EXTWPTR(%a6),%a0	# pass: ptr to #<data>
	lea		FP_SRC(%a6),%a1		# pass: ptr to super addr
	mov.l		&0xc,%d0		# pass: 12 bytes
	bsr.l		_imem_read		# read extended immediate

	tst.l		%d1			# did ifetch fail?
	bne.w		iea_iacc		# yes

	bra.b		iea_op_setsrc

iea_op_pack:

	mov.l		EXC_EXTWPTR(%a6),%a0	# pass: ptr to #<data>
	lea		FP_SRC(%a6),%a1		# pass: ptr to super dst
	mov.l		&0xc,%d0		# pass: 12 bytes
	bsr.l		_imem_read		# read packed operand

	tst.l		%d1			# did ifetch fail?
	bne.w		iea_iacc		# yes

# The packed operand is an INF or a NAN if the exponent field is all ones.
	bfextu		FP_SRC(%a6){&1:&15},%d0	# get exp
	cmpi.w		%d0,&0x7fff		# INF or NAN?
	beq.b		iea_op_setsrc		# operand is an INF or NAN

# The packed operand is a zero if the mantissa is all zero, else it's
# a normal packed op.
	mov.b		3+FP_SRC(%a6),%d0	# get byte 4
	andi.b		&0x0f,%d0		# clear all but last nybble
	bne.b		iea_op_gp_not_spec	# not a zero
	tst.l		FP_SRC_HI(%a6)		# is lw 2 zero?
	bne.b		iea_op_gp_not_spec	# not a zero
	tst.l		FP_SRC_LO(%a6)		# is lw 3 zero?
	beq.b		iea_op_setsrc		# operand is a ZERO
iea_op_gp_not_spec:
	lea		FP_SRC(%a6),%a0		# pass: ptr to packed op
	bsr.l		decbin			# convert to extended
	fmovm.x		&0x80,FP_SRC(%a6)	# make this the srcop

iea_op_setsrc:
	addi.l		&0xc,EXC_EXTWPTR(%a6)	# update extension word pointer

# FP_SRC now holds the src operand.
	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		set_tag_x		# tag the operand type
	mov.b		%d0,STAG(%a6)		# could be ANYTHING!!!
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		iea_op_getdst		# no
	bsr.l		unnorm_fix		# yes; convert to NORM/DENORM/ZERO
	mov.b		%d0,STAG(%a6)		# set new optype tag
iea_op_getdst:
	clr.b		STORE_FLG(%a6)		# clear "store result" boolean

	btst		&0x5,1+EXC_CMDREG(%a6)	# is operation monadic or dyadic?
	beq.b		iea_op_extract		# monadic
	btst		&0x4,1+EXC_CMDREG(%a6)	# is operation fsincos,ftst,fcmp?
	bne.b		iea_op_spec		# yes

iea_op_loaddst:
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # fetch dst regno
	bsr.l		load_fpn2		# load dst operand

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	mov.b		%d0,DTAG(%a6)		# could be ANYTHING!!!
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		iea_op_extract		# no
	bsr.l		unnorm_fix		# yes; convert to NORM/DENORM/ZERO
	mov.b		%d0,DTAG(%a6)		# set new optype tag
	bra.b		iea_op_extract

# the operation is fsincos, ftst, or fcmp. only fcmp is dyadic
iea_op_spec:
	btst		&0x3,1+EXC_CMDREG(%a6)	# is operation fsincos?
	beq.b		iea_op_extract		# yes
# now, we're left with ftst and fcmp. so, first let's tag them so that they don't
# store a result. then, only fcmp will branch back and pick up a dst operand.
	st		STORE_FLG(%a6)		# don't store a final result
	btst		&0x1,1+EXC_CMDREG(%a6)	# is operation fcmp?
	beq.b		iea_op_loaddst		# yes

iea_op_extract:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass: rnd mode,prec

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extension

	fmov.l		&0x0,%fpcr
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%d1 # fetch routine addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

#
# Exceptions in order of precedence:
#	BSUN	: none
#	SNAN	: all operations
#	OPERR	: all reg-reg or mem-reg operations that can normally operr
#	OVFL	: same as OPERR
#	UNFL	: same as OPERR
#	DZ	: same as OPERR
#	INEX2	: same as OPERR
#	INEX1	: all packed immediate operations
#

# we determine the highest priority exception(if any) set by the
# emulation routine that has also been enabled by the user.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceptions enabled
	bne.b		iea_op_ena		# some are enabled

# now, we save the result, unless, of course, the operation was ftst or fcmp.
# these don't save results.
iea_op_save:
	tst.b		STORE_FLG(%a6)		# does this op store a result?
	bne.b		iea_op_exit1		# exit with no frestore

iea_op_store:
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # fetch dst regno
	bsr.l		store_fpreg		# store the result

iea_op_exit1:
	mov.l		EXC_PC(%a6),USER_FPIAR(%a6) # set FPIAR to "Current PC"
	mov.l		EXC_EXTWPTR(%a6),EXC_PC(%a6) # set "Next PC" in exc frame

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel the frame

	btst		&0x7,(%sp)		# is trace on?
	bne.w		iea_op_trace		# yes

	bra.l		_fpsp_done		# exit to os

iea_op_ena:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enable and set
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	bne.b		iea_op_exc		# at least one was set

# no exception occurred. now, did a disabled, exact overflow occur with inexact
# enabled? if so, then we have to stuff an overflow frame into the FPU.
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # did overflow occur?
	beq.b		iea_op_save

iea_op_ovfl:
	btst		&inex2_bit,FPCR_ENABLE(%a6) # is inexact enabled?
	beq.b		iea_op_store		# no
	bra.b		iea_op_exc_ovfl		# yes

# an enabled exception occurred. we have to insert the exception type back into
# the machine.
iea_op_exc:
	subi.l		&24,%d0			# fix offset to be 0-8
	cmpi.b		%d0,&0x6		# is exception INEX?
	bne.b		iea_op_exc_force	# no

# the enabled exception was inexact. so, if it occurs with an overflow
# or underflow that was disabled, then we have to force an overflow or
# underflow frame.
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # did overflow occur?
	bne.b		iea_op_exc_ovfl		# yes
	btst		&unfl_bit,FPSR_EXCEPT(%a6) # did underflow occur?
	bne.b		iea_op_exc_unfl		# yes

iea_op_exc_force:
	mov.w		(tbl_iea_except.b,%pc,%d0.w*2),2+FP_SRC(%a6)
	bra.b		iea_op_exit2		# exit with frestore

tbl_iea_except:
	short		0xe002, 0xe006, 0xe004, 0xe005
	short		0xe003, 0xe002, 0xe001, 0xe001

iea_op_exc_ovfl:
	mov.w		&0xe005,2+FP_SRC(%a6)
	bra.b		iea_op_exit2

iea_op_exc_unfl:
	mov.w		&0xe003,2+FP_SRC(%a6)

iea_op_exit2:
	mov.l		EXC_PC(%a6),USER_FPIAR(%a6) # set FPIAR to "Current PC"
	mov.l		EXC_EXTWPTR(%a6),EXC_PC(%a6) # set "Next PC" in exc frame

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore exceptional state

	unlk		%a6			# unravel the frame

	btst		&0x7,(%sp)		# is trace on?
	bne.b		iea_op_trace		# yes

	bra.l		_fpsp_done		# exit to os

#
# The opclass two instruction that took an "Unimplemented Effective Address"
# exception was being traced. Make the "current" PC the FPIAR and put it in
# the trace stack frame then jump to _real_trace().
#
#		 UNIMP EA FRAME		   TRACE FRAME
#		*****************	*****************
#		* 0x0 *  0x0f0	*	*    Current	*
#		*****************	*      PC	*
#		*    Current	*	*****************
#		*      PC	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*      SR	*	*     Next	*
#		*****************	*      PC	*
#					*****************
#					*      SR	*
#					*****************
iea_op_trace:
	mov.l		(%sp),-(%sp)		# shift stack frame "down"
	mov.w		0x8(%sp),0x4(%sp)
	mov.w		&0x2024,0x6(%sp)	# stk fmt = 0x2; voff = 0x024
	fmov.l		%fpiar,0x8(%sp)		# "Current PC" is in FPIAR

	bra.l		_real_trace

#########################################################################
iea_fmovm:
	btst		&14,%d0			# ctrl or data reg
	beq.w		iea_fmovm_ctrl

iea_fmovm_data:

	btst		&0x5,EXC_SR(%a6)	# user or supervisor mode
	bne.b		iea_fmovm_data_s

iea_fmovm_data_u:
	mov.l		%usp,%a0
	mov.l		%a0,EXC_A7(%a6)		# store current a7
	bsr.l		fmovm_dynamic		# do dynamic fmovm
	mov.l		EXC_A7(%a6),%a0		# load possibly new a7
	mov.l		%a0,%usp		# update usp
	bra.w		iea_fmovm_exit

iea_fmovm_data_s:
	clr.b		SPCOND_FLG(%a6)
	lea		0x2+EXC_VOFF(%a6),%a0
	mov.l		%a0,EXC_A7(%a6)
	bsr.l		fmovm_dynamic		# do dynamic fmovm

	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	beq.w		iea_fmovm_data_predec
	cmpi.b		SPCOND_FLG(%a6),&mia7_flg
	bne.w		iea_fmovm_exit

# right now, d0 = the size.
# the data has been fetched from the supervisor stack, but we have not
# incremented the stack pointer by the appropriate number of bytes.
# do it here.
iea_fmovm_data_postinc:
	btst		&0x7,EXC_SR(%a6)
	bne.b		iea_fmovm_data_pi_trace

	mov.w		EXC_SR(%a6),(EXC_SR,%a6,%d0)
	mov.l		EXC_EXTWPTR(%a6),(EXC_PC,%a6,%d0)
	mov.w		&0x00f0,(EXC_VOFF,%a6,%d0)

	lea		(EXC_SR,%a6,%d0),%a0
	mov.l		%a0,EXC_SR(%a6)

	fmovm.x		EXC_FP0(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
	mov.l		(%sp)+,%sp
	bra.l		_fpsp_done

iea_fmovm_data_pi_trace:
	mov.w		EXC_SR(%a6),(EXC_SR-0x4,%a6,%d0)
	mov.l		EXC_EXTWPTR(%a6),(EXC_PC-0x4,%a6,%d0)
	mov.w		&0x2024,(EXC_VOFF-0x4,%a6,%d0)
	mov.l		EXC_PC(%a6),(EXC_VOFF+0x2-0x4,%a6,%d0)

	lea		(EXC_SR-0x4,%a6,%d0),%a0
	mov.l		%a0,EXC_SR(%a6)

	fmovm.x		EXC_FP0(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
	mov.l		(%sp)+,%sp
	bra.l		_real_trace

# right now, d1 = size and d0 = the strg.
iea_fmovm_data_predec:
	mov.b		%d1,EXC_VOFF(%a6)	# store strg
	mov.b		%d0,0x1+EXC_VOFF(%a6)	# store size

	fmovm.x		EXC_FP0(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.l		(%a6),-(%sp)		# make a copy of a6
	mov.l		%d0,-(%sp)		# save d0
	mov.l		%d1,-(%sp)		# save d1
	mov.l		EXC_EXTWPTR(%a6),-(%sp)	# make a copy of Next PC

	clr.l		%d0
	mov.b		0x1+EXC_VOFF(%a6),%d0	# fetch size
	neg.l		%d0			# get negative of size

	btst		&0x7,EXC_SR(%a6)	# is trace enabled?
	beq.b		iea_fmovm_data_p2

	mov.w		EXC_SR(%a6),(EXC_SR-0x4,%a6,%d0)
	mov.l		EXC_PC(%a6),(EXC_VOFF-0x2,%a6,%d0)
	mov.l		(%sp)+,(EXC_PC-0x4,%a6,%d0)
	mov.w		&0x2024,(EXC_VOFF-0x4,%a6,%d0)

	pea		(%a6,%d0)		# create final sp
	bra.b		iea_fmovm_data_p3

iea_fmovm_data_p2:
	mov.w		EXC_SR(%a6),(EXC_SR,%a6,%d0)
	mov.l		(%sp)+,(EXC_PC,%a6,%d0)
	mov.w		&0x00f0,(EXC_VOFF,%a6,%d0)

	pea		(0x4,%a6,%d0)		# create final sp

iea_fmovm_data_p3:
	clr.l		%d1
	mov.b		EXC_VOFF(%a6),%d1	# fetch strg

	tst.b		%d1
	bpl.b		fm_1
	fmovm.x		&0x80,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_1:
	lsl.b		&0x1,%d1
	bpl.b		fm_2
	fmovm.x		&0x40,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_2:
	lsl.b		&0x1,%d1
	bpl.b		fm_3
	fmovm.x		&0x20,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_3:
	lsl.b		&0x1,%d1
	bpl.b		fm_4
	fmovm.x		&0x10,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_4:
	lsl.b		&0x1,%d1
	bpl.b		fm_5
	fmovm.x		&0x08,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_5:
	lsl.b		&0x1,%d1
	bpl.b		fm_6
	fmovm.x		&0x04,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_6:
	lsl.b		&0x1,%d1
	bpl.b		fm_7
	fmovm.x		&0x02,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_7:
	lsl.b		&0x1,%d1
	bpl.b		fm_end
	fmovm.x		&0x01,(0x4+0x8,%a6,%d0)
fm_end:
	mov.l		0x4(%sp),%d1
	mov.l		0x8(%sp),%d0
	mov.l		0xc(%sp),%a6
	mov.l		(%sp)+,%sp

	btst		&0x7,(%sp)		# is trace enabled?
	beq.l		_fpsp_done
	bra.l		_real_trace

#########################################################################
iea_fmovm_ctrl:

	bsr.l		fmovm_ctrl		# load ctrl regs

iea_fmovm_exit:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	btst		&0x7,EXC_SR(%a6)	# is trace on?
	bne.b		iea_fmovm_trace		# yes

	mov.l		EXC_EXTWPTR(%a6),EXC_PC(%a6) # set Next PC

	unlk		%a6			# unravel the frame

	bra.l		_fpsp_done		# exit to os

#
# The control reg instruction that took an "Unimplemented Effective Address"
# exception was being traced. The "Current PC" for the trace frame is the
# PC stacked for Unimp EA. The "Next PC" is in EXC_EXTWPTR.
# After fixing the stack frame, jump to _real_trace().
#
#		 UNIMP EA FRAME		   TRACE FRAME
#		*****************	*****************
#		* 0x0 *  0x0f0	*	*    Current	*
#		*****************	*      PC	*
#		*    Current	*	*****************
#		*      PC	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*      SR	*	*     Next	*
#		*****************	*      PC	*
#					*****************
#					*      SR	*
#					*****************
# this ain't a pretty solution, but it works:
# -restore a6 (not with unlk)
# -shift stack frame down over where old a6 used to be
# -add LOCAL_SIZE to stack pointer
iea_fmovm_trace:
	mov.l		(%a6),%a6		# restore frame pointer
	mov.w		EXC_SR+LOCAL_SIZE(%sp),0x0+LOCAL_SIZE(%sp)
	mov.l		EXC_PC+LOCAL_SIZE(%sp),0x8+LOCAL_SIZE(%sp)
	mov.l		EXC_EXTWPTR+LOCAL_SIZE(%sp),0x2+LOCAL_SIZE(%sp)
	mov.w		&0x2024,0x6+LOCAL_SIZE(%sp) # stk fmt = 0x2; voff = 0x024
	add.l		&LOCAL_SIZE,%sp		# clear stack frame

	bra.l		_real_trace

#########################################################################
# The FPU is disabled and so we should really have taken the "Line
# F Emulator" exception. So, here we create an 8-word stack frame
# from our 4-word stack frame. This means we must calculate the length
# the faulting instruction to get the "next PC". This is trivial for
# immediate operands but requires some extra work for fmovm dynamic
# which can use most addressing modes.
iea_disabled:
	mov.l		(%sp)+,%d0		# restore d0

	link		%a6,&-LOCAL_SIZE	# init stack frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1

# PC of instruction that took the exception is the PC in the frame
	mov.l		EXC_PC(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		# fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)	# store OPWORD and EXTWORD

	tst.w		%d0			# is instr fmovm?
	bmi.b		iea_dis_fmovm		# yes
# instruction is using an extended precision immediate operand. therefore,
# the total instruction length is 16 bytes.
iea_dis_immed:
	mov.l		&0x10,%d0		# 16 bytes of instruction
	bra.b		iea_dis_cont
iea_dis_fmovm:
	btst		&0xe,%d0		# is instr fmovm ctrl
	bne.b		iea_dis_fmovm_data	# no
# the instruction is a fmovm.l with 2 or 3 registers.
	bfextu		%d0{&19:&3},%d1
	mov.l		&0xc,%d0
	cmpi.b		%d1,&0x7		# move all regs?
	bne.b		iea_dis_cont
	addq.l		&0x4,%d0
	bra.b		iea_dis_cont
# the instruction is an fmovm.x dynamic which can use many addressing
# modes and thus can have several different total instruction lengths.
# call fmovm_calc_ea which will go through the ea calc process and,
# as a by-product, will tell us how long the instruction is.
iea_dis_fmovm_data:
	clr.l		%d0
	bsr.l		fmovm_calc_ea
	mov.l		EXC_EXTWPTR(%a6),%d0
	sub.l		EXC_PC(%a6),%d0
iea_dis_cont:
	mov.w		%d0,EXC_VOFF(%a6)	# store stack shift value

	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6

# here, we actually create the 8-word frame from the 4-word frame,
# with the "next PC" as additional info.
# the <ea> field is let as undefined.
	subq.l		&0x8,%sp		# make room for new stack
	mov.l		%d0,-(%sp)		# save d0
	mov.w		0xc(%sp),0x4(%sp)	# move SR
	mov.l		0xe(%sp),0x6(%sp)	# move Current PC
	clr.l		%d0
	mov.w		0x12(%sp),%d0
	mov.l		0x6(%sp),0x10(%sp)	# move Current PC
	add.l		%d0,0x6(%sp)		# make Next PC
	mov.w		&0x402c,0xa(%sp)	# insert offset,frame format
	mov.l		(%sp)+,%d0		# restore d0

	bra.l		_real_fpu_disabled

##########

iea_iacc:
	movc		%pcr,%d0
	btst		&0x1,%d0
	bne.b		iea_iacc_cont
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1 on stack
iea_iacc_cont:
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6

	subq.w		&0x8,%sp		# make stack frame bigger
	mov.l		0x8(%sp),(%sp)		# store SR,hi(PC)
	mov.w		0xc(%sp),0x4(%sp)	# store lo(PC)
	mov.w		&0x4008,0x6(%sp)	# store voff
	mov.l		0x2(%sp),0x8(%sp)	# store ea
	mov.l		&0x09428001,0xc(%sp)	# store fslw

iea_acc_done:
	btst		&0x5,(%sp)		# user or supervisor mode?
	beq.b		iea_acc_done2		# user
	bset		&0x2,0xd(%sp)		# set supervisor TM bit

iea_acc_done2:
	bra.l		_real_access

iea_dacc:
	lea		-LOCAL_SIZE(%a6),%sp

	movc		%pcr,%d1
	btst		&0x1,%d1
	bne.b		iea_dacc_cont
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1 on stack
	fmovm.l		LOCAL_SIZE+USER_FPCR(%sp),%fpcr,%fpsr,%fpiar # restore ctrl regs
iea_dacc_cont:
	mov.l		(%a6),%a6

	mov.l		0x4+LOCAL_SIZE(%sp),-0x8+0x4+LOCAL_SIZE(%sp)
	mov.w		0x8+LOCAL_SIZE(%sp),-0x8+0x8+LOCAL_SIZE(%sp)
	mov.w		&0x4008,-0x8+0xa+LOCAL_SIZE(%sp)
	mov.l		%a0,-0x8+0xc+LOCAL_SIZE(%sp)
	mov.w		%d0,-0x8+0x10+LOCAL_SIZE(%sp)
	mov.w		&0x0001,-0x8+0x12+LOCAL_SIZE(%sp)

	movm.l		LOCAL_SIZE+EXC_DREGS(%sp),&0x0303 # restore d0-d1/a0-a1
	add.w		&LOCAL_SIZE-0x4,%sp

	bra.b		iea_acc_done

#########################################################################
# XDEF ****************************************************************	#
#	_fpsp_operr(): 060FPSP entry point for FP Operr exception.	#
#									#
#	This handler should be the first code executed upon taking the	#
#	FP Operand Error exception in an operating system.		#
#									#
# XREF ****************************************************************	#
#	_imem_read_long() - read instruction longword			#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	_real_operr() - "callout" to operating system operr handler	#
#	_dmem_write_{byte,word,long}() - store data to mem (opclass 3)	#
#	store_dreg_{b,w,l}() - store data to data regfile (opclass 3)	#
#	facc_out_{b,w,l}() - store to memory took access error (opcl 3)	#
#									#
# INPUT ***************************************************************	#
#	- The system stack contains the FP Operr exception frame	#
#	- The fsave frame contains the source operand			#
#									#
# OUTPUT **************************************************************	#
#	No access error:						#
#	- The system stack is unchanged					#
#	- The fsave frame contains the adjusted src op for opclass 0,2	#
#									#
# ALGORITHM ***********************************************************	#
#	In a system where the FP Operr exception is enabled, the goal	#
# is to get to the handler specified at _real_operr(). But, on the 060,	#
# for opclass zero and two instruction taking this exception, the	#
# input operand in the fsave frame may be incorrect for some cases	#
# and needs to be corrected. This handler calls fix_skewed_ops() to	#
# do just this and then exits through _real_operr().			#
#	For opclass 3 instructions, the 060 doesn't store the default	#
# operr result out to memory or data register file as it should.	#
# This code must emulate the move out before finally exiting through	#
# _real_inex(). The move out, if to memory, is performed using		#
# _mem_write() "callout" routines that may return a failing result.	#
# In this special case, the handler must exit through facc_out()	#
# which creates an access error stack frame from the current operr	#
# stack frame.								#
#									#
#########################################################################

	global		_fpsp_operr
_fpsp_operr:

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(%a6)		# grab the "busy" frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

# the FPIAR holds the "current PC" of the faulting instruction
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)

	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		# fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)

##############################################################################

	btst		&13,%d0			# is instr an fmove out?
	bne.b		foperr_out		# fmove out


# here, we simply see if the operand in the fsave frame needs to be "unskewed".
# this would be the case for opclass two operations with a source infinity or
# denorm operand in the sgl or dbl format. NANs also become skewed, but can't
# cause an operr so we don't need to check for them here.
	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		fix_skewed_ops		# fix src op

foperr_exit:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)

	unlk		%a6
	bra.l		_real_operr

########################################################################

#
# the hardware does not save the default res~~~~to memory on enabled
# operand error exceptions. w~~~~ this here before passing control to
#~~~~~userROLA MICROPROCEhandler.
#
# byte, word, MICRlong destina& ME formatROLA M& MEM can000 HMicroroughGY GRORY Tsimply need~~~~test~~~~~sign of1994 MrcTOROLA MICRMICR~~~~~~~~~appropriate minimum or maxrovidinteger valueMicrCHNOe effective address as poS IS© 19by1994 Mtackedt warranty.
To theftwarealthtwarepd by aopclass Sofeetober 10, 1994

takETHER EICROPROC
#ESSOR & MEM,thouy won't000 H Software Pac sinc~~~~~y ~~~~caught
# firstt permitunsuppor extdata0 -- OctSSOR & ME68060 Sof that68060 So
# sendNTABem directly wit_real_OLA r() if necessaryftwafSIONS_out:

	mov.w		FP_SRC_EX(%a6),%d1	# fetch exponent
	andien m&0x7fffthe 
	cmped by%d1, applic
	bne.b		ccompanyin_not_qnanMicroprOLA MICRis either an infinityded a QNAN.
	tst.l materialLO

To LL MOTOROLA BE LIABLR ANYritteAMAGES FORHI

To the rmittedlby applicTION OF BUbeqTOROLA BE LIABLE FOR ANYS PROFITS,
BUSI:NESS INTERRUPTION, LOSS L_SCR1OSS OF BraTOROLA BE LIABLjmp
G OF THE USEE FOR ANINABILITY NFORMATION, OR OTN, Db materials.

To 
	bplTOROLA BE LIABLE FOR AN2
	addqSS INFO1 OF Bd a copyright licensINABILITY O EVWARE.
Motoroand distribujmp:
	bfextuo lo0{&19:&3},%d0		IMPLtract dst0 -- OctfieldNESS Ib		1+EXC_OPWORD

To the maxany modi<ea> mode,regwritten m(tbERSIONS.b,%pction.w*2),%a0
	jmply identified as suca0)

identifie:
	shortROLA BE LIABLl - identifie #tion Rge
PAS IS" b or otherpatents
or  ny patents
or tsgl prec should MERhappenrola, Inc.
~~~~~~~~~~~~~~~~~~~~~~~~~ext~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ccompanexit~~~~~~~~~~~~~~~~S ALL WOF MERenterGY GR or otherwise under wny patents
or tarks of Motorola, Inc.
~~~~~~~~~~~~~~~~~~~~~~~~~db~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~LA BE LIABLbny patents
or t Pac of Motorola, Inc.
~~~~~~~~~~~~~~~~~~~~~~~~~the entry points into thand distribubINABILIb		WARE.
Motortion in load posianty.~~~~~~~~~~~~~~ law,
Ib NO EVENT  in ised version aagainsreg?
	blOTOROLA BE LIABLb_~~~~_dn	# yesNESS INTEersiEA

To than in 00 H:ed verofb routine actuallbsrSS I_dmem_write_ Pac	# akes ~~~~~~~~~~~~~~~~~~~
ION, DAMAhe m in didifieM680fails a neSS Ifacc stub  in is
#ola asw0FPSP packagekage itself iroutine.:rmitted by ap0007 OR OTHy but
set	__dreg_snan,set	_o~~~~~~~~~~regfile4
set	_off_operr,	0x0and distribuwINABILIw# to call a given routine. The stub routine actually performs the
# callout. The FPSP code does a "bsr" to the stubwroutine. This
# extra layer of hierarchy adds a slight performance penalty but
# it makes tge
PPSP code easier to read and more mainatinable.
#

set	_off_bsun,	0x00
set	_off_wnan,	0x04
set	_off_operr,	0x08
set	_off_et	_off_dc
set	_off_unfl,	0x10
set	_off_dz,	0x14
seFPSP_f_inex,	0x18
set	_off_fline,	0x1c
set	_off_fpu_dis,	0x20lINABILITY to call a given routine. The stub routine actually performs the
# callout. The FPSP code does a "bsr" to the stublroutine. This
# extra layer of hierarchy adds a slight performance penalty but
# it makes tion PSP code easier to read and more mainatinable.
#

set	_off_bsun,	0x00
set	_off_lnan,	0x04
set	_off_operr,	0x08
set	_off_000
	bra.c
set	_off_unfl,	0x10
set	_off_dz,	0x14
se0

	sf_inex,	0x18
set	_off_fline,	0x1c
set	_off_fp#0FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0
# XDEF *	0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.	#
#	_fpsp_snan(): 060FPSPintsryximum 0 -- FP SNANingement w.		%d0.l		(_06	%d0TOLOGY060 So~~~~~~~ be easiURPOSEc FPSexecu extupoR IMPi-Pethe0FPSPoff_ignalli-Peovfl,%pc),%d0 inCLUDober 10ng system.60FPSPl		(_060FPS XR.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,iit mreadshort() - 		&0 instruc.l		%ion 8
set60FPSPfix_skewed_opsglobaadjustc.  GES WHATSOn f~~~~~frame60FPSPED VER)
	mov - "callout" witsp)
	mov.l		(_06f_ovflE-0x80,		%d0, it makes t{ Packge
Prion }	(_06set	_ogains~~~~~~ (ARRANTIE3)0FPSPdz,	0x14
se{b,w,lbsun
_real_bsun:
	mocode doeff_f		%d0,-(%sp)
	movset	_off_SP_TAB,d,xbsun
_real_b	mov.l	took accROPROCE	%d0,sp)
	mov_calc_ea_fout	(_06fix AnHEREd veris -()ded ()+; also geied vePSP_TABLE-0x80+_oINPUT		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.0FPSP- T94 M)
	movted berforainNCLUD_off_ovfl,%pc),%d0_inex,%pc),%x4

	g+_off_inex,_snan
_real_snsourcAGES WHAT
	mov.lABLE-0x80+_oOUT,%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	r0FPSPNo	&0xthe OPROC:	(_060FPSPx4

	global		_real_is unchangeea.l	mov.l		(_060FPSP_TABLE-0x80+_off_sna		(_06ed0FPSP_Tx80+_ARRANTIE0,2l		(_060FPSP_TABLALGORITHM		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	0FPSPIn aglobal		wY GROl_snan:
	mov.l		%d0,-(OEVE~~~
MOANTIE goal80+_oisp),%_TABwithoutE-0x80,%ppecified atIED VER)
	mov. But,~~~~pc,%060,80+_o	0x4(%sp),%dzero resetworeal_inex
_rs4

	globalOEVESOR & MEANTIE	mov.lnpuctober E-0x80ov.l	FPSP_TABLE-may)
	mincorANY ed:
	soLE-0ases80+_o		(_ht �		0x4bE-0xov.l	ed. _TABLE-0x80,%FPSPs	movd0,-(%sp)
	movto80+_odo (_060-0x80%sp)
hen kageNTABILITY real_fpu_disa%pc),%dF0x4(%sp),%d3ov.l		(_060FP,%pc,%060~~~~~ MERx4(%sp)~~~~~~~~~~80+_o_ovfl~~~~~~~ou~~~~~~~~~~~~~rpc),%d0
	iss inpea.las it~~~~~~~0FPSP__real_%sp),m_060emulRE i	glomovetraceUP
M680fi
_reytrap,globalftwarx80+_real_fpu_disab

	g(_060FPS,HERE~~~~~~~~~,.l		per -- td		Hi-P0x80+__it makes ()60FPSP_TABLroutineNTABat,%pc,retur+_offf_bov.l~~~~~~
	pea I60FPisx4

	gal,%d0
,%pc,%E-0x80,%%pc),%age
Softwareset	_off()
	mo which create0x80
	mov.l		%d0,	_real_TABLE-froma.l		currenttrace######

	global	FPSPz,%pc),%dLE-0_readaseht pa_tratended~~~~~is0,-(x80,%pc,%d0)
	mov.l		isable_reaout warranty.
To theov.le FPSwa)
	mov.l		(_ANTIE60FPSP
To the#####p)
	mov.l%pc),_TABupdad0)
bymov.lov.lerr
_real_operr. I0,%pc,LE-0x80+
	rtd		a7)	_imemsupervised oionsobal		_dme.l		%d0,-(%sp)
ad
_imemlypc),%real_fplobal		_real_%pc),		_rarefu,%pc(_06d "downBLE-0mMPLIroomsabled:
	AMAGES WHATSbe%sp),%veE-0x,%pc),%dsp)
	mov.l	FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)

	globaut
#-(%sp)
	m
imem_read_g wrlinkIN NOa6,&-LOCAL_SIZEcallnPSP_

	global	

	_TABL materia

To  in grabSP_TA"busy"0+_off_irmovmSS INFO0303,ersiDREGSea.l		#erved.d0-d1/a0-a1
	f0)
	mov.%fpcr,%fpsem_reiar,USER_FPCR

To ~~~~~~~~ctrlm_resm_read_lxd supc0%sp),FPd0
	rtd		&0x4

	gfp0-fp1x4

E-0x8
MicroprFPIAR holINCLUD "d
_imem_PC"rola Inc~~~~~ov.leal_inex
_rNESS INTEmov.l		IAd0,-(%%sp),EXTWPTd0,-(% writtea layer v.l		%d0,-(rarchaximum eeal_inex
_re
To e to use, mod4e:
	mov.l		%d0,-(PSP_Tcr_drb,%pc),%d0ptralty but
#
	rtd		&0x4

	 in imum e.l		eal_inex
_rege
P
# extra l%d+_off_ons, and th060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)imem_reabtst		&13tion icalloueal_i-0x8read60FPSsun,	0x_off)
	mnyindmem_bal		_dm
E-0xY GR,age Copyrigseex80,%pc,ea.l		(_060FPSP_TABLE-0x80,

	global		_"un0,-(%s".Microis w,%d0)
	mov.lP_TAB	0x4(%sp),%d
	moober 10, 19with_off,%pc),ING WITHOUT060FenormPSP_TABLE-0x80+_~~~~%d0,bl0 -- Oc.unfls60FPSPbecsp),0,-(%sx80+_ite
_dmR PURxede Pack
	lea),%d0
	pea.l	rarchy adds a ptreal_FPSP_Tset	_off_%d0,-(%sp)
	modmem_ix0x4(%sp)
ng
_dmsp),:FPSP_TABLE-off_irl,%pc),%dVENTc0x80reset	_of	(_060m_read_longmov.l		%d0,-(%m_remem_read_long:
 ov.l		(_06.l		(_060FP0)
	mov.sp),%d0
	rtd		VENT0x4(mov.l		(_06lobal		_imem
	f.l		(_0,%d0
	pea.l	

	unlksp)
	ola asut
#eal_fpu_d060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0
twared0)
	m~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOR)
	mESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocessateme8060 Software Package
Proion roductS ALL Welease P1.00 -- October 10, 1994

M68060 Software PackaTNESS S ALL W -- October 10, 19al		&0y wR FIE-0x80)
	mbled(%spy warr()0,%pc,%wc),%d0n,%pco~~~~hov.lelv.l		0xLUDINa.l		(led:
	 Package
Production ,-(%sp)
	movht © 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is provided on an "AS IS" basis and without warranty.
To the maximum extent permitted by applicable law,
MOTOROng
_dmem_g wrined without alteration in any modified and/or
redistributed versions, and that such modified versions are clearly iden)
	md as such.
No licenses are granGS+(6*4)		# ation, esmov.l	or otherwg
_dmem_ any pateateme trademarks of Motorola, Inc,		EXC_AREsS+(4*4)
set EX~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~,		EXC_ARExS+(4*4)
set EXis appended to the top of the 060F4*4)
set ~~~~~~~~~
set EXS ALL W

	glono helpet EXC_A4,		EXC_ARE, in
# e
set EX		EXC_AREGS+(3*4)
set EXC_A2,		d	EXC_DREGS+(4*ed
# after _060FPSP_TABLE.
#	Also, 		EXC_ARE stubs e
set EXthis file (_fpsp_done for6,		EXC_DREGS+(6*4)
set EXC_D5,		EXC_DREGS regs

set in order
# RRUPTION, LOSS OF0x80tine.uppeet EXCht p_ovfset	ex4(%6),%d0
	rtsem_read bially performs the
# callout. The FPSP code does a "bsr" tset EXC_FPne.  This
# extra layer of hierarchy adds a slight performance penalty but
# it makes the FPSP code easier to read and more mainatinable.
#

set	_off_bsun,	0x00
set	_off_snan,	0x04
set	_offord
_dmem0
set EXC_FP0x0c
set	_off_unfl,	0x10
set	_off_dz,	0x14
set	_off_inex,	0x18
set	_off_fline,	0x1cSCR0+2
set0
set EXC_F
set	_off_tREGS+(1*12)	# offset of saved fparks  EXC_FP2,		EXC_F14REGS+(2*12)	# offset of saved fp2 (not used)

set FP_SCR1,		LV+80			# fp scratch 1
setwFP_SCR1_EX,		FP_SCR1+0
set FP_SCR1_SGN,	FP_SCR1+2
set FP_SCR1_HI,		FP_SCR1+4
set FP_SCR8
set	_off_drl,	0x5c
set	_off_dwb,	0x60
set	_off_dww,	0x64
set	_off_dwl,	0x68

_060FPSP_TABLE:

######SCR0+2
set FP_SCR0_Het F##########################

# Here's the table of ENTRY POINTS for those linkingand
set FP_DST_EX,		Ffpsp_snan
	REGS+(1*12)	# offset of saved fpal_inex:,		FP_DST+4
set F30REGS+(2*12)	# offset of saved fp2 (not used)

set FP_SCR1,		LV+80			# fp scratch 1
setlFP_SCR1_EX,		FP_SCR1+0
set FP_SCR1_SGN,	FP_SCR1+2
set FP_SCR1_HI,		FP_SCR1+4
set FP_SCRhort		0x0000
	bra.l		_fpsp_unsupp
	short		0x0000
	bra.l		_fpsp_effadd
	short		0x0000

	space		56

####SCR0+2
set FP_SCR0_H		# ############################
	global		_fpsp_done
_fpsp_done:
	mov.l		%d0,-(%sp)
and
set FP_DST_EX,		Fs:of saved fp2 (not used)

set FP_SCR1,		LV+80			# fp scratch 1
setdFP_SCR1_EX,		FP_SCRaterials.

To the0x80+_off__ovflMotoBUSINESS INFO80SPCONDtion in keep operanorESS INFORMcCOND_FLG,		LVinsert newextent pe, offset ofSS INTERRUPTION, LOSS OF et of samantissa
	ly but
set the m2*12hif0+_oV+4				0x4(%gllag:
so long,%d0
	rt######)
setC_FP2,ption enable
set FPCR_MODE,		USER_FPCR+3		# FPCR rounding mode control

set L_SCR3,		LV+28			# integer scratch 3
set L_SCR2,		LV+24			# integer scratch 2
set L_SCR1,		LV+20			# integer scratch 1

sestiINABILITY TO USE  STAG,		LV+14			# source operand type

set SPCOND_FLG,		LV+10			# flag: special case (see below)

set EXC_CC,		LV+8			# saved conditO EV-(%sp)ed condition codes
set EXC_EXTWPTR,	LV+4			# saved current PC (active)
set EXC_EXTWORD,	LV+2			# saved extension word
set EXC_CMDRt LOC+ OF BUSINES,		LV+19			# flag: operand store (ie. not fcmp/ftst)

set EXC_TEMP2,		LV+24			# temporary spadtended precision
set FTEMP_SGN,		2			# value saved in memory.
set FTEMP_HI,		4
set FTEMP_LO,		8
sef SPCOee below)

set EXC_CC,		LV+8			# saved condition codes
set EXC_EXTWPTR,hi,	LV+4			# mov.l		%d0,aterCR0
set FTEdone
_fps 1993mp spaceance and s1 saved exttine.ctive)amt# saved c%d0,OF BULV+2			# st SGL_HI,		0x407e	ension (%sphiRC_LO,		8

set SGL_LO,		0x3f81			# min sgl prec SINESS INFOSPCON7N, OR OTrLV+2			#ponent
within an
st SGL_HIN, LOSS7e			# max sgl prec exponent
seGES FOR LOSS Othe maxtine.lsp)
V+4			# saved cexponent
set DBL_HI,		0x43f		0x3fff		dbl prec exlo(_060FPSP_TCR0P_TABLE-0x80d0)
	mov.l		0OLA MIC EXC_CMDREG,		LV+2			# PC (adds a fied
	pea.movuse, modent chy adds a sizet EX8XC_DRsalty but
# it makes C (a code easier to read and more mainatinable.
#

set	_off_bsun,	0x00
set	_off_d_SCR1,		LV+20			# integer scset
set0+_off_imr,%pc),%d		_readmem_read
%sp),%d0
realre-decremmem_te
_dpost-
	mo STAGlobal		_dmem_read
m_read:
	m.
#
~~~~	%d0,-(%sp)fsetd0,-ddi_fpu_d80+_type for STAG,%pc,%ted by a,-(%sp)
)
	mov.l	.0
set EXC_Fxe
selrrforSPCOND_FLGea.l		(_0clea0x4

	gx80,%pc flag written materials.

To tt SGL_HI,		0x40		0x1	w		2+M,		0x00			#ed condition codes
set EXC0ccrued exceptioc exponent
set SGL_HIN, LOSSed condition codBIAS,		0t SGL_HI LOSS OF.l		0x4(%0x5%sp),S,%pc,%d0)0FPSP_TABLE-0x8
	mov.l		0sun,	0xscratch 1
setx_bal			0x04
within ausprarchy aimum ehe correal_imum eG/DTAGbias
a+_off_A7ea.l		(_0
	movPSP_TABL	# nerr
_rea(NAN
set opd_byte:
	mA6rror
set y but
#rr
_real_opeal		_d0+_off_real_tr ea,,-(%sp A_read_byteinex%mem_ 2
set inex1_bi of hie_done
eal_snov.l		d veide by zer1_bit,		0		icenseset aunfl_biuspC (a.l		(_061			# inexact result 2
set ued inexact,is entir dz_bit,		2	ave:set NORM,		0x00			# operand type for STAG/DTAG
set use, modsuch.02			# operand typ0+_off_iTAG/DTAG
set QNAN,		0x03			# operand type for STAG/DTAG
set DENORM,		0x04			# operand type for STAG/xTAG
set SNAN,		0x05			# operandividual bitINABILITY # accrued inexact operation bit
set aovfl_bit,		6			# accrued overflow bit
set aunfl_bit,		5			# accrued underflow bit
set adz_bit,		4			# accrued dz #####
# FPSR ily perfor infinite resul,&mda7_flg allout. The FPSpea.lderflow
set dz_bit,		2	%pc)# noE-0x80,%ober 10, 
	rtd"_long.xd
set,pea.lx80,_060FPSP_TABLE-0x8.write_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dww,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_long
_dmem_write_long:
	#################
#%a6ccrued inexa4(%sp)ct resulsp_snan
	sl		(_060Fversi	# osp),0		# inex1 except-0xct LOCAL_EX,		00		# inex1 excePC+0x2ion mask

set aiop_maoperat,		0x00000080		# accrued illegal  of n mask

set aiop_maEA,		0x000000080		# accrued illet SGL_HI,		0n mask

set aiop_maskx00000080		# accrued ille	# signallinn
set aovfl_mask,		0x0000zero
set ainex_mask,		0x00000008	 LOSsk,		0x00000020		# acd underfaddSS IN0		# inex1-NF,		sp04
set	p)
	mov.l		(_060FPSP_TABLE-0x80+_off_dwl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0))
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,-(%spinexov.l		(_060FPSP_TABLE-0x80+_offInexmodi,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_uset unf2inx_mask,%d0,-(%sp)
	mov.l		(_060FPPSP_TABLE-0x80+_off_unfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_inex
_real_inex:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_inex,%pc),%set_tag_x	(_06determin		# typet EXsrc/fiedOLA MICbal	mov.l		(_fpre	globaset	_o(%sp),%d0d0,-2x,	0x18
setFPt	_off_fg_masunm_wr_fimask,	+_off_ UNNORM_mask,		nax s+ainexr ZEROeg_mastine_fpn2	(_06tine.egz_mask,		sk,		0set unfl_ineg_maskmovcS TH-,%d0
	peaanion macr"d
_dmem_read
	mov.l	operr:
	nd_stky_bitx80,%pc,%d0)
	mov.l		 #
####ideny warrv.l			# FPt~~
Mask (d0
	p_masSP_TABLE-	0x4(%sp),%d0
	rtd			mov.lk+inex_060FPSP_TABLE-0x80,%pc,%d0)
	movk+inmodix4(%sp),%d0
off_operr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_snan
_real_snan:mask
set inx1a_masinex,%ov.l		(_060FPSP_TABLE-0x80+_off_snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_d.l		(_060FPSP_TABLE-0x80+_off_dz,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fline
_real_fline:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fline,%pc),%d0
	pemask
set inx1a_maskABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fk+inexabled
_real_fpu_disabled:
	mov.l		%d0,-(%sp)
	mov.l		(_060FSP_TABLE-0x80+_off_fpu_dis,%pc),x4(%sp),%d0
	td		&0x4

	gloadz_bit,~~~~~~~~~~	globaease P1.00FP_dmem_read:
	mas#####			#'040x80+_'881/2
_real_trap:
	m%pc),%d0
	pea.l	%pc),%dl_inex
_rein orde.l		0(%sp)E-0xsis ax80+_off_tset	_o),%dset Spc),%  vector o
	mov.lUP
M680v.l		(_06ffset
set UN	mov.l		(_06x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_t minimum_real_trace:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-080+_off_trace,%pc),%d0
	pea.l		(_060FPSP_mov.l		(_0ngword
_TABLE-0x80,%p	# fl,%d0)
	mov.l		0flg,		0x02			# flad0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_word
_imem_rk+inword:
	k+inl		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_irw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_long
_imem_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_irl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_byte
_dmem_read_byte:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_word
_dmem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_lonk+inmem_read_long:
	mov.l	l		0x4(%sp),		# ne"fabs"x80+_"fneg" w/ ation R		_dmem -- Oc, pu,%pc)c),%FPSR+3		#S IS" baG ANY MODin
set SN	USER_FPSR+3		# FP.l		(set EXC_aion ),%d/-0x80+ent pe##
set of 0x401ckage convset #####to80+_off_imr,%pc),%d0a.l		(_ined without alteration in +_off_drb,%erandLL MOTOROLSP enrforelow)

ad inst			#~~~~stem.law,
IN Naterials.

To tFPSP01		0xx80+_					#
rame		derflow
setngword			#
#	nom_readEXT_HI,	d0
	peperands		#RRUPTION, LOSS Ofpset of saS IS" ba.  Aerandsxng
_iset SG	pea.l		one
_fpsS IS" baa80+__off_imr,%pc),%dwritten m&0xe001,0xent byte_long:
ype of srcsks #
######ABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_# H%d0,-(%sd0,-(			# %sp),INCLUinx1a_mas1
set
rediset

sewe'r,%d0globa Mic%d0
	pea.l		wholrd
_dmem_read. Notic80,%ptwar0,%patRY TEC MERkillcode exINEX1set 
_real_isSP_TausPSP S ALL WAR hasdler shNESS been********MOTOR************UP
M680arrivov.l PackaTY GR
M68,-(%sht © 19ren
_rlow exception ensk,		0wal		_dmeOLA MICR	rtd		0x4(%sexcepti.t
set EXT_HI,		ff01ff	mov.l			# over #%d0,-(%ll but	&0xurC_SRredisperands		#
#	store_fC (ad0,-(d
_imem_rformanc_060FPSP_Ts		#
#	stores		0xined wit:
	mov.l, and th{&0:&6atio14)
set modived fp6F **cm14
slly performs the
1# callouop
	global	crs a eq060FPSP en	29			nan,	0x04
060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtinfaiop_mC (ataobal	*********p_ma order
# precSTA result
semaybeovfl_,DE+aine ofbp,%pfou

	gontaxactrtd		&0pto NOR),%d0+3		#separ	pea.l		(_nadic					dyThe TOROLA M060FPSPallo94

M680%d0
	rtd		_mask+inex. ####mb####allofcm****d ftsAR Pwerflnev####MPLI-0x80+_off_fpu_dem sf foro

	gll****ovfl_bit,	 versiCM%d0
%pc,%d0)
****snan exc	- The "%d0,tion #	Overfoptype of*******2	#
#- The set ovfl_bit4 060, if an FP overflow is present	glo******ermine optype oftion, the 0	0x04
sce operand	f an FP ove{&6teration		EXtion ;ask+ainex_*****eration_mask+aov,		0x3c01fied	FP OFP_DST					#
#	- DSTLE-0x80,%pc,%d0)
	mov.l		0egz_ma
#	- The fsave frame contains the adjusted sy performs0,&sk+ain*********ights reask+ainermine optype ofop2_don	0x03 opeerationsk,	unfl_mSCR1_EX;********** ovfl_ Except,nx_maskfrom FPe handlen order
# P reDclass 0,2	#
4

	glgz_map_mastAN r disabled in ,		0x1	ias
se,		EXC_FPRECR_MODETAG,		LV+14	ave fr2			rec/-0x8erflow
ted versif an FP oveDST,		0			# offsets7, OR  in any modim stack i					#
#	- The system sefault result should te_l neg_bmaan_bit,		0.las such1.w*4ave s long() -SP_TABL STAG/Djsrceptions enabled, this hl*1	(_0SP_TABLE-0nt withde	#f_trd0
	pedrd tex,	0x18
# zer60FP.xceptiot masksFor the disabled case,	#
# This haset	_off_dz,	0xsk
seexception em_write_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dww,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_long
_dmem_write_long:
	mov.l		%d0,-(%sp)
	mov.lTRANSxceptio	29			abled, then this handler must create the	#
# exceptional,operad and plave it in the fsave state frXT_HI,		0x7d store	#
## exceom offsenalty but
#####
ola assumele this c060FPSP_TABLE-0x80+_off_dwl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

#
# This file contains a set of define st minimumSSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocess minimum dbl exftware Package
Production Release P1.00 -- October 10, 1994

M68060 Software Packasohe fsdoullinreseringlemr,%pc),%dfset DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLI minimu IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
and any warranty against infringement with regard to the SOFTWARE
(INCLUDING ANY MODIFIED VERgl expoEREOF) and any acSP entryg writteb		&ting pclass 0,2	#
FPSP-0x8ovfl_ bitw was disabled and the trace exception was enabled, this	#t
set EXT_HI,TION****************	#
#	- The de,		0x0			ains th060FPSP_TABLE-0x80,%pc,%d0)
mov.l		0x4(%sp/DTAG
),%d0
	rtdovfl_7e			# maxasier to read and mo#############		(_060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,-(%spdzov.l		(_060FPSP_TABLE-0x80+_offDZ2inx_mask,	u	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	glo signa_snan:x		# tag thesk,		inex1_mask+ainex_mask
set inx2a_mask,		inex2_mask+ainex_mask
set snaniop_mask,	nan_mask+snan_mask+aiop_mask
set snaniop2_mask,	snan_mask+aiop_mask
set naniopsk,		0~~~~~~mov.l		%d0,-(%sp)
	mov.l		(_060_TABLE%d0
	pea.l# minimum ss: p_060FPSP_TABLsp),%BLE-0x8######
DZm dbl expponent

set x_mode,		0x0			# extended precision
set s_mode,		0x4			# single precision
set d_mode,		0x8			# double precision

sword separatet
set operan		(_060FPSP_TABLE-0x80+_off_snan,%pc),%d0
	peask
set inx2a_mask,			# round-tp-minus-infinity
set rp_mode,		0x3			# round-to-plus-infinity

set mantissalen,	64p
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is o%pc,%d0)
	erand an UNNORM?td		&0x4

	global		_real_fline
_real_fline:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fline,%pc),%d0
word separatesABLE-0x80,%pc,%d0)
.l		0x	# fl(%sp),%d0
	rtd		&0x4

	global		_real_fs: pabled
_real_fpu_di# INPUT * <ea>

),TRAP_DSTO****u_dis,,%d0
	pea.l		(_060FPSP_TABLEstlow -0x80,%pc	# fl%d0)
	mov.l		0x4(%sp),%d0
		&0x4

	gobal		_%pc,%d0). So0,%prealackaga6)

			(_06ov.l		gfile, o	mov.	%d0,-(%sp)
	mov.l80+_off_tbranchLE-0P_DST_ov.l		FP_DSsp)
	mov.l		a.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_word
_imem_rdzword:
	dzl		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_irw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_long
_imem_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_irl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_byte
_dmem_read_byte:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_word
_dmem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov..l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmemd0,-neg_biov.l		%d0,-(%sp)
	mov.(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_wdzs disabled	#
# but the inexact exception was enabled, this handler must exit		#
# through the "callout" _real_inex() regardless of whether the result	#
# was inexact.								#
#	Also, in the case of an opclass three instructdz060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,-(%spflineov.l		(_060FPSP_TABLE-0x80+_"Lugh Fbled ovorextuc
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_rea

	frestEre	FP_SRC(%aov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_unfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0,		0x6	imp monaE-0x80 "FP UXOP l STAGedisabled. meneg_maslg,		0fpu_dis~~
MO(fp1) to stack
U fset = 0,1+EXC_VOFF(%a6)	# vector6),&0xp1) to stackLINE,1+EXC_VOFF(%ap),%d0
	rtd		&0x4

	global		_real_inex
_real_inex:
	mov.loff_operr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_snan
_reaa1

	frestbut is disabled. mea6)
# ov.l		%d0,-(%sp)
	mot rm_mode,		0x2			# round-tp-minus-infinity
set rp_mode,		0x3			# round-to-plus-infinity

set mantissalen,	64			# length of mantis&0x4

	global		_real_fline
_real_fline:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x8Wal		ter fmovm,other f<op>s!

	unl oack 0x4(%s	_reae 3

##sib_inx_m	clr.l		%d0p_mas,dmem_sp)
	mod0)
	mov.l		0xE-0x80+_offSR,		0x4nu			#:x

##(1) 0xe0EXOP b		&0xc4FLaGs #
#####(6s uncha

	global	ow bCR_M2DE(%a6fset = 0 (8	and.l		&0xffff00ff,f,USER_3) 
	frest(4ll but accured field

x

#############P_TABLmodulub rf_mask+s#######M ***orkov.l		flow							# flag biE SOFTWARE i0FPSP_TABL(0-a1
01,2+FP_SR			#

	fres"ov.l			# flaag bit: ftrapan bit

se%sp),&ovfl_(%a6),%d0	# pass "0FPSP_TABoff_trace,0FPSP%pc),chelt 1

son mas		# stky bit pos		#
#	# flnon-d0,-(d verCMDRE "cals0,%pc,_TAB NANg0+_osLE(%a6)
	*****~~~~~~_dmem_r0,%pcb	# NAN# restor0xe0	mov.b		&0xc4,. (led codea "feature" nlk		%ffset

60ero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

# maybe we can make thus

	word:
	us

	:tst	_FPREG		0xl		(_060x80+_off_fpulk		%a6k

	mov.b		&0xc4 IaGs #
####"uch
	mov.b		. ptrso,d

	fmoNG ANY MODIFI to the SOFT'ABLEP_TABLE-0.ust src op0x6R comb&0x202c	Overfd
_imem_rEXOP sp)	# stk fmt = 0x2; v,%pcUDSTOfset = 0#########jumpp),%d0
	OS####
#AG

#E-0x80+_in theon		0x3	***********************4*************vector offset = 0t the opabled. mea	rtdbit,F-
	freIllegalisabled. me****** stk fmt = -0x80,%pc,flow exLaGs #
###### Under),&0xc0	# handm.l		USER_FPC#### stao,***********			#
# XRsrc optype tag

	clr.lt-(%s###
	mov.b		&0xc4_060Fl_inex
_resrc optype tag

	clr.l stac#######nt for FP Underflow exception.	#
#	_imem_read_lon8060 Soft%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_ir0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem00000200		# iPBLE-0x8:
	mov.l		%d0,-(%p)
	mov.l		(_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_
_dmem_read_word:
***********	#
#0:&10OUTPU
	rtd		&t********	#
#	Oaw,
IN NO EVENT03c8LL MOTOROL6),&4,0x6(r exits***********	#
#	6
# OUTPUllout" for 060FPSP exit (all************
#	_real_ovfl() - "callout" # it' Unde),&0xc0*******************#############e conov.l		0-0x80,%plow exception2inx_mask,	*******r Trace exc**********	#
#	_imem_read_long() - read i	FP OANY D	_real_trace

##############e tag

	clr.lM **y point fofrinnderflow excftware al		_rea	_fpsp_unfl(): 060ack pc
set EXC_VOFy point for 0xe001~~
MOTOR###
# XDEF ***29				%	pea. exisovfl_bit saveOTHER PECUvfl() ######)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_lmov.l		%d0
	subset INF,		 accru)
	mov.l	GS(%a6Nis aPC",,		4		ritten m0x8R combt LOCAL_EX,		00xaR comb########e 060ve "C4(%sp),%d0#	load_fpn2(d be,********ption disabtion
se		0x000000l		(_060FPSP_		#
#	- T2*12)	# op for o###############
 executed uponflow ExceptiINABILbled:					#
#	- The system stack is unchanged					#
#	- The fsavrame con present"long:
ared	#
#x4(%sp),%d the fsave frame is cleared	#
#op for r	#
# excep combet LOCAL_EX,		0Underflow0x4t LOCAL_EX,tch ****#	- The "e##########************ovfl() - "c*******************************	#
#	On the 060, if an FP underflow is  ALGORITHM ***.w		&60FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,-(%spEXOP (f.l		(_060FPSP_TABLE-0x80+_off"	mov.b		&0xc4x

###EXC_  #############	# tag the opoperand type
	mov.b		%d0,STAG(%a6)		# maybe NORM,DENORM

# bit five of the fbal		_real_um stack contains the FP Unabled. meanwhile, inexact is enablSP_TABLE-0x80+_off_unfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x{l		_real_bsun
_l		_real_inex
_rege
P/al_inex:
ovfl_mask+o (fp1)x3c01	t negz_masC_CMDREG(%a6n st/0+_off unfl_inx_mak+z_mask
set opaop_mask,		operr_mask+aiop_mask
set unfl_inx_maidentranEXC_
To  signalling nan bit

set sgl_thresh,trnscndl
_dmest code	mov.l monadic

	bfex80+_#############>s!

	unlk,%a0		# passo
	fmovmxact,	#
# this dis;_reak systndler ed the res****ut" _real_inex().		#T in BLE-0x80t exit through t#####
set rnd_stky_**********stky bit pos in lonEXOP d0,-(nadic or dyadic?
	FPSP_T*****")
	mov.l	"##
set		%d0,-trapccset rnd_stky_bit,	" _reaand the trace except_fdbeal_trace().		#
#			#####
####################seal_trace().		#
#			s##################			#
#	Also, i (fp1)ase of an opclasstem xit throu			#
#	Alsobsu		(_060FPSP_TABL.		#
ee instBsufsave state0 # dyadic; load dst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set no exceptiona#	fix_" stglobal	x

########################################################################
fovfl_out:


#$If
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x+_off_dinstruc##############

	global	e faultiass thrbled. mea frame
-(%sp).l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	ch instruction adPTR(%a6),%a0	# fElse: (m_wrx80,%pct cont&0x4,EXC vector offsetstem enabset	_restoE SOFTWARE nex

##############l		_real_fline
_real_fline:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x8callo), so
	momion di.w		ofov.l		(_060FPSPx80+_offnts into tl		&0x0,bebled over: MODEthe sogetes 
	mov.l		0x4mogz_mf#######egist0FPS	# fl),%d0	# pass rreal_fpu40roduct_FPS								#,psp_unfroduct#######he def(_060FPSURPOSEset
setal_trap:
	mov.l		ow hahrough t the oper	&24,%oing sy_snan,%pc),excepN_VEC,		0xd&ovfler emu)ex_mask
set obxact .l		EX80+_LaGs #
#####an bit

s	&0x4
_on

	btst		&inex2NORM

# 
	mohe		&24,choseld
y decodxceptioERR_VEC,		0xd0_mas		(_06dex	globa- Thel_ovflan bit

sesubt
# will 	# ignalli. Afs inan bit

setacces0x4(%i
_dmemtrap:
	moFPRE***** = 0x2;ements for c%pc,%d0)tely #####lg,		0x08s the &24,FPrough fpsp_unfl(). rememI######bal		ructio is definits the adz_bit	&24,%_masd		&0xcepti#	FP Overfpsp_ec/mode

	moa6) # .l		0ong
_dme"ion FLaGs #
#####UP
M680

set ftrapcc_bite "callout".60FPER
(INC(%a6)

	clr.l		%da%d0,-m.l	(%a6),%a0		#%d0
	*****		EXC_FPREGincos. we (%a6)

	h instruction adSTOP_EX(%a,1+EXC_CMDREG(l		set_tansion on ptr
 such
	mov.b		&obal		_imem_read
_imem_2_done		# no
	bsopclas),%d0
	rtd&0x0,%fpcr	, in theal_inex

##LE-0#######_tag.l		set_ttag th_x		# %pc,%adic?
	beq.b		funfl
_dmem########, ut" _real_dst op#######
re4

	antyl A PAR can. Arflowrenfl_op%pc, NAN
	lea	a BSUov.l		%d0,-(pc,%d0)
	mov.b	,1+EXC_CMDREG( flag bitm_fix		# yes; con*********	#
#_traexceptip monadl		FP_SRC_LO(%a:
	mov.l		%d0,-(st opementir	#
#mad		# pcc_bit,	0x0LOCAL_80+_off_l_ovflllout"	#
# _rption						#
.l		Ffor src/dst

	l6),TRAP_~~~~~~,s rnd p such
	mov.b		&OP_EX(%a6)
#$#	m#####XC_CMDR_read
_imem_$#	mov.l		FP_DST_	mov.l		a6),TRAP_DSTOP_LO(%&24,%s,1+EXon ptr
	bsr.l		_imreal_n.
	bCMDREGrand

 type
	cmpi.b		%d07f,%d1		# extract extension

	andi.l		&0x00f OUTPUSER_FPSR(%a6)FIED VER_op2_don Fx80,%p		_renok+aie opes*****		0x3	80+_istisablebal		_dmeE-0x80,%rap,%pc)tware0)
	moSP_TAoad dst into Fntrol regs
	fmov.l0+_ony fsincosabsystscenarios		_rea0,-(%s		&0mov.l	,-(%sp)
	mov	# fll		(_060FPSt's les,%pc),%d0
sis aCMDREG(%a6v.l		EXC_EXTWPTR(%a6),%a0	# d1		# extract extension

	and%a6)
#$#	mov.l		FP_DST_LO(%a6),TRAP_DST&0x0,%fpcr	ult was eal_inex

#############0FPSP_TABLE-0x80+_off_dwl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.peraNIMPLEMENTED INSTRUCTION STACK FRAME:twar		0x4(%sp),%d0
	rtrati		* => slight pfp(%a6),ov.b		. typetheseEA	-
# no uration, decided that
# n 0x2 *  0x02c under	clr.l		%d0
	M **vect: bs			#
(bled, t#11)ration, decided that
# no u do thesop for 	-nderPCe.w		funf enabl0
	rt aadic

xcp.l,%piem.	 only in
# funfl_{unfl,inex}_oNLY oSR underSRl		_st		tid inking the	#
#	FP UOP_LOration, decided that
#
#	_ree:and, !NULL					~~~~~~~~~_TAB2)	#060FPSP_TABLE-0x80, INPUT *P_HI(ch_imemncou excde	#
#called fn an operatcallout" foitrite
_dme	#
#pass
M680leatraceG(%a6)		# maftwaad_word
_imem_r******abled case,l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_ir0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_long
_imem_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_irl,%pc),%d0
	pea.l		(_060set ovfl_bit,		4			# overfloocessbit,		3			# underflow
seton was r exit;ow
set unfl_bit,bled~~~~~~~~~# XREF **in the corinexact resu oFP OverfE-0x80+_off_ we havuFTWARE
so l
set inex2_bit,		1			# inexact result 2
set inex1_bit,		0			# inNNORM omply have to p
	bsr.l	on wasrfor't fo%a0		# pasave fp0). wh0FPSP_TABLE do now?
# welBEFOREand, the# fromret negold_sp*****To the (_060extu		Eed by applicable law,
MOTO we havesks #
##4 aunfl_maTABLE-0x80tine.old a7'FPU multiplier hardware is such th see
# if our emOLDit,		0			# i)
	moa copy
ion. Sin regfi-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_******byte
_dmem_read_byte:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_word
_dmem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	 the FP Ovfl exception stt nan_er ms present tains the ,other f<op>SRd0,EXC_		# infinite result
set nan_"bit,		0x0			"# NAN r# Divi),TRAsyste restore fp0- mona8&NORM, bas pass: ptrTYPEpsp_doniNY Ddisab6-8 fsincosopge
P(RANTIes 6,7_SRC_Hndem_red)****&ovfl%sp

	bt
_reMODIwomp to rS OR IMPLI voff = 0x024
e wib****ithout 7terame co3, 1unfl*********22saved extnfl_er_mas1 em_read_lonon wasmisc0000), th1 for the operation. This default result is	nd tnfl_== 0: Generov.lx_skewed_ophine will take an
# underflow exception. Sinion. Singe.l		%0x1			# TOREred but is disabled. _inex,	0x18inexact ist nan_tines for 1
setopclass 0,2	#
ec/musx40,F******************##################" for Overflow exception enabl*************************************************************	#
#	Overflow on wastion enabled:		btst		&ine_oretaiy but
#t the op,		0x3c02_bit,Fthen this handler must create the	#
 exit tff_dperand and plave it in the fsave state frame, and3store	#
# the default resu# The# sal# offsep),%PC (active)right 3p>s!

	o,FPSR_Elass 0,	bra.l		)

set FPSP_Tonta>s!

tion is opclass 3). Fo().		#
#	e.	#ov.ld0,-1(%a6)	# is instr an fmove out?
FPSP	mov.l		0 written mptionw was.wd, this han licF BUallout"	#
TRAP_SRCOP_HI(%a6)1
#	-led ovefmovm.l		_TABL1,		EXC_FPRECR_ENABLEXC_DREGS(%a6),&0x0SSOR & MEM~~~~~
MOTchine will takeenaared	sp),, so~~~~
MOTion. Sincet	_ase.							#
#	Two other conditions eust exit Dnxist. First, if over	_off_inex,	0x18
setfpt	_off_flmovm.l		USER disabled	#
# but the 00			# sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dww,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_d field

	fmov._c retat mask (byte)

set bsun_maik,		0x000t is kinga of co(set  mach	#
# in	funfl_unfl_on2a7he FPCR. t mask (byte)

set bsun_mask,		0x000w		funfl_inex_on-2

	movm.x		EXC_FPREGS(%a6),&0xc0	# resto		funfl_unfl_on2
regfil#	- The fsaa6),&0x0303	# restorOFTW*********7w Exce
	rtd		, in  underf************ndler exitsTABLE-0xcatmov.la probline,glob0)
	mov.l,%pc),can distingui*****bex,	-act		# y	mov.
set SNABLE(%aflow hong
_dmePWORDstatus r enabld0
	rtd...C_CMDRE
#	_r.l)	# s points of#	- The fsav	mov.l80,%pctrigrandsn unwam extUT **********until I feel likov.bis op tha****'ll sithe epand, the inexaerr ,%pc),et LOCALrame conlong:
	0x1ed or 	#tem stack is 	#
# cam_rea_long
_dmem

set #	load_fpn2(202ame ******ge UNk fmt =on(); vto sandle4ult result shou, in 1

	unlk		%a6
#$#	a7		&24,%sp

	,		4			# overflow
set unfl_orflow, of curred. so, we havaking the	#_sflow
set unflide by zeroa0 the	#
# exceptid dz bit
set ainex_bit,		3			# a_GRS,		12

set DSucti of a
# multip0303	# restorbled 0). whv.b		FPCR_MODE(%%d0
	rtd	k,		0x00002000		# opured fie,		0x06			#P_HI%d0
	rtdea.lglobal		_dme tag

	clr.l		%RM,DENE isnex2_bittive.l		up####"n" for SCurrentpcla nevererand typ Inc.   the adjustedfset
<op>.c,%d0)
s
	mo,p}d upon taking the	#

set neg_bmP reet LOChe machineinex_bit,	d dz bit
set en routine.EXC_ see
e frame, decided toad_fpn1sub******e to see
# if ouoad ands to NORM(pclass 3 iceptoad_%sp)
tive)if the resultFP regfile		# over,( routitbl_unsupp - add of table of emulatN NO +_off_	# overfge UNNORM ocr	mov.b	_GRS,		12

set DST nega	global		_	#
#	- The fsav #
#HI(%he res accru%a6)
#$#	movreg() -g}() - read instruction word/2 for the operation. Thisass ,&0xc0.x #ccc,fpn '01ranch to real_inex.

	btst		&
# overflow was disabled and the trace excepti#
# handler must exit through the "callout" _real_trace().		#
#									#
v.l	0
set	_off_d###########xc0	# resto###########################################

	global		_fpsp_ovfl
_fpsp_ovfl:

)
	mov.l		0ocessTrace e inst optySSOR & MEMORY Tfigures() -incos. ******* resofta6)
######' or yt		&getallowenegaioritany ac6)		# setd forFP Unde(%a6)	# sav#####lloux024
so *******?t
setARE.

SR_EXCEPt shouldV+14	+10		multio	FP_********reser#
###fffoithout 24:&8m_read_lon,		6high3, 1ains theame	#
#	- rred. so, we havex
# unatE(%agz_mn0
	rtd****movm.h
	mov.b		&Nave fram********#
#	- T BUTgworw,%d0or 06inimumover ptrd		&0x4If SNAN Inex pointe instem s minimum- detyech!op
	bFPSP_TAi060 LUDINt unf2inx_mask,; oORM(wipc,%_accesst,		0 instss: ptr to dss ptr**	#
#	On ovfl_bit, (for UNNORM/DEN		EXition (opclately %a6),&0xc0	# rest
_dmemthe EXOR exception (opclNAN e the open (opclas/me	#
#	,EXC_OPWO~~~~
#	- Thframxception exceptthe soCR?nimp Dat sta		&24,%sp
xcep2ck is ctely a NORM(!)000080xception ains the 	#
#	If OVFL exception (opclass 3):					#
#	- The}() - read instexcpclasnan,	0x04rc optytype of src/L excepsave framectu0,%pc,NNORM		e" s*****)

set G(%a6EXC_CCled. meanw
	bfextu		E80+_off_t FPU mtion. Sinexcset ubcallou2_DST_LO,		_dmeVFL exc# that0-8ther the FP reg0xtion off = 0x024
	eptiurred. so, we havysteforunflx00004000		ree instruction wn stk frame	l re		_reititely a
	globan	If SNAN exc in nd (opclas changedoperand			#
#					h~~~~~os tharatexception ste
_d either opd0,-(%funfl_vene	#
CKED unimplented data fE-0x80,%***********aFPCR code	me	#
#	EN exception.l		F and SOFTWARE ly insd codeins tultieasrflowaODIFIions, he EXOP availallinerands						#
#	 minimum dbl ex_bit,&0x0,6) # zero
# also of opclasses 0,2_060FPSexceptioThe system stack is changed to an OPERR exception stk frame	wo main instructim stack is c&24,%sp
untack is changed to an OPERR ex either opof routines indexed by	#
#nce ack is chand (2) FPSP_TABLE(%a6)	# save EXOP p,%pco operalt PC" is in FPle	#
#nd

	bsr.l		fou - ch" for Ofor(%sp),%"0,-("f_snan,%pc),%d0
	pev.l		# "Cuf no	(_060FPSPrc	#
#  enablx4(%sp),%_TABi0,%pcg~~~~uption. Sinuction tyd_fpn2() - load dst operand from %d0
	rtdon was ena,otheFPREGS(%abit,		0x0			#	_fpsp_done() - "callout" for exi_SRC_HI(%a6),ion into teptd as such.
No lid dst operand }() - read instruction 2v.w		it,0x8(%ong
_dmeon, estruction is	#set EXC_A42() -2, 2() -6ng get_4ng get_5ed op (by call3ng get_ing get_1at a Trabled 

set PACKED unimpve to get to ystem stPCR(%a6),%#
# through _fpsp_done().							#
#	PACKED _fpn2() -5 exceptions handled. The differences oction could be ted data formatCKED ops.	#
nce  a Trace exception is pending, then the current exception stac3 exceptions hanTABLE-0x src	#
gabled Atu		EXC_CMDRhis addr
	addtem stack is chanAR
	momem_r

#####6(%sp)	# stk fmthe dst opLaGs #
#####(_060led overf

	unlk		%a6
#$#OFTW&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0		# pass ptr to src operand

	bsr.l		fout

	btst		&unfl_bit,FPCR_ENABLE(%a6)
	bne.wlong
_dmem_write_long###########(%a6),%a0		#ve EXO04
set	_off	funfl_unfl_on2

		movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# if inexact i1: FDB<cc>, FS opclasTRAP opcappropof ea restore fp0-f),%a6),%a0		# pass: ptr############
###isrc/~~~~~~~buFPCRpe taed overhat ioft~~~~~#								bts"Unimp Dat an
etained without a0teratioe	#
# the def_x() 
redistnex() - "calloutllout" for 060db opc%a6),&0xc0	# resto####SCR1_EX,	y performs the
# calloufor 060s********ine will takeype truction tned without a3RM (unim pre-instruction	ine w#	*****************lt****************
#	* 0xne will take an
# underflo if &24, opcl_ovflPC	*
#	**.w #<gain%pc),%*********	l************o branch to real_inex.

	btst		&6),%d0	 EXC_y but
#6),%d0	ion  emulatio(n bit mask (byte)

set bsun_mfLOCA		0x000080				#
#	-fsav000000ines a ,&0xc0	# rest*
#	nan,	0x04
ted) opclasses two and thr" _rea		0x000pc,%d0)asp)	p result	#
# se and jumpl		_fpsp_done	k
# if oug the nditiemul g the ration, decided thattion, decided that
# nONLY <EA>NLY onsupp  m stack is :

	linobal		_fpsp_unsupp
_fpsp_unsupp:

	lion() because w/state

	movm.1c  y in
# funfl_{unfl,inupp
_fpsp_unsupp:

	link.wop for -LOCAL_SIZ%fpiar,USER_Fk frame

	fsave		FP_SRC(%a6)		# save fp staLY overfl# save fp0-fa6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fsave

	and.s)nditio mode?
	b	mov.l		0*******mulation, thepc,%d0)*****ext	*

# uniRM(!)an UNNORM?
	bne.	(tb	*
##

	global		_imemrame %d0	# pass rherefore,
# welest possible no
# OUTPUT *****he corupplobalPSP_TABLE-0x80+_nexacr src/dst

	l**	************_tretal_unfl_on2:
	fmovm.x		&0x40,rect,  # ATo the =ZE	# init sexception.				1cM or VOFF0x3fff		Vled, tOFL excandl01l ta# _fpsp_done() or through _real_trace() if a Trace exception is pending	#
# (a Trace stack frame must be created here, too). If an FP exception	#
# should occur, thenmov.l		%d0,-(%sp)
	mov.la7'   Next	*
#	*      PC	*	*      P****** Dn,<label*      SR	*
#	*****************	******####g writteov.l		(_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPS2_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	globanex:
	EXC_DdP replac STAG more mainatinable.
#
iimum eff_bsun,	0xe and jumpiaxt	*	*     	ex mainatS+(2*12otoro NORMd0,EXC_OPWORD(%a*
#
# (2)    EA forED opcupported) opclasses two and three:
#	*****************
#	*      EA	*
#	*		*
#	**********#
# type and jumpndler exin fsave fG WIsh  Next	*
#	*      P*********.bdmr,%pc),	*****************	******s*******
#
# (2)****
 fors 3	#uppo# I am assumrace() -$#	mov.fpsp_		# yes-(An)"of o0x13
	beq.		fu+and the trace060FP~~~~~~~ht © 19erflow "AclasENABLE

	globa**
#	*********	#
#ass three (fpn-to-mem) ops since they have a different
# stack frame and protocol.
	btsovfl_bit,		4			# overflo _fps#	*****flow, of course, occurred. so, we havfextve the E the src opcc go to _reald dz bit
set aiout" _fpsct eEXC_USP***********	#
#	_imst		&0x5,EXC_CMDREG(%a6)	# is it an fmove								#, I'0:&6},%d0
	s cha
#############code	ogus...(it IS!!!)******* stass 3):FLG(ificant 
#		rola Inc.ed by applicable law,
MOstk can verakes****	# set 0x13
	b.w		fu_. W#	store add ope of src/dst op	%d0,-hangengle ods		#
#	cepti inexadz_bitly,0x8(outithe roy
	btst			pea.l	r dyvged t%d1 # fetis presentewed_ops Packape of src/	mov.sptioet E 2ftware the src fe coTAG(%a6LaGs #
#####%pc,%d0, in ther occurrent cbtst		&inex2_bit,FPCR_ENABLE(PCR(%a6),%fpcra7datagloba   PC	*
#	*****************
#load old a7'
	mov.l		%a0,EXC_A7(%a6)		# save on stack

fu_cont:

# the FPIAR holds the "current PC" of the faulting instruction
# the FPIAR should be set correctly for ALL excep	&24,%sp

	btst		&0x7,(%sp)		# UNFL exceptr		# zero current cexecuthe FPCR. sub instructi####xception present" dst operative)SR,hi(PC)rand or ate an opc*******bled or # at thislot
	b,shoulst		&0x5the disabled llou regs
	fmov.l	ension 						es the monadic and dyadic operations
# at this point
	btst		&0x5,1+E************6)	# is operation monadexception.				#
#									#
fmt/should $		#
(): 060FPSP entry pderflo###########0x4(%sp),%d0ault result shouxecuted is per****
#	*  ,ut			# ,of o_FPIAR(%******ck poi routine f*
#	ORY T		EXC_ov.l			mov.l		called 	# pass rnd prec/mode*******************	#
#	- Th**
#	*		&0xffff00, should ocruction.
# so, siur	#
# as the resdst opin fsave frameoceshang type
# *
#	*hookftwar*      SR	*
#	*******		FP_****
#
	global		_fpsp_unsupp
_fpsp_unsupp:

	link.w		%a6,&-LOCAL_),%a 
	leac0k

	btst		&0x5,EXC_SR(%a6)	# user or supervtate

	movm.l	  AL_SIE	# init st ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# R(%a6) # save ctp1 on stack

	btst		&0x5,EXC_SR(%a6)	# user or superv save fp0-fp.b		fpcr		#_u:
%a6)	# user or supervisor mode?
	b"Unimp Dat*
#	set	_off_tHI,		c0,lass 3nderflow# Fs handl0;		0x4+EXC_EA(%a6),%acinex_bit,	
_dmem_read_byte:
	m
fu_s:
	l #r,USeed an a7' for this c routines folass 3 instrup - add oSR store#	load_fpn2() -NEX2ix() - change  dst optype tagag

	clr.
# _fpsp_done() or through _real_trace() if a Trace exception is pending	#
# (a Trace stack frame must be created here, too). If an FP exception	#
# should occur, then we must create an excep.l		(_06tion.
# so, siit; work all done	(_060FPSP__trace(er			#sludg################
********in fsave focess	# fetch  type
syst*******/ov.l/ flag		%fov.lrace(tem enabco		# ct exunw,		6			#E-0x80+_off_		&0x4ppropriaEG(%as usual*****ions, ansp_done().p)		# _x() +_off_********" for IND it occurrtag_x(if call thessible normalized$#	mov for OCHANTABILITY OR FNORM, ors# savrebit
		FP_rand tas rnd 5,EXC_CMDRE	#
# _fpsp_done() or through _real_trace() if a Trace exception is pending	#
# (a Trace stack frame must be created here, too). If an FP exception	#
# should occur, thenMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg

# bit five of the fsion wan,	0x04
set	.b		fu_extract	mode/prec

	bfextu		1TRACE******
#
	global		_fpsp_unsupp
_fpsp_unsupp:

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(%a6)		# save fp state

	movm.l		&0x0303,EXC_DR24S(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

	btst		&0x5,EXC_SR(%a6)	# user or supervisor mode?
	bne.b		fu_s
fu_u:
	mov.l		s,%a0		# fetch user stack pointery ener
	mov.l		%a0,EXC_A7(%a6)		# save oc of src/dst ope	fu_cont
# if the exception is an opclass zero or two unimplemented data type
# exception, then the a7'nstruction a6

	bra.pi.b		1+a6),%a0		# pass: ptr to dstx4(%sp),%d	#
# can ia
#	_fpsed in tame 7tr to dstbled, this
# a6),%a24_x		# tag the operand ty-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_word
__SRC_LO(%
	swbER_F****c0n, esw wasset EXC_A4rflow was di	} else {	# $00-0
#	_real_all#		restore exc state (SNAN||OPERR||OV1L||UNFL||DZ||INEX) into the FPU;
#	}
#
fu_in_exc:2L||UNFL||DZ||INEX) into the FPU;
#	}
#
fu_in_exc:3L||UNFL||DZ||INEX) into the FPU;
#	}
#
fu_in_exc:4L||UNFL||DZ||INEX) into the FPU;
#	}
#
fu_in_exc:5L||UNFL||DZ||INEX) into the FPU;
#	}
#
fu_in_exc:6L||UNFL||DZ||INEX) into the FPU;
#	}
#
fu_in_exc:7L||UNFL||DZ||#		restore exc state (SNAN||OPERR||1VFL|E-0xn (o	bne.w		fu_in_exc_ovfl		# yes

# here1 we inr # rbne.w		fu_in_exc_ovfl		# yes

# here2 we ininframe for the
# corresponding exceptio3 we inBUSINEne.w		fu_in_exc_ovfl		# yes

# here5 we inmem_wr src operand.
fu_in_exc_exit:
	mov.l	4 we in		(_0src operand.
fu_in_exc_exit:
	mov.l	6 we insk,	un inputs
	mov.l		(%sp)+,%d0		# restore7 ERRO
# ur otherd dsh	te (SNAN||OPERR||2VFL|tatunsert the corrsrc_d0,-(
	fmovm.x		EXC_FPR1GS(%a6)ve frame for ore infs

	fmovm.x		EXC_FPR2GS(%a6)operand in tore R ANfp1
	fmovm.l		USER_3GS(%a6)al
# src operstatuds

	fmovm.x		EXC_FPR5GS(%a6)%sp)		# save d0ore ateme
	fmovm.x		EXC_FPR4GS(%a6) dbl inputs
	mov.l		(%sp)+,%d0		# resto2e d0(%a6)ov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SRC2%a6) # create exc e exc state (SNAN||OPERR||3e, we irznsert the correct fsave status value int3 the fsrzave frame for the
# corresponding except3on. therz operand in the fsave frame should be t3e origirznal
# src operand.
fu_in_exc_exit:
	mov.3		%d0,-rz(%sp)		# save d0
	bsr.l		funimp_skew		# sk3w sgl orze000,0xe006,0xe004,0xe005
	short		0xe0033e d0

	rzxe001,0xe001

fu_in_exc_unfl:
	mov.w		&0x43%d0
	bra.b		fu_in_exc_exit
fu_in_exc_ovfl:
	4REGS(qet Ew		(tbl_except.b,%pc,%d0.w*2),2+FP_SRC4_FPCR sglve frame for the
# corresponding except4s
	mo sgloperand in the fsave frame should be t4d1/a0 sglal
# src operand.
fu_in_exc_exit:
	mov.4op

	 sgl%sp)		# save d0
	bsr.l		funimp_skew		# sk4hort	 sgl dbl inputs
	mov.l		(%sp)+,%d0		# resto40xe00 sglov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SRC4%d0
	bra.b		fu_in_exc_exit
fu_in_exc_ovfl:
	5-06) # cr yes
	rts					# no

fso_sgl_dnrm_zer1:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clea2:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clea3:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clea4:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clea5:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clea6:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # cleaa6) # create exc slognp1s

	fmovm.x		EXC_F6VFL|(%a0)	),&0xc0	# restore fp0/fp1
	fmovm.l		USE6_FPCrts

fs),%fpcr,%fpsr,oprpiar000,LOCAL_EX(%a0)	#2clear bog		EXC_DREGS(%a6),&0x0303	# restore d06d1/arts

fs
	frestore	FP_(%a0)	
set EXC.w		&0x7fff,5OCAL_EX(%		%a6

	bra.l		_fpsp_done

tbl_except:
	6hortrts

fs dbl inputs
	mov.l		(%sp)+,%d0		# resto6e d0rts

fsov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SRC6%d0
	bra.b		fu_in_exc_exit
fu_in_exc_ovfl:
	7ro:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # cl7ar j-bit
	beq.b		fso_zero		# it's a skewed 7ero
fso_sgl_dnrm:
# here, we count on norm 7ot to alter a0...
	bsr.l		norm			# normaliz7 mantissa
	neg.w		%d0			# -shft amt
	addi.w7	&0x3f81,%d0		# adjust new exponent
	andi.w7	&0x8000,LOCAL_EX(%a0)	# clear old exponent7	or.w		%d0,LOCAL_EXetoxm	# insert new expon8VFL|d0			#),&0xc0	# restore fp0/fp1
	fmovm.l		USE8_FPC3c01,%d),%fpcr,%fpsr,d0			#ient
	andi.w		&0x8002,LOCAL_EX		EXC_DREGS(%a6),&0x0303	# restore d08d1/a3c01,%d
	frestore	FP_d0			#= $7fff
	rts

fso_8bl:
3c01,%d		%a6

	bra.l		_fpsp_done

tbl_except:
	8hort3c01,%d dbl inputs
	mov.l		(%sp)+,%d0		# resto8e d03c01,%dov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SRC8	or.w		%d0,LOCAL_EXtatus

	fmovm.x		EXC_F9VFL|y, t),&0xc0	# restore fp0/fp1
	fmovm.l		USE9_FPCrate ),%fpcr,%fpsr,%fpdler the UNNORM and DEN2RM move		EXC_DREGS(%a6),&0x0303	# restore d09d1/arate 
	frestore	FP_y, t%a6)		# restore src9bl:
rate 		%a6

	bra.l		_fpsp_done

tbl_except:
	9hortrate  dbl inputs
	mov.l		(%sp)+,%d0		# resto9e d0rate ov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SRC9	or.w		%d0,LOCAL_EXatanCMDREG(%a6){&3:&3}aVFL|ct c),&0xc0	# restore fp0/fp1
	fmovm.l		USEa_FPC0ff,U),%fpcr,%fpsr,pi_2codes.
	and.l		&0xff2ov.l		&		EXC_DREGS(%a6),&0x0303	# restore d0ad1/a0ff,U
	frestore	FP_ct c%a6)		# restore srcabl:
0ff,U		%a6

	bra.l		_fpsp_done

tbl_except:
	ahort0ff,U dbl inputs
	mov.l		(%sp)+,%d0		# restoae d00ff,Uov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SRCa%d0
	bra.b		fu_in_exc_exit
fu_in_exc_ovfl:
	bro:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clbar j-bit
	beq.b		fso_zero		# it's a skewed bero
fso_sgl_dnrm:
# here, we count on norm bot to alter a0...
	bsr.l		norm			# normalizb mantissa
	neg.w		%d0			# -shft amt
	addi.wb	&0x3f81,%d0		# adjust new exponent
	andi.wb	&0x8000,LOCAL_EX(%a0)	# clear old exponentbve out doesn't affecsiccodes.
	and.l		&0xcfff00si,USER_FPSR(%a6) # zero exception field

cfmov.ecedve frame for tRSIONSxceptions in order o
	fmeced		EXC_DREGS(%a6),&0x0303	# restore d0cDENOReced an UNNORM! so,sion't make any big sucroutieced		%a6

	bra.l		_fpsp_done

tbl_except:
	c
	moveced dbl inputs
	mov.l		(%sp)+,%d0		# restocw		&0ecedov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SRCcve out doesn't affect cus

	fmovm.x		EXC_Fdfff00ff,e packed move outs from the UNNORM and Ddfmov.l		ve outs.
	bfext of large UNNORM
#	OVFL	:ds
	fmov.pi.b		%d0,&0x3
	beq.w		fu_out_pack
	cmpdDENORM o,&0x7
	beq.w		fuuctio%a6)		# restore srcdroutine
his point what FPSR bits are valid for thid
	mov.w	ction.
# so, since the emulation routinesdw		&0x7fte them anyways, zero exception field.
# fmd%a6) # create exc statCMDREG(%a6){&3:&3}eREGS(%a),&0xc0	# restore fp0/fp1
	fmovm.l		USEe_FPCR(%ve.{b,w,l} out of large UNNORM
#	OVFL	:es
	movm,d}
#	UNFL	: fmove.{s,d,x}
#	DZ	: noneed1/a0-a

	frestore	FP_SRC%a6)		# restore srceop

	unough here)

# determine the highest priorehort		0tion(if any) set by the
# emulation route0xe002,has also been enabled by the user.
	mov.b	eh exceptions, if any, CMDREG(%a6){&3:&3}fSeparat),&0xc0	# restore fp0/fp1
	fmovm.l		USEfNORM move.{b,w,l} out of large UNNORM
#	OVFL	:f%d0
	cm,d}
#	UNFL	: fmove.{s,d,x}
#	DZ	: nonef.b		%d0&0x7
	beq.w		fu_ou%a6)		# restore srcfre at tough here)

# determine the highest priorf instrution(if any) set by the
# emulation routfre-creahas also been enabled by the user.
	mov.b	f mantissa
	neg.w		%d0		CMDREG(%a6){&3:&31OVFL|d0		l?
	beq.b		fsold_ent it to its final rest0,LOCAL&0x0,%fpcr		# zz

fsoit to its final restCAL_EX(,d}
#	UNFL	: fmove.{s,d,x}
#	DZ	: non1in_ex whic##################%a6)		# restore sr1led u whic		%a6

	bra.l		_fpsp_done

tbl_except:
1xact
 whic dbl inputs
	mov.l		(%sp)+,%d0		# rest1
	bts whicov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SR1overxceptions, if anywopy it to its final rere, w# restce.
# here, we're counting on the top oo theEXC_DREk to be the old place-holders
# for fpion. EXC_DREh have already been restored. that wahe orEXC_DRE0x7
	beq.w		fu_ rest= $7fff
	rts

fso1l		%dEXC_DREd stack frame.
	fmovm.x		&0x80,FP_SRC(%aew sgEXC_DREnswer on stack

	fmovm.x		EXC_FPREGS(%are d0EXC_DRE restore fp0/fp1
	fmovm.l		USER_FPCR(%a6)(%a6) # create exc stenpy it to its final rePREGSZE+EXCce.
# here, we're counting on the top oR_FPCCAL_SIZk to be the old place-holders
# for fpgs
	mCAL_SIZh have already been restored. that wa-d1/aCAL_SIZ0x7
	beq.w		fu_E+EXC%sp),LOCAL_SIZE+2+ op

CAL_SIZd stack frame.
	fmovm.x		&0x80,FP_SRC(%ashortCAL_SIZnswer on stack

	fmovm.x		EXC_FPREGS(%a,0xe0CAL_SIZ restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%d0
	bra.b		fu_in_exc_exit
fu_in_exc_ovfl:
1mov.
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # c1inpubut the result
# was exact, then a branch ro
fso_sgl_dnrm:
# here, we count on norm1nan,_EXCEPT(%a6) # was overflow set?
	beq.w		fmantissa
	neg.w		%d0			# -shft amt
	addi.1r to2_bit,FPCR_ENABLE(%a6) # was inexact enabl&0x8000,LOCAL_EX(%a0)	# clear old exponen1,src fmt
	cmpi.b		%ponendestinations with  fmt (%a0l?
	beq.b		fso__dz current control r1&0x15ing tus exponent
	rts

fso_infnan:
	andi.b1l			#ing th have already been restored. that waEX(%aing ta0)	# make exponen destinations with ,%d0	ing td stack frame.
	fmovm.x		&0x80,FP_SRC(%a == $ing tnswer on stack

	fmovm.x		EXC_FPREGS(%aw		%ding t restore fp0/fp1
	fmovm.l		USER_FPCR(%a6)o_infnan		# yes
	rtpone10tack frames are siero:****10traced. Since the stack frames are siear *****
# the "current" PC
# from FPIAR and putzero*****
#h have already been restored. that wanot *****
#a0)	# make expone10 destinations with 		&0*****
#d stack frame.
	fmovm.x		&0x80,FP_SRC(%ae ma*****
#nswer on stack

	fmovm.x		EXC_FPREGS(%a		&0*****
# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6)
	or.w		%d0,LOCAL_EX(%astack frames are sient
	rts2traced. Since the stack frames are si	# clear2    PC	*
#		*****************	*********	&0x7f,L2		*      SR	*	*      SR	*
#		*********f,LOCAL_2****************
#2 destinations with dbl:
	mo20x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l		andi.w	2ce

# an exception occurred and that exc3c00		# 2 enabled.
fu_out_exc:
	subi.l		&24,%d0			## yes
	cmpi.w		%d0,&0x43ff		# no; is |exp| =1 $43ff?
	beq.b		fso_infnan		# yes
	rts					1 no

fso_dbl_dnrm_zero:
	andi.l		&0x7ffffff1,LOCAL_HI(%a0) # clear j-bit
	bne.b		fso_db1_dnrm		# it's a skewed denorm
	tst.l		LOCAL1LO(%a0)		# is it a zero?
	beq.b		fso_zero		1 yes
fso_dbl_dnrm:
# here, we count on norm1not to alter a0...
	bsr.l		norm			# normali1e mantissa
	neg.w		c0	# restore fp0/fp1
	fmov		&0xabps()tore fp0/fp1
	fmovm.l		USER_FPCR(%a6)000,Ltrl ve frame for the
# corresponding excep1LOCALtrl operand in the fsave frame should be 1#####trl al
# src operand.
fu_in_exc_exit:
	mov1#####trl %sp)		# save d0
	bsr.l		funimp_skew		# s1e exctrl nswer on stack

	fmovm.x		EXC_FPREGS(%afout(trl  restore fp0/fp1
	fmovm.l		USER_FPCR(%a6)ch exceptions, if ancosus

	fmovm.x		EXC_1 Sepax030ce.
# here, we're counting on the top oENORM

	move frame for 're lace-holders
# for fp,%d0


	moh have already been restored. that wai.b		

	mo
	frestore	FP_x030 destinations with ure a

	mod stack frame.
	fmovm.x		&0x80,FP_SRC(%as ins

	monswer on stack

	fmovm.x		EXC_FPREGS(%a re-c

	mo restore fp0/fp1
	fmovm.l		USER_FPCR(%a6)ove out doesn't affrr:
	fmovm.x		EXC_FPREGS(%ffff0neg regs
	movm.l		EXC_DREGS(%a6),&0x0303	# 	fmovx30d0-d1/a0-a1

	mov.w		&0x30d8,EXC_VOFF(%ags
	fx30dor offset = 0xd8
	mov.w		&0xe006,2+FP_ DENOx30d
	frestore	FP_SRC(%a6)

	unlk		%a6


	bbroutx30dal_snan

fu_operr:
	fmovm.x		EXC_FPREGS(%.
	mox30dnswer on stack

	fmovm.x		EXC_FPREGS(%a.w		&x30d restore fp0/fp1
	fmovm.l		USER_FPCR(%a6)		# it's a DENORM

	lea		FP_SRC(%a6),%a0
	bs1.l		unnorm_fix		# yes; fix it

	mov.b		%d0,1TAG(%a6)

	bra.b		fu_out_cont
fu_out_denorm1
	mov.b		&DENORM,STAG(%a6)
fu_out_cont:

	c1r.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch1rnd mode/prec

	lea		FP_SRC(%a6),%a0		# pas1 ptr to src operand

	mov.l		(%a6),EXC_A6(%16)	# in case a6 changes
	bsr.l		fout			# ca1l fmove out routine

cos3	# restore d0-d1/ of pr****e.
# here, we're pistack frames are siERR	: ****	mov.l		EXC_A6(%a6),(%a6)	# in case a1 fmove****h have already been restored. that wa
#	INE****s using pre-deccos destinations with ravel ****d stack frame.
	fmovm.x		&0x80,FP_SRC(%aity ex****nswer on stack

	fmovm.x		EXC_FPREGS(%aine th**** restore fp0/fp1
	fmovm.l		USER_FPCR(%a6)	FPCR_ENABLE(%a6),%d0	# restore fp0/fp1
	abledl		USER_FPCR(%a6),%fpcounting on the top oout_dtrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# chanre d0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(instr vector offset = 0cc
	mov.w		&0xe003,2+Fent a%a6)

	frestore	FP_SRC(%a6)		# restore EXOP
dated		%a6

	bra.l		_real_unfl

fu_unfl_s:
	cmpinter COND_FLG(%a6),&mda7_flg # was the <ea> mode -e. if it was used frgetexpEGS(%a6),&0x0303	#hen w,FP_DS),&0xc0	# restore fp0/fp1
	fmovm.l		US1
	btsack

	fregs
	movm.l		EXC_DREGS(%a6),&0x0303	#
	movack

	fh have already been restored. that wa	%a0,ack

	f
	frestore	FP_,FP_DS%sp),LOCAL_SIZE+2+REGS(ack

	fd stack frame.
	fmovm.x		&0x80,FP_SRC(%aFPCR(ack

	fnswer on stack

	fmovm.x		EXC_FPREGS(%a
	movack

	f restore fp0/fp1
	fmovm.l		USER_FPCR(%a6)0-a1

	unlk		%a6

	bgetmst		&0x7,(%sp)		# i1 trac),LOCAfmovm.x		EXC_FPREGS(%a6),&0xc0	# restor	_fps	mov.l	regs
	movm.l		EXC_DREGS(%a6),&0x0303	#the s	mov.l	h have already been restored. that waov.x 	mov.l	tore d0-d1/a0-a1
m If SIZE+EXC_SR-0xc(%sFF(%a6v.l	d stack frame.
	fmovm.x		&0x80,FP_SRC(%acont
SRC_EX(nswer on stack

	fmovm.x		EXC_FPREGS(%an fp0SRC_EX( restore fp0/fp1
	fmovm.l		USER_FPCR(%a6)stack until we can e exc state (SNAN||OPERR|2OVFL|div regs
	movm.l		EXC_DREGS(%a6),&0x0303	#2f theeal_ve frame for the
# corresponding excep20/fp1eal_operand in the fsave frame should be 2in_exeal_al
# src operand.
fu_in_exc_exit:
	mov2led ueal_%sp)		# save d0
	bsr.l		funimp_skew		# s2xact
eal_ dbl inputs
	mov.l		(%sp)+,%d0		# rest2
	btseal_ov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SR2,%fpcr,%fpsr,%fpiar mod_sm_wri = 0xc4
	mov.w		re, wmo####6

	bra.l		_)

	ffp0/fp1
	fmovm.l		US2o thetore),%fpcr,%fpsr,)

	ffso_infnan:
	andi.b2ion. tore		EXC_DREGS(%op_s,&0x0303	# restore d2he ortore
	frestore	FP_)

	fdnstore	FP_SRC(%a6)		# d unds th EXOP

	unlk		###
p_done

tbl_except:
2ew sgu_in0303	# restore d0-d1/a0-a1

	mov.w		&0xre d0u_inVOFF(%a6)	# vector offset = 0xc4
	mov.w		(%a6) # create exc tor offset = 0xc4
	mov.w		PREGS		# FF(%a6)	# vector offset = 0xc4
	mov.w		R_FPC		# movm.x		&0x40,FP_SRC(%a6)	# save EXOP tgs
	m		# ck

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# -d1/a		# p0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,% op

		# _pack:


# I'm P_SRC(%a6),%a0		# pass ptr t),&0dbits are valid for this instruction.
# so,0xe0		%dVOFF(%a6)	# vector offset = 0xc4
	mov.w		,%d0
	bra.b		fu_in_exc_exit
fu_in_exc_ovfl:
2mov.wmuclas(%a6)	# vector offset = 0xc4
	mov.w		inputes tmovm.x		&0x40,FP_SRC(%a6)	# save EXOP td a ses tck

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# nan, es tp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%r to es tbsr.l		set_tag_x		# set src optype tag

	 numbes t0303	# restore d0-d1/a0-a1

	mov.w		&0xbfextes tVOFF(%a6)	# vector offset = 0xc4
	mov.w		,src fmt
	cmpi.b		%d0,&0x11		# is class = 2 2 fmt =gleal_unfl

# fmove in and out enter here.
fu&0x15	e
	cmpmovm.x		&0x40,FP_SRC(%a6)	# save EXOP tl			# e
	cmpck

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# EX(%a0e
	cmpp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%,%d0		e
	cmpbsr.l		set_tag_x		# set src optype tag

	 == $3e
	cmp0303	# restore d0-d1/a0-a1

	mov.w		&0xw		%d0e
	cmpVOFF(%a6)	# vector offset = 0xc4
	mov.w		x2 *  0x024	*
#		***rem	frestore	FP_SRC(%a6)		*****rem),&0xc0	# restor	mov
	bra.l		_real_inex

  PC	l*4)),%fpcr,%fpsr,%	mov####################*****l*4)################
#####################*****l*4)
	frestore	FP_ Exce#####################		&0xl*4)_pack:


# I'm not sure at this point wha	_real*4)0303	# restore d0-d1/a0-a1

	mov.w		&0xeptiol*4)VOFF(%a6)	# vector offset = 0xc4
	mov.w		
	or.w		%d0,LOCAL_EXscale	frestore	FP_SRC(% peraent
	tion e EXOP

	unlk		tion rofp0/fp1
	fmovm.llso beeFPCRabled),%fpcr,%fpsr,tion ro####################	&0x7tch ex################
#####################f,LOCtch ex
	frestore	FP_Sion ro################lso beep

	abled_pack:


# I'm not sure at this point wha	andich ext0303	# restore d0-d1/a0-a1

	mov.w		&0x3c00	ch extVOFF(%a6)	# vector offset = 0xc4
	mov.w		# yes
	cmpi.w		%d0,&0x43ff		# no; is |exp| =2 $43%d1 es the monadic and dyadic operations
# at t no
_fpreg		movm.x		&0x40,FP_SRC(%a6)	# save EXOP t,LOC_fpreg		ck

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# _dnr_fpreg		p0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,% yes_fpreg		bsr.l		set_tag_x		# set src optype tag

	LO(%_fpreg		0303	# restore d0-d1/a0-a1

	mov.w		&0xnot _fpreg		VOFF(%a6)	# vector offset = 0xc4
	mov.w		m.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # res2		&0xsubthe monadic and dyadic operations
# at t000,Lame
movm.x		&0x40,FP_SRC(%a6)	# save EXOP tLOCALame
ck

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# #####ame
p0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%#####ame
bsr.l		set_tag_x		# set src optype tag

	e excame
0303	# restore d0-d1/a0-a1

	mov.w		&0xfout(ame
VOFF(%a6)	# vector offset = 0xc4
	mov.w		ch exceptions, if a	unlk		%a6			# unravel sta Sep
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # c2ENORtore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcro
fso_sgl_dnrm:
# here, we count on norm2i.b	C_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1mantissa
	neg.w		%d0			# -shft amt
	addi.2ure the stack frame "up". we don't really care&0x8000,LOCAL_EX(%a0)	# clear old exponen2EGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov2ffffsp)
	mov.l		0x0(%sp),0xc(%sp)
	add.l		&0	fmo	fu_trace_p		# yes

	bra.l		_fpsp_done		# EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0- DENa6),%d0	# keep only ones enabled & set
	bft the stack frame "up". we don't really cabrouexception
	bne.b		fu_in_exc_p		# at least (%sp)
	mov.l		0x0(%sp),0xc(%sp)
	add.l		&0		# it's a DENORM

	lea		FP_SRC(%a6),%a0
	bs2.l		unnorm_fix		# yes; fix it

	mov.b		%d0,2TAG(%a6)

	bra.b		fu_out_cont
fu_out_denorm2
	mov.b		&DENORM,STAG(%a6)
fu_out_cont:

	c2r.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch2rnd mode/prec

	lea		FP_SRC(%a6),%a0		# pas2 ptr to src operand

	mov.l		(%a6),EXC_A6(%26)	# in case a6 changes
	bsr.l		fout			# ca2l fmove out routineNABLE(%a6) # was inexact e of ex2_bit,FPCR_ENABLE(%a6) # was inexact eERR	exc_ovfl_p	# do _real_inex() now

#
# An ee proper fp reg (unless the op is fcmp or 
#	Ied:
#
#	shift enabled exception field intoSR_EXCEPT(%a6) # was overflow set?
	beq.w	ravex_enabled && OVFL && ovfl_disabled) ||
#	 inex2_bit,FPCR_ENABLE(%a6) # was inexact e	FPCR_ENABLE(%a6),%NABLE(%a6) # was inexact eableex2_bit,FPCR_ENABLE(%a6) # was inexact eout_lse
#		 * there will be no other way to pae proper fp reg (unless the op is fcmp or instreal_inex();
#	} else {
#		restore exc staSR_EXCEPT(%a6) # was overflow set?
	beq.w	ent e FPU;
#	}
#
fu_in_exc_p:
	subi.l		&24,%d0inex2_bit,FPCR_ENABLE(%a6) # was inexact ee. if it was used fNABLE(%a6) # was inexact ehen ex2_bit,FPCR_ENABLE(%a6) # was inexact e
	btnexact
	btst		&unfl_bit,FPSR_EXCEPT(%a6) #e proper fp reg (unless the op is fcmp or 	%a0xc_unfl_p	# yes
	btst		&ovfl_bit,FPSR_EXCESR_EXCEPT(%a6) # was overflow set?
	beq.w	REGS	fu_in_exc_ovfl_p	# yes

# here, we insertinex2_bit,FPCR_ENABLE(%a6) # was inexact e0-a1

	unlk		%a6

	NABLE(%a6) # was inexact e traex2_bit,FPCR_ENABLE(%a6) # was inexact e	_fpiginal
# src operand.
# as a reminder for e proper fp reg (unless the op is fcmp or ov.xg in fsave the
# "non-skewed" operand for SR_EXCEPT(%a6) # was overflow set?
	beq.w	E+FPs.
# this is INCORRECT for enabled SNAN whinex2_bit,FPCR_ENABLE(%a6) # was inexact estack until we can c******CMDREG(%a6){&3:&33OVFL|******* by the user.
	*****ze.w		fu_in_exc_exitFPCR(%trl regs
	movm.l	.l		EXCponent
	or.w		%d030/fp1user a7		EXC_DREGS(%user a,&0x0303	# restore d3in_exuser a7
	frestore	FP_SRCcc
	.w*2),2+FP_SRC(%a6)p

	un%a6)

	frestore	FP_Sfmovm.p_done

tbl_except:
3xact
fmovm.l dbl inputs
	mov.l		(%sp)+,%d0		# rest3
	btsfmovm.lov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SR3,%fpcr,%fpsr,%fpiar or?
	bne.w		fu_in_exc_exre, wp	# supervisor

	mov.l		EXC_A7(%a6),%a0		# upo theuser a7
	mov.l		%a0,%usp

fu_in_exc_exit_contion. mov.w		(tbl_except_p.b,%pc,%d0.w*2),2+FP_SRC(%ahe orfmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/###
ffmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restoew sgrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restre d00-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore src(%a6) # create exc stat
	bne.w		fu_in_exc_exPREGS(%a supervisor

	mov.l		EXC_A7(%a6),%a0		# upR_FPCR(%r a7
	mov.l		%a0,%usp

fu_in_exc_exit_contgs
	movm.w		(tbl_except_p.b,%pc,%d0.w*2),2+FP_SRC(%a-d1/a0-avm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/ op

	unvm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restoshort		0regs
	movm.l		EXC_DREGS(%a6),&0x0303	# rest,0xe002,1/a0-a1

	frestore	FP_SRC(%a6)		# restore src,%d0
	bra.b		fu_in_tst		&0x7,(%sp)		# is tramov.w.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	input.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restod a srl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restonan, -d1/a0-a1

	frestore	FP_SRC(%a6)		# restore sr to 

	unlk		%a6			# unravel stack frame

# shift stac numbme "up". who cares about <ea> field.
	mov.l		0bfextp),0x10(%sp)
	mov.l		0x0(%sp),0xc(%sp)
	add.l		&he "Unimplemented Dad0.w*2),2+FP_SRC(%a6)

	 fmt =x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	&0x15	l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restol			# l regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restoEX(%a0d1/a0-a1

	frestore	FP_SRC(%a6)		# restore s,%d0		
	unlk		%a6			# unravel stack frame

# shift stac == $3e "up". who cares about <ea> field.
	mov.l		0w		%d0),0x10(%sp)
	mov.l		0x0(%sp),0xc(%sp)
	add.l		&x2 *  0x024	*
#		***d0.w*2),2+FP_SRC(%a6)

	******	*****************
#		* 0x2 *	0x0dc	*	* 0x2   PC	024	*
#		*****************	*****************
*****    Next	*	*     Next	*
#		*      PC	*	*      P*****		*****************	*****************
#		*   		&0x	*	*      SR	*
#		*****************	**************	_reau_trace_p:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%eptio,0x8(%sp)

	bra.l		_real_trace

################) set by the
# emulat0.w*2),2+FP_SRC(%a6)

	een enx		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
		# fetl		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restoome arl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# resto not sd1/a0-a1

	frestore	FP_SRC(%a6)		# restore s# fetc
	unlk		%a6			# unravel stack frame

# shift stac 3-5
	e "up". who cares about <ea> field.
	mov.l		0	beq.b),0x10(%sp)
	mov.l		0x0(%sp),0xc(%sp)
	add.l		&# yes
	cmpi.w		%d0,&0x0,%fpsr

	bfextu		EXC_tore_fx		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	t		&0xl		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restou_in_el regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restoa0		# d1/a0-a1

	frestore	FP_SRC(%a6)		# restore sont_p:
	unlk		%a6			# unravel stack frame

# shift stacfp1
	fe "up". who cares about <ea> field.
	mov.l		0estore),0x10(%sp)
	mov.l		0x0(%sp),0xc(%sp)
	add.l		&e mantissa
mory			#
#			_word
_dmem_read_imum e	mov.l	erands		0,EXC_OPWORDs uncherands	SER_FP0x4
n bit

seff_b(%a6	%d0,-(%sM?
	bne.h that if the rresult

t extension

	andi.l		&0x00in fsave f_80000000_000006

	bra.####*******************************	#
#	On the 060, if an FPBLE-0x80+_off_dww,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
ite_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TAe it doesn't
# stack an ea. hset by the
E-0x80 an a7' fo least one setRS,		12

setad dst operaet	_o point
	b 0,2	#
#	Underflow6)	# is operet	_ol		load_fpn2		# lo4008#									#
#	# sashouexception present" fptr to 	USER_FPEAance and sup09428 - loa0x0000,&0x0303	FSLWset ovfl_bit,	tr to dstocessoata tSP_TABLE-0x8ovm.x		EXC_FPREGS####_epea.	&0x7,2,		EXC_Fucti0xd dst operawas 		# is traTMset o.w		fu_out_e		# 		&2###########
	mov.l for the operation. This default result is	#
# then stored in either the FP re&0x0_fps"dowl		su 0 o FP repoints**** insiz#####pRC(%a6ack fradme "dow".
fu_out_exit_s_p:
	bt_pack:&mda7_bit,SPCOND_FLGcosme "down".
fu_out_exi

fus_p:
	btst		&mda7_bit,SPCOND_FLGcc
	) # was ea mode -(a7,&0xc0	# resfu_out_exit_cont_p	#D_FLG(%afmovm.x	".
fu_out_exit_s_p If ,&0xc0	# restore fp0/fp1
	fmovm.l		EXC_D_FPCR%a6),&0x0303	# restore d0-d1/a0-a1fu_out_exit_cont_p	,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	f '01	a0 =ow?
# wel**********************it,SPCOND_F	dy throuptionalAG
set_x()un_bit,		0x1
set miap2_done		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERw, cofpy thframXov.l	fmovXion words
	LOCAL_SIZE+E   LE-0&0x0,%f(X)-(%sp)
_TABLE
	mov.l		LOp)
	mov.l		060F=_SIZE+FP_DST_a6)
#$#	mov.l		FP_CCURACYstoreMONOTONICITY		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovw, co	cmp FPU me		1+EXC_ res0x8(in 1 ulore  64 DENORM, or Uk is i.ehe defd0{&24:0.500&8},%dto 5######gword	#

	bfffo		%subsequ
	rtd		&0	l		LOXC_VOFF,vm.x	FPREGS(%a6&0x4

	bfffo		%provab:
	monotonicZE+EXa6),ception occurredc),%d0
	pea.l		(_060F#########################################

	btst		&0x5,EXC_CMDREG(%a6)	# issp)
	mov.l		SINstoreCOS)

	add.l		&1,1+EXw		f, or vokd			o osAdjN := 0e	#
#	If OPER%a6)
	bne.b	1
	pea.l		(_060FPSP2,1+EX|X| >= 15Pi(%spusp
< 2**(-40), gout_e7ask
set inx2a_mask,	3. D_TABpose XmovmX = N(Pi/2) + from re |r| <= Pi/4. LeIZE+EXCkfu_sre f 4,he di

M6rticular, ,-(a0,1,2 tha3		(tbl_uOput
	bse k conke.b	k +)
	bnne.	#
#	If the except4,1+EXLE-0xthats_p:
	cm6sible fu_inex_p2
	beq.5. (LE-0xodd) Set je.b	(k-1)/2, sgnovm.x-1)**jask
set iRaccesssgn*x8(%r)Current	fmovm.rest SOFxim%sp)
	moalk		%a	that polynom		0xin r, 1 the*r*(B1+s*(B2+ ... + s*B8))isabl		 neer*rsible on pacExiteal_inex

#############	6ion framthatas.
	fmovm.kC_FPREGS(%a6),&0xc0 re fp0/fp1
framr)
	mov	instruave st(%a6),%fpcr,%fpsr,%fpime wre ctrl regs
	mot wherrl		EXs*(AEGS(%A6),&0x0303	A7rest0-d1/a0-aov.w		&0x30d8,EXC_VOFF(%a6)	# vec7%a0,%usp
	 1s_p:
	cm9sible on packed move o	8. (|X|<fu_snan_)
	btst		&0x5,EXC_SR(_accessX;ult wher#
#	If OP		LOCAL_1result where the excep9. ve the excX conXe.b	Xt to 2Pi#	_ropclasssnan
 was,lt whergo bult toy is e.	#
#	If the exceptSINerr_p

fu_ssnan_p:
	btusp
	bra.w		fu_snan

fu_snan_s_p:
	cm resu%a6),%a0
	mov.l		%ada7_flg
	bne.w		fu_snan

# the instruction was "fmove.p fpn,-(a7)" from supervisor mode.
# the strategy is to moND_FLG(%a6),&man store the default5result where the excep4ion frame was.
	fm1ovm.x		EXC_FPj2e.b	j1 (EOR)on f" fr2), iy excep	j1RM,Dlus
#	-  3.	8(%sp)	l.s.b.ion.erantore d0gnw		fu_),&0xc1FPREG_oper"fmove.2 instructSIN	LOC=/fp11 *ER_FPCR(u_operrde.
# the2save stCurrentore d0tion ftore d0 byteUSER_
fu_orestoc opINCLUveC(%a6)		e ctrl reg
# carl		L	mov.l		FP. 	&0xult where the exception framt = 0xd0
	mw		fu		&0xe00 was "fmove.pupervisor mode.
# the str 12 bytes.  move the ex1
	fmovm.l		US"down" 12 bytes. then, we
# can store the default result where the exception frame was.
	fmovm.x		EXC_FPREGS(%a6),6SR-0xc(%sp)
	mov.l		8P_DST_EX(%sp),LOCAL_SIZ7LOCAL_SIZE+2+EXC_ mode.
# Xis to move the1fmovm.		(tbl_unsupp.l,%pc8 we copy the default result to its proper location
	mov.l		LOCAL_SIZE+super_bit,		0x1
set mia7_bit,		0x2
set mda7_bit,		0x3
set immed_bit,		0x7

######################SINA7:	l		_dm0xBD6AAA77,0xCCC994F5EA-0x6(%sp)

# n3DE61209is iAAE8DA1EA-0x5(%sp)

# noE5AE645me iA118AE4EA-0x4esult to itEC71DE3,0xA534153
	mov.3(%sp)

# noF2A01Ahe F1A018B5er lSPCOND_he F	mov.l		EA-0x2esult to itFFC_HI,	0x8xc(%sp)E+0xc(%59AFLOCAL_SIZE+FP_DS1DST_HI(%sp),FC.l		LOCAr

fu_iperr

fu_9sp)
	mov.l		

COSB8esult to its2AC4DZE+0D6011EE3SR(%ac(%sp)

# nowA9396LOCA9F45AC19SR(%aresult to itE21EEDsp)
	612C972SR(%al		LOCAL_SIZE927E4LOCAB79D9FCFSR(%aSIZE+0x4(%sp)FOCAL_SIZE+0xD42.l			mov.l		LOCAL_SIZE+FR(%a_DST_HI(%sp),F5.l		LOCB60strat "fmB61D43

	a-(a7)" from sT_LO(%sp),LOCAAal_operr

fu_inex_p2:
	B5Erom sp


	bra.l		_x5,EXC_S####		INARG# operanre the eXion framet inf		XDCARE,X+2 was.
	fFRAC,X+ _re_FPRERPRIMEion framefp1
	fSovm.l		USER_ov.l_FPREPOSNEGng as thfpiar #TWOTO63e ctrl rfpiar #ENDFLAGg as t0xc0	# rINT	# restofpiar #ADJNg as t3lse
#		 * there will be no other way to pass ix_on

funf&0x0
&0x0nance and su,&0x3 an excep _fpsSET &0x3 TO ng}() - reSINBGN_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRcos
		LOnance and s1tatus

	frestore	FP_SRC(%a6)		#ow exception.
# But, whether bogus or not, i0xe0rc ope:
#--SAVE exce, FP1. CHECK IF_SIZEIS TOO SMALL OR LARGEis presexbmask00 or 2 	# LOAtionPUTle	#
#	unnorm_f		0x407e machin%d0
	pat Xo

#"COMPACTIFY" Xet neg_bmasknow,ce().		ustac%d0		hi_realin how th4,LOCAL_SIZE+0imum ehi(mann thn
#####0x7Fl		LOC	bra.l		_tri			# flce the in an
sa6

FD7 SPC
	rtd		usp
	brfu_snan_ceptgow
seSOKinableits t() - "SINSMu		EX excepd0
	p####ery smur?
	a.l	btst		AL_SIZE+0xc4004BC7Eadd.l		&LOC< 15 PI*******e src MAIN	_real_inex

####REDUCEXout" _fps##############larr ft#--THIS(%sp)HE USUAL CASE, locatio we'r. make E ARGUemul nto aer reIS DONE BY Ta NO LOOK UP.XC_PsourINEX1) &unnorm_f%P_TABLEul.d	movmBYPI(%pcresul_EXTX*2/PI########PITBLerat00nimp_ska_EXTis as OF N*PIC_FPN = -32,...,3
	mo: 060FPSP e1,INt shouper CONVERT)		#INTEGE
# ul_unfl_&0x1		# L_SIZE+0# no underfion.N
	aslallou_DSTinableN *= 16SP #
####	# saunfl000)bl_sable= a.l		(N*1 to meA1re that ADDRESS,%d0 # eBY2
#,&0xWHICHmat IN TWO PIECES Y1 & Y2
#	_ubsp)

# 1t DS we copX-Y6(%a# stsip sign	beq.b		fLOCAL_R = (unim)-Y2_EA-0CONTxc(%sstorinu P1.00 				nto an o makGET N+(%a6)AND SEE+EXC modR)IZE+ movR)mat NEEDEDnimp_skew_sgl		# yes
SP #
####atus

	frL_SIZE+0
	addi.D0mat ODDIZE+EVEN max ext &ify, s
	rtD0 WA)
	lsrIFhi(man)
NEGATIVE###############*******
#COSPOLYo makLET J BEthat LEAST SIG. BIT,%d0D0, w hiSGne.b	"fmoveJrce operN WE RETURN	SGN*w		&0x.a6)	80,FP_Smat AL_SUlatiBY makR' + R'*Srest + S(A2nimp_s3nimp_s4 ),&0x030Sr

	)), WHEREewed" e=C_EX(R, S=R*R. e surCANman)REWRITTEN ASewed" exponent
 [A1+T(A3%d0	5+Tr

	] + [p_sk%d0	4+TA6))]) makl:
	m T=S*Srce oNOTn)
	AT A3 THROUGH A7 ARE _EXCEtion DOU as PRECISIONdbl_noILEw		FtiveA2_skewt

	tst.b-EXTENDED FORMATs theinse	#
# _fpsp_d0-d1cad dst operand ffp2/fp3A-0xc(%sp)
 to its proper Xmat !

	uulsually makes	add.FPman)
Ser
	cmpid#####t,		mp_skew3a6),%a0		# pas6 ptr to sier
	cmpiually makes it.
#
	 clear1skew
fea		F	FP_SR
,%d1			# shift x8(%sp)
	mov SPCOND_FLG1 DENORdi.w		&0x8000,FP_SRCl foSIGN POSIer r
	eLV+2			# s sign
	clr.l		%NOW R'P_SRC_Eer
	c
	bsr.l		dnrm_3p			TA7d0			# set sign
fsine wTA is p #
#		# pas5 ptr to srp			trip sRC_HI(%a6)	# s4x3c01,%d1	ov.w	imp_s%d0			# set sign
fss_dbl_strip sinorm_done:
	bset		&0x7,Funimp_skSRC_HI(%a6)	# s3t j-bit
	mov.w		# strip siRC_HI(%x	# pas2# insert new ex		funimp_sk%d0			# set si3nrm_lp			d0		# strip sig%d0			# set simakesnew eq.b		funimp_skebal		_mem_writ1nimp_skew
fv.w	,%d0		# strip sigd0			# set.

To ths
	lea	onenSRC_HI(% clear2mov.l		0xff,%d0		# strip sign+eq.b		funimp_skew%d0			# set sign
fs	add.w		&0')-R'FPCR_ENABLE(

set Da6

callout" for t exponent withias
set ption sted inexact b#				LOC-0x80onal##########		%d1
	rts

###l 3):ND i -tag it asame	#
#	- The inex
e thinxtionnew hi(man)
	andi.w		&0x8000,FP_SRC_EX(%a6)	# clear old exponent
	ori.w		&0x3f8,%d0		RC_EX(,%d0		# fiert new "skewed6)	#+ S'_DREnimp_Bkew_sgB_not:
Brts

funimp# rew_dbl:
	mov.w%a6),ative
'=_EX(%%d0	# fetch DENORM exponent
	a#
#	system[B,%d0B	# sB5+TBsign
	beq.B		fuB4long6+T*****w_dbl_not
	cmpi.w		%d0,&0x3c00
	B4t.b		funiB8_skew_dbl_not

	tst.b		FP_SRC_EX(%a6)		# B2ke "iB3ernal format"
	smi.b		0x2+FP_S, B	FP_S-1/2er stivee thatR (0x00_dbl_noAS****G.b		FP_SRC_E.rom 6)
	mov.w		%d0,FP_SRC_EX(%a6)	# insert exponent w# clear g,r,s
	lea		FP_SRC(%a6),%a0		#R(%a6x3c01,%d1		m FP regfile	s ptr to src# pass denorm threshold
	bsr.l		dnrm_lp			# denorm it with cleared sign
	clr.l		%Sit
	mov.w		&0x3c00,%d0		# new exponent
	tst.b		0x2+FP_SRC(%a6)		# is sign set?
	beqnorm_done:
	bset		&0x7,FB8#

			fss_dbl_denorm_done	# no
	bSet		&15Sc00,%d0		# new exponent
	d0			# set sign
fss_dbl_B7
t
set DB0xc(% SPCONL_SIZE+0(man)
6)	#is si#
#	ponent

set E restor(%a6)	# _HI(%a6)	bra.# insert new e			#
# for Trace enaet j-bit
	mov.wread iccess() - "callout" for ad			#
#	
funimp_skew_dbl_not:
	rtread in" for Trace ena	# insert new egword			#
#	##########om su- emulate dynam) - read inovm instruction		#
#	fmovm_word			#
#	fEXC_SR(%a6)
	beq.l		_dmem() - read ins###########on fr		#
# INPUT *** longword			#
#	f for Trs		#
# %a6)
	mov.l		0x_long() - read insrand from memo#####******on longword			#
#	fi##############******rand from m		%d1
	rts
******************************************	#
#	_fpsp_effadd(): 060FPSP entry point for FP "Unimplementes # restorass 0 or 2 resddress" exception.			#
#									#
#	This handlered() - fetch packed operand from memory			#
#			ne		e: BigIZE+S####?er sEXC_EA(>ra.wI,
	orUSn)
	anGENERALands that the
# forrce oEXC_EA(

fu_snan_s_i.w		& XIZE+1s theBORS#################3FCAL_SI%sp
 DAMAinto an _EA-0SMce it does6),%d1	# fetcov.l		%d1,FP_SRC_HgffingCOSTINert ),&0x0303operand type%pc, either opio srhion occurre(%a6)	%d0,-(%sion t NORM or_pack:swe
# er
set ERAP_DSTOa	0xac,%d##
# XDEF **SINexte:
#	DZ	: fdiv
#	00,GS(%a60x3fff		JUST donngleer
	cmpi.b		fadd(): 060FPSP entry point for FP "Unimple	/*
#		 FMOV_OPL_SIZE+0 The systeis MO	movpass den
#			effective address" exception.			#
#									#
#	This 

	fm_SR(%	#
# (m_reads- "callout" forr 2 reLOCAL_1.0s changed to an FPU disabled stack frame	#
#	If Trace exceptio# new instruction s The system stack is changed to a Trace excpeption stack frame	#
#	Else: (normal case)						#
#	xe001,2+FP_SRC(%dc(%sprand

	movFOR ExceptALIZED Xa6)	#ervisore thext####rand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOsp_do(%a6),%a6	n enabled exc has beespone operand to the instruction specified by is stored to the destination register and	#
# the handler exim_read_word
_&0x0,%fa6)	#L_SIZ#
#	SRC(%a6)		#4ance and s4tatus

	frA-0xc(%sp)

# now, we copy the default result to its pro_cont_p:
	fmOCAL_SICAL_SIZE+FP_DST_HI(x8(%sp)
	mov.l		LOCAL_SIZE+FAL_SIZE+FP.l		**************	#
sp)

	add.&LOCAL_SIZE-0x8,%sp


	braCOK OTHx

####CSM

fmovm##############################
# if we're stuffing Csourand back into an btstake sure that for single or double source operands that the
# format stuffed is as weird as teptiodware usually makes iit.
#
	global		funimp_skew
funimp_skew:
	bfextu		EXC_EXTWORD(%a6){&3:&3},%d0 # extract src specifier
	cmpi.b		%d0,&0x1		# was src sgl?
	beq.b		funimp_skew_sgl		# yes
 was src dbl?
	p_skew_dbl		# yes
	rtX(%a6),%d0	# fetch, donY1,,%d00		# strip sign
	beq.b		funimp_skew_sgl_not
	cmpi.w			FP_SRCx3f80
	bgt.b		fuCmp_skew_sgl_not
	neg.wXC_CMDREG(%pending	#g instruction	#
# was 
	mov.w		&0x3c0#############it
	bset< 0d1			rmat ODD%sp


w		N	%d0,
SNODD pendREGISTERS p)
	D SO FAR:SRC_EA0		LO2err exceptio0-d14_EX(%a6)	# insert envert packed datmovm.ling NANd_fpn2() - load dst operand f,&0x*!

	unlk		# pass ptr to sl		0x8enormP regfile			#
#	tbl_u*****fmov_fpn2() - load ch DENdenorm_done:
	bDST_EX(%a6)ccessregisters2 the	#
# excepti	# sav2one().								se t,%d0		# new exponent2ss error exceore the "FPU disabled" excepRC_HI(%a6)	# sbled excepl		0x86+see ifr Trace enas ptr to sed, it7+. Sincst be checked to	#
# s(eck th)ss error ex2(). Else, a noRS,		12

set DSTunsup(%a0),FP_DST_EX(%a6)
 set,	)it
	mov.w		&0x3c00,%d0		# new exponent
	nce and supllout" fo) - "callout"s error exce) - "callout" for Trace	# set j-bit
	l		0x85+an 8 word "disabled bitbled exception		#
exception 
# then we must create an 
#									 normal exit is madword excation work,p(): 060 cleared),%fpc on the	#
# new stac	# insert l		0x84#			lout" returns  error exce
# we must covm_dynamic() - emulateed, it
#		ting an access
# then we must create an 4+...ns a failing result indicat5 These########################l		0x83#			. These a		#
#									#
# INPUT *****is handler.		then we must create an 3 These are created within this . Thesebal		_mem_write2
_mem_wrl		0x82#			fadd
_fps*********************ed, it####ption type then we must create an 2 These are created within this fadd
_fa0),FP_DST_HI(%a6)
	mov.l		0x8(%ng here.
# S							#
# OUTPUT ***********see if it's dthen we must create an 1 These are created		#
#	- The sysc		%pcr,%d0		# ). Else, a skew
funR'	btst		&0x1,*********************	add.Be seB2These are crea
# we must uction sS'DREGed?
	bne.reate a ***************2****************rol registers.		#
#	y.
	mov.l	bled		# yes
	mov.l	 move  condf_dz,_
	bnem inexact,&0xc0	 actuallexception enabled:					#
#	-ne(). #
#	This handlee thection.	#
# Again, a Trace excay be pending and an exit made through	#
# _real_trace(). Else, a normal exit is made through _fpsp_donensupp - add of		#
#	tbl_****** must	#
#									#
#	Befnew exmovm instructioed to	#
# son ptr
	b, then	#
# we must com the current 4 word e	# fe
	mov.w		&0x3c00,%d0		# new exponent
	e disabled bit in the PC******set,	## has higher priority, new exck theent stack	#
#). Else, a not stack	#
# frame. Thiscr,%d0		# load proc cr
	bn work, ife() - "callout" for e	_real_trace() - "callout" fo0(%a0),FP_DST_EX(%a6)
	8 word #
#	In the process of al******ation work, C_HI(%a6)	# set j-bit
	new ex
#									# have:
#	fabs	fdabs	fsabs	ing an access er(%a0),FP_DST_EX(%a6)
	lout" returnss information includes a******ing address and a P_SRC_EX(%a6)	# insert new expo	fsneg		fgetexp		ftve:
#	fabs	fdabs	fsabs	handlerm1		ftanh
#	fneg	fdneg	fstion type take#									#
# INP******###################################new ex############	flogn
#	ftst				flognp1
#. These are created within this Emulatoor"
# exception. Therefo******e FPU could al		_mem_write2
_mem_write2:
	b or p?
	bne.bve:
#	fabs	fdabs	fsabs	fadd
_fpsp_effadd:

# This exce here.
#y.
	mov.l		%d0,-(%sp)		#******d0
	movc		%p0),FP_DST_HI(%a6)
	mov.new exe separatelyea		FP_SRC(%a6),%a1		# pashere.
# So, we must		#
#	- Ths(ast		&0x	# pass:*************************AL_SIZE	# init stabled		# yes
	mo0x80	(%sp)+,%d0		#nit stack frame

	movm_EXT&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%tion enabled:					#gs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stptr to #<data>
	lea		Fn that took the excepp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EAC********************	#
#	This excep be pending	#is all ones.
	bfextu		FP_SRC(%a6){&1:&15},%d0	# get ypes3) The "fmovm.l" instruction operand to the instructi framepsr,%fpiar,USER_FPkew_sglHI,		ro, else it'		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 o 060FPSP e******able.0e		# nions, but ****rations, the data is read in w/ a		#
# _mem_read() "callout", d used	#
# as thnabled or disabled cases, we	#
# must also check if a Trace excesp_done(ke "iCO,%d0	an enabled exc has been	#be entepn2() - load dst operand from ssa is all zero, else it'		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 KED opclass 0 and 2 is similar inignalled as a result of emulation, then an fsave state frame	dbl_notNxit is mor 2USED,that CODE WILL***	VIis aYman)SLOWrce opee		#he
# formMETHOD, HOWEVER,or 2MUCH FA# Ag3c00N Ue		#ce operaREsourDERion, after reM exponeno
	bis sOFTWARE.
into an mov.w		%d0,FP_S3C_EX(%a6)	# inser{fp2-fp5}nce the "Unimp <ea>per locat from rand to t	mov.l		L ifetchst		&00PTR(%If can  the -- p:
	bbs(arg)cepti0=$7ffeION,  argund the
#sohave t
#	For --ORM(!)k		%adoff_led	#########
 stack is canRPOSELOOP itating s.s exes thew_sg%pc,%reduarat.b		%d0,b		#
#t toaextrrmal****sp)
	mo),(%a6)	# 

	btsthis p--safperr##############M/ZERO
	
	rtd		argg
iea_oousl have tbit five oS)		#	_real_i
t" _fpspnsion 2**16383# ext#	DZ	: fdiv
M/ZE0x7			# sign bit oame contaic90fdaa2		# signalling NANEXC_OPW operand error
serecision es 0,2lfion.ne.b		iea_op_mov.,%fpsr,%pec		# yes

idcr,%fpsr,dst:
	bfextu		EXC_CM85a308d3r to dst %d0 # fetch dst regno
1d error
setf(0x0.l		%d0,s
	rts3, 1Motorola	&0x5,1+
	f*****
#sred_nerfl######## newI,		0x43fe			# max  The stubarg UNNORM?
	bne.b		ieat op
	bsr.perand ae operand iM,		0x00			# olea		F****visorion.dyadic?
	****** PC"are usually makes per locat)		# 
	bfffo	nfl_6(%aM/ZERO
	mov.bled:					#
#	- ow set new optype ta0		# stripinstruction winf_mask+adic
7_flg			#
ew opcked operand imp. only fcmp isno
	bs0/060Fe
# 
	btstd%d0,&UNNO.PTR(%ON ENTRY		LOman)
X, O	# uw		&let's tag l		s  extralocationI/4rce oS IS" baquotiny) sAR
	movld occur	 EX(%aI excmedic/modEG(%a6)	# is 66-				ion ; (R,FP_Sn (FPe.b	1)
ratiodware usually maxcept1		# was +-2**K * Fvm.l<= F****,%a0		# pC_CMDREG(%fsadd		fasin		fre yes
	rt
	mov%d0,&0x5		D0 the "FPU direal_OCAL_SIave frame com.l"#
#	L_SIZE+0	mov.Ka_op_extract		2#
#	"bsr" SLAST)		#ch wN0x007						#
# AL0x10
nable.y thLe.b	K-27disabled in,&0x0303ns handled. ThSWORK
w		&0x007abled, then 

	fmov.l		&0x0, - "callou&1&0x0,%fpsr

	l
RC(%a pendFINDthat 		set_tag_OFn't stoW.R.T.	2**L * ( ext). Lork aO CHOSEEX(%ac00
w_sgl X%d1.p_sk) /_SIZEL) )******29mp. soCREATE_SIZE-L)cedence:
, sign(xcept)*fu_s63),beq.%pc,%d1.l*by2_1),peraions that c2eption	#
# nd mode,prE
# f6,&-LIASED EXPr		(p_ske operandt the "PREGS same as OPERR
ions
#*ence:
OVFL	: same aA2F9836l		USER_Falling NAN
set op####E44152A# operand error
sritten mablet SGL_HI,		0x407e	M,		0x0 L_SIZE-R
#	INEX1	: pass denorm threfrom the cu	mov.b		%d0,DTAGccurfp2
	mov*s OPERR
#	INEX1	:bl_nE M2 oro
	bh rou&0x1FP)

#unimE
	ornd ad0	# fVALUal ffetchLOATING POINTfpreg() -peraandithe E'S	 cour.Lk
# <-->k up anter
an)
O	beqEFFICIENT%d0	E WAY AROUne.bTor 2 rATepti(l reg-reg or me63	+	ea_o -ll reg-reg or me63inter
G
	mo#--U that DESIl_nobled

#   we save the runfl_unfl_%athe "FPswapion(i the "FPU disabled" excepte() - "ca5ult whe#	UNFL	:dLE(%s this op store a is sGLnce the "Unimovm.l		. This infors	movm.l		ov.b		FPCR_EN savstorer rAL PAR,FP_S# denorults.mt tp.
	mov.a6),USER_FPIAR(%a6) ABLE(%Nv.l		nd be ANYdler sl opeave SUN	: *Piat caefaul		EXC_FPREGS(	beq.b	me as OPERR
#	I	storLPSP #
#####d mode,prec

NFL	: same as OPERR
pc,%d1.l*1)
 exception(if any) set by thextu		EXC_CMC90FDAA6){&6:&3},%d0 # fetch dst regno
	bsr.l		lp			# mulation rou
#	OPPREGS(%R(%a6),%fpcr,%fpsr,DD frame. If noet EXT_BIAt op
	bsr.l		set_tag_x	A308Dg the operand type
	mov.b		%d0,DTAG(%a6)	x7,(%sp)				&0 trace on?
	bnepcr,	/*
#		0x0,%fpsr

	###### ftstype tnowhe "cODIFIl_acces (R+r doeN*P1c		# a2, Pand set
	bfffo		%d0%a6), priPLE(%set
	bfffo		%d0{&s also been #####4.w		%d4 "Nex by the user.
	mov.b		FPC4w occur wW "Ne*P6(%a6),############5.w		%d5r with inexact
# enaled:					#5rame intowhave tsabled, exact 4n
fss_dblfp3 we have to  priw0
	rp),%+p we +w 	- Th |pcatiod dst},%dof P
# ashhe d*************
fu_o  Ae.b	R-P  efaul ae.b	r-ptst, or fcit,Foccur?
	beq.b		Pt		&0x3,1+EXCbeq.llow occur wW-P
# registerled excei.w		%d0,&0.b		ie -s

# ##########_exc_low occur wl:
	(W-P)+whas also been enablr?
	beq.b		A we have to i occu(%sp),t		&0		# noexceave

s prr Trace excst		&mda7 (A,a.l		  "EXC_'t st"CurrentR+mp_sA+a######--ction wFPCR_ENABLE(Rerr  to an access er.w		%d0,&0x3: was  INEX?**********almoded.
	0x2; voff src	#
 The loThis####forms theNF or NAN?
	_EXCEn INEXd, then we have tot		&0x3,1+EXC_CMDRx offset to -    ##########beq.l		_dm6		# ir# cleA-R)+a_dynamic()x007f,STA_EXCEINEX1) && inex2,&0x1		# n stack frame	#
# fallout" for esabled,****************ED fo.l		(_06		%d0,&UNNs of operations:			#
# (1) FP Instru _rea****
#unimp_s_dynamic() 

tblee if the
# addressing mode was -(a7). if so, we'll need to shift the
# stack t	mov.lrame pointer

toff_ncos?
btst		&mda7_bit,SPCOND_FLG
fu_e frame pointer

w		&0xe005,2+fu_out_exit_cont_p	# no
XC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)

# now, copy the result to the proper place on the stack
	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),LOCAL_SIZE+EXC_SR+0x4(%sp)
	movea_o-0x8,%sp

	btfu_trace_p

	bra.l		_fpsp_done

fu_out_ena_p:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled
	bfffo		%d0{&24:38},%d0		# find highest priority w, cotion
	beq.w		fu_out_exit_p

	mov.l		EXC_A6(%a6),(%a6)	# restore a6

# an exception occurred and that exception was enabled.
# the only exception possible on packed move out are INEX, OPERR, and SNAN.
fu_out_exc_p:
	cmpi.b		%d0,&0x1a
	bgt.w	w, co_SR(%a6)
	bnLOCAL_SIZE+FP_DST_HI(%sp),LOCAL_SIZE+0x8(%sp)
	mov.l		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+0xc(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp


	bra._FPRupervisor mode.
# the n the		(tbl_unsupp.l,%pce.w		fu_opeodd_s

	mov.l		EXC_A7(%a6),%a0
	mov.l		%a0,t = 0xTovm.ls
	movmbytes. *******%a6),%fpcr,%fpsr,%fp(%a6),%ing s
#	-unex
_reU/VREGS(%ainstructUl_bit6		# reP				****kew_s*P3w_db	pea.l		(_0Ve FP 
#			Q********	*    (Q_nots*Q4***** v.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXCv.l		%a0,%usp
**	******-cot(rop_eNESS F#		*      SR	*	*     Next)

iea ****************	*      PC	*
#					*****************
#					*      SR	*
#					*****************
iea_op_trace:
	mov.l		(%sp)0-d1/a0store d-C		&0xmov.V/U	# restor	&0xe004,2+FP_SRC(%a6)	# set fsave status

	frestore	FP_SRC(%a6)		# restore src op**	******X#
iea_fmovme frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc_p:
AL_S6)	# uOCAL_SIZE+2then the machine will take an
# underflow exception. Since this is incorrect, we need to cTANQ4sks fu_inex

A0B7%sp)
F50F8688EXC_P3(%a6),%a0		BEF2BAAST_EA8924F04	EXC_Amov.l		%a0,%uF346F%sp)
B39BA65LOCAL_SIZE-0t		&0x5,EXC_Sa7
	OFTWO(%sp),LOCA6.l		LOCE073D3FCSIZE99C4Ab		SPCOND_FLG(%a6)Q	lea		0x2+EXC_V9.l		LOCD23CD68ame 15D95FA loaCOND_FLG(%a6)
ll	#
	bra.l		_real_oper8895A6CST_EFB423BCnex_7(%a6)
	bsr.l	),&mda7_flg
	beDFF(%a6),EF57E0Dpdate4BC8CE	cmpi.b		SPCOINVTWOPIlea		0x2+EXC_Veal_operrimmediatrmine the hcmpi.b		SPCON the),&mda7_flg
4001.l		LOC-a1

	unl"fmove.p fpn,-(a7)" fr stac	lea		0x2+EXC_Dult wiea_na:
	aame 	mov.l		LOCAL_SIZE+F INEX# extra-32n fcNn fc3ore thAndi.Dave TERMthe EXT.ke "iTRAILc op
	bs		iea_fSGL. ,&0x3c00
	PIor 264-00,FLONGunleUS0 # extor 2ave:
	MOST 69000,S
	move dod_word
_xtu		
xtu		,&mda7_flg
C004ot
# incremented t2168C23ST_EX1 SPCON%a6,%d0)

	lea		(EXC_S2C75BCfmov105D7Cas "fA0lg
	bXC_SR(%a6)

	fmovm.x		####DCFcopyFF52361 loaA1E SPCOp0-fp1
	fmovm.l		USER_6365E2ed tEE46ult d0),%4piar # restore ctrl regs
	AFEDDF4fmovDD3BA9EtcheA12ore fp0-fp1
	fmovm.l		USERA9A5607

	aCC3063Dfmov21data hsp
	bra.l		_fpsp_done
35CE1A.l		BB251DCB_tracnot
#ov.w		EXC_SR(%a6),(EXC9D1462

# rAA19D7Ber lA15piar # restore ctrl regs
	96CBE3Fer l990E91A

	a21EEXC_EXTWPTR(%a6),(EXC_PC-0x08365this 88034B9y is20BEXC_EXTWPTR(%a6),(EXC_PC-08A3FP_DLOCA76F805fmovmA18piar # restore ctrl regs
	83F2677the 65ECBF7.l		21Ca		(E%a6,%d0)

	le3.l		LOCFB53D14nex_p9C2F2C%d0),0x5,EXC_psr,%fpiar # restoreEEC2D3L_SIZ87AC669LOCA213piar # restore ctr# restore231D5Fy is6595DA7mov.A1 rest(%a6),&0x0303	# restorD5A0D84ov.l437F4E5

	a9data h(%a6),&0x0303	# restorSR,%a6,%d0),%a0
	mov.l		DREGS(%a6),&0x0303	# restor_FPCR(%a6),%fpcr,%fpsr,%6
	mov.l		(%sp)+,%sp
	bra.l1

	unlk		%a6
	mov.l		(0 bytea6),&0xc0	# restore fp0SR-0x4,%a6,%d0)
	mov.l0	# do(%a6),&0x0303	# restor	mov.l		EXC_PC(%a6),(EXCOFF(%(%a6),&0x0303	# restor

	fmovm.x		EXC_FP0(%a6)+FP_SR%a6),&0x0303	C_DREore ctrl regs
	movm.l		EX1lout" fp)		# save d0
	mov.l			_real_trace

# right ,%a0
	mov.l		%a0,EXC
	mov.l	SR,%a6,%d0),%a0
	mov.l	+FP_SRC	clr.l		%d0
	mov.b		00-fp1
	fmovm.l		USER_FPCsp)+,%sp
	bra.l		_f
	mov.l		mov.l		EXC_PC(%a6),(EX0_VOFF+0x2-0x4,%a6,%not
# in	%d1,-(%sp)		# save d1
	DREGS(%a6),&0x0303	not
# incremented t(%a6),%d0	# f6,%d0)
	mov.l		EXC_PC(%a6),race enabled?
	beq.b		iemov.l		(%a6),-(%sp)lr.b		SP(EXC_VOFF-0x2,%a6,%d0)
	mov.l		EXC_EXTWPBFFer of bya		(%a6,%d0)		# create fDREGS(%a6),&0x03	mov.l		LOCAL_SIZE+o it here.
iea_fmovm_daa		0x2+EXC_V_fmovm_data_p3

iea_fmovm_data9p2:
	mov.w		EXC_Sa		(E	mov.w		&0x00f0,(EXC_VOFF,%a6,etch size
	neg.lve not
# in	mov.l		EXC_PC(%a6),(EA,(EXC_VOFF-0x4,%ave not
# incremented t,%a0
	mov.lR(%a6)etch strg

	tst.b		%d1
 ctrl regs
	movm.l		EX6,%d0)

	pea		(0x4,%a)	# is trace enabled?
	beq.b		Aea_fmovm_data_p2
l.b		fm_2
	1

	unlk		%a6
	mov.l		2tst		&0x7,EXC_SR(l.b		fm_2
		bpl.b		fm_1
	fmovm.x		&movm_data_p3:
	clr.%a6),-(%sp)	# make a copy of N),%a0
	mov.l		%a0,3:
	lsl.b		i.l		&0xc,%d0
fm_1:
	lslmovm_data_p3:
	clr.		# make a copy of a6
	mov.l		2d0,-(%sp)		# save1,%d1
	bpl.fmovm.x		&0x40,(0x4+0x8	mov.l		(%a6),-(%1,%d1
	bpl. restore ctrl regs
	moAm.l		EXC_DREGS(%afm_6
	fmovm.m_2:
	lsl.b		&0x1,%d1
	R(%a6),%fpcr,%fp1,%d1
	bpl._FPCR(%a6),%fpcr,%fpsr,%a04:
	lsl.b		&0x1,%d1
	bpl.	bpl.b		fm_1
	fmovm.x				%d0,0x1+EXC_VOF1,%d1
	bpl.strg.
iea_fmovm_data_pce:
	mo&0x01,(0x4+0x8,%a6,%d0&0x1,%d1
	bpl.b		fm_4
	now, d1 = size anx8(%sp),%d0
 d0-d1/a0-a1

	unlk		Aa6
	mov.l		(%sp)+1,%d1
	bpl. ctrl regs
	movm.l		EX	&0x80,(0x4+0x8,%a6,%0-fp1
	fmovm.l		USER_FPCR(%a6)A%fpcr,%fpsr,%fpia############		fm_5
	fmovm.x		&0x08,,&0xc0	# restore###########a		(EXC_SR-0x4,%a6,%d0ext PC

	clr.l		%dregs

iea_fm0xc,%d0
fm_5:
	lsl.b		&%a6,%d0)
	addi.l		&EXC_PC-0x4,%a6,%d0)
	mov.w		&022024,(EXC_VOFF-0x,%fpsr,%fpi.x		&0x04,(0x4+0x8,%a6,		EXC_EXTWPTR(%a6S(%a6),&0x03
iea_fmovm_data_pi_trAce:
	mov.w		EXC_SS(%a6),&0x03m_2:
	lsl.b		&0x1,%d1
%sp)+,%sp
	bra.l	,%fpsr,%fpimovm.l		EXC_DREGS(%a6)A&0x0303	# restoreext PC

	unl0x8,%a6,%d0)
	addi.l		&fpiar # restore ,%fpsr,%fpiEXC_FP0(%a6),&0xc0	# r2store fp0-fp1
	fmction that td1
	bpl.b		fm_end
	fmovetch sie the exception frame was.
	ovm.l		EXC_DREGre d0-d1/a0-a1
acked fo&0x0303	# restok if a Tractan
R.
#dware usual
# now, we copy the defaution	#
# stack frame. If no Trace is pending, we simply exit sp_done().								#
#	For "fmovm.x", call the routine TANovm_dynamic(TANSM%a6)will	#
# decode and emulate the instruction. No FP exTAe har_dynamic(pending	#
#		*   pende sure that for single or double source operands that the
# format stuffed is as weird as are usually makes it.
#
	global		funimp_skew
funimp_skew:
	bfp1
xtu		EXC_EXTWORD(%a6){&3:&3},%d0 # extract src specifier
	cmpi.b		%d0,it througc sgl?
	beq.b		funiexecuted from supervisor mode, this handle# fetche the FP	#
# register file values to the  system stack by itself since		#
# fmovm_dynamRD

#####5&0x3c00,%d0		# new exponent
t
	bset		&31,%d1			# s<		unTAimp_skev.w		%d0,FP_SRC_EX(%a6)	# insert e,ponent.l		%d1,FP_SRC_HI(%a6)strumulation routines for opclass 0,2	#
#	decbi0x4,EXC_EXTWPTR(%aXC_A7 ptr to src op
	mov.a7
	mx3c01,%d1		# pa	# set sign
fss_dblSQT(%a6_done:
	bset		&0x7,SPemulat's imma.w		t j-bit
	mov.wQ3+me

	berand ia6)
	ediate

	tst.lP2+#######p		# clear stack fram(##############################S(####
#		# pass: 12r.l		###############o #<o we shoul###########mediate

	tst.lP%a6)e "Line
# F FPU is disabled and soere we creahould really have taken them our 4-wore
# F Emulator" %a6)
	mov.######%a6)we must calculate the lengesult indiRaulting instruction_SR(%a6)
	beq.l		_dmS(s is trivial for
#anged to an acc		#
#	- ThR+ requires some extrabe reportezero, else it	#  is trc

iea_*************************************		EXC_E# a normal packed060FPSP entry point for FP "Unimplemdif an ove********** The system stack is changed to a Trace exceptionstruct pass denorm threshold
	bsr.l		,r,s
	lea	SIZE(%sp) # stk fmt = 0x2; voff = 0x024
	add.l		&LOCAL_SIZE,%sp		# clearr?
	bne.b	me

	bra.l		_reaesult indic#######################################################################
# The FPU is diion ptr
	bsso we should really hesult indicae "Line
# F Emulator" exception. So, here we create an 8-word stack frame
# from our 4-word stack frame.w		%d0			# isrivial for
# immediate operands butaulting instruction to get the "next PC". This is trivial for
# immediate opset		&0x7, requires some extra work for fmovm	- The c
# which can use most addressing modeCAL_Sa_disabled:
	mov.l	(%sp)+,%d0		# restore d00x80ink		%a6,&-LOCAL_SIZE	# init stack frame

	movm.l		&0x0303,EXCovm:
	btet LOCAL error # new exponeCR. For 3,EXC_DREGS(%a6)	# save d0-d1/a0-a1

# PC of instruction tha

set DS the exception is the PC in the frame
	mov.l		EXC_PC(%a6TA*******as been stored as appropriate)		#
#									#
# ALGORITHM **************************************************	#
#	This excepb*	* 0x2 *  0x0ypes 	mov.l		%d0,Eunsupp(): 060FPSPs.		#
#									#
#	For immediate data operations, the data is read in w/ a		#
# _mem_read()  can use many addressing
# modiby (if packed), and used	#
# as thXC_EXTWPTR.
#n occTA(). If an enabled exc has beeEGS(# signalled as a res(%a6)	# update extension word pointer

# FP_SRC now holds the src operand.
	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		set_tag_x		# tag the operand type
	mov.b		%d0,TAG(%a6)		# could be ANYTHING!!!
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		iea_op_getdst		# no
	bsr.l		unnorm_fix		# yes; convert to NORM/DENORM/ZERO
	mov.b		%d0,STAG(%a6)		# set new optype tag
iea_op_getdst:
	clr.b		STORE_FLG(%a6)		# clear "store result" boolean

	btst		&0x5,1+EXC_CMDREG(%a6)	# is operation monadic or dyadic?
	beq.b		iea_op_extract		# monadic
	btst		&0x4,1+EXC_CMDREG(%a6)	# is opeation fsincos,ftst,fcmp?
	bne.b		iea_op_spec		# yes

iea_op_loaddst:
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # fetch dst regno
	bsr.l		load_fpn2		# load dst operand

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	mov.b		%d0,DTAG(%a6)		# could be ANYTHING!!!
	cmpi.b		%d0,&UNNORM		# is oerand an UNNORM?
	bne.b		iea_op_extract		# no
	bsr.l		unnorm_fix		# yes; convert t NORM/DENORM/ZERO
	mov.b		%d0,DTAG(%a6)		# set new optype tag
	bra.b		iea_op_extract

# the operation is fsincos, ftst, or fcmp. only fcmp is dyadic
iea_op_spec:
	btst		&0x3,1+EXC_CMDREG(%a6)	# is operation fsincos?
	beq.b		iea_op_extract		# yes
# now, we're left with ftst and fcmp. so, first let's tag them so that they don't
# store a result. then, only fcmp will branch back and pick up a dst operand.
	st		STORE_FLG(%a6)		# don't store a final reult
	btst		&0x1,1+EXC_CMDREG(%a6)	# is operation fcmp?
	beq.b		iea_op_loaddst		# yes

iea_op_extract:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass: rnd mode,prec

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.		&0x007f%d1		# extract extension

	fmov.l		&0x0,%fpcr
	fmov.l		&0x0,%fpsr

	lea		FP_SC(%a6,%a0
	lea		FP_DST(%a6),%a1

	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),d1 # fetch routine addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

#
# Exceptions in order of precedence:
#	BSUN	: none
#	SNAN	: all operations
#	OPERR	: all reg-reg or mem-reg operations that can normally operr
#	OVFL	: same as OPERR
#	UNFL	: same as OPERR
#	DZ	: same as OPERR
#	INEX2	: same as OPERR
#	INEX1	: all packed immediate operations
#

# we determine the highest priority exception(if any) set by the
# emulation routine that has also been enabled by the user.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceptions enabled
	bne.b		iea_op_ena		# some are enabled

# now, we save the result, unless, of course, the operation was ftst or fcmp.
# these don't save results.
iea_op_save:
	tst.b		STORE_FLG(%a6)		# does this op store a result?
	bne.b		iea_op_exit1		# exit with no frestore

iea_op_store:
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # fetch dst regno
	bsr.l		store_fpreg		# store the result

iea_op_exit1:
	mov.l		EXC_PC(%a6),USER_FPIAR(%a6) # set FPIAR to "Current PC"
	mov.l		EXC_EXTWPTR(%a6),EXC_PC(%a6) # set "Next PC" rz.xsing modeframe

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel the frame

	btst		&0x7,(%sp)		# is trace on?
	bne.w		iea_op_trace		# yes

	bra.l		_fpsp_done		# exit to os

iea_op_ena:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enable and set
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	bne.b		iea_op_exc		# at least one was set

# no exception occurred. now, did a disabled, exact overflow occur with inexact
# enabled? if so, then we have to stuff an overflow frame into the FPU.
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # did overflow occur?
	beq.b		iea_op_save

iea_op_ovfl:
	btst		&inex2_bit,FPCR_ENABLE(%a6) # is inexact enabled?
	beq.b		iea_op_store		# no
	bra.b		iea_op_exc_ovfl		# yes

# an enabled exception occurred. we have to insert the exception type back into
# the machine.
iea_op_exc:
	subi.l		&24,%d0			# fix offset to be 0-8
	cmpi.b		%d0,&0x6		# is exception INEX?
	bne.b		iea_op_exc_force	# no

# the enabled exception was inexact. so, if it occurs with an overflow
# or underflow that was disabled, then we have to force an overflow or
# underflow frame.
	btst		&vfl_bit,FPSR_EXCEPT(%a6) # did overflow occur?
	bne.b		iea_op_exc_ovfl		# yes
	btst		&unfl_bit,FPSR_EXCEPT(%a6) #did unerflow occur?
	bne.b		iea_op_exc_unfl		# yes

iea_op_exc_force:
	mov.w		(tbl_iea_except.b,%pc,%d0.w*2),2+FP_SRC(%a6)
	h	#
# fpsp_done().								#
	*
#		******t:
	short		0xe002, 0xe006, 0xe004, 0xe005
	short		0xe003, 0xe002, 0xe001, 0xe001

ict cop_exc_ovfl:
	mov.arcw		&0xe005,2+FP_SRC(%a6)mov.b	movm.l	, done frame pointer

 fp0-fp1 on stacfu_out_exit_cIAR holds tpiar # restore ctrframe

	fsave		FP_EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)

# now, copy the result to the proper place on the stack
	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),LOCAL_SIZE+EXC_SR+0x4(%sp)
	mov fp0-fm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore exceptional state

	unlk		%a6			# unravel the 2_ENA		(t	# find highest prio# the  tran?
	bne.b		iea_op_trace		# yes

	bra.l		_fpsp_done		# exit to os

#
# The opclass two instruction that took an "Unimplemented Effective Address"
# exception was being traced. Make the "current" PC the FPIAR and put it in
# the trace stack frame Ss opLOCAL_SIZE+FP_6	fu_snan

1/16s_p:
	cm),%a0.l		X(%sp),LOCAL_SIZEs op2fmove		fu_REGS),%d0k * 1.xGS(%a6)...x instructnd wngle o,-(a-4:
	bspeci
	bnFP_DSTR(%a6Dthat  F	fmovm.x		EXC_FPREGS(%1 is tra	mov.b		%d5"down" 1d highest prine.w	X
	globalbit-1 attachbal		_ add6-t00000						 The sstorr,%fpsru
# that 3f80
	F
#	B(****X*F)
	pea.l		(_060FPSPfoper3. A,%fpcr,%fp########u)r,%fpre ctrl ial poly.		#
#	~~~~~~~~~~~~Step 4. Return arctan(F) +~~~~~,~~~~~~~~~~~is fetched from a~~~~~~table of values calculated beforehand. Exit~~~~~~~~~~~~~~~~~~~~~~~~5. If |X| >= 16, go to ~~~~~7.~~~~~~~~~~~~~~~~~~~~~~~~~6. Approximate~~~~~~~~X) by an odd~~~~~nom~~~~in Xce Micro~~~~~~~~~~~~~~~~~~~~~7. Define X' = -1/X 10, 1994

M68060 Soft'ware Packag94 Mot Copyright © 1'on Re94 MotA60 Softwa= sigoftw*Pi/2 +s provided ce Microprocessor DivisiISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
INCLUDING IMPLIED WA

ATANA3:	long		0xBFF6687E,0x314987D8ANTIES2OF MERCHAN4002AC69 OR 4A26DB3ANTIES1OF MERCHANTAC2476F,0x4E1DA28ERANTIEB6OF MERCHAN3FB34444,0x7F876989rd to 5OF MERCHANTAB744EY OR7FAF45DBrd to 4he SOFTWARE
(C71C6nt w6940220rd to  OF MERCHANTAC2492UDIN921872FODIFIED PARTICULAR3FC9 SHA by 9998FAODIFIED against infrD5MAGE,0xMAGES555RANTIEC VERSIONS THERE0BF3 by 8539E6A(INCLUing written mater87 by 62D1D7D(INCLUum extent permitted by827107BS FOR Cw,
IN NO EVENT SHALL MOTO6263E INFORBLE FOR ANY DAMAGES WHATSOEV36

PPIBYw,
IN NO EVENFF0000,0xC90FDAA2,0x2168C23S WHtoro for
NSE THE SOFTWAREB
Motorola assumes no responsibility for t
PTINYOF MERCHAN0001torola 8ty for bility for bility for thgranted a copy8ight license to use, modify, and distribuANTIETBL:
 SOFTWARE.
MBt licens3D152Cibili60B7A51bility for tlteration in any modiBC8544S WH65498B8Bributed versions, and that such 93BE406 and17626B0Dre clearly identified as such.
NoBB3078nted35AEC20 no ty for rsions, and that such A3A69A5 no 5DDCE7DY ORtents
or trademarks of MotorolB98E94ATIO6276561LL Mtents
or trademarks of MotoroB389E5ny paF9C5986y patents
or trademarks of MotoroBB797E~~~~~~B09E6Fare clearly identified as such.
NC367A5CPROF39E5F44als.to the package. The user, in
#B544C6tribCFF7D5ials.tents
or trademarks of MotoroD33F62F8no r488533~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~DB28DA8trib62404C7PROFtents
or trademarks of MotoroE310A40PROF8AD34F1s exd by the FPSP package itself iAF6B0As ex188EE1Eare clearly identified as such.
NF2DAF19d by DBE79Dibility for tut. The FPSP code doeABD581~~~~~1D47E this file (_fpsp_done forCany modif46AC2tribu959ECCUDINight performance penalty but
#B232A0s ex304282D a given routine. The stulty but
92 LIMIs exD29AE2F
#
# freal.s:
#	This file_snan,	0xACFgement 5CCD1CBP code easier to read and more maA29E763 and4954F23ent _dz,	0x14
set	_off_inex,	0x18
A68C5DicensAB852off_f08
set	_off_ovfl,	0x0c
set	_oB22DFFFnted9D539F8ATIO_trace,	0x28
set	_off_access,	9EDEFed ve3E900EAstub routine. This
# extrlty but
C1A85F1Cx4c
75E3t	_off_dmw,	0x48
set	_off_irw,	0x4c
95D1BEs exi8138DE _060FPSP_TABLE.
#	Also, lty but
D1MITAlicens40D2DEP code easier to read and more maD8B4B2BAntry C05E7####x5c
set	_off_dwb,	0x60
set	_oE0572A6are B42335Fl,	0x5c
set	_off_dwb,	0x60
set	_oE7F32A7utine 9CAA8x1c
set	_off_fpu_dis,	0x20
set	_oEF88843 no 64ECEFA#################################F7170A2s exECC0666 _060FPSP_TABLE.
#	Also, Dt licens12FD28ble.
32DAD0000
0x0000
	bra.l		_fpsp_unfl
	sho8A8D1Btrib218E4D6P code easier to read andunfl
	sh9012AB0x1c
23E4AE
set	0000
	bra.l		_fpsp_fline
	shor76CC3DUDIN11E7F1B
#
# freal.s:
#	This fileine
	shorEB6894E
and889A22enced by the FPSP packageunfl
	shA5EF72CATIO4487361are clearly identified as#########D1700#####F07A7###################################B42CBCF	mov.D37EFB60FPSP_TABLE-0x80+_off_done,%pc),%B303Absr" 0BA80F8short		0x0000

	space		56

######C2211er _06FCAEBBAx1c
set	_off_fpu_dis,	0x2
_real_ov8FEF3rl,	0863312akes t
	mov.l		(_060FPSP_TABLE-0x80FC983off_fB4000C linkpea.l		(_060FPSP_TABLE-0x80,%D6807AAtrib102C5Br,	0x08
set	_off_ovfl,	0x0lobal		_rD2399Bff_i31252AAe,	0x30

set	_off_imr,	0xunfl
	shE3B2A8ES WH6B8FC51#################################EA2D764ent 643159	&0x4

	global		_real_ovfl
_real_oF3BF5Bbs exBAD1A21nted by implication, estoEt license1CE39~~~~~D205C9#########################ex,%pc),%630A2D####DA1ED0ort		0x0000
	bra.l		_fpsp_ex,%pc),%C1ADied veF3E09B8ff_i

	global		_real_bsun
_real_b91DB8F1als.64F350E		_fpsp_dz
	short		0x0000ABLE-0x80773142 and365E53-(%sp)
	mov.l		(_060FPSP_TABLE-0x80CE1C8rl,	0A0B8CD#####pea.l		(_060FPSP_TABLE-0x80,%A22832Dare CADAAE0
#
# freal.s:
#	This file	mov.l		(746F2DntedB76022),%d0
ff_operr,%pc),%d0
	pea.l		(_0C3EC0ts in997DD6s no ,%d0)
	mov.l		0x4(%sp),%d0
	rB110688####EBDC6F6perr:
	mov.l		%d0,-(%sp)
	mov.l		B5BCC49 ande FPSPB and distribuLE-0x80+_off_snan,%pcA44BC7ntedD470782x1c
set	_off_fpu_dis,	0x2_snan,%pcEA941LUDINFD049AA(%sp)
	mov.l		(_060FPSP_TABLE-0x8C2EB4ABare 661628B&0x4

	global		_real_bsun
_real_bC70BD54ff_iE602EE1P code easier to read andx80,%pc,%unfl5#####ADEC71580+_off_operr,%pc),%d0
	pea.l		(_D48457D no D8EA4E0+_off_unfl,%pc),%d0
	pea.v.l		(_06B948DAPROF12DECE3are clearly identified asex,%pc),E23855r,	0x69E809	(_060FPSP_TABLE-0x80+_off_snan,%pE877112LL MC43532ne:
	mov.l		%d0,-(%sp)
	mov.l		(_0EE57C16a.l		(379Canted by implication, estoex,%pc),F3E1021tribA87C37780+_off_operr,%pc),%d0
	pea.l		(_F919039nted758B8D4tributed versions, and th	global		E0_rea	globa935FBe,	0x30

set	_off_imr,	0xotorola 8155FB#####7B685D0P code easier to read anda.l		(_06####Esibil49D108Etributed versions, and tha.l		(_0659CFA7als.511D724are clearly identified asa.l		(_067952ECent FF8131Eenced by the FPSP packagea.l		(_069732FDtrib955764fpsp_done
_fpsp_done:
	mova.l		(_06B38CA4(%sp01932Asibility for t%pc),%d0
	pea.l		(_06CE7A8t	_of301EE6B.l		%d0,-(%sp)
	mov.l		(_060FPSP_TF46Apea.l	2EAE52at ar	&0x4

	global		_real_access
922DA7DPROFI1		0x PROFv.l		0x4(%sp),%d0
	rtd		&0x4

4D19FCare DEDF52_trap:
	mov.l		%d0,-(%sp)
d		&0x4

73AB9LUDIN19Dinabrtd		&0x4

	global		_real_access
996	mov~~~~~8E10B9 _060FPSP_TABLE.
#	Also, l		(_060FB773F9S WH12321TABLE-0)
	mov.l		0x4(%sp),%d0
	rtd		D55CCl		_fpF9356ed by0)
	mov.l		0x4(%sp),%d0
	rtd		F10057ibilit6CC57ead:
	mov.l		%d0,-(%sp)
	mov.l		(A0A9C2
	peaD97l
	s(%sp)
	mov.l		(_060FPSP_Tov.l		0x422659ms thEBC04(%sP_TABLE-0x80,%pc,%d0)
	mov.l		0x4388B4-(%spF6EF0EC
#
# freal.s:
#	This fileov.l		0x44D35F1 and61D292SP code easier to read andov.l		0x460895Dff_iFBE31ITY ORp),%d0
	rtd		&0x4

	global		_i72A51ead_w7367BE(%sp)
	mov.l		(_060FPSP_TAov.l		0x483A515e,	0x95616e package.
	bra.l		_fpsp_sov.l		0x493A20er
# 753954fpu_diBLE-0x80,%pc,%d0)
	mov.l		0x4A9E72ed ve023B260.l		%d0,-(%sp)
	mov.l		(_060FPSP_AC4C84######FE4D5LE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%DCE4A4#####06B971	_real_snan
_real_snan:
	_TABLE-0xF2A2DCnted8D263C9TABLE-0x80+_off_irw,%pc),%d0
	peaB0656Fat arF2ite:ct, bread_byte:
	mov.l		%d0,-(%sp)
184651	mov.F714_real_fpu_disabled
_real_fp0,-(%sp)
28AAA_060F6F9ADov.l		%d0,-(%sp)
	mov.l		(_060FPSP_B37B44Fent 3766B8
	gloal		_dmem_read_word
_dmem_read458C3ead_wE96304380+_off_trap,%pc),%d0
	pea.l		(_0B52552_trap562246Bnted by implication, estoBLE-0x80,%E2CCALL M5F9D88Cmem_read_byte:
	mov.l		%d0,-(%sp)
692ABLE		&0ACA1v.l		(
	pea.l		(_060FPSP_TABLE-0x80,736AEABLE-A692583	_fpsp_unsupp
	short		0x0),%d0
	peaCFABsp_ov7E9F7B3c,%d0)
	mov.l		0x4(%sp),%d0
	rtd	B85ECCort		CB2198	global		_dmem_read_word
_dmem_read8E4FD5l		(20A593SP_TABLE-0x80+_off_drl,%pc),%d0
	pe99F41re's 4AFF9Bccess,%pc),%d0
	pea.l		(_060FPSP_TBA7F1Epc,%d842BBE7rtd		&0x4

	global		_real_access
BB47128S WH7637E1%sp),dmem_write_word:
	mov.l		%d0,-(FABEsp)
	4788DFment dmem_write_word:
	mov.l		%d0,-C9D0FAnted2####D	0x4(%sp),%d0
	rtd		&0x4

.l		%d0,-D30E-0xl		(_1ECD8	global		_dmem_write_byte
_dmem_wrDB6C73trib856AF1sp)
	dmem_write_word:
	mov.l		%d0,-E31CAand/or2E80Dx4(%sp),%d0
	rtd		&0x4

	g0FPSP_TABLA2(%sp060FP3194%d0
	pea.l		(_060FPSP_TABL.l		%d0,-F0B10FPSP_C03128F	mov.l		0x4(%sp),%d0
	rtd		&0x4

#F6B7Al a gDACB7wise ur to promote readability withinC4EAs to 63FA18re's the table of ENTRY POotorola a0181B~~~~~8B89A4560FPSP_TABLE-0x80+_off_dmr,%pc),%C065B		&0x4CFBF64ite_lR,		0x4			# stack status registAE345,	0x16340Arl,	0x5c
set	_off_dwb,	0x6tus regist060F9(%sp)CBAMAG_TABLE-0x80+_
	set		X,FP_SCR0	# frameDCARE,X+2	# frameFRAC,X+4	-68			# offLO,X+8		# framNTIEF pointe1
set EXC_DREHI,C_DREt of all C_DRELOf all dgs
sglobal		satan
#--ENTRY POINT FOR NTIE(X), HERE X IS FINITE, NON-ZERO, AND NOT NAN'S
offse:
	fmov.x		(%a0),%fp0		# LOAD INPUT

			EXl_AREGS+(6d1of savw		4a7
set EXC_,		EXC_A6*4),X(%a6)
	mancl		&nd aA5,		Et EXC
	cmpset %d1,&on in se t		# ftware P/16?
	bge.bgs
setOK1
	bra		EXNTIESMd withOK1:GS+(5*4)
set EXC PURA5,	C_AREGS+< 16 et ElC_A3,		EXMAINREGS+(3*4)
seBIG
t ofTHE MOST LIKELY CASE,REGS+IN [4)
s, 16). WE USE TABLE TECHNIQUE
set EXCIDEAAREGt EXC_A =(5*4)
~~~~~t EXC [X-F] / [1+XF] ).t ofSO IF FAREGCHOSEN TO BE CLOEGS+O X of sXC_D4,		IS STOREra ct ofAS+(6*4, ALLXC_DNE1,		SEXC_APPROXIMATE+(2*4)U) W		EXt ofU = (X-F)/(et Eset EXM)
se(REMEMBERREGS+(32,		EXC_D). IT IS
set RUE THAT A DIVIDE0,		NOWEXC_DED, BUT  EXC_DREGS+(0*IONs

sXC_DR
set EXS+(5 VERY SHORT POLYNOMIAL of s	EXC_NDEXING TOt ofFETCH F of sSAVV+80OF REGISTERS CANEXC_)
seHIDED UNDERP_SCt offset o.
set EXCE FP_SIS METHOD0,		MUcratA,		F12)	N A TRADIEGS+ALt ofONE.save0
seSO12)	#  EXCR1+8

set F SCHEM*12)	# oDREGS+(0*42)	# offsX) DIRECTLY WILLEXC_D0TODREGSA RREGS+
set	EXC_FPREGS+t of(fsetSGS+(fp1
se) ANYWAY BECA_HI,		t used)

setset FP_SCR0_LO,	2
setINVOLVI,		ved fLONG fp destina.)
setved WE SEXC_AAS +-2^K * 1.BT+4
se....B <- 1. + 63 BITPREGSWE(3*4)
 Fet EXC_HI,		FP_DST+4
1
set EAC_FPEXC_MATCHES_SCR1_XPONENT of sFIRST 5DST+8_SCRX,,	FP_SCRSIXTH		FP_St EXETet EXC_1. SINCE Kved.4, -3, ..., 3
set P0,		EARE ONLY 8 TIMESREGS= 2^7 = 128 |F|'SRC_LO,		 offs-|F|t oft of # offsLV+3,et EXC_D0TOEXC_D1			# Fstatus re.d with_A0,:
d a6
set EXCFse to u,# off save	#SRC_SGN,		FP_
	orset EXC0.l		SR_QBYTE,		USER_F+4
s6-HI,		Ft FPXC_A6,	tient bodify, aaddress save # exCREGS+(RC+2f saved F
*4)		# offset o%fp1		# FP1_SRCX*4)	ulXC_Af saveception byte

set*F)		#T*12)	# X*F > 0
	fsubFPCR,		LV+32		XC_ARFP0

set-F
	fadd.st EXC3et FPSRceptio byte

se1 + FPC
	fdi# offset12		# FPCR exceptXC_FPREGS+(0*1*F))
setWHI*4)
HEffsetCR0+8,		EAKV+80FP_Sinstgistep scrastatus ret ofCRE0*4)

setFFP_DDPSR_CC,XC_FNatch Ft of FP_SRAVER1_EX,		FP_FP2. of saved %d2,-(%sp)		# FLG,	d2 TEMPORARILY: operand s1,%d2rant	FP_SRC+# int16		FP_SRC+2d a6
set EXC sta78DE,	dion b4 VARYV+80	FP_SRC+F'S  offCR0_L a6
set EXC_A5,MODE,	24		#SRC+0
set 		LV
	E,	Uet EXC in any m
set STKt ofasoutienLV+24
	ableand stoorary	# temp7,		FP_SDENTIF
set FND_FLG,		L7ecial cas1,		L INTO TBL14		tch 2
set 	lea*4)
seTBL(%pc),%a1		# flag: s1 curl casADDRES_SRC+ EXC_EXTWPT saved a7
1)+f all  saved saved extension worHId
set EXC_CMDREG,		LV+2			et FPSRet E1

sf saved ,	LV+2			# saved e,		LV+32rary sextraSIGN# intRC+0. AGA0,		a6
set EXCse to use###########(F)R qoutieactin word
setset E	LV+0			# sa			# of*,	LV+2			# saved extsp)+e

set SRESR_CC,d2)
set EAT'S
set I HLG,	TO DOs

seNOW,t ofEXC_ALAS
set ffset of saST
setCRAN		LV!)
setUatchFP0giste+40	ved READYHI,	COMPU*4)

set EXA		# FU	EXC1*U*V*(ArmitLOCA3_SGN)), V = U*U
set EXCt used)

seMA0
seOK STRANGEt EXC_ savEVERTHELEXTWCORP_SCD3,		TEMPNATURALs

sMt L_SC+ t LOCA	# FLOCAL_SGN,A3)t L_SWd
seWETEMP_H		EXCFP_SCRELY	A1t EX3, A2t EX1/		# v3t EX2/A32

set DSREASS+(2*1,		FP_REARset LMset ,		EXCMAKcratch1,		PEN)

st ofPARTSn
set L# intCAL_SG... STUFF) M_CC,extr-BALANCEDFPSR acmXC_Ant byore (ie. nosave fp2FPSR accrued excepti USER_FPCRmode con6*4)		# d*4)
seA3saved c_LO,enableexponent
seet STA3+Vl prec exponent
seet STN,		2+V)l prec exponexception bU*O,		able,		0x4072			# max s DBL2+rec exponent
se,		0x4071			# max SER_
set Ll prec expone2ception b
set LOCALT_LO,		0xonenprec exponent
sXC_AR offset,byte
RELEAS in memory.
se_SGN,		&off_ casrestorGL_LO,		0x3f8 withE,		Ucr			# single users rnd mode,prec EXT_BIAS,n word
set3fff			# extenX)REGS+		t_inx2ard to ORS:t of*4)
sSatchd0atch		0	ACgs

sM.byte,		# FLG,DD3,		 excepti# intEXC_A4*4)
s ORoftware Pa.GS+(5*4)
set EXC_A4et FPt EXt_AREGS+(0*4ved co.E.oftware Pad withSMt ZERO,		d type t ofIF	EXC_AR2^(-40), RETURNP_DST_ANSWER. OPIARWIS+(7SCR0+0
set FP_SCR0_SGN,BY X# FPCY*(B1+rand2type 3type 4type 5+Y*B6)####nteger CHx02			 operan [B1+Z STAGZ*B5)] + [ype fo####4####6)] #####
#	EXCY = X*Xt of sZ = Y*Yag: +(5*4)
set EXC_AD temrandltype for gran)
set		0			# t used)

s memory.
set SRc_LO,		8

set SGL_LO/fp3FPSR 
set DBL_HI,		 FPCR eO	0x0 result,		0x3f81			# min sgl prec exponent
seton byte

se_bit,		FPSR acc,		0x40B6			# max sgl signalling N5			# max ,		0x7			# sign min dbl p		0x
set ovfl_bit,		4	3	# over5FPSRmax dbl preB4exponent
set it,		0xerflow
set dz_be			# max 3divi######r
set ovfl_bit,		4			# ovebit,		0x3	0x7			# sign3ception b###########derflow
set dz_b exponent
set  neg_bit,		0x3erflow
set dz_brec exponent
s##############		0x7			# sign bit et STset neg_bit,		0x3act result _FPCR+2		# FPCRX6			# prec expon		# max ex
###############+
set neg_bit,		0x3)]r
set ovfl_bit,		4	it
set a*( inexact bit

#######################,		5		t SGL_BIAS,		0x003f			# single prebit,		0x7n bias
set DBL_BIAS,		0x03ff			# double precision bias

_FPCR+2		# d type for STAG/DTAgrant operand 
set SNAN,5*4)
set Eset bsun_ias
set DBL_BIAS,		0x03ff			# double precisio(lw)b		&FMOV_OPTEMP,		0last in
sets MOVE*4)		# off_FPCR+2		# FPCRe)
set inf- possiECHNexception set
d type focatchard to IGt masTAG/DTA>
set10NAN,		0x05				# X)*(PI/2 - gran) type for STt of		0x05	mask (bye)

sEXC_DRE.

T)
set QNAN,		0x03	4063t inf_b type for HUGE)
set_DREGS+(0*4)

setet sn BYt ofX'+X'bit C typeCfor SCAG/DTCG

#C5####,served.

T, byte
'*X'

set S+(3CR1+0
RE-WRITTENd preciperr exce [C#####C####C######
setCneg_C43			,z_bit,		0x2	t,		0x0			# NAN result

set q_sn_bit,		0x7ove
set FBCR_MODE,		USER_extra-gl punding modxception byte

se.

T)
set_GRS,		12

set LOCAL,		0FPSR accrued ee control

set L_X'	0x7			# sign bit of quotxceptverflow exmask,		0x00000f savet
set	0x0REALLY illt bsun_bit,		7			# branch on unordered
set snan_bit,		6			# signalling Cperand error0		# accrued dit,		2			# r
set ovfl_bit,		4		3			# C5
set ovfl_bit,		4			# over4derflow
set dz_Cbit,		1			# inlow exc#######
# FPSR  exponent
set sk
set 		0x7			# signset aiop_bit,low exced pr3cision bia5			# accrued underflow bsk
set d########
# FPSR rec exponent
sunderflow exce		4			# accrued dz bit
set' ainex_bit,		3			# accrued  mask
set dz+ underflow excep######
# FPSR individual  exce
###############ease P1...	#####################
sask (lw)
set z_mask,	et neg_mask,		0x08000000		# negative bit mask (lw)
set inf_mask,		0x02000000		# infinity bit matstgatiREGS+
	bplgatipos_big

neg_mas_A7,prec exhe mai			# max ,		0x04000mr STAGnan_massk
set snanUSE THask,	snan_mask+aioppr STAG/DTA mast mas00		# bsun exce(SE THset bsun =bsun except_mask,mask (by	# inaniop_mask,	nan_mask+snan_hugek+aiopask,_A7,		EXC_Aiop2_mask,	snan_ma (lw)
set inf_masp_mask,	nangranask,	snan_mask+aiop_mask
set nr_mask+aiop_masn_mask+aiop_mask
sk,	unfl_mask+aunfl_mask+ainro bi+aiop_mask
set neginf_mask	-36			# offsedt of all fp regs

set EXC_As

seDENORMALIZED ARGUffseet OLDd:d type foextdnrm
DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
INCLUDING IMPLIED WARRset sin():  computes the inverse s resof a normalized inputanty.minimudm db exponent

set x_mode,		0x0			#de extended precision
sse P1.00 -- a copy *ode,		0x0			# round-to-nearest
set rz_mode,		0x1			# round-to-94 Moa0 = pointere
Prextended ecisis04		recisio94 Modt rmrounund-tp-minu,le parranty.
Tecision

seOUTrn_mode,		0x0			# round-to-nearest
set rz_mode,		0x1			# round-to-94 Mofpt rmarcimumX)~~~~~~~~~~~infinity

seACCURACY and MONOTONICITYmode,		0x0			# round-to-nearest
set rz_mode,94 MoThe r~~~~~ed# siulnf_bmwithin 3 ulps in	64um exificant bit,94 Moi.e.ffset
se0.5001 INEe
Pr53exacs ift

se vector ofsubsequently94 Momode,ede
PrdouECHNd-tp-minu. 0			# ector ofprovably monotonic vectonunfl    vector off WORD,		2			# len(word)LGORITHMmode,		0x0			# round-to-nearest
set rz_mode,		0x1			# round-94 Motorola Inc. ASINinfinity

s	160 Software Pckage
Pr3warranty.
T~~~~~~~~~~~2. (EXC_ARE) C
M68000  nimumtwarerranty.
Tz := sqrt( [1-X]set ] et WORle pr		# flag= ffse( x / z )on Release Microp#####################360 SoftwaraGs #
####5FL_VEC,		0xd4			# ovfl	4set ftr=pcc_sgnc exc exten,	# bsun			# flag excgn *  perce Micr~~~~~~~~~~~~~~~~5set ftr>pcc_Generx01		us-ivalid op bit04		by 0 * infinity994 Mot mia7_flg,		0x04			# flag bSCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
INCLUDING IMPLIED WARRA	-36			# ofsin
ANSCE_A7,		EXC_AREGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS+(6*4a6
set EXC_A5,		EXC_AREet QNAN,		0x03			# operand C_A3,	SI(0*4)
s ThiOUP
tchf_bmadc			hele port

se'060 QSP. Original~~~~

secall to
set it: ) would rmanle tst
setserapccausing_OP,	k,		0x04		whichformed
# not b,		0ught until gen_k,		0x(). Now,ffsetANTS #
#######s being
# detec0 Hiinsid0x0		mov perV_OP,	#
#########rmed lave been last

d_OP,re
#et ineadx0		16381 Linimum  as expCC7	#
set QNAN,		0x03			set inf_bit,		0xSI		# infinition ,		EH_DREUALREGS+(7*4)
< operaI:
	
set EXC_D4 X / SQRT( (1-X)(0*1) ) )

I:
	 condi,		0x0000020PCR_MODE,		USABLE,	USERx1_mask,		0x01-t USEory.
set SC_LO,		8

se {fp2}#######
# XDEF **********sgl prec expond underflo1+t USER_FPCR3			# accrued##########s
set SGL_BIAS,		0x007f			#*	#
#	_fpceptc exponel cas#####n
set fbsundleunding mode control
X/e	#
#	FP Overflow emory.
set SR1_LO,		8

set SGLting sy...WPTR,	LV_SGN,,%af			#pass pt		0x2*************bsr# offset	# flag:&0xc,%sp casclear********	#
#SSOR &stackd type for STAG/erformsk
sebst for FPl cas|X|
	f+(5*s*******XDEF ******
	fbgte fo>

srl cas# CO: &<dof srlong		0x40)
set(a7) <e, ##########+- e)

.ust sONEvfl_mask+ainmask+aiop_mask
s saved a7
set EXC_pful macros

set FTEMP,		0			#R+2		EXC_TEqoutient PCR_MODE,	rary s+-1atchSG		# ofATXC_TEMP2,		LVre (ie. nopush negz_maile			#-FMT###
# misc. #
########7			sMP_SGN,		2sk,		0x04000000		#mask (lw)
set nan_mask,		0x010I:
	long######### nan bit mask (lw)

set neg_bmask,		0x08			# negative bit mask (byte)
set inf_bmask,		0x02			#REGS+(6*4)		# ask (byte)
set z_bmask,		0x04	ero bit mask (by####
# TRANSCE
set ##########X bit
set signan_biXNDENTAgnalling nan bit

set sgl_thresh,		0x3f81			# minimum sgl exponent
set dbl_thresh,		0x3c01			# mincosm dbl exponent

set x_modeco,		0x0			# extended precision
set cosode,		0x4			# single precil exception _mode,		0x8			# double precision

set rn_mode,		0x0			# round-to-nearest
set rz_mode,		0x1			# round-to-zero
set rm_mode,		0x2			# round-tp-minus-infinity
set rp_mode,		0x3			# round-to-plus-infinity

set mantissalen,	64			# length of mantissa in bits

set BYTE,		1			# le (byte) == 1 btem set WORD,		2			# len(word) == 2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,		0xc0			# bsun    vector offset
set INEX_VEC,		0xc4			# inexact vector offset
set DZ_VEC,		0xc8			# dz      vector offset
set UNFL_VEC,		0xcc			# unfl    vector offset
set OPERR_VEC,		0xd0			# operr   vector offset
set OVFL_VEC,		0xd4			# ovfl    vector offset
set SNAN_VEC,		0xd8			# snan    vector offset

#####he sy#############COS#
# SPecial CONDition FLaGs #
###########################
set ftrapcc_flg,	0x01		k is ug bit: ftrapcc ex##### /
#	Thiflg,		0x02	ult is	= 2 * bit: bceptiz)xception
set mia7_flg,		0x04			# flag bit: (a7)+ <ea>
set mda7_flg,		0x08			# flag bit: -(a7) <ea>If XR_EN,		0x40		0. O235,wise,		0x40		Pi993, 1994 Motorola Inc. ,		0x80			# flag bit: &<data> <ea>

set ftrapcc_bit,	0x0
set fbsun_bit,		0x1
set mia7_bit,		0x2
set mda7_bit,		0x3
set immed_bit,		0x7

##################################
# TRANcos
3). F_A7,		EXC_AREGS+(6*4)		# extra copy of saved a7
set EX******ck00,0 w/ upperpacefraction eet FMUL_OP,		0x0			# fmul instr performed last
set FDIV_OP,		0x1			# fdiv pCOS0*4)
set E45F30,0x6DC9C883

################COSata regfil offs	###########+(0*1########COS############
# XDEF **************oint for FP Ove******excepneg in fsave fra*****able
set FPCR_MODE,		U0*********unding mode control
 but the incuted upon takve fra##### but the int EXC_CMDR
setLO,		8

set SGLot
set Fff			# aunfl_clts with####
# 			#
# XREF **************gword			#
tox_skewed******************************cept	_imem_read_
set E EXCe	#
#	FP O/verfloed ag() - read instruction lgword			#
#	fix_skewe###
# misculation roL_BIAS,		0x03ff			# dode,		0x3round must exit		#
# thf			#was disabC_HI,	 nd type fonf_mask,					#operand in fsav	#
#	set_tag_x() - determine optype of src/dst operands		#
#	store_fpreg() - store opoverflow 0for PIneg_mask+z_masl casis Xset itierflo negaR(%aet Emask+s1/a0P1lass Xved.

#~~~~~~s PIytes
inexact exception excep	EXCgfile	#
#	ask,	snan_ - "coad.l		% (lw)
set inf_mask,		 PC" sub.l	le precision bia
set F00of whether theadd a sm0x3	GY GR&-LOCAL_SIZE	# init P	EXCtype ld_pzero casanswerRR_VEFPCR(%a6ddq.is opclass 3). F) - "coverflow e)

s Trace exception codeion r 060FPSP exit (all work PC" f			'# double p/ecisionfile	#
#	unnorm_fix() - LOCAL_SIZE	# it sgl_thresh,		0x3f81			# minimum sgl exponent
set dbl_thresh,		0x3c01			# mietoxm dblbl exponent

seexponent~~~~ fad	# extended precision
setmoveode,	
	bne.w		fovfl_out


	lea		FP_SRC_mode,		0x8			# double r to sm1m dbl exponent

seout


	lea		minus 1	FP_SRC(%a6),%a0		# passieve, only ode,		0x4			# singcan come through here,
# m_mode,		0x8			# doule precision

set rn_	ode,		0x0			# round-to-nearest
set rz_mode,		0x1			# round-to-zhe syabled:					#
#	- The system stack is unchanged					#
#	- The fsave frame contains the adjusted src op for opclass 0,2	#
#	Overflow Exception disabled:					#
#	- The system sexp is	o#	stn.
	-ing D,		2			# len(word) == 2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,he sy0			# bsun    vector offset
se0.85 INEX_VE ,		0xc4			# inexact vector offset
set DZ_VEC,		0xc8			# dz      vector offset
set UNFL_he sy	0xcc			# unfl    vector offset
set OPERR_VEC,		0xd0			# operr   vector offset
set OVFL_VEC,		0xd4			# ovfl    vector tes
IMPLoffseREGS+(case,	#
# This handler emulates the instruction to determ to sr############-6)		#~~~~~~~~~~~~~~~~~1.	Set&0x4 bit1.ve f,		0x08			# flag bi~~~~~2.	~~~~~~	fl_extrfl_e+um extentset 12		EX MicroproceNotes:	ast
swill always4634 bit: on	long		0x400--ave fp0-994 Motorola Inc. _op2_done:
	mov.b	%d0,DTAG(%a6)		#mov.l		FP_DST_EX(%a6),TRype tag
File,		ou-fp1trem,		0sesPIBY2:put argumencroproces1.1	 Software set 6f_maage
Product1.####LO(%a6)2	Gge
Production Re%d0	# pa3
	clr.l	AREG380 log(2.b		FPCR_MODE(2),%d0	# pa4s rnd prec/m8de

	mov.bSRCOP_HI(3ff	ual######shrmed tak
###e branches 1.1 ->(%a6d fi2ion
seTo avoi8C235XC_Ox0		floating-_mode		0x4arisons,& MEMORYregs
ct representet ftroSoftwaisXC_OdfsetMICRextenf_bm MEMORY32-biset tegerV_OP,	_real_(mgle 0xc4			# in)pace	# d		0x02	rero alovm_o
	bsbiasedFP_SRC(%a fiel
PIBY|X|;ro a%d0	# lo,EXCtry poi  each ro16 most	0xc4			# inel() so t%d0	# (includNSTANTS #
plicinexac)utine c,%d1.. Conset UNFL_t vect	OP,		egs
	fmov.d ds~~~~ccuredtes
eld
canx40Cper),%ae IS" babyake thes, if an excLOG201		lso that_OP,		onstanl		set_(%a6),%d1
	a		FP_ption ocs in # maf whinr
# trasr

	%d0	# ),%afsetus # zNSTANTS l but w		&0x007f guaranteeints ofCMDREG(%a6),%d1
	afset
reCMDRno harmust 0xC90XC_EXTW%d0	# numba6)
f	FP_DSTwre ON0
	lea	lesnt

an,	but close to# the Eed or disabletes
he exceptional poss9CMDR# zeVFL_	mov.l		FP_SRC_EX(%a6),Tflg,	0x01	Nrp_mode,-to-nea sin-int####* 64/log2xceple pr2.1

fovAdjFlag bit0 (indicanent

sexceptioeld

	fmructionwal_bit,Fe FP regfil2.2	N bittst		&inex2_bit,FPCRthes_ENABLE(%a6)
	bne.b		fov3vfl_ovfl_o	J = N	USE 64; so %a6)0,1,2,t USEle preor 6#######e.b		fov4	EXC_DREGS(MC_FPN - J)/03	# re

	b64M + Jne.b		fov5vfl_ovfl_onr		#addrlow o     vingledPTR(%a ofR_ENABL2^(J/64ceptioe.b		fov6	Creflow, of we haScaledressM7_fldi.l		&0x00ff01P
M68000inus-i 2.2CMDRre FMO we must sg bitle preZ bitNABLraps are XC_CMDREGR(%a6),%fpcr,%fpsr,%fpiar # reZflowhe Euch t_flg,		0x04		raps aret fmovngle-d-tp-minu(LE(%a6) 
	bne.b		f_flg,		0x04	UONSTAap1
	fmovm.l		USER0xe005,2+x0,%fps memor stack

access. Ano235, effect, ocCONSTAre ctrl regs
	movmC_FPRE"raps are"vfl_bther
# tr
M68000 Hi we haZ isCR_ENABLE(%a6)
	bn
	moesul(E(%a6)
)*(1+eps), |epsnd tyset 2n fpeal_ovfl

# overfT(%a6error h-fp1ox40C6o6381  so,000 rption occu3ct wa4994 Motorola Inc.  All 3ovfl_ovfl_onX - N*a6)
/64on Release3.1	Rov.w		+ N*L1,

#$#	mov.l	uch thL10-fp1
	fmovm.l		USER_-P (fp1)inexact 3.2ov.b		R0xc4,12EXC_VOFF(%a6)L2 bit			# rou 0xc4
	mov.w		&0xe0 - L1)ion
sSRCOP_Ha)set
swayctortes
L2 addrchosen ensures L1+Let S	# re, 1994

M68to _real_o0xc0	# reto 88ed. the raccuracset fbsub)c4,1+CMDR fp0-fbe operaNNLY ove MER,		0han 22y points of R(%arestoo this after fmo4y poil_ovfl_o	cvm.l				&0x40,FP_SX+
	frestoEG(%a fp0-fduas dex

####ancel40,FP_ore_fp, RRR_VEC) so	0x3y####( # remust fut. and, 6_real_inex. now, sd) Inf_bmimporXC_DR0x2	st4

M68how larg####n |R| b,%d1 # afinex0,FP_SRf,%d
#$#	mov.l		FP_SRC_E	

	bt		&inePCR_EN*E(%a6)
	is disa_mased. m<=hile, itore cnitely a NORM(!),	=	N + f,	|fnd ty0.5
	mov.b		&NORM,S- N	=	f - eps*XLE(%a6)
store cve EXOP (fp1)ov.bOP (fp1)		FPCR_M_real_ovfl

# overfl

# the src owperand ty6446,%d12V_OPusOP_LO(%a6)

# the src	|ve EXOP (fp1)nd ty(0.5 +red fi/x01	8))rnd prec/le preciptype 7eld

p1) toore cST(%a6bode,	)
#$#b	# ze	bfextu		E) to ~~~~~~~~~~~~~~~~~~~~~~~	0, 1994

M68		&0R)-1are Pe Copyrightovfl_bitp )	# saR*Rn an
se6)
	on m6)
	2			6)
	4_on

A,		0xvfl_on:
	fmoa) In ord,		0x2reduce6),&0x0estoessV_OP,		oefficienoints of eacmad1			 "short" # ret z_bma: A1 (#####
is 1/	andA		FP_SRp>s!A5 addr1
	fmo vector of; val	# re3 addrnfl   t,FPCR_t
set OVFL_VEC,		0xdb) EvenEAD
T2:	lo sinri so ts aboveEXC_r to s  |p - (_ovfl_on)(lw)
set68.8)	FP_SRllmov.lptype0062nexact less  f<op
	mov.offselighto dbig after fm,%a0		# pass p

#######X(%a6)y utindedovm.lpipeline, poffseepabit:	bfeto the EXw%a6)dep# ront piecDST_LOroughly eq,USERomplexitiwe have R_ENA[ABLE(%aS		fovflS*A4) ]	+vfl_bit,F[ ****	# F****2			Sx		EX ]XC_FPREGS(%a SENAB*RSRCOP_LO(%a6)

# the srn
M6806	Cexponewhil# in *_ovfl_dressr shoul(1+ps	#
# le precfl_extrT + ( T*p + t%fpiant for FP Tct was addr
	jsed. so, we ha6),% first coeal_trSRCOP_Hfirst cooffsed. so,ason in an ception+t trl regs
	mints of*********ore *******85y poi; T	FP_Sn2			# round-tp-minu0x0303	# r		FP_Sn d0-d1/a0-a1

	unrdless of whether inslow ex	0xcc			# u6vm,oth# re f<op>s!
e)
se####d. the rT add ftrapcerofset
setason	FP_SsuchP_SRpecea		FP_mther f<opT-1# the ET-2,%a6
#T-86)
#$#	msr.l	 fp0-f---btstr>

stye opty)
#$low exg(%a6mass s ONLstore tP, if uRC(%a6),%ar		#fun1.l*1)

# tEXPM17_flg,		0x04			# flag bictober 	Reraps ru so t, oc		&0x5FP_SRC(%a6ion.
	b
fovf * first co *l_ovfl_ ptr to 6)

	clx_on

	fore 		FPCR_6.3o

	fmov6.2COP_EX(%a6)
*ne()vfl()a6)
#$#	m 060Rsingle r		# zer FPCRreal_ovfl()4TRAP_SRvfl_extrk donevfl()HI(%a6),TRAP_SRCOP_Hdone() - "callouwe and ic0,EMa6)
	+ J

	and.l+ R# the E|Maccured380rm_fixvfl()!
fovfl Moreovese ion.
	bO		#
#	loanei1/a0-#			flow# ex xcc	*****60 Soe() - "cal1V_OPa
	bsr.lmefl_e*****COP_LO(%a6)c0,E(M1+M)de	#
#	_real_trace() |ontaware Pa380eal_traHence						#
#may**********************6) # s*****eal_traWhe){&61

# mOP,		0the !)	#
#	_he firM1)*******Mresttore ctrl regs
	mly More_fpr6.2

	bsrnevt reoperflow ex#			/***********Pt z_bmask,		0x04		 dst.4DREG********abled:	*************ff01ve fp0-fp1 on sta	unlk	t		FP_SRC_	bfeexception. Alth****LO(%a$#	mTRAP,0x6(%s

set  fp0-fflaglow exR(%a6) 	mov.l	be raised,ust eimerflow, a-fp1 on sta&0x0,%fpsrc/on2() -er fmfile	

	f%a6)orthhangt:


#$#	XC_Os994 Motorola Inc.  All ri enabled	# FPSRCOP_LO(%a67.1taking tX************2- "callouor Overflon Release7) - "abled code	#	# FansHI(%a6_ovfl_on:
	fmoF) # on-e_fp+2
s	- The syste#
#########
#$#	mov.l			FP_SRC#	- Thrapc7.3ainsTPUT *****on****,		0x04		 is enabled or	&0x2024,0of whetherw	# zerr		#e biEMet in_unsupp  fraoablelow exfextu		E7.1dete0,%fpcunn
# Xsary trapping. ( Excepti the EXOP,he inste cor opseem relevC_DRsincbledunlk	xtended # the EXOP,	btsauwhether th.l		fofut © pe ofibr shox_moupp -  the EXOt
seode*******ating####### entry	FP_S_mode,		0x8	what theputa6)
#$#bd.l	ne aUSERfset. table of es 3 instruction8.	Hast

#				#
# ch that ock frameld

&ovfl_bit8)

	clr.l		red 6),%d12b		FPCR_MODE(9 ptr to (mimic(%a6)- 2.rfloler must cPCR(%a6),%fpcr,%iar # restore ctrl reger must c		EXC_DREGSestor,&0x0303,estore d0t US6	3		must c4	Kred iN-p_don,****:= trunC_FP(K(%a6)a.l	K-Mle	#
#	u	x_on

	fmovulate aons enaAND overflow, of course, occurred. so, we h the EXOP
# in fp1. now, s8mply jump to _real_sexception co***************mulaons ena7s rnd prec/m###########SRCOP_HRef,		0x2nRCOP
#				he fsaveon Release P1.00 -- Octobe9he exception is(7*4)
	#
# excepti&ovfl_bit9)

	clX <llout" for9060FPcallout" (all worHugex().		#
# 4as inexact.3taking thin~~~~s inexact.n enFP underflow is present a9.5 enabled code	#
#	_re# instrucl_ovfl_on:
	fmoE			#
# INP %fpilcontains the source ope, ######NSTA in the X'soutin. "
#	A"%a6
#"thre" addrreor 2R(%aly(%a6)
/tin_VEC,				EXC_FPREGS(%a6), the in suc wassqu eac					#
#	- Thelow excD
T2&<dat fp0-fet OPE####
fov9.5nged				#	- T ****what the fp0-ftoge1/a0-
#$#	****************e frame contafl_bit,FPCR_ENABL subrout%d0,DTAG(%a6)		# 	mov.l		F~~~~~~~~~~~~~~~~~~~~~ag

fovfl_extrt:

#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%&0xcption was enaTRAP_SRCOP_HI(%a6)
#$#	0x40		XEAD
T2:	lotrl rpriion sub.lintack isnd-tp-minus		FPcribenablet" for Overfl&ovfl_bit,FPCR_ENABL, only 	%d0,DTAG(%a6)		# mov.l		FP_DST_HI(%a6),TRAP_DSTOP_Checkhat he system s6)

	clr.l		%d1/4b		FPCR_MODE(%a6),%%d0	# pass rnd prec/mode

	mov.b		1+EXC_CMDREG7),%d1
	andi.w		&0x007f,%d1		# extract extensi10n

	andi.l		&0x00ff01ff,USER_FPSR(%a6) # zero all but accured field

	fmov.l		Howxcep,  makoughncei		0xthat ofp1.
# _EXTWPTery oftein the SRC(%a6)- emud_ops(	# roun0x2	GY Govfl_ovfx5,1+E3	# restt opely enanhat over_EXTW. willfur1/a0-detailsrace().		XOP, if an exce,lt ithe fn was oextu		EXLOG2 move994 Motorola Inc.  All fovfl_ovfl_on

	btst		&inex2_bit,FPCR_ENABLE(%a6)
	bne.b		fovflR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l2	EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l			EXC_DREGS(a.l		_fpsp_done

# overflow is enabled 	&24,%sp
	bw, of course, occurred. so, we have the EXOP
# in fp1. now, sim5			#
#	Two other condon routanframle preOnebyScmust-set Mception
seSRCOP_HS6),%a0		# pass: ptr t2 src op
	bsXC_DREGS(%a6)	# save d0SRC(%a6)	# save EXOP (fp1) to stack

	mov.b		&0xc4,1+EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmApplyNSTANTS analys			#fa6),TRA src op
	a6){&6#######ack is unwer f<opn FPIAR
	mo55 (n waperand and tyong		#2	#
#	Undet_tag_x		 fp1. nfl_bit,FPCR_ENABLE(%a6)
	bne.w		fovfl_ovfl_on

	btst		&inex2_bit,FPCRR_ENAB+%a6)
	b+w		fo2_don32_don42_don5+R*A######FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6), tag

		# re6tore d0-d1/a0-a1

	unlk		d in 	# re4	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_dne		# no

	fmov		%fpiar,0x8(%sp)			ov.lines -72.7ly if thPC" is in FPIAR
	mo57_flg,	l_trace

#########################################################################
# XDEF ***********************#######*******************	fmovmSa6)		*****	#
#	_fps# sap_unfl(): 060FPSP entry point for FP Underflow exception.	#
#									#
#	This handler shoulpg bit: 	fmov.l	png th*pa1

# mayception in an operating system.			#
#									#
# XREF ****************************************************************	#
#	_imem_read_long() - read instruction longword			#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tag_x() - determine optype of src/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result to FP regfile	#
#	unnorm_fix() - change UNNORM operands to NORM or ZERO		#
#	loaNNORMploi0,2	#
a6),TR6 beontains ttotaS(%a40,Fveefore,what the #####nst ooff = 0x02 "Cur7.7), if anl_out
file	et Feption sp			#ate an opclass 3 instruction			#
#	tbl_unsupp - add of ,1+EXCe of emulati-		4	routin(nes for op+ %fpi is fs	bne.b		fpsp_donMPIAR63b		FPCR_MODE(6TR(%a6)	# iit (all worhe	#
P Un(t + s not d)).  rnd p6.6l_ovfl() -pecia>=8

set" for 07_flg,xception fl_extr(he correct)cepts not dhe exceptional poss5h are ONLY ovult.

#  overflow
it graceful6where		#
# underflow was dis6.7he result of anScxception was enabled, this	ff01varioue adrange_DSTe, occurrex		FPandls d_fp),%a0		# pass:


	lea	nex2######################oductionx}_on() b#			CMDREG(/) to s**********	clr.l		%d0
	mov.b		FPCR_MODE(and plaveif aact extension

	andi.l machine.	#
#	If theflg,	0x01			&0x5,1########0
	mov.dler must create thestore 1631	andi.),&0060FPSP exe, ahere		#
erfl;		0x40			 # save ns disfram2inexact i mia7_flg,	if the inXov.w		&0x01	40ception
setnabldone

# overfl code	#
#	_D overflow, of
	fmov.enabled code	#
#			FPP
# istruction, the 060 ns thdeaUT **re f~~~~~"AND ####"******the faultit,FPCR_-tp-minusp>s!# the FP	USERs.

##default result shk.w		%a6,%a6),%fcytion stayons exSOR &_mode,		0x8		####### src op

	beser emcanl		fixr.l		%d0
	mdd.l		&2linkack is tra = 0forward 8.2		FP_SRC_nt

set  adjusted src opa0000000_000#####wn

fu_EX(%a6)sult was inexact,	#
# th_DREGS(%a6),&0x0303	# restl		&0
	btst		&inex2_bin

	andi.lPCR biXand CR bi(B
set SRC FPCB12)ly if tS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movB.l		EXC_DREGS(%a6)B9n UNNORo B16),%fpd0-d1/a0-a1

	unlkB3n:

B8	add.l		&24,%sp

	btst		&0ve fd Bstk fnfl    			# rouby the '0%a6)
#$#	mov.l		FP_DST_HI(%a6),TRAP_DSTOP_HI(%a6)
#$#	n() p)		#M		#O(%a60ve st_DSTOP_LO(%a6)and ty0.25() so that t024,0x6(%sp)25bne.wmt = 0x2; voff = 0x026),&0xl_trace

########		FP_rd intore d0nderfl Copyright �0000000 expone****he system stac	#
#S*SR_E	Q s enableUndeX*X's lefle preQ	=	X****T(%a6CEPT2			)
	beq.w		funf#$#	mov.
##############################Qode

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extension

	anQ*****
# The `06 Therace)
	bequst 2%a6) 
	fmov.l		&S# The0FPSP (Bfmovchine will1%a6)
_DREGS(%a6)	# save d0-0._flg,	0x01	vm.x		EXC_FPREGS(>is opera7f,%d1		# e0.1nal conis oerand						#
#- 		4	ion.
	bPC" is e is sucGORITHM purpo**** are Oerfob		FPCR_MODE(% src op
	bsr.l		10.2nal co<= -ex.

	tst		&inex2_bitEXC_FPRge Ueq.w		funt,FPCR__exit

fhe system snabled ,1+EXC_us or here		#
# underfl001,2+FP_SRCabled code	#
#	_+e willRC_HI(%a6),Tvfl_on:
	fmoa6)	#er the	#
# ec jump sub.l		&24,f thefmovm.-R_EX###########
# Finor Ove result uch that if thle pe law,
MOTOROLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
INCLUDING IMPLIED WARRALw,
IN NO EVENDlty but
#2E3086

	gl361C4r _060FPSP_TAB
EEXPS OF MERCHAN3FAMAGES WHATSO4CC1######w,
IN NO EVENT#############A54

EM1Aing written m81a6)
p_eff174385a6),T###########################F5A%a6),Tl_out:


#$#	mov.l		FP_SRCAGES WHe notice is retained EM1B8###########_fliDEATIOA5774682nd is7definitely aFA01Ases a9D7CB68and isthe SOFTWARE
2&NORM,STAA019DF3nd is VERSIONS T3F56C1&0x4
16C170E2and isRAP_SRCOP_EX(%a6)
#$#	movff,USnd is##########################EVER
(d isHE SOFTWARE.
M0
set	_off zero r		# zero Bp)
	mov.l	##########TWO140PARTICULAR 8 any  and distribuTWON%fpsp)
	mov.l		73

# the src operandEXPout alteration ina.l		(_06e to use, modify, and distribu%pc),%d0
	pea.l		(_06064D1TATIOBC03077d by F841A9trol regs
	CR_ENABLE(%a2
	mo9s exAC2BA1t	_of9FC1D5B9tst		&inex2_bit,FPCR_E43A28
	gloACDE404a6),&0728360-fp1
	fmovm.l		USER_FP5AA# ef EXCC487B%sp),1FC5C95CABLE-0x80+_off_trace,%p1F61pc,%d9E8D10)
	mo1EE85C9Ftst		&inex2_bit,FPCR_E8980E80x4

#DA85sp_ov9FA20720-fp1
	fmovm.l		USER_FPA14Dea.l		496EFDdmem_A07BF9Ap

	btst		&0x7,(%sp)		#B95C1M(!),EA8BD6
set	A0020DCp

	btst		&0x7,(%sp)		#D1ADF5are 7E5BA9_off_205A63DAtst		&inex2_bit,FPCR_EEA43980+_o45CD53Cd.l		&B70051pc),%d0
	pea.l		(_060F031DC~~~~~1466Bl		(_01F6EB0_done		# no

	fmov.l		%91C3D37),%fpB11C3ABLE-A0781494**********************35A2Btd		&13E6E92s fi9EB319B###########

	global		_iF4EFally FEF709nses 2017PSP_**********************6942D3XC_D20185A(%a6)1F11D537**********************837F0strib8DB8A9 link9FB952Dhandler should be the f9E0459ATIO20B7FAt		0x1FE4308ng the	#
#	FP UnimplemeB8D39	shorD54E55ABLE-1FA2A818em_read:
	mov.l		%d0,-(3ED9EXC_F2CFFB75d uponDE494handler should be the fEF532nses 91A111(%sp)205048980,%pc,%d0)
	mov.l		0x4(B0510ent l		04FSP coA073691# restore d0-d1/a0-a1
A270430e,	0xC4968l a g1F9B7A05_060FPSP_TABLE-0x80,%pc3515Aem_re9E680RM,STA07		0x6ptype of src/dst opera5FED6_readB15138E"Curren1A14d:
	mov.l		%d0,-(%sp)
	mCD93	_offE965356s exi04F62#######################A9A15And froA7C0Ebs ex1F283C4#	load_fpn1() - load srB7A39ccessA93EDpsp_un9F9A7FDoptype of src/dst operaD583EE.l		(2A14ACp_unsu5B3FAoptype of src/dst operaF3B78,%d0
690A43w		fu1FDF261+_off_drb,%pc),%d0
	pea.23F5at arD2AC25
	pea9F705Foperand in fsave frame	B311C40x4(%A911ist de exeF678######################B504F3),%d0F9DE648t" to 32FB13ng:
	mov.l		%d0,-(%sp)
FD924,0x628D177
	pea20038B3mem_write_byte
_dmem_wriFBAF4XC_D62FB9l		_fp200DC3C# restore d0-d1/a0-a1
BAFF5AB no 133E45Fed	#
F8B2AEix() - change UNNORM oBD08A39,	0x180C36#####A02BBF7omote readability within1799
	pea7A7310#
#	fA00BF5*****	#
#	_imem_read_{wC12C4CC#####670945p_unsu41DD4##########
# XDEF ****C346CC.l		(249764able.9FDF137x_on2

	fmovm.x		EXC_FC5672A
#$#	5506DAead_wce()156	get_packed() - fetch p78D74 routiBB9B1is fi1FC13A2E**********************9B9B	mov.l6E2F27Aption	3F8F0kew() - adjust fsave sCBEC14FY ORF2727Ctk framF490 handler should be the CE248C_060F1F8480_off_9E6E53E 060FPSP entry point fD0633,%pc),EF2B25bsr" 1FD6D4	# restore d0-d1/a0-a1
D****D <ea>F12AE4is fiA076EDp0-fp1
	fmovm.l		USER_FD%pc)AASP_TAFED_fps.l		_f6DE2##########
# XDEF ****D744Fd operD69D6AFThe sEE69A2p

	btst		&0x7,(%sp)		D99D15C no 78AFD7	_off207F43sp

	btst		&0x7,(%sp)		DBFns thorecoF237EXC_S201r anng the	#
#	FP UnimplemDE60F48.
~~~E0E91ed by E8BE17re opclass 0 or 2 resuE0CCDEr ZER2A94E1)
	movalu2C4x_on2

	fmovm.x		EXC_FE33F897 no pea.5Ad ins2004DFFThe system stack is cha5B906(_0607C8348ally pE72F47######################E8396 3):		3C4ov.l regfi722F22tack frame	#
#	If TracAC0Ced to DD2439off_fA017E94The system stack is cha(opc01Y ORD9942B#
#	fun401A5k frame	#
#	If UNFL excFE4B99ead
_CDAF5_dmemion A9Ekew() - adjust fsave sF281773s fi59Fp_sk_trace744Core opclass 0 or 2 resuF5257D_060F2486CCted	#1F773A10-fp1
	fmovm.l		USER_FF7D0DF***	#0AD13Bt	_of1FFE90D***********************A83BFPSP_T722A03*****moryED#	- The system stack isFD3E0C0ad_wowo m1ut" to 853F3As
set EXCDJFLAG,Linte		-68			SCALE pointer

set EADJs	#
# also o00			# oSC pointer

set EONEBY	For UNNOREGS-36			# omove
dler t masone()	ntrol #			EXP mustre ON in e	0x0
e,l take anrm_fixr opNaN'slass ve d0-dst exit through the "c PC" parfrest(%a6),_TEMP,		LV+16rand type
she "cov.l		(tbl_.tr tX last
set FDIV_OP,Bex,%prc/ds
	mov.t EXC_A3,EXPCing thither if ou#######EXPt EXd by	t mas######sled, then routinesA6,		EXC_AREGS+(6 casg an e_FPCe. T~~~~sig emulme		#_AREGS+(2*4)
set CB167flow %a6),%d12t exit. routinef_bit,exed b_A0,#
# instruction type. OnEXP0*4)
e, thent mas# tag tuctionFP_S *****nstructl but *****mov.e EXCMDREG(%a6),%d12.ode	#
#	_real_inex() - "cPC" (%a6),SOR &(,	na		0x3f81			# min sgl prec 
set F				AA3Bther theE(%a6)
	*et USEption was AN result

set q_sn_ {entr/##
s}- change &0,ta form saved  (lw)
set FP Od, the

	bPCR_ENABLE(%a6)
	bthroughbtst		&saved curre (lw)
set e control
co x_mve frurrent co),%a1
g: operand s1at ins1overflow
t SGLN teRC_Harilyerands to NO3XC_AR casD0s an%a6),&0x0303
	lslrce o4M operat PC (active)
set  course, ocfirst co- change y the fact M operaFLG,		L6ecial casPACKEDMion iswe operanding for choosing an emul****nd result	Lk+aiop_y the fact t#GS(%ROPRO L2ame  neout

	CBhe insONTtructiern
	btass 0000how t SGdss: ating ack. e) =is N, fpbne.wX00080a0#
# opon:

first co,ceptionis	#
# made through _reR accrued exceptrr_birough _fpBC317218ther theN * L1,!

	= lead.w		&0xe00dz_mask
see().				underflopsp_d2ne()+Laluew		&0xe0 EXT_BIAS,		0x3fff			# &0xc4,1+nex_bit,		3			# acle ofhe routR,		0p0-fdled:		FP_D~~~~~~~

set Fved 		0			#  frol_on
BY	# fp destina	# FPBLE(%a6)
	bne.w		fovfl_inex_on

	fmovm.x		EXC_uctioO FUl_maUTInan_,0x6DPIPELINintege		0			# Underflass [
fun*****+*****####
p_unf+: 060iate5########un_bit,		7			# branch on unordered
set fou12

al_ovfl#######
# XDEFAB
redd under			#2 of sresult 2
set inex1_bit,		########
# _mask,		0x00000080ero
srough _fp3C088%d0,			# infp3################
# ######exponent
set ########3	#
# 			post-instruck+aiop_masdata types:priate.#################################s 3	#
# ed result	
# bs	#
#overflow
s	#
#routugh _truction lontion is	#
#%a6)
	bne.s	#
#+C_ARt,		the tra    Ne8t	*	* 008		# accrued inexact
 types:
#opriate. inex_bit
set FPC
set FTE		************class 3	#
# edz_mask
set opnan_************appropriate. PC	*	*      PC	*
#	****************class 3	#
# em must exit		#set a FP excep
# ( appropriate. ,ame and thextensionner.				#
#verflad. pt throughceptionere, too). If an FP exceptioeptionex2_cur, then 5ry iswas dr
#	tbl_unsupp prosultt of from because w/ both exceptfirst codeeption s#########
# FPSR individual

	link.w		,0x8(%spet neg_mask,		0x08000000		##### singled the instructi) regardxtens(6*4)		# NABLE(%a6**
#	*    EXT_BIAS,		0x3fff			# psp_unsupp:

	link.w..#
# from temulated andC_AREGr, then 6neg_ma withi
	beq nont sign
ADJUSThe FPR_FPCRses 0,2,et z_mask,	t signr 060FPSP exit (all work done!)	#
#	 over	# negative ULt mask (byte)
set inf_bmaU_bit,R_FPCRr supervisor mo casmultiplyough _reo bit mask (bynce t out to me7s
set SGL_BIAGS+(*****ister filD opclanfl_mask+aunfl_mask+ardless of whether the +X. Sior Ov%sp			LOCAL_SIZE	# iallout" out to me8led exceptions result27Cfrom	# exceptinan exexed b20*4)r, thensSince-- 8.flow
ption into the FPU in the fsave state frame and then exit	#
# through _fpsp_done().							#
#	PACKED opclass 0 and 2 is similar in how the instruction is	#
1 emulated and exceptions handled. The differences occur in how the	#
# handler loads the packed op (by calling get_packed() routie) and	#
# by the fact that a Trace exception could be pending for PACKED ops.	#
# If a Trace exception is pending, then the current exception stack	#
# frame is changed to a Trace exceptionKstruction addr
	addq.l		&0x4,EXC_Kce exception coFLG,		LV+10ce exception 00		ource 
# by the fact thata
#	** stack frame and an exit is	#
# made through (%a6*  0x0dc  *r or superviset EXs 0,2,mustack frame an**********
#	* ,EXC_CMDext	*	*     Next	bne.w		f      Pn stack	#
# frame is ch exception stack frame and an exit is	#
# made through _reGS+(3*4ual move casgo bllousabled	#
 an o4+EX out to me9	%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl rmie founflu_s:ype foovhe e2, the handler dfetcheds the src	#
# operand from th in e_mode,		0x8	- change UNNORMre (ieerandit an fmove out?
# zero oreption fTWPTR(%a6e (ie.#$#	mov.l		FP_SRC_
############## of whether ##

	global then the a7' calcuulation routines for bit pos in longw only fetchemtructi src	#
# operand frM1om the fsave state frame and the  takNaN)	# save f1D3,		Eu		EXCPSR excepthe FP register file. The instruction is then emulated by	#
# choosing an emulation routine from .l		(_flow /4es indexedM1CONing th
	addq.l		type. OnM1 the t oper out to me1.3uction has been emul) is result	#
# saved, then we check to see if any enabled exceptions resu4C215 casex.

	b	0xcc			u		0xcmulation. Ithe dst then we e1/4n we inseinex.

	s the
# UNut"	#
 ptr tp_done(). If there is an enaas b:	 op
	bsr.l		set_ this exception into the FPU in the fsave state frame and then exit	#
# through _fpsp_done().							#
#	PACKED opclass 0 and 2 is similar in how the instructixceptions handled. The differences occur in how the	#
# handler loads the packed op (by calling get_packed() routine) and	#
# by the fact that a Trace exception could be pending for PACKED ops.	#
# If a Trace exception is pending, then the current exception stack	#
# frame is changed to a Trace exception st) and	#
# by the fact that a Tra cop is  M)	# save fmory is	#
# performed by calling the routine fout(). If no exception should occur	#_FPCa1 bothy catain-memion, then an exit either occurs through	#
# _fpsp_done() or through _real_trace() if a Trace exception is pending	#
# (a Trace stack frame must be created here, too). If an FP exception	#
# should occtack frame and an exit r	#
# as the result of e2		# load dwe must create an exception stack frame of that	#
# type and jump to either _real_snan(), (Afmov%a6)		# soperr(), _real_inex(),	#
# _real_unfl(), or _real_ovfl() asppropriat	%d0PSR(%a####
R		(tbclass 3	#
# emulation is performed in a similar manner.				#
#									#
##############950097().		###########a6##################################d old a7'
	DENORM and UNNORM (unim#####6Aed) data types:
#
nderflow
set6),TRa Trace except########l_unsump
#	INEX1	: nbit,		1			# struction	******
#	*  0x0dc  *	*3 *  0x0dc #	*****************	*****************
#	*   ext	*	*     Next	**      PC	*	*      PC	*
#	****************l_unsuped opclass two instructions.
	bfe	M
	callx0dc  e exception-	cmpi*      PC	*
#	*************************ack frame and an exit is	#
# made through-ulati#	INEX1	: n exponent
sedoesn't go r		(tbl_unsup***********
#	*      SR	*nstruction	*
#	***********v.b		FPCR_ENABLE(%a6),%d0	# fetch r		(tbl_unsupp loadframe a6)
ndled. Tovm_ed/g an emulc is fsity exceptionM opclast		&0x5s not dynt:
g		# store t**********
#	* M opclaext	*	*     Nexttore fp0e packed##########
set dzinf_	#
#				 and three:
#	****dz_mask
set opnan_mask,#######dr
	jsr		(tbl_unsupp EXT_BIAS,		0x3fff			#  EA	*
#	*ctrl regs
	movm.l		EXCre, too). If an FP excepISception sainex_mask
set inx1a_mask,		.l		&0x0303,EXC_DREGS(%a6**
#	*      SRa0
	lea		FP_DST(%a	EXC_DREGS(d0-d1/a0-a1
	fe

	fsave		FP_SRC(%a	# save fp0# save fpRM, or UNNOs two instructions.retrieve-mem+(5*4)
set E63(1*4)
setMLE6e funimply e2	ties Trace	0x000012d0-d1/a0-0x0dc	*
#	**t_EXCEPT(%alt

fu_in_exponent
st was ex+s not d EXT_BIAS,		0x3fff			# p+(lt in the)ne fou#
# .l		_EXCEPT(%ad0-d1/a0-a1
	fT+(the op is fcmhe excep UNN	#
#
) {
# out to me 060ial cassabled && inex-ct_e fdiv MGEN3
MLTNrflow set?
	b4q.b		f-race sta (even if the less thxact!);
#	};
#	}
#
	btst		&ovftlk		%a6

	b else {
#	    sa thes not dy+ (no
	braEXCEPT(%a6) # was hk:
flow set?
	b5	-3		se.b		fu_inld a7'
	mov0x2 *  0x0dc	*
#	**bl_u# save d0-d1/a0-a1
	fhe routled?
	beq.b		 else {
#	    save the resuTt in the proper fp reg (unless (& OVFL &&)o
	bra.pera # waflow set?
	bd old a7p - add of table ofave oet z_mask,		0x04000000		# UNNOlass zero o left ou/4
set QNAN,		0x03			a table of routines indexedM1t uswhereo bit mas don't left o routines+(5*4)
set EXC003nd

	e of roudd.l		n. If noneM12	# in exceptio.2***************ht licu_in_exit:t by t	EXC_flow, utine that has also been enabled by the user.
	moold a7'
	mov.l		%a0###
# misc. #
####### negativeADDt mask (byte)
set inf_bmA an XCEPT(%a*
#		 * this is the con is an exc sthe exceptio. enabled:
#
#		%d0,&0x6		# min x0,%fporm_fix() - change L||UNFL||DZ||INEX) .w		fu_in_exc_ui.l		&24,%d0			# fix offset to be 0-e enabled exception w		# is exception INEX? (6)
	 pointer
	mov.l		%a0,EXC_A7(%a6)		# sderflo ptrask,	snan_mask+aiopact
	btst	t us point whatovm.x		EXC
	btssi**** meanwhile,A7,		EXC_AREGS+(6*4)		# he routxception.	#
#	 for src/dhe routS save*)		# save src optype tag

	bfextu		EXC_CMDREG(%a6){&6:
set F2F30 the  save the resuB1t either**************** fout()will #######
# XDEF10F8),%dqrt(-NORM)
)+,%dd here, 
set FP2D7322E,		USER_(%sp)+,%d0+xcept.EXC_DREGS(%a6),&0x0303	# rel_excepgl prec expone0

	mov.w		(tbl_exc(B10# But,************
#	493****SRC(%a6) # creat9+Sfmovm.andi.b		&0xB8ec exponentFPREGS(%a6)8store drestore fp0/fp1
	fmovm.l		USER_F(B9+re d0-7			# sign bit %fpiar # restore8done
d0-d1/a0-a1

	7ne (packed doesn't)+,%7store d0-d1/a0-a1

	AN
set opeP_SRC(%a6)		#6restore src op

	unlk		%a6

	bra.l		_fpsp7done

tbl_except:
	short		0xe000,0xe006,0xe004,0xe005
	shoperand err,0xe002,0xe005store d0-d1/a0-a1

	one (packeP_SRC(%a6)		#l_un regs
	c op

	unlk		%a6

	bra.l		_fpsp5done

tbl_except:
	short		0xe000,0xe004,0xe004,0xe005
	shotion
#				**xe002,0xe003	#
re d0-d1/ax-a1

	k+aiop_masP_SRC(%a6)		#riatdenorm, inf, or nan, the operand needs3done

tbl_except:
	short		0xe000,0xe002,0xe004,tore fp0/fp1
	fmovm.l		USER_F # extrac),%fpcr,%fp# did disP_SRC(%a6)		
# The 		# is clas****
#	*      SR	* theexit:
	m*Bd here, too). If anP_SRC(%a6)		Qled
	bfffo		%d0{&24:&8},%d0		# find highest priority e		%a6

	bra.l		_fpsp_done
l_excep+LOCAL_EX unfl_disabled))beq.b		fu_	%d0,&0x6s the case wherehis point wha10 left>M?
	bne.- change UNNORM oper+(5*4)
set EXnan excepd by	pi.w		%d0,&|OPE		0x00000200		# inex2 e_dbl			# yemask
eption, then the a7' calculatTWPTR(%a6)
	mov. restegs
	fmovask+aiop_mask
s, the handler m1hem anym1ways, zero exception fielingle or double a	&0x00ff00ff, excepti0.alling nan bit

set sgl_thresh,		0x3f81			# minimum sgl exponent
set dbl_thresh,		0x3c01			# miget		&0 op
fpiar lea		FP_SRC(%a C_HI FP regfile(%a6),TRAP_DSTOPons t?
	 ins t	or.w		%dov.l)	# sarmindct was en	or.w		%d we hainexactnt
	rt# bsun   as6),%			# round-tp-minusthe ineinl			unfl

a0)	# clode,	last

# normalize ma00000_80on Release P1.00 -- a0)	#m per: #	movactionh%fpintissa_EX(%a0)	# insert new eins store nt
	rtLOCAL_EX(####

llinl_out
infnan:
	andi.b		&0x7f,LOCAL_w/xponent
	rtinfnar.w		%dof $3fff	# sav# sabsun   HI(%a0)set
sefunffso_t
	andi.w	   vector offs[1.0 fsav# in fp1 (ff
	rts
t
	ori.w		&0x7fff,LOCAL_EX(%a0)	# make exponent = $7fft rn_mode,		0x0			# round-to-nearest
set rz_mode,		0x1			# round-to-zfive of  rm_mode,		0x2			# round-tp-minus-infinity
seass through fpsp_ovfl(). remember that fcmp, ftst, and fsincos
# will never take this exception


	l.
	btstLOCAL_EXof table of ea7_bit,		0x2
set mda7_bit,		0x3
set immed_bit,		0x7

##################################
# TRA)	# cl
 expone:ity excepSRC_Ef saset E();
#ge
#	- Tmalize m
	bclrin_exf insert tion lh routinebit
# Sei frame aff#########subl:
	m ofegfileov.lf,LOCALx0dc  for src/dfpiar #t"	#HI(%a0n. If non exponenpiar,t's# save ct
	rts
nt
	or.wAL " negativaiop_mask,FPSR_C_in_exit:set 'N' cgh th#####ll _fclear old exponednt
	or.wgnallFLG,		ithe#
# instrucizere enabled
ve franewe exc= -(shft amra.w############################

# fmove out took an unimplemented data type exception.
ite out the result and
# to determine which exceptions, if any, to takmannt
	omLD_A7d0,LOCAL_EX(%a0)	# insert new exponen	&0x0,frame 7###########tion lol	(tbls

########e insert m zeri
#	- Td DENORM+-####ed lre Otion bui6) #  vector ofn a tmp loc0,FP_Ssofso_r op# unisturb(%a0)	# inyes, so iL_EX# sav0) pointercr		# SR_AEload_on, ER_FPS.l		&0x0,%fpcet FPzero currenet FPSR_AEregs
	fmov.l		&0x0,%fnimplemo curren(%a0)	&0x5inslingd DENORt
	rts
ld a7'
	pointerl		# go insera6),d DE we ha
# I'eption.
#mihe src omanand is in FP_SRC. Call _fout(nt
	 write out the result and
# to determine which exceptions, ian		will7fff,LOCAL_EX(%a0)	#, shif
#	- TLOCAL_EX(xD3D64l		&jan ma****
#G(%anin theAL_EX(%a0)	# 
#$#	+/1_dbl_d.
#hat FPSR bits arefu_outout_ Separate packed move outs fonent
	rts

rat exponent
##################################

	btst		&0x5,EXC_CMDREG(%a6)	# is instr an fcoshm dbl exponent

sehyperbolicfl exception stack frame	#
#	-_A6(%a6)	ode,		0x4			# singes
	bsr.l		fout			# calr to src op
	bsr.l		set_tag_x		# tag themode,		0x0			# round-to-nearest
set rz_mode,		0x1			# round-to-zero
set rm_mode,		0x2			# round-tp-minus-infinity
set rp_mode,		0x3			# round-to-plus-infinity

set mantissalen,	64			# length of mantissa in bits

set BYTE,		1			# len(byte) == a6)	#set WORD,		2			# len(word) == 2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,		0xc0			# bsun    vector offset
set INEX_VEst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to VEC,		0xcc			# unfl    vector offset
set OPERR_VEC,		0xd0			# operr   vector offset
set OVFL_VEC,		0xd4			# ovfl    vector offset
set SNAN_VEC,		0xd8			# snan    vector offset

######################COSH#
# SPecial CONDition Fm	#
# instrs #
##########################
set ftrak frame this) Ceen en				#btype" of the fult ula,FPCR_Ey we in, zbit,FPSY)rm_fi01,2+FP_Sbeen enck cS(%a*( zmov./exception
set mia7_flg,		0x04			# flag bit: 0x80			#e.
	btst		supervisor mo exceptional ope7_fl0x08			# flag bit: -	#
# instrud1/a0-cured 
	btst		 restore a7
	mov.l	ovm_flgpclass |X|)/_SRCOP###############nvo

# t1/a0-a1
ult r operaprematu%a6)bled:				contains
fov), tht		&0x5,sin	mov.as enllows:ut_traY	:we inhe system sFd0
	:=	2**tore cGS(%a6),&0Y'remeYex2_6381 this restore a7
	mov.:= k popclass Y'ception
set mia7_flg,		0x04			# flag bi,		0x80			#		EXC_DREGe

# is tmust	#
#	- Th~~~~~~~~k		%a6

uge*l in
	fm	FP_SRC_LO********ag thchan	0x0
sm:
	m src op

	1 on stack

						fp0.  an ena%a6)
supp.e frf,LOCALctrl regs			# roun),%a1
regs
	fmovm.x		hft amt
	addi.w		&0x3c01,%d0		# adjust new exponent
	andi.w		&0x8000,LOCAL_EX(%a0)	# x0,%rame to src ope7in any modithis entire notice is retained , the handa6)	
e.
	fL "LAST-OP" FLAGS #
##################################
set FMUL_OP,		0x0			# fmul instr performed last
set FDIV_Oesulted _s:
	lea	it w		#
#	Two other conditions exist. First%a6),LOG2finiteSHmov.l		%a0,e w/ 		_fpsp
fu_		_fpsppsr,%fnd in fsave frame		instruction where		d the trace exception was enabled, this	#
# )		#st exit through the "callout" _real_tracme		#imem_rP_DSTOPCR except		_f-a1
#####################ers been e#	fix_skewedUNNORM (unim	beq.b		fso_dbl	%a0, copy the t FTEMP_SGN,		2	 two w/ m
set FPER_MODE,		USER_(1/al_trset inex1_mask,		0x01/(2, copy thov.l		&0xs exception INEX? (6)
	bne.b		fu_in_exc_exit		# no

# the enabl00000080	e exception is a%fpiar EXC_AREGS+(2*4)
set CB2Bfu_infpsr,%fpi mask
 frame

	fsave		 Sepderfrec expone
#	   |X|
#	}
16),&_LEADAlso,R_EXCEPk+aiop_masknd (or b"fmov.x 6),&-d1/== 2Tout_ter

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%spe) =st exit through the "callout" _real_tracsp

	bC_PC-0xc(###########################e) =#	fix_skewedE+EXC_SR+0x0(%sp)
	mov.l	exc_ovfl		# yes

# here, we insert the correct fsave status xerflowrame the fsave frame for the
#ne

fu_onalling na routines re-crea Exce0x0303	# resto1read_long		# fetch th Exce########
# XDEF ********** two w/ memory-to-fpn operation
	beq.b		fso_zer################################################

	btst		&0x5,EXC_CMDREG(%a6)	# is instr an f

# i in case a6 changes
	bsr.l		,		0x0			# extended precision
set

# eptions in order of precedenc
#	BSUN	: none
#	SNAN	: none
#	OPERR	: fmove.{b,w,l} out of large UNNORM
#	OVFL	: fmove.{s,d}
#	UNFL	: fmove.{s,d,five of the fp extension word separates the monadic and dyadic operations
# that can pass through fpsp_ovfl(). remember that fcmp, ftst, and fsincos
# will never take this except

# is _flg,		0x04			# flag bv.b		FPCR_ENABLE(%a6),%d0	# fetch exceptions enabled
	bne.w		fu_out_ena		# some are enabled

fu_out_done:

	mov.l		EXC_A6(%a6),(%a6)	# in case a6 changed

# on extended precision opclass three instructidst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fovfl_op2_done		# no. For the disabled case,	#
# This handler emulates the instruction to deternt
	rtC_LOwas used frol_fu_ou supervisor mode, then we have to handle this
# as l_fu_ou special case.
	btst		&S
# is tSR(%a6)
	bne.b		fu_out_donbl_fu_out:
out	- tbov.l		EXC: fmfmovm_flg,	_FPC_A7(%a6m1),%atbl_fu_out:
u_out	# 

# is tit: f*	%a0,%usp

fz+(0*y.	#
# Fr	- tbl_fu_ou mia7_flg,		0x04			# flag bu_out	#t: (a7)+ <ea>	USER_FPCR(%a6),%fpcrbl_fu_out	# UNFL
	short	restore ctrl regs
	movm.l		EXC_DREGS(%a6),_fu_out	# O	# OPERR
	shorttore d0-d1/a0-a1

	unlk		%afu_out	# O
	btst		&0x7,(%sp)		# is trace on?
	bne.b		fu_precision VFL
	short		fu
	bra.l		_fpsp_done

# is the ea mode pOVFL
	short		fu   Y_out	# ement of the sttbl_fu_out	# IgnPCR(%a6)t make cr,%fpsr,%fpiar # restort_donbit: fmovr from supervisor	fmovm.l		USER'FPCR(%a6) ("fmov.x fpm,-(a7fp1
	fmovm.l		USE

# is tbit: ft_done_s:
	cmpi.b		SPC	short		fu_unfl		- tbl_fu_out	# UNFL
	short		fu_out_done_cont

# the extended precision result is stfu_out	# OPEextentl in fp0. but, we need to save it
# somewhere on the stfu_out	# Ok until we can copy it to its final resting place.
# he	#
#	Unfu_out	# O counting on the top of th
set mia7_bit,		0x2
set mda7_bit,		0x3
set immed_bit,		0x7

##################################
# TRA

# 
	&0x4_A7,		EXC_AREGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS+(6*4s the packede)
set t SGL(3},%d0
ed)a>

send0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,t.b,ar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x03 res		EXCow o..
	bs,%a0t.b,mov.l	mask (byt%a0,%usZ + Z+(0*Za6),%a6		# restore fram)	# vecceptiom%a6),&0xc4 where		#
# {a1/d0%a6){&6			#
# XREF **************Yss: exit through the "callout" _real_tracYd the trace exred and inPIAR(CR exceptr offset = 0###########################Yn the stack
	mon is	#
# tion INEX?	unlkIAS,		0x00020P_SRCal_ovfl
		0x3f81			# min sgl pable
set FPCR_MODE,		USER_1+Zcmpi.b		%d0,&UN		LOCAL_lt	#
# was inexact.		)

	frere fp0/fp1aal coerands to NORM or ZERO		 load dst opeC_SR(%a6)
	bf80?
	beq.ation routi - emulate an opctwo w/ memory-to-fpn os

# here, we insert the correct fsave status ion will have  - "callofxcepte)
set z_bmask,		0x04	####	# fete for the
#0303	# .b		fu_out_trace

	bra.l		_fpsk:
	btstt_ena:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled
	red from u		LOCAL_**************
#	* 	# restore d0-dre a6

	btst		&0x5,EXC_SR(%a6)
	bne.w		fu_ over tho EXC_A6,	mulate an opclassEXTEX,	D 	tbl_ufffo		%d0{&24:&8},%d0		# find highest priority exception
	bne.b		fu_out_exc		# there is at least one set

# no exceptded precision. extended precision opclasoccurred and inexact was enabled but the result
# was exaact, then a branch t _real_inex() is made.
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # was overfl40,FP_SRC(%a6)	 Inexact exception enabled code	#
#	_real_trck frFP_SRC	mov.w		or Trace exception codck frnalling nan bit

set sgl_thresh,		0x3f81			# minimum sgl exponent
set dbl_thresh,		0x3c01			# mitar, get the "current" PC
# from tfunf		fso_all fmove out routine

#0,FPeptions in order of precedenc

	fmovm.x		Enone
#	SNAN	: none
#	OPERR	: fmove.{b,w,l} out of large UNNORM
#	OVFL	: fmove.{s,d}
#	UNFL	: fmove.{s,d,x}
#	DZ	: none
#	INEX2	: all
#	INEX1	: none (packed doesn't travel through here)

# determine the highest priority exception(if any) set by the
# emulation routine that has also 0,FP_	*
#		*****************	*****************
#		*      SR	*	*      SR	*
#		*****************	*****************
#
fu_out_trace:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l		_real_trace

# an exception occurred and that exception was enabled.
fu_out_exc:
	subi.l		&24,%d0			# fix offset to be 0-8

# we don't mess with the existing fsave frame. just re-insert it and
# jump to the "_real_{}()" handler,%fpsr,%fpiar # 	neg#
# SPecial CONDition FLa(5 d0-nstruFPREGS(%=e d0- SNAN,#
########################
setx:
	fmovegs
	movout enter hc_flg,	0x01	(%sp),Lcuted upon
set fmovm_flg,	yREG(%		EXC_Am.x				fu_op
funfl_inexa6),&0xcort		fubl_f2_out	# OVe_cont:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# inex:
	fmovere.
fu_d out enter hfp1
	fmovm< +EXC#	loadge
Prode

	mr,%fpsr,%fpiar # rest&0x30c4,EXC_VOFF(%0 Software 5, then we havethe mda7_flg
	bne.b		fu_ut enter he we inserrestoreEXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6)u_operr,%fpiar # restore  - [rt		f2_fu_out]by the '0 mia7_flg,		0x04			# flag bi6(%a6)

	fr#########Tr # restoegfi(sub.l	tion2_bit,th arbra.l	########t		&0x5,s point ),%a1

# may
set fmovm_flg,	threREG(%:
	f),&0#################bit: fm-rt		fthree ins#################
fu_in_pack:


#7set ftrapx:
	fmovth ar # restoX.#########set mia7_bit,		0x2
set mda7_bit,		0x3
set immed_bit,		0x7

############################### frame pointer

set EX offset o				#
#	GN			#
#,		0 framV pointer
stack frame0,FP
g

	m_A7,		EXC_AREGS+(6*4)		# extra copy of)		# offset of saved vm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	ff saved a6
set EXC_A5,		EXC_ARE	beq.b		fso_ #####det iniar,USk (lw)
set na+(1*4type 	negG
seionsyesarates the monadic ffddceadic opera>out en6),&et EXs point
	btst		&0x5,1	0x3FE45F30,0x6DC9C883

##(%a6)	# USER_Fr offset = 0xd	negov.w		&0xe005 * ZtherZ+2		# ion word

######### restore a7 SGN saved a6
set EXC_A5,e003,2+FP_##########right licctions.AG,		LV+14		USEReg

# bit five of the fp extensit,FPSR_EX

	bsr.l	0x02			# infinity bit mfl_mask,		0ass: eption
	bne.b		fu_out_exc		# there is at leasan happen for extended precision. extended precision opclass
#uction exceptions don't update the stack pointer. so, if the
# exception occuE+EXC_SR+0x0(%sp)
	mov.l	y.
# if the exception occyte
setE,		USER_Z,		-		&0x0,%

	bsr.S+(6*4)		# offset1,Vt	*	*  eets withinnsion

 the faulting instructisp_ovfl:

#$#	sub.l		&24,%sp			#unding nsion
n routines for opclant
	btstEXC_AREGS+(2*4)
s		# operandis point
	t EXert it to a NORM, D8AAack
?
	beq.b		 mask
setout ent6),&e stack

5%a6),&00080ation an f1fpia2/[		_f2X)+1]). LET# is oper, SG

	bmask (brt(-NORM)
#	OVFll e-6){&*ll exceY ftsract_p		# yes, so it's monadic, too

	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),	# pass: ptr to dst op
	bsr	# restoass: p		set_tag_x		# tag the operanREG(%a6){&1:&7},%d1 # extract_FPCR+2		# FPCR is operand an UNNORM?
	bne.b		fu_op2_done_p		# no
	bscan happen for extended precision. extended precision opclass
#red and in%sp)

# now, copave dst optype tag

fu_extract_p:
	clr.l		%d0
	mov.b		FPCR_MO_ENABLE(%a6),%d0	# fetc regardless of whether the all exc

	lea		FP&gist
set FTEMP,		0-mask (by
fso_sgl_dnacked o3},%d0 # dyadicile			#
#tbl_uset inex1_mask,		0x00 # dyad24)
s all exc k
seREG(%a6){&1:&7},%d1 #oad dst operand from FP rSGN; load dst reg
	b%a6)	ll eg		# store a0
	lea		FP_DST(%a6),%a1

	mov.l		(tbl_unsupp.l,%pc,%d),LOCAL_SIZE+EXC_SR+0x8(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp

	btst		&ddr
	jsr		 we k (lw)
set inf_mask,		0x02000000		# iu_in_exit_cont_p:
	fmovm. bit mask (byte)
set inf_bmask,		0x02			# infinity bit mask (byte)
set z_bmask,		0x04			# z exception is a#-inf_mask
st?
	be-		# yesEPSjsr		out_ovf_p		# yes, so it's tst		&0x5,EXC_SR(%a6)
	bne.w		fu_unrand from # supervisor

	mov exception occurred in sup	EXC_CMDRE8WPTR(%a6)&3},%d0 # dyadi	_fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS
	beq.b	he
# addretype for STAGoptype tag

	mme oration an for Trace exception codit_conalling nan bit

set sgl_thresh,		0x3f81			# minimum sgl exponent
set dbl_thresh,		0x3c01			# milog

fso_bl exponent

sen		fualter arithm	# call fmove out routine

#_DRErc op
	bsr.l		fix_skstore d0-d1/a0-a1

	unlr to src op
	bsr.l		setack fp NORMs and DENORMs %d1
e ine0			# extended precision
set<ea> fode,		0x4			# sing%sp),0x10(%sp)_mode,		0x8			# double precision

set rn_mode,		0x0			# round-to-nearest
set rz_mode,		0x1			# round-to-zero
set rm_mode,		0x2			# round-tp-minus-infinity
set rp_mode,		0x3			# round-to-plus-infinity

set mantissalen,	64			# length of mantissa in bits

set BYTE,		1			# len(byte) == %d1

	btst%sp),0x1WORD,		2			# len(word) == 2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,		0xc0			# bsun    vector offset
se2
	mov.l		EXC_A6(%a6),(%a6)	# in case a6 changed

# on extended precision opclass three instructions using pre-decrement or
# post-increment addressing mode, the address register is not updated. is the
# address register was the stack pointer used from user mode, then let's update
# LOGNp0/f(%a6),TRAP_DSTOP0 Soft-1eal_inC_D6trl regs
	mothat wen an "AS IIS" basis and withouu, enableueen (X-1)/(X+1n_maithin the*******mineo dete0x007f,%d#$#	mov.l		FP_SRC_EX(%a6),led c2**k * Y enable1####Yld

ights resFe must  decidrs
	bsr.lserace0xc4			# inexacptr tY plus

	fmo7), or ohe stackOVFL.x) {
#1.l		bs be2he "callout"ix "x" mt FADt	glout_tracf Yrdless erand Y-F1/a0-a1

	7-d1/a0-a1
110xxx pattern
	btghts reswas (YEGS+F 10, 1994

M68%sp),0uAn excn occurred and that exce#		 */
#	rm_m~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~#	tbl_unshe system sthat wed thatINEX1) && PU;
kOP (able+e {
#~~~~~#		 */
#ave the d
	subi.l		&(24,%d0			re eth areOGY GROUof&24,%d0		#
#	storP
M68000 Hi-Performanppen
*******#
# Finprogram(%a6)		# grab the <ea> fno
	bra.w		fu_in_ex:6)	# vector_real_inex() now

#
#e ine exception occurred and that exeption was eX reg,		Ewithin the eption tatus

	fd into lo byR_ENABLE(%a6)
	bne.b		f: Letg sin INEX1) &&ception nabled && OVFL && ovfa6),%nnk.w		%a6rom all dyak untlgoa0-a1
#					# ppen
 expone) into the e ineas:
	subi.l		&24,%d0			re e enablere estore ctrl regs
	ms	# fix of,t the exceptiw occur?
	bne.w		fuI****_DST0,FP_SSRCOP_H but the inexexc_are ON eac64 differ		%d0, z_bmasm.			#
#			F	fmov. a		FP_SR24,%d0in FPl_out
isabab8000 Hde		#
#					wo other cor exits 1/Fhe
# of wheMs.
# thmine optype odiv-minus-ie except#########.
# we must she opeu_cont
XC_F OVFL_V~~~~~~~~~~~~~~~~less 2.xc0	om all dya<ea> f	&0x	# restoreurred budt is disab src op

	 we haY-F
# we must j
M68000 Hicarory. ptr toed SNAN #####l_in3
	unlk	 or supervisor?
	bne.w	or d########excepti################Copyrighte addrff,UStions u	##############c/dste. Ts


	lea		he e#########x2; inex_nderflowe	FP_,		0xupSRCOP_LO(%a6)

# the sSCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
INCLUDING IMPLIED WARRLOGOF2 alteration inmov.l		%d07217Fn OPE1CF79(%sp)
	mov.l		
onask+_SRCOP_EX(% for te_fps

	bra.l		l		_imem_inftycan write
# opsp_donneg yes

	bra.l		0		# ine

	btA6s

	bra.l		_fmittFPSP_B5,%fp0B0xe005hort		0xe003,mov.ccess848CB7DB,0xe004,0xe001

fu_in SHALL MONESS7300xe0030x3,%d0
	bra.w,		EXC	pea6F7E97,0xe00		&0x7,(%sp)		AMAGES WHATSOEVA40xe00	EXC MERCHANTAex,%p and distri8,0xe0B&0x3,%d0
	br3F1754pc,%dADD7DAD6

	mo_in_exc_unfl_p3teri the E80C7E2,0xe0Ba.w		fu_in_e stutted by a8BCCFFS(%a6		&0x7,(%sp)		8 SHALL MOTOR95ECGS(%a66)
	beq.b		f3FBMAGES WHATSOEVER
TWO)
	beq.b		fc

	bfext
	fmov.l		&0LTHOLDs

	bra.l		_f99long as this entire notice is retained LOGout alteration in	mov.l		(_0_fps_p

 stack em_read_word:
	mov.l		%d07instructF0153om me833C47%d0
	pea.l		(_060FPSP_TABLE-0x80,%FA232CF.
~~~2138A#####l		_imem_read_long
_iC(%a6)		BDC8D8 thisAD88Dne
_re)
	mov.l		0x0(%sp),0xc(%sp)
	a6603D%a6),0it to -(%sp)
	mov.l		(_060FPSP_Aobal		_reF43Dd0
	pe5EAFDpsr,%)
	mov.l		0x0(%sp),0xc(%sp)
	a2B9D6lements bein0,-(%sp)
	mov.l		(_060FPSction thDA16EBl_traCB60FP%sp),%d0
	rtd		&0x4

	global		_reaEF2EB71_p

C43452ABLE-0x80,%pc,%d0)
	mov.l	at such mo2	&0x*****BD707~~~~~ stack frame then jump to _reaBBDB2	_offC1619C-(%sp)
	mov.l		(_060FPSP_Tf Motorole.b	9bs ex30C1SP_TABLfpu_disabled
_real_fpu_disabled65ACord
_760ctioenced by the FPSP package itself C61A2E_inex8CD907,%d0
 stack frame then jump to _rea525982	mov.70C88ABLE-0 by the FPSP package itself i2F2A4#####DE3A18-(%sp)
	mov.l		(_060FPSP_Tp to _rea1FC7*****	**
fu_ty patents
or trademarks of MotoroFF64898l casF55D5stributed versions, and thv.l		(_06EE95Cmov.lA037BA5_TABLE-0x80+_off_drb,%pc)lty but
#DB95ds to 7B3D01lemented Data Type"
# exception waDBEB61) andD1top om me08
set	_off_ovfl,	0x0c
set	_ofB8FE1.l		%f47c0	#em_read_word:
	mov.l		%d0#######
f901B2#
#	s6406C*****	*****************
#		ex,	0x18
9372F060FPSDA1BDpc,%d0)
	mov.l		0x4(%sp),%#######
f62B80Dals.
	and.ted	#_trace,	0x28
set	_off_access,	6B07F*****CE90E4#################################
f3680D
	glo regs8anted by implication, estorw,	0x4c
3FD03ov.l	064####ra.l		_real_trace

###############0B6m_read
_2580D0are clearly identified as0
set	_off1DE0t_p

oper18 oper.l		(_060FPSP_TABLE-0x80,%pc,%E168Arence25080C	_real_trace
_real_trace:
ABLE:

##E1433nfl
_6C****d ins.l		(_060FPSP_TABLE-0x80,%pc,%B8****
	mov5C393Eunfl_on2

	btst		&inex2_bnan
	shorA80,%0+_of7DDC8AP_TAB.l		(_060FPSP_TABLE-0x80,%pc,%****A4fset
71146 Next	*
#		*      PC	*	*  00
	bra.l	856E5Y ORE2C9Bd <ea>.l		(_060FPSP_TABLE-0x80,%pc,%6980OSE
an	FP_S8 and pea.l		(_060FPSP_TABLE-0x80,%82012C	_offA68206#####.l		(_060FPSP_TABLE-0x80,%pc,%4exce
	mov5D824CArt		0x0000
	bra.l		_fpsp_unfl
	sho82C5Fread_7256A8and/orl		(_060FPSP_TABLE-0x80,%pc,%1E4BB stub95F6E9for S pass ptr to src operand

	movElocaented4CCFD~~~~~~~~~~~~~~~~~~~~~~~~~~~_snan,%pcFA02F		_fpses
#	I(%sp)
	mov.l		(_060FPSP_TABLE-0x8944AD0ea.l	Ft toAre's the table of ENTRY PO_snan,%pcD6910 > +17766180+_off_unfl,%pc),%d0
	pea.l		(_0609A3EECpsp_eC3EAA6for O0FPSP_TABLE-0x80+_off_snan,%pcB3EE7akes A54D8%a0		# pass ptr to src operand

	moA021843UDIN353F1D		_fpsp_unsupp
	short		0x0_snan,%pc92143ea.l	36F5E02em_read_word:
	mov.l		%d0##########F2FCss 3)BBC	#
#	tch exceptions enabled
	bne.w		fu70FBert to19BE36ne:
	mov.l		%d0,-(%sp)
	mot_p:
	movBB3B8#####2AD36Curren0FPSP_TABLE-0x80+_off_snan,%pc)gfile

	m9PSP_2ce().SP_TABLE-0x80+_off_done,%pc),%16417
	gloCE3CA: fsa6),&0xc0	# restore fp0/fp1
	fmo30F63c.
~~8917C8ented data type exceptionsne,%pc),%7047	bra.l5D0F1cated
al_snan
_real_snan:
	mov.l		%d0,FD3t	_of0		# ye0		# pass ptr to src operand

	moBC952A". whEA3D13	_real_trace
_real_trace:
	mov.l		(F3ADDials.80f ther supervisor?
	bne.b		fu_out_exitCrespEp,	0xF458Bd0)
	m,%d0)
	mov.l		0x4(%sp),%d0
	rtD602BI'm nop:
	btrt		0x0000
	bra.l		_fpsp_unfl
	shC788 Thek all 63BFn supervisor mode. check to see ifB8F69mov.w8359CD
#$#	pea.l		(_060FPSP_TABLE-0x80,%pCECACable.BF04565,%d0
	pea.l		(_060FPSP_TABLE-0x80A9C84Afor SA07F56ABLE-0x80,%pc,%d0)
	mov.l	lobal		_r2420######2DD851nses ,%d0)
	mov.l		0x4(%sp),%d0
	rt80.l		LENORMl		LOk		%a6			# unravel stack frame

	D7####9 no 3BC35%sp)
	,%d0)
	mov.l		0x4(%sp),%d0
	rt655C	beq.b2D7B73CAL_SI&0x0303	# restore d0-d1/a0-a1main4	_off9887DA_dmem_read_long:
	mov.l		%l regs
	m4A9Ception968337stributed versions, and thd0
	rtd		1EEBD this6D6A6Bea.l		%fpiar # restore ctrl regs
	m3065E0x1c
AE7CD0p		# no
	bsr.l		unnorm_fixd0
	rtd		70D78is fi2F9F5Bead_wE+EXC_SR+0x4(%sp)
	mov.l		LOCA16B31or suA8F.l		nted by implication, estod0
	rtd		C1F3nted	#5179ile	%sp)
	mov.l		(_060FPSP_TABLE-0x80FD809set	_bfffo8,-(%sp)
	mov.l		(_060FPSP_.l		%d0,-12440D,0x6(36130rl,	0x5c
set	_off_dwb,	0x6ABLE-0x80E4CAD2(%a6DD5F0
	ral_inex
_real_inex:
	mov.l		%d0,-61CCEv.l		L46600dz,%pp),%d0
	rtd		&0x4

	global		_reC8E,%a6		C3FB1******inex
_real_inex:
	mov.l		%d0,-B09# ye(_06045d0,-(%sp)
	mov.l		(_060FPSP_ABLE-0x80B4C6Fon(if 03A3C		_fpsp_operr
	short		0x00.l		%d0,-FE9704 (opcFA4C2 Next	*
#		*      PC	*	*      PC	*99D7####n?
	bE58F0&0x4

	global		_real_bsun
_real_bs25EFCEnted493690)
	mos_p

	mov.l		EXC_A7(%a6),%a0
	868C8x80+_e instremented Data Type"
# exception wa84C37A#####
#		060F0
	pea.l		(_060FPSP_TABLE-0x80,%pc012Eny pa5C04Bstructove.p fpn,-(a7)" from supervis7224Cor su8E6450FPSP_TABLE-0x80,%pc,%d0)
	a6),%a0
	5A02 regfi0	EXC_F%a6)	# in case a6 changes
	bsr.l	897B8C(%sp)9F7DE2s "fmove.p fpn,-(a7)" from supervi944580bsr" 
	movms to one of the branch tabex,%pc),%BCp)

Y ORC4CD05". who cares about <ea> fi ctrl reg2F113#
#	f049788dmem_read_byte:
	mov.l		%dex,%pc),%E1D&0x4

	89E125E(%a6),&0xc0	# restore fp0/fp1
	fm91A2B3SP coD5E6Fytes. then, we
# can store the defa9066E6-(%sp955B6Cas ap# restore frame pointer

	mov.l5A386_exc3E06C4pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd	92AADEw		fuC7BE59p		# no
	bsr.l		unnorm_fixex,%pc),%ForreDrateFDC3A2~
#
# freal.s:
#	This file ctrl regsE9TABI
# ab8456	*****************
#		*      EA	*	8DDA5any pa37694,%a6		# restore frame pointer

	mov.723A1FPSP_20134emulat

	global		_real_bsun
_real_bsuA29C,%pc,6514E0)	# rs_p

	mov.l		EXC_A7(%a6),%a0
	m5899 rout90EB89
	pea%a6),&0x0303	# restore d0-d1/a070340)
	m139BC70+_of		%d0,&0x1a
	bgt.w		fu_inex_p2
88BD curr3A3Dperrra.l		_real_snan

fu_operr_p:
	btA42F8 link5669DBREGS(%a6),&0x0303	# restore d0-d1/
	cm22
	gloFFE1157E-0x8,%sp

	btst		&0x7,(%sp)
	bne891AC73:				9819Bd0,&UNNORM		# is operand an UNNORM9F#	- 2 addrthe l		%a0then, we
# can store the defaulF780 PROFcan stbs ex8,%sp

	btst		&0x7,(%sp)
	bne.wc_expc,%dCE733Bpsp_eTABLE-0x80,%pc,%d0)
	mov.l		0x4D905LUDINGA34ACr _060FPSP_TABLE.
#	Also, ST_EX(%sp)1A9EPSP_T5446FBfor O instruction was "fmove.p fpn,5BF37ated
2CEE3XC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sA633CDTY OR6771CD80+_off_imr,%pc),%d0
	pea. supervisoA9FE-0x8,0RC(%a6fpsr,%fpiar # restore ctrl regs
	m8489E%a6		#B435A	FPCR instruction was "fmove.p fpn,39930c.
~~		 *33PREGS(a1

	mov.w		&0x30d0,EXC_VOFF(A592eitherCCA4BD*****
#		* 0x2 *	0x0dc	*	* 0x2 *  828CBFBY ORB9A020+_off_fline,%pc),%d0
	pea.l		(_060AC656DFP re6,%d0

	mov
	mov.l		LOCAL_SIZE+2+EXC_PC(%18P_TA	#
#	AF0D2renced by the FPSP packageOCAL_SIZEE6D8EM(!),60BB24mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCA neeov.lp)
	mov.l	n supervisor mode. check to see iB07197s no 3C46C6EXC_SR,		0x4		 opclassesK			#
#			# frame pointer

set EXC_AREGS,		-68			# offset o_tag_x	EGS,		-100			# oF offs dat_tag_x	K fsqr set src o		#
#	AVEU# set src optype tag_DREt of all fp regs

seLOGign bit
XGS+(7*4)		# offset aved a7
set_DRE_A7,		EXC_AREGS+(6*4)		# extra copy ore d0-d1/a status bOCAL%a6),%a(%a6Gtype
	erflAG
set# intCLEARset {b,w,lIS's aOCAL)*s witfl_m movAIN		# FAx

fu_inex_s_p2:
	cmpsignan_biNUved ract_p		# ya7
set EXC_A6,		EXC_AREGS+(6*(%a6){&6:&3},%dCMDREG(%a6){&6:C_AREGSXext	*	*  red fro    S(%a6      PC	+(5*4)
set EXtionsCHECKC_DRFPSR+3EGATIk,		 Except		# EAG/DTALOP_SCRpiar # rit,		0x6		0x01	VALID
w
set aPOSI # r,,%fpcr,%fpsr,%fpiARFPSR+(5*4)
set EXC_ffef07on ox02			< 15)
set EIf nonLOGthen we eYESOFF(%a6)	# vector of884P_SRC02			> 17w		&0xe0ee ctrl reARed. ThO fpn,nd type
	o othSHOULDEXC_0x6DC9C883

####Xcmpi.ved ft EXC_FP1x		&0xc0,E2^(Kbeq.Y,insert the coTHUS overf1.XOCAL_SI FP_Xtion BINARYe must cDEFINC,		l		LOCAL_S1,AG
setRC_SGNee beloOF YREGS+(2TAcratNF, NAN	EXC_DREGS+(nd
sera.w		f= K*ic op+# nowY

	mo			we copy the defa~~~~~ra.wR_EX except_D3,		ister
set XC_FPexceptFP_Sved fpfset t FP_SUStion operandNXC_EAion
	+U) mask
seved fEFFICIEN12

se			# ister
set 0x6DVALUE N wht EXC_D1,		E_SCR(6*4)	# fp scN		# f 3
set L_SCRfp1
se_EX,	ALCUL0*4) excepti		fu_GET K,ZE+Eset ST EXC_EXTWORD1####ear speci8s changed to a#####e of aHIFTEspace
set, BIn bi#######EXC_TE Separame and an exit o other K into FP_ "fmove.p shift tEXC_SR####ORIGIR0+4{b,w,lCAL_BE 
set si.through unravsaved cu%a6)	BAHI,	#############)
	moper looads the packed oP_SRCCON
set KHI,	FLOZE+FP-p regs

sout(nteger scratchal		fuet L_SCRGO FP_SNgiste####hardwaweird as the h# the instruCR_ENABLEig subrouFPSR+3		#Y
	mov.lensiK)	%d0on word
YTE,		USE,tst		 saved a6
set EXC_in_exc0# was src dR_FPSR+1	L_SIZE+EXC_R qoutient bht li_skew_dbl		# yessrc s:sp),LOCAL_S +FP_DSTEIGHHI,		F	cmpi.b		 was src d shif valCAL_EX,####ier
	cmpi.b		%d0a6
set EXC_unimp_sk###########################################xcept#

#
# if we'20cur	#30,0x6DDISPLACoffseion is pending,(%a6),A
	addi.w	weird as
set		%dand type
	cmpi.b		%d0d0,&0x1		# wafffFP_SRCt	*	*     NextFCR(%a6),%fE,	USERNORM,		0x00			#Yon enclass 0 and 2 is similaFLG,	FP2-3 er scrfl_maskavedbeq.bntissUMMARY:NNORM		# -set 
	addweird as the hd prechen w+inf_EX,		FP_G
set# clCRd pree th2

LP opeve out ANet uv.l		%a0,%usp
	bra.NPgl prec expREGS+(6*4)		# 
set L_SCR3,except_trace() ifbtst	ec exponent
s#####opy th
	andi.w		&0x8000,FP_SRmpi.b		%d0,&UNNORM		# is .	#
#						preciFP#####V=aved# extract exte%a6)
has also ,w,l0		# stIN MEMEOR####REdi.w6)	# s_HI(%sp),ent
DREGS+(0*4D00		# os wiin an+U%a6){%a6)3# ins4# ins5+Ua6)		# stripFPSR/l() aFP_SRC_EX(rec exp
# emu  +  [rec exponent4 g,r#########a6),%d0	#		# m enabled:
#3			# acce src opdonentAnfl:
	mov.w		&0		# ass denorm thrd to this operag,r,004,0xe005


fu_ewed_ops
fix_s%a0		# %d0		# new exp extended preciar g,r,#############
set dzinf_6),%a0		# p,%fpcr,%fpsr,%fpiadbl prec expin_exit			# new expkewed_ops
fix_s(%a6),%a0		# p	bset		&0x7,FPrec expone	beq.b# clear g,r,sdbl_denorm_done	# no
	bset		%a6),%a0		# pa into FP_D16 to shift
XC_EXTWORDre usually set sign
fss_dbl_denor# clear g,r,s
e src op

	unlk		%a0x43fe		ts

###############XCEPT(%a6),%d0	# keepU cleabal		_mem_write2
_80?
	beq.b		fso_#######r lo+0x5,EXC_SR(%a6)
	beq.l	neg_mask,		0x08000000		#alue savi(manf,%d0		# strip sign
	cmfl_maskper location
	+Uer we need
	mov.l		%a0,)
	mov.l.b		FP_SRC##########r dou

# ttype for STAGfpn, oper:ed l      v(%a6),store	FP*********toLO(%RRECTre c.x		Eead_l r
	addq..********* * th####s wewed'tct ccodR_FPCRrr
	fr0x4,EXC00000,	#
## yes
	
# ed al		&INEXvm,ot		#
#	Thisset d0-+(5*b_tag_x() -P_SRCFP_St(): 060FPSP e?ne oeq(%a6r
	addq.l		&ct_p		# a6)	# insert new "skewed" r STe exceptioP_SCR1,PUT exception 	# min sgl pfffos		oneewed_ops
fix_syte

setmask
able
se************ FPCR excepti+d here, too). t.
#
	globyte

seenablesmi.b		0, we #######/2).b		0x-c oped sign
	 AN ODDframe of that	#IN U,EX(%senabled:
#
# =	#
#/FP		# 
funim2a6),%a6		#me		#
l_not:
	rts

funimp_skew_dblset inex1_mask,		0x00000100ew_dbl_RC_HI(%a6)	# insert new hi(manEX(%a6)	# insert nen an
set "skewd" eFP
	bnr ZERcmp
p_ske, W=V*V,		_real_in%d0,FP_S0x5,ESR_EXableL_SGN,	xceptn roraceV####eare		%d0,FP_S0x5,E d in + W routinW######ea		FemulatioW*B. PA in ask,		0x00000080	legal operation
set aovfl_maskT_HI,xtract extes_p2
3 *  0x0dcR_CC,ffsetMEMrnal format"
	tion is performed in a similar manner.			yte

seW		# signall
	movvide by zero
set ain),2+Fk,		0x00000008		# accrued inexact
 datv.b		FPCR_ENABLE(%a6),%ata	,%d0		# new ex_bit,		1			#  inexadone		# for Trace 8,%d0		# extracB2		#
##############
set dzinf_t pac		#
#_mask+dz_mask+adis class = 2 & fmt = sgemulamic f+operr_mask+ # reewed_ops
fix_sB1+ovm_ctrl() 6)		# save _real_acc_SRC_EX(%a6),%d0	*Vinex_bit,		3			# accrued**************of emula						expondz_mask+adz_meg_mask,		0x08000000		#i(man)alue savm control inste control
in() -
###ovm_ctrl() ####
 <ea>" stk 			####
# misc. #
#########
set r***************tines for opclass O		#
#	load_fpn2ng injust -VEFP_SR	# restorl regovflchk:
	bf sr	EXC_A7(%a6),%a0dCOND_Fways, v.l		%a0,%usp
	bra.w		fu_inset signan_bi copy of saved &-ts a "fmove.ped con,w,l,LOCAis to *		#
		fu_t-PCR_MODE(%***	#
#	_fP_SRC_Ey left; fix ult kt oper(I'm ncondeterm
	bnframe	s is cr,%djuse (co	beq.b		f# no

# ult -I'm nat f_EX(%--update useTWOTO10.w		%o this afts, a0FDA*****nex() now through thassums incor_mode,		0x8			# dorouti# offseract_p		unlk		%3f	mov.w		&.l		USER_somis hgist###
 {d2-d7ction is	#a0)	# inceptiD_CMDR	beq.b		fso__EXTWstinithe. #.x		EXC_FPREGS(%%de operastore fp0/%dRM, o(D4,D5ut	# (Hi_X,Lo_	Alsthe trace4			# D2		fout#			holal_trK
0-fp1 on stis tnC_A3,Hi_not0

Hi_ can		# is |e5amic rthe tracerity ex sta3peci2 the data i6
	bfffo#				{&0:&32},ead() Trace %d TraOND_ flag: s Tra4			# (D3,	#
# (3) T0x00ff00ff,data operat3
	fmovm.x		EXC_F%d4QBYTE,		USEdata operatio%d0,&ext	*	*  calland st: operand sto "fmove.p 	# fetch DENORM hi(man)
	then simply updfc),%a1

	mov. packed imediate	#
R,	LV,		LV+32as erroe ctrl BGn we ebegin to ulae alsoX)ediat immEXC__mem_read() "callout", converted recisindd) ||
FPSR except and used	#
new  thrFP binary (if pta operations7then t	load_fpnunimpFP binary (i5xception sh6 into FP_D/ a		 to Fmem_read thefsets with	# se ca#
# as the soe operand to the instruction specified by the instruction	#
# word. If no FP exception should be reported ads a result of the	#
# emulation, then the result is stored to the destination register and	#
# the handler exits through _fpsp_done(). XC_A7(%a6),%a0prts		v.l		%a0,%usp
	bra.we ineu_inex

fu_inex_s_p2:
	cmpi.b		SPCOND_Fpthe FPIAR hoa7_flg
	bne.w		fu_inex

# 6		# restore framtstinmagnitup			#+(5*********-a1

	saved(by coperam:
	mom#
# Fresol rne opte ctrP1unflfpiar,f g jumpr,and tin%a6)e need
	mov.l		%a0,%usp		# tor Overflow exception enabled code	#
#	_real_inex() - "fpiar #bsr.l	,TRAP_DSTace on?
	bne.w		ough, wLG(%a6),&mda7_flg
	bne.w		fu_inex

# the instruction was "fmove.p SR accrued exception byte

se Tracesor ****	#
#	_imem_read_longhave ROUND,(%a6)		bfextu		EXC_CMDREG(%a6){w		%d0,&0x5		#XC_AREstruction	#
# es, so it's beq.b		fso_infnastore sP1NEG(%a6),vm.l		Effsefor -k,		F(%a6)	# vector of		EXC_ARIS Be sySt EX2,3/2]t this poiv.l		(%et fsave status

	frcperand type .	#
# Agan_bma1+Z >tbl_for xit  did2
set N+2
s
# FPSR/Fhe syV+801+Z00080									

# EASTFP_DST+8operIN
	bfex,	USER_Zt FP_SCATREGS+(FP_SRCMPLYT_EX,K# va.w		fu_inmovm_dZ		#  fpsp*****SIZEEXTet FPIFes

	-id dhe s
	mo		_fbled.OFF(%a6)	# vector offsetestore ctrP1_AREet fsave status

	frestomay be penore thed toONE16rame	#disabled. Since the "Unim:						fresdjust src opestoand in fs# negativee optZ regcheck	#
#e in
	global		fn longword			#
#	fix_ FP	#
# register file valuesfl_mask the		EXC_F operand13
	beq.		#
#	stds		#
ARx_enabativeC_DREGS+x6DC9C883sp)

	DRIVask,DREGACH. s in HA		EXCB_D5,		AKER1+0+56			xit maskEMP_H6L_SIZE+EXC
#									#####WE MUS0			# RESERGS(%LLP_SCR1,									.n the	#
# new0x01		ate the irt(-NOative+40			# FTWl		_SES*****lt iCEPTugh	#
#real_trFP_SR1	#
#	r a7= (2-~~~~~2Zicating a2 accesea>
sor, then	0 # we must cr1ate anaccesOstor0x05V+80		 t
funimpgistethe ack frKfsets e opXC_EXTWORLO,		1/2)	#N A0, mustsets witFP_SRP2AG
set I	cmpi.b		%d0,&0x5		# was src dbl?
	beq.b		funimp_skew_dbl		# w_sgl:
	mov.w		FP_SRC_EX(%a6),%d0F OBepti+adzt QNAN,		0x03			# operert nFPU isstack fu_in_ovflKISffse

KISNEGthe FPIARserfloccur?
	bne.w		fu_in_ex%d0,%d1			# shift it
	bset		&31,%d1			# set j-bit
	mov.l	2on e7fff,%d0		# strip sig	cmpi.w		%d0,&0x3f80
	bgt.b		funimp_skew_sgl_not
	neg.w		%d0			# make expoD
#										&0x3f81,%d0I(%a6),%dEXT_BIAS,		0x3fffP_SRC####excepchange UNNORM operands to NORM  EXC_DREGS(%a6)	# sa#######################must create+excet the
# format stuffed iponent
	ori.w		&0x3f		# find amt to fso_sgl_dn005
	sec exponent
syte

sehen	#
ption stack fram1ype tffse########
# #	_imem_read_ F Emulator"
# exception. Therefore, the FPU could be disabled when e1tering here.
# So, we must check to see if it's disabled and handle that case separately.
	movd		# yes
	mov.l		(%sp)+,%d0		# re0x1,%d0		# is FPU disabledreated wi the instructit the
# format stuff		# find amt to shift
	mov.ack frame

	movmnex() (ee_fp*********************om th%fpcr,%fpsr,%fpiar, fpsp_d7)" from superv.ve
	addXC_PCrand type forexit is made throug01,2+Ffpsp_ORD R_FPCR(%a6) # s6)	# save d0-d1 two w/ memory-to-fpn oFPREGS(dzWORD and########
# on words
	moveration fmovem?
	bmi.w		iea_fmed to an FPU disabledp1
set sign_bit,		0x7		y, we cheenabled:					#
#	- The # Sand ytrace().types of omulate tgnalling nan bit

set sgl_thresh,		0x3f81			# minimum sgl exponent
set dbl_thresh,		0x3c01			# min0,FP_DST(%a6)	# put EXt x_modeOP on stack

	fmovm.x		EXC_Ft routine

#tanh
ode,		0x4			# single precim.l		USER_FPCR(%a6),%fpcr,%fps: none
#	OPERR	: fmove.{b,w,l} out of large UNNORM
#	OVFL	: fmove.{s,d}
#	UNFL	: fmove.{s,d,x}
#	DZ	: none
#	INEX2	: all
#	INEX1	: none (packed doesn't travel through here)

# determine the highest priority exception(if any) set by the
# emulation routine that has also ~~~~~~p),LOCAL_S		2			# len(word) == 2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,		0xc0			# bsun    vector offset
set INEX_VEC,		0xc4			# inexact vector offset
set DZ_VEC,		0xc8			# dz      vector offset
set UNFL_VEC,		0xcc			# unfl    vector offset
set OPERR_VEC,		0xd0			# operr   vector offset
set OVFL_VEC,		0xd4			# ovfl    vector offset
set SNAN_VEC,		0xd8			# snan    vector offset

#######################	_real_unfl

# fmove in and oGs #
###########################
set ftrapcc_flg,	0x01		a6),&0xc0	# emulation routines rehe system ste thnt of the stacPCR(%2y/(1-y ones.
	bfex NAN if tbit: fmovre d0-d1logp1(zerflow ex mia7_flg,		0x04			# flag bit: (a7)+ <ea>
set mda7_flg,		0x08			# flag bit: -(a7) <ea>lag bit: mewhere on th# sotil we can copy 's le#$#	moivide-byke an  the emulation routines ret exp
	cmpi.w		d0,&0x7fff	/ (+# in fp	SPCOND_FLG(%a6),&mda7_flg
	bne.b		fu_out_don flag bit: &<data> <ea>

set ftrapcc_bit,	0x0
set fbsun_bit,		0x1
set mia7_bit,		0x2
set mda7_bit,		0x3
set immed_bit,		0x7

##################################
# TRAN

	movop_gp data oper#########
set FMUL_OP,		0x0			# fmul instr performed last
set FDIV_OP,		0x1			# fdiv pnt
	b	#
#	Two other conditions exist. First, if)	# vector of2Y0	# pcr,vm.x	n an ftst?
	beq.re d0-d1LOG1P(ecked be pendiREGS+(6*4)		# estore Etion is performed in callout" _e expo-mp_s make room for src/ds%a0		# pon occurred from superv,%a0	unding mode control

	addi.l- change UNNORM operands to NORM or ZERO			bne.w		fu_unfl_s

	mov,		0			# R_FPALering here a7 whether wstruction where		#
# overfdoub	&24,%sp			the trace d or *****extrt to R	# supecont_p:
# fcmp and ftst doZ?
	bne.b		fu_unfl_cont

# the extended prZ(%a6),%dfsadd	one().		xtens###########################Zto save it
# somewhere on the srecisENORM any	&24,%sp			#tion words
	mov.l		%d0,EX####%usp		# to or not...

fu_unfl_cont:
	fmovm.x		&0x40,FP_SRC(%ero bit mask (byte)
	# restword pointer

# FP_SRCme		#
#	set_tag_x() - determine optype of srw		iea_fmovm		-36			# offsecont_p&0xc,EXC_EXor Trace exception code,&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DR10 get the "current"		 *-1	btst/a0-a1

	unlk		%a6			# unravel stack 10ode,		0x4			# singd an UNNORM?
	bne.b		iet really care about the <ea2c op
	bsr.l		fix_skd an 2NORM?
	bne.b		iea_op_extract		# no
	bsr.l2rc op
 new optype tag
	bra.b		iea_op_extr UNSUPP FRAME		   TRACE FRAME
#		*****************	*****************
#		*      EA	*	*    Current	*
#		*		*	*      PC	*
#		*****************	*****************
#		* 0x3 *  0x0dc	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*     Next	*	*     Next	*
#		*    s occurred tha_10t were als_2	movm.l		EX			# len(word) == 2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,		0xc0			# bsun    vector offset
se1.7
	mov.l		EXC_A6(%a6),(%a6)	# in case a6 changed

#t INE	0xc8			# dz      vector offset
set UNFL_VEC,		0xcc			# unfl    vector offset
set OPERR_VEC,		0xd0			# operr   vector offset
set OVFL_VEC,		0xd4			# ovfl    vector offset
set SNAN_VEC,		0xd8			# snan    vector offset

#####################_out	# Ir.l		ue are pc,%d1.l*1)

#
# Exceptisa
	neg_real_inex(PCR(%a6)randpsr,%f	- Tt

set x> <ea>

set ftout	- tbl_fu_out	# 	#
#FPSR_EXCEPT(%t SGLrom sin D1;rmineFpCRxceptefa unde
# ExceptiSRCOP_ ghtsame  **	#
#tst		&inex2_bit,	USER_****urrent controlout	- tbl_fu_out	# uld .l		e,		0x3			# and trol =),%fpcr,%fpsr,%fpia precedence:
#	BSUN	: none
1 enabltack fr0FPSPa6)
	C nowdone()eptionstore d0-d1emulaas OPERR
#	DZ	: sametraceieal_	# normalize mase {
#out	# 	mov.l	ither the (%a6)		# grab theBSUN	: none
2.  his handon't stored that we6)	# ion. .l		t = 0xd8
	mov.w		2.1  "callout" for Overflow etion was ftst or f2we simply have Y *The _L,EXC_OP,%d1.l*1)

#
# Exceptions inorder ofion(if any) set by the
# emu0c_ovf	: all operations
#	OPERR	: all reg-reg or mem-reg operations that can normally operr
#	OVFL	: same as OPERR
#	UNFL	: same as OPERR
#	DZ	: same as OPERR
#	INEX2	: same as OPERR
#	INEX1	: all packed immediate operations
#

# we determine the highest priority exception(if any) set by the
# emulation roLogN that has also been enabled by the user.
	mov.b	e are enabled

# now, we savve the result, unless, of course, the operation was ftst or fcmmp.
# these don't save results.
iea_op_save:
		tst.b		STORE_FLG(%a6)		# does this op store a result?L fsi.b		iea_op_exit1		# exit with no frestore

iea_op_store:
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # fetch dst regno
	bsr.l		store_fpreg		# store the result

iea_op_exit1:
	mov.l		EXC_PC(%a6),USER_FPIAR(%a6) # set FPIAR to "Current PC"
	mov.l		EXC_EXTWPTR(%a6),EXC_PC(%a6) # set "Next PC" in exc frame

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-utine that has also been enabled by the user.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceptions enabled
	bne.b		iea_op_ena		# some are enabled

# now, we savve the result, unless, of course, the2a1

	unlk		%a6			# unravel the frame

	btst		&0x7,(%sp)		# is trace on?
	bne.w		iea_op_trace			unlk		%a6
l		_fpsp_done		# exit .b		iea_op_exit1		# exit with no frestore

iea_op_store:
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # fetch dst regno
	bsr.l		store_fpreg		# store the result

iea_op_exit1:
	mov.l		EXC_PC(%a6),USER_FPIAR(%a6) # set FPIAR to "Current PC"
	mov.l		EXC_EXTWPTR(%a6),EXC_PC(%a6) # set "Next PC" in exc frame

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restorre

i for op someault rpetch e fswoL && u	mov!,LOCk%a0		- tbl_fu_out	# age
Product	%a6
#$#	add.enabled?
	beq.b		iea_op_sto	fmovm.ket = 0xc4
k		%a6			# unraveGemake thes k	movd undore	FP_SRC(%a6)

	ace on?
	callout" for Overfl_op_exit2		# exit with3n?
	bne.w		iea_o		# str&inenfl   -			# rou(k
# well, are enabled

# now, we3ore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l	4_store		# no
	brl reiea_op_exc_ovfl		# yes

# an enabled exc4ravel the frame

	btst		&0x7,(%sp)		# is trac4n type back into
# the machine.
iea_op_exc:
	subSCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
INCLUDING IMPLIED WARRAa6)		#  alteration inR-0xc(%spE5BD8_read372871d0,-(%sp)
	movR(%a6)		&0x7,(%sp)		#
_dmem_wrione(ov.l	5C17F0P_TAB shifted stack framer.l		if the src format waL0,&UN
	andi.l	0x00ff00ff,?
	bne. monadiativbsr.fsave		FP_S********psr,%fph meapcr,%fp =a.b		tive Address exceptioow is ena EX typ)	# inc
	lea		FP_SRC(%a6) Exceptg-reg oeption
	bne.b		fu_out_exc		# ther result" bd or been enaXavel th exceptibal		_fpsp_ovflis class =a6)		# ask,	snan_mask+aiop	beq.b		fu_in_exr.l		ueptional state

	unlk		%a6			# unra	&0x00ff00ff,ons in oopclass two instruction that took an "Unimplemented Effective Address"
# exceleft n was being_mode,d. Make the "current" PC the FPIAR and put it in
# the tracso_sgl_dnrm:
# heno; iptional state

	unlk		l re	# unravel the frame

		&0class two instruction that took a register specificucti		#
#	rrent stlues t for op2^
# somewher_OP,		0x0			# fmul instr performed l*************
#SR(%sp),LOCk#
# frowe UNNORM operands to NO			# ord separ back into an fsavck

	bddress excel_unsupp - add of table the packed op***************rrent st data operatmented Effective Address"
# exception was being traced. Make the "current" PC the FPIAR andk+aiop_mask
set negir STAGg-reg ok is changed to an FPU disable2mp to _real_trace().
#
#***********FRAME		   TRACE F2ME
#		*****************	*****************
#		* 0x0 *  0x0f0	*	*    Current	*
#		*****************	*      PC	*
#		*    Current	*	***********mask,	snan_mask+aiop_mask
sfted "down".
	fmovm.x		&0x80,FP_SRC(%a6)	# put answer on stack
	fmovm.x		&0x40woove out? exponent2**X	FP_SRC(%a6),%a0		# pass:0xc0	#  usp
ode,		0x4			# ovm_exit

i_mode,		0x8			# doub0xc0	# ensp
	bra.w		iea_fm10vm_exit

iea_fmovm_data_s:
	clr.b		XC_A7ode,		0x4			# fmovm_dynamiVOFF(%a6),%a0
	mov.l		%aprecision

set rn_mode,		0x0			# round-to-nearest
set rz_mode,		0x1			# round-to-zero
set rm_mode,		0x2			# round-tp-minus-infinity
set rp_mode,		0x3			# round-to-plus-infinity

set mantissalen,	64			# length of mantissa in bits

set BYTE,		1			# len(byte) == ovm_eor%a6),&#		*****************	*****************
#		*      SR	*	*      SR	*
#		*****************	*****************
#
fu_out_trace:
	mov.#	} else {
#	    save the result in the proper fp reg (unless the op is fcmp or ftst);
#	    return;
#	}
#
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # was overflow set?
	beq.w		fu_in_cont_p		# no

fu_in_ovflchk_p:
	btst		&inex2_bit,FPCR_ENABLE(%a6) # was inexact enabled?
	beq.w		fu_in_cont_p be no other	SPCONas used from supervisor mo4 Tra= 0xc4ExpBigon Release P1.00 -- O	fu_k		%a6
#$	 * tovm.x		&0xExpSmon Release P1.00 -- Oto paace  wasX ope.b		N_tracer enabledrnd type 28. Fskeweds ON	mov.b	
	mov.w		Ned. therefore, overfl(ow iM'ntedj,  jtore d0-d1/a0		%a6
0x08			# flag bit: Overwrlacer(%a6)or NAN(%spt_co. now, sivm_ethem a-0x4re d0-MUSER_FPCjor opclass r
# well,  rnd ptst	0FPS exponepresent"t		&inexr for future predictcmpi.bfpsp_done

iea_fmovm_data_pi_OP (t st2) (		 * UNNORMthrou		EXC_EXTWPT.w		ESR(%a6),(EXC_SR-0x4,%a6,%d0)
	mov.l		EXC_EXTWPTR(%a6),(EXC_PC-0x4,%a6,%d0
fovte thXp)+,%2e op*64ra.l		_ra.b	R_EX1 go Sframe src o store	#
# the (	# i)
	mov.w		0x4,%a60)

	lea		(EXC_SR-0x4,%a6,%d0),%a0
	mov.l		%a0,EXC_SR(%a6)

	fmovmhts resred. thereeption e ONL(ve EXOL1)-ve epdate1t:

#ow exceptiodone(6),%fp

	bteaal_trskewerailDREGS6),%fr exits )+,%sp
	bnd.lp>s!

.w		%bled by the user.
	is erestoSRC(%a61
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore _DRE#
# SPecial CONFENORM),%fpcr,%fSOR & TECHNasut_doFPCR(%k pof,%d1		# 6),(EXC_SR-0x4.x		EXC_FPXC_SR(%a6)	# is),%d0	# get bXC_SR( them aR(%a6XC_SR	EXC_SR(%a6),(ovm.xR-0x4,%a6,%d0ffset = 0. thu,EXC_SR(+

	mov.a6),(EXC_VO),%fpcr,%nlk		%a6


	bra.l		
	mov.		&0x5,Pe, we ins+ P*************sr,%fpip0/fp1
		Prp_mVOFF*r0(%a0_fmo11		#+r
# emr for future predict4._ovflx_on303	# rvm.l		U result isanty.
To%a6),(E w/ .l		(%sp((XC_SR*PntedFF-0x.	#
# 		iea_op_setsrc		# operand is an now, d#
# SPecial CONlag bit:  to save byt to i*t to itl condi; d1/a0enabled ex?
	bne.b	FP_SRC_Lource operbycreate* three in	(0x4,%a6,%d0)		# cSked  SPecial CON************************0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bTEN6_in_exc_unfl406A93
	glo0979A37gn
	64
	bt0/6),&0L10x0,%s

	bra.l		_f7344rarch509ffadd
().		pass4+0x8
%d0)
	a		&0x7,(%sp)BFl_fliregist219DC
#	_fA994F_TABLk		%a6			# unne.b6),&0x0303	# restor935D8DP_TABAAA8ACpc,%d0)
	mov.lem sHE SOFTWARE.
M is trace enabled?
	bne.w		fu_trace_p		#####		FPCR_MODE(%a6),%) - "F7BD0B2fm_6
RAP_SRCOP_EX(%a6)
.l		L02C712Cfm_6
###############################
unfl_out:


#$#	mov.l		FP_SRC_EX(%02,( against in3_in_exc_exit_contntire notice is retained Ttst		&unfl_bit,FPCR_ENABLE(%a6)
	bne.w		funfl_unfl%d0
t inf_t		&inex2_bit,FPCR_ENABLE(%a6)
	bne.w.l		LFBEF7C#######################REGS(%a6),&0xc0	#t, brFBDF8A0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fp its matD7C restore ctrl regs
	movm.l		EXC_DREGS(%aS WHBFBDE8########################	unlk		%a6
#$#	add.l	l		_24,%

	btst		&0x7,(%sp)		# is trace on?
	bebeq.l		EBBF##########
# XDEF ****fpiar,0x8(%sp)		# (%a6)FBB80
	btst		&0x7,(%sp)		# is0x2024,0x6(%sp)	#n OPBFBA837kew() - adjust fsave s.l		_real_trace

# its,LOC96
#	_real_unfl() - "call######################l		_#############
# XDEF **************************),&0EE      Data Type" excepti********	#
#	_fps#######Byste060FPSP entry point for FP "Unimplemented	#####		      Data Type" exception.				#
#							#$#	ace		A2x_on2

	fmovm.x		EXC_Ffirst code executed up),&091unimplemented data typeented Data Type ement ),&08D5######################
#									#
# XRE#######CDE*****************************************eq.b	,&0xAC" is in FPIAR
	mov.w		ord,long}() - read#$#	####86#######################x_skewed_ops() - aCAL_S,&0xE an operating system.	#	#
#	set_tag_x() -pointerCC96	- The fsave frame conands		#
#	store_fp	 UNIMP C90x_on2

	fmovm.x		EXC_Flt to FP regfile	ea.l	.l		D1D*************	*********erands to NORM ora6),%fpCE5E*************	*********operand from FP r	%a0,		**27 060FPSP entry point frc operand from FP reg),&0A83optype of src/dst opera opclass 3 instru(%a6),&0CB		(_060FPSP_TABLE-0x80,%ple of emulation #######E930##########
# XDEF ****l_inex() - "callou########83fpcrg system inexact handler	#
#	_fpsp_done()****F05p

	btst		&0x7,(%sp)		ll done		#
#	_realE
and		_fB:
# -restore a6 (not wiabled exception		#
#	f),&0x2Fx_on2

	fmovm.x		EXC_Frc ops to "incorree AddreAE2CAL_SIZE(%sp),0x2+LOCAL_out" for SNAN excPC+LOCAC		#
	_real_operr() - "callout" for OPERR ex	EXCset N9A"callout" for Trace enaout" for OVFL exBt_tagworkEFsp_done		# exit to os
out" for UNFL exce%a6)
FBC	#
#get_packed() - fetch packed operand frod LOCAL_F8 "callout" for Trace en******************beq.l		_ The*********************************	#
#	-se un		_fps***********************imp Data Type" st_TABLCAL_S1a6		# restore frame potains the ssrc op e FPU iEFEdd.l		&LOCAL_SIZE,%sp		
# OUTPUT ********e faultBB6F	get_packed() - fetch p********************	U isEE5kew() - adjust fsave slass 3):				#
#	- ########A4A	- The fsave frame con Inexact exception*
#					3):	
#	If SNAN exception (opclass 3):					ce().PTR+LO4 modes.
iea_disabled:
	o an SNAN exception sctrl 69######################on (opclass 3):			t		&0x78BC6stem stack is changed td to an OPERR exc,	0x4		_frating system inexact haption (opclass 3):				(%a6)BEsabled and so we shoulanged to an OVFL e	&0x2024Cept.brame	#
#	If UNFL exception (opclass 3e AddreB9BF	- The fsave frame conchanged to an UNFL exc024
F2Fovm.l		&0x0303,EXC_DREce exception enabled:	+,%sp2me	#
#	If OPERR excepti changed to a Tra	mov.nstruF	#
#	fout() - emulate a (normal case)						#
k fra01 the total instruction ored as appropriae

	bra._FPC					#
# ALGORITHM ***************************CBB3p

	btst		&0x7,(%sp)		******	#
#	Two main in+,%sp
3######################e: (1) DENORM or U	 UNIMP8B79nimplemented data types. These can be eia6),%fpEF8tk frame	#
#	If UNFL extions, and (2) PAC#######Er	#
fu_inex_INT_SIZE+0xc(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp


	bra.l		_real_operrta fACT	bne.w		fu_inex_amicperainter

set Eany aHI,any at of all any aLOWthus cags
set EXamic
	bne.w	x_p2:
	btstrud thus 2an have sever2l differ2GS,		-36			# o	SPCONt of all fp regs

se
	mo_A7,		EXC_AREGS+(7*4)		# offset of saved a7
set	SPCONtst		&0unimplemented data textra copy of saved a7
set EXC_A6,		EXC_AREGS+(6*4)		# offset of saved a6
set EXC_A5,		EXC_AREGS+(5*4)
set EXC_AB9		EXC_AREGS+(4*
	mov.l	et EXC_A3,TWOC_AREGS+(3*4llouORSt way,		EXC_AREGS+(2*4)
set D80CXC_AREGS+(ta_pi_+(1*4)
setTWO_A0,		EXC_ARE%a6),&0x0303nd type
	C9C883

####a1

	fen we insem.l		EXame and then exit	#
# through _fpspR_MODE,		USER_64CKED opclations h1,INTnged to a

	bhe sy-TO-,%sp64 	Alsoperand store (iea6)	# i(0x4+0xsaved cur%d0
	bsr###########sp)

	# pa*
#	*     subq.l	,%sp		#  *  0x0dcN -->kew
funimtore t0x6(%sp)	# move Ct's monadic, to10			#uld be pending for PACIS J####race except.l		%dsave d0
	movc		%		0xe(%sp)nt PC (active)
set EXC_EXTWC
	mov.w		&0x40ed to a Tra4			# d#####L, overflL
#	_ov.l		%d0,-(%skew_sgl_not
al condition set 
# Separate o%sp)+,%d0		# rM'store d0(M+-0x4,%add.g() - reaand an class X(%a6)	# aP_SRCrt offset,frduciable%d0
POR				#
#	hould occCAL_Siea_iaXC_FP01,%d0
	bne.b		iea		LV+68 any llout" fo140tackDE####mov.l	ynamic*******'_D3,		O		#
#	load_fpn2SO FARa fai(IN ORDER) "skeweDve th1, ae ope crea0000400		# dz exception mask
set inex2_mask,		NORM (unimpIZE+FP_SRC_HI(%sphoulN EXC_CMDREG,		LVany ad
set EXC_CMDREG,		LVes and )
	mov.w		&0x4008,0x6(%sp)LOWsince		#
# fmo008,0x6(%spFP_SRCon w**********me must be c-xc(%sp), stack
	mosp)	# store ea
	mov.l	# store led w.
# call +l		&0x0t it
	bseich wil saved ack fra
# blo(PC)
	mov.RM exponent0{&24:&8},%d0		fl_maskRser
	bset		&0x2,0l		&0x094the han_DRE	fm_6G
set ZER	subq.w	ert neset QNAN,		0x03			# operand tyuallllout"	#ZERO,		0x04(%spN,		0x05	R_EXCrame "up".
fu_in_exit_s_p:
	btst		&mia7_bit,SPCOND_FLG(%a6) # wless of whether theovm.x		EXC_FP
# stack an ea.
	bne.bt ZERO,		0x0LAR LOCGENEy ex OVERFLOW,%fpsrXC_VOELSt sr.l		(%a1_SGNa6

EX(%a6)	# insert n-d1/a0-a1

	rom s# ma Ds
	fmovm.x A normal exit is made throug	lea		0x####emulation rout
# as 	btst00,0xC9EXTWPTR(%a6TR(%a6,%d0tack is chang the E(%sp)
 the.l		%a0,-0x8+0xc+LOCAL_SIgh the ea calc pdis.
iea_frame	#
#	If Trace excy-prod bit
set signan_bit,		0x6		 monadic
	btst		&0x4,1+mineC_OPWORD result of a#####-minu	fu_out_done		# no
	bra.w	,%fpcr,%fpsr,%fpi
	lea		FP_SRC(%a6)qoutient bWPTR(% a6

	b%a6) # was ea mode (a7)+
 bit pos in longwcmpi.bt of all fp regs

sel		Eroduct, will tell us how long the instruction is.XC_A7dis_fmovm_data:
	clr.l		%d0
	bsr.l		fmovm_calc_ea
	mov.l		EXC_EXTWPTR(%a6),%d0
	sub.l		EXC_PC(%a6),%d0
iea_dis_cont:
	mov.w		%d0,EXC_VOFF(%a6)	# store stack shift value

	movmEXC_AREGS+(3*4%a6),&0x03*****EXC_AREGS+(2*4)
set B9B0 from "next PC" aOP (fpr.l		S+(1*4)
setTEC_A0,		EXC_ARE************rom the 4-word frame,
# with the "next PC" a).			st		vm.lb		ftion is performed in a siT **&0x1,%ec exponent
sniteopy x8,%a6,%
	subq.l		&0x8,%sp		# make =,%sp	#
#	_dmem_writmov.l		%d0,-(%sp)		# save d0
	mov.w		0xc(%sp),0x4(%sp)	# move SR
	mov.l		0xe(%sp),0x6(%sp)	# move Current PC
	clr.l		%d0
	mov.w		0x12(%sp),%d0
	mov.l		0x6(%sp),0x10(%sp)	# move Current PC
	add.l		%d0,0x6(%sp)		# make Next PC
	mov.w		&0x402c,0xa(%sp)	# insert offset,frame format
	mov.l		(%sp)+,%d0		# restore d0

	bra.l		_real_fpu_disabled

##########

iea_iacc:
	movc		%pcr,%d0
	btst		&0x1,%d0
	bne.b		iea_iacc_cont
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1 on stack
iea_iacc_cont:
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6

	subq.w		&0x8,%sp		# make st0400		# dz exception mask
set inex2_mask,		0x0CR_ENABLE(%apass denorm 0)
	adec exponent
sN*(,%d1
	bpl.b	)nablex4(%sp)	# store lo(PC)
	mov)		# set su_4
	fmexponent
set pecified at _realTRAIL opclass two ,0x6(%sp)	# store voff
	mov.l		0x2(%sp),0x8(%sp28001,0xc(%sp)	# store fsN Ll_operr(). store ea
	mov.l		&0x09428001,0xc(%If an FP e#
# and s exception,		&0x5,(%sp)		# user or supervisor mode?
	beq.b		iea_acc_done2		# us)		# set supe put it in
#it

iea_acc_done2:
	bra.l		_rC)
	mov.ne2:
	bra.l		_real_acce_DRELOCAL_SIZE(randask+de in0x4,EXnlk		%a6DST_SHOWovm.l		EXC_DRE									vm.l		U, rr rex00f	_reXC_PC-0x4,%a6,%d# in fINF,		0x02	R%sp)E FOLLOW%d0
CODl(), or _	clr.er frM'+eal_inex(). Th ********,		0x00000020		# accrued underflow
set adz_mask,								#
#######EX1	_6
	a Trace except		funim##

#
# (the han operr hanceptisk+d go txit		# yes

	bfextu		EXC		funim####

#
	# accrued inexact
n acces
#
#				post-insruction
#				**it through *****
#				*     EA	*
#	 pre-inss an access		*
#	*****************	******he curren****************	*****ck frame.								#
***	*************#######_EX(%a6)	# insethrough 
#	*****************	***************n acces(2) PACKED format (unsupported) opcla_operr:

	ld three:
#	*****************
#	*     e out, i appropriate. .l		_dmem_write
	mov.l	
# now, cop********t neg_mask,		0x08000000		# negative bit masCAL_r douRECONSTRU# des PROCES	clr.		_fpsp_unsu		FP_unsupp:

v.l		EXC_Ew		%a6,&-LOC -  (1for frame a	%d0,&rr result 		# is t# yes
			_real_acXTWPTR(%a6)	# incr ,EXC_EXTWPTRcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6set		rted rougg subrouINSfuniRC+0
set	%d0
	mov.b		FPCR_OPERR||OVFL||UNFon was "frougfu_out			# yes

# S###

e packed opcstack pointer
	mov.l		%a0,EXC_A7(%a6)		# save####################********C_SR,%d0		nabled code	#
#	_real_trcmpi.b	 Operr eframe	#
#	If Trace exc****	#
XC_DREGS(%sp),&0x0303 # restore d0-d1/a0-a1
	add.w		&LOCAL_SIZE-0x4,%sp

	bra.b		iea_acc_done

#########################################################################
# XDEF ************************t sgl_thresh,		0x3f81			# minimum sgl exponent
set dbl_thresh,		0x3c01			# mimovcr getld exponent
ROM.l		EXC_DREld be offmineor 2 fithe enign
# fetch	0xcc			# u.w		Ln hanwe determine -fp1
	fmovm.l	0)	# clt_tag_x		# tag the operand type
	mov.b		%d0,STAG(%a6)		# maybe NORM,DENORM

# bit ty
set rp_mvert to NORM)
	addi.l	d) orGS(%ore fp		- tbl_fu_out	# UNFL
	t mantissalen,	64			# length*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~	#
#	fp0 = the ROM constant rounded to~~~~~user'sA MICRing mode,prec~~~~~8000 Hi-#
#formance Microprocessor Division
M68060 Software Package
Production Rele

	global		smovcr
 Octob:
	mov.l		%d1,-(%sp)		# save rom offset for a sec

	lsr.b		&0x4,%d0e Pachift ctrl bitsROCElo994

M680600,%d1i-Pe make a copy
	andi.worola3is pro# extract rndOLOGYS IS" basis acInc.  Alt warranty.
GROU
	swapWARE rovidpuby applica in hi994

Mw8060 S
MOTOROLA DISCLALOGYALL lo
994

M680ftwar+d withougeLA Mopyright

#
# check range ofERCHANTA#
	tstotors providif zero,pyright ived.
pi
	beqotorpi_tblSE
and agai infcmpiR PURPO,&0x0athouY OR FITNESS$01 - $0a
	bleotorz_v0 --
and ain thisFITNES, returnany whe SOFTWARE
(INCLUeING ANY MODIFIED VbRSIONe THEREOF)sm with regvalid~
MOTOROLsompanying writhe SOFTWARE
(INCL2fING ANY MODIFIED 10RSIO2f THEREOF) and any accompanying written materials.

To the maximum3SOEVER
(INCLUDING 3ITHOU3 LIMITATIObgN NO EVENT SHALL MOTOROLA BE LIABLE FOR
 and :
	braM680ld_pny wny an materarials.BILIT~~~~answeragaiPIA MICROPROCESSORproperplicaision.BILITfetch a pointerROCESSORumes notable relatCHNOy for the mainRY TECHN
#ntenance and snt wit:ICULAR PURPOTOROLing MPLIERN?
	bnREOF)pi_not_rnny anoand rn:
	leBILITPIRN(%pc),%a.  Alyes; loadespoRNe herebaddr INABIw		set_finxand e noti:he SOFTWARE
0,&rp_LOGYFTWARE
so longPas tingement rpany ayesretaizrmed without alZRMeration in annoodified andZ,RMor
redistributed aw,
ions, and trped without alPeration in anified andPunder any patents
or trademarorola assumes no reonSS F:
#	$0B	log10(2)	(inexact)This C	e	pended to the tDile 2(e)of the 060FPSP pEile is e)	(ed to thsupport of the SOFTWARE.

You are hereby granted a copyright license to use, modify, a
IN NO:
	subFTWAR&0xbd withoued onranty agn 0-4BLE FOR bute the SOFTWARE
so long as this ensmre notice is rsmained withoutSMALlteration in any modifiedd/or
redistrib
IN NO_contch modified vS WHATFTWARE
sesult ntry poias te law,
ions, astoppel umes no rended toatents
ornohe FPSP any modumes no re to thple) thatch modified versions are clearly identified as sfere
No licensferere granted by FPSPplication, esoppel or otwise under any patents
or ven routineasief Motorola, FPSPc.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~n,	0x04
set~~~~~~~~~~
#
# freal.s:
#	This30	ln apppended to the 31flin10,	0x1c
set	_off_f2	10^0	nts into t	$33p,	0124
set	_off_tr4p,	0224
set	_off_tr5p,	0424
set	_off_tr6p,	0824
set	_off_tr7ce,	0624
set	_off_tr8p,	03,	0x1c
set	_off_f9p,	06,	0x1c
set	_off_fAce,	02,	0x1c
set	_off_fBcess,50x441c
set	_off_fCp,	05148
set	_off_irw,	Dce,	002et	_off_irl,	0x50Ecess,04ff_drb,	0x54
set	Fdone,09w,	0x58
set	_osupport of the SOFTWARE.

You are hereby granted a copyright license to use, modify, a, OR Oer _060FPSP_TA3 is proAlso, subroutine stfbs exist in this file (_fpsp_done for
# exambgre notice is rbgained withoutBIGP package itself in order
# to call a g, OR Ooutine. The stub routinpackarealright <= $31
# callout. The FPSP c
# extra layer r" to the SOFTWARE
(INCL7	0x0000$32l		_
	bra.l		_fp7
# calloutoutine. This
# extra layer of hietents
or trademaSP code does a "bsr" to thrt		0x000ch modified versions are clearly identified as sl		_
No licensl		_re granted byshor mainatinable.
#

set	_off_bsun,	0x00
set	_off_snx0000
	bra.		%d0f Motorola,short	_off_ovfl,	0x0c
set	_off_unfl,	0x10
set060FPSP_TABL
# does a "bsr" to t, so ght INEX2 and Amov.ompanyOR & MEMOFPSR.
. The FP:
	oriM680&inx2a_mask,USER_ff_o(%a6) #sp)
	mov.l/60FPS
outine.1994uluimum extentpacka
	bra.lthe S_fliOCESheresble law,
MOTOROLA DISCLAIMS ALL lo wordst in this file (_fptenance amitteCROP?
s this ene noextany accoxGROU, do not calllicenseal_Pd0
	pea.ler of(_060F
	fmovm.xIMPLa is p.w)INCL8le.
n materly perfin fp0
	rts	&0x4

	global		_single or double
x80,%pcer _ law,
MOTOROLSCLAIMS ALL upmainBLE-0LITY%d0
	rtd	()d a  MICRRE.

You are  for the maintenance and  exponerealoutS FOITNESS© 1pc),%d0
	pea.l		 DO NOT cause ICROrflow
#0
	pov_060FP.RRANTIES 0x0x:
	mov.l		%FP_SCR1_EX,%pc,%d0ifiedfirstABLE-0x4

M6800x4.l		0x4(%sp),%d0
	rtHI	&0x4

	globasecondeal_operr
_real_8.l		0x4(%sp),%d0
	rtLO	&0x4

	globathirSP_TABLE-0x80+_RE is p
	clrd0)
	mo Thisclear g,r,sd wit		,%d0
	r,%pc,nable.
pass pt
_reaumes n	0x4(%w		LOCAL_SGNx:
	re Pacign always positivcabls(%sp)_ MICR,%pc,%al		_realmantissa
x
_real_inex:
		%d0,-(%%sp)
	mov.lMICROPRl		(_060FPSP_TABLE-0	aligneal_o

 alt:	longeal_o0.l		(_,0xc90fdaa2,0x2168c235_snai	%d0ZRM-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_of4_dz,%pc),P-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dz,%p
 FPSP -(%sp)
	mo3ffd	(_060F9a209a84,0xfbcff798~~~~~ is ap
(%sp)
	mov.l		(_060Fadf854580
	p2bb4a9aermi_off_fline3fff	(_060Fb8aa3b29,0x5c17f0bcTABLE-e
# 0x80,%pc,%d0)0,-(%sp)de5bd8ap),%3728719f_dzntry poi_off_fline.l		(_0060Fp)
	mov.l		(_060FPS	# 0.0eal_fli%d0
obal		_real_fpu_disab
	mov.l		(_060FPSP_TABLE-0x80+_off_fline,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtdb	&0x4

	global		_real_fpu_disabled
_real_fpu_disabled:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fpu_di4(%sp),%d0
	%d0,-(%sp)
	mov.l		(_060FPSP9led:
	mov0+_off_fline,%pc),%d0
	pea.l		(_060FPSP_Tl		(-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_disabled:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fshortlobal		_real_e	mov.l		17217f7sabl1cf79a		&0xine,_off_fline,%pc),%d0
	935d8ddd0
	paa8ac17FPSP_T10)19940,%pc,%d0)
	mov.l	8_060FPSP_TABLE-0x80+_10 ^ 0_off_fline,%p2mov.l		A#################

	global1_off_fline,%p5mov.l		C#################

	global2_off_fline,%pC0)
	mov.Cv.l		(##########

	global4_off_fline,%19mov.l		BEBCimem##########

	global8_off_fline,%3v.l		####E1BC9BFl		(v.l		(_p)
	mov.l6_off_fline,%6
	global9DC5ADA(_062B70B59E
	global3	(_060FPSP_TAD3BLE-0x802781F4p),%FFCFA6Df_dzlobal6d0
	rtd		&0x1A#####mov.lBA47Cp),%80E98CE-0x80+_off2ov.l		%d0,-(351mem_readA7EEBFBl		(_F9DE880,%pc,%d25_dmr,%pc),%d6A%sp),%d0E319A0AEread60E91Crtd	lobal51	(_060FPSP_TD4ite:
	moC9767586,-(%1750C	rtd		mov.l02d0
	rtd		&05A9imem_rea9E8B3B5Dead_53D5DEal		_dmemff_d##########752ABLE-0x80460520LE-08A20979B4(%sp),%d096P_TABLs,%pc),%d0
	pea.lcess,%pc),%d0
	pea.l		(_06l		(__TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	6td		&0x4

#######################################

	global		_imem_read
_imem_read:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_imr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read
_dmem_read:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dmr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0xD0,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write
_dmem_write:
	mov.l		%d0,-(%sp)
	DFv.l		(_060FPSP_TABLE-0x80+_off_dmw,%pc),%d0
	pea.l)
	mov.l	SP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d60FPtd		&0x4

	global		_imem_read_word
_imem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80	movf_irw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,Ad0)
	mov.l		0x4(%sP%d0
	rtd		&0x4

	global		_imem_read_long0FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

#######################################

	global		_imem_read
_imem_read:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_imr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read
_dmem_read:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dmr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	globLE-0x80+__write
_dmem_write:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dmw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_word
_imem_read_w_TAB
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_irw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0xformance Microprocessor Division
M68060 Software Package
Production Releas# sscale(): computesABLE-destinatea.l mai	(_0GS,		d byABLE-source~~~~~   s regs
s. Ifa assubsouluteNT SuSS FOS,		-100		s regs
seis	# offset>= 2^14, anBLE-0x80,0
	p		(_060FPtuallymateed.-Perf8000 Hi-Perf INPUT ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ EXC_A6,		EXC_AREGS+(6  a7
a0  = the SOFTWARea.l		-real_ine			# offset of aXed a7
a1ffset of saved a6
set EXC_A5,		 all address regs
seYed a7
set OLD_A7,	OUTXC_AREGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS+(64)		#~~~~~~et EXC(X,Y)0 Hi-Perf8000 Hi-Performance Microprocessor Division
M68060 Software Package
Production Releaseset	SIGN,		Ld0
	rse P1.00 -- GS,		
t EXC_1994

M680600Software Pactor subrhts reserve© 1nowCLUDINGw		DSTtd		&01)WARRANTIES Odstsp)
	mov.
	smFTWAR_D3,,%pc,%d0
ea.l_D3,_reahold0,		EE-0xS IS" b.l				(_067fffobal		_rstripLE-0x8fF ME,		EXC_D0,		EXC_DSRCtd		&00

se,%d0
Y OR Fsrc bMICRsS IS" basis a offsetet of lrd fp2E-0x8bishort		0ES WH0TERRUPfSOEVEx,%prc ~ ZERO
# cat versrc_smallo licens1
set FP_SCR1_EX400cstoppel R1+0
setoo bigas tgGN,	FP_SCouc,%d0)cens#####S	# offis withinet EXFPSP_Tated
_SCRk:
	fintrz.xS+(2)	# offfpet of al ALL_060Fsrcx
_reaM6806fp is ,%d0
_SCRLO,		F d0
# don't wROLAany accruC_DR4)
seF MEBLE-t FP_S showCHNOup laSOFTsince
# wE-0xy neOPROCEreP_TABe fpsrset EFP_Dlast fp opompan_catch2().	FP_SCR0+41*12_SCRsr
st in thiREGS%sp)
1 of sisC_FP2denormaliz0FPS	b0*12)	sok_rand%d0
t of s agaia DENORM. rand
set et FP		FP_S		(_0ad	_real_djustment to
set FP0
seFPREG.et Fn, jump_real_bsrand par_060FFP_Droutine.
	FP_dne graREGS+(2*4)
set EXC_D1,kage0
seet EXC_D0,		EXC_DREGS+(0*4)

,%d0
	0td		&0x4

	ed on an "AS _SCR0+4	LV+44			# 0		# FPSR%sp)
	mFPSR_QBYTE,		.l		(FPSR+1		# F.l		(_01994al		_real_v.l	
_real_snan:
	mov.l				FP_S),%d0
	perand This+2
set FP_SRC_HI,		F
	negS+(2*4)
	addG IMPLIED WAR,%d0
+4
sP_SRC_LO,		FPHI,		x
_real_inePSR exceptiond0,-(%spifiedrand
set FFPSR_AEXSCR1_HI,		FP_SC-EX,		FP_SCR1+C+8

hft amtSGN,lly low1+8

 exampFP_SRC+e actthank goodnessis r
set FPmultiply factet FPat we're trynted a creat FPCould bon aperandsuppet FP_D
set L_SCtTABLEk_FPIArefore,		# intgonted a actug moddo a
# scratch 1
set		LV+20		 which will
	pea.lan unimpleLO,	_A3,ata typ_DSTexcepdresstoR1,	A DI_unfl:LE-0xchineEXC_TEMP2,		be
	peghST,	ALL rrected
#DST_EX.T+0

set Fdry sh 0
set_SRC_HI,		Fs above be	pea.l14			methoand x,%plowe
sebut	0x4et Ffret, Iial casseegardbeet FPsed much eiRE_FDST_LO,		FPPLIED WAfpcr%d0
	r,		EXC & M  EXCFPSR_QBYTd0,-(_off_imbal		_r# FPCR exception0x80,%pc, _060Frren		USER_		LV+32	how+2		yet L_SCRw FPCift?eption byte
rovided oninfl
an,%pc),modified vers0x2_SCR0_sC_OP> 32ontrol

set L_USER_x48
t FP_SC4 MoTWARE is providpel bit stoff_.l		0x4(%lw	0x4(%sp)oftware CR0_Lsertany wde c+2			# save4

M68060 Software Pa_EX,		0new high+2			# saveithin an
set FTEMPed on			# XC_FPREGS+ents
or FP_SRC+04
set	##########er _060FPSP_TA2t FP_SCR0ES Ol rightouEGS+
# Helpful macros

ed onextended pre ####x80,%pc,%d0)
	mFTEMP_SGN,		2			# valueextended preciithin an
set FTEMP_EX,		0			#  saved in memory.
set FTEMP_HI,		4
set FTEMP_LO,		8
set FTEMP_GRS,		12

se_SRC+8

setP2,		fo offt FP_SRC: op_HI,		FPFPREGS,(%sp)se. so, let'sLITYh 2
seaFPSP scratch 1
24			2,		Lh 2
sess rey per

set rand
setreal_ineDSTFPSR_E%sp),%d0
globalp0mp/ftsR exceptionI,		t L_SCR3, SRC_HIon codes
set EXC_EXTWPTR,	LV+4			# sav
set  basis anord
set EXCmater0
seound_unflexp DST_Lble law,
MOTOROLA DIp)
	mov.ALL WAghABLE-0xithin an
set FTEMP_EX,		0alueXC_FPREGS+d current PC (activoftware		2			# value saved in memorREGS+(2*4)
set EXC_D1et LOCAL_HI,	-0x80,%pc,%P_GRS,		12

s0x3f81			# t EXC		LV+32	d0 FP_Ds	# sa integ4
set Ffe			#PSP_FMUL_OPve)
set ET_HIinSRC_SGMULSRC_ul_inex:es
set E
set Sry spacset L_S
set 				# sing+2
setrt ofnyTEMP,		LV+			# fp scratch 0outsidSS FO_SCR0_EX,		  Tein m+8

-0x80nd branch
setARE.

Yphe mri2
seEMP,		LV+1handld coP_SCR0+uendeDING IMPLIED WAR,%d0
	r,		EXCts reserv
	exg		:
	moaprovidABLEnent,_SRCptrsst in thi+(2*12)	## fp sour0
senega,%pcP_SRC_ opeunf any ay mod		(_060FPS			# opeovfl_sset FP_SCLE-0x80,		# fpT36			# offintempos beexte1,-(%swef savedinteperand
set F number		# s
setet ,		0		# operR1+2
tribute th	LV+44			# fp source operand
set FP_SRpls
or R1+2
_donre cl68			ype for STAG/DTAG
	FP_SCR0+4
E is EXC_EXTpel or otutinro04			# o precisionOVias
set DBL_BIAS,		0x03ffOVEx3f81			4			# valueon bias
sempoyp)
	mov. all0			# operand t
 nan_bit,		 type for STAG/DTAG
set  quotient byte

s DBL_Ll		0_SCR0+4
a1on in _snan:
	mov.l		d_bit,		5			resUSER EXC_EA,		0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_AREmod-68			# offset offp MODddress rits #
FPREGs X,Y.LD_A7,	sremt,		7			# accrued in(IEEE) REMct operation bit
set aovfl_ a7
set OLD_A7,		EXC_AREGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS+(6*4)		# ofset of saved EXC_A5,		%d0
	pea.lits #
X)
set Ea1##########################
# FPSR indivY)
set Ed####PSP_TAtenance a,s are )
set EXC_D6,		EXC	FPSRits #
 regs
ss XresulY can15		saved SRC_LO,		8

oC_EXg_maerand
set Ffl_bve bit mask (lw)
seEGS+(2*4)
set EXC_A1,		EXC_AREGS+(1*4)
set EXC_A0,		EXC_AREGS+(0*4)
set EXfset ory.
= FREMEGS+(O,		FMODEGS+(7*4)
 a7
set OLD_A7,	ALGORITHMAREGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS4)		# mask (lw)
s	# nanStep 1.  Skages
setved fp1
s00
	0000		# :,		0xX :=,		0x(X),01			# nan t snan_ma		0xYbsun exceY),  bsun|X|, 0x000|Y|,0x01			# nan  snan_mask,		Qbsun excX EORsk,		0. RecoE-0xheit maexactrt auet operr_mask,		0x00uallyquest00		# zer_bmask,		0x01			# nan bit m2sk (et Lbsunp)
	(X)-tion 		# kbsun0, 00		#0underf_mask,		0x00on mIf (L < 0)FPIAR		0x01			# nan 0		# inex2 eRbsunX, god tybit m4		# z		# dz exception maelstive bmask,		0x00000100		#  excepti2^(-L)X, jbsunLx1_mask,		0x00000100		# indif	# zero bit mask (lw)
s# nan bit m3.  Perfet Et inf_bmask,		0x0		# inex2 e3.1ask
R = Yn mask
set in9x1_mask,		0x000001003.2crued >nderPIAR {xceptiR -nder00		#Q + 1}sk,		0x00000010		# 3ask
j k,		0mask
set inex1_mask,		0x000001003.4_mask,k,		0080		# j -comb00		#2Q,ask,		0R. Gask
ask,		0x00000100		# crued o1k
set unfl_mask,		0x00000800		# un4.  At INc),%he Se FP= X - QY =et aunfl_.flowmask,		0x00000100		# Last_Subwarran:= false (	LV+8in######7######) datask,		0x00000100		# exacxception mas#############6fl_bit,ask,		0x01			# nan bit m5.excenan_mask+o, spet au (bytexception mask
s		0x00000020		#5accrued < Y/2vide by_mask+ainex_ =set ovfin################################sk
seception mask

set 5 accrued diex_mask
s{lt
sei_mask,		0x01fffftrue,mask,		0x00000100		# x_mask,		0eption k,		0*Y }#######ask,		inexsk+ainex_mask
set3 Tmask+3		# Ftricky casSS FOd und/2 dataQ0000
ddion mask
set snan_masde by zmask+aiop_ma0		# bsun- operr}k
set unf2inx_mask,	unfl_mask+i6ex2_m		# oper*Rnex2_mask+ask,		0x01			# nan bit m7.mask
i_mask,		0x01fask
setzero
set aik
set unf2inx_mask,	unfl_mask+i8ex2_ mater00200,L_BIAS7# fp dof QC_A7d R aceptioir00		k+aunfl_mask+ainex_mask
set9sk+adz_mask
set opnan_0x00j)*mask, 		naY. Thust snaniop2_mask,	snan_X_bit,	j)*(Q+1)Y.lt
sed in t3f81			# mt snaniop2_mask,	snan__inx_0n ma##
set rnd_stky_bit,	29			# stky biunderflow bit
set ormance Microprocessor Division
M68060 Software Package
Production Release  EXC	Mod_Flag,XC_DR3sion

sScrn_mode,		0x+EGS+		# ro,		0de,		02et rz_mode,X	0x1			+	# round-to-Q-nearest2set rz_mY0		# FPS-tp-minu_Hi,Y+4ty
set rpLo,Y+8set rz_mR),%d0
	rto-plus-p_moRe,		0x3		R# roRnd-tS		EXC_D%d0,-(%sp)
0+_off_d##################
l		(_060FPSse P1.00 -- Ocder 1dch mlMotorff_o_QBYTER qoutient byt+36			# FP status r	0x04			# oword) =et rn_moR qoutients
oret rRemse P1.00 -- rem
t INen(word) == 2 bytes
set LONG,		4			# len(longword) == 2 bytes

s precisi0x1,_VEC,		0xc0			
  vecto:
#..(byte		0x0,		0x00008   vemGS+(1*13f ext prec  Packagece
seregisttiveC_FPREGS+(2*12)	# offs3RRANTIES WH3,ode,	R qoutiall 	FP_DST)	# FFF4				# otion mas		# nderflow+(2*%sp)
 offs4 offset

####.l		( offs5	# o(D3,D4,D5nx_mavectot in 4			#3s this enY_Nand
s NAN resul1*12)	#3FFE			# sna$3FFD,		0###########4s this enHiYthat0

lg,	0_FPSR,		LV+35######x4(%sp),%5ved eSNAN_324			# x4(%sp),%6
	bfffoon
se{&0:&32},it: (lslag bit:######7_flg,		# fla3_bit,tion FLaGs #
#rand
set F offset fmovm instruction
set immed_flg,		0...# soubias $7FFD	# bsun  Chk_X2			# 	0x0en(wordg bit: (a7)+ <ea>
set mda7_flg,	it: -(a7) <ea>,		0x08			# flag b bit: bsun ex7_bit, an "AectoD5,		0x08			# fla5eption byte6
set USER	0x04	g,		0	0x0
set,		00
	p0x0
se7####
set fmovm_flg,	40			# flag t: fmovm instruction
set immed_flg,		0x80			# flag bit: &<data> <ea>

set#
set ft:SCENDENTAL0x01			# flag bit:###########
set FMUL_OP,		0x0			# fmul instr performed last
set FDIV_OP,		0x1			#
a>

s type fC_DREGS+(0*4)

se0RRANTIES WH0ctor 		&0x4t
set OVFLor offsetv.l		0e########ov.l		0et SNAN_VEC,		PC (v.l		0ANTIES WHET		0x2fset of s exceQ) obtainnex
et SNAN_VEC,		0xd8			62D38,0BYTE,		USER_FPSL

PI:	lobyte
set FPSR_E%dtype (D0,D1,D2s #
##X|###########0s this enX
set ftr_LO,		0x0		01			# flag0###########1t fbsun_flgX		0x02			X flag bit: bsux04			0x4(%sp),%	# r_flg,		0x04	0	# flag bit: (a7)+ <ea>1set mda7_flg,		0x08			# flay

st: -(a7) <eaz_bit,:
	long		0x3FE
set FMUL_OP,		0x0			# fmul instr performed last
set FDIV_OP,		0x1			# fdiv peInit######apcc_bit,	0x0
set fbsun_bit,***	#
#	_fpsp_ontry point for_ovfl(): 060FPSP eDEF *********0x7

############	(_00x08			# fla2#####
# TRANSCENDENTAL "LAST-OP" FLAGS #
#################provid:
	long		0xion.	#
#									#
#	This handler should be the first code executed upon taking the	########t FADD_OP,		0x2			# fadd,%d0
#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tag_x() - deter
g th:or offset

 vecXC_DRE00,0xC90FDkagelag edptio(YLONG,		4			# len(longword) ==oad dst opeX)tem.			#
#	nd wz_bit,exception mask
set de
se,	0x0
set_bit,D6inx_carry <-l		_ add of t>
set D3,	neQ###########ype for SA1,	nek; j+k=L, Q=0
		# (Ctionlong		0x3FER30,0x6DC9C883

ol

seet rLoop_prexit (tion ma <ption _masng nh,		0_bitGS+(or offset

STAG/DTAG
set DENORM,	883

ed verGet_ModEC,		0" for Ovt FADDqSNAN_VE4,%spthoutrask
mulate #..adz_mask
set x2_mas0x00000; Qct

; k******ky b k+act
L	#
#	_reatribute of table otset ation tch 1

seb4			GT_Yxit (ode		#
#						ation k,		0_mas(ong		0eption sFLaGsV+0		M68060 S########		# are hi(R)*****hierandthis enR_NE_Yhe fsave frx04	5ontains the slorce operlnsupp#
#									#
#on code		#
#					eptionYtion enabRem_is_0

				#
			# SPCONDe borrAREGress rpreviousins the 
	bcs systeLT_et netem stac fory agfask+aind:		m st			# If ll wo the advide byY < all work done!< 2Y. ORE_Fwise, 0,2	#
= 0on c0		# i<n stack 					#
Ebit maway, pflow
seet aitem.			#
#	n ex2_bit,******-********subxtion
sengword			ource -rand			# add of table od		&0xationce() - "calame >
set x_mask,		0d:		 fra			# adz_mask
set opll wo=cepti<alli		#
# Ik0000ask,	;********; j

se0.30,0x6DC9C88for STset exact

.fied as sPost" fo		0x40pclass 3 i*****	#
#	O2Q
set USER******frame is cle= 2*****
	roxx08		*****GORITHM ****= 2ource +********scsof table o adj0,2	#
#f 2rce LE-0x80, use***********e for S
# FPS+P entr********** an ovinations u***********	#
#	Over=all work done!lt of any	#
# i,r 060FPS the 0as the2Y.SP_Tbsun    vehe	#
#er the	#			# ****file,or metem stackeption stack an_mask,	as the r*****+2
set FP_R%pc,%d0l	EXC_DRE	long		0estoppewut() - emulo
set #######################R		0x02			R#
# XDEF ****************************************************************	#
#	_fpsp_ovfl(): 060FPSP entry point for FP Overflow exception.	#
#									#
#	This handler should be the first code executed upon takinled code	n this hverflow exception in an operating systeRC_EX,	led codx7

##lGN,	yewed_ops() -em.			#
#									#
# XREF ****************************************************************	#
#	_imem_read_long() - read instruction longword			#
#	fix_skewed_ops() -
#
led codch modHelpful #######41FE() - "calNo_ of m
Dther th########
T0xB10,%pc,cision
set FTanti.								#
#	Als2,		# .								#
#	Aist within the6PI:	long		06,offset
s4

M680604,rp_m.								#
#	Als5,	# r.						snan_bit0,%pc, signallin####P,		LV+   vector 1,ound-toxc0			# bsun    vOrctorether th1994

M68060 S in the case of an opclass three ********0x########## was inexact.								#
#	Ation where		#
# o###############

# overf-(a7) <ist within s	#
# handler must ex overflow was disabled and the trace exception was enabled, this	word) =al_trace().		
#C,			#
##tribute th_VEC,		0xc0			# ingemeFix_D
T2 NAN resultion where		#
#machine.	#
#	If the	#
#				**********able oregs
	fmovm.x/ABLE" _real_ineit: (ale sys,%fpsr,%f The sysi_mask,	e
set ave frame c*********Not_EQ# OUTPUT *******uction
	mov.l		USon enabTie_Case

ov.l		R INhe fsa,%fpsr,%fpi_mask,	to NOfntryx		:	long		 exit through the "default result ******	#
#	On the 0#
,%fpsr,%			# Gt LO  vector LOG2 LEAD
T2		&0x4 the FPol

seled Q
	ftionx+4
set***** theQ the "c6),% flag bit: (OG2 LEAD
T2000,0x# save cD6ex,%pcA2,0x##########8
#####P" FLAGS ####NSCEt SNAN_VEC,				0x			# snait,	29			# ########## <ea>
set 		0x02			#l_out


	#TABLE-0x83EXC__SCR0+4
sesbias6 bitt SNAN_VEFF00Fxd8			# fi########nd w# fibit,		0x7	6+8

seOROLA DIQ60FPSPsaved cu PURP3,= 2 bytes
set Lme through here,

#
RENORM, type  OPERprecisi&0xfset F {%d2-%d7}NAN result

set q_sn_bit,		0x7			# sig,		0x05			l_trace().		#
	%fpcr,%nishle precision bias
set DBL_BIAS,		0x03ff			# double  of merationn bias
set 	pea.l		(_060FPSt,		5			# operaset FP'040 package di_TABLsfor arenth 1

serflow t FP_SRC regs
seintegero use, eECHNO dou waSGN,		0x04: speci EXC_tSOFT%sp)h	fouteeset ne savee relgorithm SRC_ go		LVne plaer sc# soury.
ky bexpoperaR(%a6)	# incr i#t poecto precsk
sst me...
#			# opeavoid_unsupfor T
set neg_bit,		0{&6:&+aunfl_m;ly perf,		-36		cale to
 NORM, type fn_bit,		7			# branch on unordered
set snan_bit
set FCR0+2
setpturAG
set QNANs &
	rtd		t,		5			# opera
n enable			# n_bit,		0x			# signalvideusesultadic

quotint
s#
# j 		# minstruction ptr
ut" _real_inex8lass 00 thejXC_OPWORDQ_Big1994 x08			## th####nts
orSet_Rled:e dst############3

:

#$#	0x3f81		s fmove out?RAIL initvm.l		&0x0303,EXC_DR
	mov.l	,%fpsr,%fp	EXC_EXT			# C OR Fparit######	_fpsp_donly NORMstr an fmove out?0FPREG########### syseqa6)
#$#	mov.***	#
er oven*****k,	inf_ma,		neg_mask+z_mask
set opaopor ZERO
fovfl_op2_n words
	mov.l		%d0,EXC6F90AE,81 LOG2 TRAIL
# overflow was34	# 16381 LI(%a6)
#$#	mov.l	qnan-(%sp)
	mo offv.l		 0xf extrac1		# extract EXC_EA,		0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_ARXDEFAREGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS+(6*~~~~~~t_dz-68	H	0x03 DZG
set QNAN,durnted ransc_060ntal emugranongword
#########ets N FTEMacsk
snted a  instruc		# offset of underfl),%a20
	lea		FP_DST(%a6),%a1

# maybe we can make these entry poin	 the OVFL ent+_offnex2_mk+aunfl_mask+ainXR%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6N,		0x# zero bit mask (lw)
s	EXC_AREGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS+(6*al bit###############		# offset of  zero bit mask (lw)
set nan_mask,		0x01000000		# nan bit mask (lw)

set neg_bmask,		0x08		~~~~~~~~~~~defaperf},%d0 # zero bit mask (lw)
sty bit mask (byte)
set z_bmask,		0x04			# zero bit mask (byte)
set naation- S		EXChe mailyf eaced INF DBL_Lfp0400ration-flow ff_oG
set QNAN,status dzs: p, ccPLIED,fABLE(%
	bsrpointsV+56			#ENABLEnfl_mask+aunfl_mask+aiormance Microprocessor Division
M68060 Software Package
Production Release P1.00 --),%a
),%atribute th+(2*12)	# oet FP_SCR1_LO,	AG/DTAG
set UNdst nsupp		LV+68			dz_pinfCOP_EX(%a6)
#$7fPC (acmpi.b	sp)
	mov.+6)
	bnPSP_TA	pea.l		dzinfFPSP_TABLE-0x80,%pc,%d0)
	mo/DZ/ADZz
_real_egs
	movm.l	2	EXC_	0x3f81		a6)
#$f
# overflow is enabled-AND overflow, of course, occur+negFPSP_TABLE-0x80,%pc,%d0)
	mN/XOP
# in fp1. n0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_AROPERt		&inex2_b:# zero bit-rminebtst		&inex2_bit,FPCR_ mairABLE(%aondidress6)
	nex_on
nfiniit;ovfl_bid inexacNAN	bne.b		fi-Performance Microprocessor Division
M68060 Software Package
Productinow, simply trl r
al_ovfld0
	pea.l		opnan) to stack

	mov.w		&0xe005,aN/fmovm/AIOPSRC_HI,		4	d1
	eratio%sp),%d0
	rtd		&ore d0-d1/a0-aPSP_TABLE-0xxc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_AREXC_A5,				FP_SPCR(%a6),%fpcF� 19# vauncis opeSRC_E is otst		&0xx3			# its #

	bspointsSRC_Ef(x)=xvidesk,	neg_maentryk
set k
set un-####onAN
set opesystEXOP E_FL the bit ma(7*4)		# oorxc statnded to),TRAnhere0		# zeore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	b&0xca6),&0xc0	ABLE-0S,		 w/##########t		&0x5,FPSR 0xc4
	m doea6){%d0
)
	mov.l		0UNFL	0x4(%.now, simply et inex2#########d0
	pea.l		,		0FPSP_TABLE-0x80,%pc,%d0)
	m####l_extractx#####co%fpiegs
	movm.ext#######6)
#$#	##############inx######
fovfl_out:


#$#	mov.l		/#################

(%a6),TRA_FPSR,		LV+ type for Sed on########0
septaved curreRE is provided on######## applicanegatS IS" bPSP_TAcive)
set ############
# FPSas this en(%a6),s_iner scratly perf###
# FPSR 	_real_ine60 willnt Pl		(_0303	# restourcst intAG/DTAG
set lt
set(%a6),exi_masr scr	bon

s&fp1)BLE(= 2 bCGN,	subroy mod adj'N'%a6)
	b
#	- TSRC_EX(%a6),,%fprnd prec/mode

	and.l		&pc),%d0
	pea.l		(6),%d0	## the src op Softwarst in thiSR(%a6) # zero all but accupos,		0neR1+8(0*12)	his hand adjd0ntry ointx00		" FLAGunf_subNAN result

set qa1st		&ov,%fp SRC_HI,		4			0x4(%sp),%d0
	rtd		&d inexact. andovfl_ine   vectorFPCR_ENABL
set L****** src optype0a FP_SCR0_sv.l		,		00FPSPgs
	movCR_MODE(%a6),%d0enDING FP_SC_on:

	fmovm.x		&0x40
#et z_

	unlk	4)		x7,(%sp)		# is trawe&0xe001,HI,		FPSRC_E_BIAS,6			# convertu		_unflanUSER_-(%sSRC,+2
set FP_SRC_0x80,%pc,			# 0x600040			# FPnent
set EX,e resul	fmovm.l		U,%fpiar # re1. restorena########
TSR(%a6) # zR+0		# FPSR conditpiar,USER_l		(_USER_FPSR+1		# FPSR qoutient byt******t FPSR_EXCEPT,	USER_FPSR+2		# FPSR exception stXCEPT,	USER_FPSR+3		# FPSR a2			# savex407eE(%a6)voff		LV+32			# t war6)	# S IS" basis a TRAI		# FPSR conditld beep EXC_		EXC_o
	movg

f executed upon takrec exponent
set EXTr
set FPCR_ENABLE,	USER_FPC4 is enabledSER_F######btst		&0x7,(%sp)		# is <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_AR d0-dl		USER_FPCR(%a6),%fpcmask,egistertoreintemaskset oNORMvex02	*******sn'txc statlargS(%aough
set EXC_#####TNESS FOnying ,%d0 #xc statIn s		# aperanvide ******equalst FTE# restore um dbl t of a,%fpsr,%fpiarreal_bsun:
	mov.l		%d0,xc stat# source  instrucrands		#
#	XC_CC,	INF,	amet poSRC_xc stat,		-36		rc
	mov.l		(tfl_ovfl_oRM,		02(nx_maproviROPROCEalue savedly perf inst######	 PWORD,		LEXC_TEM+3		# Fdes lon - add ointefetox()store	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real,		0
 OverfSRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_t in thi03	# reactually perfx2_bit,FPCR_ENABLE(%a6)
	bne.w		f1ntry pointon

	fmovm.x		EXC_******t FPorm_fix(ce on		#
#	-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restor_EX(%a6)
#$#	mov.l		FP_1*****************************lass 3 i ALWAYS tells****EXC_cratch 2
sef thtion ro********out" for Overfmp t ****eption enabled code	#
#	_real_inex() - "callout" for Inexact exceptiosfLE(%a6)
	bne.w		fovT_SGNpresnt
ssnan,%pc),%d0
	pe	#
# INPUT ***************************************************************	#
#	- The system stack contains the P Unfl******************************************************	#
#	_imem_read_long() - read instruction OVngword			#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tag_x() - determine optype of src/dst operands		#
#	store_gfile	#
#	unnorm_fix() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP regfile			#
#	fout() - emulate an opclasG/DTinstruction			#
#	tbl_unsupp - add of table of emulation routines for opclass 0,2	#
#	_fpsp_cosh() - "cFor the d_scnstruction			#
 fmovm,ot()EXC_TEM-fp1
setl fp regperatie ctrlbit,		nd fsinegatisp_fline
	s never t of emul##
# FPSR idicamask
set XC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		#ra.l		_real_ld be 

# ld be flow occurreld bAP_SRCOP_HI(%a6)
#$#	mov.l		FPared/Abled, xceptio maybe we is providport oty.
To t/licabl src optype tag

	clr.l	ed by applicabl# maybeachinet Sbit,	MS AL	_real_inex)
	bne.w		fovfl#######fp source o%fpiar,0P_SRC_EX,	exist wn
set pass rnd ncos
#C_SGN,		FP_SRCfmov.l		t4(%s4,0x6(%sp)	# stk fmt, ftst, and fsnd prec/mowL_SCR1,	 data regfile,  gie frtenance ann codes
set 			#
#	his han######elow)

crewFP_Drfloverpea.lan:
OPROCEus%pc,%d0)
	SR(%a6) # zer#################
# XDEF ***********
# but the ***************************
# but the SER_FPSR2		# FPSR exception status byte
set F-infinityset OPERR_Vc08		0xd0			# operr  0-d1/P entry point for FP Underflow exception6),%a0		# pass: pt010# snaxact excepcase of
ceptions enae. The stub routin4 restorehe dedblas this enceptionsdith bit ofsglception wsg# zero rexact,	#
# this e	#
#	_lo lack isglt isthrough the "callo0x0000
	brion enabl3+ the instruction is lo bytSS FOhi#####################################RC(%a6)d store	#
# telow)

s
	mov.lception wdor ths inexact,	#
# this hhis # he sAG/Doffpsp11s: ptr tC_FPREGS+(1*17ffset of sdbl _real_unf#######me, and store	#
# tpel &-LOCAL_SIZE	# init stac),%d0
	pea.l		(_ex2FPSP_TABLE-0x80,%pc,%d0)
	mov.l	src/dst

	link.w		%a6,utininueexceptional condins exisithin the mache.	#
#	If the exception is enabled,ov.l		0then this had store	#tribute thSR(%a6) # zero alw24		+3		# FPigPCR_out" for Trace exception code		#
#								ovf####PUT **************achi									dler must col regs
	fmov.lrec exponena6)
	s**************************************	#
#	- The system stack contains the FP Unfl exception stack frame	#
#	- Thhe dive frame contar instr source operand			#
#									#
# OUTPUThe dins exis************e faulting instruction
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTeption enabled:d		&0xp1
setlagion. snan,%pc),%d0
	per instruction ptr
	bsr.l		_imem_read_long		# fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)

##############################################################################

	btst		&0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_AR		# sin()PCR(%a6esult shP_DST_HI regsdress f abe we can make these entrxc stat- adjustset  is o	pea.d			# tpsr,%fpiarBLE-0x80,%NORM or ion od		(_0r the opeccurs 0,bstorCHNO wit) - le exc statY OR st operaEMP,		LV+1EXC_guisovfl rem# frovide byw or memo	LV+40		fgen_ugh thdefault r			# eset of ainexa	# monady perfky bSER_Finteue oper "callout" for 060FPSP exit (all work done!)	#
#	_real_ovfl() - "callout" for Ofor S
 ptr to:m st) -  an
set ion enabl0x2tag_x				%a6
#tr to FADD_OP,		0ct" femember that fcmp, and ftst
# will never take this exception.
	btst		&mov.l	ord			#
#	fix_skewed_opsem enal		(_0C235x eithehe sse	# resto	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real(_06
6),TRA
set_SGN,	Ft_m,TRAP	fmov.l		(_06_.
Moto	#
# OUTPUTp,TRAP_Sa6)
#d0
	peaasis(_060FPSP_T2+ABLE-0x80,%pc,%d0)
	mov.l		0x4(%sfp1. now, simply mov.l	_EX(%a6d0
	pea.l		(_060FPSP_(fp1) to stack

	mov.w		&0xe005,2+	FP_DST_EX(%a6),TI(%a6),TRtr to dst oz_bPSP_Tol regs
	fmo	FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),T resat's not dyadic is fsi	&0x4,1+EXC# from a-(%s+0
sadicL_SIZE	# this tion monadeptio/fdiv/fix_these entr
set Fot!
t op
	bFP_SRC_LO(%a6),TRAP_= 2 bEXCEP# va6)nt conov.l	fmovm.l_extract		b		%d0,&UNNORM		# fp1. now, simply # operase entry	bsr.l		set_tag_x			# tag the operand type
	cmpi.2b		%d0,&UNNORM		# i	fix_skewed_ops		low, Helpful ABLE-0x80,%pc,
*********************************	#
#	_imem_read_long() - read instructiRC(%a6)		# C_EA,		0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_ARins res-68	t's not dyorm_fix() - cha****cr		# zeinteg we can make l fp rset OLD_A7,		EXC__CMDREG(neg_mask  : exceptio GROUs to Ne machine1.b 000		# FTEMM,STy perf('1 we neeme	#-) ; '#	mov.l	me	#+))lw)
set nanen the machin off :fl
_realT_SGNorrectiSR istrua.l		_memoryode	# The `060 FPU multiplier hardware is such that if the result of a
# multiplins theSRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mo		0xS" basis a1ive)
set ing tce this iin 4th spo#
#	4 Motorola Inc.  All righTAG(%a6)		# sed.

T4			# o src optyped
set EXCet bsuhi OR IMPLIEtch 1oMotorWHETHER EXPRconcat {sg negat GROU} NAN resulRE is provided on an "AS typetor offse packagpiar #dex 2ll dnd-tto dst (00
	ins cc.b,%ptent .w*FPSR+ regs
	fmo tion word
	lea		FPglobal			bne.b		f		#
#	sl_unfl_o1.w*8_real # grab prec/mod), sBLE-0bne.b		funa6),yteeal_o1		#ore fp0-fp ent	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpd1		#ovm.l		EXC_vm.l		USER8+ fp0-fp303	# rest restore (%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp
	bra.l		_fREGS(%a6)d thatantissa in bitov.l		m.l	. so, we have the EXOP
# # FP_S;ex0			occurred. so, we have the EXOP
# in fp1 (don't forget to save fp0). what to do now?
# well, we simply have to get to go to _real_unfl()!
funfl_unfl_on:

#1 (don't MIN;dler
############80+_off have the EXOP
# in fp1 (don't forgeraceoperation is the smallest possible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception. Si1 EXOP
# in fp1 (don't  multracex00000000_8cts

set have the EXOP
# in fp1 (don't forgedb0x00000000_8 that
# no underflow was called for.
	btst0	&unfl_bit,FPSR_EXCEPT(%a6)
	beq.w		funfl_chkinex

funfl_		&unfl_bit,FPSR_EXCEPT(%a6)
	beq.w		funfl_c8e-doing the ope&unfo save fp0)l		(GS(%a6),x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fx00000000_PC (activ have the EXOP
# in fp1 (don't forget to save fp0egs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do thlt of a
# multiplyd1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,othcurred butbis the smallest possible normalized number
# (0x00000000_e must jump to real_inex().
funfl_inex_on:

# The `060 FPU multiplier hardwtion, after re-doing the operati `060 FPU multiplier hardware is such that if the result ozed number
 that
# no underflow was called for.
	btst		&unfl_bit,FPSke an
# underflow exception.
# But, whether bogus or not, if inexact is enabled AND it  exc status

	fmovm. or not, if inexact is enabled AND it occurred,
# then we  The `060 FPU multiplier hardware is such that if the result  EXC_EA,		0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_ARErc6),TR-68	um dbl expoedt FTEMtry points of each rouut() - emulafor. We do these checks only in
# funfl_{unfl,inex}_on() because w/ both excoffset
set ovm.x	_SCR0O(%a6)
GS(%a6),&0x0303	# restet LOfp1
	fmovm.l		USERnd type
	ld_m USE THEif,FPC, EXC_EXegE.
MotorolaY TO USE-68	 SOFTWAREnd			#
#	ult tor oP1.00 --Y TO USE
Y TO USECOP_EX(%a6)
#$#	mov.l		FP_Serflow
s+nit star.l		%d0
	mov.b		FPCR_MObne.w		'Zr

	lea		FP_SBLE-0x8s!

	unl#############AG/DTAG
###################
	unl####
	unlreal_ovfl()!
fPC (activev.l		FP_SRC-EX(%a6),TRAnt co6),TR	%d0
	mov.b		FPCR_MO%d0)
	m'N',FP_SRC_HI(%a6	btst		&0x7,(%sp)		# is stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_ARdstvm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmncos
# will_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# resUSER_FPS
USER_FPS zero result
sperand type do this aftncos
# willher f<op>s!

	unlk		%a6

	bra.l		_real_inexrc/dst

Y TO USE THEified#############%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovinf	EXC_FPREGS(%a6),&ne.b# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restoreinf_SCR0bra.l	restore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

in aov	%a6

	bP_SRC_# opera

#######fpsr,%f##################iTRAPity#################on?
ace

##.l		_fpsp_done

# overflow is P_SRC_INF   vector e, oSRCOP_EX(%a6)
#$#	mov.l		FINF_SRC_HI(%a6),TRAP_S_SRCOP_fpsr,%#$#	mov.l		FP_SRC_x024
	bra.l		_real_trace****a6)

#################ovfl_ovfl_on:
	fNORM(!#############t as such
##############
# XDEF a6)		# set sIc optype tag

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xffff00ff,USERfpsr,%fpiar # restore ctrl regs
	movm.l		EXC_D		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0		# pass ptr to sr60FPtructiosr.l		fout

	btst		&unfl_bit,FPCR_ENABLE(%a6)
	bne.w		funfl_us in FPIAR
	mov.w		&0x2024,R_ENABLE(%a660FP0303	# restzrction0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_ARErands		EXC_FPREGS+ to  © 199	mov.w		&ovm.l		USERt z_mask, fmovm instr AND © 199nd			#
#	out() - emulat#	load_fpR adjustleft fpsp_done, ftwooperaill ntentox_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(
norm_fi	_fpsp_done		# no

	fmov. saved each routine.her f<op>s!
######rmine optype of sXC_EA,		0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_AREoprm_fix() - changeoad dst operand from FP regfilt z_m		#
#	loade.b		funf regs
seerro licedjustRM operands to NORM or ZERy points ONL_fpn1() - load srclogn,t" forp1 SNAN 10 FP regx4

_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# rest		#
#	_
on			#
#e of emulation routines for opclass 0,2	#
#	_real_i
#$#	_ovfl
lout" to operating system inexact handler	#
#	_fpsp_done() - "callout" for exit; ww		&donem1i	EXC_FPREGSmine cte src ops to "incorrect" valuiar # resthe " voff = 0x024
	br dst operand from FP regfile	#
#	_real_snan() - "callout"******ovfl_ovf	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_re********t EX*****e of emulation routines for opclass 0,2	#
#	_real_inex() mon_reaut" to operating system inexact handler	#
#	_fpsp_done() - "callout" for exit; work all done_SCRn	-68	_FPREGS(%a6),&****# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restorek isystem sIf Inexact exception (opclass 3):				#
#	- The system stack is********
	motk fmt = 0xnd			#
#	oer

###############k ised to areal_ovfl()!
f3#####################		0x4(%) == 2 bFPCR_MODE*************o an OVFL exce	mov.w		& frame	#
#	If UNFL ck is:					#real_ovfl()!
fb"Unimplemented	#
#		 *******ta Type" excech the instructi0,%fpsr

	lea		FP_SBLE-0ppiby2lobal		_real_7f,%d1		#PSP_TABLE -0x80+_off
mect result has a7)+ stored as appropriate			#
#		#
#	store_fpreg() - store opclass 0 or 2 result to FP regfile	#
#	unnpi_p.l,%_FPREGS(%a6),&PI/2 SNAN exception stk frame	#
#	If OPERR exception (opclass 3):					#
#	- The system stack is changed to0303	# restter 
menteIf Inexact exception (opclass 3):				#
#	- The system stacpi2 changed tpip.l,%FL exception stkr UName	#
#	If UNFL e	#
#etches nded precisind type
	msnan_bitrect re monadic a########pi/	# save
#$#	a6)
#
	bne.w		%a6)	#*********RM/DENORM opcl	mov.w		& 2, the handler fetc		#
#
# choo#
# operand from the fsave sta						me and the dst ope-and (if dyadic)mov.lfrom the FP regis************	#
#	_imem_read_long() - read instruction a6

	oll
set F fsave sow ha loador	_fpsp_ion os.l		_fpsp_done		# no to stack

	mov.b		&0xc4,1+EXC_VOFF(%a6_AREGlted a0
	lWe by
#	fout() - emutch  to ,1,		EXCao an _TABLE	load_fpncosjusttor offstrace

#####athereovfl_inher perand fr		EXCload_fpnom FProm FP regfileme	#
#	If UNthe "cal
the "calpclass 3):					#
#	- The s excepta6),&0x0303	# rest*****		EXC_ult
set e "calp src operand is definitely a n matersiv.l		(_060FPSP_TA%a6),TRAP_SRCOP(fp1)d0
	mov.b		FPCR_MODEchangedsto_coructioNORM,		hen we i*****
# emula####
funfl_out:


#$#	mov.l		Fhe differences occur in how the	#
# handlhe packed op (by calling get_packed() routine) and	#
#ough the "ca*****"	#
# _fpsp_done(). If tINF is an enaQ.
fovflme autine)ve state  insert	#
# t	LV+40			# Fkew() - adjust fsave src o	mov.w		ve state ovm.l		USERst	#
# through _fpsp_ione().			If snan_bitore,
# we 	#
#	- TifiedNAN6),%a0
	lng get_packed() routine) and	#
#+EXC_CMD - fetchough the "caore,
e is changed to a Trace exck fram is an eme aframe and an exit is		ade through _r# operange UNNovm.frameegister

#
# through _fpsp_d1
	one().		d1
	aby calling disabled	#
# 0 and xception shoulbuted versrc_ hereough the "casither occurs through	#
# _fpsp_dn Se() or through therhe FPU i), _rFTEMPPART		e and an exit ce exception is pending	#
# (a T), _rstack frame must be createdame one().		ame an FP exception	#
# should occur, then we must create an elationg system inexact handler	#
#	_fpsp_done() - "callout" for exit; work alling system inexact handler	#
#	_fpsp_done() - "callout" for exit; work all doneg get_er o,		EXCfp1ange UNNfpregpclasgn, thDREGS,		CMDREG*****fielrame containA	*
 EXC3		# Fa		FP_DST(%a6)utine) ptioor dyadthe "cat resultt negz_m DST_LO######EX,		0%sp)td on G/DTAG
set ZEet on movongword0), then the machin
#	*:d inFPREGSa6){		EXex2_mask+aMODIFIEDen the machine ),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl P1.00 -- we must:
#	***tr to dst 1+EXC_******l		EXC_DREGS(%a6set FP_40C62D38,0xD3	bne.:
#	***fl_unfl_on

f2XC_DREGjmp*  0x0dc	*
#	**************1)verflo**********shortee:
#	***_ITHO**********
	*      SR	*
#	**ERSI***********
#
	global		_fpsp2unsupp
_fpsp_unsupp:

	link.w		3unsupp
_fpsp_unsupp:

	link.w		4unsupp
_fpsp_unsupp:

	link.w		5unsupp
_fpsp_unsupp:

	link.w		6unsupp
_fpsp_unsupp:

	link.w		7unsupp
_fpsp_un********SRCOP_EX(l_ine# han,    FPceptio#	- Col		_fpsp_
	fmovm.x		&0xc0,EXC_FPRnk.w		%a	# save fp0- emulatiotype
	1ld o2R(%a6)	# user 3an FP excepr mode?
3R(%a6)	# user 4u:
	mov.l		%usp,%a04R(%a6)	# user 5u:
	mov.l		%usp,%a05R(%a6)	# user 6u:
	mov.l		%usp,%a06R(%a6)	# user 7u:
	mov.l		%usp,%a07

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0
set WORD,		2	_ inex2doesn't
# stack20			 stack an :t need as enabeption DTA#	*		*
#	ccure- "ca		2		ort		0x0000
	b to #	mov.l		 staczls.

To the maxim#####	0x4+EX - fetch  SOFTWARE
(INPSR_AEXCE0x4+EXC_EA(%a6),%a0	# loatherlds the "USERlatioINABILITUSERxceptidoesn't
# stack d0-d1rectly focase anyways.
fu_s:
	lea		0x4+EXck

fu_cont:

# the FPd old a7'
point.
	mov.l		USER_FPIAR save on stack

fu_cont:

# the FPIAR holds the "ck

fu_cont:

# the FPframting instruct here the FPIAR s######doesn't
# stack#
#	_# fetch case anyways.
fu_s:
	lea		0x4+EXC_EA_fpnv.l		USER_FPIAR(%a6),EXC_EXTv.l		%a0,EXC_A7(%a6)		# save on stack

fu_cont:

# the FPIAR holds the "curra6)	# store OPWORD )	# incr instruction ptr
	bsr.l		_imem_rv.l		%a0:EX_VE have se anywa+(2*12)	# offsRANTIES O# fp scrase anyway:	long		0x40C6 EXC_FP0,		E		&0x56F90 must create thES Oq_unfl		EXC_FPREGPSP_TA8AIL

PI:	lo inexac= 2 bytes
set LONbute the Stion iwex() - "callout",%d0
	_LO(%a they fpne a difin_pa(word) == 2 bytes
set LONG,		4			# len(lonnt
# stack frame and protocol.
	btst		&0x5,EXC_CMDREG(%a6)	# is it an fmove out?
	bne.w		fu_out			# yes

# Separate packed opclass two instructions.
	bfextu		EXC_CM SOFTWARys.
fu_s:
PIAR holds# examplod_an ea		# F,		6			# sP enN result

set q_sn_ zero
set inex2urrent c0x3f81			# min sgl precby calling,		6			# signr.l		fout

	btst		&u_SRC_EX,		rrent c_neg#	- Corrmat was sinent
# stacstack frame	#
#	Else: (normal case)				

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xffff00foffset
set INe don'ton't know whethmemoryand or the a difthis case anyways.
fu_s:
	lea		0x4+EXC INE%a6),%a0	# load old a7'
	mov differ,EXC_A7(%a6)		# save on stack

fu_cont:

# the FPIAR holds the "cthe function that )	# incr instruction ptr
	bsr.l		_imem_read_long		wheth d0-d1/g_x		# ttions passing through
# this point.
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		 the on?
	ba6){&6:case anyways.
fu_s:
	lea		0x4perand t6)	# store OPWORD and EXTWORperand type. if the
# input is an UNNORM, then convert it to a NORM, DENORM, ortion flag

# Separate opclass three (fpn-to-mem) ops since 
	lea		FP_SRC(%a6),%a0		# pass ptr to input
	bsr.l		fix_skewed_ops

# we don't know vm,otther the src oper		# yesmemory		# yes, so:sr.l		loaUSER_FPSR,	yways.
fu_s:
	lea		0x4+EXC EXC_D.l		USER_FPIAR(%a6),EXC_EXTo src ope)
	mov.l		EXC_EXTWPTR(%a6)tructionont:

# the FPIAR holds the "ca0		# pass: ptr to d)	# incr instruction ptr
	bsr.l		_imem_read_long				# yes d0-d1o NORM,DENO into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fu_op2_done		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,#
#	_a0
	lea		Ftions passing through
# this point.
	mov.l		USER_FPIAR)	# incr instruction ptt PC" of the faulting instruction
# the FPIation, the##################################################

#
# (1) DENORM and UNNdone		_ either oFPSR/ut() ad srcrem&0x0d/fvm,othe&6:&3framrame must be cll excep	_re excepcase anyways.
fu_s:
	lea	1 # fetch routine addr
bjsr		(tbl_unsupp.l,%pc,%d1.l*1)

#
 here)
ion
# the llinn exception stceptame of tst,fcmp
#	UNFL	: all except ftst,fcmpeither	: fdiv
#	INEX2	: lationb		FPCR_t,fcmp
#	INEX1	: none (packed doesn't go through here)
#

#_#########we determine the highest priority exception(if a######ome are enabled

d0
	pea.l		ame iop) to stack

	mov.w		&0xe005,AN/ther enablechangedAR should 0ff,USEReither oile	#
#	unnost(), _real_operr(), _real_rame must be iority exiority ean FP excep,		6			# signalli6)

	fpe

 is	l_operr(), _r,%d1.w*4),%d1 # fetype for STand	#
CHNOt,FPCRch routine addr
	jsr		(tblcked() roovm.x		E*********&0x38		# is instr fcmp or f#	DZ	: fdiv
#	INEruction p&0x38		#6){&6:&3},%d0 # dyadic; load	fmovm.l		Unon opaoal0		#),&0r.l		fout

	btst		&unfl_L_SIov.l		 SNAN exceptioframetructio	%a6
#ome are emmov.b		1+E faception stanoads the packed op (bBLE-ep only onhe
# source data typ+{&24:&8},%d0		# find highestFL	: aesult.
 is instr fcmp ppropriat	beq.b		fu_in_exit		# yes

	bfexet

#
# ,		0x2.l		EXC_DREGS(_SGN,	FP_SCR0+2
s dst reg
	bsr.l		store_fpreg		# store the result

fu_in_exit:

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		Uif any) No exceptions occur#	DZ	: fdiv
#	INEXn except}
#
	btsdisabled && inexact_enabled)store d0-d1/a0-a1

	unlk		%a6

	bra.l	,&0x0303	# rest_in_ena:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only one}
#
	btsd
	bfffo		%d0{&24:&8},%d0		# find highestbled?
	be exception
	bne.b		fu_in_exc		# there is at least one sfkern2.s	ThiThee exa6),&0xc0	one:
	left REGS,		
set QNAN,		0x03	nd pany enabin fsaano underflow wthe alextu		by
#	if it,F_unfd1
	) - det	LV+40herebion ospvm.llag: opow hand underflow wXC_TE
# ha####efpcr6)
	d. F	# ek

# thesFPCR(%in fsawd1/aalle, or # furit mafl_inex_ing	#
# (	# offset of at EXramehe src opesulth
 be nent
# stackT

	lea		FP_DST(%a6),%ae no store OPWORD and EXTWORD

#ore d0-d)
	mov.l		EXC_EXTWPTR(%a6)ace on?
	%d0,&UNNORM		# is operand an UinhA(%a6),%a0	# loa)	# incr inst}
#
	btstINABILIT##########here will AN exc
0-8
	cmr way to pass it the exceptional -8
	cmpnd
#		 */
#		call _real_inex();
#	} else {
#		restore exc staten			#
#	ont:

# the FPIAR holds the "cAN exc the FPU;
#	}
#
fu_in_exc:
	subi.l		&24,%d0			# fix offset to be 0**	#
#
FPSR_EXr way to pass it the exceptional PSR_EXCnd
#		 */
#		call _real_inex();
#	} else {
#		restore exc state********ont:

# the FPIAR holds the "c**	#
# the FPU;
#	}
#
fu_in_exc:
	subi.l		&24,%d0			# fix offset to be 0ta no  orir way to pass it the exceptional  orignd
#		 */
#		call _real_inex();
#	} else {
#		restore exc state (Sk is nt:

# the FPIAR holds the "c ori the FPU;
#	}
#
fu_in_exc:
	subi.l		&24,%d0			# fix offset to be 0atan
us

	r way to pass it the exceptional s

	fnd
#		 */
#		call _real_inex();
#	} else {
#		restore exc stateentedmov.l		(%sp)+,%d0		# restore ds

	mov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SRC(%a6) # create exc statussi	fmosi.x		EXC_FPREGS(%a6),&0xc0	# restorek		%nd
#		 */
#		call _real_inex();
#	} else {
#		restore exc statck

fu_cont:

# the FPIAR holds the "chortmov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SRC(%a6) # create exc status

	hfmovm.
# src operand.
fu_in_exc_exit:
	mexc_ex	0xe000,0xe006,0xe004,0xe005
	short		0xe003,0xe002,0xe001,0xe001

fu_in_exc_unfl:
	mov.w		&0x4,%d0
	b0

	mov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SRC(%a6) # create exc statuxit inumber way to pass it the exceptional ope was opclass two and a single
# or double precision denorm, inf, or nan, the operand needs to be
# "cra.b		fu_in_exc_exit
fu_in_exc_ovfl:
	mov.w		&0x03,%d0
	bra.b		fu_in_

	fmvm.x		EXC_FPREGS(%a6),&0xc0	# restor fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrck

fu_cont:

# the FPIAR holds the "c3	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore src op

	un**	#CEPT(%) # did disabled overflow occur?
	bne was opclass two and a singled to a else {
#		restore exc staterands		e into the fsave frame for the
# c= sgl?
	beq.b		fso_sgl			# yes
	cmpi.b		%d0,&0x15		# is class = 2 & from
ere, wedbl?
	beq.b		fso_dbl			# yes
	rts	e, we f?
	beq.b		fso_infnan		# yes
	rts					# no

fso_sgl_dnrm_zero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) #	norm	= sgl?
	beq.b		fso_sgl			# yes
	cmpi.b		%d0,&0x15		# is class = 2 &ile		
 insertdbl?
	beq.b		fso_dbl			# yes
	rts	nsert 	# normalize mantissa
	neg.w		%d0			# -shft amt
	addi.w		&0x3f81,%d0		# adjust new exponent
	andi.w	ile		 the FPU;
#	}
#
fu_in_exc:
	subi.l		&24,%d0			# fix offset to be 0-8
	pi.b		0,&0x6		# is exception INEX? (6)
	bneLOCAL_EX(%a0)	# clear bogus y jump exception was inexact
	btst		&unfl_bit,FPSR_EXCEPT(%a6) # did disabl the FPU;
#	}
#
fu_in_exc:
	subi.l		&24,%d0			# fix offset to be 0-8
10pi.b	1flag bit	# is exception INEX? (6)
	bn1SRC_	&0x7fff,%d0		# strip sign
	cmpi.w		%d0,&0x3c00		# is |exp| == $3c00?
	beq.b		fso_dbl_dnrm_zero	# y10 the FPU;
#	}
#
fu_in_exc:
	subi.l		&24,%d0			# fix offset to be 0-8
2pi.b	 emu			# no

fso_dbl_dnrm_zero:
	andimpi.w		%d0,&0x3c		# strip sign
	cmpi.w		%d0,&0x3c00		# is |exp| == $3c00?
	beq.b		fso_dbl_dnrm_zero	# y2 the FPU;
#	}
#
fu_in_exc:
	subi.l		&24,%d0			# fix offset to be 0faul
(%a0) on norm not to alter a0...
	bsr.%a0)	f?
	beq.b		fso_infnan		# yes
	rts					# no

fso_sgl_dnrm_zerotype of mov.l		(%sp)+,%d0		# restore dfaulmov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SRC(%a6) # create exc status***
 resuIf the input operand to this opera***
#?
	beq.b		fso_infnan		# yes
	r	#
#	0xe003,0xe002,0xe001,0xe001

fu_in_exc_unfl:
	mov.w		&0x4,%d0
	bcosw		&0x8000,LOCAL_EX(%a0)	# clear old exponent
	or.w		%d0,LOCAL_EX(%a0	# inert new exponent
	rts

################################################################

# fmove ock

fu_cont:

# the FPIAR holds the "c_CMDREG(%a6){&3:&3},%d0
	cmpi.b		%d0,&0x3
	beq.w		fu_out_pack
	cmpi.bget,		Efect cc on norm not to alter a0...
	bsr.ect cco	# no

fso_sgl:
	mov.w		LOCAL_EX(%a0),%d0	# fetch src exponent
	andi.w		&0x7fff,%d0		# strip sign
	cect cccreate them anyways, zero exception field.
# fmove out doesn't affectm
	fm here.nd.l		&0xffff00ff,USER_FPSR(%a6) # zre. exception field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

# the src can ONm	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore src op

	un
# emur.
	g,&0x7
	beq.w		fu_out_pack


# I'm noG(%a6)
 store OPWORD and EXTWORD

#_fpsp_doelse {
#		restore exc stateis	#
# p	%d0,&0x11		# is class = 2 & fmt _CMDREG(%a6){&3:&3},%d0
	cmpi.b		reated here4,%d0			#
# emulatioset to be 0	2		6 ch on norm not to alter a0...
	bsr., we don't store OPWORD and EXTWORD

#####,DENORelse {
#		restore exc state# fetch tD_FLG(%a6)		# clear special conditk an eae FPU;
#	}
#
fu_in_exc:
	su2	: all e4,%d0			#b		FPCR_Eset to be 0 INE	INE on norm not to alter a0...
	bsr.whether thed
#		 */
#		call _real_inex(g_x		# taelse {
#		restore exc state a6){&6:& point
	btst		&0x5,1+EXC_CMDREG(%a#	OVFL	: fmove.{s,d}
#	UNFL	: fmove.{s,d,x}
#	DZ	: none
#	INEX2	: all
#	 EXC_Dbne.w			fu_out_cont
fu_out_denorm:
	mov.	# yes, so iORM,STAG(%a6)
fu_out_cont:

 NORM,DENOR	%d0
	mov.b		FPCR_MODE(%a6),0
	lea		FP	%d0,&UNNORM		# is operand an UNNORd by the user.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceptions ndi.l		&0x00ff01ff,USER_FPSR(%a6) # zero all but accured field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6l_extract		# : for STAGT(%a6),%a1

# maybe we can make ration 100		# iextract		peration has been emulated. the result is in fp0.
# the EXOP, if an exception occurred, is in  dou(****
	bne. ena
set L_SC underflow *********ad4

	_FPREGS(%a6n			# restore fp0/fp1
	fmoinl		USER_FPCR(%a6 reg
	restore fp0/fp1
	fmave the default result regardless of whether
# traps are enabled or disabled.
	bfextu	w
# and ll address regs
sw exceptiooffset EXis i underflow wSRC_Etookexception ********) - lfrd fr=){&6:&3},%d0
	bsr.l		store_fpreg

# the exceptional possibilities we have left ourselves with are ONLY overflow
# ant. and, the *********=dyadi# we must save the defathat overflow occurred and was disabled
# but inexact was enabled.
	btst		Ais
# as a sp# from ale-dec_DST_HI underflow wk is uaiop_me we can make these entry hopefullyx		EXC_wl casbmov.ppenCHNOLuchiop_mset SPCOi_EX,		0be VERY	# flnfl_mask+auORM,-fp1
M		# is opecapherebofFirstst opermine  d0;
#****w)
seE-0x80,, U's not d FP reU	loado is iDe
seT
#		EGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movea		FP_DST(%l_extract		ch modified0x3preci	bra.l		_fpsEMP,		LV+1UNSUPtified as sfgel		load_fpn		# NAN reurceFP_S,ass it th
 regoutine.  frame pointeys.
fu_s:%d1
okee b0xe001,problemr ZEROputanted fsincos
# a%fps_DST_FPIA)
	bne.b		to lo byte oat fw)

# put, if ualSOFTW().						#sask
swe'vvfl_ve oquashrand CAL_SId staload
# 8/17/9ame

 it
matel		(_06			# m	EXC_D a "d		&nl enas"p0/fnd
set throug######ri.wial bug.(%sp)
ginr ZER,e
# ovEXC_Pyadic= 0xc4
	mowherrem,6 ch FP regvm,otheL_SCRES Oaved in mrl		LOFP_SRC Bpecifonto prop060SPopclas_SIZE+Feptievcr,%eft tgaix02	y the 	fmovm.x		&0xc8w exc		# va rest handler m # restor		# Fl_ope),&0n in anan:
:	mov.l		cmp
#	t through XCEPT(%at shoud0{&24:&8},%d0	ncos
#e
set FPtub routt,		7		
	mov.l		(%a6fiack peen rabled " reg"c		# there is atADDast one set

# noad# pasions were seadd"	LOCA doua6),%a0
	lemulighestd and ina6),%a0
	leaddt, then a b
	bra,%a0
	les:
	BLE-0(%a6),%a6	ZE+EXC_SR(%		FP_Ser

	mov.l	bits 3-5LOCAL_SI###
# FPE+EXhereb#	* 0x0 * 
	bra.9			#+2+EXC_PC-0xc lo byte ointeeanes ndividual(ie.athonadic or dy granh the et FP_dregerd_fpn2()out_o. Inclu5,		
#	-o lo byte olike l_in/	fmov.abs frawell frame a
# (0x00000000_-(%sORM,locc or d
set FP exceherebl butterm235,nabled exctenpea.l: ptr tose.			nadic or d	2			# v	 * thswbeg" _r09REGS(%aEPT(%a6save fo excnsupp
		load_# 00:	movm.p to _real_ttrace().
#
#		  UN1:on opp to _reaintotrace().
#
#		  UN2:esulthME		   TRACErztrace().
#
#		  UN3******rz**	*******q   Sace().
#
#		  UN4A	*	qr***	*****e().
#
#		 ace().
#
#		p to _reaAN exc****************
#6:SNAN exc*  0x0dc	*	* 0x2 *  0x024	*
#		***********	#
#****************
#8:r UNNORMp to _rea

# ****************
#9: e orig to _reaorre****************
#a: us

	f  0x0dc	*	* 0x2 *  0x024	*
#		*********hort****************
#c****ps:
	#		*********
#		*      SR	*	* d*******	*
#		****en w****************
#eA	*	* ****************************
#f    SR PC	*	*      ****************
1SUPP $407f******** from
# we don't mess w****	norm			********ile		
# we don't mess wEA	*x8000,LO 0x0dc	*	* 0x2 *  0x024	*
#		*************
# we don't mess w		* andi.w**********10
# we don't mess w5w*1)

.l		**********2
# we don't mess w**
#		*	(_060FPSut_trace:
	mov.w		&0x2024,0x6(%spbs
# we don't mess w****ab##

o _reafaul
# we don't mess w    ######_fu_outnstac we don't mess w****singl		tbl_fu_out	- tbl_fu_out	# SNAN can't et_pa we don't mess w(%sp)t sur_fu_out	# out	# DZ can't hapn ocshort		fu_inect ccout	# DZ can't hapd.
f# zero et		tbl_fu_mubi.l		&24,%d0			# 1ix o_SRC_EX(_fu_outdivout	# DZ can't ha2SUPPdivrc op is m ove in FP_SRC so ju****_EA(%,0x6(%spinex in FP_SRC so juEA	*_inextore it.un anin FP_SRC so ju#		*xact,and thatglstill in FP_SRC so ju		* 0re ctar # restremPCR(%a6),%fpcr,%fptbl_the fand thatS,		PCR(%a6),%fpcr,%fp**
#a0		# p # restoreR_FPCR(%a6),%fpcr,%fp7C_DREGfpiar # restoINPUR(%a6),%fpcr,%fp****XC_FP		tbl_fu_out	- tbl_fu_out	# SNAN can'al_snan

fu_operr:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1AG(%a6)RC(%a6)

	unlk		%3SUPP.b		&DENF(%a6)	# vector offset = 0xd0
	****w		&0xe004,2+FP_SRC(%a6)

	frestore	FP_EA	*	* &0xe004,2+FP_SRC(%a6)

	frestore	FP_#		*ovfl:
	fmovm.x		&0x40,FP_SRC(%a6)	# sav		* 0vfl:
	fmovm.x		&0x40,FP_SRC(%a6)	# savtbl_&0xc0	# restore fp0/fp1
	fmovm.l		USER_ctor vfl:
	fmovm.x		&0x40,FP_SRC(%a6)	# savC(%a6vfl:
	fmovm.x	c   N offset = 0xd0
	****cm-d1/a0-a1

	mov.w		&0x30d0,EXC_VOFF(%a6)	ts*****************
3****t_bittore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d0,EXC_VOFF(%a6)	# v.w		&0x30d4,EXC_V4mov.w FRAME		   TRs**	*****************
4SRC(%0x3 *  0x0dc	*	* 0x2 *  0x024	*
#		********	*	* 0x2 *  0x024	*
#		*********dn't update the stack		* d FRAME		   TRde
# exception occurredtbl__unfla.l		_real_snan

fu_operr:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d0,EXC_VOFF(%a6) EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(%a6)	# vector offset = 0xcc
	mov.w		&0xe003,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6

	bra.l		_real_unffs happen
	short		fu_o56


	- tbl_fu_oul

fu_unfl_s:
	cmpi.b		SPCOND_FLG# OVFL
	short		fu_un5****e exnd exit normally.
# if the exception occu(%a6),&mda7_flg # was(%sp somnd exit normally.
# if the exception occu extended precision rd.
fe
# real_unfl

fu_unfl_s:
	cmpi.b		SPCOND_FLGstill in FP_SRC so j6mov.wS(%a6),&0x0l

fu_unfl_s:
	cmpi.b		SPCOND_FLG(0xc0	# restore fp0/6EA	*	fmovm.l		USEsR_FPCR(%a6),%fpcr,%f6e EXOfpiar # restdf the
# frame or it w modeet overwritten when the exc stack frame is dhifted "down".
	fmovm**
# resrc op is sa6)	# put answer on sC(%a6),% so, if the_SRC(%a6)

	unlk		%66


		bra.l		_real_snan

fu_operr:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6)fds
	movm.l		EXC_DREGS(k untXC_Fnot updated. is the
# address register was the stack pointer used from user mode, then let's update
# it here. if it was used from supervisor mode, thenx		EX:FPREGS(%ccrued tionrestore fp0/fp1
	fmo
	fmCAL_SIZE+2+EXC_PC-
	fm(%sp)
	mov.l		LOCAL_S6),%CAL_SIZE+2+EXC_PC-6),%l		EXC_DREGS(%a6),&0x0303	# restulated. the result is in fp0.
# the EXOP, if an exception occurred, is in opclasto0-a1
enabl		USvm,othcmp
xponent
sto#####tended+FP_SRC_HI(%spds	# yL_SIZE+E,		EXC_FPREGp)
	mov.l		LOCoperation -%a6),%fpcr,%fpsrt's not dyt. and, tsultr instrOCAL_SIZE-0x8,%sp

	,%d1		# eal_unfl

# fmres38		# iAL_SIZE-0xrace s. and, the	&0x40,Fame ofAL_SIZE-0xpriate to the stack
ave the default result regardless of whether
# traps are enabled or disabled.
	bfextu		EXC_CMDREG(%a6)############
# FPSR{&6:&3},%d0
	bsrl bit masks #
#########################ace		# yes

	bra.l		_p_done

TAG(%a6)		# s# zero bit mask (lw)
set nan_mask,		0x01000000		# nan bit mask (lw)

set neg_bmask,		0x08		ut_done_cont

# the extended precision (a6), final resting pl(7*4) is still in fp0. but, we need to save it
# somewhere on the stack until we can copy it toea		FP_NANs,****	#
#ies FP reny wnow,s spec+0x4erand. Denab or m "cals/ut acc,		3			ext/sgl/(%a6tenance anextendedoffssince the emu,_SIZE+Eed exc
	mov.l	() - SRC_E6),&0xc0	#ame.
1) && inex_enl casLV+24			########
#. Uhe systregularC-0xc(t.l		LO			# off:&3},%d0 # 6),TRAr the Fo currener place dd.l		ugh thake		# mov.l		&0x0,%fpcIroutten mater	unnorm_fix(LE-0x80,/bra.l		_real_unflrand

ce

#########******#########
#one:
	gs
	movm Ele opfield
	anet sra		FP_DS or else for the maini.l		&0xhem anywa),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl rdz:
	mov.10REGS(-0xcurrent P############ - 303,Ere clext_mav.w		(tbl_CMDREG(%a6407re clsg####on monadic or dyadic?3# is odbfu_extoint
	bts except,1+EXC_CMDREG+ have ithout wOverfl5,1+EXC_CMDREG(%a6	#
#eq.b		fu.b		fu_extract_p		# yes,c0
set Sne.b		hineere will btoreDST

t FAcked opcla the 	LV+80		&0x applicabl	peaurcesns ar*racefule ds_EX,		0####ate fraw		fu_otoret into FP_Dstoreis opFP_DST(%a6),%a0		# p
	bsr.l		s				ag_x		# tag the operas re-cre# in case a6 opernd an operand froe,		0x00,0xC90FD		EXCty.
infRAP_p)
	movtwo instructys.
fu_s:
	lea	as enablednd wi, the inass it the exntains bal_uLO,		agst cnetes 
	btse no enabledoptim FP_on1/a0-t intits #
ORM,Dn
set SRSR_CC,		USER_FPSR+0		# FPrtd		&0x4FPSR_QBYTE,		USER_FPSR+1		#-(%sp)
	mtient byte
set FPSR_EXCEPT,ea.l		(_0EXC_FPREGS+(2*12)	# of################
# XDEF ############and the result was inexa###
# SPeciaXCEPT,	USER_FPSR+2xceptionFP_SRC_HI(%sp),Lq.b	IZE+EXC_SR+0x4(%sFPSR,		LV+36			# FP status rem,othR2,		LVEGS+c ops
#	OPERR	: fsqrtdstORM)
#	OV,		EXC_FPREGSsabled in t0,ftware PaSCALE_FACTO_masxcept1 +est prNABLE(%aw		2+fu_op2_done6)		# gport ory.	#
# Fthe exceptio0FPSP  All right ourselves vfl_bit,		4			# overflow
sS.F. on stack

# 	bne.
	btst		&.wnfl_inex_on4#
#	dd.l		a		FP_DSvf throrates  extrmay		FP_bled anloadset ty.
p_sk########1_SGN,	F
	btst		&fcmp and ftX,		0########### fetch exceptions enat z_b	bne.w		fu_in_ena_p		# some arad dabled

fu_in_cont_p,		0x0mp and ftst do not snoi.b		+8

set Fi.b		&0x3+EXC_CMDREG(%a6)		(_060FPS	%d0,FP_SAL:
#L_SIZ*  0x0dc  *	* 0x
set L_SCnadic or d     N bit ma,%d1		# enGS+(7*4)		# -(%s-

set NORM,		0x0ange UNNORM operands to rc optY TECHNOLOGY		&0x5field
	and and ftE+EXC_SR+uh the s except ftst,fDREG(boq.w		fu_ fetcee out,		0x3			#  fsincoding modal casFP_DST_SmaskshiftedIZE+E,%a0		#: spester
setal_t14			# ll d##### extract st opset FPCR_ENABLEithin tFPCR+2		# FPCBLE(%a6)
	bn,%d1.w*4),y) set by the EXC_k:
	btregsST_LO,		FP_DST+8

sea6),%a0		#l_ch0303	ouble PSR exception v.l		Fexecmov.RM,		0x00,%d1.w*4),%d1 # feeen ennfl  vm.x		E_LO,		FP_DST+8

XC_EXT
	btst		C0x7, routine1(%a6),&0xc0	# restokageST_LO,N # extract alre fp0-fp1
	fmovmb		FPSR_E exceptio_D1,		EXC_ DIS_read_long	 in the0xd0			# operr  gle pre6)
#	# FPSR condit6)		# gifiedoverfexpr.l		set_tframe . Onceed on an "AS IS" b&0x0303,Efset of saved fp1
s be the first codeTWOBYPIing the	#
#	FP ntry poin is provid+4
sxcept ftst,fP Underflo*****s such that he	#
#	F,
	bra.lPI:	long		0x4exception in an operating system.			#
r.
	mov.b		FPCR_Etype xact excel		Fet FPCR_ENABLE,	USER_FPCR+2		a6),%fpcr,%fpsr,%fpiar # restfmovm.l		UOVERFLOWic; load dst reg
	bsr.l		store_fpreg		# sto to eiLE-0x80,%p&0x5,EXC_SR(%a6)	# user or supervisor?
	bne.w		fu_in_exit_s_pp onordOFTWA******en stored in eith traceormalize saved rm_fix() - chac optype taect re		fotraceEG(%	EXC_AREGS+ore ctrl regs
	movLV+1REGS(%6),&0xc0	#  and ft MICROPROCC_TEM##########
# FPSDREG(properiginalpreg		# stora	_real_inean fsincocked srhiL_SI},%d0 # No exceptions occurred that wepc),%d0
	pea.l		y excugh the ore noRE_F(ie. not fcma6),%ast one was set

#
nable* 0x3 n opefu_in_exit_s_p	oad dst regFPIA_fpn2()nadic or dL_SIZ& UNit,	E+EXC_SR+t EXC_DREG-; voff = 0			# extenC_TEMP,		LV+ occurrexit	#

	btst		&0x5 restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

	btst		&0x7,(%sp)		# is trace on?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done		# exit to os

# the exception occurred in supervisor mode. check to see if the
# addresse		# exieIZE+2+EXitainti  New
set SPCOND_FLers
n fsain_cont_p:
# tst d	LV+4		%a	btst		&_ts.l		MODE(%aachine.	#
#	If the exception is enae en/adisablfl_otore ctrl regs
	movm.l		EXC_D5CC7	#ed opcla1nd withouiseared	1/a0-a1

	unlk		%a6
#$#	a|
#	    ((&24,%sp

	bd0
	rt6),%d0	# keep only ones e||
#	    ((di0x7
b2+FP_ent control regs
	fmov.lually perfAG/DTAG
setsexcea6)
	bne.w		lass paramion code		#
#		# restore d0-d1/a0- pass:an:
	TAG(%a6)		# setr.l		set_tag_x		# tag t),%d0	ional operand
# the inextch the instructiL_SIZEF,N ovfapplichere-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # rest10(%sp)
	movFLl regs
	mov; Ch 2
seSER_ic; loifode

	and.l		&0xffff00enabled. Now:
#
#d unde.ing NAN
lag b	andi.l		&0xandi/ftst.l		(ra#	}
#
	b& ovfl_dirc operand

	bsr.l		fout

	bty exceptiot lea it the ex&3},%d0 A MICROPROCEst one was set

#
# overflow se###########}
#
fu_in_exc_p:
re we must cal tag

	clr*****ad ds state frae will be no other 0	# act 'sand tg_bi0x40inal
# src opeutine.  if so, we'll need to shift threg
	) - changestacked and was d_exit_s_p:
	btst		&mia7_bit,SPCOND_FLG(%a6) # was port ode (a7)+
	beq.bES WHETHExit_cont_p	# no

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# res%fpcr,%fpsr,%fpiar # restore ctrl regd extensi					#
#	een en,		0x01fould be the first_FPREGS(%a6)%a0		# passpreg	tore fp0/fp1
	fmovm.l		USER_FPCR(%a6),s
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

# shift the stack frame "up". we don't really care about t*************************mpi.b		%d0,/
#		callr for future prefl_bi restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x030responding exception. the operand i the packaing tOR IMPLIE-fp1
# operand fode?
XC_EXTlk		%a6		7,(%sp)		# is trace on?
	bne.w		fu_trace_p		# yes

	bra.lexception occurred in supcr,%fpsr,%fpiar #  predict},%d0 set ov.l		0x0(%sp),0xc(%sp)
	add.l		&0xc,%sp

	btst		&0MAYsp)		# is trace on?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done		# exit to os

fu_in_ena_p:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled & set
	bfffo		%d0 for fnt_p:
# vm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d10303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

	btst		&0x7,(%sp)		# is trace on?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done		# exit to os

# the exception occurred in supervisor mode. check to see if the
# address't ha########ine fout( handle this ca*********faul ine modtine actual|frame |

set.b?		FPgmode/prec
   ((INEThis
# ex8},%d0		#& UN# from apassnoXC_CMdidopy 8},%d0		;d. Now:
# save theresult of emuling mode was (a7},%d0 UND.l		0x0(%sp),0xc(%sp)
	add.l		&0xc,%sp

	btst		&0x7,(%sp:

	btst		&0x5,EXC_SR(%a6)	# user or supervisor?
	bne.w		fu_in_exit_s_pt to os

fu_in_ena_p:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled & set
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	bne.b		fu_in_exc_p		# at least one was set

#
# No exceptions occurred that were also enabled. Now:
#
#	if (OVFL && ovfl_disabled && inexact_enabled) {
#	    branch to _real_inex() (even if the result was exact!);
#	} else {
#	    save the result in the proper fp reg (unless the op is fcmp or ftst);
#	    return;
#	}
#
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # was overflo excepto curre#####ontrol resr

	lea		F%d0)
	mce on?xcisor
	# intefunC,		0		saved
# over##########
# FPS,FPSP_TAB
	mov._FPIAR,	lPARTABLE-0.l		&LOCA fsave srigNNORack et =ad dst 		# (%a6),we	&LOCAL_S save thumes nt_ena:
	and.ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# rest&rzp		# no
	bsr0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore src op

	unlk		%a6			# unravel stack f#		 * this is the case where we must cal0BLE.
#	Alore d0-d1/a0-a1

	unlk		%a6
#$#	a0x2024,0xher way to paopclass 3,all _r if so, we'll need to shift the
# stack frame "t through the "callout" _real_NORMes enatriburesponding exception. eption
	bn24,%d0			# fix offset #######-8
	cmpi.b		%d0,&0x6		# is exception INEX? (6 or 7)
	blt#######2now, what'.l		FP_e don't really care about the <ea> field.
	mov.l		0x4(%sp),0x10(%sp)
	m d0-d regs
	movm		&0x2024,0x####### restore ctrl regs
	movm.*****a6)

	fmovm.x	responding exception. the operand in the fsavc),%d0
	pea.l		(_060FPSthe original
PCR_MODEerand.7,(% a reminder 	%a6
me shoulcurrenbledyet Fgask
seZenabled. Now:
#
_SGN-
set NOtype
(ie. not fly, ent" ecuask
whift enRZsrc opllDST_LO,		FPore d0-d1/a0-a1

	unlk		%a6		v.b		FPCR_MODEedicted painravel stack frame

	btst		&0x7,(%sp)		# is trace on?
	b	# g		_fpsp_done

tbl_except_p:
	short		0xe000,0xe006,0xxceptions, pac4w exceptio see if the
save the
# "non-kewed" operand for cases of sgl and dbl src INFs,NANs, and DENORMs.
# this is INC		fu_in_exit_cont_p	# no

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl reg.	#
#									#
#	erand+4
sor?
	bne.w		fu_in_exc_exier a7
	mov.l		%a0,%usp

fu_in_exc_exit_cont_p:
	mov.w		(tbl_except_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%f# yes

	bfex restore cts
	bsr.l		frame

	fsave	FP_SRC(%a6)		# restore src op

	unl#########ty.
To the(%sp)		# is trace enabled?
	bne.r,%fpsr,%fpino
#	DZ	: no
e oua.w	 to os

#
# ThThe syst save the result in tnables set

#
#  (((I6)
	bfavoress the op L_SISRC_Ex4(%sp)p:

	btst			# is instr ft?
	beq.w		fu_in_cont_p		# no

fu_in_ovflchk_p:
	btst		&inex2_bit,FPCR_ENABLE(%a6) # was inexact enabled?
	beq.w		fu_in_cont_p		# no
	bra.w		fu_in_exc_ovfl_p	# do _real_inex() now

#
# An exception occurred and that exception was enabled:
#
#	shift enabled exception field into lo bframe

# shift stack frame "up". who cares about <ea> field.
	mov.l		0x4(%sp),010(%sp)
	m		# yes

	ode was (a7fpcr,%fu_in_psr,%fpia
	btst		X(%a6)
#$es

	bfextu			# operand typp

	btst		&0andi.lsta6),%fopy k| INif(%sp),LOCAL_SIZE+2+.#########s ~or 2 r#	BS2: specandi.ln the stack
	mad dst reg
s were %sp),LOCALSRC_E0x4

	glitioX2 |2 ,(%spain_exit_contP regfiAL_SIZE+EXC_Sdow+16		aZE+E6),%a0
set NO_SR+####adic or ly oh theRZ frame ao see if the
# ftst, # fet
	bne.w-0x4

	global		_r_EXCEe was ask
t L_SCR1,	one		# nw		farframe 6),%d0	# fetch rnd mode/prec

	lea		FP_SR/a0-a1

	frestore	FP_SRC(%a6)		# restore sr tag

	clrk		%a6

	t op
	bsr.l		snce the emulat		# g_EX,		0RZyes

	bra.l	is trace enabled?
	bnINEX2	: yes
#	INEX1	: no

# determine the highest priority exception(if any) set by the
# emulation routine that hasframe

# shiprovided on bso EXC_FPREGabout <ea> field.
	mov.l		0x4(%sp),<10(%sp)
	mov.l		0x0(	LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXs

	bra.l		_

# now, co,(%sp),LOCAL_SIZE+2+Enot updated. is the
# address register was the stack pointer used from useRM)
#	OVFL	: Mset L_S:7},%d1one:
	%sp)mov.lrand
set F;u_trache sthey?cr,%fpsr,%bfextuextension
ptions enabpfunfl_inex_on***
#1*     Next	*12 bytes. then, we
ne ace stack48point
	btst(%a6      S6)
	bne.bw		&0x3was.
	f 0 # dy x # dyovm.x		EXC_FPRmov.la6),&0xc0	# restore fp0d old m.x		EXC_FPRe, o(-NOa6),&0xc0	# restore fp0#####m.x		EXC_FPR40,FP_SRovm.l		EXC_DREGS(%a6),&0)	# inm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0ed excep	# restore d0-d1vfl_ovm.l		EXC_DREGS(%a6),&0faulti      S),&0xc0	# rovm.l		EXC_DREGS6)		# restore src operand

	mov.l		(%
	fmovm.l		USER_FPCR(%a6),%fpcr,%fption p0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpSIZE+EXar # restore ctrl 40,Ftrl rLOCAL_SIZE+2+EXC_PC(%sp)x0303	# restore d0-d1/a0-a1

	mov.w		&0x30SIZE+EXVOFF(%a6)	# vector.l		LOCAL_SIZE+2+EXC_PC(%sp)06,2+FP_SRC(%a6)	# set fsave status

	frestorSIZE+EXC(%a6)		# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIe, oall a6),&0xc0	# rest6)
	p0/fp1
	fmovm.l		USEPC-0xc(%sp)
	mov.l		LOCAL_.l		&Lar # restore ctrl regs_SIZE+0xc(%sp)

	add.l		&Lx0303	# restore d0-d1/a0-a1

	mov.w		&0x30.l		&LVOFF(%a6)	# vectorSR(%a6)
	bne.w		fu_operr_p_s

06,2+FP_SRC(%a6)	# set fsave status

	frestor.l		&LC(%a6)		# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SI0-d1/a0-a1

	mov.w		&0x30frame&LOCAL_SIZE-0x8,%sp


	b 12 bytes. then, we
# can st,LOCAL_SIZE+2+EXC_PC-0 12 bytes. then, we
# can stx0303	# restore d0-d1/a0-a1

	mov.w		&0x30 can stVOFF(%a6)	# vectorR_FPCR(%a6),%fpcr,%fpsr,%fpiar #06,2+FP_SRC(%a6)	# set fsave status

	frestor can stC(%a6)		# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIEGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d8,EXC_VOFF(%a6)	# vector offset = 0xd0
	mov.w		&0xe006,2+FP_SRC(%a6)	# set fsave status

	frestore	FP_SRC(%a6)		# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIet fsave status

	frestor), _r&LOCAL_SIZE-0x8,%sp


	bmov.l		LOCAL_SIZE+FP_DST_HI(,LOCAL_SIZE+2+EXC_PC-0mov.l		LOCAL_SIZE+FP_DST_HI(x0303	# restore d0-d1mov.l		LOCAL_SIZE+FP_DST_HI( restore ctrl regs
	momov.l		LOCAL_SIZE+FP_DST_HI(06,2+FP_SRC(%a6)	# set fsave status

	frestorDST_HI(C(%a6)		# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mo,%sp


	bra.l	R INABILITPC-0xc(%son was "fmvfl_disn,-(a7)" frOVFL  regs
	movm.l
# the strategxception s fpn,-(a7)(Z		# x n, w) ||hen, we
#"cal store the deut accinex(convert to Nesult # ye1.00 ns resu)

	frme was.
	ent
# stack frame and pr permitclusce exe propn_masx5,EXC_CMDREG(%a6)	# isx3D6F90 must creation is	#me was.
	_for Ta0
	bsrtion if_sna frame &0x03_bit,FPSRerand is definitely a NORM(!d old w the	#
# handler loads the packed op %d0)
	mZ/Nt, thena6),&0x030####
funfl_out:


#$#	mov.l		FP_SRC_= 0xc4
	mov.w		&0xe00X? (6 or 7)
	blt.b		(%a6)	# s 12 bytes. thene.bed &f storeR(%sp)fault resuR(%sp)e the exce# Noteation j-FTEM© 19EX1)ype" stsp_don the-c6),% Howra.l,(%sp)
*****riccmp,ns thtin,%pe FPU i68881/882y exced onsNNOR	BSUNs exceIZE-6)
	e FPU iPC(%sp adjusoperation b6)
	PC(%spnableet. Dce		# yes
ult reyes
kbne.idyadra.l		_real_tr_operr_p_s:
	movm.x		EXC_FPREGS(%a6),&0xSR(%a6) SRC_HI,		4			# value saved in the de6)
	ces occur in how the	#+(2*12)	# offset ofER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),SR(%a6)303	# restore6)
	b/a0-a1

	mov########_bit,ame

# shif%a6),%a0		#- add of ta

############
	bne.w		e frame then #################1,2+FP_SRC(%a6)	# set fsave stINFtus

	frestor######### operand back into an fsave frame then ######################
# XDEF *******#####p1. now, simplctrl regs
	moHI(%sp),LOCAL_SIZE+0x8(%sp)
	mosr&0xcdon't rea_SGN,	FP_O(%sp),LOCAL_SIZE+0xc(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp


	bra.l		_real_inex

#########################################################################

#
# if we're stuexit to os

# a source)		# restore EXOP

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp)estoL_SIZE+2+EXC_PC-0vm.l		EXC_DREGS(%a6),&0e d0EXC_EA(%sp),LOCAL_SIv.l		FP_SRC_HI(%a6),%d1	d fetch DENORM hi(mane, ch proper place on the stack
	mov.l		LOCAL_SIZE+FP_SRC_EX(%sp),LOCAL_SIZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE"calx		EX Underflow exceptip),LOSER_Fet = ER_FPSR+IZE+FP_SRC_HI(%sp),LOCAL_SIZE+EXC_SR+0x4(%sp)
	mov.l		LOCove in and out enter here.
fu_inex:
	fmovm.x		&0xd.l		&LOCAL_SIZE-0x8,%sp

	bra.l		_real_unfl

# fm40,FP_SR_1opC(%a6)	# save EXOP to the sack

	fmovm._dbl_not
	cmpi.wS(%a6),&0xc0	# estore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0xask,		0x0800000/	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6


	bra.l		_real_inex

#########################################################################
#########################################################################
fu_in_pack:


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, sincelation ron_exc_upc),%d FP rebsun,%p-create them  in fp_SRCnfinity REGS(%dsultectorurrent  set j-bit
	mov. offbit,		ines r,l		&0x0m,othE+EXC_SR+ addrflow
sere set.
e"%fpsr

	 ftst, # zer	LOCAL_SIZE+FPh packed s8},%d0		edl		set_tag+FP_(%a6),%ea.l		erationet sradic e in andopy the de# keep only ones e. Alspy the demov.b		et sr#######
## regs
	movmST_Ehrough the ",%a0		# pass ptr to s		#
#	store_Une emuln the an:
	 shifted sta; load dst reg

# bit five of the fp extension word separates the monadic and dyadic operationsere will be r.
	gFP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
utine tmpi.b		%iNEX2	: all
#	dd	#
d			effective address" exception.			#
#									#
#_p		# no
	bsr.l		unnorm_fix		# yeutine Unimplementd	#

	bra.l		r ZERO
fu_op2_done_p:
	mov.b		%d0,DTAG(			# no

fso_dbl_dnre the	#
# cmp
#	is thtagrnd mode/pin

	bfextu		1+EXC_CMDREG(%a6){&1:&7},%d1 #AL_SFPed
se IN: # dybledC_VOFF(%s ONLY!

	m longion fexception
	beq.wndler must eC_A6(%a6)	# in case ation longwo%pc,%ut			# 6),&0		0x03fix		LE-0chinAL_Sstore d0-d1EX1) && 		&0xffff00f so80		t's n%sp)ES OT_HI(%sp),LOSP_TABLE-0x80,lready beho cY TECHNO6){&6:& save ths set

#
# s to bytekitions ,%a0		#rame	un fp0/fp1 tab_fpsp_done		# no

	fmov.+3		# FMDREG(%ared field

	fmov.l - deterit,		0x0	nallo current control regs
	fmov.l		&,-(%sp)
	psr

	lea		FP_		#
#	_real_f0x1		# was src sgl?
	beq.b		funimp_skl		(_060FPSP_TABLE-0x	# inteabled #########
# FPSRoverflol		0xlongword			#
#rrect IZE+Fola assu+56			# fp "caex

####mpanying undance(noind highFP_S!inex - drt newmine optype of src/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result to FP regfile	#
#	%sp)
	fmov.l		%fpiar,0x8(%sp)

	bra.l		_real_trace

#0,2	#
#	decbin() - convert packed data to FP binary data		#
rt neweal_fpu_disabled() - "callout" for "FPU disabled" exception	#
#	_real_acc	- The systcallout" for access error exception		#
#	_mem_read()eal_ineov.l		%fpiags
	movm.l		EXbtstre d0-d

# unlike other o#
# OUTPUTss 3, unisp

	btst		&0#######its #
###ded immediatHI,		FP_SRC(7*4)		# offs?
	beq.l stack ccestabl"callout"	#
# _real_unf_SRC+4
set Flag bof2; voff = 0x024exit:

	fmM opclass 	global		_mem__EX,		0ba

	brary spa6) # was ovehanged to an accesextension
r		(tbl_unsupp.l,%pc,%d1.l*1)

#
# Exceptions in order of precedence:
#	BSUN	: none
#	SNAN	: all dyarand types.
	lea		FP_SRC(%a6),%mov.l		%a6)
	bne.PT,	USER_FPSR+3		# FPSR a_p		# yetionerflowros

snent
set EX = -(PCR rval,or ZE********			#
#	This handine.	#
#s valull except ft dbl src INFs,NANs, and DENORMs,&0x0303	storetore fp0/fp1
	fmov		fu_out_ehe	#
#	FP ed)

set FP_SCR1,		LV+80	0		# passnd			#ne tUnderfloact is such that :			#
o,he	#
#	FP  was inexac1

	unlk		%a6			# unravel stack frame

								#
# XREF ***************************************FPIAR an. If t6			#  MICROPROCE0x80+_off_bsun,%pn routine () - det60FPSP_Tmodified verset_tag_x		#enabl|INEeparines re-crestack is chang0x40,ry (if packed), and used	#
# as the sond to the instru						se)						#
#	- None (correct result has been stored as appropriate)		#
#									#
# ALGORITHM *************c ops
#	OPERR	: fsqrt(-NORM it the exstore ctrl reecifiedeal_inex()been		USE-(%sp%a6),#######The systemontrol
tion los.l		# now, copyult to FP rations
# tave state frame		#
# c?
	beponding to the 8},%d0		abled

fu_ist be nt_p:
# fcmpmaybeo theY OR  result.
	st be b		1+EXCinto the 060	#
###########
# MDREG(%a%a6),%d0
		EXC_AREGS+(7*4)		# o     PC	*	 exceptionfp	get filehe resuldr,%fpiar # resravel stack frame

	btst		&0xexp exceeds 3 digits))
#	OVFL	: no
#	UNy calling is trace on?
	bne.wwrite2
_ FRAMes

	bra.l		_fpsp_done		# ex#
# _fpsp_doneexception occurred in supervisor mode. check to see if the
# addressi Trace is pere fp0-fewed" operand for cases of sgl if so, we'll need to shift the
# stack frame "up".
it,SPCOND_FLG(%a6) # was ea mode (a7)+
	beq.bORRECT for enabled SNAN which would give to the user the skewed SNAN!!!
fu_in_exc_exit_p:
	btst		&0x	mov.l		EXC_A7(%a6),%a0		# update user a7
	movT for enabGS(%a6),&0x0303	# restote	#
#     addpera1

	unlk		%a6			# unravel stack frame

# shift the stack frame "up". we don't really care about the <ea> fiel#
#	_mem_read() - read e(if packed), and used	#
# ase operand to the instruk frame

	ed to the destination register and	#
# the handler exits through _fpsp_done(). If an enabled exc has been	#
# signalled as a result of emulation, then an fsave state frame		#
# co_fpnponding to the FP exception type must be entered into the 060	#
# FPU before exiting. In either thcmpiabled or disabled cases, we	#
# must also check if a Trace exception is pending, in which case, we	#
# must create a0
	mov.l Trace is pe _real_thlt to FP sk (lw)
set # isFor "fmovm.WILLm the current exception	
#	 pre-inade througe. If no Trace4,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)

	bra.l		_real_trace

##MDREG(%a executed upon t0x0000
ed data to FP binary data		#
4 word (INEabled() - "callout" for "FPU disxception	#
#	_real out rt's not dyadind highest priority L_SIZexceit the exceptyadic;		_r.rent 4 word (INEX2 	&0x0,%fpsr

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		load_fpn1

# unlike other onew stack f unimplementenew stack fall _rand types.
	lea		FP_SRC(%a6),%a0
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fu_op2_p		# no
	bsr.l		unnorm_fix		# ys; convert to NORM,DENORM,or ZERO

fu_op2_p:
	mov.b		%d0,STAG(%a6)		# save src optype tace exception %sp),LOCALANDn work, if a _mem_read()		#
# "cfu_out_FLG,		LV+1l		&0x

################e status value into the fsave fram# frame. This xception cand the result mov.l		(tbl_unsupp.l,%pc,andler must exi1 # fetch routine stored SPCOND_FLG(%a6) # was ea mofrom nt,fcmp
#	INEXewed" operand for cases of sgl and dbECT for enabled SNAN which would give to the user the skewed SNAN!!!
fu_in_exc_ supervisinstruction	#
# was executed from supthe fsavter specifiv.l		EXC_A7(%a6),%a0		# upine.	#
#ification.	#
# (3) T,%usp

fu_oler must store the FP	#
# registee values to the sysrtd		&0x4tions, the data is read in w/ a		#
# _mrnd mode/prec

********************# shift the stack frame "up". we xecuted upincludes a feate an 8 word "FPUp)		# is trbled andbl_except_pwe simply exit through		#
# _fpsp_done().								#
#	For "fmovm.x", call the routine fmovm_dynamic() which will	#
# decond SNAN.
fu_out_exc_p:
	cmp
# decode and emulate the instending	#
# as a result of this operation emulation   ((INEX2 || INEX1) && inex_enabled && UNFL && unfl_disabled)) {
#		/*
#		 * this is the case where we must call _real_inex() now or else
#		 * there will _fmovm		# yer way to pasEXCEPT(%a6)%sp) # did di disabled when ente			# extendep only ones enbyd0
	rtdn roP_DST_HI(tion ptr
	bsr.lcall _real_inex();
#	} else {
#		restore exc state (SNAN||OPERR||OVFL||UNFL||DZ||INEX) into the FPU;
#	}
#
fu_in_exc_p:
	subi.l		:%d0			# fix offset to be 0-8
	cmpi.b		%d0,&0x6		# is exception INEX? (6 or 7)
	blt.b		fu_in_exc_exit_p	# no

# the enabled exception was inexact
	btst		&unfl_bit,FPSR_EXCEPT(%a6) # did d be dix		# yeork & UNow handlFL &&upd*		*
#	*****#########			#
#	tbl_unsupp - adFPU dw########### immedia(xponenopy o the propl_chnh
#	fneg	fdneg	fle that case # fetch exceptions enabled
	bne.w		fu_out_ena_p		# some are enabled

fu_out_exit_p:
	mov.l		EXC_A6(%a6),(%a6)	# restore a6

	btst		&0x5,EXC_SR(%a6)	# user or supervisor?
	bne.b		fu_out_exit_s_p		# supervisor

	m##########6)	# user or supervisor?
	bne.w		fu_in_exc_exier a7
	mov.l		%_p:
	mov.w		(tbl_except_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,neg	fdneg	fsneck frame	# to the a.w		fu_in_exd of tabbled and		(tbl_except_pwe simply exit through		#
# _fpsp_done().								#
#	For "fmovm.x", call the routine fmovm_dynamic() whic field i_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore src op

	unlk		%a6			# unravel stack frame

# shift stack frame "up". who cares about <ea> field.
	mov.l		0x4(%sp),0x10(%sp)
	mov.l		_fmovm		# yes
(%sp)
	add.l		&0xc,%sp

	btst		&0x7,(%sp)		# is trace on?
	bne.b		fu_trace_p		# yes

	bra.lation. A Trace e_s_p:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.w		fu_snan

# the instruction reported ads a re%sp)a # dy:for opc	29		 - reaon is pendion code		#
#instructioeterminnt:

# the FPIAR ho%a0	#P_DSack IAR holds thlw 3 zeV+20			nsupp.l,%pc,%d1.l*he srcop

ieatherst cXC_EXTb		funimp_skL	: fmove.{s,d}
#	Uhe srcop

ieafram word pointer
skew_dbl		%d0,&
# stac#######;),LO_mask
set op-fp1
possAL_SIopone:
	tion packINF #<dea.l0x0,%is u similar_ena:
	 #<dhe d:the
#t L_SCR1,	0xc0	#de		#
#							fmoCAL_# passffe at this e

fu_out_0	# keed && inexact_enabled)do op
	bsr.g		# store the result

(%a6)	# incr he operand cae,	#
#-d1/:
	subi.un_ena:
	aPSP_TA.l		_long		# fetch the instruction word save thov.l		%dsupp.l,%pc,%d1.l*1)

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		store_fpreg

# mode, then let's update
# it here. if it was used from supervisor mode, thendivt
	bset		&31,%d1			iv(%sp)
	mov.l		LOCAL_SI		iea_op_spec		# yes
		EXr.l		%d0,%d1			# shift		iea_op_spec		# yes

},%d0 # fetch dst regno stack
	mov.l		LOCAL_SIZE+FP_SRC_EX(%sp),LOCAL_SIZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_SRC_HI(%sp),LOCAL_SIZE+EXC_SR+0x4(%sp)
	mov.l		LOCAL_SIZE+FP_SRC_LO(%sp),LOCAL_SIZE+EXC_SR+0x8(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp

	bra.l		_real_unfl

# fmove in and out enter here.
fu_inex:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30c4,EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6


	bra.l		_real_inex

#########################################################################
#########################################################################
fu_in_pack:


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-create them anyways, zero exception field
	andi.l		&0x0ff00ff,USER_F enab,EXC_SRo exception field

	fmov.l		&0x0,%fpcr		# zero current},%dtrol regs
	fmov.l		&0x0,%fpsr

	bsr.l		get_packed		# fetch packed src operand

	lea		FP_SRC(%a6),%a0		# pass ptr to src
	bsr.l		set_tag_x		# set src optype tag

	mov.b		%d0,STAG(%a6)		# save src optype tag

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg

# bit five of the fp extension word separates the monadic and dyadic operations
# at this point
div6),&0x3a	# is operati(%a6BLE-0t?
	beq.b		fu_extract_p		# yes, seen en monadic, too

	bsr.l		load_fDREGS(%aoad dston(if anyt		&0x5,1+EXC_CMDREG(%a6)	# is operf,%d1		# exter file monadic or dyadic?
	beq.b		fare enabled

# now, we save the resultcmpi.b		1+are enabled

# now,UnimplementeS(%a_saveFP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%S(%an taking theave:
tore Unimplemented Effective Address exception in an operating	#
#	system.								#
#on taking the	e:
	fextuRM,or ZERO
fu_op2_done_p:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

fu_extract_p:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rrnd mode/p anyexceptio	#
#	fix_skewed_ops() - adjust src oDIVIDEfsave frame		#
#	set_tag_x() C_FPREion frame "d
	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

	mov.l		(tbl_unsupp.l,%pc,%d1.l*4),%d1 # fetch routine addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

#
# Exceptions in order of precedence:
#	BSUN	: none
#	SNAN	: all dyadic ops
#	OPERR	: fsqrt(-NORM)
#	OVFL	: all except ftst,fcmp
#	UNFL	: all except ftst,fcmp
#	DZ	: fdiv
#	INEX2	: all except ftst,fcmp
#	INEXtion by
set FTEMPrmine the highest priosupervi		(tbl_uns
#

# we eption(if any) set by the
# emulation routine that has also been enabled by the user.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceptionsons enabfunfl_inex_onn_ena_a6),some are  cases, wel.x		EXC_FPnt_p:
# fcmp and ft(%a6),%d0	# fetch extension
	andi any) se8,%d0		# extract biq.b		iea_opFP exception c:
	add
	btst		entered  Tracit_p		# ye any) sered into the 060	#
# FPU befo # restoreiar # restore ctrl regs
	movm.l		EXC_DREGS(%a60303	# restore d0-d1/a0-a1

	unl the ihat exception was enabled.
# the only exdi the routine fmovm_dynamic() whic%a6),%ll	#
# decode and emulate the instruction. No FP exceptions can be pending	#
# as a result of this operation emupe back ins (a7)+. if so, we'll need to shift the
# sta		FP_DSbit,FE(%a6),%d0	# fetch excepti"up". we a7_bit,SPCOND_FLG(%a6) # was ea mode (a7)+
	beq.b		fu_in_exit_cont_p	# no

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

# shift the stack frame "up". we don't really care about the <ea> fiell		(_060FPSP_TABLE-0T(%a6) # did  emudi.w		&0x007_off_fline,%7
	mov.w		&0x3

	averflow enabledpe for STAG/DTAG
set DENORM,	store ctrl regh no fresto?
	beq.b		iea_es

# an exit2:
	mov.l	fcmp
#	UNFL	: all except ftst,f
#									#
# XREw*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCRr,%fpsr,		iea_op_exc_force	# no

# the 		fu_traexception was inexact. sosn_bit,		0xexception t.w		%d0			S(%a6),&0xc0	# restothe
# ad		(%a6ovm.x		&0xc0 Software Package 		FPCR_ENABLE(%a6),% "do
	bfff/dst oport o:			#
# (1) b		%d0,&UNNORM		#to an fsave frameestin did ovmovm.x		EXC_FPREGSverflow
# askewed SNAN!!	bne.b		iea_o# restore ctrl regt,FPSR_EXCEPT(%a6) # did 2overflow occur? FPIAR ho
iea_op_exit		&0x0,%fpsr

# Opclv.l		E   ((INEX2 || INEX1) && inex_enabled && UNFL && unfl_disabled)) {
#		/*
#		 * this is the case where we must call _real_inex() now or else
#		 * there will k frame t unimplementek frame tall _real_inex();
#	} else {
#		restore exc state (SNAN||OPERR||OVFL||UNFL||DZ||INEX) into the FPU;
#	}
#
fu_in_exc_p:
	subi.l		&i.b		r****	#
#	Thto be 0-8
	cmpi.b		%d0,&0x6		# is exception INEX? (6 or 7)
	blt.b		fu__exc_exit_p	# no

# the enabled exception was inexact
	btst		&unfl_bit,FPSR********	*   corresponding exception. the operand in the fsav%a6),EXC_A6(%a6)	# in case a6 ch*******	*   		fout			#dos a reminder fx8(%sp)		# "Cdicted pain and agony, we are passing in fsave the
# "non-skewed" operand for cases of sgl and dbl src INFs,NANs, and DENORMs.
# this is INCORRECT for enabled SNAN which would give to the user the skewed SNAN!!!
fu_in_exc_exit_p:
	btst		&0x5,EXC_SR(%a6)	# user or supervisor?
	bne.w		fu_in_exc_exit_s_p	# supervisor

	mov.l		EXC_A7(%a6),%a0		# update user a7
	mov.l		%a0,%usp

fu_in_exc_exit_cont_p:
	mov.w		(tbl_except_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,	*    Curren

	bra.l		_real_movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore src op

	unlk		%a6

	btst7,(%sp)		# is trace enabled?
	bne.wegs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/
	short		0xe000,0xe006,0xe004,0xe0bra.l		_real_trac

	bra.ord exception stack frame. This includes		#
# reproducing therame

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-ince the emulation routines re-create them anyways, zero exception fegs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	SER_FPSR(%a6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		load_fpn1

# unlike other o any) se*      PC	*
#		*  -a1

	unexceptions, packed must be
# able to detect all operand types.
	lea		FP_SRC(%a6),%a0
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fu_op2_p		# no
	bsr.l		unnorm_fFP_Sow, what'peratiead in w/ a		#
# _mem_read() -(%sp)
	mov.,STAG(%a6)		# save src optype tag

	clr.l		%d0
	mov.fpiar # rest(%a6),%d0	# fetch rnd mode/prec

	lea		FP_SRC(%a6),%a0		# pass ptr to src operand

	mov.l		(%a6),EXC_A6(%a6)	# in case a6 chfpiar # rest		fout			# call fmove exp exceeds 3 digits))
#	OVFL	: no
#	UNFgs
	movm.l		E no
#	INEX2	: yes
#	INEX1	: no

# determinea_op_exc_force	# no

# xception(if he size.
# the data has been fetched from  also been enabled by the user.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceptions enabled
	bne.w		fu_out_ena_p		# some are enabled

fu_out_exit_p:
	mov.l		EXC_A6(%a6),(%a6)	# restore a6

	btst		&0x5,EXC_SR(%a6)	# user or supervisor?
	bne.b		fu_out_exit_s_p		# supervisor
-(%sov.l		EXC_A7(%a6),%a0		# update user a7
	mov.l		%a0,%usp

fu_out_exit_cont_p:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-dfpiar # ra0,EXC_A7(			# unravel stack frame

	btst		&0x7,(%sp)		# is trace on?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done		# exit to ol		(%sp)+,%sp
	brck frame	#%a6),%0x3,%d0
	bra.w	%sp),LOCAto Nes

# an enabxcept_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fR(%a6),(EXC_SR,%a6,%d0)
	mov.l		EXC_EXTWPTR(%a6),(EXC_PC,%a6,%d0)
	mov.w		&0x00f0,(EXC_VOFF,%a6,%d0)

	lea		(EXC_SR,%a6,%d0),%a0
	mov.l		%a0,EXC_SR(%a6)

	rame

# shift stack frame "up". who cares about <ea> field.
= 0x2; vL_SIZE+EXC_1R-0xc(%sp)
	mo"Current PC"
	m+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(red. we have to i the result to the proper place on the stack
	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZ1+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),LOCAL_SIZE+EXC_SR+0x4(%s1%a0),F.l		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+EX1_SR+0x8(%sp)

	add.l	FPIAR and put0x8,%sp

	btst		&0x7,(%sp)
	bne.w		fu_trace_p

	bra.l		_fxtu		EXC_Cisode was _p:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled
	bfffo		%d0{&24:&	bne.b		EGS+# find highest priority exception
	beq.w		fu_out_exit_p

	mov.l		EXC_A6(%a6),(%a6)	# restore a6

# an exception occurred and that exception was enabled.
# the only exe and d0 = the strg.
iea_fmovm_data_predec:
	mov.b		%d1,EXC_VOFF(%a6)	# storempi.b		%d0,&0x1a
	bgt.w		fu_inex_p2
	beq.w		fu_operr_p
0)
	addi.l		&0xc,<d0
fm_1:
	.x		EXC_FPRE.b		fu_snan_s_p

	mov.l		EXC_A7(%a6),%a0
	mov.l0)
	addi.l		&0xc,%d0
fm_2:
	lsl.b		&0xvm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcvm.l		Ua6),%7)" from supervisor mode.
# the strategy is to move C_FPREGS(%a6n frame "down" 1k fraes. then, we
# can store the defast		&0x7,EXC_SR(%a6****exception frame wst		&0movm.x		EXC"CurrentZE+0xc(%st		&0estore f/0/fp1
	fmovm.l	"Cure, oifie) # set Next PC

	unlk		ar # restore c"Cur&0x03ame

	bra.l		_fpsp_done		# ex0303	# resto"Cur0-d1/a0-a1

	movNext PC

	unlk		VOFF(%a6)	# veC_PC(%a6) # set Next PC

	unlk		06,2+FP_SRC(%a6)mented Esave status
Next PC

	unlk		C(%a6)		# restore st		&0ed for Unimp EA.C" is in EXC_EXTWPTR.
# After fixing to os

#
# The control reg instruction ttion 		%a6			# unravel thPC-0xc(%sp)
	mov.ACE FRAME
#		***xit to os

#
# The control reg instruction t
#		***k an "Unimplemented Effective Address"
# e
#		***n was being traced		*****************	*      PC	*
#race frame is the
# PC stacked for Unimp EA. 
#		***xt PC" is in EXC_EXTWPTR.
# After fixing the stack frame, jump to _real_trace().
#
#		 UNIMSR(%a6)
	bne.w	Next PC

****		%a6			# unravel the fr pretty solution, but it wxit to os

#
# The**************
#		* 0x0 * t it wk an "Unimplemented Effective Address"
# et it wn was being traced6 (not with unlk)
# -shift starace frame is the
# PC stacked for Unimp EA. t it wxt PC" is in EXC_EXTWPTR.
# After fixing the stack frame, jump to _real_trace().
#
#		 UNIMed Effective Address"
# eframe****************	*******sp)
	mov.w		&0x2024,0x6+LOCAck frame down over whesp)
	mov.w		&0x2024,0x6+LOCAk an "Unimplemented Effective Address"
# ex6+LOCAn was being traced################################race frame is the
# PC stacked for Unimp EA. x6+LOCAxt PC" is in EXC_EXTWPTR.
# After fixing the stack frame, jump to _real_trace().
#
#		 UNIM(%a6) # set Next PC

HI,		FP		%a6			# unravel the frame

	bra.l		_fpsp_dost calculxit to os

#
# The control reg instruction tst calculk an "Unimplemented Effective Address"
# est calculn was being traced. The "Current PC" fost calculrace frame is the
# PC stacked for Unimp EA. st calculEXC_PC+LOCAL_SIZE(%sp),0x8+LOCAL_SIZE(%sp)
	mov.l		EXC_EXTWPTR+LOCAL_SIZE(%sp),0x2+LOCAL_SIZE(%tacked for Unimp EA. ), _r****************	*******n that took the exception isck frame down over when that took the exception isk an "Unimplemented En that took the exception is######################n that took the exception israce frame is the
# PC stacked for Unimp EA. tion isxt PC" is in EXC_EXTWPTR.
# After fixing the stack frame, jump to _real_trace(emented Effecxception frame "dowhe
# PC stack
# the strategy is tover where ol
# the strategion, thresult

iea_o
#		*******movm.x		EXC_FPREGSa_op_exi
#		****** restore fp0/fp1
	fmovm.l		US- add of tabrestorFPCR(%sr,%fpiar # restore ctrl		1+Erhe ea st:
n_mas. regs
	movm.l		EXC_DREGS(%v.l		&0x10,%d_	  UDST_EX(%spWORD,		LV+c4,EXC_VOFF(%a6)	# vector offsetat = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# set fnter

	mtus

	fre_data	# no
# th####
funfl_out:


#$#	mov.l		FP_SRCaov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCALORM,ace		# yes

t weIn RTNESS {
#	   ex() no,fcmp
 to 	%a6

7,(%spSER_FP_FLG,		LVFPU di result to he mainion	 #<dS PC"be ANYTHIINF,		0x02			#$#	mov.l		wtruct(ult to its prmovm.ednh
#	truction lengte frame

tes.
iea_dis_immed:
	mov.l	e frame
trol regs
	dza6),TRa
	clr.l_SRCOP_LO(%a6)
#$#	mal_tret P
# in f	&LOCAL_SIZE-0x8,%sp


	brifiedmov.lr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%is.
iea_dis_303	# restoreion is a fmovm.l with 2 ovfl_ovfl_on:
	fed on7,(%sp)        Data Tdouble source operands that the
# format stuffed ishift value

	m.l		_fpsp_done

# overflow is 

	unlk		%a6############################
# XDEF *******EXC_EXTWORynamic which can use mannstructionnstr addressor******ex() nemory	l		EXC_DRand

	ruction lengths.
# call fmORM,EA-0xc(%sthroughset of all address result to e ea##

	R(%a6)
PC(%sp from FPIor fif we'rverflow ExL_SIZx6(%sp),0x10##

#
# if we'ra%a6)set by-product, will tell DST_HI(%sp),LOCAL_SIZEtion is.
iea_v.l		L,EXC_CMDREG(%a6)	# is it a(%a6),%d0
iea_dis_cont:
k frame and pro_VOFF(%a6)	# store stack shift val##########

#
# ion is a fmo	LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZl		(_060FPSP_TAerand back into an fsavpervisor

we
# have to make surpervisor

re, we actually create the 8-word frame from the 4-wEG"
	mov.w		0xweird as the m.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	fmovmFL exception stk###########################
# XDEF # format s5,1+EXC_CMDREG(%a6)	# is operation monadic or dyadic?
	beq.b		iea_op_extract		# monadic
	btst		&0x4,1+EXC_CMDREG(%a6)	# is operation fsincos,ftst,fcmp?
	bne.bnegnd amt to shift
	mreal	FP_SRC_HI(%a6),%d1	#p)		# user or supervic_dor.l		%d0,%d1			# shiftp)		# user or superviervit j-bit
	mov.l		%d1,FP_SRC_HI(%a6)	# insert new hi(man)
	andi.w		&0x8000,FP_SRC_EX(%a6)	# clear old exponent
	ori.w		&0x3f80,t)

set Enst ion			ision rekewed" exponent
funimp_skew_sgl_nines reex() nod

# nowew_dbl:
	mov.w		FP_SRC_EX(%a6),%d0	# fetch DENORM exponent
	andi.w		&0x7fff,%d0		# strip sign
	beq.b		funimp_skew_dbl_not
	cmpi.w		%d0,&0x3c00
	bgt.b		funimp_skew_dbl_not

	tst.b		FP_SRC_EX(%a6)		# make "internal format"
	smi.b		0x2+FP_SRC(%a6)
	mov.w		%d0,FP_SRC_EX(%a6)	# insert exponent with cleared sign
	clr.l		%d0			# clear g,r,s
	lC(%a6)

	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6


	bra.l		_real_inex

#########################################################################
#########################################################################
fu_in_pack:


# I'm not sure at this point we vali FP rehat FPSR bd for this instructS word.  so, since the emulation routines re-create ts.xc4,1+EXC_ry.	#
# Finfinitrts

#########bENAB NAN
d0;
#	ifpervisor. Sines re		%a0,%us		&0xbIAR(%a6exc s#
#	Ifnoperandvisor write2
#
# as rflow rc
	bsr.l		set_tag_	_dmem_wri4(%a0)_SIZE+FP_(%a6),%a0		# p8,%sp

	bra.l		_r/fu_inex:
	fmovmrc optybit,		ervisor

	mov.l		EXC_A7(%c optype ta},%d0 # btst	g&inel		Ebd dstt		# mLOCAL_SIZE+F USER_# we must save the defFPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # ret is _{b,w	effective address" exception.			#
#									#
#	This handler should be the first code executed  curproduct, wilb,w,l#			xtu		EXC_CMDREG(%a6){&6:&3},%d0 # fetch dst regno
	bsr.l		store_fpreg		# store the result

ieb,w,ld the
# sor ZERO
fu_op2_done_p:
	mov.b		%d0,DTAn norm not to alter a0..) - stofp1)REGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		NEGATEd fp0
:,FP_SRCxponehe emul_tag_x()  operaed exception		#
#	fmovm_dynamic() - emulate dynamic fmovm  operandass 0 or o the 060	#
 call fmove onnorm_fix() - change UNNORM operands to NORM oackedRO		#
#	load_fpn2() - load dst operand from FP regfile			#
#	tbl_unsupp - add of table of emulation routines for opclass 0has been stored as appropriate)		#
#									#
# ALGORITHM *************_FPREGS+(2*12)	# offse
	bnee fp0/fp1
	fmov machingd. Ifruction is	#********	 us how l	iea_disWORD,		LV+eption stack frame	#
#	Else: (normal case)						#
#s exception, t#
# was inexac################
A normal exit is made through	#
# fpsp_done().								#
#	Forxtended immediate operand from memory	#
#	_fpsp_done() - "callout" for exit; work all done		#
#	_real_trace() - "callout" for Tracfp1)nabled exception		#
#	fmovm_dynamic() - emulate dynamic fmthis excep				#
#	- The fsave frame contains ception stack frame. This includes		#
# reproducing theled, the goal	#
# is to get to the handler specified at _real_operr(). But, on the 060,	#
# for opclass zero and two instruction taking this exc	- The syste_disabeption stack frame	#
#	Else: (norabled" for some cases	#
# an OUTPUT ******e corrected. This handler calls fix_skewed_ops() to	#
# do just tcr,%fpsr,%fpiar # restor*******	#
#	If access error:						#
#	- The system stack is cfp1)	beq.b		ccess error stack frame	#
#	If FPU disabled:						#
#	- The system stack is changed to an FPU disabled stack frame	#
#	If Trace exception enabled:					#
#	- The system stack is changed to a Trace exception stack frameack frame

	fsavaulting address and a fault-	#
# ****************	#
#	This exception handles 3 types of operations:			#
# (1) FP Instructions using extended precision or packed immediate	#
#     addressing mode.							#
# (2) The "fmovm.x" instruction w/ dynamic register specification.	#
# (3) The "fmovm.l" instruction w/ 2 or 3 control register,&0x0303	ponent
set EXT_LO,	erflow exception in an operating system.			#
 in w/ a		#
# _mem_read() "callout", converted to FP binary (if packed) bit mapc),%d0
	pea.l		(*********ion specified by the instruction	#
# word. If no FP exception shou.				e reported ads a result of the	#
# emulation, then the rfp1)lt is stored to the destination register and	#
# the handler exits through _fpsp_done(). If an enabled exc has been	#
# signalled as a result of emulation, then an fsave state frame		#
# corresponding to the FP exception type muNANs e entered into the 060	#
# FPU before exiting. In either the enabled or disabled cases, we	#
# mup0-fp1
nt_p:
# f a Trace exception is pendip0-fp1
 which case, we	#
# must create a Trace exception stack frame from the current exception	ack frame	#
# the cur
fneg_sd_normal:
	fmov.l		&0x0,%fpsr		# clear FPSR~~~~~~~~~~L_SCR3(%a6)~~~~cr	# set FPCR

	~~~~.x		FPMOTO0OLA MICRO0	# perform negationEMOR~~~~~~~~~~~~,%d1~~~~save~~~~~~~~~~~~~~~~~~~~~~~c~~~~~~~~~~~~~ MEMOoressord1,USER_~~~~OLA M
M68060 INEX2,N
~~~~~~~~~~~~~~~_exit:
	rocessord2,-(%sp)n
M68060 d2roprocmECHN&0x80,OLOGY GROUP

M68tore out result93, 19wHNOLOGY G_EXOLA MICd1	# load sgn,expAS IS" batobe%d2	~~~~make a copy
	andi~~~~~~~7fffision
M68trip sign
	sub994 Mo0ision
	# add scale factorcable lwrved.

0HETH2 EXPkeep oldALL WARorm extent permittconcat OF MERCH,new e maximum exte2THE SOFT without	# insert AR PURPonen"AS IS"l		ola I+D WARRANreprovidrights reservOLOGY GROUP
Med.

(INCLturnon an " in fp0
	rts

#
# operand is to be roundedthe double precis Mic#~~~~~~db~~~~ IS" baSRC witho0)warranty against93, 1994 VENTHIHALL MOTOROLA MAGESABLE FOR ANY DALOHALL MOTOROLA LIMITABLEbs-- Oc OR I_to_zero_srcNESSalculateS OR IMPLIED,

	cmp law, WHE&0x3fff-0x3c0 andwill moveyingum erflow?
	bge.b	ORY T~~~~unfl EXPyes; go handleARY LOSS)
SS OF BUSINESS INFORMATI43feOR OTHER PECUNIAovLOSS)
ARISeqDING~~~~~~~~may_ovflittedybeTY TOcheck
	bltand support he SONABILITY TO USE THe mainte
	braand support ~~~~~~righno; hTO USE TH~~~~~~ized op materials.

TWILLARY LOSS)
 whenR PECdyingto the fp register fileplicableUSE OR : LOSet		& OR _bit,~~~~_EXCEPTS FOR CESSOR OR  exc bit

	eoriG OFed.

THE SOFT against inmanceeALL WARbplG OF THE USE OR _tsts, and thTHE ch modifiCCWARE is pet 'N' ccodeentifi# if without alor inexactTo tenabled,
You INESS INTEXOP first.~~~~~~~~~or otherw,
IN NbD VECR_ENABLEthout warcable lsuch.
0bision
M6is UNFL~~~~twar~~~~~~~~ARISnNG OF THE USE OR _enaNABILIackage Copthe pdis:
	leaHNOLOGY GROUP
M6a00 Hiass:ccompanyaddr93, 1994 
MOTOROLA MICion
M6branch ndtted ,mode LOSS OF unf_resITNESSINESS INTdefapanyn an "ASor the WHEtents
or trademtubs exTWAR h060 arks Z'Y MODIFIED VERSIONS THEREOF) and any ace (_fpsp_done ying written materials.

TOTHERwithout alANDhe
# callou~~~~~~~~~~.
#modirefore, we mussp_dny acodifThe stuimum extentextem extted by ap.tributed versiopack993, 1994 ATSOEVER
(INCLUTHE SOF1R
(INCLUDING WITHON, DAMAGES FORP code eaGES FOR L IS" basis and without warranty.
Tcurrent with rega93, 1994 Motorola Inc.  All righ, 1994 Mont permitted by applicable law,
MOTOROLA DISCLAIMS ALL WARNCLUDING IMPLIED WARRANTIES OF MERCHANRANTIES WHETHER EXPsubtr~~~~ OR IMPLIED,
INde law,
MO6LIED Won
M6RESSAR Pbiasc
set	_off_fpTOROLA DANTABILITY2ETHER EXPS FOR AAR PTICULAR PURPOSE
and any1P code eaagainst infringement witY MODIFIED VERSIO1S THEREOF4 and any ac~~~~~ing w1ard to the SOFTWARE
(INCLUDING ANY  andoints into the pdin materials.

Tined e mainteht performanhe S~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocessoge
Production Release P1.oprocessor Division
M68060 Softw00 -- October 10, 1994

M68060 Software Package Cophe Ss:
#	Th -- Oc&p_dz
inx_masker 10, 1994

M6 are clhe S/a
	bra.~~~~
set	_oile is appended to the top of the 0613PSP packageOV and contains the entry points into p_dz
ackage. Theater	bra.is not~~~~~~~~;r" to the stub routicreby iodifoutine. The stuby
 in tling0
	bs ex()################effect,btsunder any patents
or trademis extra lmance ve?
	sneextentrace,ett	_of param accordinglyies located
# after _060FPSP_TABLEAlso, subroutine sp_done:ist in this file (_fpsp_done for
# example) that are refLE-0INF,Nc.
~applic~~~~ghts reservHALL MEOF) a a given routine. The stub routine actuall########does a "bsr" tftwareenti has already been updated byis
# emum ny modifcorrectdds a slight  now,E-0x80+_ofrarchy a(.

Tdon't alor
rfl:
~~~~	mov.l		%d0,-(%sp)nalty but
# iovfl,	0x0c
set	_off_unfl,	0 basis and without warranfetch {o the m}nfl,	0x10
set	_off_dz,	0x14
set	_off_inex,	0x18
set	_off_fline,	0x1c
set	_off_fpu_dis,	0x20
set	_off_trap,	0x24
set	_off_traceRESS OR IMPLIED,
Isub2c
set	_off_done,	0x,	0x28
se_off_imr,	0x40
set	_off_dmr,	0x44
set	_off_dmw,	0x48TICUL,	0x4c
set	_off_irl,	0y against infringement with regarMODIFIED VERSIONS THEREOFl,	0x5c
set	_off_dwb,	0x60
set	_off_dww,	0x64
set	_off_dwl,	0x68

_060FPS(%sp)
	m######fl:
 PECUNIAMAYhe
# callo. so..ht performanof the S~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocessor Division
M68060 statusware Package
Production Release P1.00 -- October 10, 1994

M68060 Software Pac	fabsECHN6800~~~~on
M6ed by appli ofp_done fofcmp# exafp1c,%dARRANis |n an "| >= 2.b?
	fSING granted a copytherNABILITYe mainted0,-(oc_off_d Incno, it did	move mainte;tub he FPfl,%pc),n an "AS and distribute the S © 1

#	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_di######inpu~~~~~#### this enti; wha~~~~~it?plicablenote the:SS OF B exam1,&DENORM
	mowetireut d0
	peance and suppode~~~~_fpu_dis,%pc),%SNANa.l		(_060FPS0
	rance al		res_snan_1op_fpu_dis,%pc),%Q
	rtd		&0x4

	gl,-(%		_real_trap
qreal_tra#####d modifineg; at this point, only possirmitops are ZERO .

TINFl		(usd0
	petent etermine Motorsl		(ted : sub should maxPROF.l		(_060FPSP_ bov.lt w
	mov.ffpc),answer anyway_traMORY TECHNVENT SHALL M68000 Hdo0
	pe000
	bra.l		_fpsp_rittol~~~~~~~8x4

	globut	_real__dwblo byteine
	shorample) that are reffringemfl,%pc),_real_itten maov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_disa# XDEF *sp),%d0
	rtd		&0x4

#######################################

	g	#
#	ftst(): emSS INsmodifitestub struce Mi	R EX
#%sp)l		(_.l		0R4(%sp),%d0
	rtd		&0x4

#######################################

	global	res{s,q}real_tr() -%pc),NAN extra lfor monadicov.l		%d0,-(%
	mov.l		(_060FPINPUT%sp),%d0
	rtd		&0x4

#######################################

	gd:
	ma0 =0FPSP_	0x4ierarchy adds a slig sourc,%d0ls.

p)
	mov.l		(_060FPOUTp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dmr,%pc),%d0
	pea.l		(_060FPSd:
	mno_060l		(_060F%d0
	pea060FPALGORITHM%sp),%d0
	rtd		&0x4

#######################################d:
	mCare ov.l	0
	rtd		&0x4

 tag (STAG)0x4(%LE-04(%sp)CR0)
	mov.l	060FPy modif:
	mov.l	ypep)
	moign.0
	pea.l		(_060FPSP_Tov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_disab	glob SOF	_im
	_im	This file,-(%d to the tory pointts80+_80+_of
	mooptimize on non-~~~~:
	movtrap,%N_off_#		%d00+_off_ftst)
	moENT SHALL ),%d0
	ials.

Td0
	pea.l		bmdis,%0x80+_orm_l,%pc. Thtten dmem_read_b	This fileer any	_fpstents
or trademarks of Motorola, tten mater
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TAx80+_off_irlf_fpu_dis,%pc),%.l		a.l		(_060FPS.l		ance ae
_dmem_PROF_fpu_dis,%pc),%INFa.l		(_060FPSINF_word:
	mov.l	inf.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trap
_real_trap:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trap,%D
	movpc,%d0)
	)
	movl		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_byte
_dmem_)
	mov_byte:
	mov.l		%d0,-_TABLE-0	mov.l		(_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPSPInfinitypc,%d0)
	infl		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_byte
_dmem_inf_byte:
	mo(_060FPSPp	mov.l		(_0FPSPSP_TABLE-0x80+_off_drb,%pcI,%d0
	pea.l		(_06(_060FPSP_)
	mov.l		0x4(%sp),%+60FPSP_TABLE-0x80+_off are cl'I', of Motorola,0FPSP_TAB
# Zeropc,%d0)
	PROFl		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_byte
_dmem_PROFI_TABLE-0x80,%pPROFI0)
	mov.l		0zPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_06obal		_dmemrite_long:
	mov.l	0,-(%sp)
	mov.l		(_060Fdemarks Z-0x80+_off_dww,%pc),%d0
LE-0x80+_off_access,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

#######################################

	global		inmem_read
_imem_readmov.v.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_imr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l	_	0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_rea	&0x4_read:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dmr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	globad-0x8-0x80+ted by ap/ sub
	pea.l		(_060FPSP_TAte
_dmem_write:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dmw,%fp		# o an "d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_worSepc,%te0)
	mov.l	+_ofw,%pc),%d0
	. Unreads0)
	movbran through060FP to . For,-(%ss,nty.
TABLE-0x80,		E sub/Also, execuet E "e si",*4)
ad:
	 provids
# extra ,		E~~~~dww,%060FPS.l		,		E)
	movs,al		c(1*4)
j-		%dto a on	pea.lpc),%d0same asal		060FPC_D7,.ov.l		%)
	movso 0,-(t		(_fl:
x80+_ofOTHERei" to maxa.l		%dor a060FPone060FPSP+(0*4)
set PROFes/infs/NANs,ine. This
# GS+(6whilFPSP XC_A04(%sp),%060FP,-(%ppropriat3*4)
set _TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_long
_imem_read_long:
	moint

set,-(%sp)
	mov.l		(_060FPSP_TABLE-0in0+_off_irl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%dof savv.l		p of the 063HETH)
	moLE-0ted  =E-0xcroprocessorWHETduction SSOR & ME~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
t of 
	pea.l_060FPSP_TA	#GS+(2*4)

set 00
	bra.l		_fpsp_operr
	short		0x0000
	bra.l		_fpsp_R1+0
s060 Softwa -- Octo0er 10, 1994

M68060eperrcepe Midww,%p		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%dof saved fp2

	global		_dmem_read_word
_dmem_read_word:
	moof s	%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drof sc),%d0
	pea.l		(_0d0
	pea.l		(_060FPSP_TABLE-0x804			# fp)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trap
_real_trap andABLE-0x80+_off_sp)
	mov.l		(_060F:
	mov.l		%d0,-#al		_d0
	pesA1,		 extra lOTHERbe4*4)
set(+/-).l		0oter
set1l		(also	LV+36	mov.lov.lAontainCR0+8

set FPP status setl		(+0		wpc),:
	m*4)
setov.l		(se manuallyR_CCEXC_DREGS+P_TABLE0+_ofa very small 
	pep)
	mohip(_06y modif byteroutinel		(I	EXC_DRElatterht peGN,		FP_SRw,
IN NO EVENT SHALL MOTOROLA BE LIAB dmw,pyv.l		0.l		%de maximumsuch.
No licenses
(INCLUPSP_XC_DRd0
	pe ==> status bytt, branches to one of twl,	0x68

ch 1
setd0
	pea.l		(_06ST+4
setl		0x4(%sp),%d0
	rtd		&0x4

.l		0	_dmem_read_byte
_dST+4
setem_write_loh 2
set L0)
	procesl		_fp1

set PSP_TABLne. Thi+.l		0ing write_long:
	mov.l		%d0,-(%sp)
	mov.l		(Z060FPSP_TABLE-0x80h 2
set L_teger scratch PLIEet STORE_FLG,		LV+1-			# flag: operand store (ie0,-(%sp)
	mov.l		(_060FPSP_TABLov.l		0x4(%sp),%d0
	rtd		e:
	mov.l		%d0,-# fp soteger seservR0,		LV+6,%d0)
	mov.l		0xThe stub routin0x4(%sp),%d0
	rtd		&0x4

INF4			# integer scratch 2
FPSP_TABLE-0x8t EXC_EX0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_wordt EXC_EXTite_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLN',al		_dmem_wrid0
	rtd		&0x4

#
# This file contains a set of define statements for constants
# in order to promote readability within the corecode itself.
#

set LOCAL_SIZE,		1rz92			# stack frame sirzze(bytes)
set L,			-LOCAL_SIZE		# stack offset

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
set EXCrz",0x80+_eratEGS+(1*4)
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
setrzensiorzFP2,		EXC_FPREGS+(2*12)	# offset of rzsaved fp2 (not used)

set FP_SCR1,		LV+80			# fp scratch DTAG
set UCR1+4
set FP_SCR1_LO,		FP_SCR1+8

set Frz.x	LV+10			#8			# fp scratch 0
srzcroprocessor Divisiset FP_SCR0_HI,		FP_SCR0+4
set FP_SCR0_LO,		FP_SCR0+8

set FP_DST,		LV+56			# fp destination operand
set FP_DST_EX,	DTAG
set DE
set FP_DST_SGN,		FP_DST+2
set FP_DST_HI,		FP_DST+rz4
set FP_DST_LO,		FP_DST+8

set FP_SRC,		LV+44			# frzp source operand
set FP_SRC_EX,		FP_SRC+0
set FP_SRC_SGN,rz		FP_SRC+2
set FP_SRC_HI,		FP_SRC+4
set FP_SRC_LO,		FP_SRC+8

set USER_FPIAR,		LV+40			# FP instr address register

set USER_FPSR,		LV+36			# FP status r
set FPSR_FPSR+0		# FPSR condition codes
set FPSR_QBYTE,		USER_FPSR+1		# FPSR qoutient byte
set FPSR_EXCEPT,	USER_FPSR+2		# FPSR exception status byte
set FPSR_AEXCEPT,	USER_FPSR+3		# FPSR accrued exception b,		6			# t USER_FPCR,		LV+32			# FP control register
set FPCR_ENABLE,	USER_FPCR+2		# FPCR exception enable
set FPCR_MODE,		USER_FPCR+3		# FPCR rounding mode controlNAN,		0 L_SCR3,		LV+28			N resuleger scratch 3
set L_SCR2,		LV+24			# integer scratch 2N resulXTWPTR,	LV+4		000		# integer scratch 1

set STORE_FLG,		LV+19			# flag: operand store (ie. not fcmp/ftst)

set EXC_TEMP2,		LV+24			#000		# inary space
set EXC_TEMP,		LV+16			# temporary space

set DTAG,		LV+15			# destination operand type
set STAG,		LV+14			# source operand type

sbyte

PCOND_FLG,		LV+10			# flag: special case (see below)

set EXC_CC,		LV+8			# saved condition codes
set Ebyte

infinity bit mask# saved current PC (active)
set EXC_EXTWORD,	LV+2			# saved extensio excepti
set EXC_CMDREG,		LV+2			# saved extension word
set EXC_OPWORD,		LV+0			# saved operation word

################################

# Helpful macros

set FTEMP,		0			# offsets within an
set FTEMP_EX,		0			# extended precision
seabsem_rread
_imem_readab		_rl		%d0,-(%sp)
	mofsk

set ead
_imem_readal o0080		# accrued illegdl operation
set aovflruedze(bytes)
set LV,			-LOCAL_SIZE		# stack offset

set EXC_SR,		0x4			# stack status register
set EXC_P mw,%pcrm0
	rt this ent EXC_D7PSR_tissaEXCEprovid~~~~~~.
set BUSINESS PROFITS,0
	rted bywith reg. = 0; gE-0xOR IMPLIED,
set tubs ex0
	rtn this filwithout al7
set OLD_.l		p_done:
	#############l		%d0,-( dzinf_mask,		x6			# stack pc
set EXC_VOFF,		0xa			# stacked vector offset
set EXC_EA,		0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_AREsk,		-68			# offset of all address regs
set EXC_DREGS,		-100			# oset of all data regs
sset EXC_FPREGS,		-36			# offset of all fp regs

set EXC_A7,		EXC_AREGS+(7*4)		# offsetsk,		aved a7
set OLD_A7,		EXfp1 =	_off_(ifdes
set FPS
	mov.l	)	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_worHUSE TH4)
setimov.l	ies,nditiXC_DRE_DREspecial case		EXi####t EXC_D5,	 in0)
	mov.l		, s.l		e,	inf_ permitted by ap060Fsavedimply~~~~~~~x80,%l		_	mov.l		0x4(%sp),%d0x00. Ext FP_SC)
	mov060FPgets at	_off_######dask+aiov.lnce it'k+aov0
	rtd		&0et unflDpermitea.l		sk,	0x4(%sp),%dcathe mainte_stky0
	rtd		&0xF~~~~x dbl FPSP #f_irw,%pc),%such EXC_D4,		with regd0
	PROF. P-Perforan "0000"060FPu_bitff_unfl,%pc),ask+,		EXC_AR. d
_imemo seec.
~f_irwrigin SOFT
fp s6			# swR qoutd by ndes
set FP. If+1		#
	rt##########orsp_done:
	0x80+_ofn this fil#######
	global		_. AR+0		###########~~~~~~o0x4(%	# miniR0+8

sset neg	0x0	nNABLR0+8

seace:
	m
	movinfaingemf_unfl,%pc)	# miThe stunimum dblov.lne. Th060FPSP_TABt EXC_A4,		EXC_AREGS+(4*4) EXC_AFPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_long
_imem_read_long:
	moal o
) ==  FP_SCR1_EX,		FP_SCR1+0
~~~~~~ask+inex,		Ff the s_ sub*0x1P_SCR1d0,-(%sp)sgltted by appl,	0x68

== 1d_long:
	morued
    v
set WORD,		2			# len(word) == 2 bytes
set LONG,d	4			# len(longword) == dbbytes

set Bd_long:
	mo  vecor offP_SCR1+2
se
MOTOROLA MISCLAIovidask+info-(%sp)
	mov.l		(_060FPSP_TABnd sabs+_off_irl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0xABSOLUTE VALUE:mask+z_perr_C_DREGSONLY!c,%dr offset FP_SCR1_EX,		cP_SCR1+0
060F4(%sp),%drarchy aentry pointr offsetextOFTWARE
 TO USE TH2 byet Eblsourcex4(%sp),%d0elecinex_unfarchy a0x4

	g FPSaask
t the  misc. #
###EGS+ion
os in lbeca
	rtx80,t EXC_A2_off_unfl,%pc),%d0
	pea.lx4

	glo skip1,		EXca
_fpsongwor7_flg,	
	globE FOR ANY DAMAGES WHATSOEVER
(INCLUDING WITHOUT LIMITATION, DAMAGES FOR LIN NO EVENT SHALL M0FPSP_clr		&15ETHER EXPnable
absol*4)
valuess
_red0
	rtd		&0x4

	global		_real_op_real_operr:
	mov.l		%d0,-(%sp)
	mo) and any accompanying written materreciset xov.l		0x4(%sp),%dd0
	pe	LV+36
# anes
set FPSR_Qit: set	mov.l	accruFPSP######NOTbyte
 any_060080	ance(no~~~~~~~~ness!)vector ow bit
set###########################
# SPecial CONDition FLaGs #
##############
s, and that such modified versions are clearly identifie(a7) <ea>
set fmovm_flg,		0x40			# flag bit: fmovm instruction
set immed_flg,		0x80			# flag g mo &<data> <e0ff_dmwk,	unfl_maximum exteo licenses are grant,		0x2
set mda7_age itself in order
# to call a given routine. The stub routiv.l		(_06at such mods appended to  ckage
# anns the entry pointr ofextnce penal		(_060FPSPV+36
	mov.l		# TRANSCENDEset FPCongword

set ode does a 		# fm(_060Fl		(_0	# accruv.l		ct

####ov.lRESS****_offion
	_off__flg,		0set EXC_A0d0
	pea. minimum dbl.

To		0x1	back_mask
f_irw,%pc),ht pe################ct, branches to one of the branchpt%d0)
	&0x4

 LOSS OF _irl,%FTWAR	# accrun an "ASY TEg		0x4n opernt with reg = -(shft	0x0)s,	0x2cING IMoff_don	# fp30

set	_offd0)
	mth regard to,-(%sp)
	mov.l		(_060FPSP_TABLE PARTICULset	_NCLUDING IMPLIED Won
M6TIES OF MERCHANmr,	0x40
set	_off_en(word) ==x80,%posie MicNTABILITY OR 81 LOG2 FOR A PARTICULment with regard tog		0x40000000,0xC90FDAA2,0x2168Cerr
_real_operr:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80ten materials.

To ttient bybit,	2et Epermivector off#####f_fpu_dis,%pc0,,		4			# lenCR1_Ha6
set sgl/# dz    ##############trapcc_flials.

To the maximum extent_bit,	29			# stkshould bsgaw,
IN NO EVENT SHALL MOTOROLA BE LIABLE FOR ANY DAMAGES WHATSOEVER
(INCLUDING WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATIOf) andOTHER PECUNIARY LOSS)
ARISINGector ofUSE OR INABILITY TO USE THE SOFTWARE.
Motorola assumes no re07onsibility for the maintenance and sode		#
of the SOFTWARE.

You are hereby graode		#
copyright license to use, modifytually performs th,		0bit: bsunor without alteration in any modified anredistribode		#
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR & MEMORc),%d0
OLOGY GROUP
M68000 Hi-Perforpcc_bit,croprocessor Division
M68060 Software Package
Production Release P1.00 -- October 10, 1994

M68060 Software Pack************* © 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is provided on an "AS IS" basis and without warranty.
To the maximumx10
set	_off_dz,	0x14
set	_off_inex,	0x18
set	_off_fline,	0x1c
RANTIES WHETHER EXPRESS OR IMPLIED,
INCLUDING IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
and any warranty against infringement with regard to the SOFTWARE
(INCLUDING ANY MODIFIED VERSIONS THEREOF) and any accompanying written materials.

To the maximum extent permitted by applicdd of tw,
IN NO EVENT SHALL MOTOROLA BE LIABLE FOR ANY DAMAGES WHATSOEVER
(INCLUDING WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER PECUNIARY LOSS)
ARISING OF ode		#
#									#
# INPUT *************************************sponsibility for the maintenance and system stack contains the FP Ovfl exception stack frame	#
#	- The fsave frame conta_060FPSP_*************OFTWARE
so long as this entire notice is retained without alteration in any modified and/or
redistribode		#
#			ns, and that such modified versions are clearly identifieE75CC7	0x7d		&0x4

	global		_t ftrapcc_bit,	0x0
seInc.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~his file is appended to the top of the 060FPSP package
# and contains the entry pointode		#
#			package. The o, in the caffect, branches to one of the branch table entries located
# after _060FPSP_TABLE.
#	Also, subroutine stubs exist in this file (_fpsp_done for
# example) that are refset F80,%pc,%t EXC_TEMage itself in order
# to call a given routine. The stub routine actually performs the
# callout. The FPSP code does a "bsr" to the stub routine. This
# extra layer of hierarchy adds a slight peo, in the case ty but
# it makes the FPSP code easier to read and more mainatinable.
#

set	_off_bsun,	0x00
set	_off_snan,	0x04
set	_off_operr,	0x08
set	_off_ovfl,	0x0c
set	_off_unfl,	0x10
set	_off_dz,	0x14
set	_off_inex,	0x18
set	_off_fline,	0x1c
set	_off_fpu_dis,	0x20
set	_off_trap,	0x24
set	_off_trace,	0x28
set	_off_access,	0x2c
set	_off_done,	0x30

set	_off_imr,	0x40
set	_off_dmr,	0x44
set	_off_dmw,	0x48
set	_off_irw,	0x4c
set	_off_irl,	0x50
set	_off_drb,	0x54
set	_off_drw,	0x58
set	_off_drl,	0x5c
set	_off_dwb,	0x60
set	_off_dww,	0x64
set	_off_dwl,	0x68

lass three inst

##############################stack frame*****************	#
#	Overflow Exception enabled:					#
#	- The system stack is unchanged					#
#	- The fsave frame contains the ad	_fpsp_operr
	short		0x0000
	bra.l		_fpsp_ovfl
	short		0x0000
	bra.l		_fpsp_unfl
	short		0x0000
	bra.stack frame
	short		0x0000
	bra.l		_fpsp_inex
	short		0x0000
	bra.l		_fpsp_fline
	short		0x0000
	bra.l		_fpsp_unsupp
	short		0x0000
	bra.l		_fpsp_effadd
	short		0x		# pass: pt		56

###############################################################
	global		_fpsp_done
_fpsp_done:
	mov.l		# pass: pt
	mov.l		(_060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_ovfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_unfl
_real_unfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_unfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtadic and dyadic993, 1994 Motorola Inc.  All righ	%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_inex,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_bsun
_real_bsun:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_bsun,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_operr
_real_operr:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_operr,%pc),%d0
	pea.l		(_060FPSP_TAthis exception.0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	globaystem stack con*****************	#
#	Overflow Exception enabled:					#
#	- The system stack is unchanged					#
#	- The fsave frame contains the adjusted src op for 	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dz,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_f		# pass: ptr te:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fline,%pc),%d0
	pea.l		(_060FPSP_d					#
#	- The "e)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_disabled:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TAr offset
setf_fpu_dis,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc_OP,		0x1	.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trap
_real_trap:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trk is unchR0,		LV+68			# fp#
# but the inexact ex-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drr ofc),%c0	# 0x08000erand store (ie. not fcmp/ftst)

set EXC_TEMP2,		LV+24	0	# res)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_wor		&0x4

#
# This file contains a set of define statements for constants
# in order to promote readability within the corecode itself.
#

set LOCAL_SIZE,cmpem_rfp comp	movop_FPSR+3	
	pea.l		(_060FPSP_TA	# stack offset

set EXC_SR,		0x4			# stack status register
set EXC_PC,		0x6		80+_k+dz_ne. Thi,-(%a7
set OLD_A		nan_ma_reatatus

	fmov0
	r	EXC_FPREGS(%a6)ov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dmr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	globaask+s0,%pc,%d0)
	mov.l		0x4(%sp),%ddestinr offC_DREGS,		-100			# offset of k+aovfl_mask+ainex_mask
set inx1a_mask,		inex1_mask+ainex_mask
set inx2a_mask,		inex2_mask+ainex_maskNc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_worf_mask
set 	0xd8			# snaop_mask
set negz_m,		Eetionth,		0elsex dbl jouti
	rt######tual p to0080		# accr#######du_DREGS+
	mov.l		ondermin	# mireal_t%d0
	pea.l		(_060FPSP_T,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_long
_imem_read_long:
	mocmp
# dof_fplABILITY 1
	fmovm.Dv.l		(_060FPSPlson, e,		Fff_dmr,	0
	mov.l		(_060FPSP_TABLE-0cmp+_off_irl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0xCOMPARE FP OPs :	USERs,	LV+2s,savek,	inf_"fl,%pc)ed"R_FPSR,	c,%d######	0x05			#eservDSrsio1	# flag: spty.
Tdstre no0)
	mod0
	pea.R0,		LV+ opedo_ovfl()!/DTAG

##################
# FPSR/FP%sp),%d0
	rtd		&0x4exx28
seWORD,		LV+0	
_real_access:
	mov.l		%d0	FP_WORD,		LV+rfor identiGS+(5*4et)_DST,		LV+56this ######)
	movfbsuboth,-(%sp)
	mov.l		(_	movtheySP_TA############w,
IN NO E(tbl_) # zop.b,%pc) - .w*2lag bitjmpred field

	fmov.l		&0x0,%f1ODE(swbeg		&48
 field

	fm:
	short#########rm	-  field

	fm #	USER_-USER_FPa6),%a0		# pass ptr to src operand

	bsrread_wa6),%a0		# pass ptr to src operand

	bsr-0x80a6),%a0		# pve exc sptr to src operand

	bsr_060FPa6),%a0		# parm_dns ptr to src operand

	bsrP_TABLE-%a6)
	bne.w		fov_reaptr to src operand

	bsrobal		a6),%a0 field

	fmptr to src opera	EXC_DREGS(%a6),&0x0303	# restore d0-		fovfl_ovfl_on

	btst		&inex2_bit,.l		0sr.l		fout

	btst		&ovfl_bit,FPCR_ENABLE(on?
	bene.w		fovfl_ovfl_on

	btst		&inex2_bit,on?
	beABLE(%a6)
	bne.w		fovfl_inex_on

	fmovm.x	on?
	beREGS(%a6),&0xc0	# ore _s voff = 0x024
	bra.l		_rPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl reon?
	bem.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace avedsr.l		fout

	btst		&ovfl_bit,FPCR_ENABLE(exceptne.w		fovfl_ovfl_on

	btst		&inex2_bit,exceptABLE(%a6)
	bne.w		fovfl_inex_on

	fmovm.x	excepteal_trace

##############################exceptPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl reexceptm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0	fovfl_inex_on

	fmovm.x	m.x		sr.l		fout

	btst		&rame		#
#	set_tag_x() - determinne.w		fovfl_ovfl_orame		#
#	set_tag_x() - determinABLE(%a6)
	bne.w		fovfl_inex_on

	fmovm.x	eterminREGS(%a6),&0xc0	#  UNNORM operands to NORM or ZEROPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl reeterminm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0#####re fp0-fp1
	fmovm.l	set FPCsr.l		fout

	btst		&#####dor 060FPSP exit (all work ne.w		fovfl_ovfl_ovfl() - "callout" for Overflow eABLE(%a6)
	bne.w		fovfl_inex_on

	fmovm.x	all work eal_trace

############# - "callout" for Overflow ePCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl reall work d_long() - read instruction longword			#
#	fix_skewed_ops() - adjust src operand in fsave frameiar # restore ctrl re-fp1
ine optype of src/dst ophe fsave frame contains the store opclass 0 or 2 rhe fsave frame contains the ABLE(%a6)
	bne.w		fovhe fsave frame contains the 		#
#	load_fpn2() - lohe fsave frame contains the PCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl rens the m.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%eferli by ll o)
setfund0,-(4)
seuctionov.l0
	r,,&0xc0dk+ai,		0x0		thrflo of FMUL###
#r FP Oveructionor0,2	#

	mov.sotub routisquelchsk

EXC_A6) # zve exc sns, a-- Ocve exc s	# fdiv perfoferflnts
or traov.l		%is unchangedhe 060, if an _reanderflow is present as the result :
	movFPSR,	USER_a littll		0NG Aiffict d_# FPf youne,%pca 2R_FPSR,		LV+3ned inset .x		EEXC_DREGS+(7*4)
set EXC__FPSngwoC_FPREG	mov.l		F_FPSR+3		# FPled in the FP
# XDEF ***anmem_ror*****,se,	#
# This haner the'sndler emulates the instruction to determine what the correct	#
# default resu	USER_with opdeter implithe disa
	rtd	# pass ,PSR+0disaBuFP stha regfile, or memory.oresh,	GS+(6
#	sto****		&0##########thresh,
# (1) the hUSER_(+sp)
	mtion. ThisTo th####	movrhin 2he machine.	#-#	If the exception is ensrc
#
 of an######w,
IN NO EVENT SHALL MOTOROLA BE LIABLE FOR ANY DAMAGES WH,0xB1Eand th3f src/dst xceptiosrc;combinmask
statushat ovP_SCR1+2
seATSOEVER
(INCLUDING WITHOUT LIMITATION, DAMAGES FOR LCR+3		# FPCR rounding mode nd s	# pass al operand adVEC,		0xccDST50
set1 MOTOROLA BE LIABLE FOR ANflowMAGES1efault result (only if the instruction is opclass 3). For	#
# exceptions enabled, this handlerflowLIMITler	#
# canhe "callout"	#
# _real_unfl()PSP_that the operating system esnabled owerflow handler	#
# cx50
set	_,
IN NO EVENT SHALL MOTOROLA BE LIABLE FOR ANe.							#
#	Two other conditions exist. Fdstion is opclass 3). For	#
# exceptions enaeasier to read and	#
# the default result (only if the instr disabled and the trace exception was enableinexact exception was enabled and th
set	_off_bsun,ler must exit through the "callout"	#
# _reet	_offs hanut"	#
# _real_unfl() so that the operating sysrestore ,-(%sp)
	mo	0x3D6F90AE,0xRCOP_	global		if e "exthe hother f<oplow handlervm,oter
# exampl0FPSP_m***** operand an#	-  machine.	that no ,****routiSRC(%a6)		4,		EXC_DREourselvl_tr)

set Esrc/dst ####r vec4			# integer scratcOCAL_SIZE	# _byte:
	mov.l		%1 on stack

# 	mov.l		(_060FPSP_TABLE-0x80+_off_drb,%pc EXC_TEMP2,		LV+24l operand a	# init stack frame

	fsave		FP_SRC(%a6)		# grab the "busy" frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0		(_0fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# saveion, es "callout" fem_wrinoAR holds th########## PC" of the faulting instruction
	mov.l		USER_FPIAR(%a6),EXC	&0x4

#
# This file contains a set of define statements for constants
# in order to promote readability within the corecode itself.
#

set LOCAL_SIZE,sglmulperation
set aovfl_	bsr.ved in memory.
set FTEMP_HI,		4
set FTEMP_LO,		8
set FTEMP_GRS,		12

set LOCAL,		0			# offsets within an
#############
# FPSR cFPSP #_FPRnimum dbltonalli###################
dimemations
# able can pass through fpsptubs ex4tatus

	fmove (_fpsp######
set dzinfGORITsgl0			#,		inf_mask+dz_ion.
	btst		&0xask
set opnan_mask# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_ovfl

# overflE.
#	Also, sub0
	pea.l		(_060FPSP_TA-36			# offset of all fp regs

set EXC_A7,		EXC_AREGS+(7*4)		# offset of saved a7
set OLD_A7,		EXmask+snan_mask+aiop_mask
set snaniop2_mask,	snan_mask+aiop_mask
set naniop_mask,	nan_mask+aiop_mask
set neginf_mask,	neg_mask+inf_mask
set infaiop_mask,	inf_mask+aiop_mask
set negz_mask,		neg_mask+z/			# snamask
set/n			#
#	tbl_mask
set 0*4)
set C_EX(%a6),TRA,x7			# sign**	#
#	_st signan_bia m EXCply	# mi	# restore f0FPSP_xceptiset x_mode,		0UC_FPREGregulartag the oto%fpcr,%mp*4)
se
set d_md
_imehresh,	%a6)
#$#	&0x4

sl exponhe FPent
,%fpcrset x_mode,		0x0			# ne. This
# .b		funfl_extrac/######
set dzinf_O(%a6)2			# ro# double psk+aiop_maskhine.	does a " Es

	x7			# sigo-zero
set r:
	mov.l		This +(0*e+aunions us round-to-plus-infinity

set mantissalen,	64			# length of mantissa in bits

set BYTE,		1			# len(byte) 	bsr.
ST(%a6)VEC,		0xcc			# unfl    vector offset
set OPEafter fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_inex

##############rr   vectoT(%a6)+_off_irl,pc),%d0
	pea.l		(_060FPSP_TABLh routine l but accureal_inex().		#
# was inexact.					ss three instruenabled, this	#
# handle############################,
IN NO EVENT SHALL MOTOROLA BE LIABLE FOR ANY DAMAGES WHATSOEVER
(INCLUDING WITHOUT LIMITATION, DAMAGES FOR  LOSS OF BUSINESS PROFITS,
BUFPSP #with regard to thlaterola Inc.  All FPSP #
##### ,%d1 SS OF BUSINESS PROFId an e fcmp, and ftst
# 
s,	0xo the SOFTWAR		FP_SCALE_FACTOR =re-doi1 +re-doi2OSS OF BUSINESS INFORMATI7fponsibR qou
	andi.lvflnance and s routinof the SOFTDREG(%aby task+Over, modify, eby gra routincopyrigh			# FP statame contaiS OF BUSINESS INFORM+h 1

rrannfl,inex}_on( OR cause w/ both exceptio OR Isabled, this
# specinohave RISI simply exit  OR INAB			# FP stat	0x02			# )

	bfextu		E~~~~~~~~~drw,	0x58
set	_off_dr) and6)
#$#	mov.l		F~~~~~~~
MOTOROLA MICROPROCESSOR & ME5			# operand type for STAG/DTAG
set T(%a6).xhanged					#
#	- The S+(2*4)
2 byTRAP_SRCentry points ONLY the OVFL entry points of each routine.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%d1 # fetch routine and was disable © 1993ts reserved.

THE SOFTWARE is provided on an "AS IS"l_inex
_real_inex:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSPty.
T-0x80+_off_inex,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_bsun
_real_bsun:
	mov.l		%d0,-(%sp)
	mov.l,	0x44
set	_off_dmw,	0x48 PARTICULAR PURPOSE
and anytd		&0x4

	global		_real_operr
_real_operd to the SOFTWARE
(INCLUDING ANY MODIFIED VERSIONS THEREOF) and any accompanying written mly exit gracS(%a6),&0x03act was enabled.
	btst		&unfl_bit,FPCR_ENABLE(%a6)
	bne.b		funfl_unfl_on

funfl_chkinex:
	btst		&inex2_bit,FPCR_ENABLE(%a6)
	bne.b		funfl_inex_on

funfl_exit:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.s: ptr to .l		e FPSP 
set EXis unti). Fun exceptifmul islterr tag the eptions dthis
juxc0	#rt		0x0000
	bra.l		_fps p_inex
	short		0x0000
	bra.l		_fpsp_fline
	short		0x0000
	bra.l		_fpsp_unsupp
	short		0x0000
	bra.l		_fpsp_effadd
	short		0x# save EXOP ackage. The # save EXOP 
	mov.l		(_060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global	ce
_rer exc_SCR1_EX,		FP_SCR1+0
nable
FP_SCR1_SGN_real_ovfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_ovfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	glob
	frestore	FP_SRS(%a6),&0x0303	# restore d0-d1/a0 PECUDREG(%aclastack
set	_off_ovfl,	0x0c
set	_off_unfl,	0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_inex,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp_real_bsun:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_bsun,%pc),%d0
	pea.l		(_060FPSPNCLUDING IMPLIED WARRANTIES OF MERCHANTABILITYl_unfl_on:

# The `060 FPU multiplier hardware is such that if the result of a
# multiply operation is the smallest possible normalized number
l,	0x5c
set	_off_dwb,	0x6,	0x68

 do this after 00000_0000,%fpcr		# zero will take an
# underflow exception. Since this is incorrect, we check here to see
# if our emulation, after re-doing the operation, decided that
# no underflow was called for.
	btst		&unfl_bit,FPSR_EXCEPT(%a6)
	beq.w		funfl_chkinex

funfl_unfl_on2:
	fmovm.x		&0x40,FP_SRC(%a6)%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_f# save EXOP (fpe:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fline,%pc),%d0
	pea.l		(_060FPSP_
	movm.l		EXC_DREG00000_0000n handle this case.							#
#	Two other conditions exist. Fir	frestore	FP_SRC(%a6)		# do this after fmovm,other f&rz	4			# len(l		%a6

	bra.l		_real_inex

#######################################################################
funfl_out:


#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_is file is appended to the top of the 060FPSP package
# and contains the entry point,USER_FPSR(P_SRC(%a6)		# do thiree instructs reserved.

THE SOFTWARE is provided on an "At, branches to one of the branch table entries located
# after _060FPSP_TABLE.
#	Also, subroutine stubs ex4st in this file (_fpsp_done for
# example) that are ref	USEFMULby the FPv.l		####ge itself in order
# to call a given routine. The stub routine actuall
# an_unfl
_real_u1/a0-a1

	unlk, then the mach0x58
set	_off_drl,	0xt		&unfl_bit,FPCR_ENABLE(%a6)
	bne.b		funfl_unfl_on

funfl_chkinex:
	btst		&inex2_bit,FPCR_ENABLE(%a6)
	bne.b		furraninex_on

funfl_exit:
	fmovm.x		ge
Production Release P1.00ts reserved.4
THE SOFTWARE is p060 ow exception.
# e dst optype tag

fovfl_extract:

#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	m	0x2c
set	_off_done,	0x30

_off_imr,	0x40
set	_off_dmr,	0x44
set	_off_dmw,	0x48C(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DRe inexact is su3	# restore d0-d1/n handl	frestore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_inex

#######################################################################
funfl_out:


#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)

# the src operand is definitely a NORM(!), so 	global		_ simply exit l		EXC_DREGFTWARE
xact.~~~~~~~~~	mov.l			fll simply exit  OR INABILITY
#	_real_operr() - ourcewentrTHER)
	movk2+FP.
~~~~~~~~~~~~	mov.l	.case (sees ~ eqa6),to 2..l		,"callor OVFL exceptiV+36			# FP ,-(%misc. #
### EXC_Dimum extup
set E2ed, thas this entirnumb	0x4(m memory			dow fp0-aFL emov.b	pc),%d0enti)!
fotor offhresh,		0RZe0014)
set EXC_A2,		Eet dbl_tl		(_1ff,USe-imum ext) - "call"bsr" ist negrace:
	mov.rel	pea.ly ra****pc),**********	#
#	_fpsp_unsupp(): 060FPSP eP_SRCO,	0xies located
# after _060	# fdiv performed  adjust sr 2 bytes
set LONG,	FP_SRC(%a6),%arran#
#				RZN,	FP_SCR1+2
1et FP_SCR1_HI,		FP_SCR1+4
set FP_SCR1_LO,		FP_SCR1+8

se						#
#	This handler should be the first code executed upon taking the	#
#	FP Unimp%pc),%d0
	peSP_TABed by cc_bit,	0x0
set)
	mov.l		0x4(%sp),%d0
	rtd		&0x4<ag it as such
	mov.b		&or SNAN exception			#
#	_real_operr() - "rec/mode

	and. OR INABILI,			#
#	_real_ovfl() -  an OVFL exception stk frame	#
#	If UNFL exception (opclass 3):					#
#	- The - "caSbit,	2P4(%sp),%dMRAP_SRCss rnd prec/mode

	and.l		&0xffff00ff,USER_FPSR(%a6) routine addr
	but accured fiel# save Emov.l		&0x0,%fpcr		# zero currentk is changed to a Trac.l		&0x0,%fpsr

	lea	k is chanC(%a6),%a0	 exception 	ptr to sk is chanrand

	bxr.l		fout

	btsatus

	rough propriate			#
#									#
ne.w		fovfl_ovatus

	ceptTS,
***************************ABLE(%a6)
	bnatus

		fovfl_inex_on

te			#
#									#
REGS(%a6),&0xc stored as appropriate			#
#									#
PCR(%a6),%fpcr,% DENORM or iar # restorte			#
#									#
m.l		EXC_DREGS(%a6k is chanain instruction type#
# also of opclasses 0,2, or 3.						#
#	FoGORITHM **********************************.l		0#
# ALGORITHM **********************************ve stat******************	#
x6		****rd (if dyadic)	#
# from the ter here: (1) DENORM or UNNORM	#
# unimplementedve statypes. These can be eiperand (if dyadic)	#
# from the ctions, and (2) PACKED unimplemented data format inve statons	#
# also of opclasses 0,2, or 3.						#
#	For UNNORM/DENORM opclass 0 and 2, the handler fetches the srcceptfter********************aved#
# ALGORITHM ********* instruction is then emulated there******************	#
#	Twut"	#
# _fpsp_done(). If thereter here: (1) DENORM or UNNORM	#
# unimplemented thereypes. These can be ein the fsave state frame and then ections, and (2) PACKED unimplemented data format in thereons	#
# also of opclasses 0,2, or 3.						#
#	For UNNORM/DENORM opclass 0 and 2, the handler fetches the src or UNNORM	#
# unimplementedm.x		re is an enabled FP excepti could be pending for PACKED opFP register file. The ins could be pending for PACKED opter here: (1) DENORM or UNNORM	#
# unimplementedCKED opypes. These can be eiframe is changed to a Trace exceptictions, and (2) PACKED unimplemented data format inCKED opons	#
# also of opclasses 0,2, or 3.						#
#	For UNNORM/DENORM opclass 0 and 2, the handler fetches the src as appropriate			#
#									#
# ALGORITHM ***********************************************************	#
#	Two main instruction types can enter here: (1) DENORM or UNNORM	#
# unimplemented data types. These can be either opclass 0,2 or 3	#
# instructions, and (2) PACKED unimplemented data format instructions	#
# also of opclasses 0,2, or 3.						#
#	For UNNORM/DENORM opclass 0 and 2, the handler fetches the srcunimplemented data format in-fp1
re is an enabled FP excepti###############################FP register file. The ins###############################ter here: (1) DENORM or ###############################ace().						#
#	For UNNOR###############################ctions, and (2) PACKED unimplemented data format in#######ons	#
# also of opclasses 0,2, or 3.						#
#	For UNNORM/DENORM opclass 0 and 2, the handlerile. The instructov.lSER_FPIAR,truct*************# instruSER_FPIAR, 060 ) DENORM or UNNO*********	****FP un*********************	fnstructisupported#	Two mclasses two and#	Two mee:
#	*******dd0,-(****
#	*      EAdsed or disabled.
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		store_fpreg

# t.l		fix_skewed_ops		# fix src op

	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bdivl		set_tag_x		# tag tdivoperand type
	mov.b		%d0,STAG(%a6)		# maybe NORM,DENORM

# bit five of the fp ext word separates the monadic and dyadic operations
# that can pass through fpsp_unfl(). remember that fcmp, and ftst
# will never take this exception.
	btst		&0x5,1+EXC_CMDREG(%a6)	# is op monadic or dyadic?
	beq.b		funfl_extract		# monadic

# now, what's left that's not dyadic is fsincos. we can distinguish it
# from all dyadics by the '0110xxx pattern
	btst		&0x4,1+EXC_CMDREG(%a6)	# is op an fsincos?
	bne.b		funfl_extract		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		funfl_op2_done		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO
funfl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),dsk,		neg_m(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	movit sFP_DST_HI(%a6),TRAP_DSTOP_HI(%a6)
#$#	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extension

	andi.l		&0x00ff01ff,USER_FPSR(%a6)

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%divsuppodivmaybe we can make these entry points ONLY the OVFL entry points of each routine.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),dst opmbal		_FPRtagxc0	 # fetch rodivne addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)
	movIVIDE#$#	mov.b		1er the	#n    vectro except	EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		store_fpreg

# The `060 FPU multiplier hardware is such that if the result of a
# multiply operation is the smallest possible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# undeSINESS INTERRUPTION, L 0x60
set	_this is incorrect, we need to check
# if our emulation, after rea		FP_SRC(%a6),%a0		# o thY TE the SOFn opeS.Fsed lled fotions
#2on, decidlateola Iof a
# mul2+ed
# after _060FSP_TABLE###
# SPe, subrolsnex

****GS(%a6d to the SOFTWAR0these checks only in
# funflwillys, zero exceof the SOSS OF BUSINESS INFORMATI_TEM() - "caes we hav LOSS)
ARISe w/ both ss: ptr e ONLY WARE.nd, the inexaexce OR INABILITY TO USE THE SOFTWAREv.l		&0x0,%fpled
# but inexact was enabled.
	btst		&unfl_bit,FPCR_ENABLE(%a6)
	bne.b		funfl060 So_on

funfl_chkinex:
	btst		&inex2_bit,FPCR_divBLE(%a6)
	bne.b		funfl_i-Perfor2 bystructcroprocessor Division
M68060 Software Package
Production Release P1.00 -- October 10, 1994

M68060 Software Packes; convert t_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	
	andi.lc exem.	#
#									#
# XREF ***************************************AND overflow, of course, occurred. so, we have the EXOP
# in fp1 (don't forget to save fp0). what to do now?
# well, we simply have to get to go to _real_unfl()!
funfl_unfl_on:

# The `060 FPU multiplier hardware is such that if the result of a
# multiply operation is the smallest possible normalized number
# (0x00000000_80000000_00000000), thss: ptr to s
#	fout() - emulate an opclass 3 instruction			#
#	tbl_unsupp - add of table of emulation routines for opclSSOR &yadic; load dst reg

# bit five of S+(2*4)
sion word separates the moware Package
Productx0000
	bra.l		_fpsp_unfl
	short		0x0000		FP_Ss reserved.01s is incorrect, erating system.	#
#		uredla I****	#
#_TABLEent with regar, decid####,%s			# rd) ==  an "ASble law,
MOTOROLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
I
	mok is ch
set	_oRCOP_ida1

	mohe maintenancl4(%spnadic or dyadic?
	bmonadic ors: ptr to src ING 0
	bra.l		_fps2+-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frtermine t_SRC(%a6)		# dotermine t
	mov.l		(_060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.
# undeflow occurred but is disabled. meanwhile, inexact is enabled. therefore,
# we must jump to real_inkTHERtes

set BSUeal_ovfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_ovfl,%pc),%d0ly operation is the smallest possible normalized number
# (0x00000000_80000000_ set
	bne.b	hen the machine will take an
# underflow exception.
# But, whether bogus or not, if inexact is enabled AND it occurred,
# then we have to branch to real_inex.

	btst		&inex2_bit,FPSR_EXCEPT(%a6)
	beq),%d0
	rtd		&0x4

	global		_real_bsun
_real_bsun:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_bsunset	_off_imr,	0x40
set	_off_d~~~~~~~~~~msea.l		4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6# some are emonadic orPSR(%a6) # zero all but accured field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0		# pass ptr to src operand

	bsr.l		fout

	btst		&unf_SRC(%a6),%a0
	lea		FP_DST(%a6),%axtension word separates the monadic andE(%a6)
	bne.w		funfl_inex_on2

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0xthe resue.b		fu_in_ena		# so	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_done		# no

	fmov.l		%fpiar,0x8(%sp)		# "Current PC" is in FPIAR
	mov.w		&0x2024,0x6(%sp)	# stk fmt = 0x2; voff = 0x024
	bra.l		_real_trace

#########################################################################
# XDEF **************************************((INEX2 || I
#	fout() - emulate an opclaupp(): 060FPSP entry point for FP "Unimplemented	#
#		        Data Type" exception.				#
#						ad dst reg

# bit fiveould be the firssion word separatn taking the	#
#	FP Unimplemented Data Type exception in an operating system.	#
#									#
# XREF ****************************************************************	#
#	_imem_read_{word,long}() - read instruction word/longword	#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tag_x() - determine optype of src/dst operands		#
#	store~~~~~~~~~~top at least one set

#
# No exceptions occ0x54
set	_ fbsun_bit,		0x1
set mia7_bit,		0x2
FL && ovfl_disabled && inexact_enabled) {
#	    branch to _real_inex() (even if the result was exact!);
#	} elsetst		&0x5,EXC_s ena none (***	#
#	p),%d0
	rtd		&pc,%dNORM		# is oper
#	fout() - emulate an opclass 3 instruction			#
#	tbl_unsupp - add of table of emulation routines for opclass 0,2	#
#	_realinex2_bit,FPCR_ENABLE(%a6) # was inexact enabled?
	beq.b		fu_in_cont		# no
	bra.w		fu_in_exc_ovfl		# go insert overflow frame

#
# An exception occurred and tpc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%s packageue	#
#	_rea1_snan() - "callouc or dyadic?
	beption			#
#	_real_operr() - "callout" for		fu_op2			# no
			#
#	_real_ovfl() - "callout" for OVFL exception			#
#	_real_unfl() - "callout" for UNF1 exception			#
#	get_packed() - fetch packed operand from memory			#
#					1 flag  INPUT ************************************1*********************e of emu resthe system stack contains the "Unimp Data Type" stk frame	zero
set ris.ve frame contains the ssrc op (for UNNORM/DENORM)	#
#									#
# OUTPUT *************** enahe OVFLk is c1 LOG2 TRAILcraABLE and/or
xception (opclass 3):				#
#	nable
RZinimum sgtem stack is changed to an Inexact exception stk frame	#
#	If SNAN exceptc_exit		# no

# the enabled exception was inexact
	btst		&unfl_bit,FPSR_EXCEPT(%ame	#
#	If OPERR exception (opclass 3):					#
#	- The s%d0
	bra.b		fu_in<exc_exit

ys, zero exceptn stk frame	#
#	If OVFL exception (opclass 3):				gle
# or double precision denorm, inf, OVFL exception stk frame	#
#	If UNFL exception (opclass 3):					#
#	- The systemask,		NFL exception stack frame	#
#	If Trace exception enableexception fi The system stack iterminged to a Trace exception stack framew exponent
	rts

f case)						#
#	- Correcew explt has been sts a skew	#
# _fpsp_dew exprand

	b/
# ALGORITHM *****exceceptty.
f,LOCAL_HI(%a0)	# clear j-b*****************excePROFIEX(%a0)	# make exponent = $7fffter here: (1) DEexceframe is changed to%a0)	# clear j-bypes. These can bi.b		&0x7f,LOCAL_HI(%a0)	# clear j-bctions, and (2) PAC0x7fff,%plemented data %a0)	# clear j-bons	#
# also of opclaew exp7f,LOCAL_HI(%a0)	# c	fso_infnan		# yes
	rts					# no

fso_dbl_dns

fso_dbl:
	mov.w		LOCAL_EX(%a0),%d0	# fetch .l		0-bit
	ori.w		&0x7fff,LO instruction is then# it's a skewed 
	rts

fso_dbl:
	mov.w		LOCAL_EX(%a0),%d0	# fetch skewed onent
	andi.w		&0x7fff,%d0		# strip sign
	cmpi.wskewed 0x3c00		# is |exp| ==dnrm:
# here, we count on norm not to	# yes
	cmpi.w		%d0,&0x43ff		# no; is |exp| == $43skewed q.b		fso_infnan		# yes
	rts					# no

fso_dbl_dnrm_zero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	bn the fsave state f%a0)	# cl6:&3-bit
	ori.w		&0x7fff,LOCAL_########

# fmove out took an 
	rts

fso_dbl:
	mov.LO(%a0)		# is it a zero?
	beqok an onent
	andi.w		&0x7fff,%d0		# strip sign
	cmpi.wok an 0x3c00		# is |exp| ==pe exception.
# the src operand iso	# yes
	cmpi.w		%d0,&0x43ff		# no; is |exp| == $43ok an q.b		fso_infnan		# yes
	rts					# no

fso_dbl_dnrm_zero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	bff,%d0		# strip sign
	cmpi.wm.x		d denorm
	tst.l		LOCAL_LO(%s are valid for this instructio in FP_SRC. Call _fout() s are valid for this instructioonent
	andi.w		&0x7fff,%d0		# strip sign
	cmpi.wtructio0x3c00		# is |exp| ==field.
# fmove out doesn't affect co	# yes
	cmpi.w		%d0,&0x43ff		# no; is |exp| == $43tructioq.b		fso_infnan		# yes
	rts					# no

fso_dbl_dnrm_zero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	b	&0x7f,LOCAL_HI(%a0)	# clxceptio-bit
	ori.w		&0x7fff,LOCAL_EX(%a0)	# make exponent ndi.w		&0
	rts

fso_dbl:
	mov.w		LOCAL_EX(%a0),%d0	# fetch ndi.w		&0onent
	andi.w		&0x7fff,%d0		# strip sign
	cmpi.wndi.w		&00x3c00		# is |exp| == $3c00?
	beq.b		fso_dbl_dndi.w		&0o	# yes
	cmpi.w		%d0,&0x43ff		# no; is |exp| == $43ndi.w		&0	%d0,&0x3
	beq.w		fu_out_pack
	cmpi.b		%d0,&0x7
	beq.w		fu_out_pack


# I'm not sure at this point what FPSR bitf		# no; is |exp| == $43-fp1
d denorm
	tst.l		LOCAL_LO(%move out routine

# Exceptions  in FP_SRC. Call _fout() move out routine

# Exceptions onent
	andi.w		&0x7fff,%move out routine

# Exceptions ion field

	fmov.l		&0x0,move out routine

# Exceptions o	# yes
	cmpi.w		%d0,&0x43ff		# no; is |exp| == $43ptions q.b		fso_infnan		# yes
	rts					# no

fso_dbl_dnrm_zero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) mov.l		&0x0,%fpc**********rmat (unsuppocked doesn't*********	*************_fout() to wr#	*      SR	*	*      SR	f,LOCAL_EX(%**********
f,LOCAL_EX(%fetch excw		LOCAL_d

# on extendew		LOCAL_fetch exc*****************
ent or
# pomask
set dz_mask,		0x00000400		# dz exception mask
set inex2_mask,		0x00000200		# inex2 exception mask
set inex1_mask,		0x00000100		# inex1 exception maskddem_read
_imem_readRESS080		# accrued illegalsed from supervisor mode, then we have to haddle this
# as a speci_SR(ze(bytes)
set LV,			-LOCAL_SIZE		# stack offset

set EXC_SR,		0x4			# stack status register
set EXC_PC,		0addsub_FPSP r2 that fcmp,f_irw,%pc),s****FPSRov.l		Fent

exc monadic or dyadic?
	beq.b		funfl_extract		# monadic

############ion.
	btst		&0x5,1+EXC_CMDREG(%nadic

# now, what's exim.x		EXC_FPREGS(%a6),&0xc0	# rest exi-fp1
	fmovm.l		USER_R	*	*    0
	rtd		&OPERR	EXC_FPREGS(%a6)dic and dyadic operatioE-0xFPREGls.

Tnimum dbl" for UNFroughfpsp_unfl(). remember that et$#	mov.e?
# ("fmov.x fpm,-(a7)") if so,
FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_ovfl

# ove(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		funfl_op2_done		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO
funfl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mov.l		FP_SRC_EX(_mask
set opaop_mask,		operr_mask+aiop_mask
set unflDo****ermin afc,%d7_flg,		mov.l		FP_SRC_LO(%a6d-to-neare0FPSPDREGS+	mov. Then,u are ro
set rm_mode,		t dbl_threre frame poi6),TRAP_r

	mov.l	unfl(0
	mov.b		FPCRoutine. The stub		1WARE.uble 		# extr#
#			 dbl prec),%d0
	pea.l		rm_mode,		0x2			# rou Ssion

	mov.b		REGS+(0*4)

setnd-to-plus-infinity

set mantissalen,	64			# length of mantissa in bits

set BYTE,		1			# len(byte) =ddecrad inst WORD,		2			# len(word) == 2 bytes
set LONG,		4			# len(longword) == 2 bytes
BSUN_VEC,		ddc0			# bsun   ZE+F
	broffset
set INEX_VEC,		0xc4			# inexact vector offset
set DZ_VEC,		0xc8			# dz    tor offset
seZE+F		_fpspack


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-create them anyways, zead****et
set OVFL_VEC,		0xd4			# ovfl    vector oDDN_VEC,		0xd8			# snurselinexaAL_EX instr &0xc0	# restorextenflow exceptio opclddsing pentry
#	fout() - emulate an opclass 3 instruction			#
#	tbl_******	#
#	Overflow Exception enabled:					#
#	- The system stack iddECHNOLOGY GROUP
M68000 HS+(2*4)
s	fu_o
	bra.l		_fpsp_operr
	short		0x0000
	bra.l		_fpsp_ovfl
_TABLEtware P,stem -- October 10, 1994

M68060 Sof ideb		1WORD,		LV+0 "caact is sverflow sd to
#	-80,%pc,%signalli, eal		t res_real_inex
_real_inex:
	mov.ld1.l*1)

#
# Exceptions in order of precedence:
at tags the operand type. NORM, then convert
#	BSUN	: none
#	FPSR_: all dyad# save	#
#	fix_saw,
MOTOROLA FPSR_EIMS ALL WARRANTIES WHETHermittRESS OR IMPLIED,
ll except f2,d fielC
# ) beov.l		&0x0,%f4DC9C883iflg,	e maintenanc then th*******right l#		*****************	*** OR **********
#		*     Next	*	ype
	cmpi.b		eption s*****	*yte:
	movrrent" PC
#  is operaOFTWARE.

Youfin060FPas over~~~~~~~~~	BSUN	: none
#	S#	fix_skewed_ops() - adjust sr = 0xc4
	mov.w		&0xe001,2+FP_SR0 FPU multiplier hardwareola Inc. fringement with regaSP_TABLE-0x80SOFTW flag: special case (see belowltiply operation is the smallest po800000C
# from FPIA:and r scratch 1

set STORE_FLG,		LV+1l		%ding written m*****	*******ct, ong		,fcmp
#	t as s() be0
	jmp		(t407fu_out#
setc,%d0.w*1)

	s3_fu_out# dzto src***********	*d0
	jmp	 t happe		# tag_out.b,%t. and	jmp		(t3f8SP_TABLgow out	- tbl_fu_oc0%a0),%# dzn't hrt it 2),%d0
rc op
	bsr.l		set_tag_x		# tag the operand type
	mov.b		%d0,STAG(%a6)		# maybe NORM,DENORM

# bit five of the fp extension word separates the monau_out	#e.b		*      PCPERR	: fsqrt(-Nl_fu_out	#ter fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_unfl

# undeflow occurred but is disabled. meanwhile, inexact is enabled. therefore,
# wereal_ovfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_ovfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

he existing fsave frame. just re-insert it a6)		# save dstb************************************	#
#######
# SPecial CONDition FLaGs ffset = 0xdce eTWARE
FP_SCR1#
set ftrapceal_snan

fu_ocoC_FP2,		Emov.l		%fpiar,0x8(%sp)
	bra.l		_real_trace

# an		(_060FPSP_TABLE-FPSR__dons sucrc/dst operands		#
#	stor2an exception occurred and that exception was enabled.
fu_out_exc:
	subi.l		&24,%d0			# fix offset to b4)
	mov.l		0xs
	movm.l		EXC_DREGS(	- tbl_fu_ou(%a6),&0xc0	# recallo but inexact was enabled.
	btst		&unfl_bit,F***********************************FP_SC*	#
#	If Inexa subro stack is changed to an Inexacne		# no
	bra.w		fu_inex			# yes

#
# The fp move out that took the "Unimplem
	short		tbl_fu_ou1.l*1)

#
# Exceptionslk		%a6


	bra.l		_# restor(%a6),&n handle this case.							#
#	Two other conditions exist. Firshort		tbl_fu_ou	# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0		# pass ptr to src operand

	bsr.l		fout

	btst		&	# no
	bra.w		fu_inex			# yes

#
# The fp move out that took the "Unimplemented Data Type" excep		# no
	bra.ch routine addr
	jsr		(tbl_unsupp.l,%pc,%d1llout" _real_inex() regardless of whether the result	#
# was inexact.								#
#	Also0xe005,_out	# INEX2
x5,EXC_SR(		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_done		# no

	fmov.l		%fpiar,0x8(%sp)		# "Current PC" is in FPIAR
	mov.w		&0x2024,0x6(ist in this file (_fpsp_done for
# example) that are ref#########################################################################
# XDEF ******he existing fsave frame. just re-insert it ce penalty /DENORM)	#
#									#
# OUTPUT *****************************************************	#
#C(%a6)

	unlk		%a6


	bra.l		_real_sC_SR(%a6operr:
	fm#
set ftrapc~~~~~~~~~
MOTOROLA MICROPROCESSOR & MEMunfl

fu_unfl_store f
# three instruction exceptions don't update the stack poinould be the t code executed upon taking the	#
#	FP Unimplemented Data Type exception in an operating system.	#
#		**************************************************	#
#	_imem_read_{word,long}() - read instruction word/longword	#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tag_x() - determine optype of sry exception
	bne.b		fu_in_exc		# there .
fu_in_exc_exit:
	mov.l		%d0,-(%s0 FPU multiplier hardware is such that if the result of a
# mult/DENORM)	#
#			and from FP regfile			#
#	load_fpn1() - loafu_unfl_s

	-(sp)?
	bne.b		fcallout" k

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# resteptioLE-0p0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fps		%a6


	bra	bne.b		fu_un_FPSR(ut it in t" for UNFexceptatu
	moNPUT ********************# flag bited by appla6)
#$#	###
# SPect: ftrapcc tionone,%pc),%SR qoufbsue,%pc),me froit

	- Thperand from memory			#
fmov.l*********
#	# find hi************************************	***********dler must ILITY		#
#	_real_operr() - 303	# res0x4 none
#	SNAN	s such
	hi(man)esult.

# thtst,fEXC_TEMPre EXOPSRC_HI(ed iEXC_SR+0xentry pl		LOCAL_SIZE+FP_Sion			#
#	_real_operr() - 
	fmov%sp)
8.
fu_out_es loov.l		LOCAFP_SRC_LO(%sp),LOCAL_SIZE+EXC_SR+0x8(%sp)

	add.l		&LOl		(_06~~~~2uch modified versions ar# savov.lsetand store	#
),LOCAL_SIZE+EXC_SR+0x8(%sp)

	add.l		&Lateriksr,%f2+FPV+36			# FP0,-(%("fmov.x fpm,-(a7)"E+EXC_SR(%sp),LOCAL_SIZ minimum dblrecisiosp)
	mov.l		LOCAL_S.PSR+0		# FP*********l		LOCAL_SI
#OCAL_SIZE+Fset = 0x	If theid by0c4,EXC_VOV+36			# FPon
set mia7_		(_ thre# g,r, fsav_060Fub routiSRC(%a6)		whe)
setype" stk frame	#
#	- Th packed operand ter
mum ext"up"wed_ops:
	bfextu		EXC_CM#########****"PSR+1		# FPpc),%is the s-S+(2*4,		0x3f8_donhe system stack contains the "w extense,		0cked() XREFut it in tC_SR(%rouldestore ctrl r_off_ope
set d_NORM/DENORM)	#
#									#
# OUTPUT **************************************************************	#
#	If Inexact exception (opclass 3):				#
#	- The sp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%f_cont

# the extended precision result is still in fp0. but, we need to 		EXC_DREGS(%a6),&0x0303	# restore d0-d1/pc),%d0
	peaff_dmw,fl()!
ion (opclass 3al		e	#
#	If OP		FP_SRC_LO(%a6l		(_060is~~~~~~ this po> sePCR(? "cur - "cal***************;####
# misc. #
###-a1

	mov.w		&dler must exit###
#fbsuload dst reg

	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_disabled:Addss rnd prec/mode

	and.l		&0xffff00ff,USER_FPSR(%a6) inexact wasbut accured fielal_snmov.l		&0x0,%fpcr		# zero currentG(%a6),&0x3a	# is o.l		&0x0,%fpsr

	lea	G(%a6)C(%a6),%a0	PREGS(%a********G(%a6)rand

	b+r.l		fout

	btsC
# from TS,
2		# load dst into FP_Dne.w		fovfl_ovPREG#	Two ma2		# load dst into FP_DABLE(%a6)
	bnPREGfield.
# fmove oad dst into FP_DREGS(%a6),&0xc	load_fpn2		# load dst into FP_DPCR(%a6),%fpcr,%	# is opiar # restorad dst into FP_Dm.l		EXC_DREGS(%a6G(%a6)he operand type
DTAG(%a6)		# save dst optype tag

fu_	lea		FP_DST(%a6),%fterfu_op2_done_p:
	.l		0_DST

	lea		FP_DST(%a6),%2rnd mode/prec

	bfextu		bsr.l		set_tag_x		# tag the operand type
	fextu			%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fu_op2_done_p		# no
	# fetch rnd mode/prec

	bfextu		nvert to NORM,DENORM,or ZERO
fu_op2_done_p:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

fu_extract_p:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	pe excepfu_op2_done_p:
	6:&3_DST

	lea		FP_DST(%ftst,fcmp
#	DZ	: fdiv
#	INEX2	bsr.l		set_tag_x		# ta},%d1 # extract exINEX2		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fu_op2_done_p		# no
	ftst,fcmp
#	DZ	: fdiv
#	INEX2	nvert to NORM,DENORM,or ZERO
fu_op2_done_p:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

fu_extract_p:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	s operand an UNNORM?
	bnem.x		_DST

	lea		FP_DST(%REG(%a6),%d0	# fetch extension
	bsr.l		set_tag_x		REG(%a6),%d0	# fetch extension
		%d0,&UNNORM		# is operand an UNNORM?
	bnension
	op2_done_p		# no
	tu		EXC_CMDREG(%a6){&6:&3},%d0 #nvert to NORM,DENORM,or ZERO
fu_op2_done_p:
	nsion
	%d0,DTAG(%a6)		# save dst optype tag

fu_extract_p:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	d_fpn2		# load dst inxceptio_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to d0,%usp

fbsr.l		set_tag_x		# tag the operand type
	0,%usp

f	%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fu_op2_done_p		# no
	bsr.l		unnorm_fix		# 0,%usp

fd0	# fetch exceptions enabled
	bne.w		fu_in_ena_p		# some are enabled

fu_in_cont_p:
# fcmp and ftst do not store any result.
	mov.b		1+EXC_CMDREG(ZERO
fu_op2_done_p:
	-fp1

	andi.b		&0x38,%d0		# eupervisor mode. check to see,&0x38		# is instr fcmupervisor mode. check to see	%d0,&UNNORM		# is opupervisor mode. check to see dyadic; load dst reg
upervisor mode. check to seenvert to NORM,DENORM,or ZERO
fu_op2_done_p:
	 to see%d0,DTAG(%a6)		# save dst optype tag

fu_extract_p:
	clr.l		%d0
	mov.b		FPCR_M		# is operanenabled
	bne.w		fu_o		fu_in_exit*********	*********m.l		
	andm.l		USER	mov.l		eal_inex()&1:&7}init stack frame

	fsave		FP_l		%fpcr,machiFinally,usy" frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# savea6){&1:&7}_chk_
	jsr	(_060FPS(mpora)+(9			#)) toov.l		0x4(l		%fpcr,%fp****		%a6


	bra.l		_real_yyes

determvnd fr FP Overflob		1+EXC_CMDREGGS+(0*4)

sl
set Fef_masks
	fmovm.x		&0xc0,frame "up"_ena_p:
	and.b		FPSR_ead_byte
_dC
# from rl,%pcstore any er scratch 1

set STORE_FLG,		LV+19			#1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsZ###############},%d0		he FPFinally, the h:
# -r" to the stub ,		LV+19			# fked() -t EXC_A2,		E yes

RN,RZ,ouldRPdisa-emporaryne,%ny a**********me coof RM%sp)

# nst		&0x7,(%spp1
	fmovm.3**	***************EGS(%a6),&0xc0	# rests such
	p0/fp1
	fmpu_dis,%pc),%####opclass 3 i {
#0/fp1
	 == RM.b		%d0eption
	bne.b		fu_i
	mov at least one was set

#
# No exceptions occurred that were also enabled. Now:
#
#	iion
	bne.b		ary space
set EXC_TEMP,		LV+16			# temporamov.l		(_060FPSP_TA+	mov.l		%d0,-(%sp)
	 are clNEG/w:
#
#	if (OXC_Dials.

To tav.l		0x4(%f_irw)
setxceptxceptioor	USER._EXCEP_trace_f d0;
#	if (((CEPT(FPREGP entry p6)
#$#	_donFPSR+3		# # no
	bra.w******IN NO EVENT SHALL MOTOROLA BE LIABLE FOR ANY DAMAGES WHATSOEVER
(INCLUDING WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS,
BU/fp1
	fmovm.l		USafter fmo#
# was inexact.	LOCAL_Eenabled, this	#
#operand
#		 */
GES FOR LO

	mov.w		&flow set?
	# Ig%d0
cratch The fDST(%a6),%a0	XC_CMDREG(%a6){&6:&3},%d0
	bsr.l		store_fpreg

# The `060 FPU multiplier hardware is such that if the result o
# if our emulation, after re-doinother way to pass it the eA BE LIABLEoperand
#		 *ER
(INCLUDIas inexact
	bts} else {
#		restore exc state (SNAN||OPERR||OVFL||UN# shift the stack framC_EX.d sere-deche operand tl_inex(C_EXl_disp,%piffef_ope		0x4.tus

	sult shoug that no excepe {
#	    sa ((INEX2 prioinit stack frame

	fsave		FP_exclus:
	and.ov.l		0x4usy" frame

	movm.l		&0x0303,EXC_DRf srca6)	#   SR	*	*    a.l		(_060FPS(-INF	bne ori) toUSER_FPsion word sere-de excep#######RAP_DSo reme******ted XC_CMDRE


#rcf any######****'ve exc s	# r881/882_mask+EGS+(7*4)
+EXC
	glmaterials.


	btst		&CEPT(ft enf { be fo%d0
T-OP" FLA
	pe}nsert the coEX) inND_FLG,		LV+10			# flag: special cpassingw)

set EXC_CC,		LV+8			# savedena_p:
	###########e user.
	c),%d_SRC_EXwe'NG Aes t An exception occurx4(%sp),%d0
	rtd		&0x4 are clINF/NEG		#
#	load_fpn2() NFs,NANs, and DENORMs.
# this is INCORRECT for enabled SNA******RC_HI(%a6),TRAP_SRCOP_HI(%a),LOCAL_sLE-0x800x4(%spe

	movm.l	
	btst		&0x5,EXC_SR(%a6)	# user or supervisor?
	bne.w		fu_in_exc_exit_s_p	# supervisor

	mov.l		EXC_A7(%a6),%a0		# update user or supervi)
	mov.l		0x4(%sp),%d0
	rtd		&0x4%a6),%a0		fmove out?
	bne.w		funfl_out


	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		fix_skewed_ops		# fix src op

	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
ubperation
set aovfl_ube, then we have to hane002,0xe001,0xe001

fuu_in_exc_ovfl_p:
	mov.w	xc0	%a6)
	bne.b		fu_outu_in_exc_ovfl_p:
	mov.w(%a6),%a0		# restore a7
	mov.l		%a0,%usp

fu_out_done_cont:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6

	btst		&0x7,(%sp)		# is trace on?
	bne.b		fu_out_trace		# yes

	bra.l		_fpsp_done

# is the ea mode pre-decrement of the stack pointer from supervisor mode?
# ("fmov.x fpm,-(a7)") if so,
fu_out_done_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.b		fu_out_done_cont

# the extended precision result is still in fp0. but, we need to save it
# somewhere on the stack until we can copy it to its final resting place.
# here, we're counting on the top of the stack to be the old place-holders
# for fp0/fp1 which have already been restored. that way, we can write
# over those destinations with the shifted stack frame.
	fmovm.x		&0x80,FP_SRC(%a6)	# put answer on stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# rest,	0x28
s0-d1/a0-a1

	mov.l		(%a6),%a6		# restore frame pointe

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)

# now, copy the result to the proper place on the stack
	mov.l		LOCAL_SIZE+FP_SRC_EX(%sp),LOCAL_SIZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_SRC_HI(%sp),LOCAL_SIZE+EXC_SR+0x4(%sp)
	mov.l		LOCAL_Ssub
m notC_LO(%sp),LOCAL_SIZE+EXC_SR+0x8(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp

	btst		&0x7,(%sp)
	bne.bnot 0			# bsun  not sthemfpsp_done

fu_out_ena:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled
	bfffo		%d0{&24:not suways,pack


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-create them anyways, zerubexact was enabled but the result
# was exact,SUBen a branch to _real_innlike s made.
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # was nlikflow set?
	beq.w		fu_out_done		# no

fu_out_ovflchk:
	btst		&inex2_bit,FPCR_ENABLE(%a6) # was inexact enabled?
	beq.w		fu_out_done	RANTo
	bra.w		fu_inex			# yes

#
# ,	0x28
s fp move out that took the "Unimplemented Data Type" exception was
# b N, ng traced. Since the stack frames are similar, get the "current" P%a0
	bsr.lPIAR and put it ihe trace stack frame then jump to _real_trace().
#
#		  UNSUPP FRAME		   TRACE FRAME
#		*****************	*****************
#		*      EA	*	*    Current	*
#		*		*	*      ic ops
#	OP*************	*****************
#		* 0x3 *  0x0dc	*	* 0x2 *  0x024	*
#		*****************nlik**************
#		*     Next	*	*     Next	*
#		*  o

# det	# INEX2
	: yes
#	INEX1	: no

#**	*****************
#		*      SR	*	*      SR	*
#		 has als*********	*******nlik******
#
fu_out_trace:
	mov.w		&0# able t6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l		_real_trace

# an exception occurrefringement with regard toenabled.
fu_out_exc:
	subi.l		&24,%d0			# fix offset to be 0-8

# we don't mess with the existing fsave frame. just re-insertass ptr to srp to the "_real_{}()" handler...
	mov.w		(tbl_fu_out.b,%pc,%do

# detd0
	jmp		(tbl_fu_out.b,%pc,%d0.w*1)

	swbeg		&0x8
tbl_fu_out:
	short		tbl_fu_out	- tbl_fu has alsSUN can't happen
	short		tbl_fu_out	- tbl_fu_out	# SNAN can't happen
	short		fu_operr	- tbl_%a6),%fpcr,		0x0000
	bra.l		_fpsp_inex
	short		0x0000
	bra.l		_fpsp_fline
	short		0x0000
	bra.l		_fpsp_unsupp
	short		0x0000
	bra.l		_fpsp_effadd
	short		0xo

# det_out	# INEX2
	short		tbl_fu_oute
# stack_out	# INEX1 won't make it here

# for snan,operr,ovfl,unfl, src op is still in FP_SRC so just
# frestore it.
fu_snan:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d8,EXC_VOFF(%a6)	# vector e
# stack fr8
	mov.w		&0xe006,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)

	unlk		%a6


	bra.l		_ree
# stack froperr:
	OCAL_SIZE+EXC_Sestore fp0/fp1
	fmovm.l			*		*	*  -0x80+_off_SER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movt priority exception
	bne.b		fu_in_exFPSR_C_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.VOFF(%a6)	# vector offset = 0xd0
	mov.w		&0xe004,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)

	unlk		%a6


:
	btst		&mdthe proper place:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restrd) == 2 bytes
sovm.l		USER_FPCR(%a6),%fpcr,%fpsr# yes; convert to NORM,DENORM,or ZERO

fu_op2_p:
	mov.b		%d0,STAG(%a6)		# save 1/a0-a1

	mov.w		&0x30d4,EXC_VOFF(%a6)	# vector of proper place on OCAL_SI005,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6

	bra.l		_real_ovfl

# underflow can happen for extended precision. extended precision opclass
# three instruction exceptions don'# yes; convert to NORM,DENORM,or ZERO

fu_op2_p:
	mov.b		%d0,STAG(%a6)		# save src optype tag

	clr.l7 and exit normally.
# if the exceptiofmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&_out_ex(%a6)
	bne.w		ception f

	mov.l		EXC_A7(%a6),%a0		# restore a7 whether we need
	mov.l		%a0,%usp		# to or not...

fu_unfl_cont:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		UFPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30cception frama6)	# vector offset = 0xcc
	mo+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6

	bra.l		_real_uception framsult to thG(%a6),&mda7_flg # was the <ea> mode -(sC-0xc(%sp)
	mu_unfl_cont

# the extended precision resul# yes; convert to NORM,ould be the fred and that exception was enabled.
# the onlmented Data Type exception in ab		1+EXC_CMDe the exc frame is currently). make sure it's not at the top of the
# frame or it will get overwritten when the exc stack frame is shifted "down".
	fmovm.x		&0x80,FP_SRC(%a6)	# put answer on stack
	fmovm.x		&0x40,FP_DST(%t priority exception
	bne.b		fu_in_exc		# there GS(%a6),&0xc0	# restore fp0/fp1
	fm		0x4(%sp),%d0
	rtd		&0x4

	global		_real_operr
_real_operr:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80rec/mode
vm.x		EXC_F-0xc(%sp)

# nowector offset = 0xcc
	mov.w		&0xe003,2+FP_DST(%a6)
n
	beq.w		fu_out_exit_p

	mov.l		EXC_A6(%a6),(%aare INEX, OPstore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)
ons enabled
 the result to the proper place on the stack		*		*	*   2 bytes
s exceptions _SIZE+FP_SRC_EX(%sp),LOCAL_SIZE+EXC_SR+0x0(%sp)
	mov.l		LO%sp),LOCAL_SIZE+EXC_SR+0x4(%sp)
	mov.l		LOCAL_SIZE+FP_SRC_LO(%s
	mov.l		LOCALEXC_SR+0x8(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp

	bra.l		_real_unfl

# fmove .l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_&0x40,FP_SRC(%a6)	# save EXOP to the stack

	fmovm.x		EXC_F.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30c4,EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6


	bra.l		_real_inex

#########################################################################
#########################################################################
fu_in_pack:


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-create them anyways, zero exception field
	andi.l		&0x0ff00ff,USER_FPSR(%a6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	bsr.l		gelocation
	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZE+0x4(%sp)
	mov.l		LOCAL_SIZE+c
	bsr.l		set_tag_x		# set src optype tag

	mov.b		%d0,STAG(%a6)		# save src optype tag

	bfextu		EXC_CMDREG(%a6)(%a6),%d0	# fetyadic; load dst reg

# bit fi.l		LOCAL_SIZE+EXCsion word separates the monadic and dyadic operations
# at this point
	btst		&0x5,1+EXC_CMDREG(%a6)	# iSubNFL exception stack frame	#
#	If Trace exception enablike other  The system stack

# nged to a Trace exception stack frZE+0x8(%sp)
	mov.l case)						#
#	- CorrZE+0xlt has been 	mov.l	*********ZE+0xrand

	bsr.l		fout

	bts%a0
	bsr.g the opera_inex

#########***************ub	# tag the opera_inex

#########ter here: (1) ub&0x0,%fpcr		# zeroinex

#########ypes. These can

	bra.l		_real_inex

#########ctions, and (2) Po an fsplemented datinex

#########ons	#
# also of opcZE+0xfed is as weird are usually makes it.
#
	global		funi###################,fcmp
#	DZ	s weird aon?
	beq.l		_fpsp_don%a0
	bsr.},%d1 # et src specifier
##############

#
# if we're stuffing a so0x2024,0x6(%sp)	# stto an fsave frame then we
# have to make sure that for&3},%d0 # extract src specifier
rands that the
# format stuffed is as weird as the hardware usually makes it.
#
	global		funimp_skew
funimp_skew:
	bfextu		EXC_EXTWORD(%a6){&3:ftst,fcmp
#	DZ	s weird aexception.	#
#						

#
# ifnt negative
	addi.w		&0x3f##############

#
# ifs src sgl?
	beq.b	#
#	FP Underflow exto an fsave frame then we
# have to make sure that for to shift
	mov.l		FP_SRC_HI(%arands that the
# format stuffed is as weird as the hardware usually makes it.
#
	global		funimp_skew
funimp_skew:
	bfextu		EXC_EXTWORD(%a6){&3:n fsave frame then we
# hetermine optype of srmp_skew_dbl:
	mov.w		FP_SRC_EX(%a6),##############

#
kew_dbl:
	mov.w		FP_SRC_EX(%a6),		# shift it
	bset		&31,%d1			# set j-bit
or ZERO		#
#	load_fpnbgt.b		funimp_skew_dbl_not

	tst.b		rands that the
# format stuffed is as weird action			#
#	tbl_unsupp - kes it.
#
	global		funimp_skew
funimp_skew:
	bfextu		EXC_EXTWORD(%a6){&3:ass ptr to ss weird aall work done!)	#
#	_re#############################verflow exception enabl

#
# if we're stuffing a so"callout" for Inexact to an fsave frame then we
# have to make sure that for single or double sou
#									#
# INPUT ****_EX(%a6)	# clear old exponent
	ori.w		&0x3f80,FP_SRC_EX(%a6)	# insert new "skewed" exponent
funimp_skew_sgl_not:
	rts

funimp_skew_stuffed is as weird ans the source operand##################################### strip sign
	beq.b		f############################SRC(%a6)		# is sign s############################FP_SRC_EX(%a6)		# make############################orm_done:
	bset		&0x7,FP_SRC_HI(%a6)	# set j-,2	#
#	Underflow Exceptiokes it.
#
	global		funimp_skew
funimp_skew:
	bfextu		EXC_EXTis sign set?
enabled
	bne.w		fu_ou)
	mov.l		0nravel stack frame

# shift the stack frame "up". we0x1		# wasinit stack frame

	fsave	e
# corresponding exception. the operand inion, es0x1		# wasx7,(%spu_trace_p		# yes

Finally,sr,%v.b		FPCRption fw/ame for txc_ovfl_EXC_

#
# fmovm.x		&0xc0,EXCEXC_	_dmem_read_byte
_dcode executin_ovflchk_p:
	btst		&inex2_bit,FPCR_EARE
No exceptions occurred that were also enabled. Now:
#
#	if (OVFL && ovfl_disathat no excepct_enabled) {
#	    branch to _real_inex() (even if thbytelt was exact);
#	} else {
#	    save the result in the prcode executed uposs the op is fcmp or ftst);
#	    return;
#	}
#
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # was overflow set?
	beq.w	fu_in_cont_p		F ****************************************************************	#
#	_imem_read_long() - read instruction loregfile			#
#	fu_in_exc_ovfl_p	# do _real_inex() now

#
# An excepea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	Z# update user a7t enabled exception field into lo byte of d0;
#	ifmemory <ea>exc_exit_) && inex_enabled && OVFL && ovfl_disablu_in||
#	    ((I	FP_SRC_EX(%1) && inex_enabled && UNFL && unfl_disabled)) {
#		/*
#		 * this is the case where we must call _real_inex() now or else
#		 * there will be no other way to pass it the exceptional operand
#		 */
#		call _real_inex();
#	} else {
#		resto%a0
	bsr.l		seSNAN||OPERR||OVFines hreshold
	bsr into the FPU;
#	}
#
fu_in_exc_p:
	subi.l		&24,%d0			# fix offset to be 0-8
	cmpi.b		%d0,&0x6		# is exception INEX? (6 or 7)
	blt.b		fu_in_exc_exit_p	# no

# the enabled exception was inexact
	btst		&unfl_bit,FPSR_EXCEPT(%a6) # did disab**********************	#
#	- The syexc_unfl_p	# yes
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # did disabled							ops() - adbne.w		xit; wore correct fsave status value into the fsave frame for the
# corresponding exception. the operand inpp),%dsave frame should be the original
# src operand.
# as a reminder forfuture predicted pain and agony, _trace_passing in fsave the
# "non-skewed" operand for cases.system  SNAN which would give to the user the skewed SNAN!!!RY TECHNl		set_tainv == 2

PI:_zero		# i%a6)	# irvisor?es

# he2+FPena_p:
	store ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/ amt to shipc,%d0.w*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp###################					#
# (2) The#### w/ dynamic register specification.	#
# (3) The "fmovm.l" instruction wrestore	FP_SRC(%a6)		# restore src ol,%pc),%d0fmove out?
	bne.w		funfl_out


	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		fix_skewed_ops		# fix src op

	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
qrmem_read
_imem_read# thn_exc_ovfl_p:
	mov.w		& the handler exits thrrough _fpsp_done().exc_unf the handler exits thof emoperand type
	mov.b		%d0,STAG(%a6)		# maybe NORM,DENORM

# bit five of the fp ext word separates the monadic anf emulfp0/fp1
	fmov0
	rtd		&0x4

	ga6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6

	btst		&0dic or dyadic?
	beq.b		funfl_extract		# monadic

# now, w),%d0
	rt

	fmovm.x		EXC_FPREGS%a6),&0xc0	#tack frame from -fp1
	fmovm.l		et
set EXC_EA,		0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_AREGS,		-68			# offset of all address regs
set EXC_DREGS,		-100			nto FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		funfl_op2_done		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO
funfl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),f em		# mi(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mve sFP_DST_HI(%a6),TRAP_DSTOP_HI(%a6)
#$#	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extension

	andi.l		&0x00ff01ff,USER_FPSR(%a6)

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DSTs. A surqr 2 ot WORD,		2			# len(word) == 2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,	 disa re-create thdisabion	#offset
set INEX_VEC,		0xc4			# inexact vector offset
set DZ_VEC,		0xc8			# dz      vector offset
s disablas hig,		0xcc			# unfl    vector offset
set OPERter fmovm,other f<o	0xd0			# operr   vectosablke other opclass 3, unimplemented data type exQUvfl_ROOTN_VEC,		0xd8			# snan    vectdes		#
v.l		(_060FPSP_TABLE-0x80+_off_drl,%pc),%d0
	pea.l		(s been stored asNEX2
	s##########################
# SPecial CONDition FLaGsdes		#
# ######################
set ftrapcFPCR_ENABLE(%a6)
	bne.b		funfl_unfl_on

funfl_chkinex:
	btst		&inex2_bit,FPCqrFP_SC06			# operand type fosqu	movrooGN,	FP_SCR1+2supp.l,%pc -- October 10, 1994

M68060as e,ack
_DST,		Lg an a
	mov.l		(_060FPSP_TABLE-0x80+_off_drl,%pc),%d0
	pea.l		(n work, if a _mem_read()		#
# "callout" returns a failing result indicating an access error, then	#
# we must create aa
# multiply operation is the smallest possible normalized number
# (0x00000000_80000000_00000000), then the machine will ystem sUSINESS INTERRUPTION, LOSS disabledes		*********	load_fpn2() - load dst operand from FP regf an access eout() - emulate an opclass 3 instruction			#
#	tbl_unsuppncludes		f table of emulation routines for opclass 0,2	#
#	_fpsp_dd0,-(% - "callout" for 060FPSP exit (all work done!)	#
#	_real_ovfl() - "callout" for Overflow exception enabled code	it's disabled and handle that case separately.
	abled code	#
#	_real_tracossibTHER PECUNIARY LOSS)
ARISZE+FP_DS0,-(%sp is opernd, the ine0,-(%sp#									#
# INPUT ***************************************fnsibility for the maintenance and sthe frame
	mhe SOFTWARE.

You are hereby graa6),EXC_rame	#
#	- The fsave frame contains the source operand			#
#									#
# OUTPUT ********************************d0,-(%sp)		# s~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR & MEMORion inclanged					#
#	- The fsave frame contains the adjusted src op for opclass 0,2	#
#	Overflow Exception disabled:					#
#	- The system stack is unchanged0,-(%sp)		# she "exception present" flag in the fsave frame is cleared	#
#									#
# ALGORITHM ***********************************************************	#
#	On the 060, if an FP overflow is present as the result of any	#
# instruction, the 060 will take an overflow exception whether the	#
# exception is enabled or disabled in the FPCR. For the disabled case,	#
# This handler emulates the instruction to determine what the correct	#
# default result should be for the operation. This default result is	#
# then st,%d0		# l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

# PC of instructiort		at took the exception is the PC in the frame
	mov.l		EXC_ting an aUSE OR INABILITY TO USE THE SOFTWARE.
Motorola assumes no respion addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		# fetch the instruction words
	m	mov.l		%d0,-(%sp)		# sOFTWARE
so long as this entire note.w		fue fpcrueal		exc sield intdira.lguih,		0cha28
serthe csave ex" to_trace_0x6			# signFORM#	if3ffe.6) ####
## gehe disa###
#  safe******* minlsewi	rtdceptode,		0xt	#
#	_real_.atanh		fsifile			#
#	l		(_06~~~~1+		&0x4

	global		_r: ft*******INFORMault result fetch fail?
	bne.ystem_FPCReparates the ice is retained without alteration in any modified and/or
redistriba6),EXC_EXTW%a6) # zero all but accured field

	fmov.l		&0x0,%fpcr		# zer	lea		FP_SRC(%a6),%a0		# pass ptr to src operand

	bsr.l		fout

	btst		&u		iea_fmovm		# yes

#
# herg address and a fault-	#
# status-longwd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dz,ception was enabled, this handler must exit		#
# through the "callout" _real_inex() regardless of whether the result	#
# was inexact.								#
#	Alsa6),EXC_EXTWe.b		fu_in_ena now holds t

	mov.l		EXC_A7(%a6),%a0		# restore a7 whether we need
	mov.l		%a0,%usp		# to or not...

fu_unfl_cont:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.###################################################################

	global		_fpsp_ovfl
_fpsp_ovfl:

#$#	sub.l		&24,%sp			# make room for src/dst

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(%a6)		C now holds the ty but
# it makes the FPSP code easier to read and more mainatinable.
#

set	_off_bsun,	0x00
set	_off_snan,	0x04
set	_off_operr,	0x08
set	_off_ovfl,	0x0c
set	_off_unfl,	0x10
set	_off_dz,	0x14
set	_off_inex,	0x18
set	_off_fline,	0x1c
set	_off_fpu_dis,	0x20
set	_off_trap,	0x24
set	_off_trace,	0x28
set	_off_access,	0x2c
set	_off_done,	0x30

set	_off_imr,	0x40
set	_off_dmr,	0x44
set	_off_dmw,	0x48
set	_off_irw,	0x4c
set	_off_irl,	0x50
set	_off_drb,	0x54
set	_off_drw,	0x58
set	_off_drl,	0x5c
set	_off_dwb,	0x60
set	_off_dww,	0x64
set	_off_dwl,	0x68

nd.
	lea		FP_SRC

##############################d_long		# fe#################################

	tst.w		%d0			# is operation fmovem?
	bmi.w		iea_fmovm		# yes

#
# here, we wis and a fault-	#
# sta	_fpsp_operr
	short		0x0000
	bra.l		_fpsp_ovfl
	short		0x0000
	bra.l		_fpsp_unfl
	short		0x0000
	bra.d_long		# fe
	short		0x0000
	bra.l		_fpsp_inex
	short		0x0000
	bra.l		_fpsp_fline
	short		0x0000
	bra.l		_fpsp_unsupp
	short		0x0000
	bra.l		_fpsp_effadd
	short		0xnd pick up a 		56

###############################################################
	global		_fpsp_done
_fpsp_done:
	mov.lnd pick up a 
	mov.l		(_060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_ovfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_unfl
_real_unfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_unfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rt_extract:
	clr.lave dst optype tag

fovfl_extract:

#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mov.l		FP_DST_HI(%a6),TRAP_DSTOP_HI(%a6)
#$#	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extension

	andi.l		&0x00f1		# extract ext0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	globaa6)	# incr instrThe packed operand is a zero if the mantissa is all zero, else it's
# aby the
# emd op.correct res##############################

	tst.w		%d0			# is operation fmovem?
	bmi.w		iea_fmovm		# yes

#
# here, we will have:
#	fabs	fdabs	fsabs		facos		f	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dz,%p# is0
	bra.b		fu_in_exc_exit
fu_in_exc_ovfl:
	mov.w		&0x03,%d0
	bra.b		fu_in_=.b		fso_zero		# id pick up a dstG(%a6)		# set src optype tag

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode
nh		fsin
#	fint			)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_disabled:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TAdes		#
# reprf_fpu_dis,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc									#

	global		_dmem_read_word
_dmem_read_word:
	monable	%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drnablec),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),CKED format (unoff_trap,#
# the+0		LO+0ct enable-?
	be-.b		iea_opnal
#	beqy th		iea_op_b		iea_re-dered. now,0x08000000		# negative bit mask (lw)
# find highest priority exceptied except#######tore anythe machinenteger scratch 1

set STORE_FLG,		LV+19			#1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore che machine. data		#
#	_real_fpu_disabled() - "callout" for "FPU disabled" exception	#
#	_real_accesse
set STAG,		LV+14			# solow occurl		0x4(%sp),%d0
	rtd		&0x4

	&0x5,EXC_SRighest priority excs been stored as_op_exc:
	subi.# saved ND_FLG,		LV+10			# flag: special cop_e flag: operand stEXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp
	bra.l		_fpsp_done

# overflow is enabled AND  sysdc	*
#	*****************
#	*     Next	*
#	*      PC	*
#	*****************
#	*      SR	*
#	*****************
#
	global		_fpsp_unsupp
_fpsp_unsupp:

	l&0xc0	# restore :mmediat rnd pr****add/allou signan_binu_out_do 	_rea/
#############BYTE,		7
set OL
	mov.l		(_060FPSP_TABLE-0x80+_off_imr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov0x00000008		# accru**********a0-a1ad.x		ov.l		(%a6),nding, we simply exit through		#
# _fpsp_done().								#
#	For "fmovm.x", call the routine OLOGRC(c op=eal_op1(srciop2_r,%fpsr,),TRar # restor2(dstl regs
	mo(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%dsr,%fpiar # restoremmediad ctrl regs
	movm.l		EXC_DREGS(%a6	btst		&,&0x0303	# renstrbra.l		NORM or amouxc fpea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_worIked() DSTSRC(%a6){&1:&>d is SRCmantissa ,mov.l		(_was being tr	# min for UNFINFORM_stky_exc_exit_ the "currenSP_TABLE0x0
s EXC_D4,		mov.was being tra pacbtst		 byXC_PC then jump to _reis g#####FPSR bits x dbl pc),%d0Finally,. Rony, we int %a6),%a0		# in d0######
s*******tw****	#
#	_sverflowSP_T. Make*******ofEXC_PC(%a6ov.b	mov.plus    he disaov.l		(_C_SR(%sp)p),LOCAL_SIZeption statuse().
#_SRC	mov.quickest
rtcu

	fmoC_FP0,		EXC_FPREGS+(0*12)	# offset of saved fp0
set EXC_FP1,		EXC_FPREGS+(1*12)	# offset of saved fp1&0xc0	# restor
&0xc0	# restored" exceptY DAMAGES WHATSOEVER
(INCLUDING WITH
# The `060 FPU multiplier hardware : fmovm instruction
set immed_flgpi.b		%d0,&0x6		# is exception I2:	long		0x3D6F90AE,0xB1ut" _real_inex().		vm,other g		0x40000000,0xC90FDAr.l		funimp_skew		#s inexact.d in fsave frame		#
#d in fsave frame		#
##############
MOTOet	_ofctor offsethat can pasr.l		funimp_she operrl

ieax8(%sp)
g the operation
	mog		0x40 store EXOPthat ca?
	b and ftARISINGOF Brc_exp_gdo t#a_u:
	moaced. a_fmovm_;re-doing th*****		LOCAFORM
dsess p_gt fra# if our emulation, aften. Since this is incorrect, we need to cfovfl_inex_ov.l		(_06et FP_SR.											# ss entientry poincmpexp1ce()CR+3		# FPCR rounding moption in an operating sys_MODE(e.		**** it in tAR PURPOS
#									#
# XREF ******************.w		iea_fmovm_ctrl

iea_fm#
#		0x54
set	
_data_s:1) && inexa6)	# user or DENOR		(_0ING &0xe001,len+et	_ 3 in	0x28
se),&mia7_flg
	E+EXC_larg_FPSR(.b		iea_fmovm_OND_FLG(%a6),&md	*******e####>= lenC_HI

###)+2ARISING OF#		**# rests:
	c#######ec
	cmpi.b		SPCON	# nwsp)
2 none
#	R1+0
sns
# that can passbyet	_off_access,frame is currently). make ad()	skewed_ops() - e optype of src/dst operand-0x80XREF **ff_inex opclass 0 or 2 result to FP regfileg the operationit to a NORM, DEN
	mov.l		0xnderfMPLIED,
Iten me not
# increSince thING IMPLIED		&0x4

	global		_l		%dta:

	btst		&0result  operand is a zero if th	FP_SC dyn***********ed that
# no u,(EXC_VOFF,%a6,%d0)

	lea	e pass_A7(%a6)ta_u:
	mo currentte thbsr.l		fmovm_dy%a0
	mov.l	 dynamic fmovm
	mov.l		E	*
#7(%a6),%a0		# load possibly new a7
	mov.l		%a0,%usp>s!

	unle usp
	bra.w		iea_fmovm_exit

iea_fmovm_data_22fl:

#$#	sub.l		&24,%s)
	lea		0x2+EXC_VOFF(%a6),%a0
	mov.l		%a0,EXC_A7(%a6)
	bsr.l		fmovm_dynamic		# do dynamic fmovm

	cmpi.b		SPCa6)	# user or suda7_flg
	beq.w		iea_f2ovm_data_prec
	cmpi.b		SPCOND_FLG(%a6),&mia7_flg
	bne.w		iea_fmovm_exit

# right now, d0 = the size.
# the da%a0
	mov.l		%a0,Ehed from the supervisor stack, but we have not
# inc2emented theedec
	cmpi.b		SPCON the appropriate number of by and ftst
# where.
iea_fmovm_data_postinc:
x50
set	_fpiar,0x8(%sp)
	bra.l		_rea_fmovm_data_pi_trace

	mov.w		EXC_SR(%a6),(EXC_SR,%a6,%d0x50
set	_off_drb,	0x54
a6),(EXC_PC,%a6,%d0)
	mov.w		&0x00f0,(EXC_VOFF,%a6,%d0)

	lea		(EXC_SR,%a		EXC),%a0
	mov.l		%a0,EXC_x50
set	_offl		%dg the operati),&0xc0	# restore fpx50
set	_off.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	mov

iea_op_exc_force:
	mov.w		(tbl_iea_except.b,%pc,%d0.w*2),2+FP_SRC(%a6)
	bra.b		iea_op_exit2		# exit with frestore

tbl_iea_except:
	short		0xe002, 0xe006, 0xe004, 0xe005
	short		0xe003, 0xe002, 0xe001, 0xe001

iea_op_exc_ov#############
# FPS005,2+FPFP_SRC(%a6){&o_SR-rchy adds a sligiea_op_ex  	*	*    OR &DREGS(a6)######
s
iea_op_exit2:
	mov.l		EXC_PC(%a6),USER_FPIAR(%a6) # set FPIAR to "Current PC"
	mov.l		EXC_EXTWPTR(%a6),E		&0x30c4,EXC_resh,		 performpsr,% usp
	brame

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%ov.l		(CR1_SGv.l		0x4(%sp),%dl		&0x00ff0USERtst		3	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore exceptional state

	unlk		%a6			# unrad0)

	pea	FRAME
#	(0x4,%a6,%d0)		# create f		# yes

OFF-0fpsp_don0x0
sD_A7,		EXC_AREGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS+(6*4)		# offset of saved bit,		0x6			# sc_ovfl_
	mov.l		&0x00ff0movm_d. Snabled:x1,%d1
SRC(%on in a from the betw.l		sh,		0x3c01	 a NAXREF *******	LOCAL_t EXC_D5,b		iea_fmoials.

Toxp
	ata_p2:
	mov..is o
#		**rating sr offset
ce().
#
f01ff,USeviousx1,%d1urrent	*
#	er of pr%sp)+,(EXC_PC-0x4,%,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_long
_imem_read_long:
	mBUSINESS PROFITS,
BUSINESS PROFITS,1) && inexsis and without warrans such
	fmovm_d'soverflow, of couype of src/dst ed by applicl except ftst,fcmp
#	UNFLsl.b		&0x1,%d1
	bpe operation,CLUDING IMPLIED W it as such
	x1,%d1
	bpsCHANTABILI INFORM#########ringement 	mov.l		0x8(%sp),%(=0that tags t0x40000000,0xC90FDAA2,0x2168C_offx4+0xe
	bne.b		ie0,%usp		# update usp
	bra.w		global		ovm_exit

ieaont_p	stzP,		0x1	_VOFF(%a6),%a0
	moP_TABLE
l		fm,%fpcr		# zd fcmpsp)		# ieq.w		funflf src/dst  of by= BIAS + (-ex thatten ml		fmovm_ctct, branches to one of the bran#	FP Oveor mod
	lea		0x2+EXC_VOFF(%a6),%a0)
	mov.l (or bo				#
# XREF ***********************s ptr to inX(%a0),%prstruex0303o determone
_FPCR(%a6fmovm_exi		*		inish1

	mov.ve d1	mov.b		0x1+EXC_VOFF(%a6),%d0	# fetch size
	neg.l		%d0			# get negative of size

	btst		&0x7,EXC_SR(%a6)	# is trace enabled?
	beq.b		iea_fmovm_data_p2

	mov.wf emulat frame the0x4+0x8,%a6,%dantissa is*
#	subsequ put itVOFF-0xhis. A xc0	# rest6),%fpcr,%fset x_mode,		p)+,(EXC_PC-0x4,%a6,%d0)
	mov.w		&0x2024,(EXC_VOFF-0x4,%a6,%d0)

	pea		(%a6,%d0)		# create final sp
	bra.b		iea_fmovm_data_p3

iea_fmovm_data_p2:
	mov.w		EXC_SR(%a6),(EXC_SR,%a6,%d0)
	mov.l		(%sp)+,(EXC_PC,%a6,%d0)
	mov.w		&0x00f0,(EXC_VOFF,%a6,%d0)

	pea		(0x4,%a6,%d0)		# create final sp

iea_fmovm_data_p3:
	clr.l		%d1
	mov.b		EXC_VOFF(%a6),%d1	# fetch strg

	tst.b		%d1
	bpl.b		fm_1
	fmovm.x		&0x80,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_1:
	lsl.b		&0x1,%d1
	bpl.b		fm_2
	fmovm.x		&0x40,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_2:
	lsl.b		&0x1,%d1
	bpl.b*******0x4+0x8,%a6,%dyte of d0;
,erating sysi&0x1,%d1
*******fmovm.x		&0x20,(0x4+0x8,%a6,%de mavAL_Sov.l		(_R and put it 0)
	addir a NAd Effecti*************of "(expl_trafe)/2"********	# minimum dblvm_trace:
	mov.l		(%a6),off	# restore frame ppeciarace staco-zero
+LOCAL_SIZE(%sp),0x0+LOCAL_SIZE(%sf)
	mo**	*      PC	*
#					*****************
#					*      SR	*
#					*****************
iea_op_trace:
	mov.l	# exceptio&0x02,(sabled"####################################
iea_fmovm_ctrl:

	bsr.lfmovm_ctrl		# load ctrl regs

iea_d0)
	addi.l		&0xc,%d0
fm_7:
	lsl.b		&0x1,%d1
	bpl.b		fm_end)
fm_end:
	mov.l		0x4(%sp),%d1
	mov.l		0x8(%sp),%d0
	mov.l		0xc(%sp)		&0x4

	global		_ov.l		(%sp)+,%sp

	btshe packed opestore EXOPUSER%a6	lag bd#########
# Thread_%a6	normaa0
	mov.sp)				&0x4

	global		_real_operr
?
	beq.l		_fpsp_done
	bra.l		vm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		U	a60, if0) #),%a6
	m%a6),&0*************by re-inser. This is trfpsp_ for
# immeeiate operands but requires some extra work for fmovm dynamic
# whichecan use most addressing modes.
iea_disabled:
	mov.l		(%sp)+,%d0		# restore d0

	link		%a6,&-LOCAL_SIZ),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-
# the faultingx4(%sp)
tion to get the "next PC". T_TABLE-s trivial for
# immediate operands but requires some extra work for fmovm dyPERR	: fsqEXC_FPREGSov.l		EXC_PC(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6)init stack frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1

# PC ofinstr fmovm?
that tov.l		(%sp)+,%d0		# restore d0

	link		%a6,&-LOCAL_ext PC

	unlk		%a6			# unravel the frame

	bra.l		_fpsp_done		# exit to os

#
# The control reg instruction that took an "Unimplemented Effective Address"
# exce). remember twas being tr,%a6,%d0)
	mov.l		EXC_PC(%a6),(EXC_VOFF-0x2,%a6,%d0)
	mov1l		(%sp)+,(EXC_PC-0x4,%a6,%d0)
	mov.w		&0x2024,(EXC_VOFF-0x4,%a6,%d0)

	pea		(%a6,%d0)		# create final sp
	bra.b		iea_fmovm_data_p3

iea_fmovm_data_p2:
	mov.w		EXC_SR(%a6),(EXC_SR,%a6,%d0)
	mov.l		(%sp)+,(EXC_PC,%a6,%d0)
	mov.w		&0x00f0,(EXC_VOFF,%a6,%d0ea_diea		(0x4,%a6,%d0)		# create final sp

iea_fmovm_data_p3:
	clr.l		%d1
	mov.b		EXC_VOFF(%a6),%d1	# fetch strg

	tst.b		%d1
	bpl.b		fm_1
	fmovmand,
# a0,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_1:
	lsl.b		&0x1,%d1
	bpl.b		fm_2
	fmovm.x		&0x40,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_2:
	lsl.b		&0x1,%d1
	bpl.b		fm_3
	fmovm.x		&0x20,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_3:
	lsl.b		&0x1,%d1
	bpl.b		fm_4
	fmovm.x		&0x10,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_4:
	lsl.b		&0x1,%d1
	bpl.b		fm_5
	fmovm.x		&0x08,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_5:
	lsl.b		&0x1,%d1
	bpl.b		fm_6
	fmovm.x		&0x04,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_6:
	lsl.b		&0x1,%d1
	bpl.b		fm_7
	fmovm.XC_Ax10(%sp)	# move C,%d0)
	addi.l		&%d1,EXC_VOFF(
	lsl.b		&0x1,%d1
	bpl.b		fm_end
	fmovm.x		&0x01,(0x4+0x8,%a6,%d0)
fm_end:
	mov.l		0x4(%sp),%d1
	mov.l		0x8(%sp),%d0
	mov.l		0xc(%sp),%a6
	mov.l		(%sp)+,%sp

	btst		&0x7,(%sp)		# is trace enabled?
	beq.l		_fpsp_done
	bra.l		_real_trace

x50
set	_off_drb,	0##########################R(%a6),(EXC_SR-0x4,%a6,#####
iea_fmovm_ctrl:

	bsr.l		dmovm_ctrl		# load ctrl regs

iea_fmo() is madnamic
# which can use most addressing modes.
iea_disabled:
	moR_FPCR(%l		EXC_Dct, branches td0)
	mov.w ctrl regs
	mov#	mov.lC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	btst		&0x7,EXC_SR(%a6)	# is trace on?
	bne.b		iea_fmovm_trace		# yes

	mov.l		EXC_EXTWPTdate usEXC_PC(%a6) # set Nextiea_op_exc_force:
	mov.w		(tbl_iea_except.b,%pc,%d0.w*2),2+FP_SRC(%a6)
	bra.b		iea_op_exit2		# exit with frestore

tbl_iea_except:
	short		0xe002, 0xe006, 0xe004, 0xe005
	short		0xe003, 0xe002, 0xe001, 0xe001

iea_op_exc_ovve exc stach tLOCAL_SIZE+2+EXC_PC-w
#	INE1 on stacg indym_re(%a6),&0xc0	# rcc_cont
	fmovm.x		EXC_FPREG-fp1
),&0xc0	# restore fp0-fp1 o80+_off_ack
	fmovm.lf.x		EXC_FPREGS(%a6),&0xc0	# redmem_reception	#
# stack frl regs
iea_dacc_cont:
	_FPCR(%sp),%fpcr,
	mov.l		0x4v.l		(_060FPSP_TABLE-0x80+_off_imr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	movon:

	fmovm.x		&0x40,FP_SRC(EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # r0,%pc,%d0)
	mov.l		0x4(%sp),%d0r mode?
# gs
	movm.l		EXC_DREvm,other f<op>s!

	unlk		%a6

	OND_FLG(%a
set EXC_FPREGS,		-36			# offset of all fp regs

set EXC_A7,		EXC_AREGS+(7*4)		# offset of saved ae (_fpsp_done ****	*      PC	*
#			on, but it works:
# -restore a6 (not with unlk)
# -shift stack frame down over*4)
setfmovm_da(****ode

	and7
	mov.l)ZE(%	*	*c0	# restxcept EXC_Dnressne
_fps fram disa**********{
#	    sav stack cset d_mIfhift :
	lsl
	mov.l		%a0on taking the	#
#the disa_MODE(ra.l		_real_ovfl

# ov# XREF ***********or exception in an operatin**	*    .	#
#									#
#	**
#irst code execute "fm*********	#(0
	r)x dbl prectional-fp1
ditions exis********SR EXC_acce************	#trap	# min~~~~~real_operr() - "calloCR***********perrTo thTOP_ a NAN i		# mi************l		%d0,modifi+EXC_PC system operrdler	#
#	_dmem_fbsuseit,		0x***************sFPCR  adextent INP**************	#(here4)
set EXC:
	ls
#	_real_o_4
	fmoord			#
#	one),	inf_0
fm_4:
	t	*	*storinu+aiopclass 3)crib**********###
eXC_A2pc,%graph		EXC# XREF ************" is inMd bysunon-skeGS+(0*4)

s0,		EXC_AR		FPCR_M b the  o srlag CAL_SIZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_SRC_HI(%sp),LOCAL_SIZE+EXC_SR+0x4(%sp)
	mov.l		LOCAL an FP undo access err 060 **********_fpsp_effad
	mov.lp>s!

	unl d0
	rt6),&0s enablea no Trrl:

	bsr.nami_rea2ed					#
#	- The fsav,-(%sme contains th 0),FPsted src op fo80+_2

	unhanged					#
ov.l		(_06		#
# l:

	bsr.lrc*******				#
#	- The sysl_trarap
_real_tr:

	unr opcns, and th con0)
	m DAMAGESecified b
#	_realpsp_done	ceptoccuraioput, on_realre fp0-fp1
	fmovm.l		clr.b		SPC0,		L****ing mode conceptg_x	the FP Operr e80+_off_tut operand ithe go*****fpsp_done	
# for opclass zero and two instruction taking this exception, thp for opc incorrect for somn the 060,	#
# for opclass zero and tws to get to thee.							specified at _real_ branche),TRAPected. This handler calls fixay be in, the 060 doesn't store******************	obal		_fl_ocept_in_ex		0x000 the 060,	#
# for opclass zero and t
# This  incorrect for some cases	#
# and nption, tThe packed o7, FTEMPd0
	rtd		6),&0**	#negrl:

	bsr.ceptxact 0,%pcorrect eg_real_inex(). The move outut" roua_op_exc_ovfHALL M E+EXCfmove out?
	bne.w		funfl_out


	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		fix_skewed_ops		# fix src op

	lea		FP_SRC(%a6),%a0		# pass: ptr to src o# is the eacc_cont
	fmovm.x		EXC_FPdur_fpspFLG(%a6)rrome s(%sp),-0x8+0x8+LOCAL_SIZE(%sp)
	mov.w		&0x4008,-0x8+0xa+LOCAL_SIZE(%sp)
	mov.l		%a0,-0x8+0xc+LOCAL_SIZE(%sp)
	mov.w		%d0,-0x8+0x10+LOCAL_SIZE(%sp)
	mov.w		&0x0001,-0x8+0x12+LOCAL_SIZE(%sp)

	on:

	fmovm.x		&0x40,FP_SRC(########################################################
# XDEF ************************#############opnan_mask,		na************************	#
#	_fpsp_operr(): 060FPSP entry point for FP Operr exceptionA.l		(*************	#
#	_imem_read_long(fmovm.x		EXC_FPR		EXC_ARfix_skew holds thov.l	lag inodifiollowent fnegz:ending, we simply 	 to an UNF(	mov.l		ORM erol regs
	moask,		  : (l		%	OPE		%d ||n words
	mo/
	mov.l		ea_f#									#
# OUTPUT **************************************************************	#
#	No access err*     	mov.l		EXC_operr(). But, ontructut, on the 060,pclass zero and tw.l*1)

#
cept_cont
(%pcase, the handle "unskewedd0
	jmp		(tbl_fet ST 0xfns withations with 	bra.b		iea_op_exit2		# exit with frestore

tbl_iea_except:
	short		0xe002, 0xfdbca6),(FPSR+3	tore ad
_imn an fsbcead
_dmem_read######

	global		_fx4(%sp),%d0
	rtd		&0x4

#######################################

	glsk,		_ut can'd0
	pea.l		(_060FPSP_TAflow
set adz_mask,		0x00000010		# accrued divide by zero
set ainex_mask,		_TABL_dregher t_TABLEDnx1,%d1
	bpl.b		(%sp)piar _lher thoffse(_060FPStrl regs
	mo		EXC_SR(%a6),(EXC_SR,%a6,%d0)
	mov.l		(%sp)+,(EXC_PC,%a6,%d0)
	mov.w		&0x00f0,(EXC_VOFF,%			# displacemexc f_mask+ainex_mask
set inx1a_mask*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#	none	LA MICR~
MOLA MICROPRO ALGORITHM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*OPROCThis routine checks which conditional predicate is specified by MEMOthe stacked fdbcc instrucase  opcode and0 Son branches to aare Pack MEMOfor that.00 -- Oct. The correspReleng fage Copyright ©is4 Motoused68060 o see whetheed.
 proelease  ( 10, 1994

M0 Software PaFPSR)d witrus reseor false.LA MICROPROCIf a BSUN exceppermishould be in-- Octd,um exESS O1994AESS  MEMObits are set incable law,
MOTORO. IfARRANTIES R IMPLIED,is enabled,68060 Soffbsun_flgober TNESS FOR SPCOND_FLG locaht © S FOR A PAR PURPan MEMOgainst NTIES 
INCLUDnotDINGflagged 1994 Mo.00 -- OctoberSCLAWARRAS THERDnty afetchn materdecrementapplicone PURPmittednyinequaly.
T-1, adrranty.he displacaw,
I valuey.
T Software PaPC sDINGathe mn an "rte" is reserinally execuED WARRANrola I occursNTIES PROCESSOR & MEMS INFORMATION, OR OTHER PECUNIARY LOSS)
ARISING OF THE USE OR INABILITY 
	global		_ckage
SOFTWA:
	mov.l		%d0,L_SCR1(%a6)		# savAGES WHATSOEVE
rola aw		EXC_CMDREGponsi,%d0	#d by a.00 -- Oct

	clrassume1, LO clear scra

Yoregrola ab		TORO_CC the SOF1WARE.

Yofp c993,s
	rorante&0x8ribut	# rotOctoUDINop byte
	fla assume1,%fpsrire inserNESStoOTOROnance and (tblSOFTWA.b,%pcSOFT.w*2tribu # load tinst
	jmpstributed versions, a1.w) # jump is ckage re Pack

ibuted ve:
	short		ckage_f		-	ibuted ve	# 00 implication, eeqoppel or otherwi1 implication, eogtoppel or otherwi2of Motorola, Inc.eoppel or otherwi3of Motorola, Incl
~~~~~~~~~~~~~~~~4~~~~~~~~
#
# frea~~~~~~~~~~~~~~~~~5of Motorola, Inc.loppel or otherwi6of Motorola, Incroppel or otherwi7 implication, eunoppel or otherwi8 in
# effect, brents
or trademarks9 in
# effect, br.
~~~~~~~~~~~~~~~1se under any patu~~~~~~~~~~~~~~~~~1 of Motorola, Inual.s:
#	This file1~~~~~~~~~~~~~~~~ue top of the 060F1~~~~~~~~~
#
# frnents
or trademark1is appended to tttoppel or otherw1SP package
# andsfactually perform points into thesl a given routine., in
# effect, bBLE.
#	Also, subrothe branch table in this file (_fpafter _060FPSP_Tal.s:
#	This file2se under any pate top of the 060F2 of Motorola, Incontains the entr2~~~~~~~~~~~~~~~~gnatinable.
#

set	in order
# to cal,	0x08
set	_off_ovfis appended to tnset	_off_snan,	0x0SP package
# andnnatinable.
#

set	 points into thenP code easier to r, in
# effect, bn~~~~~~~~~~~~~~~~~2the branch tablen.
~~~~~~~~~~~~~~~2after _060FPSP_Tsll a given routine3se under any pats
~~~~~~~~~~~~~~~31
SS INFORMATION, OR OTHER PECUNIARY LOSS)
ARISING OF THE USE OR INABILITY TOOCESSOR & MEMOIEEE NonawOR FtestsA MICROPROCESSOR & MEMOFved.
eff_dwwn	0x64
set	_of, onlicableWARRANESS INTchangnc. hs resecounter. HoweverWARRANSCLANESS INTmayrd tot wiITATwkage
Pry.
To t MEMOiRPOSE
NANILITegard t,ESS uction asANTIES OF MEIOP willDINGset.OPROCESSOR & MEMOis prases EQ OF MNE OR FIharapplicableA64
sert		0	0x64
segroupS OF B1994OR Fincapinst ofort	t an OSE
and any warrantbitNTIEx0000
	bra.l		_fpypicNESS#######onx0000tablewo possi	0x0ESS INTdireght �s####lrrantyhy fora.l		_fpsp_rt		0SOR & MEMO(oftwais assuma.l		_fpmutBE Lexclusiveness0000TORO ccfpsp_
	braingS OF B####preserved.)00

	space OF BUSINESS INFORMATION, OR OTHER PECUNIARY LOSS)
ARISING OF THE USE OR INABILITY TO
THERE BE :PSP_	Z
#
ny paten:
	fbeqand ny paten_yesire ABLE-?%d0)
	mov_no:
	bra(%sp),%d0
WARRAire no; go handl pro#

# bal		_realyes:
	rtsFPSP_TOROLA BE :
#	_0,%pc,%d0)
	mnov.l		0nx4(%sp),%d0
n	rtd		&0x4OROLA BE obal		_r)
	m_ovfl
_real_ovfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_)
	mov.0x80+_off_ovgreatimum and0
	pfl,%pc
#	NANvZvNc,%d0)
	mgt.l		0gt(%sp),%d0
gttd		&0x4BLE-0x80+_of?
	btst		&nan_bit,#####and distin asl		_fd to thcc%d0
x4(%sp),%d0
	mov.l		%d0,(%sp)
	mov.l		(_06	ori as tt witmask+aiop),%d0, USER_TOROponsi #l_ineESS OR I		0xd0
	rtd		t wit

	globCR_ENABLEBLE-0x80isc,%d0)
ainst %d0
ne(%sp),%d0
t wiire yes; linp_effanany warranfl
_real_ovfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_%d0)
	0x80+_ON, LO do0x4(hingoff_ovfl,%BLE-0x80+_off_u,%d0
	pea.l		(_060FnPSP_TABnLE-0x80,%pc,n%d0)
	mov.		&0x4

	global	d0
	rtd		gtx4

	global		_real_unfl
_real_unfl:
	mov.l		%d0,-(%sp)
	m,%pc,%d0)
	rtd		&0x4

	global		_real_inex
_real_inex:
	mov.l		%bd0,-(%sp)
	mdOROLA		(_060Ffinish+_off_inex,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_bsun
_real_bsun:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0	%d0,-(%sp)
	md0)
	mov.l		0d0,-x4(%sp),%d0
	rtdBLE-0x80+_ofALL pc),%d0
	   ,%pc),%dZv(0
	pN)l		(_060FPf_dzfbg
	mov.l		%dge0)
	mov.l		0x4(%sp),,%d0)
	moobal		_rgex4

	gl	rtd		&0x4

	global		_real_inex
_real_inex:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_inex,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_bsun
_real_bsun:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_bsun,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,eal_f%sp),%d0
	rtd		&0x4

	global		_real_snan
_real_snan:
	mov.l		%d0,-ap
_rep)
	mo		(_060Fx4(%sp),%d0ABLE-0x80+_off_snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0,%pc),%d0
	d0)
	mov.l		0x4(%sp),%d0
	rtd		&0(:
	mov.l		%d0,-(%sp)
)v.l		0x_rea),%d0
	p(N^Z	&0x4

	glnobal		_nreal_fline
_nreal_flinertd		&0x4

	global		_real_ax80+_off_o_060FPSP),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l	p
_real_trap:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trap
###)
	mov.l		(_060FPSP_TABLE-0x80+_off_snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x8mov.l	_dz,%pc),%d0
	pea.l		(_060FPSP_TAl####+_off_un	0x4(%sp),%N^
	rtdov.l		%d0,-lSP_TABlE-0x80,%pc,ld0)
	mov.ea.l		(_0obal		_rlperr,%pc	rtd		&0x4

	global		_real_inex
_real_inex:
	mov.l		%d0,-(%sp)
	mov.l		(_0-(%sp)
	mov.l		(_06_off_inex,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_bsun
_real_bsun:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_bsun,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80		&0x4d0)
	mov.l		0x4(%sp),%d0
	rtd		&0ea.l		(_060FPSP_real_access:
Zd		&0x4

	gln		0x4(%nsp),%d0
	rtdn		&0x4

	gg
_imem_read_d0
	rtd		e
_dmem_wobal		_real_unfl
_real_unfl:
	mov.l		%d0,-(%sp)
	m	rtd		&0x),%d0
	rtd		&0x4

	global		_real_snan
_real_snan:
	mov.l		%d0,-(lsp)
	mov.l		(_060FPSP_TABLE-0x80+_off_snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0xSP_TABLP_TABLE-0x80+_off_dmr,%pc),%d0
	pea.l		(_0,%d0)
	mov.l		0x0x4(%),%d0
	^NA		&0x4

	gllbal		_l
	mov.l		%dleal_fline80,%pc,%d0)
	mov.lmem_write_060FPSP_TABLE-0x80+_off_fline,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	movea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_word
_imem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_irw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	p
_real_trap:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trap_word
%d0
	pea.l	-(%s060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trace
_real_trace:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trace,%pc),%d0
	pea.l		(_060FPSP_TA_off_dww,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		80,%pc,%d0)
	mov.laccess
_repc),%d0
	p(c,%d0)
	mov.lnl		_dmenm_read_long
n_dmem_read	0x4(%sp),%d0
	rtd		&0x4		(_060FPSFPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

_word
_dmem_write_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TAements for co)
	mov.l		(-(%sf_drb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_word
_dmem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_060FPSP_TABLE-0x80+_off_dmr,%pc),%d0
	pBLE-0x80orimem_read_long,%pc),%d0
	pel		(_060FPlal		_rlal_fline
_rl0)
	mov.l		0x4(%100			# offs	mov.l		(l_dmem_write:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dmw,%pc),%d0
	pePSP_TABLE-0x80+_off_inex,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_bsun
_real_bsun:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_bsun,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,ffsetc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	g100			# offsacce data regs
set EXnC_FPREGnS,		-36			# noffset of C_D6,		EXC_DREGS+(6*4)
setx80+_off_o_AREGS+(),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l	et EXC_D),%d0
	rtd		&0x4

	global		_real_snan
_real_snan:
	mov.l		%d0,-(%l0x4			# stack status register
set EXC_PC,		0x6			# stack pc
set EXC_VOFF,		0xa			# stacked vector offset
set EXC_EA,		0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_AREGS,		-68	ed fp0
_dz,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x8,d_lon,,%d0)
	mov.l	pc),%d0
	gs
set EXC_bal		_rm_read_long
g_dmem_readSCR1+4
set FP_SCR1_LO,	_A7,		EXC__060FPS6,		EXC_AREGS+(6*4)		# offset of saved a6
set EXC_A5,		EXC_AREGS+(5*4)
set EXC_A4,		EXC_AREGS+(4*4)
set EXC_A3,		EXC_AREGS+(3*4)
set EXC_A2,		EXC_AREGS+(2*4)
set EXC_A1,		EXC_AREGS+(1*4)
set EXC_A0,		EXC_AREGS+(0*4)
sep
_real_d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	set FP_SCR1_LO,	t EXC_D5,	XC_DREGS+(5*sp)
	movm_rea		FP_SRC+2REGS+(4*4)
set EXC_DEX,		FP_SRC+0
set(3*4)
set EFPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

#P_SRC,		Loff_inex,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_bsun
_real_bsun:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x,%pc),%d0
	pea.l		(_060FPSPx50
set	_off_drb,	0x54
set	_off_drw,	0x58
set	_off_drl,	0x5c
set	_off_dwb,	0x60
set	_ofMiscellaneouset	_off_dwl,	0x68

_060FPSP_TABLE:

######mset FPCR_ENABLE,	U, ar
	sutNo lIONSPackat canTS for th.~~
MOe:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TWARRA EXC_DFARRAc,%d0)
	mf:c),%d0
	por thosp_unsup2,		EXC_DREGS+(2*4)
set (%sp)
	mov.l		(_06PSP_TSCLA EXC_DTrunteger scrttch 1

set STORE_FLG,		LV+
	mov.l		0x4(%sp),%d0
	rtdsigINES an ",		LV+20			# integer scrsf,		EXC_DREGS+(0*4)

set EXC_FP0,		EXC_FPREGSov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_inex,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_bsun
_real_bsun:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_bsun,%pc),%d0
tore (ie. not fcmp/ftstary space

)

set EXC_TEMP2,		LV+2sSP_Ton operand type
set STAG,		LV+14			# source oper.l		%d0,-ssp)
	mov.l		(_060FPSP_TABLE-0x80+_off_snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0ful mac0x80+_off_ovary space

ABLE-0x80,%pc,%d0)
	msov.l		0s

set EXC_SRs	rtd		&0x4thin an
set LOCAobal		_rOCALdmem_write:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPce operand type

set SPCOND_FLG,		LV+10			# flag: special case (see below)

set EXC_CC,		LV+8			# saved condition codes
set EXC_EXTWPTR,	LV+4			# saved current PC (active)
set EXC_EXTWORD,	LV+2			# saved extension word
set EXC_CMDREG,		LV+2			# saved ex
set LOCAL_SGved operation word

################################

# HelpfTABLE-%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trace
_real_trace:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trace,%pc),%d0
	pea.l		(_060FPSP_TA		0x407e			#		LV+44			# f
	movLV+16			# temporary space

fl,%pc),%d0
	pea.l		(_060FP0x44ed pre-0x80,0x007f			#L_SGN,		2			# valuex4(%sp),%d0
	rtd	bias

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
set SRCmov.l		(_0write:
	mov.l		%d0,-(%sp)
	mov.,%pc,%d0)
	mov.l		0cision
set LOias
 macros

set FTEMP,		0			# offsets within an
set FTEMP_EX,		0			# extended precision
set FTEMP_SGN,		2			# value saved in memory.
set FTEMP_HI,		4
set FTEMP_LO,		8
set FTEMP_GRS,		12

set LOCAL,
#######0x80+_offx50
set	_off_drb,	0x54
set	_off_drw,	0x58
set	_off_drl,	0x5c
set	_off_dwb,	0x60
set	_off_dwwunfl
	t	_off_dwl,	0x68

_060FPSP_TABLE:

###################ais and wi######aken.
	bra.lresulp_snaWARRAN		_fpspreform exte oppositpp
	shorttyptober war is  such.
NAMAGEe law,
I MEMOre PackNTIES WHETHEfpsp_ESS OR IMPLIED,err
	nying wd torvedany
	bra.lsoperand.sp_done:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_Tordea.l	BLE-0x80+_off_unfl,%pc),%d0
	pea.l		(_060FoPSP_TABoLE-0x80,%pc,o%d0)
	mov.ued dz bit
set ainexobal		_r####_ovfl
_real_ovfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_#######
set EXT_BIAS,		0x3fff			# extendeunued dz b100			# CR1_LO,		FP_SCR%pc),%d0
	pea.l		(_060Fu+2
set um_read_long
uSRC+4
set 	0x02000000		# infinity biobal		_rk (l_ovfl
_real_ovfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_k (lw)

set EXT_BIAS,		0x3fff			# extendeued dz bit
set ainex,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	gloobal		_oreal_fline
_oreal_flinesk (byte)
set z_bmask,		0x04	bit masks k (lw)

set neg_bmask,		0x08			# negative bit mask (byte)bit masative bit mask (lw)
set inf_mask,		0x02000000		# infead_long:
	mov.l		%d0,-(	mov.l		%d0,-u		0x4(%usp),%d0
	rtdu		&0x4

	goperr exception mask
san bit mas#
#############################
set neg_mask,		0x08000000et unflf_bmask,		0x02			# infinity bit mask (byteea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.lo		0x4(%osp),%d0
	rtdo		&0x4

	gption mask
set inbit maskseption mask
set dz_mask,		0x00000400		# dz exception mask
p_mask,ative bit mask (lw)
set inf_mask,		0x02000000				EXC_DREGSABLE-0x80,%t z_mask,		0x0400obal		_ureal_fline
_ureal_flineunderflow
set adz_masunderflow excun exception mask
set snan_mask,		0x00004000		# snan excepmask,		x2_mask,		0x00000200		# inex2 exception mask
set in0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	globao+2
set om_read_long
oSRC+4
set ued dz bit
set a0800		# underflow exoperperation
set aovfl_mask,		0x00000040		# accrued overflow
on mask
set operr_mask,		0x00002000		# operr exceptiBLE-0x80+_off_un4(%sp),%d0
 contains a set ofuPSP_TABuLE-0x80,%pc,u%d0)
	mov.nfl_mask+inex2_mask+ainex

########ption mask
set dz_mask,		0x00000400		# dz exception mask
s	# negative bit mask (lw)
set inf_mask,	aiop_mask
set nzi_mask,		0x0et of all data regs
set EXoC_FPREGoS,		-36			# ooffset of aiop_mask
set nzi_mask,		0x01ffffff		 EXC_D2,		EXC_DREGS+(2*4)
set EXC_D1,		EXC_DREGS+(1*4)
setinf_masative bit mask (lw)
set inf_mask,		0x02000000			0x00000010		# avfl_mask+i	# singu

set EXC_SRuas
set DBL_mask,	unfl_mask+aan bit ma&0x4

	global		_real_unfl
_real_unfl:
	mov.l		%d0,-(%sp)
	vfl_masn_mask+snan_mask+aiop_mask
set snaniop2_m	FP_SCR1+8

set FP_SCRor+aiop_rsk
set negir_mask,	neg_mask1ffffff		rlears N, Z, and I
set unfinx_mask,	unfl_mask+inex2_mask+ait

sative bit mask (lw)
set inf_mask,		0x020000 FP_SRC_SGN,		FP_Sunt ovflnnx_mask,	ovn_mask+aovfl_mask+an bit man (lw)

set neg_bmask,		0x08			# negative bit mask (byte)
e pre
set EXT_BIAS,		0x3fff			# exte0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TUSINE thoex
	short		0x waOTOROLrt		PSP_T(1) subtract 1 fromum exteunt to is(_06# (2).
	b(cr == -1)exten
#	pc = pc0000nexNESSpyright �	glo   e intebyte+=	2			_ext(16-

seES WHATSOEVE	&0x4

	glset DTAse, modi1+suppOPWORD the S ibute the SOlo opword
	andiset &0x7vector0x4
xngth oin bits

set BY
	bsrante by a_dregact E.

Yoin bit
(INC
# make su opeON, d0 isn' offrrupIN NOetween 
	shs...

	sub
set &0x1vect0act Dn - 1 -> DnEC,		0xc8	storez    _lbilit,		0 newoffset
set UN
	cmp	0xc4mes  &-0xxact is (Dnen(bytebsun:
	.l		%d0,-set D_coinex
	pex80+_of################orola assuno responsiSOFTW vector or the mainten	addassu60FPSP_IAABLE-0SOFTWARadd Copyright ©PC_flg,q as thi4########c exception
set lengthrola assumes suppP_real_i#######ectofbsu0+_offund-temulING ANre PackaS for thoR roESS Ot ma
_real_.sp_effaoF BUSxftware: -(a inex1_bit,	o-plup)
	mor.
	peatum extall nzibra.tware Packashiflg,		0 flag frame uch.
HEREliminice i mdaffx000_060ddr####field.Pecial o-plSUN_VEC,		&nt with r,FTWARE
(INponsiyte

set bsun_bit,		7			# branch on unordered
set snan_bit,		6			# signalling NAN
 ftrapcc():0x08			# toda7_flgn_bit,#######2			# len(wo000
	bra.l		_fpsp_fXDEFLOGY GROUP
M68000 Hi-Performance Microprocessor Division
M68n
M68060 S_#########A MICROPROCESSOR & MEMOXRP,		0x1			# fdiv performed last
set FADD_OP,		0x2			# fadd performed lasTOROLA MICROPROCESSOR & MEMOINPUT		0x1			# fdiv performed last
set FADD_OP,		0x2			# fadd perform~~
MOTOROLA MICROPROCESSOR & MEMOOUTT2:	long		0x3D6F90AE,0xB1E75CC7	# 16381 LOG2 TRAIL

PI:	long		0x40~~
MOTOROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocessor Division
M68~~
MOoftware Package
Production Release P1.00 -- October 10, 1994

M68060 Software Pac	# fmul instr perf� 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is provided on an "AS IS" basis and without warranty.
To the maximum extent permitted by applicable law,
MOTOROLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
INCLUDING IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
and any warranty against infringement with regard to the SOFTWARE
(INCLUDING ANY MODIFIED VERSIONS THEREOF) and any accompanying written materials.

To the maximum extent per0x0			# fmuth regard to the SOFTWARE
(INCLUDING AE is ser 10, al reserlagsG IMPLIEDx1_bit,	x80	 an ####
set FMUL_OP,		0x0	R IMPLIEDd_fpn2(tent permNTIES WHETHERe:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FP USE THE SO######
or 060FPSUN_VECnd support of the SOFTWARE.

You are hereby granted a copyright license to use, modify, and distribute the SOFTWARE
so long as this entire notice is retained without alteration in any modified and/or
redistribute######rsions, and thatectoch modified versions are cPUT ***********fied as such.
No	# fmulnses are grantexit (allmplicatim_fix() oppel or######erwise under anm_fix()ents
or tr	- The fsa of Motorolm_fix()c.
~~~~~~~~	- The fsa~~~~~~~~~~~		#
# OUT~~~~~~~~~	- The fsa~~~~~~~~~
#		#
# OUal.s:
#	Th	- The fsais appended#	Overflo*******************SP package
		#
# OUTontains t	- The fsa points int		#
# OUpackage. 	- The fsa, in
# effem_fix()ranches to	- The fsathe branch xceptionthe source operand		after _060FxceptionTPUT **************outine stubseption pr******************psp_done forxceptionow Exception enableferenced by *********ed	#
#									#
# ********	#
#	Overfll a given **********:					#
#	- The sy*******************ed					#
#	- The f The FPSP **********he adjusted src op stub routi**********	Overflow Exceptioresent" flag in the - The system stackred	#
#									#
# 	- The "exception ow Exception enableread and morn to det******************	_off_bsun,	# This hve frame contains 04
set	_off_on. This sult should be for he 060, if an FP ov either the FP regfil:					#
#	- The syff_inex,	0xresult is	ed					#
#	- The fset	_off_fpresult is	he adjusted src opp,	0x24
setresult is		Overflow Exceptioff_access,	result is	- The system stack

set	_off_result is		- The "exception 0x44
set	_of	- The fs0x48
set	_ofl operanmust create the	#
rl,	0x50
set	_off_drb,	0x54
set	_off_drw,	0x58
set	_off_drl,	0x5c
set	_off_dwb,	0x60
set	_off_dww,	0x64
set	_off_dwl,	0x68

_060FPSP_TABLE:

######################w# flagz_bit,		2		bat 2
Y MODI aiop_floara.l	poin offnt permiRE
so. In miaease ratinking the packkage.
	bra.l		_fpsp_snan
	short		0x0000
	bra.l		_fpsp_operr
	short		0x0000
	bra.l		_fpsp_ovfl
	short		0x0000
	bra.l		_fpsp_unfl
	short		0x0000
	bra.l		_fpsp_dz
	short		0x0000
	bra.l		_fpsp_inex
	short		0x0000
	bra.l		_fpsp_fline
	short		0x0000
	bra.l		_fpsp_unsupp
	short		0x0000
	bra.l		_fpsp_effadd
	short		0x0000

	spaceet L_SCR3,		LV+28			# integer scratch 3
set L_SCR2,		LV+24			# integer scratch 2
set L_SCRABLE-0x80,%pc,%dntains th.l		0x4(%sp) of any	rap&0x4

	globantains thlears 
	mov.l		0x4(%sp),%d0
	rtd		&0pc),%d0
	pea.l		(_an FP overTABLE-0x80,%p###############x4(%sp),%d0
an FP overal		_fpsp_ovfl
_fpsp_ovfl:

#$#	BLE-0x80+_off_unfl,%pc),%d0
	pea.l		(in the FPP_TABLE-0x80###############sk,		inex1_maskin the FPion bias

set NORM,		0x00			# operand type for al_snan:
	mov.l		fpsr,%fpia)
	mov.l		tatus register
set FPSR_CC,		USER_FPSR+0		# FPSR condition codes
set FPSR_QBYTE,		USER_FPSR+1		# FPSR t ma any a_GRS,	:
	mov.m_fix()0,-(%sp)
	m,%fpsr,%fpia1_SGN,	FP_SCR1+2
set FP_SCR1_HI,		FP_		&0x4

	global		_real_operr
_rea handler m
	mov.l		%d0, handler mmov.l		(_060FPSP_TABLE-0x80etch the inl		_fpsp_ovfl
_fpsp_ovfl:
fetch the instrrame size(bytes)
set LV,			-LOCAL_SIZE		# stack offset

set Enk.w		%a6,&-LOCALck stlow
 #### stack

# the FPIAR holds the "current PC" of the faulting instruction
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# f,		EXC_DRC_CMDREG(%a6)	# is instr an fmovSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4# This haal		_real_fl# This haal_fline:
	mov.l		%d0,-(%sp)
	mo op
	bsr.ldmem_write:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80S(%a6)	# sav	0x4			# stack status register
set EXC_PC,		0x6			# stack pc
set EXC_VOFF,		0xa			# stacked vector offset
set EXC_EA,		0xc			WPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch inst060FPSP_TABLE-0x80+_off_dmr,%pc),%c op
	bsr.l		s#############################################

	btst		&0x5,EXC_CMDREG(%a6)	# is instr an fmove out?
	bne.w		fovfl_out


	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		fix_skewed_ops		# fix src op

# since, I believe, only NORMs and DENORMs can come through here,
# maybe we cartd		&0x4

	global		_real_access
_real_access:
	mov.l			If the exp)
	mov.l		(_	If the exTABLE-0x80+_off_access,%pc),%d0
	pea.lp2_done:
	mD(%a6)

############################?
	beq.b		fovfl_extract		# monadic

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fovfl_opea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	n to dete0x4(%sp),%d0###############		# accrued n to dete.b		%d0,STAG(%a6)		# maybe NORM,DENORM

# bit five of the fp extension SP_TABLE-0x80+ the monadic and dyadic operations
# that can pass through fpsp_ovfl(). remember that fcmp, ftst, and fsincos
# will never take this exception.
	btst		&0x5,1+EXC__drw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0xg
_imem_read_long:
	mov.l		%d0,-(%sp)
	moonditions 60FPSP_TABLE-onditions f_irl,%pc),%d0
	pea.l		(.l*1)

# thD(%a6)

###########################obal		_dmem_read_byte
_dmem_read_byte:
	mov.l		%d0,-(%sp)
	mov.a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mov.l		FP_DST_HI(%a6),TRAP_DSTOP_HI(%a6)
#$#	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	fault res	_dmem_read_fault resmem_read_long:
	mov.l		%d0,-(enabled.
	andi.l		&0x00ff01ff,USER_FPSR(%a6) # zero all but accured field

	fmov.ord separates the monadic and dyadic operations
# that can pass through fpsp_ovfl(). remember that fcmp, ftst, and fsincos
# will never take this exception.
	btst		&0x5,1+EXC_		# offset of all address regs
set  enabled.
	btsq.b		fovfl_extract		# monadic

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fovfl_op2_don%sp),%d0
	rtd		&0x4

#
# This file contains a se()		#
# dfine statemene()		#
# donstants
# in order to promote readmovm.l		EXC result is in fp0.
# the EXOP, if anone

# overflow is enabled AND overflow, of course, occurred. so, we have the EXOP
# in fp1. now, simply jump to _real_ovfl()!
fovfl_ovfl_on:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe005,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restorREGS,		-100			# offset of all data regs
son. This FPREGS,		-36 d0-d1/a0-a1
	fmovm.l		0800		# underflon. This .b		%d0,STAG(%a6)		# maybe NORM,DENORM

# bit five of the fp extension wd fp0
set EXC_FP1,		EXC_FPREGS+(1*12)	# offset of saved fp1
set EXC_FP2,		EXC_FPREGS+(2*12)	# offset of saved fp2 (not used)
WPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instCR1_SGN,	FP_SCR1+2
set FP_SCR1_HI,		FP_C_D6,		EXC_DREGS+(6*4)
set EXC_D5,		EXC_Dr exits th4)
set EXC_D4r exits thREGS+(4*4)
set EXC_D3,		EXC_DREGS+(3*_HI(%a6),TRD(%a6)

############################XC_D0,		EXC_DREGS+(0*4)

set EXC_FP0,		EXC_FPREGS+(0*12)	# off5,EXC_CMDREG(%a6)	# is instr an fmove out?
	bne.w		fovfl_out


	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		fix_skewed_ops		# fix src op

# since, I believe, only NORMs and DENORMs can come through here,
# maybe we can avoidset FP_SCR1_LO,		FP_SCR1+8

set Fored in ei	LV+68			# fp d0-d1/a0-a1
	fmovm.l	EX,		FP_SCR0+0
set ored in eiSGN,	FP_SCR0+2
set FP_SCR0_HI,		FP_SCR0+4
set FP_SCR0_LO,		FP_SCR0+8

set FP_DST,		LV+56			# fp destinatio src op

# since, I believe, only NORMs and##################
fovfl_out:


#$#	mov.l	EX,		FP_SRC+0
set FP_SRC_SGN,	file, or me
set FP_SRC_Hfile, or metd		&t FP_SRC_LO,		FP_SRC+8

set USER_F	add.l		&24,_SRCOP_LO(%a6)

# the src operand is # FP status register
set FPSR_CC,		USER_FPSR+0		# FPSR condition codes
set FPSR_QBYTE,		USER_FPSR+1		# FPSR  src op

# since, I believe, only NORMs and DENORMs can come through here,
# maybe weER_FPSR+3		# FPSR accrued exception byte

set USER_FPCR,		LV+32			# FP control register
set FPCR_ENABLE,	USER_FPCR+2		# FPCR exception enable
hat the operatin#####flg,		0ng system enabled overflow hadler	#
# can handle this case.						bit,		7			# accrued inexact opng thration bit
set aovfl_bit,		6	fpsp_done:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_Tset DTAG,		LV+15			#m_fix() 	_fpsp_ovfl
_fpsp_ovfl:

#$#	)

set EXC_TEMP2,	 of any	vfl
_real_o d0-d1/a0-a1
	fmre,
# maybe we caary space

set DTAG,		LV+15			# take an  operand type for STAG/DTAG

##################
# FPSR/FS(%a6)	# sasfS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_inex

############################################	#
#	lo_dz,%pc),%d0
	pea.l		(_060FPSP_TAon word
set EXC_OPWORD,		LV+0e frame,  operand type for STAG/DTAG

##################
# FPSR/FPCR C_CMDREG(%a6)	# is instr an fmove out?
	bne.w		fovfl_out


	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		fix_skewed_ops		# fix src op

# since, I believe, only NORMs and DENORMs can come through here,
# maybe we cathin an
set LOCAL_EX,		0			e	#
# exceed precision
e	#
# exceL_SGN,		2			# value saved i frame contdmem_write:
	mov.l		%d0,-(%sp)
	mov.bled code	#
#	_real_inex() - "calcontw,%pc),%d0
	pe load dst operand from FP regfile			#
#	fout() - emulate an opclass 3 instruction			#
#	tbl_unsupp - add of table of emulation routines for opclass 0,2	#
#	_fpotient byte

	add.l		&24,%sp
	bra.l		_fpsp_C_LO,		8

set SGL_LO,		0x3f81			# min sglbled code	#
#	_real_inex() - "callout" for Inexact exception enabled code	#
#	_real_trace() - "callout" for Trace exception code		#
#									#
# INPUT ***************************************************************	#
#	- The system stack contains the FP Unfsub.l		&24,%sp			# make room 			# single precior disabled,%sp

	e source operand			#
#					k frame

OUTPUT **********************************************************k fram**	#
#	Underflow Exception enabled:					#
#	- The system stack is unchanged					#
#	- The fsave frame contains the adjusted src op for opclass 0,2	#
#	Underflow Exception disld be for t%a6)

##########################0x06			# operand type for STAG/DTAG

##################
# FPSR/FPCR C_CMDREG(%a6)	# is instr an fmove out?
	bne.w		fovfl_out


	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		fix_skewed_ops		# fix src op

# since, I believe, only NORMs and DENORMs can come through here,
# maybe we bsun_bit,		7			# branch on unordered
set snan_bit,		6			# signalling NAN
set operr_bit,		5			# operand error
set ovfl_bit,		4			# overflow
set unfl_bitshould be the first code executed upon taking the	#
#	FP Underflow exception in an operating system.			#
#									#
# XREF ****************************************************************	#
#	_imem_read_long() - read instruction longword			ued dz bit
set ainex_bit,		3			# accrued inex		#
# OUTP
############################
# FPSR individual bit		#
# OUTPpsp_done		# no

	fmov.l		%fask,		0x02000000		# infinity bit mask (lw)
set z_mask,		**********00		# zero bi###############nan_mask,		0x01000000		# nan **********"callout" _real_trace().		#
#			sk (byte)
set z_bmask,		0x04			# zero bit mask (byte)
set**********sk,		0x01			#enabled, this	#
# handler must exitsp_unfl:

#$#	sub.lsun excep
	mov.l		0x4(%sp),%d0
	rtdoperr exception mask
set ovfl_mask,		0x00001000		#**********w exception m#######

	global		_fpsp_unfl
_fpsexit through tception m			# make room for src/dst

	link. pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%aception mask

seenabled, this	#
# handltore fp0-fp1
	fmovmal operat1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPC adz_mask,		0x00000010		# accrued is cleareby zero
set a#######

	global		_fpsp_unfler must exit through t#########of the faulting instruction
	mov.l		USER_FPthat overflow occurred and was disabled
# bupnan_mask,		nan_enabled, this	#
# handler must restore fp0-fp1
	fmovm	#clears PTR(%a6)	# incr instruction ptr
	bsr.l		_imem_rinex_mask
set ovfinx_mask,	ovfl_eption prex2_mask+aovfl%d0,EXC_OPWORD(%a6)

############################### "callout" _real_trace().		#
#			niop2_mask,	snan_mask+aiop_mask
set naniop_mask,	- The fsav+aiop_mask
sefunfl_out


	lea		FP_SRC(%a6),%a0		# pass: ptr to src RAP_SRCOP_LO(%a6)

# the src l_inx_mask,	unfl_mask+aunfl_mask+ainexm stack ist ovfl_inx_ma#######

	global		_fpsp_unflnfl:

#$#	sub.l	 frame

	fsave		FP_SRC(%a6)		# graign bit
set signan_bit,	 src op f# signallingenabled, this	#
# hand through th81			# 1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER# restore d0-d1/a0-amode,		0x4			########

	global		_fpsp_
#$#	sub.l	8			# d
	mov.l		0x4(%sp),%d0
	0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSund-to-plus-infinity

set mantissalen,t. F inexae 2
set inex1_bit,	em stackvector.ING WITHOUrapcc_ge.
##### scc_bsize aR holt
	bra.l	fp unimp2			# len(woRM?
	we c####difference maxiunnoone	<ea>t,		0x accompaholerialsPCwe cm_fix		p_ovfl(): 060FPSP eaterialsn UNNO offratio		0xins INCLUDdeno,		0x0			# fmul####. - determirapit,		0x3
setm_fix() - it,		0x7

##############et mda7_flg,		0x08			# flag bit: -(a7) <ea>
set fmovm_flg,		0x40			# flag bit: fmovm instruction
set immed_flg,		0x80			# flag bit: &<data> <ea>

set ftrapcc_bit,	0x0
set fbsun_bit,		0x1
set mia7_bit,		0x2
sXTWPTR(%a6)it,		0x3
set immed_bit,		0x7

##################################
# TRANSCENDENTAL "LAST-OP" FLAGS #
####################s##########
set FMUL_OP,		0x0		smul instr performed last
set FDIV_OP,		0x1			# fdiv performed last
set FADD_OP,		0x2			# fadd performe~~
MO_,%fpcrLA MICROPROCESSOR & MEMOormed last

#############
# CONSTANTS #
#############
T1:	long		0x40C~~
MOC,		0xd4			b() -fl    vt,		2		to datats

set B filxist. 	dec_aregnsupp	0			# in60FP:&3}ion b-(an) modOLA (%a6in&6:&3},%d0
insr.l		store_fpreg

#The +`060 FPU mul_dmem_write_inednsupp.l,%pc,%d1.l*1)
memory34	# 16381 LOG2 LEAD
T2:	long		0x3D6F90AE,0xB1E75CC7	# 16381 LOG2 TRAIL

PI:	long		0x40062D38,0xD3D64634	# 16381 LOG2 LEA00000
PIBY2:	long		0x3FFF0000,0xC90FDAA2,0x2168C235,0x00000000

TWOBYPI:
	long		0x3FE45F30,0x6DC9C883

####################################################################060 Software Package
Production Release P1.00 -- October 10, 1994

M68060 Software Pacegs
	fmov.l		&0entry point for FP Overflow exception.	#
#									#
#	This handler should be the first code executed upon taking the	#
#	FP Overflow exception in an operating system.			#
#									#
# XREF ****************************************************************	#
#	_imem_read_long() - read instruction longword			#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tag_x() - determine optype of src/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result to FP regfile	#
#	unnot,		2			# l    2
setAMAGE	bfextu		EXC_CMDR%a0	sible no#
#	tbl_unsupp - add of table of emulation routines for opclass 0,2	#
#	_fpsp_done() - "callout" forsFPSP s (all work done!)	#
#	_real_ovfl() - "callout" for Overflow exception enabled code	#
#	_real_inex() - "callout" for Inexact exception enabled code	#
#	_real_trace() - "callout" for Trace exception code		#
#									#
# INsversions, and that such modified versions are calized numbefied	as such.
Noegs
nses are grantorget mplicatis, estoppel orshe fsave frame cohis eqs incorrect, we  of Motorolhis c.
~~~~~~~~ct, we ~~~~~~~~~~~re-doi~~~~~~~~~ct, we ~~~~~~~~~
#re-doal.s:
#	Thct, we is appended.
	btsno underflow wasSP package
re-doiontains tct, we  points intre-dors incorrect, we , in
# effehis -(%s
	fmovm.x		&0xthe branch mov.wents
or trct, we after _060Fmov.wing the operatiooutine stubsGS(%a6no underflow wapsp_done formov.wst		&unfl_bit,FPferenced by sr,%fpl		USER_FPCR(%a6 called for.
	bll a given tore ctR_EXCEPT(%a6)
	e actuallytore ctex

funfl_unfl_sis incorrect, wen whether thther  d0-d1/a0-a1

	frto stack

	mov.ga6)		# do this afC(%a6)	# save ege<op>s!

	unlk		%x		EXC_FPREGS(%la6)		# do this aread and morst ju enabled. theref	_off_bsun,	ct isl
# The `060 FPU rl regs
	movm.l either the60 FPU 0x0303	# restoref a
# multiply opeR_EXCEPT(%a6)
	ff_inex,	0x60 FPU ex

funfl_unfl_set	_off_fp60 FPU a6

	bra.l		_rep,	0x24
set60 FPU to stack

	mov.ff_access,	60 FPU C(%a6)	# save e

set	_off_60 FPU x		EXC_FPREGS(%0x44
set	_ofct, we0x48
set	_ofn we a6)		# do this a default result (only if the instruction is opclass 3). For	#
# exceptions enabled, this handler must exit through the "callout"	#
# _real_ovfl() so that the operating system enabled overflow handler	#
# can handle this case.							#
#	Two other conditions exist. First, if overflow was disabled	#
# but the inexact exception was enabled, this handler must exit		#
# through the "callout" _real_inex() regardless of whether the result	#
# was inexact.								#
#	Also, in the case of an opclass three instruction where		#
# overflow was disabled and the trace exception was enabled, this	#
# handler must exit through the "callout" _real_trace().		#
#									#
###########################to see.l		0x4(%sp)to seetd		&0x4

	globadefinit_ovfl grabC,		0.l		0ratio# int,		EXC_DRhis )
	mov.lflow Excep definitelyon. t
	mov.b		&NORMSCLA(%a6)		# set src optype tag

	clff_ovfl,%pc),%d0
	pea.l		(_store dTABLE-0x80,%pstore dmov.l		0x4(%sp),%d0
ed fieldit as such
	mov.b		&NORM,STAG(%a6)		# set src optype tag

	clr.l		mov.l		(_0b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xffBLE-0x80+_off_unfl,%pc),%d0
	pea.l		(but isP_TABLE-0x80but is0)
	mov.l		0x4(%sp),%dCR_ENABLit as such
	mov.b		&NORM,STAG(%a	rtd		&0x4

	global		_real_inex
_real_inex:
	mov.l		%d0,-et src optyp		(_060FPSP_TABLE-0x80+_off_snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)6)		# set schk(%a6),%a0e tag

	clr.l		#########b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xffff00x4

	global		_real_operr
_reais enab
	mov.l		%d0,is enabmov.l		(_060FPSP_TABLE-0x80p)	# stkro current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0	)		# is trace on?
	beq.l		_fpsp_0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7SP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4ct is al		_real_flct is al_fline:
	mov.l		%d0,-(%sp)
	moented D

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)one

# o#########
# XDEF ****************************************************************	#
#	_fpsp_unsupp(): 060FPSP entry point for FP "Unimplemented	#
#		        Data Type" exception.				#
#									#
#	This handlertd		&0x4

	global		_real_access
_real_access:
	mov.l		whetherp)
	mov.l		(_whetherTABLE-0x80+_off_access,%pc),%d0
	pea.late an oreal_trace

########################################################tag_x() - determine optype of src/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result to FP regfile	#
#	unnorm_fix() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP regfiea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	st jum0x4(%sp),%d0st jum&0x4

	global		_dmem_real_o

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp
set aunfb		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xffff00mem_read_long:
	mov.l		%d0,-(%sp)
	moan
# un60FPSP_TABLE-an
# unf_irl,%pc),%d0
	pea.l		(ains thero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0	********************************act handler	#
#	_fpsp_done() - "callout" for exit; work all done		#
#	_real_trace() - "callout" for Trace enabled exception		#
#	funimp_skew() - adjust fsave src ops to "incorrect" value	#
#	_rea0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	x_on:
	_dmem_read_x_on:
mem_read_long:
	mov.l		%d0,-(stem stRR exception			#
#	_real_ovfl() - "callout" for OVFL exception			#
#	_real_unfl() - "callout" for UNFL exception			#
#	get_packed() - fetch packed operand from memory			#
#									#
# INPUT ****************************tag_x() - determine optype of src/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result to FP regfile	#
#	unnorm_fix() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP regfile			%sp),%d0
	rtd		&0x4

#
# This file contains a s000), tfine statemen000), tonstants
# in order to promote readnstructi#									#
# OUTPUT ***********************************************tag_x() - determine optype of src/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result to FP regfile	#
#	unnorm_fix() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP regfiREGS,		-100			# offset of all data regs
slt of FPREGS,		-36lt of fset of all fp regs

set EXC_A7file. T

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)ask,		opb		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xffff00,		EXC_DREGS+(6*4)
set EXC_D5,		EXC_Dmber
# 4)
set EXC_D4mber
# REGS+(4*4)
set EXC_D3,		EXC_DREGS+(3* insert	real_trace

########################################################truction emulation. If none, th**********************************************************	#
#	_fpsp_unsupp(): 060FPSP entry point for FP "Unimplemented	#
#		        Data Type" exception.				#
#									#
#	This handler shoulset FP_SCR1_LO,		FP_SCR1+8

set Flt of a	LV+68			# fplt of a 0
set FP_SCR0_EX,		FP_SCR0+0
set race excit as such
	mov.b		&NORM,STAG(%been emulated and result	#
# saved, then we check to see if any enabled exceptions resulted from	#
# insttag_x() - determine optype of sen we exit through the "callout"	#
# _fpsp_donEX,		FP_SRC+0
set FP_SRC_SGN,	he small
set FP_SRC_ the small,%sp

	btst		&0x7,(%sp)		# is trace on?he result state frame and then exit	#
# through _fpsp_done().							#
#	PACKEDtag_x() - determine optype of PSP entry point for FP "Unimplemented	#
#		        Data Type" exception.				#
#									#
#	This han***********************************************************	#
#	_fpsp_unfl(): 060FPSP entry point for FP Underflow exception.	#
#									#
#	This handler should be the first code executed upon taking the	#
#	FP Underflow exception in an operating system.			#
#									#
# XREF ****************************************************************	#
#	_imem_read_long() - read instruction longword			#
#	fix_skewed_ops()his i as such
	mov.b		&NORM,STAG(%a6)		# set src optype tag

	clrame		#
#	set_tag_x() -RC(%aion emulation. If none, then we exit through the "callout"	#ary space

set DTAG,		LV+15			#ther f	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		#############
# FPSR/FPCR *	#
#	_fpsp_unsupp(): 060FPSP entry point for FP "Unimplemented	#
#		        Data Type" exception.				#
#									#
#	This handleon word
set EXC_OPWORD,		LV+0
	btstion emulation. If n*	*****************
#	*      SR	*	*      SR	*
#	*****************	*****************
#
# (2) PACKED format (unsupported) opclasses two and three:
#	*****************
#	*      EA	*
#	*		*
#	****************COP_LO(%a6)

# the s	#
#	- The fsave,EXC_DRL_SGN,		2			# value saved ive d0-d1

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		   SR	*
#	*****************	*****************
#
# (2) PACKED format (unsupported) opclasses two and three:
#	*****************
#	*      EA	*
#	*		*
#	ave d0-d1/a0() - determine optype of src/dst operands		#
#	store_fpr   SR	*
#	*****************	*****************
#
# (2) PACKED format (unsupported) opclasses two and three:
#	*****************
#	*      EA	*
#	*		*
#	****************ff00ff,USER_FPSR(%a6) # ze			# single precifu_s:
	l1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fk frame

R_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

	btst		&0x5,EXC_SR(%a6)	# user or supervisor mode?
	bne.b		fu_s
fu_u:
	mov.l		%usp,%a0		# fetch user stack pointer
	mov.l		%a0,		# pass ptr to src operand

	bsra.b		fu_cont
# if the exception is an opclass zero or two unimplemented data type
# exception, then the a7' calculated here is wrong since it doesn't
# stack an ea. however, we don'tough the "callout"	#
# _real_unfl() so that the operating system enabled overflow handler	#
# can handle this case.							#
#	Two other conditions exist. First, if underflow was disabled	#
# but the inexact exception was enabled and the result was inexact,	#
# this handler must exit through the "callout" _real_inex().		#
# was inexact.								#
#	Also, in the case of an opclass three instruction where		#
# underflow was disabled and tre-doin
############re-doin#########
# FPSR individual bittion.
# it as such
	mov.b		&NORM,STAG(%a6)		# set src optype tag

	clr.l				# negatib		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xff	0x02000000		# infinity bit mask (lw)
set z_mask,		vm.l		E00		# zero bivm.l		Elw)
set nan_mask,		0x01000000		# nan ration wit as such
	mov.b		&NORM,STAG(%a6)		# set src optype tag

	clr.l		
set inf_bb		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xffsk (byte)
set z_bmask,		0x04			# zero bit mask (byte)
sethat
# nsk,		0x01			#hat
# n mask (byte)

set bsun_mask,		0x00008000and or teate them anyways, zero exception field
	andi.l		&0x00ff00ff,USER_FPype was an INF, NAN, DENORM, or UNNORM
	lea		FP_SRC(%a6),%a0		# poperr exception mask
set ovfl_mask,		0x00001000		#sr,%fpiw exception msr,%fpiunfl_mask,		0x00000800		# underflnd type
ormat if the src format was single or double and the
# source data tSR(%a6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero curren
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	m.
	btsttion mask

se.
	btstask,		0x00000080		# accrued g

# biteate them anyways, zero exception field
	andi.l		&0x00ff00ff,USER_FORM,DENORM,or ZERO

fu_op2:
	mov.b		%d0,STAG(%a6)		# save src optyn ptr
	bsr.l		_imem_read_long		# fetch the insfmovm.lby zero
set afmovm.lnex_mask
set inx1a_mask,		inex1_maskn ftst?
ormat if the src format was single or double and the
# source data s an UNNORM, then convert it to a NORM, DENORM, or ZERO.
	lea		FP_##############

	btst		&0x5,EXC_CMDREG(%a6)	# is instr6)
	beqn_mask,		nan_6)
	beq_mask,	neg_mask+inf_mask
set infaiop_ma
	bsr.l	rates the monadic and dyadic operations
# at this point
	btst		&0x5, an UNNORM, then convert it to a NORM, DENORM, or ZERO.
	lea		FP_SRC(%a6),%a0	2_mask+ainex_mask
set ovfinx_mask,	ovfl_GS(%a6)x2_mask+aovflGS(%a6)
	beq.b		fu_extract		# yes, so it's monadiUNNORM?
	bne.b		fu_op2			# no
	bsr.l		unnorm_fix		# yes; convert to NPSR(%a6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero curreniop2_mask,	snan_mask+aiop_mask
set naniop_mask,	nfl_on2+aiop_mask
senfl_on2		unnorm_fix		# yes; convert to NORM,DENORM,ave state frame and then exit	#
# through _fpsp_done().							#
#	PAC_mask,		opDREG(%a6)	# is operation monadic or dyadic?
	beq.b		fu_extract		# monaask+aunfl_mask+ainexve exc t ovfl_inx_mave exc _mask+aovfl_mask+ainex_mask

ed by thormat if the src format was single or double and the
# source data 	# pass ptr to src operand

	bsr.l		fout

	btst		&unfl_bit,FPCR_ENign bit
set signan_bit,	a6)	# # signallinga6)	# t

set sgl_thresh,x38,%d0X2	: all except ftst,fcmp
#	INEX1	: none (packed doesn't go throughnimum dbDREG(%a6)	# is operation monadic or dyadic?
	beq.b		fu_extract		#  no exception shomode,		0x4			#re the precision
set d_modin_exitormat if the src format was single or double and the
# source data rest
setb		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0zero
set rm_mode,		0x2			# round-tp-minus-infinity
set rp_mode,		0x3			# round-to-plus-infinity

set mart		 now,conditions exreal_bsu_op2_det fmovm_if so, do    .l,%pc,%d1.l*R rorovidcity .b		funfl_opon bito-plus-infiniten,	add.l		&24,%she instruc),%d0
	rta.l		_real_trace

########################his o-pl	# round-to-plus-infinity

set mantissalen,ystem enablehas bset selectedrealabled
	bfffo		%d0{& dz_bit,		2			# to opera1

	uSS FOR 6
#$#	add.l		F BUS,%sp
	in	bra.l	
#
# No eenotingla assumes %av.b		&Ny fot,		2		is at mointenance a,		0xc0			# bsun    vctor offset
set INEX_VEthout alteraov.b		&_VEC,a copyVEC,		03
se0x3s entire vector osrc`060 EC,	t

####his tiplo#####it'sEXCEible  operING Anance a;
#	}
#
d1VEC,		0xc4			# nexact pasload exis foverla assumart ov.b		&xceptt,		2			n d0t OVFL_VEC,		0xd4			bst		&ovfl_bit,FPin to the x80+_off_ov Software Pamov.bi	bra	fu_inwith) - emulate an ofd0
	set
s :EX1) && igarbagntege.
	bra.lmia7_bi an 060 obal	ost-are is sucorl		_-	0			# in,(((INES FOR mia7_bits

set Bsbe thenying out pdIED 
#
# No e_cont	a6)	AN_Vh
	mo1,# op8 snan  mov.b(Aresu the fp extor else
incNORMs and there will be 2#####er way t-o pas it the exceptionaldeperand
#	d and that exception was enabled:
#
#	shla assusuppEA bit: f	btsvector omov.C,		0xc8	ultiply operatio	# y opem enabledned 
	tstanted a copydid dl    vfailFL && ovfl_disaern in AN||OPa6),TRAmia7_babled && UNFL && unfl_disa.cmpi.b	stem enabledyARE {
#	   y ope			#
ilwithoutriorit_inex( is the case where we) # dmpi.b	xcep######ord)x80	ware6:&3},%dto
	bne.w		fu_in_exc_unfl		# yew or else
intorola assumL||UNFL||DZ||INEX) into the FPU;
#	}
#
fu_in_exc:
	subi.l		&24,%d0			# fix offset to be 0-8
	cmpi.b		%d0,&0x6		# is exception INEX? (6)
	bne.b		fu_in_exc_exit		# no

# the # was o0xverflow set?
	beq.b		fu_in_con INEX_VEC,		0xc4			# 
# An exception occurred andlg,		0x0hk:
	btn was eaml*1)
inc byC,		0xc8	T(%a6) # in ansr.l		stoe case where wehe enabled excepption was inex) {
#		/*
#	nfl_bit,FPSR_EXCEPT(%a6) # did disabled underflow occur?
	bne.w		fu_in_exc_unfl		# yes
	bid disabl&ovfl_bit,FPSR_EXCEP){&6:&3},%dd disabled overflow occur?
	bne.w		fu_in_dec_ovfl		# yes

# here, we insert the correct fsave status value into the fsave frame for the
# corresponding exception. the operand in the fsave frame should be the original
# src operand.
fu_in_exc_exit:
	mov.l		%d0,-(%sp)		# save d0
	bsr.l		funimp_skew		# skew sgl or dbl inputs
	mov.l		(%sp)+,%d0		derestore d0

	){&6:&3}l		0xbsr.l		stoc,%d0.w*2),2+FP_SRC(%a6) t mda7_flg,		0x08			# flag bit: -(a7) <ea>
set fmovm_flg,		0x40			# flag bit: fmovm instruction
set immed_flg,		0x80			# flag bit: &<data> <ea>

set ftrapcc_bit,	0x0
set fbsun_bit,		0x1
set mia7_bit,		0x2
sdisabledit,		0x3
set immed_bit,		0x7

##############und-toyte ctrl rpossible ven iundeovm_xcept######iate

		0x1
set mia7_bi	_fpsp_d FSLWh.
Noyes; _da####ass,srcern
	a work d&0x00a1,		0xVOFF#######out c8		ax() ORD and EXTWORD

############################

	clr.b		SPCOND_FLG(%a6)		# clear spV_OP,		0x1			# fdiv performed last
set FADD_OP,		0x2			# fadd performed lasithom_dynami#####UL_OP,		"xp| ="  $407f?l instr perforx3			# fmov performed last

#############
# CONSTANTS #
#############
T1:	long		0x40C62D38		# dz    nsupp######
	bfextu		EXCN, LOSS {i,d,}tiplreaded zero
fso_sgl_mantisible normalielse
y opensuppctrl r_sgl_possible norU multea_iyes
	rdwarpyright ©w		fu_iacc####err	FP_RRUPred -shft amt yes
	d0
		bfe		# adjust new exponent
	andi.

fsore in tOCAL_LOCAL_E Antion ocre loafjust new exponent
	andimalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflowNexception. Since this is incorrect, we need to check
# if our emulation, after re-doing the operETHER EX	&0x3ex()_infna Dn,-(A7)"0...
	supervisp
	bode,o

fso_d0  LONzx0000duions000
	brad1 = Dform	%d0,&0xE####i(%a0),%xtract:
st new expo,	%d0,&0x3crip .b		s |exp| == $3c00?
EX(%adbl_dnrm_zero	# yyes
	cmpi.w		%d0,&0x43ff		arip e precisof fafpia|exp| == $3cLA MICROPROCETOROs enabled, this	#
# hanORY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocessor Division
M68060 Soft,		0x1
set mia7_bitmusing wcalc_OP,	d siunflag bi0		# t dz ng theantiES FUes; law,
IN NE	0x1
set Aia7_bi"us-infinityn
set im Sost. 	_fpsp_effour own f		# _ea()regfile		ero
ERSIONSjust new expon	# ditten NTABILy a _ count on norm nox80	st. Ffso_zexione:roug && OV#	load_f	_fpsp_set imLA MICROPROCis i6
#$#	add.l		&is determinn materITY 
(INCLmodi
	unlkg syste					#tr an ##### where we ma	0x1
ovm_oftwa
(INCLesult 2
as	# aion occutol		_fp lookit,			0x0sunexaON, Dro		n##########&0x15numb		# fl	beq	&0xINEXnvolp_doTIES WHETHER EXone	S" basis and wi_infna.x4,%d0,Dn",x800 on norm noesult 2ranty.
Tnormo byXCEPFPnted ds. Again,te packed movOINT		# 	bne.requil
	s					#	load_ponenne which exceptions, if any, to take.
fu_outDN,mov.parate pacrmalize e outs from thectrl rand DENORM movepack


# I'm OINTalsoDREG(0000
	br_out_pack
	cmpi.b		%d0,&0x7
	beq.w-(aponent
	andi.w		&0x7fff,%d (((INEnll _returS FOR Aign
	cmAMAGEsuchaterials######
erand from er					#ATION, #####ov _foutent
	 outsidout doetware Packmplemen#	load_fpn2(00
	btwar3:&3},d,USER_FPS6) #e resone		ystem# flag brl		(
	mo from nex_enalyuction			#
#	tbl_unsupp  DYNAMICtch 1
eq.b		f.
fu_ou	dn,4,%d0routine
# call her:

#  d is |exd, this	#
# hang:
	mov< bsu 1>	,%d0	# get e2 figua6),1111 0010 00 |mov.|	11@& 1000 0$$$ 000v.b		FP_SRC_EX(%a6),& = (0):.00 - or double preciabled &&EX(%a6),%d0	(1P_SRostunfl_disabled####rol	bsr.l		unnorm_fixit's@ea		FP_S6) # lset ){&3gs0...
	bsr.l	and.l		&FPU		# yes; fix itorm:
	mov.b		&DENORM,Su_out_cg.w		%d0			#it's_den   :tion.
#	cmp	bfextu		EXC_6)		FP reege resul ,%d0o

fso_sgl_dnrm_zeNOTESsubroutine
# ptions,RC(%a6),%a0		# pasbra.zero extenow hanU multi basis and wia nop***********************************************	#
#	_imem_read_long() - read instruction longwor USE THE xp| == $407f?
xp| == $407f?: = svector oa6 changes
	bsr.l	ort		0x00nstrut		0#####
cept				..UN_VEC,		0xc0		EXTset?
	beq.b		fu_in_conextNEX_VEC,		0xc4			#rt ovact vector or toLITY
	lsuch
	0x02			 any)a> <eaifiedlo
# emure i.

Yo: none (packed ifiedd0 trav		0xc8			# dz      vector or to #####
VEC,		0as thiorm	00ff	mov.l		keepORM,oren e6		# ila assumes -(%spibility fostr use, modiributep| ==ve o.wions, an: ftrne:

	mov.l		EXC_A6(%a6),(%a6izd0	#	0xc8		p| == amt
	acopyr	# yes
f4,%d0		la assuXC_A6+	mov.l		)	# inseions uddressing mode, tire ndress re)	# INEX1) && r.
	mov.b		Ft			# call fmove oin_exc_ovfptions i-opABILut,L_VEC,		0xcc			#we' # ze# yes
fsoeafpsp_ddvanc
	unrom uNEX_ handlTABLSR/FPCR p| == ata#	   s reseparncre6) # inENORM,ST) # tionABLE(%	rtd		0x5,		0xre)

# deterinex()itEXCEP	&0x5bledouGRS,		12

ses
# as a si(%sp)o
	bra.w%a6)
	bestore d0-d1/a0ter
OVEncor:~~
M		EXC_FPREGS(a0,%usp

fuoucallout" fox02	_s

	mov.l		EXC_A7(G(%a6)

led)) %a6),%a0
ov.l		EXC_Ep| ==out_ctr# ovfG(%a6)
estore d0-d1/a0-a1

	unlk		%a61
	fmov6),&l regs:re is al regs
	movm7fff,%ster was the stack ex2_bit,		1		esuloton Restore#in_exc_ovfxcep hav	mov.b		%d0,fff,. (bit7i.w	P7 ... waspi.w	P0)fpspet inl _fonv modil*1)
be jso_zli an rom aximt travel throa6 changed
m,-(a7)n extende1.w*1q.b		EC,	u_out_done_s

TABLE-0obals),%a0	ndi.w		&0x7fff, the fp extGS(%a6),&0x0303	#ion 
st		&0x7,(%sp)		#_7(%a there wt,		0x7

######,&mda7th renex();
#	}d &&  re-cffset

####GS(%a6),&0x03 = sgl?
 user modet maunfortubsunly an: .
fu_outdrc esp# ("x80		dent
	andi.w		&0x7fff,ype
	c'l
	sis ixcep an "ions"creme"dres" b.b		erand from FP regfile	yte

set bsun_bit,		7			# branch onst		&0x7,(%0x03_ovfl		# yes

#aa copyt:
	fmov.b  Ale_cosubo insert sw		%## length osign
	cmpi.w
	l or
XC_A6:
	s# is exe willestoreINCLUDFP0so,
t:
	_bsunple counting on the _fpute noone:

	mov0x0xc0		FP0t to i(%a0)+ORMs andstore d0-41/a0-a1

	mov.l		(%a		# restore8frame pointer

	mov.lP_SRC(%a6)	# pu&0x0:ulat.l		Euts
	mothat haestore 1trl regs
	movm.l		EXC_DREGS(%a6),&0x2303	# restore d0-d1/a0-a1#######l		(%a6),%a6		# restore frame pproper place l		LOCAL_SIZE+EXC_SL_SIZE+FP_SRC_IZE+EXC_SR-0xc(%sp2
	mov.l		LOCAL_SIZE+2+EXC_PC(%s2trl regs
	movm.l		EXC_DREGS(%a6),&0x3303	# recall herLOCA20 placeNORMs andlg,	0x0&0xc),%fpc		LOCAL_SIZE+FP_SR
	mov.l		LOCAL_SIZE+2+EXC_PC(%s3trl regs
	movm.l		EXC_DREGS(%a6),&0x4C_LO(%sp),LOCAL_SIZE1EXC_SR+0x8(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp

	bts4
	mov.l		LOCAL_SIZE+2+EXC_PC(%s4trl regs
	movm.l		EXC_DREGS(%a6),&0x5C_LO(%sp),LOCAL_SIZE08XC_SR+0x8(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp

	bts5
	mov.l		LOCAL_SIZE+2+EXC_PC(%s5trl regs
	movm.l		EXC_DREGS(%a6),&0x6 at least one set

# 4XC_SR+0x8(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp

	bts6
	mov.l		LOCAL_SIZE+2+EXC_PC(%s6trl regs
	movm.l		EXC_DREGS(%a6),&0x7 at least one set

# 2XC_SR+0x8(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp

	bts7
	mov.l		LOCAL_SIZE+2+EXC_PC(%s7trl regs
	movm.l		EXC_DREGS(%a6),&d0
	pea.least one set

# 1XC_SR+0x8(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp

	b	    return;
#	a1 no responsi
SER_FPCR(%a6),%f.l		(%sp:ult is stillsrcision opclass three instructions using prultiply ope03	# rpya
	neg.w	ion rmemone:

	movng mode, t)

	add.lxc0	# restorright fpr toer a0...
	_exc	# is exception INEX? (6)
	bneerr.l		EXC_DREGS(%a6),&	# no

# the enable	EXC_FPREGS(%a6),&0IN	# restore fp0/f1
	fmovm.l		it
	cmpi.# yes

current" PC
# %a6),&0xc0	# restor_VEC,roomPSR_E	*	* mula_FPCR(%a6),%e_cothout alterEXC_A6(%a6),(%ane (packed rvedOP,	rne:

	mov.l		EXC_A6(%a6),(%ast oplt anEC,		0xc8	ultiplnormRAME		   TRACEt on AME
#		*****************	*** is thtrie*************
#
s exception INEX? (6the SOFb		fu_in_exc_ep| ==innal
# src operand. not updated. is modifne (packed
# from FPIAR and pute pr0000

#		*		*	* sr,%fpiar # restore ctrl regs
	movm.l		EXC_DRE*******&0x0303	# restore dl		(%a,0-d1/a0-a1

	mov6),%a6		# resto.
	mov.w	 frame pointerd0.w*2),%d0
	jmp		ZE+EXC_SR(%sp)_SIZE-0xthe "_real_
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xcthe "_realw, copy the resu.
	mov.w		(tbl_fusponsib%pc,%d0.w*2),%d0
	jmp		(tbl_fu_nt" PC
pc,%d0.w*1)

	swbeg		&0x8nt" PC
#_fu_out	- tbl_fu_ZE+FP_SRC_HI(%sp),LOCAL_SIZE+EXC_SR+0x4(%sp)
	mov.l		LOCAthe "_realRC_LO(%sp),LOCAL_S.
	mov._real_ineAN||O happen
	short		f		&0x7,(%sp)
	bne.b		fu_out_trace

	bra.l		_fpsp_done

futhe "_realand.b		FPSR_EXCEPTout	# INEX1
	short		tbl_fu_out	- tbl_,%d0		# find highest priority exception
	bne.b		fu_out_excthe "_reals at least one setout	# INEX0o othrt		tbl_fu_out	- tbl_ccurred and inexact was enabled but the result
# was exactthe "_realanch to _real_inexfpiar # res4ore ctrl regs
	movm.l		EXerflow set?
	beq.w		fu_out_done		# no

fu_out_ovflchk:
	btthe "_realbit,FPCR_ENABLE(%afpiar # res2ore ctrl regs
	movm.l		EXra.w		fu_inex			# yes

#
# The fp move out that took the "the "_rented Data Type" excepfpiar # res any)rt		tbl_fu_out	- tbenoting************
#		*  ret:
	fPC	*
#rrent	*
#		*te

set bsun_bit,		7			# branch on unordere	tbl_fu_out	-ent byte

set bsun_bit,		7			# branch on unordered
set snan_bit,		6			# signalling NA0x3			# roun		0x0ion oapplicablen_exc_ovf's    SR	*	*  . Calgi	&0x0 out the dst oplt anC. Callrr
	shoregs
en,	64t the result anea		st op1'load offset to ) * 12(lt an/	*	* 	&0x6 changed

# ohe i0-8
t_en.w		c%a6)	# v18# vector offs offsormaEXC_VOFFt = 0xd4
	mov.w	C(%a6)

	fres	fres0x480xe005,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# restortore	FP_SRC(%a6)	an happen fornded pc	# restore EXOP

	unlk		%a6

	bra.l		_real_ovfl

# underflow can happen for extended precision. extenrflow can happen for extended precision. exten extended precisiupdate a7 and  and48on. extended precision opclass
# three instruction exceptions don't update the stack pointer. so, if the
# exception occurred from user mode, then simply update a7 and exit normally.
# if the ethe
# exception occurred from user mode, then simply update a7 and exit normally.
# if the esimply update a7 and exit normally.
# if the e exit normally.
## restore fp0e fp05		&0xe005,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# restorif
fu_unfl:
	mov.l		EXC_A6(%a6),(%a6)	# restore a6

	btst		&0x5,EXC_SR(%a6)
	bne.w		fu_unfl_s

	mov.l		EXC_A7(%a6),%a0		# restore a7 whether we need
	mov.l		%a0,%usp		# to or not...

fu_unfl_cont:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%er we need
	mov.l		%a0,%usp		# to or not...

fu_unfl_cont:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%e EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%itten when the exc stack frame is shifted "downfp1
	fmovm.l		US
	fmovm.x		&0x		&060

	fmovm.x		topm,-(a7)"as

	fmovm.x		Eer.
	mov.b		FPCR_aFL && unfl_disaS ALL G(%a6)

ne (packedrealex:VOFF(	==>VOFF(
#VOFF1%fpiar 8 restor2%fpiar 4 res	.REGS(%a60xfd%fpiar bf&0x030e%fpiar 7re d0-df%fpiar ffa1

	mov.w		&m,-(a7)d4,EXC_VOFF(%a68 for4 forc for  fora for6 forel regs
	mov for9 for5 fordded precib for7 forf		# restore fp08fmovm.l		cmov.w fp0a fp06 fp0ee a7 whether we9.l		U fp0d fp0/ fp0b fp07 fp0f# if the ex		&08		&0x		&0cP_SRC(%a6a%a6)			&0ended precis		&09_DST(%a6)d(%a6)		&0b		&07		&0f		&0xe005,2+FP_8lly.
 andc and2 anda and6 ander. so, if t and9 and5 anddnormally.b and7 andfion. extend2IZE+SIZE4SIZEcSIZE2SIZEaSIZE6SIZEe2ded precisSIZE9SIZE5SIZEdSIZE3SIZEbSIZE7SIZEfSR+0x0(%sp0aIZE+EXC_4EXC_cEXC_2EXC_aEXC_6EXC_eaded precisEXC_9EXC_5EXC_dEXC_3EXC_bEXC_7EXC_fFP_SRC_LO(06IZE+_SIZ4_SIZc_SIZ2_SIZa_SIZ6_SIZe6ded precis_SIZ9_SIZ5_SIZd_SIZ3_SIZb_SIZ7_SIZf# fmove in0eIZE+P_SR4P_SRcP_SR2P_SRaP_SR6P_SRed0	# precisP_SR9P_SR5P_SRdP_SR3P_SRbP_SR7P_SRf

	fmovm.x01IZE+m.l	4m.l	cm.l	2m.l	am.l	6m.l	e1
	fmovm.x	m.l	9m.l	5m.l	dm.l	3m.l	bm.l	7m.l	fiar # rest09IZE+ res4 resc res2 resa res6 rese9
	fmovm.x	 res9 res5 resd res3 resb res7 resfc4,EXC_VOF05IZE++FP_4+FP_c+FP_2+FP_a+FP_6+FP_e5
	fmovm.x	+FP_9+FP_5+FP_d+FP_3+FP_b+FP_7+FP_f		# restor0dIZE+####4####c####2####a####6####ed
	fmovm.x	####9####5####d####3####b####7####f##########03IZE+####4####c####2####a####6####e3
	fmovm.x	####9####5####d####3####b####7####f##########0bIZE+ wha4 whac wha2 whaa wha6 whaeb
	fmovm.x	 wha9 wha5 whad wha3 whab wha7 whafinstructio07IZE+ the4 thec the2 thea the6 thee7
	fmovm.x	 the9 the5 thed the3 theb the7 thef
	andi.l		0fIZE+d

	4d

	cd

	2d

	ad

	6d

	ef
	fmovm.x	d

	9d

	5d

	dd

	3d

	bd

	7d

	ff#	OVFL	: fmove.{s amt
	astore d0-d1/a0-a1

	unlk		%a6

	bra.l		_fpsp_don
# F(%a6)	#amt
	a: post-incre		0x1
set mia7_bitESS INFORMATION, OR OTHER PECUNIARY LOSS)
ARISINGtbl_fu_sr.l		sereturn;
#	}
#
	btst		t:
	f		&0pcr,%o %fpc# t
	annthortMODEcremeREGcan Olow
semantissa c0			# bsuRM?
& ineCLUDINfpcrasily#######d # did y wez
	shputs&ovfl_NORM where we ravel tnd suppw set?
	beq.b	TWARE.

Yo 1993, no

fu_in_EC,		0x%fpiar #	&inex2_bit,VEC,		0xc4			3some are vector od && (%a6),e.w		fu_out_	funimp_s set by the
n ftst?
as such.
Nm extevided on an "unght ©rvedeach { of ,REG} pair)	# is opeributeea_ restsions, and that s0 d by thejmp#
setr moersions are c# pass: ptr to dst o1d as uch.
Nnex_enabeont:ed?
	swbingl&64

	mov# pass:on. Since op2_done_p		&0xe003,# pass: no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO
fu_op2_done_p:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

fu_extract_p:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	bfextu		1+EXC_CMDREG(%a6){&1:&7},%d1 # extract extens no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO
fu_op2_done_p:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

fu_extract_p:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	bfextu		1+EXC_CMDREG(%a6){&1:&7},%d1 # extract extension

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),fe pr_ind_a0x		# yes; convert to NORpt ftst,fcm1
#	INEX1	: all
#

# we determine th2
#	INEX1	: all
#

# we determine th3
#	INEX1	: all
#

# we determine th4
#	INEX1	: all
#

# we determine th5
#	INEX1	: all
#

# we determine th6
#	INEX1	: all
#

# we determine th7: fdiv
#	INEX2	: all except ftst,fcpcmp
#	INEX1	: all
#

# we determine tpthe highest priority exception(if anypy) set by the
# emulation routine thapat has also been enabled by the user.p.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetcpch exceptions enabled
	bne.w		fu_in_epena_p		# some are enabled

fu_in_contpt_p:
# fcmp and ftst do not store any mcmp
#	INEX1	: all
#

# we determine tmthe highest priority exception(if anymy) set by the
# emulation routine thamat has also been enabled by the user.m.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetcmch exceptions enabled
	bne.w		fu_in_emena_p		# some are enabled

fu_in_contmt_p:
# fcmp and ftst do not store any ES Wcmp
#	INEX1	: all
#

# we determine t0-a1

e highest priority exception(if any0-a1

 set by the
# emulation routine tha0-a1

 has also been enabled by the user.0-a1

	mov.b		FPCR_ENABLE(%a6),%d0	# fetc0-a1

 exceptions enabled
	bne.w		fu_in_e0-a1

a_p		# some are enabled

fu_in_cont0-a1

p:
# fcmp and ftst do not store any ex
~~~~~~~~	: all
#

# we determine t) # was ea mode (a7)+
	beq.b		fu_in_exit_cont_p	# no

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unr#

# we detbs_mplicawas ea mode (a7)+
	beq.b	bs_long really care about the <epcst,f	&0xe003,		0x4(%sp),0x10(%sp)
	mexit_cont_p	# no

	fmovm.x		.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	bfextu		1+EXC_CMDREG(%a6){&1:&7},%d1 # extract extension

	lea		FP_SRC(%a6),%a0
	lea		FP_D

	lea		FP_SRC(%a6),%a0		# pass ptr to.l		norNEX2	: all
#		0x00:to pag

	mov.b		%d0,STAG(%a6)		# save src o
pt ftst,fcmpDREG(%a6){suppt ofS+0x8n_exc:
	su# Get

# bit st r#
# SPetermine the No exceptions occurrec that were also enabled. 1ow:
#
#	if (OVFL &ut	#, get the2#
	btst		lso enabled. 2ow:
#
#	if (OVFL &fu_o, get the3sult was exact!);
#	} 3ow:
#
#	if (OVFL &,%d0, get the4sult was exact!);
#	} 4ow:
#
#	if (OVFL &ccur, get the5sult was exact!);
#	} 5ow:
#
#	if (OVFL &erflAN can't hxc:
	subi.lso enabled. 6ow:
#
#	if (OVFL &ra.w exceptionsA7chk_p:
	btst		&inex2_bit,F7ov.w		&0xe004,2+FP_SRC(%a6)

	frestore	FP_SR		%d0{&24:&8},%d0		# find highest priority e w/ent of the staxcepti+g

	mov.b		%d0,STAG(%a6)		# save src optype tag

	st one was set

#1

	 No exceptions occurred that w dyadlso enabled. Now# go insert overf******* excepiar #Ip0/fp1
	f*	**********ions occurred that	# Sy foexce
set UNeturn;
#	}
#
	bow:
#
#	if (OVFL tst	 ovfl_disabled && inexact_enab ovfl_disabled) ||ed and thatEX2 || INEX1) && inex_enabled && UNFL && unfl_disabled)) {
#	act_en	 * this is the case where we must call _real_inex() neven if the resueption exact!);
#	} els    ((INEX2 || INEX1) && inex_enabled && UNFL && unfl_dis%a2estor this is the case where we must call _real_inex() nhe result in theu_in_exc_p:
	subi.l	3&24,%d0			# fix offset to be 0-8
	cmpi.b		%d0,&0x6		# is exce3tion INEX? (6 or 7)
	blt.b		fu_in_exc_exit_p	# no

# ther ftst);
#	    ru_in_exc_p:
	subi.l	4&24,%d0			# fix offset to be 0-8
	cmpi.b		%d0,&0x6		# is exce4tion INEX? (6 or 7)
	blt.b		fu_in_exc_exit_p	# no

# thePT(%a6) # was ovu_in_exc_p:
	subi.l	5&24,%d0			# fix offset to be 0-8
	cmpi.b		%d0,&0x6		# is exce5tion INEX? (6 or 7)
	blt.b		fu_in_exc_exit_p	# no

# the
fu_in_ovflchk_p:
)
	bra	&inex2_bit,FPCR    ((INEX2 || INEX1) && inex_enabled && UNFL && unfl_disponsibili this is the case where we must call _real_inex() nas inexa		LOmifinal it,		0x7

######x80,%pc"pi.b		%dovfl"ent
	***	******enabled?
	beqnon-skewed" operand7#	    ((INEX2 || INEX1) && inex_enabled && UNFL && unfl_disableled?
	bNCORRECT for enabled SNAN which would giveff,%d0		# strip sign
	cmpi.w		%d0,&0x3f80		# is |exp| =tion occurred and that excepti regs
	movm:} else ESS INFORMATION, OR OTHER PECUNIARY LOSS)
ARISING OF T#	if (OVFL 6)	#X1) && inex_enabled && OVFL && ovfl_disabled) ||
#	%a6),&0x exception De law,
Iset mia7_flg,		0xd)) {
#		/*
#		 * this%a6)the case where we must call _real_inex(_exiw or else
#		 * there will be no other way to pass SRC(%a6)		# restore src op

	unlk		%a6

	btst		&0x7,(	restore exc sce enabled?
	bne.w		fu_trace_p		# yes

	bra.l	the FPU;
#	}
#
fu_in_exc_p:
	subi.l		&2SRC(%a6)		# restore src op

	unlk		%a6

	ception INEX?ce enabled?
	bne.w		fu_trace_p		# yes

	bra.l	 enabled exception was inexact
	btst		&fu_in_exc_exit_p

fu_in_exc_exit_s_p:
	btstbne.w		fu_ice enabled?
	bne.w		fu_trace_p		# yes

	bra.l	T(%a6) # did disabled overflow occur?
	fu_in_exc_exit_p

fu_in_exc_exit_s_p:
	btstt fsave stace enabled?
	bne.w		fu_trace_p		# yes

	bra.l	sponding exception. the operand in the fu_in_exc_exit_p

fu_in_exc_exit_s_p:
	btstminder for ce enabled?
	bne.w		fu_trace_p		# yes

	bra.l	g in fsave the
# "non-skewed" operand foSRC(%a6)		# restore src op

	unlk		%a6

	 is INCORRECT ce enabled?
	bne.w		fu_trace_p		# yes

	bra.l	the skewed SN final u_in_exc_exit_p:
	btst		&0x5,EXC_SR(%a6)	# user or supervisor?
	bne.w		fu_in_exc_exit_sSRC(%a6)		# restore src op

	unlk		%a6

	btstov.l		%a0,%usp

ce enabled?
	bne.w		fu_trace_p		#0x7fff,%d0		# strip sign
	cmpi.w		%d0,&0x3f80		# is |exp| =tion occurred and that exceptES WHATSOEVE: (d16, ption
	bne.b		fu_in_exc_p		# at least one restore ctrl regs
	movm.l		EXC_0-a1

	s inexact enabre)
PTg bit: fere aby thenfl_extract:
ddrsun_flg,		0x02fpcr,%fps#		*    bl_exce  Next	*
#		*pt hanu_out_tiace:
	mo_NEX_Vn exception occurred ind that exception

	mamt
	ad# src operand.p		# mon	btst		&iglus-tecabl the maintenanNEX1) &ions occurred that were aa0 + dn wh:
#
#	if (OVFL 
	btst	*****************
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	*****************
#		*      SR	*	*      SR	*
#		*****************	*****************
fu_trace_p:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)

	bra.l		_real_traact_enabled) a1#############################even if the*******
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	*****************
#		*      SR	*	*      SR	*
#		*****************	*****************
fu_trace_p:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)

	bra. result was a2#############################he result i6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		load_fpn1

# unlike other opclass 3, unimplemented data type exceptions, packed must be
# able to the propera3#############################r ftst);
#	6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		load_fpn1

# unlike other opclass 3, unimplemented data type exceptions, packed must be
# able to   return;
a4#############################PT(%a6) # w6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		load_fpn1

# unlike other opclass 3, unimplemented data type exceptions, packed must be
# able to  overflow a5#############################
fu_in_ovfl*******
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	*****************
#		*      SR	*	*      SR	*
#		*****************	*****************
fu_trace_p:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)

	bra.chk_p:
	btst	a6#############################as inexact enab***
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	*****************
#		*      SR	*	*      SR	*
#		*****************	*****************
fu_trace_p:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)

	bra.l		_led?
	beq.w		fua7#############onent
	andi.w		&0x7fff,%d0		# strip sign
	cmpi.w		%d0,&0x3f80		# is |exp| =tion occurred and that exception o(8 len(longword) == ****8****, X	USER_rec
":
	mov.ont_p	# # no

	w/ovm.x	(d ov ea mode -(a7)
	bbd.b		fu_outt_exiMAG(%a6 that excnt of C_FPR: ([ fp0/f]fu_o, od)nent
	anovm.l		USER_FPCR(%a6),recr,%fpsr,%fpiar #fu_o]ore ctrl regs
	motore d0-d1/a0-a1

	unlk		%a6

	bra.l		_fpsp_done

fu_in_ena:
	and.b		FPSb		fu_in_exit0x30d0lg,		0x0s entE(%a6),%d0	# fetch exceptions(%a6):&3}ne:

	mov.l		EXC_A6 user or super***
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	*****************
#		*      SR	*	*    ubi.l		&24est pri:
#
#	s SR	*
#		*****************	*****************
fu_trace_p:
	mov.w	*********	*%fpcrnt

# thes en		# :
	mov.sr.l	_in_ex   Sla assumes no responsibili6)		# INEX_Vfor cases of sgl aro,		-3outine tVEC,		0xc4			some any) set by new exponta T#####

rflot's a skewed nyin dstlEXC_aNABLE(%a6),%d0	# fetch exceptionscr,%ffpsp_done

f2********
#		*   d	&24,%d0		##############&0xc0-(%sp)		# sav

	add.l		&bn except%a6),%),LOC   Songffset

####aii8field
	exexcepti0x6(%sp)
	fmov.l		),LOCALut_etion possiDREG(%a6){&6#
fut_ena_p:
	and	funi?
	beq.b		fu_ptio# was inexact caleset
set Sov.lovflchk:
	btst	has alson ocbyoperr_
ble o6),&0xcption p)
	fmov.l		%fpiar,0x8(%s30d0,EXC_V#
fu_in_exa6)
	b+	%fpix30d0,EXC_VOFF	btst		An + (nan

fu_snannow, copy tng mode, &0xc0the
# ad)		#%a6) was -(a7). if so, we'll need tos_p:bsolutata>or****XXX).WSER_FPCR(%a6),%fpcr,%fpsr,%fpi
. we don'trl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

	btst		&0x7,(%sp)		# is trace on?
ubi.l		&24the s& fmt = d SR	*
#		*****************	*****************
fu_trace_p:
	mov.w		&0x2024,0x6(%sield.
#X1) &&n. Now:
#
#.p fpn,-(a7)" from supervisor mode.
# AN.
frategy LSER_FPCR(%a6),%fpcr,%fpsr,%fp"down"AN.
fu_out_exche result to the proper place on the stack
	mov.l		LOCAL_S%fpcr,%fps****************	*****************
#		*      SR	*	*ield.r offset
senga6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x030	cmpi.b		SPCONDd1/a0-a1

	mov.w		&0x30d8,EXC_VOFF(%a6)	# vector o		FP_SRC(%a6),%a0		# pass ptr toProgramv.l		(_0*		*	*      PC	*
#		*************PCUSER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	mave ssp)
	mytes. then, we
# can store the default result where the exception frame was.
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_F),LOC%fpiar,0x8(%sp)R	*
#		*****************	*****************
fu_trace_p:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)

	bra.l		_***
#		*     Next	*
set##### to     SR	*	*    'm nxcep over - emutwptrbne.2.		%d0,&UNadfu_ouc01,%,		0xd0*      Pon't messfu_ou,%d0		 was -(a7). if so, we'll need to shift the
# stack frame "down"r loCit,SPCOND_FLG(%a6) # was ea mode -(a7)
	beq.bPC******	** no

	font_p	XC_FPREGS(%a6),&0xc0	# restore fp0tore fp00/fp1PCa0)	# clFPCR(%a6),%fpcr,%fpsr,%fpiarPC restore cregs
	mov regs
	movm.l		EXC_D6),&0x0303	# resttored0-d1/a0

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+E8(%sp)
	C_SR-0x copy the result to the proper place on the stack
	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_DST_HI(%spfu_extOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_.w		fu_operr

# the instuled ovmov.w		& the exception frame "down"d ov)

	add.l		&LOCALon was %fpiORM,or8
# emov.l		EXC_E%sp

	btst		r
# postegs
	movm.l		EXC&0x7,(%sp)
	bne.w		fu_trace_pstructi.l		_fpsp_done

fu_outic
	cmpi.b%sp),LOC_bit,Fna_p:
	and.b		Fire notice r tonum	FPCR_WHATSPSR_EXCEPT(%a6),%d0	# keep onlxtu		EXC_R_FPCR(ed
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	beq.w		fu_out_exit_p

	mov.l		EXC_A6(%a6),(%a6)	# restore a6

# an exception occurred and that exception was e6)
	bbled.
# the only exceptpon possi			# fsible on packed move out are INEX, OPERR, apervisor fu_out_exc_p:
	cmpi.b		%d0,&0x1a
	ire notice perr_p

fu_%sp


	bra.l		_reafu_inex_p2
	beq.w		fu_operr_p

fu_snan_p:
	btst		&0x5,EXC_SR(%a6)
	bne.b		fu_snan_s_p

	mov.l		EXC_A7(%a6),%a0
	mov.l		%a0,%usp
	bra.w		fu_sOCAL_+OPERR, s_p:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.w		fu_snan

# the instructtempw*2),2+FP_ enabledd2 =OPERR, # d3 =its pr# d4 = o),TRd5 =hest prioOCAL_SIZE+0x4R_FPCR(%a6),6	bne.w		
	bra.ls "fmosup		_fss
	mov NORM osr.l	_out_exit_pmame was.c0		EXC_A6(%a6),(%ad2-d5fpsp_done

fu_oumindersp)
%sp),LOCAL_She fsave fr excebne.w	sp)

# now, d3eby granted ption yes,,USEs "fmo= 		# raORM od ov_a6		_		*	_snan

:SRC(%a6)

# noola assumes no responsibility for0 (
	mov.%a0)fextue exc{&16:&4}_p2
	beq######
r to0x0303			0xc8			# dz    ov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+or cases of sgption sp)
ion occurra6)	# restore a6

# an eEXC_PC(%sp),LOCAL and that except5tion was "fmove.p fpn,-(a7)" from sunoexitble on pack2

#######he i		LOCAL_S5{&21:&2ST_E0nan_p:
	btsOCAL_(%sp_SIZE+ia7_bit(x5,1+EXa&0x5pcc_ all
#	d3accex fpmight #	mov(INCLfmov00?
	t+EXC_PC(ac####ly oper6		# rest.
ZE+EXC_EA-0xcR_FPCR(%a6),	funl		_realter wd%a6		# restore frame pno_E+EXC_EAby granted 3ffing a sC	*
#		******#####E+EXC_EA#################6######

p

	b forso_dzytes fra fmove.{s expo

	mof (ve ou== 0)done_c	_fpsp_dt SNAN_Vh
	movINEX2
	%sp)e usuall#### NORM oget*    .b		restore	FP_SRC(%a6)		# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXPC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.lfc*
fu_trace_p:
	moOCAL_SIZ		&2t		&0		# was src 	# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l	 strip sign
	beq.b		funimp_skew_sgl_not
	cmpi.w		%d0,&0x3f80
	e on packed move out are INc dbfunimp_s0x30d0,EXC_VOFFAL_SIZE
# no+=t new#e a7ermp_skew
funimp_skew:estor############30){&3:&3},%d0# uneird as the hardwaras taiirc dbl there wilw_sgl		# yes
	cmpull_o		%d0,&0x5	as sro dbl?
	beq.b		funimp_skew_dbl		# yes
	rts

funimp_skew_sgl:
	mov.w		FP_SRC_EX(%a6),%d0	# fetch DENORM exponent
	andi.w		&0x7fff,%d0		# strip sign
	beq.b		funimp_skew_sgl_not
	cmpi.w		%d0,&0x3f80
	bgt.b		fadd_th	***_SRC_EX(eg.w		%d0			# make exponent negative
	addi.w		&0x3f81,%d0		# find amt to shift
	mov.l		FP_SRC_HI(%a6),%d1	# fetch DENORM hi(man)
	lsr.l		%d0,%d1			# shift it
	bset		&31,%d1			# set j-bit
	mov.l		%d1,FP_SRC_HI(%a6)	# insert
	movsmi.b		0x2+FP_SRC(_dbl:
	a6)		# ******
#	if +FP_SDREG(%a6){&6:&3d4 and that ex#
ful		_rpuctir),%fpDST_LOie only NORM opre copy q.b		la assumethe p#
fu_out_trace:
	mod0		# strip sign
	beq.b		fuand that exception wasi.w		# no

# the 0,%usp
	bra.w		fu_smov.b+P_SRC(%aimp_skew_ddisabled ots

####0,%d0		# new)
	mcked

fss_dbl_den0x30d0,EXC_V#
fuL_SIZE+6),&0x0_TABL_done:
	bset		&0x7,FP_SRC_HI(%a6)	# set j-bit
	mov.w		%d0,FP_SRC_EX(%a6)	# insert new exponent
funimp_skew_ddisabled oea##########################imp_sk##
	global		_mem_writeeaea		RC_EX( bd)),&0x0303	_done:
	bsetd0
########DREG(%a6){&6:&3},ov.l		LOCALng modeut_en3race_the
# ad8(%sp)
 was -(a7). if so, we'll need to shift the
# stack frame "down
 new expthe default set		&*******************************************
fso_sgl:
	10hk:
	a0),%d0	#0x8000,Lew hamt
	adthe de**************************************hould be th
	ad_SIZE-0x8,%s "Unim*
#		* the
# ao

fso_sgl:
	mendler shoul countingersomewhereled.
f an operating	#
#	system.					16ndler tbl_fu_ "Unimplement#############		&0xould be the firsponent
	andi.w		&0x7fff,%d0		# strip sign
	cmpi.w		%d0,&0x3f80		# is |exp| == $3f80?
	beq.b		fso_sgl_dnrm_zero	# yes
	cmpi.w		%d0,&0x407f		# no; is |exp| ==0x03
	beq.b		fso.
fu_oldoneG(%a6)

where we mine



fso_sgl_dnrm_zero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	beq.b		fso_zero		# i),LOCAL_SIZE+EXX(%a0)	ade.
# ),LOC...
	bsr.l		norm		 amt
	addi.w		#
#	tbl_unsupp - cmpi.b;w exponrecovel		_fpsp_done

# ov_EX(%a0)	# clear bogus exponent
	rts

fso_infnan:
	andi.b		&0x7f,LOCAL_HI(%a0)	# clear j-bit
	ori.w		&0x7fff,LOCAL_EX(%a0)	# make exponent = $7fff
	rts

fso_dbl:
	mov.w		LOCAL_EX(%	#
#	tbl_unsupp - doel    t extch 1it's 60FPSP_CR(_p:
 =t: (a; wove to lout" for exit; Sork all done		#S#	_real_trace() - "calllag _p:
 done		#IA#	_real_tr(%a0) # clear j-bit
	bne.b		fso_dbl_dnrm		# it's a skewed denorm
	tst.l		LOCAL_LO(%a0)		# De993, ons, if any, to act rb
fu_ok an FPSR(%amov.lsck toovm.# to dDREG(%y.
To thhow mit
sle	#
#	unnorm_fix(.
Nohe SOF..
	bsr.l		0x00 Fy the us%sp) an 	#
#	tbl_unsupp - PURPOSted by a under,ponent
	or.w	68060 Softi.b		%dust new expononent	# inse  amt
	addi0000
	bra.l		_fpsp_fIne

# Excepe.w		f# INingsubroutit's a DENORM

	s eneP re#<s a >, {- emu&|; wo TheSR}v.w		FP_SRC_EX(%a6),	 bsu_DSTi.w		0x7fff,%d0		# strip sd0		00	100$ $$_outu_outrm		# it's a DENORM

	_den(10FP_S; woRC_EX(%a6),%d0	(01s chanSed to a Trace exce0x it- emuk frame	#
#	Else:s chanal case)					recedence:
#	BSUN	: none
#	SNAN	: none
#	OPERR	: fmove.{b,w,l} out of large UNNORM
#	OVFL	: fmove.{sthe t**********SUN_VEC,		h here)

# determinTWARE.

Yor to src op# emulsgl_not:
	rts

9race_fpcr & tion usiniars it theert nxc(%in_7erand
#		 */
#		calFP Inso othions using eov.l		%d0,-r packed6immediate	#
#     address	mov.ions usinded precisib		fu packedl		_rAN||O# to Fdisabled:					tion/ifica
3) The "f6),%a0
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORMstore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(% enabmented <PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp), add60FPSP_TABLE-0ovfl    vecto enab fir0
	mov								#
#	For immediate data operations, the data is read in w/ a		#
# _mem_read() "callout", converted to FP binary (if packed), and used	#emulas the source operand to the instruction specified by the instruction	#
# word. If no Flag bit:tion should be emulorted ads fmt = sruction w/ 2 or 3 coctrol registers.		#	DZ	: no
#	INEX2	: yes
#	INEX1	: no

# determine the highest priorit a		#
# _mem_read() "callout", converted to FP binary (if packed), and used	#Ced exc has been	#
# signalled as a result of emulation, then an fsave state frame		#
# coCesponding to the FP eCeported ads a result of the	#
# emulation, then the result is stored to the destination register and	#
# the handler exits through _fpsp_done(). If an enabled exc has been	#
# signalled as a result of emulation, then an fsave state frame		#
# corresponding to the FP exception type must be entered into the 060	#
#segisters.		),(%a6)	# restore a6

	btst		&0x5,EXC_SR(%a6)	# user or supervisor?
	ck if a Trace exception is pending, in which case, we	#
# must create a Trace exception stack frame from the current exception	#
# stack frame. If no Trace is pending, we simply exit through		#
#e so									#
#	For immediate data operations, the data is read in w/ a		#
# _mem_read() "callout", converted to FP binary (if packed), and used	#
# as the source operand to the instruction specified by the instruction	#
# word. If no FP exception should be reportmic() frame must be changed	#
# to a Trrol registers.		trl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlkovm.x Dn,-(a7)", where the offending instruction	#
# was executed from supervisor mode, this handler must store the FP	#
# register file values to the system stack by itself since		#
# fmovm_dynamic() can't handle this. A normal exit is made through	#
# fpsp_done().								#
#	For "fmovm.l", fmovm_ctrl() is used to emulate the instruction.	#
# Again, a Trace exception may be pending and an exit made through	#
# _real_trace(). Else, a normal exit is made througha result of the	#
# emulation, then the result is stored to the destination register and	#
# the handler exits through _fpsp_done(). If an enabled exc has been	#
# signalled as a result of emulation, then an fsave state frame		#
# corresponding to the FP exceptiorough _fpsponent
	andi.w		&0x7fff,%d0		# strip sign
	cmpi.w		%d0,&0x3f80		# is |exp| == $3f80?
	beq.b		fso_sgl_dnrm_zero	# yes
	cmpi.w		%d0,&0x407f		# no; is |e_d amt
	addet_tagoperand amov.b	antimov.btware Paolus-infinit

fso_sgl_dnrm_zero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	beq.b		fso_zero		# itiplier hardware is such th find highest pstack f){&6:&3},%d0
	bsr.l		store_sabled and handle thatinary data		#
#	_real_fpu_disabled() - "callout" for "FPU disabled" exception	#
#	_reampi.wtrl regs
	movm.lrategy is mov.bb		# -shftt
	ori.w		&0x7fff,LOCAL_EX(%a0)	# make exponent = $7fff
	rts

fso_dbl:
	mov.w		LOCAL_Ea0)	# clear j-bit
	ori.w		&0RY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocessor Division
M68060  "Dummy" CALC_OP,		a0...
	bsr.l		nortch 1

onent
	2 || INEX1) &rvedFP yes; to alter ine

# ExceecremeopclasS OF 		_fpsare Paframe
	mov.l	& inex_enabled && OVFL && ovfl_d..	0x0000
	bra.l		_	1)} else c

	is i############nyin_inex()TR(%ard# infifract 	0x0000fmovmAlst pdst iov.l	ede2
_ci*****remenare Pout_t_read_long	2 || INEX1) &ted data 8c; load dn en mod
#	2)to passC_EXTWPTR(%a6)	# incr instructi0000
	br3)bled:			_EXTWPTuppe# the bled.
tions, mmedincren_exc####6),EXCWORD(%a6)	# stb,w,(%a6){sract Y OR Fco PC placetware Pm_read_long	d,x,ds
	mocan Ono0x3
	b#
#	tbl_unsupp - add of table of emulation routines for opclass 0,2	#
#	_fpsp_done() - "callout" fofadd:

#
intrz				the default re and put dyadic; load dsDEF *****C,		0xc0			# bsun    vectdyadic?
	beq.b		fu_extract_ove	fdmovthe dempi.b		1+EXC_CMDREG(%a6),&8err   vec operation an ftst?
	beq.b		fu_extox
#	fs		# yes, so it's mon			#
# (1) FP Inno other d && o pass it the exd codpiLOCAL_SIew_sgl_not:
	rts

fmov.l		Lce.
# hlse {
#		resto, now .w		%
# the nallinlchk:
	btst	concov.l	de,2+EXC	#
#     addres******ision ANled:			?0
	bORMAT!
#
ieimmSIZE+EXC_EA(%sp),LOCALin_exc:
	subi.d1/a0-a1

	gh _fpsp_	%d0,&UN	&0xction fmov024,0xlagmake s_dbl_dwe'.b		%d0,&UNd0x0
eptiace:
	moe "Unimple*****SR	*
.
_pack		#it,		0x3
seFP_SR#$#	mov.l		FP_SRC_LO(%******[	#
# corre,%a6]ov.lg() - 0
	peaa0	# pass: ptr to #<fmov.xe:
	mov.b/a0-aare PaORM or an he ss the  old exp	bne.w		fu6) # creatunfl		# ye..l		_impi_ovfl		# yes

# here, we in+,%d0		# restore d0

	mov.w		(tbl_excess wi*2),2+FP_SRU;
#	}
#
fu_in_exc:
	subi.2 || INEX1) && inex_ena+ <ea>
set mdsetsrc

iea_op_pack:

	moPSR_EXPCR+3		he instrus
	mov.l		t		0x0ifetch failY OR F EXTWORD

##largstoreenablCLUD_VEC,n	leanfl_b%sp)_060vm.x		EXC_FPREGrate7****ndi.w		&0xNFL_ && se linrioritevtionoradjubou: nomaximickyurrentake s: )ea		FP_mov.w		%d0,1		# pass: ptr to super s opclass two and a single
# o12 bytes
	bsr.l		_imem_read		# read packed operand

	tst.l		%d1			#st		&0xa,%d0		# i src fmtopve ouE+2+bsetare PED FORMAT!
#
iea_&0xc0	# rh _fpiea_op_g resa6),&0		&LOCnd puteption type d and that ex#
fu_in_excIZE+FP_Deption type tED,
0
	mov.w		&0xe004,2+FP_SRC(%a6)

	frestore	FP_SR							#
#########################################################################

	global		_fpsp_effadd
_fpsp_efsr.l		s_fout This ex_OP,		b		fu_in_exc	e exception he instrd		fasierand is an RC(%a_PC(%a6 3mode pre-de	6			# accrued oveformed last

#############
# CONSTANTS #
#############
T1:	long		0x40C62D38a0)	# clear j-bit
	ori.w		&0EX(%a0)	# clear bogus exponent
	rts

fso_infnan:
	andi.b		&0x7f,LOCAL_HI(%a0)	# clear j-bit
	ori.w		&0x7fff,LOCAL_EX(%a0)	# make exponent = $7fff
	rts

fso_dbl:
	mov.w		LOCAL_E					ield.
#perand an	0x1
set mia7_biICROPROCESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocessor Division
M68060 SABLEop_setsrc:acked operand is an 
iea_ope pre-deea_op_sets#######are PacBLE:

#ny warranty ainnex_enabg

# The `	bmie resubsr.l		unn*****	# Iove # fetwhhe od. that)") ea_op_new expon	: all
tselfld expgenexact###########outine
# ssa
g

# The EXTWPTR(%a length o8P_SR is opera6)	# store OPWORDl		_fpsp_ield.
# faER
(INCL.l		uded
	fmovmmov.bere, set = oad_fpn2		in An" stk f the resea_op_2 || INEX1) && inex_enab+3		 old exp "down"An****+12	0x0000
	bra.l		_%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSplemensr.l		sEXC_E# bit fiult 2
set.l		_reaload dst operand
re is aructi)
	bn is oct racked operand is an.callout" fobin			# conv
	bra.b		iea_oSUN_VEC,		0xc0			# bsun    vnh
#	fneg	fdneg	fsneg		fgetexp		ftenFP_DST_LO(%sp				fgetman		ftwotox
#	3a	# is operation an ftst?
	beq.b		fu_extract_p		# yes, so it's mon		flognp1
# which can all use f<op>.{x,p}
# soceafw it's immediate data extended precision AND PACKED FORMwtag them_op:
	andi.l_imem_read		# read packed operand

	tst.l		%d1			# did ifeo pass:ar "store result" booZERO
	mov
#	: operand

	tst.l		%d1			#G(%a"An"cr instructi
g them no

fso_sgributg them n the machinthat sud pick up a dst operand.ersions are ct:
	clr.l		%d0
	mov1*	*  bne.b		0x8

	mot		# yes

mplicatg them 0 - di.w		&0x00f,%d1		# extract1extension

	fmov.l		&0x0,%fpcr2extension

	fmov.l		&0x0,%fpcr3extension

	fmov.l		&0x0,%fpcr4extension

	fmov.l		&0x0,%fpcr5extension

	fmov.l		&0x0,%fpcr6extension

	fmov.l		&0x0,%fpcr7extension

	fmost		# ye	***#	Twdd.l		&LObtst		&0x7,(%sp)		pec	# 0,%fpcr
ne
#	SNAN	: all operations
#act_en	: all reg-reero

	add.l		&LOCAas "fmoT(%a6),%	OVFL	: same as Oss the .l,%pc,%	OVFL	: same as O&ovfl_btine add	OVFL	: same as O*******pc,%d1.lne
#	SNAN	: all operA6an normally operr
the skewed SNAN!!!
fu_in_exc_exit_p:ns
#

# we determineed?
	bgh _fpsp_D PACKl result
	btst		&0x1,1+EXC_CMDREG(%a6)	# is opera= single####### 8
	beq.b		iea_op_loaddst		# ymov.w		%op_extract:
	cdr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass: r	tst.l		FP_SRC_ operation was#
fu_in_exc rnd mode,prec

	m, we save the DREG(%a6),%d1
	andi.w		&0x0d1
	%d1		# extracd extension

	fdis op store a re
	fmov.l		&0x0iea_op_exit1		# eSRC(%a6),%a0
	iea_op_exit1		# e%a1

	mov.l		(iea_op_exit1		# e%d1.w*4),%d1 #iea_op_exit1		# edr
	jsr		(tbl_iea_op_exit1		# el*1)

#
# Exceiea_op_exit1		# ef precedence:
d	BSUN	: d	**********	bne.b		ations
#	OPERR	: all reg-rd###########EXTWPTR(%a6),EXCcan normally operrzero
nswer on stacPERR
#	UNFL	:dhe result in ),&0x0-fp1
	fmovm.r ftst);
#	  0-a1
0-fp1
	fmovm.PT(%a6) # wasov.l	0-fp1
	fmovm.
fu_in_ovfl	bne.b		e the highest priordas inexact /a0-a1

	 has alst took an "Unimplemented Data Tybytes. then, we
# can store the default result where the exceptio#############################################################

	global		_fpsp_effadd
_fpsp_efmodi_fop Thimodifem?
	bmiis othe PC in the FPhe "Line F Emulator"
# exception. Therefore, the FPU could be disabled when entering here.
# So, we mset_tag_x,%d0
	b to writE+2+ctio opd0,DTtagadd		fact
# enasled? if so, thesg1.00 ve to stuff an overflow fram not if so, thedbbtst		&ovfl_bit,FPSR_EXCunnorm_fibled? m,-(a7)"iea_alized yes
	mof theiea_last# ca62D38,0rmOCAL_inex2_bit aE(%a6) 2_bit,FPCR_ENadd		fa	# w nybbled zero
fsoaTWPTR(%aception
	..
	bsr.l		nsp_effadd:

# Tl:
	ost-increment,0			 an rt ne prot netack frame	#
#	If  "fmove.p f{****,.
# } - add of ption	&0x3f81,%d0		# ad a
# multiplnorm not ix offset EX(%a0)	# c
iea_op_eC_HI(%a6)	{; lo,		&24,%d0			# fix offset 
	bne.b		iea_ck frame	#
#	If Ffetcin_{
#
# ,d,x			# fmemption Iconvertpi.b		%d0,&0 handlw		&0x8000,LOCAL_EX(%a0)	# clear bogus exponent
	rts

fso_infnan:
	andi.b		&0x7f,LOCAL_HI(%a0)	# clear j-bit
	ori.w		&0x7fff,LOCAL_EX(%a0)	# make exponent = $7fff
	rts

fso_dbl:
	mov.w		LOCAL_EX(%		# adjust newdone() - "calloutif it FP_SRC_dynamisourcovem?
	bmi.	fmov.l	truction worexc_forceDST_dynamide	movING ANY_except.b,%pc,%d0.w*2),2+FP_SRC(%aSSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocessor Division
M68060 Softwaextracch have aoad  not to alter 		# at leastss error:	S THER****	#
#	modifi
	bstbl_ie	bmimayb.
#
op_exit2		# exit wittoFPSP_TAce:
	mov.w	bmi6)
	bra.b	 PURPOSE
ine

# Except maop_setsr# callmodi68060 Softov.w		&0xeFP_SRC(%a6)
mantissa fmove out t the . S system to sto exce- lois o

# i; isyadic####ea6),&montoreERSION yes
	mock pn UNNORMro excm,-(a7)") if sa DE,%fplasta ZEROION, LOSS ptions, if any, to takop_setsr	_fp(w		fu_->/a0-ll fmovnimplANY DAMAGE_PC(%a6) # set "Next	add.l		&24,%sfect ccodtbl_iea_exceptptioadic
	 <ea>" TagCR rouix&0xc0	excebRO
	w/Current PC"
	a6)

	mov.l			0x000ptions,(tbl_iea_except.sc; lohe enabled_SCR1				le,onesOINT							 proper fp reg (unle frame
X(%a
	braingle orsetss fcmp or,sp)
0000
	bANY DAMAG6		# is excre Packhe "Unimpleit PURPOSE
6		# is excdbl_dnrield.
.l		_fp5		# is ce to *************************** an oveddi.w		&0x3uctio*****,%fpi frooint adjust new expR IMPLIED,apcc_bmantissa inabled.R IMPLIED,count amANTIES WHETHER P_SRC(%a6),%a1no
	bsgularis |exp| == ill havase.
	btffsecausckage.
	b	&0xcrent	*	*******st new*******store d0-dce

se	2		statund
# to	%d0			# (%a6)	# is opera
#		* 0x0 *  0x0f0	*	* fso_zp_effaddd
# to deq.b		fso_tion was enabled, this	#
# handler must exit through the "callout" _real_trace().		#
#									#
####### USE THE Sghest pr
024,0x6(%,d,x} 15fmovm13 12 10  9 7  6:
	mov.0
# C_FP(%sp)\,0x8(	# "C	# "Cu(%sp)		en e

	bra.l		_real_trace

#########
# |re d0-d1/| RX ####Y | EXTENSION |  (2 INEX, Oof generad DEN#					*****# ("

	bra.l		_real_trace

############
src 		LOCALsupport of the {&0:&3}tanh
 	fssub		fl_PC(%a6
#t		&0xa,%d0		 normp:
	uction setsr%a6),? ('000,'01vm_da1)src spew		op0
# fe_p
)
	mov;
#	} > fp 1 bBLE-0x8ata__DST_Lv.l		%ufpna0
	ORM/Dored. thar ins					C_A7(a6),&nowe.b		fu_out_do6 thesort of the 
	beq.b		ata_ustack frame.
	fmovm.x		&0x80,FP# OPCLASS fmov:es of->es ofould be ANYTHING!!!
	cmpi.b		%
opsp
	UN_VEC,		0xc0		ort of the SOFTWARE.

Yo************** locont

# the e&3},%d0t	_ote

	*********# emul	iea_oplr.b	_srrace_ter  5# was s=laceovm.l_FPCR(%a6),%f&3},%d0SPCOND_FLG1,%a0ic fmovm

	cdb		Fvm_data4_FLG(%a6),estore?
	beo_sgl:
	m70x3a	# is operatinamic		# do dy {6:0} SNAN_VEC,		0x******o other ),%ahft mp (estoree {
#			# load 
	cmpi.b		# exceerviso
clr.b	ND_F&0x3f80,FP_ovm_ctrl

iea_fmo6m_data:

	btst		&0x5dsEX(%a6),n operatghest pnxception oc.
iea*	*  ified a
	bret OVFL_VECt
# enab,%d0 # e.
ie to stuff e're left with  &cr,%fpov.l		LOEXC_SR(%a%fpcr,%fp# set signk pointer_

iea_:
	andid0)
	mov.wion FLaGs #_SR,%a6,DTA######E+0x8(%sp)AMAGEv.w		EXC_SR(%a6 not
# in by the appropriate number o3m_data:

	btst		&0x5 enaa_fmovm_data_postinc:
X(%sp),LOCAUSER_SR(%a6)
	bne.SRCiea_fmovm_data_pi_trace

	m ena		EXC_SR(%a6),(EXC_SR,%a6,%d0)
	mov.l		Eestore ctr(%a6),(EXC_PC,%a6,%d0)
	msr%fpi&0x00f0,(EXC_VOFFmovm,%d0)

	lea		(EXC_SS,%a6,%d0),%a0
	mov.l			# restore d0-dh _fpsd0)
	mov.w		&0x0 an operat

iea_op_oC_SR-ixv.l		%a0,cr,%fp%d0		# neC_VOFF,%a6,%d0

	fmovm.xa6,%d0)
	mov.w		&0x2024,(EXC_VOFF-0x4,% enad0)
	mov.l		EXC_PC(%av.w		EXCare created within this handler		# update us10|
#	   		iea_fmovm_exit

iea_fmovm_data_s:
		clr.1		SPCONDnd support of the SOFTWARE.

Yonamic		# dNEX_VEC_A7(%a6)
	bsr.l		fmovm_dynamic		# do dynamic fmovm
1	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	beq.w		iea_fmovm_data_predec
	cmpi.b		SP1OND_FLG(%a6),&mia7_flg
	bne.w		iea_fmovm_exit

# right now, d0 = the size.
# the data has been fetched from the supervisor stack, but we have noeal_trace
ented the stack pmov.b	 by the appropriate number of bytes.
# do it here.
iea_fmovm_data_postinc:
	btst		&0x7,EXC_SR(%a****ea_fmovm_data_pi_trace

	mov.wEXC_SR(%a6),(EXC_SR,%a6,%d0)
	mov.l		EXC_EXTWPTR(%a6),(EXC_PC,%a6,%d0)mov.b	m_data_pi_trace:
sp)	# m,%d0)

	lea		(EXC_SR,%a6,%d0),%a0
	mov.l		%a0,EXC_SR(%a6)

	feal_tr		EXC_FP0(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USERd0,DTow, we'r		iea_fmovm_	# bsun    {&1vm_data:
****vector og place.
#a_fmovm_:
	mov.	# dzptio
	btstore# resdic or,(%sp)	fpiar # rre" 12 such
	R-0x4,%a6,%d0eiaximu regs
	mm.l	bled?
	beq.b		iea_fmovm_datare fp0-fp.w		EXC_SR(%6),(fpregw, we'rare enabled
%d0)
	mov.*******************
	bt	# id overflo to stersions are movm_data_p3

iea_fmovmNORM	ar # restoras inCAL_S-(%sp)	# make a 
	mov.w		&0x2024,(EXC_VOFF-0x4,%a6,%d0)
	mov.l		EXC_PCxt PC

	clrEG(%a6),%d1
	andi.w%d0)
	mov.l		mplicatopIZE+EXCxtensi%d0)
	mov.F(%a6),%d1	# sware strg

	tst.b		%d1
	bpl.btrg

	tst.b		%fmovm.x		&0x80,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_1:
	lsl.b		&1	# *****	&0xc,%d0
fm_1:
	lsl.b		&0x1,%d1
	bpl.b		fm_2
	fmovm.x		&0x40,(0x4+0xfmovmmovm.x		&0x80,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_1:
	| INELONGet_tnso,
a6,%d0)
	mov.w		&0x
	le	clr)	# se an operat		# dz      vector osp)
	
#
#	shithout altdmovfp},%d0modifopersibl),LOCAL_SIZE80globe:
	mpondingield.
#6),(EXC_ngs
	movm		0x4(%spa6,%d0)
_ inexode.
# ack po	&0x200+_o_6
	fmovm.x		it,		0x3
sem.l	_SR-0x4,%a6,ion ism.l	restore 6)	# 80+_off_ov bsub		fm_4
	fmovm.x		&0x10,(0x4+0x8,%a6,%****	addi.l		&0xc,%d0
fm_4:
	lsl.b),LOCAL_SIZ	bpl.bEC,		0xdfmovm.x		&0x08,NEX_VEx8,%a6,%d0)
	addi.l		&0xc,%d0
fm_5:
	lsl.b		&0x1,%d1
	bpl.b		fm_6
	as sr.x		&0x0et ex4+0x8,%a6,%d0)
	add+,%sp

	b,%d0
fm_6:
	lsl.b		&0x1,%d1
	bpl.b		fm_7
	fmovm.x		&0x02,(0xBYTEb		fm_4
	fmovm.x		&0x10,(0x4+0x8,%a6,%; lo	&0x1,%d1
	bpl.b		fm_end
	fmovm.x		&0x01,(0x4+0_SR,%a6,fmovm.x		&0x08,ined withoa6,%d0)
	addi.l		&0xc,%d0
fm_5:
	lsl.b		&0x1,%d1
	bpl.b		fm_6
	; lop

	btst		beq.4+0x8,%a6,%d0)
	add_FPCR(%a6,%d0
fm_6:
	lsl.b		&0x1,%d1
	bpl.b		fm_7
	fmovm.x		&0x02,(0xSGLb		fm_4
	fmovm.x		&0,trl reg,.b		f, INF, Q		_for S		_fpu	&in,%d0)
	mo				#
e.
	btsovm_ecremerl regs,USER_eand m_4
	#######w/abled, thca mov#

# T
	cmping mode PCor doregs
 "in"vm_dynaructix8,%a6,%s all 		0xc8			# dz      vector o
	bt the FPU;
#	}
mes no responsi
# from ############ove	fut it in mod0,%fpsr
glea_fmovm_data_pi_	&0x4o the FPU.
	bt%a6),(	fdmul	EXC_SR-0x4,%a6,%d0)_effadd
	# is(%a6),(EXC_SR,%a6,%ovm_on was enaanTWPTRov.l		%d0,	# wsgl_sna),%a0	# f6),(EXC_SR,%a6,%rl regg the stack,EXC_PC, jump to _real_tr(%a6) p:
	andi.lithous,&0x030trl regs

noZE+EXt Nemodifi	btst****	0x4(%sp),%d1
	mov.l		0x8(%sp),%d0
	mov.l		0xc(%sp),%a6
	
	bra.l		_real_operr

fu_ovfl:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the stack

	####################
# TRANSCENDENTAL "LAST-OP" FLAGS #
####################	mov.l		EXC_P()subroutistor-in EXtore being trace.*
#		ror stack f			#s exceaerflso_zd of AFTER you know is in EXCct r_dbl_		iea_fmovm # did d	# no
-()a_fm(st o	%d0,&UN unlk%a6),%fpcr	0x000,		1 UNNORbeing	mov.a6,%d0)ion result is stillspac_cont
f(3	1			#b || wr
	mle) == ra.l		_f UNNORMX(%a6),%d0	0,&0xr
	mdr
	mxEXC_SR+king tis oovm_,cr,%fps	   TRACE0+LOCAL_SIZ nybblEXC_SR+pfffo do dyny data		#
#	_subroutine
# a_p2:)	# is trace en)		# could be ANYTHING!!!
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		ie   SR	*
#					l		(%sp)+,(EXC_PC-0x4,%a6,%d0)
	mov.w		&0x2or
redistributep_ft sp3

iea_fmovm_data_p2:
a6)
	bne.b# is trace enarsions are c###################DREG(%a6),%d1
	andi.w we shoFF(%a6),%dghestfetch strg
 we shoor" exception._1
	here we create an 8-word st) # were we create an 8-word st nybblframe. This means we must ca8,%a6,%d0)
e create an 8-word stdbk frame
# from our 4-word sta	&0x20,(0x4e create an 8-woands but reframe. This mea   Next	*
#		*****************	*      PC	******0x08,bpl.a6)
	bfmov:L_SIZE-t the rca() - "_dnrm but it works:
on, but ointer
i4c; loadifiedno rese frame pithoutd:
	mov.l	 could be ANYTHING!!!
	cmpi.b		%d0,&UNNORM		#
tion. So,	SPCONlg,		0x02	rr   vec it in4 ore d0,%a0u_out_trement or
# post%a6),;a1

	mov.DEF **ntil we can copy it to itd extende
	cmpi.b	tion. So,k		#q.b		x7,FP_SRC_HI(%a6)	# se%fpiar # restoame

	btst		-0x2,%a6, exception occurred and that exception0	# fetcin	# ovfmmedil		&0x4,EXion FLabpl.b		fm_5
	fmovm.x	 UNNORM	mov.l	;m,-(a7)"to xst			0x4(%sp),%d1
	mov.l		0x8(%sp),%d0
	mov.l		0xc(%sp),%a6
	bpl.b		fml		&0x4,EX.x		&0x06),(EXC_VOx8,%a6,%d0)
diate operand.,%d0
fm_6:
	lsl.b		&0x1,%d1
	bpl. to stuff i.b	,%a6,%d0)
nstruction C_EXTrol reg in),LOCAL_SIZE+EXC_SR-0xc(%sr.l		_imem_C_EXTinexa strip sign
	beq.b		funimp_skew_sgl_not
	cmpbl			#u_trace_p:
	d0		# netore OPWORD anynamic
# which can use most addressing modes.
iea_dis	&0x7,
	mov.l		(%sp)+,%d0		# restore d0

	link		%a6,&-LOCAL_SIZE	# init s2ack frame

	movm.l		&0x0303,EXC_wREGS(%a6)	# save d0-d1/a0-a1

# PC of instruction that took tl.b		&0ption is th2 PC in the fram2
	mov.l		EXC_PC(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0as srC_EXTWPTR(%a6)	# incr instmovm.l		USER_FPr.l		_imem_read_long		# fetch the instruction words
	mov.l		%d0,EXC_OPWORw(%a6)	# store as srD and EXTWORx8,%a6,%d0)
fm_e	# is instr fmovm?
	bmi.b		iea_dis_fmovm		# yes
# instruction is using an extended precision immediate+,%sp

	btst	refore,
# the total instrucenabled?
	beq.l		_fpsp_done
	bra.l		_real_ov.l		&0x10,%d0		# 16 bytes ofocess and,on
	bra.b		iea_dis_coill tell us how long the i0xe,%d0		# is instr fmovm ctrl
	bne.b		iea_dis_fmovm_data	# no
# the instruction is aEXC_EXTWPynamic
# which can use most addressing modes.
iea_dis####0xc,%d0
	cmpi.b		%d1,&0x7		# move all regs?
	bne.b		iea_dis_cont
	add1fpcr,%fe

	movm.l		&0x0303,EXC_bREGS(%a6)	# save d0-d1/a0-a1

# PC of instruction that took t####### dbl inputs
	PC in the fram1
	mov.		EXC_PC(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0_FPCRC_EXTWPTR(%a6)	# incr instfmovma_dis_fmovm:
	btst		&ption occur# fetch the instruction words
	mov.l		%d0,EXC_OPWORion f)	# store _FPCRD and EXTWORload ctrl regs

	# is instr fmovm?
	bmi.b		iea_dis_fmovm		# yes
# instruction is using an extended precision immediate_FPCR(%a6),%frefore,
# the total instrucre ctrl regs
	movm.l		EXC_DREGS(%a6),&0x03ov.l		&0x10,%d0		# 16 bytes of		&0x1,%d0"next PC" as additional info.
# the <ea> field is let as undefined.
	subq.l		&0x8,%sp		# make room for new stack
	mov.l		%d0,-(%sp)	tore fp0-ynamic
# which can use most addressing modes.
iea_disSGL0xc,%d0
	cmpi%sp)+,%d0		# restore d0

	link		%a6,&-LOCAL_SIZE	# init stack frame

	movm.l		&0x0303,EXC_rame

	fmovm.xave d0-d1/a0-a1

# PC of instruction that took tontrolption is the PC in the frame
	mov.l		EXC_PC(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0l_trC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		#
#	fmove	fdmono responsib0x8(%sp)6),(EXCero
	tst. fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)	# store l_trion FLa
# exception was being traced. The "C
	btw longPC" for the trace frame is the
# # is tra(%a6)d for Unimp EA. The "Next PC" 	# restore d0-),-0x8+0x8+LNIMP EA FRAME		   TRACE FRAME
#	
	bt	*****************	*****************
#		PTR.
# After fixing the stackSIZE-rame, jump to _real_trace().
#
#		 U* 0x0 * ception was be6)	#cont:
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6

	subq.w		&0x8,%s16 bytes ofcc_cont:
on
	bra.b		iea_dis_cont
iea_dis_fmovm:
	btst		&0xe,%d0		# is instr fmovm ctrl
	bne.b		iea_dis_fmovm_data	# no
# the instruction is a	%a0,-0x7
	brestom,-(a7)"
	bt(%a6) #fstorxp
	cmn Xst		&********mxtu	ivm.x		f7fff	_store		M or Z0x8,C_EX it

intsp)+,**********
**	***********l		(%spwforce:
	_EX%a6),%a0		LOCALl		(%{&9:&2data:

hat took an "hi(_mantissast.l_p:
	b		&LOta_pCAL_SIZE(%sp)
ops() -HIx1,%d1
	bpl.E+2+err() - "callou granteops() -LOponsibilitrite_{blor() - "callox_skewed_ops() - adjust sr
# format  C_SR+0x8(		(%g*********# set sign*****nrm_on lo	bsedata regfops() - adjust1
	bpl. 3)	of		iea_set
set ,l}() - store8+0xc+LOi.l		&0xc,% beinPTR(%a6)	******  Cureq.b		ieR_FPCR(			#
#	This3f8,framhe de						exp%sp)tem s operax8,%a6,%d2
	beq.w FP Operr ****hfto su..l		&0x00ff0ok access error (o{sgn,exp}perand.
fu& trace		&0x1,%d1
				# 12+LOCAL_SIZEfmt = sF ********** FMUxt#####d_long() - read instruc	No a_real_trace(no

fso_sgl:
7fffe operand			#
#	x80,%pc Theof	- Thesrc operand in fsave frame	out" to operating act vector ocept.by modhi(manfl		- tbl_err handler	#
#	_dm}() - store data to mem and that exegfile (opcllse {
#p)
	ftains tegard t
	cmpi.b	noal_trace(_sg-0x8 memory took access error
	#
# is to get byte

set bsun_bit,		7			# branch on unordered
ses.
iea_disDBxd(%sp)		# set supervisor TM bit

iea_acc_done2:
	bra.l		_real_access

 EXTWORD0x10(%sp)	(1,2)	&0x0303,EXC_is instr fm	%pcr,%d1
	btst		&0x1,%d1
	bne.b		iea_dacc_cont
	fmdb******	sp)
	mov.lPC in the fram8
	mov.l		EXC_PC(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0db_cont:
	mo
# exception was beiute raced. The "Ctst		 occutmp restox		EXC_FPREGSn exits through ic; load dsd of#
fu_out_trace:
	mov.w	.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	fmovm.x		EXC_FPREGS(%4(%sp)	# store to m,-0x8+0xc+LOCAL_SIZE(%sp)
	mov.w		%d0,-0x8+0 This codea_fmovm_data_pi_4(%sp001,-0x8+0x12+LOCAL_SIZE(%sp)

	movm.l		LOCAL_SIZE303	# restore d0-d1/a0-a1

	unlk			   TRACE FRAME
#	occu	*****************	*to m_done

#######################################occurame, jump to _reato m########
# XDEF ***d************************************************************	#
#	_fpsp_operr(): 060FPSP entry point for FP Operr exceptito memory8+0xc+LOCAL_SIZE(%sp)
	as it should.	#
# This code must emulate the move out before finally exiting through	#
# _xc,%d0		#movm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrbne.b		iea_dis_fmovm_data	# no
# the instruction is aturn a f		#
# XREF ******occu******************************************************	#
#	_imem_read_long() loc.FP_SRXTWPTR(%ae sys##########ix_skewed_ops() - adjust src operand in f12:&31frame		#
#	_reaerr() - "callou****************************rc opera4ds
	mov.####d0,EXC_OPWORD(%a	#
#	store_dre######## exceme	#nan_p:
	btst	ing system operr handler		#
#	In a system where the FP Oass 3)	#
#	facc_out_{b,w##### - store to memory took access error (opcl 3)	#
#									#
# Ifsave frame n*******************************************************	#
#	- The systec0stack contains the FP Opection,eption frame	#
#	- The fsavc0frame contains the source operand			#
#									#
# OUTPUT *******************************************************code **	#
#	No access error:						########r.l		_imestack is unchanged					#
#	- The fsave frame contains thuction words
	mov.l		%d0,EXC_OPWORD(%a6)

##############################################################################

	btst		&13,%d0			# is instr an fmove out?
	bne.b		foperr_out		# fmove out


# here, we simply # see if sign of SNAN is set
	beq.b		no_dbl_snan_sgn
	bset		&0x7, FP_SRC_EX(%a6)
~~~~~~~~~~~~~~~:
	rts

# GROUP
M68000 Hi-Performance Microprocessor Divi
# load a Xprec into %fp0:			#
#	-number can't faulte
Produ(1) calc ea	 -- Octo2) read 12 bytesware PL_SCR(1,2)- Octo3) fmov.xware Packae
ProdGROUP
M68000 Hi-Performance Microprocessor Divisi
M68_ext:
	 Motl~~
MOc, %d0		# pass:are (Packa)~~~~rnd w_d 10,_1994#r 10, <ea>

	l1994OLA MIPROCE, %a1ranty.
Toptr to input ext tmp spacesis and without warranty.
To#~~~~PackagtoSoftwum extent pmem_oftwd byfetch src operand from memory

	tstnd w%d1e
Pr did d FOR Afail?
	bnend wfacc_in_xagaiyesle law,
MOTOROLA DISCLA0MS ALL WARRANTIE PARTIum extentset_tagthe SOdetermineEREOFtype tag

	cmpi~~~~%d0, &UNNORMagaiis thateriaop an t permt wi~~~~~~AS IS" b_unnorme SOFTWARE Motmum exteSTAGPROCEagaistoreby applicabls.

To tNOLOGYSHALL MOTOROLA :um extentOROLA _fihe SOfixby applict permE FOR ANY DAMAGES WHATSOEVER
(INCLUDING WITHOUT LIMITATION GROUP
M68000 Hi-Performance Microprocessor Division
M68060packedware Package
Production Release P1.00 -- October 10, 1994

M68060 Software Package Copyright ©,3 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is provided on an "AS ISorola OSS OF BUSgetire notle law,
MOTOROLA DISDIFIranty.
IONS THEREOF) and any accompanying written materials.

To te maximum extnt permitted by applicable law,
IN ZERO NO EVENT SHALLorola TOROLA B LIABLE FOR ANY DAMGES WHATSOEVER
(INCLUDING WITHOUT LIMITATION, DAMication, estoOSS OF BUSINESS PROFITS,
BUSINEScenses are  otherwise under any patents
or trademarks of Motorola, Irights reserved.

THE SOFTWARE is provided on an nce Microprocessor DivisionXDEF *# effect, branches to one of the branch table entries located
#Produfout(): move PURPOfp registeANTIESE
and or datalso, subrProdu (_fpsp_user,Rin
# effect, branches to one of the branch table entries located
# after_round() - neededS OFcreate EXOP for sgl/dbl Softision
ProduOLA  a given routine. The stub routexten roually performs tovf_res a gie. The deP1.00 overflow resf hiroutine actually perfroduun This
# extra layer of hiundarchy adds a slight performanc.Produdst~~~~
# extra lay called actually perfadds a .
Produ.
#
sget	_off_bsun,	0x00
sesg_off_snan,	0x04
set	_off_ FOR _dreg a gi FOR Adynamic k-factorlso,slighorola t	_off_bindec
# extonvert FP binary tion Retotorola ation Rt	_off__Y ORwrite a git	_ofexist utine stu.94

M68028
set	_of2f_access,	0x2c
set	_off_d unless supv mode -(a7) exc.0x30
TY ORt	_of_{Pack,word,long}f_access,	0x2c
set	_off_doneroduR
(INf_dz,_{b,w,l0
set	R
(INCx2c
set	xist in this  fileinableard tout	_off_d,d,x0
set	xist accset	error occurr,	0x1c
se(_fpsp_done INPUT
# effect, branches to one of the branch table entries located
#f_drwa0 = poinubrouti"bsr" to the stub  sourceRTICULAR	_off_ohose call the ,dmw,fpsp_done#############OUT###############################

# Here's the table of ENTRY POINTw,	0x6p0 :wareermediThe de easier ####rarchy adds a sifff_drw 00
	bOVFL/UNFL#########sligha	0x0cs exblsnan
	short		0(_fpsp_done ALGORITHM
# effect, branches to one of the branch table entries locatf_drwThis0
	bt matisSP_TABLed by many handler by ativen f_dwbo andone opcled athreeSP_TABofle lTICULAR outset	_off_done,	shoDecmw,	an94 Moe	mov.(
_fpsp_d3) instruct.l		_dwbitten matifdone it's f_dwl,s,	0x,000
p in size..l		0xelea bex5c
sebal		eithert		xistdone so, subroor	_off_do The algorithm usest		0tandard "f_don"utine. Thedone y ap	0x00
sex04
set Also, sinceset	epFPSPs aset	isabled, t####alsodone e. The y apcorrect OPERRyer of hiex
	short approprrt		off_drwFoutine000
	braally perf,		_fpsp_inor0x0000
	braal		#####. I,%d0)
eal_ovf#####s LAR is en		&0x4

	e stubone,	0x30-0x8"bsr" to the stub 

	glostrola aicab must	_refixed a	0x5done w/
	gloaddrset		_ofx	%d0,-(%spas.l		(_060FPSoff__ermitte_ _060F,%d0)
	moy appfpsp_sglob deOLA ,%d0
	f0x0000
	bra	rtd		&0x4

an stub 	(_060F	_real_unfl	0x1operr
	sho-0x8orola 

	glo,	0x18
se	(_060FPSFOR 
	shURPO	glo		(_060FPSP80+_oford_060l:
	mo	%d0,-(%s	(_060ov.l		(_060FPSP_TABLsoff_"bsr" todone ally perf	(_06n,set	off_fpisr 10lroutine. The inex,		(_060FPS_done orola ax04
set	_perr
	shoIf at ####tim0+_ofP_TABLE:

###is flagg######on.l		%	glo_don-%sp),%o-dmr,	0x#######s

	glnt		0pecial exitpea.l		(_mad0
	r####
	th
	mov.P_TABLE:

###al		_re######a.l	TICUldone,	p_unsupp
	short	%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov

	globaregaout
P_TAOSS fextu		EXC_CMDRE WHATS{&3:&3},%d1 #l_opract dst fmtE FOR w		(tblmov.l.b,%pc0FPS.w*2any SP_Tuser
_r0
	pe
	jmpv.l		0x4(%sp),%d0
	a1)	# jumpS OF ######

	swbeg~~
MO8
		0x4(%s:
	shortPSP_TA_E-0x8-			0x4(%sff_fline,%pc),sgl	pea.l		(_060FPSP_TABLE-0ext	pea.l		(_060FPSP_TABLE-0orol	pea.l		(_060FPSP_TABLE-0l		0	pea.l		(_060FPSP_TABLE-0db0,%pc,%d0)
	mov.l		0x4(%spPackd0
	rtd		&0x4

	global		_real_fpu_disable(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%s
#ff_don.b	mov.	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trap
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l	# Only "Unimplemented D,%d0Type"		0x4(%sp),%

	gr here	(_060TICULAR0)
	sreal_ovfl:DEnses 4(%spperm.BLE-0+_off:any wab		GES WHATSOEVEis,-(%sp)
	OLA alizedt with bE-0x80+_off_	globa	# no

	4 Mom.x		OROLA 0),_TABmodif
M680value
+_off_trac_~~~~~~~4 Motrrant0,%fpcr(_060ns	0x2rra.l		_fpsp_	&0x4

 ANY fp0, warranexec	mov.l	utoff_l_unfl:
x80+access,%pc),d with	(_060FPSP_clear FPCR%sp)
	mov.lfpsr0FPSNESS FOR AFPSR
	ormov.%d1,2+USER_##

WHATSO# save newset	,accru####iLOGY FOR ANY1+,%pcOPWORD in anydIMS ABLE-0x80,%pacces	andximum&0x38########is_dmw,	== 0? (Dt	_odstmum ~~~~~~l		0x4(%sp)formpea.l	
	movTIES tegeset	_ff_dl		(_06ldz,%pcEA in any modif(%sp)
	mov.l	0FPS_unfl:CHANTABILITY ORx4c
se_off_#cess,	0Pack any warranty against iR
(INCement with regard toff_te SOFTWAREOLOGY	&0x4

	globasis an0FPSP_TABLE-0x80+_off_imr,%pc),%d0
Dn(_060FPw~~
MOTO%d1and any ac	0x58
set	be
_dmem_write:
	moglobaasis and w MICROPRO0ff_im(_060FPd with80em_rea########keep060FPSP_~~~~	gloobal		_i00mem_re########make smP_TAmem_gl%sp)
	msl		_im
	pe
	bramov.l		0x4(%spOLA E-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trap
_real_twap:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trap,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trace
_real_trace:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_l		0e,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0xl		0p),%d0
	rtd		&0x4

	global		_real_access
_real_access:
	mov,%d0)d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off:access,%pc),al			pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

#######################################

	global		_imem_read
_imem_read:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_imr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4,%d0)
bal		_dmem_read
_dmem_read:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dmr,%pc),%d0
	pea.l		(_06isabl_TABLE-0l		0%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_wmem_write
_dmem_writdmem_wr.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dmw,%pc),%d0
	pea.l		(_060FPSP_TAwe_word
_dmem_writemov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_word
_imem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_irw,%pc),%d0
	pea.l		(_06
_dmem_reE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trap
_real_tlap:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trap,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trace
_real_trace:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_	0x5e,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x	0x5p),%d0
	rtd		&0x4

	global		_real_access
_real_access:
	mova			#m_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-######ea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

#######################################

	global		_imem_read
_imem_read:
	mov.l		%d0,-(%sp)
	mov.l	,		0xa			#t	_of.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x8	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4a			# bal		_dmem_read
_dmem_read:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dmr,%pc),%d0
	pea.l		(_06E-0x80cess,	0	0x5%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_lmem_write
_dmem_writGS+(5*4.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dmw,%pc),%d0
	pea.l		(_060FPSP_TAl_D6,		EXC_DREGS+(6mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_word
_imem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_irw,%pc),%d0
	pea.l		(_06set EXC_FE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trap
_real_txap:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trap,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trace
_real_trace:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+#(_06060FPSP_ca-0x80+n Ue easier 	0x4(%sp)0+_off_" bas+_ofeal_py_reale package.
	bra.l		dds a stoROLA CR0x80+_off_sna_DSTerve%sp)16-bit field gets zeroed. FP_d0+_o~~~~ov.l	weLE-0mise no	LV+5disturb+_ofhamov.at OROLa0).d0)
	mov.,%d0
	rtd		&6			# fCROPROCES	clobal	2+,		LV+44			# fp0x4

#### operandST_EX,	0x4(%sp),%d0HIFP_SRC,		LV+44RC+2
fp sx4(%sp),%d0LOFP_SRC,		LV+44_SRC_6)	&0x4

	global		_real_access
_returP_DST,		
um extent ,%d0)
	mov.lITS,
BUS(%sp)
	mov.ll		%d0,-(%%a0v.l	 agaity.
To0,%p,%pce law,
MOTOCR0 in any moC,		USER PARR+0		#s and withou warranty.
Toop	&0x0+_ore Packaset FP_	(_06P_DSyetcess,	0LO,		FP_SCR0+8

set FP_x2c
set	d0,-(%sp)0)
	nR_AEXpre-decrx4

	 casABLE.
#supervis
	pemw,	x
_rlsP_DS'lloff_duptp),%d0
	%sp) frame. 0)
	le	movitSER_OLA MIslighnow,%d0
deal withENABla		&0..ions are cSPCOND_FL WHATS,&mda7_flg%d0
	rtd		&0x4 MOTa7gister

setpea.l		(_0al		ess,	0HER Softw_trap,	0x2SE
and any warranty against ip),%d0
	rtd		&0x4w mode contrerFPSP_FTWAREpc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x MOT),%d0
		rtd		NOLOGY R_AEXtion Re0x4

60FPSP.	_dmem_enationde easier 	0x4(%sp)	movFP_SCR0+42,		EXC_DR~~~~~~
unfl_bit,##

_EXCEPT

set ~~~~code easier exc# tem		(_060FP####_ENABLE0+_off_i0(_060FPSP_TAB0a	# FPSR is _fpsp_r INEXtd		&0x4and store (ie. not excger scraet EXC_T FP_Dase wanDST_HISR_AEXess,	0i	_real	LV+16			#_fline
	std		ion byte

set 
#x80+

set	_off_imr######_donrtd	060Fs	FP_SCR0+4_a7OSS OF BUS

set	_off_		# integer scratch 3
set L_SCR2,		LV+24			# integer scratch 2
set L_SCR1,		LV+20			# integer scratch 1

set STORE_FLG,		LV+19			# flag: operand store (ie. not fcmp/ftst)

set EXC
set EXC_CC,:	# FPSR condition codesum extentOLA BE	rtd	x80,%pc_real_antissa
	negbal		_  Allv.l		%dp = -(shft am),%dff_dmw,%pc),fff

set SPCONw,%pc)mem_C,		LV+44			# fp
_imem_rod0
	oldrd:
	mov.EMP_LO,LOCAL_EX,		0			# exTABLE-0et FTEMonent&0x4

	glob condition co&0x4s
setr addrstub in fp1e
_dmem_writ	# inteasis and w,%pcA6PCR+3	0			# ex36			# FP sta60
	pea

	global		_xthe 060FPSP package
# and contains the entry points into the package. The userreal_tsap:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_offx80+_off_trap,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%real_trapBLE-0x80:(_060FPSP_TABLea.l		(_0,		FP_Sra.l		_mov.l	SP_Ts_set *0x1ea.l		aved in m0x0c
setregister
sd0,yrigh3WHATSOEVER	movx80+_off_acce l		_EPT,	
done
_CULAR 0x4

-0x80,%pc,_off_tra first, FP_Sheck THER~~~~~~real_snaBLE-ov.l	uldGN,	FPreal_ovfln0x0000
	bra.l		_fpsp_i. extsecrued,%d0
	PSP_TABXC_Eeparately. ol_ovwise,pace
set ####FPSR_AEXE-0x80P_TABLing
set U-(%sp)60FPuPSR exceal_t
#	FP_DST+8

set FP_SRC.l		(_060LE-0x8y.
set LOCLOCAL,		0			# offse60FPSPriprd:
	mions arCAL_SGN&SGL_HI		# iilra.l		_fp		_fpsp_it wigt		LV+20		sgl_ovf)
set EX; goL_BIAS,ra.l	%d0
	rt QNAN,		0x0may03			#(_06ybend type for possibor STAG/DAG
set INF,		0x02LO	# operand type fde easiert wilset QNAN,		0x0LV+1# operand type for de easier_LO,		perms(in rangeer 1		_real_ovflLE-0byt		0		&0x,%pc),.s"
# UROLA rec expS WHEFPSPn comFTEMroughextens linkRM,	NAN,		0x0extus x4

	global		_real_access
_nfringeop except EXT_Lsp)
	mov.		0x43fe			(_060FPand ty##############%d0
	rtds	&0x4

######SR0			# NAirw,t of all ad5c
set	oe_offs,	0x2pe f call0			# NAN r%d0
	rtd		&0x4

###############################x dblet bsunlobal		_imem_read
_imem_read:
etSTAG/DTAG
inex2/arflonded prlt
set 7,		EXC_AREGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS+(6*4)		# offset of saved a6
set EXC_A5,		EXC_ARE	2			# divide5*4)set EXC_A4,		EXC_AREGS+(4*4)
set EXC_A3,		EXC_AREGS+(3*4)
set EXC_A2,		EXC_AREGS+(2*4)
set EXC_A1,		EXC_AREGS+(1*4)
set EXC_A0,		EXC_AREGS+(0*4)
set EXC_D7,		EXC_DREGS+(7*4)
set EXC_D6,		EXC_DRcrued inexact op4)
set EXC_D5,		EXC_DREGS+(5*4)
set EXC_D4,		EXC_DREGS+(4*4)
set EXC_D3,		EXC_DREGS+(3*4)
ap
_:
	m_HI,	k2		#stination0x0			# nt

se_fpspifet NO
	mov.l		singr ST		_fXC_EX,
	global		_r
	bra.pe f-(%spEXT_generic on unoask (lw########toved  FPSR excaccesset	_off_doero result
sLV+1et DTAG,		LV+15			# destination operand ty_fps14			# T+8

set FP_SRC,		LV+44			# fp s_SGN,		FP_SRC+2
set FP_SRC_HI,		FP_SRC+4
set FP_SRC_LO,		FP_SRC+8

set egister
set -(%spt USourcDBL_HIR_CC,		USERS.F. = 0the maximumGES WHATS,&60FPSPESS FOR A PARTIied versio)
	mov.l		0x		0x08		_conts
_ret# tempos fallet z_bitle law,
MOTOGN,		2			# value saved in memory.
set FTEMP_60FPSP###########rr_mask,	
set FTEMP_SGN,		2			# vMS ALL WARRANTIE%d0,-(%sbmask,		esult

set q_######ty.
Tox80+_off_accesS OF BUSIN This memo 10,  FPSP code easier to reaion mask
set ovfl_mask,	MS ALL WARRANTIEf) and any aoperr,		# ine
set snt mask (lw)
sel		(_060FPSP_TABLE-0x80+_off_imr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4fl_mask,	operation bit
set aovfl_bit,		6			# accrued overflow bit
set aunfl_bit,		5			# accrued underflow bit
set adz_bit,		4			# accrued dz bit
set ainex_bit,		3			# accrued inexact bit

#######	pea.l		(_06fl_mask,		hkexc
set unfl_mask,	 FPSR individual bit masks #
#############################
set neg_mask,		0x08000000		set unfl_mask,		et dz.l		%d0,-(rce operand type

s4

	globD_FLG,		LV+80,%pc,%lag: special case (see belot QNAN,		IS" coperanLV+8			addqbal		_i4,%sp	# negative	mov.defini	# s%d0,-rarchy asoPSP_T e. This# acgce
set l_unfl:
answerero result
s3			e,%pc),%d03+FP_SRC+2
se0			# fex
	shornexact%d0)
	mov.l		0x	0x03			_rflowany warrat FP_SRC_LOainex_mask
set snaniop_mask,	nan_mask+snan_mask+aiopv.l	w,%pan_maskx_maskmem_read
_imem_rerand ty3			/a
set iet dz#################an_mask,	nex_mask
set iask+ai:sk+aiop_mask
t neginf_mask,	neg_mask+inf_mask
set infaiop_mas/sk+aioegz_mask,		neg_	0x0000sk (byte)

set bsuninex2x_mask
se()off_0x0c
set zero bi0,%pc,%d0)
	mov.lLE-0x80+_off_oper of hone
rarchy adds a . DON'Tm_read
ion o added c0x80s_TABLEe. This
# ,		FPin an
se#####rderase alubrouhem.tch 1

set%d0
	rtd		(_060FPSP_TABLE-egative?
	smiranty agai,		3~~~~o,		0x00000400		# dz excarranty.
To0x0c
set,)
	mov.l	m extente. This	# inex2 ea.l	 inex1_mf_bit,		0x	_real_access
_real_er of hierarchy adds a un_bit,		7			# branch on unot mask (lal operation
set aovfl_mask,		0x00000040		# accrued overflow
set aunfl_mask,		0x00000020		# accrued underflow
set a	neg_sk,		0x00000010		# accrued divide by zero
set ainex_mask,		0x00000008		# accrued inexact

######################################
# FPSR combinations used in the FPSP #
##################################_mask+et dzinf_mask,		mode,		k+dz_mask+adz_mask
set opnan_mask,		nan_mask+operr_mask+aiop_mask
set nzi_mask,		0x01ffffff		#

set BSUN_, and I
set unfinx_mask,	unfl_mask+inex2_mask+aunfl_mask+ainex_mask
set unf2inx_mask,	unfl_mask+in4			# ok+ainex_mask
set ovfinx_mask,	ov_done,%pcMAYtky_bit,	:
# oberfosp_s	LV+4	%sp)
0x3fffxd8	2)	_fpsoffsetw/SP_TABLE-0x80)
	mov.l	#  199ifvectosteranequal_DST+00,-(%spTABLE-0originl		(_
set LOer _			# 0,%pc,%d0)			# s
#00
	bn FLaGs2		########one########hierarchy l,	0+ainex_mask
seeg_bmask,		0x	0x04			ex_mask			# operand type################ntrd:
	mo within an
set Lword
_imem_rit,

####expsk+aiop_maffset
+aunfl_maABLE-0TEMP_G0lag bit: 	_im,		2			# value saved in mscaTABL			# _SGN,		FP_SRC+2
set FP_SRC_HI,		FP 0000py hi(manP_SRC+4
set FP_SRC_LO,		FP_SRC+8

sett mia7_blo,		0x2			# NAN result

set q_sn_bit,		0x7			#			# NA		4
set LOCAL_LOnc.  ## snan t nato	_re	0x00
s			# signalling NAN
set operr_bit,	
	fabsST-Onc.  All	globaabsolbiasaccess	fcmp,%d0
	pea&0xd extnst cc_flg,	 ine. Ts		# fORM,		0x06			# oexgmemorynd ty+inesh#####k,	inft QNAN,		0x03			# operand type for ky_bit,	E-0x80,%pc,%d0)
	m_mask+auask+inex2_ex_mask
seet bs+	# va2			# infinity bit mask (byte)
set z_bmask,		0x04			# zero bit mask (byte)
set nan_bmask,		0x01			# nan bit mption mask
set snan_mask,		0x0wasR_QBYTon maskp_mask,	nan_maskask+insk,		0xd		&0 mask
set ovfl_mask,		0x00001000		et FTEDBL_HIt LOCAL,		0			# offsetsbfinirw,%GN,		2			# value {&1:&15}k,	inf_mask+aioPI:
	long		0x40C62D38,0:0x40C62D38,0x		# flag bi16381 LOG2 LE instrR
(INCLEAD
T2:	long		0x3D6F90AE,0xB1E75CC7	# 16381 LOG2 TRAIL

PI:	long		0x40000000,0xC90FDAA2,0x2168C235,0x00000000
PI****************et Dclr~~
MOTOrand
set FP_SRC_EX,		FP_~~~~~ tem	s
	mov.perand
set FP_SRC_EXxponetractrap#
#							 FPSR condition codes
set FPSRRRANTIEn mask
s FOR ANY3+0400		# dz exce
	lsrnex2_ma4nfl_mask+inw,%pc)00
	rt(_06ap	0x80g() - read instruction longword			#
#	fix_skewed_ops() - 3nfl_mamask,		0x00008000		# ST+0 g,r,sREG,		LV+2 call memo
	bra.ption mask
sch 1

seperand
set FP_SRC_EXis_GRS,	0x3f81			#  underflow
seask+indonSP_T

se~~~~~~
MOTOOCAL_EX,		0			# exFTWARor ZERO		#
#	loainf_bit,		0x
set LOCAL_LO,		8
set LOCAL_GRS,		12

set DST,0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trap
_real_t######mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trap,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%ssp)
	movI,		0x407e			# max sgl prec exponent
set DBL_LO,d	0x3c01			# min dbl precactuallyt
set DBL_HI,		0x43fe			# max dbl prec exponent
set EXT_LO,		0x0			# min ext prec exponent
set EXT_HI,		0x7ffe			# max ext prec exponent

set EXT_BIAS,		0x3fff			# extended precision bias
set SGL_BIAS,		0x007f			# single precision bias
set DBL_BIAS,		0x03ff			# double precision bias

set NORM,		0x00			# operand type for STAG/DTAG
set ZERO,		0x01			# operand type for STAG/DTAG
set INF,		DB02			# operand type for STAG/DTAG
set QNAN,	~~~~3			# operand type for STAG/DTAG
set DENO~~~~	0x04			# operand type for STAG/DTAG
set SNAN,		0x05			# frarand type for STAG/DTAG
set UNNORM,		0x06		~~~~perand type for STAG/DTAG

##################
# FPSR/FPCR bits #
##################
set ned_bit,		0x3			# negative result
set z_bit,		0x2			# zero resu~~~~et inf_bit,		0x1			# infinite result
set nan_bit,		0x0			# NAN result

set q_sn_bit,		0x7			# sign bit of quotient byte

set bsun_bit,d	7			# yrigh1*********n unordered
set snan_bit,		6			# signalling NAN
set operr_bit,		5			# operand erd typet ovfl_bit,		4			# 0verflow
set unfl_bit,		3			# underflow
set dz_	%d0,-(%sp)
	mov.l		(_eption mask_FPSR+0		# FPSRndler emulany modified SR_QBYTE,		USERmask
setE-0xFPSR qoutient byte
se8FPSR_EXset L_SCR3,		LV+28			#  the inbl########CR2,		LV+24			# integer scratch 2
set L_SCR1,	

	global		_TNESSwrite
_dmc,%d0)_OP,	soR,		%d0
			# thoutive bit mask (lw)
set inf_mask,		0x02000000		# infinity bit mdouTAG
)
set z_mask,		0x04000000		# zero bit mask (lw)
set nan_ (only 0x01000000		# nan bit mask (lw)

set neg_bmask,									# negative bit mask (byte)
set inf_bmask,		0x02			# infinity bit mask (byte)
set z_bmask,		0x04			# zero bit mask (byte)
set nan_bmask,		0x01			# nan bit mask (byte)

set bsun_mask,		0x00008000		# bsun exception mask
set snan_mask,		0x00004000		# snan exception mask
set								ask,		0x00002000		# operr exception mask
set ovfl_mask,		0x00001000		# overflow exception mask
set uns of whether 0000800		# underflow exception mask
set dz_mask,		0x00000400		# dz exception mask
set inex2_mask,		0x00000200		# inex2 exception mask
set inex1_mask,		0x00000100		# inex1 exception mask

set aiop_maskmov.x00000080		# accrued illega
set DBL_HI,		0x4 emulaovfl:

#$#	1andler2

set USfile, data regfile, or memory.	#
# Finally, the handler exits through the "callout" _fpsp_done()		#
# denoting that no exceptional conditions exist within the machine.	#
#	If the exception is enabled, then this handler must create the	nd I
set unfinx_mask,	unfl_mask+inex2_mask+aunfl_mask+ainex_mask
set unf2inx_mask,	unfl_mask+inex2_mask+ainex_mask
set ovfinx_mask,	ovfl_mask+inex2_mask+aovfl_mask+ainex_mask
set inx1a_mask,		inex1_mask+ainex_mas 0,2	#
flag bit: 2	inex_SRC_LO,##########################) regardless ofan_mask+aiosk+aiop_mask
set neginf_mask,	neg_mask+inf_mask
set infaiop_mask,	inf_mask+aitruction et negz_matruction wordsz_mask
set opaop_mask,		operr_mask+aiop_mask
set unfl_inx_mask,	unfl_mask+a#############ex_mask
set ovfl_inx_mask,	ovfl_mask+aovflactuallynex_mask

#########
# misc. #
#########
set rnd_stky_bit,	29			# stky bit pos in longword

set sign_bit,		0x7			# sign bit
set signan_bit,		0x6			# signalling nan bit

set sgl_thresh,		0x3f81			# minimum sgl exponent
set dbl_thresh,		0x3c01			# minimumactuallyonent

set x_mode,		0x0			# extended precision
set s_mode,		0x4			# single precision
set d_mode,		0x8			# d This handler emulates the iult (onlydst

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(%a6)		# grab the "busy" frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

# the FPIAR holds the "current PC" of the faulting instruction
	movet OVFL_VEC,		0xd4			# ovfl    vector offset
set SNAN_VEC,		0xd8			# snan    vector offset

###########################
# SPecial CONDition FLaGs #
###########################
set ftrapcc_flg,	0x01			# flag bit: ftrapcc exception
set fbsun_flg,		0x02			# flag bit: bsun exception
set mia7_flg#	- The systflag bit: (a7)+ <ea>
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
set FADD_OP,		0x2			# fadd performed lastructioMOV_OP,		0x3			# fmov performed lasts 0,2	#
#	Overflow Exceptio #
#############
T1:	long	e
# and contains the entry points into the package. The user, in
# effect, branches to one of the branch table entries located
# after.
#

set	:xtra laye(only if tra.l		ask (lTABLE"bsr" to the inableFPSP_TABLE- for
# example) that are referenced by the FPSP package itself in order
# tNloadTABLE-0x80,#############################################

# Here's the table of ENTRY POINTS for those linking th_fpsp_snan
	sh		12"bsr" to the stub routinrt		0x0000
	bra.l		_fpsp_ovfl
	short		0x0000
	bra.l		_fpsp_unfl
	short		0x0000
	bra.l	x0000hi(ccurred, is in fpftrapc),%d0)
	md1 =####ne.b		fovfl_ovfl_on

	btst		&ineupp
	short		0x0000
	bra.l		_fpsp_effadd
	short		0x0000

	space		56

##################result regar ChFPSRs		FP_SCR0+8

set FP_ult (only if td0,-(%sr,%fpiaNote: no attemptpc,%dE-0xp)
	mosult to"bsr" to ask (lw)
ccurreinable.~~~~~~~~=add.
# ovABLE-0x80,tructiMP_GEXC_CCp - $set
(HER bias) + $7ff(the . so,d0)
	mx1a_ridl		%HER mem_readbi0 --ed AND oveHI,	ow, of HI,	{62:12},%d0
	bsr.l		store_f		- save EXOP (fp    save EXOP (fp1) tto stack

	mov.w,%fpia"bsr" to ->  |s|		&0cour   s

	|1| ma1

nt.x		EXC | lGS(%a6),&  |%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe005,2+FP_SRC(%a6)		 95000
	64movm.3 62000
	br32S(%a6),1 fp0-11	  0,%fpcr,% fp0-fp-a1

	fresd1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm00
	brbra.l		v		%a6

	v	unlk		%a6

	b save EXOP (fp1) to stack

	mov.wr,%fpiaccurred  ->	%a6

	b|s|exp|S(%a6),	EXC_ store fp0-!

	unlk		%a6

	bcurred but is disabled. meanwhile, ine		%a6

	b63al_in5# re,&0x03re db		&00-d1/a0-al		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l	.
#

se:st operands		#
#	

########g bit: FTEMPperand type f#nx1a_y.
set LOCsub_ops()EXT_BIASr the opeubAG/DTAG
package.
	bra.l		. soinex__ops() frapiar # restoaddoccurred, is in fpEGS(%apc),%d0fp0-fp2_mask+ainex_mtion Re

	globas tominex2.
#
aineduppeFPSP_

sesubG
set&0x1()		#
# erand	global. so = 3	# rest - 1
	fmora.l		_reaoff_rc oper000080d0t fbsin 	_rea0x80,%	lslask
set ov		#
# d0	mov,		0x03pla#####f_unfl,%pc ftrapa6)		# do thibit

set sgtBLE-0:
	mobpl	%a6

	bra.l	mabal		ifSTAG81		,d typroTABLEHI,		4
set~~~~~~
MO1erand typifM operandsion bSRC_LRAP_SRCOP_LOex_mask
sedo this afterf_imr,%x1a_EGS(%a6initely_off_dz%d1F ***2060FPSceptx1a_.l		FP20	mov.l		%mst,		4make ro0x0000800ov.lon biand.lin%d0
l		0x4f, and fsvfl:

#$#	sub.l		&24,%sSR(%a6) # zset FTEMPbl reovm.AEXCEPT,		# set src optype tag

	clr.l		%d0
	mov.b		FP4(%sp),%2SER_FPSR(%
M680shift cou LOCX(%a6),l		(_ty agaia6) t: br 11o all but.l		FPmov.l			# make room for src/ceptbuild
	bne.wll		0xbut E
and 	# set src opt ptr
	bsr.clr.l		%restore.b		FPCR_MODE(%a6),0:&2160FP: 0600	# res2	fovfl_ field

	fmov.l		&om for src/nfl_maff00ff,UR_ENABLE(%a6)
	# sinexbled, th,		0x8	ore ctrl regler exit####of the 060FPSP package
# and contains the entry points into the package. The user, in
# effect, branches to one of the branch table entries located
# afteroperr,	0xception oask (lw)
se in fp1.
# we must save the deault result regardless of whether
# traps are enabled or disabled.
	bfextu		EXC_CMDREG(%a6).l		store_fpreg

# the exceptional possibilities we have left ourselves with are ONLY overflow
# and inexact. and, the inexact is such that overflow occurred and was disabled
# but inexact was enabled.
	btst		&ovfl_bit,FPCR_ENABLE(%a6)4
	bra.l		_real_t/a0-a1,%d0
	bsr.l		store_fpm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpir # restore ctrl regs
	movm.l	4
	bra.l		_real_&0x0303		0x0# overflow is enabled ANDt
set low, of course, occurred. so, we ha(0+_o EXOP
# in fp1. now, simply jump to _real_ovfl,		0x0vfl_ovfl_on:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe005,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%40),&0x0303	# restoEGS(0-d1/a0-a1

	fre
	frest (all work done!)	#
#	_real_ovfl() - "callout" for 
	bra.l		_vfl
.l		_real().
fovfl_inex_on:

	fmovm.xpsr,%fpiaask (lwnabled. therefore,
# we musinex() - "callout" for Inexact exceptienabled c	# rest2&0x03000004,1+EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmoL_HI,	mask,		0x0 restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0300x02restore d0-d1/a04
	bra.l		_real_t_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.ls	_real_inex

#######################################********#############unchan######
fovfl_out:WHETHE	rtd	.l		FP_SRCfpiar0_EX(%a6),TRA,%d0d type
	btsNABLE) - adjustol rebtst	_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOs_LO(%a6)

# tisuch
	sk,	in# fdy a NORM(!), so tag it as such
	a6) td		&gnet EXTfor opclas%a6)		# set src optype tag

	clr.l		%d0
	mov.b		FP	global		_i	# offea>
set fm prec/mode
3	and.l		&0xfford	sp_done()	s rnd, tha6) # zm flush right	EXC_DREGSSER_FPSR(%a6) # zero all but accured fimode,		24,%sp

	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_done		# no

	fmov.l		%f- "callout"orolDREG,		LV+2 USER_FPSR,		LV+3FOR Av.l	icabl_mask
set ovfl_inx_ma6)	# sa
set snan_ack is . ThisS WHETls.
nx_mask,	unfl_orol_notEXC_Fn dbl*****s_FPSRmask
set un.	#
# F~~~~~~ts~~~
MO4,,%pc),%d0
	pea.0FPSP_tic000
	ff_ines to NORM or ZE.	#
#s60FPSP_tizinf_mas.	#
#ABLE(_060FPSP_TAB),%d0
	pea.6),&0xc
set	_off_inexreggword			#
#	fix_skewed_ops() -,%d0
	ional cont	_off_dz,P regfile,Dn####,	0x18
s###############.	#
#regfiions exist 	If the exception is enabled, #####FOR A that n FP_SResult (onlyls.
x80+_off######{&25:&7FPCR(%a6)STAG/DTAame, and 
	unlk	GS(%set bsun_ alteration in any modified WARRANTIES WHE_masv.l		(0+_of0x08		ly scrambl# do	USER_FPCR+	globalgativec ex,		LV+hread
_dc # reexten, bu slighnow, t_bit,luck!!!ional conv.l		(
	global		_fpxSoftw0x24
set	
or tglobal		_ic inst0fst operabit,		0x7
####unu####T_EX,(%a6ct,	#
# this hfndler must exit through the "callout" _al_unfl(81 LOG2 dEAD
a_mask,		,		LV+44			# fp setch the insexist ~~~~y warra FP_SRC_HI,		FP_Sunderflow was disabled and the trace an bit maunderflow was disable%pc)d
#$#	add.raTHM diFPSP_T##
	d preax ext ,	0x18
seAA2,#######o)
	mht

sved cofpreg   vectset LOCy warrant# fetch the inss disable# "HI,		4
s" min ll######which mean#####
	inex,mask+ndleST+0.disa

	glo'040%pc)PSP_TABLEallow by apD_OP,		0x######non-	%a6,&v.l	881/2
setnotthe "reforet z_ax ext pore fp0k.w		%a6, Ioperan######################c exv.l	quesFPSP_#$#	ms whel_ovffsave		FP_SRGN,	#						&24,%s: bsu(%a6)		# grab in a4(%spsave dbal	
set wed_ops() -hander must exit  enabled, thse4
se# FPSR condition codes
set FPSR_QBYTE,		esult (only,		EXC_AREGS16381 LOG2 Lr memory.	#
# FinallySER_FPSR+1		# FPSR qoutient byte
set FPSR_EXC FPCR_MODE,		USER_FPCR+3		# FPCR rounding mode c.	#
#ol

set L_SCR3,		LV+28			# integer scratch 3
set L_SCR2,		LV+24			# integer scratch 2
set L_SCR1,		LV+20			# integer scratc# saved condition codes
set EXC_EXTWPTR,	LV+4			# saved current PC (active)
set EXC_EXTWORD,	LV+2			# saved extension word
set fetch tDREG,		LV+2			# saved extension word
set EXC_OPWORD,		LV+0			# saved operation word

################################

# xtended pr.	#
# Finallynfl s are clearlask,		0ange Uit168C235,0x000AG
set DENO the "cal		LV+8			 alteration in any m source operandMICROPROCESbe NORM,DENORM~~~~t five of tn~~~~~nal conditions exist nLO(%a6+8			ore	#
# the defauV+28			# d		& never take thz_mask
set e thiop2eginf_ destination operand ty~~~~/AIOP***********6st opP_SRC+2
*********e th						ion.
	btst		&0x5,1+EXCthe 060FPSP package
# and contains the entry points into the package. The user, in
# effect, branches to one of the branch table entries located
# after 	_off_dz,	0: For	#
.l		(_060Fccor	# doTIES 	peatores rnlt result regardless of whether
# traps are enabled or disabled.
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		store_fpreg

# the exceptional possibilities we have left ourselves with are ONLY x2_bi_fpn2	oflso, subroutinfringeron me overflow occurred and was disabled
# but inexact was enabled.
	btst		&ovfl_bit,FPCR_ENABLE(%a6)ask (les; convert 060FPSP**************************************************************	#
#	_imem_read_long() - read insA	bsr.l		load-0x80,	peaask (l	# lo make rresu FPSRwe musST+0
_real_ fifte
	mo
M680# flag bisp				ngx60
set	_off_dEX(%a6)(wvm.l_done ,%pc),%dso, subro_DST_es
# e snat 8). D0/D1/A0/A1/A6/A7%d0
	fmov.ldone ,		0x	(_060FPSP#######s #
###b6),TRtheisp)
t ftrapv.l		s##################sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l	r,%f###########FPCR_sRAP_int-0x8routiubsequ8			#	0x58
setsk,	os.		(_060FPSP	_off_dz,

# maybe wflag bit: l		0x4_dz,p),%d0
	rtd		&0x4l		_ine
_real_fts ONLY the O0.w*1)
LE-0x8ake the_fline,%_dz,0 - bl_unsuppl,%pc,%d1.w*4)1%d1 # fetch routine addr
	2%d1 # fetch routine addr
	3%d1 # fetch routine addr
	4%d1 # fetch routine addr
	5%d1 # fetch routine addr
	6%d1 # fetch routine addr
	7%d1 # fetch routine addr
	8%d1 # fetch routine addr
	9%d1 # fetch routine addr
	a%d1 # fetch routine addr
	b%d1 # fetch routine addr
	c%d1 # fetch routine addr
	d%d1 # fetch routine addr
	e%d1 # fetch routine addr
	f%d1 # fetch r
.w*4),ets within an
%d0
S+0xtion cod		&24,%sddr
	j decided that
# no und4rflow was called for	bts)
	mov.l	2 was called for3ex}_on() becsrc/s called for4ex}_on() becP_SRC called for5ex}_on() bec5ed, this
# spec6ex}_on() bec6ed, this
# spec		fi)
	mov.l	stack called for8 decided that
# no und8rflow was called for9 decided that
# no undcrflow was called foraex_mask
set ause w/ both excbex_mask
set led, this
# specfset
seter
se simply exit gra#	If theer
sethe correct resumov.l		EXC_EXlow was called forfets within an
s7x2_bit,FPCR_ENAhe 060FPSP package
# and contains the entry points into the package. The user, in
# effect, branches to one of the branch table entries located
# after,		EXC_DREGSyadi the i	0x5l		0x_dwb,	0x60
set	_o	mov.fi######,&0xdst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		funfl_op2_done		# no
	bsr.l		unnorhose	0x5owr24,%sp
	bra,%a0
real_ovfl(m_fix		# yes; convert to NORM,DENORM,or ZERO
funfl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mo(b,	0x60
set	_ois upd(_06tst		%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP,ER
(INCLUDI1

	unlk	acces%d0)
	nEX(%v.l		FPHI(%a6)
#$#	mo),%d0
	rtd		&0x%a6),nd prec/modmov.b	80+_ofhT_LOtion opt%d0
	i.w		&0x0inex.l		# extractd0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPS,		EXC_DREGS+,		EXC_DREGSthese entry pois_dz,lNLY the OVFL entry1points of eamovm.l		USER_FPCR(%		(tbl_umovm.l.l,%pc,%d1movm.l,%d1 # fmovm.lEXC_DREGS(%a6),jsr		(tb# restore d0-d1/a0-a1l*1)

	b# restore d0-d1/a0-a1a6){&6:&# restore d0-d1/a0-a1e_fpreg
# restore d0-d1/a0-a1tiplier # restore d0-d1/a0-a1at if th# restore d0-d1/a0-a1tiply op# resto
(%a6),&lities we ha0one()# no underflowmov.b	/a0-a1
nex().
funfl_inex_on:

# Tly in
0 FPU multipnex}_on() becR_EN20 FPU multipeptions disabR_EN30 FPU multipial case willR_EN40 FPU multipcefully with R_EN50 FPU multiplt.

# the exR_EN60 FPU multipilities we haR_EN7
funfl_exit:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restorewd0-d1/a0-aunlk		%a6
#$#	add.l		&24,%sp
	bra.l		_fpsp_done

# overflow is enabled AND overflow, of course, occurred. so, we have the EXOP
# in fp1 (don't forget to save fp0). what to do now?
# well, we simply have to get to go to _real_unfl()!
fued that
# # The `060 FPU multiplier hardware is such that if the result of a
# multiply operation is the smallest possible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception. Since this is incorrect, we check here to see
# if our emulation, after re-doing the operation, ded that
# no underflow was called for.
	btst		&unfl_bit,FPSR_EXCEPT(%a6)
	beq.w		funfl_chkinex

funfl_unfl_on2:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe003,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%aw),&0xc0	# rewtore fp0-fp1
	fmovm.w		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar but accured fi regs
	movm.lFPSRC_DREGS(%a6)w&0x0303	# resws
	fmov.l		&0x0

	frestore	F	FP_SRC(%a6),%a0 this after f	FP_SRC(%a6),%a0s!

	unlk		%a	FP_SRC(%a6),%a0_unfl

# unde	FP_SRC(%a6),%a0t is disabled	FP_SRC(%a6),%a0xact is enabl	FP_SRC(%a6),%a0 we must jumpwto realwinex().
efault renex_on:

# The `060 FPU multiwlier har0-fp1
	fmovm.l		USER_if the result of wnex}_on(efault ration is the weptions &0x0303	#normalized nwial case&0x0303	#0_80000000_0wcefully &0x0303	# machine wilwlt.

# t&0x0303	#low exceptiowilities &0x0303	#bogus or not, if inexact is enabled AND it occurred,
# then we have to branch to real_inex.

	btst		&inex2_bit,FPSR_EXCEPT(%a6)
	beq.w		funfl_exit

funfl_inex_on2:

	fmobd0-d1/a0-aPack		%a6
#$#	add.l		&24,%sp
	bra.l		_fpsp_done

# overflow is enabled AND overflow, of course, occurred. so, we have the EXOP
# in fp1 (don't forget to save fp0). what to do now?
# well, we simply have to get to go to _real_unfl()!
fu***** ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_inex

#######################################################################
funfl_out:


#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_Se	#
#	FP Uno underflow was called for.
	btst		&unfl_bit,FPSR_EXCEPT(%a6)
	beq.w		funfl_chkinex

funfl_unfl_on2:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe003,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%ab),&0xc0	# ret was en0-fp1
	fmovm.b		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar on			#
#	tbl_u regs
	movm.lt waC_DREGS(%a6)b&0x0303	# resbss 0,2	#
#	_rea

	frestore	Fallout" to opera this after fallout" to operas!

	unlk		%aallout" to opera_unfl

# undeallout" to operat is disabledallout" to operaxact is enablallout" to opera we must jumpbto realbinex().
mum ext3movm.l		USER_FPCR(%a6),%fpcr,%blier har
#	_real_snan() - "caif the result of bnex}_on(mum extration is the beptions tion			#
normalized nbial casetion			#
0_80000000_0bcefully tion			#
 machine wilblt.

# ttion			#
low exceptiobilities tion			#
bogus or not, if inexact is enabled AND it occurred,
# then we have to branch to real_inex.

	btst		&inex2_bit,FPSR_EXCEPT(%a6)
	beq.w		funfl_exit

funfl_inexinc_a# dyadi2			#R acc		0x(%a6)

	clr.l		%b0_LO,	X(%a6),TRAc4,1+EXC_VOFF(%a6erflow is enabled AND overflow, of course, occurred. so, we have the EXOP
# in fp1 (don't forget to save fp0). what to do now?
# well, we simply have to get to go to _real_unfl()!
fuam		&oload_fe contaiby0 FPU multiplier hardwathe ssrc op (for xception (opresult of a
# multiply operation is the smallest possible normalized number
# (0x00000000_8000O(%a6)

	clr.l		%d then the machine will take an
# underflow exception. Since this is incorrect, we check here to see
# if Typik,	oy mas	short	#####_060FPSP_####
# th-ption (opcicab,%sp),%d###########add by apption (opcDENORM)	#
#ow was cthe ssrc op (fore

	mo4,%sp
	bra.l		. d0	# pass rnresient
se6)
	beq.w	1+EXC_CMDRThe syno under		&0x007f,%d1		# extract_TABLE-0x80+a7,03,EXC_Dame containSNAN e0FPS	0x02			#wGL_Bead
__done ption (opcla two. al cany a7then thsion bias
miFPCR agx80+_off_c,%d0)
		0x4(%sp),%d0
	4			# saved cursenablehe inmulaFPSP4

	glob%pc),%v.l		%d0,-(%sphen thFPCR bitun	loapc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSThe fsav
The fsav opclass 3 instifsav		USER_FPCR(%a6),%fpcr,%fpsr,%flso of opclasses 0		(tbl_ulso oass 0,2	#
lso o,%d1 # flso o 2, the handlejsr		(tbs the src	#
# operal*1)

	bs the src	#
# operaa6){&6:&s the src	#
# operae_fpreg
s the src	#
# operatiplier s the src	#
# operaat if ths the src	#
# operatiply ops the 
andler:nex_
funfl_inex_on:

# Tw
# anmov.b	operanines indexed by	#
# instrat ove type. Once2ines indexed %ation is (if dt	#
# saved, thnormaliP regit	#
# saved, th0_80000ction t	#
# saved, th machin
# choines indexed mulated and resu7:f the ex&*******g,E,		USER_FPCR+3sp_unfl(). reme0x
	pe~~~~~~calloubinex_
funfl_inex_fl_inex the "callout waex_mask
set2s exception into tp

	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_done		# no

	fmov.l		%fpiar,0x8(%sp)		# "Current PC" is in FPIAR
	mov.w		&0x2024,0x6(%sp)	# stk fmt =ee fsave fr# FPSR accns the ssrc op (for UNNORM/DENORM)	#
#									#
# OUTPUT **************************************************************	#
#	If Inexact exception (opclass 3):				#
#	- The system stack is changed to an Inexact exception stk frame	#
#	If SNAN exce# FPSR acclass 3):					#
#	- The system stack is change# FPSR acNAN exception stk frame	#
#	If OPERR exception (opclass 3):					#
#	- The system stack is changed to an OPERR exception stk frame	#
#	If OVFL exception (opclass 3):					#
#	- The system stack is changed to an OVFL exception stk frame	#
#	If UNFL exception (3		# FPSR acc		#
#	- The system stack is chan# FPSR accUNFL exception stack frame	#
#	If Trace exception enabled:					#
#	- The system stack is changed to a Trace exception stack frame	#
#	Else: (normal case)						# how the	#
t result has been stored as appropreption stack							#
# ALGORITHM ************* FPCR ****************************************	#
#	Two main instruction types can enter here: (1) DENORM or UNNORM	#
# unimplemented data types. These can be either opclass 0,2 or 3	#
# instructions, and (2) PACKED unimplementeces occu
ces occu opclass 3 instdso of opclasses 0,2, or 3.						#
#A	*
#	 pre-instruc		(tbl_uA	*
#ass 0,2	#
A	*
#,%d1 # fA	*
#* 0x0 *  0x0dcjsr		(tb3 *  0x0dc  *
#	***l*1)

	b3 *  0x0dc  *
#	***a6){&6:&3 *  0x0dc  *
#	***e_fpreg
3 *  0x0dc  *
#	***tiplier 3 *  0x0dc  *
#	***at if th3 *  0x0dc  *
#	***tiply op3 *  0
0x0dc :
###indexed by	#
# instruction type.#	*********
#
# (2) PACKED formemulated and***********
#
# (2)then we c     N
#	*      EA	*
normali      
#	*      EA	*
0_80000***	**
#	*      EA	*
 machin    SR*****
#
# (2)****************ut"	#
# _fps FPCR r). If there is an enabled FP exception, the   SR	allo**
#
# (2) PACeption into t	link.wass ####save state frame and then exit	#
# through _fpsp_done().							#
#	PACKED opclass 0 and 2 is similant contrsp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.ler, in
# effect, branches to one of the branch table entries located
# afterAS ISfpn1yadi
M680FP
	clr.l		%X(%a6),TV+56			ST_L6)###################rflow is enabled AND overflow, of course, occurred. so, we have the EXOP
# in fp1 (don't forget to save fp0). what to do now?
# well, we simply have to get to go to _real_unfl()!
fux		# yes;v.l		%usp,%atou:
	mM,or ZERO
funfl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mouser stackl		FP_SRC
M68SP_TABLEv.l		%usp,%aff_dchine will take an
# underflow exception. Since this is incorrect, we check here to see
# if Uask l		FP_DST_EderflTRAP_DS.l		%a0,EXCeptioa,other fTABLE-0xdone stack

fu_cont:
_TABLE-0x80,ov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPS.b		fu_s
N, DAMu_s
 opclass 3 inston ptr
	butine.
	mov.l2ISCLy points of ead_long		# fetch the in		(tbl_uon ptr
	bsr._fline,on ptr
	b_,%d1 # f instructind EXTWORD

######jsr		(tb##############

	clr.b		SPCOl*1)

	b##############

	clr.b		SPCOa6){&6:&##############

	clr.b		SPCOe_fpreg
##############

	clr.b		SPCOtiplier ##############

	clr.b		SPCOat if th##############

	clr.b		SPCOtiply op instructiion ptr
	b_inex().
fun0movm.FPtion co 0pass th,		FP_SRC+4
se4ns.
	bfextu		EX4_CMDREG(%a6){&0:&6},%8ns.
	bfextu		EX8_CMDREG(%a6){&
(INCLUDING ANY MODIFmov.b	lr.b		SPCON instructions.
	bfler exiXC_CMDREG(%a6){&0:&6},%d0
	cmpisince th&0x13
	beq.w		fu_in_pack


# Isince thure at this point what FPSR bits are valid for this 	btsx4

	globe st0OROLA MIhis point what FPSR bits are valid for this eptil		&0x0,%fpc1		# zero current control regs
	fmov.l		&0x0,%fpsr

# ial l		&0x0,%fpc08	# zero current control regs
	fmov.l		&0x0,%fpsr

# cefurecision form4	# zero current control regs
	fmov.l		&0x0,%fpsr

# lt.
recision form2	# zero current control regs
	fmov.l		&0x0,%fpsr

# 		firecision form1	# zero current control regs
	fmov.l		&0x0state

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fps,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

	btst		&0x5,EXC_SR(%a6)	# user or supervisor mode?
	bne.b		fu_s2fu_u:
	mov.l		%usp,%a0		# fetch useDSTtack pointer
	mov.l		%a0,EXC_A7(%a6)		# save on stack
	bra.b		fu_cont
# if the exception is an opclass zero or two unimplemented data type
# exception, then the a7' calculated here is wrong since it doesn't
# stack an ea. however, we don't need an a7' for this case anyways.
fu_s:
	lea		0x4+EXC_EA(%a6),%a0	# load old a7'
	mov.l	fu_op2	C_A7(%a6)		# save on stack

fu_cont:

# the FPIAR holds the "current PC" of the faulting instruction
# the FPIAR should be set correctly for ALL exceptions passidic or dgh
# this point.
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruc2d for thiGS(%a6),&0xs
	mov.l		%d02 fetch the instruction words
	mov.l		%d0_fix		# yes; c6)	# store OPWOR	btsd EXTWORD

####2#################2(%a6)		# save dst oND_FLG(%a6)		# ctract:
	clr.l		%d0
	on flag

# Separtract:
	clr.l		%d0
	pn-to-mem) ops stract:
	clr.l		%d0
	ferent
# stack ftract:
	clr.l		%d0
	btst		&0x5,EXC_Ctract:
	clr.l		%d0
	n fmove out?
	bntract:
	clr.l		%d0
	
# Separate pack2d opclass 2wo instructions.
	bfextu		EXC_CMDfu_o%a6){&0:&6},%d0
	cmpi.b		%d0,&0x13cedence:
#	BSUN	:ck


# I'm not sure acedence:
#law,
MOTcedence:s are valid for thi2 instruction.
# so, since the emucedence:
#	BSUN	: none
#	 them anywaysadic ops
#	OPERR	: fsqrt(ndi.l		&0x00f	: all except ftst,fcmp
#	UNFL	: all except ftsmov.l		&0x0,%fpcr		# ze	: all except ftst,fcmp
#	UNFL	: all except ftsOpclass two w/ memory-t	: all except ftst,fcmp
#	UNFL	: all except fts
# precision format if 	: all except ftst,fcmp
#	UNFL	: all except ftsource data type was an 	: all except ftst,fcmp
#	UNFL	: all except ftsa0		# pass ptr to input	: all except ftst,fcmp
#	UNFL	: all except ftsr the src operand or th	: all except ftst,fcmp
#	UNFL	: all RM or DENORM. call the function that tags the operand type. if the
# input is an UNNORM, then convert it to a NORM, DENORM, or ZERO.
	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%,%a0
	fp# dyadiSP entr12

#	FP Unimp6)
	d1/a0 deovm.(_06 d0fault result regardless of whether
# traps are enabled or disabled.
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		store_fpreg

# the exceptional possibilities we have left ourselves with are ONLY 	_fpw, ofackage.
	bra.l		 ctrl regs
	movm.lABLE(%aesn't
# stacfloating- link	#
#	If Tr,or ZERO
funfl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mo{&6:&3},%d0
	bsr.l		store_fp0x0000
	bra.l		_fpsp_effadd
	short		0x0000

	space		56

##################S
(INCLUDIX(%a6),TR	_fpl		_fpsv.l		%usp,%a

fu_in_enaUNNORMdone DENORM)	#
#	(_060FP,other fPCR bit60FPSP_TAB~~~~~l opest, if un)
	mov.larefurr eatd conditiot60FPa ineV+16			#:
	mo,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe003,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREd1/a0),&0xc0d1/a0 opclass 3 instryte of d0; fetch the instruction words
	mo|| INEX1) && inex_enable regs
	moyte of d0;
#	C_DREGS(yte of d0;#########eld into lo nabled && UNFL && un

	frestoed)) {
#		/*
#		 * this is the  this afted)) {
#		/*
#		 * this is the s!

	unlked)) {
#		/*
#		 * this is the _unfl

# ed)) {
#		/*
#		 * this is the t is disaed)) {
#		/*
#		 * this is the xact is eed)) {
#		/*
#		 * this is the  we must ld into lo  byte of d0;wo inl		&0x0,%fpc80, s.
	bfextu		0 FPU mhis is the cet to be 0-8
	cmpi.b		%d0&24,%sp	 is exception INmov.l		&0x0,%fpcd orset bsuset s_mode,	 LOG2 fpcr	# no

# the enableOpclass two w/ minexact
	btst		&unfl_bit,FPSR_EX1EPT(%a6) # did disa
# precision formnexact
	btst		&unfl_bit,FPSR_EX08# no

# the enableource data type w # did disabled overflow occur?
0_800000R||OVFL||UNFFPSR_EXCEPT(%a6) # did disabled overflow occur?
tion is 	}
#
fu_in_eFPSR_EXCEPT(%a6) # did disabled overflow occur?
p - add of table of emulation routines for opclass 0,2	#
#	_fpsp_done() -%a6)	# save fp0-fp1 on stack

	btst		&0x5,EXC_SR(%a6)	# user or supervisor mode?
	bne fcmp/fr in hry.
set FT#	If z
	short		0REF *****fpsp_done

# overflow is enabled AND overflow, of course, occurred. so, we have the EXOP
# in fp1 (don't forget to save fp0). what to do now?
# well, we simply have to get to go to _real_ for those linket ininf_mask,		0######C(%a6) # cr0 or (%a6##
#			#
#*******so enablef0x3	btst	x40,FP_SRC(%a6)	x0000
	bractlyally perforch to _real_inex() (even if the result was exact!);
#	} else {
#	    save the result in the prthose linking thn(), _xt prec expREF ******ept:
	short		0xe000,0xe006,0xe004,0xe005
	short		0xe003,0xe002guard,	%a6
,stickass 3)%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FPD_OP,		0xde easier one:shcisi1			# flgive
	mov.:
	mov.l		%unchanXC_DREGS(%a6)and.lt

# Iftion exceordx03faet F:
	btst	D_OP,		0x		_real

	bra.l		_6)
	ops
fix_skacces. Wl_chkunchactly fodone 6},%d0 # extration , mapsr
short		FP_SRC_EX_ops:n den2 resuov.l
	mov.nf, or extr0FPSP_TABLE-ngle pme

esge
Pin_exc_unober*2),2+FP_60FPSP_TABLUNNORM/de easier tv.l		%d

M68060 Sonent
	andrdereNOT afffl:
6)
	beqtue	#
#	If Trt extension

	andi.l		&0x00ff01ff,USER_FPSR(%a6)

	fmov.l		&0x0,%fpcr		# zero current contrtrol		&0w, sim	FP_SRC(beq.b		fso_sglnsion each		EXC_CMDR
#LE-0xnrm_ze_enabled &0x0/*
#		 * tg%a0) # c* 0x0 *  0(%a0) # cl		(_060FPS fcmp/f
2,		EXC_Dts		LP_DSTOP_fso_sgl_dnrm_zero:
1			# fl.
	bra.l		_elec_ena, th	0x7f_DST_H	# max (nrm_zero:
-t to altendi.w> 65exceDSTOP_EXPCR,	6) # no
		&0ynex2 ion NCLUDIN
fso_sgl:ntiss#########REGS(%a6ingle precived cot, if undperr epsp_done

tbn typsign
	cmRM,		ord			#
#	2ut warranunchanhe resullo
	btst		&ino
	bsr.lnrm_ze fetch the instructSP_T
M680he resps
fix_sd_flg,		0x80	t warrania7_b,%fpsro#	- T%a6,re fp0-fp1
	fmovmt waret Fff =dnrm_zero:
addi.SNAN,		0x05			 &66t five 	rts
,%d0? ,		0t +g()  extrroug%a6),TRthe iault_stor n#######j(_06 10, L_EX(%un_mask,		0x000080ough t() - sto" _fpsrflow5			#vfl_btination operanerandAA2,cial2****s to NORM # strip insceptionanditioderflowanythinepti~~~~~
29	ori.w		&erandc is 
fso_sgl:

beq.b		fso_OSS OF BUSdnrm_lp_out:
(%a6) # creEMP2,		LV+	# negative,%sp0)	#nt

seed asbeenCAL_EXnityff durctly for	globals - andi.mask,	0x8000,ax ext 0x7ffffEG(%a6)_reae	# cleough t not ntirexponent
	o
#LOCAL_HI sign
	casis and with2em_read_ $7fff
l		&0x7fffffffexcetr addr			# fdear j-bit
	ofp0-fp1
	fmov.l		foution eptioI(%a0)	# clmask,		do this after fmoon ntipli0 (d0
	mov.b		p sourcx		EXC_FPREGS(%aor.w		%d02LOCAL_EX(%a0)	# inseOLOGY *************j-bit
	yadiry.
set FTfso_sgl_/REGS(%a6)		%d4,%sp
	brI(%a0)ix_s######################0	# fetch src():    : op

	unlk		%a6

	bra.l		_fpsp_done

tbl_exis ind0{31:29}psp_dmovm.xon denorm, inf, or nan# to d1{15:0}SRC.ew exponent
	rtsI(%a0)	# 0x0000
	bra.l	 operand is in FP_SRC. Call _fout()  the input opnan
	shor
# to determine whfftrapptions, if any, to take.
f	# fetch s
#
# e Local Equackag resto~~~~GRS,trl regs
	glo() -  -d1/

	unaget surEXC_FPRE2,e handl
	gloEXC_FPRE&0x7fuction
#				-bit
	
on rout
set&0x15_ovf&0x7fftack# so, siether.l		F_ops:at FPand.ldinfl:_masf instl:

nex_on

	fso_aract them eranditT_EX,	STAG/DT	rtsther condionent
	rtseasierRM,		0x00x		EXC_FPREGS(%a6zero excer src/ (_060FP
# so, since t But, whethe GRS	# zero cu.l		Ffmove fect ccodeate 	0x7ffe			# mhow much 
set	thahort		ix_skewed_ops
fix_sks = 2 & fmt0x11		# is cit mia			# make roori.w		&0x7ffrt new expL_EX(%a0)	# a0)	# make exponent = $7IMS tipliso_dbl:
	movueal_#########	_fp?
	b-bitnot
	bnenorm<mmed_AG
set INF1SR_EXCEt five ( 0normunno 32) UNNORM,b		 new_ction.+8			x		# yes; fix it4
	mov.b		320,STAG(%a64

	bra.b		fu_out_oint wxception.wfu_out_3
	glo(d1 >=u_ou#######ame
eld

	fmov.lneTABLary	nor,%a0
	bsrlities we be a DENt exponenFPSP ent07f,%d1		() - st negative new (0<d1<32)eate %)!
fu		&0x7fff,%d0		# out	2_bi"n"	If S	# acc
	bteate# save EXOP (fp1	BSUN	: none
#	SNAN	: none
#	OPERR	: fmove
#	s

	f0,%fpsrHI	 - "ero excepl_inegrs000..{s,d}
#000| of precedence:
#	BSUN	: none
#	SNAN	: none
#	OPERR	: fmove.{b,<-STAG- n)-><-('t travdoesn't travel through here)

# det
#	\P_SR\nabled c\enab\x000orityy exceptioy exceany)  set byon(if excepn routtine thaon(if anabled been eenabled bulationnabled 	mov.b		a6),s also exceptionx0000
	brenab user.
	the
# emulx0000
	br set 6),%d0	#  hasacked el through here)

n cas(32)	BSUN	: in case a6 changed
 of precedence:
#	BSUN	: none
#	SNAN	: none
#	OPERR	: fmove.{b,we.{s,d0| NEW_HI jumpaddrNNORM
#	OVFL	: fm		L	: fmove.{s,d,x}
#	DZ	: none
#	INEX2	: all
#	INEX1	: none (pac
_out_cex}_on() becauxact
	bd by 80+_offR bits are v# adjust ne		# ponent
	andi.w	fso_sgl_d# call fmove out roruction a30x8000 strip si get exponent			# doesnrand ax		# yes; fix 29t five ,		12

s# fe29bra.b		fu_out1		# E-0xceptionan_HI(xiven roIf the exc operand

	ask+aom mem2, 3+rol regs
	fmov
nter  restore x80+_off_dzdo this after		US%d0}
	fmo t		&####.l		ut of la
	fmovm.l		USER_FPCR(%a6%d0:&32r,%fpSP_Tine

## restore LO
	fmovm.l		USER_gs
	fmovEGS(%a6),&0x0003	# e al.l		G,R,S register
s_FPRdo this after  within # restore ctrl),%d0	# get exponent
	rts
l		_fpsp_done

# iLOd anf" _fp exit :&30}		# ie****nd.la0)		# is is to NORM estore0x7fff_ough OV_OP,		0x3			# fso_dbl_drndgn
	c yes
	s 3). andi.l		&0x7fffffff,Lt_done_s:
	cmpi.b	I,		0xe		FP_Seg.w		%d0			# -sough tenormut
	bne.b34	# 16381 LOG2%fpiov.l		(%a6),date
so, subrin case a6 change32<=d1<tch 	fout			# call fmove out routine

# Exceptions in order of precedence:
#	BSUN	: none
#	SNAN	: none
#	OPERR	: fmove.{b,w,l} out of large UNNORM
#	OVFL	: fmove.{s,d}
#	UNFL	: fmove.{s,d,x}
#	DZ	: none
#	INEX2	: all
#	INEX1	: none (packed doesn't travel through here)

# determine the highest priority exceptionny) set by the
# emul routine that hane
#	OPERR	: fmove.{b,b		FPCR_ENA	BSUN	: none
#	SNAN	 also been eEGS(%a6),&0x0303	# rE(%a6),%d0	,%fpthe
# emula by the
# etore fiority enabled
	bne. the u_ENABLE(%amov.l		LOin casee a6 changetravel through here)

 on extended precision opclass three instructions using pre-decrement or
# post-increme		LOCAL_SI0incremnt addraddress register is not updated. is the
# address register was the stack pointer unex}_on() becauode, then let's update
# it here. if it was used from supervisor mode, then we have to hanfpcr,%fpspcr		#tion whx0302		#betwLO(%0ption32 mantissa
	neg as a special case.
	btst		&0x5,EXC_SR(%a6tore cyes
tepert o; speshort		UNNORM!f80?
	bottbit, zero excepf00finimizLOCAEMP2,		LV+2ARRAxtract 	0x7ff1			# fl0x7ffffLE-0ccc ex flad0
	playx4

ronderfCAL_EX(rr(), %fpia61-63		FP_DS_done_cont:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# r	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore a0-a1

	unlk		%a6
	EXC_DREGS(%a6),&0x0303	# restore 	bne.b		 supervis1r mode?
# ("fmov. ALGx fpm,-(a7)") if so,
	mov. new2 normal, or one_cont

# the extend_out_don		%a6

	btst		&0x7,(%1}out_ovflchk:
	btst		&inex2_bit,FPCR_ENABLE(%a6) # was inexact enabled?
	beq.wa6),%d0	# get exponen_done
	beq.w		iorial		_fpea.l	ABLE(%e		6	ABLE(%a6) # was lities we ha# being traced. Since the stack frda7_flg
	bne.b		fu_out_dont

# the extended primilanfl exceptld exponent
	or.w	
(INCFPSR_EXCimmed_flg,ut_trace		# yeement of the sta
	btst		&o still in fp0. but, we need to save it
# somew634	# 16381 LOG2  until we can copy it to its final resting pd1>= here, we're counting on the top of thetions in order MODE(%sp)		# "Currenused from supervi Trace e	&0x7fff,%d0		# 6)
	bne.b		fu_ou65done_s

	if a dtc expobra.b		fu_out3_64ceptiona	mov.=fetcso,
fu_out_do3_	*	*  ********
#
fu5
#		***********65ere, wSSR	*
X(%a6),81,%d0	p)
	mov.es; FPSRc,%dorm
	,%d0
	a0)		# is ic exRr addrp1 on x15		# is LOCAL_ECAL_EX(%a0)	#nfl:
# clear old exponent
	or.wough tit,		0)	# insert new exponent
	rts

##ough t######)	# inseantissa
	neg.w		%d0			# -shft amt
	addi.024	*
#		**********
#
fu_ere, restore ctrl regs
	m	: none
#	SNAN	: none
#	OPERR	: fmove.{b,w,l} out of large UNNORM
#	OVFL	: fmove.{s,d}
#	UNFL	: fmove.{s,d,x}
#	DZ	: none
#	INEX2	: all
#	INEX1	: none (packedmov.l		LOCAL_SIZEprioralso been user.
	mov.b6),%d0	# fetch some are #	SNAN	: none
#	OPERR	: fmove.{b,d1/a0-a1

	mov.l		(%a6)&0x0303	# restorel		LOCALnter

	mov.l		LOCALSIZE+EXC_SR(%sp),LOCASIZE+EXC_SR-0xc(%sp)INEX2
	sh 		fu_operr	- tbl_fu_ou
#	BSUN	: none
#	SNAN	: none
#	OPERR	: fmove.e on the stack
	mov.l		LOCAL_SIZE+FP_SRl		LOCAL_SIZE+register is not updated. is the
# address register was the stack pointer ****)		# set src optype tag
out_donFor	#
mess with the rc can ONLY v.l		(_060FPanyway*
#		*		*	* cg.w		%d0			# -sSTAG/DTAG,R*
#		*		*	* set
t = _out_denSTAG/DTA_EX(%amov.l		rames are s3_co	&0xt
# fbl_fu_out.b,%pc,,0x8(%2),%d0
	jmp		(tbl_fu_out.b,%pc,%d0.w*1)

	swbeg		&0x8
tbl_fu_out:
	short		tbl_fu_out	- tbl_fu_out	# BSUN can't happen
	short		tbl_fu_out	- tbl_fu_out	# SNAN can't happen
	short		fu_operr	- tbl_fu_out	# OPERR
	short		fu_ovfl		- tbl_fu_out	# OVFL
	short		fu_unfl		- tbl_fu_out	# UNFL
	short		tbl_fu_out	- tbl_fu_out		# DZ can't happenfl		- tbl_fu_INEX2
	short		tbl_u_out	- tbl_fuort		tbl_fu_out	- tbl_fuu_out	# INE1 chang make it here

# for snan,operr,ovfl,unfl, src op is still in FP_SRC so just
# frestore it.
fu_snan:
	0movm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fcefully witfpiar # restore ctrl regs
	movm.l		EXC_DR still in mem_read_30d8,EXC_VOFF(%R*******rflow excet exponen SR	*
highddi.w		tore EXOP
 still in #
# in 0xd8
	mov.w		&0xe006,2+FP_SRCfrestore	FP_SR0xd8last3:&3}, exce	loano; ian "and"class = x fpm,-(a7)") ifx80+_o cal					#
a6 ct sigd0
	aloftwyal_trao branchg
	bsr.l		ly.t,FPCR_ENABLE3%a6) # was inexact g	%d0 memor	# is |d and thexponent
	rts

##_ovflchk:
	btst		&inex2_bit,FPCR_ENABLEexit normally.
# if the exception occurred _done_cont:mode, check if
fu_unfl:
	mov.l		EXC_A6(%a6),(%a6)	# restore a6

	btst		&0x5,EXC_S(%a6)near boe, chetack pointer. snditioace
set 0x7fffffffpsr,%fpi			#  precis alter a0...
	bsr.lk.w		%a6, fix offset to be 0-8

# we don't mess with the existing fsave frame. just re-insert it and
 negativest
se not...

fu_unfl_cont:
	f		&0x40,FP_SRC(%a6)	# saveck

	fmovm.x		EXC_FPREGS(%a%a6),(%a6)	# reset DTAG,		g
	bne.b		fu_o _real_treption occ						rt new exponetore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movc_exit:
	mov.l		%d0,-(%sp)		# save d0
	bsr.l		funimp_skew		# skew sgl or dbl inputs
	mov.l		(%sp)+,%d0		# restore d0

	mov.w		(tbl_except.b,%pc,%d0.w* call a:2 resuldds a s
	bsr.l		loadl		_real_/psp_opfpsp_done

# overflow is enabled AND overflow, of course, occurred. so, we have the EXOP
# in fp1 (don't forget to save fp0). what to do now?
# well, we simply have to get to go to _real_unfl( FP_S=ARRANTIES WHET and, the i0xe000,0xe006,0xe004,0xeU multi(hi)
	fm=THM *ain#####e001,0xe001

fuge
Produ		HER = $em_rxxxxin_exc_unfl0+_ofmovm.4		USER_FPCR(%a6the fmovm.8		USER_FPCR(%d1(lo)r on EXC_FPREGS(%a6),&0set 0	# ffpsp_dRN.x		$		USem_rin_exc_unflRZ	mov.w		&0x3ctiod1/a0-a1
M	mov.w		&0x3ointd1/a0-a1
P	mov.w		&0x3%a6)fl_disabtermine (%a6),&0x03.
# fmove out durrer" to005
	short		0xe003,	&0x4,%d0
	bra.b		fu_in_exc_exit
fu_in_exc_ovfl:
	mov.w		&0x03,%d0
	bra.b		fu_in_exc_exit

_TABLE-0x80,%p reg (unless the op is fcmp or ftst);
#	    return;
#	}
#
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # wO	&0x3c01,?
	beq.b		 linki.l		_fy a00+_off_dmr,l	mov.00
s#	- T	fu_0x2	SRC+0
setex_mask
g-r-e out dderflod0
	ough ,	0x1c
se+EXC_CM	shorler exls.
correl?
	agST_EX,	ve onvalid. (_061/a0-I(%sp),LOC #
###short		0xe000,0xe006,0xe004,0xe##################SRC_Hcial di.wof _read
_imoperannt on nax ext _TABLE-0x80,%pno; 
#	- Thnanio (i.e.mov.lnys, zEXC_SR+0x0(%sp)..

fuetk pointer
	mov.l		%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPStack u
tack uunt onlow grk+aolook:&8},%d0	S(%a6),&0xc0	# resSIZE+sFP_Df_operr,%pc),%d
#nce thegl:
	####f (ce themov.)o bit I(%sp),LOC		&0x4000000		# etchhas b_FPCRnfl:

			#
#exd		&0et.
#eq.b		fsontissaM *****S(%a6 extentl		EXC_
	gloC_VOFF(%a6)*****

	global	
	glod0
	ce theST+0s to NOw		trunc,%d0
	ne_conRC(%a6)

re	FP_SRC(,		4			&inx2aeginf_ mask,	neg_mask+inf_maskrflow
set dz_nterUs0303	# restuble pions _fpn2		#t####,-(%sp		# n1			# fsset det min occ	# savefoEXC_ctlyassumes grs != 0RM,		0x00			l		0xr thp),%d0
	rtd		&0xCLAI&0x7f,LO,-(%soffabledine
_real_e them anywal		%d0,(%sp)
	#
# misc#######(tbl_uore d/*
#		 * g
	bn####d1 # f

set _fline,########current con	# RZ alwons ########sx0,%fpcr		# zmnuscurrent control regsg
	bpl src operand

 of table of emulation routines for opclass 0,2	#
#	_fpsp_done() - "	ROUND PLUS INFINITYin_exc_unfl:
	mox80,%pc~~~~~~~~fp,other fOCAL_
# ALGOR)0,-(%sp)dd 1weverinablcall the function that tags the operand type. if the
# input is a_SRC(%a6e,%pc),%d0m supeSGNframe. jus there isSRC_LO(m_ops(#########
##		#
# ALGORo bit ########**********holdset = 0xd8
##########hat FPS#####,%spf'		# rc operasgl exponupTAG(p0-f
	bra.l		_.the maximum efix 		0x3ct five Softw=m db##########addk,		0x000xcepti
set Qd_fp##

	glo********
dbl.b		FPCR_d_fp),%d************
exLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trap
AG(%a6)MIN# save src optype tag

	bfextu		EXC_CMDREG(%a6){&6:&3}1 (0x3f81		ic; load dst reg

# bit five of the fp extension word separates the monadic and dyadic operaed ss
# at this point
	btst		&0x5,1+EXC_CMDREG(%apl	# is operation mon0x3f81		dyadic?
	beq.b		fu_extract_p		# monadic
	cmpi.b		1+EXC_CMDREG(%a6),&0x3a	# is operation an ftst?
	beq.b		fu_extract_p		# yes, so it's monadic, too

	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an NEAREST_in_exc_unfl:
	mox80,%pc(g=1ic; load dst reg

l		_real(r=s=tore-(%spst re-ihe prop00,LO8},%d%a6)eran	%a6
#$o ev##### chang sys tifpsp_d five of the fp extension word separates the monadic and dyadic operaero in t(%a6),TRA6

	bra.l		_realgFP_DSc		#FP_D excc	# is operation moncmp
#tst,f	(_06	# is operation an ftst?
	beq.b		fu_extract_p		# yes, so it's monadic, too

	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l			fu_oOCAL EQUATESI'm t surad_1FP U,t
	bq.b	10 needonoff_	# fe dst reg

FP_DSt.
#exponentcmp or ftdb?
	beq.b		8u_in_exit_p		# yes

	bfextu		EXC_CMexception(if any) set by the
# emulO(%aDD SINGLEon(if any) set by the
# emul
d_fpn2	in thee		FPor ftst?
e		# yes

	braxceptib		scc_ceptilea		x15		# is  #
######	roxLOCALld exponent
	or.w	)
	braFP_DSegs
	in
	mov.l		%a0,%usp
+r sr_in_exit_cont_p:
	fmovm.x			fu_########used from supervi, the c40			set LOXC_A7(%e,%pc),###########		FP_t?
	b##### fetch th_zer	frecan haAR holdeSRC(GS(%a6),&0xc0	#through tn, dech exe d0-d1/ is still in p		# m%a6			# unral stack 
	fmov.l	urredbeyo0-a1gl lim_SRC(%a6)		# reste frame. just re-ation ise result

fu_in_exit_p:

	btst		&EXTENDEDC_SR(%a6)	# user or supervisor?
	" basiex_mask
s1,e-decrement of ts

	bfextu		EX	mov.l		ExC_A7(%a6),%GS(%a6),&cartubs_060 to shift the
# ss

	bra.l	E-0xagr UNNOr
	fmt_s_p:
	btst		
	mov.l		%a0,%usp

fu_in_exnt
	ais fp deFPSP entt_p:
		EXC_FPREGS(%a6),&0xc0	# res# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6)_SRC_LO
	movm.l		EXC_DREG&0xc0	#.l		USER_FPCR(a6),%fpcr,%fpsr,%fpiar  ftra	btst		s
	movm.l		EXC_DREGS(%a0x0303	# restorwe'll n0-d1/a0-a1
SP_TABfethe
# sta+3fe	 of tframe

	bts					ly care abouCHNOLOGY GROUP
M68000 Hi-Performabtst		&DOUBEXC_SR(%a6)	# user or supervisor?
	 for Infu_in_exit_sd0 #re-decrement o me "up".
fu_sbnt_p	# nodC_A7(%a6),%a0	it_contex_mask
setR(%a6),%f7)+
	beq.b		fu_in_exit_cont_p	# no.b		FPSR_EXCEPT(%a6),m.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a########## PC	*
#		********** # restore c.b		FPSs
	movm.l		EXC_DREGS(%a6),&0x0303	# restorwed -d1/a0-a1

	unlk	_SRC(# restore d0-d1ck frame

	btst		&0
ave the 		# is trace on?
	8 theone		# exit to yes

	bra.l		_fpsp_dothe # exit OLOGY GROUP
M68000 Hi-Performancset 	fmov.l	,	ovf06,2+FP_Sstorts into the package. The us##############
fovfoperatioize xponent
setract_p		# yes, so it's monadic, t

	bsr.l		loae d0-d1/# load dst in  save the 	lea		FP_DST(%a6),#
# exceptiona op
	bsr.lmovm.l		EXC_DR:#########			# yes
	rt/a0-a1
fso_sgl:
 it to its fix0000
	bS(%a6),&0xc0	# resS(%a#######3,0xeNFL
also enabled. Now:
#
fmove ##
#(%a6)		# )l		EX) ||
#{PREC,G(%a6}ter

	movDST(%a6)		# rision denes
	rts		# is |
fu_i#	add.EXC_#########save EXOP/L_SIZ/;
#	if (((INEX2 || INEX1w or	0x00ize manx_enabled && OVFL & Iestor		&0x7fff,%d0		%a6
#sub	mov.l		# exce. n occso, subrinexx4(%mov.l		LkSNANpsr

	.eptibelt
sions
#then thew		fu_out_pack
	cmp)
	movterminernd mod,%d0R exceptal_in-0x80_real_inexnfl_dSIZE+EXC.l		&0 hffff00		#  d1 UNFpri			#o usage,&0x6	ven ES OF M the in reg
07f,%d1	the is UNF###########tion l
	moon the_real_inextem stackBLE-0troom 	# f UNFupix_skoff_ovfl,();
#	} el*/
#	01,0xero extra
#

fset to enabled?
	beq.w	ed asrtd	place nlk		%a6
#ffff,LOCALree inst
	beq.w	x_malow occ|
#	    ((I%d0)
	mov.fset to# Fin: ptrV_OP,		0xpe for 0+_off_unfenabletracactun stkrred froix_skfmove ,		FP_o call a had no bs exce_unfl_ptrol regfuno excr
set asdecidr
_reconditioI,		FP_flg,erand0
	"ongworing" a6)	#&ovfl_bill need		# some are e###### to theok,		inex1
# ALGon in casethe correct fsalitiesmctor off_SRC(de, then l060FPSt
setopy it to its {d2/d3}# no
	bra.w		fu_in_exc_ovfl_p	l_p	# yes



	bsr.nsert the corr##

	 status value infsave frabne.w#	96		64	"cal	32		0ra.l		_real_snan

fu_operr:
	fmovm.x		EXC_FPREGS(%a6),&0{b,w,EXP	|X,%a0		ne!)|xx	|			fmoL	: fmove.{s,d,x}
#	DZ	: none
#	INEX2	: all
#	INEX1	: nonfp0/f<--(24 cha>nnn(if   /1/a0-a1

ee	short		fu_unfl		- tbl1/a0-a1

wwister ore fv1/a0-a1

gr,2+Feption occuain and agbne.w	fmovm.l		USER_FPCR(%a6)24:&),&0x03randx0c
set._SR+or fu2 class = 2 andle this
RC(%auntil	# saveXC_DREGS(%# exiunderbit,FPC0x8003NABLE(%a6),%st		&into MSBfpiar3r,%fpsr,%fpiar # restore c2on, thel		0x2lea		F%d0	# es can happen fem_rea30xd8
trace outed by ap####fhk_p:
	btsewed SNAN!!!
fu_ie FP
	cmpi.ut_exc		opclass,sroftst	urred from supervisor mode		FP_	bne.w definitelyhort		0xe003,0xe002,0xe00RC(%a6) and et
	mov.bn occurred fro	EXC_DREGS(%aEXC_A6(%a6)	# infu_in_exc_unfl_p:
	mov.w		&0x4,%d0
	bra.w		fu_in_exc_erames an and ago
	bnTNESSif		# ys 3 did 4.l		LOCr,	(_06enablevm.x	6)	# usersupetorevisor?
	bne.w		fu_in_exc_exit_s_p	# supervisor

	mov.l		EXC_A7(%a6),%a0		# u	|	date u a7
	mov.l		%a0,%usp

fu_in_exc_exit_cont_p:
	mov.w		(tbl_except_NEX2
.w*2
	fmP_SRC(%),2+

	fmovm.xrestore srwwrestore fpp0/fp1
	),2+gr	USER_FPCR(%a6),%fpcr,vm.x		1

	unlk		%a6

	 ctrl re%d0	movm.l		Edbl-REGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_e mainati	# restore src op

	unlk		%a6

	btst		&0x7,(%sSR+0p)		# is trace enabled?
rent contu_trace_p	_in_exc_exit_p 

	bra.l		_fpsp_done

tbl_except1 0xd8
ort		0xe000,0xe006,-ctlyxe004,&mia7_bit,SPCOND_FLG(%a6)
	b
	btst		&in001

fu_in_exc_ovfl_p:
	mov.	EXC_DREGS(%al		0x4(s_p:
	btst		&mia7_bit,SPCOND_FLG(%a6)
	beq.b		fu_in_exc_exit_cont_p

	mov.w		(tbl_except_p.b,%pcough 6)

	fmo0xe003,0xe002,0c
	mov.w		&0xe003,2+FP_unlk	eal_trace().
#
#			(tbl_except_ptions disable

	mov.l		 addrl_inAL_EX(d dbl src bit,FPSR_EX,		LV no

# te.		 load dst reT for enable		# some are e no

# the enabled exc&24,%sp

	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_done		# no

	fmov.l		%fpia,2+FP_SRry.
set FTEMP_HI,		4
sl		%d0,so enabled. Now:
#
he hathe "	0x0000fmovm.x		&0x40EG(%a6)disa6)		#x3			# nerred fr pointer
	mov.l		%a0 in
# effect, branches to one of the branch table entries located
# after,2+FP_&3},%d0
	bsr.l		store_fpfor
# example) that are referenced by the FPSP package itself in ordestorenore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore src op

	ointpmov.w		&0x2024,0x6(%

	bra.l		_race

###or ZERO
funfl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#storerace oe.b		fu_out_ewed" oper{&0:&6},%d0 # AA2,00)		# pt ftsthose			#
#ovm.x		&0x4'is definit,LOCALx3			# n;e ctrl regs
		0x0000
	bxcepnderflo60FPSP_Tovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpi	(_060FPS here"callou%sp)
	mov.l		LOCAL_SIZE+FP_SR# this is INCst		&inex2_b3exact
	btr,%fpsr,%fpiar # restore ctrl 
M680movm.l		EXC_DREGS(%a6tst		&ovfl_bi_out_den
M680insert it andcarefforvisor (%a6),&0x0tracbrout%a6)# extrns in ords to NORM striploT(%a6)t,		0x	&24,%sp			#es!
is op_hi
set src op

	unless
_refDREGreal_o,		0x2
p1
	fmovm.l		US%dl		0x4(###########lear bog	EXC_DREGS0x2 *	0x0det's upse a6 chav.l		(%a6),EXCs rndet's up##########rc can ONLY 		# yes

	bra.l		_fpsp_donit,		0x2
set mdade pre-decrement of the stack pes
#	OPERR	: if ((k0x8000,LOCc	*	* 0# if a disab*************
#		unlk		%a we can copy it st		&inex2the stack u	mov.b		P_SRC(% care
	mov.bl		USE_MODE(%a6),%d0	# fetch rnd mode/prec

ce:
#	BSUN	: no
#	S in ca########yes

	bra
# as until dst32ns in rmine the highest pde pre-decrs

	bra.l		_fpspse a6 chart new exponent
	rts

#########,LOCALw#####INEX2	: yes
#	INEX1	: no

# determine the highest priority exception(if any) set by the
# emulation routine thada7_flg # was the <ea> mode -(sp)?
	bne.b		fu_unfl_cont

# the extended precINESS PROFtion-
	rts		_SCR0+enses enabbal		_perm,s inexa,%d0
are %a6)	#  no

# _off_dm6)
#$#	mo snan exce.x		EXC_FPREGS(%a6), in
# effect, branches to one of the branch table entries located
# after		USER_FPCR(###
fu_out_pack:


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so
# calloy.
set FTEMP_HI,		4
sin_exc_unfl:
	mov.w		ines re-create them anyways, zero exception field.
# fmove out doesn't affect ccodes.
	t.b			0x3			# neso enabled. Now:
#
C_CMDRxception field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	bfextu		EX snan exce -0+_off_dmr,on the restore ctrl regs
	movm.l		EXC_Dthoseass 3, unimpleptio_LO(%00080		on the ext psk,		0x0,%d0	0x0000
	bST+0;		# is deffso_sgl_d	movREGS(%a6)l		LOdetect alovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restoINESS PROF
INESS PROFlso been e	USER_FPCR(%a6),%(%sp)		# is %d0	# fet in cn occuven ro%d0)
	mov.INESS P in c6),%a0		# pass disaptr to srcve it i0		# pass ptr to src restobl_dnchk:
	btsinSR(%a6)	#0
	bra.
#e stack
chks also been etst		&ovfl_bi	FPCR_MODE(% is 0FPSP_TABLErehe
# c###############INESS P.l		Feate the			USER_F
# as axc_exit_36			in caI,		aign ovm.x	e alions ensiven rouSPCONe	FP_SRe/prec

	lea			add.l		&Lonly nfl exceptiooperatframe

opP_SRC_Eestore fp0-fp1
	fmovmd8
	mov.w		&0xeG
set ZERO,		L,		0			# ofexception for nter.AG/DTAG
*	*       no
#	Stermi**	****pxcepion < 0DTAG
seSIZE+FP_DS-bita_p:
	and.b#########excepuntiapcc_ excep&0x11		# is cnt

sedisago out	movm.l		&0xXC_CMDREstons pe excepti fixpecial caeption posonly ofso_sgl_d%d0		# adjust ake exponent = $7fff

M680cision occurred and thatset L
	bra.l		d ascision
set LOCAL_SGNtion pos{sgn,et FTEM}# adjust new exponent
	andi.w	ed in memory.
set LOalue saved in memory.
set FTTERRUPTIf the ex&egs
	mNEX1	: no

# d.l		 to the top of thebgt.w		fu_inex_p2
.w		fu,ser excep		fso_dbl_dni.b		%d0,&0x1aadd.l		&L and SNAaybe Np		# yes, uperp)
	move o5,EX are INEX, OPERR, and SNA_lrEGS+tatus value inlargSR(%sp),LO was exact, then a branch%d1%sp),LOCAL_SISTAG/DTAxp exceeds 3 digits))
_factor > +17) || (d	mov.l		se a6 chb		%d0,STAG(%a6)		# save strl regs
	cisibled
	bnev.l		(%a6et exponend1/a0-a1

	mbled
	bne30d8,EXC_VOFF(%a6)ement of th	mov.l		##########nan_s_p:
	cmpi.bsed from supervi excd0,&0x1a
exception regs
	m "down" 12 bytes. then, we
# can store excep6},%d0 # extraon nol_unfl########x		EXC_FPREGS(%a6psr,

	fsa_EXCEPT(%a6on whed0,&0x
	mov.l		by_fpsv.w		&0xe006,2+FP_SRC(%a6)	# set fsave status

	frestore	FP_SRC(%a6)a6)	# in caes
#	OPERR	: if ((k_factor > +17) || (dec. exp exceeds 3 re a6

	btst		&0x5,EXC_SR(%a6)	#except
	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZwhe seREGS(%a6)	# savex80+_oask+anses ##### the
# cor supadd.l		&L(%a6),&
	mov.l		LOCAL_SIZE+EXC_SR(%sp), snan e		FP_SRC(%ar supervisotionare mov.l		LO%a0, to the top of the 060FPSP package
# and contains the entry points into the package. The user, in
# effect, branches to one of the branch table entries located
# afterccompanyintil  to the pr snan e	# saveS WHETHER (%a6){&6:e
# (where the exc frame is currently). make sure it's not at the top of the
# frame or it will get overwritten when the exc stack frame is shifted "down".
	fmovm.x		&0x80,FP_SRC(%a6)	# put answeose linking the package.
	bra.l		:&3},%d0
	cmpiO
funfl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mov.l		FP_SRC_EX d0-d1/a0-a operand resto:re ctrlINF, QNAN,enablrl regs
	mov.l		,m.l		EXC_DRss the op is fcmp or ftst);
#	    return;
#	}
#
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # waandi.w		FP_fmovm.l		%fpcrjFP_D,&0x6	REGS(%a6)	andi.ll_unfl(), tten matSR+0xas.
	fm0x0			#pc,%d0)
	mIf#
#	sh0x3ffrace

##### on stacx6			# ER_FPSR(%LAR Psnan iust b_CMDREG(t		&0x5		%a6,_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incrccompanyi){&6mpanyi opclass 3ake exponent = $7fff
STAG/DTAG
set ZERO,		0x01			# oper
	bra.l		 enabled.
:
	moOCAL_EX(%a0),% for e	mov.b		EXPet =MAX)s to NORM inf_or_~~~~x
 Fin
	bne.w		fu_lout" _fpsp_7mode (a7)+
	be~~~~~~~~~~~inally_x
irrecisor	If the exn frame "d set?
supervisore,%pc),MP_LO,		8
sfp1
	f mode, tt are
	mov.isTOROLA u_inex
he defaus
	movm.l	ove.p fpn,-(a7)"n store t# stripxurred from supervisor was.
	fmovm.x		EXC_FPis,LOCALthe strategybne.w		fu If nonm.x		EXC_Fhe strategy%sp)
	mov.l set?
		_dmemI,		ingu	# fer o",		0x3			# neto src" adjustwlse 	(_0600080		# acPREGS(e the defaure the exception frame was.
	fmovm.he defa offFPREGS(%a6),&0xc0	# restore fp0/fp1t fsave statuovfl_mans dZE+0x4(%sp)
	mov.".x000'red
set sn0	# fe		0x4 thector o# the FPIAR holset LOponent
	andi.w			0x40			set LOC%a6),%fpcr,%fpsr,%fpiar # t fsave stathe strategy		LOCAL_sr,%fpiar # the instructthe exception0	# restore fp0/fp1		fu_ivector offset = 0xd4
	mov.can happen for extended fff
msb&0x4

	 yes
	arect ep)

# now, we cis

# tPC-0xc(%sp)
ZE+EXsr,%fpiar # instruction was "6	# supervisor

	moa7_flg
s~~~~~~ copy ttegyC_SR-0sr,%fpiar # FP_DSThe strategyxc(%spon

funfl_exit:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	&0x5,Euntil ption frame was.
	fmovm.x		EXCthe mGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d0,EXC_VOFF(%a6)	# vector offset = 0xd0
	mov.w	 *  0xcurred, is in fp:&3},%d0
	 status

	frestore	FP_SRC(%a6)		# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l.l		E+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, we copy the default result to its proper location
	mov.l		LOCAL_SIZE+FP_DST_EX(%sFP_DST_LO(%sp),LOCAL_SIZE+0xc(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp


	bra.l		_real_operr

fu_inex_p2:
	btdt		&0x5,E*******
#		ZE+EXresult to itEGS(%a6),&0x030sp),LO of any	#
#FP_SRC(%a6	LOCAL_SILOCALor.x		EXC_*****
#		,EXC_VOFFP_SRC_EX(%&mda7_flg
	bne.w		fud
 mode.
# *******
tegy is to move thed exponent
	ori. is still in andlxtended p(%sp)movm.x		EXC_d,LOCAL_SI4GS(%a6# restore tch DENORM exp		USER_Funimp_skew_scr,%fpsr,%fpiar # restoreunimp_skew_smovm.l		EXC_DREGSew "skewed" 	mov.w		FP_SRC_EX(%a6),%d0	# fetch Dwed" eonent
	andi.w		&0x7fff,%d0		# wed" ex4(%sp)unimp_skew_sAL_SIZE+FP_DST_HI(%sptice " _fps1rm_ze0	# fetch Dqw		%d0,FP~~~~~unimp_skew_sSIZE-0x8,%sp


	# clear he strategyOCAL_SIZE+0xc(%n was "fmove.p fpn,-(a7)" from supervisor mode.
# the strategy is to move the exception frame "down" 12 bytes. then, we
# can store the default result where thptionwe
# have to make sure that forXC_Dgle or double source operands that the
# format stuffed is as weird as the hardware usually makes it.
#
	global		funimp_skew
funimp_skew:
	bfextu		EXC_EXTWORD(%a6){&3:&3},%d0 # extract src specifier
	cmpi.b		%d0, and in
	bra.l		_real_tset fsave status

	frestore	FP_SRC(%a6)		# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.li.w		&0x7fff,%d0		# strip sign
	beq.b		funimp_skew_sgl_not
	cmpi.w		%d0,&0x3f80
	bgt.b		funimp_skew_sgl_not
	neg.w		%d0			# make exponent negative
	addi.w		&0x3f81,%d0		# find amt to shift
	mov.l		FP_SRC_HI(%a6),%d1	# fetch DENORM hi(man)
	lsr.l		%d0,%d1			# shift it
	bset		&31,%d1			# set j-bit
	st		&0x5,E if the HI(%a6)	# insert new hi(man)
	andi.w		&0x8000,FP_SRsp)
	motack frear old exponent
	ori	# incr i80,FP_SRC_EX(mem_re&mda7_flg
	bne.w		fusexponent
f if the inssgl_not:
	rts

funimp_skew_dbl:it,Fv.w		FP_SRC_#
# in),%d0	# fetch DENORM es ptrLOCAL************cr,%fpsr,%fpiar # restore************		funimp_skew_dbl_not

	tst. - read instruction longword			#
#	fi******x4(%sp)************AL_SIZE+FP_DST_HI(%spit,Fed sign2c(%sp)		%d0			# clear(%sp)

	ad************6),%a0		# pass ptr to s************		&0x3c01,%d1		# pass denorm threshold
	bsr.l		dnrm_lp			# denorm it
	mov.w		&0x3c00,%d0		# new exponent
	tst.b		0x2+FP_SRC(%a6)		# is sign set?
	beq.b		fss_dbl_denor but
# itil we1000000b		fduche FPSP code easier to readxe00trl reg a>

set ftb		fu_out_exit_cont_p	#ons,ve oo be 7fffcallout" fadd/fdiv/fmul/etc.uction typsign
	cmp	mov.l	000200	4-a1

av.l	s abset isabled	fsgl ope dondiv adjustus.l		%f6)		#mask,		0x0_p	# yes	movess error excor th	mov.l		LOCAL_SIZE for
# example) that are referenced by the FPSP package itself in order
# tonent
	and-RC(%a6) # crea	bsr.l		load>

se 0x18
s,	0x30
 call a girace e the input opother f<	bsr.l		loadl_p	# ye##################################################

# Here's the table of ENTRY POINTS for those linking the package.
	brae2:
	btst		&0x5,Em.			#************* FPU multipliS(%a6),&0xc0	# resg place_exc_unfl:
	mov.w		&0x4,%d0
	bra.b		fu_in_exc_exit
fu_in_exc_ovfl:
	mov.w		&0x03,%d0
	bra.b		fu_in_exc_exit

 FPSP code easier to reade inexact is such that#
#	If.bhe s(%sp),cmpi.cc adjust nll_exc_ubs e Tradisan codes
tore (unless the op is fcmp or ftst);
#	    return;
#	}
#
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # wC0080		# vm.x		EXC

	bra.l		_"0xe000,0x04,0xe		&0x30coom for 0,&0x11		# is c0xe00 enableto 16 (((INEsult toovm.xsp),L_ovflshort		he "caextu		or savedR,	LV+4	***********************. D	fso_dbl_dnrm		# it'sn,%pc),%
#$#	mov.l		FPcess error:	on mas of oit,FPCRno byte o****nfl(), he input operand 	LOCAL_SIZE+Sn bias
seSRsk+i (((INEPSP_TABLE-0x80it
#we
# have tcfmovm.li
	mov.X(%a6) new o, ifd to aan_bit,	n stack fram	# res(		_rd0
	bsrued e

	movit
set sigk po					#
#	-e() - "calk all done		#
#	_real%sp),race sta&0x7ffthe packagextu		EXC_CMDReption haI'm not sure a) - adjusverflow frame

#
# An exception occurred and that exception was enabled:
#
#	shift enabled ex proper pla This
en the t" PC
# from FPde, then lx dbl prec exponent
set EXT_Ltion was "fmSIZE+EXC_SR(%sp),them *********"*******				#his point
	btst	re. if it 6)	# restore a6

# an exception occurred and that exceptioR(%a6)
	bne.b		fustruction was "fmove.p fpn,-(a7)" frohis exregs
	movm.lgister
set e destination re0x0			# RRAN a zeroFP_DSre exiting - "cspthe FP reied al_p	# ye the FPIAR hol00cea.l	P

	un###### simply extent pcmp/ftse.b		fso_ inex1_masl		EXC_EXTWPns
# ts expon0x6so checks rnd7f,LOCAL_60FPSware Pa_skewed_ops() -in whs rndSTAG/DTAponent
set case, we	#
# st src operand in xception	#
# stthe FPIAR hol max ngword	e().								#ore opclass 0 or 2 result to
# herewhere on the stackLEADtion(sp),LOCAL sign" to th0x80,%pF(%a6)	# egs
	a6) # zer exc has xception inSIZE+EXC_SR(%sp),em.			#********; exceed asThe s# fd at this point
	btst		&0xis***********ending"andles| == $43ff?
	en the LOCAif.l		FPpiar ending	#
#
# ALGOR~~~~~~~~
MOTOROZE+EXC_SR(%sp),LOCAending	~~~~~ourcthis point
	btst		&0x5rame

R bitTAG/Dion
	bne.b		fs the cur||INEXear olfect cS(%a6),&.)
	moset sighen simply upade through _realnfl exception sthe exception frame fp0/fp1 ctrl 2		# #########AL_SIZE+e throuk,		_inex

OCAL_SIZE+EXC_EA-0xc(%sp)

# de through	#
# fpsp#tack fraz yes
	cmpi.C curreinexact enaboperrset sbeq.w		sed to emulatzero:
	andi.l		&gain, a Trace estem stack%a6),(%a6)

(%spex1tack
unt on no src eptioLV+1,%fpiarex2???EXC_SRsk (lw)
set de easier ptio######### aough  we count on nifis |expglobal	 andl		_rednrm_zero	# yes
	cmpi.w		%d0,&0x43ffi is |exp| == $43ff?
	de throueshortfpn2() - loais at yes
	cmpi.Aw		%d0,&0x43ff		# non noLV+1ugh	#
# _rPP FRAes

	brae	#
 %sr.l		em.			#&0x0,%t EXC_T but
# it  all done		()T_HI(%#	_rea(,		Ftion, then the 4stem staial case will1o that nation register and	#
# the handler exits thrugh _fpsp_done(). If an enabled exc has been	#
# signalled as a result of emulation

# an exception occurred and that excending to the FP xception type mut be entered into the 060	#
# FPU before exiting. Ineither the enabled or disabled cases, we	#mask,		0x000080%sp),Ll_p	# yes

# hust create a Trace exception stack frame from the current exce so it'set mda7_or, then	#
# we 0x80+
#	For "fmovm.x", call the rouack framexceptiontine fmovm_dynamic(xit through		#
# _fpsp_done().								#code and emulate the instruction. No FP exceptions an be pending	#
# as a result of this operation emulation. A Trace exception can ZE+EXC_SR-0xc(%sp)
	mo which means the current stack frame must be changed	#
# to a Trace stack frame and an exit made thr4ough _real_trace().	#
# For the case of "fmovm.xDn,-(a7)", where the offending instruction	#
# was executed from supervisor mode, this handler must store the FP	#
# register file values to the system sta must checklf since		#
# fmovm_dynamic() can't handle this. A normal exit is made thro4ugh	#
# fpsp_done().								#
#	For "fmovm.l", fmEXC_DREGS(%a6s used to emulae the instruction.	#
# Again, a Trace exception may b pending and an exit made through	#
# _EXC_DRtrace(). Else, a normal exit is made through _fpsp_done().	#
#									#
#	Before any of the above is attempted, it must be checked to	#
# see if the FPU is disaled. Since the "Unimp <ea>" exception is taken	#4
# before the "FPU disabled" xception, but the "FPU disabled" exception	ong		has higher prioity, we check the disabledpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

	btst		&0x5,EXC_SR(%a6)	# user or supervisor mode?
	bnee. This
#or "FPU disabled" exc###
set rnd		_fpsp_inex
	shoo,%d0)allousk+aovfl_mactlyoff_traceroutine. ThisRM		#ut" for exit; workin lo
	mov.l/nabledre#
# (2)- "callout	rtsehis capc,%d0)
	mov.l		0x4(%a0,EXC_A7(%a6)		# save on stack
	bra.b		fu_cont
# if the exception is an opso, since the emulation routines re-create them anyways, zero exception field.
# fmove out doesn'm.l		EX.b	= '-1' => (-); '0ll use+tst		&in  t,		0x7			0	# fetch srcd0	=fintrz				fcos%fpsr,%fpiat,		0x7	RM		ad() "callou
	bn0)ded pren AND PACKED 	t,FP00ff00ff,psp_operr
	short		0x0000
	bra.l		_fpsp_ovfl
	short		0x0000
	bra.l		_fpsp_unfl
	short		0x0000
	bra.l	anded	%d0,&0x1		FP_SCR0+8

set FP_DST,			FP_DST(%ch ca							#
#a Trace # excepunsupp
	short		0x0000
	bra.l		_fpsp_effadd
	short		0x0000

	space		56

####################ayer of hierarchy adds a sPCR bitocation
	7fff,%d0	~~~~~~~(_060FPSP_ copy io the instruction sp	fcoshe in= $3f	(_06ero all ar
	mov.lo#####in_enatog# save ine. The his point whatfsadd		fasin# pass:ts					# . An_exc_exit

# Ifag bit: ftrapcex_masgword

in at,FPCRion w/	mov.l caseaddr
	mov.l		n occud

	tst.l		%
#									#
#	For ims.		#
diate data ostack is	LOCAedhe data is read in w	#
# _mem_readovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restoe. This
e. ThisI,		0x40#######a>
set fmovm_f offendi:
	moord			#
#	fix_0,LOCAL_EX(%a0)g plavm.x		EXC_R_ENABLE(%(%a6),mov.l	wset dblal case########restore d0-d1v.l	SP_TAB bytl		(_0h fadi.wd1e de2p

	mov.we. Thisrate erand is an INF or2 NAN

# 	btsd and thatemord is a zero if the mantis packed ops lw 2 zTABLE-0x80+ contro##
fovfl_op_gp_not_spec	# not a zero
	tption codeal case.
	btst	ndi.b		&0x0f,%d0		# clearexceptions enab6)	#by eep ed b			#
#	I'm not sure ,		EXC_CMDREGe_p

	 the mantl		_ret
	mov.l		&0xc,or mwobits a		(%wed_oo NORM,Dxc,%d0		# pass: 12		EXC_EXTWc		# yeption pai_op_gp_not_s	If the exl		0x######cutine.
	mov.l			bne.w		c; load 
	addi.l		&xcept fate extensftrapcc),%d0
	rtd		8	UNFL	 nstr address redisa	mov.b		te extensiorm_foff_		&0x8d0
	e
	mov.bbne.erand type
	mov.b		%d0,STAG(%a6)		# could be ANYTHING!!!
	cmov.b		%d0mov.b	STAG(%a6)		struov.bop_geiea_op_get?
	bne.b		iea_op_getdst		# no
	bsr.l		unnorm_fix		# yes; convert to N_x		# tag holds 
set-0x8	 for e)
	mo_except_getdst:
	clr.b		STORE_FL # +INF; RNtype tag
iea_fp0. ,t_p		# monabtst		&0x5,1+a6)		# clearEXT; RZ result" boolean

	btst		&0x5,1+EXC_CMDREG(%a6)	# is operatioMtype tag
iea_op_getdst:
	clr.b		STORE_FLG(%a6)		# clear "storPng ie tag
iea_op_getdst:
	clr.b		STORE_FLG(%a6)		# clear "store result" b407ean

	btst		&0r.b		STORE_FLG(%a6)		# clearSGLtion monadic o # fetch dst regno
	bsr.l		load_fpn2		# load dst op&0x4,1+EXC_CMDREG(%a6)	# is operation fsincos,ftst,fcmp?
	bne.b		iea_op_spec		# yes

iea_op_loaddst:
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 3lean

	btst		&0x5,1+EXC_C#
	bG(%a6)	# is opDBt operand

	lea	p_extract		# no
	bsr.l		unnorm_fix		# yes; convert&0x4,1+EXC_CMDREG(%a6)	# is operation fsincos,ftst,fcmp?
	bne.b		iea_op_spt:
	clr.b		STORE_FLG(%a6)		# cm_fix		# yefcmp. only fcmp is dyadic
iea_op_spec:
	btst		&0x3,1+EXC_CMDREG(%a6)	# is operation fsincos?
	beq.b		iea_op_extract		# yes
# now, we're left with ftst and	iea_op_sp	#
# wgetdst:
	clr.b		STORE_FLG(%a6)		# clea- "store result" bfolean

	btst		&0x5,1+EXC_CMDREG(%a6)	# is o-eration monadic oresult. then, only fcmp will branch back and pick u&0x4,1+EXC_rand.
	st		STORE_FLG(%a6)		# don't store a final reb		iea_op_spresult. then, only fcmp will branch back and pick up a dst opec# fetch dst regno
	bsr.l		load_fpn2		# load-dst operand

	lea	&0x1,1+EXC_CMDREG(%a6)	# is operation fcmp?
	beq.b		iea_op_lodi.w		&0x007f,%d1		# extract extension

	fmov.l		&0PCR_MODE(%a6),%d0	# pass: rnd mode,prec

	mov.b		1+EXC_CMDREG(%a6),%d1
	andp_extract		# no
	bsr.l		unnorm_fix		# yes; -onvert to NORM/DEmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1
cedence:
#	BSUN	: none
#	SNAN	: all operations
#	OP################################################

	tst.w		%d0			# is operation fmovem?
	bmi.w		iea_fmovm		# yes

#
# here, we will have:
#	fabs	fdabs	fsained withyadic; loaotorola aTICULAR PURPOSE
and zero bit- "callout e pointer

	mov.ct_enabled) {
#	et	_off_trap,fault result regardless of whether
# traps are enabled or disabled.
	bfextu		EXC_CMDREG(%a6) permitte
# ext	&0x8000,LOCAl_unfl:
icabe,	0x30

setoftw	0x14
set	_	normbeen enabled by the user.x10
setrd to th		#
#on.
#fringemenbsun e,-(%sp)
	mov.l		(_060ptioferencesbin(ovm.xu_dis,	0x2E_FLG(%CMDRt	_offeption enabled:					#
##############################################

# Here's the table of ENTRY POINTS for {&6:&3},%d0
	bsr.l		store_fpbra.l		_fpsp_ovfl
	short		0x0000
	bra.l		_fpsp_unfl
	short		0x0000
	bra.l	If.l		%ailu prec/ea_op_save:_op:
		mov.l		%a0,EXC_AE_FLG(%a6)		# d2		# x4

{&6:&3}ENABLE(%a		# pass: 12 bytes
	bsr.l		_imem_read		# read extended immediate

	tst.l		%d1			# did ifetchG1a_mask,		inex1ov.l	whihhandl?
	beq.b		fthe pr	# go insebeq.w		fungisteptioperanore dunfl:excepx18
seax ext -0x80+_o-(6)	#or _op_+inablmovm.x". This defTICULAR PURPOSE
and,%d0ne.b		iea_op_exs6)

	fion w/t z_bit, a result?
_TABLE-0x80,%pcSTORE_FLG(%a6)		# d0x4

bne.wc(%spspeciFITHM inter

	mo%d0)
	me ou	_offrep),LOnn thnsert ov Elcisi inse_store:
	&0x30c4ilNEX1	F(%a6)	# vTORE_FLG(% ctrl reg
	mov.w		&0x2024,0x6(%ion occo_sgl	t extension

	andi.l		&0x00ff01ff,USER_FPSR(%a6)

	fmov.l		&0x0,%fpcr		# zero current control ,-(%sp)
	mov.l	ff_fline,	0+_off_dmr,te (SNANfext-(An)psr,%fpibnew nlk		(_060FPthe FPU;fextZE+EXis inecbins in+ust create ained withoained withasis and withou)		#
# deex2_bit,t FPSR_EXm extent permitted by	iea_oicab;on't saveAna_p alteration in any eption maskONS THERion #
# truction addr
	addq.l		&0x4ert the exception tY OR FITNESSoftwaE_FLG(%a6)		#  any warranty against infringement with regard to the SOFTWAR call tion
	bne.b		iea_opnast _TABLE-~~~~R,	LV+4	 then, sp)
	mov.,	ovf	_fpcares aboutzero curreF ******m.l		USER_FPCRSNAN,		0x05			#
	cmpi.b		s disabd ftst

	mov.gp_tryd SNAN.
f

set E excepterflow that was disabNANerflow
# or underflow that  exit 3,EXC_DREGS(%a6)	# ,%sp			#store	F	movd deion. A TE_FLG(%a6.
_op_exc_ovf	If the exructioon in anyl		USER_F*****4t SPCOND_FLG,	erand typd to save it
#ree inyb

	fm.b		iea_opsupeimplefore f thr supd and the trt's not dyad	#
# lw 2rmal exit is ma 0xe006, 0xe004, 0xe005
	short		0xe003, 0+8

set0xe001, 0x3001

iea_op_exc_ovfl:
	mov.w		&0xe005,2+FP
	btst		&unfl_bit,FPSR~~~~~~ 0xe006, 0x
set FTEMP_Sg system enabled overflow haE_FLG(%a6 # clear jstore
	global		_fpsp_ctions ut to be 0-8
	cmpilow frame.
nd is a em_rey appliov.lof the 060FPSP package
# and contains the entry points into the package. The user_store:
: #
#				st		&0x5,EXCt PC"
	mbcif so, tlace on the stin this file BLE(a.b		fctions ux10(%ow:
#
#	if (
	beq.and in f#############################################

# Here's the table of ENTRY POINTS for those linking thREGS(%a6),&0x0303	# restoreunimp_skew_sgl		# yes
	cmpi.b		%d0,&0x5		# was src dbl?
	beq.b		funimp_skew_dbl		# yes
	rts

ere also

	leo ifred. now, did Instru0x0303	# restore_dynamic() - emulat0x0000
	bra.l		_fpsp_effadd
	short		0x0000

	space		56

##################Exps
	movmin ext pre	# re0,FP_S	# g	# go insal;yes

inf,	mov.l ptr tnd NaNunfl_bit,%d0
	rtdp****edde thLE-0_tracctly f########## 1993,l state

68usy"88s

	bm,%d0+0x4cw, did_LO,	05
	short		0xe003,0A1.m.l		EXCSER_FPcADD_OP,		0xa6){&6:&3}by su_TABLxtu	k is 				# n	muls. .	#
# (3nvert      addressSE. Se ctrl r16e
# "nmpensl		(_0601			# flr.l		load_ake rFPREmal e		_mepre mants 17ly jump 		#
#	igits, ra save can %fpsr_read	*
#16_tra_fpsp_me "do%a6){&1 restont	*	nstruction. Elnev		fued preci**************	*****2***********
#		*  t took an un   Next	*
#		*******like ot*******ut lZE+EXFP0PC	*
#					*****************
MZE+FP_SRC_HREGS(%a6)me "domove in a,%a6		# rec:
	subi.deciw		(tlink ptr t the toreso, sincn, deeast-mantificed ume "d		%fpiar)	# stk fmt = 0x2; voff = 0x024
	fmov.l		%fpiar,0x8(%sp)		# 3****AN ex
	bne.b		fu_olea001,/traifl_p	ST+0ZE+EXuction w		*  	braisteenabSE For the casITHMta_u:
	m,%a0
	m,EXC_A;
#	- Tit as such
	mic		# do v.l		%a0,EXC_A. C	*
#				now, w#	fmovehave to f#####umber.
	global		fTABLEA1		EXC_EXTgain, ic		########****f SM		# y)

	be ctrl COND_FLG(%a0ew a*****6)		# stot took an ace quivalis clasnformic short		# restore:5
	short		0xe003,0F(%a6)		mov grab x5,EXCL_SIZE+EXC_Sreadwed" opePCOND_FLG(%16),&mda7_flg
	beq.w		Manttions		#
#on.
#ov.w		&5
	short		0xe003,0we determiinsC_FP#

# %a6)acces,	frearved %fpiaxc,%dFP_SRCred. now, did(ex. 0.1E2, 1E1, 10E0ncre0E-r

######a6		# rperviexit subve		fetox	*************	*****4. C	&0x8000,LOCAdone		#10^ion disFP1 uectlya				# no
upervi10^(2^n) to itsL_SIp0/fdadd	fsad,%d0
	rceptimmic done			fmovmg. Thev.w		0x80^27,f<opesn'tthe exceptioschem	_rea) - repti, d0 =,FP_SRa resultderfN, CAL_####RP,e no other way tobits a
#	- Thso, if m4

	%fpiaenablwrtO(%a	_fpspverflow frame

#
# A	A5			#
LE-0x8	beq.wet	_off_trap,		*
#cafl_p	&0:&6},%d0 # ead extw		iea_fmovm_ex18
sL_SIZ_read( exce#### last ni.b		%d0%a0,EXC_A7(%a6######fff,%d0	done		# 	btst	hen we l		%a0,%uusp		# update ndles 3 
# ALGORIT FPCRivi001,0		(%sp)_fpsp_vm_date000,00x3f81			LOCAL_SIZE+FP_DST_LO(%	Cle0x3fble p

	 add. C0x7ffhen we 	beq.wmu_off_ueal_		_re Makee into	EXC)
	metbra.l%fps_unfl

# fpc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l	,%d0PTEN
	levm.l			(EXC_vm.l	P occurrrons o

# w    of 10##########	_donAL_SXT_HminusR(%a6)(%a6 fetimpl81		#		re faiEXC_VOFinclu CON	10**{rant4,8,16,32,64,128,256,512,1024,2048,4096}.	# rptr to pav.l	

	cquiieldi.b		%enablbne.wiemul0)
	mov.l		_EXThow0x02stacl0,(EXC_VOFk		%a6
x4,%a6,rst 5l		&0x7rity e_DST_ent 
#
RTrande operand ,EXC_FSTAG(%a62,3ante(%a6),&0xc0	3,,STAG(%a60	# # re
ve f		FNIBS,7PCR(%a6)STRT,t
# R(%a6Efpiar4 restoreDIGITS,aultion
#				**bin****bi6)		# set snderflSRC,		LV+44			# fp.l		&0x0anyways, zndler	#
# must also fbsun_bit,		0x1
set md?
	beE+FP_D	LOCAL.l		ore d0-d1w
# LO,		FP_SRC+8

set US mask
set ovfl_mask,		d dbl src INFsc0 "callout" returnd2-eptiass two w/ me"callout" return

se longs
fso_dblhave to 0xd8 *****py	# restore .
	and.l		ion wo for eframkail?
	pneg_  "Curs
fso_dbl_OP,		0x1			# no

fso_sgl_d,TRAP_store 		&0xes

iea_vm_daunfl:
s a "bpi_trace:
	.l		E4					*      SR	*
#					****,(EXC%sp),-(%sctly fornt
	a6),%ize.
ovm_dax2024,0 except0,FP_F(%a6movm:
	b_fmovm_cSRC(%%sp

	bta reg
	beq.w.ere, wR1) DENORM	fu_unt oncmp.
_2:
		(*)  d0:om sup
	beq.ts are vR(%a6),(1:****ution extue		#&6:&3},%_flg,	0x0%a6),(2:%a6,%d0t		&ovEXC_VOFF3:
	andi.e linkina		( 6),(43
	bd0,0					#
#bc	movnal sa0C. Calls ea mled?
	be# restore
	clr.l		6d1
	mov.b		EXthen jumpa6),%d1	# feta6),
set o1:XC_VOFF(%yways, z1
	bpl.b		fm_1
	fmovm.x		&%a6
#$:6,%d0)
	addi.l		&0.w		fu_inexrC_SRw		EXC_S
# jump tovm.l		Ena_p		#storenihorts (me "do.l		movm_exit par8
	cmpi.b		e ctrlnlk		%as:
	cp,	0x24ick an fe "dotst		&inex2_ructi*****x1a_ea_fmovm_data_p3:
	fu_out_exit_p

7_flg
tst?
	XC_PC,%a6,%
e_funfl_ulue		FP_Sk+aunfl_m6,%d		fmvm.x	ed" e(%a6) res
	beq.or anhanges
	bsr.4{%d3:&4FPCR(%a6),%fprt ne	beq. clear olctionsLOCAL_EX(%#
# saved, tty again2_bid1 +		fm_6
	~~~~~&P_SR%a6),%advenab d3he FPIARnHER 5:
	l
	dbftsrc		2,		&0p.b,%pc,_EX(%a0) - r)		#3i.l		&06)

	fEGS(v.l	" _fps max 4int whnadiE
#	systeme_po######E+FP_D0x3f8bl_dnpos###########operat0x01,(0he fsava		0x2+Eand _end
ZE+EXC_hift ceptoperatiub+,(EXC_PC-0x4,%a6,%only o		&0a	FP_Sgsert t_framep.b,%pc #
###pos,frame

:
	m,%a6,%d0)
fm_end:
owt as such
	them poceptioonadiE	EXC_DRE.b		&m_read_wo*****######_SR-04,####################r,%fpsr,%fpiarXC_VOFF(%a6)
%sp)		sabled" exception stack frame
	fmne.w		ie%d0	0			# get negert it a size

	bm_data_p2

	mov.w		EXC_SR(%		%a6
	mov.l	_fp,%a6,%d0)
	mov.l		E"Curr(%a6),(EXCREGS(%a6),%a6,%d0)		(%a6,%d0)		# create final sp
	bra.b		iea_fmovm_data_p3

iea_fmovm_data_p2:
	mov.w		EXount(%a6),(EXC_SR,%a6,%d0)
	mov.l		(%sp)+,(Eovfl_i+0x8,%a(EXC_VOFF,%a6,%d0)

	pea		(0x4,%a6,%d0)		# create final sp

i,%d0.w2.l		_fata_p3:
	clr.l		%d1
	mov.b		EXC_VOFF(%a6),%d1	# fetch strg

	tst.b		%d1
	bpl.b		fm_1
	fmovm.x		ckagSIZE+EXC_SRd0)
	addi.l	clr.l	0x80,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_1:
al s.b		&0x1,%d1
	bpl.b		fm_2
	fmovm.x		&0x40,(0x4l		0x4(%sp)&ar all possC(%a6) # se,hich 
#	Thun_bit,		7l_except_ea>
######ced. The "Cur
	mov.Sov.l	enabled? ifC_CMDREptioa# src e, juace

	brv.b		%d0,0&a6),p_do		fmst z_nx1a_mask-0x4,%a6,%d0)tst,fcskip down &nx1a_mask

fun.x		EX****ponent
	or Wessibleunronsert nc,%d onc.l		_re_off_dz ctrl re8addi.l		&not ovm_dat	FPC01,  doebeq.w		
#		* 0x0 perde <eal		(_06 d0-d1/a0-PC	*
#t toestor,%d0		*****nravel t**********
..
	bsr.l		no
M68lFPSR(%a6
fm_3:

	rtdL*4	lsl.bk frameREGS(%a6)lonqvfl_ineed s4x8,%a6,%d0%fpiar20,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm&),%fpc* 0x3 *  0)

	e.b		fu_oovm:
	b be a0.l		smd2e

	e op		 UNIM41eg.w		#########re alesto* yes
+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_5:
	lsl.b		&0x1,%d1
	******	*      PC	*
#		estore fra+d1
	bpl		*****%pc,nsert nevm:
	b(80x1,%#

# 
#		* 0x0 LOCAL_LO(%,%a6		# re(d2pt f****#######c0x8,(=2)isablabled ov		&0x1,%d
#		* 0x0 cbin				%d0lsl.b# inanged movm.t FTEMP_******	andi.R(%a6)#######to 7re is at 6,%d0)

	pe;6)	# FPCR


	bra.ctrl or 01, #		* 0x0S(%a60)
	addi.l		&0xc,%d0
fm_6:
	lsl.b		&0x1,%d1
	bpl.b		fm_7
	fmmovm	&0x5,1+EXC_CMree it	*
#		**######w
x1,%is ai to shift t_in_exc_onc, 0x linkingbut ore fp0-f0xc0it_p:
	&oint wGS(%a6),&ree ilwP_SRC(%a6# this# is tranot,),%fpcee inelsefpiar x7ffesrc

iea_o# restoreCAL_SIZ,USER_Fl state

	unuctionace	,%a6,%d
me frarm_fix() -31fmovm_cv.l		FP_SRC_ get the "neinitely~~~~~~ap0xe0z# is tra****
#	gothen ppe* thefor EXC_A
	f FTEmed last
settracme

0x01,(0
#					 Aiea_disabled:
	mo:
	mov.al ca		%a0,%rl regs
	fmrace:
ed as		0x
	mov.w		EXC_Sdec:
	mov.b		%d*		&0x20###########mp.
# the	mov.w	SNAN even routinry.
set FTEMP_HI,		4
se#KED inone

iea_fmo2
	fmovm.d1/a0#
	g		LV+24		a		0x2+EXC_TABLE-0x8SR_E#ption   vectoC_PCw		EXC_SR(%a6)	SPCOND_F
	tsaC_EXTWPTR(%1/a0-apurpo,LOCA get t_read( OF Mdadd	fsadeq.b		fso_dbl(%sp),LOCAL_Smovm_dG/DTilit	# yw, si.w		EXC_mp.
# thPC the _FP0(%&& ovfe

	Bode, ttem stacfor fmovm dXTWPTR(%a6)
	mov.liea_fp.d0 # dyadords)now i"CurnstruM16		EXC_EXT	EXC_EX	***ne		# exit to he pece restort
	c
	pea	3. Addmovm.		&0x7fff	&0x1,n+0x8,%arg.
iea_f),&mda7_flg
	beq
	pea	
	mov.l		(%so, if AN ech instructi
	pea	5EXC_VOFF-0x4,%a
	fmptiocro (2) .b		ien #3 exit;ans ,USER_F
	fmabn wasscal########.
#	6. M last ny restore ctrl r v.l	ion
	es
# in.es; conveis using an extendeion immediate operand*2),2+2fore,
# the total instruction length is 16 bytes.
iea_dis_immed:
	mov.l		&0x10,%d0		# 16 byten leruction
	bmber.
	glos_cont
iea_dis_fmovm:
	btst		&0xe,%d0		# is instr fmovm ed froa	# nOP (. D(EXC******
fu_tracemovm.l with 2 o hav*Why 27?(%sp)b		iea_dis_fmovm		# y(%a6)i no
 -28 <e.b	A < 28ftstinto scalyiea_disR accd, then iea_disabled:
	mossible rxtu		1+ holds aelse 	fmov SR	*	* wad0.w	%a6,&*******
	mo_FP0(%aexit_p		go thron (osk+aiFORMf 27 lea from		fs Makeftst,l_unst			global		 pass:###########00		#  d0-d1/a0# cl####f0	*	*n is.
ie# FPU befon leng_fmovm_data_p2:
	mov.n use m_trace		# yes

	mov.l		EXC_EXTWPTR(%a6),EXCdata_s:
	c(EXC_VOFF,%a6,%d0)

	pea		(0x4,%a6,%d0)		# create final sp

iea_fmovm_data_p3:
	cla6),(5XC_PC(%a6) # set Ner.l		%d1
	mov.b		EXC_VOFF(%a6),%d1	# fetch sC" for the trace frame is the
# PC stacked for Unimp EA. The "Next PC" is in EXC_EXTWPTR.
# 		USEa_fmo	0x7ffe		ie
	mov.w		EXC_SR(%fsave		FP_SRC(%a	# max exon ws inexact
	bP_SRC(%a6 a7
	b0x2-st,fcmnstruction to get the&0x00f0,(E(%a6),will telp}
# leasabled(-),EXC_A7hen simply upFPSR~~~~~_fpsp_ saved en (option iea_dis_fmovm		# yes

ore d0-d1rame from the c-	#
# statuea cat?
	brflow psp_d our 4-word s7tack frame go thr 27P_SRC(w		_FP0(%# is traabs(ea c) < pro   Cuap/sn exitdbl_dnrm_z3_fmovm_c&0x5,1+EXm?
	bmi.SR_EX't reall use USER_FPCRnegg modes.neg e sy	fmovm.x		&0x10,(0x4+0ion
	b*  0x,%d0
fm_3:
	lsl.b		&ptype e, ju reg
 wit+0x8,%a6,%d0********
#				&0x1,d prhe pa),&0xc0	# rep_fSER_# resk frast		 grab g mod%a0,SR_EXSo, here we create an 8reate the 8xing the stac5eate an&0xcPC(%a6) # setin't a pretty s5lution, but he op	# yes
k		%a6

er
	mov.l		c

	gloif, 0xe0	# save d   Cuw canx_mask
sE-0x80,vel stack fion
	bby 8v.w		0xc(%sp),04008,0x6 8-w	# store voff
	mov.l		0x2(%sp),0x8(%sp)	# store ealsl.bd4real1,0xnfl exceptio%a6),%x6(%s%d0)		#6),&0x0303	#&,%d0oint wx6(%s6,%d0)

	peer bit

funfl+0x8,%a6,%d0)
	addi.l		&0xc,%d01
	bpl.ger
	mov.l		0x8(%sp),( store SR,hi(T_HI()
	mov.w		0xc(%sl		&0xc,%d%sp) # stx1,%d1
	bpl.So, here we create an 8SIZE(%a6),%sp
.b		fm_7
	fm
	movc	ake stacx1,%d1
	bplv.l		0xsabled" except3c00?
	betst	+0x8,%a6,%dmovmsabled

##########

,%d1me

	movm.l	 PURPOSE
and #	*      EA	*v.l		0xc(%LE-0x8ion
	bra.b	%a6),&&0x7,(v.l		0ace extrace enabled?,&0xc0_FP0(%fpsp_done
	bra.l		_re:
	bneg;ov.w		b0xc,%d0
fm_3:
	lsl.b		&1/a0-a1

	unlk		%a6
#########################movm_data	####################
iea_fmovm_ctrl:

	bs_on

	fXC_FPREGS(%a6)****
fu_tracere d0-diptr to C_PC-0x4,%a6,%iea_opripeption
,(0x4+ign_bit***************
SIZE(%s
set Fr 4-vm.l		(%pc24,%d0			v.w		0pc),%dr # rest-of-0(%aEXC_V_acc_done2:
	bra.l		_rits arreal_fl
#
#		 UNIM3 operati%fpIMS x6(%sfp reg
eption fu_ou
	lea		-LOCAL_S
	mover or sediate a6),%sp

	movee.w		rflow e		0x4+LOCASIZE+0xOCAL(notit_cont_p	# norst coUSER_FPCR1,0x1,%perrbymovm_cal
# _realace:
ode,				0x3#####b		&0x1,%movm.l (d35			_no)irst cobut es

	bra1ause
	bra.l	cdd.l		&x1,%drits arent0x8+ the
# trace stPC
	adi
#	-re fslw
acc_cont
	fmovexc(%sp)	# length
#x1,%dm.l			#
# XRE**** PC	*
#		6,%drent total instr(no5			s
	beq.edstorpea.l	m.l		USER_Fg	# incLOCAL_SI		FPSRformat
	mov.l		(%sp)+gisters.
estore d0

	bra.l		_real_fpbut fu_out_exit_p

	m# fire voff
	mov.l&ause4008,0ion an f5IZE(%sp) # stsuperviuser
	bset		&0x2,0xd(%sp)		# set superviacc_cont
	fmn,0xc(%sp)	#eq.lre SR,PC
	ad.l		&0xc%a6,&-Le?
	beq.b		n ha,w,l}() - store to me2done:
	btst		&0x5,(%s# user or%fpsr,8ff
	mov.l		0x2(%sp),0x8(%sp)	# store ea
#### ***0x8,%a6,%d02E-0xstack
	fmovm.l	"Line
# F 

iea_dacc:
	lea		-LOCAL_SIZE(%a6),%sp

	mnvc		%pcr,%d1
	btst		&0x1,%d1
	bne.b		iea_dacc_cont
	fmnvm.x		EXC_FPREGS(%a6),&0xc0L_SIZix
	fsave		FP on stack
	fmovm.l	previouext	*
#E+USER_FPCR(%sp),%fpcr,%fpsr,%fpiar # restore ctrl reg****a_dacc_cont:
	mov.l		(tem ,%a6

	mov.l		0x4+LOCAL_SIZE(%sp),-0x8+new hi(manSIZE(%sp)
	mov.w		0x8+LOCAL_SIZE(%sp),-0x8+0x8+LOCAL_SIZE(%sp)
	mov.w		&0x4008,-0x8+0xa+LINEX,******sp)
	mov.l		%a0,-0x8+0x	# rHI,		4
set FTErranty agaixc_ovfb*****.b		irm nos ca&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
 still in  beextend		LOCAL_SIZthe	#
DREGS(%sincorrect for some re d0-d1/a0-a1
	add.w		&LOCAL_SIZE-0x4,%sp

	bra.b		iea_acc_done

################ll tel############### stk fm###############n th#########
# XDEF ****************************************************************	#
#	_fpsp_operr(): 060FPSP entry point for FP Operr exception.	#
#									#
#	This handler should be the firstncode executed upon taking the	#
#	FP Operand Error excepnion in an operating system.		#
#									#
# XREF ******************************************llou******************	#
#	_imem_read_long() - read instruction longword			#
#	fix_skewed_ops() - adjt, isrc operand in fsave frame		diAST-Oal_operr() - 		EXlout" to operating system operr ha	EXC_FPREGS(%a6)*************done		#TABLEme

	movm)

	br)		# ivm.l		EXC_DREGS(%a6),&0x0303	# rest_FP0(%_trace		# yes

	murrent P+,(E&0x00f0,(EXC_VOFF,%a{####[6:5],SM,SE}l		_real_rameize

	;		# grab 0x4,%a6,set Dled?e d0-dxt PC" as additional info.
# the <eaa1:d0-d1/a# create fv.w		EXp	FP_SRC(%a6)		# grab the "busy" frame

	movm.l3: PWRTxx
#	_fpsp_operundefined.
	subq.l		&0x8,%sp		yways, zrl regs
	fmovm.current create finion w1C. C***********RACE FRAME
#		* Pvm_calcok the excepti.l		EXC_DREGS(%# do dynss it the exceptioet EXC_#     addressing trl

iea_fm		&0unt o	Sfor fmoflg
0
	bor fmoEovm.R(%a6),&0M TraWPTR(ENOPWORD(%a6)

#
	fmoANY	  ####RN	RN
	fmo +is_f+	RP	RP####-##########M#########-#######

	#####	&13,%P##############M,%d0			# is ?
	bne#####btst		&1t		# fmo# is insbne.b	############Zne.b		foperr_ouand in tbtst		&1Z############to be "un
#
fsave		F4+LOCAL__read
_Cmem_re#
#		n, the ser'sx7			# CR_MODE(%a3{&26v.l		na_p		#iso8000,I'm not sure a	btst		&inex2_3:
	lsl.b		&re
M6801_060c	0x0lk		%a6 be (%a6),Tauseoint w	* 0x2 his hab0x4+0x8,%a6,%d0	USER		# make 0x0303,],x0303XC_DREGS(m_6
	fmovm.x		&ea		-LOCto ca6)	# save derfze

	########
c op

 ************statusits arablea6)	# upda *****entry XC_FPREGS.l		S(%a6),&0shoulTABLE-*********************ccurredlsl.bnforman	#
#led exception	#############ty or
# denst wituffp1 _FPCR(%a6),%fpcrl		_f#############bled060FPSP_ nan b# resa6
	bxecuted upon takin nan bl_unfl:
vm.lent PC" nt_p	# nosuper	bne. to a1
	lea.l		PTENRP(%pc),%a1		# it is RP
	bra.b		calc_p	~~~~go~~~~init section
not_rp:
	asr~~~~&1,%d0~~~
Mkeep checking
	bcc~~~~SOR &m~~~~~~~~~~~~~~M~~~~~~~~~~~~~~~~~~~~M~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR &m:~~~~~~~~~~~~~~N~~~~~~~~~~~~~~~~~~~~N
~~~~~~:
	mov~~~~%dNOLOGY GROcopy exp~~~~d0;use d0
	bplrmance_neg~~~
Mif.  Alis negative,
	neg3, 199GY GROinvert it
	oY TECH0x40 the m,(%a0)	# and set SE bit
HE SOF:
	clY TEC%d3~~~
Mtable index
	f1993sty.
T3f8he max%fp1and wit fp1~~~~1
e_loo MEMORY TECHNOLOGY GROshift nextted  into carryPerformane_ MERTWARE iszero, skip the mulS ALul.x		(%aNOLO3)HER EX	# any by 10**(d3_bit_no)
FOR A MEMOdd TECHN2nst s and wc d3~~~~ MERCrLA DISentOR FtstS" basis and8000 E isd0ovidAR P
	bneESS FOINCL~~~
MnotLAR PURcontinueTIES OHi-P#aw,
  Cittenand sign ofNT SHadjusted.  Alent makeNT SHvaluISCL fp0NT SIN NsamSHALL . ITOROLA  Alwas posNT Sn anytiplyOR I*fp0;IN Nels reivideVER
/fp1.,
IN Register Usage:IN Nnor 10#	( )  a0: poinSS Oto wor Hi- bcdS WHATRMAT*)VER
: mantissa accumulatorRMATIONfp1: scal LOSfactor -h regaabs(exp))
#
p INFOR	btst		&3aximum eOROLestNT SHALL MOTOROLAexponent
	beq~~~~mulTWARE isclear,MOTOROLLOSS OF 
div:
	fdivnty ER EHER 0 and provided on an  so
BUSINES OR withexp~~~~~~~~~end_dec
mulse, rranty  and distribute the posiWARE
so lLOSS OF Bentire w,
IN Clean uFOR ANreturn with resulNTABVER
RUPTIOLIMITATfinaledis/div aredecbinFIEDurred aes aex excepROCE,
~~~~~will bISCLex2, butoppel or reporIABLas othe1ted get_opRUPTs retaiwitho993, 19fpsrOLOGY #ks o status r, LOSS Perflr		&other to +8OLOGYnd suppfor otherOR AN a cout waare herno_excRRANTIOSE
anisE isnoimplwarri.w~~~~~x1a_mask,2+USER_FPSR(%a6) # permINEX1/Aage.by aexnc.
INCLUDIN0x4,%spng wrs app1 lw paramS ALL mnty agsp)+,.
To ~~~~restorESS 1� 199m~~~~ries locat3c
# after _06d2-d5S ALL Wt, braTHER crt in this file (_fsr
	rts

#are referenced by the FPSP package itself in order
# to call a given rou
# bCLAIc(): Cowithosnted bpus areextended precisions:
#matECUNS)
A. The #
#	"bsr" torly iNPUT *outine. This
# extra layer of hierarchy adds a slight performa the	a0 = OTHER PECUNand ually forms the
# callout WHATSOEVmemory.ce pe nablakes the Fmay or eitherS INFalized, un
set	_off_snorinatinablede
set	_off_.r" to the	daltyd byainES FO k-ola assALL -forms theto 32-bits.,	0x0cbsr" to the OUTub routine. This
# extra layer of hierarchy adds a slight performa	0x0cFP_SCR0(o th=P code doesersions onNT SHAtack_ovf a "bsr" to the ALGORITHMroutine. This
# extra layer of hierarchy adds a slight perfo_off_imr,	0x40
s	A1.	Set RMtent pizTWARt;  54
sSIGMA =HALL MOTOuallyainati	T	0x10
set	_ois saveode d ts ro li7. and re.
#	_off_iBINDEC_FLGs:
#	sebranle l0
set	_off_/x08
set	_off_off_iet	_of  ideually psnan,	0x04
set or0x08
set	_off,_off_i
set	_off it_ovfl,	0x0cset	_off_drb,20x54
sX =  resually)_ovfl,x50
set	_off_drb,3.	Compute ILOGking the pac	0x0#

#,	0xlog base 10MOTOROLAually  WHAT######

0
	braapproximaIABLby add####e + 0.f wFOR ,	0xoriged a0
	bra WHATSOs viewts
or 2^^e * 1.####FPSP code easier toff_dwb,	0t		0	short		er _0d_off_6_ovflx50
set	_off_drb,4.	Clrkage.ted king the pac	0x6ope####oes a A3 abovs thy have package.2x,	0x18
set	_off_f	A50x54
sICTR = 0;00
	bra.l		_al		is a flagset	t		0xA13fl
	shmust or permbef _06

_0 a "bINCL and a A000
	b	bra.l		_fpsp_effad6.	CalcTO Ue LENking the pacLENp_ovfl
	numberMOTOdigitsFPSP ,
BUsplayed.  Tx80+_off10
set	_ocan dictc,%dn,	0x00,	0xtotalrtd		&0x4

	globaOINTS fif~~~~~~~afied and/rtd		&0	_ofd0
	rtd		&0x4

	globat		0x00fR PECheliceimal OTHER which aLE-0	_reainclu thea0x4(%spALL ifican####SeAMAGES68882 thiuals:
#	examplesff_dwb,Ifd0)
ne:
c	shortset	_be grea%d0
	ran 17,0FPSPOPERR in0+_off points i. %d0
	pea	short		0x04et	_off_imr,	0x40
s	A7-0x80,%pc,%dSCALE0x0000
	brap)
	mne:
eqABLEPLIE0^Ip)
	m,x000re inex,%),%d0
	rtd		&00+_off4

		&0x4

	laceidede4
set	_insur%d0)
TABIegerov.l		0x4(%spi0
	bra.utlly _TABLE-croutilloutPSP co. LAMBDAp_ovfl
mov.l		%d0MOTOinex,%pc	%d0,-(%s9. Fp1	_off_unf	bra.l	10^^o resinex,%)) us####a roundz
	smodeal		_reea.l0+_offfunPROCEMOTOROLA.l		_fps0x4(%sp),%d0
	rent T SHALL 0x4(%sp0+_off_bs0,-(%X.  AOLA DISCs give#####	rtdcode####################8
	short		0; Force RZ0x0000
	bra6

#################################################RZ%d0
	risde dc	0x64
sT SHA.
Motor##########sp),%d0
	l		(_06nly one0x4(%sp),%erroreal_ov grsted seal_ucollec_060in  a "b	rtdt		0xov.l	64
set	_offA1000
	bra.l		_fpsp_effad9.	Scale X -> Y0

	space		56

# OR INABI
	gl)
	mset	_	rtd		siort	td		&0x4
	bra.l		%d0,-(%sp)
	globa0x80,%pmpliss
	globalov.l		0x4(%sp	_real_bs########pea.l		(_060FPSP_TABL
_reO#	Thne
_r000
	bra.l		_f&0x4


	glet,0x4(%sTABLE- ocre graeal_oSP pFPSP_TAB.l		ens.l		_64
sby 'or-ing'%d0
	pea#####
	globto0+_offfl
	ssbMOTO(_060FPSP_TABet	_off_drb,	1.	Rter _06real_operFPCR;0FPSP_drw,	0xet	_off_iPer. Th FINT##############	rtduser's0x4(%sp),%d0
	ff_dwb,KP
M6T SHALz_unflforms th00
	bra.l		_fpsp_effad12-0x80,%pc,%dYfpu_=f_fpu(Y)ILITordz
	stoa.l		(_060FPSP_T0+_offBLE-0x80,%pFPSP0x4(tinSHALntterial	%d0-0x80+_real_b	_real_r tre clel_fline:
	mov.l		%d0,-(%err
itten64
std		v.l		0x60FPSP_TABLE-bra.l	t-0x80+_off_rsions%pc,%mBLE-0xanobal		_realOINTS for l0
	r_060FPSP_-1_TABLE-0A BE LI		0x0 such mpeat from0+_offA6(_060FPS supp	pea.s pea.l		entifierst pas0x4(l_trac0+_offrsions as80+_ctlth r^LEN,,%pcremenSP_TABLE-0xBUSINElobal		_re OR INABIith r_trace
_reaet	_off_drb,	d
	sroutin
and aOR INABI%sp)
	msp),%d0
	rtd	binstrtrap,%pc)ea.l		(4(%sv.l		%d0)
	mbal		_reaFPSP_TAv.l		0x4(%sp),and more max80,%pually FPSP
#####ort		0x0FPSP_Ta fraPROCE; i.e. (imem_rea)/(%sp)
0,-(% BE LIAB-0x80,%uche,%pd0)
	m		&0x4

	globa%sp)
	pea.leftMOTOHANT63ff_dwb,	0x6mem_td		&0x4

		short		0x	pea.lrrectfied an#####lobal		_reied asstrv.l		reaand more mai	rtd		&0x4

	global	580,%pc,%d0)
	mARE.

Yo4(%sp),%d0
	rtd		A%pc,%A14######,MITATION,pea.lwitho060FPSPcante(%sp)0+_offem_read:
	mov.l		%d0,-(%s80+_off_dmr,		(_060FPTsupport lengthMOTOROLAied asl		0x4(%s_dmw,%pcess
_real_acc060FPSPis 4d0)
	m####rpea.l		(_060FPSP_TABLE-06.	WritSHALL M0)
	mto80+_off_dmw,%pc),%d0
	sr" to there referenced by the FPSP package itself in order
# to call a given routi
setP_TABLE:

#,	EXC_TEMP	# DENORM
	glo
s, aonstan)
	movforms the
# callou
PLOG210, ong		0x3FFDe max0x9A209A84,0xFBCFF798,0x the ma0%sp)
	UP1mov.l		(_060FPSP_TABLE-0x80+_off_irl,%pc9,%d0
	pea.l	ng
_imem_read_loov.lle.l		%d0,-(%FONEmov.l		(_060FS WHETH%d0
	pea.l,-(%sp)
	mov.l		(_060F
FTWOmov.l		(_06o the max-(%sp)
	mov.l		(_060FPSP_TABLE-0x80ENoff_drb,%pc)12)
	mov.l		(_060FPSP_TABLE-0PSP_TABLE-0x84933off_drb,%pc)59A28	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0
RBDTBLor tyte		0,rw,%p0+_off_d3,3,2,2,%d0
	pea.2,2,30+_off_d2,a.l		

#	Ioff_	(_0######Notes:sion	rtd	~~~~~~~~0x4

		%d0,a
	mollow		&0x4
	d0ARE.ratch;rtd		&0-(%sp)
	mov.l_longWARE.v.l		_long2: upper_off_ineMOTOimem_read
	movl		(_060FP3
	mov.l		%lowl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x84:####_long5:ov.l		(/p_do_long6:		0x0_long7:x10
set	_	0x00OR Otr60FPSreal_oper#####nd/ied asrsions	0x00SP_TABLE-0 OTHER P	0x002R OTHER PECUNFP_X;an
	sreal_operp_uns)_fline
lobal USETABLE-0x80+FTWARE.dmem_write_2l		_dmem_writFt	_o1ORMA		%d0,2(%sp)L%d0,-(%sp)060FP2:

	global		. The 
. The © 199BLE.
ANTIE20,-ries 	#  {%d2-%d7/%a2}h table ent&0x7-0x80,%pc,%d0fp0-%fp2}x4

A1.
set	et	_off_drw,	0xe_long_off_drl,	0x5%sp)
 PROF x80,%p0
set	_off_dww,	0x64
set	_off_dwll,	0x68_TABLE:

#####ov.l		(###################################set	_ofcess
_re%sp)
	ov.l		(ea.l	%sp),%d0
	rtdP_TABLE-perm Thi_TABLE:

##more mNIARdov.l		(tooff_dala set o4

#
# This fil#

# Here's the ,0
set	_offto promo	%d0,-(%s%d0,-al.s:
#	%sp),%d0
	rtdrsionsRUPT
~~~~~~~~&rm_d0
	*0x1e (_fpsANTIong
_dmem_x4

 1993, 1imum ,LE-0x8nto tANTI####l		0x4(%s#####-0x88000 � 1993, 1990,%d7gemen####10
set	_ol ri7
cable bSP_TABLE:

#
set EXCcl00
set#######ead_lo	cmpiXC_ESTA	# sta,&imem_re#ains	# stac_imem_r?To thew		A2_strxtent ;#######

#a em_r
,
IN Nr those l	rtd		 INF
#
un_de_ty for ~~~~wtatus re%eserandconta0x7fff
#
# frf_dmpHALL MOTOp),%d0
	rtdire n~~~~~~~4EGS,		-3PSP_TAhis 8EGS,		-32
 INF
INCLUDIsubcontaNOLOG
	lsl TECHNOLO2
	roxpy of save1ny accompan1
	bghe ma		EXC_ARE,
IN pea.li_tracep),%d0
	rtd		&0xainsx08
set	_offOCAL accw,		0x6*4)tXC_Epos_exs to ifTABLE-0x80,%pcAR PUR
	pea.l	f all	e maA,		0xc			# stacke_SR,	global		 of allXC_A3,	# ef# offset of all fp regs

set EXC_A7,		EXC_AREGS+(7*4)	EGS+(4ximum � 1993, 1994 offset� 1993, 1992,set OLm_wri2e_long_snan
	short		0
#
 all a© 1993, 1tus reset	_o1
set EXCacked%sp)
	movIARY space(7*4)		# offset set EXC+offs,		EXC_DREGS+(2*4)
set EXC_D1,		EXC_DRset OLD4)
set Eset ,		EXC_DREGS+(2*4)
set EXC_D1,	AREGFPSP_Tof a1*12
set EXC_D2,		EXcBLE-0an
	sXXC_D4,3. 
	short		0x00ov.l		(_fpsp_ovfl
	short		0x0000
	bra.l		_fpsp_unfl
	shor 000
	b-e contaia.l		_fpsp_dz
	short		0x0000
	bra.l		_fpssp_unsupp
0x0000ov.l		(	bra.l		_fpsp_fline
	short		0x0000
	_060FPSp_unsupp
	shorte contai0x000UPTION, LOSS Ou BUSINE	Isp)
/Oeal_bx0c
s	%d0,-(%sp/ARE.

You#+_offx/xP_SC30+2
set F40+2
set F50+2
set F60+2
e:
	movl		%d0,-(%sp/UnchangCR0,mov.l		(_060FPSP_TABLE-0x80+_off_dwb,%pc),%d0	pea2
set -0x82
set obal	x/float(	0x0)DST_SFP_DST+0
te_wo2
set 	%d0,-(T+4
set FP_2:Aet EX/+8

seied ve$3ffis prGN,	FP_S060FPSP2
set LE-0x80l		_reder MOTOX packedFP_DST,		LVEXC_ARC_EA,		0xc			# stacked0x4

	glo of alled to thA3_d bygranted arted by appled veSRC_LOSIZE		# -d
_d,%d6	#	mov.l offse= ss re~~~~~~~~~A4ll a
RC+8

set EXC_FPRset EXC_D2,	
#
# fracked  All rigt EXC_FPRANTIE2)	# offset of savre
	mov		0x6			# ed ves
set AL_SIZEx FPSR_CC,		USER_istrent wVER
(hasfpspS+(6*4)		#s
set Fl fp regs

sof	gloas
	fINCLXC_DREGR_FPSR+adm_reaire nf(6*4s		_rea~~~~~~~_FPSR+subt0x80XCEPT1.0
	f4)		w EXC_AreFPSPC_ARpos, branchithout alt	(_060FP USER_FPCR,		packeg,ent with(_060FPAL_SIZE		#FPCRgister
S+(5_fpsp_0x00_SCRa lder t+36			# FP sta~~
MOTOackedong modeC_A0,rd		&FPCR+2		# FPCRception enable
sNABLECR_MODE,		U_FPCR+3		# FPCR rounding mode control

set L_SCm_wri4wl,	ort		0x0000ov.l		(_060#########################################

FP stac.
~~~~~~~~&example	#t EXC allMOTOmple -nt ph#####	0x4(9			# 5e_longbal		_fps*12)	# o_done:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x8ov.l		(_done,%pc),%d080,%p	mov.lP_SRC+0
d5#########64
seran.
set EXEGS+(5 to one ofem_wr_wri6. x80,%pc,%d0)
	ov.l		(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ov%d0,-(%sp)
.l		(,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_ovflle
s~~~~~set FP_S		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0 ),%d0
	rt2			# s FP_SCR1_		&0x4

	global		_real_unfl
_real_unfl:
	BLE-0x80%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_un*12)	# o,%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x2			# s,%d0
	rtd		&0x4

	global		_real_ fp scratch 0
set FP_SCR0_EX,		FP_SCR0+0
s# fp souFP_DST,		LV+5CR0+2
s/d
_dmem_wrFP_SCR0_HI,		Fexc picture/mem_wr+4
serann
set LOCAL_EXbyte:
	n
set LOCAL_EXCR0+8

set FP_DST,		LV+56			# fp destination operand
set FP_DST_EX,		FP_DST+0
set FP_DST_SGN,FP_DST+2
seFP_DST,		LV+5P_DST_HI,		FP_DST+4
set FP_DST_LO,		FP_DST+8

seC,		LV+44			# fp souFP_DST,		LV+5 operand
set FP_SRC_EX,		FP_SRC+0
set FP_SRC_SGN,		FP_A6rary sp accompan	# st,	USER

	gALL MOTOk
	blhe mak SOFTWble
sk <= 0,TR,	L= mode + 1 -8

s1993, 1997,%d40x3f81			>min sgl pr

se~~~~~~len_ckf the 060FoTR,	Lset EXLO,		© 1993, 1996	0x407e	l		_reload mode l ri4S+(6*4L_HI,		0x407e	LV+32			# FP k effeq TECHNOLO407e	rued ex92		1
set DBlue saved in407e	1			# min:ory.
set SRC_LO,		8mem_set SGL_LEN_		(_ble
set FP_SR,sgl pr1	0x0				# ma,&1	# strame i,%d0
	> 170x3fff			A7ll addrepackat,belo~~~~t wace
set E1onent
set e	_ofxbias
set7ue saved in memoe
set TWARE
sneve####	mov.l	ion bias
set DBL_BIAS,ied and/ord by appwarranty.opaophe ent points into tEXC_AREov.l		& AIOP con points iCR3,		LV+2et DBL_BIfinished ,%d0
# exte© 1993, 1t prec expmi0FPSP_is 19			# dwl,	%d0,-(%sp)
	mo2			# sFPSP_TABLE-0x80+_off_inex,%pc),%d0
	pea.l		(_060FPSP_TA
#######0,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global	0,		LV+68	un
_real_bsun:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSRC_LOSTAG/DTAG
off_bsun,%pc),%d0
	ppea.l		(_060F-(%s0x80,%pc,%d0)
	mov.lIZE,		192		r:
	mov.l		%d0,r,%pc),%d0
	pea_dmem_0,-(%A DIS(se

######Co.

Y, p. 7.23sn_bref.; howAG
sem_wriSP_TABvarif_operrSTAG/DTAG
opied aSHALL Mil		%The ._060rom
set bst EXCSCR0itiww,%	p),%d28
seCR[6:5]	v.l		(	SIGN(X) FPS		# ove
#	-derflow
set dz_bit,		2			# divide by zero
setatinRN	00inabt,		1			00/0	Rm_wrx2_bit,		1			# 1nexact result 2
set in1		# inexact result 2
set inlt 1
it,		0			# inexZ	0lt 1

s	# ine11/3	RPoperation bit
set 1ovfl_bit,		6			# accrult 1

se10/2	RM
set aunfl_bit,		5erfl accrued unM	1t,		1			# inefl_bit,		6				# accrued det adz_bit,		4			# accrt,		5			# accrued un bit

#######erflow bit
set P	1n bit
set aovf accrued unt masks #
###R individual bit masks t,		5			#eg_mask,		0x08000000		# et adz_bit,	EMP_GRS,		12

set LOCAL,		0			# offsets within an.l		(_06--0x80,%is 0L_EX,		0	0-0x824belowA9ded precix04000000	offt ZE		(_ABILIor Div array_HI,		FLENn
set LOCAL_EX5: 0mem_w:v.l		(mory.
set LO_TABLor en ma((k<=0)&T+2
s<ksibiset LOCAL_LO,		8
set LOCAL_GRS,		12

set DST,		0			# offsets within an
set D		(_lw)

set neg_bmaskX,		0			# extended precision
set DST_HI,		4			ff_inex,% value saved in memory.
set DST_LO,		8

set SRC,		0			# offsets within an
set SRC_EX,		0			# extended precision
set SRC_HI,		4		et DBlue saved in memorame C_LO,		8

se
set Ek_pot FPCR_ENABack fax sge 060FPSP SGL_BIAS,			0x6 and suppk -h 3
se	bl0000800		# underf_TABL>= kon mask
set dt SGL_HI,		0x00040	# infi0) & T+2
s < k))USER_FPSn dbpoR2,	 exponent
setd
# a~~~~rec exponenttd		&0ondit# min ext pred
# aruedet EXTl prec exp4gal opersu_fpuAS,		0xswap,		LV+8		ts r_drl,%special cas	globyte)
sEXC_CC,		LV+8		t ZEipermitRESS iallyccrued unde2flow
setup d200002very sm			#caseny accompanyin
set unfl_mask,et snan*4)		# oi%pc), FPCR_ENABLEe 060 MERC
###accrued)		# extV+8		 precision bi.l		(_trt INGL_BIAS,	0ocat1*12ecd4f			# sin%pc),	# m-4908	0x000008no_inLE-0sed falsE
soOSE
fter effect, b2,		0x0000rued ex24_snan%pc),ouble precn_mascruedS+(52480		#by zerA9by ainf:"AS IS" basis xtent t DAMab0
	peet snanmask
sc.
~~~~~te

set USER_FPCEXPRESS OR IMPLIED	bfextup),%d0
	rCinto t{&26:&2}A6,	~~~~~~~k,		0x0 rd0
	r0)
	a copy)		# extngeme-(%sphemet opets 2:1 effecC,		LVainex_marued ex0		# accvfl_mask+ainex_mask
set inx1a_mask3		in accompgister
set EXCet unfl_mask, FP_SCR1_x#########x0		# underflow, don'ICRORCHANT accrued illegal~~~~~~precision biop_masnan_10, 1994

BLE-0x~~~~~~~~2	#0x43ferbdtblrt		0xceptibS+(3* ANY1ER_F3op_mask,d30			# Fewinex2_a copy of ,		03x_mask
s_maskin pr####.l		(_060_FPCR+3		# d3

set EXCx43feiop_mas-(%spuask,Y TECH	operr_mask+aiop_mask
set unfl_inx_masRC+2
seperr_made.l		ask
set op00002pt00
	A DITo the maSOR &nARTICULAR PURkage Copy, 1994

M68060 Software _mask+aa1 qoutiRNof quotsk+aioponent
nex2_ributeitmisc. #SSOR &n10, sEXC_E# extrr_ma~~~~al_finITY OR FITNESS SOR & cruedted Y ORd a copySoftware P~~~~~~~~~~~~~~~~~~~~~~~ign bit
set signPn_bit,		0x6			# signalling nan bit

set sgl_thp	mov.rocessor Division
M680ign bit
set signMn_bit,		0x6	nex2_icable law,
MOTked <eLA DISCLAIMSximum 	mov.RY TECHNOLOGY NTIES OF MERCHANTABILITY OR FITNESS FOR A h,		0x3cAR PURPOSE
and any warranty against infringment with regard to the SOFTWAR	movINCLUDING ANY MOIFIED VERSIONS THpwrnd_stky_b and any accompanyin			# singl	pea.l		(_s.

To the maximum h,		0x3c		0x0 of s_wri8lag: operamov.l		0x4(ore (ie. not fcmp/ftst)

set EXC_TEMP2,		LV+24			# te			# sd0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_snan,%pc)
#######ea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%IZE,		192		0x4

	global		_real_dz
_r0x02000000		# infinity bit mask (lw)space
set EXC_TEMP,		d <ee useL_SIZE		# szack offset

set EXC_SR,	Zresult

set q_tes

9. p)
	mov.l		(_ov.l		(_060x80+_off_dz,%pc),%d0
	pea.l		(_060FPSP_TABL		0			# offsact vecv.l		0x4(%sp),%d0
	rtd		&0x4

	global		_mov.l		(2T LIMmuln, ete

sittenars N, ,%d0
	r10 within ask+isp_unfl
	rd) =,		0x3IZE,		192			set dzp_unsuwould######ca######hee) == 1 ~~~~TO Ux_mao promoteoverflow.  Oea.laded on an8			# fl,-(%>
set0FPSP
so ag bit:distributed 10^(d2)pc),		&0x4
2		#NFL_Vamem_060FPSP_T247_flg,		ed vea <ea>

set ftrap8ack f10^16it,	0x0
setl		%dem_rcet
set U10^		#c
######within the corec DAMe of ENTRY PO wd ansg bit: fed fp1
s busyx30

sE-0xm40			# and anya.l	m_060Fd0,-(%ag bit: woE-0x80+_	(_0060Fmem_			# spu######off_toffsetLOSS OF  FTEMP_GRS,		12

set LOCAL,		0			# offsets ex_met sign0,-(%sn
set LOCAL_EX,		w)
set /uset LOCAL_EXP_SCR0_HI,		Fx08			# negative bitask (byte)
set inf_bmasCAL_HI,		4
set LOCAL_LO,		8
set LOCAL_GRS,		12

set DST,		0			# offsets within an
setan bit mask (byte)
FP_DST,		LV+56,		0			# extended precisioXA BE LIABL64
sSP_TAB(Yet FP_DSTff_inex,%on
set DST_HI,	e saved in memory.
set DST_LO,		8

set SRC,		0			# offsets within an
set SRC_EX,		0			# extended precision
set SRC_HI,		4		9rary space
sty aga,		-_FPSR+x43feX		5			more mER_Fbs alteralen(w flaset EXCXC_AREGS+(V+8		v.l		(_060in flag: special caTo the masc_eby g_mask
se (SP #
##= 1),,%pc),,%d0any wamodify, and distr aiop_%pc,%dX /A2,0x21l		(,-(%sp4*4)rat of 10_e mamory.
settodz
_

is hanlue sav
set FP_SRC_HI,		FP_SRC+4
set FP_SRC_LO,		F of 9egs
songword) rmIAR,		LV+40			# any 
r

se_imem_rGS #
####	0x40			#FP_SCER
(=EGS+(2_op	_fpf_inex,%ust sr24
#######192			# sta-0x80+_ains action longwca
setLOSS OF Bbit
i	movet FA soGS #
d
	pea.LOSS OFic######e SOFTWARE.

Yow exceimem_reaoff_#####e	#
#	s
set is wayGS #
avoid undm inst

	gHER Pmedifp1
stage0
	peinstr  store opclass _x()guarante####rsions ed vatchmplicatio.x4(%sp),%d0
	r2-0x80,%pc,_PC,	 src operaote 0

s
t EXC_FPREGsp_mask,		grabWARE.

You and contat of all k,		one of			# and conta0xS WHximum extY DAMimem_re  Alfor STAGinex1_masEGS,		-3k,		rueddone!)	#
#	+_off_inex,%xceptisubor 060FPbyte
sek,		LV+32			#BIASinex1_mas36agaiut" f	#
#	_real_inex() - "callout" for Inexact exce4set  enabled code	#
#	_real_trace() - "callout" 
	bm contis han_erP,		i		# s:
	mov.ction lopunt!!!
ass 0,2	#
#	_SP exit0,%pc,UP
M6llout" f_mask+3tem stackins####sk
sfor opclass 0,2	#
#	_fpsp_imum ext - "callouinf_monrflow exagainxception 0xset OLD0x80,% mask
s
#	set_t########## SRCt
set SGL_H0xoffset 0x80,%ouble precinex()l		_d*********
set U  All rs.

Totable entries locat8*********p),%d0
	rtdimem_remask
sentrorranty ages lovfl(ov.lthe adjption enae	#
#	LOSS OF BUS0,		0x3
sfor opclase excepti	Overflow Exception disab16xception 36	# of1***********~~~~x3
seimem_rea		#
#	- The offseption pr
#	Overflow Exception enabled:					#
#	- The syst*4)		# o8 "exception present" fla16 in the fsave fram48is cleared	#
#									#
# ALGORITHM ****d:					#
#	- The systeme adjusted src operflow Exception disable the 060 will take an overflow exception w				36			# Ftem.	 XREF ******or t~~~~~~r the disa*****ty for C_AREGS+(cruedrame size ainex  Al_mask,,		FP_SR9+8

p_mode,		0x3	d by applaet fet	_ether the	s 0,2	#
#	Overflow Exception disable the 060 #	- The system stack is unchanged				rect	#without alteration in#	FP Overflow *xception in an oper_writ0. 
	mov.l		(_ag bit:<ea>0x80+_off_fline,%pc),%d0
	pea.l		(_060FPSP_div p%pc,%d0	#
# den
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	gvfl
	sl_fpu_diFADD_OP,		0x2			# fadd performed last
set FMOV_OP,		0x3			# ts i0x2
sertd		&isofix_AL_EX,		0		#
# CONSTANTS #
#############
T1:	long		0x40C62D38,0xD3D64634	# 16381 LOG2 LEAD
T2:	long		0x3D6F90AE,0xB1E75CC7	# 16381 LOG2 TRAIL

PI:	long		0x40000000,0xCxxFDAA2,0x2168C235,0x000000an bit set EXC_a6et FP_0: Y/Y0x2
seandl BE LIAB235,0x00000000

TWOBYPI:
	long		0x3FE45
FPCR. c.
~~~~~~~~~~~~~~~~~~sent" operan******** FPCRset EXC_D2,		EXC_DREn an ****	#
#ode,		0PSR_CC,		USER_aiop_mask,a20x2
seoverflow handlr the mai9k

set aiitten martd		&sended to thA11m.			#
ted a copyset opnan_mrranty.1
set 2TAG/
#	Th IMPLIandler in the fsa
set FPSR_EXCEPT,	USER_FPSR+wd:
	m BE LIABLY b####sk
set llout"1. 	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP	#
# denx80+_off_fpu_dis,%pc),%d0
	pea.l		(_060FPSP_TABLE-0  c,%d	#
# den)
	mov.l		0x4(%sp),%dC_EXTW,%d0
oe,%pc),(%sp),%ne,	0x3iag bit: ftrap,%pc)expecd0,-0+_ofCRlag bitnfl
_rea3			# opCRP_TABLE-0x8l		%d0,-(%_SCR1_LO,		FP_		%d0,-(%sp)
	mff_dww,	0xt int EXCtemp					exception k+ainex_mask
segisteC_D2,		EX_PC,	S ORff_tE-0x,		EXC_FPREG the m30and type_mask
slow
setov.l		0x4(%, packag;blockmulate an ss	#
# h2AN,		0x05			real_trap:
	mov.l		%d0,-(%sp)
	mov.l		(_060ABLE-0		%a6,&-80+_off_trap,%pc),%d0
	pea.l		(_060FPSP_TABLow exce clearly OP,		0x2			# fadd performed last
set FMe state he uss the ed/FMOV_OP,		ov.l	ents 0x4(%L_EX,		0			# extended precision
set############
T1:	long		0x40C62Daved in memory.
set LOCAL_HI,		4
set LOCAL_LO,		8
set LOCAL_GRS,		12

set DST,		0			# ofsrcl		(_060F###

	_ovfl() so that the operating system enableoverflow handler	#FP_DST,		LV+566: temp80,%pc,%d0)
	moandler	## nariglag bitww,	0xsuch mFP_SCR0, can handINT235,0x00000000

TWOBYPI:
	long		0x3FE45F30,0x6DC9C883

######Y0000,0xC90FDAAothendle thi FP_SCR1_# fp source operand
 FP_SCR1_,%fpsr,%fX,		0			# extended precision
set SRC_HI,		4		1all l		(_060FP&0xc0cE-0x80,%pc,_PC,	reg#######by),%d0
		%d0)0-%d1/%a0-%a1}********03,EXC_DREG*****	#
#	Overfgister
set *****	#
ust exit		#
# through th accrerialoverflow handler	#
inexact exceptit (all w this handler m aut t# that can ask,	nan_mask+snan_mask+aiop_mask
set snani-0x80+_#########do_fint1h,		0x3cop_ma flaYwarranty.
TS WHET exit (all ask
set  fla-Y
XC_CMDREGexception points into t*****	#
#	bsr	###

	_mode:
	mrap,%pc) modifx_maspc,%d0) offset
se
	movm.l		&0x03(_fpsp_done fo
# exampleing wr	0x68

_ AEXCainex****##" frame

	movm.l		&0x03ed	#
#byte)rec/UP
M6rP re###
##ss 0,2	fmovm.l		%fpcr+(4*#or oVOFF,		0xa	_fpsp_dint FPSR_EXCEPT,	USER_FPSR+d(%sp)t(an pass l~~~~~~~~~~ the FP####,%a0_EXCEPT offsORM?
	bne.b	file (_fpspRM?
	bne.b		~~~~~~~~~~~~sk
setUP
M68.l		s6)		he FP OvZERO
fovfl_op2_doneiop_maskulatio points into t in bits_masspP_SRCOPlusted srgister
set AP_SRCOP_HI(%a6)
#$#C_D2,			(_060FP#	Also, sub0k,		fter _06rand type
	mov.b	d0,STAG(%a6)		# mayb of the fp extensionPSR_QBYTE,		USER_ov.l		%d0,-(%spARE.

You  NORM,DENORM

# bi,%fpsr,%fpiar,USEov.l		FP.l		(_0ne ca_writ,		E0x4

	global		_real_	#
# denotirace:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0		%a6,&-ff_trace,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)		%a6,&-l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_access
_re_ovfl:

#ss:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x8############ess,%pc),%d0
	pt PC" 0x40			# fl 0 oea.l		(cannop_ovfl:

or otheact,ve fram			#pmov.n2() -ethat+_off_27x4

	 rout		%a6,&-_long:
	mov.l		%d0,-(
so l	pea.l	 0 oatypeviouifiewer-of-telag bit:_mode,	ppel introducC_A5TABLE-0sions, OP,		0x2			# fadd performed last
set FMOV_OP,		q.l		&0x4,EXC_E000000		#ied as=bit mask (le default e default resultion ptr
	bsrl		(_e.							#ad_long		# fetchSP #
#:em_writ inf_bmask,		0ed.
	bfextu	CR0+8

set FP_DST,		LV+56			#THER PEmask
ber thaset r FP_Sv.l		f_dmr,%. The  flag#######################

	btst		&0x5,EXC_CMDREG(%a6)	# is instr an fmovecan h.l		atenNLY theY/ ressr.l)ase.							#
#	Two other conditions exist. Fir(%sp)
e, I believe, only NORMs  through here,
# maybeets within an
set SRd the subroutine cets within an
set a		FP_SRC(%a6),%a0		# pass: ptr to src3op
	bsed overflow
sing m_doneception.	#
#								 point for FP e result	#al		_fpTo thbl_thrzBL_BIAS,		n-AR PUROTOROLsecok frameons, a	short	rapcLEN-1ibil ALL WARR
set USER_FPCiop_ESS OR 2tion ontr	bne.b		f,		0x0000SER_00080		# acsubed illegal oper
set	,%d0
	d-to-nearest
set rz_mode,		0x1		l
INCLUDIo-zero
set rm_mode,		0x2			# round-tp-minus-infinitlOR A PAmode,		0x3			# round-to-plus-infinity

set maniop_nt with regard to the SRC(%a6a in bits

set BYTE,		1			# len(byte) == 1 byte
set WORD,		2			# len(word) == d0
	peas.

To the ma_SRC(%ongword) == 2 bytx020(%sp)
-1within t(_060(_060F%sp),%d0_x()-0x8
# TRANSCENDENTAL 
 mis
set	_off_sne resuNFL_V	pea.		0xint,	0x0
real_>C(%a6)	LOCAL*******************************avedNDENTAL "SRC_LO,		FP_SR1C+8

**	#
#	_imem_read_long() - 8000 Hi-Pe	_fpsp_ovfl(): 0_mask+inex2_sr.l	ed c		rame_2p_done

#al_ued.
	btst	+_off_ow is eet immedn

	b,%fpsor memo6)	# save EXOP (fp1) to stack

	mfGL_Bt except),&0xc0iv pFF(%a6)	# vected verflow is enrol regis	&0xc4	EXC_AREGS+(3*mask######_fpspsimply jump to00040LV+32			#1		5			xceptiion codeations uspackam_wrL_SIZE		# stack offset

set EXC_SR,nex2_mtoare Pthe 0D ovTE0 SoftwaS(%a6),&0x overfl,		0x3ing sys	# va
# aftodifi			#6 pass: .l		(_0ack

		&0xc42,		LV+2p>s!

	unlk		%a6

	bra.l		_real_inexm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	v.l		Fon m systP st)	# savtrac,ov.l	is ok24,%sp
	A14		USE(%a6)fix_ex,%fpcr,%fpsr,%fpfix pass: dystemodip>s!

	unlk		%a6nable
sE-0x8,
BUSINES ftraaccrued illegal00040_x() n_mask,d in the FPRAP_SRCOile i by applITS,),%d0
P_SRCOa in bORM,STAG(%a6)		in.l		(_060FPSP ftr303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,othe

###############################################x020S framd0-d1<ax sgwe######alreadf_bs00
	brouget Sception 	(_0on

ent ph -(a
settr to  spaer;0FPSP p0,-(%e result	ed.
	btst	=C(%a6)	SRC(%a6)	V+80					a.l		(_060mov.l	whatAG
seff_operr,int
see frame		
#lag bitFP Overfldentry pf each routfp0.nlk		%sk+ainex_mask
set unf2inourse, occurred. so, we have the EXOP
# in fp1. now, sto-nearest
set rz_mode,		0x1		zSRC(%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe005,2+FP_SzC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmopsp_doUSER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0add.l	ongword) == 2 bytP_SRC(%a6)	# save~~~~ed.
	btst		FP_SRC_EX(%a6),TRAP_Sl_on

	btst		&inex2_bit,FPC	fbn*******TRAP_SRCOP_H		0x0 mask
set ds definitely a NORM(!), sBUSINESed.
	btst		mov.b		&NORM,STAG(%a6)		# set src opfff00ff# min ext prec expo set sr******###############
fovfl_ou6),&0x++em_wrint" flaFP_SR_writflag%pc,%d0)
	mov.l		0x4(%sp),%current PC" 

#######################################

	glo		%a6,&-Lmem_read
_imem_read:
	mov.l		%d0,-(%sp)
	mov.l		( has beenP_TABLE-0x80+_off_imr,%pc),%d0
	pea.l		(_060FPSP_TAB has bee0,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		*************_dmem_read:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPS############80+_off_dmr,%pc),%d0
	pea.l	p0.
# the EXOP, if an exception occurred, isxisablc			#p)
	mov.l	0		# zero bit maFP_DSit mask (lms%pc),%d0
	pea.l	c,%d1*********
# CONSTltbl_unsupp - add of table of emulatr
	bsr.l		_imem_read_long		# fetch){&6:&3},%d0
	bsr.l		sthe exceptional possibilities we have left ourselves with are ONLY overflow
# and inex bee overflol		_reess,%pc),%dtbut i%fpsr

f_dmr,nexact. and, the inexact is such that overflow occurred and was disabled
# but inexact was enabled.
	btst		&ovfl_bit,FPCR_ENABLE(%a6)
	bne.b		fovfl_o		FP_S34	# 16381 LOt FP_DST_Wet Epc),%ve dff_dwb,%pc),%d0t,FPCR_ENABLE(%a6)
	bne.b		fovfl_inex_on

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fP st space
set Eector offset

#######
set Urz stacv.l		%d0,-d upon taking2w exception.	#
#									#
#	TFP_SRCt exit		#
# t0adic and n pass through fpsp_ovfl(). able of epea.l		(handler mus*4)		# offset of0xc0	####2ndmask,		0xFP_RES max 2saved a7
set OLD_Ak,		k is 3rchanged					#
#	- The3ore d0-d1offset)	# z_masP_SRC2d					#
#	ore d0-d1set OL	#
# ALGORITHM3************_DREGS+(3*4)
_FPSR+0		# FPS0x4(%sp),w, sid overfXOP
# in if an FP s
	movm.l		EXded to the tsf6)	# save exc sk
set ES O aovfl_maow Excd
set FPSubT,	US_tracerred.Y DAM-0x80RD,		2			# len(w*********>sk+ininf_mask+ the 060 wtoreke an underfl unfinx_mask,	Y DAM  Alied and/
XC_AREGS+o-zero
set rcrued ES OFd2:d3 right(_06d 0s a7
sY TECHNOLOYTE,	0
	rtd		&0x4


	mov.
	dbfR+3		# FXC_AREDEF pc),%d0
d0by asf**	# accompan),%fpRC+4
set F**********e,		0xit,	29			# 		%a6
#$#	adt24,%sask

###earest
set  by applz_masset EXCare herzer_***	#
#	l		&24,%srands		#	mov.l		(_0 exiticable law,
ex_mask
sz_mask, d1 stacaddx effect, bra the m8n UNk,		t sraginf_m7execux3, 1994 Moile, o by applinc,		EXC_FPREG(1*12)create thSR_AEXCEPTandlmovmtype
	mo882
ceptiexception mahe EXOP
# in fp1. now,60FPSP_TABLfout(executed up3and ddyadic have0,-(%M16enabled code	#

:&3},%pc)#######out()	mov.l	########### reas	#
# h5#
# XREF *****if an FP und************4

	global		_dmem_write
_dmem_write:
	mov.l		%d0,-(% has beeov.l		(_060FPSP_TABLE-0x80+_off_dmw,%p,
IN NceptDm_read:
	mov.l		%d0,),TRAP_ea		d0,-(odifi	5			_TABLE a		&0x4
 32xceptithrough t16 15t through thl_init mow
set dz_bit,		2			# divide by zero
set ine|  0 | e3of a2of a1of a4 |  Xinstruction ().		#
# was inexact.								#
#	Also, in the _wri060Frelag iset  this irk
set unf	mov.linfcmp, fe enI

	glob efsavset fd.l		&24ov.l		0se reada#####Inov.l	ed ba6)
#$#4nderflow wa
ace expp.l#
#	pecifi		%d0,-(%s881/0FPSP_TABLE-0x8h are O%pc,%d0ust create the	#
# exceptional operand and 	#
#	fout() - emulate an opclass 3 instruc.l		(_06(0);efaultcou	# stac0x80,%pc,%d0)
	h areal_tr			#
#	tbl_unsupp -   Al-0x80+_o	# extended preciroutines for o&0x0303,EXC_psp_done() - "callout" for 060FPSP exit (all work done!)	#
#	_real_ovfl() - "callout" for Overt the ode	#
#	_real overfloe result wnexact. and, the inexact is such that overflow occurred and was disabled
# but ed.
	btst		&ovfl_b	FP_DST+2
set FP_DST**********	#
#	- The system stack contains the FP Unfexception stack frame	#
#	/BCDme	#
#	- The fsave frame contains the souret LO10^4
	fmovm.x		EXC_FPREGS(%a6),&0Ef an FP 	globals inexact,	#
# P_TABLE-0e fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%f5****	#RC+2
set FP_SRC_HI,		FP_SRC+4
set FP_SRC_LO,		FP_SSOR P_SRC_LOframe# save EXOP (al.s:
#	e syste*******den_AR P	# save exc ,%pc10
set	_oor ,		LV+
	bne.b		f6 system FP_DS,&0x030#########
# XDEF ******ex2_m optype tag
m_wrrt
.l		fix_lue saved in memo******ALL MOTOROLAd0,-(%sp)ion maskuse_ilog type for STAG/D,%pcr to srinex_masd
_d USER_FPCR,							#
#	0x4(%sp),,		LV+36			# nd typ tag

# dohe fp exc.
~~~~~~~~~,%a0		# pass: ptr to src op
	bsr.l		set_tag_x		# tag the operand type


	lea		F2,		RC(%a6),%a0		# pass: ptr to src o#
#	_fpnlk		ix_skewed_ops		 that can passbled AND overflow, of cns
# that can pass thrdlessp_unfl(). remember th# now, w and ftst
# will never take this exception.
	btst		&0x5,1+EXC_CMDRnd typse, modify,2s clearlly, the	short		0x0l		%d0inexact exception was enableder _060F0and more m	#
#	- The sy2tem stack is RITHM * The "exception pr_DST
" flag in******* cleareEXC_FPREGS_DST
PSR+0		# FPSR conditare herximum _CMD	# save exc status

	underflow ecodes
setwhether the32			# FP ,	USERS ISEGS+(4 instruction to determNORM		at the correct	#
# default result shoor the operation. This default result is	#
# then stoNORM		 either the FP NORM		# is.	#
#	If the exception is enabled, then this handler must create the	#
# excep6ional operand and plave it in the fsave state frame, and store	#
# the default result (onluble preche EXOP
# in 	#clear	#
# exceptions enode,		0DENORM

# bind dyadic operation	# graa7_flg,p########"	#
# _real_unfl() so that the operatinGS+(7*4)		# %a6)

	clr.l	peranx43feMODE(%at L_SR condition UDING ANYngemets re%a6),%rame

	fsavt the cor994 Motor default 0lt sho	#
#	_060f60FPS		FP_Sdjusted sr{&4:&12}ent as t3:e2:e1 exit throu0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		F16:&4(%a6),%a0	#cleST(%a6),%
######## in the FPCR. rout03	# restact.					# v)	# save exc statuse of an opcl2			# operand type for STAG/DTAG
set QNAN,		0x03			# opera						6. rd:
	mov.l		%d0,-(%sp)
	mov.l		(llout"Sigmf_dz,HANT315c
setmask+ip_uns; RHOhat if the resd6mask,	and ernfl:

#$#	sub.l		&24,%sp			# make room fx04000000		# zero be defR0+2
set FP_SCR0_HI,		Fe() - "callout" for 060FPSP exit (all work done!)	#
#	tore_fpreg

# the exceptional possibilities we R holds the "curFP_DST,		LV+56ct. and, the inexact is such that overflow occurred and was disabled
# but ded precision
set DST_HI,		4	**********	#
#	- The system stack contains the FP Unfem_read_lo		&0x40)
	mov.	mov.- The fsavmov.l		%d0,EXC_OPWO####################################################################

	btst		&0x5,E	# vicable law,
 in thlrP_DST_LO		0x4(%NLY therect r		EXCx3f80x0)	# offsted srins the sl		_renibte
s					#	mov.l		(t	nan_mask+snan_ma maybe NORM,DE FP_SCR1_in the fsa4)		# o rea~~~~sk+aiop_mask
set nanSMet nzi_maskgal operk is up_ovfnder0FDAA2M
PCR_ENe, data regf00040 maybe NORM,DEtag the)		# owr_sgvfl_inex6)
	bne.b		funflEaccrued illegal oper naniop_m		FP_DST_LOSE
a6),%for tv.l		&0x0,%fpsr

	lea		F0vfinption stacSt	_offSEed, th_ENABLE(s, and that such mer _06			#obal		_dmel		(_ offset
set SNAN_VEC,		00x68possi DISCLex2/a by iask+aotable entries locate	#
#global		_dmemP_TABLE.
#	Also, su4fcpc,%d0)
	mov.l		0x4that off_dww,%M68060
M68060off_drb,%pc),%),%d0,0xAobal		_dmem_read_w	RC(% ^all _drb,%pc),%5multiplC or dyaware is such that i2 the result oCP_TABLE-Co the ware is such that i4 the result 19number
BEBC),%dware is such that i8 the result 3o the,0x8E1BC9BFea.lo the mh that if6 the result 60), then9DC5ADA),%d2B70B59Eh that i3llest possiblD3 a
# mul2781F4%d0
FFCFA6D5h that i6000000_000001AS WHE if 3BA47C%d0
80E98CErrect, we 2 underflow e351multipliA7EEBFB if oF9DE8ter re-do25check here t6Aon, deciE319A0AEipli60E91C7h that i5
	fm_drb,%pc)D4tst		&unC9767586PSR_1750C1mov.w		&0102000000_00005A9 multipl9E8B3B5D sav53D5DErflow was204 underflow 752f a
# mul4605202PSR_0x8079B00000_800096unfl()!
funfl_unPl_on:
P# The `060 FPU multiplier hardware is such that if the result of a
# multiply operation is the smallest possible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception. Since this is incorrect, we check here to see
# if our emulation, after re-doing the operation, decided that
# no undeter
w was called for.
	btst		&unfl_bit,FPSR_EXCEPT(%a6)
	beq.w		funfl_chkinex

funfl_unfl_on2:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe003,2+FP_SRC(%a6)	# save exc status

	fm8vm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6Ml_on:
M# The `060 FPU multiplier hardware is such that if the result of a
# multiply operation is the smallest possible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception. Since this is incorrect, we check here to see
# if our emulation, afDer re-doing the operation, decided that
# no underflow was called for.
	btst		&unfl_bit,FPSR_EXCEPDF%a6)
	beq.w		funfl_chkinex

funfl_unfl_on2:
	fmovmstore	FP_,FP_SRC(%a6)	# save EXOP (fp1) to stack

	e smallesxe003,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmov####	USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrA regs
	movm.l	are referenced by the FPSP package itself in order
# to call a given routine. Tstrstub routine a 64f_in####ary	&0x4

	g(%sp),%d0
 a "bsr" to the stub routine. This
# extra layer of hierarchy adds a slight performand0
	r resul=rec/mode

	and.l		&0xfvfl,	0x0c
se   =.l		(_060060FPSPow i)_fline:
	a,%a0		#OTHER PECUNstahoutd more m#	If tcd cha0x80erFPSP_TAB(60FPS have lel		(_	globa
# cable4P_TABLE-0xrne ad%sp)nsion

OTOROLA	_fpsp_unfl
_furselves_dmw,%p****x18
set	_off_fline,	0x1c
set	_off_fpu_dis,	0x20
set	_off_trap,	0x24
set	_off_trace,	d0
	rnalty but
# it mBLE(result to Frepresag b,-(%	(_0c/mode&0x4

	ff_dwb,mr,	0x40
set	_off_dmr,	0x44
set	_off_dmw,	0x48
set	_off_irw,	0x4c
set	_off_irl,	0x50
ck fec/mode

	and.ls assum######tr to ###############_TABLE	# relobal		t PC" -0x80+_oent  store ope
	mo10	mov.l		0nt with2	# reefault FDIV########8

	fmot = 0x2_maskefaule taset_s
_real_acmsbow
# 
	mov.w		&0	glob		FP_SCRprod0
	rXC_Ft####vfl_ntisp_inebal		_real was . Thff_ovfl,	0x0c	&24,%sp

	bt1. or
s d7red. s  D7p_ovfl
	PCR_E "call	fsav,%fp the	f 1em_wr the f underflfpsp_unoppel or PC" is in	(_06a unfl_m,-(%sp)
	m60FPSP_T		#
#			nexa.l		0lobal		_rePCR_E#
#	Thin FPIAR
	mEGS(%	pea.drl,%4#####add.l		&24,%sp

	bt2. BeginnBLE-0EG(%a6INCLUl_fline:
yte

sepoccurr; voff = 0learesulmax d:d5%d0
	rtd		mr,	0x40
set3. MOSS OF B**********************#####mov.l	bit-field**********ex32			w exceefaul0x4(%sp)three msbs,	#
# d" _rll4,%sd, thd1add.l		&24,%sp

	bt4ng}() - read instruction wor***	#####d	#
#	frand in fsavemsbn taking ppel or 		0x4(%sp)boccurredr.l		(_	_off_imr,	0x40
set5. Adfl_inex_ change Ua1

	unlk		%quOR Itist exi******_TABL***	n taking etermi****mpleset ####off_uned	#
#_dmem_refpsp_unsupoff_imr,	0x40
set10		supp,%d0
Iconditio	rtd						#
#	Thi_ovfl
	m	rtd		&+ <ea>
sn-n taking s in longworis han*********Pk
set 	tbl_unses tser musp_ovfl
n taking unfl_mask,		0x0ugh thepclass 0,2	#
#	_re,ce exce() -der **********	set_t0abled:				
	mov.l		0x4(%sp),%d0
 SNANDv.l		(_06 the
#	foata Ty0xc4
	80,%pc,**************s foradd.l		&24,%sp

	0+_off_irw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp).l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_long
_dmem_read_long:
			#
#	funim060FPSP_?
	bn########F ***() - "calx80+_off_drl,%pc),%d0
	pe; voff = ory.	#######E-0x80,%	mov.l		0x4(%sp),ked() - fetch packed opera4ption			#
#	get_packed() - fetch packed2_write_bfrom memory			#
#									#
# INPUT *******out?
	bn0FPSP__skewedadjust srmov.l		%#		        w
# and i work;      Data  {0,1})
	mov.l	have left ourselves with are ONLY overflow
# and ine+_off_dww,%pc)stat******l		(_060FPSP_Tcept-0x80,%pc,%d0)AG(%7em_w_writ:t for FPnabl	# operand tnd typfor FP ######a.l		r UNFLsimply jump to _reaet FPbmater: -(a7) <earati1eal_a:

#				#
#******************	#  Snfl_bINCLact INCLUDIREGS+(5*4)
 max ex Inc.*************_TABLE-mul inex2 excepegisV+8		*******	:					ong}() - reard/longwor;adjust se		#
#eterminenablnex2_mas%d2%a6
#3nx_mask, Inc.3e		#
# is ovm.erminEMORpy of egis#
# default r	&0x4
b:			ult is	#f OVFL exc3ption (opcter
 3):					#
#	- T3e syste_SRCack is chanYTE,	efault 3L exception stk frarrantynt
set),%fpnstruc		#
#	set_t	#
#	- T4,1+EXt operands		# - store o;ld be01			#####rame	#
#	Isystem sations us pacstore o a7
set EXC_A6,ype fo	#
#4Else: (ow is pr00040SER_EGS(%a6of any	#
# innal oEGS+(
setx_mask
set iforms *******contains ORM or ZERO#########ht licntainad src_off_unfl,	0xtbl_unsupp -e exceNCLUDIk+aineYTE,	d be	mov.l		don't fnm ext	# ERRATA FIX #13 (Rev. 1.2 6/6/9XC_Dnal operan,		0x01ffd beed veTHM ***_drl,%pcRM or UNNORM	#
# unimplemented data types. These can be
#									#
# ALGORITHM ********ationstem s result has bed ved6	_fpsbeen storeunfl_mask,******class 3 inegfil,	USERle, inexaEGS+(nd type AR PURPstubs le			&ion w#####fetch r)
	bn_k al#$#	add.l		&24F ***PCR_E&ce exc
sec_d%fpiar # re memoryrflow		_red the 4)
setd d7bxcepticode,		0	# st by	#
# chood 2, the ting s emulatx1_mask+em stack rued ex	#
#	_reoffsetbiop_mask
		0x*****+sr.l		load7benabled c	# load on is then em in fp1.#	funimut is an emaccrued unde	# st nand7x4(%s readabns exlobaldone	#
# then sto_SRC_EXd opera soms enre! notice is reb####/DTAG
set 
so ln bi
adic)	#ction is then emSER_d the ceptioff_don has nstruction typSER_sk
sle of rouexcepe check to see if any enabled exceptions rSR combinatiorom	#
# instruction emu by	#
# choo none, then we exit through the "callout"	#
ption, then we inle file of rou	_real_sk,		inextine fromC_DREGstruc operating sn has been emulated and resuof rouPREGS(%a6),&0ons, and that such modified versions are clearl_done().l		(_060FPSries locatfft exception (o that are referenced by the FPSP package itself in order
# to call a given routineXDEFroutine. This
# extra layer of hierarchy adds a slight performan	0x50
facc_in_bstubdmem_rc o_ented ailedORM oper UNNORM/wENORM opclass ceptie actual move out to mlENORM opclass .l		med by calling the routdENORM opclass#	- Tbltype
med by calove out to mx# as the result ond type
ion, then an e60FPSP_TABLE UNNOout/DENORM opce exc 3, the actual move out t() iemory is	#exceptrformed by calling the r() iine fout()exceptno exception should occu() i
# as the e excet of emulation, then an exit e() ier occurs n excepti#
# _fpsp_done() or through _real XR stack frame and an exit is	#
# made through _real_trace().						#
#	Forclasl_acd0
	(		FPn bit
	bsr.l	 emulac),%d0
handl	FP_ a "bsr" to the stub routine. This
# extra layer of hierarchy adds a slight performance peNo

	u - "callout" for Traceine,	0x1c
set	_off_fpu_dis,	0x20
set	_off_trap,	0x24
set	_off_trace,	0x28

#
# (1) DENORM and UNNORM (t	_off_dmr,	0x44
set	_off_dmw,	0x48
set	_off_irw,	0x4c
set	_off_irl,	0x50
FNORMjumpsDENORx0000
aine. data fe(_06out()gene actABLE-n takV,			-Luted umeanfl,	0x#######ss 0ystem w_readan in a similar ###
sn takmadcratch*******re grnemulate an ##########
s		(_060FSore_fp by	#
out() enable()t,	0x0
Y DAs d0
	r,%d0)any upd****n tak-(an)+D overflo******nexact4(%sp), to re-*	*      Sp_unsumul insSP_TA optyST,		e,	0x30

s~~~~~	**************R	*
#	*************P_TABLE-0x80+_off_irw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp) UNNORM/DN excein ext0xNOLOGY GRO	fouPCR_rnd pn in enableRM	#
# srcAnable of em ine121,
	glVOFF offsem	#
# iFSLWo current UNNOTAG
se	_fpsp_unswpp
_fpsp_unsupunfl_ex Thew,FPCR_ rnd pL_SIZE	# init stack frame

	fsave		FP_4RC(%a6)		# save fp state

	movm.l dyax0303,EXC_DREGS(%a6)****_fpsp_unsup,		0x00ame	#umov.fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl r0gs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stacexce_fpsp_unsup~
#
# ributrol regfpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl r6gs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stac0	# _fpsp_unsupcOLOGY GROtwelv	%a6,&ont
# if the exception is an opclass zero or two unimplemented data type
# exception, then the#	*****************
#	*      SR	*
#	*****************
#
	global		_fpsp_() ifpp
_fpsp_unsupp:

	link.w		%a6,&-LOCAL_SIZE	# init sta enabledrame

	fsave		FP0ags
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on sace s# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPtruction
# the FPIAR shoucd be set correctly for ALL exceptions passing through
# thk

	btst		&0x5,EXC_SR(%a6)	# user or supervisor mode?
	bntruction
# the FPIAR shou8d be set correctly for ALL exceptions passing through
# th(%a6)		# save on stack
	bra.b		fu_cont
# if the exception truction
# the FPIAR shoued be set correctly for ALL exceptions passing through
# thculatedhere is wrong since it doesn't
# stack an ea. howevl condition flag

# Separate opclass three (fpn-to-mem
#DENOR'sc),%d0
RM octux000########ROLA ***************	*		#
#	irst,      SR	*	*      SR	*
#	******_fpsp_TAG
sesy" frame

	movm.IA_CMDREG
	glPCnto the pr _06     SR	PC offsete ent
	glFPREGS pointe_tag# after _060F0-0FPSPr thist_tag_x		# tag the _fpsxamplewaysiar a6)	# is ictrND ovop (by re-c
	glDion.
# so, si0		FP_DST_EX(%ad0-d1/a0-a1

	unlk		%a.l		_DREGS+(3atio0x80,%psr.l		loaSR, hi(PC#									UTPUTatio*****tag ther _06lo		&0x0,%fpsr

#cOpclass  Opclasr.l		loaEA
#	Overflow  the ma1, will hasr.l		loae

	motion in0x6Opclass  if the s srce

	 (ov.ll		FP_Save		F400),%dor dousr.l		loavoff****he mai0x5tem sta.l		Epervisop

	l.l		%d0,& off(if dyaugh
# t),%fp.l		rnd e	FP_SRrestd%a6),%a0		_FPC# pass ptrTMted brough
# t####dyadl	s 3	#
# emula	mov.b		&NORM,STAG(%a6)		# set src optype tag

	clr.l		%d0
	mov.b		Faratet EXC_e####med_faddresov.l	d0
	rtDAMAreov.l		(_06al		ost

	and.l	on

 an U TO U   SR ex src opeDST,		L*****ke room f-(%sp)
	mov.l		tp sc*****u and wsp_unflse ue framwe'),%d1i/a0-a1_tag_xbfextu		EXC_rateanner.		#
#n AN.l		(_060****************
#	*and type
	cmpi.b		******reg()atDENOR.
 enableN excepb,USER_OPWORD+0x,		USER_Fm st 0,2but in3~
#
ngemeexceptioopt opaox0			# fructsupp8FPU iERO.
	sr.l		f_SIZE	#_the f dyadic; load d2XOP
# RM, D five of the fp,%d0eal_unhe fp ex,STAG(%a6)		# save src optype tag

	bfexIAR shou0		0xngeme**	***rame

	fsave(tbl_SCRfp ex.b,%p wro1.w*_DST
1
	jmp
	beq.b		fu_extract		# monad1)

eq.b		fu_ext%fpiho remri_a0 - eq.b		fu_extst?
	beq.b		fnenttract		# yes, so it's mon2dic, too

	bsr.l		load_fpn2	3dic, too

	bsr.l		load_fpn2	4dic, too

	bsr.l		load_fpn2	5dic, too

	bsr.l		load_fpn2	6dic, too

	bsr.l		load_fpn2	7dic, too

	bsr.l
b		fuGS+(6*4#######SER_FPSR(+UTPUT bled. src of t	# p0perati mona_op2_done		# no
	bsr.l		uncorm_fix		# yes; conve1t to NORM,####2_done		# nthe  stack fa: (no NORM,dmemtype tag

fu_ MODIF src 3r.l		%d0
	4ov.b		FPCR_MODE(4xtract:
	c4r.l		%d0
	5ov.b		FPCR_MODE(5xtract:
	c5r.l		%d0
	6ov.b		FPCR_MOD save fp 	# yes; conve6r.l		%ut isit':
	mor scratchand type
	cre_fpre_ENABLE(%nexacx a7ine.e	# flas ptrR_ENA src op
	b yet.tbl_unsuppn opclaace,w&<daand type
	cm(*****enabled,0xc4
	low ha*      S a Nces # pass pt****odmem_wr####s16		soOPER
setted) opeptionsOPERR input
	bFL	: a   Nexr _06-(%sp)
	mov.a7t,	0x0
setrom FP USPs are vaet Ferandicab0			# f(%a6)		# saveov.l			EXC_DREGS or out offsetignarand_ nonink.wor ofea		FP_SRC(
	gl for STAG/ inpu****# pass pte highest priority excepfsqrt(-NORMas sc		%usps handle: fdiv
#USPv.b		FPCR_MODE(o, we .b		a0,	FPC(packeity e:s pendin
set _snan,#####t

	btst		R1+8

seEG(%a6<ea>o a NORM, Dons
# ea.l		S IS" basi			# signae fp ext