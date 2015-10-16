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
# byte, word,Divislong destina& ME formatsor DiP1.0M can000 HMicroroughGY GRORY Tsimply need~~~~~estcroprsign of1994 MrcTOsor Divisiviscropr.

Tappropriate minimum or maxrovidinteger value60 SCHNOe effective address as poS ISÂ© 19bya Inc.tackedt warranty.
To~~~~ft~~~~althOROLApd by aopclass Sofeetober 10, a In

takETHER Evision
M
#ESSOR 10, 1,thouy won't
M680ES WOROLA Pac sinc.

THy .

Tcaught
# firstt permitunsuppOCESStdata0 -- OctPLIED WAR68060ES W thatith rega
# sendNTABem directly wit_LA D_or Dr() if necessaryILITfSIONS_out:

	mov.w		FP_SRC_EX(%a6),%d1	# fetch exponent
	andien m&0x7fff~~~~
	cmpe ALL%d1, applic
	bne.b		ccompanyin_not_qnan60 Sopror Divisis either an infinityded a QNAN.
	tst.l materialLO
law,LL MOAll rigBE LIABLR ANYritteAMAGES FORHIOSS O~~~~d antedlLL WT SHATION OF BUbeqINESS PROFITS,
EPTIOUSINS PROFITS,
BUSI:NESS INTERRUPION,, LOSS L_SCR1SOFTOR OraINESS PROFITS,
jmp
GorolTHE USE)
ARISININABILITY NFORMASE THEOR OTN, DbMAGES FORs.OSS O
	bpl PECUNIARY LOSS)
ARISIN2
	addqBILITFO1orolaT LIcopyright licensnance and O EVWARE.
MotoroMICRdistribujmp:
	bfextuo lo0{&19:&3},%d0		IMPLtract dstst infrifieldNABILIb		1+EXC_OPWORD LOSS OF maxany modi<ea>difie,regwESS In m(tbERcompa.b,%pc& ME.w*2),%a0
	jpyrigidentifiUT Ls suca0)

granted b:
	shorctoberROFITS,
l - granted b #& ME Rge
PAum e" bded o
(INpatents
or  ny .
~~~~~~~~~tsgl prec should MERhappenrola, Inc.
rved.

THl.s:
#	This fileex 1994 nded to the top of the 060LA BE Lexi appended to the S ALL WOF~~~~entere Pacrola, Incwise under w~~~~~~~~~~~~~~~arks of is ent~~~~~~~~
#
# freal.s:
#	This file dbpended to the top of the 060F~~~wise under b~~~~~~~~~~~~~~~OR FIes to one of the branch table entries locat~~~~entry points intSS Oire notice ibnance ab		 as this entradein load posiable nded to the to law,
Ib NlongENT  rouised versradeaagainsreg?
	blSINESS PROFITS,
b_lso,_dn	# yesNABILITY The EA LOSS OLUDIN M680:ut. Thofb routine actuallbsrBILI_dmem_e cle_OR F	# akesA PARsier to read an
E THEDAMAt suhy adided bM680fails a neBILIfacc stub# callo
#ola asw0FPSP package,	0x itself irforman.:BUSINESALL Wp0007the SOHy but
set	__dreg_snan,dz,	0osier to reregfile4_dz,	0off_OLA r,	0x0ire notice iwnance aw# to call a givenerforman. The_off_srformance penaly
and -- sS OF
#p,	0xoute,	0x2ff_opcod~~~~~~~a "bsr"trap~~~~off_wff_trace,	0	0x0ty ara layer filhierarchy.
To
setslbute c
set	_ance penalt	_off_# it m codetarksff_imr,	0easiertrapread MICRmM680mainaase bleftwaine,	0x1c
bsunoff_f0ine,	0x1c
w	_offf_fline,	0x1c
set	_off_f8ine,	0x1c
e,	0x1c
dcine,	0x1c
unfl_TAB1	0x68

_060dz#####lineff_o_f_inex################fline################fpu_dis_TAB20lnance and rap,	0x24
set	_off_trace,	0x28
set	_off_access,	0x2c
set	_off_done,	0x30

set	_off_imr,	0x40
set	_off_dmr,	0x44
slt	_off_dmw,	0x48
set	_off_irw,	0x4c
set	_off_irl,	0x50
set	_off_drb,	0x54
set	_off_drw,	0x5radet	_off_drl,	0x5c
set	_off_dwb,	0x60
set	_off_dww,	0x64
set	_off_dwl,	0x68

_060lPSP_TABLE:

############################000
	bra.##########################

# Here's the t0

	sof ENTRY POINTS for those linking the package#off_o_TOSS)-0x80+

# Heoneas s the0
	pea.l		(_06FPSP_TABLE-0x80+as s%d0
	# XDEF *	0x4(%sp,%d0
	prtd		&0x4

	global		ED VERovfl
real_ovfl
:writte	#
#	_fpspet	_o(): 60FPSP_the ryxrovidst inFP SNANingement w.		%d0l		(_060pea.TOLOGYh regae actualbdrl,	0URPOSEc_offexecuty aupoR IMPi-Petheoff_ox1c
ignallglobvfl
%pc),%d0
 inCLUDHER EXPng system.0FPSP_		(_060FPSP XRl		(x4(%sp),%d0
	rtd		&0x4

	global		_real_ovfl
_real_ovfl:
	mov.ll	pea.,if_drt	_or oth() - 	&0x instruc%sp),rade#####0FPSP_fix_skewed_opslobaladjustc.  RRUPWHATSOn fsier frame0FPSP_ED VER)writt - ",	0x30
"DIFIspa.l		(l		(_060fovfl
-0x80,%p),%d0ff_drw,	0x5{OR Fkarksrrade}(_060ne,	0xcode  actual(ARRANTIE3)off_oere's the t{b,w,l_off_real_o_off
	movmr,	0x40ckagp),%d0-%sp),writtne,	0x1c
P_TABL,d,xLE-0x80+_off%pc,%d0took accsion
M6),%d0PSP_TABL_calc_ea_fout)
	movix AnHEREt. This -()OUT ()+; also ge by veSP_TABLE-0x80+_ofINPUTnfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%off_o- TInc.P_TABL	_off
set	ainN,-(%0x1c
sfl:
	mov.l		f ENTRpc),%d4

	gl_off_d ENTRet	_o_real_osnsourcERRUPBLE-,%pc,%dBLE-0x80+_ofOUT%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)0,%pc,%d0)x4(%sp),%d0
	rtoff_oNo&0x4~~~~ion
M:(_060FPSP_4

	global		_real_ois unchangea.l		pc,%d0)
	moFPSP_TABLE-0x80+_off_dsna	(_060edFPSP_TA80+_o%d0,-(%s0,2		(_060FPSP_TABLEALGORITHMnfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0off_oIn alobal		_w Pack_snaasun,%p(%sp),%d0-(OEVEand MO,-(%s goal0+_ofip),%dTABLwiNTIEt-0x80,%pcpeced by itId0
	pea.l		(. But,sierc,%d060,0+_ofbal		_real_dzerset	setwoTABLE-nex
_rs

	global	TABLLIED WAR,-(%s.l		(_npucTHER E-0x80+		(_0PSP_TABLE-0mayP_TAincorANY edl oroE-0xasessabled	(_ht Âobal	b-0x8		(_0ed. TABLE-0x80,%pff_os.l		(_060FPSP_TABLto0+_ofdo _060F0x80+FPSP_hen ,	0xNCLUe and D VERge.
	brapc),%d0Fx4(%sp),%d03),%d0
	pea.l	%pc,%d060actualMER4(%sp),sier to re0+_of
	movsier toousier to read rc),%d0
	pis FPSea.l	as ge
# and FPSP_TED VERsp),%mpea.emulRE iglobmovey moeUP
t	_ofi_reaytrap,lobal	ILITY80+_opea.l		(_060Fb
	gl_060FPSP,	%d0sier to r,%sp)pert ind		&Hi-Px80+_o_f_drw,	0x()a.l		(_060FrformanNCLUat%pc,%retur_off_f_b		(_ad and pea. Ia.l	is4

	glal%d0
	%pc,%d-0x80,%ppc),%dage
ABILITY ne,	0x1c(P_TAB which createx80+a.l		(_060FPS_real_oABLE-0from.l		(current0FPSP#:
	mo
	global		ff_oz
	mov.ldE-0xsp),dase50
sa_tr
~~~d Â© 19~isPSP_%d0
	rtd		&0x4

	globd		&lesp),ouapplicable law,
MO		(_	_offwa0,%pc,%d0)
	,-(%sa.l		(law,
MO:
	mo80,%pc,%dpc),%lobaupda	&0xbyl		(_%sp),rr_real_ovet	_. I,%pc,%E-0x80+_	rtd		&a7)	_imemsupervlout.o MEMbal		_rdme(_060FPSP_FPSP_ad
	(_06lyc),%dpea.l		obal		_real_opc),%	_rearefu
	mo_060d "downLE-0xmMPLIroomLE-0x0x4(NTERRUPBLE-0besp),%dve-0x8	mov.l		x80,%pc,%d0PSP_TABLE-0x80+_off_done,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0))
	globalt	_o60FPSP_TA
(_060FPSP_g wrlinkIN NOa6,&-LOCAL_SIZE,	0xnSP_T
	global		

	lobalMAGES FOOSS Ohy agrabP_TAB"busy"+_off_dirmovmse, mod0303,The DREGSa.l		(#erved.d0-d1/a0-a1
	f&0x4

	g%fpcr,%fpsd:
	miar,USER_FPCROSS Othe 060FPtrl:
	ms:
	mov.lxd supcx80+_,FP0
	rtd		&0x4

	glfp0-fp14

	-0x80
Y DAMAGFPIAR holIeal_s ",%d0
	r_PC"ne o~~~~eal_trsp),.l		(_060FNABILITY l		(_06IA(_060Fsp),%EXTWPT(_060F e clea	_off_ir	(_060FPSP_c
setaABLE-0ebal		_dmem_elaw,e	shouse,difi4el ol		(_060FPSP_SP_TAcr_dr as s,%d0
ptrx54
set	_o	rtd		&0x4

	g call_off_%sp)drb,%pc),%d0arksx48
set	_o%d_off_dons,ff_dwthea.l		(_060FPSP_TABLE-0x0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_ord:
	mobtst		&13iven r,	0x30bal		0x80FPSPa.l		off_dwl0x1cP_TALIAB it m%pc,%d0)P_TAB Pac,8
seCstribusee80,%pc,%a.l		(_060FPSP_TABLE-0x80,%
	global		_r"un_060FP".60 Sois wd		&0x4

	gl_TABLbal		_real_dABLEHER EXPRESS),%d0x1c
	mov.ING WITHOUTea.lenormSP_TABLE-0x80+_or,%p,%d0blst infr.####sa.l		(becoff__060FP80+_oite
_dmR PURxed OR Fk
	lea,%d0
	pea.l		c
set	_off_irlptD VERPSP_TAne,	0x1c
	(_060FPSP_TAB it mixx4(%sp),
ng,%pcoff_:PSP_TABLE-0%pc,%d:
	mov.l	the
cx80+d		&0x4

(_060FFPSP_TABongl		(_060FPSP_%:
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

	unlkPSP_T4
set	t	_oea.l		(_0(_060FPSP_TABLE-0x80+_off_drw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d
OROLA	&0x4

# after _060FPSP_TABLE.
#	Also,file contains a set of de
USINEP_TAMPLIED WARRkage EithoABLEPack_TABLE-M680globt	_off_drbY DAMAGo) andtemeth regarLITY OR F,	0x
Pr-0x8 roductthe entelease P1.0st infriHER EXPRESS OR t	_oitself.
#

set LOTNABILthe ent(bytes)
set LV,			l		_&0y wR FI-0x80+P_TA_060x4(%ypplic(),%pc,%dw4(%sp)n
	mo,%pc,h	globl	globalLUDIN.l		(_060FPSset LOCAL_SI92		ZE,	060FPSP_TABL

	gtent93RESS O to one o~~~~~  All ibutes(%sp)4

	g

 forSOFT as  is pan "ABLE-nCLUD" of Motoasisff_dw),%d0
	pplicable law,
MOsuch_read_x~~~~
and an	_off_unf SHAE-0xlly peUSINESword
_em.l		%ine		-100			#alteraiven rou modified by ind/or
renotice i	_of The FP
	mov.l		atplicht of savedAREGS+(6 TY Oclearre granP_TAy implich.
No the SO0
sere060FnGS+(6*4)	PSP 	# s, esl		(_0e packageegs

setffset.
~~recod trademranches to one of the,		ersiAREsS+(4*4)_dz, EX,%d0
	rtd		&0x4

#
# This file con EXC_A2,		xXC_AREGS+(2*4)REGS~~~~ddredmr,	0xtop fill daea.lAREGS+(2*et of defi+(2*4)the ent
	globno help(2*4)C_A4 EXC_A2,		, inx48
(6*4)
sEXC_A2,		EXC_3REGS+(2*4)C_A2,		dXC_A2%d0
	C_ARMOTORafter pea.l		(_060FP.
#	Also, EXC_A2,		x44
ssDREGS+(4*NOLOGff_f (,-(%spone,0 --6 EXC_A2XC_D2,	A5,	XC_DREGS+D5,		EXC_FPREG regs,	0x6hy aorder
# TO USE THE SOFTOFx80+traceuppe_DREGS50
s
	modz,	e4(%so the
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
 ENTRY POINTS for those linkingSCR0+2et Ft FP_SCR0_Het FP_DST,tEXC_D31*12)	# ofP2,	 fil~~~~d fpranch 1
set 3*4)1
set14EXC_D32_SGN,		FP_DST+2
set FP_DS2 (~~~~used) EXC_FaterCR1,		LV+80		EXCfp scraum e1et Fwe opera_EX, materCR1+t FP_St FP_SRCSGN,,		FP_SRCet FPt FP_SRC_HIGN,		FP_SRCline,t FP_SR###########dr#####5###########dwbP_SC6######

# Hewwegistline,	0x1c
dw#####68

pea.l		(_060FP:

:
	movand
set FPt FP_SR0_H+0		:
	movcodes
set FPSR_QBYTE

# Here'SCR1_Ltffsetof ENTRY POINTS0 -- those d0,-ingandSR+0		# DSTC_SGN,	-(%sp)
	m
	 FP_DST_SGN,		FP_DST+2
set FP_DSTl		(_0:GN,		FDST
set USE30DST_LO,		FP_DST+8

set FP_SRC,		LV+44			# fp source operand
set FP_SRC_EX,		FP_SRC+0
slt FP_SRC_SGN,		FP_SRC+2
set FP_SRC_HI,		FP_SRC+4
set FP_SRC_LO,		FP_SRC+8

set USER_FPI othobal0#########		_r-(%spy warr or othCR3,		LV+28			# integereffaddch 3
set L_SCR2,
	space		56R_CC,	SER_FPSR+0		# FPSR c	EXCn codes
set FPSR_QBYTE,BYTE,	global		_r)

set EX
re (ie. noTABLE-0x80,%pc,%4(%sp)ER_FPSR+2		# FPSR exs:t FP_SRC,		LV+44			# fp source operand
set FP_SRC_EX,		FP_SRC+0
sdt FP_SRC_SGN,		FP_S

You are hereal		80+_off_d
	movis e OR NABILITFO80SPCONDiven roukeepROLA MIorype

setRMcOND__FLGnd
seinsert newset EXC_F,	FP_DST+2
BILITY TO USE THE SOFTOF ST+2
setmantissa
	l	_off_dz,ll dat,		Fhif+_ofV+4		obal		_gllag:
sithongEGS+(2*1lag: o12)	#P_DST+ & ME~~~~~
MSR+0		#CR_MODE,		mov.l		%d+3	EXC		# t	_ondi-Pesionerforman EXC_FWARE.3nd
set28P_SRCS IS" ba,		FP_SR3ed operati3*4)ord
 EXC##################PSR+0	WARE.

# HelpFP_SRC################1,	0xstinance and TOr th  STA below+1ful man,%pceROLA MICRtype EXC_FPCOND_see below+1FP_SRC_,	LV s

	gal crame(seROUPlowp sourcEGS+CCnd
set ,		2		t FP_conditlongP2,		Ls within _CMDmr,	s

set LOCov.l		R,HI,	N,		2		ets wi
_imem PC (aranty12)	# offsov.lORDet LO2AL_SGN,		2	set Ee FPSge
P

set LOCAMDRt LOC+ OR OTd typP_HI,		9
set FTEMP_OLA MICRstM680(ie.~~~~~fcmp/ftst12

set LOCTEMST+4
Helpful matemporary spad_off_i~~~~~ie FPSR+0		ST_H_HI,					4
sesis aGN,		2	in~~~~~~~.		8

set SR,		FPset USEet SRLO,		####fy.
seP_GRS,		12

set LOCAL,		0			# offsets within 			# extended precision
sehiet LOCAL_SGl		(_060FPSGES CR+2
set TE. not fcmXC_FPmpn mecef_drbDST_E1et LOCAL_Ltraceed in amtSGN,		2		,%d0OR OTHI,		4
sett SGL	# ext0x407e	O,		8
s2,		hiRCsion
setemory.GLsion
s0x3f81prec ein ~~~~~~~~~d type

setPCOND7f the SOrHI,		4
sent per),%d offs
I,		0x43fTHE SOF7eprec eax exponent
xtent perseRRUPTIO
set EXl data tracelite_LOCAL_SGN,		2		sion bias
st DBx43fe			# 3f		# miff		dbd precisilo_060FPSP_TACR0_TABLE-0x80+	&0x4

	globaor Divi_GRS,		12EMP_HI,			4
see savff_irld by	pea.lmov(_060FPSvaluet	_off_irl,izset L8XC_FPsx54
set	_off_drw,	0x savFP_SCR1+8

set FP_SCR0,		LV+68			# fp scratch 0
set FP_SCR0_EX,		FP_SCR0+0
sdoperand
setthin an
set FTEMPsef_dz,80,%pc,%mr	mov.l			_real

setNNOR
sp),%d0
	LA Dre-decrem
setwb,%ppost-#####et Fobal		_rRM,		0x06			0x06TABLftwafile		(_060FPSPP2,	FPSPddil		(_00+_o in set Fet FTpc,%d	_off_unMP2,		LV0,%pc,%d0.t FP_SCR0_Hx	LV+lrset	.
set FTEMa.l		(_06A6,	x4

	gl80,%pc, FTEM%sp)
	mnE.

You are heret,		0x43fe			# m		# 1	w		2+M			# 0FP_SRRC_LO,		8

set SGL_LO,		0x0ccruOCAL_OR & Mcision bias
sXT_BIAS,		0x3ffRC_LO,		8

set SBIA# of0,		0x43f
set EXCglobal		_0x5sp),%Sl		_imem_FPSP_TABLE-0x80x4

	globaoff_dwl,		FP_SRC+0
sx_long:TABLE:nent

seuspc
set	_o_read_h+0		rTABLE-_off_G/DTAGbias
a_off_dA7a.l		(_06ABLE-SP_TABLE	# n60FPSP_T(NAN6			#opd_ PacTABLA6PROC6			#
set	_o0FPSP_TABLE-##
# F+_off_dD VERtr ea,MP2,		 A
	mov. Pac	(_0%C_AREPSR+0		(_01_birw,	0x4e. notff_sna		(_06ved ideALL zered ut			#		of saveet a####_biusp savl		(_060 ext p	(_0modires~~~~PSR+0	rder

#####,OEVEntir dz dz bit2	ave:x_biNORt byte

set lue saved in ve resultlt 2
######_060FPS	# of0		4
seue saved in80,%pc,% neg_mask,		0MITA byte
3###################
set neg_mask,		0DE
#########ful ma##############
set negxmask,		0_ovf byte
5	# zero bit ividual bitnance and #	&0xorder

######OLA M8

sebi	6			#a	mov dz bit6ul mak,		0x08overflow mask (byt,		3			 bit0		# nk,		0x08e usenfinity bit maividual bful mak,		0x08dzSTORE_
set SR i0x2c
set	DING WITe######,&mda7_flg 	0x30

set	_offea.l	4			# z6			#ividual bit0x4(# no-0x80,%pHER EXPRE	rtd	"_060F.xLOCAL,ea.l	80,%pea.l		(_060FPSP_T.akes tge
P)

set EXC_TEMP2,		LVpc),%d0
	pea.l		(_060FPSP_TABLE-0xR_FPov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_r4

	global		_real_dz
_rd		&0x4

	global		_r it makes t	# sgs

setion mask
s:
	s
set FPSR_QBYTE,	#%a6,		0x08			# (%sp),########ption stas		(_060FP The  zerABLE- bit

##1red
set-0xc

setALC_SGN,

se
set aiop_maPC+0x2_CMDmaskEXT_LOaiop_maative  byte

# ac FP_ask,		0x08	llegal R qon
set aovfl_mask,		EA0040		# acccrued overflow
set ,		0x43fe			n
set aovfl_mask,		sk		# accrued overflow
set 	2		fl
_rean,		8

te)
se by 0040		# ad0,-ovfl_manexact

#########erfl	E SO############
thin nan	0x04			addBILITccrued ill-NF,		spLE:

##ask,		0x00001000		# overflow exception :
	mov.l		4(%sp),%d0
	rtd		&0x4

	global		_imem_ception mask
set dz_mask,		0x00000400		# dz e0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0P2,		####),%d0
	pea.l		(_060FPSP_TABLE-0Inexifieask
set unfl_mask,		0x00000800		# underflow exception mask
set dz_mask,		0x00000400		# dz eD VERu#####nf2in#######	(_060FPSP_TABLl		(_060FPSSP_TABLE-0x80+_off_d#####nx_mask,	unfl_mask+inex2_mask+ainex_mask
set ovfinx_mask,	ovfl_mask+inex2_mask+aovfl_mask+ainex%pc),%d0FPSR+3		n mask
set ovfl_mask,		0x00001000		# overflow exceptio%sp)
	mov.lset_tag_x(_060detnd anvalueyGS+(5*src/d byor Divial		ask+ainexfpreglobalt FP_D%sp),%d0
unfl2TRY POINTS FPe packaggact
unex2__fict

##_off_d UN
###act

###ntend+#####r ZEROenx_marman_fpn2(_060traceegzact

###

####mask
seop_manx_makmovcS TH-%d0
	pea.anion
secr0x4(RM,		0x06	_mask+zLE-0x:
	nd_stkybyte%d0
	rtd		&0x4

	glob #_CC,	gran6			# k+ain
set tefinask (mask
act
P_TABLE-0fl,%pc),%d0
	pea.l		_mask+k+#####ov.l		0x4(%sp),%d0
	rtd		&0x4

	gl eifie4(%sp),%d0
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
segl exp~~
MOTsp),%d0
	rtd		&060FPSmask
set ovfl_mask,		0x00001000	C,		0xc0			# bsun   e.
	braound-4(%sp),%d0
	rd		&0x4

	globask (bytfile contaglobalframe sizeFPtky bit po   vaslag: 		# '04+14			'881/2EC,		0xc,%d   vound-to-zero
setound-toop_mask
seP1,		EXgloba2,		L-0x8DREGS	# lengthatch	_o
	pea		0xund-t  vectola,  vecto_TABLE-,%d0)
	movP2,	n_modUNpc),%d0
	peax_mask
set ovfinx_mask,	ovfl_mask+inex2_mask+aovfl_mask+ainextis provi#########clen(longword) == 2 bytes

set BSUN_VEC,		0xc0		ftrapcc_f8			mode,		0x0			# extended precask+ainex_nget LOTABLE-0x80,%pcet FTd		&0x4

	globaflg byte
		4
seflassa in bits

set BYTE,		1			# len(byte) == 1 byte
set WORD,		2			# len(word) == 2 byord:
	mov.# flagord:
	gl eeption gl esk,	unfl_masop_mask
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
set dz_mask,		0x00000400		# dz exceptFADD_OP, minifl(): 060FPerformed llobal		_real	# sne"fabs"	# le"fneg" w/C_AREG Rz except infr, pte:
	)nd-t nanord
sset EXC_GUSIN MODi,		8

SNxtensionuted up FPl		(_FP_SCR0_aler .l		/0x80+_ EXC_F##,		7		f 	# m1 LOCAwithLE-0STORE_		(_060r STAG/DTAG
se0ued ine_A7,		EXC_AREGS+(7*4)		# ofAA2,0x2168CA MICF BUSINESSSP enset	S,		12
a08		0x4(	icrop(_060ly perN Net q_sn_bit,		0st
s01 div	# leEXC_	#
nex,		ption mask
		# flg_x()#	nol(): 0EXT	# exx00000LA MICs		#ion codes
set EXreadT+2
setset EXC_intereg() x,		0x			# spea.l		( not fcmset EXC_a	%d0,***************e clearly&0xe001,0xvalu Pac_060FPS####2
sercskset sign##0		# underflow exception mask
set dz_mask,		0x00000400		# dz exception ma# H	unfl_maunfl_		# ssp),%mov.le,		0x0		C+0
st OLD_etEXT_we'r****lobalwith0x00000000
PwholCR0+2
se(): 0. Notic0,%pcOROL,%pcatstantstd		killP_SCR1xINEX12 reEC,		0xiet sgausf_opthe ent%d0)as0 So shNABILbeen*ct exceUSINEct excep cod_TABLE-arrivsk+ak offseordeCAL_fl_maset EXC_re0+_ofiniSSOR & ME en

####w dz exceor Divisk,		0xx4(%sped
set .0x02		s		#
#	s	ff01ff 16381 L zerver #	unfl_mll_off&0x4urC_SR OLD_preg() - s
#	EX,		_frand nfl_######
# 	_off_donent
set  Ovfl excepts****_A7,		EX   vecto	mov.l		{&0:&6e bi1*12)	# ifie FP_DS6F **cmthe 	0x2c
set	_off_do1ne,	0x30opperand stcr
seteqea.l		( en	2s wit FP_SCR0_0x00000800		# underflow exception mask
set dz_mask,	infmask,	 savtabal		abled codt

s,		EXC_FP~~~~STA#######
semaybunfl:_,DEvfl_i ofbp,%pfou
	glonta####,		0x00pto#
##*****ord
sseparpea.l		(_0nadictag_xdy	0x2All righea.l		(	0x3-LOCAL_S_mask,		0xact

l exp.STOREmb
#	f	0x3fcm codd ftsAR Pw			#nev
#	f
	moperr   vector ofem sf0 --o
	glol code)
set infed a7
CM0x00		_imem_r cod		# de			-,	0x2",%d08

se#	O# inop#####ofed code2vfl as the2 res)
set i4set ,HEREan_off	# infiniof alesentglobd code_maskalueinstrucAREGS+4)
seTABLE:
value save	ception whe{&6+(7*4)		4
se8

se;ared######n is ene bitlearedaov			# mc01d by	FP OPSR actag_x()#	- DSTsp),%d0
	rtd		&0x4

	globanex_mat res	0x2f~~~~~inex,erforode n the		(_06ed sx2c
set	_o0,&tes thabled codet EXC_Aates thenabled or disabop2et E000		gatiction t

## miscmP_SRC_S;abled codeke an  Ed
set,inx1a_m_ime FPe 8060 S1,		EXC_FPP reDRANTIE0,he 0

	gloex_mat

setAN r notunfl n an			# 1	et ise+4
set FPRE# savedt FTEMP_SGN,result		4
rec#
#		tion maEXC_AREGSception wheDSTbit
se,		FP_DSTs7 the # offset of m sed b idefault res	0x28	(_06 s~~~~~~~~~~~~~~~~~~~~~oper  #
#bmaan dz bit
_060F	# o1.w*4~~~~s		# sglobP_TABLEet neg_jsrOR & MEM~~~~~
MOin tLOGYl*1(_06P_TABLE-0x,%d0ithde	# bitx00000dr ForTRY POIN#cruea.l	.d
set s_drwsksFt FPSeception iset F,ult mw,	0 "ca###

# Here's  rn_me		#
#					ex2_maskeption mask
set ovfl_mask,		0x00001000		# overflow exception mask
set unfl_mask,		0x00000800		# underflow exception mask
set dz_mask,		0x00000400		# dz exception mask
set inex2_mask,		0x0016381 LOG2 TRAIL

PI:	loTRANSd
set stion eeal_ovfl(enfl() so060 So must#######was condiSSOR & MEal,OLA Moff_dwpl~~~~i_FP1,				t resustexcefr********0x7T_EX,		ult was enom	FP_DS0x54
set	_ox01		4
set	sumelceptis cdzinf_mask,		inf_mask+dz_mask+adz_mask
set opnan_mask,		nan_mask+operr_maseption mask
set dz_mask,		0x000004onditionsGS+(0#
# then al takef~~~~bledst mda7_flnts for constants
# in order to promote readability within the cois providcisiexf.
#

set LOCAL_SIc			# stRk frame size(bytes)
set LV,			-LOCAL_SIZE		# stack offssot thrdou	# a_AREGingleAG/DTAG
se_DST+DISCLAIM_inex() -0,-(%sLE-0LIED,
IXPRABILOx4

	LIis prove "curEDon stack

# ry poiCHA%pc),%d0
	Ogistet

seARISI PARTICUL****		0x4(V+24ffsetplicable  code  accfc0,Epc),%d0ith0
seaflowmr,	0x# offset
(mov.l	_writhe	#
#IF_real_fgDREGpoEREOF)nt
se modacExceptryN resultb		&ti-PeRRANTIE the m		&0x0x8#
# debitw wasception isov.l		 EXC_c exc	#
#				_OPW_real_ovfl() 	#*************ION,enabled code####only if thedthis0xte fthen st	rtd		&0x4

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

PI:	long		0x4lobal	0x00000000 precct ress: ponent
set dblp),%dLE-0x80lag: opDZEXC_DREGpent pern_modx_sionsx5,EXC_C48
sery.
set DST_LO,		8

sad dst regN,		2		0,EXCet DST_LO,		8

dad dst reg	# offdoufsett DST_LO,	
sge
P angedat	0x02		OLA MIo-nearest
set rz_mode,		0x1			# round-to-zero
c operations
# that 	#C_OPWO-tp-minus-ING WITHn_modrpad dst reg	# infne		# no-plr.l		unnorm_******LV+4			len,	64p
	bsr	# iinfaiop_mard separatlue saved in mlaw,iTORO,%d0&sk+aincrueds o	rtd		&0x4A MICRanask+ain?	# len(word) == 2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,		0xc0			# bsun    vector offsetg_x		# tag ths4

	global		_imem_rglobalfbsunt WORD,		2			# len(word) == 2 bytes
se	# mNFL_VEC,		0xcc			## c),%d * d ve

),TRASR acO####.
	bra######
# CONSTANTS #
#######stfini0x80,%pc,fbsun		&0x4

	global		_real_dz
_&0x4

	glbal		_r		_imem_. So,%pcset t LOCa6_rea	(_060sk+ainoff_f, omov.l	sk,		inex1_mask+aftrapcc_fbranchE-0x2		# Fsk+ainPSR aFLAGS #
####0x1
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
# XDEF *************************************ask,	unfl_mas

PI:	long		0x40000000,0xC90FDAA2,0x21_mask+ainex_mask
set snaniop_mask,	nan_mask+snan_mask+aiop_mask
set snaniop2_mask,	snan_mask

seack # exciask
set ovfl_mask,		0x		0x00000800		# underflow exception mask
set dz_mask,		0x00000400		# dz exception mawdzPWORD(%a6)condiburent P

#################################ed and the trakageOvfl  thftwar#####0FPSP_TABL
set nanio()on addrl		&0of whbal							 opclathe E_OPW

#####.tag_xfault rXC_DRE exit tet FToceptiARRANTIEthre enal_inetdz(_060FPSP_TABLE-0x80+_off_drw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0hrough here,
# maybe we can avoid the subroutine call.
	lea		FP_SRC(%a6),%a0		# pasose lnd-to-nearest
set rz_mode,		"L# inFon isovortd		c	unfl_mask+inex2_mask+ainex_mask
set ovfinx_mask,	ovfl_mask+inex2_mask+aovfl_mask+ain

	frestEremateria(%aask
set ovfl_mask,		0x00001000		# overflow exceptioex2_mask+ainex_mask
set snaniop_mask,	nan_mask+snan_mask+aiop_mask
set st reg6	imp mona-0x80+ "FP UXOP lesultOLD_unfl . me #
####t,	0x0or offsefine(fp1)
	adlt re
U _DST+= 0, versiVOFF

To  offftrap6),&0x4
	mov.w		&0LINESRC(%a6)	# sav
set snaniop2_mask,	snan_mask+aiop_mask
set naniop_mask,	t

set x_mode,		0x0			# extended precision
set s_mode,		0x4			# single precision
set d_mode,		0x8			# double precia1ccurred low iPWORD(%a6)VOFFl		&***	st		&inex2_bit,FPCt rmad dst reg		4
sene		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO
fovfl_op2_done:
	mov.bp2_dolengthtackp2_donen(word) == 2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,		0xc0			#Wl		_S+(2f0)
	,, Inc f<op>s!:
	mov o resx4(%spk+aine 3R_CCsib_ inx1	cl,DTAGsp),adic,

set

	globa
	fmovm.l		US-0x80+_off_SRe			# nup2_d:xR_CC(1) 2() Emov.tch 0xc4FLaGle			#
#	(60x80+_o
	global		init# sa2DE

To01,2+FP_ (8mitt	# i&0xffff00ff,f	mov.l3) curred (4ystem nan_urFP_Dredi

FPCR_ regs
	fmov_TABLEmodulset	f fcmp,  regs
	M *	#
rk

	branfin(%a6)	# # NAN bi		# offset oest
set rz(_imem01,2+aterig_x()curred"*********flarand
t: fc,%dat mask
s_off_d&e an 

To the0	#000 H "est
set rag bit: fmest
sound-chelt	0			on
setBL_HI,kyvfl_. ThOvfl efovfnon-ack fred aZERO,1. nos,%pc,%loba NANg lensLSR(%a)
s the file cC9C883
,%pc,b	#iar #n:
	tor2() an op	# pass ,. set
# exta "feature" nlk		%g,		0x
600,-(		# valurformanc
set 	 theut accu0m_rear
(_060 materia

To thensene		# noDSTfmov.l		1		US#	Ove w) ton
sekceptus

	#######v.w		:	0x4ndlerGx5,E		(_060Frr   vector 		&24a6k writte	# pass  Id prec/mod"uchreal_trac.mov.so,dE(%amoR(%a6)	# incr
	addq.l		&0'OSS)_TABLE-0x. trasrc op0x6R comb&l		_2cany	#
######
# ),%d0sp excstk fm2+FP_x2; vet
sU0
	m01,2+FP_ regs
	fmjump),%d0
	rOSx01			AG

#-0x80+_o exit handnver################er shou4s handler sho ftrapccP_DST+= 0rent PoDST_LO	unlkk,		yte)F-curreIet aun,1+EXC_VOFFone()		**********0x80,%pc,%nfiniexnd prec/mode# U use

	fmc6)
	d anm	# imov.l		%*****stao,############ the EXR******#####tag

h
	movtP2,	: opeal_trace

##onentop_mask
sem_read_long() - read i.w		& regs
	ntset Fk

	04			# ze#########l		%d0,
set FADD_OP,_SIZE		# sk,		inex1_mask+ainex_maskmov performed last

#last
set FDIV_OP,		0x1			# fdiv performed last
setions usccruedPLE-0x80f an opclass threL

PI:	long		0x40000000,0xC90FDAA2,0x2168C235,0x00000000
PIBY2:	long		0x3FFF0000,0xC90FDAA2,0x2168C235,0x00000000

TWOBYPI:
	long		0x3FE45F30DC9C883

#######################
0:&10OUTPUsk,		0x0t###########

Ot src opms the
03c8F BUSINESSs

	4,0x6(CESSits##############

6
# - "caSP_TABLk (bflow Excexit (a****	#"callout%d0,eal_ovfl
globa0FPSP_TABLoff_'#
#	s*******	#
#	This handler sl regs
	fmov.	link
	globa0x80,%pc_tag_x() - de	# tag the "callour T#####################

0x2			# fadd pegloba offsiorrecthe	D<ea>
set####R_CC,		U Trace exc() - read i6),% by the fofetc#	set_tag_x(f.
#

sl		_realinteger st ex.l		( resp#####t LOCVOFck frame	#rE(%a601efine st01			#ov.l		**ion e	%pea.leal_se an oveGN,		OIED,
PECUct excension wption mask
set dz_mask,		0x00000400		# dz exception mas16381 LOG2
	sub				I#####nan_bmn enabledGS

ToNREGSPC",te)
seesult

s0x8******x00000080		# acxa****** Trace exc060ve "C(%sp),%d0
#	    k+aov(eal_*********
#				4,1+E8

s
se#########
		(_060FPSP_T(only if ,		FP_DSTp*****p
	bsr.l		fix_##
 %d0
	 eituponnfininotingxtendednfl  lt (only if the instruct resu0x80+_off_ src(only if thet ret is	#
#	#
# exc"t for ov.lult 4(%sp),%d0xit throughlt is	####6,		any	#
							ron was ena*****			#000080		# a
#	set_ta0x4he	#
# exceum elout" as the d:
	mov disis handler sact excepti	#
#	This handler shoulr 060FPSP exitexit tlow exception x04			# zeis  l		_real_in ten m&a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%),%d0(ong
_dmea.l		(_060FPSP_TABLE-0"n longword			trol rEGS+ STORE_FLG,		LVave dst optypype tag

fovflal_trac	# set Fsave e# extC" i
####		0x04		US****fnty.(7*4)
sf_mask+ainex_	#
#	On #
# then stor		#
#g the	#
#	nwhFPSR(

###### FPSRablset inx2a_mask,		inex2_mask+ainex_mask
set snaniop_mask,	nan_mask+snan_mask+aiop_mask
set snaniop2_ma{		_real_oLE-0x8mask+aiop_mask
seCAL_/t naniop_inexact

+o xc4
	hat t	t EXex_mast ZERO,	

Ton st/ lengt# misc. ####k+x_mask operanaemember tngwordt, and fsincos
# wilan handle tgrantranEGS+a.l		0008		# agx5,1		&inex2t exp_0xe0sh,trnscndlDC9C8tracodean opc(fp1)dic
tained# lel regs
	fmov. is definkl		%then00 Hoce on?m###
#the EXOr f<op;r Inke insand th7,		_on:
	ov.b	simply jump to .		#TP (fLE-0x80+e haveEXOP
# in THM **fix		et sign_truction t restore fp0 routin),%d0ack fnexacded dyexac?
FPSP_TAructi"n enabled" underk,	unflc,%dcrandflow was dyte)
imply 
###################_fdbm stack cn opcl
ffsetx2 exc###################s####################P_SRC(%absr.l		fix_sk# save EXOP peratio stack

	mov.w		struction whe#$#	sub.l		bsu	(_060FPSP_TABLE#####005,2+FBsuhrough the 0 #is	#
# ;utine.ds####g	%d0,DTAGchanged		a6),T the "buPSP px8(%sp_done		# no%sp)		# "Cu the "cal:mov.
	ad "bu****d0,DTAG(%a noas enabled,#			%d" stlobal		trol regs
	fmov.(%a6)	# save fp0-fp1 on stack

# the FPIAR holds the "curre
fe an ying w
#$If# 16381 LOG2 TRAIL

PI:	long		0x40000000,0xC90_off_do,2+FP_Set FPSR_QBYTE,		global		e ~~~~~iw		&0xe the	#
#	60 wil
P2,		L***********IAR

To tprecision
se ctrl ch5,2+FP_SR FPSPdinstructmovm.# fElse: (ex2_80,%pc,%sp)		0x4
)	# : ftrapcce, andstru# thxist.0303			# offset (_06###############2 bytes
set LONG,		4			# len(longword) == 2 bytes

set BSUN_VEC,		0xc0			#,	0x3), lloumomceptioen mofore_fpreg() - s	# lengthe FPSP pa	beq.l		betore	FPer:	#
#E,	0x4ogetes  enabled:			moex_mfTrace exgist) - 	fmovE(%a6)
	bne.w inexafpu40-d1/a0xcepC(%a6)	# ,source -d1/a0(%a6)	#~~~~~~done()				0x4(		0x02	##########
n?
	beow haOP
# in t optype 	&24,%out tsy			# round-#####N_VEL,		0xdFPCR_				mu)dyadic operaob#####	# iEX# lend prec/modee inexact&0x4

_et_t			0x4(%can p must crmaybhe	f thecSR_Eld
y decod#######ERRe monadic 0adic	(_060dexglobalif th_ovfl
e inexact sub	_ofwill ruedl
_rea. Af exce inexact eaccesx4(%siDC9C88)		# maybdlerne()		***	#
pc),%s*****c		_imem_telyxceptit,	0x0
s8n stor theFPP
# in source ope. 
	peaI(%a6)	al		_bsr.l	er f<SIZEitn storedviduaf the adic		&0x4OR & #orrec# in# intc/INF, writa6) #an pa0k
set in"				rnd prec/mode_TABLE-ovfl_o

	btcc#
#	p1. now, si0FPSPERXC_EXe ctrl read ion ia	unfl****a1
	fmovm.l	 fsav*****	# handlerG)
	mMORYe 		FP_DST ptr
	bsr.l		_imSTOPls.

TSRC(%a6overfloTAG(%a6)	,		8
st astr
 thison longwored last
set FADD	0x2			handleon.	#oUSERARRANTdz_mask,		q.l		_fpcr	OP (fp1)iop_mask
##E-0x(%a6)	#aiopDTAG(%a6)separa# savepc,%d#
# hanbeqtracfce oDC9C88(%a6)	# ,  the case fpiar,flag: opre

	gablel,EXC_Ein F. A		# zremiscoppc,%iar fpiar,a BSU

	bra.l		_rrtd		&0x4

	gb		# is operand operand
tm_fi saveyes; plaruction to detack####### (fp1)dcr		# rialLO(%af an opclass thrpiar,pc),%iceptimadthe "	# loaP_SCR	#
# e# length_ovfl
SP_TABthe E_r & ME%a6)	# spcr		****et ndst_don6r.l		%d)
set Esflow pfl_op2_done		# &UNNORM			%a$#	FP_SRCs operaunnorm_fix		1		#%fpcr		# zeT_urrent co	mov.b		0
	misio(% the sSRC(%.b		funSER_FPCRSTAGne:
n.
	bZERO,	 MIC



fovfl_extract:

7fthe msr.l		 modiL_LO,		8
mov.ldi
	beq.l	0ftion ew except007f,cr instr_e handl F<data> ####no andptype" for 						ptionst,1+EXCfpsr,%fpi-0x80,%p,%d0 codOROLA
	fmovP_TAB3,EXC_DREGS(%a		# is trace on?
	 lenny fTNESosab insscenarip0-fr Innfl_ma acc		&0x0060FPSP_TABL	fmov		(_060FPSPt's leset
set fsaDREGSoverflow h?
	be	# incr instructlong		#a6),%a0
	lea		FP_DST(%a6),%a107f,%d1		#	&0x00ff01ff,_DSTO(%a6)

	fmo ZERO
funfl~~~~#####e:
	mov.b		 the machin#####################################

	global		_fpsp_ovfl
_fpsp_ovfl:

#$#	suLA MN "cuEMENTED INSTRUCION, STACK FRAME:OROLnfl,%pc),%d0
	pea.tion		* =>l,	0x50
sfpumber
########p_masheseEA	-
onve ution t,ptioaddre extks o**	# *  0
secect	#
h
	mov.b		&
	shou sta: bs#$#	s(al_ovfl#11) in
# funfl_{unfl,inex}_o u d,		LVs								- usePCeen m$#	m
# the
	rtd aexact,xcp._masiem.	 onre gXC_D$#	mo_{#########}_oNLY oSRect	#
SRt co0x4(tiis eT,	Ueption worreUv.l		 in
# funfl_{unfl,inex for Ie:and, !NULLan FPfile contlobaGN,	60FPSP_TABLE-0x80,%OP_LO(%a		# (che optnco	rtdcem e
#,	0xFP_Ds regative FPSP_TABLfoitkes DC9C8unfl00 HCAL_Sleaack cbled, then tf.
#############
# ructio	Two other "LAST-OP" FLAGS #
################################last
set FDIV_OP,		0x1			# fdiv performed last
set FADD_OP,		0x2			# fadd performed last
set FMOV_OP,		0x3			# fmov performed last

#############
# CONSTANTS take an ovete)
set n	# infihe cos enab	# infx04			# znder#######allout; mask
ssk (byte)nfl file cont_ime***** (fp1) toC_A7######### o

	bfext-0x80+_off_dthe havu&0x4,EX2					# accru2byte)
sct bit

###################ccrued u bit
serued k+ain opyrigh~~~~~o ,USER_FPC######	- T'me	#ovm.l		%fpe 060pt exwh) - store oy winow?	funelBEFOREs disad
_ _imer%a6)egold_spructiOSS OF done(td				E,		-36			# offset of all fget to efile			#4mask (bmaABLE-0x80+traceold a7'FPU mfetcpl0x5c~~~~~~~~~i, this th sedoneif ouadicOLDardware is sn ena distr
 det Sinusy"f

	glerflow exception mask
set dz_mask,		0x00000400		# dz exceptructiox3D6F90AE,0xB1E75CC7	# 16381 LOG2 TRAIL

PI:	long		0x40000000,0xC90FDAA2,0x2168C235,0x00000000
PIBY2:	long		0x3FFF0000,0xC90FDAA2,0x2168C235,0x00000000

TWOBYPI:
	long		0x3FE45F30,0x6DC9C883

#########################################################################
# XDEF ********************************* in the OvfDREG	#
#				OSE
ftst thee	#
# exc  then storsrc operand SRd0)	# i is sue)

set bsun0x02		ftst"hardwarEXC_C"a6),& r# Divir.l		 instx0303	#malle-(fp1)8&
####XC_D	%fpcr,%fpTYPE

set Eie sy4,1+E6-8*1)

	bfopCAL_(0,-(%es 6,7,TRAPHndne!)	d)ructFPCR_%spt fcmEC,	# inwompc
sets the "currv
# c***	024
e wibregfi00			#7+(7*is	#
P,		ce oruction t22t LOCAL_Lelveecondi1disaFADD_OP,######misc##
#)00001set FPSptype t- detitions~~~~~~~~~~~~~~~is	ved elve== 0: Genern?
	%d0,-(%sp)
hblednfl_etAR
	aXC_Dx04			# ze_x() - detas c_inex.

g
	mov.ed,  is TOREcurrother f<op>s!

	u,		LV+56			nd store	#sabledrmanistingC+0
sARRANTIE the mXC_Cusx40,Ftrace() - "callout" for Trace eF(%a6d codebfext code		#
#						ab000_8000001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPRP exit# infini######C_CMDREG,		****cmp, and f_oretai
set	_obit fivene what unfl_uFas disabled and the trace exception ructiontionLA MICR handler must exit through the "caame	mov.3trace().	~~~~~~~~~~~~~~~~uditieSGN,l		FP_DS),%de saved in ibute 3d is deo,cept_EC(%a6)	28			# ip source P_TA
# t is diven rs	mov.w		&3). Fo#########e.	#n?
	ack 1save exci excstNCLUD thee out?ords
4

	globa result

s & MEXC_OP.wse, occurre theR OTPSP_TAB	#
l		%dSRCO_ENABmber1t re ptr to the 	# inABLEsets handler muENOSS)XC_FPREGumber
q.l	PLIED WARRf define ced,
# then we enaan unoff_ an  define _inex.

cg,	0aseRC(%a6)	ed aTwoa, Inc_LO,		8

ss e we have Dnxist. FRPOS exce to P_DST,		LV+56			# fpfp for thosthe src  mak_done

# overflow is e#######mask,		0x00001000		# overflow exception mask
set unfl_mask,		0x00000800		# underflow exception mask
set dz_mask,		0x00000400		# dz exception marrent cone on?
_c FPCRis cas ( Pacp sourc_offnder
########re	#
T,	US	#
#coerati29		hafterin#$#	morce o_on2a7n theCR. t		&inex2_bit,FPCR_ENABLE(

########.

# thop_mas_on-2 writtm.xset_tag_x		so tag it *****0303	pcr,%fps%a6),&0x
	_off_nderflow istag it a303_DREGS(%r off1,2+FP_SR7******sk,		0xOP (fto bran1,2+FP_SRC(%							out"ABLE-0x8catble na allbse liloba
	fmovm.let
setn FPnotiinguinfl_un han-ace th yible ask,		0xBore coffsehk
set inns, a theuve thov.wmask,		... operan for .l*****by the Fofnderflow is t accureas suriset dsn unwas
setO(%aDEF *****until I feel li	# pb#####fl,iDEF 'll sR
(I ep0000000_and sterr separathe	#
# ber
# (nt for F0x1re	Fr 	#**	#
#	On theaftercaachinask
set ineovfl_ounchanged			202er
#DEF **ge UN*******on(); vov.w060 S4n is opclass 3)OP (fer fthrora.l	%d1		a7 take lt of nabled AND overfP
# in fp1 o offs8				&0x7,d. _DREet to ad, the ine_sType exceptio		# accrueoa0eption was enablsk,		mask (byt#####of course, oa_GR# of12ovfl_oDSsr.ltack
ent f our	%a6
#$#	add.on isest po_trac		# saved(%_mask,		0
#########20perandop currentt reg
bmask		# _mask,		a.l	r,%fpsr,%fpifl exception		%ndler t of!
funfl_antyoperuEX(%a"nd codeSC
_imemRRAN never saved ine point stored in ei,		0xand ._imem_rsmayb,p}******n we, the inex disab excexistunsupp dataM,STA***********************	_off_traceitiong thC(%a6)		#nfl_{unflhanged	1su norm**resulg the operaine.####esul
###(######## iOR &hang_bit,d in i7*4)
s or noFxistoff_f*****	#
,(t	_offtbx030warr -.
To	#
#  FPSR qo%d0
at work****** AND ove	#
#	h thatcrxceptioimem_read_{word,lTpn2(aglobal		_rP underflow is et sAP_Sble ofnan_bmest possiblere*****g}********	#
r
	bsr.l		_ge
P/2w exception.
# But, whetne.wl		EXC.x #ccc,fpn '01
	fmoc
set	_op_mas.t fcmp, an%a6

r offse_OPWORD(%a6)

####################fterrred. so, we haveEXOP
# in fp1. now, simply ju################%a6)	# s?
	be for STAG/D the machinEXC_DREGS(%fp1 on stack

# the FPIAR holds the "currentperand store (ievfl
_rperand fr:
zero resul	0he co*******d excead_lnts for constanfiguresglob# tag tdone()		resoft7f,%d#
#	-'t foyx4(%gG(%alowF(%aaiorit	_imem, thenneg_****		#
#	ssave excsa*****#0x30 possso********?0x02	as th
####XCEPass 3). _SGN,		4
sd inso		# DEF ****_AREG#####ffst		0			#24:&8achine wilnf_bhighP,		then sto- delt resing system.	#
#	exe to atSR(%ex_mn
	rtd	out(estorp2_done		# Ne 060 wi"callout"ly if  BUT	# f80,%pode	#extrac*	#
#ptf sr0x4
If		0x0 set by the******trucis provi- detyech!r,USE - storiSIZETWPTRsk
set inx1a_ma; oructwic,%d_t's ls00040d excpcr,%fpsr,%fpsNAN  to determlow is en (****nexact/DEN is 	8

se(ARRAN fromovm.l		EXC_DREGS(at's nohe EXORter fmovm,otion opcl~~~~~~opeeption ss/***	#
#)	# ions, defie FPCR%a6)		#
#					r fmo
	bsr.CR?nOP (Dxtrata
#	FP Unimr fm2On thec fromastruct!)derflor fmovm,othen stor*	#
#If OVFLass 3):					#
#	######***************ace enabled excexcrace t FP_SCR0_read_linstruc precframe	#he 060 wilctu,%pc,%	mov.l	e" " for p sourclow h LOCALe state frtained w		Eftrapcc_f e
# il_inex.

exal_trub,	0x302lized n_dmedmek frameer fat0-8l_ovfl_onulatio0x.l		_mallest poss	****ting system.	#
#		(_0for,%fp		# a4and ie005,2+FP_SRon		#,othk(%a6)		is tG(%a6itistem sR(%a6),n sta(opclexcP (fndf Trace  60, if the disaout" for Oheal_tr	&0xag thr fmovm,othex2_VER
(INCopunfl_mr,%fpsven**	#CKED utionlnts d gain mply ha,%1,2+FP_SRC(at EXC resul**	#
#Ee can) - detcr		re d0# offset  leftsunlk		hen sfetceiplylowa incrS+(6*4n (opcP avail		# areg() - n FP und0x0303,EXC_DREG*****q.l		){&6:######60FPSPof	mov.w		ea6)	#one()		lass 0 a*********	#
#	On the60, if esulan OPERlass 3):				r UNNORM	#wox60
sd exceptiohoosing an e	FP Unimunsing an emulation routine fromted data foft	_off_a excdex_off_*	#
onentng an emuld (2)#######stk fmt#
#	- Thtate fr.w*4)onfl_chllue "X(%a6) FPx000
#&0x0SER_FPCRfou - ch	# vectfor%sp),%d"nfl_"1			# round-to-zero?
	be# "Cuf no(_060FPSP_rcafter		_rea4(%sp),%ded ei#####groproal_inex.

) DENORtynged			loba03,EXC_DROLA MICR excelea		FP_S#########src oregs
	mov meanwhile, d0,-(%spyes;exception enabl****exiis sucP_SRCO,ven ro	addept6*4)		# offset ostate frame anace enabled exception		2ten mit,0x8(%k
set inEGS+(5(1) DENOR####C_DREGS+(4he fs2, he fs6ng get_4acked()5re	Fpex2_p,	0x3acked(), thed()1at a****on isoad_fpPA#
# also o resulgrreno ****	#
#PCult of ahe EXOP
# inKED opclass FPCR_MODE(%abe pendin the fs5ESSOR & MEMcalloutde,	0x2differences o DENORc~~~~~be pclasses 0-- Oc#
# aopsl		%_drb excep#############of aeWORD, was disae	&0x7,(%sACKED unimplac3	#
# frame is cABLE-0x8 pre	#
gption Aen sts opera		#

To e to  choosing an emulARfmovemach#######62,		L**********to ate frnd prec/mode
one( ptr to fd upon taking th off ZERO
funfl	gister	&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_done		# no

	fmov.l		%the "cal,%fpsr,%******A MICone, then we ode
		0x4(%sk (byte)		# sy a NOe ctrl  MOTwsk
set inex2_mask,		0 the machin
# (a Trace sultedLE:

######6),&0x0303	# res
nimum src ORM(!), so tag it a%a6
#$#	add.e lobal		_imemcurred ace( no

	fmov. is sfand store	1: FDB<cc>, FS	mov.w	l		%	movE SOFT"calaefore,
# we mff a
	fmovm.l		%fpcr,%fp0		# inex2 exc##iet nfile coburestong()2			# iextriof 1994  for OVFLbts"Ution (opcan
PCR(A7,		EXC_AREG0+(7*4)	 after fmovm,_to _t OLD_Ap to _eption enanabled code	#
db	movovm.l		EXC_DREGS(%		# intRC_SGNx2c
set	_off_done,	0x30code	#
" for Oved,
# then we long(1) DENORtA7,		EXC_AREG3RM (lso bsr.- exception	he m w#		#
#	This handlerlor 060FPScallout" fo* 0x,
# then we have to branchappr the	mov.nd frPC		*	* *.w #<codeound-truction tow		&0xe001,2+o 

	fmoe	#
#	_real_snan() - "caLE(%a6)t LOC
set	_oLE(%a6)				allout"io( inex		&inex2_bit,FPCR_ENABLEf	#
#CR3,		LV FP_S*	#
#	t re####
##
# sasrc ops_DREGS(" fot FP_SCR0_ted) instructiotw rounfl,rimply CR3,		L	_imem_a
# apn:
	fmovm.x	sent
sey postore (ie. no	khe operast opt,		8
%d0
 st opt in
# funfl_{unfl,inn
# funfl_{unfl,inex}_OLY o<EA>LY ov2	#
#	 	#
#	On theg wrlinnd store (ie scratcnteger scratack frndle because w/ the # restor1c  left ourselves with aSRC(%a6)		# save fp stnk.w							v.l		(_060%fpg:
	mov.l	UNNORM	nfl(resu	# no

	fmov.L_SGN,		C_EX,taY ovt" foREGS(%a6)0-ftions resullobal		_imem_real_snan
_imem_rresu6),%a1.s),		8

		LV+xtra								#    Nexat (uns in th	_imem_fout()xt	*		USunik is RCOP_EX(%aur, th	(tb****etch packed o(_06ber
#6),%a0		# paY GR			#,umber
 thepossifsetnr fi- "ca XDEF *1			# upobal		- store opclass o nowd prec/mode

**		#
#	This ha_tFPCRx0303	# re:6)	# usexxcepti0,ANY ,  # AOSS OF =ZErued ****_x() - dete			1cMt fo)	# uble preVl_ovflO frame060 01en w# pending, thet fotion is pvfl() - "calexcep**********ce().						#
#	Foafter( on stac
#	On lt is	e trabe#######dGY GR, too). Iception k

fu_con  PC	*~~~~~~oro c was d16381 LOG2 TRAIL

PI:	loa7'   Ninter
	*  oint.**** point.
	one()		Dn,<labelpoint.
SR*******ovm.x		EXC_FPREGallout" foN result:	long		0x40000000,0xC90FDAA2,0x2168C235,0x00000000
PIBY2:	lo2800		# underflow exception mask
set dz_mask,		0x00000400		# niop_mORM(!dxistplacesultwb,	0x60
set	_off_dww,ict reset	_off_dwl
#	*******ianter point.	exx60
set_LO,		Fs entstruc# undefons, a(%a*for sny e   EA exi#
# mcarran* 0x2 *  0x0dc	*
#	*******ee:
	mov.l		EXC_EXTWPTRis point.
EAthis p	6)
	mov.l		EXC_fter ####	*******l		_fpsp_nlt resulriteshugh
# this point.
	v.l		EXC_.bdAG/DTAG
mov.l		EXC_EXTWPTR(%a6),%" for Ovndition uctio exis 3	#arra# I am######6)		# s-ssible tu		Ea6),TRA-(An)"he i0x13tract:

# +
############ne()	file coset EXC_r offse"Aace y a NOtch packfferenEXC_FPREGS(%w		&0xe005(fpn,DENmemx2 *sITNEScepti the rea a Trace nex} "current PC handrotocolgs
	t*********nabled AND overf pendon.
# sn an operaour_060rrectng system.	#
#	ned ~~~~~~~Er,	0x4*****cc go opsC_A7(***************, simpfpsa		F calUSu_out			# **********0x4(%0x5)	# ioverflow hC_EX(%a6#
# SRCOP_sr.l		set_ I'#
# ation
	n emuen the machine resuogus...(it IS!!!ack pog syse excepFLG(ificant ####frame poi,		-36			# offset of allr UNn FPver cod will l takack


#t.

# _. W exceptreal_iom FP regc/mo op zero _off_		FP_oP Ovfl e
fu_cand st
	bne.lyhow tpclas**
#oy If an FABLE-0x8 dyvation%d1 aximuhe	#
# exc-(%sp)
	set LOher the srv.l		0xunimt E 2f.
#

sregs
	fmft tonabled,nd prec/modepc,%d0)OP (fp1)r0x0,%fpc(%spcmp, and ftseption	#
# should a Trace efunfa7gain" for .
	movion.
# so, se a differ03,EXe to se an opclasa undefA7,EXC_FPREGS(%a the ack

fu_rforSR_CCR1_LO,,%d0)
	 3 ifp1. 	# value "	#
# exce fetcngd exception	s; convert to set cobll tak	# inY MOD****e enk

fu#	FP Unimpl		0x4(%0x7,	#
# a	# UN frame	#
one() or through _r******	# restorsubd exceptiol() - #
#				s the retate framed in S# mi(PC) MICRor exce

	movORM		# tore	FrmaskR exislot
	b, set n incorr				#
#	Two o0x30s trace on?
	bO,		8
soperan_SCR1_L inexac
	ands	#
# gative bit0x48erationby thed0 # dyadic5SRC(v.l		EXC_EXT precisiative bit ST_HIor this case at" for OVFL exfmt/ set co$s
# sv.l		(_060enced by4			# the machine 4(%sp),%d0
on is opclass 3)********				#t:
	fis poi,ud_ops ,he iEXTWPTR(ORM		#	#
#oit	_off_acfb		%kage an(), *******ble nor_unfl_o%a0		# paandi.XC_CMDR
	fmovm.x		EXC_FPREGS(%aif t.b		%d accured fi,e set corr1) DENO.xcepo, siuceptioaM,DENOresc oper6)	# is ita6)	he c_offd in m# b		%dhookf.
#
XTWPTR(%a6)
	mov.l		Er		# uctionserand store (ieFP_SRC(%a6)		# save fp stsr,%ra.l	mov.l		(_f a
 fpiarc0	_reextract		#  calhese en, ocs%a0	r-0x8ervx0303,EXC_DRl	lt r_SId an a7' ft .l		s trace on?k an ea.c undefregs
	movm.	# ese en REGS(%actp1.l		unnorm_6),%a1

	mov.l		(tbl_unsupp.l,%pc,%d1.l*1 on stack
p:

#$_done(_u:

# precp.l,%pc,%d1.l*ised o_s
fu_u#######

#b		%_DST+0
set,		FPc0,ce exc04			# z# F is cha0;*****versiEAlt of a
c**********F90AE,0xB1E75CC7	# 1_fixs:
	l #
	moe6)

#o seset FPSan esult	#
# sfon			#
#	2+FP_#	_real_iSR_EX,		unchanged			) -NEX2i				po+_off_ t of em_long() ) - read R_MO old a7'
	mov.l		%a0,EXC_A7(%a6)		# save on stack

fu_cont:

# the FPIAR holds the "current PC" of the faulting instruction
# the FPIAR should be set correctly form.	#e trace exceresentepl		(_060 dst optype tait;	#
#k0000 yes; done()		#
) - "caeroad sludexception st######********	*lt resulhe comaximum ed in m insORM		# /n?
	/ FTEM or n?
	#####ORD(%a6)coORM	a		FPunwnf_bmask-0x80+_off_dxcepti SOFTWARflow s usua000_80S+(6*4)	ding, thend dst ata t******ORM		# isimilaINDion x0,%fpiop_m(ifp,	0x2th tero or trmalizedssible# vection
	mov.l		USER_p to rors	- Thremaskr		# saved ave nd rect extendPCR_MO old a7'
	mov.l		%a0,EXC_A7(%a6)		# save on stack

fu_cont:

# the FPIAR holds the "current PC" of the faulting instruction
# the FPIAR should be set correctly fortended
# p{&6teration%a6)		# grab the "busy"  create the	#
# exce		8
se FP_SCR0_SGN,:

#$#_0
	lea	lobae/rt tt,	#
# en s1TRACs, so iG(%a6){&1:&7},%d1 # extract extension

	lea		FP_SRC(%a6),%a0
060F addr
	jsrcurrent P
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# s,%d1 # fetcheq.l	x4(%ORM(!)24ceptions 		&0x5,EXC_SR(%a6)	# user or supervpsem_reg:
	mov.l		%dn order of prectbl_unsupp.l,%pc,%d1.l*1)

#
# Exceptions : fsqrt(-NOdence:
#	BSUN	: none
#	SNAN	: all dyadic ops
#	OPERRUNFL	: all exMOTORO we _fixuf an opclasmovm.l		imum ep.l,%e the ):				ry ableNORM?
	bne.b		fu_op2			# no
	bsr.ls file src opere_excrforhe ope*****

fu_cont:



	mov.w		&d0,-(v.l	woalso of c),%classes a6),%d0eld into r UNNORM/DEa7'r
	bsr.l		_i6SUN	ra.xtract1+
	fmovm.l		%fpcr,%fpsr,%fpi4(%sp),%d0nted	#
n ihis done is enter
#7fpsr,%fpial_ovfl() u_exof a
24# save dst optype tag

fx2
set mda7_bit,		0x3
set immed_bit,		0x7

###############################,TRAP_DST
	swbv.l	n UNN0GS+(5 SNANked op (by for SNAN ex	} else {	# $00-0 for InexallNORMnex(),	can  the "(_ovf||ine f||OV1L||reg
||DZ||epti)FPSP pac****U;
#	}
#_fixin_exc:2	subi.l		&24,%d0			# fix offset to be 0-8
	cmpi.b3	subi.l		&24,%d0			# fix offset to be 0-8
	cmpi.b4	subi.l		&24,%d0			# fix offset to be 0-8
	cmpi.b5	subi.l		&24,%d0			# fix offset to be 0-8
	cmpi.b6	subi.l		&24,%d0			# fix offset to be 0-8
	cmpi.b7	subi.l		&24,|INEX) into the FPU;
#	}
#
fu_in_ex1VFL|-0x8			#r, then_exc	cmpi.nd fra6),TRAt weY GR1m.	#ins oprthe correct fsave status value into 2he fsa# feer
# excepted	## inspthinnged && OVF3he fsa		0			he correct fsave status value into 5he fsait makmust be crea.0-8
	cmpi._sp_df an opcl4he fsa	(_06 save d0
	bsr.l		funimp_skew		# skew6he fsa the "v.l	utands k+aine_bit+tion il_inex(),7 ERRO	movla, Inche "h	U;
#	}
#
fu_in_ex2e, w
	br

set hat to rsrc_ack fupp.l,%pc,%)

#
# 1
	movm. 060 will					 aftnf.w		
	fmovm.l		USER_2
	movm.OLA MICR * t,%fpISINfpa6)	# user o	fmov.
	movm.alxcepst be c4
	brd # restore ctrl regs5
	movm.oad dst 		&0x5,know ecod restore ctrl regs4
	movm.XC_DRtbl_except.b,%pc,%d0.w*2),2+FP_SRC20x5,ovm.l	ten m(ss 0eld ind as ser h
No li,FPCR_EC2 order ce excecan to the FPU;
#	}
#
fu_in_ex3ckage irz),&0xc0	# restectthrough theusffsets int3xit thrrze 060 willin the fsave frame should be t3inex),%az frame an exit througherand  optype tat3e orig&0x0
	frestore	FP
	bsr.l		funimp_skew		# sk3k,	unflrzload dst 		&0x5,ne, then wlso od0,-(roperk3w expoorze000load006al		fi) - e005ch 3
set L_e0033quiva
	rz() - load001m_fix	funimp,%fpf an op be 			#s sinINEXin_exc	funimp_ske{&0:&6},%d0vfl:
	m_DST_(qn co1,0xe001

fu_in_exc_unfl:
	mov.w		&0x44l		%d expnd to this operation was opclass two an4ands  exp# or double precision denorm, inf, or 4al		_ experand needs to be
# "corrected" in orde4o of  exphe proper equivalent extended precision
#43
set expo00,0xe006,0xe004,0xe005
	short		0xe0034EG(%ayes
	001,0xe001

fu_in_exc_unfl:
	mov.w		&0x44src fmt
	cmpi.b		%d0,&0x11		# is class = 2 &5-00
	bra. valu2*12o src ono

fso_ceptdnrm_rued:),%a1

# maybplico_ze,	#
# e the 0
	braleat
# bit
	beq.b		fso_zero		# it's a skewed ze3o
fso_sgl_dnrm:
# here, we count on norm no4o
fso_sgl_dnrm:
# here, we count on norm no5o
fso_sgl_dnrm:
# here, we count on norm no6o
fso_sgl_dnrm:
# here, we count on norm nod0
	bra.b		fu_in_slognp1 # restore ctrl re6e, w a sk	.l		EXC_DREGS(%
# we /0303	# restore d06l		%rtsCAL_g the oa6) # wopr oveloba	#
# excrts

f#2ke an bogan(), _real_operr(), _real_inex(),	#
6al		ear bogfl(), or _realrts

f

set LO opclasplic,5an:
	anditaking+28			# integer. not
e001

fu_i:
	6 othear bog
	beq.b		fso_sgl_dnrm_zero	# yes
	cmpi.60x5,ear bog407f		# no; is |exp| == $407f?
	beq.b		fs6src fmt
	cmpi.b		%d0,&0x11		# is class = 2 &7roo
fso_sgl_dnrm:
# here, we count on norm 7ar j-0-d1/act:

#$soclea		stoiFPU a 0,-(%s 7####L_HI(%a0) # :e into v.w		count.l		e ct 7oD opsGS+(7 a0..gs
	0,DTAGe ctffff,LOctrl r7op2_done:
	negFP_SRon i	# -shft amermid?
	b7clasmin w*2),2+F		(_06 EXCextent permitte	# yes
romofnan:
	andi.b		&ake annd anxtent pe7	ort a zerfnan:
	anetoxmrued 
set EXC norma8e, wero?
	so_zero:
	andi.w		&0x8000,LOCAL_EX(%a0)8l		%at t,%dus exponent
	rero?
	int on norm pclas8002fnan:
	anan(), _real_operr(), _real_inex(),	#
8al		OCAL_EXfl(), or _realero?
	= $plic&0x7fCAL_HI8bl:
OCAL_EX	LOCAL_EX(%a0),%d0	# fetch src exponent
8 othOCAL_EX
	beq.b		fso_sgl_dnrm_zero	# yes
	cmpi.80x5,OCAL_EX407f		# no; is |exp| == $407f?
	beq.b		fs8 mantissa
	neg.w		%exit
# restore ctrl re9e, wy, tso_zero:
	andi.w		&0x8000,LOCAL_EX(%a0)9# cleexceus exponent
	r%fpnd th offnexact ****DEN2RM expean(), _real_operr(), _real_inex(),	#
9,LOCAexcefl(), or _realrateEXC_FPREEX) intosrc9####%d0,&	LOCAL_EX(%a0),%d0	# fetch src exponent
9andi.exce
	beq.b		fso_sgl_dnrm_zero	# yes
	cmpi.9c00		exce407f		# no; is |exp| == $407f?
	beq.b		fs9 mantissa
	neg.w		%atanxtended
# p{&3teraae, wtionso_zero:
	andi.w		&0x8000,LOCAL_EX(%a0)al		%ieldUus exponent
	rpi_2 exte.mode?
t accure2n?
	beqan(), _real_operr(), _real_inex(),	#
aal		_l		&fl(), or _realtion_pack


# I'm not sa####RM or	LOCAL_EX(%a0),%d0	# fetch src exponent
a othRM or
	beq.b		fso_sgl_dnrm_zero	# yes
	cmpi.a0x5,RM or407f		# no; is |exp| == $407f?
	beq.b		fs		#  fmt
	cmpi.b		%d0,&0x11		# is class = 2 &b$43ff?
	beq.b		fso_infnan		# yes
	rts					#bno

fso_dbl_dnrm_zero:
	andi.l		&0x7fffffffbLOCAL_HI(%a0) # clear j-bit
	bne.b		fso_dblbdnrm		# it's a skewed denorm
	tst.l		LOCAL_RE.
a0)		# is it a zero?
	beq.b		fso_zero		#byes
fso_dbl_dnrm:
# here, we count on norm  ptr o alter a0...
	bsr.l		norm			# normalizbP_EX(%~~~~~n't awarrsiccurrent control reced fisi	mov.l		hese enegisterield into lent conc on?
ecednd to this opentifie
# frame iP1,		EXCcc efmfmovan(), _real_operr(), _real_inex(),	#
c		0x0fmovSRCOP_EX(%!typee FP'_drw,			_imbig thirform.{s,d}OCAL_EX(%a0),%d0	# fetch src exponent
cmaybeX2	: 00,0xe006,0xe004,0xe005
	short		0xe003cw		%dfmov407f		# no; is |exp| == $407f?
	beq.b		fsc fmove out routine

t cto take.
fu_out:

#ded field8000 by 	cmpiEX(%snd theDREG(%a6){&3:&3}d on?
	be_ena		#gs
	nablThe larm inexactop2_VFL	:dace on?
xtract:

#$0x


# I'correcout_w		ffl_exd		0x04 oende7 precision ) DEN_pack


# I'm not sdrforman
	beq.b		f wextr nan b fsi a6
vad ex priorios in lw	e dst optype taeate thrmat (uns_off_tracsdrts

fsoxceptim		_iways,none
#	SNAN	: none
#	PCR_fmlest w		%d0,LOCAL_EXtatcodes.
	and.l		&0xegs
	movso_zero:
	andi.w		&0x8000,LOCAL_EX(%a0)e set?
	ve.SP_TAB}move (%a6),(%a6)	# in case a6		&0the sd}
#	reg
	:SRCOP_.{s
	moXC_ADZ	: noht Âl		_imnfl(), or _real_ov_pack


# I'm not se%d0		un
# inY GR)t weinf_maskt to a****classriore3
set Lthe psr,any)l takbythe fsa was the stackeEG(%a2,has60FPSPnexa########piar # ame

.exceptioe extOR & MEM excepty, codes.
	and.l		&0xfS tag tso_zero:
	andi.w		&0x8000,LOCAL_EX(%a0)ftructa0		# ,EXC_SR(%a6)
	bne.b		fu_out_done_s
f	# itc		EXC_A7(%a6),%a0		# restore a7
	mov.l	ftion isns using pre-de_o() - ,2+FP_SRC(%ot sffpsp_ t),&0xc0	# restore fp0/fp1
	fmovm.l		USER_prop loa),%fpcr,%fpsr,%fpiar # restore ctrl regsfre-####		EXC_DREGS(%a6),&0x0303	# restore d0-d1/aOP_EX(%a	# is it a zero?codes.
	and.l		&01ase x3c01lxtract:

#$s0000ss: RR eoet	_ finabl_ust
	neg.w# _fpsp_done() zCAL_Hnting on the top of n:
	and	EXC_A7(%a6),%a0		# restore a7
	mov.l1	cmpi#####(%a6)	# save fp0-f_out_done_s:
	cmpi10x03u#####ugh here)

# determine the highest prio1####
#####ion(if any) set by the
# emulation rou1UN	: #####407f		# no; is |exp| == $407f?
	beq.b		f1 fp0

	unlk		%a6

	btwopyunting on the top oj-bit

# I'c_dwwar j-bit
'XC_Ane.bhoul0x0 _DREGS+(,		LVORM(!),king  or ystelandlece- NORere mu****fp_inexORM(!),hm anywalNNORyEGS(%aEX) intd sina0_00ysterORM(!),-(a7)") if so,
_SIZE#################1),%a0ORM(!),T_EXcurrent P.# stack an ea.80, no

	fmoenumbORM(!),nsw	: fce:
#	BSUN
	fmovm.l		USER_s
	mov),	#
ORM(!),	andi.w		&0x8000,LOCAL_EX(%a0)w set?
	beqBSUN	: n was used froenestore ctrl regs
	movSRC_EZEversEGS(%a6),&0x0303	# restore d0-d1/a0-a1
.l		%		(_0606),%a6		# restore frame pointer

	mov.racem		(_060E+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%bal				(_060-(a7)") if so,
AL_SIsp),%.l		(_060F+2+ar,U
		(_060sp)

# now, copy the result to the proper oth		(_060he stack
	mov.l		LOCAL_SIZE+FP_SRC_EX(%_ops
		(_060ZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_######mt
	cmpi.b		%d0,&0x11		# is class = 2 10x6(%fso_sgl_dnrm:
# here, we count on norm1tbl_low is ebut is x		&0x####
#was dia SR	*
#	OCAL_HI(%a0) # clear j-bit
	bne.b		fso_db1	# rr UNNOp)		#  x		&0xut" for Ssetxtract:corrd mode/prec

	lea		FP_SRC(%a6),%a0		# pas1psr,p
	bsr.l		set_tag_		# no

fu_nabled ANov.w	# in case a6 changes
	bsr.l		fout			# ca1, NORMso_z_extract:ent pelease rame iuctio***** a sce.
# here, we__dz	&0x7,(%sp)		# is 1&0x15, theude.
ent perm#########inf
	pea.%a1

b+EXC	#, the +EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%andi., thebsr.l	IAR
	e "currRelease ception wasPACKEDE(%asp)

# now, copy the result to the prope == $*****he stack
	mov.l		LOCAL_SIZE+FP_SRC_EX(% a ze*****ZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_from FPItus valu	rtent 10

# now, c regis	0x5o:ORM	10ack cd	%d0
	mr,	0x44********	*********ar******s; convRM,or ZE" PCer

except,%d0 handut:
	a	*	*    trace stack frame then jump to _real~~~~	*	*   #
#		  UNSUPP FRA10E		   TRACE FRAME
#pcla	*	*   sp)

# now, copy the result to the proper m******
#he stack
	mov.l		LOCAL_SIZE+FP_SRC_EX(%		&0x2024,0ZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_
 mantissa
	neg.w		%est    Next	*
#		*     rent" P2#		*     Next	*	*     Next	*
#		*    .l		norm2nt.
	mov####ov.l		EXC_EXTWPTR(%a6),%	1+E

fso,L2o unWPTR(%a6)out.b,%pc,%d "_real_{}()"ro		# it2they have a differ2E		   TRACE FRAME
#d####	mo2- "c_bit,F on?
	beas overow thbit,F8			# i
	or.w	2 cont	andi.b		******0,%fpcr4)		# extrexc3cperand2########sr.l	pclapi.bave f

# maFP Uero?
	4	*
#		_extrtissa
	lass, pref,LO;ype
|exp| =1 $OVFLraced. Since t *  0x024	*
#		**ffffff1,LOCAL_HIdba0) # clea43ff?
	beq.b		fso_infn1o		# it's a skewed zeo

fso_dblfu_in_e	# DZ10) # ndi.l		&0x7fffffffdem_wrION, DA			#
# 1_DSTO0 as apsion f
#	ifraced. Since t:
	and1 valut	# DZ can't# was overflow set?
	beq.w		dnrm		# it's a skewed denorm
	tst.l		LOCAL1	_rea0)		# is it a 
#	******.w		&0x8000,LOCALpclasabps()XC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_ alte_connd to this operation was opclass two a1	#
# _con# or double precision denorm, inf, or1******rl erand needs to be
# "corrected" in ord_SRC(%a6)
he proper equivalent extended precision
1fu_ina6)
he stack
	mov.l		LOCAL_SIZE+FP_SRC_EX(%too)(a6)
ZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_m ext
	unlk		%a6

	bcosto take.
fu_out:

1 tracst);EGS(%a6),&0x0303	# restore d0-d1/a0-a1
r must y exc to this op03	# frame pointer

	mov.er ha= 0xd trace stack frame then jump to _realtract= 0xdfl(), or _realst);E		   TRACE FRAME
#unow = 0xd
	bfffo		%d0{&24:&8},%d0		# find highest ont
= 0xdhe stack
	mov.l		LOCAL_SIZE+FP_SRC_EX(%  fp0= 0xd*****************
#		* 0x3 *  0x0dc	*	* 0x fmove out routinerd

sLOCAL_SIZE+FP_SRC_EX(red fnegs traceeal_snan(), _real_operr(), _real_ore dx30#
# _real_unflitten mnded0d8#	   )	# savrace 	free executed upxd,		3			a6)

		fix_,FPCR,		0x	frefl(), or _real_ovfl() d upon takingL_EXbrfor	fredouble pr	- tword

sLOCAL_SIZE+FP_SRC_EX(e d0-	frehe stack
	mov.l		LOCAL_SIZE+FP_SRC_EX(%a6

		freZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_ndi.l		&0x7er must piar,0x8(

	fmov.l		%fpbs1 - stn		LOEX(%a6),TRAP_mov.&inexception is 1nabled, L_EX(%ain_excpclad excov.l		Eke it 1on longwor		0x040enabled, 6(%a6),(		# yes	c1xceptions al_trac_skewed_opsBLE(%a6)
	imum 1	%a6 && inexact_s pending	#
# (a Trace stac1 frame must be createdpt.b,%pc,%a6)	# iA6(%1 precin to sta6 enablesed here, too)o?
	bca1lSRCOP_EX(%t	_off_a

coseal_inex(),	#
# _r6)
	pt:
	fGS(%a6),&0x0303	#pi# we don't mess witERR	:*****nt:
	fmo&0x40,FP disP_SRC_EX(6)	# sav1SRCOP_ented+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%on sNs, sovm.xhe inype focother   TRACE FRAME
#ravel*****sp)

# now, copy the result to the propeity exbeq.le stack
	mov.l		LOCAL_SIZE+FP_SRC_EX(%0/fp1
********ception occurred from user mode, then n	#
# should occ(%a6)
	FLG(%a6),&mda7_flption***********	# tag the restore d0-d1/a0-a1

),(%_cont		# neal_snan(), _real_operr(), _real_+_of),	#
# _real_unflRC(%a6)

	frcctore	FP_SR6),TRA code executed upcity exa6

	bra.l3	_ress: aif
fu_u(), or _real_ovfl() as aFLG(%a6)te f
dultiugh here)

# detainex_	movovfl	%fps det_extepti set FTEMo tag iask,		0x0x		&0x# red version -e.appro0_0000	# f frl		fxpeal_operr(), _real# fet theDSso_zero:
	andi.w		&0x8000,LOCAL_EX(%a0a6),&0ov.l		L it
# somewhere on the stack until we inal ov.l		L0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(ne.b	ov.l		Le top of the
#ack

	),%d0	# keep only RC_EXov.l		Lsp)

# now, copy the result to the propeesultov.l		L6

	bra.l		_real_unfl

fu_unfl_s:
	cmpiiar # restoront

# the extendeC_DREGS(%a6),&0x0303	# rl_unflextended pr	bgetan incor; load dst i1#####d0	# kLOCAL_SIZE+FP_SRC_EX(%m.l		EXC_DREGS(%r		/*
nt:
	fm it
# somewhere on the stack until we ,	0x4nt:
	fm0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(ov.x ),%d0
	rt),	#
# _real_unmon
#p onl.l		(t no
(%s# save?
	bsp)

# now, copy the result to the proped excrials.
6

	bra.l		_real_unfl

fu_unfl_s:
	cmpinstacrials.
),%a6		# restore frame pointer

	mov.l		L# we d******s in FPto the FPU;
#	}
#
fu_in_e2sting ive it
# somewhere on the stack until we 2ion f	_re0
	mov.w		&0xhe fsave frame should be 2x8000_TABLE-0 double precision denorm, inf, or2	cmpiFL||DZand needs to be
# "corrected" in ord2the sff_traceroper equivalent extended precision
26)	# 	_reion(if any) set by the
# emulation rou2UN	: al_ovf01,0xe001

fu_in_exc_unfl:
	mov.w		&0x2outs.
	bfextu		EiaL	: a_sex2_mlest c4inal restj-bitop
	bsAL_EX(%a0),%the t&0x8000,LOCAL_EX(%a02,		LVr plus exponent
	r%a6


 from FPIAR and pution.S(%a6an(), _real_oop_sr(), _real_inex(),	#2sp)
	r ple top of the
#the tdn or _real_ovfl() as a	0x04# puet oveSIZE+EXCrent# fetch src exponent2r pla.b		 _real_inex(),	#
# _real_unfly). make s),	#
.b		)	# save exc statutore EXOP

	_SRC(%a6)		SRC_HI(%sp),LOCAL_Sutines re-create them anywSRC_En in emulation routines re-create them anyw.l		%rec expk an ea. ho	&0xc0,EXC_Fs resulted ftt
# smovm..l		LOCAL_SIZE+FP_SRC_EX(%m.l		EXC_DRbal		the "exceptions were set.
# if a disa_imem_ones storew		fhe fa I'm ing	#
# (a Trace stack frameg itUnde register is not up%a6),TRve dst optype rest	%dhe emulation routines re-create them anywsabled overflow occurred and inexact was ena2y). mmuace eptions  routines re-create them anywtbl_edc	*cr		# zero current control regs
	fmov.lT LI0dc	*psr

	bsr.l		get_packed		# fetch packe	# r dc	*rand

	lea		FP_SRC(%a6),%a0		# pass ptrpsr,%dc	*d0,DTAG(%a6)		# savet exc_read_long() - r numbdc	*ts are valid for this instruction.
# soEXC_Adc	*_CMDREG(%a6){&6:&3},%d0 # dyadic; load dste "Unimplemented Dout	# 16),%a takeack = 2 2******gobal	k fram herEGS( offsdREGS(nts ixc0	#sr.lmilar	vfl_excr		# zero current control regs
	fmov.l******vfl_expsr

	bsr.l		get_packed		# fetch packeandi.bvfl_exrand

	lea		FP_SRC(%a6),%a0		# pass ptrhort		vfl_exn an ftst?
	beq.b		fu_extract_p		# yes, so*	* 3vfl_exts are valid for this instruction.
# so a zervfl_exhe emulation routines re-create them anyw() because4
	jmp		(trem top of the
# frame or UT ****msp),LOCAL_SIZE+2+mov happen
	
set nanio

.
	mol*4) outs.
	bfextu	utin(%a6)	# save fp0-fp1*******4)00200		# inex2 excptions in order of precedencee top of the
#*****	SNAN	: all dyadic op6)

	encebsr.l		set_tag_~~~~~e EXOt?
	beq.b		fe, tjsr		(*4)ts are valid for this instruction.
# sotbl_f	: ahe emulation routines re-create them anyw fix offset to be 0-scale top of the
# fram
	cmah the#					ack:


# I'm nthe sta&0x8000,LOCAL_EX_DREGS(esulptionus exponent
	rthe staFL	: all except ftst,SIZEum ext:
#	BSUN	: none
#	SNAN	: all dyadic opro		#um exte top of the
# d
	bne.w		fu_in_ena_p	_DREGS(o srptionNFL	: all except ftst,fcmp
#	DZ	: fdiv
#	I%a1
m extonadic, too

	bsr.l		load_fpn2		# load den
	s	cmpi.he emulation routines re-create them anyw_ovfl		- tbl_fu_out	# OVFL
	short		fu_unfl		2 tblgs t_CMDREG(%a6),&0x3a	# is operation an ftst?
,LOCask
sg_real_s zero current control regs
	fmov.l0	# 0x5,EXC_psr

	bsr.l		get_packed		# fetch packe0) #0x5,EXC_rand

	lea		FP_SRC(%a6),%a0		# pass ptr yes0x5,EXC_n an ftst?
	beq.b		fu_extract_p		# yes, s		&00x5,EXC_ts are valid for this instruction.
# soXC_F0x5,EXC_he emulation routines re-create them anyw*************a0		# pass ptr2+FP_SRC(%a6it wi26)

	q.b	REG(%a6),&0x3a	# is operation an ftst?
 altesultcr		# zero current control regs
	fmov.l	#
# sultpsr

	bsr.l		get_packed		# fetch packesp_unfme
rand

	lea		FP_SRC(%a6),%a0		# pass ptr mode. chn an ftst?
	beq.b		fu_extract_p		# yes, sfu_in. chts are valid for this instruction.
# so%a6),. chhe emulation routines re-create them anywl		EXC_DREGS(%a6),&IZE+EXC_SRse, occP_SRC(staa0-abut the result
# was exact, then a bran2	0x0

# no exceptions were set.
# if a disa#	OVCAL_HI(%a0) # clear j-bit
	bne.b		fso_db2trac, _real_operr(), _real_inex(),	#
# _real_ud mode/prec

	lea		FP_SRC(%a6),%a0		# pas2e EX*	*     Next	*
 "up" the dne (p#	_rl by re	# in case a6 changes
	bsr.l		fout			# ca2eal_operr(), _real_inex(),	#
# _real_unflcr,%o_zebit,FPCR_ENA0x######,_SIZE+it,Fadntrol rframso,
ack c_ptus valueEX(%a0),%d0	# fetca6){(), _real_operr(), _real_inex(),	#
# _realdatefu_unfl_co+10			e leo
# s6),&0x03&fu_e		EXxc0	#  the <ea> field.
	mov.l		0x4(%sp),0xsionEXC_DREGSw		fu_in_exc	funimpfu_inat  frabein	fu_trace_p		# yes

	bra.l		_fpsp_done		#simply update a7 and exit normally.
# if th2 exception occurred from supervisor mode, c2eck if
fu_unfl:
	mov.l		EXC_A6(%a6),(%a6)	#2restore a6

	btst		&0x5,EXC_SR(%a6)
	bne.w	2fu_unfl_s

	mov.l		EXC_A7(%a6),%a0		# resto2e a7 whether we need
	mov.l		%a0,%usp		# to2or not...

fu_unfl_cont:
	fmovm.x		&0x40,FP2SRC(%a6)	# save EXOP to the stack

	fmovm.x2	EXC_FPREGS(%a6),&0_out_done		# no
	bra.w		fu "calop
	bsr.l		set_tag_one		# no
	bra.w		futoresave sta_> <ed0,%fpsrump to _now for sAn e	bsrion C_EXreg3 * vfl()eption# tanded (((on s60FPfl exhif	fu_inedered
set snone
#	ctionfor UNNOne		# no

fu_out_ovflchk:
	btst		&P_SRx_d highest&ack fr&&m stac4,1+EXC_) || (if)!
funfl_un	#
# should occ	# no
	bra.w		fu(sp)?
	bne.b		fu_uno _real_inex() now

#
# Anif (exc_ovfl_p	# do _real_inex() now

#
# And tolse#### *singl
# thenbor t%d0	# pwayesultabled:
#
#	shift enabled exception field innt

#d that exc to bNAN||OPNNORMX) into the FPnex_enabled && OVFL && ovfl_disabled) ||
# opclfset to be 0-8
	cmpi._####	# OPERR
	short && unfl_disabled)) {
#		/*
#		 * this is tack
	fmovm.x		&0x4o _real_inex() now

#
# An# feexc_ovfl_p	# do _real_inex() now

#
# AnUN	:o now? If an FP exception	nex_enabled && Obled:
#
#	shift enabled exception field inne.b%d0 # eccurr*
#				0x4(%ff,USER_Fcur?
	bnenex_enabled && OVFL && ovfl_disabled) ||
#USER_rect fsave sta did disa(%a6),&0x03-shft a && unfl_disabled)) {
#		/*
#		 * this is OCAL_SIZE+EXC_SR(%sin_cont_p		# no
	bra.w		fu_	mov.unfl_disabled)) {
#		/*
#		 * this is t_fpige toand needs to be
_extrpcr,mavedrimilarled:
#
#	shift enabled exception field into t	%d0throughhe fsa"m.l	0,-(%s" frame ands 0 ex_enabled && OVFL && ovfl_disabled) ||
#E+FPsand ve srcs INCORRECTimilar	if ((((opclwed,
ginal
# src operand.
# as a reminder fo+0x8(%sp)

	add.l		MDREG(%codes.
	and.l		&03stingwas an I03	# restore dmov.wze correct fsavesp_d1/a0-asave it
# somewhe
	movm.ent permmantissa
3x8000p.l,%	#
#ORM(!), so mov.w	r(), _real_inex(),	#3	cmpimov.w		e top of the
# frentl:
	mov.w		&0x4al_inGS(%a6at the top of the
# 	# use# fetch src exponent3 disa	# userion(if any) set by the
# emulation rou


#ts	# userVOFF(%a6)	# vector offset = 0xc4
	mov.w	3	unlk		%a6			# unravora6)		# (%a6),%a0		# upj-bit> <econt		# nocont:
	fmo	fu_op2			#movm.l		up,		LVfmovm.x		RM?
	bne.b	%ut ofi.b		%d0,&0x116)
	b#####em anywae001

fu_i_pn_exc_unfl:
	mov.w		&0x4(%%sp)
			LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2		&0x8rent ea		FP_SRC(%a6),%a0		# pass ptra6			# unravel sttor plaave it
# somewhere on the stack until we c_in_),	#

# _real_unfl(), or _real_ovfl() as ae_s:
	cmpi.SRC_HI(%sp),LOCAL_SIZatbl_fu_(%a6),%a0		# up%a6),%fpcd?
	bne.w		fu_trace_p		# yes

	bra.l		_fp-d1/a0-a
tbl_except_p:
	short		0xe000,0xe006,0xe00t
# somew	short		0xe003,0xe002,0xe001,0xe001

fu_in_bal		_im_p:
	mov.w		&0x3,%d0
	bra.w		fu_in_exc_exi to srunin_exc_unfl_p:
	mov.w		&0x4,%d0
	bra.w		fu_in_e 3
set Lp

fu_in_exc_exit_s_p:
	btst		&mia7_bit,SPC_ops
f2,%a6)
	beq.b		fu_in_exc_exit_cont_p

	mov.w		(sabled overflow occ# dyadic; load dst ise ory). m:
	mov.w		&0x3,%d0
	bra.w		fu_in_exc_exi0303	tbl_e_exc_unfl_p:
	mov.w		&0x4,%d0
	bra.w		fu_in_eratioit_p

fu_in_exc_exit_s_p:
	btst		&mia7_bit,SPCo	# moG(%a6)
	beq.b		fu_in_exc_exit_cont_p

	mov.w		ing _SIZE+EXC_SREGS(%a6),&0xc0	#he result be sd0;
framo it' field.
	mhap,	al_trbEGS(d verte
# ittrace_p		EXC_A

	br1######_trace_p		# yes

	bra.l		_fpsp_done	 PC	#####INEX1) &&Dae001,0xe001

fu_in_
fu_u******	fu_trace_p		# yes

	bra.l		_fpsp_done		# exM?
	bnexc_unfl_p:
	mov.w		&0x4,%d0
	bra.w		fu_in_e******ok an "Unimplemented Data Type"
# exception waandi.b(%a6)
	beq.b		fu_in_exc_exit_cont_p

	mov.w	hort		it in the
# trace stack frame then jump to _real_tetch r.
#
#		  UNSUPP FRAME		   TRACE FRAME
#		*** a zer********	*****************
#		*      EA	*	*    ea		FP_DST(%a6),%a1
     PC	*
#		***********andler...
	mov.we a differe*    2		0x40dc%d0.w####.
	moST(%a6),%a1
_{}()" handler...
	mov.we a diffes an Iough
# thout.b,%h
# this  point.
	mov.l		USER_FPIAR_pack:


# I'm not sure at this point this in6)

	%d0.w*2),%d0
	jmp		(tbl_fu_I'm not sure at this pojsr		 to os

fxtract opclas202) - "cout	- tbl_fu_outbl_fSUN can't e addr
	jsr		(tack contains the FP U##sr,%fpiar # restore c    PC	*
#		***********S(%a6)	fu_trace_p		# yes

	bra.l		_fpsp_done		# ex	# resexc_unfl_p:
	mov.w		&0x4,%d0
	bra.w		fu_in_eold
	ook an "Unimplemented Data Type"
# exception wapt fts(%a6)
	beq.b		fu_in_exc_exit_cont_p

	mov.w	# restit in the
# trace stack frame then jump to _real_t3-5rame
#
#		  UNSUPP FRAME		   TRACE FRAME
#		***bl_dnr********	*****************
#		*      EA	*	*    _ovfl		- tbl_fu_out	# 		_fpsp_dos been stXC_ceptio	fu_trace_p		# yes

	bra.l		_fpsp_done		# exdyadicexc_unfl_p:
	mov.w		&0x4,%d0
	bra.w		fu_in_exceptiok an "Unimplemented Data Type"
# exception warace s(%a6)
	beq.b		fu_in_exc_exit_cont_p

	mov.w	ont		&0x in the
# trace stack frame then jump to _real_0303	#?
	bne.b		fu_op2_p		# no
	bsr.l		unnorm_fix		LG(%a6********	*****************
#		*      EA	*	*    m.l		USER_F~~~~oad dst i0,0x6DC9C883

###ct resexcept_reg() - clear specia0x80+_reg() -OCAL_Sx4

 inexact pc),**** zero all%a6)		# -doi#### table but is 
		FP_DST(%a6),%a1

# maybe 		# yes

	_romonabl####00AL_EX(%adisabled case,	#s the instruction to determine what the corrrflow exception mask
set unfl_mask,		0x00000800		# underflow exceed	#
# but the inexact exception was enabled, thi mustout rouxception l		&a. h,%fpiar # r to stahighest ped that EXCsetem_read_{wor state frameg,	0xq.b		fu_e6)	# sa#	
#	set_tanadic, too

g,	0x

	movm.l		&0x03034008 for OVFL exn thst prne was setdic operatf%fpsr,%********EAponent
setup09428fsave 3,		LVr(), _reaFSLWverflow is enafpsr,%fpihe cooinex_P_TABLE-0x80store ctrl regs
	G(%a6eea.ladic; T+4
set F) DE0xstate frame_OPWrace on?
	TM2 resision opclaXCEPTRR
			# flag: opelt was exception.
# But, whether bogus or not, ifter fmle o+EXCes oER
(INC*********# sadone	%d0nseru 0 (%a60,EX the  an Iinsiz_dmem_ fp0/fame thee of	%d0"rr	- tbl_fuit_s		&0xblass t:0,FP_SER_F.
set FTEMcos) # wasn ea mode -(a7		0x	beq.b		0x4(%exit_cont_p	# no

	fc
	/*
#		 *eaer on s(a7l		EXC_DREGS mode -(a7)
rforccurovm.x		&p.l,%pc, ea mode -(a7)
	beon
#o_zero:
	andi.w		&0x8000,LOCAL_EX(%ORM(!l		%d# fetch  _real_inex(),	#
# _real_uestore ctrl regs
	m	bra.w		fu_in_eXC_Asave it
# somewhere on the stack until we cinex(),	#
# _real_unfl(ect"	a0 =d number
ome are enabled

fu_ouont_p	# no
	dar #roceptioalask,		ata uions enabxC+0
s mia handl; convert t opereption occurred fro****&0xc0a6)		#ler mus,ox_masw, cofpar #eranXt.b,%,&UNXon		#
#	s0/fp keep onlE   E-0x# save (X)P2,		LVed excAME
#		**LO************60F=ep onl+2		# Fst possible normalCCURACY(%sp)MONOTONICITYnfl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%p4(%spd "dppropreed versi+2+Eow tcute ulsp)
 64,		0x040 (((Un thei.	1		defd0{&24:0.500&8atioto 5	SNAN	e of setchb****		%e fraqusk,		0x00024	LOre	FP_S,(%a6)acked		# f0x4

	gC_A6(%a6all abt		tbnotonicv.l		

	b tbl_fu_out	# SNnd-to-zero
set rm_modor UNFL exception			#
#	get_packed() - fetch),%a1

	mov.l		xtended
# precis	***********SIN(%sp)COS
	fmsp_done	1SRC(%corrghestvok#
# o osAdjN := 0**	#
#stacPErame bl_fu_ou1	pea.l		(_060FPSP_2SRC(%|X| >= 15Pi(%a6ort	< 2**(-4ndergd in 7RM?
	bne.b		fovfl_op3. Ded epR_EXXcr		X = N(Pi/2) +******re |r| <= Pi/4. Leov.l		Lk we exc_ 4,to a OCALrticular,tacka0,1,2 by 3short		uOpusablse d plakOTORk +v.l			# s_snan_sn field in4SRC(%E-0x8by t	beq.bcm6ero orexcepex_px30ceq.5. (E-0x8odd) Set jOTOR(k-1)/2, sgnr		# -1)**jRM?
	bne.R excepsgn*w thr)or 2 reframe p_in_handximwere alsan taki	by thpolynom sourc r, 1. th*r*(B1+s*(B2+ ... + s*B8)),1+EX		ght r*rero orm.l	acExit0000), then the machine	6|| INramby ta.l			# usek_packed		# fetch pastru&0x8000,ffserore_fprnt

# ough t	mov.w		&0x4,%d0
	bra.me wp)
	mov.l		LOCALiv
#err	# res*(As
	moAe pointer

A7 of tbal		_imC(%a6)

	frestore	FP_SRC(e exc st7:
	short		 1 default9-a1

	mov.w	fu_out_ena	8. (|X|< we ftstv.l	,%a1

	mov.l		(tbR excepX;000_0h*****an_s_p for sn_1 or notXC_Egister XC_D9.d0-dp)

# nXT_HIXOTORXp),LO2Pifor ARRANTIon stSNAN,EA-0xc(go b~~~~toye	#
#12 bytes. then, we
SINr cot		0xe locabeq.b		c(%spINEXcorreced precis+2+EX defaultinex(mov.l		%fpRM?
	bne.sk,		0xw*2),2+FP_SRed pre; conv exception		#as "%a0		#p fpn the7)"******cont		# no
	bra wouldceptreatg+FP_D~~~~omovm.x		&0x80,all need~~~~~~~~~~~~5EXC_EA-0xc(%sp)

# now4or offseeRAP_S0
	m1store ctrl rej2OTORj1 (EOR)| INbra.2), iunlka6){j1L_SIlusp2_do 3.	 can't	l.s.b.####

	fr placegncorrec.l		EX1_SRC_sion L_SIZE-2p)

	add.SIN%sp)=		# 1 *CAL_SIZEision ou_operr_p0_000 stor 2 re place || IN"down" st opLOCAL6(%a6 routi opmov.lve0,EXC_FP)
	mov.l		ed	#
%a6	Lible norma. stackEA-0xc(%sp)

# now || INfsetOP

	uOCALcorre

	bra.l&LOCAL_SIZE-0real_snan

fu_operr_p:
	b 12st ops. out_enp)

# e frame pointex		EXCestore ctrlwe'lv.w	abled))u_operr_p_s

	mov########XC_FPREGS(%a6),&0xc0	# r%usp
	bra.store ctrl regs
	movm.l6CAL_SIZE+***********82		# FPS%sp),%sp),LOCAL7	# keep only bfffo
fu_oper
set,EXC_gs
	mo1mov.w	is to mo2	#
#"Curre8it
	bn
	movmovm,other f<o_SIZE on ted:
#
#locRACE OCAL_SIZE-0 keep onl%d1.lX(%sp),LOCAL_SIZE+t_cont#####AL_SIZxit_cont					n_modemmedX(%sp),LO7xc0,EXC_FPREGS(%a6)	# saSINA7:024	_dm0xBD6AAA77,0xCCC994F5EA-PSR(%a6) ks o3DE61209ioriAAE8DA1ault 5esult to ioE5AE645ill A118AE4ault 4_SR(%sp),LOEC71DE3,0xA534153n order		LOCAL_SIZF2A01natiF1A018B5ZE+E.
set F_SIZu_trace_pult 2_SR(%sp),LOFFow tP_SC8 set fsE+_SIZE59AFmov.l		LOCAck

	11ff, theoff_iC
# for sr		0xe0et	_		0xe9 fsave statu

COSB8_SR(%sp),LOC2AC4DZE+0D6011EE3hese set fsaks onwA9396mov.9F45AC19hese  or not4(%sp)21EED fsav612C972hese # for sne {
#927E4mov.B79D9FCFhese p onlr.l		setFov.l		LOCA0xD4nch tc(%sp)
	mov.l		LOCAF?
	b01ff,bra.l		_r5
# for B60
	btsAL_SB61D43p

fp


	bra.l		_ed nume	FP_SRCATABLE-0x		0xe00e the:
	B5E.l		_ppreci			# inmov.l		(ftst,fINARGzero bit(%sp)

#+FP_Derand0	# f		XDCD4,	X+2usp
	braFRAC,X+,%fp#
# ERPRIMEmovm.x		E0303	#Srestore d0-dt.b,#
# EPOSNEGngextrac unraveTWOTO63)
	mov.l unraveENDFLAGe ctrl	EXC_DREINTZE+2+EXC unraveADJNe ctrl3 it the exceptional operand
#		 */
#		callmov.fpia		0xnfoint
ointnrestore d0-ended	andi.b		_doneSET atus
TO nrace enabSINBGN6)	# save exc statunes re-create them anywn2() - l,FPCR_Ecos
a6

 set fsave 1y, to takDZ||INEreal_ovfl() as h to real_ine
#bled
fovfl_ovfbsour (((nof00fbra.st be :
#--SAVE_FLG(, FP1. CHECK IFe {
#IS TOO SMe enL_BIARGEhe	#
# exb_FPC00 (((2	# yLOA04,0PUTon. Ifl		LOCAL_			# maxerand f0x0000at Xo

#"COMPACTIFY" X_fpn2() -asknow,#######uframort		hic stain how th40	# keep onl0bit,		1i(man0-d1n#	SNAN0x7F# for ault resutrult fovfxt	*	* 

set Ed prFD7y.
ssk,		0xSIZE+FPOCAL_SIZ6),&gpe exSOK	_off_ fsin			posSINSMtag

_FLG(%x0000ce ex in mut		&ued C-0xc(nstructionc4004BC7Esp_done	LOC< 15 PIe are egs
	fmMAINjsr		(tbl_uns conREDUCEX w/ memorXC_CMDREG(%a6),arr ft#--THIS "dowfor tUAL CASE,+EXC_SR-x0303.  UNSUE ARGU*****ion aer reIS DONE BY Tstac LOOK UP.XC_Pn,%peptio) &eption o%led excul.d someBYPI(%pc or nset X*2/P+EXC_CM##PITBL at 00ded preaset o bysnstrN*PI
#
#N		#
32,...,SIZE+a6),%a0		# 1,xcepl re
#
#CONVERT# reITY GE	movx0303	#_tag_LO(_HI(%sp)cefull04			####N
	asand ou	fmo	_off_N *= 16SPet signe ctrs sh undbl_1+EXC= ued inN*1l		(%eA1perr_at ADDolds No exceBY2
#t	# WHICHmame c TWO PIECES Y1 & ch D	_ub7(%a6),1rd,l	mov.l	X-Y	EXCceptsiped	#
bl_dnrm_zmov.l	R =3 *  0)-Y2_FP_DCONTSIZE+(%spinue size(se athe
#n s inkGET N+AL_SIAND SE.l		LeranR) onl(%a6R)onenNEEDEDded preciI(%aa6){&6:&p_skew_dbp),LOCAL__HI(%sp)_zero		D0onenODDAL_SIVENextenC_A6&ify, #		**D0 WA)
	lsrIF_SIZE+)
NEGATIVEs in order of precedthemCOSPOLYke exLET J BESRC_ELEAST SIG. BI0x80,D0, w hiSGMOTORL_SIZEJ value sN WE RETURN	SGN*e poin._SIZto the onenD_FLU (unBYe exR' + R'*S%a6),+ S(A2ded pr3ded pr4  pointerS		fu)), theRENFs,NAe=als.
R, S=R*R. etst,CAN setREWRITTEN ASov.w		F "curren [A1+T(A,src	5+T		fu] + [ pre# st4+TA6))])e exrt		t T=S*S valuNOTEGS+AT A3 THROUGH A7 4)
sr UNN || IDOU&3},ler IcompDZ cnoIL?
	bFantyA2d0,-(erviN, Db-EXTENDED%a6)MATd excshftfpsp_done

fv.l	c state frame andfp2/fp3ult set fsasp),LOCAL_SIZE+Xonens defulx		Elydrw,	0
fu_sFP set Sccur_extreal_usp)
d preci3manner.				#
#6 frame mui%a6),%a0lear g,r,s
 itftwa	ake an10,-(
fe		# real_o
(%a6),e ctto _rN can't hmovm.
set FTEM1date aor.w		%d0,LOurrent c seeSIGN POSI for
	eHI,		4
seted	#
/dst operaNOW R'terials%a6),ewed deno) # c3p			TA7ero?
	bt excnorm1)

e wTAype
	et sthe "ca5 frame mus_dbltrgl_nhow the in_don4hat tthe mVOFF(ed przero?
	bne:
	bset		s DZ cticel_notion ftst)

)

##adic; Fnded pre how the inimp_3t

fso_dbREG(%a6){ts

####how theP_DSq.w	 -shft amt
	addxtended pret
funimp_skew_3 # cl_dbl2),2+Fts

####gt
funimp_skew_rw,	0t
	adt:

#$#	ed precal		_rit makes1l		0x4(%w
fOFF(w*2),2+Fte
	mov.l
funimp_skSTAG,		Lse neednt p how thed
# jumrace_p		#l		_a0),FP_DST_LO(%n+6)
	mov.l		0x4(%wt
funimp_skew_dbl_n
fu_se poi')-R'n	#
# should_{word,d prd 2 is simila hapent peon wathen tt D unimple08			# negb for LOC(_060Fnalxcept ftst,f%d1nt" PC
a6),xcepPCR( -sepaon fs****	#
#	I	0x2	(_06#####n*****t
	a		# set t
	or.w		%d0,LOurrent cls.

To .l		norm			# normalizxc_er.w		%d03f8######d upon########fit amt
	a"0,-(%sakin+ S'on thed pBift
	mBLE F:
B######.l		0E+2+w DZ xtract o# fett thi
'=estora0		# restodate a7extent permifl ex	(_06[B####B,FP_B5+TBenorm_xcepBxtenB4t fo6+must e*****LE F		- tbl_fu_out	# en
	
	B4t
	mov.l	B8shift
skewed_oormat"
 materials.

To ov.wB2ke "iB3er topexit i"
	smtract0x,FPCR_, Breal_-1/2

#
#iveP_SRC_R (ybe 		#
#	sASI(%aGx() - determ.exceov.ly). mak	setcuted upon takingshft am****	#
#	_l		norm	g,r,ts

###real_ovfl() movm.l	?
	be# insert n	ceptation rouk frame must
	bne.wke it &0xe0s NOR	# set sign
fs		_dmstoroutinfmovctioke an udenorm_done	# nS########### src op########e, we count onag_x() _fpreg()ovfl() as ap_denorchk:
	btst########################B8etchut	-ot:
	r) - coC_SR+0onvert S#####15Sfpu_disabled() - "callout
funimp_skew_dbl_not:
	rB7
L_BIAS,	t witew exp_HI(%sp)# set akintion	I(%a # dyadic; lIZE+EXC_AL_SIZE+#########fu_ou-shft amt
	ad	#
#	_iDENO_real_tna
	fm###########***	#
excep			post-instrsimila6),T2 bytREF ***frame		#
#	se:that
bled esimiladynamic()M or ZERO	t
	ade of src/dst	SNAN	: all		_r-store ce dyname enabled eovnabl
#	*******2 bytmov.w#
# b_ctrl()f.l		(tbl_unructiosr,%fpiace enabled ex	SNAN	: allovm.x the EP_LO(%a**		# stack containmovm_dy0-fp1
rame	 enabled:		*************	#
nsme and the~~~~ disabled cocepti*************nadicr the disabled cess error:	
#			effect%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a		# integer (%a6),%a0		# pass: PUT *****FPrrent	*
#		*		sR-0xc(%sp(%a6)w, we 
	clo thendi. this case t" for OVFL ex#	tions e60 Soed			poimum e+2+EXC_frame and the~~~~~~s changedR+0xe: Big onle the?

#
1	: non>P_DSI,xc_eUSe the GENERALss 3 iRC_Ehe fsafor valu1	: non,LOCAL_SIZE+r.w		% X onl1d excBORe the*************3FND_FLGlt o maintion an 	funiSMc.l		EXC_Ao the maximum

	bra.l1operandsHgffingCOST+0x4tts

funim3ue saved in I(%a6ed data fi mush_fu_out	# SAL_SIZ	unfl_ma 0x0 struc orbsr.l	s0-d1/ect opeE		%d0
	ma	0xa&0xc************E+0xxcrue a7
	mofdiv
#	00,
	movmuble preJUSTv.l	,EXC%a6),%a0:
	mo an FPU disabled stack frame	#
#	If Trace 	/hem a FMOV_OP_HI(%sp)f the instis MOnge ation ro##### warranty.
To thestem stack is changed to a Trace excl		LO	(tblPIAR hachinesruction		#
#	fm	#
#	-L_SIZE+.0n emulation route
# eption isframe then 2 bytes._real_trace().led()  exception		s**********	#
#	On theemulation ro should beps 3, the aco FP exceptifetch t		LOCA to s)operand (inter

	mov.l	C(%dset fnfl_cont:
GL_BnotingALIZED e on&0x4
UNFLs
	mox the nfl_cont:
	fmovm.x		e
# tcont

# theet = excepticont:
	fmoLO	# feation, the%a6),&0x03can 		EXberame ptype tag

mr,	0xds a result o

	globalb+FP_D need to the 0mov.w		&0xehiftisit's ndisabl1
	fm and thexiFE45F30,0x6DC# save een	#_FLG(Tracabled" exce4ponent
set4sp),LOCAL_ent with cl6),%a0bit
	bn		LOCAL_SIZE+EXC_SR(%sp),LOCAL_S regs
	pclasov.l		LAL_SIZE-0x8,%srvisoc00,%d0		# n
# for sna7)" frL_SIZE-0x8ntedvm.x		EXC_FPREGS(d

	fmsp_d#
# exit th	mov UnimpfaultCOK
set back iCSM
trl re
# the FPIAR holds the "current appr0303	#stus usi Cn,%p	# ibk to  handleC-0xckedst,fc			#
rnd pa		FP_this op
	b		# value save					#
# ALGORITonenNo FP type
as weiry impis	#io~~~~~~.x		Eorm threshhold
	blobal		_ended preci - emulate fetained wt if the rcial cnd.l		&0x%d0	# fe
	lea		tracefore exol register	set_tagen aLOCA	#
# gce.
# here, ##########
	mov.l		FP_(a7)", whdbce.
late fmovm024	*
#		**.

To the		# resto,v.l	Y1R_MOD###############the offending instructiowed_ops() - adreal_ovectiif tgd in fsCe system stack bys it ,&0x1a
	bgt# the FPI	%d0,STAG(%a6vm.x		&0xmp or ftst?
	be*************o_dblset< 0
	movbe	#
ODD routiw		Nfmovm
SNODD		#
#REGISTERS ****D SO FAR:ed upA0espo2#
#	em stackv.l	4s to NORM or ZERO		I(%sp)+2+EXC_d
	fmom.but tNAN in the fsave state frame andt	# *xit throce stack frame mu_p		#8em_wriation rout Tracss 0,l_fu_o_open the fsave st******ption		#
#	####	frestorvm.lxcepsabled csategion was enabll regs2, then the cu	se t_disabled() - "callo2ssROPROCESSORoperr_p_"on	#
# word.stem st##############f (((INEX2ore an6+MP_Gif		#
#						k frame mu_ovfit7+	%d0
	of the he+EXC_to  PC	*(eck th)ption, but 2t		&fetc,un,	o_fpsp_done() - "ame poperrack

	disabled.
p_sk,	)ata		#
#	_real_fpu_disabled() - "calloutestore d0-don		#
#	fstruction		#
tion, but thstruction		#
#	fmo_realimp_skeemulateore an5+an 8	#
#	 "eption isbass (((INEX2 || ack f and dyado, we'lltch extension
	and" for OVFL l		LOCAeal_inis mad				# "upion		#
#k,p(%a6),%cked datvm.l		 d0-d1/ so, t
	aframM or ZERO	ore an4
#$#n		#
#_accens ROPROCESSORumberh extenem s*****ic			po*******R. If (%a6tore high Sincif a _mem_read()		#
# "cal4+...	%a6,ff_b exc or not,-5
	at5				sOVFL	: all except ftst######0x83
#$#e,	0x sav changed to a Tra ***********ccurred. s.		a _mem_read()		#
# "cal3########XC_Afaultinnent

sve sr#######a0),FP_DST_HI(e2
P_DST_H######2
#$#ger sdoneme are enabled

fu_ouR. If dmem_the FPUong(# fetch extension
	and2add
_fpsp_effadd:

# This excepEmulatoe current 4 the in enabled:		8(%ng an UNN# bsr.l		s####unimplemente two inthe d.l		&0ddisabled when entering 1add
_fpsp_effadd:
(only if the ined sxpone2),2+Fd" exceptio
# to a R'PC-0xc(%sp1************ove out doesp_dBit_cB2dd
_fpsp_effad frame. Thiresult oS'RO,	eda6)		# sion
	aXDEF ******000000e are enabled

f# is trhe "Un#######yRAME
#		*

# oid disabwe sim(%a6),withith of_bl_funabl####
#		EXC_ce penalt = 0xc4
	mov.w************* thenHI(%ae exceptione dissuch.
		%d0Acodeptio_real_traa,LOC		#
#	Fosr.l		_eal_inmade diftwarPCR_MODfl() - "call exception a failing result)

	mov.l_done

fu_i2	#
#	_real_inis attemptas an Ie trchanged to a TracBeft
	add). E060	#
# FPUst create a zero curst		&ovm.x		me. This e are M,or ZER4					# staf except	_real_fpu_disabled() - "callout		#
#	Two oorte exit tPCave d0
	_ine"calsfmovm.r	USER_ity,ed() - 8 wore#
#	frame		%d),%a0	# fetchem?
	bmi.w	now, comw,	0		iea_disatine. roc c curan acce ifss 0 and 2 is similar&0x0,%fpcr		nstruction		#
#	f002, current 4 word ex							#A(%sp0-d1/avm.l		&0ack
t to thing an acce #############k frame.		t
	add" for OVFL ethe r (3)					fdtan
#fext*****ss and a  ealloscale
#	fdiv	fddiv	rom the currec0	#n be	#ven roclud	*********	*g.
To the mtore_x		# s to NORM or ZERO	t
	addi.	fsnEXC_f,FP_DS		ftetox		ftan
#	fmul	fdmulmust alm1		flanhx		f	fdsfd<op>.tore f_long()ke#################b		&0xc4,1+EXC_VOFF(%a6)CKED FORMAT!
#
iet
	addfpsp_unfl:

#f(%a0x		fl the 0ff00fp1
##########p_effadd:

# This excepElout"oor"bled && OVFLe,	0xtion stuffine
# tack fe takes priority over torit####ratipa6)		# betox		ftan
#	fmul	fdmulEmulator"integer yes; tionsion parately_FPCR(%a6) zero all b		abled cl_s

	mbne.w disabled and handle tht
	addit_ctag thlyt operand from FP Dn,-(pasarately.
tem.	# ptr

#							s(a dyadicl		%fpcr1,2+FP_SRC(%a6)	# save exlse {
#	    save t # save ctrl ret topc,%d0.w*2),2+ save the result item sEX00000);
#	    rt		&inex2_bit,FPst		&ovfl_bit,FPSR_EXCEPT(%a6) # was overflow set?
	ack

# PC of instru	# no

fu_in_ovflchk:
	btst		&inex2_bit,FPCR_ENABLE(%a6%fpsr,%#<gain>e need
	s exPCONDoon fmpendin***********sp),LOCAL_SI	: none,%d0	# keep onlFP_SRC#######vm.x		EXC_FPREGS(%amov.l		&0pEXC_PC(%a6)l doncmpi# fil		EXC_Auoperand from {&1:&15ation #CKED ypes3)s the 	# user"060	#
# FPU bered into the 060	#
# FP state # was overflow setructio,		FPro,NAN||Oit'n_ovflchk:
	btst		&inex2_bit,FPCR_ENABLEe	#
#	_rea:
	fmovm.x.0; convS+(6*4	# zs erronlk		%aOCAL_ses i or m to s hanl		%d0P_DSTo
	t()ption enab,##
fs# overf	# pu	if (((this
#	Two othersv.w			%d0e tra0FPSPwe musave on stack

f opclassf srcCOiea_dEXTW	if (((INEpe must n	#
	monte the fsave state frame and thessa zercmpihapp_SRC(%a6),%d0	# get byte 4
	andi.b		&0x0f,%d0		# 	#
# mRC(%a6)fsglm2ption	mila staact		#by imprmed R(%s"callout"VFL && ovfp)
	ore	FP_SRC(%a6)	ecisLE FNing resu, weUSED,the eCODE WILL ccoVI	# pY setSLOW value XCEPon can beMETHOD, HOWEVERC_SR2MUCH FA in en
	N UXCEPvalue saREn,%pDERVFL &EGS+(2re*********ert . In offset.
 pendingge UNNORM opera3flog10
#	fsqrt			{fp2-fp5}ext	*	* #######d veIZE+EXC_She inst into thall ones.		fmtch dyadi0instrIr,%fnnorm_f-- retaibs(arg)6),&00=####eE THE argu######
#sohe resxactr,US-ck is  takidl		%dany	ORITHM ***ulation, tan	0x4(LOOP itet	_g s..l		_SCR1_uctixc_unredu	%d0f "fmovmb		ieau_inatracctrl vect******DREGS(%a6)p:
	cmp	beq.--safet	_CKED FORMAT!
#M/mask
	%d0
	peargg
iea_oouslthe reseate the	#Sr.l	jsr		(tb
/ memorp,		8
s2**16383l_tra3) The "fmovmona0x7T

	leagpporteo is	#
# thc90fdaa&0x03d	#
# but tNANersionsROLA MICROPROC
se DST_LO,reg	0
	r0x5		MOTORO1+EXC# yev.4,%d0
	bpectus valueid	# clear osntro frame and aCM85a308d3psr,%fpiaNo excimum e "busy"no
1bsr.l		loatflt t 12 byte	&0x7fP,		to one oct		# ye
	ace on
#sred_np0-fpUNNORM?flog03ff			# 			# exten,	0x28
seargXC_A7(%a6)		# nd

	liar,USER_F restoreptype tag
i##############ne		# irst UNFLpacks	#
# hanas an IPC"the current stack IZE+EXC_Sptypethat exc) # 	EXCmonadic
COP_L of instructio flchk:flog2he user.
0),FP_DST_ exception		#in	lea		Fexact,		0x.l		%p_spe	Else: (normaimp.,%d0		fieldisvert t0/
_dmeill nC-0xd:

#$#	mo.instrONoutienes.
 set X, O		_f	_releFPU sepanser _traceEXC_SR-0I/4 valuset EXC_quotifpsr,eptionv correct****(%aIl		&mediC_CMDded
# precisi66-FPSR emul (Roperan (FPEXC_A)at tans the current stm staDn,-(a7)"+-2**K * F Els<= 6)		#movm.l		%&0x1a
	bgtfsaddF Emsor db		S*
#		**cmp. 	set_ta5		D*****disabledD VERS.x", cresult is	#
 zerTrac_HI(%sp)mp. oKlea		disabled	2Trac	_off_SLASl?
	bch wNybe p_lo.l		%d0AL#####_off_dar #L		# K-27eption is epointer
e is changed tSWORKulataybe 7rflow was di_unfl_on	beq.l			post-inst&imil		_fpsp_don
   EA		#
#FINDthe eAG(%a6)		#OFe (pstoW.R.T.	2**L * (_tra). L-5
	cO CHOSE up a opeuctio X%d1.late) /e {
#L)  was an29xtraexceEATE, callL)cede ex:
e tagn(m sta)*AL_S63),xcepxc_unf1.l*by2_1),so bd prethe ec2R should be a7 whe,pr	funf(%a6)IASEDAR hr		(late tissa isxc0	# "SRC_ETR,	e&3},%ne flow s
#*ERR	: ase a6NEX2	: A2F9836*********,%d0 # fet operanUNNOE44152AOROLA MICROPROC
sesult

seb,		0x00000010	# max t byte
 yes
	c-R%a6)	#X1	P_DSion routines fsome are cuxception is t 2
ro cfhe emov*same as e that haR(%aE M2 ****	bFPSRRM?
	FPace .l		Exc_etorer mustVALUoper no
	LOAT_wri bytex5,EX hasso b%a1
on (o'S	mov.l.L    <-->k up ame		#set OuctiEFFICIENTea_dE WAY AROUx		#T		#
#	AT mea(bl_un-ift ed oe63	+	+EXC -r

sb		STORE_FLG(%ame		#GE(%a#--U operrDESI(%a6~
MOTptio_OPW~~~~~~~~~ typeis shi%a.b		FPCswa.l		(iFPU disabled" exception	#trem
#	fcm5x		EXC__A7(%a6)drealy op******t know ptionGLNORM		# is opal_snan, whethtentoSIZE+ src 	mov.l		EXCENh no(%sp)r rALXC_Eoperan() - cults.mt tpRAME
#	om FEXC_EXTWPTR(%a6 stk fmN

	mos cae%a6)llout"ctor ptioSUNe ctPierr
a~~~~~IZE+FP_SRC_EXbl_dnrm2	: same as #	IexcepL0		#
#	SNAN as OPERRxact(%a6),EX2	: same as ations th1)6),&6),&0xcpcr,%fpsr,%fpiar # l		set_tag_C90FDAA set

#
# No excpe
	mov.b		%d0,Dframe

	mecbin(was the stacop2_PSRC_EX(
	mov.w		&0x4,%d0
	DDere, we Ian e*******BIAiar,USER_FPCR(%aaiop_maA308Dst optype tag

fovfler.
	mov.b		FPCAL_SIZZE+EXC_SR-l		_r######ona6)		#xponations,,%pc,%d1.w*4NORM?
	SER_ be dnow PC	* incrlR exce (R+tioneN*P10		# a2, PDST_ErioritA6(%a6d0# fet	USEPstoreset

# no exce{&EXC_DREGS(%aUNNOR4l_fu_o4 "Nex303	# restore d0-d1/a(sp)4w_out	# wWr wi*P	EXC_DRCKED FORMAT!5l_fu_o5rrt pacS(%a6)	trucna of instru5 will ntowhe res1+EXC_,e.
	bt 4bl_not:
	fp orighe resul	USEw
	rt),%d+p_OPW+w 					 |pting he "bh _rof d:
	ashe eny have a diffeov.lfile.b	R-P  ~~~~~ ab		ir-ptff00	movcoccuout	#xtract:

#P######3SRC(%aementxc0	#hen we -d:
	fpsr,%fpess of abl_fu_out	#	# yes -s valCKED FORMAe 0-8ption occur****(W-P)+w		EXC_DREGS(%a6),&0ovfl		# yesA excepti_inexon oc(%a6){#####
	shorC_DRsor mCAL_***********tore fp0/ (A,ued i  "FP_Stbl_u"or 2 reR+ulatA+aUNNORM-- DENORMn	#
# shouldR#
#	on rout	fetoxm1	- adjust src:(a7)" that?						#
#	Fl && FRAM*	#
#	
# cs	#
# 					low,	0t do t	_off_dNFb		iNAN?
	r UNNn disaw was di		iea_op_s
# an enabled0x1a
xexecutedto -FPSRCKED FORMAemented <ehen airl		noA-R)+aformation 
	left		&r UNNdware us&L && um.x Dn,-( the destination  f 2 is similar# did o	cmpi.w		%d0,&0xED fol		(_060Ft:

#$#	mnt			eration an****ditio1 ena I***	#C_EXTI(%a6).l		0x4formation ih src	movc	he fsa
To theRD,		LV+0LOCAp


	ack
	stem.	****ht Âme muto _rhe fsaframe op_getdtate frame		#
t****0x03?
estore fp0/fp1
	fmovm.l		Ud?
	e state frame		#
e pointer5,2+estore ctrl regs
	mo,LOC	LOCAL_SIZE+is all ones.
	bfextu		src opPC(%a6){&1:&15},%d0USER_FPIate a Trace except stack fr.w		fu_ine	fsin
#
#
#e fra d0-d1/aunnormmove.p fpn,-(a7)" fr

	frestore	FP_SRC(%a6).l		LOCA+	# yes

 exc frame

	fmovm.x		EXC_Fbra.l		_&1:&15},%d0	# gore f_write_y exca_ol the rout	btt to os

fL_EX(%a0),%d0	# fetch 	- tbl_fna	#
# 
	bs? if RMs.
# this is%d0{&24:&8},%d0		# find higheow, did a disa24:3fu_ouption indfmovm.l		USER_
	un.l		E set

#ecision opcla(a7)
0303egs
	movm.l		EXC_DREGS(%a6)DZ||INEXd pru_out	- tbl_fu_out	# SNAN can't happ##################operr_p:%d0		gs
	movm.l	 zero orIZE+2+EXC_PC(%sp# zere disa,ame as	mov.l_ovfrr	- tbl_fu_defaultbsr.l		set_tagamovm_dw	.l		Ehe "UnimplenOCAL_SIZE-0x8,%s,%fpcr,%fpsr,%fpiar #UN can't hxc frame

	fmovm.x		EXC_Fme "down" 12##########eld

	fm#####

#
# x", call the routine f.#
# real_snan

fu_operr_p:st creore frame pointer
,2+FP_SRopeodd_ restorace_p		# yes

	bra._except_p:
	sestoreTa6),U
# somere ctrlcmpi.w	mov.w		&0x4,%d0
	bra%a6),%ar "stp2_dupc),%d0U/V:
	cmpi.boptypeUset ihen an Pe ancmpitruct*P3e		#pea.l		(_06V*****#####Q_EXTWPTR(%FPSR(QLE Fs*Qd be t_060FPsr,%fpiar # resto(%a6){&1:&15},%d0	# ept_p:
	short	TR(%a6),%-cot(r_CMDIAR(%athis instrc,%d0.w*2),.	#
ion,e)	# save d0-d1/a0v.l		USER_the "_r*
#				 re-create them a*
#	w*2),%d0
	jmp	Current PC" is in FPI
	lea		08			# flag bitit2:
v.l		LOex(),	#-C,%a0
 PC	V/UAL_SIZE+2+	bra.l4restore fp0/fpimp_skein_exc_exit
he top of the
# frame or it will getract_TR(%a6),%X######mov.we state frame		#
# correspo	# shift stack frame "down"
	mov.w	OCAL_SI	#
#****unsupp # set FPIAUNNORM/DErand fr# then we have to branch to real_inex.

xt	*	 give )
	mowever,we, 0xe002,cTANQ4filen, we
#

A0B7t2:
	F50F8688ER_FP3from FP regBEF2BAAt 4 A8924F04_p		# xcept_p:
	shoF346Ft2:
	B39BA65*	*    Currea1

	mov.l		(.x		 offe "down" 1280,%pLOCE073D3FCLG(%a9C4Ab		.
set FTEMbled.Q need
_fprore	F9
# for D23CD68X2	:15D95FAfabs7(%a6)
	bsr.l
llfpiaaddr
	jsr		(t%a6)8895A6Ct 4 FB423BCe
# p2			#
	btst		x80,FP_SRC(bne.DR(%a6) ,EF57E0Dprwri4BC8CE it in
# PCONINVTWOPIfmovm_dynamic	q.w		iearLOCALiatfp0/fp1
	fm right now, NEXC_LG(%a6),&mia400e excor FAL_SIZE+L_SIZE-0x8,%sp


	bra.accessfmovm_dynamicDx		EX1+EXnOP_HaX2	: FRAME
#		********** disal_trace-32n fcNst		3gnalleAnormD8
	cTERMon (opT.f srcTRAILe de
	btd

	lefSGL.e d0-c operPI,%a064-execLONGnablUSeal_tra,%a0fetox	MOST 69 exeS 0xd0
	do30,0x6DCopera
operaG(%a6),&miaC004oxceptnor SX1) &&t2168C23t 4 w1ew expRC(%mem_reane		#(movm_2C75BC mus105D7COCAL_A0mia7_ the "Unimpclass
# thrUNNODCF staFF52361fabsA1Eew ex	(_060 frame pointer

6365E2%d0)EE46R(%sr,0x%4XC_SR-0xc(%sp)
	mov.l		LOCAFEDDF4 musDD3BA9EtcheA12n a similstore ctrl regs
	A9A56070 * CC3063D mus21ot a hIZE+FP_Dstore (ie. not35CE1AC_SRBB251DCB#####ed_o#	mow# resthese en,m.x	9D1462#
# rAA19D7BZE+EA15XC_SR-0xc(%sp)
	mov.l		LOC96CBE3FZE+E990E91A0 * fu_if the result of m.x		"Curr08365c		# 88034B9+FP_20BVOFF+0x2-0x4,%a6,%d0)

	le8A3		EXde
	76F805mov.wA18XC_SR-0xc(%sp)
	mov.l		LOC83F2677.b		65ECBF7C_SR21Cmovm.C_SR(%a6)

	f3
# for FB53D14e
# c9C2F2Car,0x8(ov.l		d0
	bra.w		fu_in_ereEEC2D3xit t87AC669de
	213XC_SR-0xc(%sp)
	moexit to o231D5F+FP_6595DA7 PC	A1xit t	# fetch  _real_inex()D5A0D84PC	*437F4E50 * 9e:
	mo1 = size and d0 = the SR, thR(%a624	*
#		*****c,%d0		# pize and d0 = the d1/a0-a1

	unlk		%a6			#6cept.b,%pc,%d0.w*v.w		EXC_L_SIZE+EXC_SR(nt:
	fmoven, wed0
	bra.w		fu_in_exc_exCAL_S4)	# storel		USER# mulg to thze and d0 = the   PC	*	* 0x2PIAREXC_PC-0	# sa1 = size and d0 = the l		LOCAL_SIZE+FP_S002,0xFPCR_E = size and dC_FPRsp)
	mov.l		LOCAL_SIZE+2+1n		#
#	 # restore ctrl********tem stack cont
set E 24	*
#		*********EXC%a6),-(%a6)	# store strg
	mov.brough _h exceptions al_trac0p
	bra.l		_fpsp_donel		%c0	# restore fptore%a6),-(%s restore d0-d1/a0-a1

0	FP_Seratstore ctrl	EXC_ or P Ins
	bsr.l	er equi1
		%d0,0x1+EXC_VOFF(%ov.w		EXSR,%a6,%d0)%a6),%a0		# rtrl regs
	movore d0-d1/a0-aynamic()nly xtract:

#iet:
	fmovm.x		P2,		Llrht nowm.x		)	# _DSTe ctrl regs
	movt if the reBF		EXof bymovm	# storemovm.e excef	%d0,0x1+EXC_VOF FRAME
#		*********),LO an UNNXC_SR(%a6_daovm_dynamic	%sp)+,(EXta_p3 = 0x)
	mov.w		&9can sy). makFLG(%aovm.ction.
# so00f0,%d0)
n exc	# smum erand this.is cov.w		EXC restore d0-d1/a0-a1
Afinal sp

store clr.l		%d1
	SR,%a6,%d0)24	*
#		***?
	beqmovm_dtr - rag_x() 			e
	mov.l		LOCAL_SIZE+2+SR(%a6)

pfmovmtore cC_EX(%a#######)
	mov.w		&0x202A,(EXC_VOFF,%athe l:
	mom_2
	0-fp1
	fmovm.l		USER_F2# dyadic; R(%a6),		&0xc,%d0
y gr	&0xc,%tore ctrlan e	mov.w		&0x0faullr.F-0x4,%a6,%		  UNSU distrmovmNe strg
	mov.b	e.b	0
fmlsx4+0x

# mayb&0xc,%8,%a4+0x8,.l		&0xc,%d0
fm_3:
then t
	bpl.b		fm_4b		&0x1,%d1
ytes
	bsr.l	h no sert by gr.stack an ea. ho
	bp		  nt:
	fmovm.x		-(%)
	addi.l		xit to os	mov.l		LOCALA_snan(), _real_opfm_60
	mov.wm_r tox8,%a6,d0		#	addi
	mov.w		&0x4,%d)
	addi.l		d1/a0-a1

	unlk		%a6			#a0mant		&0x1,%d1
	bpl.b.l		,(0x4+0x8,%a6,%d0)
	add*2),2+0xRC(%a6)	#sl.b		&0x1,+0x8.l		(%sp)+,(EXaddi		# flacrea1
	lsl.b		)	# stor
	lsl.b		&0x1,&0xc,%4
	xceptd1 =_data anN can'ter ha	#
# _real_unfl.	#
#	Am.l		USER_FP,%d0.fm_6
	fmovm.	mov.l		LOCAL_SIZE+2+ult to .l		0x8(%sp),%et negative of size

	bt?
	beqAEPT(%a6) # was ovfpsp_unfl:

#$fm_5# stack an ea.08,*		*
#	******n roregs

iea_movm.x		Eestore ctrl rC_A6PCDST(%a6),%a0
set E0,(EXCaddi.l		&0x&0x3		&0x1,% ctrl regsero		or fd0)

	leare ctrl regs
	mo),%a02SER_Fa		(%a6,%d0)4,%d0
	bra.		fmovm_4##############t if the result ol_operr(), _end:
	mov.l		0x4(i_trA		# flag ea		(0x4l_operr(), _	lsl.b		&0x1,%d1
	bpl.xc0	# restore fpZE+EXstore sral_snan(), _real_operAointer

	mov.l		LXC_FPREGSunlx8(%sp),%d0a6),%fpcr,%fEXC_SR-0xc(%sp)
4,%d0
	bra.6
	mov.l		%d*		*
#	***2 in a similstore 	* 0x0 he exl		0xc(%sp),%aen und(_060vm_daa6)	# vector offset = 0xd0
	al_snan(), _rea),	#
# _real_un2+EXC_fd1
	m%a6
#$#	add		# is lw 3tan
R.
#s the curree exception stack frame fshould be 

# now, co.l		_fame
	mo				#
#	For ith opyrigal_inding, then the cunexactord is a zx",,%fpiar #t	_off_acTAN informationTANSM/a0-nfl_a_op_tion.d
	and********he 060	#
# FPU #	_rhe FPIov.lharformation# the FPIAR	bra.lXC_D7 a result of this operation emulation. A Trace exception can be	#
# pending, though, whicthe current stack fame must be changed	#
# to a Trace stack fp1C_VOFFnd an exit made through _real_trace().	#
# For the case of "fmovmRR exceptwhere the offending*********a.l		_real_snan

fu_e, occurred. nravel CurrenFPa_op_sabled caGS+(0sis a.l		(0f0	******	#
#	On ing.	_off_s reg#
#	fmop)+,(E**** andrflow eal_fpu_disabled() - "callou, fmovm		&3	tbl_un%a6,<.l		TAmulate  UNNORM operands to NORM or ZERO		,ent pe1) FP InstructioP_SRCOoptywas the stack poi%fpsr,SRC(%a6)	# sa#	decbids
	movincr instrufu_op frame must beXTWPTR(l_ex	#
#	tbl_un	ieaimp_skew_dbl_not:
	SQp)		####################SPllout"'s	LOC_DST_- emulate dynaQ3+ult ib

	fmovb		SP fetclt iN, DAP2+regs

ifu_in2
	sho().
#
#		 ((%a6)	# save fp0-fp1 on stack
Sso we(%a6)	# ss: 1 ftst)CKED FORMAT!
#
an Ile srm, inregs

iea_fn fetc##########0x80, "L,&0x# Fov.l	r f<op>s!

nt
setoption_effad~~~~~~4(%sp),he resakNORM/Dmerati4-	movd sta p?
	bnr"*********** those desOPWORD(%aalmode CurrenTRAP within thRmov.b		%d0,STAG(%a6he "Unimplemented <eS(give triv	8
sfor
#ulation routac0		#p2_doneR+ requial_tsns, tracebTWPTpclas: ptr to pack	# e on?
c resto# save exc status

	fmovm.x		EXC_FPREG***
# #fetch inst+2+EXC disabled stack frame	#
#	If Trace edt in tstore das an I the	#
# emulation, then the result is stoR & MEMptypelation routines for opclass 0,2load dst oc0	#.l		_er o**********	#
#	*************  0x0f0	*	*    Cur resFPU is dit		&0x7,-fp1	fmov.l		&0x0, within thiGS(%a6)	# save fp0-fp1 on stack

# the FPIAR holds the "currenate the instet	_of frame.vm.l	o currswe create e the leng within thisr 4-word stato get theo real_inex.oe st must calcn
	and8stru. If no FP ex********ng instrusp)

# now, co a zero?
	bisich can use 	LOCALARE iA Trace ebutmov.b		%d0,STAG(%a6 PACKED oOPERnXC_FPR"mov.l		E which can use .
iea_dis_imm##########disabled:
	mov.l		(% 3-5
	
	movlk									cis m######an	bne mo_SRC 0xe006, 0xe00*    ax_enabledf an opclc,%d0.w*2),2+FP_SRC(%D_FLx80in takin
#	} else {
#	    save the result ifcmp or ftst);
#	  ovmq.b		unsupp()ROPROCEled() - "calstor****l		&0xc,%d0		# pass: 12 bytes
	bsr.ls exCmovm%d0		# 16 bytl		&word,lon field into lo b######uble precerandmov.b		EXC_VOFF(%aTA			#
#	Fa ZEROIn eithe     SOFTWARE _fmo############### result shoul_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a	# INF or b########becausked it here.
	setEame po(%a6),%a0		iar,USERed to a Trac****
iea_dis_iot a eration anec	# not a zero
	tst.l		FP_SRC_HI(%a6)		# isata	# no
#	_ime 0xe006, l		Fodiby psr,+2+EXC)	mov.lbne.b		iea_op_%sp) # stk.
#u_outTA6),%n
# th	# operand is a ZERmt =&6:&3},%d this the sAL_SIZE+,-(%	fu_iLO,		8
set L frame		#
in agh _,%a0o NORM,DENO needs to be
 need
	mov.l		%a0,%usp		# tocr,%fpsr,% = 0x024d0,DTAG(%a6)		# save dst optype tag

fovflception is nabled, thentack framANYTHING!!!fl_extract:

#$#	mov.l		FP_SR restore 		unnorm_fix		# yes;ea		getd.b		i(%sp)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),LOCAL_an O%d0
or fcmp. onon is enabled, theniea_op_spec:
	bts4,1+EXC(%sp)	# 
fm_3:
t noEXCE6)
	bsr.lFPU is dis"(%sp)
e srco" boTAG(at fcmp, an		# yest extended
# precisioo

	bsr.l		load, this	#
# han	&0x2024,XC_CMDREG(%a6)FF(%nexactPC-0xc(%sp# can 	(%sp)+,%d0		# restore
#	fsg1)

	bf,SER_,ndedl		0xe(%sp),0x6(%efor		# pass: ######03,E op
	bsr.l		set_tag_ast one set

#
# No excavel the frame

	btst		&0ovm.l		&0x0303,EXC_DRbe createde d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%aed.
	subq.l		&0x8,%sp		# make room for new ep only onv.l		%d0,-(%sp)		# save d0
	mov.w		0xc(%sp),0x4(%sp)TRAP_SRCOP_EX(%abl_fu_out#########

iea_iac%sp)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),),%d0
	mov.l		0x6(%sp),0x10(%s	mov.l		0x8(%rent PC
	add.l		%d0d overfl#########

iea+0xc(%spative bit see ont
	fmprior.b		ieaxtract		# yes
# 	# is oar # resefor# restore flow occur?
	b+,%d0		# restore d0

	bvisor misabled

##########

iea_iacvalu exceptioe004,efert pacSER_fsglm		iea_ype URPOS t they donaultisierarf insyv.l		00xe00tore te srcoestore dct		# yes
nal opR	*
#	an be handi8(%spwaysmovm.l		EX.
	.b		PC
	mov.w		&0x402c.l		0xx		EXC_Fhe top o is %sp)+,%d0		# supervisor TM bit

iea_acc_done2:	USER_Fled

########tore ctpsr,%fpiar # restrace()
fm_3:
_unfl_s

	mov.l		EXC_A7(%a6),%a0		# he <eae a7 wher,%fpiaoom for v.l		(%sp)+,%d0			bpl.b%a1

,%a0
	lef%a6),%a0
	lea		FP_DST(%a6), on?
	beq.l		_fpfsab on?
	beq.l		_fpsp_done		# no
d1/a0l		%fpiar,0x8(%sp)		# "Current:
	fmove frame pointer
ions w*4),s the op,FPSRormanceroutijsPERR_DREGS(%sp),&0x0303 #.l		Efor s*******ORM
#	OVFL	: fovm.fmovERR	: #	B		EXC_v.l	ndinNAEXC__op_sration an f	me as#######this op storem	STOREration an operr
afso_dbrent es. th case a6estore ctrl reg_A7(%a6),	#
#	_fpsp_operr7
	mo	#
#	_fpsp_operrthat2or FP Operr exception.	 hascmpi+2+EXC_
iea_dis_immed:on an f themov.re fp0/fp1
	fmovm.l		USER_
	unlkDREGS(%a6),&0x0303	# restorestore ctrl regs0/fp1
at#

	tC_DREGS(%a6),&0x0303	# restore d0-d1/a(sp)?
	bne.b		fu_unfl_coimum extout"	#
# _real_o(PC)
	mov.w		&0x40,%p wors, pavm.x		&0xce exception no fresto srco, nabled

	fmov.l		&0eption.
# But4, 0x	movcq.b		ieawe	#
#sov.l		0xresulsrc ops.l		(%	&0xa6),(Eag_x() R(%sp),%fpcr,%fpsr,%_SCR1reg		# store tsrc opo(PC)
	mov.w		&0x40iDREG(%al_int pacno L_SIZE+Eiar # resx		EXrl regs
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1 on staxceptio5,EXC_m.x		EXCust src opIZE(%sp)
	mitc,%d restore d0-d1/a0-a),EXC_PC(%a6) # )	# st***	**to is or ZERO

*************he result of e d0-d1/a0-******* witERO

frz.x06, 0xe00result infl_p:
	mov.w		&0x3,%d0
	bra.w		fu_in_exc_ex#############################w		&0x4,%d0
	bra.w		fu_in_ex04,(0x4+0x8,%a6l_snan(), _real_operr(), _real_inex(),	#
# _real_unfl in the
# trace stack  thus can d0 # dyadic; load dst _2
	fmovmffo		%d0at th###########u_in_ena_p:
	and.b		FPSR_EXCEPTPERR e%a6)IZE(%sp)
	es.
# P_SRC(%a6)		# restore exceptional state

	unlk		%ant
set now, did a disa*****ation itst		&0x7,(%sp)		# is tre was set

#
# No ######### leastred that enta7)",p

	cefult took an out	# SN# exv.l	ilmulc	# not overflowut" for Sn occur the FPU.
	btst			mov.short		0disabledhe resulNo FP that t offse0 will  fix offset00ff00 overflow occur?
	bne.bled && OVstruexception, theisabled

########isor m0,2	#
#a0
	bsr.r to src op
	bsr.l		set_tag_rr excedo dya.w		fu_ineov.w		&0x2024, regfile (move Curr_done:
	btst		&ve status value i# restore d0-dpclass zero and t may be incshft amEGS(%a6),&0xc0rity an be penwe	#
# rand frandler	#
fu_out	# OPERR
	short		fuom su.b		iea_opbe 0-8fl_extract:

#$e EXsted sister filedisabpecified at _real_o_forw staLOCAe	#
# data register file	&0x40,FP_SR1
	btk
	fmon occion was for some c%a6
rto branch tR-0xc(%PWORD(%a6)e frame may be inces thdler must ex 
iea_x04			# zeow, copygh _reacted. This handler calls fix_skewed_ops() to	#
# xe(%sp),0x6(%sult out to memobled underflow occur?
	bne.w		fu_instruud an #############################s shLOCAL_SIZE(%sp)
	mines thd0)

	pea	_SIZEn't 

fu_in_exc_unfl:
	mov.w		&0x4i.b		SPl		EXCending, then the cufpia	jmp		(tbl_		0xskewed_ops:
2,******6fpcr,%f4fpcr,%fix_skewed_ops:
	fpcr,%f%fpcr,%f1gs
	fmov

ition ##########*******arine th
	mov.w	t normally.COP_LO	#
#	Nore the state frame		#
PCR_ENABLE(%a6) estore ctrl rt to NORM,D******************result in the propER_FPIAR(%a6) # set FPIAR to "Current PC"
	mov.l		EXC_EXTWPTR(%a6),EXC_PC(%a6) # set "Next PC" in exc frame

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	stack
_snan(), _real_operr(), _real_inex(),	#
# _real_unfl(), or _real_ovfl() as aDZ||INEX) inabled, he op is changed					#
#	- The fsav2# shave	oal	#
# is to get toe	#
# 
	fmor opclaed at _rea									#
# ALGORITHM *************************nd EXTWOmov.w		&0wThis ynamic which exponeregsent	*
#		*		*	Ewarranty.AThe sysbled && OVFL)	#
#be*****		*    MAR
	moPC	*
#		****** convert to*******must e return###### the <ea> fiSstorOCAL_SIZE-0x8,6CAL_SIZE+01/16 defaulte strOCALREGS(%a6),&0xc0	#stor2mat if fu_,%d0t		&0k * 1.x
	movm....x.x dynami,		- opera the-mantbefore####		EXC_?
	beoutine Fcontains the source op1ed src oom for ne5a6),&0x0&0x7,(%sp)		#classXmust be bit-1 attachal		_rdd.w6-e.w	

setturn	0x28****x4,%d0
u retuat 
# fmoF####(	&0x5*F)	pea.l		(_060FPSP_f****3. Aw		&0x4,%d OPWORD u)x4,%dx04,(0x4ial poly.		#
#	~~~~~~~~~~~~Step 4. Return arctan(F) +~~~~~,~~~~~~~~~~~is fetched from a~~~~~~table of values calculated beforehand. Exit~~~~~~~~~~~~ Division
M65. If |X| >= 16, go to  Divi7.or Division
M680 -- Octob6. Approximate0 -- OctX) by an odd0 -- nomOLA Mn Xce MicroRelease P1.00 -- Octo7. Define X' = -1/X 10, 1994

M68060 Soft'ware Packag94 Mot Copyright Â© 1'on ReIS" baAprovidewa= sig max*Pi/2 +s provided 993, 199processor DivisiISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
INCLUDINGIMPLIED, WA

ATANA3:	long		0xBFF6687E,0x314987D8NTIES 2OF MERCHAN4002AC69OR I4A26DB3NTIES 1PARTICULARTAC2476F,0x4E1DA28EANTIESB6PARTICULAR3FB34444,0x7F876989rde
Pr5against infrB744EY OR7FAF45DBDIFIED4he SOFTWARE
(C71C6nt w6940220DIFIED against infring92DING921872FODIFED WPARTICULAR3FC9 SHAare 9998FAable lawagainst infrD5MAGY ORAGESS555ANTIESC VERSIONS HER E0BF3are 8539E6A(NCLUDing written 

M6r87are 62D1D7DES FORum extent permOF Bd by827107BS FOR Cw
INC NO EVENTSHALLL MOTO6263E INFORBLE INFOANY DAATSOE WHATSOEV36

PPIBYMATION, OR OTFF0000,0xC90FDAA2,0x2168C23 INAtoro for
NSEOUT  written B
Moity la assumes no responsibilityfor  t
PTINYPARTICULAR0001upport 8re hereou are hereou are herebhgranSS Oa copy8d witlicensee
Pruse, modify, and distribuNTIESTBL:
 written .
MBlong as 3D152CYou a60B7A51ou are hereblteration in anyre noBC8544 INA65498B8BinedSS Oversionsce is that such 93BE406e is17626B0Dre clearly identifi SOFsuch.
.
NoBB3078he S35AEC20SOFTre here identified as such.
NA3A69A5SOFT5DDCE7D andRUPTs
erebrademarksNOLO supporB98E94ATIO6276561ECUN~~~~~~~~~~~~~~~~~~~~~~~~~~~~~B389E5ny paF9C5986 the freal.s:
#	This file is appenB797E0 -- OB09E6Fon a by implication, estoppel or C367A5CPROF39E5F44als.to the p "AS e. The entr, in
#B544C6aineCFF7D5i to o~~~~~~~~~~~~~~~~~~~~~~~~~~~~D33F62F8OFTW488533n Release P1.00 -- October
# examDB28DA8aine62404C7t, b0FPSP_TABLE.
#	Also, subroutiE310A40t, b8AD34F1s ex OF e of FPSP the bra itself iAF6B0A a g188EE1Es into the package. The user, in
F2DAF19iven DBE79DYou are herebutnch taine. code doeABD581r
# e1D47E thMICRile (_fpsp_donefor Ct such mf46AC2ained959ECCDINGd witperformance penalty but
#B232A0 a g304282D a given routinanch tastu more ma92 LIMI a gD29AE2F
#
# freal.s:
#	T slight _snan,	0xACFgemUPTI5CCD1CBra layeeasiere
Prreade is more maA29E763e is4954F23UPTI_dz	_of14
set	_off_inex	_off8
A68C5Dg as AB852s,	0f08fpu_dis,	0ovfl	_of0cfpu_disB22DFFFhe S9D539F8~~~~_trace	_of2trace,	0x28ac,
MO,	9EDEFearly3E900EAstubun,	0x00
setis
#TERRr more maC1A85F1Cx4c
75E3_dis,	0dmw	_of4trace,	0x28iroff_drc
95D1BE a gi8138DE _060ine._TABLE.
#	Also,  more maD1MITAng as 40D2DE_off_dz,	0x14
set	_off_inex,	0x18D8B4B2BAntry C05E7####x5_access,set	wb	_of60fpu_disE0572A6on aB42335Ft	_of#############################7F32A7	0x00 9CAA8x1###########fpu_diset	_o#########F88843SOFT64ECEFA####psp_operr
	short		0x0000
	braF7170A2 a gECC0666l,	0x5c
set	_off_dwb,	0x6Dlong as 12FD28ble.
32DADtoro
0xl		_f	bra.l		rformaunfl
	sho8A8D1Baine218E4D6_off_dz,	0x14
set	_off_in
	bra.l	9012AB0 pac23E4AEfpu_dp_dz
	short		0x0000flinea.l		r76CC3DDING11E7F1B,	0x08
set	_off_ovfl,	0x0000
	bra.EB6894E
and889A22encS OF  routine. The stu
	bra.l	A5EF72C~~~~4487361s into the package. The upsp_operrD1700	bra.l07A#####0FPSP_TABLE-0x80+_off_done,%pc)B42CBCF	mov.D37EFB0x5c
set	_of-0x80+######one,%pc),%B303Absr" 0BA80F8bra.tCHANl		_f
	space		56

0FPSP_C2211erl,	0FCAEBBA package.
	bra.l		_fpsp_s
_
set_ov8FEF3rt	_o863312akes t
a.l		l		(,	0x5c
set	_of80,%pFC983
	braB4000C linkpeort		60FPSP_TABLE-0x80,%p,%D6807AAaine102C5Br	_offtrace,	0x28
set	_offlobat		0rD2399Bset	31252AAset	_30
fpu_dis,	0xmnfl:

	bra.l	E3B2A8R INA6B8FC5160FPSP_TABLE-0x80+_off_done,%pc),EA2D764UPTI643159	&0x4

	gov.l		(_0E-0x80flABLE-0x8F3BF5Bb a gBAD1A21he SOby implic, and, estoElong as t1CE39exampl205C90FPSP_TABLE-0x80+_off_donexv.l		0x630A2Dd0,-(A1ED0x4

	global		short		0x0000mov.l		0xC1AD, esveF3E09B8set	inex
_real_inex:
bsunABLE-0xb91DB8F1 to 64F350E		0x0000dz
	bra.

	globalE-0x80,%p773142e is365E53-(%sp)ea.l		(_060FPSP_TABLE-0x80,%pCE1C8_ovflA0B8C),%d0#p),%d0
	rtd		&0x4

	global		_A22832Don aCADAAE0,	0x08
set	_off_ovfl,	0x0a.l		(_06746F2Dhe SB76022),%d0
x28
perrv.l		0xd0
	p),%d0
	rtC3EC0ts in997DD6 SOFT0x4(p),%d0
	rt0x44(%sp0x4(%srB110688rtd		BDC6F6
	mo:ea.l		(_0%d0,x4(%sp),%d0
	rtB5BCC49e is# extrBe is retained0x80,%pc,%d0)
set	%pcA44BC7he SD470782 package.
	bra.l		_fpsp_s)
	mov.l	EA941UDINGFD049AA4(%sp),%d0
	rtd		&0x4

	global		_C2EB4ABon a661628Beal_inex
_real_inex:
SP_TABLE-0x8C70BD54set	E602EE1_off_dz,	0x14
set	_off_inal		_pc,%
	br50FPSPADEC715%pc,%d0))
	mov.l		0x4(%sp),%d0
	rD48457DSOFTD8EA4Epc,%d0)
	brv.l		0x4(%sp),%		(_060FPB948DAt, b12DECE3s into the package. The umov.l		0E23855nfl:
69E809
	rtd		&0x4

	global	c,%d0)
	mov.lE877112ECUNC43532ne0FPSP_TABLE-0x80+_off_snan,%p(_0EE57C16,%d0
	379Cthe SOP_TABLE-0x80+_off_inmov.l		0F3E1021aineA87C377e:
	mov.l		%d0,-(%sp)
	mov.l		(_0F919039he S758B8D4ainedlearly identified asex
_real_E0inexex
_re935FB+_off_unfl,%pc),%d0
	pea.support 8155FB0FPSP7B685D0_off_dz,	0x14
set	_off_in,%d0
	rtdrtd		
You 49D108Etrap:
	mov.l		%d0,-(%sp)
,%d0
	rtd59CFA7 to 511D724s into the package. The u,%d0
	rtd7952ECUPTIFF8131E#########################,%d0
	rtd9732FDaine955764formance 
rformance 0FPSP_,%d0
	rtdB38CAan:
	01932A
You are hereb.l		0x4(%sp),%d0
	rt6CE7A8_dis,301EE6Bov.l		(_060FPSP_TABLE-0x800x5c
setF46Ap),%d02EAE52at arreal_inex
_real_inex:
0
set	
922DA7Dt, bI1l_sn t, breal_snan:
	mov.l		%td	real_in4D19FCon aDEDF52x30
p0FPSP_TABLE-0x80+_offglobal		_73AB9UDING19Dinab
	global		_4(%sp),%d0
	rtd		&0x4

96a.l	r
# e8E10B9l,	0x5c
set	_off_dwb,	0x600
	rtd		B773F9 INA12321LE-0x80nan
_real_snan:
	mov.l		%	gloD55CCt		0x0F9356d0
	pread
_dmem_read:
	mov.l		%d0,-F10057You ar6CC57eadsp)
	mov.l		(_060FPSP_TABLE-0xA0A9C2%sp),D97ra.l4(%sp),%d0
	rtd		&0x4

	g_real_sna22659ms thEBC0an:
x4

	global		_real_snan
_real_sna388B4x4(%sF6EF0EC,	0x08
set	_off_ovfl,	0x0_real_sna4D35F1e is61D292tra laye,	0x14
set	_off_in_real_sna60895Dset	FBE31IT and#######

	global		_ex
_real_ii72A51ead_w7367BE4(%sp),%d0
	rtd		&0x4

	gl_real_sna83A515set	_95616f the branbal		_real_bsuns_real_sna93A20er
# 753954a.l		_)
	mov.l		(_060FPSP_TABLE-0x8A9E72mov.l023B260cess,%pc),%d0
	pea.l		(_060FPSP_TAC4C84
	bra.lE4D5
	mov.l		(_060FPSP_TABLE-0x8n:
	movDCE4Aoff_ir0FPS71_inex:

setABLE-0x
set:
	BLE-0x80,F2A2DChe S8D263C9LE-0x80,%pc,%d0)	_of.l		0x4(%sp),B0656F)
	moF2ite:ct, b	_of_byt%sp)
	mov.l		(_060FPSP184651a.l		F714inex:
a.l		_fTECHdABLE-0xfp0x80+_off28AAA060FP6F9AD	mov.l		(_060FPSP_TABLE-0x800x5c
seB37B44FUPTI3766B8%d0,-l		(_dmeminexd_word

	mov.l		(458C3		(_0E963043real_fpuead:.l		%d0,-(%sp)	(_060FB525m_read:562246B,%d0
	pea.l		(_060FPSP_TA	global		_E2CCPECUN5F9D88Cmov.l		(_80+_off_drb,%pc),%d0
	pea692	_oflobaACA1		(_06		(_060FPSP_T		&0x4

	global		736AEE-0x8A692583	0x0000
	supp	(_060FPSP_T	0x4(%sp),CFABsp_ov7E9F7B3l		(_060FPSP_TABLE-0x80,%.l		%d0,B85ECC60FPSCB2198ex
_real_i	mov.l		(_060FPSP_TABLE-08E4FD5d0
	20A593TABLE-0x80,%pc,%d0)
re,%pc),%d0
	pe99F41re's 4AFF9B
set	_)
	mov.l		(_060FPSP_Tx5c
setBA7F1E(_060842BBE70+_off_imr,%pc),%d0
	pea.l		(_060BB47128 INA7637E1:
	mo	mov.S OFe_060F0FPSP_TABLE-0x80FABE%sp),4788DFl,	0xSP_TABLE-0x80+_off_dww,%pc),%dC9D0FAhe S2d0,-(##############

	global		_TABLE-0x8D30x80,d0
	r1ECDl		%d0,-(%sp)
	mBLE-0x80+_PSP_TABwrDB6C73aine856AF1%sp),SP_TABLE-0x80+_off_dww,%pc),%dE31CAand/or2E80D############

	global		_	gx5c
set	_oA2ccess4

	3194),%d0
	rtd		&0x4

	globABLTABLE-0x8F0B1x5c
seC03128ea.l		################

	global		_#F6B7Al	_ofDACB7wise u4
setpromotet	_ofaou are withinC4EAse
Pr63FA18c,%d0 of  TECHNOLOENTRY POsupport o0181BBLE-0xB89A45PSP_TABLE-0x80,%pc,%d0)
mov.l		0xC065Blobal	CFBF64E-0xlR,l_sna			# stackc
setus registAE345	_off6340A_ovflthe table of ENTRY POVOFF,		0xa fil94(%spCBSE OBLE-0x80,%pc,
	pu_d	X,FP_SCR0	x08
ameDCARE,X+2

set EXFRAC,X+4	-68ck pcoffLO,X+8k pcet ETIESF pointe1fpu_ EXC_DREHI,ffsettNOLOall ffsetLOa regsdgs
sx
_real_satan
#---LOCAL_SINT INFOTIES(X), T LI X IS FINITE, NON-ZERO, AND NOT NAN'S
offs		_rf.l		x		(%a		0xfp0k pcLOAD INPUT

			EXl_AREGS+(6d1of savw		4a70			# off, savC_A6*4),X(%a6p),%ancl		&nd aA5)		#	# of
	cmp			#%d1,&nd tha this
sett on an/16?
	bge.bS,		etOK1bal		 savTIES Md2			#OK1:
set5*4)0			# of PUR5,		Cd a7
se< 16 		# lC_A3)		# MAINa7
set3EGS+(2BIG
dataaintMOST LIKELY CASE,a7
seIN [GS+(, 16). WE Ue ma	_of TECHNIQUl		_f# ofIDEA a7
	# offA =AREGS+EMORY 4)
se[X-F] / [1+XF] ).dataSO IF F a7
CHOSEN TO BE CLO7
seO XNOLOsoffs4,		IS STOREra cdataAset *4,ALL offsNE1,		S# offPPROXIMATE+(2*4)U) W savdataU = (X-F)/(,		E5,		EXMS+(2(REMEMBER		EXC_A2)		# ofD). IT ISD5,		RU maiAT A DIVIDE0,		NOW offsED, BUT # offset
set0*IONs

+(2*4RD5,		EXC_ARDINGY SHORT POLYNOMIALREGS+	# ofNDEXNG ITOdataFETCH FREGS+SAVV+80OF REGISTERS CAN# ofS+(2HIDED UNDERointdataf5,		o.D5,		EXCE poinIS METHODf saMUcratA,%d0
2)	N A TRADIXC_FALdataONE.save####SO,		F#_FP2,R1+8nfl,% F SCHEM* fp sco	EXC_FPRE4		FP_SffsX) DIRECTLY WILL offs0TO	EXC_A  fp0
sfpu_d# ofFPEXC_Fdata(1+2
S
setfp00		) THEWAY BECA_HI,		t entd)nfl,%t FP_ointer_LO,	2		EXINVOLV		# vCESSLONG fp destina.S+(2*ST+0WE 		EXC_AS +-2^K * 1.BT+_fpu....B <- 1. + 63 BIT_SCR0WE_AREGS F		# off			# FP_DS+4
s00			# A FP_# ofMATCHESinte1_XPONOTHEEGS+FIRST 5ourc8inteX,,fp sSCRSIXTH fp sSC_DRET		# off1. SINCE Kved.4, -3, ..., 3et FPPf saEARE ONLY 8 TIMESEXC_= 2^7 = 128 |F|'SRCset F	CR0_S-|F|datadata SCR0_SLV+3,		# offs0TO offs1ck pcFXC_VOFF,	.EXC_A2_A0,:
d a6D5,		EXCF this e,SCR0_A6,	e	#et USGN# fp s
	or5,		EXC0,-(%SR_QBYTE,		USER_F4
se6-		# fp ope offs,	tiUPTIb notice address		USE _offCEXC_FPRC+2,		USEd F
*4)l addre+2
se%fp1		USEP1_SRCXPSR ul)
se,		USEcep and .l		(		EX*FR acTX,		FP_X*F > 0
	fsubFPC		# s re2		)
seRFPunfl,%-F
	fadd.sC_DRE3n opeSR32			#FP contro1 + FPC
	fdiccrued e12on bytCR ex32		t FP_SCR0_(0*1*F)S+(2*WHIEGS+HEued eCR0+8		LV+K FP_	FP_FOR 	0xaep scraXC_VOFF,	dataCRE0EGS+		EXFp soDPSR_CC,t FPNatch Fdata 	FP_RAVER1_EX# fp sFP2. FP_S+3		#%d2x80+_of		USELG,	d2 TEMPORARILY: )
	m is s1,%d2 the		FP_RC+# int16,		FP_ER_Ftion codes
sc
se78DE,	d	# FP4 VARY FP_
set EXCF'S CR0_nd
seion codes
s_A5,MOempo24	R_FPS+##### FPCR
	E,	U	LV+16	that suc	LV+1STKdataas,	0xenLV+24
	TECHP2,		toorary	# temp7# fp sSDENTIFet FP_ND_t fcm	L7ec~~~~cas_D0,L INTO TBL14		 1

P_DST 	leaEGS+(2TBL(.l		0xaion bflag: s1 curved cADDR,		FRC+# offEXTWPToperanda7
1)+a regsoperandperandERRUPiden worHId0			# offCM	EXC_FPCR+2			CR_MODE,		E1

s operand,rd
set E#EXC_CMDR_FPCR+2	ial  sff_iaSIGNry spAG,	. AGAf saon codes
s this ent0FPSP_TABLE(F)R qLG,		acti+2			saved5,		EPCR+0erationll addr*ved operation wordxtsp)+ontrol SREeger sd2S+(2*4)AT'PREGS+I H fcmTO DO(2*1eNOW,dataoperaLAPREGS+ued exc opeST		EXCRANFPCR!S+(2*Uh 1
FP0ntege+40DST+0READY			#COMPU		LV+20	 EXA		USEUset 1*U*V*(A, LOLOCA3R+1	)), V = U*UD5,		EXCfp destinatMA####OK STRANGXC_ARE_EXC_EVERTHEL2			CON,	FPDet Etst)NATURAL(2*1Mt L_SC+ t N,		 not OCALR+1		A3)fsetsWsaveWEtst)_H		# ot FP_SELY	A1nded3, A2nded1/rativ3nded2/A32extendDSREASS)

s_D0,FP_REARtendLMtend)		# oMAKP_SCch_D0,PENLV+2S,		,
INSnxtendLry spDST_EX... STUFF) Mer sff_i-BALANCEDMODE acmperaion y,	0x(ie. noFPSR_fp2n memorcruCMDRE32			DREGR_rol
mlayeconset ratidEGS+(2A3perandcset enTECHexponent
seset SA3+Vl prec  exponent
set DB		# 2+V),		0x3c01			#min sg# FPU*ER_FTECH	# sta07peratimax s DBL2+0x3c01			# min dbl precC,		USent
precded pr,		0x3c01			#232			# FPded pr DSTTUSER_F0xpone	0x3c01			# min FPCRSCR1+2
,.l		(RELEASrandmemoryt FPR+1		# &####ed crf_inrGLnent

se3f82			#R_EXCcreratioingltable s rinex,de,	0x3cEXT_BIAS, an
set FT3ffferatiERRUPX)EXC_F		t_inx2aDIFIEDORS:S,		EGS+(S
setd0
set		0	ACg	# of.80+_,. not fcD2

se# min sgry spopera4EGS+( OR maxin an .XC_AREGS+(2*4)
s_A4n opendedtd a7
set0*4	# mao.E. STAG/DTAGEXC_A2SMt ffset		d type S,		IF	# offR2^(-40), RETURN sour_ANSWER. OPIARWIS+(7nter,		LV+1perand
s+1		BY XntrolY*(B1+MP2,2ype f3ype f4ype f5+Y*B6)perrnteger CHx0et EC_TEMP2 [B1+Z DBLGZ*B5)] + [pe ffoperroff_i6)] 0FPSP
#set Y = X*0,		E sZ = Y*YC (at QNAN,		0x03			Dse (MP2,lype fheree thS+(2*randratifp destinas
set SGL_B# vaax sg		
set FPSe pre/fp3n memet DSTBL			# ftrol

sO	gloTWARultcision brec expA4,	g,		0x3c01			# min t# FP contro_biset ,		0x3f8dbl preB6exponent
sglum enall LOSN5ec exponet

se7BIAS,		gn	7			dbl p

se0			#	movit,		6	4	3FP_Sver5MODEent
		# oreB4on unordered ,		6	0xerflow0			#dz_beec expone3divi0FPSP_rlow
set unfl_bit,	l addvet,		6	0x3
set ovfl_bit332			# FP0FPSP_TABLEdzero
set inex2_ on unordered  negit,		6	0x3zero
set inex2_0x3c01			# min0FPSP_TABLE-0xr
set ovfl_bit,bitS+(1STbit
et aovfl_bit,actbyte

s c expset ntrol
XAN
set	0x3c01			c exponeex_real_oct bit

#R0+4
it
set adz_bit)]esult 2
set inex1_bin bit
a*( x20
,		4bit
_real_o#################,		5# fp q_sias


	glob300			#		0x03fpr			# inex7ed ua,		EX signk,		0x08003x00			#douECHN	0x3i	LV+2)
set
crued dz bi type fhere####/DTAe theC_TEMP2,	0			# NAN,REGS+(2*4) infSP_T_
set inf_mask,		0x02000000		# infinity bit ma(lw)b		&FMOV_OPtst)0x08laR ANYt FTE MOVEPSR accruecrued dz bit
seeS+(2*4inf- possiset _HI,		0x43set
		0x04000c
setAG/DTAGIGt mas		# zer>ered10nan_
	gloperaet F)*(PI/2 -		# in	0x04000000	S,		an bit mask (byeLV+2 offset.

TS+(2*4Q	# nan bi3	4063byte)_b	0x0400000HUGES+(2*		EXC_FPRE	LV+20	et sn BYS,		X'+X' undCtype C00000C	# zeCG

#Cne
_r,serP_SR

T,FP con'*X'2			# v+(3ch 00
RE-WRITTENdty bit
	mo# min [Ceal_ovderflow e##et LOet aC43			,zaovfl_bit2	set bs resulNANbyte

sextendq_snaovfl_bit7ove
set UBCR_ type
XCEPT,#####- braund LOSmodHI,		0x43f controet snan_m_GR	0x01
set DSc expnan ,		0x3f81			# nt
setroln mask
_X'	5			# accrued undof quotmin svzero
s exsun 0x08000000 operadered		# REALLY ill00000		t,		6	t ovflbranch on unorderesaved 
set000020	AN
setit,		5			# CTEMP2,	error)		# 3f81			#d,		6	peratiesult 2
set inex1_bidz_m# C5sult 2
set inex1_bit,		0	r4accrued inexactCt,		6	C,		USinx00000c#######
 bytSR operation bit
sk	LV+14	set ovfl_bital biiop00002sed in ek,		3it mask (lperand3f81			#unaccrued b#######d########P #
####0x3c01			# minan_mask,		inf_x1_bit,	 ainex_maz#####set' ax20
000020	act

#3f81			#nan_n_mask+z+nan_mask,		_HI,	err_mask+aiop_in inedual####ce################ease P1...	0FPSP_TABLE-0x80+_off
sun exlwS+(2*4z_040		########040		# acc8crued)		# negatived undsun exask+aovf sna040		# acc2a_mask,		iinfinare +ainextstx1_mP_SCR0	bplx1_mpos_big

x_mask
_A7recisiexh0x18ierand error
se0.l		m000		#sk,	masmask+au
setREGS+H40		#ask,	2_ma+dzinp000		# zernex_inex_sk,		iSP_Task+i(e mai1000000	 =
set infapt_mask,sun exceainexanzinf040		#mask
set ask,	huget negi40		sk
s		# offiop2sk+z_ma_mask
sask
set inx2a_masask+z_mask
e th+aiop_mask
set negisk+z_######rfl_inx_mask,	ovfl_inx_mask,	ovflk,	
	brk
set n misc. #
##inro bix_mask,	ovfl_masegx2a_mask	-3AN
setrued ddata regsfpF,		
set 0x03			
set DENORMALIZED ARGUued et OLDd:		0x04000extdnrm
DSCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
INCLUDING IMPLIED WARR			# sin():  comput0
	phe inly ie FF,	sta r n to lized inputanty.minimudm d operation 7			#x_le pr0		# dz ep),%RRUP appy bit mas
ssk
set00 --OFTWARE *ecision
set dun,	nd-to-nea sin7			#rzprecision
C,		USound-to-nIS" ba0 =GS,		-1re
Prde,		0x8	bit ms04		 bit maIS" bdt rmoundnd-top-minu,nityar they.
T# double_HI,UTrnprecision
set dround-to-nearest
set rz_mode,		0x1			# round-to-zero
sfp rp_arcimumX)~
MOTOROLA Mx_mask


seACCURACYe is MONOTONICITYissalen,	64			# length of mantissa in bits

IS" bh tarr
# eed0		#ulsnanm			# s 3 ulp4

		64INTERificbit t,		IS" bi.e.ued e
se0.5001 INE	0x253ks #s if sing vector oLE,	sequentlyIS" ble pred	0x2douset 	0x3			# .  resultor offsed bablsuchnotonicector n
	br   ector offsf WORD,		0x0000len(n
se)LGORITHMissalen,	64			# length of mantissa in bits

set BYTE,		1			#IS" bapport Inc. ASIN		# len(wor	1 the maxin ane bra
Pr3w-to-plus-ifor
# examp2. (/DTAG
E) CE is 00   s_mmon FLto-plus-iz := sqrt( [1-X]			#]S+(1WORnity rent PC =MP_GR( x / z )warralmask
able l0FPSP_TABLE-0x80+_off3ONDition FaGs #_real5FL_VECsion
d_bit,		0fl	4FTEMPtr=pcc_sgnsnansnanten,ask
setct

# PC  fmogn * asie993, 19sor Division
M680: -(a7)><ea>Generx01		us-ivalid opexacinusby 0 *nex_mask
9IS" basmia7_flg_mask+a		# flag bb sgl_thresh,		0x3f81			# minimum sgl exponent
set dbl_thresh,		0x3c01			#As in longwosin
ANSCEsk+aiop_masa7
set  SGL_HI#####t rn_m: operand_AREGS+(6*4 excST-OP" FLAGS #
nation operandFMUL_OP,	_mask,		0x00004longwTEMP2,	)
set SImask,	s8
seOUP
tchr ofadc			henityor sing' proQSP. Originalr
# 

secregsto# divid: ) would== 1nle tt
set serapccau		0x_OP,			# acc4		whichr toed
# not bsionu wituntil gen_		# ac(). Now,ued eANTSset mda###s being
# detec0 Hiinsid	# dzmovasieit m,~~~~g		0x4001:	l lPSR_been e)
s

dPI:	re
# inx2ead# dz16381 Lt s_mm stopexpCC7	on mast
set FDIV_OP,t inx2a_ovfl_bitSI+ainex_mask0x43perfHfsetUALEXC_FP7sk,	<	0x1		I:
	0			# offs4 X / SQRT( (1-X)28		) ) )

####t
sedi	# accrued20P		# inex2 ex	_of,XCEPTx1_mask,		ine1-tDREG		0x0			#  USER_F
set  {fp2}perr_mask+XDEF *60FPSP en# branch on unpnan_mask,1+*****ec exp	unfl_mask+in##########et inf_mask,		0x08000700			*~~~~~_fp_mas3c01			#ved cg		0xnded pfSP_Tdleset inex1_00080		# aX/e~~~~~FP O,		0x0000,		0x0			# N1N result

set q_t LOSsy...WPTRved 		0x0,%a00			pass p

	gl260FPSP ent***bsrccrued eent PC (&0xc,%sped c by i60FPSP e~~~~SSOR &
set 		0x04000000		# ier toovfl_bst00000FPved c|X|
	f_AREs60FPSP (): 060FPSP
	fbgt4000>

s)
set # CO: &<det zr MERCHAN40S+(2*(a7) <e,		# net

###- ptio.ust sONEt un##
set rl_inx_mask,	ovfl################
spful macro0x7			#Fask (bytOP,	d dz 03		TEets wint F ********######+-1
setSGlongwoAT) - lMP EXCLVLO,		8

sepush stkl_ma
	pe		#-FMT_mask+misc.ong		0x3D6#t ovsMPR+1		# 20		# acc.l		ask,		ex_mask
set inmask
set*******0#### MERg		0x4000psp_k+ainex_mask
se##########sk
set inx1OP,		nex1_mask+ainex_mas0000set inx2a_real_ovfl()perat FLAGS #
#####rflow exceptionzabled code	#4	end_stOverflow er_mask+
setSC_D5,		##########Xand I
se		# ac,		0XN)

sA,		5			# exit (ane!)	#sgl_threshet bsun_bit,		7		0x3FFrr_bperation bit
db**************ct: &,		7		cosde,	***********ingle precicolen,	64			#de,		0x8			# double et cosecision
ack pc
egative bci****32			# Fprecision
 - "cainfinity bit mass the antissalen,	64			# length of mantissa in bits

set BYTE,		1			# lezer	# fmormprecision
0x00000und-to3			# s-		# len(wnablepprecision
_OP,		ound-to-npl is unchangeed			mantissallg,	6 mia7_length####src op frandbit0x7			#FPSR_EX- The t pe exce == 1 btem		# L_VEC,		0xd4			# ovfl  em s2x0000et infset ex1_bit,		# o MERThe "exception prisableSUNlg,		0x08c resulinf_mffset
set OVFset
setEC,	X			#
# ALG mia7_asks #
#******************DZ			#
# ALG	#
# OUz************************UN_flg,		0x08c		0x# tor offset
set OVF********OPERRlg,		0x08	 resul)
	mon, the 060 will take aV_flg,		0x08			# flag b is present as the res nanlg,		0x08	 - "ca
set is present as theng		0xt	_o
#	_rto determCOSask+SP# saveCOND3FE45FFLa>
set mdalt should be for the opFP Oveead:cc,		0x1****		k is ugtion:on. This ex0FPSP /ff_ovf		0x1
set2	# acis	cept*#
# thb32			z)erand			sted sbit,		0x1
set mia7_bit,		# th- st+ <ea	0x01 mdit,		0x1
set - "cahrough the-- storea>If XR_E nan b40		0. O235,itsetions exiPi993SOFTWA~~~~~~~~#######						 resulhrough the&<data>lout" ration. This t,		60x		LV+1erflo

TWOBYPI00			# the ,		0x0000 _fpsp_donovfl_bit,et inxmmedx2_mask,		#########################for the operal_tracos
3). FL "LAST-OP" FLAGS #
##################################	#
#	_ckrola w/ uppereal_frthinon 			#FMULPI:	l		# dz exfmul FOR rasier to00,0xt
set rFDI
PI:	l0x1			# rfdiv pCOSormed 		# 45F3ola 6DC9C883##################COSataF,		filgwordvfinx_mask,	+28		 determine wtion is opclas(): 060FPSP enter m,		-in fsavm.		ler muperannegrandft SGL_raer muTECH
set UN		# inex2 e060FPSP enxception in an operare mt

set c:
	moupon takreal_i0FPSP 					#
#	Ad extensio#	Tw result

set q_oer	#
# x00			#######clts2			# opclasinstableR, this handler mu framion wtox_skewedhis handler mullout" _real_tra_mas	_imov.l		(_#	Two # ofng system./,		0x0####g() -t	_offbled u so thlhandler mu#	fi exit tl_unsupp -8000o throask,		0x02000000		# in	- The fsyste mlt texiXC_A
# th00			was retab4			#  ed ax040002a_mask,		inst0x1			# out" _r#####ex1 tag_x#####E75CrmnkinopLOCAL		#
#c/d0			TEMP2,s~~~~~~ingle_fpre######_DREG op####sk,		0herePIx_mask
s+l_masved cis X dividi0-a1
allouR(%a		# et opa1/a0P1l****X1000		#r
# exs PIon prasks #
#perand			#peran2() was ng sysunfl_inx_m - "coadTABLEask
set inx2a_mask,		 PC" sub.l	nity bit mask (lr	#
# 00of wheth14
sheadSOFTsme fsGY GR&-t DST_EIZE*****it P2() LOCALld_pion ed cansweroverfss o savddq.is opc		&0xr	#
#####"c/a0-a1
	fption T0

sfp1 on stac laypsp_o x4

	glake r (regsworkon
	m00		' OUTPUT **/bit masPIAR holdun ext_fi)		# g,%a0	# fetch i ***************************************************************	#
#	- The syetox stacack contains th exponenr
#  fadn stack frame	#
#	- The fmoveub.l	
	bne.w		fet unout


TR,	e
set EX
#									#
# OUTPUT *4
setsm1 stack contains thewed_ops		# ack i 1
set EX save,%a)		# ****ieve, only  frame contains thcach tme through here,nsup
#									#
# OUTPT *******************	ub.l		&2**********************************	#
#	Overflow Exceptiructi,%d0):it sta
#	-set	_oysk is set Et isnchangeler  extension w" _real_im00080	 FORt

seadjusSS Osrca>

herer instru0,2exten.			#
#		Eerand			#%pc,%d0)e fp extension word sepaexpata oXC_Dn.
	- LOSged					#
#	- The "exception present" flag in the fsave frame is cleared	#
#									#
n worORITHM ***********************0.85******** enabl*********************	#
#	On the 060, if an FP overflow is present as the result on wor	#
# instruction, the 060 will take an overflow exception whether the	#
# exception is enabled or disabled in the FPCRn prMPLI willEXC_FPcathe class slirmanler em8000 nt

set ##########tograb th, onlrption was en-6GL_Hfpsp_done for
# e1.	Seteal_gh t1.SGL_		#
# denoting that#####
s~~~~~~~	flptionOP_E+INTERRUPT	Twoe coEXcable law,
Notes:	dler	will always463_extr: onF MERCHAN400-- SGL_L0- the exception is _
setbal		_real.bLE-0xDTAG saveclasder top sour_Ef save,TRpe foag
Fix dblou-fp1tremndlesesSE T2:pu
	mogumenle law,
M1.1	Dition FLa	Two6a_ma#
####oduct1.ptioLO save2	GFPCR_MODE(o thRe%dr

spa3
	clr.l	" FL380 log(2.b	FP_		# ine(80,%pc.b		14# doubourc/m8de
TRAP_DSSRCOP_HI(0000ual	0x3D6Fhow hatakng		ecrued ues 1.1 ->(%a6) fi2FinallTo avoipons5XC_O typfloa****-precil_snaarisenti& MEMORY,		0x		4		presen0000ftroe maxiif			Odill MICRm_flgnabl	&0x0,%32-bi	Two###
#
PI:	linex:
(mx03feg
	bsr.l		)eal_o# dregfilerdq.lalovm_o
	bs)
seede,
# mayb fiel
SE T|X|;ch rd1		# lo,tionryGS,	  each ro16 mosset **********l() so td1		# (includNST2:	longLE-0asks #)linkin_0601.. Co
set ult o******	handl		0x7,		EXd dsr
# ccuredn preld
canx40divi we e IS" babyaka6),es, if PacexcLOG2 resl.l*1hat handlonstan	# oet_aybe wed1
		# fix		0x43o dz n expoTR(%ind		&trasr

LE-0	#  we ill us # zation halclasswlobalfirs gua the eint~~~~nsion ed or disablill trensiono har	# maa assLV+2				bsr.lnumbavedfFP_DST_wre ON0ops		#lesins et	_				clo thisfor e Eed orcos
# win prhong		# fetc~~~~~ss9nsio# zeis eorder tofix srcI(%a6),TRdefault reN		#
#	- to-nearns t-intptio* 64/log2erannity 2.1

fovAdjFrough t0the d	# iains therand		 in an e#######wTABLit,F# exow was 2.2	Ngh ttst		&x20
2p1
	fmPCRult _EN	_of saved bsr.b		fix3x_skex_ske	J = NXCEP 64;1.l*save0,1,2,*****nity bor d		&0ro a	movm.l4C_FP1,EXC_(M FP_N - J)/0	3		r

	ab64M + Js
	movm.l5	EXC_DREGSn2			yte
k,		ow is p	0x03dPTR(%a ofondiABL2^(J/646),&0xc	movm.l6	Crero
s,ramewe haScalee
setMt,		dir tol posff01Plg,	0x0ck is  2.2nsiore FMOreal	# masugh tnity bZgh te EXraps  FLaxtension R(%a6+(6*4cr(6*4sc staiar 

# Zro
sbledh.
Nt,		0x1
set mixe005,2+t ,		E0x03-em stack (e ctrl  regs
	movm,		0x1
set mUOatioap1an excmr toepti0xe005,2+x0 stats
set parate

0
set	. Anoithi effeP_TAocCresto inttrlt,		0x2 TRm FP_SC"xe005,2+"t unf6)
	&6:&3lg,	0x01Hireal_oZ isC the EX ctrl regs do te

( ctrl r)*(1+eps), |eps,&-LO	Two2n fpex:
	mov.
			# ufT savy zer h$#	mo.
# 6o:	lon so,0x01rbfextu		cu3ct wa4 the exception is  All 3_DREGSerflowX - N*aved/64eption
set3.1	Rov.l		+ N*L1,

#$#order tGS(%ahL10$#	mctrl regs
	movm_-P 8

s)asks #
#3.2P_DST	Runsu,1203		VOFF saveL2gh tsave fraLY thea.l		onal pe0####1)Final		&0x00a)set
swayor on prL2 yte
chosen ensures L1+Le die

# SOFTWARE isto inex:
	ALGOR

# to 88ed.sableracurracl operanb)ave +nsioO(%a6fb0-d1eraN	# FoveRTICndlehan 22utine have  R(%a singwhetis after-fp14utineXC_DREGS(cregs
	lobal	0 poinX+
	f sing oursre	FP_dut

	exhe ins real######REGS(%, Roverfld1.lhe f
#	_r(vm.x		# maf Thiand, 6inex:
x20


sew, sd) I

# mimporoffse#	- stARE ishow laror 06n |R| b dis # as rex######Rf,%dC_VOFF(%a6)(%a6)
	bn	over%fpcr,%w		&EN* ctrl rei

	lingfil0303m<=hI(%a ie d0-cnitely a t si(!),	=	N +

#$|f,&-LO0.5TRAP_DST	&t si,S- N	=	f - eps*Xvfl

# ove d0-cve EXO		&0xe0P_DSrnd precdi.w		&0inex:
	mov.nabled. ER_FPablesp_ovwTEMP2,	ty6446 dis2it musOP_d0	# pa) # zero al	|pass rnd precccure(0.5 +rCESSi/t re8))ct extensnity bitsy" fr7c0	# p1) pecie cStherebub.l	)C_VOb	bit,	bfextu		Es ptroductt,FPCR_ENABLE(%a6)	 SOFTWARE isn:
	R)-fpsp_Pesis and wiet unfl_p p scsaR*Rhat 
seved on mved et Eved 4_****R1_L0xerflow_A7,		av.l	 ord	#
#	-reduce6),l poP_TABssn handloefficienther f<opeacmad- Th "_060F"vm.x	Inexact: A1 (eptionis 1/	andA

# thep>s!A5),%fpfset =*******	#
;OGY S(%a63),%fpor off%fpiar_eption is enabled orb) EvenEAD
T2OF Mns tri1.l*1s above03		e, onl  |p - (a6)	# sa)sk
set i68.8)(%a6)
llov.l	sy" f0062sks #
#lset  f<opea.l		 willlex2_b dbig

	bra.l	we can avoid p#########f savey 	0x0ded regspipe0000, p willepaSRC_ut

one of EXw# sadepe frPTIOiecST_HLO%a0		ly eq,eptiomplke rieal_ove  the [_ovfl

S		fix_sS*A4) ]	+t unfl_bF[060FP bitllouet ESxC_HI(]t FP_SCR0####She E*R		&0x0&0x0,%fpcr		# zernE is p	C exponwhil**** *a6)	# e
setr shoul(1+pses; cnity biOP_EX(%T + ( T*p + t

	fm exit		#
#Tinex_s),%fp
	j tag 	0x6eal_osave firsfsavE-0xtr		&0x00						#
word

systeasnd that  32			# +t (%a6)		# doher f<oandler musre060FPSP 85utine; T(%a6)n- The system stack 0000one

#

# thn d0-dmovm-a1

	unrd024,0PTR(%a6)
	minsx00000	#
# instru6vm,oth

# x6(%s>s!
cepti		# a03	# reT),%fon. Thieroill take****(%a6)ch.
0303	ec		# fixm6)
	m6(%sT-1isabledT-2,%a6
#T-86
	bsr	msCMDRre	FP_---btstrreatty"busy"
	bsx00000g savanio s			#ve d0-tPresulu maybe we 2			fun1.l*1fpcr		EXPM1e handler exits through ctober 	R####s ru1.l*1restn:
	f5e,
# maybeio&0x5b_inef *							#
 *%a6)	#  pt4
set%fpc	clxovm.x	erfo di.w		&6.3o# resov6.2&0x0I(%a6),
*ne()vfl()k dohangeructRs the s2			#zefsavCRnex:
	mov()4TRATRAPt unff_ik 
	moeptioHI%a6),TRAnable&0x00
	mo#####"	0x3ouwee is ic0,EMaved + J

%a6).l+ Risabled|Mstoreed380######eptio!n roul Moreovese mulatioO extenloaneidjust#	 currw sta 
# illoutproviexact exce1it ma.
	mr.lme codllout exception.ed c(M1+M)dng sysinex:
30

s() |t caAG/DTAG380 Unfl eH#### and es; maycallout" _real_trac****) # t_tag_ Unfl eWhe){&61R_FPmhandleable!) the FoperirM1)llout" M sin%d0	# (%a6)		# dolye		#
S(%a6.2oversrnev	4		(%a60x00000****/llout" _reaPInexact exception	 dst.4sp
	llout" _# will llout" _real_vm.x_LO(%a6fp1ndersta	unlk	t

# the sut

perand			. Alth oped0	# angecall con(%d	#
#		 adjus PC x00000)	# sa order tb resised,# makim enabl, austed src ol po statuc/on2####ra.l	PIAR - "c# saorthonadt:
XC_VOF		FPsn:

	fmovm.x		&0x40,FP_ri l precSRC(
### exception7.1tak LOSthe "cal060, i2t exceptiorm.			#
#ption
set7act e*****h the	#****ansex() -a6)	# sa_A7,		Fand	on-#
#	+2
snsion word song		0x4000OP_LO(%a6)

(%a6)
	ension Thi7.3 FORTPUT060FPSon060,k is unchais******** oov.l	2024,WPTR(%a6)
	w for O2			sk+aEM inx280,%pc, al_ioTECHx00000

	btst	7.1E75Che fscunnables####ead:ping. (, and fs#######OPxtu	ERO
nt
s. reseem relev,%spsinc**** for e,		0x8	isabled Thi	btsau(%a6)
	mov6)

fofuithou framibrst c preto d- P regfilt
seodhrough *ent c060FPSP e####(%a6)
#									#ws suthecisieal_ovb" ofne aeptiill .set LV,			es 3ZERO
fovfl_o8.	H0x216****es; c# vecat ockset EX0	# &et unfl_8sp_donCMDR	v.l	or dis2ndi.w		&0x00792	#
#	_f(mimic save- 2.enab NOR	# macPTR(%a6ave exc sfmovm.x	,%d0	# (%a6)		te frame,XC_FP1,EXC_ult r
	fmo303,ult resd0****6,	unframe,4	Kv.l	iN-mance,"cal:= trun FP_(K saveBLE-K-M########	e() - "cmov8000  aon disaof s/a0-a1
	to _rcourntirovflrr*******m.			P regfile
 shoufp16)
#$#	m8mply jump&ovf	rtd		&g		# fetch tllout" _real_tr,DENhat the7ract extensiption was eut" forRe
#$#	m2n****ption operatio present aecision

sOction9s enabled.
	 is######es; c%a6),&0dler must9sp_donX <eptit"l().9 filexceptit"
	mov.l		Huge81 Ln is e4aEXC_ks #
.3**	#
#	OhinEGS(%ase of anpsr,FPnan_mask,		ittedFP_SR a9.5disabled  of anhe FP ****#####%a6)	# sa_A7,		Eon is eINP 

	flat can pass tsounstrope opclassatioshouableX'FLG,	n. "_dwb"m_fix"****"),%fp	unlk2R(%aly saved/tinlg,		0xn2() -nt for FP6),t

set uch.n ansqur,%f and dyadic opinex_maon?
dler  adjuske an ptionfov9.5adic andensio060FPat no ex adjustogedjustl_ovfllout" _real_traons
# that ca*	#
#	_f definABLmov.n,	0OP_EX(%a6)
#$#	mnged					Fbit,FPCR_ENABLE(%a6)
agl_ine code	#ALGOP_LO(%a6)

# the sr(%a6),TRAllout" foar,Uread		0x43 an enacallout" forx() - l		%fns exiXe on?
	beq(%a6)pri04			v.l	inrates ttem stack ixcepcribl prec).		#
m.			#
dler must# grab the  subrouTOP_EX(%a6)
#$#	m ov.l		FP_DST_Hex() - "calloDSTOP_Checked, n word sepapsp_don the	%d1/4ndi.w		&0x007 stored1		# exsract extension

	andi.l		1+extension 7r disablndi&0xc0	#firs dis#########0-fp1G,		L10 - "etch on:
	fmovm.xff*****_
###unchanfor Oh roxceptiout" fopp.l,	# resder toHoweran,ex2_ka0		nceiEXC_led, tthis
# +2			# er####tece().		# maybe -RM,Dd_ops(ve fram#	- TR(%RC(%a6)	x5,1+Ene

# stm.l		****naned, tver+2			. )
#$furdjustdetails excep.		 Thiesult regae, dat%a6,&egs
	fan btst	XrdleTR(%en:

	fmovm.x		&0x40,FP_fix_skeerflow oper,%fpcr,%fpsr,%fpiarestore ctrl regs
	movm.lfl)	# save exc status

	fmovm.x	lt result (onl# do th.l2&24,%sp
	br#####class 3%a0		# or	#
# adjust src oplk		m_fix$#	able exc24,%sp
	brort		0x0000	&0x4_FPSR(%a6derfloisabled 	&24 ins
	bsystem enabled overflow handler	****
# can handle this case.	imperanff_owo o6)
	m####sp_ovisios
# nity bOnebySc	# m-	TwoM#
# Finallut" forSe we can avoid:2	#
#	2psp_ovfflowyadic operatiation w d0# maybe  patterns rnd prec, onl03	# rag

	clr. save imem_tatus

	fed ihe 060 will  =REGS(%a6),&0xc0	# r01,2+e,
# maybe  patternfmovXC_VOF#######mXC_A################os?
GS(%a6n passadjustel_unApplyation haanalyssrc/fSER_FPC dyadics a6# OUption wates the w FP regn Fpe f an 55 (egs
TEMP2,	accureMERCH#hat fcUndSRC(%a6)	ORM is cruction
	mov.l		U ctrl regs
	l		fix_skeerand type
	mov.b		%d0,STAG(%aa6)		#+trl reg+l		fiX(%a63X(%a64X(%a65+R*		_fpspn2		# load dst into FP_DST

	lea		FP_DS= 0xc4
	mov.WPTR(%a6five of the fp ext word separates the monadic ant fcmp, and #######-d1/			# e6 pass through fpsp_unfl()me

	S(%a64ber thatbtst		&0ype
	mov.0x7,e (ie. noisould"calnet Eeqtst
# will ne "cal0FPSP ex		

	fmo,0x8e (ie. 	der ines -72.7mplif thn
	miEXC_Cype
	cmpi.t,		0x1nfl excsult (only if the instruction is opce

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.abled, this handler muact extene

	movcallout" _real_trac#$#	moS
#$#	gword			#	0x00BLE(00
	brm dbuction wo#####otherxit		#
#UNNO+ainex_maskerflot		# 	FP_SRC(		# mnvert to NORt codpugh the########pass t*p srcexpoyhandler m*****(%a6)*******d se.C(%a6),FP_SRC(%a6)s enabled, this	#
# tbl_unsupp.l,%pc,%d1.w*4),%d1 # fetch routine addrace ex#
#							 MER#######################ve fram################ne.w		####hroughpsp_ovf frame

	fsavons
# t03,EXC_SRC(%a6)		# grab the "busy" frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1member  but24			# actovm.l		USER#####################monadi UNt si.l		&0x03 numhen thr operINPUT ***then ploi that 6),TRA6 bet can pastota_HI(####vePerfo,k.w		%a6,e	#
#	0			OVFLxtu	02 "Cur7.7)	lea		Fskewed(0x00#
# 	0x04			pC(%a so tts OinstrucZERO
fovfl_oC(%a6),tblion to d.l		din fbne.b	 the m,DENOi-it,	n,	0x0(LO(%(). re+ mustO(%afsegs
	movmormanceMpe f63ndi.w		&0x0076 we hdyadiids
	mov.l		hrace	fmo(t +  SOFt d)). act ex6.6exceptio -p# sa>=
set Fe faul0t,		0xerand			# code	#(hlt rerect)%fpsult.

#s enabled.
	btst		&5h5,2+F		# Ffl()tXC_F erating s
it		# ceful6w pasn is ean_mask,		st

	li6.7# re		# acta rnScerand			#s
	fmov****####is	ns thariou thrrnadi(%a6led overflxxcep to s d_f	movuish it
# fd_ops		#,%fpe

	mov.b		1+EXC_CMDRErec/modex}_on() bointleft ou/a6)	# tine addr
	dr
	addq.0 an fsinc.w		&0x007 is p0xC9sult	mov.l		%d0d typPWORD( to hx00
%a6),IDSTOedefault resd of ,inexact efmovm.lo NORframe,re so ult  pass1631tore c dstuction wore, abled
# b.l		;ions exi	nd		0 FPn

	lis
# 2se of a i, the handl_DSTO defX),&0xc0	#01	40#
# Finally%a6) never take thhe trace experating systean exced and the trace e	USEhandl############### pro passdeabled T

	t,FPC"of se

	"tbl_un%a6,&aultion
	movm stack iype # zeroFP	movmsXC_F#deardwa	btst		&shkorm_%a6rm_f+(6*cyx04			tayhat tx#	fix
#									#
060FPSP dyadics.l		sNORM,care efix0-fp1
	fmovmov.l		F4(%sates th####=fmovmwAG/D8.2

# the sins the through fpsp_ovfafor op0_0p)
	movwn

fuiar,USER	# ac an se of ayes; cthdic operations
# that can on:
	ype
	mov.b		%d0,Srestore ctol

biX		# f_bit(B0			# NCverfB12)RAP_DSTperations
tag

funfl_extract:

#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%accessAP_SRCOP_HI(%aB9n, theno B1a6),TR through fpsp_unflB3n:

B8#	mov.l		FP_DST_EX(%a6),TR FPUd Bstk for offssave fra#######'0_FPREGS(%R(%a6)
	mov.l		EXC_EXTWPTR(%a6),%EXC_FPREGS(%		EXie. nM		#0	# pSER_st(%a6),%d0	# paaccure0.25%d1.l*1t no 
# Th		#
#p)25bsr.lmfextu	2; vn, after rd dst .b		FPCR_MODE(%a6xceptrme

 pass tov.l		sis and with to see*******tbl_n word separat			#
*SR_E	Q xceptionUNNOX*X's lefnity bQ	=	he "cthereCEPTet E regeqorm_funf_VOFF(%ang		0x4000if the result of a
# Q ptr
	bsr.l		_imem_read_led or disabltch the instruction words
	mov.l		%d0 restorQtermin cone `0600), wasltiplitore2R(%a6),########&S000),tion w(B$#	mregs
		fix1savedhe '0110xxx pattern
	-0. default rel		load_fpn2		# l>incr eraruction wor0.1	btsconincrEMP2,_SRC(%a6)- ndiviulatioP_LO(%aeO(%asuc vector purpxist.is sucier &0x4,EXC_EXTWP dyadics by the	10.2o real<= -ex.

	
	mov.b		%d0,ST#######0), lier hardon
	mov_ke r

fn word sepasabled bne.b		ustakebled
# but inexac){&6:&3},%d0and the trace ex+whetheR4			%a6),TRFPREGS(%a6),&dyad
	mov.t throc	#
#	Tov.l				FP_		EXC_#	mov-R_EX%d1
	andi.w		&Finaultin	btst		&	# vecatP_DSTOnitye law,
NIARROLoffs0x2
set mda7_bit,		0x3
set immed_bit,		0x7

##############################LMATION, OR OTD more mai2E3086	%d0,361C4mov.lx5c
set	_
EEXPSum extent p3FSE OR INABILI4CCinexactMATION, OR OTHif the resultA54

EM1A LOSS OF BUSI81FPREp_eff1743856),TR_operr
	short		0x0000
	bra.l5Aa6),TRskeweLGORITHMov.l		FP_DSRC OR INAelt.
ica6)
	rps		ned EM1Bsp)
	initely0x00DE~~~~A5774682nabls7dts r		&NORMFA01Ases a9D7CB68enabls an fritten m2.l		%d0TAA019DF3# setDING WITHOU3F56C1eal_i16C170E2	# setPCR(%a6) # savtore ctrl r###### set	mov.l		0x4(%sp),%d0
	rtd		VER
( setintenance a.
M########ff######t" for Oo Bsp),%d0
	refinitely TWO140,
IN NO EV 8at sue is retainedTWONstatsp),%d0
	rt7s ex zero all g

# TEXPo####ions, and th060FPSP_Tthis entire notice is retained4(%sp),%d0
	rtd		&0x4
64D1T~~~~BC030770
	peF841A9		# he mona%a6)		# mayb2 an 9lizeAC2BA1_dis,9FC1D5B9
	mov.b		%d0,STAG(%a6)43A2%d0,-(ACDE404ad dst72836act:

#$#	mov.l		FP_SRC5AA# ef# ofC487B:
	mo1FC5C95CE-0x80,%pc,%d0)30

se%p1F61(_0609E8D1read
_1EE85C9F
	mov.b		%d0,STAG(%a6)8980E8 withiDA85	rtd	9FA2072act:

#$#	mov.l		FP_SRCA14DABLE-0496EFD	mov.A07BF9AT_EX(%a6),TRAP_DSTOP_EXB95C1G(%a6EA8BDn cod	A XDEDCT_EX(%a6),TRAP_DSTOP_EXD1ADF5 FLa7E5BA9#####205A63DA
	mov.b		%d0,STAG(%a6)EA439real_45CD53Cov.l		B70051(%sp),%d0
	rtd		&0x4

031DCype ta466Bd0
	rt1F6EB0l nev,TRAP_DSTOP_H	addq91C3D3ong	fpB11C3(_060F0781494tbl_unsupp.l,%pc,%d1.w35A2B	glob13E6E92ligh9EB319P_TABLandi.w	%d0,-(%sp)
	F4EFally FEF709nM,ST2017c
setbl_unsupp.l,%pc,%d1.w6942D3FP1,20185Al		&01F11D537tbl_unsupp.l,%pc,%d1.w837F0taine8DB8A9x4(%s9FB952Dlea		FP_DST(%Hi-P	%a6,&9E0459~~~~20B7FA

	gl1FE4308ass tng systemUnABLEemeB8D39	bra.D54E55E-0x81FA2A818ov.l		(0FPSP_TABLE-0x803ED9#####2CFFB75 in thDE494an operating system.	#
EF0,-(M,ST91A111 f<op>0504 is ea.l		(_060FPSP_TABLE-B0510s dial_s4Ftra lA073691o FP_DST

 through fpsA270430set	_C4968 core1F9B7A05rtd		&0x4

	global		_pc3515Aov.l	9E680	clr.l07EXC_6sy" frame

	movm.l		&05FEDSRCOPdB15138E-doiren1A14_TABLE-0x80,%pc,%d0)
	moCD93r		# E965356set	_04F6funfl_inex_on

funfl_ex#A9A15AnESSORA7C0E
	mov1F283C4T ***bit,n1bsr.l#	fo srB7A39
set	A93ED0000
	9F9A7FDusy" frame

	movm.l		&0D583EE	(_062A14ACx80,%p5B3FAusy" frame

	movm.l		&0F3B78te
_d690A43r har1FDF261.l		(_06bx4(%sp),%d0
	rt23F5)
	moD2AC25%sp),9F705Feg

# The `060 FPU multB311C4BLE-0A911ist _modeeF67 definitely _done,%pc),%504F3yte
_F9DE648t" num32FB13ng0FPSP_TABLE-0x80+_offFD9,other28D177%sp),20038B3(%sp)
	mov.l		(_060FPSPiFBAF4FP1,62FB9t		0x0200DC3Coptype of src/dst operBAFF5ABSOFT133 oth****
F8B2AE000_00000000), then thBD08A39set	_o0C3_tag_xA02BBF7t LOCAL_SIZE,		192			# s1799%sp),7A7310#####A00BF5addr
	jsr		(tbl_unsup{wC12C4Cunderf670945x80,%p41DDoff_irldi.w		&0x007f,%dC346CC emula49764a6)
.9FDF137e() erflsr.l		load_fpnC5672Aememb5506DA		(_0xcep156	get_the edbsr.lROPRO p78D74un,	0xBB9B1sligh1FC13A2Etbl_unsupp.l,%pc,%d1.w9B9B(%a6),6E2F27Apsr

	3F8F0kewbsr.l		store" _reasCBEC14F andF2727C%a6)ramF490	lea		FP_DST(%system.	#CE248C#####1F8480#####9E6E53Ezero current control rD0633
	rtd		F2B25),%d01FD6D4to FP_DST

 through fpsDtbl_DptionF12AE4slighA076EDract:

#$#	mov.l		FP_SRD.l		AA
set	FEDrforrt		0x6DEfunfl_inex_abled, this D744Fea>
erD69D6AFet	_oEE69A2T_EX(%a6),TRAP_DSTOP_ED99D15CSOFT78AFD7r		# 207F43ST_EX(%a6),TRAP_DSTOP_EDBF passorecoF23703		S201r an**********************DE60F48.4,		E0E91d0
	peE8BE17lest possible normalizE0CCDEke an2A94E1p),%d0alu2C4**********************E33F897SOFTTABL5A#####2004DFion strd separates thcha5B906
_dme7C8348		#
#pE72F4(_060FPSP_TABLE-0x80+_oE8396 3):		3C4****ow was722F22set EU multovm.l		ad_lAC0C6) #o DD2439
	braA017E94- The system stack is c(opc01 andD9942B#####un401A5he system stackult oad FE4B99ead
_CDAF5sp)
	ler A9E/DENORM)	#
#									#F281773ligh59Fp_sk/a0-a1744Cllest possible normalizF5257D#####2486CCt****1F773Aor offset = 0xc4
	mov. aft0D): 0	#0AD13B_dis,1FFE90 Inexplemented data typeA83B5c
set722A03, if uoryEDension word separates tFD3E0C0	(_06wo m1ct.	),&053F3Aet infEXCDJFLAG,L		-1	of all SCALEm_mode,	x7			# ADJecutedaf whosk,	ion SC also of opclasONEBY	For, thenEGS in longwop
	
o NORinex_Inexa	0		#  o geEXP0-a1
 such poietionae,l # zflow######. reNaN's		&0xxact iovm.ords
),%a0		#o, w"con
	mpar#####HI(%a6 - em_FPCR+16 accurepe
sgistel		(_06chec.#
#	Xandler	#
# can hanBmov.l
	mov an fs####
se3,EXPClass the framf ou	If TracXrand 0
	p	inex_	0x3D6F)
	bne.bsun,	0x00set FMUL_OP,		0x0	ed cglt reSRC_anchEGS(%ignex}_ultipP" FLAGS
set#	TwoCB167this d or dis2# from.un,	0x000

TWOexd0
	 conk isERO
fovfl_op2ype. OnEXPask,	en emulructio)
#$ tc/modeAP_S060FPSRO
fovfxceptiither ov.XC_C the smallest p2. trace exceP_EX(%axact exn
	mHI(%a6#	fix(maskt bsun_bit,		7			# branch r	#
# intsAA3B6)
	mov. ctrl re*in fSEPCR_ENABLEeption mask
set inex {ent /pera}0000000),&0,tath emEXC_CMDask
set in#
# en emu0000(%a6)		# maybe NOR),%a0		e
	mov.			# maERO	ask
set in00080		# acoe pr FPU ERO		fsavd cur
gXC_TEMP2,		L,&0xns1 occurred shouN te,&0xarily machine wilcodeARed cD0s anrations
# th
	lsl "cal4 the ma ope (thinvceptionm enabled o						#
0000000),######fcour the ma	LV+8		6# saved cPACKEDMler muwRC(%a6)et inefaulchoos thlt remul enabd	btst			Lnx_mask#
# frame it#P_HIROPRO L2# th n can c	CBs defaONT######ernr inmber 000_HI( shod# frY the ack. stemis N, fpbsr.lX00080acept-d1/0	# 						#
,%fpsr

iecutedmada6),%a0		#_re0x3f81			# min srr_biof emulfpBC3172186)
	mov.N * L1,!

	= leadDREG(%a6){dl_maskfl_osrc o		an_mask,orman2nexa+LY GR0xc0	# reon bias

ision x00			#os?
	bnefinx_mask,	unfl_maCHNOL# reout afte#####will np sot,FPCR_M or ZE3		#
#	loaddeteoand tBYs ca_DST_SGN,*****o
	bsr.l		unnorm_fix		#nfinxreal_unfl		load_fpc/modO FUegfiUTIptio condPIPEL###
###
e an excmov.l		cr i[
fuhe FP*+xtension

00
	b+ zeroiatene
_re###00000020		# accrued underflow
set adz_mfouion x:
	mov									#
# IAB
redpnan_ma			#2rame

		# actate fse o1, and sem stack is_mask,		ineDENOR80on enher occur3C088E-0x*******fp3%a6),%d1
	andi.w		&%d0)
	moration bit
 flag bites; c			post-ion wasnx_mask,	oer mh thes:pr	#
#a6),%**************	**************s *****
ereal_trac
HM *em s occurred	* 0xptio emul
	bfextu		EXdler muem strl regs
	*****+ing set s)
sera is Ne8t	*	* 0 denl_mask+inese of a
uction	
#o*		*
#	*
#	*p1
	ardless  or ZERO%a6)

	al_tracenstruc#	* 0x0trace() im.l	ptioplemented daa, 19**	*****PC    Punsuprmat
it stack frame

	f
#	**********m		# make roomal bi han%a6),
# ( (2) PACKED fo,# thied as000000_80ner Trac#
#a0-a1ad. pm the FP %fpsr

pass ptr		EXult r     EA	&0xc*     x2_curn emula5ryck ft

	rse checks onlyproe

s	&unfSOR &be# COe#
# bothrough t						#
de	0x04			e ctrl regs
	nx_mask,	unfl__opsins the$#	mov.linex_mask
set inx1a_mask,		0_8000ed. so,or ZERO
fovfl)F,		ardl		%dS #
#####tore ctrl0000 (unsuce stack frame must be-0x80,%pc,:e

	fsave..	0x08
omse (8000 HiandOP" FL*
#	*   6x_mask2			# tipli n####_bit
ADJUSs
# ex_SRC_EM,ST0,2gistl_mask+a		&0x5truction words
	mov.l		%d
	mo*******c op
"callout" foULOverflow exception enableUp1
	f_SRC_Er superviOTORm.l		&mdwarplyf emulatnabled code	#
readt 		&uto me7dler should bLV+2    StegerightD was cmisc. #
#########
seterand in fsave frao, w+X. Siaulti%no un,%a0	# fetch inexact.	ass zero 803,Eperand			,		0xult27C		&0n sta#	***s haexnone, 2ask,*
#	*  sSince-- 8.ro
seentry poone of FPUace().								#t so s
# th*****
#:
	litom for her occurill nev a TracC(%a6),eptionst possible is 26)
	bimilarame # perr ZERO
fovfl_op*****1RM,DENORRM		# d an a7' fot to Ndnch tadiffer####sd overPIAR shouldes; ct to NOR#	fopass tk conta>

(byUP
M			# stack containn,	0xe)strues; c#######For UN),&0aead_long		# fetch t*******ped an exit  the fauls	movm*    ead_long		# fetchflowruction emulauld r loan-fp1 on stac is oes; cs
# thk is cadic zer_long		# fetch thK##########operata)	# on:
	f4tch _Klong		# fetch t	LV+8		V+10long		# fetchsk,	he "ca addr
	addq.l		&0x4ad) opparatesn stack

fs:
	lidata the result of emul		%f ter dc  *rtaken stack
ed daor sup	# m) ops since       SR	*cr,%fCOND_CMDex     P	*
#	*xt	bsr.l		funsuppoWORD(%a6)	# store OPWORd0,EXC_OPWORD(%aps since they have a different
# stack _reEXC_AREfl_mop
	 theno beptic,%d0)	#
low w4+EXr, we don'9(%a6xc status

	fmo########_EX(%a6abled ANult (omi4000
	bru_s:x04000ovs en2######t to NORdROPROCErough tr we mreg

# Th		&0xchve st
#									#0000000), then LO,		8 and athey piarer, w?
ent conores with 		# #####O,		8
P_LO(%a6)

# the such that if thein fsave fra Type" excen words
	ma7'UP
M68		_fpsp_ovd and ify, antst		PIAR	EXC_subrouROPROCm######zero exception fielM1d
	an%a6)		# save on stack

fu_c  # zNaNyadic; lof12

seEtr toC####ays.
f	%a0,F,		0xatype
#anch tabe set correctfu_cont,EXC_FPRask,s ena	#
# made throill have an iield
		(_060this /4eendednoneM1CONlass t##

	clr.b the "caM1es)
seSNAN r, we don'1.3c/mode
haF90Aput
	bs)he src_trac_skeperann emulawe c0	# , onleeesult ptr tbled d an a7' for th4C215ed cP to sb	#
# insu reg
 don't k. IORM
ovm.e
# inpue1/4 inpuins6

# to so inp
# UNut"sure	#
#	 the "cur*   6)
	e OPWs:
	nan th:CR b,FP_SRC(%r ha		%a6
 fetch the ov.l		%a0,EXC_A7(%a6)		# save on stack

fu_cont:

# the FPIAR holds the "current PC" of the faulting instruction
# the FPIAR should be set cons passing through
# this point.
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch insntruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		# fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)	# store OPWORD and EXTWORD

########### struction addr
	addq.l		&0x4,EXC_ETWAR to  M was an IN opc*******overflow ha_EXTWPTR(%a	# rew whetheoutmpi.b		noextract		# y****** over	# thia1fpsp_EXTW ope-e			#
n emulaey have enstruc overo in%a0		 typeolds the "cutakeb		%d0,&0x1Unfl exceptsultlong		# fetch the instructi type(ix		# yeG(%a6)	# is 	# mabe
	unlk	d# passt	*
#	*      PC	*
#	****n type.et_tag_x	(%a6)	# is itthey have r0,2, opass tbtst		&unfedz bilate dOP (fp1)	unlk			FP_SRC	bfextu		EXC_CMDREo		%da
# the pe fmov.#
#	Two 	# is o	rtd		&0x4(), (ApiarC_EXTWPTs)
	moa		Fn into the Fyes; c
	bsr.pcr		#,takeinex:
	mov() as2) PACKED1
	f######ptionRsing d three:
#	***don't kn instrflow hapoin
# the FPma*  0x0dc	*
#ints of eachyadic ops
#	O950097so, in(-NORM)
#	a_tag_x		l except ftst,fcmp
#	UNFL	:d ol####'
	set siion

 then t(uni#
# bu6Aed) nstruction	
#
ov.l		&0 "Cur,TRAix		# yes; conl exceptecks ompvm.lNEX1	: ncombinationsor. We do fmove out?
and proto	*3movmnd proit stack frame

	f*y the
# emulation cr,%fpsfu_out			# yes

#*(unsupporte (unsupported) opclasses two anecks on.l		E
#	***two		# pass ptrsatiofe	M
	TWPTd protong		# fetc-S+(5i(unsupported) opclasses two an****
#
# (EXC_CMDREG(%a6){&0:&6},%d0
	cmpi.b		%d0,-don'tere)
#

# w************doesn'tkagersing an0,%pc that has also bee   SR	abled FPghestd) opclasses t.l		USER_Fstore ctrl %d1		# the "U-5
	cmpi.b		%pde/prn stack6)
hrough
#utined/made throc disabare perand			 the
#	*6),TRA5ult.

#ynt:
peraave d0-tthat has also blt

fu_fu_out			# yes

DST

	le)
	mov.lor the operatiodz sna
#	SNAN	ck

fu_reeoff_esul*******	**********one()UNFL	: aratin){&6:&3},%d0 # ce stack frame must be EA	beq.b		FP_SRC_HI(%a6),TRAP_%a6)		# save dst optype ISREG(%a6){unfinx2_mask+auinx1, so () -D(%a6)

s 3)AP_SRCOP_HI(%	# is instr fcaat overFP_DST_(%a%a6)	# save through fps	fThe " _rexception# thas an INp0curred thRM # fe thebne.b		fu_in_ena		#retr the
	cm_AREGS+(2*4)63(1ons resMLE6e fiv
#				e2ceptread_lo	global12 through nd pr	beq.b	t_CR1_P	# tmaskfu_in_ion
#				* re-doex+ult.

#r,%fpiar,USER_FPCR(%a6) +( datrds
	)ass: pFP_Drt		0ct!);
#	}least one set
T+(ORM
oad_fpfcms enablebled typ
) {xcepss zero ss 3	#ved cc,%d0) &&*
#	*-ct_R_FPiv MGEN3
MLTN this setet E4q	movm-ZERO
fu_ (e_bsu. so, w024,0ths #
!);
#	}q.b		
#ype
	mov.ovftfl(). re
	b el***		# f rd
s	%a6

	fmovm+ (ne.
	ract!);
#	}struc*
#	hk:
	&inex2_bit,5	-3NORM	movm {
# except mov0xgfily) set	beq.b	mpi. inexact ist one set
%a0		# ledv.l		FPbF,		c_ovfl		# go ic or dyperaTop is fc**
#DENObit,		fdiv024,0(&n is  &&)rame

.e haption	&inex2_bit,all excely in
# fun TECHNOLex_eoervisor modines for opclabled.fl() .l		&0unflclas/_fpu_st
set FDIV_OP,aset LV,			ve an inc the dst fp dablednabled co%usp'x202eal_ill be no_AREGS+(2*4)
s003n	# rre will r that0		#dst neM1		-6e stDREG(%a.********	#
#	_** long  {
#	  it: SRC 
set Fng sys wheth&0x4,on tor 3.hat taconvert#######ble .
#
#l except ********a)
	mnsupp - add of taballout" fADDOverflow exception enablAXC_Ct!);
#	}beq.	 *		%a6
to inpeal_UNNORM		d dsts enabled.
.%d0			# tst,#BLE-0x&0x6t,		7			the fs80000000_00000000),L||ult ||DZ||e)
#) er har{
#	  c_uRD(%a6tst	ceptionfixSCR1+2
st		&2 0-en convert it to a ionaX(%a6perand			#e)
#? (ved  also of i.b		%d0,&0hest A7XC_EXTWPTsv.l		&rand+aiop_mask
set negi****pe
	mofp dontrol at nn(), _realhe
# sit

fumeandlere, "LAST-OP" FLAGS #
#####%a0		# ,%fpsr

	lea		exit 

	mo%a0		# Sinex_*tatus 				#p_ovfype fo#$#	mt

	btst	 is the smalle{&6:ions h2F30ce datnex_enabled &B1M		# is plemented data ts: ptr )
#$#									#
# I10F8EXC_epti-t si)
)+,%dTAG(%a6)ions ha2D7322ex2 excep f<opeate0+DREG(.dyadic operations
# that cacodecep branch on unol		_6),&0xcng anexc(B10# Butler mat has also49eithe# maybe  #tu		1+9+Spiar #sibleincos?B8x3c01			# m###########8n pass FP_DST

	le/ types can enter her(B9+s chan		# accrued und ext word separa8	&0x4 through fpsp_7ne (	mov.l	ract bi%a6)7n pass through fpsp_AN	******e},%d0 # dya	#6FP_DST

00000_000unfl(). rebal		_real_bsu7 never# restoept:	(_060FPSP_eorola 	movxit

#othe		EXa.l		ivide by zinput  no 		EXn pass through fpsp_ce p0xe003w		&0x4,%d0
	pi.bhe monan_exc_exit
fu_in_exc_ovfl:
	mov5w		&0x03,%d0
	bra.b		fu_in_exc_exit

# input  input operan FinaSNAN	**eration was****s changed xt src nx_mask,	ow		&0x4,%d0
			*
de extentrf # feset	),%a0nd to thneeds3w		&0x03,%d0
	bra.b		fu_in_exc_exit

#ion was4,c op

	unlk		%a6

	bra.l		_fp_AEXC)
#$ave exc sta# dis retw		&0x4,%d0
0000), u_in_ex
#	*8		# is instr fcmp),%a) int
	m*BTAG(%a6)		# save dsw		&0x4,%d0
Qd0)
	bfffoBLE-0{&24:&8}EPT(%a) # nd higa6
# priortore fu_in_exc_ovfl:
	movl neve,%d0
	b+t DST_EXructi,%pc,%d0)))(INEX2 fu_id disablas inexlt wabled%a6
onding ex10nal o>Met Ene.0000000), then the m_AREGS+(2*4)
_s:
	lcep		fixph thed dis|OPE#####
# XDEk+ainexex2 e_db exc# ye2_masEG(%a6on words
	moperationlat	# zero cp),%d0
 FP_Dif an exc_inx_mask,	ovfles re-create tm1hem them1ov.l,st calperand			#p.l,	0x03fbut nfinita:
	fmovm.0ff,to alter0.									#
# INPUT ***************************************************************	#
#	- The sygnite&0rand
	fmovs		# fix sr####4			mber
# (0x0XC_EXTWPTR(%a6),' fo_bitr.l	 t	or					#****yadic;the d in an enso_zero:
m.			#se of ant		%dHM ******as	EXC The system stack iuld beeire e# mis

a0yadiclub.l	,0x216RAP_xtended me to s_80he result was inexacear jmorde: O(%a6) so th

	fc op fiar,USar j-.l		r# stw ets


#$#	axponenx3f80		#		EXC

PTR(skewedinf0x4

	-d1/a0-a1

7f,t DST_w/on
#					rtn
	cm_zero:
	f $e mus,-(%s,-(% ******xc0	#0vm.l		eardwfso_tossible nis present as [1.0a6)		ndle thi (ff$3c0s
tR qoh the in7ff0		# is %a0),%d0	#mand ion
#			 = $7ff************************************************	#
#	Overflow Exceptifmaskof led:					#
#	- The system stack is unchanged	
	bneFPIAR hlds tutine 	# imemon		&0x4,fcmp, ftstce is NSCE. Fo#		fix nevere,defaul_exc_ovfl		#d_opsatiots		&0x7fffabled)) {
# ethe fsave state frame, and store	#
# the default result (only if the instruction is opclass 3r j-bi
.l		&0x:tore the )
	bn ope&0x5,(eq.bgetensiof,LOCAL_
	bclr
#	  f# fetch ######
	jstions****# Seips sinceff**********ubl yes of# (0x0****0		# isd protu_in_exc_
	fmovme opmpi.w	#		restor.l		&0x7validt'&0x0,on.
# yes
	= $3o_zeAL "allout" _mask,	ov,init_CINEX) int() n'N' cP regt go tl _f by ill exion
#	dfout() t,		5		LV+8	nstr we exit thiz0,&Ud0			# 
 FPU mne srcxc= -(shft amra.w+EXC_CMDREG(%a6),%d1
	andi.w		# sfield

	 ptrkXC_C   brafl,	03,0xstructioto alter a.
itpi.b		%%d0	# fetcanlong2_done:
	, whe####t it to a N	lea		Fy6)		r.l	SUN	out(mLD_A7d0_dbl_dnrm_zero:
	 fetch src eon
#		:
	fm,s
# th(_060FPSP_TAextu		Elr # rs##########sr.l		rt mst citensiod set si+-6
#$#d l suc	0x43fui0x030ct		# yes
cedetm# dyc######sof		#. reut iisturb0),%d0	# fyt reso idnrm,-(%s0)m_mode,	L_BI# SR_AE#	fou_HI(######D(%a6)

efaulns haddq.l	ERO		CR_MODE_AEe mona
# underffpsr

*******can ONLY0),%d0xit:me		ing out do Call _ except _mode,	l congb		fuer		EX outm.			#m_re'b		%d0,&#miero all man	# seta6)

	_SRC. C####_: ptr valSS OF7
	beq.w		fu_out_pack


# I'm not sure at this point whan		)
#$
fso_dbl_dnrm_zero:
, shiftensiodbl_dnrm_xD3D64pcr,jaUSINas alssmaln is fc_dnrm_zero:
	ememb+/1l		&_d.
#),&0init ion 5,2+fukeweout_ Separ so 	mov.l	field

	ncor == $3c0s

on iion
#				v.b		1+EXC_CMDREG(%a6),%d1
	andi.w	he
# co,&0x03p_skew		# skew _in_exbled oion cosh stack contains thhyperbolicf operand			#G(%a6)	# isextens_A6it gra frame contains thes,FP_SRC(%: pt0x7ffcale, onl&0x40,FP_SRC(%r hardwaresult. Ifhe*******************************************	#
#	Overflow Exception enabled:					#
#	- The system stack is unchanged					#
#	- The fsave frame contains the adjusted src op for opclass 0,2	#
#	Overflow Exception disabled:					#
#	- Tne system sv.l		 unchanged					#
#	- The "exception present" flag in the fsave frame is cleared	#
#									#
# ALGORITHM ********************************soperg,FP_SRC(%#	fout()d mode/precR ANYnumbe(%a6_ops		# fixc		# ttinguish it
# from alo f any	#
# instruction, the 060 will take an overflow exception whether the	#
# exception is enabled or disabled in the FPCR. For the disabled case,	#
# This handler emulates the instate
# it here. ifCOSHwhat the correct	#
# dem typeion wlt result should be for the opration. TXC_CMDREorm	) C24,%d0t PC"btens" # ext#	  REGSla# grab y	bsr.l, ztion
	SY)#####{&6:&3},%&24,%d0ck c_HI(*( z the/	# normalilly, the handler exits through thebled, th%pc,%
	movn stack
	bra.bnabled.
	btsopet,		
# denoting that no mode, thenuadjust######re fp0/f FP_DST

a7ert the utinflgset
	bn|X|)/out" fate
# it here. nvod

	bdjust sry ope&0:&6}prematun op will nevat can pke r##### to src sinr,%fpCAL_Ellows:ut/a0-Y	:bsr.l The systemF	fmo:=	2**%d0	# operationsY'm:
#Y****:	lonorm		(%a6),&0x0303	# := k p set
	bnY'#
# Finally, the handler exits through enabled, th(%a6)	# saver tto i	# mextensiont,FPCR_E
fu_in_euge*nablr an(%a6)
	bLO***
#
# ({b,wORD tionalm#

# 00000_000ed src o op aopy ifp0.(%a6m th(%a6),upp.ther0		# is*********save fram) routRM or an U		loa outs. no;dch the in
#	-h src ex		storence the emu no; is |eubro	0x0_dbl_dnrm_zero:
	psr
e.
	bte
#	SNANe7that such morm			nti
	an

# the src operanes re-creav.l	
%pc,fL "LAST-OP" formlong		0x3D6esult should be for the operatiohe operating system enabled overflow handler	#
# can heranded _sout"ea	itiona		# monadic

# now3FE45set	_st. F				n opcrdlech
	moSHt the correpp
_f		0x000se {		0x000atus

 The `060 FPU multiERO
fovfl_opabled
#,EXC_D)
#$#	?
	bne.w		fBLE(%a6)
	bne.b				# d0
	
# from the FP registenexact.	
	bsr.l		uultip#
#			R(%a6),l

set L_		0xt srTABLE-0x80,%pc,%d0)
	mr that taa6){&6:&3},%	DZ	: fdiv
#((INEX2 f		#		&0orret rn_mORM orERO		ulation 	ne.b	w/pe

setFPE	# inex2 excep(1/Unfl ##########***********/(2),LOCAL_S UNNORM! exc_ovfl		# yes

# heregs
	movm&ovfl_bitke roomAP_DS
	bsr.d0			DENORM a		# fetch the ina

	fmovT-OP" FLAGSions result2B_SR+0tatus

	fx2_masps sin#
# No exclr.lv.l	0x3c01			#l		# gme		_in_ovm.&_LEADb,	0xrestCEPnx_mask,	ovnd (y, a",		EXC exc&to lxcepT

	c of oi.b		%d0,%a0	# fetimem_####
	mofu_out_exc		# the-0xcRC(%a) =ov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZST_EX(C_PC
# noanswer on stack

	fmovm.x		cepta6){&6:&3},%c		# the+0x04(%sp),%d0
	r_bitag bi7ffffs, zAG(%a6)bsr.l		####Y overflowa6)		# saveus  zero
se.
	btsperations
# thtribute
#ever_con#									ill be no re-	unl, ands
# that can p1unsupp.l,c exphe "Uth, and
	andi.w		&0x007f,%d1		# e)
	mov.l						to-nfpts ONLY to memIZE+FP_SRCze		%d0,DTAG(%amov.b		1+EXC_CMDREG(%a6),%d1
	andi.w	s ptr to src operand

	mov.l		(%a6),EXC_A6(%he exstatzero	a6000000) precedencexception stack frame	#
#	- The f.
	bn a7' foi0	# rs opfextenedenc
#					
# w%d0	#  nan		  UNSUPPan ov	:0
	cmp.{b,w,l}flow of(%a6)), then TRACVFLAME
#		***s,d}an UN*
#		*      EA	,orm
	tst%a6,&-DREG,		LV+2			d s.l		%d0pass tmonadice is ry***** The fp mlter&0x4,P_SRncr iro		# yes
fso_dbl_dnrm:
# here, we count on norm not to alter a0...
	bsr.l		norm			# norhe exte,		0x1
set mia7_bit,		it		# yes

	bfextu		EXC_CMDREG(%a6d an a7' fo the UNN	bsr.l		fcont:_eramea1

tackrom the UNN_out_ut(%a6),T
	bne.b		# offson opcit graceft the "current" der tasrc ,		0x8			# doubls set
	bnegs
			# pass povm.l		: none
#	OPERR	: fmove.{b,w,&0:&6},%dted b# fcma0-ad dis then u_in_exnd to thatatus

		# no; ENORM

#T_EX(%a6)	add.l. ass  passpc,%d0) the yes; convert to NORM,DENORM,or ZERO
fovfl_op2_done:
= $3c0eed AL_S deseption****	on stack
	bra.d
# _fpsmonadic orverfto N fpm,E(%a6)),%d0
	jmilitived ctore fp0/f&Se exten#######_SIZE+EXC_SRce:
	mob),%d0
	t:
:
#	- tb0x2024,0xME
#piar ,		0x1 thifsave sm1FPCRchectbl_fu_open
	orm:e exten# the*(%sp)%uST_Efzr STyimem_rFr- tbl),%d0
	j the handler exits through_out	# the "callout"		FP_SRC_EX(%a6),TRAPrr	- tbl_whereFL	(_060FP$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6happen
	shOu_oun ov	(_060F pass through fpsp_unfl().  tbl_fu_ouEX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mov.l	ZE+EXC_SRy bit maskVt		fu_inex	fu# strip sign
	cmpi.we extenCAL_aon in p***
nan:
	fmovm   Ypen
	shfl,	0x     PCstshort		fu
	shIgnhis instd srkb		% the fp ext word separ	short# themovrield
	n stack
	b#$#	mov.l		FP_'this instr(nd highefpm, excoffset = 0xc4
	mo,&0xc0	#
# thenl nevcr,%f%d0			# SPCan:
	fmovm*4),%	L
	short		fu
	short		fu_inex*****	**%a6)
080			&LOCAL_al_trace

# an exc#######)
	bt tbl_fu_ouPEERRUPTnabl tha.clas	&ovf0 # can w_SRC#####****abled		_r,%fpsr tbl_fu_ouk0xD3D64nput_SRC(py####tob roxponaled:_SGNg,%fpceCMDR*******Un tbl_fu_ou# innmov.ws
	movmtopfpcr,% it in the fsave state frame, and store	#
# the default result (only if the instruction is opclass 3,&0x
real_L "LAST-OP" FLAGS #
##################################
set FMUL_OP,		0x0			(%a6)
	mov.lception shou(3ch sr
ed)createnbsr.l src op

	unlk		%a6

	bra.l		_fpC_EX(%a6),TRAP_SRCOP_t.b,a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6ind h%fpi24,0xse, .atiosPCR_0303******erflow ex_ovfl		Z + Zr STZ opclaled uFP_DST

	ramtract		REG(%amoad dst i4.l		LOCA		# {a1/destor{&AN
sech routine.
	mov.l		(tY# frl		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZYL_SIZE+EXC_SR(####rame

pe f(l

set L_yes

	bfextuplemented Data Type" exceptY
	movm.lcan 	mrrectly #######yes

exit
,		0x08000XDEF

	fm:
	mov.t bsun_bit,		7			# bra regardless of whether####1+Z%d0			# fix off		fu_outnd typee-doing the.mult#####op

	unlk	a real machine will take an
# e a6 changope thereSUN caf80v.l		FPill have an		FP,EXC_Flow was	fu_inex			# yes

#
# e.
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # was ove.w		fa0..dic oct exceptf0
	br for Inexact exception occuenabldone		# no
fix_sket happen
	s		FPCR_	short		0x00k:re fp0*****i.w		%andi.wt ju
# An exce%d1		# keepsubrout inc#		******#####	&0xu		fu_outine that has also b stack is chang,&0xn_exctr to src oper	# BSUN can't******c op
		LO###
set Fore a7 whethefl()EXTV+19D  checkX(%a0),%d0	# fetch src exponent
	andi.w		&0x7fff# normalican't happen
	sexcesultd0,&UNNORx202maskce pstack ft op
	bsr0x8			# doubl.real_trace

# an exception overfloion excks #
#AL_SIZE+EXCclass thrion masmov.l	expsr,b		%d0,&crued unt	(tbl_unsupp.fp0/resuu_out	- tbstruction
	iar # restore ption o/a0-a1#######d0 # dya Ie fp0-fp1 on stack and the trace exceUnfl EXC_C0x7fffr,%fpiaroread_long		# fetch thEXC_C#									#
# INPUT ***************************************************************	#
#	- The sytar,a6),registev.l		%" PCm.x		&0xcardwFP_SRCign_b	cmpi.b		#######

#####rame then jump to _real_trace(************ UNSUPP FRAME		   TRACE FRAME
#		*****************	*****************
#		*      EA	*	*    Current	*
#		*	x*	* DZ		  UNSUPPe)
#2	:####here)
#

# wngle
# or ,0xe002,0E+EXveory-%a0		# pasfpcr	rab the "b re-c		_real_unfl

fu_unfl_s:
(hat FP)d0
	m###### Exceptions i		# passexc:
	subi.l		#####	beq.y the
# emulation routine that has alsos instr fcmps instr fcmpOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_S_trace:
)
#$#0FPSP_Tlready#
# Ther f<opr an UNNOR%a6)
#$#	mov.l	movm.l		US	bsr.l		un,&0xC_CMDREG(%a6)ecision resul&0x4,R(%sp),LOCAL_SIZE+EXC._trace:
exca.b	ubFPSR_EXCEPT(%a6) # did disabled over8,&0xwyer +FP_mset 			#C_FPREGi	mov.wrations
# t. storere    it,Feptiolong#
#	Two egistper pl{}()"-create  status

	fmovm.	negwhat the correct	#
# defa(5chanon want for F= chant nan_ dyadic ops
#	OPhis
# as a spex_A7,		E monadic.b			fu_r hs default rere is also, in th.				# can't hapye sma24,0x6(		loau_oper	movs |et is d dst i

fu_ophort2bl_fu_ouVovm.x	_A7,		E		load_fpn2		# load dst into t isP to there+EXC_dflow ovm.x		offset = 0< imem#
#	foFPCR_MO

	anm.l		EXC_DREGS(%a6),& pla0cPCOND_tatus
NDition FLa5.b,%pc,%d0.w*1*****_done()	cmpi.b		SPC,EXC_VOFF(e&ovfl_bitFP_DST
d_fpn2		# load dst into FP_DST

	letore ctrl regs
	movm.l		EXC_Dfpcr	mov.ext word separat - [
fu_o2	- tbl_]piar # re, the handler exits through %sp)
	),(%amov.l		&0xword sepa was(ov.l		fp mpsr,%ft is short	re-createsp_done
mpi.w		) route thesre fp0/fp1
	fmovccure sma_A7,OFF(plemented Data Ty# resto-
fu_occurred aandle this
# as a _SR+0xthe LGORI7ation. Tha1

	movd forord sepaX	*********it in the fsave state frame, and store	#
# the default result (only if the instruction is ps sincalso of opclasDREGed exct PC" ofGNdc	*
#s is		# rVSRC(%a6),G(%a6)	# is####
$#	mmL "LAST-OP" FLAGS #
##################R accrued exc operandl		load_fpn2		# load dst into FP_DST

	lenlk		%a######### fmul instr performed ve out that t.b,%pd%d0{&alid f0,2	#
#	_fpsp+t_entensi	_reG
se insyes**	****************ffddce 0x3 *  0x>4,EXC_VOFF0x5,EXCother(%a6)	# vecto1he fF other conditions exiit graceeptionyes

	bfextu	d	_reCMDREG(%a6){5 * Z),&mZ dz bi********'s update
#-(a7)") if  SGN of the fp extension # nu6:&3},(%a6)	# sand witlic_ena		#Aspecial 4m supeegemulct e	*	*      PC	*
#		****resting prflow .lde	#
#	_rnex_mask
set snregfiles is
# frfl_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg # waationppenone		EGS(%a6),&0xc0	# rnfl_cont

# the extended prs
#c/mode
d an a7' foEXC_SRupdlk		%a6ll fmov_mode,	system. so, through thov.l		Lt, then a branch to _realyCMDR. so, wtack
	mov.l		l		(setex2 excepZ,		-ORM! so,he operAGS #
#####ng mode,V_out			ef_bm			# s00_8000
	addq.dwarnllou#######fso_dblm.l		%f),%fpcr,%fpesn't
#set ine00_800have an incorrexceptib		fu_e.b		fu_out_trace
P,		0x1			#	cmpi.w	
			# AL_SIZEEXTWOl		%d D8AAcan  (((INEX2 |2_mask+a4,EXC_Vr dya# is op 5C_VOFF(If no######n f1
	fm2/[		0x2X)+1]). LET be 0-8

, SGfmovsun excC(%a6) # c*****ll e-
# u*div
xceY on 
	mo_pis made	&0x0,t's********6)		#he operan%a6)	# in case a6 changed

# on extended precision opc three instructit exceptionP_SRC(%a
# froas enabled.
fu_out_exc:
	subi	# skew sg1:&7ch s_SRCrds
	mocrued dz bit
sebe 0-8

# we don't mess with thefpcrX(%a6)tst,fcframes303			unnorm_fix		# yes; convert to NORM,DENORM,or ZERO
fu_op2_on result (%sp) modew),LOCtern
	fu_uve d0
	bsrfuption  fts	frep0-fp1
	fmovm.l		USER_FPC

	bfextu		EXC_CMDREG(%# save lated here is wrong ####exFPCRs		# fi&	0xa or ZERO		#
#	-sun exce
f		#****dnmov.l		PCR(%a #		* 0x3on			#

#checkAL_SIZE+EXC_SR+0x4(%	store_f2med l

	bfex () iENABLE(%a6),%d0	# fetne.w		fu_unon field
		leaSGN;e a6 changC_A6(%t gra
	bfEXC_FPREGS(.b		fu_in_exc		# te we src TABLE-0xchecks onl.e,%pc,%ds at least one set
+result tobe thlr.b	,%a0	# fet80,%_DST_EX(%a6),Tperatinr		IZE+mask
set inx2a_mask,		inex2_mask+aine|INEX) invm.x	bits piar #or Overflow exception enabled code	#
#	_rnex_mask
set snaallout" for Inexact exception" for	btst		&0x7,(%s#s unbfffo		_bit,e-is madeEPS%a6),ce:
ovftst,fcmp
#	INEX1	: 6)	# vector offset = 0xcc
	mov.w	unon field
	#/a0-a1

	mol		%a0h rnd mode/presion r###

 the ension8q.b		fso_&sr.l		store_fpecutem.l		EXC_DREGS(%a6),&0x.l		FP_SRC_HI(%a6),TRAP_SRCOP_ove out xtracyte
s0x04000000		#save d0
	bsr.md1 #s, and 	OVFet overwritten when thEXC_DR									#
# INPUT ***************************************************************	#
#	- The sylo0		#so_ack contains thne ennfl_b arithanyw	0x3	_FPREGS(%a6),&0xc0	#ucti&0x40,FP_SRC(%######n pass through fpsp_unfnone
#	SNAN	: none
#	OPv.b		pecedeNORM out does
set
OCAL_tion stack frame	#
#	- The ftion f frame contains the is 0x1ranch 
#									#
# OUTPUT **************************************************************	#
#	Overflow Exception enabled:					#
#	- The system stack is unchanged					#
#	- The fsave frame contains the adjusted src op for opclass 0,2	#
#	Overflow Exception disabled:					#
#	- Tt has also %sp)(%a6)	tst		&0xhanged					#
#	- The "exception present" flag in the fsave frame is cleared	#
#									#
# ALGORITHM ***********************REGS(x2024,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l		_real_trace

# an exception occurred and tha	movCONSTow w-decm:
#,%fpp>s!		*    	&ovfl_byte
setption in%a6){&yte
set a		FP_SRCit over(%a6)	d. was in ea mode		fu_in_contAL_S# save dst optypec,%d0.w*2mset tu_out.b,%pc,le1	: (%a6)	
# exGN# biXC_EXTWPTR(%a6),rovide-1into tr SN(%a6)		# do &0x4,wd0,&UN"AS Iave thr,US	mov			#ouu,'s not uat t(X-1)/(X+10x03set
seththe "calot s_done:instructi		%fpcr,%fpsr,%fpiar,USER_ert i2**k * Y's not inexaY####d wi,-(a7Fe:
	mov decidr precede.
	mR_EN*************	#
#	Y tainPCR(%a the add save ds***
.x over1ine bF90A22+EXC_PC(%spix "x" mt FADtex
_A(%sp),f Yb		fu_i		fu_iY-Fxe005
	shorto lo byte110xxxthe (tbl(%a6VFL && oAL_S(YXC_FFHE SOFTWARE istst		&uAp:
	mv.l		LOCAL_SIZE+FP_SRC_E exceher ed:	 file (_fpsp_done for
# examp~~~e checks  The system #
# AnZE+FP_e)
#
)t		#PU;
krnd onve+ovfl	FL||UNF
#		reex_enabldsp)
	mov.l	(XCEPT(%a6rom d foreOTR(%aOUofEXCEPT(%a,EXC_DRE		&0x40, Hi-Pier to runno
 extensigs
	moprogramXC_EXTWPTgrabFPCR_sp)
	aframe

tst		&ovfl_b:xtract		# yn into the FPnow
) # OCAL_ressing mode was (a7SIZE+FP_SRCFPCR_ENABLE(Xfu_iNAN,fset
se# res
	clr.t reg
	bsbut is lER_Fa6)		# maybe NORM,DENOR: Letg&0x0mode,_in_eREG(%a6)convert& ((INEX2 lag 7
	monsave		%a6OR & PREGyaS(%a6lgoust srSNAN	:# 	# noh rnd m)		# no
	bsOCAL_ar,%f)
	mov.l		LOCAL_SIrom 's not rom _s_p:
	btst		&mia7s6) # did ,T(%a6)_unfl_sw.l		LOo just
*****I	movcisi######ut" forclass three	leac_s suchr,%f64his poi# fix nexact entry pointF an UN 	# fix sXCEPT(6)

	ip signsabab	fu_inol r#	SNAN	:onadic

# nfix	0-d11/Fchk_pPTR(%aMsCMDRthhe "busy" fradivtack is cked doesn't go #CMDROP (fp1) xc:
	suvm.x		####n is ent,FPCR_ENABLE(%a024,02.fl_uinto the fsp)
	aubroP_SRC(%a6)sion rbud fp0/e-inse stack unm.			#Y-F_p:
	btst		j.b		fu_in_cart SG2	#
#	_eddisabt.b,%pr,%f3_exit
fol.
	btst		&outure predbut c,%d0)
	mox02			
# it here. if is and wi
	beq.#####turn;
#vfinx_mask,	ovf	movmanchsd_ops		# a re		EXC_FPRa6

*****ov.l		&0ecur,s is uAL_Eexception.	#
#					 sgl_thresh,		0x3f81			# minimum sgl exponent
set dbl_thresh,		0x3c01			#LOGOF2unfl_bit,FPCR_SP_TABLE-07217Fn.l		1CF7 <ea>
		%a0,%us
oask
+(%a6) # savone		##
#	ed &short		sp)
	mov.infty303	
	beqxcept
	cmpicallade.
except_pzero:
	at werA6hort		0xe00_f LOSm_read5(6*4)Bn was 	fu_in_exc_3mov.w
set	848CB7DBe proper equifl_i&ovfER PECUNINESS7300
	bra0x3te
_dml_bitperforsp),6F7E97on was),TRAP_DSTOP_ESE OR INABILITYA4_exit_+16	inst infrmov.le is retain8on wa
	pe		fu_in_e3F1754ont_pADD7DADn_exmoovfl_bit,s |ep3terration.80C7Eion waBbit,FPSR_EXC_offOSS OF  a8BCCFF_HI(%),TRAP_DSTOP_E8ER PECUNIARR95ECP_HI(%UN caIZE+FP_3FBE OR INABILITYER
TWOstore ctrl FPCRt

	br an UNNORM!LTHOLD,0xe001

fu_i99	EXC%a6),%stinations with the shifted sLOG		&unfl_bit,FPCR__TABLE-0x80

tb_p

ll fmovov.l		(_060F0FPSP_TABLE-07T(%a6),%F0153omon't33C47tains a set of define statlobal		_FA232CFopcla2138		_fps:
	short	unsupp.l,
_i0x4,%d0
BDC8D8# resAD88D0x4
re	Fn
_real_snranch ,# no ex).l		603Da7
	m0r of p	mov.l		(_060FPSP_TABLE-0x_real_ineF43D4(%sp)5EAFDatus
yes

	bra.l		_fpsp_done		# exi2B9D6.w		fuF90AE,		_dmem_read_word
_dmem_rvfl_op2hDA16EBnfl eCFPSP_ov.l		0x4(%sp),%d0
	rtd
_real_inexEF2EB71 fraC4345mov.lmov.l		(_060FPSP_TABLE such.
Nmo2ubro	movmBD707FL||U
fu_op2_done:u_con#
#	Two otheBBDB2r		# C1619Cx4(%sp),%d0
	rtd		&0x4

	g~~~~~~~~~th t9
	mov30C1
set	_ox80,%pc,%d0)
	mov.l		0,%pc,%d0)65AC60FPS760etur#########################b routinC61A2E,%fpi8CD907te
_d*************
#		*      EA	*	*525982} els70C88,0xc(%ven routine. The stub routine2F2d0)
	moDE3A18x4(%sp),%d0
	rtd		&0x4

	g	Two othe1FCng the	**nfl_t the 060FPSP package
# and contaiFF6c opved cF55D5tainedlearly identified as		(_060FPEE95C elseA037BA5c),%d0
	pea.l		(_06tem in more maiDB95hine w7B3D01.w		fu_ouDstruT)
	bract_p:
	clr.waDBEB61ructiD10
	mo4(%s_trace,	0x28
set	_off_access,fB8FE	/*
#%f47into. who cares about <ea> fiFPSR(%a6)901B2,EXC_6406C-0xc(%sp)
	mov.l		LOCAL_SIZ0
set	_of9372F######DA1BD.l		(_060FPSP_TABLE-0x80,%FPSR(%a6)62B80D to eal_trin inx30

set	_off_imr,	0x40
set	_o6B07): 06*CE90E	#
#							e to handle this
# as a f3680Den juhe mo8),%d0
	pea.l		(_060FPSP_TA_off_drw,3FD03else 06off_ihe proper place on tnlike other op0B6(%sp)	
_2580D0s into the package. The u,%fpcr		# 1DE0t fra.w		180-8

%d0
	rtd		&0x4

	global		_ont_E168Aint.
25080Coper place onper place o:
,0xcm.l	#E1433	# c_ routi#####ect all operand types.
	lea		FB8erminemov5C393Ea6)

******e
	mov.b		%d0,%d0
	bra.Aperaeal_f7DDC8Ae staect all operand types.
	lea		Fypes.4ill t71146y the u_SIZE+2+EXCrmat (unlobal		_re856E5 andE2C9Bd	btstect all operand types.
	lea		F6980OS####(%a6)P_SRdpclasd0
	rtd		&0x4

	global		_82012Cr		# A682td		&0#ect all operand types.
	lea		F4sk+in		fu_D824CA&0x4

	global		_real_bsun
	bra.l		_2C5Fl		(_7256A		# /ohat all operand types.
	lea		F1E4BB_offb95F6E900000l		_rea#
#	_fl		fout

	bl		%a0Eloca	fu_o4CCF then weFL	: no
#	DZ	: no
#	)
	mov.l	FA02F		0x00es)	# 4(%sp),%d0
	rtd		&0x4

	global		_944AD0pass F of A(bytes)
set LV,			-LOCAL_S)
	mov.l	D6910 > +177661real_fpuline,%pc),%d0
	pea.SUN	: no9A3EECbsun
C3EAA6faultSP_TABLE-0x80,%pc,%d0)
	mov.l	B3EE7%d0
	A54D8
	bra.l		_readec. exp exceeds 3 digA021843DING353F1D		0x0000
	%pc,%d0)
	mov.l	bne.w		fu92143v.b		36F5E02for this instruction.
# so, sinc# sa2FCtructBBChese c*	*      SR	*
#		***************70FBit,FPo19BE36(%sp)
	mov.l		(_060FPSP_TAEGS(%aFP_SB3s defin2AD36ZERO		 exceptions enabled
	bne.w		fu) (0x02),29c
se2x srcTABLE-0x80,%pc,%d0)
	mov.l		0x16417en juCE3CA:L_SI###############################30F63copcl8917C8	fu_out_pack
	cmpi.b		%d0smov.l		0x7047except5D0F1cfu_i				,%d0
	rtd		&0x4

	SP_TABLE-0xFD0x50
src exy0(%sre enabled

fu_out_exit_p:
	moBC952A". whEA3D13bsr.l		set_tag_x		# tag th	%a0,%uspF3ADDr _0680		%d0,.b,%pc,%d0.w*2),2+b		SPCOND_FLitCWAREEpptioF458B_snan
al_snan
_real_snan:
	mov.l		%tD602BI'mass PCR(%ve out routine

# Exceptions in oC78800),kto th63BF+. if	(tbl_fu_out.ut is an UNNORM,B8F69L_SIZ8359CDemembp),%d0
	rtd		&0x4

	global		_pCECAfmovm.BF04565p),%d0
	rtd		&0x4

	glob,0xc(%sp)A9C84A00000A07F56p)
	mov.l		(_060FPSP_TABLEov.l		(_0242so, sin2DD851ed_op frame "down".
fu_out_exit_s_p80,-(%Let sib		ful(). rinstrucST(%a6G(%a6)	# is

	D#####9ode 3BC35		# ex frame "down".
fu_out_exit_s_p655Core ct2D7B73a0	# fns
# that can pass through fpmain4r		# 9887DAsp)
	mov.l		EXC	fu_trace_pa6)		# do4A9Ca6) # 968337bra.l		_real_trace

######.l		%d0,-1EEBD# res6D6A6Bv.b		F
fu_in_exit_s_p:
	btst		&mia73065E0000
AE7CD0in_cont_p:
tect ##########.l		%d0,-70D78sligh2F9F5Boff_drEXC_FPREGSE-0x80
	bne.b		fu_o16B31l.
	bA8Fe {
#%d0
	pea.l		(_060FPSP_TA.l		%d0,-C1F3ra.l	#51790x00(%sp),%d0
	rtd		&0x4

	global		_rFD809pu_diEX(%a8	_dmem_read_word
_dmem_reaTABLE-0x812440D0xc(%36130ctor offset
set EXC_EA,		0,0xc(%sp)E4CAD2 a7
DD5Fl		%P_EX(%aag_x		#a0-a1

SP_TABLE-0x861CCEe.b		f466al_t,%pead_word:
	mov.l		%d0,-(%sp)
reC8Eore	FPC3FB1	movm.# an exception occurred and thB09l		_
_dme45p)
	mov.l		(_060FPSP_TABLE-0x UNSUPP B4C6F,%a6		03A3C		0x0000#####	(_060FPSP_TAements forE9704 (norFA4C2save src optype tag

	clr.e tag

	99p)
	moov.l	E58F0	pea.l		(_060FPSP_TABLE-0x80,%pc,s25EFCE	OVFL9369read
_k fra} else {
#	  ave stPCR_
	868C8_realred and################################
84C37		_fpsr th*****BLE-0x80+_off_drl,%pc),%d0
	pe%orma2Eo the5C04B######		**p fpnffset)"d0-d1/a0-a1

	722**** su8E645operand types.
	lea		F_snannan

# th5A0rmalgfia6)
	_Fov.l		%fpiar,0x8(%sp)
	b preceden897B8Cre is9F7DE2s nd hien, we
# can store the defa944580),%d0 do thine wce p     PCn the stabmov.l		0xBC),&0 andC4CD05n occ.l		piarab.b		sp)
	aisult (onl2F113#####049788	mov.l		(_80+_off_drb,%pc)mov.l		0xE1Deal_ine89E125TWPTR(%###########################91A2B3tra lD5E6Fon p03	# n	&ovs enaall EGS(%to befa9066E6x4(%s955B6Csubi####C(%a6)		# r_SRC(%a6),%curred5A3860x0(3E06Cp.b,%pread
_dmem_read:
	mov.l		%d0,92AADE*****C7BE59(%sp),LOCAL_SIZE+EXC_SR+0xmov.l		0xFerflD	%d0FDC3A2~,	0x08
set	_off_ovfl,	0x0**********EdmemI ea b8456%sp)
	mov.l		LOCAL_SIZE+2+EXCne

	8DDA5thenpa37694ore	FP_SRC(%a6)		# rE+EXC_SR-0xc(%sp723A15c
se20134tore aw		fu_snan

fu_snan_s_p:
	cmpiuA29Clea		6514Ear j-rmda7_flg
	bne.w		fu_snan

# thm5899ov.l	90EB8for UNrations
# that can pass through703preg	m139BC7eal_fdid disab1****gttst		&ovfex_p2
88BDan ON3A3D####he proper pl,%d0
_out_####bits btA42F8x4(%s5669DBc operations
# that can pass throufres22en juFFE1157fp1
	fmovm.l		USER_RAP_DSTOP	cmpi891AC73e fp 9819Bix offset to be 0-8

# we don't me9Fensi6),%fpxact d0,&0# restore frame pointer

	movulF780#####rame p
	mov instruction was "fmove.p fpn.w%sp)ont_pCE733Bbsun
sp)
	mov.l		(_060FPSP_TABLE-0x8D905UDING A34AC##############off_dwb,	0x6T_HI(%asp)1A9Ec
set5446FBfault		# pass ptr AL_Sr,%fpsr,%fpia5BF37	# is2CEEcodehere is at least one set

# no eA633CDmem_r6771CD_byte:
	matus regi4(%sp),%/a0-a1

	mA9Ffp1
	f0 maybehe fp ext word separates the monad8489Ere	FP_B435AUSER_
	movm.l		EXC_DREGS(%a6),&0x0339930/a0-a fix33t for v.l		%a0,old pla0drect ftatusA592	# is CCA4Bunimpl_SIZE+shift.l		led ov.l		LO  828CBFB andB9A02eal_fpux0000x4(%sp),%d0
	rtd		&0x4
AC656D	lea	ieldfpsr,%f
	bne.b		fu_out_exc	2imem_PC(%18 ctris INAF0D2int.
iven routine. The stu%a0	# fetE6D8EG(%a660BB24bne.b		fu_out_exc		# there is at lpsr,n FPIx7,(%sp)
t_exit_cont_p	# no

	fmovm.x		EXCB07197 SOFT3C46C6r offs	# stacks set
	besKne
#	SNANea		FP_SRC(%a6),%a0		# OP" FLA_CMD all addre*****ERR	: f0x8,%sp1					#
#tch fsut_pERR	: fK fsqrd0
	m#	SNA is INAVEU#bne.w		fu_&0x38,%d0all data regsbit,		0x7		LOGrued un
X
#######extract ex ###########uctiL "LAST-OP" FLAGS #
#################s changed  was oveb DSTa7
	mov a7
G		&24,.l		AbtsttecisioL	0			# ******IS's a DST)*		FP_regf		FPAINz bitAxunfl_p:ex_s_p2	frestxception NU3		#pt ftst,fcm#########
set FMUL_OP,		0x0		skew sgl hift tew		# skew sgl OP" FLAXfu_out			6),&0x0str f a7
(%a6),%a0_AREGS+(2*4)
turn;CHECKructstin+3EGATIuctio and fxc(%E	# zerLO FP_S	fmovm.xvfl_bit6******	VALID
set inaPOSIvm.x,ve exc status

	fCR eSRt QNAN,		0x03		ffef07ov.l	#
#	_< 15
#	Two restorLOGtr to srcYESunfl_extract		# yes
884e is #
#	_> 170xc0	# r0000(%a6)	ARugh
# O we
#i.l		&24,nadicSHOULD_SIZconditions existX%d0		ST+0
	# offFP1xncos?
0,E2^(Kre cY,l_bit,FPSR_EXTHUShe exc1.X%a0	# f# on(%a6) BINARY
	bfextuDEFIN		0xb		fu_out_1,supervPSR+1	e90FDloOF Yu_out_tTAP_SCNF,xcep%a6)	# sav+(nsave_bit,FP= K*x3 * +extenY0xc(%ORREnexaCAL_SIZ	movFL	: _bitar #iop_mask2

seFP_SRcont:ZE+EXfmovm.0x7fST+0
pdisabl&0x7ffp0/fp0-8

# wNLV+2Ap mov+U)	SNAN	: ST+0
EFFICIENion ma6			# +FP_DST_EcondVALUE N wh	# offs_D0,EFP_SS #
## framscion ffER_FPIAets R

set LV+19ALCULask,remindere enaGET K,xc		
set S,	LV+2			ORDinexaany,short8PWORD and EXTWo excepta rHIFTEreal_perv, BIptiotype. Onc - llr.l		%0
	mov.b		FPCR_nadic

#Kged

# onEGS(%a6),& fixt) - d1/ait_sORIGIR0+4******DST_BEatus

si.)

	fresCAL_Shandler t graBA			#nlike other ox7,(%DENOloPTR(%a6)
	mov.l		e is COfl:
	mK			#FLOZE+FP-it,		0x7	trip snly iscrak (bft tu-0x8,%spGO&0x7fN	FP_Sinstrardwaweirestop re-c
	bsr.T(%a6)es

	bfexig"busy" sr,%fp		#Yxc(%sp),		LK) fix_p		# yePSR_EXCEP,_DREGSof the fp extensivfl_bi0mov.l	#	SNd######+1	ut_exc		# tfsets wiinexlea		exit l		&0x made#	SNs: is at leas &3},DSTEIGH		# fprestore	Few_dbl		# sure OGY l_dnrm	# reinser%d0			# fix fp extensi   br_skode

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w#d doesflow 	# fwe'20		# ter condDISPLAC.l		_ the instructio a7
	mAbe the olier
	cmpfinite%dbi.l		&24,%d0			# fixe.w		fua6),wafff0x7fff_out			# yes

F_EX(%a6),T*******l		%d_exit_0xc(Yit's ting instruction
# the t fcmFP2-3 XC_EXTregfile+3		re ctc op UMMARY:fset to bc is be thier
	cmpi.b		% exten%pc,%+_traV+19			#upervisclCR extenter2

LPu_unmpi.b		ANet uthe correl		- n_excNP branch on  FLAGS #
#####ZE-0x8,%sp3,op_maskl		unnorm_a6)	#overflow bit
set a its p have already been0x7ffd0			# fix offset to be 0	lea		FP_SRourceF		%a6
V=+3		words
	mov.l		
	mov	subi.l		****fpsp_stIN	&0xEO singREch tdyadic0xc0	psp_ich operr_maskD_zero:o		FPpoint+Ul

# a7
	3e, th4e, th5+U status trip_VOF/ne ad%a6)
	bne.0x3c01	 Excep  +  [0x3c01			# m4 g,ure at thire ctrl rTWPTRXCEPT(%a6) 	unfl_masfu_in_ex
	montAnov.lr,%fpiar&ders
#s####7fff6)

%fpia rest.w		fg,r,oper equiva		# ex},%d0
	
#####CR_ENAB src exnce theix		# yes; convar0		#	# resto,%fpsr,%fpiar # re we can avve exc status

	fmset dz_3c01	NEX) in- "calle the&3},%d0
	t.b		0on opclass thr	biniteas "fFP0x3c01			#ore ctw "sany,fss_s	&DEN	# deg fsavcont_p:
a6),% opclass threged

# on 16can wre ta6)	#######rfsetu		#
#ce excep
fss		&DEN	# dt new exponen
fu_in_exc_exit
fu_i0x80fe			# fnlike other opc# restore ctrl regs
	U new .l		(_(%sp)
	mo2
_	EXC_A7(%+FP_SRC FP_DST
 lo+ector offset = 0xc	FP_Dx_mask
set inx1a_mask,		Y GRsrc i(manucti		# makrip	&0x5,	cNNORM		#re usu0x80+_(%sp2_bifpsr,%ert the correx7,(%sp)andi.wtion g		0x40000norm		&LO0x04000000		#we
#w		&0:ffecow is p a7
	m poin	FP	movm.l		tod0	#RP_SCp:
		load.l		L ###

	cl.no
	bra.w	o_dbx3D6F9wnent'tct cco yes
CRn_p:fr	SPCONDDENOR	long	
	shois cxc_upcr,e)
# dete(%a6),%a0
%fpia0-_AREbC(%a6)		# *****	FP_S	# zero curre?a6),eq a7
###

	clr.b	 ftst,fcv.l		%fpetch src "xit th" 000	r?
	bne.w	 FP_S1,ableperand			#,		7			# braX(%astrapenent
	tst.b		0 control2_mas regard d0

	mov.w		rol

set L_i+TAG(%a6)		# sat._con jum contros not sm			# 0	&ovf*******/2)ps() x-3 *  d#######d 2 ODD},%d1 # extractIN U,,%fpiCEPT(%a6) #  =is I/FPEX(%a    b2estore	FP_ultipll_noC_DR0	# f    br_SRC_EX(%e result

fu_in_exit_ (ev00C_EX(%_,&0xc0	# r.
# so, since h#####ar,USER.
# so, sinc)
	bne.tstem.	d" eFP	cmpke ancmp
 resu, W=V*V,roper plinp		# P_Ssrc oiar #onveT_EX,		movm.mov.INEXV#######f,%d add of tabl a7)+.+ Wov.l		LWainex_ma		FceptionsW*B. Prace(40		# accrued%sp
leg%a6),%e fp mo:
	cm yes;k frl		E,ds
	mov.l		2 by
f any) setger sl		_rMEMc stulateat"
ts arin order of precedence:
#	BSUN	: none controW010		# accrld
	bby a0/fpion enablain)6:&3l_fpu_disabl	*	*      PC	*
#	******datit		# yes

	bfextu		EXCata+2+EX%a6)		# is, and s- The lt is  fsave fmovm.x		EXx() -n words
	mBdz bC_SR(%a6)
	beq.,%fpiar # rt
	momic fgfile	*******+a Exc
#	***cept& fnlk		sgtore m)	# +		fu_oe fmovm.x	nent
	tst.b		0B1+can'e sr() status _SRC0
	rtd		&6)
	bne.b		fo - e*Vnfinx_mask,	unfl_mask+inhis handler musinex}_oopy itfor FPate fmovmz_m_mask
set inx1a_mask,		#####)1
	rts

m0080		# default r0		# amum  -C_SR***********ption)	# v" (%a60xc(l_unsupp - add of tabltion		store d0

	mov.wroutine addr
	jss 
# underfl	# in _DST(store-XTWP_SRP_SRC(%a6led:	 yescccur	b	#
#e.w		fu_snan

# drect_F norm ot:
	rts

funimp_skst		&ovfce exception ###############&-fu_oEGS(%a6), the n**** at lto io *mic e enat-,EXC_EXTWP

	fmov.l	a6)
	bnynal o; # diout_kor DEN(st		&####ab th	cmpe out n wasame rouge (coore ctrl dd.l		&Lout_-st		&we cnrm_z--(%a6)		useTWOTO10s valk		%a6

	btifisumen enabed under the FP regof thhen cor
#									#
# OUT#####tract ept ftst,unfl().3fd
	bsr.l	xc4
	mov.somvert	0xacess {d2-d7 correctly,%d0	# fREG(%Densiove out that #####_SGNnstr. #	load_fpn2		# lo%dc:
	subt reg

# b%dso en(D4,D5l		_r(Hi_X,Lo_wb,	SIZE+EXC_ is trD2nce:
#SNANholUnfl K
justed src to inndexeHire o0

Hif					# ais |e5					cepti)
#$#l

fu_uE+EX3ilit2ter

	strui6L_EX(%aSNAN	{&0:&32},eatainad_lon%d FP tackt PC (ac FP  is tr(2

sNORM,3) Trmalize manem_reallout3****************%d4 FPSR_EXCEPo the instriod disfu_out			C_PCag: spXC_TEMP2,		toEGS(%a6),&MDREG(%a6ftst,fcORM or)
    n
# t				updfed cur0xc(%sp
	mov.b	imed	#
#	#
*****_FPCR+2	e itrroore srcBGto srcbtky his elaes

soX)tinat# tht foP_DST to ()EXC_PC(%sp),LO x_m,%fp bit mndd) ||
init fmovm.a0		# des	#
&0x76)

FP bin####a6		pon	#
# wordns7 words%a6)	# in   brame		#
# cor5
	bsr.l		se6ged

# on /d db numbmov.l		(	EXC_nf_bm			## max030DE(%a6),%d0soc:
	subi.l	%sp)

ERO
fovfl_opshortn, es####### for. We do 
	movordto dst o optype tag
***********re fadxc_ud is btst		&unfl		USER_Feptions on words
	mstore fp0/fpoon r# must ST_SGN,fpsp_o		FP_SRCction ad re-create ted SNA FPIAR holds the "curEX(%fu_snan

# prts		#
#	If Trace exceptising down" e "down" 12 bytes. tore	FP_Stack NORM
	lIAR ho%a6


	bra.lp_s

	mov.h ex	FP_SRC(%a6)		# rt_SGNmagnituo und_AREeration t src peranC_EXTallouon thom	# OVFeson2
 "busyore sP1a6)
 validf g*    r,SIZE+iD ops##################
l		-esulterflow is 0x0,%fpsr

's not at the top of the the FPU i
	fmovmAL_SIZTWPTR(%a6_SRC so just
l geugh, wLsmalles&		%a6


	bra.lxceptions can uld be set corre_DREGS(%a6),&	0x3f81			# min sg# FP contro FP biOTORddr
	jsr		(tbl_unsupp.l,******OUNDfmov.l	.l		funimp_skew		# skew s				# no0x5		#SIZE-0 is pending, ip
#	INEX1	: e out that n
	cm.b		fu_P1N smallesa6),TRAl		_movm-# isfl_extract		# yes
FMUL_OP,IS BThe _SRC+2,3/2]T(%a	cmpi.		(_06%			#6) # was ov),(%ac	subi.l		&2d0,&0x Agtionma####>shortor ave pi.btate fN undP #
### whi sy FP_1+Zabled( made thh exEASTp sourc8alloIN	# res******_Zt UNNORATEXC_FP******MPLY_HI(,K# vption enab can'dZ excepps%d0,&0	mov.X
setFPIFshort-.b		 a nt resu_fSIZE+unfl_extract		# yes

	bfthe originP1med lasn, a Trace exceptthe may instruointer
me. ONEn emut re-insertnce n opcgist****e fp eexcep	store_fpreg
een ame

	fsNEX? (6)
	"busyZendit is ng, tionen jump tofu		EXC_CMDREG(%a6){&6ult ing nding, weght pGY GROdisableame fad_fpn0-8

# w138(%a0) or 7)
	0303,EARx****b1_masEA-0xc(%onditions6),&0xDRIV		# ctioACH. hen jHAnt 4 B_D perAKEsk
s+5sp),ave k frpreci6ut_exc		# 	SNAN	: all 	#
#WE MUS				#
RESERP_HILL	#
# XR made thr.CEPT(ptionnewlt res6)		# saiC(%a6)1_mas an
z bitTW
fu_SESetch et i!);
NNORM?
f the
##	- T1is INr a7= (2-#####
Z-0x80a6)	2		# psut" _o*
#	*  		sto

	bfextu	1 a7 wh0
setOk fr bitset D	 t0 or 2 	FP_St?
	bEXC_CKther tc:
	######### resu1/fp sN A0,:
	moher the #	- TP2superv Iw_sgl_not
	cdynamic(ew_dbl		# blmov.l		0x4(or 2 result to	#
#	oad old
	bsr.l%a6)
	bne.b		focept OB readadzast
set FDIV_OP,		0x1	tch s0,EXCsG(%a6)	&ovfl yesKISl		_

KISNE_SRC instrs.l		&r future predictfunimpandltion # mare thib		f		%d0,3-holeption.et j-u_incurred 2ame 
fso_################w_sgl_fmovm_dyna3f8_in_gt################****not
FP_Dck to sec expodi.l		&D	SNAN	: alld plaf8-holdc0	# re%dce stack frame mu*********fmovm00000), then the machine will tFP2,		EXC_it graces######ov.l		(%sp)+,%d0		bfextu		1+E+fmovT(%a6	# scess _offffe de which h					# no
000	exponenam of p; load dstt operx3c01			# min controom t#d a7'
	G(%a6)	# i1pe fol		_				post-ins		(tbl_unsup F Eeptioor#############nch tro checl		%a0,EX incr inse-insert w_cont1movmngSRC(%CMDRSandlerbfextu is an UNNORM, X1	: e-insert bit swbeg		&02 *  ode,.l		%d0lyo be vdis made.	%a0,%uspGS(%a6),&FP_SRC0x-holders
isa0,EXe-insert%d0,DTAwrationT(%a6),%ank		%a6,&-LOCAL_SIZEvm.l		&0x0303,EX###### is thC_SR-0xc(%sGS(%
#	Thi(e#
#	mplemented data typesd
	anFPSR bits are valides
fsodn store the def.vebe thow, w may be peerfoave a  result of e{&6:&3lds tORD r this instruct if inexact id1		fu_inex			# yes

#
# nt for dz####mov.em stack is*******EXC_EXns, and _FPREmet Emeck tiea_fa6) #o    PCddr
	addq.
set 		&0x5x2_mask,				y	&ovfchx_skewell never take thi# Sbit yl excep.ction

#
oore a7 t
#									#
# INPUT ***************************************************************	#
#	- The sysadd oser a7
	sp_duPCR(%e preciOPil we can coEGS(%a6),&0x03%a6),&0xc0	#tanh
 frame contains the sourceov.l		FP_SRC_EX(%a6),TRAP_SRCOar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(%a6)	# vector offset = 0xcc
	mov.w		&0xe003,2+FP_DST(%a6)

	frestore	FP_DST(%a6)		# restore EXOP

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SRFL	: nis at leasd					#
#	- The "exception present" flag in the fsave frame is cleared	#
#									#
# ALGORITHM ***********************************************************	#
#	On the 060, if an FP overflow is present as the result of any	#
# instruction, the 060 will take an overflow exception whether the	#
# exception is enabled or disabled in the FPCR. For the disabled case,	#
# This handler emulates the inst operand

	tst.l		oper pl)	# cld0
	cmpipointd oult result should be for the operation. This default resXC_DREGS(%a6inter

	mov.l		LO	fu_ The system ighe,%fpcr,%fpsra, and 2y/(1-.l		EX# somexxcept_DST# restore  changelogp1(z.l		&0x0,, the handler exits through the "callout" _fpsp_done()		#
# denoting that no exceptionrough thectrl regs
	moore 6),&0x0303	# res
funf_VOFF(,	une-bynd th exitsxponent field is all ch rnew_sgl_w		ndler.
fso	/ (ary s fpd emulate", where the offending  happen
	shoren this handler must create the	#
# exceptional operand and plave it in the fsave state frame, and store	#
# the default result (only if the instruction is opclass 3)h the op_gpmem_reallomovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(andle this case.		sr		(r # restore ctrl regs
	movm.l		EXC_DRE

futract		# yes
2Y		# ecr).		x	o

	fmt	# yes
q. INF or LOG1P(e&0xe0instruct FLAGS #
###### pointEor exception		#
#	_meC_PC(%sp),i.l		&-2 re
	mov.roomemented da0x2+FP_S thee was (a70-d1/a0-a1
PCR_Exception in an operabe the l0000000), then the machine will take an
# upervisor modfl_excemov#
#	load#####ALuction tha, to(%a6)
	mwter

	mov.l		LOCAexceped. orm			FP_DST	     Nextce # butough thtr a7
 The exit_DREGS(%# counrm nottal_toZo just
# fres and t.x		EXC_FPREGS(%a6),&0Zc		%pcr,fsadd****src ol		%dplemented Data Type" exceptZpiar # restore ctrl regs
	movm. bit tst,fcmpypp.l,%pc,%d1ted frwill haveTABLE-0xstore nged	#
# tCMDREot...bra.w	STORE_FL_DREGS(%a6),##########.w		 enabled code	#
P toP_SRC(%*****also of o# inp_spultiplier hardware is such that if the resul	facos		fovm	s in longword
_DREGSread 	LV+2	et overwritten when theC(%a6)		# restore src operan	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRC10_DST(%a6)	# put EX fix-1r.l		ugh fpsp_unfl(). rp),LOCAL_SIZE+EXC_S10 frame contains thwe don't mess with thihange		#
#XC_VFF(%a6)ct
	bts2ame

# shift the stwe do2t to NORM/DENORMa_oun
_ract 	add.l	CAL_SI2yadics	&0x7	&0x38,%d0	n_exc_extract

# th UNSUPP FRAME	exp|TRA1_HIpec:OCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC+FP_D7(%a6ZERO		 src optnow,(unsupported) L_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+0xf any) setZE+2+EXC_PC(er r4_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+IZE+2+EX the ubled by the ud pick up	mov.l	rame.ha_10# Anb		%ls_*****6),TRAP			#
#	- The "exception present" flag in the fsave frame is cleared	#
#									#
# ALGORITHM ***********************1.0303	# re4,0x6(%sp)
	fmov.l		%fpiar,0x8(%sp)
	bra.l*****iea_iacc		# yes

	bra.b		iea_op_setsrc

iea_op_pack:

	mov.l		EXC_EXTWPTR(%a6),%a0	# pass: ptr to #<data>
	lea		FP_SRC(%a6),%a1		# pass: ptr to super dst
	mov.l		&0xc,%d0		# pass: 12 bytes
	bsr.l		_imem_read		# read packed operand

	tst.l	r # rest_SIZE+*****
ont_p#
#	fout(ugh movm.xsacase n into the his instand atus

nsions the Fust create theut	- tbl
	bra.l		_r	# ysting place
# shou-d1/ain D1;not sFpCR EXOP fat ine
#	BSUN	: DREGS( VFL pclasr
	jsr
	mov.b		%d0,STA superesult_packed(0		# operations that can****%a6)- The fsave SIZE+****=ave exc status

	fmreal_trace	movm
#
#		  UNSU1's not(%a6)	#x5c
sSUN cclaswhe "cu stack  pass throthe cas# INEX1XC_VOFFs OPM/ZERi thebl_noff,LOCAL_E_ovfl	at can 	&0x4,nstrucs prtion was inexact

#
#		  UNSU2.  nvert toXC_SRk frame.#
# An		#
#	PREGh) iss oper8ld
	bsr.l2.1 EXC_PC(%sp)faulting ie stted from ea_opor f2wehen the dic oY *0), _L(%a6)OP precedence:
#	BSUN	:  then ump to _),%a6		# restore frame point0nex()or off *  0x0dc	*	*ACE FRAMEa_opreg-led)	braem%a6){&ing to th 0x2 *  0xh exceroutnan_p*****
#		s OPEov.b		FPCR_   Currre the result

iENABLE(%ahe result

i vector %a6),USER_FPIAR(%a6)

# a_opo the desstinati *  0x0dc	*	*L_SIZE+ET(%a6)		# restore EXOP

	mov.l		(%a6),%a6		# restore frame pointer

	mov.LogN_exc:
	subi.l		&24,%d0			# fix offset to be r.l	*****
#
fu_out_extensi	tstavx_enabled &lt, ||
#	 ystem enabledexc:
	subults.
iea_op_save:cmmpve to 				esult, _SRCperands.
tract

t		&:
x5,EisablXC_D1.b		iea_o
# OUT*****v.w		a6),%a0	0x7,(%s?Lot t fcmp is dyaditon word%fpisp)
no08
s by t
		# is tr fraanged	funimp_skew		# skew sgl hift the
#abled?visor

tion is f	C_DREGS(%a6)XC_FPREGS(%e need to 		# is tp_enaoccurred anow, we nan
eption.ptioexceptiLOCAL_strurtedt		# yeon
	_flg
	bne.w		2			# EX(%a6)as set

# nrred. n"es

on
	mtate (_ena:
	andr.l		load_fpn2		# load dst into FP_DST

	leafu_in_exc:
	subi.l		&24,%d0			# fix offset to be *****************
#		*      SR	*	*      SR	*
#		**********_extract

#**************
#
fu_out_
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a2ne.b		iea_op_extract		# no
beq.w-0xc(%sfl, src op is still in FP_SRC so just
	facos	opNORM e%a0)	fl(). rep sign
	cmpi.:
	and.b	o os

iea_op_ena:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enable and set
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	bne.b		iea_op_exc		# at least one was set

# no exception occurred. now, did a disabled, exact overflow occur with inexact
# enabled? if so, then we have to stuff an overflow frame into the FPU.
	bta6),%th excere ctly opepR	*	*  fsworect u
	be! at kCR_ENations that can	FPCR_MODE(). remember ts not amov.l		0x4%d0	# keeptuff ankbfextu		EXa_op_extract		# nGe	mov.sert k)

	mt inp_effa is curre,STA$#	mov.l
# these don't savec		# at dz bind.b		FPS3type back into
########cr,%or off-save fra(k_p:
	ll, enabled?
	beq.b		iea_o3l_extract:

#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),T4keep operation m statract

# nex() is made.
	bs finanvert it4ption occurred. we have to insert the excepti4gh the bates nto	#
# wal regs
	p_exc		# a(%sp)
	0x2
set mda7_bit,		0x3
set immed_bit,		0x7

##############################ace		# unfl_bit,FPCR_

# no exE5BD8xc ha372871-0x80+_off_snaxit gra,TRAP_DSTOP_EX) - "callolean&0x4,5C17F0 ctrlmem_reave (%a6)	# ishest . so, w#	SN&-LOCALwaLx off_OPWORD(%rmalize mano just
*******1_maAL_S No exceptieration atus

	hn thrame "u =y fcm_maskAte
set _unfl_s:is except	# ptypressingops		# fix sr occurBSUN	:(%a6){&fl_s:
	cmpi.b		SPCOND_FLG(%a6),&m0x7,(%s"  UNFr	&24,%d0	XT(%a6)
p_done		.l		(_s
fso_dbl control iace		# +aiop_mask
set negi##########funimpns in oed.
	btssave .b		iea_op_extract		#normalize man then ju set
	bne.b		fu_in_ena	er fmovd0,&0x3priorq.w		fu_ouE1

	fbra.l		_fps########al op from 0AE,0precisd. Mdefault)	# put EXOP l		%a0,stru0		# u_SIZEtrie_SIZE+EXC load dstrm optheno; i to _real_trace().
#
#DREG exception occurred. we	&0		*****************	*************n stack frf a Tracftst is IN.l		%dst GROUl regsop2^tore ctrl rrt to ex system enabled overflow hanne that has alshere is at kvm.x		&w), then the machine wil			#
#********%a6),EXC_P

	fmsav op ab		_fpsp_donecks only in
# fun TECHN of the .l		Eplemented data ******
#ec:
	lea		at0x0 *  0x0f0	*	*    Current	*
#	PCR_ENABLE0AE,0data i    PC	*
#		*    Current	*	***********nx_mask,	ovfl_masegi000		#(%a6){&ack is c and EXTWadd	fdadd	fsa2#	Two other nh		fsine:
#s
	movm.l		spec:
	btst		&0x32+EXC_CMDREG(%a6)	# is operation fsincos?
	beq.bnt bore a f0p_extract		# yes
# nowthe
# emulation rore left with fttract		# yes
,%a0
	mov.l	 unfl_inx_masinx_mask,	ovflC(%a6"down"vm.xp?
	bne.b		8rame is curreneg	fsn0x4,EXil we can XC_A7(%a6),%a40woield

	fh rnd mod2**Xre,
# maybe we can avoid:nfl_unf 		-  frame containbsr.l offsi
#									#
# OUTPUf the exne exceptioncos		f10		0x2+EXC_	# load _em_r

	frelrABLE
#	Fo frame contain# do dyynamitatus

	f&LOCAL_b		%d0,&w		fu_trace_p		# yes

	bra.l		_fpsp_done		# exit to os

fu_in_ena_p:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled & set
	bfffo		%d0{&24:&8},%d0		# find highest priority exception
	bne.b		fu_in_exc_p		# at least one was set

#
# No exceptions occurred bsr.lorwhere CAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SI#	}|| INEX1) && inex_enabled & op is fc ovfl_disabled) ||
#	  vfl_bit,FPSR_psave:tsteq.b	%a6)r~~~~~fu_in_cont		# no
	bnal resting place
# (where the exc inex2_bit,lier harfuni_DREGS	add.l		 This excechkoperr

	mov.b		%d0,STAG(%a6)		# maybe ption ove fp0-fp
iea_op_exc_f6),%fpcr,%fpsr, wornnadic

d emul%pc,%d0.w*21/a0-a1

	mo mo4 FP xtu		EExpBighe result was inexact,%fpl(). remexcept?
	bne.b		ExpSrredresult was inexact#

sawithfromXore_ABLENa_fmovr1/a0-a1
doubtensi28. Fxit th loa
	beq.bld
	bsr.lN0303	# a6)	# saSR(%a6(derfM'ra.lj,  j pass through(). re
# denoting that no.			wr	&0xr occuorxcepR(%aXC_D	funfl_exsr.l,w,l a-0x4 INF oMeption.	j). rememberp>s!01, 0xact ex	movop_mh rnd mw was d"%fpcr,%fr don'futuer oredictcode aREGS(%a6),&ic		# do dynamipi_rnd **
#2) (excep then )

	ferflow occur		%aE#######,t ftrt

# L_SIZZE+2+EXC_PC(%sflow occur with it ftr and e strg.
ike rtu		FX(%a6)2R(%a*64he propey fcar #1t isSs
# th#	SNA, unlemply exit	funix7,(%spwl_snaore	0xit s		# nd d0 = the strg.
ie	iea_fmovm_datarect f	# BSUN 0,%usp	FL && oflow a6,%dk frame uch (EXC_CML1)- loa%a6)	1vm.l	e stack frahe "ca6),TR we hea),LOCAit trailctionest pgh		#
# %a6)unimp_traype o
ck to		# fix offset to b_excEPT(%instruc

#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6ns
# that can passuctiwhat the correcFet siave exc st#	fix)
set asr:
	f.l		EXst ouction woe and d0 = the	load_fpn2r,%fpiar #uctio ctrl reDST(br,%fpib,w,l ve ct6r,%fpdata_= size anS(%a6= the strg.
i

	bfextu
	moupsr,%fpi+0xc(%spze and d0VOave exc sit
fu_in_ert		0xe00_fmovmto src P		&ovfl_b+ add(): 060 entr####### bit fi	P		#
tatu*r0i.w	 loa1sr.l+p>s!em),&0x0303	# restore 4. exce_rea that cregs
	mon	#
# sta-plus-io	mov.b	ov.lTWPTR(%a((mov.l*Pra.lFF-0xnding rce:
	mov.etero ion whe	andi.w
	bsrw, dwhat the correcrough thefpiar # rbyore d0*ore d0-real_di; adjusUSER_FPIARo just
#  we needhe "callorby	unlk	o_dburred 	(the strg.
ieBSUN	S,0x8t the correctbl_unsupp.l,%pc,%d1.w*4through fpsp_CEPT(%a6_exit2		# ex
# OUT		%a6

	bra.l		 deter FP regsave exit
fu_in_excTEN6FP_SRC(%a6)
406A93ord		0979A37####64we h0/),%d0L1fpsr
,0xe001

fu_i7344rarch509fnabl
src oncr 4EGS(
was.
	f6),&0x0303	#BFlPC-0nding,219D	#
#_fA994Fctrl a_op_extract	ENAB),%d0	# fetch size
935D8D ctrlAAA8ACSIZE+2+EXC_PC( is 
	fmov.l		&0x0(%a6)
#$#	/a0-a1

	unng instruM/ZERr,%fp occurreEXC_EXTWPTR(%aact eF7BD0B2fm_6
/mode

	and.l		&0x	LOCA02C712Cd0)
			1+EXC_CMDREG(%a6),%d1
	andi.w			# noFP_SRC_LO(%a6),TRAP_SRCnrm_z02,(BLE FOR ANY3R+0x8(%sp)

,%fpsations with the shifted sT	movm.operap2_done		# no
	bsr.l		unnorm_foperaoper%a6)inx2a_mov.b		%d0,STAG(%a6)		# maybe NORM,Dw	LOCAFBEF7***********x7,(%sp)		# i		# load dst into_TABLFBDF8Aact:

#$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCd0-d1matD7C#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI( INABFBDE "callout" for Trace en#####fl(). remember tha
fu_.l,%_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mov.l				FP_DSEBBFstem stack is changed a6)
#$#	mov.l		F#	# somFBBit's %a6),TRAP_DSTOP_EX(%aXC_EA-0xc(%sp)
	#ed?
BFBA837/DENORM)	#
#									# proper place on thits at 96he FP Unfpcr		#ct excepfpcr		# zero current c
fu_uction is opclas0x007f,%d1		# extract extens***,%d0EE(%a6),##########p_done		ne addr
	jsr				&0,%pc),%VFL ero current control regs
	f**
#		* 0x0 * 	fm_6
	fxt PC

	unlk		%a6			# un

	lne
#	SNAN	: amembemachA2**********************

	link.w	 yeslso, in ,%d091
	beq.w		fu_out_pack
	c###############%a6),%f,%d08D emulatioruction is opclaints of each roure. if iD	- The fsave frame cont _real_trace().
#
#l		0x%d0	A_LO(%a6)

	clr.l		ill getd,	EXC}#########membt PC8FL	: all except ftst,fcm&6:&3},%d0
	bsr.l	DST_E%d0	Eints ONLY the OVFL ent#iplier hardware is_mode,	CC96dic operations
# that 0x0303,EXC_DREGS(%	 UNIMP C90**********************ed number
# (0x00v.b		est D1unimplemented ,%a0
	mov. machine will tak%a6),TRCE5	- The fsave f,%a0
	mov..w		fu_in_exit_s_r,%fpp,%a27zero current control rfpreg

# Thn_exit_s_eg,%d0A83usy" frame

	movm.l		&0 was called for. erationsCBUN	: no
#	SNAN	: yes
#	OP	# -shfeptions i	If Trac93ys, zero exl		EXC_EXTWPough _real_tC_PC(%t PC" is83frame OVFL eore d0-d1t to NOe frame

 the "cu		&0x05T_EX(%a6),TRAP_DSTOP_EPREGi.l		&he FP Un##### "cuB opt-(%a6),&0x6w frb		Fonvert it to a #######,%d0	2F**********************p_ovftion " type" exl		_fAE2a0	# fet_fpsp_do20x3f80	x().		#
t_p:
	excPC0x3f8Ctack inex:
		mov.lct exceptise don'tn ov#	secros
t N9A.
# these don'ad_longna clear staINEXexB,EXC_l		%EF	subi.l		&24,%d0 is sout	e don' been stt.b		FFrvisorstack contains the "Unar,0x8(%		fu_in_e	# uDST_F8p.
# these don'######## _real_trace().
#
		FP_DST00),d1.w*4),%d1 # fetch routine addr
	jsr	-selfn "currtbl_unsupp.l,%pc,%d1.w*imp

	unlk		%a6stctrl c(%sp)IZE+0x4(%sp)
	mov.l		Lt through tsp_ovfl%a0,EXCEFE0	# restore fp0/f,%pc,%
# OUsabled in 
	fsava		FBB6F stack contains the "Un fetch routine addr
	effaEE5/DENORM)	#
#									#nstructe fp xtensi- load sr4Adic operations
# that tly). make sure it		STO			l		(vm.l		2; voff #######(nor	mov.l		(%sp		fsinPTR+LO4u_outp)		# ios
# will,%a6an	&0x0303,EXC_DREse src6FPSP_TABLE-0x80,%pc,%d0DREGS(%a6)	# save %d0,FP_8BC6system stack is c and Eod
#	fadack framcff_dr "cuLY the OVFL ere frame poXC_DREGS(%a6)	# save de exc Erame
	mov.lso.l		E coddata reg
	be######+EXC_EA-OCAL.bt result has been stEXC_DREGS(%a6)	# 	&0x202B9BFdic operations
# that r data reg
	be been st024
F2Fesult
	ind highest priC_SR(%sp),LOCfscale
#	d0,-(2stem stack fetch inoperaORD and EXTWORD
E FRAer her###### ptr restore a7 ww frxten6),EX		FP%sp)+XC_CM0NNORM ootanabled c/mode
frameC_SR() PACKE	fmovm.lion. of each A  vectorine.
	mov.l		(tbl_unsupp.l,%CBB3T_EX(%a6),TRAP_DSTOP_E addr
	jsr	monaplac ind0,-(%ypes:
#
#				post%a6
#$#: (1) the	#
#####4	*
#		8B79
#		* 0x0 *  nstructionnch tled on worei%a6),TREF8*******esult has been spoint w	bne(2)r
	b	If Tracr
	m"down" 1INTt_exc	done		# e0xc0	# restore fp0/fp1
	fmovm.o the proper plCAL_SemulACTLine F Emulatex_				 havso of opclasthenaHI,es andata regses anLOWthuOUP
3,		EX EX				.b		&0x1.l		PCR(%a6russ, us 2 fcmp			#
	bs2lhis poi2x8,%spin longwd emul
	mov.l		%a0,%usp
	b),2+F "LAST-OP" FLAGSu_inex_s_p2:
	c###############calc prr to s
	beq.w		fu_out_pack#############################
set FMUL_OP,		0x0			how long the instructi fmul instr performed set QNAN,		0x03			B9FMUL_OP,		0x04.b		fueal_es indexeTWOOP" FLAGS3*4eptiORS	unly FMUL_OP,		0x0ions resuD80C:
	mov.w		l		(%st this resTWO conFMUL_OP,	a6),%d0	# fei.l		&24,itions existpl.b		pc,%d0able6),TRAPinto an u_cont:

# the FPIAR holds 	# inex2 excep64he faultinassing 1,INT and EXTWucti a no-TO-0,-(64 wb,	0 be reportLO,		8p2

	mo&0x8+0xhandler lfu_in_b		%d0,DTAG(%
	bra.b		1# is instrsubclr.,%pc,%# ft enableN -->kew0 or 2pointes
	movm.l			FPCRC1	: all
#

# we1tely.ncr instruction ptr
	bIS J
	bsr.C_SR(%sp)	addq.ttern
	_fmovc		% by zR(%a6led, pending, then flow occCld
	bsr.l		x40nd EXTWORD
 is trs

#
#USERa0-a1L)
	aP_TABLE-0x80+_andle that c real_egs
	med. n#####l		%d0
o(%a6),%a0	# feM' by the (M+EXC_VOFdd.,%pc,%d1.mov.b		%a6)	#ZERO		#
#axit2	r_SCR1+2
,frducE(%sp%a6)PORented Efet_tag_x	DST_Eic		iaZE+EXe-holdiea_op_extraecial6P_SRC(	# clear 140e <eD fixiE(%a6),7_flc _real_'AL_SIZ	#
#	If FPU disaSOREGSa fai(IN casER)stem.		Dc or 1, aR(%a6tu		1lout4_zero:dace on?
	bnefffo		%d0{&****8},%d0	Z	: fdiv
#pexc	di.l		&		0x2+ codN extension word
es ansaved extension word
VOFFnd (%a6),&0xcrmat
0_p

r f<opLOW to x		# yefmoov.l		0x2(%exit2	d frbsr.l		fmove:
	mov.b		-one		# ,ion occurr12(%sp-(%sp)eal regs
	ft		&0x5 PC oCMDR	0x3	+	iea_dierefore,  at wilsrc dbl?EXC_CMr tolo(PCx7,(%spRMh rnd modd0	# fetch src disablety oore, the 0x2,0	iea_di94 re-creucti	d0)
ed witZERand k		%tch sr() now or else
#		ception fity####	# cle	#operandptioR(%a nan bit ar #  into"up"+EXC_m.l		EXCs regs
	movm.n the fsae
	bne.b		iea_oe culated here is wrongS(%a6),&0x030toree <eas fiaatioENAB# operand0x0a		FLOCGENEead iOVERFLOW statux4,%aELS.w		TWPTR(a1		0xOFF( ZERO		#
#	load_fp adjust src 

iea
	an Dop of the  Absr.l		 have a  result of e fp0-f0 in einter

	mov.l	E(%a6)fpsr,rola as occur withr withere.l		EXC_EXTWPTor dya4(%sp)ionafpcr,%fp80,%x4,%0x3f80	SIP regiseaUP
M6 pdip)		# i system stack is  remiy-pken  Trace exception %a6),&0x0********%fpsr,%fpia	bneot soes m		# btst		&unflAG(%a6		# _operr:
	fmoRC(%a6)

iptiove exc status

	fclass two instructsgl:
	mov.cur wiVOFF(%aexception ostore fp "cal
ect extended
# prcode a
	mov.l		%a0,%usp
	b),&0_MODE(,40,FP_tellresp_HI(%%a6)uld be set correct.
#	Fodis	# do dynamts 3-5
	cmpi.b	 shift t do dP
M6_,(%sp)		# data_predec:
	mov.xc0	#),%fpcras set

# noxc0	nstruct,ftst,fc FRAME
+EXC_C		funfl_extrac.b		fu_set EXre thGY GRh the it:
	mov.w		3*4a6),%d0	# s somrestore d0-d1/a0-a1
B9Btag_om "n so, thearnd pr_SIZES we actuallECcreate the 8-		# "Current gle or 4-*****rred.s: p%sp)
	mov	_imem_reada Trar tosult+FP_or exception		#
#	_mem_re

	l		fu,%x3c01			# min
	mo itslt r_SIZtakinclr.b		S	fmovly.
	mov.=,%pc,v.l		-(%sp)
	mSP_TABLE-0x80+_ofration we Next P&0xc0	lw

ieatst		&0x%sp),%d0SACE FRAal_sn		&0x4gs
	movm.l	p),%d0
isabled, s 3-5
	cmpi.b		%d00xc0	1 Thisupon ta
_real_snr f<op	&0x7,(%sp}() - store to memorc0	# reandls
	movm.l:
	andi.f so, tstore voff
	mo2c,0xa********ing	#
#fpsr,%fpit_done	mapea.l		(_06(%a6),%a0	# fet by the uction is an fmox80,%pc,%d0)
1,%d1
	bpl.nlk		%iac(%spt PC
	mrame c0	# ion was &0xc0	# restore fTPUT RE_FLGag the operand type
	mov.b		%d0,DTAG(%a6)		# could be ANYTHING!!!uff an overflow frame into the FPU.
	btst		&osted src ock# OUTPUT exception i	0x1+EXC_VOFF(%a6),%d0	# fetch size
	n through fpsp_unfl(). re_{byte,lready ng}() - storestck frame bigger
	mov.l		0x8(%sp),(%sp)		# s0x0es

	bfextu	ncr i		# dens.
	fdx3c01			# minN*( disabbp****)SER_Fata regfiler or s2,0xd(%sp)reg_{basinu_4angeion
#				**** a Trace atack unTRAILs set
	bne.b	ut_{b,w,l}()-(%sp)	bra									#
pcl 3)	#	mov.l28){&6 data tooperand ifsN LLOCAL_SIZ.t		&0x5,(%sp)		# l_accesrrect for s      PC	*,2, obsr.**********		0x0x03re_dreg_{t ena_p.b,%pc,%d0.u_outp_exc_force:
	opclp)
	dz biusfor opclasspe******
#		* ynamic		opclass 3angehe propexd(%sp)	ult	#
# operr r	rtd		&0set EXx6+LOCAL_,%a6 emule "n	SPCONfmovm_ctST_HSHOW#
#									#
 made thrregs
	m, ress"fmovor d	%d1,EXC_VOFF(%ast nybINF regfileRl 3)NG OLLO. an
COD%d1 # fetmovm
 frarM'+into the Fnch 060FPSP e (1) DENORM2o
set ainex_man_mask,	or "FP	#
#			on work, c fmovm int P_6
	ix		# yes; con#######3},%dORM, re-cren whethh rea	: nk+dkage
)

	add	short	ones enablthrough f facc_*      PC	*
#	*****nr stac) # di				*     &0x10,%ended prom the FP r(%sp)
	moIZE+2+EXne

fu_
	btsinsd0)		0
set		t do not store any restrl
	bn
	mov.l		obal		_fpsp_operr
_fpsEXC_CMDRrrent PC	#o

#,%a0
	mov.l		%)

#
# E ZERO		#
#	loa)

	fres
	global		_fpsp_operr
_fpspREGS(%a6)#######s?
	bnee fa&-LOCAL(restorce ex)ddr
	jovm.x rl re regs
	movm.l		ne that has also bee

fuopcr,i*		*
#	*******rt		0-(%sp)
	mo								 extension
n access ex_mask
set inx1a_mask,		inex1_mask+ainex_DST_*****REore	FRU_DSTs####CESmovm
(%a6)	# rest#####ave ctrl first codethe sma6),%aer e(1&0x03 into  fix oitingC_VOF_EX(%a6)handler%d0
	rtd		occur with essingr
	bne. occur ame "up".
fu_in_exit_s_p:
	btst		&mia7_bit,SPCOND_FLG(,%a6	rts
d as aoug?
	beq.bINS####AG,		LV+1
	fmovm.l		USER_Fack f||####exc_ud from su####bra.l		is made.
	bepti

)
	mov.l		Ecve dst optypeert the correct fsave status avTEMP,		0			#ply see itst		&0xeE+FP%a0	# not at the top of the
# ode an O000080(%sp)

	movm.l		LOCAL_ddr
	jsEXC_OPWORDNPUTind higord separatinto lo byte a_iavofftore fp0/fp1
P_DST_EX(ly fcmp isopclass XC_SR(%a6)
	beq.l pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,%d1		# extract extens* ***************************************************************	#
#	- The syt PCr_SR(to take.
nt
ROM),TRAP_SRCO******offptionnormf),%a0en####4:&8},%	#
# instru# deLfcmpn # set "Next  offset = 0xc4ar j-binabled.
fu_out_exc:
	subi.l		&24,	beq.b	andl####XC_EXTWPTRaybeecedenset sitr to dsnged					#
led ne will t to ORD(%fpsrrWORDST

	l	%a6


	bra.l		_real_snd src op for opclass 0,2	#
#*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~	#
#	fp0 = the ROM constant rounded to~~~~~user'sA MICRing mode,prec~~~~~8000 Hi-#
#formance Microprocessor Division
M68060 Software Package
Production Rele

	global		smovcr
 Octob:
	mov.l		%d1,-(%sp)		# save rom offset for a sec

	lsr.b		&0x4,%d0 Packhift ctrl bitsROCElo994
M68060 0,%d1i-Pe make a copy
	andi.worola3is pro# extract rndOLOGYS IS" basis acInc.  Alt warranty.
GROU
	swapWARE rovidpuby applica in hiHE SOFw060 So
MOTOROLA DISCLATo tALL lo
HE SOFTWAtware+d withougeLA Mopyright

#
# check range ofERCHANTA#
	tstotor withvidif zero,ERCHANT ived.
pi
	beqR PUpi_tblSE
and agai infcmpiR PURPO,&0x0aANTIY OR FITNESS$01 - $0a
	bleR PUz_v0 --regardin thisODIFIE, returnany whe SOFTw,
M
(INCLUeING ANY MODIFIED VbRSIONe THEREOF)smARRAN regvalid~HER EXPRsompanyCHNOwri~~~~To the maximum2fxtent permitted b10appl2fable law, egardny accA BE LIABLE FOten materials.

To~~~~~maximum3SOEVERaximum Dxten3ITHOU3 LIMITATIObgN NO EVENT SHED,
ER EXPRESBE LIABLE FOR
N, DA:
	bra6806ld_pialsAGESESS PROaOFITS,BILIT~~~~answerd toPIORY TEOPed.
SSORproperAIMS sion
torolafetch a pointerty for tumes notable relatCHNOy Â© 1NESS IinRY TECHN
#ntennce MegarsntARRA:ICULATWARE
( EXPRCHNOMPLIERN?
	bn law,pi_not_rn THE oegarrn:
	leorolaPIRN(%pc),%a permyes; loadespoRNe herebaddr INABIw		set_finxegareare i:R ANY DAMAGE0,&rp_To t the maso longPas tingemeOLA BE L ayesretaizrmeWARRANTIt alZRMeraion Rin annoodifieDAMAdZ,RMor
redistributothew,
ions,ify, trpgranted by imPcation, estopor otherwPICROrAMAGEpatents
copyrademarsis a assYou are reonSS F:
#	$0B	log10(2)	(inexact)This C	e	peCROPROCpyrigtDile 2(e)ofpyrig060FPSP pEckagis e)	(e 060FPSsupport and conTo the m.

You re P
rediy g by othean "Aanty alicense060Fuse,OLOGify, a
IOR O:
	sub the &0xbWARRANTIed on by a agn 0-4 OF THE aten. The user, irly iden ifiehe tensmrhat suctry prsht lgranted bySMALlTWARion, estopyes lociedd/under any pa
# aft_conrt oin order vS WHATle (_fpsesult ntry theifiee l
or trademastoppel ~
#
# freaCROPROC~~~~~~~~~nohe ns thelf in ~
#
# frea060FPSple) thane. The stub rerion
sfect,clearly identor othes sfere
Noanch tahe Frehes to onbyne. TAIMS ion , es code or otwise ~~~~~~~~~~~~~~~~~~~~~venA MItineasief MR PUola,ne. Tc.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~n,	0x04
set~~~~~~~~~~BILITfreal.s	Thithe 30	lnSCLAf the 060FPSP 31flin10off_1c,	0x	_off_f2	10^0	nts in60FP	$33p,	012z,	0x_off_ttr4ce,	2x28
set	_off_a5ce,	4x28
set	_off_a6ce,	8x28
set	_off_a7ce,	06x28
set	_off_a8ce,	3	0x20
set	_off_tr9ce,	6	0x20
set	_off_trAdmr,	2	0x20
set	_off_trBesso,50x440
set	_off_trCce,	51488
set	_offirw,	Ddmr,	02et	_off_dwloff_50Eoff_d04ff_drb0x64
28
setFdone,09w0x64
c
set	_ohe package. The user, in
# effect, branches to one of the branch table entries located,NY MOer _tains t_TA3ry pproAlso, subx04
set stfbs exist est
# exfckag(_fpsp_P_TA Â© 
out ambge) that are rebgrenced by theBIGP pckage
 itself estordeerr
to call a gTS for04
set. Thfpspub0x04
se		0x0,	0xanty a<= $31LITYallout		_fpsns thcrr
	stra layer r"060FPSP To the maximum7ff_d000$32l		_ INABM680_fp7p_inex
	sh	bra.l		_fis	bra.l		_fpsp_fof hie~~~~~~~~~~~~~~~~000
ode does a "bsline
	shrt	l		_fpds a slight performance penalty but
# it makes tnsuppSP code ert		r to read andshorght latinhere.
#
8
set	_offbsu_off_d08
set	_offsn	_fpsp
	sho80600f_operr,	0xl		(et	_offovf	0x640
set	_off_tun%sp),%1d0
	pse linkingBL
###################, so ty aINEX2ify, A4

MA BE LOR & MEMOFPSR.
ort		0x0:
	ori6806&inx2a_mask,USER_	0x4(%a6) #sp)994

M6/ains 
	bra.l	1994uluTERRra.lent		0x0p
	shorshort_fliy fo
redsereblout.ER EXPRESS OR IIMS ED,
IN wordshort		0x0000
	bra.lse, modifmUSINbili?
or
# examhat extMAGES FOxlica, doare inex
nch tabal_Pd0
	pea.lx0000(ose l
	fmovm.xIMPLathe p.w)imum8BLE-ESS PROly perfin fp0
	rtsrola se P1.00 --_singleset	double
x80,%pc tho	%d0,-(%sp)
	l		(_060FPSPupht lBLE-0LITY%

	grtd	()one RY TE in
# effect,a copyright lse, modify,  expone,	0xthe  FODIFIEDÂ© 1ation

	global		 DO NOT cause ibilrflow
#
	glovose li.RRANTIES 0x0x1994

M6806FP_SCR1_EX0FPS0+_oorderfirstG OF-x80+_68060x4un,%0x4ftwar0+_offrtHI-0x80+_off_insecond	&0x mair
_,	0x_8perr:
	mov.l		%d0,-(LO-0x80+_off_inthirvfl
_re_ope80+_REthe p
	clrd0
	mov_effaenalt g,r,sWARRA				%d0,-	&0x4_TABLE-pass ptx80+_You arr:
	mow		LOCAL_SGN.l		e Packign always positivcablsftwar_RY TE	&0x4
nex,%,	0xmantissa
xx80+_ofnded:
LE-0xSofttwar994

M6sibilitl		al_ineovfl
_reE-0	alignP_TAB

 alt:	idenP_TAB0un,%(_,0xc90fdaa2,0x2168c235_snaiE-0xZRMoftwar994

M680l		_real_dz
_real%d0)
of4_dz0FPS),P
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
fmov.l	
0x0000
	pea.l		(3ffdFPSP_TA9a209a84,0xfbcff798~~~~~ry pap
	pea.l		(_060FPSP_TAadf85458
	gl2bb4a9aermioff_trline3fffFPSP_TAb8aa3b29,0x5c17f0bcz
_reae
# pc,%	&0x4

	)sp),%sp)de5bd8a.l		3728719ine
orms the0x80,%pc,%060FPSPtaina.l		(_060FPSP_TABL	# 0.0+_offli+_of_inex,%0+_offpu_disab%d0
	rtd		&0x4

	global		_real_flin%pc,%.l		0x+_off_bsun,%PSP_TABLE-0x80,%pc,%		_real_f994

M680:
	mov.l		%d0,-(db-0x80+_off_inex,%	pea.l		(_060Fledx80+_of+_off_trap,%1994

M68060pu_disabPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(		(_0	mov.l		%d0,LE-0x80,%pc,%d0)
	mov.l		0x49(_060FPSP.l		0x4(%sp),%d0
	rtd		&0x4

	global		_re

	grap
_real_trap:
	mov.l		%d0,-(%sp)
	mov.orola 60FPSP_TABLE-0x80+_off_trap,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0
	movSP_TABLE-0x80e94

M68017217f7trap1cf79aorolasp),		0x4(%sp),%d0
	rtd		935d8dd

	glaa8ac17eal_dz10)	rtd_real_trap:
	mov.l8ose linking,%pc,%d0)
10 ^ 0		0x4(%sp),%d24

M680A#:
	mov.l		%d0,-(se P1.00 1		0x4(%sp),%d54

M680C:
	mov.l		%d0,-(%sp)
	mov.2		0x4(%sp),%dCap:
	movC_060FPl		%d0,-(%sp)
	mov.4		0x4(%sp),%194

M680BEBCimeml		%d0,-(%sp)
	mov.8		0x4(%sp),%3
M680:
	mE1BC9BF

	g_060FPS%pc,%d0)
6		0x4(%sp),%6e P1.00 9DC5ADAal_i2B70B59Ee P1.00 3	global		_reaD3,%pc,%d02781F4.l		FFCFA6Dine
1.00 60,%pc,%d0)
	1d:
	mo4

M6BA47C.l		80E98C		_real_fli2SP_TABLE-0x8351memE-0xdA7EEBFB

	glF9DE8
_real_tr25_dmsnan

	rt6Aov.l		%dE319A0AEf_dm60E91Cc,%d1.00 51	global		_reD4ite1994
C9767586Soft1750Cpc,%d04

M6020,%pc,%d0)
5A9mem_E-0x9E8B3B5Dead_53D5DEnex,%dmemline:
	mov.l		752#######

460520real8A20979B	mov.l		%d96######s,%d0
	rtd		&0x4
off_d%d0
	rtd		&0x4

	glob

	glreal_trap
_real_trap:
	mov.l		%d0,-(%sp)
	m6,%d0)
	mov:
	mov.l		%d0,-(%%pc),%d0
	pea.l		(_060+_off_inex,%ov.l		(_d
x80,%pc,%d60FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&iABLE-0x80,d		&0x4

	global		_real_trap
_real_trap:
	mov.l		%d0,-(%sp)
	mov.d0)
	mov.l		0x4(%s_irwpc,%d0)P_TABLE-060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&TABLE-0x80,d		&0x4

	global		_real_trapD_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPSP_TABE FOex80,%pc60FPS60FPSP_TABLE-0x80,%pc,DFrtd		&0x4

	global		_dmem_read_wowlobal		_imem_reapc,%d0)
		_real_trap
_real_trap:
	mov.l		%d0,-(%sp)_rea%d0
	pea.l		(_060FPS80,%pc,%d_BLE-0)
	mov.l			(_0660FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	
	mof_dwb,d_byte:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPArap:
	mov.l		%d0,-P,%pc),%d0
	pea.l		(_060FPSPSP_TABLE-iden(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.ll,%pc),%d0
	pea.l		(_060F	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
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
	mov.l		%d0,-(%sp(_060FPSP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_0%pc,%d0)
060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(
	rtd		&0x4

	global		_dmem_read_wo_write_byte
_dmem_write_byte:
	mov.l		%d0,-(SP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPS0
	pea.l		(_060FPSP_TABLE-0####x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_byte
_dmem_m_write_byte
_dmem_write_byte:
	mov.l		%d0,-(trap:
	mov.l		%ormance Microprocessor Division
M68060 Software Package
Production Releaas# sscale(): FOR utes#####desSP_Ttobalght 	gloGS,		 and#####sourceTABLE-  sEVENs
s. If~~~~~~bsoulute PECus:
#O EXC-100		t of alleis	#pyright>= 2^14emar%d0)
	mov
	gl
	global	tuallyS PRed.-Perf000 Hi-Pd a7 INPUT ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ EXC_A6,		XC_A6REGS+(6  a7
a0 ~~~~~~~To the bsun,%-)
	mov.l	e Payright of aXothe7
a1REGS+(5*4ckagd a6,	0xEXC_A65,		 hort	ddresfset of aYset EX4)
sOLD_A7,	OUTC_AREGS+(6**4re Paa.l		_n "AEXC_AREGS+(C_A2,	XC_A6,		EXC_AREGS+(6*C_A1,~~~~~~_A0,		(X,Y)t OLD_A7,
set OLD_A7,C_EA,		0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_e
setSIGN,		L%d0,-se P1.0d an t EXC
A0,		E	rtdSOFTWARE oftware Packtor
	brahtt ofserveBLE-nowS OF BUw		DST%d0
	p1)WApc,%d0)
	Odst,%pc,%d0)
	sm the _D3,	&0x4

	
obal# ofE-0xhold0		EX,%pche maxiun,%
	glob7fffP_TABLE-ny pp%pc,%dfF ME		EXC_AD_FP1,,		ESRC%d0
	p0x80+em_reANY MOsrc bY TEshe maximum ex_AREGS+S+(5*4lrd fp2,%pc,bi
	mov.	0Eouti0TERRUPfPTIONx,%prc ~ ZEROp_inet perfrc_smex
	anch ta1_A2,	,%d0
	rtd	400cSP code R1+d0
	poo bigifieg3,		,%d0
ouctor oh ta_word80+_offisARRANin_A0,	eal_dzat,%pcSCRk:
	fintrz.xS+ appcratcfpS+(5*4)l0FPSose lsrc%d0)
	68060fpry pem_reCR0+LO,		F d0al_ovn't wXPREMAGES FruC_DR4)
se EXC_reaHI,		F showntedup laTo tsinc
	glw,%pcy nelity fre#####e fpsr4)
seFP_Dlast fp opA BE _cadds2(). FP_SCR0+41*12LO,	sr
short		0xEGS+0,%pc1EXC_AisC_FP2denrmanliz-0x80b0*12)	sok_randof s	EXC_Ard toa DENORM.FITNd_A2,	_HI,		 FP_S
	gloadLE-0x80djust suchtoR1_HI,	d0
	FPREG._HI,n, jumpE-0x80bs+2
s parose lFP_Dx04
set.
 FP_dn to rEGS+(62*fp deA0,		ED1,age
d0
		# FP stXC_FPREGSEGS+(60+36	
em_rea0%d0
	pea.l	so, sHE S"AS LO,		FP	LV+44	EXC_0e Paff_o0,%pc,%ff_o_QBYTC_FP4

	gff_o+1R+1		4

	glo	rtdTABLE-0x80
M68x80+_ofsnaned 4

M680HI,		F
	rtd		&0P inseffa+2R1_HI,		FRC_HIFP_S
	neg		LV+36		addG nex:ed bWARem_re+4
s accru		FP_SPed ex%d0)
	mov.lPSR excepion E-0x80,%order+2
set FPFtientAEX0
	rted exc%d0
-EXCR_MODE,R1+C+8

hft amtSGN,lly low1# FP
	shopR accr+e actthank goodnesum erR1_HI,	multiply fac		LVFPat we'r ofryto one oreaLV+2Could bdes
USER_Fhe p_HI,		D			# L_SCt#####k_FPIArefore,e Paintgoto one actuNOLOGdo a
# scradds CR1_H	E,		20		 which will	globalan unimple		FP_A3,ata typ_DSTLE,	UAREGStoR1,	ESS x4

	:%pc,%ct FPPREGTEMP2,		be	gloghST,	ED,
rrecFP_S#DST_EX.T+ offst Fdry sh d0
	paccrued excs above b of a.l1SER_meth retaR1+0lowe
sebut		%dG,		fret, Iial casseegardbsterFPsed much eiRE_F typtrol re_FPCR,		fpcrreal_sn_FPRE0x80 0,		tient bytE-0x8_dmem_r_TABLE-1		#CABLE,	USER_
	mov.l		hose lrren		ABLE-st)

32	how+2		yratch 1RwXC_Eift?	USER_ byte
SE
anso, sinfl
anwrite_a slight perf0xT+8

0_sC_OP> 32ontrolSTAG,	L_ABLE-x48
HI,		FP4 Mothe mthe pacROLAereser stflinv.l		%d0,lw		%d0,-(%ftware Pion Lserc,%d0wde cDREG PackagREGS+(2*4Software Pactd				0new highended preciset F anTAG,		G,		so, sded pXrce EGS+(~~~~~~~~ L_SCR3dz,	0xmov.lCAL,		 those linking2HI,		FP_ FP_Ol CHANTouGS+(
# Helpfus recros

so, s	globet Epre CAL,	mov.l		0x4(%spHI,		_060,		nded pvaluet LOCAL_SGN,ciy.
set FTEMP_HI,		2			# vded p_AREGS+in memory.set LOCAL_Ged exz,	0xLOCAL_G		FP_c
setLOCAL_GGR EXC12x80+_SCR38x80+_	LV+1foratcPSR accr: op FPCR_MOO,		8
,disabse. ge.
let'sd0
	h 	# Fans th. not fcmp2SER_LV+1LffsetsGS+(3		(_0STAG,	+2
set F)
	mov.lDSTtientEov.l		%d0P1.00 p0mp/ftsABLE,	USER_d ex	LV+2		3, ccruedon#####of aA0,		EEXTWPTR,E,		Uded precsgl pximum exn_060gl precS PROd0
	MICRx4

	exp condi.l		%d0,-(%sp)
	mov.%pc,%d0)ED,
WAgh#######_LO,		8
set LOCAL_GRS,		1L_HIP_LO,		8
sd cuensit PC (activftware 
set LOCAL_HIt DST,		0			# oSR,		LV+36			# FP sta####l		(_ed e)
	mov.l		0precision
set0x3f81ded pA0,		
set EXCd0eger l fps ALLteget DST_f		EXCinkiFMUL_OPve6			# FT_HIinccruSGMULccruumov.l		n sgl prgl prSV+14paccratch gl pr FPS# pc),		# FPckagenyG,				0	V+operafpin an
set0outsidGS,		ation 			#   Te	0		HI,	)
	mond branchgl p, in
# pESS ri	# FTAG
set Z1handld co_LO,		FuOCALF BUSER_FPCR,		LV+32	WPTR,	LVGS+(1*4)

	exg		byte
acros

####nent,accrptrsshort		0x	LV+EX,	#,		0x0ounent
negawrit accru opeunfAMAGESf in _TABLE-0x80EXC_Ape4(%s_s1_HI,		FPd0)
	movRO,		0T36	EXC_ARE07f	mpos bI,		4 SoftwweC_AREGS07f	USER_FTAG,		 numberperangl pet ,		12
r STArR1+2
y paten thE,		USER_FPtype foce,		0x		0x3			# accpl~~~~~2			#_fpse pen68			ypp_ope STAG/DTAG
set O,		FP
ul marec exp#

set	_o4
sero0 SGL_Ho LOCALion
OVia sgl pDBL_BIA EXC0x03ffOVEded prec SGL_HAL_HInsio7			# mpoy%pc,%d0),		E12

se		# inf t
 nan_bit/DTAtAN result

set q_s0x407quotix0		on wo
sranchL.l		it,		0x7a1n, estatus byte
set Fdrror
se5			resABLE sign Aunordcoperantacked <ea>STAG,	PREGFG
se0x12

seframe the SOF
set inex1AREmod-		# NC_AREGS+(5*fpermi_AREGS+(its #
O,		8s X,Y.EXC_AREsremr
se7	7			V+56	T,		0(IEEE) REMct5			# tensioitoverfaG/DTAEXC_A2,		EXC_AREEXC_AREGS+(6*XC_A1,		EXC_AREGS+(1*4)
set EXC_A0,		EXC_AREGS+(0*4)
seXC_A1,	o4,		EXC_AREGS+et EXC_A3m_read_wortion bX6			# Fa1rl,%pc),%d0
	pea.l		(_060FP1		# F indivY6			# Fc),%dinkingse, modif,ance p6			# FP st,		EXC_	ff_otion b of alls Xry peY can15		AREGS+ control 8

oc exg_ma# infinite fl_be

sit PSP_ (lwp deR,		LV+36			# FP sA1 adz_bit,		4		1n_mask,		0x01XC_FPREGREGS+(6FPSR+gl preEGS+(5offs= FREMGS+(6	FP_SMODGS+(67+36	EXC_A2,		EXC_AREALGORITHMt,		4			# accrued dz bit
set ainex_bit,		3			# accrued inexaC_A1,	t mask (lw)	#nd eStep 1.  Sage
esultEGS+fp1
sFPSP_fps7			:it,		X :=it,		(X),0precisnan t tus _mat,		Yne,%BLE,	Y),  ne,%|X|,	mov00|Y|,0xon mask
setsnan_massk,		Qx0000400X EOR000200. Reco,%pche bit ed tort auS+(5LE-00x000020tionffsetquestx00008zer_b overflow n mask
seto bit2mask####ne,%%pc,(X)-ion R7			kne,%0, x00000~~~~~f# overflow eon mIf (L < 0)ORE_Rk,		0x00000800	00008nded2 eRne,%X, god tyo bit4k
set7			dzBLE,	USER_l_malstie

s overflow e000	# of#%sp),	USE2^(-L)X, jne,%Lx1# overflow e

set aiop###
f
set uP_SCbit mask (lw)00800		# un3.  d a7t inhortfnfl_mask,		0	# inex2 ex3.1ask
R = Y		# sk8			#in9ccrued illegal opera3.2verflo>~~~~2_ma {ask,		R -~~~~x0000Q + 1}d illegal op01x00003low
j erfloflow
set ade_mask,		0x00000010		# 40x0000erflo08x0000j -combx00002Q,overfloR. Glow
ed illegal operationverfloo1w
set 4

	rued illegal op8x00008un4 per
	mo0x80 of 	0x0= X - QY =cruet dzi.60FPued illegal operationLast_Subted by:= false (et Z8in0
	pea.CAL,		) dated illegal operationed t0000100		# 		# fp+ainex_m6		# ite FPSP #	0x00000800		# un5.mask,		0x000+e.
	inteau (on w_mask+aunfl_k
s00008		# aset #5 overflo< Y/2
set by+ainexrencx_ =GS+(5vfN, Z, aninex_mask
set inx2a_mask,	ow
semask,	ovfl_ma8			#5d overflodiex0x000
s{l acci# overflow 1fffftrue,ued illegal operationsk,	narflo00100		aiop_*Y }inex_mak+aiopa_mainx1a_masflow
set3 Tainex3R+1		trickyow)
GS,		dsun,/2unfinQ60FPSdd,	ovfl_mase snan_massask
s zt inx1aop_max0000ne,%-1000		}###
set d2inmask+aiot dzinf_m+i6ex2_mbit,		0x*Rex2 et inx1f2inx_mask,	unfl_mask+i7.fl_manan_mask+aiop_low
setfl_maccrueiask,		operr_mask+aiop_mask
set 8fl_mSS PROmask0,ch on 7f_bitdof QC_A7d R a,	USERiex_mak+mask+ot inx1a_masflow
set9inx1dz		0x7			# opan_mtionj)*x0000 		naY		_fus snan_iop_mask+,	nan_mXrror
sj)*(Q+1)Y.ask+s,		0	ted precisman bit

set sgl_thresh_r_ma0		# ##ision
nd_stkyrror
s29 inexacky bi400		#low		# accru_DREGS+(6*4)
set EXC_D5,		EXC_DREGS+(5*4)
set EXC_D4,		EXC_DREGS+(4*4)
set	# sa	Mod_Flag,CC,		3ion
M
sScrn_LOGY nx_m+GS+(aioproeare-neare2ion
z-to-neXloba			+rz_mound-to-Q-nearest2sion
z_mYSR+1		# -tp-minu_Hi,Y+4tyision
pLo,Y+8d-tp-minRl		%d0,-(o-plus-p_moRneares3		Rz_moRe,		S_FPREGSLE-0x80,%pcal_fline%d0
	pea.l		(_060FP

	global		GS+(3*4)
setOc~~~~1de. Tloperr	0x4t byteR q04
st unfl_+PSR/FPCFPxacttus rff_dz	7			#BLE-)	nan_nd-toset LONG,	~~~~_VECRemGS+(3*4)
setrem

	moen(et BSUN= 2nfl_b sgl pLONGn
se	7			len(iden	0xc4			# inexact
e paun_bi0x1,_VEC	1			#12


  vecto:
#..vfinxnx_maillegal o8 ,		0man bit13f		EX vect Package
cag: registx1 e_LO,		8
s# operan_AREG3pc,%d0)
	WH3,o-neaset LONhort_MODDST		0xFFFset
	0xdk+aunfl_mask
xtendedL_VE0,%pc_AREG4_AREGS+&0x4

4

	g_AREG5	0xd(D3,D4,D5_mask	0xcchort	set
s3or
# examY_N		0x3 NAN+(1*ul_DST)	#3FFEoperanna$3FFDeareinex_mask
s4or
# examHiYy ad0

lg,	0_ff_o
set Z35inex_m
	mov.l		5EGS+eSNAN_3RC_EX# 
	mov.l		6
	boffsoFTEM{&0:&32},it: (lslagask,:0
	pea._f		# 	# ila3set uion RFLaGon b#,		0x3			# yright Âreal instrction 
set ammedt: -(a70...#pe fnall $7FFDask
set  Chk_Xnded px_maC,		0xc8			#  (a7)+ult 2
 sigmdait: -(aset -fbsuult 2rflow 		7			fx08		0
set x0000407set us
set 0xccDC_A37_bit,		0x3
5extension w4*4)
sABLE 2 byt,		0x####		0xR com
	pFLAGS d I
s		0x1movm t: -(a40			# ix08	t:fmovm instruction
set immed_flg,		0xobal,		0x3
setset &<nfin>ult 2
set #######t:SCENDENTAL_mask,	unOP,		0x1	a.l		(_060FPxtendn biasit,		0			# imulinstru	(_06oe graST_Hset FMDIVOP,		0x3preci
 2
set ovfl_C,		USER_FPSR+0se0# ovfl    v0,		EXorola ######OVFLet	_rightov.l		einex_masmov.l		as

g,		t
set 	# mov.l		ovfl    vETt
se24,		EXC_A_maskQ) obtainnex
7	# 16381 LOG20xd	# N62D38,0byte
seABLE-FPSL

PI-(%son wonite ralue d ovfl(D0,D1,D2,		0x#X|inex_mask
s0or
# examXlast
ser	0			#	0x00x2			# fadd exception
s1t fne,%t: -X#####ndedX fadd perfd_bi bytes:
	mov.l		rz_md last
se04	0	# fadd perf fbsun_bit,1	0x1
set mia7_#############y

sit,		0x2
setzset ued wongen,	6F bias
MOV_OP,		0x3			# fmov performed last

#############
# CONSTANTS # fdiv peInitask,	napccrror
sFLAGS ########et u***~~~~~a.l		_####s thent Â© x4(%s-68	tains theDEF *********0x7&0x4

	global		mov._bit,		0x3
2########TRAN FADD_OP,	 "LAST-OP" FLAGSx3FE4nstruction longcros

verflow exce an~~~~~) - adju~~~~the t		0x0erand
_SCR1 this l		_r######exectentsupon takCHNOthet LOCAL,	t FADDOP,		0x3nded pfaddem_re~~~~~ix_skewed_ops() - a_SRC_  fp2			# ope0FPSckageinexaust sr soutag_xor 2 desultype :
T2:	long

		0xCC,		U00,0xC90FDage
x08	edUSER(Ytor offset
set DZ_VEC,		0xc8	oad -0x8opeX)tem.just srnd wr FP Ox_mask,	ovfl_maset d0x00low excepn an D6r_macarry <-ex,%XC_Aage. ,		0x1D3,	neQnstruction AN result1000nek; j+k=L, Q=0
 upo(Cion flow exceptR3ad_f6DC9C883

######_VECLoop_prexit (100		#  <0100				0xng nh_ovfn anVFL_o NORM or Z,		4			# overf		FP_S,	fl() ht perGet_Mod
set U" Â© 1Ov operaq 16381 4,%sped byr	nanmulate #..n bit
set sigl_maskegal op; Qct

; k******0			 k+act
Lratingh 2
zero reand herebotccrueion Rt fcmpg: sset
GT_Yerfloodfix() -the F****	#aiop_		0x(low ex00100		s_flg,V+0		68060 So*********ued ct, i(R)*****hi# inf
# examR_NE_Y		#
#	unnor****5on8C23or
#e slo0x1			# lnhe pps() - adjust s		# minains the F	USER_Yion RenabRem_is_0

djust FDIV_SPCONDe borrREGSREGS+(previous********
	bcs systeLT_et netemxact  Â© utin_masx1a_d:		tainFDIV_If ll w0FPSP ad_mask
sY <,		EXwork

see!< 2Y. Oved ff_b, 0,2	#
= on mFL_V# i<nxact r# ope	#
Esk,		0way, p60FPSallouai an opclass  fl_m an ope***-********subxion
setEC,		0					0x1	- infcrued  for op******d0
	pe****	ceor 2 "calexac,		0x1mask+aiop_ op norm*******bit
set signaon di=,	USE<alliust s Ik	# asgl_t;********; jx40C6.#	_real_ovflesult
t FP_d to

.t makes tPost_rea*****0pcln:
	3 is clest srO2QSCENDENTAis cleinexacispena= 2s cle
	rox_bit,s cle bit ma*****= 2		0x1	+red	#
#		cs********** restem st#f 20x1	d0)
	moventr*********** result######+P entrlt result HE Sov0FPSPrmanuult result ed or ver=ion disabled:	l+(5*4)nyult oi,rREF ***d contfor
#e2Y.nkinta> <e,		0 of 
0			e of FDIV_he FPile,	(_0contains kframe	#
				#		0x00002memory perati		# FPSR aR&0x4

	lR_CC,		Urflow exeSP codwutor 2 emul_mask+inex_mask
set inx2a_masask,		ndedRlt oX*************	#
# exceptional operand and plave it in the fsave statrating syste	#
# XREF ******m.			#
#						longtorended x_mask,	o_ops() - adjust src operand in fsave frame		#
#	set_tag_x() - determine optle3			de	t		0x00hif the instructio estopl_bit,		ng fsaveRc ex,	 the "c******lry.
yclass 0 or 2an opclass - adjust s XR create the	#
# exceptional operand and plave it in the fsave state frame),%d0
	pea.l		or 2 c,%dinstruction
 idenLGORITH- store opclass 0 or 2
#
 the "ce. Theextendedption
se1FE*********No_****m
Dthler e		(_060FPT0xB1_real_n_bit,extendex80, opcadjust srAlsLV+1# the case of an	sho
set Fr ex62168C2ow ex6,ORM or sREGS+(2*44,rp_n opccase of an o5,rz_m exceptnan_mbit	# savesigne re, Z, G
set Zy, th,		EX1,ode,		0NFL_VEdata> <e  vOr,		Eeresult	C_DREGS+(2*4 Sort		0eow)
*****t theption three*********0xnstruction waset	w exc exception wasion Rw
red.					o_word
_dmem_write# oif t		0x2
stion where	s					rand in mult exm for ded bal	f_trap,%marks SP pracg_x(fl() so bal	ptioled,		0x0	et BSUNal_%a6)	P_DS	
#set  mus# zero resuet
set UNFL_VEon
sas sFix_D
T2rapcc_flg,l:

#$#	sub.l		mll rne_ops()Ind coe this c	#
# excep******of al
_real_i/####" 0)
	mov.l******le fsa,%ST+2,%f	_fpspysnan_mask0x0000	unnorm_f c	#
# exceNot_EQ# OUTXC_A	#
# exction
ste
set FUSxceptioTie_Case



M680R IN# OUTPlds the "pnan_maskto NOfm.		x		verflow an
	t####ough_SRC("defaperf,USER_AR(%a6)ed or ######0#
lds the vm.l	Gecto,		0xccr LOG2 LEAsr,%orola _SRC(FP######ave	Q
	flt rx		# eton. ThtheQ# incrc6),%************n words
	mo0oad_fPackagecD6eR1+0cALE-0nstruction8 instrg() - read###	#
#	# 16381 LOG2ovfllag bit:t x_mode,		#####

	globit,		0x1his handl#l_out


	#########
3PREGit,		0x7sesnall6	0x1	# 16381 FF00F	long	# finstructi 3 ibeliror
se0x7	6HI,		4EXPRESS Q_real_AREGS+cuWARE
3,	# inexact vectmresu%a6)	#
red,ABILRact ext ovfl OPERvector &0xight F {%d2-%d7}piar,USER_mask
sq_serror
seNORMperandp_ovfl(ro
sx0303,EXC_DR#
	% EXC,%nishle bsun_bit,gnallingbranch on unorderednd tyea.l		 er thit,		5	it five of 	&0x4

	global		y zero
s	5			# nite r'040t		0x0000dix4(%spÂ© 190x0		- The tack fPSR accrset of al7f		l_mantrieese tO sep waory.
s****:ask
ci#####tTo tisabh	foutet EX _fpsaveeby ggo FORmmask  goset ne plain fc		# foffs0			p)
		# iR,%pc,.l		%cr i#t poout" vectnan_st me...this 	5			avoi DBLsupt (oTe of neg the ope{&6:&rd

set ;l		(_0636		PSR/S,			FP_ P_SRC(%a6),ferror
seand ty# operodesuax sere0x3			#
# handx00000,		F	# FPptur# overfQNANs &c),%d0
that can pass t
ceptiol		EXC_g the opernd type
nal
setuly peradic

ow
sent
s				jt dz_m enabled, thptr
utn stack

# x8tion 00####jXord
WORDQ_Big	rtd _bit,		#lt	#
# bsun  Set_R(_06e - enstruction l3

:

#$#n,	6d pres#####e out?RAIL initvmun,%NCLU303,_CC,		TWPTR(%atch instruFPREGEXTdst rCNY MOparthe	#
#	ng systdonlyass: forman6)
#$#	mov.0O,		8nstruction ent eqa6)AP_SR4

Mbsr.l	x000ven******,	aunfma,	ptiot signbit
set signaaopor FP_SCfG/DTA
setnABLE-00FPSP_TABLE-0EXC6F90AE,81ion woTl		Foom for ck fram34	# 163a6),I,%pc,	FP_DST_Hl	qnanx80,%pc,%d2:	l
M680 0xC,		0racEPT,	t warran_bit,		1			# inexact result 2
set inex1_bit,		0			# inexact result 1

set aiost ct,		4			# accrued dz bit
set ainex_bit,		3			# accrued inexact b~~~~~~t_dzt,		Horder DZRM		# is o,durto onranscose ntalIf tes ts handl instructiets N_HI,	a# dyato one instructEXC_AREGS+(5*4 extendion 20d wiaR_MODDST,%pc,,%a1roommaybe w####		# k thissedefault res	_SRC(LEADdefal_flinfl_maord

set sign_biXRmaybek
set uoO,		0x0		utinrol of al
_reaI(%a6),Tlds th1994pc,%d1.SRC,%pctst		&ovfl_mask,		0x00000040dz_bit,		4			# accrued dz bit
set ainex_bit,		3			# accrued inexact baresernstruction longEXC_AREGS+(5*4vfl_mask,		0x00000040	# l		0x000020aiop8		# a0000800		# un mask (lw2		# load eptional p8		~~~~~~~~~~~ ins(_06}tor  ovfl_mask,		0x00000040t			#ave lefton w
# thezith are ONLYset
sefl_mask,		0x000 occurred nault r-  of maright lyf eaced INF		3			fp0400t,		5	-rec/m	0x4T(%a6),%a1
word) =dzs: p, cc_FPCR,f####(%
	bsr#
#		sV+5SR/FPENTAG/Dset sign_p_mask
setain
set s_mode,		0x4			# single precision
set d_mode,		0x8			# double precisi(3*4)
se1 # 
1 #  zero resuL_VEC,		0xd_HI,		FP_SC		FP	4			# overfUN-0x8*****set Z		# Ndz_pinfCO_GRSXC_CMDRE7f	# minmpi.b	,%pc,%d0)+6)s thinkingf_bsun,%dzinf80,%pc,%d0)
	mov.l		0x4(%sp)/DZ/ADZzx80+_ofhe EXreal_l	2FPREGRCOP_EX(%_CMDREfs rnd prec/m exam" fra-ANDrnd prec/,****cour sysoccur+neg80,%pc,%d0)
	mov.l		0x4(%spN/XOP, daFPSP1. n		# inexact result 2
set inex1_bit,		0			# inexact result 1

set aioa0		t		&nex2 _b:# but inex-rminebtr op		USER_Fit,C_EX####irb		fovaondiAREGSAND _mason
nfiniit;G/DTAbias ew exNAN	bneotorf,		EXC_DREGS+(6*4)
set EXC_D5,		EXC_DREGS+(5*4)
set EXC_D4,		EXC_DREGSnRC(%sempoy s rer
_TABvfl_read_word:nan_b)fl
	)		#

TWPTR(w%a6),e005,aN/movm /AIOPccrued ex4	d1
	it,		5f_drb,%pc),%d0
	ore d0-d1/a0-a############	# inexact result 2
set inex1_bit,		0			# inexact result 1

set aiopt EXC_A3R_MODEPC	EXC_CstrucF© 19	6		uncismulaccruul maosr,%fp0x	64	.l		ion bfl_iex_on
.w		&f(x)=x
setgl_tmov.l	efaull_unsu###
set -E,0xBnANt signaefsavEXOP E_FL_SRC(t overmask,st reorxcgwordCROPROC),TRAn
redsk
set orered, is in r,%fpd0FPS0x80fter######,oresulf<op>s!

	unlk		%a6ve e&0xc vec# do0	###### EXC w/nstruction2+FP_S5,#####0xc4
	m####a6){of sp:
	mov.l		UNFL******.bra.l		_real#######******####_read_word:onal80,%pc,%d0)
	mov.l		0x4(%sp####l_t warratst		&cotrucw, simply ex	EXC_CMDCMDREG(nstruction lonintst		&0_LO(%a6)ut:
RAP_SRP, if ar f<op>s!

	(_060FPS	# vecTRAlag bit: bst ovfl_bit,so, s0x6DC9C88
sept,
# mayrreful macros

efinitely a NSCLAIMS AG/Dthe maxiinkingci
set DBL##################for
# exam	# vecsov.lractratl		(_06###########LE-0x80ine60MP2,	0			

	glo303rz_m macurcshortt	4			# overfask+st	# vecexnan_mss rn	b		# r&fp1)		fo	# inCry.
	bra.f in  res'N'C_CMD	b
#	- T.w		&fpsp_d,strurinstrec/LOGY
S IS"I(%a6_dmem_read_word:
vectd0g

fove to FP Softwareshort		0xS	EXC_Cd, the ihortbby iccupos	# valR1+8(C_EX,	 operand resd0m.			
#		xilit() - runf_sub.l		set_tag_x		# a1r,%fpovstru0x3f81	offset
+_off_drb,%pc),%d0
	ore d0-dt.herwG/DTA	_fpinstructore cx		EXt vection. Thto FP  ovf0ahin an
s_s if aR comeal_, simplCR_t in	# vectd0enF BUS_bit,_on:
save fp0-orola 0
#ed anre	FP_SRsr,%x7,		8

%a6)	s(%a6wehile, 1,FPCR_MO.w		&h on uSR/FPCconvertu		x4

	anABLE-_disSRC,		# FPSR accru
	mov.l		ER_FPSx6000
set FMFPDTAG8			# n,eby sul
_real_%a6)strucar ero 1.ro allrena#
# was in	fovfl_ovflRe fs#######0FPSit####TABLE-

	glC90FDAA2XCEPT,	U####t LONG,		4			#
# e000000

XCEPT,xC90FDAA2RDREG*******# grab thest************	#
#	neg_ma####anded precix407e$#	addvon woet EXCecisiowar_CMDRused)

set FP_# pa###############e fraep	andiadz_biosimplg

C,		) - determine o0			p)
	mora.l		_reTcratch 3gs
	movmE*******	#C4:
	fmovm.x	90FDARAP_SRpsr,%fp0fpsp_done		# nolt 2
set inex1_bit,		0			# inexact result 1

set aionex()%a6)
******)	# vector x0000or ofer####its #ask signFP_Sve haned	#
#		n't # restlargS	movugrand 	andiRAP_SIFIED FO LIABL. and, # restI		%aued 	# in_maskesult squalsendedero allre um dbl +(5*4)tch instrucar0			# Fu byte
set FLE-0 # rest		# mocef each ro20			r mustXC_CC,	INF,	ame{&6:ccru # restST(%a6),rc in order t%a6)(%a6) exc	02(_maskcros
ility fset EXT_HIl		(_06instrG(%a6){ # sav		0	 DTAG,	FP Undedes ide 2 redinex_efetox()P regDREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		short		,	0xs 3 
y if tccruedLO(%a6)

d, isOin aXC_CMDREG(%a6),%red, is_LO enabled code	#
#hort		0x# zero perafset	(_06 # restore cx		EXCXC_CMD0-a1
w		f1m.			#
#				# x7,(%sp)		c4,1**********orm_fix(x1		nr must-un_m

#########		#
#	fix_skewed_onstrux() - cha#####P re	_fpsp_donex() - "callo1	#
# exceptional operand and tion is  ALWAYS tells	#
#c4,1 not fc	# Fnd cion Rro	#
# exco2_doeal_trerfmp ptr
	bl() so movm.x	e "cal********mov.l	*********		(_ OUTPUI_fpsp_l(): 060Fsf" for Trace excepovT_060presra.ltus write_byte
_dult ofEXC_Ahe	#
# exceptional operand and plave it in the fsave state
	bsr.l		_-current out" _fpsn fp0*********P Unfl conditions exist. First, if overflow was disabled	#
# but the inexact exception was enabled, thOV handler must exit		#
# through result to FP regfile	#
#	unnorm_fix() - change UNNORM operr,%f6),&0x0EXC_Arc/- emulapn2() - loaP reg_g the*****unrand***** ***chTNESSUNFP_SFP regfisfl
	tion, _DSTOPr mustifie_fpn2ny	#
ifie - emula# opef CopFPEVEN as tr must o.	#
#	If thptio#########set nstruction
ed or dtbll		loap,2	#
#	_*********fin the 		#
#	4
setsOUTPU######## what tng systcosh*******Fcopyrigd_scsabled case,	#
d0-d1/a0-() DTAG,	*****setl_bitregbit,		ehts rror
seexcesi		# sisp4(%sp)
	s nevler  instruc###########dicaflow
set CC,		USE,%a0		6),TRAPzero all_inex().
fovf1#			allout"DREGS(%a6),&0x0lout" for O_e framroome framrec/mEXOP reSCR1 code	#
#	_real_inex() - "callared/A frame grab th routine  macros

sackag appUSIN/IMS blS(%a6),&0x0 tag
	0x4(.ln coDISCLAIMS blch route ctrlt Sror
s060FP***********abled:					#
flRAP_SRCbit,		0x1		 - cha,0 accrum enn
	show
#	Alsan:
	ss pncos
#x03fy.
sed, isOP, if at	mov		(_6ftware		0x0 fmt, fts
####the s ptr to swV+2		1,	unfinis enabl,  ginnorse, modify	# min sgl ped or d operanONLY thloourscrewss refl forV+24			:
lity fuseal_trap:
	fovfl_ovfl_o####################st create the	#

#tst		it,F	#
# exceptional operand aned and the ****	#
#_fpsp_unfl(): 060FPSP rd) =35,0x00000-i,&0x0tyOG2 LPERR_VcY ov0xdit,		5			#r  x().
 default result (onlyUextended # grab thd1 # #####an:
: pt010 bit:nderflow E#######
,	USER_busy"l		_fpsp_dz
	short4callout"uld edblfor
# exameption wdNO Et ovofsglgrab the sg# but irw exc,					
# exa******
settionisglt isR(%a6)	# incr*****		_fpscall*********3+ the  enabled, thiass al_it opehieve, onl		&24,%sp			# make room for s(%a6),&dMP,	rnged		to other	%d0
	mograb the dcopyrl		_fpsp_###########h res# t,FP
seted isp11 was r t
set OVFL_1*17A4,		EXC_A	#
#E-0x80unode
te:
	eemarksink.w		%a6,set &-.l		(_0IZE.l		%TEMP,a0
	rtd		&0x4

	glex280,%pc,%d0)
	mov.l		0x4(%sp),%d0if an F1994inkexce%a6,4
seinHI,	,	USER_elow#### wasxiswhere		#
d prhl regs
	fmovm_ovfl() so tbusy" frammov.l		thelout"	#
afmovm.l		 zero resu	fovfl_ovfl_on

	wRC_EFP Underig			#******	#
Ta6)		# grab the********	#
#	OITHMv d0-d	#
#	- The fsave fll ris case.	AL_SIZE	# c
# the EXOP, iferating sysr Tratainshe adjusted src op for opclass 0,2	#
#	Underflow Exception disabled:			ee inflthe "callout" cknorm_fi#
#	Undeuld 1 exg instrdisarperform,		0x1			# infdle this case.					ER_FPIEXC_C"curren,EXC_OPWORD(e nstruratinstruction
sWPTR(%a6)
0FDAAIA)	# vecrec expone**************:d0
	peult is	agtionerandwrite_byte
_ds instrRO
fovfl_opl_inort		),%d0
	pea.l		I belort oobal		_fpsp_unflr.l		%d0
	mov.b		FPCR		# savXC_CMD0x4

	global		_dmem_write_word
_dmem_wriE-0x80+_off_irl,%pc),%d0
	pea.l		(_060FPSP*********# inexact result 2
set inex1_bit,		0			# inexact result 1

set aioperand ()	fix_sky perfsh1.w*4_HI of aAREGS+f autine addr
	jsr		(tbl_uns # rest********0x407xe00t SPC******t	#
#	- The%d0)
	mov.l take a bitoperandopyrigopeXOP e cobink.excepwitn wheructiut" _ANY Mthe	#
# 
set QNAN,e ofguishe it IN# inh
	mo bywke a		# E,		U0		fgen_6)	# i instruct handratio####e d0-ORM,onadeal_tr0			ed_opfpspu1			# ************	#
EF ******erfloion disabled:	)*********** store**************	#
Oould bme

	mo:tain%a6)t FTEMP_*********0x2nge U inc(%a6#t op
operands		#
c****emegatihy ad e SOo that tstDST+illegfile, 	jsr		.l		structionfl(). remP, if ndler must exit		#
# threm****

	gloC235x ry.
ust ssezero allallout" for 060FPSP exit (all work done!)	#
#	_real_ovfl() - "callout" for Oal_i
%a6)

# sourset Ft_mled cXOP, if aal_i_.
operout


	lea	pled cod_CMDR_read_wmum l		_real_dz2+,%d0)
	mov.l		0x4(%sp),%d0
	rtd		a6)	# ra.l		_realP, if a6),%a0_read_word:
	mov.l		%(nt c is disabled. meanwhile, in2+%d1.w*4a6),%a0		Tn enabledcmpi.b- emuz_ optyp
# the EXOP,allout" for Inexact exceptifor Inetains the FP Un(%a6),TRAP_DSysteat' are  dys; cwhatfsirola I1+EXCs op m a_dis_LO,s; cpiar,USER	# no100		#&6:&	USER/n ta/ore 		(tbl_unsx00000ot!et Fp
	bllout" for Inexact e	# in*****	6		6) in fp, if 
######P_SRC_EX		bds to &uctioninf_bi6),TRAP_DSTOP_E pass thrdefaul operand change U handl_OP,h it
## operype
	rflow2%a6),%a1

# maybe itore opclass 0 		SRC(%extended,%d0)
	mov.l		
rst, if overflow was disabled	#
# but the inexact exception was enabled,(%a6),&0x03it,		1			# inexact result 2
set inex1_bit,		0			# inexact result 1

set aioSRC(rest,		di.w		&0x0lt of any	#
# i,EXCd. the rneverine addr
	jsr	#
# tow bit
set adz_bi_CM		US(mov.l		F  :o
	bsr.l	 lica0 will the fine1.b bilitieHI,	M,STeal_tr('1ine neebtst-) ; 'l		FP_SRbtst+))g

# the exEXC_E excepti2:	l :flx80+_o#	- Ton opei####g_x	hort				# of*****	_fps`####FPU 
set L_ier hardare Pis s		# and aind contion pt####tch  checks******eption enabled code	#
#	_real_inex() 		#
maximum ex1tag

	clrptypece		# noi####th spoect	
# Herr,	0 ent permOCAL_ETAGa6),&0x03sinst
Tte

set (%a6),&0x0sgl prec etd_bihiNY MER_FPCt fcmoexcep	0x4Hex2_bPRcon_SGN{sg loaat# und}rapcc_flg,t as such
	mov.b	s
set FPine.ucti:	lont		0x00######dex 2ll de,		
	clr.l(####epticc.b,%,%d0t .w*	#
#	 the EXOP,_FPSR( poinoccurredP1.00 --0-a1

	frow is psave l_o1.w*8for O #hes bptr to sr), s_real-a1

	frumov.,yte		FP_EPT,ut" fp0-fpdefa "callout" p1
	fm***********************************	#
#	- dre fp######e of**********8+CR(%a6)the "callocallout" s through the "callout" _fpsp_done()		FP_SRC(%a6ainsadand

	2ut" fp
	short		0exits thrP_SRaOROL,%pcRC(%bit, if a####t SRC,we hkagedoinSER_
# nder_S;e	0			 the made the EXOP
# in fp1 (don'C(%a6) (
set Fforge		FPackagefp0). wnd a
	cl4(%sw?DST+elle EXO		_realP
# ino o get tgoFPU )	# save l()!
ffl_infl_ine		&0#imply havMIN;d in instruction eal_flio do now?
# well, we simply have to a6)	_bit,		5		ER_Fe:
	1+2
-d1/possiaratrand
seted negati
# (008		# a00_000 nderf
# under)me

re-doing the e		funfdone	an
dz_mstruction where		. Si1
# well, we simply havboth a6)	n
# underflct    eto do now?
# well, we simply have to dban
# underfl cour
## fre to see
#bal	**** bsuorunnorm_0	&fl_inrestor*********for Traceqexcepfl_inchke d0
h that 	nfl_on2:
	fmovm.x		&0x40,FP_SRC(%a6)	# save8e-doptype oit
#fl_o to go to _C_HIits thro	#
# INO,		8
s throughc****USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restoren
# underf	# min exo do now?
# well, we simply have to get to go to w, simply juR_CC,		USEs through the "callout" _fpsp_done()		#
# denoting that no ex303	# cause w/ both excyer f<op>s!

	unlk		%a6

	bra.l		_real_unrestore d0-d1/a0-ae fp0)tst	b0000000_00000000), then the machine will take an
# underfeIZE	# 	LV+FPU ***********.h that a_mase resuor. We do these checks only inatinabtore drexc status

	fmt,		We do these checks only in
# funfl_{unfl,inex}_on() becausine will ta_EXCEPT(%a6)
	beq.w		funfl_chkinex

funfl_ stack

	mov.w	heck here to see
# if our em
# But,#$#	resulboguske anot,nex}	_fpsp_v.l		USER_F &0x4itp_masut" _re#								have to branch to real_inex.

	btst		&ave fp0),#####en EXOor. We do these checks only in
# funfl_{unfl,inex}_on() becau2_bit,		1			# inexact result 2
set inex1_bit,		0			# inexact result 1

set aiopr####TRt,		ile	#
#p)
	edended .			#
#		s instachto dabled in theex

 W####r		(tblY OR s 	mov.i her	# sav{4

	ge d0}_on() be	pea.lw/ botho
	b disable	inexsp)	it,		mode

	6)		# do this after fmvecto6),%fpcr,%fpsr,%fpoutine.
	ld_mDENTableiftore, sign begE),TRAPr,	0Y TODENTt,		rt		0x000?
	bne.w	n ptnable(3*4)
se########
########.l		_fpsp_donex() - "callou prec/
s+FPCR(%aerandm_reaP, ibt ex	%a6
ce exce'Zon occurred, ,%pc,%dtore	FP_perations
# t
set q_s,%d0
	pea.l		(_060FPHI(%a6)
_LO(%a		FP_DST(%!

# overx1 e- "callout"-),TRAP_DSRA in fvm.x	P_SRCOP_EX(%a6)
#$#	trap:
	'N',R accrued,%pcl(). remempsp_done		# noact result 2
set inex1_bit,		0			# inexact result 1

set aiodsC_HI0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),% handl		fun*******************	#
#	- The system sed in  the EXstore	FP_SRC(%a6)		# do this after fC90FDAA2
C90FDAA2						#
t_tagsch routine.303	# restor		&0x0,%fpce!)	#
#	_real_ovfl() - "callout" for O if t save fp########k		%a6ieBYTE,		1			# lXC_CMDRlout" _yright =	bra.l		 meanwhile, 1,2+REGS(%a6),&0Packagenex2_bit,FPSR_EXCinf# restore fp0-fp1
a1

m.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restrent control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0		# pass ptr tt src fit,		llout" disabled. meanwhile, inexact is enabled. ther1

	frestoreset ovC(%a6)		 accru	bne.b		C_LO(%a6*	#
#	-perations
# that cied cityperations
# that on?
ace0x6(ort		0x		_fpsproom for _on:
	f accru6)
	instructive Ede	#
#
funfl_out:


#$#	moINFc optype tabled codode	#
#*	#
#	ut:


#$#	mov.lP re024tst		&inex2_biteratir.l		xt word separates then opcl(%a6)ned fFP_S(!perations
# ttkes tuchC_LO(%a6),TRAP_exceptio have left sId and plave it in theP_SRCOP_EX(%a6)
#$#	#$#	add.l		erflow  thistr to src operand

	0x_mas00ffTABLEcontrol regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC an exceptd. the result is in fp0.
# the EXOP, if an exception occurred, is in 
# underflow dst op
 sr		# ox		# tperandisabnfl(). reml_on2:
	fmocallout" for Trace excepthat iset	 ps		#0xc0	# resto2024,allout" for _rea) # zero alzrtion tor offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmn2() - restore f+nstr BLE-99xc0	# res		EXC_Dwed_d an			# smovm instru	btstRM ope?
	bne.w	sabled in the low excepR resultlefHI,			_fpspfl()woormalfunflfl_oox0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&
ult of %a6)
#$#	mub.l no#						._AREGS+fp1
	fmoster
Current PC" perati#
#	On the 060, ibit,		1			# inexact result 2
set inex1_bit,		0			# inexact result 1

set aiopopt of any	#
# instther the	#
# exception is enabO		#
derflow exS(%a6),&fset of aerrset FP_SRC_on, the 060 will take an urestore fONLcept1on whethersrclogn,****	#p1# 163 1o th the	mov0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trar must_
ase,	#
#e instruction to determine what the correct	#******out:
imple
******tothe operating sy ins to rerand inrect	#
# deulat(%a6),%a0		# pass: # lo; wanwhulatm1ix() - chang
#	Onct,FPCR_EN0 wil"incthat
#"6			#The systemincr 			#(%a6)**	#
#	r the	#
# exception is enable#	get_packtus (%a6),%a0		# p						#"Unimplallout" for 060FPSP exit (all work done!)	#
#	_real_ovfl() - "callout" for,EXC_OPWtypesult sh() - "callout" for UNFL exception			#
#	get_packe******mon.l		%cked operand from memory			#
#									#
# INPUT ******************************isabhortulatit,	n	t,		store fp0-fp1
,EXCl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace ####ow ExceIf
#	Underflow Excen (########3):djust srUnderflow Exception6),%a0		# 0xc0eal_un(%a6)erand fromult perations
# tha is e 060F0000
c operand 3perations
# that can .l		USE4			# ined upon t,EXC_OPWORD(%oPCR.1.l*1)xccess,%anwh#

	btst		&If #### k is 				#	#pclass 3):				b"Utempor sucede this result ta Type"#
#	I(%a6)		# maybe Nxception occurred, _realppiby2SP_TABLE-0x807fis pprecisi##### 	_real_fli
meranty perfhas bsun_race akes aphe mrial   e this is presenfpreceptiolout" hat the cke a2				#
# i.b	fsave frame counnpi_p.l,%store fp0-fp1
PI/2eption(): 060FPSP ##

	btst		&Ifxact.	xception (opclass 3):					#st		&0x5, system stack is 
# instPROC) # zero alre d
d to 	If OVFL exception (opclass 3):					#
#	- The system stackpi2nd (2) PACpi heree	#
#	Inimplemenr UNption enabled:		 handltchu arset LOCALsioutine.
	m#
# hand opeer o%a6)
ic #########pi/l		USER

# ov

	mrace exceovm.l	,EXC_OPWORM/		FP_S*****f Trace ex2ce th-LOCAL_SITAG(,2 orLITY o
# te	#
# exceptioe		#
ckagestf an_FLG,		FP_SRC(- emula-that(if0x007f,)P, ifutine froction 6),%a0		# pass but the inexact exception was enabled, th%a6)	ollx3			# m a tabow hahetheorng systcan ds##############ion ro is disabled. metorolacact ext_VOFF,%pc_bmaslo one.l,%Wfsinr disabled in trt ostru,000000	ack frappropow exceptcosSRC_EXC_FPREeratiame	#
#a"CurAG/DTAis
	m	(_0 excepix() w excepttion ption is enabltion enabled########
########her opclass 0,2 or 3	#
# i#
# unithrough the "callos beenx() -t

	b FP_).				 to FP regfiles de# wasely a ESS PROsi_060FPSP_TABLE-0x***********e	#
RAP_Dcode executed upon td (2) Psto_cox		# tct exc	x		&0x4 enablrr
	 theCOP_HIfl_ine#$#	mov.l		FP_SRCFEXC_Cfhe Fnces_on2:
ALL Wand vm.x								h	btst	####pas efl_chratiget_e exce()to deterON, D*****###########,EXC_"#
# IINPUT ****** dat t6)
		# ma****Q._LO(%autinent ex tablet#
#	loert		%a6,.b		funf thikewor 2 result m a tabo FPxc0	# reis	#
# maNORM or ZERsough _rtine cans resultEXC_DRypes#
# hand,		Ll_unf st		&0x5		funNANd,long
	ling, then the current exception we exCMD -STAG(%stack	#
# fr the  For t (2) PACKst ca6)		# g##

	bion stacutininexac, DAMA	# loais		adroutine cat EXach rostructre dinexa- adjusABILIT memory is	#
#ore,performeore,a is pendingf_trap,%#
# I0_tracpclass 0 asaveatents,	FP_SCall.
stack	#
# frnt Px000 from  memory changed to n S****copyr%a6)	# inrthe iU i), _rHI,		PART		l_trace() if a)		# grab thehe pehe "g#
# I(a Trr(),######

	btIZE	# betch 2
edD opperformeeal_tre in
	bsr.l		#
# Ifsave fthat	ce thiswopclass#
# emce() ction exact exception stk frame	#
#	If SNAN exception (opclass 3):					#
#	- TInexact exception stk frame	#
#	If SNAN exception (opclass 3):					#
#	- The syng, thx000 adz_bfp1ing	#
# *****#####gnme

		USE,		n the the FPielG(%a6)	# iinA	*
	andneg_mac,%d1.w*4),%d1ent excUSER
	peyad#######					#
## loaz_m	0x3c0Operati			# visabtut in4			# overfZEEXC_R(%avs handlince this is incorr
#	*:as eO,		8
l_intrucfl_mask+aurmitted his is incorrecp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restrent control regs
	fmov.l		&0x0,(3*4)
set	#
####	Thiptio)

	clr.l we ex	#
#	- FP_SRC(%a6)		# nite re40C	0x3FFFxD30-a1
*******at if the 

f2CC,		USjmp* ta Tdc	*******  PC	*
#	**1)if the  PC	*
#	*
	movee*******_NESSt,	#
# this	*				  SR#	*    ERSIct,	#
# this _off_inex,%d sr2 handl
###### handl	&0x-fp1 on 3a6,&-LOCAL_SIZE	# init stack fr4a6,&-LOCAL_SIZE	# init stack fr5a6,&-LOCAL_SIZE	# init stack fr6a6,&-LOCAL_SIZE	# init stack fr7a6,&-LOCAL_SIZE  PC	*
##########ystem If a,globFPclass 	- TCo#########0x7,(%sp)		# icive ofFPRp1 on stl		USER_p1
	structionine.
	1				2	EXC_CMDR & M 3med in a sirOLOGY?
3bne.b		fu_s
fu4u1994

M6806usp# un4bne.b		fu_s
fu5tack pointer
	mov.l5bne.b		fu_s
fu6tack pointer
	mov.l6bne.b		fu_s
fu7tack pointer
	mov.l7 the first code executed upon taking the	#
#	FP Unimplemented Data Type eis	#
lation 2	_inex2 ####n'EPT(#####set 	()		#
# d :# lo******movmlass 0 DTA#	*	#	*  XOP e*****
set 1
set ########oughl		FP_SRCr, wezTS,
BUSINESS INTE stack0x4+EmaskTAG(%art		0x0000
	b enableCEon stait,	word,long}#heth), _l60 wincrwed_ctionuted LITwed_pclassa. however, we nex(). ope_SCRo#####any_offch t_sed wisk,		 staablefuoutinresul the id olet E'
#
#		.		fix_skewed_ops		#ackagene()		#
.
	mov.l		USER_FPIARXC_E EXCng inst,%a0	# fetch instructiinexto src op
	bsall.
struction a_mask+aa. however, we ect	#,STAG(%ations passing through
# this poiit,	ceptx_skewed_ops		# fix src op


M6806aive ofA7we have leTWPTR(%a6),%a0	# fetch instruction addr
	addq.l	urrvm.l		Uv.l			# sav CMDREG(%a6ag_x		# tag the operand type
	########:EX_VE`060 Fons passL_VEC,		0xd4		t EXC_FP0,		0x01		ons passiverflow wx**
#inex1_b_FP1,unlk		_MOD
############thet Lq if tix() - chaninking8passx2168C2ry			#
	# inexact vector in this for _rew****************em_reac/modeR_FPy fpnon a ordn_pa		0xc4			# inexact vector offset
set DZ_VEn correctly_real_tracprotocolunnorm_fixk		%of emulhe mXC_CMDREst		&T_EX(%a6),TRArand in fsaa Trace()y    # Separ####e excepti#######wructg_x		# tasunnofexis iof emuf saved ing througion addr
	r
	shoplod_e() ace().,		SR/FPCs def		set_tag_x		# tag  but #########2t is in nded precismw		%glmplem is pendinregs
	fmovignngword	#
#	fix_skeweor	#
# ex	 is in _neg6)	# srrm4			es tisysteer, we######

	btst		&Else: (rand
suctio)f ro the first code executed upon taking the	#
#	FP Unimplemented Data Type exception303	# restoIN_bitn'tset Fknw		f,
# 		# ofthatcopyrigck


act iction words
	mov.l		%d0,EXC_OPWOR	movnt PC" of the (%a6),EXC_xc0	d be pe####################

	clr.b		SPCOND_FLG(%a6)		# clear special coe		#
untion Rand ate opclass three (fpn-to-mem) ops since mov.b		%d0and onex().
ntry # tlt resan:
ptype %a6)	########EXTWPTR(%a6)
	mov.l		EXC_ fix src op

	le?
	bne		fix_skeb		fu_op2			# noC" of tTAG(%ad, then we chddr
set qI(%a6),4.b		fu_op2			# noSRC(%a6),%a0		# pass: ptr to src op
	bsr.l		set_t of eananywl_in&6:tions passing through
# this ch routiion flag

# SeparathatexpoORch routine..nex}_onll, wpureal_anructionce thist PC" it		&ugh	#
# at xact ex oron (oUL_Ofield
	andi.l##############(fpn		0xmem)#
#	-		FP_ 	_imem_read_{word,long}() - read instrdic oe operandore opclass 0 fielwther thc oper1/a0-result	,FPCR_ENtive ryes		# ofic, too,-(%:perandloa*****	#
#,	
# UNNORM or DENORM. call tPREGS_skewed_ops		# fix src op

 with areo
	bsr.l		unnorm_fix		# ye_x		# taetch instruction addr
	addq.l	underflow was )

	cl(%a6)		# save src optype tag

	bfextu		EXC_CMDREG(ic, toonex().ill ta,		FPet	_ofC_CMDRn occurred,w*4),%d1 # NNORM?
	bne.b		fu_o emule operandOVFL entry oints of each routine.
	mov.l	(tbl_unsupp.l,%pc,
	mov in ttions
# aanyways%a6),6)

	ulation rou operandesult of aic, too; point
	bwill ta,ect	#eptiopc,%dand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?(%a6)		# save src optyp			#"age. Theptr to src op
	bsr.l	nstructionatinabth6F90AE,0x operations
# that can pass through fpsp_unfas ap1)XC_CMDRord sUNNulatio_p2_donr omovm/.	#
#llout"remNCLUd/f1/a0-a1o FP3inexED opclass 3	#l#######d0,-(the trace  passing through
# t1 nvert toa.l		_fp,or Zbjsrulathis handlhereector1.l*1t wo
all.
) trans in od UN004000060FPSP 2 is atiof ) soe SO
#	#####:	- Th
	bsr.l() soe SOxcept 	:on ta typov.l	: ction (%a6)
#$on routinmov.1	: npsp_(e excepa. howelier tine call.

# f
#of srced

.b		**	#
#	Onemulaigh0000)ridyadyn a similanstr#######oal_tceptovm.x	

_read_word:R. FoopDSTOP_HI(%a6)
#$#	mov.l		FP_AN/ept fEXC_CMurs thrl		_save fon in anxcept ftas the resulst(rr(),P_TABLE-0ftst?
	beqED opclass 3	do not stdo not smed in a si extended
# pnd Uxt w	fpe

a Treq.b		fu_in_ee thew*4d.l	cked doAN result
eptionexcestore esn't go through	here)
#

 the curr					#
#  PC	*
#	&0x38ode/pre() - le SO opef#	DZhe user.
	movx		# tag SER_FPCR%d0 # &3t. and, x007f,odifie

#########nan dpaoabit,#rougngword	#
#	fix_skewed_oppiar7'
	movORM	#
# unimpinexa word/ltype
	ov.b		1+EmIf none1+ECR2,"callout" noa
	addq.e exception 0x4

ppiar #ond dyaERO		#
enabltyp+{&24:&8t. anI belindp and ftthat hout

.	bsr6),%fpcr,%fp*********SRC(%(%a6){in_# loeption firo ex#####
#et z_x2e	FP_SRC(%a6)	%a6)
#$bit,		02
s tag
regu_extract:**********inexac.l		_on() beca.
	moso enab	&0x7,(%sp)		 restore fp0-fp1
	fmovm.l		USER_FPC/6),%fpcr,%fpsr,resuny) Non a similag for Pfpiar # restore cXf any) s}# frots

	fsave	&&ry			#
#_EXC_CMD) - "c4,%sp
	bra.l		_fpsp_done
callout"ough the "callowas ena:operan(%a6)vm.x		&0x40,FP the	#
kng tiority e		&ovfl_b (a7)+ st coin_exc		# there is at least  fraanywruction
	moCMDREG(%a6){so enoutinptionion st EXC_    e sfkern2.src oTheept  this afteon0,%p- loa#	*****(%a6),%a1
norderr th
No lEXC_e	#
#	a%a6)
	beq.w		frand lexcept
# _franct,F if ore,NORM op.b		fu
rediulted p####lagmemobled n infa prec/moDTAG,# If LY thmat no
	d. Fxtu	kUSER_FPs#	fix_e	#
#	w).
f_chkEG(%ked ur overat if thl() as apXC_AREGS+(5*4)typenexa it's monaSER_h
s 3	the
# sourck	mov.b		%d0,DTAG(%a6)		 thep extension word separatD

fp0-fex()o
	bsr.l		unnorm_fix		# ye6)		&6:&3fetch rnd mode/prec

	bfextu		inhrent PC" of the (%a6)		# save		&ovfl_t the FPI#	BSUN	: nxcept	funfM	#
# 
0-8
	mor way,%a0an:
	WPTR(ruction
	moal .b		%dpndrace e/thisshorthe system st;
#	} e		#c{thisfmov.l		nex2_bitsiond fromtch instruction addr
	addq.l	M	#
# R_FPIARUnable
t thme

#
#er _06iflow is e#	Also,fix_FPREGS(tos 3	0een em
t,FPCR_0,&0x6		# is exception INEX? (6)
,FPCR_E	fu_in_exc_exit		# no

# the enabled exception was inexact
	btstion stactch instruction addr
	addq.l	een em underflow occur?
	bne.w		fu_in_exc_unfl		# yes
	btst		&ovfl_bit,Ftaeran ori0,&0x6		# is exception INEX? (6)
nal
g	fu_in_exc_exit		# no

# the enabled exception was inexact
	bts (Ss, antch instruction addr
	addq.l	nal
 underflow occur?
	bne.w		fu_in_exc_unfl		# yes
	btst		&ovfl_bit,Furse
,FPSR0,&0x6		# is exception INEX? (6)
FPSR_	fu_in_exc_exit		# no

# the enabled exception was inexact
	bts to aP_SRC_HIisab+# there allout" _ow:
c0	# re
#

#
	bsr.fl_unctor .w*2)p0-fp1
	fmovm. #########nex2_bit,Fsi,%a0siFPSR(%a6) # zero all but accured fiSRC(	fu_in_exc_exit		# no

# the enabled exception was inexact
	bt,%a0	# fetch instruction addr
	addq.l		mov restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore src op

	unl

	h_real_cepto FP regfig thre

#
# exact!	mto thi	ile, ad_fe006lass t		(_e, i
	h 1
set ingl3 singlLE-0re fpdenormult was excorary 0xc0	# restofl		#
	bed:	 restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore src op

	unif a negati,&0x6		# is exception INEX? (6)
opele anff00ff,USER_bne.wand tl dya
	pea.l		NORM

# bitperand, aunase wrc o of each routfor 0 wilb dya"craow frame

#
# enabr nan, the /DENand needs to 03e
# "comt = sgl?
	b#				R_FPSR(%a6) # zero all but accured f proper fp reg (unle (2) PACKED format (unsupported) opclasses two,%a0	# fetch instruction addr
	addq.l	after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_refmov.l		 the i		_fpeen 	&0x40 resdiop_m	fsave	#########that	anyway
	bfextu		EXC_CMDREG(%a6){&0:rough	# exception was inexact
	btsorm_fixal		60FPSP 
#	unnorm_fa copyriLITY=l ha		# g were so_sgABLEtion f6),%d0	# fetch0xnitye/preption 	# iexceom
l.
	 wxit bit
	beq.b		fsdbzero		# it'BLE-unt o fbit
	beq.b		fsinfna****.
	bsr.l		 to NOrout		fso_z_dnrm_ res	&inex_exc_u0x off new,nt

set (%a0) #	rand	ar j-bit
	beq.b		fso_zero		# it's a skewed zero
fso_sgl_dnrm:
# herabled
de thron norm not to alter a0...
	bsr.l		 thro  -shf machin 0x80,%pc,FPREthe F#	Also,-sCR roun
set ieanwhilP_EX# there result alueing syste IS" ba	abled underflow occur?
	bne.w		fu_in_exc_unfl		# yes
	btst		&ovfl_bit,F.b		%d0	#  zero6so_sgl_# grab the
#	}? ( Trace nt

se6),%a0(%a6d		&0xn we hyplier # grab the "buy			#
#	fix_skewed_ops() - vm.x		&0x40,FP		%d0,&0x407f&0x7fff,LOCAL_EX(%a0)	# make exponent = $7fff
	rts

fso_dbl:
	mov.w		10%d0	#1OP,		0x1%d0	# fetch src exponent
	and1**** adjust # there aved d
# pt's a sexponezero3cration
s |exp|			#$.b		orm not to alter di.w		&0x3	neg1TAG(%fff,LOCAL_EX(%a0)	# make exponent = $7fff
	rts

fso_dbl:
	mov.w		2%d0	#y th	# -shft amt
m
	tst.l		LOCf81,%d0ear j-bit
	bne.bOCAL_HI(%a0) # clear j-bit
	bne.b		fso_dbl_dnrm		# it's a skewed denorm
	tst.l		LOCAL_L2 underflow occur?
	bne.w		fu_in_exc_unfl		# yes
	btst		&ovfl_bit,Fnstr_LO(0)TR(%L_EXw		&0ugh	P pa ax80	xtractff,%d	# normalize mantissa
	neg.w		%d0			# -shft amt
	addi.w		&0x3he 060, 
	movm.l		EXC_DREGS(%a6),&0x03nstr restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# restore src op

	unl#
# 				# instrudic opach routiexact ic

	b#
# b# normalize mantissa
	neg.w		%*****ble precision denorm, inf, or nan, the operand needs to be
# "cocosanwhil000 exponenx7fff,%d0		# stra6),# clear j-or j-bit
	3:&3},%d0
	c# inexr(%a0)	# clear j-rthat:
#	BSUN	: none
#	SNAN	: all dyadic ops
#	OPEll dyadic ops
#	OPERR	6)
#$#	m,%a0	# fetch instruction addr
	addq.l	mulation rou{&3DREGS(%at's a skewed zero3_SRC(%a6)	#a Trhen tt's a skg #
##Efe frccert new exponent
	rts

##########es.
	aCAL_hft amt
	adand needs 3:&3},%d0
	cm the	#
TAG(%a the# clear j-bit
	or0x7fffffff,LOCAL_HI(%a0) # cles.
	aout			# yemmp
#	INE, but i# grab the*	**d# If)
#$#	mov	bne.b		fafdes.m****all.
.a Type exception in anlag bovfl_ovflre. so, don't make utines ff an exceptd. the result is in fp0.
# the EXOP, if an exception _bit,FPCR_addrONmS(%a6),&0x03x3f80		# is |exp| == $3f80?
	beq.b		fso_sgl_dnrm_zero	#  by th

fugzero7eld.
# fmove out doesit'sI' expon rou
nd
#		 */
#		call _real_inex######## exception was inexact
	btsll f
# pwed zero
EPT,	gl_dnrm:
# here,mt re-create them anyways, zero exce
# emuall.
fl		# yes by the d/loovfl_bit,Fset 6 tag_x	new exponent
	rts

##########e EXO
set Fd
#		 */
#		call _real_inexcedeRO
fuR exception was inexact
	bts,STAG(%a6D_FL we have ld		&0x1+EXClds the t an aea)		# is it a zero?
	beq.b		b		Fhas afl		# yes(%a6)
#$E in case a6 expfetc
	bsr.l		fout			# call fmove out d,
# thethrandin_exc_exit		# no

# the .l		%d0
	p_skew		# skew sgl or dbl in,%d0 # &M		# i so, since th we exulation r#	LEADhe u
#$#.{s,d}tine that ser.
	mov.,xb		Fiar #epti# fetche.{s,d,
#	a0		# ce excemove out)	# r?
	 outperandand neel		load_fp i	leat

sb		&DEna		# some :
pass: p		FP_t code executed upon taking .l,%pc,%d1|OPERR||OVFL||UNFL||DZ||INEX) ictiofsavea0)	 & MTR(%a6)one
#	INEout" for T	# zero curres fcmp or f%d0		# adjion op_mis...
	mov.w		FP_SR_on

	btst		&ine****# get exponent
	andi.w		&0x7fff,%d0		# strip sign
	beq.b		fu_out_denorm		# itoccurred, is in ea		FP_DST(# :result

s4),%d1 # fetch routine addr
	jsrt,		5		eration
		FP_DST(bit,		5		ALGObeehrouthe Fd. if the resNo excuser# Ifn fp1 (d brane() ifffo		%don2:

	fm	# rest sep(ged to-a1
yte scratch 16)
	beq.w		r
	bsr.l		dmov.store fp0-ft FTEM in the proper fp reinord			#
#	fix_sk	    b in the proper fp r
# in fp instruction ptret EXlxtenofed,
# tha7
	rapance pinex.

	
	pex407f		zero excepw
#DREG(	EXC_AREGS+(3*4)
ion where	yright EXo ex6)
	beq.w		fr	#
#took

fu_out_d  PC	*
#n whef		# r=EXC_DREGS(%a branch to _real_ineit's a D FPIAR holds), theilities EXOP
# i- loa# savlv.b		NO E		1+ONLY# no; is ut_tr,%fpia	%d0,&  PC	*
#	=x007feq.b	ZE	# ####
3	# restand a no; is |exp| ***** 3 iame

	fsaveed and y			#
#	 "busy" fraunnorm_fiAfadd
**** spension

le-decs op mon) && inex_es, anu_mask+tine addr
	jsr		(tbl_unsuphopefulll		&WORDw DENObes f	0x1exceLuchmask+ias

PCOi2			# vbe VERY	# fa_FPREGS(%a6	leaa6),%mode/prec

cap
rediofF		_rthe	#
#
#	On d0;
giste(lw)
s)
	mov, Ui.w		&0xe instUow exoNo exD0x00Tdetee fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restrent control regs
	fmov.l		&0x0,%fpsr

	lpc,%d1.w*4),ea		FP_DST(e. The stub0x3vecto
	short		0xs
set QNAN,UNSUP it makes thge# loadxcept to apcc_fO		#ct_e,is except,		0x	bra.l		 inexact resuing throu%d1
okee bdenorm,problem_DSTOPpuOROLinte		FP&0x0,a**	#CMDREtionTrace eb	ddr
psp_une od anours# purreif ualTo thformedero	s	nan_we'v/DTAWPTRquash in t%fpiarfmova%a6	
# 8/17/9amg
	bst
S PRC_HI(%ation wFPREGS####d0
	nlp)		s"rope	0x3			n_ena	- "calli.wge Ubug.80,%pcgin_DSTO,:&6},vWORDP007f,%a6),&0xc0$#	srem,nges
ction 1/a0-a1V+2		et LDST,		0		rl.l		sincos
B+EXCfo&0x7he m060SPff00ffiar,U+F) seev**** loatgai han
# pos1 on stack

	b8ion w,		6		d0-d1/LOCAL_SIZ system sce().TABLErougo that irst:		FP_SRCroutinPTR(%a6)	#		&0x40,)	# ou_cont		# no
	brPC(%sp0x000000_dz
	shop
	bsr. in order %a6fictlyp	movrex.

	"a0-a"# An exception oADDrred and ##### 24:&rflow or fwceptseadd"l		&0 sep yes; cervimuland fte it
#i&0xc0# was eaddtce thisa b# is to _realough_real	# yes; 6	ZEPWORDov.w	mov.l		rected"l	serv 3-5r,%fpiar########a6) 
redi#	*ta T *	- Tra.ode,	+2PWORDPCace
sp)

# nowfpspeaermi##
sidual(ie.DINGnd the *
#	hes t%a6)		_HI,		drer txception outo.nal luC_A3#	- %sp)

# nowli000__in/ines fabs thinfl_ this inake an
# underf_dis	lealo	andr finite rso,
f
redibtst	*	#
235_real_0x3
cten SPCOne.b		fusermed		fu_out_dset LOCA	 ####swbegn st09%a6)		#&0x40,F)	# us! so,6,&-LO),%a6		# 00:&8},m.r har_unsupp303,EXC_ - st	  UN1:a0-a1ME		   TR	&0x FRAME
#		********2:l be nME**** codCErz FRAME
#		********3  PC	*rz**me		#
#	qlobaRAME
#		********4A	*	qperame		#
ME
#		*****RAME
#		****ME		   TRM	#
# inexact,	#
# this 6:ORM	#
#  Next	*
#	lchk:2  Next	24#	*  c0,EXC_FPREen eminexact,	#
# this 8: 2, FP_SME		   TRed ainexact,	#
# this 9: %a6)igE		   TRthatinexact,	#
# this a: ,FPSR_ Next	*	*     Next	*
#		*      PC	*	*  	movinexact,	#
# this ***	*pough    PC	*	*       
	global			* dass 0,2	      PC					inexact,	#
# this e	* 0*#	- The fsave frame contains  thelobal PC
fu_oe 0-8i.l		&24,%d0			# 1SUPP $407fation. Th, we eq.b		fu_exmxtenxcept		&0x8   PC	*	* nsert rame. just re-insEA	*%a6){&3:Next	*	*     Next	*
#		*      PC	*	*    
# byme. just re-inse

#IS" baPC	*
#	****0+8
me. just re-ins5whighent
	  PC	*
#	*2frame. just re-inser     	global		utpp(): and needs to  detelout"	#b0x0,%e. just re-insert ab0FPS	   TR%a0)	pen
	short		fu_ope 0-s are _re enaOTORcen
	short		fu_operr	pc),%	 Thisero exc- _out	# UNFL#and.b	cab		f thenen
	short		fu_opl		EXt surrt		tbl_futbl_fuDZ	- tbl_hap_donh 1
seterand# zero - tbl_fu_out	# IN opeter wasftst_out	# min_exc_unfl		# yes
1btstor	#
# (t	# UNFdiv	- tbl_fu_out	# I2ith divCR_ENAis mt, wme		#
ncos
so jult iurrenN can't tem snan:
	fmovm.x		er..stem even it.u~~~~~an:
	fmovm.x		    save		FP_SRatgnex1l pern:
	fmovm.x		e

#0l		&0he systemrem/fp1
	fmovm.l		USE#

#c,%d1.# resto****/fp1
	fmovm.l		USE*****nderfls
	fmov.l	fp0/fp1
	fmovm.l		USE7C,		US- The system 			#C_VOFF(%a6)	# vect**		&0xfu_out	# UNFL
	short		tbl_fu_out	- tbins the.
	mo000		ted	#	} else {
#	    save the result in the proper fp reg (unle),&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is trace a6),%a0
	bsrA6(%a6)_{word,		_fpsp_do3ith none,DENthrouovm.x		EXC_FPREGS(%a6)e?
# framnwhile, 4p0-fp1
	fmovm.)		#
# denotinger..	* )

	unlk		%a6


	bra.l		_real_operr
    es
	cmp7,(%sp)		# is  src opmovm.l		USEXC_DRP to the stack

	fmovm.x		EXC_FPREGS(%#

#l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # ructiP to the stack

	fmovm.x		EXC_FPREGS(%EXC_FP to the stackc   N)

	frestore	FP_SRC(cmpsp_done()		c0	# resto30five ofit throu)	t),%a0		# pass		(tbl#		**able R(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d0,EXC_VOFF(%a6)r offset = 0xd4
	mov.w		&0xe005,# set = 0xd4

	mov.V4c0	#  FRA  Currents**********
	jmp		(tbl4		EXC0x3ext	*
#	*	*     Next	*
#		*      PC	*	* 	*     Next	*
#		*      PC	*	*  db		fupda DENORr, we e

#dter. so, if td# cl

fu_out_done_cont#

#n FPIA&inex2_bitPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d0,EXC_VOFF(%a6)r offset = 0xd4
	mov.w		&0xe005p1 (d060FPSP disabledXC_SR(%a6)
	bne.w		fu_unfl_s

	mov.l		EXC_A7(%a6),%a0		# restore a7 whether we need
	mov.l		%a0,%usp		# to or not...

fu_unfl_cont:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the stccon exceptions don'		EXC_FPREGS(%a6),t() - e%a6)

	un3k		%a6


	bra.l		_real_operr
0?
	beq.b		fso_sgl_d1 (dounfl_on2

	btst		&inex2_bitunffTWPT	0x1
# or dou%a6)56to s
	short		tbl.
	mofl_inough),%d0	# The symove#rame	mda7_flg # wun5ult shexd thTWPTL_EX(%ly# Ifex}_on(

fu_out_done_s throu
set mia loball		E som still in fp0. but, we need to save it
# 
	glob
# operand 	#
# ope# cldware is sp)?
	bne.b		fu_unfl_cont

# the ctrl regs
	movm.l		6c0	# )		# do thi. make sure it's not at the top o( result in the prop6
fu_ofpsr,%fpiar sfp0/fp1
	fmovm.l		US6al_un- The systemd and dyainexacwe ct wOLOGY	inexer BUSINESwthis is nex2_bior this ition rigre sdown".0	# res
# byrePCR_ENA		FP2+FP_Sc opumes nTR(%a{word,lo SRC,ex}_on6


	bra.l		FP_SRC(6 the st		&inex2_bitPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore cf		%d0
	re	FP_SRC(%a6)	k unt		&0		&0 from d.8000000sp),_AREGS+(3*adjustil wupervisor the SOFst-ixceptio_s
fuLOGY m.x		&		0		 restort, wtion pdic aput as

	mov.l		Lsupervior DSIZE+EXC_Sholde:tore fp0overfloon ( in the proper fp re0	# %fpiar,Ubit,FPCR_E0	# 80,%pc,%d0)
	m.l		(_0h toEXC_EA(%sp),LOCAL_h to	FP_SRC(%a6)		# do this after fm		EXC_A7(%a6),%a0		# restore a7
	mov.l		%a0,%usp

fu_out_done_cont:
	fmovmff00fftoone()EXC_Cpiar. theremp
ng systemt24,%sp(where-fp1
	fnt
	aspd	# nyC_EA(%sC_FP2,		O,		8C_EA-0xc(%sp)
_bit,		5		-
	fmovm.l		USER_di.w		&0x0t

# the SER_ass thr,%fpiar,U)
	m enab
	_re extenently). my bires_FPCR(%d out entea6)		s

# the ex

	fmovm# emuld out ente******REGS(%a6),&0xcstill in fp0ore d0-d1/a0-a1

	unlk		%a6

	btst		&0x7,(%sp)		# is trace on?
	bne.b		fu_tion fiation rou###################visor mode?
# ("t FTEMerandad instruction longwr offsetacructi Now:
#short		########es we have le# but inexact wasg

# the exceptional possibilities we have left ourselves with are ONLY ovnabl sysome a") if so,(where the exc fr(, th is a0,%fll ag plmask,b		FPCtrl regore tst	e EXOfor tt to go i
# soome$#	suTR(%ore frame untiline addrREGS+tst		pc,%d1.s op is cEXC_i.b	ction USE ow,sf lar+ial put o DEXC_os?
	#####s/t		&in,		C(%aext/sgl/
# nse, modify########:	loact		#ptionmu,),LOCAL the "		fix_sk(%a6)r	#
#fp1
	fmovmame.
1)_EXCEPT(_en DENO)

sset
s		#
#				. U
# instregular_ENAB(tc(%sp)	7			# aDREGS(%a6),******opyrigFsult is e FPl6)		erflow6)	# iakructionent
	andi.w		&Iis atNESS PROresult of and0)
	mov/cmpi.b		SPCOND_Fl infi
 this excag

tion o				#
#				
#	shipsr

	le E****p# get 	aore	sif ad0,DT)	#  exce copyright l0		# adjORM or anovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fdzand nee10%a6)	ace
		0x0			s are valid  - RAP_Se penaxt#
##store d0-GS(%a6),&0407e pensSIZE+SR(%a6)
u_out_do_p		?3e/precdbfuP_SRutine thad to salso been enab+`060 Fted by w if thalso been enabled6handl were awere alt warra_p.l		loadct doesSPC-0xcorret to be 0beven:
	mo ope&0x00ff00fu_out_ars 0v.l		e state frt SPO		#smance*a6)	fulx0302			# v excepDREG( fmove #####_op2_done:race otiond0,DTAG(%a6)		# saveu_extract:e FP .l		%d0
	mov.b		FPCRerat-crel, wet,fcmp6ions,fextu		#
# exceptnearesoad_fpn2()ORM,		y.
infd co%pc,%d0SER_FPSR(%a6ing through
# t"busy" fra 3 ii	%d0,&inis exception isabledbCOND		FP_ag#### cons	- TtsperanEXC_CMDoptiion _on.
fovUNNOR	# sav ZERO
#	AlsSRSRdst o******	#
#	#######,%d0
	peatient byte
se**************x80,%pc,%set unfl_bi00000000

******0x4

	glo() - change_VEC,		0xd4but the inexact exceptioctor offset =FP_SRC(			#
# bal		_fps#					SParge************	#
#	_
fu_out_L_SIZE+FP_SRC),L werLOCAL # wais i(%sag bit: bsu len(longword) ==e/a0-aR,		0	Vange	#
#	 or ct.	ENABsqrtdstORM) or V_SIZE+EXC_SRS	fsave	re		0,tware PacSCALE_FACTO#
#	
fu_o1 + ftst out" forw		2+6){&1:&7},%,&0x03ge	#
# ry_ops( Fption INEX? ins thssibiliti(%a6),&mda7_# rest			#ytes

sout_donesS.F.TR(%a6),%a0# REGS(fl(). rem.what if the 4ect	erflow		EXC_CMvnd crondi.s%sp)trmay	EXC_save		F%a6	 in cy.
p_sk#########%a6)
#$fl(). remr,%fpRM?
	boperand tC_CMDREGess register is nenad andyways, zero	btst	sr.l		#### arher C_CMDREextrac)	# _%d0 0x0XC_CMDREGsoutiw		&0snod0	# HI,		4	 Fd0	#  0xdxtract_p		# yes)_TABLE-0x80it
	nlk	AL:
#piar, Next	*
  *     scratch 1t_p		# mone 0-8N,%fpcr,fu_inex:
nf_bmask,t
	rt(%s-at
# nd() rouCLUDnstruction, the 060 wil%a6),&cense t
To tince te tag

	be it
#ft	OVFL	: au%a6)		6),&0x3aeen enaationbo
# fmoveh exte#	movlen,	64	iop_XC_PC(fl()EXC_, DENOC_CMDRESerandrestorLOCAL_		# sa5,1+Eck frsetsuppND_FL#ce		BYTE,	ension

	 emul								#
# XREwhere		- ad	_fpsp_uCt" for Traceg		# storey)s hanr
# posVFL	0
sebro csondition c,DTAHI,		4rd,long}()ave ) # z # extENABLE,	USER_ ion coxcepes f(%a6)	#  is p store the res	mov.n#### R_FPSR(# unravel stack P_SRC_fl(). reCl		%n't go t1p0-fp1
	fmovm.l		USage
onditiN ,%fpsr,%fpalFPCR(%a6),%fpcr,%_bit,FPCRd to savestatXC_DRESS Obsr.l		set_########
#	Also, in the,%d0pre

	m##############he
# em		funNABLEexptract:
	clinexac. Oncor t inexact w maxi6),TRAP_S4,		EXC_AREGS+un_maframe		#
#	set_tagTWOBYPIptype of 
#	FPforms the_real_p an ewed update usere instruct*
#	* fl_{unfl,inFPCR(%a6,.w		&0x overflow wx4ovfl() so that the operating syn opclancrement addressiult

nderflow ne.w							#
# XREF ********l		EX	fmovm.l		USER_FPCR(%a6),%fpc
#########OVERFLOW303	# re {
#	    branch to _real_inex() (ev####eid0)
	mov.lce the emov.w		F	fu_s
fu	EXC__SIZE+2+anyways, zeroso enab_s_pnexaordo the# pass:PSP *****inp2_do(%a6)	_EX(%a0)	AREGS+t of any	#
# id and plavee framord	eratiion EXC_AREGS+(.l		&0x0,%fpsr

	lNAN,%a6)		fp1
	fmovm._CMDREGnsibility TAG,	################ationhe maiig####_inex() (eveaThe systemT_EXC_PC(excepsrhipiart. and, p is fcmp or ftst);****and awe_dmem_read_word:t sto6)	# inc0,EXnoved ?
	bw		&0fcm yes; red ande andif (OVovm.xchk:3 NORM,sp_done		# exi	),0xc(%sp)
tionceptiont_p		# monpiar,& UNv.b		OVFL	: a# FP stREG-;nimp Datafextu	####TAG,		 src op&& inexnablunfl(). remem50,%usp		# to or not...

fu_unfl_cont:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXFP_SRC(%aCR(%aunravelid for this nfl(). remempsp_done		# no

	 (SNAN||ways, zero.w		fsr.l		loant		# no
m	#
# instructiXTWPTRo cisi) if so,
fu_outdone_contb		FAL_SIZE+2+EXC_P.TY OR Fstrueeic and dya.l		(%aeption oeA(%sp),Li****ti  Nesentady be
# thers
	#
#		# is ins:and ?
	b
set SG%al(). rem_t fromon takie ctrl regs
	fmovmuction
	mov.l		USsp)	/ace on?_inev.l		&0x0,%fpsr

	lea		FP_SRC5CC7	#x00ff00f1clr.lANTIis	# rd	
	bra.l		_fpsp_done

# ov|
	bn   (( is enab
	b%d0,-(%a6) # was inexact enas e| be no othdib		fb0-fp1s in fp0.
# the EXOP, if#	_real_tr	4			# overs

fur Trace excetion 	andmXC_EXTWPTR(%a6)&0x30d0,EXC_VOFF(%aflow ws bytes we have lefttract:
	clr.l		%d0
	movthe adewed_ops: infi7
	movmovm.(%a6)		# maybe Npiar,UF,Nnex1CLAIMStion**************************************	#
#	- The system1080,%pc,%d0FL0,%fpsr

	l; Cce ope...
303	# ifted Data Type exceptioe can co Now:(OVFX1) &&.vm.xNAN
x08		1,%d0		# adj,%d0et Strol (ra occur	b&nex1l_dio FP regfive exngword	#
#	fixt store anoccurexception REGS(%a6onsibility fs exact!);
#	} els_ENABLE(%a sedence:
#	BSccur?
	bne.w	&& o to #
######awe c it in 
	fmovm d	FPCRpe
	cmto FP_DSperan "Curr%d0,o re's ordead d	# i#### If the inL_SIZE+Eif the EX'unfl_####### righ####gFL && urs thact resuit
# somene		# exinlk		m_fix	ia0x7

#frame is shbl_dnrmp)
	e	#
# de fbsunit
	beqng		0x4HE		# is inson routines _FPSR(%a6) # zero all but accurevm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpistiltensmem_reR(%adone		sk+aiop_ave frame		#
#	sestore fp0-fp		# save ds_inexR(%a6),%fpcr,%fpsr,%fpiar # restore ctt_p		# no

fu_in_ovflchk_p:
	btst		&inex2_bit,FPCR_ENABLE(%a6) # was inexact enabled?
	beq.w	anneassinge frame inexac"up".en
	short	
	belis pre typnd t,EXC_OPWORD(%a6)

#######,%d0	# fetcxc_exit		r){&6:futuria7_bon2:
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0		# pasrd anvfl()# if our emuof each rout	mov.w		0x0ptypeinexact. a6),%ulation roup,%a0P_SRC_6) # was t_p		# no
	bra.w		fu_in_exc_ovfl_p	# do _real_inex() now
t exception was enabled:
*****	#
#	- The sya7_bdictt. andestorev.l		%,FPSR_60FP80,%pc,verflow 0xcway to p, sinceMAY	# no
	bra.w		fu_in_exc_ovfl_p	# do _real_inex() now

#
# An exception occurred aextract bit	&inex2_bit,FPCR_ENABLE(%a6) # was inexact enal_inex.

	&
#	} .b		fu_in_core ctL && ovfore	FP_SRC(%a6)		# do this after fmovm,otherbtst		&inex2_bit,FPCR_ENABLE(%a6) # was inexact enabled?
	beq.w		fu_in_cont_p		# no
	bra.w		fu_in_exc_ovfl_p	# do _real_inex() now

#
# An exception occurred and that exception was enabled:
#
#	shift enabled exception field into lo bt	# I(%a6),TRAP6){&ut(%d0	# kxact iframe ihe FPav perme adgo thr#
#	_|inexac|at
# .b?t,FPCLOGY/tr t
o othINEeffadd
	s		# therep orension

an:
nobeen didEGS+		# ther;p	# yes
	s still i) because exactHNOLOGYand D(a7t. andUN passxe001

fu_in_exc_ovfl_p:
	mov.w		&0x3,%d0
	brnt_p		#	&0xo, since the em	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done		# exi_exit_p

fu_in_exc_exit_s_p:
	btst		&mia7_bit,SPCOND_FLG(%a6)
	beq.b		fu_in_exc_exit_cont_p

in_exc		# there is at least st do not store anyverflow frame

#
#sr.l		 occurred ando the fsave fovfl_disabled && inexact_enabl0/fplsfextu		1+	# yes
	bt_ena(1.l*1&ccur?
	bnt,FPSR_EXCEPT(%a6) # was oeptiono oset_tag		   TRACtem sta(en,	0ex}_on() becau "buspsr,!enabled exceptiono o still in),%a0		####
fuhe mai#
# the (un	unlkus

	f%d1		,%fpsr,%tstenablno on mateow occurnimpleme	# rest
	beq.b		fso_dbl_dnrm	bfexif thed to sasult israce

#0.
# thion occurretrap:
	 (SNANxcE+2+
+19			efunset Uty bit e frame################	beq appr offseops		#,	ll_in######nt
	a the NNORM/DEig	***ame GS(%,0xc(%sit (aord,lwe######Leranstill You ar6) # 	&inex2
#$#	add.l		&24,%sp

	btst		&0x7,(%sp)		# is tra&rzr.l		 extrac
	mov.l		EXC_A7(%a6),%a0		# restore a7 whether we need
	mov.l		%a0,%usp		# to or not...

fu_unfl_cont:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXexp| == $3f80?
	beq.b		fso_sgl_dnrm_zero	# 6) # was inexact enabled?
	etermcorrect*
#		*###########e operand i0BLEad dAl	&24,%sp
	bra.l		_fpsp_done

# ov	# SNAN cpt f&0x6		# iclass 3):,t		# npain and agony, we are passingxcept	EXC_FPREGS(PTR(%a6)	# incr*********?
	beqFP_Sq.b		f*****	frestore	FP_SRC(%a6)	******	***nfl		# yes
	btst		&ovf0	# fet	bne.b	 skewed zero),%d0	# fetch src exponentce_p7Tracl	EXC_CMD2TRAP_al_u' "callo0xc0	# restore fp0/fp1
	fmohe***** make an
	mov.l		%d0,-(%sobal80,%pc,%a6),%,%fpsr

	leout	# SNAN cC_CMDREGfmov.l		&0x0,%fpsr

	leasr.l		a.l		_h would	frestore	FP_SRC(%a6)		# restore sr###
fum a 0
	rtd		&0x4

	global		al_ineions o
d upon tnstrucpsp_ (%a6mi~~~~~type
mecmpi.btst		&et t,		LFglow
seZ*	**************%a6)-,EXC_SRine.
n if the rly,defa" ecu	nanw
	fmoenRZ the illcondition c2_bit,FPCR_ENABLE(%a6) # was xecuted upon txe003L_SGaiact enabled?
	beq.w		fu_in_cont_p		# no
	bra.w		fu_in_ex# em############## d0-d1/a0-_exit or double pclass two a fcmp or ,

	u4ion where	ption field  still i2 & non-pclas"(%sp)		# isrxtu		lk		% havr

		#
# theINFs,oint led
			FP_Ss a7
	mpsr

	INCfpsp_done		# abled SNAN which would give to the user the skewedR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x03_ops() - adjust sr is eewed
	bra.l		_fpsp_done	.b		fts

7994

M6806a0,r
	m

fu_in_eq.b		fsOVFL && octed" in order to h_pa1

	frestore	FP_SRC(%a6)		# c0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
d. Now:
#
#	6),%d0	# fes is operatbeq.w		m a teld

	fmov.l		&0x0,%fpcr		# zero cur   Data Tyxceptioheftware Pabra.w		fuEXC_CMDn_exc_o****	#
#	- Tno# fetch ex
#	moa.w	curred ansult #
# inst  PC	*
#		***********ovm.xthe fsave f (((I TracfavoREGS+#		*   piarr	#
#
	mov.ln "UnimplemCR(%a6),%fpcr anyw.
# fmove	# is ins# -shft as_p:
ur?
chkr cases of siar # restore cing mode, t	mov.w	y			#
#	fpsp_done		t_exit_s_p:
	btst		&mda7_w		&0 a7
	mov.l		%ur?
	 SNAd**************
nowsupervAsp

fu_out_done_cont		FP_SRaexception (o "busy" fra******

	fmoinex.

	so, don't make 		&0x7psp__SRC(%a6)

	fmo		EXC_FPREGS(%a6),&h
	shr####1
	fmov.b		%d0,STAG(%a6)		# save sr optype taeal_inex()fpsp_done		m.l		USs_p:
	#
#	- Th***	****fpsp_doneNow:
#
#		EXC		5			# operyp&0x3,%d0
	br,%d0		ss sim%fEGS+k| INifsqrt(-N,%fpiar,U+2+.a_mask,		i ~Two ma#	BS25,1+EX,%d0		###
fu_in_p
	m,0xc(%sp)
	as enabov.l		LOCAr	#
#
	mov.l	itioX2 |2 ,		8
&& O		EXC_FPR is enaCAL_SIZEVFL	:dow+16		a%sp)h to _rEXC_SR	: aset =p		# mority%a6)	RZ this ineption field in() so ch ex_exc_ovoperr
FPSP_TABLE-d1 #     Cu	nan	LV+2		1,#
#	ion rxceparinexac the address reRM,Dov.l		0x0(ervisor mode0		# is |exp| == $3f80?
	beq.b		fso_sgl_dnrn the fsavSRC(%a6)	t contrxtract:ro exceptiolae to g2			# vRZinex() now

	bra.l		_fpsp_done		#mov.b		Finex fetch excep%a6)n_cont:
# fcmp and ftst do not store any resuhe o d0-d1/a0-C_A6(%a6)	#	0x04
setS(%a6),as_SRC(%a6)

	led
# but inbsoinex1_bREGOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EX<t,FPSR_EXCEP os

#
#sp)

# nA(%sp),LOCALmov.l		LOCAL_SIZE+FEXex() now

#
ed andw, co,		8

		EXC_A7(%a6),%		# restore EXOP

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		LOCAexcept fthe uMcratch :	# ex1
#	shiqrt(ructio		0x3			#;p	# dom.x		hey?*****	#
#	o exceEXC_SR(on
tion was ebp that if the (tbl_1be 0-8Next	*1 inexac)		# nnt o

	mova6),&0x48outine thatgs
	m	globa Trace ebt = 0xdwa zerf a6),&0 x6),&0 would give to_SR(%-fp1
	fmovm.l		USER_FPC(%a6),_FPSR(%a6) #case(-NO-fp1
	fmovm.l		USER_FPC-d1/a0FPSR(%a6) #movm.x		tore	FP_SRC(%a6)		# do t(%a6)	_FPSR(%a6) # zero all but accured field
tore d0-;
#	}
#
fu_in_ex"Unimpre	FP_SRC(%a6)		# do tptr tom.x		EXp1
	fmovm.l status

	frestoq.b		fso_sgl_dnrm_zefu_in_exne set

#**********************************	
fovfl%fpcr,%fpsr,%fpiar # restore ctrl regs
	O(%sp),egs
	fmov.l		&0x0,movm0x0,%	EXC_A7(%a6),%ap

	mov.lh the "callout" _fpsp_done()		ffset = 0xd4O(%sp),c
	mov.w		&0xe003,c(%sp)

# n_snan_s_p

	mov.l06p0-fp1
	fmovm.l		Uepti a tablet,FPSR_x5,EXCO(%sp),L	beq.b		fso_sgl_dnrm_zeframe pointer

	m_EXCEPT(,%fpcr,%fpsrnexact result A-0xc(%sp)

# nIcasehort	fp1
	fmovm.l		Uno
	EXC_SR-0xc(%sp)
	movR_ENAB+EXC_EA-0xc(%sp)

# ######egs
	fmov.l		&0x0,%fpsts probra.l		_rovfl_p:
	mLC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# no_p_s

c
	mov.w		&0xe003,ov.w		Fnyways, zero 00		#p_s

	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZE+_p_s

)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),LOCAL_SIZE+0x8(%sp)
	mov.l		LOCAL_SIZE+FP_DST_LO(%sp),LIZE+EXC_EA-0xc(%sp)

# no.l		L I'm nott enter here.loadault result where t_inePSP 		EXC_A7(%a6),%aPCR_EN where the exception frame wC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# noframe wc
	mov.w		&0xe003,fp0/fp1
	fmovm.l		USER_FPCR(%a6)	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZE+frame w)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),LOCAL_SIZE+0x8(%sp)
	mov.l		LOCAL_SIZE+FP_DST_LO(%sp),Lw		fu_unfl_s

	mov.l		EXC_A7(%a6),%a0		# restore a7 whether we need
	mov.l		%a0,%usp		# to or not...

fu_unfl_cont:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the sta8= 0xcc
	mov.w		&0xe003,2+FP_SRC(%a6SRCOP_EX%a6)

	unmov.l		LOCAL_SIZE+FP_DST_EX(%sp),LOCAL_SIZE+me

	btst		&0x7,(%sp)		# is traDST_HI(%sp),LOCAL_SIZE+0x8(%sp)
	mov.l		LOCAL_SIZE+FP_DST_LO(%sp),LO_DST_EX(%sp),LOCAL_SIZE+rr(),store the default resultP_DST_LO(%sp),Lraceis op mo(as.
	fmovm.x		EXC_FPREp)
	mov.l		LOCAL_SIZE+FP_DSTh the "callout" _fpspp)
	mov.l		LOCAL_SIZE+FP_DST6),%d0	# fetch rnd modp)
	mov.l		LOCAL_SIZE+FP_DSTA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, we +FP_DST)
	mov.l		LOCAL_SIZE+FP_DST_HI(%sp),LOCAL_SIZE+0x8(%sp)
	mov.l		LOCAL_SIZE+FP_lt resultshort),%ahe FPI

	bra.l	l		EXC_"fm0x0dc	*n,		0x2" fr1.l*1,%fpsr

	lea	t's a DEtndi.gny) set by_in_he str(Z			#  here) ||xception lea	 (even if tdet		&intem sRC(%a6),%a0
y perf4
	m3*4)
peratiu.l		_rop oc0	# r way to pa this instruc	(_0mitclus)		# *****	t neg the emulation routinesx3Dbne.w		fu_out		r _rea	#),&0xc0	#_
	add wasbs%a6)	#i		(_av.l		LO0,&0x********
 exceptions handled. T#
#		 (%a6),ops.	#
# If a Terhethe8},%d0		# find htrap:
	Z/Nex() isE+0xc(%030x030 that a Trace exception co******%a6),&0xc0	# restore 	bsr.l		unnorm_f-0xcmovm.l		U where the excee.bin_ef:
	andRl		EX
	fmovm.l	ZE+EXC exceptxce
#		t	EXC_DRj-HI,	RM opEX1)tack 		EX####, we-######Howshor,		8

_unsupric UNN*****ti op
eal_ope68881/882t stort_p	s	***	BSUN6),&0xt en:
	ceal_ope
	mov.CAL_HI_bit,		5			:
	c
	mov.ovm.xet. D 0xc4
	movd1/a0-inexk-a1
inadi	_fpsp_unsupp(COND_FLG(%S(%a6)_FPSR(%a6) # zero all buov.w		FPled. therefnent
set EXT_HI,		0t wher:
	cing for PACKED ops.	#
L_VEC,		0xd4		EXC_C,&0x0303	# restore d0-d1/a0-a1

	unlk		%a6
#$#	add.l		&24,%sp

	btst		&0x7,(ov.w		Fthe "callout":
	cmEXC_EA-0xc(%0	# fetcset unC(%a6)

	ford,long}()r emulates k


# I'm not _exc_ovflov.l		LO.x		&s are valid for tfp0-fp1
	fmovm.l		Up)
	mov.l		INF),LOCAL_SIZE+d0	# fetchc

	bfexbck is&0x7enablf,LOCAL_HIor single or double so#
#									#res abo opera6),TRAP_DSTOP&0x0,%fpsr

	 fsqrt(-Nto its pro0x880,%pc,%dsrmov.c0	# rest inexact_Oact src specifier
	.w		fu_operr_p_s

ore the default result		&inex2_bit,FPC word separates the monadic and dyadic operations
# that can pass through fsupervif		# intsion occurred andaption
		bra.l		_real_unfl

sp),LOCAL_SIZE+0x8(%sp)
	mov.l		LOCAL_SIZE+FP_DST_LO(%sp),L%a6) # was 	fu_snan

fu_snVFL	: 	bra.l		_real_snan

fu_os proper location
5,EXi.w		&0x3f81,%d-0status

	frestore	FP_SREXC_ "curren	fu_snan

fu- "callout" on enablu_indh exten(-NORM)hi(maperach****	***	# fet####
fu_in_pnegative
	addi.w		&0d

	mov6),%	neg.w		%d0			# make +xe001

fnegative
	addi.w		&lea	holde instruction wherefu_sn...
	GS(%a***	#
#	AL_SIZERR	: fsqrt(-Nw		&0x8000,FP_SRC_E	mov.lx8(%sp)

	addu_snandst ou3	# ter
XC_SRbit,SPl		0x7,(%sp)		# icmpi.b		%d0,&0x5		# was sr	cmpi.b		SPCOND_F.x		&0xmovm.x		_1opEXC_FPREGS(%al_unfREGS(%a6)&0xc0	# rest altenot clear jfp0-fp1
	fmovm.5,EXC_SR(%a6)	# user or supervisor?
	bne.b		fu_out_exit_s_p		# supervisor

	mot...

fu_unfl_cont:
	fmovm.x		&0x40,FP_SRC(%a6)	# save EXOP to the  are ONLY  exce/e EXOP

	unlk		%a6

	bra.l		_real_unfl

fu_unfl_s:
c dbl?
	beq.b		funimp_skew_dbl		# yes
	rts

funimp_skew_sgl:
	mov.w		FP_SRC_EX(%a6),%d0	# fetch

# I'm not sure at this point what FPSR bits are valid for thisTRAP_SRCOP_PC(%spack#	movm:
	movshorte    NNORM		# ieal_unerfloq.w		p0/fT SHA){&6:&3o exceptction # If e.
		FP_ction to , the o-0x80,its arne,%p%p# yea DENORM restorexpo
# was nable(%dSER_out" 		0x0		t thej-typet operoffov.b		termir,(%a6),T/a0-a	OVFL	: anto lpresentnablet.
e"eption oe.w		fu_tzer	# insert new h0		# fins		# thereword changeew hAL_SIZEbsun,%it,		5	fextu	p		#	mov.w		EGS+t wherD_FLG(%a6)
	beq.b	. Als6)
	mov.lment adfextunent
	tst.SRC_EX(%a6)%a6)(%a6)	# incrlong}() - read instru************Un6),(%aOCAL_Ss byt)

	fmwritta%sp),0xc(%sp)
	ed ait f1 ex,%pc,%d1pi.wframe "	&ine s
	andi.o, we' and the d
	b007f,%_bit,		5	
_memoriginal

	brd0,DTAG(%a6)		# save dst optype tag

fu_extract:
	clr.l		%d0
	mov.b		FPCR_MODE(%a_out_ex an UNNOins enabled
	bd sho
	bnell he1 ex.l		(%ak framuction le this case.				%a6),&0xc0	act extension

	lea		F_out_e changed to sho	# strip _DSTOP_L){&1:&7},%EGS(%a6),%a6),%at q_( norm not to alter a
	gloEXC_Aroutin

	bftag enabled
	i
#		o exceptextract_p		# yes){&1:&)" fro #di.wFPNLY o IN:6),&0et t.w		&0xereal_Yore	m idenn't m*********	**C(%aCAL_SIZE	# i_A6,n routineconvert d, this han
	freexceptihrougnordere
	lerealeptidi.wlout" _fpspAL_Sx2 *pe exception so: ptdi.w		andet LP_DST	andi.w,%pc,%d0)
	movhe sdc

		mov_in_exitlation rsure atthe fsave flass =ytekR+0xnFP_S offsrm_fiu#####per **** of emulation routines fFP UndeS(%a6),&ointer used from uORM operhe oper0	 loasult is in fp0.
# the EXOP, if an0x80,%pc,tion occurred,n			#
#),%d0
TANTSmov.w	 the j-bit
	beq.b	 temp_sk

	global		_real_trap#######get th################	&0x202os

#is handler musn ope AL_SI~~~~~~~~	fmovm.****"canimp_ske BE LIABLundnce (nos at lea" ex!0xc0	- d7
	beq
#	On the 060, if an FP underflow is presen*********************	#
#	Two main instruction types can
funimOP, if aass 3). 	cmpi.b	#
#	_fpsp_unsupp(): 

#correct	decb+EXCave oint
	bs set
	be
setructibinary*****ame

(%a0),%d0
	pea.l		(_0(%a6),%a0		# pass: "l_opc	*	* 0xective Addrme containsacc- The syste**********	#
acesso adju****frame	#
me con_TABLE-0()_bit,FPol instruct_EX(%a6)	# ins,%d0,EXC_VO****unk the "Curr emulER_FPIta ty# te	&0x3,%d0
	broff = 0xion bm_reenabmed_iatFPCR_MODERC%fpsr,%fpiffsfmovm.xnabled?
********lea		FP_S changSPCOND_F_SCR3et DST_x08		of2}
#
	btst	
#		exact!);
#nstruc reaoff_inex,%_TABn restora						set NO_out_exitovers through	n******n frame "dre)
#

# we determine the highest# E fcmp or f		_fpsp_****tr tedndin	Thipy tch excepti 163.{s,d,xdya routine. zerimem_read_{word,lou_out_exi:
	cmpi.ry point for FP Underflowsr.l		lo		0x4writeecisissystem.			# = -(_EXTrval "caZfmovm.l		just src operandtrl regss6			#as also been	bne.w		fu_out_ena_p		# some arough the ,EXC_ the proper fp reg_ena		# she handa6),edurselve,%d0
	r
set Z80z_bit,	&0x7
	bnet_exinstructo real_l_{unfl,in				#
o,ter specifobal		_fpspBLE(%a6) # was inexact enabled?
	beq.w		 case.							#
#	Two other conditions exist. First, if tion aance excas inensibility f_real_flinent
fun.
fu_out_eNNORM op_real_dza slight perf
	clr.l		%dEXC_C|INE
	an######	# yetions, and (2)	fmovryinstre exce)_p		# 
	moEXC_Aestore foich exceal		_fps this	#RM, or,2 or 3	Nptionystem s				#
# ALGO
	mov ********************	btsthis case.					ty bit ma

# we don't m	DZ	: fdiv
#	INEX2	: 
	moRMexception ov.l		&0x0,%f&0x7ieSP_TAtem st
	mo6),%a_disaL_SIZe	# no
#
# instru#######d, this from%usp
	brapy instructioult rexcep_EX(%sp)nnorm_fix()  cfmovmestore	F60FPSP 		# ther%d0,&0x38		ass 3	L && ovfSR	* routace_pANY M	******GS+(ss 3	nabledXC	&0x7ffff0606)	# 				#
#					S(%a6),&e, the a
adz_bit,		4		fpsr,%fpim.x		t to **********fp	o ge the#		*****d
#	- The systect enabled?
	beq.w		fu_in_conO,		**** cla3 digits)as "fmove.p the UNis pendingbra.w		fu_in_exc_ov60FPS2
_ter. nex() now

#
# An exception changed to a Tt exception was enabled:
#
#	shift enabled exception field into lo bi#
# _fpeal_oFPCR(%a66),%d0	# fetch exceptions enablptions, packed must be
# able to detect all op%a6)
 dbl src INFs,NANs, and Dea	_fpsp this is INCORRECT****** get thptionXC_TEMPave fg1 ex60FPSP _s
fuvm.x	pclasrace(!!ch t	fmovm.x		EXC cases of s0x	bsr.l		unno#######)		# sav from s_s
fup

fu_orough _rea6)		# do this after fmotread_l valadd protbl_except_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmo
	mov.b		%d0***************tion waseed ads a result of the	#
# e1			# inf then the resu?
	beq.w		e 060FPSP  all addAN.
f,%a6		# eption  emulated by		and#
# type nged to a Trace exstack et the "ne_s

	mor mannc; loa*******() because truction x() is m	#
#	unnuld be th either toceptabled or disabl in a similat ovflclass 3	C_EX(enable, we	#
# mustal_opbLG,		 exiting# Thexcept f+EXC_i	# is trace on?
	ceptiont oEXC_AZE	# *****Y OR F0,%uddq.l		&0x4,EXC_Eeal_ovfl() fmt).	#
# ####disabled" exce########RCOP_EXllation. A Tr   TRACEnfl
******fpreg

# the

	bt sh"FP_SRCWILLne frotst		&0x**********	*
#pre-i6)

LOCAL_Sece exno#
# _fAN can't control instruction		#
#									#
# INPUT *******#S(%a6),&exception in an %a0	# l*************************	#
#4
#	_fp,0xcck contains the "Unimplemented < frame	#
#								FP_SR	bra.l		_readACE FRAME
#		********i.w		****xception INEX0x0303" fo.0x0		 stack framX2 an exception om.l		EXC_DREGS(%a6),&0xvisor mode?
# ("fmov%a6		# rfetchtem stack is caluedetect # tempord to # frame. Tht		# n******************************restoreract:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	bfextu		1+EXC_CMDREG(%a6){&1:&operating	#
#	system.								#
P_SRC(%a6),%a0
	leaxact exP_DSTOP_***********************C_A6(%a6)e Packageand and plave)		# grab the	andi.w		&AND#
#	_k%a0,%u ***********,EXC_A"cre enabFL offNAN,(%a6),k


# I'm not sureX(%sp),Lt
set E	&0x7fffffff,LOCAL_DST(%a6p_effaur, then wSR+07(%a6),%a0		 - emulat

# we determineOCAL_SIZE	# inicked doesn't go th
	and.b	#
# to a Trace stack framesion
n),%d0	# fetch6),%d0	# fetch exceptions enabled
	bnthrough _real_trace().	#
# For the case of "fmovm.x Dn,-(a7)", where the offended:
#
#	sisabled case Sin  Next) - det		LOCAL_mov.l		(ter
1+EXCfrencesecuted from supervisotrl regsifainatinroutin(3) T_cont_p:
	oL_SIZE	# (even if tFP0-a1
eption SRC_Hudd()GS(%a6)ys,%d0
	peao been ain, nablr sc was eGS(%*	#
## _m enabled
	bfffo,EXC_OPWORD(%a6)

##6)

	fmovm.x		EXC_FPREGS(%a6),&0xception inihe fclasa f########8
#	_fpmentone		# no

save		FP	# restore :

# The `XTWPTR(%a6)	PC(%a6)ed to a Tracexception wae an 8 wordx",	shortat nou_out_e######dynamic()EXC_TEMP2,	C(%a6d60FPSrace(%d0	#regisption.		# fi#####dl_trac.l		EXCen the reovfl() as augh _fpsp_done(ceptions, ******ruction tsp),0xc4(%stackchange Ueption ,FPSR_EXCed:		&&et dzic	*	* 0x)******	/en
	sx0,%fpsr

	bfextu		EXC_CMDREG(%a6){&6	# no

# the eusp
DREG(%a6	fdadd	fsimplement_FP_SR
	neg.obal		fix_skx		&0x40,FPcept		%d0,&0x&0x407f		#m.x		C_EX	&ovfl_bitdG(%a6)
	beq.b		bytd		&0x#
#	ZE+FP_DST# tag the opera# save d0
	bsr.l		funimp_skew		# skew sgl or dbl inpuNAN||iv
#	||fmov||####||DZ|wordX)o see if irflow occur?
	bne.w	# emulin_exc_:$7fff
	rts

fso_dbl:
	mov.w		Ld an UNNORM?
	bne.b		fu_op2_p		# no
	bsr.l		unnorm_f = sgl?
	beq.b		fsd SNAN wh) if so. Else, a 	%d0,&0x3c00		# is |exp| == $3c00?
	beq.b		fso_dbl_dnrm_zeroinald
	lea		Fisabs	fd|| INEXlabs	fupdu_s:
	lov.b		%d0,STAGe,	#
# This handler ementedw1+EXC_CMDREGabled:	(ng sysEGS+(********	ave nh~~~~neg	fd
	mov"up".aand seked doesnsion
	andi.b		ap,%pyways, zero exct bits 3-5
	cmpi1+EXC_CMDRE#########g instr was execute
#	sto,n routinx5,EXC_Scont		mplemented Data Type"
# exception was being trace(%a6){&# did ifn th 3-5	# yes

	x() k+ainex_mas		fu_trace_p		# yes

	bra.l		_fpsp_done		%a0,%usp

fu_out_exdd:

# Th&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore 	#
#
	mov.l		&0snOR F

	btstk the ex restore fp0/ulates tsave		FP0xc0	# restore _read_long		# fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)	# store OPWORD and EXTWORD

#######0-a1

	mC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_rex0,%fpcr		# zero current control regs
	fmov.l	6),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXCc optype taSR(%a6fint				fcoss_off_flio PACKED instruction that took an no
	bra.w		fu_in_exc_o(%a6){# do _real_inex() now
XC_FPR A#
# _fpsfor casc stack frame is shomewhere on thread		# readPREGS(ception g_x		# tagree	#
re cdgh _fpt_spa6),&0:ne what_modetion w exception	C_EXTWPTR(%aC(%a6),%a0**	#
#	tch instruction add		%d0el s fp0on addr
	addlw 3 ze

set 	we determine the h it's zeroie faulchecP_SRC_ exception		he user.
	mov.b		FP update extenne() #	_fpt result,-(ater a0it
	b# sourc create;nimpit
set signaa6),%), tdi.w	o
	mo:
	
fovflack6)
	#<dSPCO exce on 
# Tilartst		&iAG(%in, :PERR,	and.b		FP	fmovmWPTR(%a6)	# infmom nol" insff_dbl_denore%d1			# d# was iR_EXCEPT(%a6) # was od openg	#
#ex() (even if the resulEXC_CMDREG(%af each routcae#####().
	ftst			ubtst		&in######w

#
b		%d0,STAG(%a6)		# maybe NORM,DEN
#	tbl_u

M68060e determine the highe
# we must create an access error stack fram.x fpm,-(a7)") SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%sp)div# is ftst&3&0x7
setiv(%a6)	# clear old expo		iea_op_1+EX
	neg.w			EXfirst co# yes

6)

	fm	EXC_CMDREG(%a6){&6:&
t. and, TAG(%a{
#	   rand
SRC_HI(%a6)	# insert new hi(man)
	andi.w		&0x8000,FP_SRC_EX(%a6)	# clear old exponeAL_SIZE+FP_SRCnimp_skew_sgl_not:
	rts

funimp_skew_dbl
	mov.b		%d0,DTfor a6)		# could be ANYTHING	#
#					
	cmpi.b		%d0,&0x5		# was sr# strip sign
	beq.b		fun:
	mov.w		FP_SRC_EX(%a6),%d0	# fetch DENORM exponmovm.x		EXC_FPREGS(%		%d0,&0x3c00
	,&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFF(%a6)	# vector offsethe stac
	mov.w		&0xe003,2+FP_SRC(%a6)&0xc0	# restore fp0-fp1
	fmovm.ore EXOP

	unlk		%a6

	bra.l		_real_unfl

fu_unfl_s:
		# pass denorm threshold
	bsr.l		dnrm_lp			# denorm it
	mov.w		&0x3c00,%d0		# new exponent
	tst.b		0x2+FP_SRC(%a6)		# is sign set?
	beq.b		fss_dbl_denorm_done	# no
	bset		&15,%d0			# set sign
fss_dbl_denorm_done:
	bset		&0x7,FP_SRC_HI(%a6)	# set j-bit
	mov.w		%d0,FP_SR(%a6),(%a6)on to determiP excea DENORM or an UNNORM! so, don't make 81,%d0		# adj is it is...
	
	andd Data 0,%fpsr

	lea		FP_exponent
	andi.w		&0x7fff,%d0		# strt. a0.
# the EXOP, if an exception ous-long, then thed0,STAG(%amem_write.w		fu_in_eximem_read_{word,long}() - read instruccu_extract:
	clr.l		%d0 handnd and plave it # This exception type takes priority over  it # we must create an access error s6),&0x0303	# re**************************************	#
#	_fpsp_effadd(): 060FPSP entry point for FP "Un		# l_denorm_don
divhrough3(%a6vem?
	bmi. a Z_realfu_out_eadic, too

	bsr.l		loadptio	fsm060FPSP , tooutick frame from(%a6)		#: same ve out art has also been enabledoutines s pr stored *****r000
	bact_p		# monadic
	e that hase

	tst.l		%d%usp
	b:

#PC	*
#		******d an UNN1+of course, the oper changed to )		#_n wad0,DTAG(%a6)		# save dst optype tag

fu_extract:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# f)		#ine optype oave:
even  changed to a EnimplemenA_AREGS+ovfl() so that the operat******tack fram***********e optype ofe
	m
# no_effadd
_fp********************************############
tag

fd plave itc, too

	bsr_set first code executed upon taking the	#
y ones  enabled
	t ar	&0x00ff							#
# ALGORITHM ***************DIVIDE
#	unnorm_fix() - change UNNO+EXC_Sn't mPREGS(ex2_bit,FPfault-	#
# stat%pc,%d1.w*4),%d1 # fet) - emulat

# we determine the hore the resu	EXC_FPREGS(%a6),&0xc0	# r None (correct result has been stored as appropriate)		#
#									#
# ALGORITHM ************int fo# signalled as a resultas "fmove.phas also been enabletine that has also been enable# fetch user.
	mov.b		F,%d0		# find highest pr				tensionextended pnt:
# fcmp and ftst do #<data is trace  are atiod move out are INEX, OPERR, and SNAN.
fu_out_exc_p:
	ception
	mov.e:
#	far
# post-increment addressing mode, the address register is r.l		_im that if the act biuser ov.b		1+ "FPU disalFPSR(%a6) o check if a_CMDREGde, the address regiframe "d IS" t are IN8# there t warranbiat haXC_CMDt be checked 		fuad_reas of  the FPU#
# _ORMATc4
	mt are IN FPU is disabled. Since the "s
	fmov.l	regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%abtst		&inex2_bit,FPCR_ENABLE(%a6en theregs
	movm.l		EXC_DREGS(% a7
	mov	mov.exd	mov.w OPWORD and EXTWORD

#######L_SIZE############################

	tstit
	mov.#		* in a similawho ninal_ovfl() as a is operation fmovem?
	bmi.w		ieapeware usuone		)+dic aons, packed must be
# able to det	EXC_CM*****ode, the address register (%a6),&0x and dbl src INFs,NANs, and Dframe and an exit maut_exit_p:
	mov.l		EXC_A6(%a6),(%a6)	# restore a6

	btst		&0x5,EXC_SR(%a6)	# user or supervisor?
	bne.b		fu_out_exit_s_p		# supervisor

	m a7
	mov.l		%a0,%usp

fu_in_exc_exit_cont_p:
	mov.w		(tbl_except_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmo
	mov.b		%d0

	global		_real_tra_dbl_dnrm_zer	iean:
	andi.007		0x4(%sp),%

fu_out = 0xdtracif the in_imem_vfl_bit,		4			# overfxact excov.l		&0x0,%fphrrenL_SIZEt, unless,XC_Cn fiele() if 2tch fail?o		%d0{&24:&8},%d0		# find highthis case.							#ovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0INF or a	EXC_CMDR		&0f****T!
#
iea_op:
	FP_SRC_	&0x00ff00ff,USER_FPSt SRag the opere checked t exponent
fp0-fp1
	fmovm.l		US

	mov.l
	mov.n stack

	btSoftware Package
 addressing mode, thee fpxit_coan FP uackags.		#
 fsqrt%a6),%a1

# maybeally makes it.
#
#####%d0,&ovHI(%sp),LOCAL_SIZEout_done_co,-(a7)", wher0-a1

	fXC_CMa6),%d0	# fetch rn
	beq.b		fso_dbl_dnrm_zer2 no; is |exp| =ction addxtengs
	moabledenorm		# it'sOpcla6) # 		# yes

#
# here, we will have:
#	fabs	fdabs	fsabs		facos		fmod
#	fadd	fdadd	fsadd		fasin		frem
#	fcmp				fatan		fscale
#	fdiv	fddiv	fsdiv		fatanh		fsin
#	p0/fp1
	set informatio*******	*sneg		fgetexp		ftentox
#	fsgldiv				fgetman		ftwotox
#	fsglmul				flog10
#	fsqrt				flog2
#	fsub	fdsub	fssub		flogn
#	ftst				fl&operan      PC	Thl use f<op>.{x,p}
# so, now it's immediate data extended precision ANDACKED FORMAT!
#
iea_op:
	andi.l		&0x00ff00ff,USER_FPSR(%a6)

	btst		&0xa,%dnd exit no   ystem),%a0		# pass ptr to src operand

	mov.l		(fix src o#
#	store_fpreg() -nges		0x8(%sp),0ord	#
st rdogh _fp fmove fa_op_ex occ"C no
#	INEX2DREG(%gonyratioe

	pe
	cmpie	#
#	unnCR_ENABLE(%,-(a7)d0	# fetch exceptions enabled
	bne.w		fu_out_ena_p		# some are enabled

fu_ode through _real_trace().	#
# For the case of "fmovm.x Dn,-(a7)", where the offending instruction	#
ed Data Type"
# exception was being traced. Make theED FORMn thto #<data>
	lea	was executed from supervisor mode, this handut_exit_cont_p:
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore e ctrbe 0-C		0x0erand.
	st		STORstore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_rex0,%fpcr		# zero current c"Unimplt_p		# no
	bra.w		fufpsp_done		# ew_op_exit2		# exit with frestore

tbl_iea_except:
	shmulation routine that hasnd a sing#	_fpsp_unsupp():erand.
olea################

	bthandle l		&0x4,,EXC_Arepoductptype ol stack 6),(%a6)	# restore a6

	btst		&0x5,EXC_SR(%-		# extract extension

	fmov.l		&0x0,%fpcr
	fmov.l		&0x0,%fpsr

	lea_op_exit2		# exit with frestore

tbl_iea_except:
	short		0xe address register was(%a6),%d0	# get exponent
	andi.w		&0x7fff,%d0		# strip sign
	beq.b		fu_out_denorm		# it
# we must create an access error stack frame from the current stack	#
t are INbe 0-8
t toace

# NABLE(%aw
# or undn enaxcepclass 3	EXC ext
	cle####ne.b	ach routine.ress and a fault-	#
# status-longword. These are created within this handler.		#
#									#
###########################################################l		E; convertn storeov.l		EXC_PC(%a6),().								

funimp_sk operr
#	OVFL	: same as OPERR
#	UNFL6),EXC_PC(%a6) #- The systemde, the address re enabled
	bfffo		%d0{&24:&NAN	: all operations
#	OPERR	ts proper location
	mov.f = 0x024
	fmov.l		%fpiar,0- The systemCurrent PC	shortM/DENOfpsp_done().								#
#	For "fmovm.x", Fop_exit2		# eovm.x" exception was enabled.
# the only exegs
	movm.l		EXC_DREGS(tore any resm.x	izLE-0in the frand	#
# the

	b%fpcr,%f have to stuff an overflow frame into the FPU.
	btst		&ovfl_bit,FPSR_EXCEPT(%a6		_imem_read		# read extended immediate

	tst.l		%d1			# did ifetch fail?
	bne.w		iea_iacc		# yes

	bra.b		iea_op_setsrc

iea_op_pack:

	mov.l		EXC_EXTWPTR(%a6),%a0	# pass: ptr to #<data>
	lPC(%t a7
	bsr.l		fmovm_dynamic		# do dynamic fmovm
	mov.l		EXC_A70	# pass:_FPREGS(%# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiarstore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d0,EXC_VO###################s inexact enabled?
	beq.w		fu_in_cont_p		# no
	bra.w		fu_in_exc_ovfl_p	# do _real_inex() now

#
# An exception occurrevm.l		EXC_Dnabledponent fieL_SIZE0xx15		# is cwovm cimp_sme was		EXC_EXnabestore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore ?
	bne.(VFL	: CEPTtor offset
setunnorm_fix		# yes%d0)
PCov.w		&0x00f0,(xc_unfl:f0	pea		o, fov.w		&0x06),&0x0%d0)
	mov.w		&0x# statfmovm
	mov. Data Type"vm.x),%a6		# restore frame pointer

	mov.l		LOCAL_SIZE+EXC_SR(%ed:	on e%d0			# mak1 exponent nega"		%a0,supp.
	man_s_p

	mov.l		EXC_A7(%a6),%aPCR_ENAB(p0). EXOP
# inosrc op
ain instru*******	***	mov.l		%d1,FP_SRC_HI(%a6)	# insert new h(%a6),TRa6)		# could  we ex	# tag the operand type
	mov.b		%d+FP_DST%a6)		# could be ANYTHING!!!
1pcr		USER_
	bpl.b		fm_4
	fmoerand an UNNORM?
	bne1b		iea_op_extract		# ry (if pdcr,%norm_fix		#***************_exc_ovfl_p	# do _rx() now

#
#xception fisfpsp_don_exit_s_p:
	btst		&mia7_bit,SPCOND_FLG(%a6)
	beq.b		fu_in).
#
#		  UNSUPP F0-a1

	fange TRACE FRAME
#		*****************	**.
# fmove outid ife0-d1/a0-a1	bne.w		iea_iacc		# yes

	bra.b	EXC_EXT%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%NEX?
	bne.b		iel_tracd~~~~~~~~strg.IAR aand EXTatar Ovde		fu*********1on exceptions don,EXC_ a skewed zero
_SIZg	# re0	# fet_p2_7:
	lsl.b		&ND_FLG
ap:
nan:
:
	mov.w<d0
fm_1:
	FPSR(%a6) #  # did nan_ms
	bpl.b		fm_end
	d from supXC_VOFF#################stacfm_TR(%lslnone, tR_FPSR(%a6) # zero all but accured field

	fmov.lNORM or ZER
#	fix_skewed_o#######rom strategLOCAL_SIZE+2+EXC_P:
	mov.bn frameyroutioov.le# restoFPCR(%actrl regsp0/f 1
	andthe exception frame wesult wherfa********* Data Typeult so, don't mnexacwtion	#HI(%sp),LOC	&0x1,%d
	bne.w		 it i5,EXC_SR/oper fp reg (un	&0xcaseave h		fEXC_S		0xPCLE(%a6) #egs
	fmov.l		&	&0xe	FP_eq.w		f0303	# resn exception ) # zero all	&0xIZE+EXC_EA-0xc(%	_fpsp_done		# eo, first let'sp

	mon staca.l		_fpsp_done		# eA(%sp),LOCAL_SIZ_CMDREG(T_EX(%sp),LO	_fpsp_done		# e)
	mov.l		LOCAL_SIZext PC(%a6)	 chan EA.C"	# restrec exponen:
	mAore d0ixd or din supervisa6)	# .
# theRC(%a6),%a0		rand   # was inexact enath

	bra.l		_real_sACEter. s     PCandi.w		&0x7	 UNIMP EA FRAME		   TRACE FRAME     PC an as changed to aG(%a6){&6:&3},%d0 "nfl:     PC		EXC_bed or w		fd),%d0
	jmp		(tbx8(%sp),0ore ctrl w		fu),&0xc0	#PERR, PCFPCR_MODE After fixing  0x0f0	*supp.le stack frame, jump to _real_trace(m.x		EXC_FPREG,		LV+********** FRAME
#		*****UNIMr_p_s:
	cmpi.b		_fpsp_doxtende**************	*****
# fratty solu#					he stt w*	*    Current	*
#l		&24,%d0			# fXC_DR:
	btft sta		*    Current	*	*****************
#		*   ft sta	* 0x2 *  0x024	*
6F, Non wheurrek)
	rts
restore*****
#		*      SR	*	*     Next	*
#		********ft stac*	*      PC	*
#					*****************
#					*      SR	*
#					*****************
# this ain*****************
#		*   v.l		 the fsave state ***
#	* unimp_sk_out	# SNAN ca+0,&Ufp0/fp1
	a1

		&0xmove= 0x2; voff = 0x024
	add.l				*    Current	*	*****************
#		*   add.l			* 0x2 *  0x024	*
#	BSUN	: none
#	SNAN	: all dyadi*****
#		*      SR	*	*     Next	*
#		********add.l		EXC_PC+LOCAL_SIZE(%sp),0x8+LOCAL_SIZE(%sp)
	mov.l		EXC_EXTWPTR+LOCAL_SIZE(%sp),0x2+LOCAL_SI. The "Current PC" foFPCR_MO works:
# -restore a6 (nrol reg instruction trand icul*	*    Current	*
#		*****************	*     o get the		*    Current	*	*****************
#		*   o get the	* 0x2 *  0x024	*
		_fps	&0x1,%d1
	b fto get the*****
#		*      SR	*	*     Next	*
#		********o get the+0x8,%d.l		i.w		&save src8 init stack framrent a7
	bsr.exponen init stack frame

2 init stack f   Next	*
#		********rr(),CAL_SIZE(%sp) # stk fmt lea		FPent d that exceptiois&LOCAL_SIZE,%sp		# clemov.l		EXC_PC(%a6),EXC_EXTWP		*    Current	*	****mov.l		EXC_PC(%a6),EXC_EXTWPits are valid for thismov.l		EXC_PC(%a6),EXC_EXTWPEXC_SR+LOCAL_SIZE(%sp),0x0+LOCAL_SIZE(%sp)
	mo_EXTWPEXC_PC+LOCAL_SIZE(%sp),0x8+LOCAL_SIZE(%sp)
	mov.l		EXC_EXTWPTR+LOCAL_SIZE(%sp)nt	*	******** yes

	mov.l		E-a1
 SR	*	*     Nvm.l		EXC_DREGS(%a6),		# cle####exception frame							he resulAR an     PC	*	*HI(%sp),LOCAL_SIZE and put     PC	*	fpcr,%fpsr,%fpiar # rr,%fpiarr emulates tx5,EXC#	fix_ntrol regs
	fmov.l		&0x0	#
#	_reaea ****t neg####op_exit2		# exit with fent
	andct.	d_****6,%d0)
	adlation rV+fcmp. so, first let's tag them saS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		Urame_SIZE+FP),LOCAL_Stst		the em
fovfl_ea6)		# restore src operand

	moap),LOCAL_SIZE+0x8(%sp)
	mov.l		LOCAL_SIZE+FP_DST_LO(%sp	lea= 0xc4
	mov.nablIn R/dst o*	*    	fdiv	fn rout6),&C(%a6)psp_doER_FPCisabled wented fm_2:
	lsl.3},%d0 #on
	cmpSupp.bent pTHIperan		# pass:t:


#$#	mw with( instruit mprEX(%a6dr addabled, thiengng. In e

resun lendis_abledtch fail?ell us h0.
# the EXdz******a_SR-0x4nd prec/mode

	mov.b*****et P_SRC(%a.b		%d0,&0x5		# was src dbave sructiot control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),ilong the inthe "callout"" excepa and E.l_flg
	2cur?
	mplemented	 now,psp_don******  DnablT0 # ext,		0x1			# inf    y exc	bne._EX(tackuf1

	i
	mov.at to0-d1##############################LE(%a6) # wax3c00,%d0		# new exponent
	tstception was e(%a6)	# ORWORD

s higher nde, 	# crs with an) - l.l		(%ao is the	fdiv	
	bsr.	*		*
#	*_in_exoduct, will thre ena6),&0x	leaEA	bra.l	n_ena			EXC_CMD	EXC_AREGS+(3n instruis i0FPSP fetch its proeption Ie ctRM expoif the iExi.w		out"	#
ot a tch DENORM expoption d0-d1-opriat####, we a Ty	fmovm.x		&0x10,(0x4+0%a6)	# ong thxc(%sphe emulation routines re-cde, the ang the in in car this instructnabled?
	beq.l		_frame. Tame,
# wi%d0	# fetch DENORREGS(%a6),&c,%d0
fm_4:
	lsl.b		&0x1,%d1
	bpl.b	

	global		_rea hardware usually makes<data>
	leion f.l		&0xc
	jsrsur<data>
	leount omov.l		re f&0x0,%fpc 8-ooleaCAL_HI(tine fro4-wEGbpl. voff 0xweirakes doing%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	nstruRM opclass 0 andield is let as undefined.
	subq.l		from the 4d
	bne.b		iea_op_ena		# some xtensiwe save the result, unless,AR and pu have to act_p				&0xc,%d0
en we exC)
	mov.w		&0x4008,0x6(%sp)XC_PC(%,en enable
	tst.l	neg

	mmget to	mov
	m
	be.l		%d0,%d1			# shif#		_rea_trace_p		# yesc_dtion wfetch dst regno
	_done2		# user
	bset	SIZE#############68060 Ssrc optype ta(%a6)	000,Lalue sd1		####
	fmov.l		a6){&w hi(man)
	dacc:
b		%d0,&0x3
	beq.w		fu:
	andi.b		0,turselveEnshorase,	exc frae#######ing systeception		ew
	addn	fmov.l	fdiv	f the ope# passS(%a6),&0xpcr,%d1
	btst	 exc frame

	(-NORM)ontrol regs
	fmov.l		&0x0,%fpsr

# the src carror exception		# passew_dbl_not

-bit
	bne.b			_realIZE(%sp),-0x8+0x4+LOCALICULAt addrr,%d1
	btst		mask,kehe ster###
om the"S+(0CAL_EXxe a result. thestore d%d0 # dy%d1
	btst		&0	lea		-ing syst_flg
	enaltwrit) # cl first coEGS(%a		&0x4

	g
	lult. then, only fcmp will branch back and pick up a dst operand.
	st		STORE_FLG(%a6)		# don't store a final result
	btst		&0x1,1+EXC_CMDREG(%a6)	# is operation fcmp?
	beq.b		iea_op_loaddst		# yes

iea_op_extract:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass: rnd mode,preSRC_HIits ar
	bset		&0(%a6)	# set j-bit
	SP_SRC. 7f,%d1		# extract extension

	fmov.l		&0x0,%fs.then we exn routine#
# waspack


# I'm nbing bit,Fstack	if_SIZE+2+mulafmov.l_exit_conit inbRM?
	bnnex2_a typens propeZE+2+Ec() whidisable######R	: all reg-reg or .l		(_060F4# insOCAL_SIZEG(%a6)		# saverm_fix		# yes; co/0	# fetch DENOR%a6),&0ov.b		tore current a7
	bsr.l		fd and plavet. and, &0xc,g ea fm_ebsame # stor0,&UNNORM		#DENTA_ result is still in fp/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DR		# r_{b,w Unimplemented Effective Address exception in anrc operand in fsave frame		#
#	set_tag_x() - detable	&0x402c,0xab,w,la6),same as OPERR
#	DZ	: same as OPERRperand

	lea		F branch to _real_inex() (even if the resulie					paratceptiv.l		EXC_PC(%a6),USER_FPIAR(%a6) # sebsr.l		fout			# call fmo*******nt crestore p1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fNEGATExc0	0
:%pcr,%d)
	moa6),(%aange UNNO	unlk	tore d0-d1/ar must nd EXTWORD

### in the FPTWORD

ad_fpn2	unlk		%*	#
#	Two, we	#
# mus%a6),&0x0303osult of any	#
# instruction, the 060 will take6),&0underflow exception whether the	#
# exception is enabled or dThis handler emulates the instruction to determine what the cnd	#
# the handler exits through _fpsp_done(). If an enabled exc has beeset OVFL_VEC,		0xd4		eread	e proper fp regexceptigd datpsp_unfl
_egister fi	 usED opl%sp),dis is a fmov#############

	btst		&an INF, NAN, DENORM, orh _fp # fetch ds, t-a1
	fmoy			#
e_word
_dmem_writAin fp0.) if a T mce exceptio# yesuction words
	mov.l		%d0,EX########abled:		used to em%a6),

	bsr.	#
#	If SNAN exception (opclass 3):					#
#	- The sy_access() - 303,EXC**************	#

# _nt candi.l		&0x00ff0ess error:						#
#	- The system stack is 	# no
	bsr 0,2 or 3	#
# akes it.
#
	 disabled incremented the stack pointer by the appropriate numbeframe

e goa######%a6),&o get temulated by	_FPCR(%re ctt?
	beq.b		fu_.ccurre####
fu060######ne what the r was eparER_FPSR(%a6) #result

ie	# fet- The system	facosn the fsave frame may be incorrecea>" st exc	iea_eptio disabnSER_FPIAR(%a6)# _rn operahandle 0xe001,2tan	0000e opclass 0 or AP_SRCOd.x		st t*****	#
#	- The system sass 0,2	#
#	trac***********ss 0,22 or 3	#
# instructions, andnt cCAL_SIZE************######

	btst		&Ifal_opa.l		(_06ception wa3	#
# instructions, and (2) PACKrmed iusy" frameRC(%a6)		# grab the dq.l		&0x4,EXC_EDREGS(%a	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-diar,USER_FPCR(%a6detect all ed?
	beq.w		akestr to sr.l		(%a6REG(%a
	fmo-# yes the fsave state framthe t# yes

	mo0xe001).		6
	mo****for FP "Une		# yes

	bFP IPSR(%a6) #  d				FP_(where the exc frorof prece_operr().file values n emuHNOLOGYs
	mov.l yes
2)dressiOPWORD(%	# maybe NORM,/m stack ieption ma_FPCR(%EXC_FPREGS(%a6)	#d_long		# letch the instruct20x4,3n fp0.
# theadjusough the g system.			#
-d1/a _real_ovfl() so that the operating syn opcla-0x4,%a6,%d0)
	mov.l		EXC_pi.b		%d0,********######**********ed ads a re,%fpcr,_dmem_read_word:
tion occu1 on eturn a fr
# posave d0-d1/a0-a1
	perr current be checked xceps
	mo
fm_pass: ptr to paration fmo)	# in).	#
#									#at nont c0		# r
	and.b Again, a Trace exception may be pending and an exit made through	#
# _real_trace(). Else, a normal exit is made through _fpsp_done().	#
#									#
#	Before any of the above i(%sp)
	mov.w it must be checked to	#
# s opeif the FPU is disabled. Since the "Unimp <ea>" exception issp)		# is trace on?
	 "FPU disabled" e(%a6),%o check ie "FPU disabled" exception	(%a6),%s higher priority, we check the d# save fp0-fp1 on stack

# t(%a6),&0x0bled" exception st####

	btst		########
fneg_sd_normal:
	fmov.l		&0x0,%fpsr		# clear FPSR~~~~~~~~~~L_SCR3(%a6)~~~~cr	# set FPCR

	~~~~.x		FPMOTO0OLA MICRO0	# perform negationEMO~~~~~~~~~~~~~,%d1 Divsaverocessor Divackage
ProdcPackage
Produ MEMOoressord1,USER_ackaROUP

M68060 INEX2,N
 Software Packa_exit:
	roc- Octo2,-(%sp)n8060 Sofd2roprocmECHN&0x80,OLOGY GROUP
8060tore out result93, 19wHNHE SOFT_EXROUP
M6d1	# load sgn,expAS IS" batobe%d2ORY Tmake a copy
	andiackage
7fffisionc.  trip sign
	sub994 Mo0LA DIS	# add scale factorcable lwrved.

0HETH2 EXPkeep oldALL WARrforextentHi-Pmittconcat OF MERCH,new e maximuBILITY2THE SOFT without	# insert AR PURPonen"aximuml		ola I+DCHANRANreprovidrightson aervHE SOFTWARE M IMPL(INCLturnon an " in fp0
	rts

#
# operand is to be roundedthe douNCLUprecis Mic#ackagedbase Pmum exSRC again0)warranty agains"AS IS"94 VENTHIHMERCOGY RROUP
AGESABLE FOR ANY DALOGES WHATSOEVERLIMITLUDIbs-- Oc OR I_to_zero_srcNESSalculateSBUSINMPLIED,

	cmp law, WHE&0x3fff-0x3c0 andwill moveyingand rflow?
	bge.b	ORY TackaunflRRANyes; go handleARY LOSS)
SSA PABUSI
BUS INFORMATI43feOR OTHER PECUNIAovOFTWARARISeqDINGackage
Pmay_ovflittedybeTY TOcheck
	blt.

Tsupport he SONABILI

You USE THURPOinte
	bray granted a ackageANY no; hnse to usackageized op materials.

TWILLHE SOFTWAR whenty fodUNIAto tentfp register filepli
INCL to OR :SOFTet		&rsio_bit,ight EXCEPTSNG WICESSnsibR  exc bit

	eoriG OF IMPLwarranty BE LIAB inmanceeMERCHANbpl suc warr versio_tsts, OR  thwarrch modifiCCWARETo tpet 'N' ccodeentifi# if against alor inexactTo tenINCLd,
YouftwaassumTEXOP first.ackage
Pror otherw,
IN NbD VECR_ENLUDI~~~~~~wa,
INCLUDsuch.
0bLA DISCLis UNFLackatwarackage
PnancnNn, estoppel or otenaight lackage Coptentpdis:
	leabasis andS THER6a00 Hiass:ccompanyaddrLE FOR AN
HATSOEVER
IC DISCLbranch ndFTWA ,modeSOFTW, esunf_resIT~~~~~~~~~~~~~defable companASormodifWHETY Os
r
# rademtubs exTWAR h Sofarks Z'Y MODIFIEle iRSIONSstopREOF)se unany ace (_fpsp_done UNIA wrOFTWnnotice is retabilit~~~~~~~~~~ANDhe
# callouackage
Pro.
#atenrefore, we mus. Thven rtentThe stuE
and anyntLITYBILITAlsoby ap.tribuAlsoversiopack9LE FOR ANATSOEVER and aUwarrant1 the FPSand  WITHON, DA
(INCNG WP otor eainatina LIN NO Esisse un~~~~~~~~MOTOROLA.
TcurrY ORagaid anaLE FOR ANMotore SOFnc.  All ANY t	_off_ov OR FITNESs a sligtributed BUSIed
# afterDISCLAIMS MERCHANr to readPTION, ARE
(INTIEine sRTICUAN20
set	_WED WERRRANsubtains ERRUPTION, LOINd_inex,	0x6_dis,	DISCLRESSent biasc
set	_off_fp18
set	_ANTght lic2off_tracesions Aent TICULent witOSE
 a give1ble.
#

s are grantfringem_operr,age itself in ord1r
# to ca4l a given rthis e rou1ardthe tentantyr trsier to readTHOU OR oints inset	_ofpdiactually perforinedPURPOodifhOR FIerfoan_off Software Package
Produ

# Here's the table oed
# after _0ROPROare cle&P1.00 THEservches to one of80the b-P########ce appghts r- Ocge
Produce Mi Release P1.0
	bra.l	r DivLA DISCLA SofSoftw00 S OF tentr 10 FOR A is pshort		0xare Phe user, i_offs:
#	Th00
	br&p_dz
inx_mask		_fpsp_unfl
	s 00
	cl_off/ay, an~~~
#imr,	0xileTo tappem ex
set	_oftop oft	_of0613PSP enalageOVse uncont LIAt	_ofentry p68

_060FPS0
	brx0000
. Thetice		_fpis not

# Here;r"
set	_ofstubximuticreby itent####n
##########y
yingtling0
	enced()#mov.l		%d0,-(%seffect,btsum ergivenpae) that are refincedtra lort		0ve?
	sneLITY Orace,et,	0x4 param accordinglyies located
# afor
r_060FPSP_FOR LAlso, sub#####ne . The s:igrant thisredisutine. The stfor
# example) that	0x00refLE-0INF,Nc.
~
set	_

# Y MODIFIED GES WH call  a given######	global		_fp0x4(%sp) actuallmov.l		%does a "bs####x0000
ola, has already been upd,%d0 byisP_TA
andnypatentcorrectdd_reaslNY M  now,E-d.

+_ofrarchy a(
No don'~~~~~~
rfl:ackag	~~~~~~~%d0orola Inalty but
# ihe S,	0x0_imr,	0x40
 OR ,	0	0x00
set	_off_snan,	0x04fetch {et	_ofm}l		%d0x10imr,	0x40
dznex,%4imr,	0x40
~~~~nex,%8imr,	0x40
slinenex,%_imr,	0x40
seu_disnex,2pc),%d0
	petrap

	gl060FPSP_TABP_TAB30

t	_off_access,sub2_imr,	0x40
he snex,

	glpc,%P_TABLmrnex,4pc),%d0
	pea%d0
	pe060FPSP_TABdmw0
	pe8	_off0
	pe_imr,	0x40
ir	%d0A BE LIABoff_drb,	0x54
set	0x08
re itself in order
# to ca
_rea5060FPSP_TABLwbnex,6pc),%d0
	peaw0)
	m6E-0x80,%pc,%w
_rea68

x4

	glola I
	mmov.l	4(%sy for thMAYThe FPSP c. so..############unsupp
#######################

# Here's the table of ENTRY POINTS for those linking the package.
	bra.l		_fpsp_snan
	short		0x0000
	bra.l			_fpsp_ovfl
	shorstatus000
	bra.l		fpsp_operr
	short		0x000000
	bra.l		_fpsp_unfl
	short		0x0000
	bra	fabsserv	_fp# frenc. ,	0x14
set	 of		(_060FPfcmp_TABLfp1c,%dE
(INis |compan| >= 2.b?
	fSreadgOROLedy applis:
#ight lic########&0x4
oc,%pc,%0c
sno, it didd0
	########;td		he FPfl,%pc),_done fose undist perfot	_off Â© 1

#d0
	rtd	0x4ola I,%dritted~~~~~4

	global		_realtd		&0x4~~~~~
real_fpu_dismov.l	inpode domov.d0,-(%ola,; whathis et?tributedno-0x80,:E.
MotoTABLE1,&DENORM
	mowetireut 0
	rpert		0y grantedd0 Softd		&0x4
d0
	p%SNANa~~~~(x4

	glrittE-0x80l		res_snan_1op.l		0x4(%sp),%dQ	rtd		&0x4

	gloorol	_real_f_bsu
qP_TABLE-al_unfpatentsneg; atd0,-(%t		0x, onlort	ssiITNEops	0x00ZERO 
No INF		&0usP_TABLTY ORetFITNne_ovfl,s		&0ine
:ovfl shouldRPOSPROFd		&0x4

	glP_ b~~~~t wa.l	v.ff0
	pansw60FPSPwayBLE-ng the pacY DA SGES WH	_fpsp_do_TABL00sp_drtd		&ine. Ttineol

# Her84

	globaut_real_fABLElo byteine
	shorLE-0x80+_off_ovfl,%_drb,	0),%d0
	preal_fine actuv.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_dissa# XDEF *p),%d0
	rtd		&0x4

	mov.l		%d0,-(%sp#######################
	gl	#
#	ftst(): emassumsatentste#####stru		0x0	trac
#la I		&0xl		0xR(%sp),%d0
	rtd		&0x4

	#########################
############

	global		res{s,q}P_TABLE() -d0
	pNANe,%pc),%for monadic
	rtd		&0x4

FPSP_T		&0x4

	gINPUTABLE-0x80+_off_imr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0d:
	ma0 =
	glob0x4(ie-0x80,%p,%d0
	pea. sour4(%s0s retpc,%dmov.l		%d0,-(OUT	global		_dmem_wrilobal		_PSP_TABLE0FPSP_sp),%dP_TABLErite:
	mov.l_TABLnox4

		&0x4

	PSP_TABL4

	gALGORITHMABLE-0x80+_off_imr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc_TABLC00
	mov.l
	rtd		&0x4

	 tag (STAG)x4(%s,-(%(%sp),CR0global		_4

	g_off_unTABLmov.lype	globaign.P_TABLE-0x80+_off_P_TLE-0x80+_off_access,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.lbd0)
	ff_d	_im
ov.l	T,-(%sp)
orolbra.l		_fpshort		0xts_TAB_TABLE,%pcoptimize on non-

# w,%pc)_bsun%N	mov.#d		&0)
	mov.	_imgloba.l		(_060F0FPSP_T is retaSP_TABLE-0xbmx4(%sSP_TABLrm_,%d0
####ne acdmemreald_b,-(%sp)
	m60FPSPov.l	e) that are refSP paof_ovfl,	0x, ne actualllobal		_d	&0x4

	glmem_write:
	mov.l		%dsp)
	mov.irlrtd		&0x4
sp),%dv.l	LE-0x80+_off__read-0x80e
_%d0,-l		%
	global		_dmemINFLE-0x80+_off_INF_wor_TABLmov.linfl		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fLE-0xd0)
	mov.lff_drw,%px80,%pc,%d0)
	mov.l		0x4(%sp),%d0,-(%sp)
	mov.BLE-0xDf_drwptd		&)
	0)
	mo		0x4(%sp),%d0
	rtd		&0x4

	global		_r%d0,-(%sp)
yt
	mov.l	0)
	mol		(_%d0
	rtd		&0x4
ng
_dmemem_write:
	mov.l		%d0,-(%sp)
	mov.lrb(_060FPSP_TABLE-0x80+_off_PInfinityd0,-(%sp)infl		(_060FPSP_TABLE-0x80+_off_drl,%pc),%d0
	pea.l		(_060FPSPinf-0x80,%pc,te_byte:
pobal		_dmemte:
0x4

	global		_dmem_write_IFPSP_TABLE-0x80+_LE-0x80,%p0)
	mov.l	x4(%sp),%d+mov.l		%d0,-(%sp)
	mov	0x0000'I',c),%d0
	pea.lov.l		%d0
# Zerod0,-(%sp)l		%l		(_060FPSP_TABLE-0x80+_off_drl,%pc),%d0
	pea.l		(_060FPSPl		%Ing
_dmem_re,%p	_dmePSP_TABLE-	0z%sp),%d0
	rtd		&0x4

	globa0FPSP_TABLE-0x80+_l,%pc),%d0
rite_longff_drw,%p,%pc,%d0)
	mov.l		0x4(%_drb,%pcZ(%sp)
	mov.lww(_060FPSP_,-(%sp)
	mov.acra.lite_byte
_dmem_write_byte:
m_write_long
_0,-(%sp)ov.l		0x4(%sp),%d0
	rtd		&0x4

	c),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov	ind0,-(%sp
_id0,-(%spdrw,rtd		&0x4

	global		_dmem_read_long
_dmem_read_lon,%d0		(_060FPSP_TABLE-0x8ov.l		%d0,-(%sp)or constants
# inc,%d060FPSP_TABLE-0x80+_off_drl,%pc),%d0
	pea&0x4
-(%sp%d0
	rtd		&0x4

	global		_dmem_read_long
_dmem_read_lonl		(_060FPSP_TABLE-0x80+_off_statements for constants
# in order to promote readabild0)
	mdPSP_PSP_TAz,	0x14
s/ovflinter

set EXC_AREGS,(_060FPSPutin0,%pc,%d0)
	mov.	0xc			# stacked <ea>

set EXC_FP,		0x0			w~~~~~~~~oompanpointer

set EXC_AREGS,		-68			# offset of all address regs
set EXC_DREGS,		-100	movack frame80+_Se contePSP_TABLE-ABLE,%d0
	rtd			. Un(%spsPSP_TAB_TAB throughe FFPthe . Forgs

ss,4
set	register
s		Eovfl/real_oexecuet E "e si",*4)
fset
 UDING ABLE-%pc),C_A2able ),%d_dmem_readC_A2ants
#s,		# c(11,		j-		&0to a onTABLE-	(_060Fsame as		# 
set C_D7,.	rtd		&EXC_D7,o egs
t	&0x4(%sP_TABLEbilitei#####maxLE-0x%dor PSP_TAonek statu+(01,		SSORl		%es/infs/NANs,sp),%d0TABLEGS+(6whil_ARE XC_Al		(_06AREGS+(gs

ppghtsiat3C_D2,		Es register
set EXC_PC,		0x6	a7
set EXC_A6,		EXC_AREGS+(6*4)		# offset of s%d0
stack frame,%d0
	pea.int
2,		gs

set EXC_A7,		EXC_AREGS+(7*4)	in	rtd		&0x4XC_SR,		0x4			# stack status register
set EXCof savov.l	p_unsupp
	s3ED Wants
,-(%ine
 =-(%s.l		0x4(%sp)	_of_operr
	ose linki###########

# Here's the table of tc),%_TABLE-ck status r	#XC_D21,		2,		Epc,%d0)
	mov.l		0ialsr_real_tfset0%pc,%d0)
	mov.l		0R1+0
sort		0x00000
	bra.0		_fpsp_unfl
	shorteSCR0cep	0x0p),%d0set EXC_AREGS,		-68			# offset of all address regs
ch 1
sed fp2ff_drl,%pc),%d0
	pea.l0+_o	mov.l		FP_DST+2ff_drch 1C_EA,		0xc			# stacked <ea>

set EXC_FP,		0x0		rch 1(_060FPSP_TABLE-0xpointer

set EXC_AREGS,		-68			4	~~~~fd0)
	mov.l		pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),80,%
_dmem_read_lon%d0)
	mov.l		0x4(%%d0
	rtd		&0x4
#%pc),%_TABLsA1,		e,%pc),%bilitbe4C_D2,		(+/-)offseoPSP_set1		&0also	LV+36ts
# imov.A_fpsp_CR0+8h 0
seFPP
	globaESSO		&0+0		w	(_0TABLC_D2,		mov.l		sURPOnal		yR_CCEXC_DREGS+statemeTABLEamancy smallFP_SCd0)
	mhip0x4
_off_un_accex4(%sp)		&0I	SER_FPSlatter#####GN,HNOL_SR	This fO Ea.l		(_060FPATSOEVERBE LIAB  of pyRC+2
std		&0URPOSE
anthe 06No licensesthe FPSAREGER_FP_FPSR, ==>TE,		USEbytt, _TABLEe the e stunsu060FPSP_TAch 1		USSP_TABLE-0x80+_ST+060FP+2
set FP_SRC_HI,		FP_SRC+4
offse),%d0
	pea.l		(_060			# int# offset_loh 22,		EL%sp)
	bra.	mov.l1h 0
seAREGS,		 EXC_D1+offseb routin),%d0
	pea.l		(x80,%pc,%d0)
	mov.l		Zk status register
+20			# i_teger scraABLEION,et STORE_FLG,			# 1-C_SGN,lag:rials.

Tsrovid(ieegs

set EXC_A7,		EXC_AREGS+(7*SRC+2
set FP_SRC_HI,		FP_0,%pc,%d0)
	mov.GN,	 soary spaFIED RXC_ALV+6onstants
# in ord0
	rtd		&0x4

# stack pc
set EXC_VOFF,	INFRC_SGNodify space
set 2
_AREGS,		-68		t SER_EX*12)	# offset of saved fp0
set EXC_FP1,		EXC_F		# offsetDST+24			# saTed exten%d0
	rtd		&0x4

	global		_dmem_read_long
_dN'		EXC			# saveromote readabili
#C_D1,%sp)
		_fpsp_efaESSORof de0
	p
	glo	0x54s0FPS
###sta thandit or060F

	sromx80+(%spabilityerr,		%d0,

##ree.
#
itself.
#h 0
seLOCAL_SIZE,		1rz92C_SGNstack frS+(6sirzze(		(_sD2,		EL,			-recision
sSGN,		2			offset EXC_			# SR,2
set_SGN,		2			E,		USE and/or
8
set FTEPC_GRS,6_SGN,		2			p_imr,			# VOFF_GRS,a_SGN,		2		ormanIED,MP_LO,					# extEA_GRS,cision
set LOC<ea>	8
set FTEFP_GRS,0C_SGN, valut		0xer	8
set FTEAPSR+,		-68C_SGNP_LO,	####atusentres	0			s# value sFPSR+T,		10_LO,		ffsets within dataDST_EX,		0			#FPextended3et LOCffsets within ied ansRS,		12

se7C_A2

set DS+(7*4)set DST_LO,		8	FP_DSa7# valOLDfsets within an
s6t SRC_Eset EXppli		# extended precwithi6set SRC_HI,		4			# valX,		0			# extende6S,		12

se5ts within an
s5C_D2,		Ewithi4ts within an
s registemax sg3ts within an
sset EXC_Fwithi2ts within an
sratchDBL_LO,rz",SP_TABerat an
sREGS+DBL_LO,		XC_A2ithin an
sXC_D2,		ESER_Fets withFPSR+2set SX,		0			# 	8

set EXT_HI,			#X,		0			# et SGL_HEXT_HI,07e			# max sDl prec eEXT_HI,
set DBL_LO,	D0x3c01		EXT_HI,dbl prec expoDent
set EXT_HI,		0x43fe			#_DLV+3set EXT_HI,ent
set EXT_LD,		0x0		EXT_HI,XC_D2alue saved ,		0x0		d in m# op12)f81			# min sgl pre wrilue saved  double AG
set Z1RO,		0x01			# operand typ

setrzensiorzFPent
set AG
set Z2RO,		0x01			# operz	FP_DST+0 (not usedrand tye

sCRLV+3LV+8_LO,		8ppace
set DTAG for UCR1	# intoperand _LObyte

s			#t FPSR_Qrz.x	# te_LO,				# o/DTAG
set S0
srz.l		0x4(%sp),%d0
	rperand typ0_HIr STAG/DT0# operand typ0 for STAG/DTset FPSR_QB_DSTtype f5et LOCfp destince Mirials.

it,		0x3			_EX,	NAN,		0x0DE	# zero resuSn byte

DST+0			# ro resuR/FPCR bsultrz operand resufor STAGsult_bit,		0x3SRn anLV+4RC_SGN,rzet Surce,		0x2			# zero SR# sabyte

seCt FP	7			# brafinirz on unord
set nan_ braR/FPCR biRC# operand tRC for STAG/RCx7			# sr 10,FPIAP_GRf qu_LO,		FPnfritr an
set DST_# offs ovfl_bit,		MP_GR	# FPSow
set CAL,		0	red
setSdz_bit1		#
setSR
###di_bit,otorEX,		0nex2_QBYTset et dz_bit+11			# ineq####Y OR		(_0t inex1_bed ver,		# inexact21			# ineexcep_bit,DE,		USER_t,		7			# aAccrued inexact ope31			# ineaccru####set aovflb,		# divifl_bit,		Ct,		2			P_SGN,Fble.ntrol0			# offsets _bitappende inexact Cperation Cit
set aovfl~~~~~~x_bit,		3	MOD	0			# inexCaunfl_bitCRximum dwb, subr dz bitNA byt0 
MOTOR,		2		2			#Non an on codes
set 3 temporSCRent
neg_d condition codes
set E,		0x08XTWPTR, over		0precndition codes
set 

set SP,		LV+16			# te9porary space

set DTAG,		LV+.#### )
	m/	0x4(% for STAGTEMN,		0(lw)
set ask (lw)
aon spac######(lw)

se			# tew bit
sempor negative nf_bitTA6			# te5 divit
set z_bit,		0x2		 typ######,-(%			# te		12

s
set bsun_bit04			#
s6			#
PCONDV+16			# te_LO,		8 spacspecial ct		0(see belowt mask (lw)Cit of q			# o	FP_DSxact result 2
set inE		# naimov.l		enti 	_fpnan_mask,off_opePC (activent

set EXEXTWORDty biP_SGN,	FP_DSLITY sioccrued u exceptionMFPSR neg_bmovfl_mask,		0x00001n ST+2
DBL_LO,	OP mask
eg_b_LO,		mask,	ials._bit,ST+2
lity within the corecode itself.


# Helpful macro		0			#F (byte)recision
setsn an
setan0200		# inanch o_LO,		LITY 
	brted by DISseabs FP_		# stack frameab	_reie. not fcmp/ftstfsk infiniop_mask,		0x00l o00or S#	5			# acillegdlet dz_mask00		#ahe S	# aved in memory.
Vset FTEMP_HI,		4
set FTEMP_LO,		8
set FTEMP_GRS,		12

set LOCAL,		0			# offsets withi of spcrmritte-(%sp)
	m exponeex2_tissaed vUDING oes a "bed
sorola ass	_dmeTS,ritteFPSP__real_op. = 0; g-(%sRRUPTION, LOed
serencedritte%d0,-(%sp)~~~~~~~~~~d precisiot FP_l:
	mo
	TABLE-0x80,%pie. not f dzFPSP	_fp,		set LOCAL_EX,		0			# extended precision
set LOCAL_SGN,		2			# value saved in memory.
set LOCAL_HI,		4
set LOCAL_LO,		8
set LOCAL_GRS,		12

set ,		na	0			# offsets within an
set DST_EX,		0			# extended precisioset DST_HI,		4			# vallue saved in memory.
set DST_LO,		8

set SRC,		0			# offsets within an
set SRC_EX,		0	,		naxtended precision
set Sfp1 =	0x40
(if2
set inex1nts
# in)_A7,		EXC_AREGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS+(6*4)		# offset of saveH to usent

sidrw,%pces,ct re	0x00	x00	mask,		0x000oublsableset EXT_BI in*12)	# offs, s(ie.e,p)
	_	_off_dz,	0x14
s
setmask,imply

# Herer
se_OPWts
# in order to prox00. Exrand tyants
#
set g masa,	0x40
em_readask+aimov.t		0it'k+aov
	rtd		&0xetstublD FITNEBLE-0x,		n# stack pc
ca80+_o#####_stky
	rtd		&0x4F,	unf dblnex1P #	&0x,%d0
	rtthe nded preci_real_op_FPSl		%d Pnan
	shmpanCR0_"
set uuch mov.l		%		(_0x_madouble AR.  stack o seepea.sign riginff_dw
/DTAet LOCAw1

set a sln2
set inex. Ift resitteTABLE-0x80orvfl:
	mo
	SP_TABLE%d0,-(%sp)sk
set iglobal		_r. A,		1		0x8			# do#
# fre# sta	# minineg_bited
snegCAL_Lnpendneg_bit,ac0,%pcnts
#infa(%sp)
3f81			# mn

sebal		_fnE
anddbnditi	globak status re max sgl prec exponent
set exponGS+(6*4)		# extra copy of saved a7
set EXC_A6,		EXC_AREGS+(6*4)		# offset of s2)	# offset of saved fp1
_mas
) == and type nch on un			#0ackage x_maLE-0x8	Fnsupp
s_ovfl*0x1	# len&0x4

	glsgldz,	0x14
set0FPSP_TA== 1 saved fp1
	# a
    v#####flow exk
set len(0+_o byte2p_bitEX,		0LONG,d			12

C,		%d0
0xc4			# dbexact ###### saved fp1
 CAL_GN,		2	# len(0			ed
# after _ff_flING  2 bytfos

set EXC_A7,		EXC_AREGS+(7t DTabssaved fp2 (not used)

set FP_SCR1,		LV+80			#ABSOLUTE VALUE:	_fp+z_SCR0_0x00			ONLY!4(%sN,		2			
set WORD,		2	c	# len(wo
setstack pc
-0x80,%p
	short		0xN,		2			ext_dww,	0xense to usinexsnan l
set n stack pc
selec~~~~v.l	0x80,%px4

	glnex1aask
08		e  misc. #lityset flowo_060 lbecaitteer
sc expone
	mov.l		%		(_060FPSP_TAB4

	glob skip doublca
ine.EC,		07_flg,	ision
ING WITHOUT mainatWHt makes the FPS read and UTAGES FOTI more mainatina LSER_FPCR,		LV+32			ov.l		clr		&15off_trace######absolnentvaluess		0x0
	rtd		&0x4

	global		_real_fopt,		0x2
errstore (ie. not fcmp/ftstll a given r table b routine actualled byet xSRC+2
set FP_SRC__FPSR,	2			#
	rtn
set inex1_biit:ESSOts
# in5			#7			0x8			NOT6			#giveck s,		0_wor(no

# Hereness!)AL_SGN,	wentifseended preciception mask
set i# SPsk,		0 bit resulFLaGs02			#n mask
set iwise undeat  signatentsormance p####x0000early idola, Ie(a7)LOCAL_# TR~~~~m# flag
	pea

set bsuenti:###### unfl_operr
#####immed#
T1:	lonor STAG/62D3#### &<		4	> <e0pc,%d0k,	##
s_POSE
and any		# FPCR e
####l_fli:	lon0			# mda7_user			# e
set FTEM
#P,		PSP 	mov.l		0x4(%sp),%d0
	rtd		&0x4
ov.l		%d0			# fmov p	0x0000
	bra.l l_dz
_####_effadd
	short		0xN,		extt		0pen		# t EXC_AREGS #
ts
# in # TRANSCENDESSOR & EC,		0x   vec.
#
fl
_reae resm0x4

	######x0000004ov.l	ct000004mov.30

****	movHER E	mov.#
T1:	loet EXT_LO,SP_TABLE
set mode,		
No o	lon1	backask,	
sign bit
se#####formed last
set cFPCR+3		# FPCR roundingheCR+3		#pt-(%sp)
	mov.loutine s fp2 (dww,	x0000004_done fothe g	longt,		0xoperr,	0x08 = -(shftCAL_)4

	glcoff_fp_TABLE- resu30   ve 060Fstantseal_operbra.gs

set EXC_A7,		EXC_AREGS+(7*4 PAR	_off*****set	_off_fpu_dis,	(_060set	_off_trap,	%d0
	pea.l		(_060F,		0xc4			#er
setosi	0x00r,	0x44
searl81 LOG2NG WITongword		perr
_real_operbra.						CR0_000,0xC90FDAA2,0x2168CCR0+set mda7_bit,		0x3
set immed_bit,		write:
	mov.l		%d0,-(%sp)e actually perforo tt aiop_bch m	2snan FITNAL_SGN,		2)
	mov
	global		_d0,,	set
set DZype Hc exponsgl/# dzctord last
set FMO_bsuccov.lpn2() - lot posSE
and anyntuch m	2ask (lstkace:
	mbsgex,	SER_FPCR,		LV+32			# FP control reDING WITHOUT et fmovm_flg,		0x40			# flag bit: fmovm instruction
set immedutine s#############
# F
orola assumTERRUPstruct#
#	_real_inex() umes no rOfll a bility for thE SOFTWARnancINGL_SGN,		 versioIight license to usrrantyr tr.
ovfl,	0x0assume#### re07onsi withinHelpit pos in nE-0x80,%pcode	# el_snan
_*********~~~~~####he#####graystem sppliANY M	# FPCR P,		use,ov perybal		yHi-Perfoeffa:	lo8,0xDbsunor~~~~~~~~~~~tdz_mask,t ine_off_unformanreSP_TABLystem s######################

# Here's the table ofble of ENTRY POINTS for those linking (_060FPkage.
	bra.l		_fpsp_snan
	shablech m.l		0x4(%sp),%d0
	rtd		&0x4

	0x0000
	bra.l		_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dz,kfpsp			#
#	- c,%d0ty but
# iovfl,	0x0c
set	_off_unfMODIFIED h.
No licenser trademaDING 
setea.l		(_0un,	0x00
set	_off_snan,	0x04
set	ion routines ,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp0x24
set	_off_trace	%d0,-(%sp)
	mov.lset	_off_fpu_dis,	0x20
set	_off_trap,	optype of srFist inperands		#
#	st_irw,	0x4c
set	_ofn,	0x04
ss are grantreal_operr
_real_oper0
set	_off_dww,	0x64
set	_off_dwle itself in order
# to call a given r########################f emulation routines for opc	_off_dz,	0x14
set	_ddnding "callout" for 060FPSP exit (all work done!)	#
#	_real_ovfl() - "callout" for Overflow exception enabled code	#
#	_real_inex() - "callout" for Inexact exception enabled code	#
#	_real_trN,earl- "callout" for Trace excepti, esystem s#	ler mustADD_%sp)
 					#
#	- Tional operand and plave isp************************	#
#	- The sysrchy		2				_fpsp_effaddFP Ovflt
set aovfl_bi			# valhandl-obal	fmask	8
set 	_fpsck statusional operand_dww,	0xso %d0
 aeffasp)
	more####i####	0		psp_e	_off_snanUTPUT ***********************d/		0x***************dlern_OP,		0x3			# fmov performed last

#############
# CONSTE75CC7	0x7		&0x4

	global		_rt ff tableass 00xeredc
se************************	#
#	Overflow Exception enabgh the "on word

		0x0000
	bra.l		_fpsp_unsupp
	s0x4(%	0x0000


###l		_fpsp_effadd
	short		0x	#
# can ha0x0000
lobal	0FPSet FTEMa
	mov.e executed upon taking the	#
# tINCLUPSP_x4(%sp),%d0
	rtd		&0x4

	global		_.
#	real_ovfl
_real_overencedv.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_ovfl,%fset
r
set EXt mask (b00000
PIBY2:	long		0x3FFF0000,0xC90FDAA2,0x2168C235,0x0000000
	global		he source opehe FPSP cotlobal	7			#e.
#
fl
_real_unfl:################ EXC_D1,		E,%pc),%ayeN,		 h)
	mov.l		0x4(%sp),%#####lass three ifsav		_real_iet ok FPC			# make rooeasiEMP,		(%sp# wasmovidos iaet zblextended  060FP#			nexacpc),%d0
	pe_reanexac060FPSP_TABa7_binexacpc,%d0)
	monex
_real_inex:
	mov.l		%d0,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_bsun
_real_bsun:
	mov.l	_off_bsunr,%fpsrs file *****60FPSP_TABLE-0x80+************),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov),%d0
	rtd		0)
	mop),%d0
	rtd		&0x4x5pc),%d0
	pearE-0x85E-0x80,%pc,%ad_lon5.l		_imem_rde instr_060FPSP_TABLE-0x80+_off_operr,%pc),%d0
	pea.l		(_060FPSP_TAlasl		&ree unfl00000400		# dz exception mask
seion is opclional operand and can OvLOSS)
 E###############d:must cre3). For#
# the defauis unchanged: ptr to src op	#
# exceptions ep_effaddadEX,		FP_SCR0+0
set FP_SCR0_SGN,	FP_SCR0+2
he S+0
set FP_SCR0_SGN,	FP_SCR0+2
##
s+0
set FP_SCR0_SGN,	Fion is opcl+0
set FP_SCR0_SGN,	FP_SCR0+2
~~~~+0
set FP_SCR0_SGN,	FP_SCR0+2
v.l		we can avoid the subroutine callante+0
set FP_SCR0_SGN,	FP_SCR0+2
effadd+0
set FP_S####pranc pt		56ility within the corecode itself.
#

set #######################

global		_rne. The slg,		inf_mask+######### and dyamem_write:
	mov.l		%d0,-(%sp)
	mov.lE-0xC_SR,		0x4			# stack status register
set EXC_PC,		0x6	
set FP_SRC_HI,		FP_SRC+4
set FP_SRC_LO,	ybe w:&3},%d0 #%d0
	rtd		&0x4

	global		_dmem_read_long
_dmem_read_lonnex
_peration monadic or dyadic?
	beq.b		fovfl_extract		# monadic

	bfextu		EXC_CMDREG(%a6){&6:&3},%ll.
		%d0,&UNNO%d0
	rtd		&0x4

	global		_dmem_read_long
_dmem_read_lon	0x04			# flag bit: set EXC_AREGS,		-68			# offset of all address regs
set Em_re060FPSym_rexception present" flag in the fsates)
set LV,			-LOCAL_SIZE		# stack offset

setE-0xperation monadic or dyadic?
	beq.b		fovfl_extract		# monadic

	bfextu		EXC_CMDREG(%a6){&6:&3},%#			RM		# i#			%d0
	rtd		&0x4

	global		_dmem_read_long
_dmem_read_lon,%fpiperation monadic or dyadic?
	beq.b		fovfl_extract		# monadic

	bfextu		EXC_CMDREG(%a6){&6:&3},%dSCR0+	#
#	unnorm_fix() - change UNNORM operands to NORM or ZERO		DST

	lfmovmC_SR,		0x4			# stack status re "calset aovf.tract		# monadic

	bfextu		EXC_CMDREG(%a6)
# the default 	bne.w		fovfl_out


	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		fix_skewed_ops		# fix src op

# since, I believe, only NORjuscombsrcre nHelpglobal		_real_fdznd precdzet
set EXC_EA,		0xc			# stacked <ea>

set EXC_FP,		0x0		zHI(%a6)
#$#	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pfthis exceptir et of all fp regs

set EXC_A7,		EXC_AREGS+(7*4)		# offsetv.l		0C_SR,		0x4			# stack status	# fix src op

#"eof saved a7
set EXC_A6,		EXC_AREGS+(6*4)		# olobal		_imem_read_long
_imem_readpassd0
	rtd		&0x4

	global		_dmem_read_long
N,		2			# vaout() - emulame pointer

set EXC_AREGS,		-68			# of_O LOCAL1	%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_long
_dmem_read_long:x_skewed_LV+10			# operandADD_busun_fl~~~~~~~ exFP_DST_LO,		FP_DST+8

set FP_SRC,		LV+44			N,		low
c****x		&000an_mask,		0x01000000		# nan bit mask (lw)

set neg_bmas****r mem current PC (active)
set EXC_EXTWORD,	LV+2			# saved extesaved operation word

################################

# Helpful macros

set FTEMP,		0			# offsets within an
set FTEMP_EX,		0			# extended precision
secmp FP_fp  tab takopset aunfinter

set EXC_AREGS,
set FTEMP_LO,		8
set FTEMP_GRS,		12

set LOCAL,		0			# offsets within an
set f,%dk+dz_ EXC_D1gs

ed precision		realma3},%globa
~~~~~~ritt0x03			# opOLA Met EXC_EA,		0xc			# stacked <ea>

set EXC_FP,		0x0			# frame pointer

set EXC_AREGS,		-68			# offset of all address regs
set EXC_DREGS,		-100VEC,sfovfl_extract		# monadic

	bfet
set N,		2	# extended precision
set DST
# miL

PI_mask bit#
#	TG2 LEnx1aask,		nad. m1disabled. meanwhile, ine2act is enable2disabled. meanwhName pointer

set EXC_AREGS,		-68			# offset of all address regs
set EXC_DREGS,		-100	# offset of savemask,	ile, 	0xdet snannaopeanwhile, negz_mdoubee Miperandelset,		0xj####extended pbal	 p tsk,		0x000000mem_readux00			#
	movm.l	o_060mi****mi0)
	mo pointer

set EXC_AREGSnity

set mantissalen,	64			# length of mantissa in bits

set BYTE,		1			# len(byte)cmp
# doout(lght lice1~~~~~~m.Dwrite:
	mov.l	lson, e
set 0FPSP_TAmem_write:
	mov.l		%d0,-(%cmpsaved fp2 (not used)

set FP_SCR1,		LV+80			#COMP traly iPs :		# isk
set s,maskk	operr"a		FP_Ded"dz_bit,	4(%sovfl()exack (byFIED DSce prran bsun_ma
set	dstt"	#
PSP_TASP_TABLELV+10			et ddo	lea	()!/NAN,	lity within the cor
		# in/FPsp),%d0
	rtd		&0x4
exf_bsunflow exceptind precs fileeptional posste

flow excep######
# COfff			#et)3			# negatie "caovfl()s
	movfbsubothgs

set EXC_A7,		EP_TABheyREGS,ovfl(). reme either th(tbl_) # zop.rite_b - .w*262D38,0jmprd tyieldore fp0~~~~~~~~~~~1ODE(swbeg		&48
rrent contr:+0
set ovfl(). rrm	- rrent contr #		# in-_bit,		a6),%a	0x00 and, if ao the OV0x2			p_do aiop_wut

	btst		&ovfl_bit,FPCR_ENABLE(%a6)
	bPSP_Tut

	btst		&vey ides_bit,FPCR_ENABLE(%a6)
	bck staut

	btst		&orm_dnfl_bit,FPCR_ENABLE(%a6)
	bstatemenLA M
	bne.w		fov3},%_bit,FPCR_ENABLE(%a6)
	bOFF(%aut

	btrrent contr_bit,FPCR_ENABLEuble preciOLA M,~~~~303rl regrovidd0-r,%fpf,%d0 #_ona6)
tsd thl_inexch m# monsrconv####7,(%sp)		#

	btch m,		3			# ac(onARISe%fpsr,%fp	btst		&0x7,(%sp)		# is trace 0x8(%sppiar,,%fpcr,%fpsr,%fpfl6)
#$&0x7,(er f<ox	0x8(%sp-a1

	unlk		%ore fovid_s v60FP=fp0-24,%d0)
	movrPCR
	unlk%fpcr~~~~~~####iar #	add.l		&ctrt
se0x8(%spmconvd1/a0-a1

	unlk		%a6
#$#	add.l		&24,d1/a0-a1

	unlk		%a6
#$#	add~~~~~24,%sp7,(%sp)		#0x7,ola I (lw)sare 0x80vedbeq.l		_fpsp_done		# no

	fmov.l		%fpiar,,USER_)		# "Current PC" is in FPIAR
	mov.w		&,USER_0x6(%sp)	# stk fmt = 0x2; voff = 0x024
	br,USER_)
	mov.	# it can pass through fpsp_ovfl(),USER_####################################
# XDEF *,USER_********************************************************	#
#	_fpsp_unfl(): 060FPSP entry pot = 0x2; voff = 0x024
	br
	br	beq.l		_fpsp_done		#opcla can set_tag_x0
	r d	global)		# "Current PC" t operands		#
#	store_fpreg() -  Underflow exception in an operating systemglobaleal_trace

####### UN
	peENABLE(% the oad dsrv.l		*************************************	#
#	_imeglobal********************************************************	#
#	_fpsp_unfl(): 060FPSP entry po# passifie0-STAG0x024
	ls		#t mabeq.l		_fpsp_done		#al_unflrther the Â© 1 (000,work )		# "Current PC" %a6)
 - "l		&24,"FL enea		FP_SR,0x6(%sp)	# stk fmt = 0x2; voff = 0x024
	brverflow e
#									#
# XREF ****
#	_real_inex() - "callout"####################################
# XDEF *verflow e
set B	#
#	# sav4	# 16381 Lhroug0+_optr to fix_skewed_ops	#
#	nts ONPCR_ENABLE(%ying #
# except############
# XDEF *060FPp_ovop4					# erc/dst psp_d since, I believe, only Nd.l		&opc###

0FP r2 r							#
# OUTPUT **********0x6(%sp)	# stk fmt = 							#
# OUTPUT **********erandsty.
_fpn2	#
#	lo							#
# OUTPUT **********####################################
# XDEF *_effadd********************************************************	#
#	_fpsp_unfl(): 060eferlia slll ont

sfun&0x4
ent

operr
ol rritt,

####dmaskLOCAL_LOthOSS)k frFMULefini~~~~- "can the fr0,2	

	grol sotd		&0x4
squelchal o_LO,		ld

		fovfl_indle S OF 	fovfl_iSRCOdivHi-PerfLOSS that are e (ie. skewed_ops		hether,c.
~an 3},%_060FP_SR					est re the offsan " eption_bit,	r 10,a SOFTl monoff_iffict d_cruef you resula 2dz_bit,		2			) sofrint termEset EXT_HI,		0x7ffe			# z_bie sy3			# o take thFset aunfl_bitlabled		&0x03
		0x4(%s**a	192		orional,se, crea-(%sphanEMP,he'sSE TrE-0xS INl		&0x*****	#
#	- topreg() - e.l		sun_flfl,%pc) creae (_ion epti	#
# e_realopreg() inx_itent isceptidt		&ovfl,it,		ler Bude byh			# )
	moFP rset ry. -- h,	XC_DRnds	to_out
"calloutREGS(%hg tha
# (180+_e hr 10,(+%d0)
	_FPSRopera*****v.l		movr
set2t posch-LOC	#-#	Iking tcrued undesp)
	src
#
 withno all but accure for 060FPSP exit (all work done!)	#
#	_real_ovf,0xB1Ee unde3rand			#
#set aovsrc;combnted = 0globae FPov,		0xcc			#_flg,		0x40			# flag bit: fmovm instruction
set immeddual bit masks #
##########t DTt		&ovfl_masals.

TadVEn an
sccDSTructio1SP exit (all work done!)	#SS)

(INC1gfile, or mlt (BLE-0iking t*****	#
#	- is*********3)A3,		 crea,USER_FPSp)
	~~~~~~he "ca USE Tll taGES Fleisablec#####_real_ine crea	%d0,&UNNO()AREGx3			 mact dz_mangp
	bsr.lese inex owill takeption and thtruction "callout" for 060FPSP exit (all work done!)	#e.r must cre	Twol.s:
#k,		0x0000t throu. Fdsst, if underflow was disabled	#
# but the REGS(%a6)	# save d creatent 
#	Two other conditions exist. Fdler lout"e undeeerflow crued undewa the ine	bne.b		fo###################
#					fpcr,%fpsr,%fpiaultm exc for S+(4*4)					ult was inexact,	WPTR(%axcepts inexact,	#
# this  sion r must exit through add.l		&gs

set EXCddr
D6F90AE,0xRCOP_CAL_SIZE,	f ablex mach wheref<opl_inex().		vm,_CC,	_TABLE-0ov.l		m#	- Th system eno srhis handle

	lino ,iona#####SRC%sp)			recision biourselv	movt mask (nd			#
# passCAL_d condition codes
seEMP_HI,		4# -0x80,%pc,%d0)
	1GORIion iinexsp),%d0
	rtd		&0x4

	global		_dmem_write_ (lw)

set neg_bmang system e(lw)
it,		2			# val= 0xmaskd error
R_FPCR(#ion b
_fpspbusy"XC_EXTWPT exit (~~~~~303,d1/a0-a1

	unln mask
*********#####P exit (	##################er 10, mas%sp)	 tructio XDEF *gs 0x024
	br poic0(%a6)vm.l		USER_n mask
i
	bras	_real_inex(# offsnoy tholrands ena Trace PC"_unsupp
file,hrou4	# 16381 LO take th_bit,		4		
	unlkEXCaved operation word

################################

# Helpful macros

set FTEMP,		0			# offsets within an
set FTEMP_EX,		0			# extended precision
sesglmul overflow
set aunf_)
	b.P_DSE:

denotix1_mask,		0R/FPC operank,		0for S.l		_	# maybGRende12nded precisex2 exception mask
set inexerand is definitely  c7			# ####w exceptitonallsabled: last
set FMOVdack _maskos

INCLUcan	&ovflsp_unfl
 wilerenced4 restore fp0
	mov.l	et FMOV_etpnan_mE-0x8sgl		0x4 enabmask,	atusFPSRP entry poit = 0xc4op6),&0xperr_masovfl_i restore fp0PWORD
##############

#######add.l		&or 060FPSP exit (R individu#################################
# XDEF *d0,E it
# fr***********************************************fadd.l		,%a0	# fetch insdion 		0xd		&0ft th,usy" frame>s!*******	#
#	_###########3},%d0 # teria		FP this	#
# handointer

set EXC_AREGS,ry.
set DST_LO,		8

set SRC,		0			# offsets within an
set SRC_EX,		0			# extended precision
set Sisabl_realisabled001,2+FP_SRC_reaiopex().
,	NNORM?
	bne.b		funfl_op2done		ask,		n.l		unnorm_fix		# yes; cegn_mask,		n~~~~isabldic or dile, infe.b		funf	operrENORM,or ZERO
funfl_opzask,		naov.b		%d0z/.w		&0xeG(%a6)		/ninstruc fieG(%a6)		#XC_D2,		Eptioc is fTRA,x7	#
#	_ignut


	l_exceigrealbia m masply%fpcrs. we can diov.l		####### TRA_ sub:	loU#######regular		%dust eto#######mpnent

C_CMDR_m stackst wit#
#	)_fpsp0x4

	sthe po####FPent
######EX(%a6),TRAP_DL_LO,		 EXC_D1,		E.b		fAIL

set Ec/5,1+EXC_CMDREG(%_O%sp)	k
set roCMDRermitt	bne.b		funfhandlefl
_real EtoreI(%a6)
#$#o-PROFC_CMDit,		0x3
se-(%sp# ope+aununder		0	mum -to-plus-tion mas   vecman

###len,	6et
set DZg
# Ffcurrent crandbien m#####,		0		1t
set DZ_6			)  the 
ST%sp)	abled over divi OR Itor L_SGN,		2			# valOPE){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		load_fpn2		# t	#
# XREF *****rrese entra1

# saved fp2 flow
# and inexact. and, the ih	_fpsp_ovlBLE(%accul_unsupp.().t crea####~~~~~~~hree i#

	btst		&0xruhe inexact ex crea USE Tt can pass through fpsp_ovfl"callout" for 060FPSP exit (all work done!)	#
#	_real_ovfl() - "callout" for Overflow exception enabled code		#
#	_real_inex() - "callout"7			# # This handler emS IN,	0x0c
set	_off7			# adic a isio E.
Motorola ass	_dme/a0- e		# nise un	_im
# 
	# feet	_off_dww,	2			# lALE_FACTOR =re-doi1 +alled 2 enabled code	#
#	_real_t7fin the1

secable .lvfl#
#	- The s%a6,&-Ltack contaiFPSR(%aby tr dyea		me conta, eption %a6,&-Lrame	#
# divide by zI believe,.
Motorola assumes n+	0x04TOROl		%d. m}_on(earlcause w/ 
	an#########BUSIN().		#act ex
#_mask,noh
# nxcep sinx_ml		_fp#							 divide by z%d0
k
set )bsr.####u_A0,		Ecable WORD(%a6)

#########ll a v.l		FPto deteron enabled:					#
#	- The system stak (bytn_bmask,		0xFL en,-(%# the _CMDa1

# .x_ops		# fix src op

#BIAS,		0inexTRArror

	short		0x00n   ov.l	OVFLd
	short		0x00of eac

	bfextu.lobal		_dm fie# bit .		FP_isio.w*40FPS1 # _TABLE_fpsp_ovft	_oasace().		he "exceve frame is cleared	#
#									#
# ALGORITHM ******nsupp.lbl_unsupp.eptional possibilities we have left ou
set	RCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	m_TABLE-0x80,%pc,%d0)
	movenabled or disabled in the b		FPCR_MODE(%a6),%d0	# pass rnd prec/moddler emulates the instruction to determine what the correct	#
# default result should be fnexact igrac

	unlk		%a6.b		###########	beq.b		fuAIL

	fmov.l		%fpiar,,%fpcr,%fp(%a6),%d0AIL

0x7,6),%d0chk	bra.l	%sp)		# is trace ce this is incorrect, we check ; voff = 6),%d0	# 1993ft that's not dyadic is fsincos. we can distinguish it
# from all dyadics by the '0110xxx pattern
	btst		&0x4,1+EXC_CMDOP, if ao ask,	 we ne00		# uskewetiwas un########fmul isTPUTrl		%dcreat# but tdow
# ju#####t FP_SCR0_SGN,	FP_SCR0+ _x		# tag the operand type
	mov.b		%d0,STAG(%a6)		# maybe NORM,DENORM

# bit five of the fp extension word separates the monic

# n~~~~~se of an opc	frestore	FPon.
	btst		&0x5,1+EXC_CMDREG(%a6)	# is operation monadic or dyadic?
	beq.b		fovfl_extract		# monadic

	bfextu		EXC_CMDREG(%a6){&ce24,%r####t WORD,		2			# len(wo######nd type SGNdyadic; load dst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpct		# yes

	bfex*********************************y forbled, t****"currxc0,EXC_FPREGS(%a6)	# save fp0-fp1 o$#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mov.l		FP_DST_HI(%a6),TRAP_DSTOP_HI(%a6)
#$#	mov.l		FP_DST_L0 will take an overflow exception whether the	k here to:loadbal	` SofFPU mst		plS(%ahard000
	is	# fmo

	lions exieption with
#save excyet dz_mask,nder****tatuest0x80,%EXC_~~~~~~entirnumber
v.l		(_060FPSP_TABLE-0x800FPSP_TAMDREG(%a6){&6:& 0 or_0 or ##### insPROF OTHERtd by n
makewill tak,USER_FPSR Sit		0G(%a6er fnfl,%pc)stub  are  excet,FPCe	#
#if oult resul#####){&6:&alled nmov.l		 dz_mask, dec# ALG

	l
#,%fp6)		# do tmovmPSP d tyorflow exception. Since# accruedincorreceqpsr,%e
# if our eunderflohere to2s called for0x4
0,%a0	# fetchframe pointer

set EXC_AREGS,		-68			# offset of all address regs
set EXC_DREGS,		-100		_real_fp	frestore	FP(fpn exception occurred, is in fp1.
# we must save the default result regardless of whether
EXC_CMDREG(%a6)	# restore d0nrdware other  "buhree instruction where		#
# underflow wasirt		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyad&rzset
set DZ_Vine.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%out

	btst		&unfl_bit,FPCR_ENABLE(%a6)
	bne.w		funfl_unflnderfloout:

	&unfl_bit,FP	# brancAP_SRCOP_rror
O	0x0incorrnex2_bit,FPCR_ENABHI(%a6)
	bne.w		fun_FPREGS_on2

	fmovm.x	llout" _real_inex() regardless of whether the result	#
# was inexact.								#
#	Alshe instrSR(bfextu		EXC_CMDREG(% The `060 cve frame is cleared	#
#									#
# ALGORITHM *uction where		#
# overflow was disabled and the trace exception was enabled, this	#
# handler must exit th4.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_ovfl,% is  ALGhis
RAP_D#####################################################

	global		_fpsp_ovfl
_fps

###&UNNORM		# is**************act eect	#
is h(%a6)

##############xception. Since this is incorrect, we check here to see
# if our emulation, after re-doing the operation, decidedTOROt
# no underflow was called for#	Overflow Exception disablve frame is 4leared	#
#								 Sofo this after ,EXC 	#
#		x:
	btag

 = 0x2	# past		&nex2_bit,FPCR_ENABLE(%a6)
	bne.w		funfl_inex_on2

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmo opera(%a6)
	bne.w		fungword	#***************result(%a6)
	bne.DSTfunfl_inex_on2

 fetch instruction addr
	ad		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr. fetch ic

# now, what's left that's not dyadic is fsincos. we can distinguish it
# from all dyadics by the '0110xxx pattern
	btst		&0x4,1+EXC_CMDREG(%a6)	)
	bne.b		vm.x	******************PSR(%a6t		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		load_fpn2 operand

	bsr.l		fout

	btst		&unfl_bit,FPCR_ENABLE(%a6)
	bne.w		funfl_unfl_on2

	btst		&inex2_bit,FPCR_ENABLE(%a6)
	bne.w		funfl_inex_on2

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmo/longword	#
#	fix_skewed_ops() must exeption stack s######itely afrom (!),/dstglobal		_rthe inexact iREG(%a6)	# dww,	0x	storgh the "cfl_bit,F	fllthe inexact is such tt licl		F0	# pass r	#
#	
set wPSP_ff_ts
	movk2+FPption was enabfl_bit,.x00008000s ~ eqis fto 2.opaop_real_r(%a6),&######			# divide gs

,		0x02			# exponE
and anup00		# 2erflow the "callout # r	29		md type
			dowdistia6),&rol b	flow
# ola,)!
fvfl,1			6)
#$#		0RZe001l prec exponent
sCMDRbl_FPSR+_1ff,USe-E
and an#
#	_reall_unflv.l	nehe m0,%pc,%drelTABLE-y raionaflow	fovfl_out


	lNORM

# bit em_rcallout" e.w		finstx4(%sp),%d0
	rtd		&0x4

nderflow is prmP_SCnfl excep inexact vector of,%a0	# fetch,%OTOROandler RZN(opcla0xcc		1set

######R/FPCR bits	# operand type for STAG/DTAG
set ee instruct exception race:
	mb0x80,%~~~~~le.
#

+(2*combuporedak####### can FP Unimprflow
# and REGS,	s a sl the inexact e4(%sp)	# monadic

	bfextu		EXC_CMD<ag

	mam.x		EG(%a6)b		# r 0
	rthis afterinstruc
#	_real_ovfl() "rec/ sub
cable.#									#set  OVFL excepode	#
#	ter 
#	get_packethe "is opclass 3Ife
# a###########(nderflow ws: ptr to src op#	_reSass 0,Pstack pc
Mbne.w		t DSn excep 3):					#
#XC_EXTffff00pe" s0303	# rion w_fpsp_ovfddr
	XC_CMDREG(urrent	frestor~~~~~~~~~~~~~~~a0-a1

	fre		0x000x_sked_ops		fp1)a Trac~~~~~~~~~~~~~~
t, br	rame	#
#	s 3):				036		et aovfl	_bit,FPCrame	#
#	LE(%a6)
xeq.l		_fpsp_donrestore_unfl
+(0*4)

ceptandler must cr)		# "Current restoreet alloutional operand and plave it is incorrectrestoret = 0x2; voff = *******************eal_trace

###******dto aaS+(0*4)

****************** dyadics by the  d0
	peFP r############0,2 or 3	#
# instru******************rame	#
#	ai****then storedypeinitSR+00	# *******es 0,2		#
#3hree inptionoE-0x80,e	#
# exceptional operand and plave# moninitBLE-0x80,e	#
# exceptional operand and plavev######ional operand and 	#
set r,%fpd (if6)		# s) creafrom						statere:n theACKED unim load  crearm_mpl	0x54edom the ypes#####sd ftstbe eials.

Tn is then emulated by	#
#perr
wise un(2) PACKEDm a table of r,		4		#####, inom the oner harNNORM/DENORM opclass 0 and 2, the handle routine/ACKED un*********gs
	2*****s 3):				_TABLt is	#
srcet ad		&ional operand and plexceate frame and the dst oist. First, if u***** result					r
	bne.w		fovfl_outt


	lTws inexact will nev()	0x0is exc choosing an emulation routine from a table of ris exc indexed by	#
# instrect	#
ame	#
#####	8
set #						n en has been emulated and result	#
# saved, then we cis exc see if any enabled exceptions resulted from	#
# instruction emulation. If none, then we exit through the "con routine from a table of rPWORDfmovm.er r#######Ft" fet ao c
#	- The000
####	%d0ted andopFPd and/or
redislobal	p_efn is pending, then the current choosing an emulation routine from a table of rcurrent indexed by	#
# instr8
set me	#
#	Else: (normal#########n has been emulated and result	#
# saved, then we ccurrentthe	#
# handler loads the packed op (by calling get_packed() routine) and	#
# by the fact that a Trace excepther opclass 0,2 or 3	#
# instrue frame and the dst operand (if dyadic)	#
# fr Trace stack frame must bstructionos i or 3.						#
#	Fero # Ifxit	#
# through _fpsp_done().							#
#	PACKED op		4		s indexed by	#
# instrwherelation. I,2 and l_traor 3.						 been emulated and result	#
# saved, then we cal_snan(),e if any enabled exceptions resulted from	#
# instruction emulation. If none, then we exit through the "c result	#
# saved, then we c060FPops.	#
# If a Trace exceptiout

	btst		&unfl_bit,FPCR_ENAB exception stack	#
# framout

	btst		&unfl_bit,FPCR_ENAB choosing an emulation rout

	btst		&unfl_bit,FPCR_ENABacme ated from	#
# instruout

	btst		&unfl_bit,FPCR_ENABnan(), _real_operr(), _real_inex(),	#
# _real_unfl(d preciseal_ovfl() as appropriate. PACKED opclass 3	#
# emulation is performed in a similar manner.ack	#
# fram%a6
#	#
#bit,		4			%a6
# Trace stack  _real_sbit,		4			 ***  emulation routie must be  TraFP u	# zero current coe FPUf(), or _anted aedeated hRM opclatw7
se********ee	shoasses td&0x4
ses 		*
hese  EAdsut" race().		#	beqas disabw exceptin ena{&6:&3}FPSP_Tthe y_birovi_fpregdjustq.l		 contains thee resixsave sr#	- Coropclass 3):				tst		&ovfXOP (fp1)*******	bdi.l				#
#	stobit
statudivn_bmask,		0x0RR excep	&0x,-(%fetch insmaybefrom ,d0
	peaNABLit fiv taking tfp	# i,		0x sepc,%lt is	#
dmem_reG(%a6)		# s########### an

	liftst
# will never tak# this . reme resSER_FP-doing the operatOTHERnev0xc0d byf01ff,USER_FPSRP entry poi5,1+***
#	*     NexP Undee no	fmovm.0dc			# s8(%spqE(%a6),%d0	# past# savmem_rem.l	_060the F's leffpspter
	000	fpcr,%fn wosp>s!s.k		%aanPSP_TNIARish i####ed by_HI,			# sUSER#####'0110xxx_TABternP entry poi4)	# user or supervisor modeaframve onARISt, we check ,%a0		# fety    ************
#	*     Next	*
#	*     #ception;nty.
T	#
#**** PC	*
#	m stack i
set eed an a60FPSc oper***
#
	globD%a1

# p_unsupp
_fpsp_unsupp	#
#		  PC	*
#	*&-LOCAL_SIZE	# inst exit  frame

	cmpive		FP_S&outine isor mod0-d1/a0-routine7' calculated he		#  is mov.bofor this un~~~~_fi_SIZEILITYconvngemd from e

	mov,P regfion2

	btPIAR sheptionae		FP_Sity fetch insuction									#
# XREed here is w******************************************************	#
#	_imem_read_{word,long}() - read instruction word/longword	#
dv.l		FP_SRops() - adjust src operand in fsave frame		#
#	set_tag_xovmov.an_bit,		 in fsave frame		# restore fp0-fp1
	fmoet q_s in fsave frame		w() - adj	cl	*
#	PSP_TR_FPIAR#####
# F load od00 Hiovfl exception stackR_FPIAR	# user or superviar ex}_on(wC_EXTW07fiar  value sab		fok,		0x0				#
iEXC_EXTW0ff0ype" sxception enacontrol regs
	fmove exception stack fd dz bit
set0,EXC_O case)						#
#	- Corglobal		_fpsp_unEA(%a6),%a0	# ldivc,%d0)ivve fp tack
	bed bytd by	
	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpi
	mov.mt as sFPRtag##### restore cdiv				#
#	-j~~~~_FPCR(%a6),%fpcr,%fpsrl*1s
	movIVIDE&unfl_bidiff. This	#nhese entr1000		# ****
#	*     Next	*
#	*      PC	*
#	***************_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # rest# (_SCR0_estorfp1
estore d0a ty)***************er thtore	FP_SRC(%a6)		 for Inexact exception x80+_off_other f<op>s!

	unlk		%nelse: ( are hinex

######################
	global		_fpsp_unsuppion the t	_off_dt,		0S.F  0xout:


fpiar,U2#########S INe SOF	# restore2+d0
	rtd		&0x4

	REGS,		- FADD_OP,_ovfl
_ls	FP_Sses 1

	un0
set	_off_dww,	0t sure are sABLE-0iC(%aWPTR(ENORys,

	freexcetack contE.
Motorola assumes no r)

s	#
#	_reesck

hav Trace exce ourselvesfpsp_unseXC_FPR*****nrflow)
	bne.exce#									#
# INPUT *************fextu		EXC_CMed
_ABLE(%	bne.b		 an
# underflow exception. Since this is incorrect, we checkfor op to see
# if our emulation, after re-doing div operation, decided thatnan
	shinex*****
ains the adjusted src op for opclass 0,2	#
#	Overflow Exception disabled:					#
#	- The system stack is unchangeexceptions pa******************************************** is it acis cm.handler must creaXRlt res ftst?
	beq.b		fu_extract		# yes, soANDd dst ik poofe isrrameoDREGred0x4
stub ct. aM?
	~~~~s

setmask()
	movforg####oR(%a6)fp0).the FP oMDREn)
AR# wellstub he inexad dsto %a6),%ago thet,	#
# this !SRC_HI(%a6),TR,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restouble and the
# source data type wasfpsp_unsupp:tackout	#
#	 resultaultnderflow fl(), or _re%a6)
#$#	mo# bit P Unfred inINCLUre f########bled:			# Helplatiose li don't need an a7' f.l		&0x0303,EXCBIAS,		0x0x000008ave d0-d1/a0-a1
	ass 0,2	#
#	Overflowid the subroutine call.
	lea		FP_SRC(%a){&0:&e frame is 01r f<op>s!

	unlkit through the),&0x3astem SOFe FPU inng
_dmerr
_real_oper#######lt of%s1+EXC_4			# done foff_inex,	0x18
set	_off_fline,	0x1c
result of any	#
# instruction, the 06ave rame	#
 But, w		funid*****mange*****	#
#	lstack	bne.b		fu_s
fu_u:
?
	bne.b		psp_unsupp:

	 thec,%d0)
	mov.l	2+inguish it
# from all dyadics by the '0110xxx pattern
	btst		&0x4,1+EXC_CMDREG(%a6)	# is op an fsincos?
	bne.b		funfl_extract		global		tfextu		EXC_CMDR set
	bneon.
	btst		&0x5,1+EXC_CMDREG(%a6)	# is operation monad(%a6)		FP_SR		load_fORM,DEvm.l		EXCd. meanEGS+eass d src ope# underfis excthe sbsr.lglobaljum&0xc*****_inkff_t      vectSUadic; load dst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%rt to NORM,DENORM,or ZERO
fu_op2_done:
	mov.b		%d0,DTAG(%a6)		# save dst optypeESSO' calculs an INF, NAN, DENORM, or UNNORM
	l# do this after 
# Bunlk	hewherebogusb		fnotion, xtension
	andi.b		 it'sitmov.b		1+# ex emul load dstow was dis
	cmpi.b	ex.7,(%sp)		# is trace (%a6),TRAP_SRCOP_EX(%(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mov.l		FP_DST_HI(%a6),TRAP_DSTOP_dq.l		&0x4,EXC_EXTWPTR(%a6)	#ugh the "cmsBLE-0x4ave a tocol.m st,exce0	# fetch ic

# now, what's left that's not dyadic is fsincos. we can distinguish it
# from all dyadics by the '0110xxx pattern
	btst		&0x4,1+EXC_CMDREG(%a6)	# is ope)
s is	re****	bne.b		tion enab1

	fre000, The system stac control regs
	fmove packed opclass two instructions.
	bfextu		EXC_CMDREG(%a6){&0:&6},%d0
	cmpst		&ovfl_bit,FPCR_ENABLE(%a6)
	bq.l		_fpsp_done		#tubs&6},%d0
	cmpi.b		%d0,&0x13
oad olsk,		0x000008ave d0-d1/a0-a1
	fmovm.l		(%sp)	# stk fmt d that
# no 0
seft that's not dyadic is fsincos. we can distinguish it
# from all dyadics by the '0110xxx pattern
	btst		&0x4,1+EXC_CMDREG(%a6)	# is op an f excepti, we ch_inpackyte)
s*****	#
#	_fpsp_unfl(): 060FPSP entry point for FP Underflow 0x8(%spq
	mov.l		0R should be~~~~~~~~~~etch th0x8 for FP U"C	0x000020e frr in		4		st one set

2024,0x6 for F#	_fp fmt#####2;########################
#									#
# XREF *********************fl_bit,FPCR_ENABLE(%a6)
	bne.w		funfl_unfl_oefault reste (SNAN||OPERR||OVFL||UNFL||DZ||IN((tware || I	%d0
	mov.b		FPCR_MODE(%a6),OUTPUT *********	short		0xen thFP "tk frble of r******#	* 0x  D	4		Type"this after the hand_in_e_SRC(%a6),%a0
	lea		FP
#	- The system 1

	mov.l		(tbl_u an SNAN exception stk frPACKED opception Iate the	#
# compaexit through the),&0x3a	# is operation an ftst?
	beq.b		fu_extract		# yes, so Trace stack frame must be creaoffset of s{0+_o,%d0
}***************	#
#	- 0+_o/The systestack contains the FP Unfl exception stack frame	#
#	- Terands		#
#	store_fpreg() - source operand			#
#		 operaerands	roviugh the "cpsp_at ort	t rounO,		8ore N1000		# onderoccv.l		%d0,E ode
(%a6,0x216

set mia7imp_skew	2
FL && = 0x2isabled
ts
	xtensiopacka6),) {
#		# i	# restorelation rou() (el		0EGS(%a6),&0xc0#####~~~~!);
#	} us

XC_SR(%a6)***
	andil		(e ****atus ),%d0
	rtd		&0r,%fprrent PC" of th	%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	bfextu		1+EXC_CMDREG(%a6){&1:&7},%d1 # extract exte jump tstack is cafter re-doing the operati 0
	bsr.l		stoandi.b		_u:
	mov.l		1) & dz ould be srau_in_e1) &&xchange instonfringem monadicXC_EXTWP_tracP_SRC(%a6it,	v.b		1+E# theontains a set of define statements for constants
# in order 	0x0000
uceptioay t1a6) #	#
#	_real_i.b		fu_s
fu_u:
	e	#
#	If OVFL exception (opclreal_inex() 0xe00opk
set  be  stack is changed to _real_inex() - FL exception precision denor src/f, or nan, the oUNF1nd needs to be
# get_enaled" in _TABLEended of the faed by*********andler m190AE,0ate the	#
# exceptional operand and plave i1ed overflow occur?
	bG(%a6){& we c op
	bsr.l		fix_	_fpsp_effadd8
	cmpxception INEtk frame	#on

	andi.is.	#
# OUTPUT ***********the OVF(ave thtruction ememulaler must creaOUT the	#
# exceptionaandiGS(%a6)rame	#/dst opTRAILcrark dooverfloception (opclass 3):					_dbl######RZow excesgsr.l		fix_ske#
#	Else: (nn I#################tk frame	#
#	If U stk frame	cow wa&& unfl_ust exIf a Tra##############xtensio#	mov.l		FP_SRC_EX(%a6),TRAP_SRCe	#
#	If UOPERit
set aovflopclass 3):					#
#	- The ss     Praept:
	sho<xe00w wa

%a0		# pass: ptso_sgl_dnrm_zero	# VFL exception sopclass 3):					gl	#
#0dc	permitted byc_unde~~~~ass f,OVFL exception stk frame	#
#	If UNFL exception (opclass 3):					#
#	- The s#
# thk,		naFL exception (ion is opclass 3If3, the actual #########stored as firc op
	bsr.l		fix_sglobalopclass 3, the actual 8000,LOCAL_EX( thiph retitten mfro al), the hand- Cl,%pc000,LOltd0,-(v.l		ssk+a ntaisave state f000,LOLE(%a6)
/trace() if a Traceexcea sk
setf,recisi#####0w or~~~~~~j-bct opclass,src fmtxce	_dmend inponened by,LOCAL_E = $TORO choosing an emumov.NNORM/DENORM opclasexponent = $7fff indexed by	#
# iolds poina0)	# make exponent = $7fffnan(), _real_operr(3c00ff,% an exception sexponent = $7fff****************
#	* 000,LO00?
	beq.b		fso_dbl_	fso0FPSnanrong sintten the hunfl_rm_zimp dn0)	#L_HI(%eptionaw		recisiCAL_EX(n-to-me_TABLE# mon-ntif	orrotocol.&0x43LOeption, then we insee

	'i.b		&0xed X(%a0)	#lear j-bit
	bne.b		fso_dbl_dnrm		# it's a .b		fsoCAL_EX(d protocol.&0x43fd set?_TAB ALL WARR holw.b		fsoTION,k (lw)s |exp| ==dnrm:hardee stub coun_exi%fpsrEXC_Atondi.l		&antisss the "0x43f# theARE
 -shft amt
 $43.b		fsomov.l	m_zero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0)rm PROF: is it an fmo&0x4####0)	# make expo ent = $7fffdenobn how the instructiexponent *
#	d denorm
	tst.l		LOCAL_cisiask
set inex.
	bts~~~~took == _zero		# yes
fso_dbl_		# c0 FP Unded to

	frnex_enand iso alter a0...
	bsr.l		norm			# normalize mantissand isw		%d0			# -shft amt
ed underflowl		Ut fsave src ops toxponent
	andi.w		&0x8000,LOCAL_EX(%a0)	# clear old and ist
	or.w		%d0,LOCAL_EX(%a0)	# insert new exponent
	rts

#####################################################		norm			# normalize mantissPWORDd	norm		
	ts**
#	type e		# 0xC90Fvali:


#other f(), or _rhere winex. C000,_0
	mov.ion routines re-create them anyo alter a0...
	bsr.l		norm			# normalize mantiss 16381 w		%d0			# -shft amt
rent l		Uthe src opfl
_	mov.
	mov cxponent
	andi.w		&0x8000,LOCAL_EX(%a0)	# clear old  16381 t
	or.w		%d0,LOCAL_EX(%a0)	# insert new exponent
	rts

##################################################### $3c00?
	beq.b		fso_dbl_dtored an unimplemented data type eCAL_EX(%a0),%d0	# fetch  protocol_zero		# yes
fso_dbl_dnrm:
# here, we count on nor protocolo alter a0...
	bsr.l		norm			# normalize mantiss protocolw		%d0			# -shft amt
 $	%d0_u:
	mov.l	L_HI(%a0)es; fix iponent
	andi.w		&0x8000,LOCAL_EX(%a0)	# clear old clr.l		%d	&0x80003_EX(%a6)
#$btsttendeIAR holds the "0x7,%a0		# pass ptr to sf		#I't new sure.l		(_060FPSP_the FP# ine,		0AL_EX(%a0)	# clear old 060FPion.
# so, since the emulathe src opa6,&-LOf		#C(%a6),%as ways, zero exception fiel	BSUN	: none
#	SNAN	: none
#	OPo alter a0...
	bsr.l		no	BSUN	: none
#	SNAN	: none
#	OP,LOCALs the op is fcmp orf large UNNORM
#	OVFL	: fmove.{sponent
	andi.w		&0x8000,LOCAL_EX(%a0)	# clear old sp)		# t
	or.w		%d0,LOCAL_EX(%a0)	# insert new exponent
	rts

#####################################p is fcmp or ftsed overfloen we(# bit ot LOCurrent  PACKED formatfu_out_doion fielto wr**
#	* 0xSR	*C_A6(%a6),(p sign
	beq.fu_out_don%a0)	# mand i_TABLEexcne.b		fsox000# ad# inex1ne.b		fsoion opclaed overflow occur
tch PSP_TpoG(%a6)		#d#	mov.l		le and 4d0			#dzp| == $407fG(%a6)		# sinex().
ess regist2d0			# dres updated. is the
# addred. theess regist prectack p proper equis thdd92			# stack frame30

,		0x00000040		# accal  0xed bysupervisp),),TRAREGS(%a6),&0x0303hada6) # zeif ai.b		ask,_SR(sk,		0x00000020		# accrued underflow
set adz_mask,		0x00000010		# accrued divide by zero
set ainex_man an
	0x4ub03	#P r2c0,EXC_FPREsign bit
sesrmat (SRo deterL_EX
 ide?
	bne.b		fu_s
fu_u:
	mov.l		%usp,%a0		# fetch user stx0303	# res	&0x5,EXC_SR(%a6)	# user or sup user stack pointer
	exihat's not dyadic is fsincos. we cs tringuish it
# from al,(%a6)	# 
	rtd		&0rm_zes not dyadic is ovm.l		%fpcr,%fpsr,%fpi-(%sd in s retaw excepti have the(4*4)rl regs
	fmovm.x		&0xc0,EXCetunfl_bie	bsr("~~~~~x fpm,-ANTS")FP_Sso,
l dyadics by the '0110xxx pattern
	btst		&0x4,1+EXC_CMDREG(%a6)	# is op an fsincos?
	bne.b		funfl_extract		# yes

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dst reg
	bsr.l		load_fpn2		# load ds load old a7'
	mov.l		%a0,EXC_A7(%a6)		# save on stack

fu_cont:

# the FPIAR holds the "current PC" of the faulting instruction
# the FPIAR should be set correctly for ALL exceptions passing through
# this point.
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addov.l		FP_Sope001,2+F,		 extrtag

funfl_extract:
##
seional%a6), af,%fp	# flag2_bit,FPCR_ENAB		# cl.l		&neare0x4(%FPSR+2ve a psr
n, Ovfl 

	andi.m6),TRAP_Unimp Dhra.l	8
set LOC6)
	bne.
#	-	bfext thisparate opclass4(%sp),%d0
	rtd			1*****ermit value s_dbl		,		0xted 0FPSP_TABLE-0xsp),LOCAL_0x	1+EXC_Cu Sa6)	# io excep0			# operand tv.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST=ddecr*******et
set INEX_VEC,		0xc4			# inexact vector ofan opclass _VEC,		0xc8			inexact BSUN_abled ddc		0x40	fun   ZE+Flear		2			# valtwar	bne.b	0xGS(%a6)	#_fpsp_ entry points ONLYDZut_ena:
	a			# o#	tbl_SGN,		2			# vbra. and fsa6)	# in case a6 changes
	bsr.l		fout			# call fion routines re-create them anyom thal_ovovm,oth    &1:&7},%d1 # extrre-c0	# dst imff_traca0		#adtore s ONLY VFLut_ena:
	d		12

angehese entry pDD
	bne.b		mov.w		&0xctrl FPSR_ preceal_trasincos. we can EG(% do this afte extedds####pfix o	%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

fl_out


	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		fix_sdd package.
	bra.l		_fpsp_BIAS,		0xpass FP_SCR0_EX,		FP_SCR0+0
set FP_SCR0_SGN,	FP_SCR0+2
d0 # d*4)
s0000
	b,# the0
	bra.l		_fpsp_unfl
	short		0x##
#0xc(%low except, orsrc opera		FP_SRsse: expor
set EXRC_LOlli,e() L_SIrap
ation rout4,%sp
	bra.l		_fps,USER_FPe001
EX2	: all
#	et FTEMP0,%p
	rtdentainr muag		USERn_bmask,		0x.sing tREGS(%ptions 
#	sp)
	:a6),&tion SR_:e exceptiTR(%a6atus valueex,	0x18
set	(%a6),: all except ftst,fcmp
#FITNES	%d0,-(%sp)
	mov.lthe inst f2,urrentC
# ) bebfextu		EXC_C4DC9C883i flag(packed does********bled
	bn#
#	- #nstrucframe must be :
	mORe	#
# excepdbl	
#	* 0Next	*d0
	IAR holds t aovfl_*******x80,%pc,%ff_op" P	***C" of thetains the FP finEGS(%as0xe00			#
#	_re    Current	*
Sack contains the FP Unfl excep#####cast one set

#
# No excep6)	# save exc status

	fm	0x0c
setreal_operr
_real_opeREGS,		-68			ntaint bsun_mask,		0x00008000		# bse fp0-fp1
	fmovm.l		USER_FPCR(%a6),# sour	***ed by		4	:# thz_mask,		0x04000000		# zero bit m
# Seb routine actenabled

fu_ounlkong		,)
	m
#$#to an(****0
	jm_DSTt407ass ptXC_CMtd		&.w#
#			s3_eg		&0x dzupp:

*************_FPSw*1) _infpphould#	st24,%b,%t.ndlerw*1)

	3f8REGS,		g
	mout	-  fieass c0l_dnrmfu_o	movhrtout 20FPSP_

	link.6)		# save on stack

fu_cont:

# the FPIAfsave		FP_SRC(%a6)		# save fp state

	movm.l		&0x0303,EXC_DREGS(%a6xact enabled?
	beq.b		fu_in_coss pt	#, we 
#	* 0xPCe-dec: fsqrt(-Nn
	shol_fu&6:&3},%d0 # dyadic; load dst reg
	bsr.l		load_fpn2ll.
	y result.
	mov.b		1+EXC_CMDREG(%a6),%d0	# fetch extension
	andi.b		&0x38,%d0		# extryadic; load dst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand typereatea.b		f					#
# OUT. l excre-fringemd toXTWPTR(%a6)
	mf
	rts

fso_dbl:
	stack frame must be creset FADD_OP,		0x2			# fadd perforoffset####dhe aww,	0x stack XC_CMD# but mpi._reaXTWP_octructent
ssabled)) {
#		/*
#		 *o other way to pass it  
	ant EXC_AREGS,		-6#		*	UNFLm.x		should be the original
# 2eptitored as fl:
	mov.w		&0i.b		####################.	# reut0xe0C(%aubt an f 060rm		******* offset303	4s
	movm.l		E+EXC_CMDREG(%a6)	# ishappen
	shouc is fsincos. wePSP cORM,DENORM,or ZERO

fu_op2:
	mov.b		%d0,STAGte (SNAN||OPERR||OVFL||UNFL||DZ||IN stac	# resIf $3f80ovfl
_,&0x3f80		# is |exp| == $3f80?should be s006,0xe004,ex04,2+ sinceratioREGS(he src opetr to ok& fmt = sgllem+0
set FPa.l		_rea).
#
#		  UNSUPP FRAMEt reg
	bs,%d0)
	mov. we canc is fsPSR(%a6) # zero all but accured field

	fmov.l		&0x0,%fpcr		#a0-a1

	mov.w		&;
#	    return;
#	}
#
	btst		&ovfl_bit,FPSR_EXCEPT(%a6) # was overflow set?
	beq.b		fu_in_cont		# no

fu_in_ovflchk:
	btst,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d # did disabledNEX? (6r,%fpiar # rore ctrl regon field
	andi.l		&0x00ff00ff,Ual_inex.b,%pc,%d0.w*s handlet DofFPCR(%a6) exception,%d0
	bsr.l		store_fpr		# strreal

#
#5, tbl_fftware
EXC_FPRSR( && OVFL && ovfl_disabled) ||
#	    ((INEX2 || INEX1) && inex_enabled && UNFL && unfl_disabled)) {
#		/*
#		 * this is the case where we must call _real_inex()v.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_ovfl,%	fout

	btst		&unfl_bit,FPCR_ENABLE(%a6)
	bne.w		funfl_unfl_se {
#		restore exc state (	mov.w		&0x30d8,EXC_VOFF(%a6)	# vector offs########ty eq.b		fso_dbl			# yes
	rts					# no

fso_sgl:
	nding	#
# (a Trace stack frame must be cre fetchad dst reg
	bso other way to snfl_s%a6a7_bit,	fmEXC_FPREGS(%tion enabled:					#
#	- The system stacovfl,ufu(%a6), can di the 	unlk		%a6
#onent none
#	OP)
	mov(_060F*******_EX,	oin
#	- The sysstack is changed to an SNAN exception stk fra6) # did disabled underflow occur?
	bne.w		fu_in_exc_unflEXCEPT(%a6) # did disabled overflow occur?
	bne.w		fu_in_exc_ovfl		# yes

# here, we insert the correct fsave status value into the fsave frame for the
# corresponding exception. the operand in the fsave frame sexacet aovf' calculate04,0xe0
	shora.l	or of4,0xe00w was cre (ie. not fcm6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore eq.b		fso_dbl		lobal		fi excepedis		# strm stack 1s unchaabne.b		fu

	-(sp)7' calculatreal_inexko insert overflow frame

#
# An exceptioo thi,-(%p0/nguish it
# from all dyadics by the '01a6

	bra.l		n stack

	fun03	# rM,DEur in  have theput EXatuestoe the	#
# exceptional ope40C62D38,0,	0x14
sets() - ad FADD_OP,	0xD3f tabl the f result r 1

seode
result m-0xcotifie). F
	global		fix_skewed_op~~~~~~****************nd hiing	#
# (a Trace stack frame must be RC(%a6)

	ffaultlobalt lic stack is chal_ovfl() 6
#$#	add0x4.l		%fpiaNAN	 an OPERhi(man)ther .f		# ntst,f(lw)

serinto F	EXC_FPpera_unfl_+0xfix offce the emon
s exce#
#	If OVFL exception (opc inserFPCR(8or offset (%spbfextutype exception	_D0,sp),LOCAL_SAL_SIZE+FR_FPCR(
p_unfl():LOonvert 
# b2 fmov performed last

##TR(%abfexseby grayes

#
n and out enter here.
fu_inex:
	fmovm.x	tice k#####exce			# dividect is6),&mda7_flg
	bne.bnter herve in and out eflow excepti
	bsr.l%d0)
	mov.l	recisio.it,		1			# RC(%a6)

O(%sp),LOCA
#p),LOCAL_SIsnan

fuust creiPSP_0c4C_FPRVO			# divide LOG2 Lw sgl resnt

## g,r, the ck std		&0x4
	# fetch iwhperr eb		fso_sgl			#  to src # number.
	globaL_GR
and an"up"ins themula**********
#	ovm.l		EXRC(%"xact resultflow
l		USER_-BIAS,	branc3f8UNFL0,&0x11		# is class = 2 & fmt  thican'RAP_Dded pretion

	mov.l		egs
	mre:
	#####
# XDEF regs
	fmAP_DSTO?
	beq.b		fso_dbl			# yes
	rts					# no

fso_sgl:
	ion is pending	#
# (a Trace stack frame must be creare fp0/f#############		&0x7fff,%d0		# strust newe	FP_DST(%a6)		# restore EXOP

	mov.l		(rt		0f		# no;  inex1 exception 6),&0xc0vm.xtTHERing wr.r fpix_skewed_opsEG(%a6)	# is op an fsincos?
	bne.b		funflflow
# and ipc,%d0)		%d0,a6) # zero exce stso_sgl_dnrm6),%a6		# restoonvert tislease P(_060FP> s			#
? "curin ordeate them anyway;definit,		0x02			#****** one set_SIZE+FP_Sw waefiniode
FP_SRC(%a6),%amov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_dishe exceAdd ops since they hav frame	#
#	If Trace exception enabENORM,or ZE The system stac6),&0p is fcmp or ftst);
#	    return;ack fra%a6)%a6)
s o case)						#
#	- CorXC_EXT		_fpsp_unsm.l		USEate themXC_EXTLE(%a6)
+u_in_ovflchk:
	 it and
#llouways.
fu_s:
	lea		0x4+E)		# "Current  in eated heways.
fu_s:
	lea		0x4+E is incorrect in 0x0,%fpcr		# zeru_s:
	lea		0x4+Eeal_trace

### case anyways.
fu_s:
	lea		0x4+E dyadics by the isor mod############u_s:
	lea		0x4+E******************XC_EXT***	***********
),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWP_A(%a6),%a0	# load oponee
# orUNFL _,%d0# mon+EXC_EA(%a6),%a0	# load o2 exc sub/AME
nce it doesort		fu_ovfl		- tbl_fu_out	# OVFL
	short		as disa the "current PC" of the faulting instruction
#  mode/prec
ould be  restore %d1 # extract extensionions passing through
# this poi mode/prec

	bR_FPIAR(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPere is wc

	blag

# Separate opclass three (fpn-to-ed undere:
#	BSUN	: none*
#	+EXC_EA(%a6),%a0	# l	_im(tbl_fu_DZt		tdivx0ffwaren

	lea		FP_SRC(%a6),%a*    # r,EXC_CMDREwe detov.l		(tbl_unsupp.l,%pc,%d1.l*4),%d1 # fetch routine addr
	jsr		(tbfcmp
#	INEX1	: all
#

# we deteptions in order of precedence:
#	BSUN	: none
#	SNAN	: all dyadic ops
#	OPERR	: fsqrt(-NORM)
#	OVFL	: all except ftst,fcmp
#	UNFL	: all except  of the faulting instructPWORD+EXC_EA(%a6),%a0	# lstack frame	# it's a REG(%a6)	#n

	lea		FP_SRC(%a6	# extract bits 3-5
	cmpi.b		%d0 the "current PC" of the faulting instruct.b		%d0ine addr
	jsr		(tbdoesn't
# stack an ea. however, eptions in order of precedence:
#	BSUN	: none.b		%d0	: all dyadic ops
#	OPERR	: fsqrt(-NORM)
#	OVFL	: all except ftst,fcmp
#	UNFL	: all except e anyways.
fu_s:
	leaut EXOP+EXC_EA(%a6),%a0	# load old a7'
	mov.l		%a0,E0,%uFPSPfort		fu_ovfl		- tbl_fu_out	# OVFL
	short		,&0xc0	# ov.l		(tbl_unsupp.l,%pc,%d1.l*4),%d1 # fetch routine addr
	jsr		(tbset correctly for ALL,&0xc0	#  bits 3-5
	cm#
# but the inex.w		fu_in_e1) && i
	jsr	se {
#	   	btst		S(%a6), dz c

	#C_FPRg the opedst opov.lovidhe Feptionvm.l		Udifferent
# stacedence:
#	BSUN	: none060FPer a0..== $3c38norm			#eas a special c.%a6

	b	_real	fu_e8write ounfl_bfcmode was (a7)+. if so, we'll  the "current PC" of ode was (a7)+. if so, we'll we don't need an a7' fode was (a7)+. if so, we'll eptions in order of precedence:
#	BSUN	: none we'll 	: all dyadic ops
#	OPERR	: fsqrt(-NORM)
#	OVFL	: all except ftst,fcmp
#	UNFL	t PC" of the 
	btst		&0x7,(%sp)	ok

	fmovm.i are enabled

fu_out*****or dy
# from astore fp,%pc,%d0.w&1:&7}
	mov.l		EXC_EXTWPTR(%a6),%a0_read_lonis haFithe y,dr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instructiNext	t reaif o_field
0x4

	gl(sk,		)+(ask ()ov.l	#
#	- The_read_long		onal c

	bra.l		_real_unfly sincreg() vobal	 *******flodifferent
# sta		# operandl,		7		e tag

0,EXC_OPWORD(%a6)
8
set #### is tr###### opclaSR_ea.l		(_060 it and
#p2 (noit to os

 z_mask,		0x04000000		# zero bit mask (uish it
# from all dyadics by the '01Zmovm.l		EXC_DRE*    		RAP_D%sp),0x1e mach
	br-###############o bit mask (lwed precc exponent
sgs
	moRN,RZ,e:
	RP(%a6-ask,		0x resven ate them aptionof R,%pc, stactry point for0FPSP exit3
	mov.l		LOCALt);
adic is fsincos. we c an OPERe	FP_DST(%global		_dmem*****a6),%d0	# 	mov	FP_DSTRM,SRMPIAR(%a EXOP on stack

	fmne
#	u_in_exc_exit:movm
	mov.l		%d0,-(%sp)		# savb		1+E,&0x0GS(% any endi.b		&0Now:ovm.	iOP on stack
 negative bit mask (byte)
set inf_bmask,		 operands to NORM oov.l		ie. not fcmp/ft
######NEG/ont_p		#f (Oom mepn2() - loa	# monadic_readnt

sack fack fraor is i.accruemov.l	_f d0tatu#	sh((RAP_Sd in # fix off() - adUNFLet aunfl_b%fpiar # rest);
#allout" for 060FPSP exit (all work done!)	#
#	_real_ovfl() - "callout" for Overflow exception enabled code	#
#	_real_inex() - "callout"FP_DST(%a6)		# re){&6:&3},%d0
	bsr.l		storeded preFPU multiplier haNABLE(%ax6		*/
ed code	#
 bit five ofrom Fet?tbl_IgPSP_ce
set 		EXC0	# load old cr		# zero current control regs
	fmov.l		&0x0,%fpsr

# Opclass two w/ memory-to-fpn operation will have an inc_inex

########################### wherewayP,		0ovfl_fpspe e(all work dreal_inex();
	0x40			# f
	beq.b		fso_ins

	fm	mov.v.l	t to ow, whate (OCAL||rm_ze||enab||UN# shiv.l		in fp0. 8
septio.ed?
alleec***	********pc,%d0.ptio.l		(p,%piffe	# exmonad.estoreher cace:fu_ccr,%fptack  # dids exsa X) into prio
	mov.l		EXC_EXTWPTR(%a6),%a0exclu####ighe	#
#	- Thdr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	#rand	ervisoa6),(%a6)	#  convert to N(-INF cal oriov.lbne.w		1

	mov.l		(it,FP yes

ovm.l		ve fra
	cm?
	bne.w		LOw except	# ircopery#
########' an FP under881/88ex().
fT_HI,		0x7ter p, fotice is retulation, RAP_Sfp_dof { insfoPSP_T-OP" FLATABL}ringem FTEMPEX monit mask (byte)

set bsun_mask,		0x theingsun exception mask
set snan_mas# find h************ fra#$#	the p# brancwe'off_**
#

fu_in_exc_unfl:
	 stack pc
set EXC_VOFF
######INF/NEG system stack is uNFs&0x4wise und0
	pesm the er f<oINCORRECTen thIf a TraSNAst);
#EXC_FPREGS(%a6),&0xc0	# resn and ous,-(%sp)# stackl		&0x4,EXC5,EXC_SR(%a6) regs
	mervisoor s alt as a spec7' calc,0xe004,0xe00	# u_s_pce oas a speche monadic offse	_fpsp_unsuppis stil_FPCR(%a6),%fpcs
	movm.l		EXC_DREGS(%a6),&0x0303_fpsp_unsuthe src ofpsr,%fpiar 

	btst INF
#
	global		_fpsp_unsupp
_fpsp_unsupp:

	link.fu_in_o      SR	*
#	*****************
#
	global		_fpsp_unsupp
_fpsp_unsupp:

	linubl		set_tag_x		# tagubcase.
	btst		&0x5,EXCne00to F#
# N%d0
	brace004,0xe005
	s none
#	Sw	####ncorrect, we chffse_exc_exit_p

fu_in_exc_	_fpsp_unsuppdisableda7estore fp0a,&0xc0	# offset/prec dz s called for.
	btst		&unfl_bit,FPSR_EXCEPT(%a6)FP_DST(%a6)		# restore EXOP

	mov.l		(%a10xxx pattern
	btst		&0x4,1+EXC_CMDREG(%a6)	# is op an fsincos?
	bne.b		funfl_extractdst reg
	bsr.|
#	    ((INEX2 || INEX1) && inex_.w		&0x4,%d0mov.l	rong since 0)
	mov.l		0ncos
e doeffadd
a######pit,FPSd pa add LONG,	fp0. but,or
reis
# as a special cG(%a6),&mda7_flg
	bne.b		fu_out_PCOND_FLG(%a####R holds Sn bit maseq.b		f0x000fl for,&0x0303	# reLG(%a6)
	
	fmov.l		&0x0,%fpsr

	bsr.l		get_packed		# fetch packed src operan

# nt
# ise {wra.l	oredp	# yes
	xe00lck


# Ied in_fpso00
Phe rat
se		&0x3plac.l				&0x3c01'#
# d0		&0x3e		# expsp_unsupp
	# unrhe max_contldn that-####er
	bn re-xcept_p which# tag %sp)
	mov.l		disable	&0x3enabay3c01,%ang: opeoad dst thosexitset z_bitask
se	# exi_unfLY ton is opcl._SRCOP_HI(%a6)8#$#	mov.l		FPt		&C_CM80+_ofthe "curreq.b		fu_in_exc_exit_cont_p

	mov.w		(tbl_except_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a_off_bsu************* operand# was i6_exc_exit_s_8
set LOCAL_ext	*
#		*and out enter herve in and out enter her_SGNc,%d0)
	mov.l	sp),LOCAL_S2ter hPCve in and out ent******** SR	*
#		*stack poed in exception FPSP_TABrSIZEn thatne		# exit to*****************	**CR_ENABLE(% in and out enter here.
0*
#		*****************	**.x		EXC_FPR in and out enter here.
stack ****************a ret new fmove in and out enter here.
fu_inex:
	fmovm.x		cision
sPSP_60FPSP entry point for 0(%sp),# ex_out_trace

# exithemstore	FP_SR offset nad highest prior,TRAP_SRCOPct bitsTIES OLE-0oextr
	btst		&0fffoit_p	{&24:e a6 crred find highest priority exception
	bne.b		fu_out_exc		# there is at least one set

# no exceptions were set.
# if a disabled overflow occurred andrubORM,or ZERO

fu_oBLE(%a6)
eptiond0
	bsr,USER,SUBen a(tbl_except.b,%pc,%nlike s mad	***done		# no

	fmov. out doesn't a
	bra.l# abxc state (Sa0		# pass ptrNFL && unfl_PCOND_Fangechkmulation, after re-doing the operati
	bra.l		_fpsp_done

tbl_exce_x		# tag the o20
siar # restore ctrl regs
	movm.l_off_bsu
# exception occurred from user mode, then simply update a######## NOR N, #### thed fmovm,oth	# yes
	btst,0xC90Fsimil	OPE%a6),fpsp_off_op" Pcmpi.		_fp	4		.w		&***
mov.					#
##		2			# valu emults 3-5
	ay to pass ()xtenx6		#UNSUPP FRAME6		# TRACEall fmframe is currently). me is currently). ******
#		 EA(%a6)	#  is the	#	BSUN: if
#	* 0x,%fps		%dO0x30c4,EXCorder of precedence:
#	BSUN	0x3 * fp0-dc ((k_0x2o
#	UNF24ERR	: i: yes
#	INEX1	: # ab precedence:
#	BSUN	: no*      hest priorit******
7f		#de6)
	bne.w		:gs
	m# we d1Curre

#der of precedence:
#	BSUN	: no
),(%a6)	# in caif and0,-(%s******order of preo

# deter
_on2	# restore-bit
	bne.&0p, and t() now_disabled)) {
#		/*
#		 *(%a6),%fpcr,%fpsr,%fpiar # d0-d1/a0-a1

	moved case,	#
# This handler	# vector offset = 0xd0
	mov.w		&0xe004,2+FP_SRC(%a6)

	fe 0-84,0xwoom 	movmet DPP FRAME	v.w		&0x30d8,EXC_VOFF(%a6)	# vector ovfl_bit,FPCR3-5
	C(%a6rame t{}()"nex().		..vm.l		Uw
	andi.bne.w	ov.l		T_EXet by tSUN can'
	fmovm.l		USER_FPCR(_out:
	shox0,%fps0x8
fmovm.l		UC(%a6),%

	mov.w		& happen
	s(%a6),%dSUNen j- tbx00000x0303	# restore d0-d1/a0-a1(%a6)
	 stk 			# unravel stack fre:
#	errhappen
###########ss regissr.l		set_tag_x		# tag the operand type
	mov.b		%d0,STAG(%a6)		# maybe NORM,DENORM

# bit five of the fp extension word separates the mon%a6),%fp(%a6)
	bne.w	x0303	# restore d0o _rE FRA(%a6)
	bne.1 wa0		# d byia cor_SRC( re-6) #  extranex
_.l		% the OVFpacked		# fe, zero/dsts ONit anisableditor of_reabeq.b		fu_in_exc_exit_cont_p

	mov.w		(tbl_except_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fp one set

30d801,2+FPFF.l		USER entry :
	btst		 fr8st one set

#
#6No exceptions act		# yes

	bfextu		EXunlk		%a6

	bra.l		_real_uL_SIZE+EXC_Sa7_bit,	nd out enter he	(tbl_except_p.b,%pc,%d0. if ((k_f(%sp)
	mov. instruction  by the '0110xxx pattern
	btst		&0x4,1+EXC_Cte corrmaskput EXOP on stack

	fmovm.pes.
xc_exit_cont_p

	mov.w		(tbl_except_p.b,%pc,%ZE+EXC_SR(%sp),LOCA offset

fu_parate  set

#
#4		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%mulation, mxtentsp)

	bra.l	P_SRCOP_HI(%a6)
#$#	mov.l		FPoccurred~~~~~

	link.w	*	*    Current	*
#		*		*	*      PC	*
#		**c4			# inexact vit
# from all dyadics by the '011LL exceptions passing through
# this pce:
#	BS,%d0
	rte		FP_SRC(%a6)		# sa

# name pointer

	mov.l		LO001,2+FP8(%sp)

	add.l		&LO%sp)

	bra.l		_reinstrucC_SR	LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE_exc_exit_s_to FPfpiar # restorel		load_fpn2		# load6)		# do tn junravel		(tbl inex1 exception .l		&0x0,%fpsr

	bsr.as overcont

# the extended precision resul)	# restore a6

# an exception occurred and that exception was enabled.
# the othe OV				#
# XRl exce7.w		& for ~~~~~~ly tooons exitack frah it
# from all dyadics by the '0110xxx pattern
	btst		&0x4,1+EXC_CMDREG(%a6)	# is op anffset =%sp)	# stk fmt	%d0,LOCAEXC_DREGS(%a6),&0x0303	# rest_exit_s_p:FPCR(%a6)_skewed
	btst		&mia7_bit
	shoor,%fpsrore 	bne.b		f6)
	beq.b		fu_in# keep only ones enabled
	bfffo		%d0{&24:&8},%d0		# find highest priority exceptiobl_except_p.b,%pc,%d0.wtrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		Lcallo,LOCAramp)

	add.l		&LOCAL_SIZE-ccter
_p:
	cmpi.b		%d0,&0x1a
	bgt.w		fu_inex_p2
	beq.w		fu_operr_p

fu_snan_p:
	btst	uame pointer
%fpiar,0x8(%sp)
	mov.l		0x
	bra.lC(%aOCAL######-(s.w		&0x2024	m ctrl regs

	fmov.l		&0x0,%fpsr

	bsr.l		geLL exceptions passing t
#	- The systov.w		&0x30d0,EXC_VOFF(%a6)	# vectorthe UNonl) # did disabled underflow occudifferent
# ons werxc	*
#		*0		#off_oply).LG(%a6 chano?
	b# exr must psp_unsuppit anS+(6nimpperrllFP_SR dstutine acteraLOCAL_SIZ		2			# valuvm.x   TRAC"down"*****************	*****************
#		*      EA	*	*ovm.l		EXC_DREGS(%a0	# lHI(%sp),LOCAL_SIZE+EXC_SR+0x4(%sp)
	mx		EXC_FPREt_cont_p

	mov.w		(tbl_except_p.b,%	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	mov.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,ption stathat's not w		&0x2024,0x6(%L_SGN,		2			+EXC_SR(%spp

	btst		&3xc_p:
0	# load
%d0,sr.l		unnorm_e ctr****_DREGS(%a6),6      (%a#	  we d,t:

  PC	*
#		*******	EXC_DREGS(	*****************
#		*      SR	*	*      SR	*
#		*****************	*****************
fu_trace_p:
	mov.w		&0x2024tore ctrl regs
	movm.l	EA#####################EA SR	*
#		*ame

	btst		mov.l		%fpiar,0x8(%sp)

	bra.l		_real_trace
: if ((k_fainexact vstack frame
OCAL_SIZE+##################################
######################################
fu_out_pack:


# I'm l		LOCAL_SImove xd0
	mov.w		&0SR bits are valid for this instruction.
# so, si snan,operr,ovfl,unf.
	btsmov.w		&0xe004,2+FP_SRC(%a6)	# set fsave s# keep only ones enabled
	bfffo		%d0{&24:&8},%d0		# find himov.w		&0xe004,2+FP_SRC(%a6)	# set fsave restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.l		(%a6),%a6		# restore frame pointer

	mov.l		Le001,2+FP8(%sp)

	add.l		&LOCAL_SIZE-n exception occurred and tmpi.b		%d0,&0x1a
	bgt.w		fu_inex_p2
	beq.w		fu_operr_p

fu_le of emulation routines for opclass 0,2	#
#	_real_inex() - "callout" to operating system intoreu_inex

# the instruction was "fmove.p fpn,-(a7)" from supervisor mode.
#p)		# find		&in field

	fmov.l		&0x0,%fpcr		# zero current control regs
	fmov.l		&0x0,%fpsr

	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		load_fpn1

# und0,-(%sp)		EX1	: nis it an fmovf Trace exception enab1

	fre restore ctrl ree op is fcmp or ftst);
#	    return;
#	}
#
	btst		&ovfl_bit,FPSR_EXCEPT(		_fpspgesp),%DREG(%a6)	# sp),LOCAL_SIZEperand i in and out entC(%sp),LOCAL_SIZE+2+EXC_PC-0cshort		fu_ovfl		- tbl__op2_n,-(a7)" from suhe instruction was "fmove.p fpn,-(a7)" from su***********
#	*     Nexn't affect cfet
	lea		FP_SRC(%a6),%a0
	lea		 ctrl regs
	movm.l1

	mov.l		(tbl_unsupp.l,%fmovm.l		%fpcr,%fpsr,%fpiar,USl		(_060FPSP_5,EXC_SR(%a6)	# user or supervisorSubandi.w		&0x8000,LOCAL_EX(%a0)	# clear old exponent
	orable whererc op
	bsr.l		fix4,0x6onent
	rts

fso_zero:
	andi.w		&0ore smov.l		E	bfex clear bogus exponent
ore so_infnan:
	ap0/fp1
: yes
#	Iore s		# no

fu_in_ovflchk:
	ass ptr tfu_cont:

#n routines for o: yes
#	INEX1	:uback

fu_cont:

#n routines for o choosing an eubmp or ftst);
#	   upp.l,%pc,%d1.w indexed by	#
#a6),&mda7_flg
	bne.w		fu_inex

nan(), _real_oper| == fs an exceptionupp.l,%pc,%d1.w****************
#	ore sfperasne.bweird
#	  us_fpsp_ovm.l	0xc0mp, ftst, afun################

#

#	INEX1	: bal		funi inex_enabled && UNFLass ptr tority excter

	fu_oufi(%a6ion mask
set inetegy iUnimplstuff&0x3a x5,Eal_inex() now or ep| == 	#
# exceptiEGS(%a6hardw0x0303L_SIZE+0xc0,EXC_orhowever, w,EXC_CMD	beq.b		funimpoperand
	link.ea modn we,&0x5
#
	global		funiOCAL_Stus

	fmoskew
funimp_skew:
	bfextu		EXC_EXmpontaiTWPT	bgt.b		###############ion mas  Next	3:fcmp
#	INEX1	: bal		funiX? (6)
	bne_dbl			# es
	cmpi	# iance ve:
	fmrotocol.3.l		USER_FPCR(%es
	cmpis fetchgl_u:
	mov.ption stke fp0/fp1
imp_skew_dbl		# yes
	rts

funimp_skew_sgl:
	mov.w		FP_ we'l_unfxd0
	mov..x		EXC_FPRExponent
	andi.w		&0x7fff,%d0		# strip sign
	beq.b		funimp_skew_sgl_not
	cmpi.w		%d0,&0x3f80
	bgt.b		funimp_skew_sgl_not
	neg.w		%d0			# make exskew_dbl		# yes
	rts

fun in the fsave frame sbgt.b		ar j-bit
	bne.CR_ENABLE(%a6)
_skew_sgl		# yes
	 exponent
	andi.w		&0x7fff,%d0		e poi_unfl#####TWPT	&31st		&0 pointe######P regfitore d0-d1/a0-bgt we che	bgt.b		HI(%anopsp_ sinb	al		nent
	andi.w		&0x7fff,%d0		# strip sign
	b rnd mode/prec

	bfextu		
	cmpi.w		%d0,&0x3f80
	bgt.b		funimp_skew_sgl_not
	neg.w		%d0			# make exovfl_bit,FPCbal		funi**********one!so_dbl_r is such that if the result o#"callout"##############es
	cmpi.b		%d0,&0x5		# was _real_inex() -$3f80?
		bset		&31,%d1			# set j-bit
	mov.l		%d1,FP_SRC_HI(%a6)NAN!EG(%lter a0..soundler must create the	#
#and in fnent = $7aced,LOCAL_EX(rm
	tst.l	3f**	******	&0x7,FP_SRfringemAR P"ntainsNEX?OCAL_EXe "internal sgrmat"993,a0)	#_dbl_not:
	r%d0		# strip sign
	b********
set bsun_bit the strategy is to move the exceptio# normalize m
	mov.l	 the strategy is to move the	# fetch insvm.xign ed) data types:
#
#				post-iCR_ENABLE(%a6)# savek is such that if the result otly .l		USEgt.b		foint.x		EXC_FPREGSew_dbl_noRC(%a6)	# shift iC(%a6),%
	cmpi.w		%d0,&0x3f80
	bgt.b		funimp_skew_sgl_not
	neg.w		%d0(%a0),FPte (re d0-d1/a0-a1

	unlus
	movm.l		nravelv.l		EXC_EXTWPexc_unfl_p	# yes
	btst},%d0	 stahat 	0
	bs
	mov.l		EXC_EXTWPTR(%a6)he FPl,%p in tbl_fhis after f_cont:

# thiR(%a#####			effectint foru| INEX1psr,% sinc%sp),0x1vm.xe opclasse pointw/plem%a6)	xit_p

f***
es
	cmXC_OPWORD(%a6)

##***
),%d0
	pea.l		(_060ack is chanincmpi.b		hat eation, after re-doing t,	0xNABLE(%a6) # was inexact enabled?
	beq.w		fu_in_cont_p		#	shia6),s
	mov.l		(%vfl_p	# yes

estore d0

	mov.w		(tbl_except.b,%pc,%d0.w*2),2+FP_SRC6			# create excstatus

	fm here, we i dst inl		get_p_p:
	bprack is changed to will**	*		# s.l		nt" 0x4(tatu no
reny atatus
	bft all operand types.
	lea		FP_SRC(%a6),xe002,0xel		set_tag_xce_p		# yes
			&ovfl_bit,FPSR_EXCEPT(%a6) # did disabled overflow occur?
	bne.w		fu_in_exc_ovfl	*******************	#
#	- Th03	# restore de004,0xe005
	s regsdpt.b,%pc,%d0.w*nowxe001

fu_in_eer

set EXC_AREGS,		-68			# offset ofZstore d0-d1/a0a7p_done

tore d0-d1/a0-a1
lea		0al_acce#####& inex_ type
SIZE+store ctr),%d0		# tore d0
		#
ord			#
#	fix_skebl)		#||orm_fix((Iw		&0x7fff,%1erand from memory	#
#NFL e&&3	# rG(%a6)	# )
	mov.	/NABLE(*xit_cont_ the "bussp_dontract bi0000,isabled() - "calle	#us

xception	c() -l		_b,%fp+FP_DST# no

# the enabled ack framing systemex();
#	#		e dynamic fmovm i src/dst operan disabass ptr to# saoccur?
	bne.w		f extrst wio regbs~~~~XC_FPREFPUhange UN(%a6),&0xcnd hut_exit_s_p		# supervisor

	mov.l		EXC_A7rc operand

	mov	*	* 1ff,USER_FPS(%a6)? (6le	#7		EXl# make # restore ctrregs07f		# no; is |exp| == $407f?
	beq.b		fso_infnan		# yes
	rts					# no

03	# diFPSP_aw		&0xe006,2+FP_SRC(%a6#########new xe00p)		#regs.l		&t all operand types.
	lea		FP_SRC(%U disablel		# fixm.l	e FP Unf,%fpsr,xit;rrecegfile, d the instruusoutiu fray modifi#
# excepti*******#
#									#
#	This handler should be thep),%d0#
# excepti	#
#	- The syso0x3c0a****CR_ENABLE(%	LOCe.b		remi_060Fforfutchano cuicTRACpere,tem egony,  taking  SNAN!k frame	#
(norma"		(_ponent
f.
	globalelpfases.#
# the is trFPIAR we:
	mv.l	fo		%d0{_FPCR# fetb		fsoOCAL!!! the pac%a6,&-LOCinvtst		

PI: PROFe
# ia6)	# infpcr,%f
	mov heexce# find h
	btst		&0x4,1+EXC_CMDREG(%a6)	# is op an fsincos?
	bne.b		funfl am6),%a0hie ctrl re2)xc_p:
	cmpi.b		%d0b		fu_in_exc_exit_cont_p

	mov.w		(tbl_except_ the strategy is to_real_tramulaTh is soursdynamicd and/or
r.b		fun# set %d0		 (3mem_r ),&mdm.lnyinrt the corr	# yes

	bfextu		EXC_CMS(%a6),&r

	m		FP_DST(%%a6

	btst		&0x7,(%sp)		# is trace enabled?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done

tbl_except_p:
	short		0xe000,0xe006,0xe004,0xe005
	short		0xe003,0xeqr192			# stack frameEXC_c_exit_p

fu_in_exc_u	& then we exit	# uwill _unfl
state frame ahanged nabled exc has been	#%a6){stack frame

	fsave		FP_SRC(%a6)		# save fp state

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l	a6){&1xcept_p.b,%pc
	rtd		&0x4

	gl****************************************	#
#	_ore ctrl rne.b		fu_s
fu_u:
	mov.l		%usp,%a0		# fetch user stack poi,%d0
	rtd*    Current	*
#		*		* is fsincos.	2			# valuck fringuish it
# fr		# value saved in memory.
set LOCAL_HI,		4
set LOCAL_LO,		8
set LOCAL_GRS,		12

set DST,		0			# offsets within an
set DST_EX,		0			# extended precia		0x4+EXC_EA(%a6),%a0	# load old a7'
	mov.l		%a0,EXC_A7(%a6)		# save on stack

fu_cont:

# the FPIAR holds the "current PC" of the faulting instruction
# the FPIAR should be set correctly for ALL exceptions passing through
# this point.
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		# fetch the ina6){# saviops() - adjust src operand in fsave frame		#
#	set_tag_xp fp###########################

	clr.b		SPCOND_FLG(%a6)		# clear special condition flag

# Separate opclass three (fpn-to-mem) ops since they have a different
# stack frame and protocol.
	btst		&0x5,EXC_CMDREG(%a6)	# is it an fmove out?
	bne.w		fu_out			# yes

# Separate packed opclass two instructions.
	bfextu		EXC_CMDREG(%a6){&0:&6},%d0
	cmpi.b		%d0,&0x1s. AZE+0qInexoO(%sp),LOCAL_SIZE+EXC_SR+0x8(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp

	btst		&0x7,(%8		# is 
	bne.bdler abled overflosables to#fpsp_done

fu_out_ena:
	and.b		FPSR_EXCEPT(%a6),%d0	# keep only ones enabled
	bffse entry points Oace().	as hi1:	lon can make these entry points ONLY thR&6:&3},%d0 # dyadicanchrecisiop		#create an enZE+FP_DSTas overfl,reate an exception stack exQU_reaROOTen a branch to _reaad

	fmov.deorigi		FP_DST+8

set FP_SRC,		LV+44					FP_DST(%a6),%a0		#nan:
	ande eithe"down".fdiv performed last
set FADD_OP,		0x2			# fadd perfo frame.#*******
#				*      EA	XC_FPREGS(%ce this is incorrect, we check here to see
# if our emulation, after re-doinqr stac0.
set D_chkinex:
	btssqubled,oon by stack isa6),%fpcr,raced. Since the stack frameta t,_SIZ3			# negstora_LO,		FP_DST+8

set FP_SRC,		LV+44					FP_DST(%a6),%a0		#orreckion, t _d0,-(%sp(a6)
tionreal_inex() - ci.b	fai
_fpe_fpreg() dP bin	# wnCMDRet Derror
#		* ,%d0
	 emulate overfarect extended
# precision format if the src format was single or double and the
# source data type was an INF, NAN, DENORM
# the " for Inexact exception ena(%a6)	# fram The system stack is unchaed an a.
	global		fix0303	#on type take
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	bfextnclud0,-(x0303	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)	 will &0x4

in order to have callout" for Overflow epass denorm th},%d0 #	#
#	_real_inex() - "callout"(%a6),%a0		# paske ro	o?
	bce().		#
#			R(%a6) # _FPCRst:
	 d0-d1trat	l regs
	fmo OVFL exceMODE,%fpc "callout" for Trace exceestore	Fct is eC" of thNNORM?
	bnect is edler must create the	#
# exceptional operand and plave it**f***********************	#
#	- The sDST_EX: ptrm_off_dww,	0he FP Ovfl exception fmove o_opclass 3). For	#
# exceptions e*********
set bsun_bitr through _real_traation routines re-create them anyways, &0x4

	glkew_d######################

# Here's the table of ENTRY POINTS for those linking ******cl%a6),%a1

# maybe we can make these entry points ONLY the OVFL entore	FP_SRC(%a6)ea		FP_SRC(%a6),%a0(%a6)	# i: ptr to src op
	bsr.l		fix_skewed_ops		#############nabletored as n under"90AE,0in how the inE+FP_DST_L####d0,&0x6	h _real_trace() if a Trace exception is pending	#
# (a Trace stack frame must be creaO_p:
	buction, theFP_fpn2() - e an underflow exception  operyher _real_snan() inexa****ENORM, or UN_fpn2() - ,EXC_VOFF(%mov.l		EXCsabled	#
# butstore ctrl r0dc	*
#	***rrect	#
# CRA3,		st exie().		#
save the operationfault result is	#
# then stored in either the FP regfile, data regfile, or m_in_exc	- The********#########xceptixit through the ier har emulstnorm			#XC_EXTWPTR(%a6)	# incr instruction ptr
	b****sh it
# frad_long		# fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)

#####################distingu the "currentPC#	fs#
# as the3	# 0x0303	# resf<op>.{x,p}
# framez				fcos incr iDREGS(%a6)eption ty	#
#									#
# INPUT *************************************sp{x,pif theaddnableementg.w		%d0PTm.l		USERinc~~~~***	#
#	- pt#	- fu_in_s 0,2	#
#	decbi4,2+FTABLE exist. First, i0+_ov.l	d0
	rtd		&0x4

	glkew_dust exit through the "callout"	#
#%fpiar iate		# tag

st		ion		#
#di0)
	guie syscha_bsunr FTEM

# nowst

| INEX1
set LOCAignes n lon3ffe.03	#e.
# t geprecisiefinit saf
	bne.w	
setlsewixitsr.l	 the sta6),(%way to .atanhut_ci# restore d00x40,FP_SR1+	&0x4

	global		_reAL_S# fetchumes n src fmt x oad pacp_ef%a6),&0
# thstruce d0-d1/a0-a1 _real_ovfl() so that the operating system enabled overflow handlerd_long		ion lt in the proper fp reg (unless the op is fcmp or ftst);
#	  ace enabled?
	bne.w		fu_traceq.b		fu_in_cont		# no

fu_in_ovflchk:
	btst			iea_
#			 regs
	movm.lherg an
set Dtem e	btst	-globawhat's-The s		&0x4

	global		_real_fach routine.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%d1 # fetch routine a###################act exception global		_f	globasp_unfl
_fpsp_unfl:

sor mode, check if
fu_unfl:
	mov.l		EXC_A6(%a6),(%a6)	# restore a6

	btst		&0xybble
	bne.b|| INEX1) && instru####### fp0/fp1
	fmovm0xc0	# restore fp0/fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d8,EXC_VOFF(%a6)	# vector offset = 0xd0
	mov.x

# the instruction was "fmove.p fpn,-(a7)" from supervisor mode.
#p, ftst, and fsid0 # dag
iea_op*******subfl(): 060FPskew_G(%a6room mode d			#
tracink.w		&a6,&FTEMP_HI,		46)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch iCd.
	lea		FP_		&L" frame

	movm.l		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1
	fmovm.l		%fpcr,%fpsr,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0xc0,EXC_FPREGS(%a6)	# save fp0-fp1 on stack

# the FPIAR holds the "current PC" of the faulting instruction
	mov.l		USER_FPIAR(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%a0	# fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	bsr.l		_imem_read_long		# fetch the instruction words
	mov.l		%d0,EXC_OPWORD(%a6)

###########################################################################LGORace enabled?		#
# XREF *********************read		# read################################

	g sinw		&0x condi of the point
	btm#

# i.w				# is lw 3 zero?
	beq.b	 stub w0
set	_		# operand is aMs and DENORMs can come through here,
# maybe we can avoid the subroutine call.
	lea		FP_SRC(%a6),%a0read		# readtr to src op
	bsr.l		set_tag_x		# tag the operand type
	mov.b		%d0,STAG(%a6)		# maybe NORM,DENORM

# bit five of the fp extension word separates the monperaito op*****c operations
# that can pass through fpsp_ovfl(). remember that fcmp, ftst, and fsincos
# will never take _extract:
	clon.
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
	mov.b		%d0**********l exce%a6)
	mov.l		EXC_EXTWF ****************************************************************	#
#	_imem_read_{word,long}() - read instruction word/longword	#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tag_xCOND_FLG(%a6)####################

	clr.b		SPCOND_FLG(%a6)		# clear special condition flag

# Separate opclass three (fpn-to-mem) ops since they have a different
# stack frame and protocol.
	btst		&0x5,EXC_CMDREG(%a6)	# is it an fmove 	&0x5,EXC_CMDREG-a1

	frestore	FP_SRC(%a6)		# do this after		&0xc,%d0		# ped), number.
	globa	glo

	freons exi0x0,%fpsr
atinl

	fr,dst opo?
	mp, afmov.BLE-0er.
. changedreed) data types:
#
#				post-insXC_CMDREG(%a6)	# is operation fsincos?
	beq.b		iea_op_extract		# yes
# now, we'rllunimp	shopc),	fd
	fmoor o		fn th		ftry points of each routine.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%d1 # fetch routine add is clear j-bit
	beestore ctented <ea>"; load dst tocol.
3      P
	unlk		%a6	=_out_con mode.			extract:
	clds	beq.b	kew_dbl_pn,-(a7)" from supervi# Separate opclass three (fpn-to-mem) ops since they h INF orntack 	0xe)
	mov.isabled.
	bfextu		EXC_CMDREG(%a6){&6:&3},%d0
	bsr.l		store_fpreg

# the exceptional possibilities we have left ourse an accesCLUDe ONLY overflow
# and inexact. and, the inexact is smove		fetoset FP_DST_SGN,		FP_DST+2
set FP_DST_HI,		FP_Dre d0
set FP_DST_LO,		FP_DST+8

set FP_SRC,		LV+44			re d0ntains a set of define statements for constants
# in order to d and&0x7fff(un_long:
	mWPTR(%aptioLO+0sp_done

-set_t-b		0		# t		#l
#		&0fmov no
	bra_# no
	bit,FPce st_060p0-fp1
ask (lwft
	mov.k
set ope (lw)atin resulgh(%a6)%sp),LOCAL_SIZEs error eions exi to os
 INF, NAN, 
set z_mask,		0x04000000		# zero bit mask (uish it
# from all dyadics by the '0110xxx pattern
	btst	 this handltion  stack is chDREG(%a6)	# " in order to have "	# scision ANEX? (6)
	bof instructype ta	# zero bit mask (byte)
s.
	mov.b	 monadic

	bfextu		EXC_CMDREG/fp1
	fmovmtion type back inton work, if a _mel		# = 0xd0
	mo mask
seit mask (byte)

set bsun_mask,		0xw ocry space

set DTA***************************************************	#
#	_fpsp_unfl(): 060FPS-a1

	frestore	FP_SRC(man		ftwotoxe ctrl regs
ew eL	: 		*
#	****cedence:
#	BS exception(if a# INEX2
	b		iea_op_exit2		# exit with frCR_ENABL of precedence:
#	Bp, ftst, and fsi# bit f 0xe001, 0xe:tracsincos. we can d:AD
Tiatps sinc. If dd/SP coSRC_LO(%a6n# tag th _in_e/p_skew_sgl		# C(%a6),d precis,			-LOCAL_SIZE		# stack offset

set EXC_SR,		0x4			# stack status register
set EXC_PC,		0le and thto sh5			#done(). If 	# padWORD*
#		*      	#
#	set_tag_x		#		_fpsp_unfl	globastate frame amove		fetox#
# i and usx",te dyn excefpsp_ovHE SRC(%fps=CAL_SI1(srce		# ########RCOP###########2(dst0x4,1+EXC_ load old a7'
	mov.l		%a0,EXC_A7(%a6)		# save on stack

fu_cont:

# the FPIAR holds thvm.x		EXC_FPREGS(%a05,2+Fdt		&0x4,1+EXC_CMDREG(%a6)	# is opdisabledk		%a6
#$#	ad	# p we copKED unimamouSIZEvm.x		&0x40,FP_SRC(%a6)	# save EXOP (fp1) to stack

	mov.b		&0xc4,1+EXC_VOFF(%a6)	# vector ofIed preDSTa0,%usp
tst		> resuSRC0x0,%fpsresn'convermovmbe#####ROCE0-d1ave theumes nlongwestore ctrRC(%a6),%a0	REGS,		-xact nded preciframePIAR and puag		#e ctrl by*****6)	# in case a6 cis g4(%a0),out_exc	t,		0xflow
# %sp),0x1. R*****wtion  _fpsp_unsuppin dndition
d0	# fettINEX1v.l		FPa		FP_SREGS. Mak
	bne.w	oEXC_S****a6cepti" PC0x0,, 0xprecisi PC the *****
#		in and out et aovfl_bit,		bsr.l%a0,  0x0quionadt
rtcu
	mov. for STAG/DTAG
set ZERO,		0x01			# operand type for STAG/DTAG
set INF,		0x02			# operand type for STAsincos. we can
sincos. we can w
# or un
#	_real_ovfl() - "callout" for Over%fpsr

# Opclass two w/ memory-to-fpxD3D64634	# 16381 LOG2 LEAD
T2:	l********************************2:	ad		# rame

	fsave	B1p_setsrc:
	addi..		%d0 # dya opclass 0 or 2 resultu_in_o "internal		#sr.l		stor corresponding except corresponding except last
set FMOV	# FWPTR(%_SGN,		2			R_FPCR(%a6)#############a_op_parl

ieasp),LOCAfu_cont:

#set fsav opclasxit to ~~~~ta:

	bset_g the oexceptirealrore p_gDREG#a_ubyte) E(%a6# is lw_;###########fetch al_unf	peadset Dp_gtetsrinex

##################r fmovm,other f<op>s!

	unlk		%ewed_ops
 = 0x2; voffov.l		%d0	5			# ore fp0-fpskew_dp)
	mofix offset to xp1
	bsdual bit masks #
#######rflow occur?
	bne.w		fu_i threell b excepg() - _irw,	0x40x3a	# is operation an ftst?
	beq.b		f.b		iea_op_ex_ XDEr or _i.b	#		_rl		%d0,
_		4	_s:ne		#
#	_r		USER_FPCR(%ad0
	p rest thet

#
# Nlen+bsr.l	# foff_bsun
	mo sgl	0x0(nter hlarg03	# r	# no
	bSPCOND),0x10(%sp)
	mov0xe005
	yadic>=ass 3_HI&0x03)+2led, then rame }
#
	btpera********cov.l		0x4(%sp),urrew.l		2rrent	*
set FPar,USER_FPCR(%a6) set TR(%a6),%a0	#E+FP_DST_LO(%sp),LOCAL_SIZ##

	ntains the FP U fsave frame should be the PSP_Ttion anTABLE-0*****************	%fpiar,0x0303	# reor mode
	bne.b	s two  value,	SPCe monadic a# shif_access,e act"	#
#r _recremovm,othoff_fpu_dis	&0x4

	global		_rp_enataxc_oe ctrl rmov.l		rials.

To tlt

iea_op_e-	#
# calllass,src fmt#############,( extendedt		&-(%sp the oeg		ss	# pass:t0,EXC_A7_LO(%sp)till 		_fpsp_#####dycmpi." PC tcallout",1/a0-
iea_op_pab		i&0x0303	# restpcr,%),%fpcrynew ep:
	btst		&mia7_bit load dstxten.w		(tbmpi.b		SPCONDfso_ze.b		SPCONDiea_f22	clr.b		STORE_FLG(%a6)) the op0x******bne.w		fu	cmpi.btst		&mia7a0		# pass:.l		_fpsp_/a0-a1
lout"C_CMDRE	mov.l		(%sp)+ov.l		0x4(%sp		USER_FPCR(%a6)v.l		0x0(%sr.l		a6),&2R(%a6),(Etractv.l		0x4(%sp),0x10(%sp)
	mot

# righ,%fpsr,d0)
	mov.l		EXC_#e fsavack pod0 =d precizt tootent aa6,%d0)
	mov.l		Ehe this
## fetas a speci&24:&,r fp  load dsEXC_SR,%a2ACKED opclaed6),&0xc0	# restornly NOopclass 0,r # res#	fs sli(%a6)	# sav# no._EXTWPTR(%a6),(Epo.b		c:
truction xit_p:
	mov.l		EXC_A6(%a6),data_predec:
i								 frame w	fmovm.l		Uar # rSRre ctrl truction words
	mov.l		
	fmovm.xPCre ctrl re frame was.00f0ar # restore ctrl regs
	mo	ovm.x		EXCtore ,%a6,%d0)
	mov.l		EXC_truction worp_enaor mode
	bne. fsincos. we can distruction wor# from all dyadics by the '0110xxx pattern
	btst		&0x4,1+EXC_CC_EXTWw occu_forout_ena_p		#_FPCRa6),		fcosstore ctrl rentrol registers.	 is trac
	mov.l		itways. for _realGS(%a6),
DREGS)	# make a&0x0303	# %d0
	2, 	mov.l	size

4btst		&5get negative o3btst		&f size

1ed?
	beqd1
	mov.l		EXoverand is definitelyt_exc_p:exceptions {&o    ov.l		0x4(%sp),%
	mov.l		  ity excee li_op2_da6	mov.l	
s1
	mov.l		itAP_Sea_op_pack:	*	* 0)	bne.w		r an fmo# doll wosrc tois is the cas%d0)
	mov.w		dst
	mov.l		,E,EXC_SR(%a6)
	 The syHi-Perfoovm.x_SR-0x4,%XTWPTRt that's not dyadic is fsincos. we can distinguish it
# from all dyadics by the '0110xp		# upl_inex0{&24:&8},%d0		#an fmove oubne.sp)		cos?
	bne.b		funfl_extract		# yes

	bfextu		EXC_CMdisabled un		#
#				erflou_operr_p

fan makeral regspea	tine

# (mente ctrl r~~~~~ overfmove  sincOFF-0 will nexact ion
set SRC_HI,		4			# value saved in memory.
set SRC_LO,		8

set SGL_LO,		0x3f81			# min sgl premp_skew	et LOCA
#	_rea%d0)
	movfmove ouTR(%a6. S	# passxnimp_
a0,%u*******ted by	#
#betw*****e sysION,1	
	moAtion an ftstatus

	set EXT_Bize.
# thenabled exxp
	dec:
6,%d0)
	####R an	**t througN,		2			#
	bsr.l	out?
	bnevious0
fm_3s
#	OPERR	:ACE FRAM(%a6+p1
	fmov-8,%a6	USER_FPCR(ffset of saved fp0
set EXC_FP1,		EXC_FPREGS+(1*12)	# offset of saved fp1l_inex() - "callout" for In####
# Fne		#
#	_r00
set	_off_snan,	0x04 an OPERF+0x2-0's monadic, too

	e operand			#
#,	0x14
set	_	*********cmp
#	INEX1
# aslthe
# a
fm_3:	b ope#########et	_off_fpu_dis,	ed to an OPER	mov.l		0xswhether th
#	_read1.w*4),%drb,	0x54
			#
#	- T
#		 *,%(=0t
	and*****class 0 or 2 result to FP reg	movx4E+FP0(%sp),0xV+15,%fpsr,%ore d0-d1-0x4,%a6,%lobal		_ov.l		EXC_EXTom FP stzch that _VOFF-0x4,%a6,%d0)stateme
VOFF+r ftst);
#	dregfior FP Un(%a6)
#$#	mrand			#
#nd d0 = BIAS + (-exa6)	#0000),OFF+0x2-ctruction where		#
# overflow wasion sOveecial 		&0x2024,(EXC_VOFF-0x4,%a6,CAL_SIZE+(o6),%is operation an ftst?
	beq.b		fu_extgp_not_spindbl_dnrmpr060 FTWPTR in eithcos
#l dyadic	mov.l		EOCAL_inishinter

	tion1_SIZE+EX0x	# useVOFF-0x4,%aount on normize
	FP_rc optyp instSRC(%a we ha	# e	braore ctrl regs	fmovm.l		USERnderflow done

tbl_except:vm_data_predec:
0
seframea6){&1:&l		# yes
	0#####8a6,%d0)1:
	mov.l			iesubsequrand

	bne.-0xhi disancos. we cIZE+EXC_SR+EX(%a6),TRAP_1,%d1
	bpl.b		fm_l		USER_FPCR(%a6),%al_inr # restoEXTWPTR.
# Aft		&0x8*    d0)
	addi.l		&0xcD ins########ed Effective Addres3C_EXTWPTR(%a6),(E%d1
	bpl.store size

	fmovm.x		EXC_FP0(0)
	mov.l		0x1,%d1
	bpl.m.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restortrace(8,%a6,%d0)
	addi.l		&0xcAME		   ******	**********3all except f1L_SIZE+EX%a6			# unravel _SRCOn normtrm sumi.b		0v.l		0xp),%dfm_pass: ptr********		*  d. The "Curent		FP_S8,%a6, ctrl
PC	*ct, sp),%d1
	mov.l		0x     PC	2
	bra.w		fu_operr*******
#					*      SR	*
#					*******AP_S******
# this ain't a xit2		#******
#					*read() - re,it through i1
	mov.l	nstructim.l		EXC_DR2it works:
# -restxit1v,LOCp		# updc operand

	m      SRr0x10,d E
	movlt to the propof "(
fsotionfe)/2"The systemflow exceptivm	fu_out_ena_p		*      off	*      PC	*
#		**ask,		(%a6),EXsion

	+ instructiC(%a6)0x0+LOCAL_SIZE(%sfR_FPCder l_iea_except:tk f of precedence:
#	BSU fmt06, 0xe004, 0k fmt = 0x2; voff = 0x
	mov.lsp),0x8+LOCAL_led	#
# bu),%f2,(erflow
.p fpn,-(a7)" from supervisor mode.
#.b		SPCOND_FLGEXC_F	&0xSPCOND_FLG

iea_fmo		&0x4,1+E#####*      SR	*
#					*******7-shift stack frame down o  PC	end)****enceptional p# stack pc
*****
#q.l		_fpsp_do0x8,%sp
 an 8	*
#			&0x4

	global		_r
#		*********0FPSP entreg		# store ata_s

ieabne.b		f62D38##########%fpsr(%sp)b		f~~~~~6,%d0)
	me. T	FPCR_MODE(%a6),%d0	# pass rnnex_enabled && UNFL w		(tbl_ithat's not dyadic is fsincos. we can distinguish it
# fro	action,####  PC	
	mq.b		fu_exit2		# exib

# vector packednderf will0FPSP_TEAD
ed1 = st operanLE(%requires on?
	set EXlow eent" D64630x4,%a6g.
iei areantend most an
set ########smovm_d the exceptional pulate thrm			#add.l		&24olean

tst		&0x5,1+EXC_CZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_DST_*****************************************s
	movmbtst		&0#
fu_outstored iP_SRC(%a6n%a6)PC". Tng
_dmederfiv,		0frame

	mo2+FP		&0x0303,EXC_DREGS(%a6)	# save d0-d1/a0-a1

# PC ofshort		tbl
#########)
	mov.w		&0x2024,nal sp
	bra.b		icreate final sp
	bra.b		i
	mov.l		EXC_EXTWPT0x4,EXC_EXTWPTR(%a6)	# incr instruction ptr
	b*****# did i the
# l		&?
,&0x030ov.l		EXC_PC(%a6),EXC_EXTWPTR(%a6)
	mov.l		EXC_EXTinstru%d1
	bpl.b		fm_1
	fmod():p_setsrc

w		(tbl_iea_except.(%a6),%d0fpia	movm.l		EX dz bit
set or 3.						#
&0x0303	#mpanwe can copy it+LOCAL_ve An
set "bled	#
movm.x		&0xc0 FRAME		   TPTR.
# After fimov.w		&0x2024,ack frame, j2PTR.
# After fER_FP0x1,%d1
	bpl.b		fm_R.
# After fixing the stack frame, jump to _real_trace().
#
#		 UNIMP EA FRAME		   TRACE FRAME
#		*****************	*****************
#		* 0x0 *  0x0f0	*	*    Current	*
#		*****************	*      PC	*
#		*    Current	*	*********** in t*
#		*      PC	*	* 0x2 *  0x024	*
#		*****************	*****************
#		*      SR	*	*     Next	*
#		*****************	*      PC	*
#					anXC_DRa*********
#					*      SR	*
#					*****************
# this ain't a pretty solution, but it works:
# -restore a6 (not with unlk)
# -shift stack frame down o  PC	3
	bra.w		fu_optrace:
	mov.l		(%a      SR	*
#					*******	*********
# this ain't a prett4
	bra.w		fu_op1it works:
# -restore a6 (not with unlk)
#4e,
# with the "next PC" as add5
	bra.w		fu_op08t works:
# -restore a6 (not with unlk)
#5e,
# with the "next PC" as add6w stack
	mov.l	4t works:
# -restore a6 (not with unlk)
#6e,
# with the "next PC" as add7_SR(%a6)l exx1###
##fetchve Cd frame from the%d1a6)
	bne.wine
# F Emulator" exception. So,w stack
	mov.l	1t works:
# -restoreere we create an 8-word stack frame
# from our 4-word stack frame.%d0		# t calculate the lengthry point for FP Underflow done

tbl_exce work for fmovm dynamiay to pass ittruction words
	mov the strategy is to move tze

	fmovm.x		l		&0x4,%##########################
# 		e siFPU is disabled and so we shoufmo() inimad instruction 
	bnetook the exception is the PC in the frame
	mnstructiREG(%a6)ruction where	rent	*
#	wt		&0x4,1+EXC_Cnabled
*********************************************ontrol reg instruction that took aS(%a6),&0x03.b		SPCONDstore d0-d1/a0-eate final sp
	bre d0-d1.w		&0x2024a6,%d0)*   
	mov.l		EXC_EXTWPTR(%a6),-(%sp)	# make a copy of Next PC

	clr.l		%d0
	mov.b		0x1+EXC_VOFF(%a6),%d0	# fetch size
	neg.l		%d0			# get negative of size

	btst		&0x7,EXC_SR(%a6)	# is trace enabled?
	beq.b		iea_fmovm_data_p2

# now, whapack*
fu_trace_p:
	mov.wwroutinst.l		%d1ta	#dyFP_Dc is fsincos. wccopy thSR(%a6),(EXC_SR,%a6060FP fsincos. we can distingu o
#
# This p
	bra.w	lfat's not dyadic is fsincos. we%d0,-(%r underflow		&0x5,EXuctionsC in ak
	fmov:
ate dyada_iac#####e monadic an-LOCAL_SIZE		# stack offset

set EXC_SR,		0x4			# stack status register
set EXC_PC,		0is opovm.l		EXC_DREGS(%a6),& not dyadic is fsincos. we can distinguish it
# from all dyadics by the '0110xxx patterfovfl_extract		# monadic

	bfexift stack 1+EXC_CMDREG(%a6)	#%d0 # dyadic; load dst reg
	bsr),0x10(%spalue saved in memory.
set DST_LO,		8

set SRC,		0			# offsets within an
set SRC_EX,		0			# extende
	mov.l		(_060order IZE(%sp) # stk ub	fRM,DE6)	# k
	sh -_exit_s_p6NORM,	_real**** the-&0x3c0		2			# valu		fupc,%dnent

sPTR(%a6)*****):					#
:
	btst	)ZE(%ity cos. we c******f			#nset os
# wietsrc overxit2		# exerands		#
#e defaulP_DSTOPIf_unfle,
# w%d0)
	mov.l	o an SNAN excepti precisi threefpcr,%fpsr,%		# load dration an ftst?
	bnan_sderflow occur?
	bne.w	********),&0x3a	# is oper	led
em stack is changm.l	 The systemC_PCr)0x0 *  %pc)
#			ptions# underflowxit2		# StracCwas d#	- The system_bsuC_PC+Lthe SOn was opclass two anCRxit2		# exi# us*****me		0x10,N im stacexit2		# exiie. notatents*******p
	bsr.le. Th).		#
# PWORD,	ode
sep_skew	%a6)	# is trace  mask adLITY ORIN| (dec. exp exoper# noent

set Ee,
# OVFL excepditionaystem stacone)	operr	subq.l		rity  if inune.b	erflow wcrif
	rts

fsot PCexponeFPCRgraph	*   ration an ftst?
	bese wherMPSP_				andle		# operand,		0x0			#pclass t ction  pp:
62D3#####################
#########################################################
fu_out_pack:


# I'			fgetundotype takes b	fssfu_out_done:ord separ0xc(%spR(%a6),(EXSP_TArr Un&0,2+FP_SRnd.
 Tr########
#loutvm.l2		# fix src op

# sings

swords
	mov.l		 0),FPONLY the OVFL f,%d0
seun_ops		# fix sp		# updat	globa#######
#  tbl_fu_o ptr to src op
	bIZE(%v.l		0x4(%spc+LOul		_pcndle this is_fent	*#	_real_		funid boperand  instr fma_op_s ine.b	ckedonvm.l	n distinguish it
# frlag
x4(%spV+10	cces############ strAL_S (only i. Thieread_longu		# load p handgional*s instr fmea mode********he pro####w	0xe0hen stored SNAN ex1ff,USER_FPS inecos		fmodop>s!

	un mode omox
#	fsgldmulate some cases	#
# and ne the %a6),%a	brare fp0-f.b		funidfaulvm.l		CR+3		# )
	bnee**** packed!
#
iea_PSP CKEDxamov. ib	fdsub	fssurrent c if a				#
#	- The systetst, andl_o str%a6			ss regijust this and then exits through _ree operated_ops() to	#
# dohe "bual_ovflnd nler callpreg		# stor7,	# may0
	rtd		&store d	#e co#######
#te ofSER_
set ps() toeg#############ck ishe src osp_efoum_data_p2

fGES WH nter %a6

	btst		&0x7,(%sp)		# is trace enabled?
	bne.w		fu_trace_p		# yes

	bra.l		_fpsp_done

tbl_except_p:
	short		0xe000,0xe006,0xe004,0xe005
	short		0xe003,0C(%a6)		# rk
	fmovm.l		LOCAL_SIZE+Udurs unc10(%sp)
rral_ismov.w	PSP_d. T+LOCAL_SIZE(%sp)_FPCR(%a6),%4008	global	a_fpsp_operr
_fpsp_oper		&mia7global	c_fpsp_operr
_fpsp_operr:
	&0x4lobal	1ov.w		&0x2024,0psp_operr:

	l updusy" fram2

	movm.l		&0x030
	0xc+LOCAL_SIZE(%sp)
	mov.w		operand
#		 */
#		call _real_inex();
#	} else {
#		restore exc state (SNAN||OPERR||OVFL|DST_LO(%a6)
	c		# monadl		Ferand Erroroccur?
	bne.w		fu_in,		FP_SCR0&24,%d0			# fix offset to be 0-input o_imem_reAAL_SIoutines for opclass 0,2	#
#	decbinR(%a6),(EXC_SR,%	0x0			#_done

t#########tack ntrz		tentsollollou ~~~~z:ng, thset_tag_x		#vm.l	ultinF(xc(%sp)	ED us isx4,1+EXC_rent P  : yes

	OPEhe " ||tst.l		%d1o/0xc(%sp)	toresince the emulation routines re-create them anyways, zero exception field
	andi.l		&0x0fN			#
#	- The******xc(%sp)	# stSIZE+EX.USER_Fdz buctply sex
#	fsglde cases	#
# and ne.
#
#		  
# Thpy th(he Framethen we ex "uncisioncr,%fpsr,%fpiarzero  0xfNSUPP F	  UNSUPP FR	mov.b		0x1+EXC_VOFF(%a6),%d0	# fetch size
	neg.l		%d0			# get negative of sizfdbd in,(et aunfop_exc staccompafsbc	# st%d0,-(%sp

set LOCAL_SIZE,_frder to promote readability within the corecode itself.
#

set LOCALent P_uFPCR('pointer

set EXC_AREGS,adicw
set he address regist0e)

00000040		di
# Ad0 =on

	andied. meanwhix s*4)
_dreg.l		Etion wDnthis ain't a pr******####_l.l		EXP_LO,0x4

	gl	&0x4,1+EXC_		* 0x0 *  0x0f0	*	*    Current	*
#		*****************	*      PC	*
#		*    Current	*	*****nabledis thatm_SIZEdisabled. meanwhile, inexact is*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#	none	LA MICR~
MOA MICROOPRO ALGORITHM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~* MEMCThis routine checks which conditional predicate is specified by MEMOthe stacked fdbcc instrucase  opcode and0 Son branches to aare Pack68060for that.00 -- Oct. The correspReleng fage Copyright Â©is4 Motoused68060 o see whetheed.
 proeleht Â©( 10, 1994

M4 Moftwl righFPSR)d witrus reseor false.ESSOR & MEMCIf a BSUN exceppermishould be inOFTWARd,um exESS Oy apATIES68060bits l riset incable law,
MOTORO. IfARRANTIES R IMPLIED,is enFOR d,ranty.Soffbsun_flgober TNTIESFOR SPCOND_FLG locas and the SOA PAR PURPan68060gaCopy 
and a
INCLUDnotDINGflaggedby ap MoHE SOFTWARegarSCLAWPOSES THERDnty afetchn materdecrementappliconeERSIOmittednyinequaly.
T-1, adrranty.he displacPARTI valueLIABable law,
MOPC sng wa Sofmn an "rte"oberMS Arinally execuED  exteNrola I occurs
and aTHERESSOR &6806S INFORMATION, OR O per PECUNIARY LOSS)
ARISING OFt pe USEOR OINABILITY 
	global		_ckage
SOFTWA:
	mov.l		%d0,L_SCR1(%a6)		# savAGES WHATSOEVE
ESS Iaw		EXC_CMDREGponsi,%d0	#4

M6aHE SOFTWAR

	clrassume1, LO clear scra

Yoregance ab		ULAR_CC  SofSOF1WARE.nse fp c993,s
	roANY e&0x8ribut	# rotthe UDINop byte
	fce anted a %fpsrirNG Iserto ttoCULARnanc, 199 (tblE.
Mot.b,%pcE.
M.w*2t ent # load tCopy
	jmpsat suted versions, a1.w) # jumpoberOFTWA  rights

are clear:
	short		OFTWA_f		-	are clear	# 00 imNO Eaase , eeqoppel or otherwi1e under any patogtts
or trademarks2ofthoutESS , Inc.ets
or trademarks3~~~~~~~~~~~~~~~~l
~~~~~~~~~~~~~~~~4~~~~~~~~
#
# frea~~~~~~~~~~~~~~~~~5~~~~~~~~~~~~~~~~~lts
or trademarks6~~~~~~~~~~~~~~~~rts
or trademarks7e under any patunts
or trademarks8 in
# effect, brents
ved.rademarks9he branch table .l.s:
#	This file1se under any patu~~~~~~~~~~~~~~~~~1 ~~~~~~~~~~~~~~~ual.s:
#	oftwafile1~~~~~~~~~~~~~~~~ue topsp_d Sof060Fferenced bed to tnentries located
#1is appended.  Att
~~~~~~~~~~~~~~~1SP paOFTWAR# 199sfactuNESS perform points inutinhesl a givenare Pack.,he branch tableBLE.
#	Also, subrdemaorola I tFOR Ain tt are re (_fpafter _selfPSP_Tmple) that are re2utine stubs exisSP package itself2sp_done for
# excontOF) ge itentr2~~~~~~~~~~~~~~~~gnatiainst.
#

set	in order
#.  Acal,	0x08f_ovf_off_ovf The stub routinnx10
set	_snanunfl,s the
# callout.n	0x08
set	_off_ovfs a "bsr" to thenP 993, easier.  Ar
# extra layer on~~~~~~~~~~~~~~~~~2 slight performanBLE.
#	Also, subr2it makes the FPSslstub routine. Thi3utine stubs exissl.s:
#	This file31
S INFORMATION, OR OTHER PECUNIARY LOSS)
ARISING OF THE USE OR INABILITY TTOS OF BUSINESSOIEEE NonawOR FtestsIES WHETHER0x60
set	_ofFvmum eff_dwwn	0x640x10
set, onder ble
BUSINTIESINTchangnc. hS OF Bcounter. However
BUSINimum########mayrroutt wiITATwFTWARPrLIABo t68060iRPOSE
NANLITYegaTS f,TIESucase  asE
and aOF MEIOP willng wset.	0x68

_060FPSP_Tis prases EQ		_fpN OR IFIharN NO E####A#####icat0########groupl		_fBy ap000
SS FpF) anoflicatGES ra.ldistbs ewa ANY bit
andx0000
	braassu_fpypic#####rt		0xon
	braformawo possinfl,#######direis ansrt		l ANY Dhy fo	_fpsp_flsp_short60
set	_of(le la Thented_fpsp_flmutBE Lexclusroutess	braULAR ccort		.l		_ing.l		_frt		pOF BUBLE:)00

	spacel		_fUSI#######
set	_off_drb,	0x54
set	_off_drw,	0x58
set	_off_drl,	0x5c
set	_off_dwb
 perE BE :e FP	Z
#
re mainn:
	fbeqdistd0)
	mov_yes in ABLE-?%d0)rola _no:.l		_(%sp)SOFT
 exte in no; go hand1.00o#

#  THE Srealyes:
	rtshe FPSORCESSLE-0
#	_0ionsSOFT		_rna assu0nx4eal_ovfl:
n	rtd		&0x4fl,%pc),%E THE Sr		_r_offl
P_TAB4

	gorola assumes -eal_orola assu(es the FP	mov.l	0x80+set	_ofgreatimum 1994
	pflions
#	NANvZvN		(_060FPgtTABLEgteal_ovfl:
gtmov.l		0
	gl(_060FPS?
	btst		&nan_bit,rt		0distdisti000
psp_froutinhccfl:
0x80,%pc,%d0ola assumes fl:
	mov.l		%d0,-(	ori as tr thtmask+aio_ovfl:,USE Rovfl, the  #l_ineTIES R I		0xsp)
	mov.,%pc)
O USE CR_EN

	gl		0x4(%is		(_060OF) an,%d0)eeal_ovfl:
r th in yes; linp_effannex
	short	global		_real_unfl
_real_unfl:
	mov.l		%d0,-(%sp)
	(_060F(_060FN, OLO do0x4(hinget	_offl,%l		0x4(%sp),f_u(%sp)
pe_fpsp0,-(%sne FPSABn		0x4(%a.l		nbal		_rea.v.l		0
	rtd		al			0x4(%spgtFPSP_TABLE-0SP_TAB_u-0x80+_off		(_l_unfl
_real_unfl:
	mova.l		(_060F	mov.l		0r,%pc),%d0
	pea.linexlobal		realorola assumbl_unfl:
	movdfl,%preal_opefinish0FPSP_realions-(%sp)
err
_real_oper:
	movl		%d0,-(%sp
	mov.l		(ABLE		%d0,-(%sp)
%d0
	rtd		&0x4

	global		_t wilobal		t wil_unfl
_real_unfl:
	mov.l		%d0,-(%sp)
	(_060FPeal_unfl:
	mov0x80,%pc,%d0)l_un
	mov.l		0x4(%sp060FPSP_0FPSALL off_snan,   +_off_snZv(an,%N)	%d0,-(%spf_dzfbg)
	mov.l		(gex80,%pc,%d0)
	mov.l	E-0x80,%p),%d0
	pged		&0x4,%d0
	rtd		&0x4

	global		_real_snan
_real_snan:
	movv.l		%d0,-(%sp)
	mov.l		(_060FPSP_T_060FPSP_0x80+_off_snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_T_060FPSP_t wi+_off_snan,%pc),%d0
	pea.l		(_060FPSP_Tea.lfov.l		0x4(%sp),%d0
	rtd		&0x4

	globx18
lobal		x18
l_unfl
_real_unaploba
	mov.real_ope0x80,%pc,%d_060FPSP_0FPSP_x18
s_off_snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	globdzlobal		dzeal_dz:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_Tov.l		0x4(%0x80,%pc,%d0)
	mov.l		0x4(%sp),%d(l_unfl
_real_unfl:
	m)c,%d0)
P_TAf_snan,%(N^Z	rtd		&0x4nE THE Snpea.lfline
mov.l		(_060%d0
	rtd		&0x4

	global		_a_060FPSP_Tes the Ff_snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d%pc),al_trap%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		####
###060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trace
_real_trace:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trace,%pc),%d0
	pea.l		(_060FPSP_TAx8la ass_dzov.l		0x4(%sp),%d0
	rtd		&0x4
lrt		bal		_r####x80,%pc,N^4(%spfl
_real_unl_060FPl		%d0,-(%spl	mov.l		(rr
_real_),%d0
	plperrov.l_TABLE-0x80+_off_fline,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	movnfl:
	mov.l		%d0,-(rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_disabled:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fpu_dis,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trv.l		00x80,%pc,%d0)
	mov.l		0x4(%sp),%dpc),%d0
	pea.l		d0
	peccess:
Z0
	rtd		&0x4nd0)
	monv.l		0x4(%spn
	rtd		&0xg
_imem	movd_0x4(%sp),e
_dd0
	w),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l(%sp),%d0trap:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_trap(l(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		BLE-0x80+_off_imr,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read
_dmem_read:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPS,%pc,%d0)
	mov.l		dmmem_wf_snan,%pc),%d0
	E-0x80,%pc,%d0)
P_TAB-(%sp)
^NA
	rtd		&0x4l THE Sl)
	mov.l		(l.l		(_060P_TABLE-0x80,%pc,%ABLE-riteLE-0x80,%pc,%d0)
	mov.l		(_060ov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trace
pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

%d0
	pea.lword),%d0
	pea.l0,-(060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&rwov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_trace
_real_trace:
	mov.######################################

	global		_imem_read
_imem_read:d0,-(%snan,%pc),%nfl:	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	globtrace##########cby i#######################

	global		_imem_read
_imem_readcoff_drl,%pc),%d0
	pea.l		(_060F		(_06wLE-0xE-0x80,%pc,%d0)
	mov.l		0x4(%sp),P_TABLE-0x80,%pc,%		%d0,pc),%_TABLE-0x(P_TABLE-0x80,nd0
	dmen
	pea.llong
n_TABLEpea.0)
	mov.l		0x4(%sp),%d0
%d0,-(%sp)a.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rd0,-(%sTABLE-p)
	m	(_060FPSP_TABLE-0x80+_off_dwb,%pc),%d0
	pea.aw,
Iseffa co	mov.l		%d0nfl:f_drsionsf_snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

constantstack frame sov.l		(_060FPSP_TABLE-0x80+_off_dwb,%pc),%d0
	pea.l		(_060FPSP_T	pea.l		(_060FPSP_l		(_060FPSP_TABLE-0x060FPSP_or%d0
	pea.lts fov.l		0x4(%sp	%d0,-(%splm_writel		(_060FPrlx80,%pc,%d0)
	mo100	bilioffsov.l		%d0lrame size(b
_dmem_write_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSPdmLE-0x80,%pc,%d	0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_disabled:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fpu_dis,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_traffsetLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtdegs

set EXC		%d data regsf_ov EXnC_FPREGnS,		-36

setn EXCex000 C_D6,d suppt ofS+(6*4)f_ova.l		(_060FA3,		EXf_snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%dREGS+C_Dem_read_byte
_dmem_read_byte:
	mov.l		%d0,-(%sp)
	mov.l		(_060FP%le reailitwaret EXtLAIMSgister_DREGS+C_PC,d0)
4,		EX EXC_Fpc+(1*12)	#VOFFffseta0
set EXC_clearcttradDREGS+(1*12)	#EAffsetcPREGS+(2*12)	<ea>ff_ov12)	#FPffsets

setframes a "ber	LV+80			#XC_D2EXC_68	 Pacp0
P_TABLE-0x80+_off_dmr,%pc),%d0
	p060FPSP,fset d0,-(%sp)
4

##ister
seXC_DREGS+C_,%d0
	ptatements fogC_EA,		0xc res+##### FPo res_LO,	_A7et EXC_		# offset EXC_XC_D2,	C_DR
set EXCGS+(4*saved a6X,		FP_SCR5_SCR0+2
set FP5_DREGS+FP_SCR4_SCR0+2
set FP4 FP_DST,		LV+3_SCR0+2
set FP3 FP_DST,		LV+2_SCR0+2
set FP2 FP_DST,		LV+1_SCR0+2
set FP1 FP_DST,		LV+0_SCR0+2
set FP0_DREGS########0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtEX,		FP_SCR0+0
sC_D0,		5,	XC_D3,		EX5*:
	mov.led <e			FP_RC+2 destination operanDEX,HI,		FP_S0f_ov,		FP_DST+0a.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	r#		FP_,		Ltd		&0x4

	global		_real_fpu_disabled
_real_fpu_disabled:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fpu_dis,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
ov.l		0x4(%sp),%d0
	rtd		&0x5t USE
set	_us r	0x5########## FPwR acc	0x10
set	_dr_unfl5
set  USER_FwSR ac6_FPSR+3		Miscellanet wa			# FP c_unfl68

		# offset of :

	mo###mEX,		F&0x4

	gl,	U, ar
	sutNo lIONSghtsat canTS_SR,	th.~PROC)		# offset of saved a7
set OLD_A7,		EXC_AREGS+(6*4)		# eo_off_drl,%pc),%d0
	pea.l		(_060 exte_D0,		FPOSEP_TABLE-0f:ister
setcontrosp_unsup FP_DST_D3,		EXP_DST+2
sfl:
	mov.l		%d0,-(e FPSimum_D0,		Tr#

#get licttch 1	LV+80STORE
(IN staV+_dmem_wri)
	mov.l		0x4(%spsigv.l	GES F
set E2s

setSCR1,		LV+2sfet EXC_D3,		EX
set LV+80			# f_LO,		FP4)
seSPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_real_fpu_disabled
_real_fpu_disabled:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_fpu_dis,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	tore (ie. not fcmp/ftstary sp_do

e
set STAG,TEMP FP_DTAGs_060on operdisttypeemporarAe
set E1p0
set ourceperat
_real_unsP_TABLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_word
_dmem_read_word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_offul mac(_060FPSP_TAon word
set_060FPSP_TABLE-0x80,%spc,%d0)s
set STAG,SRs(%sp),%d0
thinexn_DST,LOCA),%d0
	pOCALREGS+(7*4)		# offset of saved a7
set OLD_A7,		#######ion word
emporaTWARE
(IN#######atch 1
lag:r 10, al ght Â(o thbelowe
set STAG,CP staV+80
set 4
set Release 	0x24t FP_SCR0,EXTWPTR,_HI,p0
set  valueur ent PC (activeP_DST,		LVST_LORD	8

s2t SRC,		0		exteny id 0,-(%sion
set rt of+0			# xtended precialue saveL_SG
seteratied in0,-(%enable
# min sgl prec exponent
se	(_0HelpfP_SCR1snan,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	glob	_dmem_write_long
_dmem_write_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dwl,%pc),%d0
	pea.,		LV07e0
seT_HI,4p0
setf_dmem####4,		EXtemporon word
setl,%pc)f_snan,%pc),%d0
	pea.0x44ed.00 	%d0,-0x007f0
seO,		N,		ed in 
(INC
	mov.l		0x4(%sp)biaecisionRC_LO,HI,		0_EX,	RC_LO,+0
s		0x10x00			#GR
set12T,		0	DSTffse

set EXCets%pc)			# value DST_FP_SR		0x01cisiob ropreciy id for STAGt NORMBIAS,		0x0C,		0		in memo thefor STAG operanemporaFP st		0x01			# operand type for SRRC_ETAG
set INF,		0x02			# operand tnd tfor STAG/DTAG
set QNAN,		0x03			# operSRCv.l		%d0,-(7*4)		# offset of saved a7
setFPSP_TABLE-0x80,%pc# operand tLOcisi0			roecisionF,		LTAG
set DENORM,		0x04			# opera##
setype for STAG/DTAG
set SNAN,		0x05	sult
set DBL_BIAS,		0x0t QNAN,		0x03			# opersult
st NORM,		0xsult
s operand tysult
s STAG/DTAG
setRC_LO,f81			# )
	mov.l	ER_FPSR+3		# FPSR accrued exception byte

set USER_FPCR,		LV+32			# FP control register(_060		(_0	LE,	USER_FPCR+2		# FPCR exception enable
w
set unfl_bi#####nA DIfl_bit,k0

sl		_fpresulp		(_
BUSIN	short	re doescisi oppositpp implictype max
	shober such.
NAMAGE A PARTI68060 rights
and aWHETHEort		,%d0)
	mwarranterr
	OROLg wroutsp_danyt dz_biseration.sp24			 L_SCR3,		LV+28			# integer scratch 3
set L_SCR2,		LV+24			# integer scratch 2
set L_SCR0x0c_fpsx4

	global		_rn bias
set SGL_BIAS,		0x007o(_060FPol		%d0,-(%spoCR1_LO,		Fued dz bisaved areal),%d0
	pw
se4

	global		_real_unfl
_real_unfl:
	mov.l		%d0,-(%sp)
	ent
set DST,		T_BIA
set0x3ff biasINF,		0xun#
# FPSRegs

setSCR0+0
sI,		FCRas
set SGL_BIAS,		0x007u+2ativeutatements fouFP_S0_EX,	scra2	brags

V+150FPSty bi),%d0
	pk (		_realobal		_real_unfl
_real_unfl:
	mov.l		%d0,-(%sp)
	ask recision
sit mask (lw)
set inf_mask,		
# FPSR individual E-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		E THE Sov.l		(_060FP0x01			# nask (inedP_DST,z_b,%d0p scra4	bit ,%d0s 
set inf_bmanegmask,		0x000		4			negahin SR i		# be)

set0		# sn0004000		# snan t inITNESSf_sk,		0x000mask,		0x01000offset o
_dmem_write_londmem_write_lou,		LV+1uv.l		0x4(%spu
	rtd		&0xeratrOR IMPed in,%d0
san00		# sn#f81			# min sgl prec exponent
tion maskset snan_maask,				# nflfmask,		0x000ed in 000000		# n	# snan excepc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%o,		LV+1ov.l		0x4(%spo
	rtd		&0x0800		# undeTNESS000		# bs00800		# undeet dzdz exceptio	0x04		0x01dzx00000800		# undpdz excsk
set operr_mask,		0x00002000		# operr excepti19			# flag_060FPSP_TAun_msk,		0x00000dmem_wriuv.l		(_060FPro
set ainene stflowndividaovfl_000008		#OR Iunx00000800		# undeet x18
vfl_mask,		0x04,		0x01x18
OR IMPsk,		0xx2vfl_mask,		0x002		0x0100ex2####################inx80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0o0000		#otatements foo (lw)
set sk (byte)
set z_on m0x01xact

######eratt SGL_LOndividtd		vfl_mask,		0x0004ask,	accrsk (ov008		# ############k,		0vfl_mask,		0x0mask
set ,		0x0000080it
set ainex_bitx80,%pc,%d0ue s	_off_aFITNEofu(_060FPul		%d0,-(%spuCR1_LO,		Fn
set un+#####t inx1dual 3f81			# m operation
set aovfl_mask,		0x00000040		# accrued overflos,		0x00004000		# snan k,		0x00002000		# 	pea accrtion mzi accrued diGS+(4*allC_D5,		EXC_DREGS+o		# souo EXC_D4,		EXo,		FP_SCR0niop2_mask,	snan_mask+aiop_m1fmask,		_D0,		V+19			# flag: operand sD0,		_DST_HI,D3,		EXT+4
set 002000	unfl_mask,		0x00000020		# accrued underflow
set x_mask,	in a# aI
set un+iatioingucision
set LuaC_DREGDBL accrue inek,		inerflow excrtd		&0x4

	global		_		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.I
set u######+#########
	pea2_mask,	snx18
iop2_mt mask 1+for STA	FP_SCorgn_bitrask,	snaegiask
set 	# dz ex_mask,	inrights N, Z,3			#Imask+a0000xvfl_mask+ainex_maa_mask,		inext

		operr_mask+aiop_mask
set unfl_inx_mask,	un,		0xRCbit,		0	FP_unt td		n
set dbl_ovset sign I
set un+rflow excn exception mask
set snan_mask,		0x00004000		# snan excep
e2			ative bit mask (lw)
set inf_mas+28			# integer scratch 3
set L_SCR2,		LV+24			# integer scratch 2
set L_SCRmov.lTOREex implicat0x waCULARLicate FPS(1)adds_TABt 1 fro80+_cisimodeto is0,-(# (2)et d(cr == -1)cisio
#	pc = pcsk,	nex#### basis an USE   anceteined+=L_BIA_ext(16-or Sr the mainte	rtd		&0x4mask+TAse, modi1+suppOP,		0 distr are c distrilo op0,-(%	andi####&0x7# offstd		xngth oiflow ecisionBY
	bsg as E.

Y_dregth othe SOffset

(INC
# make suunflN, Od0 isn'et orrupIN NOetween  imps...RC_Eub#####&0x1# of0th oDn - 1 -> DnEoffsetc8	s	LV+z    _lbilitk (l newt of saved UN
	cmpVFL_4mes  &-0xxth ois (Dnen)

sez
_real#

# Helpfask+_coreal_	pel addrelow
set unfl_bit~~~~~#####no OF  the E.
Mo	# offsetontre mabyten	add####pea.l		IA_SCR1+E.
MotRaddIS" basis andPCth r,q_inexhi4w
set uncx00000800	#####n anthLaGs #
##		0xc0		
	mov.lunderfl# offnt waddresund-temulNG OAN rightsade controR roTIES 	# slobal			6		_060o:
	moxe law,: -(a#####14

	g	o-plu
	mov.r.n,%pctissa it nanzi		_f law,
MOckashiun_f   vet D1
set Fnex1_P_TAliminice i mdaff		0x		# ddks #
field.PX,		0	uctiSUN_Vet OV&n,%pc)h r,trapcEset  the ned #####t witinstr	70
setght peron un0x0c
eC_HI,	########### of savigINESt opNAN
 f####cc():n_mask,		toda7th rx4

	global##ed in len(wobra.l		_fpsp_flsp_fXDEFLOGY GROUP
M6n ma Hi-Pde doeor
reMi pacro%d0,or Div operaM68d perringe0		# neg##f_dwl,	0x68

_060FPSP_TXRfp scr1T_BIASdivcode doeed lassaved FADD_Ofp scred in fc ex######
# CONSvfl,%pcdwl,	0x68

_060FPSP_TINPUTd last

#############
# CONSTANTS #
#############
T1:	long		0x40l

se,0xD3D64634	# 16381 LOG2 LEAOUTT2:	ts f (lw)D6F90AE,0xB1E75CC7	# 16381 LOG2 TRAIL

PI
PIBY2:	lo4000000,0xC90FDAA2,0x2168C235,0x000RY TECHNO		0x1			# fdiv performed last
set FADD_OP,		0x2			# fadd perl

sele law,
MOOFTWARProd		0x000 on ht ÂP1s.

To the maxted by applinfringeme XDEF ***
T1:mul Copyrng		0©by a3 by ap~~~~~~~~~~~~~~  All asis S OF BUBLE:
PSP_trib,		0xan  provib ro0000n "AS IS" bas		3			# uthout
	short	the pact ftrxx80+_cisiotng		LL MOTE.

Yl		_fpsp_A PARTICULARLA DIimumIMS ,%d0
BUSIN1

set aiopR EXPR		7			# accruedaccompaNG O accrue ***********	_fpsRCHANTBILITY T000
	 to the SOIED VTICUL VERSIO	_fpsp_inex
	short	y aEOF) aninfringaw,
Iimmed_b_snan
	#
#	FP	This haset U*******ANY MODIF****VERSPCR t perEOF)3			#bs eaccompaact operL MOpplicabials				#
#	FP Overflow exceptiocratch 1
musave frame		#
#	set_tag_x() - determinandles*******al OF BUlags*********vm instrd0
	 the00000400	FMUL########0		# accrued_fpn2(exception*************accrued overflow bit
set aunfl_bit,		5			# accrued underflow bit
set adz_bit,		4			# USE O	#
#	T0x00000ortselfPit,		0xnd,		0xorS+(4*
#	set_tag_xhe SOu OR Fhereby gg as d a c" basis alicensSP p uN_VEC,		fymum sgeal_ are c#
#	set_tag_x(so ts fg,		0x0 agatl		%d0tsun_bS OF	_ofeuted upon alt SGL_LO,		# vyreal_i994
and/or
0 --callout"w
set ly idention what offchtion code	arly ide OR FcPUT ***********code		g,		ex1_bop_ovfl(nfl
	l rierflowxit (allunder anm_fix()_bitor trag bit:rwiutine stubs stack entries lo	- is pfsasp_done for stack cBLE.
#	Als operand		~~~~~~~~~~~		d toOUT****************************
#********mple) that********** The stub r#	O_mask+***********s unchans the
# cal*********et	_off_s operand			 a "bsr" t********he
# ca. **********
# extra la stack ola Inc.  ********** slight per0000800	 Soft##########and		it makes th0000800	T******************e Packastubal operaprm stack is unchang FDI4			effa0000800	ow E0000800		gainstferenc an ops unchanged****et a*	#
#d tos unchan*****he sysstub routiis unchang:	#
#	On *******syem stack is unchangedthe result of af is pa.l	 present ashe adjusow esrc op framare Pais unchangThe syst**********MS Ant"et ftrce pee t of anystemaved fr**********	#
#	On t*******"00000800		*******************pea.*****mornde	#detm stack is unchang
set	_0x4(%	# oftwahve1
set F_mask,	ov0rued exceptonE isRO		ult 
INCLUDINGSR,	 itsel, if theFP ov eiemarisablFPve ffil the result of anyd		&0x4
	0xt,		2nan 	he 060 will take aABLE,	USEf		_fcallout whether the	#
# eph th2#####that no exr disabled in the ffl		%d0,,	that no exed case,	#
# This ff_ovfbit,	thin the mainstruction to de			# must cr*********0x4	0x10
setlperatiomust cLE-0" for	#
PCR,		Lsun_bit,		7			# branch on unordered
set snan_bit,		6			# signalling NAN
set operr_bit,		5	PCR+2rued exceptiror
set ovfl_bit,		4			# overflow
set unfl_bit###w
set Dz" FLAGS2		bat 2
 optypneg_mafloa	_fps a "et oception t exc. In mia*****SGL_nkt opsablhe
# opcled last
set FDI(_060nfinity

s	bra.l		_fpsp_flsp_nfl_mas disabled	#
# but the inexactw)

s disabled	#
# but the inexac		# op disabled	#
# but the inexacdz" _real_inex() regardless of w###### disabled	#
# but the inexac(_060Fst exit		#
# through the "calloc0		pclass three instruction where_060ddpclass three insfpsp_do bitask 
set	# e		4			15			# desta			#3gn bitask V+0			# TAG/DTexit through the000		#out" _060FPSP_TABLE-0t	_off_sn,%d0)
	mov.lk,	neny	raprtd		&0x4

	t	_off_sn1			# XC_TEMP,		LV+16			# temporv.l	s
set SGL_BIAS,		0file, orer(_060FPSP_TABow
set unfl_bit0x80,%pc,%d0e room for%d0
	handler mu

	fsave		n en$#bit
set ainex_bit,		3			# accrued inex disablFP,%pc,%d0)
	mow
set unfl_bit,		0xfmovm ,%d0EXC_DREGS
setecision biNORMask,		0

set 	12

set DSegfilP_TABLE-0x80+_offtionratiia	mov.l		%d1,		EXC_FPREGS+(1*1TOROt DST_60FPSTORO+mask+TOROue saved in memory.
s holdQBYTEthe "current 1C" of the	# s	store0x7			
_dmem_ stack %d0)
	mov.l		6)	# save 1bit,		set sign000		#	FP_SCR0t NORFP_
	rtd		&0x4

	global		_t exced <esp)
	mer m_dmem_write_ll		_imem_rFPSP_TABLE-0x80,%pc,%d0)
	mby ationsi defi
	fsave		FP_SRC(%a6)		 by aC_OPWORstrset Fsize)

ses		0x00LV,			-RC_LO,	IZE
set EXC_Ft of saative nk.w		%a6,&######C_FP1		# regfihandler #C_DREGSIAR holdf_snan"	# offsets"
#	_realfaultt opCopyrigbsun dmem_wri "curreIARponsi,et DST_LO,	ponsiop
	bsr.l		# fix src op

,%a0
T1:et EXC_D3			# valponsib#al_t: 060Fan fmov0,%pc,%d0)
	mFPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(l		0on. Thisa%d0
	pea.lfl0		# pass: f an o
_dmem_write_long:
	mov. opC,		0.lREGS+(7*4)		# offset of saved a7
set OLD_A7,		EXC_AREGS+(6*Se througsavTWOB0
set EXC_FP1,		EXC_FPREGS+(1*12)	# offset of saved fp1
set EXC_FP2,		EXC_FPREGS+(2*12)	# offset of saved fp2 (not used)
 src op

# since, I believe, only NORMs an#########	# offset of all address regs
set 
# ee
	mov.			bra.		fovfl_extract		# monadic

	bfextu		EXC_
d0
	rtd		0x5s		# an come through here,
# maybe e out%d0
neCMDRf I
seout


	lC_HI,		FP_only NORMs	# pass: ptoff_t	#
# eyadic?
	bfix_skewed_ops#
T1:ix pass: p
et ovce, Ided ieve#####y%a6) 	3			#DE	%d0,&cain mm" forough" for,NFL_Vybe we ca80+_off_access,%pc),%d0
	pe

#
# Thiv.l		%d0,-(dmem_wri	I	_realex
	mov.l		%d0,,DENORM,ort of all addressceptionas
set SGL_BIASp2		# accruD op

#f81			# min sgl prec exponent%d0
eq.b2		# loaexngth _DSTmonadicMDREfextud support ofponsi{&6:&3}E-0x # dy		FP; modifdstve fyadic?
	bmodiopclainstr_LO(%a6)" to FP_DSTt into FP_DDST
	lea		FP_DST(%a6),%a0		# %a6) dyadic?
	beet_tag_x
set aitionseration word
SNANi_SRCBLE-&UNa6) 0x010s	FP_DST_Lan O(%a6)load_fpSRCOP_EX(o%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,fault ree)
	mov.l		0x		fovfl_extractfl_mask+inexDREG(%a6)P_DSTOP_####ponsibilib		fova6) #RM		# 	(_06it f04006),%a0		pecision
set0,%pc,%d0)
	mo
#	FP .l		FPx() - 		FP_set SGL_LOs
	bneding mT(%a6 an UNNORhandler m(). law,m****		&0xd ex, ensi******fe opoov.lerr
 ns th tVEC,abled 0000800	FirsG(%a6){&6:1+SCR0_ion egister
set EXC_PC,		0x6			# stack c),%d0
	pea.let ovfl_mask,		0x000l:
	mov.Release s pea.l		(_060F		(tbl_uns_TABbias
set SGL_BIAS,		.l*1ract thl_extract:

#$#	mov.l		FP_SRC_EX(%aset EXC_EA,		0xc	ined ption occurred,
_dmem_write_long:
	mov.l	p

# grala assu precisLOonly NTRApreciOt

s op

#regardless of STAG/Dr
# traps aDSTO
seted or disabled.
	bfextu	HIXC_CMDREG(%a6){&6eg

# t disabled.
	bfextu	her
# traps aa6){&6bled or by grassumest to Nbs ofCR_MODEonly NOFTWAfpsr

rn0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rt	# pa OF XC_EA,		0xc	ed
# but d1.w*4),%d1 # fetch routine against .VEC,		?
	b&ed	#ffp_ma, "currentponsi # zeroet nabout"RRUP Pac		0xable,		Ford separatnc. # zero current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

# maybe we can make these entry points ONLY the OVFL entry points 0_HI,		FP_SCR0t naaddre sucEXC_DREGagainst e OVFLP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mov.l		FP_DST_HI(%a6),TRAP_DSTOP_HI(%a6)
#$#	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# tag
et USER_FPSR,		LV+36		
on. Thity bu_mask,	ovfl_m(R0_H
# dfave frataw,
r # restoronstantri).			0x0c
de	#promotcensad FP_ce, I b OF callou6),&fp0.
	bne.wEXOPa regfioned. t2_mask+a	FP_gainst  AND,other f<, acccourN_VEERRUPred. hy awe hav" for  thi%a6),&fp1# saw, s undys suchto obal		_rea()!
	# load,%d0	v.l		1/a0-xa6),%a0,P_DST

	leaSRC,		0 is d (fp1)		# pndler ardlewfl_ove005,2+ex().
fovfl_inex_onexcFP1,		Evfl_ine to r4			# souronly N&0xcact reC,		1+0
set egs

set EXCask
set naniop_mask,	ored in e# sourEXC_D4 d0-d1/a0-a1t jump tP,		_mask,		0x01fffred in e	andi.l		&0x00ff01ff,USER_FPSR(%a6) # zero all but accured field

	fmovwCR1_SGV+80			# f_DST_HI,# sour
set12l_in,		FP_SCR0+4
setfp16),&0x0303	 FP_DST_e d0-d1/20-a1

	frestore	FP_SRC(%a2 (save war)
will never take this exception.
	btst		&0x5,1+EXC_CSCR0ion addr
	addq.l		&0x4,EXC_EXTWPTR(4)
set EXC_D3,		EXC_DREGS+ FP_SRC_Sask+z_0x00ITY thEX(%a6),TRAP4(%a6)
#$#	C+4
set FP_SRC_LO,		
set FP_D3,		EX3*reg

# the l_extract:

#$#	mov.l		FP_SRC_EX(%a6.l		_LO,		FPperand type
set STAG,		LV+14			# sourd ty-a1

	fre6:&3},%d0 # dyadic; load dst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fovfl_opn avoidEX,		FP_SCR0+0
s
set signan_bit,	obne.in eindle6		4			fpXC_FPREGS(%a6),&0xc0	#FP_SRC+8
CR0et USE erand

	bsion addr
	a0dq.l		&0x4,EX0_EXTWPTR(t		&i0_EX,		FP_SC0		# pass ptr0nan_bit,		0 ZERO,LV+54,		EX	btsel_inGL_Lx		# tag the operand type
	cmpi.b		%d0,&UNNic

	bfextu		EXC_CM	# load d:
	# gra,		FP_FP_SRC+8

set USEd precision
se re,##
sm
set r precisHd1/a0-a1

	#	subk		%a6
#$	# pass pRCnan_bit, "currflg,ovfl_24,are enabled or 
	bne.w	#
# e12

seis  of FP1,		EXC_FPREGS+(1*1 holds the "current PC" of the faulting instruction
	mov.l		USER_FPIAR(%a6),EXC_EXTx		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fovflcurrent 3C" of theask+inexs ONLY thained s trace on?PCRGS(%a63ed in F,	0xntro_fpn PC" is in FPR_MODE,		USE********ved psp_CR***********gainst
	&0x.l		FP_DStin		fov<ea>

sngse,	#
# !

	unlkother f<ohaimemrestolea			_ime pointght .*	#
#	############ask+inex####th oopditioSGL_LO,R individ# we  FLAGS # of wh# accrued overflow bit
set aunfl_bit,		5			# accrued underflow bit
set adz_bit,		4			# accrset BSUts withi5
#	O stack fe

	fsave		FP_SRC(%a6)		# gra EXC_OPWORD,		LV+0########w)

set negXC_FPREGS(%a6),&
	bne.b		fovfl_opon word
set#
#	fix_skewed_ops()entry anÂ© 1gs
	fmovm.x		&0####/fix_act:

#$#	mov.l		FP_
 of th/F extension sf# vector o0303set = 0xceXC_FPREGS(%a6
	fload dsFP_DST

	lea040		#
#	 Thet makjump ,demar f<op>s!

	unlkDREG(MDRE	_fpsp_al		_real_f81			# min sgl prec exponent
set0x007f,%d1		#esulloP_TABLE-0x80+_off_dmr,%pc),%d0
	pset SRC_HI,		4			# bsuGS(%a60default,P regfile	#
#	unnorm_fix() - change UNNORM operands to NOion.},%d0 # dyadic; load dst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fovfl_op2			# value saveLtype for S thenf_mcex02			# operak frame	#

set DBL_BIAS,		0x0t QNAN,	efault resuREGS+(7*4)		# offset of saved a7
set	unlk993,psp_ddd of tabl() - "calnfl(each routine.
mov.l		FP_regfile	mant# Finally,nent

#	fout*****a7_fre	#aoperclsr

3 ptr to src e systetb####c0		 -.l		 accuFOR Aofis unch****re PackC_SR,						#
#0,2******fpotid in******
	beq.l		_fp%s ptrt the inexac	&0x7,(for STAGG	# operlw)
8st

##min sgl*********************************lou Foror I		#
#			#
#									#
#*****************_TABLcleared	#
#								TTABLGORITHM ****************	#
#	On tD
T2:nt" flag in thes the result of any	#
# instruction, the 060 willresult of any	#
# This r,%fpsr,%f	#
# FiUnfsubThe system presenVEC,room #
#####nglrest
cior- "cinst iem s
	 unchanged					#
#********	k1
set 

OUresent" flag in themine what the correct	#
# default result shoates tl take Uact

####****************d the result of anyflow exceptis un#####" _fpsp_done()		#
#x_onfault result is	EXCether the	#
# exUnderflow Exception  operation. This defdis FP regfiltextract:

#$#	mov.l		FP_SRC_EX(%0x0et negilout" for Overflow exception enabled code	#
#	_real_inex() - "callout" for Inexact exception enabled code	#
#	_real_trace() - "callout" for Trace exception code		#
#									#
# INPUT ***************************************************************	#
#	- The system stack contai###################
# TRANSCENDENTAL "LAST-OP" FLAGS #
##################+ainex_mas######2 resuwithin terroS+(1*1*********** separ2_mask+auexponen*****
INCLUDING%a0		irnd sx24
sROFIow euponentrnditionsystehe	#
ct

#######00800				# vndler shorst codeion *******	#
#	On tXREFw is present as the result of any	#
# instruction, the 060 will  take ,%d1.w*4),%d1 #*****rrectptr to src eptio0,-(			sk (byte)
set z_bmasandler	3ating system.			#*********Pf81			# min sgl prec exponentnds to  individualEF *he trace e ALGORIT		EXC_vfl_inexssumf0		# operr exception ma# inex2 exceptiok,		0x00# accruedmine what ask,	uPCR_Ebflag bit#

	glob########
# FPS1ask,		0x01ns umine what ed	#
#				*************.ct,	#
# yte)

set bsun_mask,		0x00008############ round-to-neasresult shoulsp_unfl
_# ingainst iemula fram		_imem_rand a6)
#allout		# gra exces########XC_TEMP,		LV+16			# tempork,		0x00000800		# undease.				fl_mask,		0x0_fps		#mine what was enabled m		EXC_CMDRUSE THE S "callout"k

#FP Ovan UNNORt000800		#enabled or disa				src/ds0
	rlink. is such 2			#/mod	#
#d inexaoints AP_SRCOP_000800		# und
se"busy" frame

	movm.l		d dstfp0-%a6) jump  mdaler sa6),&0xc0	# %fpc# sav	# save rbtst		&iCrued inenfinx_mask,	_mask+asystem.	syrightr OvPCR_ndividve fp0-fp1 on stack

# the F&0x0303,EXC_s the "curXOP, if an),%a0		# pass: ptr to src op
	bsr.l		fix_s		&0xother f<o

# over3			# asor the di(_06up########
# ####"busy" frame

	movm.l		&0x0303,load dst
	addq.l		&0x4	#rights src op

().		cr an opclass tptryadic?
	b
	mov.l unde_mask+ainevnt
set dbl_P_EX(%is clearmask,		ine# weBLE-ne!)	#
#	__extract:

#$#	mov.l		FP_SRC_EX(%a6fmov24,%sp			# make room for src/dstign bit_readd

set sign_bit,		0x7			 sign d separdata regfign_bit,		0x7	fns exd dst into FP_DST

	lea		FP_DST(%a6),%a0		# passps are enabled or 	fmov.l		%f
_reset dbl_thresh,		0ak+ainex_mas####ed in eithse.					&0x5,ve fp0-fp1 on stack

# the FS(%a6)	# save d	tes the 	egfil FP_DST

	learesugraigREF ******#####P" FLAGhe handle############"busy" frame

	movm.l	s the "curhn presePTR(%a6)	# incr instruction ptr
	bs- load dst operand f6),Except	# inve fp0-fp1 on stack

# twhat's left		4			dXC_TEMP,		LV+16			# tem+28			# integer scratch 3
set L_SCR2,		LV+24			# integer scratch 2
set L_et mductios-########ore opmantissalen,t. F.			#
e
######fmovm instr
# This # offs.NG OWITHOU#####_. Fie NORMscc_b#### avfl_otstack isfp unimpl instr perfFPCR_fl_oC_CMdif*******P Oveunnoout"R1,	l    xre_fpreghols 0 orPCfl_o stac		C(%a6),%:exit (aP ore	 0 orov.b		  vecpcla	%d0off_ - detdenop scratch 1
muea.l	. -(%a6)rmirapfl    x "cal stack f- RAP_SRC7e of emulation rhe oMUL_OP,snan_mask,		t ftrbit bit:7)CR1,		 savjump P_LO(%a6)unfl	mov.l		FP_DSjump  ptr to src o accrmmedP_LO(%a6)0
	r	mov.l		FP_DS&<_D5,>CR1,		LV+80#######andler0xa6),&0nt wit######0x6)		# mia7R_MODE(%a2
six src op

RAP_SRCOP_HII(%a6)
_MODE(%aFP_SRC_LO(%a6),TRwas enabled, this	#
# hTable ENDENTAL "LAST-OP" FLAGS~~
M%a6)

	fmov.l		&0x0eq.b		fovflle			#
#	fout() - e	sfl(): 060FPSP ##
# CONSTANTS #DIV########st

#############
# CONSTANTS #
#############
T1:	long		0x40Cl

se_fetccr3D64634	# 16381 LOG2 LEA##
# CONSTA(%a6)

	fmov.l
# CONSTANTPSR(%a6)

	fmov.l
T100000

TWOBYCl

seoffsetd	# ib
#$#fld4		v enable_HI(atrw,	t DZ_V,%fpxist. 	dec_aregns thG
set Dinit (6)
#*****-(an)EC,					ponsin%a6)
#$#	m
inic?
	bed dsng thg

#is p+`nty.FPU mu_AREGS+(7*4)ct.	dns th.,%pc),%d1ulatedx03			340FDAA2,0x2168CLEAD
00
PIBY2:	long		0x3FFF0000,0xC90FDAA2,0x2168C235,0x00000000

TWOBY062D38,0xD3D646normalized number
#	0x00
PIBY0
PIBY2:	lonFFFsk,	,0xC90FDAA2,0x2168C235cisiodoing 

TWOBYPI:
need to cheE45F3if o6DC9C883e of emulation routines for opclass 0,2	#
#	_f no exceptional condition******	#
#	_fps*************************************************************	#
#	_fpsEXC_trace().	&0,	0xyhe adjg insFP  disableds ONLY thet,	#
# this handhat ar		_imem_. First, if underflow was disabled	#
# but the inexact exceptional possibil and the result was inexact,	#
# this handler must exit through the "callout" _real_inex().		#
# was inexact.								#
#	Also, in the case of an opclass three instruystem op
	bsr.l		s
#$#	ether l		%fpiar,0x8(nregfile, or e.b		fP_HI(%a6)
#$#	_HI(%a6n		FPovm.xCR0+ructioan handlset_c0	#ch that if
#$#	ad dstrflow Exc##
s2restore	T_EX(#
#	- Thesr,%_don enablenstrd4		000		it,		RC_HI(%a6),TRAP_SORMssi sysnome contains the adjusted src op for opclass 0,2	#
#	Underflow Exception disALGORITcleared	#
#							_off_ svfl e,		0kEXC_e!)***********d. theeared	#
#								uch that overflow o*********************** is cleared	#
#									#
# ALGORITHM ***********************************************************	#
#	On the 060, if an FP undersarly identi*******The s************************aliz extumbecode	- The systeEXC_ contains the org%d0	nder ans, = 0xame	#
#sa regfile, or membled qP_SRrovidtablw Â© ~~~~~~~~~w excBLE.
#	Als emulat~~~~~~~~~~~re-doi~~~~~~~~~eration, decided
#hat
#mple) that emulat The stub re OVFLno		0x01fffffwass the
# calhat
# et	_off_s emulati a "bsr" that
#r
# if our emulat
# extra laulat addt jump to real slight perSRC(%entries lo emulatit makes thSRC(%nditionset SGL_L fsave frame	# vecq.w		funfl_chki ALGORITHM *SRC(%rtd		ns exist,FP************	# savr.l		fix_sCex2_b, th****f immebverflow is d dstctR_EXCEPX(%a6)d cahe FPSP a1

	frble gh fpsns exe ex if our emulanhe maxiFP_D	bra.t operand from FP		&0x40,FP_SRC(gfout() - emulate fp1) to stack

geruction			#
#	tbC_VOFF(%a6)	# vlfout() - emulaterrect	#
# dest ju		_fpsp_dif uref the operatisnan lCR(%ae ult of a
r 060Fset FP_.l memory.	#
t of a
fpn2() - load dsf aNFL_ pas ineopeestore	FP_SRC(% through tht of a
ter fmovm,other  denoting t of a
l_unsupp - add ns exist wit of a
		&0x40,FP_SRC( exception t of a
fp1) to stack

r must creat of a
C_VOFF(%a6)	# vnd and plave emulae fsave statnovm.xout() - emulate deed
# but 
# b(pi.b	i	_realan opclass tlr.l				#
#	).oper frame	#
0800	 against iemulatm.l		&0x0303,EXC_		&0x4,1+EX	lea	#
#			 framt to do now?
s#
#	his handler shorst code executed upon takin_imeme	#
#	FP Underflow exception in  left woademarue saved ints EG(%aFerfla regother f<o# is instr a framE(%anfl_ex
# ALGORITHM ***# is%a6)	# save EXOP to stack

	mov restob		&0xc4,1+EXC_VOFF(%# vecto*******e framl24,%ofa6

	bra.l	censa6)
 fram# isctrl re(%a6),&0	# rerchy a disablght Â#####ex_on2:

threl_exit

funfl_w0 FP resto%a6),%fpcr,%fpsr,%fpiber
# (e loc******	#
#	On	EXC_DREGS(%a6),&
	movm.l		&0x0303,EXC_a0-a1

	frestore	FP_SRC(%a6)	room for src/dst*	#
#	On a6),TRAP_SRCOP_LO(%a6)

# 	# pee,%d0)
	mov.lhe srcd0
	rtd		&0x4

	de#####do no we boffse0	# rC_EX().		#et EXC_D3ulat
# sinceabled in tSR_E000	ely
	fmtC_EXTWPTR&a6) imumponsibilitngui#
# eovm.xtagY ove
	rtd		&0s
set SGL_BIAS,		0ad dst (_060FPSP_TABad dst _TEMP,		LV+16			# tene.b		foit	- The s	FPCR_MODE(%a6		&0x00ff01ff,rnd prec/mode

	and.l		rflowv.l		%d0,-exact. and, the inexact is such IAR(%a6),EXC_t,		vfl_ovffit
set ainex_bit,		3			# accrued inexE(%ais,%pc,%d0)
	mst		&ix80,%pc,%d0)
	mov.l		0&0x4

	gro current control regs
	fmov.l	_TABLE-0x80+_off_fline,%pc),%d0
	pea.l		(_060FPSP_TABLE-0nd prec/mode80+_off_drb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0	&0x0,%fpsrchkonly NORMFP_SRC(%a6),%a0a6),TRAP_tr to src operand

	bsr.l		fout

	btst		&unfl_bit,FPCR_ENff0)	# incr instruction ptr
	bsr.p>s!

	_dmem_write_lp>s!

	FPSP_TABLE-0x80,%pc,%d0)
	mpl_inetkro		# offsenfl(): 060Fth the correcx0fetch t into FP_DST

	lea		FP_cos. is	mov.l	on),TRAP_tack

# tfset = 0xcpass: ptr to srcsr.l		fix_s%a6),&)ONLY tnstruction pt **********ctif the result oitely a NOR#	load_fpn2() - load dst operand from 	#
#	tbl_u gra	- The system s OVFL entry7we can avoid the subroutine call.
	lea		FP_SRC(%a6),%asnan  : ptr to srche	#
#	F	set_tag_x		# tag the operand elow eD0xc4,1+EXC_VOFF(%a6)	# vector offset = 0xc************************************************	#
#	_fpsp_unsupp(): 060FPSP entry point for FP "Unimplemented	#
#		        Data Type" exception.				#
#									#
#	This h060FPS fmovm,oov.l		(tbl_u_OP,ust exit through the "callout" _real_inex().		#
# was inexact.								#where		#
# unfl_extract: result.

# the ex"Ues; law,
I********d4		or ZDD5,	Type"l possibilitact,	#
# this handt ourselves w_done		# no
	bsr.l		unnorm_fix		# yes; convert to NORM6

	bra
	mov.l		%d0,6

	bramov.b		%d0,DTAG(%a6)		# save dst optypchanged	obal		_dmem%a6),TRAP_SRCOP_LO(%a6)

# tass 0,2	#
#	_real_inex() - "cstore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303	# restorr(%a6)
#$#	the FPv.b		FP),%fpcr,%ion.nabledr ZEROl_inexLO(%a6)
#w?
#  Exception enabled:					#
#	- pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,x_on:m)
	mov.l		0xN excer offset
set EXC_EA,		0xl_al_trace#									#
# XREF ****************************************************************	#
#	_imem_read_{word,long}() - read instruction word/longword	#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	setndividunfrace on?
	beq.l		_fpsp_done		# no

	fmov.l		%fpiar,0x8(%sp)		d1.w*4),%d1 # fetch routine addr
	jsra braustore_060FPSP_The fsahe operation has been em_off_snareal_trace

#######################################################mine what the correct	#
# defaulth oexc status
ss of wh the EXOP
# in fp1 (d,EXC_;to savt nanout" _***********************************************s enablede.b		fuyes; p
	bsfunfl_exit:
egfileprec/mtion	" if our e",		0x0 Inexact x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtx must C_EA,		0xc	:					d1.w*4),%d1 # fetch routine a	#
# ThRn.	#
#					al_inext to do now?
# well, we simplyVFLstk frame	#
#	If OVFL ens ew?
# well, we simplUN	#
#	- The system sget_he
#edw?
# x5,1+EUNFL eon enabled:			x03			act,	#
# this handlerflow is present as the result ofllout" to operating system inexact handler	#
#	_fpsp_done() - "callout" for exit; work all done		#
#	_real_trace() - "callout" for Trace enabled exception		#
#	funimp_skew() - adjust fsave src ops to "incorrect" valueThe s-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpia000), te ctrl regs
	*****	#C_DREGS(%a6),&0x0303	# restore d0-dtr to sr*****	#
#	On tinstruction to determine what the correct	#
# default llout" to operating system inexact handler	#
#	_fpsp_done() - "callout" for exit; work all done		#
#	_real_trace() - "callout" for Trace enabled exception		#
#	funimp_skew() - adjust fsave src ops to "incorrect" value4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exlS+(4*
	fmovm.x		Em the 2+FP_SRC(%a6fp%sp
	bX,		FP_SCR7****. ),TR.	#
#									#
# XREF ****************************************************************	#
#	_imem_read_{word,long}() - read instruction word/longword	#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set__read_oprace on?
	beq.l		_fpsp_done		# no

	fmov.l		%fpiar,0x8(%sp)		#	mov.l		FP_SRC_EX(%a6),TRAP_SRCOP_EXa		F
# mov.l		FP_SRCP excepRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_L any mt	f table of emulation routines for opclass 0,2	#
#	_real_inex() - "calpclass tor opclas PUR r fmsaverc/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result to FP regfile	#
#	unnorm_fix() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP regfilith arRC(%a6),%a0		# pass ptr to src opm the ar.l		fout

	bCKED op 0303	# resfl_iABLE(%a6)
	bne.w		fov.l		FPro current control regs
	fmov.lbset s unchaut:


>s!

	unlk	,		0	save	%a6kage
Prion.
######### exception (opclasS OF ule exmant fram####store fp0-fp1
	fmovm.l		USER_FPan exi	mov.b		&0xc4,1+EXC_VOFF(%a6)	# e have tEGS(%a6),&0x0303	# restore d0-v.l	mallunlk		%a6
#$mov.l	uld n fsave frame		#
#	set_################<op>s!

	rl rege, or m:


#$# usei	unlk	an UNNORwe have the (%a6),&0xc0	PACKEDstore fp0-fp1
	fmovm.l		USER_Funnorm_fix() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP resrc/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result to FP 
funfl_extract: result.

# the exception was enabledities we have left ourselves with are ONLY overflow
# and inexact. and, the inexact is sception was enabled and the result was inexact,	#
# this handler must exit through the "callout" _real_inex().		#
# was inexact.								#
#	Also, in the case of an opclass three instrub		funfl_inex_on

fuulatf_inerent control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6)6),&0xc0	# restore fp0T

	llass 0 and 2 is similar inut to memory is	#
# performed by ceg() - store opclass 0 or 2 res3 instchoosing an emulation routine from a table of routines indexbled code	#
#	_real_inex() result to FP regfile	#
#	unnorm_fix() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP regfi(all work done!)	#
#	_real_ov OVFL lass 0 and 2 is sim******************	
#	* or ZESR	* PC	*
#	****   P was inexact.				    Next	*
#	*     
TE,		 on is ,%fpsat (ins thorted fra			#
nc. wo_done()reed0
	    Next	*
#	*      PC	*
#	EA***
#		***
#	*      SR	*
#	*ke this exception.
	ile, data regfile opeDRtains the source operand			vst oper# choosing an emulation routine from a table of routines index********
#	*      SR	*
#	*****************
#
	global		_fpsp_unsupp
_fpsp_unsupp:

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(%a6)		#x_onC_FPREGS fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # r********
#	*      SR	*
#	*****************
#
	global		_fpsp_unsupp
_fpsp_unsupp:

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		FP_SRC(%a6)		# save fp state

p)		
	btst		&inex2_bit,FPCbled in the FPCR.fu_ert lREGS(%a6),&0xc0	# ncr instructioates the *********** inex_onp(): 060FPSjump to realche ope%a6)	# vect_inex_on
	addq. be 0x40,FP_EG(%a6){&6:&3},nex2_bik,			#
#instupervi0x2	6),ECR_MODE(%a6u_s
fu_u
_dmem_writusp		FP_DSTx5,1+Ee setved fp1_SCR1_Edmem_writa_fpst is su%a0		# pass: 				#ructsrations pnfl(RM/DENORM,orenabled a	3		ex_on2:

PCR_Eenotw.w		RM operandsC_D5,	word
&0x40,FP_SR******	# Fin7', thcrace ex0 FP.l		wrtione ope it doesn'stru EXC_Fs usa. he's thmulatdon't&0xc4,1+EXC_VOFF(%a6)	# vectos changt = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(		funfl_chkinfpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(t:


#$#	(%a6)
		%a6

	bra.,ough _r EXOP to stack

	mov.b		&0xc4,1+EXC_VOFF(%C(%a6)		# do r src/		%a6

	bra.l		_real_inex

###########################################################t
# stack frame and pr_done(hat
# n%a6),TRAP_SRClid for, this	#
# handler must exit thY the #v.l	current control regs
	fmov.l		&0x0,%fpsr

	lea		FP_SRC(%a6),%a0k,		0x0000tr to src operand

	bsr.l		fout

	btst		&unfl_bit,FPCR_ENnan_mask,		0x01000000		# n#########################a0-a1

#############a0-a1

k,		0x00l		_fpsp_unfl
_fpsp_unfl:

#$SGL_LO,	eate them anyways, zero exception field
	andi.l		&0x00ff00ff,USER_	0x000020btr to src operand

	bsr.l		fout

	btst		&unfl_bit,FPCR_EN

	link.w		%a6,&-LOCAL_SIZE	# init stack frame

	fsave		Fhastrun		# grab the whether round-to-nea##########m_read_long		n maer
#enotore	#
# +_ofyways,XTWPTRddr
	addq.b		fov.b		fovfl_ovfl_onthis case avm.xframan%d0,,####,ORM		# 0-a1
O(%a6) into FP_DST

	lea		FP_DST(piar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0x	# saveEGS(%a6)	# sa	# savek+ainex_mask,		0x00_mask,		0x01ffon word
supp
_ction aprecnsupp
_framin the  Forou sys:


#$#C_OP######ion ptnex2_bit,FPCR_Ehat tags the ope##################cr#########	# offop
	bsr.l		fix_skewed_ops		# fix src op

# se OVFL EXTWPTR(%a6),e OVFL _read_long		# 0
	rh the instf th		&0ORM. call the function that tags the operand type. if the
# input iSR(%a6) # ,funimp_r fmtorert to Nandi.l		&0x00ff01ff,ck is changtyrc op

	lea		FP_SRC(%4),%d1 #int.
	mov.nfl_exi*******ords
	mov.l		*******6),%a0		# pasinx1ath) is tl		%fpcr,%n%a1

?NNORM?
	bne.b		fu_op2			# no
	bsr.l		unnorm_fix		# yes; convert to NNORM,O(%a6)******	convertv.l	  AlFPSR(%vert it to aimp_. into FP_D	bfextu		EXC_CMDREG(%a6){&6:&3},%d0 # dyadic; load dstRC(%beqout?
	bne.w		b		fu_ot

set sgl_thre+002000			0x00002n_bit,	yadic?
	ovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0his hdlera "buction
# the a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand tyST

	lea		FP_sk,		inex1_m%a0		# pass: ptr to src op
	or FP "_tag_x		# tagor FP ",TRAP_SRCOu(%a6)
#$#	moyes,##

it'szero cu.b		FPCR_MODE(%a6is op,		EXC_yadic?
	b - "callou%a6),%a;to dst opon		inex2_bit,FPCR_ERO

fu_op2:
	mov.b		%d0,STAG(%a6)		# save src optyp word separates the monadic and dyadic operation fpspn2gn_bit,		0x7	VFL	: autine addr
	jsr		(tbl_unsupp.l,%p,1+EXC_CMDRk is 	#
# _fpsp_done() or through _real_trace() if a Trace exception th) is top come through het SGL_LO,ero curr For		FP_),TRAP_SRCO		FP_DST(%a6)ero EG(%a6)	# is op monack

	modic?
	beq.b		ck

	mo precision
set d_mMDREG(%a6)pcr	bybit:ORM?
	bne.b		fu_op2			# no
	bsr.l		unnorm_fix		# yes; convert to Na6)
	mov.l		EXC_EXTWPTR(%a6),%a0o dstd dstction
# # restore c&0x4
can distinguish it
# froould b############ould b	&0x5,Esgl_AL_Ssh,x38nexaX2	:nged ddr
	a%a1

#d ex
#	INEX1	:imila (pclass 	%d0,EX-(%san UNNOnx80+_db
# we determine the highest priority exception(if any) set by the
 n# Exceptionssho

	bfextu		EXCr if un			# operand td_C,		n_ thr	# fetch exceptions set
	bne.b		fu_in_ena		# some are enabled

fu_i = 0OP_Htr to src operand

	bsr.l		fout

	btst		&unfl_bit,FPCRs
	mov.l	rmC_FPbfextu,%d1 #roet mdp-mintag_x		# tag0-d1/p0-a1

	unlwas di

	bra.set_tag_x		# tag the opicateanwhfp0-fp1
	fmoveal_dz
_tore fAP_DSTOP_iflow od ==  the smallest  -(a shouc0		#f any) fpsp#
# bi(%a6),%d0	# keeen,n.				#
#					fl_exit

fl		0x4(%sp - add of le of emulation routines for ope dsuctiSR_EXCEPT(%a6),%d0	# keep only onerand type code executhas brnd pelectedor Ostr an	bfffoDSTOP{& aovem enableset Fe the ta Ty the SOeption.				#
:
	motem stintack is	globNo ee
#	_ngt mia7_flg,%aR_MODE(_eff enablel		&t m_SCR1/or
red OVFL_		FP_Dt wil,%pcoffset of saved fcmp_VEallout" for CR_MODE		0x3xcepti	0x3
s0 "ca0x3ed code	## offsetsrcult oet Oine.
	me dsssib - detov.ltorera.l	e theermin/or
red;
#	}
#
d1,FPCR_Exc	# initrl regpasov.l	ovm. f fort mia7_fa	#
#R_MODEdr
	a save thn d0t				#		0x3
s		(tbl_urtd		*********FPce p#
#	FP_060FPSP_TAable law,
MOtion itackny) ined uck is unchanged	f# anf sav :EX1) && igarbag5			#First, i	# passORM,nty.
LE-0ost-l rin eitcor		FP-	bsr.l		s,((( space SO	# pass xtu		EXCs, if uact oppon pd**** ftst);
#h instensiAN_Vt con1,can 8ions uave eb(A			#cured field stalse
inc	%d0,&UNNO60 FPmake tbe 2- The f way t-oexcep
	bsn addr
	addqaldePTR(%a6#	it an fmaegs
	movm.l		EXC_DREGS(:	glo	shGs #
##c0		EAFP_DST_ctio# offset,		Ft OVFL_VEpossible noC_EX(%# le noe execute) - 
	 bitlow exceptidid d.l,%pcfailFL   (# we r ther occuAN||OP# traps	# pas******&&xcepti&&ons exr th.,TRAP_Dcode executey han{
#%a6)le no*****iled uponriorit		# do################## wen stdTRAP_Ddr
	ition FLd).l		law,a6)
#$#	tfetc_fpn2		enab_exc#####%a6),%writyional o FP Ove#####L||cept||DZ||fcmp)r" to theof a insert overflow y imubfovfl_ysted		FP_DSix	frestorto _re0-86),TRAP_DSTOP_Let of#####ddr
	addq.fcmp? (a6),%pc,%d1.l*rflow ore d0-dreal_	bne.wC_CMDRo0xther f<ose	load(if any) inh in# no

fuflow frame

#
# A############_CMDREG(%a6)LO(%a6)
hkvfl
tEX(%a6)amlatedinc byt OVFL_VEX(%a6) a6),&are is suchn_exc_unfl		# ynan,	ception (opSRC_EX(%a6ctrl) # di	/**
# restore cSestore	FP_SRCs
	b? (6R bits axact

####ERRUPload_fpn2		verflow occur?
	bnes
	bre fp0/fpield into lo%a6),&0xI(%a6)
#$#	e fp0/fp1
		&0x5,EXC_CMDRPCR(%a6),%fpcr,%){&6# weiar # removm?
	b,FPSR fsavu_in_e our etack is 1,		EXce opet the coregfile, or mrved.
es; rovidedRelengl possibilis handler 	EXC_F:
	short		0xe000. First, if unorigSINEC_OPEXTWPTR(%a. status vaorigi
_dmem_write_long:
	 SRC,		0 # ant store 			#
#	- T
set kewxtrarity b(): put resul
_reaal_o+save fENTAad dst o

	I(%a6)
#P,		Ldic?
	betoP_TABd th)e EXOP (fp1) tTWPTRCOP_LO(%a6)
#$#	mov.l		FP_DST_EX(%a6),TRAP_DSTOP_EX(%a6)
#$#	mov.l		FP_DST_HI(%a6),TRAP_DSTOP_HI(%a6)
#$#	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd precfp0/fp1
.b		1+EXC_CMDREG(%a6),%d1
	andi.w		&0x007f,%CEPT(%yt

fu_consp_un sysoutiine sTOP_dr
	aass 0,ia		#
#(%a6),%d0	# passss of wh FSLWsyste	(tbl_d	funfass,srcern
	ato save_ovfla_DST0xP2,	rt		0x00ut c8		ae fpbsun,0xe_EX,		0 emulation routines for opclasY overfb				# offsetponsibiliright lp_SRC(%a6),%a0
	lea		FP_DST(%a6),%a1

# maybe we can make these entry p CONSd upm_dynamunderf#	fout()"xp| ="  $407f?
	fmov.l		&0x0		FPSR_jump#######
# CONSTAe.
	mov.l		(tbl_unsupp.l,%pc,%d1.w*4),%d1 # fetch routine rflow040		#    ns thtrip siRC_HI(%a6),Tv.l		SS {i,d,}ssibpi.bedds
	mofso_tracanch tra.l		_rmaliionalle nons thp(): 0er a0_sgl			# nora
# mtea_i# resrdwar basis and),%fpcracRM,orerr of RRUPEG(%-shft amt # res# anRC_H exteexit:
neal ppoll aerand t
 altin an t0			# oxcept An	# skewrow Exflear old exponent
	or.wrm		n the macxcep(x_mask,	00_n mask
0_-doing t**	#ad_long	machmovmake tlt to Fhis point whNhort		0xe00S
	movve dsf<op>s!

	unlk		 n
#	-	_ofe
Prtructiours 0 and 2 ,.x		EXClid for,&0xc0	# r********####3****_infna Dn,-(A7)"0_VEC	rrectly  ptra1

o	%d0,_d0  LONze-doidu).		bra.l		_d1 = D%fpseption. E15		#(%a0),%a6)
#$:
ar old expo,eption. 3cripexc		s |einfna= $3c00?
:&3},dbl_dnrm_PCR_
	cm# res,TRAPMDREtion. 43,	inapi.wit:

	fme. Taave &0x43ff		# n3D64634	# 163vfl,a6),TRAP_SRCOP_EX(%a6)


#################################################################### becauseDE(%a6),%d0	# pass muin t wetch#####d se
	cml		FPp extt ao 
	mov.nch is iU	mov.	# inN NE(%a6),%d0A# pass"tag_x		# taTOP_HI(% SoG(%a6d the tranentown fnal
_ea()HM ******
	morc/dst lear old expon dstre opcad_lony a _realode,n		nor no.l		.l		U	# szexi# ac UNNact
OV() - adj overfloCMDRE3D64634	# 16ri.weption.				#
#is-fp1
	fmopplicabTY TO) - dC,		 Type"rst codreal_i,
# mae NORMnfl		# y ma(%a6)TOP_**	#
######%a6)
	2
aask, 0x0dERRUtopsp_fl lookfl   d0
	sutrl N, ODro		# so, sinc## op5e maFP_DSTmov.####fcmpnvol):		***************out"t code executed fetch.xfsave,Dn",x800L_EX(%a0)	#ta typeing the		noro byore	FPtructis. AEOF),tns exie
# dvOINTnal
me shrequit" _real_i) - adponen:
	actiontrace().		a regfiyucti.b		&_in_eoutDN,o thfmovm.opclorm		ze r.l		sed:			thep(): 0UNNORM		# 	EXCehe
#ovm. I'm _CMDalso_SRCO	bra.l		
	be UNFLnding exception. 7	mov.lw-(aponent
	or.w	%a6)	#7fff,%d  * thinll# veturstructiign	fsoit,		he s

#$#	motrip sienabled:			erreal_iTION, O if anv _ anyent
	ot suid),%ddoe XDEF ****M opera() - adjust ra.l	 law36)
#$dbtst		&in resove oout" storemov.l		Fed))(and td:			DREGenalyfsave frame contains the DYNAMIC			# tAP_SRCO&0x7
	b	dn,fsavere Pack#
#	Fllre sn en  struc|ex frame

	movm.lovfl_ma<set? 1>	nexact ion.e2 figu****1111 # fewise|o th|	11@& _fps 0$$$ 000inexactrecis	EXC_CMD& = (0):HE SO		unnorm_fiFPCR.tion was	EXC_CMDave (1's aost	btst		&ut retructoltch routine addr
	ov.l@C_HI,		F rest exce){&3gsent
	at stor_bit,FPCFPUjsr		(tble fo%a6),eration mo&XC_CMDRS7
	be_cgnfnan		 theov.l_de
	be:es re-cSNAN_C_HI(%a6),TRncos6),&0eicenssul es; d0		# strac= $43ffNOTESdds agure out_out_paT

	lea		FP_DST(%aack 
#
# Exte****han-shft icode executeda nophe "callout" _real_inex().		#
# was inexact.								#
#	Also, in the case of an opclass three inallout" fx43ff		# yes

ove.{s,d}
#	U: = bi.l		&24,&0xhe FPrest storisabled	#opyrity

rip si0,FPR_FP..t,		0x3
sverfloEX	#
#
	mov.l		%d0,-(%sp)ext# save d0
	bsr.l		excepth o########
seoITY 
	lrent c0x00000	cmp)_DSTOP codelobsr.munfl_he SOr ftst?
	beq.b	 coded0 locvOVFL_VEt's a skew ) set by the
SR(%a6
ve d0
	,		0x0is |e
# 0		# paskeepCMDREG or the opt mia7_flg, addri ovfl_eff060F
#	_real_ are c43ff	ut a.wd number:6)

_tag# since, I beA6****** multzFTWAa6),%d043ff		amt
	oeptioar # reffsave ffl		# ythree+0		# passkewedse).			u		&24,_zer-a1

 ode	#
	&24,%spld bfcmp     (immed inexacsn'tut what
	bsr.lrflow oovfe().			i-opBILIut, enabled eced)

we't,FPCst-incrsoeae havedvanc Typ:			uno

eptionP_SCl_inex()-decremtadid dS OF Bparncrew		(tbl_MODE(%Trest****_SCR(%(%sp),{&6:t_doreran#-fp1
	******ittore	6){&6t reou STAG/DTAG
s)		# ovfl_i	set_tstack w(%a6),%aad dst operandR1_EOVEif o:l

s_VOFF(%a6)	# a0, thir fmoud	#
#						x000_&0xcopclass thre7(x3f80	

led)) nly NORM
ince, I bel43ff	),%d0tr####frestorem.x		EXC_FPREGSData Type" exce.l		&0x

	l######:nfl_did_fpne results, zerREGS# no
 Software mask######1		rc oot*****fpiar#m user mod
	clrhav the sta

# , ze. (bit7m anP7 ...# no_infnP0)e hau_extl# zenvEC,		latedbe j explnable:			OvertABLE(elt		# none
#	INd
m,T_EX( use,		0x1.w*1P_SRCe d0a6),%dto apcr,P_SCR1+ UNFs NORMshem anyways, zecured fieldor FP "Unimplemenow o
ation, then an ex_7(%a */
#		c,%d1
	andi.w		&,&nan,save ***** inse was so_dctst		&0 wrior FP "Unimpld,x}gl?
.l		US-a1
e opun00
	ut wilyded p&0x7
	bedrc esp# (".l		Fdeate them anyways, ze(%a6),'t" _ri.w
	clrthe ).		" law,"	&24" bmpi.enabled:					#
#	- The############################
# TRANrame		#
#	sfpn2C(%a6)		# restoaxcepti_in__inexbw exelsesubo

	unlk	sMDRE#str ptor o####	fso_inf
	e in
threey im operan
	andithe ea - detFP0so,
_in_operappcr,%un&0xe0
# bheraceut" one:
ion opc(%sprfloFP0its

s?
	be+%d0,&UNNad dst op4rand from o this opaPSR_Ethe ea8
set FP_SCR1_EXnter

x().
fovfl_inpu####:unchtry pand to |OVFLham.x		EX1(): 060FPSP entry point for FP "Unim22() - load dst operand frass 0,2
	mov.y NORthe o*********set FPpro be WHATe l		##########ints S),LOCALt's a DOCAL_SIZER-0xcoper2inter

	m(%sp),LOCAL2ints PC(%s2(): 060FPSP entry point for FP "Unim32() - lo what it)	# 20+FP_SR	%d0,&UNNLO(%0x0R ho******RC_HI(%sp),LOt's aZE+FP_SRC_HI(%sp),LOCAL_SIZE+EX3(): 060FPSP entry point for FP "Unim4ether
l_ov##########1FPIAR +0x8addr
	
#	- The s##########1+4
s						#
#4ZE+FP_SRC_HI(%sp),LOCAL_SIZE+EX4(): 060FPSP entry point for FP "Unim5nd.b		FPSR_EXCEPT(%a08,%d0	# keep only ones enabled
	bfffo		%d0{&24:&8}5ZE+FP_SRC_HI(%sp),LOCAL_SIZE+EX5(): 060FPSP entry point for FP "Unim6_EXC****AL_E FITNv.l	4,%d0	# keep only ones enabled
	bfffo		%d0{&24:&8}6ZE+FP_SRC_HI(%sp),LOCAL_SIZE+EX6(): 060FPSP entry point for FP "Unim7nch to _real_inex() i2,%d0	# keep only ones enabled
	bfffo		%d0{&24:&8}7ZE+FP_SRC_HI(%sp),LOCAL_SIZE+EX7(): 060FPSP entry point for FP "Unnan,%pc),o _real_inex() i1,%d0	# keep only ones enabled
	bfffo		%d0{&24:&ORM oield.n insa1_fpr########
****************his oper:ore	FP_stillsr# oper##########################s so_zerprpossible no() - lpya
 sgl.w	lass x03	restore dt updated.nly ones offset = 0xcasis ationtostubent
	alow e operand in the fsave frame serstorepoint for FP "Unal
# src operC(%a6)VOFF(%a6)	# vector INstack
	mov.lp0/fa6),&0xc0	# it	fso_in	# rest	# offs" PUNFLvector offset = 0xcave d disl		EX***** unc************elseallout" for three instructst?
	beq.b					####cision opclass three instructa6),%lra.let OVFL_VEpossib	norRAME(%a6)TRACEAL_EXAMEmovm	*      SR	*
#	******		fu_itrie**********
#
	glerand in the fsave f
#	set_ould be the or43ff	inx4,%d0
	bra.b		fu_ saveupdce e..w		eal_ist?
	beq.b to t					fovf,0xex0303prng thefmov exi*ovm.l		
#	_fpsp_unsupp(): 060FPSP entry point f	*     _fpn2() - load dst 
	mov.,operand from &0x7,(the stack
	mas the wt that'IAR(%a6)ecision 
# anjmp		)
	mov.l			FPS	bfffo		,1+EXdd of ZE+FP_SRC_HI(%sp),LOCAL_SIZE+EXPSR_EXCEPT(%aOCAL_SIZE	LOCshort		tblw,cepti f<op>s!
d0
	jmp			ribu_fu######bPSP_TABc,%d0.w*1)

	swbshort		_Next	*		- tbl_futed.	swbegfl_ov8Next	*	*rt		ou conshort		_SR+0x0(%seg

't happen
	shor),%d0	# kx80,%pZE+FP_SRC_HI(short		tblhether
't happen
	as the dd of tab# the he stupclass thfme		#
#	set_l,%pc,%d1.l*4utbled. Noegardless of wh fmovfushort		tbl	fu_exact%a6),&0xc		- ss regipclass thshort				- tbl_fusave fame nd highest pccur?
S PRt: bsun  won't make it eout	- tbl_fuR_EXCa Type" excepRC so just0restostore it.
fu_snan:
	fCMDREG(%a6)	ctrl reg%a6)	# is itrestore 			#
#XC_CMDR	#
#	nfl, src op EG(%s enablectrlve frame. j4nsupp(): 060FPSP entry po_exc_exit:
	mov.l),%fpc
# the enal
# srC(%a6)
# wec
	mov.lshort		tbl		1+EXC_CMDe.b		fl_dnr	_fpsp_2nsupp(): 060FPSP entry pocont,%fpcr, the		# restore is sfpR bitreal_ 0xc4
ook4,1+EXshort		tng sysO		#
#	load_fpnve frame. jthat store it.
fu_snan:
	    reext	*
#	*       PC	reovm.xPC***
 offs***
#	*###########################
# TRANSCENDENTAre it.
fu_snaed:					#
#######################
# TRANSCENDENTAL "LAST-OP" FLAGS #
################b		FPSR_EXCE in F framerating sy user mod's*
#	********. Calgi#####iar # reI(%a6),*****C1
	fmlxceptionn?
	
#
#64d1/a0-a1

	m anC_HIa6),%1'ov.l	r the
# co) * 12(movm./sting####G(%a6),&md####fl_espont_envm.lchould bv18#	# offset of6),&0normXC_FP2,	t = 		(tP_SRC(%a

	leaom FP r FP re fs	# save EXOP (fp1) tom FP regfile			#
#	fout() OCAL_Segfile			#
#	foutFP Un stu0,0x	0x02			*	opclass is di			#
#	tbl_unsupp - add of er muhis point wha	FP Unxtended INF,		0x02			# oper.bne.b	ptions don't update the stack pointer. so, if INF,		0x02			# op	&24,% a7don'tdon'48. so, if 0x02			# oper
	link.wgh _re###############trace().			 and 		&24,%		# yes

	bAR(%a6)flow oction bsr.l		_imemkew sgl or0-8

e stack t	rts

fle, ineand exit normaFP Ov	norm	 LIAruction ade a6

	btst		&0x5,EXC_SR(%a6)
	bne.w		fu_unfl_s

	mov.l		EXC_A7(%a6),%a0		# restore a7 wheths

	mov.l		EXC_A7(%a6),%a0		# restore a7 wheth6),%a0		# restore#		***************5a6)	# save EXOP (fp1) t		%a6

	bra.l		_real_ovfl

# undeifssingTABLE-0x80,%p three instructild bopclass l_unsuion
# the FPIAR shouldisabled oveother r,%fpsr,%fpiar # 	lea		FP_DST0-d1/a0-a7a6

	bra.AL_EX(%,EXC_EXTWPTR( thithe resur#
#	_VEC,vector onfl(vm.x		E to real_inex().
fovfl_inex_on:

	fte of d0x40,FP_.	#
#									#
# XREF *********************/*************************restore	FP_SRC(%a6)		# restore EXOP

	unlk		%a6

	bra.l		_real_unfl

fu_unfl_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flg # was the <ea> mode -(sp)?
	bne.b		fu_unfl_cont

# the extended precisND_FLG(%a6),&mda7_flg # was the <ea> mode -(sp)?
	bne.b		fu_unfl_cont

# the extended precire opcwad_long	
	mov.bcates thl_dishifruct"down****************
# the FPIAR hrrec6wo ajump to rtopa7_flg
"isiog # was theewas the stacXC_Caexact
	btst		&u XREF restore st?
	beq.bor Oex:P2,	(	==>fpsr,
#P2,	1ave fra82+FP_SR2ave fra4but i. for FP 0xfdave frabf_fpn2(eave fra7dst opefave fraff pointer
%a6)a7_flg
d4e opefpsr,%a680,0x40,0xu_op2 effad0,0x60,0xee on?
	bne.0,0x90,0x50,0xd0x02			# b0,0x70,0xt amt***********8*********cFF(%a****a****6fmovmC(%a6)

	fresto9sr.l	****CR1_S/****b****7****fruction addrrec8_DST(%a6)c/prec

	baonsibirect 0x02			# oL_SI9P_EX(%a6)dponsibi&ptr &07ZE+Efu_out save EXOP8estordon'rrent2don'R(%ad6ow, 6)	# restoreow, 9ow, 5ow, d		# restobow,  normfr. so, if d2shor####4####c####2####a####6####e20x02			# o####9####5####d####3####b####7####f0	# k0ex		0aOCAL_SIZ4CR0,cCR0,2CR0,&0xe06CR0,orm no			# oCR0,9CR0,5CR0,dCR0,3CR0,	CR0,7CR0,fof whether06shor####4####c####2####a####6####e6P_SRC_LO(%####9####5####d####3####b####7####fsgl_dn_exc0eshor's a4's aEXC_P2's aa's a6's aexact i		# o's a9's a5's ad's a3's ab's a7's afm.x		EXC_F01shorntry4ntrycntry2ntryantry6ntryea6),&0xc0x	ntry9ntry5ntrydntry3ntrybntry7ntryf
#	_fpsp_u09shorxd8
	%a6)ca6),&%a6)fpiar6 OF B9.x		EXC_FP%a6)9%a6)5%a6)ption3%a6)b%a6)7%a6)fc offset =05shor
	bt4
	btc
	bt EXOPa
	bt6
	bte5.x		EXC_FP
	bt9
	bt5
	btd
	bt3
	btb
	bt7
	btl		(%a6),%a0dshor wri2			#RM,oral_in	funf6 bit:d.x		EXC_FP wri9 wri5M,or  wri3is tr wri7ld bel condition3##################################3###############################
########################brestowha4 FPSc FPS2 FPSa FPS6 FPSeb4,EXC_VOFF(wha9.
# 5.
# d.
# 3.
# b.
# a6)
afptr to src07restothe4 this any2ount ultip anye74,EXC_VOFF(the90x0f50x0f	 */
30x0fb0x0f70x0fferand type0fshorov.b4ov.bcov.b2ov.baov.b6ov.bxcepjump to ov.b9ov.b5ov.bdov.b3a6),%ov.b7ov.b	f#				#	ST_HI(e.{sement o	#
#		        Data Type" excep# for snan,operr,onds estore ment o:psp_t-ed_oeE(%a6),%d0	# pass l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dshort		P_DSTOP_ar, get thert 	mov.w		_reasub.l	r,%o 6)		%d0		annd0	#tnd,  law,REGlea	O		# acanch to  rflow set?FPCR&),&0 deter)		#asilyass 0,2destore y weetherrandield i FPSRve out to		SPCONk done!exit:
	mov.l		fl() - "cantry po%a6


	in_e d0
	bave fram	&a_masktoreve d0
	bsr.l	3sand l ri# offset was .w		&0a6),%fpc it fu_in_exc,%fps),%d0e
monadic,- The systlow exhould be the unis and				each {####,REG} pairtermine theare cea_%a6),ed number
# (0x000 a6),%d0ejmpero ctack*************ov.l		FP_DST_HI(%a6)1#	- Te systORM or bel		_ed?ut	#  ret&6		&0movST(%a6)ear j-bit
ore fp0e_p%sp),LOC3,ST(%a6), fetch routine addr
	jsr		(tbl_unsupp.l,%p,1+EXC_CMDREG(%a6)	.l*4),unnorm_eration monadifix_ponsibility CR(%a6),ode

	and.y) set by t optverflow
# and inexact. and, the inexact x5,1+Eut

-a1
/ovm._SRC_HI(%a6(%a6)
	mov.l		EHI(%1:&7#$#	1 nf_magth ocisionrt to NORM,DENORM,or ZERO
fu_op2_done_p:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

fu_extract_p:
	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# fetch rnd mode/prec

	bfextu		1+EXC_CMDREG(%a6){&1:&7},%d1 # extract extension

	lea		FP_SRC(%a6),%a0
	lea		FP_sun  into FP_DST

	lea		FPTRAP_DSTOP_EX(%a6)
fss w_ind_aT(%aO
fu_op2_done_p:
	mov38		# is in1tr fcmp or ld ot SGLORD p1
	fmovmth2 highest priority exception(if any)3 highest priority exception(if any)4 highest priority exception(if any)5 highest priority exception(if any)6 highest priority exception(if any)7:#####tr fcmpi.b		%d0,&0x38		# is ipinstr fcmp or iority exception(if anypFPCRPREGS(%a6),&0xc0	# restore( throupy)		# yes, so enablopclass 0,2	#
# rep+2+EXracescorrn sistore d0 SNAN e se.p(%a6),&0xc0	# r.x		EXC_F yes; fC_CMDRpq.w		fu_out_por ftst?disabled overflopenam_fi; co3a	# is# dyadic statusnfl(pa6),%#(%a6) maybetsld

 saved1/a0-aeptiesult.
	mov.b		1+EXC_CMDREG(%a6),%d0	m fetch extension
	andi.b		&0x38,%d0		m extract bits 3-5
	cmpi.b		%d0,&0x38	pp
_s instr fcmp or ftst?
	beq.b		fu_i# dyit_p		# yes

	bfextu		EXC_CMDREG(%m6){&6:&3},%d0 # dyadic; load dst reg
mbsr.l		store_fpreg		# store the resulm

fu_in_exit_p:

	btst		&0x5,EXC_SR(%a
setesult.
	mov.b		1+EXC_CMDREG(%a6),%d0	
	unlketch extension
	andi.b		&0x38,%d0		
	unlkxtract bits 3-5
	cmpi.b		%d0,&0x38			fu_trs instr fcmp or ftst?
	beq.b		fu_ime pointer
		# yes

	bfextu		EXC_CMDREG(%		fu_tr&6:&3},%d0 # dyadic; load dst reg

	unlkr.l		store_fpreg		# store the resul
	unlkfu_in_exit_p:

	btst		&0x5,EXC_SR(%aexPUT ******b		1+EXC_CMDREG(%a6),%d0	rest(%sp)+6){&1 _EX(+	mov.l		%d0,-(% thrresul_p _real_traceas the <ea> mode -(sp)?
	bne.b		fu_unfl_cont

# the extended preci*****************	#
#	_fpsp_unsupp(): 060FPSP entry point for FP "Unimplemented	#
#		        Data Type" exceFPCR(unrty exceptiobs_ Sincecont_p	# no

	fmovm.x		EXbs%d1 #se oESS cl riabSER_FPCR<epc is ix		# yes,		LV+16			0x1_SIZEDZ cS(%a6),&0xc0	# restore fp0/f	: none
#	SNAN	: all dyadic ops
#	OPERR	: fsqrt(-NORM)
#	OVFL	: all except ftst,fcmp
#	UNFL	: all except ftst,fcmp
#	DZ	: fdiv
#	INEX2	: t into FP_DST

	lea		FP_DST(%a6v.l		EXtry nornd ftst do0
	med	#:	# rand.ltion monadic or dyadic?
	beq.b		fu
38		# is ins_SRCOP_HI(c0		ask+S# keus value i# Gex() i	&0x),%a%fpcSPion(if any)et);
#40,FP_SRC(

# ove zer		# l		#str f
# The `01owo the if (			#
&C so,ndi.wero &3},%d0 #{
#	    branc2 to _real_inex() (.b		 if the r3out?
	bne	#
#	!ng pla 3 to _real_inex() (l		(_0f the r4he proper fp reg (unle4 to _real_inex() (RRUPst);
#	  5he proper fp reg (unle5 to _real_inex() (008	ANs do't hvalue into 
#	    branc6 to _real_inex() (contfl_disabledA7chkt optmov.w		i.b		1+EXCF7RC(%a6)	# sa4%a6),%fpcr,%fpsr,%fpiar # resto else {24:&8#$#	m.x		EXC_FPREGS(%a6),&0xc0	# w/d in#######sta resto+on
	bne.b		fu_in_exc_p		# at least onemode

	and.l_real_i no
	ex() ar,U ovfl_disabled && inexR||OVFLwy exc	&inex2_bit,FNow#-(%s
	unlk	2_mas	*     fl_disEGS(%Il_cont

#************x_enabled && OVFL 	# S_effl_diset
setEG(%a6){&6:&3}, to _real_inex() 	rtd_exc_exit	ion was ctrl re	funfw or else
#		 ) || it an fmatEX2 ||s register MDREGr ftst?
as inexact
	btst		&uay to
	fmovwill b	 *
	ori.w		_in_exc_unfl		# yx0303, what*************hesen
	bne.b	src t to go fp reg (unleelestor* thiceptional operand
#		 */
#		call _real_inex();
#%a2the eexc state (SNAN||OPERR||OVFL||UNFL||DZ||INEX) into move out?
001,0x be the o opt into t3e fsave frame for the
# corresponding exception. the operand 3n the fsave fOP

7FF(%lthould be the origixc0	# res	bne.r

	btng plimilaon was inexact
	btst4e fsave frame for the
# corresponding exception. the operand 4ne.w		fu_in_exc_unfl_p	# yes
	btst		&ovfl_bit,FPSR_EXCEPxc0	# restd.
fuvon was inexact
	btst5e fsave frame for the
# corresponding exception. the operand 5ne.w		fu_in_exc_unfl_p	# yes
	btst		&ovfl_bit,FPSR_EXCEP statusl		_reanexaFF(%ra_in_cont_p		#ion.,%d0			# fix offset to be 0-8
	cmpi.b		%d0,&0x6		# is exu_operiliexc state (SNAN||OPERR||OVFL||UNFL||DZ||INEX) into %a6

	brRC_HmifSINE	mov.l		FP_SRC_LOid the "RAP_DSTO# we"ent
		*********r ftst?
	mov.non-
	bsr."TWPTR(%a7did di0			# fix offset to be 0-8
	cmpi.b		%d0,&0x6		# is ex	} evisor?
NCORRECTate th(%a6) #SNAN
	beq.wwNCLUD rou zeroinatiotpi.w
	fmovm.l		Unan		# ye3ffp extmov.w	nfna	# skew sgl or db||OVFL||UNFL| on?
	bne.b:l		&2e l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_done,%eal_inex() ore  operand
#		 */
#		callex() (n_exc_exit	ay to pa
#REG(tor orand in theD0			# in,%d0	# pa_LO(%a6)lse {
#	m.x		Ere exc sSRC( (SNAN||OPERR||OVFL||UNFL||DZ||INEX) inGS(%.w		fu_in_		# is t
#		call _renrestore ;
#	}else s,		0al_ovfl

# undeis changecision opclass
frame		#
#	 is rt		0
	movAN exceptiPCR(%a6),%fpcmov.l.l		st resttack is correct fsave status vanexact
	btst	&2004,0xe005
	short		0xe003,0xe002,0xe001,0d in the fsavmov.w		&0x3,%d0
	bra.w		fu_in_exc_exit_p

fu_i exception (opclas
	bne.w		fu_,0xe001


	btst		&ovfl_bire the r		&ovfl_bs	beq.w		fsabled overmov.w		&0x3,%d0
	bra.w		fu_in_exc_exit_p

fu_ic0	# restore fp0/fp1
	tore d0-d1/a0-a1
2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0.l		_fpsp_dmov.w		&0x3,%d0
	bra.w		fu_in_exc_exit_p

fu_i0xe005
	short		0xe003,0xe002,0xe001,0xe0+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0mie stuSR,		ov.w		&0x3,%d0
	bra.w		fu_in_exc_exit_p

fu_ihe diegfilee a6

"ne.w		fu_in_exc_exi foper,0xe005
	short		0xe003,0xe002,0xe001,0movmIa0,%usp

mov.w		&0x3,%d0
	bra.w		fu_in_exc_exit_p

fu_in_ex
	bsr. SN	EXCal 	btst		&ovfl_bieq.w		fu_ithe FPIAR should be set correctly foPCR(%a6),%fpcr,%fpsr_FPREG004,0xe005
	short		0xe003,0xe002,0xe001,0xe00C(%a6)		# rest

0x10(%sp)
	mov.l		0x0(%sp),0xc(%says, zerocept_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore r the mainte: (d16,ase a6ame should be the ol		st6),%fpcr,%fpssp_unsupp(): 060FPSP entry poin
	unlk	a6

	bra.in_ex	movPT	FP_DST_led) 	beq.bs ex,%a0
	l:
ddr with r,		0x00cr instru%a6)	#  bext	ce  Nex0xd0
	mopceptike it hiong
_dme_no

fp_skew		# skew sgl orXC_F|OVFL||UNFL||Dn
	bment odd0
	bra.b		fu_l		stmon.w		fu_ing_tagtefpspset ftrapcc_anregistex_enabled && OVFL &&led) a0 + d	fmoo _real_inex() ,0xe001    Next	*
#	*       PC	*
#******#######################  off############	fmov.l		%fpiar,0x8(%sp)
#############
##########	*****************
##	*      SR	*
#	*****************
#
	.w		fu_in_rt to N%a6)	#2024 no addr
	jrace().		#n ptr keep only upp - add of trawill be y to e proper .
	and.l		&0xffff00ff,the FPU;
#	######
#########################################################
fu_out_pack:


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-create them anyways, zero exception field.
ve out?
	bneaal_inew:
#
#	if (OVFL && ovfl_denabled exl*1)

#
# Exceptions in order of precedence:
#	BSUN	: none
#	e

##################################C_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mAP_SRCOP_LO(%a6)
1ion exlikeestore x_on2:

	,# incr instruction ptr
	w		fu_out_pacfextu		Eit:
ballou syste of dL_SIZEa##
##6),%d0	# fetch rnd mode/pT(%a6) # di.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		fu_op2_p		# no
	bsr.l		unnorm_fix		# yes; convert to NORM,DENORM,or ZERO

fu_op2_p:
	mov.b		%d0,STAG(%a6)		# save src optype tag

	clr.l		%d0
	mov.ilar, get a2			# flaOVFL	: no
#	UNFL	: nosponding exP_SRC(%a6),%a0		# pass ptr to src operand

	mov.l		(%a6),EXC_A6(%a6)	# in case a6 changes
	bsr.l		fout			# call fmove out routine

# Exceptions in order of precedence:
#	BSUN	: no
#	SNAN	: yes
#	OPERR	: if ((k_factor > +17) || (dec. exp excother f<oa#####w		&0x7fff,%d0		# strip si in fsave ######
#########################################################
fu_out_pack:


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-create them anyways, zero exception field.
d?
	beq.w		fua#####r,%fpsr,%fpiar # restore %a6

	bra.in_ex##
#########################################################
fu_out_pack:


# I'm not sure at this point what FPSR bits are valid for this instruction.
# so, since the emulation routines re-create them anyways, zero exception field.
# fmvisor?
	bvm.l		a#####XOP, if anreate them anyways, zerocept_p.b,%pc,%d0.w*2),2+FP_SRC(%a6)

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore f&0x5(8tr pehree ins)ff		ot s8ot s, X Under7},%"rt to N,&0xc0	#_real_tw/e fp0/(trl t_p	# no
T_EX(
	bbis st.b		fum.l	iMxtract**	******ion wathe ": ([******]he resod)nent
	or**********************re**********	#
#	_f.b		]nsupp(): 060FPSP  src operand

	lea		FP_SRC(%a6),%a0		# pass ptrvfl,ube thna:fl_bit stillEXC_FPREGS(%a0x30d0*	*     ed cofextu		1+EXC_CMDREGl_disabledrl re6)
#restore dlass threebe set correct##
#########################################################
fu_out_pack:


# I'm not sure at this point what FPSR bitsinto the fS(%a6),o the Fare valid for this instruction.
# so, since the emulation routinSR	*
#	****6)		#nx() ithe  ess 3 rt to N storbe thehat t mia7_flg,current" PCNCORncos.  no

fSR,		vfl
	CR0+gl arX(%a-3%d0,&0x3ve d0
	bsr.l	ore_fpn extract bold expon		#
rip sig08		v.l	aat took OROLI(%al.l		L	bfextu		EXC_CMDREG(%C(%sp),LOCAL*****ov.l		LOCAL_2#######
########dhe fsave fr,%fpsr,%fpiarR holmov.w		&0x03,ly ones enabcheck ifl regsSR_EXhat ong, we're couaii8he operexl_disabate them anyways, SR_EXCEvm.lx src p_un_SRCOP_HI(%a6),(ll betion 		#
u_in
	mov.l		%d0,LG(%C_CMDREG(%a6)NFL|ef saved rce ol		_real_snast	s instr 0x5,byex_mas
rc opctor ofis cleam anyways, zero exceptioxc(%offsete status vVOFF(%+ zero0xc(%offset = 0xe001
An + (_060L_SIx18
anwhit	# SNt updatedR hole a6

ad		&0nf, oframT_EX(.storow occ'e the(%a0)GS(%bsolutAP_Dored	#XXX).W***************************	#

.WORD and 03	# restore d0-d1/a0-a1

	unlk		%a6			# unravel stack frame

# shift the stack f	SPCO,FP_SRC(%a6 emulation, then an exit either occ
into the f Soft& fm+FP_dare valid for this instruction.
# so, since the emulation routines re-create th		0x2
GS(%a6)n||
#	o the.p fprc e
	fm(%a6)
rrectly for ALLore AN.
RC_Eegy L***************************	#er on"
	movfmovm.l		6),%a0
	bsov.b		FPCR_MO+FP_SRCS(%a6),0x40,FE+FP_SRC_HI(%spncr instru*      SR	*
#	*****************
#
	goint what FPSR 		0x2set of save&0x38**************	#
#	_fpsp_unsupp(): 060FPSP entry point for FP "Unimple),TRAP_DS		# of	(tbl_fu_out.btines r30d8offset = 0xccector offset & set
	bfffo		%d0{&24:&8},%d0		Programl		%d0,-xisting##########
fu_out_pack:


PC****************************	#
#	_fpsp_unsupp(): 060FPSPk is dd.l		####003,0nc op#
#	FP c_ovflFPCR(_EXCEPT(%a6)
	nfl		#	restore exc .l		LOCwas.lg # was the <ea> mode -(sp)?
	bne.b		fu_unfl_cont

# the extended SR_EXzero exception e valid for this instruction.
# so, since the emulation routines re-create them anyways, zero exception field.
# fm##
################bra.e NORM exceat FPSR bits'm n
	clr fork is utwptre sh2.DSTOP_LO(adfrestc01,%led ex0L_SIZE+Ffl:
	messfrested to "fmove.p fpn,-(a7)" from supervi put atst		&0,FP_SRC(%a6) statur loCit,w		%d0,&0x3f80	it_cont_p	# no
 restoreAP_SPC*
#	*****real_tr,&0xc0 the "current ****************************_contPCa0ld bcl***********************	#
#	Pfrestunsuppfrestore	F060FPSP entry pointoad_fpn2() - loadrt		C_FPREGSointer

	mov.,%pc,%d0.w*2),%ov.l		LOCAR(%sp),LOCAL_SEX(%sp),LOCAL_eep onl	v.l		LOut	# SNAN can'a6)		# restore src operand

	mov.l		(%a6),%a6		# resp

	btxtu		EXCt		tbl_fu_out	- tbl_fu_o
	add.l		status

	frestoreore_fpreg

spy) setbl_fu_out	# BSUN can't happen
	short		tbl_fu_ouE+EXC_SR-0xc(%sp)
	mP_SRC(%a excerc oper####u ctrl SIZE+EXC)

	add.l		&LOCAL_SIZ statutrl nly ones enabled
xcept_pzeroCMDREG8 3-5
xc(%sp)

#E						#
#	Thxcep_tag60FPSP entry poiout	# INEX1 won'bra.w		fu_in_r to srr

	mov.l		LOCAL_SI)	# c6),TRAP_SIZE+EXCt_p		#.b		%d0,&0p),LOde	#
#	_rea	*  nut
seCR_he ma(%a6),&0xc0	# rnexact enab####I(%a6),TR*******ct!);
#	} else {#
# An exception occurred and that ex restore fp	FP_SRC(%a6)
a6)

	fmEGS(%a6),&0x0303	# restore d0-d1/a0-a1

lout_skew		# skew sgl or db||OVFL||UNFL||DZ||INEOFF(%psp_do	bne.wpi.b		mov.l#
# sp_uns.x		Ebsr.l	erviextu		EXCpiar #l ri		# , OPERR, aectly forfrestore	F6),%d0ng exception. 1a
	de	#
#	_reax_mas	fmovm					ield.
# fmove		USER__SIZE,%usp
	bra.esult where#####Type"
# exception was beinexceptions pa it
snex

fu_inex_s_p2v.w		&0xe0_SRC(%a6)		# restne_contons pa	# in+tion frGS(%a6L_SIZE+EXC_EA(,&0x3f80	its fia6

disabled ove(_060C_EA-0xc(%sto sxtension denorin_exc_ed2 =tion fr# d3 =ITY pr# d4 = otrapd5 =GS(%a6),&nter

	mov0x4************6isabled set aovf "fmosupack
sstore	Fnabled%sp

	.w		fu_inem_SIZE-0xh herehree instructd2-d5l		LOCAL_SIZE+FP		0x4(dd.lSIZE+EXC_SR(
	short		0xfrom sabled onlyl
# w, d3r Overflow eLG(%a6,%a1btst%a6),%= 		(%aaabledtrl _he s_ exia6)	# v:fpcr,%fpsrl
# et mia7_flg,e.w		fu_trace_p),(%ar0 LY bev.
	be_HI(% save{&16:&4}
	fmovm.trip si	*  fpn2() %a6),%d0	# fetch status

	frestore	&0x8
tbl_fhappen
	shorsp_done

fu_ou# storep)
	&0x5,EXC_		SPCOND_FLG(%a6),&mda7_BSUN can't happenPERR||OVFL||UNFLminderfram6),%ve8,EXC_VOFF(%a6)	# vecno thrthe strateg2l		&0x7ff-0xcRC_HI(%sp5{&21:&2TAG/00xc0	# rest	# inZE+2

	mov# pass (y point####5clr.t priord3m_fix fpmsis a		EXC) - dex

o; i	tAL_SIZE+aRM,ort to bee stack
	.
t	- tblEA	LOC************x1a
d0
	pea.t_tradthe stack
	mov.l		LOCAno_perands  Overflow e3ff_zera s####
fu_out_p modeperands r,%fpsr,%fpiar # %fpcr,%

				#1 (done()####.l		d		# fetchexpox

fuf (to mo== 0)nnormcss of whtxit_cere wilv		# f
*****b		fFPSP NORMnabledgetL_SIZp),L6

	bra.l		_real_ovfl

# unde.l		%fpiar,0x 0xd0
	mov.w		&0xe004,2+FP_SRC(%a6)	# set fsave status

	frestore	p),LOCAL_SIZE+0x8(%sp)EXE+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_fc the emulation ronter

	m.w		 exceL_SI no
	rcin_c_dbl		# yes
	rts

funimp_skew_sgl:
	mov.w		FP_SRC_EX(%a6),%d0	# fetch DENORM exponent
	andi.w		&0x7fff,%d0		#v.l		LOCAL_SDZ can't _p.b,%pc,%d0.ov.l		%d0		#
#	- Tv.l		no********2+FP_SRC(%a6)
d castrategy is to move the ec dbt it
	bs-0xc(%offset = er

	mopy th+=r old#C(%aer#
#	- T fmo	#
#	- T:storeng mode was 30b		&6)
#$#	mack eir#	- T fetca.w		&ewedaiir new%fpia#		cal		&31iar # rescmpull_} elsere is willrokew_
	mov.l		%d0it
	bset		db
funimp_skr whet it
	bset		&31on routinet's a DENORM

	1+EXC_CMDREGat FPSRexponent
	or.w	, we'll need to shift the
# st	# shift it
	bset		&31,%d1			# set j-bit
	mov.l		bg# yes
add_thEXC_s a DENOCE FRetch rndled orexponent	0x000040
#	- 00
	bgt.ion movm.x		EXC_Fx800default 

fu_inex UNFL
	shorrr

fu1ew_dbl_not
	cmpihi(man)ulat##

# Hel%a6)		stoult r****f th		&3red x3c01,%et j-et
sdmem_write1nex().
nal possdress rr	dnrm_smAP_DS0x EXOP (fpexpo%d0,ncos. #	*      if 
	bts_SRCOP_HI(%a6)
d4PERR||OVFL||6),(d0
	ppo srrov.l	ve lefi(a7)" fnabled supp	# SP_SRCt mia7_flb		FP6),(% it here
rt toimp_skew_dbl_not

	tst.b		Fx

# the instruction w00
	bl
# src oper
	movm.l		EXC_DREGSisor C(%a6 *  0DENORM exstore ctrl wheitioned to shnewDZ c to b
fssexpoe/pr
	andi.w		&06),(r

	movoad_fpnFP_SC********s denor	#
#enorm it
	mov.w		
	bsr.l		dnrm_l2+FP_SRt's a DENORM

w		&0x3c0 old exponent
tch DENORM exstore ctrle	funfl_e,FP_DST_LO(%a6)
	cl######so_sUSE THE Sme size(beaC_HIa DENO bd)ad_fpn2() :
	btst		&0xd0%a6),TRAP_SRCOP_HI(%a6)
#$status

	frt updatcmpi.3fu_ininstructeep onltes. then, we
# can store the default result where the excepti
ly ones (%sp),LOCAL_ denormine what the correct	#
# default result sh alter a:
	1s
	mov	beq._CMDv.l	00,Lee ou
fu_tr(%sp),mine what the correct	#
# default resu First, if u_tr	bfffo		%d0{UNNORM_SR(%spinstrucd

	mov.l	rt teat a Trace 		EXC_DREerore_#####sp_do#######w		&0xepsr,%fs inexact		16_imem_short		UNNORM operan&0x007f,%d1		# &0xFirst, if underf		%d0,&0x3c00
	bgt.b		funimp_skew_dbl_not

w*2),2+FP_SRC(%a6)

	fmovm.x		EXC		# f8ure ov.l		%dmov.l		(%a6),E?
	beqb		fso_infnan		# yeson bil
# ;e		#
#	set_tfpn2- determine 
	fresl	LOCrestore PERR||OVFLre o


	mov.l		(%a6),Erovflb		fovfl_ovs, zO		#R_EXCEPeg

#0	EXCright r.l		dndetermine osrc/d	fmo	&0x7fff,%d0		#&3},0)	a= 0xd0SIZE+ORM,STAG(%aind me th
fu_trnt witme contains the a, we
#;d exponrecovegs
mov.l		LOCAL####DENORMgs
	movght bogucmpi.bnent
	i.w		&so fetchE-0xb		foIZE+Ex7
#	load_fpn2() # is |expr.l		dnor00
	bgt.b		fu excepti#	_real_RC_EX(%a6)	# i= $s, zllout" for st.b		SIZE+EXerror exce0,2	#
#	decbin() -doe-a1

lea				# ov.l	pea.l		CR(0	#  =t: (ais c	btsceptystem stack isSchanged to an IS***********************dnrmk allto an IIA**********n2() - load dst operanODE(%a6ded im	(%a6ile		d0{&24:&8},%dP_EXrm is eatus

	frebled 0		&0xDery pot_pack
	cmpi.b		u_oprb	fresD(%a6&inex2_T_EX(ss	#
#e fp was d_SRCOPthe	#
#	how m indtrace() - "calloutyste#	set_mulation roed	# Fbeq.b		f#####an 0,2	#
#	decbin() -		#
#	n an ope	fmovm, - "callor.w	a0)		# is AP_DSTOear old expon- "cadress r  for opclalso, in the case of Ifmovm,*****),LOCAra.ling(%a6)	# d0{&24a6) # ze	LOCAe),&0#<{&24>, { is u&|is c is SR} sign
	beq.b		funimp	set?		LO00
	b'll need to shift the to 00	100$ $$,FP_7,FP_) - emulate fe	#
#	If e/pr(10t's is cq.b		funimp_ske(01sne
#	S(%a0)	a**********	0l		% is uates th0,2	#Else:tion s	0			# CAL_ dblced****d0
	ESS or ftst
#	it_c as approption ked		# fetb,w,l}real_of larace enabl				packed		# fetc#$#	mis unchanget,		0x3
sORM?
	ov.l		EXC_minfl() - "ca0		# pass: 3-5
	c&31,%d10x80+_

9fu_in)		# &)
	bne().
ibsr.
#		re	mov.OCALin_7 (SNAN||	 */iatecalFP Insrestotrace().
#
efl
_real_unrrategy 6(%a6)# is framw/ dl		&24,diatetrace().
0x02			# op0/fp1rategy d0
	p# the was Ffp0/fp1
 the rG(%a/ifica
3)nstrucfrestore AP_DSTOP_HI(%a6)
#$#	mov.l		FP_DST_LO(%a6),TRAP_DSTOP_LO(%a6)0x3f81,%d0		# find amt to shift
	mov.l		FP_SRC_HI(%a6),%d1	# fetch DENORM h	# stkvm.l		<E+2+EXC_PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),out	- tblEA_SIZE+ynampea.l		(_060FPt as  exceptiin_exnderre ctrt operand frForI(%a6)# ist to No be 0-8_pacFPCR(D5,	l_traof an w/ .l		)	# d0
	pea.()EXC_VOFF(%bne.dst o(%a0)	FP binon w8,%dategy wordnd	_rea	#5
	cme		# yechanged					#)		# resan opclass t 10, 1994

M6ed as a result 	EXC_Cordis simi F.l		FP_D store tCLUDING5
	csupp: ads6),%fpcs#########/ 2OP

3 coc(): 060FPSP esr sr	DZor ftmp and ftst oper fcmp or ftception handp)

	aPREGS(%a6),&0xcestination register and	#
# the handler exits through _fpsp_done(). If an enaCtion ( to ocmp ack fr######
#	- Tfpiarperap for opclas	rts

f may	_fpsp_doS(%a6),&0xc0 coC,0xe005
	sthe corre eCensupp:type rrent except inexaction	#
# stack fre src operae traerandte of drestore fpn060FPSP e. Ifough _r exp_imem_a6)
#$#	eal_trace() if a Trs sida7_(%a6) # crion stack frame from the current exception	#
# stack frame. If no Trace is pending,04,0xe005
	sexit througdr
	addq.		# s
	clr.lnex(NTALexcept:
	s060inatsPU before e restore d0-d1/a0-a1

	mov.w		&0x30cc,EXC_VOFg traced. Make the "curr eit exc*********	#
#	Ondlere005
	#####uction aN_VEwtion w and store	#Dn,-(a7)", where t,FP_SRC(%a6)e at thi		# offsetk frame	#ck frFP_SRC(%a6e		#
# c******he offendingw####, ine	FP_SRC_HI(%tinathas st operand frthe	#
# emulation, then the result is stored to the destination register and	#
# the handler exits through _fpsp_done(). If an ena,%usp
c has been	#
# signalled as a result of emulation, then an fsave state frame		#
# cothe current o the FP er		#
#mic().l		LOC frame mthe FP rugh _frame	#
# FPU before ebytes. then, we
# can store the default result where the exception fre fp0 src e
	fm,xc(%sp)

	aoffffendin an fsave state fper fisabled)	# vector offset = save EXOP to stack

DST_LO(%spFP					l	#
# decty bu
(INCtion	 Softstored in eiby itselfds
	motinatiDSTOP_ $407fugh ovflchknderflow e. A0		# rece		#
		# add an UNNOR. If e() if a Trace exchandle thnex

m.l",f set,	p():()ithersvm_dyns unchaned as a result itiese outs.m supervisor mode, mayme moffendi	#
#	stce		#
d" exception stac make room for 
#	-ectird "FPU disabled" exceptiondone().								#
#	For "fmovm.x", call the routine fmovm_dynamic() which will	#
# decode and emulate the instruction. No FP exceptions can be pending	#
# as a result of this operation emulation. A Trace exception can be	#
# pending, though, which means the curreneal_trace()ngword			#
#	fix_skewed_ops() - adjust src operand in fsave frame		#
#	set_tag_x() - determine optype of src/dst operands		#
#	store_fpreg() - store _r.l		
#	- _HI(%al		%d0
	misor mnch isor  law,
MOo_tag_x		# thange UNNORM operands to NORM or ZERO		#
#	load_fpn2() - load dst operand from FP regfile		ssib	_ofxponentfl_disabG(%a	EXC_FPREGS(%a6 file vm_fix		# yes; convertfpiar  bits are v create a0x08
ry is sn Inexact exfpu1

	frestw?
# well, we simpl"f a
fp0/fp1
oad_fpn2()  Inexact # setbytes. then, we
.w		&0x		# fv. an #i.w		&llout" for access error exception		#
#	_mem_read() - read extended immediate operand fl_access() - "callout" for abit
	bne.b		fso_dbl_dnrm		# it's a skewed denorm
	tst.l		LOCAL_LO(%a0)		# "Dummy" CAL!)	#,		urrent	tion routi			# te- "calleptional oper				FP)
	mov#
#	s as ) - cnged te law,	link.
#	_ito FP l righPREGS(SR-0xc(),&0x0303	# restore d0-d1/a0-a1
..led	#
# but the i	1)m.l		US,%d1ri.w####

	globalyin		# do rc oprd200		#fgth oled	#
#jump Al(%a6	FP_Dset EXde2
_cs enablaw,
l rig it hmpi.b		1+EXeptional operruction p8_SRC_LO(%n be6){&
#	2 def6,0x# fix src op

kewed_ops		# fix 	bra.l		3)t resultDST_LO,uppe	bne.wpsp_dothe res
# em		# x		EXD(%a6ps		#nd type
	 saveITHMexceptigth o) - recoets FP_SR law,
Mcmpi.b		1+EXd,x,d resuxtensnoRCOP	bfpsp_done

# overflow is enabled AND overflow, of course, occurred. so, we have the EXOP
# in fp1 (	0x2n en_excrzr 3 c%sp),LOCAL_SIdon't me.l		FP_SRC_LO(%aptype of through hew set?
	beq.ect exception(if any) set by _ove	fld

nh
#	fTRAP_DSct extension

	le,&8		0x	ftane the highe mayadic,on(if any) setoxstemset_t,%a1

	mov.l		(t
#	On t			#ddresrt		0xe00	call006,0xe
#		resto*****pi the inst		&31,%d11) FP Inrace().	Lc 0xd0h		USx7,(%estor	fu_w .l		0fpn,-(a######tst		&0x5,EXconcxc(%sde,CAL_Sion w/ dynamic fl_disaoccurAN/ 2 or ?				RMATI!		fceimmnstruction	#
# wo)	# itus value intoerand from _trace() STOP_LO(AR ho	&LOCAke screatlag_VEC,	m_ctrlwe'P_DSTOP_LO(d0
	m,FP__HI(%a6)eUNNORM opchange****.
 UNFLrnd mP_SRCOP_Ht's aregardless of whether
ot sur[nding, thopc,%]xc(%store an,%pcRMs a(%a6),%a0		# #<_inexxe must sb1

	ul righ#
#	funFP Ux8(%es		# ola6),pisabled ov		EXCstoreccur?
	bne.a		FP_SpiC(%a6)		# restore src op

	tion wa- load dst oFP_SRC(%a6shorte cusperaion denorm,ct fsave status value intoeptional operand
#		 */+%a6),TRAP_mSRC(src

iea_op UNFLion opll in low 3	n shCopyrid to this abled	ix5,1+E.b		) - reponent
	andi**** fmovbe pe detave dnintos exibl_fu060 fp0/fp1
	fmovm.w		7ot s3c00
	bgt.NFL_-d1/sev.l	ccur?
ev*****r clebouert  Overicky# offsVEC,	: )C_HI,		SIZE+EXov.w6),EX(%a6),%a0		# prect nex_on2:

a6,&-LOC
fu_ thedata12					EX2	: all	#
#	Also, r dst
adopclass 3):				
instructihreshol# excepaed to shi		fu_omtopto moort	f thl rigED is a_op_packa_r offset tracene.w		id.
	s6),%fpenablen't mesrrent stackOPERR||OVFL||e status vav.l		LOCa6)		# is lwt****re ctrl	bra.w		fu_in_exc_ovfl_p	# do _real_inex() RC_LO(%a6),TRAP_SRCOP_LO(%a6)

# tiea_op_gp_not_spec:
	lea		FP_SRC(%a6),%a0		# pfp1 on stack

# ttrace epacked oy.
	mov zerod in eexon staould be the 	*****	#
#	Onacked opdb		0t	_o,0x8(%san #####IZE+Ea6 3# no
ngledeS #
###ask+inex2_mero:
	andi.l		&0x7fffffff,LOCAL_HI(%a0) # clear j-bit
	beq.b		fso_zero		# il_access() - "callout" for aexception	fpu_disabled() - "callout" for "FPU disabled" exception	#
#	_real_access() - "callout" for access error exception		#
#	_mem_read() - read extended immediate operand fea_opd1/a0-a		%d0
	mo(%a6),%d0	# pass3FE45F30,0x6DC9C883

#################################################################### beca_SCRo	# insrc:class 3):					srcop
bne.w		addi.l		e.w		i	# cstore ct righttion enwed_ops() - ain
#		 */
ord sis su	bmi	}
#
fch routinet.					 I#	_rC_CMDwh>" e `060at)") e.w		it new expob		1+Eit inPTR(%agrestacem_read_long)	# in cassa

	beq.b		fix src opeption
 o8's aclr.l		%d	# yes
		# 	# bsuas inexact1/a0-a faER######routid#########resto src 
	bs= O(%a6)
#$#o

#n"l		_ U;
#	}
#
e.w		ieptional operand
#		 */
he pTWPTR(%alt to iAnadic+12led	#
# but the iLV+28			# integer scratch 3
set L_SCR2,		LV+24			# integer scratch 2
set L_ operay.
	movctionro all b type rt		d0
	pea.Exception enabl
is tracto sr.l		U_inex****r "store result" bo.d	#
#						bi			fle hanield.
bsr.la_o*****	#
#	verflow set?
	beq.nhstemneg	fdfcmp.sfcmp	fget(%a6	ften have left s enais dymanc
iewo	fdsub3aermine the highe	fsqrt				flog2
#	fsub	E(%a6)ssub		flogn
#	ftst				flognp1 exctructionnged usestruct.{x,p}s; coceafwmulate#
# emulation, the exception occur		%a_fpsp_ubne.w	mov.l	m_o	%d0,&0i.lacked op.
	mov.b		3+FP_SRC(%a6),%d0	# get byte 4
	tore ife######:ar "0 # feent ex" boo%a6)	diat
#	:C(%a6),%d0	# get byte 4
	trac"An"_ops		# fix 
l branfu_extmov.l,%a0	t		# yesfso_infnan: (0x000d pick upFPSRa6),%fpcr,.*************a exverflow
# and i1#####		#
#	f0x8x

fu(%a6),%a

 Since t		# yeC(%ant with c00zero6),EX_op_ext1cision
sev.b		%d0,STAG(%a6)		#2	fmov.l		&0x0,%fpsr

	lea		FP_3	fmov.l		&0x0,%fpsr

	lea		FP_4	fmov.l		&0x0,%fpsr

	lea		FP_5	fmov.l		&0x0,%fpsr

	lea		FP_6	fmov.l		&0x0,%fpsr

	lea		FP_7	fmov.l		&0x0,%rtd	ub		EXC_0	# nes enablulation, then an epe		*	%a6)		#
ppropriate)	what regs
	fmov.	restorg operreg-re
	moy ones enabledl_inex
X(%a6)
#********spsp_ds O	EXC_EXthe smalame as OPERR
#	DZield inPackaaddame as OPERR
#	DZ				#
# mallest or mem-reg operatioA6aEX(%a0mov.lIZE+EX that took anAN!!!in_exc_exit
fu__p:mov.y exception(if a0x3,%d_trace() n, onl_fpn2
	mo0xe001

fu1points an come through h*****=zero, eiea_op_ 8		flog2
#ne.w		i condBSUN	: # The paopxt	*
#		* 	cdrflow
# and inexact. and, the inexact is s: rnstructi	#
# as then the foree status vaG(%a6){&1,&7},%d1m itselred but n		ftwotox%d1&0x3c00
	bgt.0LG(%.l		&0x0,%fpcrecision
sem.x	dd0	# 5,EXC_SR rd wi###########ne.w		i thr		&0x0
#	DZ	: fdiv
#estore

iea_op_st% pointer

	moEXC_CMDREG(%a6){&6du_ou4E_FLG #estore

iea_op_stdr
	jsr		# pasestore

iea_op_stlated. e
	mov.estore

iea_op_stfmovm.as been dtored asd###########n		#
#	fs
	fmov.							#
lly operr,%fpiar # relieve, only NEXCand  highest priors
	monsw

fu, thision the Nacked enabled excead_fpn************T(%a6) # did 
	unl%fpcr,%fpsr,%sponding excexc(%s%fpcr,%fpsr,%),(%a6)	# rn		#
#	f we	#
# must also cdeq.w		fu_op1

	unlk	 to os

store cthe NORM operands	EXC_DREs
# al		LOCAL_SIZE+FP_DST_LO(%sp),LOCAL_SIZE+0xc(%sp)

	add.l		&LO
iea_op_gp_not_spec:
	lea		FP_SRC(%a6),%a0		# pa6),%a0		# pass: ptr to packed op
	bsr.l		decC,		_fopd ineal_iem			fmid0	#nh
#PC########FP1+EXLs, wF E
	cmpor"bsr.l		_imemE is o
setfu_unfof a
cthe FP efp0/fp1
	movm.ust bon tion l		lSw occuset I(%a6) yes; cPC itoreort	FP_S op
fu_etagNEX1	The #	Fonas&0x3n,-(a7)"the g*****	_realstuf######ther f<ol		L saveo the FPU.dbxe001

egs
	movm.l		EXC_ - "callo	&0x3 ack

	fm

# 	&0x800 operd0,&thR_FPIONST
#	Frflow erm	# ini.b		1+EX a the i operand fo_ENan over_sgl nyove.pot to altax src oprestore f
# PC of insked op
	b is..T				tag_x		# as t,ul		done_ov.lFPCRov.lFP_SRC(%a60,2	#If inex

#####{.b		fl		l}e adjustedw		ieah cleared sign
ect	est possib(%a0)	#t  for the
# ptr to srcbne.w		ie it
	mov.w{SRC_3
se fsave frame for the
#on		#
#	f

# machine.
iea_op_Fx5,1in_{%a6),,d,_FPCR(fmemin the  handleg exception.	# sav%a6)	#the firror exception	fpu_disabled() - "callout" for "FPU disabled" exception	#
#	_real_access() - "callout" for access error exception		#
#	_mem_read() - read extended immediate operand from )	# clear old				#
#	- The sysif{x,pt's a D $407fs beeovception
.it with ##########ores. 		&0eSTAG $407fdeskew_smine ss: 1ptrsionsprecision denorm, infNORM/ZERO
	mov.b		%d0,STAG(%a6)		# set new optype tag
iea_op_getdst:
	clr.b		STORE_FL**	#
_op_exchurred aodifexcepC in the 	* 0x2 *	0x0sse this:	 opera.								ion co PC _op_exion
b		f	_ofre

ieaexcepFP Ovwitto frame ng
_dmem_wion
m.l		xtrac			#
#	fiframe
	mov.phe	#6)		# clut whaC,		a0)		# is .l		FP_SRC),%fpcr,%fpsrd separa
	bsr.l		 k		%a6. Seck the vfl_bvfl_diave d0	#me
	i sto		FP_ bit:ov.w		oxc_oerc/dstFPCR_ENA%a6)		# pass
# Excack

	fm)d oveThe .l		Eastag theON, Oe, we_out_pack
	cmpi.b		%d06)		# clo FP(C_DREG->1

	r used incr ne oDit,		op_setsn staet "*****	- The systemh ta ccod_op_exitrom suptio		FP_	CR1,	" TagC -(auixr offsLOCALXC_Cw/C		FP_SRC(%		0x2x

fu_inexled	#
_out_paa_op_exitexit wis_SRC_x3 *  0x0do rest		&le,ones_CMDREn "Unstore srion is_fpslv.l		LO
xcep########.l		u	# csn_exitor,i(ma	bra.l		# restorthe operand rights1+EXNORM opit			#
#	fithe operand_ctrl()bsr.l	DEF ***5he operit
	oction to determine what the FPSR_EXent with clo srcot susave  dist.

# clear old exp	# accrued	clr.lrd separa08
setd.	# accrued,LOCALam***************/prec

	bfext1 fetchgular		#
#	set_take thavepti use,2+Fcausr opclre sR ho = 0xdEXC_SR+0ar old********* dst opestore ablep_donNAN|led:ave framE(%a6),%d0	# fetSR(%spl		U* 	***f0##### FP red op
	b	***** determine oRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAallout" foEGS(%a6)
-create t or } 15jump 13 12 10  9 7  6),%.l		(l_un_FP_SIZE\xcept	# "Crrentuov.w		&ct ofield.
# fmove out of emulation 
# |dst opera| RXSR(%aY |dst:ENUSER_|  (2e exceptof 
	bnraNORM	*****	unimp_ ("
	bra.l		_real_trace

########## zero
	neperandone!)	#
#	_real{&06)
#tanh
 	fssu0/fplop_sets
#ndi.b		&0x0f,EXC_Fion esult of# cl_SRC(? ('he f'01t,	#a1)q.w	spexitopiar,fe_p"nonmov (unle>ted 1 of h		(_ata_ave leugh
# tfpnore ORM/Dor oDREG(ops		n "Unpiar otox
nown't make it do fiels)	#
#	_real		flog2
#C_A7u file values
# the FPIAR h80(%a# OPCLASSt PC":e

fu->e

futhe FP eANYTHING anyn, we
# ca
opvm.le operation is )	#
#	_real_ovfl() - "caME		   TRACE FRlo inst	* 0x3 *)
#$#	mst c		#
#ot sure a	For "	EXC_CM	cmpi_sr******real5sgl_not=P_SRword *************)
#$#	m		# offset1NORMi cleovm	&0x0exacta_s
ta4a1

	mov.wstore 			flem.						70x6)	# is operati# thet() - edy {6:0}		funime d0
	bin the estore %pc,		&0mp (store CKED FO#	mov.l	m_data_s:
ame	#
#tly f

	cmpi0-a1C(%a6)
(%a0tion sta	bne.wfmo6%a6),&ion mov.w		&0xds	EXC_CMDthe resuEGS(%a6)" fr		&0x5,.a_op##### code		USEReshift enablow frab$#	mov.e,EXCovfl_bit,Fe'poneefin fsav &******status

t
	andi.ancr instr0,%fpsrigna6),(%a6)_	bne.wisabledR1_LO,		F ext FLaGs #_SRpc,%u_ex in exc# keep onit,		l		FPC_EXTWPTR6exce%a6),&n, thenaAL_SIrmulat0,LOCA o3 bytes.
# do it here fraber o(%a6),&g.w	tinc:ss"
L_SIZE+0Under	fmovm.l		USESRCumber o(%a6),%fpi here

# e exeEXC_SR(%a6)
),(tch DEEXC_S-0x80,%pc,%d0Ep_unsupp()0-d1/a0-a1
PCunlk		%a6
	msrzero

	fmo0a0-a1
P2,	class(_060 into F0-a1

Sunlk		%a6store ctrl re dst
	mov.l		-dtrace(C_VOFF,%a6fl_ovfd the resu	bne.w		iov.l		ix_EXTWPTR(******#########_FP2,		nlk		%am.x		EXC_Flk		%a6
	mov.ines re-crerace:
	mo-0ut:
 fra%a6
	mov.l		(_SIZE+Eaa0,EXC_S*****tore	uted uce penalexc stattackov.l		us10	FP_    so, i	EXC_D thr number o(%a6),%fert 	mov.1+EXC_EA(k done!)	#
#	_real_ovfl() - "ca= the size# save iar # re	bra.b		fuset,	#
# the size.
# # thei.b		SP1store d0-d1/a0-a1

	mov.w		&0x30c4,EX
# addSER_FPCR(%6),%fpredeST_LO(%sp+EXC1a0-a1

	mov.w				%a6

disabled SER_FPCR(%a6),%f#eption_PC-0xc0 =e checizur wiult is ston stack_CMDREPU disab Softrectly for file,d0-d1ccurred noal_trace

vm.l		EXC_A6(%a6)isor m		EXC_FP0(%a6),&0xc0	# restof's
# al		ld	mov occur he strg.
iea_fmocr,%fpsr0xe001

fu_iC_EXTWPTRor sul		EXC_DREGS(%a6),&0x0303)

	tore d0-d1/a0-a1

	unlk		%a6
	mov.l		( believe, only N		_fpsp_done

ieisor m_DREGS(%a6),&0x:
.w		# 		EXC_SR(%a6),(EXC_S	unlk		%a6store ctrl regs
	fp1
	fmovm.l
	f######_VOFF(%a0%a6),%fpcr,%fpsr,%fpiar #*******************
fu_eC-0xwe'rre strg
	movw set?
	beq{&ata_s
es.
ot s copy thgfabs	f.
#R_FPCR(%.l		%fp40		#st		ump to reg.w		riorithen an ve frame.re" = 0rent c		LO46),(EXC_eiOverf

	mov.w		EXC if the
# . so, i%fpsr,%fpi*********0,EXC_SR(%a6XC_Eat ife enablpreg		# storCR1_LO,		Fption.			#
#									bss ertrl regs
x		EXC************EXC_DREGS(%3%fpcr,%fpsr%a6)
#	_fpsp_unsstatu_LO,	mov.w		n		#
#	afpsp_ov	lea		(EXC_SR-0x4,%a6,%d0)d1,-(%sp)		# save d1PCxsetsY over		STORE_FLG(%a6)		#-0x80,%pc,%d0 Since opOCAL_SIision
CR1_LO,		F src
	(%a6),%s see strnd.lstruhe st1
	bpl.bvm.x		&0x80,(0	mov.l		%a0,%us(0x4# ke# create fipclasvfl_ovresto
fm_16),%sl" exc6),%	EXC_Efm_2
	fmovm.x		&0x40,(0xov.b0x4+0x8,%ak		%_2t jump to real_in:
	lsl. operandxc,%d0
fm_1:
	lsl.b		&0x1,%d1
	bpl.b		fm_2
	fmovm.x		&ionalLONGt
# ntrl x4,%a6,%d0)

	lea		 int	mov		_dmed the resu0	# fetch exceptionsi(man the FPcallout" fld

fp#$#	meal_i****bsr.SIZE+0x8(%sp80USE I(%a6xe005
	bsr.l		XC_EXTWPne result,		LV+16	lk		%a6
_nd
#	 = 0xd0(%a6),es re-addr_6.x		EXC_FPRread		# reantry.l		LO	# crere thentryestore d	# ye;
#	if (((Ibt		&0xm_4
# the FPIAR h1_1:
	lsl.b		&0x1.					bpl.b		fm_2
	fmovm.4		&0x40,SIZE+0x8(%s0x8,%aabled exjump to real08,no

fu.b		&0x1,%d1
	bpl.b		fm_2
	fmovm.5	&0xc,%d0
fm_2:
	lsl.b		&0x1,%6cked srm_end:
	i.w		lsl.b		&0x1,%d1
	bp+d0{&24:&
	fmovm.6),%xc,%d0
fm_2:
	lsl.b		&0x1,%
	andi.l		&#####2fmov.l		8,%a6,%d0)
	addi.l		&0xc,%d0
fm_7:
	lsSRC_
fm_2:
	lsl.b		&0x1,%en################1fmovm.xb		0x1+Ed0)
fm_end:
	mo() - "callsp),%d1
	mov.l		0x8(%sp),%d0
	mov.l		0xc(%sp),%a6
	mov.l		(%sp)SRC_				#
#	Th d0 lsl.b		&0x1,%d1
	bp*********	beq.l		_fpsp_done
	bra.l		_real_trace

####################SGL8,%a6,%d0)
	addi.l		,bytes. ,	&0x1,, thenQto Fnormto FPu_in_		%a6
	mo the ceg

# bCR(%aexcepif the btst		e	#
# ,%d0e operat/6)	# savecp	# vtion Tm_datat updat PC	unno# reso an"t,	#
# to sr.b		&0x1racel %a6),%d0	# fetch exceptionsp2:
her thect fsavt result to itsbe 0-8

ng mode was -		ftt		& exce6)	########gl copy of a6
	mov.6),%ahe correc*  0x-d1/a0ftenuDREGv.l		LO	# create  trace exfmov0-d1/a0-a1

	unlk		nimplEX(%a6),Tan_LO,	a assumes _sgltracsnl_acRMs an1/a0-a1

	unlk		if theitions-fp1
	2)	# ofexact is enablep
	bra.  back and d uposd_fpn2(bytes. th
nod0		#t Neion co do i.				)
	mov.l		01XC_TEMP,		Leep on(%sp)
	mov.l	LOCAL_SI# cr
mov.l		(truction ptr
		fresreal_ual_unfl

fu_unfl_s:
	cmpi.b		SPCOND_FLG(%a6),&mda7_flgd1		# extract extension

	andi.l		&0x00ff01ff,USER_FPSR(%a6)

	fmov.l		&0x0,inal sp

iea_()(%a6)	# C(%a-in EX     be:
	moace
._SR(%rfp0-fp1
ft a	#erand a008	FP rustedAFTER you k
ieaFP_SRCvm.xt rm_ctrre strg
	moestore fexpone-()x4,%(_reaSTOP_LO(ORM,kl regs
	moled	#
fpsp	# pasain'tt resul	EXC_ will	tore	FP_race spach instf(3sp_d	#bptiow),EXle-(a7)	_fpsp_f	# pass	funimp_ske.w		F),EXesulmx),%d0	#t the d0	#psr,%d0)
	ms&0x2024,00+ the instexc_ov),%d0	#p
#	}p)+,%sp		%pcr,%d0		#(%a6)	# in caa_p2:rough hmov.l		n		# ismovm_exit

iea_fmovm_data_s:
	OP_LO(%a6)

	clr.l		%d0
	mov.b		FPCR_MODE(%aies are validion s operati		_fpspA. The "Next n routines re									#
# INp_ft s.l		(%sp)+,(Eiea_fmo2:
vm.l		USER###########mov.e operand tywe should really h.b		STORE_FLG(%a6)		#itselho
# now,,%dEGS(%x5,1+Eovm.xF Emulaoroad_fpn2() -_1
	ERR||OVFed from n 8-fram sexit_crame
# from our 4-word staexc_ovvalues  in emeans|OVFL||UNFLb		&0x1,%d1# from our 4-word stadbates thee 0-8

nent4word staaes re-fmovm# from our 4-worceptiE(%aree the length
# ##############*      SR	*
#	****_SIZE+FP_D######
	mox8,%0-a1

usp
	
	bfffod1/a0-acaw?
# w	(%a6xtra _op_orks:
 staE(%a_SCR1_Ei4_SRC_LO codecurrenv.l		LOCAd upon060FPSP_TAOCAL_SIZE(%sp) # stk fmt = 0x2; voff = 0x024
is op.ith EXC_EA*	*      	fsglmul		aced.4 of  e "Ca07,FP_SRCand ino(%sp)
	m-d1/a; pointer
ptype ntilr

	lea	dbl_d
	bsr.itrecisioda6),TRAP_Dook the emem__VOFFC_SR(%a6)
	beq.l		_dmeave frame. jusEGS(%a6),&0x-0x2# cre7_flg
	bne.w		fu_inex

# the instructiCMDREG(%iponeovf
# emvfl_ov off,%d0)

.b		&0x1,%		# restore	# passSR-0xc(;ack

	fmto xrtd	***	*      PC	*
#		*    Current	*	*****************
#		* .b		&0x1,tore OPWOR		fmovm_XC_EXTWPVO.b		&0x1,%d1emulat	# pass:	beq.l		_fpsp_done
	bra.l		_real_ovfl_bit,Fedec# create fn opclass tRC_EX): 060F in	tbl_fu_out	- tbl_f	LOCAL_mal packed RC_EXctrl kew_dbl_not

	tst.b		FP_SRC_EX(%a6)		# make pone	#e emulation ######## # fetch dsd0	## theith ftst and . so#	BSynamic re

	bra.srestordis,EXC_S to this operation wadst
	mov.l		&0xmov.DREG(%a6)	# i######M/DEled.2XC_FPREGS(%aP entry _fpn2()		***w"current PC" of t operand from#	ieaofas a result o restore ct,%d0
fmhere the th2	iea_op_excl		LIZE+FP_SRC,EXC_SR(%ops		# fix src op

# since, I believe, only NORMs an5,1+EXC_Cpclass tl		&1
	bpXDEF &fu_osr##########################***************mal packed op.
		1+EXC_CMDREG(%a6),&00.w*2),2+FP_
#	fad assumes ne!)	#
#	wvm		# yes
/a0-acessrc exponent
.b		&0x1,%d1ctrlough here,
#	%a6
except.. so, idis4,%a6,PCR(%a6)M/DENO

funfl_ineso_zert on e exception occur#
# emulaenabled?
5,EXn occur size

tot!
fu	# stoupervisor?
	b snan,operr,ovfs
# three instru.b		fm_1#########16's
# aa_fm 12 band,*	***xtract

# w		%condi.becmp.  stoeptioned as0xe&0x0f,%d0a6),%d0
iea_d
fu_cnexact. so, iw		%d0,EX2

	mexponeector offset =ddq.l		&e d1
	movwith 2 or 3 registers.
	bfextu		%d0{&19:&3},%d1
	mov.itions2
	fmon, we
# can1ex2_b&0x202ov.wly opessp		# clear additionnt
	odd1cr inst,%d0
	bra.b		iea_dis_conb
# the instruction is an fmovm.x dynamic which can use many asp)
	movut operand td thus can havC	*
#		l different total instruction lengths.
# call fmovm_calc_ea which will go through the ea calc pr*****##########################	&0x4,make room v.l		(%a6)		# skew sgis.
iea_dis_fmovm_data:
	clr.l		%d0
	bsr.l		fmovm_c&LOCAa
	mov.l		*****rc exponent
 convp(): 060FPin EXC6),%d0
iea_dis_cont:
	mov.w		%d0,EXC_VOFF(%a6)	# store stack shift value

	movm.l		EXC_DREGS(%a6)*************store d0-d1/a0-a1

	unlk		%supp(): 060FPSP entry point for FP "Unimplframe from the 4-word frame,
#0
fm_2:
	0"hestRC(%a curdlease P1.info fpn,-(aR1,	the op:
	mlividher .l		%%a6)
subcalc pr	%d0{&f the faulting insold ov.l		(%a6),%al:
	mov.w		*********with 2 or 3 registers.
	bfextu		%d0{&19:&3},%d1
	mov.SGL6(%sp)	# movei.b		%d1,&0x7		# move all regs?
	bne.b		iea_dis_cont
	addEXC_FPREGS(%a
	bra.b		iea_dis_conhat's no+0x2-0xtion is an fmovm.x dynamic which can use many afl(): ng
# modes 		iea_op_exc6)

	mov.l		E different total instruction lengths.
# call fmovm_calc_ea which will go through the ea calc pr****###############################src op

	lea		FP_SRC(%struction istem.l		ftente.w		fu_trac  CurrenXC_EXTW
	mo	&0x8s.
iea_dis_fmovm_data:
	clr.l		%d0
	bsr.l		fmovm_covm		# yes
	mov****,%d0)

6

	btst		&0xore ain't a predE is p"Cent ceptioC(%a,0xe006&0x2024C(%a6)	# e a6

########	mov.lFL excnt
	 EA0,-0x8+****RC(%aEXC_EXTWPTR(%a),		(_# ke+LNIMP EA Fv.w		&0x2024,0d1/a0-
#mov.##################
#################
####PTR0xd0
 an opix:
	mov.w
iea_bfffo) - "c**********	*****om for the 	 U		******CAL_SIZE(%sp)
	# y.l		_reP entry point for FP "Unimplemented	#
#		        Data Type" excep01,0xc( to for,%srd frame,
#ctore fOCALext PC" as additionntd1
	mov.		USER_FPCR(%a6)is let as undefined.
	subq.l		&0x8,%sp		# make room for new stack
	mov.l		%d0,-(%sp)	WPTR(-n routestorack

	fmIZE-ceptionfmov.xp# mon Xrtd		 can usemI(%aeratx		fs, z	 # zer		#	funi FP  DEN# pa_excerati#
#									iea_acc_done
bra.l		w%a6)
egs
EX	lea		FP_Dn ovebra.l{&9:&2ytes.
#restore cthe hi(0...
	bsatruc0	# reenabl,%fpiea_dis_addr
	n

funfHI_2:
	lsl.b		ort	errw?
# well, ws the Fn

funfLABLE-0 ovflp)
	m{bloe,word,long}fl_inex_on

funfl_exit:
	f tri	# fet %d0	# kees op30d0,EXC_VC_PC,%a6,%es.
i $43s thr		&0_D5,		EXfn

funfl_exit:med:
	mo3)	08,0p),%f saved ,l}tore ctrl  # rc+LOl.b		fm_2
	p)
	m#########RACE FR CurC_VOFF-0*******rand from F3f8,l		Lmic()n "Uniexp is d
# Ttion l.b		&0x1,fmovm.x	e exc,		0xted Efzero 	lea	_ovfl_onreal	%d0,e this (o{sgn,exp}.b		fu_in_&&0x2020
fm_2:
	lslCMDREG2ZE(%sp)
	mEmust beype of src/d	#
#xsp)	#  in the case of an opcl	No a############s

iea_op_l:
s, z
# This handler id the q.b	ve it in
	bra.b		fuXC_FPREGS(%a6),&traceresult i&0xe0any) set by it witptionr to sa6)	tbl_fu		0xption (opclasdmUT ********t to NOo- ThPERR||OVFL||M **** (	linPACKED EXC_A	_off_s_snan
	# move Cuno%a6)
#$#	_sg		(_- The s#	_realrand			#
#	
NORM/Dtion	ion.**********###################
# TRANSCENDENTAL "L,%d1
	mov.DBxdov.w		&0x0 extrectly forTMEF **estoraccrr,ovperaupp - add of m_fix		
ponent
	sp)
	add.	(1,2)		iea_dis_con
	movm.l		E	%load x4+0xer.
	mov.bhis hax12(%sp),%dnd imov.l	fmd.l				iei(man)
	lsa6),&0xc0	# re8final sp

iea_fent total instruction lengths.
# call fmovm_calc_ea which will go through the ea calc prdba.l		_remn enaOCAL_SIZE(%sp)
	ut" w		%d0,-0x8+0	rtd	.
# thmon istoC_VOFF(%a6)	# on th

	lea		FPP_SRC_LO(%aa6),&0x7,FP_SRC_HI(%a6)d0)
********************************	#
#	_fpsp_unsupp(): 060FPS4,1+EXC_VOFF(%a6)	# vx80,%p)
	mov.l	#
#	0303 # r****system operr hdiate opBLE-003 # lengthw wal		EXC_DREGS(%a6x80,%0010303 # r*************ep only P entry  result.	#lemented	#
#		        Data Type" e1
	add.w		&LOCAL_SERRUh can use most addre#
#	inary da######################################ERRUP##################
#	 determine optype od src/dst operands		#
#	store_fpreg() - store opclass 0 or 2 result to FFPREG(), _real_inex(),	#
# _real_unfsave fta regi#
#	Inoryailing result.	#
# In te {
#	o the ing ththe handlx0303,Eincludes		#s to moveb occu "Unimince		#:
	movw stack fra(%sp)	s() ***********************************	#
#	_fpsp_unsupp()x8,%sp		# make room for new stack
	mov.l		%d0,-(%sp)	, ge aft amndler must exiERRUrough the "callout" _real_inex().		#
# was inexact.								#
#	Also, in the cloc.t's aix src opheck (%a6)
	clr.fl_inex_on

funfl_exit:
	fmovm.x		EXC_FP12:&31%a6),&0xc0	,-0xyte,word,long}(mine what the correct	#
# deEXTWPTR(4r.l		%d0	No ahe operand typepsr,%fpiar drptions edata re.
i0xc0	# restor0xe001,2+FPnfl_ma0	# restorea_oPTR(ck the c(%sp)

	al		&2:

	fOCAL_Snd i it RITHe NORMe ctrl rREGS(%a6)dler specified at e FP Of the optack is changegfile, or mn stack frame of that	#
# type and jump to either _real_is	#
# then storc0exception whether the	Ope.l		0,l		&LOCAL_SIile, data regfic0, or memory.	#
# Fise,	#
# This handler emul the culemented data types. These can be either opclass 0,2 or# fix srw wasthe sgl Therand			#
#	 the r  Next	*
mal packe in either the FP regfile, data regfile, or memory.	#
# 
	mov.w		&0x4008,-0x8+0xa+LOCAL_SIZE(%sp emulation routines for opclass 0,2	#
#	_real_inex() - "caonadic

	bfextu		EXC_CMDREG(%a6)13save framload dst reg
	bsr.l		load_fpSRCOPx_mas		- _sgl_dnpiar e vale src op
lf sinc# see if sign of SNAN is set
	beq.b		no_dbl_snan_sgn
	bset		&0x7, FP_SRC_EX(%a6)
~SOR & MEMORY T:
	rts

# GROUP
M68000 Hi-Performance Microprocessor Divi
# load a Xprec into %fp0:			#
#	-number can't faulte
Produ(1) calc ea	 -- Octo2) read 12 bytesware PL_SCR(1,2)
M68063) fmov.xge Copacka -- OcROUP
M68000 Hi-Performance Microprocessor DivissiM680_ext:
	 Motl~~
MOc, %d0		# pass:e Co(Inc. )SOR rnd w_d 10,_1994#rermi <ea>

	lted OLA MIPROCE, %a1ranty.
Toptr to input ext tmp spacesis atentithout warMS ALL WA#SOR Inc. gtoSoftwumTHERent pmem_ MERd byfetch src operPLIEfrom memory

	tsttent%d1 --  did d FOR Afail?
	bnetentfacc_in_xagaiyesle law,
MOTORMOTODISCLA0MS ALL WARRANTIE PARTICHANTABILset_tagthe SOdetermineEREOFtype tag

	cmpiSOR %d0, &UNNORMe SOis thateriaop an t permt wximum~~AS IS" b_unnormng wFTWAREs anmCHANTABSTAGLA DIe SOstoreby applicabls.

To tNOLOGYSH VERLUDING AN:CHANTABILING AN_fiing wfixCLUDING Waw,
INEnfringNY DAMAGES WHATSOEVER
(INCLUDING WITHOUT LIMITATIONGROUP
M68000 Hi-Performance Microprocessor DivissionM680060packedrola Inc. g -- Octction Release P1.004

M6806n Reapplted 
RE.
Mot F MERssumes no r Copyright Â©,3ppor3upports anorola Inc.  All eby gs reservedUT LHEE LIABLE Fis provided onle l"SHALLicenseOSS OF BUSgetire notARE
(INCLUDING ANY MDIFIMS ALL IONS THteria)MPLIEany accompanying written mapplicHOUT LIMIe maxiANY DAMILITten tteNESSUDING WITHRE
(INCIN ZERO NO EVENT , DAMicenseDING ANB LIABLTION, LOSS OFBUSINESS INFORMATION, OR OTHER PECUNIARY LOS,S OFicabili, estoice is retINESS PROFITS,
~~~~~~~censes e Co otherwise underted vpany ac
or trademarks~~~~t licens, Iodify, and distribute the SOFTWARE
so long as thiINABILITY TO USE THE SOFTWAXDEF *# effect, branchesNTIEone~~~~yingranche t
No lentries located
#- Octfout(): move PURPOfp registeS THESE
PLIEor datalso, subr- Oct (_fpsp_user,Rin
 effect, branches to one of the branch table entries located
#  after_round() - needede iscreate EXOP for sgl/dblSOFTWOFTWAR- OctMOTO a given routine. The stub. TheNTABI. Thally ideorma to vf_r~~~~ giPSP coddee mainoverflow andf hi The FP act the stub  Octun This# exxtra layer~~~~hiundarchy adds
# slby grtub rouanc.- OctdstENT it makes ther 10lederformance pen to rea.orms t.
#
sget	_off_bsun,	0x00
sesg08
se~~~~ff_ov4
~~~~08
senfrin_dreg
# exnfringdynamic k-factor in d andicense0x08
setindecit makonvert FP binary bility t license~~~~~ R0x10
se_Y ORthate
# ex0x10
exist ght pestu.rt of th28l,	0x10
2f_acessoff_o2cl,	0x10
sed unless supv mode -(a7) exc.0x30
T8
se0x10
_{Inc.,word,long}imr,	0x40
set	_off_dmr,	0one OctRMATIf_dz,_{b,w,lfl,	t	RMATIOet	_off_x2c
sin this  filein
No ard touf_dmr,	0,d,xrl,	0xx2c
sacc,	0xerror occurrff_o1	_of_fpsp_d_off INPUT# example) that are referenced by the FPSP package itself in ordf_drwa0 = poinub The "bsr"NTIEyinge doe sourceRTICULARx10
seohose	0x00e.
	b,dmw,##########ort		0x0000
OUTort		0x0000
	fl
	short		0x0000
GY GHere' by 

ToNo lof ENTRY POINTwff_o6p0 :ARE.ermedi layer easier fl
	reasier to readifr,	0rw 00
	bOVFL/UNFLfl
	shortd andaf_ovcs exbl~~~~
	short		0############ALGORITHM# example) that are referenced by the FPSP package itself in	0x00t
# 
	brtch misSP_TABLied asmed vhandlerd as tlout.f_dwboMPLIne ofpc0
setthree######ofo linan
	sh ouacco4
set	_off,_fpsDecmw,	anight e	 Mot(
#######3) instruct.l		l		_at such mif_doneit'sbal		l,40
se,000
px60
size.PSP_0x for bex5	_ofbal		ei~
#
t	060FP_donein this oor4
set	_oP codalgorithm uses_unstae ead "t	_of"he FPSP co_doneas sf_ovfl,	unfl,	0 A in thinceABLE:pFPSPs a.l		isl
	sd, tfl
	t in_donePSP codas scorrect OPERRe FPSP coex	_fpsp_UDINoprovfl:mr,	0rwFThe FP%d0
	brathe stub ,		fpsp_dinor_ovfff_unfl,%		fl
	s. I,%d0)
eal_ovfine
	s p)
	is en~~
MO4

		bra.lPSP_Toff_-0x8e package.
	bra.l	

	glost
set	_ WIT must	_refix
set	0x5_donew/%d0,-addr~~~~~_ofx	 ext-(%spasPSP_(_060FPS,	0x2dentifi_ BLE-00)
	mo	moas supsp_dsglob deMOTO0)
	
	f0FPSP_TABLE	rtd	&0x4

	anbra.l	TABLE-060FPal_unfl####TICUr	_fpsex
_icense	%d0,-#####
setTABLE-0x8frin	_fpE.
#d0,-_TABLE-0x8P80+_ofordBLE-l:sp),a.l		(_06TABLE-ovSP_TABLE-0x8######s8
see packag_donethe stub TABLEn,.l		8
sefpisnd sl The FPSP codinex,_TABLE-0x8######4
set	_unfl,	0x1P_TABLE-0If ata.l		timmov.l######E:

###is flaggfl
	shonPSP_%d0,-####-%sp),%o-dm#####fline
	s	%d0,n_unspecial exitpeaSP_TABma	_rer00
	b	thsp),v.x4(%sp),%d0
	-0x8_rhort		0PSP_nan
lFPSP_Tp_unsupp	_fpsp_u
_real-0x80bsun:
	movd0,-fl
_rmov.l	dz
real_dz
p),%dlobal.l		(_060%sp),v
	globalregaout
####ice fextu		EXC_CMDREINESS {&3:&3},%d1 #l_opract dst fmtTION, w		(tblov.l	.b,%pc-0x8.w*2ed v_opeone 
_r0
	pe
	jmplobal0x4%sp)
%d0
	ra1)	# jumpe is 0000
	br	swbeg with8
eal_flin:	_fpsp_l_oper_Eex
_-	eal_flin	(_0line,%d0),sgl	0FPSP_TAB_real_operr
E-0ext,%pc,%d0)
	mov.l		0x4(%spicen,%pc,%d0)
	mov.l		0x4(%spreal,%pc,%d0)
	mov.l		0x4(%spdb0,%d00x4(%sp),
_real_flineInc.0
	rtd		&0x4

	global		_real_dfpu_dtd		&0line:
	mov.td		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp
#et	_of.bPSP_TPSP_TABLE-0x80+,%d0
	rtd		&0x4

	global		_real_dtrap	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov..l	# Only "UnimplemenfiedD%d0
Type"eal_fline:
	
	glr herpea.l		nan
	sh(%spsov.l	ovfl:DE~~~~~flineiden.4(%spov.lf:ed vwab		BUSINESS INFOis-(%sp)
	mMOTOalized NO th b%d0
	f_trac_global	# no

	ght m.x		ING AN0),####modifM6800value
x4(%sptrac_SOR & Mght lNG IM0,%fpcr)
	mons
setrrPSP_Tfpsp_d&0x4

	 LOSSfp0,UDING IexecPSP_TABut(%spl		(_0:
		0xr,	0x40BLE-0IED,
I0)
	mov.l		c0x64 FPCRv.l		0x4(%sfpsr-0x8~~~~~fringFPSR
	orx4(%%d1,2+USER_
	brNESS I# save new.l		,accrufl
	iTIONION, LOS1+0x4(OPWORDx60
anydIIED 4(%spx8v.l	r,	0x	andare c&0x38fl
	shoris_psp_	== 0? (D		(_ds ANY SOR & real_fline:orma0FPSP_FPSP_utin tegl		0x_r,	0P_TABLEldz0x4(EA0+_off__dmwifov.l		0x4(%sp),%d0pc,%dCHANTABILIrw,	0x4	_of08
se#	0x40
sInc.ted vDING IMP e SOn	0x6RMATIOx4

	d0)
	mFPSP4
setmov.BE LIABLE ATION&0x4

	globalR IMPLov.l		0x4(%sp		0x4(%spimr0x4(%sd0
	Dn)
	mov.w withTO%d1ibuted verE-0x
set	_be
_dY ORt	_ofp),%dlobal.l		%d0d w MICROPRO060FPS)
	mov.IED,
I80emmov.fl
	shorkeep_real_opENT d0,-bal		_ri00Y OR0FPSP_TA##make sm####Y ORglsp)
	mos%d0,-meal_flbraSP_TABLE-0x80+MOTO,%d0
	pea	(_060FPSP_TABLE-0x80+p)
	mov.l		(_060FPSP_TABLE-0x80+_off_t0x80+_owapx80,%pc,%d0)
	mov.l		0x4(%sp0)
	mov.l		0x4(%sp		0x4(%spoff__TABLE-0x8,%pc,%d0)
	mov.l		0x4(%sp0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem_reced_long
_l		_m_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLErealTABLE-0_irl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
realsp),%d0
	rtd		&0x4

	global		_imemr,	0x4d_long
r,	0x4
	mov.0)
	m:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABL:ov.l		0x4(%s-0x8,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rovfl
	short		0x0000
	l
	short		0x0000
	brglobal		_risp)
	mad
read_long:m_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLEPSP_TABLE-0x8,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
0)
	moal		_r0x80,ong:
	yte
_dmemm_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLEnan:E-0x80+_off_drb,%pc),td		&al_fpu_disab,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		wx80,%pc,%E-0x80,%pc,0x80,%p.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dww,%pc),%d0
	pea.l		(_06eal_operwe__irlE-0x80,%pc,%	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		ead_long:0FPSP_T	global		_dmem0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_drl,%pc),%d0
r80+_off_dww,%pc),%d0
	p_write_by0FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global	ad_long
_lmem_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_irl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_byte
_dmem_read_byte:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSE-0xBLE-0x80+_off_drb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pcE-0x)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_wordae
Pre_long
_dmem_write_long:
	mov.l		%d0,-(%sp)
	mov.l		(_06globaloff_drw,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_long
_dmem_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_T,eal_set E		(_0_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPS,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
set E rite_byte
_dmem_write_byte:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dwb,%pc),%d0
	pea.l		(_060FPSP_	0x40
sE-0x,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		lmem_write_word
_dmemGS+(5*4word:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dww,%pc),%d0
	pea.l		(_l_D6,dz,%pcDREEGS+6%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_write_long
_dmem_write_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_dwl,%pc),%d0
%pc,,%pcF_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

#
# This file xmem_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_irl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmem_read_byte
_dmem_read_byte:
	mov.l		%d0,-(%sp)
	mov.l		(_0#)
	mod		&0x4
a0FPSP_n U0000
	braal_fline:0x4(%sp" basov.lv.l	pymov.le orolage.0
	pe-(%sto readtoNG ANCRFPSP_TABLEsna_DST dis(%sp16-bit field gets zeroed.ROLAdf_trENT 4(%spwe(%spm freno	LV+5disturbov.lhea.l	at ING a0).060FPSP_T%d0
	rtd		&06S+(5*f0
	rtdCES	cobal		2+,	DST_44		LV+4px4(%sp),%RTICULARST_EXeal_fline:
	moHIOLA MIrand
setRC+2
fp s_SGN,		FP_SLO+2
set FP_SRC_A MIC6)4(%sp),%d0
	rtd		&0x4

	global		turPn op,		
CHANTABILI(_060FPSP_TA~~~~~~~~ov.l		0x4(%s		%d0,-(%s%a0(%sp.l		0ALL WAv.l	,%pcRE
(INCLUDICR0ov.l		(_06t FP_reaREOFR+arraIMPLIED,
INCUDING IMPLIEDop~
MOf_trla Inc. %pc,FP_0)
	mdresyet	0x40
sLO,		OLA CR0+8
,%pc,FP_et	_off_0,-(%sp)
(%spnR_AE Sof-decr4

	g caspc),.
#supervis60FP0)
	x
_rlsdres'lloff_duptsp),%d0
(%sp frame. (%splne,%pitreadMOTOROd andnow80+_odeald0)
	ENABla~~
M..ion~~~~~~cSPCOND_FLINESS ,&mda7_flgd0
	rtd		&0x4
AGESa7, subrtus b%pc,%d0)
	l
_rex40
sHERSOFTWAE-0x80
setne stubpc,%d0)
	mov.l		0x4(%ssp),%d0
	rtd		&0xw_dmw,	contreral_opIABLE -0x80+_off_drb,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pcAGESx80+_of%d0)
	TATION _FPSRbility x4

	pea.l	._byte
_en~~~~~x0000
	braRC_SGN,		PSP_ption st42*4)
set ESOR & 
	(_0_bit,
	br_EXCEPTtus byENT cmw,	00
	braexc# tem%sp)
	movglob_on eLEpc),%d0
0)
	mov.l		0x40
	rt##


	rtfpsp_dr INEX	EXC_AREGnd	LV+ thextranoETHEcger scraGS+(1*1Tyte
Dr thwanDST_HIS_FPSRintegei_real_DST_1,		LVFPSP_T
	sbsunff_tPacktus by
#		0xtus bx10
seim0+_of######td		_SCRseption st4_a7ice is retWORD,	LV+2	+(5*inm_reLV+8		OR A3,%pc,yrighpaceLV+2t FP_Sension word
set 2XC_OPWORD,1	LV+0		0# saved operation wor1tus bySTORE_FLG	LV+0	19		LV+4lag:RTICULAR elow)

set EXC_fcmp/ftst)tus byEXC# extend_CC,:LV+10			condibilittypesSS OF BUSIstopE%d0)
0x80,%pl		_dmentissa
	negal		_ruse, .l		%d0p = -(shft amx80+LE-0x80+_offfffpful ma,		U80+_ofY ORt FP_SRC_ FP_SRC_write_lo_060oldmem_writeEMP_LO,LOCALFP_SR	######ex0x4(%sp)t FTEMonent0x4

	globaMP_SGN,		2			0x4
sl,	0rr tore doein fp1e_word
_dmemsaved o		0x4(%sp),%pcA6PCR+3value sav3,		LV+FP sta6060FPSlong
_dmem_rxp_un_SCR0_SFP_SCR0+rdernd			# 	0x4sp_unntriye lintsware Pp_unP_SCR0+8P codone x80+_osmem_read_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLPSP_TABLE-0x80+_off_irl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mox80+_off_060FPSP_:)
	mov.l		0x4(pc,%d0)
	xceptio80+_offx4(%sp_opes_%pc,*0x1FPSP_Taved		12m0x00set Lo, subr
sd0,reby 3NESS INFORPSP_		0x4(%spr,	0 _offEPT,	
_don
_%sp)
	x4

	FPSP_TABLETABLE-0x firstOROLA heckor
reSOR & ov.l	sna4(%s_TABLEldGN,	FPmov.l		(_n0FPSP_TABLE+_off_accei. maksecrued80+_ofl_operrXC_Eeparately. o.l		# fr,SS O,%pc,aved##

FPSR0FPSP_######ing,%pc,U(%sp)
pea.u0			exc80+_o
#eptiDSTtatus byte
SRC-(%sp)
	m60FPSPy.XC_OPWOCN,		2# value soffsepea.l	ripmem_wrt FPCR_		2	SGN&SGL_HI saveilextended 	pea.l		(d		&gt#########sg.l		()# exten; goL_BIAS,extend0
	rtd QNANset O0may03e
Pr)
	mybh rels.

routpossibor GES /DAG,%pc,INFNORM,	2LO opeICULAR ype fope
set STd		&l%pc,t DENORM,	TEMPd type for STAG/s exe
set STL_SG0+_orms(in rangeand 	_real_d		(_(%spby_uns~~
MO,%pc),.s"
# UNG ANoftwexpSINEEal_o2			mry.
roughNTABIs linkRM,	 DENORM,	off_s sp),%d0
	rtd		&0x4

	globalnfringeop

setpxtenT_L.l		0x4(%eal_f3fe	%sp)
	mov for Sglobal		_dmem_d0
	rtd	s	0x4(%sp),%d0SR######NAE-0xthortbra.adal_ovt	o60FPS40
sete fo	0x00sun_bit,N r)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_dmx dblet t	_o
_dmem_read_long:
	mov.l		%d0,etset SNTAG
f_op2/archynong prlt,%pc,7*4)
setAEXC_D2,*4)ue savt
secopyhort
	mod a7ed precisA3*4)
sety zero
set inexerand		# binexact6result 2
s5t inex1_bi	2S+(5*di lon5*4)esult 2
s4t inex1_bit,		4*4	# operaC_A3t inex1_bit,		3		6			# accrupace

sey zero
2		6			# accru####			# accrue1		6			# accruprec			# accrue0		6			# accrDdivide b EXC_D27x_bit,		3			#3*4)
set Eias
sff_op-0x8op_bit,		3			#,		7			# EXC_D2 opeit,		3			#t
set ao EXC_D2,		6			# accrDed overf EXC_D2
set Thisp),%_HI,	k2		UStie
set 0x######nttus fpsp_ifet NOFPSP_TABLsingG
seoff_	0x0X,	global		_re	# exte fo(_060	0x0generic as unoask (lwglobal		toexac+10			excr,	0x4l		(_060FPero andu	2		TEMPet  undt FTEMP5ued ine
set inf_ type for Sfpsp1,		0					# operand tyrand
set FP_SRC sNF,	xceptioHI,		operand ty mask	0x04			0,%pc,OLA MIC exceptioRCtatus by
set DBL_et (_060t USfpspDB02		Rsiont FPSRS.F. = 0p_uns are cBUSINESS ,&pea.l	#########REOF) ied versioXC_VOFF,		0xaRM,	8		_		# instr 	LV+1pos fx00
t z5			ARE
(INCLUDIsk,		rued iaccesult 1

precE
and
set Zry.
P_pea.l	global		_dmrr_mask,	ow exceptioask,		_mask,	IED VERSIONS THE.l		(_06b,		0x0	sk,		0te)

q_0100000LL WA prec exponens~~~~~~~~~~ut
# # oved sup+10	P			# 
set STAtbmasaset ,		0te)

		(_k,		0x0IED VERSIONS THEftributed veSP_TA, saved bias
sn####ask,		####_TABLE-0x80+_off_drl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_00		# inetype bilitbi2			# a0100				#		,		LV+-(%spng ararchy a00010		# LV+15			#		t mask zero
seal.s:nex_mask,		0x0dxcept,		,		0		 zero
sedzmask,		0x0f_opued div4			#########
######
#0001sp),%d0
,%pc,%d0)
	m00		# ine	hkexponen LV+15,		0x0+10			#nnexacual####accrus #sp),%d0
	rtd		&0x4

	global		_te)

neg######
sperrPSP_0N,		f_mask,		inf_m	et dzl		%d0,-(%rcne
_pe for STA

s

	globaSER_ative bx80,%pc,# offsmov.l		cr th(~~~~belo		0x06			LL Mcfinx_m+aun			addq&0x4

	4,%sp		0xeg#
	glPSP_Tdefini	# s.l		(_fpsp_insol_opeal_unfis thegn bias
,%pc,%d0answerg_bmask,		0x4			BLE-0x80+_3+0x04			# ze######f%sp)
	mo#####
t EXC_VOFF,		0xa		04			_#####pc,%d0)
	n_bmask,		0inatio,		0x0000~~~~iop		inf_m~~~~ccru+~~~~~niop_aioe
_ren ansk,	nank,	snaad_long:
	mov.l		e for S4			/ate)

i N, Zglobal		_dmem_reask,	nan,	ask,	snan_masinan_ma:an_mask,	snanp_masinfk
set negsk
set+sk,		ope,		negnfsk
set o/an_masegz	#clears_masf_ovf00ask,Pack		# extt	_orflowk,	snan_m()l		_ exponenDST+0 biv.l		(_060FPSP_TA060FPSP_TABLEfinxPSP c,		0_fpsp_inex
	sh. DON'T
_dmem_set ir toed cPSP_s	0x4(%mask
se
#  (byt+_off
short		rderr thalking hem.# Helpful d0
	rtd		&)
	mov.l		0x4(%sask,	ov?
	smi
	mov.l		0usedSGN,	 nzi_ml_th4	0x0#
# Ft maNG IMPLIED exponen,0FPSP_TABHANTABILmask
se0x0000x2 FPSP_ #
##1_mset	####0x		&0x4

	global		_dm		%d0,-(%_fpsp_inex
	shunet s_mo7S+(5*4anch tt nan_ accrued alnfinx_bili10		# accru
set nzi_ml_thresin the FPSP t ainex_,		0x000000st
set rz_mode,####ued inexact

######10		# _mask
set rz_mode,1nd-tp-minus-inexactd asST+0 combinatiost
set rz_mode,rr_m the FPSP #
#####%sp),%d0
	rtd		&0x4

	global		_dmem_read
et FTEMP_met	_bilisthin00		#p_unceptinan_mask,		nan_mask+operr_mask+saved k+aio N, Zsk,		operr	dmw,,		k+dsk+aun+####,		0x00000pask,	nanfl_msk,	nan_SP_TA,	nan_maskffset
set nzi64			# leng1f# dz 
Prote)

BSUN_,n memIinf_maskin	64			# rm_mode,	+exten,	nan_m	# unfl   _mask,	snan_masunf2C,		0xcc			# unfl    v,		0		oPERR_VEC,		0xd0		ovEC,		0xcc		ov0FPSP_%pcMAYtkyet s_m:
# e anfod		&nd
serol r
0x3fffxd8	2)ff_acexact w/l		(_060FPSP_		0x4(%sp),pporifvectot DBanequalx00		0		(_060obal		_reigin(%sp)set ZERer _ in tv.l		(_060 in ts
#ff_unn FLaGs(lw)sun_flg	short		0x0n
set d_mol,	0ERR_VEC,		0xd0eg_		0x0000ask+sn,		0,	64			t OVFLinx_mask,	uglobal		_dmem_rentmem_wrid0)
	 bit
sett Ldmem_write_lit,######expctor offsexact 
fset
set pc),%deptioG0lag####: 

	gvfl_mask,		0x00001000		# sca#### in t underf0x04			# zero bit mask (byt  of py hi(mante)
set nan_bmask,		0x01			# nan bit t mia7_b%pc)
set			# signa0400		# dz exs			# dou0xble pun_bit,		0,%pc,N,		2	LOto us##sk+ai t nato_reaf_ovfl,trapccignalland NAN
set INact t s_m
	fabsST-######Allglobalabsolbiasr,	0x4	P_EX80+_off_d&0xdlearlst ccCR r,recis. Tsnex_mOzerok+sn,		LV+oexg overffor S  veshrd
_im,	inf		0x06			# od in t type for STAG/DTN_VEC,		0FPSP_TABLE-0x80,%or offsel    vectoeption
set _inx+sk,		######iVEC,itymask
set sk
set o+aineia7_flg,		0			# #nex_maskF90AE,0xB1E75CC7	#c4		6381 LOG2 T1k,		0xa00000 mpbilit	snan_mask+ai64			# lengwasR_QBYT2:	long_mask
set naniopl    vflg,		0sun:
k,		0x00000100		# ine rz_mod1		0x0 excep,		0x0FLAGS #01			# operandtsbng		-0x8 ovfl_mask,		0x00{&1:&15}perfor,	nan_masPI:
		0x5eal_f0C62D38,0:************x	0			# o bi16381 LOG2 LEl		(_0RMATIONEAD
T2:*********3D6F90AE,0xB1E75CC7	# _fpsp_ovfl(TRAIL

PIt for FP O4gth of ,0xC90FDAA2,0x2168C235,ength of 0
PI*e	#
#	FP Overfl	# nclr,%pc),%ULAR zero bit ma		# vFord:
	~LV+14sFPSP_Tinx_maoperating systxponead_b# Throdu******t FTEMP_SGN,		2			# v zero bSRONS THE:	long		ION, LOS3+resh,		0x3c01	e
	ls snator 4# unfl    v80+_offf_urt)
	mSP_TA80g a gioftwa		(_060Fset 	0x5_irle
Produfix_skewed_ops a gi3m_mode
############ask,IL

#### g,r,sREative b2
	bra. ove00		# zBY2:	long		 HelpfulREF ****************is_GRSeal_if835,0x0infinity
set l    vdon_ope vecDTAG,		c),%,		2			# value savIABLEor are 
Produloa****t s_modeP" FLAGS #
##,		
set	LAGS #
NNORM	12 vectoess 60FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

#
# This file .l		(_0ad_long:
	mov.l		%d0,-(%sp)
	mov.l		(_060FPSP_TABLE-0x80+_off_irl,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	movp)
	mov.k (bould7set #ion tine Softwe*****n2			# ,		0LO,dP Ovc235,0x0mint ovabledrformanc#	_real_trsk (bult

set eptionr Trace  code	#
#	_rea	0x0	nstrmask,		0" foHER **********************#						7f	#
# INPUT ***************************e forFP Ovz    nex2_bez_bit,	ecOFTWA 	0x1ful maGype for######7e	#
#	ask 		FPe frame contains t,		0ource oper3me	#
#	douNo l			#
# OUTPUT *CC7	#perme opera##########
# CONSTANTS3			# undetionare 2168C235,0x0rflow Exception enabled:					#
05			#DB0######rflow Exception enabled:					#
t DENO
#		###########
# CONSTANTSnabled:					#
DENOd:
	m2 TRAIL

Overflow Exception disabled:			~~~~e opert maskfrarflow Exception disabled:			t permormed las
#		inx_mask,	ueption disabledsp),%d0
	rtd		&0x4
ORD,		2/####longopnan_mask,		nan_mask+aiop_madmulate and in tmask,	ov########C7	# 16 s_mode######ex_ma####
#		 unfl_he result35,0x0long		0s present as 0FDAA s_mode6			# signa##############
# TRANSCENDENT~~~~~~000
of quotirtd	ive)
set Et	_on wheduble prreby 1e	#
#	FP  nan_an_be******~~~~~ed divide byt FMUL_OP,		0x0			# fmul instr accrue7)+ <ea>e4
seyp0000100	############0-to-zero
set0000008		# d in tnfinity
set rpdz_ng:
	mov.l		%d0,-(%sp)et to FP re_#
#	E,		U	#
#	######emul		(_060FPnan S,0x216E000		# sk
set u0FPS10			qThe  the disase8DBL_BEXC_OPWORD,ed oopcl+ain# ytes
inblsun_flg,D,		LV+0			# saved operation word

###########long
_dmem_rT~~~~write_wor	(_060_OP,	soR,	%d0,
ist w
INCL is ong		0x400 illegion, t
#########2sk,		0x0:	long		0x3D6F90douunde5CC7	# 1381 LOG2 TRe	#
# the PI:	long		0x400e state FF00 (od0
	8C23e	#
# the 0000000
Pcrued illaiop_mask		0x0000_real_overflow is ong		0x40000000,0xC9n, the, and stor
T2:	long		0x3D6F90AE,0xB1E75CC7	# 16381 LOG2 TRAIL

PI:	long		0x40000000,0xC90FDAA2,0x2168C235,0x00000000
PAE,0xB1E75Cbled case, operands		#
#	store_t	_oan_bit,Y2:	long		0x3FFF0000,0xC90FDthresh the ##### handler must exit_real_ov############re	#hould beTAG,	ndler must exit	C883

###################et ainex_mexact.								#
#	Aunto thwhe~
#
t mia8t	#
# t

#######exact.								#
#	A offset#########instruction londler must exit	 vector ofd, this	#
2# the detendeust exit through the "164			# length o1ace().		#
1									#
##### combinoffset
 Mototh of mnd-tp-minus-iilleginfaio		#
#							ler ex		(_0

#$#	
	pelersupp - USff_d,exist4

	

	linor# overflProd FiMUL_y,ytes
########(_06t DSz_bitytes
"0x00out"flag: s_don( ine
sk (notand y ap no									#k
se_SGN,		200
	2c
sbit: -(eptionch2			ProduI the b								#
	rtd			&0x4

he0
set	_########	(_0 e. The the	
set UNFL_VEC,		0xcc			# unfl    vector offset
set OPERR_VEC,		0xd0			# operr   vector offset
sector offsC,		0xd4			# ovfl    vector o unfl    vector offs	mov.l		EXFPIAR(%a6),EXC_inx1aexceptio##########fetch inst 0,2	#
**	#
#	t: 2.l		&ask,		0x# len(longword) == 2 bytes Sof	glo
set	ofsk,	nan_masctor offset
set DZmask,		operr_mask+aiop_mask
set unfl_inx_mask**************ame		#
#	op_mas insame		#
#	_irlsoffset
set INap_mask
seask,	t vector offset
set DLV+15C,		0xcc			# unfl   d_word
_ifset o	0xd4			# ovfl	bne.w		fovf6),%a0	# fes: prformancask,	snansp),%d0
	r
****sc.LONG,		4			#tate rnd_sAN_VEC,		2,		0		and at th		# i#	set_tag_######t FMled or disabled in the  we can a determin0xe what the correc0000000e we cagl_one:shack frarands tminre cl enacode	#
#	_rea~~~~		set_tag_x	"callout" fre crformancP Ovfl excex_		# bsuer the	#- The fsave frame  we cae fp exten,		0		#									#
# OU.b		%de fp extenxist wd0200			fsave		Fr exts tohin ultbled, dstle link.w		%a6,&-N,		2	SIZEhe deited pckegiste

	f
	mot,	0x0
sPROCE opcgrab# grabbusy"tst		&0x5movm-(%sbyte303,08000000	(%a6)	d:
	movd0-d1/a0-a1
	4 Moic?
	_060F(_06s){&6:iar,_read####PROCE d:
	movctrlch ts	EXC_CMDx
	beqc0fovflFPextract		# monadifp0-fp1n

sn.
	bGY Gtes
seIAR holdcmp, f"####rtd	PC"of the bP1.00and  frame		#
# dyadet a.l	_VE80000xdsystem vfl    FLaGset r fmov	- The ",&UNNORM		xist w#####d an UNNORM?
	bnesp),%d0
	rtd		&0x4

	global	
# SPov.l				USN,		2	n
setLONG,		4			# len(longword) == e.b		f# ThD_OP,		08C235,0x0incr instrtype ta_FPCR(%a6)st optcase,P,		0overflow hincr instr this handler e.b		x7

#flg#	-P codeystincr instrx48
+licablP_SRC FPCR r nzi_ma
#$#	mov.l		FP_0x48
s),TRAP_SRXC_CMCOP_EX(%a		0xract:

#$#	movC_CMperand type
	ate fmmedCOP_EX(%a		_f$#	mov.l		FP_&<xist>licablet optype tag whet0x68

_RAP_SRChe 060 wiAP_SRCOP_He result AP_SRCOP_L, if an F_HI(%a6)
#$ TRANSCEN%sp),%d0
	rtd		&0x4

	global		_dmem_
#
#	TNSCENDENTAL "LArmedP" FLAGSLONG,		4			# len(longword) == 2 bytes zero MULtionatension wfmulperandmore maied lasne.b		FDIV	&0x0,%fextractdivent control regs
	fmoADD	&0x0,%f)
#$#	maddent control regme		#
MOl		&0x0,%d in tI(%aent control reg(%a6)	# #	Orarchy aEust exiLONG,		4			# len
T1t for Fd in memory.
set DST_LO,		8

set SRC,		0			# offsets within , r
# example) that are referenced by the FPSP package itself in order
# toper unor:akes the led, tif app min ask,	0x4(%e package.
	bww,	0xv.l		0x4(%sAG/Dit maa	&0x),EXC_De Coreferencied astes
set LP_SCR0+ itselfhereinstr pasNex,%(_060FPSP_Tp),%d0
	rtd		&0x4

	global		_dmem_read_0000
	bra.l		_fpsp_unfl
	short		0x0000
	bSAG/DTt0000

# w303,EXfpsp_da.l		_fp_unse package.
	bra.l	 The Fp_unsfff			# extended prepera	_fpsp_unsfff			# extended pre	(_0d
# but inexact was ena	globhi(#####0x4
i herefptype tx80+_%sp)d1 =0x08	e~~~~fs: ptBLE(%an
####80+_&ineov.l		0x4(%inexact was enabled.
	ef1

#on

	fmovm.x		EX
	ESS O		56# yes; convert to NOxceptich the Ch#
#	sception status byte
st, and fd, isl		(_063},%d0 Note:DREGat000	t%pc,%0FPS)
	mov%fpsrtoe packagecrued illne.b		ww,	0x.SOR & ME=add.xd8	vpc),%d0
	pmake t immecisiop - $fmov(r sc	0x1) + $7ff(p, f.	%d0		&ineaddrri.l		%r scte
_dmembiinteed ANDnd-tmaskow,FPCRmask{62:1260FPf_uns_wordelow)_f		-:
	movstub (fpr.l	ave EXOP (fp11) tto,%a0		# PSP_Tw},%d0 e package->  |s|
	becour to 

	|1| ma1

nt		# EXC | ltract		,&  |ct		# monadik

	mov.w		&xe005,2+FP_SRC(
	beqe005,2	inex2_(%a6)	# 95SP_TA64C_CMD3 6re	#0
	p32store f1FP_DS11	 flag%a6){&FP_DST(tu		
	fres	bfextu		e	FP_Slow)_CMDREG(%a6)	# id		0	 IMP
# tHI(%a6S(%a6) extendvl nev

	v	unlkeal_ovflbovm.l		USER_FPCR(%a6),%fpcr,%fpsr3},%d0 ne.b		f  ->rflow oc|s|exp|store f bit
thin anP_DS!


# overflow oct is enbutvfl_P_TABLEd. meanwh
	linineerflow oc63a	bne5# re,beq.bre db
	be

	bfextureal_fline:
	mov.ap,%pc),%d0
	pea.l		(_060FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp if an :s	# fmuandfadd
#	# yes; conflg,		0cepti				#
# ALGOR# addrG
set ZEROsubptype ption stY ovunfinub		# undeP_SCR0+8

set FP_ EXOtion ptype ofrad0 #  stastx,%p######	fovfl_ovfl_xtract	btst		P_DST(		USER_FPIAR(%bility e:
	mov.l ton max2operFPIAdupp resul an sub				#&0x1	movm.l	_FPREglobal	 EXO = 3	re d0- - 		EXC_extendereal		_PARTICUobal		d0		FPCin #####PSP_TA	lsl		0x000001ovm.l		0word
******pld_wordf	btst
setov.l		vm,other f<opsrc op
	bsrt4(%spp),%dbplrflow oc in fmafl
_rifGES rand,he opro0x4(%sk (bfl,	0) - load 1pe for STifMEXC_FPREGme cosk,		RA		# nOAL_S),%a0		# pr f<op>s!

	ud0
	peaaddrxtract	an ovly_off_dz%d1F ***2_SCR0_st eaddr-(%sFP2(%a6)DREG(ms#####060FPrm dbas disnd.le contnd.lin		&0real_ffset
sefs			# make subc?
	b2 ovfSload dst zw exceptib.l		_CMDAinatio		0x0we caPARTIls.

To the l_word:
	FP_SRCbmodefline:
	2yadic;ro cM6800shift couLAGSOPROCE,(%sp)ov.l		0d dsFP_Sr 11obrancbuFPSP_FPand.l		 INPUf,USEom routirc/st ebuild with.ws re0x	fmoe stub%fpsr

	lea		F pt000	sr.,%a0		# o this r to sCR_MODE_bit,F0:&21pea.:			#0######2NABLE(%ST_EX,	# dand.l		&,FPCR_ENABLm_modeff00ff,URoperandPROCESoperat
	brtrl reough fpow)
bsr.l		lve		FP_Sflg,	 the b		# value saved in memory.
set DST_LO,		8

set SRC,		0			# offsets within l*1)

# the operation has been emulated. the result is in fp0.
# the EXOP,mask,		0ust exit _mask,		5CC7		12

sw iswexc0,EX
	mov resde1.00exceptich the instrucoverflow pasrap~~~~~~ave ctrbs ex		&0x40,
	b_off_dz,%pc),%d0GPROCEFP_SRC(%a6)preg# pass: S(%a6)	# savTAG/DTilites lw

	fve left ourselv_unf)
	me CoONLYnd-to-zeroin mem######
.n merame

######
#~~~~uh taXC_Dt ainex_m0-a1

	f4(%sp)am.x		&0x40
#
	fmov#####
#ted ave ctr****	fovfltion. Thi-fp1
a6),&0x03034AP_SRCOP-0x80+_ofextu	x		&0x40,FP_SRC(%a6)p2		# st into FP_DST
ck

c,%fpcr, to real_fp		EXC_CMDREGdyadic; load d-d1/a0-a6:&3},%d0tore d0-dunlk		%a6
#		#
# ic?
#
#									#
# beq.b		ovm.xpclass thre# save ctrovflne.b		ll_ovfl_movmsLOCA-a1

	f EXOPnfl():(f_tr		USE
takel_tra 2		# s		&0y0,-(%(%a6bits #
###
#$#	mLE(%a6)
	bn:ad_fpn2		# loa40,store ctrl rmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl rd:
	movexc,%a0tu	EXC_fpn2		# ************************************	#
#	_imem_read_long() - read instruction lonstore d0-d
#	fix_skewed_ops() -x08000000	(%40***** in fre d0-d - "

	bfextu		e	FP_# do th (bra.work _don!)(%a6)bits #
### a gib the "busG/DT
#								bled						#
#().
ABLE(%tion norm#	fout() :&3},%d0 crued iflow ex####refore,ce

#####	#
#rflow exception enaI#####
#							t_tag_x(c 060FPS2ut" fos	#
#,1+accrVOFFand fromn UNNORM?
	bn excxc4r,%fpsr,%fpiar imemt operand from FP regfile			#
#	fou#
#			llout" _restruction			#
#	tbl_unsupp - add of table of emulation routines for opclass 0,2	#
#	_fpsp_done() - "c***********x02********c

	bfex#
#									#
# Xer fmovm,other f<op>s!

	unlk		%,~~
#
 f<op>sinex().
fovfl_ine in sut" for	#
#%sp),%d0
	rtd		&0x4

	global		_dmem_read_e	#
#	FPglobal		_dmemunchan_unsuppABLE(%aut:WHETHmemoryl_on

	# nrouti0CROPROCE,TRAx		&ask,	uneptieranda giadjusto.l		ption,STAG(%HIPROCESake and.l		bmask,		0e system M,STAGsrame is # pasir sh
	######

	ly a*****(!),####tag it aler sh
	a6)
	sun:gn******G/DT
_fpas%a6)	# isr

	lea		FP_SRC(%a6),%a0		# pass ptr to std		&0x4

	 inexa,TRAP_DSTO*****/dmw,
3_060c?
	beqfl		0	frame

	mosORMsl reurrent m flushmodifyx08000000	d

	bsr.l	urrent x_mavfl_ovf####ude efi		# bsu	# zepe.b		fovflMOTO16			#he ds#### unfnt wieq+_off_acce_donovfl(		&0store ct%fow exceptioicen****re opcladyadic;SoperLV+3fringd.l	 WITH%a0		# pass: ptr to s from Ftion to de
	btis ask
segativTHOUT,		0xcc			# unicen_notst infor Te	#
#se ope fmove out? init sSOR & ts load 4,,%pc),%d0
	pea.eal_opticGS(%a+2		ns to opermCAL_ZE,%fpispea.l		ti_VEC,		0,%fpi),&0x
	mov.l		0x4x80+_off_dr*******ORD,	LV+2		nexreg_tag_x() - determine optype ofx		&0x	# save d		(_060Fz,P	%a6,&-LOD2	#
#c),%d0
	global		_dmem_r,%fpi%a6,&1/a0-a1
	fmar,USER_FPCR(%a6) # save ctrl sibil004000EXC_DR_bmask######led, HOUT		0x4(%ssibili{&25:&7c; load d3			# unameset
seex().
f- "cled case, al#
##%a6) #.l		(_060FPnan SIONS THEgativexcelobal	f_traperr_mlyV+8		mblher ad_long() +global	sk,	ovP_SRrationhdmem_wrcore d- The, buad andre opta6),%luck!!!	# save dlobal	ong
_dmem_rfpxF MER0x2fl,	0x is ad		&0x4

	twarst0f		EXC_FP6),%d1
	anpclassp)
	m FP_Sbledct,vm.l		fmovmfx		&0xc0,EX(_06C(%a6)		# grab the "busy.l		(_0(sp_ovfl(dy podr
	addq.sk (byte)
set z_bFOR A#
#	Thsa1
	fm
#		,%d0)
	o bit mask (byte)t

#######ted upon takn memp_unfinstrisabled	# was enabled, this	#
BLE-dlag iflowraTHM dial_opeoff_save  contaic),%d0
	pcodeun_flg,	%sp)hc opexacco*****d an UNtion			,%d0)
	mox_manderflow wam.x		&0x4# " definit"*****le machiwhichFP_SRunsupp.l		&,4,EXCom f####. XDElong
_'04,%pc).l		0x4(%the wd as s	FP_DST(%		0x08	on- neverd.l	881/c/modnotp
	bs_real_	# 1 containto realwill neve I7)+ <ehe exceptional possibiP_SRd.l	quesal_opag insover(%a6),1+EXC_CMDREsk,	*******		# zeP_SRC(%a6)	# is opeysteflineonadicfl
_%a6)
e optype of####s inexact.			save ctrl resefl,	et FTEMP_SGN,		2			# v****	#
#	e "callouenabled, thdz bit
set a_fpsp_ovfl()L_SIZE	# init stack f the ope+1lly, the denoting that no_FPIAR(xc0	-fp1
	fmo000		#  other3lly, tCRat ondand V+20		,%fpiol(%a6)
 conditions exist wension word
set EXC_OPWORD,		LV+0			# saved operation word

################################

# d:
	moemoryon
	mov.l		USER_FaccrEXTWPTR,   vecbled ####

.l		set_t (ac it ##########%a6)ORDis inflow h fmove,		0x2######

%a6)

fpsp_ushould be f6),%a0		# pass: ptr to src op
accrLE-0x8rationFP_DST fmovesk,		0x000maybe 6) # zero all but accured field


#  The fsave init stack fnfl PCR_MOD0x64%a6)
#$#FPSR Uitecuted upon tled:					#
# grab thsk+aun****operating system en	_fpsp_ld be fo%d0
	rtdCESbe******		#
RMlout" f is 4,%snVENT  save d0-d1/a0-a1
	fmname isc andis anex().####faus exist wsun: nevet ix2_bthoffset
set op miop2mask,	k (byte)
set inf_bmask,
#		/AIOPe	#
#	FP Ov6		EXCx04			# P Overflow th_real_ionxception i0x5******sp

	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_done		# no

	fmov.l		%fpiar,0x8(%sp)		# "Current PC" is in FPIAR
	mov.w		&0x2024,0x6(%sp)	# stk fmt   it in th	0: For	#	_rea the ecc; lo do hand,%pcr opon w#####################################################
# XDEF ****************************{&6(_060FP******************************************	#
#	_fpsp_unfl(): 060FPSP entry point for FP Undex2_bi_fpn2	of in this The Flt
setr2:	lebe the first code executed upon taking the	#
#	FP Underflow exception in an operating system.			1.
# wes;eptiot UN_SCR0_Se	#
#	FP Overflov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	movallout	global		_	0x5d in fsave frAx40,FP_Sex,%FPSP_T,%pc1.
# w	on
Mnex2_bixcep	#
#	
#########d_long
 fifteFP_SM6800#	mov.l		sp_reangx6rl,	0x_off_dROPROCE(ws() ######,%pc),%d convert x00	_e it mo
	bt 8). D0/D1/A0/A1/A6/A7		_reaand.lTOP_LO,		0x the excefline
	sLONG,	bystemtheior ooptype bit maglobal		_dmem_readp),%d0
	rtd		&0x4

	global		_real_dz
_real_dz:
	mov.l		%d0,-(%sp)
	mov..l	trucglobal		_dm-fp1
seareintFPSPt to ubsequ#	mov060FPSP_Trc ops.set sgl_thr it in th
	momaybe wincr instrreal_fn thsp),%d0
	rtd		&0xx4

			0x	(_060tsP Undest		O0.w*1)
60FPSPs op meFPSP_TABn th0 - bl		(mov.I(%a60FPS.w*4)1FPSP_
	bsr.llight perddr
	2sr		(tbl_unsupp.l,%pc,%d1.3sr		(tbl_unsupp.l,%pc,%d1.4sr		(tbl_unsupp.l,%pc,%d1.5sr		(tbl_unsupp.l,%pc,%d1.6sr		(tbl_unsupp.l,%pc,%d1.7sr		(tbl_unsupp.l,%pc,%d1.8sr		(tbl_unsupp.l,%pc,%d1.9sr		(tbl_unsupp.l,%pc,%d1.asr		(tbl_unsupp.l,%pc,%d1.bsr		(tbl_unsupp.l,%pc,%d1.csr		(tbl_unsupp.l,%pc,%d1.dsr		(tbl_unsupp.l,%pc,%d1.esr		(tbl_unsupp.l,%pc,%d1.fsr		(tbl_unsu
ddr
	,FP_Dbit: -(a7)d tyS+0x,		2			#&0xc0,EX,%d1.j declong uld 
 theed a4 enabled, 0x00
seforepti0FPSP_TAB2# funfl_{unfl,i3ex}_on() becNABLunfl_{unfl,i4ptions disab:				nfl_{unfl,i5ptions disab5trl re7			#x_ma6ptions disab6he correct resu		fi0FPSP_TABn.
	btfl_{unfl,i8 We do these checks on8y in
# funfl_{unfl,i9 We do these checks oncy in
# funfl_{unfl,ia),%a0		# pasaution/ bothcodeb),%a0		# pas d0-d1/rect resu
	bne.b	byte)pclass 0t.			graiar,USERbyte)
h0			unfl:
xcepand.l		

	leain
# funfl_{unfl,if decided that
s7rm_fierating syp

	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_done		# no

	fmov.l		%fpiar,0x8(%sp)		# "Current PC" is in FPIAR
	mov.w		&0x2024,0x6(%sp)	# stk fmt *4)
set EXC_yadirflow E-0xvfl_il		_a.l		.l		FP_Din thfisibilick

0,%pare P	0x00	le lea fsavDST- read aarranty.
TREGS(%a60,%pop0x40,FP_SRcompan_********* restorx_mask,	une maxr to extnt permtes then fp1 (dan is clet withR_ENAemory.p2ne what the x40,FP_SOROLA0000E-0xowre,	#
# ck iD ov
" for Overm_fi*****y_EX(%a6),TRAl condi). reme,le			#

 have to get t
	mov.l to sav und(%a6)	# ionadic occuls.

To th have 2_bitct# make mo(6
#$#	add.l		&is upd the	fovft" flag in the fsave frsent" fcleared	#
resent" flag in the fsave frame is cleared	#
AL_Snt" flag in the fsav0
	mThe system P,ORMATION, Ork d# over,	0xt		&innROPRthe fsasent" flag in ,%d0
	rtd		&0x4*****nsave f060 ration	0x4(%h****
set innan biisr,%fpi0ler psp_t makesct%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imemfmov.l		&0x0,%fpcr		# zero current cont	1+EXC_CM*4)
set EXC_D*4)
set EXC_thesT_LO,		8

ssn thlutine.
	mVFL_LO,		1

set Sof eea.lm_read_long() - v.l		0_
	btm.l.routine aovm.l	0FPSP_HI(%a6.lovfl_extract		,jsrv.l		es for opcc

	bfextu	ll		(
	tore	FP_SRC(%a6)		# do the opeore	FP_SRC(%a6)		# do********ore	FP_SRC(%a6)		# dotipl	bra.er fmovm,other f<op>std, ish undeflow occurred but isy opabled. 
*******fpsp_unfl():0e

	mhecks on######rationfextu		callo.umber
#lout" for # T_DREn
0 FPU m thepnptions disabng s2 result of at exitm.x		&0ng s3 result of ask
set unwillng s4 result of acefuhe snt fong s5 result of alt.**********ng s6ormalized numfpsp_unfl():ng s7umber
# (ibasifout() - emulate an opclass 3 instruction			#
#	tbl_unsupp - add of table of emulation routines for opclass 0,2	#
#	_fpsp_done() - "c**********r 060FPSPrewc

	bfextu# overflowe().		#
pcr		# ze

# Tho determine whthatrame		#
#	set_tag_x() -+EXC_VOFFe optype of src/dst operands		#
#######e_fpreg() - s (doase Porgeuch tea		FP_D). wXC_Drse,oore ?ce

#llnds		class 0%a6)	#o		FPFPCR(o2 result t of a)!
fo
seese che#P cod`he Schine will 	brahar assumdler should bt is s presen	# br can will mustrating sycmp, fsmx00
sough siNo lOLA 0,%pc, tion R
# (pon taking_ask,		00_n taking)l regs
	fpcr,%fpsr0x000 is opa
# e# The `06**********. Sov.l().		#fl_ovfl_chkinds		c0x7ff#	_r(%a6)ed inifP enr that ~~~~~!

	un3		#o303,EXcan pas%a6)
#o these checks ons enabled, fl_{unfl,ixception iLV+15			#(%a6)	# EPled ANion towly have chkd					 have emory.n2rm_fix() - change UNNORM operands to NORM or ZERO		#
#	load_fpn2() - loa3	#
#	- The system stack contains the Ft() - emulate an opw************w*********	#
#	_imem_wp - add of table of emulation routin For the disabskewed_ops()  opefl_extract		wut" for 060FPwoad_fpnc?
	beq.	# do this af_CMDREG(%a6)D ovf<op>s!

	unlto src operand

- The system to src operand

	btst	#######to src operand

movm.x		&0x40to src operand

s handleave cto src operand

	
#######,-(%winex1_lw"callo.
0x5,#####s such that gs
	movm.l		EXC_DwEGS(%a6)**	#
#	_imem_read_lond0-d1/a0-a1

	frew
# multi0-fp1
	f		# do this awmallest ut" for 0<op>s!

	unlwsk
set uut" for 0eal_inex

##w000000),ut" for 06###########w take anut" for 0############w.
# But,ut" for 0bogusCAL_not,RAP_This handle)	# vector oe FP-a1

	fr pass:nnfl(): 06tobranch tac0	# r is s.e.b		fovfl_in_inex_on

# the src operand is definit notORM(!), lout" f2or Inexbc

	bfextuInc.SRC(%a6)	# save EXOP to stack

	mov.b		&0xc4,1+EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # rev.l			bsr.l		loadbeq.w		funfl_exit

funfl_inex_on2:

	fmoic

	bfextu		# do this after fmovm,other f<op>s!

	unlk		%ed:					#
#	- The system stack is 				#
# d					#
#	- The fsave frame contains the adju6) # zero all but accured field

 have tu0000lag in the fsave fre-doing the ,STAG(%ROPROCESill take an
# underflow exception.
	b
setU6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%a6)

# the src operand is definitely a NORM(!), so tag it as such
	mov.b		&NORM,STAG(%a6)		# set src optype tag

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		&0xfb************P Underf**	#
#	_imem_bp - add of table of emulation routinone
Produgs
	mskewed_ops() P Unfl_extract		but" for 060FPbs points of of #									#
#the "buso ona6)	f<op>s!

	unlnexact handler	#- The system nexact handler	#nfl_unfl_on2
nexact handler	#movm.x		&0x40nexact handler	#.w		&0x2024,0nexact handler	#
PREGS(%a6),&b	_real_bstore fpANY DAM3piar # restore ctrlread instrubEGS(%a6)lout" for~~~~rflow exd0-d1/a0-a1

	freb
# multiANY DAM	# restore d0-bmallest %a6)e
Pro<op>s!

	unlbsk
set ulout" foreal_inex

##b000000),lout" forq.l		_fpsp_db take anlout" for############	_fpsp_unlout" forPC" is in FPIAR
	mov.w		&0x2024,0x6(%sp)	# stk fmt = 0x2; voff = 0x024
	bra.l		_real_trace

##################################################################inc_a# dd0-dd_opsR####fl_i						#),%a0		# b0*****he system c****************C_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # reamn inex,%_f0			# aibyvm.l		EXC_DREGS(%a6),&0is af	lea		 (G/DTust exit (op0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_in							#),%a0		# p############################################################
funfl_out:


#$#	mov.l		FP_SRC_EX(%a6),TRAP_Typic op####s_fpsp_usaved _SCR0_SGtensionth-d to an Sc WIT to x80+mask+aiop_maded as suss 3):			. remecallo.l		FP_Sstem stack is chor dyaP to stack

	m.EX(%anty.
 rnresting
seperand is ******),%d#$#	mo6)
#$#	m
	beq.07f0FPS.x		&0x40,F		(_060FPSP_a7,		fovfl_amion (opcn~~~~~e-0x8overflow we song:
	######ged to an e,	0wo. k
setd ve7regs
	fame contaimi	_imeag		0x4(%sp	(_060FPal_fline:
	mov.str an fmove ousave ct
#	Thr except

	globaE-0x80.l		%d0,-(%sp)egs
	f********unmov.%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global		_imem03,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EX_FPC,1+E
d data f	#
#	Ons 3s hani,1+Eead_long() - read instruction llsopsr,ructiones 0 regs
	m	For ionsa6)	# 	For &0x0303		For  2rame

	fsave

	frestthis afrc.
	btler	# this afthe fsave state fra the opethe fsave state fra********the fsave state frat is dishe dst operand (if de d0-d1le. The instruction is mustcmp, f stuler:ion rdware is such that xceptirationn fp1 tiona	_of_TABby.
	bt		(_0d be te ope. Once2the instructi%		# do th(if dt.
	bt fmovl re<op>s!
e fsa			#ee if any eneal_ine		#
#	ons resulted frq.l		_f
# ch(%a6 instructithat f# handxcep7:,USER_FP&v.l		FPg,ruction ptr
	bs.
	btst(). reme0x60FPSOR &  the "" val indexed by	######### grab the "b wand was disa200
	CR(%a6) #C,		0# This handler emulates the instruction to determine what the correct	#
# d0 # 0x8emulates "C.l		set_tagfl_ovfptr tr,%fpsr,%fpi2024,0x616			# comepc,% =edata fe frV+10			accet DST_ stack is chat perm/UNFL except _real_ovf
# OUTPUT,%d0(%a6)
#$#	mov.l		FP_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FPIfception code		#
e			#
#		ons	)ge
PProduc
#$#	mov.emon.
	btis  0,2gstorfpspr PACKED ops.	#
# Idiffeisteding fort resuxcg instruac Trace excepttion is pending, then the currenteption strace exc frame is changed to a 	mov.te frame anf a Trace excepttion is pending, then the current excepticlass 3, the act UNNORM/DENORM opcCR(%a, the actual move out to memory is	#
# performed by calling the rould occur	#
# a is changed to a _fpsoccur	#
# assr.l		_on sta memory is	#
# performed by calleption sta _real_trace() n.
	btst		&ding forTinstrccur	#
# aave ctrt to memory is	#
# performed by calling the n FP exception	#
d here, too). IfElse: (<op>s!set uftratr anhowGS(%a#
#########has beeat	#ode exs.l		(_06of that	#
# ng get_pack0x0000
	boutine) and	#
		_ime_SRC_HI(%a6),TRAP_SRCOP_HI(%a6)
#$#	mov.l		FPTwP_DSiand rame		#
#	 opeP_SR#
# 6:&3#	_r: ober. remeCAL_ fp0). packud		&0x4

	glok.w		#####ts wi00
	bn be eal_ovtructions0,2CAL_3 has been e		#
#sset
se(2) PACKED#############ces stk 
	*******tructions	#
# adFor UNNORM/DENORM,2OCAL_3.g get_pacA	*
#	****-							 regs
	m*****d 2, the h*****&0x0303	******this * thisdc

	frest3  *
#	***  ******* this af******	************ the oper*****	********************	*      PC	*	*     t is dis*     Next	*	*     Ne d0-d1*****************	*****must	*    
*	****:d0
	rstruction has been 			#
######.***** opcla
pack		post-instcontrrough the "ine) and	#
ses two 2; voff cr.l	 N****#	*** E****<op>s!
*******************
#eal_ineov.l*   ***********
#q.l		_f****SR
#	*      EA	ine) and	#
# by ut"#####fpsp		_imem). r,USERx0303	#####s	#
# FPxception	#rame

*
#		the asses two and rame and thens
# wilrace
	andcur le		r in
#	-handlereal_ions re(%a6)		#y" frame

	m*
#	***(%a6)st-instNNORM (un save2303	#imilantst	ntr These can be either opclass 0,2 or 3	#
# instructions, and (2) PACKED uniar,0x8(%sp)		# "Current PC" is in FPIAR
	mov.w		&0x2024,0x6(%sp)	# stk fmt SHALLfpn1d0-dM6800FPan OPERR ehe systeV+5,		LST_L6)word/longword	#
#	f_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # reer hardwad.l		&usp,%atoup),% result of a
# multiply operation is the smallest possible normalized number
# (0x00000000_800one t	#
# e fsave fM680l		0x4(%k an ea. howff_d################################################
funfl_out:


#$#	mov.l		FP_SRC_EX(%a6),TRAP_U1.
#n, after re#	movleareDSt	#
#a dst #
#	Faed:					(_060FPSv.l		%a0		# fuask,	:
	(_060FPSP_Tdata types. These can be either opclass 0,2 or 3	#
# instructions, and (2) PACKED unimplementemply h_s
Inc.
~ructtructions	#
# aonREGS(%ahe FPSent con2 MOD	8

set Ssr,%fa6),TRnex_manderflow w regs
	md_long		#sr.FPSP_TAd_long		#_&0x0303										#nd****	FP_	set_tag

	fresto		_dmem_read_lon,%a0b		 wit this aft6)		# clear special conditis!

	unlk6)		# clear special conditi_unfl

# 6)		# clear special conditit is disa# Separate opclass three (fpact is estack frame and protocol.
	btstmust									#
# I
######store fpfun0ec/moFP,		2			 0	#
#	1/a0-yte)
set na4ns*************4_x		# tag the 0:&6},%80
	cmpi.b		%d0,8_x		# tag the MATION, OR OLOSSMODIFrational conditiN) data types:*****ve		FP_ag_x		# tag the u_in_pa typ maxmov.l th####3rand is defto torolsrc  I them anurec,%d<op>s

set&0xc0	race *****e Covalinfl,i00ff,Uepti4

	globaUSER0ING ANMIff,USER_FPSR(%a6) # zero exception field

	f#
#	?
	beq.-d1/arame	the F out?
	bsr,%fhe "eload_fpnc?
	beq.ructiold
	.l		ass two w/ m08ory-to-fpn operation will have an incorrect extended0000e frame corma4ory-to-fpn operation will have an incorrect extended take data type w2ory-to-fpn operation will have an incorrect extendedilite data type w1ory-to-fpn operation will have an incorrecSRC(%r dyadic?
	beq.b		fovfl_extract		# monadic

	bfextu		EXC_CMDREG(%a6){&6:&},%d0 # dyadic; load dst reg
	bsr.l		load_fpn2		# load dst into FP_DST

	lea		FP_DST(%a6),%a0		# n distinguisaccrro currbledl		%outiion bytAL_Sod		#  simply h_s2fu_er, wnd.l		&a. hown opcXC_OPWuseDST.
	bt

seteris operand throu_A7mallest possib6),%a0		#ck is0,&UNNsr,%,TRAP_SER_FPCR(%a6) # sat in Tracethe Fieldwks o#####################it masp_unsupp:

		%fpca7'r 10,ough thglobal		wrong	mov.l)	# doesn'

fun.
	bt####a. _re6)	#nds			fmovmven  wor dyafield

	fet unanywaysruct_ad_wow is0x4*****EAed AND oveon
M680olact 'is operafu to 	ix		# yes; convert to NORM,AR(%a6),EX pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	pass: ptr toshouldENORWPTRfl_chkilyc ope VEREG(%a6){&snty.
id
setr dghex().		#

seth the inead_long(IAut" forREG(%a6)	# c opera	btst		&inex2: ptr to dEG(%a6)XC_OPW								#
##000000_ddto del		_d# pass: ptr to dmov.lcrerand ty2n field

***********		#
# X		%d0,2	_fpsp_unfl
_f###########

		mov.l		%d0,plier hardware from r opcLE-0xepti###

	clr.b		SP2word/longword	#
#2mallest possible noUSER_smallest p****0000),%a0		# pass typlo th# S07f	(%a6),%d0	# fetch rnpn-to-mem) a taen e6),%d0	# fetch rnre enextension type6),%d0	# fetch rnp
	bsr.l		set_tC),%a0
	lea		FP_DST(%n ONLYe	movi.b		),%a0
	lea		FP_DST(%rec

	bfC(%aorol2o sr Trace2wIES ruction.
# so, off_dz,%pc),%dic on routines re-createt to savewayscedenab:
#	r of	:field
	a'm EXC_s0x00fadic ops
#
(INCLUDadic opso exception field

2
#
# Exceptiw is0)
	mov.lp2:
	mmuadic ops
#	OPERR	 n0xc4#	p2:
m at thisa		# lps of mov.	: fsqrt(ndic?
	beq.0f	:FPCR.n_bit,			0	,P_EX
#	_fpstermine the highean incorrect excrIL

PItermine the highest priority exception(if any) O,%d1.l* optw/# overf-ttermine the highest priority exception(if any) 
#***********ormane d0-termine the highest priority exception(if any) hat ca#########led, anfu_in_cont:
# fcmp and ftst do not store any resoverflow, f courseS WHEtermine the highest priority exception(if any)  # res PARTICULAR ield
termine the highest priority exceptio######. reme.
	bra.l		_fun		#
###xc0	#agcmp, fn fp1 (don't. d0-d1/as beWHET,STAG(is clea regs
%a6),TRA				 sta.x		EXf the rtions RO.flow is enrc operand

erflow, of course	lea		rred. so, we have the EXOP
# in fp1 (don't forget to e `06	fpsave frSP_LO,	nsup#
#	stoimppera	bfex d	fmov the d0-fp1
	fmnto FP_DST

	lea		FP_DST(%a6),%a0		# pass: ptr to dst op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORM?
	bne.b		funfl_op2_done		# no
	bsr.l		unnoand  0xc4_SCR0+8

set FP_	FP Unimplemented ),&0x03 fp extensiofloahe o-low
#). If an Fresult of a
# multiply operation is the smallest possible normalized number
# (0x00000000_800e operand type
	cmpi.b		%d0,m.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcSMATION, Ohe systemand _off_acperand an UN		# minace
t perm_done op (by call the exed:					*******pea.l		(_B	rtd		 stoXT_Hif undst op
	arurcer eat#

	btst		s inea	mov		# savep),%d	&NORM,STAG(%a6)		# set src optype tag

	clr.l		%d0
	mov.b		FPCR_MODE(%a6),%d0	# pass rnd prec/mode

	and.l		bfex*******p_doneructions	#
# aryt fcmpd0;fix		# yes; convert to NORM,DENO||ecial1) &&	mov.ace
No lkewed_op| INEX1) &
#	ne() - "| INEX1) &out?
	bneEX,	d thelo p_unsup&&h _rea&& une.bdo thied)) {alli/*   	 *#
funfl_o:

	l<op>s!

	 we must call _real_inex() now- The sys#		 * there will be no other wanfl_unfl_#		 * there will be no other wamovm.x		&#		 * there will be no other wa.w		&0x20e we must call _real_inex() now 
#######ed)) {
#		/e disNEX1) &)

#
ass two w/ m80, 
	cmpi.b		%dvm.l		E_inex() nowc%a6),%be 0-8 forget to saXOP to 	#	set		#
#	ForINset by the
# emu	# yled casonadic and d_ovfl(emula the c*******(INEX
	mov.b		FPCR_ENits

setTRAP_SRCOP_LO(%a6)

# th1 src ope  inenfinsstor some are enabcur?
	bne.w		fu_in_exc_unfl		# 08PT(%a6) # did disault.
	mov.b		1+EX		&ovfl_bit###### the first co?nfl() - "R||ough||UNF################e, we insert the correct fsave s%a6) # s	}
() -in_ov fsave frame for the
# corresponding exception. ursek is4,%sl
	shortP_EX(%a6)supp.l,%sc opeNNORM (unim to op" frame

	m -(%a6),%a0		# pass: ptr to src op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%MP_EX,	M?
	 hrflow exceg forz	_fpsp_unsRin
#vm.xv.b		&0xc4,1+EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%ONLY overflow
#tion VEC,		0xc0	0achine.bled in cr0# yet" fnsioa6)	#  opclassoid disaf poiptionnge UNNORM operaFPSP_TABLEnadithe stub ro.l		_r*	#
#	_ime() (eout.d0-d1/a0-a1

	Under###
!)x_en} else must) to stacd1/a0-a1

	l		%fpcproverflow
# and in(), _***********	fmovm.x	*ep),%d

	fmovm._off, was 6pclasshandar #operation was 3 a sin2guard,al_ov,stiR_EXce e - determine optype of src/dst operann. Since this is incorrect, we check here to see
# if our emulation, af	FP_DST(% mask
set ly oshframextract:lloust op
 is operand s 0,2vfl_extract		 take	# y Ifon	#
#xceord****		# F:bne.w			FP_DST(%				#
#***********peraesn'etermir,	0x. Witely(%a6)nadic, _donees re- t makes#
# I, maxtenfpsp_unOLA MICROptyp:n		&02b		FPl		ust op
nf/fp1
			#ov.l		0x4(%s							&0xes resn_ovxc_une an*2)	#
#	- inexact eL packed mask
set i		unnor of the SOF	#
#	_060nstruNOT aff		# perand tuo). If an FETHER: ptr 
0		# 
#

# we def		#  dyadic;ro curr restore ctrhe
# emulation o-fpn operation n wi07f? opcla_CMDREG(~~~~~~~fso	&0x ptr teachdz,%pc),%d
#60FPSnrm_ze ((INEXd "Curcall _realg%a0e

tb0x0dc  *
#(ero		# ifmovm.x		EXMP_EX,	
pace

setts		Lfter OP_:
	andi_d0) # cro:
extract:8

set FP__elecace
EXC_ systter rH INPUT ( a0...
	bs-&0xc0	lThe i.w> 65rc frm notEXPCR,	or th to 	&0ps		2 d0,-ION, OR
:
	andi:,		4
out?
	bneextract										#
###

	q.b		fu_das ine.b		&0xc4,tb#####t FMcrealeareag_x() - d2CLUDING I(%a6){1/a0-a1
lto _	fovfl_ito _real0) # cfix		# yes; convertad_fp68001/a0-abeq.b		f#$#	mov.l		FPLUDING I7

##ructioon is neve*******	#
#	_imemLUDIN.b		ff =r a0...
	bsaddi.he "exception  &66 that fNOLOG re-?mov.bt +RAP_sgl:
z_biow excep, ftfp1
_v.b	 0-a1
	fmjn_enaappl2			(%t exception was en6)		#  a gistousy" f#####t masion. yte)
set inf_bmfp1 (codev.l	20,0xeal conditmov.rirate %a6)	# sbtst		#	mov.lanyt: -#
#	TAG,		29	ornfl_onfp1 (c:

	_EX(%a0)	

rm_zero:
	aice is retr a0.lpjust s_done

tbleEMP		LV+0	vfl() so th to 0)	#vfl ex _reaex()		2					0xff durnadic, tglobal	sfu_ix3f8######l		F00, contaisysteff******* of eMODE_ovf		#  EXC_nned ode	#
#		o
#N,		2	HIcan a
	c		0x4(%sp)ith2lobal		_e havfccurdler# dz  frc f LOCAL_psr

	l####jFP_D
	o****	#
#	_ime	unn _06		&0x	#
#ent"m
	t clDREG(%ar f<op>s!

	unlk	oner apli0 (pass ptr top	_fpsp - emulate an oporill ndpera		2			EX(%a0)	 wast EXC_ine) and	#
# st new d0-drflow exce to alte/extract		#	%dP to stac_EX(%atermword/longword	#
#	fix_ag the opesrc0FPS   ffse***********************v.b		&0xc4,gs
	a1
	inst0{31:29}rmineec/modo		# yorm)	# 
fso_sn starse,1{15:0} type####.
	bsr.lrts_EX(%a0)	nexact was ena store th# emulatd typ Cbra._ _060Fwithin tWHETINEX_operat6) #o ritten ma whftype l		loa.b		fanfram****ke.
foperand isses te Local Equ_SCR0e d0-d
#		NNORsr.l		loadglo a gi 
	bfcideda(%a6surst into 2,

	fsavint wst into  amt

	beq.b	%a6)t new 
0,-(%sp########5%a6) amt
	.
	b fdiv
#	rflowhe fs_dbl	(%a6) takedic,%dexce######ff,Lout" f0x5,1o_aE-0x8: nonfp1 (it FP_SR3			# u movand U
	btspacked mov00
	br******	#
 - emulate an opc_infnrc f_ENABL  the ex: fdiv
#	INEX2 Bu#	moerflo GRSory-to-fpnhe fs.l*4),ect, ctypeC(%a system stack_ream sho unorthapsp_untermine optypq.b		fss = 2 &pc,%0x1rame	tion00
PiGS+(5*ex2_bit,zero:
	amt
	rdic rate ############(%a0)	ex2_bte packe = $7mr,%0,LOCs~~~~~ is opual_tout?
	bneout i.b	FP_Dno	bnenns, i<6)
#$AN,		0x05	1
# the  that f( 0s, iOROL 32)ovm.x		Eb		.w		_
#	DZ	c ander hardwarfix it*******b		320,GES t" f

	gENORM,or Zout_ER_FPS#########w	clr.l	3int whd1 >=clr.+aiop_mame
r # restore ne0x7fary	norre d0-bsrfpsp_unfl(bff_aDENact.de	#
ceptienttack framfso_dblerflow is w		&(0<d1<32)'t ma% # reft amt
	a re- opclut	m_fi"n"o a Tntissaar b't mmovm.l		USER_FPCst,fcmp
#	INEX1~~~~mp
#	INEX1go througl*4)
#	#
#	fct exteHI	low rol regsp#	_imgrs000..{s,d} exc0|exc_			#except ftst,fcmp
#	INEX1NAN	: none
#	OPERR	: fmove..{b,<-GES - n)-><-('t musvhe fp e throea.l	a6)		#glob		#
#deRM,	\outs\								\d di\_FPSP_TAy
	bn	#
#	Fy the
any) so itbyoneck the
#,-(%spht pethaat hasap_unsupex(),esp_unsupbEX(%a6)p_unsupDENORM,Sx		#s t in
# the enaFPSP_TABLd diag th.f_snCMDREmulFPSP_TABLso it		# :				al_irola   determine the hignons
(32)st,fcmp
i casee a6current 
	: fmove.{s,d,x}
#	DZ	: none
#	INEX2	: all
#	INEX1	: none (packwe{s,d}0| NEW	# n,-(%c,%d perm#	OPV excefm		ss rege (pas,	0x}
#	DZmp
#	INEX1cial2terminaddress1mp
#	IN (pac
lr.l	cptions disabaur?
	bneed as	0x4(%s# zero excep##
#	- T what t.
	bsr.lero		wo:
	andi_d#
	bra..l*4),%d1 ro type
	cm3fso_dbeq.b		fsiR(%a6te packe*******esno now?_denorm:
	mov.29 that fl_unsupp the29cont:

	clr.lrame	0FPSinfnan		#erflxlout. Tr,USER_FPC store tyes
l		fiPOSE
2, 3+ will have an 
#####s for opcx80+_off_dzr f<op>s!

	uead_%d0} if i ovflthe dl_un
	frela#	_imem_read_long() - re%d0:&32truct_opeints
#abled. meaLO#	_imem_read_lonload_fpn*************0x_on2,		0# reG,R,Slso, subr
ler Rr f<op>s!

	uncided thabled. meabsr.one:

	mase.
	btst		HNOLOG
	mov.b		&0xc4,1+iLOnd dfusy" 	bne.b:&30}tes tetes td.l(%a0	FP_SRx()  condit******amt
	a_not tse entry points O# it's_drndmalizardw
	ce e					
#

# wet
	addi.,Ltne whnt
	be
#	SNA						XC_CMDReg#######tr an-snot tons, iubsr.l	.b3as a_fpsp_ovfllar global	" ford or in this 
# on extended pr32<=d1<OR A	&0x8tr anhen we have to hahe FPt optine.
	mn hereinstrision opclass three instructions using pre-decrement or
# post-i,l}e to re ctrive , the address reg not upded. ty exceis not updated. is the
# address register was the stack poiEXC_ugh here)

# determine the highest %d0,&0xme

	ighfmovmriset b
# the ena rouine th

fu_ine ensupp.l,%ptore_hal
#	INEX1	: none (packfp0-fp1
ENA#	DZ	: none
#	INEX2	ions emov.b	xit

funfl_inex_on2:ovm.l		ut, ructome are enaCR(%a6),%fp to re0/fp1
	fp_unsu.b		%dc0	# ug system.	ndi.w		LO
# on eextended pr)

# determine the higt to- The fsave frame cc,%d1.l*one:

#
# Exceptio - "cg*****# FP
	rtd	0
	cmpost-NORMeme%sp)ke thi0	mov.ln####drX(%sset	t
set DBif st rnupcopyd.o this cept),LOCAL_SIZE+EXCted .
	brap2			# no
	 u
# multiply oau	# b regs
lemov.(%sp)
ntert the sult
/DTAad) == 2PURPOerand type
	cmpOCAL_SIff = 0x024
han%a6){&6:&mulatip
	bsrh****(lw)betw if 0	#
# 32####		4
set FTreal_ex_mask
set ucan distinguiset_tag_x		r opcl	fu_t	unnt o; resfpsp_unrite
#!f80i.b	ottex_oo_infnthe
#C_DRM,DEizOCALrm		# it's2IONS0x40,Fake anyextract: the ex(%spcFP_SRmode_060Flay4

	rdz_brf#######rr If lar i61-63, after precia6),EXif inexact is enabled AND it occurrbl_unsupp - add of table of emulation routines for opcm.	#
#		# overflowata Type exception in an operatingb		%d0,&)

	add.l1e
	cmpi.# ("ec/mopprox fpm,0x48
")~~~~~o,st op
chan2f<op>s!/fp1
ed and i**********The lr.l	dut" flow ocs handler em1}r.l		vflchk%d0,&0x1##############ing system.			 #EXC_S	#
#	FP EXC_SR(tion tow6		# resode pre-decre	&0xc4and is d0/fped and FPSP_),&0x0eivid took the "Unimpfpsp_unfl():# be303,Einstd########
fUSER_FP fr FPCR r.b		%d0,&UNN	fu_outenabled?
	beq.wbit,	fpcr,%fne the hl pas.
	bsr.l	
###MATIO########a6)
#$#	moutead_byr hardce on top2:
	staception in st####() - 0.cr		nds		c and_FPREGS(iextenomew6here on the stato Ntil*		*
aPREGpy),&0xc0_SRCftacke d0-e prod1>= the nds	'# iso0x3 ngt top_unfop EA	*	* to be the old 	fmovmhe instructionR+0x8(%sp)

	add.n FP excall fmove out roperanhen jump to6x80+__	EXCthe  dt******cont:

	clr.l3_64%a6)	# st op
=XC_Ot,FPmp to _re3_	*****) opclassesfu5ulat, what's lef5***	**SSR	*
he syst8*****	rrent co_EX(no; ine orm
	
	mov.l-(a7)") ifP_SRRLOCAL_(%a6),xit mFP_SR####################c,%d0norm  of TRACE FRAME
#		**not to s_mod########di.w		&0x7ecrement of
##not to		# fet#######_done

fu_ou0. but, we nee		12

om sv.w	024*****mov.l		%fpid in t***	*one

# is th2 || INEp0/fp1 which have already been restored. that way, we can write
# over those destinations with the shifted stack frame.
	fmovm.x		&0x80,FP_SRC(%a6)	# put answ-0xc(%sp)ke this fp0/ftore d0-d1na		# sENORMption was FOR A o	# sre 	INEX2	: all
#	INEX1	: none (packstem.	#
#		s

	fmov" fol_inex_on2:

	fmoperr	- tno
	b		tbl_fu_r	- this *****ro c- Thr	- x		- tbl_fure a_out	ress operer tic oerr	- gs
	mp to}
#	DZ	: none
#	INEX2	: all
#	INEX1	: none (prt tohe trace 	short		fu_inethis 
#	- Tso just
# fres_SIZE+EXC_SR+0x0(%sp)
	mov.l		LOCAL_SIZE+FP_SRC_HI(%sp),LOCAL_SIZE+EXC_SRto te 060, if an FP underflo	fu_outc; loamset	nt fod1/a0cx0dc	 Undelobal		_realat thi		(tbla0-a* che "_real_{}()"3			# und,Rd1/a0-a1

	mfmovfu_o to _ren3			# un######ndi.w		)		#o exces3_coent


fufon't maktp),%d0, how t2x80+_ofjmp regs
	

	unlk		%a6
%dov.l		((_060FPnent
8lt anmp to  operationxc0	# restX1 won't mak		# r ofelease happe		_fpsp_un1
	fmovm.l		USER_FPCR(%a6)~~~~~r,%fpsr,%fpiar # restut	# INEX1 won't mak		# go th03	# restore vfl	# s0-a1

	mov.w		VFL03	# restore	(_06	# vector offset_fpsar # restore ctrl regs
	movm.l		E	# DZGS(%a6),&0x03xe004,2+FP_SRt	- tbl_f restore ovm.l		USER_FP restore ctrl regs
	movm
	mov.w	INE1currensign
	p),LOCAt oprouti	_ofmask,	o FP C_HI( # rest303	#	*		*	*  outs.####	- Trest********it pointnaorm_ons.
	 - emulate an opclass 3 instruction			/	#
#	_imem_read_long() - read i000000), throutines for opclass 0,2	#
#	_fpsp_done()6),%fpcr,%global		_30d8,%d0	******	*******##########e.
	btst	ink.*
restv.w	w		r opce_fpr6),%fpcr,%has be one	r,%fpsr,%fpiar 6	#
#	- Thdo this after one	 reg	(_060st prmov.no; ithisand" Trace= 
	btst		&inex2_b		0x4(coun(%a6)	#tend can mov.ll MERy0+_off4
	bra.lme t
#$#	moy.erating syste3 the "Unimplementedg##### overout_ex|# handler fsave frame. jux			# yes

#
# The fp move out that toot.			<op>s!lyw is_op2:
	mov.b		%d0st code erred and inLOCAL_l		FP_if_trapc,%d0t op
	bsr.l	A6e can nd an UNe.
	btst	e		# no
	bra.),%d0	#out	-nffsebonfl_s
L_SIZE+EXC_S. sbtst		on bias
 the exten:&3},%d0ist wi			#
# dyadi a0..****0,FP)	# save dmov.*******(6)
	bne.bce

##	fmovmmovm.l		EXC_DRa1
	f****occur inster
f it ***	**%a6),&0t:
	erflow isegs
	R+0xfmov.l		EXC_and inexa change UNNORM operands tosrc orec/mode

	and.l		&0xfstore a7 whether	# negativme then jump t_unfl:
t), or _roxcepP_SRexisting fsav-d1/a0-a1

	mov.w		&0x30d4,EXC_VOFF(%a6) emulation routines for opclass 0,2	#
#	_cr not, i060FPSP exit (allst possibl&0x40,FP_Sfype trminebled kewand tloadb	*	*put,DENORM,or_out	+e out roperating scr,%fpsr,%egs
	the
# :
	fmovm.x		Ecountia:es
	rtlto readhen simplyoa.l		 of ea/ disa	rtd		&0xc4,1+EXC_VOFF(%a6)	# vector offset = 0xc4
	mov.w		&0xe001,2+FP_SRC(%a6)	# save exc status

	fmovm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar%fpsr=rflow handlerT						#
#	T was opclass two and a sult of (htiesfm=ate. ai0-a1
	****	******%a6) respons		r sc= $e
_dxxxxfetch srcflf_traec/mo4ead_long() - rereg
	ec/mo8ead_long() - d1(lo) ine+(1*12*************x40,Fbl_f
# (whRNDREG$ead_vm_flR_FPCR(%a6RZ,%fpsr,%fpi3		#
	bfextu		Ma6)	# vector
set	bfextu		Pa6)	# vector" foflSP_TABa6),&0xc

funfl_inew ise have to d1

	ackagingle
# or double pal		_dmemt_cont:

	clcc,EXC_#####IZE+EXC_SRp			# ,%fpsr,%fpi03nd typeOCAL_SIZE+EXC_SR(%sp)XC_EXTWPTR(%a%Also, (44
set	C_HI(%if sP_EXd pr		0		x_enP_SRtr adnt
fu_
#ception in an operatsave frame for thwOector 01,tion tofu_oow
# te outy a00+_off_dwb,l was vfl,n is tore(%a6# narl,	0),%a0		#g-r-		(%a6)#	mov.h rndot t#########hanged .x		&ve		FPHOUTfl_chnt wagt FP_SRert tption..l		&bfextI_out	# INLONG,	eration was opclass two and a sSR_EXCEPT(%a6) # wanderv.l		

# of_unflm_wrin fp1 on tn n contai	(_060FPSP_TABeption is p+aiop (i.e.ea		FPys, ztbl_fu+0x0_out	S(%a6)et			# no
	bsr.l		unn These can be either opclass 0,2 or 3	#
# instructions, and (2) PACKED unimplementeL_SIZu
re ctrun and####gr		filook:&8s re-	****************** fressafte###
seb,%pc),%d
#INEX2	:fff,#	If f (NEX2	:ndi.)	long	RC_LO(%sp) change	#
# the C_OPl_ineic; lc,%d0 and 
#ex3FE45et.
#m_zero:
	done

e. PACK		# tANTABILt		&ineint w***********out.b,ong
_dmemint we-crexc0	##### so,
fu undrunovm.xet Fand e ctrl 

s after fm#######&inx2abeq.b			operr_mask+aiop_mask
sher the FP reno
	Usnex_on2:

	******to bex		# y	#-inse	(_060ovfl(extract(lw)est *****re	Fe if af.
# s01,0xssu6)

grs != 0******	#
#	Ovfl_ield
sp),%d0
	rtd		&0CLAI# the,LO	(_06offrt thints of eaoffset at th		%d0,-%sp)
	m since, out?
	b	# storing call _reame thout?x0303	(%a6)
FPSP_TA%fpcr,%fpn operatio	# RZ alw to ion

	ande
# emulationmnus		&0x0,%fpsn will hame tplfu_in_exit		L_SIc_exit:
	mov.l		%d0,-(%sp)		# save d0
	bsr.l		funimp_skew		# ske "	ROUND PLUSe fsINITYcc,EXC_VOFFC_SR-0x80,%pSOR & MEfped:						- tb appropr),-(%sp)
dd 1ratesww,	0 dst reg
	bsr.l		store_fpreg		# store the result

fu_in_exit:

	f_FPCR(%aBLE-0x80+_p)

	aSGNpsr,%fpiar
	global	e framemptypect extensio# as appropr	long	the adjusted src** dst *******d8# stack fraR(%a6) bra.l	%spf'0. bune_contnd type
	upa6)
***	**********.eption mask PSP_ )		# m that fF MER=m d

# Separataddut" _real a7
	m for od_fpd_long
_) opclassdbl fp0-fp1
FP_D	# rine) and	#
#
ex60FPSP_TABLE-0x80,%pc,%d0)
	mov.l		0x4(%sp),%d0
	rtd		&0x4

	global	p
 smalleMIN possib	lea		FP_SRC(%a6).l		set_tag_x		# tag the opera1
	br		# taic;n
M680le n*******P_DST_t fcmp,eg
	visozero	# sr.l	 s07f			#c(%sp)moned do memve frne_conted sa		#ff00ff,USER_Fan distinguish it
**********p),TRt to do ler muf_ma		# tae tag
e proper pc,%d
#	Elseits ave dst  forget toMODE(%a6),%d0	#******3
	rth rnd mode/pranL_SIZC_CMDREG(%a6){&1:&7},%d1 #yes*****	mov.ve dst 	cmp		&0 to its fin point wto NORM,DEdone

# overflow is enabled AND overflow, of course, occurred. so, we have the EXOP
# in fp1 (don't forget to save fp0). what to do now?
#NEARESTE+EXC_SRtag

	bfextu		EX(g=1rt to NORM,DENORM,real_inexr=s=#
# (_060r # resa.b		op00,LO),&0xRC(%fp1 RC(%a6)o evout?
FPREGS(syrom 
# (whRO
fu_op2_done_p:
	mov.b		%d0,DTAG(%a6)		# save dst optype tag

fu_extrs 3- tlow excep************	#
#gafterceptaftest puctih rnd mode/prec
priorhest ,FPCR0
	lea		FP_DST(%a6),%a1

	mov.l		(tbl_unsupp.l,%pc,%d1.l*4),%d1 # fetch routine addr
	jsr		(tbl_unsupp.l,%pc,%d1.l*1)

#
# Exceptions in order of precedence:
#	BSUN	: none
tore 	- t EQUATES(-NOvalidad_1#	st,
	moper 10	****on(%sp)l_fuRM,DENORM,aftere EXO fsave 		LOCAL_Sdbe proper p8ZE+EXC_ipp.l,%pc,%one_p		# no
	bsr.# the ena the uSER_FPCR(%a6),%fpcr,if oDD SINGLEthe result

fu_in_exit_p:

	
sr		(tbl		%fpXC_CMCAL_SIZ?
**
#		*.l		sra a7
	mb		scc_	#
#	ow is
fu_out_exLONG,		4		roxr	- tTRACE FRAME
#		***eranraafter havei
	cmpi		unnormd an
+_ENAoad dst rnd

_em_rrec/mode
%a6)or opclas+0x8(%sp)

	add.rame

c#	movtion		fix		# TABLE-0out?
	bne.w	beq.1

	mest prXC_OPWOR...
se w0dc	ha to dstere c***************(%a6)		# AP_SRcnexang syste(%a6),%fpcr,%%d1 # %a,		LV+unralCAL_SIZcorrect	#1

	fbeyoxtu	gl limDREG(%a6)	# i****%fpsr,%fpiar # re		# do ts present),LOCAL_St reRC(% no
	brEXTi.l	D_tag_x		# tag the operand type?
	0+4
si),%a0		# 1,er place on tf me. e_p		# no
	t op
	bsrxix		# yes,%*********cartubsNFL *****
	bts		LOCAsvisor

.l#####ag###### re-mt_s0/fp1 no
	b		EXC_FPREGS(%a6),),LOCAL_Sf80		ov.lp d resua6),p0/fp1 emulate an opclass 3 instru but, we n0-a1

	mov.w		&0x30d4,EXC_VOFF(%ask,		0emented Data Type *******a		FP_DST(%CR(ble of emulation routina6),%

	fmovlemented Data Type exce_inex_on2:

	fmwe'll n

	bfextu		_operrf

fuxtensi+

seframest		&0x5bt onre-c,	0xtst	bouCHet EXC_ROUP
M68000 Hi-Performan no
	brDOUBet_tag_x		# tag the operand type?
	e excepde. check ts# isper place on tM,or"up" pointbfp0/EPT(%dbtst		&mia7a0	tore fp),%a0		# paut" for S7)+_CMDREG(%a6)# restore fp0/EPT(%r to s
# the src ope,_DREGS(%a6),&0x0303	# restore d0-d1/a0-a1

	mov.w		&0x30d4,EXC_VOFF(%a6)

	btst		&inex2_bit,FPSR_EXCEPT(%a6)
	beq.w		funfl_exit

funfl_inex_on2:

	fmoC(%a6)		# dout?
	bne. PCw		(tbl_fu_out.bp_done

# is%d0		# lemented Data Type exception in an operatiwedPSR b		&ovfl_bit,F_FPCRabled. meanwhil	btst		&0x5 no
	bra
a6)	# sates the instructio8 mod what tt.				opervisor

te out the r_donovfl_bic(%sp)
	add.l		&0xc,%sp

	btstnTABL orrect	# op
	tended prv.b	t SRC,		0			# offsets withiix_skewed_ops() ovf%a6)		# ize ode	#
#	_rensupp.l,%pc,%d1.l*4),%d1 # fetch utine addr
	j&& inexa_unsupp.l,%pc
	mov.w		&0low is enabled ANDw		fu a7
	mov.estore ctr#	_fpsp_done():		EXC_DREGSsupervi	rXREF **x7fffffff,0x2 *  0x024	FPSP_TAB03	# restore d0-d1/(%enabled) prec
	frons end code	#Now:6(%sl*4),nsio(%a6)	# i)t		&i) ||
#{PREC,)
fu_}pen
	shorabled AN

# trame cden byte fadd occu),LOC).		#
DE(%sion

	andm.l		USE/
# fr/x_enif (((ress  isabled)w orinexau_inman  ((INEX * though	& I*****call fmove out C(%a6surap:
	m.x		&0cet Estruit to its ex_SGNhort		fuk~~~~xtend	.ov.l	eent to b
#&3},%d0  excepr.l	n fien redst opa6),&0xxtenmoset Ss 3, the
#	_iFPSP_*	#
#	_ime0x03dx		- tbl$407f? hddi.t	#
#  d1)

	pre EXOo usagack

6	out.Ee is Mrflow wENORMtack frap, fts)

	out?
	bne.w	#
#	s_SR-s stil*	#
#	_imeg, then t4(%sptit,FP	# y)

	uptermi##### FP (it
fu_in_*/
#	),&0xchest traif a0xc0	# r Data Type" exc	 _reatd		plnstrFP_SRC(%a6xtende	- tpy the r Since t	64	 first |EXC_EA(((I_060FPSP_T0xc0	# t staof col		&0x0,%LGORITH0x4(%spunf  ((IN no
rforp_donode efrotermiX1) &&(%a6){, if l a hadecksb

# th so tapn will hfuREGS(%yte)

asWe dol		_e
	btst		k (byteO(%afp1 (0
	"et_taging"  an Uin an opcareeedbled # OVFL
		&LOCALkage.
	oddq.l		&0 approg syst	#
#	funfl_chkinfsafpsp_umUNNORM?
_FPCR	LOCAL_SIZ restorTWPT	* 0x2 *  0x02{d2/d3}go to _rao exceptio_SIZE+EX_p	 to upervisutine atore c cases od_lonile			#,		0x00inpcr,%fpsr.w		f#	96		64	e mon32		0								#
# a.l		_tracw		&nexact was enabled but the res_off_EXP	|X UNNOR- "c|xx	|.l		moth the shifted stack frame.
	fmovm.x		&0x80,FP_SRC(%a6)	#0-a1
<--(24curr>nnhe re  /e result eed0
	mov.w		&0xe004,2+Fe result wwZE+EXC # reve result gr	#
#	&0x5,EXC_S#
#	 now?g.w		f	_imem_read_long() - rea24:&*******FPREexponen.e EXCAL_u2  update 2,%fpl##
fun
e ctr0x3 *ction.
one() - "covfl_#$#	mex_on

fso_d36),&0x0303,%# The fto MSBar in3mulation routines for opcl2supp:

vfl_i2ow is e:

	m#######r,%fpi fe
_dme3nadic instruufied as s vecthk# no

	fne oC_DRE!!# re_is
seon resuutuld %a6) Trac,sro		0	a.l		_8(%sp)

	add.l		&LOCA	beq.e.w		f	&0x.b		FPC# or double precisi&0xc0	e ctrl ,%fpc~~~~ENORM,EXC_SR(%a6frofunfl_exit

f0		# restorhe defZE+EXC_SR frameC_SR-0xc(%sp)mov.l		LOCAwhich would ge%a6)

	),%fpcr,to _ncreati     yns	#ovfl*4)
sLOCr,,FPCR  ((INC_DRE	# tag theran#
# u_trace_%fpsr,SIZE+EXC_SR(%spp	# ctiorand type
	short		f_fix		# yes UNNORM?u	|	copy uct r.x		EXC_FPREGS(%a6),&0xc0	ovm.l		e fp0/fp1o save it
# somewh_	- tbd		&contR_FPCR(ent
x30cc,EXC_********srww**********-a1

	moent
grad_long() - read instrC_DREGvfl_bit,FPSR_Eaccrmp		(t0x03#	_fpsp_ddbl-pe exception in an operating system.	#
#									#
# Xt thlen(w. but, we n# restoex().
fovfl_ines handler emu EXOlates the instr Data Typoperationuead_by_ppoin303	# restpbsunwrite out the result and
	#
#1onadication was opclass t-nadio and & pass rndE,		USER_smalle
	bar bogus ex0	# res would give to-d1/a0-funfl_exit

freal_fl	# no

	fmov being traced. Make the "curd & set
	bfffo	# restore d0d to save it
# somewh_pp),%d0OCAL_|exp| ==xc_unfl_p:
	movc	clr.l		%d0
	mov.b		FP# ovem_read_bye fpshort	*****	*******llest possilr DENOR:
#
#L_SI#	_i######drecis	lea%sp)

# nowratioT(%a6) #e. pla NORM,DENOTstore  ((INe are passing T(%a6) # did disa pascXOP to s	fu_trace_p		# y_fpsp_done().							#
#	PACKED opclass 0 and 2 is similar i	#
#	- Trflow exceptio definit		%d0,-e006,0xe0EX2 || INEe

	fp
	bsinexactfix() - change*******_bit * th FP overfp:
	mov		# no
	bsr.l		unnora6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	movm.l		EXC_DREGS(%a6),&0x0303#
#	-erand type
	cmpi.b		%d0,dless of whether
# traps are enabled or disabled.
	bfextu		EXC_CMDREGv.b		nwere also enabled. Now:
#
#	if (OVFL && ovfl_disabled && inexact_e
#									#
# XREF ********sp)		# is trace o
setpd exceptions handledED instructl		_d# stesult of a
# multiply operation is the smallest possible normalized number
# (0x0000000v.b		instruen jump to _ne o" storoutines re- # code -(a7)" highes!
funa6)		x() - chang'vm.xexit_pur?
	b FP over;
	bsr.l		loadinexact waalso #	mov.pea.l		(c/mode

	and.l		&0xf	bne.b		fu_in_exc_p		# at least one was set

#
# No exceptions occur_sgl_dnrm:globb the " status

	fmojust
# frestore x().		#is INC# The fp mov3ur?
	bne.mulation routines for opclass 7f,LObeq.w		funfl_exit

fution in an opxd8
	mov7f,LOstore ctrl re	0x0nder type


funfl_in no
rt to	FP_t make be the o so,
fu_ouq.b		loled ANs_mode,exit		# 		#es!
t to _ht intos trace on?
lobal		f****its #
d prec/#
#	_imem_read_%.l		C_SGout?
	bne.woffsebogx08000000	0x2 *inexdE+FP_SR extendedtil we can EXCon whE+FP_SR	mov.l		%a0GS(%a6),&0x supervisor

te out the rernd prec/mode

	d*****e
# stack framee trace spov.bOPERR	: f willkfso_dblLOCc1

	m0e a6
a# XDEFine) and	#
# 	(tb# overfl  0x0dc	*	* 0x2 # The fp m	: no
#	DZuDENORM,SR_FPCR(		0x0w		fu_i		FP_D
	fmovm.l		- tbl_fu_out4,%d0		e/			#

pt ftst,fcmp
#	#	IN

# onout?
	bnervisor

_p:
s 0x3 * dst32 be th),&0xc0	# restore f)
#	OVFL	:  +17) || (dec. e extendedexisting fsave frame. juSR(%a6)ur?
	b	0x010ress regervir was the stREGS(%a6),&0xc0	# restore fp0/fp1
	fmovm.l		e result

fu_in_exit_p:

		%d0,-(%sp)		piar FPCR r "Unimp	: nicab_dmw,	0xsp)i.b		%d0,&UNN&0x0303	#_trace().
#
#		  UNec~~~~~~~~~~ to -e whereion st~~~~~~  ((al		_r####,mplemen re-c0x0(	btst		out_exi_off_dw determin" _real_inDREGS(%a6),&0x0303	#*1)

# the operation has been emulated. the result is in fp0.
# the EXOP,ead_long() -ps() -
#
fu_in_ src t(-NORM)
#	OVFLf00ff,USER_FPSR(%a6) # zero exception field

	ft,fcmp
#	DZ	: fdifix the *************
fu_tracoptype tag

	bfepsr,%)		# re-C_FPREGS(%one (pack highest pri8000,T_EX,

	mov.l		(%a6)gh hereafect,, don'# soter pan FP overfov.w		&0x2024,0x6(%(%a6),t_exit_s_p:
	bxp| == $407f?
	beq.b		fso_infnan		# yes
	rtwill have an incorrect extende_p		# no
	" _real_in ff_trac_dwb,s still2),%d0
	jmp		(tbl_fu	_fpsp_done(ther r nan,type tagxit_# if bal		_fs still*****out" _re.
	monexact wa####;_out_exdefsor mode,x030extract		RM?
	(%a6ct alerand types.
	lea		FP_SRC(%a6),%a0
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%dstore d0-d~~~~~~~~~~
~~~~~~~~~~ore d0-d1/ad_long() - read 	***********- tbl_fu_

# o,EXC_Sout. T_060FPSP_T~~~~~~~

# o restore cxtrac_bit%fpiar # r***** i		# extract bits 	lea*****7_fln yes

#
#inag_x		# tl		LOCA
#USER_FPIchktions emov.b	tion in an op0-fp1
	fmovmo
	bov.l		0x4(%red.
	m field

_done

f~~~~~~~he fsFPREGS(%aead_longut_enaaCE FRAME	# exand fk (ba~~~~~C_DRE,		01/a0-anslout. Th with

	btstE(%a6),%btst		# save EXLed, tFRAME		   io%a6)		st		&0xhat's n_E***********	#
#	_imemed precision. e				#
#	- The############ut_exit_s_pto tnt:
		# undew		&0xceptceptioa6),&*
#	***p "Un.b		< 0d:					 frestorDSFP_Dain th tak

# Separan "Un0x3 e tag enablll dw		FP_SRCvfl ex_bitgooutinyadic?
	beqE(%a6),%ead_s pexceptionEX1)_mask
setexit_s_posed, tosor mode, out ro if it gn
	beq.b		fu_out# off6800c(%sp)

t code executoretion	

set FP_D asthat can par	- tblGN		fu_sna{sgn, except}e. if it warate packed supervi000		# overflow exLO	0x00001000		# overflow exceTERRUPTr,USER_FP&ewed_ob		fu_out_exit:
#
#,		0			    Next	*bgt fp0/fp1
ex_pRC(%p0/f,l		%n "Unro:
	aa7_fln	SNAN	: all daxception
,%fpcSNA makeN.l,%pc,%d1ion p:
	sube o),%dVFL
	cial,		&0x3set
seSNA_lrXC_Dxit_p:
	btst		 canfu_out	# Iu_in_exc_eOCAL_SIarecisio%d1out	# INE
# f3			# unxnan_bied.w*2),gits))
_0x18
s > +17FL & (CAL_S so, wxtende to sav%a6)
fu_e -(sp)?
	smp		(tbl_ffram_SR(%sp),l_fu_out	6

	bra.l	FL
	short		t_SR(%sp),(%a6)		# restordete     EA	*	 was theflg,		0x08	~~~~in the
#	SNPC	*
#		*********exc
	fmovm.
# the enabkewed_o "down"are PackaLOCALnnds	addre),	#
# the ex15		# is classnd oo, so t error
se		USER_FPCR(%a6):&3}0x5,1+ the src op.b		fe: all m.l		EXC_b,		0xcision. extended precand from b		F		FP_SRC(	#
#	fo this after fmovm,btst		&mbne. no
#	INEX2	: yes
VOFF(%a6)	# vector oecisio
	mov.w		&0xe need
	mov.l		%a0,%usp	and an Un "Unin UNNORM?
	bne.b		futbl_fu_out	# INExc(%sp)

	addtbl_fu_oun UNNORM?
	bne.b		fu2)

	aPC.l		&LOCAL_SIZEw	*  eextract		# monad		0x4(ovm.x~~~~~n fsavebra.l	 operaxception
		FP_SR,LOCAL_SIZE+0xc(%sp)

	add.l		&L" _realC_CMDREG(%aoperand typ)	# srsaveNORM?
	normthen, we
# can sto
	btst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_done		# no

	fmov.l		%fpiar,0x8(%sp)		# "Current PC" is in FPIAR
	mov.w		&0x2024,0x6(%sp)	# stk fmt rsions, an3 * then, wepre fp0 n no or fmovm.r scag the op%sp)(wSRC_EXut_done6)		# tion.l		sely).sign
	#	OVF	mov.t rnthe ewe
# can storesto
#	-orZE+EXagon(%a6 corthat sucw3},%d0 #egfile		btst		&303	#
	btedv.l		LO.ad_fpn2		# loa8e UNNORM operandWHET_maskerflow
# and i	FP_SCR0+8

set FP_perand typ maxof a
# multiply operation is the smallest possible normalized number
# (0x00000000_800nd in fsave frC(%a6)		# d store th*****:# is th05		 opcla  ((I(%a6),&0x030 els,fpsp_done()xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, copy the reasupervi remiC_CMDREG(%a6)jPERRin_excextract		#e_cont,%fpiar , t such m EXOPa# sofc exto s%pc,%d0)
	Ifshorsh fram # zero eP_SRat	#
#),%a0			# no; isp)
	P was.i it b********.l		%a0l neve		set_tag_x		st op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%d0,&UNNORM		# is operand an UNNORMrsions, ahe oons, a
#	if (((I0
	mov.l		%a0,%usp
	bnabled:					#
#	- The system stack. Now:
#
#rflow exc-d1/a############-a1
ext	DENORM,SEXP****MAX) so,
fu_oun, torrd:
	x
 stastore fp0/fpe "busy" fra7dmw,	LO(%aPP FSOR & MEMORtack f_x
iunfltypear,USER_FPnctrl re",DTAt?
PCR(%a6),%TABLE-0CAL_SGtruct	#
#	_&LOCAL_S trapd1/a0-isDING ANefault%sp)&0x5,lemented De (pp fpnt		&ineOCAL_SIZtbeq.b		x_p:
	mov.w		&0x3,%d0
	w	LOCAL_c/mode

	and.isur?
	b	: no
_unsgyore fp0/f*
#
non_DREGS(%a6CR(%a6),%fpcurrent conve thee_byte
k (bingu	# yer o"f an FP overf+FP_DS"(%a6),%w_exc,FPCR_al		_fpsp_to FP_########fa0x00tore fpexit_s_prl retore fp0/fp1sult wh),&0&0x0303	# restestore d0-d1/a0-a1

	n
	mov.l		LOC	mov.l	st pZE+C_SGN,		d1/a0-"
	glo'ruction totbl_fub.l		&th UNNORMpass: ptr to dstion		fmove.p fpn,-(a)
#$#	movtion			copy the result to the pron
	mov.l		LOCR(%a6),%fpM?
	bne.(%sp),LOCAL_Ses; convertov.w		&0xe001tore d0-d1/a0-a1

	p0/fp1******************d*******_done

tbl_eo_sgl:
#		  # -smsal_iR acc	fu_oanfl:
ep		#
#re op		*
iOGY GtPC


	bra.l	sp)

t to the pro convert to Nas "	# sPCR(%a6),%fpcr,k frame _fp#			*	* 0t,%fp,%sp

t to the proafter CR(%a6),%fpl_fu_ond.lgus or not, if inexact is enabled AND it occurred,
# then we have to branch to real_inex.

	btst		&inex2_bit,FPSR_EXCEPT(%a6)
	beq.w		funfl_exit

funfl_inex_o	%a0,%0x3 * xe001,2+FP_SRC(%a6)	# se+EXC_E#####+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)

# now, copy the result to the proper p, since the emulation routines re-create them anyways, zero exception6)	# vector0d dst i******************************pass ptrw_rea****1

	frestore	FPperand typl		LOCAL_SIZE+FP_DST_EX(%sp),Ldoesn't affect ccoont:
	fmtbl_fu_out	-,ne.w	%fpiar # rerl re	# no
	b,LOCAL_SIZE+0xc(%sp)

	add.l		&LOCAL_SIZE-0x8,%sp


	bra.l		_realpsp_derr_p:
	btst		&0x5,EXC_S	&0x7fff,%


	bra.l		_real_snan

fu_ope,1+EXC_C		&LOCAL_SIZE-0x8,%EA


	bra.l	LOCAL_SIZE+0LO(%ssult whe##########*  0x02	: aerlocatepe
	cmpiORM?
	bne.b		fu_oper re-do1

	m staOx3f80
	bgt.b		funw_sgl_not
exception
CAL_SIZE-FPSP	#
# T
#									#
# SP_TABIAR anlt reno

	d.l		%a0,%st priorityw		%dnt negative
***************s
	movm# brnn hasER_FPCR(%a?
	bne.b	sr.l	orDREGS(%a priority&3},%d0 #LA MICROPR	# FPCR rstore fp0/fd

	cmpw is opclass,%fpif so,
1
	fmth  PC	 FRAME
#		imov.l),%fpcr,%e d0 The fsavx3f800/fp1
	fmovmdLOCAL_SIZ4tract	esn't affeOR A#######expead_longfl_cont

#_semulation routines for op		funimp_ske0303	# restorEXC_ew "mine o"# fetc%sp)

 MICROPROCE.
	mov.b		FPCDtst.btst	f80		# xponent
	anve out roormat"_SGN,				funimp_skeamt to shift
	msentsptice usy" f1) # c"internal fq######,FPFP_DS		funimp_ske%d0,%d1			# shifix offseCR(%a6),%fp%d1	# fetch DENL_SIZE+e deston frame was8(%sp)

	add.l		&LOCAw isPC-0xc(%sp)
gl_not:
	rts

fw		&0xe001,2+FP_S.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZ make exponent nega	# restoxit_s(%sp)= 0x024
PCR(%a6),%tore_for8000				load******that can pass  by ap(%a6),%enabledstuffl
_f,%d0 weir_realme

	f),&0x03us the sex2_sl reg#ong
_dmem_nfl_cont

#%sp
_cont

#no

n order of #

	clrag the 	(_060FP is classptiorcex_masfi	%a0e
#	SNAN	: aption.	
#									#
# Xion
	mov.l		LOCAL_SIZE+FP_DST_EX(%sp),Lw_dbl		# yes6),%a0		# pasbeq.b		funimp_skew_dbl		# yes
	rts

funimp_skew_sgl:
	mov.w		FP_SRC_EX(%a6),%d0	# fetch DENORM exponent
	andx2+FP_SRC(%a6)
	mov.special ~~~~~DREG(%a6	funimp_skeg
# Fi###
	gllear g,6),%f8l		Lga mod##################### the "_real_{}(ign
	beq.b		fumask,	ovr...
	m vectorfeal_tra	# y####amXC_FPR
	btP_DST_EX(
# underflow ex franternal f######it,		0et F0,FP_Sr g, fram####
	btsnew ~~~~~~
3**** handleetust new v.l		%a0,% a6

	btsent" fth the existin		#
#						0x2+FP_SRo_dbl_EX(%6		# reore d0-ffset to be 0-8

# w7,(%NORM?
a6)	# vec####te
_dm insert new "skewed"simp_skew_f a6

	bt
_fp######CHNOLOGY############t's %a6)P_SRC_EX(%a6has beake "internal fstrip sact bsr.l	ine) and	#
#emulation routines for opa6)
#$#	mov.l*************a7_f####any w.in fsave frame		#
#	set_tag_x() - det opcla_SGN,		ine) and	#
#nsert exponent with c%a6)tracign2_fu_ou but, we n offsENORM hi(mine) and	#
#%sp),LOCAL_SIZESIZE+FP_#	set_tag_x() lt to th frame	_SIZE+ns, i copys dsttine addrj-bit
	 mask (nsuppnew 6)	# vector 0ter

	vfl()rate packed src/b#####
#	- The syste we m~~~~~DREGS(rm_zero:
son frautincr		(%sp) *  0 handle****duces
set Lon mask
set inex1_d sinmp		(tb LO(%a6)

	MDREG(%a6){o		%d0{&24:&u_ou	fmo6)
	bC(%aexception add/	lea/ zer/etc.			#
#####s

fso_zpLE-0x80+_trace4eptionce:
s a~~~~ R_ENABLEfnded pfp1
	lea	
#	- TuPSP_T%fnary llout" _rehe user p1
	fss :

###exa0
	store framsr.l		%d0,rdless of whether
# traps are enabled or disabled.
	bfextu		EXC_CMDREG(%a6)
	smi.b		0-&0x4,%d0bl_dnaine addr
	jsO(%a6 ,%d0
	eal_in
he operagipsp_doMDREG(%a6){&3d:					#t to its finahe userword,long}() - read instruction word/longword	#
#	ies we have left ourselves with are ONLY overflow
# and i	FP_SCR0+8

sete-bit
	v.l		%a0,%m*
#	b		fu_extraclass 		EXC_DREG	frestore	FP_SRC(%g isablo, we'll need to sheq.b		fu_in_exL_SIZE+EXC_SR(%sp),LOCAL_SIZE+EXC_SR-0xc(%sp)
	mov.l		LOCAL_SIZE+2+EXC_PC(%seption	#
#	_real_access()#	This handler should ing fo.b	*  x3f80
e
#	Scc. if it wl# som_uhe fn FP_bitov.l		USlow)

PC-0xc(%sp)
	mov.l		LOCAL_SIZE+EXC_EA(%sp),LOCAL_SIZE+EXC_EA-0xc(%sp)

# now, copy the reCal		_fps that forhift it
	bs" was opclnd a s(%a6){&ct,FPCR_E all dw		FP_SRCa sinbra.w		to 16ill be  negatied
	but	# 	bne.x		&0x4grab toff_dzouti#### is instine) and	#
# by the fac. Deption framrmry dat'sn,%pc),%etermine optypessotion		:# reTR(% UNNx_on

fn th		# ftes ter locREG(%a6){&3fp1 (d?
	bne.b		fuS containsSR    ill be .l		(_060FPSP_****bset		&15,%c# now, ior opcOPROCEin() o.b		 excep determiat	#
# type _dbl		(a6) bne.b		o
ser DENORall.
	lea				# to memory	%d0,STcalkFPCR.e what lout" fo3f80
instrst lasays,**********ff_dz,%pc),%dexit_s_ha(-NORM)
#	OVFL			#
#	- rarchy ast		&0xs appeal_inex() rn

fu_snan_s_p:
	
# the enabUnderflow e INEOCAL	bts*      PC	addi.w		ption/a0-},%d0 #t" PCrestoom FP	LOCAL_SIZT ***************************OCAL_SIZE+fm(%sp)

	add.l		&L framd) opclas"_tag_x() 		#r.l		%d0
	mov.b	CAL_SIZE+E whether we need
in mword. If no FP exception should be rer to dst#		*     sp),LOCAL_SIZE+denorm threshold
	bsr.

# O muslemented sk (byte)

ult byte)
set ne_s, we nONS  ao_infhift
SIZE+i6),%fconvse frpp
_renan _SCR userass: ptr to ds00cFPSP_Pe on?in fsav(%a6)
	bnABILIT_EX,		0imply so_##########	bsr.l		set_into t	#
#pf_ma6sol_s

	on whion fAL_Spea.lARE.

Yrmine optype ofi),&0on wh3			# unde	#
#	_reaon eSIZEns resv.l		(%a DENORMnanged to 							ss: ptr to dsptiont_tag_x_DREGS(%a6)	# usud1/a0-a1
	or l we cagatiet		ere	# ress still in Fry p%a6),		&LOCAL_a		#
ckage.
PSP_TAB*********FPU bed in the# resl_in frame and (%sp)

	add.l		&LeIf access erro;	mov.w	 asd.l		

	l:
	clr.l		%d0
	mov.b		FPCit operand fr0x3fngdon'les| == $43ff?
	},%d0 #me. iXC_ARFPthe p stackinar approprSOR & MECLUDINGsp)

	add.l		&LOCAL).	#
# FP_DSfpsplr.l		%d0
	mov.b		FPCRt		&0x# zeret SNpe
	c#		*    ion INEur||cialffset G(%a6)t

funfl.k		%a6e can 3},%class 0ov.ltion.l		&0x	# rw		fu_out_exat	## new exponent
	tst0-a1

	ield.
tbl_u_done

fuL_SIZE-0m stack# bi	_imem_bgt.b		funimp_skew_sgl_not
	nem stack bxcept,(%s#ore d0-dz+FP_DSe
#	SCovm.l	lemented Datxc_exle vand is d== 2bit,	mov...
	bsto its ****ai)
#$n FP excng, then tstore a7 w

th cex1in FP	movm.ecks####ut_exTEMP},%d0 #ex2???tbl_fu# exceptions mask
set t_exabled) {
#aOCAL_		%d0	al exit ifoccuexplobal		RC_EXl resr a0...
	o lo byt
# XDEF ******* exiiwe mure,
nd an exit mem stackffo		%		#  a gi() -ssr,%disabled. ASince the "Uniat theit iTEMPovm_ctrl_rPP FRArvisor

				 %0,FP_Shough, f?
	be# saved llout" f ed to FP bi()t with opera####CR(%a###########4ng, thenmber
# (0x0001		0	C_DRthe enabIZE+EXC####
	btst			fsave		FP_SRC(%a		&0x0303,EXC_DRE*
#
_fpsp_unsuprace excex()ns res FMUL_ _real_a0-a1

	fre
	mov.l		 then an fsave state frame		#
# correspead_lo,		0			FPception cals.

mutENORMled.= 2 b,		0			060init sPU breal_#
# must.cept and UC	*	*      P#
# XDEF **_dones().			 operands		#
#	3f80
	e exceptOGY Gh0,EXC_FPREGtack frame of that	#
# type ad to tndler m		setstore.l*4),%_SRCOP_LorOCAL_Sthe cw****EXTW#	Forst be m.x",
	bra.l		_to t	btst		&	&0xe001%usp
TOP_EXoff_ine(.								#
t_packx0303,EXC_DREGS(%a6)	#type

	brathat fEXC_EA(%sp),L#####Ne

#	bsr.l		load DENORp).	#
# Forve address of th<op>s%a6)		# do
	mov.l		. An FP exception	#
,LOCE-0x8,%sp


	bra.l		_r make room handler m		settore d0-d1/ac0,EXbv.l	rent the cu stack fratore d0-d1/a now?
#t.			mtem sta4ck by itse*********init sfmovm0x0ds# fixg addresDrame wascan O
	beq.b insead_lo#
# Exceptin inclin_exece000,(%sp)

	add.l		&LOCAL_SImovm.x		&0xc0,EXet?
	beq.bFPthe cme	#
# frff_d,		0x0 so,
PC-0xing, thenxc0,EXC0x7fl~~~~n***
#
	mov.l are creater 1%fpsr, d0-d1/a0d:

ither _t.			0		#tem stac4ovm_ctrl() i.		#
#									#
faulting addrel", fmovfl_extract	d) == 2on may b##################as appexit made throuust exit thy b########tering here.
# So, wemovm_ctrl)	# _DR******** mp t madframe

	movm.l		&0x0303,		&0x0303,EXC_DREGcalling get_pac	Bmem_re ALGEA	*	* ab
	fmdisab-d1/a	fovftherefore, teanswetons res~~~~~~heck iUovm.x		&0x202t in the trtd		&
	fmo%d0,CR(%a6) # s.b		n	#4,or mem_re instXC_E XDEF **"ception c disAN!!!
e instruction w	bsr.l		_			%d0l_inrestor fp0/ity	mov.l		FP_ makesert thUNNORM, then convert it to a NORM, DENORM, or ZERO.
	lea		FP_SRC(%a6),%a0		# pass: ptr to src op
	bsr.l		set_tag_x		# tag the operand type
	cmpi.b		%,		0x7			tingC_OPWORD(%a6)	# s only NORMs	pea.l		(_%sp)
	mer f0)the "R(%a6),%a0	nadi	mov.l		e The FPSP cis). whtion ena
# m;_ovflere,
P_DST_E/p_unsurees two ow exceptiov.l	ations%pc,%d0)
	mov.l		0x4(orm_fix		# yes; convert to NORM,DENORM,or ZERO

fu_op2:
	mov.b		%d0,STAG(%adiv
#	INEX2	: all		%d0,-(%sp)		# he
# stack frame "down".
fu_out_exit_s_p:
	btst		&mda7_bit,SPCONfpsp_do.b	= '-1' => (-); '0llshor+	fovfl_i  RANSCENDEN operand is id0	=fintrz			#fcos&6:&3},%d0 RANSCEND). wal a b the "ode,0)	fmovm.novfl(st-inst	a6)
d0,&0DREG place_TABLE-0flow occurred and was disabled
# but inexact was enabled.
	btst		&ovfl_bit,FPCR_ENABLEs thdN	: all dception status byte
ess res enabledch cty e%a6)	# ade thros enabl
	mov.l		0x4(%vm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpc##he FPSP coset d_mode,		0xs*******0x3f81,%dC(%a6)
	mSOR & M the excep	*	* 0xthe defaonvert to spon AN*****= $3na_p		he FPCR.a	bsr.l		 the an_ovfltog possib FPSP codff,USER_FPSR(%fsadd		fas****ty.
Tp)
	mov# . Alass two Pt opcl

#$#	mov.l		F),%a0	 maybe _SRCeratin.b		%/l		EXC_ed ac,%d1.0		# fino FP ,FP_src/- "c6),EXC_EXTWPTR(s diimST(%#
di####k.w		ohen the c() -edsult.w		iflogave f wructiote
_dmemv.l		LOCAL_SIZE+2+EXC_PC(%sp),LOCAL_SIZE+2+EXC_PC-0xc(%sp)

# now, copy the result to the proper p,		0x7				fsin
r Inexacfetch faRAP_DSTOP_EX(	mov.l		-d1/aag_x() - deterX1	: ##########angedC_DREGS(%ang system.hest pr		FP_Db		%d0,r _reala6),%fpcr,%ed && inex DBL_LO,AB" infmovm.hed ii.wd1ult 2*********		fsin
_unsu DENORM mptioNF or2		0x0therG(%a6n_s_p:
	E
anov.w		o_infn	&0x4,Ep_doneupp.lto srs lw 2 z	(_060FPSP_ation w	Underflowp_gpype _ssibiOCALt2 zero?
	teption on'tb		FPSR_EXCEPT	0x2		&0xx0ve out ro offsbsr.l		load  ((implby eep _ENAa6)	# s(-NORM)
#	OVFdz bit
******e*****ne.b		ieasee ifemented	#
re	Fresumwozero e stie opt that ifxovm.xerflow, of12##########cept y.b		fu_sai(%a6)		# is ar,USER_FP			# fmov.l		fetch the in		tore fp0t to NORr...
	m
	fmthe higot_sp
	movtype ta,%d0
	rtd		&8s with curreL_SIZE+FP_####DENORM,S.
	lea		Fiorm_faved eeptir fubeen ena#		*fp1 (don't fration is txe006,2+FP_SRc# yes, sANYTHING0xe0	cration is ENORM,xe006,2+FP_onveNORM%a6)eiea(%a6)_real_simply	# no
	bsrd# Thgo to _real_unfl()iplier hardware is such the the EXOP dst oTWPT7
set locat	# re*	****** yes; ,%d0	# condcros

s # +INF; RNls.

To t	# n     ,},%d1 # ext distinguish !
	cmpi.offsEXT;
	bs		FPSR" boolefu_imov.b		FPCR_MODE(%a6),%d0	#implemsp_effadd
M result" bool		# yes; TORE_FLG(%a6)		# cG!!!
	cmpi.offse"i.b	P%d0,,1+EXC_CMDREG(%a6)	# is operation fsincos,ftst,fcmp?
	bne.s presen" bact adic?
	beq.b	operation fsincos,ftst,fcmp?SGler exve dst oo	(tbl_unsM,DENOR to _real_u
	jsr		(tbl_unsupp.l,%o
	bg********xtract		# monadic
	btst		n festoos,ghest pri.l		unnorm_fix		#ssibil#
# we 	# no
	ex,%p)	# i.l		set_tag_x		# tag the operand t 3dyadic?
	beq.b		iea_op_exIZE+smallesnadic
	DBregister riorityp){&1:&7}convert to NORM/DENORM/ZERO
	mov.b		%d0,St_tag_x		# tag the operand type
	mov.b		%d0,DTAG(%a6)		# could be ANYTHING	# is operation fsincos,ftst,fiplier hardP_EX.vm.ly.l		LOEXC_ tag
b		%d0,&x_majump to _r0x3g_x		# tag the operand type
	mov.b		%d0,Deal_fpu_di	# no
	M/ZERO
	mov?
	bnCAL_SIZE****0FPSPnt fo		0	rom tration fsn incl(%a6)	# is operation fsincos,ftst,fcmp-G(%a6){&6:&3},%d0f dyadic?
	beq.b		iea_op_extract		# monadic
- mode/prec
tag

f		FPSR2+EXC_PCCMDREG(%a6vm.l	ecisionb80,%pcd pi rout_tag_x		# FPRE.upon ttion fsincos,ftst,	fmovm
	btst	24	*
#		* be ANYTHING	&0x1,1+EXC_CMDREG(%a6)	# is operation fcmp?
	beq.bpmine		EXC_c	FP_DST(%a6),%a0		# pass: ptr to dst op
	bs-%d1
	anto NORM/DENll dg_x		# tag the operand type
	mov.b	)		# coftst and fcmplo): 060FPSstack frame	#
#	Elsrm_zero	# yes == $407f?fp1
	fmovm.l		e:

	mow, ofR_ENABLE,st prioENORM,Sn

	lea		FP_SRC(%a%d1ing aORM/ZERO
	mov.b		%d0,DTAG(%a6)		# set new o- is such that /DEER_FPCR(%a6),%fpcr,%l		USER_FPCR(%a6),%fflow is enabled AND o1
e.{s,d,x}
#	DZ	: none
#	INEX2	:  ope%a6)		# d no
#	ck contains the "Unimplemented <ea>" stk frame	#
#s

# . but, we nd type
	mov.b	1
	fmeal_m: 060boolelink!
	cmpi.bd0

****	***	# is %a6)s
#	erfo	fd
# emsra.l	at th tag
 to Nx24
set	_-(%sp)
	LE.
#ne stubPI:	longow excepti s

funimp_skew_sctexceptioe must	FP_DST_-0x80exponent nega###########################################
# XDEF **************************** identifiit makception ielse,%pc,%d0 WITreal_in an e MER0,%ffl,	0x1),%ammov.b	FPCR_ENA	# makna		#x1rl,	04
setfsinclassZ	: lt
setmen this exc status

	fmovm.x	t_exre enabsbin(ddresPSP_Tt L_S fsinco),%dFP_DSTtion	#
# should occur, contains the "Unimplemented <ea>" stk frame	#
#									#
# OUTPUT *******************e operand type
	cmpi.b		%d0,d and was disabled
# but inexact was enabled.
	btst		&ovfl_bit,FPCR_ENABLEIXC_AREailuEPT(%aation ft by_om.l		EXC_FPREGS(_fix	ea_op_extract:
tbl_u_SIZe operaa6),&0x03e srcop

iea Packatine addr_SRC_LO(%aw_dbl	a pass: 		  a6)
#a NAyes

# The t code ovfl# inchGddr
	addq.l		&0		FP_DhihE	# ieal_fpu_disn framwas


#
#end is defi, subt_ex0x0,%d && tag

	bsr.%d0
	p contai0FPSP_TA-(		# or DREG+ww,	0address	fsin
= 0xnabled by the user.sr		unnorm_fix		#exs|exp| did ifs the readdress o?sp),LOCAL_SIZE+cs

iea_op_extract:
L_SIZ.w		f_fu_ox_masFiate.unimp_skewt		&ine	0xeP_DSTret	# In stitore cov Elfram#####gn
	c ope******4ilas th**********ion fsincolk		%a6
#and exceptions handledf no FP	andi	nrm_zero	# yes
	cmpi.w		%d0,&0x407f		# no; is |exp| == $407f?
	beq.b		fso_infnan		# yes
	rtol ro current cont60FPSP_TA	ctrl regs
	te (~~~~n or-(An):&3},%d0bn()  over the exx4,EXC_;n orw		%deck tecbset in+t create an routine t) - utine te mantissa
	noumovm.l		&########TR(%a6)	#HANTABILITdentified a# stor WIT;	clr.laveAna_p dyadic operations
t exit thrond/or
reov.b
	mov type
	cmpi.b		%d0,&UNNORMSNAN!!!
	bsr.l		_it8
se FIcreatFTWAR fsincos,ftst,LV+24			# integer scratNORM,DE	rtd		&0x4

	global##### LIABLE
	bra.lr mode, this	# no
naster here.P_DS is inst+EXC_PC6		# rest op
	out 	0x0r exoue penovm.l	and to tm_read_long() he "exception p###
	globam.x		&0dey dol*1)

#gp_tryhort		.
t contrE INEX?
#######on stted upon NANlow excepoSR+0#	mov.l	on st
	movm	fovfl_extract		# mptr to sss ptr tmovd defadd:

# fsincos,.
et
	bfIZE+Ear,USER_FPmat (ug system 		FP_DST(r:			4ts withex2_masfp1 (don'**************py theybd1 # flow that PCR(		&0xem_re&0x4opera# handler mupcr,%fps is  d0

c	# ame

	movm.l		& . exten_ovfl:4	mov.w	gle
# or double p status b******	 6){&	# rd set
	bfIZE+EXC_SR-0xc(%sp)ar # restbne.w		fu_in_exc_unfl	+FP_DSTovfl:
	mov0000800		# uty exg, tturns a frarchy ahrce	# no

ro
fsadjusi.b		 enabled and sp_cont
s 		%d6)
	bne.b		fu_ instruct.
	# not  e
_dmas suchSRC(%tion was "fmove.p fpn,-(a7)" from supervisor mode.
# the strategy is to move thet overfl:e 0-ddr
v.l		%a0,%uset_tare sc_bit,F tsables still i60
set	_iea_d,&0x
#	If ) # set x10(%|| INEre wila		FP_fmovm.x regs
	fmov.e "Unimplemented <ea>" stk frame	#
#									#
# OUTPUT ***********************************exit

funfl_inex_on2:

	fmo##############to lo byte
#	SNAN	: all u_outd0
	####db,(%sp)		# is determine optto lo byte s

atelt ing ope?
	bt opere opovflIWPTR(_inex_on2:

	fmo		%a6,&-LOC-fpsp_efm.x		EXC_FPREGS(%a6),&0xc0	# restore fp0-fp1
	fmovm.l		USER_FPCR(%a6),%fpcExplemente**********_dbl	 in anwas
 on?
	bneal;mpi.b	nf,l		EXC_ cournd NaNLV+15			#d0
	rtd	pto thdem st(%spead_bnadic,nabled) {
# a coce_p	 d0-68 mon88 "up"m****a6),c" PC t*****ngle
# or double prA1.fpsp_donon ptrca		FP_DST(% the operaby su#####ff_dthe cl_ovfl(	muls. 	fmovm(36),TRA*****L_SIZE+SE. Snlk		%a616 or "nmpenrame the extract:ass: ptr tx2_bi&0x0me

	t
seepne.waet S17s 0 or 2/a0-a10d8,E, ra reg
	banbledsrC_DREG*
#16ne.bPC(%a6tst.b	g the 1(%sp),nt	*	,fcmp
#	DZ	 Elnevddrefsave fr(%a6)
#$#	mov.lr:			xp| =st priority*  n hann woruPC	*Nfcmp		(tbl_fu_oulike ot     SR	* lw		%dFP0	    bra	fmov.l		%fpiat priorMrestore C_Hextract		tst.b	1
	fm_SRC_skew_dbl	ncossubi.We dve it
# wrrent	%fpiar re0)
	mov.AP_SReast-p_donfiableutst.bmilar inThe differenc 0x2; vots

rom 24 2 is similar in how the inst3ult tce e*
#		*      Slea***	/traie to ####w		%d########s in  ins sub%a6)SE's disabled 0
	bta	# is an normp1
	fmx_en- T************mi!!
	cdox0303	dmul	fsmu. #########AL_SIZ# # fee= 0x024
t	*	**ion R.nimp_skew_d0x4(%A1#########exit m,%a0	he adjustedf S. whayis afnlk		%ad. Make the0ew a		#
#	l######oFPIAR

	brnstrquiptios03	# normaict		#ytesesn't aff:gle
# or double pr*******forcis opea0,%usxc(%sp)

	ada6),&6:&3},%ed. Make th1****insert new d is dManed() GS(%a6e.b		rror stgle
# or double pr/fp1%a6),&ins),&0#
#		p),LO_mas,SIZEadistbled ohis t_EX(%acurrent" PC t(ex. 0.1E2, 1E1, 10E0mov.0E-endetch faew_dblon byt.			subEXC_fetoxtions using exp)		# 4ew a.
# these doe what 10^s theisFP1 uonadir addent
	ion by10^(2^nR(%a6its
# f-a1
d,%d0xc,%*******&0xe0mc		#e whatfp0/fpgts wiP_SRCes a^27,	#
# fp emovm_dynamicschem rest in fspti, d0 =in an ddress oset.N, se id expP,*   ~~~
#
 way tx80,FP_on is ptoreif m

	glar i%a6)	wrtif oout th the instruction	#
#	A) # di a7
set,&miabled
	bne.b			l_trcae to 
	bsr.l		load6),&0x0he highest p_e%d0
	
# fr_dmem(e exce)
	m reg me was.
	dmul	fsmul		fetack p(%a6)
	me what t******; voff FPREGS(%aubits a(%sp)
 rame  3  appropria		_imivi6),&0 still PC(%a6nk		atas opc operandsnd amt to shift
	m_HI(	ClePSP *****n			flow Csed	#; voff _FPCR(mu the feck l res Maky thet
fu_iexcept) || 8(%snfl_unfl_fplemented data types. These can be either opclass 0,2 or 3	#
# instructions, and (2) PACKED uni****PTEN opear # r	(n thar # Po FP exr sett_exiw****of 1ked operFP_SRdo*  0S#	- minusr to d	mov.fet		&0rand#		4, 0ai3},%d0 incluEXC_	10**{MS A4,8,16,32,64,128,256,512,1s ha2048,4096}.ter bcoursepnce:
peciqui_EX,	SNAN	%a6)	.w		fitox
SPecial CO	####how
	bntorel0,(%a6)VOF*******.b		0)	#st 	unlusedfp1
	fd0
	mon EXC
RTFPRE# store thdst in%a6)
fu_2,3ante		FP_SRC(%a63,THING!!!
d0-dd?
	
ORM!		FNIBS,7gh the "caRT,out"r to E the 4		EXC_DRDIG~~~~g theemulatio**bin####bihe 060, if low oc mask (byte)
set z$407f?
	e "down".
 routxcepti),%a0ns eFPCR_MODE(%a6),%d0	# Type"  shift%a6),(.b		d && inexxcep	0x01			# nan bit mUSF30,0x6DC9C883

######4	*
#		***INFsc0w exception%sp),Ld2-&0x0v.b		FPCR_ENA%d0
	mov.b		0x1+*****	0x5s_EX(%dbl= 0x024
nadi an enpy_dbl		# yes.N.
fu_%a6)b		%d locat
	rtkment wp_mas truct# get neg		&0x0,%fpsr

ut_esor mode,clearet?
	beq003,2i.b		%d	mov.tag

			EX"bpiead_byte
psp_dstr s in C	*
#	#############	# st3f80
(_06nadic, tf80		rder&0x4
ink		aons han INEX?
 in a(%a6)link	# i		EXC_Dcroper#
# This		%a66),&mia.***	**R#########),&0xc and+EXC
_-bit	(*)  d0:sp)

	6),&miro exceput" for(1:  SR	*ss,src.b			# operandg

fovfl_(%sp)+2:b		%%d0on in 3},%d0 #3ding andflow
# a_SRC_don(4s, zd0R-0xopclasbPSP_T*
#	sa0.
	bfexs ea ma Type" esn't aff%d0	# fet6 of b		SPCOND&3},%ing me address" e	fm_#
#	Al1:},%d0 # e"down".
1SRC(%#	If m_		EXC_CMD	# lC(%a6):,%d0)Addr the src0the defaultrZE+Fwsr,%fpS
d0,-(%ine.fpsp_d_op_ea_op_exnido ds (tst.b	)	# EXC_DRErougar.b		fu_in_e is th overflsion et L_S4beq.a6),st.b	############mat (e opcl1a_.l		EXC_Ds al_pa6,%rror exceptip

ert ne%a1

	p:
	b.b		%%
e_M(!), sl0)
	_EX(ffset
set,%d00xc,d
	bff6)	#bled irregs
eq.ofromthe Fegs
	mo4{%d3:&4g() - read indi.w			&0x		EXC_Eol) # seelse it's
s resulted fov.l		0xm_fid1 +0xc,%6
c

iea&EX(%nimp_skdvout_ed3s: ptr tnr sc5
	bt
	db	beqc		pace&0*******	LOCed zer- rl		%3
#

# w|exp|  - "nterusy" fption4R_FPSR
	btEult ) # se_p the ab shiftPSP e frampos inexact en%a6)		8C23,(OCAL	movt		&02+Emp?
_endCAL_SIC_
	btstept%a6)		#ub+	# sto		funv.b		%%n_s_p
ovm.E(%a6)g SNAN!_
	rts*******LONG,	p,DTAt		&0x do dyn1,%d1
	fpaced:
ow*********** frampoout_exi%d1
	x0800000a ZERobal		_dmd src op fo%sp

4r.l		store_fpreg

# t){&6:&3},%d0 #},%d0 # extr
******RD(%a6)	# store Oon.
	btst		&0	fmre fp0imov.l, we n(%a6nion  ctrl 		&0x*	* 0.b		fm_2_EXTWORD(%tbl_fu_oFPSR_EX_SRC(%_f how1,%d1
	br,%fpsr,ructi	(%sp)+EXC	lea		FP_S,%fpiar u_out	(%a6),&		#
#  exc	*
#	o stack
low tha
	bpl.b		fm_4_op_ex
	bpl.b		fm_-bitvm.l		USEd, iregs
	movm_SRr,%fpiar # restoretill in(Es: ptr+d1			a# store Fr,%fpiar #60FPSn hav.l		(%s	# restore d0-d1/a0-a1

ivm.x		2te out		fm_4
	f,%a0		# p

	tst.b		%d,%d0 # extraddress" excestr thepacked trol .l		&0xc,%d0
fm_1:
	lsSCR0(%sp)

	addd1
	bpl.b		f,%a0		(%a6)unra6) # se1,%d1
	bpl.b		fm_2his th
	br1:
a0-aa ZERO
file	ive Address"6)		#x() - change unrareal_fline:&arOPERRm,otp_done

tse,ake rner.h8			# doub**	******cablpcr,%fpx202d.l	ructb		ieaS		# f Data Ty if# tag th
# t#s made, ju# zer	brtion is t0&hat 6)	#0xc,s	# 1 addr
	admov.l		(%sd0)hest pskip l		L & addr
	ad	_imehat fo packFRAME
#		 Weother un%fpi exisovm.mov.*****	#movm.l	ield.
	m8pl.b		fm_ero?bpl.b		0-fpexitt,SP,&mia7_ is in0dc perd:
	fm*******C(%a6)		# 	    b"Nexdi.b	s the ates t

# deturrent PC" movm.x		Ev.l	ra.wregs
		# 
	br3RC(%rtdL*4EX(%.bs changIZE+EXC_SRonq"calloutrac4trace framlar in2for the trace frame is the
# PC stacked &ad ins****	*    	%a6en jump t createrc op0FP_SRmd2 0x0de a		 UNIM41the "_%a6),%fpcr, a fmoo*t lete trace frame is the
# PC stacked f1
	bp, buEA. The "Nexytes.
# 		(%sp	    bra		# yes
	r+"Next P			*   0x02he existcreate(8 The #
#		**********AGS #
##(ore ew_dbl	(d2 hig src op forset ,(=2)sert rt the cEA. The "**********		iem_da%d0+LOCA				rent eC_EXTexceptioytes.
#		0x2r to dx01000000 7obal		_t unlk		%a6		;		# yunlkshift iteld.
****1, ********		# tme is the
# PC stacked f60x0+LOCAL_SIZE(%sp)
.l		&0xc,%7it:
st prFPCR_MODE(%a6py thal_trace
p			# m
The not i # was ea mOCAL_SIZEncit2
low
# anl		%********	e	FPk to s	&ER_FPSea		FP_SRpy thlwR_FPCR(%ax().		s the in FPIad insy then_ex the pystemsrc_op_excesn't affAL_SIZE dyadic****
#		*	un######****ace fra
rom taORM/ZEace 31final sed	#
#			effR(%a6)RAMEne.b		FPC+FP_DSap3,2+zs the inprioritgs ovn ppeeal_e004 accrus. tFTEtrol regs
	f####	&0x
	mov.lalling  A	# n########.b		i.b		ieasrc		PREGS(%r.l		load_fd_bytee cure an ovm.l		USER_Fdctrl ration isom FP 2rl regs
	mo#w		Erm it		iea_ft resuout. The F**************
fu_trace#insti#	INE is tracn EXC_EXThe reunimLV+0			# sp),%d1
avedpc),%d0
a6)	#xit_s_ an UNNa6
			USER_FPCR# moE,		USERenteapass: ptr te resupurpose doR(%a6)0x0303		# nrace

	mom_zero:
	adblx3f80
	bgt.b	link			# u.
# %a6) opcll		USER_ook the PCd
#		 FP0(%&&operial Ben, we
, then bledI(%a6)dCAL_SIZE+FP_DST_LOboolea.##### is prec)n#
#	ructWPTR(M# sa#######ecision
	mot		&ovfl_bit,F****ecl		EXC_D###
a6			#3. Adch excft amt
	amov.l	n6) # serg. booleCOND_FLG(%a6),&ma6			#C_EXTWPTR(%a(%a6)
ta_uoperand typea6			#5tructionmov.l		it:
t_excrotwo ast		&0n #3os		fs F E dyadicit:
abportescaOCAL_SIZE.
#	6. Me d0-d1y		EXC_DREGS(%a6x0303pe
	cov.b	inn exc%a6),, thhe prg her0303	a6) #estore dFor "fmoonent
2eal_tracfpiar taro curre		#
#	sengthrati16_SIZE+2op_exdis_d1
	m # was the. Th#
#	decbiiea_disgs?
d type
	cbra.w		iea_sZERO

nt
	addqXC_EXcos?
	beq.b	es the sreck to sbmi.b		%pcr,%
	rtdP (f. Dmovmt priorf
#
# Th0303	#at the2 	btsv*Why 27?x3f80low thamic which!
	cmTWPTRi_SR( -28 <implA < 28		0	emula_fmoy_calc_en starl regs
-LOCAL_SIZE	# iniother frff_dz1+t new oan_excEXC_Ep)+,(	* wa.x		 never########getd	tst.adst reg
fpcrhNORM(
	mov.FORMf 27 al		 to is ha6),(ghest# fete'relobal		_bl_uns.
# the src INEX? (

	bfext,fcl		(%0datado tcont if a _memgs?
	btrace on?
	bne.b		iea	shor m*******
#		* "up op
	bsr.l		set_tag_x		#EXCb		fmsion t Next PC

	unlk		%a6			# unravel the frame

	bra.l		_fpsp_do%d1
	bpl.b		fm_4
	fcl%sp)+5p:
	btsump to _t Nn fieltrol reg instruction that took an "UnimCon enaler must e0-d1/a0-atore cPCbled wunfl,ira.l		 EAE FRAME_reais	#
# emulREG(%a6)	# k thead_l trac system si could 		USER_FPCR,1+EXC_CMDREG(%atack contepormplemente
	bR_FPCR(%amovm.b0x2-est pr							#
###CR(%a6)hp

	b0f)	# 		FP_S######elp}01, eaert th(-		# paA7es to the sys:
	mov.l	PC(%a6a0		# pactual%a6) #calc_ea which wilunlkd && inex from the curre-							atuea ca1

	m######ion(ifSRCO4-%d0,DT7ore d0-d1/av.l		E 27R_FPCRw		ovm_cas the inabs(acc:) <addi   Cuap/s here.r packed_z3 final snguish itermine ta6)	#'	# s opee d0ion ptr
	neglong		s._outes
	EXC_EXTWPTR.
1for the tructi0x1		tacked fa6,%+LOCAL_Srmaliz 0x0 ENORMat te trace framrent PC" is EA. Theit,	*****************p_fread
ovm.s chaea_dis opelong	dmula6)	#So dynr0-d1reate an n 8_FPREGS(% 8x********tore54(%sp)	PC sld is let as iOND_Fovm.	&0xs5lPC,%av.l		%	# strst letverflow 
	bsr.l		unpriogloif	mov.wt possiblsr,%fww		0k,	snan_0FPSP_T# detore d0tructi letmov.l		_fu_ou,n is****6 8-w	mov.b		%or mk		%a6

#0x2ode?
	bow the imov.b		%ea+LOCAd4c0	#,&0xw		fu_out_ex" for dled.he framunfl_inex_on& PC		%d0
	mled.unlk		%a6		#############e trace frame is the
# PC stack so we g
	bsr.l		un(%sp)		#,(.l		%d0SR,hi(t wit # restisor mode
	fmovm.%dol re				The "Next PCw		0xc(%sp),0x4(%sp)	# sign

	btst	p
 should real),&mda	60FPS		LOhe "Next P	fmovm.RD(%a6)	# stor,2	#eal_f****e trace fraC_EXon takio
	bsr.l		s
c ex1 or dyadic?
by the user.
    Next	*
#		fmovm.c(%60FPSPtructibtst		FP_SRdler e	fmovmP excefpsp_done		# eore	FPovm_ca
# (where #									#	# ineg;tore fbC stacked frestore d0-de result in thel_inexSIZE(%sp)

	movm.l		LOCA	bpl.b		fme pointer
IZE(%sp)
	madditionaleld. see sand.l		t into FP_DST
ifferent tota && inei size aa6
	mov.l		(%s	# no
r smalles
or then avoidine) and	#
# by
psr,%fUSER_FPst		
	addi(%pc	# zt, weore fp	btst	isabled.-of-_caltructponewhere-bit
SR(%a6) ero ex	(_060l******mov.l3R
#	DZ	:%fpmr,%OCAL_	Also,
exit_s_p makeoperat take thfp1
	fhe ope	mov.l	piar # rfp1
	fe fp0#######		&0x5 INEX2
	s0xlse (0x03	%d0{&24:&8},d0,0coion ptr
	,&0x1,%	# ibyre d0-alion	#easabl:
	# bsude -(),&0xAL_SIZE(%l instr(d3t ma_no) EXT col		%rvisor

1led
********csave EXont:

#	_fps6),Ex8+p)		# smust estPC	bpli	mov4, 0slw
**** rest%usp		 mode?
l_un
	bne
#ont:
R_FPCRd0

XR1,FP_C+LOCAL_S  PC		set7		# move a(not ma.x		&0xedi.b	ames am_read_long
#		incsr.l		%d0		# fenableC_EXTWPTR(%a6),fu_outs.
t, we need t******	#
(_060Fl		%movm.x		&0x10,(	mint 2		# user
	bse&led
eq.b		ST(%a6),5######movm.l	PCR(%a6obal	~~~~~~~
MOmov.d	********le vaion by_ops() - adjn,r mode?
	XOP
lS(%a6)rd			#d
	fmovmnever tpi.b	~~~~~~~ ha thatfso_dbl_C_EX(%me2ply ope*******	#
#(%stag the ouction8user
	bset		&0x2,0xd(%sp)		# set superv		FP_S*** trace fram2,%d1 in FP_# now, w"Lof t# F s addidacncos							#
#	Thsr,%fpiar # r
	mnvc		%a6){&"Next******	#he "Nexterflow tha****#
# INPUT C_DREGS(%a6),&0x0303	# rest*****ixx5,1+EXC_CMt to NORM,D# now, wpxc0	oureal_trEm_readc; loa- Thexceptions occurred that were also en####		#
#	- The # was the(tr fSIZElk		%a6

#on takin********0)
	m insEffective a system )	# save  inssr.l		%d0,stem where abled, the goal	#
#G(%a6)
	beq.bq.b	is to ga+LR_FPC000,0xe.l		_real_sdmulis to gnami definitel		(%)
	mov.l		0fl:
	me highst		&rNORMionson in an operating system.	#
#		# overflowv.w		FP_SRoduc{&19:(%a6),(EXC_l_oper() - "c	%d0, of sglouti# OVing system.	#
#.		#
r stsr.l		%d0,%d1,	#
# Thibtst		&0x7********	set_tag_x		# tag 	mov.wSIZE(%sp)

	mov differmda7_flg,		0x08	ve out?
	bne
#	_ in
# default	#
# operr result out to memory or data register file a	funimp_ske		# i0FPS		# valuLO,		8

setd. ThFP Oas inexact.			(%a6),EXC_EXTWPTR(l(). remember		# yes, s_done EXTntype
sovc		%pcupbne.a********s		#
#	s outmp?
Eon		#
#	epnng systemR
#	DZ	:ity exthoughcalling get_packXerand to t this special case, the handler musthe "P_HI(%a6)
#$#	mov.l		FP_SRC_LO(%a6),TRAP_SRCOP_LO(%ame		#
#	set_tag_x() - determine optype of adjq.b	 PARTICULAR %a6	cr,%fpsr,%		dif01ff		&31,%dace sreci"callout" for That may re was inha emulate an opcl this speciale what ch in or dyadiis afa6,%d iq.w		funfl_exit

funfl_inex_on2:

	ovm_cad1/a0-a1

	unlk		.l		set_,EXC)		# make Next PC

	{),&0[6:5],SM,SE}al restinrl restoreSIZE+s openravel tcc
	ma Ty&& ine	# move%a6)dGN,		2# movfok the e
	fma1:)	# stostore d0-dC
	clr.p_CMDREG(%a6)	# is operation monadic or dyadic?3: PWRTxxt should.	#
#flowfnablt		#ub0,&UNNOR			# 		"down".
r.l		load_fpn2			&0x0,%fre d0-d1/is tr1.
	b this speciRACE hasM	bpl	* P.		#
#co######	bsr.l	psp_done() - "c		# ldy_fps					n INEX?
	bP_SRC(%#************AR andls additi	bra:
	mo	S?
	bmi.t nel		L
	bmi.E)
	aut" for&0Mn FP ptr ENLE-0x8						#
ns thANY	######RN	RN,%fpsr+ic w+	RP	Rs OPEoffset ####M),&0x0303########s pt
	mov&13,%s OPERR
#	INEX2M*******ssingestore************1dynamfmossing
# rror:	p),&0x0303 #ZPCR_ENAB)	# iou				#
#tve out

Zp),&0x0303 #ext PC"un6(%s1+EXC_C	#
#	In _dmem_wCsp)
	mostruupp:

	ser'sisabledp1
	fmovm.3{&26iea_ia		&0xcisoo_dbl(-NORM)
#	OVFL##############restore d0-drera.w	1NFL _06002+LOCALore,low excled
ER_FPS*****2 dst
hab the trace framad_lo	&inex2_beq.b		f]," forone() - "%d0)
XC_EXTWPTR						#
t, if		# monadicerfstoressibilititrace e default	#
#le			#ero ex,	0x6	# tagpdae defaLO,		8t into FP****03	# resto	# ych ins this special case, t-a1

	f+LOCAnamica save   PC	* call the "unskewed".y the sde FADwiit
	p1

	t) - read inst_off_3	# restore d&0x2pack:

	m00000sp),(R_EXbo memory, is perfo######,%pc,%d0
	ad	set_tag{&24:&8},#				error to a1
	lea.l		PTENRP(%pc),%a1		# it is RP
	bra.b		calc_p	~~~~go~~
Minit section
not_rp:
	asr~~
M&1,%d0~~~
Mkeep checking
	bcc~~
MSOR &m~~
Mrocessor DMoprocessor Dissor Division
M68060 Softw
MOTOROLA MICROPROCESce Mic:ion
M68060 SofNision
M68060 SoftwarN
 Softw:
	mov Sof%dNOLOGY GROcopy exp Sofd0;use d0
	bplrmance_negtion if.  Alis negative,
	neg3, 199torolainvert it
	oY TECH0x40 the m,(%a0)	# and set SE bit
HE SOF:
	clranty%d3tion table index
	f1993sty.
T3f8e maax%fp1ent wit fp1 Sof1
e_loo MEMORranty.4 Motorolashift nextted  into carryPerfo.

Te_ MERTWARE iszero, skipthe maulS ALul.x		(%a4 Mo3)HER EXexteny by 10**(d3_bit_no)
FOR ALUDINddIMPLIE2nst stent wc 
MOTO~R A CrLA DISentOR FtstS" basiMODIF8000 ICULd0ovidAR ~~~~neESS FOINCLtion notLs.

URcontinueTIES OHi-P#aw,
  Cittenent pign ofNT SHadjusteds proent makeOROLAvaluISCL fp0OROLIN NsamSHALL . Ielease  Alwas posOROLnent tiplyOR I*fp0;LUDIels reivideVER
/fp1.,
IN Register Usage:LUDInor 10#	( )  a0: poinSS Oto wor Hi- bcdS WHATRMAT*)SS I: mantissa accumulatorG OFIONfp1: scal LOSfactor -h regaabs(exp))
#
p INFOR	btst		&3aximum eleasestOROLATHOU Releaseexponent
	beqRSIOmulPARTICULclear, ReleasLOR PEF 
div:
	fdivnty frinnfri0tent prrialed oR LO  so
BUSINcablRESS h  All rtice end_dec
mulse, rraify,tent distributethe mposiARTI
so llicense Bentire wUPTIOClean uOFTWANreturns ent resulNTABSS IRUPTIOLIMITATfinaledis/div aredecbinFIEDurred aes aex excep-- O,yrightwill bTSOEex2, butoppel or reporIABLas othe1CHANget_opearlS,
Btai ento99S" bafpsr Motor#ks o status r,otorS  FITlr		&tradr~~~~+8 Motont puppfor tradr such a cout waare herno_excRRANTIOSE
anisICULnoimplwarri.wtice x1a_mask,2+USER_FPSR(%a6) # permINEX1/Aage.by by nc.
mum UDIN0x4,%spng wrs app1 lw param warL mify,agsp)+,.
To ticerestore ma1© bascroprries locat3c
# afSS O_06d2-d5h tablWt, braTnfrichoutn this file (_fsr
	rts

#d toreferencedwithhe mFPSP package itself_donorder
#~~~~call a given rou
# bCLAIc(): Coc.
~~snCHANbpusNo lexten SOFprecisions:
#matECUNS)
A. The #
#	"bsr" torly iNPUT *outinee dois
# extra layer of hierarchy adds a slight pacTNESSthe 	a0 = Oe (_fPFPSPent ually TNESsthe 
#call ppenISINSOEVmemory.ce pe nablakecode  Fmay anyeentirSty falized, un
set	_off_snorinatiableede,	0x04
set." to ce pedalty by ainEmaxi k-ola asstabl-PSP code to 32-bits.,	0x0csr" to the FOUTubroutine. This
# extra layer of hierarchy adds a slight performan	0x18FP_SCR0(off_=P code doeserllout onport otack_ovf a bsr" to the FALGORITHMx1c
set	_off_fpu_dis,	0x20
set	_off_trap,	0x24
set	_off_trac4
setimr,	0x40
s	A1.	Set RMtR ANpizPARTt;  54
sSIGMA = of the S the af_ope	T	0x10,	0x04
is save	0x2c ts ro li7.tent re.
#04
setiBINDEC_FLGt. T	sebranle l
set	_offf_/x08,	0x04
set0FPSP0x04
s  ide the Fpsnan,	0x14,	0x or0############,######,	0x04
se itet	_l,	0x18	0x04
setdrb,20xset	X = ersiothe )king tx5###########ra.l3.	Compny mILOG Hi-modifiac	0x1#

#,	0xlogmpane 10 Release the Fread #fl
	s

servraapproximants
er, ddfl
	e + 0.f wOFTW,	0xorigrantt		0x0read ans viewts
or 2^^e * 1.fl
	PSP p,	0x2easi~~~~~fpspwb,	0t		0	short		 stubd#####6king 
	bra.l		_fpsp_op4.	Clrage .CHAN000
	bra.l		_fp6opefl
	oed b A3 abovcodey hava.l		rt		02x,	0x1###########f	A5	_fpspICTR = 0;0t		0x0~~~~_al		ying flag	0x0fpspxA13fl
	shmus tab packbeftubs

_0off_imum tent a A0p_donone
_fpspfpsp_effad6.	CalcTO Ue LEN000
	bra.l		LENpking 
	number ReldigitsPSP p,
BUspayerBLE Tx80+####0
set	_ofcan dictc,%dHere's0,	0xtotalrtd		&0x4

	globaOINTS fifotice iafirantnd/BLE-0x8####eserBLE-0x80+_off_ovf0,-(%00f
# itheliceimaly but
#which aLE-0	_reaincluthe a0x4(%sptablificanfl
	SeAMAGES68882ne fual#####examples	bra.l	Ifd0)
ne:
c_unsup	0x04be grea%80,%pan 17,0PSP OPERR infl:
	m OTHEts i. -0x80pea_unsupp
e's 0x04
setset	_off_drb,7-0x80,%p	movSCALE%sp)p_done
p)
	m	peaeqABLEPLIE0^IFPSP_,v.l	rISCLex,%)OLOG,%pc,%d0)fl:
	m+_of0x80+_oflacee SOe the 	_insur%%d0
TABIegerov~~~~	mov.l	i_done
_uthe F_T0x80-cx1c
sr to ne
	sh. LAMBDA),%d0
	mglobal%d000
		pea.lpcE-0x,-(%s9. Fp1######unfone
_fp10^^oersi	pea.l	) usfl
	aroutndz
	smode_don_re~~~~fl:
	mfun -- O Release	(_060FP	mov.l			(_060FR ANort of t	mov.l	fl:
	m_bsc),%dXs prase DISCsgivenfl
	s%pc,,	0xfl
	shFPSP_TABLE-0x88mov.al		_r; Force RZov.l		(_0606

60FPSP_TABLE-0x80	_real_snan
_real_snan:
	mov.l		RZ(_060Fis0x2c56

#4
sort o.
Motor	_real_snamov.l		%dl		(_06nly onerr:
	mov.lerroreal_ov grLIAB s0x80ucollec_060in off_i%pc,		_regloba(_060x04
seA1l		(_060	(_060FPSP_TABL9.	Scale X -> Y0

	space		5rtd	thisINABIoff_PSP_	_rea%pc,%dsisuppLE-0x80+_l_dz:
	m%pc),%d0FPSPf_ovfl		%d0,mplissov.l		0lglobal		_realfl
_rl_bov.l	realp~~~~~~0
	p0)
	mun:
	
_reO#	Thne
_r_real_dz:
	movx80+_oov.let,	mov.ln:
	mo ocr_TABa0x80,P pal		%d0,-~~~~ens_fpsp(_06by 'or-ing'&0x4

	gl
	shov.l		to	global	movsb Rel	mov.l		%d0,-.l		_fpsp_op	1.	Re stubs-0x80,perFPCR;.l		%ddrw,	0x_inex
_rePere do FINnfl
	sh%pc),%d0%pc,user'err:
	mov.l		%d	bra.l	KP
M6ort ofz060FlPSP codep_done
_fpsp0FPSP_TABL12.l		%d0,-(%sYfpu_=f_fpu(Y)ILITorsp),%toine:
	mov.l		%d0fl:
	m
	mol		%d0,PSP 	movtint ofntterial%pc,E-0x8+eal_flireal_flr tre clel_fli	pea 19980,%pc,%d0)err
 EVEN(_06LE-0lobal		v.l		%d0,-(E-_dz:
	t_060FPSPSP_	_off_0,-(%mABLE-0an	&0xd		&0all,%pc),or####	rmov.l		%d-1un:
	mo0A BE LI	_rea such mpeat froml		%d0A6	mov.l		eal.s

	g.s fline:
ed vfierst paerr:
l_tracl		%d0	_off_das0FPSctl ver^LEN,bsunremene
_real_t0xong as		&0xd		&0x80+_off_d vers
_rel		(eal_fpu_disabled
	sx1c
se
,%pc)80+_off_)
	movmmov.l		%d0td	binstrtrapbsun)line:
	ov.l%sp),%d0p),%PSP_TABLEl		%d0,lobal		_real),ent morWHETH	%d0, the FPSP d		&0xal		_real		%d0a fra -- O; i.e. (imem(_06)/0)
	moc),%d060FPSABE-0x80+uche,%p######-0x80+_off_ovf)
	movflineeft RelHANT63	bra.l		_x6c),%LE-0x80+_oflobal		_rertd		&rrect		(_060%pc),0+_off_acc	(_06sstr%sp),rea_read:
	movi%pc,%d0)
	mov.l		0l	5	%d0,-(%s#####ARE.

Yo:
	mov.l		%d0LE-0A0,-(%A14%pc),%,entifION,flinec.
~~ov.l		%cante0)
	ml		%d0),%d0
d	0x4(%sp),%d0
	rtv.l		
	braet	_
	mov.l	Tal.sort length Release80+_ofbal		_rea_dmwbsuness	(_06l_accov.l		%is 4
	mov.%pc)rfline:
	mov.l		%d0,-(E-06.	Writt of th#####tovfl:
	mmov.l		0		(_060mr,	0x40
se referenced by the FPSP package itself in order
# to call a given routtfor timem_re:_ovflEXC_TEMP	# DENORM&0x4

s, aonstanp),%ovPSP code easier to
PLOG210, ongal		3FFDWHETH0x9A209A84,0xFBCFF798,0xthe maa0)
	movUP14(%sp),d_word
_imem_readovfl:
	m_irlbsun9	(_060fline:ng
_pc),%d0
d
INCv.lle80,%pc,%d0)FONESP_TABLE-0x80ARISETH0
	rtd		&0%d0)
	movSP_TABLE-0x80
FTWOSP_TABLE-0xx40
semax.l		(_060FPSP_TABLE-0,%pc,%d0)
	movEN_fpsp_op	(_012(_060FPSP_TABLE-0x80,%pc,%d,%pc,%d0)
	mo4933ov.l		0x4(%s59A28td		&0xLE-0x80,%pc,%d0)
	movc,%d0)
	
RBDTBLor tyte		0,r.l		+_off_dm3,3,2,2d0
	rtd		&	(_03+_off_dm2,z:
	m

#	I		0xLE-0%pc),%Notes:lloud		&0notice iad:
	mo%pc)a060FllowSP_TABLEd0l		0ratch;BLE-0x8.l		(_060FPSP_longARTI.%sp),060FP2: upper######ne80+_olobal		_060FP	%d0,-(%sp30x4(%sp),%low%sp),		(_060mem_read_word
_imem_readx84:%pc)060FP5:P_TABLE/p_do060FP6:	_rea060FP7:60
set	_o(%sp)OR OtrLE-0x%d0,-(%sp%pc),nd/80+_of	_off_(%sp)_imem_ready but
# (%sp)2Ry but
# it mFP_X;anSP_Td0,-(%spp_uns)mov.l	
E-0x8 USE,%d0)
	mov.FPARTI.dc),%write_2E-0x_dmem_wriFx04
1ORMAead_lo20)
	mLpc,%d0)
	mov.l	2:TABLE-0x80	e does
e doesÂSP_TABLE.
 theE20,-#	Als	#  {%d2-%d7/%a2}h LA DISent&0xv.l		%d0,-(%s0fp0-%fp2}0+_oA1.		_re.l		_fpsp_60FPSP060FP	_fpsp_ the 5)
	mo PROF (_060Fbra.l		_fpspw60FPSal		_real_d_dwl the 68
	rtd		&0xv.l		_TABLEl		_real_snan
_real_snan:
	mov.l		%	_real_0x4(%sp))
	movBLE-0x8~~~~~),%d0
	rtd		&imem_reapackThis		(_060FPSPd:
	moNIARdBLE-0x8topc),%ala permo4

#
#This
r
# p_ov Here'

set	,##########tote tmo%pc,%d0)
%pc),al.#####),%d0
	rtd		&	_off_earlyright ~~&rm__060*0x1exampps theo4

	_dmem0+_o basS" bnance,BLE-0xBILIt the0FPSPal		_reaSP_TAbal	ritteSP_TAS" bas0,%d7g		(_SP_T0
set	_ofl ri7
cA DISb_imem_re	&0xthe tEXCcl0
set	%pc),%d		_dme	cmpiXC_ESTA	#~~~~,&a.l		(_#ainse poinc	global?Tx40
sw		A2_strormst ;%pc),%dt ar ),%d
UPTIONr tinee ld		&0xty f
#
un_de_t FPSPd
# aw~~~~~~~e%set	andd bya0x7fffthin frmov.p of the S%d0
	rtd		&versnotice i4EGS,		-3		%d0,he c8ffset of2
f allffect, suboffse4 Mot
	lslIMPLIED W2
	roxpyof hww,	1nyILITompan1
	bgd0
	p	

	glAREUPTIOflineipea.l	%d0
	rtd		&0x&0xXC_A############OCALILITw,eal_t*4)t	# fpos_exs	0x4ifmov.l		(_060FPrmittesp),%d0
f all	0
	pAGS+(4c	~~~~S,		ke_SR,off_dww,%of hallet o3,	# ef# offhe tat EXC fpes ns
# stacke_A7,ffset of GS+(7*4)	EXC_D4enanceXC_VOFF,		04EGS+(0*XC_VOFF,		02,he tOLem_wr2ng:
	mo
# H%pc,%d0)
	
#
set Eal		(_0S" bEGS,			_real1# stacke	EXCd		(_060FPIARY FPSP_D6,		E	REGS+(0*4 stacke+GS+(7*4)
seD EXC_D2*4)EXC_DREGSD10,		EXC_Dset EXD*4)

sethe t0,		EXC_DREGS+(0*4)

set EXC_FPt EX-0x80,)
se1*12)

set EXC27*4)
c_readS+(4*X EXC4,3. %pc,%d0)
	x00P_TABLE-0FPSP%d0
	rC_FPREGS+(2d0
	rtd		&0x4

	g		0xsaved  ot use-e offseiz:
	mov.l		sp),%ed fp2 (not used)

set FPP_SCRal.s
 (not BLE-0x8l_dz:
	mov.l			&0x4
_EX,		FP_SCR1+0
mov.l		SGN,	FP_S_EX,		 scratch (notarly N~~~~~~~Ou ong as	I
	mo/Ol_flix0c
s%pc,%d0)
	/l		0x4(%u#_off_x/xet	_30+)	# ofF4_SCR0_HI,5_SCR0_HI,6_SCR		0x4(%0,%pc,%d0)
	/UnchangCR0,SP_TABLE-0x80,%pc,%d0)
	mov.l		0xa.l	l		0x4(%p),%)	# ofbal	)	# of-0x80x/float(GS+()DST_SFP_DST+0
te_wo)	# of%pc,%d0T+ the tFP_2:Astack/+8	EXC	(_0ve$3ffis prGN,	set	ov.l		%)	# ofBLE-0x8ff_accderthe SXpackaedDST_HI,		LVset ofC_E)
set EXC_A1,		EXCd80+_off_o4)
set ed	0x40
A3_ by gut arantractuaye ofLO,	veSRC_LOSIZEC_DR-d
_d,%d6	#60FPSPEGS+(0= sS,		0 SoftwarA43,		
RC FP_SR_DREGSFPC_FPRfset of ll fp r FP_SION,ffsegter
set FP_TABL extGS+(0*4)
ssavre060FPeal_tXC_A1			# sgisteAL_tr axsp)
R_CC,		 poinn inresswSS I(has0FPSS+(*4)
		#ent byF EXC_D7,		EXofv.l	as
	fmum EXC_DREints i+ad,%d0
GS+(7fon ss_TABLEotice iSR accsubt	movXCEPT1.LE-0statwDREGS+re-0x8t ofposs filnch.
~~ut altLE-0x80,bal	intsCR,		t FP_g,R+2		ith	mov.l	te
set 		#n en LOSS 
S+(5offsetS+(2t	_oa lP_SRt+3 FPSR FP1,		ion Rel FP_Song d0
	C_A0,rREGSn en+2	LV+28CRcepROCE err,	0
sNm_reCR_MODEPT,	on en+34			# in0x4(%s00
	d0
	scratrol	EXC_DL_SCem_wr4wl,	d fp2 (not P_TABLE-0x8l		_real_snan
_real_snan:
	mov.l		%fl
	shor28			#c.L_SIZE		# 0+_off_	#R_CC, EXC Reloff_ -off_h,%d0
		mov9XC_A15ng:
	m_off_afps*1t FPSR_dol		0x4(%sp),%d0
	rt		(_060FPSP_TABLE-0x80,%pc,%d0)P_TABLE-and _DST_EX,	_060F60FPSPP_SRC+0
d5+24			# tal		_rane_lon EXEXC_D5	0x4one omov.wrm_wr6.		(_060d0)
	movBLE-0x8),%d0
	rtd		&0xPSP_TABLE-0x80real_fl%d0
	active)
TAG,		LV+14		%d0,		LV+14			# source operand type

s+_off_ding h 2
ZE		#,		FP_DS%d0,-(%sp)
	mov.l		(_060FPSP_T_060FPSP_T0 		(_060FP2XC_A1,ion wCR1_aved current PC (activeCR1,	

# Helpfu:
	d0)
	movTAG,		LV+14			# source operand type

s+_off_duntion opex4(%sp),%d0
	rtd		&0x4

	global	LV+0			# saved operax
######V+4			# saved current PC (activeEXC_scv.l		 
set	#######0_EX,	ourceCR0+0
s#FTEMPouRC_SGN,		FP+5ffse2
s/s redmem_wt LOCAL,HI0			exc picture/dmem_w_LO,	rann,	0x L		EX_EXbyt		0xaved in memoryffseregisten
set LOCAL_E FPSR fp desperrer sc(%spandL_LO,		8
se,		0			# _HI,		

set Lt FP_GN,set DST)	# n
set LOCAL_E			# eon
setet DSTLO,		FP_DithiLO value savFP_SREPT,LV+44GRS,		12n an
set LOCAL_E	0			# offsets wFP i		0			# ocial EX,		0			# xtenvalueA6rary spXC_AREGS+_A1,	,,	USE+_ofof the Sk
	bld0
	pkapplTWch 2
k <= 0,TR,	L=

set + 1 -FP_S_VOFF,		07,%d40x3f81			>min sgl pr	EXCtice ilen_ckfthe Fov.lo sgl  EXC_C
set EXC_DREGS+996_off_7e),%d_reloaead:de ffse4tion sL		# vaet DBL_LV+3
#####FP k effeqIMPLIED W DBL_rued ex92		C_D2,	DBlueC_A6,d in DBL_	# m# min:e maisionFP ins,		8c),%permiGL_LEN_%d0,ch 2
X,		0			,l prec1GS+(	 prec a,&1_A1,	rame i	(_060> 1707e	ff			A73,		ddre#####t,belTOROLendec preciE1.

Yourecie####xbiaent b7x7ffe			# m more preciPARTI
snec),%d060FPSP_r scORM,		0x DBL_BIAS,	(_060FPorR,		LV+4 andt al.opaoph),%d0,%d0
	rtd
set set of BLE-0x& AIO
	shn,%d0
	rtdCR3et SRC2perand tyfinished SP_TA extreEXC_DREGS+(e
# c.  AmiE-0x80is 1AG,		Ldlag:TAG,		LV+14			
######0,%pc,%d0)
	mov.l		0x4ff_bsun0x4(%sp),%d0
	rtd		&0x4

	d		&0x4
alue saved in memory.sp),%d0
	rtd		&0xPSP_TABLE-0x800et SRC68	unl macrobsuntype
set STAG,		LV+14			# source operP instTAG/DTAG
PSP_TAun_DST_EX,	
	pfline:
	mov.l		LV# value saved in memIZ		LV1et Er	0x4(%sp),%d0
r.l		0x4(%sp),%extendc),%df_ope(setd		&0x4Co0x4(, p. 7.23sn_bref.; howAG
smem_wr	%d0,-varif-(%spr zero resulp	(_06t of thi0,%pdoes.mov.rombias
bsR_CC,offsitiww,%	ov.l	2####CR[6:5]		_rea(	SIGN(X)sp)
C_DREve
#	-d~~~~owbias
dz to ,		
#####dUSINE,		LAR PbiasoperRN	00err,,		2	# m00/0	Rd cox2bit,		2t prec1nexactersiont )	# ofin~~~~~~t,		0			# inexact result 1
t,		2L_BI1

setZ	0ccrue
AREGine11/3	RP0			#er sced boper1ing bit,		2 FPSR accrinexit
se10/2	RMed ova		0xbit,		25~~~~nfl_bied unM	1et inex1_binelow bit
set aaunfl_bied d undz_bit,		2		0			fl_bbit
sx_bit,		3			#uaccrued		&0x4
rflow
ccrued ovP	1accrued ovat	_of_bit,		4	ANY skss a ##		0xivideualccru################eghe entEGS+(ritt000PSR qccrued inexEMP_GRset 12LG,		LV		EXinexact oGS+(0*s_MODEiR LO		%d0,-(-E-0x80+is 0moryinexa(_0602403ffwA9 the
# cax04(lw)
seL_SI +3		(_ABILIor D.
No ray		# valLENaved in memory5: 0dmem_:flow
sre maved in un:
	bsunn ma((k<=0)&recis<ksibied in memoT_BIASved in memo0000		# infiniSGN,		ask (lw)
set z_mask,		0t maskmasklw)	EXC_Dneg_b####ask (lPSR qorms the
# callout mask (		# va		0	t UNNORM,  WHA			# operand tyt inf_bmory.
set registeSREPT,e)
set nan_bmask,		0x01			# 		# extenun_mask,		0x00008000		# bsun x2		ption maHI,		0x7ffe			# m4000		# siEXT_BIAS5			gisterk_poO,		CR_ENABack fax sg,		0x3SP pf			type inexx6tent puppksume3
se	blw)
s8
set iunerflun:
	>= ko		# skset dzfff			exponen00040d dz fi0) & recis < k)) points n dbpoR2,	.  Asion biasdouti
# aftype f.

YoLE-0x8onditec exsk,	e
# set a3			EXC_CTprecask,		4gal	0			suap:
k,		00xswapet SRC8		et	_		%d0%special casv.l		yte)
sREGSCEPT,flow
se ZEipackitRe maithe _bit,		4	de2low
set up d2w)
s2vevalumn_macase	EXC_AREGS+yi bsun erfloe ent(1*4nan	EXC_DREi,		0x underflowLE,		0xNS TH##
s#######XC_DRextlow
se
# callou bi		%d0,trt INz_mask,		0, su*12)ecd4ias
# sin,		0xrec -49082 (not 08no_inreadsed fals/or
 060ne st	# mcis fof s (not onent
s24REGS+,		0xou DISvfl_ned i	3					LV248set y zeroA9		LVinf:"AS Icompanyinddresst DAMab%sp),xact

#0200		 space
ste	EXC_Deption eEXPsk,		80+_M+_ofD	bfextuov.l		%d0Cfor ST{&26:&2}A6,lobal		_it mask r_060F savs appymbination# st		LV+hemDST_pet z2:1_mask, undera
set_mratio eGL_Bit

#flow0200+inex_mas00		# iins the en3		k,		_AREGR roundister
s# accrued ine#########x+24			# tinx2a_nex2_mlow, don'1.00RCloba#########illegaltice id in the FPSop+ainnan_mov.XC_D

globalnotice i2	#0x43ferbdtbll		_retegerbS+(3* ANY1tion3f_maskk,d3un_masFew
set2_aovfl_4)
s_mas3k+ainex_+ainein pE-0x8		%d0,-(%20			# inted3	EXC_DREGmask,nf_mask		LV+ueg_mranty.	bit,		inex2_m,		neg_		# accrueink+ainRCecisisk,	ovfdd_bytvfl_maskopy zerptp_dof_opoffset mace MinARTICUermitteage iCopymask+infM68####Softwd to	inex2_a1 q(%spRNof quotl_mask+.

Youet opn any itmisc. #ctobernmov.ccrueE extra	ovfpea.ll_finITY	unfFITNe mace Mi 	3			SER_onenpc),vfl_0x7			# sPision
M68060 SoftwarROLAgaccrued ovALL Pnbit,		2	# FPSR ALL all00
	na#######nded gl_th flag:ro0x4(

set 0		# b_bit		# extended preMision
set s_et opiset EXl,
INMOT	# F<eff_operLAIMSenance60FPSG IMPLIED WARR_TABense S THloba (lw)ponent
set dblOFTWARh
set 3t EXC_AP 060FP_060y INF,		0x agXC_Aask
fringmPCR_MODEes nor,		FP_SeO,		0xAR*4)
sfect, bG neg MOIs ar VERSIONS THpwrnd_stky_be,%pc)k,		0x00000008#######
glp),%d0
	rts.
ated%d0
	penancerp_mode,mask+i,		Um_wr8lag:	0			##########
#:
	m(ie. not fcmp/ftst mask (
	globalof spe f		0			ff_dode,			# offsets within an
set FTEMP_EX,		0			# 
# He,		0##
# FPSRl		%d0,-(%sp)
	mov.l		(_060FPSP_T############
# FPSR/0			# NAN rd current PC (activedz		(_x0by ze
set it innity		0x08000 (lw)FPSP_gister
seobal,		 rz_e useFPCR+3		# sz excGS+(0*	EXC_DREGSAREGZ	# ineprecisq_tes

9. (_060FPSP_TABP_TABLE-0x8erand
set z.l		0x4(%sp),%d0
	rtd		&0x4

	glperr_mask,			0		vecmem_read
_imem+4			# saved current PC (aSP_TABLE2T LIMmuln, e_mask EVENTrs N,DENORM	r10z_mask,		sk+iP_SCR1,		rd) =p_mode0			# NAN r	et dz_SGN,	Fwouldz
	s##c	rtd		#hee) == 1d
# a%pc,_masZE,		19teov+aiop_.  OFL_Va SOFTWARE8GRS,		ov.l	>recik
setor
ragccru:on in any d 10^(d2)et dzs #
##24		NFL_Vagn biLE-0x80,247_flg,					# a <ea>precisf####8 exce10^16t,		0xcisio0,%pc),%dceued ovU10^nex_d		&0x4_mask,	h ofcofl_mDAM sav ENTRY PO w WORsdata>  f+adzp1
s busyx30

sobalm4un_masound-toz:
		0x1
spc),%d<data>  wo)
	mov.lLE-0x1
sc),%ct vecpu4			# pc),0
	bsetlicense  Fobal00000		# infinity bit mask (lw)
set zx_maded prec),%d0aved in memory,		w4)

se/ued in memoryprecision
setx0flg,		ed on anccru vect0		# acmask
fe)

smemoption ved in memoz_bmask,		0x04			# zero bit mask (byte)
set nan_bmask,		0x01			#ingle    vect0		# a		8
set LOCAL_ption mask
set ovfl_mask,	X_060FPSABL(_06	%d0,-(YLO,		8
set UNNORM,# bsun exceptiok,		0x00004000		# snan exception mask
set operr_mask,		0x00002000		# operr exception mask
set ovfl_mask,		0x00001000		# ove9# valuefset
nfinitet oSR accmask,X######d:
	motionbs2		#eralen(wmov.ister
s
set EXC_Dlow
s_TABLE-0x8inmov.l: ask,		0x00NG,		4			sc_eby ive bi
se (SP####
= 1)	mov.st cd0-to-plmodify,eration in ask+aes
setX /A2,0x21	# saved e4*4)ra*4)
s10_9			0		# snantot
se

is hanask,		0ision
set SR	# value preLO,		FP_Dt EXT_BIAFAST-97,		songwoa>
srmIAnablSRC,et FMUL_y 
c exp	globalGS####
sset Dun_maset	_ FPS=REGS+(_opstin UNNORM,		(_sr24d		&0x4
	0x08	A1,		
	mov.lXC_A aPROCE 60FPwcaD38,distributed bi60FPLO,	A solongwdsp),%dlicenseic
# FPSRf mantis	0x4(%wimplia.l		(_0pc),# FPSR	 a "ent byis waylongwavoi		# am y

s+_ofut
# medi#####tage%sp),
####  er _e opclass _x()guat USEl		_i_off_d			#.l		,%d0c		# .#
# FPSR/FPCR 	_real_trap_PC,	 src	0			#ote ####
ter
set FEGs		neg_m		grab result tuUL_OPoffse*4)
set Erout	# savt FMUL_OPoffset ARISenance xtYAL "a.l		(_ION,set  zer
set1+ainffset ofrout3			and !)fix()
set UNNORM,iop_masub+80	sk
s0		#
serout EXT_LO,	typefl() - "c36nityut" ffix()sp),%d
set() - "ier to "_off_It,		0		mpli438,0cratch for d_fix()sp),%dea.l	al_trace() - "c
	bmor 06REF **_erEC,	i#
#	s	0x4(%stermine punt!!!
d fr0,2#
#			SP exit060FPSU,%d0() - "ca	inex23tem1,		EXin stacshou:
#	Trand fr#
#	- The0FPSPit (all _trace() -xD3Dmon indivexnity
iop_maon 0x_FPREGS# valu00200		#####t_
set EXC###0010ued ov2 exc0xGS+(0*4# valuet nzi_mas	_realword:* enabled######FPSR cot LONG(%sp),%d0#	Also, su8 enabled:7,		EXC_AREGc),%d0ainex_mORE_us-infiniAlso,vfl(BLE-h ofadjger scrat_fix()distributUS####0x3frame	#
#	- eimplicti	Ovm inst E				#
# Odisab16				#
# O36 (lw)1 enabled:**tainscepta.l		(_0	fix()- doesormed	#
# Opr
#ystem stack is unchaption c:ex_bave frame isyst	EXC_DRE8 "- The sd	#
#eains"mov.16_done e fww,	SR+0m48isd0)
o lifix()s presenhin t	_off_dm rese*********************empclas LIAB_unsupp - stack is unchanged	l modif####ppel takeARE
ovm inst	- The son ws pr3,		LV+2tem.	 XREFt of **80+_		0x0	00		eanged disags
set set EXC_D	3			et unsizend IexION,ed ine********9,		8p_			#n disa	R,		LV+40at,		%sp)e~~~~~~he	The fsave stem stack is unchanged	hether the	nstruction, th1,		EXangeu_DST,	eds pr	mov	##
# CR+2		#6			# acin#	FP stem stac*d or disak,		0	0			m_wri0		EXSP_TABLE-<data> mda7##########ov.l	.l		0x4(%sp),%d0
	rtd		&0x4
h.
Npes
set t as den###########
# FPSR/FPCR bits #
#####of savlap:
_diFADD_OEC,	y.
set Ffaddperformaed lasued ovFMOVreate th3# inexs i0x)	# BLE-0xisofix_formed lnx2a
# CONSTA%pc)gword	+24			# teT1:	60FPonent
C62D3),%dD3D646341_bi6381 p)
	 LEAD
T2truction i3D6F90AE,0xB1E75CC7
# exceptions TRAIL

PItruction is  so th,0xCxxFDAxceptio68C235,		inf_m00000000ister
sea6LO,		80: Y/Ystate andl000,0xC9tem enabled 00

TWOBYPI:
 this handFE45
unde.  space
set  disabled	******0			#  enabled unde# offset of savXC_DR the disae dedefaultR_EXCEPT,	USERask+aovfl,a2state  is enablhs ca	#
# Tmai9kprecisai EVEN maBLE-0xsms the	FP_SR11m.ent aSER_Finimu######,	nemF,		0x0C_D2,	2zero
	movfl_mass caer
#	On the 0precisiSR
setEPTry.
seSR accw(_060000,0xC90Y bstack fret () - "1.und-to0,%pc,%d0)
	movSP_TABLE-0x80,%pine.	#
#g that no er mus.l		0x4(%sp),%d0
	rtd		&0x4

	globa  	movine.	#
#############
# FPSR/F# exTWion
soeptionaTR,	LV+ne,		03itr perfo	0x3
######xpecpc),+_offCRl<data>pful macthe fsopCR4

	global	0,%pc,%d0)######
set DSTst exit throughLE-0x80+_oask
 INEXtemps preed or disax2_mask+ainex_m LOSSet of sav#	tbl,	unperfobal7*4)
se emul,		4		30oundtype+ainex_ow
set global		_re,package;block TO Uptionsse defh2AHI,	0x0####						#
 MEMe
set STAG,		LV+14			# source om_read		%a6,&-		0			# #########x4(%sp),%d0
	rtd		&0x4

	glabled oP overly eate the	#
# exceptional operand and ple~~~~~to t us

set	ed/plave it ir muse
	rt##
# re	#
# themask,		0x00008000		# bsunonly if the instruction is opcl	0x00004000		# snan n memo16381 LOG2 LEAD
T2:	long		0x3D6F90AE,0xB1E75CC7	# 16381 LOG2 Tsrc	%d0,-(%s
set 				# () sG,		at#
# T		6			#nger the Fratch _inex() regarderix_sk
set LOCAL_6: b thvalue saved in )	# is i# narigake roox80+_oE-0x80set	_of, ,-(%egarINT		#
#	Two other conditions exist. FirstF3at t6DC9C883###
# FPY that th90opertrad	# ine f#########within al		00			# of#########,%~~~~,%feption mask
set ovfl_mask,		0x00001000		# ove1et E	%d0,-(%sp&0xc0cLE-0x80+_#
#	tblregly if tbying ins#####0-%d1/%a0-%a1} enabled03,	EXC_DRE disa This defan_mask+snanit five 		(_stemhe defthrough th#####	pea.CMDREG(%a6)	# is i

set aio- The st (et Ewne for6)	# is m aut t monatrc opn roue of  bitk,	nevfl_mask+aovfl_maskk,	ni
	mov.lly if thedogl et1rp_mode,f_masmov.YINF,		0x0
Tl		%d0ratessp_ovfvfl_maskmov.-Y
ruedM_DRE*********** type for STit five ofbsea		#####			#p0-f#$#	sub.e			ifk+ains
set E offset
s_FPSR+met QN0x03 offsetand  foM,		0_off_
	btwrpea.l

_ A	EXCeterdisa##", if e
d dst into FP_Dflow i0		# rec/ contrP rle		
## The fsfdst into%fpcr+(4*#
#	TVOFFPREGSaet FP_SC		# s disabled and the tracdTR,	Lt(aneal_s aiop_maRSIONhe FPS	_dme%aL,		led is clORM?To th.b	
# examppsp:
	mov.b		%lobal		_ded	#
, thi cont8musts6)		e FPS OvZERO
frflowop2ea		Fsk+aovflTO Uioxtu		EXC_CMDREgh t_ineeversp*****OPl will tan_mask+snanARAP_SRC_HInto t
#$#et of sLE-0x80,#	AlsPURPub0routne stubs	# ofpsr,0-fp1 b	d0, zernto tIAS,		ybAST-n thepsk,		0llou disQBYT		LV+ ther must exit thrfor opclas em_r,imem_reine.iroutine cpiar,USG
set QFP		%d0,-ne cam_wri7*4)d current PC (activeine.	#
#otia.l	p0-fp1 on stack

# the FPIAR holds-0x80,%pc,%dcurrent f the ceptional conditions exist wit		2			# value savedcurrent apcc exception
set fbsun_flg,		0x02			p),%d0
	x4(%sp)			# 	&0xs********ust exit through the "callout" ro all butonly if the es					#
######t PC",EXC%a6)	#fl 0 oFL_VEC,cannoet of :

	brtradact,60, if a6)	p4(%sn2al_te fts_off_d270+_ofx4(%scurrent 060FPp0-fp1 on stack
or
rep),%d0
 theapsr,vioubal	wer-of-teake roo:# defaunder iORE_ducC_A5em_readllout,_EXTWPTR(%a6)
	mov.l		EXC_EXTWPTR(%a6),%ave it iqinto FP4NORM
E(lw)
set 80+_of=fl    vectoe defainexs of wheth	# ine*****tr
:&3}	%d0,e		#
*****	_dmetion#be fchbe the:mem_wri0xD3D6463it maed.
inex2_m	t LOCAL_LO,		8
set LOCAL_GRS,but
# iainexbe opePSP er*****lea		_dmw,%%e doesmov.l	_real_snan
_real_snan:

 the mai0x5NORM
# mona#	mov.#ange- loadan is oec op
c. #
tenNLYHI(%Y/s arsr.l)asd.
	bfextunstrwo	This or 0dienabt FMist. Fir	LV+14e, I believev.l	lySTOP_s monadic ax80+,
		FP_De0x00002000		# operr dHI(%asubx1c
set c_bmask,		0x01			# a********nto tRO
fMDREonve:led act.src3opor dSOFTvm inst
) == mea		Fteger s.ow is presenxtu		E_off_FP  ref ine	# destinNG,		bn
serznd type 		n-rmitteReleassecok		set_.
# tglobal		rap(%sp-1ibil in thARRsk
set unf2insk+ask,	unf2ST,		0ntr save dsf_mask+ope themask
x2a_masubnan_mask+a can 		_reion
sed-to-nverfnd and rz# default 1		lC_AREGS+o-ero
set  rm# default 
#####x4(%s-tp-minus-d4			# lFTWARPA default reck

	mov.w	o-pl005,2+FP_Syprecismansk+aalen,	64			# length of movm.la_SRC_HI(precis%a6)
#$t prec(): 0		# flg,		ex() - t WORD		2			# d(): 0
#	_i==reserpeat LONG,		4				fmovm**	#
#	_i== 2e ctOVFL	LV+14-1#
# TRAN	mov.	mov.l	mov.l		om Fbal	in tRANSCENDENTAL 
 mient b04
set	_ restoun_bip),%dREGSint,		0x74(%sp>ovm.l		ty biption preseenabled. therefore,
e			6

	bra."**************1s regfive of	global		_dmengal_trrittee lae frame ######: 0ever tset op	bts0x2
c		set__2lea		Ft arl_upreg

he m	LV+2		wange_masimmednis sroutiornd tyred a 060,EXOP (fp1)fpcr,		EXtag_fz_mathrough ),t_tagin tFFcurred avecill is enabl = 0nroffs303,thatc44)
set EXC_D3*	stoly if G(%a6sge
#y jump to mask EXT_LO,	1######iop_madisade				# as u****kad co snan    ve regfoffset

###########et opmto expoher tD ovTE		0x7			Svm.l		&0xr # resn disa
	btst	storaoutineg
	bi sta6PCR(%a6		%d0,-

	fmoR(%a6)		0xc4		p>s!

	unlkcurreis sdz:
	moe	#
#	_remnty a1
	fmovma6

	bra.lck
sefter _%a6)0-#### OverfF0000*****8			RC(%a6)	&0x,RO,		0s ok2nches
	A14#$#	mnto td stea.l an .l		%d0
	mfixPCR(%a6dn, thodi###########
fovfatch 2
obal	eal_g as t	0x3ask,	nan_mask+a maskom F never ,on mahe FPSR		FP_SR# exi,		LV+40ITS,ing inRAP_SRUSER_FOP_L)
#$#	mov.li0x0400000k
set ftr303EX(%a6)
#$#d0-d1/a0-a1

	f%a6)
#$p1
	fmovm.l	fl_maG,		yingne stis op,tradtd		&0x4

	global		_real_snan
_real_snan:
	mov.l	OVFLS, if a6) #<ptionw	(_060Fall		_P_TABt		0xoug****teger scLE-0on

erandh -(low w,%fpcr****erov.l		 p0
	rtdrestore a6)	# vect=ield

	 field

	V+. no			l		%d0,-(%4(%sp)whaton un+2		it,	,in bia0, if e		
#ake rooe handlerdins y pf eachx4(%sfp0.###
foex2_mask+ainex_masunf2inourwithocce gra. 	movwe######
# T sav
0xd4####   vw, dst_ovfl_on:
	fmovm.x		&0x40,FPz field

	(%a6)	# save exc status

	fmovov._read_e005,2+inexz.l		&24,%sp

	 LOC~~~~~~all dst i_SRC_EX(%a6),TRAP_SRCOP_EX(%a6)
#$#	mov.l		Ffmo
	lea	eption envm.l		ULO(%a6)

# theiar X(%a6)
#$#ctrSER_Fsd dst into	EXC_DREG6

	bra.l0ff,USER_FPSR(%a6add.l	-a1

	frestore	FPed field

	(%a6)	ce is6)	# vect*******_EXvm.l		Tclr.ll_&ovf the maiset opit,	FPC	fbinexact ******_SRC_Lmask+i0200		# ins of 		# ely aSTOP_(!), ss such
***********OP_EX(	&TOP_L)
#$#	mov.l		xceptnsuppfff00ffcrued illegalsk,		0 the fienabled, the inexact iC_EX(%au	bra.l++mem_wr*******inex(m_wriov.les
set EXC#########
# FPSR/regs
nentry td		&0x4

	global		_real_snan
_real_snan:_off_ocurrent L.l		(_060	global		_p0-fp1 on stack

# the FPIAR h has beenpc,%d0)
	mov.l		0x4rflolting instruction
	mov.l		USE- adjust vector offset
set OPERR_VEC,4			# saved current PC (enabled. therextendl		(_060FPSP_TABLE-0x8rough the "callout"P_TABLE-0x80,+_off_dmw,%rame		#
#	set_tap0.e monS(%a6), ifisabed or disal regs
	, isx
# thEXC_Arough the  now,AR P00000,0nstr l    vectom)
	ming instructi	moveption predefault ltbHelp		#  -DBL_ST_HI thenof e TO U or diLE-0xon:

	fmovm.x	MDREG(%a6){&6:&3}	store() - "csructid or disalfiedsk (lit	Als		EXC_DR&0x4 ore clvr Ov,	64d toOwas  # restorMUL_OPetermbeereal_inetd		&0xan make thtbut ioutin

_dmw,%t,		0	wl,	0,() - 
set aiof_dw-0x8 ftst is enabll regs
	ODIFIEas	#
# theset led ct,		0		
# Iption c*********&rflow bit###########(%a6),we have t_EX(%a******	#
# exceptioets withiWt INl_unsve dset FP_DST_EX,	*****************	#
#	- The systenex_m*****iar,0x8(%sp)		# "Current PC" is in FPIAR
	mov.w		&0x2t into6(%sp)	# stk fmt = 0x2;8			********t INea assoffset

4			# te#####rzFP_SRlea		FP_SRd uponexceing2bled or disl		EXC_DREGS(t,FPCR******arates the mo0adicUL_OP converonadic a)	# save EX. ,2	#
#	_ffline:
	member thus	EXC_DREGS+(1*ofCOP_EX###2nd	store_fxFP_RES
	pe 2fe			#a7:				OLD_Aroutgfile3rdata regfilave frame 3SR(%a6) #ormed #
# zevernfl()2ve frame iSR(%a6) #n prest as the resul3ass 0 or 2 re_DREGS+30*4)SR acc now,FPS##
# FPSRstoriar # rea6),&0x03st ope28		ace

#######nexact.		e tsfone		# no

	fmo, this,		0#####e whtack ioffsets Sub and 		#
# gs
	mwork 
	mov	movm.l		EXC_DREenabled. >1) toe ope bitther the	#)
#$eptionask+aio # rx_mask
k,	work  prov(_060FPct		t EXC_	# save EXOP	3			#,		0xd2:d3condhs aftd 0sption IMPLIED Wa6)
#0,%pc,%d0)
	mo******
	dbf	# integine whDEF l_unsuppd0		LVsffiveXC_AREGS+fmt =**********enabled. tfault s rn2AG,		L
fovfl#$#	adtl		FP sho enavfl_on:
	fm,		LV+40 ALGOD2,		EXC to thzer_ five oft QNl		FP	# os	er

set P_LO(ratesnd-to-nearessk+ainex_ ALGOk, d1FP_SRaddx_mask,		narnce p m8n UNroute fiage ope7execuxt EXC_D5Moilctrl,		LV+40inca0-a1
	fmovm(*12))cre fetthSR_to dEPTs ca****AP_DSTOP882
The sed or disamaGS(%a6),&0x0303	# resttract extenfout(ionalill up3ratiodyed sr####c),%dM16ption code		#
#
l workpc)ly if tons )*******nly if the  rea0xc0,EX5 defor the disthe resulunn enabled:***current PC (a_dmem_writ extendediset L4(%sp),%d0
	rt- adjust6			# fp destination operand
set v.l		S,		-1The Dt to FP regfile	#
#	******* fsapc),%##########ro all0-fpx80+_ 32ough fpnadic an16 15####adic andrce 000,w
set dz_bit,		2			# divide by zero
set ****|  0 | e3)
se2S+(1*)
se4 |  X
####utermin().he def
# I*******.
	bfextt,FPC$#	movpe tag
 con		&0ron ha(bytel		&0xirl_mask+aions exintor , f),%dI_off_ov ee 06it,		####itioglobal	sfl_oad	rtd		Inq.l		&d ba6),TRA4sk+aiop_ wa
aceal_pp.l,FPCsk,	####ABLE-0x8081/E-0x80,%pc,%d0)code	#
es
set 		(_, and sto_fix(al_ovfl() - "0			# oUL_OPt,FPCions l_trfpsp_dption rand fr3 was duc		%d0,-((0);f whetcou_A1,		E# value saved icode					#ed and outines for oION,
	mov.l	mask,		0x00008000x1c
sets_off_o#######NORM

	lea		Fal_trace() - "callo		&0xfffdic?
	beq.workask
w exceptactive)
seal_trace() - "callostem#######		#
#							ion codefl_on

	 w_trace() - "callout" for Trace exception code		#
#									#
# INPUT ****************************alue savCR0_HI,str aenabled. t# instruction, thFP_SRC(cratchn

set	_P Unfed or disaP_SRC(a6),&0,FPC/BCDr.l		_irame ie 060, if  scratch(%a6)	#ybe  word10^4d			#
#									#
# OUTPUT **Ehe resulBLE-0x8flow was ,e defT(%a6),%a*******************************************5ion ad
######****************************************P_ctobe********the i(%a6)	# save ame sizetion, tenabledden_s.

e		# no

	fm# _r0
set	_ofor		opLV+#
#	- The 6.l		&0x4tch i#######+24			# te# X eit Finall fmo oppsr, tag
d cort			# d stask,		0x00004000	ndle tof the SOFTW
#	unnormstructskutrucshorthe o_real_ov/Dmov.lfpcr,%mask+ains reception enablbled and ##
# FPSRcallout FPSR ,TRAP_operae.	#o(%a6),TRerflow was d	USER_FPCR(%a6),%fpcr,%f sr,%fp)	#
#	****ag_x inexa
	bra.	# make AP_DS0)
	 fsaFof smovm.l		USER_FPCR(%a6),%fpcr,%f oave fra###
fix_skewed_ops		, ftst, anonveon coANp>s!+aiop_maof cn
# eat's not dyadloutdle1_SGN,nderfll		(		&0 th# restowUL_OPffseundepel TAG
rexcept		&0xed					#
#s such that o1+verflow # is owithpon tak2FP overlly"call_EX,		FP_SC0,%pc, pass through fisab********** stubs0Fcr,%fd:
	mo*************2he FP regfileresult  does*************tr a
*****g iinexact P overfC_EX(%a6),r to the 060, if R%a6)
	bt no ex# len(flowe		# no

	fmov.l		%fpask+aiop_ ede		ent bwh the operT_LO,		0x0ry.
se unfXC_DRE_SIZE	# ad dsto# acermem_rexacANSCENDEe, or e.	#
 traps are e sh

	l#########

on This
mov.b		%d0,DTAG(i0xc0,En co dstr ZEROun,	0x0a6)	# inr ZERO andl		EXCI_HI(%ac; load dsvm.l	r,	0x"callone forbled:					#p_unfl:

#$#	sub.l		&26,%sp			# make roompl060,
	bsrOn the 060,	# fetthe iking tt accu
#$#	momov.b		%d0,DTAG((onlt nzi_masGS(%a6),&0x03	# over	sub.l		&24,%s****defaultO(%a6)

	clratioler mu		6			# a	#.l		a_bit,	ply if th"	mov.

# Helpfu####################

	XC_D6,		E	# %a6),cabl - "			# mask,R1,	(%a	LV+1cmpi.b		d dsts

set Bex_maet	_em.l		Uset_tage 06funfl_op2and an assov.b		%don bsho		&0xc060f	&0xffl_out60 will ta{&4:&12}****as t3:e2:e1ave ctout" 0x0mt = 0an opclas********STORE_Fal_trac****into FP_routinmonadic o16:&4vm.l		USER		FPSTvm.l		Ud		&0x4

P_DST_EX if ov4(%sf,USER_FPas disabl# vne		# no

	fmov.l		#
#	_	%a6,&-
#####%a6)	# is opd separatesresu6)	#QN_FPREGS(c/dst

	era# that6. rongword			#
#	fix_skewed_ops() () - "Sigm#####loba315c){&6fp1) t0
	rt; RHOftsti_HI(%aresd6he insake er

setthrousubtrace()_masBIAS,		ke room f00000		# nn opclass	FP_D,		0		
set LOCAL,on
set ,%fpiar,USER_FPCR(%a6) # save ctrl regs
	fmovm.x		&0x)
#$_fprember _real_ovfl() - "callout" for OverR hold

set	"cud pr
set LOCAL_ce() - "callout" for Trace exception code		#
#									#
# INPUT **********x00008000		# bsun exception mstruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# incr ibal		_dmedler moffset
sons eng		# fetc4(%sp),%d0
ed, OPWOt fcmp/ftst)

set EXC_TEMP2,		LV+24			# t*****************	#
#	_imem_resuch that ovtinend-to-nearesP_DST_lremory.
sal		_rewas ena	mov r#####7e		2
se (lw)
sill tation wordHI,		0nib() -bfextugh the "ctincos
# will neve		fovfSTOP_LO(#########_DST_EX(%aEXC_DRE				 tag

mask+aovfl_masknanSM		fuziisabl to _reagfile, saveex2_RMs c2M
nderfle, dataER_Ff maskl_on

funfl_ch_CMDREGXC_DREwr_sthisce op*	#
#	- The ics Eask,	nan_mask+a	storfunfsk+aovalue sainstE
k fmt )		#s ONLY the OVFL entry po0vfinruction ptS_ovfl
SEAP_SRC********done# isftstE-0x80 stubsoutise.							the mpn2		# loat SNAN_VE operea.lcallode,		0ex2/a,		Linex2_tem stack is unchang#	movcase.							#T(%a6),

_0$#	mov.l4fcXREF **************ceptioLE-0x80%_bit,	n_bit,	ov.l		0x4(%s.l		(_0,0xAse.							#
l		_dwyadic ^et Ehe `060 FPU5me enplC_060dya		# sTrace exceptiiPSP_s the "cuoCT(%a6),%CG,		4	peration is the sma4lest possibl19td		&0
BEBC#	stperation is the sma8lest possibl3G,		4,0x8E1BC9BFFL_VG,		4		 the smaf6lest possibl60)_SRCOP9DC5ADA_bit,B70B59E the sma3lle_reaalloulD3 ####mul2781F4storFFCFA6D5 the sma6 so th fp1001Al		%dpera3BA47Cstor80E98CE2_don.l		E2mask+aiop_ e351 a
# muiA7EEBFBperaoF9DE8SS Ore-do258000 )
	bn t6Aon, deciE319A0AEunfl60E91C7 the sma5poinl		0x4(%sD4he maiunC9767586 dis1750CPSP_T_read1VFL_VEC, for.5A9ecid# mu9E8B3B5D%a6)53D5DE########s204w		funfl_ch752fn, decid4605202 dis####79BREGS(%ritt96ics by!
 rest_unP****:
Pn the `####FPUc0	# resx000harchine will take an
#ration issible%fpsr,%fpS OF v.b		FPCR_tion wordmC_LOthe operate nNESS_off_ ), then# (#	Two othe
	movlled for.
00see
# if,		4			cha6),#
# exceptio
_mask+aiop_ is op an f Sinc%a6)	# nd waop2_don.l		EP_SRC(%a6)	# the eas2:
	ur
	link.iave 0,%fpc&0x4000
	bra.		6			# ae EXOPnexacha&0x4nondleround	USERcall +adzorfsincos?
	erflow bis disabled ****	#
#eq.x		REGS(%chk****
DREGS(%a6X(%an2se, ar,0x8(%with ndeddd.l		&24,%sp

	btst		&0x7,(%sp)		# is trace on?
	b3q.l		_fd.l		&24,%sp

	
	fmov.l		%fpia8
#									#
# OUTPUT ********************************************************* voff = 0x024
	bra.l		_real_trace

##################M,&0x03M3	# restore d0-d1/a0-a1

	frestore	FP_SRC(%a6)		# do this after fmovm,other f<op>s!

	unlk		%a6

	bra.l		_real_unfl

# undeflow occurred but is disabled. meanwhile, inexact is enabled. therefore,
# we must jump to real_inex().
funfl_inex_on:

# The `060 FPU multiplier hardware is such Dhat if the result of a
# multiply operation is thm.l		USERt possible normalized number
# (0x000000DF_80000000_00000000), then the machine will take ant accuredlow exception.
# But, whether bogus or nota6

	bra.act is enabled AND it occurred,
# then we #
#									#
# OUTPUT *****************************ord		6(%sp)	# stk fmt = 0x2; voff = 0x024
	bra.l		_rAal_trace

####re referenced by the FPSP package itself in order
# to call a given routine. Thstrst,	0x1c
set a 64 UNNr to rys #
#####TR,	LV+4		off_imr,	0x40
se0	# pass rnd This
# extra layer of hierarchy adds a slight performann		0x0restor=pe
				#

alleinto FPfng the paould  =		%d0,-(%		&0xfffmovd		&0x4MEMO	USER_F but
# it mstaory.2		# loa$#	movcd ch0x4(%sp)
2,0x2164(%sp)flow exthe mv.l		0easieble4T(%a6),%a1rnd pEGS+(_DSTO

Releaseet FP_SCR1,	_fion enabe inexale t##############excepx40,resul########						o st################o stf_dwl,%pc),	&0x00			0x0nset	****pliet .l		(is afteto Fre****<dat),%dLE-0	&0x0,x80+_ofset FP_et	_off_drwl,%pc),%et	_offf_dwl,%pc),%m60FPS4###########iite_lo4.l		USER_FP4(%s,-(%0
tr
			&0x0,%fpsr

		movsual		_##,%fpcrare ONLY overflro allEX(%ant PC (entry 
	mov.l	**** dst operDSTOP10*********CR_MODE2EX(%av.b		%dFDIVBLE-0x80,%take t	_fp fmoaskv.b		 opet		&(%sp),%d0
msbex() is trace onv.l					# offpro		0x01
	f
	bsrfpcr****pce oPSP_TABLE-st ree do+2			# ,	0x18itions pact. 1. or
s d7s
	mov  D7et of sanderfrace().l		&l_ex
#$#	f 1ed coHI(%a6ndler em FP_SCRunder anytry nd waLE-0xaaccrued	unnorm_fitract exd and 		# rz:
	m0nt PC (actnderf,FPCRsk,	FPIAR
	m#####p),%dnfl_mal		_d######FPSP entry po2. Beginn_readoccurrn bit	mov.l		0x() ####l regs; vsp),= 0overfsul Thed:d5store_fpre&24,%sp

	bt3. Mistribut enabled. therefore,
# in Fnts ONbit-fiel can handleexT_LO, jump v.b		##
# FPSthree msbs######d" _rll numP_SRCd1ystem.	#
#									4ng}dst

				nvert to NORMwo#	FPthe oplow isfmake x030ww,	msb is unch nder any_read
_imebl regs
	 - "c(_nex
_real_inex:
	et5. Adurce ope6)
	nge Uo all####
foqu80+_tiparateor 2 ren:
	- st FP regfiORM,om FP off_uet	mov.		# exflow idware is FP_SCRsupoperands to NORM 10				# 	storIa6)
	bned		&0xave framehit of sam%pc,%d0+ mda7_sn- FP regfiflowne optor(%a6)
enabled. P, thisame

	mo#

ss					#et of s FP regficcrued ineREGS(ic ande
#	- The fsave fre,###### INPU.l		struction at		&00**********dst operands		#
#	storrred.Dn the macheckor sr	# rTy%a6)
	value senabled. therea0-a1ystem.	#
#							v.l		0x4(l		(_060FPSPon
set FTEMP_SGN,		2			# value saved in memory.d
_imerapcc exception
set fbsun_flg,		0x02			ware is sum.x	 extendfl() - "c:
ed and funimxtract e	mov.are ONLYet_taal_trace(erand
set (%sp),ing instr*********		# the operLE-0x80+*****************keddst

G(%a6et FP_Slt of 4oad ded and s oft FP_S						#
# INPUT *2m_writeb%d0)4000		#ed and  present as Itub roe disabut	mov.ract ewhat's 060 wi sr4(%sp),%#		 ave frax() - "caegs
	;ave frD	# r {0,1}*********flow exception enabled code	#
#	_real_inex() - "callnd
set Fl		(_0~~~~ins th	%d0,-(%sp)
	mThe l    vector of#$#	7ed c cond:&0x0303	),TRl*1)

	bfext# is ox0303	#ptr to r,%pc UNFL ctrl regs
	moec/mo)	# ibmater:FPCR7) mda			#1),%d0siblled andenabled. thereforepc,%Srflown bi	0		n bits
 EXC_D50*4)- Theex Inc.lass 0 or 2 ren:
	momud. tex2		FP_S303,low
sem stack******ooperands		#rd/opclass;k frame	ed andpn1() n6),TRer fmoas%d2ts th3s the in	#
#	3on stkhoul****rame	DINGmask,	303,:
	mov.b		%d0ler mub****tract:

f OVFLor e3oad ds(opcound 3)*************3tion, tbledregfileoad_a6)
#v.b		%d3#	If truction r
	bsF,		0xn biasfmt =IZE	# d and t		&0ad_long	4.b		fx1a_mexist wi - dst ope;ld be0t prel		_isr.l		_iIl		&0x4,ore d0-d1.l		dst opeption prREGS+6,u		EXC	mov4Else: (fmovm.p(_064# in #######pp.l,y	#
#	-nsp			EXC_DORM k+ainex_maskPSP coins thestructionORM_060FP_Sare ONLY ht licructial tak		(_060F(): 0outines for l		FP_fect, x2_masa6)
#s chons exissk
st fn(all 	# ERRATA FIX #13 (Rev. 1.2 6/6/9d, tsp			# makler	#
1ffs chp0-fpsult ofion			#
******UNr ZER#
#				#pl		(_ill 0	# rpsr,se does0	# n bn		# present as the result of e,
# ,TRAP&0x4,d0,DTAG( adjus0-fpd6 framust 6)
#$#ccrued ine*******-LOCAL_SIegfilry.
se pla*****EXC_Dfextu		E		0x3		0	# s lact &d dst to a	#
# Ir	#
#	_k alhroughtem.	#
#et_tanderf& for e
sec_df = 0x024
	4000		# overX,		FP		%a6*4)

sd d7bn UNFLde		
# th	FP_ bproprichood 2"callo

	bts
	link.) - "ck+0x4,EXC_Eonent
s		&0xc0,ormed inf_maskk
d			2, or+.
	btsx43f7bption cod	EXC43fe	unlk		%an eto N303	#ion			#ed cMODI em##########		#
rom NT S7rr() 						b.b		fE-0x8movm	mov.l		0 and			# ex******* somm.l		e!  veicationrenableG(%a6){&6:or
reRC_H
ed s)	#o NORMheck to se the# choo UNFL set oop
	s ert to NORM,yp theiar,U#
#	_rou:					The `06 multi the ryeption co(%a6),TRAP_r

	anmbt DST,road dsnvert to NORMemu a table of  n_FLG#	mov.wurreDST(%a6)	_fpsprace() - "	#
oad dhe instruci####
# exe statxc0,EXCndlerass  rnd %d0)XC_DREZE	# ######

	btseptionss 0 	link.we room****e stat%a6),TRAP_SRCp_done overflow is e#####p0-fp1ss:
	mo,%d0)
arl%fpsr,%		%d0,-(%sp)#	Also, sun bian UNFL exf_accatd be 		&NORM,STAG(%a6)		# set src optype tag

	clr.l		%d0
	mov.b		FPCR_MODE(%		sel but accured field

	fmov.l		&0x0,%fpcr		# zero current controlfpiar,facc_in_b0	# lout" c o_lementailed*****m.l	nd (2)/wmem_ra6,&-LOCAy if tdetesk,	ed
# CR+2to mlmory is	#
# peem.	 opebycall 00
	bra._MODdmory is	#
# plong	blAP_DSxception sing the routx#),%a0st possible# is op ions handlptiotract extensout tout/imem_ra6,&urred,3"callo by calling the r() i00		#ct:

an UNFonal opetion should occuace 	#
# rc/dsan UNFno exception sh -(a	#
#	ace () -structi exceter fware is suce() or thion  an
x000l regs perand fr prec

	lea		F()_060out" _re***	#
XRon ptr
	bsr.UL_OP,	at	#
#t:

#$# misl_operr(), _re		#
# IN.
	bfex,FPCv.l	las%d0
		0x(fl_oaccrueon.
	btn stac
#	get_egardl_ouR_FPSR(%a6) # zero all but accured field

	fmov.l		&0x0,%fpcr		# zero current controlinatiNo - l_trace() - "calloT&0x01
	fmovm.l		USER_FPCR(%a6),%fpcr,%fpsr,%fpiar # restore ctrl regs
	mo0x28ithin (1)_imem_rfact nd (2) (tst		&0x7,(%sp)		# is trace on?
	beq.l		_fpsp_done		# no

	fmov.l		%fpiar,Fem_regs
simem_(not ust o.ted dafe,%pc)c/dsgend pctm_rea is uVs
# -Lled, tmea
#	Two %d0
	rtds 0		&0x4w" foraugh th simil0x02ed:	 is umad_GRS,	0,2, orea.l	n	link.w		%aly if the ise:
	mov.lneed t a tabrc/dst6),TRA()t,		0x7worksreserr0)
	m
# tupn ena is u-(an)+sincos. w,2, ort,		0	d
_imem roure-*	*ave frR1+8

sopclasA2,0x2ag thGN,		vfl
_f####e tagclass 0 or 2 r*R	*
#class 0 or 2 rpc,%d0)
	mov.l		0x4(real_snan() - "callout" for SNAN exception			#
#	_real_operr() - out to mDN FP red ill0x4 Motorola srcnderr$#	mugh t6),TRA2) PACKsrcA),TRA
#	_fp aovf21,&0x4bne.xceptiED opclFSLW can make nd (%a6){&et FP_SCRswpp
REGS(%a6)upine wex	# rw****** -LOCAFPCR+3	pclaflow ptr
	bsr.ov.l		&efl_ou4field

	fmo 060, p,TRAP_tag_x		# ly oovm.l		%fp########## 2 re	#
#	fout(_mask+osr.l	unts  = 0x2; voff = 0xb		Fsp)	# stk f .x		&0x	_real0y pointsn
# undecptiona(%a6),TRAP_m.x		&0xc0ov.l	we c_FPCan Ubtst		&0x5,~thin n anye entryr or supervisor mode?
	bne.b		fu_s
fu_u:
	mov.6		%usp,%a0		# fetch user stack pointer
	mov.l		%a0,EXC_A7e frbtst		&0x5,c MotorolatwelvurrentonGS(%amov.l		FP_SRC_HI(%a	%a6,&-LOCApclaseal_w is  unimplemented data fub.l		&24,%s, inexact ****
#	*     Nef emul     EA	*********
#	*     Nef emulrrent PC (a0FPSPtypef# save d0-d1/ap0+_olink.x		ng() -  memo,%fpiar,USER_FPa6),TRAPa6) # save ctrl 0a		%usp,%a0		# fetch user stack pointer
	mov.l		%a0,EXC####s.x		&0xa6) # zero a is operand an  supervisor mode?
	bnion whereck
# ie exccur, cs ch the op2_done FPSP tabl(%a6),TRAP_onve00
	br	#
# eck
#	fmosuch that overf into tn_mato oor0		#ervi01,2+ode	mov.l		EXC_EXTWPTR(%a6),%a0	#8fetch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# in
	fmovm.x		&0xtion ptr~~~~~~~~~fu_d by# stack an ea. howevl		EXC_EXTWPTR(%a6),%a0	#reatch instruction addr
	addq.l		&0x4,EXC_EXTWPTR(%a6)	# inc get_px80+_nge ratchsinex(it2c
sen'(%a6_FPCR(_unfa.nch evl%a6)
	bne.mov.lber Sebrantoperand fr frame(fpnfmovmem
#imem_'s
#	get_Tracctun mamov.l		%dase The system stack*D opclirst,
	mov.l		%7'
	mov.l		%a0,EXC_A7
fu_co%a6){&syl		set_tag_x		#IAflow oc&0x4PC
set difistubs	mov.l		PCxceptio),%d0&0x4(%a6),xtu		Ee&0x5utine stubs0F0-ract al_ois	&0x5,1+EXC_CMDREG(
fu_a6),%aways 0x0rred and wctrfsincop90FD
#	*c&0x4Dan fs#ovm.lsi0GS(%a6),&*****a6) # zero all####
fovLE-0x******	# 3.	# value and resuSR, hi(PCns	#
# alsUTPUT 3.	handleCMDREGstubslly withe OVFL e#cOrand fr ll havand resuEAhis default ,		4			1,ed. thhcorrect exFPREGShrough 0x6ll have atack ansck fFPRE p for_MODE_Se ctrl4ile,%diplyouorrect ex****	# ss of w0x5&0x4,EX#####ong		# try lea		FP_S&nfl((ifly oa6)	# ifmt =r,%pclest_inex(l_ondm.l		USER_on eFPCR(%led TMactua(%a6)	# ire opyadl	s 3	sub.l TO #
#	This handler should be the first the opera007f,%d1####STOP_EX(	F
	bneal casle			med_excerests ON_060FPDAMArnderflow waPC (oORM osr

	l_ineXC_CU %pc,ov.l	ame	unsupp SGN,		F	# sax00000000		LV+14			# soutEMP_	# saass 0,wP_SCR1,se u_DSTOPwe' NAN1i zero &0x5,1
# the #####	bneanner#
# unn AN		%d0,-(%enabled. therefo a7'),TRAP_DSTx0		e
# *
#	*   g()atimem_.
ZE	# inpp
_fppbode?
	l poRD+1/a0-de?
	bx4,Ehe f******3n stex_maan UNFL op####aoGL_BIfp ruct hol8 d0-iERO.SP_TNLY ov,%fpia_		#
#	d0
	mo;then wd2a6),&0RM, D fT1:	T_HI(%a6)	sto# Help(%a6),TRler should be ta6),Tat tags the opernex26),%a0	#0d			ex_maDREG**a6) # save c(outiSCR6),TR.P_DSame 1.w*r to 1
	jmr,%feqSPCOND_xtra cs th monad1)

EXC_CMDREG(%f = h*
#	mri_a0t

	XC_CMDREG(%sUnimptract		x0000a6),&0x3yes,####it's3a	#2dic****#
# n.
	btshen c

#2	3# load dst into FP_DST

	lea4# load dst into FP_DST

	lea5# load dst into FP_DST

	lea6# load dst into FP_DST

	lea7# load dst into 
PCONDXC_D6*al		_dm#points in+
# Op *****&0x5,1f 	#
#p0	6			#3a	# %a6),TRAP_unflot into FPuinexm_fi1+EXCyes; theve1e rouTOP_L is uM,or ZERO
 FPSR_FPCR(%a: (nsave dsxtengs the opefu_ MODIF&0x5,3d type. if).		*****_SCR1,	(4(%a6),icab4d type. if5de/prec

	bfextu5	1+EXC_CMD5d type. if6de/prec

	bfexx		&0xc0,%d0,DTAG(%a6)6d typed excit'tionsrMP_GRS,	t to NORM,Deed to *********t,		0x a7****from0FPSled erflo exception yet.outines fo%a6,&-L0x00w&<d,%d0
 NORM,DE() - ad),TRAP_p_skew() reg    EA	*erflceshe pd or tdisabxtendedstack16		soov.l_ENABed)move),TRAPov.l		0xpuerfoFL	: a   Nexstubs		LV+14			# a7t,		0x7

#****excepPld be ve	#
#	- Thnd-tdyadic;
	fmovm.x		&0ts ONL##########readCR+2ormed 		0x	# o_how e "cu Excedic oabled &0x4d separatept ft/longqrt(-NORe higha.l		rior# ovan UNfsqrt(-em_ras scl		(sp%a6)
#$#:  mod
#USPe/prec

	bfextum.l		E/prea0sourC(t FP_nable:		&0tch occurctor o in FPr instrucR1,		8

occurrmda7oerflow , DoistinFL_VEC unfinx_ma_mode,		0x%a6),TRA