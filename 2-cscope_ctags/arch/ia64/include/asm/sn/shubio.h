/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2005 Silicon Graphics, Inc. All rights reserved.
 */

#ifndef _ASM_IA64_SN_SHUBIO_H
#define _ASM_IA64_SN_SHUBIO_H

#define HUB_WIDGET_ID_MAX	0xf
#define IIO_NUM_ITTES		7
#define HUB_NUM_BIG_WINDOW	(IIO_NUM_ITTES - 1)

#define		IIO_WID			0x00400000	/* Crosstalk Widget Identification */
							/* This register is also accessible from
							 * Crosstalk at address 0x0.  */
#define		IIO_WSTAT		0x00400008	/* Crosstalk Widget Status */
#define		IIO_WCR			0x00400020	/* Crosstalk Widget Control Register */
#define		IIO_ILAPR		0x00400100	/* IO Local Access Protection Register */
#define		IIO_ILAPO		0x00400108	/* IO Local Access Protection Override */
#define		IIO_IOWA		0x00400110	/* IO Outbound Widget Access */
#define		IIO_IIWA		0x00400118	/* IO Inbound Widget Access */
#define		IIO_IIDEM		0x00400120	/* IO Inbound Device Error Mask */
#define		IIO_ILCSR		0x00400128	/* IO LLP Control and Status Register */
#define		IIO_ILLR		0x00400130	/* IO LLP Log Register    */
#define		IIO_IIDSR		0x00400138	/* IO Interrupt Destination */

#define		IIO_IGFX0		0x00400140	/* IO Graphics Node-Widget Map 0 */
#define		IIO_IGFX1		0x00400148	/* IO Graphics Node-Widget Map 1 */

#define		IIO_ISCR0		0x00400150	/* IO Scratch Register 0 */
#define		IIO_ISCR1		0x00400158	/* IO Scratch Register 1 */

#define		IIO_ITTE1		0x00400160	/* IO Translation Table Entry 1 */
#define		IIO_ITTE2		0x00400168	/* IO Translation Table Entry 2 */
#define		IIO_ITTE3		0x00400170	/* IO Translation Table Entry 3 */
#define		IIO_ITTE4		0x00400178	/* IO Translation Table Entry 4 */
#define		IIO_ITTE5		0x00400180	/* IO Translation Table Entry 5 */
#define		IIO_ITTE6		0x00400188	/* IO Translation Table Entry 6 */
#define		IIO_ITTE7		0x00400190	/* IO Translation Table Entry 7 */

#define		IIO_IPRB0		0x00400198	/* IO PRB Entry 0   */
#define		IIO_IPRB8		0x004001A0	/* IO PRB Entry 8   */
#define		IIO_IPRB9		0x004001A8	/* IO PRB Entry 9   */
#define		IIO_IPRBA		0x004001B0	/* IO PRB Entry A   */
#define		IIO_IPRBB		0x004001B8	/* IO PRB Entry B   */
#define		IIO_IPRBC		0x004001C0	/* IO PRB Entry C   */
#define		IIO_IPRBD		0x004001C8	/* IO PRB Entry D   */
#define		IIO_IPRBE		0x004001D0	/* IO PRB Entry E   */
#define		IIO_IPRBF		0x004001D8	/* IO PRB Entry F   */

#define		IIO_IXCC		0x004001E0	/* IO Crosstalk Credit Count Timeout */
#define		IIO_IMEM		0x004001E8	/* IO Miscellaneous Error Mask */
#define		IIO_IXTT		0x004001F0	/* IO Crosstalk Timeout Threshold */
#define		IIO_IECLR		0x004001F8	/* IO Error Clear Register */
#define		IIO_IBCR		0x00400200	/* IO BTE Control Register */

#define		IIO_IXSM		0x00400208	/* IO Crosstalk Spurious Message */
#define		IIO_IXSS		0x00400210	/* IO Crosstalk Spurious Sideband */

#define		IIO_ILCT		0x00400218	/* IO LLP Channel Test    */

#define		IIO_IIEPH1 		0x00400220	/* IO Incoming Error Packet Header, Part 1 */
#define		IIO_IIEPH2 		0x00400228	/* IO Incoming Error Packet Header, Part 2 */

#define		IIO_ISLAPR 		0x00400230	/* IO SXB Local Access Protection Regster */
#define		IIO_ISLAPO 		0x00400238	/* IO SXB Local Access Protection Override */

#define		IIO_IWI			0x00400240	/* IO Wrapper Interrupt Register */
#define		IIO_IWEL		0x00400248	/* IO Wrapper Error Log Register */
#define		IIO_IWC			0x00400250	/* IO Wrapper Control Register */
#define		IIO_IWS			0x00400258	/* IO Wrapper Status Register */
#define		IIO_IWEIM		0x00400260	/* IO Wrapper Error Interrupt Masking Register */

#define		IIO_IPCA		0x00400300	/* IO PRB Counter Adjust */

#define		IIO_IPRTE0_A		0x00400308	/* IO PIO Read Address Table Entry 0, Part A */
#define		IIO_IPRTE1_A		0x00400310	/* IO PIO Read Address Table Entry 1, Part A */
#define		IIO_IPRTE2_A		0x00400318	/* IO PIO Read Address Table Entry 2, Part A */
#define		IIO_IPRTE3_A		0x00400320	/* IO PIO Read Address Table Entry 3, Part A */
#define		IIO_IPRTE4_A		0x00400328	/* IO PIO Read Address Table Entry 4, Part A */
#define		IIO_IPRTE5_A		0x00400330	/* IO PIO Read Address Table Entry 5, Part A */
#define		IIO_IPRTE6_A		0x00400338	/* IO PIO Read Address Table Entry 6, Part A */
#define		IIO_IPRTE7_A		0x00400340	/* IO PIO Read Address Table Entry 7, Part A */

#define		IIO_IPRTE0_B		0x00400348	/* IO PIO Read Address Table Entry 0, Part B */
#define		IIO_IPRTE1_B		0x00400350	/* IO PIO Read Address Table Entry 1, Part B */
#define		IIO_IPRTE2_B		0x00400358	/* IO PIO Read Address Table Entry 2, Part B */
#define		IIO_IPRTE3_B		0x00400360	/* IO PIO Read Address Table Entry 3, Part B */
#define		IIO_IPRTE4_B		0x00400368	/* IO PIO Read Address Table Entry 4, Part B */
#define		IIO_IPRTE5_B		0x00400370	/* IO PIO Read Address Table Entry 5, Part B */
#define		IIO_IPRTE6_B		0x00400378	/* IO PIO Read Address Table Entry 6, Part B */
#define		IIO_IPRTE7_B		0x00400380	/* IO PIO Read Address Table Entry 7, Part B */

#define		IIO_IPDR		0x00400388	/* IO PIO Deallocation Register */
#define		IIO_ICDR		0x00400390	/* IO CRB Entry Deallocation Register */
#define		IIO_IFDR		0x00400398	/* IO IOQ FIFO Depth Register */
#define		IIO_IIAP		0x004003A0	/* IO IIQ Arbitration Parameters */
#define		IIO_ICMR		0x004003A8	/* IO CRB Management Register */
#define		IIO_ICCR		0x004003B0	/* IO CRB Control Register */
#define		IIO_ICTO		0x004003B8	/* IO CRB Timeout   */
#define		IIO_ICTP		0x004003C0	/* IO CRB Timeout Prescalar */

#define		IIO_ICRB0_A		0x00400400	/* IO CRB Entry 0_A */
#define		IIO_ICRB0_B		0x00400408	/* IO CRB Entry 0_B */
#define		IIO_ICRB0_C		0x00400410	/* IO CRB Entry 0_C */
#define		IIO_ICRB0_D		0x00400418	/* IO CRB Entry 0_D */
#define		IIO_ICRB0_E		0x00400420	/* IO CRB Entry 0_E */

#define		IIO_ICRB1_A		0x00400430	/* IO CRB Entry 1_A */
#define		IIO_ICRB1_B		0x00400438	/* IO CRB Entry 1_B */
#define		IIO_ICRB1_C		0x00400440	/* IO CRB Entry 1_C */
#define		IIO_ICRB1_D		0x00400448	/* IO CRB Entry 1_D */
#define		IIO_ICRB1_E		0x00400450	/* IO CRB Entry 1_E */

#define		IIO_ICRB2_A		0x00400460	/* IO CRB Entry 2_A */
#define		IIO_ICRB2_B		0x00400468	/* IO CRB Entry 2_B */
#define		IIO_ICRB2_C		0x00400470	/* IO CRB Entry 2_C */
#define		IIO_ICRB2_D		0x00400478	/* IO CRB Entry 2_D */
#define		IIO_ICRB2_E		0x00400480	/* IO CRB Entry 2_E */

#define		IIO_ICRB3_A		0x00400490	/* IO CRB Entry 3_A */
#define		IIO_ICRB3_B		0x00400498	/* IO CRB Entry 3_B */
#define		IIO_ICRB3_C		0x004004a0	/* IO CRB Entry 3_C */
#define		IIO_ICRB3_D		0x004004a8	/* IO CRB Entry 3_D */
#define		IIO_ICRB3_E		0x004004b0	/* IO CRB Entry 3_E */

#define		IIO_ICRB4_A		0x004004c0	/* IO CRB Entry 4_A */
#define		IIO_ICRB4_B		0x004004c8	/* IO CRB Entry 4_B */
#define		IIO_ICRB4_C		0x004004d0	/* IO CRB Entry 4_C */
#define		IIO_ICRB4_D		0x004004d8	/* IO CRB Entry 4_D */
#define		IIO_ICRB4_E		0x004004e0	/* IO CRB Entry 4_E */

#define		IIO_ICRB5_A		0x004004f0	/* IO CRB Entry 5_A */
#define		IIO_ICRB5_B		0x004004f8	/* IO CRB Entry 5_B */
#define		IIO_ICRB5_C		0x00400500	/* IO CRB Entry 5_C */
#define		IIO_ICRB5_D		0x00400508	/* IO CRB Entry 5_D */
#define		IIO_ICRB5_E		0x00400510	/* IO CRB Entry 5_E */

#define		IIO_ICRB6_A		0x00400520	/* IO CRB Entry 6_A */
#define		IIO_ICRB6_B		0x00400528	/* IO CRB Entry 6_B */
#define		IIO_ICRB6_C		0x00400530	/* IO CRB Entry 6_C */
#define		IIO_ICRB6_D		0x00400538	/* IO CRB Entry 6_D */
#define		IIO_ICRB6_E		0x00400540	/* IO CRB Entry 6_E */

#define		IIO_ICRB7_A		0x00400550	/* IO CRB Entry 7_A */
#define		IIO_ICRB7_B		0x00400558	/* IO CRB Entry 7_B */
#define		IIO_ICRB7_C		0x00400560	/* IO CRB Entry 7_C */
#define		IIO_ICRB7_D		0x00400568	/* IO CRB Entry 7_D */
#define		IIO_ICRB7_E		0x00400570	/* IO CRB Entry 7_E */

#define		IIO_ICRB8_A		0x00400580	/* IO CRB Entry 8_A */
#define		IIO_ICRB8_B		0x00400588	/* IO CRB Entry 8_B */
#define		IIO_ICRB8_C		0x00400590	/* IO CRB Entry 8_C */
#define		IIO_ICRB8_D		0x00400598	/* IO CRB Entry 8_D */
#define		IIO_ICRB8_E		0x004005a0	/* IO CRB Entry 8_E */

#define		IIO_ICRB9_A		0x004005b0	/* IO CRB Entry 9_A */
#define		IIO_ICRB9_B		0x004005b8	/* IO CRB Entry 9_B */
#define		IIO_ICRB9_C		0x004005c0	/* IO CRB Entry 9_C */
#define		IIO_ICRB9_D		0x004005c8	/* IO CRB Entry 9_D */
#define		IIO_ICRB9_E		0x004005d0	/* IO CRB Entry 9_E */

#define		IIO_ICRBA_A		0x004005e0	/* IO CRB Entry A_A */
#define		IIO_ICRBA_B		0x004005e8	/* IO CRB Entry A_B */
#define		IIO_ICRBA_C		0x004005f0	/* IO CRB Entry A_C */
#define		IIO_ICRBA_D		0x004005f8	/* IO CRB Entry A_D */
#define		IIO_ICRBA_E		0x00400600	/* IO CRB Entry A_E */

#define		IIO_ICRBB_A		0x00400610	/* IO CRB Entry B_A */
#define		IIO_ICRBB_B		0x00400618	/* IO CRB Entry B_B */
#define		IIO_ICRBB_C		0x00400620	/* IO CRB Entry B_C */
#define		IIO_ICRBB_D		0x00400628	/* IO CRB Entry B_D */
#define		IIO_ICRBB_E		0x00400630	/* IO CRB Entry B_E */

#define		IIO_ICRBC_A		0x00400640	/* IO CRB Entry C_A */
#define		IIO_ICRBC_B		0x00400648	/* IO CRB Entry C_B */
#define		IIO_ICRBC_C		0x00400650	/* IO CRB Entry C_C */
#define		IIO_ICRBC_D		0x00400658	/* IO CRB Entry C_D */
#define		IIO_ICRBC_E		0x00400660	/* IO CRB Entry C_E */

#define		IIO_ICRBD_A		0x00400670	/* IO CRB Entry D_A */
#define		IIO_ICRBD_B		0x00400678	/* IO CRB Entry D_B */
#define		IIO_ICRBD_C		0x00400680	/* IO CRB Entry D_C */
#define		IIO_ICRBD_D		0x00400688	/* IO CRB Entry D_D */
#define		IIO_ICRBD_E		0x00400690	/* IO CRB Entry D_E */

#define		IIO_ICRBE_A		0x004006a0	/* IO CRB Entry E_A */
#define		IIO_ICRBE_B		0x004006a8	/* IO CRB Entry E_B */
#define		IIO_ICRBE_C		0x004006b0	/* IO CRB Entry E_C */
#define		IIO_ICRBE_D		0x004006b8	/* IO CRB Entry E_D */
#define		IIO_ICRBE_E		0x004006c0	/* IO CRB Entry E_E */

#define		IIO_ICSML		0x00400700	/* IO CRB Spurious Message Low */
#define		IIO_ICSMM		0x00400708	/* IO CRB Spurious Message Middle */
#define		IIO_ICSMH		0x00400710	/* IO CRB Spurious Message High */

#define		IIO_IDBSS		0x00400718	/* IO Debug Submenu Select */

#define		IIO_IBLS0		0x00410000	/* IO BTE Length Status 0 */
#define		IIO_IBSA0		0x00410008	/* IO BTE Source Address 0 */
#define		IIO_IBDA0		0x00410010	/* IO BTE Destination Address 0 */
#define		IIO_IBCT0		0x00410018	/* IO BTE Control Terminate 0 */
#define		IIO_IBNA0		0x00410020	/* IO BTE Notification Address 0 */
#define		IIO_IBIA0		0x00410028	/* IO BTE Interrupt Address 0 */
#define		IIO_IBLS1		0x00420000	/* IO BTE Length Status 1 */
#define		IIO_IBSA1		0x00420008	/* IO BTE Source Address 1 */
#define		IIO_IBDA1		0x00420010	/* IO BTE Destination Address 1 */
#define		IIO_IBCT1		0x00420018	/* IO BTE Control Terminate 1 */
#define		IIO_IBNA1		0x00420020	/* IO BTE Notification Address 1 */
#define		IIO_IBIA1		0x00420028	/* IO BTE Interrupt Address 1 */

#define		IIO_IPCR		0x00430000	/* IO Performance Control */
#define		IIO_IPPR		0x00430008	/* IO Performance Profiling */

/************************************************************************
 *									*
 * Description:  This register echoes some information from the         *
 * LB_REV_ID register. It is available through Crosstalk as described   *
 * above. The REV_NUM and MFG_NUM fields receive their values from      *
 * the REVISION and MANUFACTURER fields in the LB_REV_ID register.      *
 * The PART_NUM field's value is the Crosstalk device ID number that    *
 * Steve Miller assigned to the SHub chip.                              *
 *									*
 ************************************************************************/

typedef union ii_wid_u {
	u64 ii_wid_regval;
	struct {
		u64 w_rsvd_1:1;
		u64 w_mfg_num:11;
		u64 w_part_num:16;
		u64 w_rev_num:4;
		u64 w_rsvd:32;
	} ii_wid_fld_s;
} ii_wid_u_t;

/************************************************************************
 *									*
 *  The fields in this register are set upon detection of an error      *
 * and cleared by various mechanisms, as explained in the               *
 * description.                                                         *
 *									*
 ************************************************************************/

typedef union ii_wstat_u {
	u64 ii_wstat_regval;
	struct {
		u64 w_pending:4;
		u64 w_xt_crd_to:1;
		u64 w_xt_tail_to:1;
		u64 w_rsvd_3:3;
		u64 w_tx_mx_rty:1;
		u64 w_rsvd_2:6;
		u64 w_llp_tx_cnt:8;
		u64 w_rsvd_1:8;
		u64 w_crazy:1;
		u64 w_rsvd:31;
	} ii_wstat_fld_s;
} ii_wstat_u_t;

/************************************************************************
 *									*
 * Description:  This is a read-write enabled register. It controls     *
 * various aspects of the Crosstalk flow control.                       *
 *									*
 ************************************************************************/

typedef union ii_wcr_u {
	u64 ii_wcr_regval;
	struct {
		u64 w_wid:4;
		u64 w_tag:1;
		u64 w_rsvd_1:8;
		u64 w_dst_crd:3;
		u64 w_f_bad_pkt:1;
		u64 w_dir_con:1;
		u64 w_e_thresh:5;
		u64 w_rsvd:41;
	} ii_wcr_fld_s;
} ii_wcr_u_t;

/************************************************************************
 *									*
 * Description:  This register's value is a bit vector that guards      *
 * access to local registers within the II as well as to external       *
 * Crosstalk widgets. Each bit in the register corresponds to a         *
 * particular region in the system; a region consists of one, two or    *
 * four nodes (depending on the value of the REGION_SIZE field in the   *
 * LB_REV_ID register, which is documented in Section 8.3.1.1). The     *
 * protection provided by this register applies to PIO read             *
 * operations as well as PIO write operations. The II will perform a    *
 * PIO read or write request only if the bit for the requestor's        *
 * region is set; otherwise, the II will not perform the requested      *
 * operation and will return an error response. When a PIO read or      *
 * write request targets an external Crosstalk widget, then not only    *
 * must the bit for the requestor's region be set in the ILAPR, but     *
 * also the target widget's bit in the IOWA register must be set in     *
 * order for the II to perform the requested operation; otherwise,      *
 * the II will return an error response. Hence, the protection          *
 * provided by the IOWA register supplements the protection provided    *
 * by the ILAPR for requests that target external Crosstalk widgets.    *
 * This register itself can be accessed only by the nodes whose         *
 * region ID bits are enabled in this same register. It can also be     *
 * accessed through the IAlias space by the local processors.           *
 * The reset value of this register allows access by all nodes.         *
 *									*
 ************************************************************************/

typedef union ii_ilapr_u {
	u64 ii_ilapr_regval;
	struct {
		u64 i_region:64;
	} ii_ilapr_fld_s;
} ii_ilapr_u_t;

/************************************************************************
 *									*
 * Description:  A write to this register of the 64-bit value           *
 * "SGIrules" in ASCII, will cause the bit in the ILAPR register        *
 * corresponding to the region of the requestor to be set (allow        *
 * access). A write of any other value will be ignored. Access          *
 * protection for this register is "SGIrules".                          *
 * This register can also be accessed through the IAlias space.         *
 * However, this access will not change the access permissions in the   *
 * ILAPR.                                                               *
 *									*
 ************************************************************************/

typedef union ii_ilapo_u {
	u64 ii_ilapo_regval;
	struct {
		u64 i_io_ovrride:64;
	} ii_ilapo_fld_s;
} ii_ilapo_u_t;

/************************************************************************
 *									*
 *  This register qualifies all the PIO and Graphics writes launched    *
 * from the SHUB towards a widget.                                      *
 *									*
 ************************************************************************/

typedef union ii_iowa_u {
	u64 ii_iowa_regval;
	struct {
		u64 i_w0_oac:1;
		u64 i_rsvd_1:7;
		u64 i_wx_oac:8;
		u64 i_rsvd:48;
	} ii_iowa_fld_s;
} ii_iowa_u_t;

/************************************************************************
 *									*
 * Description:  This register qualifies all the requests launched      *
 * from a widget towards the Shub. This register is intended to be      *
 * used by software in case of misbehaving widgets.                     *
 *									*
 *									*
 ************************************************************************/

typedef union ii_iiwa_u {
	u64 ii_iiwa_regval;
	struct {
		u64 i_w0_iac:1;
		u64 i_rsvd_1:7;
		u64 i_wx_iac:8;
		u64 i_rsvd:48;
	} ii_iiwa_fld_s;
} ii_iiwa_u_t;

/************************************************************************
 *									*
 * Description:  This register qualifies all the operations launched    *
 * from a widget towards the SHub. It allows individual access          *
 * control for up to 8 devices per widget. A device refers to           *
 * individual DMA master hosted by a widget.                            *
 * The bits in each field of this register are cleared by the Shub      *
 * upon detection of an error which requires the device to be           *
 * disabled. These fields assume that 0=TNUM=7 (i.e., Bridge-centric    *
 * Crosstalk). Whether or not a device has access rights to this        *
 * Shub is determined by an AND of the device enable bit in the         *
 * appropriate field of this register and the corresponding bit in      *
 * the Wx_IAC field (for the widget which this device belongs to).      *
 * The bits in this field are set by writing a 1 to them. Incoming      *
 * replies from Crosstalk are not subject to this access control        *
 * mechanism.                                                           *
 *									*
 ************************************************************************/

typedef union ii_iidem_u {
	u64 ii_iidem_regval;
	struct {
		u64 i_w8_dxs:8;
		u64 i_w9_dxs:8;
		u64 i_wa_dxs:8;
		u64 i_wb_dxs:8;
		u64 i_wc_dxs:8;
		u64 i_wd_dxs:8;
		u64 i_we_dxs:8;
		u64 i_wf_dxs:8;
	} ii_iidem_fld_s;
} ii_iidem_u_t;

/************************************************************************
 *									*
 *  This register contains the various programmable fields necessary    *
 * for controlling and observing the LLP signals.                       *
 *									*
 ************************************************************************/

typedef union ii_ilcsr_u {
	u64 ii_ilcsr_regval;
	struct {
		u64 i_nullto:6;
		u64 i_rsvd_4:2;
		u64 i_wrmrst:1;
		u64 i_rsvd_3:1;
		u64 i_llp_en:1;
		u64 i_bm8:1;
		u64 i_llp_stat:2;
		u64 i_remote_power:1;
		u64 i_rsvd_2:1;
		u64 i_maxrtry:10;
		u64 i_d_avail_sel:2;
		u64 i_rsvd_1:4;
		u64 i_maxbrst:10;
		u64 i_rsvd:22;

	} ii_ilcsr_fld_s;
} ii_ilcsr_u_t;

/************************************************************************
 *									*
 *  This is simply a status registers that monitors the LLP error       *
 * rate.								*
 *									*
 ************************************************************************/

typedef union ii_illr_u {
	u64 ii_illr_regval;
	struct {
		u64 i_sn_cnt:16;
		u64 i_cb_cnt:16;
		u64 i_rsvd:32;
	} ii_illr_fld_s;
} ii_illr_u_t;

/************************************************************************
 *									*
 * Description:  All II-detected non-BTE error interrupts are           *
 * specified via this register.                                         *
 * NOTE: The PI interrupt register address is hardcoded in the II. If   *
 * PI_ID==0, then the II sends an interrupt request (Duplonet PWRI      *
 * packet) to address offset 0x0180_0090 within the local register      *
 * address space of PI0 on the node specified by the NODE field. If     *
 * PI_ID==1, then the II sends the interrupt request to address         *
 * offset 0x01A0_0090 within the local register address space of PI1    *
 * on the node specified by the NODE field.                             *
 *									*
 ************************************************************************/

typedef union ii_iidsr_u {
	u64 ii_iidsr_regval;
	struct {
		u64 i_level:8;
		u64 i_pi_id:1;
		u64 i_node:11;
		u64 i_rsvd_3:4;
		u64 i_enable:1;
		u64 i_rsvd_2:3;
		u64 i_int_sent:2;
		u64 i_rsvd_1:2;
		u64 i_pi0_forward_int:1;
		u64 i_pi1_forward_int:1;
		u64 i_rsvd:30;
	} ii_iidsr_fld_s;
} ii_iidsr_u_t;

/************************************************************************
 *									*
 *  There are two instances of this register. This register is used     *
 * for matching up the incoming responses from the graphics widget to   *
 * the processor that initiated the graphics operation. The             *
 * write-responses are converted to graphics credits and returned to    *
 * the processor so that the processor interface can manage the flow    *
 * control.                                                             *
 *									*
 ************************************************************************/

typedef union ii_igfx0_u {
	u64 ii_igfx0_regval;
	struct {
		u64 i_w_num:4;
		u64 i_pi_id:1;
		u64 i_n_num:12;
		u64 i_p_num:1;
		u64 i_rsvd:46;
	} ii_igfx0_fld_s;
} ii_igfx0_u_t;

/************************************************************************
 *									*
 *  There are two instances of this register. This register is used     *
 * for matching up the incoming responses from the graphics widget to   *
 * the processor that initiated the graphics operation. The             *
 * write-responses are converted to graphics credits and returned to    *
 * the processor so that the processor interface can manage the flow    *
 * control.                                                             *
 *									*
 ************************************************************************/

typedef union ii_igfx1_u {
	u64 ii_igfx1_regval;
	struct {
		u64 i_w_num:4;
		u64 i_pi_id:1;
		u64 i_n_num:12;
		u64 i_p_num:1;
		u64 i_rsvd:46;
	} ii_igfx1_fld_s;
} ii_igfx1_u_t;

/************************************************************************
 *									*
 *  There are two instances of this registers. These registers are      *
 * used as scratch registers for software use.                          *
 *									*
 ************************************************************************/

typedef union ii_iscr0_u {
	u64 ii_iscr0_regval;
	struct {
		u64 i_scratch:64;
	} ii_iscr0_fld_s;
} ii_iscr0_u_t;

/************************************************************************
 *									*
 *  There are two instances of this registers. These registers are      *
 * used as scratch registers for software use.                          *
 *									*
 ************************************************************************/

typedef union ii_iscr1_u {
	u64 ii_iscr1_regval;
	struct {
		u64 i_scratch:64;
	} ii_iscr1_fld_s;
} ii_iscr1_u_t;

/************************************************************************
 *									*
 * Description:  There are seven instances of translation table entry   *
 * registers. Each register maps a Shub Big Window to a 48-bit          *
 * address on Crosstalk.                                                *
 * For M-mode (128 nodes, 8 GBytes/node), SysAD[31:29] (Big Window      *
 * number) are used to select one of these 7 registers. The Widget      *
 * number field is then derived from the W_NUM field for synthesizing   *
 * a Crosstalk packet. The 5 bits of OFFSET are concatenated with       *
 * SysAD[28:0] to form Crosstalk[33:0]. The upper Crosstalk[47:34]      *
 * are padded with zeros. Although the maximum Crosstalk space          *
 * addressable by the SHub is thus the lower 16 GBytes per widget       * 
 * (M-mode), however only <SUP >7</SUP>/<SUB >32nds</SUB> of this       *
 * space can be accessed.                                               *
 * For the N-mode (256 nodes, 4 GBytes/node), SysAD[30:28] (Big         *
 * Window number) are used to select one of these 7 registers. The      *
 * Widget number field is then derived from the W_NUM field for         *
 * synthesizing a Crosstalk packet. The 5 bits of OFFSET are            *
 * concatenated with SysAD[27:0] to form Crosstalk[33:0]. The IOSP      *
 * field is used as Crosstalk[47], and remainder of the Crosstalk       *
 * address bits (Crosstalk[46:34]) are always zero. While the maximum   *
 * Crosstalk space addressable by the Shub is thus the lower            *
 * 8-GBytes per widget (N-mode), only <SUP >7</SUP>/<SUB >32nds</SUB>   *
 * of this space can be accessed.                                       *
 *									*
 ************************************************************************/

typedef union ii_itte1_u {
	u64 ii_itte1_regval;
	struct {
		u64 i_offset:5;
		u64 i_rsvd_1:3;
		u64 i_w_num:4;
		u64 i_iosp:1;
		u64 i_rsvd:51;
	} ii_itte1_fld_s;
} ii_itte1_u_t;

/************************************************************************
 *									*
 * Description:  There are seven instances of translation table entry   *
 * registers. Each register maps a Shub Big Window to a 48-bit          *
 * address on Crosstalk.                                                *
 * For M-mode (128 nodes, 8 GBytes/node), SysAD[31:29] (Big Window      *
 * number) are used to select one of these 7 registers. The Widget      *
 * number field is then derived from the W_NUM field for synthesizing   *
 * a Crosstalk packet. The 5 bits of OFFSET are concatenated with       *
 * SysAD[28:0] to form Crosstalk[33:0]. The upper Crosstalk[47:34]      *
 * are padded with zeros. Although the maximum Crosstalk space          *
 * addressable by the Shub is thus the lower 16 GBytes per widget       *
 * (M-mode), however only <SUP >7</SUP>/<SUB >32nds</SUB> of this       *
 * space can be accessed.                                               *
 * For the N-mode (256 nodes, 4 GBytes/node), SysAD[30:28] (Big         *
 * Window number) are used to select one of these 7 registers. The      *
 * Widget number field is then derived from the W_NUM field for         *
 * synthesizing a Crosstalk packet. The 5 bits of OFFSET are            *
 * concatenated with SysAD[27:0] to form Crosstalk[33:0]. The IOSP      *
 * field is used as Crosstalk[47], and remainder of the Crosstalk       *
 * address bits (Crosstalk[46:34]) are always zero. While the maximum   *
 * Crosstalk space addressable by the Shub is thus the lower            *
 * 8-GBytes per widget (N-mode), only <SUP >7</SUP>/<SUB >32nds</SUB>   *
 * of this space can be accessed.                                       *
 *									*
 ************************************************************************/

typedef union ii_itte2_u {
	u64 ii_itte2_regval;
	struct {
		u64 i_offset:5;
		u64 i_rsvd_1:3;
		u64 i_w_num:4;
		u64 i_iosp:1;
		u64 i_rsvd:51;
	} ii_itte2_fld_s;
} ii_itte2_u_t;

/************************************************************************
 *									*
 * Description:  There are seven instances of translation table entry   *
 * registers. Each register maps a Shub Big Window to a 48-bit          *
 * address on Crosstalk.                                                *
 * For M-mode (128 nodes, 8 GBytes/node), SysAD[31:29] (Big Window      *
 * number) are used to select one of these 7 registers. The Widget      *
 * number field is then derived from the W_NUM field for synthesizing   *
 * a Crosstalk packet. The 5 bits of OFFSET are concatenated with       *
 * SysAD[28:0] to form Crosstalk[33:0]. The upper Crosstalk[47:34]      *
 * are padded with zeros. Although the maximum Crosstalk space          *
 * addressable by the Shub is thus the lower 16 GBytes per widget       *
 * (M-mode), however only <SUP >7</SUP>/<SUB >32nds</SUB> of this       *
 * space can be accessed.                                               *
 * For the N-mode (256 nodes, 4 GBytes/node), SysAD[30:28] (Big         *
 * Window number) are used to select one of these 7 registers. The      *
 * Widget number field is then derived from the W_NUM field for         *
 * synthesizing a Crosstalk packet. The 5 bits of OFFSET are            *
 * concatenated with SysAD[27:0] to form Crosstalk[33:0]. The IOSP      *
 * field is used as Crosstalk[47], and remainder of the Crosstalk       *
 * address bits (Crosstalk[46:34]) are always zero. While the maximum   *
 * Crosstalk space addressable by the SHub is thus the lower            *
 * 8-GBytes per widget (N-mode), only <SUP >7</SUP>/<SUB >32nds</SUB>   *
 * of this space can be accessed.                                       *
 *									*
 ************************************************************************/

typedef union ii_itte3_u {
	u64 ii_itte3_regval;
	struct {
		u64 i_offset:5;
		u64 i_rsvd_1:3;
		u64 i_w_num:4;
		u64 i_iosp:1;
		u64 i_rsvd:51;
	} ii_itte3_fld_s;
} ii_itte3_u_t;

/************************************************************************
 *									*
 * Description:  There are seven instances of translation table entry   *
 * registers. Each register maps a SHub Big Window to a 48-bit          *
 * address on Crosstalk.                                                *
 * For M-mode (128 nodes, 8 GBytes/node), SysAD[31:29] (Big Window      *
 * number) are used to select one of these 7 registers. The Widget      *
 * number field is then derived from the W_NUM field for synthesizing   *
 * a Crosstalk packet. The 5 bits of OFFSET are concatenated with       *
 * SysAD[28:0] to form Crosstalk[33:0]. The upper Crosstalk[47:34]      *
 * are padded with zeros. Although the maximum Crosstalk space          *
 * addressable by the SHub is thus the lower 16 GBytes per widget       *
 * (M-mode), however only <SUP >7</SUP>/<SUB >32nds</SUB> of this       *
 * space can be accessed.                                               *
 * For the N-mode (256 nodes, 4 GBytes/node), SysAD[30:28] (Big         *
 * Window number) are used to select one of these 7 registers. The      *
 * Widget number field is then derived from the W_NUM field for         *
 * synthesizing a Crosstalk packet. The 5 bits of OFFSET are            *
 * concatenated with SysAD[27:0] to form Crosstalk[33:0]. The IOSP      *
 * field is used as Crosstalk[47], and remainder of the Crosstalk       *
 * address bits (Crosstalk[46:34]) are always zero. While the maximum   *
 * Crosstalk space addressable by the SHub is thus the lower            *
 * 8-GBytes per widget (N-mode), only <SUP >7</SUP>/<SUB >32nds</SUB>   *
 * of this space can be accessed.                                       *
 *									*
 ************************************************************************/

typedef union ii_itte4_u {
	u64 ii_itte4_regval;
	struct {
		u64 i_offset:5;
		u64 i_rsvd_1:3;
		u64 i_w_num:4;
		u64 i_iosp:1;
		u64 i_rsvd:51;
	} ii_itte4_fld_s;
} ii_itte4_u_t;

/************************************************************************
 *									*
 * Description:  There are seven instances of translation table entry   *
 * registers. Each register maps a SHub Big Window to a 48-bit          *
 * address on Crosstalk.                                                *
 * For M-mode (128 nodes, 8 GBytes/node), SysAD[31:29] (Big Window      *
 * number) are used to select one of these 7 registers. The Widget      *
 * number field is then derived from the W_NUM field for synthesizing   *
 * a Crosstalk packet. The 5 bits of OFFSET are concatenated with       *
 * SysAD[28:0] to form Crosstalk[33:0]. The upper Crosstalk[47:34]      *
 * are padded with zeros. Although the maximum Crosstalk space          *
 * addressable by the Shub is thus the lower 16 GBytes per widget       *
 * (M-mode), however only <SUP >7</SUP>/<SUB >32nds</SUB> of this       *
 * space can be accessed.                                               *
 * For the N-mode (256 nodes, 4 GBytes/node), SysAD[30:28] (Big         *
 * Window number) are used to select one of these 7 registers. The      *
 * Widget number field is then derived from the W_NUM field for         *
 * synthesizing a Crosstalk packet. The 5 bits of OFFSET are            *
 * concatenated with SysAD[27:0] to form Crosstalk[33:0]. The IOSP      *
 * field is used as Crosstalk[47], and remainder of the Crosstalk       *
 * address bits (Crosstalk[46:34]) are always zero. While the maximum   *
 * Crosstalk space addressable by the Shub is thus the lower            *
 * 8-GBytes per widget (N-mode), only <SUP >7</SUP>/<SUB >32nds</SUB>   *
 * of this space can be accessed.                                       *
 *									*
 ************************************************************************/

typedef union ii_itte5_u {
	u64 ii_itte5_regval;
	struct {
		u64 i_offset:5;
		u64 i_rsvd_1:3;
		u64 i_w_num:4;
		u64 i_iosp:1;
		u64 i_rsvd:51;
	} ii_itte5_fld_s;
} ii_itte5_u_t;

/************************************************************************
 *									*
 * Description:  There are seven instances of translation table entry   *
 * registers. Each register maps a Shub Big Window to a 48-bit          *
 * address on Crosstalk.                                                *
 * For M-mode (128 nodes, 8 GBytes/node), SysAD[31:29] (Big Window      *
 * number) are used to select one of these 7 registers. The Widget      *
 * number field is then derived from the W_NUM field for synthesizing   *
 * a Crosstalk packet. The 5 bits of OFFSET are concatenated with       *
 * SysAD[28:0] to form Crosstalk[33:0]. The upper Crosstalk[47:34]      *
 * are padded with zeros. Although the maximum Crosstalk space          *
 * addressable by the Shub is thus the lower 16 GBytes per widget       *
 * (M-mode), however only <SUP >7</SUP>/<SUB >32nds</SUB> of this       *
 * space can be accessed.                                               *
 * For the N-mode (256 nodes, 4 GBytes/node), SysAD[30:28] (Big         *
 * Window number) are used to select one of these 7 registers. The      *
 * Widget number field is then derived from the W_NUM field for         *
 * synthesizing a Crosstalk packet. The 5 bits of OFFSET are            *
 * concatenated with SysAD[27:0] to form Crosstalk[33:0]. The IOSP      *
 * field is used as Crosstalk[47], and remainder of the Crosstalk       *
 * address bits (Crosstalk[46:34]) are always zero. While the maximum   *
 * Crosstalk space addressable by the Shub is thus the lower            *
 * 8-GBytes per widget (N-mode), only <SUP >7</SUP>/<SUB >32nds</SUB>   *
 * of this space can be accessed.                                       *
 *									*
 ************************************************************************/

typedef union ii_itte6_u {
	u64 ii_itte6_regval;
	struct {
		u64 i_offset:5;
		u64 i_rsvd_1:3;
		u64 i_w_num:4;
		u64 i_iosp:1;
		u64 i_rsvd:51;
	} ii_itte6_fld_s;
} ii_itte6_u_t;

/************************************************************************
 *									*
 * Description:  There are seven instances of translation table entry   *
 * registers. Each register maps a Shub Big Window to a 48-bit          *
 * address on Crosstalk.                                                *
 * For M-mode (128 nodes, 8 GBytes/node), SysAD[31:29] (Big Window      *
 * number) are used to select one of these 7 registers. The Widget      *
 * number field is then derived from the W_NUM field for synthesizing   *
 * a Crosstalk packet. The 5 bits of OFFSET are concatenated with       *
 * SysAD[28:0] to form Crosstalk[33:0]. The upper Crosstalk[47:34]      *
 * are padded with zeros. Although the maximum Crosstalk space          *
 * addressable by the Shub is thus the lower 16 GBytes per widget       *
 * (M-mode), however only <SUP >7</SUP>/<SUB >32nds</SUB> of this       *
 * space can be accessed.                                               *
 * For the N-mode (256 nodes, 4 GBytes/node), SysAD[30:28] (Big         *
 * Window number) are used to select one of these 7 registers. The      *
 * Widget number field is then derived from the W_NUM field for         *
 * synthesizing a Crosstalk packet. The 5 bits of OFFSET are            *
 * concatenated with SysAD[27:0] to form Crosstalk[33:0]. The IOSP      *
 * field is used as Crosstalk[47], and remainder of the Crosstalk       *
 * address bits (Crosstalk[46:34]) are always zero. While the maximum   *
 * Crosstalk space addressable by the SHub is thus the lower            *
 * 8-GBytes per widget (N-mode), only <SUP >7</SUP>/<SUB >32nds</SUB>   *
 * of this space can be accessed.                                       *
 *									*
 ************************************************************************/

typedef union ii_itte7_u {
	u64 ii_itte7_regval;
	struct {
		u64 i_offset:5;
		u64 i_rsvd_1:3;
		u64 i_w_num:4;
		u64 i_iosp:1;
		u64 i_rsvd:51;
	} ii_itte7_fld_s;
} ii_itte7_u_t;

/************************************************************************
 *									*
 * Description:  There are 9 instances of this register, one per        *
 * actual widget in this implementation of SHub and Crossbow.           *
 * Note: Crossbow only has ports for Widgets 8 through F, widget 0      *
 * refers to Crossbow's internal space.                                 *
 * This register contains the state elements per widget that are        *
 * necessary to manage the PIO flow control on Crosstalk and on the     *
 * Router Network. See the PIO Flow Control chapter for a complete      *
 * description of this register                                         *
 * The SPUR_WR bit requires some explanation. When this register is     *
 * written, the new value of the C field is captured in an internal     *
 * register so the hardware can remember what the programmer wrote      *
 * into the credit counter. The SPUR_WR bit sets whenever the C field   *
 * increments above this stored value, which indicates that there       *
 * have been more responses received than requests sent. The SPUR_WR    *
 * bit cannot be cleared until a value is written to the IPRBx          *
 * register; the write will correct the C field and capture its new     *
 * value in the internal register. Even if IECLR[E_PRB_x] is set, the   *
 * SPUR_WR bit will persist if IPRBx hasn't yet been written.           *
 * .    								*
 *									*
 ************************************************************************/

typedef union ii_iprb0_u {
	u64 ii_iprb0_regval;
	struct {
		u64 i_c:8;
		u64 i_na:14;
		u64 i_rsvd_2:2;
		u64 i_nb:14;
		u64 i_rsvd_1:2;
		u64 i_m:2;
		u64 i_f:1;
		u64 i_of_cnt:5;
		u64 i_error:1;
		u64 i_rd_to:1;
		u64 i_spur_wr:1;
		u64 i_spur_rd:1;
		u64 i_rsvd:11;
		u64 i_mult_err:1;
	} ii_iprb0_fld_s;
} ii_iprb0_u_t;

/************************************************************************
 *									*
 * Description:  There are 9 instances of this register, one per        *
 * actual widget in this implementation of SHub and Crossbow.           *
 * Note: Crossbow only has ports for Widgets 8 through F, widget 0      *
 * refers to Crossbow's internal space.                                 *
 * This register contains the state elements per widget that are        *
 * necessary to manage the PIO flow control on Crosstalk and on the     *
 * Router Network. See the PIO Flow Control chapter for a complete      *
 * description of this register                                         *
 * The SPUR_WR bit requires some explanation. When this register is     *
 * written, the new value of the C field is captured in an internal     *
 * register so the hardware can remember what the programmer wrote      *
 * into the credit counter. The SPUR_WR bit sets whenever the C field   *
 * increments above this stored value, which indicates that there       *
 * have been more responses received than requests sent. The SPUR_WR    *
 * bit cannot be cleared until a value is written to the IPRBx          *
 * register; the write will correct the C field and capture its new     *
 * value in the internal register. Even if IECLR[E_PRB_x] is set, the   *
 * SPUR_WR bit will persist if IPRBx hasn't yet been written.           *
 * .    								*
 *									*
 ************************************************************************/

typedef union ii_iprb8_u {
	u64 ii_iprb8_regval;
	struct {
		u64 i_c:8;
		u64 i_na:14;
		u64 i_rsvd_2:2;
		u64 i_nb:14;
		u64 i_rsvd_1:2;
		u64 i_m:2;
		u64 i_f:1;
		u64 i_of_cnt:5;
		u64 i_error:1;
		u64 i_rd_to:1;
		u64 i_spur_wr:1;
		u64 i_spur_rd:1;
		u64 i_rsvd:11;
		u64 i_mult_err:1;
	} ii_iprb8_fld_s;
} ii_iprb8_u_t;

/************************************************************************
 *									*
 * Description:  There are 9 instances of this register, one per        *
 * actual widget in this implementation of SHub and Crossbow.           *
 * Note: Crossbow only has ports for Widgets 8 through F, widget 0      *
 * refers to Crossbow's internal space.                                 *
 * This register contains the state elements per widget that are        *
 * necessary to manage the PIO flow control on Crosstalk and on the     *
 * Router Network. See the PIO Flow Control chapter for a complete      *
 * description of this register                                         *
 * The SPUR_WR bit requires some explanation. When this register is     *
 * written, the new value of the C field is captured in an internal     *
 * register so the hardware can remember what the programmer wrote      *
 * into the credit counter. The SPUR_WR bit sets whenever the C field   *
 * increments above this stored value, which indicates that there       *
 * have been more responses received than requests sent. The SPUR_WR    *
 * bit cannot be cleared until a value is written to the IPRBx          *
 * register; the write will correct the C field and capture its new     *
 * value in the internal register. Even if IECLR[E_PRB_x] is set, the   *
 * SPUR_WR bit will persist if IPRBx hasn't yet been written.           *
 * .    								*
 *									*
 ************************************************************************/

typedef union ii_iprb9_u {
	u64 ii_iprb9_regval;
	struct {
		u64 i_c:8;
		u64 i_na:14;
		u64 i_rsvd_2:2;
		u64 i_nb:14;
		u64 i_rsvd_1:2;
		u64 i_m:2;
		u64 i_f:1;
		u64 i_of_cnt:5;
		u64 i_error:1;
		u64 i_rd_to:1;
		u64 i_spur_wr:1;
		u64 i_spur_rd:1;
		u64 i_rsvd:11;
		u64 i_mult_err:1;
	} ii_iprb9_fld_s;
} ii_iprb9_u_t;

/************************************************************************
 *									*
 * Description:  There are 9 instances of this register, one per        *
 * actual widget in this implementation of SHub and Crossbow.        *
 * Note: Crossbow only has ports for Widgets 8 through F, widget 0      *
 * refers to Crossbow's internal space.                                 *
 * This register contains the state elements per widget that are        *
 * necessary to manage the PIO flow control on Crosstalk and on the     *
 * Router Network. See the PIO Flow Control chapter for a complete      *
 * description of this register                                         *
 * The SPUR_WR bit requires some explanation. When this register is     *
 * written, the new value of the C field is captured in an internal     *
 * register so the hardware can remember what the programmer wrote      *
 * into the credit counter. The SPUR_WR bit sets whenever the C field   *
 * increments above this stored value, which indicates that there       *
 * have been more responses received than requests sent. The SPUR_WR    *
 * bit cannot be cleared until a value is written to the IPRBx          *
 * register; the write will correct the C field and capture its new     *
 * value in the internal register. Even if IECLR[E_PRB_x] is set, the   *
 * SPUR_WR bit will persist if IPRBx hasn't yet been written.           *
 *									*
 *									*
 ************************************************************************/

typedef union ii_iprba_u {
	u64 ii_iprba_regval;
	struct {
		u64 i_c:8;
		u64 i_na:14;
		u64 i_rsvd_2:2;
		u64 i_nb:14;
		u64 i_rsvd_1:2;
		u64 i_m:2;
		u64 i_f:1;
		u64 i_of_cnt:5;
		u64 i_error:1;
		u64 i_rd_to:1;
		u64 i_spur_wr:1;
		u64 i_spur_rd:1;
		u64 i_rsvd:11;
		u64 i_mult_err:1;
	} ii_iprba_fld_s;
} ii_iprba_u_t;

/************************************************************************
 *									*
 * Description:  There are 9 instances of this register, one per        *
 * actual widget in this implementation of SHub and Crossbow.           *
 * Note: Crossbow only has ports for Widgets 8 through F, widget 0      *
 * refers to Crossbow's internal space.                                 *
 * This register contains the state elements per widget that are        *
 * necessary to manage the PIO flow control on Crosstalk and on the     *
 * Router Network. See the PIO Flow Control chapter for a complete      *
 * description of this register                                         *
 * The SPUR_WR bit requires some explanation. When this register is     *
 * written, the new value of the C field is captured in an internal     *
 * register so the hardware can remember what the programmer wrote      *
 * into the credit counter. The SPUR_WR bit sets whenever the C field   *
 * increments above this stored value, which indicates that there       *
 * have been more responses received than requests sent. The SPUR_WR    *
 * bit cannot be cleared until a value is written to the IPRBx          *
 * register; the write will correct the C field and capture its new     *
 * value in the internal register. Even if IECLR[E_PRB_x] is set, the   *
 * SPUR_WR bit will persist if IPRBx hasn't yet been written.           *
 * .    								*
 *									*
 ************************************************************************/

typedef union ii_iprbb_u {
	u64 ii_iprbb_regval;
	struct {
		u64 i_c:8;
		u64 i_na:14;
		u64 i_rsvd_2:2;
		u64 i_nb:14;
		u64 i_rsvd_1:2;
		u64 i_m:2;
		u64 i_f:1;
		u64 i_of_cnt:5;
		u64 i_error:1;
		u64 i_rd_to:1;
		u64 i_spur_wr:1;
		u64 i_spur_rd:1;
		u64 i_rsvd:11;
		u64 i_mult_err:1;
	} ii_iprbb_fld_s;
} ii_iprbb_u_t;

/************************************************************************
 *									*
 * Description:  There are 9 instances of this register, one per        *
 * actual widget in this implementation of SHub and Crossbow.           *
 * Note: Crossbow only has ports for Widgets 8 through F, widget 0      *
 * refers to Crossbow's internal space.                                 *
 * This register contains the state elements per widget that are        *
 * necessary to manage the PIO flow control on Crosstalk and on the     *
 * Router Network. See the PIO Flow Control chapter for a complete      *
 * description of this register                                         *
 * The SPUR_WR bit requires some explanation. When this register is     *
 * written, the new value of the C field is captured in an internal     *
 * register so the hardware can remember what the programmer wrote      *
 * into the credit counter. The SPUR_WR bit sets whenever the C field   *
 * increments above this stored value, which indicates that there       *
 * have been more responses received than requests sent. The SPUR_WR    *
 * bit cannot be cleared until a value is written to the IPRBx          *
 * register; the write will correct the C field and capture its new     *
 * value in the internal register. Even if IECLR[E_PRB_x] is set, the   *
 * SPUR_WR bit will persist if IPRBx hasn't yet been written.           *
 * .    								*
 *									*
 ************************************************************************/

typedef union ii_iprbc_u {
	u64 ii_iprbc_regval;
	struct {
		u64 i_c:8;
		u64 i_na:14;
		u64 i_rsvd_2:2;
		u64 i_nb:14;
		u64 i_rsvd_1:2;
		u64 i_m:2;
		u64 i_f:1;
		u64 i_of_cnt:5;
		u64 i_error:1;
		u64 i_rd_to:1;
		u64 i_spur_wr:1;
		u64 i_spur_rd:1;
		u64 i_rsvd:11;
		u64 i_mult_err:1;
	} ii_iprbc_fld_s;
} ii_iprbc_u_t;

/************************************************************************
 *									*
 * Description:  There are 9 instances of this register, one per        *
 * actual widget in this implementation of SHub and Crossbow.           *
 * Note: Crossbow only has ports for Widgets 8 through F, widget 0      *
 * refers to Crossbow's internal space.                                 *
 * This register contains the state elements per widget that are        *
 * necessary to manage the PIO flow control on Crosstalk and on the     *
 * Router Network. See the PIO Flow Control chapter for a complete      *
 * description of this register                                         *
 * The SPUR_WR bit requires some explanation. When this register is     *
 * written, the new value of the C field is captured in an internal     *
 * register so the hardware can remember what the programmer wrote      *
 * into the credit counter. The SPUR_WR bit sets whenever the C field   *
 * increments above this stored value, which indicates that there       *
 * have been more responses received than requests sent. The SPUR_WR    *
 * bit cannot be cleared until a value is written to the IPRBx          *
 * register; the write will correct the C field and capture its new     *
 * value in the internal register. Even if IECLR[E_PRB_x] is set, the   *
 * SPUR_WR bit will persist if IPRBx hasn't yet been written.           *
 * .    								*
 *									*
 ************************************************************************/

typedef union ii_iprbd_u {
	u64 ii_iprbd_regval;
	struct {
		u64 i_c:8;
		u64 i_na:14;
		u64 i_rsvd_2:2;
		u64 i_nb:14;
		u64 i_rsvd_1:2;
		u64 i_m:2;
		u64 i_f:1;
		u64 i_of_cnt:5;
		u64 i_error:1;
		u64 i_rd_to:1;
		u64 i_spur_wr:1;
		u64 i_spur_rd:1;
		u64 i_rsvd:11;
		u64 i_mult_err:1;
	} ii_iprbd_fld_s;
} ii_iprbd_u_t;

/************************************************************************
 *									*
 * Description:  There are 9 instances of this register, one per        *
 * actual widget in this implementation of SHub and Crossbow.           *
 * Note: Crossbow only has ports for Widgets 8 through F, widget 0      *
 * refers to Crossbow's internal space.                                 *
 * This register contains the state elements per widget that are        *
 * necessary to manage the PIO flow control on Crosstalk and on the     *
 * Router Network. See the PIO Flow Control chapter for a complete      *
 * description of this register                                         *
 * The SPUR_WR bit requires some explanation. When this register is     *
 * written, the new value of the C field is captured in an internal     *
 * register so the hardware can remember what the programmer wrote      *
 * into the credit counter. The SPUR_WR bit sets whenever the C field   *
 * increments above this stored value, which indicates that there       *
 * have been more responses received than requests sent. The SPUR_WR    *
 * bit cannot be cleared until a value is written to the IPRBx          *
 * register; the write will correct the C field and capture its new     *
 * value in the internal register. Even if IECLR[E_PRB_x] is set, the   *
 * SPUR_WR bit will persist if IPRBx hasn't yet been written.           *
 * .    								*
 *									*
 ************************************************************************/

typedef union ii_iprbe_u {
	u64 ii_iprbe_regval;
	struct {
		u64 i_c:8;
		u64 i_na:14;
		u64 i_rsvd_2:2;
		u64 i_nb:14;
		u64 i_rsvd_1:2;
		u64 i_m:2;
		u64 i_f:1;
		u64 i_of_cnt:5;
		u64 i_error:1;
		u64 i_rd_to:1;
		u64 i_spur_wr:1;
		u64 i_spur_rd:1;
		u64 i_rsvd:11;
		u64 i_mult_err:1;
	} ii_iprbe_fld_s;
} ii_iprbe_u_t;

/************************************************************************
 *									*
 * Description:  There are 9 instances of this register, one per        *
 * actual widget in this implementation of Shub and Crossbow.           *
 * Note: Crossbow only has ports for Widgets 8 through F, widget 0      *
 * refers to Crossbow's internal space.                                 *
 * This register contains the state elements per widget that are        *
 * necessary to manage the PIO flow control on Crosstalk and on the     *
 * Router Network. See the PIO Flow Control chapter for a complete      *
 * description of this register                                         *
 * The SPUR_WR bit requires some explanation. When this register is     *
 * written, the new value of the C field is captured in an internal     *
 * register so the hardware can remember what the programmer wrote      *
 * into the credit counter. The SPUR_WR bit sets whenever the C field   *
 * increments above this stored value, which indicates that there       *
 * have been more responses received than requests sent. The SPUR_WR    *
 * bit cannot be cleared until a value is written to the IPRBx          *
 * register; the write will correct the C field and capture its new     *
 * value in the internal register. Even if IECLR[E_PRB_x] is set, the   *
 * SPUR_WR bit will persist if IPRBx hasn't yet been written.           *
 * .    								*
 *									*
 ************************************************************************/

typedef union ii_iprbf_u {
	u64 ii_iprbf_regval;
	struct {
		u64 i_c:8;
		u64 i_na:14;
		u64 i_rsvd_2:2;
		u64 i_nb:14;
		u64 i_rsvd_1:2;
		u64 i_m:2;
		u64 i_f:1;
		u64 i_of_cnt:5;
		u64 i_error:1;
		u64 i_rd_to:1;
		u64 i_spur_wr:1;
		u64 i_spur_rd:1;
		u64 i_rsvd:11;
		u64 i_mult_err:1;
	} ii_iprbe_fld_s;
} ii_iprbf_u_t;

/************************************************************************
 *									*
 *  This register specifies the timeout value to use for monitoring     *
 * Crosstalk credits which are used outbound to Crosstalk. An           *
 * internal counter called the Crosstalk Credit Timeout Counter         *
 * increments every 128 II clocks. The counter starts counting          *
 * anytime the credit count drops below a threshold, and resets to      *
 * zero (stops counting) anytime the credit count is at or above the    *
 * threshold. The threshold is 1 credit in direct connect mode and 2    *
 * in Crossbow connect mode. When the internal Crosstalk Credit         *
 * Timeout Counter reaches the value programmed in this register, a     *
 * Crosstalk Credit Timeout has occurred. The internal counter is not   *
 * readable from software, and stops counting at its maximum value,     *
 * so it cannot cause more than one interrupt.                          *
 *									*
 ************************************************************************/

typedef union ii_ixcc_u {
	u64 ii_ixcc_regval;
	struct {
		u64 i_time_out:26;
		u64 i_rsvd:38;
	} ii_ixcc_fld_s;
} ii_ixcc_u_t;

/************************************************************************
 *									*
 * Description:  This register qualifies all the PIO and DMA            *
 * operations launched from widget 0 towards the SHub. In               *
 * addition, it also qualifies accesses by the BTE streams.             *
 * The bits in each field of this register are cleared by the SHub      *
 * upon detection of an error which requires widget 0 or the BTE        *
 * streams to be terminated. Whether or not widget x has access         *
 * rights to this SHub is determined by an AND of the device            *
 * enable bit in the appropriate field of this register and bit 0 in    *
 * the Wx_IAC field. The bits in this field are set by writing a 1 to   *
 * them. Incoming replies from Crosstalk are not subject to this        *
 * access control mechanism.                                            *
 *									*
 ************************************************************************/

typedef union ii_imem_u {
	u64 ii_imem_regval;
	struct {
		u64 i_w0_esd:1;
		u64 i_rsvd_3:3;
		u64 i_b0_esd:1;
		u64 i_rsvd_2:3;
		u64 i_b1_esd:1;
		u64 i_rsvd_1:3;
		u64 i_clr_precise:1;
		u64 i_rsvd:51;
	} ii_imem_fld_s;
} ii_imem_u_t;

/************************************************************************
 *									*
 * Description:  This register specifies the timeout value to use for   *
 * monitoring Crosstalk tail flits coming into the Shub in the          *
 * TAIL_TO field. An internal counter associated with this register     *
 * is incremented every 128 II internal clocks (7 bits). The counter    *
 * starts counting anytime a header micropacket is received and stops   *
 * counting (and resets to zero) any time a micropacket with a Tail     *
 * bit is received. Once the counter reaches the threshold value        *
 * programmed in this register, it generates an interrupt to the        *
 * processor that is programmed into the IIDSR. The counter saturates   *
 * (does not roll over) at its maximum value, so it cannot cause        *
 * another interrupt until after it is cleared.                         *
 * The register also contains the Read Response Timeout values. The     *
 * Prescalar is 23 bits, and counts II clocks. An internal counter      *
 * increments on every II clock and when it reaches the value in the    *
 * Prescalar field, all IPRTE registers with their valid bits set       *
 * have their Read Response timers bumped. Whenever any of them match   *
 * the value in the RRSP_TO field, a Read Response Timeout has          *
 * occurred, and error handling occurs as described in the Error        *
 * Handling section of this document.                                   *
 *									*
 ************************************************************************/

typedef union ii_ixtt_u {
	u64 ii_ixtt_regval;
	struct {
		u64 i_tail_to:26;
		u64 i_rsvd_1:6;
		u64 i_rrsp_ps:23;
		u64 i_rrsp_to:5;
		u64 i_rsvd:4;
	} ii_ixtt_fld_s;
} ii_ixtt_u_t;

/************************************************************************
 *									*
 *  Writing a 1 to the fields of this register clears the appropriate   *
 * error bits in other areas of SHub. Note that when the                *
 * E_PRB_x bits are used to clear error bits in PRB registers,          *
 * SPUR_RD and SPUR_WR may persist, because they require additional     *
 * action to clear them. See the IPRBx and IXSS Register                *
 * specifications.                                                      *
 *									*
 ************************************************************************/

typedef union ii_ieclr_u {
	u64 ii_ieclr_regval;
	struct {
		u64 i_e_prb_0:1;
		u64 i_rsvd:7;
		u64 i_e_prb_8:1;
		u64 i_e_prb_9:1;
		u64 i_e_prb_a:1;
		u64 i_e_prb_b:1;
		u64 i_e_prb_c:1;
		u64 i_e_prb_d:1;
		u64 i_e_prb_e:1;
		u64 i_e_prb_f:1;
		u64 i_e_crazy:1;
		u64 i_e_bte_0:1;
		u64 i_e_bte_1:1;
		u64 i_reserved_1:10;
		u64 i_spur_rd_hdr:1;
		u64 i_cam_intr_to:1;
		u64 i_cam_overflow:1;
		u64 i_cam_read_miss:1;
		u64 i_ioq_rep_underflow:1;
		u64 i_ioq_req_underflow:1;
		u64 i_ioq_rep_overflow:1;
		u64 i_ioq_req_overflow:1;
		u64 i_iiq_rep_overflow:1;
		u64 i_iiq_req_overflow:1;
		u64 i_ii_xn_rep_cred_overflow:1;
		u64 i_ii_xn_req_cred_overflow:1;
		u64 i_ii_xn_invalid_cmd:1;
		u64 i_xn_ii_invalid_cmd:1;
		u64 i_reserved_2:21;
	} ii_ieclr_fld_s;
} ii_ieclr_u_t;

/************************************************************************
 *									*
 *  This register controls both BTEs. SOFT_RESET is intended for        *
 * recovery after an error. COUNT controls the total number of CRBs     *
 * that both BTEs (combined) can use, which affects total BTE           *
 * bandwidth.                                                           *
 *									*
 ************************************************************************/

typedef union ii_ibcr_u {
	u64 ii_ibcr_regval;
	struct {
		u64 i_count:4;
		u64 i_rsvd_1:4;
		u64 i_soft_reset:1;
		u64 i_rsvd:55;
	} ii_ibcr_fld_s;
} ii_ibcr_u_t;

/************************************************************************
 *									*
 *  This register contains the header of a spurious read response       *
 * received from Crosstalk. A spurious read response is defined as a    *
 * read response received by II from a widget for which (1) the SIDN    *
 * has a value between 1 and 7, inclusive (II never sends requests to   *
 * these widgets (2) there is no valid IPRTE register which             *
 * corresponds to the TNUM, or (3) the widget indicated in SIDN is      *
 * not the same as the widget recorded in the IPRTE register            *
 * referenced by the TNUM. If this condition is true, and if the        *
 * IXSS[VALID] bit is clear, then the header of the spurious read       *
 * response is capture in IXSM and IXSS, and IXSS[VALID] is set. The    *
 * errant header is thereby captured, and no further spurious read      *
 * respones are captured until IXSS[VALID] is cleared by setting the    *
 * appropriate bit in IECLR.Everytime a spurious read response is       *
 * detected, the SPUR_RD bit of the PRB corresponding to the incoming   *
 * message's SIDN field is set. This always happens, regarless of       *
 * whether a header is captured. The programmer should check            *
 * IXSM[SIDN] to determine which widget sent the spurious response,     *
 * because there may be more than one SPUR_RD bit set in the PRB        *
 * registers. The widget indicated by IXSM[SIDN] was the first          *
 * spurious read response to be received since the last time            *
 * IXSS[VALID] was clear. The SPUR_RD bit of the corresponding PRB      *
 * will be set. Any SPUR_RD bits in any other PRB registers indicate    *
 * spurious messages from other widets which were detected after the    *
 * header was captured..                                                *
 *									*
 ************************************************************************/

typedef union ii_ixsm_u {
	u64 ii_ixsm_regval;
	struct {
		u64 i_byte_en:32;
		u64 i_reserved:1;
		u64 i_tag:3;
		u64 i_alt_pactyp:4;
		u64 i_bo:1;
		u64 i_error:1;
		u64 i_vbpm:1;
		u64 i_gbr:1;
		u64 i_ds:2;
		u64 i_ct:1;
		u64 i_tnum:5;
		u64 i_pactyp:4;
		u64 i_sidn:4;
		u64 i_didn:4;
	} ii_ixsm_fld_s;
} ii_ixsm_u_t;

/************************************************************************
 *									*
 *  This register contains the sideband bits of a spurious read         *
 * response received from Crosstalk.                                    *
 *									*
 ************************************************************************/

typedef union ii_ixss_u {
	u64 ii_ixss_regval;
	struct {
		u64 i_sideband:8;
		u64 i_rsvd:55;
		u64 i_valid:1;
	} ii_ixss_fld_s;
} ii_ixss_u_t;

/************************************************************************
 *									*
 *  This register enables software to access the II LLP's test port.    *
 * Refer to the LLP 2.5 documentation for an explanation of the test    *
 * port. Software can write to this register to program the values      *
 * for the control fields (TestErrCapture, TestClear, TestFlit,         *
 * TestMask and TestSeed). Similarly, software can read from this       *
 * register to obtain the values of the test port's status outputs      *
 * (TestCBerr, TestValid and TestData).                                 *
 *									*
 ************************************************************************/

typedef union ii_ilct_u {
	u64 ii_ilct_regval;
	struct {
		u64 i_test_seed:20;
		u64 i_test_mask:8;
		u64 i_test_data:20;
		u64 i_test_valid:1;
		u64 i_test_cberr:1;
		u64 i_test_flit:3;
		u64 i_test_clear:1;
		u64 i_test_err_capture:1;
		u64 i_rsvd:9;
	} ii_ilct_fld_s;
} ii_ilct_u_t;

/************************************************************************
 *									*
 *  If the II detects an illegal incoming Duplonet packet (request or   *
 * reply) when VALID==0 in the IIEPH1 register, then it saves the       *
 * contents of the packet's header flit in the IIEPH1 and IIEPH2        *
 * registers, sets the VALID bit in IIEPH1, clears the OVERRUN bit,     *
 * and assigns a value to the ERR_TYPE field which indicates the        *
 * specific nature of the error. The II recognizes four different       *
 * types of errors: short request packets (ERR_TYPE==2), short reply    *
 * packets (ERR_TYPE==3), long request packets (ERR_TYPE==4) and long   *
 * reply packets (ERR_TYPE==5). The encodings for these types of        *
 * errors were chosen to be consistent with the same types of errors    *
 * indicated by the ERR_TYPE field in the LB_ERROR_HDR1 register (in    *
 * the LB unit). If the II detects an illegal incoming Duplonet         *
 * packet when VALID==1 in the IIEPH1 register, then it merely sets     *
 * the OVERRUN bit to indicate that a subsequent error has happened,    *
 * and does nothing further.                                            *
 *									*
 ************************************************************************/

typedef union ii_iieph1_u {
	u64 ii_iieph1_regval;
	struct {
		u64 i_command:7;
		u64 i_rsvd_5:1;
		u64 i_suppl:14;
		u64 i_rsvd_4:1;
		u64 i_source:14;
		u64 i_rsvd_3:1;
		u64 i_err_type:4;
		u64 i_rsvd_2:4;
		u64 i_overrun:1;
		u64 i_rsvd_1:3;
		u64 i_valid:1;
		u64 i_rsvd:13;
	} ii_iieph1_fld_s;
} ii_iieph1_u_t;

/************************************************************************
 *									*
 *  This register holds the Address field from the header flit of an    *
 * incoming erroneous Duplonet packet, along with the tail bit which    *
 * accompanied this header flit. This register is essentially an        *
 * extension of IIEPH1. Two registers were necessary because the 64     *
 * bits available in only a single register were insufficient to        *
 * capture the entire header flit of an erroneous packet.               *
 *									*
 ************************************************************************/

typedef union ii_iieph2_u {
	u64 ii_iieph2_regval;
	struct {
		u64 i_rsvd_0:3;
		u64 i_address:47;
		u64 i_rsvd_1:10;
		u64 i_tail:1;
		u64 i_rsvd:3;
	} ii_iieph2_fld_s;
} ii_iieph2_u_t;

/******************************/

/************************************************************************
 *									*
 *  This register's value is a bit vector that guards access from SXBs  *
 * to local registers within the II as well as to external Crosstalk    *
 * widgets								*
 *									*
 ************************************************************************/

typedef union ii_islapr_u {
	u64 ii_islapr_regval;
	struct {
		u64 i_region:64;
	} ii_islapr_fld_s;
} ii_islapr_u_t;

/************************************************************************
 *									*
 *  A write to this register of the 56-bit value "Pup+Bun" will cause	*
 * the bit in the ISLAPR register corresponding to the region of the	*
 * requestor to be set (access allowed).				(
 *									*
 ************************************************************************/

typedef union ii_islapo_u {
	u64 ii_islapo_regval;
	struct {
		u64 i_io_sbx_ovrride:56;
		u64 i_rsvd:8;
	} ii_islapo_fld_s;
} ii_islapo_u_t;

/************************************************************************
 *									*
 *  Determines how long the wrapper will wait aftr an interrupt is	*
 * initially issued from the II before it times out the outstanding	*
 * interrupt and drops it from the interrupt queue.			* 
 *									*
 ************************************************************************/

typedef union ii_iwi_u {
	u64 ii_iwi_regval;
	struct {
		u64 i_prescale:24;
		u64 i_rsvd:8;
		u64 i_timeout:8;
		u64 i_rsvd1:8;
		u64 i_intrpt_retry_period:8;
		u64 i_rsvd2:8;
	} ii_iwi_fld_s;
} ii_iwi_u_t;

/************************************************************************
 *									*
 *  Log errors which have occurred in the II wrapper. The errors are	*
 * cleared by writing to the IECLR register.				* 
 *									*
 ************************************************************************/

typedef union ii_iwel_u {
	u64 ii_iwel_regval;
	struct {
		u64 i_intr_timed_out:1;
		u64 i_rsvd:7;
		u64 i_cam_overflow:1;
		u64 i_cam_read_miss:1;
		u64 i_rsvd1:2;
		u64 i_ioq_rep_underflow:1;
		u64 i_ioq_req_underflow:1;
		u64 i_ioq_rep_overflow:1;
		u64 i_ioq_req_overflow:1;
		u64 i_iiq_rep_overflow:1;
		u64 i_iiq_req_overflow:1;
		u64 i_rsvd2:6;
		u64 i_ii_xn_rep_cred_over_under:1;
		u64 i_ii_xn_req_cred_over_under:1;
		u64 i_rsvd3:6;
		u64 i_ii_xn_invalid_cmd:1;
		u64 i_xn_ii_invalid_cmd:1;
		u64 i_rsvd4:30;
	} ii_iwel_fld_s;
} ii_iwel_u_t;

/************************************************************************
 *									*
 *  Controls the II wrapper.						* 
 *									*
 ************************************************************************/

typedef union ii_iwc_u {
	u64 ii_iwc_regval;
	struct {
		u64 i_dma_byte_swap:1;
		u64 i_rsvd:3;
		u64 i_cam_read_lines_reset:1;
		u64 i_rsvd1:3;
		u64 i_ii_xn_cred_over_under_log:1;
		u64 i_rsvd2:19;
		u64 i_xn_rep_iq_depth:5;
		u64 i_rsvd3:3;
		u64 i_xn_req_iq_depth:5;
		u64 i_rsvd4:3;
		u64 i_iiq_depth:6;
		u64 i_rsvd5:12;
		u64 i_force_rep_cred:1;
		u64 i_force_req_cred:1;
	} ii_iwc_fld_s;
} ii_iwc_u_t;

/************************************************************************
 *									*
 *  Status in the II wrapper.						* 
 *									*
 ************************************************************************/

typedef union ii_iws_u {
	u64 ii_iws_regval;
	struct {
		u64 i_xn_rep_iq_credits:5;
		u64 i_rsvd:3;
		u64 i_xn_req_iq_credits:5;
		u64 i_rsvd1:51;
	} ii_iws_fld_s;
} ii_iws_u_t;

/************************************************************************
 *									*
 *  Masks errors in the IWEL register.					*
 *									*
 ************************************************************************/

typedef union ii_iweim_u {
	u64 ii_iweim_regval;
	struct {
		u64 i_intr_timed_out:1;
		u64 i_rsvd:7;
		u64 i_cam_overflow:1;
		u64 i_cam_read_miss:1;
		u64 i_rsvd1:2;
		u64 i_ioq_rep_underflow:1;
		u64 i_ioq_req_underflow:1;
		u64 i_ioq_rep_overflow:1;
		u64 i_ioq_req_overflow:1;
		u64 i_iiq_rep_overflow:1;
		u64 i_iiq_req_overflow:1;
		u64 i_rsvd2:6;
		u64 i_ii_xn_rep_cred_overflow:1;
		u64 i_ii_xn_req_cred_overflow:1;
		u64 i_rsvd3:6;
		u64 i_ii_xn_invalid_cmd:1;
		u64 i_xn_ii_invalid_cmd:1;
		u64 i_rsvd4:30;
	} ii_iweim_fld_s;
} ii_iweim_u_t;

/************************************************************************
 *									*
 *  A write to this register causes a particular field in the           *
 * corresponding widget's PRB entry to be adjusted up or down by 1.     *
 * This counter should be used when recovering from error and reset     *
 * conditions. Note that software would be capable of causing           *
 * inadvertent overflow or underflow of these counters.                 *
 *									*
 ************************************************************************/

typedef union ii_ipca_u {
	u64 ii_ipca_regval;
	struct {
		u64 i_wid:4;
		u64 i_adjust:1;
		u64 i_rsvd_1:3;
		u64 i_field:2;
		u64 i_rsvd:54;
	} ii_ipca_fld_s;
} ii_ipca_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte0a_u {
	u64 ii_iprte0a_regval;
	struct {
		u64 i_rsvd_1:54;
		u64 i_widget:4;
		u64 i_to_cnt:5;
		u64 i_vld:1;
	} ii_iprte0a_fld_s;
} ii_iprte0a_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte1a_u {
	u64 ii_iprte1a_regval;
	struct {
		u64 i_rsvd_1:54;
		u64 i_widget:4;
		u64 i_to_cnt:5;
		u64 i_vld:1;
	} ii_iprte1a_fld_s;
} ii_iprte1a_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte2a_u {
	u64 ii_iprte2a_regval;
	struct {
		u64 i_rsvd_1:54;
		u64 i_widget:4;
		u64 i_to_cnt:5;
		u64 i_vld:1;
	} ii_iprte2a_fld_s;
} ii_iprte2a_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte3a_u {
	u64 ii_iprte3a_regval;
	struct {
		u64 i_rsvd_1:54;
		u64 i_widget:4;
		u64 i_to_cnt:5;
		u64 i_vld:1;
	} ii_iprte3a_fld_s;
} ii_iprte3a_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte4a_u {
	u64 ii_iprte4a_regval;
	struct {
		u64 i_rsvd_1:54;
		u64 i_widget:4;
		u64 i_to_cnt:5;
		u64 i_vld:1;
	} ii_iprte4a_fld_s;
} ii_iprte4a_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte5a_u {
	u64 ii_iprte5a_regval;
	struct {
		u64 i_rsvd_1:54;
		u64 i_widget:4;
		u64 i_to_cnt:5;
		u64 i_vld:1;
	} ii_iprte5a_fld_s;
} ii_iprte5a_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte6a_u {
	u64 ii_iprte6a_regval;
	struct {
		u64 i_rsvd_1:54;
		u64 i_widget:4;
		u64 i_to_cnt:5;
		u64 i_vld:1;
	} ii_iprte6a_fld_s;
} ii_iprte6a_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte7a_u {
	u64 ii_iprte7a_regval;
	struct {
		u64 i_rsvd_1:54;
		u64 i_widget:4;
		u64 i_to_cnt:5;
		u64 i_vld:1;
	} ii_iprtea7_fld_s;
} ii_iprte7a_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte0b_u {
	u64 ii_iprte0b_regval;
	struct {
		u64 i_rsvd_1:3;
		u64 i_address:47;
		u64 i_init:3;
		u64 i_source:11;
	} ii_iprte0b_fld_s;
} ii_iprte0b_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte1b_u {
	u64 ii_iprte1b_regval;
	struct {
		u64 i_rsvd_1:3;
		u64 i_address:47;
		u64 i_init:3;
		u64 i_source:11;
	} ii_iprte1b_fld_s;
} ii_iprte1b_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte2b_u {
	u64 ii_iprte2b_regval;
	struct {
		u64 i_rsvd_1:3;
		u64 i_address:47;
		u64 i_init:3;
		u64 i_source:11;
	} ii_iprte2b_fld_s;
} ii_iprte2b_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte3b_u {
	u64 ii_iprte3b_regval;
	struct {
		u64 i_rsvd_1:3;
		u64 i_address:47;
		u64 i_init:3;
		u64 i_source:11;
	} ii_iprte3b_fld_s;
} ii_iprte3b_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte4b_u {
	u64 ii_iprte4b_regval;
	struct {
		u64 i_rsvd_1:3;
		u64 i_address:47;
		u64 i_init:3;
		u64 i_source:11;
	} ii_iprte4b_fld_s;
} ii_iprte4b_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte5b_u {
	u64 ii_iprte5b_regval;
	struct {
		u64 i_rsvd_1:3;
		u64 i_address:47;
		u64 i_init:3;
		u64 i_source:11;
	} ii_iprte5b_fld_s;
} ii_iprte5b_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte6b_u {
	u64 ii_iprte6b_regval;
	struct {
		u64 i_rsvd_1:3;
		u64 i_address:47;
		u64 i_init:3;
		u64 i_source:11;

	} ii_iprte6b_fld_s;
} ii_iprte6b_u_t;

/************************************************************************
 *									*
 *  There are 8 instances of this register. This register contains      *
 * the information that the II has to remember once it has launched a   *
 * PIO Read operation. The contents are used to form the correct        *
 * Router Network packet and direct the Crosstalk reply to the          *
 * appropriate processor.                                               *
 *									*
 ************************************************************************/

typedef union ii_iprte7b_u {
	u64 ii_iprte7b_regval;
	struct {
		u64 i_rsvd_1:3;
		u64 i_address:47;
		u64 i_init:3;
		u64 i_source:11;
	} ii_iprte7b_fld_s;
} ii_iprte7b_u_t;

/************************************************************************
 *									*
 * Description:  SHub II contains a feature which did not exist in      *
 * the Hub which automatically cleans up after a Read Response          *
 * timeout, including deallocation of the IPRTE and recovery of IBuf    *
 * space. The inclusion of this register in SHub is for backward        *
 * compatibility                                                        *
 * A write to this register causes an entry from the table of           *
 * outstanding PIO Read Requests to be freed and returned to the        *
 * stack of free entries. This register is used in handling the         *
 * timeout errors that result in a PIO Reply never returning from       *
 * Crosstalk.                                                           *
 * Note that this register does not affect the contents of the IPRTE    *
 * registers. The Valid bits in those registers have to be              *
 * specifically turned off by software.                                 *
 *									*
 ************************************************************************/

typedef union ii_ipdr_u {
	u64 ii_ipdr_regval;
	struct {
		u64 i_te:3;
		u64 i_rsvd_1:1;
		u64 i_pnd:1;
		u64 i_init_rpcnt:1;
		u64 i_rsvd:58;
	} ii_ipdr_fld_s;
} ii_ipdr_u_t;

/************************************************************************
 *									*
 *  A write to this register causes a CRB entry to be returned to the   *
 * queue of free CRBs. The entry should have previously been cleared    *
 * (mark bit) via backdoor access to the pertinent CRB entry. This      *
 * register is used in the last step of handling the errors that are    *
 * captured and marked in CRB entries.  Briefly: 1) first error for     *
 * DMA write from a particular device, and first error for a            *
 * particular BTE stream, lead to a marked CRB entry, and processor     *
 * interrupt, 2) software reads the error information captured in the   *
 * CRB entry, and presumably takes some corrective action, 3)           *
 * software clears the mark bit, and finally 4) software writes to      *
 * the ICDR register to return the CRB entry to the list of free CRB    *
 * entries.                                                             *
 *									*
 ************************************************************************/

typedef union ii_icdr_u {
	u64 ii_icdr_regval;
	struct {
		u64 i_crb_num:4;
		u64 i_pnd:1;
		u64 i_rsvd:59;
	} ii_icdr_fld_s;
} ii_icdr_u_t;

/************************************************************************
 *									*
 *  This register provides debug access to two FIFOs inside of II.      *
 * Both IOQ_MAX* fields of this register contain the instantaneous      *
 * depth (in units of the number of available entries) of the           *
 * associated IOQ FIFO.  A read of this register will return the        *
 * number of free entries on each FIFO at the time of the read.  So     *
 * when a FIFO is idle, the associated field contains the maximum       *
 * depth of the FIFO.  This register is writable for debug reasons      *
 * and is intended to be written with the maximum desired FIFO depth    *
 * while the FIFO is idle. Software must assure that II is idle when    *
 * this register is written. If there are any active entries in any     *
 * of these FIFOs when this register is written, the results are        *
 * undefined.                                                           *
 *									*
 ************************************************************************/

typedef union ii_ifdr_u {
	u64 ii_ifdr_regval;
	struct {
		u64 i_ioq_max_rq:7;
		u64 i_set_ioq_rq:1;
		u64 i_ioq_max_rp:7;
		u64 i_set_ioq_rp:1;
		u64 i_rsvd:48;
	} ii_ifdr_fld_s;
} ii_ifdr_u_t;

/************************************************************************
 *									*
 *  This register allows the II to become sluggish in removing          *
 * messages from its inbound queue (IIQ). This will cause messages to   *
 * back up in either virtual channel. Disabling the "molasses" mode     *
 * subsequently allows the II to be tested under stress. In the         *
 * sluggish ("Molasses") mode, the localized effects of congestion      *
 * can be observed.                                                     *
 *									*
 ************************************************************************/

typedef union ii_iiap_u {
	u64 ii_iiap_regval;
	struct {
		u64 i_rq_mls:6;
		u64 i_rsvd_1:2;
		u64 i_rp_mls:6;
		u64 i_rsvd:50;
	} ii_iiap_fld_s;
} ii_iiap_u_t;

/************************************************************************
 *									*
 *  This register allows several parameters of CRB operation to be      *
 * set. Note that writing to this register can have catastrophic side   *
 * effects, if the CRB is not quiescent, i.e. if the CRB is             *
 * processing protocol messages when the write occurs.                  *
 *									*
 ************************************************************************/

typedef union ii_icmr_u {
	u64 ii_icmr_regval;
	struct {
		u64 i_sp_msg:1;
		u64 i_rd_hdr:1;
		u64 i_rsvd_4:2;
		u64 i_c_cnt:4;
		u64 i_rsvd_3:4;
		u64 i_clr_rqpd:1;
		u64 i_clr_rppd:1;
		u64 i_rsvd_2:2;
		u64 i_fc_cnt:4;
		u64 i_crb_vld:15;
		u64 i_crb_mark:15;
		u64 i_rsvd_1:2;
		u64 i_precise:1;
		u64 i_rsvd:11;
	} ii_icmr_fld_s;
} ii_icmr_u_t;

/************************************************************************
 *									*
 *  This register allows control of the table portion of the CRB        *
 * logic via software. Control operations from this register have       *
 * priority over all incoming Crosstalk or BTE requests.                *
 *									*
 ************************************************************************/

typedef union ii_iccr_u {
	u64 ii_iccr_regval;
	struct {
		u64 i_crb_num:4;
		u64 i_rsvd_1:4;
		u64 i_cmd:8;
		u64 i_pending:1;
		u64 i_rsvd:47;
	} ii_iccr_fld_s;
} ii_iccr_u_t;

/************************************************************************
 *									*
 *  This register allows the maximum timeout value to be programmed.    *
 *									*
 ************************************************************************/

typedef union ii_icto_u {
	u64 ii_icto_regval;
	struct {
		u64 i_timeout:8;
		u64 i_rsvd:56;
	} ii_icto_fld_s;
} ii_icto_u_t;

/************************************************************************
 *									*
 *  This register allows the timeout prescalar to be programmed. An     *
 * internal counter is associated with this register. When the          *
 * internal counter reaches the value of the PRESCALE field, the        *
 * timer registers in all valid CRBs are incremented (CRBx_D[TIMEOUT]   *
 * field). The internal counter resets to zero, and then continues      *
 * counting.                                                            *
 *									*
 ************************************************************************/

typedef union ii_ictp_u {
	u64 ii_ictp_regval;
	struct {
		u64 i_prescale:24;
		u64 i_rsvd:40;
	} ii_ictp_fld_s;
} ii_ictp_u_t;

/************************************************************************
 *									*
 * Description:  There are 15 CRB Entries (ICRB0 to ICRBE) that are     *
 * used for Crosstalk operations (both cacheline and partial            *
 * operations) or BTE/IO. Because the CRB entries are very wide, five   *
 * registers (_A to _E) are required to read and write each entry.      *
 * The CRB Entry registers can be conceptualized as rows and columns    *
 * (illustrated in the table above). Each row contains the 4            *
 * registers required for a single CRB Entry. The first doubleword      *
 * (column) for each entry is labeled A, and the second doubleword      *
 * (higher address) is labeled B, the third doubleword is labeled C,    *
 * the fourth doubleword is labeled D and the fifth doubleword is       *
 * labeled E. All CRB entries have their addresses on a quarter         *
 * cacheline aligned boundary.                   *
 * Upon reset, only the following fields are initialized: valid         *
 * (VLD), priority count, timeout, timeout valid, and context valid.    *
 * All other bits should be cleared by software before use (after       *
 * recovering any potential error state from before the reset).         *
 * The following four tables summarize the format for the four          *
 * registers that are used for each ICRB# Entry.                        *
 *									*
 ************************************************************************/

typedef union ii_icrb0_a_u {
	u64 ii_icrb0_a_regval;
	struct {
		u64 ia_iow:1;
		u64 ia_vld:1;
		u64 ia_addr:47;
		u64 ia_tnum:5;
		u64 ia_sidn:4;
		u64 ia_rsvd:6;
	} ii_icrb0_a_fld_s;
} ii_icrb0_a_u_t;

/************************************************************************
 *									*
 * Description:  There are 15 CRB Entries (ICRB0 to ICRBE) that are     *
 * used for Crosstalk operations (both cacheline and partial            *
 * operations) or BTE/IO. Because the CRB entries are very wide, five   *
 * registers (_A to _E) are required to read and write each entry.      *
 *									*
 ************************************************************************/

typedef union ii_icrb0_b_u {
	u64 ii_icrb0_b_regval;
	struct {
		u64 ib_xt_err:1;
		u64 ib_mark:1;
		u64 ib_ln_uce:1;
		u64 ib_errcode:3;
		u64 ib_error:1;
		u64 ib_stall__bte_1:1;
		u64 ib_stall__bte_0:1;
		u64 ib_stall__intr:1;
		u64 ib_stall_ib:1;
		u64 ib_intvn:1;
		u64 ib_wb:1;
		u64 ib_hold:1;
		u64 ib_ack:1;
		u64 ib_resp:1;
		u64 ib_ack_cnt:11;
		u64 ib_rsvd:7;
		u64 ib_exc:5;
		u64 ib_init:3;
		u64 ib_imsg:8;
		u64 ib_imsgtype:2;
		u64 ib_use_old:1;
		u64 ib_rsvd_1:11;
	} ii_icrb0_b_fld_s;
} ii_icrb0_b_u_t;

/************************************************************************
 *									*
 * Description:  There are 15 CRB Entries (ICRB0 to ICRBE) that are     *
 * used for Crosstalk operations (both cacheline and partial            *
 * operations) or BTE/IO. Because the CRB entries are very wide, five   *
 * registers (_A to _E) are required to read and write each entry.      *
 *									*
 ************************************************************************/

typedef union ii_icrb0_c_u {
	u64 ii_icrb0_c_regval;
	struct {
		u64 ic_source:15;
		u64 ic_size:2;
		u64 ic_ct:1;
		u64 ic_bte_num:1;
		u64 ic_gbr:1;
		u64 ic_resprqd:1;
		u64 ic_bo:1;
		u64 ic_suppl:15;
		u64 ic_rsvd:27;
	} ii_icrb0_c_fld_s;
} ii_icrb0_c_u_t;

/************************************************************************
 *									*
 * Description:  There are 15 CRB Entries (ICRB0 to ICRBE) that are     *
 * used for Crosstalk operations (both cacheline and partial            *
 * operations) or BTE/IO. Because the CRB entries are very wide, five   *
 * registers (_A to _E) are required to read and write each entry.      *
 *									*
 ************************************************************************/

typedef union ii_icrb0_d_u {
	u64 ii_icrb0_d_regval;
	struct {
		u64 id_pa_be:43;
		u64 id_bte_op:1;
		u64 id_pr_psc:4;
		u64 id_pr_cnt:4;
		u64 id_sleep:1;
		u64 id_rsvd:11;
	} ii_icrb0_d_fld_s;
} ii_icrb0_d_u_t;

/************************************************************************
 *									*
 * Description:  There are 15 CRB Entries (ICRB0 to ICRBE) that are     *
 * used for Crosstalk operations (both cacheline and partial            *
 * operations) or BTE/IO. Because the CRB entries are very wide, five   *
 * registers (_A to _E) are required to read and write each entry.      *
 *									*
 ************************************************************************/

typedef union ii_icrb0_e_u {
	u64 ii_icrb0_e_regval;
	struct {
		u64 ie_timeout:8;
		u64 ie_context:15;
		u64 ie_rsvd:1;
		u64 ie_tvld:1;
		u64 ie_cvld:1;
		u64 ie_rsvd_0:38;
	} ii_icrb0_e_fld_s;
} ii_icrb0_e_u_t;

/************************************************************************
 *									*
 *  This register contains the lower 64 bits of the header of the       *
 * spurious message captured by II. Valid when the SP_MSG bit in ICMR   *
 * register is set.                                                     *
 *									*
 ************************************************************************/

typedef union ii_icsml_u {
	u64 ii_icsml_regval;
	struct {
		u64 i_tt_addr:47;
		u64 i_newsuppl_ex:14;
		u64 i_reserved:2;
		u64 i_overflow:1;
	} ii_icsml_fld_s;
} ii_icsml_u_t;

/************************************************************************
 *									*
 *  This register contains the middle 64 bits of the header of the      *
 * spurious message captured by II. Valid when the SP_MSG bit in ICMR   *
 * register is set.                                                     *
 *									*
 ************************************************************************/

typedef union ii_icsmm_u {
	u64 ii_icsmm_regval;
	struct {
		u64 i_tt_ack_cnt:11;
		u64 i_reserved:53;
	} ii_icsmm_fld_s;
} ii_icsmm_u_t;

/************************************************************************
 *									*
 *  This register contains the microscopic state, all the inputs to     *
 * the protocol table, captured with the spurious message. Valid when   *
 * the SP_MSG bit in the ICMR register is set.                          *
 *									*
 ************************************************************************/

typedef union ii_icsmh_u {
	u64 ii_icsmh_regval;
	struct {
		u64 i_tt_vld:1;
		u64 i_xerr:1;
		u64 i_ft_cwact_o:1;
		u64 i_ft_wact_o:1;
		u64 i_ft_active_o:1;
		u64 i_sync:1;
		u64 i_mnusg:1;
		u64 i_mnusz:1;
		u64 i_plusz:1;
		u64 i_plusg:1;
		u64 i_tt_exc:5;
		u64 i_tt_wb:1;
		u64 i_tt_hold:1;
		u64 i_tt_ack:1;
		u64 i_tt_resp:1;
		u64 i_tt_intvn:1;
		u64 i_g_stall_bte1:1;
		u64 i_g_stall_bte0:1;
		u64 i_g_stall_il:1;
		u64 i_g_stall_ib:1;
		u64 i_tt_imsg:8;
		u64 i_tt_imsgtype:2;
		u64 i_tt_use_old:1;
		u64 i_tt_respreqd:1;
		u64 i_tt_bte_num:1;
		u64 i_cbn:1;
		u64 i_match:1;
		u64 i_rpcnt_lt_34:1;
		u64 i_rpcnt_ge_34:1;
		u64 i_rpcnt_lt_18:1;
		u64 i_rpcnt_ge_18:1;
		u64 i_rpcnt_lt_2:1;
		u64 i_rpcnt_ge_2:1;
		u64 i_rqcnt_lt_18:1;
		u64 i_rqcnt_ge_18:1;
		u64 i_rqcnt_lt_2:1;
		u64 i_rqcnt_ge_2:1;
		u64 i_tt_device:7;
		u64 i_tt_init:3;
		u64 i_reserved:5;
	} ii_icsmh_fld_s;
} ii_icsmh_u_t;

/************************************************************************
 *									*
 *  The Shub DEBUG unit provides a 3-bit selection signal to the        *
 * II core and a 3-bit selection signal to the fsbclk domain in the II  *
 * wrapper.                                                             *
 *									*
 ************************************************************************/

typedef union ii_idbss_u {
	u64 ii_idbss_regval;
	struct {
		u64 i_iioclk_core_submenu:3;
		u64 i_rsvd:5;
		u64 i_fsbclk_wrapper_submenu:3;
		u64 i_rsvd_1:5;
		u64 i_iioclk_menu:5;
		u64 i_rsvd_2:43;
	} ii_idbss_fld_s;
} ii_idbss_u_t;

/************************************************************************
 *									*
 * Description:  This register is used to set up the length for a       *
 * transfer and then to monitor the progress of that transfer. This     *
 * register needs to be initialized before a transfer is started. A     *
 * legitimate write to this register will set the Busy bit, clear the   *
 * Error bit, and initialize the length to the value desired.           *
 * While the transfer is in progress, hardware will decrement the       *
 * length field with each successful block that is copied. Once the     *
 * transfer completes, hardware will clear the Busy bit. The length     *
 * field will also contain the number of cache lines left to be         *
 * transferred.                                                         *
 *									*
 ************************************************************************/

typedef union ii_ibls0_u {
	u64 ii_ibls0_regval;
	struct {
		u64 i_length:16;
		u64 i_error:1;
		u64 i_rsvd_1:3;
		u64 i_busy:1;
		u64 i_rsvd:43;
	} ii_ibls0_fld_s;
} ii_ibls0_u_t;

/************************************************************************
 *									*
 *  This register should be loaded before a transfer is started. The    *
 * address to be loaded in bits 39:0 is the 40-bit TRex+ physical       *
 * address as described in Section 1.3, Figure2 and Figure3. Since      *
 * the bottom 7 bits of the address are always taken to be zero, BTE    *
 * transfers are always cacheline-aligned.                              *
 *									*
 ************************************************************************/

typedef union ii_ibsa0_u {
	u64 ii_ibsa0_regval;
	struct {
		u64 i_rsvd_1:7;
		u64 i_addr:42;
		u64 i_rsvd:15;
	} ii_ibsa0_fld_s;
} ii_ibsa0_u_t;

/************************************************************************
 *									*
 *  This register should be loaded before a transfer is started. The    *
 * address to be loaded in bits 39:0 is the 40-bit TRex+ physical       *
 * address as described in Section 1.3, Figure2 and Figure3. Since      *
 * the bottom 7 bits of the address are always taken to be zero, BTE    *
 * transfers are always cacheline-aligned.                              *
 *									*
 ************************************************************************/

typedef union ii_ibda0_u {
	u64 ii_ibda0_regval;
	struct {
		u64 i_rsvd_1:7;
		u64 i_addr:42;
		u64 i_rsvd:15;
	} ii_ibda0_fld_s;
} ii_ibda0_u_t;

/************************************************************************
 *									*
 *  Writing to this register sets up the attributes of the transfer     *
 * and initiates the transfer operation. Reading this register has      *
 * the side effect of terminating any transfer in progress. Note:       *
 * stopping a transfer midstream could have an adverse impact on the    *
 * other BTE. If a BTE stream has to be stopped (due to error           *
 * handling for example), both BTE streams should be stopped and        *
 * their transfers discarded.                                           *
 *									*
 ************************************************************************/

typedef union ii_ibct0_u {
	u64 ii_ibct0_regval;
	struct {
		u64 i_zerofill:1;
		u64 i_rsvd_2:3;
		u64 i_notify:1;
		u64 i_rsvd_1:3;
		u64 i_poison:1;
		u64 i_rsvd:55;
	} ii_ibct0_fld_s;
} ii_ibct0_u_t;

/************************************************************************
 *									*
 *  This register contains the address to which the WINV is sent.       *
 * This address has to be cache line aligned.                           *
 *									*
 ************************************************************************/

typedef union ii_ibna0_u {
	u64 ii_ibna0_regval;
	struct {
		u64 i_rsvd_1:7;
		u64 i_addr:42;
		u64 i_rsvd:15;
	} ii_ibna0_fld_s;
} ii_ibna0_u_t;

/************************************************************************
 *									*
 *  This register contains the programmable level as well as the node   *
 * ID and PI unit of the processor to which the interrupt will be       *
 * sent.								*
 *									*
 ************************************************************************/

typedef union ii_ibia0_u {
	u64 ii_ibia0_regval;
	struct {
		u64 i_rsvd_2:1;
		u64 i_node_id:11;
		u64 i_rsvd_1:4;
		u64 i_level:7;
		u64 i_rsvd:41;
	} ii_ibia0_fld_s;
} ii_ibia0_u_t;

/************************************************************************
 *									*
 * Description:  This register is used to set up the length for a       *
 * transfer and then to monitor the progress of that transfer. This     *
 * register needs to be initialized before a transfer is started. A     *
 * legitimate write to this register will set the Busy bit, clear the   *
 * Error bit, and initialize the length to the value desired.           *
 * While the transfer is in progress, hardware will decrement the       *
 * length field with each successful block that is copied. Once the     *
 * transfer completes, hardware will clear the Busy bit. The length     *
 * field will also contain the number of cache lines left to be e is sub*
 * transferred. is subjjitions of the GNU General Publicct tLicense.  Seect 	 file "C theNG" inthe Gmain directoryof theis archivect tfor more details.
 ect tCo/

typedef union ii_ibls1_u {
	u64- 1997, 20regval;
	struct0-20005 Si_length:16;rights reerror:1*/

#ifndersvd_1:3IA64_SN_SHbusyM_IA64_SN_SHUBIO:4#def}Silicon Grfld_s;
e HUB_WIDGEu_t;

/G" in the main directory of this archive
 * for more details.
 *
 * Copphe Gile "COOPYIN  Tis aregister shouldile loadet Idfre datothe Gt is sutartms aThl is  thetaddressisID			ntificin dbits 39:00	/*he G40-bit TRex+ physicald condiso acciblebleas describ			 * Sect92 -1.3, Figure2 ands */
#d3. Sincis al lso acohe bottom 7 Crossofdget 0x0040000	IIOlways takene from
zero, BTEO_WSTAT								/*er	IIO_Wefinecacheline-alignistend condtions of the GNU Generthe GD			0x004000UM_ITTES		7
#define HUB_NUM_BIG_WINDOW	(IIO_NUM_ITTES - 1)

#define		IIy/

#i (C)ilic2 Silicosarigh rigts rlico	IIOaphics, Inc. All /

#ifnde_H

_H
7IA64_SN_SH0x00:3rotecti _AS_H

#24004000xf
#118	T_ID_MAX	0xf
#118	_e #defNUM_ITTES		7Protectiund DIO_IBIG_WINDOW	(e		IIO_ILCSR	defi)
Protecti	
#defWID			Register 0	/*dget Ctalk Widgicati
							ions	IIO		IIO_I/*0	/* Crosstalk s arlso ac0x004000 from		IIO_II* CoIO LLP Loat cessibl 0x0. ss Protectie		IIO_STATR		0x00400180	/* IO LLP Log RegisStatu		IIO_WO Graphics NCRLR		0x00400230	/* IO LLP Log RegisControl Rosstalk 400148	/* IO GraILAPRR		0x00401130	/*IO Local Arrupt Protec#defi0		0x00400150	/* IO Scratch ROgister 0 */0 */
ine		IIO_ISCR1		0x00400158	Override00150	/* IO ScratcOWAgister 0 *1
#defineOutboundog RegisISCR1		400148	/* IdcratcIne		IIO_ITT 2 *TTE1		In8	/* IO Translation Table EntryScratcIDEMgister 0 *get Ma	0x00400170De_ITT Error Mask */_ITTEine		IIO_ILCSR		0x00400128	/* IO LLP Control and Status Register */
#define		IIO_ILLR		0x0040013Writinge frt	/* IO LLP Logets up_ISCR0ttribute	IIO_ISCRIIO_IIDSR IO Interrn

#ditiay 6 148	/* IO Graoperaegis. Read		0x0	/* IO LLP Lohase-Widget Map 1 *TableffectIIO_IerminaR		0xany	IIO_IIDSR	n prog0400. Note:	IIO_WSTAT		stopptecti Graphics midstream cRegishave an adverse impantrynO Traidget Maother 0 *. If a 0 */R		0x00RB0	 from
40	/*ed (duey A f0120ne		IIO_ITTE1		 handl		0xor mexample), LLPhO_ITTE4PRB sog Register00148	/*gist	IIO_WSTAT		oheire		IIO_ISCRdiscard#define		IIO_ILAPO		0x00400108	/*al Public
 * LIO Local Access Protection Override */
#define		IIO_IOWA		0x00400110	/* IO Outbound Widget Access */
#define	ctIO_IIWA		0x004001	0x/* IO Inbound Widget Acces Entrill64_SN_SHUBIO_H

_200400128	_ASnotife		IIO_IMEM		0H

_H004001E8	/* poisoncellaneous Error#550128	/* IW Cro Error Mask */
		0xine		IIO_ILCSR		0x00400128	/* IO LLP Control and Status Register */
#define		IIO_ILLR		0x00400130	/* IO LLP LocontainIefinerupt DestinwhichefineWINV0	/* entefine		Iso ac0	/* 0x004000*/
# A   *58	/* ssta cratch Register 1 */

#define		IIO_BE		0x004001D0	/* IO PRB Entry E   */
#define		IIO_IPRBF		0x004001D8	/* IO PRB Entry F   */

#define		IIO_IXCCn2 */
#define		IIO022defi	0x00400170	/* IO Translation Table Entry 3 */
#define		IIO_ITTE4		0x004001g Er Error Mask */g Er	00150	/* IO ScratcECLegister 0 *F_ITTE1		E _AS Clear	/* IO Scratch Register 1 *BhicsRegiste2/
#defineBTE	IIO_ISCR0		0x00400gisteammaDestlevelp 0 wel		0x0finenodle Eso acIDRB EnPIilicssag0 or PaS0x00or IO ScratcXSM	interrupt willile "s subry 8  O_ITD			0x0040000ocal Access Protection Override */
#define		IIO_IOWA		0x00400110	/* IO Outbound Widget Access */
#define	i2 */
#define		IIOGrap/* IO Inbound Widget Accespi_id64_SN_SHUBIO Tabstal8ne		IIO_IMEM		0H
ine		IIO_IPrruptror PaWrappe	/* Id4_SNound DeGrap Error Mask */Graphine		IIO_ILCSR		0x00400128	/* IO LLP Control and Status Register */
#define		IIO_ILLR		0x00400130	/* IO LLP Lo48	//*
 gistresourcrt A_IGFfe/

#dforme Erupt Roe-Widget Map 1 two per */
#dx00cou Regs locat/

#degist_IPRs Tcal AEntster 0 *B_ITTE1PIIO_W*/
#RO LLP L38	/* Tabre 17 differ08	/quantitie Entry can */
#deso acmeasuristeGivratchesry 2, Pae		II*/opegiss,ead A0		0x00PRTE2_A		ILCSR7 */
ry 1ss Taror P15IIO_ISCm */
common; menu sel Regiss 0 through 0x*/
#define*/
#ier   le Ery B n Re#define		IIO	0x0040. As40033gistRBe		Iessie Entry PRry 3ss Tone0	/*availIIO_IinatPRTE6IO PIO Read Addressne		I Graphi/
#define, Par_e		IIO_ITT330 */
 Add5ss Ta_IPRIOy 7 * A */
#defHence Table En*/
#deI support IO l 17*16=272 p/* IIO_ISomb00148O_IPRf00148	/* IO GEntry148	/*3get Ma01C8	/* IO PRB Entry D   */
#define		IIO_IPRBicense.  See O ReaIO Local Access Protection Override */
#define		IIO_IOWA		0x00400110	/* IO Outbound Widget Access */
#definepcrO_IIWA		0x0040B *//* IO Inbound Widget Accesippr0_c:x004IO_IPR IO P1t A */
#define	cctIWEI/
#define	26:4fine8	/* I_IPR Error Mask *_IPR
ine		IIO_ILCSR		0x00400128	/* IO LLP Control and Status Register */
#define		IIO_ILLR		0x00400*/
#define		IIO0x0040034t 0x004003t d Add0	/* IO Scratc IO 2_Be		IIO_IPR5_ITTE1		* IO PIO Read Addr Part B */
ess TablBpB		0x00400378	/*p IO 3O Read Addre6IO_ITTE4* IO PI:32*/
#define		IIOss Tab IO PIOps Table Entry 6,O Reine		IIO_ILCSR		0x00400128	/* IO LLP Control and Status Register */
#define		IIO_ILLR		0x004001	/* follow*/
#dss TablScratcwE2_A	ot_IPRm/

#dtole Euctudef  */ister probably inine		IIO_to an/
#defIO LLP L,art B */
nameIIO_ISCister */O LLP Loisectiv					agossta3IO_IPRO_ISCsB		0 LLP L138	/* gister ine		IIO_IPRneedsagble Enhecked carefully*/
#define		I#define	Bne		IICRB1_AO_ICTOT#define	 IO ScratcB8	/* IBO_IPR18	/* IB CRB Timeout */
/
#Cefine		IIO_ICTP		0x004003C0	/* IDefine		IIO_IDTP		0x004003C0	/* IEefine		IIO_IETP		0x004003C0	/*2IIO_IPR18	/* IO */
#define		IIO_I0	/* IO ScratcCTPe		IIO_IPRC30	/200408	/* IO CRBPrescala40015	0x02B */
#definRBe		IIO_ICCR	4130	/*20408	/ B */
0_ress Table e Entry3CRB0_B		0x00400408	/* IO CRB Ent		0x0040/
#define		IIO_ICRB0_C		0300400410	/* IO CRB Entry 0_C */
3define		IIO_ICRB0_D		0x00400418	3* IO CRB Entry 0_D */
#define		I4CRB0_B		0x00400408	/* IO CRB Entdefin 0_E */

#define		IIO_ICRB1_400400410	/* IO CRB Entry 0_C */
CRB Timeout _ICRB0_D		0x00400418	4* IO CRB Entry 0_D */
#define		I5CRB0_B		0x00400408	/* IO CRB Ent5y 0_B */
#define		IIO_ICRB0_C		0500400410	/* IO CRB Entry 0_C */
5_D */
#define		IIO_ICRB1_E		0x005* IO CRB Entry 0_D */
#define		I6CRB0_B		0x00400408	/* IO CRB Ent6y 0_B */
#define		IIO_ICRB0_C		0600400410	/* IO CRB Entry 0_C */
6_D */
#define		IIO_ICRB1_E		0x006* IO CRB Entry 0_D */
#define		I7CRB0_B		0x00400408	/* IO CRB Ent7y 0_B */
#define		IIO_ICRB0_C		0700400410	/* IO CRB Entry 0_C */
7_D */
#define		IIO_ICRB1_E		0x007* IO CRB Entry 0_D */
#define		I8CRB0_B		0x00400408	/* IO CRB Ent8y 0_B */
#define		IIO_ICRB0_C		0800400410	/* IO CRB Entry 0_C */
8_D */
#define		IIO_ICRB1_E		0x008* IO CRB Entry 0_D */
#define		I9CRB0_B		0x00400408	/* IO CRB Ent9y 0_B */
#define		IIO_ICRB0_C		0900400410	/* IO CRB Entry 0_C */
9_D */
#define		IIO_ICRB1_E		0x009* IO CRB Entry 0_D */
#define		IACRB0_B		0x00400408	/* IO CRB EntAy 0_B */
#define		IIO_ICRB0_C		0A00400410	/* IO CRB Entry 0_C */
A_D */
#define		IIO_ICRB1_E		0x00A* IO CRB Entry 0_D */
#define		IBCRB0_B		0x00400408	/* IO CRB EntBy 0_B */
#define		IIO_ICRB0_C		0B00400410	/* IO CRB Entry 0_C */
B_D */
#define		IIO_ICRB1_E		0x00B* IO CRB Entry 0_D */
#define		ICCRB0_B		0x00400408	/* IO CRB EntCy 0_B */
#define		IIO_ICRB0_C		0C00400410	/* IO CRB Entry 0_C */
C_D */
#define		IIO_ICRB1_E		0x00C* IO CRB Entry 0_D */
#define		IDCRB0_B		0x00400408	/* IO CRB EntDy 0_B */
#define		IIO_ICRB0_C		0D00400410	/* IO CRB Entry 0_C */
D_D */
#define		IIO_ICRB1_E		0x00D* IO CRB Entry 0_D */
#define		IECRB0_B		0x00400408	/* IO CRB EntEy 0_B */
#define		IIO_ICRB0_C		0E00400410	/* IO CRB Entry 0_C */
E_D */
#define		IIO_ICRB1_E		0x00E* IO CRB Entry 0_D */
#dcal Access Protection Override */
#define		IIO_IOWA		0x00400110	/* IO Outbound Widget/so acSl

#ily frieble erParamTE2_A	som*/
#dmon08	/0		0x00
	/* I_IFDR	 efin#defiTtions of the G Entry IIDSRg RegiE5ine		ficIO_IPRy 80368		0x00400378	/*STAICTO		8_C	 Entr40050400408	/Ens		0xefine		IIO__D */
#define8_D */
#dCTRL90x00400408	/ECR 8_E */

#dnSCR0rol004005a0	/E */
#defi5a0	/PROTEC598	/* IO */
#efineh Re0400IOpt Regfac40024tO_IGFX1ine		IIO_ICR */
9y 0_D_OVRRDD */
#define9_BO*/
#def */
#de oion le Eine		IIO_ICRB8_OUT* IO CRACCESS/
#defineOWA04000168	/* Iw*/
#deac30	/_D */
#define9_DINO CRB Ec0x0040040ICTO		Ie		IIDIn	0x00400378	/* 05d0	/* IO CRB Edntry 7EV_ERR_MASK_A		0x004003 */A_A		0x00400device */
B		mask_E		0x004005d0	/LLP_CSRO_ICRBA_A		0x004005CS		IIOLLP5d0	e		IIO_ITEntry 1A		IIO_ICRB8_D		0x0LOGO_ICRBA_A		0x004005L_D */
#delogA		0x00400A/* IOXTALKCC_TOU598	/* IOefineXCC0400XControredi05d0unt t004003_A		0x00400Ae0	/* IO TT6/
#defineIO CRB EnTine	D_C */
ailsRBBine		IIO_IP62		0x0040IOEntryCL0	/* e		IIO_ICRBECA_D */IOO CRB Eclea0	/* IO CRB EntryIGFX_0IO_ICTOTGFX0fine		B_CIIO_ICRB81_A		0x004001_D		0x00400628BCT_0		IIO_IBCTB/* IO CRIO C28 Ent100400Be0	/ B	0x00	0x004003LS_ICTO		B_ELS	/* IO CIO CRBIOLSO CRB EntBLS */

#define		IISAICRBC_A		0SA0400640	/* IO CRSAEntry C_A SA */

#define		IIDBCO Read AdD064ry 8_E */

#dnDry C		IIO_IDCRBA_A		0x00400C_N		0x0040065N	/* IO CRB Entry N_C */
#defiNe		IIO_ICRBC_D		0I		0x0040065I	/* IO CRB Entry efin*/
#defiIe		IIO_ICRBC_D		OPRM		0BC_A		PRB0
B */
#define		ITE_A(_x)		(/
#defiTEry 0+ (800400_I))y Dy 0_D */
#defin	/* IO CDO Read AddB67 IO CRB Entry  */
D		IIO_NUMIO_ICR		8O CRTotal numbon Tfable tIIO_IMess_E *ine		IIO_ICRB8_D		 0_E */x)/* IO 0_E */((x) - 8))O CR0_E */
ID_IPRne		 EntD_D	B Entr

#define		II	/* IOIO_IC/* IO CRB069 B_A */
#define		 D_Entry 7, P 0_E */

#dne		IIO_ICRB40062try S 			IIO_ICRB4_E	E_		0x004_IS_U		IIO_Ifine	0x00400630	/* 	/* IO C118	RB E	0x00400378 /
#def3RBE_C		400628	/* A_A		0x00400E_D	SHF4005a0	/12fine		IIO_Ie		IIO04Bor MbIWELfffO CRin y A_ CB_CNT, Max C	/* IBitO CRB x00400610	/* 00400630SNEBE_B		0x004006a8	/* IOSSN */
#defi7Sequ#def N* IO CRB Spuriou
/* key_IPR_E		0x004005d0	C		9		IIO_ICRB8_D		0x00SMH		0x0_KEY6*/

53474972756c6573ullO CR"SGIrules"iddlble 0 */ne		IIO_I05a0	/ine		IIO_ICRB8_BTEIO_ICR#define	/* IO CRSpuriodefilsDeptDeserved//
#defi0l theB		0x004006a8	/*RCLSIO PRB E10_ICRBC_D		0LAPOLerved St004006 Middls /
#defn RegstSAIO PRB EDESB08	/* IO BTE0130	/e		IIess 0 */
#defist.IIO_IBDA0	0x00410010	/* IO BTEtry tination Address 000410ess 0 */
#detry A_C/C0	/* Ide001_ITTE1		LAPO 		0x00NOTIFY08	/* IO BCx00400630	ess 0 */
#deIO Mle Enrotection Regst	/* IO BTEINstination Add18	/* IO C6x00410028	/* t Regi0		0upt Address 0 */
#defOFF00410010LS1		 */

#asmetefsetTE7_A	0 */
ebugs. 1E Interrupt Address 011   O_ICRBDry 9_C-00130	/*/* IP		0OSefine A */bion t*/
#de17otificatiD */
 Subme 1 */
s148	/* IO Gine		IIO_ICBTE0042RB Etry 8_E *0e HiIIO_ICRB8_D		0x00RC042002O_ICRBon Addre8	stin */
	0x00420 CRB Entry0x00410 StadefiA */
#de1 * Statinction RegstIA008	ntryon Atificatio CRB En8rupt AddresT  */


#define		IIO_IPCR		0x00430000	/ */
##er		0xanc 6 ** IO Potection RegstIIO_IPCR		0x00430000	/IN4005a0	rupt Addre
#defi/

/**************ble 05a0	/us/

#deshub diagene		IIO_IBSA0		0xASE0x004		0xnation Ad	/* Crosstalk echoesress ication AD* IO PerMacB Entper Iakable En0x0040053_IPCitratireturstalk ing *O006880x004000 ManaasstaO CRB Entvalue _xICMRexpecment 8	/* a*/
#dfine		IIOng */

/rangect t0, 8 - 0xF Perf		IIO_ICRB8_0670	Spuri_ICRBDREV_/*0400ct t_x) <und D* IO CRrrorIN ? \
tions of the GNU	he PA PRBhe Gble Entry 0,/* I I- (IO L field'selds -1)) << 3) *ect 	GFX Flow Cry A_C Node/ntry 9_CA */
#dEntry C_A try E_D */
W_B */
ITS	4IIO_IizTE Sr      *
 *r assi0	/*ect ***********/

typedng *	((1<<***********/

typed)-1 CRB Entry D_C***********SHIFT	0x00400630	/* IO/* PI**********1***********/:11;
		u64 wv_num:4;
		u64 w_r/

:11;
		) 1992 - 19wid000-232;
	} u64 d_aphics, Inc. Al0-20	32;
	} UBIO_H4******************Nv_num:4;
	is avv_num:4; Table64 w_rev_num:4;
		u64 w_rsvd:	*
 * ii_wid_fld_s;
} ii_	*
 *  The**********************
 *				BIO_H5	*
 *  The fields id_u_t;

/w_part_num.
 *024ation Aned inrev     4plained iny va:3;
	}fld_s;
}T_ID_MA      d_ne IIO_	*
 *  The fields in thred bieldri16******************INIT(MFG_NU, pi,	IIO_, cpu)CRB7(\
	((4 ii_wid &64 w_rsvd:ght (C) 19e SHu*********          _) |			*
 * pi)	0x00 (C) 1992 -32         )2005 fld_sst{
	u64 ii_wid********* Tabed i (C) 1992 -talk are s0-2005 1;
		u6a*********************	*
  in ending.     w_tx_mx_rty:xplained in
	u64 ii_widhip.  Scratch* IO CRB E (0040fine			IIO_IPR)ne		IIO_IBSA0		0SCRATCH_REG */

/********SCRdefiinatumber		*
 *  The 
 * L *									* */

#define		*********	/* IO Cb8ry 7, Pncal dCrosstUL	IIO_ICRBE_C		 *						BIT008	/* IO/
#deff the Gfine1UL Itrol Rnage ol. ect t*****hip.  ect of the Gfine	2P Lof    ow cont.ol.  ******2*********      	IIO_II	4*
 *************************3***********************8*
 *************************4**********************10*
 *************************5**********************2g64 w_crazy:1UBIO_H
8_u_t;

/6**********************4ad_pkt:1;
		u64 w_dir_con:1;
7**********************8ad_pkt:1;
		u64 w_dir_con:1;
8*********************10ad_pkt:1;
		u64 w_dir_con:1;
9*********************2 *									*
****************ge */******************4			*
  flow control.         1_IBDAasp     *
 *								LP Lo**********************0010t guards      *
 * avector0630	/Tumbe*/
#de3TIIO_IEne		IIO_ICRB

#define		IIO_TTEn Ad7CRB EntEsexternaed 0..6e 1dres7_/* Hw manualCrossonn AdRea1..7!s ofCRB EnefineMEM is a bit  *			IO Perf		IIO_ICRB8_lds _W0ESDalk wUL 8_D */
#de0Desct down Part A */
B		Er assiin the m     B LB_	(talkSHu4dresst Addrch#defdocumente.3.1.Widgfour8.3.1.1). T). Th1      px00408fourprovi1 S by this register appliesing ntersB0	/	/mane Enworkarng */C_A a bugng */

/PIable EIO_ISCR* Cr, we'vect trcessin*/
#dg window 7#defim0040'rol.  0.
 XXX doe		II	/* Cw_tx_pping or SN1??IZE field in IO L *				Control osstal fourn the- 1e is r asUs avO Reopestorion is a Iart rrogLAPOrt B */
first for nd willsIZE field in SWIN0 is WINtions of thed********** TableIO Lot A l 328	/_WARM_RE_ICRBC_A8_Cefineons ad  CRBor  ipuone		lk WIt s2_A		/* e ch R * a IO Psy 0_D */compl
#defi, tat_u_, Paing onfouro2_A	fou****IQ Arb
#desoc_num/
#dthIO CRBtargdresySIZE field in the  */
#0040      he GI15, twIBDA0esofrequ    *articutry argetsPCnly    *
 *the	*
 ***turnO PResponaling */

* IO respose.  ess 8	/*ss 0n a Pargets 8stalk Wuppleregis0x00400420008350	B_REtion Arovided    *
 * byADDRIO_IE	2ill Shi	/* Cf* IOpr/
#dd    REV_
		u6;PRBA-P LoSpis now tutionl :umber tha		IIO_ICRB8_FIRSThe GENTRYIO C80	/* IO CRB EnE_ernalB0_Areg(u64)_ICRBD*****t+ (6o the mv* byhe Gpro400408	/**************** by_ame resstalk(chdres)ll ret0002ame  + 1*  *
 * byccessehthe IIAlias space bC valubit fopro0	/* Is************ is 2    resetelds ref this arosstalk allDws access by all nodes.         *
 *	3							*
 **************************Ews access by all nodes.         *
 *	4							*
 *********field in Tion i	/* IO CRDEV(_tnum)	       & 0x7O PROWA rlds rTE2_A	"ecode"REGIONIZtion 8.3.1.1). T000-2COD					efine	ntry 7rectory  in Sec Entry 3IOx004005e0	/* IO CRB Entralue is a Pthe regin thePx0041 in Secon abo**********f the G64s 0x0lds re   Wthe regiswidgTE6	*/
#defin al*********f one, * e.g. IO CRB 3Ay 7
 * nly0x004otection RegstDA1the ILAPR rAthe regi3tion 04005e6get Maaectorp	IIOngO Locaause the bit in the ILAPR r 		0x00400GION_ll rASCIvid*****wect t          *
 * protection fics e REster II wis S		0x00400rO Lothrough the IAlias spand MANation Ad6ill targ400618	/#definedealble Eis aowias space.   errupt). A XTis regisystemnc004005xC */
pk8	/*ne		rgistiPCR		*******V	*
 *  The fieldimsg Acc	*
 *  The fields in tIMSGTis a rector tDeh Re*****MeessagRTE7_A	_C */
s a bit vector **************
    *
 *"S*********m***/

typedef union ****************/

typedeSN1NSpuri     po000-2005 - 199lapo_rSN1 ne/
#define		IIO_ICB***/

typetarg*****any olapo_fld_s;
} ii_ilapotarg???, Inc	*
 *  The fields sstalk w */
#orS*********************/
N		u64 w_rev_******    *
 *origtion  for  SHUB fie
#defihe Gble fineGraphes somefourbelauncheonly an err********t Addrowards a widget.          de:6    } 0x                *
 *							SN1          *
 *									*
    	*
 *  Thi0x*****           *
 *							***/
towards a     et************	u64 i_wx_oster MEts. Each bit in the regi_par*********  *
 * pro/
#defs Hub***/

tyntrywhileSpur0_C *req/gisters ava****xbower as	u64 ian 3esse erri****by Xbow 1. ****W maiys
 *	IO_Isvd:nIt is av,cesso 4rform  Stacri2SIZE field in		ot be II_XBOW_CREDr's regioedef   *
 *					u64 i_ toREV2oac:1;
th4         iowa                   of mi_ITTE1ister 0 *use wh#defineuise, #ng****questa S    (depFG_NMae RE's queue)SIZE field in IO Lo be    This rus		0x0RB EntB******_IZE field in the     MULTdress	(1Lx004063 CRB Entry D_C#dB_SPUR_RD	iiby s {
5aphics, Inc. Alics, Inc.WRii_wstat_riIPCR		0x0043u64 i__RD_TOii_wstat_r49w0_iac64 w_crazyiA */O:7
} ii_iiw48fld_sIO_ILCSR		0x00400128	/* IO LLP Control and Status Register */
#define		IIion of IO_ISCRding***/

RB En Tablets. 4 i_'s					quet be.
eermissiein ordur nomainStatuhe IIatibilitry 3 SN* IO P    rites t {
		u64 w_          *
 * control for up to 8 devices per wit in the regiidget. A dive t/* IO BTE	*
 *  Thi(able Eion:  T_ of _		IIO IO Per hosisteC_VLDTE7_A	 registenotng */

O_IWSHub.Hub)

#dre eabled rn theMRu64  * Thstalk	020x00400630	/* I w_tx_mx_clC) 19920x7stalkfld_s;
} 
 * upon detrsvd ii_ilr    et.      Fe		IIeared byx								*
 *  This reg      lies tof* IO 	IIO_ rIO a*
 *disa * Stes file guards      ic   led r          *
 * . WheBA		 0=TNUM=7 (i.e., Br64 icMessic Whether fine		IIO)ss rPRECISE   *
 * o52f this register  ShuLR_RPP    *
 * o1_s;
} ii{
		regol.       Qppropriate fefin******dingble Dll not c		IIO_IPP		0*************: _ICRBD_DR)t can     Crosbu0188IOPCR	/

#defedrock? See ster r*****lifies all the PIO PDR_P0708	/	(1004001ed.  0x0in requeter  the Wx_IACion 8.3(or mhe Gwi int	IIOows is************_s;
} 1O Localm.nbouomed.    s perbo0 */L0x000			0x004fication )	/* IO BTE me t  *
 					i* IO CR1		ow contclea EBUSYASCIo *
 * o2wxfld_sr_con****     xternass          *
 * control for up to 8 devices per wiidem_regval;
LENGTs/* IO	*/
#defi     *
		u64 i_wx_oa/Tication Ane		IIO_I(    h the IAlias space.  in the regisidem_regval;
	CT_POISO*************8	u64 i_wd_dxs:rofilon:  This em_f4} ii_iiwawf_dxsZFIL_MODE devices per widge   *
 * *********can alPacket Hea err_ICRBDIEPH1       *
 2SCR1		*********;
		u_VALl;
	s *
 * ot          ii******OVERRU     ins the************urosstion iYPExterna3*****an error*
 *  Thl8_dxol       Whether e LLP sSOURCg     						Sscri***************ol   1 */

#defineving tUPPLxternaions ruleis regi

typedef union ii_ilcsr_u {
	CM the d	164 w_crazy:1_fld_s;
}ol   	7the bit for90	/*_TAIL****n_IIDEary             *2ld byESS} ii_cr**************i_iiway vaol   38nullto:6_u_t;

w contSHORT_REQ	atused. Ahe LLP signal       PLY	bad  ****he LLP signaLONG;
		u

typedef untry:10
} ii_iiwa64 w58dem_******can alEntry 4 i_wb_dxs:8;
		u64 i_wc_dxs:8;
		u64 i_wd_d0x00_PI1_FWDan er4:2;
		u3of on 400408it vaORWAR64 w_rsubiidx004es to PIO r       
 *									*
 an erssibl********* * control for up to 8 devices per widg Inc. A_HDf th  This 9s simply ahe Iiuestts inixsd by 8 devices per widg    d_4:2;
		u1must i400408b *       _IPCR

typedef union i**/

typedef7

typedef union ii_ilupt Address 0 e IIOCRAZ    *  This 6

typedef ucrazy********w/
#d

typedef union ii_ilcics,F.
 *l      ous mechanisefield  PRB 68_F      illto bevarious prllavail_sel:2;
erati

typedef union ii_il8	/* **********************ol Regi    13e is a bit vector t launcpDmaster WidII-de theed non-C

typedef u2errupts are           *
 *Cspecified via this registe***********ter address is hardcoded in Bspecified via this registeAlr_regval;
						*
 *		ter address is Aspecified via this registe9************ vector leared b80_0090 w9specified via this registe;
		u64 i_wf_ss space of PI0 on the noperacified via this registe*********e Enne		aill upt Regioperastefine	widget. A dst tarpltry A_C */
#05b0	F*****:stalk).64 i_rsvd:      
 * ffse_PENDING	filen extntry E withn the         0xFFrFX0		0x00ter alof PI1_3:1;7on the node specified NOble e proto Op0x01A0_0090s space of_iiwWAKE******ill Reactiv *
 *he II****y 8_B      *
		u64 i_wc_dxs:8;
		u64TIMEOU****ister'sMa*******400618	/& mar8_E	n */

#		u64 i_wc_dxs:8;
		u64EJEC1992ccess e SHU08	/IIO_s access  tratch memory				 fourvia a WB4;
		u64 	u64 i_wc_dxs:8;
		u64FLUSH	0x8 	*
 n th the mILu64 i_wc_	/* IO		IIORB E***
:ARNINGts;
} ii_iiwapi1_forac:1_in i_pi1_forwar      4 i_ous prodsr4 i_rsvd:30;
	d_int:1;
		u64 i_rsvd:30;
	} ii_iidsr_fld_s;
}              ******             *
 *									*
 *          *
 * control for up to 8 devices per widget. A devicehe SThe */
rell r iefinnce of the***********	u64 i_m400138	/* Ius******ing the LLm ii_8_dxupower:ism.     gister snses fnyripnt:1;******;
} ii_ill t_E		0x0fine	fileignohardwarect to peiregvregimd by ********e ILc****I    very dN anrouowerTE2_A		ter ng */

tr Inteeques*/
#define		upt Rd thOK *		idget/* IID==1,fr alcaart B *00400m****s
	} ibledw contnvertad          nor  ageUpd changID==1,nterrung */val;
	struc
 * 
		uhisues frquiesceder as/
#dewise,****ect to wilhe I intco Statu/
#def********ieCRB Efine rn manfileegisePIO oback doo340	ekn iilauncIIO'ng retugfx0_rQThis is ato g006b8no dmaidget toividuei, Part*
 *t*****y A_C *cpu00	/* ;
}sn0nller ass a ws  *
 0x0040h_C */
* IO P3_A	dIO Peasily. So, AVOID u	IIO_II_numphicO Perfre are Easyac:8;
ca to idgted OWA ,ng */5 w_rt       A-ESCR1		 Access iliccrb0_a   *     aine			IIO_ICa_sidnrrupt  * the  Erro.ograspng uinitiate    e gr
 *							g    ics*****et to   *
tion T * the I processor that initCR1		et to   *
dress *****on            *********v8.3.1initiateiow*
 * the II processor that initd tond returnet inituee processor t initet Access es from tb processob that initiab    _olon donve************hat inthrough the IAlias spac***************************************4 i_wc_dxs:8;
	rotection   by all re are two instances of thitalk quare are two instances of thni *									*exc thro*************/
***********excre are two ackcnt_t;

/_ids;
} ii_iiwan      ack_cn} ii_ition. re a_u_t;

/i_rsvd:46;
	} ii_igfx1      _u_t;

     r_u_t;

/i_rsvd:46;
	} ii_igfx1_flre are two h**********************************T       . Theswb*
 * the IIIrsvd:46;
	} ii_igfx1wb****gfx2000-2tv} ii_iidem_fl union ii_wstat_uirs fi_igfx1_u {
	all_ihe grap*********************on */

#hics credi*******t;

/**re are two instances of this rrite-s wids. Thesebteondi
	} ii_illr_fld_s;
on ii_wst******2 - 199scr0000-2005 0 */:64;
	********************      */

#defibePRBB	**************************
 *2	/* re are two instances of this register. This regiindivre are twolnetue		IIO_ITrsvd:46;
	} ii_igfx1ln_uche incoming s
		u**************************
 *crate incoming d. Th**************************
 *x indiv****** regisrol.           *s. These*****64 i_pi_      c processoc that initiac_		IIphics Nodhes4;
		usdeter*****scr1_u1992 - 199sbarro         ch:64;1
} ii_iscr0_u8	/*scratch:6do*********
 *i_iscr1_fld_s;
} ii_ef unqtalkin thec_gbgister 0 *B8i_iscr1_fld_s;
} ii_g***********c_s;
m_flddsr_fld_sscr1_*************te_a****or thatc_coh					d_s;
} iiscr1_fld_s;
} ii_c;
} ii_igfc_*******ld_s;
} iiscr1_fld_s;
} ii_uizhe incominc_/
#defindow to a 48-bit          *
*/
#dX0		0x00u64 									*
 *  This regil     **************d processod that initiad_sleeftware in cGBytes/nod        Ws    :29] (Big Wpri_rsvd:30;
	} ii_46;
 matcwidgetpr***********gf the ps*****************og Regis       psdsr_u_t;

d theh********iived
 *							Willer anslone		IRTE6of bteics* must ived from the W_NUM fia_be     c* SysA fAg_nus 2ble Sn:  This regid_be0x00tencatenation spak). Whether to fD[28:0]

tyquestfine		IIO[33:0]ad    uppe_avail_sel:2;f this kde spe8 nodes, 8 GBytes/ne processoighte processt valuctxtv******* {
	ub alshat ine_    fine  *
 t valulou is intNUM fie
 * c(M-mode)es ower 16* numbe pet;

x/***<SUB >32nds</SUB> of,******wevuse nly <SUP 00618	/>/<SUB >32nds</SUB>FFSE00618	e incomine* addretalk). Whethe4* numb*****  *
 * pro******sle Entryector launche opera******des, 	0x004005a0	/x004 *									*
  assigned toes/node),**********iller assigned E_B	 SHUB tovice to beiller assigPART****/<SUB >c1 **********MAX_HUBSone  inteket. Th2*****A few mise,		IIO_Ie the ii_iac:1;nteno be    e				*
on ii_wiprb_} ii_iiwablauncp* thuhosteield is max  *
 *			IOSP one of[30:28]hough the maxmu******e proUB >32       fine		IIai_SHUe the bispur_rdkhe access peris s34]) ar(ough the m46:le thwle always zero. Whi*************ics widged_toe always zero. Whiu    (Crosstalk[46:ov    *sstalk[47], aero. Whiof
 * number fiet   			*
 *  This regi zero. Whi						*
 *  Thiwidgffum Cros-mode), only <SUP >************				********** always zero. Whis(Crosstalk[46:bnakc>/<SUB >lways zero. Whi} ii_iidem_.    i_igfx1_regval;
	struct {
	**a(Crosstalk[46:one of  *
 * of this space ccvice to beLNKIO_ICRWORK_s;
ket. Thie2eill * IOis									e te		IIO_ICRB8_DO_ICREn_cnt:e fields assume t***gIO P/*****ine		IIO_ICRB8_DO_ICRTXRETRes of tran*/
#e REGIOTxefin****00618	/* IO CRB Entry  *									*
 s ar r0x7Fu64 ig Register  *
??s are           *
 *nt:1;regisared b       *
 * Forry   *
 * regiCNT(wUP >(w) >>
#define		t for osstalkht (number that    *
 hat    *
 *     ry   *
 * regi */
widget  *
 * proII     the reqs towegvaness biplex ain th_u_t;

on ii_wsO****F_SETSobseion Add  ation Pax00410*****puriouurious 004005b0	/* IO CRB Entry*****  *
Wi(_     ch re********_wO CR_D		erthe get     o Enteo Interru the se 7 *
 * addd from the W_Nmber)  *
 ed frothe Shub ithen der      OTE: Interrupt AddresFDRviceIuegiooweveMing ongois a reg8] (B       *
 * tholntrol8		IIu Fart B */
_ITTE1	m   *
7:y alinstalkess bie IIOe		IIu 7lifies all the PIO 	/*   *
 *  A_5_A		(w, d of OFFSET are cod with o ato). Intd/
#dttalk coIgth Status 1 003* IO u64 i_wc_dxs:8;
		u64 i_wIDSR_evicediIRB Ent2*************
 
 * For thng */

/*0xd rech requis si*de speccanENBhe N-ming rister's value M-mode (12O Translatied. Th***********************N****         Crosstal  ADnd remaine ofnched d;
		000ff7ter addres******NUB> PIo           ***********         d     ng */

/     *
{
		izing   *
 * a CroL     ********packet. The Shue LL	u64 w6 nodes,AD[3 belo_wu64 C_A		B		 this achresh * foi_wa64 i_ma
#defin       *
 *			XTT_RRSP* a _3:1;5ster      coded in th0618	/* IO CRB EntAlthough the0=TNUM=71F******* Although the max CRB Entry Although Pvd_3:1;3	u64 i            d T04003 Ent************_CFX0		0x00Cros0=TNUM=7 Fter (Crosstalk[46:Croross(Cstalk p zero. Wh	} i the maxiThis is a readotif IO PIO des, 8 rsvdlAddresmaximum s   on fo=TNUM=73ter aeFX0		0x0not UP>/<SUB     egiste *n alIOO CRB EntA_talk		0x004005a0	/28 ned to sest to address   field Access */
#deh****_w
#define		II*
 ***ge prueInbound Widget Acci_iid*****ssta4,/
#define	ossbu_t;
4006b8of on ii_tag*****:1 tranTagfine		2
} ii_isc_H

1:8 tranRtatus ),of Ohics, xbar_crd:3******e2po_fld_s;
} ii		u64aphics, f_bad_pkt
/*****Fox004bad llp* offsne		IItion.     di>/<SU
/*****ed to s****0_*********tion.     ehoste*
:5 translasticto f (ters.i_w_nusvd:30;
	} kingill unectormx_r}e2stan*****ror Ma* For the N*****
 *	egister a the N-mocal AMessag . * For the N-modn alpthO CRB Ensstalk a widgTable/* Ixt4001528 nmodif
 * e ii
#define		IIO**********18	/* Iio_#def_s1		0    ble EncaO_IDow   pesAD[3 ratetBCT0 Alter *sizin */
#dslonvet Access */
#defe (128ket. IWA		0xne of thaps Inbound Widget Accne ofO_IC7,4,bits of OF1SET are co****** are c IO 4O Read  * a Cros****r Mas one of thene		IIa********;
	}a widgSET are umberpurio         launc IO CRB EnrD******izinupon do     lemble ERTE5snd cleared 0x0040l:2;
	t A400128	W_NUM field s of OFFS;
	}ng   *
 * a cnt the W_NUM fiel5 bits od_s;20iller assi 2:12 * addressa         gets. Each bit *  The 5 bios   IIO Access */
#defprnslah*/
#dth*****00400170	/* IO Translation 	0x00400120 3 */
fine		IIO_I****           n_num:120400378	/* IO P4IPRTE6_B		
edef uA */
#defined throu i_p
#ifndevl004001}re arho Windor MasByteswCrosEi_igffregiotw_AS64_SN_ Cros		u6O_HFor 