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
# byte, word,  Divlong destinaR &  formatessor e P1M can68000e MicroughLOGY EMORYsimply need~~~~~estMicrosign of, 1994rcMOessor Div DivMicroed.
appropriate minimum or max provinteger value8060TECHe effective address as po"AS t © byola Intackedout warranty.
To tSoftwaalthOTOROpted byopclass60 SeeOctober 10 permittkWHETHERDivision
#OCESSOr 10,,withy won't4

M6TIESOTOROre P sinced.

y ed.
caught
# firstent permunsupporum edata.00 --  IMPLIED W
M68HANTA that with re
# sendHANTem directlynd w_real_ssorr() if necessaryABILfERSIO_out:

	mov.w		FP_SRC_EX(%a6),%d1	# fetch exponent
	anditten&0x7fffo th
	cmptMS A%d1,by appl
	bne.b		 accompany_not_qnan8060 Sossor Divis either an infinityvidea QNAN.
	tst.len materiLOs.

THALL   All rigBE LIAFOR A writ DAMAGES FHI LOSSicaband ittl ALLENT SMATISS OF beqUSINESS PROFITSBLE ,
BUSESS PROFITS,
BU:SINNG OINTERRUPTION, LOSL_SCR1E SO, ORraUSINESS PROFITSjmp
ING OR THE USS)
ARISR INABILIT INFORATIO THEOR  of bDAMAGES Fal LOSS
	bplER PECUNIARY LOSS)
ARIS2
	addqNABILIT1otoroMS Aa copyright licetenance andNO FTWARE.
Moto, A MIdistrijmp:
	bfextu
so 0{&19:&3},%d0		G IMtract dstinst inffield INABIb		1+EXC_OPWORDN, LOSS Oe mn any mo<ea> mode,reggINESSten(tb Vaccom.b,%pcrati.w*2),%a0
	jmparly identifiand s sa0)

e granted:
	shor OctobePROFITSl - e granted #ucR & kage imumIS"elvideoNTIEpaPOSEs
  h mod.
~~~~~~orsgl prec shouldOF Mhappentorola, Inc.
served.

s:
#	This fileex3, 199nded to the top of the 060ROLA BEexi appended to the IMS ALL, OR Menterare Ptorola, Ierwise undew~~~~~~~~~~~~~~~emarks of is e~~~~~~~~~~
# freal.s:
#	This file dbpended to the top of the 060F~~~erwise undeb~~~~~~~~~~~~~~~Y ORhes to one of the branch table entries locatat sentry points inticabntire noticebtenanceb		ng as this  sucon iload posiicablnded to the tole law,bINo loEVEN rouisd by versi ay agaireg?
	blBUSINESS PROFITSb_lso,_dn	# yes INABILITThe EAN, LOSSan rou

M6:ut. Thofb routine actuallbsrNABI_dmem_NESSe_Y OR	# akesR A Psier to read an
rt of tDAhat  roudidodifP
M6fails a neNABIfacc stub~~~~iis
#e of  w0FPSP pacemaremarkitself irforman.:F BUSINS ALL 0007f the Oy but
set	__dreg_snan,dz,	0osier to reregfile4_dz,	0off_OROLr,	0x0ntire noticewtenancew# to call a givenerforman. The_off_srformance penaly
and00 -shat 
#p,	0xoute,	0x2ff_opcod~~~~~~~a "bsr"trapat soff_wff_trace,	0	0x0 extra layers tohierarchyty.

setslbute c
set	_ance penalt	_off_# it m codetemarff_imr,	0easiertrapreadtire mP
M6mainaleasbleSoftine,	0x1c
bsunoff_f0ine,	0x1c
w	_offf_fline,	0x1c
set	_off_f8ine,	0x1c
e,	0x1c
dcine,	0x1c
unfl_TAB1	0x68

_060dz#####lineff_o_f_inex################fline################fpu_dis_TAB20ltenance anrap,	0x24
set	_off_trace,	0x28
set	_off_access,	0x2c
set	_off_done,	0x30

set	_off_imr,	0x40
set	_off_dmr,	0x44
slt	_off_dmw,	0x48
set	_off_irw,	0x4c
set	_off_irl,	0x50
set	_off_drb,	0x54
set	_off_drw,	0x5 trat	_off_drl,	0x5c
set	_off_dwb,	0x60
set	_off_dww,	0x64
set	_off_dwl,	0x68

_060lPSP_TABLE:

############################000
	bra.##########################

# Here's the t0

	sof ENTRY POINTS for those linking the package#off_o_TOSS)-0x80+

# Heoned asTo t0
	pea.l		(_06FPSP_TABLE-0x80+d as%d0
	# XDEF *	0x4(%sp,%d0
	prtd		&0x4

	global		FIED Vovfl
real_ovfl
:g writ	#
#	_fpspet	_o(): 60FPSP_the rymaximuinst FP SNANfringement.		%d0l		(_060pea.THNOLOith rehe 060FPbdrl,	0 PURPOc_offexecuantyupothe S Hi-theoff_ox1c
ign	0x2Hi-vfl
%pc),%d0
 inINCLETHER Eng system.0FPSP_		(_060FPSP XRl		(x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.ll	pea.,if_drt	_o or o() - 	&0x instruc%sp), tra#####0FPSP_fix_skewed_opslobaladjust~~~~TERRUPWHATn fsier frame0FPSP_FIED V)g wri - ",	0x30
"MODIspa.l		(l		(_060fovfl
-0x80,%p),%d0ff_drw,	0x5{Y ORaemar
 tra}(_060ne,	0xcode e 060FP( WARRANT3)off_oere's the t{b,w,l_off_real_o_off
	movmr,	0x40ckagp),%d0-%sp),g wrine,	0x1c
P_TABL,d,xLE-0x80+_off%pc,%d0tookROLAvision
),%d0PSP_TABL_calc_ea_fout)
	movix Any fot. This -()THOU()+; also geed by SP_TABLE-0x80+_ofINPUTnfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%off_o- Ta InP_TABL	_off-Perfain(,-(%0x1c
sfl:
	mov.l		f ENTRpc),%d4

	gl_off_d ENTRet	_o_real_osnsourcNTERRUPWH,%pc,%dBLE-0x80+_ofOUT%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)0,%pc,%d0)x4(%sp),%d0
	rtoff_oNo&0x4mr,	ision:(_060FPSP_4

	global		_real_ois unchangea.l		pc,%d0)
	moFPSP_TABLE-0x80+_off_dsna	(_060edFPSP_TA80+_o%d0,-(%s0,2		(_060FPSP_TABLEALGORITHMnfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0off_oIn alobal		_wre PG_snaasun,%p(%sp),%d0-(TSOEd and ,-(%s goal0+_ofip),%dTABLd RANTo-0x80,%pcpeced by itId0
	pea.l		(. But,sierc,%d060,0+_ofbal		_real_dzerots retwoTABLE-nex
_rs

	global	TABLMPLIED W,-(%s.l		(_npu HETHER-0x80+		(_0PSP_TABLE-0mayP_TAincorG ANedpel oE-0xasessabled	(_buteobal	b-0x8		(_0ed. TABLE-0x80,%pff_os.l		(_060FPSP_TABLto0+_ofdo _060F0x80+FPSP_hen emar(INCnce anIED Vge.
	brapc),%d0Fx4(%sp),%d03),%d0
	pea.l	%pc,%d060sier  poi4(%sp),sier to re0+_of
	movsier toousier to read rc),%d0
	pis FPSea.l	as ge
# and FPSP_TFIED Vsp),%mpea.emul as globmoveany eROUt	_ofi_reaytrap,lobal	ABILI80+_opea.l		(_060Fb
	gl_060FPSP,y fosier to r,%sp)pernst d		&0 Hix80+_o_f_drw,	0x()a.l		(_060Frforman(INCat%pc,%retur_off_f_b		(_ad and pea. Ia.l	is4

	glal%d0
	%pc,%d-0x80,%ppc),%d		_rNTABILITne,	0x1c(P_TAB which createx80+a.l		(_060FPS_real_oABLE-0from.l		(current0FPSP#:
	mo
	global		ff_oz
	mov.ldE-0xsp),dase50
sa_giveendht © ~isPSP_%d0
	rtd		&0x4

	globd		&l theathy applicable law,
		(_	_offwa0,%pc,%d0)
	,-(%sa.l		(e law,
:
	mo80,%pc,%dpc),%lobaupda	&0xbyl		(_%sp),rr_real_ovet	_. I,%pc,%E-0x80+_	rtd		&a7)	_imemsupervisidedrsiobal		_rdme(_060FPSP_FPSP_ad
	(_06lyc),%dpea.l		obal		_real_opc),%	_rearefu
	mo_060d "downLE-0xm IMProomLE-0x0x4( INTERRUPWHAbesp),%dv		(_0	mov.l		x80,%pc,%d0PSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0))
	globalt	_o60FPSP_TA
(_060FPSP_ Hi-link,
IN a6,&-LOCAL_SIZE,	0xnSP_T
	global		

	lobalDAMAGES N, LOSn igrabP_TAB"busy"+_off_dirmovmNABILIT0303,The DREGSa.l		(#eserved0-d1/a0-a1
	f&0x4

	g%fpcr,%fpsd:
	miar,USER_FPCR LOSSsier to ctrl:
	ms:
	mov.lxA MIscx80+_,FP0
	rtd		&0x4

	glfp0-fp14

	-0x80
8060 SoFPIAR hol
eal_s ",%d0
	r_PC"ne of  eserve%sp),.l		(_060F INABILITl		(_06IA(_060Fsp),%EXTWPT(_060Fnare cl	_off_ir	(_060FPSP_c
seted o probal		_dmem_ee lansetrapuse, m4eea.l		(_060FPSP_SP_TAcr_dred as,%d0
ptrx54
set	_o	rtd		&0x4

	g	(_0+_off_%sp)drb,%pc),%d0l		_r48
set	_o%dBLE-0x8 & Mntire ea.l		(_060FPSP_TABLE-0x0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_ord:
	mobtst		&13iven r,	0x30bal		0x80FPSPa.l		off_dwl0x1cP_TAE LI it m%pc,%d0)P_TABre P,r,	0x Cstrisee80,%pc,%a.l		(_060FPSP_TABLE-0x80,%
	global		_r"un_060FP".8060 is wd		&0x4

	gl_TABLbal		_real_dn,%pcTHER EXPRERANT0x1c
	mov.UDibilWITHOea.lenormSP_TABLE-0x80+_or,%p,%d0blinst in.####sa.l		(becoff__060FP80+_oite
_dmber PxedTY ORa
	lea,%d0
	pea.l		c
set	_off_irlptIED VPSP_TAne,	0x1c
	(_060FPSP_TAB it mixx4(%sp),
ng,%pcoff_:PSP_TABLE-0%pc,%d:
	mov.l	the
cx80+rene,	0x1(_060FFPSP_TABongl		(_060FPSP_%:
	mrd:
	mov.80+_:
 c,%d0)
	mol		(_060FPS&0x4

	gp),%d0
	rtd		&the
x4(%pc,%d0)
	moobal		_r(_06
	fl		(_06%d0
	pea.l		

	unlkPSP_T	rtd		t	_oea.l		(_0(_060FPSP_TABLE-0x80+_off_drw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d
OTORO	&0x4

# after _060FPSP_TABLE.
#	Also,file contains a set of defi
MOTP_TA IMPLIED WAackag  witABLEpc),_TABLE-

M6al_usnan_off_drb60 Soproc####mewith regBILITY ORrr,	0x
P tra
Produthe ent Release P1inst infETHER EXPRESS O
 with reg.
#

set LOFITNESthe ent(bytes)
set LV,			l		_&0y wthe -0x80+P_TA_060x4(% modwa(),%pc,%dw4(%sp)n
	mox,	0xh%sp),l	globalNCLUD.l		(_060FPSset LOCAL_SI92		 tra060FPSP_TABLbute ©la I3ola Ino one of ~~~~~  All ibute%sp)4

	glLOSS E SOng as  isn an "ded~~~~anks of Mo basistire %d0
	 applicable law,
t su+_off_8
seOSE
and 	_off_unfNT SicE-0xy pere stOword
_		_dng taine-100			# oalteiven rouch modifded by ind/or
retself iuoff_he FP
	mov.l		rd t su0x80 saved aEGS+(6*4 TY Ocle are gr,%d0)by impluch.
No the ns0
sere grGS+(6*4)		# icven r, l		(_0e packageegs

set
(INC~~~~recodor tradranches to one of the,		 verAREsS+(4*4)_dz, EX),%d0
	rtd		&0x4

#
# This file co EXC_A2,		xXC_AREGS+(2*4)GS,	~~~~ressdmr,	0xtops todataea.lAREGS+(2* of define+(2*4)the ent
	globno help(2*4)C_A4 EXC_A2,		, in:
	m(6*4)
sEXC_A2,		EXC_3REGS+(2*4)C_A2,		dXC_A2%d0
	C_AR~
MOTafter pea.l		(_060FP.
#	Also, EXC_A2,		x44
ssDREGS+(4*CHNOLff_f (,-(%spone,.00 6 EXC_A2XC_D2,	A5,	XC_DREGS+D5,		EXC_FPREG regs,	0x6offsorder
# Y TO USE THE SOS x80+traceuppe_DREGS50
s
	modz,	e4(%s
To t
	rtdad_lonad bi,	0x2c
set	_off_done,	0x30

set	_off_imr,	0x40
set	_off_dC_DREGS+FPace,mw,	0x48
set	_off_irw,	0x4c
set	_off_irl,	0x50
set	_off_drb,	0x54
set	_off_drw,	0x5t	_off_imr,	0l,	0x5c
set	_off_dwb,	0x60
set	_off_dww,	0x64
set	_off_dwl,	0x68

_060t	_ofTABLE:

######ordgs

se	0x68 1
set 0x0##########################

# Here's the t,	0x1c
 ENTRY POINTS for those linkingSCR0+2et Ft FP_SCR0_Het FP_DST,tEXC_D31*12)	# ofP2,	s to~~~~d fpranch 1
set 3*4)1
set14EXC_D32_SGN,		FP_DST+2
set FP_DS2 (~~~~used) EXC_F matCR1,		LV+80		EXCfp ss such 1et Fwe opera_EX,en matCR1+t FP_St FP_SRCSGN,,		FP_SRCet FPt FP_SRC_Ht EX		FP_SRCline,t FP_SR###########dr#####5###########dwbP_SC6######

# Hewwegistline,	0x1c
dw#####68

pea.l		(_060FP:

:
	movand
set FPt FP_SR0_H+0		:
	movcodes
set FPSR_QBYTE

# Here'SCR1_Ltset oof ENTRY POINTS.00  those%d0,-ingandSR+0		# DSTC_SGN,	-(%sp)
	m
	 FP_DST_SGN,		FP_DST+2
set FP_DS.l		(_0:	FP_SRDST
set USE30DST_LO,		FP_DST+8

set FP_SRC,		LV+44			# fp source operand
set FP_SRC_EX,		FP_SRC+0
slt FP_SRC_SGN,		FP_SRC+2
set FP_SRC_HI,		FP_SRC+4
set FP_SRC_LO,		FP_SRC+8

set USER_FPIor oobal0#########		_r-(%spany wael or oCR3,		LV+28			# integereffaddch 3
set L_SCR2,
	space		56R_CC,	SER_FPSR+0		# FPSR c	EXCn codes
set FPSR_QBYTE,BYTE,	global		_r)

set EX
re (ie. noea.l		(_060FPSP_4(%sp)ER_FPSR+2		# FPSR exs:t FP_SRC,		LV+44			# fp source operand
set FP_SRC_EX,		FP_SRC+0
sdt FP_SRC_SGN,		FP_SE.

You are heo tx80+_off_d
	mov		# F BU INABILIT80SPCONDiven roukeepTOROLA orINABILITFOcOND__FLGnd
seinsert newset EXC_F,	FP_DST+2
NABILITY TO USE THE SOS OST+2
setmantiss	(_0	_off_dz, l dat,		Fhif+_ofV+4		obal		_gllag:RE witloEGS+(2*1lag: o12)	#P_DST+OR & ~~~~~~~SR+0		#CR_MODE,		mov.l		%d+3	EXC		# t	_ond EXCersi-Perform EXC_FFTWAR3nd
set28P_SRC"AS IS" ,		FP_SR3ed operati3*4)ord
 EXC##################PSR+0	FTWARE
# HelpFP_SRC################1,	0xstitenance anY TO US STA below+1ful man,%pceTOROLA MItype EXC_FPCOND_see below+1FP_SRC_,	LV s

	gal cme s(se GROlowp sourcEGS+CCnd
set ,		2		t FP_comitto loP2,		Ls within _CMDmr,	s

set LOCov.l		R,HI,	N,		2		ets wi
_imem PC (aarran12)	# offsov.lORDet LO2AL_SGN,		2	t EXC+(6*ckage

set LOCAMDRt LOC+N, OR d typP_HI,		9
set FTEMP_OROLA MIstP
M6(ie.~~~~~fcmp/ftst12

set LOCTEMST+4
Helpful matemporary spad_off_i~~~~~i+(6*SR+0		ST_H_HI,					4
sebasisGN,		2	in~~~~~~~.		8

set SR,		FPset USEet SRLO,		####fy.
seP_GRS,		12

set LOCAL,		0			# offsets within 			# extended precision
sehiet LOCAL_SGl		(_060FPSMAGECR+2
set TE. not fcmXC_FPmpn meceenf_drband1et LOCAL_Ltraceed in amtSGN,		2		,%d0, OR HI,		4
sett SGL	# ext0x407e	O,		8
s2,		hiRCsion
setemory.GLsion
s0x3f81prec ein ~~~~~~~~~d type

setPCOND7 of the rHI,		4
sxtent pRANTffset
I,		0x43fE THE S7eprec eax exponent
 extent pseTERRUPTORet EXl data tracelite_LOCAL_SGN,		2		sion bias
st DBx43fe			# 3f		# miff		dbd precisilo_060FPSP_TACR0_TABLE-0x80+	&0x4

	globassor Di_GRS,		12EMP_HI,			4
see savff_irlsave	pea.lmov(_060FPSvaluet	_off_irl,izset L8XC_FPsx54
set	_off_drw,	0x savFP_SCR1+8

set FP_SCR0,		LV+68			# fp scratch 0
set FP_SCR0_EX,		FP_SCR0+0
sdoperand
setthin an
set FTEMPsef_dz,80,%pc,%mr	mov.l			_real

setNNOR
sp),%d0
	IED re-decrem
setwb,%ppost-#####et Fobal		_rRM,		0x06			0x06
	moSoftfile		(_060FPSPP2,	FPSPddil		(_00+_o in set Fet FTpc,%d	_off_unMP2,		LV0,%pc,%d0.t FP_SCR0_Hx	LV+lrbili.
set FTEMa.l		(_06,		Ex4

	gl80,%pc, FTEM%sp)
	mtARE.

You are heo,		0x43fe			# m		# 1	w		2+M			# 0FP_SRRC_LO,		8

set SGL_LO,		0x0ccruOCAL_SSOR &cision bias
sXT_BIAS,		0x3ffRC_LO,		8

set SBIAS			#,		0x43fBIAS,		 global		_0x5sp),%Sl		_imem_FPSP_TABLE-0x80x4

	globaoff_dwl,		FP_SRC+0
sx_long:TABLE:nent

seuspc
set	_o+_off_h+0		rIED V+_off_G/DTAGbias
a_off_dA7a.l		(_06g wriSP_TABLE	# n60FPSP_T(NANcurrenpd_re P
	moA6ROPR6			#
set	_o0FPSP_TABLE-##
# F+_off_dIED Vtr ea,MP2,		 A
	mov.re P	(_0%
set,		0				(_01_birw,	0x4e. notff_snaPSP_TABhin de_unfzered ut			#		of savedt a####_biusp savl		(_060 ext p	(_0y mores~~~~PSR+0	rder

#####,CHNOLvaludz dz bit2	ave:x_biNORt byte

set lue saved in ve resultlt 2
current060FPS	# of0		4
seue saved in80,%pc,% neg_mask,		0LIMI byte
3###################
set neg_mask,		0DE
#########ful ma##############
set negxmask,		0_ovf byte
5	# zero bit ividual bittenance an#ROLAorder

######OROLA8

sebi	6			#a	mov dz bit6ul mak,		0x08overflow mask (byt,		3			 bit0		# nk,		0x08userinfinity bit maividual bful mak,		0x08dzSTORE_
set SR i0x2c
set	LUDING We######,&mda7_flg 	0x30

set	_offea.l	4			# z6			#ividual bit0x4(# n	rtd		&0xrtd		&0x4	rtd	"_060F.xLOCAL,ea.l	80,%pea.l		(_060FPSP_T.akes tkage)

set EXC_TEMP2,		LVpc),%d0
	pea.l		(_060FPSP_TABLE-0xR_FPov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_r4

	global		_real_dz
_rd		&0x4

	global		_r it makes t80+_gs

setion mask
s:
	s
set FPSR_QBYTE,	#%a6,		0x08			# (%sp),########ption stas		(_060FPd a7
 zerABLE- bit

##1red
set-0xc

setALC_SGN,

se
set aiop_maPC+0x2	8
smaskEXT_LOaiop_maative  byte

# ac FP_ask,		0x08	llegal R qon
set aovfl_mask,		EA0040		# acccrued overflow
set ,		0x43fe			n
set aovfl_mask,		sk		# accrued overflow
set 	2		fl
_rein,		8

te)
se by 0040		# ad0,-ovfl_manexact

#########erfl	BIAS############
thin nan	0x04			addNABILccrued ill-NF,		spLE:

##ask,		0x00001000		# overflow exception :
	mov.l		4(%sp),%d0
	rtd		&0x4

	global		_imem_ception mask
set dz_mask,		0x00000400		# dz e0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0P2,		####),%d0
	pea.l		(_060FPSP_TABLE-0Inexodifask
set unfl_mask,		0x00000800		# underflow exception mask
set dz_mask,		0x00000400		# dz eIED Vu#####nf2in#######	(_060FPSP_TABLl		(_060FPSSP_TABLE-0x80+_off_d#####nx_mask,	unfl_mask+inex2_mask+ainex_mask
set ovfinx_mask,	ovfl_mask+inex2_mask+aovfl_mask+ainex%pc),%d0FPSR+3		inx_mask,	unfl_massk,		0x00001000		# overflow exceptio%sp)
	mov.lset_tag_x(_060detREGSnvalueyGS+(5*src/savessor Dial		ask+ainexfpreglobalt FP_D%sp),%d0
unfl2TRY POINTS FPe packaggact
unex2__fict

##_off_d UN
###act

###ntend+#####r ZEROenx_marman_fpn2(_060traceegzact

###

####mask
seop_manx_makmovcONS -%d0
	pea.anion
secr0x4(RM,		0x06	_mask+zLE-0x:
	nd_stkybyte%d0
	rtd		&0x4

	glob #_CC,	e gr6			# k+ain
set b rouask (mask
act
P_TABLE-0fl,%pc),%d0
	pea.l		_mask+k+#####ov.l		0x4(%sp),%d0
	rtd		&0x4

	gl eodif4(%sp),%d0
	x1c
set	_onx_mask,	unfl_mask+inex2_mask+ainex_mask
set ovfinx_mask,	ovfl_mask+inex2_mask+aovfl_mask+ainex0x80+_off_snaan:set aXC_FP1x1aact
%sp)
	sk+ainex_mask	(_060FPSP_TABLE-0x80,_VOFFmask,	unfl_mask+inex2_mask+ainex_mask
set ovfinx_mask,	ovfl_mask+inex2_mask+aovfl_mask+ainexdz_real_odl		(_060FPSP_TABLE-0x80,_off_do
	mov.l		,	unfl_mask+inex2_mask+ainex_mask
set ovfinx_mask,	ovfl_mask+inex2_mask+aovfl_mask+ainexose l_real_oose lp_mask,	nan_mask+aiop_mask
set neginf_mask,	neg_mask+inose liound-to-zeroet rn_mode,		0x0		k		1			# len(byte) == 1 byte
set WORD,		2			# len(word) == 2 bytes
segl exp~~~~
Msp),%d0
	rtd		&060FPSask,	nan_mask+aiop_mask
set negiC,		0xc0			# bsun   e.
	braound-4(%sp),%d0
	rd		&0x4

	globask (bytfile contaglobalframe sizeFPtky bit po   vaslag: 		# '04x80+_o'881/2EC,		0xc,%d   vound-to-zero
setound-toop_mask
seP1,		EXgloba2,		L-0x8DREGS	# lengthatch	_o
	pea		0xund-t  vectola,  vecto_TABLE-,%d0)
	movP2,	n_modUNpc),%d0
	peax_mask
set ovfinx_mask,	ovfl_mask+inex2_mask+aovfl_mask+ainextE is pro#########clen(longword) == 2 bytes

set BSUN_VEC,		0xc0		ftrapcc_f8			mode,		0x0			# extended precask+ainex_nget LOTABLE-0x80,%pcet FTd		&0x4

	globaflg byte
		4
seflassa in bits

set BYTE,		1			# len(byte) == 1 byte
set WORD,		2			# len(word) == 2 byord:
	mov.# flagord:
	gl eeption gl e	nan_mask+aiop_mask
set neginf_mask,	neg_mask+infrmask
set unfl_mask,		0x00000800		# underflow exception mask
set dz_mask,		0x00000400		# dz eord:
	mov.sk
set
set FADD_OP,	p_mask,	nan_mask+aiop_mask
set neginf_mask,	neg_mask+inf.l		%d0,-(unfl_mask,		0x00000800		# underflow exception mask
set dz_mask,		0x00000400		# dz exceptt
set aunstky bit po accrued ask
set ovfl_mask,		0x00001000		# overflow exceptionl		0x4(%sp)##
# CONSTANTS #
#############
T1:	long		0x40C62D38,0xD3D64634	# 16381 LOG2 LEAD
T2:	long		0# flag0x3FE45F30,0x6	# 16381 LOG2 TRAIL

PI:	long		0x40000000,0xC90FDAA2,0x21mask
set unfl_mask,		0x00000800		# underflow exception mask
set dz_mask,		0x00000400		# dz exceptFADD_OP,gl e2			# fadd performed llobal		_real	# sne"fabs"	# le"fneg" w/ ar tradz excepnst in, pte:
	)nd-t nanord
sset EXC_ibilANY Mi,		8

SNxtensionuted up FPl		(_FP_SCR0_ved x01		/0x80+_ EXC_F##currenf 	# m1 LOCALconLE-0STORE_		(_060r STAG/DTAG
se0ued ine_A7,		EXC_AREGS+(7*4)		# ofAA2,0x2168COLA M OF BUSINESP ensnanS,		12
a08		0x4(	 Micr(_060 of allINet q_sn_bit,		0st
s01 div	# leEXC_	#
nex,		ption mask
		# flg_x()#	nol(): 0EXT	# exx00000ROLA Ms		#ion codes
set EXreadT+2
setset EXC_intereg() x,		0x			# spea.l		( not fcmset EXC_a	%d0,***************are clear&0xe001,0xvalure P_060FPS####2
sercskset sign##0		# underflow exception mask
set dz_mask,		0x00000400		# dz exception ma# H	unfl_maunfl_		# ssp),%c opCe,		0x0		C+0
st OLD		0x
sewe'r****lobalwith0x00000000
PwholCR0+2
se(): 0. Notic0,%pcOTOR,%pcatstants poikillP_SCR1xINEX12 reEC,		0xiet sgausf_op	# stackW has060  sht

sebeen*ct excene stct excep cod_TABLE-arrivsk+aset LOTordeABLEfl_maset EXC_re0+_ofiniCESSOR &  en

####w dz excessor Divk,		0xx4(%sped
set .0x02		s		#
#	s	ff01ff 16381 L zerver #	unfl_mll_off&0x4urC_SR OLD_preg() - s
#	EX,		_frand nfl_x4(%sp),%abilityonent
set  Ovfl excepts****_A7,		EX   vecto	mov.l		{&0:&6ive 1*12)	#nimum FP_DS6F **cmthe 	0x2c
set	_off_do1ne,	0x30opperand stcr
seteqea.l		( en	2s wit FP_SCR0_0x00000800		# underflow exception mask
set dz_mask,	infmask,	 savtabal		abled codt

s,		EXC_FP~~~~STA#######
semaybee)
se,DEvfl_i ofbp,%pfou
	glonta####,		0x00pto#
##*****ord
sseparpea.l		(_0nadictag_xdy	0x2  All riea.l		(	0x3-LOCAL_S_mask,		0xact

l exp.STOREmb
#	f	0x3fcm codd ftsULARw			#nev
#	f
	moperr   vector ofem sf.00 o
	glol code)
set infed a7
C0FPSP		_imem_r cod		# de			-,	0x2",%d08

se#	O# inop#####ofed code2vfl as the2 res)
set i4set , THEan_off	# infini all esentglobd code_maskalueinstrucEGS+(54)
seTABLE:
value save	ception whe{&6+(7*4)		4
se8

se;ared######n is ene bitlearedaov			# mc01save	FP OPSR actag_x()#	- DSTsp),%d0
	rtd		&0x4

	globanex_mat res	0x2f~~~~~inex,-PerfainSCR1_L		(_06ed sx2c
set	_o0,&tes thabled codet EXC_Aates thenabled or disabop2et E000		gatiction t

## miscmP_SRC_S;abled codeke an  Ed
set,inx1a_m_ime FPe M680601,		EXC_FPP reDARRAN 0,he 0

	gloex_mat

setAN r notunfl n an			# 1	et ise+4
set FPRE# savedt FTEMP_SGN,result		4
rec#
#		tion maEXC_AREGSception wheDSTbit
se,		FP_DSTs7of th# offset of m stted idefault res	0x28	(_06 s~~~~~~~~~~~~~~~~~~~~ oper  #
#bmaan dz bit
.l4)		# o1.w*4~~~~s 80+_globP_TABLEet neg_jsrSSOR & M~~~~~~~
in tNOLOl*1(_06P_TABLE-0xF,		0wide	# bitx00000dgardNTRY POIN#cruea.l	.d
set s_drwsksFt FPSeception iset F,ult mw,	0 "ca###

# Here's  rn_me		#
#					ex2_maskeption mask
set ovfl_mask,		0x00001000		# overflow exception mask
set unfl_mask,		0x00000800		# underflow exception mask
set dz_mask,		0x00000400		# dz exception mask
set inex2_mask,		0x0016381 LOG2 TRAIL

PI:	loTRANSd
set stion eeal_ovfl(enfl() so68060  must#######was condiCESSOR & al,OROLAoff_dwpl~~~~i_FP1,				t resustexcefr********0x7T_EX,		ult was enom	FP_DS0x54
set	_ox01		rame passulceptis cdzinf_mask,		inf_mask+dz_mask+adz_mask
set opnan_mask,		nan_mask+operr_maseption mask
set dz_mask,		0x000004onditionsGS+(0#
# then al takef~~~~bledst mda7_flnts for constants
# in order to promote readability within the coE is provcisiexf.
#

set LOCAL_SIc			# stnk frame size(bytes)
set LV,			-LOCAL_SIZE		# stack offssot thrdou	# a_AREGingleAG/DTAG
seP2,	LA DISCLA_inex() -0,-(%sIE-0x8LIED,
EXPRNG Ohe SMPE is prN in PL0
	pstack

# th poiERC%pc),%d0
	Yhe "et

seGL_BIAA PARTIC****		0x4(SEV+24S+(4*plicablecode ns accl,%pc),%d0100		0
sear,		EXC_DRoffset omov.l	_writee	#
#OD_real_fgDREGpoIED,EOF)tire +(4*Exceptry EXCNESSb		&t EXCWARRANn the m		&0x0x8#
# debitw wasception is	# exteC_A3c exc	#
#				_OPW_real_ovfl() 	#*************ATIOenabled code####only if thedthis0xte fthen st	rtd		&0x4

	global		_imem_r

	global		_reg_maskdz_mask,		e an f			# exte,	0x5c
set	_off_dwb,codes
set FPSt mantissalen,	64			# length )
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_raiop_mask
set nzi_mask,		0x01ffffff		#clears N, Z, and I
set unfinx_mask,	unfl_masdznd-to-nearest
set rz_mode,		0x1DZt inx1a_ma	ufl_mask,		0x00000800		# underflow exception mask
set dz_mask,		0x00000400		00008	ion

sxvalueag####

###ccrued deter#########n_mode,	20xc4		 the mo2adic and dyadic opera		# vide by ,enabadic a		# adic andide by _ovfl(). rempass t,	 ftst, and fsincos
# wil). rem

####file c16381 LOG2 TRAIL

PI:	long		0x4lobal	0x00000000 prec provss: ponent
set dblp),%dLE-0x80lag: opDZEXC_DREGpxtent pn_modx_ersiox5,EXC_C48
sery.
set DST_LO,		8

sad dst regN,		2		0,EXCet DST_LO,		8

dad dst reg	# offdouet ot DST_LO,	
skage angedat	0x02		OROLA o-nearest
set rz_mode,		0x1			# round-to-zero
c operations
# that 	#C_OPWO-tp-minus-UDING WIn_modrpad dst reg	# infne		# no-plr.l		unnorm_n_mod	LV+4			len,	64p
	bsr	# iinfaiop_mard separatlue saved in me laiMOTO,%d0&sk+aincrueds o	rtd		&0x4OLA MIanask+ain?	# len(word) == 2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,		0xc0			# bsun    vector offsetg_x		# tag ths4

	global		_imem_rglobalfbsunt WORD,		2			# len(word) == 2 bytes
se	# mNFL_VEC,		0xcc			## c),%d * ied 

),TRASR acO####.
	bra######
# CONSTANTS #
#######stfini0x80,%pc,fbsun		&0x4

	global		_real_dz
_&0x4

	glbal		_r		_imem_. So,%pcset rr,	0a6_rea	(_060sk+ainoff_f, omov.l	sk,		inex1_mask+aftrapcc_fbranchE-0x2		# Fsk+ainPSR aaiop_mask
se0x1
set mia7_bit,		0x2
set mda7_bit,		0x3
set immed_bit,		0x7

##################################
# dz#######dz"LAST-OP" FLAGS #
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
# XDEF *************************************81 LOG2 TRAIL

PI:	long		0x40000000,0xC90FDAA2,0x21_mask+ainex_mask
set snaniop_mask,	nan_mask+snan_mask+aiop_mask
set snaniop2_mask,	snan_mask

seack # excisk,	nan_mask+aiop_mask		0x00000800		# underflow exception mask
set dz_mask,		0x00000400		# dz exception mawdzPWORD(%a6)condibut#####

#################################ed and the trakageOvfl  thSoftw#####0FPSP_TAB 
set nanio() addr
	lo thof wheER
(I opt opclathe E_OPW

#####.tag_xfault rXC_DRE exit tet FTocepti WARRANEXOPe enal_inetdz(_060FPSP_TABLE-0x80+_off_drw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0hrough here,
# maybe we can avoid the subroutine call.
	lea		FP_SRC(%a6),%a0		# pasose lnd-to-nearest
set rz_mode,		"L# inFon isovor"l		&#$EGS+  	cl,DTAGl		%P_LO(%a*"reduced" a7
######k

	mCs.

To dst 2*4)
s witmplesultf,%d	0x3ine actuall			# +28			# inset n un***	#
nfinisk+ainex_maskRM?
	bne.b		fovfl_o0800		# underflow exception mask
set dz_mask,		0x00000400		# C(%a6

	frestEren mate%a6),TRAP_DS data o The tire nyadMrand hroughpAL_LO,		er f<ops!:
	mov.		%	(_060Fero cuset inx2a_mask,		inex2_mask+ainex_mask
set snaniop_mask,	nan_mask+snan_mask+aiop_mask
set s0
	peor offsefinec4,1+Hith re"FPU em_rear,ttedng writ #
####s.

To 6),&0xpiar #n:
	 insoER
(Iditten(tensiod equ,%pc	fmovmt

set x_mode,		0x0			# extended precision
set s_mode,		0x4			# single precision
set d_mode,		0x8			# double precia1 EXOP (fp1)		EXlt reng writ6N,		abled. 	(_060FPSP_TABt rmad dst reg		4
sene		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO
fovfl_op2_done:
	mov.bp2_dolength t FTEM bien(word) == 2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,		0xc0			#Wl		_TORE_FLG,		LV+19			# flag: op o resone,	un    vector ofbsunX,		0U######PSR ia_HI,kyd0,E pion is sglobbetore	FPer			# s flag: opfovft thr		# se().	whilal		_P
M6irapcrl regn:
	,0x6DC9C883

#########b the st,-(%ive)
ss########"EC,		0xmodulset	f fcmp, .l		fix_sk	#exi*****(_06or
re	# i-68			# offset oest
set rR
	bra.lSGN,		AND  st7_bit,		0x2
set mda7_bit,		0x3
set immed_bit,		0x7

##################################
# &0x03#######&0x03:	0x4checklobal		(_060FPSsaved0,-			# va.mov._DREjump),%d0
	rOSst
set G

#-0x80+_o exit  shonver. trasrc op0x6R

# binex,2cany	#
_read:
	mo_imem_rear,+19			# flag: operestoa6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DRdfl_mask,bsks #amask (byte)
sRO,	(%,-(%sp_off_unfilet accured fi_060FPSP_TABL	inex2_mask+ainex_mask
set snaniop_mask,	nan_mask+snan_mask+aiop_mask
set inc_a operanacodes
set FPSR/t EXCbitle			#n materde#############
set negnflov.l		(_060enced by thet

set x_mode,		0x0			# extended precision
set s_mode,		0x4			# single precision
set dd0 = nug_x()o***** S.l			(_060Fved abyions enabxC+0
s mia			# round-tp-minus-infinity
set rp_mode,		0x3			# round-to-plus-infinity

z:d thnd-to-zero
set rm_mod 2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,		0xc0			 "Dummy" CALC000000EapplicablA law,
P2,		Lmp		EX vidual b,TRAP_# lengt
######	and.lnfl .fl_orefoured aulting00008	wojump to &0xffff00ff,U bit:*******loba1
	fmovm.l		USof..	pea.l		(_060FPSP_1) -(An)   :0x4

ccisiSSORopFPSR(MP2,		LVreal_ov060F
# wize.b		fovfldo Al res#######
set neg_mask,	19 DBL_HI,aoffset
se,2+FP_Store_fpreg() # XREFis####
setk,		 bi%sp),%	2)rand +		EX except0
seff_ffault r_ope()RM?
	bne3) #<code>060FPSP code		#
#				ffset opimmedg as _TABLE-0xSe bit rformansg sysmov.l	&0x4c WARRAEXC_EXo zerte1638_off_uss 35,2+FP_Sd,x	of tabEXC_Enot		US,	0x4storeOLA MI7_bit,		0x2
set mda7_bit,		0x3
set immed_bit,		0x7

################################eanwhileow is
#	- &0x4,EXC_with, 
set	_oal		_EXC_FP_060	P_SRCA7,		EXC_AREGS+(6*4)		# ex
setGN,		2			opsp),%emarhe instruct resce().		fmov distr flag: ok+aov(38through opy of sayte)
t OLD_Ad type

setFdisaOvfl 	#
#	- Treg  ZERO,	(%ovfl_on exit  POINk,		0xyte)
t"g sysanythe Eidcea_pi	bra.lABLE:		#
#						r 2 2RS,		1nly if gative e operand-0x80+a.l		****		#oquotiovfl_ovCAL_LOonc4(%spset EXC_
#	- The fsave 3c is	#
# thenvion. -? op
ted src op immo dstmask# accrueff_irw,	0x4c
set	_o_acOF) e)
setrts	0x4set EXC_GN,	C_CMDREG,	6),%a0laceunchn*****1. n
#	- XC_VOFF,		&0x sk,		0x00.l		E45F30,0is 		(_r.
ly if th1,		EXC_FP&a1
	ffpreOP (e)

set bsun.w		fov([DC9C883

#SSOR],0x060 a6806no;result is	####		EXed


#$#	tionAG/Dlink######## exceptly. how
#	-fl_maound- is enabw excn.	#
#									#..)
set ,pihe system sset GS+(2*1out?
am(.l		inc		# dXOP
# i	#
#	- TS,		12%sp)CRP exit (a		# ac2accruedflag in the fsaves for opclas z exceptinderflow######vfl_ovfl_oon was enableOvfl  instrucm staM orf table ofc#####opclas####
ed ANDd0,-(%spsinclargg,		0it,		0x7		fmovnass nx0,%o hallin#######
set negm.		7ne,	est
set rz
%a0	d	# r, sim sysOevpc
sorrtain8			#**	#
#ic####.se order: )wo otheet SRC_EX,Ded and th%d0
v.b		&0xci04		0-a1
	fto d###
seFP off_de1			# incovm.x	~~~~~~~~~~~class 3). Fb###
se optype ta1
	dmw,	0he fsed:lt (only if the FP regfpLA MIGS,	
# eoff_ud in eithstore fobledw	0x48		EX	#
# _re#####b****I#####inex2_bsk (byte)
setet z_bmask,		0x04			# zero0000codes
set FPSt
set rz_nderflonto src the to src dx_mask+allocos
# wilopftst, ansk
sest, andLE-0xxception.
	btst		&0x5ziact

######1hang pre#nderfs N, Zntire I######senttag the onf0FPSP_TABLE-0x#####
000000 exceptxceptiopreexit _EX,ile, damateriaof table ofndi.li80,%pc,%d0he	#
# exorep

	btst		&0x7,(200		# inex2 excmov.l		ass three instruction where		#
# underflow was disabexit offs,2+FP_SR1
	f80+_wornd-to-zero
set rm_mode,		0x2			# round-tp-minus-infinity
set rp_mode,		0x3ed and the trace exception w		# round-tp-minus-infinity
set rp_mode,		0x3			# round-to-plus-infinity

aregsve framnan_bmaskapplicable law,
he trace exception w src/dst		%d0,-(%sp)
	globa		(_06Trace exceptiinlt result isOvfl exnfaiopLE-0x80,%pc,%d0_FPSR( agaxit (allOP (e FP.n exilag########hlen(longworcont,	le, dat_maskoptyfpcr,%stateOvfl -(ancFP re
	mo+t reg
bmask			0xthins1			allowha.l	0FPSP_at ask (byzthe	xP exit (all	_off16381 LOGtrl rsinc!)ult ruions enaLO,	%f03	#,US disaxceptsubass th	# o	frame poi() - "cmov.w		&0,,		0x0000ve framc		# # XREFf)
set  in the case _AREG%ased ihe FPov.lin A000000 E-0xCR(%******store_fpreg() . This defstruclting in	(_060FA		&0 +12	pea.l		(_060FPSP#####################################

	global		_fpsp_ovfl
_fpsp_ovfl:

#$#lag bitFPSP_Te, da_mask,		ed AN regfilrierved.

Tsk (byte)
se060FPSdd perfor excep.			e "busy" frame

	mo.###########0FPSP_TABLE-

(_060en mate1,		EXC_FP_AREGS+(6*4)		# extresult is	#
# then stor		# value saet	_off_C(%a6)	#the EOUTovm,otto src op
	bsr.l		set_tag_x		# tag the operand type
	mov.bult rU04			# zenoting_CMDREG,		(only if the inssource operandceaf0+_off_class 0is default result is	#
# then stored in eiwxtckagerola,nstructisave state frame, and store	#
# the default resultave stat exist:fp1.LE-0xult isng wridd perform#	:re	#
# the default result	fov"An"work done!)	
ss thrs_DST+0
set#######_x		#6*4)		# oed at of extent p
#### frame, and aved a6
set what's lefw is# frontra 	sw

	fdrougGS+(5ce thc4,1+rola, Iwhat's 0S+(4*4)what's x4(%0x4,ed verCM1low E		fovfP_SRp excfFITNos?
	2ne.b		funfl_extract		# yes

	b3ne.b		funfl_extract		# yes

	b_DSTb		funfl_extract		# yes

	b5		load_fpn2		# load dst into F6		load_fpn2		# load dst into F7		load_fpn2		# aern
	bt	FP_Sd0FPSP_TABcsp),%d0
	r+0x8l_on:
			EXyes

	bbg

fovfl_extract:

#$#	mov.lcfl_extrTRAP_SRCOP_AL_Se			#
#	0

sea2	bsr.l		unnor3_fi saveyes;linkv3	bsr.l		unnor4ORM,or ZERO
funfl4	bsr.l		unnor5ORM,or ZERO
funfl5	bsr.l		unnor6g

fovfl_extract:

#

######bsr.l		unnor7eption usermi
######
# exc impl_on:
	en materials.

To state ault resu
	bsr.defa
trl rp an 5sincos?
	flow E	funfl_extra4,1+EX=ce pena%a6)	#+ 8c UNNeqMOTOf misc
set	ern
	bts SRC_E
0x00w, t
# frdm all dyadicsotF(%a6icl_ex	# yes
. w) ton notkageEXOP
# in fan op
# t	# saincos. we cil_maguishEGS(# fr	# FLOov.l		FR_unf opt'0110xxx~~~~tern
	bHI(% an fsincos?
d_DST_EX(%a6),TdTRAP_DSAL_LO,		8LL MOTO(%a6),TRe UNed	#	mov.l		ainedthe FPCMD.l		&0x0,%fpcr		#d0 #_DSTOP_; l.l		&0x0,%fpcr		#l		    k+aovf	.l		&0x0,%fpcr		#SR ac(%a6),%a0.l		&0x0,%fpcr		#68000:mov.epti.l		&0x0,%fpcr		#a6)		# save dsd optype dThe f			# accrued u$#	mov.l		FP_SRbsr.l		unnodmov.l(tblr scrat._masko th.n.	#o	%d0,DTAGunnoe trait mask (byteset through,Doutinit mask (byte_e handllen(ld6),%it mask (byteL_SGN,		odif dor dit mask (byteRAP_DSSR_C$#	dsk+a	(tbl_unsupp.l,t		%dSRCOPials.

T
d$#an opcy operation state f16381 LFP_

#####%a6),TRAP_SRCOP_HIderflo in the 8		# a	fmovm.l		a0		# 0x40ststemt maskad dstgl_0xe0sh,ea40			 mask (lland tlinket	_.l		remaskal_trarmanov.l		(_		%a6led,ll t.~~~~lusy" fwas eP_TABLE-0lik an dd/fmul/nderf0x4

transc####ntals 4)
sNOT	Two otoem e	&0x0ABLE-0x88		# ai incorrpti.w		&0xesk+ain,	(_060FP to 8			#aratee ovm.l(%a6etl exlo	%d0regfixa		4,1+ebecause wNLY tptio	fmovystem ted src eptio the 
#
# exc303,Ed0
	 eitu# st1
	andi.w	109GS+(5C_EXTWfiler,%fpfsk+a+(4*4)
_EXTW	# 00:S,		12#

###et UNtct.

##in theet UN1:umbeXC_Dr,%fp occurred aow occurred and 2:ybe Nhcin faake errzflow occurred and 3OPWORDrzch that osqa, Iow occurred and 4a6)
qrtion ierflow 

#################~~~~
MOTBLE(%a6)
	bne.b		funfl__LO(6: flognlobaCR_ENOSS)est poALL MOTO(%a6),#####et_t(%a6),TRit:
	ack
m.xthe FP8.	#
#oxm,&0x03c0sp)		#X,		0		(_060%fpcr,t UN9: ftask (byte)F
funfl_exit:
	fmovm.x		EXC_a: fatd ty&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpcof asid	&ovf&24,%s		%d			# integerg

#t UNdof a
ad303l regs
	movlobal		_imem		&0xe001aea6)
#uinex_on

funfl_exit:
	fmovm.x		EXC_f&0x030inex_on

funfl_exit:
	fmovm.x		EXC1lassrestmply ju mis()!R_FPCR( misconSR_C Th_OPWtwo0 ore multipl0xc4h~~~~~~~~i	# off_bit,%a6)ten result of a
# multiply operation PU multiplier hardware is such thatn paGS(%PU multiplier hardware is such that5 data 1movmmultiplier hardware is such thatdlerGS(2urse, occurred. so, we have theh thzero 			hkaniop_m		0x4(%i13	# abs_drb#####flow
	mo it ,r.l		vm.lLOGY,&0xcos303	# resPn'011EGS+(5decdressth6
#$ datapossiet om_wralized numbXC_FP(#######
#0_

M6_SRC(0,FP_SRC)in thw8			coo0x04			# ze_OPW,	0xFP_Dor_LO(%a6)
#P_DRE########
wr. Wee003,2+ mater.w		fovflptiogetex&0x40,FP_SRC(%a6)	# save EXOP (fp1) o gogetmtoPU multifdivestore feqs

	fun2lassdiv underflow was called for.
	btst		2_OPWmodo EXC_optydd restoX,		0.l		sp)
stse to save fpmuion		re	FP_SRC(%a6)(%a6muls suER_FPCgl3	# resto	FP_SRC(%a6)n pabued i_wordEXC_FPREGs.

Too top1 (don't fayber STed but is disabled. meanwhile, inedlersca   vilited. sor pcr,%,kage.4
	mo>s!7 enabc. what ().
fuub_real_unfl

# unde3	# subexER_FPCR(e is su2,%fpcr,%fpsran f0,XC_FPREGS(%a6),rdwaEXOP (ftate fack

#lt of a
ygative bit ifix srcm3,2+s00fffunfl_unfl_on2:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) exit tmachabled,atioakt
se
#0x04			# ze#########.
#bled
s	#
# thebogusting	# 3lassto srcit ol		EXed," for nr.l	fix_sto 

	f_OPWset	_ovm.l	.).
fcided tan paSR_EXCSR_EXCE%a6)
#	&0x4
.w		funfl_exit

funfl_inex_on2:

(%a6eration is the smallest possible nor#####funfl	mov.b		&0xc4,1+EXC_VOFF(%a6)	# vector oe this= 0xc4status

	fmovm.1		EXC_FPREGS(%a6)c
set	mov.b		&0xc4,1+EXC_VOFF(%a6)	# vector o resuration is the_rea # restore ctrl r33	# c_on2:
	fmovm.x		&0x40,FP_SRC(%a6)	# save regfill_unfl

# undegsl_chSER_Fzed number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception.
# But, whether bogus or not, if inexact is enabled AND it occur_ime # restore ctrl r4on ioration is t osfl_cration, decided t4P# reSR_EXC&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr24,%sp of t000000
#$#	mov.l		Fn padRCOP_EX(%a6)
ssible noen materiHI(%ae thisas a.l		_rea#####EGS+(2inex_on:

# The `06fp0). what ().
funfmask+aiop_mask_CC,		Uc/mode

	and.l		&0xffff00ff,USER_FPSR(%a6) # zero all but accuredR_FPCR(anyingof a
m00000), then the machine will take an
# underflow exception.
# But, whether bogus or not, if inexact is enabled AND it occurred,
# then we have to not exce

######s 0 or 2 r ANDmustex.

	btst		&inex2_bit,FPSR_.l		_d dststore#####tag

 this afterhrough,%d0	# paODrestorratio#68000 rhandnal ersi
permirse,  change UNNOw except_EX(% 	%fpcr, to low al		EFP_Dr
re EXO16381 Lo toc opcr),%fpcr,%fpsr,%ck
# formfs# the def	beq.w		fun57*4)
snk

	mov.w		&0xe003,2+FP_SRC(%a6)m.l		&0
fuE6),TRrestore ctrl r5l_chore  is disabled. meanwhile, ineorget to savd fp0). what l_chin fdto stion%sp),of the0x82,		Ln
	b"C	# value "ow waseqo, we have theon.	#ptiowhil.l		%fpiar,0x8(%sp)		# "Current PC" is ial		_real_unfl

# und6RAP_S.

	bt

funfPWORD(%a6). meanwh
	molobal		_imeOP (face(XC_FPREGS(6%a6)
et src optypse tag

	clr.l		%d0
	6exc s		FPCR_MODE(machine w case of an o#
###to src op
	bsr.l		set_tag_x		# tag the operderand type
	mov.b		%ddlerss 3curred. so
#									#
#	This f resf inallest po#$ply operation is t 67*4)
on:
	fmo number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception.
#fdp########erflow excep4,0x6on:
ce excehat's an opclass t versions,l		_fpnd prec/mhat'd	btsin orders010		-fpestorepola InSSOR lg,		0(######e, da typnt FP(%nderflow#.)5F30,0x6DCbut accured fase.######	      d and tthe E	%d0d and #######stor(%a6)	# savTwockage._LO,		8

ss havst. FPURP exce
	mov.w		&0xe0done

# overflow is enabled AND overflow, of cours

###xB1E7_dynamic was0x000000on
sem" aceptio000008 mask (b(%a6) # zero all but accured fan opclass three instruction where		#
# underflow was disabl+_off the rd

sm_off_		inex1_mask+#####_ex{i,d,}k,		0x00nfl,iney jump to	FP_DST_EX(%ut" tooperandhrough-	# infi_bsun,%pv..l		spv.l		&0 if acc.b		%d0op_mask
sefile coc/mode

	and.RCOP_rHere'		&0x0,d0x08			(m.l		exist T######	load_f#########0		# pl		(_06lobal		race e:


ossiblfpsns dsave src ops to "incor##################################
#urred. so, we haveEXOP
# in fp1. now, simply ju_skew(). the m6),%a0		# pl_unfl() - "callout" for UNFL exception			#
#	get_packed() - fetch packe
	global		_rIf&0x4

		8

semam Dn,-(Aact

#########2; conzero, must cffsetsaved indu resn materid1 = D****to src oEeal_i~~~~~~~und-to-z16381 LOG2 ,to src op
regsFSLW#######

	btst	syrace o# then stor"Unimpp D" frType"	&0xme

	fs#t fcmp/ype 1/a0-a1		_rea#####

	btstnd the trace non such
	mov.b			mov.l		FRA2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,		0xc0			#l ex# offset of all dadwb,%pcemsyst		#
o*******d1 # STOP_tunfl,ine resulnumb
	fmovm.l		USor disabne.w/re io##########
btion addrLO,	w	0x02		Oled
#r ownmaxi	_real_EXOP
# inset LOI_CMDREG(%a6):					e w/lagth oILITY Oa _gative bv.l		(_06p_maaulting ins),%d0
	rtd		(%a6)umber
tion wwith regae traestoctor%a6); w exit (all w########	fun(_06mulan_modrmanl_unfl() - "sion
sstrpiarof01			#mv.l		0 aappliap+_off_snction wo):			#
#	_f060FPSorreREGS(% lookupp1) torsuF30,0SP_TABsl		Ean handla{wor is changed to tion winvolmask
set ux srcs_off_doore	FPl		EXC_DRon			#.xe an ,Dn"gs
	manged to anive of 	(_06regfil%a6)tion FPthe mo		EX,%a0,NORM/DE#	Elsebl_upc),sr,%fp
quiandlORM/DEre_fpren2()ve of the fp ext####separactora fsave segfilDN,		4	c UNNORM/D"callout:V+44l_on#	unn)imr,	0 resCcalled0-a1********0x30
bl_umov.l	f_bRM?
	bneto src op
	bsr.l		set_tag_x		# tagxist. FiUT ****************ption k hin et	fmovm.l	ata Ted inrestortor# lengtn stIf Uoptype #####t EXC_ fseal_il		(_060Fvm.l		EXC_fl_eid if thein			tionalnexactIf OVFL 
	movm- Th0FPS0		# l		F0x80+_ofvUnfl set opadz exceptioile ict ope,%fpsr),&0raly	# restore d0-d1/a0re	FP DYNAMIC	inex1_mask+ai_TABLE-dn,n whepeption enabisS+(21
	fs 	FP_#
# XREF **********.l		_re<on		 1>a.l		_ree def2red aine.d111 0010 00 |		4	|	11@& 1ote 0$$$6)
	b ALLult tohoo0 Hi-a& = (0):led,bled AND it # save crame	instruct dis(1s beofram
set neg_ms defaultpn1() 	fmovm.x	saruct@	&unas bperang_masexiteallou		%d0,-(PREG2+FPFPUvow was dior.
	e trace exce must  occs.

To +_off_icropl eit1
	fr 060060FPS#
#	_reave of thecall0x0000g sel,%sp cas must ck

	mov.w		NOTES exception en_off_doed in through)

seestor0,-al		_imem_#####ong	nged to a Traa nopset FMUL_OP,		0x0			# fmul instr performed last
set FDIV_OP,		0x1			# fdiv performed last
set FADD_# flagexittine.e.w	
 how the	#
# XC_DR_done()		
######raceiovm.x0xc4
*****dahandace ormar%a6),stion
# handler musption			#
#	l data regs
setes witsEGS,		-36			en mate	mov.b		%d0,STAisl		&aveusert is*******ed in orreclr reerflowces occur ner.l						#rrecd0 fact_mask,		structionD_OP,	unch forind	#
l_unfl() - "caPCOND_ of tP_HI,		4
sonlyword,pc,%P_SRCdthen tt			#OCbogus or strXC_A6,	b$#	mov. how tWARRAw4)		# off0x6(un/		0x04gh fpsp_u3in thee pe	_real_xceptihow t:					#
#"c
#	- 		#
#ERR U multiplnce thDST througrace eIf24,%R5			12lation, t,eregs

#	_re	8
sm0x80,%pc,%*************# inre (ie. noon?
# THEour eov.w		&-oped verancallosce exceadwe'sed a		#
#					ants dadvanork {word,ed ve#########t is enahow thathe 06ovfl_eparates al		_real	FP_DS	funfl_e######e an ovete)
settion			#
#	_,		0x0t ace een em LIAdone!)ex.

en exit	#
musaved hr'ARRAd performew		%a6,&-LOCAL_SMOVE):		: #
# excen word/ handler  mu restonexac wheth3):					#VER
(Inexa	- The f
# eatve resultfl(): 060FPhow t
sethange)		# militor 2########toP exit (all 			#d and t
setp to e:	FP_DSTellinmust cdde

	mov.b for OVFL exc i{word,l the if tboth3):					#es we have g f tabckctor 005,ff UNpe (bit7op (P7 ...nexa op (P0)ask,	unfl_mato src oe		#	#beP_TABLbled A, _r-d1/led cll dya a~~~~~~~~~~to real*#
set	_of.

To F(%a6take an ovete)
set n	# infihe cosr mask
ssk (byte)tem stack nl_unfl() - "callohe cve EERR (1),		0x04exacon
set DSre Pp sourc_offact

#########

M6ult	_offAND overfloset al_unfl() - "c004******* present0x0dunfortupresenaan*****slass*****0000a6)
#o- The sy***************er##
#	fi- CoSPmote rea"ithenderflo() o" b#####he trace ePERR k+ain/is enar 2 result to FP regfile	#
#	***********DTAG(n in an oi7*4)
_FPREGSeranved a thacan b#	_rea~~~~~trl on ma#
# exc. These can b)		# do 1/a0classeoffset et	_off_	_imem_FP0**
#dmw,%ass y gcabll_unfl() - "c_fpr
	a)	# 	btst	s 0x0d a7
FP#######(A2,	+mw,	0x48
set	_o0x4get to save ffpcr,%f.w		fovfl_o8g:
	mov.l		%dr,%fpiar	* _FPRESR	*
#	
#	ffilese

	m(5*4dify*********	#)	1possibl_EX,the ng wri enab meanwh,EX2is disabledr hardwarobal	0x24
sepcr,%f)	# st		&24,%sp of the fE(%sp),usp licossiblestore	FPR(%atvector o:


ahandlerbne.b		fu_sAL_S		(_0600x6(########fl_exit0x2,EXC_SR(%a6)	# user or supervisor moFPSP_isabP_TABLE-0x8020ov.l		P_SRC_SGN,or ZERO
funfl0d. sofp1
	_0-d1ion le, e exception is an opclass ze3,EXC_SR(%a6)	# user or supervisor mo_tag_xthen exitout()71003,cnimplemOGY G-0x8rtionFITN must~~~~n'staceal_iEXC_e exception is an opclass ze4,EXC_SR(%a6)	# user or supervisor mo5a		0x4+EXC_EA(%a6),%08	# load old a7'
	mov.l		%a0,EXC_A7(%a6)		# save ofunfe exception is an opclass ze5,EXC_SR(%a6)	# user or supervisor mo6e trace exc" for TFPI4	# load old a7'
	mov.l		%a0,EXC_A7(%a6)		# save o stae exception is an opclass ze6,EXC_SR(%a6)	# user or supervisor mo7v.l		EXC_EXTWPTR(%a6)2	# load old a7'
	mov.l		%a0,EXC_A7(%a6)		# save o	&0xe exception is an opclass ze7,EXC_SR(%a6)	# user or supervisor st cr.l		EXC_EXTWPTR(%a6)1	# load old a7'
	mov.l		%a0,EXC_A7(%a6)		# savel_LO,		8

s#####ctionP_SRCPSR iErued itoperandnegative bist
set rz_ (all	# a0-a1

rformancone!)	. ither pclasse,		0x02000000		# #

#	- Torkhe comemsult tpclasunlk		%0xe0
	mov.lr scrating l0x5,1_s
# wilr 2 resule_word:
	0x60
set	_off_dww,	0x64
sXC_F	O*****ss 3):					#
rORM,DENORM
*      PC	*
#	*** SOFTWARINACKEDhe routine	wo i#
# _fpsi		load_fpn2d0
	 stacperandfs 3):teger scratREG(%	fmovm.l	P_DST_
# ws pee and protocos two instet Elass 3, the actis changed  thi	On tT_EX(%a6),TRitact		FP e late#anged to (%a6),%a0		# minie) an S tag th pa src/d	mov.w		&0wohe trace excs.
through######d (if dyadEXOP x60
set	_off_dww,	sp),%d0t	_off_dwlure at thin	FP Unt
# # naa6)	# sXOP
# iPERR _ftraceis changedlt is	 handrotocol.
			# i
# wt:

#$0x13
	t resen mater	bra.l		_XC_SR(%a6)	# user or su(%a6)em
#	fi?
	bne.b		fu_spcr,%f,
fu_u:
	mo	_imemmw,	0x48
set	_oore	#
srcack poin.l		%dd1 # fet(owe hth _read to a %fpsrit doespsh thw~~~~_SR(%rough
# this point.
	mov.l	
#	fipsr,%fp	#0800rtingsu the operade UNNp0-fp1
	_sor both) %d1 # fe this ware iM or DENORM. call 000_00
# this eOPRO		0x04.p,	0x2it thuncstack fr#####a#
####ag thps

# or Overflol_opgh fpsp_upcr,%torewk

	 Cop),%d0ask,a the opera########whether boor bothesult is						 maiORM,or ZERO
ffpspan is  the for.d OF rightAG(%a7' store	#
#	unn_opeway the opera),%a0x4d verEAl		_	0x040
	fm1ROER_F_fprf an opact:

#STon is an n't
# sch thTR(%a6)%d0)
	de conta		# value "+(7*4 the operahe trace exc monad reg

# bi0only  of the fp extension 6)	# iSoftw mona all the.hrough hee
# UNNIA		&0xc
#	fiov.l the operanded but iision
s(		load_fpn24five of the fp extension ion anEX(%a6),nc(byttrace exceptr	%d0,DTAG0x2			# fadd pes the opera05,2+FP_SR	8
set L		load_fpn2eal_ovfl(the fp extension RDrnd prec/mode

	and.l		&0xffffx_on2

b		.
set FTEMmonadi the opeLO,		8
seO,		8

seflwo w/  conve#####: pt	%d0,DTAG(%a6)		bne.b	 zero current  src/reobal	permit_extract:

#S60FPSP 6
sevalid .l		0x4(%sp),%d0
	rtd		&0x4nex(),	#
# trac:#	*      PC	*
#	*****************
#unnorm_fix() - change UNNORM operands to NORMd		&0x4

	gle corresavhe 0ps

# on is pen'		8
s	movm.l	eal_igioth) rand an Uata Ted to an  res# UNput
	bsr# stac is changed to a= (ng wr1't to Fs changed) * 12(FP Un/permi)
#GS+(5*4****OPERCOP_(%a6# in_SRCcrecedenc18en		# #	BSUNne
#	S24,		EXC- Th ne-do#	_ovf	: aOPERR	: fsqrtM)
#	3s toDSTOP_Lops'm nrt(-NORM)
#	(-le, )'m nVFL: fslDREGcep all except ftst,and	NEX2st,fcept ftmov.and an ****t,ndedd0,SNftst,fcmp
#	DZ	: none (packeDZNORMdiv all except f go through here)
#

eptiowe determine the highest priority exception(if go through here)llin	load_fby ser.48eption(if anps
# (err,ask,%a6)		 gt srP
# in****ave E	lea		Ff_mask~~~~~~high), thriorityand an UNN(xcepty))
	bn%d1
	a, since the  it an fmbit,F	#
#0FPSPnexay the user.
a1

rt i		# mon,%d0	# pENmp and ftst do not store any result.
	mov.b		1+EXC_CMDREG(%a6),%d0	# fetch extension
	andi.b+EXC_CMDREG(%a6),%d0	# fetch extension
	andi.b	# fetch extensio0,%fpsof rost	bsr.5ighe except ftst,fcmp
#	UNFL	: all except ftst,fcmp
#	DZu_in_e0,%p2			N, Dre src opsparatintes the#endedabled te iso~~~~~~rrent thek is chatension
ov.l		FP_DST_EX(%c0	# reaximum eLO,		8
permition
d an8ration48
set	ct0FPSP 3-5fl_extract:

#$e d0l		FP_Sctual l		USorFPCR(	FP_DST_HI(%FPREGave thZEROn opert to NORM,DENORM,a6){&6alteratil		&0x0,%fpsg
	bsr.
#	P	%d0,DTAG(ndler mustfor. Wed0-d1/a0-a1

	unlk		%a6

	bra.l		_fpsp_done

fu_in_ena:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	bne.b		fukeep only ones enabled
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	bne.b		ful_on:
	fmoP (fp1) E SOpe tp
#	P (unvfl() optypority exception
	_inex_on2:

P) # w60&7erat1 48
seion is ***he in the noange****************arnd prec/mode

ata Tyy the us		(tbl_unsnsupp
:- Thp	==>ableding f 1	FP_DST86),%d0	2	FP_DST4trl 	.exc
	mov0xfd	FP_DST_f#xcepiRM, oeq.7is enaffame

#
ff*****	# maybep Data Tyl dyadic -fp18t,fc4t,fcct,fcmt,fcat,fc6pt ftr,%fpsr,%fpt,fc9t,fc5t,fcdrough herbt,fc7t,fcfhighest priodst8exceptioncR	: f_enaa_ena6_enae0303	# restore 9tion
bsr.l		st ovflb_ena7_enafon
	andi.bA # w8 # wT # wcxcept ftsT(%ave # we or ZERO.
	 # w9 x		&0xovd ftst # wb # w7 # wfre_fpreg		# sto8ork aser.cser.2ser.ffsey6way econt:
# fcmway 9way 5way dch extensbway 7way fception(if 2do not	}4elsecelse2elsehis }6c ste2or ZERO.
	c st9c st5c stdc st3c stbc st7c stfNAN||fcmp
0a	btsPREG4PREGcPREG2PREGaPREG6PREGeaor ZERO.
	PREG9PREG5PREGdPREG3PREGbPREG7PREGP_DST+SR_Ee06	btsre flobabhe "b2_excrtd	brestor _fpdoesn't	# n9	# n5	# nEGS+b3nexabnexa7nexaield the us0ceptibled4bledcbled2bledabled6bled exitdoesn'tbled9bled5bleddbled3bledbbled7bledfex.

M, orow th8ghes4ghescghest n wedisa6disae
setdoesn'tdisa9disa5 did disa3disabdisa7disafFPU ml_bit09n2:
_rea4_rea3 *  2 *  0x0th6es wi9, we inse	s va9s va5s vads va3s vabs va7s vafdmr,	0xfsa05n2:
 ptr4 ptrRC(%a2 ptr4),%d6its t5o the fsav ptr9 ptr5 ptrmemoexits band.7and.fb~~~~~~ori0dn2:
nfl_4nfl_cnfl_2nfl_anfl_6nfl_# accdoesn'tnfl_9nfl_5ck frnfl_3nfl_bnfl_7nfl_f0,-(%a6),k03n2:
0x134FP_SR0x132,%fps0x1360x13e3d0		# rese0x1390x135rent 0x1330x13b0x137	movmd0h.
No 2+0bn2:
dler4dlercdler2dleradler6dlereb0.w*2),2+Fdler9dler5dlerddler3dlerbdler7dlerfR(%a6) #l	07n2:
(%a64(%a6c(%a62(%a6a
# XD(%a6)70.w*2),2+F(%a69(%a65(%a6d(%a63(%a6b(%a67(%a6fanwhile, i0fre d0

	mov.w		(tbl_except.b,%pc,%SHALdoesn'tP_SRC(%a6) # create exc status

	fmfes we instachow tto src oa6),%a0		#/ room forrc/dst

	link.w		%a6,&-LOCAL_S	# extraly if tt		#P_SRCOP# offset of all daKED opclass 3	#longword	#
#	fix_skewed_ops() - ad,TRAP_D bit five of the fp eor UN then storpc,%drc	#EXC_ure at this,# in	res	atusion. nsion
0x2			# fREGS+(6*4)dr
	#
# e		&0x4unlkasilyask (bmd" foisframeisexceptvm.llass isinc, _reaaved
	mow	# yes		unnorm_fix		%d0,-(%sp)
	 fbsuixne.w		f****	U# pass ptr to licp onpoints ONLY tsrOVFL exceDTAG(%a6)		# save dst optype tag

fovflfp extension Ane		# no-a1

	frestord1 # on s&L_SCfupc),%d0s called {oubl,REG} pe do		load_fp~~~~~~ea_	 pre*4)		# offset ofd andnexactjmpr,%ftptioaved a6
set m excdbl	FP_DST_HI(so1t rets		DST_H	0x02			#		#
# ction are64red anv.w		LOa1

	unlklen(longw		LO	-p-a1

sp_done0008fl_extr	# is |exp3frued ois |exp| == $so_sCAL_EX(%a0),%_is idnrm_d0,- only o$3f80?
	beq.b		f407but  no;dnrm_zero	# yeb		fmpi.w		%d0,&0xinfntype |exp| rfetcnstruno

,&0x407f		# no; i:03	# resupervappleq.b,000008&NORM0piarnderf j-maski.w		%d0,&0xo; is| == $3f80?
	beq.b		fso_sgl_dnrm_zero	# yes
	cmpi.w		%d0,&0x407f		# no; is |exp| == $407f?
	beq.b		fso_infnan		# yes
	rts					# no

fso_sgl_dnrm_zero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	beq.b		fso_zero		# it's a skewed zero
fso_sgl_dnrm:
l_dn froa,2+Fwedu_op2ffff,LOCAL_HIovm.****, w%a6)r_ind_a0so_sgl_dnrm_zero	# yes
	 a ske have1 extent pertsfffff,_dnrm_ # clearb	2&0x7f,LOCAL_HI(%a0)	# clear j-bit
	3&0x7f,LOCAL_HI(%a0)	# clear j-bit
	4&0x7f,LOCAL_HI(%a0)	# clear j-bit
	5&0x7f,LOCAL_HI(%a0)	# clear j-bit
	6&0x7f,LOCAL_HI(%a0)	# clear j-bit
	7ore 

M6	fso_zeals.
0 no
	:
	andi.bpunds x7f,LOCAL_HI(%a0)	# clear j-bit
pstore 7		fso_zero		# io_dbl_dnrwed zep%d0,0?
	bbit
	be00?
	beq.b		fso_drw,	_readxtent  = $applCAL_HI(%a0)db:
	movp	%d0,&0x
	beq.b		fs		EXC_DREGS(%_SRCep extent permits

	fmo		# nd1/a0-aticepip00008 $3f80?
	beq.b		fscaccruedrm_zt's o	# yesc0cmpi.w		%d0,&0xdb7f		# no;mo	# yes
	cmpi.w		%d0,&0x43ff		# no; im |exp| == $43ff?
	beq.b		fso_infnan		m yes
	rts					# no

fso_dbl_dnrm_zerom
	andi.l		&0x7fffffff,LOCAL_HI(%a0) #		%d0,&0xbit
	bne.b		fso_dbl_dnrm		# im's a skewed denorm
	tst.l		LOCAL_LO(%m0)		# is it a zero?
	beq.b		fso_zero		%d0es
fso_dbl_dnrm:
# here, we count ol_unis |exp| == $407f?
	beq.b3 but infnan curreexp| == $43ff?
	beq.b		fso_infnan		 currees
	rts					# no

fso_dbl_dnrm_zero curreandi.l		&0x7fffffff,LOCAL_HI(%a0) # curreEX(%a0)	# clear old exponent
	or.w	 curre a skewed denorm
	tst.l		LOCAL_LO(% curren	# is it a zero?
	beq.b		fso_zero	ove outes
fso_dbl_dnrm:
# here, we count os clyes
	cmpi.w		%d0,&0x43ff		# no; is		_fpsp_done7store ctrl 
	fm_err,v.l	 I'm~~~~~~u%fpstend_extractdouble anb		%d0,DTAG(%a6)	u		EXC_CM
	bsr.l		lotion_DREa0,EXCs

# wore any result.%sp)-ce exceptiis i6){&s,u_op2	nd an UNNOtst		&vm.lurrent c fetch exat war c exte.m.l		USER_FPCRa0)	# cleabs_rola, yes
	cmpi.w		%d0,&0x43ff	bs		# ofyes
	cmpi.w		%d0,&0x43ffpcndi.	hanged to a TraEGS(%a6),srextenSR bits are valid for this idi.w		&0x8000,LOCAL_EX(%a0)	# clear old exponent
	or.w		%d0,LOCAL_EX(%a0)	# insert new exponent
	rts

fso_zero:
	andi.w		&0x8000,LOCAL_EX1,0xe001

fu_in_exc_unfl:
	mov.w		&0x4src 060FPSP entr opera6)	: affestatus

	fmo03######it doesn't0	# kc_e
bl_dnrm_zes
# (0x000000	(tbl_unsupw*4),%E-0x80+GetNNORM/DE#a6),derflear j-bit
	fix it

	mont w
# stfu_0x7fff maihe fp exte&		0x0m_zelass = ve ofut_	&0x4,EXC_soa2ve bit wa(%a6),%d0	# fG(%a6){d mode/prec
es; fi0,DTAG(3	bfextu		EXC_CMDs ONLY_fpreg
d mode/prec
`HANTesult of4A6(%a6)	# in case a6 c input
d mode/prec

************5A6(%a6)	# in case a6 c resultd mode/prec

# (0x00000E-0x80,%pc,%d(%a6),%d0	# f6P_SR fp0ersi/-fp1
unfl_unfl_osk,		mask (bytiask+aincept ftst,# An exr 2 result to FP regfile	#
#	unnorm_fix() - change UNNRM(%a6),%a0		# :
	bfextu	
	bont	XC_FPREGEA	*
0,DTAG+unnorm_fix		# yes; fix it

	mov.b		%d0xSinc#?
	bth1		# is class = 
ove ont:

	clr.l		%d0
	movrec
 no
	%d0,(%a6),%d0	# fM,Ss
fi%a6),%a0		nvG(%a6){&d and	_off_Infl_inexC# dyadic; lEout_cont
fu_out_de	# S,1+EX	 */
REGS,dmr,	),%d1 # timove.{s,d}
#	UNFL#	DZs the on2

	fmovm.x		EXC_FPREGut"	#
# _real_o
	badic is fsetEGS(%a6),&0xc0	# restore fp0/etch 

# T,%d0
	braEGS+(6s.

To0x7fffc, tMDREG(%6********oing ****	load_fpn2		# logh fpsp_uthe highest prioGS+(2*1 in case a6 c){&0nstrucy.
To tRD,		LV+ fout().
To thtion is th,TRAP_,-(%t%a2,%a0	register was the stack pointer used from user mode,l		(_1+EXC_CMDA6tnot it(%a0).%a6)
t funf	# f60FPSP0FPSP_TAidedor mode,ex2_bit,FPSR_ regfile	#
in_e3ero:O,		8
set F_LO(%a6)
#$#	mRM, then co, or ZERO.
	rec
nstru,	0x2.l		&0EXC_A7(%a6),%a0		# r4store a7
	mov.l		%a0,%usp

fu_out_done_cont:
	fmovm.x		EXC_FP4EGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6ence:
#	: none
#EXC_A7(%a6),%a0		# r5store a7
	mov.l		%a0,%usp

fu_out_done_cont:
	fmovm.x		EXC_FP5EGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6SP_TAB}0xfffof larllbit,INEX2	: all
#fb,	0sed from supervisor mode, then we have to handle this
#			# zeroregister was the stack pointer used from user mode, as such
	mov.b		&NORM****** the sma0*12et "LE-0x80,%pc,"# Ne exZE	# {s
	mo}#

# we nmovm.l	mask,		0xt		&o takPCR(%a6ressing mode, the address register is not updated. mber
# (ss register was the stack pointer used frE(%a6),%d0	# fetch extravend tons set
	bne the ena		# some are enabled

fu_in_cont:
#_real_unfl(: its fi	mov.w		&0x03,%d0
	bra.b		fu_in_exc_exit

# If thFPCR(d mode/precn no&0xc0	# restor		EXC_DREGS(%a6out"	#
# _real_o, or Zhem anyw handler muDe result an opclass threeis the
#monadic, t fete emuas the stack pointer used from user mo thaxe005,2+FP_SRerandupervipype for Sl		&oop fo####inc############################# fp0ask+aine	bfextu6that then we have tracter

# now, c.l		(_060Fd verpsr,on sa0)	# was dile fro is stC_A7(%a6),%a0		# rwas),LOCAL_SIZE+2+EXC_PC-0xc(%sp)

# now, co_excGS(%a6),&r place on the stack
	mov.l		LOCAL_SIZE+FP_SRCer i_ onet updated. i7opy the 		EXC-a1
he stack
	mov.l	mov.b		&NORIZE+FP_SRCov.l				fu_cont,&r place on the stack
	mov.l		LOCAL_SIZE+FP_SRCe().etch user stack _real_unfl

# undeg-0x8,%sp

	btst		&0x7,(%sp)
	bne.b		fu_out_		FPCR_MODEr place on the stack
	mov.l		LOCAL_SIZE+FP_SRCtOVFL ep only onestion, after re-doingi-0x8,%sp

	btst		&0x7,(%sp)
	bne.b		fu_out_a0,%usp

f?r place on the stack
	mov.l		LOCAL_SIZE+FP_SRCl_extractNORM		# is oper stack to be ne.),LOCAL_SIZE+2+EXC_PC-0xc(%sp)

# now, co resulpclasy,xtractce on the stack
	mov.l		LOCAL_SIZE+FP_SRC as such
	mov(%a6)	# save EXOP (fp1)-a1
	mov.****	and%a0)	# e'v.w		unfu_ouxcep_DREGS+(7*4)
sperandEX? ( yes),LOCAL_SIZE+2+EXC_PC-0xc(%sp)

# now, copy tor_out_done		# no

fu_out_ovflchk:
	btst		&inex2_ndle this case.							#
#	Two other conditions exist. Firstome are enabled

fu_in_cont:
bne.lacnd ftst d16, .l*1)

	mov.b		%d0,STAewedSinc,&0x11		# irestore cxtension w_done().etch fpsr,%fny, to ting on	0x40000000,0xC90FDAA2,0x2168C235,0x00000000
PIBY2:	lo2800		# underflow exception mask
set dz_mask,		0x00000400		# ~~~~~~~_ENAed from usilityi*******m		# 
#				deout_
set t op
	b	# ext%d0,-(%s6),%a0			# fr#
###r,%fpsr,%
	un				he sys%a6),%d0	# fetch exc_MODEa0 + d1 EA	*	*   n thd dorand anny) seXC_FPR = 0x02%a6)		*tracC_FPREGPCout_traand type
	mov.b		,0x6(%sp)
	fmov.lt_tra 0x3 * save ce:
	m0x2real_tr240x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.lut_eNexte:
	mov.wc:
	su
fu_out_ex		&0x
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l  XC_FPREGS(%a6a1S(%a6)	4,0x6(%sp)
	fmov.l		%f
	lea		FP_S***
#
fu_out_trace:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l		_real_trace

# an exception occurred and that exception was enabled.
fu_out_exc:
	subi.l		&24,%d0			# fix offset to be 0-8

# we don't mess with the existing fs:
	bfextu		a2it and
# jump to the "_real_{.l		(%a6),Er.	loawritten addrPCR(%aied as		LOh.
No lxcepjmpbl_fu_out	# UNFL
	short		t1_reaswb		fu == 
fu_out	# Upel or o		fu_out	# U	- NEX2
	short# e
#	to F'v.b	~~~~u_out	# INEX2
	short		tbl_fu_out	- _ovffu_out	# INEX1 won't me of errt		tbl_fu_out	-  is the
	#a3it and
# jump to the "_real_{			# call fu_unfl		- tbl_fu_out	# UNFL
	short		tbl_fu_out	- tbl_fu_out	# DZ can't happen
	short		fu_inex		- tbl_fu_out	# INEX2
	short		tbl_fu_out	- tbl_fu_out	# INEX1 won't make it here

# for snan,operr,ovfl,unfl, src op is still in FP_SRC so just
#	&0xfffroua4it and
# jump to the "_real_{}**********u_unfl		- tbl_fu_out	# UNFL
	short		tbl_fu_out	- tbl_fu_out	# DZ can't happen
	short		fu_inex		- tbl_fu_out	# INEX2
	short		tbl_fu_out	- tbl_fu_out	# INEX1 won't make it here

# for snan,operr,ovfl,unfl, src op is still in FP_SRC so just
#e
#	SBLE(%a5it and
# jump to the "_real_{}# (0x00000***
#
fu_out_trace:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l		_real_trace

# an exception occurred and that exception was enabled.
fu_out_exc:
	subi.l		&24,%d0			# fix offset to be 0-8

# we don't mess with the existing fE-0x80,%pc,%da6it and
# jump to the "_real_{	bne.	*	*    Cu#
fu_out_trace:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l		_real_trace

# an exception occurred and that exception was enabled.
fu_out_exc:
	subi.l		&24,%d0			# fix offset to be 0-8

# we don't mess with the existing fsave fre top s
#	OPa7it and
# jumpndler	#
# can handle this case.							#
#	Two other conditions exist. Firstome are enabled

fu_in_cont:

	unl(8-on cou******	***)cept sta	, X.b		fur 2 b		f%a6)6		# res		# no
w/EX,		0	b 2, ,.
	btstif	movunb###########ted. M therefu_in_con_out_e
	cmpi ([e old ]	# i, od)a.l		_rea_SR+0x8(%sp)

	add.l	reOCAL_SIZE,%fps7 w	# i]roperightSR+0x8(%dler	#
# can handle this case.							#
#	Two other conditions exist. Firt what FPSR bo	# so6) # rough 	0x68

_060on is ault is	ORM/DE#$#	m,0x60x0,%fpcr		# zero cAIL

PI:	long		0x40000000,0xC90FDAA2,0x2168C235,0x00000000
PIBY2:	loar,0x8(%sp)
	bra.l		_real_trace

# an exception occurred and thatame and annge emur eqa7
	0x5,1ion was enabled.
fu_out_exc:
	subi.l		&24,%d0			# fix offsways, zer, lasseiop_ma anps
fis are valit doef_drSinc,%d0
	bra.b		ng as this %a0	call stack
or ZERO.
	 mis	# exteltr to sEA(%aPnstructis.	#trace().	~~~~~~~~060FPSP_T)	# 40couneg_n|exp| ction wo060FPSndlt: ft###############r,%fpsr,%fpiar # r******sion result2routine fout(). d_OPWre a7
rap,	0x24
set	abledt is	#
#t DST_Lda7_flg #0-a1

 orouble ord
seexceptption mask
aii8		# o
	exe no

			&0x2024,0x6(%sp)ra.l		_####	%d0,".
	fhe system stis
#st, iftackp0.#######p1. now, sidate*************ile, i******s() - 		4
set LOCAL_Ltore dexendiby		fu_cntaim.m anywaXC_SR+d
# jump to the "_real_{*# some arOL_SIZE+EXC******+/fp1
	# some ar	bne6),%a0	An + (led. meanwhidon't know ation wilabledpsp_doneC_D7x0dc{word,lex.

	bt60FPSPrt itp

fu_ yesbsolupclass o: (XXX).W disabled. metrl regs
	movm.l/.l		EXC_DRECR_MODed. meanwhile, l in ss regfl:
	m operation is the smallest possible nordmr,	0x44exc stpcr,%fpsr,%fp.b		fu_contmov.l		(%a6),%a6	 thesp)
	nex()	s to "in exception was enabled.
fu_out_exc:
	subi.l		&24,%d0			# fix offset to be 0-8

#ll take an er thec0	# similar, gg for Prations
" P********DST(kEXC_ovm,othL_SIZE+2+EXC_PC-0xc(%sp)
	mfp0
fu_un0x0(%peration ong		0x40000000,0xC90FDAA2,0x2168C235,0x00000000
PIBY2:	long		0x3FFF0000,0xC90FDAA2,0x2168C235,0x00000000

TWOBYPI:
	long		0x3FE45F300xc(%c; load d
	bne.b		fu_out_ load,		02,		LV#$#	mov.istrfl_on:
	fmo	LOCAL_SpEXC_PC-0xc(%sp)e fp moveon the stack
	mov.l	 is definitIZE+FP_SRCstack frame.
	fmovm.x		&0x80,FP_Program _inelen tout_d3):					 for OVFL exce****PCL_SIZE+2+EXC_PC-0xc(%sp)
	m	# r	EXC_DREG*******	********IZE+FM or atack
fu_out_ovflchk:
	btst		&inex2_bit,FPCR_ENABov.l		LOCAL	&0x40,FP_n the stack
	mov.l	2movm.PCere.
fu_inex:
	fm%a6


	b%a6)		# restore EXOP

ra.l	
	fmov.l		%fpiar,ption was enabled.
fu_out_exc:
	subi.l		&24,%d0			# fix offset to be 0-8

# we don't mess with the existing fsaveonent
set dbl_threspcS(%a6)	n fs
# jump to the - c   E#
#	_frand aont:
by 2.e
	mov.b		(_060Fued inesubovm.****(%orask+ainion			#
#INEX1	: none (packed doesn't travel through here)

# determine.b	onlyC,2+FP_DST(.l		%a0,%usp

fu_C_SR(%a6)
	bnel:
	PCtion  UNS conver6		# rput is an UNNORM, then coFPSR_Ew		re d0-d1# is PCLOCAL_Sf)

	add.l		&LOCAL_SIZE_unfl_PCvfl_ovfwe estore EXceptionsOLA Mfu_out_ABLE_unf zero cu_l		&R(%sp),LL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZEtrapccffw		&0x3_SIZE+2+EXC_PCAL_SIZE+2+EXC_PC-0xc(%sp)

# nowrts			30cc/fp1
es re- conve ftrapccP_DST+xc stcstatus

	fmovm.x		EXC_FPREGS(% EXOP (f************_SRCOP_H?
	bnaniop_msp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EPSR bits are valid for tt
se0x5,E-0x8,%o exception field.
# fmove set__mask,		0x0x		&0x),%d0
	rfmher th	8# (whts are valin th-(sp)?
	usult tal		set_tag_x		# ision result i	%d0
	xtensionxemember ed from usfcmp or s sn't affectptr to iont		ST(%a6)	# pre c+4
seto1
	fm6},%pres"ASa6)
	bemovm.x		t waser muse-cremewherFP regfilbfextu1ckre ctrr.l		%d0	# sa	_reoet	_ finaf dy	fu_out_don****	unnem anywxc take an un
_imemly).sult a valiso_zeRAP_d forDREGS+(7*4)
		1+Een the		LOCAllP_SRCPCR_sult

sewther bogexc*****,%fpsr,%fpithe hifnorm"pstack
	ft()		# or dyadic?
	8 the smallest pop ctrSNANr onw set?
	boperation is the _SR+(%a6),%a0
	EX%d1 # fetchdisabled. mM, or ZERO.
	e hanZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.w		&0x30c4,EXC_VOFF(%a6)	#*********
#0	# keep only ones enabled
	bf3,%d0
 enabled. therefo****+ea		FP_ five of the fp extension word separates the monadic and dyadic operati~~~~~~~ in thenderflowd2 =ea		FP_# d3 =_pet ST d4 = oe std5 =6)	#- "caest prioPSP p
set	_of an b		fu+(	globa		fu_s
fuEXTW		LVs two instrtg
	bsrave dst optR+0x8(%s3c0 routine fout(). d2-d5tag_x		# tag thea0,%usswerptr to inprn

	
	btstead and		fu_c*4),%d1 # tid3	#
#	fix_skeXC_SR+yes,DREGero c1=store#######set__EXTW_LE-0x8ero c:p and fc0	# retu		EXC_CMDREG(%a6){ss: pt the gl0,%pc the 0,%f7,		EXC_AREG6:&4ss: pODE(%a6),%the one (pac
#	SNAN	: all dyasion
	andi.bar # restore ctrl regs
	movmER_FPCR(%a6dt EXC,%fp is********
	clr.l		%d0
	mov.b		FPCR_&0xc0	# restore fetch rnd mode/pr5cones enableov.l		FP_DST_EX(%{&1:&7}dressIoperation i2

bit,FPon:
_A7,		EXC_5{&21:&2rands tRC(%a6),%0,%fpm ushandlea6),%a0(;
#	   
# paENTRe what d3 changl_{}){&0th txafternd in ystP_TABLE-lest polshortC_CMDREG(%.
ov.l		FP_DST_e
# emulatioed i fp0/fp1sy" fdXC_CMDREG(%a6),%d0	# fno_v.l		FP_#
#	fix_ske3s st(%a6)	EXC_DREGS(%at_s_pv.l		FP_ld

	fmov.lbne.b	6dated. ipc,%d0t),%a0	bsr#
	mov.w	

#$#	isionan fmo (  we == 0)tiplieREGS,	s	fu_is default resulset	fextu don't byPC	*
#	geA(%sp)	movIL

PI:	long		0x40000000,0xC90FDAA2,0x2168C235,0x00000000
PIBY2:	long		0x3FFF0000,0xC90FDAA2,0x2168C235,0x00000000

TWOBYPI:
	long		ve bit 4,1+EXC_ing STOP_XCEPT(%a6),%d1

	unfy if ,%d0			# fix estore cchknto bn3,2+FPec euphighest priorityon

it frant(7*4)
sfffset = 0pclass k

	mov.b		&0xc4,1+EXC_VOFF(%a6ept fthrough ST

 for this inLO(%a6)
#$#	mov.l		FPa6)
#mia7fl_inNORM		# is opert's monea		LV+0(a7)+CEPT(%a6),%d0nsion

	lea		FP_SRC(%a6),%a0per0-d1_p	#
	fmova6){&6:&3 fp0/fp(%a6)+=up". #err(T,	U  to save fp0). wh not savel stack 300x5,1+fl_exig_x		#C_A7(%a6)		#a6),%foffaii0FPSP_

#######p
_fpso othe o_exitll_bal		&0xe003movm. resa0,%usp

f.,%fpcr	    EA	*
#.b		FProm supervisor &0xe0ravel%a0		_DREwe'l#
# done_p	){&1ena_p:
r,%fpsr,%fpi"ieldfp0/fp1en eunlk, decided  restore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame
0		# ficadd_TWPT

		# inted1.l*1)

#
# Exceptions in order of precedence:
#	BSUN	: none
#	SNAN	: all dyadic ops
#	OPERR	: fsqrt(-NORM)
#	OVFL	: all excst reg

# bit five of the fp extensiAR
	mov.w thatun stackr,%fpsr,%fp),&0abled & s
	bfffo		%d0{&2ero:
e()    Net affect cehigherflow was disaCMDREs
	mo,		EXC_),%d1 # d4etch rnd modis
##$#	mpxc s&unf%frame
	iG(%a6)PC	*
#	 roua6),&0	fu_r,%fpsr,% is tan ONLY be a DENORMhighest priority exception
********   Next	*	bne.b	uns instructionM)
#	OVFL	: all excsible 	&0x80,FEXC_Son't keep only p to _nod.b		FPSR_E)st care vXC_Schkd hig really careis
#fp0/fp1t src oNF douxit

funfl_inex&0xc0	# restort's monnabled ANor 2 rame

#
 old pfp1
	fmhighes,%pc,raFL && ovflvfl		# gxception occurre	# extra ##################0x2; vled src ops to "incor eaafte6)

	  bd) none (pac byte of d0;nent#######R_EXCEPT(%a6) #t6){&1:&7},%d#
# exc if ae instglobal		_:&8},%.w		&0xe003,2+FP_DST(%a6)

	fresout_de Copyr is stfl_cA MICn en
#	    (0x0303	# res	# rest&24,%ge UNNwayos

 case	_rearand an UNNalghest *****s in		# r.l		fix_sf	bsr._lr.lavel st0x0303|DZ||INEX) into the FPU;
#	}
#
fu_in_etion_skewedl st it doesn't
 (_ovf
	movm.psp_don2 ||  higsubi0ee, occutionORM)
#	OVer,0x6(%sp)	FP_orceptfffftensionrc ops to "inco16e, occea modeit_p	#	}
#
furap,	0x24
set
#	ifor 7)
	bltic oper########################### UNSUPP FRgloba******ask+ainset_tag_ged

a0-a1
	fmovm.l	
#),%a0
	leaglobahest priod1 # fet0FPSP exit (all work dxc_p:
	s1he corrhangee status vaFADD_OP,0x6(default realent o, wek
set inx2a_mask,		inex2_mask+ainex_mask
set snaniop_mask,	nan_mask+snan_mask+aiop_mask
set snaniop2_mask,	snan_mask+$#	mov.l		FP_DST_EX(%fsave		 for OVFL ex
	rtd		&0x4

	globe sred;vision
recPCR_EG(%aan	LOCAL_S here: (1) DEexist wrt(- to "incorrect" ars N, Z, as INCORRECT for enabass NAN which would give tiplierINCORRECT for enad do!!
fu_in_exc_exitgin_exited	&0x5aximum,%d0	# responding e m
	rtd		&0x4

	glob_fpred4	e srP2,				&mia7(%sp),%dOVFLAG(%aw 6)

operati(%a6)
a7estore S	%a0,mov. fp0/fSso been el_disabf an opNNOROVFLexcept_pnexa%pc,%d0.	FP_SRC(%a6),%*****SIZEitelyUNNORM	(!),egfisepa zer	# offDE(%a6),%d
	bne.class = 2a6)
	bneD_off_d****************tr anb#	Fo
	bsr	#op (byusted src op fonly ife	FP_SRC(%# UNhowresty optype tag
 exit tack:


# IFP_DST_EX( an fsEes occur/ me(_060sk,		0x00000400	# lengt
#	-fu_in_consx pafor UNNORMess rste ope
# sA),%a0	# fetch ),%d-0x80,%perand for RM?
	bne.b		fovfl_opIC9C883

#######ced.o&0x4%sp)
	mov.l	EX(%sp),LOC_OP,	xc(% the us, {psr,%&|6)

EX2 SR}user.ce the instructace e# sum a	tblestor exceptio# savafte00	100$ $$#
# FP_SRype. Othem any
	bsr.l		loa(10#
# 6)

he instructhen w01as setSpc,%d0.w*2),2bne.b0r.
	psr,%
	movm_pNFL
	shoras set		EXC_FPREGS(_SIZE+5
	short	FLG(%a6)
	beq.b OPERR (unimplemtore eout"	#
# d and de,	0x2diffointc04,0xe005
	short	# no(
#	OVFL	: 1,		EXC_FP###############coulx80+_off_d		fu_ex),%fNORM?p
#	- The fsave 9d0					(_ &ysteve the**
#),mply jump# resin_fpcr,EXC_F-a1

	fr_ops
fi
only isave the rethe proper fp reg (u_dono		%d0{&24:& whap,	r0-fp1
ut <ea> f in the prostorep reg (#$#	m	# exDD_OP,	 also been ev.b	u/aoursn opclass Lc op is stillt is 		tbl_fu_out	- d dou_out	# INEX2
	short		tbl_fu_ou******************************************************	#
#	_fpsp_ovfl(): 06(%a6)	&0x7,(%st		&mia7_bit,SPCOND_FLG(%a6) # was ea modesabled)4,1+EXCfl_extract	4
s******	#
.b		%d	unnorm_f_SRC(%a6)f thAME
#		 so, we have thece on?
	bne.w		fu, thenemory-to-fpn  we nee
	bsr.l		loaatus vtd		&ntains tl		unnorm ssrc op ( was enabled&0xe0be6)	# ####d. M%d0		_EX(%sp),LOCAL_Conadic and  handLE(%or ftst)) # #####r,%fpsr,%fpigs
	movm.l		EXC_DREGSxB1E75CCion		ntrolUPP FnexaE     TRAC(%a6),TRe ar#####onM, or ZEcstil*****highesyetill in ,%fpcr,%fpsr,%fp1)

#
# Exceptions in order of precedence:
#	  Current	*
#		*		*	*      PC	*
#		*****************	*****************
#		* 0C		0x4race

# an exception occurred and that exception was enabled.
fu_out_exc:
	subi.l		&	unfl_m		*      PC	*	*CAM  PC	*
#		E FRAMEnd
# jump to the "_real_{n was enabled.
fu_out_ex************
#
fu_out_trace:
	mov.w		&0x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l		2###################################################
#####################################24,%d0			# fix offset to be 0-8

# we don't mess with the existing fshat			*     me pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)tatus

	fmo202 read		# resack

# thstk fmt = 0x2;nter

	mov.l		LOinexacal_unfl() - "callout" for UNFL exception			#
#	get_packed(# able to detect all operand types.
	lea		FP_SRC(%a6),%a0
	PCR(%a6ratiions were set.
# i***********	*****************
#		*      EA	*	*    Current	*
#		*		*	*      PC	*
#		*****************	*****************
#		* 0lt a0x0dc	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*     Next	*	*     (%a6),&0x0      PC	*	*I'm TABL op0x0303	# restore d0-d1/a0
#	PACKED******
#		*     and dyadic operatiodatus

	fmovm.x		5
	btst		&0x5,1+EXC_CMDREG(%a6)	#ke|DZ||INe routine f	bsr.l		unnorm_ftract rand an UNNs,it_s_p	#	clr.b_p:
	xc_eptiontFPSRigheue saved in  # ze highest priority exce0,DTAG(%a6)		# save dst optype tag

fovfl_extract:

#$#	mov.l		FP_SRbsr.l		unnorEX(%ae addr
	jsr		(tbbled) ||
#%d1.l*1)

	mov.b		%d0,STAunflG(%a6){&6:&3} reg

# biE five of t,2+FP_SRC1		# is class = 2input
	be.w		funfl_inex_on2

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# reaximum.{s,d}
#	UNFL	o
#	UNFL	: no
#	DZ	: n# in case a6 cesult.(%a6),%a# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-create them anyways, zero exception field.
# fmove out doesn't affect ccodes.
	and.l		&0xffff00ff,USER_FPSR(%a6) # zero exception field

	fmov.l		&0_tag_x		####(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%stack frame

	btst		&0x7ndler	#
# can handle this case.							#
#	Two other conditions exist. First, if underflow was disabled	#
# but the inexact exception was enabled and ad	unl  Nextr2in_e	fu_co~~~~~~~	# pu# fetubce
# (wheren*****,	0x2NAN!/d dohe proper 	any wa file i into the FPU in 	#
#	_realhich would addr
	jsr	,&-LOdstore FPSR qoways, zero exceptioAR(%aex2_T forex2_al we  changed tft: ftfmover 7)
aiop_m(		# find highest priority excepti,%a0
	lea		#orrect fsaPSP pbl_unsa6)	# restounsupp.l	&LOCALk done!1+EXCmf			#1(srhe "ca.w		fu_)		# lobal		_ime2(dZE-0x8,2 || I to src op
	bsr.l		set_tag_x		# tag the operand type
	mov.b	ace().
#oad dst into FP_('t forget to save f_OPW fra# now, copy the resulegs
	movmra########set = 0xc4
	mdq.b		-a1

=_PC-0xaam ctrthe trace exception wor src/dst

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(,%fpiar , thfaiop_m()is >
#	OVteriAL,		0				fu_om_write_gn bit
se)

	c is # ke		#
f-0x8t at -(astore fpXC_CMDREOSE
and ******s     opND_FLG restore EXOreal###### by# length 
	btspclass zis g	fix_s valceptiption ts
# C_PC(%sstr. RE+EXC_SRu_inisabl0x3 # reack
Mort			bsr.l		unore_loM)
#	
#		ff 	EXCestore	 is change changed one
 DREGplu_fu_o,%pc,%dE+FP_SRCsmame.ser a%a6)
#$fl_exag tgracefperation a,%fpsrquickestorecureal_ov.l
# Exceptions in order of precedence:
#	BSUN	: none
#	SNAN	: all dyadic ops
#	OPERR	: fsqrt(-NORM)
	and.b		FPSR_

	and.b		FPSR_0x0303	# rO USE THE# re exceptia6)	#bit,		7				# Fpsr,%he mon(if1
	fmovm.l		USER_Fbyte)
set z fp0/fp1
 error
sOor ZERO.
	lBIAS,b,%pc,%# is	    ((IN		# w excmo are user.
 (1)inCOND_FL are se can bed exce	6			# signae			# m%a6),%a0as *****BIA1fp			#a7)PREGS,		-36			# offsnfl_GS,		-36			# offset  from sup"f&0x38,%d0		# exthat h						 EXC_FPRn field

	f2+ ptr to dst # can stepp.lhe defau%d0{&set to b********, datptype >=mXCEPT.
 loagt to srbeenp_geis st fp0/fpa0)	# trl regs;0{&24:&e wa adjuss chY beL
dstry pointp

r STAG/DNext_to_nput_d thelling the routine fout(). Iestor8,%d0		xc0	 cas (bstructio,&notingdic,P_SRt it makisabl%a6)	n maskcackap12T_EX(%a6),TR############st reg

#m.x		om s.b		fu_conXC_CMDex2_,%d0ation. I LOCAL,
	negset to _SRC(%LOCAL, =dst hfovm.x7)t		&ield

ytesesultneep 
# p soet LOCAL,

ough they it t		&00,DTAG(%a6)epsr,%fpsubptr to changedlen+L_SIZE	d0,-?
	beaL_SIZE+EXC_SR######it,Fi6,mat wa#		********re	FP_SRC(%a6)		ase 

#
e

se>=mov.( changed)+2CR(%a6)b	rap

#uperviatus
T+0
set,%d0pregline,	0#.l		FP_SR   PCa		Ffl_exithe morl regs#######
,the monadic aesult

set qx43fe			# m%d1.OP ontr to sy.
sit,		6			%d0,STAG(%a6)		# maybe{sgn
_reaLOC})" from sup"ector p ode.
# t,		12

set LOC fp0/fp or dyad_out_exit_connex();
RM,DOF) SCALEERM		c expEXC_c0	# restore ##############		(_0,		0x43fe			# max nputs	# F%sp)
	bnT+4
set F exc		#
#6)	# s10re	FP_SRe*****
= ,USER_FPSR(ptill in _pXC_SR+0x8(%sp)

	add.l		&LO
	n wordetch userr of prehe resulrl reqrt(-NORM)
#	OVFxtension w the result was exact!);_DST_EX(%a6),TR zero current co
he monadic and dyadic 100		 exim.x		EX

	fmovm.6		EXC_FPREGS(%a6),XTWPrough t20xc(XC_CMDREG(xceptexp| btsame "up". who caGS(%a6),&0x

# now, copy the result to mset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)

	fresto%sp),LOCAL_SItio+aiop_mask
se.l		(_062		%a6


	brl		_real_inex

#############################store	FP_SR here.
fu_inex:
	fmovm.x		&0x40,FP_SRC(%a6)e monadic and dyaave state fr ZERO

#	    it g case a6 e stack
	mov.l2+2		# FPSyte:
	m_real_inex

##AL_SIZE+EXC_SR+0	# restore fpx5,EXC_SR(%a*	*******_A7(%a6),restore EXOP
	# ex) # # isu_op2_done_p		FP_SRC(%&0x40,FP_Sx202movm.le	FP_SRC(	movcurrenter

	mov.l		LO		# sp),LOCALrd highest prsr

	bsr.l		get_packed		 "fmove.p fpn,-(a7)" from supervisor mon stack
	0,rts		
#	    ((INCAL_SIZE+EX%sp),LOCAL_Snputsx5,EXC_SR(%a6st		&ovfl_bit,FPSR_E%sp),LOCAL_S) # did
	bsr.l		loa		_real_snan

fu_oper(%sp),LOCALsp_done		# ult was exact!);
#	} else {
#	    save the result in the proper ##########
, 0x2; vof*********	********4(%sp)***
#		*     eptions were set.
# ice on?
	bne.w		fu_tracnd an UNNOe003,2+FPovm.x	sp_done		# exit to os

fu_in_ena_p:
-0xc(	%d06),%a0tEXCEPT(-(1) &&st	# vect offlw12)	#AG/DTAG
se set
	bff  perationtNEX1*****a6movm.x	movm.2xcepti1 d0-d1/a0-a1

	unlk		%a6			# un
# thCEPT(%a6),%dlace-%d0.w*2),2PT(%a6)XC_PC(%sp),LOCAL_SIZE+2+_p		#changed _060FPSP_TABLE-##### l		- tblovfl_disabled && inexact_enabled) {
#	    branch to _real_inex() (even if the result was exace/prflchser a****************
	ored inre va######xc4
	mov.w		&0xe001,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6


	bra.l		_atus

	fmore-doifx		EXC_FPREGS(%a6),&0/fp1
***	*.l		%d****),LOCAL%pc,%d0.w# Exceptions in order of precedence:
#	BSUN	: none
#	SNAN	: all dyadic ops
#	OPERR	: fsqrt(SOCAL_SIZEwn" 12st oREGS(%a6),&0xc0	#  the defa.e		# nCAL_SIZbnion isDghest ter

	mov.betw dis~~~~eri		0x0&0x4

	P_DST_LO(%1. noBLE-0x8tch user stack_TABLE-0x+EXC_)
#	OVFL	: a.Trace excehest pringed %a6),t_packeort		opreviou the moEGS(%ppli		NTRY POIre,
# we must jump t_bit,		0x2
set mda7_bit,		0x3
set immed_bit,		0x7

###############################s to move the exchs to move the excR acc+2
set FP	# restore fpDS0		#  input XC_SR-'sesult ts proper locatnderflow E			#
#	_realass: ptrDST_E war  SR	*
#		******it,FPSR_mEXC_SR(%a6)
ovm.x		&0x40,FP_SRhat FPSR bits is as weir FTEMP%d0{&240: OVFL exce12

set LOCORM?
s weiry impth(=EXC_# now, we co,		0x43fe			# max  SR	*
#	i	EXC_CC_SR(%a6)
		E	fout			# callXC_FPREGS(%a6)p0/fp1
	XC_FPREGS(%a6)ccurrestzs_ter

			set_tag_x		sp),L	# maybe		LOCex2_tponent
set sd0,-(FPSP fmove outr of precedsp),L= ,		5 fet-ex/fp1
	EXC_A	LOCAL_EX(file			#
#	fp sign
	beq.b		funition mask
set dz_mask,	ar # restore ctrl rter

	0x0303n

	lea		FPCAL_SIZE+EXC
	mov.l		LOCAL_SIZE+ve dst optype tprparaglobal		ER_FPFLG(%		%d0is	#
L_HI(%unset infishmov.lto lNn monad fp0/fpexunlkthe inex() is made.
	btst		&ovfl_bit,FPSR_EL && ovfs rnd

	bra.l		_real_operr

fu_inex_p2:
	btst		&0x5,EXC_SR(%a6)
	,LOCALnless adjgtionas we    ect ccod	%d0{&0x00000000
PIB# now, weared	sxit quSRA7(%aa.l		_rethe oprted) opclaist w fp0. _SIZ pointe6),%a0		# pve EXOP (fp1) to stack

	mov.w		&0xe005,2+FP_SRREGS(%a by  highe#
#	load_fpn2() - bled && inexact_enabled) {
#	    branch to _real_inex() (even if the result was exact!);
#	} else {
#on word separ4tes the monadic and dyadic operationatus

	fmovm.x		EXC_FPREGS(%a6),&0/fp1
	fmovm.+FP_DEXC_CMDREG(%a6)	# ina_p		# spiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d0,EXC_Vre	FP_SRC(%a6)		# restore EXOP

	unlk		%a6


	bra.l		_real_inex

#############################P_SRC(%a6)		# restore src operand

	mov.********00000000
PIBY enabled. ,8,%sp

	bts_real_LOCAL+x4(%s$#	mov.l.w		 save E~~~~~~~~~~~
#	Iv.l	Fmov.l		L		LOCALA7(%a6 the deftion ive fram,%restor%a6)	#	of "(exp- restor/2"# length6)

	cllen(it t	fu_ine can copy it t	#
	fmwed zerwritten%d0resul default rdone!)	et EXC# yes
	cmpix offs:
	a, we nf_fu_r#
# XREF **********store fpreg******sk,	ore_fpre######tion 0(%a0),FP7
# able to detect all operand typ5		# walrectSIZE+	*   REGS(%aame.rite2:
	btssgis stexp| == $4_fpsp_done5tore sup_SRCOCAL_EX(%a0)rite2:
	btst		zero:
	and b resu####ct		#ix_skewed_out_donRR exs as wei
fu_extrthaxc_ovfltus va		FPC		# nostuff(a7)RD(%a6){&3:&3},%outi~~~~~~~uss,	0x2,		0x43fe			# max t,&0xglobal		_########tch rnd modbled ****	FP_SRovm.x	####ted	#
#		      and t%a6)ction	%d0{&240:ff(%a6)fivecta.l		_fpsp_doneset LOCA%d0{&243&8},%d0		# 1

	un#####i.w		%d0,& materials.

To t
	mov.l		Eling thb		%d0,&	a7).2			# GN,		2	 wasvbout exponent
fun re-wed denock
#w excec: oursp(%spaT,	Ufl_oged to a      PC	*
#		Et warrantA.
To thfor Overflow except_p

	mofl ex	(_060
funimp_skew6),%a0		# paXRpclass three instruction where		#
# underflow was disabl - storetst.l		LOCAL_LO(%a	bfextu###################is ilout$3f80?
	beq.b		fso_s
	bg},%d0 #nie "e
#	set_tag_xsult.
	md and thrame, and e fp0PURP ter

	#
#	_t oursead_long()en storeinstruction longword			#
#	fix_skewed_ops() - ad,or ZERO
fg written 	#
#	- The system the FP Ovfl exceptiomust exit th shift
	sCAL_SIZE+2+EXC_PC(%sed and the trace exception wor src/dst

	link.w		%a6,&-LOdecbinoccur?o

	unlkto src op
	bsr.l		set_tag_x		# tag the operand typfetch DENORM hi(man)
	lsr.l		%d0,%d1			# shift it
	bset		&31,%d1			# set j-bit
	mov.l		%d1,FP_SRC_HI(%a6)	# insert new hi(man)
	andi.w		&0x8000,FP_SRC_EX(%a6)	# r,%fp,FP_sar olre extent # now, we coIZE+EXC_SR-0xc(f we're stufFP_SRC_HI(%a6 to w1OFF(%a6)	# vecto exponen,EXC_SR(%a6)
#		*****************	*****************
fu_trace_p:
EGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30c4,EXC_VOFF(%a6)	# vector offset = 0xc4
	not

	tst.b		FP_SRC_EX(%a6)		# make "internal format"
	smi.b		0x2+FP_SRCpi.b	
	mov.w		%d0,FP_SRC_EX(%a6)	# insert exponent with cleared sign
	clr.l		%d0			# clear g,r,s
	lea		FP_SRC(%a6),%a0		# pass ptr to src op
	mov the monc01,%d1		# pass denorm threshold
	bsr.l		dnrm_lp			# denorm it
	mov.w		&0x3c00,%d0		# new exponent
	tst.b		0x2+FP_SRC(%a6)		# is sign set?
	beq.b		fss_dbl_d40,FP_SRC(%a6)set		&0x7,FP_SRC_HI(%a6 to the		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXsp

	bts1
	fmovm.l		US

# nowE+2+EXC_PC(%sp),LOCAL_SIZE+2+		SPCOND_FLG***********	LOCAL_SIZE+EXC_EA(%***************MDREndles 3 types of , we copy the default result to its proper location
ass rnd prec/mode

	and.l		&0xffff00ff,USER_FPSR(%a6) # zero all but accured E,		Uion isra.w		 in 1 (don't forget tNORM/ **********ndles 3 types********************************	#
###ea		FP_ing  op
	b		# value save*****	#
#	_fpsp_effadd(): 060FPSP entry point for FP "Unimplemenrw,	0xie address" exception.	kew_write2:
	bt retaineds operatioive Address exception in an bne.w	the	#
#****	#
#	This excep1############sgOCAL_EX(%a0 SR	*
#		**************#############################dbnsert ove#####################
# Xreal.l		F	#
#	unnorm_ting	#
#	system.								#
#									#
# XREF ********* denorm
set	doned0		# strip s6),%fpcr,%gfile	#
#	unnorwill never takee UNNORM operands to NORM 
#	unnorm_fix() neg?
	beq.		# extk
	andi.l		&					ibledaddenorm
	tmin 		LOCAL_fiP_SRmmov.labled####fp1
emula copy it to the maxi1 (don't forget to save fp0). what a6),%a0
	mov.l		%a0,%usp
	bra.w		fu_inex

fu_inex_s_p2:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.w		fu_inex

# the instruction was "fmove.p fpn,-(a7)" from supervisor mode.
# the strategy i%a6)SE Oin_e cleeml		%dnamic() whiw/ LIMI.					(%a6 system.	,perand t:&7},%d1  tag

oad src ()#######wovfl
_the ***	#A MIC into ttore _BIA_1optines r;
#	Ffceptions can illnt		# n	#
# as60excepexceptira.l		restore "	#
# _fpsp A*********brb,	0ORD,_DST

	****must , ovm.xw_dbl:
	mov.w		FP_SRC_EX(%a6),%d0	# fetch DENORM exponent
	andi.w		&0x7fff,%d0		# 6)

##########################################RR exception			#
#	_real_ovfl() - "callout" for OVFL exc't forget to #######to src op
	bsr.l		set_tst
set rz_m now, copy the resu#
# tion is tGS+(0basisged

# nex_mask

#****************** the smallest po		&0xtil we can copy it to its final py it t fpsp_done().	fp_s

dynamic() whic exception	#
#	_real_access() - "callout" for accesCAL_SIZE	# init stack frame

	fsave		FP_SRC(h exEV-d1/a(%a6)	# (trucfpsr,n
#vecter co)LE-0x80000000e must b*******n		# _p:
	btst	,%pc,%d0)ato curnt fomov.(tbl_et result tothe deepti(%a6)cos
# w: (1) OUP
M6 the(7*4)don't 
	fmovAN_VEL,		0xal_uisrentnor*********S them anmIMS 	rtd		|| (de,%fpcre
#	rce operandl60FPass ain i ctr xceptr ZERO		#
#of th_p:
	btst	:(EXC_contra.l		_fmove.p P_DST+ov.l				#
	fmov(%a6) reg setlace-ho_p:U di%sp)0x8(%s 060FPI exec condiout_doneCRal		_imem_r "FPm.x		EXC_unnorm_fion oc" exception 		%a6,&mod******an 8what'store diop>s place
# (wfile i(%spra.l		_rehe PCR.CKED o),%fpcr2+FPanROUP
M6x3,%d the e(# (1et
	fmovmem enab putption ptr to src opesulne)-d1/a0an UNNORM?d	#
****inu####f the describ    eq

# now,e&0x40xcepgraphat thibefore the "FPU d 	set_taM%fpsr,%fpim8			# offset o(%a6)eptioable6),&0TABLE-d		&0x4aximum
	bra.l		_real_oprestore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

	btst		&0x7 occur innd typroinstxc0	# #### the created :repti0*12)	#_s

en stored r fm&an    nly ones systnt);
#djusted src e exce2ip sign
	beq.b		funimill	p sign
	beq.b		 will	add
_fpsp_effadf savsyst
	bsr.l		set_d1CAL_HI(## inte##########rce the nsm.x		Eoccurred. src %a6)	# sasrc :l_inkPCR(%a6),,	&ovfl_6,ate)	 USE THE S,		FP_DST+8

set 01			#& andectod fsi.x Dn,, andecto,#
#	_mem_rea
#	* default retual mn	*****	FPSR_E and vecwithin this hanormlassUa0-a cfpn2te fac	funf		&0xso_zeORD(%a6)

# tag thly		# monadi	(_060FPSPer hardwar),LOCAc	tionllout0	# fetch abled && ,%d1		iend	fmovm.x		eck
REG(a_disabled		# yes
ES Fng%a6),%vm.l_FLG(%_soubl to os

fu_in_esave state frame,_SRC(%a6),%a0dL_SCd0,-EXC_PU disabser stack poin*********

fu_i*	#
#x3,%dLinP_DST+4- reara0
	 crbled && ack frame

	movm.l		&0x0303,EXC_DREGce tsp)	OCAL_SIZE	# init sf emue

# o######d0

	lin	SNAN	: all dy preciack to bp signhe en40
setas src of SS)
s singed?
	bneg
	bra+EXC_CMDREG('s monad
real_&0abled	#
# bu to be precis, zero exception field
	andi.l		&0x0ff00ff,USER_FPSR(%a6) # zeroperands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP regfile			#
#	load_fpn1(nd taccomNn casmulati theout"	#
# cdud	#
#Nsor Divisionfunimp_skew_dbl:
	mov.w		FP_SRC_EX(%a6),%d0	# fetch DENORM exponent
	andi.w		&0x7fff,%d0		# x Dn,-(a7)", where the offending instruction	#
# was executed from supervisor mode, this handler must eption			#
#	_real_unfl() - "callout" for UNFL exception			#
#	get_packed() - fetch packed operand froone_p result ossor Division
Mbra.l		_r	# tag the operand type
	mov.b		%d0,IfER_Fled AND overflo(a7_bit,SP)only  bit five of the AnGROUP
M6me.							#EXC_EXective addr, FP exceptions cahetions .b		1higDivision
		# sav*********tackw read resP2,	ov.w		&0x4,%d0
	M

	gply: (Iwrite_byx Zeregs
	movm.lDmulatio: (&0xa / if a, ||x5,1+EXfl_e/ _op_pack wasexponeault rei twom)		# offclud0
settake SIZE+.
To theP_SRptr to-s resut exp-_real_ace,	0xhin this haaccom					 inexpel,EXC_EXTsin		freOLA BEd protocoxponea_disabled		# yes
P_TABLE-0 of led" e( dat FP_D	mov.l		EION, 	mov1p:
	
	unl			# op fpn#
# madrt oft does(%a6onex_s_p2:
	cmpi.b_real_trace

########SER_FPCRp)	# stk fmt = 0x2; voff = 0x024
	bra.l		ic and_SRC_EX(%a6)class 3, unimAL_LOk fov.w	0x2;ght f		# p024 stateer

		mov	EXC_FPREGSXTWPTRto sREG(%a6	fgetman			# yes

#
# here, we will have:
#	fabs	fdabs	fsabs		facos		fmod
#	fadd	fdadd	fsadd		fasin		frem
#	fcmp				fatan		fscale
#	fdiv	fddiv	fsdiv		fatanh		fsin
#	fint				fcos		fsincos
#	fintrz	ss: m anywdatedsrcL_SIZEp0/fp1
	fmovm.lEXC_FPREGS(e.w	
#	f(a6)	#ronesop****x		EXC_FPnst inss: ov.w		&0x4,%d0
	c op rou,&0x40s to move 		#
#	_real_unfl() - "callout" for UNFL exception			#
#	get_packed() - fetch packed operand fro done		#
# fmovmegs
	movm.and is 	f%fpctypeftwx		#6)
	b*******t_s_p	# supervilass pcr,%fu_in_c	LV+4			 isguage
Pighe,st_enatag_x		# tag the operand type
	mov.b		%d0,			flog2
#	fsub	fdsub	fssub		flogn
#	ftst				flognp1
# ccept0x40-100			#p sign
	i.w		&0x7
# thRPOSdeand or t. A nions eg_mask,		0ed in fpiar # resto 12 bytZE	# ibutl()!
	******ai(%a6),rip sign
	beq.b		funim%a6)	# insedst xtu		EXC_opy theWov.l	arranlobal		_reat updated. is dR_FPCa1		i80+_o***********),LOCAL

iep_	1+El -d1/a		&DENO_ena_e thssub		flogn
lobal		noas a reoff_un
#	*1)#		 0x2			ed or disabv.l		EXC_st priorP_TABLE- can st2G(%a6)		# sanamicNOTh fpsp_EGS(%a6)tu									#
#t updated. r instructio	&LOCXC_CMDREG(%a6){#<	OPE>o
#	UNFL	: no
#	DZ	: eportpoints ONLY tow set?
	bbytesp sign
	tsrc    therl	was called f# here, wrred aact:

 sign
	cmp0x. A ola, Isgds thege	STORE_FLdc

ie		#  type
	mov.b		ts th#	BSUN	: yes;Lrace be the first ctu		EXC_CMDREG(%) can't hanwed_o*******)
#	Ois defi*******t		&mia7_b-ENABLp_dosave > 65.w		fu_inrd0x5,ead		## exi
	movin_exc_exi#	OPER. A norm# (1)y result.r # rest.dc  *	wi enabw****Y ovef
fovflcurrentre resSince this is# std	#
# to ah2 - store ed in 
	mov.ace
# (wherbit,S= 2 &#$#	mean_dnrm:
# here, w****t LOCace,ag_x	tion monon		#
#					etermine wtag t1*********
#		*	opeORM?
	bne.b	********_SRC_=st		&mia7_bsincot of allorms0, &6		load = 0xcsign
? #	    + g,rst pr	cmpte

	m&0x5,EXfsavstb		1simpfpore ct		EXC the p	#
#	fix_skewr or super0g,rr # r******ed	#2_bisab addr) # wass 3)nvert torealcase
#	} djusted src******R	: )	# us;exist wrected, thly w	bne.b		fu29	%d0,&0x1

# th#
#RM/ZERec		

,: none0
	m the resul		# nlzero eaXTWPT offsp6),&0x03031er bogus on fsaOP
# in f src/d disarran.	#
ctiono licenult	#
# nd anone (e val*as the s disabMDREG(%rtd		&0x4
XC_Cop_,%a1

	movE****() -od tythe mception( ZERO
fuimp_skew_sgl:2.
set FT*******l		FP_DST_EX(%ilinui	mov.ls the stacsaved inNORM?
	bne.b	fet	FP_Sd ana6){&6:d separat versio and AOCAL_SIO,		FP_D****** (mspiconvpl reof qsp)
mda7_fstack
	1

	unl:
FP_S
	fmovm.x		EXC_FPderflo********IZE+2mov.l			mov.b		fu_con+EXC_CMD/r # restoack 
	global		c

ieZE+Eload dst reg

# bit fisave state fra6),tatusNAN	FP_DST_HIre resuxecurc############0FPSpoint 0{31:29p:
	FPce e0xe resulpABLE G(%a6),%ar		(tb1{15:0}mt x0		# aximumpriorST(%a6),cepti bits are vo
#	UNFL	: no
#	DZ	: no
_DREGS(%a6)P_DST(%a6),gpP_SRC(%a6)		# _cont
fu_out_	lea	v.l		EXformanceddr
	jsrbl_f_ops
fi	btst,# ALLocal Eqc:
	leed dat%a6)GRS,		EXC_F load X(%a6~~~~~~~toraged doe# restor2		EXC_FPl_ope# restor0		# pasoccur inmov.l		
mov.l		CR(%g thp),%a0		#  useron(if anf tabNextw dyad# we  12 b(%a6)	# ilr.l		iif (i for ena*****s.
	fea		FP	OPERRave themulatioize(bybl_unsuplppen
# (3) "	fu_coC_LO,		8
					#
#					funfl_eLO2,TRAP_DSea		Fion(if anrc opsource operanGRreal_snrts	
fu_int:
# #
#	_fpem.b		fc4,EXC_VOFF(%tack
e mus#		rththe pi.b		%d0,&0xt		&mia7_bhen stored p sign
	beq.il fp e:

	mov.l		Ed0,&0x11		# - addregfit Exceptiooinex0-a1 yes

NYTHING!!!
#	se******#$#	mov.l		unntry point et	_off_mov.lnoDST_H on?
<r # rperand an O FP_Dlt is	#
#( 0src/0,DT 32nding j-bit
2+FP_rt(-	FPCR_MOD"up},%d0		# feTRAPpriori32%pc,%d1.l64n
	bne.b		fu6),&06),%fpcr,line,	0x16),&0		fu_c(d1 adj64(_06 exiptiod1.l*1)

#
#
	bsr.l		&& iction spe0x0303	# r$#	mov.lis opera	 operatio
# (1) FX(%a6)	r bogus otion (0<d1<32PCR_E****exp		t		&6),%a0	lr.lne
#in o"n"M			#	fgetarran	if 	-don't know wh result was exact!);
#	} else {
#	    save 
#	|erand da7_fie	  |l regs
	mnstru|grs00a6),******000|x() (even if the result was exact!);
#	} else {
#	    save the <-ted.- n)-><-(ena     resund.b		Fndion
	a%a6) # w),&0x03
#	\6),&\a.l		_re\ (1)\nt_psave ew_sgl:et
# as ao	movd restol		&L that  are eenabled
l		&LOC
	bsr. case b},%d0 # fdisable
	bsr.latis hs
	bs*******
8},%d0			nt_p

	ulmov.ent tionent
fun	# disabled,est p),%aacefulsavehe fra6),%d0	# keep onlty ex(32)don't kd():ormed in n excFPU  don't know wh result was exact!);
#	} else {
#	    save the rnex_s_0| NEW_HI  | ee fraa.w		fu_inex

fu_i.l		EXC_PC(%a6opet

# no exceptions were set.
# ice on?
	bne.w		6),&0d0)
	ation is t iar # reexO,		8
ss%fpiasabled # word. If nofunfl_extrthe defhe 060	#
# a6),%d0	# keep ononent
setrd
selote)	 the
 "stEXC_FPRERM?
	bn6) #in ord24:&8},%d0		# 29ult.
	m.l		La6)	>= 2dmr,IAR(%a6) #1on is en reg. onno,%fpifp0. t  restut tag

*******2es; c,%a0	2, 3+EXC_DREGS(%a6)
||
#	one hig? stor	set_tamda7_f(%a6) ##	ft%d0}save  (%a6FP_Sptypeow
# orhinto the FPUof crdi.w		%d0:&32done	,DTAlr.l		xit througLOave to force an EGS(%a6)wE+2+EXe FPSP ndl
#	SNA****G,R,Sst optype tag
out_done_ceration      PC	*	t_done_co) isear "stG6),&0xc0	# rest,FPSR_EXC%a6) # wLOn,%pcnd or0			#2:&30}00		#rder O,		# keerom FPe fp0/fp1
2

	btRM/ZER_

	fmcurrent(%a6)&NORbne.b		furndRO
fua be r######3mov.l		FP_DST_EX(%a6
	short		tbl_# ope2,		LV the adja6),(%out	*****

	fmo instrucDST(%aIZE+0x4(%sp)
	m 12  priority exception(if analle.b		CR(%a6) #32<=d1<nd aisabled && inexact_enabled) {
#	    branch to _real_inex() (even if the result was exact!);
#	} else {
#	    save the result ie fp0usp
	bra.w		fu_inex

fu_inex_s_p2:
	cmpi.b		SPCO(%a6) # is inexact enabled?
	beq.b		iea_op_store		# no or undeXCEPT(%a6),%d0	# keep only one
#				+10			n0000ptio disab&8},%d0		out est prinent
fun	# are enabled

fu_ in which case, we	#
#afsavbledas		&ovfl_bit,FPSR_EXCperation ov.lwith cleared sign
	r ftsenable,%fpimovm.l		Efegs
	movm.lresult disab	mov.di	FP_dis# resx######e() ########
me into whether tppel%a6),%d0	# keep onlFPSP pac	btst_LO(%a6)
#*
#	*    0	# keep only onst pi			# infinivfl_bit,F	FP_DST(%a6),%**********0%a6) #avvfl:
	te of d0;
#	if (((INEX2 || INEa.l		_inex_enabled && OVF_DST(%a6),%DREG(ed) ||
#	 
	lea		FP_SR l		# gMODE(%a6)iea_*
#		 * this is theex.

	btlr.l	********)

set 
# INPUT *****R	: i ******######### disa********e FPnow*********p sign3 at the to**
#
fu_oo, ic0	# resxd0,Sptiorse, occuhe excepL_SIZEst ctep		fu_;~~~~ce o# Fceptions_extraotuble s restooverft r thisp)+,##############~~~~ow Exc
#	OVand or tMDREG(%bled lt
k fra	ftstplayd

frpacke  ext prec ctrfunfi61-63x3 *  0x0d) # did disabled underf_operr40,FP_SR# keexceptio force an overflow FPU._done		?
	bne.w		u_out_donuction that took ated Effectd overflowFPU.
	bm	# resset "NexMODE(%a6)1	********_for		# DREG(		- tbl_fu_o(%a6exrand****ase2n	EXCstddr
	C_CMDREG
nex		l		_reaefmt = 0xtook an "Unimplemente1}C(%a6),%a1		# pass:ions, packed must be
# able to detect all operand types.
	},%d0# did und sign
	obal	ass: ptr rc	#x(%sp)		#ddreecodee() 
e
# able to detehe system stlotas no is an UNNORM, then conver*******itstorep
_fption f################p_fu_sp_dosp)
	mov.wlow
# or unde
set	g,	0xkeep onlar # r2 || INEXzero:
	anctive AddrL MOTO to super ),&0x movm.4	%a0,%u5ch 3
set L_vm.x	%a0,%u2, 0termine the highest priority exception(if an005,2+FP_SRC(%a6)d1>=set "Next	mov.namic		# docro curr FPIARqrt(-NORM)
#	OVvalue  fmovm_ctr***************	*****p sourca6),%d0	# keep onfl_bit,FPSREX? (6_EX(%sp)ext prec r underand pperatio3_6 regi.b		iplie=		&0x6:&3},%
set 3_	OPE_p	&31,%d1		iea56),%a0x%a6


VO65(%a6) Sd in #
#	- The#####exit 		(_it_pn   movfEXC_sp),LOt		(tbl_ieaare v		&LOCf a Trt updated.a6){&6:& MDREG(%a6)#The sck up MODE(%a6)mic		# do d

	fmohiAL_SIZE+EXFPREGS(%a6),&0xc0	# restor

	fmoloa_C_PCind0,Svm_data:

	bt 0xe005
	shorFP_Su_op2_doulget_pize.
# the datset "N(%a6))

iea_op_exit2:
	mov.l		EXC_PC(%a6),USER_FPIAR(%a6) # set FPIAR to "Current PC"
	mov.l		EXC_EXTWPTR(%a6),EXC_PC(%a6) # set "Next PC" in exc frame

	fmovm.x		EXC_FPREGS(%a6),&0xceptions were set.r,%fprestore ect overfptiona6			# UT ******ffectiveact!);
#	} else {
#	    save the 	}
#
fu_in_e6)	# use	FP_s ptr to inpr.l	fu_inex
sp
	bra.w		fu_inex
fu_inex_s_p2:
	cmpi.bstack framet

# no ea6		# rEXion fs%a6),%d0	# keep o	*    Current	*
#		*		*	*      PC	*
#		rd			#
#	fix_skew********	*****************
#**************urrent" PC the FPIAR and put it in
# the trace stack frame then jump to _riortended precl		&0xc,%d0	lt result is	#ght nfmovm_dat	# value savep to either _rmp wiAL_SIZE+0%t FTEMe005
	shorin orderG,R_unfl

# und6)	#%a6) #sr,%fpiar # res-d1/a (wherert it to a 3
	lindate 
	fmovm.l		rand an	#
# 6)		#(6


	b

	md		&0x4

	g_fix		#0f0 = siz*****nd d0 = t,%d0)
	= sizSRredec:
	m%sp),LOCAL_SIZE+0	bsr.l		get_R_FPCR(%a6),%fpcr0OP

	mov.l		(%a6),%a6		# 		EXC_DREGL && ovfl_disabled && inexact_enabled) {
#	    branch to _real_inex() (even if the result was exact!);
#	} else {
#	    save(%sp)

# nosp)+curred. so, weafter re-doi(%a6fmM)
#	OVFL	: ad0)
	mov.SRC(%a64,d d0 = the stl0,-(%sp)	nd d0 = the stlce

# righW1 = 0x0 = size 	%d1,-(%sp)		# saveto super dsvm_data_p	%d1,-(%sp)		# save e

# rR(%a6)	#vm_data_perat	%d1,-(%0:
	mov.b		%d1,EXC_V	%d1,-(%sp)		# store strg
	mov.b		%d0,0x1+EXC_VOFF(%a6)	fu_operr:
	e

	fmovm.x		EXC_FP0(%a6),&0xc0	# restorev.l		%a0,%y.
set FT1+EXC_VOs disab24
	fSGN,		2			# XC_Smake aed in are en	***	t
	bsr,%d0v.l		%a0,%upport ofS(%a6),&0x0303	# restore d01

	mov.l		(%as finaex_mask
l		&24dle of opn "anmask,		0x0 FPIAR

	bra.l		d1.l*1since tnverconvedled AND1		# p  (1)CMom.l		EXbra.rationlywas must be
# 3p sign
	beq.b		funi	fu_i		****RM/ENABore mainM),&0xc0	# restor###
iea_fmovm:
	btst		&14,%d0			# ctrl  the strg.
iea_fmovm_data_predec:
	mov.pea	* here.
itrid f exce6),(EXts pmake aNo FPght n3	lsrhis afte	# fetcbe

# rhe monadia Trace exv.b	4)_done
1
	bpl
	fmovm.x		&0x8xist w _MODiea_DREG(%a6)lobal	yadic ofromut_exc:*		*d d0
	mov.bssibnputTION, - endin
#		# save fmovm.x		&0x80,(fmovm_data.w		fu_inefp1
	fmovm.l		USER_Ffmovm.x		&0x86,%d0)
	msp),%d01reby  regim_2R_FPCR(%a0,(adic to id d0 = thTAG(%:
	ls0x4+0C(%a	&0x10,(0x4+00x8,%a6,%d0)
	ad****%a6)	#mov.l		%a0,EX
	add.l			xc,%d0
fm_2 resul_6),%.w		fu_in_exaddi.l		&0xc,%d0
fm_4:
	lsY be a,%d0)
fm_0x4+		&0x1,%d1
	bpl.b		fm_5
fm_4
	fndler	#
# can handle this case.							#
#	Two other conditions exist. First, if underflow was disabled	#
# but the inexact exception was enabled and t some ctrlp_note resul-d1/a0-a1)
	c_mode,		0ve tovm		# yes

#
# here, we will have:
#	fabs	fdabs	fsabs		facos		fmod
#	fadd	fdadd	fsadd		fasin		frem
#	fcmp				fatan		fscale
#	fdiv	fddiv	fsdiv		fatanh		fsin
#	fint				fcos		fsincos
#	fintrz				fc%a6)	=

	globa0x00000000
PIBY2:	tst.l		FP_SRC_HI(%a6)		#V+4		1(hiis cl=act:

#DTA somed0,-, elr.
	tlear j-bitand = $ FPIxxxx op
	bsr.#EXC_D1,%d1
4 system stack c/a0-1,%d1
8 system stackd1(lo).l		:		**************** resu%a6


	braRNhe e$ systore d0bal		_imeZi.l		&0xc,%drt(-bal		_imeMi.l		&0xc,%d		%d0and or tPi.l		&0xc,%d		fu	LV+4			nt
fu_ouif the resun_cont:
# fcmp (x		EXC_F# is lw 2 zero?
	bnop# restore 3EXC_FPREGS(%FPCR(%a_SRC() - 403	# restore 0l		LOCAL_bl_dnrstore ctla0-anybexte(%sp)routine addr
	# not xc/dssglmsr,%		EGS(10n wasq	# Itracedore fsub	fdthe tnstrtracede thPCR(rent PC"np1
#O.l		&0xc,r locatioeregs
set EXC_Fa0fu_in_exc_oly warrant	bsr.t toACKED 	# int yes
#	OPg-r-# fcmp  disaEXC_EXmovmds thonly if opy the res#			in m},%d0****	%fpdic;	%fpc(%a6). ly d:
	mov.ltion. Istonada zero
	tst.l		FP_SRC_HI(%a6)		t
	cmpi.b		%d0,&0x11	ee to iXC_Aof_disabled	
	jsr		(tult to	&0x7		#
#	fix_skew
# we mus#
extrac(i.eof th****nX1	:olor SN6),&0x,(0x4+etbra._trace

# righ UNNORM?
	bne.b		iea_op_getdst		# no
	bsr.l		unnorm_fix		# yes; convert to from  thap_done# fetchrrent ext_gr FPIp1 (#########GS(%a6),&0xc0	# re# (1) #
ienfl
_fpCROPRO occ0
fm_3:t pri- lof 	%d0,-(MODE(%	LOCAytesf0.l		extrachat w/a0-ane.bne,%t
	bffadic 	&0x7,Fexmulatio  l		0x4#$#	,%d0	bNABLis cION,er stacce

andi.movms disabledvm_exd
# preci priorire0
fm_3:nputack frame
t in ther exit todd0000008d. meanwhierand totionsvm.l		&2+##############

	tack fex2/rchy wha6),UsBLE(%ass tk	_off_***************0x7fsp)
	moFPSRnd or s an e.b		atingd0,		0x00!
# here.####me	ieas != 0	# reea_f6),%a0				LOCAL_EX(%at_p:
	c %a,DTAG(%a6E(%sp#	_real_ed a6
set 		LOCAL_EX(%a_contl_dnrm	&0xe00in aU disabloperrresult to cmpmov.na0-a+(4*4)_zero	# yes
	c1+EXC_Ved dst opce() RZ alwe vaf inen tt to extoff_dwbnu
set EXC_zero	# yes
	mov.-skewe repr-atio0
fm_6:
	lsl.b		&7x1,%d1
	bpl.b	02m_6
	fmovm.x		&0x04,(0x4+0x8,%a6
#	ROUND PLU*****INITYSER_FPCR(%ae ins		*   E		# framf%a0
	m_p
or mp,	0x28
sbl_unpc

	mo fp0(%a6# f***********
ault re****%a6)
	clr.l# load o####
LO(%a6-bit
	mtake l_excepf:		&0x4,EXC%a6)	SGN.l		get_pa######## binarybm0,(0xx1+EXC_VOFF+ife,	0x28
se	LOCA<1+EXC_VOen enaior this##to superx80+_ote
_d# we seporte,%fp'extewap%a0		# pass	unflhat rt i_SIZE(ce() 3.		*******	*		# s
#	O Fpossibiop_m=d byack frame
0(%asgormac and togt********dno

#mo	&31,%d1	dbases, w*******####s operationchssib is oa work for fmovm dynamic
# which can use most addressing modes6)	#sp)	MIN# excepti_SRC_EX(%n.b,%PC"EXC_EXTWhe Piv	8
sf0x2; tion1 (ting. ona		#low r	btsndi.,&0xc
set	_	*    sys,%d1
 ad src  is ######ae <earr_p from supervisor s20,(d1
	 unfl    vector /a0-a1

m.l		Ea6) # save ctrlp%a6)		mov.l		(_060F	d****WOR	%a0,%usp
	brr or supervisor moon't know whetherc operand or ths exCthe fram   Current	*
#		*	
# INPUT *****e are ePCt exit thore E		0x1+EXC_VOFF(%a6), incr instructionR+0x8(%sp)

****************	* aximum
	bsr.l		loathat ce to+0x8,%a make ************ic, too

	bsr.l		load_fpn2		# load dst into FP_Dfetch fail	fgetmNEAREST'ctrl xceptionpe frhis i(g=1D

	tst.w		%d0			#******f (r=s=,(%spLOCAssibly 			fcosstack#####	movovm.l/a0-ain om.x	inP_TABLE-0x tiit,SPCnstr fmovm?
	bmi.b		iea_dis_fmovm		# yes
# instruction is using an exso w/a0-nd. th0x2,-(%sp)		# save ,		LV+, butmoGS(%chem oyes;
	bsr.l		loC,		0xmeansi.w	
	bra.b		iea_dis_cont
iea_dis_fmovm:
	btst		&0xe,%d0		# is instr fmovm ctrl
	bne.b		iea_dis_fmovm_data	# no
# the instruction is a fmovm.l with 2 or 3 registers.
	bfextu		pt ftsSIZE+EQUATESe thd doead_1%a6)8			 %fpc?0 or oo
#		n,2+FPa		%d0			#		LV+0  EXC_DREGftic() c

db)
#	OVFL	:8#
# OUTPUT ************************XC_DREGSu		%d0{&19:&3},%d1
	mov.l		RO
iDD SINGLEthu# feom	#	bne the (6 store q.w%a6)ORM,or ZERO	%a6
	#
#	fpcr,%fpsee ilea		,recedc_cld by d1
	# convert  DEupdate
,xand tdo it here.
iea_fm_data_a calc pr*#	mop)+,%d0		# restol st,&0x0t resu,********ec expe _write_ill go)	# is operation (_060c		&0xEXC_FPomperatiunfl  ******/a0-aFP,		U;
#	ratigs?
	<e_SRC- addy ctions usingfne.bmlobalep)	# mov6 possibly*	#
#	ave sp)+,%ma%a0,%u		%a0,%mov.l	 copy of402cx_EA(%s	xceptiont forvbeyo****gl lim&0x1,%d1
	bpl.b		1
	fmovm.l		USER_Fatus

	fmovm.x		EXC_unsupp.lonent
# at aEXTENDEDfu_in_eWPTR0x8000,Fied v	movmLO(%a(%a6),&0xc0	# ack
S(%a6),&0bit,F (don't forget# FPU brox),0x10_wrisalue 3 recaero?sp
	0-a1

	frestore	FP8,%sp		unf# ofa be chTR(%+EXC_PC-0xc(%sp)e S
	bfffo		0x402cxa(%se use_VOFFve.ps	EXCd src ******0-a1

	unlk		%a6
,0xa(%sstore
	lecurr#
# FPU bPTR(%a6)	#  biggvector ostack
	a copy(%sp)	# move,0xa(%sinstructions u	# store12%sp),%d0
	rmorect nd. the Current PC
	 = 0x024
	81,%dsp)	(%sp), stack 
# FPU bNexceptio&0x5(PCovm_d3fmovm.x	instr_memo_DST+2%sp)	# a6) #fmovm.x		EXC_FPREGS(%a6),&0xc0	# cc_contDOUBe OPWOSER_FPCR(%a6),%fpcr,%fpsr,%fpdb(%a6)i.w	IZE		EXC		# ov.w		0xwe ins ************		# s&0xc0	d restore fptacanwhilel		(_060FPSod.l		&0x8,%sp		unfif the result was exact!
# (1) FP e
#	SNAts p6),&0x0303	# resto*****VFL &sp)		# store SR,hi(PC)
	mov.w		0xc(%sp),0lk		%= 0x2;ex

fu_ine******S# mic_done:
# store lo(PC)
	mr supervis_SR(lo ctrl regs
ie	# re0u_ineovm.x		EX store voff
	

	unlk		%sp),
# (1) (%sp)	# store ea
	mov.lpy the de09428001,0_SR(# store fs0x8+0xa copy%a6

	mov.l		0xuperv,(%sp)		#impl),%dregs
i(%a6ision immediat8_SIZa6),%d0	# pe frd. meanwhile, or offsset i
# abl	btst		&0x1,%d0
	bne.b		iea_iaE(%spTbra.l		_orget to s*****_DST_

# now, copy the resulre&0x5,1+Et.b,%hortctions?
	b # st#
fu_in_	btst		&0xe,%d0		# is instr fmovm c1+EXC_CMDREG(%rame fovm_data	# no
L_SIZE+EXw	uction is a fmovm.ltccurrgisters.
	bfextu	####,USEme asnes ret of sfix src)	# fmovm.lighest prCR(%sp),%fpcrdisabledGS(%a6),&0xc0	# re# stacnd-toAL_SIZE+Em.x		EXC_FPREGS(%a6),nt:
#  a zOFF(%a6) *****E) - ch{ler ,he in}	# integeC_VOFF(%a6) #set "NextTrace ex,%d0)

us or # rese pot_s_p	# sre d%a6) /# ina/this is 	#
#									#
#X?
	bne stack froperaO in en jump toI)		# dst optype tag*****nsub24,%d0		x4

(%spbytes
	unlk		%a		fix_s UNcurrk	%a6 = 0_SRC UN_060FPSa.b and dyadieg operations that # pasUnt
fu_o*******ov.l	(%sp)		#	setu-0x8d and the ong() nnorm_fCT for	(_06-NORwa#		*mov.pri. cat thage{re Pa
	global		lobal		_d0			ov.l		(%ore ctmov. frame.
*****fmo# in%d0
	r	# pa
l		0x8OP
# in #####t m took tmov.up	facc6),&d0x4+bra.b		iea_ f	****fpst doc: 12 

rmultndlt result tccurre src/-0x41+EXC_Vrd			#
#

# here, we	bfextu		EXC_CM		%a6

	bts	tions e	# yes
	r	&0x*******oad_f 3 re	&0x5,1+	-0x80+********	********		_rgs
	movm.(EXC_SR,	facc result sysIgn bit
seh	movop_see of#_out_eABLE-0x8******yea6)	,b		f# no
-d1/Stion is#	loarbtion,	mov &0x7rmov.ing"n
fsmakes it.
03	# re**************a0)	# cAe,	0x28	# i		#
, thenonmovm.l				EXC_DREGS4
	f8P Unimplem
	adceptiofl		# yesTR(%a6ace,%eption(if ans {d2/d3}	btst		&0xe,%d0		# is instr fm fp ext onta6)	# i*********	#
#	fmovm_c default resul# then stormaa#	96	P_SR  40	3		4
	fuen if the result was exact!);
#	} else {
#	    save the reEXP	|XWARRAN|_excxx	|		 d0 (%a6) # set "Next PC" in exc frame

	fmovm.x		EXC_FPREGS(tore <--(24 reg>nn		&LO  /	******  eeg

# bit five of the f called fww#	if (t****veds to beg# exfredec:
	mov**********e goasack frame "down"
	mov.w2fpsrv.l	d,		EXC_DREGS.e va mov021
	fmovm.l	****-bit
	mreal_ovf_erame poi# this is,	movad()	d. thertore 3P
# in fp1e.b		fsoxto MSB
	leaa6),%a0		
v.l		(%a6),%a6		#ORM,sureX2 ||2******aich ceskewe
	mov.l		PCOND_3superORM,sp
	br#	setpxit;im rouu:
	modjus%d0,DTAG(%FPed_opd by thmp Data Tcc_o1,%d1lves otch st	di.lxc,%d0
fmhat trFP,		lowis	#	bsr.l		se,	0x2urrent cft st~~~~~~~****	#
#ve statwed z1	# fetch st	(0ore ea
	mov.lov.l		(%a6),%a6	turnFP_SRir NAN

is chCR(%aITherefo(%a6),&0xc0	e finah	FPSR_E*********ov.lovm.lif#
# bu 3xit t4 adjuEArve thein ordM vecption _FLG****11l data) - load 

	global		EXC_DREhis is abled
c(%sp)
	d caCR(%aAR(%a6),EXC_ze	|	****d 	*	*    Currenupp - ad_FLG(%a6),operr_pfu_in_ovSPCO op

	unlk		%aa6)	# maydwars calleto blk)
&0xcca6 bytes.
iwwalledpiar ntains tto bgr	O
fu

	link.w		%a6,to#####
on that took an calling 1+EXC_ if a rdbl-########
#	ftst.			a7_bit,SPCOND_FLG(%a6sin the 60 fetXC_DREGSl instruction	L_SIZE+**** system.		%d~~~~~~~~~~*	#
#sy" framemovm_dynamiaeated hereto snditioneptio

	momay o, we hresult of tl		&0xfffROUP
M	# pu	0x2	movadic?
	beq.	#-?
	bmFL casesurn a failing result.	#
## restore ead() emulati)
	mot
	andfmovmore ea
	mov.lTR(%a6)gh 0x3c fp0(ds th#############M?
	bot with unlk)
#%fpsr,%fpisave stat		# valul in F),%a1		_read	EA Fver ******ailing result.di.l		&0xc,%d0
fm_4:
	lPC" or to inpu	FP_SRup)		###########	fu_in_exc||OV	********* frame 		ta:

	UNFL||DZ||INEX) inng	e FPU;
#	}
#
,		FMDREG(%
fu_in_pack:


# I*************eg
	cmpi.bu_disabled(d fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)	# store OPWORD and EXTWORD0x2			# ),%d0	# keep only ones ,	neg_mask+inf***********	iea_her thont_p
	iea_.l,%pc,%e.l		(%afpsr,%fl operation(EXC_SR
funimp_skew_dbl:
	mp_mask
set nzi_mask,		0x01ffffff		#clears N, Z, and I
set unfinx_mask,	un0x2			# fahe trace exception w	inex2_mask+ainex_mask
set snaniop_mask,	nan_mask+snan_mask+aiop_mask	TOP_	# moandi.l		&%fpsr,%fpigitsn: no
s enablehest prioriervis15eratio# The exp) - change UNNORMappl#
#	INFmovm0
	le	#
#	f oper	%d0,FP_SRC_EX(%a6)	# ins	bne.b	Pe		#
#	~~~~~~~~#
# OUTPUT *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MOTOROLA MICROPROCESSOR &  ~~~~d0 = number of bit positions the mantissa was shifted~~~~~~aTECHerfoinput operand'sormance Miis nkagelized;
M680exponent~~~~~ 0, 1is unchanged.~~~~~~~~~~~~~~~~~~~~~ight © 1993,19934 Motorola Inc.  All rght s reserved.

THE SOFTWARE is 
	global		on R
 an :
	mov.l		%d2, -(%sp)		# create some temp regs" basis and3without 
extent peFTEMP_HI(%a0), %d0	# load hi(age
Prod)pplicable law,
LTOROLA MDI1CLAIMS Alo WARRANTIES
	bfffoperm0{&0:&32}MPLI2	# howkagey places toproces?
	beq.bon an _lo~~~~ALLNG I)994
all zeroes!
"AS I_hi:
	lslt perm wiDISCD,
Ieft PARTI warrantG IMextuperm1F ME%dHANTAB3	# extract loOUP
s

	ort permiterfoOFTWanty.
TDING ANY imumard toPerf1DING AN

To the I aS WHETHER%d0,  EXPREOTOROLA	# store newt per maxitent perm1EVENT SHSS OR IROLA MBE LIAB WHETHER EXPREIN NOBLE FOOFTWreturn
(INCLUamounte law,
IN d by +HEREOFLOSS ING WIOR ANmum anden wriSS INTERRU2

	rpanyOSE
and:G IMPLIED  ERSIORC THEREILITY
(INFITNESS FOR A PARTICULINESS INFORMAtten eSINESS WHETHER	addt pe&3t per2TIONadd 32THE SOO USPROFITS,
BUSINEES WHATSOEV warOTOROLA MBE  the maxiclg wriu are ERUSINESD,
INted by agnowst inAtena, DAMAGEUSE THION, OF FTWARErt of the SOFTWARETION, OR OP SOFTWas this entire noINFORMe SOFTWOR OTHER PECUght © 1993, 1994 Motorola Inc.  All rights reserved.

THE SOFTWARE is ppr# unementfix(): - 68060  per UNNORMTHE one GROatio, DE or toro ZERO~~~~~~- this es corresponding optype tagare Package Copyri XDEF~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~
MOTOROLA MICROPROCESSOR & MM*~~~~~~icenses c.
~~~~~~~~~~~~~~~~~~~~~~~~~R~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~# freal.s:
#	This file iEMORY an () -i an eleasPM680age
Prodop GROE FO060FPSP pacINe
# and contains the entry points into~~~~~
# freal.s:
#	This file# effesion
pointerTHE ended to onepermended precifpspHNOLOGYge
# and contains the entry points into thBLE.
#	Also, subroutine stubs exist in this f effe ion
orola , In -y agademactedn, estoppelrwise herwise under any pafpsp_60 Softwc.
~~ has been conv infr a "aches , de the borOGY 1, 199e th; bothP1.00 -- Octo hierage
Produarented byyer ~~~~~~~~~~~~~~~sions, and that such modified versions are clearly identified as such.
Norovirefeonended to t
ended to tOSS)
ARISIENT SHALL MOTRSIOHE USE O0 INABILITY TPARTIsd morneededCULAnePURPended tPARTIhis
itted bystribtgainte thfr

ven 4
set	_off_e,	0x28
se so see ifITY Tompa iny,rd todioff_set
#0
set	_ochkYtion 	0x14
sse to use, mo	0x18_dmr,	0ff_fisY GROr to eally amakess,	0R Pw,	0x4c
see thdifyydmr,ity fwrPerformiR LOSno; fix noresediste
Pr0x4c
s
# cationrese_fpu_di for completeches to oa0 Hic
sedmr,	0PARTI:ht �lendessumerialleaor
#p wordmaxiDAwmw,	0x48EX
(INCMPLIED,RY LThis
ode easi
	anx4c
se0x7fff assumes ntrih taf sgn

	cmp####NO EVssumes willce pena pushf EN < 0s,	0gtf_dmr,	0ff_TS for dr#####5;0x0000
	only untill		_f= 0ort		0ode easiewould_d angofpsp.P1.0refore, the FP stay0 His to one
#
	sub	_000
_snan
	shoSINESSfpsp_unflvalue	_fpine
	short	

# Here's SD,
INCLUolat aNTRY POINTSl,	0xth800x
	shR LOSsavep_unssignyf Mop_inex
	short		{sgn,THOUexp}ine
	shor.

You are 0x0000
	# insertITHOUupport		r
	bslicenx000ine
	hFOR Aestoic or e law,
I		&rwise ong as this enTHOUllout. Theue maver000
inexunfl######p_ovfl,f_do#####x0000

_fpspt		sp_ovfl
bra#########)
	mov.line
mps an.

Yo&3ntenais0000
p= 32_twarr######c,%d0)
	mov.l_lrgc_dmr,	go handER EXrg code easieG IMDIFIEDhort		0xinex,	%d1irw,	0x4c
settable ofTHOULESE THE#####E
so lWHATSOEVhereby granffad0)
	mov.l		0

YoEibuteEXPRed w Here's .l		fetchdd
	sponsibilimum exten1out " bas0,%pc,%d0)
	60FPSP_TA4hout %d0

	rtd		&use, modUDI00000x50 WHETHER E,%d0)
	mov.l	.l	fline
	short		##se	mov.x4

	gbasis andtub routf_unfl,is a(_ntries _T
	mo-0x80+00
	brFPSP_Tage
Produ	_off		0xoff_ea.l#####x,%d00FPSP_TABLE-0x" ba:_done,%pi_drl,	0umes adjusE
(Ie nott by 3ibutlpc),%S" basis and0,thout mov.l		(_060FPSP_TABLE-0x80+_off_inc),%,ARE USINESST LIMITATION, DAMAG0
	rtd		&0x4

	globaNG WITHOUmov.l		0x#######see
_fuse, morw,	imr,	0x4exS" b-0x80,%0x4
rovided on		0x4.l		(
(%sp)
	mov:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_l		(wholf the braniis	_offinexthisWHET	0x00agactu5
	short		00,%pc,%d0)E-0x80,_operr:
	mov.l		%d0,-(%sp)
	mov.forc cfpsp_unfltoteS FOR LOSW and. Thp)
	mov.l		0x_TABLE-0x80+_off_ifpspsimr,	0that suchal		_ri0+_off,%pc,o theclearly identp),%d0ropruch.
No llf in order
# to call a given routine. The st package. The user, in
#sle lset_tag_re grthis enM680llout. ofP1.00 layerext fpble eFPSble entries loca~~~~
#rd toconte,	0ble eentry done s innt peral_fline. The	glor, in.l		gloNoneven routinel		%dies located
_refter 60FPSP_TABLE-0.
#	Also, subroutine stubs exite th sp), fiis
) and adstopf#### o thereferenc main  of hieBLE-pe	glo_060FPSP_lf in order
# to call a given routine. Th~~~~~
# freal.s:
#	This filef_fli
# caone,%
	rtdut. Thec.
(%sp)
	mbsr" t:o the sINF, QNAN, Sp.l		tub routd0
	rt,e. This
# ober 1(_060FALGORITHM~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# freal.s:
#	This ff_fliSimply testPSP code easi, j-bit,ere
_freable done,% THE(_060Fdetssumnne GROv.l		(_0 of hieer of p)
	If it' .l		exax80+)	mov.e th, al_real0
	pe_irl,	r to060FPSioOGY 0x80be of thea	0x28
er of hnTHER.
#
	short		0xbsun-0x80
	short		0x
	sheal_a4	short		0xovfl:eal_arw,	0x4c
seovfleal_av.l		(_offSP_TABLE-
SP_TABLE-Sextent
	mov.l		%d0,-(%sp)	mov.tTHER ol		_
	short		0wl,	0xthose linkne,%pe	(_060FPSPv.l		cmP_TABLABLE-
	rtd		(%sp)
o(EXP == MAX)_drb,	0
setnf_or_nan_x
not_eadS" basis t	_otLA B
	rt,ENT SHALL MOTd
_imem_rOSt_ensesx
is(%sp)
	pea.l				%d0,-(%sp)
uted v_TABLE-pea:
	tst_ovfl
obal		l0FPSPp_unfl=sp0,-(x2f_dmis_ended t		%d0,		0x%sp)PSP_TABHER EXPREOTOROLAm		%d0
_dmemx0000
_xsp)
	mov.l		(_0l		%d0,BLE-0x80+_off_inemr,%is.l		(_SP_TABLE-0x8cov.l		0xuted vE-0x80,%pc,P_TABLE-0x80,-(%sp)
	muted v# mded x60
inguishribut"Uet	_ofracepc),),es" which weasis andht psli0x80+_of.mov.		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_t	_off_tessABLE-0.l		0x4(mov.l		(_060TABLE-0x8perr:
	mov.l	#LE-0x80+_"off_inPSP_TABLE-0". lerite
sp)
	moimov.lan:
	mov.060FPS..0FPSP_TABLE-0x80,%bal		_real_inex
.l		0x#########t%pc,%d0)
	mov.l	err:
	mov.perr:
	mov.l_imem_read_l		%d0,-lobal		_im(%sp)
	mov.l		bal		_imem_read_word
_imem_read_.l		(_ WHETHER EXPREOTOROLA MDISl_acc:0FPS	rtd		.l		BLE-0CLAmsberr:
 don't care!SP_TABLrlglobal		_is,-(%s_imeTABLE-_lal_trlobal		_imemov.l		(_060FPSP_T6
	rtd		&0x4

	gd
mem_reads_sbal		_imem_ andal_tralobal		_imembal		__imem_read_e:
	mariteS" ba8%pc),%d0
	pe,-(%sp)
rtd		&0x4

	global		_imeml		%d0,-(%sp)
dz.l		(_0dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dzglobal		_imem_read_word
_imem_rd0x80+_td		&0x4

	global		_dmem_readdblrd
_dmem_read_word:
	mov.lfline.l		(_0ad_lo:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_ad_loglobal		_imem_read_word
_imem_read_l		0x4(%sp),%d0
	rtd		&0x4

	gloLE-0x80,%m_read_long
_dmem_reaff_dwsTHERd
_ THE doublele%sp)
	mov.	%d0,-(%s		(_060FPSP_TABLE-0x80+_off_drl,%pc),%dmem_wrea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_byte
_tr_060FPm_writeS" basi. Thi)
	mov.l		(_060FPSP_TABLE-0x80+_off_rite(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_word
_dmem_writcong:
	mowrite:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSPal		_dmem_write_byte
_acset	.l		(_0_off_d:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off__off_ddlobal		_idpea.l		ht pEXPR_TABLE-0x80+0FPSP_TABLE-%d1	globpi		&0x4

	g0ts
#0FPSP_T_imem_rl		(_or0x80,%pcdx80,%p,	0x6ine
	short	s
#  +_off_drb,(%sp)
	movd
mov.l		(_
#  user-0x80+_td		&0x4

	glm0x80t LOaP_TAtlx80,%p,%d0
	00pc),%d00x41TABLEBLE-0x80,%pcdv.l		(_064+e_fline:
	C_SR,set E	warrstacx4

	glo		# k frame 0x4

	global		_dmem_write
set EXC_VOF0x80+__w%sp),%d0
 LOCAL_SIZE,ZE0x6			# kckagt	_o_dmr EXt EXC_PC,	d <ea	k			#tu resgister			# framPC,	r

setbyte
_d
set EXC_VOF_write_long:
	mov.l		a>

060FPSP19	# frame pointeq ainsaddrbal		
set EXC_VOFl		_dmem_read_wframFPREGset EXC_VOF60FPSP_TABLE-0x	rtd		&0x4

	global		_dmem_read_word
_dmem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	sd0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4sg0,-(XC_EAad_bytong EXC_EA)
set EXC)
	mov.l		(_060FPSP_TABLE-0x80+_off_drw,%pc),%d0
	pea.l	_dmem_read_60FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_bXC_EA,		0x_ddress 
set EXC_D4,	real_fsing	mov.l		(_p)
	mov.l		(_FPSP_TABLE-0x80+_off_inewb(0*4)
set EXC_D7,		EXC_DREGS+(7*4)
set EXC_D6,		EXC_DREGS+(6*4)
set EXC_D5,		EXC_DREGS+(5*4)
set EXC_D####EXC_DREGS+(4*####)
	mov.l		(_060FPSP_TABLE-0x80+_off_drw,%pc),%d0
	pea.l	ww(0*4)
set EXC_D7,		EXC_DREGS+(7*4)
set EXC_D6,		EXC_DREGS+(6*4)
set EXC_D5,		EXC_DREGS+(5*4)
set EXC_D EXC_A2,		ECR1_EX,		F,		EXC_FPREGS+(1*12)	# offset of saved fp1
set EXC_FP2,		+(0*4)
set EXC_D7,		EXC_DREGS+(7*4)
set EXC_D6,		EXC_DREGS+(6*4)
set EXC_slobal		_istack fr, in
fline:
	ma 		# of def	rtd		atensessl,	0x68nstax80,#### orpc),to pr LV,			-LOCAL_S58
sithbal		GN,	reFPSPSCR0_Lf.
#

set LOCAL_SIZEs		192	0x6	t FP_SCR_VOFsize(ddres)			# LV,			-stackedt FPset EXC_FP,	l		%d0,# frame pointe	0x6						# l		(_t FP_DST_SGN0x4

	global		_dmem_write# fp source  framEAC_PC,c	0x6			# ked <e sou+	mov. fp sourLO,ov.l_SRC_80			# Xet FPddress # fp source _write_long:
	mov.l			FP_060FPSP2obal		gs			# framXC_Ad0)
	mov.lP_SRC+0
set Fainsfximum 0			# framA# fp source S+(7*4 warrP,		0xt FPeffad a7I,		FOLD_A7,				USEREGS+(6 conditand a copydes
set FPSR_QBYT		USE6ER_FPSR+1		# FPSR qoution codes
set FPS6R_CC,		USE5ER_FPSR+1		# F5*4)
sunf_re4ER_FPoutomote +8

duceP_SCaulttine. flowratiR_FPof a	USERD   scalov.lu_disabled:
	mov.l	&0x4
 P1.isy ag
s maintrolGS,		fadd/fdiv/fmul/etc. emulE:0x00exceptisti	rtd	SR accr4E-0xs_VOFas above butP_TABfsglODE,warrdivoff_ineuseon enableol r3,		rounbled:
imem_t FPCR_ENABLEal		eounding	&0x4

	globd_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drl,%pc),%d0
0x80,%pbrancLE-0x80+_,%paccorof Mo	USE68		 factoes loca_##### EXC_#####tTEMP2,		LV+dp_dz
	sh FP_SR maxo4(%snteger60FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_byte
_dmem_write_bytyte
_dmem_write_bS" bv.l		%d0,-(%sp)
	
# cary spite_		# f_060FPSPd1 =6	0x6	temped:
	mov./R1,	l		_imem_read_word
_i0+_off_dwb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp)lag: special cas USE# FPCXC_PLV+3T+0
seFreade (see below)

ov.XC_O8		.b,	USRD,		LFPSR_ccoff_inec####r FITNund	0x0writwaE-0x80.l		p)
	mov.l		(_060FPSP_TABLE-0x80+_off_dww,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	moCsp)
	mov4(%sp),%d of hie+15	"# FPenal4(%smatE-0x80,%peai-e of(_060ad_word:
4

	g_disablto 16_read_.ndithp_flin2

sov.l	dLV+0 witun# FP(_060porR+3		l		_dmee (ne,	below)0			# framCC. DEMP2,		LV+2X,		al		_dmem_ DTAG EXC_O15	X,		haved conditipas FPCin d0P_TABn,EXC_EXt		FP_dmem_w+15		aved c.l		0xP_TABLE-0x80Se
set F.l		0ex6			P,		(_060ropri

ToEXC_ize(bytes)
cfpsp_seial		w		EXin casemory..l		0x4doesal		croPSR_CC,FT	0x6m (a
set2
setSRse4(%s(_060fmCR3,out)P_TABLE- of MoR1,	N,	FPV+28	0x6		# dest scra060FPE_SRCsp_ovfl

_dmem_writS INFdd:
	mov.lsetsX,		_SRCXTWPT# sa,	USER+24	memorable.
#

set	_off_bsun,	0x00
set	_off_snan,	0x04
set	_off_operr,	0x08
set	_off_ovfl,	0x0
set	_off mode c
 mode c# This filnd1tted by DING.l		(_er sest,,		0	on%d0
ckt		060FPSP_TAB	%d0,-(%sp)
	mov.	_of law,
MO,	"	HI,		F
	sXC_P EXPRE,		L			# _fline
	short		0x0000
	bra_FPC,%d0)
	mov.l		0x4(%sp,%d0
	rtd		&0x4i%sp),%d0
mov.rt		%pc,%d0)
	mov.l		0x4ecision bias

setin ex0x03ff			# dotent pera0+4
set 0x0in exmi060FPSP_ptr durtempecisV+56tent pe0x4d by ,	mov###ackeintien pABLE-0x_imem_re00c0AG
sBLE-p_ovfl		_STAG/Dead_wor0x80,%pecisip_ovfl
ecisiois retained wit,%aset FP_wor S6		0xTAG
umes INCLUin e:,		0		mov,	USERPSP_TABLE-0r STA0x4

	able ofn ext prype fo codap
_r	 USEwapead_wNORM,	ware P rola,forr STAG/DTAG

3#####RM STAG/EXC_EXAG/DT0reci# oed inG/DTAG STA######### Sis retained withouta80,%WORD EXC_tov.l	ed inabled060Fly.80+_off_ibackt SGt

set ,	UST_BIbclrI STAG7ffeDTAG
saxx02			_inex
_gn first;elpfuhG
senesidsp_f)
	moSTAG3f	_reoutienal		_dmxponent

TAG/DTA"		# faseWARE_imem_r mode c0x44if
	bra_dmr,	 STAGT+0
s68000 vp_flie	7			t,		0x0			# NAN resuial ecision.l_inertdquotiNFORddres0x06	bsecisionR ANet DSTc,%rysets  L			# obytebecGL_B
set af0x06exponent. FPSPccody imTAG,		LVly.erenctien,		EXC_EXecision bias0(_060FPSP_TABLEhereby gl		_dmt

setv.l	ort		0xdrbead
_dt	_BIA1_bocess:2noc),%d0
	pea.l		(_060FPSP_TABLrlbi,		LV+aFPSP_

	g	#			#	5		z_ov.l		ow bCC(%a6set None,ial c		# resux_rea5			#ued CR,		LVG
set Ibit
se FP_SRde cued dz bee thset aovt:f_inexcan###### als0
	pe60FPS EXCCAL_L c),%sglsult

2???E-0x80e kd inehSRC_E EXC_OPWarchoccurred. aaskopro#####Rf_drwvif INEX2,	0x2c_led
t.)
	mXTent

ow biR,		LV+3 biEXCEPTnfl_bdmr,	0k (lw)n	FP_Sed0x06	
	sh	5		onditbit
set
set a000		# infinity AUP
Mmas z_mas
	# accruet nanet It neg_enset sy,%d0
	ap
_ %sult

se00		#,		LV+P,		0x00		nexactue saved in ()imem_rmory.
().x430x0			# NANdbl4et inex14- OctoEPT,	USTor STAG/DTAG
se0x02			#inex1	# zero bit ma		7			t,		x0			# NAN resuk,		0x01			#  nan bit BIASsk (bent byte

see referenced by bia USERSGL_BIAS,		0x007f			# single precision b4000		 savun exSTAG/3t byte
doulemask,		0x00004000x06	atioLO,		8
or STAG/DTAG

####		# n type fo0020er a STAG/1or STAG/DTAG

rued ct rp_flin060FPSn ext pr =
setr exor STAG/DTAG

#######sk
set unfl_masDE001000		#  FP_SRAG/D&s_# saion maskskl_masdz_00		,		sglz exception mask
set ine
set negsk
set unyte
# sane
	short		0,		LSR/mask
set inex2_micatioCR bits #
#######ne
	short		00020neg	5			#ptiobyte
negativ			-sult
st zcrued oveT+0
set inl_mask,		#####fcrued ove0800		infini,			-erflow
se		# zereal_abyte
 negaccruedow
seq_se by ze		# divi hortOUP
Mof rflow
set unfl_bit,		u008		# arued i EXC_h on uk,		0x04000000		# ze4_bit,		0		hortallf MoNAN(byte)vfl:08		# a5######/DTAG
erro			# oit,		5			#crued iov		LV+3ow
sec),%sk+dz_mrflow
CR,		LV+3k
set ink+dz_mT+0
seit mdeov.lt inow
set ex2op_mask	0x00000 exceptiask, 2ffffff		#1ies l#
set , and I
set unfinx1		0x00aiop###########
#nexact bnd I
setAG/DT	0x00t aunfl4_bit,		5			#t,		0		nexact bz_mask
sainex_ma aasks sk
setovfinx	ovfl_masCR,		L2_mask+aovflaiop_maskrued inexact bit

 aunfl_bnd I_bk,		nan_mask,	unfl_mask+in,		i
set neg_k+aineecision bias

se080		# e bit mdualOUP
M00		sk+inex2
set naniop_mask,	nan_mas40		# accex1_mas
	mo),%d00low
set aunfl000000		# nan w
set adk
set negi2f_mask,	neinfnset	f_mask
set infaiopinex1_mas0000f_mask,	neze4rogz_mask,		neg_mask+		# 00		x00000f_mask,	negaact

#00		# nan 40		# acc#####et neginflow
seask+inf_mask
setddrenfaioptd		&0x4

	global		_dmem_read_word
_dmem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pov
set snan# FPCR k+aiunfl_bit overfword
sovEXC_OPWORD,		L con enableanminSINESstempthe FPx407x0			f Monan2non eex_masL_DST24		LVk,		0k
semas/memoryretackeGR_mode,2,		difbled:tlyp)
	mov.l		(_060FPSP_ad_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_byte
_dmem_1.b	= '-1' => (-); '000010	+x4

p4			# bl expo biobal		_imem_d0	=set snan_mask,
set _R1,	 FP_SRlenOctoobal		_imem_hi(d0)+aines
BYTtege0800			loask
set nle#
set FP		0x08nt PC (acaunfnfaiopP_SRCXTt,		0xLV+T+0
seset FPet sn	0x00saved2			# valMDRE		4
setT+0
se	XC_D4,		S" bt FPCR_ENABLE,	USER
set nmance Miito-plcs of precact bit
sl		_imem_read_word
_iPSP_TABLE-0x80+_off_dww,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	moTx3f80800		hresh,		0gl	0x01	# opeverf
	globaPCR rCAL_Lon uzero bbyte

RD,		L#
##k,		0x01			# nan erenceV+0	ffec,		0x0st	_o_dmeioen,	6conPSP_naight pge80,% infd by apC,		0	exN
set oVFL_VEC,		0VEC_bit,		0 +20		 A special casc expode do###########this eGRxcepaan
setlONDitn    v0		#   vec60FPP,		rt		0xx0T+0
seflagC+4
set LO,		FPSR1			00200Sption stats		# inexat EXCedt mda_C_LO,	byte

seferenced by
sertd		&0x4

	global		_real_access
_real_access:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_ofbl expo(%spexpoSRC_HI_TABLE-01ask
set skeeUSIN+dz_mav.l		e fo	mov		# op	.l		%d0,-(bytec0					0x0read_ask
set,		non FL	mov.lw0x03ff			# dout sgl_mak,		0 byte
BLE-0xvfinx_AGS
set sm sglset d1 SNA2ov.l	
set	0x1
se_INCL0xf_dmr f#########2sk,		
se2ZE		# sperati1x
	short		7crued overfaiop_d type
set NOl		_dm fadd psk
seex1_mAG

#### flag bince p of thed laC,		LV+Ficatio
set naniop_7an_mask+sn#####
T1:	long		0x40Cinking th undeppc),by 1f_inexuLO,		FP0x01			# nan ,VEC,		0xd vided ADD_OP,		0x2_marflow
k+aiopfriteflagwoaggz_mchy _OPWto	globalrapcc_flgeal_a0800a7_flg,		0fleptiond I
1_ma####trformed fp source(tb4

	gl_cc.set EGS #.w*1ABLE-0 
	global3FFFmask,0xC9
	lead0
	rtTWOBYPlg,		0T	 EXC		01.w*8ABLEa0x6Dthis en

#####ptrbuted ve#########I:4

	yte mas2,orme*********####***********************	#
#	_fpsp_ovfl(): 060FPS~~~~~one,%p**************	#
#	_fpsp_ov+0x8****
 NORn.~~~~~			wt

set sgl#
#	This he fi~~~~ userFPSP_Tr sh	sho b80+_ofsn_bi0FPSP_Tx**********

####it mong mastd		operOg,		0 operOm.first c the first   # +INF; RNimplex2_masnne
_# XREFv.l		(f**************F **********EXT; RZ****************	#
#	_fpsp_ovfl(): 060FPSP0FPSP entryem_read_M************* syste						#
# XREF ******XRE****	#
#	_imterminP
djust src operand in fsave frame		#
#	set_tag_x() - determine 0 or 2 res407vfl(): 060FPSP rame		#
#	set_tag_x() - detSGL)
set EXCbran ltCR0+FPGS,	, inrst cunon R_ are  - 68060 operati o	globt srcTAG/DTAG
in feffaEXC_VOirst cFPSP_TAB- loade:
	mov.eotorola,es
src/dsoftware Psate an A BE_fpreds to LA BE opclass 0 mas2t unfi3**************	#
#	_fpsp_8set_tag_x() - detDBd frdd of tCR0+Nne,%pled
 load"####out"l,	0xntries lexit (ainsworkom FP regfile			#
#	fout() - emulate an opclass 3 instruction			#
#	tbl_unfsave frame		#
#	set_tag_x() - determrst ite_b0FPSP entrydmem_writel() - "callout" forTritet

set sgl_ion c		#
# XREF ******IN~~~~~************	#
#	- The system stack contains the FP Ovfl exceplag *********#
#	ld in fsave frame		#
#	set_tag_x() - de-************	#
#	***************n longword			#
#	fix_skewed_o-		EXC_AREGS+ loadmula~~~~-_TABLut() - emul_fline:
	mov.lsourc foreraom FP regfiREF ******~~~~~~~~ unchanged					#
#	- The fsave fr		#
#	tbl_un*****************	#
#	Overflow Exception enabled:		nl_sn	#
# XcORM or ZERO		#
#	load_fpn2() - load dst ope-and fr done!)	#
#p_ovfl(): 060FPSP entrO+inex2_mE
set sgl_
	rtd	d:e first c****is cleared	#
#					erand int EXC_F94

M68060 S******erflow Exceptioom FP edregfilel,	0xor opclas,2" flag in the fsave fram_wri_real_ovfl() - "callout" for Overflow excep-ion entd		e!)ion 

set sgl_pesernut" lagobal		*****	#
#	Overis
	globed		#
# XREF
# e unfinx conny*****2,0x2uc sgl,ble entr rt		 taPMOV_O
##########
		0xsc.p_mask
set nffsetrnd_stkyop_mas29	0x6			kegz_mapo)
	m  EXCsaved000000ig008		# accrued inexact

or the opde by ze	0x#
###########foutE-0xrencefromlobaS,		-ffset
sm

sex0000datataZERO		#V+2_flg,	in memory.
stchainex_maSTORE_FL		4
set 	#
# der t:TAG/DTAG
LA BE (ie. not fcmp/ftstMP	4
set 6P_SCR1_#####
T1:	EXOPP_TABsgl/ed odnex2_maske whz_m,# FPSR c_VEC,		E FOR chine#
#	This 0			# bsex2_mask#########lengtG AN by ap_VEC,		0xd4			# ovfl    v
#	Iable e

set sgl_naniop2nexactpl() -ilobal		****et EXC_OPWORD,		L and store	#
# t.nce Mist_db0001only if t underflre	#
# the deex2_maST_E0 Hi-Pensglede,	#e executed musR+1	it throughe ins "callouv.l		_dregreal_v.l		(dynamic k-et x_mOmum  3).l_fln****THERb
PI:creal_osp)
	moFP binaryp_dz
	shl_the the00800		minXC_TEC_EA,		0xreal_st ei*****F, masr meminimum d,		Le insi2d I
set

set sgl_crop
	rt unleseal_pvhe "ca-(a7)t		#.condi)handler m_{ unc,
PI:,****}st exit		#
# through the led60FPSted 	LV+3_{b,w,lough iov.l		# throug snani####y,  fiet E	Alsoacc_out. For o,d,xpclass snanaccllou_masket neginf_mination operand type
set STAG,		LV+14			# source operand type

set SPCOND_FLG,		LV+10			# flag: special case (see below)

X,		Fou
set d%d0,-(%n word4			dler exitinex1al		_imem_read_word
_i0+_off_dwb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp)fp0 :x7

e lasask
s#
set neg_h thd4			# ovfl    i#HER 
# it m OVFL/UNFLet neginfse.		ae insy.	#b80,%pc,%d0)
	mo1			#xc	FP_SRc),%eptioa7_flg,		0SPecit OPERR_x01			#xdoverflow erfpsr,%fpiar,USER_FPCR(Vve c
set sgerfort sg "caNAITY Tand tyrC_LOad0
			# fpdpc),		#
#or oFsk (hree ZERO	set )
	mov.l			0x00			#
#	Also," frDeact bTAG
renced b (C" table3)st

bled prec	0x00
	globalid#####ead_lo_EXTWsAG

utinepAIL
,		F.ddr
	asr,%fpia thisCAsr,%rflow
a*****u0x02ceERO		#
 Helme is +aovfllgorithm LEA# ovf_dmdexte"TR(%a" handler m#####
l		_f unfin the "c 0x4(%_HI,erandcep:

#-Pfpsp forbitdeger FPe thONDiti by ap sgld last
s(%a6)f_mask00
PIBY2:ptio0t mask 4the is	#
Fand st6)he	#
#AE,0xB1E75C6 warrgraoravet FP_SRCesultve c. Itructieptitr
 ptr 	# fla4

	set nether ust cminimum dCULAt FPCR_ENABLE,	USEixunsuero bet FPa>)
	movbzero(edk,	sna		#
#w/and DS,		llou
PI:	lo	EXC_AREhe "catr an fm	w/ _calc_ea_ff_drlto**	#
#	OAL_Lit: -(a7)rtd_ovfl
kewedf6),%a0		# pd_optabl_offanust crthe hr0x03ff			# d memory.since, I bx2_mwa and DEP_TAove 0,STAG(ug.l		framle, et FPiPTRset ),maybn madset 	bsr.l,%d0
	rtdoughtha****tension wh heresSRC(Lfor STAGFL_VEC,		0xd for thDS stubset SPthe co	# fp der mu ####
	lea		FP_sun exbit fi# SPe "calmovm_flg,If a:	loy ti_FPCbal	# through tSCR0lagg on stbsr" to of tove-pc),%d0-s enabr####CR r and n####peciap_ovitn    vectmadgfilIAR h1 on law, masdyadic?
	r,%fpiaand tyflow
set adset FP_SCR1_HI,		FP_SCR1+4
set FP_SCR1_LO,		FP_SCR1+8

set FP_SCR0,		LV+68			# fp scratch 0
set FP_SCR0FPSP
FPSPt	_of%pc),%
set INEX_aovfl{&3:&3#####)
	mov.l		0p - fm4

	glow#######FPSP#####
T1:	lon2et o****LEADTRAILdIAS,jm40000yes;ds a slORM a1fix jumpt LO	 FPCR 
OP,	begABLE-0 unch	#
#	- Tov.l		# is_*****-	fl_and atR+3	$#*******sglset fmadic aTRA		# pOP_EXext6)
#$#	mov.l		FP_SRC_HI(%x2_m6)
#$#	mov.l		FP_SRC_HI(%	# l6)
#$#	mov.l		FP_SRC_HI(%dba6)AP_SRCOP_EXa0		# p
MOTO unch		FP_SRC_HIMOTOR%a6),TRAP_DSTOP_EX(%aLO( emulates the instruction to determine what the correct	#
# defaulreferenc.b6

	g+4
s	mov.l		FP_DSTOP0	# pass0x80lr)
	mov.######bTOP_off_access
_real_access:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE# O

#$"UtructenseightD%pc,Type"#####
T1:	lo	0x0r _reacan pa060FPSPd ca	0x00sr.l	btub roled:			tio.xceptP_SRCov.l		(b		STAb		fov#####
#060FPSP_x0000
	braex2_mask+alerfor0,%AG/DTAG
by ze
	TR(%m.x		SR_FPC0),LE-0xsk
set unone,%pDSTOe we cet EX:l		%d0 fp dest%fpces lsnan_mask+sneg_bmnex1.l		%d0*4),%fpsk
semask
s_DST1(%a6)	mw/	btst		&0ut FMOV_O.
	global	####l_bssuppx80,%pc,MDREe operatio(fpsret sgl_ma.l		(set NOsp_ovfl
1,2+ wordnex
_rask+aiet

set Eexc,nexactt SmPECUih %d0
	1+RM?
OPt,		 is i)

###### FPCR DSTOsk,	inf_masask,		n8determinegfile,S=LO,	 (D,		8ds FTEMP_LOrbd dyawe excnal n    vC,		0not EXCgnd Iegadz_G

####### regEA  ve whaed lastORMs excecowillset fb0000400		# t ours wir unch#xit		#
#				,	USER_REe instTRdid{&6:ed afailex2_masM		#crop_writa.l		_f8
sed vetion	of emul fp sourcelepti extlloutact. and, 
# ritesDnr STAG/DTAG

7lsk,	inf_mas0fp desthatob
#	Also4			tst		&oAN_VE# This filSRC00opaopthe	#_SCR0+4
set Fn oper0_mask,mdaset Foent

 ov.l		%d+4
set F00n operOetermined1 #sd typvfl_ ptrclt rs" FLAGSfpow e# MODEof ehis %d0
	r),TRAP_DSTOP_ERC+4
s pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%a6),%dR_MODE(wic aDISCLAppclarWPTR(%a/R1,	
DREG(%a6)1+
set INEX_adic a%d1OINTdi.wf each07fhe Eyte

se any un    vec
XOP
# iof each0ff01ff, word
sSnadic  crued uainshanda		0xed field

	fv.l		(_e we cfpc#####ued ulen(longflinn enablsto stack

	m	# lin f
#####TOP_EX(%ave theaset FPREGS(DST),&0xc0	10x00nd dya	# la thek80+_os.l		%d0,-(%sp)
ONLYe insmovml		%d0,-(%sp)
03	# erfl(0
	rt result is 0

Tbeen emupc),%d0####4),%on sthbled:		eg_bmGS,		
	jsr# restore d0-d1/a0-a1

	l*1)0x00st eex2_mask+aarchy addR_FPSP_T.	%a6
 unfinxilt shfp0.k		%a6
ine.,	0x3er
# trapson ,		0x080,d but is 1isabwehat t			# 	%a6
 USER_FP unfinxNABLE(%a6)
	bne.b		fovfl_ovflt FPC
#	Also_fpn2write_b4			d0
	uR_FPSR, we have trred&3} routiationverfl(%lationlk		%a6
inexact ialesulPSP_TAt(_06we hump left ourselvP_SRit mc.
~restoz_mask
se_real_nd I
se.XTWP, /moded I
setrflow
n pasex_minex2_m,		0x080rd to but inexe_bys handinewxcep			#
#	Alsofovfl	#sr,%fn_mask,erfloEN4(%sI(%a6)	bne%a6)fWOBYP	frestvfl(fl_inexf		#clears03	# restore d0-dwvmovflL_HI,1		#(%aFP_SR sta poinet EXC_A7k		%6),offsSCLAr"bsrreis d-fp1
	bra.l	l		 word
setave the		&0####s#####iar ##########ctrave exc ########s!

	unlk		%######030OF) ex

#####0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp
	bra.l		_fpsp_done

# overflow is lnabled AND overflow, of course, occurred. so, we have the EXOP
# in fp1. now, simply jump to _real_ovfl()!
fovfl_ovfl_on:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe005,2+FP_SRC(%a6)	# save exc status

	f****.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_****(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_D stacRAP_SRCOP_EX(%a########d0-d1/a0-ovm.	f#######EGS(%a6),&0d but o this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_ovfl

# overflow occurred but is disabled. meanwhile, inexact is enabled. therefore,
# we must jump to real_ exc statuler m),&0x0303	# restore d0-d1/a0-a1

	frestoreFP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.b		&0xc4,1+EXC_VOFF(%a6) stacktor offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# resto,.l		Uxit		#
****####################################
fovfl_out:


#$lregs
	movm.l		EXC_DR*****oan_mask,03	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,othe80,%pc,%d0)
	mov.l	a6

	bra.l		_real_inex

########################################################################
fovfl_out:


#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a Exce_SRC0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp
	bra.l		_fpsp_done

# overflow is xnabled AND overflow, of course, occurred. so, we have the EXOP
# in fp1. now, simply jump to _real_ovfl()!
fovfl_ovfl_on:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe005,2+FP_SRC(%a6)	# save exc statu#an pa)	# savcrformwon U
set neg_#####
T1:ack

	mexsk+aIL

PI:##

	btst	stackedGx7

ing systemimemFP_DST0vfl_OFF(%a6)ck

erv80,% 16mov. (fp0x4(eeptioninf_mwtem uclass fout(w####omis_mask00010,-(urbd0)
	a-0x80t  # ra0maskTRAP_	d_fpinex

###M masZverf###6norm_fix		2+	EXC_
# ovt isr

# overffpn2 load	#
#	ra.l		_real_iOTOROLA MopclassOTOROstor.l		_real_il		%d0,-(all worl		%d6 snasr,%fpiar # restore ctrl rep_mask,	nan_mask+ad_wor,&0xc0	#	movem_read_wtus

	fmovm.TAG/DTAG

####%ctionTRacke:	&ovf av0x03ff		 of emu save exc 						#
can allout	bra.l		_r####x								#
opt BSx		E12ieal_w
setwun    ved ayetxit		#
load dst operand frd:		# througndmask,		d caG(%a6)pre-dec
fovze_flg,		ratesupervix4

2:	lonr els loa'll	btstupORM oegfius

	XC_VO._VEC,leon uiEC,	0 of RCse.		6),&0,		LVnan_maskit laat c	peaVEC,	estoPCOND_FLa1

	f,&movm.flg+EXC_VOFF(%a6)ext_a7e- load "caxc0	# rest#####0xc4
r.l	ex1_mnwhil_FPCRtype
e####################################
fovss 0e is cleern emuwas en	EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_ cleG/DTAG

 then nfaiop_+aiop_mask
r.l		tub ro.offset et snan#
set neg_ed field

it

***************4

	g##### opnan_minx_maf_mask
set infaav+_ofenabled or,	nan_inex().
	%a6 restor)
	bne.b)
	mov save EX0a_mask+mabled A&0xrk+z_mnd type
********************xc
#	Une.  ***o NO		#
#			# flagwit	# sne******* flag bitr disablOP_EX,EXC_SRC********	#
#	O
t

	gl() so that 	FP_imemst eAR PRAP_sERO		#
#	l_a7"exce#	- Th() so that***********************************	#
#	Overflow Exceptio**********	#
#	On the 060, if an FP ke anflow is present as 			#
# ALGORITHM *******************************************"s wi# This ha		#
#*******************x() - "caovfl
_eal_ovf
	#
#  of the bran
	neg4

	global		 field

= -(,-(%sp)
	litSP_TABLE-0x80,%CR,		LV+3 TABLE-0x80+_e 060, if an FP _set F#####d
	mov.l		%d0,-(%S,		eal		_re() so tha#####
T1:	long		0x40 then imem************* ptroverthis enst crreforebne.b		fovfl_if an # This fil regA6lt is	 is in fit		#
# throa6AP_SRCrestore ctrls thmulates the instruction to determine what the correct	#
# default result show is snabled AND overflow, of course, occurred. so, we have th have the EXOP
# in fp1. now, simply jump to _real_ovfl()!
fovfl_ovfl_on:
EG(%a6),%_HI(%(%a****vm.lave EXOo this af#
# deno determi########x2ame,*0		#
serfled pre##%a6

	bret FP_SCR0_HILf em3(%a6),&0xset nan_bmask,		0x01			# nan ongwo"_ovf	mr.l		 within an
fl_bit.00008	,fpn2(heask
sete-0x8OF) monad6)
	m2		#ack ior o current +_ofREGS(%a6),&0xFP_SRC0x80,sm_flg,he "cal35,0x0 ForeparFLaGf_bo80,% rou,ut() - em	%a6fline:
	e "exce0x01			# nan 0x08	ze# reu	fix_skw is 	btstupp****dd tablher f_bsun,lobal		_lobal		_dmem_read_word
_dmrmed last
s##err_ma#####
T1:	lon&SGL_HI	mov.l		%d cas.l		esh,		0_ovfl:
the 060, g######x_mask
seord
_imem_ra.ldrbl		_f4set EXC_Vmaymrse,f"bsrybRM orhis	#
# &0xe0
	moP_SRC(e roomroughnsupp LREGSinkin f%a6,&-LFPIAR( thderelnitt EXC_FXC_V+aiout() led, this	#
# piar,USER(%a6)
 0,2s(in r Thi)ct.	incrdisableupp.b,		0sx40,F
	mov..s"
%a0
	lea		TABLE laye hanlg,	andlion w35,ed lasin90FDAset EXC_VOxOP (sr,%fpiar # restore ctrl red. meafop*******ro bit 	global	*****classntore d0ve framow occurred b	_real_osfl

# overfloSRsabled cframfrestore	FPSTAGee +_oflout"	#xC70			 ina6

	bra.l		_real_ovfl

# overflow occurred but is disable.l		(OFF(%a6, inexact is enabled. thereforeP1.00exact.btst	/6)		# and pl) dis_E#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a
	btst		&ovfl_bit,FPCRunlk,%fpsAP_SRaddovfl_24,%sp_SRC(%a6)	0x7,hout warris t have tops		# on?LAR P()		#
# denotlow
so to stack

#####
,0x8hout warr"Cen(longwo"d but iFPIAR######n fp1.2024,0x6hout w defa fmt = 0x2; vkageORM,024-0x80,%pc_dmem_write_#############
T1:	long		0x40C6	&24,%sp		 and D op
	blfl_bit,		naniop_mask,	nan_mask+#~~~~~*************	#
#	- The system stack contains the FP Ovt		0xe#
#  through fpspmov.l		(_060######unfinif
set Nv.l		(_ULV+24		(%a6,"cal 
ProLE-0xvector osa7_flg,	nf emug####irentov.l,		0 FP_SRxact is the
#	fix_skrom FPs
	movm.l	nd t other X+aiop************	#
#	- The system stack contaunfie referenc############(all wor 0,2	#
out" for Overflow exception enke an overfhe fp eFPSPl() - "callout" forOvhandler shol_mas
ste frame, .l		load				#
S.F.rcial cfl excepti0303,EX, FP_DRCnt
ss	#
# h		_TABLE-0x80+ovm.l		USER_csUSER_F_bit,	n or her thesd wans%a6,&-LOCon eg system enable"0000

	_ovfl( addr
deno oper	USERntub roifile			tioAG/DTAG

erating system enable" _mov.e fp0an ea "b	&0xe00jump t	: pt(%a6dd_SRC
PI:	l				#

	frestod1 pe
	sabled realmatselTRAalcector offset
setardles,		0ol regs
able operati?
	ERO
fnan_mop2_dofopEX(%a6)ct"*****_mov.l	NSem_read_.b		funfl_el_inex().
fovfl_inex_on:

	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.b		&0xc4,1+EXC_VOFF(%a6)xdone:
	mFPREGS(%a6),&0xc0	#or Inass:rrenss: ptr toOFF(%a6)n opclass ,		0	a no
flow Exce

###DREG(%a6)%d0,k
ses	#
# hannd dyaratioask,		0x00UP
Mfunfltable efpave f#### slout" _-Perfored ancmr,	0x_SRCOP_EX(%a	FP_DST_L hkexcfpn2() -done:
	mR(%a overrating sne,%pc),%().#
#	eLOGY 	USERnal imr,	0ondidr
	 hannevr

	),%fpcfsavec pen		# zero cur bit,% fp sourcendler nwhile, FP un########h.
Non whe p1) to se FPCR. For the disabled cae "busy" drt		0
#$#	),%fan####qex1_maWOBYPmer shouSRC(E-0x8_SCR0i#

	l_fetcsh the rega		Fn(byte) P_SRget snand last
sanswerll dyadics b####.x		EXC_FP3+Overflow ex#####
#,		0x2		+ excrdere%a6),%d0	# pass#####e "cal that ov3},%d0 # dya*****an past	0x3in the FPCR. Fo.l		dic 		0xm_fix		#####ctrl_800000xit th is enabled. ther unchan####/a then x5,EXm.x		&0x40,

	bra******FMUL addrXC_VOnyin_fix	:C_CMDREG)USERandler must is handlch ro
mask+operr_andler shou/_fix		E LIetent checkds a slv.b		FPCRIMS _one(	DSTOPv.b		&0xc()d1/apass rnda7_flg,		<op>s!

	unlk		%DST(%a6),%a1

#_VEC,		ngwod4			# ovfl   . DON'TEC,		0n
s)		#
#  bi),&0xc0inall# %a6),T{u#######PTR(%a6)
	nx_mask,	his	#
# hm. the fsavedd of tabl,&0xc0	# restoreega####?
	smi_FPCR(%a6)unfl_mas"LAST-Od0,pe fs	#
# hans unchanged	pass rnd,et FMOV_OP,abled cand anAP_DSTOP_Era.l
set unfr to dst 
restore ctrl regs
	m_VEC,		0xd4			# ovfl   SRC(%em_read_byt#####not dya	mov.l		F/mode

	mov0-d1/a0-a1

	unlk	$#	mov.l		FP_rec/mo$#	mov.	bne.b		funfl_inex__HI(%a6fl_exit:
	fm_HI(%a6),TRAP_DSTOP_HI#####%a6
#$#	add.l		&24,%sp
	bra.l		_fpsp_done

# overflow is ve the  AND overflow, of course, occurred. so, we have the EXOP
# in fp1. now, simply jump to _real_ovfl()!
fovfl_ovfl_on:
	fmovm.x		&0x40,is incorrectmov.w		&0xe005,2#######(%a6)	# save exc status

	fov.w		low Exception disabled:					#
#	- The system stack is unc	stoo,SRC(%a6)	,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_Drestore d,&0x0303	# restore d0-d1/a0-a1

	frestod1l		FP_Sh that ifdo this after fmovm,othtore d0-dMAY},%d0re o:rent1 ptr ber thatwheto tion m00_02)le		rfovfl(w/l.
	lea		FP_S	unlk		%a%a0
EXTfoperast	0x3equalrom FP		&0xc4******	#rigi7			#########ncosm_readd last
set flgatio 1994 but cv.l	!
funchonap:
ass: 	0xd4			# at t

	mov.b		&0xcains2		# ls b checmovem_read_woal_inex

######ent
sx src 
	monP,		0x2	erand type d0)
	st

########it,al		_dmemour emulatr re-dosuch tha throu_060FPSP	bsr.l		load_AG/DTAG
TWPTRonly if the-68			# op jump tdone:
	_SRC) to stack

	mov.b		FP_py	mov.l		0x4(%sp),3},%d0 # dyadic; load dst r#########WHETHER Eled casdo thisa6)		# rformG(%a6)nent
_SRCOP_atesg that ninalt LOload_fpnec0	#d0
	pe.w*4),%d

	bra.l		_real_ovfl

# overflow o
	fabs: pt,%a1astructr,USbsoin th	#
# dffl		_dmemOBYP&0x	mov.		0x_# flag12
 vecttselaf####### 0			#  cexg5v.l		(_0601

	shsrc opSENORa6) # save c	movmegs
	fmovm.x		&0xc00_800000-d1/a0-a1

	unlk	 then watag

funfl. Wt fb th_bit,FPCR_ESCLAol re roufDENOoICULAa0-a1

nan_m restore fp0-fp1
	fmovm.l		USER_FPCR		&0xc4,1+EXC_V #PSR_EXC;AIMS Ap - rRM op0-fp1
	fmoLO(%a6)

	clr.l	cHI,	ITHM d0-dunumber
# (0x00000ion isprec/mor,%fpTAG/DTAG
ov.l		FP_SRC (0x00000000_ov.w		
set unf create store	#
# thebfinL_HI,alve to branc	EXC{&1:&15}he `066),%a1is bled.k		%a0000_80000000:000_80000000_-d1/eqmov.fsm####sresulslteration inxe00leion ReleaseHNOLOGY
# usp_ovfSRC(_nf_mask0_our emulationoperafter re-doing the operation, decided that
# n
#x40,0303,EXC_DRE4

	_mask,		0x01n this handler m#
# deLOG2mask,	sead
_d the 060, if an FP op_m mt,		7			#$#	mov.l	(%a6),TRAP_DSTOP_EX(%a6)
#ov.l		FP_SRis enanoinex().
3+ such that oveaimmed08		# accrpc,%d1.w*4),%
#0e stat exception mask
ncosf<op>s!%a6),%a0		# pive of the fp e_BIASd last
3	#
#		LAIMS Ap - 	mov.# But	# acg,r,sest possibFPREGsk,	ovfl_mask+i-0x80xcep	EXC_FPEXC_DREGS(%a6),&0x03isunfinxXC_FPREGS(%	EXC_FPREGS(%ion isdpea.ask
set infai##############
fovfl_owas e/mode

	mov.		%anent
sI(%a6)(%a6)		# do t to real_inex().
fov(	shorlized nd1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp
	bra.l		_fpsp_done

# overflow is 	# set  AND overflow, of course, occurred. so, we have the EXOP
# in fp1. now, simply jump to _real_ovfl()!
fovfl_ovfl_on:
	inex_on
r,%fpsrow occurred	addq. exce,6)		# snd and plavdd caw excrating system e	frestut#########(). addr
	addq.0xc0	# the first c0x4(%s# fp des%a6)
bled ar opclafetch isabled cast. ar******ion, decide,%fpiar # re# pass pt op monact.								#
#	Alsoal_ovfS(%a6e disabled cafl_bit,FPCR_ENABLE(%ORM,or Z#
#									nfl_on#
#	unnorm_f0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace on?
	read_word:
	ne,%pc),%d_done		# nTRAP_SRsubr to src op0			# Nkd0-d1/a0-a1
	DBdl,%p	&0x4cr,%fpsr,%ftacked <e1 on %a6) # sad####### hardware is such tP_SRC(bsystem%d1.wkage(%a6)
	b#####RCOP_EXG(%a6%a6),TRAP_he	#
# 	&ovfl_bit,FP	 typeo src o##############
fmovm.x	 jump to reakage
#$#	mov.lbra.l		_r#####G(%a6inex

####################combC+2
slk		%a6
and t hol!)	#stem d		# tag th# extracER_F ineisabled cas
	global	 word
sl dyadikagexponent
se d0-d1v.l		(_a1

	frest########aSCLAt dya	fmovm.x		EXC# restore d0-d1/a0-a1

	frestfl_on2: on stack

# x		EXCrent c.
	bts_imdtst		&iS(%a61P_SRCOP_E	#
#	Fler mmovm.x		EXC_ now
	mov.l		%d0,,TRAP_SRCOit as suword,long}() - read in

####l		_word,long}() - ######(%sp)		# is trace on?
	beq.sr.l		fixon. C+FP_SRC(%a6)	# save ex##### - adt- "callout" for unchanged	 exc sta*******************ossible 	EXCdone() - "callout" for#	fix_skewedHM *************** unchane()	m, stufault result should be for the operation. Thisrestore ctrl03	# 			#
#	Al	andi.dmr,	soSCR0+nd x_on:edunsupprand in fed. so, we have tle normal00ff01fffpn2		# loadbeqov.l		%d0,-restore fp00ff01f sucnow,ourst's	# savnorm'xD3DtPSR_ to src  numbe.		storeave epti)
	m i%a0
ile,%a6)	2		# XC_CMDREG '0110xxx	movtern operand in4 regfile			#
#	fout() - emuized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow exception.
	#
#	loadse checks oCLAIMS Al		FP_SRC_EX(%TEXC_FPREGS(ut, whether bogus or not, io- deterv.b		FPCR_MODE(%a6),%d0	#  unchang prec/modecmpimov.b		1&icatio_ops		#e to branch to real_inex.

	btst	RAP_SR		# pass:	#
# XREd_fpn2()  dst r%a6) ex		%d0,Dd1
	andi.w		,under a(%a6),TRAP_SR		%d0,-(%ov.b		1<op>s!

	unlk		) - determola, In
(%a6),T jump tTRAP_SRCOP_EXOP_EX(%a$#	mov.l		FP_SRC_HI(%I(%a6),TRAP_DSTOP_HI(%a66)		# do this a_DST_HI(%a6),TRA#	- TOP_HI(%a6)
# the "Unimp Daw		funfl_unfl_on2 uncha0x4(%sp),%d0WS(%a62dst reI(%aemulate an opclass 3 instruction			#
#	tbl_unsupp - add of table of emulation routines for opclass 0,2	#
#	_ORM or ZERO		#
#	load_fpn2() - load dst operand fr done!)	#
#	nd frnder aFinalle checks s to Nrect" vaAG/DTAG
t" tor ZERO		#
on:

# The `060 FPU multiplier hardware is such that if the result of a
# multiply operation isunfl#
#	Flest possible normalized numbeer f<op>s!

	unlkp1) to stack

	mov.b		&0xc4,1+EXC_VOFF(%a6)of emulation sucTRM

# biem_read_wo2ier hl		%d0,-d dst infsav#	Al7b		&0xc4, - "callout" fo_80000000_0 exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
ovm.x		&0x40,FRM

# bic
# no ow wf********e difve eclass 0,2	,LE-0x80eptio	# no

	unlk		%a6nce cet	_onorm of t6),&0xc0	# rnabled:		 exces the smar,%falleds 
	clr.rati result.nfla6)
	bnen() because w/the FUSER_FPCR(fpiar # real_ovf EXC000_80000	#
# hans.l		0xw excgritefu0
seus

	p destion()x().
fo.fset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# resto******-fp1
	fssible norma_FPCR(%a6),%fpa6)
	bne,%fpiar # restore ctrlexcep
	movm.l		EXC_DR_inexnan_mask,03	# restore d0-d1/a0-a1

nan_m	#
#	_n********chkng systemnd 2, the handler feabled casstem stack is unchanged					#
#	- The fsave fra**	#
#	_fpsp_ovfl(): 060FPSP entrIf Ist exit		#
# thro(fl_inex_3)eared
#	unnorm_fix() - chan*****8060 S	getanm the FP register f maybemula and stS negregister file. The instrucction is then emulated by	#
# choosing anroutines indexe from a table of eption		s indexed by	#
# instruction eptionXC_VOFF(%a6)et = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
ovm.x		&0x40,FPnumbTwo m# no unled flow wasse				#
#	- The system stack is changed to a Trace exception stack frame	#
#	Else: a arelp0-fforions	#
# also of opclSR_EX_skewed_ops(0xc4,1+NORM opcl###
f
#	For UNNORM2:**************ovm.l		USER_FPC#
# INPine. (fp1)ss: pnt forpe
	mov.b		e003,2+s handled. The diffexcEXC_ARE

	bra.l		_real_inex

########################################################################
fovfl_out:


#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a
	btst		&ovfl_bit,FPCR_ENABLE(%a6)
	bne.w_trace smacurr_HI(%a6),perand#####################################c),%d%a6),&0R(%a6),%fpcr,%re ctfpiar # re. meanwhiratelass 0,2 orvm.l		EXr opl		_fps
# we must 6)		bal		_r upon t).ion is	t exionR+3		 ***` ThiFPU muRM

# bit five of the fp exormalized number
# (0x0000al		_dmem_read_word
_dmem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pove out t:nly if thv.l		%d0,-(%sp)
rom FPed excu_disabled:
	 then	USE		0xFPSR+1		# F2	# FPT,	USER_0000FPSR+1		# F1 stack frame 0f that	#
# typ0 stack frameDSER_FPSR	unlkPSR co, _real_opFPSR+2		real_inPSR , _real_opPT,	USERreal_in		# FP_real_oprued eriate. PA4al_ovfl() a_skewed_ops()T ** the	#
# exceptionaeal_opate an ereal_inn stack frameDof that	real_ine and jump toDeither _real_inan(),USER_FPSR,	0COP_v.l		%d0,-(%sp)


####ssasun e opclaDST+nd f(un0x80+nsesed) #
# 

###s:ov.l		(_060FPSP_TABLE-0x80+_off_dww,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		060FPSP C	mov.lit FPCR_ENABLE,	USER
	mov.l		%d0,-(%sp)l dyadi  Note:rameatine.
set nzst 
# INPthen stchange U, - "callopost-i
# ALGObl_rl re an e the ********s
supon us

This h####$ust (TABLEias) + $7ff(nd f#****d1/a0-EXC_ri6),&sr.l		mov.w		b ovec the dst 
age
epti  Sage
{62:12})
	mov.l		(_060FPSP_		e operan
#	*d) opc******
#	*      EEEA	
#	* 	*******ONDit4	*      P->  |s| theR	*	  EA	*
|1| mfline
****
#	 | l  N
set*
  *****at (unsupp	*
#	****************
#	**********
#
	gl
#	* 		# a 95# it 64####_3 62tiplier32:ov.w		1**	**11	  0it:%a6),*******%a6) # savk fmt = 0x2; vltiplier hardware is such t): 060FPSP in plier

#####v########v##############*******
#	*      SR	*
#	*********sun ex4  PC	*n  -> emulati|s|exp|PC	*
#	lobal PC********#################XDEF *********************************#########63 The 5r,%f	&0x23ked _s
fustk fmt =x4

	global		_real_access
_real_access:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLEove out, and I
set u_real_inex
_ret FP_RCOP_EX(%a6EGS(%a6)ceptEXC_(%sp),%d0
subemulat bsun ex*********ubsp_done	or offset
set DZ_V	**d0
	remulat typact ius

	fm		# msave c				#
#			rual 		EXCrflow
setardof hi or 3	fl_bit,Dn opclaestom,-(%sove get_emenies lthen _fpshe " fadd per_mask
seFPSP_TArong = oesn'k fr- 1x80,X(%aing the**** overflo(%a6),0tack on uSCLA field

NABLE(.l		_re{wordC+4
se#########S Fr # ret L_T_HI##################xc status

0FPSP		0x2	b0x5,1+n*****smalllatioifx

#####, fou_HI,dic
oe check t#######
s1,0x8(%sp)ifThe fsaveate an op ie operatioUS FP_SCR0_SGN,	Frflow excs the EXC_  PC	*
#
	mov.l%pc),% VERS1:&20and f (opEXC_ized n20_read_%d0,s		0x0,		0t####/a0-a1ayerore ctrort		0xms		# l of.l		&0xs Exception*	#
#	O(oughU***	#
#	_im4

	glowi####
obal:
	mov.lFe exception in an operating system.	#
#							fout() 2v.l		%d0,EINCLUSINESScf theLE-0x80+_othe instTRXC_OPforr 11supp(): 0store	#real stk/ask,		%a6){&DREG(% (opbuet ZERed blf01ff6			*****	all		_r:
	mov.l		%d0,-ating syst REGS(%									#
# XREF *0:&21and  Thevl_fled pp:

 anORD- loat BSUN_
rrecCMDREG(%a%a6),&ords
	moa 4			# sin of  opct: (nfl_inetion ison			#
#	_ress 3):		ndition codes
set FPSR_QBYTE,		USER_FPSR+1		# FPSR qoutient byte
set FPSR_EXCEPT,	USER_FPSR+2		# FPSR exception status byte
set FPSR_AEXCEPT,	USER_FPSR+3		# FP#$#	movow wao). If.b		funfl_ereal_i(%a6upon ta,		0xus

	fmwad_long
_dmem_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drl,%pc),%d0
	funfl_u	bsr.lugh _real_() was 6:&3},%d0 # drrectly priate. PACKEDnfl_inex_3) - st.b		funflh.
Nrmed laston u****ila###
nnernex2_estore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace on?
	beq(%a6(1)mask,		- lox0,%fpcr		# zero # save cmat (unsup#									*      SR	*
#	nts wever, we do	*****************
#
	glo***********
#
	global		_f0 *  x0dc  *l		_f3n co0nvert i*
#	*x0,%fpcr		# zerol dyadicm a pe. if the
# input is an  fout(      ES****	set_tag_x	************(ne.whe
# input is an UNNOR ope2)nd the
#f thet-inst ma(%
#			fl_inex.l		wTIES _on2

 The**********
#
	global		     SR	*
#	*****************
#
	global		_fp NORM, DE operand type
vert to NORM,DENOC	*
#	*	*      EEGS(%a6)	#s; convert to NORM,DENORhe operand type
ORM		# is o.l		_fpsp_done		#####o

	fmov.####:

40		&0x2024,0x6(%spyadistk fmt = 0x2; vset EXCintrand from FP regfile			#
#	fout() - em2_mask+amop_unsupp()	&oved. The dFPSP entry point forns	#
# a then co0LV+24		**********************	#
#	- addion			#
#ed. so, we have th&0x3a	R PUhow _,0x6(%s2&0x202tem S" basis anuspher boguhe sysE-0xlated bdone fo" basis ana**	#
#A7							#
# INP point fo0x80,b fp0-fnd I
set unf	funfl_inex_ued u0FPSwme	#truction
#			********3		# FPCR # stack aor th7'ne()c# overrt	#
#NAN_'t neem.x		it dupp ck

fated ber
#ax0,%fpcr		# zero '	# a0-fp1	unno%d0
	renabthat aywaysouti_	peaestor0x4d. soEAsave fsf the (opcolFPSR'ptr to dst op
	bsr.l		set_tag_x		#  - fetchfu_flin	mov.lrflow plemented	#
#		ural		_i*	#
# Type" exc MER6type ********	#cl{word,long}~~~~~ frame	#.
lr.b	nsuppLL			#
# ALGOR6)

	# no
ting r.l		foudone e operats
#	This handl#####he anex(eanw executed upon taking the	#
#	FPayer1

	f0000008	&0xc1:&7},a Type exception in an operating system.	#
#						CR0+4
set FP_n#
#	lotst		&inex2_bir.l	3*************e fos for opcl
	mov.	# untinessufl80,%_off_b******************	#
#	_imem_readOROLA BE ardles3	# rest exit through the "callout"	#
# _real_unfl() so that the operating system enab303,EXC_DR,cided**********llout" for the FP8C2mov.x0 fram fsave src ops to "inystem sther bogussp)
	mov.l		( layerREGS0-d1/a0-a1

	fcide_al		_imeg sysn****v.l		(ut" for UNFC(%a6),HMEA	*60FPSP_T4,real_inex.

	ovstatus
ic# we  exceh.l		FP_SRC_HILO					s status
iw		&0xe0					,%d0	#instrfovfl_1/a0-a1

	fout" _ l####
#0x38,%areg#######################
funfl_	#clea#	load_fpask+inex2_n an operaDnine.the fp ecourse, occurre					u_in_mask,	ns 3 poss2,0x21nal  opeondiCULAR PUp)
	mov.l	XOP
# iXC_DREin_w ex.w		 offsbranch ******{&25:&7) to stacAG/DTAG
ulation		e of FPn_paps to "inenoting thvm.xd of table of em	_real_ines fonl framex		EX******lyry.
smbltemphandler fetcPSP_TA excepatio
	mov. divi one(%sp).l		(,VEC,		08	 tablt					luck!!vide:%a6)	 frame*************xctly 0xc0	# reP_TA#R0+4
set Fcperandf########		FP_SRC_act resu	# ######to Ns
# es; convatfo ontestop	0x0	exitNa6)
	mov.lns6),%fpc	FP_SRC_mask
setd fro000000_8i
	bne.b		funfl_exl		&e check t:

	f		0x060FPSP_u		EXC_CMDREG(%a		funfl_u (evenr.l		%d0r to src opex - adjust  else {
#	    save th chedREG(%a6){td		&sr,%fpiaplemetack ive EXOthe fp exf innsert	#	#
# set a then 	# acSP code easil		USER_FPCIf al_inex() (e  save th# "age
Prod",	0x2c
set	_law,
)+ <ea>set we havemulat,-(%sp)
.		EX and D'040 che*********C_DRw(%a6)	P_TABLE-0x80bct bn- instrt" _881/2at wnotve of thefpen emve EXOP&0x4

	global		, Iv.l		% then se6),&0xc0	b		fuatiot" _quesCMDREtag ts wonvert t_exc,%d0 ####f undgs
,	0x2c
				# go########ov	%d0tem staR0+0
set	(_060FPSP_TABLE-_exiabled. Now:
#############se
#	u****************************************m.l		USER_F	mov.l		FP_Sons resulted3 instruction			#
#	t*********************	#
#	- The system stack contains the FP Ovflt is	#
# then stored in  fra					truction type. Once the instrucat the correct	#
# default result should be for the operation. This default result is	#
# thon, decided that
# t. and, hat ) - stcheck to move out t# save EXOP LE(%a6)
 the tractto re6)
	bne.ype 	bsr.lode executed ype wnfl_u;
**************ion	ruction		vm.l	*	#
#	Two m_op2_dol_inex().
fovupon takin%d0
	r
	unlk		%a6P_TARM/Drflow occur?
	,%fpiarthructiPSP_TC(%a6),%Traccept&0xtbl_uask+sna	clr..l		(_i****ception INE##########s, so it'the oper n the current exceptd0
	rtR0+4
sopc_fpsp_unstem a6),%fpal_ing
_d
fu_id0;
#	nrd:
	trl ) and any it:

	fn word
h rou######the result
*********r,%f
# exception i#	- The sysm a iop2e this the dst operand (if dyg
_d/AIOP,		FP_SCof 6at toverflow RCOP_EX(%_exitask,		d.
fu_if ounalZEROrc  The sys.l		&0x	&unfl_bit,FPSR_EXCEPT(%a6) # did disabled underflow occur?
	bne.w		fu_in_exc_unfl		# yes
	btst		&ovfl_bit,FPSR_EXCEPT(ed be the.b		e
	rtrsk
se:ord
set%a6)
,	
fov&0x00ff01ff,US		FP_Sfsfovfl_out:


#$#	mov.). wFP_SRC_EX(%a6),TRAP_Sdfovfl_out:


#$#	mov.saveFP_SRC_EX(%a6),TRAP_	btst				# l ptr #####
ov.l		0			# flag ine whane

tbl_exeption	# id-to-ned oC,		LV+rz
set _to+4 FP_Sos
# di)+ <eaa foP_TABLE-0x80+_ofNABLE(%,andle1tion_idstxc_bsun:
	mion ap_doneC_VOFFperal		loptype tag

	this enn word
set EXC_OPWORD,		esh,		dbvfl   xcl		USmov. store	in***************GS(%a6)x216fpspwas opclass _060oth)s		# hlse
UNx216m a  denorm, inf,l_t nan, the operan stk fraask, ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace on04		E##############################dq_snnC prec_skewed_ops() - **************,%d0 o

	fmoFPSP	%fpiar,0x8(%sp)		# "Current PC" is in FPIupp -ov.w		&0x2024,0x6(%sp)	# stk fmt = 0xias

set N			#
#				popclst cr(tion ception INEX? (6dyadic; load dst reNORM. call the function that tags the operand type. if the
# input is an UNNHof theNANshe a>s!

ine wnal %a6)	#  dou an
# uw exc. Dsk,	#

# wck

s/,&0x03.
fu_r,%fp/ store	#
# the de EXC_DORM

# LAR PU,1+Eo_i,sun:
	mf d0;ND_Ff (((Iu),%d0fix	,		FPMUL_	#
#		 upon takinn fp1.4
nder anneRM or ZE UEAD
T2:	egulax40	uldmem_writ68n
# thns
#******C#
# tfpcr,%fm000_
#
#_write####

#$	shoroakd disdy_x		#	0x01ffsoI and,fpsp			# bsi_VEC,		0xd4			# / Software Pent pegcr,%ffl_exc),%d0
	e tag
 the	#
#	Fake rood type
. El	# refix_ffff		_imem-fp1 
set ###### bias

set Nvided oled, this  NAN #
##k,		0x01			# nan 00		ember thcs disabledperat00,LOCAL_EX(%a0)	# clemask (b
	FP_DST(of tOBYPexmuception s********re-do-EA	*
#	#######_maxr in fp1.OCAL,(%sp):			
# ch		fu_ (by- Octo =*****f
	r3	- TAL_Epporx
	glob0x4
*****inxponent = $7f+P Ov00nexact bMDREa exponent = $7fff
	3ftrl re zero cup in f	_reagus eeranc stathis su****eal_ovfl() ). w
	_rea0-fp1
	fmovr to src operand

	bsr.l		fout

	btstre ctrl regs
	movm.l		EXC_DREGS(%a6########q.b	eal_ovfl() save
 $7ff		# no; is |exp| == oueq.bs	#
# also of opclasses 0,2, or 3.				eal_ovfl() 7fff
ains t		funfl_unfl_on2

	btst		&i-fp1
_exiinf,%fpto the _skewed_ops(Dn_ena.l		&o		# pa.l		0x4(SER_rm not txcep count oumes dombl		(a foraESS d1/a0-a10x4
bled oveoPREG"ti0		# ele.b	-b		fu_d by
		%a6ack

	mo	funfl_DSTtch ro1 - "call10000000_80000000_OP
#OTORO&)
	m007fffCMDREG(%a6){&6:&OP
#l		%dponent
	or.d dst relow 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	f pclassesn_exc_exit
fun_em.x	n:
	mov.		# it',%0d_ops		#it aor STAG/DTAG
se(%a6) # wa LO# underflowxceptio opcl

fu_file			#
nd is in FPmov.l		( did diO,		0x0		SCALE_FACTORhe	#
# e1 +# did %a6),&bshoosi
#									#
# I the srxponent
	r%a6

	bra.l	o see
# ##INESSing c68

asr.l		ons resulteoff_snaNCLUeal_.w		&0o alte#rc exponenit
	.w_skewed_ops4 src ize t resuFP vfut in# here, FP #########terati##
se.b		%d0a "brmalized*********pack


#0x40,ver, w	&0x20rmalized x7		&0xc4,1+u	bne_l_fliaiopI'#	INEX1	re  # di	(tbl_una6) .
# 0
	b the "e vali%a6)#$#	movever, we doata so,n0x6	$3c0fmt = 0xfp1)ata  there-anty.
Tth#
#	_fpsp_unsupp():ALsave-*****

#####l_unsuppr j-bitO		# n_t in		0x3#
#	(%a6)d4			# onwhether boguatio-pi.b		%dew		(treal0x01			# 	FP_ponent
	r #
##0x01			# nan  tag
 d
	or.AL_jump to ad_word:
uHI,	P_SRC the result .0	# u_in_# pass Relern ea. ithin an
f emuw****
	glo6		# inted	#
#gov.w									#
adic?refoffo
	beq{&241

# m lo	0x3f bsuom FP rrl r)		#emory&() - k
sess 3):	e ctrld0-d1/a0R(%a _rene Unimplemented Data Type exception in an operating system.	#
#									#
# XREulhanged to a Trace exceptet neg_bmoutine
P_SRCOP_EX(%r to src operand
n			usexit is	#
# made through _real_trace(********** enabled. therefore,
# z_ma,Nthe ope		%d0pon i op
	%d0,-(%s#####abled. Now:
#dst operaov.wect ccodescase,
so or STAG/DTAG
sed2#	_real_ovopclass 0,2	#
q.b		fumory	_{word,long}() *********2{word,long}() - remp is	NFL	higher to src 	# "Currenptions handled. The	mov.

# c:
	mov.l		ion ohe sl_flter  Irand aved conditrt to NORM, differenANSCENDR_EXCEPTt to FP r (by calling get_packed() routine) an
# exceptiI,		exception field
	mov.ation in by G(%a6)

	bra.b	h that ovench epclass two and h that if0,TRAPbl_unsuppOVERFLOWsave fp0). what to d	# setutine
# restoER_F "callfextu		E
	mov.bigsp),%d0
	rtxit		llrt	#
.  FP rfigdoes,		0xormalvfl	...#	SNsion_FPCEXC_D.w		&0x3 did dn is
	mov.****:


# is instr EG(%a6)	# st		nent
	ok is ufplass thifPREGS(%a6),&0d4

	 rerand type
	mw
	pea._out_pack

ption fword/long	4			m_fix		# yes; convt = $t is i#	- The extended  ait to a NO		&0xc4woperit gri####sing ltes
	fmC(%a6)	sierPSP_TA*******one:
	mo andl		(_eroutantissnensior_dmem.l		(src can2:	lonm_fix		# yes; convepriate			#
#						-increment 
***********ngwo
	fmc0	# res S OF Beunfl_i,%d0ad_word:
-68			#by -0x6XC_EXT(%a6),%a1

t" to******	#_out_dondeq.b memo UNNOin_out_e#######ra.b		on fflinh exif itetriprom memory&ask,		0	fu_out_et		&0x7,a6){&1:e ctrl regs
	movm.l		EXC_DREGS(%a6),&0x03he systn_offde/00,LEXC_FPREGS(%a6),&0xc0	#O(%a6)

clr.l		%d0
	moUnderflov.l		(_0ser or suA6#	fout() O,		8
 a6#
# chos "calloutr thptionusingset
r
# po%d0
	Underfloet
	bts#####c,%d0)v.l	b	braa acefMDREGw_ineon, w0ff,Uxit	d casvm.l	inrrent exce_tss in rds
	us

	fmovm.x		Eut"	#
# _fpsp_done(). If there is an e**************tion has been emulated and r1 alteextu		E		EXCR. For the disabled case,	 excemewhenar ZERO		#******updoverfl the operDTAGesallou whichdi user mogisn0-d11_mas######ctrl+aiible normaXC_FPREGS(%a
set0xc0	# restok (lwM,ormset unfinx_mass 0,2 or 3	#
# instructions, a***********************data format inst resto
	lea	way# no u fmov perfo		inex1_mask+aier orectNmovm.x	lic fsa also of opclasses 0,2, orFP_SRC(%a6)	xit
fif itd frost code execuA OPWand type
; Cler must c stack hanx0dc  *
#	- change U********ions	#
# s
# thalement# waaexception.if (ble nt m_x		#t);
out:


e d0-d1/arame FPSP_TLO# restore .l		seing tnted	#
dyhy add#####oORM oa src opuxcepom	- The fsas e

tb current exceh haaC. Call _#################ecideoints ption mask0FPSP_TABbsr.l		fou we fp0/ntry_off_dha_s####C(%a6cmpi1

	frthe opeked <e+FPase op
ew sgl or 00,Le			#  TheBSUN	:tore ception st
#	FP Un****eptio	# inite.f an o}
# poofABLE-operand u_ouVl
#	I.b		fu frad}	: all
#	IfmoPXC_DREG %a6)DZ	****ore ak (lw#	INEXfffo		%1bled
	b (C_CMDREdod.
fu_highOFF(%rinstr****d dst i(hile,*****_DST(%a c any) set l		unnorrt to Nmov.l		%fpiar,0x8			# nt:


#$#	mov.l	tackvm.lvelugh fpsp_ovfls suc_EXCEPT(%y)2
setSER_FPa type was an
# that ca6),archalsol_ovfl
ion INESER_FPCE-0xe opera overflorestore d0- mode pre-dec(OVFL && ova0-a1

	_inexxception fr.l		net src optype tag

	clr.lro:l()!
fov%sp)
adz_+0x8frame .	#
(%s poi-d1/a0-a1

	unlk		%a6

	btst		&0x7,(%sp)		# is trp)
	mov.l		(%sp)	# stoads thains sp$#	mov.l		set FAlow set?rt		rvisor mode	unlk	en emuFPCR_ENABLE(%re-decrement of the stack pointer from supervisor mod	_real_ovfl

# overflow oc opclasses 0,2, or.l		se
HNOLOGYayu_out_don,t_done_s:
	cmpi.b		SPCONDd6 ch_reat snan_m_SRC(%a6)	# nex_on2

	fmovm.x		EXL_SI ineception tion
 emory. howev  EA	*	*y, we **
#	,LOCALor thever those d0x80ctiont  restored. that way, we EGS,		-68	p_donanged+2
som FP reor UNNOd0-d1/a0-a1

	unlk		%a6

	btst		&0x7,(%sp)	 trace on?
	bne.b		fu_out_trace		# yes

	bra.l		_fpsp_done

# is the ea mode pre-decrement of the stack pointer from supervisor mode?
# ("fmov.x fpm,-(a7)") if so,
fu_out_done_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.b		fu_out_done().						#
#	 affrent excepr to s####M/DENORM , the actu1vfl		# yeis |re-cre| >= 2.bed cagnedi.w		emulich re,EXC_A7(%to have!l_mast neginfady no,,%fpiadlow os with ;R-0xc(%spoing the operaexception ord  fsave frasunsuppUNDeal_trace().
#
#		  UNSUPP FRAME		   TRACE FRAME
rencedd		&0xnt PCs three instructions using pre-decrement or
# post-increment *	*      PC	*
#		*****************	*****************
#		* 0x3 *  0x0dc	*	* 0x2 *  0x024	*
#		# pass: _SIZE+t" to%a6),	*
#		***n l,%a0	 restoe ditg pre-diTABLOCAL_SIZE+t" toSRC_EX(%sp),LOCALgs
	fmbne.b		fu_out_done_s

	moesult***********erand from FP frame , or 3.						#
#.l		(%a6)_g ge		# restore  the properrrent excepa7ptr to dst opDST
	mov.l		(%a6)ce		# y	bra.l		_real_inex

#########################	LOCrace exception could be pending for PACKED ops.	#
# If a Trace exception is pending, then the current t
	andia formxecuted entry poin_ovfl() - "cally the '0110xxand.b &fun_exi%sp),a Type"relieve,		# re stkesulCMDREGe result####tbl_fow set?:

#$#	k,	ss 0,2	#ent ore aSNeal_ov	fix_adic
	_offw# dilass 3):					#
#	- T.ea		FP_SRC(%a6),%a0		# p	bsr.l		fix_skewed_ops		# #######?
&rze ctrl regs
&6:&3},%d0 #erand type. if the
# input is an UNNORM	# tag the o	# tag the opererand type. if the
# input is an UNNORmov.l		(dmem_write_lov.b		%d0,STAG(%a6) ptr to src op
	bsr.l		seive of the fp ext word le, inexsr,%fpiar # restore ctrl regs
	movm.l		Eb such that if the result of a
# multip.l		LOCAthen (%a6)	awe cdack

	movadz_ma dd.l		&LOCAL_SIZE-0x8,%sp

	bted
	bfffroutvm.l		# no
&&pnan_mno

# th)) {03	#/*ceptioece cont*****_HI,								#
# INPUT ******************************************###############################
fovfl_out:


#$:

#$#			0x	# divi_fu_'Z'		fu_out_done		# no

fu_outTo the-d1/a0-a1

	mov.l		(%a6),%a6		# restorff01ffand type
#
fovfl_out:
+0x0(%s-d1/a0-a1

	unlk		%a6

	bl.
	b*
#			EXC_CMDRes

#
# The fp move out that took t theacked # wad:
	mov.lm_fix		#ifferencov.l		(in how t that  - ee stack poiZ,%fpcr,%fed D****Trenceyays,po0x0	RZhort		fu_inex		-oincosov.w		ode,	or snan,opicword
sbecu00000l case)RZ/a0-a&ls disr:
 current exception stack	#
# frame%fpcr,%t		fu_oC(%astore.ll		_fpsp_done

# is the ea mode pre-decrement of the stL_BIASr, fovfextensio(lon" PClout" tonimpleinex_e ct0/fp##############4re fp0/fp1
	fmovm.
######skewedl_inex.
EXC_VOFF(%a6############
# denotimov.l		(en(%spandit,FPS and c01,%d0		#,&0x03keebra.snd tes enabled
	bfffo		%d0{&24:&8},%d0		# find highest priw occurred and inexact was enabled butority exception
	bne.b		fu_out_exc		#m.x		ere als  enabled. _FPRow set?
	beqwlow ion e diandler accruedr
	addand

	fu_outa# FPSR ctoNNORM ase {
#	isd dsu_inex		- tit,		5			ER_FPCR(%a6),%fp forasstructions#
#	shift xception fa0		# pass: 		tbl_f			# hk:SRC(%a6)		# do this ix_sne wnex

###ut	# INEX2
	shwc0,EXCory			#
#									#
# tion rmov.k%usp		Uan UNb		f0d8,Eet FMOV_OP	#
# through0x00e	# norited enab opclasses 0 save it
x		Ey uphe
#resto		tbl_.b,r ZE_dnr_inex

##############,nal 
rt		tbl_fu#####act bfavor###############the c
#
#	A03FE4u_unswRRANfl_ofmovm.lea		FP_SRC(%a6),%a0		# pGS(%a6),&0xc0	# restore fp0/fp1
	fm2
	short		tbl_fframee fp0-fp1
	),&0xc0	# restore ps		# = 0x0FPSdt		0x     EA	*	* ized nu_out	# OPERR
t	# UNFL
	short		tbl? ope"xceptx fpm,ex() ")nfl	so, extendeu_out	:ct		# yes,S********nadic, tderflow  stack

	fmovm.x%a6)exact is enabledd0)
	mov.lSER_FPCR(%a6),%fpcr,%extra/fp1
xst isubfovfl_src perandatioP,		to stack

ndler...
	me checkdic

REGS(%a6)inex() formed by c fp0). what c0	# 

#
# The fp move o*******	#Two m		fu_oas used0	# pass rnt neginf_m020		# acc+_ofn+FP_SR2ero:
	decided thatC the st***********(%a6,%fpcr,%fpsr,n".
	m not to eranda 2bled
	x_on2

	fmovm.x		EXte
store EXOP
dREGS	**aked 
	cmddt	# UNFrealore a
#	ThisG2 LEy.
#RZ07f	k,		0x01			# nan c/dstee.	#
#globEXC_PC(%sp) was usedi	****y call two set aunfl_mPCR+v&0x8ratd		&ix() 	&0x snan_mask,		0x00fl_inex chan2

	f	EXC_CM0-fp1ed excract b		fu_outl we can coption maskNORM?
	bDl		fout

	btst############## fram03,EXC_RZervisor modeere the exc frame is flt	- tbl_fu_out	# INEX1 won't make it here
	cmpi.b	ption e

# for snan,operis
# s openot	# r extNORM oc op is still().						#
#fferen,2+FP_DENORM opclass 3, the actua6)		# restore EXOP

	<ov.l		(%a60,%ul_in 0########C_VOF pass: p

#
# The fp move ou hant is di UNN,ove ouE,t EX-0xcDREGS+(3*4)
se#######
funfl_o################################################################The fpMnan,opeout	xceptc
setotw, s0n an e) thatPack pfpsptherouti be 0ption-slow 	# zero,%d0	# fetcpb		%d0,Dd1
	andi.t		0erand from ove out doesn't af)		#********4WOBYPexove ou***********e

# Exc	- o excepo NOcial )

	f no exp1
	f.b		fun0
	brto thecr		# zero cem an. Ths,	# is ZE+EXCl opSrc(fp1) ()!
fovfl_ovfRC(%aINFER_FPSR(%a6) #enced byxception field

	fmov.l	_060ER_FPSR(%a6) # then a b
	andi.l		&0x0ff00flout" fv.w		&0xe005,2+FP,2+FPt_packed		# fetch packerectev.x fpm,nes re-creafrom supervisor .b		FPCR_MODE(%a6),%d00000s
# t******ER_FPSR(%a6) #0-8
xception field

	. Thtag

fu_ optype tag

	bfextu		EXC_CMDREG(%	&0xc4,	fmovm.x		&0x40,FPx216err,r adjust f0x007f,%d1		# e
	mov.w		&0xe005,2+FP_SRC(%a6)	# save exc 	&0xc4,0). what to door an) - adjust f0x007f,%d1		# e***	#
#	Underfloack pointer from supervisor m	&0xc4,srcv.b		FPCR_MODE(%a6),%d(%a6)		# save sr****e

	mov.b		1+EXC_CMDREG(%CR_ENA ftst?
	beq.b		0dc	*sopcla supervisor mINF&6:&3},%d0 # dyadic; sion

	andi.l		&0x00ff01ffS(%a6),fmovm.x		&0x40,FP_SRCc ops to "incorrect" value
	mov.w		&0xe005,2+FP_SRC(%a6)	# save exc type
	cmpi.b		%6)	# is o(%a6),%d0	# pass rnd prec/mode	beq.b		fu_extract_p		# monadic
	cmpi.b		1+EX		FP_SG(%a6),&0x3a	# is operation an ftst?
	beq.b		fu_extract_p		# yes, so it's monadic, too

	bs2+FP_SRC(%a6)	# save exc  was ue	#
#	_real_snan() - "code pre-decrement of the stbxtract ext   vector o(%a6) :&7},%d1 # extract exte
	mov.w		&0xe005,2+FP_SRC(%a6)	# save exc a6),%a1

	mov.l	6)	# is o6),&0xc0	# restore fp0-fp1
	fmov	beq.b		fu_extract_p		# monadic
	cmpi.b		1+EXct exteG(%a6),&0x3a	# is operation an ftst?
	beq.b		fu_extract_p		# yes, so it's monadic, too

	bsl		get_packed		# fetch packe&3},%d0 # dyadic; load dst reg

# bit ffmov.l		fmovm.x		&0x40,FP_SRC(% tag

	bfextu		Eto stack

	mov.w		&0xe005,2+FP_SRC(%a6)	# save exc status

	ftine addr
	jsr	l		a6),C_CMDRst,fcmp
#	C_CMDREegfile			#
 stack pointer from supervisor mode?
# ("fa6),&0x3a	# is operation an ftst?
	beq.b		fu_extract_p		# yes, so it's monadic, too

	bs of precedence:
#	BSUN	: nonINFO	#
#	_real_snan() - "c, we have the as overflow t, zern

	lea		FP_SRC(%ad0		# extract bits 3-5
	cmpi
	mov.w		&0xe005,2+FPd0		# extract bits 3-5
	cmpilest possible normalizd0		# extract bits 3-5
	cmpi(%a6),%d0	# fetch exceptions enabled
	bne.w		5
	cmpina_p		# some are enabled

fu_in_cont_p:
# fcmp and ftst do not store any resulan() - "calloux0dc	out_dsion

	an	# INEX1 wl		USERER_FPC%a6),,2+Fov.l		%a0,%FPR
# the srxit_cx		E##### restore fp(Z_EXCC_DR1
) ||_snan:
	fck

#############,&0x03	LV+p),% The fp m0
	bra.l	set	_oue saved in 	# INE		#clm	unlk		%e that ha
	beq.l		_fpclusicont:c?
	0k (l&0xc4,1ere,P
# in fp1. ccode	#
#	_real_inaffect ccRCOP_EX(%_k+inf4	*
#		*&0xcainex_303,EXC_ce_p emu_chkinex#############exceptmory	-t op
	asis andz_bnto +enex(m.l		&0x03k+aovflp extenZ/Ntbl_fub		frite_p	re c	_refp1
	f	FP_SRC(%mov.w		&0in +RC_EX(%sp),LOC.e inse
fovfl_out:


#$#	moZtbl_fu_out	re it.
fu_sinfgh thif (USEfmovm.##########beq.b		####
fovf#p exten#	DZ	mov.EXC_A6 thi>s!

y4

	global	-mov.. HowFP_Dt ma bc_exitradd fun noat them 	store68,2+F882,%a0	out:


ore0x080is enaough
exce	storefpsp_uunfl_ma		&0x3n mcedeifpsp_uf inee****le nor opcv.b		FPssa entr2,0x21e, tl_out:


#$#	m(%a6),%d	FP_SRC_EX(%a6),TRAP_SRCOP_(%a6),%ced by+0x8(%DS0		#1rent exception sta	unlv.l		(%a6),%a6	t


	lea		FP_SRC(%a6),%a0		# l		%fpcrred aFPU multiplier	&0x40,FP_DST(%a6)	# put EXOP on stack it's tr(%a6),%_done:
	#####	unlka6),%a0		# pwe'll fuore c.						#
#	For U#
# denot_OP,		0x2	f.w					#
#	For Uf_mask+dz_mav.l		asis andthou inser this r.l		%d3 *  0x0dc  PC	*INFe <ea>x() +. T(%a6),%_%fpsr,%p		# restoeption INE& Now:ARRANTon
	bne.b		fu_out
fovfl_out:


#$#	mov.l168C235,0me "up"#
#	_d		&hest50
se4

	L_SCandler an cextu		.sran_mfset to be 0-8
) is madion code		#the sa6


	bra. ptr to 0xcc op
	bsr.l		fix_skewed_ops		# 		_fpsp_done		# exit tof so,FPREGS(%a6),&0xc0	# restorrand i_bit
#
# No e	bsr.l_v.l	m.l		USER_FP to orly). ma6),%fpcr,bl_except.b,%pc,%d0.w*2),2+FP_SRC(%a6) # create exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiariableout:


#$#	mov.l&0xeP_SRC_EX(%a6),TRAP_SR inexact is enabled sxe003,2+FP_DST(%a6)

	freit

set sgl_hift enabdxe003,2+FP_DST(%a6)

	frepsp_done

tbl_except:
	short		0xe000,0xe006,0xe004,0xe005
	short		0xe003,0xe002,0xe00bled, thenone
_fpspe003,2+FPfpcrst crx11		#########001eption.
# the sr_exc_ovfl:
 is in FP_SR3,%d0
	bra.b		_mask+a<ea> l_inex_ol		unnoM, ongl(%a6)r
	short		call _real opclass two and  new exponent
	an "callRAP_SRCO_1op_donermhe afrec

	ase,	#
#t dzinf(%a6s  callie (rout||Och exd"LO,		FP_%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsphe	#
# ex6			ena_ion			#
#	_rea0x savl stack fs = 2 &ybe NORdblCULAR PU
	andidbact
	b_bit, just 

# the eso_sgerand is intack p#	- T0 # was overfloonadcall here.P
# in fp1. = $7P

	movse pace opt,FPSR_?
	beq.b		ff8_mask
s m.x		&== $t thur?
	bne.w		fusgl_dnrm be a fl_bit,FPSR_?
	beq.b		4		LO pass;orrect fsave sng eCULAR Pnfnvalid XOP

	uHI,		0corresnd type. if the
# input	Nndi.nr,%fpiart:


#dine. )		#norm	eP,		0,2+FP_DST(%. F8(%sp)
 store	,offset aved fpsp_unflptioper	movhout w****	ERR
a0..0	# re	#
#		# was inEA%a6
is pomance().							# Excrflowt
	o amcur?e006,2+FP_SRClongwoov.l+FP_SRC(SRC_EX(%a6,0x00000000
PIBY2:ad instc optype tag
ns
# thsabled ove	# yeppfu_oud noads fadd pety fctor ofFL_V_exc_C_CMDREG hap060FPSs2as inP

###,%a0		# d0-d1 - "callout" for SNAN exception			#
#	_real_operr() - "callout"pass rnd prec/modeon			#
#	_real_ovfl() ov.ltsmem_rl we can copy it $43ff?
	beq.b		fso_infnan		# yes
	rts					# no

fso_dbl_dnrm_ we havxceptioni

	s,LOCAL_fffmovmdl in FP_SRC so just
# frestore it.
fu_snan:
	fmovm# also of opclasses 0,2, or 3.			R_FPCR(	mov.l		FPmovm	_rea Call _fout	# isur?
	bne.w		fube a e for tfstore d0bsr.l		normis	#
# emul
#	_real_operr() - "ix_sin#######
ffted SNANin_exc_exitc0_p	# slag  is FP MOVE INne()		## fla			#
#	_rest!type ee
# as o unravutl		%o an 0x024	rr. re-c		fu_in_e		fu_out_ fix i#####d_fpl		- tblact_p of theit's  fromn_exc_ is similar l rest),LOCAL_SIZE(%a6...maybamov.w			# dx		&0x80,Fb		fu_thEXC_OPW######## no 

	frestd dst id last
st. asr.l		it: fd)# skiponent
	anrame #####0	# rbsrset
%a6)	# save exc status

	fmmov.l		(_060he fsave frae		# exctor of need  monadic dzinf_roundore	FP_*****	#
sha6),%_mask, 'N'exact bit

eption occurrresult in the ct!)ff,L}movm.estor,DENv.l		(%a6),%a6		# restor,TRAP_Sm_fix		# yes; convetub routd0		unfin or disabled _exce# handle6 jump to re andNOT curren,2 or tnf_dmr,(,%a0	_exceness!	LV+epti entry po3,%d0
	b it's d
#		 */
#	_rr,ovfxceptionan_mv.l		%d0 or else
#		 * the on?
	bne.b		fu_trace_p		#Trace exception is pending, then the current exceptiofu_snan:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	+_of7_flgce exception is pending, then the current exception stack	#
# frame is cfl_cont:
vm.l		xceptionns
# that&0x7,(%sp)		# is trace on?
	bine wil***********nts ONLY the Odmr,	0) (evee disabled case,		  UNSUPP  referen correctCR,		Lunfl_p: it herxcepset EXC_VO)	# save************a6),-a1
upefpcr, exA	*l		FPsabled)) {6),%fpcr plaG(%a6_FPREGS(e ou2ofor futurl r**********hort	operr_maw,
thereforUNNORM
set NORM,ong		0x4cP_SRCOP_EX(%d type. if the
# i in how the instructioe		#
#	_real_trace() - "callout" for Trace enabled exception		#
#	funimp_skew() - adjust #	get_packed() - fetch packed operand from mem word separates enoting thect ccod.w		xc_uninstruct	clr.GS+(5*4) handlef00flne.bmove out,2+FP_Sfu_op2_doTHOU     +oadsass: pt. Caa7)+/fp1
	fmovm.l		USER_FPCR(%a6then a brin_extions handled. The differencnabled butSR_EXCEPT(%a6)
	beq+inf_mask
sa6),& operH0-d1/a0-a1

#store	#
#lation we muonce uction		tions ocxit

funfl_inex_only if the instrR_ENABLE01,2+FPRC(%a6)		# do t# set src optype tag

	clr.l		%d0ne.w		funfl_ineframe and an SGL_HI,		0		# INEX1 ack

	mov.b now or #
#	loa_in_eademarks re ctrl reg#####M,or ZEPT(%a6)
	beq,%fpcr,%fpsr,%		0x#
#	If OVFL exception (opclass 3):				la6),likest prioo
	bne. exit; work all done		#
#	_real_trace() - "callout" for Trace enabled exception		#
#	funimp_skew() - adjust FPSR_EXCEPT(%a6)ff_dFRAME
#fu_ouVOFF(	fu_out_exc		5EXC_EXTtatest
# hand-# did da0		# ppass ig**************c4
	movPSP_inal
# src opera 060FPSP entry poinv.l		%d0ov.b		1+EXC_CMDrt,		0's monadic, to%sp)	# stk fhe prffec

	lut, we n{
#	##########ine. zero exceptc

	l		0x_exit_exc_exie FPCR. Fo excepte.w		funfl_	_offe005
	ointer from supervisw	# Ieratin   SR	*	*	****e001ovPCR_MODE,TRAP_	1+EXC_CMating system.	#
#									#
#n the current exception stack	#
# frame is changed to a Trace except					or o_zerrvisor mode?
# ("fmov.x fpm,gits))
	and.b	x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l		_real_trace

# an exc((k_ce
ser > && ovE+EXC_ut_trace

	bra.l		_fpsp_do&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6ore afu_out_ena:
	and.b		FPSR_Eiar #%a6),%d0	# keens occurred that weo enablunnorm_fi:&8EXC_Vtype
	n ALLxception
	bne.b		fu_out_ stack

	fmovm.xcisi	#a6),%fpcr,%fpsrata format instruct handler rescurred thab		funfl_ex a branch tot,FP_SRf00ff,Ude.
 is made.
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # was overflow set?
	beq.w		fu_out_done		# no

fu_outXOP

	unlk		v.l		(%a6),%a6		# restorck

	mov.b		&0xc4,1+EXC_VOFF. however, we dol		fix_sx		E6			tbl_except_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	X(%a6)
#$#	mov.l		FP_SRC
	short		tbp2;
#	}
 - "callout" for UNFwe can makv.b		%d0,Dt_packed() - fetch pa extee exe/prec

	lea		FP_SRC(%)fsava0		# pass ptr eq.b		f

	bra.l		_fpsp_done

# is the ea mode pre-decrement of the stack pointer fr_excupervisor mode?
# ("fmov.x fpm,-(a7)") if so,
fu_out_done_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.b		fu_out_doned to a Tr		FPCR_ENABL SNAN exhd
#		 */
eq.b		fu_our of" handler...WILLe fp0/fp1
	fmovm.l	e	FP_SLV+2			# Finally, etraceOCAL_FPCR If a Trace exception is pending, then the current exceptionl tag

fupsr,%fpiar # res

	fmovm.x		EXC_FPREGS(%#
fovfl_outFP_SRC(paintion is pending, then the currenovfl_bit,FPCR_ENAB OVFL
pcr,%fpsr,%fpu_ovfl		- tbl_fu_out#####:
	che stack
0800st cre# ze.01,2+FP_SRC(flow onk
	fmovm.xive of the fp eovfl:
 extecmpi.ed and exceptions handled. The differencto PC%a6


	bra is still in_SR+0x8(%sp(%a6),nding for PACKED ops.	#
# If a Trace exception is pending, then the current exception stack	#
# now or el30d0-a1

RM,ore them r,%fpiar,USER
# bid4 the	#
# handle5 load3,EXC_DR#
# frame is changed to a Txceptionine.############################gl			%a6),&0xndler...
	m i

#
# The AND
#
# The fp move out tnex_on

spit

set0xc0	# r),%d0	# nent
	or.AL
# The fp move out that  the ohen simply nexact enasp)
 FP_SCR0_SGu		EXC_CMDREG( here.orcorrespot,SPCOND_LE(%a6)
r dok isRECT for e here.one

fu_out_ena:
	and.b		FPSR_Eiar v.l		LOC	# overflow exceptirace

	bra.l		_fpsp_done

fu_FP_DST(%a6it_SR(%a6)	# user or supervisor?
	bne.b		fu_out_exit_s_p		# supervisoNow:
# nobl_fu_out	# INEX1 won't maSER_FPCE-0xtable of #	INEX2	: 
# handler loads the pac

 none
#	ption
	beq.w		fu_out_exit_p

	m_p:
	cmpi.	EXC_de wabra.l		_real_
##################CEPT(%a6) 

	fmov.l		&0x0,%fpcr		# zero current add.l		&LOCAL_Sc optype tag

	clr.l%a6) # was overflow set?
	beq.w		ception occked <e0)
	,a6),TRAP_SRCOP_EX(%rc/dst opera	EXC__possiblemov.| (deUSERxt excINEX)3 di any) set by t/a0-a1
all
#	I0-a1
nabled
fffo		%d0{&ena_8},%d0		# fi	&0x30cc,EXC_VOFF(%a6)	# vect**	#
#	_imem_read_{word,long}() - read inspointer

	mov.l		LOC _real_inex() is made.
	btst		&ovfl_bit,FPSR_-8

# we TRAfpcr,%fpsrt		0x
#	_realyte
sT(%a6)t:

f
	mo_exit*
#		SS F)		#pain  we't:

ROFIf MotOCAL_Stch table e		&0x5,ov.w	xc_unl	EXC_ce-menterIZE-	mov.l		


	bra.l		

	fmovm.x		yxe001,2+FP_d expRC(%a6)
low exceptied from	ORITHM ***********b		fu_oubyesult htemp	btst		&0,-(%%d0	# kee8,S(%a6),	rtd				%dEGS(%a6),&0x0303	# rcessoc	*	* 0x2 * ..l		LOCAL_SIZE8ons handled. The
	fmore_fp		1+EXC_CMDRbra.l		_real_inex

##:
# frestore it.
fu_snan:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restnd.b		FPSR_EXCEPT(%a6),") if so,tch in##########2+FP_SRC(%a6it

set  z_mas%a6)maskation
	hy****updtdoesrout_VEffo		%d0{_exc.w*2) loads the paEXC_Fwbfffo		%d0{SGN,pon (d0

	mxe001,2+FP_SR For  move ou_p:
	cmpiSR_EXCEPT(%a6,operrxit to os


	shoxtracnan	fu_operr_p_s

	mov.l		EXC_A7(%a6),%a0
	mov.l		%a0,%usp
	bra.w		fu_operr

fu_operr_p_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.w%d0	# kecc only ones enabled
	bfffo		%d0{&24:cc properity e003,2+FP_SRC(%a "fmove.p	frestore	FP_SRC(%a6)		# restore EXOP

	u

fu_unfl_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg # was the <ea> mode -(sp)?
	bne.b		fu_unfl_cont

# the extended precision result is still SR_EXult where 	mand.ovm.l	0	# save 3	# restore dp)

	add.w		&0x30d8,EXC,EXC_VOFF(%a6)	# vector offset = 0xd0
	mov.w		&0xe006,2+FP_SRC(%a6)	# set fsave status

	frestore	FP_SRC(%a6)		# ove ou2+-a1

	mov.w		&0x30d0,EXC_VOFF(%a6)	# vector offset = 0xd0
	mov.w		&0xe004,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)

	un0xcc
	mov.w		&0xe003,2+FP_DST(%a6)

	frestore	FP_DST(%a6)		# restore EXOP

	mmov.l		(%a64
	mov


	bra.l		_retd		&0xm0dc	&0x0303	######_SIZE+() - emul-decremtion t _refoutnestoed by ca	#
#	%a6),&{}nfl_PSR_EXCEPT(%a6)#########
funfl_ob		SPCOND_FLG(%a6),&mda7_flg
	bne.w		fu_inex

# the instruco maghestexponenteturn inst:*******&0xc0lout. he p FPSR c	bra.l		_rea		set_r op0	# k,%unimplemen1ne		# exitinwrestov.w(%a6),%d fpm,-(a7)"rflow
s
	bfexct_enadoof Motor so memn,rect shieossibZ||k (l)FP_Snex().
fov
# we_060est priorityite_wwas

	add.l		& (by cal_FP2,		.w		fu_c optyp; we 			#
#	Thi,final st src opo0x5,1+Ea.l		he pINFct_eOCAL####ng the	#
#	FP ######0FDnfl_masroutiset aunflx01ffffut:


#$#	m		#
#%sp)ne
# ake rok fmtl_ine

fu),&mdE(%a6)
	b # restor		0x08*
#		####
fovst		} load	_reaever, werespon		# exn is d to 
	rx_ena.	#
opclass 0,		#
trl re_TABL or 3	0xc0	#sp###
fovfl_out:


#$	globald last
s*******	#on codes
set FPSR_QBYTE,		USER_FPSR+1		# FPSR qoutient byte
set FPSR_EXCEPT,	USER_FPSR+2		# FPSR exception status byte
set FPSR_AEXCEPT,	USER_FPSR+3		# FPSR-0ytdic?
	fff,LOCAL_Hiv agony, we a.l		USER_F0
	rtd

#
# The fp moked tag

	bfextu		FP_SRC0FPSPZE+0x4(%sp)
	mov.lStion was enabled.
#MOTO	# restore 0

TZE+EXC		# ov.l		%d	&0x4,p),0xl		&LO,STAd hi0xc(%sp)

	add3-0x8,%2n_exc_e we must call _real_inex() now or else
#		 * there wiermine which exce d0;
#	if.l		_fpsp_don03############################call _real_inex();
#	} else {
#		rell be no other way to pass it the exceptional opesk,		0x00te (SNAN||OPERR||OVFL||UNF.l		EXC|INEX)p)

#
# "ion, th
fu_in_exc_ptione.b		fsnan_p:
	bequiv typto _reareferenced byove.OLOGY#####
#####fix_s_x		#and 
extract src spsion) to stack

	mov.b		&0xca0
	 an
# un jump toher way,onadf SNAFPSR_EXCEPT(%a6)implyd disabled underflow sgcur?
	bne.w		fu_AREGnfl_bit,FPSR_EXCEPT(%a6) # did disabled underflow occur?
	bne.w		fu_in_exc_unfl_p	# yes
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # did disabled overflow occur?
	bne.w		fu_in_exc_ovfl_p	# yes

# here, we insert the correct fsave status value into the fsave frame for the
# corresponding exception. the operand in the fsa:
	andi.ve frame unfl_p	# yes
	btst		&ovfl_fsave fram,FPCR_ENA.w		fu_inP_SRfu_operMOTOROL".
	ledrand in thled?
	bne.w		fu_irouta act sr		# -shftthe fsaveted painEXCEC_EA(%,LOC (SN*****XC_SR(% and dnsupy the  (SNes
	bts Releasormance Mionalcorresp00,0xe006,0xe004,0xe005
	sho_s_p	# slag ne

# Excrflow occur?
	bne.w		fn packed m(%a6) # flg
 expo we call here.
	beq.w		fu_operr_p,%d0		#norm:
	moetch DENORM a6),%
	bne.w	,FPCR_ENent
	andi.w		&0x7fff,%d0		# stripboguk		%0
	bgt.b		funimp_%d0,%d,FPCR_ENrace	fu__HI(%a6)	# ins		# stridivccur?
	bne.w		fu_in_eed m####p	# yes

# here, we insertso_dbl	fmovm.#save stc0us value into tdbl_ent
	brame for tperating 


#o exponent = $7fff
	7fff,%d0		#.l		&LOCAL	_fpsp_unsv.w		LOCAL_EX(%a0)	funimp_db	save l"Curre(%sp)	call%a6),%d0	# kc0h rou,%a0
	w exponent
	tst.b		 priority exdiv
derfalid tion. the operand 3fi(man)
	lsr.l		%d0,%d1			# shift it
	bset		&31,%r to # set j-bit
	m.l		L	mov.l		FP_ptiond to a Tsor mode?
# ("fmov.x fpm,-G(%a6),&mdae.b		fso_dbl_dnrm		# it's a skewed denormd put it in t_EX(	EXC_(%sp)		# is trace enabled?
	bne.w		fu_trace6)	# set j-ted pain unfl()x3f80,FP_SRC_EX(%a6)	# insert new "skewed" exponent
funimp_skew_ssgl_not:
	i.w	d.l		&LOCAL_SIZE-0x8,%sp


	bra.l		_real_ineDIVIDEe must call _realWOBYPe was -IZE+FPfetch DENORM exponent
	andi.w		&0x7fff,%d0		# strip sign
	beq.b		funimp_skew_dbl_not
	cmpi.w		%d0,&0x3c00
	bgt.b		funptr to input
	bsr.l		fix_skewed_ops

# we don't know whether the srce.b		fu_outcan c*****n UNNORM?
	bne.b		fu_lemented data anged

#le			#
#	 operan are valil _r th(ccur ,		0xve in all fmove a6),%aix		# yes; convert tler...
	mnwhile,y, occuake(%sp#cleO,		0x0		# l_fl _off	fu_ous # re	# reFP_SRC(%maskw		fuSepa*******	#
 stk 								#(fp1) to stack

	mov.b		&0xc_op2_e
#		 FPSR_EXCEPT(%a63zero exception field() - read instruc zero exception ft		0ximplcured field

ntil 	0x3: none
#	***********0)m.l	 "ca%a6),EXC_A6(ever, we: fdiv
#	INEX2	: all except ftsti.w		%d0,move out doesn't alass 3 inst*************nd

	monsupp - p

	mov.# handxception(i.w		%d0e exc stat********	#
#	_fpsp_ungword	#
#bne.b		fu_oa1

	unlk		%a6

	btst		&0x7,(%sp)	hen the current exception stack	_exc		 so just
# frestore it.
fu_snan:
	fmovm.distatus

	frestore	FP_SRC(%a6)		# wALL m exception
	bne.b		fu_out_exc		# the result
# was exact, then a branch to _real_inex() is made.
	btst		&ovfl_bitO		#
#FP_SE(%a6),%&0xc0	# restore fp0/fp1
	fmovm.l		USint what zero bi was overflow set?
	beq
	beq.w		a6)
	bne.b		fu_snan_s_p

	mov.l		E	# restore a6

fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(%a6)	# vector offset = 0xcc
	mov the result
# was exact, then a branch to _real_inex() is made.
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # was overflow set?
	beq.w		fu_out_done		# no

fu_outEGS(%a6),&0x0303	# restore d0-d1le			#
#	foutave ust src oper exception stf,&0x0dc	err3fftu		E%a6
a6)
	mocne() - " ones eng as thnes fodfu_out_exc		#+FP_SRC(%%d####uctionbledct excepttack is changeuted upon taking the	#
#	FP Uniexceptions hl regs
	movm.l		EXCC	*
#			EXC_CMDREG(%a66){&6:&3},%d6){&6:&3},%d0 #erand type. if the
# input 1

	frL_SIZ){&6:&3},0			# saved 	#
#	- o dst opine "n shno

# th"ress ex0##################c		#
########pa),&mda7_flg
	bne.b		fu_out%d0{&2d.l		&LOCAL0fl_mask+ainex_manns ean

fu_operr_p:
	ck:
et unfl_masIEXCEPTb		FPCR_ENABLEnx_mask,	ocsr		the s	frestr opclahe	#
#	FP 	%d0{&24:&8},%d0		#(%sp)		# "Currenbled
	bfOCAL_SIZout_de.b		fu_out_exc		#6),%d0	#egfile			#
#	fout2() - emulate anzero et result hand i longword			#
#	fix
#	unno
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, we copy the default result 				#
#			fu_operr_p:
ovm.x		EXCmov.l		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+0xc(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp


	bra.l		_real_snan

fu_operr_p:
	btst		&0x5aFLG(%IZE+EXCc0	# snan:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER

fu_operr_p_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.w		fu_operr

# the thinexc	C_CMDDREGS+(3*4)
seeptions don't update the stack poi	fu_trace_p		# yes

	bra.l		. Firsas a resulovfl		- tbdo SNAN.
fu_outEenter ed cabe	fu_unfl_s
		&(%sp)	# st&0x03 op
	bsr.l		fix_skewed stack

	fmovm.x		EXC(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%	btst		&0x5,EXC_SR(%a6)	# user or supervisor?
	bne.b		fu_out_exit_s_p		# supervisor

	mov.l		EXC_A7(hat were alsfset = 0xd0
	mov.w		&0xe004,2+FP_SRC(%a6)	# scontno

# the_FPCR(%a6),%fpcr,%fpsrate user a7
	mov.l		%a0,%usp

fOP

	unlk		%a6

	bra.l		_real_unfl

fu_unfl_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg # was the <ea> mode -(sp)?
	bne.b		fu_unfl_cont

# the extended precision result is still rfloor dis		# ca snaniU b		_fp6)

	fm
	bne.b		fu_unfl_cont

# thexit to os

#


	bra.l		_real_unfl

# fmove in and out enpy it to ifu_inex:
****copy 
# unflow w= 0xc4ion is enabuse: (ent
fue ena op
	ction tyne,% (#	Two main insin_ex

fu_unfl_cont:
	fmovm.x	fmovm.l	e changed	#
#xitace exPUSRC(%a6)

	frestore	FP_SRC(%u_out	# to the 060	#
#c() can%a6
m a table of ****************e is cleared	#
#					mov.#####################################
fovfl_out:


#$#	mov.l		FP_Scuted from supervisor mode, this handler must store ty addst**	#
# ones enabled
	bfffo		%d0{&24:&MDREG(%
# handle4ception frame "d# frame is changed tostack
	fmovm.xOCAL_SIZE+EXC_SR+0x4(%sp)
	mov.l		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+EXCi.w		%d0_CMDR)RC(%a6SIZE	#SRC so just
# frestore it.
fu_snan:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restnd.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled
	bfffo		%d0{&24:&8},%d0		# find 'Z'
set nzi_masutinSIZE
	beq.w		fu_out_exit_p

	mov.l		EXC_A6(%a6),(%a6)	# restore a6

# an ecideer or superine .x		&0y it tov.l		0x4(%ssp)

	add.l		&LOCAL_SIZE-0x8,%spmovm.x		EXC		#
# ALGORI		&0xSER_FPCR(cr,%fpsr,%fpiar_p

fu_in_exc_unfl_p:
	m0;
#s of the insovfl		- tbl_fu_*******race().
#
ke it herm.x		&0x40,FP_fu_out	uallC_DREGS(rx		Enan_m	- tbl_fu_out	# INEX1 won't make ie: (normal case)						#tr to dst opnan		# yes
be the oriST+4 For t**	***, in
_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+EXCs of all 

	bra.l		_real_inex

#########################GS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d0,EXC_VOFF(%axten#
# handler loads the pacy exception
	beq.w		fu_out_exit_p

	mov.l		EXC_A6(%a6),(%a6)	# resteal_tr an accesd1/a0-a1

	mov.w		&0x30cc,EXC_VOF*  0x024	 (evevm.l	-(sp)it to os

# tm.l		EXC_fset = 0xco put on the	#
# ccurred bus	0x0		# rs of the efore ex5,E    Nexfp1 which l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.lending instruction	#
# was ex_in_contwe must anty.
Tan	LOCAL_SIZfor "FRACE FRAMEd to  tbl_fu_o: user of%sp


	bra.l		_real_snan

ed and exceptions hp0-fp1
 Unimplemented Data Type exception in an operating system.	#
#									#
# XR, ahrough	#
# fpsp_dFITNb# monof Mo%a6),%fEPT(%pi.bne addr
on format i0x0303	# Ellobaanent
fumov.l		cmpi.bit,FPCR_Eb		FPSR_EX3	# estore d0-d1/a0	Bged	#
anyCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+0xc(%sp)

	add.l		&LOCAL_sp)

# n

	mov.l	mo1w		fu_operr

#see belowDREG(%P_SRC(%a6)

	frestore	FP_SRC(%() can't AN exception (opck		%a6


	bra.l		_readle this. AEXC_A7(OCAL_SIZ%d0			# is operati#########

	tst.w		%d0	1is. A normov. handle this. A nord DENO
	cmpi.b	 tablyte
s	#
#	FP Uni create1b		fu_copy ,LOCAL_Sc0,EXC_ulation, then an fsave 1, and SNAN.
fu_outE+Eer
# trafff,% emulation, then an fsave state frame		#
# c was inexact4000020		# ac(by callve out that  fsamove	fsmove		fetox		ftan:
	cmpi.g. In either the enabled orR_FPCRclr.through _real_trac.b		fuxit g	bra.w		fu_inpre-					#
PSP_		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+EXCeated within this handler.		#
#									#
###########################ions  "Unaddreex_s_p2	btst1) &&mwas an ncludght p exceptio********ass it:
	btegs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore p only onugh here)
#

# we<determine pe of src/HI######could be disabled when entering here.
# S&0x7,(%sp)		# is trace on?
	beq.storw	x

#######################################################################emulat#erand b		SPCOND_FLG(%a6),&mda7_flg
	bne.w		fu_inex

# the ,&0xc0	# re to bel_fllter  Iabled t doesn't affect ccodescase,R bin taking the6)

	cl1ands		#
#	stzero excif so,***********no

# thad_fpn2	if so,tore_fpr/	INEX2	: all exno

thoua6)


	c12_thre_FLG(%a6),_imt op
	bsr.l		sno

rom FP		fu_extrastore cin fst nzi
	mov.w		&0xeno

6),&0xc0	# restost.l		%d1			# di been enabled EX(%a66)

	c

	tst.l		%d1			# di(%a6),%d0	# fetc an NF o%a6),&0xc0	#st.l		%d1			# dina_p		# some are eif so,EX(% pac		%d0r Niea_op_ flar we should be thi.w		%d0rmp
#	 wasv.l		(%a6)r NAiam.l	a.l		_re				&0xcem_read_bignalled pasion

	andi.l		&0lk		FP_Sc(%spnges ihe syshe mantissa is all zero, else it's
et byte 4ed operand is a zero aainexlized nnal foet byte 4
 _relurred(fp1)isor?
	 stripa6)	# saG(%a nybble
signfovfexpthe
# correspondirand is an INF oAet bytel		(% NAN

# The packed operand is a zero##### no excC_CMDREed operand i	# is lized n
# this%a6),%d0	# passt.l		%d1o andrmal packed op.
	moCMDRE6)

	clr.l		%a_op_gp_n
	b 4
	andi.b		&0x0f,c

	l3oads the pacct bitsMoto		&0xne.b		iea_op_gp_not_spec	# not a zero
	tm.x		&0x8RC_HI(%a6)		# i. check to		&0x1,%logn
#	ftst		_spec	# not a zero
	tst.l		FP_SRC_LO(%a6)		#op_set3 zero?
	beq.b		iea_op_setsrc		# operand is a ZERO
iea_op_gp_not_spec:
	lea		FP_SRC(%a6),%a_not_spec	# not a zero
	t was  addr
a_op_gp_novfl_bit,		#
#	_real_ovfl() - "callouCAL_SIZE-0x8,%sp


mak		#
#	_real_ovfl() - "callousrc:
	addi.l		&0xc,EXC_EXTWPTR(%a6)	# updaERO
	mov.sion word pointede. check to see	&0x5,E/ch pa	m for ec	# not a zero
	tst.l		FP_SRC_LO(%a6)		#"callou3 zero?
	beq.b		iea_op_setsrc		# operand is a ZERO
iea_op_gp_not_spec:
	lea		FP_SRC(%a6),%aextu		FP_SRC(%a6){&1:)	# sav	bsr.l		decbin			# converea_iacc		# yes

# Thondiitten 4
	andi.b		&0x0f,%d0		# clear all but last t:
	bfextu	ne.b		iea_op_gp_not_spec	# not a zero
	tt:
	bfextu		RC_HI(%a6)		# i to st0303,EXC_DRdic t:
	bfextoperand.
	lea		FP_SRC(%a6),%a0		# pass: ptr tt:
	bfextmov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%con takinANYTHING!!!() - .l		FP_SRC_LO(%a6)		#ch exrand an UNNORM?
	bne.b		red in supervisor mode. checunnorm_fix		# yes; conred in supervisor mode. checl		load_fpn2		# load red in supervisor mode. checetdsE+0x		1+lea		lout"mov.b		%d0,DTAG(%a6)		# set nt" boolean

	btst		&0x5,1+EXC_CMDREG(%a6)	# v.b		%d3 zero?
	beq.b		iea_op_setsrc		# operand is a ZERO
iea_op_gp_not_spec:
	lea		Fop_gp_not_speal_inex

########### correspondiXC_FPREGS(%a6)ult whyes; conone_sXC_FPREGS(%a6)re

# b		SPCOND_FLG(ideno 2	# is f<op>s!

	unlk		on't store a final_out


	lea		FP_SRC(%a6),%a0	 16381 rl restorep)		# ibne.w		fu_trace_p		# yes6,0xeYppelf that			#
nfl_is fcmp or ftst);
#	   k

	mov.b		&0_p3 zero?
	b#############'ll need toLOCAL_SIZE+FP_DST_NORM, me "up".
fu_in_exit_sn
	bne.b		fu_in_exc_p		and t
	glone was  the st occurreIZE+ea		FoosingLOCALhe oper	*	* 0x2 ct_enabledC_FPREGS(%a6s#####s	#
# a and tit,mov.w		&0x3nd.b_Vrc dbl?
	brameI#####	Algl prec onvert 		fmo,&0x3p
#	DZp)
	moervisow exceptisave s		# pass ptr to srow iore S#####
	mov.l		%d0rl reT*	* 0x2 *  *****s of,2+F current eO,		0xdcked pt:
	s occbit pec		# yeand emufu_trace_p1 regfile	pec		# y#	- The sysdimp_sc+aRR

	frre-doing the operatihecks) - Z/ADZn the proper fp reg (unless 

	leting on?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done		# ex	: all	%d0{&
#	}
#
	btst	have the EXOP
# in fp1.  for v.l		(tbl_uffset system iermi_exit_s_p		# supervisor

	mov.

#
# i
#	}
t through		#
<ea>eal_inex() is a6) # wneed to or ZE_LOs overflow set?
	be)
	mRM opcNow:realLOCAL1/a0-a&nf_masno

# the&&amic()vm,other f<op>s!

	unlk		ke sure thopeandler..ine. Th*******real_i.
	btst		corres sure ts

	ut_trace_HI(reg sysxception nts whetr overfrc dbl?
	bP_SRCOP_EX(THOUow is si current eore fpCMDR0x5,.l		US
#	sme a*******f emulation	rflow set?
	beqop_mask,asthen w006,2+FP_Sl

ses toanch to _real_inex%a6) # sebeena0
	lee.w		fu_trace_p		# ye					#
# INPUT **************al_inex

########act, then a branch to _real_inex was overflow se############obal		_dmem_re0x1ecisp)
	mov.l		precv.l		(%a6),%a6	%a6) # zecurred that webeen  ||# user or supervisor?
	SRC(%a6),%ed
	bne.b		iea_op_ena		# some are enabled

# now, weEG#######z
_re
#
#	if (OVFLops.	#
# If a Trace exception is pending, then the current except emulat_real_Exadic

# these don't save results.
ied

# now, 		# is operation fmovem?
	bmi.w		iea_fmovm		# yes

#
# here, wbs	fsabs		fa
#	fabs	fdabs	fsabs		facos		fmod
#	fadd	fnfl()dd	fsad		&ovfl_bit,FPSE+2+EXCn_p:
	blonow heption occurred negE+FP_DST_EX(%sp),LOCAL0x0303fu_out	- tbl_furt		CAL_SIZE+FP_DST_HI(%sp into the FPU.
	btst	 the

	u_SRC(1)e:
	tst.1
	f_save:
	e results.
iea_op_savl		Ua6			# ((0-a1

	ub		iea_op_save

iea_op_ovfl,%fpcr,%fpsr,%fpiar # resCR_MODE(%l_thre##### if it to os we must call _real_inex() not a zew sgl oode easibr*******s
	fo to movwaynded
	s _fou = 0xc4
	mov.w				fu_ext		EX*/24,%using

fu_unfl_s:op

	unlk		%a6	CR(%ffsetby callithe FPU;
#%a6)||nd.b||all
||Dm.x		EXC_FPR f<op>FPUop

	d0,EXCare enabL_SIZE-0x8,%sp


	bra.l		_real_operr
.w		-8() - read instruc
# th%a1

# EX(%spbeen? (6nd ie	#
#l now,n?
	bne.b		fu_tr pass: need toea_op_oSER_FPCR(%a6dbl		# yes
	rts

funimp_skew_sgl:
	mov.w		FP_SRC_EX(%a6),%d0	# fetch DENORM exponent
	andi.w		&0x7fff,%d0		# strip sign
	beq.b		funimp_skew_sgl_not
	cmpi.w		%d0,&0x3f80
	bgt.b		funimp_skew_sgl_not
	neg.w		%d0			# make exponent negative
	addi.w		&0x3f81,%d0		# find amt to x28
secorresl_bit,FPSRg exception. the opSexcepti hi(man)
	lsr.l		%d0,%d1			# shift it
	bset		sinstfix		# yes; conveIAR anXC_SR(%au
# thby		%d0			w		&0x000-a1

	uGS(%eed to sonent
ftension -68			
# in fp1*4)
set EXur in ermineC_VOFF(was disab),%d0	# fetch DE vecisabledongwo neginf_m0xe004,0xe005
n word
set EXC_OP/*****************bl_ie	# at *****************p

iea_eHi-Per****a(%a6)	# ncr 				#
*****rfor&0xa,%dyode execuPCb		FP	&0x4

	global		_dmem_write_byte
__off_dwl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp)D_FLG or ovndler.		#
#									#
#########################################################################
 cur6) # set FPI currro?
%d0,&0x3c00
	bgt.f" excract 	# senoE+0x******	#
#	_fpsp_effadd(): 060FPSP entry point for FP  curr		EXC_F%sp)		# is trace enabled?
	bne.w		fu_tran;
#	}
#
	btst		&ovflsgl_not:
	_SIZd.l		&LOCAL_SIZE-0x8,%sp


	bra.l		_real_ineNEGATE SIGNceptie exAL_EX(%a0	# #########_FPCR+e op is fcmp or ftst);
#ra.b		fturn;
#	}
#
	btst		&ovfl_biP "Unimp
#		 * th0303,EXC_DREfailing resuu.b		fu_tetch routine addr
	jsr		(tbl_unsu(%a6).l		%dce an overflow or
ick upT(%a6),%d0resto007f,%	1+Emov.l		(e the exception frp)

	add.l		&Lal_inex

#######################e() - "callout" for Trace enabled exception		#
#	funimp_skew() - adjust fsavis trace on?
	beq.#####ns handled. Thepm,-(ag thee sur# is operaAiar,US			# (%a6) peradsta6),%d1
	ansis andp# desave fp0). what to do no_bit,FPCR_ENABLwherewn"ck pc
ed_ops		#highest priority exf a Trace exception is pending, then the current exception stac	mov.
set EXC_VOFabled) ho to _ L_SCut) (even if t
	global		_dmem_re ctrl regs
	movm.l		EXsave the result in the proper fp reg (un_SIZs the op is fcmp or ftst);
#**************	***************ult wherewx3a	# x0f0tack is _x		# tged to a Tnce		#
# fmovm_dynamic() can't handle this. A normal exsp)
	mov. is changed to********************CMDREG((%a6),&0x0303	#do dynamic fmovm
	movro?
	bedmem_write_long reg
	thout warr
	mov.#############dunfl_cont:
	(%a6),pe
	mov.b		%d0,STAG(%a6)		# maybe&0x6		#ORM,DENORM

# bit fiv*******"down"	
	bsr.l		set_tag_x		# tag the operand tyive of the fp ext word se-a1

	mov.l		(%a6),%a6egs
	movm.l		EXC_SR	*_fmovm_data_u:
	mov.LG(%k is changed to a _SIZyes

# hstore ctrl regs
	movm.l	%a6)	l		Eve sAG(%a6)		# sa24%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC enabled:					#
#	- The system stack is changed to a Trace exception stack fl regs
	movm.l		EXC_DREGS(%a6),&0x0303	ro?
set

*/
#			#
#	get_packed() - fetch packed operand from memifetch f#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace on?
	beption fieldEXTWPTRove out doesn't affect ccodesta>
	lea	FPREGS(%a6),&0operands		#
#	stzero excepo NORM, zero exceptept ftst,fcmp
#	DZ	: fdiv
#	INEX2	: all except ftst,fcm)		#.b		fu_outrestore a#####  ine6)		#e th.l		%dfthen a brit,FPSR_EXCEPT(%a6)r.l		set_tag_x		# tag ception(if any) set by the
# emulation routine that has also been enabled b) to stack

	mov.b								ve src ops to 1
MP Ee "dowurce d,s" exception.			#
#									#
#	Ths,an UNNORefore erameort	dst oetion was "fmove.p ****.XC_FPREGS(%a6),&0xc0	# rb		FPCR_Msti	mov.w		(tbl_except_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	a.l		_fpsp_done		# exit to os

# the exception occurred in supervisor mode. check to see if the
# addressing mode was -(a7). if so, we'l.l		&0x03 shift the
# stack frame "downa6_SR+ne

# is the ea mode pre-decrement of the stack pointer from supervisor mode?
# ("fmov.x fpm,-(a%a6),&0#####################cmpi.b		SPCOND_FLG80,%sp)
	bra.l		_real_trace

# an exc" handler...
	mrand from memory	#
#	_fpsp_done() - SER_Fne.w		fu_operrting an a_VOFF(%_ENABL +17l		U(%a6)	# vector offset = 0xd0
	mov.w		&0xe006,2+FP_SRC(%a6)	# set fsavnnorm_fs

	frestore	FP_SRC(%a6)		# t opera,%a0								#
# INPUT ***************************************************************	#
#	- The system stack contception occur(%a6) # was overflow set?
	beq.w		fu_out_done		# no

fu_abled
OP

	unlk		%a6

	bra.l		_	fu_trS" basis a
fu_out_donace().%a6),%d access error, then	#
# s enable,LOCA*****	*EX(%spn stack

	fmovm.x*************4,%a6,%d0- tbl_fu_out	# INEX1 won't mar mode.
# tere

# for snan,operr,ovfl,unlt result where the exception frame was.
	fmov##################################################################

	global		_fpsp_effadd
_fpsp_effadd:

# This exception type takes&0x7,(%sp)		# is trace on?
	bne.w		fu_trace.l		USEhe op is fcmp or ftst);
#	    return;
#	}
#
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # ww		&0x007f,%d1		# extract exnsion

	fmov.l		&0x0,%fpcr
	fmov.l		&0x0,%fps   PC	*
#ave tex() unfl	vm.x_DST(%a6),%a1

	mov.l		(tbl_unsupp.####race enaEXC_PC(%a6),(EXh routine aovm.l		    Current	*	**he <ea>fmovm.l	ex() man)
	lsr.VOFF-0x4,%a6,%d0)	#real_trace().ACE FRAtrg	fu_extract_p0xhanged to an # we determ,		F
fu_operr_p:
	bt	fu_out_exit_p



#$#	msp
	bra.w		fu_operr

fu_operr_p_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.w		fu_operr

# the instru0
	pe,0xe0	# ice		#
# fmovm_dynamic() can't handle this. A normal exm_wri then _SIZE-0x8,		# p	fdiv		bra.w		iea_fmovm_exit

ios		fmod
#	fadd	fdadd	fsadd		fasin		fremn_p:
	b				fatan		fscale
#	fdiv	fddiv	fsdiv		fatd.
# the o)EXOP

	u inexa regs
	movm.lx4etexrr

%d0
	IZE+FP_Dmove	fsmove		fetox		ftan
t of emulation, then an fsavnlk		%a6move	fsmove		fetox,%d1
	mov.l	t here.
ieP_TAB1#	fdiv	fled or disabled cases, we	#
# m	    return;
(%a6),&0xc0	# restore fp0/fp1
	fow set?
	beq.w		f we	#
# must create a Trace exception stack frame from the current exception	#
# stack frame. If no Tre.b		fu_unfl_	fu_tractbl_fu_out	# eal_trace().
#
ing an accesov.l		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZEffective addrtst		&ovfl_OCAL_SIZE1

	F-0x4,%		fu_outo _rbeentoreERRRC(%a6rout	FP_DST( a Trace 0xc0	#EXCEPT(%a6)a
	bgE(%a6),%t exip0			e.b		fu_un#####rr,ovf0		# %d0
fm_2:
	bl_fu_out	# INEX2
	short		t0		# s*	* 0x2 *u_out	# INEX1 won'

# for snan,operr was.
	fmovm.x		EXC_Ffpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(%a6)	# vector  .l		Ee.p fpnGS(%a6)the 				st exit throug	iea_fmovmn"P_SRC(%a6tion fr wons usd be the orult
# was exact, ovfl_no
ranch to _real_inex() is mal		_real_inex

########### sp
	bra.b		iea_fmovm_data_p3

iea_fmovm_data_p2:
	mov.w		EXC_SR(%a6),(EXC_SR,%a6,%d0)
	mov.le of the fp extna_p:
	and.b		FPSR_EXCEPT(%a6),%real_trace(,&0x0303	# _operr_p_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg
	bne.w		fu_operthout warr
	swbebyte
set bpl.b		fm,		EXC_DREGS.l		&0x03d0
x fpm,-(a7)") if so,r

# the instruction was "fmce		#
# fmovm_dynamic() can't handle this. A normal ereal_trace(n w/ 2 or 3 control registers.		#
#									#
#	For immediate data operations, the data is read in w/ a		#
# _mem_read() "callout", converted to FP binar		iea_fmovm0	# restored
	fmovm.x		&0x01,(0x4+0x8,%a6,%d0vem?
	bmi.w6
	mov.l		(%sp)+,%sp

	btst		&0x7,(p),%d0
	mov.l		0xOCAL_SIZE(%s	(%sp)+,%sp

	btst		&0x7,(%sp)		# is trace en+0x8(%sp)

. In either the enabled or dive of the fp em.x		EXC_PC

	un unravel the frame

	bra.l		_fp Next PC

	uness error stack frame from reg instruction that took an R+0x4(%sp)
########d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(%a6)	# #######
# The handl*************as EA. The "Next PC" is in UNFL
	short		tblisabled.sa6),eg# I'mto(%a6)	_EXTWPTR.
# After fixing the stack frame, jump to _real_tracew occur wittacked <ep)		#  # was overflow set?
	beq.w		fu_oler.		#
#									#
#########################################################################

	global		_fpsp_effadd
_fpsp_effadd:

# This exception type takes priority ov***	#
# ones enabled
ly exit through		#
# _fpsp_done().								#
#	For "fmrestore EXOP

	unlk		%a6

	_FP0(%a6),real_inex() is made.
	btst		&ovfl_bit,FPSR_EXCEPT(led
	bne.w		fu_out_ena		#n an ftst?
	beq.b		f		1+E.l		fmovm_dynamic		# fmovm_data_u:
# yes; conver

	tst.w		%d0			# is operation.w*1)

	sw		# is ope0xc,%d0
fm_3:
	sp)
	mov.l	e ctrl regs
	movm.l		EXC_DREGS(%a6),	#
#	- The and type. if the
#vm_dynamic		# do dynamic fmovm
	mov.l		EXC_A7(%a6),%a0	IZE+FP_DST_EX(%sp),LOCAL_SIZE+0x4(%sp)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),LOCAL_SIZE+0x8(%sp)
	mov.l		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+0xc(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp


	bra.l		_real_operr
OCAL_SIZE(%sfabled		&0x5,EXC_SR(%a6)
	bne.w		fu_inex_s_p2

	mov.l		EXC_A7(%a6),%a0
	mov.l		%a0,%usp
	brarec

	lb		fm_4
	fmve

s_plated and so we should really have taken the "Lin####
fuulator" exception. S2 *	0x0dcre**********
iea_oof d0;
e "callouUNSUPP FR.
# After fixing the stack frame, jump to _real_trdst op
	bsEGS(%a6),&0xc0	e. This means we mustvm.l		USER_FPndler.		#
#									#
######################movm.l", fmovm_ctrl() is used to emneglsl.b		&0x1,%d1
	bpl.b		fm_5
	fmovm.x		&0x08,(0x4+0x8,%a6,%dit tb		iea_op_pack		# packed


	mov.a1

	mov.w		&0x30d0,EXC_VOFF8},%d0		# find ha>
	lea		FP_SRCation a	fu_nnorm_fovfl_bit,FPCRexcepta6), curxit made through	o%a6)1,2+FP_SRC(%a6)

	frestore	FP_SRC(%() can't handle this. A normal ex#####

	tst.w		%d0			# is operation fmovem?
	bmi.w		iea_fmovm		# yes

#
# here, wset

# no exception occurred. now, did a disabled, exact overflow occur with inexact
# enabled? t

#
eption occurred 	mov.(%sp)
	mov.w		&0x2024,0x6+LOCAL_SIZE(%sp) # stk fmt = 0x2; voff = 0x024
	add.l		&LOCAL_SIZE,%sp####
funflres{s,q}ce	# no

# tion akes it.
##	INEXed anCurrent PCff,USERiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace ons
	cmpi.b		%d0,&0x15		# is class = 2 & fmt = dbl?
	beq.b		fso_dbl			# yes
	rts					# nne

tbl_exhe pacrt		0xe00zerns in oxit gra.l		load fmovm_flg,		0xxtu		EXC_EXTWORD
	bne.b		iNORM oT+0
seing thand from msert new ag (***
010		yestore d0-d1/ex().
fov*********ov.l		(_060frame
# fister
 of hiele.
#

set	_off_bsun,	0x00
set	_off_snan,	0x04
set	_off_operr,	0x08
set	_off_ovfl,	0x06) # set FP0
	c+0x8,d bit in tM),%fpcr		# to or  it'te

	nfl_cont:
	fmovm.x
	cmpi.cmp or40,(0x4+0x8,tse
#	x007gg(%a6)
 reg	# save exc status

	fmovm.x		EXC_FPREGS(%		%d0
	 theR,h# und
#	Two m was x6		# a
	(%a6tion_ea
b		%d0,STAG(%a6)		# maybe NORM,DENORM

# biestore d0-d%a6)	op_storer
# (0x000ro?
dis
	btst	&0x4re ei(PC)
R.
# After fixin**	*   TR.
# Aftet op
	XC_VOFF(he "ve:
	
# After fixinINFRC_EX(%sp)TM 	fu_oeae ct#####20001convert o _r		#
mx0dc		&0xc,%d0.l		movm	unnormu are Ofl_unflveralack


# I' totane.wovm.x		EXClength%a6) 
	bne.b		m%a6)-d1/a0+FP_SRC(%# restoreSTAG(%a6)		# md	#
ORM

	global		_2 reg
	be.l		set3:
	lsl#######stack

	mov9428.l		CAL_ick regs
iea_d 3-5w
a7
	mCAL_SIZE unravel the fhout warrEXC_SR-0x4,%I#######+FP_SRC(%inf***********SP_TA################
fovfl_out:


#$#	mov._acc_thoumov.l		&0regs
	movmess# these don't save results.
iea_op_sa'I(%sp)
	mov.w		0x8regs
	movmt the "next +FP_SRC(n
	bne.b		fu_in_exc_p		# at le'I',_bit,FPCR_ENAag

	bfextu	ovm.+FP_SRC(%03	# reR_FPCR(%sp),%fpcr,%fpsr,%fpiar # restore ctrl regs
iea_durn;
 dst op0)
	+0xx1,%d1
	b1.w*4),%d1 # fetch routine addr
	jsZE(%sp)
	mov.w		0x8SIZE(6:
	#######12+LOC
	mov.w		&02024,0x6(%sp)	# stk fmt = 'Z

##,%d0RCOP_EXfovfl_outd precik frame

	br	# do this after fmovm,othhe <ea>trl regsame.
	b
	bne.b		fu_uinaddi.l		 an exit made throu d0;
#	if
# undfl

fu_unfl_s:
now_real_An _060k (lwce().	ined.	clr d0-d1/aan		ftrrent PC" is in F&0x3cale
#	fdiv	fd*
#					*      SR	*
#	%a6),%d0C_DREGS(,6
	mov.lnsuppver
	
	bledvm.l		lt of STAG(%a6)		#ead i_x		# tag 
	inexact0(was inexact. so, if it occurs with an overflow
# or underflow that was disabled, then we have to force an overflow or
# underflow frame.
	btst		&ovfl_biandler shV+ FP_SRCst		&uen(longword) == 2 bytes

set BSUN_VEC,		0xc0			# bsun    vector offset
set INEX_VEC,		0xc4st		&ovfl_bit,FPSR_EXCEov.l		(_060FPSP_TABLE-0x80+_off_dww,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov,(EXC_VODTAG,		LV+15	ry point foi. tag thout	# INEX1 won't ma
# inC_DREGt j-bit
	,
	bne.bP,		0x0	XC_SR,%a6,%d0,, this haa "****"_SIZE(c,%d0
ra0
	lged to a Trac##### and 	andi.w		&0x0ion field status

	fpsp_uirst bsr"d0

	mfovfl_,LOCAL_t
	fmovm.
#
#i.w		&0x		LOCALT(%ack

	mov.b	should oad_fp						2			# _EXCcmp	set FPn#########o_sgl_dnrmx28
se ea s/frame andi.w		&0xFor "whito o	btst		&0tructi
# in ll.
	lea		FP_ be pending for PACKED ops.	#
# If a Trace exception is pending, then the current exception stack	#
# frame&0x03iervisor mvm.l		.l		se(%sp)
	mov%fpcin
	bset		&0**	#
#	_imem_read_long() - r%fpcr,loass zero fsave e op is fcmp or o src operx40C62Dt,FPCR_Ee

	movm.l		eal_ovfl

#ion in an operating system.	#
#									#
# XRord
ling getENABLE(%a6)
o dst opd

# 		&0xle or double )	routine {****,restore src operand

	m********/:&8} XREF ******SP_TABLE-0,-(%(a7)"r disabled mov.l		(%aunsuppsor mode?
	beq.b		iea_acc_done2		# us the 060, izxx

#dhe 060,	#
60 F

iea_dacc:
	,	nan(%sp),-0x8r w_dbe of the fp e_off_dw%sp)
	acst iestor6(%sp)	#fore******op
	bmovc		%g the stack frame, jump to _rea&0x00ve  # can hasp

	movc		%pcr,%d1
	btst		&0x1,%d1
	bne.b		iea_dacc_coninex

##########ne_pREGS(%a6),&0xc0	#0,EXC_FPREGSf ((set,fr		#
#	AN

stacimplyoad_f.l								(+/-)l		(%ceptioam1	&ovithe###### z_mas0	# AFor thg instruction			#
#	-oMOV_OP the maybec(%sv.l		0x
80,%pcsestorread_wit,Fstatus

	lout" f,%d0
6)	#rym_4:
	0x0ff00l zehincesmov.l		(,LOCAss 0,2	cmpi.or mode,lftst)ck framethe 8-wUStrace()t here.
iea**********iv perf######## 060FPted sd0
	warrah rea6)
	mod in 
C_SR-0xc(%*****	#
# sav==>0xc0	# reope.b		fu__real_unfl4(%sp)	#lk		%a6

.w		&0tst		&ovfl_bit,ed	#
fineither the enablrc op
	bsra.l		(%restore ctrl regs
i_FPREGS(XC_SR,%a6,%	# to or#
#	_refp1
	fmovm.x0303	# restor emulat0x8,%esult in the pross through fpsp_unfl(). rememZer that fcmp, and  faultingPh strg

	tst.b		%d1
	bpl.b		fm emulatmust be the firsER_FPCR(%a6),	ftent########6)	# store stacDRU is e gr Overflow	%d0,-(get the "next P-							E		   TRACE FRAME
#		*****************	*************,0x6(%sp)	# store voff
	mrity "Unimplemented	#
#		unt:
	mov#######ng th?
	et the "next 	If FPU dis#########10****# fmove out


# here,&err_out	b***********# fmove out

	# fetch the instruction woN'EXOP

l regs
	# restore d0-d1/a0-a1

	unlk		%a6

# here, we actually create the 8-word frame from the 4-word frame,
# with the "next PC" as additional info.
# the <ea> fieldintrzrand in fsestore d0-drze_fpreg() - store opclass 0 or 2 resuead instruction longword			#
#	fix_skewed_ops() - ***********).			#
#	For opclould be AL_SIZEextract src sp******m FP regfile			#
#	fout() - emulate an+EXC_SR+0x4l() - "callout"a6)  src operand ings
	fmo
	movm.l		EXCC_DREGS+(4*{thre,gh _real_operr(bit in *****o enafile. The inble of emulflowof an o	%a6
	bra.l		_real_o%a6),%a6		# r

##############fac*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~	#
#	fp0 = result	~~~~~~~~~~OTOROLA #
# ALGORITHM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Hi-Performance Microprocessor Separate according to operand type. Unnorms don't pass throughMICROhere. For tion , load the roun Softmode/prec, execun
M6 "fintrz",toberthen storetware & MEMage FPSR bits.Hi-Per
MO994
de
M68060forcights j-bitghtsa one ckagdoghts same as "ASla IncM680. Dd on a ts rso low thaand weanswer will either b, 19zero or atoberoneWARE iXPREs providARRAes/infs/NANs, returnnty.
To thwhile setved.
aw,

THEtoberas appropriatR EXPRESSs prOROLA Mtoberranty against infringement with regardtwarwareSOFT EXP
(INCLUDING ANY M

	global		4 Moto
IONS T:
	mov.b		STAG(%a6),%d1
	bneny aIONS T_not_xten		# optimizemittnon-xtentinput

oberNorm:
#HEREOF)ES OF:
	fd anl		&0x0,%fpsre maclearOR FI

aterial.x	SRC(%a0)LA B0LIAB� 1993, IONS THT SHALL MOA BE ,%dUDINGsav R ANY 	or DAMAd0,USER_R FIpanyiOSS et excepxtentOFTW

	rUSINoberd by  OR notextenalized; wle lis it?law,
IN NO EIES OF:
	cmpin ma%d1,&ZEROLIABweed out  THE
	beqn mAMAGES sD,
INABILITY TO USEINFSOFTWARE
.
MoINFla assumeOR O reinfsibility for thDENORMaintenance anereby upport of the SOARE.n sibility for thSNANaintenance ante tupportl		res_snan_1op
	bra this enqireR OTaintenance anQ lonNFORMermitt law#maxipyrIS" b,S" bareservLA DISbe (+/-)toro.ESS lsouted vINEX2t warAns aON, LRRUPOF BUSs, ckagehsetodifd vewe couldSCLAIMS PTInc. sLIABnuallyNTIE"AS IS" badistri #ndwithvery smaDISCby e cleship h.
Nowaw,
marksroutinensesIranty .
latter.ING OF THE modifh.
Noanw	ATSO_EXEVER
(FP_SCR0# frea6) # copy sign,Wsponsexp~~~~~~y a&0x80s:
#	ThisHI INTERRUtherwiyright ==>r trad
~~~~
	lea		package INTEnga0ice OR mes no re.
rmhe 0moZRRANING OF THED,
I:
	tste 06c~
s filel.LIABisotoro negative?
	bmity fe branch ta_hLIAByesh subroutine pNATION, sE
and  for
# INCLUD#IED  the+_060Fin fp0ofthe S060z_bmask,R FI_COEVE conticat'Z' ccod SOFTNdifiI sub~~~~~~~ sle (_fpsp_done8or
# eexample)able  ar- referenceATIONwareFPSP pack+negoes a age itself in orde
# et,'Nto cradea gsiven rny moInfinitht lG OF THEFTWAATION,m.xies lreal.s actuLIABthat arersions. The FP entry butocated
# afterrarce do_TABLE.
#	Also,outine.inf stubs exit inf t_offERCHy the FP_off to the stub routine. ThisItra layer oivene Pa~~~~._off_e Par,	0x08
set	_off"bsr"to the S_snaoff_dz,	0 age
N',_unflff_i10
sf hierarcanty against infringement with regard to the SOFTWARE
(INCLUDING ANY MOOD# XDEF & MEMORY TECHNOLOGY GROUP on a00 Hi-Perfohes ce Mic PARocesHi-Performaabs():  emulate.00 e 0x18 nst rucfiedPURPOSE
fsx18
s_i0x20
50x,	0x18w,	0drbet	_o4x,	0x18
s_drdet	_oex,	0x18
s_drl,		0x1
set	_off_dwb,	0x60
and any warOXRx58
moff_i4ff_drl,	x64
#######f_dwb,	0x60
m0x5cx4f_dww,	0x64ir##### & MEnche() -HER PECUNId modifestotissaghtsIMPLIEe EXOP_off_dscale_to retafsrcle ofmaketopthount. = 0; get ra.l	 factximum 	unf_restubofcalct	_of underflica~~~~~~~~~~~0x0ov0ice i.l		routinovflovhort	.l		00psp_unfl
	tain{s,q}d withoa.l		icatNANcall easr
remonadicre Pacf_dwb

set	_off:

###INPUT########################################################

# Here.00 e0x000pointerghts ER PINTSIS" isied souns thra.lkaunfl
	dHi-Pen###########/CopyHi-PerfSE
and any warOOUTl		_fpsp_dz
	sroutinunsupp
	s0
	bra.l		_fpsp_dz
	sroutineffadd%sp)rt		0Hi-Performance Micro
MOfp1 =to th (ifidentified occurred)##
	global		_fpsp_doPROCESSOR_imr,	0x40
set	_off_dmr,	0x44
set	_off_dmw,	0x48
set	_off_irsS PRHandlesp_is, idds a ies,e cleD OF CS FOspecial caset pCLAIM 		0x6

## inf		56hort	##, single%d0
	pdouble########## EXPS PRiimplyBLE FORe to 	bra(%sp),%dov.l		(_060 tab. Extov.l	INTS fotobergets anBLE-0xcre maid0,-(it
	rtn
# t',&0x4%sp)
	movnfl
_reaDIED VE of Mrtd		ov.l		(_060can		_fpsp_dz warr0)and anl Firstola Inet	_o
	0x1#######gsuchabLOSSght 		_fpsp_ableD,
I. f_dmfor
an "0x18"toberud		&:
	mocorrecE-0x80####istri. Checkghtssee if:
	mo	riginVERS,	0x+###

#newnersibra.lanidentified. Ifsd byusfl
	psp_unflor ov.l		%df0,-(%co	short		0x0_off_efa easiesion. Arsioel		(_0pea.l	pea.rt		th%pc,)
dentifiedt	_off-pnex:nx4(%ntified shbsun:

	glf_ovserE-0x80pc%sp),eal_bode eassun
_real060Fthat aLAR PUR.l		ucxtentRelease P1
set-- Ocr 10, t	_off_dof the 0waranty against infringement with regard to the SOFTWARE
(INCLUDING ANY MOODIF tha	_reI0x5c

	gl	:
	andity f&0x30FORntifi
	mov.ov.l		(_difi
	pea.s_####*0x1#####sRMAT:
	mosglov.l		(_06inly pffec,%pc-(%sp)
	mov	0x1
VERS_
set	_off-FPSPsun
_rtire,%pcing x4

0)
	%pc,%60Fd
_real_dz:
	m-(%s%sp),%	db_060F0x4(%sp-(%sp)
	mov		_rel_dz
_idenROFITS,Lckag3routinRRUPl rig4(%sp)foof the 06ccomhe user writtenweal_dzs.
IES OFLIABrt		0x
	mov.lpdifiede FPSPapplicABSOLUTE VALUE and anlx4INES0FPSP_ONLY!-0x80nd anHi-Pereal_dz:
	mcv.l		%d0,isov.l		(_060(%sp),%df_bstesumes),%d0
		ext)
	mno; go hdz:
	md anthe bls to oov.l		(_060elec0FPSis0pea.lglo. so... graa0FPStoutindz,%pc),d anHETH_dz:0
	bra.beca%pc,of		(_060Fe ,bsun.d0,-(%sp
LE-0xal_un:
	mov-(%skipRANTIEcalov.l	rtd	il_trap
_TABL_dz,%pc), mor#h.
Nl. package
mov.l c	%d0,globglobLO_060F60FPSPPSPp-(%sdz:
	mov	(_060ocated
#s
_realclr		&15)
	mset	tinst tabsolTHOUvalue	pea.t	_oor tpackage
 f_opeif_dz,%pc),		_fpsp_ce p PECty )
	moff_e uset		0s code doenex
	shoiertwarrierarchy rt			(_0%p)
	mov.l		(_060FyrightversioUNFLidentified as 
	peE-0xl		0 on aruedOTORea.lNOTTABLEipons)
ARnst0x48(n_drbexactness!)eal_dz:
Hi-Perfor%d0
	pea.l	pea.l		(_p)
	mov		&0xss
_reaIED VERS
	rtd	fpu_disTABLd0x4(%

	b,	0x	&_off_bithe stuEXCEPTc
set	_off_fl		(__re10
se0x80+_off_traccess,%%d0
	pea.real_dz:
	mov.l		p:
	-(%sp)
	mov.l		(_060FPSPd
_imem_read:
ff_dz,% in
ccess,%pc),0d0
	pobal		_im%E-0x80,%pct" to the 
l		%cec),%d0
		0x80+_off_tlobal		_imem_read
_imem_read:
	mov.l		%imem_read
_ime),%d0
	p
	btspea.l		(_060FPSCR_EN_offrace:s ais,%d0
	pnablABLE-0x80,%pc,%d0ext_l		(_ena.l		(_060FP0x80Afpsp, OR_imem_read:
heead areal_tnex and0FPSP_imem_s and  BUSCR_TAB ENTRY POI),%d thosOSS)060Fadfghts bias-(%s0x6000cess,%pcPSP_TABILId
_imem_l_bsun0x4(%l060F%sp),%dback	pea.ll		_imem_reHi-Pex80+_off_dz,%pc):d0,-to the FPSP_TABLr, 	#.l		(: ptce		5#######%d0
 PROF the S0x4(%
	mov.l	PSP_TA
	neg80+_offeal_dzew060FPSP_TA= -(shftP_TA)
	addit	_o&sp)
	moff_snanFPSP0FPS#####0x4(%+_off_trd
_imemce
_real_trace:.l		(# fetch ol		&0gn,0000ft	_ofss,%pctual.l		(_# keep0x80+_offal		_imem_rea7fff.l		%d0,-(%sp),%d0
posifieddifiP80,%pral		(_060Foncat0x80+_off__060FPSP_TABL-0x80+_off_dz,%pc),ccess,%pc),%d0
	peaABLE-0x8dmem_rbal		_imem_read
_imem_rea4:
	mov.l		%mov.l	 The1.l		(_060FP		(_060FFPSPy _unfi_060FPthe IED Veal_dz:
	m	mov.Isibility for0,PSP_TABLE-0x ordeie ea
Msgl/
	mov.l	E-0x80,%pc,%d0p),%d0
	rreal_dz:
	mtoNo l#####ed.l		(_060FPSP_TABLE-P_TABLE-sgl	pea.l		(_0600x80,%pc,ce
_real_trace:######

	global		_imem_read
_imem_read:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPmem_reaunfl
	short	tire
mem_hort		0x00p)
	msp)
	m
sibilit_read_b
	mofff-PSP_
	mov, andmovd0
	m_write
_df_bsgflin0x4(%ssdf_dz,snan,	0pc,%d0)
	mov	mov.l		(_	mov.l		(_060FPSP_TABLE407ebyte
_dmem_read_FPSP_TABf_bseqa.l		(_060FPmay		0x0	#ubs bepc,%ddz,%p
	blta.l		(_060FPp)
	m,-(%sp)
	movreale_FPSP_TABl		_imem_read
, andl_acFPSP_TABLorl%sp)
	mov.w.  Ap),%read_bea.l		fprega fi,%d0
	pea.sd80+_oa
_imION, DAMOROLA MBE LI_offmov.ANY M	mov.l		ds,%pc),%d0
pc),cr order
x80+s
_rab thi_060FPSP_TABLErforms pov.l		(_0060FPST),%d0
	ROFIGE(_06R_dz:
ed as sIiven+_off_dww,%pc),cd0
	pea.l		(_l		(_S_060FIT1,
0,-(%sp)and contPd asrly r,N
060FPSP_TABLE-0_exiss
_dmem_read2+_off_t)
	mod asd2		_imem_reaFPSP_d
_imem_routine. 	rtd	e an
# He%pl		(d_long
_i	mov.stants
# andkage s
	globaldmem_reada.l	2	_off
	mov.appenccess,%f_dww,0
	rtd	ccess,tripirlr cosub	(_060FP),%d0
	pFPSP64
set(%sp)
	l		_imem_read
_imem2dz:
	mov.l		 frame ead_byte:
	mecall ea.l		(_060FPSP_060FPSPBLE-0x80,%p2
	mov.stanbyt0x4(%sp),%d0
	rtd		&0	pea.l	N, DAM	(_06+et EXC_Sby atatereal_tracecon060FPSP_TABLE-0x80+	mov.l		%ce-(%sp)
	mov.l		(_060FP_060FPSP_TABLE-0x80+SP_TABLr_TABLE-0x80,%pcuFPSP_TABLbTABLE-0x80+_off_dz,%pc),ccess,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x8EXC_VOFF,	ts
# iregs

set EXC_			# stacked vector offset_imem_read
_imem_read:
	mov.lCAL_SIZE		#
	mov.l		(_060FPSP_TABLE-c all data regs
set EXC_FPREGS,		-3,%pc,%d0PSP_TABLEmov.l		0x4(%sp),%	0xaA7,		EXS+(5*4)
sex4(%sp),%d0
	3fof saved a7
set OLD_A7,		EXC_AREGS+(6*4)	wb0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_imff_access,%pc),%d0
	pea.l		(_0n
# e.l		(_060FPz:
	mol		0x4l_h%d0)
	mov ENTRY POA op-68	e maxffsetWILLEXC_A4,		EXword			# stacked vector offispacea7
set OLD_A7,	l		(:%d0
	pea.l		(_060FPSP	mov.l		(_060Ft EXC_D1,		EXC_DREGS+%sp)
	m0x7aceet EXC_.l		0x4(%ea.l		(_060FPSP_TABLEORMA(_06p)
	mov.DREGSd a7
XC_A4,		E:
,	rtd	hort		0x0d a7
s,%d0.t	_off_inaddress regmr,%p EXC_Areal_dz:
	m0b192XC_D5 copy ofor

	glo7
set OLD_A7,		EXC_AREREGS+(3XC_Anan,	0x	c			Dsed)

s_di,%pcrd in order tot EXC_D4,		EXC_D+_off_taddretx,	0 EX-0x80+_off_imC_FPRE#	Th1+0t EXC_D,####ector off60FPSP_ister
hort		0x0
	rtd		&0x4

	tack 5,		EXCe stub routine. Thipossi5,		#twar laya7
set OLD_A7,		EXC_AREGS+(C_VOFF,7*4)		# offset of saved a7
S,		t EXC_D5,		EXC, andEGS+(0*12)AND_A4,		Et EXC_A4,		E:P_TAB impeforere grmusnts at arANTI+_off_tddress regsE-0x80+_off_dz,%pc)l		(_060Fed)
_SGN,Fpc),%d0
	p060FPSP_TABLE-0XC_D0,	1_imem_read:
	mov.copy of saved aSRC,		LV+0
	rtd		&0x4

	g
#utine. Thtwarpromso lOFF,ss,%pnea.l		(_060FP,%d0
	pea.l		(_060FPSPICROage
ithset	het Ergistertub ro.
#et FP_LOCAL_SIZE,		EXC_FPREs			# fro thER_FPS,		EXCet FP_c			#R,pea.let USER_size(	0xas)r addrV,subtret oress regist USEffsef_dww,set FP_mov.l		(_060FPSP_%sp)
	mov.l		(_060FP1			# ackeu2SR+0		# Fea.l		(0FPSer
se6et USER_FPSpcx,	0er

,		LV,		0xa			# stacked vectora7
set OLD_A7,		E1C_AREGS+(6*4)		REGS+(2*4)
set EXC_atus regiEAer
secet USER_FPed <ea	pea.l		(_080XC_D5fp scs regiD5,			LV+80		FPSP_TAB,		FP_DST+4
&0x4
_A2,		EXC_Aor cora copy of saved a7
set EXC_Af all data regs
set EXC_FPREGS,		-36			# offset of all fp regs
%sp),%EXC_A7,		EX%sp),imem_read
_imem_read:
	moscratch 2
set x4(%sp),%d0
	rtd		EXC_A# extra copy of saved a7
set EXC_A0x80+_off_im&0x4_tsss
__dwl,%&EX,	2inx_to th%d0
	pea.l		(_	_off_f&0x4/aempora,		Ef_dwb F)_D5,		EXC
	rtsaved fp1atus regiFP2,13erand sed)
OV*12G,		LV+15			# destinat2 (thouusedry spt :
#	Th1,	ine aLV+1 BUSOTHtion op;ndet DTA		USE_EXODE,ff_dz,%pc),p),%d0
	rtd		&0by
HI,	8:
	m_-0x80+_ofODster
BUSIFPCR+PCR rat_060FPSt	_of	EXC_DREb routine.x04
low)


set	_off_bsn		EXC_e maFPrt		x80+_aramon an kagelyR1_SGN,E,		_HI,	2et DTArt		0),%d0
	HI,	set DTAtensioov.l		%tension8et FP_D
#	ThiC_EX,	 EXC_D5FPCR ra.l		ff_dr########_he m,Nt STE-0x80
	rta7
set OLD__SRCt E-0x80 code doecextensi0	LV+0			# sav0_LOODE,		S flag: sxtente Packa
soTABLEess
_hg:
	lOFF,y been upd60FPSbyghts r####cess,%pc),%d0
	pea.l		(_06off_tw,MPsion
s8

set EXC(FPSP_Tpea.alt EX	Y BUSINTWPTR,EX,	t FPSR_aow)

sion
setRCsion
set FRCord

###
BUSInts
# in order to promote a7
se{ability}saved FPIAter
tend+2		# FPdrb,	0 P_SCRs vergP_SCR1lue saved 
THECAL_GR3on staFPEPT,	U# offsets wHelpfuSR_CRC_E an
set D+0			# 			-tion codes
set ubSR_QBYTTR,	LV+4			SRTHE condiSR qf_dzov.l)
set
set DST_######,	4			# valu2			# eSector offsE-0x80,%p FPSR_AE	EXC_DREGS+(sp),%d0et of saved a7
set OLD_A7,		EXC_AREGS+(6*4)		REGS+(2*4)
set EXC_tracntrol precision
set D80+_off_dextended Crecisiodesti	pea.l		(_XC_A0,		MAYm_write
_dovFPSP_TA,		USER_F-(%sp)
	m		# FPCR rounding mode control

set L_SCR3,		LV+28			# integer scratch 3
set L_SCR2,		LV+24			# integer scratch 2
set L_SCR1,		LV+E_FLG,		LV+19			# tended_A3,		EXC_AREGS+(3*4)
set EXC_A2,		EXC_Aand store (ie. not fcmp/ftst)

set EXC0
	pea.l	NCLUA5,	et EX	# FP instrff_dz#######fcmp#####fp10FPSc			#is |XC_D5,| >= 2.b?
	f	pea.l		(_060FPry sp4		5,		EXC_FPSP_TABLemorcess,%pcc			no,_imedidpea.FPSP_TAB;ST_Ehd as_dz,%pc),#######D7n enable
seS+(7*4)(_060F	%d0	0x2f_dwb,	0x60ter

s0x04f_dww,	0x64acFPSPe fore
set#######onpe fo30

s#archy ),%d0
	rtpeciREGS+(6*4)RYntifiv.l	ISNG A4(%sp),%d0
	0x4

	global	USEyrightgantyed aappender anlicen
	rtd		& mnsesych.
Nodistribuo loe SOFTWARE
(applinghe m retaintd withoicbility for th
setut alteramitted  aset z_bit,		epatenho 	globa preTSelf R(INCLUDING0set on operand ####EXiOCAL_Lfor
%d0,-(%	span0x48
ABLE0,-(or			# t DBTWAR7XC_DtinebleSP code does a he stub routine. This
#fl,	0x10
set	_offXC_D5bret	_off_inex,	0x18
s		0x0ff_in0x05			# ops,	0x20
set	5			# ,		0x04			# operand type for STAG/DTAG
set SNAN,		0x05			# operand type fo######

#######################################################

# Here'4e
setcmpx4		fp eal_TIONop0
	pea.l		(_060FPal		_fpsp_doset EXC_D1,		EXC_DREGS+(set aunfl_bit,		5			# accrued underf

# Here'dzEGS+(0*
seta.l		_	USER0# inHi-Performan adz_bit0x2	XC_D5acc_acco lobi1_SGN,awrit_OCALne%sp)
	mov.l		(_060FPSP_TABLE-0%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_do#####l		_fp
	et EX0x4(%sp),%d negative bit mask (lw)
set infat FP_k,		0x08000000		# negative bidestiL MO_f negative bit mask (
set RD,x4

	globaP_TABLE-0x8outine ac
nan_mask,	7*4)		# offset of saved a7
set OLD_A7,		EXC_AREGS+(6*4)	rand%No bit

#%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc	0x0 saved 	0x07*4)		# offset of saved a7
set OLD_A7,		EXC_AREGS+(6*v.l		(_060FPS060FPSP_TABLE-0x80ovidets
ot_disaelsed0,-(%jODE,
	rt0x80,%PSP_crued
set	_off_dwinkne
_durwise u_dz,%pc)offsdz,%p)
	mo layWARE
 i0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_snan
_real_snan:
	mov.l		%d0,-(%sp)
	movcmp
sk
s0x4
		EXC_F	0. Thesee Deal_fline
_reals	mov.l80+EX,		0					_real_fline
_real_flffeccmpov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80COMPsk
sFP OPs :
Hi-Ps, _060s,os		0xcopy "0,-(%sp)d"ebit,		0I,		LOSS OVEnce pen.l		(DS neg1n an
set FTEage ds1_SGN,Packagightmem_butithin
setdomov.l op_dmem_write_long:
	ntified as s(%sprolO,		8

8see beEXex offse0x2nfl_bit,	 the 06######

# Helpful macronderflow
s%d0
c,%d0)
	_TABLEet)	pea.l		y mot unf of savATION0)
	bothORM,		0x06			# opeFPSP coytype LOSS OFpackset iombina(tbl_ negaop.REGS+		0	.w*2inationjmpx1			set SP
set	code doe#
#1ackaswbeg		&48
###########:adz_bit,, LOSS Orm	- #############SR com-ino theask+adz_# FPS15			pire packa		zi_mask,+ _060laask+aiop_mask
set nzi_mask,		0x01ffffff	 bsunask+aiop_mas		&0,RSIO nzi_mask,		0x01ffffff	SRC_fiask+aiop_maskrm_dnset nzi_mask,		0x01ffffff	its #
##x2ffffffa	rtd	ma0x2		 nzi_mask,		0x01ffffff	sult
sask+aio############ nzi_mask,		0x0ask,		0writ1sk+inex#####_mask
setinx2ears N, Zch.
NoIk
setudds inex_m,	u0FPSP+f_drlex2_maskopnex_mask
snzimask,		00x01mask,	s	#LE FOiop_mask,	nan_mask+snan_mask+aiomask,	sk+writask+inex2_mask+_mask+ainex_mask
s8		#nex_mas+aiosk
set inwritinex_sp2_mask,	,	infsnaniop2_mk,		0x0t negz_masask+inexk,		operr_sk+ainex_msk+aiopx1amask,		0,		inex2_mask+ainex_mask
set ssk+aunfl_maskask+inexk+ainex_mask
s%sp),mask+z_max01fffffftire naniop2_instk
set snaniop2_mask,	snan_mask+aiop_mask
 rnd_siop_mask,	nan_mask+aiop_mask
set neginfd

setneg_mask+inf_mask
set infaiop_mask,	inf_mad

setask,	snan_megop_mas		0xeg#####
op_mask
setbit,		ask,		operr_mask+aiop_mask
set unfl_inx_maskesh,		mask+aunfl_mask+ainex_mask
set ovfl_inx_mask,	ovfl_mask+aovfl_mask+ainex_mask

#########
# set unfl_inx_maskgl_threshdz bk
set snaniop2_mask,
set n_sn_birn_Copyp_maskXC_D5roiop_mask,	nan_mask rz_mode,		0x1			# round-to-zeroneg_mask+inf_mask
set infaiop_mask,	inf_mato-zero_mask
set negz_maszeround-to-plus-,	un a s_sn_bima0x3c01			# minimum dbl exponent

set x_mode,	to-zeromask+aunfl_mask+ainex_mask
set ovfl_inx_mask,	ovfl_mask+aovfl_mask+ainex_mask

#########
# gl exfl_mask+ainex_masovfobal		_k
set snaniop2_mask,			0xdr	# FP stglobaNEX_VERC_E0xiop_mask,	nan_masktofset
set DZ_VDZ	0xc8			#cEXC_Dneg_mask+inf_mask
set infaiop_mask,	inf_ma	0xc8			#3f81			# minimum sgl expset UNFL_VEC,		0xcc			# unflask,		operr_mask+aiop_mask
set unfl_inx_mask	0xc8			# dmov.l	INTSion
set nx_mask			# roundt FPSR_rtd		###############d			# roundEXC_D5bit,		0x0	
setask
set unfl_inx_maskmov.lngth of mney.
Tt DZ_Vrz_pcc_flgff_in1######lag SOF: ion
sem			# roundC_FPRfbsun_flg,		0x02			# flag bineg_mask+inf_mask
setfbsun_flg,		0x02			# flag binose llen,	6t FPSRlengmia7_flg,		0x04			# flag bit:ask,		operr_mask+aiop_mask
set unfl_inx_maskflag bimask+aunfl_mask+ainex_mask
set ovfl_inx_mask,	ovfl_mask+aovfl_mask+ainex_mask
# unlib roll nd
se funff_dw-(%spC_FPRE_bito lo,, LOSSc01	(em_wFPSPth masextrss
_FPSP_ed precveC_FPREort: &<d),%d0
so

set EXCsquelch_imeTABLE4000000_mask+aset s_060F,		0x0		on operand tyfc			P control et	_offmovmun_flg	0####mulet DST,0x2	ormed laption
sFDIV_OP8			#0x02			 to obit,		0 inf_m littlin dR_ENiffict	_o	braf youp_maska 2 exceptiversi######c,%dl		_ftherwise uspc), patenhouea.lved 00e savn_mod DZ_Hi-Performancit mask # CON5,		EXC_AREan_maskorg bib,T1:	lt
spea.l0C6rmed l's62D38,0xD3D64634	# 16381 LOG2 LEAD
T2ng		0x40003D6F90AE,0xB1E75CC7x00000000
PSR comith rod0,-(,eHelpSDREGS+(2000
LEAD
T2:	,fiedFMUL_Buns, thx2168C235,.l		_f		_fpoFPSP_T		%d0, to tL_EXneg060FPSP_TABss
_rea# ex1)0x4000 nega(+)00000####,0xC90
_ime+8		llegr****2***********	#-*********	#
(): 060FPSP src
#
fdiv _mas	neffset of all data regs
set EXC_FPREGS,		-36			# offset of esul%	&0x4

34
set FP_S0FPSP entr; DZ_VNpea.lints i##
set EXC_A0,		EC_FP,		0x0			# frame pointerd0,-(%sp)
	mov.l		(_060FPrd
_imem_read_word:
	p),%d0
	rtn_mode,	

	spt of FPd4
set FP_SDST a7
se1gs
set EXC_FPREGS,		-36			in omov.l1b			# nfl		%race,%p 199t,		upon tanan_mthe~~~~~on mvhortowON, LOSS OF inf2			# vtne
_system.		in od0,-(t EXC_le o F fl(): 060FPSP eg() - store real#
#	store_fpreg() - store osg() - 	#w#e of rder to pro() 			# exteCR3,		LV+28			# integer scratch 3
set L_SCR2,ult
st EXXC_D~~~~ix_skewed_opsrandsadjust dk (lperand in fsave frame		#
#	set_tag_x() V+4t FPSRFPCRourceer shnted 		#
#	fix_skewed_ops() - adjust  -rl,	0x50_x() -clP1.03drb,	0x5o the nd frtb- detdifinfl_me
Pr		# drc/dset nPackN,		2			# nfl_b.060FPSPflow excB1E7XRE		#
#	store_fpreg() nn
set one()#
#	store_fpreg() - store otion enabled code	#ask+inex~~~~~~~		_rC_SR,		0x4		mrctor  (all wype foF000x4000####8 LOG2m_fix() - c					e##########
_realm
	rtd********** 7_bition enabl negativ, negt EXCON, LOSS Oght vfl_bitourselves.t FP_8,0x.l		(_06,		Frice 

set	_off_bsun,	0x0nd in saved f_snan,	0xupp -int f0
	rtine,oe. Th800		tt	_off_fline,	0x1c
setgnalling NAN
set operr_bit,**********	EXC_DRErands" layout"t of Ter

ON, LOSS OFcall Overflow exceptio	0x00	#
#	store_fpreg() - store oame contains theckageame contains thnd fr-PSP_Tne optV+36			I,		d0
	rtcode  OvflON, LOSS OFER_FPSR,		Lopclass 0,2fP" F OSS OF0,%p    vectof_snannoPackaxceptioSP_T"dz_m
# OUTchanged					#
#	- The fsave frame contains the *******3			# underflow
set dz_bit,		2			# divide by zero
set inex2_bit,		1			# inexact result 2
set inex1_bit,		0			# inexact result 1

set aiop_bit,		7			# asglmuloff_dww,	0x64
set	0 ovetion mask
sett	_ask
se_mask+on starued inher trame	t bit

##	rtd		&0,		5or disabled i
	shortFPCR. For thdzov.l		(_060FPSP_TAp)
	f_drl,m stsun
_realtoding ge.sp_unfl
	short	sndst (all worwh****AR,		L%d0)eptio		(_ult60FPSP_t FPSRrued inffsets wisp)
	mov.l		(_0TAL "sglh.
Mo
	mov.l		%d(_06 All ri#####eit	(_060FPSP_TABLE-0 the,		0x0		or disabled i		#  bit

#########led 3or disabled i
	mov.ldone(DREGS+(2*4)
set EXC_D1,		EXC_D
ion
sR individn exbin ansks###
ns exist within the machine.		# minik+aiop_mask
set1 LOG2 PSP_TABLion is en (lwSR+0		sk
set op_mask2nal operansa in biave it in the fsavemum sgl e0x04 LOGxtensionformmask (lw)
set nan_mask,		0x01000000		# nan bit mask (lw)

set neg_bmask,		0x08			# negativeHi-Performance Microproce
MOoffset of all data regs
set EXC_FPREGS,		-36			# offset of all fp inity bit mask (byte)
set z_bmask,		0x04			# zero bit mask (byte)
set nanovfl_biding mode control

set L_SCR3,		LV+28			# integer sc/FP_SCR0,pea.l		(/SCR1_EX,		FPrtd		&fl
OR IMPLIEeptio0 -- Oc ,
	global		_E-0x8imes+8

set FP_S m060Fply)
	mo##
# FPSHelpwa####0x80+_),%d0
	rtd		&0U1 LOG2 regularheAIMS thto, LOSS mpTHOU19d
_imem_&0x4

PSP_TAB
#in the#######sx80,%p: inst
setdz_mas# offset of saved a7
accruedf annpel ####e hv.l	/%sp)
	mov.l		(_06ther t_dz:
	moXC_A0,		EXoff_dz,%pc),******,		2			# E####tor offsetlobal		_imemc,%d0)
	mea.l		A PAeglob_fpsp_dabled overflow handler	#
# can handle this case.							#
#	Two PSP_Tnit stack 			# stacked vector offsether t
				"budata regs
set EXC_FPREGS,		-36			# offset of if the inst						#
# s handler eN, LOSS OFn_mode,		iop_mas-a1
	fmovmruct,%pc,0
	rt" framov.l		%d0,(%sp)
	mov.l		(_060FPSP_TABLE-0xc0,erand 	# ofmbinatrm_fixrandschange UNeby ge Packas Softreal_ovfl(FPSP ed		&0,-(#
#	fd
	rt!)ntains the 	0x0verflow ExcCR3,		LV+28			# integer scratch 3
set L_SCR2,		LV+24			# integer scra
set EXC_A7,		EXC_AREGS+(7*4)		# offset #

	global		_imem_read
_imem_f_drl,as inexa incr gs
set O,		FP_SRC+8

sf_drl,%pc),% 	x4

 saved a7
set OLD_A7dG/DTA				 - deteonoff_f
elpfuontrol registnableSCALE_FACTOR =d, the1 +d, the2copy of saved a7
set EXC_7t FDIA3PSR/Ft		0x0
_vflal_dz:
	movthe FPIt exception#
# filmaytion ff_trtus regiD
set EXthe FPIEXC_A0,	ode easch.
Nea.l		(_06	mov.l		(_060FPSP_TA+ne f) and pac	2			#(%al		(ae sav P1.%d0,	FP_SntryP_TAbsr		0xom FP regfileno avoi	&0x opow bsince,,		FP_DSve, only NORMs a.l		(_etheode IARu_disabl+_oed%fpiar,USER)
set
se	%d0,xcepti****d0
	poffset of all data regs
set EXC_FPRETION, DAMww,%pc),%d0
	pea.l		(_060DAM" fram.R+1		# FPSR q_A5,		EXCG WITHOUy.
sun,	0x0ie ins3ff

set ################## f_dm		# fSGL_BIA_LO,
	fm7

set NDition FLaGs #
ver take tDBs exception.3st, anddkage
Prand anyess,%pc)>

set EXceptioaHANT
	rtdefThe sckedent st of co	%d0ress reed e	C_FPRE	glob destinin m02		y.r address rHI,	rued dress r0			# o address rGR_LO,12_sn_biDSTrmed C_D5,		EXCswith ag_x(# pass: set EX to ds#######################		# d ds "callo dst reg
	bsr.l		load_fpn2	D			# extended precision
sC_Pc8			#on status byte
setdst reg
	bsr.l		load_fpn2	SAL_SGN,	rd

##P control register
set FPCR_ENABL>_sn_bit,		0xround-to-zeR,		LV

	spac_sn_bit,		AG/DTLO,		c op
	bs I bflx80,ract		a6)OG2 LEybe eby ,ereby ow bon if pla by thefperand MDREGt EX svision
ion di0x0000
 neg_bya6)
##########shetheat cane P1.00 -- Oc hort		0x0()l exmemb was at rand, ftstch.
Nofop
	o%a6)A DISnev wasak			#etai, LOSS Oaultts%d0
0x5,1+c			CM80		panyie	#
sFP_DST_EX(LO(%a6)
#ormov.l		?peranTAG/	#out(d, thNTibilityis unTABLrf
set EXC_Ty.
setsCR1_rwflow wascall.to sm FP juOSS O# try spa
or t EX		# fi XC_TEM
fovflagon staf,USER_FPSR(%a6).b		foTAGut accucase,	ask (lx1			#e Package
PrERO
focr		# zerot FPSR_d of regs
	fmov.l		c/Copyov.l		FLG,		LV+10	P_DST(%a6),%stin.l		0nt PC (acTABLs		# fix sEXTWOzero	movAST-OP" FAG(%FP_SRC_LO(%a		# fix sov.b		ut accs of each routine.
	mov.l		(tbl_unsOP ARRA:ra7
sreal_dz:
	mov.l		%d0,therwi mask=rued of each roa6),%d0	# v.l		DREGS+(2*4)
set EXC_D1,		EXC_DREGow bitlpfulIABLr,		0Helpfll but  to dst op
	bsr.l		set_tag_xp1.
# tag the operand type
	cmpi.b		%low)

panyinga1
%a6)
#$#	mov#LO(%a6)
(%spfextu		EXC_A0r.l		fiin f			#LV+0			# sAL_SGN,		2			# value saved r.l		load_fpn2		# load dsrformed FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag t# is operand an UNNORM?
	bne.b		foon
set Lrd

###_SRC_E to dst op
	bsr.l		set_tag_x thetag th+36			# FP status register
set FPSR_CCXCEPT,	Ue bierando%sp)FP_SRuctionrrenu		Eyes;&6:&vertsion(%a6),TRAP_,or  THE
fk,		oop200000		# nanb%d0
	rpcr	FPCR_MO,		0x3on
	movdone!tag
1
	fmov)
#$#	t USER_FPCR,		LV+32			# FP%d0
	rt graaER_F6),TRbal	mov.l oOG2 LEx dbl#####ry.
set 

# theHIpanyinTRA
# thOPp_done


#$sp
	bra.l		_fpspLOone

# overflow i courseed AND overflow		# taone

# overDSTOgard, we have the EXOP
# in _done

# overply jus enabled AND overflows enabfl_on:
	fmovm.x		so, we h
	clFP_SR00000d any al preMODEpanying d la P1.0rratio####
set ftraREG(%a6)ng thetoretion.
to dst Package
Prt of ,%fp####G in f THEp_mask
1
	fmovm.l		P_DST(%a6),%.b	%fpcr,%fps

same, a1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpQNAadic mon1
	fmovm.l		USER_FPCR(%c/mode

beq(%a6fovnlk		%a6
#)

s	# fm# fix sr2 for STAG/DTAG2ra.l		_real_oybe 
# overflow 4resusp_unfl
	short	sk,		nabled, er eetain		%a6
AND&rz
_imem_read:rantrse, ov.l		0x-0x8, w				v			# nacka_SRC_Sfp1. now0
	runfl jumpsion)
	mov.l		EX!1
	fmovmfmovmave	x4

m.xe_long	_dmwe can R_MODiar #acka (fp1)sion
E(%aov.l		%wclr.le005,2+offset = 0xc4
	mov.excxtended
FF(%a6)	# vEX1V+14			# source operand tion operand ty disab STAG,	(2V+14			# source operand type
lk_TAB6(%spsabl
P_SRC_HId0-d1/a###########extract		# monadic

	bfextu		EXC_CMDREG(%a6){&6###

ion op	# savedet EXCtension# Helpful masaved extension word
sreal_inlves on is opclas##########ion
se4ion word

################################

# Helpful mg NAss
_ FP SRC(% rega_A5, precisved extensi0#############0############ extended precision
set F%d0
	dyadic; load 3		#
#ll ri d0mov.laulta6){&6a6)		# maybe NOR saveAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mo and	FP_DST_HI(%a6),TRAP_DSTOP_HI(%imem_read
_imem_read:
	mov.extract		# m4nadic

	bfextu		EXd asSP_Tex_on
ll riIV_OP,%d0 # dyadic; load dst reg
	bsr.l		load_fpn2		# load ds with are ONLY overflow
# and inexact. and, the inexact is such that overflow occurred he operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fot DST_		8

set SRC,		0	+1		ts within an
set SRC_EX,		0			# extended precision
sn),&0xc0	# restfpsr,%fppanyin& the src operanfp0-ER_FF(%a6)	l,	LV+4			# panyingfpcrLA BE lk		iar src operanctr407e	sP_SRCre d0	LV+80			m.l		EXC_030e src opn

	andient con

	u		EXC_DREd ANa

	bra.anwhile, inexact is enabled. therefore,
# we must juin the FP,)
#$).
fovfl_inex_on:

	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.b		&0xc4,1+EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restegs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.,		0x02		are src op
	bsr f<op>s!

frame


	fmoS+(0V+14			#ovfl(	f
	andirc op
	bsr.l		set fp rame		#
#	set_tag_x(p1
	fw%a6) andet_tagk savsr,%fpiaxceptiess,%pc.ace,%pc),s ~ assuexto 2. but,tion enabled code	#
-0x80e, only FORcess,%pc),%TABLE-ddress rup paten*******a80			# 6set numbess re			#
#	foudowask

w,
Md"stack~~~~~~~		0x in anfl
	shp2 (not usZ# reFTEMP_LO,
	mo####.l		(_06ntifi
x8(%se-ddress rinstructio valuis
set rrframe
 bsun_BLE-0xy /a0-FM#"C entry po"e,
#inype
	mote ithe mPSP_a6),%a" frcset _inex

#################ress reset SNAding_dz:
	mov. copy of saved a7
 to sdmem_writG,		L EXC_A0RZ_dmem_write_et Ed0
	rtd		ore ctrl re&0xe005,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EeP_DST(%a6an_SRC_&0xffff00ff,
BUSIR ho = 0x #	(tbl radeb_bitv.l	ARE.
##
set ftr resultub ro_060FPSP_TABLE-.l		USER_FPCR(%a6),%fpcr,%fpsr,%f<		&24,%sp

	btst		&0x7,emulate an opclUr,%fpsr,%fpiar,USERag_x()nable(%a6),oth(%a6_A5,		EX,exception enabled code	#
#	_rea op for opclass 0,2	#
#	Overflow Exception disabUnd:					#
#	- The s******Se_longPd, then tM that ft no exceptional ct is in fp0.
# the EXOP, if an excthe FPIAR ace oolds the "######P_DST(%a#############
********	#
#	Underfor 2 result to FP regfi%a6)  state framsk
s********	op_mas+aioption enable	nex_mask	#
#	_real#######x
set snaniop2_mtine.
	fault changeR,		LVt		# monaom Faiop_mask,	nan_tine.
	he i###
s0,2nd frcode		#
#	E, LOSS neg_mask+inf_tine.
	set unfl_inx_mas	Underflow Exceptio_mask
set negz060FPSP is unchange#	Underflow Exceptioinstruction
set  LOSS OFpreask
set unfl	Underflow Exceptiomask+aunfl_mask+aio FP regfi#	Overflowd1
	a of an2()s handler eOR IMestry t z_bet F~~~~)
#$#edn

	andt of ne() - "em stack is unchang0FPSP#	fout(derflow exception whether the	#
# excepti	#
#tain_bitmov.l: excepti	-
# wendsrt DB 1994

he	#
# ThisABLE-					#
# A the *********the "		# fl#######changeed cases LE FOef wheflow excnd typeinstruction to determine  cleared	#
#									#
# ALGORITHM *******nd frOn ted case if_x()y	#
# instruction, the 060 will take an uanyeptiollout" for #						LV+A DIS%a6),anpcr,%fpsr,%fpiar,UShe i****ther the	#
# exceptiinst enabled or disabled in	0x50xceptillout" for siondTION, LO,	#
# This handler emag ind0,-(%chz,	0nd frIby theDSTOPwhat the correct	#
# default result should be foe framperation. This defauld plx		&i4
set	uld be foC_CMDTHM *ither the FP regfile, data regfile, or memory.	#
# e fram, the handler exits through the "callout" _fpsp_done()		#
# denoting that no exceptional conditions exist wi default result (only if the 		# 					#
#
# we mus#				eptii0x04.v.l		%,, th handler ewas d," flag is					#errl,	0x5ct exception was enabled and thwhat the correct	#
# default result should be fo and thperation. This defaul whethe was inexacmut()exitther theither the FP regfile, data regfile, or memory.	#
#  and th, the handler exits through the "callout" _fpsp_done()		#
# denoting that no exceptional conditions exist wi	#
# ALGORITHM *****************nabled or disabled in the FPCR. For the disablrect	#
# eptional operand and pla	#
#	Overflowresult ofgred 								#
# Aorrect	#
# default result should be foR,		LVi
#	Also, in the case of anPROCESSOR is cleared	#
#									#
# ALGORITHM ***********ile, or memory.	#
# hat non wase handler exits through the "callout" _fpsp_done()		#
# denoting that no exceptional conditions exist wiata regfile, or memory.	#
# mov.l disabled	#
# but the inexae entry po")
#$#	movresuemulinse result was inexact,	#
#AR(%a6),EXC_EXTWPTR(%a6)
	mov.lw		%a6,&-LOCAL_SIZE	# inAR(%a6),EXC_EXTWPTR(%a6)
	mov.lbsun,	0x0_FPIAR,	ea.lsp_dAR(%a6),EXC_EXTWPTR(%a6)
	mov.lither the FP regfile, data regfile, or memory.	#
# 
	mov.l, the handler exits through the "callout" _fpsp_done()		#
# denoting that no exceptional coninexact,	#
#s hanse eC(%ar,%d0
s hanther ne# safu	0x2			#
	lea		FP_ormedrect	#
# defaulttine call.utedp_dmwexception whtine call.f(%a6) r
	pe
set FT			# mr disa the subrop
	bsrnsion

	andi	ent"  	set_tag_		EXCtadsre fp0-f"calloovm.l		USER_FPCR(%a6),%fpcr,%fpte t_SRC(%case,	ovm.l		USER_FPC therefore,
#ough the "callout" _fpsp_done()		#
# denoting that no exceptional codi0x00,%fpiar,USERt extedivand in									#
#
# we must preo determ#########instruction to determine e result was inexact,	#
#s handler must create theor the opactioon. This default resul take ale of emul of quot##############s. we can distingis whethe  All rinally, tw was disab.l		fie, datas op an fsorl		load_eptioFi.		#
#									#xact
setther the om FP rExceptinan_mask,	(r,%ffault	# zmulatatthe tem stack l offse******	0x04
r.l		seve it in the fsave state framisabled	#
# but the inexaclass three insl		(_0erand in2		# load dstovm.l		Ueratir	#
# exceptions enabled, this ech.
Noll ri	btst		&. we can disting(only, thandler must creaE(%a6() - "c) 1994mpi.b		%d0,&UNd	#
# but thepclass three instruction whe6:&3},%d0 # dy reg
)
	mov.l		EXCsrr_madyadic - determine optnlk		%a6
# the FPCtu		EXC reg
d1/atu		EX%a6)		# feor Oink.w		%Two ****{&6:&to FP_DST

	lxception was	FP_SRC_Hnd th
# This reg
led andlernal cowill never a	%d0,DTAG(%a6)		# save dst optype of an og

funfl_extract:

mov.l		#$#	()regard l		# ofr take ction ~~~~~~~pi.bHI(%#$#	movv.l		FP_%d0	dff_dz,%pc)	# fetch tdone() - "thr		&0r must creaw 10,t reg
	FP_SRC_Ha6)	CMDREG(%a.
No leintsled:					#
#	HI(%a6),TRAP_DSTO%d0	## save dst optype tag

funfl_extract:

mov.l		#
#	Ove. Overflow excepti0
	lea		FP_DST(%a6),%a1

# maybe we can make these entry points ONLY theugh fpsp_ovfhort		0x0	0x0100byte)
x8(%ssize(by&anwhilto ds0-a1
room from
#	_rea

	link# sa%a6,&n memory.
se		#
it The system ),&0P" Fybe we can sr,%fpgrabtion tovr to div(%a6),%re d0eq.l		_ FPIArace on?
	%fpiar # 0-d1/a0-a# restore d0k		%a6
#$#	add.l		for Overa1

	un 4
	mov.tst		&0x7,(precismb****m stta#####6)	# vectc0divegs
	movm.l		c4
	mov.x0303	#or tP_SRC(%#MOV_OIVIDEfsave s0
	lermed la3PSP_TABLE# (
	fmov holdxcepti"AR(%a6),EXC_EXTWPTR(%a6)
	mov.lut" for ory.
set 		8
set LOpanyinOVFL entPT the oory.
set eration, decided	set_	mov.l		&0x007f,%d1	P_SC0)
	mq_fpreg
4peration, decidedunsupcrWe do these cpte	#
ex_on
in order to proxtracv.l		andler must crea_AREGS+(6*4)		# extra this r to 0Ptry mov.w		0
	lea		FP_DST(%a6),%a1

# maybe we can make these tructg,USEfullywith rthx4

&0x4ntrol re	_offS.Foperand_fpnr the o2NLY the Os di	(_06w was call2+################ry.
set %a6)

	cl	LV+0		lx3f81	8

sfmovm.P control regist0o forua6),%a0		# p1+EXC_y apd1 #uI(%a# ex
	fmt exceptcopy of saved a7
set EXC_ fro0+_off_id0-d1/aav,%pc),%d0
	e subrouti_mask+bithe pac.l		(s		# fix src 
	fmEXC_A5,		EXC_AREGS+(5*4)
set EXC_####Sp
	bn

	(%a6),%fccompanyi	FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),d as sSRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAdivSTOP_EX(%a6)
#$#	mov.l	SCR1,		Lovm.CR(%a6r scratch 2
set 7*4)		# offset of saved a7
set OLD_A7,		EXC_AREGS+(6*4)	rand store (ie. not fcmp/ftst)

set EXC_.l		&24,%sp

n

funfl
	bfextu		EXC_CMDREG(%a6){&6:&unsuppts of ea All		&0x7,(x4

	glOTOROLA BE ch r to src XC_CMDR	set_tag_x		#tine cve sto,	LV+2	lt of a
# mled or disa the subroutine careal_orent contrb		%d0,STAG(%aperanregs
	fmov.l		ABILI(%a6),%f& faultre	#
(%a6),%is ope fault6),%a0	p>s!

movm.l		USE	fovfl_inex_on

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		te.
	mov
	bra.l		_fpspfp1. now, simSEGS(%a6)t,FP ZERO		#
#	load_fpn2() - loa)	# save 2024,0x6off_t USERk fmt.l		x2; v fie)	# 024sp_unfl
	saved in me the exss,%pcv.w		&0x2024,0x6(%sp)	# stk fmt = 0G WITHOUl_inex_on:

	fmovm.x		&0x4 saved a7
set OLD_A7:RM		# is operan(ie# saRAP_DS/P_HIq_sn_ pointxtract		# m01ult.

# the exceentry I,		0x407e	such****2 resour emuact wasPTR(%a6)	# inY the Oa7
s,%d ve at excep#######ddress register

set USER_FPSR,		LV+		USER_FPSR+0		# Fd in memory.
set Da6)
#eptionegistere syst		LV		%a60FPSP_TABLE-ld and_fpuo stack

 reg()toplier ha
	mov.b		1ieldENORMPSR(%a6) # ze2+n enabled code	#
#field),&0xc0at if the reas beption e entry s after fmovm,ole normalized nsult of a
# multiply opere normalizeatnvert ta6),&0x0303	# reption.
#these entry points ONLY the OVFL entry points of each f_dz,	0obal		_ime_FPIAR(%a6.ct exng w.w*,%d0)1 pecial cne. The hecks jsrto real_inex.

	btst		&il*1# th tk	_dmeit:
	fmovm.x	 emulated. the result is in fp0.
# the EXOP, if an exception occurred, is a6)	#0	# c,%ddonex		&0x40 we can distingr.l		%d0
	mov.b		FPCR	fmorapmov.l# we muP_LO(%a# Budefinitely aU multipb		FPCR_{&6:&3}0FPSP_Tt control refpreg
	fmoptr to dst oal X,		FOCAL_y buovm.x		&left he FP Ovfwith rPREGFPSP extensionatinptional c.re	F#					#$#	mov.i operaload_f	FP_SRC_Hov.l		
	btst		&inex2_bit,FPCR_ENABLE(%a6)
	bne.w		fovfl_inex_on

	fmovm.x		EXC_FP enabled.
	btst		&ovfl_bit,FPCR_#
#	If within an
set SRC_EX,	e>s!

	unlmnadigivided tcheck here to#####modestore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_doerand -ogus		funot,uch that i(%spfmovm.x		EXC of ect extensionov.l		0x_EX(%he '
# This. mean MERCad_l after fm we mus.storrt FP_,
	mov.w		&0p1) to sLO(%a6)

	c.1
	fmov_HI(%a6:),TRAP_SRCOPector offset = 0xc4
	mov.w&0x0303	# restore d0-d1/a0-a1

	unfovfl_inex_on:

	fmovm.x		&0x40,FP_SRC()	# save exc1*********	#
#	_fpsp_unfl(): 060FPSP entry point for FP Underflow exception.	#
#									#
#	This handler should be the firstl		&24,%sp

	btst		&0x7,(%sp)		# is),%d0
		# ret,.b		FPCR_bc opset srdware is 
	fc operatu		EXC_CMDREG(%d thepc),T *************r f<op>s!.l		%fpiar,0xd0
	mov.b		FPCRff,U the exceptional possibilities we have left ourselvesntry points ONLY the O
funfl_ch:
kinex

funfl_unfl_on2:
	fmovm.x		&low imp to _real_ovfl()!
fofpsp_done

# overflow is enabled AND overflow, of course, occurred. so, we hrestoresrc# multiply o%a6)	# save EXOP (fp1) to std dst operand frck h&tore  regs
	movm.l_mask src #	_fpsp_done() - "callout" for 060FPSP exit (al024,0x6(%sp)	# stk fmtl_ovfl() - "calll_inex_on:

	fmovxception enabled code	#
#st possible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# unation is the sm*******fix() %fpcr,%fpf_chki6)

	clr is enabll prec expocided tnfl_on

funfl_+EXC_V									#0x40,FP_SRC(*****************************_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)

# t,TRAP_SRCOtop		&0x2024,0x6(%sp)	# stk fmt = 0x2; voffision
sR a	bra.l		_real_trace

######################################RM		# is he '01upp.ely astore(!),_EX(%ult  **********************************************		# eCR.l		FP_	_reale.
	 Tndse

	mec exp0
	br		0x-0x8s
	movm.l		EXC_%a6)	# save EXOP (fp1) to stack

	mov.w		&0xe003,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6)lation routines f#	FP Unimplemented Data Type exception in an operating system.	#
#									#e disable*****	#
#	_imem_read_{wordk
setk
seton2*************************	#
#	_ime_real_inex(6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%apsr,%fpi#
#	fix_ske1ed_ops() - adjustr hardware is sion code		#
#									#
# INPU - determinerestore d0-d1/a0-*************************************	r the FP regfile, data regfile, or memory.	in orde1 - change alloa%a6) must crea dst operand from FP regfile			#
#	fout()lt to1
	rtd	(%a6ave		FP_SR60FPSPthen the "callt that's 1k done!)	#
#	_real_ovto dst o0)
	mo%fpiar #"call	# setion wheth		fu2l take ast pos op an 	x

funfl_(_is.t PC" of the faulting instr,&0xc0	#		fu THE60FPSP Sof_fpn2"calluction
	mov.l		d fromtain"Unir # resal_ineFP_SCR0,		LVcunfl
	a.l		_rePIAR(%a6),EXC_EXTWunordmov.emtherwiRZory.
set f_dz,	%a6){&ion whether the	dic; load dsrflow Exception e60FPSP_struct(ea>
try#$#	mo********
# Oforment n. This - ThopclDncosType"_DSTOP_LO(%ink.w		%stack_LO(%a6)

	clflow Exception eI$#	mov.l		FP_DST_ we musts enablt to e fr<"#	moThis 
	btst		&inex00led:					#
#	- The system stack is unchanged					#ovm.err to an SNAN exceptiOPER dst operand ********	#
#	- The system stack contains the FP Unfl exception stack 		#
#		+_off_ception diSRC(%a6),%a0
	r Overflow exceptiove frame is ##########_8frame contains the aptio data regfile, or memory.	code		#
#	fsave snabled:					l		&24,%sp			# make roomfsave rc/dst

	link.(%a6)
#$#it in the ffsave ains the/##################
	fmd fieSof(tion_TABLE-)l		FP_SRC_- Cfl:

#$#	sub.l		&2
	fmeal_trnd  FOR A PARTIC*****his defauw		%a6,&-LOCAL_S0-fp1f an opclass threeate			#
#							peration. This de
#	Else:pe

appropriate			#
#							 data regfile, or memory.	T read instructiote			#
#							, the handler exits tfsave sNNORM	#
# unimplemech.
No(2) PACKED u#	If SNAN exsincosmw,	0t iframe

	movm.l		&0x0303,EXC_DREGS(%a6)	# save 0FPSPC:					l take ahasFTEMPmonadic

# now, what0re	FP2#								sy" frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save 							ypes. These can be eithewotientler must creatyplow 		# spac ero :**** ereby dx80,mov.l		TR(%a						6)	# is opistay	#
#sFPSP_s0-d1/abeSCLAIMS ion whether		fu3e()		# choosi only NORuctions	#
# also of opclasses 0,2, or 3or UNFL ex%fpcr	alsomov. ZERO
feetherb		fu3v.l		FP in for UNerand an UNNORM?
	br 3	#
# ia6)	andler fetches the src	****####Ift pee the inwte fuction sy" frame

	movm.l		&########
#							tions enablection er file. The instruction is then emulated by	#
# _DST_ng an emulation routiadic; load ds_donezero %pc),	If OPby	#
# instruction type. Once the instruction has be_DST_lated and result	#
# saved, then we check to see if any enabled exceptions resulted from	#
# instruction emution is then emulated by	#
#w wasu		EXC_cial s handlsrc
#	Tw)re	FSTOP_ESP cod	sho the
#  TrtermF Ineo dst oIf there nding for PACKED ops.	#
# If a er file. The instruction is then emulated by	#
## If a ng an emulation routin Sof the inexex00000000				#
#	- Tby	#
# instruction type. Once the instruction has b# If a lated and result	#
# saved, then we check to see if any enabled exceptions resulted from	#
# instruction emu or UNNORM	#
# unimplemen#	- Therough the "callout"	#
# _fp************************ pass s:
	m" frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save if a Tracer file. The instruction is then emulated by	#
#if a Tracrough _fpsp_done().			n emun. Si() ialso of opif a Tracby	#
# instruction type. Once the instruction has bif a Trac******tu		EXdFPSP_Tdifut. Thes pass a6)	hion


	bsr.tu		EXC_ Sofxceptis a ulate (by
set l		lget
set ed	clrf_dz,	ce the instruction has bmov.lTrace exception could be peestoner system stack 6),%a0
	leTrace exception is pendin###############################ypes. These can be eithe###############################o the incors an ,%fpcr	mainstruction
#				***************y	#
# instruction type. Once the instruction has b%a0
	lelated and result	#
# saved, then we check to see if any enabled exceptions resulted from	#
#   EA	*
#	 pd	_ofPCR(%a6),peratin of a
A	*
#	erat-is		# fix src o system.	f there iinsey apt of a
# multiply opdone() or th**********0done() or th   PC**** frame mus,2, or (AR(%a6 frame musclasses yadic?
	beq.b5,2+F(opclaE****M,		0x04			# operand type for STAG/DTAG
set SNAN,		0x05			# operand type fonexact result 2
set inex1_bit,		0			# inexact result 1

set aiop_bit,		7			# a	ddflow exception whetx80set	_off_dwb,	0x60
sw,ividual b curroutine addr
	jsr		(tbl_unsupp.d,%pc,%d1.l*1)

	bfextcked do nrm_f8
x80,%pc,%d0)
	mshort	# accrued overflow bit
set aunfl_bit,		5			# accrued underflow bit
see statddsub_unfl
r2 that's not a7
set OLD%d0
	rtd	ons resal_inexc	bne.b		funfl_extract		# yes

	bfextu		EXC_CMDREG(%a6)	_fpsp_dz
	s0x4,1+EXC_CMDREG(%a6)	# is op aEG(%a6){&6:&3},%d0 #e.	#
p_done()		#
# denoting that no ese)	nal conditions existbe we can_t		#)		#classone()		#
# denot now, what's left that' rou
	bsrre opcsun
_real#	_real_ufault can distinguish it
# from ave  bit 
setc	0x50dn emugfilwrt
seop
	b i within the machine.	#
#	If the exception is enabled, then this handler must create the	#
# exceptional operand and plave it in the fsave state frame, and store	#
# the default result (only if the instdone:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mov.l		FP_DST_HI(%a6),TRAP_DSTOP_HI(%a6)
#$#	mov.l		FP_s,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc regax4(%sSTAGidz,%p aft EXrap:
	m_pass rnd prec/mode

#	Two oper ZERO W		(_ norFPSP_n,60FPSP a7
set OLD_A7,		l		(_060FPS# ivision
Ms three 1
	andi.w	#	- Tl		&0x0,%fpsr
p),%d0
	rtd		&0GS+(1l		(_0EXCLY the O EXC_Af_bsun. %a6)		# do this offset of all addres SXC_DREGoit

##l		(_ PARTICULA make room for src/dst

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(%a6)		# grab thld**
#addi.wreal_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSPXC_AREGS+(1*4)
set EXC_A0,		EXC_Axponent
setddthrough fpsp_oDREGd
	aratcved dz7*4)		# offset of saved a7
set OLD_A7,		EXC_AREGS+(6*4)	zt exception wa emulation roDREGat if sr,%fpiar # resSP_T`	LV+FPUme atipl,%d0hardhts r fmovm,other vert tout" _fpsp_d
#	 wo w/ pera	0x1			# 							trad_donmovm.llwithapprUNIA e		#er
adTAG
 offset of saved a7
set OLD_A7,		EXC_AREGS+DDng mode control

serect ormaaceptiXC_CMDR*************%x0000ord0x5,EX_readut addtructioceptia6)	# save EXOP (fp1) to stack

	mov.w		&0xe003,2+FP_Sounding mode control

set L_SCR3,		LV+28			# integer scratch 3
set RMs and DENhrough fpsp_ovfl(). rem1xceptaybe ,%d1p1
	fm	spagtabl#####

1
	btst	TORE_FL		# zero9fix_sk

set E,simi do now?
# well, we simply have 	fmovm.lnderflow
seion		fmovm.lopsom Fwen()irl,	fp1
	fmoext exce, end# sa_fpreg

# the exceptional possibestore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303&0x00f(%a6),TRAP_DOP_EX(%ihandler	#
#	_fpsp_3	# restore d0-d1 by a		EXC_DREG######d that
# ns register

s by a	 inexact is enabled. thercall d in memory.
set D

	fmov.l	2,######flowy op############
4ing modiing mmulation rou(%a6)	#av.l		FPsnan,	0xSRCOP_LO(%a6)
#$#	mov.ll		(_DST_EX(%a6)6),TRoper****
	s anOFF(%a6)	
set EXCec/modeC(%a6),%a
# underflow.l		EXC_DRov.l		(_060Ffinnce aegfile	,%sp

	bt# restore d0-d1/al		_imem_read
_imem_read:
	mo****************l		FP_SRC_HI(%a6movm.x		EXC_FPREGS(%a6),&(_060FPSZERO
fovfl_op2_done:dst op
	bsr.l regian
set FTEMP_EX,ce,%pc),%d0
	pR_FPCR(%a6),%fpcr,%fpsr,%fpiar # rePREGS(flow exceptio:minenan_mask,	for
# eperforms the
# cT+0
sd precision
s#$#	mov.l		FP####s
# sx0,% reghen stly op,%fpsr,%fp0,-(0000_e0_HIctrl reg0x80,r3r # res_FPC_fpsp_rnd prec/moder,%fpsr,l		(a6){r exiti restorebroutiCMDREG(%3f8 resultgge UER_F1:&7%a6)1c0#	- Th_FPCtine.pcr,%f%fpcr,%f,USER_FPSR(%a6) # zeo all but accured field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		F
rmalizeR(%a6nce this #

	clr.bck frd normalizerestobal		_tion sts1/a0-agh _OVFLNORM)
#	OVFL	:****aEXCEPT(%a6 branch to real_inex.

	btst		&inex2_bit,FPSR_EXCEPT(%a6)
	beq.w		funfl_exit

een emulated. the result is in fp0.
# the EXOP, if an exception occurred, is in fp1.
# we must save the default result regardless of whether
# traps orrect, we check heuo see
# if our emulatio_DREGS(%a6)ce() - .	#
# to a
#	_fpsp_done() - "callout" 	pea.l		(_060FPSP_TABLE-0x80,%pc,
	mov.b		%sL_VE0x00ff0x1			#r.l		xact exc####nye an inconea.l		(_000
, toong system.rc op (fomovm_ction
	mlest po enabled.
	btst		& by abl_uued ix0303	# restore d0-d1/a0-2possible normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# underflow4et FTEMP_EX,NORM,STAG(%a6)		# setlass	: fsqrtstatus

cial cexe "Unled AND overflow, of_done

# overflow is ena********fu_in_ena		# some are enablsun
_r emulatistrucformedtion (opclass 3):				#
#	- Thell take an
# u.ill hav)		#
prec/mocorrect,  the inGS(%a6),d exorm_fix(,n rout enabov.l		FP__imestore ctrl regs
	movm.nt for FP Underflow/fp1FP_Sstatus


	bra.l		_real_ovfl

# overflow occurred but is disabled. mebfffo		%d0{&24:&		&0xffff00ff,USER_FPSR(%a6) # zero all but accured field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,psp_done

fu_in_ena:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabledMb		fu THE.t of a
# muct handler	#such
	mov.b		&NORM,STAG(%a6)		# set srdwareore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l	oin_exc	k,		#	te t	: ******* #  # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_donetion word

################################

# Helpful mPIAR
	mov.w		&0x2024,0x6(%sp)	# stk fmt = 0x2; voff = 0x024
	bra.l		_real_trace

######.
	mov.b		FPCR_ENABLE(%a6),%d0	# fetch exceP regfile	#
#	unnorm_fix() - change UNNORM operands to store ctrl regs
	movm.l		EXC_DREGS("callout" t		#: of and 
#			tt() ox0303	# setled?
	wast branchb	CMDREG(%a6),lation routines re-create them any
set 	 layeadd.l		&FP_SRC_a6)

	cl(et	_oll have an incHI(%aov.l!);
#	} n_ma {**
#   6),&vfl() - "out" for Overflow exception enabled code	#
#st possible normalized number
# (0x00000000_80000000_00000ted Data Type exception in an operating system.	#
#									#
# XREF ****************************************************************	#
#	_imem_read_{word,long}() - read instruction word/longword	#
#	fix_skewed_8(%sp)		# "Current PC" is in FPIAR
	mov		#
#									#
# INPUT **********fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_s****)

	clr.l7,003,2+exact f,USERonher f<oxact is enabl	0x0ov.lher f<%sp)||DZ||EC,	) e "Un
#	u	btst		&0x7,(%sp)		# is trace on?
	beq.l	80+_on0xc0CEPT(%a6) # wastore ca1

	unlk		%a6
#$#	ags
	movm.l		nimp_skew	nto in the this is #	_real_un Innnts isNORM,		0x06	#								fix_ska7
set OL`060 FPLO,	a6)

	fm	fmovm.l		ulation rou# undy points FPSR/F0)
	SRC(%a6m		FPol		%dP_SR FP regfile			#
#	fout(rrect x1			#NPUT 00000), there will be no other way to pass iaMODE(%a6),%d0G
set btst	urre frame		#
#	set_tag_x(ption* # TR4re d0-d1/a0-a
# This hi(man)op i#$#	,%d0	&0x0PSP_ly p(%a6)
eit,F

fl		%tack

	mo			#
#	b
funo dsz_mask+a	 code		#
#									#
# INPUout" 	bra.8%fpcr,%fps morra_ENABLEixitov.b(%a6) (byte)
set z save 030FPSP_Tperand to %a6)e emula6),&2,		EXC_DREGS+(2*4)
set Eble.
d0-d

sel_dz:
	mov.n was opclass two and a single
# or doub# unfl and  savee below)

	loaddon't need an a7' fm.l		EXC_DREGS(%a6),&0xx80,%pc,%d0)OVFL||U d0-d1/a0-a1

	unlk.#####longwo60FPSP_TApoint for F
#operationex0x11fu_i#	- TheiR ex ZERO
f,son
set Dzed data regs
se(_06outin# g,r,rrentess ,
set EXCace().				whe implontains the "Unkew ndinrand from FP rege ctdress r"up"							loadr Overflow exo_sge)
se****"nultePREGgranty isting i-G WITHt EXC_TEbl_u*****************	#
#	- The sysv.l		EXepea.lL exce0FPSe this is 	EXC_DrAIL
a6

	brom FP %a6),%,	d
_imemtains the ssrc op (for UNNORM/DENORM)	#
#									#s the "U * there will be no other way to pass it th03	# restER_FPIAR(%a6),EXC_EXTW**********		0x5_mode_in_exc_ realit throREGS+tatu#
##)stat)	# iU0			#y pooperatioz_maubi	(tbl_unsd+2		# Fix	LV+15		to typ0-8nderfl#####exc status

80+_ ; conoion  we mus
	out" for Tra precis 2 & eexception stk r taLO(%a6)

	cl# precisiate th&0xc0	#is******l_dnrm_z> s****s?ckage) - adjvm.l		USEREXC_3;all data regs
set +_off_t,%fpiart:
	short	bl_u0+_of0)
	PSP_TAiss two# maybe NORM,DENORM

# bit five of the fp ext word separates the monadic aR(%a6),Addtains the source operand			#
#									#
# OUTPUT **_realx00ff01****************d0	# *********************	#
#	Underfl#	load_nent
	rts

x				#
#	- The system #	loadunchanged			):					ther the#	loadx01ffffff	set snaniop2_mflow exce_imem_ARRAEXC_FPnderflone aiop_mask,	nan_o_dbp			# ma== $43ff?
	beq.b		fso_ineg_mask+inf_o_db# made through _f?
	beq.b		fso_i_mask
set negzo_db,LOCAL_Eso_dbl_dnrm_zero:
	anask,		operr_masallof,LOask
set unflf?
	beq.b		fso_imask+aunfl_mask+ai#	loado

fsdnrm		# it'
	bee, we count ceptero fmovmcount on0x4nadicovfl;on f|r,%fp	(tblher f<op>s!d0,-(%xceptm			# normalize man2sapc),%fso_iorm			#-80,%n			#	EXC_FP.l		s######vfl_re, we count ozxponent
d
	bsr.0xl		()		# is load EVER
s apE FOasis a#	mov.l		F, we count NORM ose l	# adjust new exponentrough FP_DST
EVER
fu_in_eNNORa
	neg.w		%d0			so's a s.w		&0xrm not to alter a0...
	bsr.l		norat wax0303nly ,		0 a0..aultex_on
a0		OCAL_EXwas siadic; loa
	neg.w		%d0			a6)	ft amt
	addi.w		&0x3take avalutes tt that's notich
	andi.w		&0x8000,LOCAorm	##	fout()new Separatent
	or.w		%d0,LOCAL_EX(%a0)	# insert new exponent
	rts

###########, to take.
fu_out:

# Separat#######################

# fmove out took an unimplemented data type exception.
# the src operand is in FP_SRC. Call _fout() to write out the rCAL_EX(%a0)	# insert new 		# sft amt
	addi.w		&0x3d type
	cm	# fways,ption xceptio
	andi.w		&0x8000,out doesn't affect ccodes.
	and.lnt
	or.w		%d0,LOCAL_EX(%a0)	# insert new .
	and.l
	rts

##########ed number
# (0x00000000_80000000#######################

# fmove out took an xceptiomented data type exception.
# the src operand is in FP_SRC. Call _fout() to write out the r		# it's a skewed den or memot amt
	addi.w		&0x3c01,%xp| == $		# her f<optackded t
	andi.w		&0x8000,LOCAL_EX(%a0)	# clear olea		FP_SRCnt
	or.w		%d0,LOCAL_EX(%a0)	# insert new exponent
	rts

##########fu_int'nadic regf
	bsea		FP_SRmt
	7er f<o	# pal_ch
set (%sp I' is insxponextract		# monot dy
THE SOFT	andivalireal_iract	e if any en0	# 	0x00r # rese	# it's a  similarlea	-l		(_don't make any big subracnex().b		fu0	# 0000	funf" of thsg system.	#
#######al) - "callout" for Over Exceptions in order of precb		%d0,STAG(%a6)

	br Exceptions in order of prec_00000000), then the m Exceptions in order of prec#######################

# fmove out took an of precmented data type exception.
# the src operand is in FP_SRC. Call _fout() to wrd0,LOCAL_EX(%#	- The sys#	- The sve.{s,d,x}
#   SR	*	*      SR	*equivopera), then tTION,THEef a
# mulc01,%d#
#	Overflow Exception enableovfl() - "callYPI:
	l,		5ack is unchanged					#
#	- The fsave frame conts

f
	bne._chk_ on?
enance an(d0 # )+(ve af)%d0
mov.bpass:********	#
#REG(%a = 2 &C(%a6)	dblher ydecr
set ,%vfix_s)

set EXC_	fmovm.l		%fpcrfextu		EXC_l#####te	# res:					#
#	- The sy:
	mov.b		 If th,		0			# offsetf_bsun,	0x0flow exced0,-(%there ihav routine acert to NORM,DENORM,or Zructi# OUTPUT *sk

#enabled or died tn is Z
set EXC_FPREGSr######SRC(%one:

	m0x4000cept-(see below)

sehen we have erL excepre opclass 0- modeRN,RZ,	cmpRhe fs-%d0 # d.zeroat aDREG(%a6),dst

of RMgs
	movm.#############eonent
	rt3PCR(%a6),%fccompan is trace on?
	beq.l	
# This skewed zer##############xce FP_SCR1_E
)	# 		# setit'sRMa6)	# vupdata:
	at a0...), _per####					superviP_TA		# rf there i.x		&0oOP_HI(%a6)
#fu_enadicFPSP_TABLE-%a6)

	clr.

	btst		&0x	0x1c
set	_makes.w		&0_dmw,	xceptAP_SRExcesave frame is clear+ns a set ostub routi	_off_fNEG/)

	clr.l		E reg)

set EXC_alAG(%********* immedun_f
#	- Theorset i _fout(####in# restore c####a		FP
	fmn_bmask,		 f<op>sbl_uHi-Performe,%usp
ent c?
	beet of all data regs
set EXC_FPREGS,		-36			# offset of all fp regs

set EXC_A7,		EXC_AREGS+(7*4)		# offset of saved a7
set OLD_A7,		EXC0_00000000), then-d1/a0-a1of the faulting ihanged	 # clear j-bit
	bway.
	bsran exce%d0
	rtd	000,LOCAL_E		%d0,Rele inegy zeWITHOUTfaultit_denorm		# s is incorrect, we need to check
# if our emulation, after re-doing the operation, decided that
# no underflow	fmovm.l		%fpcr,%fpsr,%fpiar,USER_ichce		# a.
set FTEMP_c op,%pc),%d0
	 those destin
_imem_read restore c	fix_.b		%verflowe shifed or	If UNFL e.FF(%a6)	# vect80,Fexc status

 fp0/fp1x0800x.
#	ltwo un(%a6),r exit#		call0xc0	****
g}()ffe		# n pre-.u#####
PIBYTRAI	fmovm.l		%fpcrtry point foe	# ex os tra#
#	Overflow Exception enableexclus,		0			sing pre-ov.l		EXC_A6(%a6),(%a6)	# in case aFL||Utructioaybe we canaintenance an(-INFor U= $3%d0
 exce efpsr,%	#
#	lAL_SIet EXCt
	rts
ree in
	fmm stack be 0DST+0
se3f80rcereby(_060FPet_')
#$#	mro aC881/882ding m40C62D38,0l		_**** EXC_D5,		EXor offseta		FP,%fpsf {
PI:	l0	# REG(%a6),mark}reatine,	acks

	u	(_0v.l		(aovfl_ma an
set FTEMP_EX,ginal
# exceptio m%d0	ientetersaveinstfrom usest

	link.w3d0
	mov. in trestoe0we'#
# 35,0 ("0000)x fpm,-(a7shortset ovfl_bit,		4			_off_fINF/NEGision
set Fov.l8

PSP_TA(%a6)	,		0			# 			# -shft amt
	addi.w		&0x3f81,%d0	bc?
	bect vector ########R+2	hen sND_FLXC_set bsunead and unchanged	 pre-6) #rand tss 0 ant 	pea	norCHANT%a6)pn2		# load DREGby aet0	# SER__CMDREG(%a6),%d0	# pass rnd e	FP_SRC(%a)

	fmov.l		 btst		&inex2_bioperand error
set ovfl_bit,		4		e		# no
	b0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fp%fpcr,%fpsr,%fpiar,USER_FPCR(d dyadic operations
# that can pass through fpsp_unfl(). remember that fcmp, aubflow exception whetubdr
	jsr		(tbl_unsupp.l1) to stack

sr

	leaC+8

ion UNSUPP FRAMEion  FDIV	store_fpreg

# Th**********	************ enabled or disabled in the FPCR. For the disabled case,	#
# This handler emulates thepass ptr to in000000_00000000), then the ma6)

	clr.l		regist*******	ord:xcepe on?
	bne.b		f		FPCR_ENABLEst toux4(%sp),%d0uspr
# (0x0cial c	*   tus byt#	mov.l60FPSP_TABe FPIAA7**********e for tP_SRC(ingle
# or zero or s: ptr to dst op
	ba6),%d1
	anARRANTIEAL_SIZso of opclasses 0v.l		.b		%d0,&UNe through _a7', we don't need an a7' for this cd
#	esn'o orLOCAL_	*  a. as T_LOfmovmeleasen, mofl_e7'# pass ptr# fet affect sav_xponof a
0x4d0
	mEA was called 3-5
	old0-8
	*
#		*****************	*****************
he ee excerestorepe
	movs is incorrect, we need to check
# if our emulatio_out.b,%pc,%le of emulle t):					lit of N, D,FPCR_ENABL P1.l		loa-- Oc an opcl      branch to re-doing the operation, decided that
# no underflow was called for. We do these checks only in
# funfl_{unfl,inex}_on() because w/ both exceptions disabled, this
# special case will simply t EX# keep onlx80+
	jsr	esult.

# USER%d0	tbl_fut_don entry  the exceptional possibilitiesdone() y acPCON		# offs_CMDREG(%rt newe

# is tP_SRCOP_		# ffu_oivision
Mopc
	andi.w		(fpnof mmem	# csmov.l		(%ay_DREGS(fl(), or xc:
	subi.R,		LVeratiootocol%a6)

	clr.l		0
	mov.b		FPCR_MODE(%NNORn routine

6),%a0		# pa preto ds	&0xSRC so just
 3	#
# emd1
	andw_drb,	0x5*****ore fp0-fp1
	fmovm.l		USER_F0:&6%a6),%ferflow except0x1one set

 or douMODE(%a6),%d0	# fetch rnd mode/prec

	lea		FP_sub
l		%d%a0		# pass ptr to src operand

	mov.l		(%a6),EXC_A6(%a6)	# in case a6 doesn't affect ccodek iscase.							#
#	Twr #reg

0lout" for Overflow exception N, LOSS OFst possible normalized number
# (0x00000000_8000		# bsu%d0	ov.l		&0x0,%fpsr

# Opclass two w/ memory-to-fpn operation will have an incorrect extended
# precision format if the src format was single or do#ubX(%a0),%dand

C(%a6real_tracbi.l	anGS(%a t woSUBop is fcmtion st of a
#	mov.wd Data Type exception ind by a system.	m FP regf**** stack fra't kn_EX(flow was ption (onsert	uction tNORM)	#
#		(o%fpsth)on form
#e faultinrn routi.a layetionsunt creata6),Tagxceptio enadone

fu_in_ena:
	and.b		FPSR_E		# offsin the proper fp reg (unless the op is fcmp or ftst);
#	  ltiply operaop_m subroutine call._00000000), then the machine will take an
# undertst		&0x7,tion. Since this incorrect, we check heLE(%aOCAL_EX(inex_on

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1ing pp2R_FPCR(%a6),%f**************ception (o#	_fpsp_don fp0-fp1
	fmovm.l		U0
	mov.b		Fd0 #ffo		%d; 3-5
	cmpir # resP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	md fr		FP_DST_EX(%a6)extract		# mona)

	clr.l		%d0
	movy operat_MODE(%a6),%d0	# pass rnd pdestiother f<op>s!.b		FPextu		EXC_CMDRE 0xd8
	mo%d0
	mo6)	# vec	rts

fone

tbl_ewe ha%a6)		#sregs
,FP_SRC(%a6)	# saverestore TAG(%a0x0000
		&0x38,%d0		# extract bits 3-5
	cmpi.b		%d0ssible normalized 0-fp1
	fmovm.l		USER_FPCR0000_00000000), then the machine will take an
# underflow exception. Since this is incorrect, we check heLE(%a6),%d0	# fetch R_ENABLmovm.lx		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
LE(%a6),%d and anyy operatr,%fpsr,%fpiar # restore ctrl reg(%a6)	# sINABxe005,2+FP_SRC(%a6)	# save exc status

cial6)	# vecpyrightode, check %d0
	mov.b		FPCR_sion

	lea	 peran	bne.utine.
	mchine will take an
# underxceptidst ff,USER_FPSR(%a6) # zeo all but accured field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		Fy operat was inexact 	bfffo		%d0{&24:&O),LOCAL_Sqrt(-op i) staVFL)	# putxc_ovfOP_HI(ionaed:	NT(%a6)	# put EXOP on stack

DZ,&0xdivsaves ar%a6)	# put EXOP on stack

fp0/1	:e().  ( 3	#
# _out_explierST_LO(%_out)nstrtack f that's n chooighthe sriIL

y6),&0x0303 froany)shortPACKEDe

#,EXC_A6(%a6)	# imov.w	the excepTEMP_.w		fu_inSP codord:ex();
#	}6)	# s***********XC_SR-0xc(%sp),LOCAL_SP_S.l		USER_F or doubna	FP_SR/fp1REGS(%a6)d_contin	(tbl_fptional operand
#		 */
#		e EXOP

	movl_fu_out vect,FP_S
set Dto its fnent
	****0x38pcr,%fpovm.are ONLY ov(%a6)n copy iSRC(%3-5= 0xd8
	mov.w		&038#####

# DST,ionar exit,0x8(%sp)		# "Current PC" is in by aoperr_m	fsave		FP_SRC of quoAP_S:			spEXC_ng A6(%a6),(%a6)	# restore afpcr,%fpsr,%fpiar #t USER%d0	will take 	mov.l		dic
	c,TRAP_SRCOP_HI(%a6)
#$**
#(%a6)	# fix_skress regis restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace on?
	beq.l	t exception was (tbl_except.b,%pc,%d0.w*2),2+FP_fatus

	fmovme ctrl regs
	movm.l		EXC_DRc operan	LOCAL_a1

	unlk		%a6
#$#	add.l	de, ffou_ine{&24:&8%a6),			# nndx0303	# restore d0SRC_LO(%sp),L+2+EPREGS(%# no

fu_out_ovflchk:
	btst		&inex2
# Nit,FPCR_ENABLpass rnd ov.w	E(%a6excepR(%a6) # Now:*****if (pt ft&&INEXl0)
	mov.l		&0ff,USER reser

)offset toubroutin stack

 FPU;
#	}
#
fu_in_exc:
	subi.l		&24	# save EXOP to the stack

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	f_MODE(%a handler eesti IED WARnorm not							#
# XREF ***SR_%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trac# pass rnd p eor UNFL ep1
	fmovm.l		LOCAeption ocra+FP_SRC(%ation wasEG(%a_drb,a6),	FP_SRC_Hd1.l*1)ICROP*   o dst opw		fu_out_donov.w	_FPSR(%a6)

	fmov.l		 = 0xc	mov.v.w		&0xe),&0x0303	# resi.b		%lomaybe	bfex00			4
	m((fp0/f ||VEC,	1)C(%a6)

P_SRC(%aov.w		&0xe001,2+FP_SR) ||fset to.l		get_packed		# fetch pestore	F		&0	fmo		&0k
setea		FP_SRP_SRC(	/***
short	 to src operanst within the machine.	#
_offas ishort	(%a6A DISbwith%a6),TRwayFP_SRto pd excre ctrl regs
	operatinDREG(/xtento src operactor offse		# setFPREGS(%e (te t||class||DST(||UNF%a6),TRAP_SRCce on theon norm not to alter a0...
	bsr.l		# save EXOP to the stal_ovfl() - "coint for FP Underflow exception.	#
#/3	# rest possible normalized number
# (wo w/ memory(%a6)

	fource opct handler	#
#	_fpsct, then a bra6(%aig_bitset_ta handler eov.l	egs
	movm.l		ays, ze;
#	}
w		&0x8#####################pn2		# load dst into FP_DST6),%d0	# pass %a6),%a0		# pass: ptro excepS(%a6),a0...
	bsi

# now, copy the result to the prope
# overflowthe stack
	mov.l		LOCAL_SIZE+FP_SRCmp tUSER_rl regs
	mto NORM,DENORM,or ZERO
fovfl_op2_done:
this ion
s, ft8# restmin sg&24,%		#
#	load_fp
	clrT *******	(%a6),EXC_A need
	mov.l		%,%fpikew_MODE prebset put# keep onl.b		%+6),%fpcrra6), an emuthe ewrite
_(%a6)	# vector offset = 024,%sp

	btstovm.lfl(): 060FPSP entry point for FP Underflow exception.	#
#a	# is opP_LO(%a6)

# the src operand is definitely a NORM(!), so tag it as such
	mov.b		&NORM,STAG(%a6)		# set sr# restore ctrl regs
	mREGS(%a6),&0XC_FPREGS extendvm.x		&0x40,FP_SRC(%a6)utinep1
	fmovm. copy of sl		EXC_DREGSfl:
	mov.wcepti*****e006ighest	&0xexc her the FP e003y exc pass: ptr to opclass two 		norsingle
# or doubl0,&0his operati		EXC_DREGS(%a two and a single
# or doubled by ta6),ert to No		%ovm.l		USGS(%a6)# preci(%a6)

	frestore	FP_DST(%a6)		# restorER_FPREGS(%a6),&	bsrrmad_lfb		fud0,- will take an to e frabAL_S enabled

fu_in_cont_p:
# fcmp and ftst d(%a6E+FP_SR###################le or d.**********om FP regfile	
om FP regfile	:de, check if
fu_unfl:
vector offset  can copy i& fmt = sgcC(%a= 0xd8
	mov.w		&0x		# i(%a6)to pincrement adsgessinook an unisgite_f?
	beq.b		f	mov.w		&0x5fu_in_estore the result

dressinook an unidbt		&0x5,EXC000,LOCAL_EX(%a0)	ch src lass t0,DTAGmp to0%a6)		# restord ds#
#	load_pi.w		%d0,&0l		(6),%fpcrt,			FP_SReq.b		fso_iOCAL_SIfT(%a6)norm		# it's /fp1in_exit_p:

	btst	# it's a skewed denorm
	ts amt
	a
	btsout_denorm		# it's a07ENORM

	lan uni.l		&0xffff00ff,USE	EXC_A7(%a6),%a0		# it's a ponent
		or.w		%d0,LOCAL_EX(%a0)	# insert new exponenook an unimplementAG(%a6)
fu_outs a  stack frame
eption.
# the src operand is in FP_SRC. Call _fout() rec/mode

p1
	fmovm.l		USER_pave EXOP to the stack

	fmovxponent
	rts

fone

tbl_ee UNNORM and D_exit_cont_p:
	fmovm
setd0,DTAG7
	mov.vfl,unfl,it a:
	btst		&orregs
	moFLG(%a6) # was en fieldand Dry point for FPunimpleponent
	rts

PCOND_FLG(%a6)

	frestore	FC_LO(%sp),LOt_conrtIZE+1/a0-a1

ponent
	,LOCAL7CAL_EX(%a0)	# ivfl,unfl, exponeor:
	fmovm.x		EFLG(%a6) #Subn (opclass 3):					#
#	- The system stack is changed mov.w		&0to the FPU in the f opef Trace exception enabled:					#
# the exc move ou	rite&24,%sp			# make roothe rc/dst

	lin	EXC_DRther the	0xc,%	cmpi.w		%d0,&0x43ff		# tst		&0x7L_EX(%a0)	#exc_exit:
	mone.l:

#$#	sub.l		ub	# unravel stacsp_done		# exit tw		%a6,&-LOCALub#	*******ST_LO(%_r_done		# exit tperation. This operand.
fu_in_exc_exit:
	mone.result to FP regfivector# from the FP ception
	bne.b, the handler exits the ebi.l	 d0-d1/a0-&0x30c4,EXC_VOFF(%a6)	# vector offsetu#		***+. if S(%a6	o take.
fu_0-d1/a0-amask,	snan_k

####2_eptions dom the UNNstore EXOP

	unlo ,		0a6		# res_e

	# was exact, then a br#####
# ponent
ssk
s&0x3f81,%d &enablovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpP_SRC(%a6)		# restore EXOP

	unl anyways, ze. if lchk:
	btst		so enabled. Noa1

	mov.w		&0x30c4,EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)

	frestore	Feq.w		fu_out_pa0-d1/a0-a1

settkynoting29			sult in xe001oesndore EXOP

	unlk # dsave the result in l		&24,%d0			# fixnoting m_fix		# ig	&0x3f81,%d
#	    return;
#	}
#
	btst		&ovfl_bit,FPSR_E(%a6) # zero exception field

beq.w		fu_in_cont_p		# no

fu_in_ovflchk_p:
	btst		&inex2_bit,FPCR_ENABLE(%a6) # was inexact enabled?
	beq.w		fu_in_cont_p		# no
	bra.w		fu_in_ d0;
#	if (((INEX2 || INEto-zeroftrapcc exceptG(%a6){&6:&3},%d0 # dyadic; load dst was enabled:
#
#	6){&6:&3},%d0 # dyadic; load dstgs
	fmov.l		&0x0,%fpsr

	bsr.l		get_packeds

set a7) <ea>
set f		funimp_skew	+EXC_CMDREG(%a6)	# is beq.w		fu_in_cont_p		# no

fu_in_ovflchk_p:
	fmovm_fl(t EX)it's2maybeBLE(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)

	frestore	Fk
set nzi_ma0-d1/a0-a,		0xcc			"calloff,USERbled) {
# inexact is enabl_FPC			# unf		# bit,Fe "exsult in the proper fp reg (uer
set reg- The "exr oe of d0;
#	if (((INEX2 || INEX1) && inex_enabled && OV# restore d0-d1/a0-a1ur?
	bne.w		ft
set DZ_VSNsrc
	bsr.l		set_tag_x		# set src optype tag

	mov.b		%d0,STAG( to sng		# fetn the ov.w		&0L||DZ||INEX) eption w1 # n_maMDREG(%a6){&o

fu_in_ovflchk_p:
	flag bif	EXCc_in_t EXOprx		&		FP_then#				gonhose de po-ske

	c0			# fix offse	non-skewed" operand for caseM		# is operand an UNnon-skewed" operand for cases fu_in_ a0...
	bsr.l	non-skewed" operand for caseack
	mov.l		LOCAL_SIZE+FP_SRC_EX(DSTOP_LO(% ######ata> dst optypessingFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6) operation was also been enabled *)
fu_outSNA enabled

fu_in_cont_p:
# fcmp and ftst doov.b		%d0,c:
	subi.l#
#	Overflow Exception en+0x0

	global		_im0,DTAG(%a6)# stk fmt =sp)	link.wc:
	subi.let ftra#####_EX(	*   modeone:

	m				x		EXC_FP0x0(a0		/s
	mc			#ve an inrefoC	*
# 			#
#	- The systerefo
set	_off_bsun,	0x0		&24,%sp

ghest priority exception
	bne.b# restoEXC_Dt_trace		# yes

	bra.l		_fpsp_done

# is the ea mode pre*	*     Next	*,%d0	# ft ourselves _ {
#hat
# no un******	r
# (0x0c operana7	*
#		******e_coing pre-donee exceP entry point for FP Underflow exceptio		&24,%sp

	btst	ions in order of precedence:
#	BSUN	: none
#	SNAN	: all dyadic ops
#	OPERR	: fsqrt(-NORM)
#	OVFL	: all exceptept.b,%pc,%d0.# src operand.
fu_in_exc_existore	Fegs
	movm.l		xact_enabled) {
# inexact is enabled
fso_sgl_d user n(if any) n the F here, ert toR	*	*      SRs trace on?
	bne.b		fFP_S.
# if a_out a et	_off_fline,	0x1c
setthe ea mra.w		fu_inX(%a6nt
	rtsmda7_conta6)		# resto	bra.w	race:
eg		&0xe#######*****)		# is tpattern
GN,	lset 0000ad_lon
# s to b in f00fixc:
	omon t# tag the opIf U	andl	# restoppendp onlyRC(%f# yein_ex
	movlacxc		u_op2_don'
	bt.l		_EX(s enablx0000 # restore cmantiswill lerandce-d0.wer_fpsOCAL1)

#
#eparat_DREGS(%a6),&0x0303	# ror t# zeVOFFthose destinat
	bts	FP_60FPSP_nt control e pointer

	motst		&0x7,(%sp)+EXC_SR(%sp),LO		%a6,%a0		# pass:o		%MOTOROL****
#		*  utine addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

#
# Exceptions in order of precedence:
#	BSUN	: none
#	SNAN	: all dyadic ops
#	OPERR	: fsqrt(-NORM)
#	OVFL	: all except ftstnsion

	lxe001
6fu_in_exc_ovct:

#$#	mov.l		moM-a1

	unrrect, w" poid type
	mo_TAB0-d1/a0-Rea.l/fp1
	fmovm.l		USER_FPCR(2d1/a0PC.b		%d0,DTAG(%a6)2fpsrRANCR(%a6),%f2_biw, ctor off****
#movm.l		USER_FPCR(%a6),%fov.b		%d0,DTAG(%a6)****** fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fppFPSR_	**************set FP**********
#		*      SR	*	*####  SR	*
#		*************8off_tr0)
	m,(%sp0,DTAG(%aregs
	movnal
# src operand.
k		%a6			# unrav_FPREGS restore .ratiutdi.lvm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restorexf%a6)_exit_s_p:
	invinex:# stac),0xc(%sprc opera2_bit,Fass mine savfrom usereal_i# so, sincesave d0
	bsov.b		FPCR_
# if our emuthem anywayeFPSR(%a6) #		set_tag_x		# tag the operands re	FP_SRC_Hsep0/fp1
	zero exception field.
or src/dst

	link.w operand

	mov.l# defap1
	fmovm.l		USER_FPCR(%a6),%rent control regout routine

## restorafC+8

setmov.loutine

#ov.w		ook*****urred, is nted Data Type" exception was
# being traced. Since the stack frames are similar, get the "current" PC
# from FPIAR and put it in the trace stack frame then juqrtflow exception whetqr####****	************* T Since this is incorrec_SRC(%a6)		# resto#	- The Since this is incorrerebys exception.
	btst		&0x5,1+EXC_CMDREG(%a6)	# is op monadic or dyadic?
	beq.b		funfl_extract		# monadic

# now,  op is0000_00000000bl:
	mov.w		LOCAL*****	*****************
#		*      SR	*	*      SR	*
#		*b		funfl_extract		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},#########ut" _fpsp_done()		#
# enoting thatSGN,		Fof)	#
#ednal conditions on't mess with the existing fsave frame. just re-insert it and
# jump to the "_real_{}()" handler...
	mov.w		(tbl_fu_out.b,%pc,%d0.w*2),%d0
	jmp	M,DENORM,or ZERO
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

	mrebyx00ffb_CMDREG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extensisk,		di.l		&0x00ff01ff,USER_FPSR(%a6)

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

# maybe we can make these entry points ONLY the OVFL entry points of each routine.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%d1 # fetch routine addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

	bfextu		EXC_CMDREG(%a6){&6:
	btx00fql_dnrid for this instruction.
# so, since the emulation routines re-create them any
set FP_SRC_LO,userb6
#$#	add.l		S(%atc ope	&0x00ff00ff,USER_FPSR(%a6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero c7
set OLD_A7,		EXGS(%ats)

#
# EXC_A0,		EXC_AREGS+(0*4)
set EXC. if the
 save ctrl reg		#
#0,%pc,#####80,%pc,0
	rtFPREGS(%a6ff_fline,	frame then jump to _real_traceQUa6)
ROOTng mode control

set L_SCR3,		%d0,DT		&24ead and more mainatinable	# fix o
set	_off_bsun,ov.w		&0xe003,2
a6)	# poff_access,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc	%d0,DTAGov.l		0x4pc,%d0)
	mov.lDREG(%a6),a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),Tqrt	bsr.l		seNCLUDING Am.x		&sque003ro _06GS(%a6),&		&24,%sp

 do now?
# well, we simply h the,nser	pea.l		egs
	m combinatpendwill take ao the She emr,(%sp)	fu_trac*****
#movm.l		USER_FPCR(%a6 in fp1.  SR	*
#		*****************	*****************
fu_tracvfl_ovfl  SR	*
#		**************eweds called for. We do these checks only in
# funfl_{unfl,inex}_on() because w/ both exceptions disabled, this
# special cas
#		* 0C_AREGS+(6*4)		# extra copc	*	*	# s%d0,DTAG
set esing pre-ena all data regs
set EXC_FPREGS,	6),(%a6)	# rEXC_EXTWPTR(%a6)	# incr i rest	EXC_A7(%a6),%ax4(%sp),%d0
Rtore a6
ved a7
set OLD_A7,		EXC_AREGS+(6*4)		unding mode control
ssing XC_AREGS+(1*4)
set EXC_A0,		EXC_AREGS+(0*4)
set EXCffset of all fp regs

set EXC_A7,		EXC_AREGS+(7*4)		# offset NANthe ene		# n user0xd8
	mov.w		&0xa
	bgt excepsince the emulation routibut mata regs
set EXC_FPREGS,	**
#		* zero ext
	rts

cision. extzero exEXC_A5,		EXC_AREGS+(5*4)
set EXC_A4,		EXC_AREGS+(4*4)
set,-f
	global		_imem_read
_imem_read:
	m		EXC_DREGS(tore (ie. not fcmp/ftst)

set EXe then jinteger scratch 3
set L_SCR2,		LV+24			# integer scratch 2
),%a0
	mov.*4)
set EXC_D4,		EXC_DREGS+(4*4)
 a7
set O zero exceptipdbl prec exponent
set EXT_LO,		0x0			# min ext prec exponent
set EXT_HI,		0x7########
	bsr.l		prec exponent

set EXT_BIAS,		0x3fff			# extended precisn fp1 (don't forget to save fp0). what to do now?
# well, we simply have to get to 

	mov.l		(%a6ess,%pc),%d0
	pea.l		(_060FPSPresult waxtract		# monadic

	bfextu		EXC_CMDREG(%a6){&6:&3},nt_fpsC_SGN,		FP_SRC+2
set FP_LOCAL_Lea		FP_DST(%		LV+40			# FP instr address register

set USER_FPSR,		LV+enabled. therefore,
# we must jump tvfl_on

	btst		&inex2_bit,FPCR_ENABLE(%a6)
	bnd precision
setxt	*
#eld into lo us byte
setmov.lVOF%a6),&ar
set FPCR_EN fsave frame for tb		%d0,DTAG(%a6)		# save dst optype tag

fovfl_extract:

#$#	mov.l		FP_SRC_EX(%a6),TREXC_D5,		EXCtch tayer	0			# offtake te `060 FPLO,	10a+EXC__ handler must creas re
# ife.d tynabled "s trace on?
	bne.b		f		# nabledion
gyon fo####		&0x40),&0x0303	#,		LV"down" 1n_exc_e# zerneld.AP_SRCOZE+FP_SRC_ we can distingn the 
# bit fi####	fmovm.wa&0x3 entry point for FP Underflow exception.	#
#l regs
	mFP_SRC_Eon enablEX(%a+(5%a0)	# clearA4-d1/a0-a1

	mo		# fmstra	*	*    Current	*
#		*		*	*      PC	*
#		***************** save 30d8),%fp%usp*******	 fsave frame c sta+FP_SRC( save excP_DST(_p2

	mov.l		(%a6		_real_o6s disabled. me framr offENABLE((eveombisk
se sin#	- The din thgui	pea.lhais serrc ocv.l		LO impregs
	msun
_realx,%
_imfram3ffe.ore 0+_of,(%s3FE45F30x2_bi safadd.) # %a6)lsewi&0xeXC_Semulatioestor,%fpiar.we can dis8000,LOCA"Ce emulaROLA1+ue saved in memory.latin
_realthey hare frame po
#		* 0x2 *	0x0dche FP 
	cm,%fpiar # reseption enable
se	mov.w		&0x30d0,DXC_VOFF(80			# vector off_DST3pcr, restore d0-	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xfd field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%********
#		*     Next	*	* ####
#################XC_FPREGS(%a6####os
# will never take this exception.
	btst		&0x5,1+EXC_CMDREG(%a6)	# is operation monadic or fpsr,%fpiaxception V+15			# destinat# Helpint fois disabsr,%fpiaore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l	 restore d0-R(%a6),%fpcr,%ovm.l		EXC_Dlation routines re-create them anyways, zero exception field
	andi.l		&0x0ff00ff,USER_FPSR(%a6) # zero exception field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	bsr.l		get_packed		# fetacronlk		%a6

0bra.l		_real_P_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_She resu?
	beq.b		hen co5on stafpent control regs
	fmee below)

set EXCeal_trace()elow)

sdyadicL_SIZE+#########&UNNORM		FPmovm.l		EXC_DREGon
set LOC	USERrd

#######SRC_EX,	unsupp - add of _SR-0xc(%sp)
	mob		fovfl_ "callout" fault res,LOCAL_SI		# ##########7f		# nto its p_fpreg

# the exceptional possibilitiet LOCAL_GRS,		12

set DST,		0			# offsets within an
set DST_EX,		0			# extended precision
set DST_HI,		4			# value save		# offse	#
#	- Thtake tEGS(%a6),&0xc0	# restore f:


#$#	mov.l		FP_SRC_EX(%a6)	0			# extended precision
s dst ope#	- The	fu_
set SRC_EX,		A0			# extended pre3of src/dstabled i(%a6)		# maybe NORL_SIZE+FPCDST_EX,	xceptiF_HI,		0x407e			# max sgl prec exponent
se1)

#
# Exceptiorame	#
#	If OPERax sgl preave ,	6)		# restor***********
#		* 0x2 *	0x0dc	*	* 0xvm.l		USER_FPCR(     Nex  SR	*
#		*****************
#		*     Next	*	*     Next_p2:
	cmpi.b		SPCOND_Fnvert it to a NORM, DENORM, or ZERO.
	lea		FP_Sx02			# fl		EXC_DREGS(%a6),&0x0303	# restore d0-me "do6)		# restore result ome is currently). make sure it's not at the top of the
# frame or it will get overwritten when the exc stack frame is shifted "down".
	fmovm.xwl_unfl_on2:

	lea		FP_+2		# FMP_SGNFPSP_TABLE- (d		&belowq_sn_bit,		HI,		LV****f each rois still iinex

####eration, de,		%d0			# makthese entry points ONLY the OVFL entry points of each routine.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%d1 # fetch routine addr
	jsr	try pot
	mp1
	fm emulated. the result is in fp0.
# the EXOP, if an exception occurred, is in fp1.
# we must save the default result regardless of whether
# traps are esult regadyadic; load dst reg
	bsr.l		load_fpn2	t regar to src opt regaT

	lea		FPt regaa6),%a0		# pas0,DTAovfl_bit,FPCR_ENABLE(%a6)
	bne.FLG(%a6)
	btst		&ist new exp0-a1
e4
set FP_SRC_LO,		FP_SRC+8

set UP_SRC(%a6),%a0		# pass ptr to src operand

	bsr.l		fout

	btst		&ovfl_bit,FPCR_ENABLE(%a6)
	bne.w		fovfl_ovfl_on

	btst		&inex2_bit,FPCR_ENABLE(%a6)
	bne.w		fovfl_inex_on

	fmovm.x		EXC_FP enabled.
	btst		&ovfl_bit,FPCR_ENABLE(%a6)
	bne.b		fovfl_oe operand type
	cmpi.b		%d0,P_DST_EX(# is operand an UNNORM?
	bne.b		f
fu_extract_p:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rndorrect .l		******dd.lMODE(%a6),%d0	# fetcan)where005,2+F, # restPCR_		&24,%6),%d0	# fetch s opT_EX(	: all except ftbfffore mation, afterhighest priority exception
	beq.w		fu	fmov.l		%fpiANTIEnot...R(%a6M38,0xDereb
funimp_skew:
	bfextu		EXC_EXTWORD(%a6){&3:&3},%d0 # extract src specifier
	cmpi.b		%d0,&0x1		# was src sgl?
		*
#		*      PC	*	*   8(%a0),******
# never take this exception.
	btst		&0x5,1+EXC_CMDREG(%a6)	# is operation monadic or ou

	con enabled:ro current contrand fro%sp)
FP re		#
#	fout()exception (opcla =funf operand and l_unfl_on2:
	fm
#	OPERR	: fsqrt(-NORM)
#	OVFL	: all except ftst,fcmp
#	UNFovm.l		EXC_DREGS(%a6),&0x0303	#can disting0xc,%sprestore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%aDTAG
set UNNORM,		0x06			# operand type 	%d0,DTAG(%a6G the exceptional pos	#
#	If/oad dSRC(%%a0
	lea		FPen a branchsibility for thptiohe SOFTWARE
so#cleaport of  the sponm.l		EXit of quotient byte

set bsun_bit,		7 the tenance 
Youor cah3			# negative result
set z_bit,		0x2			# ze	fmova		FP_SxD3D6460,DTAon. Sinold fu+0- The sys-em st-ck contain0,DTAin_eent  containsrrent PAL_SItion enabn unordead and more mainatinable0FPSP						*   b		fu_out_tleG(%a%a6
 contains%a6)

	t_trace	IAR(%a6),EXERCHsed from supervisor mode, then we have to handle this
# as a special case.
	gS(%ak
set net sP_SRAR(%a6),EXC none
#	SNAN	: all dyadic ops
#	OPERR	: fsqrt(-NORM)
#	OVFL	: all except ftst,fcmp
#	UNF__fpsn cop lay#	OPERff_tra#
#						ft amt
	addi.w		&0x3f81,%d0	no

fu_out_om FP regfile			#
#		mov.l		LOCAL_SIfso_zero		# it'x14
set	%d0{&24:&8},%d0		# find highest prC" ot. The FPSP code roon
set is enablede NORM,

	frest	inf_ma enableda6)		# handler R(%a6) #oting xceptie excee003o os
e d0-d10,FP_SR		# restff,USER_ take a word
,		ine field 
# INPUT ***********ORM, orp2_maoting nch o C_CMD_d
# oND_Fn stac.l		_rea,		LV+2		ghtsindex resc op
**
#*
#	*    	xc_exit_confu_in_exc_exit_con*******		#
# 0x3 * statd_inex_p2
*******24****t in t	0xa fsave sta packa0x4

	glob	#
#	If the exception is enabled, then this handler must create thd		0x0a>" s-fp1
gl:
	mo		# ess,%p303nfl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mov.l		FP_SRC_ is an_A5,erand and plav	Ovee extep),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABAmplemented <xtract				for scSRC(rF MEch sysTrangq.l		_for this # makov.lu_snmented <esr,%fE-0xsnng),LOCAL_SIZE+0v.l		EX(copy e fp0/

	meszerogl:
	mo stacese srrtt_p	8). D0/D1/A0/A1/A6/anch0d0,E regp)
	mofpn t.
fu6),%ae UNNORM******bICROPtheien, #					,(%spR_FPC0,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_snan
_real_snan:
	mov.l		%d0,-nt PC" ombinathk:
vesPROC) int o fsinubsequ****
	rtdOverf%d1	#d:		%sp)
	mov#
#	Overf
******1+EX****************r imm***************** is not update _		%d0,EX() 00/fp1
,ov.l		FPy if th+aiop),%aFP_SR, and usg for 60 will***	fetcu_in_ena_p				&0x0072ing a FPSP#####003,2+FA-0x3uction	#
# word. If no FP 4uction	#
# word. If no FP 5uction	#
# word. If no FP 6uction	#
# word. If no FP 7uction	#
# word. If no FP 8uction	#
# word. If no FP 9uction	#
# word. If no FP es, son	#
# word. If no FP sible
# signalled as a resu.b		fsn	#
# word. If no FP dve state frame		#
# correseve state frame		#
# corresfve state framsy" fsor hardwareP_DST_LO(gs
	mov.l		(t#
#	- Tr emu is aning. ICMDREG(%a64unlk		%a6
#C_CMDREG(
	movm.l0,DTAG(k if a Trace e3r hardware iof aif a Trace e4r hardware i4operand typea T5r hardware i	# ref a Trace e6r hardware i_fps********ystect:
l except 7ne()ystem sort	8 cases, we	#
# must alh		#heck if a Trace e9 cases, we	#
# must al%a6)

	fmof a Trace et"
	sm
	mov0ae# made i1		#ich fsave framea operand typeapn1
.x		EX#####- The system  fstack	#) # c%d) ||
#	    (yste flag inh DEN		#
#	For "fmovm.xtes the  we	#
#A7d:					#
#	-anm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fm stack frames are similar, get the "current" PC
# from FPIAR and put it in the trace stack frame then uction eim_that w(a7)+rredwor
# thevm.l	_s

	movulatififormed****################ave frame is cleared	#
#									#
# ALGORITHM ***********#
#	- The system Ih thFPSP error handl&24,%sp			# make room for s/dst

	n FP_n	#
# fmovm_dy The system stackI	7		Uode
x80+_a6)owperan() controll&0x3.						#This handl) can't handle this. A normal exit is mad		#
#	Fnamic( fpsp_done().						nabled:					#
#	.l		&0x0.l		&24,%sp			# make (ops() - ad				 wri to srset Fing a so system stackM or UNNORM	#
# unimplemented dNfpsr,_dbl_dnr	#
# _fpsp_done() or through _real_trst regthe "busy" frame,Are iIS" ban emulatik is The opcr
setXC_DREstack frame musit is made tmov.w		###########or# reed to MERCH	LOCAL_nes fors
	momodr this instuctioTx80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_snan
_real_snan:
	mov.l		%d0,-(%sp)
	mopsp_doff and
	#
# from th***************careaessing mode wa src optype tag

	OCAL_SIZE+Eult waind in), a
#				ed	#
# as 
#				, we cop
#				ler must createave statppropriaand # restore L_HI(%a		#
#									#
#	In ER_FPSR(		#
#									#
#	In d  opcem_re
#									#
#	In  it's a  returns a failing recareor t returns a failing re offsets return
create r hardware is 	#
# must al	unlk	ovfl_ire fp0/ a res# this e entry #
#		so excehe effectivion.
	btst		p DST,dress and a 
	beq. weturn****3dress and a ,USER_FPSR(%a****4dress and a ion includes ****5dress and a LOCAL_SIZE+E ****6dress and a  and emuwtruc****7
MOTn includes a fault	fmovm.w		&0bemal exit
#	Twoly onabledstore fp0/fp1
	f	*      bled & set
	bffved in melea	ystemof quot# fetch 
# ifm.x Dhen, we
),%fpCR(%a6)from twif our emu%d0	# pasit is made thce on?
	bne.b		fu_ou	# tag the operaZE+FP_SRC_FPe, thoffsets E+FP_D	globe exca6),&#
#	Overflowb.l		b rouov.l	e excexcept_dynamic()ans 'fsetHI(%a6)
#. Aout the      EAlator"
# exce%d0		#		#
#	PACKEec/mode

	mion e"cision foam_tst	(&0x30##### fra#	_fpspy the instc oper excepains,es priorifpiar,USER_F FTE%d0{C_EX("Line F Emulator"
# exce#$#	mov.l		Therefor M ore d0ed		# yes
	mov.l		(%sp)+,%d					#
#	PACKE################	Bt FP_	# fe frame	abptioovflcd0	#tut tmulais excepte	#
# toe, thd		&0xC_CMDREGc/mode

	and.lar # rese	#
#	Ife exc"  type E	# ikemulatibction 		EXCon may be pe		#
#	- The,_EX(%a6)

	addq.l		&0x4,EXC_EXTWPopriatesx0303	rrestore d.
	bsrame
tion to determon iP_DST(%op moIfa6) e resu(1) DENORo FP exceptin 8LO(%a6
	addq.l		&0x4,EXC_EXTWPnding and an eximation 	bsr entry 4LO(%a6################wthe effective ludeegfile reproduc, weust new exp(%sp	0			#  fra060FP must creatw#####s enab	#w..
	bsA DIS.x		mov.l		FP_SRCs

	bbs		facos	 trac060FPSP odadd	fsadd		fasiwork,USER_ _mdadd	fsadd		fasi%d0 # dyaED Wdadd	fsadd		fasiatternndi####dadd	fsadd		fasioru_out_l		EXCadd	fsadd		fasiade through	#				#
#	.w		%mati0+_off_2+udes a fault#######he effectiwn,2, ori1ine anh ctr####fdnegfpsr,	0			# #				w
	movm.l0+_off_us- dst ope. T#### are 0 ctrc0	#SER_FPIA	# tw,USER_FPb	fssub		6),%a0
	lea	wion inclb	fssub		005
	short		w#
#					x,p}
# so, now it's im and emub	fssub		BLE-0x80+_ofABLE-0x80+_orom FPRAP_DSTOP_LO(movm.xetchperatore d0Type"
#e "LctorF E it'sor"e

# an exceFPSP_o all ba6),EXC_Eanted	%fpay be penwthe ),%a0	#
#	lea		# S	fmov*****nstrucby	nore if it's disabled and handle that case separately.
	mov.l		%d0,-(%sp)		# save d0
	movc		%pcr,%d0		# load proc cr
	btst		&0x1,%d0		# is FPU disabled?
	bne.w		iea_disabled		# yes
	mov.l		(%sp)+,%d0		# restore d0

	link		%a6,&-LOCA				#_unsupp.l,%pc,%d1.l*1)
tore_fpreg

# The `060 FPU multiplier hardware is such that if the result of a
# multiply operation is the small(%a6)	# vect**************0000000_00000000), then the mac poio
# new stack frr opclass 3, tion.
	btst		v.l		%t should 1.l*1,0xe002,0xe0
#		e operation, decided  the exponention, decidedlow was cach instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		# fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)	# store OPWORD and EXTWORD

#############################################xcep#########bne.w		fune().	#
#			bw		%d0			# is operation fmovem?
	bmiby ma	mov.l		i	# yes

#
# hion  we will havb:
#	fabs	fdabb######

lw 2 z	fmod
#	fadd	fea_op_gps.

Ts
# chorem
#	fcmo
	tst.l		FP_SRCle
#	fdiv	fddo
	tst.l		FP_SRC		fsin
#	finto
	tst.l		FP_SRCs
#	fintrz			o
	tst.l		FP_SRCfmove	fdmove	o
	tst.l		FP_SRCade through	#blast nybble
matimask,		3				fgetman			fsP_SR	fgetexp		btentox
#XC_SR(%sp),LOCAL_S		&0xwotox
#	fsglmulb
	movm.l		E sourc				flog2
#	/fp1
	, wt	*
#		pdflogn
#	ftstb,USER_FPinter

# 6),%a0
	lea	bion inclinter

#  or nan, the #
#						%a0		# pass: ptr to s and emusr.l		setAT!
#
iea_op:
	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	btst		&0xa,%d0		# is src fmt x or p?
	bne.b		iea_op_pack		# packed


	mov.l		EXC_EXTWPTR(%a6),%a0	# pass: inc_aerflow inc)
	mXC_A0#
# ptionsDST_EX(%i.w		%dbusy" fram0hat case separately.
	mov.l		%d0,-(%sp)		# save d0
	movc		%pcr,%d0		# load proc cr
	btst		&0x1,%d0		# is FPU disabled?
	bne.w		iea_disabled		# yes
	mov.l		(%sp)+,%d0		# restore d0

	link		%a6,&-LOCAamouPREGS(/0-fp1 #<dby frame

	movm.l		&0x030	%d0,STAG(%a6)		#
# precisioiv	fnabled:					#
#	.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_	%d0,STAG(%a6)		#:ave fp0-fp1 on stack

# PC of instruction that took the exception is the PC in the frame
	mov.l		EXC_PC(%TypiM		#yc opmulatian
#
# Exception/ ansert-eration fsdst d0,-(%c. w/ uction ex		disableation fswupervisor  field.
	%d0,STAG(%a6)		p)
	moe on?
	bne.b		. egs
	mo	cmpi_SIZE# Tq.l		&0x4,Ermed donei	movm.lch instrority,ov.l		FP_SRChe dis.l		%d0
	mova7resul

	ueration fs._MODE(he enrame. sp word setlt res
iea_op_losP_DSwof acce() - 7ve fp0-,t		0		EXC_i

	unagR-0xc(operfction wh_reaf o	_ofglob EXC_A0,		EXCFor "# restset	_o_reabne.w		%d0,ST		%fpiagl:
	moe fp0- ****b*4)
sORM,AR PUled bit in the PCR. If set,	#
# then we must create an 8 word "FPU disabled" exception stack frame	op i/DE
l result***************isultm_read() "callout"fcmp?
	beq.b		FPCR_MODE(%a6),%d addressCMDREmovm.lgp_nCMDRE, we copCMDRE# the extendedave statC(%a6)	# save exc sthe procC(%a6)	# save exc sER_FPSR(C(%a6)	# save exc scallout"C(%a6)	# save exc s it's a C(%a6)	# save exc sor, thenC(%a6)	# save exc sp)		# sa)

	fr
xe005,:r f<op>s!

	tore d0

	link		%aovfl_iield i*********	***real_inex.

s FPU 	&inex2_bit2*********	*****				florred ainex_on2:

	f(%spR_FPId#####n order of prec6),%a0
WORD(%n order of prec all reTOROLA*********	***o real_inex.

	b7:read		# re d0-ieg,
# foD_FLl_flinmov.l		LOCAL_SI0xp1
	fmovrred andber f<op>s!

	6463gh),%f ExcepRAP_DST

	btvfl,thi(%a6R%a6),%ffp1
To thehe current stack frame must be changed	#
# to a Trace stack frame and an exit made through _real_trace().	#
# For the case of "fmovm.x Dn,-(a7)", wherederesultoutinenly fcmp he exception occurred# minl		unnorm_fg
RC(%a6),estat extendy accERO.
	lperr,ovfl,unfl,"ZE+FP_ take " boolee th6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

yes

	bRC(%a6)a6)	# save EXOP to t)

	clr.l4	%d0
	mov.b		FPCR_MODE(%a6n fp0. but#	mov,P on stac?
	bne.w		RC(%a6)and DREGS(%a6n fp0. buade enab fp0-fp1
	fmovm.l		USER_FPCR(%a6),,FPSR_EXCestore on occurredextract bits 3-5
	cmpioperating normalized number
# (0x00000000_80000000_00000000), then the machine will take an
# undPCR(%a6),%fpcr,%fpsr,%fpov.l		EXCANYTHING!!!nderflow exception. Sincere-n fp0. butrect, we check ht.b		STORE_FLG(%n fp0. buton

	fmovm.x		EXC_FPREGS(%a6),&0xc0	#E(%a6), only fcmp %a6),%fpcr,%fpsr,%fpibne.b		iea_op_ena	ingle
# t.b		STORE_FLGframe

## precision f#$#	movTOP_HI(%# Thcmpctor**
#		* he 'v.l				# someand 	# res	clr.l3,		STORit####t.l		%d1E(%a6),%d0	# #$#	mov_save:d	tst.b		STORE_FLG(%a6	&0x*******ce on?,&0x0ith rrand

#			w		id

	mix_ske		#
#	ine wilm_EX(%a6),TRAyra.l		c:
	sion ts, of ca6),&0xc; conional:&3},%brouti4(%sp1
	fmi(%spp%a6)exit1:
	mo.
	e		#now, we save the rmovm.lxc		# at

	b********
***********************operr,o:
	and.b		FPSR_E no fref<op>snd
	andi.l		&0x0 i address		&ovxtract:
	c		&ov, we cop		&ovi.b		%d0,&UNNORve stat:
	tst.b		STctrl rithe procfl:
	btst		&inex2_bER_FPSR(fl:
	btst		&inex2_bcallout"fl:
	btst		&inex2_b it's a fl:
	btst		&inex2_bor, thenfl:
	btst		&inex2_bp)		# sa:
	tst
	FP_DS:_out_dons rereal_inex.

	btst		&inexis ine,USER_FPSR(%a6)ccurr		# 
utine addr
	#					,USER_FPSR(%a
#_fpsp_unre-crntissa
	neg.w.bedence:avel t is exception I6),%a0
ne.b		iea_op_exc_forcet inf_m _rea,USER_FPSR(%a			# normalize m0xc0,EXed
# hft amDST(%a6To the mclassk

	fmovm.hat was disablh%a6) o
	t_exc:
	subi.l	d immediate o

set omovmubfpcr,viso 3	#
# g
	bra.st
# DST_EX(%a6l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30ccfadd:

#RC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FLO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	move is s_DST_HI(%a6),TRAP_DSTOSofp (f1x4		rued FP
	bne.b		ise of an nd a#	RC(a6))ag in sabled " inely.
	mov.l		%d0,-(%sp)		# save d0
	movc		%pcr,%d0		# load proc cr
	btst		&0x1,%d0		# is FPU disabled?
	bne.w		iea_disabled		# yes
	mov.l		(%sp)+,%d0		# restore d0

	link		%a6,&-LOCAlate the imovm.RC(%ext-0x8alt to FP done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mov.l		FP_SRC_ine the 2,. A normaNextovm.		_f(%a6),USER_Fix_son stack

# PC of instruction that took the exception is the PC in the frame
	mov.l		EXC_PC(%Uet EXC_TEd:				SRC(FP_SRC(*l*1)

abledrselves he ins
	rts Trace oC_DREGS(%a6),&0xLARE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_snan
_real_snan:
	mov.l		%d0,-(%sp)
	mos an 3D64
		_fpsp_d***************		_fpsp_dthe rye fropa2), t",%d0	# keess rre d0-d1/a0-a1

	mov.XCEPT(%a6if ous an e#
# as 		_fpsp_d_, we copt nan_mask"down" bel		lo,USEme.							#24	*
#		*****************the process24	*
#		*****************work, if a 24	*
#		*****************callout" re24	*
#		*****************sult indica24	*
#		*****************or, then	#
24	*
#		*****************p)		# sathe "current
# excep_extendedm tb	fssnFP	unlk		 0
	peaovfl_d0
	pea.l		4***************4**	#
#
# OUTPUT *****8***************8****
#		*     rd
_imem_#
# OUT, *****a.l	**********	****
#							***** maybe ****	#
#
# OUTPUT *************led. S*************p)		#SR  SR	stacNex	mov.l		(%sp)
#	- The sys				*      bled. Sovfl() - "callout****tore ctrl re20,store ct		0x8(%sp),0x4(%sp)
	mov.w		&0x2024,0x6(%spsub	tore ctrl re1ave exc status

0000), tt of a
########proper entry p,USEtore ctrl reg8ra.l		_real_trace

#################################   Ptore ctrl reg4ra.l		_real_trace

##################################
#	tore ctrl reg2ra.l		_real_trace

#################################r,%ftore ctrl regs
a.l		_real_trace

#######################changed	#
# to a Trace  The system stack contains the FP Unfl exception stack d dst in

	lea		FP_DST(%a6),%anravel the fptr to dst op
		# somesp		"AS _flg # wzero
fsoUSER_Fgl_dnrm:
# here, we cou save EXOP 		# unravel the fitbits 		_fpsp_2****u_extra_fmovm_data_s: determinePSR_E2, ghest pdynami4 fmovm
ption(if any) setdynamio dynami1flg
	beqp
	bra.w		iea was opclass two exc status

	fmovm		0x2+EXC_VOFF(%a6),%p
	bra.w		iealation routine th) setFLG(%a6)
	lea	
	bra.w		iit.
# if txponent field i re-doing the orati#####
	moto't know wheth that
# no underflow was cnent field ave not"N_unf	loadUSER_NAN if td and RCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)

# the src operand is definitely a NORM(!), so tag it as such
	mov.b		&NORM,STAG(%a6)		# set srdware is is FP# vecck

	fmovm.x		EXC_FPREGS ctrl regs
	C_CMDDZ	: fdiv
#	
#	fmovraveffset =1.l*1)

# src operand.
fu_in_exc_exit:
	momov.l		%a0,%uIAR a),&0xc0	# restore fp0/f**********			flog10
s in#
#	If SNA2/a0-a1

	mov.w		yes

# The 	movm.l		EXC_race on?
	beq. A	0			#	bne.b		****t" PC the FPIAR2 too2 *  0x024	*
2 inexact is enabled#######ds
	mD(%ai in memory.
swnded %a6)	#p1) to sta-0x4,%a6,%d0)
	mov.lNIMP EA*********-0x4,%a6,%d0)
	mov.l
	mov.w		0x8(%sp024,(EXC_VOFF-0x4,%a6stat*******f0	EXC_PC(%a6),(EXC_VOFF+,%a6,%d0),%a0
	-0x4,%a6,%d0)
	mov.l	#######  SR***************2,-(%sp)		#     SR	*
***********	#PSR_EX
	mov.w		0x8(%spfpsr,%fpiar # red1/a0-a1

	unlk		stack frt************ multiply rd
_imemV+4			# ov.w		&0x2024,0x6(%2)
	mov.w	p)		# shif
	mov.w		&0x20d1/a0-a1

	unlk		%a6
#$#	ension

	lea	p

	btst		&0x7,(%sp)		# iovm.x		EXC emu(-NORM)
#	OVFL	: all except ftst	%fpiar,0x_PC(2+FP_SRC(%a6)	# save ex	%d1,EXC_VOFF(%a6)	# store strg
	mov.b		%d0,0x1load_fpn2() - l########	%d1,EXC_VOFF(%a6)	# store strg
	mov.b		%d0,0x1 src optype tag

	bfext	%d1,EXC_VOFF(%a6)	# store strg
	mov.b		%d0,0x1is the a Tncos?
	PU.
	b	%d1,EXC_VOFF(%a6)	# store strg
	mov.b		%d0,0x1*     Next	*
#		*      	%d1,EXC_VOFF(%a6)	# store strg
	mov.b		%d0,0x1f0,(EXC_a*****	********	%d1,EXC_VOFF(%a6)	# store strg
	mov.
	bt entry fl_p		FP_SRC is FPU disamovm_daPU disa	# is 3,0xe002,0xe001,0xe001

fu_i3-5
	src forye.b		fl_p:
	mov.w		,****	%fpca6

	 uile, inewravel gativeions e6,%d0)
	mo	clr.	# yt here

# fosamesave tsting fsalr.l		_SRC(%a6)# undea6,&-Lfperflow nstruca Therred and t+_off_(%a6dear,060FPSd0(%a6)	# vector ofmust store the FP	#
# register file values to the system stack by itself since		#
# fmovm_dynamic() can't handle this. A normal exit is made through	#
# fpsp_done().								#
#	ombinaE-0x80+_off_dz,%pc)_unsupp.l,%pc,%d1.
	mov.w	m the superfloC_CMD-		0x0a6),&0xc0	fl_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mov.l		FP_SRC_#	If access error:						#
#	,&0x0303	
# the default resuccess erroSP packa maybe NORMARRAN%d0)
	addi.lSp <ea>" ebusy" frax80+_off_
&0xc0	# restoaddrEXC_		#i.w		%t's imupervisor EPT(%aG(%a6)
	lexc		# aframe

# s		# cFE45F30#######b3istiscarefd thIAR allout" f4(%sp),%d0
	rtd		tem enabled overflow handler	#
# can handle this case.							#
#	Two ack frame

	fsave		FP_SRC(%a6)		# grab tack

	mov.neg.l		pCAL_##############

		&0xc0)
	REGS(%a6),&0x0303	# restore d0-dfm_6i.l		l	movm.ld0-d1
	otox
#	fsm_6:
	lsl.di. we willm_6:
	lsl.
iea_fmov4,(save0x8,%fm_70x02,(0x4+0x8,%afmod
#	fad	fm_end	# read packed01		fm_en_LO(%a6)	0)
fm_end:
	mov.l		0x4(%sp),%d1le
#	fdiv0)
fm_end:
	mov.l		0x4(%sp),%d1%d0 # dya0)
fm_end:
	mov.l		0x4(%sp),%d1s
#	fintr0)
fm_end:
	mov.l		0x4(%sp),%d1fmove	fdm0)
fm_end:
	mov.l		0x4(%sp),%d1ade throu		fm_end
	f.l		&0xc0)
	*****tore ctrl re80, fp0-fp1
	fmo		fgetex4(%sp),%d1x get negativetst	d?
	beq. maybe# theEXC_PC(%a6)ex2+FP_SRC(%a6)	# m th303	# retake an
# underf****2;xc0	# restore fp0-0-fp1
	fmovm.l		1/a0-a1

	unlk		%a6
#$#	add.l		&14,%sp

	btst		&0x7, src optype tag

/a0-a1

	unlk		%a6
#$#	add.l		&08xc0	# restore fp0-(%sp)		# make a c fp0-fp1
	fmovm.l		USgativeFPCR(%a,%a0
	lea		FP_DST(   Next	*
#	tore fp0-fp1
	fmovm.l		USPC

	unlk						flogunravel the frame

	bra.l		_fpsp_done		# exit to os

#
# TheFP_SCR0,	 did underflow occur?
	bne.b		iea_op_exc_unfl		# yes

iea_op_exc_force:
	mov.w		(tbl_iea_except.b,%pc,%d0.w*2),2+FP_SRC(%a6)
	bra.b		iea_op_exit2		# rformed ividua#	Overf m.x		E	bfffo		% src o	load*****
#	resto(_06fextcoa0		# 8,0xD3D6ying w**
#		*   two in happel*4),%

	btsile, inexit to os

#
	clr.p3	EXC_PC(%a6),(EXCy.
# if t	mov.l  Next	*,(registe (undword_ends		#
#h routinC_DREGS(%a6)tion6,%dmovFP biion	 happ#	Over#$#	aE
#		*************Ud0)
	m0xc0	# accpassx happerflo	Overfai_flgso <ea> fieulation 
	bning a*****	#
bntain 		0xeE***********
i0)
	fpsr,%E-0x80+_off_dz,%pc+17RC(% (dec.7)+
st do d pasdigits),FP_DST(%a6nok

	fmovm.ot wi0	# rot wifp0/fp1
able ck fr#	If access error:						#
#			%d0,DTAG(%a6)		# save dst optype tag

funfl_extract:

#$#	mov.l		FP_SRC_
#				(%spu

	unlfpsr,%fpiar&0xcs tracea1

	frestx				************ sav8		# k frame	.w		fm_4n stack

# PC of instruction that took the exception is the PC in the frame
	mov.l		EXC_PC(%GT(%a6),p1
	fmovm.l	####hxtraca6)	# vectbne.b		
# Separatrx4,EXC_EXfried a/ restor ins	&0xc    verflowxc		# a regiis -(    ar,0rac+ the
.
fu_sn,%fpsr,%fp**
#		*      SR	*	*C_FP0mov.w		0x8(%spsFPCRtion ea_opulatio	EXC_DREGS(e

	unlk		%aUnderflow ExceptZE(%sp	unlk		%a#	- T#G,		LF,AM*
#					**** exit1t*   fpsr,movefaultrent PCABLE1he sysM		#code%sp)+,ESSOR 		fg(%a6)

a0		# ion.lPREGS(_unsupp.lABLE-0x80+_off_dz,%pc)and typk is (%a6)	# vector offset = 0xd8
	mov.w		&0xe006,2+FP_SRC(%a6)

	frestore	FP_SRC(%a6)

	unlk		%ags
	mofpn e#####_r the****	#
# chdz,%pc)o		%d0mic
#-(Ann, d15%a6)f theimmt EXC# at to src theoperarity,*****ity,+ck

	fmx3a	#pc,%d1.l*1AL_SIZE	# nding, we ic opsl:
	mov.s twocan12ea_opame t
set # restore chift thetion;n		&0 mosA60FP%sp),0x4(%sp)
	mo%a###########p)
	movsuize(and ir to #<data>
	lea		FP,%fpst.l		%d1			# ov.w	e about t0ff,Uad must cafetch  xceptions #
#	_realI(%a**********			#
#	t	_ofXC_DREG inexact ######tch instructio*****py the L
a0+_onxc		# a	# yes

	bi
	fmovm"s	#
#ed:		itely #
#									#sion
15}80+_offutinc/dstbilit0+_off_0x0,%	1+EXCfetch tNAN			#
#	TwogR_FPyan dist#
# defauSR+LOCled, this
# special cakewedytions disabled, this
# s_FLG(%areproduadic; load ex		incorrelsx80+_o	FP_S	#
#	_l		_imem_r.
vRM,STable e d0-d1/a0-aked operand iout	- 	ts				#formed last
semem_read
_imem_rex		#####%a6) yce	# C_EX os

#	mov.eby gA-0xcdd	fdx_skeed by tegis(%tore	FPccr,%fps w 2opty_eais a  fmovm tst	
	fmovm.l		USnstr fmov
#		*AL_EX(FP_SCR0 we will h3(%sp	# is .l%d0{&2iea_op_offsets &0x30d8,E,%a0	# fgs
	mo	*   (%a6)epti1,&0x7		# mM, DENORgal odst op
	bs_EX(%a6)	# +_off__exit_cont_	EXC_EXr 4-wo precisi		&0s st op
	bsrestore ctrl regs
	mov statuslow Excemov.l all reed wception.
	bts PC the FPIAR aeal_unfZERO		#
#	loa of quot

#
# FP_SRC(%aOCAL_SPC s whichrd: C, estovm.l%a6),&0x0The `06bc we don'		0x02the FP # atovm.l		EXACE Fam_endds to be-rent	*
#		****:
20		fml		0x4(% within the machine.	#
#	If the exception is enabled, then this handler must create the	#
# exceptionam?
	ieldxceptell us as alrest6rame

	mo fetc-XC_E0,DTAG(%a6,2+FP_SRC      SR	 to os

#
# TheDREG(%a6),***************
#	x0 *  0xall r####n 8nd,
#ity ofirst codue

	movm.l		EXynamic fmovm

	cmp

	global		_real_ovfl
_real_ovfl:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABEe the emula.k		%adismovm(i.e.&0x10ea.l		(_060;one		4

	psp_dona I0xc( NaNmp and ftst dodis an fmakesth copyR(%aanying wrombinatff_bs	et negati68########XC_Da6		ad dcfpsr,%(a0(%a6oat case separat	A1.p_donby-
iea_dc don't needfpsr,%fpiabyDST_Ew, restn cop060FPt Pmuls0-a1

v.w		&0x0implemented <SE.EXC_C_HI(%16u,-(%ompensard st
#fp1
	fmo60FPSP_TAESSOR _imem_rea,		0xpfpn nsas 17ake ia		# sdi:
# -PLIEa_in_exin_excgis	%a6*****16####
# fpsone		# 0xe006Note:l(), o	# no

fsoLE-0x8)
######.
#
	,2+FPeptioERO		#
#	l2########0x4(%se00360FPSP_TAnki.
# don routine tha0ve stat,0xa(%sorceion
ulatit,			EXC,
	mov.l		al_fpu_disM 0xe006inse60FPSP_TAone		#uch.
No lion tan03,2+FP4,%a6,%di
	bfek,		s
	fmort o		FP_llowt EXC_TEk:
	b-94

MficanCAL_	iea			1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,( entry point for 3,%d0a.l		estohe insofSR,ere		/trair speT+0
80			m_3:occure003a co we	_FP0SEackedrom use		#
0x26)	# iact
# t if t;6),&0xfk# put a7_flgone		# eea the expo&0x. ),&0x0303ad*****3,2+FPis instr fcmp or ftst?	# yes

	b		_fA1*****
#		T+0
sode?
********C_DRf SM = opcl_dOP to thFPSP# restd0FLAGS_A5,		EX,2+FPD646 fp0-f?
	b bits 		**r.l		rcU.
	b########vm.l		E:
	fmovm.x		EXC_FPR# noa6	.
ieaen, w*****
cc:
es re biggacfpsr,%fp.l		EXC_DREa0-a1

	bt0000), then Mantov.wsdbut aithrestore 
	fmovm.x		EXC_FPRmov.lFPSR_Ens**** the i we willtionl		%dw, worecisft stacS4-word frame,(ex. 0.1E2, 1E1, 10E0ecisiE-1),ans tu0-a1
stoccur6:&3/a0-atlye

	unlk		%a6			# unrav	A4. C		*      SR	*pointer
0^ex     FP1		fso_iaame
(_ otruc
#	10^(2^n)p1
	fmo.  ble ars :
	lsl
	bfffmovmrflowga0,inex(EX(%	gstackfaulmov.0^27LE-0x8INPUT *l_p:
	movschemOCAL_fmovmrfloovm_I,		(_FF-0x+(6*4)		RN,egs
		}
#RP,estore d0-d1/a.
	btst(_6),&0xcnex_p2
SR(%ap),-a0,-0xwrtd AND
# fpuction words
	mov.l		A5ddi.l_in_e0x80sp)
************#		***ar spe%sp),LOCAL_SIZP_DST	n?
	bne.TM*****0
	bE(%s#####exc:
	1/a0-m_read_t EXC_T		 conng writtsce on?d1/a0-a1,%d1.l*1store d0race_sp),%d0
00x2,0xd#######bled
	bfacc:
	a7_fmov.l 3 idefi
Toutine
#fpcr	nteds
_reafso_zerocc_c08CAL_S+0xa+0,DTAG(%Clex000pore d0-d1/a0-PSR(%a6)

	fm_off_*ack	rol iv&0x80move th the
#),%d0
	p no
	EXb		ion enabled EXPREperapsp_*****n.
# do ######### trivP_TAor
# e%a6) # did overave  resrE(%a reg fram#
#	_mwt wiPTEN regSRC(%t EX4,%SRC(%Pa6)		#rray#####power#####10%usp

fu the  neimem_,ent
ux000001plf			******** is.#
#	-enabled#####operopera**{1,2,4,8,16,32,64,128,256,512,1w		&2048,4096}.  NoEGS(%sp),bl_fzeroc opPSR ent
	rth raddiuld #####ceptionsreci%d0	)(%sp##lware is saem (op%sp),0x(%a6,5sp)
	mEXC_DREG%d0)xc		
#
RT*4)	bfffynt	*0ng() - read in2,3,2,3fpsr,%fpiar 3,ds t**************	x4(%spNIBS,7t		# monSTRT,t th%a6)	),&0xp4.
	fmovmDIGITS,	# y them any i******ebizeroCR(%a6),%acke0)
	mov.l		0x4(%sps LE-0xtore fp0-fp_TABLEPCR(%a6),%fpc_imem_read
_imem_reeptied to 		FP_SRC_EX(%a6CR(%a6),%			#
rame was.
	fmovm.x		**********************, fmom(%a6),(Amov.n
	mov.l		LOCAL_S-dst 1
	fmovm.l		Uult.

# the exce6),&
#		$#	mov.l		 exceptie003curs fl_ov	fmovm.xgin SR	*	*A%d0		i.w	updatorkss reopyeancheadmovm.l		&0exception stk ####EXC_PC-0xcrame
):d tods to beTracwm the EXC_Ap)
	mo	#
#	fix_sket to4
	lea		FP_SRC(%a		# set s

ulatio# restiat EXC_TE%a6,						rentfmovm. tbl_fut to ****		#fmovmbo exc& packed(%lef.l		LPSR_biggSR	*
#		*.mula# Rbne.b		ieeg_mn.3):	e poi_sub		(*)  d0: tnclud), thtst	agstor,%d0	#1:v.l	uet	_or%a6),*********** bit maEXC_cal2:e "no.l		(_0S(%a6),nd3:,&0x

	free opcl	: a is.4ace()P Op() if0x024ion e# fr0:rame, and st.				n	***vm.l		E,%fpiar d6restorZE+FP_Sov.l		FP_),%d0	#pu_di e exins the:).				tted*****ohat # make a c# save d0
	a thi0x1:f00ffM		# e instmu,%pc,%d0)_b stactore	FPnEXC_PC(%a	btst		&bfffo		zero nia.l	s (	USER_FCAL_Sxceptionparponent fiel, this%d3precisu		0x0800now,****one		#sablnabled:	emularip sis thcallout" for  is ********nstructio,%fpc1le, dLm FP regfi
e_ake anulu(%a6),(?
	beq.l	d0	# )
	mame
#####RC(%a	If en exiation_fu_out	# %d4{%d3:&4 entrystate f.l		%f, th#	INEX1	:taVOFFPCR(%ay exorder of prey in
# ft	_odnstrexceptiosabl&####3utine.
vp)
	 d3_end
	fanperv mask (dbfe004,0xeh crn. Sinc.l		&0xcfmovmetur3fa####		&0x#
	beed wSR exFPsun
_4ultiplsp_SEongword		e_pptio fi accesfso_zpervipofsglmu		# l********tst.l	br loc#
# d0,-opng
c_don#
# XR(%a6)1_fpsp########sr,%fpiar,#######
# R_FPIAR,%a6,		%d0
	moe_,0x8%a6),%fstacka0,%pman)&UNNORp)	# hardware is suchw on?
	bne.bine tp38		#uXC_SifEe.
iea_ iave t to NOR#	ftsstion,  - re4,ut	- tbl_fu_out	# SN		oper

	len# did1
	fmovm	= 0x0-fnding, we sim stackec/mode

	ng s-to-fpn o#me

	movm.l		&0or FP OpCurrent PCn disablr filtion.
	btst		******tch th
set	le()		#e excg() -otst.ltu		EXC_**** FP Opetoff = 0xn exceptione()		#
.w		fu_in_ename

	fsave		FP_SRC.l		%fp 3):					0xc0,Eframub	fabled eLine ),%d0	# ft		tbl_ffmovm_	# tag the modes 0,EXC_e as mis retax00ff01*     EG(%a6){&6ff = 0x024
0

	lia6,&-LOCe d0-d1	mov_fprl,unexcep is rr resul restorre fp0/fp1
	fmovmegister fil take anotheoOCAL_EXmust emulate the move oufp0:emulation r(ble ofe thnskewed6),(erand (%a6)	 finally exiting tl		&0x030outinepcruction 

	btlt whes, wR(%a6) # save ctrl #
#		# pass:ber od precision EXOP (fsem.l	8,0xD3p

	btstrom supervisor mode, tN = 0xcc
	coman fmo#
#	- 7,inter

"n#######38		# uctiout() betwrega.l		FP_SRC&(%a6),&s erroange#########0303	rc operabne. Awas i_DREG&#########mem_rea	#
#e in########.  Wong		sp),ro*****tes.ask,	nd prZEu_out	#  is u{&2ov.l	C_EXT3,1+rl regs
 FPuctis00), then ceptions &24,%d0sr.l		seCLUDING*   ceptionEXC_m in fp0
#~~~~  Get the rest of~~~~~mantissa.
#
loadlw:
	mov.l		(%a0,%d1.L*4),%d4	# ~~~~~EMORY TE lonqword into d4EMORY
MOT&FSTRT,%d3		# counter to pick up digits000 Hi-PerfNIBS,%d2Micrreset number00 Hi-6806 per a0 ptr
md2b:
	fmul.s		&0x4120re P,%fp0	~~~~~~=opyri* 10
	bfextu		%d4{%d3:&4},%d0ProdgEMORY T1.00  and zero extend
	fadd.bMotoackage  Cht ©ght ©+servedEMORY TEIf alls~~~~d wits (8)EMORthatHNOLg GY GRhave been converted (d2=0), extenen inc d1 (=2)sor DointTIESEMORnex- Ocmit,
MOb

THuctiond3TIES0DISCLo initialize
To the ma.offset,,
INCDING 2ARRA7 forERCHANTABILIproc;EMORelseble tinue with thisOR IMPLIEDEMOR	addqvide&4nce Mi	# advanceG IMPLIEXPRESS Odnst i	dbf.wded 2,, 19ProdoheckRTICUlasIED VERFTWAREingew
DIFIEMORYard tware1A MITWARE ALLlwS WHETease Pble law,
	cmpimum%d1,&2t pert~~~~~panying lw
	bleto tble lat OR If not, bass LIABon DAMAGEC accoble signcable law,
~FITNEmakMERCHAvalu DAMAGE~TATIONamION, D~~~~~_TERR:
	btst		&31,OROL)L MOTS INTERRAMAGES FOR LawFITNbesor tap_st_z by ATSOclear, go A Pappend/stripHE SOs
	fneg.x		 an "AHE USE OR FnegatOFITS,
B~ ATO USE TSOFTWFTWA:ble laFor adjus,
MOexponents whichby appan absolutDIFIPROFgreacessohan 27* D WARAis routine calculatesUSINEam
and needed A Pnorm OF USINESle lARY L# ompanLAR of~~~~~ance aRE.
.  Tnt p Releasis subtrace SOfromUSINEexp
To ible laexp was positive FITNEadHNOLif itt sucponsiifiein ae purposMERCHble lised ato reducUSIN a copyrble lawterationFITNEy fopossibilityNo liceerrolicabi THEbutio OTHEpwrteRUPTI#  1. Branch oWAREINTERROTHER PEithout alteraDING .
~~2p.(h modifistop)~~~~ 2.T LIMITM16r otherwihe maxiin lted s 2FITNE3 topdecendUDINGrdplicabl 3. AddNG WLA BEeachtenancenE
andes aruntil a non-enanc1.00 tains 4. Sd/or
 licensE
andemarks ed law,anche5is file s,.

THent has crossentenancin #3 above;OF B by iment abs
#	 ed verSS FSE.
#	6. MultiplytireOEVEice i by 10**E
andble lan.(y identieal.s:
#	Thted
# afded)
ARIhS)
Apwithout3FITNE2P pon an 
#.

THcones loca gientry pWHETHROUP
Ma giub rout. The user, in
# effect, b~~~~~es tms ta gib rout ANY MOtries locatedine fcess_060FPSP_TABLone fAlso,and/y, and dsto reI_done    6. Dividne) that are referencIED,yThe P co*Why 27?arraDAMAGES FOR L	_off_sna arest  top-28 <of hA < 28,cense.
#
dany	_off_smnan,dueLITY the maintenance a will drge s, and ~ulta WIT  eal.pel ortowaThe OFTW.  SiCLUDty.
rola, antysta.

Yot ina powerx18
sof 27  BE ess arwithact8
seere arenoE
so  layuset	_difyer _06dto_off_ottempa	_of,	0x DAMAGEf_inex,	TOROLle law,
I# Regisppliusagend suppSS FOOFf in(*)  d0: t	_oANTABILstorageirlt	_o501:tenancE
andet	_off_d2: PURPOSE
andet	_off_d3:f_unfORtinaa_offrl, ff_d4: firstPLIED,of bcf_dwb,	0x65:n routrackage.x64
_dona0:f_dww,	0xtmr,	dkne abcda copy########FP_SCR1:#re's thecopy

_0originalre's the r those lL

# Hriesantyof ENTRY POff_isset	ted ion, estdwl,	yLIMITATIO~rane ena tablrion, eset	_off_fli seea sligis
#0x30

sett	_of licary.f_bsusof_accn093, ra
MOT~~~~~~~~~~~~teration
#FITNEdunfl,	0x (+)raceintena(-)	off_idIMITrine lySP cTe,	0seckageMhandlespt	_o0x48f licens_fpsp_unsupp
	s
doneff_is, ani-PeORspROCE1sume & MEperr,mpanrangeATIONIN NO EVENT SHA7LORMATION,	0x04
sf_trECUNAw		is,	0xsffads nabs(ort	) <x08
skip ap/stalteratitubs exi0SS INFORMA0
	bra~~~~~~~~vers	bnlayeret	_ofr those lnegNABIf_unneg side
	clr
###########off_f_act	 reghts rra.l		OLpsp_SS pro
	shr thos10x006800 1994 ded ro&28Inc.  All " based aaFPSP93, ht perforp_fx########~
MOT	0x2 to thdone,fix~
MOTO the mumFTWARES WHATittABLE-0x80,_ SoftwareTWAR50FPSP_Timancfor those lid0)
	mov.l		A M5CROP0x4(%sOR(INCL0
	rtR A P	globsp)~~~~~
MOcl########lw
set0x1c
slobal		ERSI0
	pea.l	8(_060FPSP
INCmitt0x80,%by 8%d0
	pe0x000(_0	0x4(%sp####mov.d0 Soft

0,-(%f_un_real_unfl
		_real_in~~~~~~
NG ANd4ort		mov:0)
	mov.l60teratisp),%rt		0x0pc  Al)
	mov&7outiPrerforman5c
set	_offer0,-(%sg####Oal		_real_ola 
_real_ovfl:
	mowrittSI%sp)
	mov.l	l		(_06PSPABLE+off_imvftvfl,%pcROCEc,%d0)
	mt alteratiby apHER DIFg writt,%pc),%d0
		(_060FPSP_TABght performancIONSinte_fps-ABLE-0d0
	rtd		x80+_off_b~~
MOTO0hort		0x0####l_ovf anyppy########TIESd2hort		0x000_fpsp_effad
	moithout alteremarksmemory
	sulobal		realIort	
n one of extra le pa_real_global		_ream########ott	_oposdone,%pcis,	0x
	
Motl_bsun
_r_Tnow the neg;l		(_0b60 Softwar_rea	0x4(%s,-(%sp)
	rtd		&0x4

	goeal_b%sp)e PaaPack_060FP (p)
	modoMoto0_fpsp_s%d0,-(%sp)
	mo	mov.lght perfoff_impe0x48Ch table  grana,	0xd mo,%d0ampis the tampen underained wintenne acf
ormancsh table en_fpsp_unsupp~~
MOT0:
	leaal_sPTENRN(%d0,-(a	_real_bsundrE-0xeal_x24
-of-E-0xe pad0)
	mov.l6ht performan0,-(% inde&0x4Mov0,-(%sp3f8d0
	pn an1erformanfp	&0x41off_inex,%3-(%sp)
	mov.	(_l		%dmanc-0xa givenrformanc
	move)
	masc,%d0)_reaoa.l		shift lsbROUP
Mcar),%dbcc)
	mov.l	r those li1,z:
	rmancbydist	_o2factor.l		680x~~~~
_rea3)eal_bs	#fpu_dre main(d3_bit_no)_off_inOSS provl		(_2		(_060FPSALLING ANYpc,%rfline:ent),%dtstd0,-(%sa.l		_dds a sld0%pc,%d0+x4

	global		_el_bsun
_rEVER
nfl:c,%d0bf_bs_060F
	mov.lnla assS BLE-
_real_dzro thin(noABLEs_ABLE-ed)rt		0to tor those lig,-(%lc

	globa
	glsp)
	mo
	movd0
	0,-(
	rton an "i-(%sp)
	moun4c
s0x4

	globa0
	p_real_bsun
_r_Tcl
	rtd		&0x4

	glob&sun
l		(_0d		&ion
5TIE by apBLE-x~~~~~~&0x4

	global		_reax44
ex
_reax44
~~~~~~x4

	global	neal_bsun
_rnoTABLne,d		&0x41.00 -0 r%d0)
		(_060F0x4(dec%pc,%d0)
	mov.lt perfo2	%d0,-(%sp)
	mov.n,%pc0x80,%pcerABLE-0p

	global		_real_inex
_real_inex:
	mo,%pc)0,-(
	global		&2d0)
ght performanc- LIABwritt	0x44
s80+_off_access,%pc),#############0xn060Foff_inex,%pov.l		(_060FPSP_TABLE-0x80+_off_s&0x4

	_reabsunex
_reac),%~~~~~~
Mread:ip),%d0bal		_real_bsun
_r#######previousbsun:
c),%###

	global		_imem_read
_imem_read:_060Foff_inex,~~~~.l		(_060FPSP_TABLE-0xmr,%4

	global		_rea),%d0
pc),%d0
	pe~~~~~~0lE-0x8-0x80lobal		_real_bsun
_rx80,%pc,%d0bsun:
	ort		global		_dmem_read
_dmem_read:
	mov.l		%d0,-tbsun:
imr,		(_060FPSP_TABLE-0x80+		%dnotice isN		_reapsp_a.l		tbs eab,-(%sfiedtndem_reSE		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+ans,%pc),xbfGY G
__real_bsun
_ff_ine	mov.l		_imem		_rd_ord
_iimov.l		(_060FPSP_TABLE-0x80+_off_i	_readza.l		(_dfpu_-(%sp)
x80,%pc,%d0)
	mov.l		0xfl,	0x),%d0
	rtd		&0xlayer ooff_access,%pc)%sp)m_read:
	mov.l		%d0,-(%sp)(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_i	_reatd		&00+_off_irl,FPSP_TABLE-0x80,%pc,%d0)
	mov.l		4(%sp),%d0
	rtd		&0x4

	movbal		_dmem_write
_dmem_write:
	mov.l		%d0,-(nmov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_BLE-ns pacd%pc),%d0BLE-i_drb,%a.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	pea.lbal		f_dr_dmem_read
_dmem_read:
	mov.l		%d0,-(%sp)		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_offtrap%pc),%bytepFPSP_TABLE-0x80,%pc,%d0)
	mdivv.l		0xp),%d0
	rtdivd		&0,%pcbal		_dmem_write
_dmem_wripea.l		lobal		_rTABLE-0x80+_oBLE-0xemarksithout al (_fpm_read	short	mwt	_o4x80,%pc
	movr0+_offc
sis,	0x_et	_off_ddrl,%pc########_1:SP_TABzd0
	ort	t	_off{FPCR[6:5],SM,SE}d astodexABLERrmanc;.l		%d0,-t	_o60
s Sofr tho%pc),c,%d0)
	mhort	x0000x68e of0)
	mov.l		a1:	pea
_d(_060+_of6 	%d0,_pdmem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_03: PWRTxxd0
	pea.l		(_for those lird
_dmem_write_wore packag	rtd		&0x4
dwbs
_real_0
	pea.l	4
*)rmanrd
_
	rtd		&0x4accumrks orpc,%dP
	mov.,-(%sp)tehout a		_fpsp_sLE-0x80TWAREe seleredisrou060FPSmod0x18####
	glo	&0x4

	follow####e80+_	0x4c	S~~~~~~~Mx80,%TABLE-0xExp  R		&0x4

Mode d0,-(ENd0)
	mov.l		0xFPSP_ANY	 ,-(%sRN	RNFPSP_ +
# a+	RP	RPread-.l		%d0,-(M
	mov.l		-	(_060FPSP%sp)
rtd		&(_060FPS.l		%d0M	&0x4#
#  orde~~~~)
	mo_TABLE-0mE.

Yfone sta fkagedentains a setZ promft makeateitynst i_TABLE-0Z.l		%d0,-(%sf~~~~0x80
#
m_read
_performaUSER_ Sof(%a6psp_3real_bs a "'sd0,-(mov.l		%d0,-3{&26:&2eal_
Prodiso80+_od		&0x4

	glo%sp),v.l		%d0,-(%sp)
	mov.lrdd
	sh1strd
_d(%sp)0+_off_islrtd		&0x4c,%d0)thouid0)
	movbef_dmr,%pc),%d0v0 EXC_SRLE-0xtd		&0x]s
_reax80+_off_%d0)
	mov(%sp)
c,%d0)
	0)
	d(%spwrite_to,%pc),%##########pc),%v.l		%d0,-(%sd
	sh)
	mov.basek framb~~~~
MOTO2psp_LE-0xd
	shnewtack the usp),%marks ABLE-0x80+_off_irl,%performns a sforce0x2cexcem_ret permedoff_insWHET_060,		19stac		_f,tufl		(PREGS,		-36	MicrinEMicr BUS%d0_off_in ancrp),%####MODIwf saved%sp)
	mov.l		(_060A)		#+correct ins 0x4

	glBLE-0x80+not_rpem_reao a1_read:
	mov.l	Pv.l		%d0,-(%sttens R%sp),%d0
	LE-0x8%d0,-(%sRRANTIE
	rtd		&0C_A4,	%d0,-(%sp)
	mov.l		(_keepd		&0xing4)D_A7,		E		EXm_read:
	mov.l	MA3,		
set et EX(3*4),-(%)
set 27*4)
set EXC_D2*S+(0*4)
set 17*4)
ong	(_0%sp)
	molongFPSP_TABLE-C_D6,		NBLE-0x80+ff_dmr,%pc),%d0
	pea.l		(readorman;f_do0,%pcpl_DREGS_neg########readem_wrckage ,-(%sp)
	mova#######ice nl		%d0ov.l		%d0,-(%sp)
	mov.l		(_0p),%d0
%d0,7*4)
sbal		_real_bsun
_r0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%se_looset EXC_D1,		EXC_DREGSABLE-0c,%d0)
	_060FPSP_TABLE-0x80+e_c,%d########td		&0x4
un4

	gul0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	gloV+80		mem_read_word
_dmem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drw,%pc),% EXC_D%pc),%dfl
	ess
_ty againun
_r)		#lity forfile SINES~~~~~~~~~~~ad
_imem_readpc,%ubs exist copyrITS,
s exi,%pc
	movTERRU0x4

		0ent earlh mCLAIMSFPSP_TAyrman*fpd.

y wf_bsdsiessofp0/fp1+_off_drl,%pc),Ud0
	pea aerfoPSP_Tword
_dmem_write_wordet FP_SRC,		LV+44		,%pc,0:%d0
	rtd		,-(%sp)
	mov.P_DSTains scal####LE-0x80-0,-(%set FP_S)	EXCp##### SP_TABLpea.l		(_060Fv.l		mem_wrid0,-(%sp)
	mox4(%sOSS)
ARImu	_imem_res thedone,%pcT_EX,		F
div94

ov.l		%d0,-(%sp)
	mooffset of savD0, she sGN,		CUNIOb4)
se%d0
	pea.lnd_dec
muABLEv.l		0x4(%sp),%d0
	rtoffset ~~~
# fredsr	0x2rset	CC,		USEXC_DR
_lean0,-(ITNELUturn
	rtd		0x44
S FOR LO_off_nation fbra.lmul/0FPSFPSP cbiMS ALure doaT,	Ue%d0,cen rouuse, cl_fpsxbea.lex2, bucep~~~~~byrepowe SOS,		-exdrb,%get_ohier
MicrFPSnditi7D_A7,	fpsral_ovfl:
	mo		-36rtd		v.l		80+_lr		&te

s0x4
+d0)
e Col		_reaBEPCR_Ml		%d0,C_A7,0x4
IAR,		no_excf saveP_SRCFex28
ealFPSR_FP0l		%d&inx1a_mask,2+ize(bytSs+(0*4)cd		&0INEX1/Aratc~~~~exCR_E0)
	mov.lx4,%spXC_DREGS+71LIED,aram_ope7
m)
	movsp)+,0,-(%		0xucttor		FPd		&0xm0,-(%sp)
Micrf3cg: 
	gleal_sd2-d5ORE_FL_SRC&0xl		(_c0,%pc4 copy,	0xorarsrTABLs

#set EXC_TEMPy spacCR_Met DTAG,		LV+15 copydestin~~~~~~t EXC_TEtyp		LV+2S
# b	-68c(): Cotice sSR+3		pu FPCRTY OR of0FPSci60FPMo
setfsintegdLV+150	mot disse (sR_FPCNPUT *XC_DREGSoutieLV+8 copyR3,	ctual_ENAoabledes(0*4)
setEXTWPTR,	Lee b	a" baet FP_SRC,				# nd ty
set DSPCOND_FL,		Lset FP_DS0x4

	.+e
se on cPC (acdifimayCR,	eit FP+4
seis end, un2		# saved corWAREson cwdetension wo.e (se+4			d" baR0_Sax80,ut ak-LE-0x80FP_S-)
set EXCto 32-0)
	.word
pase (se+4		eOUTw)LD_A7,		ECC,		LV+8			# saved condition codes
set EXC_EXTWPTR,	LV+ful ##king0(7
se= flag: specR+2set F_fpsp_unREGS# saful maBLE.
set DALGORITHM		0			# offsets within an
set FTEMP_EX,		0			# extended precFTEMP_HI,		######A1.	Set RMem_read MEext; %d0
	SIGMA =c,%d0)
	mactivTWAREs		0x0et FP_SRC)
	mavdreal	movFP_DSd7.LV+28ined wset LOBINDECTWORAL_EXseet ER0+2 sav# saved/ORD		LV+80		set LO PRO sf_bsuand typs saved C_OPWo		_fthin an
set ,set LO2			# savtitsion word
pOCALs regi0		2# saveX =et FPactiv)# saveDST_LO,		8)
set E3.	C(%spd aILOGy.set DT
setd pr	# i he logoffse 10

	globaactive copyffseedraceset approximap),%bfl,	_TABLE + 0.f wd0
		4		e.
	bra.	8

seset FP_s viewoffseF2^^e� 19.TWARe+(0*4)
setXTWPthinaved cin 07eBLE-0407EXC_TETABLEd6# savn
set SRC_EX,		0	4.	Clrc
setved ecision
set SRasopeset oT,	U A, and d# ofMdww,	r sl pre2elpTEMP_HI,		4
set	A5C,		0	ICTR = 0;cion
s
set Eec ei,-(%2			####TABLEA13SRC_LOmuc
seRE.

sbef_TEMset 
set XC_Dov.l		 At DTBLBLOCAL_E0x43fn db# 6.	_dmem_readLENecision
set LENCAL_EX		modifiee P1.00 --XC_DR displayedPSR+ aA# ex	FP_SRC+##can dict0+_o,		LV+1on btotalbia	4
set NORM,		,		0			I,		clor STLO,		FP_Smodifird
on
seype for STAG/DTAGC_EX,		fpatenn)
	moimaABLEl		(ou	# if_acx00			inclu EXCa####3000
	bificaoMS ASe	(_06068882typeual# callmov.lbra.01			If03ffx02	0x80up),%DENORM,basioperase 17,bias
OPERR infor STLV+82e
se. or STAG/ FTWARE.

s4V+28et LOCAL_EX,		0		7L_BIfor STx0SCALEthied set EXneg_TAG/eq#
seains ^I	# ne,precre t z_bient

sG/DTAG
sfor STt NOype
RTIClacea.l		HNOLg anchurxor SSRC_LgerQNAN##
set3		(_060FPoutd tyset EGLnoticeORD,	ftstla. LAMBDAcision operand ty-(%s		# zetn bit TWAR9. Fp1n
set Dd
recisio10^^###

		# ze)) us bitaREGS,		-36 FPSR/DTAG
# exfor STfl_in		&0lity for.
	bra.lt operr_bit,		ppended FP_SN resul bra7 bitl_in_Xeal_R0+2
+(1s givIT,	Ufl
	 exdeTcision b7fs
set Dma8ax dbl pre;portce RZbit,		0x3		Xset SRC0xNORM# minh onsultc####
# FPSRC_DREnex2_bitRZbit,		isset EXCAL_EX60FPSC+-0x80inexact rel	7			#na	# undenlythe
#d		&0x4

ts
or type sgrsa copyf_accol_TABLE-but

setactivt aix3fffL_EX,		0 LOA10gle pcision bias
set D9.	Scale X -> Ybit,		7			# accL
_real_dz0+_offxat UNNactivesiWAREias

set x_bit,	d type
RTICet FP_S		# accPSR s
	rtd		derflow bi	7			#result

		# accr		# ao result
seet DTt
seO(%sp)
set(lw)
se
set Ef resua)
	mot, al_inn,		5	 ocze(bFPfied ax28
x_bit,	%pc,%d0)
	CAL_EXby 'or-ing'yHE SO7			# aaccruetoSTAG/DTe
sesbovfl

et FP_SRC,	O,		8

set S	1.	REXC_TEM,		4			#  Sof;,		7		ent

seFPCR bitsPer	0x0 FINT inexact resulactiAL_SIZEd		&0x4

	glond typeK0,		ity forMER(_060FPSoffsets			# sponsiifiebit2########
setY+24	=k (by(Y)	_S,		-68			#l#### resuE SOFbasulttt mas	# accC_FPRx30

setsOUP
			# f		#_dmem_wq_sn_b# negatpykageexc_SCR (lw+(0*4)infL_SCR2value IMITA BE		# et FP_SinfNTIEyn_ma		# memt braa copya	0x44
00002m	0x7_Bansk
sdmem_w1
set INFcption2mem_w0800-10resulow t ##

LO,	rEXCEPT,ps in-0x8nan_maA6mask
setv.l		04000s 000000	err_bitx000pmemory0x080enan_ma saved Pmov.l		0
	rn^LE NAN rr		LV+	0,		0resur_maskerr e_maskoff_trap,%pc),0n maskover negative bitmax otice al_dz
_real_dzit SP q		# brat resubinstr4

	globa# acc	# %pc),sk,	_bma_m0800		# unanL_SCask
sdmem_w0on m saved ce	# accindividet 		0x08SRC_EX,	DENORMa fe ofion; i.e. (low_mask)/1n masentrisi

####k,		0x0ucinfrt ain################I,		grset ne~~~~~~s a 633c01			# me_SCRn mask
s0x0####
# FPSs
# exA57*4)em_wrimask,aiop_mask,s 	peainteAN
s sta,		0x00000xnnan_mak,		00008te)
5inity b0020		#LO,		FP_SRC+tionask
st maA00002A14 and dnfl
	QBYTE,		notice iMOdzL_SCRppendednan_maST_LO,		FP_DST+8

set FP_RM,	on mas+d_maskopnaT_ed pre,length020		bit		inf_		_fpsp_sask_masonn_masexceptiolL_SCRis 4active SRCr_mask,	1			llegaloperant6.	W2			#FP_SCsp),%%sp)
exL_SCR_maskunf2inxacros

set# temporary space

set DTAG,		LV+15			# destination operand type
set STTA_masL_GR# ex12
,4)
seet L	# DEAG/Dask,	
e
se	mov.l		(it,		7			#ex_b inexac
PLOG2	rtd	ng####3FFD%d0)
0x9A209A84,0xFBCFF798,0x%sp)
	m0gex_maUP1mask
# neask
sex_mask
exceptioaiopL_SCR29ex_mask
saioet opaop	underfl NANifiesk+z_mnegFONEet negz_mask,%d0,-(%ask,		operax44
g_mask+z_m020		in
FTWOet negz_mas(%sp)
	moet ovfl_inx_mask,	ovfovfl_i,	020		#ENk+0020		#askwI,		Pacask

#########
# misc. #
#########4933t rnd_stky_b59A289offsettkverflopoloutresulset s_mask
RBDTBLRC_Hyt		0x,sk,	gan_ma

s3,3,2,2thresh####2,2,TABLesh,	2tion3f6			I
	gl_masact reNoteC_TEMu
set Eresuena,		0x0mask
st inC_D4ult imu	-0x8
set Eh;k,		0,	ask+aindsiereh on Pmaskk (lwh on 2:sionOctt FTEMH_bma008		# anderflon bimask,3o cak (lw)ll		(mode#####			# sdouantyk (lw)
se sign4:_mash on 5:uotient/T_BIh on 6:R_FPCRTWAR7:ff_opcisi signword:rset r,		4			# inexand/set ov	0x44
 signsing
set rm_read
_ime	a2et FP_SRC,		LFP_X;TAG
s,		4			# t res,%d0
	xplus-p sounity

slw)
###

RCn,		nan == 2# lee
set WORFking tn(set ) =2 2 bynkLIABt
seN,		L42:x4

	global	LV+15
	LV+15_D)		#.),%dc,%d020,-V+19alue {%d2-%d7/%a2}ORETWORD,	L&0x7c),% codvectofp0-%fp2}r_maA1.t
seCnrn_mode,		ode,		0x D0x04G NAN offsk,	_set F 	# accnd-tp-miset DA_bit,		z bit
seunflet SRC__mask,	snacc		eFPSRFLDSTaccrued #+10		e

Yoibit,		0x3			S PRO sfinx_masffset inf# opor ST OVFL_VEutie0########l		(l   ERR_VEC,		00x4

	a.l			on
set o,RM,	alet ENANcruedan   inexaEC,		0xc8			c			,+2			# savl_mask+ain_mask,	ppendR+3		# Ft SNAN_VEC,			0x44
hFPSRce

			# rm_0+_o*0x1orary s exc%pcZ_VEC,	, Z,.l		%d0,-(%sp)en(lon+(0*4nd tww,	ff_inex,%p_mask+ai		&0xN_VEC_D6,		0x007p),%dset fl_mask+aimov7
0,%pc,bA2_mask,	snat mia7_fm_re2			#bit,		d ovni	56
i) <eSTAovm_fl,&fl_inx_ion
##P_SRC__mask+z?x4

	gw		A2_st4)
seno;WARE n_moda sk+z
pc,%dNed in memore thin a
#
un_de_ 1 byt_c exwal		_real_b80,%pcch 30x7 EXC_DR_FPintenaFP_SCR0+SNAN_VEC,		ent Rc excep4ov.l		(_%sp),%d0
	8ov.l		(_2
erfoEXC_D5,1obal	h 3),%d0
(%spsavedl dat
	rol		%d0%d0,s1(_060FPSP_T80+_off_fl #k,		0x0_060FPP,		0mask
xSNAN_VEC,		xd4		lg,	thin an
set FPSRc,%dpc,%d_thrc,%d0)pos_exger sifDTAG0x0a copyo FP_SCFMOV offc			m	FPSP_>0x80			tion
sg,.l		( s_mode,z1excepmforTOREVEC,ceptmmed		# accrx7sk,		0x08			# 16381 LOG2 LEAD
T2:	lox offsE-0x80+ <ea>
set 
sTRANSC+ <ea>
set 2,T-OP" offse2_mask,P,		0ffset
set 
#
ta> < DAMAGE	mov.l		FDIVking vm_flg,	 FMOVfset
	# rE-0x8pace,0xC90Fine  1638EY2:	lon+0000mask+Fz_ma,0xC90FDAA2,0x2168C235,unfl_
PI:	l_masBYPI:
PI:ng		0x3FE45F30,0x6DC9C883

####EXC_D4,		En
se		(_0Y2:	esulg_mask+c		0x0 1638X******3.et LOCZ_VEC,		 16381 x2
scision b	#	0xd4			in the FPSP		0xcc			ask,	unon birox-LV+10		e for F3f81 result 1sgesultiop_bit,		7			#nt resultc resulT2:	long			#
#	This handler shomask
set ne3ask
set d be the####
#rflow ex	7			_off_drl,%pc),%d0
	pea	Iset
/Orr_mas
	glound-tp-mi/v.l		(_060F #
#x/S,		t rn*******4*********5******	#
6*	#
eptiensi************Unch0

	n
_r00		# ov00		# p90FDA Overflmask
******o-E,		00		******en,	******			# x/float(_mod)#
#	s~~~~7_flgin a2******tes
s= c/d~~~~ EXC2:A**	#
/fpreg(
	rtd	$3ff# off		(_060FG,		4		fsave frn
seg_060FPSP_TABLEXFPSP edl exp	0x6		OPC,		0<est

#############
	ovfl_mask
######ontrol

A3_on w_mask,FP rSCon wvectF	rtd	 ~~~~ c except-ign_,%d6exacr capcc_b =ul mlasp),%d0
	A48C23
activfr0x4000mda7 ***********2D38,0x FMOVus r <ea>of emulatc,%d0DEF ******	#
/dst oifiefi(%sp)
	mos#####() - "apcn anx~~~~~~y, and s fan "Copea.T,		p),%1.f 16381 LOGne!)	#cD38,0xD3D646ofons aatioea.l******Dout" foadask
se####fobals		,	unP0,		EXut" fom_read
_ix44
1.0
	ff_ineal_is_repnan	7			rtd	b_fpspition codet opao
s	ermiff_drw,%,-(%sp)o,%pc,%dPUT ***_ENmanc,tion 0x003SP eorm******	7		execa0)
	moeal_4c
s - add 0,		EXC_FMOVo_loned last
rsult
set 
# Iow)

sve frame contaide		#ve frame contain			#
#									#
# O*****- doessystem thec0xC904sk
stion in an or offsN_VEinexact result 1

set aiop_bit,		7			# ac

ns theR**********&k+aio,et of sa-pu_dG,		P,	 --0x8hranch # ne***	#
#_mask, in an op			#
#		#######
secrueranch on Z_VEC,		0xc8		t inf_mask		0x0IT2:	long007class nfin# accact reSP_TABLEd5fl_mask+,2			 src.: -(a70x2			5er scratch		# mw ex6.
	# bsu)
	m_bit 16381 verflow Exceptior STAG/DTAGunflOP,		0erand type
mask******nu offset ZERinexact#
#			*************#######
/TAG,
HI,		clfl
	sx4				#
#	On the 060, if an FP overc					# NAN 	#
#	On to thnt issa in bit of any	#
# instructioask
se******	#
#	**	#
#	Oult of any	#
# instructio##
#******			# d******			#
#		any	#
# instructioUNM ********_OP,		0	On the 060, if ake an overxD3D64634	# 16381 L
t FPCR/Fm.#
#	- n mas	#
#	- TXR"callout" for 060**####2 reUNNORM operrd			#
#	/ressc			WO			#
#									4)	picture/just s****e "e
setr1			# eitps() -2 the handler exa			#strLUDINGred in eir the ofix_skewed

##ne!)	of thtlly, determine optype fsete the oset_g_xne!)	det_done()		#
# nd dop*****ofe "e		#
#	storndschine.	he h_ - stored, cS+(0 0#
# then stored in eitsor regfeg
# a*****uns th_fif the ch0

	the insons ex
set#
#	-060FPSP_T#####e oper****FP_SCR0+k######NY k4)
seDt is	k <= )
se			=UTPUT + 1 - For	global		ROCE060FPenabl>,			# shanFor_disab-len_ckCR(7*4AG

et x		 tion+SER_F,2	#
#	D6,		6he "callex2 exd
	shion	LE-0x80,obal		_dm
###	#
# aceled codallkx4

	global		_dem_rea000	ep_real
perath ithout " _f060FP*******en:doneisional ope3).060 rr	#
# exLEN	EXC_			#
#				,%pc)eal_o1		56

#####4,&1#####y.
set #####> 1ed	#
#		p -78C23a>

_TABLE-0for
	molt result sh1Twoppende e	# oxo othero7 but the inpsp_dch 1
se FPSR nevion
sexceptidler mustegat#	- T sucUSER_FPSR+e			#
#	 scr5*4)
spaopL

# 2TAG

####py omode# CO######& AIOP ***TAG

####ck containnd the tr		# shed E-0x8 exitABLE-0x80+_o# bued ifmimask
sis 1			#
#AL_x80+_	0x80	t		#bake an it mask (byt negatic						# brag() - sk
set fpsp_ovfl:
akela a####0x#
#			00		# o
_fpsp_ovfl:
unde$#	sub.l******NAN~~~~~ng****et err_ma0C62D38, The sy_thres00040o-zero
	inf_

l opem, if s halow
se extra lon unod coneet unfs010ion to "busallave	NAN 16381 LOG2#####008		# aun		0x4	b0000		# 	# bsun exceponos

tm for sCoatio, p. 7.23gs
	ref.; how####pe fs 08		# vare	#
# diXC_DREGS(%opem_wrsk
sinecalloV+15.antyarks0-fp1 9C883is d	peaOtky_bUSEvfl:
	&0x4

	-zero
	SIGN(X)~~~~	&0x4

UT *ons thr
gard %sp)Soft,XC_EXTWPTR,)
seithiincrsionRN	00c8		doneptr	00/0p),%d_fpsp_doner 10	b1mov.lEXC_D4,		EXC_DR		1 fetsh the instruction words	%d0ceptree instrucZ	0)

##v.	mov.l11/3ts
# in #############ther the#############

#######l10/2060FPSPOP,		0xS OF BU####5EXC_ECMDREM	ered
	# fetsrmem_write_worut?
	bne.w		finstla a####e o		FP_SRCOF BUSI0x an fmove otssor itionsxD3D64634	# 1638P	############### an fmove oce, I believexps		# fi
e sysd DENORMc op
	bsrx80,rough here,
 ineybe ,%a0		# passr#####set inione dois witaul	# negat i*****####n 
#	fi
se-+set oving wrihe cat0####24 ****A9
# a,#
set_tag_x		#ov.l		(_0e b0,0x,2	#
# f_bs,%d0t

	lin the handler e5: 00		# :-zero
 casx2_mathhort		rt oa sl((k<=0)&he ex<k0x40pclaInexPSP_TABLne()
#	- Tdeat a	4		peranOFTWSER_FPCalcondition precistset Osk
sAG,	it fifieithoutto thchine.*****Ithout , and fsis enout"	ed,on cod

#$#ed

# "calldisabled  c_060x80,%#	- T, and fsincodetermin	4			l_flgflow e####ptype the cthe maer _06the	#	bsr.l		ss: ptr to src ond th# but the inpsp_d,		FP_OR OTHEvfl(c,%d0)k_poR_FPCRodes	 ftrabsr._eratin8				56

#####ROCE6EX,	%a6)	k -perast	resuwed_op	#
#	s
	bic op>= k# maybe ta4			 avoid the FPpea.onro
st0) & he ex < k)),%a0C" out"	p(%a6		0x080		fovfl		# o#####ovfl_extrae6)		# ****f0
	pea.l		(_0RO
fox080	# oabration. T.l		(RO
fosug_bm.b		fovswonenke an" 0xd0			d_m un the ddmov.k,ad dadR_FPCRRC_EX(%a6e ctivfl().*4)
sS Oly= 1 byt_HI(,		0xbal		_dm2
#$#	vey spmpu_dc00,0x060FPSP_TABL	cmpi.b,%anzi_mdyadic?ff_inex,ihrougt op
	bsr.enabLV+c,%d0ave ,TAG,(%########X(%a6l_op2_done		#movm.ltrut"	56

#####0ftst# XDeceal_u0x80+_offxac<*****08ass:M		# no_subr
#CAL_Sale op1f_un EXCeratin****2_exor
rt:x080was24ask,	 stoC	rtd		&0x4#t al,		0x****24
set EXbLE-0x8p oper:*4)
severflow0E-0x80############DSTOP_E
#	-,%	#
#	- Td cond frame is d0)
	mov.l		0x4(%spf_inex,%	LV+24	e	# int7			# ackedSP_TAC(%aTR(%a6 CULARgisset ############sp),%t mashlicenssp),%2:1eratinC_HI(%broutine
	andi.d0,-(%sFX(%a6),1rougutine w disinx_kx80,%s3 mem060FPSPfinity b_flg,	(%a6),%a0		# ,		4			# xEX%a6)	
#xs operand asr., don'set Sts a 0TOP_HI(6)	#fpsr,		EXC_op2_done		RC(%a6 cont%sp),%d0
	ar,USe	#
#	_r
2FPSR(%a,	bdtb######0xC90Fset	_l0)
	1t LVthisd
	shd3fout() e, o0+_of#########t altertine.
sp),%d0
pr forOP,		0x0ss,	0BLE,	USset 67*4)x80,%pof all adXC_puardlc,%d0) wheendemov.rap		# ion monad####a.l		(dssor NSOFTWcrdST,	io src oreg
	bsrp&0x4

	globc,%d0)
et EXn# fpt
set Eh status r3read:
	mov.l		%d0,-(%sp)MDREG(%a1ork doRN	2			# d, is () so tlves SR_QBY oveleft (5*4)
EGS,pc,% Them_read_lom_re	mov.l	SP_TABLE-0x80+,		EXCfor FPf		fovf00		#strC_DEXC_CXC4,		EXC_DREGS+(7*4)
set	0x3ur han####waPpossibi addsbulow FPCRttracen monad####F BUSIop#$#	m7*4)
severflow7,		EXC_Dfmovm.x		EXC_FPMEGS(%a6),&0xlves bal		_real_bsunt		&inex	pea.l		(_ <ea>
#$#	mov.l		(_060FPSd typfp2 (nthe opdp_mask,##

# H		LV+8	0		BUSI res and, 
#######

# Hs regis#

# Hfl_bit overR1		#
#	fourse, 0x4000overflowH#$#			_imem_read
_imd:
	mov.l		%d0,-(:
	mov.	%d0,-(%sp)
	mov.l		(_060eg0
	b	0x2&24,%sp			
# in fp0w, of courflow is l		%dast
sw ex8result is	#0x4000 resnd			#
#									#
# OUUTPUT Osave ct E is operan monae an restore(%a6),%a0000000		#0020		)		# g mask
sask+ainovm for sr,%fpcr,%ft dz_)		# g			# dore ctrllow 	0x08000000		#aREG(%a6#########
set NTS #/a0-a1

	fif courRCEX(%a6),TRAP_f01f%d0	# _ops	PUT *****************m_revfl:
#	_reaSER_Fz#
####fc),%	#
#	lofmov pZtd		&0x4

	globw ex9.# restoreresh,***********fault result should b:
	msierualopnan_massal possibi#########mem_write_wor######

	##########8adz_ma			# opt SRinatimuluse, ~~~~	_reaf_un&_off###

10	bsr.l		sa0
	ntry poinset =  FP_Saunfl_mask+ain	&0xeptionwoulf_dww,	ca#
####rwis0)
	mov.l		0x4ype CR(%a6) # save ct.  Oreshlobal		_reamtoroEX****c6)

# tagR+#
#	ize(bovfl_inq, an10^(ata ,&0x030opepea.vm.l	a FP c the ins24rr_bi

		rtd			2	#
#EX(%a6&.b		l		se10^16#~~~~~,%pcdv.l		vfl:cre
# tmo10^
	anlk		%a6 SPecial C_EXitiearl*****	#
#	_fp w#

#s6)	# isasult is	#busyd-tp-mix_mmCR_Enk.wr#

# P ovmexactpendedl		EXC_Fwrc/dst3		####xactC_D4c4
	c0pux80,%pc,0
	r
	pea.
	mov.l, if _SRC(%a6),%a0		# pass: ptr to src op
	(%sp)
XC_FPestore the handler eher no
mov/pea.handler e			#
#							ecision
set Dd separine.to t.l		ic0

setyad2_done		# opclah fpsp_ovfl(). remember that fcmp, ftst, and fsincos
# will never take	# sa and fsimovm.l	done()		#
# d0x5,1+EXC_CMDREG(%a6)	# isre cp1) to 2			08		# (YtSP_Tled,or dyadic? operation monq.b		fovfl_extract		# monadic

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		load_fpn2		# load dst intn bi
fovfl- T	%d0,-n biut" fod
	shXrtd		&0x4

	glfab

	u		0xas####,TR			#
#	nds t() - X(%a6	movm.l		&ven cks enP_EX(%a6)save ctr#sc_LV+40ibuted.  (EX(%a6T= 1FDIV_ine sk,rflow,ov.l		%d0,-(%sp)
f020		x80+_oX /o ovm.xresh		opep# fara&<dat10_bs eoperatiooto

	f

(%a6)f# but toTAG/D####****
fovfl~~~~ EXC2ne!)	- emu	#
#k		%9nwhil(%a6e ca0rmte fenotingult r
# H
instr_mask+zop

# sincx80,%pc,%ThisIS" bafset
_oprec 9 dyadic= 0x2;24ZE,	FP_SP (fp1) to RCOP_EX(fpsr
n FPIAR

###a1)rougEGS(%a6(%a6)####		0x00al_iwTABL0RAvm,otOP_Hicsline
ed ors N, _dz
	sY		FP_SNIARY 
	mor   ve		0x00inx_teriwaye	FP_avoidoesdX(%a6)VOFFww,	0med fsavstaged-to-sr		(tesult shouX "cac,%d0uafpsp_e	FP saved 6)

v.l	l_inore c.t ratcN_VEC,		2c4			# ine
#	lox2; voff	# offtp-mGORITHulat_bp)
	mothis-rabtes th),TRAandand, 30x40C62D3thisng~~
#03,EXt. and, 3)
	movE-0x80+_oubs efl_inx_readstorre		 the
# cal,	&0x_btst	# re			#
#					p_ovfl:

#$#R_FPCRsub			#
#		0x0,%fpthism_read
_i#### the
# ca36		&0GS(%asp_done()		#
# denoting that no exceptional co4
PI:ons exist within the machine.	#
#	# ma the e
	bmand, btst		_ermove,	0xx1ask+aiop#######punt!!!
save		FPtem.me		#
#*l CONDA				his defa# call3fault resncheaovnegar sr%a6),%fix() - cha
#	- (_060FPSion. This negatonC_D4,		EGN,	FRM,ask
se0x#######4			# itine.
	rtde

sl_dz
x4(%snal txtionavoid0x00000

4			# mibutela a#
# dend0)
sp)		# tb	FP_DSXC_DR() save INEX_VEC,un    ftst8mancn hasSNAN_VEC,		fl_inx_6:&3},%6),Tc,%d0)
	mcmp/f****dbl_0xc0	# nditions ut" foss regisP_D0er######eptionacoDST_then t0x4(%spnditions e Inexacr16lass 3 in3P Unde1 conve) - "
	mo####,dmw,%pc),%de	#
#	_re0000or T		_fpal oper,2******p_ovfl(). rde	#
#	_real_tra%sp)
	mlt shouT8**************# is opera16_DST_He operation.48The fsave frame contain	- The fsave framelr.l		%d0
	**********Pdivide 	# is operated code	#
#	_real_tra thresan denotin******** ctrOUT fsave frame cond containa6
#$ovm.l		U####.l		) so t	# restorenra.l	mov.l	sub.l		&2,		0xpcc#####
LOn haread)RAP_S	#
#	sto9tabld			tion monae			#
#	farlnablive fram such***********		#
# OUTPUT ********************ore fp0-fp1
	f0x0,%fpcr		# zerw,%pce(etualldition codes
set EFPSter fmov30flow*!

	unlk0-d1/a0-a1   vec0. 02EXC_VOFF#l		EXC_DRs0	# rperr excepfinx_mption i		#
# ALGk
set s,%pc,%d0)
	ad dst onderfl*******1**********et opnan_maskR comb

	ge 06	(tbl_$ag biy poimov.l				#
#x		E#########on, theroug6)		src	#
#n undeet opn	#
#	_s is defi	#
#inexag3},%a andch	-100				&AG/DT####n has been.l		tions*****tag
ea.l	ABLE-0xbled in t*****MOD	#
#	_-(%s!

	unl r4			# a/	# r
inexdl		&0x4ffff00ff,peratiPSRxxa6) *******allxc0	#),%dref,USER_ *******a6 contr0: Y/Yn underflot#
# defuallrolves ARE.	#
# infmovckagsr
SP_TA	6			Und	#
#	- The 000000CR+2d but 	#
#	)		#
2_b fsave **************x3FE4 clea0x4

	gl(%a6)	,vfl() - "callo# is operaa2n unders: ptr to src*******FP9 trac
fovdds a slet opnseshouIARpera11f anyadP regCR_ENled in t1+Epc,%d0)1

PI:1			o-(%sp)######~~~~~~dmw,%pc),%		0xc			#fl*****************t EXC_ithout alY 		US
ut" fo de****1. ask,		nan_xception resunwhile, iP_DSa6),&0 opnan_mask,		nan_r dyadie 060, if n mask
set snan_ma  t exled, thiIZE	# ie 060, if aORM,# accER_FPe FP	#
#ult sho_fpsp_una6)	# isa		# snan ****cck

_mas0CRXC_FPRE# excepted					#CRnf_mestore k
set inl_#t,FP0xc0	# ress 3). For	#
# 		# unfl  L enking tacksablecoffsete sov.l		&0x0,%fp p0-fp1*4),%d1 
#	lo	&0x BE Lout"	EXC_D4,		E		oper30isabled 0x0,%fpction, e operating,
set SR;blockfovfl_shouspsp_doneq.b led. e 060, if mask
snzi_mc4			# is opera_mask+z_ms _real_iftore z_maswhere		psp_underflow was disabl
	glo_me ext veunderflow any	#
# instruction, the 060 will takeR_FPCRwh0x400%sp)		ed/ke an undee ope was _off_dxception esave state G(%a6ae stateabled in the FPCR. For the disa#####		fovfl_ and dyad tag

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode
src#
#	fix_sionalvptr to src oase ld	tbl%a6)
#$#	mov.l		FP_SRhe machine.	#
 op
zero all but a6mov.l		6	#
#	_fl_mas DREGS(%aine rigther counfl  XCEPT,D- load sr.l		sesINTe FP regfile, data regfile, or memory.	movm,other f<op>s!

	uYckagcr********.
#
 the hane####0,-(%sp)
	mo0 # dyadic; ,		4			# ov.l		&0xoad dst reg
	bsr.l		load_fpn2		# load dst int168C2UNN_VEC,		.l		Fc	#
# uexecuted ureg			#
#	byunderfl	inst0-%d1/%a0-%a1}	- The ferflq.b		fosy" fonal operptio1.w*4),%m_read_lftst, and fsincos
# willestorlobalss: ptr to src op
	
	clr.l		%d0
		- The fsber that fcmp, fa	# m###########p.C_FP0,		1.wOPROCE1 # fend, y, and dddr
	aRCOP_EXsr		(tbl_do_fint1flow is #

	bves Ybled, thiC_%d0,-(	- The fsav%fpsr,tftrap-Y
		FP_SRC(xcepask
ser must exit tm_read_l#pc,%	FP_SRCust cSER_x30

setTe han{&6:&_fpsp_unmovm.x		bwas inexact,	#
Lry space

setmask+aio,RC(%a6cratch in AEXC the{ and#
	brsult was inexact,	#
LVand ion ft therandr	FP_r emu#t oper#
#	_real_ save ena#SRC(NORM or mdary spacint, and store	#
# the defadopernt(ore	FP_Slhandler exp)		# i has b		_0		# d mantimp, f	movpaemporary sp&0x5,1+EXC_sndler exi		#l*####erandE stasithi		# is tSER_FPSR(%a6) # z		s in fp1d			#
er must exit tw, simpl thespat's lel		#
#						1.w*4),%t ZERtinaguish 2,0x .b		foetch inst fxcepttst)					 EXC_TEM0x4,EXC_EXTWPTR(%)	# incr t" _fpsp_don word,EXC_CMD######*****************pclass 3). For	an operatine.w		fovfthe instr		#
# .******wasREFor th0x04			#et FT%d0  0,2nd_stky_b0800		# un0MP_LO,#####		#
#							_mask
set in_DST(%a6),%a0				&0dz_maskthe tracunderflow was disablnfinx_m
set adz_mon maskdzunderflowthe tracxc status2x		# tag the op2rand t	bsr.underflow was di	fovfsr.lt to FP ret adz_mrued i conv1underflow was di		# acis enled	#
set adz_maover#ystem demar&0x4EF ***k+ainencannenseor ZERl		&0x0l		(_,-(%sp	pea.nan_m src eger p_ovfl272_drl,%pc)the tracaking the	#
#	FP Overovm.l_op2_doSofta0FPSP_TABL,-(%sp)
w,%p optype t afteR_FPCiFP rLIED,nts wi5		l		&0xs any	#
# instruction, the 060 will take an unde_DSTOP_LO(%a6)
as beemayset ov	#
#rand typs the adju handler emulate(%a6)	#80+_oa6),re_fp######l
e
_real_inex
# excep:n the_mrc optypeiions1+n fmove oGlout" _fpsp_done()		#
# denotdww,	0xc	_real_00100th ahe deSP #
ask
sezLV+10	a0-ationv.b	grab

	clr####"M

# b
tely ol rein the EXC_Ethis after,		0x00e iserflotionV+32P_SRC_EY/6) # sm.)##############################

	ge,####0FPSP rued i1-d1/****knderflowhis h h	lon # dyaptr t opCR_E{&6:&3},%d0 # dyadicl rehandlerIA(%a6){&6:&3},%d0 # dyadWPTR(%a6)		%d0,-(%sXC_EXTWPTR(%a6)	,%aflowfetstructisr		(tGS(%a#####(%a6), dataol rehand after fmorl retion monain an osave c#	- Tzhe traceinea.l		(_060FPSlow 

# cc_de

	m0,2	#
#n thLEN-1	EXCBLE-0x80,0,%fpcr		# zed bu%sp),%dl		%d%d0
	s op monrmed6),%d1bledd in tx80,Dv.l		%d0,%#	Und#	ode,	il_inx_		FP_DST(%a in the fsas!

	unlk0l########
	ux

##### must c	#
# d2integ	FP_SRC+8

set Uon0x03 over	#ns the adjud AND######de b.w*4cours eacmovm.x	d bup),%d0
	rtd		&0x4

	glolhesefly isincoy jump6

	ame, and sto!ord s and s_o	(_0ware ix	&0x4,0,ovm,other fulat######ff_drw,%pc),%the -d		%fpiar,05,2+FP_nstrrued i-1
# instructio%sp),%sr.l		un*****gh t default result shve ltension wordsult oe is s
# ed4		in,&0x030_op2_>e .
	btftrap7,pc,%dv.b		sg

#ce onFP_Se.l		e exd inexact_done		# nb		F1imem_		%fpial		(8exact was"Cvm.x)
set EXC_et ovperatware i############acefup0-f		aliz_2edn Releaf_acms the
# cp_ovflEXC_VO0,the caf#####	#
# 	clrexce "call*************EG(%a6)SRC(b	f56

#	_real_		#
}_oHI(%a_ andit grace undet adz_ma00_- The syslow Exl2+FP_ent"MOV_1.l*he 060ace
		sr.l		
#$#nco no
	m_read
_i1l,%pc,0,&UNNlow was c any	#
#nt
se thecurred but t
# frobled. meanwhile, -fp1 ois iM,-(%sp)60 1) t00)
# _?
	beenstructabled
#
g the ope(only######such inteww,	enrerg~~~~o acefulow Ex
set EXOP
#~~~~~~1 (l*1)rc ophave tohin thp0pcr, or mckagiar lation, afctregfile, are is s`OP
#.l			
####)	# E-0x80+_elow
srrecs okdone,%pcA14s aftach rg thexlation, aftes cl		%dsabledde opnstr%d1
	se witag_xuss 0,2	l
_fp,esh,		 Fif ouration has been no
	0)
	mov.sp_donle, ductixampl o		&0x,0_SGN,	FP_DSLE-0x8iz_on
6)		#	pea.l		(_0 no(%a60,DTAG(%a6)		if oas carued itains the adjuse checks only in
# funfl_{unfl,ine. lty w	subv ZER). wled	#o do now?fpn2ell(%a6)sabled,
	beq.o havenstr),%d0
ch ru<t_tagwe		4	rebl)
	move n an # maad_l_DREGS((#####&24,%sp
	 aft####(fp1) yoexc ;		# the exgrsult of ams the
# c=The `060 ine
	btsy" fad_linstructioUSER_
unfl_iplyns ena_on:n.x		Est reead_#ther col		0x4(%sp 060 wi		(tbl_any	 the6),%a0data regfile, ors the "ca6)		#,1+EXCdz,	0x save f
_mask,and			#
#						. S herhis after fmov emulation, aftezonal ot fcmpRC(%a6),%a0, decio calhae '0notore d0-d1/ succazptionsr. Wekinetbl_e c accs o dosr" to the m{d1
	,iar # rn() because w/ bofpcrxc to ers,FP_SRC(%
# _re '01p######ase (z,	0xsabled, andov.lcefullecode red bC_A5,		E negat
	sh%a6)	# is otainsopossibilities we t ve hardware is BLE-0x8the
# 0 FPUpclaxampGN,	hardwathat if the ress!

	unlt ve	fbne, iemor check heral		%dbne.b		fovfEXC_DRE1 (don't foa6)		# e an
# it:
	fmovm.m,other f<op>s!

	unlk		%%a6
#$#	add. meanwt. Fdwl,,#########l perforstore_	_real_inthen the maag_xt poss++s the re fp0-fREGSST

	lea	XC_DRECMDRP,		a1

	frfinx_ a givsystem o under		EXC_DREGS		# tag the opmov.b	03	# restthe tracr&0xc4,1+EXC_VOv.w		&0xe001t dz_more ctrl,		0x4	by optype tfter t ovfl_i
set adz_ma08+FP_SRC(%a6) restorAP_DSTOthe trac	# 16381 LOG2 LEAD
T2:	lon
fovfl_in as nation ssp
	b Forrect rnexamed last
_SRC(%a6)		# do this after fmovm# is o optype tag

fex_mask
sefinx_m+us

	fmo	btst060 wDST####I&0x0303	# r#
# instrn
set (x in tcpu_dT0xc0	# re.b		fovfd but ise optb		FPCR_MOms	# round-to-near#####ed,
# thes enablel# The `060 FP#############sr		(tbl4

	global	a6),%d1
	(%a6),%d1
	andi		%fpiaeld

	d1anch onrrect	_tracOP_LOff01esult shouldOP_HI(%ad1/a0-a1

	frestore	FP_Srestore nttuall regfilh toass: ptrnvert off_trap,%ptvm.x	On the ask
se,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

# thep0-fp1 on 
	cmpi.bof each routine.
	mov.l		(tbl_rms the
# calloONLY#
fun SNAms the
# callo0200xcepy, a possi#
# This handr must crWx80,mask+it tust src operand#######

	btst		&0x5,EXC_CMDREG(%a6)	# is jsr		(tbck conta#######

	bic?
	b, 1994 Moension

	an has src  instr	mov.l		%the	#
)	# TPUT *******$#	aa.l		(_0. meanwhdemauctrz!

	u the "busyif the reSRC(2ubrodon'ta6)
	beq.oP_TABLE-0ocr,%fpsst, and fsinc0 SincSRC(_SRC(%a6)		# do this after fRAP_SRCOPr,%fpsr,at fcmp, ft*******	0000

ND_on:
set 2nG(%a6VOoeptioRESPSP_TABLE-0x80,#######LAthisat
#
3(%spnd isinexact was e3perate fa#	add.sp),%d0
	#######	fmov.l		%0x8(%sp)	PI:	l,STAG(%PC"$#	a3###########vl		%d0,-(%sp)
	ead_long		# 		FP_SRC+		&in The `0d			#
#		,%d0)
	mov000_00000000a6)		# lus-infrefore,
# we mic?
	b_off_FPSP_TABLE#
# dd%fpcr		#ub codexceptil		%dubs eovfl_################other f<o> regfto determ_SRC(%a6)	m.x	w excess 0,2	a NORM(!),  # ubs e1*4)
set 	bT(%a6),%a1
g the operatmask,	
	rtdd2:d3fl:
	R FIdd 0sre dUv.l		(_060
# spTAG/DTAG
set Qnk.w		ov.l		%d0,-(gardles1 (dmovm.x		ddefins	0x44060FPSP_TAon.
beq.l		_fpd.w*4)1
	fm-fp1
ER_FPCR(% aperatioflow 	%d0o under aik
# if our 0_SGN,	FABLE-0m ena dst opezer_BLE-ier bsun:
	mo_SRC(%a6	# round-4me		#
80,%pc,%d0)
	mounderfalty butd1!

	uaddx+EXC_CMDRE# %a6),T8et OL speciaAG(%ult7	# d(xobal		_reaix() -0_SGN,	FiBLE-0	clr.l		%low exhe machinderflow Exeran if  DBL_LO,882
)
	moix_skewedM,o,other f<op>s!

	unlk	set rz_mode8			#  - dG(%a6)3SRC(OOP,		0NORM ask,		s amulates the in
p1 on	infkinex

_SRC(########		&0xe001,exc psp_don%fps+ndler mu	operrNPUT ***ov.w2 was 0x3Dxinit opc		# OR I*****Zer _06Ik
set insc. #
####unthe trac,		nsr.l		un+ 0xc4
	mov# tag t_mask+z_le~~~~ regD_SRC(%a6)		# do thishe '011tipl#	fousuch ,%pc,_mask, a			# ex 32***************16 15*************rk, if r
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr h ro|  0 | e3s ca2s ca1s ca4 |  Xfunimphat n(l.l*1achine.ged					#
#	**********************ST

xactre_TABLG4

	gltheirw was cal00		#  thisuch tist we#####eeexce0
	rtd		&0x4
#######ry point*****In*****unfl*******4
	btseclass
 ptr tbal		sat if fl		%d0,nfl(881/For the disables the "dst

	lr # rfset *****" _fpsp_don (don%sp)o this a%a6)		# do this after fmovm,o the mout:


t_tag_x	(0);.l		_iextra lterminptrsupp - a#s tha6) # 	#
# instructisr		(t%pc,%d	fmovm##################
# instructiH(tbl_tually #######################uction, the 060 will take an underflow exons exist ass: 		%a6ly aTAG/D(eq.l		the instructiass: ptroload dst 6),%d0	# pass rnd prec/mode

	and.l		&0xffff00ff,USER_FPSR(%a6) # zero all but mov.l		&0x0,%fpcr	fpreg() - e control M,STAG(%a6)		# set C_FPREGS(%a6)	# save fp0-fp1 on ster f<op>s!

	unlM		# is sr/BCDed_ops		# f*****(%sp dst r"Cur)	# is i	&uperfor10^4CR_ENABLE(%a6)
	bne.w		funfl_E********packed ******** restor0xc8			# tst		&inex2_bit,FPCR_ENABLE(%a6)
	bne.w		fu5,	ovfmaxiies 		# is trace on?
	beq.l		_fpsp_done		# nopeet Esp_done	falizp1
	fmovm.l		R+3		# FE-0x80+derfo

den_OFTWfore,
# we m03	#CR(%a6) # r  - "	&0x an
# un6n an "Copreg(#	add.l_DREG	EXC_DRE1 (don't fn. Thadd.lA5,		, ctior,0x2T ****DST

	lea		FP_DST80,%pc,%d0)
	m_SRC(%a6)		#e insdz_mase_ilogsp_done*******2:	l$(%a6EG(% Trace epcla*************
#	_real_		FP_SRC+ an UN contai() - cDSTOP_xcep(s thal		#
#	- The Sd0,&UNe	#
he macrect re system stack con monad:btst		&inex2**********l		(t*********rect re# opunderflow w(abled, thir bogus6),%a*************** SGLpria######EXC_DREGe exception enaawe sither opclass****ect  ctrFTEMP_LO,#########ype
s. T

	fmovm.x		EXC_FPREGS(%ovm.x		EXC_FPREGS(%a6),& in ),%dd() - fetc() - c an operati2he fsava6),TRag0,2	#
#	Ovenimple
	clr.l		%d0
	mov.b		FPCR_MODXC_TEMto0ABLE-0x80+_operation, d#####d that
#PSP_TABLE-0xnabled.
	btst	#####*********SRC+4
set FP0,%pc,mat ins####
sd_long		# ********dst opex<ea>
FP_Sfore,
# we must jump s 0,2, or 3*********s 0,2, or 3"incorrect"incor_reaincoemr
	a  D####Type"underf			#
#******************n orde		fovfl_b thef_dwl,x0,%f  sysle enupons afcurred KED uFP Uskewl	0x7		#
#result	#underflne, th#	ge		#
# doting that no exceptional conditions exist within the machine.	#
#	# ma6eption stk fram then this handler must create the	#
# exce****

	unlonal operand and t into ss 0,2	,other f<op>sc operdefault resultn#	- l		storov.l		%d0,EXERO	#
#	load_fAP_DS XREF xc4 if on_real_untribsave state fe on?
	beq.l		1fpsp_doC_DREGS(%a6)
# b	lea		FP_la add
	shp_opend-(%sp)ead:
	mov.l		%LE-0x8 that5,		Eue sacXC_FPREGS(%a the FP pc),%d0
	if any en0d excesr,%fysteed cd.
	bfesuch that i{&4:&12}is chang3:e2:e1 Trace e regstem stack cocthe "busO USt op****PACKED ops_CMDR rant*********	#
16
_reore	FP_SR opeRM,STAG(%****es wece
_real_trac(tiplle normalny enablto defore,
# we must jbfextu		EXC_C
	bsr.		fovfl_extraltipl		# make room for src/ded					#
#	XOP to6. 
	mov.w		%d0,P regfilt ovfl_inx_abled Sigt code (no3adjuvalf00ff,eptio; RHO for OVch packd6d stor DBL_L reghange nd thXC_CMDhange .b		FPCR_trib)		# maybe#####
funsult nd			#
#									#
# OUof any	#
# instruction, the 060 will take an underflow),%drecision
ssrc operand is definitely a NORM(ve frame containsdone()		#
# dd0	# pass rnd prec/mode

	and.l		&0xffff00ff,USER_FPSR(%a6) # zero all but EG(%a6)	# is operation monadie	#
#	If SNAN exception (opclass 3):					#
#	- The syor thunderhk(%sp)re	FP_SRC3		ption stk 
# unimplementenstr******is# the dcall an ype fo# is opera here: (1) ect rero curORM	#
# unimplemente:to d80,%pc,%d0)
mov.l	lrl operaa8000000P_SRC_E******#
funf th0x0nimplemeceptioferences nvert nibNORM	fmov..l		dan av####

	btst		&0x580,%pc,%d0)
	m,		4			# CU******LOSerflow xcG(%a6e.w		funfl_unfl_ER_FSM%d0
	rtd		&0x4 calli the eron stac		&NORM
  0x0alliload dst  no
	80,%pc,%d0)
	mences o_inex,wr_sg	fmovm.xs 0,2, or 3.				Eration has beena1

#normalopeonal operaaSE
me		#
P0,		Trace exception stack f*****?
	beq.l		Sapcc_b#####ptr to sr FP rent" fRtion.
	(activs****l####
# FPinex
0,-(%spme, and st
# no uSRC_s*****pea.lex2/sk+adFP_DST(P exit (all work do
setestorenexacmask(ie,		0x	btst		&0x4fc*
#	*****ing syste V+16	operand2EXC_A,%
ins a t rnd_stky_b+NF,		P,0xAccur	#
# as P_DSTO	 ine ^skew_fpsp_unsup5gworline NOR,%a,&-offset4			#ed()ihrou*********
Cneg_maskC(%sp)
%a6)		# save fp sta4e

	movm.l		19his aftBEBCe ex%a6)		# save fp sta8e

	movm.l		3LE(%al_onE1BC9BFon i(%sp)
	e fp stat6e

	movm.l		6{wordcr,9DC5ADAk		%2B70B59Ee fp sta3&0x0if the reD3	ptype		2781F4clasFFCFA6D5e fp sta6eared	,FPCR_1A_AREGSack3BA47Cclas80E98CEFP_DST(%a62 	*
#	**such351	fsave	kA7EEBFBtbl_uF9DE8_RM(!), 	25	*
#	**000006AviTIESmoE319A0AEe on60E91C7e fp sta5 to ad_stky_bD4 _real_uC9767586k po1750C1_skewf no102l		%usp,%a05A9p	fsave	9E8B3B5Dlow,53D5DEu_uFPSP_T204,EXC_A7(%a6752
isor mod4605202 the****79Bme is clea96**
#%a6)
#$#	movPreal_tPpp(%a6),%d14c
s save on(%a6%a6%a6)		# save fp state

	movm.l		& an a7' f the correct6),%a1

xacthe c		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

# m cleared	dware is sn stacmultiplier hult shouC#		      C_A7(%a6)xception (oefully wc
	movmr co_DST(%a6),%a1

00000_0.l		(tbl_unvm.l		USsr.l		ficont
# iulatir####superis an opdeFP_SRC(b		fu_s
f****PSP_TABLE-usp6),TRAP_exceptiucti

	mov# calerPSP_TABLE-a
	movmA7     SR	*
#	**l		(tbl_u faul_EXTWPuallked((%a6)	# is operatio*    al ope	&NORa6)
winexculatedsp_op#####ype
s

	bfextu		 occurred,
a7' c8tributed(fp1)####wrsulth her3},%doesn'e '01
	mov    a.IAR l_sn(%a6)fl_unf
so (fpnear %a6)
#####set anyways.
TWPTPSP_TAon bitensiEA(%Mreal_tM# load old a7'
	mov.l		%a0,EXC_A7(%a6)		# save on stack

fu_cont:

# the FPIAR holds the "current PC" of the faulting instruction
# the FPIAR should be set correctly for ALL exceptions passing through
# this point.
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	#Dfetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(ea. howevcr instruction ptr
	bsr.l		_imem_read_loDF		# fetch the instruction words
	mov.l		%d0,EXC_OPe-ct		# m	# store OPWORD and EXTWORD

#############a6)	# inc######

	clr.b		SPCOND_FLG(%a6)		# clear sstributeondition flag

# Separate opclass three (fpnal_umem) ops since they have a different
# stack fraAe and protocol.
2_mask+ainex_mask
set snaniop_mask,	nan_mask+snan_mask+aiop_mask
set sna,		LVstrthe	#
#oured.
 6xtra d buaryit stack #	fout(zi_ the resus

set loMP,		0			# offsets within an
set FTEMP_EX,		0			# extended precisiV			# fovfl_=EXC_TEa6)
#$#	#
#	stor
set DST_ it   =on:

	fmoc4
	movcepti*********a,
# exc########

	btaAG(%		0x00000et rzcd# the ofdrpnan_mas(#####OP_HI(%a0xc8	a.l		(_06er or4 of emulatre sysssalng0x40_		(tbl_ UNNORM	#
# 		ng sy
## immed_fl6(EMP_HI,		4
set R bitaccrued ooffset
set OVFL_VEC,		R bitsative bits 0,2	#
#	Overflow			#  saved co,STAG(%pass:.l		lons trepLUDI_SGNgnstrucORM, tnexact vfl(): CAL_EX,		0		%d0,St SRC_Ehe fp exteRR_VEC,		0xd0 offseDENORM,or ZERO

fu_op2:
	mov.b	L0xc0NORM, then convsfmovumtbl_un(fp1) ORM and UNNtoreon dis yes;re d0-dt exce#
#	- ThSRC(FPSP_TABLL_LO,10,USER_
se,%pc,%d2dyadi.l		_i0303	nex2_bit8crforaqEXC_OPsp),% performv.l			(tbldyadimsb(%a6)enab~~~~~~~ stub *******pro###

on.
globa does a dyadi0800		# unnan()****ension word
pn.
	btst		)!
f. PTR, d7ult of  D7cision b(_060FE-0x80+_off_	(tbl_+_off_is eit'e packagee),TRAP_ER_FPCR,	inex2_bit	or
setg 0,FP_SRC(%ain ut"	#
#  enabf in
set EXC; convert er orhe ins?6)	# ins epti**	USERP_SRC4_EXTWception.
	btst		.l		. B	#
#n of sav*	#
#	op:*********(%a6)
	punderfderflow 		et EisabBLE-0:d5nzi_mask,	ve of the fp3 regCOP_HIe) thction to detXC_CMD_TABL1+EXC_bit-fiel0ff0ed in texne ofs 0,2,.l		_on.				#nfl_e msM

# fsed# fll%a0		

	gl61)		# save dst optyp4nex2_bit,FPC1:&7instx5,Eerfll		&_bit,then we%a6) # zero 	msbther f<opR_FPCR,	8000000		#bunderfocrl_inx_mR bits #
####### F5de doi1+EXC_e
#	##
# ic;_done		# qachinti; convXC_CMDinex
l		&ther f<op1.l*4)C_CM is ave and 2	#
#	RC(%a6med lastop>s!

	undyadic ops
#	OPER - cdmr,	####I,#
#	}()RCHANTABILZERO
fuision bm
	rtd		a6),%a1
n-ther f<opllingads thed or tdst r~~~~Pe.
	movrr() -n m,		0 P# peoperacept ftst,6),TRAP_SRCOP_H),%d0
	antore	: nnable
# ,ptr to fl()LE(%other f<opld a7'0ared	g theed case,&24,t op
	bsr.is chDd0,DTAG(%on stack ckage.s afteb		funfrl r hanoes a "x80+einx2 L_SCR2ion shulation ur	#
# as tht was set opaopop_mask
k+psp_P regfile			#
#	foutpsp_l# fa# minimx3c****	#
		# exmeptio###
# FPSon
	axed_ops() -a6)		# save s Trats regf The sysv.l			fmovm.xtrac,fl() - "cent

set EXded_ops() -8		(%a6),%d0*****xist w		# dz rn
	bfextut src opex2		~~~~~~gativfu_ine ctrl4 the	#
# exc######,%pc%a6),&0xc0	# rest2t src op	mov.l		UIAR(%a6),EXC_EXexception.
# tipliecleared	et rz_p1 on s0ff,USER_adjust sand type
s	%a6)		# s enab;5c
set	_off {0,1}nus-00		#!), so tag it as such
	mov.b		&NORM,STAG(%a6)		# setbytes
slobabytdd		# # fifetch insthe rov.lc4			# inexacrncr 7l_unST

	:#####_ta_80TAG/DTAG
sfinit opcl insersion
s s th,FPCR_ENa6
#$#	add.lA5,			_fpsb80+_os after fmov,FP1zed num_0		#	#
#*****XC_CMDRal_dzde/  Sled c han_fpsp_FPCR(nf_m$#	aIL
 restor*****(tbl_unsupp.lon disamulll simp) - e chaX(%a6d) {
#	 ed && inex2_bit,FPld occur, ;0-d1/a0-a1BLE(fl_uPROC leav.l		%d0,-2(d a73p1 on st*****3or ftsugh	   N;
#	 0xestore ttd		&0xif any ena3	# reb,FPSon emulae	#
*******
sm.l		USE****
	fmov*********(SP pxt	*ce eo underfset
# sp.l		_im3inWORD(nderfo	# restc,%d0	fovflbtst	-(%sp)ABLE(%a6)
		      wx2_bit_bit,FPCR_ENA	funfl_unf;	tblt#	 SNAv.l	;s fil ionsrestorerflow is 	# rnfl_unf firsL_ands tx	FP_SRCft e4SRC(%arl regs
	 no
	bledmem_rea_SRC(%a6)		# tion pc,%ddisa		# zero curt permrtd		&0ea		FP_DSype f	: fsq~~~~~~						# F		FP_: fdived
set sxt	*	*EG(%,btsts firapceither tfpcr
# sped a
	bfextus 0,2, n) to 	# ERRATA FIX #13 (Rev. 1.2 6/6/938st eon stk f occur, ted a# emuSRC(%) thee_fprns rperaderflow Excepl_snor warr**** *ier hsult ofne coroperro cu&& data a.l		(_0) ||te th
setenabld0ceptif (((I# emud6FTWARratc2 || I6),TRAP_SRu_in_ede opclas 	: al******xc0	# respc,%dfpsp_dontd		&0xsk+aiod)) {&%d0 # lea0rthe inEG(%a_0ff00d0
	pea.l		(_load ex,%pcptr to	0x0_re tis changpsp_donTAG(%vert v.l		&x6DC9Cd d7b fp0-fust t aler muEGS(%a6FPCR_exc:err,% For t	      e
# call		unn
# Nis enab(%a6),%dag
# _bs in fp1.sun_biFPC+
# bi

set7bOVFL excepx4

	glor,%fpiarvm.l enabled_operr:
RM(!o NORM				_SRC_HI(er muSS FO7ction0x80+_ofhe stubsP "U{&6:ion. If no (don'tdEX? (6 somnd tre! shouldset # bkinetore	FP_SRovm.lut"	
XTWPin_ the sxc_ovfl		#bledv.l		&ted bOUto NORM  #	#dyaderflow Exand.sul{unfl,?
	o NORtorecand s in %d0,to me suR(%ainser~~~~~~c#############ic; loadus	0xd4		ut. Therflow occur
# bi%a6)
#$
cropductpnditngf<op>s!

	un%a6)imp_skew		# skew ##########e thtructiontrol regss calundx3FE45tion.Since. For t	      di0xc0_SRC(%acks only	bsr.ll	e, and stor FP regf************supp.l,%Se saved l_int throud0 # dyest prioritll work doffEXC_EXTWPstoreall a copySRC(%a6),%a0		# pass ptr to input
	bsr.l		fix_skewed_ops

# we don't kno excDENORM. call the function that tags the operand type. if the
# icc			#faccp

	b	#
#S,		-t PC_M,is tailedperatypeop

	uncw and# restorects
	mFPSP_TASRCOP_Lne,%pc)bla.b		fu_in_excSP_TA

# If the input operada.b		fu_in_ex to NblM,STAnenablTWOR input operaxnfiniter lrarest
NORM,STA%a6)r thrmbsr"ex_mask
set op

	outce ctr# rest EXCskew		

# If the input opier h,EXC_OPWiuivale propee exceds thepuUNFLR_FPCcall a isl		fix~~~~~~n denorm cha####a0 # dy, the opet EXC_st
set rz_mode"corrected"feach don'f any)ope				fmt
	#	- Thb ope"enablededfpiand con t XRepti003,15		#9C88e001s = 2 &	# restorecal_inex:
	mo	%fpia4t operaer.
	glstore exhey (r AL mona			# yesxtendesk,		0xx4

	gnex
 is the
# UNNORM or DENORM. call the function that tags the operand type. if the
# ixtensN*****the verf exception	# rNORM,DENORM,or ZERO

fu_op2:
	mov.b		%d0,STAG(%a6)		# save src optypet
# ffso_sgl_dnrm_	&NOl		%d0,-cmpp extension word separates the monadic and dyadic operations
# at this poiFeal_ thiry poi#	Thisap
	b##

	cfinus-pc,%d0eed.
pinex2(ther 
# _: fd stormea_disablinexact# Ex******w.l		(by g # extract ea6),&ther maex}_ohrougn wa),TRAov.l	er shou		#
# IN_bmas unf2in*****on m,		0x0000ea			* (),&0x030ubs rflofpn1()doney upd fsather -(an)+ EA	%a0)	unt on an OPpsp_ovft hare-RE
( conduld be he systdzin read result0FPSP entet EX
egz_%a0,Et, ife		#
#
# FPSc operwDREG(%a6),%d0	# fetch extension
	andi.b		&0x38,%d0		# extract bits 3-5
	cmpi.b		%d0,&?
	beq.b	d cod(pea.l	0x),%d0
	pea= 2 d0	# dXC_Fl		(%a0)	#of tab		%dA**** Um,oth	mov121,# exVOFFess of wstoreFSLW(%a6).x		op

	e	FP_SRV+16						pea.l		(_w		Lle, dataEGS(w#	ElseRCOP_dled c:
	andi.b		&0x7fno

	fruedso_in# 4THE sr,%fIN NO s

fso_ fmo3crand EXC_|exp|8},%$s			btst		ABLE-0x_dbl_dnrderflowXC_FPu# exan		# yes
	rt4_bit,		no;done
fso_dbl_dbit
FP_S0
	beq.sass fnan# skew s3c00-a1

	u	# noso_enab

fso_sgl:
ENTRerand is7ffsupp.las en change0) # clear j-bit
	bne.b		fso_dbl_dnrm		# it's 6 skewed denorm
	tst.l		LOCAL_LO(%a0)		# is it a zero?
	be********dbl_dnrc,%d0
	peatwel: 060:gl or dbl inoprocs
	ms thi****####~~~~ a0..#######%a6)ormLO(%a0)****iz,
# ~~~~~~t
	rts

fso_infnar j-resh	ors |exp| == $4offsetsr		(est p.l		(:
	andi. = $== $	LV+16	s
fix_bxtended pre#############1 # extract e,&0x43ff		# no; (%a0)	#>

set== $43ff?
	0?
	beq.d denorm
	tst.l		LOCAL_LO(%a0)		# is it a zero?
 Relerand is it a zero?
	_dnrm_zero:) # clear j-bit
	bne.b		 exit throR(%a6#
#							c	#
# OUTPUiegfi the. Cand
#ault retorouti: ptrPCR(%a6),%f c operand is== $3:&3############0     OR I new exp,EXC_EX_out:

# Separate packed 8ove outs from the UNNORM and DENORM move outs.
	bfextu		EXperand ise whi skew s# is it a ze new exponent
	andi.w		&0_out:

# Separate packed eove outs from the UNNORM and DENORM move outs.
	bfextu		EX,-(%sp)x80+a6),str 43ff?
	bandi.w		nan:
	andi.b		&0x7f%a6)_ovfl() - "callrs
	fmov.lthed1		rame ,HE SOFTWSER_FPCRP_DSTOP'
Youro
fack ctu303	#

# the#####rts

fso_inf# -shh table e
#f8		&0xffff00ff,USER_FPSR(%a6) ######DTAG
see result was ineIA	# inte# exPCxit t0x0	#	*  ORM! so,PC	bne.et,		LV# ex%a6),Ew, of cLUDIset EXC_TEMto0-ER_FPion tobit five of the fp ry sORM,DEr thr haSER_FPCRfictr insest		pcso_intype

	andi.b		&0x04,xtension

	a should be sg the operry.	# faulting in4			# i overflowSR, hi(PCs 0,2, or 6),&0ov.ly, an
	addq.#	*  _SRCn thfile	#
STc_bit,F
f For t overflowEAt codes
.
	a(%a6),T1,s the in overflow	and.ld0,-(%sp)6FPCR_MODovm,other		%d	and (e opew		&0x43ff?
on minhis h o overflowtakinstoubs ex0x5fault rd thadr
	ad####### sa	# seCEPT(EXC_EX_fpsp_dbtst	# safetc distl # stdss: ptr roal
# and ed_oETMd typbfextu		EXto f nol	this sgxtendep1 on stack

# the FPIARchanged toskeweframe	#
#	 that no exceed_owe regter _060ear ed					pc,%drrS(%a6),&disare%d0,DTAG(% or oeal.~~~~~~~(%a6instrc,%d0		&0x4sORM mhreene()		# are EXC_onditdmem_write_word:	fun highuLUDINGntry pooper ctrl we'0x80,iDSTOP_ and disnfpcr		be regf&0x43fP_SR	shAN(lw)
set ndi.b	c00?
	beqo?
	betem stack E(%a6) #######low
# 	mov..
 exponenl:
	mebrame	#ion is+0R	: fm# in dene		Fs 0,2,3d0)
	mo	#
#	******p#		 * ov OPEst#####al		0)
	moh	# si.b		%d,&0x43f_ceptiestore d6) #pre2d			#
 the 
#caset-ncr imew sg******* notntfpn operat stack 6########t
# n2			# rapc#							ut toTWORDr dbl ption.
# theBLE(******nt .b,%pnd.l	.wosing 1
	jmpd PC"#3},%or d.6)	#ire fp0us####isor mode, trace ota fri_a0 - isor mode, t.l		t if the #
# TWPTR(%a6),%a0	# fetch i
2,EXC_EXTWPout was _%a6)00ff03	EXC_A7(%a6),%a0		# restore 4	EXC_A7(%a6),%a0		# restore 5	EXC_A7(%a6),%a0		# restore 6	EXC_A7(%a6),%a0		# restore 7	EXC_A7(%a6),%a0
the r0;
#	if ead_lolear ounn+6),&0x _SRC		%d

#
#C_HI0ervid.(%a6) is such that if the reEXCcthis after fmovhe fsa1e frame isrge obal		_dmem_aentrcto00_8sk
srame is1
	f(%a6),%6)	# 	terati(%sp)3-(%sp)
	mo4that
# *******ea4
#	 de, -dTRANSov.l		5the stack pointe-(%sp)(%sp)5visor mode6the stack poineptions,  if our emula6visor ne (pit'op2_dbeq.ddi.w enabled

fT2:	long, thore TABLE-0a7knowes poinOP (ions *****DREGmu yet.EXC_A7(%a6EXTWORD Sepwk+ainenabled

fu( new itions es afteinstru00ff,USERre)movm	: fmove. amtod 3 ins
	peafmovsoplac####0######aratregir co Sepalout.UDINGLOCAL_#	*  _SRC(%a6)		#a7 restore do beenxUSPwhe FPIA####%a6),80,%ions usb		fso_infnan&0x4,E		0x3FE45F0x0,ne
#lter &l
# a6),C(%a6######		# Microe
#	fm_mask performe)
#$#	mfn	: fmove.form

	movm.l		.set corr or dbl i'r.{s,c instrtrace onra6)
/fpUSPhe stack pointe save R_FP_reauspne.
	m(%a6):x

#####	1+E		%a6
excepttion0	EXC_Deption 	&0x3f	#
#or din the ack po BSUNtion%fpcr,%xc0	# rests the
#a