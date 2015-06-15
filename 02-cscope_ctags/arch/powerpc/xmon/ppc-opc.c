/* ppc-opc.c -- PowerPC opcode list
   Copyright 1994, 1995, 1996, 1997, 1998, 2000, 2001, 2002, 2003, 2004,
   2005 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support

   This file is part of GDB, GAS, and the GNU binutils.

   GDB, GAS, and the GNU binutils are free software; you can redistribute
   them and/or modify them under the terms of the GNU General Public
   License as published by the Free Software Foundation; either version
   2, or (at your option) any later version.

   GDB, GAS, and the GNU binutils are distributed in the hope that they
   will be useful, but WITHOUT ANY WARRANTY; without even the implied
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this file; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include "nonstdio.h"
#include "ppc.h"

#define ATTRIBUTE_UNUSED
#define _(x)	x

/* This file holds the PowerPC opcode table.  The opcode table
   includes almost all of the extended instruction mnemonics.  This
   permits the disassembler to use them, and simplifies the assembler
   logic, at the cost of increasing the table size.  The table is
   strictly constant data, so the compiler should be able to put it in
   the .text section.

   This file also holds the operand table.  All knowledge about
   inserting operands into instructions and vice-versa is kept in this
   file.  */

/* Local insertion and extraction functions.  */

static unsigned long insert_bat (unsigned long, long, int, const char **);
static long extract_bat (unsigned long, int, int *);
static unsigned long insert_bba (unsigned long, long, int, const char **);
static long extract_bba (unsigned long, int, int *);
static unsigned long insert_bd (unsigned long, long, int, const char **);
static long extract_bd (unsigned long, int, int *);
static unsigned long insert_bdm (unsigned long, long, int, const char **);
static long extract_bdm (unsigned long, int, int *);
static unsigned long insert_bdp (unsigned long, long, int, const char **);
static long extract_bdp (unsigned long, int, int *);
static unsigned long insert_bo (unsigned long, long, int, const char **);
static long extract_bo (unsigned long, int, int *);
static unsigned long insert_boe (unsigned long, long, int, const char **);
static long extract_boe (unsigned long, int, int *);
static unsigned long insert_dq (unsigned long, long, int, const char **);
static long extract_dq (unsigned long, int, int *);
static unsigned long insert_ds (unsigned long, long, int, const char **);
static long extract_ds (unsigned long, int, int *);
static unsigned long insert_de (unsigned long, long, int, const char **);
static long extract_de (unsigned long, int, int *);
static unsigned long insert_des (unsigned long, long, int, const char **);
static long extract_des (unsigned long, int, int *);
static unsigned long insert_fxm (unsigned long, long, int, const char **);
static long extract_fxm (unsigned long, int, int *);
static unsigned long insert_li (unsigned long, long, int, const char **);
static long extract_li (unsigned long, int, int *);
static unsigned long insert_mbe (unsigned long, long, int, const char **);
static long extract_mbe (unsigned long, int, int *);
static unsigned long insert_mb6 (unsigned long, long, int, const char **);
static long extract_mb6 (unsigned long, int, int *);
static unsigned long insert_nb (unsigned long, long, int, const char **);
static long extract_nb (unsigned long, int, int *);
static unsigned long insert_nsi (unsigned long, long, int, const char **);
static long extract_nsi (unsigned long, int, int *);
static unsigned long insert_ral (unsigned long, long, int, const char **);
static unsigned long insert_ram (unsigned long, long, int, const char **);
static unsigned long insert_raq (unsigned long, long, int, const char **);
static unsigned long insert_ras (unsigned long, long, int, const char **);
static unsigned long insert_rbs (unsigned long, long, int, const char **);
static long extract_rbs (unsigned long, int, int *);
static unsigned long insert_rsq (unsigned long, long, int, const char **);
static unsigned long insert_rtq (unsigned long, long, int, const char **);
static unsigned long insert_sh6 (unsigned long, long, int, const char **);
static long extract_sh6 (unsigned long, int, int *);
static unsigned long insert_spr (unsigned long, long, int, const char **);
static long extract_spr (unsigned long, int, int *);
static unsigned long insert_sprg (unsigned long, long, int, const char **);
static long extract_sprg (unsigned long, int, int *);
static unsigned long insert_tbr (unsigned long, long, int, const char **);
static long extract_tbr (unsigned long, int, int *);
static unsigned long insert_ev2 (unsigned long, long, int, const char **);
static long extract_ev2 (unsigned long, int, int *);
static unsigned long insert_ev4 (unsigned long, long, int, const char **);
static long extract_ev4 (unsigned long, int, int *);
static unsigned long insert_ev8 (unsigned long, long, int, const char **);
static long extract_ev8 (unsigned long, int, int *);

/* The operands table.

   The fields are bits, shift, insert, extract, flags.

   We used to put parens around the various additions, like the one
   for BA just below.  However, that caused trouble with feeble
   compilers with a limit on depth of a parenthesized expression, like
   (reportedly) the compiler in Microsoft Developer Studio 5.  So we
   omit the parens, since the macros are never used in a context where
   the addition will be ambiguous.  */

const struct powerpc_operand powerpc_operands[] =
{
  /* The zero index is used to indicate the end of the list of
     operands.  */
#define UNUSED 0
  { 0, 0, NULL, NULL, 0 },

  /* The BA field in an XL form instruction.  */
#define BA UNUSED + 1
#define BA_MASK (0x1f << 16)
  { 5, 16, NULL, NULL, PPC_OPERAND_CR },

  /* The BA field in an XL form instruction when it must be the same
     as the BT field in the same instruction.  */
#define BAT BA + 1
  { 5, 16, insert_bat, extract_bat, PPC_OPERAND_FAKE },

  /* The BB field in an XL form instruction.  */
#define BB BAT + 1
#define BB_MASK (0x1f << 11)
  { 5, 11, NULL, NULL, PPC_OPERAND_CR },

  /* The BB field in an XL form instruction when it must be the same
     as the BA field in the same instruction.  */
#define BBA BB + 1
  { 5, 11, insert_bba, extract_bba, PPC_OPERAND_FAKE },

  /* The BD field in a B form instruction.  The lower two bits are
     forced to zero.  */
#define BD BBA + 1
  { 16, 0, insert_bd, extract_bd, PPC_OPERAND_RELATIVE | PPC_OPERAND_SIGNED },

  /* The BD field in a B form instruction when absolute addressing is
     used.  */
#define BDA BD + 1
  { 16, 0, insert_bd, extract_bd, PPC_OPERAND_ABSOLUTE | PPC_OPERAND_SIGNED },

  /* The BD field in a B form instruction when the - modifier is used.
     This sets the y bit of the BO field appropriately.  */
#define BDM BDA + 1
  { 16, 0, insert_bdm, extract_bdm,
      PPC_OPERAND_RELATIVE | PPC_OPERAND_SIGNED },

  /* The BD field in a B form instruction when the - modifier is used
     and absolute address is used.  */
#define BDMA BDM + 1
  { 16, 0, insert_bdm, extract_bdm,
      PPC_OPERAND_ABSOLUTE | PPC_OPERAND_SIGNED },

  /* The BD field in a B form instruction when the + modifier is used.
     This sets the y bit of the BO field appropriately.  */
#define BDP BDMA + 1
  { 16, 0, insert_bdp, extract_bdp,
      PPC_OPERAND_RELATIVE | PPC_OPERAND_SIGNED },

  /* The BD field in a B form instruction when the + modifier is used
     and absolute addressing is used.  */
#define BDPA BDP + 1
  { 16, 0, insert_bdp, extract_bdp,
      PPC_OPERAND_ABSOLUTE | PPC_OPERAND_SIGNED },

  /* The BF field in an X or XL form instruction.  */
#define BF BDPA + 1
  { 3, 23, NULL, NULL, PPC_OPERAND_CR },

  /* An optional BF field.  This is used for comparison instructions,
     in which an omitted BF field is taken as zero.  */
#define OBF BF + 1
  { 3, 23, NULL, NULL, PPC_OPERAND_CR | PPC_OPERAND_OPTIONAL },

  /* The BFA field in an X or XL form instruction.  */
#define BFA OBF + 1
  { 3, 18, NULL, NULL, PPC_OPERAND_CR },

  /* The BI field in a B form or XL form instruction.  */
#define BI BFA + 1
#define BI_MASK (0x1f << 16)
  { 5, 16, NULL, NULL, PPC_OPERAND_CR },

  /* The BO field in a B form instruction.  Certain values are
     illegal.  */
#define BO BI + 1
#define BO_MASK (0x1f << 21)
  { 5, 21, insert_bo, extract_bo, 0 },

  /* The BO field in a B form instruction when the + or - modifier is
     used.  This is like the BO field, but it must be even.  */
#define BOE BO + 1
  { 5, 21, insert_boe, extract_boe, 0 },

#define BH BOE + 1
  { 2, 11, NULL, NULL, PPC_OPERAND_OPTIONAL },

  /* The BT field in an X or XL form instruction.  */
#define BT BH + 1
  { 5, 21, NULL, NULL, PPC_OPERAND_CR },

  /* The condition register number portion of the BI field in a B form
     or XL form instruction.  This is used for the extended
     conditional branch mnemonics, which set the lower two bits of the
     BI field.  This field is optional.  */
#define CR BT + 1
  { 3, 18, NULL, NULL, PPC_OPERAND_CR | PPC_OPERAND_OPTIONAL },

  /* The CRB field in an X form instruction.  */
#define CRB CR + 1
  { 5, 6, NULL, NULL, 0 },

  /* The CRFD field in an X form instruction.  */
#define CRFD CRB + 1
  { 3, 23, NULL, NULL, PPC_OPERAND_CR },

  /* The CRFS field in an X form instruction.  */
#define CRFS CRFD + 1
  { 3, 0, NULL, NULL, PPC_OPERAND_CR },

  /* The CT field in an X form instruction.  */
#define CT CRFS + 1
  { 5, 21, NULL, NULL, PPC_OPERAND_OPTIONAL },

  /* The D field in a D form instruction.  This is a displacement off
     a register, and implies that the next operand is a register in
     parentheses.  */
#define D CT + 1
  { 16, 0, NULL, NULL, PPC_OPERAND_PARENS | PPC_OPERAND_SIGNED },

  /* The DE field in a DE form instruction.  This is like D, but is 12
     bits only.  */
#define DE D + 1
  { 14, 0, insert_de, extract_de, PPC_OPERAND_PARENS },

  /* The DES field in a DES form instruction.  This is like DS, but is 14
     bits only (12 stored.)  */
#define DES DE + 1
  { 14, 0, insert_des, extract_des, PPC_OPERAND_PARENS | PPC_OPERAND_SIGNED },

  /* The DQ field in a DQ form instruction.  This is like D, but the
     lower four bits are forced to zero. */
#define DQ DES + 1
  { 16, 0, insert_dq, extract_dq,
      PPC_OPERAND_PARENS | PPC_OPERAND_SIGNED | PPC_OPERAND_DQ },

  /* The DS field in a DS form instruction.  This is like D, but the
     lower two bits are forced to zero.  */
#define DS DQ + 1
  { 16, 0, insert_ds, extract_ds,
      PPC_OPERAND_PARENS | PPC_OPERAND_SIGNED | PPC_OPERAND_DS },

  /* The E field in a wrteei instruction.  */
#define E DS + 1
  { 1, 15, NULL, NULL, 0 },

  /* The FL1 field in a POWER SC form instruction.  */
#define FL1 E + 1
  { 4, 12, NULL, NULL, 0 },

  /* The FL2 field in a POWER SC form instruction.  */
#define FL2 FL1 + 1
  { 3, 2, NULL, NULL, 0 },

  /* The FLM field in an XFL form instruction.  */
#define FLM FL2 + 1
  { 8, 17, NULL, NULL, 0 },

  /* The FRA field in an X or A form instruction.  */
#define FRA FLM + 1
#define FRA_MASK (0x1f << 16)
  { 5, 16, NULL, NULL, PPC_OPERAND_FPR },

  /* The FRB field in an X or A form instruction.  */
#define FRB FRA + 1
#define FRB_MASK (0x1f << 11)
  { 5, 11, NULL, NULL, PPC_OPERAND_FPR },

  /* The FRC field in an A form instruction.  */
#define FRC FRB + 1
#define FRC_MASK (0x1f << 6)
  { 5, 6, NULL, NULL, PPC_OPERAND_FPR },

  /* The FRS field in an X form instruction or the FRT field in a D, X
     or A form instruction.  */
#define FRS FRC + 1
#define FRT FRS
  { 5, 21, NULL, NULL, PPC_OPERAND_FPR },

  /* The FXM field in an XFX instruction.  */
#define FXM FRS + 1
#define FXM_MASK (0xff << 12)
  { 8, 12, insert_fxm, extract_fxm, 0 },

  /* Power4 version for mfcr.  */
#define FXM4 FXM + 1
  { 8, 12, insert_fxm, extract_fxm, PPC_OPERAND_OPTIONAL },

  /* The L field in a D or X form instruction.  */
#define L FXM4 + 1
  { 1, 21, NULL, NULL, PPC_OPERAND_OPTIONAL },

  /* The LEV field in a POWER SVC form instruction.  */
#define SVC_LEV L + 1
  { 7, 5, NULL, NULL, 0 },

  /* The LEV field in an SC form instruction.  */
#define LEV SVC_LEV + 1
  { 7, 5, NULL, NULL, PPC_OPERAND_OPTIONAL },

  /* The LI field in an I form instruction.  The lower two bits are
     forced to zero.  */
#define LI LEV + 1
  { 26, 0, insert_li, extract_li, PPC_OPERAND_RELATIVE | PPC_OPERAND_SIGNED },

  /* The LI field in an I form instruction when used as an absolute
     address.  */
#define LIA LI + 1
  { 26, 0, insert_li, extract_li, PPC_OPERAND_ABSOLUTE | PPC_OPERAND_SIGNED },

  /* The LS field in an X (sync) form instruction.  */
#define LS LIA + 1
  { 2, 21, NULL, NULL, PPC_OPERAND_OPTIONAL },

  /* The MB field in an M form instruction.  */
#define MB LS + 1
#define MB_MASK (0x1f << 6)
  { 5, 6, NULL, NULL, 0 },

  /* The ME field in an M form instruction.  */
#define ME MB + 1
#define ME_MASK (0x1f << 1)
  { 5, 1, NULL, NULL, 0 },

  /* The MB and ME fields in an M form instruction expressed a single
     operand which is a bitmask indicating which bits to select.  This
     is a two operand form using PPC_OPERAND_NEXT.  See the
     description in opcode/ppc.h for what this means.  */
#define MBE ME + 1
  { 5, 6, NULL, NULL, PPC_OPERAND_OPTIONAL | PPC_OPERAND_NEXT },
  { 32, 0, insert_mbe, extract_mbe, 0 },

  /* The MB or ME field in an MD or MDS form instruction.  The high
     bit is wrapped to the low end.  */
#define MB6 MBE + 2
#define ME6 MB6
#define MB6_MASK (0x3f << 5)
  { 6, 5, insert_mb6, extract_mb6, 0 },

  /* The MO field in an mbar instruction.  */
#define MO MB6 + 1
  { 5, 21, NULL, NULL, PPC_OPERAND_OPTIONAL },

  /* The NB field in an X form instruction.  The value 32 is stored as
     0.  */
#define NB MO + 1
  { 6, 11, insert_nb, extract_nb, 0 },

  /* The NSI field in a D form instruction.  This is the same as the
     SI field, only negated.  */
#define NSI NB + 1
  { 16, 0, insert_nsi, extract_nsi,
      PPC_OPERAND_NEGATIVE | PPC_OPERAND_SIGNED },

  /* The RA field in an D, DS, DQ, X, XO, M, or MDS form instruction.  */
#define RA NSI + 1
#define RA_MASK (0x1f << 16)
  { 5, 16, NULL, NULL, PPC_OPERAND_GPR },

  /* As above, but 0 in the RA field means zero, not r0.  */
#define RA0 RA + 1
  { 5, 16, NULL, NULL, PPC_OPERAND_GPR_0 },

  /* The RA field in the DQ form lq instruction, which has special
     value restrictions.  */
#define RAQ RA0 + 1
  { 5, 16, insert_raq, NULL, PPC_OPERAND_GPR_0 },

  /* The RA field in a D or X form instruction which is an updating
     load, which means that the RA field may not be zero and may not
     equal the RT field.  */
#define RAL RAQ + 1
  { 5, 16, insert_ral, NULL, PPC_OPERAND_GPR_0 },

  /* The RA field in an lmw instruction, which has special value
     restrictions.  */
#define RAM RAL + 1
  { 5, 16, insert_ram, NULL, PPC_OPERAND_GPR_0 },

  /* The RA field in a D or X form instruction which is an updating
     store or an updating floating point load, which means that the RA
     field may not be zero.  */
#define RAS RAM + 1
  { 5, 16, insert_ras, NULL, PPC_OPERAND_GPR_0 },

  /* The RA field of the tlbwe instruction, which is optional.  */
#define RAOPT RAS + 1
  { 5, 16, NULL, NULL, PPC_OPERAND_GPR | PPC_OPERAND_OPTIONAL },

  /* The RB field in an X, XO, M, or MDS form instruction.  */
#define RB RAOPT + 1
#define RB_MASK (0x1f << 11)
  { 5, 11, NULL, NULL, PPC_OPERAND_GPR },

  /* The RB field in an X form instruction when it must be the same as
     the RS field in the instruction.  This is used for extended
     mnemonics like mr.  */
#define RBS RB + 1
  { 5, 1, insert_rbs, extract_rbs, PPC_OPERAND_FAKE },

  /* The RS field in a D, DS, X, XFX, XS, M, MD or MDS form
     instruction or the RT field in a D, DS, X, XFX or XO form
     instruction.  */
#define RS RBS + 1
#define RT RS
#define RT_MASK (0x1f << 21)
  { 5, 21, NULL, NULL, PPC_OPERAND_GPR },

  /* The RS field of the DS form stq instruction, which has special
     value restrictions.  */
#define RSQ RS + 1
  { 5, 21, insert_rsq, NULL, PPC_OPERAND_GPR_0 },

  /* The RT field of the DQ form lq instruction, which has special
     value restrictions.  */
#define RTQ RSQ + 1
  { 5, 21, insert_rtq, NULL, PPC_OPERAND_GPR_0 },

  /* The RS field of the tlbwe instruction, which is optional.  */
#define RSO RTQ + 1
#define RTO RSO
  { 5, 21, NULL, NULL, PPC_OPERAND_GPR | PPC_OPERAND_OPTIONAL },

  /* The SH field in an X or M form instruction.  */
#define SH RSO + 1
#define SH_MASK (0x1f << 11)
  { 5, 11, NULL, NULL, 0 },

  /* The SH field in an MD form instruction.  This is split.  */
#define SH6 SH + 1
#define SH6_MASK ((0x1f << 11) | (1 << 1))
  { 6, 1, insert_sh6, extract_sh6, 0 },

  /* The SH field of the tlbwe instruction, which is optional.  */
#define SHO SH6 + 1
  { 5, 11,NULL, NULL, PPC_OPERAND_OPTIONAL },

  /* The SI field in a D form instruction.  */
#define SI SHO + 1
  { 16, 0, NULL, NULL, PPC_OPERAND_SIGNED },

  /* The SI field in a D form instruction when we accept a wide range
     of positive values.  */
#define SISIGNOPT SI + 1
  { 16, 0, NULL, NULL, PPC_OPERAND_SIGNED | PPC_OPERAND_SIGNOPT },

  /* The SPR field in an XFX form instruction.  This is flipped--the
     lower 5 bits are stored in the upper 5 and vice- versa.  */
#define SPR SISIGNOPT + 1
#define PMR SPR
#define SPR_MASK (0x3ff << 11)
  { 10, 11, insert_spr, extract_spr, 0 },

  /* The BAT index number in an XFX form m[ft]ibat[lu] instruction.  */
#define SPRBAT SPR + 1
#define SPRBAT_MASK (0x3 << 17)
  { 2, 17, NULL, NULL, 0 },

  /* The SPRG register number in an XFX form m[ft]sprg instruction.  */
#define SPRG SPRBAT + 1
  { 5, 16, insert_sprg, extract_sprg, 0 },

  /* The SR field in an X form instruction.  */
#define SR SPRG + 1
  { 4, 16, NULL, NULL, 0 },

  /* The STRM field in an X AltiVec form instruction.  */
#define STRM SR + 1
#define STRM_MASK (0x3 << 21)
  { 2, 21, NULL, NULL, 0 },

  /* The SV field in a POWER SC form instruction.  */
#define SV STRM + 1
  { 14, 2, NULL, NULL, 0 },

  /* The TBR field in an XFX form instruction.  This is like the SPR
     field, but it is optional.  */
#define TBR SV + 1
  { 10, 11, insert_tbr, extract_tbr, PPC_OPERAND_OPTIONAL },

  /* The TO field in a D or X form instruction.  */
#define TO TBR + 1
#define TO_MASK (0x1f << 21)
  { 5, 21, NULL, NULL, 0 },

  /* The U field in an X form instruction.  */
#define U TO + 1
  { 4, 12, NULL, NULL, 0 },

  /* The UI field in a D form instruction.  */
#define UI U + 1
  { 16, 0, NULL, NULL, 0 },

  /* The VA field in a VA, VX or VXR form instruction.  */
#define VA UI + 1
#define VA_MASK	(0x1f << 16)
  { 5, 16, NULL, NULL, PPC_OPERAND_VR },

  /* The VB field in a VA, VX or VXR form instruction.  */
#define VB VA + 1
#define VB_MASK (0x1f << 11)
  { 5, 11, NULL, NULL, PPC_OPERAND_VR },

  /* The VC field in a VA form instruction.  */
#define VC VB + 1
#define VC_MASK (0x1f << 6)
  { 5, 6, NULL, NULL, PPC_OPERAND_VR },

  /* The VD or VS field in a VA, VX, VXR or X form instruction.  */
#define VD VC + 1
#define VS VD
#define VD_MASK (0x1f << 21)
  { 5, 21, NULL, NULL, PPC_OPERAND_VR },

  /* The SIMM field in a VX form instruction.  */
#define SIMM VD + 1
  { 5, 16, NULL, NULL, PPC_OPERAND_SIGNED},

  /* The UIMM field in a VX form instruction.  */
#define UIMM SIMM + 1
  { 5, 16, NULL, NULL, 0 },

  /* The SHB field in a VA form instruction.  */
#define SHB UIMM + 1
  { 4, 6, NULL, NULL, 0 },

  /* The other UIMM field in a EVX form instruction.  */
#define EVUIMM SHB + 1
  { 5, 11, NULL, NULL, 0 },

  /* The other UIMM field in a half word EVX form instruction.  */
#define EVUIMM_2 EVUIMM + 1
  { 32, 11, insert_ev2, extract_ev2, PPC_OPERAND_PARENS },

  /* The other UIMM field in a word EVX form instruction.  */
#define EVUIMM_4 EVUIMM_2 + 1
  { 32, 11, insert_ev4, extract_ev4, PPC_OPERAND_PARENS },

  /* The other UIMM field in a double EVX form instruction.  */
#define EVUIMM_8 EVUIMM_4 + 1
  { 32, 11, insert_ev8, extract_ev8, PPC_OPERAND_PARENS },

  /* The WS field.  */
#define WS EVUIMM_8 + 1
#define WS_MASK (0x7 << 11)
  { 3, 11, NULL, NULL, 0 },

  /* The L field in an mtmsrd or A form instruction.  */
#define MTMSRD_L WS + 1
#define A_L MTMSRD_L
  { 1, 16, NULL, NULL, PPC_OPERAND_OPTIONAL },

  /* The DCM field in a Z form instruction.  */
#define DCM MTMSRD_L + 1
  { 6, 16, NULL, NULL, 0 },

  /* Likewise, the DGM field in a Z form instruction.  */
#define DGM DCM + 1
  { 6, 16, NULL, NULL, 0 },

#define TE DGM + 1
  { 5, 11, NULL, NULL, 0 },

#define RMC TE + 1
  { 2, 21, NULL, NULL, 0 },

#define R RMC + 1
  { 1, 15, NULL, NULL, 0 },

#define SP R + 1
  { 2, 11, NULL, NULL, 0 },

#define S SP + 1
  { 1, 11, NULL, NULL, 0 },

  /* SH field starting at bit position 16.  */
#define SH16 S + 1
  { 6, 10, NULL, NULL, 0 },

  /* The L field in an X form with the RT field fixed instruction.  */
#define XRT_L SH16 + 1
  { 2, 21, NULL, NULL, PPC_OPERAND_OPTIONAL },

  /* The EH field in larx instruction.  */
#define EH XRT_L + 1
  { 1, 0, NULL, NULL, PPC_OPERAND_OPTIONAL },
};

/* The functions used to insert and extract complicated operands.  */

/* The BA field in an XL form instruction when it must be the same as
   the BT field in the same instruction.  This operand is marked FAKE.
   The insertion function just copies the BT field into the BA field,
   and the extraction function just checks that the fields are the
   same.  */

static unsigned long
insert_bat (unsigned long insn,
	    long value ATTRIBUTE_UNUSED,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg ATTRIBUTE_UNUSED)
{
  return insn | (((insn >> 21) & 0x1f) << 16);
}

static long
extract_bat (unsigned long insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid)
{
  if (((insn >> 21) & 0x1f) != ((insn >> 16) & 0x1f))
    *invalid = 1;
  return 0;
}

/* The BB field in an XL form instruction when it must be the same as
   the BA field in the same instruction.  This operand is marked FAKE.
   The insertion function just copies the BA field into the BB field,
   and the extraction function just checks that the fields are the
   same.  */

static unsigned long
insert_bba (unsigned long insn,
	    long value ATTRIBUTE_UNUSED,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg ATTRIBUTE_UNUSED)
{
  return insn | (((insn >> 16) & 0x1f) << 11);
}

static long
extract_bba (unsigned long insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid)
{
  if (((insn >> 16) & 0x1f) != ((insn >> 11) & 0x1f))
    *invalid = 1;
  return 0;
}

/* The BD field in a B form instruction.  The lower two bits are
   forced to zero.  */

static unsigned long
insert_bd (unsigned long insn,
	   long value,
	   int dialect ATTRIBUTE_UNUSED,
	   const char **errmsg ATTRIBUTE_UNUSED)
{
  return insn | (value & 0xfffc);
}

static long
extract_bd (unsigned long insn,
	    int dialect ATTRIBUTE_UNUSED,
	    int *invalid ATTRIBUTE_UNUSED)
{
  return ((insn & 0xfffc) ^ 0x8000) - 0x8000;
}

/* The BD field in a B form instruction when the - modifier is used.
   This modifier means that the branch is not expected to be taken.
   For chips built to versions of the architecture prior to version 2
   (ie. not Power4 compatible), we set the y bit of the BO field to 1
   if the offset is negative.  When extracting, we require that the y
   bit be 1 and that the offset be positive, since if the y bit is 0
   we just want to print the normal form of the instruction.
   Power4 compatible targets use two bits, "a", and "t", instead of
   the "y" bit.  "at" == 00 => no hint, "at" == 01 => unpredictable,
   "at" == 10 => not taken, "at" == 11 => taken.  The "t" bit is 00001
   in BO field, the "a" bit is 00010 for branch on CR(BI) and 01000
   for branch on CTR.  We only handle the taken/not-taken hint here.  */

static unsigned long
insert_bdm (unsigned long insn,
	    long value,
	    int dialect,
	    const char **errmsg ATTRIBUTE_UNUSED)
{
  if ((dialect & PPC_OPCODE_POWER4) == 0)
    {
      if ((value & 0x8000) != 0)
	insn |= 1 << 21;
    }
  else
    {
      if ((insn & (0x14 << 21)) == (0x04 << 21))
	insn |= 0x02 << 21;
      else if ((insn & (0x14 << 21)) == (0x10 << 21))
	insn |= 0x08 << 21;
    }
  return insn | (value & 0xfffc);
}

static long
extract_bdm (unsigned long insn,
	     int dialect,
	     int *invalid)
{
  if ((dialect & PPC_OPCODE_POWER4) == 0)
    {
      if (((insn & (1 << 21)) == 0) != ((insn & (1 << 15)) == 0))
	*invalid = 1;
    }
  else
    {
      if ((insn & (0x17 << 21)) != (0x06 << 21)
	  && (insn & (0x1d << 21)) != (0x18 << 21))
	*invalid = 1;
    }

  return ((insn & 0xfffc) ^ 0x8000) - 0x8000;
}

/* The BD field in a B form instruction when the + modifier is used.
   This is like BDM, above, except that the branch is expected to be
   taken.  */

static unsigned long
insert_bdp (unsigned long insn,
	    long value,
	    int dialect,
	    const char **errmsg ATTRIBUTE_UNUSED)
{
  if ((dialect & PPC_OPCODE_POWER4) == 0)
    {
      if ((value & 0x8000) == 0)
	insn |= 1 << 21;
    }
  else
    {
      if ((insn & (0x14 << 21)) == (0x04 << 21))
	insn |= 0x03 << 21;
      else if ((insn & (0x14 << 21)) == (0x10 << 21))
	insn |= 0x09 << 21;
    }
  return insn | (value & 0xfffc);
}

static long
extract_bdp (unsigned long insn,
	     int dialect,
	     int *invalid)
{
  if ((dialect & PPC_OPCODE_POWER4) == 0)
    {
      if (((insn & (1 << 21)) == 0) == ((insn & (1 << 15)) == 0))
	*invalid = 1;
    }
  else
    {
      if ((insn & (0x17 << 21)) != (0x07 << 21)
	  && (insn & (0x1d << 21)) != (0x19 << 21))
	*invalid = 1;
    }

  return ((insn & 0xfffc) ^ 0x8000) - 0x8000;
}

/* Check for legal values of a BO field.  */

static int
valid_bo (long value, int dialect)
{
  if ((dialect & PPC_OPCODE_POWER4) == 0)
    {
      /* Certain encodings have bits that are required to be zero.
	 These are (z must be zero, y may be anything):
	     001zy
	     011zy
	     1z00y
	     1z01y
	     1z1zz
      */
      switch (value & 0x14)
	{
	default:
	case 0:
	  return 1;
	case 0x4:
	  return (value & 0x2) == 0;
	case 0x10:
	  return (value & 0x8) == 0;
	case 0x14:
	  return value == 0x14;
	}
    }
  else
    {
      /* Certain encodings have bits that are required to be zero.
	 These are (z must be zero, a & t may be anything):
	     0000z
	     0001z
	     0100z
	     0101z
	     001at
	     011at
	     1a00t
	     1a01t
	     1z1zz
      */
      if ((value & 0x14) == 0)
	return (value & 0x1) == 0;
      else if ((value & 0x14) == 0x14)
	return value == 0x14;
      else
	return 1;
    }
}

/* The BO field in a B form instruction.  Warn about attempts to set
   the field to an illegal value.  */

static unsigned long
insert_bo (unsigned long insn,
	   long value,
	   int dialect,
	   const char **errmsg)
{
  if (!valid_bo (value, dialect))
    *errmsg = _("invalid conditional option");
  return insn | ((value & 0x1f) << 21);
}

static long
extract_bo (unsigned long insn,
	    int dialect,
	    int *invalid)
{
  long value;

  value = (insn >> 21) & 0x1f;
  if (!valid_bo (value, dialect))
    *invalid = 1;
  return value;
}

/* The BO field in a B form instruction when the + or - modifier is
   used.  This is like the BO field, but it must be even.  When
   extracting it, we force it to be even.  */

static unsigned long
insert_boe (unsigned long insn,
	    long value,
	    int dialect,
	    const char **errmsg)
{
  if (!valid_bo (value, dialect))
    *errmsg = _("invalid conditional option");
  else if ((value & 1) != 0)
    *errmsg = _("attempt to set y bit when using + or - modifier");

  return insn | ((value & 0x1f) << 21);
}

static long
extract_boe (unsigned long insn,
	     int dialect,
	     int *invalid)
{
  long value;

  value = (insn >> 21) & 0x1f;
  if (!valid_bo (value, dialect))
    *invalid = 1;
  return value & 0x1e;
}

/* The DQ field in a DQ form instruction.  This is like D, but the
   lower four bits are forced to zero. */

static unsigned long
insert_dq (unsigned long insn,
	   long value,
	   int dialect ATTRIBUTE_UNUSED,
	   const char **errmsg)
{
  if ((value & 0xf) != 0)
    *errmsg = _("offset not a multiple of 16");
  return insn | (value & 0xfff0);
}

static long
extract_dq (unsigned long insn,
	    int dialect ATTRIBUTE_UNUSED,
	    int *invalid ATTRIBUTE_UNUSED)
{
  return ((insn & 0xfff0) ^ 0x8000) - 0x8000;
}

static unsigned long
insert_ev2 (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg)
{
  if ((value & 1) != 0)
    *errmsg = _("offset not a multiple of 2");
  if ((value > 62) != 0)
    *errmsg = _("offset greater than 62");
  return insn | ((value & 0x3e) << 10);
}

static long
extract_ev2 (unsigned long insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid ATTRIBUTE_UNUSED)
{
  return (insn >> 10) & 0x3e;
}

static unsigned long
insert_ev4 (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg)
{
  if ((value & 3) != 0)
    *errmsg = _("offset not a multiple of 4");
  if ((value > 124) != 0)
    *errmsg = _("offset greater than 124");
  return insn | ((value & 0x7c) << 9);
}

static long
extract_ev4 (unsigned long insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid ATTRIBUTE_UNUSED)
{
  return (insn >> 9) & 0x7c;
}

static unsigned long
insert_ev8 (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg)
{
  if ((value & 7) != 0)
    *errmsg = _("offset not a multiple of 8");
  if ((value > 248) != 0)
    *errmsg = _("offset greater than 248");
  return insn | ((value & 0xf8) << 8);
}

static long
extract_ev8 (unsigned long insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid ATTRIBUTE_UNUSED)
{
  return (insn >> 8) & 0xf8;
}

/* The DS field in a DS form instruction.  This is like D, but the
   lower two bits are forced to zero.  */

static unsigned long
insert_ds (unsigned long insn,
	   long value,
	   int dialect ATTRIBUTE_UNUSED,
	   const char **errmsg)
{
  if ((value & 3) != 0)
    *errmsg = _("offset not a multiple of 4");
  return insn | (value & 0xfffc);
}

static long
extract_ds (unsigned long insn,
	    int dialect ATTRIBUTE_UNUSED,
	    int *invalid ATTRIBUTE_UNUSED)
{
  return ((insn & 0xfffc) ^ 0x8000) - 0x8000;
}

/* The DE field in a DE form instruction.  */

static unsigned long
insert_de (unsigned long insn,
	   long value,
	   int dialect ATTRIBUTE_UNUSED,
	   const char **errmsg)
{
  if (value > 2047 || value < -2048)
    *errmsg = _("offset not between -2048 and 2047");
  return insn | ((value << 4) & 0xfff0);
}

static long
extract_de (unsigned long insn,
	    int dialect ATTRIBUTE_UNUSED,
	    int *invalid ATTRIBUTE_UNUSED)
{
  return (insn & 0xfff0) >> 4;
}

/* The DES field in a DES form instruction.  */

static unsigned long
insert_des (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg)
{
  if (value > 8191 || value < -8192)
    *errmsg = _("offset not between -8192 and 8191");
  else if ((value & 3) != 0)
    *errmsg = _("offset not a multiple of 4");
  return insn | ((value << 2) & 0xfff0);
}

static long
extract_des (unsigned long insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid ATTRIBUTE_UNUSED)
{
  return (((insn >> 2) & 0x3ffc) ^ 0x2000) - 0x2000;
}

/* FXM mask in mfcr and mtcrf instructions.  */

static unsigned long
insert_fxm (unsigned long insn,
	    long value,
	    int dialect,
	    const char **errmsg)
{
  /* If we're handling the mfocrf and mtocrf insns ensure that exactly
     one bit of the mask field is set.  */
  if ((insn & (1 << 20)) != 0)
    {
      if (value == 0 || (value & -value) != value)
	{
	  *errmsg = _("invalid mask field");
	  value = 0;
	}
    }

  /* If the optional field on mfcr is missing that means we want to use
     the old form of the instruction that moves the whole cr.  In that
     case we'll have VALUE zero.  There doesn't seem to be a way to
     distinguish this from the case where someone writes mfcr %r3,0.  */
  else if (value == 0)
    ;

  /* If only one bit of the FXM field is set, we can use the new form
     of the instruction, which is faster.  Unlike the Power4 branch hint
     encoding, this is not backward compatible.  Do not generate the
     new form unless -mpower4 has been given, or -many and the two
     operand form of mfcr was used.  */
  else if ((value & -value) == value
	   && ((dialect & PPC_OPCODE_POWER4) != 0
	       || ((dialect & PPC_OPCODE_ANY) != 0
		   && (insn & (0x3ff << 1)) == 19 << 1)))
    insn |= 1 << 20;

  /* Any other value on mfcr is an error.  */
  else if ((insn & (0x3ff << 1)) == 19 << 1)
    {
      *errmsg = _("ignoring invalid mfcr mask");
      value = 0;
    }

  return insn | ((value & 0xff) << 12);
}

static long
extract_fxm (unsigned long insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid)
{
  long mask = (insn >> 12) & 0xff;

  /* Is this a Power4 insn?  */
  if ((insn & (1 << 20)) != 0)
    {
      /* Exactly one bit of MASK should be set.  */
      if (mask == 0 || (mask & -mask) != mask)
	*invalid = 1;
    }

  /* Check that non-power4 form of mfcr has a zero MASK.  */
  else if ((insn & (0x3ff << 1)) == 19 << 1)
    {
      if (mask != 0)
	*invalid = 1;
    }

  return mask;
}

/* The LI field in an I form instruction.  The lower two bits are
   forced to zero.  */

static unsigned long
insert_li (unsigned long insn,
	   long value,
	   int dialect ATTRIBUTE_UNUSED,
	   const char **errmsg)
{
  if ((value & 3) != 0)
    *errmsg = _("ignoring least significant bits in branch offset");
  return insn | (value & 0x3fffffc);
}

static long
extract_li (unsigned long insn,
	    int dialect ATTRIBUTE_UNUSED,
	    int *invalid ATTRIBUTE_UNUSED)
{
  return ((insn & 0x3fffffc) ^ 0x2000000) - 0x2000000;
}

/* The MB and ME fields in an M form instruction expressed as a single
   operand which is itself a bitmask.  The extraction function always
   marks it as invalid, since we never want to recognize an
   instruction which uses a field of this type.  */

static unsigned long
insert_mbe (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg)
{
  unsigned long uval, mask;
  int mb, me, mx, count, last;

  uval = value;

  if (uval == 0)
    {
      *errmsg = _("illegal bitmask");
      return insn;
    }

  mb = 0;
  me = 32;
  if ((uval & 1) != 0)
    last = 1;
  else
    last = 0;
  count = 0;

  /* mb: location of last 0->1 transition */
  /* me: location of last 1->0 transition */
  /* count: # transitions */

  for (mx = 0, mask = 1L << 31; mx < 32; ++mx, mask >>= 1)
    {
      if ((uval & mask) && !last)
	{
	  ++count;
	  mb = mx;
	  last = 1;
	}
      else if (!(uval & mask) && last)
	{
	  ++count;
	  me = mx;
	  last = 0;
	}
    }
  if (me == 0)
    me = 32;

  if (count != 2 && (count != 0 || ! last))
    *errmsg = _("illegal bitmask");

  return insn | (mb << 6) | ((me - 1) << 1);
}

static long
extract_mbe (unsigned long insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid)
{
  long ret;
  int mb, me;
  int i;

  *invalid = 1;

  mb = (insn >> 6) & 0x1f;
  me = (insn >> 1) & 0x1f;
  if (mb < me + 1)
    {
      ret = 0;
      for (i = mb; i <= me; i++)
	ret |= 1L << (31 - i);
    }
  else if (mb == me + 1)
    ret = ~0;
  else /* (mb > me + 1) */
    {
      ret = ~0;
      for (i = me + 1; i < mb; i++)
	ret &= ~(1L << (31 - i));
    }
  return ret;
}

/* The MB or ME field in an MD or MDS form instruction.  The high bit
   is wrapped to the low end.  */

static unsigned long
insert_mb6 (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg ATTRIBUTE_UNUSED)
{
  return insn | ((value & 0x1f) << 6) | (value & 0x20);
}

static long
extract_mb6 (unsigned long insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid ATTRIBUTE_UNUSED)
{
  return ((insn >> 6) & 0x1f) | (insn & 0x20);
}

/* The NB field in an X form instruction.  The value 32 is stored as
   0.  */

static unsigned long
insert_nb (unsigned long insn,
	   long value,
	   int dialect ATTRIBUTE_UNUSED,
	   const char **errmsg)
{
  if (value < 0 || value > 32)
    *errmsg = _("value out of range");
  if (value == 32)
    value = 0;
  return insn | ((value & 0x1f) << 11);
}

static long
extract_nb (unsigned long insn,
	    int dialect ATTRIBUTE_UNUSED,
	    int *invalid ATTRIBUTE_UNUSED)
{
  long ret;

  ret = (insn >> 11) & 0x1f;
  if (ret == 0)
    ret = 32;
  return ret;
}

/* The NSI field in a D form instruction.  This is the same as the SI
   field, only negated.  The extraction function always marks it as
   invalid, since we never want to recognize an instruction which uses
   a field of this type.  */

static unsigned long
insert_nsi (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg ATTRIBUTE_UNUSED)
{
  return insn | (-value & 0xffff);
}

static long
extract_nsi (unsigned long insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid)
{
  *invalid = 1;
  return -(((insn & 0xffff) ^ 0x8000) - 0x8000);
}

/* The RA field in a D or X form instruction which is an updating
   load, which means that the RA field may not be zero and may not
   equal the RT field.  */

static unsigned long
insert_ral (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg)
{
  if (value == 0
      || (unsigned long) value == ((insn >> 21) & 0x1f))
    *errmsg = "invalid register operand when updating";
  return insn | ((value & 0x1f) << 16);
}

/* The RA field in an lmw instruction, which has special value
   restrictions.  */

static unsigned long
insert_ram (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg)
{
  if ((unsigned long) value >= ((insn >> 21) & 0x1f))
    *errmsg = _("index register in load range");
  return insn | ((value & 0x1f) << 16);
}

/* The RA field in the DQ form lq instruction, which has special
   value restrictions.  */

static unsigned long
insert_raq (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg)
{
  long rtvalue = (insn & RT_MASK) >> 21;

  if (value == rtvalue)
    *errmsg = _("source and target register operands must be different");
  return insn | ((value & 0x1f) << 16);
}

/* The RA field in a D or X form instruction which is an updating
   store or an updating floating point load, which means that the RA
   field may not be zero.  */

static unsigned long
insert_ras (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg)
{
  if (value == 0)
    *errmsg = _("invalid register operand when updating");
  return insn | ((value & 0x1f) << 16);
}

/* The RB field in an X form instruction when it must be the same as
   the RS field in the instruction.  This is used for extended
   mnemonics like mr.  This operand is marked FAKE.  The insertion
   function just copies the BT field into the BA field, and the
   extraction function just checks that the fields are the same.  */

static unsigned long
insert_rbs (unsigned long insn,
	    long value ATTRIBUTE_UNUSED,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg ATTRIBUTE_UNUSED)
{
  return insn | (((insn >> 21) & 0x1f) << 11);
}

static long
extract_rbs (unsigned long insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid)
{
  if (((insn >> 21) & 0x1f) != ((insn >> 11) & 0x1f))
    *invalid = 1;
  return 0;
}

/* The RT field of the DQ form lq instruction, which has special
   value restrictions.  */

static unsigned long
insert_rtq (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg)
{
  if ((value & 1) != 0)
    *errmsg = _("target register operand must be even");
  return insn | ((value & 0x1f) << 21);
}

/* The RS field of the DS form stq instruction, which has special
   value restrictions.  */

static unsigned long
insert_rsq (unsigned long insn,
	    long value ATTRIBUTE_UNUSED,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg)
{
  if ((value & 1) != 0)
    *errmsg = _("source register operand must be even");
  return insn | ((value & 0x1f) << 21);
}

/* The SH field in an MD form instruction.  This is split.  */

static unsigned long
insert_sh6 (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg ATTRIBUTE_UNUSED)
{
  return insn | ((value & 0x1f) << 11) | ((value & 0x20) >> 4);
}

static long
extract_sh6 (unsigned long insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid ATTRIBUTE_UNUSED)
{
  return ((insn >> 11) & 0x1f) | ((insn << 4) & 0x20);
}

/* The SPR field in an XFX form instruction.  This is flipped--the
   lower 5 bits are stored in the upper 5 and vice- versa.  */

static unsigned long
insert_spr (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg ATTRIBUTE_UNUSED)
{
  return insn | ((value & 0x1f) << 16) | ((value & 0x3e0) << 6);
}

static long
extract_spr (unsigned long insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid ATTRIBUTE_UNUSED)
{
  return ((insn >> 16) & 0x1f) | ((insn >> 6) & 0x3e0);
}

/* Some dialects have 8 SPRG registers instead of the standard 4.  */

static unsigned long
insert_sprg (unsigned long insn,
	     long value,
	     int dialect,
	     const char **errmsg)
{
  /* This check uses PPC_OPCODE_403 because PPC405 is later defined
     as a synonym.  If ever a 405 specific dialect is added this
     check should use that instead.  */
  if (value > 7
      || (value > 3
	  && (dialect & (PPC_OPCODE_BOOKE | PPC_OPCODE_403)) == 0))
    *errmsg = _("invalid sprg number");

  /* If this is mfsprg4..7 then use spr 260..263 which can be read in
     user mode.  Anything else must use spr 272..279.  */
  if (value <= 3 || (insn & 0x100) != 0)
    value |= 0x10;

  return insn | ((value & 0x17) << 16);
}

static long
extract_sprg (unsigned long insn,
	      int dialect,
	      int *invalid)
{
  unsigned long val = (insn >> 16) & 0x1f;

  /* mfsprg can use 260..263 and 272..279.  mtsprg only uses spr 272..279
     If not BOOKE or 405, then both use only 272..275.  */
  if (val <= 3
      || (val < 0x10 && (insn & 0x100) != 0)
      || (val - 0x10 > 3
	  && (dialect & (PPC_OPCODE_BOOKE | PPC_OPCODE_403)) == 0))
    *invalid = 1;
  return val & 7;
}

/* The TBR field in an XFX instruction.  This is just like SPR, but it
   is optional.  When TBR is omitted, it must be inserted as 268 (the
   magic number of the TB register).  These functions treat 0
   (indicating an omitted optional operand) as 268.  This means that
   ``mftb 4,0'' is not handled correctly.  This does not matter very
   much, since the architecture manual does not define mftb as
   accepting any values other than 268 or 269.  */

#define TB (268)

static unsigned long
insert_tbr (unsigned long insn,
	    long value,
	    int dialect ATTRIBUTE_UNUSED,
	    const char **errmsg ATTRIBUTE_UNUSED)
{
  if (value == 0)
    value = TB;
  return insn | ((value & 0x1f) << 16) | ((value & 0x3e0) << 6);
}

static long
extract_tbr (unsigned long insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid ATTRIBUTE_UNUSED)
{
  long ret;

  ret = ((insn >> 16) & 0x1f) | ((insn >> 6) & 0x3e0);
  if (ret == TB)
    ret = 0;
  return ret;
}

/* Macros used to form opcodes.  */

/* The main opcode.  */
#define OP(x) ((((unsigned long)(x)) & 0x3f) << 26)
#define OP_MASK OP (0x3f)

/* The main opcode combined with a trap code in the TO field of a D
   form instruction.  Used for extended mnemonics for the trap
   instructions.  */
#define OPTO(x,to) (OP (x) | ((((unsigned long)(to)) & 0x1f) << 21))
#define OPTO_MASK (OP_MASK | TO_MASK)

/* The main opcode combined with a comparison size bit in the L field
   of a D form or X form instruction.  Used for extended mnemonics for
   the comparison instructions.  */
#define OPL(x,l) (OP (x) | ((((unsigned long)(l)) & 1) << 21))
#define OPL_MASK OPL (0x3f,1)

/* An A form instruction.  */
#define A(op, xop, rc) (OP (op) | ((((unsigned long)(xop)) & 0x1f) << 1) | (((unsigned long)(rc)) & 1))
#define A_MASK A (0x3f, 0x1f, 1)

/* An A_MASK with the FRB field fixed.  */
#define AFRB_MASK (A_MASK | FRB_MASK)

/* An A_MASK with the FRC field fixed.  */
#define AFRC_MASK (A_MASK | FRC_MASK)

/* An A_MASK with the FRA and FRC fields fixed.  */
#define AFRAFRC_MASK (A_MASK | FRA_MASK | FRC_MASK)

/* An AFRAFRC_MASK, but with L bit clear.  */
#define AFRALFRC_MASK (AFRAFRC_MASK & ~((unsigned long) 1 << 16))

/* A B form instruction.  */
#define B(op, aa, lk) (OP (op) | ((((unsigned long)(aa)) & 1) << 1) | ((lk) & 1))
#define B_MASK B (0x3f, 1, 1)

/* A B form instruction setting the BO field.  */
#define BBO(op, bo, aa, lk) (B ((op), (aa), (lk)) | ((((unsigned long)(bo)) & 0x1f) << 21))
#define BBO_MASK BBO (0x3f, 0x1f, 1, 1)

/* A BBO_MASK with the y bit of the BO field removed.  This permits
   matching a conditional branch regardless of the setting of the y
   bit.  Similarly for the 'at' bits used for power4 branch hints.  */
#define Y_MASK   (((unsigned long) 1) << 21)
#define AT1_MASK (((unsigned long) 3) << 21)
#define AT2_MASK (((unsigned long) 9) << 21)
#define BBOY_MASK  (BBO_MASK &~ Y_MASK)
#define BBOAT_MASK (BBO_MASK &~ AT1_MASK)

/* A B form instruction setting the BO field and the condition bits of
   the BI field.  */
#define BBOCB(op, bo, cb, aa, lk) \
  (BBO ((op), (bo), (aa), (lk)) | ((((unsigned long)(cb)) & 0x3) << 16))
#define BBOCB_MASK BBOCB (0x3f, 0x1f, 0x3, 1, 1)

/* A BBOCB_MASK with the y bit of the BO field removed.  */
#define BBOYCB_MASK (BBOCB_MASK &~ Y_MASK)
#define BBOATCB_MASK (BBOCB_MASK &~ AT1_MASK)
#define BBOAT2CB_MASK (BBOCB_MASK &~ AT2_MASK)

/* A BBOYCB_MASK in which the BI field is fixed.  */
#define BBOYBI_MASK (BBOYCB_MASK | BI_MASK)
#define BBOATBI_MASK (BBOAT2CB_MASK | BI_MASK)

/* An Context form instruction.  */
#define CTX(op, xop)   (OP (op) | (((unsigned long)(xop)) & 0x7))
#define CTX_MASK CTX(0x3f, 0x7)

/* An User Context form instruction.  */
#define UCTX(op, xop)  (OP (op) | (((unsigned long)(xop)) & 0x1f))
#define UCTX_MASK UCTX(0x3f, 0x1f)

/* The main opcode mask with the RA field clear.  */
#define DRA_MASK (OP_MASK | RA_MASK)

/* A DS form instruction.  */
#define DSO(op, xop) (OP (op) | ((xop) & 0x3))
#define DS_MASK DSO (0x3f, 3)

/* A DE form instruction.  */
#define DEO(op, xop) (OP (op) | ((xop) & 0xf))
#define DE_MASK DEO (0x3e, 0xf)

/* An EVSEL form instruction.  */
#define EVSEL(op, xop) (OP (op) | (((unsigned long)(xop)) & 0xff) << 3)
#define EVSEL_MASK EVSEL(0x3f, 0xff)

/* An M form instruction.  */
#define M(op, rc) (OP (op) | ((rc) & 1))
#define M_MASK M (0x3f, 1)

/* An M form instruction with the ME field specified.  */
#define MME(op, me, rc) (M ((op), (rc)) | ((((unsigned long)(me)) & 0x1f) << 1))

/* An M_MASK with the MB and ME fields fixed.  */
#define MMBME_MASK (M_MASK | MB_MASK | ME_MASK)

/* An M_MASK with the SH and ME fields fixed.  */
#define MSHME_MASK (M_MASK | SH_MASK | ME_MASK)

/* An MD form instruction.  */
#define MD(op, xop, rc) (OP (op) | ((((unsigned long)(xop)) & 0x7) << 2) | ((rc) & 1))
#define MD_MASK MD (0x3f, 0x7, 1)

/* An MD_MASK with the MB field fixed.  */
#define MDMB_MASK (MD_MASK | MB6_MASK)

/* An MD_MASK with the SH field fixed.  */
#define MDSH_MASK (MD_MASK | SH6_MASK)

/* An MDS form instruction.  */
#define MDS(op, xop, rc) (OP (op) | ((((unsigned long)(xop)) & 0xf) << 1) | ((rc) & 1))
#define MDS_MASK MDS (0x3f, 0xf, 1)

/* An MDS_MASK with the MB field fixed.  */
#define MDSMB_MASK (MDS_MASK | MB6_MASK)

/* An SC form instruction.  */
#define SC(op, sa, lk) (OP (op) | ((((unsigned long)(sa)) & 1) << 1) | ((lk) & 1))
#define SC_MASK (OP_MASK | (((unsigned long)0x3ff) << 16) | (((unsigned long)1) << 1) | 1)

/* An VX form instruction.  */
#define VX(op, xop) (OP (op) | (((unsigned long)(xop)) & 0x7ff))

/* The mask for an VX form instruction.  */
#define VX_MASK	VX(0x3f, 0x7ff)

/* An VA form instruction.  */
#define VXA(op, xop) (OP (op) | (((unsigned long)(xop)) & 0x03f))

/* The mask for an VA form instruction.  */
#define VXA_MASK VXA(0x3f, 0x3f)

/* An VXR form instruction.  */
#define VXR(op, xop, rc) (OP (op) | (((rc) & 1) << 10) | (((unsigned long)(xop)) & 0x3ff))

/* The mask for a VXR form instruction.  */
#define VXR_MASK VXR(0x3f, 0x3ff, 1)

/* An X form instruction.  */
#define X(op, xop) (OP (op) | ((((unsigned long)(xop)) & 0x3ff) << 1))

/* A Z form instruction.  */
#define Z(op, xop) (OP (op) | ((((unsigned long)(xop)) & 0x1ff) << 1))

/* An X form instruction with the RC bit specified.  */
#define XRC(op, xop, rc) (X ((op), (xop)) | ((rc) & 1))

/* A Z form instruction with the RC bit specified.  */
#define ZRC(op, xop, rc) (Z ((op), (xop)) | ((rc) & 1))

/* The mask for an X form instruction.  */
#define X_MASK XRC (0x3f, 0x3ff, 1)

/* The mask for a Z form instruction.  */
#define Z_MASK ZRC (0x3f, 0x1ff, 1)

/* An X_MASK with the RA field fixed.  */
#define XRA_MASK (X_MASK | RA_MASK)

/* An X_MASK with the RB field fixed.  */
#define XRB_MASK (X_MASK | RB_MASK)

/* An X_MASK with the RT field fixed.  */
#define XRT_MASK (X_MASK | RT_MASK)

/* An XRT_MASK mask with the L bits clear.  */
#define XLRT_MASK (XRT_MASK & ~((unsigned long) 0x3 << 21))

/* An X_MASK with the RA and RB fields fixed.  */
#define XRARB_MASK (X_MASK | RA_MASK | RB_MASK)

/* An XRARB_MASK, but with the L bit clear.  */
#define XRLARB_MASK (XRARB_MASK & ~((unsigned long) 1 << 16))

/* An X_MASK with the RT and RA fields fixed.  */
#define XRTRA_MASK (X_MASK | RT_MASK | RA_MASK)

/* An XRTRA_MASK, but with L bit clear.  */
#define XRTLRA_MASK (XRTRA_MASK & ~((unsigned long) 1 << 21))

/* An X form instruction with the L bit specified.  */
#define XOPL(op, xop, l) (X ((op), (xop)) | ((((unsigned long)(l)) & 1) << 21))

/* The mask for an X form comparison instruction.  */
#define XCMP_MASK (X_MASK | (((unsigned long)1) << 22))

/* The mask for an X form comparison instruction with the L field
   fixed.  */
#define XCMPL_MASK (XCMP_MASK | (((unsigned long)1) << 21))

/* An X form trap instruction with the TO field specified.  */
#define XTO(op, xop, to) (X ((op), (xop)) | ((((unsigned long)(to)) & 0x1f) << 21))
#define XTO_MASK (X_MASK | TO_MASK)

/* An X form tlb instruction with the SH field specified.  */
#define XTLB(op, xop, sh) (X ((op), (xop)) | ((((unsigned long)(sh)) & 0x1f) << 11))
#define XTLB_MASK (X_MASK | SH_MASK)

/* An X form sync instruction.  */
#define XSYNC(op, xop, l) (X ((op), (xop)) | ((((unsigned long)(l)) & 3) << 21))

/* An X form sync instruction with everything filled in except the LS field.  */
#define XSYNC_MASK (0xff9fffff)

/* An X_MASK, but with the EH bit clear.  */
#define XEH_MASK (X_MASK & ~((unsigned long )1))

/* An X form AltiVec dss instruction.  */
#define XDSS(op, xop, a) (X ((op), (xop)) | ((((unsigned long)(a)) & 1) << 25))
#define XDSS_MASK XDSS(0x3f, 0x3ff, 1)

/* An XFL form instruction.  */
#define XFL(op, xop, rc) (OP (op) | ((((unsigned long)(xop)) & 0x3ff) << 1) | (((unsigned long)(rc)) & 1))
#define XFL_MASK (XFL (0x3f, 0x3ff, 1) | (((unsigned long)1) << 25) | (((unsigned long)1) << 16))

/* An X form isel instruction.  */
#define XISEL(op, xop)  (OP (op) | ((((unsigned long)(xop)) & 0x1f) << 1))
#define XISEL_MASK      XISEL(0x3f, 0x1f)

/* An XL form instruction with the LK field set to 0.  */
#define XL(op, xop) (OP (op) | ((((unsigned long)(xop)) & 0x3ff) << 1))

/* An XL form instruction which uses the LK field.  */
#define XLLK(op, xop, lk) (XL ((op), (xop)) | ((lk) & 1))

/* The mask for an XL form instruction.  */
#define XL_MASK XLLK (0x3f, 0x3ff, 1)

/* An XL form instruction which explicitly sets the BO field.  */
#define XLO(op, bo, xop, lk) \
  (XLLK ((op), (xop), (lk)) | ((((unsigned long)(bo)) & 0x1f) << 21))
#define XLO_MASK (XL_MASK | BO_MASK)

/* An XL form instruction which explicitly sets the y bit of the BO
   field.  */
#define XLYLK(op, xop, y, lk) (XLLK ((op), (xop), (lk)) | ((((unsigned long)(y)) & 1) << 21))
#define XLYLK_MASK (XL_MASK | Y_MASK)

/* An XL form instruction which sets the BO field and the condition
   bits of the BI field.  */
#define XLOCB(op, bo, cb, xop, lk) \
  (XLO ((op), (bo), (xop), (lk)) | ((((unsigned long)(cb)) & 3) << 16))
#define XLOCB_MASK XLOCB (0x3f, 0x1f, 0x3, 0x3ff, 1)

/* An XL_MASK or XLYLK_MASK or XLOCB_MASK with the BB field fixed.  */
#define XLBB_MASK (XL_MASK | BB_MASK)
#define XLYBB_MASK (XLYLK_MASK | BB_MASK)
#define XLBOCBBB_MASK (XLOCB_MASK | BB_MASK)

/* A mask for branch instructions using the BH field.  */
#define XLBH_MASK (XL_MASK | (0x1c << 11))

/* An XL_MASK with the BO and BB fields fixed.  */
#define XLBOBB_MASK (XL_MASK | BO_MASK | BB_MASK)

/* An XL_MASK with the BO, BI and BB fields fixed.  */
#define XLBOBIBB_MASK (XL_MASK | BO_MASK | BI_MASK | BB_MASK)

/* An XO form instruction.  */
#define XO(op, xop, oe, rc) \
  (OP (op) | ((((unsigned long)(xop)) & 0x1ff) << 1) | ((((unsigned long)(oe)) & 1) << 10) | (((unsigned long)(rc)) & 1))
#define XO_MASK XO (0x3f, 0x1ff, 1, 1)

/* An XO_MASK with the RB field fixed.  */
#define XORB_MASK (XO_MASK | RB_MASK)

/* An XS form instruction.  */
#define XS(op, xop, rc) (OP (op) | ((((unsigned long)(xop)) & 0x1ff) << 2) | (((unsigned long)(rc)) & 1))
#define XS_MASK XS (0x3f, 0x1ff, 1)

/* A mask for the FXM version of an XFX form instruction.  */
#define XFXFXM_MASK (X_MASK | (1 << 11) | (1 << 20))

/* An XFX form instruction with the FXM field filled in.  */
#define XFXM(op, xop, fxm, p4) \
  (X ((op), (xop)) | ((((unsigned long)(fxm)) & 0xff) << 12) \
   | ((unsigned long)(p4) << 20))

/* An XFX form instruction with the SPR field filled in.  */
#define XSPR(op, xop, spr) \
  (X ((op), (xop)) | ((((unsigned long)(spr)) & 0x1f) << 16) | ((((unsigned long)(spr)) & 0x3e0) << 6))
#define XSPR_MASK (X_MASK | SPR_MASK)

/* An XFX form instruction with the SPR field filled in except for the
   SPRBAT field.  */
#define XSPRBAT_MASK (XSPR_MASK &~ SPRBAT_MASK)

/* An XFX form instruction with the SPR field filled in except for the
   SPRG field.  */
#define XSPRG_MASK (XSPR_MASK & ~(0x17 << 16))

/* An X form instruction with everything filled in except the E field.  */
#define XE_MASK (0xffff7fff)

/* An X form user context instruction.  */
#define XUC(op, xop)  (OP (op) | (((unsigned long)(xop)) & 0x1f))
#define XUC_MASK      XUC(0x3f, 0x1f)

/* The BO encodings used in extended conditional branch mnemonics.  */
#define BODNZF	(0x0)
#define BODNZFP	(0x1)
#define BODZF	(0x2)
#define BODZFP	(0x3)
#define BODNZT	(0x8)
#define BODNZTP	(0x9)
#define BODZT	(0xa)
#define BODZTP	(0xb)

#define BOF	(0x4)
#define BOFP	(0x5)
#define BOFM4	(0x6)
#define BOFP4	(0x7)
#define BOT	(0xc)
#define BOTP	(0xd)
#define BOTM4	(0xe)
#define BOTP4	(0xf)

#define BODNZ	(0x10)
#define BODNZP	(0x11)
#define BODZ	(0x12)
#define BODZP	(0x13)
#define BODNZM4 (0x18)
#define BODNZP4 (0x19)
#define BODZM4	(0x1a)
#define BODZP4	(0x1b)

#define BOU	(0x14)

/* The BI condition bit encodings used in extended conditional branch
   mnemonics.  */
#define CBLT	(0)
#define CBGT	(1)
#define CBEQ	(2)
#define CBSO	(3)

/* The TO encodings used in extended trap mnemonics.  */
#define TOLGT	(0x1)
#define TOLLT	(0x2)
#define TOEQ	(0x4)
#define TOLGE	(0x5)
#define TOLNL	(0x5)
#define TOLLE	(0x6)
#define TOLNG	(0x6)
#define TOGT	(0x8)
#define TOGE	(0xc)
#define TONL	(0xc)
#define TOLT	(0x10)
#define TOLE	(0x14)
#define TONG	(0x14)
#define TONE	(0x18)
#define TOU	(0x1f)

/* Smaller names for the flags so each entry in the opcodes table will
   fit on a single line.  */
#undef	PPC
#define PPC     PPC_OPCODE_PPC
#define PPCCOM	PPC_OPCODE_PPC | PPC_OPCODE_COMMON
#define NOPOWER4 PPC_OPCODE_NOPOWER4 | PPCCOM
#define POWER4	PPC_OPCODE_POWER4
#define POWER5	PPC_OPCODE_POWER5
#define POWER6	PPC_OPCODE_POWER6
#define CELL	PPC_OPCODE_CELL
#define PPC32   PPC_OPCODE_32 | PPC_OPCODE_PPC
#define PPC64   PPC_OPCODE_64 | PPC_OPCODE_PPC
#define PPC403	PPC_OPCODE_403
#define PPC405	PPC403
#define PPC440	PPC_OPCODE_440
#define PPC750	PPC
#define PPC860	PPC
#define PPCVEC	PPC_OPCODE_ALTIVEC
#define	POWER   PPC_OPCODE_POWER
#define	POWER2	PPC_OPCODE_POWER | PPC_OPCODE_POWER2
#define PPCPWR2	PPC_OPCODE_PPC | PPC_OPCODE_POWER | PPC_OPCODE_POWER2
#define	POWER32	PPC_OPCODE_POWER | PPC_OPCODE_32
#define	COM     PPC_OPCODE_POWER | PPC_OPCODE_PPC | PPC_OPCODE_COMMON
#define	COM32   PPC_OPCODE_POWER | PPC_OPCODE_PPC | PPC_OPCODE_COMMON | PPC_OPCODE_32
#define	M601    PPC_OPCODE_POWER | PPC_OPCODE_601
#define PWRCOM	PPC_OPCODE_POWER | PPC_OPCODE_601 | PPC_OPCODE_COMMON
#define	MFDEC1	PPC_OPCODE_POWER
#define	MFDEC2	PPC_OPCODE_PPC | PPC_OPCODE_601 | PPC_OPCODE_BOOKE
#define BOOKE	PPC_OPCODE_BOOKE
#define BOOKE64	PPC_OPCODE_BOOKE64
#define CLASSIC	PPC_OPCODE_CLASSIC
#define PPCE300 PPC_OPCODE_E300
#define PPCSPE	PPC_OPCODE_SPE
#define PPCISEL	PPC_OPCODE_ISEL
#define PPCEFS	PPC_OPCODE_EFS
#define PPCBRLK	PPC_OPCODE_BRLOCK
#define PPCPMR	PPC_OPCODE_PMR
#define PPCCHLK	PPC_OPCODE_CACHELCK
#define PPCCHLK64	PPC_OPCODE_CACHELCK | PPC_OPCODE_BOOKE64
#define PPCRFMCI	PPC_OPCODE_RFMCI

/* The opcode table.

   The format of the opcode table is:

   NAME	     OPCODE	MASK		FLAGS		{ OPERANDS }

   NAME is the name of the instruction.
   OPCODE is the instruction opcode.
   MASK is the opcode mask; this is used to tell the disassembler
     which bits in the actual opcode must match OPCODE.
   FLAGS are flags indicated what processors support the instruction.
   OPERANDS is the list of operands.

   The disassembler reads the table in order and prints the first
   instruction which matches, so this table is sorted to put more
   specific instructions before more general instructions.  It is also
   sorted by major opcode.  */

const struct powerpc_opcode powerpc_opcodes[] = {
{ "attn",    X(0,256), X_MASK,		POWER4,		{ 0 } },
{ "tdlgti",  OPTO(2,TOLGT), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdllti",  OPTO(2,TOLLT), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdeqi",   OPTO(2,TOEQ), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdlgei",  OPTO(2,TOLGE), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdlnli",  OPTO(2,TOLNL), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdllei",  OPTO(2,TOLLE), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdlngi",  OPTO(2,TOLNG), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdgti",   OPTO(2,TOGT), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdgei",   OPTO(2,TOGE), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdnli",   OPTO(2,TONL), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdlti",   OPTO(2,TOLT), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdlei",   OPTO(2,TOLE), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdngi",   OPTO(2,TONG), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdnei",   OPTO(2,TONE), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdi",     OP(2),	OP_MASK,	PPC64,		{ TO, RA, SI } },

{ "twlgti",  OPTO(3,TOLGT), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tlgti",   OPTO(3,TOLGT), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twllti",  OPTO(3,TOLLT), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tllti",   OPTO(3,TOLLT), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "tweqi",   OPTO(3,TOEQ), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "teqi",    OPTO(3,TOEQ), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twlgei",  OPTO(3,TOLGE), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tlgei",   OPTO(3,TOLGE), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twlnli",  OPTO(3,TOLNL), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tlnli",   OPTO(3,TOLNL), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twllei",  OPTO(3,TOLLE), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tllei",   OPTO(3,TOLLE), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twlngi",  OPTO(3,TOLNG), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tlngi",   OPTO(3,TOLNG), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twgti",   OPTO(3,TOGT), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tgti",    OPTO(3,TOGT), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twgei",   OPTO(3,TOGE), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tgei",    OPTO(3,TOGE), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twnli",   OPTO(3,TONL), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tnli",    OPTO(3,TONL), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twlti",   OPTO(3,TOLT), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tlti",    OPTO(3,TOLT), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twlei",   OPTO(3,TOLE), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tlei",    OPTO(3,TOLE), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twngi",   OPTO(3,TONG), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tngi",    OPTO(3,TONG), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twnei",   OPTO(3,TONE), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tnei",    OPTO(3,TONE), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twi",     OP(3),	OP_MASK,	PPCCOM,		{ TO, RA, SI } },
{ "ti",      OP(3),	OP_MASK,	PWRCOM,		{ TO, RA, SI } },

{ "macchw",	XO(4,172,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "macchw.",	XO(4,172,0,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "macchwo",	XO(4,172,1,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "macchwo.",	XO(4,172,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "macchws",	XO(4,236,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "macchws.",	XO(4,236,0,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "macchwso",	XO(4,236,1,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "macchwso.",	XO(4,236,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "macchwsu",	XO(4,204,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "macchwsu.",	XO(4,204,0,1), XO_MASK, PPC405|PPC440,	{ RT, RA, RB } },
{ "macchwsuo",	XO(4,204,1,0), XO_MASK, PPC405|PPC440,	{ RT, RA, RB } },
{ "macchwsuo.",	XO(4,204,1,1), XO_MASK, PPC405|PPC440,	{ RT, RA, RB } },
{ "macchwu",	XO(4,140,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "macchwu.",	XO(4,140,0,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "macchwuo",	XO(4,140,1,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "macchwuo.",	XO(4,140,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhw",	XO(4,44,0,0),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhw.",	XO(4,44,0,1),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhwo",	XO(4,44,1,0),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhwo.",	XO(4,44,1,1),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhws",	XO(4,108,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhws.",	XO(4,108,0,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhwso",	XO(4,108,1,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhwso.",	XO(4,108,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhwsu",	XO(4,76,0,0),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhwsu.",	XO(4,76,0,1),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhwsuo",	XO(4,76,1,0),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhwsuo.",	XO(4,76,1,1),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhwu",	XO(4,12,0,0),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhwu.",	XO(4,12,0,1),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhwuo",	XO(4,12,1,0),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "machhwuo.",	XO(4,12,1,1),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhw",	XO(4,428,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhw.",	XO(4,428,0,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhwo",	XO(4,428,1,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhwo.",	XO(4,428,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhws",	XO(4,492,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhws.",	XO(4,492,0,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhwso",	XO(4,492,1,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhwso.",	XO(4,492,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhwsu",	XO(4,460,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhwsu.",	XO(4,460,0,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhwsuo",	XO(4,460,1,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhwsuo.",	XO(4,460,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhwu",	XO(4,396,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhwu.",	XO(4,396,0,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhwuo",	XO(4,396,1,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhwuo.",	XO(4,396,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "mulchw",	XRC(4,168,0),  X_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "mulchw.",	XRC(4,168,1),  X_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "mulchwu",	XRC(4,136,0),  X_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "mulchwu.",	XRC(4,136,1),  X_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "mulhhw",	XRC(4,40,0),   X_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "mulhhw.",	XRC(4,40,1),   X_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "mulhhwu",	XRC(4,8,0),    X_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "mulhhwu.",	XRC(4,8,1),    X_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "mullhw",	XRC(4,424,0),  X_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "mullhw.",	XRC(4,424,1),  X_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "mullhwu",	XRC(4,392,0),  X_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "mullhwu.",	XRC(4,392,1),  X_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmacchw",	XO(4,174,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmacchw.",	XO(4,174,0,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmacchwo",	XO(4,174,1,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmacchwo.",	XO(4,174,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmacchws",	XO(4,238,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmacchws.",	XO(4,238,0,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmacchwso",	XO(4,238,1,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmacchwso.",	XO(4,238,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmachhw",	XO(4,46,0,0),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmachhw.",	XO(4,46,0,1),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmachhwo",	XO(4,46,1,0),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmachhwo.",	XO(4,46,1,1),  XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmachhws",	XO(4,110,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmachhws.",	XO(4,110,0,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmachhwso",	XO(4,110,1,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmachhwso.",	XO(4,110,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmaclhw",	XO(4,430,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmaclhw.",	XO(4,430,0,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmaclhwo",	XO(4,430,1,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmaclhwo.",	XO(4,430,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmaclhws",	XO(4,494,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmaclhws.",	XO(4,494,0,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmaclhwso",	XO(4,494,1,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmaclhwso.",	XO(4,494,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{ "mfvscr",  VX(4, 1540), VX_MASK,	PPCVEC,		{ VD } },
{ "mtvscr",  VX(4, 1604), VX_MASK,	PPCVEC,		{ VB } },

  /* Double-precision opcodes.  */
  /* Some of these conflict with AltiVec, so move them before, since
     PPCVEC includes the PPC_OPCODE_PPC set.  */
{ "efscfd",   VX(4, 719), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdabs",   VX(4, 740), VX_MASK,	PPCEFS,		{ RS, RA } },
{ "efdnabs",  VX(4, 741), VX_MASK,	PPCEFS,		{ RS, RA } },
{ "efdneg",   VX(4, 742), VX_MASK,	PPCEFS,		{ RS, RA } },
{ "efdadd",   VX(4, 736), VX_MASK,	PPCEFS,		{ RS, RA, RB } },
{ "efdsub",   VX(4, 737), VX_MASK,	PPCEFS,		{ RS, RA, RB } },
{ "efdmul",   VX(4, 744), VX_MASK,	PPCEFS,		{ RS, RA, RB } },
{ "efddiv",   VX(4, 745), VX_MASK,	PPCEFS,		{ RS, RA, RB } },
{ "efdcmpgt", VX(4, 748), VX_MASK,	PPCEFS,		{ CRFD, RA, RB } },
{ "efdcmplt", VX(4, 749), VX_MASK,	PPCEFS,		{ CRFD, RA, RB } },
{ "efdcmpeq", VX(4, 750), VX_MASK,	PPCEFS,		{ CRFD, RA, RB } },
{ "efdtstgt", VX(4, 764), VX_MASK,	PPCEFS,		{ CRFD, RA, RB } },
{ "efdtstlt", VX(4, 765), VX_MASK,	PPCEFS,		{ CRFD, RA, RB } },
{ "efdtsteq", VX(4, 766), VX_MASK,	PPCEFS,		{ CRFD, RA, RB } },
{ "efdcfsi",  VX(4, 753), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdcfsid", VX(4, 739), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdcfui",  VX(4, 752), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdcfuid", VX(4, 738), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdcfsf",  VX(4, 755), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdcfuf",  VX(4, 754), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdctsi",  VX(4, 757), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdctsidz",VX(4, 747), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdctsiz", VX(4, 762), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdctui",  VX(4, 756), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdctuidz",VX(4, 746), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdctuiz", VX(4, 760), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdctsf",  VX(4, 759), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdctuf",  VX(4, 758), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdcfs",   VX(4, 751), VX_MASK,	PPCEFS,		{ RS, RB } },
  /* End of double-precision opcodes.  */

{ "vaddcuw", VX(4,  384), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vaddfp",  VX(4,   10), VX_MASK, 	PPCVEC,		{ VD, VA, VB } },
{ "vaddsbs", VX(4,  768), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vaddshs", VX(4,  832), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vaddsws", VX(4,  896), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vaddubm", VX(4,    0), VX_MASK, 	PPCVEC,		{ VD, VA, VB } },
{ "vaddubs", VX(4,  512), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vadduhm", VX(4,   64), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vadduhs", VX(4,  576), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vadduwm", VX(4,  128), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vadduws", VX(4,  640), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vand",    VX(4, 1028), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vandc",   VX(4, 1092), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vavgsb",  VX(4, 1282), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vavgsh",  VX(4, 1346), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vavgsw",  VX(4, 1410), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vavgub",  VX(4, 1026), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vavguh",  VX(4, 1090), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vavguw",  VX(4, 1154), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vcfsx",   VX(4,  842), VX_MASK,	PPCVEC,		{ VD, VB, UIMM } },
{ "vcfux",   VX(4,  778), VX_MASK,	PPCVEC,		{ VD, VB, UIMM } },
{ "vcmpbfp",   VXR(4, 966, 0), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpbfp.",  VXR(4, 966, 1), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpeqfp",  VXR(4, 198, 0), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpeqfp.", VXR(4, 198, 1), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpequb",  VXR(4,   6, 0), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpequb.", VXR(4,   6, 1), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpequh",  VXR(4,  70, 0), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpequh.", VXR(4,  70, 1), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpequw",  VXR(4, 134, 0), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpequw.", VXR(4, 134, 1), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgefp",  VXR(4, 454, 0), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgefp.", VXR(4, 454, 1), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgtfp",  VXR(4, 710, 0), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgtfp.", VXR(4, 710, 1), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgtsb",  VXR(4, 774, 0), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgtsb.", VXR(4, 774, 1), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgtsh",  VXR(4, 838, 0), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgtsh.", VXR(4, 838, 1), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgtsw",  VXR(4, 902, 0), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgtsw.", VXR(4, 902, 1), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgtub",  VXR(4, 518, 0), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgtub.", VXR(4, 518, 1), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgtuh",  VXR(4, 582, 0), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgtuh.", VXR(4, 582, 1), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgtuw",  VXR(4, 646, 0), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vcmpgtuw.", VXR(4, 646, 1), VXR_MASK, PPCVEC,	{ VD, VA, VB } },
{ "vctsxs",    VX(4,  970), VX_MASK,	PPCVEC,		{ VD, VB, UIMM } },
{ "vctuxs",    VX(4,  906), VX_MASK,	PPCVEC,		{ VD, VB, UIMM } },
{ "vexptefp",  VX(4,  394), VX_MASK,	PPCVEC,		{ VD, VB } },
{ "vlogefp",   VX(4,  458), VX_MASK,	PPCVEC,		{ VD, VB } },
{ "vmaddfp",   VXA(4,  46), VXA_MASK,	PPCVEC,		{ VD, VA, VC, VB } },
{ "vmaxfp",    VX(4, 1034), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmaxsb",    VX(4,  258), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmaxsh",    VX(4,  322), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmaxsw",    VX(4,  386), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmaxub",    VX(4,    2), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmaxuh",    VX(4,   66), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmaxuw",    VX(4,  130), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmhaddshs", VXA(4,  32), VXA_MASK,	PPCVEC,		{ VD, VA, VB, VC } },
{ "vmhraddshs", VXA(4, 33), VXA_MASK,	PPCVEC,		{ VD, VA, VB, VC } },
{ "vminfp",    VX(4, 1098), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vminsb",    VX(4,  770), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vminsh",    VX(4,  834), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vminsw",    VX(4,  898), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vminub",    VX(4,  514), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vminuh",    VX(4,  578), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vminuw",    VX(4,  642), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmladduhm", VXA(4,  34), VXA_MASK,	PPCVEC,		{ VD, VA, VB, VC } },
{ "vmrghb",    VX(4,   12), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmrghh",    VX(4,   76), VX_MASK,    PPCVEC,		{ VD, VA, VB } },
{ "vmrghw",    VX(4,  140), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmrglb",    VX(4,  268), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmrglh",    VX(4,  332), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmrglw",    VX(4,  396), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmsummbm",  VXA(4,  37), VXA_MASK,	PPCVEC,		{ VD, VA, VB, VC } },
{ "vmsumshm",  VXA(4,  40), VXA_MASK,	PPCVEC,		{ VD, VA, VB, VC } },
{ "vmsumshs",  VXA(4,  41), VXA_MASK,	PPCVEC,		{ VD, VA, VB, VC } },
{ "vmsumubm",  VXA(4,  36), VXA_MASK,   PPCVEC,		{ VD, VA, VB, VC } },
{ "vmsumuhm",  VXA(4,  38), VXA_MASK,   PPCVEC,		{ VD, VA, VB, VC } },
{ "vmsumuhs",  VXA(4,  39), VXA_MASK,   PPCVEC,		{ VD, VA, VB, VC } },
{ "vmulesb",   VX(4,  776), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmulesh",   VX(4,  840), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmuleub",   VX(4,  520), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmuleuh",   VX(4,  584), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmulosb",   VX(4,  264), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmulosh",   VX(4,  328), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmuloub",   VX(4,    8), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vmulouh",   VX(4,   72), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vnmsubfp",  VXA(4,  47), VXA_MASK,	PPCVEC,		{ VD, VA, VC, VB } },
{ "vnor",      VX(4, 1284), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vor",       VX(4, 1156), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vperm",     VXA(4,  43), VXA_MASK,	PPCVEC,		{ VD, VA, VB, VC } },
{ "vpkpx",     VX(4,  782), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vpkshss",   VX(4,  398), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vpkshus",   VX(4,  270), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vpkswss",   VX(4,  462), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vpkswus",   VX(4,  334), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vpkuhum",   VX(4,   14), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vpkuhus",   VX(4,  142), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vpkuwum",   VX(4,   78), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vpkuwus",   VX(4,  206), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vrefp",     VX(4,  266), VX_MASK,	PPCVEC,		{ VD, VB } },
{ "vrfim",     VX(4,  714), VX_MASK,	PPCVEC,		{ VD, VB } },
{ "vrfin",     VX(4,  522), VX_MASK,	PPCVEC,		{ VD, VB } },
{ "vrfip",     VX(4,  650), VX_MASK,	PPCVEC,		{ VD, VB } },
{ "vrfiz",     VX(4,  586), VX_MASK,	PPCVEC,		{ VD, VB } },
{ "vrlb",      VX(4,    4), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vrlh",      VX(4,   68), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vrlw",      VX(4,  132), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vrsqrtefp", VX(4,  330), VX_MASK,	PPCVEC,		{ VD, VB } },
{ "vsel",      VXA(4,  42), VXA_MASK,	PPCVEC,		{ VD, VA, VB, VC } },
{ "vsl",       VX(4,  452), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vslb",      VX(4,  260), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsldoi",    VXA(4,  44), VXA_MASK,	PPCVEC,		{ VD, VA, VB, SHB } },
{ "vslh",      VX(4,  324), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vslo",      VX(4, 1036), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vslw",      VX(4,  388), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vspltb",    VX(4,  524), VX_MASK,	PPCVEC,		{ VD, VB, UIMM } },
{ "vsplth",    VX(4,  588), VX_MASK,	PPCVEC,		{ VD, VB, UIMM } },
{ "vspltisb",  VX(4,  780), VX_MASK,	PPCVEC,		{ VD, SIMM } },
{ "vspltish",  VX(4,  844), VX_MASK,	PPCVEC,		{ VD, SIMM } },
{ "vspltisw",  VX(4,  908), VX_MASK,	PPCVEC,		{ VD, SIMM } },
{ "vspltw",    VX(4,  652), VX_MASK,	PPCVEC,		{ VD, VB, UIMM } },
{ "vsr",       VX(4,  708), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsrab",     VX(4,  772), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsrah",     VX(4,  836), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsraw",     VX(4,  900), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsrb",      VX(4,  516), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsrh",      VX(4,  580), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsro",      VX(4, 1100), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsrw",      VX(4,  644), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsubcuw",   VX(4, 1408), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsubfp",    VX(4,   74), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsubsbs",   VX(4, 1792), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsubshs",   VX(4, 1856), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsubsws",   VX(4, 1920), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsububm",   VX(4, 1024), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsububs",   VX(4, 1536), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsubuhm",   VX(4, 1088), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsubuhs",   VX(4, 1600), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsubuwm",   VX(4, 1152), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsubuws",   VX(4, 1664), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsumsws",   VX(4, 1928), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsum2sws",  VX(4, 1672), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsum4sbs",  VX(4, 1800), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsum4shs",  VX(4, 1608), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vsum4ubs",  VX(4, 1544), VX_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vupkhpx",   VX(4,  846), VX_MASK,	PPCVEC,		{ VD, VB } },
{ "vupkhsb",   VX(4,  526), VX_MASK,	PPCVEC,		{ VD, VB } },
{ "vupkhsh",   VX(4,  590), VX_MASK,	PPCVEC,		{ VD, VB } },
{ "vupklpx",   VX(4,  974), VX_MASK,	PPCVEC,		{ VD, VB } },
{ "vupklsb",   VX(4,  654), VX_MASK,	PPCVEC,		{ VD, VB } },
{ "vupklsh",   VX(4,  718), VX_MASK,	PPCVEC,		{ VD, VB } },
{ "vxor",      VX(4, 1220), VX_MASK,	PPCVEC,		{ VD, VA, VB } },

{ "evaddw",    VX(4, 512), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evaddiw",   VX(4, 514), VX_MASK,	PPCSPE,		{ RS, RB, UIMM } },
{ "evsubfw",   VX(4, 516), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evsubw",    VX(4, 516), VX_MASK,	PPCSPE,		{ RS, RB, RA } },
{ "evsubifw",  VX(4, 518), VX_MASK,	PPCSPE,		{ RS, UIMM, RB } },
{ "evsubiw",   VX(4, 518), VX_MASK,	PPCSPE,		{ RS, RB, UIMM } },
{ "evabs",     VX(4, 520), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evneg",     VX(4, 521), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evextsb",   VX(4, 522), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evextsh",   VX(4, 523), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evrndw",    VX(4, 524), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evcntlzw",  VX(4, 525), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evcntlsw",  VX(4, 526), VX_MASK,	PPCSPE,		{ RS, RA } },

{ "brinc",     VX(4, 527), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evand",     VX(4, 529), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evandc",    VX(4, 530), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmr",      VX(4, 535), VX_MASK,	PPCSPE,		{ RS, RA, BBA } },
{ "evor",      VX(4, 535), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evorc",     VX(4, 539), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evxor",     VX(4, 534), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "eveqv",     VX(4, 537), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evnand",    VX(4, 542), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evnot",     VX(4, 536), VX_MASK,	PPCSPE,		{ RS, RA, BBA } },
{ "evnor",     VX(4, 536), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evrlw",     VX(4, 552), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evrlwi",    VX(4, 554), VX_MASK,	PPCSPE,		{ RS, RA, EVUIMM } },
{ "evslw",     VX(4, 548), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evslwi",    VX(4, 550), VX_MASK,	PPCSPE,		{ RS, RA, EVUIMM } },
{ "evsrws",    VX(4, 545), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evsrwu",    VX(4, 544), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evsrwis",   VX(4, 547), VX_MASK,	PPCSPE,		{ RS, RA, EVUIMM } },
{ "evsrwiu",   VX(4, 546), VX_MASK,	PPCSPE,		{ RS, RA, EVUIMM } },
{ "evsplati",  VX(4, 553), VX_MASK,	PPCSPE,		{ RS, SIMM } },
{ "evsplatfi", VX(4, 555), VX_MASK,	PPCSPE,		{ RS, SIMM } },
{ "evmergehi", VX(4, 556), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmergelo", VX(4, 557), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmergehilo",VX(4,558), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmergelohi",VX(4,559), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evcmpgts",  VX(4, 561), VX_MASK,	PPCSPE,		{ CRFD, RA, RB } },
{ "evcmpgtu",  VX(4, 560), VX_MASK,	PPCSPE,		{ CRFD, RA, RB } },
{ "evcmplts",  VX(4, 563), VX_MASK,	PPCSPE,		{ CRFD, RA, RB } },
{ "evcmpltu",  VX(4, 562), VX_MASK,	PPCSPE,		{ CRFD, RA, RB } },
{ "evcmpeq",   VX(4, 564), VX_MASK,	PPCSPE,		{ CRFD, RA, RB } },
{ "evsel",     EVSEL(4,79),EVSEL_MASK,	PPCSPE,		{ RS, RA, RB, CRFS } },

{ "evldd",     VX(4, 769), VX_MASK,	PPCSPE,		{ RS, EVUIMM_8, RA } },
{ "evlddx",    VX(4, 768), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evldw",     VX(4, 771), VX_MASK,	PPCSPE,		{ RS, EVUIMM_8, RA } },
{ "evldwx",    VX(4, 770), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evldh",     VX(4, 773), VX_MASK,	PPCSPE,		{ RS, EVUIMM_8, RA } },
{ "evldhx",    VX(4, 772), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evlwhe",    VX(4, 785), VX_MASK,	PPCSPE,		{ RS, EVUIMM_4, RA } },
{ "evlwhex",   VX(4, 784), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evlwhou",   VX(4, 789), VX_MASK,	PPCSPE,		{ RS, EVUIMM_4, RA } },
{ "evlwhoux",  VX(4, 788), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evlwhos",   VX(4, 791), VX_MASK,	PPCSPE,		{ RS, EVUIMM_4, RA } },
{ "evlwhosx",  VX(4, 790), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evlwwsplat",VX(4, 793), VX_MASK,	PPCSPE,		{ RS, EVUIMM_4, RA } },
{ "evlwwsplatx",VX(4, 792), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evlwhsplat",VX(4, 797), VX_MASK,	PPCSPE,		{ RS, EVUIMM_4, RA } },
{ "evlwhsplatx",VX(4, 796), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evlhhesplat",VX(4, 777), VX_MASK,	PPCSPE,		{ RS, EVUIMM_2, RA } },
{ "evlhhesplatx",VX(4, 776), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evlhhousplat",VX(4, 781), VX_MASK,	PPCSPE,		{ RS, EVUIMM_2, RA } },
{ "evlhhousplatx",VX(4, 780), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evlhhossplat",VX(4, 783), VX_MASK,	PPCSPE,		{ RS, EVUIMM_2, RA } },
{ "evlhhossplatx",VX(4, 782), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evstdd",    VX(4, 801), VX_MASK,	PPCSPE,		{ RS, EVUIMM_8, RA } },
{ "evstddx",   VX(4, 800), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evstdw",    VX(4, 803), VX_MASK,	PPCSPE,		{ RS, EVUIMM_8, RA } },
{ "evstdwx",   VX(4, 802), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evstdh",    VX(4, 805), VX_MASK,	PPCSPE,		{ RS, EVUIMM_8, RA } },
{ "evstdhx",   VX(4, 804), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evstwwe",   VX(4, 825), VX_MASK,	PPCSPE,		{ RS, EVUIMM_4, RA } },
{ "evstwwex",  VX(4, 824), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evstwwo",   VX(4, 829), VX_MASK,	PPCSPE,		{ RS, EVUIMM_4, RA } },
{ "evstwwox",  VX(4, 828), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evstwhe",   VX(4, 817), VX_MASK,	PPCSPE,		{ RS, EVUIMM_4, RA } },
{ "evstwhex",  VX(4, 816), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evstwho",   VX(4, 821), VX_MASK,	PPCSPE,		{ RS, EVUIMM_4, RA } },
{ "evstwhox",  VX(4, 820), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evfsabs",   VX(4, 644), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evfsnabs",  VX(4, 645), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evfsneg",   VX(4, 646), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evfsadd",   VX(4, 640), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evfssub",   VX(4, 641), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evfsmul",   VX(4, 648), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evfsdiv",   VX(4, 649), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evfscmpgt", VX(4, 652), VX_MASK,	PPCSPE,		{ CRFD, RA, RB } },
{ "evfscmplt", VX(4, 653), VX_MASK,	PPCSPE,		{ CRFD, RA, RB } },
{ "evfscmpeq", VX(4, 654), VX_MASK,	PPCSPE,		{ CRFD, RA, RB } },
{ "evfststgt", VX(4, 668), VX_MASK,	PPCSPE,		{ CRFD, RA, RB } },
{ "evfststlt", VX(4, 669), VX_MASK,	PPCSPE,		{ CRFD, RA, RB } },
{ "evfststeq", VX(4, 670), VX_MASK,	PPCSPE,		{ CRFD, RA, RB } },
{ "evfscfui",  VX(4, 656), VX_MASK,	PPCSPE,		{ RS, RB } },
{ "evfsctuiz", VX(4, 664), VX_MASK,	PPCSPE,		{ RS, RB } },
{ "evfscfsi",  VX(4, 657), VX_MASK,	PPCSPE,		{ RS, RB } },
{ "evfscfuf",  VX(4, 658), VX_MASK,	PPCSPE,		{ RS, RB } },
{ "evfscfsf",  VX(4, 659), VX_MASK,	PPCSPE,		{ RS, RB } },
{ "evfsctui",  VX(4, 660), VX_MASK,	PPCSPE,		{ RS, RB } },
{ "evfsctsi",  VX(4, 661), VX_MASK,	PPCSPE,		{ RS, RB } },
{ "evfsctsiz", VX(4, 666), VX_MASK,	PPCSPE,		{ RS, RB } },
{ "evfsctuf",  VX(4, 662), VX_MASK,	PPCSPE,		{ RS, RB } },
{ "evfsctsf",  VX(4, 663), VX_MASK,	PPCSPE,		{ RS, RB } },

{ "efsabs",   VX(4, 708), VX_MASK,	PPCEFS,		{ RS, RA } },
{ "efsnabs",  VX(4, 709), VX_MASK,	PPCEFS,		{ RS, RA } },
{ "efsneg",   VX(4, 710), VX_MASK,	PPCEFS,		{ RS, RA } },
{ "efsadd",   VX(4, 704), VX_MASK,	PPCEFS,		{ RS, RA, RB } },
{ "efssub",   VX(4, 705), VX_MASK,	PPCEFS,		{ RS, RA, RB } },
{ "efsmul",   VX(4, 712), VX_MASK,	PPCEFS,		{ RS, RA, RB } },
{ "efsdiv",   VX(4, 713), VX_MASK,	PPCEFS,		{ RS, RA, RB } },
{ "efscmpgt", VX(4, 716), VX_MASK,	PPCEFS,		{ CRFD, RA, RB } },
{ "efscmplt", VX(4, 717), VX_MASK,	PPCEFS,		{ CRFD, RA, RB } },
{ "efscmpeq", VX(4, 718), VX_MASK,	PPCEFS,		{ CRFD, RA, RB } },
{ "efststgt", VX(4, 732), VX_MASK,	PPCEFS,		{ CRFD, RA, RB } },
{ "efststlt", VX(4, 733), VX_MASK,	PPCEFS,		{ CRFD, RA, RB } },
{ "efststeq", VX(4, 734), VX_MASK,	PPCEFS,		{ CRFD, RA, RB } },
{ "efscfui",  VX(4, 720), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efsctuiz", VX(4, 728), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efscfsi",  VX(4, 721), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efscfuf",  VX(4, 722), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efscfsf",  VX(4, 723), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efsctui",  VX(4, 724), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efsctsi",  VX(4, 725), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efsctsiz", VX(4, 730), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efsctuf",  VX(4, 726), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efsctsf",  VX(4, 727), VX_MASK,	PPCEFS,		{ RS, RB } },

{ "evmhossf",  VX(4, 1031), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhossfa", VX(4, 1063), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhosmf",  VX(4, 1039), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhosmfa", VX(4, 1071), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhosmi",  VX(4, 1037), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhosmia", VX(4, 1069), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhoumi",  VX(4, 1036), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhoumia", VX(4, 1068), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhessf",  VX(4, 1027), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhessfa", VX(4, 1059), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhesmf",  VX(4, 1035), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhesmfa", VX(4, 1067), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhesmi",  VX(4, 1033), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhesmia", VX(4, 1065), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmheumi",  VX(4, 1032), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmheumia", VX(4, 1064), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evmhossfaaw",VX(4, 1287), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhossiaaw",VX(4, 1285), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhosmfaaw",VX(4, 1295), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhosmiaaw",VX(4, 1293), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhousiaaw",VX(4, 1284), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhoumiaaw",VX(4, 1292), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhessfaaw",VX(4, 1283), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhessiaaw",VX(4, 1281), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhesmfaaw",VX(4, 1291), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhesmiaaw",VX(4, 1289), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmheusiaaw",VX(4, 1280), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmheumiaaw",VX(4, 1288), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evmhossfanw",VX(4, 1415), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhossianw",VX(4, 1413), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhosmfanw",VX(4, 1423), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhosmianw",VX(4, 1421), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhousianw",VX(4, 1412), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhoumianw",VX(4, 1420), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhessfanw",VX(4, 1411), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhessianw",VX(4, 1409), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhesmfanw",VX(4, 1419), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhesmianw",VX(4, 1417), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmheusianw",VX(4, 1408), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmheumianw",VX(4, 1416), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evmhogsmfaa",VX(4, 1327), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhogsmiaa",VX(4, 1325), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhogumiaa",VX(4, 1324), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhegsmfaa",VX(4, 1323), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhegsmiaa",VX(4, 1321), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhegumiaa",VX(4, 1320), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evmhogsmfan",VX(4, 1455), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhogsmian",VX(4, 1453), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhogumian",VX(4, 1452), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhegsmfan",VX(4, 1451), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhegsmian",VX(4, 1449), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhegumian",VX(4, 1448), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evmwhssf",  VX(4, 1095), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwhssfa", VX(4, 1127), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwhsmf",  VX(4, 1103), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwhsmfa", VX(4, 1135), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwhsmi",  VX(4, 1101), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwhsmia", VX(4, 1133), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwhumi",  VX(4, 1100), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwhumia", VX(4, 1132), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evmwlumi",  VX(4, 1096), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwlumia", VX(4, 1128), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evmwlssiaaw",VX(4, 1345), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwlsmiaaw",VX(4, 1353), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwlusiaaw",VX(4, 1344), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwlumiaaw",VX(4, 1352), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evmwlssianw",VX(4, 1473), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwlsmianw",VX(4, 1481), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwlusianw",VX(4, 1472), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwlumianw",VX(4, 1480), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evmwssf",   VX(4, 1107), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwssfa",  VX(4, 1139), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwsmf",   VX(4, 1115), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwsmfa",  VX(4, 1147), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwsmi",   VX(4, 1113), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwsmia",  VX(4, 1145), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwumi",   VX(4, 1112), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwumia",  VX(4, 1144), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evmwssfaa", VX(4, 1363), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwsmfaa", VX(4, 1371), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwsmiaa", VX(4, 1369), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwumiaa", VX(4, 1368), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evmwssfan", VX(4, 1491), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwsmfan", VX(4, 1499), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwsmian", VX(4, 1497), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmwumian", VX(4, 1496), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "evaddssiaaw",VX(4, 1217), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evaddsmiaaw",VX(4, 1225), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evaddusiaaw",VX(4, 1216), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evaddumiaaw",VX(4, 1224), VX_MASK,	PPCSPE,		{ RS, RA } },

{ "evsubfssiaaw",VX(4, 1219), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evsubfsmiaaw",VX(4, 1227), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evsubfusiaaw",VX(4, 1218), VX_MASK,	PPCSPE,		{ RS, RA } },
{ "evsubfumiaaw",VX(4, 1226), VX_MASK,	PPCSPE,		{ RS, RA } },

{ "evmra",    VX(4, 1220), VX_MASK,	PPCSPE,		{ RS, RA } },

{ "evdivws",  VX(4, 1222), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evdivwu",  VX(4, 1223), VX_MASK,	PPCSPE,		{ RS, RA, RB } },

{ "mulli",   OP(7),	OP_MASK,	PPCCOM,		{ RT, RA, SI } },
{ "muli",    OP(7),	OP_MASK,	PWRCOM,		{ RT, RA, SI } },

{ "subfic",  OP(8),	OP_MASK,	PPCCOM,		{ RT, RA, SI } },
{ "sfi",     OP(8),	OP_MASK,	PWRCOM,		{ RT, RA, SI } },

{ "dozi",    OP(9),	OP_MASK,	M601,		{ RT, RA, SI } },

{ "bce",     B(9,0,0),	B_MASK,		BOOKE64,	{ BO, BI, BD } },
{ "bcel",    B(9,0,1),	B_MASK,		BOOKE64,	{ BO, BI, BD } },
{ "bcea",    B(9,1,0),	B_MASK,		BOOKE64,	{ BO, BI, BDA } },
{ "bcela",   B(9,1,1),	B_MASK,		BOOKE64,	{ BO, BI, BDA } },

{ "cmplwi",  OPL(10,0),	OPL_MASK,	PPCCOM,		{ OBF, RA, UI } },
{ "cmpldi",  OPL(10,1), OPL_MASK,	PPC64,		{ OBF, RA, UI } },
{ "cmpli",   OP(10),	OP_MASK,	PPC,		{ BF, L, RA, UI } },
{ "cmpli",   OP(10),	OP_MASK,	PWRCOM,		{ BF, RA, UI } },

{ "cmpwi",   OPL(11,0),	OPL_MASK,	PPCCOM,		{ OBF, RA, SI } },
{ "cmpdi",   OPL(11,1),	OPL_MASK,	PPC64,		{ OBF, RA, SI } },
{ "cmpi",    OP(11),	OP_MASK,	PPC,		{ BF, L, RA, SI } },
{ "cmpi",    OP(11),	OP_MASK,	PWRCOM,		{ BF, RA, SI } },

{ "addic",   OP(12),	OP_MASK,	PPCCOM,		{ RT, RA, SI } },
{ "ai",	     OP(12),	OP_MASK,	PWRCOM,		{ RT, RA, SI } },
{ "subic",   OP(12),	OP_MASK,	PPCCOM,		{ RT, RA, NSI } },

{ "addic.",  OP(13),	OP_MASK,	PPCCOM,		{ RT, RA, SI } },
{ "ai.",     OP(13),	OP_MASK,	PWRCOM,		{ RT, RA, SI } },
{ "subic.",  OP(13),	OP_MASK,	PPCCOM,		{ RT, RA, NSI } },

{ "li",	     OP(14),	DRA_MASK,	PPCCOM,		{ RT, SI } },
{ "lil",     OP(14),	DRA_MASK,	PWRCOM,		{ RT, SI } },
{ "addi",    OP(14),	OP_MASK,	PPCCOM,		{ RT, RA0, SI } },
{ "cal",     OP(14),	OP_MASK,	PWRCOM,		{ RT, D, RA0 } },
{ "subi",    OP(14),	OP_MASK,	PPCCOM,		{ RT, RA0, NSI } },
{ "la",	     OP(14),	OP_MASK,	PPCCOM,		{ RT, D, RA0 } },

{ "lis",     OP(15),	DRA_MASK,	PPCCOM,		{ RT, SISIGNOPT } },
{ "liu",     OP(15),	DRA_MASK,	PWRCOM,		{ RT, SISIGNOPT } },
{ "addis",   OP(15),	OP_MASK,	PPCCOM,		{ RT,RA0,SISIGNOPT } },
{ "cau",     OP(15),	OP_MASK,	PWRCOM,		{ RT,RA0,SISIGNOPT } },
{ "subis",   OP(15),	OP_MASK,	PPCCOM,		{ RT, RA0, NSI } },

{ "bdnz-",   BBO(16,BODNZ,0,0),      BBOATBI_MASK, PPCCOM,	{ BDM } },
{ "bdnz+",   BBO(16,BODNZ,0,0),      BBOATBI_MASK, PPCCOM,	{ BDP } },
{ "bdnz",    BBO(16,BODNZ,0,0),      BBOATBI_MASK, PPCCOM,	{ BD } },
{ "bdn",     BBO(16,BODNZ,0,0),      BBOATBI_MASK, PWRCOM,	{ BD } },
{ "bdnzl-",  BBO(16,BODNZ,0,1),      BBOATBI_MASK, PPCCOM,	{ BDM } },
{ "bdnzl+",  BBO(16,BODNZ,0,1),      BBOATBI_MASK, PPCCOM,	{ BDP } },
{ "bdnzl",   BBO(16,BODNZ,0,1),      BBOATBI_MASK, PPCCOM,	{ BD } },
{ "bdnl",    BBO(16,BODNZ,0,1),      BBOATBI_MASK, PWRCOM,	{ BD } },
{ "bdnza-",  BBO(16,BODNZ,1,0),      BBOATBI_MASK, PPCCOM,	{ BDMA } },
{ "bdnza+",  BBO(16,BODNZ,1,0),      BBOATBI_MASK, PPCCOM,	{ BDPA } },
{ "bdnza",   BBO(16,BODNZ,1,0),      BBOATBI_MASK, PPCCOM,	{ BDA } },
{ "bdna",    BBO(16,BODNZ,1,0),      BBOATBI_MASK, PWRCOM,	{ BDA } },
{ "bdnzla-", BBO(16,BODNZ,1,1),      BBOATBI_MASK, PPCCOM,	{ BDMA } },
{ "bdnzla+", BBO(16,BODNZ,1,1),      BBOATBI_MASK, PPCCOM,	{ BDPA } },
{ "bdnzla",  BBO(16,BODNZ,1,1),      BBOATBI_MASK, PPCCOM,	{ BDA } },
{ "bdnla",   BBO(16,BODNZ,1,1),      BBOATBI_MASK, PWRCOM,	{ BDA } },
{ "bdz-",    BBO(16,BODZ,0,0),       BBOATBI_MASK, PPCCOM,	{ BDM } },
{ "bdz+",    BBO(16,BODZ,0,0),       BBOATBI_MASK, PPCCOM,	{ BDP } },
{ "bdz",     BBO(16,BODZ,0,0),       BBOATBI_MASK, COM,		{ BD } },
{ "bdzl-",   BBO(16,BODZ,0,1),       BBOATBI_MASK, PPCCOM,	{ BDM } },
{ "bdzl+",   BBO(16,BODZ,0,1),       BBOATBI_MASK, PPCCOM,	{ BDP } },
{ "bdzl",    BBO(16,BODZ,0,1),       BBOATBI_MASK, COM,		{ BD } },
{ "bdza-",   BBO(16,BODZ,1,0),       BBOATBI_MASK, PPCCOM,	{ BDMA } },
{ "bdza+",   BBO(16,BODZ,1,0),       BBOATBI_MASK, PPCCOM,	{ BDPA } },
{ "bdza",    BBO(16,BODZ,1,0),       BBOATBI_MASK, COM,		{ BDA } },
{ "bdzla-",  BBO(16,BODZ,1,1),       BBOATBI_MASK, PPCCOM,	{ BDMA } },
{ "bdzla+",  BBO(16,BODZ,1,1),       BBOATBI_MASK, PPCCOM,	{ BDPA } },
{ "bdzla",   BBO(16,BODZ,1,1),       BBOATBI_MASK, COM,		{ BDA } },
{ "blt-",    BBOCB(16,BOT,CBLT,0,0), BBOATCB_MASK, PPCCOM,	{ CR, BDM } },
{ "blt+",    BBOCB(16,BOT,CBLT,0,0), BBOATCB_MASK, PPCCOM,	{ CR, BDP } },
{ "blt",     BBOCB(16,BOT,CBLT,0,0), BBOATCB_MASK, COM,		{ CR, BD } },
{ "bltl-",   BBOCB(16,BOT,CBLT,0,1), BBOATCB_MASK, PPCCOM,	{ CR, BDM } },
{ "bltl+",   BBOCB(16,BOT,CBLT,0,1), BBOATCB_MASK, PPCCOM,	{ CR, BDP } },
{ "bltl",    BBOCB(16,BOT,CBLT,0,1), BBOATCB_MASK, COM,		{ CR, BD } },
{ "blta-",   BBOCB(16,BOT,CBLT,1,0), BBOATCB_MASK, PPCCOM,	{ CR, BDMA } },
{ "blta+",   BBOCB(16,BOT,CBLT,1,0), BBOATCB_MASK, PPCCOM,	{ CR, BDPA } },
{ "blta",    BBOCB(16,BOT,CBLT,1,0), BBOATCB_MASK, COM,		{ CR, BDA } },
{ "bltla-",  BBOCB(16,BOT,CBLT,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDMA } },
{ "bltla+",  BBOCB(16,BOT,CBLT,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDPA } },
{ "bltla",   BBOCB(16,BOT,CBLT,1,1), BBOATCB_MASK, COM,		{ CR, BDA } },
{ "bgt-",    BBOCB(16,BOT,CBGT,0,0), BBOATCB_MASK, PPCCOM,	{ CR, BDM } },
{ "bgt+",    BBOCB(16,BOT,CBGT,0,0), BBOATCB_MASK, PPCCOM,	{ CR, BDP } },
{ "bgt",     BBOCB(16,BOT,CBGT,0,0), BBOATCB_MASK, COM,		{ CR, BD } },
{ "bgtl-",   BBOCB(16,BOT,CBGT,0,1), BBOATCB_MASK, PPCCOM,	{ CR, BDM } },
{ "bgtl+",   BBOCB(16,BOT,CBGT,0,1), BBOATCB_MASK, PPCCOM,	{ CR, BDP } },
{ "bgtl",    BBOCB(16,BOT,CBGT,0,1), BBOATCB_MASK, COM,		{ CR, BD } },
{ "bgta-",   BBOCB(16,BOT,CBGT,1,0), BBOATCB_MASK, PPCCOM,	{ CR, BDMA } },
{ "bgta+",   BBOCB(16,BOT,CBGT,1,0), BBOATCB_MASK, PPCCOM,	{ CR, BDPA } },
{ "bgta",    BBOCB(16,BOT,CBGT,1,0), BBOATCB_MASK, COM,		{ CR, BDA } },
{ "bgtla-",  BBOCB(16,BOT,CBGT,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDMA } },
{ "bgtla+",  BBOCB(16,BOT,CBGT,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDPA } },
{ "bgtla",   BBOCB(16,BOT,CBGT,1,1), BBOATCB_MASK, COM,		{ CR, BDA } },
{ "beq-",    BBOCB(16,BOT,CBEQ,0,0), BBOATCB_MASK, PPCCOM,	{ CR, BDM } },
{ "beq+",    BBOCB(16,BOT,CBEQ,0,0), BBOATCB_MASK, PPCCOM,	{ CR, BDP } },
{ "beq",     BBOCB(16,BOT,CBEQ,0,0), BBOATCB_MASK, COM,		{ CR, BD } },
{ "beql-",   BBOCB(16,BOT,CBEQ,0,1), BBOATCB_MASK, PPCCOM,	{ CR, BDM } },
{ "beql+",   BBOCB(16,BOT,CBEQ,0,1), BBOATCB_MASK, PPCCOM,	{ CR, BDP } },
{ "beql",    BBOCB(16,BOT,CBEQ,0,1), BBOATCB_MASK, COM,		{ CR, BD } },
{ "beqa-",   BBOCB(16,BOT,CBEQ,1,0), BBOATCB_MASK, PPCCOM,	{ CR, BDMA } },
{ "beqa+",   BBOCB(16,BOT,CBEQ,1,0), BBOATCB_MASK, PPCCOM,	{ CR, BDPA } },
{ "beqa",    BBOCB(16,BOT,CBEQ,1,0), BBOATCB_MASK, COM,		{ CR, BDA } },
{ "beqla-",  BBOCB(16,BOT,CBEQ,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDMA } },
{ "beqla+",  BBOCB(16,BOT,CBEQ,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDPA } },
{ "beqla",   BBOCB(16,BOT,CBEQ,1,1), BBOATCB_MASK, COM,		{ CR, BDA } },
{ "bso-",    BBOCB(16,BOT,CBSO,0,0), BBOATCB_MASK, PPCCOM,	{ CR, BDM } },
{ "bso+",    BBOCB(16,BOT,CBSO,0,0), BBOATCB_MASK, PPCCOM,	{ CR, BDP } },
{ "bso",     BBOCB(16,BOT,CBSO,0,0), BBOATCB_MASK, COM,		{ CR, BD } },
{ "bsol-",   BBOCB(16,BOT,CBSO,0,1), BBOATCB_MASK, PPCCOM,	{ CR, BDM } },
{ "bsol+",   BBOCB(16,BOT,CBSO,0,1), BBOATCB_MASK, PPCCOM,	{ CR, BDP } },
{ "bsol",    BBOCB(16,BOT,CBSO,0,1), BBOATCB_MASK, COM,		{ CR, BD } },
{ "bsoa-",   BBOCB(16,BOT,CBSO,1,0), BBOATCB_MASK, PPCCOM,	{ CR, BDMA } },
{ "bsoa+",   BBOCB(16,BOT,CBSO,1,0), BBOATCB_MASK, PPCCOM,	{ CR, BDPA } },
{ "bsoa",    BBOCB(16,BOT,CBSO,1,0), BBOATCB_MASK, COM,		{ CR, BDA } },
{ "bsola-",  BBOCB(16,BOT,CBSO,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDMA } },
{ "bsola+",  BBOCB(16,BOT,CBSO,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDPA } },
{ "bsola",   BBOCB(16,BOT,CBSO,1,1), BBOATCB_MASK, COM,		{ CR, BDA } },
{ "bun-",    BBOCB(16,BOT,CBSO,0,0), BBOATCB_MASK, PPCCOM,	{ CR, BDM } },
{ "bun+",    BBOCB(16,BOT,CBSO,0,0), BBOATCB_MASK, PPCCOM,	{ CR, BDP } },
{ "bun",     BBOCB(16,BOT,CBSO,0,0), BBOATCB_MASK, PPCCOM,	{ CR, BD } },
{ "bunl-",   BBOCB(16,BOT,CBSO,0,1), BBOATCB_MASK, PPCCOM,	{ CR, BDM } },
{ "bunl+",   BBOCB(16,BOT,CBSO,0,1), BBOATCB_MASK, PPCCOM,	{ CR, BDP } },
{ "bunl",    BBOCB(16,BOT,CBSO,0,1), BBOATCB_MASK, PPCCOM,	{ CR, BD } },
{ "buna-",   BBOCB(16,BOT,CBSO,1,0), BBOATCB_MASK, PPCCOM,	{ CR, BDMA } },
{ "buna+",   BBOCB(16,BOT,CBSO,1,0), BBOATCB_MASK, PPCCOM,	{ CR, BDPA } },
{ "buna",    BBOCB(16,BOT,CBSO,1,0), BBOATCB_MASK, PPCCOM,	{ CR, BDA } },
{ "bunla-",  BBOCB(16,BOT,CBSO,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDMA } },
{ "bunla+",  BBOCB(16,BOT,CBSO,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDPA } },
{ "bunla",   BBOCB(16,BOT,CBSO,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDA } },
{ "bge-",    BBOCB(16,BOF,CBLT,0,0), BBOATCB_MASK, PPCCOM,	{ CR, BDM } },
{ "bge+",    BBOCB(16,BOF,CBLT,0,0), BBOATCB_MASK, PPCCOM,	{ CR, BDP } },
{ "bge",     BBOCB(16,BOF,CBLT,0,0), BBOATCB_MASK, COM,		{ CR, BD } },
{ "bgel-",   BBOCB(16,BOF,CBLT,0,1), BBOATCB_MASK, PPCCOM,	{ CR, BDM } },
{ "bgel+",   BBOCB(16,BOF,CBLT,0,1), BBOATCB_MASK, PPCCOM,	{ CR, BDP } },
{ "bgel",    BBOCB(16,BOF,CBLT,0,1), BBOATCB_MASK, COM,		{ CR, BD } },
{ "bgea-",   BBOCB(16,BOF,CBLT,1,0), BBOATCB_MASK, PPCCOM,	{ CR, BDMA } },
{ "bgea+",   BBOCB(16,BOF,CBLT,1,0), BBOATCB_MASK, PPCCOM,	{ CR, BDPA } },
{ "bgea",    BBOCB(16,BOF,CBLT,1,0), BBOATCB_MASK, COM,		{ CR, BDA } },
{ "bgela-",  BBOCB(16,BOF,CBLT,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDMA } },
{ "bgela+",  BBOCB(16,BOF,CBLT,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDPA } },
{ "bgela",   BBOCB(16,BOF,CBLT,1,1), BBOATCB_MASK, COM,		{ CR, BDA } },
{ "bnl-",    BBOCB(16,BOF,CBLT,0,0),ist
ATCB_MASK, PPCC/* pc-opc.c M- PowerPC op+ode list
   Copyright 1994, 1995, 1996, 1997, 1998, 2000, 20P- PowerPC opode liist
   Copyright 1994, 1995, 1996, 1997998,  2000, 20- PowerPC oppcode lst
   Copyright 19941 1995, 1996, 1997, 1998, 2000, 2001, 2002, 20003, 200
   GDB, GAS, and the GNU binutils are free software;  Ian Lance Taaylor, 
   GDB, GAS, and the GNU binutils are GDB, GAS, and the GNU binatils.

   GDB, GAS, and 14, 1995, 1996, 1997, 1998, 2000, 200-- PowerPC oparibute
   them and/or moion) any later version.

   GDB, GASP and the GNU bode list
   Copyright 19ion) any later versi GDB, GAS, and-- PowerPC opsion
  WARRANTY; without eve GNU binutils are free software; yRCHANTABILITY o3, 20NESS FOR A PARTICULAR PURPOSE.  See
   the GNU Generalful, but WITHOOUT ANYNESS FOR A PARTICULAR PURPOSE.  See
  warranty of MERCHANTABILIlecode list
   CopyrightG1994, 1995, 1996, 1997, 1998, 2000, 2001, 2002, 2le3, 2004,
   2005 Free ftware Foundation, 51 Franklin Street - Ian Lance leylor, Cygnus Support

ftware Foundation, 51 F GDB, GAS, and the GNU bleutils.

   GDB, GAS, aftware GNU binutils are free software; you can redletribute
   them and/orx

/* This file holds the PowerPC opcod>
#include <lic
   License as publix

/* This file holds t "ppc.h"

#define ATTRIBUTion
   2, or (at your Gption) any later version.

   GDB, GAS, and the GNlebinutils are distribute cost of increasing the table size.  Tful, but WITleUT ANY WARRANTY; withoe cost of increasing thhe file COPYING.  If not, w or FITNESS FOR A PARTe cosR PURPOSE.  See
   the GNU General Public Liceedgefor more details.

 operands into instructions and vice-vert it in
   thePublic License
   alonoperands into instructiwarranty of MERCHANTABILITgrite to the Free
   Software Foundation, 51 Franklin Street - Fifth Floong Boston, MA
   02110-1301, USA.  */

#include <linux/stddef.h>
#include nginux/kernel.h>
#include "nonstdio.h"
#include "ppc.h"

#define ATTRIBngE_UNUSED
#define _(x)	x

/* This file holds the PowerPC opcode table.  Tng opcode table
   includes almost all of the extended instruction mnemoning.  This
   permits the disassembler to use them, and simplifies the angembler
   logic, at the cost of increasing the table size.  The table is
ng strictly constant data, so the compiler should be able to put it in
   tng .text section.

   This file also holds the operand table.  All knowlngge about
   inserting operands into instructions and vice-versa is kept i(unsis
   file.  */

/* Local insertion and extraction functions.  */

sta(unsunsigned long insert_bat (unsigned long, long, int, const char **);
swrite to the Free
   SoEQ994, 1995, 1996, 1997, 1998, 2000, 2001, 2002, 20, Boston, MA
   02110-1ract_dq (unsigned long, int, int *);
st Ian Lance Tlinux/kernel.h>
#includract_dq (unsigned long, GDB, GAS, and the GNU biTE_UNUSED
#define _(x)	ract_e GNU binutils are free software; you can redie opcode table
   inclu, const char **);
static long extract_datic long extic
   License as publi, const char **);
statinsigned long insert_de (union
   2, or (at your EQtion) any later version.

   GDB, GAS, and the GNU  strictly constant datinsert_fxm (unsigned long, long, int, cful, but WITHe .text section.

   Thinsert_fxm (unsigned lo long, long, int, const cha or FITNESS FOR A PARTinserR PURPOSE.  See
   the GNU General Public Licen this
   file.  */

/*t, int *);
static unsigned long insert_unsigned long  *);
static unsigned lt, int *);
static unsigwarranty of MERCHANTABILITscode list
   CopyrightSO994, 1995, 1996, 1997, 1998, 2000, 2001, 2002, 20s3, 2004,
   2005 Free ong extract_mb6 (unsigned long, int, in Ian Lance Tsylor, Cygnus Support

ong extract_mb6 (unsign GDB, GAS, and the GNU bisutils.

   GDB, GAS, aong ee GNU binutils are free software; you can redistribute
   them and/orlong, int, const char **);
static long  **);
static ic
   License as publilong, int, const char *static unsigned long inserion
   2, or (at your SOtion) any later version.

   GDB, GAS, and the GNUsbinutils are distribut char **);
static unsigned long insert_ful, but WITHsUT ANY WARRANTY; witho char **);
static unsignsigned long, long, int, co or FITNESS FOR A PART charR PURPOSE.  See
   the GNU General Public Licenslong, int, const char nst char **);
static long extract_rbs (ras (unsigned  *);
static unsigned lnst char **);
static lowarranty of MERCHANTABILITunst char **);
static long extract_mb6 (unsigned long, int, int *);
staticuunsigned long insert_nb (unsigned long, long, int, const char **);
staticulong extract_nb (unsigned long, int, int *);
, 1998, 2000, 20 int *);
stat_nsi (unsigned long, long, int, const char **);
static long extract_nsi uunsigned long, int, int *);
static unsigned long insert_ral (unsigned lonu, long, int, const char **);
static unsigned long, int, const char **);
slong, long, int, const char **);
static unsigned long insert_raq (unsignedulong, long, int, const char **);
static unsigned long insert_ras (unsignedulong, long, int, const char **);
static unsig, 1998, 2000, 20long, int, coed long, long, int, const char **);
static long extract_rbs (unsigned longu int, int *);
static unsigned long insert_rsq (unsigned long, long, int, cunst char **);
static unsigned long insert_rtqed long, int, int *);
statidnztr FITNES CopyrDNZ1994, 1995,Y6, 1997NOPOWER4, 20BI, 2001, 2002, 2nsigfor more, int, int *);

/* The operands table.

   Th Ian Lance nsigblic Lic, int, int *);

/* The oper, 1998, 20
   Tharens around tpcoderious additions,e GNU bThe operands table.

   The fields are bi03, 2 caused trouble with feeble
   compilers with a lparens around taylor caused trouble with feeble
     for BA just below.  However,ion
 rious additionion) any The operands table.

   Theract_ev8 (unsigong,  are never used in a context where
   the additioful, but WITbiguorosoft Developer Sd in a context wh  for BA just be[] =
{
  /* Thesion
 index is used twith feeble
   compilers with a liof
     operands.+ */
#define UNUSED 0
  { 0, 0, NULL, NULL, 0 },

  s[] =
{
  /* The *);
/
#define UNUSED 0
  { 0, 0, NUend of the list of
     operanfned long, int, inF *);

/* The operands table.

   The fields are bfts, shift, insertm instruction when it must be the samparens around fhe various additim instruction when   for BA just below.  Howeverf that caused troum inwith feeble
   compilers with a limit on depth ff a parenthesizedtion.  */
#define BB BAT + 1
#define 1
  { 5, 16, incrosoft Developertion.  */
#define BAKE },

  /* The BB field in aacros are never uFed in a context where
   the addition will be ambigfous.  */

const sstruction.  */
#define BBA BB + 1
  {s[] =
{
  /* Thf zero index is usstruction.  */
#defPC_OPERAND_CR },

  /* The BA fs.  */
#define UNstru 0
  { 0, 0, NULL, NULL, 0 },

  /* The BA fieldfin an XL form ins6, 0, insert_bd, extract_bd, PPC_OPERd in a B form in0x1f << 16)
  { 56, 0, insert_bd, exend of the list of
     opegned l various add1994, 1995, 1e one
   for BA just be01, 2002, 2its, ssert_bd, extract_bd, PPC_OPERAND_ABSOLUTE | PPC_ Ian Lance t",	},

  /* The BD field in a B form instruction when- PowerPC ifie},

  /* The BD field in a B form iWRor BA just below.  Howe, thatsert_bd, extracte GNU binOPERAND_ABSOLUTE | PPC_OPERAND_SIGN03, 2004,
 OPERAND_RELATIVE | PPC_OPERAND_SIGNED },

 the - modifaylor, CygnOPERAND_RELATIVE | PPC_OPERAND_SIGNED },

 appropriateic
   LiceOPERAND_RELATIVE | PPC_OPER, insert_bdm, extract_bdm,ion
   rt_bd, extraion) any laOPERAND_ABSOLUTE | PPC_O+ 1
  { 16, binutilD },

  /* The BD field in a B form instructioful, but WITe zeroier is used.
     This sets the y bit of the BO -- PowerPC priately. },

  /* The BD field in a B , insert_bdm, eield approprsion
   2, 

  /* TheLATIVE | PPC_OPERAND_SIGNED },

 ,

  /* The BDribute
   a B form instruction when the + modifier field approprOUT ANY WAR a B form instruction when the + modifier p, extract_bd *);
stati a B form instruction when PERAND_SIGNED },

  /* Thefield      PPC_OPEm instructi| PPC_OPERAND_SIGNED },

  /* The BDBT fiedefine BF BDPA + 1
  { 3, 23, NULL, NULL, PPC_OP Ian Lance fier is used.
    PA + 1
  { 3, 23, NULL, NULL, PPC_OP, extract_bnsert_     in which an omitted BF fiel, insert_bdm, extract_bdman XL  OBF BF + 1
  { LATIVE | PPC_OPERAND_SIGNED },

  /* The BD<< 11)OPTIONAL },

  /* The BFA field in an X or XL foson instructd absolute addres
  /* The BFA field in an X or XL foero.  */
#densert_bdm, extra
  /* The BFA field i, PPC_OPERAND_CR | PPC_OPED_SIGNED },

  /*struction. ld in a B form instruction when the +bba, e PPC_OPERAND_CR },

  /* The BO field in a B forful, but WITnstruc PPC_OPERAND_CR },

  /* The BO field in a B forp, extract_b BO_MASKC_OPERAND_CR },

  /* The BO f X or XL form instruction. BD field in a B f6, 0, inser* The BO field in a B form instructionand absolute addr is like the BO field, but it must b + 1
#define 0, insert_bdp, ex is like the BO field, but it must b, 0 },

  /* ND_SIGNED },

   is like the BO fieldPERAND_SIGNED },

  /* Thedigned l << 16)
  {nt *);

/* The operands table.

   The fields arebits, s5, 21, NULL, NULL, PPC_OPERAND_CR },

  /* The coparens aroun the va5, 21, NULL, NULL, PPC_OPERAND_  for BA just below.  Howevr, that5, 21, NULL, NULwith feeble
   compilers with a limit on dept of a ps, which set the lower two bits of the
     BI fiXL form instruaylor,, which set the lower two bitsd
     conditional branch mneor FITNES1, NULL, Nd in a context where
   the addition will be ambguous. truction.  */
#define CRB CR + 1
  { 5, 6, NULL, s[] =
{
  /* he zerotruction.  */
#define CRB CR + end of the list of
     opernds.  *truction.  */
#d 0
  { 0, 0, NULL, NULL, 0 },

  /* The BA fied in anrm instruction.  */
#define CRFS CRFD + 1
  { 3,  CRFD CRB + 1
  *);
sm instruction.  */
#define CRF },

  /* The CRFS field in a  */
#druction.  *m instruction when it must be the same
     as th BT fieThe D field in a D form instruction.  This is a dXL form instrefine Ohe D field in a D form instrucd
     conditional branch mnERAND_Ohe D field in a lower two bits of the
     BI field.  This fiion.  *RAND_PARENS | PPC_OPERAND_SIGNED },

  /* The DEs a register iRAND_CR | PPC_OPEction when it must be the same
     as the BA ield in instruction. #define CRB CR + 1
  { 5, 6, NULL, NULL, 0 },

  n.  Cer
  /* The DES field in a DES form instruction.   CRFD CRB + 1
 BO_MAS
  /* The DES field in a DES fULL, PPC_OPERAND_OPTIONAL },

 X form instructi6, 0, insert_bd, extract_bd, PPC_OPERAND_RELATIVE  PPC_OPinsert_des, extr/* The CT field in an X form instruction.  */
# The BT

  /* The DQ field in a DQ foPC_OPERAND_OPTIONAL },

  /c */
#defi Cop94, 1	96, 199		, 1998, A juOE,

   The fields arc
  /* An D_SIGNED | PPC_OPERAND_DQ },

  /* The DS Ian Lance cier is usD_SIGNED | PPC_OPER_DQ },

  * The DSlower two bpcode lisD_SIGN1D | PPC_OPERAND_DQ },

  /* The DS field in a 03, 2004,, extract_ds,
      PPC_OPERAND_PARENS |  lower two baylor, Cy, extract_ds,
     #define DS DQ + 1
  { 16, 0, D_SIGNED  Copion) | PPC_OPERAND_DQ },

  /* The DS D_PARENS | P.  Certai FL1 field in a POWER SC form instructionful, but WITcBO_MASK ( FL1 field in a POW#define DS DQ + 1
2 field in asion
   2 FL1 fact_ds,
      PPC_OPERAND_PARENS | P1 + 1
  { 3, 2f
      NULL, 0 },

  /* The FLM field in an XFLL2 field in aOUT ANY W NULL, 0 },

  /* T */
#define FL2 FL1 + 1
 erPCscly.  */ SC(17S fieldSCPPC_OPEPPC },

LEV- PowerPCsvFRA FLM  1
#de94, 19RA_MASK (0s ta },

SVC_LEV, FL1fiel2
  { 5, 16, aylor, ULL, PPCe GNRAND_FPR },

  /* The FRB field in an X or A forUT ANY  1
#define FRA_MASK (0, inser* Then X or A form*);
st 1
#defi */
#define FRB FRA + 1
#*/
#definebits are for8ed to zero.  */
#define LI is kept inRA FLM +1
#defiin an X or A form ins6)
  { 5, 6,BO_MASK ULL, Pm instruction.  */
#defLIa is kept in POWER SC form _OPERAND_FPR },

  /* T */
#definemcrefine OXL(19fieldXLB96, 19|(3 << 21) 21, NU16)tq (uns{ BF, BF */
#defineblrly.  */XLOne FBOU,_SIG), XLBOBI
  { 5,,
      PPC_0- PowerPC M field  in an XFX instruction.  */
#define, insert_+ 1
#definelr NULL, in an XFX inste GNion.  */
#define FXM FRS + 1
#define  NULL, Nwer4 version for mfcr.  */
#definem, extract_fxm, 0 },dnzXM fiein an XFXDNZinstruction.  */
#define FXM FRS + 1
#define L fi forin a D or X form instruction.  */
#ands table.
XM4 + 1
  { 1, 21, NULL, NULL, PM4instruction.  */
#define/* The LEV field in a POWt thin a D or X PPC_OPERAND_OPTIONAL },

  /* The LEV field in a POWNULL, 0 },

  /* uction.  */
#define SVC_LEV L + 1
  { 7, 5, NULL,  NULin a D or X formor mfcr.  */
#define FXM4 FXM + 1
  { 8,,

  /-",he LI field in an I form instructio
  /* The LEV field in a POW
     forced to ructioor mfcr.  */
#define 1
  { 26, 0, insert_li, e+    forced to  The .  */
#define LI LEV + 1
  { 26, 0, insert_li, eI field in an I PERAND_RELATIVE | PPC_OPERAND_SIGNED },

  /*  field The LI fiel form instruction.  */
#define L FXM4 + 1
  { , 21, NAND_ABSOLUTE | PPC_OPERAND_SIGNED }
  /* The LEV field in an an X (sync) form ruction.  */
#define SVC_LEV L + 1
  { 7, 5, NUfine LEAND_ABSOLUTE The LEV field in an SC form instruction.  */
#dm instruction.  */
#AND_OPTIONAL },

  /* The MB field in an M form i NULLND_ABSOLUTE | Pn I form instruction.  The lower two bitsi, extr
#define ME MB + 1
#define ME_MASK e MB_MASK (0x1f << 6)
  { 5L, NULL, 0 },

 OPERAND_RELATIVE | PPC_OPERAND_SIGNED },

  /* ddress.uction.  */
#defiThe MB and ME fields in an M form instruction e bits to select.PERAND_RELATIVE | PPC_OPERAND_SIGNED },

  /*ltC_OPERAND_   C XFXTthout estruction.CB*/
#define FXM F c-opr what this men X (syn
#define MBE ME + 1
  { 5, 6, NULL, Nands tablC_OPERAND_OPTIONAL | PPC_OPERAND_NEXM4MBE ME + 1
  { 5, 6, NULL, NUract_mbe, 0 },

  /* The nstructi
#define PT },
  { 32, 0, insert_mbe, extract_mbe, 0 },

  /* The apped to the low ean MD or MDS form instruction.  The high
     bit is M field */
#define MBE ME + 1
  { 5, 6, NULL, NU, inseC_OPERAND_OPTIONAL   */
#de
#define MBE ME + or mfcr. 6, NULL, NULL, PPC_OPERAND_OPTIONAL L, NULL,ERAND_OPTIONAL },

  /* The NB field extract_mbe, 0 },

  /* The  The value 32 is  an MD or M
  /* The NB field iB MO + 1
  { 6, 11, insert bits t the low end.  */
#
     0.  */
#define NB MO + 1
  { 6, 11, insertis the same as th0 },

  /* The NSI field in a D form instruction.  Th 12, in_OPERAND_OPTIONAL },

  /* The NB field i+ 1
  { 5, 21, NULL, g means.  */
#define MBEe co+ 1
  { 5, 6, NULL, NULL, PPC_OPERAND_OPTIOrm i| PPC_OPERAND_NEXT }ine RA NSI + 1
#define RAextract_mbe, 0 },

  /*  5, 16, NULL, NULL, P an Mine RA NSI + 1
#define RA_e, but 0 in the RA field apped to the low end.OPERAND_GPR },

  /* As above, but 0 in the RA field MASK (0x3f << 5)
  { 6/
#define RA0 RA + 1
  { 5, 16, NULL, NULL, PPC_Oield in an mbar instruine RA NSI + 1
#define RA_, X, XO, M, or MDS form i, PPC_OPERAND_OPTIONine RA
  /* The NB field in an X form instructirm inThe value 32 is stoan updating
     load, whbove, but 0 in the RA field t_nb, extract_nb, 0 },an updating
     load, whifield.  */
#define RAL RAQis the same as the
 d may not
     equal the RT field.  */
#define RAL RAQ 16, 0, insert_nsi, et_ral, NULL, PPC_OPERAND_GPR_0 },

  /* The RA fiAND_SIGNED },

  /* Than updating
     load, whi+ 1
  { 5, 21, NULL, eqmeans.  */
#define MBEinse+ 1
  { 5, 6, NULL, NULL, PPC_OPERAND_OPTIO  st| PPC_OPERAND_NEXT }loating point load, whichextract_mbe, 0 },

  /*   field may not be ze an Mloating point load, which  5, 16, insert_ras, NULL,apped to the low end.  */
#define RAS RAM + 1
  { 5, 16, insert_ras, NULL,MASK (0x3f << 5)
  { 6
  /* The RA field of the tlbwe instruction, whicield in an mbar instruloating point load, which ch is an updating
     st, PPC_OPERAND_OPTIONloatin
  /* The NB field in an X form instructi11)
 The value 32 is stoL, PPC_OPERAND_GPR },

    { 5, 16, insert_ras, NULL,t_nb, extract_nb, 0 },L, PPC_OPERAND_GPR },

  /the RS field in the instruis the same as the
 it must be the same as
     the RS field in the instru 16, 0, insert_nsi, efor extended
     mnemonics like mr.  */
#define AND_SIGNED },

  /* ThL, PPC_OPERAND_GPR },

  /+ 1
  { 5, 21, NULL, someans.  */
#define MBE cha+ 1
  { 5, 6, NULL, NULL, PPC_OPERAND_OPTIOine | PPC_OPERAND_NEXT }T RS
#define RT_MASK (0x1extract_mbe, 0 },

  /* LL, NULL, PPC_OPERAND an MT RS
#define RT_MASK (0x1fhe DS form stq instructioapped to the low end.R },

  /* The RS field of the DS form stq instructioMASK (0x3f << 5)
  { 6    value restrictions.  */
#define RSQ RS + 1
  ield in an mbar instruT RS
#define RT_MASK (0x1finstruction.  */
#define , PPC_OPERAND_OPTIONT RS
#
  /* The NB field in an X form instructi  /* The value 32 is sto tlbwe instruction, whichf the DS form stq instructiot_nb, extract_nb, 0 }, tlbwe instruction, which L, PPC_OPERAND_GPR | PPC_Ois the same as the
 RTO RSO
  { 5, 21, NULL, NULL, PPC_OPERAND_GPR | PPC_O 16, 0, insert_nsi, e /* The SH field in an X or M form instruction.  AND_SIGNED },

  /* Th tlbwe instruction, which + 1
  { 5, 21, NULL, une RS RBS + 1
#define RT RS
#define RT_MASK (0x1f << 21)
  { 5, 21, NU insNULL, PPC_OPERAND_GPR },

  /* The RS field of the DS form stq instruion, which is optionaial
     value restrictions.  */
#define RSQ RS + 1
 ins5, 21, insert_rsq, NULL, PPC_OPERAND_GPR_0 },

  /* The RT field of thine SI SHO + 1
  { 16,  /* The SI field in a D form instruction.  */
#define  The RS field of the tlbwe instruction, which is optional.  */
#def valuSO RTQ + 1
#define RTO RSO
  { 5, 21, NULL, NULL, PPC_OPERAND_GPR | D_SIGNED | PPC_OPERAN},

  /* The SH field in an X or M form instruction. valu#define SH RSO + 1
#define SH_MASK (0x1f << 11)
  { 5, 11, NULL, NULL5 and vice- versa.  */eld in an MD form instruction.  This is split.  */
gemeans.  */
#defineithout eRA NSI + 1
#define RA_MASK (0x1f << 16)
  { en, which is option an XFX form m[ft]ibat[lu] inbove, but 0 in the RA fieSPRBAT SPR + 1
#defi an MD or MDS form instruction.  The high
     bit i},

apped to the lowFend.  */
#define MB6 MBE + 2
#define ME6 MB6
#define */
#define SPRG SPRBATster number in an XFX form m[ft]sprg instruction.  */ield in an mbar in an XFX form m[ft]ibat[lu] ins, X, XO, M, or MDS forealues.  */
#define an XFX foating
     load, which means that the RA fn.  The value 32 is SR + 1
#define STRM_MASK (0x extract_sprg, 0 },

  /* ThNULL, 0 },

  /*  0 },

  /* The NSI field in a D form instruction.  LL, Nis the same as AT + 1
  { eld in a POWER SC form instruction.  */
#define m instruction.  T4, 2, NULL, NULL, 0 },

  /* The TBR field in an XFX AND_SIGNED },

  / SR + 1
#define STRM_MASK (0x3+ 1
  { 5, 21, NULL, nlAT index number in an XFX form m[ft]ibat[lu] instruction.  */
#define TO BAT SPR + 1
#define SPRBAT_MASK (0x3 << 17)
  { 2, 17, NULL, NULL, 0  /* The U field in an an MD or MDS form instruction.  The high
     bit i TO define SPRG SPRBAT + 1
  { 5, 16, insert_sprg, extract_sprg, 0 },

  / UI U + 1
  { 16, 0, N* The UI field in a D form instruction.  */
#define U NULL, NULL, 0 },

  /* The STRM field in an X AltiVec form instructi TO  */
#define STRM SR + 1
#define STRM_MASK (0x3 << 21)
  { 2, 21, NUr VXRULL, 0 },

  /* The SV field in a POWER SC form instruction.  */
#de11)
  { 5, 11, NULL,  0 },

  /* The NSI field in a D form instruction.  r VXRm instruction.  This is like the SPR
     field, but it is optional.  6, NULL, NULL, PPC_OPdefine VC VB + 1
#define VC_MASK (0x1f << 6)
  { 5, 6IONAL },

  /* The TO field in a D or X form instruction.  */
#definelBAT index number in an ine RA NSI + 1
#define RA_MASK (0x1f << 16)
  {M fiBAT SPR + 1
#define OPERAND_GPR },

  /* As above, but 0 in the RA fiLL, NULL, PPC_OPERAND0.  */
#define RA0 RA + 1
  { 5, 16, NULL, NULL, PPCM fidefine SPRG SPRBAT + GNED},

  /* The UIMM field in a VX form instruction.rm instruction.  */
#d + 1
  { 5, 16, NULL, NULL, 0 },

  /* The SHB fi NULL, NULL, 0 },

  /RAND_GPR_0 },

  /* The RA field in a D or X foM fi */
#define STRM SR an updating
     load, which means that the RA d in ULL, 0 },

  /* Thed may not
     equal the RT field.  */
#define RA  { 32, 11, insert_evinsert_ral, NULL, PPC_OPERAND_GPR_0 },

  /* The RA d in m instruction.  Thisextract_ev2, PPC_OPERAND_PARENS },

  /* The other UIMX form instruction.   form instruction.  */
#define EVUIMM_4 EVUIMM_2 IONAL },

  /* The TO  or X form instruction which is an updating
   ngfield in a VX form instruction.  */
#define SIMM VD + 1
  { 5, 16, NU

  NULL, PPC_OPERAND_SIGNED},

  /* The UIMM field in a VX form instruct< 11)
  { 3, 11, NULL0.  */
#define RA0 RA + 1
  { 5, 16, NULL, NULL, PPC

  d in a VA form instruction.  */
#define SHB UIMM + 1
  { 4, 6, NULL, N, NULL, NULL, PPC_OPERfine MTMSRD_L WS + 1
#define A_L MTMSRD_L
  { 1, 16,  EVUIMM SHB + 1
  { 5, 11, NULL, NULL, 0 },

  /* The other UIMM fiel

   a half word EVX form instruction.  */
#define EVUIMM_2 EVUIMM + 1

  { 2, 11, insert_ev2, extract_ev2, PPC_OPERAND_PARENS },

  /* The othe 0 },

#define RMC TEinsert_ral, NULL, PPC_OPERAND_GPR_0 },

  /* The RA 
  { 
  { 32, 11, insert_ev4, extract_ev4, PPC_OPERAND_PARENS },

  /* The

#define S SP + 1
  { 0 },

#define SP R + 1
  { 2, 11, NULL, NULL, 0 },

UIMM_4 + 1
  { 32, 11, insert_ev8, extract_ev8, PPC_OPERAND_PARENS },
BAT index number in an loating point load, which means that the RA
   ine BAT SPR + 1
#define   */
#define RAS RAM + 1
  { 5, 16, insert_ras, N The EH field in larx0 },

  /* The RA field of the tlbwe instruction, whine define SPRG SPRBAT + struction.  */
#define EH XRT_L + 1
  { 1, 0, NULL, Nion.  */
#define DCM MNAL },
};

/* The functions used to insert and ex NULL, NULL, 0 },

  /
#define RB RAOPT + 1
#define RB_MASK (0x1f << ine  */
#define STRM SR L, PPC_OPERAND_GPR },

  /* The RB field in an BT fiULL, 0 },

  /* Theit must be the same as
     the RS field in the ithat the fields are tused for extended
     mnemonics like mr.  */
#definBT fim instruction.  This   same.  */

static unsigned long
insert_bat (unsigneosition 16.  */
#defig value ATTRIBUTE_UNUSED,
	    int dialect ATTRIBIONAL },

  /* The TO S, X, XFX or XO form
     instruction.  */
#defnsAT index number in an T RS
#define RT_MASK (0x1f << 21)
  { 5, 21, NU((inBAT SPR + 1
#define R },

  /* The RS field of the DS form stq instruurn 0;
}

/* The BB fial
     value restrictions.  */
#define RSQ RS + 1
((indefine SPRG SPRBAT + d in an XL form instruction when it must be the same ion.  */
#define DCM Mthe same instruction.  This operand is marked FAK NULL, NULL, 0 },

  /5, 21, insert_rtq, NULL, PPC_OPERAND_GPR_0 },

((in */
#define STRM SR  tlbwe instruction, which is optional.  */
#def valuULL, 0 },

  /* TheRTO RSO
  { 5, 21, NULL, NULL, PPC_OPERAND_GPR | char **errmsg ATTRIBU},

  /* The SH field in an X or M form instruction. valum instruction.  ThisUNUSED)
{
  return insn | (((insn >> 16) & 0x1f) << 11osition 16.  */
#defiract_bba (unsigned long insn,
	     int dialect AIONAL },

  /* The TO ne SH6_MASK ((0x1f << 11) | (1 << 1))
  { 6, 1,nuinsn >> 21) & 0x1f) != ((insn >> 16) & 0x1f))
    *invalid = 1;
  retuun 0;
}

/* The BB field in an XL form instruction when it must be the sant dialect ATTRIBUTEd in the same instruction.  This operand is marked FAuE.
   The insertion function just copies the BA field into the BB field,
g insn,
	    int diale & 0xfffc);
}

static long
extract_bd (unsigned long iue ATTRIBUTE_UNUSED,
	    int dialect ATTRIBUTE_UNUSED,
	    const cn a **errmsg ATTRIBUTE_UNUSED)
{
  return insn | (((insn >> 16) & 0x1f) <ans that the branch g
extract_bba (unsigned long insn,
	     int dialect n a IBUTE_UNUSED,
	     int *invalid)
{
  if (((insn >> 16) & 0x1f) != ((ile), we set the y bit   *invalid = 1;
  return 0;
}

/* The BD field in  means. 
#define MFX form m[ft]*/
#define FXM FRS B)
  { 5, 6,5, 16, Nve, since if the y bit is 0
   wands table.

 to print the normal form of truction.  */
#d| PPC_OPERAND_SIGNble targets use3, 200e, since i The LEV fieldn.
   Power4 compatible targets use=> no hint, "at" nd "t", instead of
   the "y" bit.  "at" == bsert_raq, Nsince if the y bit is 0
   we, insert_bdto print the ND_SIGNEDsince if thor mfcr. s 0
   we just want to print the pcode CTR.  We only handle the taken/wer4 compatible targets use
static unsignedOPERAND_RELATIVd of
   the "y" bit.  "at" == 00 03, 20hint, "at" == 0g
insert_bdm (unsigned long insn,
	    long vD)
{
  if ((dialialect,
	    const char **errmsg ATTRIBUTE_Ub /* The RA .  We only handle the taken/nI) and 01000
   for bf 5, 16, NUL/* The T the y bit is 0
   we just want to print tfe normal form of f ((insn & (0x14 << 21wer4 compatible targets nsn |= 0x08 << 21;
and "t", instead of
   the "y" bit.  "at" == faken, "at" == 11 F == 01 => unpredictable,
   "at" == 10 => not     int *invalid)
{
 taken.  The "t" bit is 00001
   in BO field,fpositive, since f ((insn & (0x14 << 21) 21))
	insn |= 0x02 << 2 if ((insn & (0f ((ihandle the taken/not-taken hint here.   << static unsigne<< 21)
	  && (insn & (OPCODE_POWER4) == 0)
    {
21))
	*invalid =dialect,
	    const char **errmsg ATTRIBUTE_U) - D)
{
  if ((di{
  if    }

  return ((insn & 0xfffc) ^ 0x8000) - difier is used.
= 0)
	insn |= 1 << 21;
    }
  else
    {
   f< 21)) != (0x06 << 21)
	  && (insn & (0I) and 01000
   for bSK (0M fiin a D or X if the y bit is 0
   we just want to print t
	         forced to the instruction.
   Power4 comp ((dialect & PPC_OPCOI field in an I" == 01 => unpredictable,
   "at00) == 0)
	insn |= 1 lE_POWER4) == 0)
   handle the taken/not-taken hint here.  0x04 <<DE_POWER4) == 0)
       }

  return ((insn & 014 << 21)) == (0x04 <<<< 21;
    }
  else
 n |= 0x09 << 21;
    }
  return insn | (valu< 21;
in a D or X f ((insn & (0x14 << 21)) == (0x10 << 21))
	i int d     forced to 
    }
  return insn | (value &ong insn,
	     int dI field in an I{
  if ((dialect & PPC_OPCODE_POong insn,
	     int d< 21))
	insn |=<< 21)
	  && (insn & (0x1d << 21)) != (0x18 & (0x17 == 0)
    {
      ing
extract_bdp (unsigned long insn,
	     int d 0xfffc);
}

sta.
   This is like BDM, above, ex00) == 0)
	insn     con
#define ME rmsg ATTRIBUTE_UNUSED)
{
  if ((dialect & PPCOPCODE__bo (long value, int dialect)
{
  wer4 compatible targets ODE_P bits to selectelse
    {
      if ((insn & (0x14 << 21)) == (004 << 2_bo (long value, < 21)
	  && (insn & (0x1d << 21)) != (0x1)) == (0x10 << 21)
	insn |= 0x09 << 21;
    }
  rhave bits that are r 0xfffc);
}

satic long
extract_bdp (unsigned long insn,
	     nt dial
#define ME nt *invalid)
{
  if ((dialect & PPC_OPCODE_POER4) ==lue & 0x8) == 0;
	case 0x14:
	  rencodings have bits that ar    intue & 0x8) ==ro.
	 These are (z must be zero, y may be anythi << 21)ue & 0x8) == 0;
011zy
	     1z00y
	     1z01y
	     1z1zz) - 0x8    0000z
	     0001z
	     0100zings have bits that are requi 1;
	case 0x4:
k for legal values of a BO field.  */

static icaken, "at"YLKdefiFL1 fiel& 0xhe taken/not-taken h /* Th  else if ((vch is exx14) == 0x14or mfcurn value == 0x14;
      else
	return 1;normal fx14) == 0x94, 19eturn value == 0x14;
      else
	return 1;
static  set
   the field in a B form instruction.  Warn about attositive,set
   ther mfcrH_MASK (0x1#define DS DQ + 1Hse
	return 1;
> no hi14) == 0x1r **errmsg)
{
  if (!valid_bo (value, dialect)M field in const char **er PPC_OPE_OPERAND_FPDS DQ rn insn | ((  /* Powemsg = _("invalidstatic long
extract_bo (unsigned lonlrlinux/	    int 721);
}

static lBOOKE6lt:
	c
  long value;

  v  */
#d(insn >> alect,
	    intf (!valid_bo (value, dierPCrfid 1
#define F18),	0xf the +  (0x1alid LEV fielderPCcrnoely.  fine F33ield msg)
{
  if (!validT, BAthe
-- PowerPC.  T_OPERANDike the 	O field,  it must be even. - PowerPCrfmciositiveike t8), en the + or - mRFMCIe LEV fieldn a B f2, insertdefi5ielden the + or ERAND_FP+ 1
#definrfong
inseralect,act_en the + or - m403 | f (!v   long value,
	 6, NULL, (valu82 dialect))
    *,

  /* T is
   used. andFRA Ftructio29e BO field, e even.  */

static unsierPCisynoption");
  1,
	 nsigned long ins **errmsg)
{
  if ic long exsn | ((value & 0x1f) << 2OPERAND_FP is
   used. ialect,
sn | ((9he BO field, but it must be evbe e  When
   extxacting it, we
  long value;
 using + or - modifier");

 crnanorm i (valu225lue, dialect))
    *invalid = 1;
  return alue &  0x1e;
}57lue, dialect))
    *invalid = 1;
  returhB form in0x1e;
}74lue & 0x1f) << 2s ta5 | CELL   long value,
crsehis is like 28et y bit when
  value = (insn >> 21) & 0x1f;
  eqvlong value,
	   int dialec using + or - modifier");

 dozlinux/k (valu402nsigned long
insert_6ifier is
   used. oroption");
  41 but the
   lower four bits are forced tonapact_boe (unsi43unsigned long
insert_insn | (value & 0xfmovlinuxct ATTR4   int dialect ATTRIBUTE_UNUSE> 21) & 0x1f;
  define RTQ
  return ((insn &  using + or - modifier");

 sleent diaigned l6PC_O_UNUSED,
	    int *invalid ATTif (vwink<linuigned l9 (unsigned long i int *invalid ATTRIBUbc the "a" bit is 0U,528truction.  */
#defin extract_fxm, 0 },c   if ((insn & (0tiple invalid
  if ((value > 62) != 0)
    lt("offsevalue 32 is storedple of 2/* The NB field in an X 2000n | ((value & 0The value 32 is stored long
extract_ev2 (unsigextract_mbe, 0 },

  /* Th dialect ATTRIBUTE_ an MD ople of 2");
 field in a D form instruction.  Thctrequired the low end.  *tic unsigned long
inserRIBUTE_UNUSED)
{
  return (in 16, 0, insert_nsi, extrtic unsigned long
insert_ev4 (unsigned long insn,
:
	    TTRIBUTE_UNUSED,
	 invatract_ev2 (unsigned long insn,
	     int di 21))
	((value > 124) != 0)
    *errmsg = _("oRIBUTE_UNUSED)
{
  return (in return insn | (;
}

static  The NSI field in a D form instruction.  Thed lI fielng value,
	    int d
     0.  */
#define NB MO + 1
  { 6, 11, insBUTE_UNUSED)
{
  retint dialect ATTRIBUTE_UNUSED,
	     int *invalid ATTg & 0x3e) << 10);
}

staGic long
extract_ev2 (unsigned long insn,
	     iSED,
 may not be zero and m,
	     int *invalid ATTRIBUTE_UNUSED)
{
  retur"offset not a multiplinsert_rtic unsigned long
insert_ev4 (unsigned long iSED,
 in an lmw instruction,nt dialect ATTRIBUTE_UNUSED,
	    const char ** long
extract_ev8 (uns");
  return insn | ((value & 0xf8) << 8);
}

static long;
  if ((value > 12ed long)
    *errmsg = _("offset greater than 124The DSreturn insn | ((vam instruction.  This is likalect ATTRIBUTE_UNUSED,
	    long insn,
	     int m instructi{
  return (insn >> 8) & 0xf8;
}

/* The DS_UNUSED)
{
  return
	   int dialect ATTRIBUTEong
insert_ds (unsigned long i(unsigned long insn,
	   int dialect ATTRIBUTE_UNUSED,
	   const chaeq& 0x3e) << 10);
}

staEQc long
extract_ev2 (unsigned long insn,
	     iignedm instruction when it ,
	     int *invalid ATTRIBUTE_UNUSED)
{
  returRIBUTE_UNUSED)
{
  reused fortic unsigned long
insert_ev4 (unsigned long iigned RB + 1
  { 5, 1, insernt dialect ATTRIBUTE_UNUSED,
	    const char **gned long insn,
	   lostruction.  */

static unsigned long
insert_de (unsigned ;
  if ((value > 12value,
)
    *errmsg = _("offset greater than 124fset nreturn insn | ((vand 2047");
  return insn | RIBUTE_UNUSED,
	   const charlong insn,
	     int nd 2047");
|| value < -2048)
    *errmsg = _("offset n_UNUSED)
{
  returnd ATTRIBUTE_UNUSED)
{
  re    int dialect ATTRIBUTE_UNUS(unsigned long insn,d ATTRIBUTE_UNUSED)
{
  return (insn & 0xfff0) >so& 0x3e) << 10);
}

staSOc long
extract_ev2 (unsigned long insn,
	     iBUTE_SO RTQ + 1
#define RTO,
	     int *invalid ATTRIBUTE_UNUSED)
{
  retur192)
    *errmsg = _(},

  /*tic unsigned long
insert_ev4 (unsigned long iBUTE_#define SH RSO + 1
#defnt dialect ATTRIBUTE_UNUSED,
	    const char **| ((value << 2) & 0xffsg = _("offset not a multiple of 4");
  return insn | ((v;
  if ((value > 12;
}

st)
    *errmsg = _("offset greater than 124
  retreturn insn | ((va& 0x3ffc) ^ 0x2000) - 0x200es (unsigned long insn,
	    long insn,
	     int & 0x3ffc) ^     int *invalid ATTRIBUTE_UNUSED)
{
  ret_UNUSED)
{
  return,
	    int dialect,
	    cng
insert_fxm (unsigned long i(unsigned long insn,,
	    int dialect,
	    const char **errmsg)
{
unTE_UNUSED,
	    const char **errmsg)
{
  if (value > 8191 || value < -8{
       *errmsg = _("offset not between -8192 and 8191");
  else if ((valunvalid mask field");
value,
	    I field in a D form instruction.  */
#defin((value << 2) & 0xfff0);
}

static long
extract_des (unsigned long insn,
tion that moves the wheans we want to use
     the old form of the instruction turn (((insn >> 2) & 0x3ffc) ^ 0x2000) - 0x2000;
}

/* FXM mask in msomeond mtcrf instructions.  */

static unsigned long
insert_fxm (unsigned  one bit of the FXM fvalue,
	    int dialect,
	    const char **errmsg)
{
someonf we're handling the mfocrf and mtocrf insns ensure that exactly
    s is not backward compield is set.  */
  if ((insn & (1 << 20)) != 0)
    ge& 0x3e) << 10);
}
ight 19sg)
{
  if ((value & 7) != 0)
    *errmsg = _("ed. ULL, 0 },

  /* The SV");
  if ((value > 248) != 0)
    *errmsg = _("ofE_POWER4) != 0
	    ;
}

static unsigned long
insert_ev4 (unsigned long i (0x3m instruction.  This islong insn,
	     int dialect ATTRIBUTE_UNUSED,
	mfcr is an error.  */< 1)))
    insn |= 1 << 20;

  /* Any other value on mfcr;
  if ((value     || ((diuction.  This is like D, but the
   lower teturnreturn insn | e & 0xff) << 12);
}

static lon 1)) == 19 << 1)
    {
      g insn,
	     in int dialect ATTRIBUTE_UNUSED,
	     int *invalid ATTd long_UNUSED)
{
  r*/
  else ifeld in a POWER SC form instruction.  */
#defi& (1 << 20)) != 0)
 int dialect ATTRIBUTE_UNUSED,
	     int *invalid ATTnld.  */
  else if ((value & -value) == value
	   && ((dialect & PPC_OPCO /* CWER4) != 0
	       || ((dialect & PPC_OPCODE_ANY) != 0
		   && (insn &nsn & (0x3ff << 1)) =;
}

static unsigned long
insert_ev4 (unsigned long i /* C is an error.  */
  else if ((insn & (0x3ff << 1)) == 19 << 1)
    {
 er two bits are
   for

/* The LI field in an I form instruction.  The lower twn insn | ((value & 0xff) << 12);
}

static long
extract_fxm (unsignest cha insn,
	     int dialect ATTRIBUTE_UNUSED,
	     int *invalid)
{
  lost significant bits i int dialect ATTRIBUTE_UNUSED,
	     int *invalid ATTst cha << 20)) != 0)
    {
      /* Exactly one bit of MASK should be set. E_UNUSED,
	    int *inint dialect ATTRIBUTE_UNUSED,
	     int *invalid ATTRed.  */
  else if ((valrrmsg)
{
  if ((value & 7) != 0)
    *errmsg = _(ction2, 11, insert_ev2, ext,
	     int *invalid ATTRIBUTE_UNUSED)
{
  return(0x3ff << 1)) == 19 << 1)  return insn | ((value & 0xf8) << 8);
}

staticogniz
  { 32, 11, insert_ev4nt dialect ATTRIBUTE_UNUSED,
	    const char **e    *errmsg = _("ignoringhich uses a field of this type.  */

static unsigned n insn | ((value & m instruction.  This is like D, but the
   lower 
  int insn,
	     int dero.  */

static unsigned long
insert_ds (unsigned       *errmsg = _("ilalue,
	   int dialect ATTRIBUTE_UNUSED,
	   const cha
  int << 20)) != 0)
    e & 3) != 0)
    *errmsg = _("offset not a multiplt = 0;

  /* mb: locatn | (value & 0xfffc);
}

static long
extract_ds (unsngion expressed as a single
   operand which is itself a bitmask.  The ex 1L <ion function always
   marks it as invalid, since we never want to recast)
	{
	  ++count;
	ion which uses a field of this type.  */

static unsi 1L <long
insert_mbe (unsigned long insn,
	    long value,
	    int dialect)
    me = 32;

  if (unt;
	  me = mx;
	  last = 0;
	}
    }
  if (me == 0)
   t mb, me, mx, count, last;

  uval = value;

  if (uval == 0)
    {
ic lon*errmsg = _("illegal bitmask");
      return insn;
    }

  mb = 0;
 ED,
	     int *invalialue,
	   int dialect ATTRIBUTE_UNUSED,
	   const chaic lon

  /* mb: location of last 0->1 transition */
  /* me: location of l1)
    {
      ret = 0 & 0x1f;
  me = (insn >> 1) & 0x1f;
  if (mb < me + 1ed.  */
  else if ((valt dialect ATTRIBUTE_UNUSED,
	    int *invalid ATT */
 the fields are the
    mx;
	  last = 1;
	}
      else if (!(uval & mask(0x3ff << 1)) == 19 << 1)ction.  */

static unsigned long
insert_de (unsiS for_UNUSED,
	    const cha!= 2 && (count != 0 || ! last))
    *errmsg = _(    *errmsg = _("ignoringigh bit
   is wrapped to the low end.  */

static unsn insn | ((value & nd 2047");
  return insn | ((value << 4) & 0xfff0turn i insn,
	     int dt_de (unsigned long insn,
	    int dialect ATTRIBUTt_mb6 (unsigned long nvalid ATTRIBUTE_UNUSED)
{
  return (insn & 0xfff0) >turn i << 20)) != 0)
    in a DES form instruction.  */

static unsigned lo}

/* The NB field in D)
{
  return ((insn >> 6) & 0x1f) | (insn & 0x20);
}sd.  */
  else if ((valchar **errmsg)
{
  if (value > 8191 || value < -8,
	  **errmsg ATTRIBUTE_UNU mx;
	  last = 1;
	}
      else if (!(uval & mask < 0 || value > 32)
means we want to use
     the old form of the instruc32)
 IBUTE_UNUSED,
	     int!= 2 && (count != 0 || ! last))
    *errmsg = _(xtract_nb (unsigned leturn insn | ((value & 0x1f) << 11);
}

static long
extran insn | ((value & & 0x3ffc) ^ 0x2000) - 0x2000;
}

/* FXM mask in mif (re insn,
	     int dns.  */

static unsigned long
insert_fxm (unsigned  instruction.  This ivalue,
	    int dialect,
	    const char **errmsg)
{
if (re << 20)) != 0)
    e mfocrf and mtocrf insns ensure that exactly
    gnize an instruction w marks it as
   invalid, since we never want to recogu	   int dialect ATTRIBUTE_UNUSED,
	   const char **errmsg)
{
  if (valueTTRI || value > 32)
    *errmsg = _("value out of range");
  if (value == 3sn | (-value & 0xfffreturn insn | ((value & 0x1f) << 11);
}

static long
eTTRIct_nb (unsigned long insn,
	    int dialect ATTRIBUTE_UNUSED,
	    int eturn -(((insn & 0xffUTE_UNUSED,
	     int *invalid)
{
  *invalid = 1;
  returet == 0)
    ret = 32;
  return ret;
}

/* The NSI field in a D form ield uction.  This is the same as the SI
   field, only negated.  The extraned long
insert_ral s marks it as
   invalid, since we never want to recogield an instruction which uses
   a field of this type.  */

static unsignensigned long) value =ield is set.  */
  if ((insn & (1 << 20)) != 0)
     & 0x3e)ic unsigned  long
extracthe taken/not-taken hint here.  *n | (-vield in an lmw instruction, which wer4 compatible targets ustrictions.  */

sM4iple of 2");
 in an X or M form igned long insn,D)
{
  if ((diale	    int dialect ATTRert_ram (unsigned long insn,alue & 0x8000) != 	    int dialect ATTRIBUTE_UNUSED,
	    const c  */
#define Mxff) << 12);
}
n, which has special value
   restr that thlue & 0x1f) << 16);
}

/* The ert_ram (unsigned long insn, instruction, we,
	   t,
	    const char **errnge");
  return insn bits to sele{
  if g
insert_bdm (unsigned long insn,
	    lon,
	    int dialect nsert_raq (unsigned long insn,
	    long valufhe RA field in anFlmw instruction, which has special value
   r
   1))
	*invalid = atic unsigned long
insert_ram (lue == rtvalue)
   0x8000;
}

/* The 	    int dialect ATTRIBUTE_UNUSED,
	    con
   difier is used.
  if ((unsigned long) value >= ing
   store or an updating floatin instruction which is an updating
   store or an   */
#define Mn insn<< 16);
}

/* The RA field in the DQ forlong }
  else
    ng value,
	    int dialeans that the RA
   field mayconst char **erinsert_raq (unsigned long insn,
	    long valulid r bits to seleing poilegal values of a BO field.  */

static ix1f) << 16);
}

/* Tnsert_raq (unsigned long insn,
	    long valucld in a D x14) ==ple ong
extraurn value  == 0x14;
      else
	return an updatinended
   mnion) as like mr.  This operand is marked FAKE.  Theconst cended
   mnem)
    * copies the BT field into the BA field, and bits ion
   functifunction just checks that the fields are the samealue = (insn  long
ext fielrrmsg)
{ This operand ibo (value, dialect)ong insn,
on
   functNUSED,
	    int dialect ATTRIBUTE_UNUSED,
	    constvalue & 0x1f) <UTE_UNUSED,
	  e mr.  Thalue,
	    ibo (unsigned lonrm instr*errmsg ATTRIBUTE_UNUSEct_rbs (unsigned long insn,
	     int t value*errmsg AT FRT SED,
	 ike mr.  Thf (!valid_bo (value, dialect & 0:
	   = ((insn >IBUTE_UNUS1f))
    *invalid = 1;
  return 0;constlwim   inM(2GNED |MPPC_OPERAND_DQ },

RA,RS,SH,MBE,ME)
{
  if (le restrrictions.  */

statiOPERAND_FP long
insert_rtq (uns   value r.",rictioact_ */

static unsigned long
insert_rtq (unsigned lo connst char **errmsg)
{,
	    int dialect ATTRIBUTE_UNUSED,otlwrestriME(21,3ion) aMMBME dialect ATTRIBUTERA, RS, Sue, dialec 1;
lue & 0x1f) << 21);
}SH/* The RS field of the DS foMic unsignedlwinm("targe field */

static unsigned long
insert_rtq (unsigned  long

insert_rsq (unsigne,
	    int dialect ATTRIBUTE_UN | ((valu consx1f) << 2IBUT}

/* The RS field of the ng
in stq instructiomsg)
{
  if ((valuel
   value restrictions.  */

static unsigned l const  form q (unsigned long insn,
	    long value ATTRIBUTE__("targe SH field in an ,
	    int dialect ATTRIBUTE_UNUSED,
long inrict2rt_rsq (unsignM601  int dialeRBsert_rtq (unsigned= _("talue,
	H field in anct ATTRIBUTE_UNUSED,
	    consxfff0)its are fo,
	 field in a POWf (!valid_b6)
  { 5, 6,nt, cons | ((valact_ds,
      4);
}

static long
extr POWER SC  ATTlue & 0x20) >> 4);
}

stati-- PowerPC nt, int USED,
	 ed long insn,
	     int di */
#define((val*errmsx1f)3<< 21);
}

/* The RS field of the DS foRtatic unsigneE_UNUSED,
3rt_rsq (unsigned long insn,
	  USED,
	    const chaE_UNUSElower 5 bits are sto,
	    int dialeUSED,
	    const ch(val_("tar SPR fielvalue & 1) != 0)
    *errmsg = This is flipped--the_("targe3har **errmsg)
{
  if ((value &  5 and vice- versa.  ***errmsg BUTE_UNUSED)
{
 nsert_spr (unsigned long insn,
	    ont dialeOP(24 dialect))
    *er **errmsg)
{
  if or   int dsn,
	   OP The RS field of the DS foUunsigned lori NULL, invalid ATTRIBUTE_,
	    int dieturn ((insn  >> 16) long esn,
5id ATTRIBUTE_UNUSED)
{
  return ((insn >> 16)atic un registers instead 0x3e0);
}

/* Some dialects hx     int sn,
6id ATTRIBUTE_UNUSED)
{
  return ((insn >> ng v    *e	     int dialect, (unsigned long insn,
	     long v long sn,
7 int dialect,
	     const char **errmsg)
{
  /atic u
     as a synonym 0x3e0);
}

/* Some dialects hand **errmsn,
n whATTRIBUTE_UNUSED)
{
  return ((insn >> 
  il_("taue > 7
      || (vk should use that instead.  */
  isODE_BOOKE97
      || (value > 3
	  && (dialect & (PPC_OPuid sprg number");

  /* 0x3e0);
}

/* Some dialects h  lodrestriD(30nemonicMDMstatic lo modifierhe DS form6 stq instruct else must use spr 2Srmsg)
{
  i
  if (value <=MB || (insn 0x10rm insmust use spr 2..279.  */
  if (value <= 3 | ((value & 0x1ing e consust usevalue272..279.  */
  if (value <= 3 || (insn & 0x10dialect,
	      in |= 0x10;

  return insn | ((value & 0x17) <<dialect,
	      inng
extract_sprg (unsigned long insn,
	 & 0x17) <g valect,
	 21);
}2..279
     If not BOOKE or 405,Evalue & 0x17) <rdialect,
	 int di if (val <= 3
      || (val < 0x10 && (i& 0x17) <*errmsust u
	    long
extract_sprg (unsigned long insn,
	     7) <_("tarPPC_OPC    || (val - 0x10 > 3
	  && (dialect then both use o restriust ur 5 b_403)) == 0))
    *invalid = 1;
  return val & 7;   cons
   is  field in an XFX instruction.  This is just like Sing OKE | PSst uant toMDS72..279.  */
  if (value <= is flipped--drm inst
   (indicating  omitted optional operand) ng insn,
	      int}

/* T  (indi
  /* man omitted optional operand) as 268.  This  matter very
   much,4,0'' is not handled correctly.  This 8.  Thisg valuer very >> 11epting any values other than 268 10 && (insn & x100) TB (268)

 accepting any values other than 268  & (PPC_OPCOcmp
/* Th XOPL(< 21nt to CMP field,
      PP	{ OFPR 

/* ic unsignecmporm instsg ATTRIBinvalNUSED)
{
  if odifier= 0)
    value = TB;
  act_boe ATTRIorceCMTTRIBUTE_UN },

 F, L & 0x3e0) << 6);
}

static long
extract_ field, bng
extract_0)
    value = chartwlghis is TOATTR4,TOLGTnt dTOD)
{
  if (value =    value = TB;t long reet;

  ret = ((insn >> 16) &  0x3e0);
}

/* >> 6) & 0x3wllong ret;

  ret = L(insn >> 16) & 0x1f) | ((insn >> 6) & 0x3eed to fform opcodes.  */

/* The maireturn ret;
}

/* Macros ueq(((unsigned long)EQinsn >> 16) S field of the DS>> 6) & 0x3)

/* Thhe main opcode combined withreturn ret;
}

/* Macros usgvalue =;

  ret = (Einsn >> 16) & 0x1f) | ((insn >> 6) & 0x3e0et not a
   instructions.  */
#defireturn ret;
}

/* Macros usTaylor,d long)(to))NLinsn >> 16) & 0x1f) | ((insn >> 6) & 0x3eTaylor, SK)

/* The main opcode combreturn ret;
}

/* Macros usehe trap
   instrucLions.  */
#define OPTO(x,to) (OP (x) | (((<linux/kended mnemonics for
   the c
#define OPTO_MASK (OP_MASK |ba (unSK)

/* The mGin opcode combined with a comparison size ba (uns OPL_MASK OPL (0x3f,1)

/* Areturn ret;
}

/* Macros u0);
  if (ret == TB((insn >> 16) th a trap code in the TO fie0);
  if) | (((unsigned long)(rc)) &(unsigned long)(xop)) & 0x1f#define OPL(x,l) (tions.  */
#de & 1))
#define A_MASK A (0x3flinux/keSK (A_MASK | FRB_MASK)

/* return ret;
}

/* Macros u bit in the L fieldmain opcode coth a trap code in the TO fieTaylor, Cthe FRA and FRC fields fixereturn ret;
}

/* Macros us((((unsigned long)(ed long)(rc)) & 1))
#define A_MASK A (0x3L bit cllear.  */
#define AFRALFRC_Mreturn ret;
}

/* Macros us#define OPL(x,l) (O FRB_MASK)

/* An A_MASK with the FRC fie<linux/kek) (OP (op) | ((((unsigned ASK | FRC_MASK)

/* An A_MASefine A(op, xop, rL (0x3f,1)

/*xed.  */
#define AFRAFRC_MASKba (unsig the BO field.  */
#defineASK | FRC_MASK)

/* An A_MAS#define OPL(x,l) (N FRB_MASK)

/* An A_MASK with the FRC fiextract_dsMASK BBO (0x3f, 0x1f, 1, 1)return ret;
}

/* Macros rint diald long)(to)Ulue & 0x1f) << 21);
}

static long
etr **errc long
	   XPPC_OPERAND_DQ },

TO)
    value = TB;ely.  */rly for the 'at' bitsOPERAND_FPpower4 branch hi   loubfoption"

  rmnemonicX/
#define BBO(op, boT)
    value = TB;sefine OB(((unsigned long) 3) << 2,
	    int AT2_MASK (((unsigneu bitg) 9) << 21)
#define BBOY_MAigned lASK &B)
  ASK)
#definef
}

/*((unsignedinval) 3) << 21)
#define AT2_MASK (((unsigned**errmng the BO field and the coSK  (BBO_MASK &~ Y_MASK)
#define **errm the BO field and the condition bits ofform instruction seoting the BO 14)
	re) 3) << 21)
#define AT2_MASK (((unsignedMASK (lk)) | (((x3f, 0x1f, 0x3, SK  (BBO_MASK &~ Y_MASK)
#define  y bitBBOCB (0x3f, 0x1f, 0x3, 1,& 0x3) << 16))
#define BBOCB_M conf the BO feld and the condition bits of
   the BI fielddefin&~ Y_MASK)
#dB(op, bo, cb, aa, lk) \
  (BBO ((op), (bo), (aaASK ine BBOAT2CB_MASK (BBOCB_MAS)

/* A B form instruRS FRulhdatic ((unsi9ned long) 3) << 21)
  if (vAT2_MASK (((unsignBI_MASefine BBOAConteld and the condtion.  */
#define CTX(o (PPCd("atte

/* An 1 use sprx1f, 0x3, 1, 1)

/* A BBOCB_MASK with tld in an x3f, 0x7)

/* An User ContSK  (BBO_MASK &~ Y_MASK)
#defASK ), (lk)) | (7)

/eld and the condition bits of
   the BI fiea.  */
#dCTX_MASK UCTX(0x3f, 0x1f)
nsigned long)(xop)) & 0x1f))
#defSK &~ Y_MASK175.  */
3f, 0x1f)

/* The main opcode mask with  y bit orm instruction.  */
#definA_MASK (OP_MASK | RA_MASK)

/* A DB_MASK | BIructiX(0x3f, 0x1f)

/* The main opcode mask with ASK in w#define DEO(op, xop) (OP (SK  (BBO_MASK &~ Y_MASK)
#op, xop)wSK)

/* An 1TRIBUTE_Uefine BBOATBI_MASK (Befine CTX(op, xop)w  (OP (op)  (OP K)
#define BBOATBI_MASK (Bion.  */
#definise) ((((ulong
1
/* bit of
#define BISEigned*/
#define CTX(op, ctiolong ry for  but(op, rc) (OP (op) | ((rc) & 1))
#define M_MASK )

/* long
7et y(op, rc) (OP (op) | ((rc) & 1))
#define M_MASK K in wh) | #define M(me)) c) (OP (op) | ((rc) & 1))
#d, C.  */
#definefoC + 1
#XFXM#defi| (((unsiFefinert_rtq (unsign*/
#FX01, 2002, mf((value 
#defiumbeXRAR PPC_OPEands tabq (ulong iRT

/* An M_MASK with the SH andM_MASK | MB1) != ion.  */
#FXM4 */
#definlwarxK &~ YATTR2extraErmsg)
{
  ixff)

/* An0<< 16)Esource  */
#dine MDMD(op, xact_ 'at' bits ution.  */
#dened lfier");

  ctely.  *(op, xlse  'at' bitf (!v|PPCE300ng in & 1))
#define M_MA An MD_MASK wit6h theRd in a  *errms | ((insn >> 6) & */
#dz) << 2)(op, xforce*/

static unsigned MD (0x3f, 0x7, 1*/
#) << 2)  MDSH_MASK (MD_MASK |L form instruction.  */
#defins

/* Th  XRCop, x4nt to t ATTRIBUTE_UNUSED,
	    const char *g, long,(xop)) & 0xf) << 1) | ((r 0x3e0);
}

/* SomDS_MASK MDS (ng val(xop)) & 0xfinval1) | ((rc) & 1))
#define MDS_MASK MDS (_MASK DEOB_MASK (MDS_MASK | MB6ith the MB field fixed.  * TB;
ntlz
/* T#define char **E fields  field of the DS lue = TB;
a)) K &~ Y< 1) | ((lk) & 1))
#defi 0x3e0);
}

/* SASK | (((unsigng vaong)0x3ff)inval 1))
#define SC_MASK (OP_MASK | (((unsigB_MASKform instruction.  */
 unsigned long)1) << 1) | MDS (orm inssigned lo> 21) & omitted optional operand) as 268.  sot mattruction.  */DS_MASK | MB6_Mptional operand) as 268 (PPC_O instruction.  ant to ert_ev2 (unsign,
	    const char *anform instructionRB fiel))

/* The mask for an VA form An M_askdefine p)) & 0 >> 11A_MASK Vct ATTRIBUTEeld fixed.  */
#rm in((unsigned lolue,
	ine VXR(op, xop, rc) (OP (op) | (MD_MASK value =ATTR3extrang insn,
	     int ine MDMB_MASK (M.  */
#dor a VXR for) & 1))
#defif (!valid_b

/* An MDS form  TB;
  

/* Thinsn | (3PCODE_UNUSED)
{
  if (valuvalue & 0x3e0) << 6);
}

rm inst| ((((unsigalue & 0x1f) << 16) | ((value & 0x3e0) << 6);
}

 long)(m((unsixtract_tbr (un
   ned long insn,
	     int diale & 0x1ff) << 1))

/* 

/* An alue,
	    iATTRIBUTE_UNUSED)
{on sK in which t4)

/* An User ContexBBO_MASK &~ Y_MASK)
#defineMASK DEO (0x3form instruction with the RC biform instruction s), (lk)) | (form SK EVSEL(0x3f, 0xff)

/* An M form in(rc) &SK in which tn X form instruction.  */
#defiform instruction sSK &~ Y_MASK4uction.  */
#define BBO_MASK &~ Y_MASK)
#define y bit ASK ZRC (0x3f, 0x1ff, 1)

/* An X_MASruction.  */
#definB_MASK | BIC (0xorm instruction.  */
#define X_MASK XRC (0xASK in  the RB field fixed.  */
#define XRB_BOAT2CB_MASK | ldudefine MDSH_5ASK (MD_MASK | Stion.  */
#deinsnrmsg = _("ofcbshis is MASK  the  MD_MASK withe SH field fixed.  */
#_MASK (  */
#iste(MD_MASK | SH6_MASK)

/* Ask with thee XR_MASK (X0x3 << 21))

/* An X_L form instruction.  */
#definits c0x1f) ATTR
/* An MD_MASK truction.  *ASK & ~((unsigned lon XRARB_MASASK (MD_MASK truction.  */
#desk with the L bunsigue & 0p)) & 5ant to  1))
#define
  if (value ASK | (((unsigt math the RT aruction.  */
#defd.  */
#define XRTR instru BBOAT_Mp)) & 6IBUTE_U))

/* The mask for an VA form instrufine UCTr.  */
#de VXA_MASK VXA(0x3f, 0x3f)

/* An VXR fotd long ret;

  r68t = ((insn >> 16) & 0x
  if (valueanch hints.dsed to form opc#defin  */

/* The main  (X ((op), (xop)) | (((()

/* The main #defide combined wiThis  (X ((op), (xop)) | ((((uthe trap
   in#defineions.  */
#define (X ((op), (xop)) | ((((u| TO_MASK)

/*#definmain opcode combin (X ((op), (xop)) | ((((unong)1) << 22))

/* ics for
   the comform comparison instructiodefine OPL_MAfield
  (0x3f,1)

/* An  (X ((op), (xop)) | ((((0);
  if (ret =#defi((insn >> 16) &efine XTO(op, xop, to) (X ((o#define OPL(x,(((unsions.  */
#defefine XTO(op, xop, to) (X (( bit in the L f#defimain opcode com An X form trap instruction wp), (xop)) | ((((un  */

/* The ma An X form trap instruction wO_MASK (X_MASK | TOics for
   the  form tlb instruction with thefine A(op, xocified (0x3f,1)

/*  form tlb instruction with thO_MASK (X_MASK | TOx3f, 0x1f, 1, op)) | ((((unsigned long)(l))ier is uRB_MASn whstructionefine XTO(o) 1) << 21)
#define BI_MAASK)

/* An73OP (op) | (((unsefine XTO(op/
#define CTX(op, xop) B_MASK | BIMASK,eld and the c EH bit clear.  */
#define f) << 3)
)

/* An X_M5OP (op) | (((unsigned long)(xop)) & 0xff) << 3)
 (X_MASK & ~defiSK EVSEL(0x3f, 0xff)

/* An M form instrudlmz
#defi 1 << 27ong
extrD_MASK with ted. 44
#defrc) (OP (op) | (((r, 0x3((unsigned l7alue,
	 m instruction.  */
#define XFL(op, xop, xff9ftsrf)

/* unsig/* An   { 5, 21<<xop,efine XT{ SRMASK, but withmfme.  */
))
#deforce ME fields MASK | ME_(xop)) & 0x7efine MD(op, 8 the c) (OP (op) MD_MASK MD (0x3f, ong)(xop)) & 0dc#define insn | (8structioMD_MASK wert_d | ((insn >> 6) & 0x| ((& 0x1ff) <<8  inXLe XLRT_MASK (XRT_MASK &p)) & LK VXR(0x3f,b#define MDSH_8  asstruction
#define AT2_Mx3f, 0x7, 1)

/| ((or a VXR fo9define XLRT_MAe L bit clear.  */
#define Xb0x3ff, 1)

/*921))

/* An Xtruction.  */
#define X(op, xop)nep), (aa),efine D4OP (op) |unsigned long)1) <<orm instructine 1) << (xop)) | ((lkeld an)

/* The mask for an XL form instrld fixed.  */| ((tion.  *SK XLLK (0x3f, 0x3ff, 1)

/* An XL fine UCTX_MASK n wh XL_MASK XLLK (0x3f, 0x3ff, 1)

/* xff9fffMASK DEO (0x3e, PPC_OPE | (((unsct ATTRIBU/
#define CTX(op, xopSK in which tg)(bo)eld and the c21))
#define XLO_MASK (XL_MASK form instructiodefine Frm instruction which explicitly sets the ine UCTX_MASK in an A rm instruction which explicitly selong)(rc)inMASK EVSELdefineAion.  */
#define Xunsigned long)(slefine OBLYLK_MA (un */
#define,

  /* TXL form instructionlong) 0x3 <<1SH anddefine XL(op, xop) (OPsk with the L bpopcnt
#deLYLK_Mth tRA fields fi< 1))
#define XK, but with This is 1)

/* A10xf) << 1) | ((The mask for an VAne XRTRA_Mracting < 16))
#define XLOCB_MASK XLOCB (0x3f, 0x1f 0x3, 0x3fftSK in wh6))
#define VXA_MASK VXA(0x3f, 0x3f)

/*, 0x3, 0x3ff,ield fixed.  */
#define XLBB_MASK (XL_MASK | BB_tion.  */
#defin XRARB_MA1   in form instruction.  */
#define X(op, xop)s of _MASK | BB_M */
#define Xunsigned long) 1 << 16))

/* An wrteor a VXR fo1 An X  ME fields *errmsg = _("invaled long)(cb)dcbtstls",MASK wi the 'at' bits usHLKdefine VXR_MASK define AT1_value =K)

/*3the field) 3) << 21)
#define AT2_MASK (((unsignedbit of thnd BB fields fixed.  */
#SK  (BBO_MASK &~ Y_MASK)
#definefe */
#define fieldeld and the condition bits of
   the BI fieldne XO(O(op, xop, oe, rc) \
  (OP (An XO form instruction.  */
#definMASK BBOCB p, otion.  */
#define DSO(op, xop) (OP (op) | ((xs long)()(rc)) & 1))
#define XO_MASK)(oe)) & 1) << 10) | (((unsigned lodefine EVSE1))
# rc) \
  (OP (op) | ((((unsigned long)(xop)) &LLK ((op), (xon XS form instructiL form instruction.  */
#definadd#define c)) & 1gned long) 3) << 21)
#define AT2_MASK (((unsignalinux/kec)) & 1))
#define XS_MASK Xnsigned long)(xop)) & 0x1f))
#de& 0x1ff)) & 1))
#deX(0x3f, 0x1f)

/* The main opcode mask with & 0x1ff)X_MASK | (1 << 11) | (1 << instruction.  */
#define XFXFXM_MDS form instr3(0x3f, 0x1f, 0x3, 1, 1)

/* A BBOCB_MASK with taXO_MASK X ((op), (xop)) | ((((unsig/
#define XFXM(op, xop, fxm, p4) \e XO(op, xop,T2CB_MASK (BBOCB_MASK &~ AT2_MASK)

/* A BBOYCBa (OP (op SPR field filled in.  */
# (X_MASK | RA_MASK | RB_MASK)

/*_MASKe (XL_MAS42),O_MASK | BB_MASK
#define VXR_MASK << 21))
#.  */
#define MMB4fine XL_MM_MASK | MB_MASK {strugned long)ong)(ASK with )

/* An XFXxffand RA ME fields
#define SK (X_illed i  XISEng)(spr the ME_MASK)

/*(value =e SPR field fT_MASK long)1) << 214ASK)
((unsigned long)1) <for an VX fot7) << 2)R fieldet y MASK)

/* An XRT_MAd fiP (op) | ((((unsstwcx((unsigned l15))

/* An X foSK (XRT_M~(0x17 << 16))

/* An X.  */
#definee, dRG_MASK (XSPRt for the
/* An MDS form insst) << 2) | ((re XE_MASK (0xfff,
	    int An X  << 16))

/* An X fne XOnstructioine VXA_MASK Vtruction.  *An X form user cE field.An XL_MASK w5XSPRG_MASK (X XUC_MASK      XUC(0x3f, 0x1f)

/ld of a D
structioigned lne VXR(op, xop, rc) (OP (op) | (((rslqield fixed.  */5fine Z(ned long)(xop)) & 0x3ff))

/* The maslk) & 1))
structios opti0x0)
#define BODNZFP	(0x1)
#define BOD& 0x1ff) )
#define XE_MASK (0xfg)(xop)) & 0x3ff))

/* The maprtyuction.tion. defineop), (lk)) | insn |signed long)(cb)
/* A   indefineARB_MA* The RS firmsg = _("invalspr (unsignunsiSK (#define BASK)

/* A maBB_MASK)

/* An XL_MASK wie BODNZP	**errtion. unsBO_MASK | BB_MASK#define XSPR_MASK (X_MASK |mrc)) &  (0x18) the RL BO and BB fi_MASK & ~(0MTMSRDwith the LK stT_MASK ( (0x188) & 1))
#define MD_MASK An X (0x8)
#define BOtwncodings used ASK (MD_MASK | SH6_MASK)
branch
   mnemon bit _MASK (X_MASKLT	(0)
#define CC(op, xop)  (OP l branch mnemonici

/* Thestructi8xf) << 1) | ((op, xop, rc) (OP "source regcs.  long) 1 << 2OLGTOFP	(0x5)
#define BOFM4	(0x6)"source  BOFP4	()) & 1))
#d1, 0x1f0xc)
#define BOTP	(0xd)
#define BOTM  */
_MASK | BB_9An X form instruction.  *branch
   mnemonics. ubfset no)) & 120)

/* An Uld.  */
#| SH6_MASK)

/* A 1)

/* An set not aNG	(0x14)
#define TONE	(0x1SK  (BBO_MASK &~on.  */
#defizlong)(NG	(0x14)
# XL_MASK XLLK x18)
#define TOU	(0x1f)

/* SmASK (X_MASK  a single line.  */
#uentry in the opcodes table willMASKfit on a swhich explicitlyx18)
#define TOU	(0x1f)

/* SmSK &~ Y_MASKR4 PPC_OPCODE_NOPOWER4DE_PPC | PPC_OPCODE_COMMON
#defi.",NOPOWER4 PPCgle line.  */
#undef	PPC
#define PPC     PPC_h the SPR fiWER6
#define CELL	PPC_entry in the opcodes tigned lset notfit on a(value ine TONE	(0x18)
#define TOU	(0x1f)

/aset not ODE_PPC
#define PPC403	PPC_OPentry in the opcodes tabl| PPCC_OPCODE_32 |signed PPC403	PPC_OPCODE_403
#define PPC405	PPC4SK in which tne PPCVEC	PPC_OPCODE_Aefine PPC750	PPC
#define PPC86MASK BBOCB ne PPC_OPCODE_NOPOWER4 | PPCCOM
#define POWER4	aC_OPCODEne PPCPWR2	PPC_OPCODE_PPC | PDE_POWER | PPC_OPCODE_POWER2
#define BBOAWR2	PPVEC	PPC_OPCODE_ALTIVEC
#define	POWER   PPC_OLLK ((op), (x_OPCODE_POWER | PPC_OPine PPC64   PPC_OPCODE_64 )(rcefine MDSH_M1extraXFL_MASK (XFL (0xCOM32x3ff, 1) | (((unsignstd form instruct21(MDS_MASK | MB6_M_MASK & ~(0x17 << 16))

/* An bdefine MDSH_Mine */
#define XLOCB(optional branch mnemoniccs.  */
1
#definechar **x0)
#define BODNZFP	(0x1)
#define BODDZF	(0x2PCODE_PPC |DZFP	(0x3)
#define BODNZT	(0x8)
#define BODN2	PPC_OPCODE_PPC*/
#define VX_#define BODZTP	(0xb)

#define B4)
#define TOLC_OPDZFP	(0x3)
#define BODNZT	(0x8)
#define BOtbThe BO encod2MASK (MD_MASK  XUC_MASK      XUC(0x3f, 0x1f)

icbloption"DSH_MAm instructionBB_MASK)

/* An XL_MASK with the Bmine TONG	(0xsignine PPC403	PPC_OPCODE_403
#define PPC405	Ps_CACHELELCK
#define PPCCHLK64	PPC_OPCentry in the opcodes table wiml
   fit on ine PVEC	PPC_OPCODE_ALTIVEC
#define	POWER   PP_BOO#define	COM32 of the opcode table FMCI

/* The opcode table.

  ine NOPOWERefinPPCCHLK64	PPC_OPCODE_CACHELCK | PPC_OPCODE_BOOOPCODE_POWER4the instruction opcodthe name of the instruction.
   OPCODE_POWthe ihe opcode table is:

   NAME	     OPCODE	MASKPC_OPCODE_32  must match OPCODE.
  PPC_OPCODE_PPC | PPC_OPCODulruction. essorsASK, but with theruction.  */
#define CTX(op, xopot mattrands.

   T(unsigned long)(xop)) & 0x7))
#define Cr and prdefine PPCPW33)
#define XO_MASK Xch matches, so this table is sor The format ore
(unsigned long)(xop)) & 0x7))
#define CTX_MASKOOKE64
rands.

((lk) & 1))

/* Th PPC_OPCODE_POWER | PPC_OPCOOOKE64
#powerpc_opcode powerpc_opcodeefine PPC750	PPC
#define PPCSK		FLAowerpc_opcodVEC	PPC_OPCODE_ALTIVEC
#define	POWER   PPCSK		FLAGPPC64,		{ RA, SI } },
{ "tdlllgti",  OPTO(2,TOLGT), OPTO_MArted to put mn which explicitlyllti",  OPTO(2,TOLLT), OPTO_MAO_MASK with t, SI } },
{ "tdlgei", ,   OPTO(2,TOEQ), OPTO_MASK,	PP The format , xop, lk) \
  (XL,  OPTO(2,TOLGE), OPTO_MASK,	PP		FLAGS		{ OPE },
{ "tdllei",  OPTtion.
   OPERANDS is the list uction.  */
23define XDSS(op, xop,
#define AT2_MASK (((unsignmuP	(0x1 SI } },
{ "tdgti",   OPTO(2SK  (BBO_MASK &~ Y_MASK)
#def,		{ SK,	PPC64,		{(a)) & 1) << 25))
#dOGT), OPTO_MASK,	PPC64,		{ RA, 		FLAGS		{ OPE(a)) & 1) << 25))
), OPTO_MASK,	PPC64,		{ RA, SI } }rted to put m5)
#define XO_MASK XO (0x3f, 0x1ff, 1, 1)

/* ARA, ; this is usedOLE), OPTO_MASK,	PSK,	PPC64,		{ RA, SI } },
{ "tdlei" The format OLE)eld and the condition bits of
   the BI fie",   ,
{ "tdnli",   0xf)

/* An EVSEL form instruction.  */
#definBRLOCn XRARB_MA2e (uefine BODNZP4 (0x19)
#define BODZM4	(0PCODE_Cine Xefine Pr)) (XL_MASK | Y_MPPC_

/* An XL formT), OPTO#define PPCOM,		{ RA, SI } s taPPC_OP* An XL form insunsignMASK,	PWRCOASK)

/* A mSK (XRefine VXR_MASK VXR(0x3fstbncodings use2

/*fine	MFDEC1	PPC_OPCODE_P(0x8)
#define BODs.  */
##define S& 0x03f))

/* ne TOLLT	(0x2)
#define TOEQ	(00x4)
#deOEQ), OPTO_0x5)
#define TOLNL	(0x5)
#define TOLLE	(0unsignNZM4 (0x12 | RT_MASK)

/.  */
#define VXR_MASK VXR(0x3fOPTO(MASK,	PPCCO 21))

/* An X TOLT	(0x10)
#define TOLE	(0x14)mfdcfine MTOLNL),.  */
#define _("invK & ~(0x1msg = _("offs), X_MASK,		PO6((lk) & 1) instruction which explicitly sets do(((uns OPTO(3,TOLLE)lk)) | ((((unsigned long)(y)) & 1) << "tlleiPC64,		{ RA, S6n which exMASK,	PWRCOM,		{ RA, SI } },
{ "twlngi"#define	COM32NG), lk)) | ((((unsigned long)(y)) & 1) << 21))adned long)3,TOLNGields fixed.  */
#define XLBOBIBB_MASK (XL_MASKca) << 2) |TOGT), OPTO_MASK,	PPCCOM,nsigned long)(xop)) & 0x1f))
#dePCODE_POWER
#d OPTOeld and the condition bits of
   the BI fiecaform i, OPTO_MASK,	PPCCOM,		{ RA, nsigned long)(xop)) & 0x1f))
#de",  OPTO(3,TOLN))
#define XO_MASK XO (0x3f, 0x1ff, 1, 1)

/* AcaxPTO_MASK,	PPCCOM,		{ RA, SI } },
 },
{ "twnli",   OPTO(3,TONL), OP  OPTO(3,TOLNG XS form instruction.  */
#define XS(op, xop, rTO_M	PPCCOM,		{ RA, SI } },
{ "tlti"L form instruction.  */
#defintlbi
    *ion.  *unsiXRTLOM,		{ RA, SI   if (vB, ith the LK mfapi else SI } }
/* T_MASK,	PWRCOM,		{ R PPC_OPCODE_64 lsc_OPCODEction.  *OPCODE_CLASSIC
#define PAT2_MASK (((unsign,	PPCO(3,TOLGE), O7	PPC_OPCODE_SPE
#define 0x1f) << 16) | ((((unsiefine MDSH_M(0x1b)PPC_OPERANDB, GASe VXR_MASK VXR(0x3f,h#define MDSH_Mfine
#define XL(op, xop) (OP (op) | ((((uns **errms.  */
#defiSK or XLYLK_MASK or XLOCB_MASK with the BeqvOF	(0x4)
#defiI } XE_MASK (0xfXA(0x3f, 0x3f)

/* An VXR founsi
#define PPC, 0x1fMASK,	PWRCOM,		{ RA, SI } },
{ "twlnli"SK,	PC440,	{ RT, /
#define XLBH_MASK (XL_MASK |x3f, 0x7, 1)

/OM,		r a VXR form	(0x6)lei",    OPTK (XRT_MOPTO_MASK,,1,0), & 0x1ff) << 405|PPC4BO field and the co),	OP_MASK,	PPCCcid.  */
)

/* A PPC_OPCODE_f, 0xff)

/* An M form instrulhof the BI fie31) & 1))
#defi XLOCB(op, bo, cb, xop, lk) \if (!val,		{ RA, 3C | PPC_OPCODE_The mask for an VA form insxne XLYBB_MASK (X, RB{ "macchw",	XO(4,172,0,0), XO_MASK,	PPC4T, RAn XRARB_MA3d.  */
#defineunsigned long) 1 << 16))

/* An mfexi_COMMOXSPR((unsi3,6unsi0,0),6, 1997, 1h thene MSHME_MASK (exihis op,0), XO_MASK,PC_O405|PPC440,	{ RT, RA, RB } },
{ "mabr0K in wh0), XO_MASK1> 7
O_MASK, PPC405|PPC440,	{ RT, RA, RB }1},
{ "macchwsuo",	XOet y MASK, PPC405|PPC440,	{ RT, RA, RB }2},
{ "macchwsuo",	XPCPMsuo.",	XO(4,204,1,1), XO_MASK, PPC4053PPC440,	{ RT, RA, RBVEC	PMASK, PPC405|PPC440,	{ RT, RA, RB }4PPC440,	{ RT, RA, RBo), (MASK, PPC405|PPC440,	{ RT, RA, RB }5PPC440,	{ RT, RA, RBhe BOMASK, PPC405|PPC440,	{ RT, RA, RB }6PPC440,	{ RT, RA, RB,
{ "MASK, PPC405|PPC440,	{ RT, RA, RB }7PPC440,	{ RT, RA, RB
/* TMASK, PPC405|PPC440,	{ RT, RA, RB eag valueacchwsuo",	X SPR4,0,0),  XO_MASK,	PPC405|PPC440,	{ RTlong)1)  } },
{ "mach4,44,0,0),  XO_MASK,	PPC405|PPC440,	{ ioefine TRT, RA, RB } 6 } },
{ "macchwu",	XO(4,140,0,0), XO_Mdmac} },
RT, RA, RB } 9PC440,	{ RT, RA, RB } },
{ "macchwuo",  XOtMASK,	PPC405|PPC44,	PPC405|PPC440,	{ RT, RA, RB } },
{ "dmadaMASK,	PPC405|PPC44hw.",	XO(4,44,0,1),  XO_MASK,	PPC405|Pdmas,	XO(4,108,0,1), XO4,44,0,0),  XO_MASK,	PPC405|PPC440,	{   XOcMASK,	PPC405|PPC44PC_OB } },
{ "machhwo.",	XO(4,44,1,1),  XO_T, RRT, RA, RB }20A, RB } },
{ "machhwo.",	XO(4,44,1,1),  XOt{ RT, RA, RB } },
T, RA, RB } },
{ "macchwu.",	XO(4,140,ws.",{ RT, RA, RB } },
0,	{ RT, RA, RB } },
{ "machhws",	XO(4,10s_MASK,	PPC405|PPC44405|PPC440,	{ RT, RA, RB } },
{ "machhws.cc{ RT, RA, RB } },
_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{c5|PPC, RA, RB } },
(4,204,1,0), XO_MASK, PPC405|PPC440,	{PPC40
{ "machhwu",	XO(4chwsuo.",	XO(4,204,1,1), XO_MASK, PPC4ws.",
{ "machhwu",	XO(XO(4,2,	PPC405|PPC440,	{ RT, RA, RB } },
{ ",	{ RT, RA, RB } }, RB } },
{ "machhwsu.",	XO(4,76,0,1),  Xcc,	{ RT, RA, RB } }0,	{ RT, RA, RB } },
{ "machhws",	XO(4,108SK,	PRT, RA, RB } }.",	XO(4,108,1,1), XO_MASK,	PPC405|PPC440,t,0,0), XO_MASK,	PP but 	XO(4,12,0,1),  XO_MASK,	PPC405|PPC440,0,0), XO_MASK,	PP,12,0,0),  XO_MASK,	PPC405|PPC440,	{ RT, s"maclhwo",	XO(4,428wu.",	XO(4,12,0,1),  XO_MASK,	PPC405|PPC4cc,0,0), XO_MASK,	PL (0x"machhwuo",	XO(4,12,1,0),  XO_MASK,	PPsu.",	XO(4,204,0,2
	  clhws",	XO(4,492,0,0), XO_MASK,	PPC4ASK with the 3MASK (MD_MASK fields fixed.  */
T, SPsn,
	  "twlniM,		{ TO,ands.3TTRIBUTE_UMASK,	PPCCOM,		{ RA, SI } },
{ "tlngii{ TO, RA,SK,	PPC405| OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twlnivO_MASK with tPC40, OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tlngiivRCOM,		{ RA, ,0,0),lk)) | ((((unsigned long)(y)) & 1) << 21))
fpmong)1) << 23K | BO_MASK | BB_PMd the cT, PMRK (X_MASK fmd of a D|PPC440,	{ 3 >> 11) &K,	PPC405|Pct ATTne MSHME_MASK (xhis oper, RA, RB } },
IBUTE_UK,	PPC405|P#define MSHME_MASK (rtcatic unsRA, RB } },
	PPC4 } },
{ "maclhwu",	XO(4,396,0,0), XO((unsigneA, RB } },
ne M(op },
{ "maclhwu",	XO(4,396,0,0),deFRA FLM +XO_MASK,	PPC), XOclhwsuo.",	XO(FDEC460,1,1), XO_MASK,wuo",	XO(4,396,1,0), XObo), ,	PPC405|PPC440,	{ },
{ MSHME_MASK (XM field 4,396,1,0), XO(4,2PPC440,	{ RT, RA, RB } },
{ "maclh("offset 4,396,1,0), XOine MME },
{ "maclhwu",	XO(4,396,0,0),tform insPC440,	{ RT, RA
/* AnK,	PPC405|PP,

  /, RA, RB } },
{ s4,204,0PC440,	{ RT, RA4,168,440,	{ RT, RA, RB } },
{ "maclhw, RA, R,	{ RT, RA, RB }ine MM },
{ "maclhwu",	XO(4,396,0,0),sd} },
{ ".",	XO(4,396,1,	{ RTwu",	XRC(4,136,0),  X_MASK,	PPC40sdRT, RA, .",	XO(4,396,1ne M(oB } },
{ "mulhhw",	XRC(4,40,0), r X_MASK,	PPC405|PPC440_MASK,,	{ RT, RA, RB } },
{ "mulhhwu",	X1),   X_MASK,	PPC405|"mulchwu",	XRC(4,#define MSHME_MASK (MfK,	PPC405",	XO(4,396,1 },
{ "mulchwu.",) != 0)
40,	{ RT, RA, Rp0,	{ RT, RA, RB } },
{4 },
{ "mulchwu.", _("in  X_PC440,	{ RT, RA, RB } },
{ "mullhw.",	9},
{ clhws",	XO(4,492,0,0), XO_MASK,	PPCc,	XRC(4,8,XO_MASK,	PPC4RC(4,424,1),  X_MASK,	PPC405|PPC440,	{ RT,{ RTT, RA, RB } },
{PPC4 RA, RB } },
{ "mASK,	PPC405|PPC440,	{ RT,dT, RA, R4,396,1,0), XO_IBUTE_hw",	XO(4,174,0,0), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } 9 in C405|PPC440,	{ RT, RA, RB } },
{ "mac_COMMON 4,396,1,0), XO_,1), XO_MASK,	PPCASK,	PPC405|PPC440,	{ RT,ASK,	PPC405|PPC440,	{ R98on jusmachhwo",	XO(4,44,1,0),  XO_MASK,	vp},
{ "mullhw",	XRC(4ARB_u.",	XRC(4,392,1),  X_MASK,	PPC405|PPC44mpBO_MASK 440,	{ RT, RAhhw."T, RA, RB } },
86#definnmacchws.",	XO(
#define ), XO_MASK,	P	XRC(4,392,0),  X_M RA, RB } },
{ "nmacchwf) << 11)), XO_MASK,	P), XO_MASK, PPC405| RA, RB } },
{ "nmacchwned long)), XO_MASK,	P
/* A, XO_MASK,	PPC405|PPC440,	{ RT, RAonly 278,0,1), XO_MASK,	P4,168, XO_MASK,	PPC405|PPC440,	{ RT, RAdPPC405|PPC440,	{ RT, RA XSPR	XO(4,46,0,1),  XO_MASK,	PPC405|PPCcoun  { 3,  "nmachhw",	Xvaluechhwo",	XO(4,46,1,0),  XO_MASK,	PPC405|P3ff, 1){ RT, RA, RB  } },
{ "nmacchwo", RA, RB } },
{ "nmacchw6), X_MAS{ RT, RA, RB ,1), RB } },
{ "nmachhws",	XO(4,110,0,0), uction wh{ RT, RA, RB |PPC4RB } },
{ "nmachhws",	XO(4,110,0,0), p), (aa),{ RT, RA, RB PC405|PPC440,	{ RT, RA, RB } },
{ "nmacchwh,238,0,1), XO_MASK,	 21)PC440,	{ RT, RA, RB } },
{ "nmachhwst chaT, RA,0,1,0), XO_MAS,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RAt cha|PPC440,1,0), XO_MAS(4,46,0,0),  XO_MASK,	PPC405|PPC440,	{ RT /* The ,
{ "nmaclhw.",w.",	XO(4,46,0,1),  XO_MASK,	PPC405|PPCbK,	PPC405|PPC440,	{ RT, "nmacRB } },
{ "nmachhws",	XO(4,110,0,0vrsaNUSED)_MASK,	PPC405|P), XO_MASK, PPC405|VEC|PPC440,	{ RT, RAusprg },
{ 440,	{ RT, RA, RB } },
{ "nmacASK,	PPC405|PPC440,	{ RT,	PPC405,	PPC405|P7act_p, rc) (OPO_MACLASSI,	XO(4,, TBsn,
	     ,0,1), XO_MASK,    X_MASK,	w.",	XO(4,46,0,1{ "nmaclhws.",	XO(4,494,0,1(0x3f, 0xf0), XO_M7_MASw.",	XO(4,46,0,1A, RB } },
{  "nmaclhwso.",	XO(4,494,1,1), X	PPC405|PPC440,	{ RT, RA, RB } },
{ "nmaclhwso."atic uns94,1,1), XO_MAS"nmachhwo",	XO(4,	{ RT, RA, RB } },
{ "mfv 1604), VX_MASK,	PASK,	"nmachhwo",	XO(4ASK,	PPC405|PPC440,	{ RT,_MAS),   X_MASK,	PPC405|P), XO_MASG6, 1997, 1"maclhwso",Gove them before },
{ "macchwsuo),   ,	{ RT, RA, RB } },"mulhhw.",	XRC(4,4prg{ RT, RA, RB } },
{275|PPC440,	{ RT, RA,B } },
{ "efdabs",   |PPC440,	{ RT, RASK,	PC405|PPC440,	{ RT,B } },
{ "efdabs",   K,	PPC405|PPC440ASK,	), XO_MASK,	PPC405|B } },
{ "efdabs",   1), XO_MASK,	PPCASK,	} },
{ "nmachhwo.",40dq (},
{ "maclhove them beforeO(4,140,1,0), XOASK,	 } },
{ "nmacchwo",	Xefdsub",   VX(4, 737), VX_MASKcchwuo.",	XO(4,1ASK,	,	{ RT, RA, RB } },   VX(4, 744), VX_MASK,	PPCEFS,	 } },
{ "machhw"ASK,	5|PPC440,	{ RT, RA,   VX(4, 744), VX_MASK,	PPCE5|PPC44 "mullhw",	XRC(4,4} },
{ "nmachhwo.",ion.  */(4,174,1,1),K,	PPC405|PPC440,	{ RT2efin{ RS, RA } },
{ "efdadd",   VX(4,pi },
{ "efdcmpeq", VX(4,RT, RA, RB } },
{ "nmaclhws.",	XO(4,494,0pv },
{ "efdcmpeq", VX(4,(4,46,0,0),  XO_MAS),  X_MASK,	PPC405b },
{ "ef XO_MASK,	PP3,  XOnmacchw.",	XO(4,174,0,1), XO_MASK,	PPC RB } },
{ "efdtsteq",104,12,0,0),  XO_MASK,	PPC405|PPC440,	{ RTbO_MASK,
{ "efdtsteq", V05|PPC440,	{ RT, RA, RB } },
{ "nmaclhwso,
{ "efdcfsid", VX(4, 710},
{ "machhwuo",	XO(45FS,		{ RS, RB } },
{ { RT, RA, RB } },
{3hwu."X_MASK,	PPCEFS,		{ RS, RB } },
{ "efdcfui{ RT, RA, RB } },
{9,	XO(4,430,0,1), XO_RB } },
{ "efdcfuid", VX4, 741), VX_MASK,	P	XO(4,nmacchw.",	XO(4,174,0,1), XO_MASK,	PPiauo.",,	PPCEFS,		{ RS, R 750), VX_MASK,	i",  VX(4, 757), VX_MASK,	PPCEFS,		{ RS, RB } },10C440,	{ RT, RA, RB } },
{ "maclhw",	XO(iaXO_MA,	PPCEFS,		{ RS, R5|PPC440,	{ RT, i",  VX(4, 757), VX_MASK,	PP|PPC440 } },
{ "efdctsi,	PPC405|PPC440,	{ RT, RA, RB } },
{ "ia RT, ,	PPCEFS,		{ RS, RX(4, 766), VX_MASK,	PPCEFS,		{ CRFD, RA, fdctuiz", VX(4, 760), V9hw.",	XO(4,46,0,1), RB } },
{ "efdcfuid"iac1), XO_PCEFS,		{ RS, R), XO_MASK,	PPC4 RS, RB } },
{ "efdctsf",  V58), VX_MASK,	PPCEFS9 "nmachhwo",	XO(4,46RB } },
{ "efdcfuid",PPCEFS,		{ RS, RB } },
{RT, RA, RB } },
{ "nmaclhws.",	XO(4,494,0odes.  */

{ "vaddcuw",10_MASKK,	PPC405|PPC440,	{ RT, RA, RB } }, RB } },
{ "efdctui",  V(4,46,0,0),  XO_ASK,	PPCVEC,		{ VD, VA, VB } "efdctuidz",VX(4, 746)8,1,0), XO_MASK,	PPC405|PPC440,	{ RT, RvPCEFS,		{ RS, RB } },
{), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efdc "vaddsws", VX(4,  8969 } },
{ "nmachhwo.",RB } },
{ "efdcfuid",v{ "vaddshs", VX(4,  8	PPC4_MASK,	PPCVEC,		{ VD, VA, VB } },
{ "vadd "efdctuidz",VX(4, 79, RA, RB } },
{ "nmaRB } },
{ "efdcfuid"E_COMMON X_MASK,	PPCEFS,3RT, RA, RB } },
{ "nmaclhws.",	XO(4,494,0, XO_MASK,	PPC405|PPC440,	PPC405|PPC440,	{ RT, RA, RB } },
{ "mad in exceX_MASK,	PPCEFS,4B } },
{ "efdctsi",  VX(4, 757), VX_MASK,,  640), VX_MASK,	PPCVE9X(4, 764), VX_MAS},
{ "nmacchws",	XO(4,238o} },
{ "macchwsuo",	X
{ "m,	PPCEFS,		{ RS, RB } },
{ "efdctsf", vo), VX_MASK,	PPCEFS,	4A, RB, VB } },
{ "vavgsb",  VX(4, 1282), VX_MA4, 741), VX_MASK,	Piple o, VB } },
{ "vavgsb",  VX(4, 1282), VX_MA742), VX_MASK,	PPCE4PPC40, VB } },
{ "vavgsb",  VX(4, 1282), VX_MA), VX_MASK,	PPCEFS,4VX(4, 766), VX_MASK,	PPCEFS,		{ CRFD, RA, X_MA,	PPCEFS,		{ RS, RA40	{ RS, RB } },
{ "efdcfs",   VX(4, 751), Vvoacchwuo.",	XO(4,1 "vavRT, RA, RB } },
{ "nmaclhws.",	XO(4,494,0D, V RB } },
{ "efdcmpg40 768), VX_MASK,	PPCVEC,		{ VD, VA, VB } }D, V8MM } },
{ "vcfux",  05|PPC440,	{ RT, RA, RB } },
{ "nmaclhwsoD, V9MM } },
{ "vcfux",  ASK,	PPCVEC,		{ VD, VA, VB } },
{ "vadduhX_MASK,	PPC405|PPC440,	4RB } },
{ "efdctsi",  VX(4, 757), VX_MASK,	_MAS
{ "nmaclhw",	XO(44 RA, R(4, 966, 1), VXR_MASK, PPCVEC,	{ VD, VA, B } },
{ "nmaclhw.4{ "efdctsidz",VX(4, 747), VX_MASK,	PPCEFS,	_MASK,	PPC },
{ "vcmpequ,
{ "vavgub",  VX(4, 1026), VX_MASK,	PPCVEC,	11), XO },
{ "vcmpequ"vavguh",  VX(4, 1090), VX_MASK,	PPCVEC,		{ V1O(4,14 },
{ "vcmpequ	{ RS, RB } },
{ "efdcfs",   VX(4, 751), spefsefine XO_MASK,	PPC4{ "efdctsidz",VX(PPCSP   VX(4, 737), VX_bRT, RA, R VB } },
{ "vcm5|PPC440,	{ RT, RA,BRL RT, VXR_MASK, PPCVECt	{ VD, VA, VB } },
{ "PC405|PPC440,	{ RT,134, 1), VXR_MASK, PPCPPCVEB } },
{ "nmaclhw.5,424,0",  VXR(4, 134, 0), VXR_MASK, PPCVPPCVEA, VB } },
{ "vcmp5cchwsmpgefp.", VXR(4, 454, 1), VXR_MASK, PPCVVA, VB } },
{ "vcm5B } }mpgefp.", VXR(4, 454, 1), VXR_MASK, PPCV VA, VB } },
{ "vc5RT, Rmpgefp.", VXR(4,1,0), 1), VXR_MASK, batatic un VA, VB } },
{ "vcmpgefBld in a B fo"maclhwso",BA4, 774, 0), VXR_T, RA, RB } },
{ "n },
{ "vcmp,
{ "vcmpgtsb.", VXR(4, 774, 1), VXR_MdXR_MASK, PPCVEC,	{ VD, PPCVEC,		{vcmpgtsh",  VXR(4, 838, 0), VXR_MASK, PPCV PPCVEC,	{ VD, VA, V3(4,46,0,0,
{ "vcmpgtsb.", VXR(4, 774, 1), VXR_MAc_c clear. , RB } },
{ 	{ RS, RA, RB } },
{K,	PPC405|PPC440,	{ RT_adC405|PPC440,	{ cmpgt RA, RB } },
{ "nmachhws",	XO(4,110,0,0ic_daA, VB } },
{ "vcmpgt 750), VX_MASK,	PPC XO_MASK,	PPC405|PPC4D, VA, VB } },
{ "vcmpgtw.",	XO(4,46,0,1),  XO_MASK,	PPC405|PPC4{ VD, VA, VB } },
{ "vcmASK,	PPCVEC,		{ 4, 518, 1), VXR_MASK, Pm{ RT, RA, } },
{ "vcmpg7} },
{ "vcmpgtfp.",n,
	  EC,		{ VD, VA, VBC,	{ VD, VA, VB } },
{ B } },
{ "vcmpgtuw",XR(4, 582, 1), VXR_MASK, 
{ "nmaclhw",	XO(45405|},
{ "vcmpgtuw",  VXR(4, 646, 0), VXR_ASK,,0,1), XO_MASK,	PPC4SK,	PPCEFS,		{ RS, R",    VX(4,  970), VX_MAC440,	{ RT, RA, RB } 5	PPCEFS,		{ RS, RA }  VXR(4, 646, 0), VXR_MpD, VA, XO_MASK,	PPC405|} },
{ "nmachhwo.",	XO(4,46,1,1),  XO_Md RB } },
{_MASK,	PPC405|pgtub",  VXR(4, 518, 0), VXR_MASK, PPCVEmB } },
		{ VD, VB } },
w.",	XO(4,46,0,1),  XO_MASK,	PPC405|PPCmi_ong valueB } },
{ "v7EC,		{ VD, VA, VB }  VX(4, 1034), VX_MASK,	tional br{ VD, VA, VB ,1,1), XO_MASK,	PPC405|PPC440,	{ RT, RAmi_ep_MASK		{ VD, VA, VB (4,46,0,0),  XO_MASK,	PPC405|PPC440,	{ mi_twoption"{ VD, VA, VB },
{ "vcmpgtuh.", VXR(4, 582, 1), VXR_MAi_rPCVEC,		{ VD, VA, VB9} },
{ "nmachhwo.",	XO(4,46,1,1),  XO_Mmd	PPCVEC,		{ VD, VA, VB40,	{
{ "vmaxuh",    VX(4,   66), VX_MASK_casform i	{ VD, VA, VB 5|PPC440,	{ RT, RA, RB } },
{ "nmachhwsmd	PPCVEC,		{ VD, VA, VBO_MAS
{ "vmhaddshs", VXA(4,  32), VXA_MASK,PPCVEC,		{ VD, VA, VB08,1,
{ "vmhaddshs", VXA(4,  32), VXA_MASK,twPPC405|PPC440,	{ R7o.",	B, VC } },
{ "vminfp",    VX(4, 1098), VCVEC,		{ VD, VA, VB9} },
{ "vmaxsw",    VX(4,  386), VX_MASKd	PPCVEC,		{ VD, VA, VB } },
{ "vmaxfp",    VX(4, 1034), VX_MASK_  Similar,	PPCVEC,		{ V} },
{ "vmaxub",    VX(4,    2), VX_MASK,	dbcalongulchw",	XRC(4VX(4,  384), VX_M "vminub",    VX(4,  514), Vram0",,	PPCVEC,		{ V} },
{ "vmaxsw",    VX(4,  386), VX_MASK,	 VX_M1SK,	PPCVEC,		{ V} },
{ "vmaxfp",    VX(4, 1034), VX_MASKd, VX_MASK,	PPCVEC,		{ 0,	{ R, VB } },
{ "vmladduhm", VXA(4,  34), VX_MASK,	PPCVEC,		{ |PPC44A, VB, VC } },
{ "vmrghb",    VX(4,   12),ASK,	PPCVEC,		{ ,	PPC4{ "vmaxsb",    VX(4,  258), VX_MASumm{ "efdcfK,	PPCVEC,		{PPCVEC,		{ VD, VAPPC750EC,	 "vmrghw",    VXpmPCEFS,		 VX_MASK,	PPCV(4,46,0,0),  XO_MAS },
{ "vmrglb",    VX(4,  4, 741), VX_MASK,	P9 } },
{ "vmaxfp",    },
{ "vmrglb",    VX(4si4,238,0,1), XO_MASK,93},
{ "vcmpgtuh.", V },
{ "vmrglb",    VX(4(4, 
{ "nmaclhw",	XO(49,		{ VD, VA, VB }} },
{ "vmrglh",    VX(4,  33742), VX_MASK,	PPCE94, VB } },
{ "vctsxs },
{ "vmrglb",    VX(4,  ), VX_MASK,	PPCEFS,9r)) VD, VA, VB, VC } },
{ "vmsumshs",  VXAz,0,0), 	XA_MASK,	PPCVE	PPC405|PPC440,	{ RT, RA, RB } },
{ "mac{ "efdc36), VXA_MASK,  RS, RB } },
{ "efdcfuf",  VX(4, 754), (4,  1436), VXA_MASK,0,	{ RT, RA, RB } }, },
PPC440,	{ RT, RA,  268msumuhs",  VXA(5|PPC440,	{ RT, RA,  PPCVEC,		{ VD, VA,skewiC } },
{ "vmulesb",   VX(4,  776),T, RA, RB } },
{ "ma 332),msumuhs",  VXA(, VC } },
{ "vmhrad  PPCVEC,		{ VD, VA,dcwK,	PVD, VA, VB } },
{ "vmuleub",   VX(T, RA, RB } },
{ "ma96), msumuhs",  VXA(), XO_MASK,	PPC405| VX_MASK,	PPCVEC,		{ ne EVVEC,		{ VD, VA, VB } },
{ "vmulosb	{ VD, VA, VB, VC } },
{, VC } },
{ "vmuleEC,		{ VD, VA, VB } },
,  584), VX_MASK,u0SK,	PPCVEC,		{ VD,), XO_MASK, PPC405|PP } },
{ "efdcfuid" 40), msumuhs",  VXA(,		{ VD, VA, VB } },
{ vmulouh",   VX(4,  472), VX_MASK,	PPC	{ VD, VA, VB } },
{ "v, VXR_MASK, PPCVECdbD, VA, VB } },
{ "v9fine M, RA, RB } },
{ "no.",	XO(4,174,1,1),8,0,0), XO_MASK,	PPC409, 750), VX_MASK,	PPC, VB, VC } },
{ "vmsu,
{ 41), VXA_MASK,	PPCVT	(0lesh",   VX(4,  840), VX_MASK,	PPCVECi.  */
#de, 1156), VX_MA RS, RB } },
{ "efdcf
{ "vadduws", VX(4,bh   int di 1156), VX_MACEFS,		{ RS, RB } },
D, VA, VB } },
{ "vpe y bit   VX(4,  398), B } },
  /* End of do(4,  584), VX_MASK,r	PPCVEC,  VX(4,  398), } },
{ "vmaxuh",    VD, VA, VB } },
{ "vpkK,	PPC4  VX(4,  462),  } },
{ "nmacchwo",	XO(4,174,1,0), XO_Ml2((value & ddfp",  VX(4, _MASK,	PPC405|PPC4 },
{ "vmrglb",    VX(dckuhum",   VX(4,   14), V(4,204,1,0), XO_MASK, PPC405|PPC440,	{iuhus",   VX(4,  142), VX_chwsuo.",	XO(4,204,1,1), XO_MASK, PPC4ict"nmacchwso.",	XO(4,23X_MASK,	PPCVEC,		{ V },
{ "vmrglb",    VX(pb,
{ "nm   VX(4,   14),  "maclhws",	XO(4,492,0,0), XO_MASK,	PPCthr, VX VX(4,  266), VX_MASK,	PPCVEC,		{ VD,D, VA, VB } },
{ "vrefp"u     VX(4,  266), VX_MAT, RA, RB } },
{ "macchwu.",	XO(4,140,    4, 741), VX_MASK,	PVEC,		{ VD, VB } },
{D, VA, VB } },
{ "vrefp", "efdctuidz",VX(4, 746bo), (VD, VB } },
{ "vrfip",     VX(4,  650742), VX_MASK,	PPCE, VB } },
{ "vrlb",  } },
{ "vrfin",     VX(4,_MASK,	PPCVEC,		{ VD, V,	PPC405|PPC440,	{ RT, RA, RB } },
{ "s,0,0), XO
{ "macl, SIPC440,	{ RT, RAdefine Mwso",	XO(4,492,lwPTO(3,TOf) << 		{  1))
#define MD_MASK MD (0x3f, 0x7, 1)

/d clear.  XDSSVEC,		igned lDSting any vas",	Xm instructiSTR01, 2002, dstCCOM,		{VEC,		{ VD,VEC	PVB, VC } },
{ "vsl",       VX(4,  452),,236,1MASK,	PPCVEC,		ASK (MD_MASK L(op, xop) (OP (op) | ((((uns	PPCXO_MASK,	PPCe, di form instruction.  */
#define X(op, xop)dsTOLLT),CVEC,		{ V7xf) << VB, VC } },
{ "vsl",       VX(4,  452), VX_MAMASK,	PSK,	PPCVEC,	VA, VB } },
{ "vslb",      VX(4,  260), VX_MASK,dccong
insy for #define XLRT_MASK ion.  */
#define XF,
{ "twgti",bract_boe su.",	
#deine PPC403	PPC_O",   OPTO(3,TON| PPC_OPCOb "tdlti,		{ VD, VB, VEC	PPC_OPCODE_plth",    VX(4,  588), VX_Msu",	XO(4,460,1))
 UIMM } },
{ "vsplth",    VX(4,  588), VX_M "maclhwsu.",	EC,		 VB, UIMM } },
{ "vspltisb",  VX(4,  } },
{ SI } },
{ "t36ASK, but with the405|PPC440,	{ RT, RA, RB } },
{ "tdlti",   OCVEC,	,	PPC405|PPC440,	{ RT, RA, RB } },
{ "maclhw  OPTO(2,TONGCVEC), XO_MASK,	PPC405|PPC440,	{ RT, RA, RB } },
{P_MASK,	PPC64PPCVEClk)) | ((((unsigned long)(y)) & 1) << 21))OM,	PC440,	PPCVEC
	    const charRA, RB }ong value,
lwaRA, RB } },
{	PPCT_MASK)

/* An XRT_MASK mask with the L blh     VX(4,  900#define	MFDEC1	PPC_OPCD, VA, VB } },
{ "vsrb", , XO_MASK,	T	(0)
#define 	{ RT, RA, RB } },
{ "macchwsu",	t OPTO(3,TOLNL340,	{ RT, RA, RB } (((unsigned long)(cb)dRLOCK
#define 3 } }BO_MASK | BB_MASK)

/* An XL_MASK with the BO61), 		{ VD,9ne PPCCHLK#define XUC_MASK   instruction.  */
#defin64oPCODE_PO"vsu, OPTO_MASK,	PPtruction.  */
#de) | (((unsigned lo VB }} },
{ "vLLE), OPTO_MASK,	 74), VX_MASK,	PPCVEC,		{ V, VA, VB ine NOPOWEubsbfp",    VX(4,   74), VX_MASK,	PPCVEC,		{ VD, V, VB , XO_MASK,	VD, efine BODNZP4 (0x19)
#define BODZM4	(0x1a)slbmAn XRARB_MAiple oXL_MASK | Y_MASK)

/* An XL form inssthASK,	PPCVEC,   VXfine	MFDEC1	PPC_OPCODE_POWER
#define	MFchwso",	XO  */
#  VX },
{ "tneine BOTP	(0xd)
#d VB } },
{ "vsrfqASK,	PPCVEC,7	(0xc)
#definehm", { "twlF,	PPC405|PPC440,	{ RTfdpine MD(op, , VB } },
{ "vsubuhsinsn |X(4, 1600), VX_MASK,	PPqncodings use8MASK (MD_MASK |buhs",   VX(4, 1600), VX_MASK,	stVEC,		{ uses td.  */
#define "vsubuws",   (OP (op) | (((unsignPCVEC,		,		{ VD, VA, VB } },
{ "vwm",   V   VX(4, 1928), VX_MASPPCVEC,uses thVB } },
{ "vsubuhs",   VX( (OP (op) | (((unsiff0);
}
,		{ RA, 41DNZF	(0x0)
#deThe mask for an VA form insorfine UC	PPCVEC,		{ { "macchw",	XO(4,172,0,0), XO_MASK,	PPC4sra else  XC,		{41e BODZTting any values other than 3 || (insn  1544O(3,TOMASK,	PPCT, RA,..279.  */
  if (value <= 3 || (in,		{ VD,XO_MASK,	PPmpequc)
#define TOLT	(0x10)
#defl branch mnemonic, XO_MASK,	PPRIBUTPPC4L_MASK | Y_MASK)

/*, RA, RB } },o{ "macchws",4M,		{ RA, SI } },rc) (OP (op) | ((((unsignethncodings use4, VA,SK,	PPCCOM,		{ RA, SI } },
{ "teqi",   ,  6} },
{ "vsuTO_MASK,	PPCCOPCCOM,		{ RA, SI } },
{ "tlnli", SK,	ASK,	PPCVEC,	 XFX XLOCB_MASK XLOCB (0x3f, 0x1f, 0x3, 0x3PPC440,	{VX_MASK,	PPCVEC,		{ VD, VA, VB } },

{ "evlhwso",	XOe XLYBB_VX_MASK,	PPCefine XLBB_MASK (XL_MASK | BB_MASK)
#defne XLYBB_514), VX_MASK,	PPCSPE,		{ RS, RB, UIMM } }truction witO(4,204,0,0), XO_451K,	PPC405|PPC440,	{ RT, RA, Rne XRTRA_Mubw",wsu.",	XO(4,20, VX_), XO_MASK, PPC405|PPC440,	 } },
{ "evs } },
{ "macchwsu, VXXO(4,204,1,0), XO_MASK, PPC4, RB } },
{ "eVX(4,  714), VX, 518)chwsuo.",	XO(4,204,1,1), X, RB } },
{ "e|PPC440,	{ RT, , 518B } },
{ "macchwu",	XO(4,14, RB } },
{ "eK,	PPC405|PPC44(4, 52T, RA, RB } },
{ "macchwu., RB } },
{ "e1), XO_MASK,	PP(4, 52B } },
{ "vrlb",      VX(4, RB } },
{ "eO(4,140,1,0), X(4, 52,	PPC405|PPC440,	{ RT, RA,, RB } },
{ "ecchwuo.",	XO(4,(4, 52B } },
{ "maclhws.",	XO(4,, RB } },
{ "e } },
{ "machhw(4, 524,44,0,0),  XO_MASK,	PPC40, RB } },
{ "T, RA, RB } },
{, 518hhw.",	XO(4,44,0,1),  XO_MA
{ "brinc",   440,	{ RT, RA, X_MASKK,	PPCSPE,		{ RS, RA } },

{ "brinc", 	PPC405|PPC440,	{, 518RA, RB } },
{ "machhwo.",	X } },
{ "evs  XO_MASK,	PPC405, 51840,	{ RT, RA, RB } },
{ "maRB } },
{ "evmr",0,0), XO_MAS535), _MASK,	PPCSPE,		{ RS, RA } },
{ "evcntws.",	XO(4,108,0,535), ,	PPCSPE,		{ RS, RA, RB } },

{ "evand,
{ "machhwso",	X535), K,	PPCSPE,		{ RS, RA } },

{ "brinc", RA, RB } },
{ "ma535), .",	XO(4,108,1,1), XO_MASKRB } },
{ "evmr",{ RT, RA, RB, VX,
{ "machhwsu",	XO(4,76,0,0), BBA } },
{ "evo,
{ "evnand",    V, VX_MASK,	PPCSPE,		{ RS, RA } },
{ "e  XO_MASK,	PPC405,    VVX_MASK,	PPCSPE,		{ RS, RA, BBA } },
{ "e76,1,0),  XO_M,    V), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
{ suo.",	XO(4,76,    V39), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
},
{ "machhwu",    V, VX_MASK,	PPCSPE,		{ RS, RB, UIMM } }T, RA, RB } },
{ ,    V520), VX_MASK,	PPCSPE,		{ RS, RA } },
PC440,	{ RT, RA, ,    },
{ "machhwuo",	XO(4,12,1,S, RA, RB } },
{ E,		{ RS, RA, EVUI VX(4, 536), VX_MASK,	PPCSPE,		{ RS, RA,  XO_MASK,	PPC4, EVUIVX_MASK,	PPCSPE,		{ RS, RA, BBA } },
{ "ev8,0,0), XO_MA, EVUI4, 537), VX_MASK,	PPCSPE,		{ RS, RA, RB } ,	XO(4,428,0,, EVUI_MASK,	PPC405|PPC440,	{ RT), VX_MASK,	PPCSP, VX_MASK,	PPCSPE,{ "evslw",     VX(4, 548), VX_MASK,	PPCSPRB } },
{ "mac, EVUI,
{ "evslwi",    VX(4, 550), VX_MASK,	PPC{ RT, RA, RB },     "maclhws",	XO(4,492,0,0), S, RA, RB } },
{w",  VX(4, 518),  RB } },
{ "maclhws.",	XO(4,), VX_MASK,	PASK with the 4	{ VD, VA, VBdefine BODNZ	(0x10SPODE_POWER | PPC_ will VB  the RB5ields fixelei",  OPtruction.  */
#deodes table willVB } },
{ " VX_		{ VD, SIMM } },RS, RA, RB } },

{ "evc } },
{ASK)



/* An 5(bo)) & 0x1f) << nstructions before more genertu",  with the RB VX_M instruction which matches, so this table tu", MASK BBOCB ), Ve
   specific instructions before more generpltu",efine BBOA2), VX(unsigned long)(xop)) & 0x7))
#define CTX_MASK",VX(4VX(4, 564gned long)CSPE,		{ RS, RA, RB } },

{ "evcmpgel",   (4, 561), VBO field rCSPE,		{ CRFD, RA, RB } },
{ "evcmpgtu"SEL(opVX(4, 564Context form instrucCRFD, RA, RB } },
{ "evcmpe
#defi{ "evlddx",  orm instruction.  */
#define X_MASK XRC , RB ,  VX(4, 562)9(0x3f, 0x1ff, 1)

/* An X_MASK with the RA VUIMM_   VX(4, 564 "evSK EVSEL(0x3f, 0xff)

/* An M form instrumt440,	{ RT,6), VX_MA6> 21) aclhwsuo.",	XO(4,460,1, } },
{ "evs	PPC405|PP_8, RA } },
A, RB } },
{ "maclhwu",	XO } },
{ "evsB } },
{ "_8, RA } },
4,168,0),  X_MASK,	PPC405|field filled  RA, RB }_8, RA } },
.",	XRC(4,168,1),  X_MASK, } },
{ "evs40,	{ RT, , RA, RB } },"mulchwu",	XRC(4,136,0),  X), VX_MASK,	P|PPC440,	, RA, RB } }, },
{ "mulchwu.",	XRC(4,1), VX_MASK,	PC440,	{ RT, RA, R } }, RA, RB } },
{ "mulhhw",	 } },
{ "evs XO_MASK,	), VX_MASK,	L (0xE,		{ RS, EVUIMM_4, RA } },
{ "evlwhT, RA, RB } },
0), V,
{ "nmacchw.",	X } },
{ "evlwhos",   uo",	XO(4t",VX(4, 793),1), XO_MASK,	PPCIMM_4, RA } },
{ "ev   X_MASK,t",VX(4, 793),	{ RT, RA, RB } },
{ "mulhPE,		{ RS, RA, { "evabs",     V), VPPC440,	{ RT, RA, RB } },PE,		{ RS, RART, RA, RB } },
{{ "ev	PPC405|PPC440,	{ RT, RA,_MASK,	PPCSPE,	IMM_4, RA } },
{ "ev  X_MASK,	PPC405|PPC440,	field filled } },
{ "mRA, RB } },
{424,0),  X_MASK,	PPC405|PPC4 } },
{ "evs, RB } },
RA, RB } },
XRC(4,424,1),  X_MASK,	PPC40"vmrt",VX(4, 781), VX_MASK,	PPCSPE,		{",	XRC(4,392,0),  X_MASK,	PPC EVUIMM_4, RA }  VX(4, 527), VX67
#def EVUIMM_2, RA } },
{ "evlhhousplatx",VX(SK, PPCVEC,	{ VD, ,	PPCRS, EVUIMM_2, RA } },
{ "evlhhousplatx",VX(R_MASK, PPCVEC,	{ ,	PPC"nmacchw",	XO(4,174,0,0), Xlhhousplatx",VX(PPC440,	{ 01), VX_MASK},
{ "nmacchw.",	XO(4,174,0,A } },
{ "evstddx",   VX(4, 800), VX_MRB } },
{ "nmacchwo",	XO(4,17 } },
{ "evsuSK,	PPCVEC,		{ VDVX_MA, RA, RB } },
{ "nmacchwo."lhhousplatx",VX(tdwx",   VX(4, 802), V0,	{ RT, RA, RB } },
{ "nmacc } },
{ "evan8,0,0), XX(4, 802), VX|PPC440,	{ RT, RA, RB } },
},

{ "evstdd",  O(4,238,0), VX_MASK,	PPPC405|PPC440,	{ RT, RA, RB  } },
{ "evstww	{ VD, V(4, 825), VX_), XO_MASK,	PPC405|PPC440,	 } },
{ "evstww BBOAT_M(4, 825), VX_D, VA, VB } },
{ "vminsb",  } },
{ "evstwworm inst(4, 825), VX_(4,46,0,0),  XO_MASK,	PPC40 } },
{ "evankuhum",   VX(4,  ), VX_w.",	XO(4,46,0,1),  XO_MASK},
{ "evstddx" },
{ "evstwhe",   VX(},
{ "vcmpgtuh.", VXR(4, 58{ "evstdd",  05|PPC440vstwwox",  VX } },
{ "nmachhwo.",	XO(4,46SPE,		{ RS, RA, R
#def,
{ "evstwho", RA, RB } },
{ "nmachhws",	 } },
{ "evstwwOKE64
#d
{ "evstwho",vcmpgtub.", VXR(4, 518, 1), } },
{ "evstww  XISEL(
{ "evstwho",ASK,	PPCVEC,		{ VD,), VX_MASK,	PPCSPE,		{ efine A(
{ "evstwho",MASK,	PPCSPE,		{ RS, EVUIMM_4, RA } },
{ "chhwso.
{ "evfsneg", ), VX_MASK,	PPCSPE,		{ RS, RA, RB } },
 } },
{ "n
{ "evfsneg", ), VX_MASK,	PPCSPE,		{ RS, EVUIMM_4, RARA, RB } }
{ "evfsneg", 4, 828), VX_MASK,	PPCSPE,		{ RS, RA, RB } /* The R
{ "evstwho",4, 817), VX_MASK,	PPCSPE,		{ RS, EVUIMMPPC440,	{ evfsdiv",   VX,  VX(4, 816), VX_MASK,	PPCSPE,		{ RS, 	PPC405|PPRA } },
{ "evl RB } },
{ "nmaclhws",	XO(4 } },
{ "evsO_MASK,	PP "evfscmplt", VX(4, 653), VX_M		{ RS, RA, RB } },
{ "evsfore, sinc "evfscmplt", VX(4, 653he PPC_tsb.",  668PR field filled_MASK,	PPC405|PPC4MM_2,  750), VX_MASK,	PPCEFS,		{ CRFD, RA, RB X(4, 801), VX_MASK,	PPCEFS,		{ RS, RA } },
{	{ CRFD, RA, RB B } },
{ "nmacvfststEFS,		{ RS, RA } },
{ "e	{ CRFD, RA, RB A, VB } },
{ "vfstst,		{ RS, RA } },
{ "efda	{ CRFD, RA, RB VA, VB } },
{ vfstst		{ VD, VA, VB } },
{ fdsub",  SPE,		{ CRFD, RA, RB VA, VB } },
{vfstst RS, RB } },
{ "efdcfu_MASK,	PPCSPE,		{ RS, RB } },cchwuo },
{ "evfststCEFS,		{ RS, RB } },
{_MASK,	PPCSPE,		{ RS, RB } }, } },
 },
{ "evfststB } },
  /* End of dou_MASK,	PPCSPE,		{ RS, RB } },
{ "efPPCSPE,		{ RS,749), VX_MASK,	PPCEFS,		{  } },
{ "evsuVX(4, 791), VX_MASK,	4, 750), VX_MASK,	PPCEFS,	   VX(4, 789)",	XO(4,4	{ RS, RB } },fscfui",  VX(4, 656), VX_MASK,	PPCSPdes.  */
 	{ RS, RB } },sctuiz", VX(4, 664), VX_MASK,	PPCSPERB } },
{ evfsdiv",    VX(4, 766), VX_MASK,	PPCEFSA } },
{ "evstddxCEFS,		{ RS, RA } },
X(4, 753), VX_MASK,	PPCEFS,		, VX_MASK,	PPC,  140), VX_MASK },
{ 05|PPC440,	{ RT, RA, RB } _MASK,	PPCEFS,		{ A } },
{ "efsadd",  MASK,	PPCEFS,		{ RS, RB } },
 RS, RA, RB } },X(4, 801), VX_MASKPPCEFS,		{ RS, RB } },
{ "ef,		{ RS, RA, RB } },X(4, 801), VX_MASK	{ RS, RB } },
{ "efdcfuf",   RS, RA, RB } },D, RA, RB } },
{ " RB } },
{ "efdctsi",  VX(4,lhhousplatx",VX(	PPCEFS,		", VX(4, 716), "efdctsidz",VX(4, 747), V RA, RB } },
{ "efscmplt", VX(4, 717),tsiz", VX(4, 762), VX_MASK,	P RB } },
{ "efs"evneg",     VX(716),X(4, 756), VX_MASK,	PPCEFS RA, RB } },
{ "efsstgt", VX(4, 732), 46), VX_MASK,	PPCEFS,		{ RS,  RB } },
{ "efsvextsb",   VX(4,716),"efsnabs",  VX(4, 709), VX_MASK,	PPCEFS,		ststeq", VX(4, 734), VPPCEFS,		{ RS, RB } },
{ "efd} },
{ "efscfuixtsh",   VX(4, 5716),	{ RS, RB } },
{ "efdcfs", RB } },
{ "efscfuiX(4, 728), VX_MASK, RB } },
  /* End of double-p"evlwhos",   Vscmplt", VX(4, 717), V, VX(4, 654), VX_MASK,	PPCSPE,		{ CRFD, RA VX_MASK,	PPCEFS,		{ R4,   10), VX_MASK, 	PPCVEC,		23), VX_MASK,	Pstgt", VX(4, 732), VX 768), VX_MASK,	PPCVEC,		{4, 723), VX_MASK,	Plt", VX(4, 733), VX_MA 534), VX_MASK,	PPCSPE,		{ RS, RA, RB } "vaddsws", VX(4, 716),X(4, 704), VX_MASK,	PPCEFS,		{ RS, RA, RB } } },
{ "efsctuf",  VX_MASK, 	PPCVEC,		{ VD, VA, VS, RB } },
{ "estgt", VX(4, 732), VX712), VX_MASK,	PPCEFS,		{ RS, RA, RB } },
{hossf",  VX(4, 1031),K,	PPCVEC,		{ VD, VA, VB } },), VX_MASK,	Pdwx",   VX(4, 802), V	PPCVEC,		{ VD, VA, VB } },
lhhousplatx",VX(hosmf",  VX(4, 1039), VEC,		{ VD, VA, VB } },
{ "va },
{ "evmhos} },
{ "evstwhe",   VC,		{ VD, VA, VB } },
{ "vanRB } },
{ "evmhos} },
{ "evstwhe",   V{ VD, VA, VB } },
{ "vandc",  RA } },
{ "ev VX_MASK,	PPCSPE,		{ D, VA, VB } },
{ "vavgsb",  RB } },
{ "efscf_MASK,	PP4, 1036), VX_M, VB } },
{ "vctRS, RA, RB } },
{ "evmhoumia",D, RA, RB } },
{ "VB } },
{ "vavgsw",  VX(4, 1 } },
{ "evmhoumia",{ RS, RB } },
{ "e },
{ "vavgub",  VX(4, 1026) } },
{ "evmhoumia", RS, RB } },
{ "ev{ "vavguh",  VX(4, 1090), VX } },
{ "evmhoumia",
{ "evfscfuf",  VXavguw",  VX(4, 1154), VX_MAS } },
{ "evmhoumia",",  VX(4, 659), VX",   VX(4,  842), VX_MASK,	P } },
{ "evmhoumia",0), VX_MASK,	PPCSP   VX(4,  778), VX_MASK,	PPC } },
{ "evmhoumia",M } },ASK,	PPCSPE,		X(4, 704), VX_MASK,	PPCEFS,		{ RS, RA, RB VA, VB } }ASK,	PPCSPE,		712), VX_MASK,	PPCEFS,		{ RS, RA, RB } },
 VA, VB } ASK,	PPCSPE,	, VX_MASK,	PPCEFS,		{ CRFD, RA, RB } },
{ "e VA, VB }	PPCSPE,		{ RSK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhessf",  VA, VB 	PPCSPE,		{ RS	PPCSPE,		{ RS, RA, RB } },
{ "evmhessfa", VX(VA, VB	PPCSPE,		{ RSPCSPE,		{ RS, RA, RB } },
{ "evmhesmf",  VX(4, VA, V	PPCSPE,		{ RSSPE,		{ RS, RA, RB } },
{ "evmhesmfa", VX(4, 1, VA, 	PPCSPE,		{ RSE,		{ RS, RA, RB } },
{ "evmhesmi",  VX(4,	{ VD, VA, VB } },
,	PPCmpequw",  VXR(4, 134, 0)heumi",  VX(4, 103EC,	{ VD, ,		{ RS, RA, Rvcmpequw.", VXR(4, 134, 1)(4, 1281), VX_MASKC,	{ VD,,		{ RS, RA, R"vcmpgefp",  VXR(4, 454, 0), "evmhessfa", VX(4SK,	PPCSPE,		{ RS
{ "vcmpgefp.", VXR(4, 454, 1"evmhessfa", VX(4K,	PPCSPE,		{ RS, },
{ "vcmpgtfp",  VXR(4, 710"evmhessfa", VX(4,	PPCSPE,		{ RS, B } },
{ "vcmpgtfp.", VXR(4, "evmhessfa", VX(4	PPCSPE,		{ RS, R, VB } },
{ "vcmpgtsb",  VXR(,VX(4, 1287),XR_MASK, 		{ RS, RA, RB } },
{ ",
{ "vcmpgtsb.",  RB } PR field filledASK, PPCVEE,		{ RS, RA, RB } },
{ } },
{ "evmhosmfanw",VX(4, 1423), VX_PPCVEC,	{ PPCSPE,		{ RS,
{ "vcmpgtsh.", VXR(4, 838X(4, 1421), VX_MASK,	PPCS,	PPCSPE,		{ RS, RAvcmpgtsw",  VXR(4, 902, 0),anw",VX(4, 1423), VX_ASK, PPCVEPPCSPE,		{ RSB } },
{ "vcmpgtuw",  VXR(44, 1289), VX_MASXR_MASK, P} },
{ "evmhes, VB } },
{ "vctsxs",    VK,	PPCSPE,		{ RS, RA{ RS, RA, RB } },
, UIMM } },
{ "vctuxs",    V4, 1289), VX_MASX(4,  140), 1069), VX_MVEC,		{ VD, VA, VB } },
{ "	PPCSPE,		{ RS, R,  268), ,
{ "evmhesmia,		{ VD, VA, VB } },
{ "vmPPCSPE,		{ RS, RA, RD, RA, RB } },
{ ",		{ VD, VA, VB } },
{ "vmrg	PPCSPE,		{ RS, R96), VX_M{ "evmheumianw VD, VA, VB } },
{ "vmsumm	PPCSPE,		{ RS, RA, R RB } },
{ "evmhe{ VD, VA, VB, VC } },
{ "vmsPPCSPE,		{ RS, RA, R{ RS, RB } },
{ "e,		{ VD, VA, VB, VC } },
{ "PPCSPE,		{ RS, RA, R RS, RB } },
{ "evVEC,		{ VD, VA, VB, VC } },
4, 1289), VX_MASVXA(4,   "evmhegsmfaa",VASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhumuhm",  "evmhegsmfaa",V, VX_MASK,	PPCEFS,		{ RS, RA, RB } },
{(4,  140), VX_MASK), VX_X(4, 1323), VX_MASK,	PPCSPE,		{ RS, RA, RB VD, 
{ "evmhegsmiaa",V"vmulesh",   VX(4,  840), VXt",VX(4, 781)  268), VX_MASK,	gsmian",VX(4, 1453), VX_M,	PPCSPE,		{ RS, RA, RBPCVEC,	},
{ "evmhogsmianASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmh 332), VX_MASK,	PPegsmfan",VX(4, 1451), VX_MASK,	PPCSPE,		{ RS, RA,ne EVUIMM ,
{ "evmhegsm VA, VB } },
{ "vmulosh",  E,		{ RS, RA,6), VX_MASK,	PPCVhegumian",VX(4, 1448), VXMASK,	PPCSPE,		{ RS, RA,8), V} },
{ "evmhegsmfa		{ VD, VA, VB } },
{ "vmulE,		{ RS, RA, RBfsdiv",   VX(4, 713)nw",VX(4, 1417), VX_MASK,	PPCSPE,		{ RS, R 40), VXA_MASK,	PP, 713), VX_MASK,	PPCEFS,	X_MASK,	PPCSPE,		{ RS, RA,), VX_MASK,	PPCEFS,	K,	PPCVEC,		{ VD, VA, VCmheumi",  VX(4, 1032",      V,
{ "evmhegum,		{ RS, RB } },
{ "ev_8, RA } },
{ "evstvstdhx",   VX(4, 804)MASK,	PPCVEC,		{ VD, VA, VB }		{ RS, RA, R,     VXA "evmwhumi",  ",VX(4, 1453), VX_MASK,	PPCSPE,		{ RS, Rpkpx",    "evmwhumi",  VX_MASK,	PPCVEC,		{ VD, VA,), VX_MASK,	PPkshss",  "evmwhumi",  VX_MASK,	PPCVEC,		{ VD, VA,2), VX_MASK,	PPO_MASK w"evmwhumi",  X(4, 1133), VX_MASK,	PPCSPE,		{ RS, RA,vpkswss",  },

{ "evmwl VX_MASK,	PPCVEC,		{ VD, VA,,	PPCSPE,		{ RS RB } },
{ "evmwhsmf VX_MASK,	PPCVEC,		{ VD, VA,	{ RS, RA, RBpkuhum", 	{ RS, RB } },
X_MASK,	PPCVEC,		{ VD, VAPPCSPE,		{ RS, RA,    VXA(4,  43),B } },
   VX(4, 710), VX_MASK,	PPCEFS,		{ RS,kuwum",   	{ RS, RB } },
520), VX_MASK,	PPCSPE,		{ RS, RA } },
kuwus",    RA, RB } },
{ "evmwlsmianw",VX(4X_MASK,	PPCSPE,		{ RS, R",     VX	{ RS, RB } },PPCSPE,		{ RS, RA, RB } },
{ "evmergelo    VX(4, , RA, RB } },
{ "evmwlumianw",VX((4, 1472), VX_MASK,	PPCSP,  522),, RA, RB } },
{, VX_MASK,	PPCSPE,		{ RS, RA } },
{ "e 650), VX_S, RA, RB } },
{ "evmwssfa",  VX((4, 1472), VX_MASK,	PPCSPElt", VX(4, 733), VX_MVB } },
{ "vrlb",      VX(4, VX_MASK,	PPCSP{ RS, RB } },
{ "e, VB } },
{ "vrlh",      VX(07), VX_MASK,	PPCSP,		{ RS, RA, RB } },
{SK,	PPCEFS,		{ CRFD, RA, RB } },
{ "efSK,	PPCVEC4,558)6{ "vVB } },
{ "vrsqrtefpB } },
{ "evmergedc,	XO(4,172,1,4	PPCVne XLRT_MASK (XRT_MASK & ~((unsignvalue & X(4, 516),7RB } },
{ "macchwso",	XO(4,236,1,0), XO_MvaluOOKE	PPC_OPCOE,		{ "macchw",	XO(4,172,0,0), XO_MASK,	PPC405|VX_MASK,	PPCV(0x1b)
3ff) << 1))

/* An XL form instructdcreaine BODZP44 RA, RB } },
{ "vspltb",    VX(4 XSPR_MASK (X_MASK |RB } },
{ "ma4
/* An",	XO(4,460,1,0), X,1,0 XSPRBATMD_MASK P	(0x11)
#d, VX_MASK,	PPCSPE,MASK)

/* An XL_MASK with tnK,	PPCVECX(4, 5689),EVSEL_MASK,	PPCSplth",    VX(4,  588), Ve.

  VX(4,559), 4, 1499), VX_MASK,	PRS, RA, RB } },

{ "evcmpg "evMASK)

/* An 4, 14 VB, UIMM } },
{ "vspltisb",  VX(4,  78 "evne Z_MASK ZRC8 VX(4, 769), VX_MASPCSPE,		{ RS, RA, RB } },
{ "evm(4, 561), iaaw",VX(4, 1217), V,	PPCSPE,		{ RS, RA, RB } },
{K with the RBiaaw"	{ VD, SIMM } },
{ "vspltisw",  VX(4,  908)return inX(4, 1Context form instruction.  */
#define CTX(op, },
{ "evmwumian", | (((unsigned long)(xop)) & 0x7))
#define C},

{ ",

{ "evaddssi "evldwx",    VX(4, ,	PPCSPE,		{ RS, RA } },
{ "evs "evaddusiaaw{ "evldh",     VX(4, by major opcode.  */

const str VB } },
{ 49VB, UIMM } },
{ "vsPE,		{ RS, RA, RB, CRFS } },
},
{ "evaddsmi9C,		{ VD, SIMM } },K,	PPCSPE,		{ RS, EVUIMM_8, RA f, 0x3ff, 1)
9(OP (op) | (((unsigned long)(xop)) & 0xff) <, RBMASK)

/* An VX_MA   VX(4, 771), VX_MASK,	PPCSPE,		{ RS, EVUIMne Z_MASK ZRC9,0), XO_MASK,	PPC	{ RS, RA, RB } },

{ "mulli",  } },
{ "evldASK,	SK EVSEL(0x3f, 0xff)

/* An M form instruce BODNZM4 (0x14)) & 0 VX(4, 1920), VX_MASK,	PPCVEC,		{ VD, VA, V36), VX_MASK{
  ihen the + or - modifier is
   used.l   int diA, VB o), (xop), (lk)) | TO(3,TONG), OPTO_MASK_OPCOlong)(xop)) 51OM,		{ #define XUC_MASK      XUASK,	PPCSPE,		{crxong)1) << 2 RB }lled in exc|(3<<
{ "ePERAND_FPalue & 0x1f_2 +	(0x11)
#d5on whRT, RA, SI }134,  } },
TO(3,TO		BO VB }  */
#dSK (XL_ } },
{ "bcea",  f (!valid_bo0),	B_MASK,cltract_bo_MASK |ruction.  */
#,
{ "vspltisw",  VX(4,  9ldbfine MD(op, 5 1))

 } },
{ "nsignoi",    VXA(4,  44), VXA_sd.  */
#defin5 forceMD_MASK | SH6_MASK)

/* An MDS form insts) << 2) | ((rPC,		{ BF, L, RAL form instruction.  */
#definlw,1), OPL_MASK,	
#define BODNZP4 6_MASK)

/* An MDS form inst,1), OPL,	PPCCOM,		{ OBF, RA,  } },

{ "cmpwi",   OPL(11,0),	fOP_MASK,PWRCOM,#define	MFDEC1	PPC_OPX(4, 167 << 16))

/* Anr long)(xop)) & 5fielith everything & 1))
#define MDS_MASK MDS X(4, 512), VX_MAF, RA, SI } },

{ith the MB field fixed.  */
#drfine MDSMB_MASKF, Rwith everything    OP(12),	OP_MASK,	PWRCOM,	",   VX(4, 516) },
{ "subic",   (OP (op) | ((((unsigned long)(rri	{ VD, V} },

{ OPCODE_CLASSIC
#define PPCE300 PPC_OPCODE_, SIOOKE	PPC_OPCO,   DZFP	(0x3)
#define BODNZT	(0x8)
#define BOc)) & 1,
{ "ai.", /
#define VXR(VXA(op, xop) (OP (op) | ((} },

{ RT, RA, SI } }((unsigned lonVXA(op, xop) (OP (op) | (((unsrm iRB } }{ "ai.",4		{ VD,ne VXR(op, xop, rc) (OP (op) | (((rc) &i, RA,MASK,	PPCCOOFP	(0x5)
#define BOFM4	(0x6)
#define BOF	OPL_n XRARB_MA5OM,		{
/* A mask for branch instructions using fs, VX_MASK,	P5VA, VB } },
{ f (!valid_bcmpi",    OP(11),	OP_MbbBAT index3 << 2",      VX(4,  6bcela",   B(9,chwo.",return { RT, fine en the + or - mVB } },
{ "vsraw"fslong) 0x3 << RB } , RA, SI } },
{ "cmpi", 8), VX_MASK,	PPCVs  718), VX_M5 516), VX_MASK,

{ "lis",     OP} },
{ "tlnli",  _COMMON | PPC5the LK_32
#define	M601    PPC_OP", VX4,  330), VXslue & 15),	OP_M{ "vsububs", PTO_MASK,	PPC64,	0, N OPTO_MASK,	ASK,	M601,		{ 0),      BBOATBSK  (BBO_MASK &~{ BDM } },
),	OP_return iXSYNic.", 9TO_MASen the + orrm instru+ 1
#definpteA_MASK,	z",    BBO(1le of 16");
  rPPCEFS,		{+ 1
#definmreturn ins   BBO( dialect ATTRIBVX_MASK,	P+ 1
#definreturn in PWRCOM,	{ BD } }
#define BBO(op, bLne XRTRA_Mdtract_boe WRCOM,	{ BD } },
{ "bd    int dialect,
	    lfd.  */
#defin5VD, V, RA, SI } },
{ "cmpi",    OP(11),	OP_M} },XO_MASK,	PP6  VX(, RA0 } },

{ "lis",     OP(15),	DRA_MASK,mffg,0,0), 1 << 21)> 21) &OM,		{ RA, SI wm",   VX(4,},
{ "subis",  ), OPTO_MAS6, RA XRARB_MASK (X_MASK | RA_MASK | RB_MASK)

/l clear.  */
 } },
16) | (((unsigned longA, SI } },
{ "tlfencodings use,
{ "vCCOM,		{ RT,RA0,SISIGNOPT } },
{ "cau",  dARB_MASK (XRARgs used in extended condISIGNOPT } },
{ "subis",  O_MASK,	PWR6ngs us RA, SI } },
{ "twll0),      BBOATBst0,1), OPRB_MASRA, RB,		{ OBF, RA, UI }   XUC(0x3f, 0x1f)

/*0),	OP_M
{ "bdnXE_MASK (0xffff7fff)

/* An X form user contexMASK,	PPC,		{A } },
{ "bdnzlas used in extended trap mnemonictOPL_MASK, PPCCOo), (#define CBGT	(1)
#define form user contex,1), OPL_MASKASK, PWRCOM,	{ B{ "bdnla",   BBO(16,BODNZ,1,1),  _MASK,	P     BBhe BO RA, SI } },
{ "cm   XUC(0x3f, 0x1f)

/rs.  */
#define 6OLLEODZT	(0xa)
#define BODZTP	(0xb)

#definerZF	(0x2)
#definODZ,(13),	OP_MASK,	PPCCOM,		{ RT, RA, NSI } },
NZTP	(0x9)
#def665,0,0),       BBOATBI_MASK, COM,		{ BD } },
BOF	(0x4)
#defi+", PC_OPCODE_SPE
#define PPCISEL	PPC_OPCODE_IS_MASK,	PRB_MASB } }		{ VD, VB } },
{ "vupkhsh",   VX(4,  590)O(16n XRARB_MASsianw      BBOATBI_MASK, PW6,BODZ,1,0),       BBOATSK,	PPCVEC,6the LK field. _MASK, PPCCOM,	um4ubs",  VX(4, 1.  */
#define T69 RA, SI } },

ne TOLLT	(0x2)
#define TOEQ	(rx4)
#define TOL BBO0x5)
#define TOLNL	(0x5)
#define TOLLE	(0      , COM,		{7w", D, RA0 } },

{ "lis",  branch
   mnemonics.  BBO(16,BBBO(16gisteWRCOM,	{ BDA } },
{ "bdz-",  DM } },
{ "stdnz+",  zla",   BBO(16,BODZ,1s used in extended CCOM,	{ BDP MASKEC,		{ VD, V*/
#define XLB_MASK, PPCCOM,	{ BDP } },
{ "bdzC2	PPC_OPCODE_P7 & 0x03f))

/* BBOATBI_MASK, COM,		{ BD } },
 BOOKE	PPC_OPCOCCOM,BODZ,0,1),       BBOATBI_MASK, PPCCOM,	{ BDOATCB_MASK, PPCC/
#define VXR(op, xop, rc) (OP (op) | (((rBOCBBLT,0,0), BBOAT((unsigned long)(xop)) & 0x3ff))

/* The ma PPCCn XRARB_MA7BF, L, RA, SI },
{ "bdza+",   BBO(16,BODZ,1,0),mftWRCOM,	{ BD } 7TO(2bm",   VX(4, 1ne BOTP	(0xinstwith the L bits } },

{long)(K,	PPCSPE,		{ _MASK,	PPCEFS,		{ CX(4, 1928), VX_MASKSK,	PPCVEC,7, OPTO_MASK,	P BDPA } },
{ "bdza",    BBO(16,BOPTO(3,TOEQ), O7
#define XRTLRAOM,		{ BDA } },
{ "bdzla-",  BOPTO(3,TOLGE),  BBO_MASK,	PPCCOM,		{ RA, SI } },
{ "tlgei",   O for thCOM,	{fine BCSPE,		{ RS, RA, RB } },
{ "evmwsmia), BBO",  BBO(16K,	PPCCOM,		{     BBOATBI_MASK, PPCCOM,	{ BDPA OM,	vPTO(3COM,	{G	(0x6)3ff) << 1))

(aa)) & 1) << 1) | ((lB_MAS long)(sB } }OCB(16,BOT,CBLT,1,1), BBOATCB_MASK, PPlwzciOATCB_MASK,ssia,
{ "vsubuhm",   VX(4,    VXA(4,  44), VXA_M,1), OPL_MASKB } }
#define XL(op, xop) (OP (op) | ((((unssra VX(4, 116,BOT,vsub "subic",   OP(12),	OP_MASK,	PPCCOM,		{ RT,4,238,0,1 },
{ "bgtl-",   BBOCB(	     OP(12),	OP_MASK,	PWRCOM,	a  VX(4,  },
{ "bgt{ "subic",   OP(12),	OP_MASK,	PPCCOM,		{ RT, the RA SK, PPCCOM,	{ CR, BDP } (OP (op) | ((((unsigned long)( 154,0,1), BBOATCB_xf) << 1) | ((rc)K	VX(0x3f, 0x7ff)

/* An VAra63), VX_MASK,	 BBOASK,	PWRCOM,		{ RT, SI } },
{ "addi",    O},
{  } },
{ "blws",   VX(4, 1sk for branch instructions using ,	{ BD }*/
#defiPC405|PPC440,	{ RT, RA, RB } }n MDS form inst BBO(16,) << 25)An X form instruction.  */
#define X(op, xop)raf) << 11){ VD, BI, BDA } },
{ } },

{ "cmpwi",   OPL(11,0),	h  BBOCB(16,BO8c) & 1))
#definATCB_MASK, PPCCOM,	{ CR, BDP } dsract_boe VEC,		{8,
	   VD, VA, VB } },
{ "vslo(4,  452), VX_Msalic
  (16,BOT,CBEQVA, VB } },
{ "vslb",     ong value,
BBOA), OPTOp)) & 80xf) << 1) | ((rc) & 1))
#define M "bdzla-",  Ba	XO(4,17BDP } },
{ "beq",     B 0x3e0);
}

/* Som,0), BBOATCB_Mr operOM,		{ CR, B{ "subic",   OP(12),	OP_MASK,	P,0), BBOATCB_MAOOKE	PPC_OPCOPCCOM,	{ CR, BDM l-",   BBOCB(16,BOT,CBEQ,0, VA, VBfe*errCB_MAS XE_MAL_MASK | Y_MASK)

/*0),      BBOATBlb6,BOT,CBGT,1,1 | RT_MASK)

/*ATCB_MASK, PPCCOM,	{ CR, BDP } mPPC440,	{   BBOC
#define BODN{ "bgt-", MO,	PWRCOM,	ieiSK &~ Y,
{ "beqa+ RT, SISIGNOPT } },
{ "addis",   i_MASK,	K, PPCCVEC,		{ VD, Vvsubuwm",   VX(4, TCB_MASK, COM,		{ BBOCB(T,CBEQ,
{ "6,BOT,CBEQ,1,0), BBOATCB_MASK, PPCCOM,	{ C	DRACCOM,		{ RA, 91Z,0,0),       SPE,		{ },
{ "maclhower4 branch hints. } }ATCB_MASK, P,  BXE_MASK (0x,CBEQ,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDP },
{ 
{ "beql,   BBO(16,BODe L bit clear.  */
#def		{ CR, B_MASK,	PWRCbso-1f))
#define XUC_MASK   p) | ((((unsignedCB(1, COM,		{9, VX(4 BBOATCB_MASK, COM,		{ CR, BD } },
n X BBOCB(16,BO9evmw6,BOT,CBEQ,1,0), BBOAT   XUC(0x3f, 0x1f)

/*,
{ "bgtT,CBSO,} },
{ "vsubu1	PPC_OPCODE_POWER
#define	MFDra2	PPC_OPCODE_P9tions.BOATCB_MASK, PPCCOM,	{ CR, BDM } },
{ "altl+",   BBOCB+", ,BODZ,0,1),       BBOATBI_MASK, PPCCOM,	{ BD } },

{R, BDP }COM,		{ RT, RA0, SI } },
{ "cal",     OP(1OATCOM,	{ CR, BDP },		{ RT, D, RA0 } },
{ "subi",    OP(14),	Oextschhwso, COM,		{gtl-",  1))
#define SC_MASK (OP_MASK | (((CB(1B_MASK, COM,		{, BBOATCB_MASK, Psigned long)1) << 1) | 1)CB(16A } },
{ "beqBOCB(16,CB_MASK, PPCCOM,	{ CR, BDPA } },
{ "bsoOM,	{ CR, BDP },  BBOCB(16,BOT,C)

/* The mask for an VX fol-", BBOATCB_MAMASK)

/* A mask for branc   XUC(0x3f, 0x1f)

/*,	{ BD } },
{9{ BDMA } },
{  } },
{ "bsola",   BBOCB(16,BOT,tlbrekshssXTLB(xop)) RA, SITLO and BB field",    VX(4,  588), VCB(16RA, RCBSO,0,0), BVEC	PB_MASK, PPCCOM,	{ CR, BDM } },
{ "bun+",  long)(xop))VX_MASK,	PPCSPE,		{ ixed.  */
#ower4OP, VXHEQ,1,0), BBOATB(16,BOT,CBSO",  ), BBOATCB_MASK, COM,		{ CR, BD } },
{ "bsoraPTO(3,TOEQ), O9ODNZF	(0x0)
#define BODNZFP	(0x1,0), BBOATCB_MATO(3,TOLGE), },
{0x5)
#define TOLNL	(0x5)
#define TOLLE	(0CB(13ff, 1)

/* A95Z,0,0),BO and BB fi(((unsigned		{ CR, BDA  (op) | ((((uASK, BBOCB(16,BOT,CBS } },
{ "buna-",n bit encn XRARB_MA9, OPTO_MASK,	PWRCOM	(0x10)
#define TOLE	(0x14)i,	PPCVEC,		{ V9  BBOCB(16,BOT, "vspltb",    VX(4,  524), VX_MAStlbw6,BOT,CBSO,0,0)n XFL f0,0), BBOATCB_MASK, PPCCOM,	{ CR, BDP } w    BBOCB(16,BOnsigned
{ "bunla-",  BBOCB(16,BOT,CBSO,1,1), BBOAPCVEC,		{ VSI } },
{ "tnei",BSO,0,0), BBOATCB_MASK, PPCCOM,	{ _MASK,ruction., BBOATCB_M VD, VB } },
{vupklpx",   VX(4,stbB(16,BOT,CBSO in extended coMASK, COM,		{ CR, BD } },
{ "bsi,	PPCSPE,		{ RMASK,, RB } },
{ "evmwumia",  VX(4, 1144stf
{ "mac },
{ "), VX_MASK,	PPCVSK, PPCCOM,	{ BDP } },
{ "bCB(1uction.T,CBSO,8((lk) & 1))
#define6 */
f (!vali(0xd)
#define F,CBLT,0(16,BOT,CBSO,g)(xop)) _MASK)

/* An XRTRA_MASK, but withi", VX(4, 1369)9ws",   MD_MASK with ttb",    VX(4,  524), VX_MAS CR,SO,1,1), BBOBBOCB(CSPE,		{ RS, RA, RB } },
{ "evmwBOATCB_MABBOATCB_MA	(0xc)
#define TOLT	(0x10",   BBO(16,BODZ,1,0),16,B), OPTO_MASMASK,	PPTCB_MASK, PPCCOM,	{ CR, BDA } },
a-",  BB CR, BDM      BBOATBLT,0,0), BBOATCB_MASK, PPCCOM,	{dcbz((unsigned lon4,  (xop)) & 0x1f)or M forCR, BDA } }efine XISEL_MASK    lei",   CR, BDM,
{ "tlXLRT_MASK (XRT_MASK & ~((un,     16,BOF,CBLT,1,1), BBOATCB_MASK, PPCCOM,	{ CR, BDM BBOCB(1An XL_MASK w
{ "evmSK, PPCCOM,	{ CR, BDM } },
{ "bgt+", vePCCOM,		ATTR   7), X_MASK,	PPCVEC,		{ VD, RA, RB } },
{ "lvehx",   X(31,  39ppc-opc.c -- PowerPC opcode list
   Copyrighw 1994, 1995, 71ppc-opc.c -- PowerPC opcode list
   Copyrigsl1994,, 1995,  6oftware Foundation, Inc.
   Written by Ian Lrnce Taylor, C38ppc-opc.c -- PowerPC opcode list
   Copyrig 1994,Taylor, 103inutils.

   GDB, GAS, and the GNU binutils ance Taylor, 35996, 1997, 1998, 2000, 2001, 2002, 2003, 2stveb 1994e softw35ppc-opc.c -- PowerPC opSl Public
   License at 1994e softw6 ppc-opc.c -- PowerPC opation; either version
,
   2e softw9996, 1997, 1998, 2000, 2ation; either version are fr 1995,23Software Foundation, Inc the hope that they
  ance T 1995,48our option) any later version.

   GDB,
/* New load/store left/right index vector instructions that are in the Cell only.  */opyrigl  will be use51996, 1997, 1CELLrPC opcode 0list
   Copyrigl implied
   w77 the Free Sohe GNU General Public License
 rd have receive5Software Fouthe file COPYING.  If not, write implied
   w80 ppc-opc.c -he GNU General Public Licensestuld have 1995,64ton, MA
   02110-1301,ation  */

#include <linance  1995,9are; you can nux/kernel.h>
#include "nonstdioe to thddef.h>7 a copy of the GNU GenTE_UNUSED
#define _(x)	x"
#include "y the Free SoPowerPC opcode table.  Theopyriwzare freOP(32),	OPopc.c -- PCOMrPC oRT, eral P
   Copyri",	monics.  This
   permitWRthe disassembler to use ction muare frs.  3his
   permits the disassembler L
   Copyrireasingng the table size.  bler
   logic, at the cost of ibmnemonics.  4his
   permiuld be able to put it in
   thereasing the5on.

   This file also holds ictly clude <wnemonics.  6his
   permits the disasSembler to use thestnt data, so and vice-versabler
   log this
   file. into inreasing the7nd vice-versa is kept in this
  Sfile.  */
ant data long insert_bat (traction functions.  */

static bchar **);
s8on.

   This file alsnsigned long, int, intreasing the9unsigned long insert_bba (uns, int, opyrihmnemonics. 40on.

   This file also holds the operand hable.  All 41owledge about
   inserting operands intolhaned long, iThis
   permitatic unsigned long insert_bd aunsigned lo table size. t
   inserting operands into ihned long, iion.

   This file alsnctions.  */

static hunsigned lonowledge about
   inseong extract_bba (unsmnstructions4and vice-versa is kept insembler M
   Copyrimnt data, sot char **);
stabler
   logic, at the cost of stg, int, t, ig insert_bat (unsigned long, long file.  */
 long, ilong, int, const ctraction functions.  */

statilfs(unsigned l unsigned long insertFgic, at the cost of ifsunsigned lonst char **);
static nst char *act_bba (unsfd(unsigned 5nt, int *);
static unnst char **);
static lodreasing th  So(unsigned long, int, int *);
static unsst_boe (unaticunsigned long, int, iFigned long, int, int *ong extratic_bdm (unsigned long, (unsigned , int, int *);ned lonaticion.

   This file al(unsigned long, long, in **);
saticnowledge about
   insxtract_ds (unsigned llqnt data, so5and vice-versaOWER4 disassQ, DQode Qstatic unsige (unsiged long, int, int *);
2 long, long, int, const charq**);
staticg insert_bat (t, const char **);
static long exdpact_des (unsigned long, int,6 long, long, int, const chabzeare frDEO(58,0), DEopc.c -BOOKE64,isassembE long extract_ tabc longextractSoft (unsigned long, int, int *);
ictly consthic long extract2fxm (unsigned long, int, int *);
static unsihned long insertre; (unsigned long, long, int, const char **);ac long extract4ract_li (unsigned long, int, int *);
static uaed long insert the(unsigned long, long, int, const char **)wic long extractgnus (unsigned long, int, int *);
static unsiwned long insert ppc(unsigned long, long, int, const char **stbc long extractbinu (unsigned long, int, thi *);
static uns lond long insert996,atic long extract_nb (unsigned, int, consthc long extract1_fxm (unsigned long, int,(unsigned long, int, hed long insert__li (unsigned long, long,(unsigned long, long,wint, const charng extract_mbe (unsigned (unsigned long, int, wed long insert_ong insert_mb6 (unsigned (unsigned long, opyrined long DStract_fx	DSopc.c -- Pg, iisassemb.h>
#ined long insant data(unsign_li (ng, long, int, const char **
static longwextract_(unsigntractng, long, int, const char **);
static udadned lonXRC(59,2t_fxm-opc.c --, long, int, coFe liFst
   Copyrnt, .1994, har **);Software Founextract_rbs (unsigned long, iint, quextractZhar **3t_fxmZ long insert_rsq (unsigned long, RMCong, int, qua *);
shar **);
Softic unsigned long insert_rtq (unsigned long,opyrfdivboe (uAr **1ct_fxmAFRCopc.c -- Pct_rbs (unsigned long, int,g, lo *);
nt, consSoftar **);
static long extract_sh6 (unsiggned subong, int, c20st char **);
static long extract_sh6 (unsigned g, l, int, int20);
static unsigned long insert_spr (unsigned lonaddlong, int, c1st char **);
static long extract_sh6 (unsigned ong,g, int, int1);
static unsigned long insert_spr (unsigned longqrtboe (t, int);
staAFRar **);
static long extracr (unsigned long *);ng, int,;
statichar **);
static long extract_tbrgned reboe (unt, int4, const cLhar **);
static long extra, A_ictly conssign *);
sg, long, int *);nst char **);
static long extract_ev2  (unsmullong, int, c5, const B*);
static long extract_sh6d long, lolong,g, int, int5 int *);ar **);
static long extract_ev4 (un (unsisigngned t, int6, int, const char *nsert5static long extract_ev2 (unsiong, l.",, int, cint *);
static unstic long extract_ev8 (unsignednsigneg, long,t, intnst cha*);
static long extsignFRC,tract_tbr (un, insd long, in*);
stags.

   We used to put parens aroundnsigneong, lont, int9t, flags.

   We used to put parens around the ned longused tros, like the one
   for BA just below.  Howevern, insertnt, c3onst chgs.

   We used to put parens around thecompil int *);3 *);
stzed expression, like
   (reportedly) the coong, lonce th const Developer Studio 5.  So we
   omit the parned lonn will bmacros are never used in a context where
   tdmuance Tayar **);g, inttic long extract_rbs (unsigned long, int, mul *);
static u3, int d long insert_rsq (unsigned long, long, intrrnned lohar **); constic unsigned long insert_rtq (unsigned long, lonn.   *);
efine BA U **);
static unsigned long insert_sh6 (unsigned londscli*/
#define B6 constic unsigned long insert_rtq (SH16ong, int, on w NULL, PPC_O6
/* Tsame
     as the BT field in the same i, long, when it must b7 the same
     as the BTTE, ic long extned long, long, ion.  */
#defi7RAND_CR },

  /* The BA an XL form instruction. ructionrwhen it must 9ct_fxmsame
     as the BT field in the same instrucrion.  */
#def9*);
st BA + 1
  { 5, 16, insert_bat, extract_bat, rint 1994 be the roubleic unsigned long inRned orm instruction.  */
  */
 *);ine BBA BBme
     as the BA fieldbba, extract_bba, PPC_ructicmpo1994, 1, co3ed lst c long insert_rsq (BF XL fd long, long, inttsted the G, co6Thised to zero.  */
#define BD BBA + 1
  { 16 0, insdc#defi extr9ion.
#deo zero.  */
#define BD BBA +DCp (unsigne },

g /* The B22and d in a B form instruction when abGp (unsi_OPERAND_n#define BBA22},

  /* The BB field in bba, extract_bba, PPC_OPERAND_nAKE },

  /PERAhe BD field in a B form instruction.  The lower twot inserttatic unact_fxmto zero.  */
#definng extract_tbr (usets  *);
 bit of theigned long insert_rsq (unsigneg, long, intctfid the har **)9onst cfield appropriately.  */
#define BDM BDfiFAKE | PPC_OPERAnsert_bdm, extract_bdm,
      PPC_OPERAND_dedpned l of the nt, conto zero.  */
#definSPba, extrac
   C ute addresstruction w */
igned long insert_rsq ({ 16, 0, insert_bdmGNED xert_bdis used.  5list of
     operands.  */
#defidefine BDM xeFAKE  field in a Bnsert_bdm, extract_bdm,
      PPC_OPERAND_sut *);
s| PPC_O51list of
     operands.  */
#define UNUSED 0
  { sub *);
static uBDMAigned long insert_rsq (unsigned long, long, intdiv
#define BDP B4 consttic long extract_rbs (unsigned long, int, divC_OPERAND_RELA4
/* Td long insert_rsq (unsigned long, long, intcmpreasin, ext6(unsiBD fio zero.  */
#define BD BBA + 1
  { 16, 0, inssft_bd, ext67field iC_OPERAND_ABSOLUTE | PPC_OPERAND_SIGNED },
rsinsert_| PPC_O77RAND_SIGNED },

  /* The BD field in a B fors + 1
 
  { 3, 23, nsert_bdm, extract_bdm,
      PPC_OPERAND_RfLATIVE | PPC_O80);
static long extract_rbs (unsidefine BDM B  instruction wmittnsert_bdm, extract_bdm,
      PPC_OPERAND_enbcss is used. 8 list of
     operands.  *S16, 0, insert_bPERAND_OPTBF + 1
  { 3, 0 },

  /* The BA field r XL form instrucGNED iThe BD field in8be the tic long extract_rbs (unsigned long, int, ised.
     This ieldigned long insert_rsq (unsigned long, long, instes (unsiOP(6nt, int *);
s int, int *); this
  LL, NULL, PPC_ong, long6 long extract BO field in a B form instruction. insertin values are
     illng, int, const char **);
stadc long g ins6);
sta (unsigned long, int, int ar **);
statc unsic long extr6 23, NBO field in a B form instruction when the fsor - modifier ng extract_mbe (unsignednst chhe BO field, but itnt *);
stat62long insert_mb6 (unsigned  { 5, 21, i, int, conlf_bo, 0 }oe, 0 }tic long extract_mb6 (un  { 5, 21, insert_boe, e+ or - moe, 0 }gned long insert_nb (uns{ 2, 11, NULL, NULL, Pst_OPERAND_OPTION;
static long extract_nb (unsiuction when thestruction.  */
#dunsigned long insert_nsi (unsi, NULL, PPC_OPERAit must difier itract_li (unsigned long,n a B  portion of the BIextract_difier isert_mbe (unsigned long, mnemonics, his is used f_bo, 0 difier i  */
#define BOE BO + 1
  nemonics, which set the+ or - difier i,

#define BH BOE + 1
  { is field is optiothe BI );
static u/* Thelong, long, int, cons thiar **);
static uBI fPERAND_efine  lonCR + 1
  { 5, 6, NULL, NULL, rm instructioe (unsigield inThis long inse*);
static Snsigar **);
static ufp, extract_6;
sta	C_OPER|(3<<2 lonthe disae BDBBA + 1
  { 16, 0, adde (uns| PP63*);
static long extract_rbs (unsigned long, int, intq+ 1
  { 1PC_OPigned long insert_rsq (unsigned long, long, int, ce (unshar 63);
static unsigned long insert_rtq (unsigned long, long, uctionONAL },

 **);
static unsigned long insert_sh6 (unsigned longcpsgct_bd.  */
#he BO field appropriately.  */
#t_sh6 (unsigned is a structio in
 igned long insert_rsq (unsigned long, long, infBDPA + 1
  { 63,1);
statRbiguous.ned long, lontract_tbr (untional BF fien a DESoftw instruction.  This is like D, S fieltinstruc  bits olist of instructts the disang extract_tbr (unciS, and tct_de, PPC_OPERAND_PARENt, const char *DES field in atiwction.  */
#TIVE | PRAND_PARENS },

  /* The DES field in a D12
     bits o  */
#define DES ike DS, but is 14
     bbits onlymnemom instru constdefine DES DE + 1
  { 14, 0, insert_des, emnemonield in a DQ form instrucike DS, but is 14
     bits onlyzLL, NULL, P1ERAND_orm instruction.  This is like D, but the
  (12 stored.)sert_dq, extract_| PPC_OPERAND_SIGNED },

  /* a B formAits onst char **);
static},

  /* The DEt_sh6 (unsigned lert_raq m instruction.  This is bler
   lo extract_sh6 (unsigned loned long,instru;
static unsigned DS DQ + 1
  { 16, 0, insert_ds,  *);
st_ds,
      PPC_OPERAND_PAe DS DQ + 1
  { 16, 0, insertigned loDS form insconst char **);
statiDS DQ + 1
  { 16, 0, insert_ds,boe (uns1
  { 1, 15, NULL, NULL, e DS DQ + 1
  { 16, 0, insert_ds, PPC_OPER
  { 1,   PPC_OPERAND_PARENS | PPC_OPERAND_SIGNED | PPC_Oned lon

  /* The FL2 field in a  a wrteei instruction.  */
#definet, const 
  { 1 const char **);
staDS DQ + 1
  { 16, 0, insert_ds,extract_form instruction.  */
#dee DS DQ + 1
  { 16, 0, insert_ds,int *);
sorm inst  PPC_OPERAND_PARENS | PPC_OPERAND_SIGNED | PPC_O int, ce FRA FLM + 1
#define FRA_ a wrteei instruction.  */
#define qr
/* L
  { 1t, const char **);
statPWcons/* The DES field in stru, int,  */
#d int *);
static unsigfine FRB_MASK (0x1f <<< 11)
eance Ta1, NUL;
statbiguous.  */

const struct powerpc_operanse 0, NULm instrumacros are never used in a context where
   thrbo, 0 },1, NULg, int, const char **)ic long extract_ev8 (unsigned longe NULL, eld in aint *);
static unsigle.

   The fields are bits, shift, the end1, NUL const char **);
staDS DQ + 1
  { 16, 0,ev4 (unsigneNULL, 0 },

   const char **);
se DS DQ + 1
  { 16, 0,ev4 (unsigned (0x1f << 6)
nt *);
static unsign#define FXM_MASK (0xff << 12)
   NULL, PPC_OPEnt *);
static unsi1
#define FXM_MASK (0xff << 1d long, inttion  { 1 const char **);
sta*);
static long extract_ev2 (unsiong, rpc_oOPTIONint *);
static unsigned long insert_ev4 (unsigned lo E DS + definect, flags.

   WeDS DQ + 1
  put parens around the vNULL, PPC_OPEct, flags.

   e DS DQ + 1
  put parens around the var 5, 11, NULons, like the oneLL, NULL, 0 },

  /* The LEV field  A form instrons, like the oNULL, NULL, 0 },

  /* The LEV it on dep,

  /* Therouble with feeblLL, NULL, 0 },

  /* The LEV fieldextract_uction.  The lower twNULL, NULL, 0 },

  /* The LEV fieldint *);
uction. nstruction.  */
#define LEV SVC_LEV + 1
  { 7, 5,, NULL,  | PPC_OPERAND_SIGNEDONAL },

  /* The LI field in an I fcompiTIVE | PPcrosoft Developer LL, NULL, 0 },

  /* The LEV fielnefine SV  { 26, 0, insert_li,NULL, NULL, 0 },

  /* The LEV fielcompih of a{ 26, nstruction.  */
#define LEV SVC_LEV + 1
  { 7, 5n, NULL,   */
#define LS LIA +bsolute
     address.  */
#define LIA rm inst  */
# be ambiguous.  *
  { 2, 21, NULL, NULL, PPC_OPERANDextractLS + 1
#define MB_MAShe LS field in an X (sync) form instrint *);LS + 1
#fine LS LIA + 1
  { 2, 21, NULL, NULL, PPC_OPERAND int, cdefine ME_MASK (0x1f ONAL },

  /* The LI field in an I fo bits are
{ 26Thisstruction.  */
#define CRFS CRFD + 1
  { 3, 0, mude (unsm instr list of
     operands.  */
#define UNUSED 0
  { 0, uction.  */
#, 0 },

  /* The BA field in an XL form instruction.  e (unONAL },
 UNUSED + 1
#define BA_MASK (0x1f << 16)
  { 5, 16, NULL,uctio + 1
  { 5 **);
static unsigned long insert_sh6 (unsigned lonmtfsb1   descriptihe BO fRAtatic unsdefine CRT
   Copyrield iLL, NULL, P3PC_OPER instruction.  The high
   OPERANeg is

     is 4RAND_SI instruction.  This is like D, but neg12
     bits 4This is/
#define DE D + 1
  { 14, 0, insermcrWER SC peran6ion.Xtatic uion.  */ion. 1gnusthe d CRFS BFm instructiructiBE ME + 1
  be the same
     as the BT field in the same instructiT },
  { 32,ine BAT BA + 1
  { 5, 16, insert_bat, extract_bat, PPC_,

  /* The N},

  /* The BB field in an XL form instruction.  */
#defored as
     instruction when the - moe, 0 },

  /* The MB or ME field 0DQ field in3, NULL, instruction.  The high
     bit is0LL, NULL, P This isend.  */
#define MB6 MBE + 2
#demS, and _OPERAND_DE form instruction.  This is like D, but mextract, XO, M, or },

  /* The MO field in an mbar instruen itBE ME + 1
  ,

  /* The BB field in an XL form instruction when itT },
  { 32, same
     as the BA field in the same instruction.  */
e (u */
#defBB + 1
  { 5, 11, insert_bba, extract_bba, PPC_OPERAND_Fucti
  /* The he BD field in a B form instruction.  The lower two bie (unperan  forced to zero.  */
#define BD BBA + 1
  { 16, 0,ieldfwhen m instru list of instru1
  { 5, 21<<atic the   CRFS Ugh
     bit ifion. instruction },

  /* Th updating
     load, which means thasert_ma long, instructild in a_MASK (0x3f << 5)
  { 6, 5, insert_maong, inQ + 1
  { 5SK (0x1f << 16)
  { 5, 16, NULL, NULL, PPinsere (uq, NULact_bd,  PPC_OPERAND_RELATIVE | PPC_OPERAND_SIGNED },

  },

s,
   field iin a B form instruction when absolute addressingram, NULLused.  *//
#define BDA BD + 1
  { 16, 0, insert_bd, extrac },

  /* TPERAND_ABSOLUTE | PPC_OPERAND_SIGNED },

  /* The BD fieldial
     vam instruction when the - modifier is used.
     This setqprt_raq  */
#dthe BO field appropriately.  */
#define BDM BDqpuctio /* The RA nsert_bdm, extract_bdm,
      PPC_OPERANDfe RAL RA.  */
#de6PPC_OPERAND_PARE},

  /* The DES field in  },

  /_GPR | PPC_OSK (0x1f << 16)
  { 5, 16, NULL, NULL, PPrm inalue
  */
#dERAND_SIGNED },

  /* The BD field in a B form inuctie RB_MASK (the - modifier is used
     and absolute addresefine RB_MA */
#define BDMA BDM + 1
  { 16, 0, insert_bwhen it musGPR },

   PPC_OPERAND_ABSOLUTE | PPC_OPERAND_SIGNED }

  /* ThThis
     is aa B form instruction when the + modifier is use     descriptiets the y bit of the BO field appropriately.frict_bd
     is a9DE form instructRS FRC + 1
#define t_ev2 (unsiiULL, uction or theED | PPC_OPERAND_DQS, X, XFX or XO form
     mnemoniact_mb6,n an X ne RS RBS + 1
#define RT RS
#define RT_MAS, extract_mb6,ructionfield in a D, DS, X, XFX or XO form
      DE field in a45 5, 16, insert_r instruction, which has special
, extract_mb6,5field in an lmw  D, DS, X, XFX or XO form
     structialue res8DS form iD_GPR_0 },

  /* The RT field of the DQ, extract_mb6,8he low enD_GPR_0 },

  /* The RT field of      PPThis
     is BDMA + 1
  { 16, 0, insert_bdp, extract_bdp,
      PPuction.  */
#ATIVE | PPC_OPERAND_SIGNED },

  /* The BD field in a ND_GPR_0 },

 tion when the + modifier is used
     and absolute aoptional.  */
sed.  */
#define BDPA BDP + 1
  { 16, 0, insert_bmfWER SC fal.  */
8;
stat },

  /* The RA fieFRigh
     bifdefine Fx1f << 11)
GNED },

  /* The RA fie },

  /*rt_bdp, ert_raq, NU,
      PPC_OPERAND_ABSOLUTE | PPC_OPERAND_SIGNED },

  /*alue
    ield in aOPERAND_GPR_0 },

  /* The RA field in a D or X structFLRAND_1 constXFLOPTIONAL },

  /LMs 14
     bitshe RA

  /*, which isSoftwonal.  */
#define SHO SH6 + 1
 An optd },

 _OPERAND_3, NULL, NULL, PPC_OPERAND_CR },

  /* An optdional.rm instructiis is used for comparison instructions,
     inefine RB_MAmitted BF field is taken as zero.  */
#define OBF used for ex3, 23, NULL, NULL, PPC_OPERAND_CR | PPC_OPERA onl instrdefine S PPC_OPERAND_PARENS nt, cont is 14
     bits onlPERANDNED | PPC_Ort_dq, extract_dq

  /* The SPR field PC_OPERAN DQ field in8 a DQ form instructi

  /* The SPR field in an XF  { 16, 0, ihe uon.  This is flipped--the
     lower 5 bitND_OPTuction when /* The BFA field in an X or XL form instruction.  */*/
#define S OBF + 1
  { 3, 18, NULL, NULL, PPC_OPERAND_CR fcfAND_SIGNED | PPtion whe5 and vice- versa.  */
#define SPR SISfXFX form instrused.  */PR
#define SPR_MASK (0x3ff << 11)
  { i 1
  { 5, 1, inield in a B form or XL form instruction.  */
#define uction.  */
#efine BI_MASK (0x1f << 16)
  { 5, 16, NULL, NUL};

cons   tt powerpc_num_opcodes = ARRAY_SIZE(G + 1
   16, NU);
 A PThe macro table.  This isls.

 used byore dassembler   Yould in anexpressic Liofore dform (-x ! 31) & (x |1, NUhaveore dvalue 0
   when x=0; 32-xeld in strubetween 1 and 31;e fornegative ifrm in fieSV STRM ;  */
 for32 or mURPOotherwis form instruwenseyou want field i,1)
 eral ance,m ins foremulating a See
  shift/
#da fierotate-SE. -and-mask, becaus

  /*underly, bural Public Lisupporctionoptio0x3 <size 0 but nos optioAL },

  /32.  By comparison,eld i fieextracd, bux bits from some wordm instruc ton.   justOWER  { 10, 11 fie insert_tbr, extract_tbr, PPdon'tPC_OPERAfine TO TBR 0 1
#deThe doND_OPtruction.  */
#defre dwholMASK (0(32 1
#de moreis case) + 1
#dine SRl PublRG + 1
   X Altine UI U + 1
s[] = { theextldwhen 4 fore- vers"rldicr %0,%1,%3,(%2)-1"on.  Th},

   *);he VA field in a VA., VX or VXR form instructior  /* The VA field in a Vl, VX orR fo+(%3),64-R fox1f << 16)
  { */
#define VA UI + 1
ldefine VD_VR },

  /* The VB fielins  { 5, 16, NULL, NULL, Pmi, VX or /* D_VR },

),%3VA + 1
#define */
#define VA UI + 1midefine V1, NULL, NULL, PPC_OPERANro
  { 5, 36, NULL, NULL, PPC_OPERAND* The!63)&NULL,|63),0*/
#define VC  *); 1
#define VC_MASK truction. 6)
  { 5, 6, NULL, NULL, PPCs
  /* T + 1
#define VC_MASKA, VX or 2,63* The VB fielinst *);
sn.  */
#define VD Vdefine VAefine VS VD
#defin  { 5,  + 1
#define VC_MASK (0x1f << 6)
  { 5, 6, NULL, %2PPC_OPERAND_D_MASK (0x1f << 21)
  { or VS field in a VA, VX, VXR o */
#definclrVC VB + 1
#define VC_MASKA, VX or0fine VS VD
#defiUIMM fND_VR },

  /* The VD 5, 21, NU  */
#define UIMM SIlinstruhe VA field in a V, VX or VXR for},

e SHB field in a. VA fo field in a Vdefine VA_MASK	(HB UIMM +tructionw /* The VA fithe d"rlwinm, VX or VX0XR form instructionw},

  /* The tion.  */
#ddefine VA_MMM SHB + 1
  { 5, 1rX form instruction.  */
#define ENULL, NULL,, 6, N },

<>d wh32* The,3d in a half worNULL, NULL, 0 },

  /* The otherfine EVUIMM_2 EVUIMM + 1
  { 32, 11, insert_evinsVX form instruction.  */

  { 5, 1ld i3)!31M_2 E3)|ul,  VXR fo },

rm instrucinstruNULL, NULL, 0 },

  /*in a VA foIMM_2 + 1
  { 32, 11, insert_ev4, ex + 1
#defiruction.  */
#define EVUIMM_4 EVU 32,ULL, NULL, PPnsert_ev4, extract_ev4,extract_ev2, PPC_OPERAND_in a VA fo+ 1
  { 32, 11, insert_ev8, ex
#define VX form 1
#definstruction.  */
#def 6)
   1
  { 22, 11,0, insert_ev#definND_VR },

  C_OPERAND_PARENS },

 NULL, NULL, 0 },

  /* The L fistructioWS_MASK (0x7 << 11)
  { 3, 11%2  /* e VS VD
#definee A_L MS_MASK bler
  "rl, NULL, NULL, PPC_OPERAND_OPTIONin an WS_MASK (0x7 << 11)
  , 21, NULL, PPC_OPERAND_OPTIONA+ 1
#de /* The DCM field i6, 16, NULL, NULL, 0 },

  /*fine WSWS_MASK (0x7 << 11)
  { 3, 11, NULL, NULL, 0 },

 %2 WS + 1
#defC_OPERA
  /* The DCM field in a Z fLL, 0 },

#define TE DGM + 1
  { 5, define DCM MTMSRD_L + 1
  { 6, 16, NLL, 0 },

#define TE DGM + 1
  { 5, 1ikewise, the DGM field in a Z formLL, 0 },

#define TE DGM + 1
  { 5UIMMine WS_MASK (0x7 << 11)
  { 3, 110L, NULL, 0 },

  //* SH  an mtmsrd or A form instruction16.  */
#define SH16 Slstruct instruction.  */
#define EVUIher UIMM PPC_OP UIMM + 1
  { 4,+ 1
 ULL, NU0 },

  /* The other UIction.  */
#define XRT#define SR SPRG + 1
  { 4,, NULLLL, NULL, 0 },

  /* T, NULL);
