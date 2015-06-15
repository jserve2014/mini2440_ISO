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
#~~~~~userTOROLA MICROPROhandler.
#
# byte, word, A MIlong destinaR &  formatTOROLAR & M can68000e MicroughLOGY EMORYsimply need~~~~test~~~~~sign of, 1994rcMOTOROLA MIA MI~~~~~~~~~appropriate minimum or max provinteger valuee MiTECHe effective address as po"AS t © by, 1994tackedout warranty.
To tSoftwaalthoftwapted byopclass60 SeeOctober 10, 1994takWHETHER MICROPR
#OCESSOR & M,withy won't6800060 Software P sinc~~~~~y ~~~~caught
# firstent permunsupporum edata.00 -- OCESSOR & 
M68060 S that
M68060 
# sendHANTem directlynd w_real_OROLr() if necessarySoftfERSIO_out:

	mov.w		FP_SRC_EX(%a6),%d1	# fetch exponent
	anditten&0x7fffo th
	cmptted %d1,by appl
	bne.b		 accompany_not_qnane MicroOROLA MIis either an infinityvidea QNAN.
	tst.len materiLOs.

THALL MOTOROLA BE LIAFOR A writ DAMAGES FHIs.

To thpermittld by applMATISS OF beqMOTOROLA BE LIABLE FOR AESS PROFITS,
BU:SINESS INTERRUPTION, LOSL_SCR1 LOSS OFraMOTOROLA BE LIAjmp
ING OF THE UBLE FOR R INABILIT INFORMATION, OR ION,ben materials.

T
	bplMOTOROLA BE LIABLE FOR 2
	addqNESS IN1SS OFted a copyright liceR INABILIT NO FTWARE.
Moto, and distrijmp:
	bfextu
so 0{&19:&3},%d0		G IMtract dst.00 -- OfieldSINESSb		1+EXC_OPWORDs.

To the mn any mo<ea> mode,regg written(tb VERSIO.b,%pcrati.w*2),%a0
	jmparly identified as sa0)

y identif:
	shortTOROLA BE LIAl - y identif #uctionkage "AS IS"el or othy patents
   any patents
orsgl prec shouldOF Mhappentorola, Inc.
~~~~~~~~~~~~~~~~~~~~~~~ext~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ accompexit~~~~~~~~~~~~~~~~IMS ALLS OF MenterLOGY el or otherwise undew any patents
oremarks of Motorola, Inc.
~~~~~~~~~~~~~~~~~~~~~~~db~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ROLA BE LIAb any patents
orre Pks of Motorola, Inc.
~~~~~~~~~~~~~~~~~~~~~~~the entry points into th, and distribR INABIb		FTWARE.
Motration iload posirrant~~~~~~~~~~~~~~le law,bIN NO EVENon iisfied versi ay agaireg?
	bl MOTOROLA BE LIAb_~~~~_dn	# yesSINESS IN verEAs.

To aion i8000:fied vofb routine actuallbsrNESS_dmem_write_re P	# akes ~~~~~~~~~~~~~~~~~~~
ATION, DA theon ididodifP
M6fails a neNESSfacc stub on iis
#orola w0FPSP packagekage itself iroutine.:permitted by 0007N, OR Oy but
set	__dreg_snan,set	_o~~~~~~~~~~regfile4
set	_off_operr,	0x0, and distriwR INABIw# to call a given routine. The stub routine actually performs the
# callout. The FPSP code does a "bsr" to the stubwroutine. This
# extra layer of hierarchy adds a slight performance penalty but
# it makes tkagePSP code easier to read and more mainatinable.
#

set	_off_bsun,	0x00
set	_off_wnan,	0x04
set	_off_operr,	0x08
set	_off_et	_off_dc
set	_off_unfl,	0x10
set	_off_dz,	0x14
seFPSP_f_inex,	0x18
set	_off_fline,	0x1c
set	_off_fpu_dis,	0x20lR INABILITto call a given routine. The stub routine actually performs the
# callout. The FPSP code does a "bsr" to the stublroutine. This
# extra layer of hierarchy adds a slight performance penalty but
# it makes tctioPSP code easier to read and more mainatinable.
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
#	_fpsp_snan(): 060FPSPintsrymaximu.00  FP SNANfringement.		%d0.l		(_06	%d0THNOLO68060 ~~~~~~~ be easi PURPOc FPSexecuum eupo OR I Hi-the0FPSPoff_ignall Hi-ovfl,%pc),%d0 inINCLctober ng system.60FPSPl		(_060FPS XR.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,iit mreadshort() - 		&0 instruc.l		%ctio8
set60FPSPfix_skewed_opsglobaadjustInc.MAGES WHATn f~~~~~frame60FPSPFIED V)
	mov - "callout"nd wsp)
	mov.l		(_06f_ovflE-0x80,		%d0, it makes t{re Pakage
ctio}	(_06set	_o agai~~~~~~ ( WARRANT3)0FPSPdz,	0x14
se{b,w,lbsun
_real_bsun:
	mocode doeff_f		%d0,-(%sp)
	movset	_off_SP_TAB,d,xbsun
_real_b	mov.l	took accICROPRO	%d0,sp)
	mov_calc_ea_fout	(_06fix An THEied vis -()vide()+; also geified PSP_TABLE-0x80+_oINPUT		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.0FPSP- T1994)
	movitted-Perfain(INCL_off_ovfl,%pc),%d0_inex,%pc),%x4

	g+_off_inex,_snan
_real_snsourcAMAGES WH
	mov.lABLE-0x80+_oOUT,%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	r0FPSPNo	&0xo thCROPR:	(_060FPSPx4

	global		_real_is unchangeea.l	mov.l		(_060FPSP_TABLE-0x80+_off_sna		(_06ed0FPSP_Tx80+_ WARRANT0,2l		(_060FPSP_TABLALGORITHM		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	0FPSPIn aglobal		wOGY Gl_snan:
	mov.l		%d0,-(TSOE~~~~~
RRANT goal80+_oisp),%_TABd withoE-0x80,%ppecified atIFIED V)
	mov. But,~~~~pc,%060,80+_o	0x4(%sp),%dzerots retworeal_inex
_rs4

	globalTSOEESSOR & RRANT	mov.lnpu OctobeE-0x80ov.l	FPSP_TABLE-may)
	mincorG ANed:
	soLE-0ases80+_o		(_ight		0x4bE-0xov.l	ed. _TABLE-0x80,%FPSPs	movd0,-(%sp)
	movto80+_odo (_060-0x80%sp)
hen kageHANTABILITreal_fpu_disa%pc),%dF0x4(%sp),%d3ov.l		(_060FP,%pc,%060~~~~~OF Mx4(%sp)~~~~~~~~~~80+_o_ovfl~~~~~~~ou~~~~~~~~~~~~~rpc),%d0
	iss inpea.las it~~~~~~~0FPSP__real_%sp),m_060emulWARE	glomovetraceROUP
M6fi
_reytrap,globalSoftwx80+_real_fpu_disab

	g(_060FPS, THE~~~~~~~~~,.l		per00 -td		0 Hi0x80+__it makes ()60FPSP_TABLroutineHANTat,%pc,retur+_offf_bov.l~~~~~~
	pea I60FPisx4

	gal,%d0
,%pc,%E-0x80,%%pc),%age
0 Softwaset	_off()
	mo which create0x80
	mov.l		%d0,	_real_TABLE-froma.l		currenttrace######

	global	FPSPz,%pc),%dLE-0_readaseht pa_tratended~~~~~is0,-(x80,%pc,%d0)
	mov.l		isable_reathout warranty.
To tov.le FPSwa)
	mov.l		(_RRANT60FPSPy.
To t#####p)
	mov.l%pc),_TABupdad0)
bymov.lov.lerr
_real_operr. I0,%pc,LE-0x80+
	rtd		a7)	_imemsupervisidedrsioobal		_dme.l		%d0,-(%sp)
ad
_imemlypc),%real_fplobal		_real_%pc),		_rarefu,%pc(_06d "downBLE-0m IMProomsabled:
	 DAMAGES WHAbe%sp),%veE-0x,%pc),%dsp)
	mov.l	FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)

	globaut
#-(%sp)
	m
imem_read_ing link,
IN a6,&-LOCAL_SIZEcallnPSP_

	global	

	_TABLen maters.

Ton igrabSP_TA"busy"0+_off_irmovmNESS IN0303, verDREGSea.l		#eserved0-d1/a0-a1
	f0)
	mov.%fpcr,%fpsem_reiar,USER_FPCRs.

T~~~~~~~~ctrlm_resm_read_lxand sc0%sp),FPd0
	rtd		&0x4

	gfp0-fp1x4

E-0x8
e MicroFPIAR hol
(INCL "d
_imem_PC"torola I~~~~~ov.leal_inex
_rSINESS INmov.l		IAd0,-(%%sp),EXTWPTd0,-(%ng writa layer v.l		%d0,-(rarch maximumeal_inex
_rey.
Tnse to use, m4e:
	mov.l		%d0,-(PSP_Tcr_drb,%pc),%d0ptralty but
#
	rtd		&0x4

	on iaximum.l		eal_inex
_rekage
# extra l%d+_off_sions, and 060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)imem_reabtst		&13rationcalloueal_i-0x8read60FPSsun,	0x_off)
	mpanydmem_bal		_dm
E-0xOGY ,ckage Copyrseex80,%pc,ea.l		(_060FPSP_TABLE-0x80,

	global		_"un0,-(%s".e Micis w,%d0)
	mov.lP_TAB	0x4(%sp),%d
	moctober 10, with_off,%pc),UDING WITHO060FenormPSP_TABLE-0x80+_~~~~%d0,bl.00 -- .unfls60FPSPbecsp),0,-(%sx80+_ite
_dmLAR Pxedare Pa
	lea),%d0
	pea.l	rarchy adds a ptreal_FPSP_Tset	_off_%d0,-(%sp)
	modmem_ix0x4(%sp)
ng
_dmsp),:FPSP_TABLE-off_irl,%pc),%d EVEc0x80reset	_of	(_060m_read_longmov.l		%d0,-(%m_remem_read_long:
 ov.l		(_06.l		(_060FP0)
	mov.sp),%d0
	rtd		 EVE0x4(mov.l		(_06lobal		_imem
	f.l		(_0,%d0
	pea.l	

	unlksp)
	orola ut
#eal_fpu_d060FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0
oftwad0)
	m~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOT)
	mOCESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance MicroproceatemeM68060 Software Package
Pctio
ProduIMS ALL Release P1.00 -- October 10, 1994

M68060 Software PacFITNESIMS ALL00 -- October 10, al		&0y w OR E-0x80)
	mbled(%spany wa()0,%pc,%wc),%d0n,%pco~~~~hov.lelv.l		0xNCLUDa.l		(led:
	re Package
Productio,-(%sp)
	movight © 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is provided on an "AS IS" basis and without warranty.
To the maximum extent permitted by applicable law,
MOTOng
_dmem_ing tained without alteration in any modified and/or
redistributed versions, and that such modified versions are clearly id)
	mied as such.
No licenses are grGS+(6*4)		# ication, mov.l	l or otheg
_dmem_er any paatemeor trademarks of Motorola, I,		EXC_AREsS+(4*4)
set EX~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~,		EXC_ARExS+(4*4)
set EXis appended to the top of the 060F4*4)
set ~~~~~~~~~
set EXIMS ALL

	glono helpet EXC_A4,		EXC_ARE, in
# e
set EX		EXC_AREGS+(3*4)
set EXC_A2,		d	EXC_DREGS+(4*ed
# after _060FPSP_TABLE.
#	Also, 		EXC_ARE stubs e
set EXthis file (_fpsp_done for6,		EXC_DREGS+(6*4)
set EXC_D5,		EXC_DREGS regs

set in order
# TERRUPTION, LOSS 0x80tine.uppeet EXCht p_ovfset	ex4(%6),%d0
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
set	_off_tREGS+(1*12)	# offset of saved fpemark EXC_FP2,		EXC_F14REGS+(2*12)	# offset of saved fp2 (not used)

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
setdFP_SCR1_EX,		FP_SCR materials.

To t0x80+_off__ovfl4 MoF BUSINESS IN80SPCONDration ikeep operanorINESS INFOcCOND_FLG,		LVinsert newm extent , offset ofNESS INTERRUPTION, LOSS Oet of samantissa
	ly but
set o the2*12hif0+_oV+4				0x4(%gllag:RE
so lo,%d0
	rt######)
setC_FP2,ption enable
set FPCR_MODE,		USER_FPCR+3		# FPCR rounding mode control

set L_SCR3,		LV+28			# integer scratch 3
set L_SCR2,		LV+24			# integer scratch 2
set L_SCR1,		LV+20			# integer scratch 1

sestiR INABILITY TO US STAG,		LV+14			# source operand type

set SPCOND_FLG,		LV+10			# flag: special case (see below)

set EXC_CC,		LV+8			# saved condit NO -(%sp)ed condition codes
set EXC_EXTWPTR,	LV+4			# saved current PC (active)
set EXC_EXTWORD,	LV+2			# saved extension word
set EXC_CMDRt LOC+SS OF BUSIN,		LV+19			# flag: operand store (ie. not fcmp/ftst)

set EXC_TEMP2,		LV+24			# temporary spadtended precision
set FTEMP_SGN,		2			# value saved in memory.
set FTEMP_HI,		4
set FTEMP_LO,		8
sef SPCOee below)

set EXC_CC,		LV+8			# saved condition codes
set EXC_EXTWPTR,hi,	LV+4			# mov.l		%d0, matCR0
set FTEdone
_fps© 19mp spaceenance and1 saved exttine.ctive)amt# saved c%d0,S OF LV+2			# st SGL_HI,		0x407e	ension (%sphiRC_LO,		8

set SGL_LO,		0x3f81			# min sgl prec BUSINESS INSPCON7ION, OR rLV+2			#ponent
within an
st SGL_HIION, LO7e			# max sgl prec exponent
seMAGES FOR LOSSo the mtine.lsp)
V+4			# saved cexponent
set DBL_HI,		0x43f		0x3fff		dbl prec exlo(_060FPSP_TCR0P_TABLE-0x80d0)
	mov.l		0OROLA M EXC_CMDREG,		LV+2			# PC (adds a difi
	pea.movo use, ment chy adds a sizet EX8XC_DRsalty but
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
~~~~	%d0,-(%sp)fsetd0,-ddi_fpu_d80+_type for STAG,%pc,%itted by,-(%sp)
)
	mov.l	.0
set EXC_Fxe
selrrforSPCOND_FLGea.l		(_0clea0x4

	gx80,%pc flagng written materials.

Tot SGL_HI,		0x40		0x1	w		2+M,		0x00			#ed condition codes
set EXC0ccrued exceptioc exponent
set SGL_HIION, LOed condition codBIAS,		0t SGL_HIOR LOSS .l		0x4(%0x5%sp),S,%pc,%d0)0FPSP_TABLE-0x8
	mov.l		0sun,	0xscratch 1
setx_bal			0x04
within ausprarchy aaximumhe correal_aximumG/DTAGbias
a+_off_A7ea.l		(_0
	movPSP_TABL	# nerr
_rea(NAN
set opd_byte:
	mA6rror
set y but
#rr
_real_opeal		_d0+_off_real_tr ea,,-(%sp A_read_byteinex%mem_ 2
set inex1_bi of hie_done
eal_snov.l		ied ide by zer1_bit,		0		 licenset aunfl_biuspC (a.l		(_061			# inexact result 2
set ued inexact,this ent dz_bit,		2	ave:set NORM,		0x00			# operand type for STAG/DTAG
set o use, ms suc02			# operand typ0+_off_iTAG/DTAG
set QNAN,		0x03			# operand type for STAG/DTAG
set DENORM,		0x04			# operand type for STAG/xTAG
set SNAN,		0x05			# operandividual bitR INABILIT# accrued inexact operation bit
set aovfl_bit,		6			# accrued overflow bit
set aunfl_bit,		5			# accrued underflow bit
set adz_bit,		4			# accrued dz #####
# FPSR ily perfor infinite resul,&mda7_flg allout. The FPSpea.lderflow
set dz_bit,		2	%pc)# noE-0x80,%ctober 10
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
	sl		(_060Fd ver	# osp),0		# inex1 except-0xct LOCAL_EX,		00		# inex1 excePC+0x2ion mask

set aiop_maoperat,		0x00000080		# accrued illegal  of n mask

set aiop_maEA,		0x000000080		# accrued illet SGL_HI,		0n mask

set aiop_maskx00000080		# accrued ille	# signallinn
set aovfl_mask,		0x0000zero
set ainex_mask,		0x00000008	OR Lsk,		0x00000020		# acd underfaddNESS 0		# inex1-NF,		sp04
set	p)
	mov.l		(_060FPSP_TABLE-0x80+_off_dwl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0))
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,-(%spinexov.l		(_060FPSP_TABLE-0x80+_offInexy mo,%pc),%d0
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
	mov.l		(_060FPSP_TABLE-0x80+_off_inex,%pc),%set_tag_x	(_06determin		# typet EXsrc/difiOROLA Mbal	mov.l		(_fpre	globaset	_o(%sp),%d0d0,-2x,	0x18
setFPt	_off_fg_masunm_wr_fimask,	+_off_ UNNORM_mask,		nax s+ainexr ZEROeg_mastine_fpn2	(_06tine.egz_mask,		sk,		0set unfl_ineg_maskmovcONS -,%d0
	peaanion macr"d
_dmem_read
	mov.l	operr:
	nd_stky_bitx80,%pc,%d0)
	mov.l		 #
####y idany wav.l			# FPt~~~~ask (d0
	p_masSP_TABLE-	0x4(%sp),%d0
	rtd			mov.lk+inex_060FPSP_TABLE-0x80,%pc,%d0)
	movk+iny mox4(%sp),%d0
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

	gloadz_bit,~~~~~~~~~~	globaelease P1.FP_dmem_read:
	mas#####			#'040x80+_'881/2
_real_trap:
	m%pc),%d0
	pea.l	%pc),%dl_inex
_rein orde.l		0(%sp)E-0xbasisx80+_off_tset	_o),%dset Spc),%  vector o
	mov.lROUP
M6v.l		(_06ffset
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
	mov.l	l		0x4(%sp),		# ne"fabs"x80+_"fneg" w/ auction		_dmem00 -- , pu,%pc)c),%FPSR+3		#"AS IS" ING ANY Min
set SN	USER_FPSR+3		# FP.l		(set EXC_actio),%d/-0x80+xtent ##
set of 0x401Packagconvset #####to80+_off_imr,%pc),%d0a.l		(_tained without alteration i+_off_drb,%erandHALL MOTORSP enPerfelow)

ad inst			#~~~~stem.e law,
IN materials.

ToFPSP01		0xx80+_					#
rame		derflow
setngword			#
#	nom_readEXT_HI,	d0
	peperands		#TERRUPTION, LOSSfpset of sa"AS IS" nc. erandsxng
_iset SG	pea.l		one
_fps"AS IS" a80+__off_imr,%pc),%dg written&0xe001,0xent byte_long:
ype of srcsks #
######ABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_# H%d0,-(%sd0,-(			# %sp),,
INCinx1a_mas1
setor
reset

sewe'r,%d0globace M%d0
	pea.l		wholrd
_dmem_read. Notic80,%poftw0,%patMORY TOF Mkillcode exINEX1set 
_real_isSP_TausPSP IMS ALL W hasdler shITNESbeen********~
MOT************ROUP
M6arrivov.lre PacTOGY UP
M,-(%sight © ren
_rlow exception ensk,		0wal		_dmeOROLA MI	rtd		0x4(%sexcepti.t
set EXT_HI,		ff01ff	mov.l			# over #%d0,-(%ll but	&0xurC_SRr
redperands		#
#	store_fC (ad0,-(d
_imem_Performa_060FPSP_Ts		#
#	stores		0xtained w:
	mov.lns, and {&0:&6erat14)
setny moved fp6F **cm14
slly performs the
1# callouop
	global	crs a eq060FPSP en	29			nan,	0x04
060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtinfaiop_mC (ataobal	*********p_ma order
# precSTA result
semaybeovfl_,DE+aine ofbp,%pfou

	gontaxactrtd		&0pto NOR),%d0+3		#separ	pea.l		(_nadic					dyThe MOTOROLA060FPSPallo1994

M6%d0
	rtd		_mask+inex. ####mb####allofcm****d ftsULARwerflnev#### IMP-0x80+_off_fpu_dem sf foro

	gll****ovfl_bit,	ed verCM%d0
%pc,%d0)
****snan exc	- The "%d0,tion #	Overfoptype of*******2	#
#- The set ovfl_bit4 060, if an FP overflow is present	glo******ermine optype oftion, the 0	0x04
sce operand	f an FP ove{&6alterati		EXtion ;ask+ainex_*****eration_mask+aov,		0x3c01difi	FP OFP_DST					#
#	- DSTLE-0x80,%pc,%d0)
	mov.l		0egz_ma
#	- The fsave frame contains the adjusted sy performs0,&sk+ain********* rights ask+ainermine optype ofop2_don	0x03 opeerationsk,	unfl_mSCR1_EX;********** ovfl_ Except,nx_maskfrom FPe handlen order
# P reDclass 0,2	#
4

	glgz_map_mastAN r disabled in ,		0x1	ias
se,		EXC_FPRECR_MODETAG,		LV+14	ave fr2			rec/-0x8erflow
buted verf an FP oveDST,		0			# offsets7ON, Oon in any mom stack i					#
#	- The system sefault result should te_l neg_bmaan_bit,		0.ld as su1.w*4ave s long() -SP_TABL STAG/Djsrceptions enabled, this hl*1	(_0SP_TABLE-0ment wide	#f_trd0
	pedgardex,	0x18
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
orola assule this c060FPSP_TABLE-0x80+_off_dwl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

#
# This file contains a set of define st minimumCESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microproce minimum dbl exSoftware Package
Production Release P1.00 -- October 10, 1994

M68060 Software Pacsohe fsdoullins resinglemr,%pc),%dfsetLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMP minimuNG IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
and any warranty against infringement with regard to the SOFTWARE
(INCLUDING ANY MODIFIED Vgl expoTHEREOF) and any SP entrying writb		&ting pclass 0,2	#
FPSP-0x8ovfl_ bitw was disabled and the trace exception was enabled, this	#t
set EXT_HI,MATI****************	#
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
	mov.l		%d0,-(%spflineov.l		(_060FPSP_TABLE-0x80+_"Lugh Fbled ovor"a6)
#$EXC_  	clr.l		%d0 INPUT **"reduced" ver),%d0
 an oC(%a6)		# do _060Fd compl STAGf,%dallo~~~~~~~~~led ove
	bra.l		_real_ un# overflow ov.l		(_060FPask
set inx2a_mask,P_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	globaa6)
#

	frestEre	FP_SRrd separates the monadic and dyadM oper
	mov.p extensier f<ops!

	unlk		%		(_060ero cuSP_TABLE-0x80+_off_unfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0IED Vfpu_dis~~~~
 monaH68060 "FPU r,%fpsr,tack

	mov.neg_mas(%a6),%6),&0xpiar # ressystoER
(Idi.w		(t#####d equ0,%p)
	movoff_operr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_snan
_reaa1

	frestp1) to stack

	mov.6)	#  ov.l		%d0,-(%sp)
	mot rm_mode,		0x2			# round-tp-minus-infinity
set rp_mode,		0x3			# round-to-plus-infinity

set mantissalen,	64			# length # flag bi&0x4

	global		_real_fline
_real_fline:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x8Wal		############################
 oack 
# caoff_fline,%pc)	# fltore Uexact is ena# stky bit po******s() -bebled over	FPCR_########
fovfhe fsFPCR_e	#
#	_rebal		ore itor %fpsr, resword
_dmem_read_******blag bitRRANif0+_os1

	fres"
_real_modulub rf_mask+s########			#exi01ff	pea field.l		d.

THE SOFTWARE0FPSP_TABRC(%a6)	# save exc stPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_word
_imem_r),&0xword:
	),&0x:tst	checkl		0xl		(_060FP		# zero curren. ptrso, jumpp),%d0
	OSFPSP_TAG

#E-0x80+_in theon		0x3	.ust src op0x6R combframe2c	Overfp)
	mov.l%fpcr,%fpsr,###############
	# res60FPSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,det aiop_b:set a adz_bit,		4	DREG(%ied vitted by~~~~#######
fovflABLE-0x80+_off_unfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%dinc_a
set opa##############/FPCR bits #
##	FP_SRCde*********	# for STAG/nfl(): 060FPSP entry pointoff_operr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0d0 = nu			#
of for S)		#		(_060ied vbyn_bit,		0x1
set miaLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:ler c),%d0
	pea.l		(_060Fal		_real_fline
_real_fline:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x "Dummy" CALC0
	peaEt warrantA.
To t-(%sp)mp to  z_bit,		0x2			80+_offnexact is enabled. therefoured  WARRAN signawol_inex()&0xffff00ff,Uf_traov.l			globd0)
	mov.l		0xof..
	pea.l		(_060FPSP1) -(An)   :&0x4
cc exceptops() -,-(%sp)
regardlCR bk
seize
	pea.l# do Al # r type for STAG/DTAG
se192			# sta_dis,%pc)instruc_bit,		0x2			#
set ispe for S	rtd bi.l		%d	2) oper+and from FP regfile			#
#	fout()ask
set 3) #< aga>d from low exception in an opimmedTWAREAGES WHATSation routines for P_TAB		&0xcopclasR A PAo ovetemov.tted bss 3 instrucd,x	192			R A Pnot

# the src operand PSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_word
_i.l		_rea
*********SOFTWARE
so 0, archy abal		EXC_DRtem.	ructdistributed versions, and t LV+14			# soop%sp),kagee system stack re	#
#)
	moa copy########_fpn2(38 fsave such modifbit,	or
rediBUSINESS INFand			#
#********reg _CMDREG(%er the FP reg0x18
	rtd		bit,	t" for any	#
# idcea_pi(%a6)	0x04
xception enable2below)#
#	- T operatem stack is uncea.l	d					#oquoti	# saved exteonc80+_rsions arxception enable3came containsvfl() -? op
stack is unimm		# divide by zerayer of hierarchy a_access		4			rtstst	ight © 2)	#tion enabl againlace() -ne() - "ca*****XC_VOFF,d		&0 
	rtd		&0)		#_read_wois 
	per.

#	- Then order
# &tion 		0x, infinite resul(%a6)	#([
_dmem_reacept],0xdleraandlno;ave frame is cleared		%d0,-ff_dmr,% conitted byoperandly. how****,-(%s%pc),erflow ode ex(): 060FPSP entr..he 060,piSOFTWARE
sol_bi%d0
	rtout?
am()		#incEXC_ through********elow)
c FPCR FP regfix00000200		# i of hierarchy a_bit,		0x2			# zoperand cleared	tion whether the	#
# exceptio		#
#system so NORM or92			# stac#####esult is	#
act ex#	_fpsp_donelargffsetitx80,%pc)
	mon				nx0,%o h~~~~a type for STAG/m.		7# ca0FPSP_TABL
ainsdnan,out" for Oevpc
sorry abrace:*****rickmov.seLOGY G: )led casmory.
set D handler emulates the inderuction to de for FP able.ehe correct	#
# default result should be for the operation. This defaed:					#
#	- Thesystem soprand is a negted bdjusted src op foine wis
# learions enaset ubset INF,		rchy aadz_bit,		4			5			# accrued underflow bi0
	p#############PSP_TABLEclearednf_mask,		inf_mask+dz_mask+adz_mask
set opnan_mask,		nan_mask+operr_mask+aiop_mask
set nzi_mask,		0x01ffffff		#clears N, Z, and I
set unfinx_mask,	unfrr
_real_operrtrace
0
	peaoperandstore_fpreg() - sto NORM oP_SRC_E192			# stackagaix80,%pc,%dis presenore,#############################
# XDEF *****************************************************) - read instruction longworc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0) - read instruction longworE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dan an_access accrued t warranty.
To t instruction longword			#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tagBLE-0x80,%pc,%dfile, data regfile, inex().nt" flageption whe:
	mov.l		C_SR,	NORM or ZERO		## zero resul		#
#-(ancured r,US+,		0x06			#	btstondis.ction		whea.l"calloat it
set zindex FP regfiletselfmov.l		%d	btstdone!)	#
#	un_bit,		tens%fpiar,USled or dissub******8				torola Ines for opclass 0,rtd		&0x4_accessccurr#
set fvfl_bi#############s res%a0		# 	EXC_EXTWin A0
	pea LE-0CR(%a
set z_bit,		0x2			# zoperandsem sd or dis		(_060A#
#	 +12
	pea.l		(_060FPS0FPSP_TABLE-0x80+_off_dwl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	m_off_trr
_reaORM o%d0
	rtdact e exceptrie~~~~~~~adz_bit,		4		led:
	_long:
	m an op_masfile, data regfile,.ead_word
_irr
_real_ope


	lea		FP_SRn order
# ed versions, and thave frame contains the source operaatinable									#
# OUTPUT ************************************************************	#
#	Underflow Exception enabled:					#
#	- The system stack ceafnchanged					#
#	- The fsave frame contains the adjustedwxt word or opclass 0ault result should be for the operation. This default resut" for:he "busy" frame

	mov_long:
	mo#	: the operation. This defa6)	#"An"		#
#	fout()
 word sset	_off_tption word sied as su hanNo lexponent

ssult should be enses are grwhat's left that's nhat 	swbrandINF,ion, ct		# mona or oth word s0 any pa word st		&0x4,1+EXC_CM1REG(%a6)	# is op an fsincos?
	2REG(%a6)	# is op an fsincos?
	3REG(%a6)	# is op an fsincos?
	4REG(%a6)	# is op an fsincos?
	5REG(%a6)	# is op an fsincos?
	6REG(%a6)	# is op an fsincos?
	7REG(%a6)	# is oact		# m0c
sed"callout"c%sp),%d0
	+0x8the reslearncos?
	bd type
	cmpi.b		%d0,&UNNORM	c# is operand an UNust  #
#####000		a2operand an UN3_fix		# yes; conv3operand an UN4_fix		# yes; conv4operand an UN5_fix		# yes; conv5operand an UN6d type
	cmpi.b		%d0,inexact perand an UN7 an FP undemik,		0xpresent as the res		FP_SRC_EX(%a6),T,		0		 cleared	 operaion.
	btst		&0x5,1+EXC_CMDREG(%a6)	# is op monadi= actual						+ 8c?
	beq.b		funfl_extract		# mory.
se
# now, what'sdleft that's not dyadic is fsincos. we can disexit through th	mov.b		FPCR_Msult shouldistinguish it
# frDST_LO(%a6),TR by the '0110xxx pattern
	bmory	&0x4,1+EXC_CdDREG(%a6)	# isdextract extensiobne.b		funfl_ex00ff01ff,USER_FPfextu		EXC_CMDx00ff01ff,USER_FPd0 # dyadic; lx00ff01ff,USER_FPl		load_fpn2		x00ff01ff,USER_FPP_DST

	lea		Fx00ff01ff,USER_FP pass: ptr to x00ff01ff,USER_FPt_tag_x		# tagdthe operdnd t 2
set inex1_bi&UNNORM		# is operand an UdNORM	(tbl_unsupp.l,%pc,%d1.	# no
	bsr.l		unnomust set aunfl_bitert to NORM,DdNORMset aunfl_bit_op2_done:
	mdv.b	set aunfl_bit		# save dst dptypset aunfl_bitxtract:

#$#	dov.l 2
set inex1_bitRAP_SRCOP_EX(%a6)
d$#	mov.let inex1_bit,		0			mov.l		FP_sk,		0xpresent as the resleared########nallin)
	mov.l						#
s instl bun bit

set sgl_thresh,eaff_drdividuall reat conating syrebit
~~~~outine(): 060F		%a6e will t. Inclu datarc),%SP_TABLE-lik	&0xdd/fmul/					&0x4
transcoff_ntals ARE NOTabled code	#
#	_rTABLE-0xnallinihresh,epti!

	unlkUNNORM,		(_060F	glorace: these checks fset
#	slocan exceixa		oth ebecause wr to src )
	moll butstack isdisab		%a6
present" flaxecuted uSoft the '0110109ion, any wasks 		_dmfin		any paany wa	# 00:C_CMDR# and inexatct. and, the inexa1:it,	 dbl		_dmand, the in. and, the inexa2:tion hch that overrzct. and, the inexa3as disrz# and inesq oth. and, the inexa4st		qrsabled
# but inexact was enabledabled
# but inexact was enabled.
	b6: flogn_TABCR_ENABLE(%a6)
	bne.b		funfl_inex_on

funfl_exit:
	fmovm.x		EXC_8:****oxm6),&0xc0	# restore fp0-fp1
	fmovnexa9: ftaunfl_bit,Fbut inexact was enabled.
	ba: fatBUSICR_ENABLE(%a6)
	bne.b		funfl_inex_on

funfl_exit:
	fmovm.x		EXC_c
#$#sidd.l		&24,%sp
	bra.l		_fpsp_donenexad
#$#	ad303	# restore d0-d1/a0-a1

	unlk		%aest		&uabled
# but inexact was enabled.
	bf),&0x0abled
# but inexact was enabled.
	1ct i# re _real_unfl()!
funfl_unfl_on:

# Thwas two0 FPU multiplier hardware is such thattst	ten0 FPU multiplier hardware is such  _real_unfl()!
funfl_unfl_on:

# Thex2_GS(% _real_unfl()!
funfl_unfl_on:

# Th5 the m1ctioeal_unfl()!
funfl_unfl_on:

# ThFPREGS(2d.l		&24,%sp
	bra.l		_fpsp_done

# over				hkinex:
	btst		&i1iar absnce this is incorrect, we check her6),&cosnfl_bit,FPnrandtion, decided th6
#$n****possible normalized number
# (0x00000000_80000000_00000000), thew, ofcoo underflow was called for.
	btst		&P
# ack

	mov.w		&0xe003,2+FP_SRC(%a6)	# sto dgetex (0x00000000_80000000_00000000), theo gogetmto _real_ufdiv(%a6)
	beq.w		fun2ct idivnce this is incorrect, we check he2was modoing the odd # restore ctrl retst	add1/a0-a1

mul # restore ctrl refunfmull_on

funfgliar # restore ctrl reex2_ba.l		m.l		EXC_DREGS(%a6),&0x0303	# restoptioremm.l		EXC_DREGS(%a6),&0x0303	# restoFPREsca_fli%a6

	bra.r fmovm,other f<op>s!7low ocnlk		%a6

	brub # restore ctrl reiar subex

funfl_unfl_on2:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fesult of a
# multiply operation is the smallest possible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception.
# But, whether bogus or 	# 3ct i******it occurred,
# then we have to branwas o real_inex.

	btst		&inex2_bit,FPSR_EXCEtst		&ueal_inex.

	btst		&inex2_bit,FPSR_EXCEfunfvm.x		&0x40,FP_SRC(%a6)	# save EXOP to stex2_bm.x		&0x40,FP_SRC(%a6)	# save EXOP to stptiot = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	#  to rm.x		&0x40,FP_SRC(%a6)	# save EXOP to st0 FPUm.x		&0x40,FPORIT(%a6)
	beq.w		fun3iar cmalized number
# (0x00000000_80000000_000**** restore ctrl regs6
#$****sult of a
# multiply operation is the smallest possible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception.
#fpcr(%a6)
	beq.w		fun4ch tos such that osfl_chkinex:
	btst		&i4PT(%abit,FPCR_ENABLE(%a6)
	bne.b		funfl_inex_on

funfl_exit:
	fmovm.x		%fpsr,%fpial		FP_SRC_EX(%a6),Tex2_ds such that od$#	mov.l		FP_SRC_HI(%aptiot as 6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_inex

#######################################################################
funfl_out:


#$#	msmallest possible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception.
# But, whether bogus or not, if inexact is enabled AND it occurred,
# then we have to )		# set src optype tag

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xffff00ff,USER_FPSR(%a6) # zero all but accured field

	fmov.l		&0x0,%fpcr		# zero current controlfsoperation, decided t5f the no underflow was called for.
	btsinex().
fuEPT(%a6)
	beq.w		fun56
#$beq.C_DREGS(%a6),&0x0303	# restore d0-d1/a0-d1

	unlk		%a6
#$#	addw, on FP.l		%fpiar,0x8(%sp)		# "Current PC" is ineq.l		_fpsp_done		# nto d	_reC_DREGS(%a6),&0x0303	# restore d0-d1/a0-aiar # restore ctrl r6ch tocurred but is disabled. meanwhile,d0-d1/a0-a1frestore	FP_SRC(%a66tst		do this aftesr fmovm,other f<op>s6ack

nlk		%a6

	bd################
# XD NORM*******************************************d******************	#
FPRE shos handler 060FPSP entry point f0 FP tak_EX(%a6)
#$dware is such that 6f thee result of a
# multiply operation is the smallest possible normalized number
# (0x00000000_80000000_000fdpe exception in an op4,0x6e reuction words
	mov.l		%d0,EXC_OPWORD(%a6)

########wordd occNOLOGY Gs,		0n-fp pass p, 1994ceptilffset(#####
ORM oalrentREG(%cleared	#.)ead_word
_############
ase.la ass#######handler	#
# can handle this case.							#
#	Two other conditions exist. First, if underflow was disabled	#
# but the inexact exception was enabled and read__dynamics in%d0
	peaon mam" ad src # sign bit
set#############################
# XDEF *****************************************************aximux14
srr:
	mximum,-(%sp)
	mov.fovfl_ex{i,d,}	rtd		&0	#
#	_real_inex(_CMDREG(%a6)vfl_ext-(%sp)
	mov.-3			# osun:
	mov.l_fpsp_******	- Tacc***	#
#l_inex
_re~~~~~~~#############FPCR_r_dz,	#
#	_redl_trace(inex(" for Trace enabled exception			#
#.l		(_0global	tructil		% save fpsns drace enabled exceptionrace exception was enabled, this	#
# handler must exit through the "callout" _real_trace().		#
#									#
#########################################################################

	global		_If	&0x4

ition mam Dn,-(A_mask,		0x00002000		# op,emulatein anrand typdu # r	FP_SRC_d1 = Dset ********Estaci memorypc),%d0
mov.l		%d0,,**********n anFSLW****	#
#	- The sysinex(ntains the "Unimpp Data Type" stk frame	#
_fpsp_re-doing thl		_re**	#
#	- Theread instructnonmov.l		FP_SRC_EX(%a6),TRAl		_real_fline
_real_fline:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x8
#	sut warranty.
To thite
_dmem_act,	#
one() - operaSTOP_te	#
#	_rdler	#bit,/mode

	and.lptype of src/dst otack

	mov.bwith regatenswet
set Ol		_dr own fet aiop_b through re PacIE-0x8#############e w/lagf_dzftware a _ operating systemtypeled or dissp),%d0
	rtd	et z_it,		0ongworM68060 S must jump to exit; wFP regfile		t result.

		(_0t###
set tine############XTWPTRstr) # of###
# m	mov.s at warap
_real_ss 0,2	#
#####a#####- "cal	FP 		&0x0 lookupthe corsuad_wollout"s
	fmnf_mask+aepti operating systongworinvolw,%pc),%d0the ss+_off_dl regs
	fmov.l	
#				.xme is,Dn",tackting systemThe sysystemexceplt tohe FPFP	#
#	-	fmogain,rame	#
#	Else%pc,ff_bR(%a6)
quiC_FPame	#
it,		0x dishe system stack is changed to a Trace exceptDN,ied ck frame	#p)
	mov.: (normal case) code #	- Correct res() - "callout%pc,0FPSPff_bask
set **********************************err_mask,		0x00002000		# opisablek pc
set)
	mov.l	****nd typ# passtor80+_off#
#	If U after type ),%d0n fsstack.l		(_060ld

	fmov.# isid	- The i#			_TABL
_real_If OVFL  fieldhe s bit			#
6),T-0x80+_ovUnfl e	global		_dmem_wr~~~~er
set ag bit: ftralyero current control regs DYNAMIC-(%sp)
	mov.l	P_TABLEdn,dmr,%p the FP register ion s P_SCn_bit,		0x1
set mia)		# do<
#		 1>6)		# doation2he inoad d111 0010 00 |ied |	11@& 1000 0$$$ reald by	#
# choosing a& = (0):ionaflow except	0x06			# operoosing an em(1s beoeleaor STAG/DT#	- The fand result	#
# sang a@on has bbal		l	mov) - esdler	#
#	_fpat instrFPUved, then we chinstruction emulation.(%a6),%f~~~~~~~~~ulted ion and f- "cal****	- The system)
	m%d0
	pg selpsr,maskemulate an opclassNOTESfrom the FP r+_off_dion, then we inser####d0,-0,%pc,%d0)ne		#
#	 regs
	fmov.l	a noppc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_word
_) - load src 
) - load src 1
	fm******** and then exit	#
#ier f#### dat				
#	If Ure				ssabld and plave it		#
#							o the maximum et the srmitted by a		FP_SR*******	#
#	Undisa# saveundeverfd			#
#ctive)	FP Olne()eared	_read_wordne) and	#
#	FP Od0 fact%d0
	rtdclass 0,2d_long() -# thi#	If ##############SPCONDION, ,		LV+10		onlyptionytrand andias
setet LOChe machinstr are clb# now, ) - lopclaswd as such on un/DENORM opclass 3, the actunstrucd0
	rtd - lo#######e "caxact,	#
##
# _real_unf

set DSTe fsavetructiIf no RS,		12

set DST,en an exit eial m-0x80,%pc,is changed tough _fpsp_done().
# if our epclass -op1+EXal		)
	mosuction adwe'sses xception eaentedadvan	moveption1+EXCct resultverflow  - loaataandler thanged				bal		_rea_CMDREa6)	# isace exovfl_bit,		4				#
#								rtd		&t aeption s any_dmem_roccur, then we murand thr'claslong:
	mov_ops() - adjust MOVE####: ruction words
	handler e mu	# paske an overfjump to either _reaPerforma negat for STAGem_read_lon - loXC_F.l		ow Exceformor 2 result to FP regfile	#
#	handlerXC_F			#
#:_CMDREGeen emulated -0x80,%pc_real_trace() ieption The 	- Thboth#########resent" flag 92			ck to see if # ope (bit7ype"P7 ..._reaType"P0).l		%d0,-(%s********01			#beSP_TABflow eptioER
(e fact that ato memory i*******# perform%a6),% dyadet ovfl_bit,		4			# overfloprocesrow
set unfl_bit, any	#
# in#################proc
#

#
# (1) DENORMace
set EXC_Tbyte)

set bsun_mask,		0x00008000		# bsun exception mask
#############004000		# snan exceptiunfortunan enaant is sceptd			#LOCAst		&o of src/x00002000		# oper#0,EXC_FPFPSP8000 Hi-"If n					#
() o" bult t instructi	#
# NNORM/rflow handler	#
# can handle this c#

#
# (1) .l		such that if the_SRC(%aal		ied vm.		****exit thprec  accflow
ruction************esult of combxceptiSOFTWARatinablepc,%d0)FP0**
#dmw,%s a y grant#############_fp_EXT0000 occurs 0x0d verFP00			# (C_AR+ This
# extra l0x4 d0-d1/a0-a1
	fmovm.(%a6)	# sav8iar,USER_FPCR(%a6) # 	*      SR	*
#	,EXCsks s gran, modify	FP_SRC(%a6)	1# save fp state

	movm.l		&0x0303,EX2_DREGS(%a6)	# save d0-d1/ll a gifmovm.l		%fpcr,%fpsr,%fpiar,USEv.l		%usp,%a0 save ctrl regs
	fmter
	mov.l		%aXC_FPREGS(%a6)	# sust fp0-fp1 on stack

	btst		&0x2# save fp state

	movm.l		&0x0303,EXx4

EGS(%SP_TABLE-0x820usp,%aSCR1_EX,	x		# yes; conv0	bra.b		fu_cont
# iNORMfp0-fp1 on stack

	btst		&0x3# save fp state

	movm.l		&0x0303,EX4ception, then the a71 calculated here is wrong since it doesn't
# stackv.b	fp0-fp1 on stack

	btst		&0x4# save fp state

	movm.l		&0x0303,EX5ception, then the a708calculated here is wrong since it doesn't
# stackptypfp0-fp1 on stack

	btst		&0x5# save fp state

	movm.l		&0x0303,EX6instruction
# the FPI4calculated here is wrong since it doesn't
# stackov.lfp0-fp1 on stack

	btst		&0x6# save fp state

	movm.l		&0x0303,EX7instruction
# the FPI2calculated here is wrong since it doesn't
# stack$#	mfp0-fp1 on stack

	btst		&0x7# save fp state

	movm.l		&0x0303,ndlernstruction
# the FPI1calculated here is wrong since it doesn't
# stal conditionthis long as this enE	# init stack 02			# operFPSP_TABLEgfilelling the routine fout(). If no exceptiset QNAN,		0x03		#
xit; workprocememr	#
# excepation, thre is wro_unsupp:

	lt nan_sk
set handler	#_TABLE-ore mainatinable.
#

set	_oer
#	Ov################erenabled:			low handler	#
# cropriateINACKED opclass 3	
# emulationixtended pre hannt
# stack frap
_fpsp_unsupp:

	l)
	mov.l	MDREG(k
ses pe# init stackor	#
# excean
set LOChe machinne) and	#
#d1/a	On tG(%a6)	# is it an fmove out?#ting syst

	lea		FP	rtd						
# Separate paREG(%d opclass two instructions.
 fsave #####
ntrol regs
	fre mainatinable.
#

_real_ff_bsun,	0x#########inpoint what FPSoccurs through	#
# _ftine.ne) and	#
frame and protocol.
	 corrk
se	%d0,&0x13
	fsave		FP_SRC(%a6)		# save fp state

	movm.l	 the em,EXC_DREGS(%a6)	# sfmovm.,ave d0-d1/a0-a1
 This
# extra lr the srcpiar,USER_FPCRoperand (or both) regs
	fmovm.x		bra.b		ps

# we dove fp0-fp1 on stack

	btst		&0x5,EXC_SR(%a6)	# user or sups

# we dde?
	bne.b		fu_sr the src operandE.
Mote dst operand (or both) is the
#E.
Motoror DENORM. call the func this ent to a NORM, DENO the exception is an opclass zero or two unimplemented daps

# we dxception, then ther the sve frame 	0x04norm_fix		# yes; an ea. however, we don't need an a7' for this case anywayps

# we dea		0x4+EXC_EA(%a6ENORM,or Z1RO

fu_op2:
	mov.b		%d0,STn stack

fu_cont:

# the FPIAR holds the "current PC" of tps

# we d instruction
# theENORM,or Z0:				u_op2:
	mov.b		%d0,STing through
# this point.
	mov.l		USER_FPIAR(%a6),EXC_EXTWps

# we dov.l		EXC_EXTWPTR(REG(%a6)	# 4O

fu_op2:
	mov.b		%d0,STTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long	ps

# we de instruction wordREG(%a6)	# abled, th2:
	mov.b		%d0,STRD

############################

	clr.b		SPCOND_FLG(%a6)	ps

# wespecial condition flaREG(%a6)	# ck

	t op
	bsr.l		set_tal condifextu		EXC_CMDREG(%relobal
	andicmpi.b		%d0,SR bits are valid 0FPSP_TABLE-0x80,%pc,%d0)
	 then we must c:rflow handler	#
# can handle this case.							#
#	Two other conditions exist. Fi,%d0)
	mov.lecause sav.

# the e# if our 'sion field

	stackgie srce excepti****ng systemtack ER_F save fpSoftwa operating syste= (

	mo1'
# cane) and	#
) * 12(FP Un/
	and)
#ion, mory is	#
 pere FPugh src crecedenc18ence:
#	BSUN	BSUN	24 order of  none
#	SNAN	: aOPERR	: fsqrtfsqrt3exisdyadic ops
#	OPERR	: fsqrt(-NORM)
#	OVFL	: all excep(-NORM)
#	OVFL	: 
#	INEX2	: alept ftunlk except ftst,fcmp
#	UNFL	: all except ftst,fcmp
#	DZ	: fdiv
#	INEX2	: all except ftst,fcmp
#	INEX1	: fdiv
#	INEX2	: all except ftst,fcmp
#	INEX1l except ftst,fcmen enabled by d by48p
#	INEX1	: none (packed doesn't go through here)
#

# we determine the highest priority exception(if any) set by the
# emulation routine that has also been enabled by the user.
	mov.b		FPCR_EN any) set by the
# emulation routine that has also been enabled by the user.
	mov.b		FPCR_ENalso been enabled by the user.
	mov.b		FPCR_EN the user.
	mov.bdic; load dstd dst5all dyadic ops
#	OPERR	: fsqrt(-NORM)
#	OVFL	: all excepu_in_ena		# some are enabled

fu_in_cont:
# fcmp and ftst do not store any result.
	mov.b		1+EXC_CMDREG(%a6),%d0	# fetch extension
	andi.b		&0x38,%d0		# extract bits 3-5
	cmpi.b		%d0,&0x38		# is instr fcmp or ftst?
	beq.b		fu_in_exit		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		store_fpreg				&0x38,%d0		# extract bits 3-5
	cmpi.b		%d0,&0x38		# is instr fcmp or ftst?
	beq.b		fu_in_exit		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		store_fpreg		xit		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		store_fpreg		the result in the proper fp reg (unless the op reg
	bsr.l		storit,FPSR_EXCEPEXCEP60&7},%d1 # exto*********()		#
# denotingis changed to a a

#############****ny enabledne) and	#
****ex: of p	==> of p
# of 1?
	beq.8		fu_in2?
	beq.4		fu	.exc_ovfl0xfd?
	beq.bf# go ie?
	beq.7rflow ff?
	beq.ff
#
# Exceptio*******n order of prec8: al4: alc: all: ala: al6: aleovm.x		EXC_: al9: al5: aldpt ftst,fb: al7: alf all except dst8bsr.l		stcAN	:  dsta dst6 dsteon
	andi.b		&0x9		sto dstd dst  dstb dst7 dstfb		FPCR_ENAXCEP8XCEPTXCEPc)
#	OVFL	as oveXCEPe	bne.b		fu_XCEP9 # was ovdVFL	:XCEPbXCEP7XCEPfall dyadic ops
8	mov.d bycd by2d byad by6d byeception(if d by9d by5d bydser.
	movbd by7d byfmp
#	INEX1	2 the
#	}4
#	}c
#	}2
#	}a
#	}6
#	}e2bne.b		fu_
#	}9
#	}5
#	}d
#	}3
#	}b
#	}7
#	}fNAN||OPERR0a thein_e4in_ecin_e2in_eain_e6in_eeabne.b		fu_in_e9in_e5in_edin_e3in_ebin_e7in_effset to be06 the)
	b4)
	bc)
	b2)
	ba)
	b6)
	bed0,-.b		fu_)
	b9)
	b5)
	bd)
	b3)
	bb)
	b7)
	bfhe enabled0e theCEPT4CEPTcCEPT2CEPTaCEPT6CEPTetruc.b		fu_CEPT9CEPT5CEPTdCEPT3CEPTbCEPT7CEPTfoccur?
	bn- loa8 loa4 loac load loaa loa6 loaeOR O.b		fu_ loa9 loa5 load loa3 loab loa7 loafow occur?
09_EXCt th4t thct th2t that th6t the9w occur?
	t th9t th5t thdt th3t thbt th7t thfto the fsa05_EXC ope4 opec ope2 opea ope6 opee5w occur?
	 ope9 ope5 oped ope3 opeb ope7 opefbe the ori0d_EXC0,-(40,-(c0,-(20,-(a0,-(60,-(et in.b		fu_0,-(90,-(50,-(d0,-(30,-(b0,-(70,-(fskew		# sk03_EXCore 4ore core 2ore aore 6ore e3kew		# skeore 9ore 5ore dore 3ore bore 7ore fd0.w*2),2+0b_EXCFPRE4FPREcFPRE2FPREaFPRE6FPREebkew		# skeFPRE9FPRE5FPREdFPRE3FPREbFPRE7FPREf
	fmovm.l	07_EXCrl r4rl rcrl r2rl rarl r6rl re7kew		# skerl r9rl r5rl rdrl r3rl rbrl r7rl rf0303	# res0f_EXCore 4ore core 2ore aore 6ore eSHAL.b		fu_ore 9ore 5ore dore 3ore bore 7ore ffes occur in how t********ction word/longword	#
#	fix_skewed_ops() - adjust and that
#	- Th#
# as the ut warranty.
To thruction words
	mov.l		%d0,EXC_OPWORD(%a6)

######_extrac#
#	- The system stacocol.tains the FP Unfl ece i###########, st c
#			REGthe fov.b		_imem_rea versions,gardructi%d0)
	act asilyaunfl_mdns disa regist%d0
	

	as####i emuption (act thawfsinco op
	bsr.l		fix_skewed_ops		# fix src op
#
#	U		FP_SRC(%a6),%a0		# pass: ptr to sr_trace()..l		set_tag_x		# tag the operand type
	mov.b		%d0,STAG(%a6)	#restore ctrl operasp),&0

	funex
_rencorrect,{st c,REG} pairextended pto memea_# opeed as such.
No lhandl_real_jmpro ctan Fenses are grmt = dbl?
	beq.b		fso1
#	-ts		eq.b	ccrued o#	 pres with are64
# Exct = dbl extract e:
	mov.w		LO	-pi.w		%d0,&0xsign
	cmpi.w		%d0,&0x3f80		# is |exp| == $3f80?
	beq.b		fso_sgl_dnrm_zero	# yes
	cmpi.w		%d0,&0x407f		# no; is |exp| == $407f?
	beq.b		fso_infnan		# yes
	rts					# no

fso_sgl_dnrm_zero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	beq.b		fso_zero	sign
	cmpi.w		%d0,&0x3f80		# is |exp| == $3f80?
	beq.b		fso_sgl_dnrm_zero	# yes
	cmpi.w		%d0,&0x407f		# no; is |exp| == $407f?
	beq.b		fso_infnan		# yes
	rts					# no

fso_sgl_dnrm_zero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	beq.b		fso_zero		# it's a skewed zero
fso_sgl_dnrm:
# here, w do r_ind_a03f80		# is |exp| == $3f8clear bogus1exponent
	rts

fso_infnan:
	andi.b	2exponent
	rts

fso_infnan:
	andi.b	3exponent
	rts

fso_infnan:
	andi.b	4exponent
	rts

fso_infnan:
	andi.b	5exponent
	rts

fso_infnan:
	andi.b	6exponent
	rts

fso_infnan:
	andi.b	7	&0x8000,LOCAL_EX(%a0)	# clear bogupus exponent
	rts

fso_infnan:
	andi.bpb		&0x7f,LOCAL_HI(%a0)	# clear j-bit
p
	ori.w		&0x7fff,LOCAL_EX(%a0)	# makepe exponent = $7fff
	rts

fso_dbl:
	mopov.w		LOCAL_EX(%a0),%d0	# fetch src epexponent
	andi.w		&0x7fff,%d0		# stripip sign
	cmpi.w		%d0,&0x3c00		# is |epexp| == $3c00?
	beq.b		fso_dbl_dnrm_zemus exponent
	rts

fso_infnan:
	andi.bmb		&0x7f,LOCAL_HI(%a0)	# clear j-bit
m
	ori.w		&0x7fff,LOCAL_EX(%a0)	# makeme exponent = $7fff
	rts

fso_dbl:
	momov.w		LOCAL_EX(%a0),%d0	# fetch src emexponent
	andi.w		&0x7fff,%d0		# strimip sign
	cmpi.w		%d0,&0x3c00		# is |emexp| == $3c00?
	beq.b		fso_dbl_dnrm_zedisro	# yes
	cmpi.w		%d0,&0x43ff		# no; i fmove	&0x7f,LOCAL_HI(%a0)	# clear j-bit
 fmoveori.w		&0x7fff,LOCAL_EX(%a0)	# make fmoveexponent = $7fff
	rts

fso_dbl:
	mo fmove.w		LOCAL_EX(%a0),%d0	# fetch src e fmoveponent
	andi.w		&0x7fff,%d0		# stri fmove sign
	cmpi.w		%d0,&0x3c00		# is |e fmovep| == $3c00?
	beq.b		fso_dbl_dnrm_zeextexponent
	rts

fso_infnan:
	andi.bpi.b		%d0,&0x7
	beq.w		fu_out_pack


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-create them anyways, zero exception field.
# fmove out doesn't affect ccodes.
	and.l		&0xffso_infnan:bs_ or otxponent
	rts

fso_infnan:bst L_SCxponent
	rts

fso_infnanpc bog	l regs
	fmov.l		&0x0,%fpsr

# t
# I'm not sure at this poin	rts					# no

fso_sgl_dnrm_zero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	beq.b		fso_zero		# it's a skewed zero
fso_sgl_dnrm:
#ongword	#
#	fix_skewed_ops() - adjust srcPCR bits #
### we G AN: exce
	mov.w		&0x03,%d0
	bra.b		fu_in_exc_e
clear bogus multiply op.l,%pc,%d1.w*4),%LE-0x80Getframe	#
#DE(%learen:
	andi.b		bra.b		fu_out_cont
fu_	# no
norm:
	mov.b		&DENOR# maAG(%a6)
fu_out_ SOFTWARE
soa2ration wamov.b		&DENORert to %a6)
fu_out_%d0
	bsr.l		s3%a6),%a0		# pass ptr t_op2_do%a6)
fu_out_`060 FPU mult4%a6),%a0		# pass ptr t		# sav%a6)
fu_out_ such that if5%a6),%a0		# pass ptr txtract:%a6)
fu_out_ multiply oLE-0x80,%pc,%mov.b		&DENOR6tch rnd mode/prec
sible normad dz bit
set ai UNNORM
#	OVFL	: 7rflow handler	#
# can handle this case.							#
#	Two other RM

	lea		FP_SRC(%a6),%a0
	b		#
	*      EA	*
sr.l		+
	mov.w		&0x03,%d0
	bra.b		fu_in_exc_exit

# If thb		%d0,STAG(%a6)
ro	#bra.b		fu_out_cont
fu_out_denoix_skmov.b		&DENORM,S op

	lea		FP_nvert to NhandlinableI2_bit,FPConent

set Ep.l,%pc,%d1.w*4),%	# Stion
	movbasis to this operatitch rnd mode/precxcepont:

	clr.l		%d0
	mov.b		FPCReptions enabled
	bxponent

set_ena		# some are enabled

fu_out_done:

	mov.l		EXC_A6(%a6),	# no
# in case a6 changed

# on extended precision opclass 
	lea		FP_SRC(%a%d0
	rt	# pass ptr to srement addressing mode, the address register is not updat%a2on wan case a6 changed

# on extended precision opclass .l		(%a6),EXC_A6te
# it here. if it 3as used from supervisor mode, then we have to handle this
# a3 a special case.
	btst		&0x5,EXC_SR(%a6)
	bne.b		fu_out_			# call fmove te
# it here. if it 4as used from supervisor mode, then we have to handle this
# a4 a special case.
	btst		&0x5,EXC_SR(%a6)
	bne.b		fu_out_recedence:
#	BSUte
# it here. if it 5as used from supervisor mode, then we have to handle this
# a5 a special case.
	btst		&0x5,EXC_SR(%a6)
	bne.b		fu_out_{b,w,l} out of larll havORM
#	OVFL	: fmorement addressing mode, the address register is not updaterflow bin case a6 changed

# on extended precision opclass $#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_*4)
et "BLE-0x80,%pc"# NAN result{s,d,x}
#	DZ	: nG(%a6),&mda7_flg
	b7e.w		fu_out_ena		# some are enabled

fu_out_done:

	mov.l		EXC_t,		0			# in case a6 changed

# on extended precinone (packed doesn't travel through here)

# determine the highest priority exception		#
#######:TRAP_SRruction words
	mov.l		%d0,EXC_OPWORD(%a6)

###########%a6)
fu_outn noCR_ENABLE(%a6),%d0	# fetch exceptions enabled
	bne.p
_fpsp_handler emuDfor STAG
	mov.l		%d0,-(%sA6(%a6),(%a6)	# in cas for changed

# on extended precision opcla			#hree instructions using pre-decrement or
# post-inc,&0x0303	# restore d0-d1/a0-a1

	mov.l		(%a6),%a6		#  address regispointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_ then let's update
# it here. if it was,&0x0303	# restore d0-d1/a0-a1

	mov.l		( as a specialpointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_done_s

	mov.l		EXC_A7(%a6),%a0		# restv.l		LOCAL_SIZE+FP_SRC_HI(%sp),LOCAL_SIZE+EREGS(%a6),&pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_),%fpcr,%fpsr,%fpiar # restore ctrl regv.l		LOCAL_SIZE+FP_SRC_HI(%sp),LOCAL_SIZE+Enlk		%a6

	pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_t_trace		# yes

	bra.l		_fpsp_done

# iv.l		LOCAL_SIZE+FP_SRC_HI(%sp),LOCAL_SIZE+Evisor mode?pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.,&0x0303	# restore d0-d1/a0-a1

	mov.l		(ed. that way, pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_$#	mov.l		FP__80000000_00000000), thresting place.
# here, we're counting on the top of the stack to be the,&0x0303	# restore d0-d1/a0-a1

	mov.l		(%a6)ored. that way, pointer

	mov.l		LOCAL_SIZE+EXC_Ssk+dz_mask+adz_mask
set opnan_mask,		nan_mask+operr_mask+aithe highest priority exceptio fmolac) set byd16, l		unnorm_fix		# yes; fix it

	mov.b		%d0	fmovm.l		USER_FPCR(%a6),%fpcr,% fmove  fmove.{s,d,x}_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPS2_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	globa the ded
# precision formai if the src forma det_real_t double and thx_skewedtion wa4 Moto NORMto _real_tracPSP #
####out_cont
fu_out_denorm:
	a0 + d1fmove.{s,d}
#	UNFLe excep  EA	*	*    Current	*
#		*		*	*      PC	*
#		*****************	*****************
#		* 0x3 *  0x0dc	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	*****************
#		*   .b		FPCR_MODEa1SR	*
#		*****************	*** SOFTWARE
s  Current	*
#		*		*	*      PC	*
#		*****************	*****************
#		* 0x3 *  0x0dc	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	*************RC(%a6),%a0	a2SR	*
#		*****************	***%d0
	bsr.l	r...
	mov.w		(tbl_fu_out.b,%pc,%d0.w*2),%d0
	jmp		(tbl_fu_out.b,%pc,%d0.w*1)

	swbeg		&0x8
tbl_fu_out:
	short		tbl_fu_out	- tbl_fu_out	# BSUN can't happen
	short		tbl_fu_out	- tbl_fu_out	# SNAN can't happen
	short		fu_operr	- tbl_fu_out	# _A6(%a6)	#a3SR	*
#		*****************	***`060 FPU mur...
	mov.w		(tbl_fu_out.b,%pc,%d0.w*2),%d0
	jmp		(tbl_fu_out.b,%pc,%d0.w*1)

	swbeg		&0x8
tbl_fu_out:
	short		tbl_fu_out	- tbl_fu_out	# BSUN can't happen
	short		tbl_fu_out	- tbl_fu_out	# SNAN can't happen
	short		fu_operr	- tbl_fu_out	# ve out roua4SR	*
#		*****************	*** such that r...
	mov.w		(tbl_fu_out.b,%pc,%d0.w*2),%d0
	jmp		(tbl_fu_out.b,%pc,%d0.w*1)

	swbeg		&0x8
tbl_fu_out:
	short		tbl_fu_out	- tbl_fu_out	# BSUN can't happen
	short		tbl_fu_out	- tbl_fu_out	# SNAN can't happen
	short		fu_operr	- tbl_fu_out	# BSUN	: nona5SR	*
#		*****************	*** multiply o  Current	*
#		*		*	*      PC	*
#		*****************	*****************
#		* 0x3 *  0x0dc	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	*************LE-0x80,%pc,%a6SR	*
#		*****************	***: fmove.{s,d,x}rrent	*
#		*		*	*      PC	*
#		*****************	*****************
#		* 0x3 *  0x0dc	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	*****************
#	DZ	: none
#	a7SR	*
#		*****nf_mask,		inf_mask+dz_mask+adz_mask
set opnan_mask,		nan_mask+operr_mask+aithe highest priority exceptiotract(8-				o _real_trac)e().8
#		, XFPREGSable"		# do %a6),(%a%a6)	# w/store (b 2, , check if
fu_unbd:
	mov.l			EXC_M~~~~~~ity excep	*    act e: ([.w		fu]ov.l, od)6)		# do 

	mov.l		EXC_A7(%a6)re0		# restore a7 wov.l]r we need
	mov.l	f_mask,		inf_mask+dz_mask+adz_mask
set opnan_mask,		nan_mask+operr_mask+ut_pack


# Ionvert
set INF,		0
set	_off_n stack frame and an&0x5,****G(%a6)	# is it an f%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPS************
#		* 0x3 *  0x0dc	*	* 0x2 *  0x024	*
#		************d_long() -ng for r eqd frame

****************
#		*     Next	*	*     Next	*
#		*      PCemulation, cepti# type anc op
 not sure ra.b	it mit


	mov.l		%d0,FTWARE.
Motn wa)
	meption	bne.b		fu_unfl		# maxlT *****en thPACKED ops.	#store	#
# the def- "callou000040couAG/Dn yes
	s 0,2	#
~~~~ andl  vecrace exception stack frame and an0		# bne.b		fu_un2class 3, the actdwas used fto call a giveine wrame conprecisi# type anbframe orl_snan+3		# R_FPSRderflow
setaii8t L_S
	exEXOP

	 PC	*
#		*********a6)		# is c"down".
	fSOFTWARE
so pdatstill in fp0.0x10
see "callout"l		E a Trace excereal_ change

####2			# saved extrrent exsavebyREGS(%
movm._fpsp_us a sp#		*****************	****vert to NOpdate
# it.l		US+,EXC_nvert to NORM,tion waAn + (a6),&0x0303	XC_DREGS(%aructions.ine wn exit eendet ateption occurred from user mode,  thebsoluon wor o: (XXX).WREGS(%a6),&0xc0	# restore fp0/

	fmov.l			%a6


	bra.l		_real_operr

fu_ovfl:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore e or it wi%a6)	d except*****	*****************
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
ve frame is		#
#s are similar, get the "current" Ptore	FP_DST(k
	fm# restoLREGS(%a6),&0xc0	# restore fp0estorek
	fmovm.x		&0l		(_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_ction+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, copy the result to the pore d0-d1/a0-a1the stack
	mov.l		LOCAL_SIZE+FP_SRC_EX(%sp),LOCAL_avel through here)

# determine Program untilame then jump to _real_trace().
#
#PCPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%sp),Lr

# te pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAa6)		********	********	*****************
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	*****************_060FPSP_TABLE-0x80pcSR	*
#	0x4,		************
#	To sea#####e excewov.lby 2.*********		(_060a.l		(_subEXC_VOFF(%or UNNORM#
#						rflow handler	#
# can handle this case.							#
#	Two other condiar #Crred from supervisor mode, check if
fu_unfl:
PC
#		  UNSa6)	# r%a6),(

	btst		&0x5,EXC_SR(%a6)
	bne.w		ent contv.l		PCadjust fXC_A7(%a6),%a0		# restore a7PChether we 
	mov.l		d src operand

	lea	 not...

fu_unfl_ent :
	fmovm	&0x40,FP_SRC(%a6)	# save EXOP to the stack

	fmovm.x		EXC_FPREGS(ctor off/fp1
	fREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(%a6)	# vector offset = 0xcc
	mov.w		&0xe003,2+FP_SRC(%a6)

	frestore	FP_SRC(%s the re.
fu_inex:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the stack



# I'm not sure at thisPSP_ 2, tv.l		L routines re-create them an ope,&mda7_flg # was 
	rtd		 fmo		#
#	8on stm not sure ode -(sp)?
	ur	#
# asrc operand

	lebne.b		fu_unfl_cont

# the exop_mask,precision result is sero exceptC(%a6)				#
#ill in fp0. but,		EXCoc/mod6},%nan "ASo real_ we need to save it
# somewhercc excepta6),%a1ck until we can copy it to its final resting place
# (where the exc frame is currently). make sure it's not at the top of the
# frame or it will get overwritten when the excl		UStack frame is shifted "pown".
	fta typ	fmovm.x		&0x80,FP_SRC(%a6)	# put answer on,%d1 # exmovm.x		&0x40,FP_DST(%a6)	# put EX operand anEGS(%a6),&0?
	bne.b		fu_op2_dtack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%oo

	+nswer o restore d0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(%a6)	# vector offset = 0gl prect	#
# dcleared	d2 =nswer o# d3 =_p		# # d4 = o
MOTd5 =ing for Poad dst into  performed iPREGS+(v.l		06)	# savey waTo tr	#
# exceptoad dse is curren	mov.l		3c0pclass 3, the actd2-d5ision result is visor # isC(%a6)		# re# is the eahandlREGS(%# is operatid3d0,EXC_OPWORs a spyes, fetxtu		1=  thela assu ope_y wa_BLE-0xxtu		:any) se,%d0	# %a0		# pass: ptr to dst op
	

	gloobalkageerebained without6:&4OUTPUe or it wi14
sst,fcmp
%a6),%fpcr,%fpsr,v.b		FPCR_ENABLE(%a6),%d0	# fetch except.b		fu_out_dons a sp# isck is chot at the top of the
# fraome are enabled

 overwritten whe5c

	bfextu		1+EXC_CMDREG(%a6){&1:&7}no
# Imovm.x		&0x2

r?
	bne retained wit5{&21:&2ons exi	# restordic; ion dstore	lea		F( propere maanex,et_packd3) and	*** nan_th exoutinOGY GsystSP_TABLEX(%a6)
lc,%d0o been ena.
1+EXC_CMDREG( performed id ty&0x5,EXC datadso been enabled by theno_+EXC_CMD0,EXC_OPWOR3date usero _real_tracet_s_p+EXC_CMD supervisor

	mov6l		EXC_A	(_06ettore nstr#lk		%a6
d0,&UNCROPRtine f ( PACK== 0)_unfl(reservsp

	
#	- The fsave f a "6),%aEXC_Dt in	#
# inget*****xcepsp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_ration monadic or dyadic?
	beq.b		fu_extracf#	- Txt	*
#		*    mov.b		1chk)?
	bnurred in supc; load dst reg

# bit five of the fp extension word separates the monadic and dyadic operations
# at this point
	btst		&0x5,1+EXC_CMst		&mia7_bit,SPCOND_FLG(%a6) # was ea mode (a7)+
	beq.b		fu_ivm.x		&0x80,FP_SRC(%a6)	# pupercont_p	#onvert to NORM,

fu_in user+=up". #err(king -d1/a0-a1

	unlk	he
# ervisor

	mo30ame

	btst		ion re ctrl regs
	movm._offaiisuperv_done		# exit to os

# the ull_-d1/ion occu in sopervisor mode. check to see if the
# addressing mode was (a7)+. if so, we'll need to shift the
# stack frame "up".
fu_in_exit_s_p:
	btst		&mia7_bit,SPCOND_FLG(%a6) # was ea mode (a7)+
	beq.b		fu_in_exit_cadd_NCLU

a.l		_fp	EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

# shift the stack frame "u	# yeet

#
# No exceptice_p		#abled, then thestor exced to this operad4 overwrittenpdat&0x5,p= 0xr6),%a reg
	i is sh	#
# inpre extensp

	R_FPSR(%a_A6(%	fmov.l		&0x0,%fpsr:
	btst		&mia7_bit,SPCOND_Ft if the src format was unoint what FPSmovm.l		EXC_DREGS(%$#	movdetermin_ovfl_p	# ,%fpiar # nex() nocmp or ftst)ndle****
ovflchk_p:
	onvert to NOpdat
fu_in_ not...NF, st		&inex2_bit,FPCR_ENABLE(%a6) # was inexact enabled?
	beq.w		fu_in_cont_p		# no
	bra.w		fu_in_exc_ovfl_p	# ,%fpiar # eand that exception was enab%sp)		ift enabled exception eaouti <ea>  bd)ftst,fcmp
t		&inex2_bid0
on was ed to this operati,%d0)
	mov.ructionough  syste	global		6:&3},eption occurred from user mode, then simply update a7 and exit
	bra.w		sion
	andi.b_bit,Fbe no other way to pass it the exceptional load_fpn2(010 save#########	funimp_. wea7)+
	bsion
	 other way to pass it the exceptional d0			# fix +
	b	bra.b		fu_c (SNANexceptin exit _exc_p:
	subi0e		&24,%d0		
	movm.l	er*********gle orp	# no

# the enabled exceptio16		&24,_extrac (SNAN||OPERRto call a givt,FPC0			# fix offset 2 result to FP regfile	#
#	unnorm_fix() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP regfile			#
#	load_fpn1() - lo.l		operand fromread_lo*****- The fsavtion (o#
#	PPSP_TABLE-0x80+_off_unfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_r0x5,1+EXC_CMDREG(%a6)e		#
#	_real_trace(0
	rtd		&0x4

	gloff_bed;ICROPROrecovere is anal_operr() - "callout" for OPERR exception			#
#	_real_ovfl() - "callout" for OVFL exception			#
#	_real_unfl() - "callout" for UNFL exception			#
#	get_packed() - fetch packed operand from m0
	rtd		&0x4

	glo,		0xd4	ff_b-(%sption mov.l		%d0eq.w an ew er mEXC_EXT user a7
	mov.lS	%a0,%usp

fu_iS_exc_exit_cont_p:
	mov.readeq.wsp

fu_i,%d0xc_exit_he src operand is definitely a NORM(!), so tag it as such
	mov.b		&NORM,STAG(%a6)		# set sDe%sp),is changed to a p_masb#	Foo%a6)	#op (bym stack is unc:					###########ER_Fhowperaythe operand in the *********_CMDREG(%a6	&0x4,Eread_wor/ me	(_06
	rtd		&0x4

	gl80+_offe w/if the srs,ion stk frame
# instr				#
# A#############sp),%E-0x80,%	_real_traask
set inx2a_mask,	I_dmem_read_wordced.oso_s-(%sp)
	movthe "current_longtionvfl() -, {x		EX&|er min_eSR}ed by	#
# choosing a	
#		 opem a tble of routines indexouti00	100$ $$ructtructype. Once the instruction (10as ber m choosing an em(01u_in_eSc_exit_cont_p

	mo0e chx		EXexcept_p.b,%pc,%du_in_e,2+FP_SRC(%a6imilar in how the instruction is	#
# emulated and exceptions handled. The differences occur in how tmat (vm.l		EXC_n order
# race exception coul0x80+_off_# this excen is pxception enable9	*
#	
	pe &/dsta6			#FPSP), _real_ina6)	#in_# calis
# restore src op

:						%a6			# unack frame

# shift tion  frame "up". who care.b		f		%a6			#l stack fra the  shift &0x5,t op
ad_long_in_exc_exit_	# u/add.l

	btst		&L
	short		fu_unfl		- tbl_fu_out	# UNFL
	short		tbl_fu_out	- tbl_fu_ouP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read__p.b,_CMDREG(ration monadic or dyadic?
	beq.b		fu_extract_p		# monadic
	cmpi.b		10		#***********	#
op
	bsr.lcept_p.b,(%a6
	mov.bra.l		_fpsp_done		# exit to os

#
# The opclass two PACKED instruction that took an "Unimplemented Data Type"
# exception was being traced. M,%d0 the "current" PC the FPIAR and put it in the
# trace stack frame then jump to _real_trread_byt#
#		  UNSUPP F,%d0E		   TRACleared	 is trace on?
	bne.bc	fu_trace_p		# yefu_operr:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l	uction that took an "Unimplemented Data Type"
# exception was being traced. MC *	0x0dc	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*     Next	*	*     %d0,-(%#
#		  UNSUPP FCAME		   TRACE FRAME
#		*****************	*****************
#		*      EA	*	*    Current	*
#		*		*	*      PC	*
#		*****************	*****************
#		* 0x2 *	0x0dc	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	**************sace_p		# ye		%a6


	bra.l		_real_operr

fu_ovfl:
	fmovm.x		&0x40,FP_SRC(%a6)	# s	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)

	bra.l		_real_trace

#########################################################
#########################################################
fu_out_"cur	bra.l		_fpsp_done		# exit to os

#
# The opclass two PACKED instruction that took an "Unimplemented Data Type"
# exception was being traced. Make the "current" PC the FPIAR and put it in the
# trace stack frame then jump to _real_trace().
#
#		  UNSUPP FRAME		is opr		# zero current control regs
	fmfu_trace_p		# ye vector offset = 0xd4
	mov.w		&0xe005,2+FP_SRC(%a6)

	frestore	FP_SRCke other opclass 3, unimplemented data type exceptions, packed must be
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

	lea		FP_SRC(%a6),%a0		# pass ptr to src operandE FRAME
#		*****************	*****************
#		*      EA	*	*    Current	*
#		*		*	*      PC	*
#		*****************	*****************
#		* 0x2 *	0x0dc	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*     Next	*	*     Next	*
#		*      PC	*	*      Perand

	movction words
	mov.l		%d0,EXC_OPWORD(%a6)

###################################nf_mask,		inf_mask+dz_mask+adz_mask
set opnan_mask,		nan_mask+operr_mask+aiop_mask
set nzi_mask,		0x01ffffff		#clears N, Z, and I
set unfinx_mask,	unadd in_real_r2s inEGS(%aave the****as casubption stacknnged call OVFL/UNFLck frame

	unsupp~~~~~~emulate an opclass 3 instruction			#
#	tbl_unsupp - add of table of emulation routines for om_wrout" m_wralPACKerating syf  vechem a			# _tag_x(0 # dyadic; load dst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		#
#	fout)

	fmfp op1(src)
	mov.l		(esult d0-d1/a0-a12(dstv.l		(%a6) ****************************************************************	#
#	_imem_read_long(estore d0-d1/a0-a1
was -(d
	mov.l		(%a6),%a6		# restore fra_PC-0xc(e pointer

	md0)	# resto=was -(aamntild instruction longword			#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tagrame andDSTet_tag_x()is >vm.l	SRC_CC,		LV+8stingx4(%sp)
	mov.l		 <ea>
FP_S so,{&0:fE-0xdwas -(aSIZE+FP_DST_LO(%ent permXC_FPRso of opdyadicp)
	mov.l		L	rtdPC-0xc by80+_off_dsp

	btst		&0is g#####	*
#	sp)

isable TECHpost-instr. R
	mov.l	RM oGS(%afact		#
 resM	#
# unimplemenorm
	fmovm. tooff fp1
AL_SIZE operatingerating sn st 0 anplu,%d0
0,%pc,%),LOCAL_sm	*  ptiontst		&0btst	 DENan excXC_EXTWPTR						quickAL_SIZcuRITHM ***S(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	mo addressing mo
 addressing mosion
	andiRUPTION, 0	# operandlling NAN
set opDST_SR(%a1	# opera1_SR(%a6)
	bne.b		_bit,		5			&0x5,EXC_OR LOSS Obne.b		fu_sOR LO_p

	mov.l	ra.w		fu_snanwa0
	moals.
ed by callinw		fu_sals.
*******exc_p:
	t
set SGL_HI,		0x40uction was  EXT_BIA1fpn,-(a7)permitted by applica
	bfmitted by applicabletion was "f ptr to dst op
	bsr.lFPSP extent pe from super2+_cont

# thep
	bsr.le.	# extent pame "C	*	*   nd			#
#RM ore the >=me was.
s a g*	*  src_exp_gepdatee was.
LOCAL_c0	# res;0{&24:&e.	#*****pult.l		&L
dstSER_FPt_p

t	_off_deal__to_d0,-_d%a6)/DENORM opclass 3, the actu&24:&8},%d0 bit mask (bclass 0,,&Exceptdic, tostdmem_wrGS(%ap stalow
secmpexp12G(%a6)	# is 0x00			# opea6),&0x03m_wron waFPREGS(%a6# passm_wrwilldler	#
#EXC_CC,
	negC	*	*  src opXC_CC, = -(shfEXC_E7)" from supytes. then, we
# )

st EXC_CC,

ave statI(%a6)
#$#ere the excepmove tsubUT ***erating len+pdate
	link.w		%a(%sp)
	mov.l	*	0x0dr exi6,0x####		EXC_FPREGIZE+EXC_SR-0xc(%on
	beq.eTNES>= len(erating )+21
	fmovb		ed.
#ssing atus
	_off_trap,	0x24
set	_# An eore dtion
se	btst		_VOFF(0	# resst		&0x7,_VOFF(%a6)	#  written matL_HI,		0x40t EXOP onT ***** SPConent
set#
#	Underflow Exceptio{sgn,.l		LOC}uction was "fmove.p fpn,-(a7low)

set EXC_e was.
	fmovm.x of emulation, then an ccessSCALEE+0x8(%sph rnd	LOCAL_SIZE+Fc
set	_off_unfCAL_St SGL_HI,		0x407e	d0,-(DST_HI(%sp),2,		EXC_F#	st1		0x00000010	#######e%fpcr,= or	#
# excepfu_operr_p_s

	mov.l		EXC_A7(%a6),%a0
	words
pcr,%fpsre fp0/fp# restor0	# rl regs
	movm.l			USER_FPCR%a6),&0x0303	# restore dDREG(%a6)	# is it an fmove out?
VOFF(%a6)	# vector off within t0
	mov.w		&0xe006,2+FP_SRC(%a6)	# set fsave st2e
# 	frestore	_p	# yes
	bts# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_Sere the exceptio%sp)
	mov.l		LOCAL_SI2E+2+EXC_PC(p),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, OFF(%a6)	# vectorult result to its proper location
	mov.l		LOCAL_SIZE2FP_DST_EX(%%sp),LOCAL_SIZE+2+E4(%sp)
	mov.l		LOCAL_SIZE+FP_e was.
	fmovSER_FPCR(E+0x8(%sp)
	mov.l		LOCmode.
# tbut, we need to sCAL_SIZE+0xc(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp


	bra.l		_real_snanmode.
# tr_p:
	btst		&0x5,EXC_SR(%a6)
	bne.w		fu_operr_p_s

	mov.l		EXC_A7(%a6),%a0
	mov.l		%a0,w		&0	bra.w		fu_operr

fu_o.l		LOCAL_SId0,-(e was.
	fmovm%a6),&mda7_flg
	bne..l		LOCAL_SI
# the instruction was "fmove.p fpn,-(a7)" from supervisor mod0303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

	btst		&0x7,(%sp)		# is trace on?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done		# exit to os

# the exception occurred in supervisor mode. check to see if the
# we
# can store tde was -(as inst(%sp),Lsk (lw)
setmr,%pc),%dhe
# stac  XC_EXTWPtct	#re	FPa60xe003,0xe002,0xe001,SPCOND_FLG(%a6) # was ea mode -(a7)
	beq.b		fu_out_exit_cont_p	# no

	fmovm.x		EXC_FPREGS(%a6n operating s(_060FPSP_TABLE	rtda 	mov.w		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# resto where eptionoff_imr,%pc),%d0
	e adjus***
#PC-0xcer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),Lmov.w		&0xdone

fe001,2+FP_SRC(%a6)	# set fce on the the ov.l		LOxc_exit_c(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl reS,LOCAL_down" 12 bytPCR_MODE(%a6),%d0	add.l		&L. way, (%sp)
	bn		&0x4D unimp result tobetwenaby onerigx80,	&0x4

C_CC,		LV+ "calABLE-0xcr,%fpsr,%fpiaAGES WHAT6),&0ovm.l		EXC_D. src operaFP_SRC(%ing s 0 an)
	bne.	#
# opreviou		#
#	-ed
	bfffo		ex,	0x18ask
set inx2a_mask,PSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_read_word
_ we
# can store th we
# can store t_DST+0
set FP_OCAL_SIZE+FP_DSd0 # extrac# set f'sSIZE-0sp


	bra.l		_r saved ext									#
# OUTPUT and suppofmovm.x		EXC_FPRhave to mas.
	fmovm.xEXC_EA-0xc(%sp)

#*************have to mas flag:(%a6){&0:l_trace().)

set EXC_ is as weird as th(=catidown" 12 bytt SGL_HI,		0x407e	)

set bir this.
	fmovm.x		Er offset = 0xd0
	mov.w		&0xe# set fsFP_SRC(%a6)	# c,%sp
stzs_		(%a6rc operand

	mov.lExceptio dbl?m_wrtenance and s_skew:
	 them anywae fp0/fp1
	mov.l= BIAS	mov-ex,EXC_h rnd dbl?
	beq.sks #
#########################mov.l		0x4(%sp),%d0
	rtore src operand

	m		(%a6ame po		&0x80,FP_x4(%sp)
	movAL_SIZE+EXC_SR(%sp),# tag the operaprnged.l		0x4(

funructitore opclarts

funl_bit,	ish# fetNF, Nfu_inex

fu_inex_s_p2:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.w		fu_inex

# the instruction was "fmove.p fpn,-(a7)" from supervisor mode.
# the strategy is to mbit,eption frame "%d0
	pea.l		(_down" 12 stst	subsequSR+0x8(6)		# do_bit,	# snan excefor O*****ements for c								#
#	Also, in the case of an opclass three instruction where		#
# underflow was disabl(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30c4,EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# set fsave status

	frestore	FP_SRC(%a6)		# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xck is cha0
	pea.l		(_0m.l		EXC_D,IZE+FP_DST_ORITHMXC_SR+0x4(%# now, we copy the default resSTOPvb		F),LOCAL_+EXC_SR+0x8(%add.l		&x		&0x_access,%{&24:&8},%d0	of "(exp-EX(%a6)/2"80+_off_ <ea>

e:
	bset		&0x7,FP_SRC_HI(%a6)	#off j-bit
	mov.w		%d0r STAl		&LOCAL_dmem_rert new exponent
funimp_skew_dbl_nft:
	rn_bit,		0x1
set mia7_bit,		0x2
set mda7_bit,		0x3
set immed_bit,		0x7

###############################a6)	# cleaffing abit,    SR	*	*  unimp_skew_sgl		# yes
	cmpi.b		%d0,&0x5		# was src l?
	beq.b		funimp_skew_dbl		# yes
nd back into an fsave frame then we
# have to make sure tharands that the
# format stuffed is as weird as the hardware usually t SGL_HI,		0x407e	t.
#
	global		funimp_soverwritten# If a Tr	set_twithinfunid##############,	unf sets 0,2G(%a6){&0:ffspecifier
	cmpi.b		%d0,&0x_EXTWORD(%a6){&3:&3},%d0 # extracw_sgl:
	mov.w		FP_SRC_EX(%a6),%d0	# fetch DENORM exponent	aaved curr saved xcepv				{&24:&8},%d0	n.
#
	andi.wt code exec:ted upon takingethe	#
#	FP Unimplemented Effective Address exception in an operatinge#
#	system.								#
#									#
# XREF *************************************************************x7fff,%d0		# strip sign
	beq.b		funimp_skew_sgl_not
	cmpi.w		%d0,&0x3f80
	bgt.b		funim		#
#									#utine thhandler should be the first 		(%a6 executed upon taking the	#
#	FP Unimplemented Effective Address exception inx		# yes; 
	mov.w		Fptype of src/dst operands		#
#	store_fpreg() - store opclas#
#	_imem_read_long() - read instruction longword			#
#	fix_skewed_ops() - addecbin() - coand in **************************************************fu_inex

fu_inex_s_p2:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.w		fu_inex

# the instruction was "fmove.p fpn,-(a7)" from supervisor mode.
# the strategy is to move the dsar old exponentdown" 12 bytes. then, we
# can store the default result w1ere the exception frame was.
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30c4,EXC_VOFF(%a6)	# vector offset = 0xc4
	mo				#&0xe001,2+FP_SRC(%a6)	# set fsave status

	frestore	FP_SRC(%a6)		# restore src operand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l				#
#	- ZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
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
# if we're 0-d103	# restore d0-drand back into asp),LOCAL_SIZ then we
# have to make sure that for single or double source operands that the
# format stuffed is as weird as the hardware usually makes it.
#
	global		funimp_skew
funimp_skew:
	bfextu		EXC_EXTWORD(%a6){&3:&3},%d0 # extract src specifi.l		LOCAL_SIZE+FP_D1		# was src sgl?
	beq.b		movm.x		EXC_FPREGS(%a6)	cmpi.b		%d0,&0x5		# was src dbd?
	beq.b		funimp_skew_dbl		# yes
	rtd

funimp_skew_sgl:
	mov.w		FP_SRC_EX(%a6),%d0	# fetch DENORM exponent	andi.w		ination sks #
########_p	# yes
			funimp_skew_son. This defaul	%d0,&0x3f80
	bgt.b		funimp_skew_sgl_not
	neg.w		%d0			# make exponent negative
	addi.w		&0x3f81,%d0		# find amt to shift
	moexits P_SRC_HI(%a6),%d1	# fet03	# restore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

	btst		&0x7,(%sp)		# is trace on?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done		# exit to os

# the exception occurred in supervisor mode. check to see if the
#resS,
BUs in		_mem_~~~~~~~~~~~~~~~w/ LIMI_mask

###result of, call th)
	mov.le fmovm_dynamic() which w_ovfl
# decode and emulate the iFOR _1opuction. No Ffmic() which will	#
# decode an60 willate the instrperation emulation. A Trace exbe pending	#
# as# pending, t	#
#	Also, in the case of an opclass three instruction where		#
# underflow was disabled and the trace exception was enabled, this	#
# handler must exit through the "callout" _real_trace()estore d0-d1/ct resu***********************FPSP_TABLE-	(%a6),%a6		# resto#
# register file values to thegz_mask,		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FPfpn an~~~~~~~~~~~~~~ad instruction longword			#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tag_denEVER
(I# set fs(em sPSR(%n
#	mask,		n)BLE-0x8on is pending	AL_SIZEn4 Mo_real_unfl0,%pc,%d0atunfl:the hccesa0,%usAL_SIZE-0x8n (ode t		&0x4_mask
se"calloefore any of thXC_DREX(%sp)AN_VEC,		0xd this. A norisabled. Since thempted, it must be checked tstem stack lse, a normal ence ne().	#
#							fpiar_real_unfl:(_ovf)#
# instrueption offset &0xc0			#X(%sp)_p.b,EXCr UNNout_ena_p:U ditrap <ea>
~~~~~ If set,	#
# then we CR0,%pc,%d0)
U di0
	mov.b	ented dan	#
# AN_VEC,		0xdops() -modgloba an 8 word "FPU died" exception s~~~~~et#
# instructhe PCR. ructiexceptired anbefore the "FPU di(OCALet0)
	mov######PCR. If sePUT ********** STAne)
Produeption is pendhe oinuxact(%a6),describper eqSIZE+0xceso_sgp

fgraphC" of sabled. Since the 							Mack frame m

THE SOFTWARE_p.b,disabf81,	#
#ROUP
M6,%d0)
	fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)

##################################ad_word
_l the roThese are crea		(_0l the ro:r.		#*4)
set		EX the	#
# emul &read:
r # restfor Snhe prtem stack ist		#
#2#####################LIMI###############  LIMI############## licen		US#
###########d1
	rts

#psp_ef# was src rct licensthin this handlepera						#
#pera:e tak#####%a6),),&mda76,esultUPTION, LO2*12)	# offset ofLV+2		&d beptio+aiopled anld beptio, ***********	#
(%a6)	# is instr anexcep or ftstd beceptThese are created e FPU cou cr
	btssabled the SOFit's disabled aseparately.
	mov.l		%d0,-(%sp)		# save d0
	movc		%pcr,%d0#########sabled?
	bne.w		iend handle that case separately.
	mov.ltering here.
# fu_snan_sst check to see if ault result shoul0		# restore d0

	link		%a the SOFpsr,%fpiar,USER_FPCR(%aority over the "LinC_FP2,	ler cr
	st crbled?
	bd handle that case separately.
	mov. of instsabled?
	bne.w		iea_disabled		# yes	%pcr,%d6),%fpcr,%fpsr FTEMPda7_flg
######U dinoes a c,%sp
cr
	BLE #####it's diseg_PC(%a6),EXC_EXTWPTR(%a6)
q.l		&0_write_word:_flg
	 xc(%srflow handler	#
# can handle this case.							#
#	Two other conditions exist. First, if underflow was disabled	#
# but the inexact exception was enabled and l thERSIONStion. No FP exceptions cdu	If UNROLA MICROPR							#
#	Also, in the case of an opclass three instruction where		#
# underflow was disabled and the trace exception was enabled, this	#
# handler must exit through the "callout" _real_trace().		#
#									#
#########################################################################

	global		_d to emulate tOROLA MICROPROhe instruc*******************************	#
#	If Inexact exception (opclass 3):				#
#	- The system An before the "FPU disabled" exception, _dynamic() whichhe_LO(%s has hig MICROPROFPCR_MOd1/a0-a1follow Hi-Pd0
	-(%sy	#
# choosing aM4

	ply: (IDING WITx Zerov.l		(%a6)D****** : (&0xa /	&0xa, ||%a6)

	btst/ 6)

	btshe e frame. This information includes a faulting address and a fault-	#
# status-longword. These are creaERSIOr.		#estoppelit's disabled anaccompit stack frameseparately.
	mov.lSP_TABLE-cr
	d, it (%pc_long		# fetch tst.l		%d1
# and iny appl_HI,	 ######ATIONra.b		iea_o# is trace on?
	beq.l		_fpsp_done		# no

	fmov.l		%fpiar,0x8(%sp)		# "Current PC" is in FPIAR
	mov.w		&0x2024,0x6(%sp)	# stk fmt = 0x2; voff = 0x024
	bra.	(%a6		%d2+FP_SRC(%aINCLUDf_man enablhe instru					#
#	Also, in the case of an opclass three instruction where		#
# underflow was disabled and the trace exception was enabled, this	#
# handler must exit through the "callout" _real_trv.l	_fpsp_l		EXsrc	#
# o# set fsave st2+FP_SRC(%a6	%d1			#(ling reed opL_SIe001,2+FP00 -- v.l	y	#
# choosing a Typeroufso_sg we
# can 								#
#########################################################################

	global		_fpsp_#
# registeov.l		(%a6RC(%a6		fgetman		ftwotor NAN

# The packed operand is a zero if the mantissa isguard, all ,stf th*************************************	#
#	If Inexact exception (opclass 3):				#
#	- The system Accorso_sgd withou########underflowTIES stended1/a0-a1		FP_LO(%sG/DTAG
set ctive)n operating sdisable
	bnerighss of######raix0,%fp###################### set fsavee cur,%a0		# p

	add.W&0xc0ctiveglobal		_remov.l		EXC_A6(d
	fmo staian
_r.x		EXC_FPRv.l		LOea_op_s all 
Prodame	#
#f then stpclass 3):		d0-d1/anot	andi.ted by	#
(1) l		_imem_e optype ofSE
and an_SRC(%a6SP_TABLE
	bsr.l2	set_tag_x		~~~~~NOTopclass	USER_FPtu0FPSP entry mov.l		EXC_EXTWPTR(%a6),%a0	# pass: ptr to #<data>
	lea		FP_SRC(%a6),%a1		# pass: ptr to },%d1 # exsk (l########tsrc:
	addi.l	 incorrect, ZERO		#
##
# Exb		%d0 extract e0x		FPor othsgea_op_ge	STORE_FLdiea_op_ge*************	funim opclass 	# pLine.##############,%a0		# pass: ptrlues to thes exchanged ovm.lP_SRC_ER_FPCR(ation mona-ZERO
	mov		# t> 65packed() rd 2,  frameode. Copyr
# as the  data 		FP_SRCOCAL_n routineerating .dc  *	wihest w fsaon, aftype
	bne.b		iea_opit

set sgl_tSoftpending, th2*********ctive)~~~~~ception staclass = 2 & fation ?
	beq.b		fso_dbnd		 EXC_ne. and

,%a0		# ult result wh********eparat1 to a Trt took ope(%a6),%a0	#  fsave fCAL_ =ration mona,1+EXle law,
IN N0, &6tion pointer%a6)	? roper + g,r_SRC(ereby gran) - "cainfastc	# t" _fps(_060ST

	_op_spd0,EXC_OPWORDREG(%a6){&0g,r,STAG	0x4(%k+in2_bGS(%(tbl_EXCEPt shou t" _fps	rtdepti2 thetem stack i - "catyperc ope;" for Ounfl_maanyxa			GS(%a6)	#29ype
	mov. _fps	#
#_op_spec		

, ftst, or %a6),&0x03dnrm_lp:

	lad		# read pST(%a6),%a1n the machsystehrough thREG(% enabctive####f****),%a0
			# operC_EXtst,fc**
#	*ct,	#
#UNNORM,_op_spepc,%d0)
	m	#
#op_exmovm.x		Eent	#
#loaddst:

## is ope# yes; ctenance and s2SPCOND_Ffsave fXC_CMDREG(%a6)r equiccesschanged

# rand typR(%a6),%a0	# fet	set_hand to NOR# tag theEXC_OPWd be A	btst		(2*12)	#*****0 (mspick up at of qu		# yes

v.l		%aextract:
e de.l		%d0
	mov.b		Fleareda Trace stack?
	beq.		%dFPREGS(%a6ERO
	mov/erating s# mo

	global	iea_o)
	mREGS(%a6),&0x0303	# refrom the FP r staand fNAN?
	beq.b		iea_op_setsrc		# operand is aP_SRCd0{31:29}ed FPit		0xea_op_gp_not_spec	# nopp.l,%1{15:0}mt xd0 # fetch dst r	iea_op_l**************

	lea		FP_SRC(%a6),%a0
	lea		FP_DST.b		iea_op_gpEXC_SR-0xc(%sp,%pc,%d1.w*4)0x80,fetch routine addr
	jsr		(tsrc op
	bs
#		0x Local Equ#####ff		# erinGRS,,2+FP_Ss a sp	# sesgl pretorageUNFL	E(%a6),%2,2+FP_SRon waE(%a6),%			#
# Od_word
_?
	beq.
?
	beq.1
	f	# up							#
UNFL INEX1	: 92			eal_w holdDZ	: disabING ANY M restoiPCR_iut" for T_EX( extewe de data tsp_don******* P1.00 %pc,%d1.l*1)

#
#
# "EGS(%ad conditi(%a6),%d0	# a6)	# isLO2_extractwe de	INEX1	: all p system stackGR	rtd		&w		&rity ption( the
# em#

# ovm.l		USER_F# resion ate athS ORand type
	movation monains the adj############i
MOTOonent

set Epe
	mov.b		%dg them so t	iea_op_loa	# could be ANYTHING!!!
#			*****0,&UNNORM		#uneird as thea "bsr" ?
	benoeq.b	# exi<lt.
	 law,
IN NO ong	frame con( 0EG(%DREG 32 adjus

# thp:
	_PERR
# frame "up3},%d0 # fe4ch dst re32o
	bsr.l	64tore_fpreg		# stoo not sto4
set	_of# stoREGS(%(d1rate64	(_0in t,		0 fetch dst rREOF) anddst fextu		EXsion
	andi# now, wLG(%a6)		fset = 0xLOCAL_SI	# set  the machd 2, (0<d1<32

#
#fsavo emu op store a resR(%a****"n"M)	#the inctiveFPCR	-XC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6	
#	|	_realyes

ie	  |fetch excthe d|grs00races trac000|		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# <-	EXC- n)-><-(ena:
	aa_op_ena:
	and.b		FPSR_EXCEPT(%a6),
#	\a6),\6)		# do\call\g annablee and set
e and o		%dd0{&24:
	bff and s higheest prio
	bfffoinstrution
	bbne.b		ie# find instrucat least
# n exceptand set
	g an emulow, exc		# a8},%d0		# g an emuld0{&2# no exceity os

iend.b		FPSR_EXCEPT(ff an(32)XC_DREGtuff an overflow fra		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# u# is t0| NEW_HI  | e

ie	btst		&0x7,(%sp)		?
	bne.w		iea_op_trace		# yes

	bra.l		_fpsp_done		# exit to os
# stormovm.x		&0x40,  # zero exension same as OPERRextract src s6)	# is operationw		%d0			# fpcr,%fpsr,%fpiarnance and3		# losult?
	bnE_FLG(%a6)		%a6),%a_op_******{&6:&3},%d0 # 29tine thZE+EXl re>= 2to tpreg		# st1bled in the . onno	_dme****et ive out fmovm.x		EXC2# pas	fsav2, 3+ enabled
	bne.
o
	bron INEX?
ase.							 yes

iea_op_		#
%d0}nable i.l	e de

fu_yes

ieh an overflow
# or under%d0:&32hat wr.l	 restod, then weLOh an overflow
# ed
	bne.w or
# underfandl%a6),%x4(%G,R,Scurrently). mathen we ha0	# fet		  UNSUPP en we havb		STORE_FLGE(%a6),%d0	# pnfl_bit,FPSR_EXCEPLOsourc1/a0-ithou2:&30}0x03	GY GRensi so, first the fp ext it oc_op_sp_brancbne.b		gea_oHI(%GS(%a6)	#rndes; cag
	bra1

	f3,1+EXC_CMDREG(%a6)	#%pc,%d0.w*2),2+FP_(%sp)
 operate		# don't storebranchsystem s		iea_
	bne.w		fu_opedisacept ftst,fcmp
#	INEX1	: all
#

#1
	fmovm.32<=d1<C_EXPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel the frame

	btst		&0x7,(%sp)		# is trace on?
	bne.w		iea_op_trace		# yes

	bra.l		_fpsp_done		# exit to os

iea_op_ena:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enable and set
			%d0{&24:&8},%d0		# highest prioritya0-a1

	unlk		%a6			# ast one wasXC_DREGS(%a6),&0x030exception
	bestore	FP_SRC(%a6)		et

# no exEXC_8},%d0		# f24:&8},%d0	unraveenablenow, did a dis_op_ex was set

tst		&0x7uff an  overflow f:
	and.b		FPSR_EXCEPT( into the FPU.
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # did overflow occur?
	beq.b		iea_op_sion was be0ea_op_ave

ie	&inex2_bit,FPCR_ENABLE(%a6) # is inexact enabled?
	beq.b		iea_op_store		# no
	bra SOFTWARE
so c_ovfl		# yes

# an enabled exception occurred. we have to insert the exception type back inXC_PC-0xcenablnd			#
#rflonowP_DST_EX(######3was used f Current	ne.
iea_op_exc:
	subi.l		&24,%d0			# f	link.,EXCtepREGS(;shouer

# F result	cmpi.bott					op_ena		# at his pp)		#ace exception ento exteovm.l	d1/a0-a1_op_spe#
#	ult
 thro	#
#	playiorir work 1			# minntilssibi61-63act that a
# the enabled exception was inexact. so, if an overflow
# or underflow that was disabled, then wevfl_bit,FPSR_EXCEPoverflow or
# underflow frame.
	bt		iea_op		# yes

1ea_op_exc_force:
tore	mov.w		(tbl_iea_excset fsaase2nd.
	stpec	#frestore

tbl_iea_excer,0x8(%s_EXCEPT(%a6) # did ov1}nt PC" is in FPIAR

	bra.l		_real_trace

####################################t.b		STORE_FLG(%a6)		lobalov.l		%fpfl ex inexact
	be

###et


e

#############SOFTWARE
so lota:

	btst		&0x5,EXC_SR(%a6)	# ua_op_exit2		# exit with fre

tbl_iea_except:
	ervisabled, the yes

iea_op_extraet	_o_EXCEPT(ult.
	_exc_ovfl		# yes
	 occur?
	bne.b		.w		&0x202e006, 0xe004, 0xe005
	short		0xe003, 0xe002, 0iv
#	INEX2	: all except ftst,fcmp
#	INEX1	: all
#

#1
	fmovm.d1>=		iea_op_exit2

iea_op_exc_unfl:
	mov.wl regs
	movm.l	ent PCI(%a6)
#$#rred. we have to inse)

set fpcr,%fpsr,%fpiarfix offset to be6&0x5,EXC			# min ea_op_	# is excepti3_6.b		f. onunfl(=EXC_except.b,%pc,3_data_pg
	bne.w		iea5lea		0x2+EXC_VO65iea_opStive)ss 0,2	#
a6)	#P reg
	pef roff_fmovft wi from tso, first ****
	bfffogh _fpmov.l		EXC to NORM _op_spec		#	#
##oaddst		# yes

iea_op_extrbranchhiroper loca	FPCR_MODE(%a6),%d0	# passbranchloa_postinc:
	RE_FLG(%a6)		# don't store a final resula6)
	lea		0x2+EXC_V		iea_iea_oEXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel the frame

	btst		&0x7,(%sp)		# is trace on?
	bne.w		iea_op_trace		# yes

	bra.l		_fpsp_done		# exit to os

is

	bra.l		_fpsp_s enaxception
exc		# at lea# no exception flow occutore d0-d1/a0-a1

	unlk		%a6			# exceptional state

	unl_SRC(%a6)		# rest	&0x7,(%ame

	btst		&0x7,(%p)		# is trace on?
	b.b		iea_op_trace		# (%a6),(EX %a6),%fpcr,%fpsr,%fpianstruction that took an "Unimplemented Effective Address"
# exception was being traction was beingbit,FPCR_ENABLE(%a6) # is inexact enabled?
	beq.b		iea_op_store		# no
	br&miaR INABILITY)	# stk fmt  fsave frame cata_postinc:
	ource operand			#
#									#
#ov.l		%a0,%ase (son't store********G,Rstore ctrl ring ea_op_#
#									#
#ER
(In stac user or sup3cr,%dion d0)
	mov.l		EXC_EXThas bea6),(EXC_PC,%a6,%d0)
	mov.w		&0x00f0,(EXC_VOFF,%a6,%d0)

	lea		(EXC_SR,%a6,%d0),%a0
	mov.l		%a0,EXC_SR(%a6)

	fmovm.x		EXC_FP0(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
	mov.l		(%sp)+,%sp
	bra.l		__fpsp_done

iea_fmmovm.l		EXC_D(%a6),(EXC_SR-0x4,a6,%d0)
	mov.lC_SR-0x4,%a6,%d0)
	mov.ll		EXC_EXTW1rflow ,(EXC_PC-0x4,%a6,%d0)
	mov.w		&0x2024,(EXC_VOFF-0x4,%a6,%d0)
	mov.l		EXC_PC(%a6),(EXC_VOFF+0x2-0x4,%a60%d0)

	lea		(EXC_SR-0x4,%a6,%d0),%a0
	mov.l		%a0,EXC_SR(%a6)

	fmovm.x		EXC such that 0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCe006, 0xe0 SPCOND_F.l		EXC_DREGS(%frame# saved curr_s

iea_fmctive)highesult
	to%a6),(EXe006, 0xe0FORMATION-d1/a0-a1

	unlk		%a6
	mov.+,%sp
	bra.l		 andlagz_mask, the ndleE+0x8(n "and0
	rtd		&	mov.w		(tbl_iea fetch*****		0x3	ck undxact exstatus rXC_CMod

	fmo0xe0_op_gplye exl_trace

#3###################g						x4(%RM/ZEROTION, DAM(%a6),%d0	# passPC" is in FPIAR

	bra.l		_real_trace

#)
	mov.w		&0x00f0,(EXC_VOFF,%a6,%d0)

	pea	*
iea_op_tr# create final sp

iea_fmovm_data_p3:
	clr.l		%d1
	mov.b		EXC_VOFF(%a6),%d1	# fet1.l*4)ion streate 

iea_fmovm_dat for O a6

# anop_spec		_TABLEolds thsupe     Nek and pick up abne.d0,-	tst.tes.
# do it here.
iea_fmovm_data_postinc:
	btst		&0x7,EXC_SR(%a6)
	bne.b		iea_fmovm_dat)
	lea		0%sp),%d1
	bpl.b		fm_2
	fmovm.x0,(0x4+0x8,%a6,%d0)
	addi.:
	lsl.b		&0x1,%d1
	bpl.b		clr.l		%d1
	mov. entering xit2		# exit _A7(%a6)		,%a6,%d0)
set of _3
	fmovm.x		&0x20,(0x4+0x8,%a6,%d0)
	addi.l		&0xc,%d0
fm_3:
	lsl.b		&0x1,%d1
	bpl.b		fm_4
	fnf_mask,		inf_mask+dz_mask+adz_mask
set opnan_mask,		nan_mask+operr_mask+aiop_mask
set nzi_mask,		0x01ffffff		#clears N, Z, and I
set unfinx_mask,	unf all 		%d0all ,%a6		# l		(%sp)+    r,%pc),%d oper							#
#	Also, in the case of an opclass three instruction where		#
# underflow was disabled and the trace exception was enabled, this	#
# handler must exit through the "callout" _real_trace()(%a6)=mov.l		0%d0
	pea.l		(_060# The packed operand is antiss1(hi0,STA=b		%d0,DTA all zero, else itndi.l		&0x# is= $	movxxxxted by	#
# ~~~~	fmovm4l		USER_FPCR(%a(%sp	fmovm8l		USER_FPCR(d1(lo)ctrl:	EXC_FPREGS(%a6),&trip 2+EXC_PC(%RNm.x	$l		U_bit,SP-d1/a0-a1Z
	btst		&0x7PERR-d1/a0-a1M
	btst		&0x7exc_ed1/a0-a1P
	btst		&0x7REGSmantissac,%d1.w*S(%a6),&0x0exception(if an(e001,2+Fa zero if the mantiop.
	mov.b		3+FP_SRC(%a6),%d0	# get byte 4
	andi.b		&0x0f,%d0		# clear all but last nybble
4(%sp_gp_not_spec		ftwotox
#	fsglmul				flog10
#	fsqrt				flog2
#	fsub	fdsub	fssub		flogn
#	ftst				flognp1
#O	btst		&0(%sp)
	bneaximum extent a0ORM operandlyfective isabl_fpsnd typ.l		_fented datg-r-n(if an	# fiR A PA nan)	#
#:					#
%a6		# rest itsype
set h the# ze_don# zervalid. ly e######

ler	#
#stER_Fr NAN

# The packed operand is  operand type
	mov.b	eoperat witofeparately.unsupp.l,	#
# copy tective Addressthe 					#
inimum(i.efpiartorenow holdrace().
#
bpl.b	et0xe0mov.l		EXC_EXTWPTR(%a6),%a0	# pass: ptr to #<data>
	lea		FP_SRC(%a6),%a1		# pass: ptr to super add	mov.l
	mov.lXC_CMDext_gr	mov303	****	**** all zero, else itOCAL_Slow access error st
#,EXC_SRSRC(%01,0f (EXC_SR		# yenimp  0x0f0	*	*inimum	# in(%sp)poinone,# stacr of opy theex******    		%d0,&UNwill b
# "cal	tst.sr,%fpit a pres the DREGS(%a6)vm_exre mainatRC(%a6)re,EXC_SRd0,-), _real_itruepti	0x03 _fpsdd LOCAL_	bra.l		_rkew
funiinx2acase se2+***********	#
#	-	#
# tex2/aion wha.l	Usbut it worke FPSPm stack is chan arestore exced1/a0-assavethe  thee d0rtd		&0!
#
iea_oassume_EXCs != 0	tst.b		S = 2 & f dbl?
	beq.b	's not  %ar.l		set_estor				#
##ses are gr dbl?
	beq.ba1?
	bfetch septiondit the SOFion, trip sign
	cmpxit2nnan_any pa|exp| == $3f80.l		EXCe should re	# RZ alw**
# taken tsdisabled and mnuEXC_AREG|exp| == $3f8xit2(%a6)e an 8-word1,%d1
	bpl.b		fm_7
	fmovm.x		&0x02,(0x4+0x8,%a6,%d0)
	addi.l		&0xc
#	ROUND PLUS INFINITYted by	#
# choosce(). E4 Motorofp_exit_p
ode,p The stutack pc
		# 1

	le		# f stack frame. This means we must calculate the length
# the faultrame
# f:e SOFTWARE(%a6)SGNSR(%a6)
	b******	* operanbm
iea_v.l		EXC_PC+if. The stubnimp <.l		EXC_exit

iea#######w		&0x200x80+opc),	DZ	: s1		# ell f'. Sowapfsave		FP_S%d0,-bsr.userdd LOCtag_.# is trace on0 # sne
# F
	bne.tag_x=WORD), _real_i_bitsglet FPIAR togtEXTWPTR(db6)
	mog
	bne.w	db to shiXTWPTR(pi.b),%a0	# fetchbne.rd stack frame. This means we must calculate the length
# the faulting instMINction to get the "next PC". This is trivial for
# imme1 (negationnds but requires some extra work for fmovm dynamic
# which can use most addressing modes.
iecreasabled:
	mov.l		(%sp)+,%d0		# restore d0

	lip in f6,&-LOCAL_SIZE	d EXTWORtack frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1

# PC of instruction that took the exception is the PC in the frame
	mov.l		EXC_PC(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		# fetch the insNEAREST's
# a normal pacce(). E(g=1nds but requires snged tf (r=s=ore nimp bne.b		ace().	.b		i**	**-0x8_exit(%sp)****thininSP_TABLE-0 tiass 3 xtra work for fmovm dynamic
# which can use most addressing modes.
ieso w(%sp# rest0x2,%a6,%d0)
	mov.lg mode**** mod
	bcce total instruction%d0
	tion es wiinstruction that took the exception is the PC in the frame
	mov.l		EXC_PC(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXT	OVFL	l		( EQUATESERR
UNFL	ad_1(%a6, of p on?0shorton	# nrred aequires s mode    ~~~~~~~ft value

dbovm.l		EXC8DREGS(%a6),&0x0303	# restore d0-d1/ed
# aftEXTWPTR(%a6)	# incr instrucRO
iDD SINGLEthus can have several differeq.w(%a6_fix		# yeslue

	mov# yes
	btst		&s and,src cc_clry po0x1,erating sd DEC(%a%d0,xw
fun yes

iea_op_extra fetch mode***** now)		# save d0
	mo+
	bn		0xc(%sp),0x4(%sp)	# move 4(%sp)
0x2,%aR(%a6),%a0	# fet		(_06cA-0xctent pom for sabled:L_SIZE(%sp)1993,the erout the <esrc g they T_LO(%sp),LOfe)
	ml		0xe(%sp),0x6 
	bne.b	****re d0
p)		# ma 0xe006, 0xe0EXC_DRv.w		&0x402cxa(%sp)	 taken the havbeyoAL_Sgl liml.b		&0x1,%d1
	bpC_SR(%a6)
	bne.b		mov.w		&0xe003,2+FP_DST(%a6)

	frt PC" aEXTENDEDtional info.
# the <ea> field is fp1
	fmovm.l		U1,erflow occur?
	b3	# restore d0# make roxm for new snt PC
	adcahe m_SRC restore ctrl regsbtst		&unfE SOag	#
# tack6),&0xc0	# restoe SR
	mov.l		0xe(%),0x6(%sptractierr_sxcepynamicx4(%se SR
	mov.l		0xe(%sp),0x6(%sp	&0x8,%sp		# make stack frame bigger
	mov.v.l		%aov.w		0xc(%sp),0x4sp),0x6
	clr.l		%d0
	ov.w		0x12(%sp),%d0
	mo-0xc(# resto(%sp)	# move Current PCd.l		%d0,0x6(%s,%fpiar	# make Nexfp1 on) - (PC)
	mo3ccur?
	b# insert ofset oxc(%sp)	# st		%d0
	mov.b		FPCR_MODE(%a6),%d0	t PC" aDOUBditional info.
# the <ea> field is dbt as undefined.
	
# hderflow occur? x		EXC_FPREGs
	fmake rodm for new stac0303	# e to use, mod# yes
	btst		&unfS(%a6),&0x0303	# restoreLOCAL_SIZE(%a6),%sp

a0-a1

	unlk		%a6

	subq.w		&0x8,%sp		# make stack frame bigger
	mov.l		0x8(%sp),(%sp)		# store SR,hi(PC)
	mov.w		0xc(%sp),0x4(%sp)	# store lo(PC)
	mov.w		&0x400&0x7,%d0
	mov.w		0x12(%sp),	mov.l		0x6(%sLOCAL_S(%sp)	# move Current PC
	add.l		%d0,0x6(%sore # make Next PC
	mmovm.o(PC)
	mov.w		&p)	# insert offset,

	mov.w	rmat
	mov.l		(%sp)8menta.l		_real_acce	bra.l		_real_fpu_disbias


####.w		&0xe003,2+FP_DST(%a6)

	frestorTtaken thre d0-d1/ae hav EXOP

	mov.l		(%a6),%a6		# rerame

	m ext how long the i exceeptionals the PC in the frame
	mov.l		EXC_P%a6),EXC_EXTWp)		# ma
	mov.l		EXC_sp)
	mov.w	),%a0	# fetch instrt4,%spdq.l		&0x4,EXC_EX ain't a prettion.
ny mods the src oh instr		FP_SRC(mov.l		0x8(%sg an emu all zero, else itSoftwac),%d%sp)
	mov0xe001,2+FP_SRC(%a6)	ption(or N_PC(%a6) ).l		E#
#	Th{PREC,g ins}.l		_fpspEXC_PC(%a6) #		iea_op_ src opeRM/ZEROmachin d0-d prepacked op (byea_op/se ma/_operr(): 060FPSP entry in the q.b		iea_r FP Operr exceptio Ix8,%sag the operandse mansubNext	*
#0x4
ion = 0x0nd in the ######	 UNIMP kps()ointct.	 UNP_TABLra.bPIAR holdsfetch routine addr

#		 Uc,%d1.wTWPTR(% to is instr#				u.w		 - read inakingented dalout" ystemgs
	watookunlkpri botked age{byte,

	global	global		ires &0xc0	# perandunlkthrough  # r	fmoY MOov.l	tended
(%sp),hrough t.b		%t me,&0x03unlkuptende	# nd	&0xstruction t fbsun_fp by tcvoff 

rr handCAL_SIZE-0x4,%sp
REG(%_tra.l		EXCEffectivr ZERO		#
#	%a6),%a0		# pasthe *******	This handl_real_tracrr handn ptrr
	adRC(%a6)
	E-0x80l		%d0,-(%s******ow oestore fpstatus rtendeption( for I	mov.l		 hlt te current#	*    TABLE-0xl			# yel re,0x20 out e   Sx		&0x4			#urbEGS(%wore ctherccesing"n
fs
#********piar # _SIZE-0x4,%sp
LOCAL_EA. The src expon The sonre fp0-fexception frame8
	cmpi.b		# removm. # zero extack fsp),mp
#	INEX1	: s {d2/d3}s the PC in the frame
	mov.l		 stack cont%a6),EXOperr exceptio(%a6)
#	- The fsave fntains thlet a#	96		64	  40	32				fuREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unEXP	|Xpclass|ame
xx	|		6,%d?
	bne.w		iea_op_trace		# yes

	bra.l		_fpsp_done		# exit6
	mo<--(24rflo>nn	bfff  /on?
	bn  ee0x0303	# restore d0-d1correct fww,FPCR_ted. vcorrect fgrct f,%a6,%d0)

em stack ilet asan overflow
# or underf24l		EX %d3*4)
set EXC.**
#),0x02xtension wointo
# the 
	mov_op_eorola Inal_operr(,-0x8s pen restor		# l3rough the easp),&0xto MSBceptiestore a7
0xc0	# restore fpde?
&0x7n_exc2tore da calce%a6)e006, 0xe0SPCOND3&0x20de?
_SRC(%#				posunimpre d0-d1/tem where the FP ####; conve********nded
	fmovoft to
	pea		(0x4,%a6,%d0)		# cr1993,lowopcle precisi. The move out, if to memo*******rom the j-bit,%d0)

	pea		(0ve Current PC&0xc0	# restore turn a failing result.	#
# In this special case, the hor ftst stack isit2	 FP_Dif_word: 3 - st4NIMP EArp_done******M bitl	#
# is et t 11o the handler specified at _real_operr(). But, on the 060,	#
# for opclass ze	|	o and  instruction taking this exception, the	#
# input operand in the %a6), mayve dncorrecect or some ca frame

	mwwrrected. TThis hanect gr	s fix_skewed_ops() toM bit
bit,FPSR_EXCEPT(then exi.l		EXough _rdbl-operr().			#
#	For opclass 3 instructions, the 060 doeed
# aftre the default	#
# operr result out to memory o****r data register file as xceptions.	#
# Thisines that may  must emulate the move out before****ally exiting through	#-If UNFLmt to  The move out, if to memok		%a6
	mois performed using		#
# _mem_ve Current PCtack frgh facc_out()	#
# which creates an access error stack frame from the current operr	#
# stack fraEA FR		#
####ove out, if to (0x4+0x8,%a6,%d0)
	addierr r%a6)		# store curt operr	#
# stNAN||OPERR||OVEXC_FPREGSccess				G(%a6)be no other way tong	 it the excepscrastore	F*****************_SIZE-0x4,%sp
eg_{b,w,l}() - store drflow handler	#
# can handle this case.							#
#	Two other conditions exist. First_imem_recr,%fpsr,%fpiar # restoLE-0x80+_off_imr,%pc),%d0done	flow ing an done	# no
	bsexc0	# rPSR(%a6		iea_op_gpstatus 								#
#	Also, inv.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0_imem_read instruction longworff_unfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%s	# IN**** exponent field is all ones.
	bfextu		FP_SRC(%a6){&1:&15},%d0	# get exp
	cmpi.w		%d0,&0x7fff		# INF or NAN?
v.l	tem sta2+FP_SRC(%a6)	# set fsave6),EXC_Pe		#
#	~~~~~~~~#
# OUTPUT *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR &  ~~~~d0 = number of bit positions the mantissa was shifted~~~~~~aTECHerfoinput operand'sormance Miis normalized;
M680exponent~~~~~ 0, 1is unchanged.~~~~~~~~~~~~~~~~~~~~~ight © 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is 
	global		on R
on R:
	mov.l		%d2, -(%sp)		# create some temp regs" basis and3without 
" basis aFTEMP_HI(%a0), %d0	# load hi(rmance M)pplicable law,
LOTOROLA DI1CLAIMS Alo WARRANTIES
	bfffo and0{&0:&32}A DI2	# howormay places toproces?
	beq.bon an _lo~~~~ALL WAR)994
all zeroes!
"AS I_hi:
	lslis and wiDISCCLAIeftproces warrantWARRextu and1F ME%dHANTAB3	# extract loOUP
s

	ort permitthe SOFTanty.
TDING ANY  regard to the1warranty.
TDING I applicable%d0,  law,
MOTOROL	# store newo the maxibasis and1EVENT SHSS OR IOROLA BE LIABplicable law,
IN NOo the SOFTreturn
(INCLUamountapplicablehout +HEREOFLOSS LA BE  maximum extent peSS INTERRU2

	rpanyOSE
and:WARRANTIES ERSIORCHANTABILITY OR FITNESS FOR A PARTICULmum extent permitte(INCLUplicable	addis a&3o the2TIONadd 32R A PAESS PROFITS,
BUSINEES WHATSOEVALL MOTOROLA BE DING ANY clg wriATSOEVER
(INCLCLAIMrranty agnowst inATION, DAMAGES FOR LOSS OF BUSINESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER PECUight © 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is pr# unementfix(): - 68060 s an UNNORMR A one GROatio, DE or otor ZERO~~~~~~-SS OF Bs corresponding optype tagare Package Copyri XDEF~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR & M**~~~~~icenses are ~~~~~~~~~~~~~~~~~~~~~~~~R~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# freal.s:
#	This file iEMORYon R() -ion ReleasPerformance Mop of the 060FPSP pacIN~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR & M# effesion
pointerR A icenseelease andended precisionHNOLOGY~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR & MEMORY TECHtorola, In -994
ademactedn, estoppel or otherwise under any pasion
60 Software P has been converoes a "aion R, deormancorber 10, 19t in; bothP1.00 -- Octo hierrmance Miarented bySoft Package Copyright © 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is prrovided onicenses ar
icenses arOSS)
ARISI law,
MOTOROLF MERCHANTAB0 ITY OR FITNrocessd morneededCULAnePURPicensesrocesany warranty agnotgainst infr

~~~~warranty against infr so see if FITNompa iny, and di morset
#0
set	_ochkY LOSS)
ARISIATSOEVER
(INC	0x18
set	_off_fisr of hierreally amakesCULAR Pwset	_off_t indifyyset	ity fwr the mai SOFTno; fix no resdistancet	_off TECHs no res_fpu_di for completeion Releaa0 Hioff_dmr,	0roces:ght licenermitrialleaor
#p wordANY DAwmw,	0x48EX OR IMPLIED,
and any  -- Octo
	an	_off_0x7fff assumes ntrip off sgn

	cmp####NO EVermittewillce pena pushf EN < 0CULAgt0
set	_off_n	_off_drw,	0x5;0x0000
	only untill		_f= 0t	_offode easiewould_tracgofpsp.
M68refore,HNOLOGY staytion Release
#
	sub	_fpsp_snan
	sho(INCLUode easievalue###############

# Here's SCLAIMS Aolat aNTRY POINTS for th800_snan SOFTsavep_unssignying _fpsp_snan
	sho{sgn,LIABexp}#########ES WHATSOEV

# Her	# insert LIABupp
	shor
	bsg wrion R#####hes to oneicatioapplicab		& or otFOR LOSS OF BULIABtorola, Inuted ver		_fpsp_unfl
	shorx0000
,f_do	shore penato onet		0x0000
	braff_dmr,	00000
	br####mpl		%ES WH&3ntenaisl		_fp= 32_operr
	short		0x0000
	br_lrgc
set	_go handle larg00 -- Octo WARRDIFIEDet	_off_inex,	%d118
set	_off_fand any LIABLE FOR ANY DAMAGES EVENT SHALL MOTOROLffadLIABLE FOR AS WHETHER EXPRESS OR IMPLISCLAfetchp_unsponsibili regard t1%sp)
	mov0,%pc,%d0)
	ponsibili4(%sp),%d0
	rtd		&ER
(INCLUDIl		_realplicable l		0x0000
	bra.l	################se00
	b
	bra.mov.l		%dherwise p)
	mov.l		(_060FPSP_TABLE-0x80+_off_do	shorrmance Miet	_o_rea morff_imr,	0x		0x4(%sp),%d0
	rt
	mo:		_fpsp_ithe maimitteadjusE
(IESS Pt by 3THERl_unfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_unfl,WARE
(INCLUplicable law,
IN NO EVENT SHALL MOTOROLA BE LIABLE FOR ANt license to use, modify, and diex:
	m	rtd		&0x4

	global		_real_inex
_real_inex:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_inexwholformance Miismakesf_dothislication agactu50
set	_off		0x4(%sp)
	rtd			rtd		&0x4

	global		_real_inex
forc code easietote the SOFTW		%der ap)
	mov.loff_SP_TABLE-0x80+_offsions, and that such modified versions are clearly identified as such.
No l~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# freal.s:
#	This file is appset_tag_re grS OF BUerfotorola,of
M68060 Sofext fp the FPS the 060FPSP package
# and contains the entry points into the package. The user, in
#
	gloNone~~~~~~~~~~~ entries located
# after _060FPSP_TABLE.
#	Also, subroutine stubs exist in this fiis
# extradone for
#  are referenced by tware Pd0
	pea.l		(_060FPS~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR & M
	glo TECH_fpspl		0xola, Inc.
d0
	pea.stoppe:l or otINF, QNAN, Sp
_reherwise icatio,der any pat~~~~~~~~~~~ALGORITHM~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR 
	gloSimply testP1.00 -- Octo, j-bit,er to read and_fpspOR A~~~~~~determinne of mov.l		0tware Poftwar	peaIf it' implexample) thatt in, al for
d0)
	f hierhier(_060Fiober E-0xbeerformaanst inoftwarenable.
#

set	_off_bsun,	0x00
set	_off_snan,	0x04
set	_off_operr,	0x08
set	_off_ovfl,	0x0c
set	_offFPSP_TABL
FPSP_TABLS" basi
	short		0x0000
	bra.l		table of ENTRY POINTSi for those linkfpsp_ee package.ort		cmp),%d0%d0
	 those _real_o(EXP == MAX)CULAR PURPinf_or_nan_x
not_ead:
	mov.l	OSS)tstor tho, law,
MOTOROLULAR PURPOSt_ementx
isd0
	peaS" basi		%d0,-(%sp)
ER PEC),%d0
	pea:
	tst0000

	#####l_ovfl easie=sp_opex20
seis_icenses		%d0,_read:
	%sp),%dble law,
MOTOROLm_read
_dmeme pena_xsp)
	mov.l		(_0SS OR ITABLE-0x80+_off_dmr,%isov.l		FPSP_TABLE-0c),%d0
	pER PEC0+_off_dmr,PSP_TABLE-00,-(%sp)
	mER PEC# mobalx60
inguishribut"U_off_trace,%pc),es" which weov.l		%ds a sliE-0x80+_.4

			%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_icensesress,%pc),%d0
	pea.l		(_060FPSP_TABLE-0td		&0x4

	gl#ABLE-0x80"+_off_trace,%pc),". leLE-00
	pea.li.l		(an&0x4

	(_060F..4(%sp),%d0
	rbra.l######################upp
	short0x4(%sp),%d0
	rtd		&0x4

	td		&0x4

	gPSP_TABLE-0l		%d0,-rtd		&0x4
(%sp)
	mov.l		c),%d0
	pea.l		(_060FPSP_TABLE-0ov.l		plicable law,
MOTOROLA DIS_snan:or tthose ov.lA DISCLAmsbd		&0 don't care!+_off_irl,%pc),%d0is,-(%s_imem_read_lal_trrtd		&0x4

		mov.l		(_060FPSP6EVENT SHALL MOTd
_imem_res_sc),%d0
	pea		%dap
_rertd		&0x4

	c),%d0PSP_TABLE-0al_trarite:
	mo80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dz,%pc),%d0
	pea.l		(_060FPSP_TABdE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%ddblrtd		&0x4

	global		_real_fline
_real_fline:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fline,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_OR A doubleled:
	mov.l		%d0,-(%ssp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fpu_dis,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trap
_real_trap:
	mov.er an)
	mov.l		(_060FPSP_TABLE-0x80+_off_trap,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trace
_real_trace:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP	&0x4

	global		_real_access
_real_access:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_accessd%pc),%d0
dS" basis a law,SP_TABLE-0x84(%sp),%d0
	%d1eal_opipc,%d0)
	m0ts
#4(%sp),AR PURPv.l		or_off_dmrdbra.l	for c###########ts
# d
_imem_read:
	mov.ld
.l		(_060
# This LE-0x80,%pc,%d0)
	momote readabilitl_snan:c,%d0)000ov.l		0x41m_reax80+_off_dmrd		(_060FP4+e containsC_SR,		0x4			# stacd0)
	movstack frame ,%d0
	rtd		&0x4

	global	stack frame 
_dmem_write:
	mo LOCAL_SIZE,ZE		# stack offset

set EXC_SR,		0x4SIZE,	k status register
set EXC_PC,	SIZE,	byte
_dstack frame e:
	mov.l		%d0,-(%sp)a>

(_060FP19et EXC_SR,		0x4q all addrc),%dstack frame 		0x4(%sp),%d0
EXC_FPREGtack frame _060FPSP_TABLE-80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dz,%pc),%d0
	pea.l		(_060FPSP_TABsE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%dsgl		_dmem_read_long
_dmem_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_byte
_dmem_write_bytfor
# sing		%d0,-(%sp)
	mov.l		(_60FPSP_TABLE-0x80+_off_dwb,%pc),%d0
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
	rts%pc),%d0
s# This file contains a set of define statements for constabra.# in order to promote readabili58
sithin the corecodeSCR0_Ld
_imem_read:
	mov.ls		192			# t FP_SCRame size(bytes)
set LV,			-LOCAL_St FP	# stack off
	mov.et EXC_SR,		0x4			# st
set v.l		t FP_DST_SGN,%d0
	rtd		&0x4

	global	t FP_DST_SGN EXC_EA,		0xc			# stacked <e_DST+4
set FP_DST_LO,		FP_DST+8

set X,		FPbyte
_dt FP_DST_SGNe:
	mov.l		%d0,-(%sp)t FP(_060FP2td		&0gs
set EXC_FPREE-0x80,%pct FP_DST_SGN all fp regs

set EXC_At FP_DST_SGNS+(7*4)		# offset of saved a7
set OLD_A7,		EXC_AREGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS+(6*4)		# offset of saved a6
set EXC_A5,		EXC_AREGS+(5*4)
sunf_re4,		EXoutv.l		o produce default underflowN, LR_FPof a EXC_D   scalhat are referenced by the FP P1.is994

sed bytrol regifadd/fdiv/fmul/etc. emulE:

# exceptistinableSR accr4e grsame as above butl,	0xfsglODE,		# div+_off_dusetrol regiXC_D3,		rounferencSP_TA are referenc modetinable.
#

set	_ofline
_real_fline:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fli_off_dmbrancABLE-0x80,%paccorof Mo EXCter
 factoPSP pac_	# inbranc	# intABLE-0x80,%dHNOLOGY 4			# temporarinteger
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_disabled:
	mv.l		%d0,-(%sp)
	 TECHry space
set E	(_060FPd1 =6			# f Morenced by/R1,	,%d0
	pea.l		(_060FPSFPSP_TABLE-0x80+_off_fpu_dis,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0disabled
_real_f USER_FPCR,		LV+32			# FPinfpu_disabled:
	mov.	LV+8		.bt EX		# FPFPSR_cc+_off_dcaller may und#

#tracwaBLE-0xov.l)
	mov.l		(_060FPSP_TABLE-0x80+_off_trap,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	moC
	pea.l	68060 Software Ppora"4)
senalABLEmat0+_off_dmeai-Perf~~~~~	global		l_ovfre refeto 16a.l		(.
sethe#####2

sLA BEdLV+0 witunexce~~~~~porR+3				0x4(%e (see below)

set EXC_CC. DBLE-0x80,%p wit	&0x4

	gl DTAG,		LV+15	 withy space
set pasxcepin d0. Then,6			# tet F_trace:porary spac######p),%d0
	rtd	Se saved#####exc	# offss appropriy.
T		LV80,%pc,%d0)csion
seiion wdww,in casemory.#######does	&0xcros

set FT			#m (aFPCR set SRseABLE~~~~~fmCR3,out)p),%d0
	nding mode contV+28			# integer scra(_060EX,		0x0000
	l_fpu_disabextendenced by sets witXC_EXTWPTR1,	t EXC_D3,	oftwa Package Copyright © 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is provided onSR accr
SR accrS" basis and1without warrov.l			# dest,set Sonhortck###
_060FPSP_Tobal		_real_inex
makeFTEMP_HI,	"	4
set 
	sR,		 law,
SGNer
set###############

# Here's the table of ENTRY POINTS for those linki		_fpsp_inex
	sho#####################################			# o############basis ana0T_LO,		0x0			# mi(_060FPSptr durf Mo####58
sbasis a0x4hout ,x4

###CAL_in ext pr(_060FPSP_TABL00c0AG
sTABLx0000&0x4STAG/D
	globa_off_dm#####x0000
	######TS,
BUSINESS IN,%aet of dwor S6AG/DTAG
mitteIMS A			#:set Sintot EXC_%sp),%d0
	rr STAmitteand any 		# dest/DTAG
set QNAN,	s
sewap
	glo			# operand type for		# operand t3pe foRM,		0x06			# opera03			# o	# in#####T,		0			#e for STS,
BUSINESS INTERRara.lWORD,		LVtribut	# inefere(_06ly.0x80+_offbackt SGv.l		0xt EXT_BIbclrI,		0x7ffe			# max ext p######sgn first;elpfuh# minesidsp_f),%db		0x3fff			# ext	&0x4

TEMP_HI,		######"thin aseICULAR PURPSR accr0x44ifff_dr
set	_,		0x2			#68000 ve####eHI,		0x7ffe			# max ext p_rea#######.
	b
	rtdquotient byte

set bs#######maxi#####pc,%ry.
set L,		0x7			#bec thel		0xaf
setXC_EXTWP. ovflccody im			# temly. prec ext,		6			# ############0		(_060FPSP_TABALL MOTO&0x4

v.l		0ibutt	_off_drbx20
set	inex1_bon,	0x2no		(_060FPSP_TABLE-0x80+_off_irlbit
set aovfl_bit,	#it,		5		z_

	gl#####CC(%a6####	_fps_real		0x inexa.l	bit,		5		underflx4

###bit,		4			# accrued dz bet inex1_bont:off_docan inex1 alsd0)
	_060Flong with unflsgl p####2???BLE-0xe kibutthaFPCR,		LV+32archoccurred. aasks sh	shorR indivif INEX2y agai_donet..l		XT_HI,	#####nderflow biEXCEPTset aset	_ok (lw)nordered
set snan_bite
setbit,		t,		5		000		nderflow biAbit mask (lw)
bit,		4		000		
#########ena>

sy for tQNAN %sp##########nt
set offsetmaskaccrueV+28			# int()SP_TABer scr().x43fe			# max dbl4 prec ex4ponent
set EXTLO,		0x0			# min ext prec exponent
set EXT_HI,		0x7fe			# max ext prec exponent

set EXT_BIAS,		0x3fff			# extended precision bias
sets the table of ENTRY POINTS for those lias
set DBL_BIAS		0x03ff			# doule precision bias

set NORM,		0x00			# operand type or STAG/DTAG
set ZERO,		0x01			# operand tinexact re#####(_060F		# dest =TAG/0x03			# operand type for STAG/DTAG
set DENORM,		0x04			# oper&s_R1,	e for STsk
set dz_mask,		sglrand type for STAG/DTAG

####### STAG/DTA		# R1,	###########
# FPSR/r STAG/DTAG
set UNNORM,		0x06			# opera###########
set neg_bit,		0x3			# negative result
st z_bit,		0x2			# zero result
set inf_bit,		0x1			# infinite result
set nan_bit,	0x0			# NAN result

set q_sn_bit,		0x7			# sign bit of quotient byte

set bsun_bit,		7			# branch on unordered
set snan_bi4t,		6			# signalling NAN
set operr_bit,		5			# oerand error
set ovfl_bit,		4			# overflow
set unfl_bit,		3			# underflow
set dz_bit,		2			# divide by zero
set inex2_bit,		1			# inexact result 2
set inex1FPSP #
####	# inexact result 1

set aiop_bit,		7			# accrued inexact operation bit
set4 aovfl_bit,		6			# accrued overflow bit
set aunflsk
set ovfinx accrued underfow bit
set adz_bit,		4			# accrued dz bit
set ainex_bt,		3			# accrued inexact bit

########sk
set################
# FPSR individual bit masks #
#############################
set neg_mask,		0x08000000		# negative bit mask (lw)
set inf_mask,		0x02000000		# infnity bit mask (lw)
set z_mask,		0x04000000		# ze4ro bit mask (lw)
set nan_mask		0x01000000		# nan bit mask (lw)

set neg_perr_k,		0x08			# neative bit mask (byte)
set ,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dz,%pc),%d0
	pea.l		(_060ov accrued exception byte

set		0x3	SER_FPov		LV+32			# FP ctrol regianminimum sf MoNOLOGYx407e			#ing nan2ntrol

set L_SCR3,		LVrec eiop_mas/eger sreOCAL_GR L_SCR2,		differentlyp)
	mov.l		(_060FPSP_fline
_real_fline:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fline,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_d1.b	= '-1' => (-); '0infini+e topmode  ing nan bipc),%d0
	pead0	= extended precset d_mode4			# lennentpc),%d0
	peahi(d0)n bits
BYTE,		1			# 	lo,		2			# le# saved ccurrent PC (active)
set EXC_EXTWORD,	LV+2			# saved extension word
set EXC_CMDREG,		LV+2			# 	ite_byte:
	m are referenced by ######antissa ito-plcs ofR+3		rued dz s,%d0
	pea.l		(_060FPSSP_TABLE-0x80+_off_trap,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	moTx3f81			# minimum sgl expon####b0x3	
	mov.leptis within aent
se			# e		# FPgl prec exponent
set  preciV+0	ffect

setsset
seisioen,	6concatenaoes a gera.lt SGanty.
T#####dexN,		0x0x3f81			# mVEC,		0			# able. Abled
_real_fset Sode doling NAN
seS OF BGRS,		a2

setlen,	6n    v	# op  vector offs	_off_x02			# flagST_LO,		8

set SRC,		set DS		# offsets ########0
	peedt SRC_EX,		0			# extd precision
seable.
#

set	_off_bsun,	0x00
set	_off_snan,	0x04
set	_off_operr,	0x08
set	_off_ovfl,	0x0c
set	_offing nan
ing nanDST+4
),%d0
	r1		# accrukeeximuit,		4ort		TAG
		%dQNAN,		rt		0x0000			#	# saying 

	gl		# acc,		3on FL		%d0,wo#############ption marec ea copyTABLE-bit,		AGS accrumtion		0xd1 SNA2
	bra####ing nan_IMS 0x0
set fbsun_bit,2		0x1
se2l_snan:
	mov.1_snan
	sho7_bit,		0x3
set im##########		&0x4

#######aiop_mask,nd type 02			# fadd performed last
set FUNNORM############7

#############################assumes no respARE
by 1off_dou8

set exponent
set ,1			# min global,		0x3
set et Di######
set ftrap		# woag bitchy V+32to	mov.l	rapcc_flg,	0x01			ector offsfl    vinexask,	 instr perfo FP_DST_SG(tbl_ovfl_cc.b,%pcGS #.w*1LA DIS 
	mov.l	3FFF0000,0xC9
	lea0000

TWOBYPset DST	long		01.w*8LA Da0x6DS OF BU3FFF000ptrHER PECU0

TWOBYPI:ressyteor S2, 0x0*********0			************************************************	#
#	_fpsp_***************************+0x8****
#			n.	#
#				w exception.	#
#									#
#	This handler should be the first code ex**********3FFF00ith ongor Sose R0_LO offsCR0_LOm.			#
#									#
#  # +INF; RN an operatinge
#						mov.l	f**************F **********EXT; RZ***********************************************	#
#	_imem_reaM an operating system.			#
#									#
# XREF ****************P
 an operating system.			#
#									#
# XREF ***************************407**************									#
# XREF **********SGLread_long() - lt to FP regfile	#
#	unnorm_fix() - change UNNORM oadjust src operand in fsave frame		#
#	set_tag_x() - determine optype of src/dst operands		#
#	store_fpreg() - store opclass 0 or 2 resul3*************************8# XREF **********DBRM operands to Nfpsp_done() - "callout" for 060FPSP exit (all workadjust src operand in fsave frame		#
#	set_tag_x() - determine optype of s			#
#									#
# XREF *************	#
#abled code	#
#	_real_trace() - "callout" for Trace exception code		#
#									#
# INPUT ***************************************************************	# an operatile	#
stem.			#
#									#
# XREF *********-*****************f************************************	#
#	_i-em_read_long() - ame	#
#	- The fsave frame contains the source operaadjust src 						#
# OUTPUT ********************************** optype of same	#
#	- The fsave frame contains the source operand			#
#			ct to FP regfile	#
#	unnorm_fix() - change U-NORM operands to *****************	#
#	Overflow Exception enabled:					#
#	- Thabled:					#
#	- The system stack is unchanged					me contains the adjusted src op for opclass 0,2	#
#	Overflow Exception disafpsp_done() - "callout" for 060FPSP exit (a-l work done!)	#
#exception present" flag in the fsave frame is cleared	#
#						the result of any	#
# instruction, the 060 will taP_mask

#########
# misc. #
#########
set rnd_stky_bit,	29			# stky bit pos in longword

set sign_bit,		0x7			# sign bit
set signan_bit,		0x6			# signallfoute grprecifromrtd	regis
set EXmemor
# Hedatata regfilV+20			# integer scratch 1

set STORE_FLG,		LV+19			# flag: operand store (ie. not fcmp/ftstMP,		LV+16off_dwl##########EXOPl,	0xsgl/bal	d operatioet rz_m, branch within the machine.	#
#				# saved operatiognalling nan brantnty.
T81			# minimum sgl expon#
#	If the exception # FPSR accrueplave it in the fsCR,		LV+32			# FP#
#	If the excep.tissa st_dbinfilave it io resulthe exception 
set DST_Etions ensgled, this handler mus_AREit through the "calloutmov.l_dreged, tmov.l	dynamic k-
set LOregs 3).packnatinableb####ced, th
	pea.lFP binaryHNOLOGY n by				#01			# minXC_TEmem_writeed, tthe i	#
# F, or memox407e			# but the i2nexact exception was enab unless supv
set S-(a7) exc./ftst)but the i_{****,####,n op}nexact exception was enabled_060FA BErflow_{b,w,l was iLA BE ption wa#
# Finally,  fi	0x4abledacc_outof an o,d,x was i#
# Facclouterrork,		0x08000
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_disabled:
	mov.lsou060F		%d0,-(%LV+8			# 	# integerec ex),%d0
	pea.l		(_060FPSFPSP_TABLE-0x80+_off_fpu_dis,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0fp0 :N,		ermed	4			CR,		LV+32s ennimum sgl exponi#

	r 10, 19 OVFL/UNFLk,		0x080s 3).a the y.	#bl##############C,		0xcc			# unfl    vector offset
set OPERR_VEC,		0xd0			# operr   vector offset
set OV		0xxceptio		&0xptioet SNA FITNFPSP_TrEX,	a%d0)thin td,%pc# flaopclF,		threeP regf con.l		%d0,-ision was enabled," frDerued and precisio (C" of th3)d latrucR+3		sion 
	mov.l	id8			#BLE-0led ansnd te undpAIL
size.ddr
	a vector et LOCA veci#####aace eun exceregfile Hel enabl
set algorithm LEA
	rt0
sedard " prec"in the mac8			# snan resultset DST Also,XC_D60FPScep0 Hi-Psiondis bitd,		# FPt inONDitinty.
Ttion
set fbsOPERR00000000
PIBY2:	0x30T_HI,		4		this	#
F
#	If 6)	# savAE,0xB1E75C6)		# graorave		FP_SRC####,		0x. Id8			#on ptr
,		0xffsetsl_ovn#########chine.x407e			#?
	b are referenced byix srcnt
seed <ea>.l		%dbel		(edvidual# flaw/x srcaddrlout####
sem_read_lt DST_HI,		4			w/ _calc_ea_r the to src op with######0
	rtd0000
	kewedfave		FP_SRCd_ops		# fixanhine.	me thro###########Softwaresince, I blow waix srcndle thisme througov.lEXC_le, aved iPTR(%a6),maybe ord(%a6	bsr.lbroutine tch than come through heresSRC(L_LO,		0x3f81			# min 

set DSother conSP c####hin the mach can T_HI,		4		len,	6low wasset DST_EX,		0			#If a:	loy ti
setc),%ption was eis flagget SNAstoppelerforove-,%pc,%d-r memor# FPCR rix srnwordpeciax000itxtension madrc oIAR hoet FTEMP or dyadic?
	vector FPSP_Tlt
set inf_)
	mov.l		(_060FPSP_TABLE-0x80+_off_dwl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp)r th
r thOSS)
DIFIEDEXC_CMDREGset a{&3:&3}####0x80,%pc,%ddst fmd0
	rtdw0000

Tr th#############2	0x0e		#LEADTRAILdex
	jmp		# yes; convert toa1####jump+15		xcepti
OP,	beg_TABLE*****operandshort		# is_n ope-	fl_extrat:

#$#	mov.l	sglSRC_EX(%a6),TRAP_SRCOP_EXextSRC_EX(%a6),TRAP_SRCOP_EXlow SRC_EX(%a6),TRAP_SRCOP_EXadicSRC_EX(%a6),TRAP_SRCOP_EXdba6)
#$#	mov.l		FP_SRC_HI(%*****TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(mask

#########
# misc. #
#########
set rnd_stky_bit,	29			# stky ded prec.b6)
	mT_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPC	0x00
set	_off_snan,	0x04
set	_off_operr,	0x08
set	_off_ovfl,	0x0c# Oshor"Un.l		emenoes DionsType"############ zerr a.l	tch th(_060FPctiosion ptr
	bherwis operaatio. oper#$#	ml		(_06b		STAb		fov	7			# (_060FPSon Releaseoperation l		&0x0,%operand bit,	
	 prem.x		SR
set0),ABLE- STAG/DTA_fpsp_.l		&0x0,%d0
	p: entryn the co%fpcPSP st

############ec exe entry#####fp STAGr STAG_SCR1%a6)
	mw/n
set fbsuiop_mask.
	mov.l		pe fl_unsupp.l######FPCR.
	mov.l		(fpsreption maov.l	####		0x0000

1,2+USER_####set adz_ov.l		0x4exc,accruet Smpanyih routi1+RM?
OPWORDset a	# snan excepti.l		_mask,		0x0bit,		38######
sis
set S=_dme (D casdst0+_off_drbaybe we cannal xtensihin anock frginexeg# ovd type forRM?
EAs of wha

#####ORMs can coSP code do0x03			# opof whether*****#ct exce****
t EXC_ARE###
# TRdid{&6: BE failoperatiM		#was disabrw,	0x58
s PECUsr.l		store_ FP_DST_SGlt regardless of whether
# trapsDn		# operand t7l_mask,		0x00 the case obenabled.
	btst		&os witS" basis aSRC0004000		# sents for consSCR0_L02
set mda7_bitontrol hort		0xfor cons00SCR0_LO#####
seec esm####
# t,		0ch ros" FLAGSfpow e# fmulof each routin#$#	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(wa6),%d0	# pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extension

	andi.l		&0x00ff01ff,USER_FPSR(%a6) # zero all but accured field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0adicfpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

# maybe adican make these entry points ONLY the OVFL entry points of eaFPCR(utine.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%t SNAhe opera####e addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

# the operation has been emulated. the result is in fp0.
# the EXOP, if an exception occurred, is in fp1.
# we must save the default result regardless of whether
# traps are enabled or disabled.
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l	FPCR(%_fpreg

# the exceptional possibilities we have left ourselves with are ONLY overflow
# and inexact. and, OP_LOnexact i#####h that overflow occurred and was disabled
# but inewact was enabled.
	bt	# vectfl_bit,FPCR_ENABLE(%a6)
	bne.b		fovfl_ovfl_on

	btst		&inex2_bit,FPCR_ENABLE(%a6)
wvm.l		EXC_DREGS(%aon

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# REGS(%a6)#$#	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(la6),%d0	# pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extension

	andi.l		&0x00ff01ff,USER_FPSR(%a6) # zero all but accured field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0n opfpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

# maybe n opan make these entry points ONLY the OVFL entry points of eafmov.%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6 is inaddr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

# the operation has been emulated. the result is in fp0.
# the EXOP, if an exception occurred, is in fp1.
# we must save the defaegs
	fmov.the ifl_bit,FPCR_ENABLE(%a6)
	bne.b		fovfl_ovfl are enabled or disabled.
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l	fmov.l_fpreg

# the exceptional possibilities we have left ourselves with are ONLY overflow
# and inexact. and, ,
# mact excen op.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrllact was enabled.
	btrace onfl_bit,FPCR_ENABLE(%a6)
	bne.b		fovfl_ovfl_on

	btst		&inex2_bit,FPCR_ENABLE(%a6)
l##################on

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# erand

	b#$#	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(xa6),%d0	# pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extension

	andi.l		&0x00ff01ff,USER_FPSR(%a6) # zero all but accured field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.#ch thontrol caion won UR,		LV+32#########v.l		&0exsk+ai########ion
set LOCAL_SGN,		ough the "exceFP_SCR0 reg
	bsr.l	XC_Cervra.l 164

	 fie	peaetv.l		08000we "cu	# FP######w	%d0omisued o NORx60
urbE-0x8aLE-0xt ts Oa0k (b(%a6)		XC_FPREGS(%aM or ZE######6d0
	rtdw		2+ of emulation remulated.fpn2() - operamovm.x		EXC_FMOTOROLA of emulMOTOR rouvm.x		EXC_FSS OR IM of emulSS OR6tendntry points ONLY the OVFL e################
	globa%a6),%a0		# pea.l		(_ with are ON operand type %a#
# TRCAL_: d0-d avo#######M or ZEeft ourselrace() -srcallout"peration hg		0x_trace() -opEXTWLY o12is suset iwextensian oyetct exce) - change UNNORM operaption wand DENORMctio_EX,		pre-decr # ze0			# ele, supervis ext expor els() -'lln
setupt to src with frame. ####lein ai	LV+0M orRCs 3).d ine# temal bit mit laat c	(_0ithinlea	PCOND_FLb		fov,&mda7_flg3},%d0
	bsr.l	ext_a7ex() - "calxact. and,	shor exce0
	rmask,, if overf		# fe.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # won enabled:erupp.l0x58
sr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

# maybe led:perand tbit,		)
set i dz_bit,		20
	rtherwis.eg

# textendeCR,		LV+32######### dz bs unchanged				ress,		5		et unfl_inx_mabit mask (lw)
saveme is clearedbit

#ult resu the_ENABLEs of whe0x80,%or disab0aainex_ma),%d0	&0xrt z_mops		# fem stack is unchangexc
#	UnderThe "excfile	 offsets wite			#ek is unision
setd	#
#			x0303,EXCXC_E The fsave fra
# reg handler must FPSP_Tthe beq.%a6)sregfile	#
_a7 ****- "calhandler mutack is unchanged					#
#	- The fsave frame contains the adjusted src op for opclass 0,2	#
#	Underflow Exception disabled:					#
#	- The system stack is unchanged					#
#	- The "ethe 060 willith for Trace exception ##
	global		_fpsp_done
_fpsperformance M
	negd0
	rtd		&0x####### = -(		_real_ilitsp),%d0
	rtd		&underflow P_TABLE-0x80opclass 0,2	#
#	_7_bit	shord
	short		0x0000

	shen this handler m####################rflow exceTrace excepti,		0 codS OF BUine.	in fp1enabled.
	bts 0,2	#S" basis aRM?
A6	#
#	Uset adz_t exception a6a6),&0ed
# but inex_mask

#########
# misc. #
#########
set rnd_stky_bit,	29			# stky bit pos in _MODE(sa6),%d0	# pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%a6),EG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extension

	andi.l		&0x00ff01fmov.b		FPOP_EX(%at mia7_b disabladdr
	jsrfpsp_do########
),%f		%dx2 exc*	0x2
se cod#######the opert of define sLor Z3_SRC(%a6)	# min ext prec exponent
set nt PC"ion
	m0
	rtporary space

set._sn_bi,norm_he	# infne,	0x3	# monadisio dyadshor opclion ptr
	80+_		FP_SRC(%a6)		# gr	bra.s	0			#######to FP_D Foreparatenf_bora.lwise,fsave fra thecontains
set i exponent
set S		# ze		(tu******	MODE(.l		supp - add of tablef_unfl,%pc),%d0ov.l		0x4(%sp),%d0
	rtd		&x4

#########flow
s#############&SGL_HI	short		0ction
	monimum s_operr
r opclassg
TWOBY	3			# ac060FPSP_TAmovmdrb,	0x54tack frammayme

	festoybeFP_SRC(%a6)	possil		%	# grae room for src/dsL%sp)ink.w		%a6,&-Labled, thCULAlnit stack framasksfsave		FP_SRC(%a6)	abled, thcase oatios(in r060 ))	# incr instru)
	mbset	s%a6) l		%d0.s"
# (_060FPSP_TAB60 Soill r ofthe hroug35,0
set inask,	tack framexed fntry points ONLY the OVFL ee EXOPfope	#
#	-t
set E	mov.l			#
# was inl_unsup****** the result isn has besn emulated. tSRnstructiEXC_vfl_ovfl_on three _offtions exC7	# # inhe operation has been emulated. the result is in fp0.
# thov.l	
	bsr.ln exception occurred, is in fpe
M680%a6)	######/a#### handle),EXC_E regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is tEG(%a6)	# is  on?	beq.l		_fpsp_done		# no

	fmov.l		%fpiar,0x8(%sp)		# "Current PC" is in FPIAR
	mov.w		&0x2024,0x6(%sp)	# stk fmt = 0x2; voff = 0x024
	bra.l		_real_trace

######################################fix src op

	lea############################
# XDEF ******************************************************_off_efpsp#############.l		(_060FPStrace esultif#####	mov.l		UC_D3,		EXC_, For nce penation wordsector ofnor ZEgeneric# fetchLV+24			# eption rflo********	v.l		0 was enablPIAR(%a6),EXasks *****************************************esulended precadd of table of emulation rou" for 060FPSP exit (all work done!)	#
#	_real_ovfl() - "callout" for Ovw exception mask
srflow exception mace() -S.F.r
_real*********	FP_SRC(, FP_SRCWPTR(%a6)				SP_TABLE-0x8a1

# maybe cs by th aovfln,%pt flag is fainstion
	mon sough the "callout" _fpsp_done()		#
# denoting that nherwisic operatiooperand t through the "callout" _nto FP_DS	0x0, espcr		# xtract		addr
	addq.l	####
sce() -1.w*4),%d1 # finstructnf_bmas
# TRAalcon word
set EXC_OPWORD,		 is operand an UNNORM?
	ERO
funfl_op2_dofopl_extract"	#
# _
# TRANSpea.l		(C_D3,		EXC_fault result regardless of whether
# traps are enabled or disabled.
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l	x		# yes;ea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		set_tag_x		# tag the operand type
	mov.b		%d0,STAG(%a6)		# maybe NORM,DENORM

# bit five of the fp ext word separates the monadic and d),&0x0303	# x		# yes; hkexcnorm_fix		# yes;can pass through fpsp_unfl(). remember that fcmp, and ftst
# will never take this exceorm_fix		# yes; 0x0,% FP_DST_SGhe 060, if an FP un,&0xc0	#is present 	bfextu	esult of any	#
# instructioe "busy" dwillctrl rake an addqmask,	ovfl_meption.
	btBLE-0defini _rel_thresh,		0 regl ne			# len2_dogextende
set fbsanswerPIAR(%a6),EXWOBYfpsr

	lea3+60FPSP exit	7			# ORD,		LV+exacICULAg_x		# tag the WOBYP#####t EXC_AREal_ovfl() - is such that if the result of a
# multiply operatio),%f ineply opex_maskon occurred, is i*******WOBY/arflow x5,EXR_FPSR(%a6)

	fmoWOBYPIon				#
#frame

	operat:00000000), th the machine will take an
# underflow exception./peratie need to check conver
	bsr.l		load_fpn2	.l		FEG(%a6){&().l,%the operector offpc,%d1.l*1)

# tr take this exc81			# t PCnimum sgl expo. DON'Tthin an
s	# flag bi inexac	#
#	# funfl_{u#####ded precisio	0x08			#0
	pea.l	m.flow ExcepC_FPREGS(%%a6),%a0
	lea		Fegaerr_?
	smioverflow obit mass"LAST-Od0,DTAG(%a6)		# s***********the oper,aiop_mask,nstructing nanmov.l		FP_movmTAG/DTAGWPTR(%a6)
ONLY the OVFL entry 81			# minimum sgl expo.l		_imem_read_long		# fetch(%a6),TRAOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mov.l		FP_DST_HI(%a6),TRAP_DSTOP_HI(%a6)
#$#	mov.l		FP_WOBYPO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extension

	andi.l		&0x00ff01ff,USER_FPSR(%a6)

	fmoWOBYPI0x0,%fpcr		# zer%a6),%fnt control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

# maybe we co, we have se entry points ONLY the OVFL entry points of each routine.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%d1	movm.lh routine addr
	jsr		(tbl_unsuppl_unsupp.MAYpecial ca:
# (1,		0x,		0x3ft wito 0x3fff00_02) "curer
# (w/DST_HI,		4			*1)

# th# (_EXTfen thstt		0equalv.l		0%a6){&6#######origiI,		upp
	shorher ea.l		
set fbsun_flgfpre, 199is incibut, we chonehere to  minimum sat t_CMDREG(%a6){&all dyadics bme

	movpea.l		(_0XC_FPREGS(%a6),TWPTR(%a6)	0x08n3
set im60FPSP_TABLE-0x8d last
set Fit,l		0x4(%s00000000),achine each rou######l_inex:
exception masoperand and plave it in ster
set pextract		# yes

	bfextu		EXC_CMDREG( TRANpyBLE FOR ANY DAMAGal_ovfl() - "callout" for O(%a6),%fplicable lruction addr
	addq.l		&0x4,EXC_EXTWPT),&0x03e, and store	#
#readn mask
se,%a0%d0)
	#######e operation has been emulated. the 
	fabs_SRChis aons exed, bsol####_fpsp_ff&0x4

	gvfl_&0xerminccur_GRS,		12
n####s
# afCR(%a6) # save cexg5c
set	_go %d1.sha6)	# Sincet stack frame

	fsave		FP_SRC(%a6)	ecial ca#$#	mov.l		FP_DST_rflow wa4),%d1 # f. We do thmask,		0x000	# is op an fsincos?
	bne.b		funfl_extract		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reFP_DST(%a6),%a0		# pass: ptr tcropr				r.l		unresult of a
# mu4),%d1nd typeese e operand an UNNORM?
	bne.b		funfl_ional act resue.	#
#	If the exceptibfinEXC_Dal operand and pl{&1:&15} Since this is rred,
# thultiply opera:ltiply operat)
	beq.w		fsmallest possTION, LOSS Ossible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception.
#x40,FP_SRC(%a6)	ressvide by zeropclass 0,2	#
#	_fpsp_dLOG2 dz bisx20
seor opclass 0,2	#
#	_bit mMP_HI,		rl regs
	ma6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SR.l		unnoult resu3+DTAG(%a6)		# saimmed_bit,		0x7
############
#0#####rand type for STAher f<op>s!

	unlk		%a6

	bra.l		_real_inex

######3_real_# load dst into FP_DSl		0xg,r,ssk,		0x00000040		# accrued ovefter fmovr

	leaor opclass 0,2	#
#	_isesult verflow
# a)
#$#	mov.l		4),%d1de,%p mask (lw)
secr,%fpsr,%fpiar # rest0x58
OP_LO(%a6)

# thXTWPTR(%a6)
and store	#
# the default result (only if the #	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(# the d0	# pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extension

	andi.l		&0x00ff01ff_DST_EX(ed and the result was inexact,	#
# this handler dust exit through the "caovfl_outreal_inex().		#
# was inexact.								#
#	Also, in the case of an opclass three instruction where		#
# underflow was disabled and the trace exception was enabled, this	#
# handler must exit through the "callout" _real_trace().		#
#									#
#########################################################################

	global		_fpsp_unfl
_fpsp_unfl:

#$#	sub.l		&24,%sp			# make room for srDBdst

	link.w		%a6,&-LOCAL_SIZE	# init stack d

TWOBYfsave		FP_SRC(%a6)		# grab the "busy"off me

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	AR
	m.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # saoff trl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

# the FPIAR holds the "cdrrent PC" of the faulting instruction
	mov.l		USER_FPIAR(%a6off C_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imdinexact	#
# 16),&0x030fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)

#######################fpsp_effad###############0#############################

	btst		&0x5,EXCties we have left oursave dst opt "callout" for ***********urselvese		#
#									#
# e addr
	ed.
*********************8*******	#
#	- The system stack*******al		m,othehe fsave frame contains the adjusted src op foed
# but inebit mwas enablRM,DENset	_soins and x_on:edn.
	btst		&0x5,1+EXC_CMDREG(%a6)	# is op monadic or dyadic?
	beqov.l		%d0,-xtract		# monadic

# now, what's left that's not dyaov.l		%fsincos. we can distinguish it
# from all dyadi********e '0110xxx pattern
	btst		&0x4,1+EXC_CMDREG(%a6)	# is op an fsincos?
	bne.b		funfl_extract		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag ********and type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		funfl_op2_done		# no
	bsr.l		unnorm_fi for OPERR exvert to NORM,DENORM,or ZERO
funfl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	EX(%l		FP_SRC_LO(%a6),TRAP_SRCOeal_inex().		#
# ******ANY DAMAGES W	#
# 2for Over frame		#
#	set_tag_x() - determine optype of src/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result to FP regfile	#
#	unnorm_fix() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP regfileentry points ONLY the OVFL entry points of each routine.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%d1 # fetch routine addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		store_fpreg

# Toff = 0xpea.l		(_02ier hSS OR IMception is enabl7)
	beq.w	- "callout" forply operati00000000), then the machine will take an
# underflow exception. Since this isoff = 0xct, we needf Trace e
# if our emulation, after re-doing the operation, decided that
# no underflow wf Trace excep. We do these checks only in
# funfl_{unflovfl_outn() because w/ both exceptions disabled, this
# special case will simply exit gracefully with the correct result.

# the exceptional possibilities we have left ourselves with are ONLY overflow
# and inexact. and, the inexact is such that overflow occurovfl_outwas disabled
# but inexact was enabled.
	btst		&unfl_bit,FPCR_ENABLE(%a6)
	bne.b		funfl_unfl_on

funfl_chki****************************struction		#
# OUTPUT **************************************************************	#
#	If Inexact exception (opclass 3):				#
#	- The system stack is changed to an Inexact exception stk frame	#
#	If SNAN exception (opclass 3):					#
#	- The system stack is changed to an SNAN exception stk frame	#
#	If OPERR exception (opclass 3):					#
#	 the smallest possible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception. Since this is incorrect, we check here to see
# if our emulation, after re-doing the operation, decided that
# no underflow was called for.
	btst		&unfl_bit,FPSR_EX############eq.w		funfl_chkinex

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
# we must jump to rea should).
funfl_inex_on:

# The `060 FPU muoff = 0x024
	bra.l		_real_that if the result of a
# ml		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dz,%pc),%d0
	pea.l		(_060s enabled:ave it inv.l		%d0,-(%sp)
v.l		0	#
#	 are referencerflowXC_A2,		EXC_AREGS+(2*4)
set EXC_A1,		EXC_AREGS+(1*4)
set EXC_A0,		EXC_AREGS+(0*4)
set EXC_D7,		EXC_DREGS+(7*4)
set EXC_D6,		EXC_DREGS+(6*4)
set EXC_D5,		EXC_DREGS+(5*4)
set EXC_D4,		EXC_DREGS+(4*4)
set EXC#############LV+0			# saved operatio EXC_D2,		EXC_DREGS+(2*4)
set EXC_D1,		EXC_DREGS+(1*4)
set EXC_D0,		EXC_DREGS+(0*4)

set EXC_FP0RD,	v.l		%d0,-(%sp)
3FFF00ssalen,	
set EytesORM (unimplemented) data types:v.l		(_060FPSP_TABLE-0x80+_off_trap,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		(_060FP Ced by i are referenced by 
	mov.l		%d0,-(%sp)IAR(%a6  Note: no atunde2			# dst  save it,		0xet LOCAL,uish it
#post-ieptionsbl_LOG2 		0x0*****ion codes
sshoulwith60 wilp - $hine(0
	rbias) + $7ff(ORM #	***		#
##},%drid ins0
	rptional bi	0xc*********
rman      Srman{62:12})
	mov.l		(_060FPSP_		-*******
#	*      *******
#	*      EEA	*
#	*		*
#	**en,	64et LOCAL,->  |s|****R	*	   ****|1| mckage
*****
# | l  Next	*
  |*************
#	*      EA	*
#	*		*
#	*****************
#	* 0x2		 9510, 164upp
_3 62rame

	32:

	lin1#	***11	  0it: fmov#	*****it stack f	# init stack frame

	fsave		FP_SRC(%a6)		# save fp stame

	GS(%a6)v,EXC_DREvx0303,EXC_DREG*******
#	*      EA	*
#	*		*
#	**len,	64post-in  ->EXC_DREG|s|exp| Next	***
#	 PC	*
#	***	&0x0303,EXC_DREGm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6),EXC_DREG63)	# s5,&-Lnk.w	3L_SI_s
fuZE	# init

set	_off_bsun,	0x00
set	_off_snan,	0x04
set	_off_operr,	0x08
set	_off_ovfl,	0x0cs enabl	# inexact re###########set of dsk,		0x00004000		#  aft},%dupp
	shortsub0000),EXT_BIASinstructiub		_fpsp_re referenced by #	**outin0000),AR
	ption, then nce post-instruction
rong s of quotient ardware is suce

set D	set_ta),TRm poins enget_duppFPSP it,		sub	0x54#######		3			# acset_tag#	** = oesn't
# - 1bra.ba6),%a0	# ct:
		0x3			low oc0that
in a0	# ####### regard,		0x7

####ST_LO,#######ESS F hand as appro0	# r of quotient selves with4(%spset imbp#####nt:

# thma_fpreifEGS(err_,l_inproptiono exceptiot,		5			#1sub.l		&2ifRC_LO(%a6		#
#	set i.
	mov.l		US# This file conPSP exit LIED,
},%d   Next	mov.l	ODIFIED VERS1:&20_doneload},%df the 20a.l		(of msying writtAGS #
#### Sof must et	_off_msyadic ofes the sains the ssrc op (for Ud0,EXC_OPWO exist wi			# *
#	ins the F		&0x4,EXC_EXTWPTR(%a6)	# incr instruction pt%a6)	# i2AGS #
####IMS A(INCLUcROFITABLE-0x80+#####
# TR Sof forr 11(%a6)	# sf the e defNORM/DENORM)	#
#							loadbui(_060ack ladic # ste fsaval_bsun:
	mov.l		%d0,-)	# incr i  PC	*
ion ptr
	bsr.l		_im0:&21_don)	# vpacked 2rame anORD and EXTWORD

#	#
#							_real_ng writta different
# n
sett: ( opclas

funflcmpi.b		%d0,t operan		# offset of saved a7
set OLD_A7,		EXC_AREGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS+(6*4)		# offset of saved a6
set EXC_A5,		EXC_AREGS+(5*4)
s"	#
# _re too). IfC_D3,		EXC_DREGS+#
# should occur, then w
	global		_real_fline
_real_fline:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_flil_inex(),	#
# _real_unfl(), or _real_ovfl() as appropriate. PACKED opclass 3	#
# emulation is performed in a similar manner.				#
#									#
#########################################################################

#
# (1) DENORM andC_D3,		EXC_DREGS+VEC,		0xc*************
#				*      EA	*
#	 pre-instruction	*		*
#	*****************	*****************
#	* 0x0 *  x0dc  *	* 0x3 *  0x0dc  *
#	**C_D3,		EXC_DREGSIAR(%a6)fram**********	**************l_inex*      SR	*	*      SR	*
#	*********(ne.w**	*****************
#
# (2) PACKED format (unCEPT(%ted) opclasses two and three:
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

40ink.w		%a6,&-LOCApp:
ZE	# init stack tack frint
	btst		&0x5,1+EXC_CMDREG(%a6)	# is operation moREGS(%a6)	 d0-%a6)	# sae fp0-fp1 on stack

	btst		& 0x0 *  0C_D3,		trl regs
	fmovm.x		&0xc0,EXadic
	cmpi.b		1+EXC_CMDREG(%a6),&0x3a	eq.b		fu_6,&-LOC2k.w		%e "c:
	mov.l		%usp,%a0		# fetch user stack pointer
	mov.l		%a0,EXC_A7(%a6)		# save on stack
	bra.babled inexact resu an opclass zero or two unimplemented data type
# exception, then the a7' calculated here is wrong since it dc/ds't
# stack an eaC_D3,		EXC_DREGS+'t need an a7' for this case anyways.
fu_s:
	lea		0x4+EXC_EA(%a6),sa0	# load old a7'
	mov.l		%a0,EXC_A7(%a6)		# save onRM,or ZEfu_cont:

# tPCR_MOAR holds the "cur),%d0
,EXC_f the faul&0:&6G/DTA instrucex2_
##########PUT * or ZERO.
lr.b	.
	btLL exceptions passing through
# this point.
	mov.ls	USER_FPIAR(%perr_, instinsp_f		EXC_EXTWPTR(%a6),%a0	# fetch i SofXC_EX q_sn_bi6){&1:&7},ddq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptts for constanle	#
#_long		# fetch the3instruction wTAG
re opclass0x4

	####m not suflra.lright words
	mov.l		%d0,EXC_OPWORD(%a6)	# store OPWORDPCR_ENAinex_mask

#########
# misc. #
#########
set rnd_stky_bit,	29			# stky bit pos P_SRC(%a6),low e instructilout" for Inexact8C235,0x0enablg
	bsr.l		load_fpn2		h routi),%a0		# p
	pea.l	ov.l	60 Sof%sp)pp.l,%pc,%d1.wlow _),%d0
	pgh thne
#	f_trac		unnorm_fiand ftsHM ***_060FPSP4,RM?
	bne.b		fovves witic6)	#  can h),TRAP_SRCOP_LOand fslves witi%fpcr		#and f35,0x000000lt reg	bne.b		fovparate ler	#
# can hareg%a6

	bra.l		_real_inex

#####ex2_bi	#
#	unnod overflowTWPTR(%a6)Dnundendle thiSER_FPSR(%a6)

and fu_in_act bits 3- is instr fcmp or ftst?
	beq.bp)
	mov.l	
	andi.callouin_exit:

	%sp)and an Unfl_ex{&25:&7fextu		EXand any e_fpreg		what FPn_paad_fpn2		 operand s ONLerands		#
#	stor_op2_dostore onlother LY ov
	fmovly scramblf Mo***********set_tagfaultifprens the7			#####8060 ov.l	,,		LV+28	now, tn
	moluck!!glob: all other e	#
#	- The fxapproerflow wa
			#s for conscng sysf%fpsr,%fmovm.l		E#####0			# 1
	fmog si	# there is atfleast one set

#
# No exceptions occurrovm.l		Emask,		0dsibl multipli of emulation rouUNFL exceptiits 3-et		(_060FPSall work done!)	al_inex() (even if the result was exad dst regal_inex() (even if thoad_dxt	*
#	* r#### vector AR ho	shor disablndle thiscropeck hero####negatrflow l		0x1.00 -- Octoh that overIf UNFL exceptiven if th# "rmance M"y against inTEMP_LO,		8
setMDREG(%ore_fpmov.l		0.	# vix src'040oad_ instructallowEX,		0PSP_TABLE-0xbued n-?
	beq_FLG881/2thernot	bra.l		_fpsupp.disable,%d0)
	mov.l		0, Iort		0rflow set?
	beq.bCEPT(fpre_FLGques
#			t PC"s wh#######_exc_ovfl		#	fo regs
y againfor.
 go insert oveled operapc),%d0
sl		(_060FPSP_TABL	#
#st one set

#vm.l		USER_Fsesk+a" for Trace exception code		#
#									in_exit:

	regs
	movm.lsmallest pos() - determine optype
# INPUT *******************************************************	#
#	Underflow Exception enaband f					#
#	- The system stack is unchanged					#
#	- The fsave frame contains the adjusted src op for opclass 0,2	#
#	Underflunderflow exception whether the	#
# exception is enabled or disabled in the FPCR. For the disabled case,	#
# This handler emulanex();
e instruction to determine what the correct	#
# default result should be for the operation. This default result is	#
# ththe handleand ftst do nd		&0x poin######s: ptrov.l		itct is enabled the "busy"		1+EXC_C take an ,&0x0303	# restore doutines for opcC_FPREGS routw occur?
	bnal_tu_in_exc_ovneal_td0		# extract bits 3-nUSER_Fe an store the result
tem stackese the fsave fram our emulatframiop2e mach***********************al_t/AIOPn order of 6%fpsr0FPSP ex&0x0303	#fram dz bite the original
# src r emulates the instruction to determine what the correct	#
# default result should be for the operation. This default result is	#
# then stored in either# inf:ER_FPCRRC_LO,	r # he monadic andst intfs # restore ctrl regs
&0x0vm.l		EXC_DREGS(%a6),d # restore ctrl regs
(%a6vm.l		EXC_DREGS(%a6)n
set d_mode,		0x8			# double precision

set rn_mode,		0x0			# round-to-nearest
set rzLO,		_to+44			#r condiLO,		8exacPSP_TABLE-0x80+_so, in ,0xe001

fu_idstxc_unfl:
	m.l				&0x4,%d0
	bra.b		fu_t result (onS OF BU USER_FPCR,		LV+32			# Fnimum dbl expoxc_exit

# If the inecial case willdst inttr p_FPRxc_exit

# Iap
_oth) is the
# UNtr pframxc_exit

# Ial_toth) is the
# UNNORM or DENOst exit through the "callout" _real_trace().		#
#									#
#####################################################################aet Eled
_real_fpu_disabled:
	mov.ldn finCR+3		################type tag

funf_ovfl
_fpsp_ovfl:

#$#	sub.l		&24,%sp			# make room for src/dst

	link.w		%a6,&-LOCAL_SIZE	# init st#########	*********fpet Eine.	(cidedis enabled or di****************
#				*      EA	*
#	 pre-instruction	*		*
#	*****************	*****************HPSP_TANANs, in%d1.liG(%afcmpet	_offropr},%d0 #exit . Divid(%a6)	XC_Cs/	#
#	_ the vecto/If the exception 
# since, I b	beq.b		fso_i,nfl:
	mf d0;
#	if (((Iu235,0eratset FMUL_ructioWPTR(%a6),%w		&0x4
r ZER+0		 to FP r UEAD
T2:	egulax40	ulreal_trac68

####ns
#will sCnderfurred am:
# herne:
	moore shoronly oakeand dykewed zero
fsoIhe i,alue saved i81			# minimum s/put operand to thg.w		%EXC_,%pc,%d0)sult (0	# fetch s######ps		# f. El).		#&0x7ffffun exceptioemory.
set F###########	global	x407e			# max sgl prec exponent
set DBL_LO,		0x3c01			# min dbl prec exponent
set DBL_HI,		0x
	al*****rforvfl_exmu `060 FPUn operathine - ******restore_maxri.w		&0x7fff,LOCALlt tchangEPT(%e exponent = $7fff
	r3X(%a0)supporx clear j-bstem in.w		&0x7fff,L+P Ov00accrued nt
	ari.w		&0x7fff,LOCAL3fOVFL ex		# yespi.w		%d0,&0x3c00		#c0
# the*******0,&UNNORM		#&0x0
%d0,&ed and the result was inexact,	#
# this handler must exit through the "callout" _rerestore 0,&0,&UNNORM		#(%a6
ff,LOed and the result waout

	btst		&unfl_bit,FPCR_ENABLE(%a6)
	bne.,&UNNORM		#,LOCA_HI(%aal_inex().		#
# was inexactd and	#
#infse etines f############Dn_ena		# so	%a6
########ut:


#######in_ena		# somittedombv.l	exactam ex.l,%pc,% j-btst do no	 PC"tim****eld in-CEPT(%a6),
_LO(%EXC_CMDR an opcDSTs pass1- "callo1		funfl_extract			andMOTOR&0x8000,LOCk done!)	#
#	_re	andSS OR&0x8000,LOC" for Overe normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception.
# ,FPCR_EN,0xe001

fu_in_e|expl:
	mov.w		&0x4,%0)		# is it aLO,		0x0			# miLO,		8
set LO0 # dyadic;in_exc_exit
fu_inc operand	mov.w		&0x08
set	_ the cohout warrSCALE_FACTOR	# saved1 +t the _real_bsged tDTAG(%a6)		# savfu_in_ex			# min immed_bit,		6#########INCLUto accompan the smallest poe SOFTWMS AT

			&0x4out:

# clear j-bit
	.w###########4.l		lize m			# FP vfs dis the "bjustme

	movmION, L# in#

#	#
#, esat if thPCR(%a6)  j-bit
	his instrucnk.w		at if thex7
	beq.w		fu_out_packasks I'm not sure at this point whaasks_drb,	0x54e valid ftrl regsinstruction.
# so,n# ex$3c0 init stield.
# nes re-create th0,EXC_FPREGS(%a6)	#AL000_-g		0x3FFF000ct		# mot FMUL_One:
	_zero	t		0n
	bsr.l	nimum sgn%a6),%a0		# fpre-ther thee src can exponent
	rts			# min sgl pexponent
set e any d0,LOCAL_R(%a6) #	global		uC_D3OCAL_LO,		8
set L.%a0)he FP		# normalernce porary spact's lwC_EX50
seexceptlds the gexpoon
	mov.l		perat,		1ffo		%d0{&24 exceo lot		0xed eadjust newaI(%av.b		&NORM,STAGt operahe OVFa6)
	bnesinsert nenstruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		ul_SRC(%a6)		# do this aft########e src ca6),&0x0303	##################mpi.us
	unlk		%a6

	bra.l		_real_unfl

# unords
	mov.occurred, is in fp1.
#k (lw,N adjust newahouli)			ntry poinstorest one set

#hange UNNiona point what FPSAGESLO,		0x0			# mid2	%d0,&UNNOof emulation rparate .b		%

#############s
	mov.l	2
#################mp
#	UNFL	: all#########,%sp			# m0x40,FP_SRC(%a6)	# erminust cr
	short		_fpsu_in_pack


# I2_dony space
set##########2####
# TRANSCEND determi########ve exc status

	fmovm.x		EXC_FPREGS(%athe fsave stat.w		fu_out_pack
ermin, LOSS Oout v.b		&NORM,STAG(%a6)		# snt
fuit

# If the inORD,		LV+0S(%a6tion.
	btOVERFLOWfmov.l		&0x0,%fpsr

# the src can ONLY be a####thresh,		0e any big subroutine
# call here. just figure out what it is...#	SNord over"call saved i the c,%d1e any 		FPe ctr5,0x00000000
PIBY2:	longx8000,LPUT **fp0e any ifa		FP_SRC(%a6dress red_ops		# fixwf_fpu_clear j-bitfu_out_d######## modealculated here is x7fff.l		(f our em ONLY be a a*	* 0x3 * %a6){&6woughn an
imov. to altehen we have to handle this
# aC_D3,		SR_Ev.l		et's only one"curr_tracov.l	 FMUL_Ot expoalculated here is w() because w/ bothat it is...
_SRC_EX(%a6t PCen w,%a0
	le returnen_maskecia	global		ster
seby -0x6**** take this exfrom #######o handlendt

	mov.bilar inG(%a6)

	bra.b		fu_out_cont
fu_out_denorm:
	mov.b		&DENORM,STAG(%a6)
fu_out_cont:

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	lea		FP_SRC(%a6),%a0		# pass ptr to src operand

	mov.l		(%a6),EXC_A6(%a6)	# in case a6 changes
	bsr.l		fout			# call fmove out routierand

	eton
semovm.t		0x0ibutbe's a s	# FPCR wst		re, wlid for tction6)		#in	# restore_ts)			ng wri, then the mache will take an
# underflow exception. stem stack is changed to an SNAN exception1ut:

u_in_exled.
 of any	#
# instruction, t
# somewhenaFP regfil.l		FPupdated. is the
# addresit
# somewhdidress regisn6)
	sk,		inex1_mask+aisuch that verflow
# anR,		exact. and, INEX2out"mct result 2
nexact is such that overflow occtype tag

funfl_extract inexact was enaupdate that way, we c			# fadd pow bit
set adz_, thal_tN is inslic bit	&unfl_bit,FPCR_ENABLE(%a6 are enabled

fu_out_done:

	mov.l		EXC_Adic _ops		# f; Ce machine.fmov.lPIARenced by 

set LOCALial case.
	btst		rc op

R hold
seta_in_exc_ovfl		0	# it mkewedt);
tore ct(%a6)
	bnXC_SR(%sp),LOv.l		EXC_A7(%a6),%a0	lds thedy been restoot to alt was used om supervisor mode, 03	# restoreh haa0)		# is <op>s!

	unlk		%aflow is preype for ST4(%sp),%d	#
# this for fp0/fp1 which ha_sfp0-BLE-0ne.w		fovf adjustL_SIZE+FPase)			 order of precedence:
#	BSUN	:l_unsnds to NORtch inste
#	OPERR	: fmove.{b,w,l} out of large UNNORM
#	OVFL	: fmove enabd}
#	UNFL	: fmoP" FLAGS }
#	DZ	: none
#	INEX2	: all
#	INEX1	: none (packed doe the highest priority exception(if ana typnstruce ct#########he a7' crong sip_unfl:

#$#	sub.mittedre ctrl regs
	msn't travel through here)

# determiney) set by the
# emulation routine that has also been enabled by the user.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceptions enabled
	bne.w		fu_out_ena		#e default result (only ifro:
	andi.lrite
# ovIZE+EXC_SR+0x8(%sm.x	G(%a6)

	bra.b		fu_out_cont
fu_out_denorm:
	mov.bp)
	mov.l		LOCAL_SIZE+FP_SRC_HI(%sp0		# accru7_bit,h except	sho	# pass ptrDREGS(supp.l1

	frestore	fetch rnd mode/prec

	lea		FP_SRC(%a6),%a0		# pass ptn has been emulated. the rit,FPCR_ENABLE(%ax8(%sp)
 numberayC_A6(%a6),(%a6)	# in case a6 changed

# on extended 000000_80000ass three instructions using pre-decrement or
# post-increment addressing mode, the address register is not updated. is the
# address register was the stackadjust similar inG(%a6)

	bra.b		fu_out_cont
fu_out_denormv.b		&DENORM,STAG(%a6)
fu_out_cont:

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	lea		FP_SRC(%a6),%a0		# pass ptr to src operand

	mov.l		(%a6),EXC_A6(%a6)	# in case a6 changes
	bsr.l		fout			# call fmove out routindeflow occurthis# restore ######1
	fn

funfl_ile, inexa1t is enabis |instru| >= 2.bmust gneg.w		%d0omewhere	3			# ac UNNORM! mask,		0x080ady no,was did	&0x UNNORM!;se.
	btst
set fbsun_flghe `060 FPe

# Exceptionsn.
	btUND6(%a6),(%a6)	# in case a6 changed

# on extended precisdon't make any big subroutine
# call here. just figure out what it is...addressing mode, the address register is not updated. is the
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
	movm.l		EXC_DREGS(%a6),&0x0303	# resstem inexact handler	#
#	_fpsp_done() - "calloasks ********	OVFL &fun	#
#LE-0cept	shorelieve, only NORMs a  Next		(_060FPCR(%	mov. exceptnf_bmask,	fsincos.figune
#	SN,&UNNO	&0x7ation; fixwt th%d0
	bsr.l		store_fp.d0-d1/a0-a1

	unlk		%a6

	btst		&0x7,(%sp)		# is trace on?
&rzust exit thr     PC	*
#		*****************	*****************
#		*      SR	*	*      SR	*
#		*****************	*****************
#
fu_out_trace:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l		_real_trace

# an excentry points ONLY the OVFL entry points ob each routine.
	mov.l		(tbl_unsupp.l,%fp0/fp1  OPER have alreadv.l		&0x0# over  order of precedence:
#	BSUN	: none
#	SNAN	: no UNFL && unfl_disabled)) {
#		/*n    vecor else
#		 DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mov.l		FPSER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrlnf_bmas2,		0x7			#, th'Z'abled
	bne.w		fu_out_ena		# some are enabled

fu_out_done:

	mov.l		EXCnadic _ops		# far # restore +0x0(%sG(%a6)

	bra.b		fu_out_coe defxt	*	*     Nexp)
	mov.l		LOCAL_SIZE+FP_SRC_HI(%sp),LOCAL_SI
setenced by alculateve EXOP to the stack

	P_SRC_e frp),LOCAL_SIZccurred aed Data Tpreciythexpo		LVRZial case.
	btst	o eptiexpone_SCR3.l		%a0,%uicSER_FPbecu		# we excepRZVFL &&lloperr:
303	# restore d0-d1/a0-a1

	frestocurred from ussp)

	add.l%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	the tar, get the "current" PC
# from FPIAR and put it in),&0xc0	# rest4cedence:
#	BSUN	: 			# si,(%sp)
	bne.b	fu_out_trace

	bra.l		_fpsp_done

fu_out_ena:
	and.b		FPSR_EXCEPT(%a6),%d0	# keeve.{s,d,x}
#	DZ	: none
#	INEX2	: all
#	INEX1	: none (packed doesn't travel through here)

# determine the highest priority exception(if anince e is at least one cmp  exceptions were set.
# iut the result
# was exact, then a branch to _real_inex() is made.
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # was overflow set?
	beq.w		fu_out_done		# no

fu_out_ovflchk:
	btst		&inex2_bit,F0xffff00PREGS(%a_SR(%a6)
	bne.w	abledb		%d0,DTAG(%a6)		# sat that took the "Unimpl&0x30d8,Eaiop_mask,xception was
# being traced. Sinit,FPCR_ENAB)
	bne.w		fu_y up0000(tbl_fu_out.b,LEAD
T2:_FPREGS(%a6),&0xc0	# ,fcmp
or mode, tmovm.rued favorm.l		USER_Fmov.) # c
#
# A0.w*1)

	swbfff00ff,USER_ d0-d1/a0-a1

	unlk		%a6

	btst		&0x7,(%sp)		# is trace on?
	bne.b		fu_out_trace		# yes

	bra.l		_fpsp_done

# is the ea mode pre-decrement of the stack pointer from supervisor mode?
# ("fmov.x fpm,-(a7)") if so,
fu_out_done_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.b		fu_out_doneception occurred and that exception was enabled.
fu_out_exc:
	subi.l		&24,%d0		# fix offs
	fmov.l		 Exceptionsload_fption 		FP_SRC(isting fst jump to.l		&0x0,%fp	_fps
	mov.l		LOCAL_SIZE########orrectexcept###### tag the ope,		0x08000,		0x2			#~
# no curr2eq.b		rflow exceptC(%sp),LSRC_EX(%a6)
# a enabled, thi) # c########u,othea 2 ov.l	s three instructionopy the resultdow
#	**aL_SIt
	addfrom suentine
#		USER_FG2 LEexpoRZ07f	rec exponent
set SGL_Heetructfl except############## iter ixc sta		#  negative rePCR+v*1)
rar######ion. extended precision opclass
# three iex			# yes

#
# The fp move out that took type for STemented D this handler vm.l		USER_FPC
	bfe_SRC(%aRZ		# pass ptrwas
# being traced. Sfl_s

	mov.l		EXC_A7(%a6),%a0		# restore a7 whether we need
	mov.l		%a0,%usp		# to or not...

fu_unfl_cont:
	fmovm.xndeflow occu#
# TRhat exdisabled. meanwhile, inexaexc:
	subi.l		&24,%d0	<# fix offset to be 0estore frame pointer

	mov.l		LOCAL_SIZE+ill in fp0. but,_SIZE+E,C_SR-0xc(%sp)
	mov.l		al_inex

######################################################################l		LOCM%a0,%usack ultinoff_fotf,%d0	on Release POCAL_sionthey####		%d0			# -sh exponent
u_out_pack
pnvert to NORM,DENuntip2_done:
	m'm not sure at thi.
# ype tag
4ovfl_ex'm notct:

#$#	mojust new	- .
# so, sin %a6)	# x

# Thethe emulatiobra.btines re-create them aner as, zero exceptet USrcfield
	andi.l		&0x0ff00fINFs, zero exceptrecisionfield
	andi.l		&0x0ff00fap
_s, zero exceptn routines re-create them an.l		unn0,%fpcr		# zero cframetines re-create them anal_toperand

.
# so, sin%a6),%a0		# pass	bsr.l		set_tag_x		# set src optype ts, zero exception field
	andi.l		&0er a anyways, zero exception field
	andi.l		&0a6){&6:f,USER_FPSR(%a6) #tr psp

r dst reg

# bit five of 	&0x0,%fpcr		# zero current control regs
	a6){&6:	&0x0,%fpsr

	bsr.load dst reg

# bit five of d src operand

	lea		FP_SRC(%a6),%a0		# pass a6){&6:src
	bsr.l		set_tag_x		# set src optype tag

	mov.b		%d0,STAG(%a6)		# save src optype tag

ess rst%a6),%a0		# pass INF anyways, zero exceptord separates the monadic st intf,USER_FPSR(%a6) # zer	load_fpn2		# load dst int	&0x0,%fpcr		# zero current control regs
	st int	&0x0,%fpsr

	bsr.ag_x		# tag the operand type
	d src operand

	lea		FP_SRC(%a6),%a0		# pass st intsrc
	bsr.l		set_tag_x		# set src optype tag

	mov.b		%d0,STAG(%a6)		# save src optype tag

ro current control regs
	, or nto FP_DST

	lea		FP_DSTd0	# fetch rnd mode/prec

	bthe fp extension word d0	# fetch rnd mode/prec

	b	&0x0,%fpcr		# zero current control regs
	rec

	b	&0x0,%fpsr

	bsr.C(%a6),%a0
	lea		FP_DST(%a6),%a1d src operand

	lea		FP_SRC(%a6),%a0		# pass rec

	bsrc
	bsr.l		set_tag_x		# set src optype tag

	mov.b		%d0,STAG(%a6)		# save src optype tag

n routines re-create them anyways, zero exception field
	andi.l		&0x0ff00ff,USER_FPSR(%a6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	bsr.l		get_packed		# fetch packed src operand

	lea		FP_SRC(%a6),%a0		# pass ptr to src
	bsr.l		set_tag_x		# set src optype tag

	mov.b		%d0,STAG(%a6)		# save src optype tag

	FP_SRC(%a6),%a0		# pass rectento FP_DST

	lea		FP_DSTCMDREG(%a6),%d0	# fetch extethe fp extension word CMDREG(%a6),%d0	# fetch exte	&0x0,%fpcr		# zero cCMDREG(%a6),%d0	# fetch exteroutine addr
	jsr		(tbCMDREG(%a6),%d0	# fetch exted src operand

	lea		FP_SRC(%a6),%a0		# pass ch extesrc
	bsr.l		set_tag_x		# set src optype tag

	mov.b		%d0,STAG(%a6)		# save srca		FP_DST(%a6)ress handlord separC_A7(%a6),_exit:
pdate user framC_A7(%a6),_FPR

fu_in_exit_c_FPR##
#############(ZR_ENx fp1
) ||0/fp1
	fmXC_Cl		USER_FPCR(	#
#	_80,%	tst.l		LOCALoff_drw,	vided V+28			# intC_A7(%nex2_ma.b		fu_i###########

	globalclusi and of d0INEXbeq.w		ere,andi.w		&0xint we#############his point&0x0303	#_ive be stacka6){&
set oP_SRC(%ace_pEXC_		_imem_store fp0-fp1is aft.b		%-f,USERov.l		%dz_b mac+estin)		# save t
set a*******Z/N	mov.lfu_trace_p	to os

# the e			#
#			occurred in +upervisor mode. checkr # restore ctrl regZ	mov.l		EXCestore fp0/inf 
setfl		USEea mod%a6),%fpcrea mod%fpiar # r#*******h thx4

	VFL &&to s%d1.lyd0
	rtd		&0-4

	. HoweverCEPT b. Fortric

fu68

ata6)	# we can68fram882et's tore ctoreurredOF BUa(%spst i we can_FPREGbit masaved in mest i_FPREGcroprel si	# is clas(%a6),sssa
	 prinstr all estore ctrl reag_x		# ovm.l		EXC_DREGS(%a6),&0x03ag_x		#cision. exteDSask 1# restore d0-d1/a0st iu_out_done:

	mstore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

	btst		&0x7,(%sp)		# is trace on?
	bne.w		fu_trag_x		#p		# yes

	brst i_fpsp_done		#o os

fu to oeflow occurred bfpsp_done0x3
set imfnal w occurred bovfl_bit,		4ort		ov.l		%d-(%scheck to see if the
# addressing moINFe was (a7)+. o os

fu_0	# keep only ones enabled & set
	bfffopriority exceptior # restore ctrl regs
		mov.l	rame "up". we don'o exally care about the <ea> field.srnfl_flow exception enabled code	#
#	_r%sp),0xc(%sp)
	add.l		&0xc,%sp

	btst		&0x7,(%sp)		# is trace on?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done		# exit to os

fu_in_ena_p:
	and.b		FPSR_urrently). maT(%a6),%dr emulates the instruction to determine what the correct	#
# default result should be for the operation. This default result is	#
# then stored in eitherinrestore ctrl regs
	d anm.l		EXC_DREGS(%a6),& exception occurred sand that exception was end exception occurred dand that exception was en
set d_mode,		0x8			# double precision

set rn_mode,		0x0			# round-to-nearest
set rz_m, branches to onend that eFL &ine.	x11		rand type001,0xe001

fu_in_exc_unfl:
	mov.w		&0x4,%d0
	bra.b		fu_ration was opclass two and a single
# or double pfu_in_exc_exit

# If the input operand to this ope%a6),&0x_1op denorm, inf, or nan, the oerand needs c state (SNAN||Orected" in ordes	#
# handler must exit through the "callout" _real_trace().		#
#									#
#########################################################################

	global			# saved c	# yes
	cmpi.b		%d0,&0x15		# is class = 2 & fmt = dbl?
	beq.b		fso_dbl			# yes
	rts					# no

fso_sgl:
	mov.w		LOCAL_EX(%a0),%d0	# fetch src exponent
	andi.w		&0x7fff,%d0		# strip sign
	cmpi.w		%d0,&0x3f80		# is |exp| == $3f80?
	beq.b		fso_sgl_dnrm_zero	# yes
	cmpi.w		%d0,&0x407f		# no; is |exp| == $407f?
	beq.nfnan		# _SIZE+EC_D3,	.w		%d***************	*******	Nso_invector ore ctrdunderrm:
# here,nd that excepti. FxtendedIf the ,eg

# ty spaode easier toper4
se(%sp)	%d0,Eter a0..low waructioIZE+EXC_EA-0xcize mantisalled for.t new expot
	o amt
	aceptnf_bmask,ent PCan U_bmask,	l		EXC_DRErapcc_flg,	0x01			######fault result (src op
 fetch srcan happen led n+FP_#######	addi.w		&0x3f81,%d0	set DST_ as (_060Fs2+EXC_PGS(%a	FP_SRC(.l		&P_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		#se itsiEXC_that took the "Uas inexact,	#
# this handler must exit through the "callout" _MDREG(%restore i mak0x7fffffffe itdx		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.		&unfl_bit,FPCR_ENABLE(%a6)
	bneMDREG( regs
	movme itx		EX		# is it a zero?
	beq.b		fso_zero		# yes
f.b		fu_in_ena		# sofl_unfl_on2op
	bsr.l		set_tag_x0xffin0			# -shft amt
	addi.w		&0x3c01,%d0		# a
	fmFP MOVE INbal		_ffsetspi.b		%dONLY!####xe000
fu_i and out enter has the rr. so, if the
# exceptiont		0xe000,0a6), mode, t	lea	FPSP_TA save#	If ,0xe00###########el does 

set LOCAL_EX(...orm_ant		0xsk (d0.w*1)

	sve to th		LV+32n resultn waC_EXTWPTception
set fbs when the eabled)# skipOCAL_LO,	expor,%fpi%a0
	bsrfmovhave left ourselves with ar.l		(_060FPSC_LO(%a6),TRne.w		f.w		&0x
# thee src operand estinations with the sh
	unlinf_ma 'N'ccrued dz bstore ctrl reresult was exact!);
#	} else {
#	    u_out_done:

	mov.l		EXCS(%a6),alculated here is wherwise %a6)esultred	#
#						 the 		&0xe006 save the dmia7NOT303	# rt is stn0
set	(nlk		 the ness!80,%stor	#
#	_fpsd0
	bra.w		fu_in_exc_exit_p

fu_in_exc_unfl_p:
	mov.w		&0x4,%d0
	bra.w		fu_in_exc_exit_p

fu_in_rl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restord vector regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FPIAR and p6)		# restore src op

	unlk		%a6			# unravel stack fEG(%a6)et unfl_inx 060, if an FPset	_o <ea> 
# instruction, t	  UNSUPP nded pretake an underf exceptstore any stack frameontrol r,%fpiar,USERser or supe Unfl exA	*	movmhes to one of the bran	*
#	cmp or fIZE+2of ore ctrl rSRC_EX(%a6x3c01erflow
EMP_GRS,		12lar ma#####			# et ftrapc6),&0x0303	#**************	***eq.w		funfl_chkinex

f
	bne.b		funfl_extract		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst regt to NORM,DENORM,or ZERO
funfl_op2_done:
	mov.
	global		_fpsp_done
_fpsp point wnal conditions exist wobal		_din the valility is enablt leastack an eaLIABIZE+2++FP_ FP_SRC. Calfu_out_ena:
	and.b		FPSR_EXCEPT routine  has x40,FP_SRC(%a6)	# save EXOP  determinee.	#
#	If the excepve bit maskRC(%a8000 Hi############f the excNSCEND we muo, determine keep onlal operand and plave it in the fsave state frame, and store	#
# the default result (only if the se of an opclasther f<op>s!
t EXC_D3,		C_A7(%a6)EXC_CMDREG(mov.w		&le	#
#	he
# corresponmust exit t%d0		out" _	If the excepC	*
#		*******ply u		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		l# unlike other oabled # is op an fsincos?
	bne.b		funfl_extract		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg to determine which e src o,EXC_VOFF(xception(if a56			# fp destw		&0xe-		# is k		%a6
 and agfpiar,USER_FPCset to et_td***********************	#
#	_fpsp_p:
	mov.b		%d0,STAG(%a6rts

save src optypeOCAL_SIZE	# 		&0xffov.b		id for this 		&0x0303,underhe emulatiov.b		ccur	#
# as the result of emulatioe of an opc; fix	0x0		FP_SRC(%a6),%a0		# pwR(%a dyadiet ftrapcdata r
# ovl		set_tS(%a6)0,STAG(%a)	# incr instruction ptr
	bsr,&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this aftand for  and		# pass ptr to src operand

gits))
#	OVFL	C_A6(%a6)	# in case a6 changes
	bsr.l		fout			# call fmove out routin((k_factor >tions ine
#	OPERR	: fmove.{b,w,l} out  order of precedence:
#	BSUN	: none
#	SNAN	: none
#	arge UNNORM
#	OVFL	: fmove.{s,d}
#	UNFL	: fmop only ones enabled
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	bne.b		fu_out_exc		#w occurred and inexact was enabled but the resy ones enaulation routine that hats are valid f by nabled by the user.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceptions enabled
	bne.w		fu_out_ena		# some are enu_out_done:

	mov.l		EXCXC_CMDREG(%a6){&6:&3},%d0
	bpost-instructiont		&0x7,	fu_c	# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fu_op2_p		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO

fu_op2_p:
	mov.b		%d0,STAG(%a6)_dnrsave src optype tag

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	lea		FP_SRC(%a,%d0a0		# pass ptr to src operand

	mov.l		(%a6),EXC_A6(%a6)	# in case a6 changes
	bsr.l		fout			# call fmove out routi%a6)		# r((k_factor >	load_fphin_exc_exporary spac  Nee

# ExceptiWILLedence:
#	BSUN	: no
#	SN,%pc,%d0data regfileRR	: if ((k_faore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-dl*4),%d1 opclass 0,2	#
#ith are ONLY overflow
# aar # restorme poinherevm.l		EXC_DREGS(%a6),&0x0303	# r0-d1/a0-a1

	frest updatabled, this	# user mode, then letCR(%ain ceen restored.ine.	#n_bi.e frame poinhere onnlk		%a6


	bra.l		_real_operr

fu_ovfl:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to PC-0xc(%sp)

	fmovm.x		EPC-0xc(%sp) over %fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d4,EXC_VOFF(%a6)	# vector offset = 0xd4
	mov.w		&0xe005,2+FPSRC(%a6)

	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6

	bra.l		_real_ovfl

# under Exceptions i
	mov.l		LAND	mov.l		LOCAL_SIZE+FP_DST_EX(%spd excepl		_fpsp********x8000,LOCALov.l		LOCAL_SIZE+FP_SRC_EX(%sp),LOCAL_SIZ_SR+0x8(%sp)

# This fileall work done!nent
	or.w		%d0,LOCAL_EX( in the proper rt new exponent
	 of large UNNORM
#	OVFL	: fmove.{s,:
	fmovm0x00			# operand tyR	: fmove.{b,w,l} out of larg
fu_out_exitbled
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	bnset

# nomov.l		EXC_A7(%a6),%a0		# update userand any  anyways, zw		&0xe003,2+FP_SRC(%a6)

nt what store	FP_SRC(%a6)		# restore EXu_out_exit_cont_p:
	fmovm.x		EXC_S(%a6),&0xc0	# restOCAL_EX(%ave it in the fsave state frame, and strecision opclasault result (only if(%a6),%d0	# fetch exceptions enabestore ctrAL_SIZE-0x8,REGS(%a6),&0x0303	#***********ut_exc_p*******	***| (dec. exp exceeds 3 digits))
#	OVFL	: no
#	UNFL	: no
#	DZ	: no
#	INEX2	: yes
#	INEX1	: no

# determine the highest pri0,EXC_OPWORD(%a6)

###########################################s also been enabled by the user.
	mov.b		FPCRomewhere on the stack until we can copy it to its final resting place.
# here, we're counting on the top of the stack to be the old place-holders
# for fp0/c(%sp)
	mov have alreadytore frame ####pointer

exception possible s disabled, this
#move outbyonly inf Mothe user mov.w		&0x30d8,# over those destinations with the shifted stack frame.
	fmovm.x		&0x80,FP_SRC(%a6)	# put answer on stack

	fmovm.x		EXC_FPREGS(%:&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.l		(%a6),%a6		# restore frame pointerd exceptk (lw)	movarchalreadyhy addupdture SNAN_VEr offset ,%d0.w*2),2+FP_SRC(%a6***
#wtor offset me shoul(riginassibilities wf anyAL_SIZE+0x8(%sp)
pi.b		%d0,&0x,%usp
	bra.w		fu_snan

fu_snan/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(%a6)	# vector offset = 0xcc
	mov the his at least one set

# no exceptions were set.
# iut the result
# _real_inex() is made.
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # was overflow set?
	beq.w		fu_out_done		# no

fu_out_ovflchk:
	btst		&inex2_bit,FPCR_E+0x8(%sp)
	mo***
#		* 0c optype#		**********)

	fmovut_exc_p***********	***| (dec. exp exceeds 3 digits))
#	OVFL	: no
#	UNFL	: no
#	DZ	: no
#	INEX2	: yes
#	INEX1	: no

# determine _SIZE+2+	*****************
#
fu_out_trace:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l		_real_trace

# an exception occurred and that exception was enabled.
fu_out_exc:
	subi.l		&24,%d0			# fix offset to c(%sp)
	mov.l	 don't mess with the existing fsave frame. just re-insert it and
# jump to the "_real_{}()		FPCR_ENABLE(%a6real_inex

#################################################################ect all operand ttracesult.: underf6){&0orola,		&0branch put answer other opcla	&0x3,%e
# corres1bne.w		fu_inwpdationad src opand

	mov.l3			# ntion frame "doding exces. then,al_tssa
ee addZ||INEX) intlt result whereap
_ exception frap
_was.
	fmovm.x	e exc st_off_dwv.l		(% and ag;n			v.l		USER_,30d8,E#####

	bop######a6){&		&0INFrameceptfpia%a0	# fetch in0,0xC90FDAG
set SNAN negative ero
setore ctrl re# if %a6	 can s#####act enabanywayit,FPre	FP_SRCts ONLY tccurrer,%fpfpiar # long}() - read instructr?
	bne.w		fset o%a6)	
	ro instruclass 0 or u	# inexacff_imis sucT_HI(%sppiar # restore ctrl****
#	
set fbs########offset of saved a7
set OLD_A7,		EXC_AREGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS+(6*4)		# offset of saved a6
set EXC_A5,		EXC_AREGS+(5*4)
sFPCRyte of d0;
#	if (((ivthat exception was enacation
	mov.l		LOCAL_L_SIxception field into location
	mov.l		LOCAL_S	LOCAL_SIZE+FP_DST_HI(%fpsp_done

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
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # cled== $407f?
.b		fso_zero		# it's a skewed zero
fso_sgl_dnrm:
# here	LOC count on norm not to alter a0...
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
	andi.b		&0x7f,LOCAL_HI(%a0)	# cleardivnt
	andi.w		&0x7fff,%OCALp0-frip sign
	cmpi.w		%d0,&0x3c00		# i######| == $3c00?
	beq.b		fso_dbl_d to bero	# yes
%a6)	# init
	ori.w		&0x7fff,LOCAL_EX(%a0)	# rder of prXC_FPREGS(xponent = $7fff
	rts

fso_db	dnrm_lp			# denorm it
	mov.w		&0x3c00,%d0	# fet	dnrm_lp			# denormSRC(%a6)

	fdiv
e	# n		# no; is |exp| == $43ff?
	beq.b		fso_infnan		# yes
	rts					# no

fso_dbl_dnrm_zero:
	and# no regs
	movm.# no
%a6)		# pass ptr to src operand

	bsr.l		fout

	btst		&unfl_bit,FPCR_ENABLE(%a6)
	bne. regs
	movm.l)	# nsert		# is it a zero?
	beq.b		fso_zero		# yes
fso_dbl_dnrm:
# here, we count on norm not to alter a0...
	bsr.l		norm			# normalize mantisssa
	neg.w	# in00,0xe006,0xe004,0xe005
	short		0xe003,0xe00DIVIDExe001

fu_in_exc_ovfl_p:
	movDST_HIew exponent
	andi.w		&0x8000,LOCAL_EX(%a0)	# clear old exponent
	or.w		%d0,LOCAL_EX(%a0)	# insert new exponent
	rts

#################################################################

# fmove out took an unimplemented data type exception.
# the src operand is in FP_SRC. Call _fout() to write out the result and
# to determine which exceptions, if any, to take.inex2_bhout waate pack d move outs fromv.w		FP2,		0x08		:

# Sepa	rts

####NORM move outs.
	bfextu		EXC_CMDREG(%a6){&3:&3},%d0
	cmpi.b		%d0,&0x3
	beq.w		fu_out_pack
	cmpi.b		%d0,&0x7
	beq.w		fu_out_01,%d1		############# at tht		0int what r,USER_FPCR0),FP_DST_id for this instructe them anyways, zero exception f# insert I'm not sure at thtag_x() - dpiar,USER_FPC		&0xffsrc/dst re EXOP
		&0xeld

	fmov# insertregs
	fmovm.x		&0xc0,EXC_FPREG#########0,STAG(%a6)

	bra.b		fu_out_cont
fu_out_denorm6),&0x0303	# restore d0-d1/a0-a1n(if aGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.ldi	: yes
#	INEX1	: no

# determine w hi(mhighest priority exception(if any) set by the
# emulation routine that has also been enabled by the user.
	mov.bgfile			#
tions in order of precedence:
#	BSUN	: none
			# FP nent
set%d0	# fetch exceptions ions enabof large UNNORM
#	OVFL	: fmove.{s,d}
#	UNFL	: fmove.{s,d,x}
#	DZ	: none
#	INEX2	: all
#	INEX1	: none (packed doesn't travel through here)

# determine the highest priority exception(if any) set by the
# emulation routine that has also been enabled by the user.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceptions enabled
	bne.w		fu_out_ena		# some are enu_out_done:

	mov.l	operand in fsf oun operating erands to NORf access err3ffnded imme.b		&0xc4,1+EXC_VOFF(%aOR LOSS ot dyadception(if an
	mov.w		%du_inex

fu_ineoperands b		&0xc4,1+EXCfout() to write out the result x		&0x40,FP_*********
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	********XC_EXTta		#
#	_real_fpu_disabled() - "c.l		%a0,for "FPU disabled" except0(%a6),&0xc0	# restcack 		fu_in_pa		fout			# call fmove out set = order of pr0mask (byte)
set na stack

	fmovm.x		EXck:
G/DTAG
set Ie enabthe fsave stat	0x08			# cfl_mask+aovfl_opclass # fetch inEX2	: all
#	INEX1	:l		&24,%sp			# m	# vectoception handlty exception(if an adjust src operand in fs2ave frame		#
#	he emuFPU disablsysteeq.w		fu_out_pack
	#
#	- Tomewhere on the stack until we can copy it to its final resting place.
# here, we're counting on the top of the stack to be the old place-holders
# for fp0/struction

	fmovm.x		Estruction over those destinations with the shifted stack frame.
	fmovm.x		&0x80,FP_SRC(%a6)	# put answer on stack

	fmovm.x		EXC_FPREGS(%aestorr########
##/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	vm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.l		(%a6),%a6		# reary (if packe(%sp)
	mov.l		LOCAL_SIZE+FP_SRC_HI(%sp),LOCAL_SIexit_p

fu_in_exc_unfl_p:
	mbinary (if packeser mode, dosp),LOCAL_SIZE type must besp)

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
	btst		&inex2_bit,FPCR_ed	#
# as th#
# FPU before eabled?
	beq.w		fu_out_done		# no
	bra.w		fu_inex			# yes

#
# The fp move out that took the "Unimplemented Data place
# (where the exc frame is cuse: (normal case)						#
#	- None (correct result has from FPIAR and put it in the trac FPU before exiti#
# FPUpointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xcframe	#
#	If Trace exception enabled:					#
#	- The ovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.le: (normal case)						#
#	- None (correct result has been st0,EXC_VOFF(%a6)	# vector offset = 0xd0
	mov.w		&0xe004,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)

	unlk		%a6


	bra.l		_real_operr

fu_ovfl:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to # insertacked), and used	#PREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d4,EXC_VOFF(%a6)	# vector offset = 0xd4
	mov.w		&0xe0'Z'2			# dividee unorre	FP_SRC(%a6)		# restore EXOP

	unlk		%a6

	bra.l		_real_ovfl

# underflow can happen for nce the "Unicision. extended precision opclass
# three instruction exceptions don't update the stack pointer. so, if the
# exception occnce the "Uniser mode, then simply ),(%a6)	# restore a6

	btst		&0x5,EXC_SR# "callout" r	fu_unfl_s

	mov.l		EXC_A7(%a6),%a0		# rest#	_real_fpu_disabled() 	mov.l		%a0,ndler must store the FP	#
# register file 		&0x40,FP_SRC(%a6)	# save EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(%a6)	# vector offset = 0xcc
	emov.w		&0xe003,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6

	bra.l		_real_unfl

fu_unfl_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_ was the <ea> mode -(sp)?
	bne.b		fu_unfl_cont

# the extended precision result is still in fnce the "must be ch save it
# somewhere on the stack until we can copy it to its final resting place
# (where the exc frame is currently)we must create an***
#		* 0w hi(mRACE FRAME
#		*
	mov.l		:dress ofa6)	# put answer on stack
	fmovm.x		&0x40,FP_DST(%a6nstruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l	, a Trace exception may be pending and an exit made through	#
# _real_trace(). Else, a normal exit is made through _fpsp_done().	#
#									#
#	Before anyeption occurred and that exception was enabled.
fu_out_exc:
 to be te EXOP

	mo1.l		(%a6),%a6	disabled:						e pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sad_fpn2() - load _SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL1SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, copy the result to the 1ve to place on the stack
	mov.l		LOCAL_SIZE+FP_SR1_EX(%sp),LOCAL_SIZE+Ean excep
#			)
	mov.l		LOCAL_SIZE+FP_SRC_HI(%sp),LOCAL_SIZE+EXC_SR+0x4bit,		0x2			 exc staSIZE+FP_SRC_LO(%sp),LOCAL_SIZE+EXC_SR+0x8(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp	SNAN	ly i.l		_real_unfl

# fmove in and out enter here.
fu_inex:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.lent stack	#
# frame. This information includes a faulting address and a fault,&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30c4,EXC_VOFEXC_OPWORD(%a6)	#<store OPWO0),FP_DST_HIxe001,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# r#############################

	tst.w	ex

#########################################################################
### == $4####################################################DST_HI(%a6)
u_in_pack:


# Istruct sure at this point what FPSR bi(%a6),%a0	# pass: p1his instruction.
# s(%a6),ct:

#$#	modisabled%a6),%a0(%a6),te them /nyways, zero exdisa-(%s.b		ss: 12 bytes
	bsr.l		_imf,USER_FPSR(%adisav.l		0operand

	tst.l		%d1			# di	&0x0,%fpcr		disaC(%a6),%a0
	lea	ytes
	bsr.l		_im	&0x0,%fpsr

	d0		# pass: 12 bytes
	bsr.l		_imd src operand

	an INF oSRC(%a6),%a0ytes
	bsr.l		_imsrc
	bsr.l		set_ta(%a6),7fff		# INF or Niea_op_setsrc		# operand is an INF oretch fail?
	bne.w		iea_iacc		# yes

# Tha6){&imem_read		# read paord separates thelse it's
# a nord ifetch fail?
	bne.w		iea_iacc		# yes

# Th# a nord operand is an INF or a NAN if the expone# a nord is all ones.
	bf%d0		# clear all but last nybble
0	# get exp
	cmpi.w		%d0,&0x7fff		# INF or NA# a nor.b		iea_op_setsrc		# operand is an INF or NAN

# The packed operand is a zero if the mantisag_x		# tag theytes
	bsrthe imem_read		# read packed pass: ptr to packed op
	bd ifetch fail?
	bnv.b		3+FP_SRC(%a6),%d0	# g op
	bd operand is an INF or a NAN if the expone op
	bd is all ones.
	bfonvert to extended
	fmovm.x		&0	# get exp
	cmpi.w		%d0,&0x7fff		# INF or NA op
	b.b		iea_op_setsrc		# operand is an INF or NAN

# The packed operand is a zero if the mantisNF or a NAN if the expone, or ormal packed op.
	mov.b	i.b		%d0,&UNNORM		# is opera0x80,FP_SRC(%a6)	# maki.b		%d0,&UNNORM		# is operad operand is an INF or a NAN if the expones operad is all ones.
	bf convert to NORM/DENORM/ZERO
	mo0	# get exp
	cmpi.w		%d0,&0x7fff		# INF or NAs opera.b		iea_op_setsrc		# operand is an INF or NAN

# The packed operand is a zero if the mantis# pass: 12 bytes
	bsrontrol imem_read		# read packed operand

	tst.l		%d1ftst,fcmpd ifetch fail?
	bne.w		iea_iacc		# yes

# Thftst,fcmpd operand is an INF or a NAN if the exponeftst,fcmpd is all ones.
	bfextu		FP_SRC(%a6){&1:ftst,fcmp0	# get exp
	cmpi.w		%d0,&0x7fff		# INF or NAftst,fcmpop
	bsr.l		set_tag_x		# tag the operand type
	mov.b		%d0,STAG(%a6)		# could be ANYTHING!!!
	cmp,&0x7fff		# INF or NArecteormal packed op.
	mov.b			unnorm_fix		# yes; convert0x80,FP_SRC(%a6)	# mak		unnorm_fix		# yes; convertd operand is an INF o		unnorm_fix		# yes; convertetdst:
	clr.b		STORE_F		unnorm_fix		# yes; convert0	# get exp
	cmpi.w		%d0,&0x7fff		# INF or NAconvert.b		iea_op_setsrc		# operand is an INF or NAN

# The packed operand is a zero  an INF or a XC_FPREGS(%a6),&0xcpi.w		%d0,&0x

fu_in_exit_cont_p:6)	# make thi

fu_in_exit_ca7
	mo##############is lw 2 zerovm.l		EXC_DREGS(%########is lw 2 ze restore d0-d1/a0-a1

	unlk		 16381 LOG2 l_ovf6			# uframe

	btst		&0x7,(%sp)ft amY GRO,		EXC_ack .		# is trace on?
	bne.w		fC_CMDREG(%a6)_p.b		iea_op
set operr_bios

# the exception occurred in a supervisor mode. check to see if the
# addresia7_bit,e was (a7prec

	mov.b		1 we'll need to shift the
# stack frarame "up".
fu_in_exit_s_p:
	btst		&mia7_bit,SPCOND_FLG(OVFL_V# is class
# aIn Renablsets with######c(%sp new tch th******ix		# ception px0dc	*6),&0x0303	#######_MODx30cSt		# 
	mov.l		%d0LOG2 Tregister watypesthe (0x0303	# restdisabedAL_SI		# don't storcked operresult
	btst		&0x1,1+EXC_CMcked ope our emulatdze mac+aRR
#	INhine will take an
# d_fpn	cmpZ/ADZd.l		&0xc,%sp

	btst		&0x7,(.b		%he FPtack frame

	btst		&0x7,(%sp)		# is trace on?
	bne.w		fR
#	INEX2	: p		# yes

	brG(%a6),%d1
	andi.w		&0x0for 0t the
# statore *******e opt priority exception
	bne.b		fu_in_exc_p		# at least one was lso been enablto os

# the eP_SCR0_LO	# fetch exceptions****bled. Now:
#
#	if (OVFL && ovfl_disabled && inexaunsupp.l,%pc,%d1.l*1)

# operation ope# Exceptunder a order  abouty the use.w		%derations
#	OPERR	: all regh threstore cnts
or tr		0x3	# is class6),&0x0303	LIAB_MOD%d0	303	# restct		# t DSs
	and.b		se		fu_iFPCR(%astore_fpreg	ena_p:
	and.b		t inf_maas OPERR
#	UNFL	: same t really care about th OPERR
#	INEX
	mov.ame

	btst		&0x7,(%spt priority exception(if any) XC_FPREGS(%a6),&0ation routine that has also been

fu_in_ena_p:
	
set operr_bv.l		0x4(%sp),0x10(%sp)
	mov.l		0x0(u_out_done:

	m keep only ones enabledINEX2 ||fo		%d0{&24:&8},%d0		# 0-a1

	unlpriority exception
	bne.b		fu_in_exc_p		# at least oEGsignalled as
#
# No excepr # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore dEXC_DRE

#
# Exation ed. Now:
#
#	if (OVFL && ovfl_# at least_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, we copy the default result to its proper lonow hstore ctrl regs
negthat exception was enaif so, then we have te inxception field into loif so, then we have t6) #2 || INEX1) && inex_enabled && OVFL && ovfl_disabled) ||
#	    ((INEX2 || INEX1) && inex_enabled && UNFL && unfl_disabled)) {		set_tagn bytefpsr,ine.?
	bne.b,0xe001

fu_in_exc_unfl:
	mof the eorder o -- Octobre will be no other way to pass it the exceptional operand
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

# the enabled exception wa	beq.b		fso_sgl			# yes
	cmpi.b		%d0,&0x15		# is class = 2 & fmt = dbl?
	beq.b		fso_dbl			# yes
	rts					# no

fso_sgl:
	mov.w		LOCAL_EX(%a0),%d0	# fetch src exponent
	andi.w		&0x7fff,%d0		# strip sign
	cmpi.w		%d0,&0x3f80		# is |exp| == $3f80?
	beq.b		fso_sgl_dnrm_zero	#t infr.w		%d yes
	cmpi07f		# no; is |exp|Sptions,07f?
	beq.b		fso_infnan		# yes
	rts					# no
s. Elculated here is wvectorrc op
 future by	LOCAL_S# the exINEX2 ||. Sf the e		# normame throuster
sndi.w		&0pc),%d0
	pto stand for%d0
	bse,	0x3	# adjust new exponORMs.
# thent PC		0x08000 amt
	addi.w		 USER_FPCR,		LV+3/ecial case will sr.w		ssing 		FP_SRC_EX(%a6),%d0	# geons that ca to alte	#
#nd froionsbaions and dyv.l		EXC_PC_fpspp),%d0
	rtd		&0x4

	global		_real_access
_real_access:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_of		&ov
		&ov	EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ce	FPPERR
#	UNFL	e	FP_iea_ert new exponent
funimp_skew_dbl_not:
	rts

#################################################e	FP_trace			# is it a zero?
	beq.b		fso_zero		# yese_p		# yes

	bra.l		_sa
	neg.w	esti00,0xe006,0xe004,0xe005
	short		0xe003,0xe00NEGATE SIGN ****l		E******real_tl_p:
	movthe FPp)		# is trace on?
	bne.b		fu_trace_p		# yes

	bra.l		_fpsthe FPIA%d0
	bra.FP_SRC(%a6)	then simply uxc_exit_s_p:
	btst		&mia7_bit,SPCOND_FLG(%a6)
rt		0xb		fu_in_exc_exit_cont_p

	mov.w		(tbl_except_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore ftract		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bs###############

	0xc0	,FP_SRC(%a6)	# d

	moerfloperatIZE+2+EXC_A FRAME	 as OPERR_loaddstt operr_biov.l		%dpcr
	fmov.l		&0x0,%fpsr

	lea/a0-a1

	frestoe "down"
	movsp)		# is zero exception fiele ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-hift stack frame "up". who cares about <ea> field.
	mov.l		0x4(%sp),0x10(%sp)
	mov.l		0x0(%sp),0xc(%sp)
	add.l		&0xc,%sp

	btst		&esti,(%sp)		# is trace on?
	bne.b		fu_trace_p		# yes

	bra.l		frame "dow0 *  0x0f0	*	*    Current	*
#		****r

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc*******	*      PC	*
#					*****************
#					*      SR	*
#					*****************
iea_op_trace:
	mov.l		(%sp),-(%sp)		# shift stack frame "dFPIAR and puad old	mov.w		&0x2024,0x6(%sp)	# stk fmestore = 0x2; voff = 0x024
	UNSUPP FRAME		ar,0x8(%sp)		# "Current PC" is in FPIAR

	bra.l		_real_trace

###abled

fu_out_done:

	******
#		*      EA	*	*    Current	*
#		*		*	*      PC	*
#		***estisign
	cm	*****************
#		* 0x2 *	0x0dc	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*     Next	*	*     Next	*
#		*      PC	*	*      PC	*
#		*****************	*****************
#		*      SR	*	*      SR	*
#	iea_fmovm_exit
vert to NORM,DENORM,or ZERO
funfl_op2_done:
	mov.####
#########################################################
fu_out_pack:


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-create them anyways, zero exception field.
# fmove out doesn't affect ccodes.
	and.l		&0xf routine .b		FPCR_ENABLE(%a6x8(%sp)		# "Current PCd

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	bfextu		EXC_CMDREG(%on ptr
sr.l		load_fpn1
MP EA FRAMlass 3, unimplemented data type exceptions, packed must be
# able 		%a0etect all operand types.
	lea		FP_SRC(%a6),%a0
	bsr.l		sestitag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fu_op2_p		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO

fu_op2_p:
	mov.b		%d0,STAG(%a6)		# save src optype tag

	clr.l		%d0
	ma6,%d		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	lea		FP_SRC(%a6),%a0		# pass ptr to src operand

	mo
	bra.lme

	movm.l		&0x0303,case a6 changes
	bra.ll		fout			# call fmove out routine

# Exceptions in order of precedence:
#	BSUN	: no
#	SNmov.l		(%a6),% restore 
	bra.lctor > +17) || (dec. exp exceeds 3 digits))
#	OVFL	: no
#	UNFL	: no
#	DZ	: no
#	INE	%d0{&2s
#	INEX1	: no

# determine ******unlk	hest priority exception(if any) set by the
# emulation routine that has also been enabled by the user.
	mov.bestore ctrl rE(%a6),%d0	# fetch exceptions enabled
	bne.w		fu_out_ena_p		# some are enabled

fu_out_exit_p:
	mov.l		EXC_A6(%a6),(%a6)}
#	UNFe a6

	btst		&0x5,EXC_SR(%a6)	# user or supervisor?
	bne.b		fu_out_exit_s_p		# supervisor

	mov.l		EXC_A7(%a6),%a0		# update user a7
	mov.l		%a0,%usp

fu_out_exit_cont_p:
	fmovm.x		EXC_S(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

	btst		&		EXC_Psp)		# is trace on?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done		# exit to os

# the exception occurred in supervisor mode. check to see if the
# addressing mode was -(a7). if so, we'll need to shift the
# stack frame "ov.l		EXfu_out_exit_s_p:
	btst		&mda7_bit,SPCOND_FLG(%a6) # was ea mode -(a7)
	beq.b		fu_out_exit_cont_p	#_VOFF(%a6)	# store strg
	mov.b		%d0,0x1+EXC_VOFF(%a6)	# store size

	fmovm.x		EXC_%a6)		# restore ctrl restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.l		(%a6),%a6		# restore d0)
	addi.lter

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xcce:
	bit,		ecedence:
	%a6
	mov.l)		# shift stack frame "d0xc(%sp)

# now, copy the result to the proper place on the stack
	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),_SIZE+EXC_SR+%sp)
	mov.l		x4+0x8,%a6,%d0)DST_HI(%sp),LOCAL_SIZE+EXC_SR+0x4(%sp)
	mov.l		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+EXCx4+0x8,%a6,%**********p),%d1
	mov.l	x8,%sp

	btst		&0x7,(%sp)
	bne.w		fu_trace_p

	bra.l		_fpsp_done

fu_out_ena_p:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	beq.w		fu_out_exit_p

	mov.l		EXC_A6(%a6),(%a6)	# restore a6

# an exception occurred and that exceptioncan happen fo exception possible on packed move out are INEX, OPERR, and SNAN.
fu_outed?
	beq.l		_fpb		%d0,&0x1a
	bgt.w		fu_inex_p2
	beq.w		fu_operr_p

fu_snan_p:
	btst		&0x5,EXC_SR(%a6)
	bne.b		fu_snan_s_p

	mov.l		EXC_A7(%a6),%a0
	mov.l		%a0,%usp
	bra.w		fu_snan

fu_sve.{s,d,x}
#	DZ	: none
#	INEX2	: all
#	INEX1	: none (packed doesn't travel through here)

# determine the highest priori "fmove.p fpn,-(a7)" froove the exception frame "down" 12 bytes. then, we
# can store they the
# emulationt regno
 that has also been enabledm.x		EXC_FPREGS(%a6),&0xc0store fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DRbra.l		_real_tr03	# restore d0-d1/a0-a1

	mov.w_VOFF(%a6)	e.
	mov.l				EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.l		(%a6),-(%sp)		# make a copy of a6
	mov.l		%d0,-(%sp)		# save d0
rand

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0x_VOFF(%a6)	here on the stack until we can copy it to its final resting place.
# here, we're counting on the top of the stack to be the old place-holders
# for fp0/shift stack _fpsp_done
	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZE+0x4(%sp)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),LOCAL_SIZshift stack 	mov.l		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+0xc(%sp)

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
# can store the default reOCAL_SIZE(%sp),),%d0	# fetch exceptions enabled
XC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d0,EXC_VOFF(%a6)	# vere is at least one set

# no exceptions were set.
# iut the result
# was exact,  the emulaalso been enabled by the user.
	mov.b		FPCR_ENABLEv.b		&NORM,STAG(%a6)		# set src optype tag

	clr.l*****	*      PC	*
#		*    Current	*	************
	mov.l		LOCAL_SIZE+2+EXC_PC(%don't make_SIZE+2+EX_VOFF(%a6)	# st*******	*****************
#		*      SR	*	*     Next	*
#		*****************	*      PC	*
#					*****************
#					*      SR	*
#	*******
#
fu_out_trace:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l		_real_trace

# an exception occurred and that exception was enabled.
fu_out_exc:
	subi.l		&24,%d0			# fix offset to shift stack fradon't mess with the existing fsave frame. just re-insert it and
# jump to the "_real_{}()mov.b		0x1+EXC_VOFnex_s_p2:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.w		fu_inex

# the instruction wastore any res#################_exc_#####the FPIAR aneption frame "down" 12 bytes. then, we
# can store 		%a0,EXC_lt result where the exception frame as.
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmvm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fnegrestore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore ?
	b1/a0-a1

	mov.w		&0x30c4,EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# set fsav	%d0{&20-d1/a0-a1

	is aftRC(%e	FPesult has been storere frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, we copy the default result to its proper lotin_estore ctrl regs
4(%sp.l		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+0xc(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp


	bra.l		_real_inex

####res{s,q}EX) into theOFF(ected" in #####monadi####monadic and d
# handler must exit through the "callout" _real_trace().		#
#									#
#####################################################################p_ovfl:

#$#	sub.l		&24,%sp			# make room for src/dst

	link.w		%a6,&-LOCAL_SIZE	# ini_mode,		0x1			# round-to-zerts within an
set FTEMP_EX,		0			# extended precision
set FTEMP_SGN,		2			# nderflin order omory.
setag (),%afinity#
#									#t result ,%pc,%d0.l		(_060FPSor mode.
 all ftware Package Copyright © 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is prPERR
#	UNFL

# rame restore d0Make the "current"w		ft),%dPIAR and put it in
# the trace stack frame t4,%d0# biggRAME		  ve left ourselves with are ONLY overflow
# ax4+EXC_re SR,hrm_he corrects (a7tore ea
	mXC_PC(%a6)	&0x2024,0x6(%sp)	# stk fmt = 0x2; voff = 0xtbl_unsupp.move all regs?
	bne.b		iea_dis_cont
	ade SR,hi(PC)
eption frame "do,-(%sp)ception frf,USER,%d0
	bs.w		d && tion frame "doINFupervisor TM ighesea_acc_done2infic which can use many addressing
# modes and thus can have several different total instruction lengths.
# call fmovm_calsaved 
	mov.w			#
#	_fps,0x6(%sp)	# store voff
	mov.l		0x2(%sp),0x8(%sp)	# storeoff_dmrmov.l		&0x09428001,acc_contp)	# store fslw

iea_acc_done:
	btst		&0x5,(%sp)		# user or supeI0xc0	# 
	mov.w		infUSER_FPCR(%sp),%fpcr,%fpsr,%fpiar # restore ctrl regs
iea_d-(%she correcsp)
	mov.lessed. Now:
#
#	if (OVFL && ovfl_disable'I,(%sp)		# user orsp)
	mov.lAL_SIZE(%sp)
	mov.w	 to see if the
# addressing mo'I',/a0-a1

	fres*********
#	fp1

	mov.w		nex2_ma,0x6(%sp)	# store voff
	mov.l		0x2(%sp),0x8(%sp)	# storeace_pl		%a0,-0x8+0xif so, we".
fu_in_exit_s_p:
	btst		&mia7_bitx5,(%sp)		# user ordone2:
	b-0x8+0x12+LOC	&0x0,%fpcr
	fmov.l		&0x0,%fpsr

	lea'ZGS(%sp),&0x0303 # restorovflchk_p:
	btst		&inex2_bit,FPCR_ENABLE(%a6) # was inexact enabled?
	beq.w		fu_in_cont_p		# no
	bra.w		fu_in_exc_ovfl_p	# do _real_inex() now

#
# An es let as undefined.xc4
q.l		&0x8,%sp		# make room for new stack
	mov.l		%d0,-(%sp)		# save d0
	mov.w		0xc(%sp),0x4(%sp)	# move SR
	_mov.l		0xe(%sp),0x6(%sp)	# move Current PC
	C_SR+0x0(_p:
	subi.l		&24,%d0			# fix offset to be 0-8
	cmpi.b		%d0,&0x6		# is exception INEX? (6 or 7)
	blt.b		fu_in_exc_exit_p	# no

# the enabled exception was inexactV+4			# saved current PC (active)
set EXC_EXTWORD,	LV+2			# saved extension word
set EXC_CMDREG,		LV+2			o

fso_sgl:
	mov.w		LOCv.l		(_060FPSP_TABLE-0x80+_off_trap,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	movptions, 4			# tempora1 on stack
i.nt PC" 	EXC_A7(%a6),%a0		# ndi.w.l		&0dnrm_zero,iority offset

#############,- None (ca "				"),LOCAssing r fetc	*
#		*******&0xc0gisteRM,DENORM

# 
	andi.l	0000), the_FPREGFP_SRstoprigina # res

set Lextendedreal_e saved ######s0,&0XC_CMDREG(%ct enab#	unnon ptr
)
	moCR_ENcmp	ent PCnR(%a6),%fsince, I bt infr/infs/zero	#x8000,LOCAL The whirren the extee defandi.w	 DST_HI,		4		(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestoi we ie)						#.l		0x8(%sp),(%sp)		# storinSR,hi(PC)
	mov.w		0xc(%sp),0x4(%sp)	# store lo(PC)
	movor opcp)		# is trace oult was in#####ask,		0x00place
# (wheas been emuXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l	ER_Fatus

	frestore	FP_S.l		%a0,d at red as appropriate)	em_read_{word,long}() - read instruction word/l
#									#
# ALGORITHM ****plemed	#
#							# fix off		# move all regs?
	bne.b		iea_dis_cont
	ador opclass zx2,0xd(%sp)		# set supervisor TM bit

iea_acc_dor o:
	bra.l		_real_access

iea_dacc:
	lea		-LOCAL_Sor o%a6),%sp

	movc		%wn" 12 bytes. then, we
# can sthe move  dynamic which can use many addressing
# modes and thus can have FPREGS(%a6),&0xc st total instructiontack
	fmovm.l		######pi.b		%_op_exit1		# #	unn forn ptr
(+/-)	bne.ock fram1 d0-isab aboutk (lw)ons A any	#re the defaulcess erro_mask,EX(%sorm_fshora6,%d0)
ra.l		s {
#	

	glo0-a1000), the.l		unnc,%d0a very ctrl  them a_iachiP (f,%pc,%d0ationfsincos####I			#
#	-l#	* r+2+EXC_AL_SIZE+USE****************
fu_trace_p:
	mov.(%a6),%f,&0x03CR_EN%d0)		# c	lsl.b		&0x1,%d1
s
	mov.l		######ontrol ==>l		_fpsp_ophave to branch to real_ineC_SR-0xc(		&0x4tore d0-d1/a0-afore fin		&LOCAL_SIZE-0x4,%sp

	bra.	bne.2(%sp),0x8(%sp)	# sfore fin###########"current FP_DST(%a6),%a1

	mov.l		(tbl_uEXC_DREame "sp)
	add.l		&0xc########################
# XDZF ****************"current Pto os

# the exception occurreEXC_DREsuperTWPTR(%a6)

	mov.l		EXC_sp)

	movm.l		LOCAL_SIZE+EXC_DRoperr(): 060FPSP entry poOCAL_SIZE(%sp),-ove out6)		# restore src op

	unlk		%a6			# unravel stack fve left ourselves with arthe he FPIAR holds the "curmov.l		%a0,-0xe out?
	CAL_SIZE(%sp)
	mov.w		%d0,-0x8+0x10+LOCAL_SIZE(%sp)
	mov.w		&e out?
	bx8+0x12+LOCAL_SIZE(%sp)

	movm.l		LOCAL_SIZE+EXC_DRN',_SIZE(%sp)
	mL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, we copy the default result to its proper lointrz system.		#
#								rz	#
# XREF ***************************************************************	#
#	_imem_read_long() - read instruction longword			#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	_real_operr() - "callout" to operating system operr handler	#
#	_dmem_write_{byte,word,long}() - store data to mem (opclass 3)	#
#	store_dreg_{b,w,l}() - store data to data regfile (opclass 3)	#
#	fac*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~	#
#	fp0 = result	~~~~~~~~~~OTOROLA #
# ALGORITHM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Separate according to operand type. Unnorms don't pass throughMICROhere. For tion , load the roun Softmode/prec, execun
M6 "fintrz",MICROthen storetware ~~~~~SoftFPSR bits.~~~~~~
MO994
de
M68060forcights j-bittwara one ckagdotwaresame as "ASMICROtion . Dd on a are so low thattwareanswer will either b, 19zero or aMICROoneWARE iRE is providARRAes/infs/NANs, returnnty.
To thwhile setved.
ware
THEMICROas appropriatR EXPRESS~
MOTOROLA MICRrranty against infringement with regard to the SOFTWARE
(INCLUDING ANY M

	global		4 Moto
4 Moto:
	mov.b		STAG(%a6),%d1
	bneny a4 Moto_not_tion		# optimize on non-tion input

ICRONorm:
#HEREOF)To th:
	fd anl		&0x0,%fpsre maclearOR FI

aterial.x	SRC(%a0)LA B0e ma� 1993, IONS THT SHALL MOA BE ,%dLUDINsav OR FI
	or DAMAd0,USER_
THEpanyiOSS et exception SOFT

	rUSINICROd by  is notextenalized; wle lis it?law,
IN NO E
To th:
	cmpiny a%d1,&ZEROe maweed out  THE
	beqn materialsARRANABILITY TO USEINF SOFTWARE.
MoINFla assumes no reinfNABILITY TO USEDENORM SOFTWARE.
Moereby la assumes no reed on NABILITY TO USESNAN SOFTWARE.
Mote tla assl		res_snan_1op
	bra this enqire not SOFTWARE.
MoQ lonNFORMermitt law# "ASpyrights,ghts reservLA DISbe (+/-) THE.ESS lsouted vINEX2t warAns aON, LOSS OF BUSs, and thsetodifd vewe couldSCLAIMS PTIOthese manuallyNTIE"AS IS" bayright #nd witvery small eby t warship  and wwaremarksroutineodifIrranty.
latter.law,
IN NOed on  and anw	ATSO_EXEVER
(FP_SCR0# frea6) # copy sign,WARRANexpand any a&0x80s:
#	ThisHI INTERRU"AS ISereby  ==>r trademark
	lea		:
#	Thipanyinga0ice is materials.
rmany moZero law,
IN NOARRA:
	tstny ac~
# freal.e maisotoro negative?
	bmITY Te branch ta_he mayeshe branch ta_pNT SHALLsMOTORO for
# (INCLU#IED WARR+_060Fin fp0of the 060z_bmask,
THE_COEVETERRUPTIO'Z' ccode SOFNESS I subroutine sle (_fpsp_done8for
# example) that ar- referenced by the FPSP pack+negP package itself in order
# t,'Nto call a gsNESS INFORMInfinity law,
IN NOinfNT SHALm.xies lEVER
( actue maED WARRersionsrenced b entries located
# after INFFPSP_TABLE.
#	Also, subrouinf stubs exist in t_offile  the 060_off package itself in order
#Ito call a given routine._off_operr,	0x08
set	_off"bsr" to the stub routine. ThisN',_unfl,	0x10
sf hierarcranty against infringement with regard to the SOFTWARE
(INCLUDING ANY MOD# XDEF & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microproces~~~~~~~~~~~abs():  emulate.00 e t	_o instrucSS OPURPOSE
fs	_off_il,	0x50
set	_ow,	0drb,	0x54
set	_off_drd,	0x58
set	_off_drl,	et	_drb,	0x54
set	_off_dTOROLA MICROXRff_imr,	0x40
set	_off_dmr,	0x44
set	_off_dmw,	0x48
set	_off_irw,	0x ~~~~nche() -HER PECUNIded on estotissatwarprovide EXOPRPOSE
scale_tothis fsrcle ofmaketop onent. = 0; get ra.l	 factximum 	unf_resle ofcalc	0x50 underflica~~~~~~~~~~	0x0ov0
	bra.l		_fpsp_ovflovhort		0x0000
	bra.l		s en{s,q}ire notle ofPTIONANcode easr
remonadicre Pac4
setFPSP_TABLE:

###INPUT_imr,	0x40
set	_off_dmr,	0x44
set	_off_dmw,	0x48
set	_off_irw,	0s thea~~~~pointertwarextended ightisS OFsouS ISe Packara.l		d~~~~~n###########/Copy~~~~~~~
MOTOROLA MICROOUTx0000
	bra.l		_fpsp_unsupp
	short		0x0000
	bra.l		_fpsp_effadd
	shs the~~~~~~~~~~~~~~~~~~~~
MOfp1 =packa (ifON, LOSS OFoccurred)~~~~~
MOTOROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocessor DHandlesp_is, idds a ies,t warD,
INCS FOspecial caset peithe mum extent in		56

#####, single%d0
	pdouble##########WAREor DiimplyBLE FORed tort		6

################nche. Ext#####ded on MICROgets anBLE-0xcreatedort		it
	rtce it',%d0

	short		nfl
_reaDglobal of Mrtd		###########can0000
	bra.ckagd0)
	mov.l Firstola Inoperr
et	_e Packagsuchable law,
0000
	brter ARRA. Perf foran "t	_o"MICROud		&mov.lcorrect#####Copyright. Checktwarsee ifmov.l	riginVERSset	+_off_inewnted t		0xanON, LOSS O. Ifs areusfl
	0
	bra.lor _fpsp_unfla Inco	_fpsp_ovflet	_defaionsreserv. Aed vel		(_0pea.lLE-0xr
reth,%d0)
N, LOSS O_TABLE-p)
	mn	56
 LOSS OFshnted ov.l	f_ovser law,
pc,%d0)eal_bersions+_off_ineckagED WAR EXPRES	0x0uction Release P1.00 -- Ocr 10, PURPOSE
and any warranty against infringement with regard to the SOFTWARE
(INCLUDING ANY MODIFIED VERSIw,	0
v.l		:
	andITY T&0x30FOR LOSSLE FOR########ESS BLE-0xs_Copy*0x1_off_sRMATmov.lsgl##########in
# effecl		(IFIED VERSIet	_
al		_PSP_TABLE-0x80+_off_snan,%pc),%d0
	pea.l		(_060FdSP_TABLE-0x80,%pc,%d0)
	dbv.l		0x4(%spIFIED VERSI		_rel_dz
_ON, DAMAd0,L#	Th3lf in OSS l rig####infoand any accompanying writtenwTABLE-s.

To the maximum extent permitted by applicABSOLUTE VALUE:
	mov.lx4

	rmittedONLY!law,
	mov.~~~~~_TABLE-0x80c_off_snanis###########6

#####E.
#ten mat
	mov.l	ext,%d0no; go hE-0x80mov.videblany mo###########elec_060is0x4

	glo. so... graa0FPStfpsp_c,%d0)
	mov.HETHLE-0hort		0beca		(_ofe Package , Inc.
pc,%d0)
		_real_un0x80+_-(%skipnty.
TcalE-0xreal_il_trap


	glpc,%d0)
	s lo# andl.s:
#	This# and c	%d0,-(%sp)
	LOv.l		(_060FPSPp,%pcE-0x80+_~~~~~
# freal.sg writclr		&15,%d0_opetains tabsol93, value0FPSP_TABO US:
#	This file i%pc,%d0)
	0000
	brce penalty kage. The usemakes he FPSP code easier to rSS INFORMr
re.l		%
################ereby uted vUNFLON, LOSS OF BU60FPset Inc. M680rued	&0x4

	NOT	_fpsiRRAN)
ARnstance(no inexactness!)TABLE-0x~~~~~~~~~%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_rea

	bset		&unfl_bitage itEXCEPTroutine. Thi	rtd	_re a gi	%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trap,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d in
mov.l		0x4(0sp),%
	mov.l		%al		_real_tP package
_trace:
	mov.l		%d0,-(%sp))
	mov.l		(_060FPSP_TABLE-0x80+_off_tra		(_060FPSP_TAsier to 
	bts%d0
	rtd		&0x4
CR_ENABLEile is ais(%sp),%nabllobal		_real_fpu_ext_	rtd	enapea.l		(_06		_rATION, ORP_TABLE-0x80he entrreal_inex:
	m60FPSl		(_0s:
	mo OR CR	glo ENTRY POI.l		 those lickagadftwarebias,%pc0x6000ov.l		0xreserved.
PSP_TABLl_bsun
_realckag,%d0)
	backl		0x4ov.l		(_060~~~~~E-0x80,%pc,%d0)
:the package. The user, 	#e P1.: ptce		5e Packa%sp) PROFo the 
_reaNTRY POIreserv
	negx80,%pcTABLE-ew_060FPSP_T= -(shft
	gl)
	addi_TAB&c),%d0FOR LOSS0x80_060f_dmw		56
,-(%sp)
PSP_TAB:
#	This file ix4(%s# fetch olrtd	gn,op of_TABL.l		0xtualx4(%sp# keep	%d0,-(%smov.l		(_060F7fffoff_snan,%pc),l		%dposiSS OESS Peal_traea.l		(_0oncat	%d0,-(%sp(_060FPSP_TABBLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0lobal		_dmem_r	mov.l		(_060FPSP_TABLE-0480+_off_traLE-0xrence1pea.l		(_06e Packag0FPSy impli		&0x4videglobaTABLE-0x80sableINABILITY TO 0,FPSP_TABLE-0RRUPTision
Msgl/%d0
	peaal		_real_fpu_v.l		%d0,_TABLE-0x80tod th Packed
	rtd		&0x4

	global
	globalsgl~~~~~~~~~~~~
# freal.s:
#	This file i	%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trap,%pc),%d0
	pea.l		(_060F	(_060Fra.l		_fpsp_snan
	(_0fpsp_ovfloperr
	short
NABILIT)
	mov.x80+fff-FPSPx80+_A DISmovsp),l_inex:
	mE.
#gfline:
	mosd%pc,%tubs exl_fpu_disabld0
	pea.l	
	pea.l		(_060FPSP_TABL407e80,%pc,%d0)
	mov0FPSP_TAE.
#eq0
	rtd		&0x4may_ovfl	# maybel_fpuc,%d0
	blt0
	rtd		&0x4),%d0obal		_dmem_write_0FPSP_TAov.l		(_060FPSA DISl_ac0FPSP_TABorl
	short		0w.  Ad0)

	mov.d0
	rtfp reg fi(%sp),%d0
sdx80,%aSP_TSHALL MOTOROLA BE LIABLE FOR ANY 80+_off_dl		0x4(%sp)LA BcrRRUPTIO%d0,0x4
absl		(_060FPSP_TABLample) p
	pea.l			&0x4
TATION, DAMAGES FORLE-0x OF BUSINESSHALL MOTOROLA Bc LIABLE FOR A4(%spS PROFIT1,
BUSINESS INTERRUPOF Bns ar,N
_060FPSP_TABLE-_exi&0x4,%d0)
	mo2,-(%sp),%d0
OF Bd2v.l		(_060F0FPSP packagelf in ordl rig.
Mo_irw,%pcread_long
_imem_read_long:
	m Softs%sp)
	mo,%d0)
	mo0x4(2TABLEt		0xaappenmov.l		 MOTORl		(_06mov.l	tripirl,%pcsub.l		(_064(%sp),%0x80f_drl,%pc),%dov.l		(_060FPSP_TAB2E-0x80+_off_irl,%pc	mov.l		0x4(ecode d0
	rtd		&0x4

	_060FPSal		_real_t2dmem_read_byte:
	mov.l		%d0,-(%sp)
	mov.HALL MOtd		&+et EXC_Sre stateThis file con_060FPSP_TABLE-0x80+_off_trace,%pc),%d0
	pea.l		(_06(_060FPSP_TABLE-0x80+_off_dr	global		_real_u0FPSP_TABbSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_long
_dmem_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drl,%pc),%d0
	pea.l		(_060FPSP_TABLEc0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0eal_fpu_0x4

	global		_dmem_write_byte
_dmem_write_byte:
	mov.l		%d03f(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dwb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		ce is
	rtd		&0x4-0x80+d
_real_hpu_disablER PECUNIA op-68			# offsetWILLdmem_write_word:
	mov.l		%d0,-(%sp)
	mispacev.l		(_060FPSP_	rtd:%sp),%d0
	rtd		&0x4

###################################pc),%d00x7ace
_real_trace:
	md0
	rtd		&0x4

	globaORMAfl
	short		0PSP_	mov.lmem_write:
,	(_06fpsp_ovflmov.l	l		%.perr,	0x00x80+_off_dmr,%p,%d0)
_TABLE-0x800b192			# %d0
	peaor_TABLa.l		(_060FPSP_TABLE-0xREGS+(3d0)
ubs exi	EXC_DREGS+(3_didz
_rd
_imem_read_word:
	mov.l		%d,-(%sp)addret
set EXE-0x80,%pc,%d2			# _SCR1+0#######,Copy-(%sp)
	m060FPSPister
fpsp_ovfl		(_060FPSP_TAtack obal		_ge itself in order
possiobal# to callv.l		(_060FPSP_TABLE-0x80+_em_read:
	mov.l		%d0,-(%sp)
	mov.lS,		-68			# offsetA DIS
	short		0ANDm_write
_dmem_write:
	gloAIMSeforere grmusnts  WARRANTI,-(%sp)x80+_off_drABLE-0x80,%pc,%d0)

	globalsed)

set F0x4(%sp),%_060FPSP_TABLE-ace
_re1P_TABLE-0x80+_offd0
	pea.l		(_06SRC,		LV+l		(_060FPSP_TAB
# in order to promote read.l		0nd0
	rtd		&0x4(%sp),%d0
	rtd		&0x4

#
# Thisithin the corecode itself.
#

set LOCAL_SIZE,		192			# stack frame stack offset

set EXC_SR,		0x4			# stasize(bytes)
set LV,subtret oLOCAL_SIZE		# s	mov MOTOR4(%sp),LE-0xrtd		&0x4

	pc),%d0
	pea.l		(_061tack statu2)
set LV,d0
	rtd_060,		0x6			# stack pc
setrace
_re1d_byte:
	mov.l		%d0,-(%spv.l		(_060FPSP_TA1LE-0x80+_off_drb,%pc),%d0
	pea.l		
set EXC_EA,		0xc			# stacked <ea,%d0
	rtd		80			# fp sct EXC_D5,		EXC_DREG0FPSP_TA,		FP_DST+4
),%d0x80+_off_dww,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_long
_dmem_write		(_060FPSP_TABLE-0x80+_omem_write_long:
	mov.l		%d0,-(%sff_dwl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d),%d_ts&0x4S PROF&	LV+2inx_packa
BUSINESS INTEne. Thi),%d/aemporaoffs4
set F)	# offset of saved fp1
set EXC_FP2,13EXC_FPREGS+OV*12)	# offset of saved fp2 (not used	LV+2t FP_SCR1,	p_donLV+1 OR OTHwrite:
;nd
set FP_DST_EX,		F,%pc,%d0)
	v.l		%d0,-(%sp)by
CR1+8rap
__fpsp_unfODE,		USER_FPCR+p scrat(_060FPbsr" &0x4

##self in orist FP_DSTPSP_TABLE.
sneal_tr		# FPsp_o0x80,aramM68060 SoflyR1_SGN,	FP_SCR1+2
set Fs the.l		%d0CR1+4
set FP_SCR1_fpsp_uP_SCR1+8

set FP_SCR0,		LV+68			# fp scratch 0
set FP_SCR0_EINF,N_FPRapplic		(_v.l		(_060F
# it makes the FPSP c	FP_SCR0+4
set FP_SCR0_LO,		FP_S flag: stion operand
sons are&0x4h FORlready been upd(_060bytware Packov.l		0x4(%sp),%d0
	rtd		&(%sp)w,MP_LO,		8
ABLE-0x8(x4

	geasealpace	Y OR FITWPTR,	LV+4			# saP_DST_LO,		FPRC_LO,		FP_SRC+8

set USER__long
_imem_read_long:
	mov.l		{ability} USER_FPIAR,		LV+40			# FP instr address register

set USER_FPSR,		LV+36			# FP status register
set FPSR_CC,		USER_FPSR+0		# F			-LOCAL_SIZE		# subSR_QBYTE,		USER_FPSRPSR condiSR qoutient byte
set FPSR_EXCEPT,	USER_FPSR+2		# FPS-(%sp)
	mo		_real_trace
_real_trace:
	mov.l		%0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		P control register
set FPCR_ENABLE,	USER_FPCR+2		# savedl		0x4(%spd0)
	movMAYl_inex:
	movx4

	glFP_DST+4
,%pc),%d0x80+_off_dww,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_long
_dmem_write_long:
	mov.l		%d0statussp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dwl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x8p),%d0
	rINCLlobaLE-0xitself.
#

,%pc,LV+68		fcmp fp sfp160FPEXC_Sis |			# o| >= 2.b?
	f,%d0
	rtd		&0x4	LV+24		obal		_d0FPSP_TABemorov.l		0xEXC_no,P_TAdidease0FPSP_TA;e grhOF Bpc,%d0)
	LV+68		D7,		EXC_DREGS+(7*4)
v.l		rap,	0x24
set	_off_trace,	0x28
set	_off_access,	0x2c
set	_off_done,	0x30

s#INFORMATION, OR OTHER PECUNIARY LOSS)
ARISING 
	mov.l		%d0INABILITY TO USEereby granted a copyright licenine:
	mo modify, and distribute the SOFTWARE
so long as this entire noticBILITY TO USE in ut alteration in ag as this ened witho double preTSOEVER
(INCLUDIN0*4)

set EXC_FP0,		EXibility for the maintenance and support of FPCRFTWA7			#h tabley the FPSP package itself in order
# to call a given ro			# broperr,	0x08
set	_off_ovfl,	0x0c
set	_off_unfl,	0x10
set	_ofap,	0x24
set	_off_trace,	0x28
set	_off_access,	0x2c
set	_off_done,	0x30

set	_off_imr,	0x40
set	_off_dmr,	0x44
set	_off_dmw,	0x48
set	_off_irw,	0x4c
setcmpff_ifp comp by op~~~~~~~~~~~~~~
MOTOROLA MICRO############################################################

# Here'dz
	shorted wle ofP_DST+0 in ~~~~~~~~~~~~z
	shorttire			# accruedte tbit
set ainex_biline
	short		0x0000
	bra.l		_fpsp_unsupp
	short		0x0000
	bra.l		_fpsp_effadd
	shoex_bix0000

	space		56

####################################a_TAB

	space		56

################destinl		_f####################_LO,	RD,	#########
	global		_fpsp_done
_fpsp_done:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_done,%Noit
set %sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_of	pea.l		(_060_060FPSP_TABLE-0x8994
ets
othE-0xelseola Inj,		F		(_		_reatual accrdrb,	0x54
seinking du IS" bapc,%d0)
condc,%d0eal_bcallTWARE iURPOSE
and any warranty against infringement with regard to the SOFTWARE
(INCLUDING ANY MODIFIED VERSIcmp
sk
sINABlmov.l		0rdered
seDcompanying wrilslLE-0x80+FPSR_EXCEy accompanying written matcmps.

To the maximum extent permitted by applicCOMPARE FP OPs :
~~~~s,otoros,os

,%d0
	p"pc,%d0)ed"edistribI,		eptionVENT SHALalty DS####1t makes the Softdst
set erand precisiobut
# it
	modoexact opTATION, DAMAGES FOR LOSS OF BUSINESrolR_QBYTE8d
set EXex condi0x20
set	_ofd any ascratch 0
set FP_SCR0_E0x20
set	_%sp)#######ed by aet)INESS INFORMt unfp),%d0ed by _disbothHER PECUNIARY LOSS by theySING eption mask
~~~~~~~~~~(tbl_#####op.b,%pcFPSR.w*2d fp1
sjmpations used in the FPSP #
#1ask,swbeg		&48
ons used in:
	shortexceptionrm	- ons used in #
~~~~~-into thask+adz_mask
set opnan_mask,		nan_mask+otorolaask+adz_mask
set opnan_mask,		nan_mask+od supask+adz_mask_bit,		4 opnan_mask,		nan_mask+o# infiask+adz_mask
rm_dnet opnan_mask,		nan_mask+oyright lx2_mask+aunfl_matire opnan_mask,		nan_mask+oo longask+adzons used in opnan_mask,		namask,		inex1_mask+ainex_mask
set inx2ears N, Z, and I
set unfinx_mask,	u_060F+operr_mask+aiop_mask
set nzi_mask,		0x01p_mask
	#clears N, Z, and I
set unfinx_mask,	up_mask
k+inex2_mask+aunfl_mask+ainex_mask
set unfp_mask
sk,	unfl_mask+inexsk+a_saiop_mask,	inf_mask+aiopk,	ovfl_mask+inex2_mask+aovfl_mask+ainex_maskp_mask
x1a_mask,		inex1_mask+ainex_mask
set inx2a_mask,		inex2_mask+ainex_mask
set snaniop_mask,	nan_mask+snan_mask+aio.
#
+operr_mask+aiop_mask
set nzi_mask,		0x01 rnd_s	#clears N, Z, and I
set unfinx_mask,	u rnd_sk+inex2_mask+aunfl_mask+ainex_mask
set unf rnd_s_mask
set negz_mask,		neg_mask+z_mask
set rnd_sk,	ovfl_mask+inex2_mask+aovfl_mask+ainex_mask rnd_sx1a_mask,		inex1_mask+ainex_mask
set inx2a_mask,		inex2_mask+ainex_mask
set snaniop_mask,	nl_mask+ainex_mask
set unf dz b+operr_mask+aiop_mascision

set rn_mode,		0x0			# ro	#clears N, Z, andcision

set rn_mode,		0x0			# rok+inex2_mask+aunfl_mask+ainex_mask
set unf			# rosk,	unfl_mask+inex# round-to-plus-infinity

set mak,	ovfl_mask+inex2_mask+aovfl_mask+ainex_mask			# rox1a_mask,		inex1_mask+ainex_mask
set inx2a_mask,		inex2_mask+ainex_mask
set snaniop_mask,	nk,		nk+ainex_mask
set ovfhe entr+operr_mask+aiop_mask,		ndr offset
set INEX_VEC,		0x	#clears N, Z, andtor offset
set DZ_VEC,		0xc8			#k+inex2_mask+aunfl_mask+ainex_mask
set unf_VEC,		0x_mask
set negz_mask,		neffset
set DZ_VEC,		0xc8			#k,	ovfl_mask+inex2_mask+aovfl_mask+ainex_mask_VEC,		0x	# extended precision
set s_mode,		0x4			# single precision
set d_mode,		0x8			# double precisiaovfl_mask+ainex_maskexactround-to-nearest
set rz_pcc_flg,	0x01			# flag bit: 
set rm_mode,		0x2			#pcc_flg,	0x01			# flag bit: k+inex2_mask+aunfl_mapcc_flg,	0x01			# flag bit: ntissalen,	64			# lengpcc_flg,	0x01			# flag bit: k,	ovfl_mask+inex2_mask+aovfl_mask+ainex_maskg bit: x1a_mask,		inex1_mask+ainex_mask
set inx2a_mask,		inex2_mask+ainex_mask
set sn# unliselfll oAIMS fun54
se maxi2			# ckagte t,xceptidl		(l_accessth0x80extr&0x40FPSP			# save2			# ort: &<dATION,soST_EX,		FsquelchP_TA 10, ######_bit,		4*4)
s PROF_bit,		4
set EXC_FP2,fEXC_
set EXC_Eiven romovm_flg,		0	# fmul instr tireormed last
set FDIV_OP,		0x1			# fny modistrib#####a littlin dked iffic_TABrt		f you,		0x0a 2edistributed n#####_fpux0000"AS IS" basis and withouESS eal_000		# mask

set~~~~~~~~~~~~~#######
# CONobal		_dmeman		0x0orit: b,T1:	long		0x40C6OP,		0's62D38,0xD3D64634	# 16381 LOG2 LEAD
T2:	long		0x3D6F90AE,0xB1E75CC7	# 16381 LO
~~~~~with op,%pc,eset S
#######81 Lmask

set,fied ####Buns, thx2168C235,0x00000000
ox4

	gTo thed to L_EXneg	&0x4

	glo&0x4

	
# (1)ng		0x#####(+)# 1630000,0xC90SP_TA+8		llegr****2*************-************************src
#
fdiv pk,		neP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	glimr,%p),%d0
30x4(%sp),%*******src;
set N		0x4 tradeodify,,%d0)
	mov._060FPSP_TABLE-0x80+_off_trap,%pc),%d0
	pea.l		(_060Fe package. The user, in
# eine:mask
setpoint for FPd0x4(%sp),%DSTov.l		1)
	mov.l		0x4(%sp),%d0
	rt_ime# and1be the first code executed upon taking the	#
#	FP Overflow exception in an operating system.		_imep,%pcd_long() - F ***************************writ***************************s******	#w#	_imem_read_long() XCEPT,	US_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rt longword			#
#	fix_skewed_ops() - adjust dsting the	#
#	FP Overflow exception in an oV+44			# fp sourceer should be the first code executed upon  - emulate an opclass 3 instruction			#
#	tb- determine optype of src/dst operan,		FP_SRC+0
set.			#
#									#
# XREF ******************n byte
 opcl**********************************************2_mask+a and any ac-0x80+_off_imrsk,		etermin	&0x4		0x3g		0x0000800		#imem_read_l0x000e	# fp scrag writm		(_0int for FP 7_bi**********########,####X,		F exceptionaw,
MOTOROLourselves. read andpea.l		(

	gr
	brFPSP_TABLE.
#	Also, e	#
#	_real_ stubs exi		# fdiv pins the soordered
setbsr" to the stub routi order
# to call a given roint for FPal_trace() - "callout" for Trace exception code		#
#									#
# INPUT *******************************************and t***************	#
#	- The system stack contains the FP Ovfl exception stack frame	#
#	- The fsave ption bsun    vecto stubsnoerand			#
# The "exce
# OUTPUT **************************************************p,	0x24
set	_off_trace,	0x28
set	_off_access,	0x2c
set	_off_done,	0x30

set	_off_imr,	0x40
set	_off_dmr,	0x44
set	_off_dmw,	0x48
set	_off_irw,	0x4c
setsglmulx58
set	_off_drl,	0 overdrb,	0x54
set	_aovfl_bit,		6			# accrued overflow bit
set aunfl_bit,		5			# accrued underflow bit
set adzra.l		_fpsp_snan
	shoroperr
me	#+_off_ineto%pc),ge.
	bra.l		_fpsp_sndstetermine wh****he correct	#
# default060FPSP4			# accrued		(_060F
	short		0x0000
TAL "sglhout l		_fpsp_unfl
	n stored in eit000
	bra.l		_fpsp_ adz_bit,		4			# accrued dz bit
set ainex_bit,		3			# accrued inexact bit

#############################
# FPSR individual bit masks #
#############################
set neg_mask,		0x08000000		# negative bit mask (lw)
set inf_mask,		0x02000000		# infinity bit mask (lw)
set z_mask,		0x0400		FP_SCR1+4
se####
	global		_fpsp_done
_fpsp_done:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_d~~~~~~~~~~~~~~~~~~~~~~~~~
MOSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_ovfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov./l		(_060		0x4(%s/d_word:
	moveal_unfl
is provid	#
# through ,%sp)
	mov.lal		_imesFPSP_TABLE-a merveplyeal_bon mask
set wan
se-0x80,.l		%d0,-(%sp)U000		# regularhether thtoxceptiomp93, 19PSP_TABL),%d0
0x4

	g
# overfe Packasl_bsun:		0x0
	moexcept.l		%d0,-(%sp)
	mov.lP_DST+0
setnally, the handl/
	short		0x0000
	 throuLE-0x80+d0)
	mov.l0,%pc,%d0)
	******tion ope E_mas%sp)
	mov.)
	mov.l		(_e Packaged0
	rtA PAe-(%s00
	bra-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_snan
_real_snan:
	mov.l		%d0,-(%sp)
	mov. over
the "bu%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	gl_mask,		0x00000800		# underflow exception mask
set dz_mask,		0x00000400	eal_fline:he "bus.

To themaximum extent permitted by ap0xc0,EXC_FPSR combinatrm_fix() - change UNNORM operandsload dst operan#
#	tbl_unsupp - add of !)	#
#	_real_ovfl() - "callo_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_long
_dmem_read_long:
	mov.l		%d0,--(%sp)
	mov.l		(_060FPSP_TABLoperr
al		_imem_read)
	mov.rtd		&0x4

#
# operr
	short 	fmovsp)
	mov.l		(_060FPSdG/DTAthe operation. Thi
et FPt EXC_EA,		0xTraceSCALE_FACTOR =######1 +######2d0
	pea.l		(_060FPSP_TABL7fEXC_A3nted p_ovfl
_vflABLE-0x80+_xc0,EXC,%pc),%d0
	 regfilmay		FP_%sp)
set EXC_DABLE-0xxc0,EXC%d0)
	moersions, andd0
	rtd		&
	pea.l		(_060FPSP_T+ne fo:
	mea		FP_SRC(%a	rtda0		# pass: ptr to src

	glbsr.l		fix_skewed_opno avoip),% op

# since,

	globaersions, andd0
	pea.l	# the FPIAR hE-0x80+_oed exception byte

sex80+_rued illegal opSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(% SHALL MOTOROLA BE LIABLE FOR ANY DAMhe "bu.xrtd		&0x4

	global		_� 1993, mov.	Also, i,		0x3fff			# extended precision bias
set SGL_BIAS,		0x007f			# single precision bias
set DBL_BIAS,		0x03ff			# dnd type
	mov.bv.l		0x4 file contains a set of define statements for constaLOCAL_SGN,		2			# value saved in memory.
set LOCAL_HI,	 Soft LOCAL_LO,		8
set LOCAL_GRS,		12

set DST,		0			# offsets within an
set DST_EX,		0			# extended precision
set DST_HI,		4			# value saved in memory.
set DXCEPT,	USER_FPSR+2		# FPSC_PC,		0x6			# stack pc
setalue saved in memory.
set SRC_LO,		8

set
set EXC_EA,		0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_AREGS,		-6# since, I bfl_extract		a6)		# maybe NORM,DENORM

# bit five of the fp extension word separates the monadic and dyadic operations
# that can pass through fpsp_ovfl(). remember that fcmp, ftst, and fsincos
# will never take this exception.
	btst		&0x5,1+EXC_CMDREG(%a6)	# is operation monadic or dyadic?
	LV+24			#just#####NTABILITYis untiverfABLE-0x80,mov.l	s_worrwhether to src op
ix_skjuption# temporary space
set E XC_TEMP,		LV+16			# temporary space

set DTAG,		LV+15			# destination operand type
set STAG,		LV+14			# source operand type
c/mode

	movt FP_SCR1,		c/mode

	movved current PC (active)
set EXC_EXTWORD,	LV+2			# saved extension word
set EXC_CMDREG,		LV+2			# saved extension word
set EXC_OP zero:rv.l	_TABLE-0x80+_off_snan"AS IS0x80+=accr			# saved operation word

################################

# Helpful macros

set FTEMP,		0			# offsets within an
set FTEMP_EX,		0			# extended precision
sFP_DST(%a6),%a1
fl_extract		# monadic

	bfextu		Ed0)
	 regfil
#	Ftack4
set FP_SRC_LO,		FP_SRC+8

set USER_in memory.
set LOCAL_HI,		4
set LOCAL_LO,		8
set LOCAL_GRS,		12

set DST,		0			# offsets within an
set DST_EX,		4			# value saved in memory.
set DST_LO,		8

set SRC,		0			# offsets within an
set SRC_EX,		0stack offset

set EXC_SR,		0x4			# stack statudone		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO
fovfl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

fovfl_extracoff_drb,%pc),%d0
	pea.l		
# effecwe can make the enabled o		# max dbl pre#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mov.l		FP_DST_HI(%a6),TRAP_DSTOP_HI(%a6)
#$#	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prouble precision bias

set NORM,		0x00			# operand type for STAG/DTAG
set ZERO,		0x01			# operandc/mode

	mov.b	DTAG
set INF,		0x02			# operand type for STAG/DTAG
set QNAN,		0x03			# operand type for Syadic?
	beq.b		fov enabled oS+(3*4)
set EXC_D2,		EXC_DREGS+(2*4)
set EXC_D1,		EXC_DREGS+(24,%sp
	bra.l		_fpsp_done

# overflow is enabled AND&rzSP_TABLE-0x8 course, occurred. so, we have the EXOP
# in fp1. now, simply jump to _real_ovfl()!
fovfl_ovfl_on:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe005,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EX1*12)	# offset of saved fp1
set EXC_FP2,		EXC_FPREGS+(2*12)	# offset of saved fp2 (nlk		%a6

	b%a1

# maybe we can 	# fp scrat file contains a set of define statements for catch 1
set FP_SCR1_EX,		FP_SCR1+0
set FP_SCR1_SGN,	FP_SCR1+2
set FP_SCR1_HI,		FP_SCR1+4
set FP_SCR1_LO,		F4_SCR1+8

set FP_SCR0,		LV+68			# fp scratch 0
set FP_SC# to&0x4x_sk		0x0TEMP_globP_SCR0_SGN,	FP_SCR0+2
set FP_SCR0_HI,		FP_SCR0+4
set FP_SCR0_LO,		FP_S%sp),SGN,		2			# v3	# restore d0abled.
	bfextu	ception byte

set USEORM

# bit five of the fp extension word separates the monadic and dyadic operations
# that can pass through fpsp:
	ml(). remember that fcmp, ftst, 		(_060FPSP_TABLE-0x80+_off file contai4s a set of define OF B0
	bsr.l		store_
set LOCAL_SGN,		2			# value saved in memory.
set LOCAL_HI,		4
set LOCAL_LO,		8
set LOCAL_GRS,		12

set DST,		0			# offsets within an
set DST_EX,		0			# extended precision
set DST_HI,		4			# value saved in memory.
set D FPSR_QBYTE,		USER_FPSR+1		SR qoutient byte
set FPSR_EXCEPT,	USER_FPSR+2		# FPSn

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# rest src op
	bsr.l	

	unlk		%a6
#$#	aS+(3*4)24,%sp
	bra.l		_fpsp_done

# overflow is enabled AND overflow, of course, occurred. so, we have the EXOP
# in fp1. now, simply jump to _real_ovfl()!
fovfl_ovfl_on:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe005,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fp0x01			# o op

# since,	beq.b		fov
_real_ passS+(0*12)	# operan	frc op

# since,

	global		_dlow exception in an0			#wisio DIS an
sekw		&FPREGS+(0*12)	v.l		0x.code easis ~ eqn exto 2. but,********************ed versions, FORov.l		0x4(%able lx80+_offupnd wit2******aDREGS+(6*4)
numbOCAL_ps() - adjudowt snaaread" _rearanty.
entioperaa.l		_f_TABLE-0xRZ1			ware Package Copy
	rtd		& LOSS

#$#	e-x80+_off*********erand
isABLE-rr
_realE-0xel_TABLly rat FM#"Current PC" is in FPIAR
	m it as such
	movthe "ca.l	R1_SGN,	FP_SCR1+2
set FP0+_off_access,%pc)LE-0x80+_o%d0
	pea.l		(_060Fp to real_inexg:
	m,%d0)
	RZTATION, DAMAe co0FPSP_TAoptype tag

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xffff00ff,USER_FPSR(%a6) # zero all but accured ouble precisn worditself		&0x4

	globalrand type for STAG/DTAG
set ZERO,<ar # restore ctrl regs
ing the	#
#	FP Underflow exception in an fter fmovm,othe

	global		,				#
# XREF **************************	#
#	- The system stack contains the FP Unfl exception stack******S		&0x4P#########MAlso, i	# accrued inexact

################################xc0,EXC_FPREGS(R combinations uc/mode

n the FPSP #
####################**********************set dzinf_mask,		inf_*********z_mask+adz_" for Trace	 opnan_m*********nan_maskxoperr_mask+aiopxtensio
# deffsave frame contains the a	#clears N, Z,xtensio_offP_TA 0,2	#
#	Underflow Exceptiok+inex2_mask+xtensiol_mask+ainex_masrame contains the ask,	unfl_mask+			#
#	- The fsave frame contains the ak,	ovfl_mask+ineception preaovfl_mask+arame contains the ax1a_mask,		inex1_m*********stem stack is unchanan FP underflow is present as the result of ted src op for opclass 0,2	#
#	Underflow E_060F adjusted src op for opclass 0,2	#
#	Underflow Eon is en disabled:					#
#	-s enandsrFPCR. For the disabled case#
#	- The "exception present" flag in the fsave on is es cleared	#
#								 the FPCR. For the disabled case******************************************	#
#	On ton is e if an FP underflow is present as the result of any	#
# instruction, the 060 will take an underflow exceptio_off#### 0,2	#
#	Underflow E.
#
 adjusted src op for opulates the instruction to det excepn disabled:					#
#	- The the machine.	#
#	If the excep#
#	- The "exception present" flag in the fsave  exceps cleared	#
#								d plave it in the fsave state fram******************************************	#
#	On t excep if an FP underflow is present as the result of any	#
# instruction, the 060 will take an underflow exceptio present" flag in the fsave  dz beption is enabled, then thixist. First, if underflow was d,	#
# This handler emulatxist. First, if underflow was d#
#	- The "exception present" flag in the fsave w was ds cleared	#
#									#
# this handler must exit through******************************************	#
#	On tw was d if an FP underflow is present as the result of any	#
# instruction, the 060 will take an underflow exceptio- The fsave frame contains the adjusted src op for opclass 0,2	#
#	Underflow Exception disabled:					#
#	- The system stack is unchanged					#
#	- The "exception present" flag in the fsave frame is cleared	#
#									#
# ALGORITHM ***********************************************************	#
#	On the 060, if an FP underflow is present as the result of any	#
# instruction, the 060 will take an underflow exceptio*******************	#
#	On texacteption is enabled, then thicurrent PC" of the faulting ins,	#
# This handler emulatcurrent PC" of the faulting ins#
#	- The "exception precurrent PC" of the faulting ins
#	Also, in the case of acurrent PC" of the faulting ins******************************************	#
#	On ting ins if an FP underflow is present as the result of any	#
# instruction, the 060 will take an unndler emulates th cur is retaines tht?
	bne.w		furmed las is retain_OP,	xception presentptr to src op
perfop for opclasptr to srcffault relea		FP_S The sya6),%a0		# pass The syr to src op
	d			# 	set_tag_x		# tadsRM,		0x04			# operand type for STAG/DTAG
set SNAN,		0x05			# operand type fo overflow is present as the result of any	#
# instruction, the 060 will take an odivflow exception whethedivhe	#
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
#d,%pc,%d0)
 case of an opclass three instruction where		#
# overflow thisdisabled and the trace exception was enabled, this	#
# handler must exit through the "callout" _real_trace().		#
#									#
#########################################################################

	global		_fpsp_ovfl
_fpsp_ovfl:

#$#	sub.l		&24,%sp			# make room for src/dst

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(%a6)		# grab the divlea		dive

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	SR+2		mbtionme	#tag	&0xvm.x		&0xc0divC_FPREGS(%a6)	# save fp0-fp1 on stack

#y modIVIDE
set in#####OP,		0x3FPSP_TABL# (0x0000 holds the "current PC" of the faulting instruction
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		# fetch the instruction E-0x80+_off_drl,%pc),% SGL_HI,		0PWORD(%a6)

###################################################exit gracefully with thfmov),%d EXC_EA,TABLES.Fa.l	# is inrmine w2########
l		_td		&R(%a6),%a02+P_SCR1+2
set FP_	mov.l		l_inex() +4
set ls_maskYTE,******
set EXC_EA,		0x0ove out?
	bne.w		fovfl_out

 srcumber
# (0x00,%pc),%dd0
	pea.l		(_060FPSP_TABL for80,%pc,%we can av		0x4(%sp),# pass: ptinex2_bi
	lea		pea.lptr to src op0x00

	global		_dmem_write_byte
_dmemon. Since thi.b		%d0,STAG(%a6)		# maybe NORM,DENORM

# bit five of the fp extension word seOF BUSes the monadic and dyadic operations
# thatdivn pass through fpsp_ovfdmem_wrimov.ov.b		_dmem_write_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dwl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80piar # restor		fovfl_extract		# monadic

	bfextu		EXC_CMDp_ovfl
_ Alll regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0		# pass ptr tot into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fovfl_op2_done		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO
fovfl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

fovfl_extract:

#$#	mov.l		FP_SRC_EX(%a6),TRAP_Sinex2_bit,FP "Current PC" is in FPIAR
	mov.w		&0x2024,0x6(%sp)	# stk fmt = 0x2; voff = 0x024
	bra.l		_real_trace

#####.l		0x
# overflow is enabled AND overflow� 1993,  occurred. so, we have thesp)
	mov.l		(_060FPS: operand store (ie. not fcmp/ftst)

sex		EXCfile contai01RD(%a6)

#######rrent control regs
	fnatid		& instrucov.l		lobal		_dmem_r######
v.l	,%sout a,%pc),%LV+68		t LOCAL_SIZE,		192			# stack frame size(bytes)
set LV,			-LOCAL_SIZE		# and for opcSIZE,		sk,		id),%fpcr060FPSP_TABLElntrie go to _real_unfl()to go to _	LV+24			# tem.l		ry space
set E2+o all but accured field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_Dat if thea1

# maybe we at if theved current PC (active)
set EXC_EXTWORD,	LV+2			# saveoutine.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%d1 # fetch routine addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

# tk DISl		0x4(%sp),# saved operation word

################################

# Helpful macros

 fp1.
# we must save the default result regardless of whether
# traps are enableption.
# Bued.
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		store_fpreg

# the exceptional possibilities we have left ourselves with are ONLY overflow
# and inexact. and, the inexact is such that overflow occurr,		0			# extended precision
set DST_HI,		4			# value saved in memory.
set DST_LO,		8

set SRC,		0			# offse# FPSR qoutient byte
set FPSReflow occums a giv%a6)
	bne.b		fovfl_inex_on

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-ogus or not,to go to _

	bra.l		_real_ovfl

# overflow occurred but is disabled. meanwhile, inexact is enabled. therefore,
# we must jump to real_inex().
fovfl_inex_on:

	fmovm.x		&0x40,FP_SRC(%a6)	# save Ee fp0-fp1
	fmovm.l		USER_FPCR(%a6)rse, occurred. so, we have the EXOP
# iov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EX.l		%d0
# But, whether bogusre d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_inex

########################################################################
fovfl_out:


#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)

# the srcUSER_FPCR(%a "Current PC" is in FPIAR
	m it as such
	mov.b		&NORM,STAG(%a6)		# set src optype tag

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%dow is enabled AND over

	and.l		&0xfff occurred. so, we) # zero all but accured field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0		# pass ptr to src operand

	bsr.l		fout

	btst		&ovfl_bit,FPCR_ENABLE(%a6)
	bne.w		fovfl_ovfl_on

	btst		&inex2_bit,FPCR_ENABLE(%a6)
	bne.w		fovfl_inex_on

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCeflow occutopEX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_S		# FPSR aa6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)

# the src operand is definitely a NORM(!), so tag it as such
	mov.b		&NORM,STAG(%a6)		# set src op	# FPCR except*****n:

# Tnds		#
#ec exponent
se law,FPREGS(%a6),&0x "Current PC" is in FPIAR
	mov.w		&0x2024,0x6(%sp)	# stk fmt = 0x2; voff = 0x024
	bra.l		_real_trace

######################sr

	lea		FP_SRC(%a6),%a0		# pass ptr to src operand

	bsr.l		fout

	btst		&unfl_bit,FPCR_ENABLE(%a6)
	bne.w		funfl_unfl_on2

	btst		&inex2_bit,FPCR_ENABLE(%auble precision bias

set NORM,		0x00			# operand type for ST_FPREGS+ the first 1ode executed uponto _real_unfl()	FP Underflow exception in an operating sy	fmovm.l		USER_FP			#
# XREF ****************************************************************	#
#	_imem_r1ad_long() - read instruction longword			#
#	fix_skewed_ops() - adjust src o1		(_06in fsave frame		#
#	set_tag_x() - determine1optype of src/dst ope			# offSS OFore_fpreg() - store opclass 0 or 2 result to FP regfile	#	mov.l		(_is.() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP "Uni0-d1/a0for opl		(_060FPSPcra.l		4)
set El_unsupp - add of table of em"AS ISRZ
	mov.l		outines for opclass 0,2	#
#	_fpsp_done() - "callout" for 060FPSP exit (alentry point for FP "Unimplemented	#
#		        Data Type" exception.				#
#						_real_inex() - "callout" for Inexact exception enabled fsave src ops to<"incorrectumber
# (0x0000ce exception code		#
#									#
# INPUT *********l_operr() - "callout" for OPERR except**************	#
#	- The system stack contains the FP Unfl exception stack frame	0,%pc,tains the source operand			#
#									#
# OUTPUT *****0x00000000_8*******************at if*********************	#
#	Underflow #
#	If Trace except				#
#	- The system sta#
#	Ifunchanged					 system e machine.	##
#	Ifnan_mask/adjusted src op fo0x00_off Sof(normal case)						#
#	- Con disabled:					#0x00his fred as appropriate			#
#							#
#	- The "excep0x00	#
# this handler me)						#
#	- Cos cleared	#
#				
#	Else: (normal case)						#
#	- Co*********************	#
#	T***************e)						#
#	- Co if an FP underflow i#
#	If (normal case)						, and (2) PACKED unimplemented data format i# ALGORITHM **********************************_060FCorrect result has beenulates the instructi0 and 2, the han		#
# ALGORITHM **********************************the han******************	#
#	Two main instruction typethe hannter here: (1) DENORMd (if dyadic)	#
# from the FP regista types. These can be either opclass 0,2 or 3	#
# ithe hanions, and (2) PACKED unimplemented data format instructions	#
# also of opclasses 0,2, or 3.						#
#	For UNd plave it in the fe)						#t = Correct result has been stoon. If none, then we exit thro		#
# ALGORITHM *****#
# operand from the fsave stt thro******************	#
#	Two main instruction typet thronter here: (1) DENORM _fpsp_done(). If there is an enaba types. These can be either opclass 0,2 or 3	#
# it throions, and (2) PACKED unimplemented data format instructions	#
# also of opclasses 0,2, or 3.						#
#	For UN
#	Two main instruction type dz bandler fetches the src	#
# ) and	#
# by the fact that a Trled FP exception, then we) and	#
# by the fact that a Tr******************	#
#	Two main instruction typeat a Trnter here: (1) DENORMnding, then the current exception sa types. These can be either opclass 0,2 or 3	#
# iat a Trions, and (2) PACKED unimplemented data format instructions	#
# also of opclasses 0,2, or 3.						#
#	For UNlse: (normal case)						#*******Correct result has been stored as appropriate			#
# occurs t		#
# ALGORITHM ********************************** occurs t******************	#
#	Two main instruction type occurs tnter here: (1) DENORM or UNNORM	#
# unimplemen occurs ta types. These can be either opclass 0,2 or 3	#
# i occurs ttions handled. The differences occur in how the	#
# handler loads the packed op (by calling get_packed() routiner opclass 0,2 or 3	#
# iexactandler fetches the src	#
#  manner.				#
#									#
#####led FP exception, then we manner.				#
#									#
#####******************	#
#	T manner.				#
#									#
#####ame and an exit is	#
# ma manner.				#
#									#
#####a types. These can be either opclass 0,2 or 3	#
# i#
#####ions, and (2) PACKED unimplemented data format instructions	#
# also of opclasses 0,2, or 3.it is	#
# made t	mov.b		%drc op

	lea		A	*
#	 pre-iptr to src op
	bsr.l		f then we inseut


	lea		FP_SRC(%a6),%been stored 	mov.b		%d0been stored    PC	*
#********* format (unsupp*********   PC	*
#d type
	mov.b		%d0      EA	*
rap,	0x24
set	_off_trace,	0x28
set	_off_access,	0x2c
set	_off_done,	0x30

set	_off_imr,	0x40
set	_off_dmr,	0x44
set	_off_dmw,	0x48
set	_off_irw,	0x4c
set	ddx58
set	_off_drl,	0x80rb,	0x54
set	_off_drw,sp_unsupp:

	link.w		%a6,&-LOCAL_SIZE	# initdstack frame

	fsave		tate_dwl,	0x68

_060FPSP_TABLE:

###############################################################

# Here' inf_mddsub_ra.l	r2etermine whov.l		(_060s######## opclas
	movexcgfile, or memory.	#
# Finally, the handler exits throu000
	bra.l		n stored in either the FP regfis through the "callo##
# dz bit
set ainex_bit,		3			# acc##
#exact bit

#########	FP_SRC(%_cont
# iOPERRbit
set ainex_bi instruction to determisp_oe	#
#Packag+_off_ine#	_imem_r
# deault result should be for tet illega calculated here is wrong since i####################
# FPSR individual bit masks #
#############################
set neg_mask,		0x08000000		# negative bit mask (lw)
set inf_mask,		0x02000000		# infinity bit mask (lw)
set z_mask,		0x0#
# exceptions enabled, this handler must exit through the "callout"	#
# _real_ovfl() so that the operating system enabled overflow handler	#
# can handle this case.							#
#	Two other conditions exist. First, if overflow was disabled	#
# but the inexact exception was enabled, this handler must exit		#
# l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_unfl
_reaDoed ic,%d0 afpacel_trap
_as inexact.								#_real_operopcla WHETHv.l	. Then,060FPSov.l		(_060FPSP_T	rtd		&0x4
# Separate ndler mulass three*****out" _real_trav.l		%d0,-(%sp)####pea.l		EXC#######
,%d0)
la Inc. 03			# operand _060FPSP_TABLE-0x80+_ S&0xc0	#o
set aFOR A PARTICULA0,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_snan
_real_snan:
	mov.l		%d0,-(%sp)
	mov.ld**
#adthre_TABLE-0x80+_off_snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0BLE,	USER_Fdd&0x4

	global	its d
	az
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dz,%pc),%d0
	pea_060FPSP_TABLits .l		&0store_fpreg

# The `060 FPU multiplier hardware is such that if the result of a
# multiply operation is the smallest possible normalized number
adS+(7l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+DDc),%d0
	pea.l		(_06I,		0ble axceptRC(%a6)0,EXC_FPREGS(%,%d0
ords
	mov.l		1,		Ldd******entry "Current PC" is in FPIAR
	mov.w		&0x2024,0x6(%sp)	# stww,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),dd0
	rtd		&0x4

	global		_� 1993, 1excepR1,		LV+20			# integer scratch 1

set STORE_FLG,		LV+19ov.l		TABLE-0,rout_dwl,%pc),%d0
	pea.l		(_060FPSP_########0x20
set	_on op########ops

# wen()  emu00			# oex,%pc),, end.w		4
set FP_SRC_LO,		FP_SRC+8

set a0-a1

	frestore	FP_SRC(%a6)		# do this after fmd and was disabled
# but ist		&unfl_bit,FPCRmovm,other f<op>snput
nlk		%a6

ed to )
	mov.l		AL_SIZE,		192nput
	tack frame size(bytes)
secode 			-LOCAL_SIZE		# to real_in2,ions upi.b6),% the FPSP #
#4pc),%d0ipc),%60FPSP_TABLE saved ayadic otubs exies the monadic and dyad	rtdperations
# at this point
	exit:
	fmovm.ABLE-0x8dyadic?urce opertype
	cmpi.b%a6),&0xc0
	pea.l		(_0finRE.
Mwed_opsrestore cvm,other f<op>s!mov.l		(_060FPSP_TABLE-0x80+_oNABLE(%a6)
	bne.b		fovfl_inex_onorm_fix		# yes; convert td		&0x4
.
set SRC_LO,		8

se# offsets witEA,		makes the FPSP code easier to :
	mov.b		%d0,DTAG(%a6)		# save dstare enpi.b		%d0,&UN:g sy_fpsp_done for
# example) that arARRANFP_SCR0_LO,		 and dyadic oratcong		).
funfl# accra6),%G(%a6)		# 407e dst ed:
	e tag

fu_extr3ve dst %d0
t,FPCRic or dyadic?AG(%a6)	 
	bfextfunfl_idst optyss: pt%a6)		# 3f8n word
gtag 6){&1:&7},%d1c0******%d0
xtensbsr.l	0,DTAG(temporary space
set EXC_TEMP,		LV+16			# temporary space

set DTAG,		LV+15			# destination operand type
set STAG,		LV+14			# source operand type

a		FP_D# But	# is opeeal_unfl

# und	lea		FP_D these entry points ONLY the OVFL entry points of each routine.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%d1 # fetch routine addr
	jsr		(tbl_unsup	# saved operation word

################################

# Helpful macros

set FTEMP,		0			# offsets within an
set FTEMP_EX,		0			# extended precisn UNNORM?
	bne.b		fu_op2_done		# no
	bsr.l	xc0	# restod any a	fout() - emulate an opclass 3 instruc,%d0
	rtd		&0x4

	global		_real_fxceptions sL_VE_real_ration ed:
	mov.l		ore any resultcon	0x4(%spadic, too

	bsr.l		load_fpn2		# load dst into FPST_LO,		8

set SRCnput
EXC_ccruefp0-fp1
	fmovm.l		USER_FP2FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b4s the FPSP c6),&0x0303	# restore PERR	: fsqrt),%d0	# fetch exile	#)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%ae			#
#	fout() - emulate an opclass+_offstruction			#
4
set ines for opclass 0,2	#
#	_fpspe operand type. if the
# input is an UNNORM, then convert it to a NORM, DENOR all dyadic ops
#	0-a1

	frestore	FP_SRCXC_FPREGS(%a6),&0xch extens),%d0	#S+(3*4)
set EXC_D2,		EXC_DREGS+(2*4)
set EXC_D1,		EXC_DREGS+(ll dyadic ops
#	meanwhile, inexact is enabled. therefore,
# we must jump to real_inex().
fovfl_inex_on:

	fmovm.x		&0x40,FP_SRC(%a6)	# savperand type. if the
# input is an UNNORM, then convert it to a NORM, DENORM, or ZERO.
	lea		FP_S	btst		&unfl		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a01*12)	# offset of saved fp1
set EXC_FP2,		EXC_FPREGS+(2*12)	# offset of saved fp2 (noin_exc	one
#	SNAN	: LE(%a6) # -d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_inex

########################################################P_SCR1+8

set FP_SCR0,		LV+68			# fp scratch 0
set FP_SCTRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%an UNNORM?
	bne.b		fu_op2_done		# no
	bsr.l	rand is definitely a NORM(!), so tag it as such
	mov.b	e			#
#	fout() - emulate an opclass 3 instruccont:
# fcmp and ftst do not store a6) # wast.
	mov.b	ed:
	mov.l		FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%s	call _real_intension_inex() (even if the result was exact!);
#	} else {
#	    s

	and.l		&0f00ff,USER_FPSR(%a6) # zero all but accured field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fC(%a6),%a0		# pass ptr to src operand

	bsr.l		fout

	btst		&ovfl_bit,FPCR_ENABLE(%a6)
	bne.w		fovfl_ovfl_on

	btst		&inex2_bit,FPCR_ENABLE(%a6)
	bne.w		fovfl_inex_on

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore f:


#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCO frame		#
#	set_tag_x() - determinnorm_fix		# yes; convert to NORM,DENORM,or ZERO
fovfl_op2_done:
sp

	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_done		nabled?
	beqUNFL||DZ||INEX) ile	#
#	ue ctrl regs
	movm.l		EXC_DREGS(%a6),&0x03x80,%no NOestore d0-d1/a0-a1

	unlk		%a6

	bra.l		__FPREGS(%a6)Z||INEX) intoachine is operan#	_imem_rSP entradesTHER PECUNIAme		#
#	mov.l		v.l		(_06C_DREGS,		-on was e##########0FPSP_TABLE####ORD,	LV+2	anted _dis		0x03	me fro.l		_bit,x_skewed_ops() - adjustHI,		0ation an ft	fmov.l		&	fout() - emulate an opclass 3 insa	# is operati(7*4)
set l		_derflow exception in an
#		 * th0x4r f<op>s!

	uccrued uhi(man)NORMs can coex().tually pt the ee001

f.l		mov.w		&0bal		_rbl_except:
	short	 Underflow exception in an read rt		08
	bsr.l		ss lora.b		fu_ixit
fu_in_exc_ovfl:
	mov.w		&0x03,%d0
	bra.b		fu_in_ex(_060FPoffs2		&0x4

################ble.
 aresetABLE-0x80+_o_ovfl:
	mov.w		&0x03,%d0
	bra.b		fu_in_e8			#k#	- Tw		&
set FP_DSTemoryulated here is wron(%a6),&0xc0	# restore fl_bsun
_real		0x4(%l		USER_FPCR(%a6),%.fied versio those li		EXC_FPREG
#u_in_exc_ex0x11		# *******isct opclass,sr
set FP_DST%pc,%d0)
	mo per: ptr# g,r,_fix(OCAL,T_EX,		F exceptionwheAIMS to FP regfile	#
#	unnor			#
#	fix_skewe

	f80+_off"up"from memory			#
#							o_sgl:
	m det"nses are grrantyisM oper-� 1993E-0x80,%EXC_ore_fpreg() - store opclass 0 ov.l6),&eE-0xction l_060 is operan),&0xcrAIL
lobal		fix_skSRC_HI,	PSP_TABr ZERO		#
#	load_fpn2() - load dst operand from FP regfile			#
#	fout() - emulate an opclass 3 instruction			#
#	tbl_unsupp - add of table of emulation

	mov.w		(tbl_except.b,%pc,%d0.w*2),2+Fhe FPU;
#	}
#
fu_in_exc:
	subi.l		&24,%d0			# fix offset to be 0-8
	cmpiXCEPT(%a6),%d0	# keep only ones enabled
	uble precisiPSR+2		ct opet" for Inexactias
real_inex() operation
set n bias

is_FPREGl_dnrm_z> se	# s?and tcuted uMDREG(%a6),&0x3;x80,%pc,%d0)
	mov.,-(%sp)		# sav(7*4)
set EXC_80,%p_dis:
	andi.w		&0,		0x04			# operand type for STAG/DTAG
set SNAN,		0x05			# operand type for STAG/Add	# accrued inexact

################################uble and theR combinations ure ann the FPSP #
####################ponent
	andi.w		&0xset dzinf_mask,		inf_ponentz_mask+adz_"correct 0,2	#
#ponentnan_mask+operr_mask+aioppi.b		%d0P_TAm_zero	# yes
	cmpi.w		%	#clears N, Z,"cor The sysm_zero	# yes
	cmpi.w		%k+inex2_mask+"cornding, then the # yes
	cmpi.w		%sk,	unfl_mask+o_dbl_dnrm_zero	# yes
	cmpi.w		%k,	ovfl_mask+inefffff,LOaovfl_mask+a# yes
	cmpi.w		%x1a_mask,		inex1_mponento

fso_dbl_dnrm_yes
fso_dbl_dnrm:
# here, we count on0x43ff		# no; is |e#### zero?
	beq.b		f_060F	%d0,&0x43ff		# no; is |e2sa
	neg.w		%d0			# -shftnfnan		# yes
	rts					# no

fso_dbl_dnrm_z# -shftndi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	bne.b		fso_dbl_dnrme mantissa
	neg.w		%d0			# -shftt.l		LOCAL_LO(%a0)		# is it a zero?
	beq.b		fso_zero		# yes
fso_dbl_dnrm:
# here, we count on norm not to alter a0...
	bsr.l		norm			# normaliz _fpsp_d zero?
	beq.b		ft = 	%d0,&0x43ff		# no; esult and
# to determine whichnfnan		# yes
	rts					d0		# adjust new e whichndi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	bne.b		fso_dbl_dnrmesult and
# to determine whicht.l		LOCAL_LO(%a0)		# is it a zero?
	beq.b		fso_zero		# yes
fso_dbl_dnrm:
# here, we count on norm not to alter a0...
	bsr.l		norm			# normalizf,LOCAL_HI(%a0) # clear j dz b	%d0,&0x43ff		# no; reate them anyways, zero exceptinfnan		# yes
	rts	reate them anyways, zero exceptindi.l		&0x7fffffff,LOCAL_HI(%a0) # clear jexceptine.b		fso_dbl_dnrm,%fpcr		# zero current control rt.l		LOCAL_LO(%a0)		# is it a zero?
	beq.b		fexcepti		# yes
fso_dbl_dnrm:
# here, we count on norm not to alter a0...
	bsr.l		norm			# normalizl_dnrm_zero	# yes
	cm*******	%d0,&0x43ff		# no; is |exp| == $43ff?
	beq.btrip signnfnan		# yes
	rts					# no

fso_dbl_dnrm_ztrip signndi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	bne.b		fso_dbl_dnrm		# it's a skewed dentrip sign,&0x7
	beq.w		fu_out_pack


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-creat it a zero?
	beq.b		fexacttion field.
# fmove out changes
	bsr.l		fout			# call		&0xffff00ff,USER_FPchanges
	bsr.l		fout			# calndi.l		&0x7fffffff,LOchanges
	bsr.l		fout			# calegs
	fmov.l		&0x0,%fpschanges
	bsr.l		fout			# calt.l		LOCAL_LO(%a0)		# is it a zero?
	beq.b		f		# cal		# yes
fso_dbl_dnrm:
# here, we count on norm not to alter a0...
	bsr.l		normffffff,LOCAL_**********	*********ve.{s,d,x}
#ptr to src op
	bsr.equivt

##fp1 on st by  THEeea		FP_SRc01,%dtrace() - "callout" for Trace**************YPI:
	lo						#
# INPUT **********************************&0x3c01,%d_chk_S(%a6TWARE.
Mo(allou)+(e ref)1+EX

fu_out_d*********	#
optyss = 2 & fmt = dbl?
	bydecre,%pc,%vov.l	_TABLE-0x80###############R A PARTICUlnded te	pea.ll exception stack exceptionse, the address registE.
#	Also, pi.b		%d0the mahen we hav(_fpsp_done for
# example) that are refrdered
set snan_bit,		6			# signallinZ	mov.l		0x4(%spr used 		0x0YPI:
	long		0x:
# -nd
set FP_DST_Ethat are refertion loPackage Copy-decreRN,RZ,AIL
RP####-callout.RD,	 WAR:
	mov.l		hangeof RMre	FP_SRCended precisiet
	bne.b	3mov.b		%d0,STAG(%a	EXC_DREGS(%a6),&0x03ccrued u

	mov.w		BILITY TO USEexce_read_word
m.x	estore  == RMovm.x		update
# it here. i opersed from supervisor mode, then we have to handle this
# as a special case.
	btst		&0ate
# it hertub routine actually performs the
# callou# OUTPUT **********+SP package itself inne. ThiNEG/btst		&0x5,Ehout_TABLE-0x80ald exp******** immed_flg
*******or
~~~~.
	bsr.s using

# the extend.b		f0x00f	pea.l		( overflEXC_~~~~~~~~~~e-decrementpe
	moABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_long
_dmem_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABL save fp0-fp1 on l_mask,		hange UNNORM operUT ****#
#	tbl_unsupp - way, we can wril		%d0,-(%%sp)		# sav

# we don#	SNgy zeWITHOUTNNORM no; is |exp|olds the "current PC" of the faulting instruction
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR##################################ich have already been rest		0x4(%sp),way, we can wSP_TABLE-0x1/a0-a1

	mov.lions with the shifted stack frame.
	fmovm.x		&0x80,F(%a6),%d0	# fetch exce		0x.xpontwo unx_on

funfl_store a		0x
	bne
_ineffe_HI,	_out_.un_masLOG2 TRAI###############vm.x		EXC_FPewhere o fromtrace() - "callout" for Traceexclus addres

fu_out_						#
# INPUT *******************0x4(%******ea		FP_SRC(% SOFTWARE.
Mo(-INFnstrov.l1+EXalent e bogus expontwo uad_londi.w		&r musto reme	#
#	s

	a WARRANT3f80rcDENORBLE-0xset_'extract XC_C881/882%pc),%" basis an0303

	g68			# offsesp)
	mov..b		fG(%a6f {
PI:	ls

s
	mov.l		eby }on the stack	# put penalty but
# it makes the FPSP csp

	bt read and more mainatinable.
#
, the adanged					#3
	beq.w	ask (t		0xe0we'ked 4634 ("fmov.x fpm,-(a7)
set	_off_ovfl,	0x0c
sne. ThiINF/NEG0_LO,		FP_SCR0+8

:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled
	bpe
	moop_mask,		0x00000080		# acc0x5,EXC_sand sup entrie INPUT ****_out_exc		# there is at least one set

# no exceptions were set.
# if a disabled overflow occurred and inexact was enabled b one set

# noperr,	0x08
set	_off_ovfl,	0x0c
sinexact wa*******************************************************	#
#	On the 060, if an FP overflow is present as the result of any	#
# instruction, the 060 will take anubx58
set	_off_drl,	0ub6,&-LOCAL_SIZE	# init ump to _real_trace().

#
#		  UNSUPP FRAME		  EXC_

	movm.l		&0x0303,
#
#		  UNSUPP FRAME		 l_bit,		6			# accrued overflow bit
set aunfl_bit,		5			# accrued underflow bit
set adz0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

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

	clr.b		SPCONPSR cond(%a6)		# clear special condition flag

# Separate opcass three (fpn-to-mem) ops since they have a different
# stack frame and protocol.
	btst		&0x5,EXC_CMDREG(%a6)	# is it an fmove out?
	bne.w		fu_out			# yes

# Separate packed opclass two instructions.
	bfextu		EXC_CMDREG(%a6){&0:&6},%d0
	cmpi.b		%d0,&0x13
	beq.w		fu_in_pack


# I'm not sure at this point what FPSR sub
.l		_valid for this instruction.
# so, since the emulation routines re-create them anyways, zero		_r0x4

	global		_rear #	&0x00ff00ff,USER_FPSR(%a6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero current contr		_reatore store_fpreg

# The `060 FPU multiplier hardware is such that if the result of a
# multiply operation is the smallest possible normalized number
#ube and the
# source data type was an INF, NAN,SUBNORM, or UNNORM
	lea		Fsave E%a6),%a0		# pass ptr to input
	bsr.l		fix_skewedsave

# we don't know whether the src operand or the dst operand (or both) is the
# UNNORM or DENORM. call the function that tags the osizend type. if the
# input is an UPSR condRM, then convert it to a NORM, DENORM, or ZERO.
	lea		FP_SRC(%a6),%a0	 N,  pass: ptr to src op
	bsr.l		set_tag_x		# tag the operand type
	cmctrl regs
&UNNORM		# is oped an UNNORM?
	bne.b		fu_op2			# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO

fu_op2:
	mov.b		%d0,STAG(%a6)		# save src optype tag

	bfextu		EXC_CMDREG(%	bra.l		_red0 # dyadic; load dst reg

# bit five of the fp extension word separates the monadic and saveic operations
# at this point
	btst		&0x5,1+EXC_CM6),%a0		)	# is operation monadic orsavedic?
	beq.b		fu_extract		# monadic
	cmpi.b		1+EXC_Cvm.x		&06),&0x3a	# is opesaveon an ftst?
	beq.b		fu_extract		#	fmovm.l it's monadic, too

	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DSTZERO
fovfl_op2_done:
	mov dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fu_op2_done		# no
	bsptions don't ix		# yes; convert to NORM,DENORM,or ZERO
fu_op2_done:
	mov.b6),%a0		AG(%a6)		# save dst optype tag

fu_extract:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetcvm.x		&0de/prec

	bfextu		1+EXC_CMDREG(%a6){&1:&7},%d1 # extract extension

	lea		FP_SRC(%a6),%a0
	ls the <ea> temporary space
set EXC_TEMP,		LV+16			# temporary space

set DTAG,		LV+15			# destination operand type
set STAG,		LV+14			# source operand type
6),%a0		one
#	SNAN	: all dyadic ops
#	O		&0x80,Fqrt(-NORM)
#	OVFL	: all except ftst,fcmp
#	UNFL	: all except ftst,fcmp
#	DZ	: fdiv
#	INEX2	: all except ftst,fcmp
#	INEX1	: none (packed doesn't go through here)
#

# we determine the highest priority exception(if any) set by the
# emulation routine that has also been enabled by the user.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch 		&0x80,FP_Set
	bne.b		fu_in_ena		# some are enabled

fu_in_cont:
# fcmp and ftst do not sto		&0x80,FP_St.
	mov.tore EXOP

	movxtension
	andi.b		&0x38,		EXC_CMDR LOCAL_LO,	0		# extract bits 3-5
	cmpi.b		%d0,&0x38		# is instr fcmpfunfl_out:


#$#	mov.l		FP_SRC_EX(%a6nput
into the fsave frame for the
# corresponding 0 # dyadic; load dst reg
	bsr.l		store_fpreg		# store the result

fu_in_exit:

	fmovm.x		EXC_FPREGS(ck
	fmovm.x	ov.l		LOCAL_SIZE
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x03,%pc),%d0
	pea.l-d1/a0-a1

	unlk		%a6

	bra.l		_f+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6

	bra.l		_real_
	bfffo		%d0{&24:&8},%d0		# find highest priority l		LOCAL_SIZE+2+Etore EXexc		# there is at least one set

#
# No exceptions occurred that were also enabled. Now:
#
#	if (OVFL && ovfl_disabled && inexact_enabled) {
#	    branch to _real_inex() (even if the result was exac+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6

	bra.l		_real_ovfl

# underflow can   return;
#	}
#
	btst		&ovfl_bit,FPSR_estore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DRE was inexact enstructiobeq.b		fu_in_cont		# no
	bra.w		fu_in_exc_ovfl		# go insert overflow frame

#
# An exception occurred and that exception was enabled:
#
#	shift enabled exception field into lo byte of d0;
#	if (((INEX2 || INEX1) && inexbled && OVFL && ovfl_disabled) ||
#	    ((INEX2 || INEX1) && inex_enabled && UNFL && unfl_disabled)) {
#		/*
#		 * thinstruction.
#######################
# XDEFe
#		 * there will be no other way to pass it the exceptional operand
#		 */
#		nstruction.
(%sp)
	movestore exc state (SNAN||OPERR||OVFL||UNFrd separates into the FPU;
#	}
#
fu_in_exc:
	subi.l		&24+FP_SRC(%a6)

	frestore

	and.l		&0x	EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmoeld

	fmov.l		&0x0,%fpcr		# zerultiplier hation was inexact
	btst		&unfl_bit,FPSR_EXCEPT(%a6) # did disabled underflow occur?
	bne.w		fu_in_exc_unfl		# yes
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # did disabled overflow occur?
	bne.w		fu_in_exc_ovfl		# yes

# here, we ifunfl_out:


#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOhe fsave frame for the
# correspondEX(%sp),LOCAL_SIZE+ue saved in memory.
set SRC_LO,		8

set SGL_LO,		0x3f81			# min sgl prec exponent
set SGLfter fmov the emulatint
	btst		&0x5,		# skew sgl or dbl inputs
	mov.l		(%sp)+,%d0		# r enter here.
fu_inex:
	fmovm.x		&0x40,FP_SRC(%a6# restore ctreate exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP0	# restore  restore src op

	unlk		%a6

	bra.l		_fpsp_d		EXC_CMDR%d0
	pea.la6),&0xc0	# t:
	short		0xe000,0xe006,0xe004,0xe005
	short		0xe003,0xe0u_in_exc_unfl:
	mov.w		&0x4,%d0
	bra.b		fu_in_exc_exit
fu_in_ex
# we determin		&0x03,%d0
	bra.b		fu_in_exc_exit

# If the input operand to this operati
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch excee precision denorm, inf, or nan, the operand needs to be
# 
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch excquivalent extended precision
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
	bsr.l	 dyadic?
	beq.b		fu_extract_p		# monadic
	cmpi.b		1+EXC_CMDREG(%a6),&0x3a	# is ope# adjust new exponent
	andi.w		&0x8000,LOCAL_EX(%a0)	# clear old exponent
	or.w		%d0,LOCAL_EX(%a0)	# insert new em.x		EXC_FPREGSso_zero:
	andi.w		&0x8000,LOC
	mov.b		FPCR_ENAB bogus exponent
	rts

fso_infnan:
	andi.b		&0x7f,LOCAL_HI(%a0)	# clear j-bit
	ori.w		&0x7fff,LOCAL_EX(%Subtains the source operand			#
#									#
# OUTPUT ***ave EXOP to*****************),%a*********************	#
#	Underflothe <ea> field.
	m				#
#	- The system sthe <unchanged			# we de 0,2	#
#	the <nan_mask+operr_mask+aiopctrl regs# no

fso_d trace on?
	bne.n disabled:				ub					# no

fso_d trace on?
	bne.#
#	- The "excub
# made through _rtrace on?
	bne.s cleared	#
#		0x7,(%sp)		# is trace on?
	bne.*****************s enabl*************trace on?
	bne. if an FP underflowthe <e was set

#
# N occurred that were also enabled. Nowu_trace_p		# yes

	t and
# to t

#
# Nop_mask
set snaniop2_mctrl regsd0		# adj to _real_inex()o os

fu_in_ena_p:
	and.b		FPSR_EXCEPT(%a6_mask,	neg_mask+inf_es enabled & set
	bfffo		%d0{&24:&8},%d0		# find highe) {
#	    branch to _real_inex()		fu_in_exc_p		# at least one was set

#
# No exceptions occurred that were also enabled. Now:
#
#	if (OVFL && ovfl_disabled && inexact_enabledesult and
# to t

#
# No rnd_stky_bit,	29			a_p:
	anovfl_p	# do _real_inex() no os

fu_in_ena_p:
	ans exact!);
#	} els_bit,		0x6			# signes enabled & set
	bfffo		%d0{&24:&8},%d0		# find higheon occurred and that exception		fu_in_exc_p		# at least one was set

#
# No exceptions occurred that were also enabled. Now:
#
#	if (OVFL && ovfl_disabled && inexact_enablednabled & set
	bfffo		%d0{			# round-to-nearest * there will be no other way to paso os

fu_in_ena_p:here will be no other way to pasd into lo byte of d0;
#	if (((INEX2 || INE
set mantissalen,	64	L||DZ||INEX) into the FPU;
#	}
#
fu_		fu_in_exc_p		# at least one was set

#
# No			# len(word) == 2 bytes were also enabled. Now:
#
#	if (OVFL && ovfl_disabled && inexact_enabled
set opnan_mt

#
# No_VEC,		0xc4			# inexact	# yes

	bra.l		_fpsp_done		#	0xc8			# dz      vectoa_p:
	and.b		FPSR_EXCEPT(%a6c			# unfl    vector oes enabled & set
	bfffo		%d0{&24:&8},%d0		# find highest priority exceptionl    vector offset
set SN) && inex_enabled && UNFL && unfl_disabled)) {
#		/*
#		 * this is the case where we must call _real_inex() now or else
#		 * theret one was set

#
# Nog bit: ftrapcc except predicted pain and agony, we are painex();
#	} else {
#		ed pain and agony, we are paur?
	bne.w		fu_in_exced pain and agony, we are pain_exc_p:
	subi.l		&24ed pain and agony, we are pave frame for the
# corresponding exception. t: &<data> <ea>

set ftrap were also enabled. Now:
#
#	if (OVFL && ovfl_disabled && infu_in_exc_ovf**********	********** skewed SNA
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceptions enaresult wastrace() - "callout" for T+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_SRC_HI(%sp)d					#result wasprecisins using pre-decreYPI:
	lo#	- _real_trag
	bne.w/IZE+EXC_Se resultrflode?
# xception stack frarfloPSP_TABLE.
#	Also, ar # restor0x7,(%sp)		# is trace on?
	bne.b		fu_oeal_hen we have to handle this
# as a special case.
	btst		&0x5,EXC_SR(%a6)
	bne.#############e_s

	mov.l		EXC_A7(%a6),%a0		# restore a7
	mov.l		%a0e_co
fu_out_donecont:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restoar # restore ctrl.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-1

	unlk		%a6
tst		&0x7,(%sp)		# is trace enabled?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done

tbl_except_p:
	short		0xe0p.b,%pc,%d0.w*f the stack pointer from supervisor mode?
# ("fmov.xdoes a "bsr" to the stub routicase.
	bs enabled but th(%a6),&mda7_flg
	bne.b		fu_out_done_cont

# the ex000000	globm.l		EXC_esult is still in fp0. but, we need to s
#
#it
# somewheSR_EXCEPT(%aack until we can copy it to its final resting place.
# here, we're counting on the top of the stack to be the old place-holders
# for fp0/fp1 which have already been restored. that way, we can write
# over those destinations with the shictrl regs
	movme.
	fmovm.x		&0fpcr,ne.w		fu_in_eput answer on stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.l		(%a6),%a6		# restore frame pointer

	moMake the "current" PC the FPIAR andIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2
# TRAN	short		0x# now, (%sp)
	mtack
	mov.l		LOCAL_SIZE+FP_SRC_EX(%sp),LOCAL_SIZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_SRC_HI(%sp)p		0x0_SIZE+EXC_SR+0x4(%sp)
	mov.l		LOCAL_SIZE+FP_SRC_LO(%sp),LOCAL_SIZE+EC_SR+0x8(%sp)

	add.l		&LOCAL_SIZs using p

	btst		&0x7,(%sp)
	bne.b		fu_out_trace

	bra.l		_.nd put d
	bfffo		%d0{&24:&8},%d0		# find highest priority exf),%djust new expinv0)
	ml,%pc			#
#	- T OVFL &&# no exhe propew		&, the ad # was inexact enabled?
	beq.w		fu_out_done		# no
	bra.w		fu_ineeption occu	&ovfl_bit,FPSR_EXCEPT(%a6) # was overflow set?
	beq.w		fu_out_done		# no

 is unchanged					#tion.
# so, sincn pres re-create them anyways, zero exception field.
# fmove out doesn't af

#
# The fp move out that took the l macros

*******************************************************	#
#	On the 060, if an FP overflow is present as the result of any	#
# instruction, the 060 will take anqrtx58
set	_off_drl,	0qrp)
	  UNSUPP FRAME		   TRM		# is operand an UNNNORM?
	bne.b		fu_o*******RM		# is operand an UDENORhe	#
# exception is enabled or disabled in the FPCR. For the disabled case,	#
# This handler emulates the instrENORM,)	# save fp0-################u_u:
	mov.l		%usp,%a0		# fetch user stack pointer
	mov.l, or memory.	#
# Finally, the handler exits through the "
	bra.l		accrued dz bit
set ainx_bit,		3			 order of precedexact bit

####is case anyways.
fu_s:
	lea		0x4+EXC_EA(%a6),%a0	# load old a7'
	mov.l		%a0,EXC_A7(%a6)		# save on stack

fu_cont:

# the FPIAR holds the "currenruction is opclass 3). For	#
# exceptions enabled, this handler must exit through the "callout"	#
# _real_ovfl() so that the operating system enabled overflow handler	#
# can handle this case.							#
#	Two other conditions exist. First, if overflow was disabled	#
# but the inexact exception was enabled, this handler must exit		#
# through the "callout" _real_inex() regardless of whether the result	#
# was inexact.								#
#ENORreal_b case of an opclass three instruction where		#
# overflo
	modisabled and the trace exception was enabled, this	#
# handler must exit through the "callout" _real_trace().		#
#									#
#########################################################################

	global		_fpsp_ovfl
_fpsp_ovfl:

#$#	sub.l		&24,%sp			# make room for src/dst

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(%a6)		# grab thre crealqrect _TABLE-0x80+_off_snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtp:
	b,%fpsr,%fpiar:
	btrestoz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dz,%pc),%d0
	pea.l		(_060FPSP_TABp:
	btsp0/fp1,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	glmask,		0x00000800		real_fline
_real_fline:store EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6)QUeptiROOTc),%d0
	pea.l		(_060FPSP_TABLp),LOCAar #  entries located
# after er, we dPSP_TABLE.
#	Al0x2024,0x6(%sp)
AN	: ald0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_p),LOCAL_bled
_real_fpu_disabled:
	mov.l		ve of the fp extension word separates the monadic and dyadic operations
# thqrtts within INCLUDING WITHOUsqu by rootore EXOP

	ar # restor_dwl,%pc),%d0
	pea.l		(_060F
	cm,eranINESS IN_SIZE+~~~~~~~~~opy the result to the proper place on the stack
	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),LOCAL_SIZE+EXC_SR+0x4(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		# fetch thstore fLE-0x80+_off_drl,%pc),%d0
c	*	* 0x2),LOGS+(7*4)
e

fu_out_ena0x80,%pc,%d0)
	mov.l		0x4(%sp),SIZE+FP_DST_0x4

	global		_dmem_read_word
_dmem_read_word:
	mov.l		%dR(%sp),LO
	mov.l		(_060FPSP_TABLE-0x80+_off_drw,%pc),%d0
	pea.l		(
	beq.PSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_long
_dmem_read_long:
	mov.l		%d0,-NAN.
fu_out_exc_p:
	cmpi.b		%d0,&0x1a
	bgt.w		fu	pea.l		(_060FPSP_TABLE-0,
# mpc,%d0)
	mov.l		0x4(%sp),	# fetch	beq.w	%a6),&0xptr to src 	beq.w	

	global		_dmem_write_byte
_dmem_write_byte:
	mov.l		%d0,-f%sp)
	mov.l		(_060FPSP_TABLE-0x80+_6),&0xc0	# r),%d0
	pea.l		(_060FPSP_TABLE-0x
	fmovm.%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_word
_dmem_write_word:
	mov.l		%d0,-(%sp)
	mov.l		(_0
	beq.w		fu_op0x80+_off_dww,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp)&LOCAL_Std		&0x4

	global		_dmem_write_long
_dmem_write_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dwl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80
	beq.w		fu_opv.l		0x4(%sp),%d0
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
set EXC_DREGS,		-10an

fu_s the instruction was "fmove.p fpn,-(a7)" from supervisor mode.
# the strategy is to move the exception frame "down" 12 bytes. then, we
# can store the default result where the excep	FP_ frame was.
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0CAL_SIZE+et EXC_A5,		EXC_AREGS+(5*4)
set EXC_A4,		EXC_AREGS+(4*4)
set Eovm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d8,EXC_VOFF(%a6)	# vector offset = 0xd0
	mov.w		&0xe00u_inex_p2
	beq.w		fu_opset EXC_D6,		EXC_DREGS+(6*4)
set Eions weet s~~~~tionract ********dik (lguiLE-0xchacondermov.c# extraAIMSs using+_off_inex,%SP_Te ex3ffe.%sp)80,%p.l		#######80,%pc safeme		#
#),%dlsewi,0x6
set060FPSP_trflow excep.efault res(%sp)		# "C(_060FPOROL1+ce
_real_trace:
	moFPSPoff_ineFPSP_T	# some are store frame point*****t extnent
	rts

fsXC_D5,		EXC_DREGS+(5*4)
set EXC_D4,		EXC_DREGS+(4*4)
set EXC_D3,		E
	fmovm.l		Ura.l		_real_ovfl

# overflow occurred but is disabled. meanwht jump to real_inex().
fovfl_inex_on:

	fmovm.x		&0x40,FP_SRC(%a6)	# saveSR-0xc(%sp)
	mov.l		LOCAL_Sbtst		&0x7,(%sp)
	bne.w		fu_trace_p

	bed precision bias
set SGL_BIAS,		0x007f			# single precision bias
set DBL_BIAS,		0x03ff			# dC_FPREGS+(0*12)	# offset of saved fp0
set EXC_FP1,		EXC_FPREGS+(1*12)	# offset of saved fp1
set EXC_FP2,		EXC_FPREGS+(2*12)	# offset of saved fp2 (n
	fmovm.l		U# But, whetherGS(%a6),&0xcbeq.b		fu_in_cont		# no
	bra.w		fu_in_exc_ovfl		# go insert overflow frame

#
# An exception occurred and that exception was enabled:
#
#	shift enabled exception field into lo byte of d0;
#	if (((INEX2 || INEX1) && R0_EX,		FP_SCR0+0
set FP_SCR0_SGN,	FP_SCR0+2
set FP_SCR0_HI,		FP_SCR0+4
set FP_SCR0_LO,		FP_SCR0+8

set FP_DST,		LV+56			# fp destination operand
set FP_DST_EX,		FP_DST+0
set FP_DST_SGN,		FP_DST+2
set FP_DST_HI,		FPEGS(%a6),&0xc0	#ST_LO,		FP_DST+8

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
set FPCR_ENABLE,	USER_fp0/fp1
	fmovm.lexception enable
set FPCR_MODE,	6)	# vector ),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXx7,(%sp)
	bne.w		fu_tr20			# integer scratch 1

set STORE_FLG,		LV+19			# flag: operand store (ie. not fcmp/ftst)

set EXC_6)	# vector 24			# temporary space
set EXC_TEMP,		LV+16			# temporary space

set DTAG,		LV+15			# destination operand type
set STAG,		LV+14			# source operand type
w		FP_SRC_EX(FLG,		LV+10			# flag: special case (see below)

set EXC_CC,		LV+8			# saved condition codes
set EXC_EXTWPTR,w		FP_SRC_EX(ved current PC (active)
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
set LOCAL_EX,		0			# e		%d0			# make e0x4(%sp),%d0
	rtd		&0x4

#
# Thisved in memory.
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
sean)
	lsr.l		%d0,01			# min dbl prec exponent
set DBL_HI,		ore d0-d1/a0-a1
per location
	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZE+0x4(%sp)
	movo or not...E+FP_Ms and DENOa6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%spion bias
set SGL_BIAS,		0x007f			# single precision bias
set DBL_BIAS,		0x03ff			# doutst	for Trace enabled exception		#
#	funimp_skew() - adjust fsave src ops to =s 3):					#
#	- T		FP_SRC_EX(%a6	EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovult result to its 
fso_infnan:
	andi.b		&0x7f,LOCAL_HI(%a0)	# clear j-bit
	ori.w		&0x7fff,LOCAL_EX(%ATION, OR OTHER PECUNIARY LOSS)
ARISING p),LOCAL_SIZEG

##################
# FPSR/FPCR bits #
##########EPT(%a6),%dNABILITY TO USE THE SOFTWARE.
Motorola assumesthe fsponsibility for the maintenance and support of the fFTWARE.

You are hte the SOFTWARE
so long as this entire notice is retained withoLOCALNNORM		+0		fu+0class 0 o- 2 re-ult to FP LOCAL resmentt to FP r_fix() two u
# XREF *h table entries located
# after _060Ffrom user mode, then let's updaRM or ZERceptionen we ha_unsupp - aile (_fpsp_done for
# example) that are refrdered
set snan_bit,		6			# signalling NAN
set operr_bit,unsupp - ad # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_s
# extra layer of hierarrame		#
#0	# keep only ones enabled
	c		# there ir mode, then let's 0x2024,0x6(%sp)
 table of emula_off_ope penalty but
# it makes the FPSP c- cherenced by the FPror
set ovfl_bit,		4			# overflow
set unfl_bit,		3			# underflow
set dz_bit,		2			# divide by zero
set inex2_bit,		1			# inexact result 2
set inex1_bit,		0			# inexact result 1

set aiop_bit,		7			# av.l	_dregued it,fcmp)
set EX68060 Softwarindex	fmo,&mda*      PC	*
#		*****************	*****************
#		* 0x3 *  0x0dc	*	* 0x2 *  0x024	*
#mask (byte)
set inf_bmask,		########
# FPSR individual bit masks #
#############################
set nd_TABea>" sx00		
set EXFP rv.l		03033). For	#
# exceptions enabled, this handler must exit through the "callout"	#
# _real_ovfl() #####	glob			#
#	- The tem sesrc o~~
MOTOROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocessor DA8060 Softwar(%sp),>" sk is ctk frF MEch###
Trang0x0303	g since  systiften-to Softwarepc,%dsp# snng*4)
set EXC_D3					#
(tractreal_bP_SCesRD,	
set EX#				es
###rt at 8). D0/D1/A0/A1/A6/A7		EXC_AL_SI0,-(%sre i (fpn# starr
_real******b#
# Athei
	cmbal		_ plac maskOSE
and any warranty against infringement with regard to the SOFTWARE
(INCLUDING ANY MODIx() - c~~~~~~~ leavesALGOintet oe, daubsequxc(%sl rim sta codefl eIED VERSIystem sta
ations, thR combinations u sta the FPSP #
######0############# _mem_read() 00(%sp),ed to FP z_mask+adz_ sta0
	mod to FP d	#
# as the sis se operand to the instru2tion specified by the inst3tion specified by the inst4tion specified by the inst5tion specified by the inst6tion specified by the inst7tion specified by the inst8tion specified by the inst9tion specified by the instation specified by the instbtion specified by the instction specified by the instdtion specified by the instetion specified by the instftion specifie
the soe d0-d1/a0EXC_DREGSh hen word
seerand		struce exiting. In either t4e enabled or disablePREGS(%aLOCAL_Sled or disable3e d0-d1/a0-ak
sed or disable4e d0-d1/a0-a4must create a T5e d0-d1/a0-a	pea. or disable6e d0-d1/a0-at,FPception	#
# 7e d0-d1/a0-a7f no Trace is p8e exiting. In either t8e enabled or disable9e exiting. In either tce enabled or disablet"
	smi.b		0aending, in whichvector offsea must create a The
# tration.n stack frame fr******	#
#	%at exception	#
# #
# The  EXC_enabled or disable# operanng. In A7 exception can *************************************************	#
#	On the 060, if an FP overflow is present as the result of any	#
# instruction, the 060 will take #
#	For im_rflowtract_%a6)wor regs
ate tr
set EX0FPSPfi4
set  ine	#
#									#
# OUTPUT **************************************************************	#
#	If access error:						#
#	- The system stack is changed to an access error stack frame	#
#	If FPU dis~~~~%a6)owrd					#
l		stoles eption sabled:						#
#	- The system stack is changed to an FPU disabled stack frame	#
#	If Trace exception enabled:					#
#	- The system( executed fromd1
	I,		4
4(%sption stack frame	#
#	Else: (normal case)						#
#	- None (correct result has been stored as appropriate)		#
#									#
# ALG,All rights tion	#
# 	glob fp0/fp d0
	pea.l	************** executed froructionnded precisor pac##### MERCH
set FP######ssing modl,%pc			#
# (2) TRPOSE
and any warranty against infringement with regard to the SOFTWARE
(INCLUDING ANY MODIFIED VERS the offendi
 the offendiR combinations s sta?
	beq.b		fu_e##################tack frame. This incked), atack fz_mask+adztack fource optack f instruction toction spthe	#
# new stack fraruction	the	#
# new stack fraexceptiothe	#
# new stack frad ads a the	#
# new stack framulationthe	#
# new stack fras storedthe	#
# new stack fra registethe	#
#
ion to e d0-d1/a0-a1
 In either the ena# no
	ck framame from the current stackso cherame. This ixception is pnstr rame. This i case, we	#
#le o3rame. This irace exceptiole o4rame. This iom the currenle o5rame. This istack frame. le o6rame. This iending, we sile o7eans the current stack frame must be changed	#
# to a Trace stack frame and an exit made through _real_trace().	#
# For the case of "fmovm.x Dn,-(a7)", where the offendwng instruc	#
# was executed from supervisor mode, this handler must store the FP	#
# register file values to the system stack by itself since		#
# fmovm_dynamic() can't handle this. A normal exit is made through	#
# fpsp_done().								#
#	For "tion is tam_ctrl() is used to emulate the instruction.	#
# Again, a Trace exception may be pending and an exit made through	#
# _real_trace(). Else, a normal exit is made through _fpsp_done().	#
#									#
#	Before any of the above is attempted, it must be checked to	#
# see if the FPU is disabled. Since the "Unimp <ea>" etion is taken	#
# before the "FPU disabled" exception, but the "FPU disabled" exception	#
# has higher priority, we check the disabled bit in the PCR. If set,	#
# then we must create an 8 word "FPU disabled" exception stack frame	#
# from twe current 4 word exception stack wrame. This includes		#
# reproducing.w		%d0			# is address of t####nstruction tw put on the	#we, we will haveme.							#
#s	fsabs		facos		the process os	fsabs		facos		work, if a _ms	fsabs		facos		callout" retus	fsabs		facos		sult indicatis	fsabs		facos		or, then	#
# s	fsabs		facos		n access errow stack wrame fro80,%pc,2+urrent stack	#
# frame. This wnformati1		ftanh
#	fneg	fdneging address and awPREGS(%a80,%pc,us-longword. Tw case, w0
#	fsqrtithin this hwrace exc0
#	fsqrt			#
#######wom the c0
#	fsqrt############wstack fr0
#	fsqrt############wending, 0
#	fsqrtfpsp_effadd
_fpsp_effadd:

# This exception type takes priority over the "Line F Emulator"
# exception. Therefore, the FPU could be disabled when entering here.
# So, we*
#		tract_by,%d0as executed from supervisor mode, this handler must store the FP	#
# register file values to the system stack by itself since		#
# fmovm_dynamic() can't handle this. A normal exit is made through	#
# fpsp_done().								#
#	For "P_SRCE	# init stack frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

# PC of instruction that took the exception is the PC in the frame
	mov.l		EXC_PC(%a6),EXC_EXTWPTR(%a6)

	mov.l		EXC_EXTWPTR(%a6)PTR(%a6),%ken	#
# before the "FPU disabled" exception, but the "FPU disabled" exception	#
# has higher priority, we check the disabled bit in the PCR. If set,	#
# then we must create an 8 word "FPU disabled" exception stack frame	#
# from tbe current 4 vector oeption stack brame. This includes		#
# reproducingbble
	bne.b		i address of tore nstruction tb put on the	#b)		# is lw 2 zeme.							#
#ea_op_gp_not_spethe process oea_op_gp_not_spework, if a _mea_op_gp_not_specallout" retuea_op_gp_not_spesult indicatiea_op_gp_not_speor, then	#
# ea_op_gp_not_spen access errob stack brame fro_mask,	3
#	fneg	fdneg	fsneg		fgetexp		bnformati
	fmovm.x		&0x80,FP_Sing address and abPREGS(%a6),status-longword. Tb case, wa6)	# updithin this hbrace exca6)	# upd			#
#######bom the ca6)	# upd############bstack fra6)	# upd############bending, a6)	# updfpsp_effadd
_fpsp_effadd:

# This exception type takes priority over the "Line F Emulator"
# exception. Therefore, the FPU could be disabled when entering herinc_astack cinc	addd0)
	ypes of operationset FTEM				#
# AL0e, this handler must store the FP	#
# register file values to the system stack by itself since		#
# fmovm_dynamic() can't handle this. A normal exit is made through	#
# fpsp_done().								#
#	For "amouy have/ZERO
	movbyis used to emulate the pes of operations operation f a Trace exception may be pending and an exit made through	#
# _real_trace(). Else, a normal epes of operations:ough _fpsp_done().	#
#									#
#	Before any of the above is attempted, it must be checked to	#
# see ifTypicodeyrest60FPSPan/fp1
	fmovm.l	/ aerant-/ZERO
	mov<ea>ola Inc. w/ 2 or 3 0,EX)		#
#	ERO
	movw optype tabut, we pes of operation0,-(%ssupervisor mod.  using extenres

# Tdisabled" eked immedia6)		# ken	#
# ng mode.							#
# (2) Tegardless ofa7,%sp),LOC/ZERO
	mov.)	# ispropnC_EA(%spAN,		0x0tdefaulperation fsin twok (bytany a7ugh _fp,lication mia7_flagtack

OSS)ft througaccof oerr,-(%s,%d0)
	mov.l	s~~~~1
	fmol,	0x5led.bne.b	pes of )
	mov.
set EXgh _fp###
Tbe_bytHER EXPREOSE
and any warranty against infringement with regard to the SOFTWARE
(INCLUDING ANY MODIFIED VERS NORM/DE
 NORM/DER combinations iM/DE the FPSP #
###################G(%a6)	# is operatcked), aG(%a66)		# is lG(%a6ource opG(%a6tract:
	clr.l	ction spv.b		FPCR_MODE(%a6)ruction	v.b		FPCR_MODE(%a6)exceptiov.b		FPCR_MODE(%a6)d ads a v.b		FPCR_MODE(%a6)mulationv.b		FPCR_MODE(%a6)s storedv.b		FPCR_MODE(%a6) registev.b		F
lr.l		:t overflow op_done().								## no
	(%a6),a1

	mov.l		(tbl_unsupp.lvm_dynd1.w*4),%d12a1

	mov.l		(%a-longwo%a6),%c,%d1.l*1)

#
#ithin td1		# c,%d1.l*1)

#
#			#
##mov.l	c,%d1.l*1)

#
########&0x0,%a1

	mov.l		(	(tbl_unsupp.l,%7: regs
	fmq.b		ieg,SPCOND_FLmpanyix4

	global		_d0xdone

tb%a6),%a0bat overflow othough, whins that can ore cdds thi STAR
#	INEX2	: same a*************************************************	#
#	On the 060, if an FP overflow is present as the result of any	#
# instruction, the 060 will take deORM/DENORMdeERO
	mov.b		%d0,STAG(%a6)		# set new optype tag
iea_op_getdst:
	clr.b		STORE_FLG(%a6)		# clear "store result" boolean

	btst		&0x5,1+EXC_CMDREG(%a6)	# is operation monadic or dyadic?
	beq.b		iea_op_extract		# monadic
	btst		&0x4,1+EXC_CMDREG(%a6)	# is op%d0	# fetcincos,ftst,fcmp?
	bne.b		iea_op_spec		# yes

%d0	# fetaddst:
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # fetch dst regno
	bsr.l		load_fpn2		# load dst operand

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	mov.b		%d0,DTAG(%a6)		# could be ANYTHING!!!
	cmpi.b		%d0,&UNNORM		# re-%d0	# fetcUNNORM?
	bne.b		iea_op_extract		%d0	# fetcl		unnorm_fix		# yes; convert to NORM/DENORM/ZERO
	mov.b		%d0,DTAG(%a6)		# set new optype tag
	bra.b		iea_op_extract

# the operation is fsincos, ftst, or fcmp.%d0	# fetchis dyadic
iea_op_spec:
	btst		&0x3,_op_exit1		#(%a6)	# is operation fsincos?
	beqdb		iea_op_extract		# yes
# now, we're left with ftst and fcmp. so, first let's tag them so that they don't
# store a result. then, only fcmp will branch back and pick up a dst operand.
	st		STORE_FLG(%a6)		# don't store a finaENABLE(%
ENABLE(%R combinations d(%a6)	# is operation fcmp?
	beq.b		n overflow frame icked), an ove6)		# is ln oveource opn oveid overflow ocction spq.b		iea_op_save

iruction	q.b		iea_op_save

iexceptioq.b		iea_op_save

id ads a q.b		iea_op_save

imulationq.b		iea_op_save

is storedq.b		iea_op_save

i registeq.b		i
ow occ:red and was tbl_unsupp.l,%pc,%d1.w*4ve

ieexception type back into
		(tbl_unsupex2_biexception typ
# Exceptis ine be 0-8
	cmpi.bithin t	iea_o be 0-8
	cmpi.b			#
##	iea_o be 0-8
	cmpi.b#######an enaexception typ		# fix offset t normally opd0	# kOVFL	: same as OPERR
#	UNFL	: same as OPERR
h an oea_oeption type baNEX2	: same ace an o6)		ubX1	: all packed immediate operations
#

# we determine the highest priority exception(if any) set by tack framaybe NORM,DENORM

# bit five of the fp ext word separates the monadic and dyadic operations
# that can pass through fpsp_unfl(). remember that fcmp, Sof_fpn1ff_i SoftFPhat they d				#
# to :
#	RC(a6)) The "fmovm.x" inmust store the FP	#
# register file values to the system stack by itself since		#
# fmovm_dynamic() can't handle this. A normal exit is made through	#
# fpsp_done().								#
#	For "d:						#

tbl_iea_extppliasrc opera#
# exceptions enabled, this handler must exit through the "callout"	#
# _real_ovfl() t		0xe002,tack is cR(%a!!!
303	

tbl_iea_exv.l	one().	#
#									#
#	Before any of the above is attempted, it must be checked to	#
# see ifULE-0x80,%ea>" stk f0*******me

	fmovm.#######e		#
#	a6),&cision o0xc0	# restore fLAR PURPOSE
and any warranty against infringement with regard to the SOFTWARE
(INCLUDING ANY MODIFIED VERSexit with
exit withR combinations exit withbinary (if pa2), t", converted te opclass two instructcked), ane		# exit task+adzexit with_ource op		_fpsp_doon was being tracection spthe "current" PC the FPIAR aruction	the "current" PC the FPIAR aexceptiothe "current" PC the FPIAR ad ads a the "current" PC the FPIAR amulationthe "current" PC the FPIAR as storedthe "current" PC the FPIAR a registe		_fpsp_doone		# exi_rame from t0
#	fnFPhe enab 0ov.l	ut
# sp),%d0
	rt4	*	* 0x2 *  0x044	*
#		**************8	*	* 0x2 *  0x084	*
#		*******e package		*****, r, inSS Ihe FPIAR an		*      PC	*	* 0xn byte
024	*
#		*****************	****#					*******
#		*      SR	*	*     Nex#					******************	*      PC	*
#					*****************PREG-a1

	fresto20,0-a1

	f*******	*      PC	*
#					***************** cas-a1

	fresto1; voff = 0x024
	fmov.l		%fpiar,0x8(%sp)		# "Current Prace-a1

	frestor8 voff = 0x024
	fmov.l		%fpiar,0x8(%sp)		# "Current Pom t-a1

	frestor4 voff = 0x024
	fmov.l		%fpiar,0x8(%sp)		# "Current Pstac-a1

	frestor2 voff = 0x024
	fmov.l		%fpiar,0x8(%sp)		# "Current Pendi-a1

	frestore	voff = 0x024
	fmov.l		%fpiar,0x8(%sp)		# ******************	#
#	- The system stack contains the FP Unfl exception stack) # did underflow occur?
	bne.b		iea_op_exc_unfl		# yes

iea_op_exc_force:
	mov.w		(tbl_iea_except.b,%pc,%d0.w*2),2+FP_SRC(%a6)
	bra.b		iea_op_exit2		# exit wit2 frestore

tbl_iea_except:
	short			0x002, 0xe006, 0xe004, 0xe005
	short		0xe003, 0xe002, 0xe001, 0xe001

iea_op_exc_ovfl:
	mov.w		&0xe005,2+FP_SRC(%a6)
	bra.b		iea_op_exit2

iea_op_exc_unfl:
	mov.w		&0xe003,2+FP_SRC(%a6)

iea_op_exit2:
	mov.l		EXC_PC(%a6),USER_FPIAR(%a6) # set FPIAR to "Current PC"
	mov.l		EXC_EXTWPTR(%a6),EXC_PC(%a6) # set "Next PC" in exc frame
		# do .x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1movm_datore	FP_SRC(%a6)		# restore exceptional state

	unlk		%a6			# unravel the frame

	btst		&0x7,(%sp)		# is trace on?
	bne.b		iea_op_trace		# yes

	bra.l		_fpsp_2*********			flog10
#an "Unimpleme2 two instruction that took an "UnimplemeDREGS(%a6),&0x Address"
# excePREGn was being tra2ed. Make the "cur2bra.l		_fpsp_done

nd put it in
# ti_trace:
	mov.w		EXC then jump to _ri_trace:
	mov.w		EXCNIMP EA FRAME		 i_trace:
	mov.w		EXC**************	*i_trace:
	mov.w		EXC* 0x0 *  0x0f0	*i_trace:
	mov.w		EXC****************i_trace:
	mov.w		EXC Current	*	*****2**********2
#		*      PC	*	* 0x2 *  0x024	*
	0x000**************	*****************SER_FPCR(%a6),%fp     Next	*
#		******SER_FPCR(%e packagSER_FPCR				***************2*
#					*      SR	*
#					*******SER_FPCR(%a6),%fpcr,%fpsr	mov.l		(%sp)tore ctrl regs
	movm.l		Eame "down"
	m,&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
	mov)	# stk fmt = 0x2; voff,&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
	movC" is in FPIAR

	bra.l	,&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
	mov#######################,&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
	mov ctrl or data reg
	beq.,&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
	mov,EXC_SR(%a6)	# user or ,&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
	movmovm_data_u:
	mov.l		%u,&0x0303	# restore d0-d1/a0-a1

	unlkre current a7
	bsr.l		fmovm_dynamic		# do dynamic fmovm
	mov.l		EXC_A7(%a6),%a0		# load possibly new a7
	mov.l		%a0,%usp		# update usp
	bra.w		iea_fmovm_exit

iea_fmovm_data_s:
	clr.b		SPCOND_FLG(%a6)
	lea		0x2+EXC_VOFF(%a6),%a0
	#
#	Fofpstack ctract_aence%a6),%a0	#,-(%sp immdeor t(_060d0CMDREG(%a6){&0:&6
# OUTPUT **************************************************************	#
#	If access error:						#
#	- The system stack is changed to an access error stack frame	#
#	If FPU di~~~~~~ABLE-0x80,%pc,%d0)
E	# init stack fraif any) v.l		EXC_PCfloav.l	-

	spert to NOR). For	#
# exceptions enabled, this handler must exit through the "callout"	#
# _real_ovfl() mask (byte)
set inf_bmask,		0x02			# infinity bit mask (byte)
set z_bmask,		0x04			# zero bit mask (bySl rights 				#
# A~~~~%d0)

	

tbl_iea_ex%a6,%d0)		#et FTE#####w optype taked imFP_SRC(%a6store a

# the exexact######8(%sp)

b3:
	lscarefas dtrac - read i
	mov.l		%d0,-(%s_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_snan
_real_snan:
	mov.l		%d0,-(%sp)
	momov.w		&0x2e currepea		ord exception st6,%d0)
	ad two instruction that took an "Ufm_6:
	lsl.b		&0x1,%d1
	 address 6,%d0)
	addi.nstructi6,%d0)
	aded. Make 4,(0x4+0x8,%fm_7:
	lsl.b		&0x1,%me.								fm_end
	fmovm.x		&0x01,(0x4+0the proce		fm_end
	fmovm.x		&0x01,(0x4+0work, if 		fm_end
	fmovm.x		&0x01,(0x4+0callout" 		fm_end
	fmovm.x		&0x01,(0x4+0sult indi		fm_end
	fmovm.x		&0x01,(0x4+0or, then			fm_end
	fmovm.x		&0x01,(0x4+0n access ,(0x4+0x8,%%a6,%d0)
	ad
#		*-a1

	fresto80, *	* 0x2 *  0rame. T0x01,(0x4+0xbsr.l		fmovm_ctrl		# loadn byteregs

iea_fmovm_ex)	# stk fmt = 0xv.l	ighest pperand type
	cmp  0x2;regs

iea_fmovm_exC" is in FPIAR

ER_FPCR(%a6),%fpcr,%fpsr,%fpiar 1 restore ctrl regs
#################R_FPCR(%a6),%fpcr,%fpsr,%fpiar 08regs

iea_fmovm_ex ctrl or data regrace on?
	bne.b		iea_fmovm_trace			#
###############C_SR(%a6)	# is trace on?
	bne.b		iea_fmovm_trace-longwor############C_SR(%a6)	# is trace on?
	bne.b		iea_fmovm_tracel		(_060Fmaybe NORM,DENORM

# bit five of the fp ext word separates the monadic and dyadic operations
# that can pass through fpsp_unfl(). remember that fcmp,get_packep_unstem sta  frameoper placa6),&memory0xc(%sp)fpsp_fl
	bfexcoalid f and wit6),%d1	# fetch binary_SRC(%areate final sp
	bra.b		iea_fmovm_data_p3

iea_fmovm_data_p2:
	mov.w		EXC_SR(%a6),(EXC_SR,%a6_d1+8
_eaa.l		_fpsp_ovflt			# yes

#UNNOd1
	mov_mem_
setSRC(%tem stfpsr,al_trace().
#
#		 UNIMP Eo NORM,acc_in_xSRC(%et	_oem staaie:
	sofp0. but,0FPSP_TAn't tion FPCR_ENAbi	# a  l		_E
#		*********
	ad******ABLE-0x80,%pc,%d0)+17) || (dec. exp exceeds 3 digits))
#	OVFL	: no
#	UNFL	: no
#	DZ	: no
#	INEX2	: yes
#	INEXmask (byte)
set inf_bmask,		ns enabled, this handler must exit through the "callout"	#
# _real_ovfl() obal		**	*u,LOCAL***********: fsincos, e

	fmovm.x	*********
#		* w		&n
# n*******l.b		fm_4ne().	#
#									#
#	Before any of the above is attempted, it must be checked to	#
# see ifGos?
	bex2 *  0x024	tionh(%sp)ovm.x		&0xet new _real_operrd" exceptfrS OF /it an ff.
#,%d0).b		with thstore aL_SIZis -(.b		or trac+CMDRE(fpn-to*	********ce().
#
#		 UNIMP E
	bts************	*s&6:&UNNORction0FPSP_ext	*
#		**LAR PURPOSE
#######*********
#		* _flg
	bne,#####)	# oF,AME
#		*******&0x3,1t_EX(*****represent****** 10, 1#####
codet to		*   RITHM ramexceptioalid freally haveE	# init P_TABLE-0x80,%pc,%d0)
 create	globCMDREG(%a6){&0:&6},%d0
	cmpi.b		%d0,&0x13
	beq.w		fu_in_pack


# I'm not sure at this point using re ied.l		_r

##*******- chc,%d0)
eft wimic
#-(AnTWPT15},%dange immX,		Fe a I,		4
s

##t

##g mod0xc(% mod+e data opertack frame
tack framee d0-d1/a0l

# uset EXC_hich can12%d0	#3a	#mul in*********		EXC_CMDRUNNO;n use mosA(_06*	*      PC	*
#		%aP_SCR1_HI,	0,-(%spsuub.l****ame

	movm.l		&0x0303,		%dS(%a6)	# save d0-d1********) # waadlly have taken _exc_exit
********nex_o********	*bal		_r_TABLt	*
#		#	SNAN	: #####lly have taken the "G2 TRAIL
asp_instore as:
	bfextuiel the "NORMnvfl ebfextu      PC	*
#	{&1:&15}x80,%pcpsp_PSR aBILIT80,%pc,().
funfl_i TRAIL
NANbal		_realgp_trylt resuion prese fsincm_read_long		# fetch tin anybsr.l		_imem_read_long		ZERO
fu######t opclass,sr0,EXd an UNls0x80,%nd in fsavelly have t.
vm		# yes
#.l		USER_FPC exception isWORD

	tsP_SRC4
set EXC_FP2,	(_060FPSP_TABLE0,EX####lasrc yb			# dis_fmovm	 andNORM instr	#
#sponsic_exit
SIZE(%
# and co input w 2_dis_ea_dis_fmovm	 ctrl
	bne.b		iea_dis_fmovm_data	# no
l		(_06nstruction3is a fmovm.l with 2 or 3 registers.
	bfexttion is using an extenre	FP_th 2 or 3 rratch 1
set eption is et EXC_OPWOR,-(%sp********* save d0-r 4-woPSR+2		#means  offsets wa0-a1

	frestore	FP_S= 0x024 "calloune.b	#######m.x 
# exception was being traced. The "Current PC" for the trace frame is the
# PC sr 4-word: Cse manemulestore fp3,EXC_Dbccalculat

	spa******te a to NORM/DACE Fax4+0xABLE-0x8-_fmovm_data_p3:
20,(0xnfl
_rea####################
# FPSR individual bit masks #
#############################
set neg_mask,		0x0800oduct, will tell us how lon a6 used to be
# -add LOCAL_SIZE to stack pointer
iea_fmovm_trace:
	mov.l		(%a6),%a6		# restoe final ters.p)
	n 8-word stac result tell us how lon 0xe006, 0xe004, 0xPROCESSOR & MEMORY TECHNOLOGY GROUP
M68000 Hi-Performance Microprocessor DExp		(_060FP.
iea_diss ho(i.e. yes
d0
	rtd		&0;btst	info the ola I_DST NaN0	# fetch excedisp****e
setth.
Moespac%a6),%d1
~~~~~~~ff_bs	r.l		fmov68u_out_trpea.		#
t loc******(a0nd so, this handler 	A1.as a by-15},%dcculated her**********byFPSPow, fpspextra####nt Pmulswo ins0-d1/a0-a68060 SoftwarSE.a6		 condi16un,%pompensat****
#u		EXC_C those liRITHM P_TABLE-0	spacpre insas 17##

ig
iea_didigitPLIEa dbl?
	an regispcr,0xc(%16&LOCr stacbtst		) The "Note:differP regfile	_fpu_e)

et FPCR_MOp)	# move Current PC2	add.l		%d0,0x6(%s those linkiext PC
	mov.w		&0x40t inf_m,0xa(%si.b	and  FP0rt offset,frame format
	mov.lM) The "sr.l those libtst		s, and thuse man%sp)	# .w		iea_i_dis_
	spp)	# mssum)+,%dllowE-0x80,%least-or tificanor Sgisp			R(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	fmovm.x		EXC_FPRE3	add# is fso_		#
#	ofSR,h****/trairap
_ARRADREGSm_3:0x4(%6(%sreg
ng. 	btsSEhe ex the ad" ex0x2(%sp)# storel		&0x;rt to fk all done:
	btst		&0ea
	mov.l		&0x. t offset,adx000esp)	# umber.
	global		fix_sks:
	bfextu303	A10xc(%sp)
ARRAN
	bts4	*
#		*ch cf SM = _acc_dnadic
	ccess

iea_d0e2			global		p)	# ithose linrvisquival	mov(%a6orc	beq.AL_SIZs how lon:)	# move Current PZE(%a6	in fs
	cmp0xc(%sacc_cont	iea_iac,%pc,%d0%a6),&0xc0	1 restore fp0-fp1 on sMantd0-dsd,
# with	fmovm.l)	# move Current Pne.b	rame insoint				#
nstructi_reagardlof ooAL_Ss: no
#	Sn 8-word stac(ex. 0.1E2, 1E1, 10E0L_SI0E-1), can umake st0x4(%consset ntlyLAR PURPOSE
and any war	A4. C     PC	*	* 0with the0^exrom  FP1pi.w		%a t		(_ onfl_
#	10^(2^n)0x8000,.  T

	aask,		0x0e're l20,(oeptiga0,-0x8 so t	g		(_00
	bne.b0^27,
	mov# inexa7
	mov.l	schem	&0x3NG!!!)	# t:
	mo		(_	clr.+_off_drRN, .l		l		ERP,e format
	mov.beq.b		(_rt to N*	* 0x2m
	mop),-0with wrt#$#	mr stadisabled bit in the 	A5k (by# resu_exd0,-*********
#	
	mov_trap
_real_fpu_disabin th	ervisor TM bitshortE(%s1		# exesul
	mo	Also, iE-0x80,		%pcr,%d1
	btsEGS(%a# set nek frame
L_SIZE the to	bset		&00x2,0xd(%sp)		he prope_acc_done:x4

	,%pced.

Tcepti		#
#	should0x4

	 table o0x4008,-0x8+0xa+LOCAL_SICleonadpd opclass twoception was eXDEF *r thvideives
	roffset CMDREG
	mov.lxact wEXregiro all butWARE is prget the "next PC". This is trivial for
# immediate operands but requires some extra wo
#	PTENL_SIe		#
E-0x4,%e		#
P3			# rrayp),-0powerp),-010fetch sr****o neas hi, minu,%d0
	pplite_****#### to #
#	__done

****clude0x8+0**{1,2,4,8,16,32,64,128,256,512,1024,2048,4096}.  Noa7
	mov.lnd ERD,	quirdjusandi.with ing 
###p)

	movm.l		AL_Showe)


###l1/a0-a1
	aem (opc*******%d0,5d0,-(%,&0xc0	#				stor
#
RTff_dper yte		0,*************2,3,2,3************3,ABLE*************
	),%d0FNIBS,7containsSTRT,_CMD),%d0Eexcep4on frameDIGITS,unno enabled? ibin thebiw		(	short		0he el.s:
#	This file is aset NORM,		0x00d by a
	short		0xe0l		(_060FPSP_TABLE-ne. ##### an
set LOCA giv	short		0				e exception frame "doe package. The user, i					m# dyadiA6,		rtd		&0x4

#
# Th-passs in FPIAR

	RD(%a6)

#######0)	#nex2 fpsp_ovflT_EX(%sp_s

****C,		0s how longinUNIMP EA

##threemov.workCAL_Sopyes.
readTHM ******t" for Inexact),-0+_off_inex ALGO): 060ABLE-0x8tem wv.l		,%d0)
0,-(%se the firsttem w4	(%sp)+,%d0		# restore d0

0FPSP_

iea_iaE-0x80,%,%d1*****%a6),ea_iac	USER_Fem w  sp)		#ss erbq.w		&0(%sp),(%lefbne.bame bigger
	mov.l.60FP# Rt they dosaion.ncors wit_ cas	(*)  d0: te####fp1 ol reagclasds to 1:e fou,	0x5r othe*********
#######dler cal2:ected. 
	bts this and3: offR0_EX,opcla thi  to 4:				#
#	#
# of****	For opca0:,		0x0200000#
#	In as how lonthe 060 d6esn't store te.							lt	#
# operr s to#	_real:	#
#	In a systiony or data register file asl		0x1:This code must emusun
_real_br on 
# and name

	movave framper pla#tionni# iss (btst		),-0x8fmovm.l	par		EXC_PC(%ae	#
#	%d3SR+2		uspace		5pick#			btst		. A Trace ex be t#####psp_ 3 instructions, t%a6		# r********e fp0-1TAL "Lix_skewed_
e_sult oulu# dyadia6),&0x03#
# writcheck_maskiea_oerr(cted. #
# (ORD(%a6)	#%d4{%d3:&4EXTWORs specisp)
	mp1 o0
	pea.lata:
	c
	shorno T%d1.l*1)

#
q.l		&0x_TABdinst#######PERR
&n st3extensiv0,-( d34+0x8,%anopty#####
	dbf6)
	bne.h crNORM		#%a6,%d0)NG!!!e	#
3failing###### lom.x d_060FP+_off4C_CMDpsp_SEla assumee_poion fi an
se tabl	&0x4pos and inex########EGS(%a6bt FP_or snan,opng
		&0x&ovfl_# dya1t,FPC		# FPSRn the 060,	#
# for shin the ,%d15,1+EXC_Ce_a6
# are vf     adpos,w		& OTHE
	fm d0-d1/a0-a1
	fmowupervisor mt		0xpon
# un
# ifEc frame i

fuor
# exathis son
	mo the 4,v.l		USER_FPIAR(%a6)fl_masr is niate  default	
 fp0-fe d0-d1/a0-a,2	#
#									#
+LOCrdware is#LGORITHM ******,%d1
	bt********* the FP Operr exception is or FP Operr ~~~~al	#
# is to get toGS(%ahandler sp,%d1
	btst _real_oeption, the	#
# input operand in the fsave frame may be incorrect for some cases	#
# and aneds to be corrected. This handler calls l	#
# that ma this and then exits through _real_operr().			#
#	For opclass# _rs re cle3tions, the 060 doesn't store the default	#
# operr result out to memory or data register file afp0:060FPSP_TAB()	#
# whicFor opc it should.	#
# This code must emulate the move opc before finally exiting through	#
# _real_inex	fu_in_ea6) ##########we simply se	bne and wtore ctrp_done for
# example) Ns also becomases	#S		&0x7,th the "n,0x8(%son
# n%a6)bsr.l	betwEMP_							#
#&(%a0)	#k frasng()				#
###ion taking thnc.  Alal		 dete&				#
###s highe fsa res,%d1
	bt.  Weset FP_Dronow, _FPRo# inc or ZED(%a6)	##
# I{&28			#
#			3,1+fmovm.l) FPput s0-fp1 on smovm.l		Uxact!);a6)	# upNCLUDINEXC_movm.l	ame
m in fp0
#~~~~  Get the rest of~~~~~mantissa.
#
loadlw:
	mov.l		(%a0,%d1.L*4),%d4	# ~~~~~~~~~~~~~ lonqword into d4~~~~~
MOT&FSTRT,%d3		# counter to pick up digits000 Hi-PerfNIBS,%d2Micrreset number~~~~
M6806 per a0 ptr
md2b:
	fmul.s		&0x4120re P,%fp0	#~~~~ =opyri* 10
	bfextu		%d4{%d3:&4},%d0Micrg~~~~~~~
M680 and zero extend
	fadd.bMotoackage  Copyright ©+served~~~~~~~~If alls reserveds (8)~~~~thatHNOLg GY GRhave been converted (d2=0),~~~~then inc d1 (=2)sor Dointsor ~~~~nex permitted b

THuctiond3sor 0DISCLo initializes reserved.offset,,
INCDING 2ARRA7 fors reserved.oproc;~~~~elseble tinue with thispermitted ~~~~	addqvide&4nce Mi	# advanceG IMPLIEXPRESS Od with	dbf.wMoto2,, 19MicroheckRTICUlasIED VER exteningew
ESS ~~~~~ard t-Per1A MITWARE ALLlwS WHETer~~~~~~~~~~~~
	cmpimum%d1,&2TWAREt~~~~mpanying lw
	blevide~~~~~~t permf not,ightsying one~~~~~~C acco~~~~sign~~~~~~~~~~~~,
INCmakMERCHAvalue~~~~~~~TATIONamION, D~~~~m_N, D:
	btst		&31,OROL)L MOTOROLN, DAMAGES FOR Law,
INbe to tap_st_zS WHATSOclear, goARRAappend/stripHE SOs
	fneg.x		 an "AWHATSO OR Fnegate~~~~~~~~ ATO USE THE SOFTWA:~~~~~~For adjus,
MOexponents whichby appan absolutESS PROFgreacessohan 27* DISCLAis routine calculatesTATIOamproc neededARRAnorm OF MERCHAle law,
I# RTICULAR of the SOFTWARE.
.  Tnt p Releasis subtrac,
MOfromTATIOexps rei~~~~~~exp was positive FITNEad lonif itt sucponsiive in ae purposes re~~~~~ised ato reducUSINESS PROF~~~~~~~FTWARE.
,
INC~~~~possibilityNo liceerrolicabistributioDAMAGpwrteRUPTI#  1. Branch oxtenION, DAMAGES Fithout alteration .
~~2p.(h modifistop).
~~ 2.T LIMITM16r otherwihe maxiin lGY Gs 2,
INC3 topdecending order~~~~~ 3. AddNG WLA BEeachHE SOFTnoprocesed until a non-E SOF
M680tains 4. Sd/or
re~~~~~oprocstributed vertains 5is file s, and thathas crosseTHE SOFin #3 above;OF BUSINESthatabs
#	  FITNESS FSE.
#	6. Multiplytire notice i by 10**oproc~~~~~~n.(y identieal.s:
#	This file ded to the top of the3,
INC2P package
# and contains the entry points into the package. The user, in
# effect, branches t entthe branch ANY MOtries located
# after _060FPSP_TABLE.
#	Also, subroutine st OR I_done    6. Dividntire notice ieferenced by theP co*Why 27?arran~~~~~~~~~~~~~~~~~~~~~ed ast iin -28 <of hA < 28,censeP codany~~~~~~~mnan,dueLITY TO USE THE SOFTWA will drge i~~~~~~~ulta WIT  stoppel ortowa theE SO.  SiCLUDty.
rola, ble sta.

Yot ina powerx18
sof 27 panyess ar of act8
seereed anoE
so  layuset	_dify, and dto_off_ottempa lay,	0xe~~~~~~f_inex,	t~~~~~~~~~~~~
# Regisppliusagend suppSING OF:
#	(*)  d0: ,	0xserved.storageirl,	0x501:HE SOFoprocirl,	0x502: PURPOSE
andirl,	0x503:LITY OR by applirl, 0x504: firsttted bof bcdirl,	0x505:p of troprocesx64
set	a0: by applitottedk# anbcdSS PROx64
set	FP_SCR1:#########copy

_0original##################L

# Hertable of ENTRY POoff_snan,GY G~~~~~~~Fdwl,	y acco~~~~~ranted a copyrion, estoppel orto seefter _is
#fy, and d,	0xecessary.arransof_accn00
	bra.l		N, DAMAGES FFTWARE.

#,
INCdTY TO US (+)race THE S(-)	_off_daccor
# alySP cTe,	0sec of Mhandlesp,	0x~
# fre~~~~~~~~~~~~~~~~~~~~
et	_off_i~~~~
MOTORspROCE1sume & MEperr,TICUrangeOTOROIN NO EVENT SHA7LL MOTOROL	0x04
s 27 FOR Aw		rola, ssumes nabs(perr) <x08
skip ap/stSOFTWAREOF BUSIN0SS INFORMAy accoN, DAMAGvers	bn ANY SING O#########negNABILITYneg side
	clr EVENT ssume	0x58
set	 reg~~~~~
MOTOROLROCESSfadd
	sh######1a.l	6800 1994 Motoro&28Inc.  All rightsis a pac93, 060FPSP_Tp_fxssumes nv.l			0x2
# effNABILfixv.l		( maximum extent permitt	0x58
set	_00 Hi-Perexte5t permiiABLE############~~~~
MOTOROLA M5CROPROCESSOR(INCL0
	rtR A P	globsp)
	mov.l	clssumes nlwe st0x1c
slobal		ERSI maximum 8xtent per

TH ALL0x80,%by 8%d0
	pea.l		(_0	0x4(%sptted
	rtd		&0x4

	global		_real_unfl
_real_unfl:
	mov.IMPLId4ort	l		(:ea.l		(_060FTWAREsp),%et	_offpc,%d0)
	mov&7age
PrPSP_TABLPURPOSE
ander0,-(%sgdSS O1994 Motorola Inc.  All rightsD VERSI%sp)
	mov.l		(_060FPSP0x80+_off_ovftvfl,%pc),%d0
	pea.l	e SOFTWARE WHETHER DIFIED VERSI maximum extent permitt_060FPSP_TABLEIONS THEREOF-0x80,%l rightsDIFIED VERSov.l		(000
	bra.lNT S All icroppyroprocessor d200
	bra.l		_fpsp_effadghtsof the SOFTWstribumemory
	subal		_rLA MIperr
nd/or
re branch tabl.l		(_gp)
	mov.l		mssumes not,	0xposNABILITYrola, 
	
Mot(_060FPSP_Tnow maxineg;	(_060b60 Softwar.l		0x4(%sp),%d0
	rtd		&0x4

	gloo		(_0Softre PaPack(%sp), (_fpsp_do		%d0f_snan,%pc),%d0
	pea.S INFO_060FPSP_off_ope0x48Cstributeto read and mom exampi######compenssibilICULAR  THE # andf
_TABLEsstributed ~~~~~~~~~~~~ov.l		0:
	leaal_sPTENRN(%pc),%av.l		(_060Fdr	0x2otorx24
-of-	0x2tablpea.l		(_06060FPSP_TABL	glob index4

Mov060 Sof3f8re Packag1PSP_TABLfp0x4

1c,%d0)
	mo3,%d0
	pea.l		(_R A PABLE-0xthe topSP_TABLE-0x80e)
	masan,%pceal_operr
shift lsbROUP
Mcaroperbcc)
	mov.l	##########1,z:
	TABLEbydis,	0x2factor4

M680x	mov.eal_3)		(_06	#fpu_dference(d3_bit_no)pc,%d0)OSS prov	0x4(2 SOFTWARE ALL IMPLIESS Orfline:entopertstpc),%d0perr
_
# afterd0LE-0x80+

	global		_ree(_060FPSPEVER
(INCESS ObERSI(%sp)
	m	(_0n an "AS pu_dad and more main(nofpu_s_	rtd	ed)
	bravide###########ggloblc	global	0x48	_fpsp_fline
	short		0xpackage i.l		_fpsp_unsupp
	short		0xOSS .l		(_060FPSP_Tclrtd		&0x4

	global&60FP	0x4(%_offion
5TIES WHETHER x:
	movx4

	global		_real_inex
_real_inex:
	mov

	global		_n		(_060FPSPnop_done,	&0x4


M68060 r,%pc),ov.l		0x4(%dec%d0
	pea.l		(_060FPSP_2,%pc),%d0
	pea.l		(_0SP_TABLE-er0x80,%p	global		_real_unfl
_real_unfl:
	mov.l		(_	glo000
	bra.l&2pea._060FPSP_TABLE-ying writtf_inex,%pc),%d0
	pea.l		(_060FPSP_TABLE-0xn0,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_neal_bsun
_real_bsun:
	mov.lLE-0xix:
	mo
	mov.l		(_060FPSP_TABLE-previous+_off_bsun,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	m
	mo		0x4(%sp),%d0
	rtd		&mr,%
	global		_real_operr
_real_operr:
	mov0l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_operr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%dt+_off_imr,0x4(%sp),%d0
	rtd		&0x4l,%ple law,
IN_real_snan
_reat BUSab),%d0 in tnd	(_06SE%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	gloan.l		(_0xbfword
_.l		(_060FPS,%d0)
E-0x80,%_imem_read_word
_im4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_06 TO US_TABLE-0x80+_of ANY MO,%d0
	pea.l		(_.l		PSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fline
_real_fline:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fline,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%n0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_dnsabled
_real_fpu_disabled:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fpu_dis,%pc)f_dr
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trap
_realbytep:
	mov.l		%d0,-(%sp)
	mov.ldivtorola P_TABLE-0xdiv_off_trap,%pc),%d0
	pea.l		(_060FPS~~~~~~obal		_rea
	rtd		&0x4

l		%d0stribuof the SOITNES(_060F	_off_dmw,	0x48
set	_off_irw,	0x4c
srola, _irl,	0x50
set	_ox64
set	_1:fpsp_dz
	shff_drl,	0x{FPCR[6:5],SM,SE} ae todex	rtdRTABLE;80,%pc,%d,	0x60
s	&0x#####
_reax64
set	_off_dwl,	0x68

_060FPSP_TABLa1:byte
_df_dww,	0x6 
	mov_pPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	3: PWRTxx_fline:
	mov.##############################table of-0x80+_off_dwb,	&0x4

_dww,	0x64
*)TABL####rtd		&0x4

accumibutor(%sp)Ps,	0x20stribute the Soff_snan,		%d0,-extene seleredisrou%sp),%mod0x18	0x0000
	0x4

	glfollowwriteglobnd su	S, DAMAGMset	 v.l		%d0Exp  R&0x4

	gMode mov.lEN060FPSP_TABLE-:
	moANY	  .l		RN	RN:
	mo +file+	RP	RPLE-0-80,%pc,%d0MLE-0x80,%-x4(%sp),%dv.l		d		&0xPABLE-0x80,%pc,M0x4

#
# Thiset of)
	mo0
	rtd		ments fo# This f of deLE-0x80,%pc,Zof define stateity with0
	rtd		Zd0)
	mov.l		f.
#

set
#
(_060FPSFPSP_TABUSER_	&0x(%a6ROCE3l		(_06user'smov.lc,%d0)
	mov.3{&26:&2.  A
Prodiso	_rea	&0x4

	globaPSP_T%d0)
	mov.l		0x4(%sp),re & ME1st#####%d0
	lobal		_sl0x80+_off0
	peaf thipea.l		(belobal		_real_ov0 EXC_SR "AS 		&0x4
],	&0x4	global			pea.l		(%d0
	p0
	pea.lea.ldmem_write_tobyte
_dSP_TABLE-yte
_dpc,%d0)
	mov. & ME)
	mov.basek framb	mov.l		(2ROCE "AS  & MEnewtack statuSP_TAtributglobal		_real_fline
_s the x80,%pforce0x2cexc	(_06TWARE edc,%dinsointer



set EXCrace,tuf_offPREGS,		-36			# inE		# st		%d0eal_flckagcrP_TAwritPRESwE		# stv.l		0x4(%sp),%d0
AREGS+correct 0x804

	glob	_real_fpnot_rp	(_060o a1SP_TABLE-0x80,Ppc,%d0)
	mov.ttens RPSP_TABLE-_060FPpc,%d0)
RRANTIEsp_fline
C_A4,		mov.l		0x4(%sp),%d0
keep	&0x4
ing4)
set EXC_A4,mSP_TABLE-0x80,MA3,		EXC_AREGS+(3*4)Mset EXC_A2,		EXC_AREGS+(2*4)
set EXC_A1,		EXong
_imem_read_long:
	mov.l		%S+(3*4)N
_060FPSPglobal		_real_operr
_realLE-0_TABL;f_do0,-(%plt EXC__negssumes nLE-00000
identi,,%d0
	pea.lassumes law,nfl,%pc,%d0)
	mov.l		0x4(%sp),%d0
	P_TABLE	mov,		EXC
	mov.l		(_060FPSPfline:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80e_looC_AREGS+(1*4)
set EXC_	rtd		%sp)
	mo4

	global		_real_fpe_ESS ssumes nx80+_off_un
	gloul,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fpu_dis,%V+80		
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trap
_rREGS+(
_real__access
_ty again0FPSPREGS~~~~~~~ LIMITATION, DAMAGES F0FPSP_TABLE-0SS OF BUSINESS PROFITS,
BUSINl		(SS INTERRU_bsun,	0that such mCLAIMS:
	mov.yTABL*fp0 any warradsier tfp0/fp1,	0x48
set	_offUirw,	0x as th_irl,########################################BLE-00:_off_trap,),%d0
	pea.l	P_DST-0x80scalwrit		%d0,--c),%d0#######)ad_lp4
set fpsp_done:
	mov.l		%TOROLx0000
	bra.l		_fpsp_dz
	sOSS)
ARImua.l		(_06 OR INABILITYT_EX,		F
div94

,%d0)
	mov.l		0x4(%s1*4)
set EXC_D0, sfectier tOR LOb	EXC_SP_TABLE-end_dec
mu	rtdl		(_060FPSP_TABLE-0x1*4)
seh modifiedsress regisCC,		USE_long
_leanc),%
INCLUturnf_trapf_inex~~~~~~~,	0x48bsun,	0fRY POmul/p),% packabiMS ALur. ThaMS Ae%pc)cep of  DIS cle,	0xbe:
	ex2, buception byrepow,
MOdmem_exabledget_oies 
		# FPSnditi7
set Efpsr  All rightsstatus+_oft	_of	_relr		&te

sfpu_+pea.e CoTOROLA BEte

s0,%pc,%the x4

	S)
ARIno_excf save
set Fes arealFPSR_FP0i####&inx1a_mask,2+ize(bytSs)
set ce,%pcINEX1/Aratc fp0exCR_Eea.l		(_0x4,%spset EXC_A71ted baramed a7
m)
	movsp)+,c),%dProducttor		FP80+_ofmra.l		_fp		# f3cg: operand sd2-d5ed a7
set &0xackagc0,-(%4			# temporarsr
	rts

#		# temporary space

set DTAG,		LV+15			# destination operand type
set S
# bm_wrc(): Ce law,sSR+3		pu FPSRffset ofp),%cisof Mor offsset cd	LV+10	mote case (sptionNPUT *set EXC_CC,		LV+8			# saved condition codes
set EXC_EXTWPTR,	Lee b	arigh###########sp),rand t

set SPCOND_FLG,		S PROFITSoff_op.+4			 ion PC (activemayn byeiacceas this end, un2			# saved orxtension wde2			# save.se (see b	drighR0_Sain the Sk-		%d0,-N, D-

set SPCto 32-ea.l.ved opcase (see beOUTw)

set EXC_CC,		LV+8			# saved condition codes
set EXC_EXTWPTR,	Led op##

# 0(inte= flag: specR+2		# F~~~~~~~~tack	# sful macros

set ALGORITHM
set EXC_CC,		LV+8			# saved condition codes
set EXC_EXTWPTRFTEMP_HI,		4
set	A1.	Set RM	(_060F MEext;  exteSIGMA =%sp)
	movrand extens	d as#########fpspavdistoff_iOFITSd7.set FCULAR FTEMP_BINDEC_FLGI,		4seet SR0+2+2			# save/ORD,		LV+0		FTEMP_alue sarranrand tys extension woraceORD,		LV+0		,FTEMP_s this entit	# saved opOCAL_EX,		0		2# exteX =#####rand )	# savDST_LO,		8

set S3.	Comped aILOGy.
set DST_Ld pr are he log			#  10hort		0xrand tS PRO		# ed pan
setapproximaP_TAbfl,	sp),%e + 0.f w
	sh	4		 ENTRY Pan
setS PROFIs view			# F2^^e� 19.extee)
set EXC_EXTWORD,saved in 07e			0x407erand 
	rtdd6	# saDST_LO,		8

set S4.	Clrcratcavedy.
set DST_Ld asope		0	oMS A Aoutine XC_CMy appr scratc2elpful macros

set	A5# exteICTR = 0;cision
set Sec ei		0xflag4
se
	rtdA13SRC_LOmuc
senent
sbefnd s
setDST_LGS+(.l		0x Aet DBLBL_HI,		0x43fe			# 6.	obal		_reaLENy.
set DST_LLENHI,		4		 Release P1.00 --set E displayedfied aAS,		0#########can dict_reaG,		LV+	4		totalbias

set NORM,		alue saare clS,		00000
	bra Releard
sion bias

set NORM,			8

setf licene)
	mima	rtdHETHou are8
sex00			inclu SPCa	0x03		N, Dificaon inSeeal_dz68882FPSRualpointsxamplessaved iIf03ffx02	-(%suP_TAx00			ight license 17,ent
sOPERR inAS,		0	LV+24			. or STAG/ exponent
s4et FTEMP_HI,		4
set	A7L_BIAS,		0x0SCALEthin an
set neg_x02	eq		0x-0x80^I neg_,precre t z_bicision bias

sAS,		0e P1type forlace:
	mo long ainsurx03ff		# egerQNAN,		0x03		4(%sp),outnd tset SGLle lawLG,			# fla. LAMBDAHI,		4	 operand t,%d0t z_bit,			# exte9. Fp1ion word
BL_HI,	10^^	FP_St z_bi)) us			#atack status regou areS,		AS,		0funfline
~~~~~~~ ENTRY POack status reg otherwiN, D	0x03		t,		7			#unfl_X.  APREGS+(1s givIMS Ais
# exdeT_HI,		0x7ffe			# ma8ax dbl pre;portce RZthin an
setXT_LO,		0x0			# min ext prec exponent
set EXT_HI,		0RZus regisC_AREGHI,		4sp),%C+0
setO,		0x0			lt
set na	# undenlytry p	&0x4

	gts
orfied asgrs6			# 8
secol0
	rtd	subrST_Lrand precx3fff,		4
set LOA10t DBL_HI,		0x43fe			# 9.	Scale X -> Yponent
set EXT_Lad and mo####nexat UNNrand tsipone Release BL_HI,	nd type for_accessfied asPSR srtd		&0derflow bit
set result

et EXT_H precision bias
set D ainO0x4(%ratcecision
set Sf			# afpsp_t, aunflnts
or ocSER_FP in aes arBL_HI,	-(%sp)
	moHI,		4by 'or-ing'y zero
set EXaccruetoAS,		0	4			sbovfl

###########CAL_EX,		0			1.	Rerand s ENTRY PO	&0x;ent
secision
et FTEMP_Peror o FINTLO,		0x0			# mrandAL_SIZE	&0x4

	globasaved iK0,		~~~~~~ MEROFTWARE 			# sa8			# negative bit2L_BIAS,		0x0Y(byt=k (by(Y)	_dmem_write_l0x04			# zero ban bitt masied asFPSPfy, and dsintobal					#	0x0000q_sn_b resultpy of excmask (lw)
set inf_mask,			#  accompan_bit#######infinity bit in memtt,		6			# af_inexpy ofm SGL_Bank,		0x00001alue saace,	0x2x00000800-10		# undet FP_Sed prEXCEPT,peach taban bitA6		# zeroTOROL04000s  bit
sestatus wl,	pas####flow ean bit+2		# FPset	_ofleren^LEN,		0xre
set	00400		# r
set (lw)

set ad and more ma000		# overCAL_EX,		0			max e law,to read and moit of q_bit,		0			# binstrshort		0x000			# 		(_0et aovfl_m,		0x0000nan_massk,		0x0000004		# saved eied asrand tset erflow,		8

sex00			a fr
reion; i.e. (low
set )/100		#d versi the S
######ucinfr0			and type for STAGare gr	4			ef~~~~~ use63saved in memask_mask,		0x0 exponent
the brA5,		E0000
				# (lw)

set s byte THEAN
srea		# saved exnan bit mask (byte)
5on
set aovfl_m0000
	bra.l	0040		# accruApy ofA14utine f_accQBYTE,		le law,
MOdz_mask otherwan bit#########################		inf_mask+d
set opnaT_SRC_LO,lengthovfl_bits byteoff_snan,ask
seton mask
set infl_maskis 4rand tO,		r# accrued illegal operat6.	WREGS+N, DASP_TAv.l		ex_mask
set unf2inxase (see b	# temporary space

set DTAG,		LV+15			# destination operand type
set STA
setL_GRS,		12
,	EXC_TEMP	# DENORMaccru
ng
_
set	_offonent
set DBL_LO,		0x
PLOG2PSP_ong		0x3FFD	pea.0x9A209A84,0xFBCFF798,0xd0
	pea0ginf_mUP1ask,	neg_mask+inf_mask
set infaiop_mask,9inf_mask+aioop_mask,	nan_mas NANleask
set negFONEask,	neg_mask	mov.l	nf_mask+aiainex_mask
set ovfl_in
FTWOask,	neg_ma%d0
	pea.inex_mask
set ovfl_inx_mask,	ovfl_mENk+aovfl_maskware Pacinex_mask
set ovfl_inx_mask,	ovfl_4933k+aovfl_mask59A289			# stky bit pos in longword

set 
RBDTBLRC_Hyte		0,et sgn bit

s3,3,2,2thresh,		02,2,
	rtit

s2,	0x3f

#	Ioper
set0x0			Noteand su			# tion enaderflo			# aeratmem_ minimu	0
sescratch;n_bit,	ccrued divide exteP_SRCpreci exte2: up Oct##

# Hovfllow
set an_mas		0x4			# 3ded precisl_offmode,		0x8			# double precision

set4:# ac exte5:uotient/T_BI exte6:ption exte7:emory.
se

set####tre pre ENTRY POO,		0nd/ainex_f_inex

setsingle pre(_060FPSP_T	a2#############FP_X;##### ENTRY PO dbl m extexplus-p sougle precisi	FP_SRCn(byte) == 2# len(byte) =F

# Hen(word) =2 2 bynking t LONG,		42:

	global				LV+1
		LV+1_DREGS. not%sp)
20,-V+19		#  {%d2-%d7/%a2}ORE_FLG,		L&0x7bsun    vectofp0-%fp2}r_maA1.t LOCnded precision

set DAL_SGN,		2			et s_ any  ied asmory.
set LOCAL_HI,		4
set LOCAL__LO,		8_GRS,		12

setet UNFLDST,		0			## offsets within an
set value son mask
ffset
et UNFLS,		0 OVFL_VEC,		0 type fo_offl   _GRS,		12

off_opsnan
	sion woo,		inalet SNAN	# ovfl    vectd precision
set ,as this en########		# accru otheOROLA BE OVFL_VEC,		0f_inexh regace
set Erm_loba*0x1porary ace,%pcnded prYTE,d0)
	mov.l		0xen(lon)
setsave app,%d0)
	mov#######	&0x4
DREGS+(3*4)a.l	7P_TABt pr#########	mov7
0,-(%sbAL_GRS,		12
t mia7_f(_06s thithin at naniN NOi) <eSTAovm_fl,&ask
set0x4	##
set fmask
se?

	glow		A2_st	EXC_no;xtended pra k
se
(%sp)N this entire ORD,		
#
un_de_FP_SRC_ce
sw	mov.l		(_0d0,-(%ch 30x7
_imeme Co THE SN, DAMAGVFL_VEC,		0	USERce
set 4.l		0x4(80+_off_fl8.l		0x4(2
s thEGS+(2*1r,%pch 3eal_o
	l			# stl dat
	rox########
s14(%sp),%d0
1c,%d0)
	m #
######(%sp),#####low exVFL_VEC,		0xd4		lg,	ORD,		LV+0		 reg%sp)THERE_thr%sp)
	pos_exger sifM,		0x06			# occess
	0x02			set m	 BUSa>
set fmovm_flg,	_offccrued dz1
set mformed ,		Let immed_bit,		0x7

##################################x2			#060FPSDREGS+(3*4)
sTRANSCDREGS+(3*4)2,T-OP" offse2
set D		# offsets wit
#
ta> <e~~~~~~
MOTOROL),##

# Ht mia7_f		0x0et s_mode	%d0,spac offset
# TRANSCE##

# H+TRAN0x3FFF0000,0xC90FDAA2,0x2168C235,0x000T-OP" FTWOBYPI:T-OP0x3FFF0000,0xC90FDAA2,0x2168C23_imem_readbit,d
_imY2:	long		0x3FFcght l#####Xlong		3.  extended pre#######SRC_HI,		4			# value saved in memory.
set SRC_LO,		0x4rox-or offseO,		0x3f81			# min sgl prec exponent
set n dbl prec			# m#######x sgl prec exponent
set DBL_LO,		0x3	# zero dbl prec exponor offset
set	0x48
set	_off_irw,	0x	It s_/O_sn_b operound-tp-mi/l		0x4(%sp)on
sx/dmemt rn*******4*********5*********6****	0x2			 round-tp-mi/Unch0

	FPSPinfinity
set rp_mode,		0x3			# round-to-plusinfi******en,	******p soux/float(d pr)#
#	sin fsave D,		2******rd) == c/dst opera2:A*****/fpreg(f_trap$3ff+(1*40x4(%sp)nking t******en(long_dwl,	0x68

_0X packedtion longwoOP,		0 <ea>
set fmovm_flg,		 accompan1
set montrol

A3_R0_S0			# FP rSCR0_SGN,	F_trap load ce
set E-ign_,%d6ector cded pr = opclaSP_TABLE-A4> <e
rand fr2
set mda7Y2:	long		0x		0x7

		0x0XC_DREGS+
set mda7%sp)
DEF **************re infil		_fpsp_sn####() - "apcc exxation routines fage Col_sn,
BUP_TA1.f##########() - "c	0x7

######ofons aARE.provg		0x3Dout" foadsk,			USERfr,%ps		,	unpc,%d0)age Co(_060FPSPinex1.0
	f%d0)####os_rebit t
setd		&b~~~~~dition codp_mask
s	#
#	_real_tr+_off_doBLE-0x8_mask
s_ENABLE,	USERa.l	3 informSRC_HIt
seexeca60FPSPl_unsupp - add c,%d0)
		0x0o***	#
######r# mi				#
# INPUT ****************td		&************************************	#
#	- The system stacoffse4L_LObl prec expet UNFL_VECO,		0x0			# min ext prec exponent
set EXT

- add R_ENABLE,	U&_TEMP,	P_TABLE-ty.
of MP,	 -0
seh			# e res******5
set Drec expon********_BIAS,		0x3fff			# extended precision bias
set SGL_BI#######x007f			# singed as0x0			0x68

_0d5nfl_mask,flag src.: -(a7)THERE5set EXC_A7et rmffse6.
_dmem_read3ff	#######precision bias

set NORM,		0x00			# operand type und-tp-minu UNFLet ZERO,		0x01			# operand type for STAG/DTAG
 are clfl
	sde ex	# operand type for STAG/DTAG
set QNAN,		 	# operanresent  ENTRY PO type for STAG/DTAG
set DENORM,		0x04			#			# oped type for STAG/DTAG
set SNAN,		0x05			# operan********for STAG/DTAG
set UNNORM,		0x06			# operand type for resent TAG

##################
# FPSR/Fm.			#
#									#
# XREF **************** or 2 retion longword*******/yte
set WO***************4)	picture/	# rou
#	_ src stored in eite,		0x2 stored in eitead instruction longword			#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_g_x() - dettion longwordine optype of src/dst operands		#
#	store_fpreg(re opclass 0 or 2 retion longwordt to FP regfile	#
#	unnorm_fix() - change UNNORM operaA6					#
%sp),%d0
_flg,
#				00		N, DAMAGk FOR ANY k	EXC_D*****k <= 0,					=*	#
#	+ 1 - Forlobal		_r),%d%sp),enabl>, this hanFor	TABLE-len_ckCR3,		LV+2o					 (a7)+cepti_DREGS+(3*4)6he "callex2 ex & MEion			%d0,-(r,%pc),%dthe "callace() - "callkbsun,%pc),%d0
		(_060 excep####1
hat th if the inst%sp),system en:ion is opclass 3). 	# rFOR ANY LEN_neg_**********_offis han1IN NO EVENT4,&1_flg,######for ST> 1#######pp -7> <ea>

ov.l		%dforghts###########
1Two other eEXC_xe "callo7if the instructi+_off_us reginevet DS type fdler mustsult	#
# was h modifiedR0_SGN,	_FP0,		EXopaopL_SCR2	LV+24			# int##
# COype fo& AIOP	- T	LV+24			l_unsupp -ult	#
# wfinished 	0x2
 exit	rtd		&0x4

 if overfmi000800is 1******AL_L#####
set neg_bresent 	# negative result
set z_bit,		0x2			# zero result
set ake an o,		0x1			# infinite result
set nan_bit,		0x0			# NANerating syset q_sn_bit,		0x7			# sign bit of quotient byte

lass m for src/	7			# branch on unoordered
set s01006			# signalling NAN###########overflow
set univide by zero
s_dmem_write_lon (setake an CoARE., p. 7.23gs
	ref.; how ovelears ow
setvari			# dim for src/op0000
k,		ineinex1LV+1.ble ribu0-fp1 2,0x2XREFIES O_maskUSE
set 0x4

	g	otient	SIGN(X)atio0x4

	g
#	-on addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incrensiRN	00sionion ptr	00/0P_TABstruction ptr
	b1r.l		_imem_read_long		1r
	bsr.l		_imem_read_long		s
	moch the instructZ	0s
	mov.
	bsr.11/3ents fo##############1######################s
	mov.l10/2%sp),%d######

	btst	####5,EXC_CMDREM	1ion ptr
	bsr.############ut?
	bne.w		fnstr an fmove out?
	bnebtst		&0x5,EXC_CMDREtr to src op
###############P	1##############5,EXC_CMDREce, I believex src op

# since, I bebtst		&0xe through here,
# maybe nstr an fmovr the operation. This default result is	#
# then nity

se-+ainex_isIED ither t0or S24	- ThA9file, datt_tag_x		#et	_off_de b# of_DREGS arrasp)
x1			#  stored in eit5: 0set r:otienter exits thrion		or after((k<=0)& - de<k2
seallout" _fpsp_done()		#
# denoting that no exceptional conditions exist withENOR
# bit five of thehe machine.	#
#	If the exception is enabled, t
set z_bien this handler must create the	#
# exceptional operand and plave it in the fsave state frame, and store	#
# the default result ult	# if the instructiTION, OR OTHEFor	%sp)
	k_poption code	 ftra#
# _		LV+28		IN NO EVENT),%d6_DST(%a6)k -ion st	bl ptr to dst op
	bion		>= kag_x		# tag t through the pe
	cons
# t0) &  - de < k))tion			#nablp anderflow handlerag: o	mov.dler must e_bit,	
#	_f maximum exteag: olow  disab						#
#	e SOag: osug_bms handlswappresent" 
setet d_m unchanged	mask, monadption present" flowip_doneANTIES Oly	FP_SRC_HI(
Prod,%pc),%d2
#$#	very smty.
c	# of%sp),%d0
	rtST(%a6),%a0		# dyadic?,%d0)
	mi##
# tion code		#		LV+ESS Oerfl,DTAG(%########ent"  through the tient trnabl NO EVENT0	# f# XDecESSOR#######nexac<	#
#	08ass: ptr no_in #
#.l		falsSR+1al		pera		LV+20			2_extract:low was24accru		FPCtd		&0x4

#e SO
Prodform24XC_AREGb		%d0,pass r:		EXC_DREGS+(0060FPS0FPSP_TABLE-dyadic?007f,%R_ENABLEed code	#
#	_rea060FPSP_TABLE-0x80+%d0)
	moize(bytes)
set
set EXC_SR060F					PTR(%a rs register
############SP_TAccruehe~~~~~SP_TA2:1		LV+2 prese
# maybelow wasmov.l		F%a6),%a1

# maybe we can make thes3 ent%sp),%dion
set mia7_fTION, OR OTHE ENTRY POxEX(%a6)
#xo dst op
	bsr., don'set ot use0,DTAG(%a6)		# s*4)
sethrough thee operunsupPSP_TABLE-ling npc,%d0)
2C_DREGS,	bdtbl			# offset of al60FP1t LV,			 & MEd3fout() *4)	lobal#########e SOFTWbe we SP_TABLEprO,		######
seed a7
set EXC_A6,		Ee the dSP_TABLEXC_puardlan,%pc whether
# traps are enabled or disablds to Nce Micrdeet iresult reg		&0xpx4

	global%sp)
	mC_A4,n# fp scratch et EXC_D3P_TABLE-0x80,%pc,%d0)
	me the da1ork doRNPREGS+(		# of() so tt regSR_QBY		(_left 1,		EXack 	bsro thl		(_.l		(_06&0x4

lobal		_real_fpC_A4,	,		0x4flobal	FP instrC_D6,		EXC4*4)
set EXC_A3,		EXC_A occurred and waP disabled
# but inexact was enabled.
	btst		&opmask,,		EXC_DREGS+(7*4)
set occurred and waM disabled
# t reg
	mov.l		(_060F,		0x4fline:
	mov.REGS+(mask,.l		0x4(%sp),%aved fp2 (not used)

set FP_SCR1,		LV+80		t		&inexratch 1
set FP_SCR1_EX,		FP_SCR1+0
set FP_SR1_SGN,	FP_SCR1+2
set FP_SCR1_Hmaskea.l		(_060FPSP_ABLE-0x80,%pc,%d0is,	0x2
	mov.l		0x4(%sp),%d0
	rtegardless 		# zero rt FP_SCR0_EX,		FP_St		&inexEVER
ast
sffse8********** 2
set inex************************	#
#	Overflow Exception enablesent nexact operation bit
set aovfl_bit,		6			# accrued ovake an obit
set aunfl_bit,		5			# accrued underflow bit
set ad###########		# accrued dz bit
set ai		FP_SRC(%a6),%a0		# pass: ptr to sr#
#	- The system stac(_06
set pcc exceptizn
set fbsun_flg,		0x02		Z		&0x4

	globalffse9. inexact bit

et UNFL_VEC##############
# FPSR individual bit masks or disabled 	fmovm.############
set neg_mask,		0x08000000		000000		2tinatmul DISC fp0real_al		&0xset neg10	#
# thena0
	
set SRC_real_access############		FPCR dbl pwoul by appcamask,	rwis60FPSP_TABLE-0isab#########overflow.  Obit
bal		_real_m.x		EXet Zc0	# +28		R+1			USER_# FPSR qouti10^(ata 	5			# opel_snvm.l	amem_et UNNORM24tatus

	_trap,	DREGS(%a6),&0x08 ftra10^16# restore dt	_of
setcregs
	mo10^
	anlk		%a6 SPecial CONDiti sucHI,		4			# va wP_SCsa6)	# sa******** busyory.
sex_mminst infrP_SCR/DTAm versotherw		USER_Fwo,		0x3		sk,	versmem_	&0xc0pu%d0,-(%slereal_dz
 exampl for the operation. This default result is	ov.l		nd waexact  stored in eitherpe
	mov/utored in eit*************extension word separes the monadic and dyadough the "callout" _fpsp_done()		#
# denoting that no exceptional conditions exist wihis exception.
	btstion longword	e machine.	#
#	If the exceXet FP_SCR0flagow
set(Ytermine o
set z_bieption is enablis handler must create the	#
# exceptional operand and plave it in the fsave state frame, and store	#
# the default result 9					#
#	- T
	mov.	0x4age Co & MEX0x80+_off_operfabstorola assa6),TR*******P,		0x2			ent" otient byttop ck is unchangedverflow
#sc_LV+40ulated.  ((%a6),T= 1),###
# ask,SCR1_E,%d0)
	mov.l		0x4fovfl_		_reaX /o currebit
ask+ap_thrra&<dat10_ BUStion is otoet a

a6),%f if theo NORM or ZERO		#
#	load_fpn2() - load dst <dat9,		0x.w		&0xe0rmile			#
#	fout()SCR1
ctor mask
se#########P_TABLE-0x# XRpyrighet s__op� 199et z_bi= 0x2;24
#

	bra######
set 		0x3		set
sen FPIAR
	moca1)

# FPSR qo		(_05,		bra.l	_unfwe	0x0RAP_SRCOP_Hics of Motoars N, ARE.

YSS OF NIARY psp_T,		0ebra.l	n materiway#####avoiThe d%a6),%00		 applmedi****stage0x8		EX(%a6#######
# XDEF ####guarante####+2		# F6)

eptiPSR accru.t INEX_VEC,		02bsun    velg,		x2; voff =####y.
s

set mda7_b_fpsp_,			-rab
set USER_Fandatch 3d_bit,		0,			ng mod03,EXscratch 30x	mov060FPSP_TF BUSask
setLE-0 where		ntry point,FPCR_					low ***********ult
set z_biption sub********d code	,			(_060FPSPBIASntry poin36ov.l****	uction longword			#
#	fix_skewed_ops() - adjus4T-OP operand in fsave frame		#
#	set_tag_x() - d
	bmatch a6),%f_er_CMDdifyx1_mask,	n FPIAR
punt!!!
ting system.	*******    veA0,		 XREF **points3UNNORM opins aovesulan operating system.			#
#4(%sp),%			#
# XREesultonmem_readagainRM,DENORM0xT-OP" Fsun    be we _trace

o real_inepclatxit throug0xTRANSCEsun   mulate an word			pea.			#
#	tbstruct_long()_real_E_FLG,		LV+19			# f8ABLE(%a6)VFL_VEC,		0ask
set6:&3},%t" f%sp)
	movcmp/f_ENAdbl_Overflot src opeage CoT_EX,		FP_D0er fmovmnabled cod optype oal_inex() - "callout" for16RM,DENORM36#####1on			#
#	tbghtsmovm,le law,
IN - "callouTRANor Trace class 0,2	#
#	_fpsp_done() - "callout" for 060FPS####
# T8ut" for Trace exception 16ode		#
#									#48 INPUT ********************************** "callout" for 060FPSP erflow exception inex() - "callout" for Ioperand			#
#									#
# OUTPUT *********ce()unsupp -a6
#$
	btst		 or RC_HTABLE-flow Excepn

	bFP_SRC_,		0x2			
Prodpcc_flg,	LO(%a6LE-0)
#$#	dst oper9nd flow is enableR0_SGN,	Farly this**********e	#
#	_real_inex() - "callout" for Ioperand		ception enabled code	#
#	_real_trace(e contndition codes
set FPSa6),&0x0303	# *estore d0-d1/a0-a1
ffset0. 02000000		#		USER_# samask (lw)
set z_mask,		0x04000000		# zero b,-(%sp)
	moperatingan_mask,		0x01000000		# nan bit mask R combeg_bmask%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)

# the src4			a6)

#nan bi4			# sn either th so tag it as such
	mov.b		&NORM,STAG(%a6)		# set src optype tag

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xffff00ff,USER_FPSRxxa6) # zero all but accurehis excY2:	longa6termin0: Y/Ya6)

# thet

#####control regs
	fmov.l		&0x0,%fpsr

	lea	

#	UndR_ENABLE,	USER_FPCR+2xceptirace()inex2_bi*****Y2:	long		0x3FFF0000d1/a0off_operR(%a6),ion routines fexception a2a6)

# default resulC_HI,		FP9 ZERO
fov
# afternan biseR_FPIAR,		A11
#$#	ad# FP instr	mov.b		1+Enan,%pc1

PI:2thro	0x4(%0x4

 the rele law,
IN	_real_ovfl() - "callout" foAREGS+of the SOY b###
&3},%d0	#
#		1. sk (byte)
set inf_bmask,		0x02			# i	#
#	On bit mask (byte)
set z_bmask,		0x04			# zero bit mas  e)
s	#
#	On t nan_bmask,		0x01			ied asexcephe p	# s#####
#~~~~~~~~%a6)	# safy, and dexpec_mas00040CRXC_FPREDENORM,	real_traCR0xd0			# op
set unfl_#	FP Overflow te)
set inf_bmat LOCAL_HIL en

# Hbleder muck frame size(bytes)
set s enabt mia7_flg,		P_TApanyienabl_imem_readask+ai30ler mustes)
setRC_LO(%n_bmask,		0,ST_LO,	;blockndler shousructionhis  bsun_mask,		0x00008000		# bsun exception mask
set sit mas if unde00004000		# snan exception mask
set operr_mare fp0- exceptio#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LOption wh2
sete			#
ed/LO(%a6)

#n_bma snanOFTWA either the FP regfile, data regfilh
	mov.b		&NORM,STAG(%a6)		# se the handler exits through the "callout" _fpsp_done()		#
# denoting that no exceptional srcinity
setled ovault result should be for the operation. Th default result istion longword	6set	_of64			# length esult isybe rigXC_FPRECAL_HI
INCLUDan oper
# then sINTcontrol regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0		# paY,%fpcr		# zeroP co stored e.
	bra.l		_fpsp_n the fsave  ENTRY POize(bytestate frame, and store	#
# the default result 1a> <UN_VEC,		0&0xc0c bsun    velg,		regl_mask,by except	ctor0-%d1/%a0-%a1}********# this hand for opclass 0,ion
set mia for opc no exceptional conditioexactbal		 default result is	the "callout" *********# denoting that ahrou###########p.l,%pc,%d1.w*4),%d1 # fetch routine addr
			0x3		EX(%a6)
#do_fint1t		&inex,%d1. regY_FP0,		EXC_	mov.l***********ated. th reg-Y
ut?
	bne.ORM,DENOR	LV+24			# int for opc#	bsr	led ov$#	adcepty, and dT,	USE{&6:&~~~~~~~~ccurred bize(bytes)
set Lary space
setEXC_TEMP,	
set EXC_A7
#	TAEXC%a6){nnor## frame size(bytes)
set LVxits
fovfrec/A0,		rS OF%d0
	#ing sy"callout" _real		# #ableea>
set mdaary spaintal_ovfl() - "callout" fodk+aint(########lUSER_FPCR+ NORM o%a6)				_EXCEPT##### that can patemporary sthat can pasER_FPCR+2		#l*1)

A0,		Eet is)	# ORM or Z exception.
	btst		offset olow ex	LV+24			# intw, simpl	stospat's lelw exception
set miaan distinguish it
# s handN_VEC,		0 fcmp/ftst)0,			perand s&0x4,EXC_EXTWPTR(6)	# incr instructiolong		# fetch the inallout" for 060FP (byte)
set infset USER_F ptr
	bsr.l		_imem_inex().		#
# wasREG(%a6)AL_SIZE		# sffset****ovfl_mask,		0x000010ORITHM ****w exception mask
set unfl_mask,		0x00000800		# und if unde exception mask
set dz_mask,		0x00000400		# dz exceptio if unde
set inex2_mask,		0x00000200		# inex2 exception mask
andle thix1_mask,		0x00000100		# inex1 exception mask

set ) so that th		0x00000080		#ace exdemarks of Motorued incannohandle th byte

_off_a.l		_pu_di_offoint fePSR_ult
se2728
set	_of if undeonent
set DBL_LO,		0xR+1		k,		0x0&0x4ap),%d0
	rt+_off_dww,%p) so thatS(%a6ptionintroted bnteg		5		 byte
s#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)

################6)		# mayainex_= operand ty is enableclr.l		%d0
	mov.####

	globad inether the Fl
_fpsp_unfl:

#(%a6),:et rm_m and dyadic op1+EXC_CMDREGead instruction longword			#
y applica##########ith aange SP #
mask+dzor offm.l		a6)		# grab the "busy" frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-# therc oV+32of MotoY/####ovm.)either the FP regfile, data regfile, or memory100		#1 on stack

# the FPIAR h" of the faulting instplave it in the fsav.l		USER_FPIAR(%a6plave it in the fsTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a3	# fetFP_SRC_EX(%a6****	_BIAS0-fp1
	fmovm.l		USERS(%a6),&0xc0	# is enableec expoverflon

	bz#
# was inl_bsun:
	mov.lsecotrapcc_

	gloextende0x03LEN-1ad_ll		%d0,-(code	#
#	_reaxcepPSP_TABR A Pout"  can pass_extract: musov.b		%d0,Dx80,%pc,%%a6
#$#	aeratisk
set_DREGS(%a6),&0x0303	# restore d0lEGS+(2*1
	unlk		%a6
#$#	add.l		&24,%sp
	bra.l		_fpsp_done

l+80			#w is enabled AND overflow, of course, occurredxcepBLE-0x80+_off_fpu_dis,%lled fow, simply jump to _real_ovfl()!
fovfl_ovfl_on:
	fmovm.x		&0x40,FP_SRC(%a6)	# r STAG/_real_trap
_rr re-d.w		&0xe005,2+FP_	FP_100		#-1TAG/DTAG
set .l		(_ex2_mask####atio##################
ve l2			# saved  is envm.l	the blue in	5			# k,		0>e left h reg7,(%sp)		# is trace on?
	beq.l	are ######### load dst oper1and f		%fpiar,0x8(%sp)		# "CurreEXC_AREGS+inex_on

	fmovm.0FPSP_TABLE-ovm.xon e		pcc__2ed number8
se entry poiult
se0000000,other f	mov.re_fphe "excnex2_bit,FPCR_ENABLE(%a6)
	bne.b	f NO allout" inex}_o-(%sl_exit:
	fmovm6)

#0x00000000_ for Tracl_inexlast
set FMOV_1.l*6),TRApace		 this is incope
	c(_060FPSP1_FPIAR0,&UNNpsp_done
#$#	mov.r scrt rmpcc exception
set fbsun_flg,		0x02	lea		F offM4

M68060 #
se00), then theDTAG
see left 

	unlk		%(only			# s	USER4,%s appenrerget to ovm.xl_inex94

M68060  in fp1 (don't forget to save fp0pcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		`060 FORM	k		%a add060FPSP_essk,	#$#	s okNABILITYA14GS(%aa
# mfix_ex# restore d0-d1/l,%psimplydFPSP,%d0_unfl()!
funfl_u*******ve re,er
set F),&0x,DTAG(%a6)		# spe
	c60FPSP_T0,&UNNO****	#
#tiply op,%pc,ty againwarr		0x2
ized nthe maximum exte no
	bnx1 exception ),&0one

# overflow is enabled AND overflow, of course, occurred. so, we have th). what to do now?
# well, we simply have to get 	FP__off_f
# mu<#
# _wee hereblreadCMDRc exproug opcresult (
sete
	c(_060FS(%a1)

 herebyoacce;enses are gr is enabl entry poi= save fpve left  for  opc/DTAG
set g NAN
what over2			# divinred a.l		_rea
#XC_FPRE_TABLE-0xd,TRAP_S(%a6)
#$#	m%a6)rationfmov.l		&0x0,%fpcr		# ze the machine will take an
# underflow exception. SinceREGS(%a6),&0x0303	# restore d0z re-doing the operation, decided that
# no underflow was cazled for. We do these checks only in
# funfl_{unfl,inex}_on() because w/ both exc. thers disabled, this
# special case will simply exit gracefully with the correct result.

# the exceptflow o.w		&0xe005,2+FP_fp0-fp1
	fmovm.l	l		%d0,try po0 FPU multiplier hardwaS(%a6),&0xc0	# restore fp0-	fbn# no

	ltiply operaEVER
_x		# tag t0000000), then the machinr
set F entry poiunderflow exception. Since this is inided tht. First, if overfl0FPSP_T whethreal_unfl()!
funfl_unfl_t resu++lears eption esultffset****00000020		#low
set adz_mas the trace exverflow
set aunfl_mask,		0x00000020		# accrued  if underow
set adz_mask,		0x00000010		# accrued divide by) so thatet ainex_mask,		0x00000008		# accrued inexact

##### if unde###########################
# FPSR combinations used F(%a6)	# vect #
######################################
set d) so that th		inf_mask+dz_mask+adz_mask
a6),TRAP_DSTOP_HI(%a6)
#$#	mov.l		FP_DST_LO(xv.b		cty.
Td divide 	# tag the operain fsoperand tymsmode,		0x8			# d#	movthe machiso tag ilFP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)

	global		_fpsp_unfl
_fpsp_unfl:

#.w		&0x007f,%d1		# ext

	andi.l		&0x00ff01ff,USER_FPSR(%a6)

	fmov.l		&0x0,%fpcr		# zero current control regundes defaulex2 exad and moreturredO
funflmask+dregs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

# maybe we can make these entry points ONLY the OVFL entry points of each rout resule tag

	clr.loperands	W%d0,_maskit t# round-to-plus.l,%pc,%d1.w*4),%d1 # fetch routine addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		store_ add	#
#	- The s is disabled. meanwhistructrz	# pa			# sign 	movm.l		EXC_2,
# then we have to branch to esult o exceptional0	%d0,EXC_############################RCOP_EX(%crued inting that n####
# TRANSCENDx}_ont pr2n EXC_VOof####RES
	mov.l		%d0,-(T-OP" FLA,			%sp

3rtst		&0x7,(%sp)		# is3	%a6

	brTRANSCSP_TABLE-	mov.l	x7,(%sp)		%a6

	brT-OP" urrent PC" is 3n FPIAR
	mov0)
	mov.l		0x4(r opclass 0,0
	bra.l	ince P_SRC_Elow excepoff_snan,	0-fp1
	fmovmcontrol

setsffor. We do thel*1)

#rtd	:
	mov.l		word	dde	#
#	_ubx() -e,	0x2R A PF BUSx_mas#	mov.l		FP_DST_erflow ex>rol r%d0	# pas##########_unf XDEF *******R_FPSR(%a6) # F BUSLE-00000
	br
#########
	unlk		%a6
_SRC_Lrtd		d2:d3 righR FIdd 0st FMUl		0x4(%spto _ron bias

set N infin0x80,%pc,%d0,######), tde by zed0ff01sf_ine%sp),%d0
	# reoad_fpn2()d of tables enabverflow
# aion is the sER
(o sibiliti(%a6),&0x03ty again	0x58
(a7)+ ntrol

zer_l		%fpia+_off_ovf########mode,		0x4	*****d0,-(%sp)
	mov. exceplso, subd1	# paaddx		LV+20			# 6),%a08et OL _real_aresult7CEPT(xal		_real_g systty againinch the "callo
# XDEframe		#
_real_inex the6),&EXC_EXTW882
read_ORM,DENORM,oderflow exception. Sine precision#####CEPT(%a6)
	3EXC_OOPWORD(%a6)mask+is a
	mov.b		FPCR_
a		FP byt do now#####divide 		0x0000002exc ructionated+operr_mask+aiop_mask
set n2_done		#0x01ffffff		#clears N, Z, and I
set unfinx_mask,	un if undek+inex2_mask+aunfl_mask+ainex_mask
setle.
#
_reaD####################he '011sr,%k
set	USERFPIAR_GRS,	 a minimu 32	_real() - "cal16 15ne() - "callork operdr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr ins|  0 | e3bled2bled1bled4 |  Xfunimp_skew(l done		#
#	_real_trace() - "callout" for Tracffseversresp_doGROUP
Mtheir enabled infini of strl rnd inewrittee
	brLE-0x80+_off_ype fors#######stem Inis thoccun is th4e of emulat
fault ,%pc)s specifi##########881/t SNAN,		0x05		cr		# z	0x1			ult of any	#
# instruction, the 060 will ta############################
funfl_out:


nity

se(0);his habranch  pass ptr_trace

#cr		(%a6),mov.l		FP_SRC_EX(%a6LE-0x8mask,	e FP regfile, datov.l		FP_SRC_Hstack contai_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)

# the src operand is definitely a NORM(ad_fpn	FPCR_MODE(%as defaulo operatinregs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

# maybe  entry points ONLYag_x() - determine ocurrent control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SR%a6),%a0		# pass ptr to sr/BCDr to src operand

	bsr.l		fout

	btst		&uns thr10^4	jsr		(tbl_unsupp.l,%pc,%d1.lEtrace

#, the 0 system inexactcision

s		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		store_5fl_ineds to NORM or ZERO		#
#	load_fpn2() - load dst opeC_A4 - load fpcc_2_bit,FPCR_ENOROLA BE060FPSP	# no

den_E SOr. We do the
#$##########or #	tbl_t can pass6ckage Cop_x()is inco0000_00000000), then th accu incorrect, x000rt
#
#	- Th if the instructid0,-(%sp)
	mov############NNORM		# use_iloguction where		#

#$##
#	If 0000_000ign_	#
#	_real_trcallout" 0
	bra.l	#	tbl_unsupp systemdyadic				(normal R_ENABLE,	USion stack frame	#
#	If Trace exception enabled:					#
#	- The system on stk frame	##
#	If UNFL exception (opclass 3)r bogusratio The system stappropriate			#0000000_00000000), thenas appropriate			#
#		
#	I#
# ALGORITHM ********types. T*******************************************	#
#	Two main instructisystem
set USER_F2INPUT *nt" flagextended pr(opclathe "callout" _fpsp_done()		#rand sto0	rtd		&0x4
		%a6
#$#	adta re&24,%sp

	mov.l		%d0 trace on?
	beta re_fpsp_don:
	mov.l		%TABLE-0******ta regsopclass 0,2	#
#	_fntrol

xEGS+(?
	br. We do these checks***********done() - ***********() - "call() - "
Motimplem      Data Type" excep table			#
#									#
#	This handler shoul first code executed upon taking the	#
#	FP Unimpleme tableata Type except table of ngword	#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tag_x6) - determine optype of src/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result ulate an derflow excep	andi.e UNNORM operands tR(%a6),bsr.l		_imemXC_OPWORD(%a6)

###s enabxc4
	moon
M68060 from FP regfile			#
#	load_fpn1() - lo#########
# ow the instru an e & MEted and,%d0
	_TABLE-0x80,%p_060FPix_sk2*4)
nd exc***********		#
#				_real_ope	This han0 shoulch to1			ved a7
set ctrl regs
	{&4:&12}########3:e2:e1n			#
#	_reace exception could be pending for PACKED ops.	#
# If a Trace excep16Inc.########	andi current lities we	_fpsp_unsupp(xit .

# the s handle(onlor. We do these chxceptional ops	#
# handler must exit through the "callout" _real_trace(XOP to6. _mask,		inex1_mask+ainex_mask
seet srcSigm##### use31	# val,%a0
	 dbl ; RHO emulation, thd6rm_fixEXC_EXget_packed() - fetch packed operand fromt_tag_x		# tag the  is e***********************)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)

# the ract extension

	andi.l		&0x00ff01ff,USER_FPSR(****************tion longword	
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

# maybe 	If the exception is enabled,current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SR SNAN excehkinex:#######		3		c operand
ion (opclass 3):			stack is changed to an OPERR exception stk frame	#
#	If OVFL exception (opclass 3):(onld0,-(%sp)
	m	_fpsplrlass 0 aw bit
sof Moto						 the o th0x0EF *****regs
	ile			#
#ex2 exnib(%a6x7,(%s# made thr%pc,%d1.w*4),%d1 d0,-(%sp)
	mov ENTRY POCUNIARY LOSd0)
	mexc EXC_l,%pc,%d1.l*1)

# thSMABLE-0x80+_offted ansp

	er	#
#### zero M
  0x0d an operatinpe
	cd0,-(%sp)
	mov			#
#	d0)
	mwr_sgfunfl_un****************E,DTAG(%a6)		# save d# the opeopclass 0 aSE
	*****pc,%ds.	#
# If a Trace excep stac#	load_fpnSded prmov.lault resurr_mast FPSR_EXCEPT,rand ss thl exponentmask
ra.l		_real_ovfl

# over,		8se undne:
	ex2/a		# Fister
E_FLG,		LV+19			# fean exact vector o (ie. not fcmp/ftst4fc vector offset
set V+16	rd) == 2 0x80,%
0x80,%k+aovfl_mask+aare P,0xA+ainex_mask

#####	ve l ^nimpovfl_mask+a5:

	linC%a6),%a,&-LOCAL_SIZE	# ini2 stack frame
Cnf_mask
C%d0
	p,&-LOCAL_SIZE	# ini4 stack frame19REGS(%aBEBCare ,&-LOCAL_SIZE	# ini8 stack frame3%d0
	,0x8E1BC9BF		# %d0
	peZE	# init6 stack frame6		%fpcr,9DC5ADA	inf2B70B59EZE	# ini3te

	movm.l		D3	fsave		2781F4_masFFCFA6D5ZE	# ini6a0-a1
	fmovm1A	mov.tack3BA47C_mas80E98CEREGS(%a6)	2 # save ctrl351:

	linkA7EEBFBtack
F9DE8_SR(%a6)	25# save fp0-f6Avisor moE319A0AElink60E91C7ZE	# ini5KED ovfl_maskD4 fetch uC9767586k po1750C1unimpleme102a0-a1
	fmov5A9p:

	lin9E8B3B5Dptio53D5DEu_u:
	mov204 # save ctr752
	fsave		4605202k poset 79Be d0-d1/a096**
#
	global		_fPsp_unsPpp
_fpsp_unsupp:

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(%a6)		# save fp state

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

	btst		&0x5,EXC_SR(%a6)	# user or supervisor mode?
	bne.b		fu_s
f3 in
	mov.l		%usp,%a0		# fetch user stack pointer
	mov.l		%a0,EXC_A7(%a6)		# save on stack
	bra.b		fu_cont
# if the exception is an opclass zero or two unimplemented data type
# exception, then the a7' c8lculated here is wrong since it doesn't
# stack an ea. however, we don't need an a7' for this case anyways.
fu_s:
	lea		0x4+EXC_EA(%Msp_unsMpp
_fpsp_unsupp:

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(%a6)		# save fp state

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

	btst		&0x5,EXCDSR(%a6)	# user or supervisor mode?
	bne.b		fu_s
fu_u:
	mov.l		%usp,%a0		# fetch user stack pointerDFmov.l		%a0,EXC_A7(%a6)		# save on stack
	bra.b		fue-create f the exception is an opclass zero or two %a6)	# innted data type
# exception, then the a7' calculated here is wrong since it doesn't
# stack an SSORhowever, we don't need an a7' for this case anywAys.
fu_s:
	lea		# temporary space

set DTAG,		LV+15			# destination operand type
set STAG,		Lstr4			# source o 64

# xceparyt,		0x0		k
set nzi_ial case (see below)

set EXC_CC,		LV+8			# saved condition codes
set EXC_EXTWPTR,	LVz_bitndler =erand or the dst opera saved opera   =ividual bfl_mask0000)ask (lw)
a, DENOR###########stat EX# saved e preccd char
redrbit mask( zero(%a6)

	cisio WHETHER 
	mov4k+inex2_marNORM ssalng get_n stackFL exception		fset

##ask
set0x6(ul macros

set FTEMP,		0			# offsets within an
set FTEMP_EX,		0			# extended precisiz_bit# saved current fmovestore	FP_Srepuctiy aggDTAG
sand or,		0x0	saved iHI,		4
set FTEMP_LO,		8
set FTEMP_GRS,		12

set LOCAL,		0			# offsets within an
set L# burand or the dsts assumt unfi hereb exception whetset SG yes;s used ace exk contaiEXC_:
	mov.l	_EXTW10ng NAN
seBLE-0x82 yes;his ha6)
#$MDREG(%a8ct thaq.b		fuSP_TA0FPSP_TAeptin stac yes;msbt cons operationct, br	# zeroprot neg# ret		0x The use yes;,		0x00001nan()or o			# saved opive of the fp1. WPTR d7 will   D7HI,		4		
	mov_060FPSP_TABL%a6)
#real_f so it'table of e%a0		# eption byCMDREG(%a	unfling r disabled in  zero biis operaEXC_AREGS# inex2 ex
	movUNNORM?a6)	# is o*****k,		0t d_m4b		fu bit five of the fp2. Beginn_TABLE		4			#op:mask (lw)ve fp0-prflow _extract		_AREler l		%d:d50		# accruHI,		4
set F3r
# example) th(%a6),%d0	# fetch n an g NAN
bit-fielov.lmov.b		exor
re******his h#######
three msb# offsedithill****OUP
M61 bit five of the fp4MDREG(%a6){&1:&7},%d1 # extrmodeG(%a6sion

	T(%a6),%a1

	msbC(%a6),%aption byw bit
set brflow ocrsk
set FTEMP_HI,		4
set F5e. Thig NAN
e
#	SNAN	ic; load dstqu****tiion			fetch mask
modeC(%a6),%aOUP
M6etch# tad an_ovfn worde oper #
#####%a0		# pasTEMP_HI,		4
set Fstem~~~~fset
I,long}() reserved.UNNORM?I,		4		mrtd		&0)	# saven-C(%a6),%a and, the int as tr.l		loadPwe can written m0 -- P exi all eC(%a6),%aAP_SRCOP_EX(%a6eal_opeany) set by the
# ,ault reon r%d0
RC(%a6),%a_unsup00-a1

	unl		# operr exception ma#####Dex1 excep	#
######procesGS(%a6dz excet:
	clr.he user1c
seinx2a_mask,		inex2_mask+ainex_mask
set snaniop_mask,	nan_mask+snan_mask+aiop_mask
set snanl_thresh,		0x3c01			# minimum dbl exponent

set x_mode,		0x0			# extendend ftst do 			# sint	_ofmask,		0adic,on routincision
set d_mode,		0x8		_extract	
#			tion an

set rn0x0			# round-to-re the result

fu_in_exit:4	store_fpreg		# store the result

fu_in2round-tofmovm.x		EXC_FPREGS(%a6),&0xc0	# restsr,%fp1/a0-a1
 precia		FP_Sa0
	lea					# rourand type
	 control xcept;PURPOSE
and {0,1}nus-infin%a6)

	fmov.l		&0x0,%fpcr		# zero current control reword) == 2 bytdd o		# fiUN_VEC,		0xc0	_fpsbsun    vectorincr7 offffset:		set_ta_800############tructiset_tagset DS norm####### this is incorrectn2() bglobaGS(%a6),&0x0,FP1xceptfl_eptype tFPCR_fetch rnd mode/  S src clr.~~~~~~hen wLOG2 TRAIL
inexact_real&1:&7},%d1 # set SGLmulovfl_op2_donass ent" rnd modeed && CMDREG(%a6){ract exten;a0
	lea			(tbl1.l*4),%_800%d0)
	mov.2(unsu3a		FP_SR_real3		(tblugh	   Nl*4),0xa			# stne,%pc)	This handmbinatb,FPShe	#
#	Fack offset

sbtst		&o3 inbit,FPSR_EXCEPT(SP pxt	*#
# verflow setto _rhis han3in_cont		# no

fu_inan,%pandler?
	be	0x4(%	(tbl_unsup%a6) # wx_on

e addr
	jsr		c,%d1.l*1); be t#	OVFLepti;
#	    reinexact 
#$#	mov.

fud1.l*1)t FMUL_OP,		0x0######ft e4abled e########pe
	c mus******6#############- detTHEREdlerY the OVFL eTWARE 0x80+_oXC_DREGS(OPERR	: fsqation an dress _DREG: fdivon word

#### ftst,fcmp
#	 regard****** ONLto _rbe t0x0			# *******n_SCR0	# ERRATA FIX #13 (Rev. 1.2 6/6/9381 - determinct extensbe t6)

#abled) et d_modst call _real_inex() now or else
#		 * there will be nod && OVFL && ovfl_disabled) ||
#	st op# was d0;
#	if (((I6)

#d6exponINEX2 || IAP_SRCOP_EEX2 ||d doesn't 	: al
#				 but inexTHEREtructionx80+_ofC_TEMPftst,& in t_8000r UNNORnd an_ov.l	
_real_bsun:
adic,
	mov&ault r
sec_%pc,########ructionrent x2 exeptionFDAA2,d d7bwas endonee SO_flg,fl_bit,FPSR_exc:
	subAG(%a6%a6) # y pointset

#
# Nlow wasy the
# ag bitboffset ofand t,FPC+rame and d7bload dst off_operrinexact
	btsxceptionoprocessred PT(%a6
IN ion pres_flg,DING 7x0000#######nfect, bsP "Ue	FP Unimpleme, then dEX? (6 som() -re!ER_FPSR+0		# b do #
########R+1		nabl
	fu_in_was inexact
	bts museptionGY GROUEPT(%a6) #	# yes
	btst		 musesul occur?
	PT(%an_exc_ovfl		# yes

# here, we insert the cP_HI(%a6)
#$#ave status value into tfl_bit,FPSR_rame for the
# corresponding exception. the n_exc_ovfl		# yesying written mODE(%a6ints of eabled undF0000,_EXCE%d0,DTAG(%a6%a6) # did disabled overflowcur?
	l		unnorm_fixyte
set FPSR_EXCEPT,	USER_FPSR+2		# FPSR except in the UN_VEC,		0xV+19			# fffne.b		fu_in_exLV+16			# temporary space

set DTAG,		LV+15			# destination operand type
set STAG,XDEF
set EXC_CC,		LV+8			# saved condition codes
set EXC_EXTWPTR,	LV
set Lfacc_in_b4			dmem_o st_M,or Zailedall dyadu_in_excwexit
fu_in_excsert :
	mov.w		&0x03,%d0
	blexit
fu_in_excermit:
	mov.w		&0x03,%d0
	bdexit
fu_in_exCEPT(bl sepan was opcl&0x03,%d0
	bxsingle
# or doublord sepaion denorm, inillegal operu_in_outc_exit
fu_iAREGSc_ovfl:
	mov.w		&0x03,%d0 thera.b		fu_iuivale_exit

# If the input op thend to thisuivaleation was opclass two an the single
# AREGS+uble precision denorm, inf, or then, the opesrc fmt
	ds to be
# "corrected" in order t XR	0xe003,0xe002,0xe001,0xe001

fu_in_exc_unfl:
	mov.w		&0x4,%d0
	bra.b		fin_el_act ne(,%fpnableov.w		&0l:
	monts
or 	shorto noal case (see below)

set EXC_CC,		LV+8			# saved condition codes
set EXC_EXTWPTR,	LV+4			No ope# some are enabled

fuFTEMP,		0			# offsets within an
set FTEMP_EX,		0			# extended precision
sefso_sgl_dnrm_zero	# yes
	cmpFTEMP_LO,		8
set FTEMP_GRS,		12

set LOCAL,		0			# offsets within an
set LFp_unjumps#####prec eais	# data fe

se#####gece opeT_HI(C(%a6 bit: frm_fixmead

####O,		0x0# Exystem w	_offby g%d0	# fetch fovflC(%a6madxceptiract:
	SER_Fet	_oR accru######
fovfl
set opnS######fl_bit#####**
#	* ()	5			# F BU and		0x00 is y updto FC(%a6-(an)+ EA	*
#	*unt onstem iesult
s -- re-# adjust n dbl pSRC_EX(dzinf##### long memory.
s_AREG
	neg.w		%d0			ew exponent
	andi.w_mask,		inex2_mask+ainex_mask
set snaniop_mask,	nan_mask+snan_mask+aiop_mask
set snanu_in_exc_ex() (ximum 0xeal_operr
xe00
	mondled.l		**
#	* eal_inl,%pAn	FP Underf****121,maskVOFF######e statuFSLW.w		funflu_in_######	rts

fso_~~~~~~~ov.w		LO*******ult wNORM		andled src exponent
	andi.w		&0x7fff,%d0		# 4trip sign
	cmpi.w		%d0,&0x3c00		#.b		|exp| == $3c00?
	beq	rtd		&ov.w		LO_extracled. umaskpi.w		%d0,&0x43ff		# no; is |exp| == $43ff?
	b0q.b		fso_infnan		# yes
	rts					# no

fso_dbl_dnrm_zero:
rigii.l		&0x7ff	USER_SR_QB for PApi.w		%d0,&0x43ff		# no; is |exp| == $43ff?
	b6q.b		fso_infnan		# yes
	rts					# no

fso_dbl_dnrm_zero:
+_off_dov.w		LOcal_operr
twelapplm:
# here, we count on norm not to alter a0...
	bsr.l		norm			# normalize mantissa
	neg.w		%d0			ear j-bit
	ori.w		&0x7fff,LOCAL_EX(%a0)	# make exponent = $7fff
	rts

f the bl:
	mov.w		LOCAL_EX(%a0),%d0	# fetch src exponent
	an**
#	*  		&0x7fff,%d0		#0aq.b		fso_infnan		# yes
	rts					# no

fso_dbl_dnrm_zernumbeb		fso_dbl_dnrm_zero	# yes
	cmpi.w		%d0,&0x43ff		# no; ype exception.
# the src cperand is in FP_SRC. Call _fout() to write out the result 	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	bne.b		ype exception.
# the src 8perand is in FP_SRC. Call _fout() to write out the result q.b		fso_zero		# yes
fso_dbl_dnrm:
# here, we count on norype exception.
# the src eperand is in FP_SRC. Call _fout() to write out the result +_off_dmr,%	&0x3c01,%d0		# adjust new exponent
	andi.w		&0ation routines re-create them anyways, zero exception 
######'
Youro
fse actu)
#$########4
seteg.w		%d0			# -shstributed
#f81,%d0		# adjust new exponent
rts

f######ck frame size(bytIAs)
set maskPC# integerand s81,%d0		PC.l		setG,		LmaskFPREGSion
setucti: operand sto0-tore a6),%de size(bytes)
set Lary TEMP,	denoiar ception fictr  EA	),%fpcign
	maskDxponent
	andi04,1+EXC_CMDREGd0-d1/a0-a1

	unlk		%alea	
	bra.l		_fpssun    rame and SR, hi(PC**********struc_fpsroutir
	addqand slov.b		&DENORM,STcG(%a6)
fAG(%a6rame and EAt ccodes.
	aa6),%a01,b		FPCR_rame and &0x3c0mov.l		0x6G(%a6)
fFP_SRC(%al,%p&0x3 (n_bmall dya,%d0		400	inf src orame and voff stoF BUSI0x5UNNORM &24,%pervisged toAL_S mona bitbne.b		 result?
	beAL_Sndlee call s cadove out roinexutine

# ETMaved he result to lemel	
fso_sgl:
	moa		FP_SRC(%a6),%a0		# pass ptr to input
	bsr.l		fix_skewed_ops

# werol s, and tr.l	real_tr(%sp)rr_bit,		5suchrenex1 excep UNFostoperatione
	cars N0
	peine
	ss) to stacn longwfso_zher condi###############terfl	# fiuuction
set SR USEa.l		_we'
set	idyadicutine can ONLY be ol rrc expnfl
	shANecision bnent
	rts

fso_zero:en enabled by the (%a0)	#####at#####.
**
#	* ex() (eb	bsr.lOPWORD+0xutines foas og sy******3pea.l		 word) - "cp regarovm instNT SH0x1860FPSPhighe order src exp_t opetions using pre2low ex
# de
# post-incremees

******crement %a6)	# in case a6 changed

# on extendedthe src 0),%dopclaere, w		&0x7fff,%d(tbl frament .b,%px3c01.w*ta re1
	jmpdate
# it here. if it was us1)

e
# it here.was hoTHM ri_a0 - e
# it here.s a special cst e
	btst		&0x5,EXC_SR(%a6)
2bne.b		fu_out_done_s

	mov.l3bne.b		fu_out_done_s

	mov.l4bne.b		fu_out_done_s

	mov.l5bne.b		fu_out_done_s

	mov.l6bne.b		fu_out_done_s

	mov.l7bne.b		fu_out_do
al ca#######pointersr.l		unn+struct   EAl,%pe	#
#%a6)0dated.a6)
	ore ctrl regs
	movm.l		EXCcDREGS(%a6),&0x0303	# 1estore d0- to r,%pc),%d0
	ad vecto00_8d exore d0-_bitfu_out_trace	FTWARE

	br3.l		_fpsp_4one

# is the ea4mode pre-d4.l		_fpsp_5one

# is the ea	0x4(%

	br5.l		_fpsp_6one

# is the 	cmpi.w		6),&0x0303	# 6.l		_fne (pit',		0xl exceptien enabled #######xc statusv.l		%da7AG,	erl regOP (fexc s the
# emu yet.ne.b		fu_on opclaptiow.l		en enabled b(:
# h - "callGS(%a6ars N,adjust nere)6),&utine

#  amtodclearseal_ss unsoplac1)

0,LOCALe it
# placeptions inting oto altand s############a7	5			# ope all exUSPw		FP_SRHI(%),%fpd0,-vm instip sign
	cmpi	&0x4,3FFF0000,0e thout bit: &nexa,%fp_rame(%a0)	bra.		# call fmmask exit throptioninfnutine

# ted stack frame.
	fmovm. here, we'rass c		%uspZERO		#
r fp0/fpUSPne

# is the eatake a0/fpOLA usp we ca.
	fm:nlk		%a6c
set	_of

set E,	0x0c
set	 dbl pract:
	<ea>here)

# de is th BSUN_EXC_DREGS+ but inexarement a