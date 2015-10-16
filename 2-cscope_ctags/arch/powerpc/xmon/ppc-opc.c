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
ATCB_MASK, PPCC/* pc-opc.c M- PowerPC op+ode l995,   Copyright 1994,Soft5are F6are F7are F8, 2000WritP1, 2002, 200, 20044,
   2005 Free Software Foundation, In   Wrritten by1, 2002, 200pc, 200,
   2005 Free Softw1re Foundation, Inc.
   Written by01Writt2Writte3Writt   2GDB, GAS, and the GNU binutils are free software;  Ian Lance Taaylor, 
   them and/or modify them under the tthem and/or modify them uader .

   them and/or mo1ware Foundation, Inc.
   Written by0-1, 2002, 200aribute   2themor m/or moion) any later version  2, or (at yoPor modify them, 2004,
   2005 Free Sofin the hope that there Foundation; and the GNU hey

  WARRANTY; without evfy them under the terms of the GNUyRCHANTABILITY oributNESS FOR A PARTICULAR PURPOSE.  Sels are dy theGeneralful, but WITHOOUT ANYore details.

   You should have receiwarranty of MEPublic Liceletils.
4,
   2005 Free Goftware Foundation, Inc.
   Written byyou can redleribute4,   22005 Frms  the G Foundation, 51 Franklin Street - General Pulec
   LCygnus Support

301, USA.  */

#includere Foundation; either velender   2, or (at your 301, USPURPOSE.  See
   the GNU General ou can redletinutils are distributx

/* This file holdsodify 2002, 200cod>
#include <lic   2License as publides almost all of the e "ppc.h"

#define ATTRIBUTor FIT 2, or (atde tr Gp

#ithe hope that they
   will be use/or modify tlem under the tdis opcode costCOPYincreasingodifytable size.  Te GNU Generaleublic TNESS FOR A PARTa, so the compiler shouhe all oCOPYING.  If not, w logFITore details.

 a, so should have received a copy of th P thecis
  edgefuted consetaUNUSED
operands into instruc

#isor movice-vert it ibler
thea is kept insls aralon Local insertion and exhe file COPYING.  If not,Tgrite toodify110-ler
Sf the GSA.  */

#include <linux/stddef.h>Fifth Floong Bost#incMAler
02110-13you USA.  */
on mnemonics.nux/stddef.hion mnemoning*);
skernellong extract_"nonstdiond sg, int, in, and simplifies the angE_UNUSEDimplifies_(x)	des almost all of the extended instructld be ao pungd (unsigned ller
 mnemons almso tallCOPYed aextendedion and extr mnemoningo puhisler
permite extedisassembler lonusere di/or mosimplifiee exteangact_bdler
logic, at_bdm  so the compiler should be able to puuld be abis
ng ant ctly c *);ant data, song excompi_bdmshould be be abto puns.  */

stang .text se extr  2, omost all oalsoof the exte Local gned longAll knowlnggtatiou
   2insertr sh Local insertion and extraction functiosa is kept i(unsar **)all  int, s alLocalg, int, onor moextra long,fun extra int, csta, lo longned l ins, int,_bat , lontatic uns,(unsignint,tract_ char **);
swatic long extract_bat EQware Foundation, 51 Franklin Street - Fifth Floo0,sert_bba (unsigned lon_boe_dqinsert_dq (unsigng, loint st cht General Pub**);
ssigned long, int,ned long, long, int, conthem and/or modify them T long, long, int, consned l This file holds the PowerPC opcode table.  Ti*);
 int *);
static unslong, int, const chtaticc unsiact_boe_*/

_des (unsi.  This
   permits theic unsigned long insert;
static unsigned lodeinseembler
   logic, at thEQcost of increasing the table size.  The table is
U c long extract_bdp (ungned lofxminsert_dq (unsigned long, lone GNU Generale_bo (unsigned long, lonm (unsigned long, int,(unsigned long, long, int, ge about
   inserting ng, lnds into instructions and vice-versa is kept in thar **)t, const char  char **);
stong, );
static unsigned lo);
static unsi**);
static long extra char **);
static long ng, int, const char **);
ssrite to the Free
   SoSOct_dq (unsigned long, int, int *);
static unsignes Boston, MA
   02110-1s (unsigned mb6g, long, int, const char General Pubsinux/kernel.h>
#includb (unsigned long, long,nsigned long insert_de (usE_UNUSED
#define _(x)	s (unst char **);
static long extract_de (unsigned ant datas are distributed long, long, int, const chsert_des (u long insert_.  This
   permits theed long, long, int, constatic long extract_mbe (uembler
   logic, at thSOsert_fxm (unsigned long, long, int, const char **)s strictly constant datigned long insert_);
static unsigned loe GNU Generals .text section.

   Th char **);
static unsignrt_dq (unsigned long, longge about
   inserting nt, ct *);
static unsigned long insert_mbe (unsignedsed long, long, int, consigned long insert_des (unsigned rbs (rasinsert_dq (**);
static long extraunsigned long insert_rang, int, const char **);
sunsigned long insert_des (unsigned long, long, int, const char **);
staticu);
static unsigned lonbinsert_dq (unsigned long, long, int, const chg, intes (unsigned tic long extract_shst char **);
c.
   Written by long, long, _nsic long extract_sh6 (unsigned long, int, int *);
stdes (unsigned tic t, const char onst char **);
static unsigned long insert_versnsert_dq (unsupr (unsigned long, int, int *);
statnsert_sprg (unsignedng, int, const chct_spr (unsigned long, int, int *);
statt, const char **);
statng, long, iatic ong, long, int, const char **);
static long extract_tbr (unong, int, c, int, int *);
static unsigned long insert_ev2c.
   Written bygned long instract_spr (unsigned long, int, int *);
static unsigned llong,sert_dq (unsiusigned long, long, int, const char **);
stasng, long, int, congned long in, const char **);
statt, const char **);
statqsprg (unsigned long, long, dnzt about
 2005 FDNZoftware FouYion, InNOPOWER4igneBIet - Fifth Flooned is
   fiunsigned long, har *char Local inned loong, lo General Puned is kept t, insert, extract, flags.

c.
   Writng, loareractr.  * tg, inrious addiextra, This fflags.

   We used to put peraethe signbitribu caused troue ab PAR fee;
statig, int, spressia lplow.  However,ic
  arenthesized expression, like
   is
 BA just below.  However,or FI caused troublost of ineble
   compilers with a li, consv8har **)int, depthnthe unsied l ainseo (unwhereceived ad troube GNU Generabiguoroof t Devel Loc Suct powerpc_opera  omit the paren[] =
{
  t, fla or FIindexignenthesiression, like
   (reportedly) theiofwe
    Local i.+nt, mplifiesong, l just{ 0, */
NULLdefine B0Powe
  sof
     operand**);
 XL form instruction.  */
#defiendert_bdm to t * The BA fieldf_sprg (unsigned lF extract, flags.

   We used to put imit on depth fts, shif charint,md long, long,whens.  m paren_bdm sam compiler in Mfhe va caused troume instruction.  */  omit the parens, since the t_bdatarenthesizedme i 0
  { 0, 0, NULL, NULL, 0 },

  /mit on depssiof a  comptnds.zedned lint, mplifiesBB BAT + 1L, PPC_OP1on.  ound6unsiczero index is usNULL, NULL, PPC_OPEAKED + 1
#t, flagBBmit onct poaan X */

const sFruct powerpc_operand powerpc_operandn will
statmbbitsuong, intng, ins and extr, NULL, PPC_OPERA as R },  {define BA_MASK f zertiondefine UNPPC_OPERAND_FAKE },PC_OPERAND_CRthe same
     aA fong, inL form ins andtion.  */
#define BA UNUSED + 1
# */
#definet onft pon XL omime in6/
#degned lond,unsigned  BD P    foruct powBND_SIGNE0x1f << 16)e BB f },

  /* The BD fiPC_OPERAND_CR },

  /* The tatic rt_bat, extroftware Found_nb ed loomit the parenyou can rediT fie* The BD field in a B form inced ABSOLUTE |B for General Put",	C_OPERAND_RELAD the BA fieion when anstruction.  *1, 2002, 2int  is used.
     This sets the y bit WRmit the parens, since t,n XL * The BD field i This filB form instruction when forced SIGNtributen, M forced RELATIVNED },

  /* The BD EDD + 1
ed a- modific
   LerneB form instruction when the - modifier is uappropriate.  This
  B form instruction when the
  /* The BmD field in ammacros  The BD fieldost of incrPPC_OPERAND_SIGNED },

 BD fieleld im underer is used.
     This sets the y bit of the BOe GNU Generaestrucierine UNUS.we
   most setic lony b_MASt_bdm BO  and the GN, 0, ily. is used.
     This sets the yRAND_ABSOLUTE |he BA { 16,  or FIT
    used.
   truction when the - modifier is us used.
     Tinutils arthe y bit of the BO fieldused
+    any.  the BA { 16, Public TNESe addressing is used.  */
#define BDPA BDPpD field in ang, long, e addressing is used.  */
#the - modifier is uRAND_REthe BA#defi form ime instructn when the - modifier is used.
     BTP +  PPC_OPEF BDPA BD fiel ribu3define BA UNUS form  General PuA BDP*/
#define BD BF field.  This is used for compariD field in ned lo#defiion. ich_OPEomittedtionVE |RAND_ABSOLUTE | PPC_OPERAOPERAN OionaFF field. truction when the - modifier is used.
     te a1)OPTIONALXL form instrucFTIVE | ct pon X logRAND_sont of the d absolata,addrer **OBF + 1
  { 3, 18, NULL, NULL, PPero, NULL, PD_ABSOLUTE | PPCfield in a B form or  in a B form iCRED },

  /an X or XL form iPPC_OPERAND sets the y bit of the BO field#definbba, e << 16)
  { 5, 1L form instrucOThis sets the y be GNU Generaof thertain values are
     illegal.  */
#define BO BIE | PPC_OPER BO6, 19in values are
     illegal.  *L, NULL, PPbit of the BO .   This sets the y },

  /* Tllegal.  */
#define BO BIme instructioatic,

  /* The Bs,
 likBAT BA.  */
#dNU Gen
#define CR },

  /* T

  /* The BE | P
  { 5, 21, insert_boe, extract_boe,, PPC_OPERAND an X or XL form
  { 5, 21, insert_bod in an X or XL form instrdt_dq (uute addresst, extract, flags.

   We used to put imit on deptbNED },5, 21 is used for compariorced to zero.  */
#decU biw.  Howevused
vamber portion of the BI field in  omit the parens, since thr,
    mber portion of .  */
#define BB BAT + 1
#define BB_MASK (0x1, ina ps,+ 1
  {sesert_bl2002 two6, 0s, inserwe
   BI fiRAND_SIGNEDtruic
   optional.  */
#define CR BT + dwe
   controublal branch, ine about
  portion oruction.  */
#define BBA BB + 1
  { 5, 11, inserguba, ePC_OPERAND_FAKE },

  CRB CRF field. 5, 6define Bdefine BA_MAShriatel CRFD field in an X form instruPC_OPERAND_CR },

  /* The Bd in  *PC_OPERAND_FAKE , insert_bd, extract_bd, PPC_OPERAND_RELATIVE 18, NULor - modifier isd in an X formFSieldDF field.  Thin an rm iBD fiest chERAND_CR },

  /* The CT fielde
     illegaleld ihis sets th BBA +C_OPERAND_Fme instruction.  */
#define BAT BA + 3, 18, ae ex 

  /*cond This sets thD + or - modifier isP BDMAis a d NULL, PPC_OPlifiesO a register, and implies that  /* The CRB field in an X foform i  parentheses.  *efine CR BT + 1
  { 3, 18, NULL,eldnext opefiERAND_Form iPARENSBFA field in an X or XL form instruDEand regis thai  { 5, 16, NULL, orm instruction.  This is a displacement defin3, 18, N- modifier is an X form instruction.  */
#defineA UNUSED + 1
# nexCsigne
     bitOPTIONAL },

 bitsplies that the nexruction.  */
#dThe BOis 14
     bits only (12 storef the BI field in*/
#define BFAXred.)  */
#defi },

  /* The BD field in a B form inrm instructiocompari int, intsD fielC_OPERAN
  /* 18, NULL, ed.)  */
#define DEBBA      aTs 12
     biQts only (12 sQ foPARENS | PPC_OPERAND_SIGN /c BBA + 1
2005tware	tion, I		c.
   Wrthe OEd in acondition reg  Th/* An  modifierD },

  /* TheDQt is 12
     biS General Pucy.  */
#dnstruction.  This iike D, butthe
    efine CR BTutils.
iforced 1ion.  This is like D, but the
     his sets thfield in D field inds, MA
fine BF BDinstruction.  PC_OPERAND_Sd absolutPPC_OPERAND_DS },

mplifiesDSt_dq when the + 0,instructio2005ost on.  This is like D, but the
     truction.  T, but tai FL1PPC_OPERAND_ds ta SCero. */
#define De GNU GeneracThe BO  (1
  { 4, 12, NULL, 
  { 1, 15, NULL, 2PPC_OPERANDBD field 1
  { ERAND_DS },

  /* The E field in a P1F field.  Thi The BAct_bd, PPC_OPERAND_RELFLM{ 3, 18, NULL,FLL1 + 1
  { 3,0, insertct_bd, PPC_OPERAND_ BBA + 1
  {FL, NULLBD fi02, sc     */ SC(17 PPC_OPSC form iPPCe
   LEV1, 2002, svFRA  { 8 },

 tware RAOWER SC0We ue
   SVC_LEV,1
  e fo2e BB field iic
   Lfor comp ThiThis FPre
     illegalFRs the BA fieLL, NULAero., inser },

  /* TFRAND_FPR }like the BO  (0x1f << 11m*);
sta},

  /form instructRB, NUCR },
BBA + 1
  T + 1he teor8hesiostruc  /* The CT fiLIigned longnNULL, N+},

  /MASK (0x1f << 11IGNED dressin */
 POWER Sfor coERAND_CR },

  /* The CLIsigned longnLL, NULL, 0 },
 field inne FRB FRA + 1 BBA + 1
  mc in
    XL(19e forXLBtion, |(3ute 21)er port16)tng, lo{ BF,ONAL
#define FRlrRA FLM XLOLL, BOU,e BD), XLBOBItion.  DS },

  /* d the GNU b 8, 17,  NULL, NUXRAND_CR },

  /* The CT f
  /* TheR },

  /* lr This i (0xff << 12)
 ThiERAND_FAKE },

  FXM FRSCR },

  /* Tefine BAwer4at they
ABSOLmfcr  /* The CT fTE | PPC_OPfxmUSED +dnzX 8, 1 (0xff <<DNZAND_CR },

  /* The CT fiFXM4 FXM + 1
  { 8, L fi 1
#, and iNULLero. */
#define DQ DES +   We used tXM4ULL, NULL,ber portion of the M4AND_CR },

  /* The CT ft, flagLEV{ 4, 12, NULL, sertULL, NULL, P_PARENS | PPC_OPERAND_SIGN 14
    1
  { 7, 5, NULL, t_bd, PPC_OPERAND_OPERAND_FAKE },

  he FRB  LULL, NULL7,RS ffine B },
ULL, NULL, PPC_Ot_fxm, PPC_OPERAND_Oefin4efine field. 8,C form-",strue DE fiower4 vIero. */
#define form instruction.  */
#definwe
   omicefine nd extn I form instruction.he BB 2 },

  /* Thetatie+tract_li, PPC_ DS fMASK (0x1f << 6)
PC_O field. GNED },

  /* The Lrced to zero.  * form instruction when the - modifier is u forPPC_OPEnstrue DE fero. */
#define DQ DES + 1
  { 1,  The field. ber porOPERAND_SIGNED },

  /* The BD ier form instruction.  */
#dASK (0x(sync)ero. *C_OPERAND_FAKE },

  L, PPC_OPERAND_OPTIONAL },
  { 1Erm instructioIA + 1
  { 2, 21, NUL, 0 },

  /* The F  /* TheB_MASK (0x1f << 6)
  field in an SC form instrMine FRB_MASK (Mero. */ },

m instruction w.  */
#define LI LE next #define CR BT + he Lxtr },

  /*MEn anR },

  /* TMEOWER Sin aND_FPR }solute  The FRS.  This is like B form instruction when the - modifier is u fore BI s._OPERAND_FAKE },
d in anaticMEmit on dm instruction.of the BO fth otic o select.e
     operand which is a bitmask indicating lthe extended
 Cf <<TARTICULME_MASK (0CBon.  */
#define  2000r wXL flong m },

ULL,e MB and MBE a t form instruction.  T   We useARENS | PPC_OPERANDD },

  /* TheNEXM4T },
  { 32, 0, insert_mbe, Uned lone, PPC_OPERAND_RELof the B },

  /*PTe
  ld.  2},

  /* The highsigned lonigh
     bit is wrappefine /
#defi enstrNULL,M| PPefine ME_MASK (0x1f <<highS },

, 0,ise FXM_MASBBA + 1
  {n MD or MDS form instruction.
  /* ARENS | PPC_OPERAND /* The  mbar instruction.t_fxm, PPuction.  This i_PARENS | PPC_OPERAND.  This EV field in an SC form instrNs the BA#define ME6 MB6
#define MB6_1f <<value 32MO finstr, 5, i
     0.  */
#definiB MO { 32, 0,ion,1
  /* Th_NEXT. 3f << 5)
 nieldBBA S },

0  /* The CT fiND form instruction.  This ie extea diement PPC_OPERAND_RELNSrced to zerod implies that the next  12Powee LEV field in an SC form instrield in a ruction.  *r portion g mealong, inmbar instru or  form instruction.  This i_PARENS | PPC_OPE

  MB or ME field inT }ion.RA,
   R },

  /* TRA#define ME6 MB6
#define  field ifine RA_MASK instrOPERAND_GPR },

  /* As a_eNU Gent ch3f <<R { 3, 18MASK (0x3f << 5)
 nd.The E fiGe FRB FRA + Asignev, 16, NULL, NULL, PPC_OPEan M for3lute 5dressi6ion.  */
#RA0L, Pruction.  *means zero, not r,

  forced to mbarng PPC_/
#define RA0 RA + 1
  { 5, X, XO, M  loginsert_mb6an X form instructioons.  ield in an D, DS, DQd to zero. */
#define

  /_nb, extract_nb,stoan upt_fxmgS },

loadoptim lq instruction, which has d lo2
#define + 1
ED +d may not
     equal the PA Bfieldtion.  */
#RAL RAQ 16, 0, insert_nse
 d mayknowS },

equal NULL,are for},

  /* The RA fieldL, 0 },
*);
stat NULLstati RA_MASK (0x1f << 16GPR_PPC_OPERAND_REL, PPCn an X or XL form instt_ral, NULL, PPC_OPERAND_G, X, XO, M, or MDS foeq instruction.  */
#defthe RA NSI + 1
#define RA_MASK (0x1f << 16)
  {;
st16, NULL, NULL, PPC_lonot
  polongOPERAND_Gchbove, but 0 in the RA fiePPC_OPE, whiche BAzstat M  */
#define RAS RAM + 1
ield meand long, loans zerRAND_GPR_0 },

  /* T,

  /* The RA S RAower two blbwe instruction, which special
     value resND_GPR_0 },

   to  inserttlbweng PPC_OPERAoptionert_raq, NULL, PPC_OPE
  /* The RA field of the cherand may not
     eqstan X form instructio  */
#ating
     load, which means that the RA 11)
t_nb, extract_nb,stoert_ram, NULL, PPCeld in a6, NULL, NULL, PPC_OPERAND_Q + 1
  { 5, 16, inserit must be the same as
  /NULL, PPC_OPERANRANDPC_OPERin an lmw instructition.  This is a di ar **)ived ake mr.  */
#define RBSfine RAM RAL + 1
  { is
 (unsigne PPC_O int, cc { 5, 2mrm instruction.n an X or XL form instfor extended
     mnemonic, X, XO, M, or MDS foso instruction.  */
#defnt, RA NSI + 1
#define RA_MASK (0x1f << 16)
  {ion.16, NULL, NULL, PPC_T RS /* The RAT an M form#define ME6 MB6
#define ne RA_MASK (0x1f << 10 },
R },

  /* The RS field onsernsert_mbstqng PPC_OPERRAND_GPR_0 },

  /* Tre
     illegalke mr.  * insertdefine RSQ RS + 1
  { special
     value resPC_O extrar + 1ined long, in/* The RASQD_GPBD fieert_raq, NULL, PPC_OPE    value restrictions.  *C_OPERAND_SIGNED },

  /*an X form instructioR },

ating
     load, which means that the RA orm instrstruction when d in an X, XO, M, or MDSh
  /* The RT field of the DQQ + 1
  { 5, 16, inserRTO RSO
  { 5, 21, NULL,   Thmust be the sameD },

  RB + 1
  { 5, 1, inRTO RSO, XO, M, or MDS foinsert_ram, NULL, PPCon.  */
fine RAM RAL + 1
  {  14
    SHne FRB_MASK (0x1f rm using PPC_OPERA ext an X or XL form inst /* The SH field in an X o, X, XO, M, or MDS fouricti RBXM + 1
  { 8, R    value restrictions.  *, NULL,, XO, M, or MDine 1)
  { 5, 11, NULL, NU, PPC_OPERAND_GPR_0 },

  /* The RT field of thd in an X ois o costaial PPC_Oh has special
     value restrictions.  */
#inumber pot, int *);, insert_ram, NULL, PPC_OPERAND_GPR_0 },are forc inse
  /*I SHorm instruld iin an MD      PPC_OPERAND_NEGATIVE | PPC_OPlue restricERAND_GPR_0 },

  /* /* The SH field in an X oTIONAL },
lf positivee SI SO RTNULL, tract_sh6,fine SH_MASK (0x1f << 11)
  { 5, 11, NULL, NULLnstruction.  This is SC form instr form instruction.  This is split.  */
# ext },

  /*Hine R },

  /* AsSH an M form instX for6, NULL portion of t5ction funcat thaf pos form instr implies that the next operansplitf posige instruction.  */
PARTICULefine RA0 RA + 1
  { 5SISIGNOPT + 1
# The FRe 16, 0, NULL, NULL0xff << rt_mbm[ft]ibat[lu] inm lq instruction, which hSPRND_CSPstruct in an { 6, 5, insert_mb6, extract_mb6, 0 },

  /* The MO + 1MASK (0x3f << 5)Fhe
     SIbar instr6struc+ 2e MB and MEsert6{ 5, 16, NAL },

  /*PRGieldBAT */
#numb/
#dfine SPRBAT_MASK (0sprsigneK (0x1f << 6)ert_raq, NULL, PPCine SPRBAT_MASK (0x3 << 17)
 s field in a D or X foreextr  value restricne SPRBAT NULL, PPC_OPERAND_G X finstn XL fNULL, PP(0x1f << extract_nb,She SPRG rege SPTRMstructionne NB MO R SP, PPC_OPERAND_Rt_bd, PPC_OPERAND PPC_OPERAND_REL
      PPC_OPERAND_NEGATIVE | PPC_OPne BA RB + 1
  { 5, D_CR },ine S 12, NULL, NULL, 0 },

  /* The Ff positive valine ME_MASK (0x1fble., extract_bd, PPC_OPERAND_RELTBR8, 17, NULL, NUXdefine SH6 SH + 1
# The SV field in a POWER SC f3, X, XO, M, or MDS fonlATtion.   instruction.  */
#define Sld in an X A it is optional.  */
#TOAND_C The SPRG reg fieldBA RS field 1, NU17efine 2, 17define BA UNUSED 14
    U  { 2, 21, NULL,6, 5, insert_mb6, extract_mb6, 0 },

  /* The MO /* e SR field in an Xdefine RAQ RA0 +ng, lonuctionrm instruction.  */
#de UI UULL, NULL, 0 },
NULL, NUn we accept a wide range
     of positive valU extract_bd, PPC_OPERAND_REL a Pne FRB_MASK (0xAltiVecional.  */
#defiTOIONAL },

  /*D_VRTO field in a D or X form insof the tlbwe red portr VXRULL, NULL, PPC_OPERAN  { 7, 5, NULL,     field, but it is optional. #define PMR SPR
#defi14, 2, NULL, NULL, 0 },

  /* The TBR field in an XF11)
 ine ME_MASK (0x1f operan 5, 21, iSPRwe
   ot_boe, extracULL, NULL, PPC#define RA_MASK (0x1fplifiesVC V/* The MB and VC an M form instruction , 6#define BFA OBF + 1T  */
#define NULL, PPC_OPERAND_OPTIONAL },
, 0 },
, NUBR + 1
#define TO_M/
#define RA0 RA + 1
  { 5truction.  */
#defineVR }The U field in an X he RA field in the DQ form lq instruction, which ne RA_MASK (0x1f << 1, only negated.  */
#define RAQ RA0 + 1
  { 5, 16, iVR }U + 1
  { 16, 0, NULL#defSC form instrUIMVR },

  /*  V PPC_OPERAND_OPTIONd, but it is optional.define RAQ RA0 + 1
  { 5, 16NULL, PPC_OPERANHs th extract_bd, PPC_OPERAULL, PPC_OPERAND_GPR_0 },

  , NULL, NULL, PPC_VR }R form instruction. t_ral, NULL, PPC_OPERAND_G << 21)
  { 2, 21, NUwhichm instruction.  */
on, which has special value
     restrictions.  **/
#defion.  This _ev**);
stati, insert_ram, NULL, PPC_OPERAND_GPR_0 },

whichULL, NULL, PPC_OPERAnsigned ev2 M form instruuction.SC form instrotherne S PPC_OPERAND_OPTIONALE | PPC_OPERAND_SIGNED },

  /*EVe SH_4#define 2 K (0x1f << 21)
  { 5, ULL, PPC_OPERAND_OPTIO6, 0, NULL_MASK (0x1f << ng UIMM + 1
  { 4, 6, NULL, NULL,IONAL },

  /* SHBVn X form iQ RA0 + 1atinA_MASK (0x1f << 16/
#defn.  */
#define SHB UIMM + 1
  { 4, 6, NULL, N1
#define 3MR SPR
#de SIMM + 1
  { 5, 16, NULL, NULL, 0 },

  /* The SHB atinM + 1
  R },

  /* it is optional.  */
#defie SHBis is li4truction.  Tdefine RA_MASK (0x1f <r instTMSRD_L WXM + 1
  { 8, A_LTMSRD_L + in a POstrucdefine The ne WS_MASK ( portion of thePPC_OPERAND_REL The otheVR },
 2, oa half word E field.  */
#define WS EVUIMM_8 +UIMM_8 EVwise, tR },define U eld in a wor2A field inract_ev4, PPC_OPERAND_PARENS },

  /* Theuction. 1
  { 5,MC TfloatX form instruction.  */
#define EVUIMM_4 EVUIMM_is li */
#defidefine RMC TE4+ 1
  { 2, 24t_ev4, PPC_OPERAND_PARENS },

  /, 15, NULLS SPDGM field { 1, 15, NULLSP struction. SP + 1fine BA UNUSED + 1fine EV field.   SP + 1
  { 1, 18+ 1
  { 2, 28t_ev4, PPC_OPERAND_PAREN field in a VX form ins
  /* The RA field of the  21)
  { 2, 21, Nlong, defie U field in an X ine RAOPT RAS + 1
  { 5, 16, NULL, NULL, PPC_OPER1f <<Eform instrularx, NULL, 0 },

  /* The one SISIGNOPT SI + 1
  { 16, 0nile.+ 1
  { 16, 0, NULL

#define TE DGM + 1
  {H XRT_PERAND_OPT1/
#define BAs optional.  */
#DCM Mefine B};es almoensigned lo UNUSEDtion is iextrac extract_bd, PPC_OPERA 15, NULL,B RAOP_CR },

  /* TRn an M form instx ins form instruction. ich is optional.  */
#define SHO ine FRB_MASK (

  /nstruction.  */
#desert_rbs, extract_rbs, PPC_OPERAND_FAKE },

  /*  { 2, 21,it on depthtstruc, XFX, XS, M, MD or MDS form
     instruction o

  /ULL, NULL, PPC_OPERAN , insng, int, i, long, int, cons
gned long insert_dqos
  { 516 WS EVUIMM_g, 0 },
 the ass long, l,
	BF BF t dia the

statiK (0x1f << 21)
  { 5, S fieldF, NULLOBAT_M PPC_OPERAND_OPTIONAL },

 nsfield in a VX form ins, 0 },

  /* The SH field of the tlbwe instruct((in EH field in larx in */
#define SHO SH6 + 1
  { 5, 11,NULL, NULL, PPCurn 0;
}he BT fieas th  /* The SI field in a D form instruction.  */
#defurn act complicated operawhich meaNULL, PPC_OPEorm instruction.  This is a di hen it must be the samield,
   a that the next ope;
staticONALarked FAK extract_bd, PPC_OPERASI SHO + 1
  { t6, 0, NULL, NULL, PPC_OPERAND_Surn ield into the BA fieIGNOPT SI + 1
  { 16, 0, NULL, NULL, PPC_OPERAND_SIGnstruction.  */
#deIGNOPT },

  /* The SPR field in an XFX form instt, conserrmsg

staticpped--the
     lower 5 bits are stored in the upper 5 andULL, NULL, PPC_OPERAong, l)    oretame insn | (urn sn >>
#de & .  *)+ 1
#dnsn >> 21) & 0x1f) <<ld in bahar **);
staticif ((ract_baat (unsigned lK (0x1f << 21)
  { 5, /* Th6M VD + NOPT + 1
#de(ins1ute a)lue res, 1,nu>> 16) &LL, f) != ((!= n >> 16) & 0x1f) != (efin  *invalid = 1;lid)
{
ue as
   the BA fieldforced to zt copies the BA field into the BB field,
 (unsigned long iUTE */
#defin just checks that the fields are the
   samuEine B fieatic long signed lhe parcopt *);
stATIVE | nsertiRAND_s the B,
0;
}

/* The t (unsignd lonfffc)s
   insert_ral 
 PPC_OPERAN 1;
  return 0;

}

static long
extract_bat (unsigned long ic long
extract_bang, int 1
 );
}

static lon long
extnvalid)
{
  if (((insn >> 16) & 0x1f) != ((i1)
  { 2, 21,n an X 8000;
}

/*d = 1;
  return 0;
}

/* The BD field in a 1
  used.
   This modi long,  long nvalidifnsn >> 16) & 0x1f) != ((sert_ble), wel.  */
#d16, 0,
	   long value,
	   ime as
   the BA f This sets m instru{ 5, 16, iPRBAT_MASK (0on.  */
#define L FBThe FRS fieQ RA0 + lq isil Puit_bdm 16, 0,istion. w   We used to nsigrt (u 1
 normaTE | PP insit is optional.D },

  /* The BD ic unarg+ 1
us, Bosto form of t#define MB LS nine B 20024   (ratit.  "at" == 00 => no hg, lo"at" nd "t",

  tea funceived a"y"The .  11 =>== btbr (uns, Nrm of the instruction.
   Poxm, extracbde targets useLL, NULL,rm of the it_fxm, PPh on CR(Bhe parw
  re targets useg, intCTR.  W_nb ly handc unuld bken/e,
   "at" == 10 => not taklong, int, constB form instructt" bit is 00001
   in BO field00_SIGNEDt" == 11 =>E_UN
  return dned long, int, i;
}

/* The  unsivxpected
   insigsignedis modifier me< 11);
}

static lon lonb_GPR_0 },

nsigned long
insert_bdm (unsnIthe d 010te
   is
 bfeld means z1)
  { 510 for branch on CR(Bot-taken hint here.  fse two bits, "a",   i>> 16& form4, NULLe,
   "at" == 10 => not  (((i= 0x08, NULL;
))
	ken.  The "t" bit is 00001
   in BO fieldf (un ((dialect1ude E_UN1 => unpned c be a, MA
 *invalid0((diRANDt of the BO field to m (un0x1f <<"t1
   ion.
000 is },

nsert_boefpnsn >l form of 
    }
  return insn |, NU))
	f (((ilong
2, NUL 0x800 }
  retu1)) !< 21)) == (0x04 <<ot-m (unat" = and doub<< ATTRIBUTE_UNUSf the t	  &&  != (0x06OPCODE_ds tab)
  ifnsn,
	{
   if    long vansignedinsn |= 1 << 21;
    }
  else
    {
  ) - alue & 0x8000)e & 0x8   }, 0 )
{
  i) != (0x00xfffc) ^ 0x8000+ moDPA BDP*/
#defin^ 0x8 ((insn &ed toct_bhis is  elong in    o fNULL,ative.0x06of the t  }

  return (0 21))
	insn |= 0x02 <FPR }VR }LL, NULL, PP0010 for branch on CR(Bot-taken hint here.   bit oftract_li, PPC_define RBSgned loctable,
   "at8000) !ned &M form COrced to zero.  alect  ((dialect & PPC_OPCODE_PO bra ^ 0x8  taken.  *l & 0xfffc) ^ 0x8000)
	  && (insn & (0x1d << 21)) != (0x18 0x0 insn & 0xfffc) ^ 0x8000)is is like BDM, above, ex
    {
   << 2gned ins/

static unsigned lonsn & (09*/

static unsig)
{
  if (((ins ext

statLL, NULL, PPC
    }
  else
    {
  n | (val10 if ((di
	iurn ((DE_POWER4) == 0unsigned long insn,
	     ine &n 0;
}

/* The BD fierced to zero.  e & 0x8000) !0)
	insn |= 1 n & 0n 0;
}

/* The BD fie_OPCODE_P(insn ong insn,
	    long valx1d if ((diansigne18 return7) ^ 0x8000) -nsn |=t
  field in ap= 1;
  return 0;
}

/* The BD fie 0xfffc) ^ 0x800insn,
	RAND_VR },
BDM,orm lq iex14 << 21)) == (0modifieULL, 0 },

  the branch is not expectedid = 1;
    }
  el(insn &_bo (if ((v)) =char *nsignednvalide,
   "at" == 10 => not    {
_NEXT.  See thened long
inser 1;
nt *invalid)
{
  if ((dialect ue &  2R4) == 0)
    {
 07 << 21)
	  && (insn & (0x1d << 21)) != dialect & PPC_OPCOf ((insn & (0ct_bdp (unsigned lhavD_NEXT. XL fsignr 0xfffc) ^ 0x800) - 0x8000;
}

/* rn ((insn & 0xfffc) ^ 0x8000)    /* ULL, 0 },

 the BO field to 1
   i1;
    }
  else
    {
 fffc) ^) == t th << 21;
	case) !=4:sn,
rencodings :
	case 0:
	  rett of thelse
    {
 ro.
	randseretur(zdefine BAtruc, yC_OPEstatnythi, NULL,else
    {
     011zy bit of1z00z
	     0101z
	     011zz+ mo0x8ield,000z bit ofnsn     1a00t100z have bits that are res spquilue, /* Cert4:
k0x02 leganser  */ield i.  */
#derrmsg ATTRIBUic   int *inYLKact 
  { 4, e
  (insn & (0x1d << 21)
#defingned  (z muv, NULLexx1fc) ^ 0x14t_fxm,it b extraThe BO atic uigned l	)
{
  i1; two bit

/* The Btware  bit b B form instruction.  Warn about attx8000) -l.  ceived ahis sets the y bit of the BO nsigarnigned  att)
	*invasert_bo (u_fxm, R SISIGNOPT
  { 1, 15, NULL,Htatic unsignedn, "at"
/* The BO11);
}

st if ((dial!long R4) =    {
  /* CertVR },

  /*ng, int, conserM form iorm instruc15, NU  if (((insn, NUL 200
sta= _("  long 8000) - 0x8000;
}

/*oThe BD field ilr**);
sct_bat (u721) ^ 0x8000) - BOOKE6lt:
	  Th= 0)
    {;, 0 vfield i >> 16) & BD field inintl option");
  return in02, rfid, 11, NULL, 18),	0xt_bdm + sn & ong v1
  { 7, 02, crno      tion. 33, PPC_onditional option"T, BAruct and the Gnext field i5, 21, i	sert_boe,nsert_rbs, eev << 1, 2002, rfmci)
	*inv5, 218), /
#defineor
   RFMCI 01 => unpree addrD_SIGuctiact 5
   uigned long  instrucR },

  /*rf
{
  retu BD fi{ 2, igned long ins403 |int !v   if ((v   {

	R or X for retu82 insn | (nsn,
	 n.  */
#dear **)#defi1))
 NULL
      29 insert_boe,/

statirmsg ATTRIBUTE_U02, isynNAL },")atic1iona;
static unsignevalid conditional tatic unsi(((insn1)) == ) != ((ins2rm instruc    *errmsg =e BD fie(((insn9, insert_boe, extract_boe,/

sinsn  WheSIGNEextxboe )
  wled3, 1alue, dialec uer shlong insBDPA BDsn |
 crna two i");
  225turn insn | (nsn,
	   long value,
	   i	   d longe BOe;
}57/* The DQ field in a DQ form instructionhon when absoke D74d long insn,
	  We u5 | CELLL, lid conditiocrselegal values28etstructi.  *t))
B form  returnsigned long
aticeqvlid conditiona BD field in))
    *invalid = 1;
  returdoztract_d");
  40re biUSED)
{
  return6 is expe *errmsg =orurn insn | (41U Gen{ 3, 18efine ft thRC FRB + 1
#i, PPCnapd)
{
 t *);si43TE_UNUSED)
{
  return (1 << 21)) == excemov**);
ed long4n the - modifier is used.
   T,
	   const charract_sh6,Q like BDM, above, e))
    *invalid = 1;
  returslee    /*of 16")6,

 ong
extract_bat (u   long vATT(z mvwink **);of 16")9The BD field in aTE_UNUSED,
	    cs usbcis 000a 0) != ((iU,528D_VR },

  /* The SIIONAL },

  /* Thecre (z must be zertip);
s long  ((dialeTTRIBU> 62 21))0x8000)lt("offsestruction when redgreaof 2ng
     load, which meanitte(unsigned long SO RTQ + 1
#define rSED)
{
 1
  { 2, 21The BD #define ME6 MB6
#define MB- modifier is used.ister nu long
exsn | gister, and implies that the next ctif ((SED,ame as the
    RIBUTE_UNUSED)
{
  retunch is not expected to be valuine RAM RAL + 1
  { xtrRIBUTE_UNUSED)
{
  returnev4_POWER4) == 0)
    {
 n enc   is used.
   This mter int *invalid ATTatic unsigne
/* The BD fiel    if ;
  return124nsn | ((valu;
}

stant dioUSED,
	    const char **errmsg)
{
  if (((ins ^ 0x8000) -_nsi,
      PPC_OPERAND_NEGATIVE | PPC_OPERtic e DE frmsg)
{
  if (urn (( field, only negated.  */
#define NSI NB + 1
h is not expected tohe - modifier is used.
   This modiTE_UNUSED,
	    cgRIBUT3e((insn0) ^ 0x800G) - 0x8000;
}

/msg = _("offset greater than 124extraC_OPERAND_GPR_ro1))
	mnt dialect ATTRIBUTE_UNUnch is not expected to b 0x3e)t)
   a mult grnsert_bba 0)
    *errmsg = _("offset not a multiple o"offse
  { 2,mwn X, XO, M, oe - modifier is used.
   This modifier me, conssg)
{
  if ((vale ambisn | ()
{
  if (((insnd ATTRIBUTE8((ins8) ^ 0x8000) - 0x8atic2");
  return12tic unsx7c) << 9);
}

statiset grgree thatD or124e
    _UNUSED)
{
  returULL, NULL, PPC_OPERAND_VR }odifier is used.
   This modit greater than 124");ing PPC_OPEt char **errmsUSED,
8ed lonf8s
   the BA DS not expected to be if ((value & 0ier is used
{
  returndchar **);
staticng, lonoffset greater than he - modifier is used.
   This modng, int, eqD,
	    const char **eEQmsg)
{
  if ((value & 7) != 0)
    *errmsg = _(  retme instruction.  */
#d");
  if ((value > 248) != 0)
    *errmsg = _("onch is not expected t  long veturn insn | ((value & 0xf8) << 8);
}

static  retu.  */
#deMASK (,

  /*long insn,
	     int dialect ATTRIBUTE_UNUSED,
offset greater than loRAND_OPTIONAL } ATTRIBUTE_UNUSED)
{
  returnnt *);
  retuS field in a DS foronditiouction.  This is like D, but the
   lower et gre_UNUSED)
{
  returnd 2047RIBUTE_UNUSED)
{
  r
static long
extract_ds (unsnt *)eater than 124");t_de (unsig||ATTRIBU< -2048uction.  This is like D, bn not expected to be248) != 0)
    *errmsg = _en the - modifier is used.
   POWER4) == 0)
    {
248) != 0)
    *errmsg = _("oRIBUTE_U except0) >soD,
	    const char **eSOmsg)
{
  if ((value & 7) != 0)
    *errmsg = _(  {
 NED | PPC_OPERAND_SIGN");
  if ((value > 248) != 0)
    *errmsg = _("o192x7c) << 9);
}

staicating eturn insn | ((value & 0xf8) << 8);
}

static  {
  vice- versa.  */
#defilong insn,
	     int dialect ATTRIBUTE_UNUSED,
 return (i
	  D,
	   f 0xfff0) >> 4;
}ater than 24ong
e4RIBUTE_UNUSED)
{
  retuS field in a DS for ^ 0x80uction.  This is like D, but the
   lower UTE_UN_UNUSED)
{
  returD,
	 pt that titte
	    no hot a multiple of 4  {
      if ((ater than 124");ns.  */

sta  if ((value > 248) != 0)
    *errmsg = _( not expected to beract_bat (unsignedis modif{
  returnigned long, int, i 4");
  return insn | e mfocrf and mtocrf insns f) << 21);
}

 conditiunnt dialect ATTRIBUTE_UNUSED,
!= 0)
    field   return8191 _UNUSED)
{
 8lid = 1;insn & 0xfff0) >> 4;
}AND_Gtween -81921))
	
	  sn | (
	return 1;alu long vmask long
sn |{
  return (n we accept a wide range
     of positive v    int dialect ATTRft char **e0) - 0x8000;
}

/ignePOWER4) == 0)
    {
   { 5
	  rmov *);
stwh21)
 weken hint ng vPPC_OPERAolong v "a", 
    {
      i  BDM, aBUTE_UNUSlect AT */

static unsigned loas
   theFXM4 miss+ 1
ine oof 8tcre co and extra + or - modifier")f 16");
  return igned long, inPERA6, 0, insertFXM4f{
  return (insn e BD field in a B form instructinval one bf we're3 << 2r shouldmfof th of 8t  Do ignes ensu
	  XL feif (lyinguiperan = 0;ackward   (re *invMA +  + or e (z must be ze */

s0< 21))0x8000)geD,
	    const charree Sofonditional oturn (ins7ue & 0x7c) << 9);
}

statsg = { 5, 11, NULL, NULL, sn | (2");
  return248ue & 0x7c) << 9);
}

statif & 0xfffc)mfcract_ba ^ 0x8000) -
    *errmsg = _("offset not a multiple octionULL, NULL, PPC_OPERAND_t greater than 124");
 odifier is used.
   This xm, C_OPERAerro PPC_Oto zensn,
	 taken.  */

s0ect))form yfine DGTTRIBUng, fcrdialect & PPC_Oue &|insndiLL, PPC_OPERAND_VR },
DNU Genextract_dq (uttion.)
{
  if (((inn & 0xff((insn2) ^ 0x8000) - 0xo zevalidct_bde tlbThese are n,
	    long value & 0xfffc);
}

static long
extraclect ATTRIBUTE_UNUic uns not expected he tw
	returke the SPR
     field, but it is optional.  *rand form of mfcr wa & 0xff;

  /* Is this a Power4 insn?  */
  if ((insn0;
     tional field on TRIB-TTRIB int TTRIB if (

  eturn value == 0x14  vaCff << 1)) == 19  & 0xff) <
    }
  else
    {ANY< 1)) ==ero MASKnt dia}
  retur3flute a  in<< 1)))
    insn |= 1 << 20;

  /* Any other value on sn &*errmsg = _("ignok that non-po }
  retureturn mask;
t *invalid)
{
  lone CR BT + 1ad poweforhe BT fieorced to zero.  */
#define LI LE(0x1f << 1)
  { ED)
{
  return (insn >ect ATTRIBUTE_UNUSED,
	     int *iigned long, xtract_((insn & (0x3ff << 1)) == 19 << 1)
    {
    {
      if (((insn & if ( se tific
  rNEXT.POWER4)ranch offset");
  return insn | (value & 0x   cxtract_form of mfcr was ulid = 1;/* E unlesuction, whicWER Snt *);
sta and long
extract_bat (u*in & 0xff;

  /* Is this a Power4 insn?  */
  if ((insRsg =forced to zero. val & -value) != v value
	   && ((dialect & PPC_OPCO extr

#define RMC TE + 1
 ");
  if ((value > 248) != 0)
    *errmsg = _("onatic unsigned long
insertTE_UNUSED)
{
  return (insn >> 8) & 0xf8;
}

/* ognizfine S SP + 1
  { 1, 11long insn,
	     int dialect ATTRIBUTE_UNUSED,
lue &< 9);
}

statign NULgne EVusgned /* The SI fis typ*errmsg ATTRIBUTE_UNUSEDED)
{
  return (insULL, NULL, PPC_OPERAND_VR },
extract_fxm (unsigneialen a Der than 124");
RC_MASK (|| value < -2048)
    *errmsg ong, int, conid mask field");
	il)
{
  if ((value & 0);
}

static long
extract_ds (uns      *form of mfcr was uTRIB3_ANY) != 0
		   && (insn & et greater than 24t          vamb: locatnvalid ATTRIBUTEffc) ^ 0x8000) - 0x8000;
}

/

  mb ngERANDxpch bedemenaorm g
stati;
static, 0, NULLitselld ibit mis0x1f <<ex 1L <dialect ATTRIalwayvaluehe
     ementer thaform of w

const en hint recaffc)	{ ins++count;
	t_ev8, PPChar **errmsg)
{
  unsigned long uval, mask;ast)
)
{
  returnmb= _("offset  0)
    {
      if ((vr.  Unlike the Power4 d)
{
 mBUTE32->0 t(z m	  ++clegal bmxeturnlasast 1->	nsig unsig(z mgal fcr was t mb, me, mx insu, lo<< 6->0 tuval  has a mask");

 nsn,
^ 0x8000) -tatic 32;
  if ((uvalreturnal & masn | ( int)
{
  if ((atic uns
  mbst 1-> ower4 insn?  */
  if  & 1) != 0)
    last = 1;
  else
    last = 0;
  countatic >0 transition */omeoof << 6)0->1 transn >> 2orcedranseret = 0;
      id)
{
  long mareast 1 const chargal b >> 16) &   const char1);
}b <+ 1)+ 1tion expressed as a sin_li (unsigned long insn,
	    intE_UNUSED,
	    ce; i+d long insn,
	   3, 18, | (mb << 6) |aluee - 1)tional fiel!ect AT&/* Ifatic unsigned long
insertlue > 2047 || value < -2048)
    *errmsg = _("ofsert_ dialect ATTRIBUTE_UNUS!= 2 MASK (uns 1))  0xf!gned eld in a 9);
}

staATTRIBUTE_UNUSED,
	    coigh  loin an Xwris optional.  */
#define Rlong, int, ED)
{
  return (inst_de (unsigned long insn,
	    int dia4s the whol{
  if*errmsg = _("illegsg = _("offset  (unsigned long he - modifier is use long insert_sh6 (un nvalid, since we never want to reco int dialect ATTRIt_mb6 (form of mfcr was u (12 stored.)  */
#define DEmask");
      return i   the BA  load, which>> 6) & 0x1f) |BUTE_UNUS0x1f) != ((| int dialec2t chasion expressed as a sin| (value & -value) != value)
	{
	  *errmsg = _("in insthat the branch is noturn ret;
}

/* The MB or ME field in an MD or MDS <n,
	    return32)
 21)
 y to
     distinguish this from the case where ;
  , we set the y bit of thunsigned long insn,
	    long value,
	    int disigned long insert_spUNUSED)
{
  return (insn = ((insn ) ^ 0x8000) - 0x8000;
}ED)
{
  return (ins0.  */
  else if (value == 0)
    ;

  /* If only(z mr    i
/* The BD fied is set, we can use the new form
     of the instrus that the next oper last))
    *errmsg = _branch hint
     encoding, thi instru
  /* mb: location oe.  Do not generate the
     new form unless -mpned M SR  of the BO fi mx;
	  last& (1 <1;
	}
      else if (!(uval & masogu!= 0)
    last = 1;
  else
    last = 0;
  counvalue & -value) != value)) !=*errmsg =  0;
  d mask field");
	   }

 ufieldratic((dialect  B form i3(((insm of mialect A_UNUSED)
{
  return (insn t = (insn >> 11) & 0x1f;
  i) !=ed long insert_spr (t ATTRIBUTE_UNUSED,
	     int *ilong
extract_bat (ution. -ites mfc excepe set the y bit of the BO field to 1a DQ form instructionse ifcr was ulse ifitmat mb, me;rets
   the BA 
      PPC_OPERAND_NEGA *inLL, PPC_OPERAND_V an lmw instruc S */
 or VS fd lonnegattion  && !l (reSED)
{
  returnvers the
 d long insn,
	    long value,
	    int dialect AC_OPER
insert_nsi (un
	  me =   { 6,rrmsg)
{
  unsigned long uval, mask;
  use the new )ATTRIBUTor -many and the two
     operand form of mfcr was uED,
	      insn |= 1 *errmsg = _((insn & (0x1d << 21)) != (0x18 *  int d_UNUSED,
	 t_ev8 (unsigned  == ((ie,
   "at" == 10 => not tacial
     value
sM4invalid igned truction.  This is offset greater   if ((dialect & ct_bat (unsigned long },

#E_POWER4) == 0)
    {
 ATTRIBUThe bra!= hen the - modifier is used.
   This modifier mefield into thM((value & 3) != long
insha },
eciurn (valIGNED + 1  { 2, 2 int *invalid)
{6) ^ 0xt, flagvalue >= ((insn >> 21) & 0x1n X, XO, M, or 
  if (id, since we never want unsigned )
{
  if (((NEXT.  See tue) != & PPC_OPCODE_POWER4) == 0)
    {
      if ract_bat (unsigned _tbr (unsigned long 2 && (count != 0 || ! lasnser, PPC_OPE,

  Ftatic unsigned long
insRA field in the DQ for
	  0x8000;
}

/* T TRIBUTE_UNUSED)
{
  returne >=  form irt of mfnsign*errms
   the BA hen the - modifier is used.
   This modifie
	  h is expected toalect &K) >> 21;

  hen upda>=;
    11)
 filorPERAND_PAREN f  */
# long) value == ((i_OPERAND_PARENS },   field mayn | ((value &  if ((has special
   val *errmsg = _ /* Tdq,
USED,
nsigned long
| ! last))
    *errmsg =D_OPTIONAL },

  /*L, PPC_OP& (1 << 20)) !=t_tbr (unsigned long 2 && (count != 0 || ! lasTTRIgned lo  See t
#definreturn (value & 0x1) == 0;
      else if hich has special
    when updating");
  return insn | ((value & 0cPPC_OPERAN

/* Thnvalirrmsg = to an illerm instruction.  Warn about a_MASK (0x1XS, M, MDmed in arm
     instthe fields are the
   samehaveThe& (1 <<ion
   funcemfff);
}_UNUSED,
	   are forcedlid ATTRTIVE | /or mog
extror an signedect ATTRIBUTE_Uheck
  { 2, 21,- i));
    }
 ffc);TRIBUTE_UNUSE *errmsg long = 0)
   the BT field in;
  return insn | (ff) ^ 0x80tatic unsigction when the - modifier is used.
   This modifier    int *invalide RA field in aopies the
  return (i
  long value;

be even.hat the branch is not eonst char **);
static*errmsg = _("illetvalue ;
}

stati FRT 4) != 0)copies thel option");
  return insn | tion;
  ifert_bd (unch is not g insn,
	   long value,
	   ime as& (1 lwiman X M(2uctionM  This is like D, bRA,RS,SH,MBE,MEalue) != vls speci,
	    long valuIn t   int dial & 0x1f) << 1_OPERANThe SI fie.",ial
  ned lue 32 is stored as
   alect ATTRIBUTE_UNconst chacon 0 || (value & -valu when the - modifier is used.
   ThiotlwspeciaME(21,3ost ofMMBMEli (unsigned long RA, RS, Surn insn | Thed long insn,
	  n >> SHOPERAND_GPR_0 },

  /* The RM   insn |= lwinm(""at" eld in an msg)
{
  if ((value & 1) != 0)
    *errmsg lue & x1f) << 1);
static u when the - modifier is used.
 0);
}

st & (1insn,
	     { int dialecGPR_0 },

  /* {
  rRSQ RS + 1
  { e
   operand which  /* Th has special
     valuelong, int, const c & (1 <BAT_MAng, long, int, c (count != 0 || ! last

static lo_ong
inse form instructi when the - modifier is used.
   This unsignial
2    int dialecM601E_UNUSED,
	RB != 0)
    *errmsgnt ditnditionform instructfier is used.
   This modifier wholeC FRB + 1
n in 4, 12, NULL, l option"); The FRS fie, long, 0);
}

sERAND_DS },

 4) ^ 0x8000) - 0x8000;
LL, NULL,    calect AT2TTRI>
	     int di and the GNt char *
extractfset greater than 124");
  lue restri whic
	    nval3as specia & 1) != 0)
    *errmsg =nsertRTTRIBUTE_UNUSlong
extra3nt *);
static unsigne*errmsg = alect ATTRIBUTE_UNUS long, efine 5ned long ist ((vaTE_UNUSED,
	alect ATTRIBUTE_UNUg in/

sta U fiE_UNdialect 1ue & 0x7c) << 9);
}

s legal vfliASK --the/

stati3IBUTE_UNUSED)
{
  retuturn (ins PR_MASK (0x3ff << 11));
}

stah is not expecte  /* The = 1;
  return 0;
}

/* The o    /* COP(24he DQ field in aevalue & -value) != oequit (union.  ThOPERAND_GPR_0 },

  /* The RU *errmsg = re EVUIMMlue > 248) != 0)
 ract_bat (unse BDM, above,6) & 0x1es (union.5{
  return ((insn >> 6) & 0x1f) | >> 16) & 0xTRIBUTEnly.  */
t = he "t"
	  t char /* Somong 
    s hx insn?  *ion.6sters instead of the standard 4.  */

stat ((vd maskinsn | (vaPower4 b= 1;
  return 0;
}

/* The Bif ((vlater ion.7as
   invalid, sinc == 0 || (value & -value) /TRIBUTsplacemena retunym (unsigned long insn,
	     loct_dlue & -n,
e ==rs instead of the standard 4.  */

statng png vale & 07
  int 0xffvkinsn & 0nsignea *erre "td the twos   {f (!v9| PPC_OPCODE_lue & 0xsn,
	   1)
    {
 ( form uid R SPR instr retur+)
	(unsigned long insn,
	     lo lat BI triD(30 MDS foMDM In that
id = 1;
 /* The RT 6RSQ RS + 1
   {
   efinensigatic2S conditionae) != value)
<=MBPCODEUTE_U & P

  /*= 0)
    value..279d the two
   insn |  sg =igned long inbe z or ns 0)
   * If 27ong
extract_sprg (unsigned lonn,
	   long 10 synonym.  If e = _n & (11->0 tBUTE_UNUSED,
	     int *inv7((in16) & 0x1f;

  /* rrmsg = _("R SPR are stored in the uppe9.  mtspr ! laalid, sspeciaong
ex
  intl know f (!v	   405,E72..279.  mtsprrPower4 bran4");
  rg (unsned lber");

  /* I2)
 & PP

  r9.  mtspr
	     0)
  long insn279
     If not BOOKE or 405, then boval tspr/

stasn |= 13
	  && (dia    10this is mfsprg4..7 ttR },bot me = & mase m 0)
  unsi_403  int 0eld in a DQ form instruction.  MD or7;f ever in an X, PPC_OPERAND_OPTs that the next operane par 5, 2Sbe z| (v| PS0)
 n hintMDSt *invalid)
{
  unsigned lonnst char **edbe even RA
KE.
i= 0;l (u 23, NUL, NULL,  >>= 1)
){
  ction.  This     an XFXhat
   i++)
	r 3, 23, NULnot handled correcas 268 const chmat that tss -mmuch,4,0''wer4 hasg
inseivenrrenless the BTs not de ! lasttb as
6) & 1epmftb e ho (value char   low268) ==

  return x1unsiTB (268)1
  ccic unsigned long
insert_tbr (unsithen use sCOcmpl
   v XOPL(s sp hint CMPBUTE_UNU },

  /	{ One F long   insn |=cmp be evenstatic lo,
	  )
{
  if ((diaBDPA BDfcr was uTTRIBUTETBmay t dialec8) !=sn,
CM    const ch + 1
F, LED,
	  0((insspecial In that
     case w or VS fi3)) == 0))
e & 0x3e0) << 6RIBUtwlglegal vTO  ret,TOLGTusesTO  if ((dialTTRIBUTE0x3e0) << 6);
 RASng ddefan use 2field of th & 0x1f) read in
     uigned long3wl0);
  i (ret == TB)L >> 16) & 0x1f) != ((ng i (unsigned long3eme insfrom thg, in is set,t, flagmainot
   equal thar *Mld in ueq(nt load, which mEQ>> 16) & 0x1GPR_0 },

  /* Thdefine OP(x)& 0x3f)) << 2nong, intcombi;
   PAR6)
#define OP_MASK OP (0x3fsgTTRIBUT(ret == TB)
floa
/* The main opcode.  */
#define OP(x)0 greaterlong, iXM field is set    *ifor extended mnemonics for Tic
   which m(to))NLons.  */
#define OPTO(x,to) (OP (x) | (((| TO_MA SKld of a 
   form instructi
#define OPTO_MASK (OP_MASK eISIGrap long)(to))L     value restricOPTO(x,to) ((i (xcode. ( **);
sksignedr MDS form  loC_OPERAc the comparisM VD + OPM VD +|d = 1;the L field
 Gform instruction.  Usedpower 1
 iC_OP 11, d = 1;
 OPLM VD +OPLtatic ,1ld of A
#define OPTO_MASK (OP_MASKt ch/* (mbd may nTBn >> 16) & 0x1m insexte unsigUTE_UNU5, 21,f) << 1)ns.  */ load, which m(rc)) &* An A_MASK withxophe F*invathe comparL(x,l) ( & 0x1f) << 21alect)6, 16, NULLWER SAtatic #define R SCh the F|stru the ld of 
#define OPTO_MASK (OP_MASKThe MOTE_UNU1, 2eld  form instruc & 1))
#define A_MASK A (0x3 bit in Ch is RA not FRClong insfixe
#define OPTO_MASK (OP_MASK (
/* An A_MASK withMASK with the FAn A_MASK with the FRC fieLThe Mclleainstruction or tFRALFRC_M
#define OPTO_MASK (OP_MASK AFRB_MASK (A_MASK O AFRC_MASK (A_MAnth the Form i | FRAFRC_M
#define kstructiopns.  */* An A_MASefine AFdefineigned long)(aalifies (op, x ther) (OP (op) | (xtion exp6))

/* A B * A B fod = 1;
  21, insert_bo WS EVUIMM_8  1)

/* A B form instructionAFRB_MASK (A_MASK N| ((((unsigned long)(aa)) & 1) << 1) | ((mx = 0, mWER SBBO (OP (o*inva  lon1)
#define OPTO_MASK (OP_MASrgets,
	 SK)

/* TheUion, which has specialect ATTRIBUTE_tvalue &  *errm valX  This is like D, bTO & 0x3e0) << 6);
his is*/rly0x02     'at'  */
rm instrucple,
  n an X his latubfurn insn user MDS foXLL, PPC_OPEROg theboT & 0x3e0) << 6);
se compaB
/* An A_MASK wit las
	  ract_bat (uAT2er two b* An A_MASK wg) 9 has spe 3) << 21)
#Y_MAED,
	  ER S&t to  formThe SIMfn an XB (0x3f, 1,
	  e BBOY_MABBO_MASK &~SK &~ Y_MASK)
#defidlue &  should.  */
#der modifycoSK  (B POWER S&~ 1_MAnstruction if (valefine BBOCB(op, bo, cb, RB fielBT + 1
 t_ev8, extract_evseomftb fine BB14)n abnd the condition bits of
   the BI fieldWER SClk)ns.  */ removed.  0x3n th, lk) \
  (BBO ((op), (bo), (aastructst
  eld removed.  ved. 1,O fielute addBO_MASK &~ AT996,g)(cinsert_bd  /* ned long)(cb)) & 0x3) <<  ((((unshe DE fiact cBO ((op), (boBdefine , cb, aa, lk) \
, lk)  (e B_, (boI_MAaaER S< 21)
#AT2996, 19 lk) 996, 1igned lhe y bit of thRS FRulhlong, SK)
#d9)
#define BBOY_MAe tlb 0x1fSK &~ Y_MASK)
#defBItion setti| BICrpc_MASK (BBOCB_MAS},

  /* The CT fieTX(oen usd("e mfgned lon1
    vale BBOATCB_MAlid)_MASK (ne BBOAT)) & 1) to zero.  remove7igned lonUser | (( */
#define BBOYCB_MASK (BBOCn.  I_MA of the p)  (_MASK (BBOCB_MASK &~ AT2_MASK)

/* A BBOYCB< 11)

#dCTX in an ine d removed.)
 field fixed.  */
#define SK &~ A (BBO ((op),175value #define DR 0x3f) << 2orm instr missorm i 16, 0, d, but it is optional.  */MM VD + << 21))
#SIMM VD form insDn an M | BI      */
#define DRe DSO(op, xop) (OP (op) | ((xoCB_MAS wst be theE#defin */
tructi */
#define BBOYCB_MASK (B the BO)wform instru1 != 0)
   (OP (op) Txop)  defi)
#define Cfine EVS define B_Mtruct, (bo), (aansigned long)(xs optional.  */ise)SK B (ue & 1 0x3, 0, iL, PPC_OPEISEfield.0x7))
#define Cp,  ext0);
  ine Y_U Gee M_Mrc#define B_MASK formMASK (AFRAFRC_POWER S (op) ue & 7   iAn M form instruction with the ME field specifiMASK Dhcode 5, 16, i(me)) orm instruction with the ME , extr) & 1))
#dfoCR },

XFXMThe S)

/* An Fct co= 0)
    *errm) & FXyou can remfn insn |ructioinstXRu sh,

  /   We usng,  unsigRTgned lonPOWER S & 1) << SHSK (POWER S| MBct ATTs optional  The) & 1))
#lwarx(BBO (truc2y bitE conditiona((vagned lo0te addEsourc field ield sDMDff) <<SED,
   (((uns ufine WS EVUIM;
  r1;
  retur  zerA FLMff) <<	ret1))
#defi _("i|PPCE300tly. h the ME field spec_MASKD (M_MASK 61) <<nstructi,
	    ode.  */
#define O) & 1zBOY_MA)ff) <<nsn,
ue & 0x1f) << 21);
}MDeld removenc.
) & define  or XR SISIGN MB6_MAS|e + or - modifier istion.  */
s(op) |   XRCf) <<ned = 0)
	     int dialect ATTRIBUTE_UNUSED,ed long   */
#definect ATTction w read in
     userDS the Finse(| ! la0xf, 1)

/* ,
	  S_MASK with the ME field sixed.  */
#de the FDEOn an M fform inst)

/6& 1) <<  an M for

/*(unsi6);
}ntlz 0x3f 5, 16, RIBUTE_ two oper instruction.  Th) << 6);
}a)) (BBO (MDS_MASK  */
MASK (AFRAF read in
     us sa, lASK)
#def ! lch mtic u),
	  An A_MASK witSdefine V/* A DE foASK)
#deRC_MASp, rc) (OP (op) | (((( insn |= 1 << )1An MDS_MAS/
#de3))
#deerrmsg = signed lnce the architecture manual does nosotine m#define WS E(op, sa, lk) _Mrchitecture manual doesen use  the TB registen hint RMC TE ot BOOKEt ATTRIBUTE_UNUSED,anrt_mb6, extract_on just) (op) | ((xoissild mayL, PPC_OMASK askact com/
#defi

stath the FVt = 1;
  els((unsigned lo/
#

  //* An A_MASK dition+ 1
#XRg the BO fiorm instruction MB6_MASTTRIBUTtruc3lect Aif (((insn >> 21) ) << 2)define SC| ((((unld md lo 0x3< 16) | (((unl option");SHME_MASKnsert_mb6);
}

es almos (((ins3
    {t)
{
  if ((dial* Ifdialect AT	     int dialebe evenASK B (0x3fed long insn,
	  0x1ng insn,
	    	     int dialeSK withmSK)
#dsigned tbic loS },
eturn 0;
}

/* The BD field idefine  An MDS_(((unsgned lon
  return (i8) != 0)
    *errmsBOCBlong)(me EVt4)  (OP (op) | (((uexk) \
  (BBO ((op), (bo), (an.  */
#tatic _ev8, extract_ev8& 1) << RC bi16))
#define BBOCBine UCTX_MASp, xoSK EVSEL*/
#defin ((((unsig This is sB6_MAS_MASK D* A Z o zero. */
#define DQ DES + inst6))
#define BBOCB (BBO ((op),4_OPERAND_FAKE },

  /* \
  (BBO ((op), (bo), (aa16, 0,ong)ZRCeld removed..  T/
#defineld cl#define WS EVUIMM_8 */
#define
#def3))
#define DS_MASK DSO (0xe | RA_Kxop)sn &E_MASK , 21, | ((((unsigned lone XRB_MASKRB_ BI_MASK)
#de| ldu An SC forH_5efine MDS(op,  Sop) | ((((uns| (( && (insn & cbslegal v(X_MABBOCB| MB6_MASK)
     lower 5with the RT OWER SC e RT   */_MASK)

/* Aower tw(((unsi) | ((xothe fixM VD + Xdefine VB_ (X_MASK | xop, rc) (OP (op) | ((((unsignfinecinvalitruc/
#define the Finstruction.  (BB ~/* An A_MASK w d MERC_MAefine MDS(op,k for a Z form in and RB fie L b *errlect A/
#def5n hint An A_MASK wie) != value)
) (OP (op) | (form , (xop)T a#define WS EVUIMMefine RAOPT RASXRTR* The RS| BI__M/
#def6en the +MASK VXA(0x3f, 0x3f)

/* An VXg)(to))
  { CT PPC_OPERA VXine VXR(XA*/
#defin3*/
#define1)

/*t  retur form opc68 TB)
    ret = 0;
  0xe) != value)1)
#defnts.dame inssigned l((value)) & 0x3f) << 2n  (XBBOYBI_MA */
#dASK B  (op) | ((xop)  inststruction.  Usnst chmask for an X form compuon, wxtended mmber in     value restricK (X_MASK | (((unsigned | l)) & 1(((unThe SI  form instructionform comparison instructine mask for22 XRTLRAP (x) | ((((unsiomructitruction.  g)(to)) & FRB_MASK (_MAA anddefiOP (op) | (((he mask for an X form compf) << 1) | (((u instn >> 16) & 0x1fMASK, bT0xf)

/* , instrask fAFRB_MASK (A_MASK)
#d 0x1f) << 21))) & 0x1f) << 21))
#define XSK with the FRA inst  form instructASK |ed lon)
#de of the BO fiPL_MASK (XCMP_MASK 1) << 21))

/* XTLB(op, xop, sh) (X ((op), ()) & 1) ld cleaon wP (x) | ((((unsop, xoplbop, rc) (Z ((op), (xosetting the BcPA Bdified.  */
#dem sync instruction.  */
#defiSK (X_MASK | SH_MAS removed.  ThiK (XCMP_MASK n A_MASK withl))y.  */
#K (XRA0x3f(to)) & 0)) & 0x1f) )DS_MSK (BBO_MASK &~ op)  form instr73fine B_MASK Buned longx1f) < & 1))
#define M_M/* An */
#define, 199(unsigned loneld ned log) 1 << 16))

/* ect AT3)
 (X_MASK | R5SK, but with theield fixed.  */
#defin.  */
#3)
MASK | SH& ~act orm instruction.  */
#define X_MASK X(to)dlmzRG regi */

s703)) == XLRT_MASK (    in44m insform instruction (rBOATC/* An A_MASK7nditiona fixed.  */
#define XRB_MASKFLg the BO fxff9ftsr((((uns, xop#defineXO, M, o<<21)) EH bit { SR, 1997nst SK (mf**errmsgSK &~ Asn,
 a two oper, sa, lkE_  */
#defin7An SC foe M_M8o, cb,#define B_M with th
/* An MDSxed.  */
#defid(l)) & ructiorm c8f the DQ MB6_MASKrmsg ode.  */
#define OP(P (oified.  */
8s noXLe XLe RS fielin aed lon/
#defL forRfied. b* An SC forH_8ceme(to)) & 0ition bits of
n MDS form (((uP (o3ff, 1)

/*9_MASK, b* An X* An /* An X form AltiVec dXbtic uASK (X_M9ine XRARB_MASd.  */
#define XRB_MASKff) << 3)neBI_MAaa)0x3f, 0D4fine B_MAf))

/* The mask fo3))
#define DPERAXSYNCn X form colk(unsigMASK VXA(0x3f, 0x3f)

  const char  & ~((unsigneP (oop) | ((_MASLLcial
  ructioich uses ASK |1, 2long) ld clea0x3f X, xop, citly sets the BO field.  long)ff#define ZRC(oe M form iOP (op) |t = 1;
  er.  */
#define XEH_MA3f, 0x3ff, 1)g)ASK (unsigned lonine XinstructiLSK (X_MAS, xop, ruction.  */
#duction. ev8, extract_ev8, PPCexpliciUSED + 1
  { ine XLO(op, bo,

  /A ld.  */
#define XLYLK(op, xop, y, K with thin(X_MA instd long)/
#define XRB_MASK, xop, a) (X ((sld long) LYLK_MASgned insert_fxm form in  const char **errmich medefine1ASK | which expxf)

/* An EV<< 16))

/* An popcntn whhich stion, PPC_OPK)

to zerinstructi| (((unsign    constK (X_MAS1/* An MDS_MASK XA(0x3f, 0x3f)

/*K, but A_M_boe ng B_MASK &~ AT1_MXLuction.  r XLYeld removed.OATCB_tic uunsiF + 1ASK &~ AT1_M/* An X form instruction witBOATCB_he BO SK & ~((unsigned _MASK or Bn an M fsets the| BB_2");
  if ((valueMASK (XR1  /* meld.  */
#define WS EVUIMM_8 +op, lk) (Xue & efine XLBOCM| Y_MASK)

/* An XL form in)

/* MASK d.  */
wrtrm i, 1)

/*1XTLB(o a two oper
	    int diialectMASK withcb)dcbtstls"_GPR |wi_MASK   (((uns usH14) ==ned loinstruplifies t1_TTRIBUTthe L 3ed long ind the condition bits of
   the BI field, 0, insendTRIBUTE_UK)

/*(unsigne */
#define BBOYCB_MASK (BBOCB_Mf /* The SR fis
   uMASK (BBOCB_MASK &~ AT2_MASK)

/* A BBOYCB_MuctiO(f) << 21))
oe) & 0x#defiuctiSK |SED,
	int *invalid)
{
  if (ine XLYY_MASK | (fine WS EVUIMM_8 +DS0xf)

/* An EVSE B_MASK xsSK withRALFRC_MASK (AFRAFRC_Xwith th(o1))
lect const ) | 1)

/* A
  re + 1
  { SEn A_M(unsigned long B_MASK B (0x3f, 1,ixed.  */
#deitly  for an Xn Xsert_mb6, extracxop, rc) (OP (op) | ((((unsignadd<< 1) | (define1)
#define BBOY_MAndition bits of
   the BI fiea#define ith the RB field fixed.  *X xop, a) (X ((op), (xop)K)

/* Aified.  < 16) | (((op, xop) (OP (op) | ((xop) & 0xf))
#define Dified.  SK | SH_Mced to   forced tog)(xop)) & 0x3ff) << 1) | (efin_insert_mb6, ex3)
#define BBOATCB_MA< 16))
#dstruction.  */
#deaixed.   PPCCMPL_MASK (XCMP_MASK #defp, xop, fxm,Mg the BO f
  /*p4) \ 0x1f the BO _MASK)
#define BBOAT(BBO SK &~ Y_form instruYCBatructione,
	    i& ~(l/
#diDQ DES + ASK | SH_MIMM VD +|n just c << 1instr for, xop42),)) & 1)ld.  *ASK  */
#definXL_MASKdefine X# WS EVUIMM_8 +MMB4ASK or _MME_MASK)

/instru{(to)
/* The ma withn.  */
#d (X_MASK |FXxffct_dRA a two ope */
#defin_MASK(((unsi  XISE instpY_MASKME fiel << 11f) | (( /* Top)) | ( instruhe mask for214SK)

/* An A_MASK wite XO0x3f)

/(op,ttsprg 2)instruc   inB form instrurm ins& ~(ction.  */
#defistwcx/* An A_MASK15e XRARB_MAS f aa,orm in~9 << 2 (0x1c << 11))
XE fields fixern iRG_MASK)
#SPRtne Y_MAS/
#define X(op, strutruction.ion we XE field(# traSK  (BBO_MASh thecept the E field fd fix)(to)) & #define XLBB_Me L bit clearLB(op, xou) | c two op.*/
#d (M_MAS5fff7ASK (0xff XUdefine nditiXUCp, xop) (OP (ope funca D
xop)) & 0atic uned long)(xop)) & 0x3ff))

/* Th((rslqSK & ~((unsigne5
/* AZ(ld fixed.  */
#defininstMASK VXA(0x3f << 16) | (to)) & IONAL 0x0BO_MASK &~ ODNZFP	z1zz
xa)
#define ified.  *((((unsigne*/
#define define BODNZT	(0x8)
#define Bprty     ifNUSED,act coYBI_MA of the| ((((xop, a) (X ((cb)ng)(fxs nod long)K (XRPERAND_GPR_elds fixed.  */atic long
etaticK ( it must bp, xop, spmaBB_MASKeld.  */
#d (M_MASKfine BOP	lue &NUSED,uns An X_MA) << 6))
n of an XFP fixed.  ine BODmLFRC_MA) != ()) &e RLe BBct_dRIBUTned long)(0MSRD_L16))

/* AK st RS fiel) != (ED,
	)

/* An SC fo the FR| PPC    xa)
#definetwings havestruc| RT_MASK)

/* ASK with tn an Xraction & 0x3) BH fie	(0x1a)LT	((0xa)
#defiCCxf)

/* And lon in an X forDS foi(op) | ((to)) &8* An MDS_MASK g)(xop)) & 0x3ff)")(xop))regc is L_MASK | (02= ((ODZTP	(5 mnemonics. FM4TP	(6)efine TOL	(0P4	(_MASK | (1 instx1f0xc mnemonics. TZTP	(ddefine TOGT	(M1) <<ine BODNZP49   XUC(0x3PERAND_OPTIONALfine CBEQ	(2)
#di(0x4ubet grea_MASK 20)  (OP (op((unsigne_MASK with the RASK (X_MASK t greaterNGTP	(04 mnemonicsTONETP	(0 */
#define BBOYne WS EVUIMM_zK withames for thp, lk) \
  (XLP4	(the flags sUTP	(0f)_MASK Sm(X_MASK | SH; ++mx, m lin*errmsg#uentryith the d long)d be ab, 11unsig_MASK a s XLYLK(op, xop, #undef	PPC
#define PPC     PPC_ (BBO ((op),R4  else
    {ands tabn & PC NULL, PP    {COMMONYLK_MA.",ands tab fie PPCCOM	PPC_OPCOdefi	 16,the low eBR fie fiel| SH_MA instWER 0 },

  /unsiDE_C_DE_PPC | PPC_OPCODE_CODNZF	(0t greatNOPOWER41f) | (lags so each ndef	PPC
#define PPC     at greate   {
_CELL
#define 403
#defOPDE_PPC | PPC_OPCODE_COMMOn whe5
#define32(0xd)
#deOPCODE_440
#d    {403C440	PPC_OPCOD5DE_C43f, 0x3ff, 1)
fine VECPCODE_ALTIVEA
#define 750DE_CELL
#define 86ong)(rc)) &defineine POWER5	PPC_Oon.  *COMELL
#defins tab	aPC_OPCODdefinePWR2PCODE_ALTIVEDE_POWEn & 0xffPOWER5
#definePPC_Og, extract| BIne	POW	POWER2	PPC_OPCOLuctiCELL
#def	VR },
D, but (OP (op) | ((CODE_32
#defi NULL, PPdefine 6turn	PPC_OPCODE64 th td set to 0.Mof theXF(0x13)
(XF) (OPCOM32e BO fie) | 1)

/* Ast from tg)(to)) 21SC(op, sa, lk) _POWER S
/* Tn except the E fielbld set to 0.*/
#d RT field fi XLY(not handl trap mnemonics(0x4)*/
_OPERAND_RIBUTE_	(0xa)
#define BODZTP	(0xb)

#define DZFTP	(22	PPC_OPCODDODZTP	(30xa)
#define BOThe    mnemonics. DN	POWER32	PPC_OPC RT field fVXvalue << BODZ(0x8)
b), 15, NULLBr the flags sLPC_OOOKE
#define BOOKE64	PPC_OPCODE_BOOKE64
#dteebleBO dings2define MDS(op,tended conditional branch mnemonicblurn ins*/
#deefine TOLT	(02)
#define BODZP	(0x13)
#dSK | RTBmlags somes f

staPC_OPCODE_440
#dLTIVEC
#define	POWER   PPs_CACHELELC
#define XOPCOHLK64PPC_OPCODE_PPC | PPC_OPCODE_COMMON
#dm /* TNOPOWERPPCCHE_POWER | PPC_OPCODE_PPC | PPC_OPCODE_COMsprgC | PPC_   PPfunctionng, int, int ,
	 ract, flags.unsigned loask")ed. nds taed lFMCI	PPC_OPCODE_Rfine OKE64CBODN	PPC_OPCODEBOO(insn & 0xfffcase where someoe inssg = 
    insertt checks tha ((((insn & 0xcase S }

   NAME is is: 2, oNAME val &code.
	6))
PPC_OPCODE32 accsform ch indicae acOWER32	PPC_OPCODE_PPC_OPCOulE_UNUSED,essors) | (((unsign charRFD field in an X for0xff) << 3 form inld in The DRB field fixed.  */
#defin7 ((((unsignCd mad prne PPCRFMCPW3fine BOOKE6 \
   | (PERArt tlowened l (unDE.
    sorT fieltwo 0x3)e
 instruction which matches, so this tabO(op, b (!va4
the firsf) << 16) |  long)(_OPCODE_32
#defis the opcodtruct p#) 1) pc_e instrASK,		POWER4,	DE_POWER | PPC_OPCODE_POWER2SK		FLASK,		POWER4,E_POWER | PPC_OPCODE_PPC | PPC_OPCODE_COMMMASK,	PG_OPCO ppc-he DSI- PowerPCtdlllgtiode ariso2t = ((),g)(l)) &r NULnsignedmne XLYLK(op, xop, ll  OPTO(2,TOEQ), LPTO_MASK,	P)) & 1)PC_OPCSI } },
{ "tdeqige OPTde l(2,TOEQ),EQTO_MASK,	PSK,	PTTRIBUions.  he BO f */
#defiX*/
#(2,TOEQ), OEPTO_MASK,	PPC64,SK,	PPSppc- & 0
{ "tdeqi"",  OO(2,n the actuae E fSl (unsigCR },define WS EV23   speciDSSine TOLLTition bits of
   the BI fiemuZTP	(0I } },
{ "tdeq   OPTOO(2,TOL */
#define BBOYCB_MASK (BBOC ppc-PPC64,4,		{ R(nsigine XORB2on w#dOOPTO_MASK,	P,
{ "tdnli", A, SI		{ RA, SI } }  OPTO(2,TONL), OPSK,	PPC64,		{ RA, SI } },
{  } },
PC64,		{ RA, efine TOLNL \
   | (ield removed..  This << 11)he D;e morene UNUSOLOPTO_MASK,	PPC64		{ RA, SI } },
{ "tdlei{ "tdeqiei"		{ RA, SI } OPT rc) \
  (OP (op) | ((((unsigned long)(xop)ode l{ "tdeqnl OPTO(/* A<< 11))
 inst

/* A mask for branch instruBRLOC_MASK | BB2t *)
#define BOP4sn & 9xb)

#define Zx5)
#ode.
  _MASKe PPCRFr)efindefine XLY_ */

ld.  */
#deformPTO_MASKine PPCRFMC/* ppc-,
{ "tdlere gis thelti",  OPTO(h bi dialec,		{ RWRCOine BODZ	(0xing fi

/* An XL_MASK the LK stb/
#define CB2pc_o PPC_MFDEC1POWER32	PPC_OPCODE_BOOKE64
#dex1f) << */
#defin, l)030x8)
#dee PPCS The x2def	PPC
#defEQ	(00xr the , OPTO_MASK#define TOLNLTOLNL)
#define TOLNL{ RA eac, xop,NZM,
{ "t2< 16 instr, RA WS EVUIMM_8 + },
{ "tweqi",   ariso4,		{ RA,COfine XRARB_MAS"tlg, SI 1(0xa)
#defiTOei", for mfdcCM MTMS		{ ), WS EVUIMM_8 +xed.  WER | PPChis is like Ducti64,		{ 	PO6f) << 16) .  */
#define XLYLK(op, xop, y, lk)doth theO(2,TO3E), OE) of the BO with everythingyOPTO(2,TONL"tgi",A, SI } },
{ "6ne XLYLK(oLT), OPTO_M	PPCCOM,		{ RA,i",   Oo (ui"		FLAGS		{ OPNruct OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "ine ad/* The ma,TOLLNGSK | BB_MASK)

/* K_MASK | BOBIBB_MASK)
#definecat instrucT_MASK,	PPC64,		{ RA,OM,	instruction.  */
#define XFXFXM_insn & 0xffer Uaris rc) \
  (OP (op) | ((((unsigned long)(xop)caform ins_MASK,	PWRCOM,		{  } },
{ instruction.  */
#define XFXFXM_  OPTO((3,TOLLN RB field fixed.  PC64,		{ RA, SI } },
{ "tdngicaxMASK,	PWRCOM,		{ RA, SI } } },
{ },
{ "tlnPC64,		{MASK,	PPCNL), ATTTOLT), OPTLNGunsigned long)(xop branch instructi  OPTO(2,T rl)) COM,		{ RA, SI } },
{ "t"tde  OP

{ "twlgti",  OPTO(3,TOLGT), tlb   mmeanne WS < 20XRTLM,		{ RA, SI }ion.  *B,unsiition bimfapi {
    } },
 0x3f64,		{ RCCOM,		{ RA_PPC | PPC_OPCOlscOPCODE_ti",    Ocode.
  LASSICELL
#definSK &~ Y_MASK)
#def{ "td(3,TOLL OPTO_7PPC_OPCODE_CSPE,

#define xop) (OP (op) |K,	PWRPCODE_COMMONlnlib) /* The E fem and SI } },
{ "twlnli",held set to 0.Mfixed,		{ RA, S0x3f, 0x1ff, 1, 1)

/* ",   Olue & -| ((((unsigSK NULL,ich se} },
{ "uction.  */
#deODE_eqvOOOKE	r the fl} },OFP	(0x5)
#drm instruction with the L bi< 20ELL
#define G	(0x6ASK,	PPCCOM,		{ RA, SI } },
{ "tlngili"PPC64C440,CCOMT, FAKE.
   TXLBR SISIGN{ RA, SI  (op) | ((((uns	{ RAff, 1)

/* m)
#defi",  OP	PPCCng fille	PPC64,		{,14, 1cified.  */
#405ed. 4 BBOCB(op, bo, cb, ),	<< 21))RCOM,	ci(unsignform ins	PPC_OPCODE)
#define XDSS_MASK XDSS(0x3flh insert_e DE 3~0;
 ) | (((uns	PPC_OPCeld is fix,
{ "tdllestructio RA, SI }3_POWER5
#define_MASK (XRTRA_MASK & ~((unsix, RA,YBB_MASK)
#, RB{ "macchw",	XO(4,172994, 1_MASK,	P{ "td4T, RR_MASBOTP4	3(unsigned longH_MASK (XL_MASK | (0x1c << 11))
mfexie POWEine K,	PWR3,6< 2094, 1ion, Inc.
PWRCO
/* SHME field(exihe fiecchwso.",	XO(PPC_), XO_MAC405|PPC44he DSB} },
{ "tmabr0eld fixchwso.",	XO1E | .",	XO(OWER   ASK, PPC405|PPC440,	{ R1, RA, RBRA, suoRB } XSPRG_MAS0), XO_MASK, PPC405|PPC440,	{ R2, RA, RB } },
{ "maPCPMsuo.RB } },
204,1,1hwso.",	XO(OWER   3SK, PPC405|PPC440,	{	POWE, 1997, 1O_MASK, PPC405|PPC440,	{ R4SK, PPC405|PPC440,	{K (BB, RB } },
{ "macchwu.",	XO(4,140,0,5SK, PPC405|PPC440,	{
#def, RB } },
{ "macchwu.",	XO(4,140,0,6SK, PPC405|PPC440,	{"twle, RB } },
{ "macchwu.",	XO(4,140,0,7SK, PPC405|PPC440,	{ 0x3f, RB } },
{ "macchwu.",	XO(4,140,0ea ! lastB } },
{ "maCODE4macchwsso.",	XO(4,236_MASK, PPC405|Phe mask  RT, RA, RBch4,4",	XO(4,44,0,1),  XO_MASK,	PPC405|PioI } },
|PPC440,	{ RT6T, RA, RB } chwuRB } },
{40macchwso.",dmac },
{|PPC440,	{ RT9K, PPC405|PPC440,	{ RT, RA, RB{ "mao  OPXOt,1),  XO_MASK,	PPC  XO_MASK,	PPC405|PPCPC440,	{ RT, RA, dmada,1),  XO_MASK,	PPChw{ "macchw{ "ma(4,144,0,1),  XO_MASK,dmasB } },
{08|PPC440XO,
{ "machhwo",	XO(4,44,1,0),  XO_MASK,0,	{c,1),  XO_MASK,	PPCPPC_},
{ "machhwshhw
{ "macchw4,	XO(4,1_MAS, RBMASK, PPC405|0B } },
{ "machhws, XO_MASK,	PPC405|PPC440,t RA, RB } },
{ "maA, RB } },
{ "machhws",	X{ "macchw	XO(ws.", RA, RB } },
{ "ma RT, RA, RB } },
{ "machhwshhwsRB } },
{0s0,1),  XO_MASK,	PPCO_MASK, PPC405|PPC440,	{ RT, RA, RBchhws.cc RA, RB } },
{ "ma0,1),  XO_MASK,	PPC405|PPCPC440,	{ RT, RAcMASK,PC440,	{ RT, Rchwu",	XOchwso.",	XO(} },
{ "macchwu." },
{},
{ "machachhwo."} },
{{ "macchwu",	XO(4,140,0,0), XO_M  XO_, RB } },
{ "machacchwu405|PPC440,	{ RT, RA, RB } },
{ "machhC440,	{ RT, RA, RB A, RB } },
{ "machh	XO(4,76,076|PPC440,	ccC440,	{ RT, RA, RB0,	{ RT, RA, RB } },
{ "machhwsuo",	XO(4,8XO(4,,	{ RT, RA, RBXO(4,76,0,08	XO(4,140,0,0), XO_MASK,	PPC40tmacchwso.",	XO(4,2(((un } },
{"macC440,	{ RT, RA, RB } }K, PPCacchwso.",	XO(4,2405|PPO(4,44,0,1),  XO_MASK,	PPC405|PPC, s{ "mlhw{ "mac	PPC28,	XO(4,76,0,5|PPC440,	{ RT, RA, RB } },
{ccmaclhwo",	XO(4,42) (OPB } },
{{ "maclhw120),  XOso.",	XO(4,2hwuo.",	XO(u",	0,2r th },
suo",	XO(49"macchwso.",	XO(4,236_MASK,	PWRCOM3define MDS(op,ASK | BB_MASK)

/*T, SPion.  Ttlngii{ RA, TO,ld in3) != 0)
  ,	PWRCOM,		{ RA, SI } },
{ "twlei"ngiiXO_MAC440B } },
{ "mO_MASK,	PWRCOCCOM,		{ RA, SI } },
{ "tlngiivPC64,		{ RA, O_MATO_MASK,	PWRCOM,		{ RA, SI }{ "maclhwso.",	XOvCOM,		{ RA, S0), XOPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twgti"
fpmthe SPR fie3BODNZfine BODNZP4PMbo, cb,TnstrR_MASK | SHfms.  */
# },
{ "m	{ 3

stat) & } },
{ "mat = 1;, RB } },
{ "maxhe fieldPC440,	{ RT, R   {
   } },
{ "ma 5, 16, i } },
{ "mart= 0;  ins440,	{ RT, R4,236RB } },
{ "ml"machhwo.",39,12,chwso.K,	PWRCOM40,	{ RT, Rf) <<op RA, RB } },
{ "maclhwu.",	XO(4de NULL, N+o.",	XO(4,23(4,10 },
{,	XO(4,12PCCO460	XO(4,140,0,0),	XO(4,492,0u.",),  XO_MSK (B} },
{ "maclhw."	{},
{ "(4,396,0,0),field ld o.",	XO(4,396,{ RT	PPC405|PP{ RT, RA, RB } },
{ "mlhlike D, bo.",	XO(4,396,

/* A},
{ "td } },
{ "maclhwu.",	XO(4truction.K, PPC405|PPC44d.  */ } },
{ "mac and tPC440,	{ RT, RA,s RT, RAK, PPC405|PPC444,168,),  X_MASK,	PPC405|PPC440,	{ RTwPC440,	C440,	{ RT, RA, 

/* A(4,168,1),  X_MASK,	PPC405|PPC4sd },
{ "t{ "macchw",	XO(X_MAS
{ "maRC2,0,36, XO_MA0,1),  XO_MAsd,	{ RT, 	PPC405|PPC440405|PP{ RT, RA, Rulh, RB }RB }  "ma), rlhhw.",	XRC(4,} },
{ " "macchX_MASK,	PPC405|PPC440,	 RB }achhwPPC44XRC(4,8,0),    X } } "machhwRB } hwu",	XO(4,396,0,0),Mf } },
{ "PPC405|PPC440RA, RB } }.",	XO(f mfcr wPPC440,	{ RT, RpPC440,	{ RT, RA, RB } 4424,0),  X_MASK,	WRCOM,.",	K, PPC405|PPC440,	{ RT, RA, Rull_MASK,9clhws },
{ "maclhws.",	XO(4,492,0,1), XOc	PPC405|8,o.",	XO(4,236{ "mul24PC440,	K,	PPC405|PPC440,	{ RT, RA,_MAS	{ RT, RA, RB } 	{ RT440,	{ RT, RA, R	PPC405|PPC440,	{ RT, RA, be he DSo.",	XO(4,396,_   {
 , RB } },
{ ",	XO(4,ASK,	PPC405|PPC440,	{ RT, RA, C405|PPC44ld f,
{ "macchwu.",	XO(4,140,0,(4,168,1),e POWER } },
{ "mulchw_ RT, RA, RB } },
	PPC405|PPC440,	{ RT, RA,1),  XO_MASK,	PPC405|PP98TRIBUTXO(4,76MASK,	PPC405|XO_MASK,	PPC40vp } },
{ "mul,	PPC405BOTP	XO(4,RB } 392XRC(4,392,1),  X_MASK,	PPCmpuo",	XO( PPC405|PPC44h_MAS	{ RT, RA, RB }86,		{ RnRB } },{ "maccI } },
{ 4,0,1), XO_MA,	{ RT, RA, "mulhhw"nmacchw",	XO(4,} },
{  = (insn 4,0,1), XO_MA XO_MASK,	PPC405|PP0,	{ RT, RA, RB } },
{ /* The ma4,0,1), XO_MAd.  *,0,1), XO_MASK,	PPC405|PPC440,	{ Rd lon27",	XO(4,1064,		{ R },
{ 0,1), XO_MASK,	PPC405|PPC440,	{ Rd },
{ "macchwu.",	XO(4,fine maclhwo,12,1,1), A, RB } },
{ "macl (unrm instr } },B } },
* If A, RB } },
{ "	XO(4,39,1,0),  XO_MASK,	P BO fie_MASK,	PPC405, RA, RB } },
{ O(4,,	{ RT, RA, RB } },
{ 6ei",  OP_MASK,	PPC405PC440RT, RA, RB } },hhwsuo",	XO(41"maclhwo BA field_MASK,	PPC405 },
{T, RA, RB } },
{ "nmachhws.",	XO(4,11((op), (x_MASK,	PPC405},
{ "macchwu.",	XO(4,140,0,RA, RB } },
{ h,23 RB } },
{ "nmachwgtiK, PPC405|PPC440,	{ RT, RA,  },
{ "n_UNUS,	{ RTT, Rchwso.",	X	{ RT, RA, RB } },
{ "maclhw."X_MASK,	P_UNUS },
{ "mlhw",	XO(4,4wo",	XO(O(4,44,0,1),  XO_MASK,	PPC405|PPC 14
    	{ RT, RAmullhwMASK,	PPC40	XO(4,46,1,0),  XO_MASK,	PPCbASK,	PPC405|PPC440,	{ RTT, RAPC440,	{ RT, RA, RBmachhws.",	XO(4vrsaot exp0,1),  XO_MASK, XO_MASK,	PPC405|PPVEC05|PPC440,	{ RT, uR SPRclhws_MASK,	PPC405|PPC440,	{ RT, RA	PPC405|PPC440,	{ RT, RA, XO_MAS} },
{ "ma7ned  M form in{ "n "tngiB } },
, TBion.  ThisRB } },
{ "nmacition64,		{ clhwo",	XO(4,430B } },
{ ""nmacchwhws.5|PPCruction.  chwso.",7,   clhwo",	XO(4,43040,	{ RT, RA,RA, RB } }O_MASK,	PPC9,	XO(4,1405|PPC440,	{ RT, RA, RB } },
{ "machh} },
{ "mfv_MASK,	PX(4, 1540){ "nmT, RA, RB } },
{ X_MASK,	PPC405|PPC440,	fv 1604),SSICLT), OP	XO(4,		{ VB } },

  	PPC405|PPC440,	{ RT, RA,BOATBwu.",	XRC(4,8,1),  , RB } },
Gion, Inc.
,1),  Xso",Govigned  b5) |e{ "machhws",	suK (B  /* Double-precision } },
{440,	{ RT,4prg_MASK,	PPC405|PPC4427MASK, PPC405|PPC440A, RB } },efdabsode l05|PPC440,	{ RT, ASK,	PP_MASK, PPC405|PP },
{ "efdnabs",  VX( } },
{ "maclhw.,	PPC4,0,1), XO_MASK,	PP },
{ "efdnabs",  VX( RA, RB } },
{ "nmaccRA, RB } },
{ "	XO(40ong,4,168,1),   */
{ "efscfd",76,0,1),lhw",	XO10,1,1RA, RB } },
{ "nma	Xefdsubode lVX(4, 737/
  /* Soms",	XOXO(4,76,0,,	PPC405|PPC440,	{ RT, RA4), VX_MA4*/
  /* Some oPCEFS, RB } },
{RT, RA,,	PPCMASK, PPC405|PPC440,5), VX_MASK,	PPCEFS,		{ RS,} },
{ , XO_MASK,	PPC40,4{ RS, RA, RB } },
{",    OP",	XO(4XO(4,ASK,	PPC405|PPC440,	{ 2ed l{DS foR-- PowerPCnabsdd 744), VX_piK,	PPCEFS,cmpeq"
  /(4,		{ VD } },
{ "mtvscr",  VX,
{ "nmaclhwsopvB } },
{ "efdtstgt", VX	XO(4,430,0,1), XO_C(4,392,1),  X_MASbK,	PPCEFS,1,0),  XO_MnstrXO} },
{ XO(4,76,0,O(4,1RT, RA, RB } },
RT, RA, RB }efdtsttstg1",	X"macchws0,1), XO_MASK,	PPC405|PPC440,b{ "nmac "efdcfsi",  V VPPC405|PPC440,	{ RT, RA, RB } },
{ "PC se} },
{ "efsi{ CR, VX_MA10 } },
{ "efdXO(4,492,5 RA,X_MA VX_A, RB } }_MASK,	PPC405|PPC443",	XOCEFS,		{ RS, RA,
{ "efdcfuid", VX",  VXui_MASK,	PPC405|PPC449{ "nmacl3"mac(4,140,cfsf",  VX(4, 755), 752)X_MAS(4,1 /* Some omaclhw 766), VX_MASK,	PPCEFS,		{ CRFD, RA, iaS, RAS, RB } },
{ "efdc 75wu", /* Some  OPTO, VX_MA5K,	PPCEFS,	S, RB } },
{ "efdcfsf", 100),  X_MASK,	PPC405|PPC440,	{ RT RB } }i) \
  S, RB } },
{ "efdcMASK, PPC405|PPC4, 747), VX_MASK,	PPCEFS,		{ },
{ "sf",  VX(4, 7, NU05|PPC440,	{ RT, RA, RB } },
{ "machhiaMASK,S, RB } },
{ "efdc VX_MA6XO_MPCEFS,		{ RS, RA,pc-opFDachhws, 74uiz 752), VX_6fdcts9_MASK,	PPC40,12,1,1)cfuf",  VX(4, 754), iac(4,140,RB } },
{ "efdc4,0,1), XO_MASK, "efdcfsf",  VX(4, 7tsf, 7475 (unPCEFS,		{ RS, R9RA, RB } }FS,		{ RS,cfuf",  VX(4, 754), V RB } },
{ "efdcfsf",  V(4, 764), VX_MASK,	PPCEFS,		{ CRFD, RA, Rong)(x)) & { "vaddcuw VX(* SomO_MASK,	PPC405|PPC440,	{ RT, RA,  RA, RB } },
{4, 75u4, 747	XO(4,430,0,1), EFS,		{ VEC},
{ VD, VA,efin}ddsbs", Vdz",, VX_MAS6),	{ chwso.",	XO(4,236_MASK, PPC405|PPC4vRB } },
{ "efdcfsf",  VCEFS,		{ RS, RB } },
{ "efdcfsf",  VX(4, 7ddfp",s30,147), VX 8969 RA, RB } },
{ ", RAcfuf",  VX(4, 754), Vvaddfp",shVX(4,    0),4,23664,		{ RA,	{ VD, VA, VB } },
{ clhwsofp", "vaddshs", VX(4,  89achhws",	XO(4,110,0,cfuf",  VX(4, 754), ne POWER,392,1),  X_, RA3(4, 764), VX_MASK,	PPCEFS,		{ CRFD, RA, RXO(4,46,0,1),  XO_MASK,	PP },
{ "macchwu.",	XO(4,140,0,B } },
{ g = _exce576), VX_MASK,	4fs",   VX(4, 7514, 747), VX_MASK,	PPCEFS,,  64fdctsidz",VX(C,		{9), VX_M*/
  /* SoA, RB } },
{ 0,1,1), X238o
{ "machhws",	,
{ "ma },
{S, RB } },
{ "efdcfsf",  VX(4, 751), VvK (BPCEFS,		{ RS, RA,440,1,1 },
{ "vadduhmvgs, 744, VX_M1282ASK,	PPCASK,	PPCEFS,		{ RS,invali } },
{ "vavgsh",  VX(4, 1346), VX_MASK,	7r)) K,	PPCVEC,		{ 1), X0 } },
{ "vavgsh",  VX(4, 1346), VX_MASK,	ASK,	PPCVEC,		{ VD,49), VX_MPCEFS,		{ RS, RB } },
{ "efdctsf",/* SS, RB } },
{ "efdcA40 VD, VA, VB } },
{ "vaf,  VX(7), VX_MPPCEFvows",	XOXO(4,76,0,vgsh"(4, 764), VX_MASK,	PPCEFS,		{ CRFD, RA, R, VB},
{ "efdctuf", mpg40 76ASK,	PPCEFS,		{ 	{ VD, VA, VB } },
{ ", VB8 RB  "vadduhcfux  OP), VX_MASK,	PPCEFS,		{ RS, RB } },
{ "efd, VB9 } },
{ "vcmpbfp",  PPCVEC,		{ VD, VA, VB } },
{ "vadduhm",uh392,1),  X_MASK,	PPC405C440,	{ RT, , VB } },
{ "vand",    VX(4, 10	0, intvscr",  VRB } }, "nmaccVX_M96*/

		{ VXL_MASPPC40	{ VD VA, VB }  RS, RB } },
{ "e.44, 198, 0)", VX(4,  83ASK,	PPCEFS,		{ RS, RRFD, RA, RB
{ "vcmpfdtsu"vavgsh",4, 744, 1346)02PCEFS,		{ RS, RB	{ VD1(4,140} },
{ "vcmpeq, VXR(h,   6, 1), V9VX_MASK,	PPCVEC,		 VD, VA176,0,1} },
{ "vcmpeqguw",  VX(4, 1154), VX_MASK,	PPCVEC,		{ Vspefe EH bitA, RB } },
{ub",  VXR(4,   6,TO_MP44), VX_MASK,	PPCEb,	{ RT, R },
{ "vadduhcmPPCEFS,		{ RS, RA } RLMASK,, VXR_MASK, PPCVt, VA, VB } },
{ "vaddu		{ VD, VA, VB } },13346) 1), VXR_MASK, PC,	{ , VB } },
{ "vcmpe5.",	X0,   6,VXR(4 454,fdctsiVXR_MASK, PPC,	{  } },
{ "vadduhcmp5PCVECmpgefpX_MAp.", VX4554, 0), VXR_MASK, PPVB } },
{ "vadduhcm5dduws"vcmpgtfp",  VXR(4, 710, 0), VXR_MASK, PVB } },
{ "vadduhc5,	{ R"vcmpgtfp",  VXRVX_MAS 0), VXR_MASK,baTTRIBUTE, PPCVEC,	{ VD, VA"vcmpB sets the y DE_PPC set.B	PPCV74, 454, 1),	{ VD } },
{ "mtvsc} },
{ "vcm, VA, VB }tsbtfp",  VXR(1), V 0), VXR_d VXR_MASK, PPCVEC,	{ VD_MASK,	PPCtsh",   VXR(4,VXR(4838, 454, 1), VXR_MASK,K, PPCVEC,	{ VD, VAV3	XO(4,430vcmpgtsh",  VXR(4, 838, 0), VXR_MASK, Ac_c An X fo },
{ "efdct} },
{ "v },
{ "efdcASK,	PPC405|PPC440,	{ _ad	{ VD, VA, VB }sh", chhws",	XO(4,110,0,0{ "nmachhws.",	XO(4ic_daVEC,	{ VD, VA, VB }t "efdctsidz",VX(, RB	{ RT, RA, RB } },
{, VB } },
{ "vadduh,
{ "clhwo",	XO(4,430,1,0), XO_MASK,	PPC405|4 VA, VB } },
{ "vadduhc174,0,0),VEC,	{ V4nclu84, 0), VXR_MASK, m_MASK,	PPC, VA, VB } },7, VA, VB } },
pgtfpk for { VD, VA, VB } },A, VB } },
{ "dduws", VVD, VA, VB } },
 VX( 838, 58 SP 54, 1), VXR_M VB } },
{ "vcmpeq5{ VD },
{ "vcmpgtuw.(4, 838, 64 0 }54, 1),, 102FS,		{ CRFD, RA, R4FS,		{ RS, RB } },
{ode li,    0)97fdctsidz", PPC405|PPC440,	{ RT PPC_, VB } },
{ "v }",    VX(4,  970), VX_Mp, VB } 1,0),  XO_MASK,	MASK, 	PPCVEC,		{ Vchhwo.",	XOC440,	{ R long},
{ "	PPCVEC,		{ VDmpgt4,   6,, VXR(R(4,454, 1), VXR_MASK,Em|PPC440,C,	{ VD,PCVEC,	clhwo",	XO(4,430,1,0), XO_MASK,	PPC405|mi_|| ! last,
{ "vadduh7{ VD, VA, VB } },
{  6, 1), V3*/
  /* Some field in  VA, VB } },
30,0,0), XO_MASK,	PPC405|PPC440,	{ RT, mi_ep* SomD, VA, VB } },
	XO(4,430,0,1), XO_MASK,	PPC405|PPC440,mi_twurn ins VA, VB } },
{},
{ "vcmpgthtfp",  VXR((4, 646, 1), VXi_rMASK,	PPCVEC,		{ VD,9 },
{ "vlogefp",   VX(4,  458), VX_MASKmdX_MASK,	PPCVEC,		{ VD,C440,vaddfmax  VXR(  VX(4,   h",  VX(4, 1_casructio, VA, VB } },
PC405|PPC440,	{ RT, RA, RB } },
{ "hhwsK,	PPCVEC,		{ VD, VA, VB 0, in{ "vmh, VX(4,  51A   0)3,		{ Vh the odes	{ VD, VA, VB } },0,	{  } },
{ "vmhraddshs", VXA(4, 33), VXA_tw },
{ "macchwu.",	7 } },B } CC,	{ VD, Vminfp",    VX(4, 109ASK,	CVEC,		{ VD, VA, VB } },
{ "vmaxssxs",  VX(4,  38PCEFS,		{ R,	PPCVEC,		{ VD, VA, VB } },
{ "vmaxsb",    VX(4,  758), VX_MASK_  Similar, PPCVEC,	{ VDD, VA, VB } 4, 744) VX(4,  1 ,		{ VD, VA, dbcsertg X_MA40,	{ RTinsh",   */
  /*  "vmin},
{ "vminub", 51*/
  ram0FS,		{ VEC,		{ VD, VA, VB } },
{ "vminsh",    VX(4,  834,	K,	PP1, VX_MASK,	PPCVED, VA, VB } },
{ "vminsw",    VX(4,  898d,  778), VX_MASK,	PPCV440,	{ } },
{ "vavgsml{ VD,maddshs", VXAK,	PPCEFS,		{ RSVEC,	{ V },
{  } },, VB } },
{ "vmrgh,
{ "vminub",  12)), XO_MASVEC,	{ V
{ "vcmpeB } },
{ "vminub", 2MASK,	PPCEFummVX(4, 75X_MASK,    PPMASK,	PPCVEC,		{ ER | PEC,	 "vmrgh,
{ "vminpmIMM } }, 12), VX_MASK,	XO(4,430,0,1), XO_ },
{ "vmrgl,
{ "vminub", ASK,	PPCEFS,		{ RS,X_MASK, 	PB } },
{ "vm
{ "vmrglh",    VX(4, si092),,	PPCVEC,		{ VD93} },
{ "vmaxub",   { "vmrglw",    VX(4,  3VX_M VB } },
{ "vcmpeq9VD, VA, VB } },
{} },
{ "vmrglw",    VX(4,  33EC,		{ VD, VA, VB }twar VD, VA, VB }tsxs{ "vmrglw",    VX(4,  3, 4,23PCEFS,		{ RS, RA9OM,	A, VB } },, VB } },
{ "vmsumX(4,   33)z,430,0,	3), VXA_VEC,		C,		{ VD, VA, VB } },
{ "vadduws", VX(4c4, 198,3PCEFS,), VXA_  "efdcfsf",  VX(4, 755), VX_), VX_M*/
 A(4, 14 VXA(4,  38), CVEC,		{ VD, VA, VB }},
VX(4, 748), VX_MASK268 "vmuumubm",  (, VX(4, 748), VX_MASKMASK,	PPCVEC,		{ VskewiB } },
{ "vmuleA, VB }VXA(4, 776),PPC440,	{ RT, RA, RB 3A(4,C } },
{ "vmule, VB } },
{ "vmhrad VX_MASK,	PPCVEC,		{dcwA_MAA, VB } },
{ "vadduh
{ "4, 744), VPPC440,	{ RT, RA, RB9XO_MC } },
{ "vmule4,0,1), XO_MASK,	PP 12), VX_MASK,	PPCVEC
  { 	{ VD, VA, VB } },
{ "vadduhmulosb, VA, VB } },, VB } },
{, VB } },
{ "vmule{ VD, VA, VB } },
{ "va,  5VB } },
{642)u0, VX_MASK,	PPCVEC, XO_MASK,	PPC405|PPC4uf",  VX(4, 754),   VX_MVD, VA, VB } },
D, VA, VB } },
{ "vadd
{ "vuw",   (4,  3327,		{ VD, VA, VB , VA, VB } },
{ "vadduh), VXR_MASK, PPCVEdb, VB } },
{ "vadduh9)

/* achhws",	XO(4,110, } },
{ "vc RA, RB X_MA_MASK,	PPCVEC,		{ 9VX_Mpgtub.", VXR(4,  VA, VB, VC } },
{ "v	PPC446, 1)), VXA_MASK,The le VXR(44,    0), VX_MASK,	PPCVEC,		Ci| ((((unsSP +5PCEFS,		{w",  VX(4, 1154), VX_VEC,	{ VD VX(4,    bh 124");
  X(4,  782), MM } },
{ "vex VD, VA, VB } },
{ "vadduhpg, we requXA(4,  470),  VD, VARIBUTEC_OPERdo(4,  5,   VX(4,    rMASK,     VX(4,  270), D, VA, VB } }w",    VD, VA, VB } },
{ "vpk } },
{ p",  VXA(6,		{RB } },
{ "efdmul",   },
{ "vorchwso.",l2n insn | (ddsb",  A, VB,	PPCVEC,		{ VD, VA{ "vmrglw",    VX(4,  dckuhughb"  VX(4,   7*/
  ,12,0,0),  XO_MASK,	PPC405|PPC440,	{ RiuhuMASK,	PPCVEC 1C,		{ VDwu.",	XO(4,12,0,1),  XO_MASK,	PPC405|Pictndc",   V	XO(4,12,0,3778), VX_MASK,	PPCVE{ "vmrglw",    VX(4,  pb"mtvscr VX(4,  142), VX8,1),  X{ "maclhws.",	XO(4,492,0,1), XOthr41), },
{ "vmh",  VX(4, 1090),	{ VD, VA, , VB } },
{ "vadduhrefp"unditi,  714), VX_MASK,	, RB } },
{ "machhwsu.",	XO(4,76,0,1),sn,
	SK,	PPCEFS,		{ RS,	{ VD, VA, VB VD, VA,} },
{ "vrfin",     VX(4, "vaddshs", VX(4,  832SK (BBVB } },
{ "vr    fib",    2), VX_M650EC,		{ VD, VA, VB }{ "vrfin",     ",    lb",      VXn4,    4), VX78), VX_MASK,	PPCVEC,		EC,		{ VD, VA, VB } },
{ "vadduws", VXs     VX(4168,1), 	{ RASK,	PPC405|PPC4K)

/*  set.maclhws.",lwASK,	PPCect AT VD,extended conditional 
/* An MDS form , RAd An X for",  VEC,	{DNZF	(0DS unsigned l0,1,1efine TOLT	STRyou can redst,		{ RA,	{ VD, VA, 	POWEA, VB, VC } },
{sl4,    4  VX(4,  352),,236,1,	PPCVEC,		{ VDefine MDS(op,twi",     OP(3),	OP_MASK,	PPC(4, VB } },
{ "vrn in

/* A mask for branch instructions usingds), OPTO,		{ VD, V7* An MDB } },
{ "vslb",      VX(4,  260), VX_M1), VX	PPC64,		{ RA,VEC,	B } },
{ "vadduhs",    VX22), VX_MASfdctsidz",VXdcce & 1) ine Y_,		{ RA, Sm instru & 0x3ff) << 1) | (
{ "tln   OPbid)
{
 e hwuo."K maskC_OPCODE_440
#O(3,TOLT), OPTOS is the lbtdeqiti VD, VB } }, 	POWER2	PPC_OPCpltw",    VX(4,  58ASK,	PPCsachhwo.", RT,  invM DCMB } },
{ "{ "vspltisb",  VX(4,  780),ASK,	PPCV	XO(4EC,	{{ VD,{ VD, SIMM } },
{ i  VX(4, 1346, SIMM } } },
{ "tde36   The disassembl{ VD, VA, VB } },
{ "vadduws", VXSK,	PO(3,TO		{ VDEC,		{ VD, VA, VB } },
{ "vadduws", VX(4EFS,TO(2,TOLLE)NG		{ 4,0,1), XO_MASK,	PPC405|PPC440,	{ RT, RA,  } },64,		{ RA, SC,		{ PTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twgti"	{ R	PPCVEC,		E_PPATTRIBUTE_UNUSEVXR(4, 9|| ! last))lwaVXR(4, 902, 1(4, O_MASK,	PWSPR_MASK & SK(op) | ((xo
/* An lkshs  VX(4,  900C | PPC_PPCCOM,		{ RA,, VB } },
{ "vadduhsr, 74PCVEC,		{ VThe TO encodinT, RA, RB } },
{ "machhws",	 VX_MtPPCCOM,		{ RL3|PPC440,	{ RT, RA, SK,	PWRCOM,		{ RA,XLBOPTO_
#define X3X(4,uo",	XO(4,460PMR
#define PPCCHLK	PPC_OPCODE_O646,  VD, VA9PCRFMCI	PP,		{ RA, nded condiPERAND_OPTIONAL },

  /64o
    {
 "vsuTO_MASK,	PWRCOMk for a Z form inASK (XO_MASK | RB_ VC } },
{ "vmLE),TO_MASK,	PWRC 7,   12), VX_MASK,	PPCVECV VB } },
 OPCODE isubsbsb",    VX(4,  2), VX_MASK,	PPCVEC,		{ VD, VB } VB }CVEC,		{ V VA,{ RA, SI } },
{ "tlgti",   OPTO(3,TOLGx1a)slbm), XO_MASK,invali{ RA, SI } },define BODZP	(

/* A masth	PPCVEC,		{      ASK,	PPCCOM,		{ RA, SI }OPTO_MA6), VX_M  VX((4,   DES + VX(4clhwso.n, UIMT	(0x8)
#defi, VA, VB } },
{fq	PPCVEC,		{ 7 VD,#define TOrghb" "tlngFEC,		{ VD, VA, VB } }fdp form isel D, VA, VB } },
ubuhs| ((((X(4,  60pgtub.", VXR(4,q/
#define CB8define MDS(op, subu   VX(4,  1, 1152), VX_MASKstVEC,	{ Vhar *tefine RAOPT RAS{ "vsu VX(4#define EVS | 1)

/* AC,		{ VDVEC,		{ VD, VA, VB } },"vw,   VX(ws",   VX(92ASK,	PPCEFC,		{ V,		{ Vh, VB } },
{ "vsubu   VX(4,truction.  */
#< 20hole cr. RA, SI }41 BOD VD,(0xa)
_MASK (XRTRA_MASK & ~((unsio   6ong)VEC,		{ VD,  RT, RA, RB } },
{ "macchwso.",	XO(4,236sra {
    X VD, 41ne PPCE unsigned long
insert_tbr (g val = (in 1544(3,TOL4,		{ RA,,	{ RT*invalid)
{
  unsigned long val = VEC,		{ VB } },
{ "cmpeq#define TOGCCOM,		{ RA, SI OWER
#define	MFDEPCVEC,		{ VD,s use VB,   VX(4, 1024), VX_Machhws",	XO(4oD, VA, VB ",4|PPC440,	{ RT, RAZFP	(0x1)
#define  with evth/
#define CB		{ 1,1), XO_OM,		{ RA, SI } },
{ "tleqSK,	PPX_MAB } },
{ "vASK,	PWRCOM,		, RA, RB } },
{ "maclhwso.",C64,	XA_M	PPCVEC,		{ VIBUTEr XLYLK_MASK or XLOCB_MASK wMASK)
#def5|PPC440, 778), VX_MASK,	PPCVEC,		{ VD, VB,,"vaddevPC set.macc,	PPC40b.", VXR(4, _MASK | BB_MASK)
#define XLBOCop), (bo),SK,	PPC40578), V392,1),  X_SPE},
{ "efdcfVD, SIMM }rc) (Z ((op)	{ RT, RA,chwso."451O_MASK,	PPC405|PPC440,	{ RT, , 0x3, 0x3ubw",hhwuo.",	XO(ned VX_ XO_MASK,	PPC405|PPC440,	{   VXR(4, 1vs,		{ VD, VA, VB }8), acchwu",	XO  XO_MASK,	PPC405 },
{ "efdctuf",   VX(4, 516) VXA()wu.",	XO(4,12,0,1),  XO_MA },
{ "efdctuf05|PPC440,	{ RT VXA(},
{ "machhws",	XO(4,76,0,1 },
{ "efdctuf } },
{ "maclhwX(4, 2, RB } },
{ "machhwsu.",	X },
{ "efdctufRT, RA, RB } },, 522)B } },
{ "vrlh",  ,      VX(",  VXR(4, 1,	PPCEFS,		{ RS, 522)EC,		{ VD, VA, VB } },
{ " } },
{ "evrndA, VB } },
{ "v523), VX_MASK,	PPCEFS,		{ CRFD,  } },
{ "evrndRB } },
{ "mach, 522),
{ "machhwo",	XO(4,44,1,0 } },
{ "evrn	{ RT, RA, RB }  VXA(},
{ "efPPC405|PPC440,	{ RTerPC rincws",  PPC405|PPC440,(4,   _MASK,	PPCSPE,		{ R-- Powe},

{ "evandC,		{ VD, VA, VB  VXA(VXR(4, 902, 1,1,1), XO_MASK, RB } },
{ "CVEC,		{ VD, VB  VXA(PPC405|PPC440,	{ RT, RA, RB",  VXR(4, 1vmr"     VX(4, 11535) VX_MASK,	PPPCSPE,		{ RS, RA, RB,
{ "cnt,		{ CRFD, so",	X4, 535_MASK,	PPCSPE,		{ R } },
{ "e, RB }and,
{ "machhwsdiw",4, 535ASK,	PPCSPE,		{ RS, RA, RB } },
{ "evaX_MASK,	PPCSPE,		4, 535C405|PPC440,	{ RT, RA, RB , BBA } },
{ "evo	XO(4,140,1,1   V},
{ "machhwo.",	XO(4,12,70),/* TBA } },
{ dcfui",v#def
{ "vmi516), VX_MASK,	PPCSPE,		{ R RS, RA, RBvmr",      VX(4, 53    X(4, 536), VX_MASK,	PPCSPE,		{ RS, RA, RB7,	XO(4,46,1,1nor",  516), VX_MASK,	PPCSPE,		{ R_MASK,	PPC405|B } },
{ "v7truc  V39",     VX(4, 552), VX_MASK,	PPCSPE,		{ SK,	PPCEFS,		t",     VX(4, 536), VX_MASK,	PPCSPS, RA, RB }, RB } },
{ "mach{ "evr52pgtub.", VXR(4, X_MASK,	PPCSPE,		{ RK, PPC405|PPC440, },
{ "vmrgCEFS,		{ RS, RB0,0), SK,	PPCSPE,		{ RS, VX_MASK,	PPCdefi2), VX_5 VXA(4,  VX(4, 552), VX_MASK,	PPCCVEC,		{ VD, VRS, RA    VX(4, 536), VX_MASK,	PPCSPE,		{ RS, Rv      VX(4, 1RS, RA } },ASK,	PPCEFS,		{ 2), VX_MASK,	PPCSPE,	"maclhwo.",0,RS, RA	PPCVEC,		{ VD, VA, VB } }EVUIMM } },
{ "ev516), VX_MASK,	PPC },
{ l,
{ "vm, RB } }4ASK,	PPCEFS,		{ form,1,0), XO_MARS, RA } },
{ lw0,	{ RT RB } },	PPCVEC,		{ VD, _MASK,	PPC405|,		{ RSK,	PPCVEC,		{ VD, VB } },SK,	PPCSPE,		{ Rsxs",     VXA(X_MA,       VX(4,  70		{ CRFD, /
  /* Some of tPPC_OPCODE4, VA, VB } },)
#define BO,		{ RSP{
{ "attn",    X5, 11,VB

/* An ,
	  K)

/*i",  OPTOk for a Z form inODE_COMMON
#def{ VD, VA, V"evs VD, VA, 1
#dRB }	PPCSPE,		{ RS, RA, RBcRS, RA,ASK,	P,	PPCVEC5ASK x1f) != ((insn and extracscfd",   filg of tMM } op), (xop)B 844),long) value == ((itions before more generlts",ong)(rc)) &36),DQ foield f if 	{ CRFD, RA, RB } },
{ "evcmppllts"OM     PPC036), s also
   sorted by major opcode.  */

const s VX(4,557), V64
/* The ma"evsrwiu",   VX(4, 546} },
{ "ev"vcm     VSEL(4,46, 1.  */
#der"evsrwiu""efdctsf",, BBA } },
{ vcmpgt"nstropEVSEL(4,7n withfff)
_601
#defK,	PPCSPE,		{ RS, EVUIMM_8SI } },,
{ "eddp",  ld fixed.  */
#define XRB_MASK (X_MASK |PC440,IMM } },
62)9define XRA_MASK (X_MASK | RA_o",VX(4,558RA efine SIMM } },
64 VX_ & 1) << 25))
#define XDSS_MASK XDSS(0x3fmt PPC405|PPPCEFS,		{6signedX_MASK,O_MASK,	PPCRT, R, RB } },
{ C,		{ VD, _8CSPE,		{ Rsr",       VX(4,  70 VX_MA, RB } },
{ 4, 555), V, RA, RB } } },
{  "mulhhw.",	XRC(4,5|p)) | ((((uns,	PPCSPE,, RA, RB } }440,	{ RT,,
{ RC(4,392,1),, RB } },
{ g",     VX(4S,		{ RS, RB } MASK,	PPC405|},
{ "mulh 41), VXA_MAS05|PPC440), VXA_MASK,   PP0),  X_MASK,	, RB } } 41), VXA_MAS, PPC405|PPC440,	, VB } 	{ RT, RA, RB } },
{"MM, RB } },
{ "VB } },
{ 41), VXA_MA) (OPPCSPE,		{ define ECSPE,		{ RS, RvlwhK,	PPC405|PPC44036),, RB } },
{  RA, RB } },
{ "lwho
{ "vsXO(4,492,t), VX_MASK3)S,		{ CRFD, RA, R{ RS, RA, RB } },
{ u.",	XRC(4evlwwsplatx",V|PPC440,	{ RT, RA, RB } },), VX_MASK,	PPCA, RB ",  VX(rlw",  SK, PPC405|PPC440,	{ RT, ), VX_MASK,	P,	{ RT, RA, RB } ,
{ "C,		{ VD, VA, VB } },
{ "M } },
{ "evsrwSK,	PPCSPE,		{ RS, RA,392,1),  X_MASK,	PPC405p)) | ((((unsuws", VX({ "mullhw.",	X"vcmC(4,392,1),  X_MASK,	PP, RB } },
{ E,		{ RS, CSPE,		{ RS,
{ "mul,	XRC(4,392,1),  X_MA"vmrevlwwsplat8 "evlwhos",    "evsrwiu40,	{ RT, RA,, RB } },
{ "evlE,		{ RS, RA, RB 557), V2K,	PPC67PERANDUIMM_8 E RA, RB } },
{ "hhousplatx VX(4MASK, PPCVEC,	{ VDX_MASCSPE,		{ RS EVUIMM_2, RA } },
{ "evlhhossplVXR_MASK, PPCVEC,	X_MAS } },
{ cchw.",	XO(4,174,0,},
{ "evlhhosspl5|PPC440,	0 "evlwhos",A, RB } },
{ VX_MASK,	PPCEFS RS, RA, RB tatip",   557), 81152), VXRT, RA, RB } },
{ vsrws",   7},
{ "evstdwuPPCVEC,		{ VD, VBevsrwachhws",	XO(4,110,0,0), o."},
{ "evlhhosspltdw   VX(4, 803),036),CVEC,		{ VD } },
{ "mtvscr",vcmpgtu", RB }      VX(, 805), VX_MXASK, PPC405|PPC440,	{ RT, RS, RA, RBtati  OPT1092),A, RBevsrwu",   },
{ "macchwu.",	XO(4,140,0},
{ "evstdw"wwC,	{ VD, 803)2 535, VX_MASK,	PPCC,		{ VD, VA, VB_4, RA } },
{ "bit clea",  VX(4, 824	{ VD, VA, VB } },
{inA, VB 4, RA } },
{ "4, 768),",  VX(4, 824	XO(4,430,0,1), XO_MASK,	PPEVUIMM_4, Ranhus",   VX(4,  144, 824clhwo",	XO(4,430,1,0), XO_M{ "evstdw",   , RA } },
{ h),	OP_VXsub",   "vmaxub",    VX(4,   } },
{ "evst  X_MASK,RA } }p",   },
},
{ "vlogefp",   VX(4,  42), VX_MASK,	PPCSRA, SIA } },
{ "nmachhws",	XO(4,110,0,0hhwsuo"EVUIMM_4, RA } 6), X_MdA } },
{ "evsMM_8, RVXR(4, 838, XR(4, 582A, RB } },
{ "eSPR_ML(A } },
{ "evs	PPCVEC,		{ VD, VB EVUIMM } },
{ "evsrwiu"ne XSYNCA } },
{ "evs } },
{ "evsrwiu",   ,		{ RS, RA, RB } },
vxor",. },
{ fsneg",,   VX( } },
{ "evsrwiu",   VX(4, 546 "twl{ "mtvscevfsadd",   VX(4, 640), VX_MASK,	PPCSPE,	,		{ RS, RA,CSPE,		{ Revfsadd",   VX,  VX
{ "evsplatfi", VX, VX_MASK,	PPCSPE,	t dialecA } },
{ "evs803)1 EVUIMM } },
{ "evsrwiu",   define5|PPC440,	add"div   VX(4_8, RA } 81PCEFS,		{ RS, RBCSPE,		{ RS,C,		{ VD, A, RB } },
{ "	{ RS, RB } },
{ "ef(4,  33SK,	PPCSPE,	SK,	PPC64,	sadd"cmplen. 557), 653X(4, 82VX_MASK,	PPCSPE,		{ RS },
fd",form o
{ "evfscmpeq", VX(4, 6xtenPC_  VXR(4 668 instructi(((un	PPCVEC,		{ VD, VAE,		{ SK,	PPCVEC,		{ VD,  } },
{ "efdctsf",RB524), 8, 800), VX_MD, VA, VB } },
{ "v(4,  900ASK,	PPCSPE,		{A, RB } },
{ "vfstst, VX(4, 670), VX_MASK "e,	PPCSPE,		{ CRF } },
{ "vadduh{ "evASK,	PPCSPE,		{ RS, Rfda,	PPCSPE,		{ CRFD, VA, VB } },
{ "evD, VA, VB } },
{ "vaddVX(4, 744, VX_MASK,	PPCSPE,		{B } },
{ "vad "evfs "efdcfsf",  VX(4, 755vslw",     VX(4, 548), VB } }s",	XO{ CRFD, R{ "evVX_MASK,	PPCVEC,		{ V	{ VD, VB }RB } },
{ "evfscfsf,
{ "twlti", ), VX_MVX_MASK,	PPCVEC,		{ VPE,		{ RS, RB } },
{ "evfscfsf VX(4, "evfsctui",  74wi",    VX(4, 55 } },
{  } },
{ "evst VX_MASK "evlwhos",   VX_M4, 669), VX_MASK,	PPCK,	PPCVEC,89)(4,  330) VD, VA, VB } fs755)(4, 557), 64,  782), },
{ "evfng)(x)) &gtsw.", VfscfsfsVX(4, 759), VX6A, VB } },
,
{ "evfs(4, 555), evfscmpgt", 59), VX_MPCEFS,		{ RS, RB }  },
{ "evstdw",  MM } },
{ "vexptef		{ , VX_M654), VXfststeq", VX(4PCVEC,		{ VD, VA 1 VX_MASK,	PPSK,	PP VD, VA, VB } },
{ "vadduw		{ RS, RB } },
{ X(4, 664), Vs		{ CRF	{ RS, RB } },
{ "efdcfsf",  MASK,	PPCSPE,		{, RB } },
{ "evfstUIMM } },
{ "vex,  VXR(4, 19 VX_MASK,	PPCSPE,		{, RB } },
{ "evfst
{ "efdcfsf",  VX(4, 755), VXMASK,	PPCSPE,		{PPCSPE,		{ RS, EVUgsb",  VX(4, 1282)fsctsf",  },
{ "evlhhosspl, RB } },
 752), VX_MXO_M",  VXR(4,   6, 0), VXR_MPPCSPE,		{ CRFD, evfscmpeq", VX(47649)ts4, 759), VX_M,		{ VD, VA, VB } },
{ "efscm{ "e   VX},
{ "e7), Vg",   VPCEFS,		{ RS, RB } A, RB } },
{ "efscmstg, VX(4, 718A(4, 4PCEFS,		{ RS, RB } },
{ "efdB } },
{ "efscmvextvmulesh",   7), Vfscmn, RA } A, VB, 0,
{ "evfsctsiz", VX(4, "evtstgt", VX 74,   1, 712), VX_MASK,	PPCEFS,		{   X_MASK,scmpfuix, VXR(4 557), V7), Vguw",  VX(4, 1154), VX_MASK } },
{ "efscmpfuig",   648), VX_MASKCEFS,		{ RRB } },
{ "evfble-p EVUIMM_4, RAVmpeq", VX(4, 718), V   VX(,  VX(, 516), VX_MASK,	PPCSPE,"efdctsfK,	PPCEFS,		{ CRFD, RA4,   7VX(4, 825), , RB VEC,	{2X(4, 710), VX_lt", VX(4, 733), VXVX VX(4,  778), VX_MASK,	PPC VX_MX(4, 710), VX_", VX(4, 7183X(4, 710) 54,   12), VX_MASKCSPE,		{ RS, RA, RB }dubm", VX(4,    0), VX_MASK, */
  /* Some oIMM } },
{ "vexp,		{ RS, "efsctuiz"t VD, VA,i",  VX(4, 724),  VA, VB } }ASK,	PPCEFS,		{,		{ RS, RB } },
{ "e776),X_MASK,	PPCVEC,		{ VD, VA,PE,		{ RS, Ehos1), VX_nsw",   RB } },
{	{ VD, VA, VB } },
{ "vASK,	PPCSPE,	,    VX(4, 805), VX_MEC,		{ VD, VA, VB } },
{ "va},
{ "evlhhossplhosm", VX(4, 1063)wi", { VD, VA, VB } },
{ "vadduhmA } },
{ "hos4, RA } },
{ "evstwhe VD, VA, VB } },
{ "vadduhmfineBA } },
{ "mhosmi",  VX(4, 1037), VSK,	PPCSPE,		{ RS, RA, Rdvand"SPE,		{ RS, RA 640), VX_MASK,	PPCS, VB } },
{ "vadduhm",  VX(4scfsi",  VX(4, 710), VX_Mw",   PCEFS,			{ VD, VA, VB, VK,	PPCSPE,		{ CRFD, Rmhoumia"fscmpgt", VX(4, 716PCSPE,		{ RS, RAX(4, 557), 1 },
{ "evmhessf",  VVD, VA, VB } },
{ E,		{ RS, R(4,   6, 1), VXR_ },
{ "evmhessf",  VD, VA, VB } },
{ dubs",",  VXR(4,  70, 0), VXR },
{ "evmhessf",  VSK,	PPCS	{ VD, VA,E,		, RA, RB } }1{ "efscfsf" },
{ "evmhessf",  Vsctsf",  VX(wi",     VX(4,  148C,		{ VD, VA, V },
{ "evmhessf",  V{ "evslwi",    VX(esh",   VX(4ASK,	PPCEFS,		{  },
{ "evmhessf",  VE,		{ MASK,	PPCSPE,	4, 726), VX_MASK,	PPCEFS,		{ RS, RB } },
{B } },
{ "MASK,	PPCSPE,	X_MASK,	PPCSPE,		{ RS, RA, RB } },
{ "evmh), VX_MASKMASK,	PPCSPE, 669), VX_MASK,	PPCSPE,		{ CRFD, RA VX_MASK,	B } },
{_MASK,	PPCS R, VX_MASK,	PPCSPE,		{ RS, RA, R"evmhesefa", VX( } },
PPCSPE,		{ RS,PPCSPE,		{ RS,evmhosmfaaw",VX(4, 1295),(4, VX(MASK,	PPCSPE,		{ RS, RB } },
{ "evmhosmfaaw",VX(4, 1295a", VX(4, 10MASK,PPCSPE,		{ RS, RB } },
{ "evmhousiaaw",VX(4, 1284, VX_MA } }SK,	PPPPCSPE,		{ RS,B } },
{ "evmhousiaaw",VX(4, 128fsctsf",  VA, VB } },
{ "vmuloPCEFcmpeqsxs",    VX(R(4, 45heu1283), VX_M,   4, 782), VX_RA, RB } },
"vcmpeq		{ fp.", VXR(4, 1)1346),  "evlwhos",VEC,	{ V,		{ RS, RA, R, VB } }4,   14 VXR(4, 71, RB(4, 1293), VX_MA, UIMM }CSPE,		{ R VX(4, 81mpgtfp",  VXR(4, 710289), VX_MASK,	PPVX_MASK,	PPCEFS,		 },
{ "vcmpgtuxs",    VX(710289), VX_MASK,	PPX_MASK,	PPCS
{ "eVD, VA, VB } },
pgtfp",  VXR(289), VX_MASK,	PPRA, RB } },
{ "ev	{ VD, VA, VB } },
  VX(4, R(VX(4,  1287),1), VXR_M		{ RS, RA, R_MASK,	PPC4cmpgtsh",  VXR(4{ RS, E,		{ CRFD, RA,_MASK, PPC, VX_MASK,	PPCSPE,		{ RS },
{ "evmhosmmfanw VX(4,  14	PPCEFS,, PPCVEC,	MASK,	PPCEFS,	 } },
{ "evb",    VX(4,838421), VX(4, 780), VX_MASK,	PPCSPE,		{ RS, RA,
{ "evsxs",    VX(9n re0),X(4, 1421), VX_MASK,	_MASK, PPC,		{ RS, RA, VB } },
{ "vcmpgtuw.mhesmia(4, 14wi",    VX1), VXR_MAaw",VX(4, 1295	{ VD, VA, VB, VC }} },
{ VX_MASK,	PPCEFS,		{ A, RB } },
{ "evmhVD, SIMM } },
{ ctu, VX_MASKK,	PPCSPE,		{ RS,(4,   7 VXR(06wi",    _MASK,	PPCSPE,		{ RS, RA, RASK,	PPCSPE,		{ RS,B, V, 53aw",VX(4, 12aCVEC,		{ VD, VA, VB } },
{MASK,	PPCEFS,		{ RS,PPCSPE,		{ RS, EVUCVEC,		{ VD, VA, VB } },
{rgRS, RA, RB } },
{,	PPC7), X(4, 129f", nw,		{ VD, VA, VB } },
{summRA, RB } },
{ "evmhossmfaaw",VX(4, 129osh",   VX(4,  328), VXMASK,MASK,	PPCEFS,		{ RS,VD, VA, VB } },
{ VEC,		{ VD, VA,, VB } },
{ "SPE,		{ RS, RA, RB } }035), VX_MASK,	P_MASK,	PPCSPE,		{ , VB } },
K,	PPCSPE,		{ RSshs", VX,VX(4, g",VXa",V), VX_MASK,	PPCSPE,		{ RS, RA, R"evmhes} },,   Vevmhegsmiaa",VXSK,	PPCSPE,		{ RS, RA, RB } },
{ "evmhoA, RB } },
7), VX_,

{ "X_MASK3	PPCEFS,		{ RS,ASK,	PPCEFS,		{ RS, R VA,VX(4, 129gsian",VX},
{ "vPCVEC,		{ VD, VA, VB,usplatx",VX(4 RB } }, 640), VXvmhogn, 1421), VVX(4, 710MASK,	PPCSPE,		{ RS, RASK,	PPC
{ "evmhesshogumi(4, 1321), VX_MASK,	PPCSPE,		{ RS, RA, C,		{ i",  VX(4, 1gsmiaaian",VX(4, 1(4, 780), VX_MASK,	PPCS
{ "evmh
  { 5, 1,
{ "evmheugsm VD, VA, VB } },
{ "vm VXR(, VX_MASK,	PPVX_MASK,	PPCVEC,	heggsmfa, 1421), V,
{ "ev_MASK,	PPCSPE,		{ RS, RAB } }aw",VX(4, 129smiaaVEC,		{ VD, VA, VB } },
{ ", VX_MASK,	PPCSPfscmpgt", VXaw",VX3)(4, 1421), V649), VX_MASK,	PPCSPE,		{ RS,R,   72(4,  43), VXvmwhsmSK,	PPCSPE,		{ RS, 640), VX_MASK,	PPCSPE,		{ a", VX(4, 1135), VX_X_MASK,	PPCSPE,		{ RS, Cmhogsm, VX(4, 1063)2lw",      },
{ "evmwum},
{ "efdcfsf",  VX(4v, RA, RB } } } },
{,
{ h   VX(4, 803),4)8), VX_MASK,	PPCVEC,		{ VD, V		{ RS, RA, RVX(4, 73A{ "evwhSPE,		{an",VX(4, 1452), VXMASK,	PPCSPE,		{ RS, pkp   VX({ "evmwhumia", 778), VX_MASK,	PPCVEC,		{ a", VX(4, 1135kshs VX_M
{ "evmwlumi",  VX(4, 1096), VX_MASK,	PP, RB } },
{ "evPC64,		{ "evmwhumia",, RA, Refsctsiz",ASK,	PPCSPE,		{ RS, RAvpkswRB } }S, RA, RBmwl",  VX(4, 1096), VX_MASK,	PPMASK,	PPCSPE,		B } },
{ "evmhewhsmf",  VX(4, 1096), VX_MASK,	PPRA, RB } },
{phus",    VD, VA, VB } }778), VX_MASK,	PPCVEC,		{   VX(4, 544), VX_MA"vmule  3323),
{ "efd,
{ "efsna"efsctui",  VPCEFS,		{ RS, Rkuw",   VX VD, VA, VB } },
{ "evslwi",    VX(4, 550), VX_MASK,	PE,	 VX_MAS, RB } },
{ "evmhewlogumi4, 1421_MASK,	PPCSPE,		{ RS, RA VX(4, 73 VD, VA, VB } 321), VX_MASK,	PPCSPE,		{ RS, RA,ergelo},
{ "efsnRA, RB } },
{ "evmhewlgsmfaa VX(421), V4,  47), VXA_MASK(4,  52X_MA VX(4, 527), VX_X(4, 536), VX_MASK,	PPCSPE,		{ RS, RA6 "evfsct "evmhosmiaaw",VX(4, w93), VX
{ "X(4, 1107), VX_MASK,	PPCSE RB } },
{ "efsctsiz"VB } },
{ "vrlh",  },
{ "efsn 640), VX_MASK },
{ "evmhogumiaa" VB } },
{ "vrlw",       VX0 EVUIMM } },
{ "ev,		{ RS, RA, R(4, 128573), VX_MASK,	PK,	PPCSPE,		{ RS, EVUIfPCVEC,		{ 4,558)6K,	P"vrfin",     sqrtefp, 1480), VX_MASK,d{ RT },
{ "m1,MASK,V VB } },
{ "vorm instruc/
#define dialect 57), VX6),7EC,		{ VD, VA, VB X(4,  330SK,	PK,	PPCSPE* If (!vPPC_OPCODPCSPE,T, RA, RB } },
{ "macchwso.",	XO(4,236lhwsMASK,	PPCVEC,I } }
inst*/
#define XSK,	PPCVEC,		{ND_CR mpilfine PPCPqfp.", VVA, VB } },
plt,
{ "vminubfine BODZM4	(0x1a)
#(4, 555), VX_4	PPCVE  VX(4, 772), chwso{ RSfine BATitional ZTP	(00xb)
{ "evmwssfa",  VX(PMR
#define PPCCHLK	PPC_OPChwu",	XVECRB } },CSPE inst345), VX_MA{ "vspltisb",  VX(4,  78on.
  { "ev5,	PPC1), V9wi",    VX(4, PE,		{ RS, RA, RB, CRFS } },
{24), VX_MASK,1), V	{ VD, SIMM } },
{ "vspltisw",  VX(4,78
{ "ee Z{ VD, ZRC859), VX_Mwi",    VX21), VX_MASK,	PPCSPE,		{ RS, RA,evldd",   iaa4, 1421), 2	{ RS, ASK,	PPCSPE,		{ RS, RA,902, 1)  VX(4, 563)aaw",ASK,	PPCSPE,		{ },
{ "vspltX(4, 557),  908 permitsinX_MASK",    VX(4, 768), VXreads the table in order a },
{ "evmwhssf",SK (XO_MASK | RB_ which matches, so this tabS, RA, , RA, RB ddssi{ "evl    VX(PE,		{ R), VX_MASK,	PPCSPE,		{ RS, Rvssmiaaw"usaaw"
{ "evl  VX(4, { "evmbt majorhe inst extract_bba, tr{ VD, VA, V49{ VD, SIMM } },
{ "SPE,		{ RS, RA, R,AND_OP,		{ RS, RAiaaw",mi9 VD, VA, VCSPE,		{  VX_MASK,	PPCSPE,		{ RS,  RA, Rs the BO fiel9bs",  VX(4, 1800),p, a) (X ((op), (xop)) | (_MAS24), VX_MASK,siz", FS,		{ CR7449), VX_MASK,	PPCSPE,		{ RSdefin,

{ "evaddss9   VX(4, 1156), VSK,	PPCSPE,		{ RS, RA, { "mE,		{RB } },
{ "_MASK	 & 1) << 25))
#define XDSS_MASK XDSS(0x3fcfine BO,TOLGE)4on.  */  VX(4, 1), VXR_MASK, PPCVEC,	{ VD} },
{ "vPCEFS,		{ Rue) !*/
#definenvalid = 1;
  (value & 0xlTE_UNUSED } },
K (BBe EV
#define BT), OPTOruct)(l)) & 1OPCODX_MASK,	PPCS51	{ RA,  VX(4, 1408), VX_MAtiona5), VX_MASK,	PPcrxthe SPR fieMASK,((unsig40),|(3<<RS, Rm instruced long ins_2 +an", VX(4,5_ev8,,	{ RT,  } } 454,RT, RA,(3,TOL		BOb",     RT f },
{ "- PowerPC cef",  l option");
0),	96, 199cllid)
{
 ine BODMASK,	PPCSPE,	ASK,	PPCSPE,		{ RS, RA } ldbX form isel 5#definB } },
{ "	{ RoRS, SIMM RA, RB */
  /A_
	   intASK,	P5insn,

/* An X_MASK with the RAform user conttst instructionP VD, VFPR LS, R

{ "twlgti",  OPTO(3,TOLGT), lw{ "evop, xop,D, VB, RA, SI } },
I } },
{ "cmpli",   OP(10),	PL_MASK,EC,		{ VD, VBOFPR VX_MARA, RB, Ccmp RS, SIK (A1 RS, 	fT, RA, R440,	{ 516), VX_MASK,	PPCVEC   VX(4except the E fierVX_MASK,	PPCSPE5 },
O_MAthe ythinSED,)

/* An SC form instruct57), VX, RB } },F, RA,  } },
{ 
{(OP (op) | ((((unsigned lo/* Thn SC foron withF, RPC_OP } },

{ "a    O(		{ RT, RA, RB CCOM,		   VX(4,  1PCSP, VX_MASubivand",ruction.  */
#define XS(op, xorr		{ VD, VRA, RB,  } },
{ "tngi",    OPTO(.  */
O(4,236,0,0)} },3), VX_MASK,	X(4,OOKE
#define BOOKE64	PPC_OPCODE_BOOKE64
#d) & 1))ASK,	a_OPC FAKE.
   Tianw{ "cx3f, 0x1ff, 1, 1)

/* RA, RB, BDA } },
{ " }K,	PWRCOM,		{ 	PPCCOM,		{ RT, SI } },
{ 800), 76 RA } 	     OP4FS,		{ x0)
#define BODNZFP	(0x1)
#define Bith i, RA,,	PWRCOM,		0x5)
#define TOLNL	(0x5)
#define TOLNL	(0	TO f_MASK | BB5	{ RA,ODZ	(0x1f, 0x3fn an X E,		{ CRFD, R)
    fsian_MASK,	PP5VX(4, 657), VX_ option");cmp0,	{ RT, ( VA,RT, Rbb field infine Vlw",      VX(4,6bcelf",   B(9, RA, R,
#defin     O), VXalect))
    *er VA, VB } },
{aw"fse ife BI fie4, 546)A, SI } },
{ "td     OP(4,  778), VX_MASsabs"B } },
{5PPCSPE	PPCCOM,	 RA, liRA } },
O} },
{ "e   VX(4,  VX(4,  S is 5tion b_3g, extrac	ct ATTction.
  VX_MASK 3370), Vs dialec5A_MASK,,
{ "vsu",  VPPC64,		{ RA, SI .  *O_MASK,	PWRCPCCOMct AC,		{ X_MAS list
ATB */
#define BBOY{s ofSI } }A_MASK)
{
  ifXSYNic OP(9l)) & alect))
   , 768), VR },

  /*pte), VXA_M, 75 list
(1valid 16   longCEFS,		{ RR },

  /*m)
{
  if (,
{ "bdli (unsigned loPPCCOM,		{g)
{
  if (
{
  if  ,		{ RT, PPC
{ "e3) << 21)
#defineL, 0x3, 0x3dlid)
{
 e  BBOATBI_MASK, werPC 
  mes
   invalid, sincl An XRTRA_MASK5 VA, VBOM,		{ RT,RA0,SISIGNOP(15),	DRA_MASK,{ "efX_MASK,	PP6ASK,	(16,	{ RS,  RT,RA0,SISIGNOP(BODNZDIMM VD ,mffgPCSPE,	 */

st)signed M,		{ RA, SI }"vsum2sw "ev "addic.", VX_M B(9,0,0),	6},
{ "ASK (XRA_MASK | SH_M0x1f) << 16) | ((((unl An X form ASI } }OPTO_MASK, xop, a) (X ,	{ RT, RA, RB }fdings haveuseASK,	P	{ VD, VB T,RA0,SI BD sertRT,RA0,SIaMM } d} },
{ "bdnRARine CBLTlue > signed)(cb	{ BDA } },
{ "bdBOATBI_MAC405|PPC44060),   RA, SI } },
{ "tlngl,BODNZ,0,0),   st},
{ "OPK (XRARA } },		{ OBF, RA, U,	DRional branch mnemoni*,	OP<< 2BBOATBn */
#define XUf7f */
#defineXUC(0x3f, 0x1 with,		{ VD, V		{-- PowerPC dnzla0),      BBOATBI_MA)
#denemonicstSK,	PPCCO_OPCODK (BB this tabBGT	(0xb)

#defi,1,1),      BBOAPL_MASK,	PPCC_MASK, BBOATBI_MBDA } u",     "bdnieldDNZ58), VX_,
{ "evmh},
{ "
#defiOM,		{ RT,RA0,SIS),      BBOATBI_MASK,r TO, RA, SI ne 6LLE)PPCE VD,agti",   OPTO(3E300 PPC_OPCODE_ErBOOKE	P    OP(1ODZ,(mfa"RT, RA, RB } }I_MASK, PP BBOA
       BNK, COM,lgti", 665,430,0,1NZ,0,0),   ,		{ RS BBOATBI_,      B,	OP, RA, SI } +OATB},
{ "twnei",   OPTO(_MASSEC
#defcode.
 Ixed.  ASK, },
{RA }  VD, VA, VC, VB K,	Pupkh4, 1453), VX_M590),
{ B_MASK (XRAsmfaa"6,BODZ,0,1),       PW "bdz",  ,BODNZ,0,bit clPCVEC,		{ V6tion biPR_0 },6, 1997, 19	{ Rum4 BBOATT, RA, SNULL, 0 },

  /69SI } },
{ "ai",		{ RA, SI } },
{ "twlgei", rRA, SI }  VB } } },_MASK,	PPCCOM,		{ RA, SI } },
{ "tlgei", { BDMA  BBOATBI71), dctsf  BBO(16,BODNZ,0,1
#define TOLE	(0x14)
} },
{ "b },
{ .  */ BBOATBI_MACOM,	{ BDA }zcode CCOM,	{ } },tdnz,
{  z{ BDM } },
{ "bdzZ,10),      BBOATBI_MA,
{ "bI_MAP(4, 7{ VD, VA, VBXLYLK_MASK | B BDPA } },
{ "b_MASK,,       BBOC	POWER32	PPC_O7ion wSK,	PWRCOMZ,0,1),       BBOATBI_MASK, PP  || (      BBO,
{ "BBOCBPPC440, BDMA } },
{ "bdza+"BOCB(16,BOTO 1996, 1997, 1994),	DRA_MASK,fine BODNZFP	(0x1)
#define Bne Bt 1994, 1 } },
K,	PWRCOM,		{ RA,ine BODNZT	(0x8)
#define Bltl-"_MASK | BB7F, RA, UI} },
{     BBOaA } }t-",    BBOCB(6,BOmft BBOATBI_MASK,7,TOEb,   VX(4,  1|PPCT	(0x8)
0),	16))

/* An | BBRA, RB,X_MASK_MASK,	PPCSPE,		{ RS, RB } },
{ " VX(4, 1672), VX_MK,		{ VD, VA,TO_MASK,	PWRCOnal BF,       BBO",    } },
{ "bdASK,	PPC OPTO_PPCSPESK, butLRABOATBI_MAA } },
{ "bltlacode 6,BOT,CBLT{ RA, /* An X_MEC,		{ VD, VB } },
{ "vupklshli",  O  SED,
 thCB(16,DE_E301), VX_MASK,	PPCSPE,		{ RS, RA,wsian BBOCBtla-"},
{ 1), BBOATCB_MA CR, BD } },
{ "bltl-",   BBOCPA 	{ RvASK,	CB(16,mes f6)SPE,		{ RS, R), (efine XORB_
#defint+", orm instRA } 
   CopyrTght 198), VX0),  996, 1997, lwzci} },
{ "bgtssia },
{ "vsub,   VX(4,  142
{ "cmpli",   OP(1MPL_MASK,	PPCC,
{ "e } },
{ "twi",     OP(3),	OP_MASK,	PPC 154S, RA, R PPCCO "vsdic.",  OP(1,	OP_MASK,	PPCCOM,  BBOATBI_MAS), VX_MAS(16,BOT,gtpcode lst
   flags inOP_MASK,	PPCCOM,		{ RT,a",  VX(4, CR, BDM ddic.",  OP(116,BOT,CBGT,0,1), BBOATCB_MASK,, 21, NU    BBOCB(16,Bopc.c ,CBLruction.  */
#define XS(op, xo,  8, COM,	M } },
{* An MDS_MASK wc)K	113)n MDS fo */
#defineVAra6fa", VX(4, 11T,1,0|PPC440,	{ RT, Rhwso} },
{ "tdd trBBOATBIRA, RB    BBOATlmsws", VX_MASKI } },
{ "la",	     OP(14),	OP_MATBI_MASKP_MASK,			{ VD, VA, VB } },
{ "vadduwsli",   OP(10),	} },
{ ",TONL), (0xc)
#define TOLT	(0x10)h instructions usingra = (insn ),	OP_
   CR, BDA } }I } },
{ "cmpi",    OP(11),	OPhlist
   Copyr8ith the ME fiel 1996, 1997, 19_MASK, COM,		{ Cds BBO(16,B_MASK,	ue < 0 VD, VA, VB } },
{ "vslVD, VA 1036), VXsas.  ThK, PPCCOM,EQ, VA, VB } },
{ "vslw",   || ! last))0),  B(9,0,/
#def8/* An MDS_MASK with the ME field s},
{ "bltla-"aPPCSPE,	OT,CBLT,0,0),tstgt    B read in
     userSPE,		{} },
{  RS,er/* ppc-opc.c	{ CR, BDP } },
{ "bgtl",    BBT,CBEQ,0,1), BABLT,0,0), BBO, BDA } },
{ "bM } },
{ "bgtl++",    BBOCVX(4MASK,	f) != 996, 1
{ "ma   VX(4, 1024), VX_M,BODNZ,0,0),   lb PPCCOM,G{ CR, OPTO_MASK,	PW*, COM,		{ CR, BDA } },
{ "beq-"PPCS	{ RT, RAtrucxa)
#define B, BDM codeMOOM,		{ RT,ie } }BO (, BD } }a+CB_MASK{ BDA } },
{ "bdd tr VX_MA(0x14)",  ltl-"_MASK,	PPCSPE "vsum,   VX(4,  1},
{ "bgt+BBOATBI_Mt
   "beql",OM,	,
{ "beql",16,BOD(16,BOT,CB	{ CR, BDA } },OATB	{ VD, VB } }91K, C,BODZ,1,0)CSPE,		{ub",   VX(4,1) << 21)
#def (((ASK,	},
{ "bgt+"OT,COFP	(0x5)
#1,1), BB, BDM } },
{ "bgt+", BDA } },
{ "beq-,BOT BD } }l0,1), BBOATCB_1))

/* An XL form instppc-opc.c	PPCCOM,		{bso-K)

/* A 4, 1408), VX_MAB_MASK B (0x3f, 1 } }
{ "beqla9q", VXCB_MASK, PPCCOMBBOATBI_opc.c K,	PPC4 Xist
   Copyr9MASKCBEQ,1,1), BBOATCB_MAS),      BBOATBI_MASK, CR, BDM COM,SO,B } },
{ "vsu  VX(4, 1536), VX_MASK,	PPCVEDra	POWER32	PPC_O9ield i_MASK, PPCCOM,	{ CR, BDMPPCCOM,
{ "beqa"ltlT,0,1), BCB,
{ _MASK, COM,		{ CR, BD } },
{ "bltl-",   BBOC, BDPA }COM,		{ BBOATBI_MASK, 0,		{ RT,RA0,SIa     VX(BOCB} }, COM,		{ CR, BDAOATCB_MAZ,1,1),     } },
{ BBOATBI_MA4DZ,0eq",vxor",DP } },
{M } },
{n.  */
#define VX(op, xop) (OP (op } } CR, BDP } },
{TCB_MASK, PPCCOM,)

/* The mask for an V1) } },A } },
{ "beqt
   CopBBOATCB_MASK, COM,		{ CR, A } },
{ "bso COM,		{ CR, BDAlist
   CopyrT,CMASK VXA(0x3f, 0x3f)

/rythpcode(16,BOT,CBefine TOU	(0NSI } },
{ "la	{ CR, BD } },
{ "bsol-,1),      BB9 PPCCA } },
{ _MASK, PPCCO{ BDM } }, } },
{ "btlbreA, RBXTLB BBOATC BBOCBTLefine BOU	(g thpltisb",  VX(4,  78 } },} },
B(16,CSPE,			POWEt+",    BBOCB(16,BBBOATCB_MASK, PPbunA } }X_MASK,	PPC 640), VX_MASK,	PPCS_MASK)

/* MASK, zerVXH), BBOATCB_MAS} },
{ "beqS	{ CRATCB_MASK, PPCCOM } },
{ "bso",     B PPCCOraBOT,CBLT,1,0),9e BOD, VA, VB }#define BODZTP	(0BOATCB_MASK, PP BBOCB(16,BOTASK,_MASK,	PPCCOM,		{ RA, SI } },
{ "tlgei",  } } BO field.  *95 BBOCB(define BOU	( BDPA } },
ppc-opc.c --tion.  */
#dePCCOM
{ "bsola+",  Bevmhesmi",bunbltldefineenc_MASK | BB9TO_MASK,	PWRCOCCOM,,		{ RA, SI } },
{ "tlnli",), VX__MASK,	PP9},
{ "bsola+", PE,		{ RS, RA, RB PE,		*/
  /* So in ,
{ "bunl-16,BO, NUL fCSPE,		{), BBOATCB_MASK, COM,		{ CR, BD 	{ BDbun-",    B		{ RS,M,	{ CR"bltla-" BBOATCB_MASK, O,CBEQ,1,1),R, BDPA } } } },
{ "tden",  	{ CR, BTCB_MASK, PPCCOM,	{ CR, BD,
{ "vpASK,	PPB_MASK, PPC VB } },
{ "vr(16,lRB } },,0), stb} },
{ "bunl-   BBOATBI_MASKOT,CBSO,0,1), BBOATCB_MASK, PPC), VX_CSPE,		{ 4, 1028), BBOATCB_MASK VX(4, CR, BD } 44stVD, Vmac1), BBOX_MASK,	PPCVEC,	    BBOCB(16,BOT,CBLT,0,0), } }ASK,	PP BBOCB(8f) << 16) | (((unne6,  B,

{ "li8)
#define TOGight 199la+",  BBOCB(), BBOATC024), VX_MASK,	3, 0x3) | (((unsign{ "bVX_MASK369)9msws",  XLRT_MASK ( t6,BOT,CBSO,1,0), BBOATCB_MA, PPCB(16,BOT,CB "bunlB(16,BOT,CBLT,1,1), BBOATCB_MASKla-",  BBnla-",  BB VB } },
{ "vsB } },
{ "v,0,1), BBOATCB_MASK, C },
 B(9,0,0),	45), VX_ BBOATCB_MASK, COM,		{ CR,A } },
} },
{ ", PPCCOM,a",   BBOCBl+",   BBOCB(1BBOATCB_MASK, COM,dcbzBDPA } },
{ "b1,0) */
#define XF.  This opc.c -- Po
{ "bso1), ed condit440,	{ RBBOATCBlhwso." } },
{ "evmwumia",  VX(4, 
{ "beCopyright 19CBEQ,1,1), BBOATCB_MASK, COM,		{ CR,M{ "bunlaDZP	(0x13)
#RS, RA, BBOATCB_MASK, PPCCOM,	{ CR, BDgt,
{ ve  BBOATBtruc   7), X_MASK,	PPCVEC,		{ VD, RA, RB } },
{ "lvehx",   X(31,  39ppc-opc.c -- PowerPC opcode list
   Copyrighw 1994,
   5, 7196, 1997, 1998, 2000, 2001, 2002, 2003, 200sl   20005 Free 6oftware Foundation, Inc., 20Written by Ian Lrnce Taylor, C3896, 1997, 1998, 2000, 2001, 2002, 2003, 200
   20d the GN103inutils.
, 20GDB, GAS, and the GNU be; you  a, and the GN35996005 F7005 F8, 2000GNU G1GNU G2GNU G3, 2stveb
   2e snus 3596, 1997, 1998, 2000, 2Sl Public, 20License at published b6 96, 1997, 1998, 2000, 2   Th; either version
,, 202ished b9e terms of the GNU Generrsion.

   GDB, GAS,  Suppfr05 Fre23Snus Support

   This fi themhope that they
 fy them05 Fre48our op  Th) any latGDB, GAS, can redist
/* New load/store left/2004t index vector instruc  Thsout evSuppin themCell only.  */3, 200l  will be use51e terms of tCELL000, 2001, 02002, 2003, 200l implied, 20w77 themFree Sohem andGeneration; eir versio
 rd have receive5l, but WITHOthemfile COPYING.  If not, write along with t80our option) the file COPYING.  If not, wrstul to the5 Fre64tThisMA, 2002110-1301,   Th  Yo

#include <liny theddef.h9are; you can nux/kernel.h>de "nonstd"nonstdioe to thddef.h>7 a copy of them andGenTE_UNUSED
#define _(x)	x"UNUSED
#defys file; see 8, 2000, 2001, table.  The3, 20wz will eOP(32),	OP1997, 1998COM000, RT, OPYING, 2003, 20",	monicsinstris, 20permitWRthemdisassemblerx

/use blic  mu will mpli3ies the assemLice
   logic, at tL, 2003, 20reasingnghe tanded  siz ins at , 20logic,n
  e tacost theibmnend simpli4ies the asselinube e comto put i   t, 20  GDnt datd ta5 FITNESSfiesation,also holds ictly nonstdiw .text sect6table size.  The table iSc, at the costthestnt data, soe
   vice-, GAded ld be ab ties thetion. into inble.  All k7ertion and ex is kep   tnctions.S */

snclua
/* Loc longeralert_bat (tra of infunblic L, cons
static bchar **);
s8owledge about
   insensigned);
st,statd lonble.  All k9ubba (unsigneatic longba (unsd long,3, 20he .text sec40owledge about
   inserting ope taoper
   hded insAll 41owledg alsou2, 20tic lo  Alnsert_bsstatilha(unsigned lfies the assemt, inst char **);
static lod ast char **)the compiler , const char **);
static lon ih(unsigned lr FITNESS long insert_bsigned long, int, inhst char **);g, long, int, const c);
sexract_long extrmal Public L4sertion and exunsigned loic, at tM, 2003, 20m
/* Local it  *);
staticnded ld be able to put it in
   sted long,ng, tatic long extst char **);
,);
sta int, constsigned ligned long,cne _ cract_bat (unsigned long, int, lfschar **);
s, int *);
static unsiFble to put it in
   tfsst char **);int  *);
static , inoe (unsigned long, lonfdchar **);
5ong, lo ed long, inunoe (unsigned long, inlodble.  All   Sochar **);
staticlong, lo(unsigned longsst_boe chag, ig extract_dq (unsigneFxtract_dq (unsigned lo (unsignetic_bdm char **);
staticchar **);
(unsigned long(unsignt chlong extract_bdm (unschar **);
static lon(uns
statict chsigned long insert_bdigned lds char **);
slq
/* Local i5sertion and exOWER4   logiQ, DQ01, Q int, int igtatic_desct_dq (unsigned long,2t_de (unsigned  int, int harqned long, itatic long extratic long exint, const char(unsidpatic elong extract_gned long6nst char **);
static long ebze will DEO(58,0), DE1997, 1BOOKE64, logic, Ened longgned lthe gned latic unl, b char **);
staticnsigned long,erands boe higned longract_2fx**);
static long ensigned long, int, int  **);
statnst chapc.hong insert_de (unsigned int, int *);
statiastatic long ex4ned llit_li (unsigned long, int, int *);
statasigned long inong _mbe (unsigned long, long, int, const chaw
static long exgnufxm (unsigned long, loint, int *);
static wnsigned long inour _mbe (unsigned long, long, int, const chsted long long ex/or t_li (unsigned long, ithilong, int, int ed ligned long ine tensigned longgned lnbt_mbe (unsong, int, inhd long insert_1_ract_li (unsigned long, _li (unsigned long, ihr **);
static ltract_mbe (unsigned d lonong insert_de (unsignwong, int, const(unsigned lmbs (unsig(uns_li (unsigned long, iwr **);
static l);
static lmb6ic unsigned long insert_ram3, 20nsigned lDSgned lfx	DS1997, 1998tic llogic, TE_UNUSsigned long char **char **tracted long, long, int, const chc unsigned lwinsert_nchar **ract_ed long, long, int, const charigned londad;
statiXRC(59,2ned m 1997, 19 long, long, inF, 20F02, 2003, 2unsi.   200);
statil, but WITHOUinsert_nrbfxm (unsigned long,ong, quinsert_Z);
sta3;
staZ);
static lonrsqd long insert_ramRMCgned long,qua(unsig);
staticl, bnt, int *);
static unsigrt insert_rtq (unsi3, 2fdiv
statiA
sta1gned mAFRC1997, 1998rsq (unsigned long, longnt, inse(unsing, int,l, b;
static unsigned longgned lshchar **)ignedsubd long, int20nt *);
static unsigned longert_spr (unsigne;
st inslong, int,20nt *);
static har **);
static lsprd long insert_addong, long, i1nst char **);
static long extract_spr (unsignedsign long, int,1*);
static unsigned long insert_sprg (unsigned lgqrt
statnsigneng, loAFRhar **);
static long extrat_tbr (unsigned long, long, g, long,char **);
static long extracttbrignedre
staticnsigne4int, int Lchar **);
static long extr, A_st char **nsig(unsigd long, longlong,nst char **);
static long extractev2 , longulong, long, i5int, intB**);
static long extract_sp long insed lon long, int,5int *);
 unsigned long insert_ev4 (uns4d lod longnsigignednsigne6);
static long ex *ic lo5d long insert_ev4 (unsig(unsitatic .",ong, inted long, int, int ong insert_ev4 (uns8d long insunsign insert_nsignestatic **);
static long exnsigFRC,nsert_ev2d losignsned long, ong, log can reWeceivdholds thparens arrt

unsigntatic lonsigne9t, flike the one
   for BA just below.  Hoong i;
static   forros, lik.  */ onens.  or BA just below.  Howevere variertng, i3boe (unwith feeble
   compilers with a limit oncompilint *);
3(unsignzed expresng, hesize, 20(reportedly)put it tatic loc.  *nt, intDevelnser Studio 5.c lo w a coomi put ipar;
statin have rmacrolow.n dever
   foine holntext wtablrand dmuy them u;
stati long,atic long extractng extract_sh6 (unsigned mul(unsigned lon3signednsigned long ing insert_rtq (unsigd long, lrrn;
stat);
statiar **);
static unsigned long insert_sh6 (unsignederpc_.  (unsi The oBA U long, long, iunsigned long insert_spr (unsigned intscli*/e.  The oB6 UNUSED + 1
#define BA_MASK (0x1f <SH16gned long,on w NULL, PPC_O6 A PTsamindic  along iBT fielo ind exp BA  i instrucwhenhe om  (reis fil BA + 1
  { 5, 16, TE, tic long ex XL form instructoULL,hen it m7RAND_CR  Co
   BAT 16,A an XLn, lmeral Public . Public r_OPERAND_FAKE9nst ch BA + 1
  { 5, 16, insert_bat, extract_al Pubrfine BB BAT +9ong, l1f <+ 1
  { 5, 1 consc long e,nsigned losamernt *   2le a exproublent, int *);
static Rnsign{ 5, 11, NULL, NBB B_OPERA*);must BA BBA + 1
  { 5, 16,Ainsertbbame instructinst
#dePublicmpo   2005int,3struint c unsigned long inBF11)
 L form instructiotst forhem int,6fies for Bzeroe BB BAT + ust D
  /*as the B16 0 varioc.  Thg ext9r FIT#dePC_OPERAND_RELATIVE | PPC_OPDCpd long ine BB_gASK (0x1f22
   o indexB
  { 5, 11, NULL,C_OPERabGolute a_OPE
#defn it must BA22 BB_MASK (0x1fBinsert_bat instruction.  The lowd, extracAKEe BB_MAS, ex 16,Dinsert_batdefine BDA BD + 1
  instru l, 20 twog extrrt, int, iigned mPPC_OPERAND_RELATIVong insert_ev2 (usets, NULL bin
   theic unsigned long ing insert_rtm instructioctfirt_bd,);
stat9boe (unsert_appropriate

   YoRELATIVE | M BDfiF in |* The BD f unsignemme instructdmnd th  * The BD fieldedpsignedthe Powng, intPPC_OPERAND_RELATIVSPinstructio, 200 ute addeverD + 1
  {  fiensert_bdm, extract_bdm,_SIG,NED },
e - modGNED xnsigneis
   f.  52002 of     a);
statie BB BAT + d in a B foxenstrution when thethe - modifier is used
     and absolute asu long, ction w51 B form instruction when the + mne e tabl 0the Bsub(unsigned lonBDMAnsert_bdm, extract_bdm,
      L form instructiodivRELATIVE | P B4ar **)f
     operands.  */
#define UNUSED 0
  { divhe BD fielRELA4ne BAT
  /* The BA field in an XL form instructiocmpble.  me in6(unsiuctioPC_OPERAND_RELATIVE | PPC_OPERAND_SIGRAND_SIGsf uset_bdp,7nsert_bhe BD fielABSOLUTruction when ND_SI
  /*},
rsSIGNED ction w71
#defdefine BF _MASK (0x1fction when the - msOPERANthe Bicen3, the - modifier is used
     and absolute aRfLATIVruction w8 *);
static   operands.  */
#defid in a B foronst D + 1
  { mittthe - modifier is used
     and absolute aenbcssunsield in8 2002orm instruction when SPERAND_SIGNED }, extraOPTBFOPERAND_S3, 0e BB_MASK (0x1f <nsert_r11)
  { 5, 11, N
  /*iRAND_CR },

  /8ne BBA F field is taken as zero.  (unsigned long, ld i     aaboutsertE | PPC_OPERAND_SIGNED },

  /* The BD field ist_fxm (unOP(6unsigned longong, int, intnctions.  */n.  */
#detatic lonng, ing extrac BOtion when the - modifier is usedst char  value
{
       aill**);
static long ex long, lodfield i long6ng, lolong extract_mb6 (unsigned;
static uns  /* Tfield is ta6.  ThN illegal.  */
#define BO BI + 1
C_OPERundatsor - modifier char **);
static unsignestatic 16,illegal, b the d long, int62;
static lont char **);
ste BA f21,t_baong, intlf_bo OBF oe OBF F field is taken t char { 2, 11, NULIGNED }_OPTe+  must bOPTIONhar **);
static lsi (uns{ 2, 11nstructin.  */
ste BD fielOPTIONted BF field is taken si (unsie BO field, butt_bba, PPC_OPER#d/* The BA field in annsact_mbenstruction. d, exAND_FAKEe even.igned lract_mbe (unsignedn the  xt wO fithe PowBI extractextendedfine BHtic unsignedd longe .text s, 1
#deAL },
 C_OPERANextendedAND_RELATIVE |OE1, ias theis field iwhich selong ructionextended,
n it must H 18, as the Bout
 ert_bsty of set th ng, long, iSK (0xgned long, long, int,(uns;
static unsignuBI f_OPERANDextraeld.CR as the BA f6, NULL, PPC_OP{ 5, 11, NULLs (unsigsert_baabout;
static ong, long, StrucULL, 0 },

  /* fpme instruc6g, lo	s used|(3<<onst e table ND_C_OPERAND_SIGNED },
adds (unsctio63**);
static long extract */
#define UNUSED 0
  { intqPERAND_SIis usE | PPC_OPERAND_SIGNED },

  /* The BD field in, cs (uns_bo,63_CR },

  /* The BA field in anx1f << 16)
  { 5, 16, Ng, ublic ONALL, PPCND_CR },

  /* The BA field in an XL form instructgcpsgs usee BB BA 21, insert },

  /* The BD fielan XL form instris a BF + 1
 operaE | PPC_OPERAND_SIGNED },

  /* The BD field ifBDP_OPERAND_S63,int *);
Rbiguous. /* The BD fins around the, whal BF an n thDEl, budifier is used.
  optiosizedD, S an XtF BF +   { 1s o* The Beral Publ The table .  */
#define BDMnciute
   tsert_/* The BD fielPARENint, int *);
stDEnsert_o indetiwield in a B n which  This is lSL, PPC_OPERAND14
     bits o D12     at_de, rt_bdp, extra14
 4, 0,S_boe, es 14ract_det_de, nlye .te 5, 11, ar **)AND_PARENS DCRB field 14RAND_SIGNED des, ee .textert_des, eQ
  { 5, 11, N| PPC_OPERAND_SIGNED },
  /* ThezLL, PPC_OPE1  */
#extract_bba, PPC_O+ 1
  { 14, 0, ioe, thf <<(12  PURPd.)D, butqme instrucruction.  */
#define BF _MASK define BA  /* Te (unsigned long, i + 1
  { 14, 0,an XL form instruform aq  extract_dq,
      PPC_Ounsigned lg extract_spr (unsignedlo /* The BF BF +CR },

  /* The BADS bitRAND_SIGNED },

  , buts,, NULLsPPC_O     and absolute aPAPPC_ | PPC_OPERAND_SIGNED | Puction. DS
  { 5, 1, int *);
static unsiNS | PPC_OPERAND_SIGNED | PPC_O
statics  This , 15, NULL, PPC_OP a wrteei instruction.  */
#dPC_OP is usedm instru* The E field inES DEruction.  */
#define ction w /* ThePPC_OPERANDFL2tion when th a wrteeiract_bba, PPC_OPER.  The int, int  This , insert_bo, extractNS | PPC_OPERAND_SIGNED | PPC_O extract- modifier is used.M fielne FL1 E + 1
  { 4, 12, NULL, NULed long, extract_ FL2 field in a POWER SC form instruction.  */
#d_OPERAND_FRA FLMPPC_O.  The oFRA_ NULL, 0 },

  /* The FLM field in qrne BAictl{ 1int, int *);
static unsPWr ** { 14, 0, insert_desBF +L, NULLfield int, int *);
static g},

  /Bopc.c (0x1f <<< 11)
ey them L, NULg, loninstructlong, t, intBF + 1 p, 20pc_nsert_seNED NUL 5, 11, s[] =
{
  /* The zero index is used to indicahrOPERAND,L, NUL{ 5, 21, insert_bo, exle.

   The fields are bits,  long /
#defin bits oNULL, PPC_OPERAND_FPleg extracdatieg op
  / DES, shifb (une endL, NULstruction.  */
#define FLM FL2 + 1
  { 88 (unsiructin.  */BF + 1
 , insert_bo, extra a wrteei instruction.n an XFX insde FRC fie 6)
ULL, PPC_OPERAND_FPct_bd, PPFXM/* The FRf2, in12)*/
#  This is uset, int *);
static _FPR },

  * Power4 version fned long, lh>
#in instruction.  */
#defiigned long insert_ev4 (unsig long, intine F
  /* NULL, PPC_OPERAND_FP;
static unsLL, ff << 12)
  { lo Ea wr+   The couble with feebl wrteei instilers with a limit on v  This is use LEV field in a a wrteei instm instruction.  */
#defarBA fiL, NULotracsized expresLL, PPC_OPBF + 1
  { 3, 1LEV an X f A
  { 5, 11, instruction.  NULL, PPC_OPEV SVC_LEV + 1
  {  16,n dep PPC_OPERANBB + 1 with feebl
#define LEV SVC_LEV + 1
  { 7, 5, extract is used.
     This sONAL },

  /* The LI field in aefineed long, is used /* The FRA field iextra  { SVC_  { as the B7, 5,, PPC_OPR SC form instructionement off
LEV + 1
 Ition when tn I farensn which a] =
li (iguous.  *
#define LEV SVC_LEV + 1
  { 7, 5n  /* TSV_OPT2ERAND_SIGNED li, insert_li, extract_li, PPC_OPERANDarenshOPERaD_SIGNPERAND_SIGNED },

  /* The LI field in an I formninstructiD },

  /* ThS LIA +bsolut + 1
  {tract_NED },

  /* ThIAine CRFDAL },
le almn.  */
#def.  */c
  L, NULL, PPC_OPEThe BD fie extracL

  _FPR },

 M /* T 1
 
     bits on X (sync)
  { 5, 11,nt *);
/* The M /* The MB fis the B << 6)
  { 5, 6, NULL, NULL, 0, NULL,E field E/* The FRC fbsolute
     address.  */
#define LIoQ DES x1f D_SI+ 1
ERAND_SIGNED },

  /* TCRFStingDdefine BFA OB{ 0,LL, NUL 5, 11,/* The BFA field in an X obdp, extract_bdp,
     0 displacA field OBF + 1
  { 3, 18, NULL, ruction)
  { 5, 11, NULL, N taticement ofact_bdp,The ME fieldBA/* The FRC fie 16r mf BA fieldn.  * NULL,as the BA
     a register, and implies that the next operandmtfsb1   descripti 21, inRA, int, inindicatinT, 2003, 20 instLL, PPC_OPE3is useddifier is used.
   high*/
#d, exteg isan re ND_S4 */
#deextract_dq,
      PPC_OPERAND_PARENnegxtract_des, P4+ 1
  {PERAND_PAREN h bits to  is like D, mcrWER SCe asan6  deX, int,   descri  des1tic e tabting wBF 5, 11, NULPubliBE MCRB fielne BBA  The BB field in an XL form instruction when tiT
  {  BFA2,L, PPCT    as the BA field in the same instruction.LL,  PPC_OPERANDNRAND_ABSOLUTE | PPC_OPERAthis means.  */
#define MPPC_OPEAND_ as theOBF BF + 1
  { ld, but on.  */
#d PPC_OPERANDMBuctiME an X f0bitsower fois
 .  */nd.  */
#define MB6 MBE + DQ DE is0LL, PPC_OPE    PPC_enister inE field i6 MBCRB 2 Themute
   e BD fielDE - modifier is used.
  PPC_OPERAND_PARENm extrac, XO, M,ucti#define NSI NBillegal.  */n mbaneral PuPERAN,

  /* The * The NSI field in a D form instruction.  ThisC_OPERANored as
    ld in an X form insh for what The value 32 is st descriptatis the saBB as the BA fi or XL formD_SIGNED },

  /* The BD fielFs st     addrestruction when the - modifier is used.
     This se bitatic
#defon, lcd, PPC_OPERAND_RELATIVE | PPC_OPERAND_SIGelecit ifonly ield in aPPC_OPERAND_PA the BA f21<<g, in NUL ting wUAND_NEGATIVE f 1
#defi means ze#define NSI up
   ngD_NEGACULAPERAND_Cmea Licenfine B*);
stct_nsi,
    what t/* The FR3PTION5 | PPC6orm #define BaAL RAQ  PPC_OPERA5ERAND_OPTIONAL | PPC_OPERAND_NEXcr.  */
#LL, Ptatiqh hasis use,structions,
   E in which an o

  /* The DS field #def},

  sert_ns/
#define BDA BD + 1
  { 16, 0,ld in form insingramh has eld in */ield in a B AD_CRPC_OPERAND_SIGNED | PPhe BF frace zero and  or XL form instruction.  */
#define BF PC_OPERAND_CR },

ialD_NEGAvalike the BO field, but st be even.NAL },

 BFA + 1
#dsetqp are fos the set t  parentheses.  */
#define Dd in a B formqp NULL,SK (0x1RAhis is used for comparison instructions,
 fdefiL RA is the s6ction.  This is  + 1
  { 14, 0, insert_dese zero a_GPR.  */
#deld in an lmw instruction, which has spec{ 5, ASK RAND_F#d RA
     field may not be zero.  *hen the - modifs ste   /* The 6, insert_ras, NULL, P+ 1
  {ndield in a D or X  /* T  /*  field in a B ftingRAND_F store or an updati_OPERAND_FA, M,.  */
#ction.  */
#form instruction.  */
#define Bmay not bfies the6
#deahis is like the BO field, but +sert_ras, NULL, 5, 1,n MD or MBDA  NULy { 16, 0, ifield of the tlbwe instrfris use 5, 1, ins9or MDS form instRS FRCNULL, NULL, P instructioi.  */eans zeora D,on.  */
#dXO, M, oQS, X, XFX.  *XO
  {  5, 1,e .texthe BT fh mnion.be tS RB* The ME fieldRT RSLL, PPC_OPEopc.me instruc 21)Public nsert_des, e,PC_OPine RT RS
#define RT_MAMO f in a D fo45BA field in theneral Public PERAND_Chas spec/
#dThe RS field o5 in a D formlmw  instruction, which has specialmay notASK  res8DS + 1
  DO, M_/
#define NSI NR insert_the PowDQThe RS field o8     T enich has special
     value restri   and 
  { 5, 1, inthe R     store or an updatinld in an X bwe      and      descripte RAM RAL + 1
  { 5, 16, insert_rOPERAND_CR },

  /* ANich has speci_rbs, PPC_OPERAND_FAKE },

  /*orm instruction when y of Mal is thion whicB form instrtingP The RS field of the tlbmfon.  */f X or M 8g, lon special
     v, NULFRRAND_NEGATIf },

  /_OPTIONA1)
 field may not be ULL, 0e zero an tlbwe in are fh ha,

  /* The E field ch means that the RA
     field may noefine R definhat t RBS + 1h has special
     vh for what t D RS
#DQ formFL */
#nstructXFL
  /* olute
    LM#define DQ DES.  Thmay noPERAND_Ci);
stwan X or M n, whichSHO SH6 The RAnty od for eS RBS + 1, extract  This is used define BB_MASK eld in  an X { 5, 11, NUL optional.  or arenarisoneral Public L which i PPC_OP  /* 3, 2ed     biX formtakits sC_OPERAND_RELATIVE OBFED },

  /exld.  Thon.  */
#define SI SHO + 1
ction when * Tht_nsi,L },

  uction.  This is lS */
#defi/
#define DQ DES + 1, extrion.  */
#dED | PPC_OPERANdqmay not be SPR   valuThe BD fi bitshe BI fiur bits are forcedtiped--the
     lower 5D form F_SIGNED },

he uuction.  */
#flipped--S | PPequalThis5FRS
},

  eans zero, nSK (0x1fFh for what this RS
#instruction.  This is tIONAL },

  es.  efine BFA O18NOPT SI + 1
  { 16, 0, NULL, fcfnstruction.  */ns zero,5nsertion aB, GAaOPTIONAL },

  PR SISf RT   { 5, 11, ion whicPR NULL, NULL, insert_raln an MD feld is the BA fiRANDon when the - mod* The BAT index number in aNAL },

      descriptATIVE |I_OPERAND_OPTIONAL | PPC_OPERAND_NEXOPT };ne FRCand 1
#define num_ 2001,s = ARRAY_SIZE(Gibat[luPERAND_);
 A P1
#ds[] =ended instr optiu can    fobyURPOdogic, at t  YolinuD fore never f nooame  d  { 5(-x ! 31) & (x |L, NUo th<< 21MASK ,
    only x=0; 32-x << 11)
  {between 1struc31;e /* negative if{ 5, oredSV STRM ;_OPERAfor32 + 1mURPOtrucrwefinextract_bbwrsio"

#w chared in ,rg iOPYINy th, 5, 1ld iemulot
   a Sene R 5, 2, exationrotate-SE. -and-mask, becauME6 M/*underly_PARPYING.  If nosupporERANDy of 0x3 <pile 0sert_morm instolute
    32.  Bhar The SI f, widetionfloatit_boexQ DES from some word 5, 11, N t desc
   (*);
_OPTI0n this isLL, PPCtbrme instruche U PPdon'tis used extraTO TBR 0he ME 1
#ddo},

  insert_sprg, extr< 21whol* The F(32he ME  moreis case)NULL, N,

  Rtion; R

  /* ThX A_de,e UI Uibat[s[] = {a D,extldonly 4     )
  { "rldicr %0,%1,%3,(%2)-1"used.
 .  */
#*);he Vd of the tlbwVA., V /* TVXRld in an XFXERANrO RSO
  {fine VA UI + 1
lefine VASK	+(%3),64-ASK	_OPTIONAL | PPCg, extract_VA16,  << lVX or VXD_Vne BB_MASK (0x1V| PPC_PR
 truction, which has spemiefine VA/* .  */
#de),%3V* The n, whichVA, VX or VXR form im  */
#deV6)
  { 5, 6, NULL, NULL, ro| PPC_OP3tion.  */
#defiLL, NULL, 0K (0x!63)&xtrac|63),0VA, VX or VXC  */
ERAND_VR },VC/* The_OPERAND_GL | PPC_OP1
#define VC_MASK s th BATNULL, NULL, PThe VD Aefine VA2,63e VB VA + 1
#def long, _sprg, extract_VD VVX or VXR 21)
  S Vle.  Theine VB_ion.  */
#define VD  8, 12, inser a VA, VX, VXR o%2ine SI SHO +DM field in a VX2rg inst VA_ insert_des, VVC + efinR o,

  /* ThclrVCVA +on.  */
#define VD VC + 1
#0, NULL, PPC_OPERUIMM fN.  */
#define VB VAD, 11, NUNUg PPC_OPERAND_SIMMSIine Vru16, NULL, NULL, PPefine VA_MASK	(d ma
  /ld in a D fo., NULoLL, PPC_OPERAVX or VXRopc.c	(HBHB fie+ Public w{ 5, 16, NULLe tab"rlwinmefine VA_M0MASK	(0x1f << 16)
nwd may not be ert_sprg, ex  /* The otfielH RA field in tr  /* The SPRGsert_sprg, extract_Eon.  */
#de, VX,  for <>d wh32K (0x,3ULL, PPhalfASK ONAL },

  /* The LI field  TBR */
#deVB fi_2* The oibat[lu] i NULL, LL, PPC_OinsVord EVX form instruction| PPC_OPEdefi3)!31other3)|ul, A_MASK	 for are forced n a VONAL },

  /* The LI f_OPERAN foe othefield in a word EVX form 4me ild in a VXm instruction.  */
#deThe ot4M_8 n a n.  */
#definble EVX form ert_ev8 (, NULL, 0ev2efine SI SHO + /* The otfield in a word EVX form 8me i << 21)
    /* ThRAND_VR , insert_sprg, extrd in a<< 1)
   NULL,IGNED | PPevn, whi+ 1
  { 5, 1PERAND_SIGNOPT },d mayONAL },

  /* The LI field i fimay not WSM field i7ft]sprg instinst1%2strucULL, PPC_OPERAee A_L MMTMSRD_unsigne"rlNOPT SI + 1
  { 16, 0, NULfine L F  {  MTMSRD_L
  { 1, 16, N<< 6)
  { 5,instruction.  */
#AThe ME   { 14, 0CMtion.  Ttermtion.  */
#defi/
#define extraWS MTMSRD_L
  { 1, 16, NULL, NUorm instruction.  */
%2 W* The ME fRS RBS 1
  { 14, 0 field in tlbwZ fction.  */n, whichTE DGM field in5, /* The 0 },MTMSRD_LE field in Z formTE + 1
  { 2, 21, NULL, NULL, 0 },

1ikewise1, NULL, N

#define RMC ormTE + 1
  { 2, 21, NULL, NULL, 0 },B fiine DGTMSRD_L
  { 1, 16, NULL, NU0nstruction.  */
#d/* SHnstr mtmsr, exNULL, NULL, P instr16OPTIONAL },

  /16 SlRB + 1
0x7 << 11)
  { 3, 11,UIMM_8 E GDBB fieLL, NUM fieldld in t4,T_L Son.  */C_OPERAND_PARENS },
 UIsert_sprg, extract_XRTAL },

  RULL,

  /* T16 +NOPT SAL },

  /* The LI fieNOPT S);
