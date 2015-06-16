/******************************************************************************
 * This software may be used and distributed according to the terms of
 * the GNU General Public License (GPL), incorporated herein by reference.
 * Drivers based on or derived from this code fall under the GPL and must
 * retain the authorship, copyright and license notice.  This file is not
 * a complete program and may only be used when the entire operating
 * system is licensed under the GPL.
 * See the file COPYING in this distribution for more information.
 *
 * vxge-reg.h: Driver for Neterion Inc's X3100 Series 10GbE PCIe I/O Virtualized
 *             Server Adapter.
 * Copyright(c) 2002-2009 Neterion Inc.
 ******************************************************************************/
#ifndef VXGE_REG_H
#define VXGE_REG_H

/*
 * vxge_mBIT(loc) - set bit at offset
 */
#define vxge_mBIT(loc)		(0x8000000000000000ULL >> (loc))

/*
 * vxge_vBIT(val, loc, sz) - set bits at offset
 */
#define vxge_vBIT(val, loc, sz)	(((u64)(val)) << (64-(loc)-(sz)))
#define vxge_vBIT32(val, loc, sz)	(((u32)(val)) << (32-(loc)-(sz)))

/*
 * vxge_bVALn(bits, loc, n) - Get the value of n bits at location
 */
#define vxge_bVALn(bits, loc, n) \
	((((u64)bits) >> (64-(loc+n))) & ((0x1ULL << n) - 1))

#define	VXGE_HW_TITAN_ASIC_ID_GET_INITIAL_DEVICE_ID(bits) \
							vxge_bVALn(bits, 0, 16)
#define	VXGE_HW_TITAN_ASIC_ID_GET_INITIAL_MAJOR_REVISION(bits) \
							vxge_bVALn(bits, 48, 8)
#define	VXGE_HW_TITAN_ASIC_ID_GET_INITIAL_MINOR_REVISION(bits) \
							vxge_bVALn(bits, 56, 8)

#define	VXGE_HW_VPATH_TO_FUNC_MAP_CFG1_GET_VPATH_TO_FUNC_MAP_CFG1(bits) \
							vxge_bVALn(bits, 3, 5)
#define	VXGE_HW_HOST_TYPE_ASSIGNMENTS_GET_HOST_TYPE_ASSIGNMENTS(bits) \
							vxge_bVALn(bits, 5, 3)
#define VXGE_HW_PF_SW_RESET_COMMAND				0xA5

#define VXGE_HW_TITAN_PCICFGMGMT_REG_SPACES		17
#define VXGE_HW_TITAN_SRPCIM_REG_SPACES			17
#define VXGE_HW_TITAN_VPMGMT_REG_SPACES			17
#define VXGE_HW_TITAN_VPATH_REG_SPACES			17

#define VXGE_HW_ASIC_MODE_RESERVED				0
#define VXGE_HW_ASIC_MODE_NO_IOV				1
#define VXGE_HW_ASIC_MODE_SR_IOV				2
#define VXGE_HW_ASIC_MODE_MR_IOV				3

#define	VXGE_HW_TXMAC_GEN_CFG1_TMAC_PERMA_STOP_EN		vxge_mBIT(3)
#define	VXGE_HW_TXMAC_GEN_CFG1_BLOCK_BCAST_TO_WIRE		vxge_mBIT(19)
#define	VXGE_HW_TXMAC_GEN_CFG1_BLOCK_BCAST_TO_SWITCH	vxge_mBIT(23)
#define	VXGE_HW_TXMAC_GEN_CFG1_HOST_APPEND_FCS			vxge_mBIT(31)

#define	VXGE_HW_VPATH_IS_FIRST_GET_VPATH_IS_FIRST(bits)	vxge_bVALn(bits, 3, 1)

#define	VXGE_HW_TIM_VPATH_ASSIGNMENT_GET_BMAP_ROOT(bits) \
						vxge_bVALn(bits, 0, 32)

#define	VXGE_HW_RXMAC_CFG0_PORT_VPMGMT_CLONE_GET_MAX_PYLD_LEN(bits) \
							vxge_bVALn(bits, 50, 14)

#define	VXGE_HW_XMAC_VSPORT_CHOICES_VP_GET_VSPORT_VECTOR(bits) \
							vxge_bVALn(bits, 0, 17)

#define	VXGE_HW_XMAC_VPATH_TO_VSPORT_VPMGMT_CLONE_GET_VSPORT_NUMBER(bits) \
							vxge_bVALn(bits, 3, 5)

#define	VXGE_HW_KDFC_DRBL_TRIPLET_TOTAL_GET_KDFC_MAX_SIZE(bits) \
							vxge_bVALn(bits, 17, 15)

#define VXGE_HW_KDFC_TRPL_FIFO_0_CTRL_MODE_LEGACY_MODE			0
#define VXGE_HW_KDFC_TRPL_FIFO_0_CTRL_MODE_NON_OFFLOAD_ONLY		1
#define VXGE_HW_KDFC_TRPL_FIFO_0_CTRL_MODE_MULTI_OP_MODE		2

#define VXGE_HW_KDFC_TRPL_FIFO_1_CTRL_MODE_MESSAGES_ONLY		0
#define VXGE_HW_KDFC_TRPL_FIFO_1_CTRL_MODE_MULTI_OP_MODE		1

#define	VXGE_HW_TOC_GET_KDFC_INITIAL_OFFSET(val) \
				(val&~VXGE_HW_TOC_KDFC_INITIAL_BIR(7))
#define	VXGE_HW_TOC_GET_KDFC_INITIAL_BIR(val) \
				vxge_bVALn(val, 61, 3)
#define	VXGE_HW_TOC_GET_USDC_INITIAL_OFFSET(val) \
				(val&~VXGE_HW_TOC_USDC_INITIAL_BIR(7))
#define	VXGE_HW_TOC_GET_USDC_INITIAL_BIR(val) \
				vxge_bVALn(val, 61, 3)

#define	VXGE_HW_TOC_KDFC_VPATH_STRIDE_GET_TOC_KDFC_VPATH_STRIDE(bits)	bits
#define	VXGE_HW_TOC_KDFC_FIFO_STRIDE_GET_TOC_KDFC_FIFO_STRIDE(bits)	bits

#define	VXGE_HW_KDFC_TRPL_FIFO_OFFSET_GET_KDFC_RCTR0(bits) \
						vxge_bVALn(bits, 1, 15)
#define	VXGE_HW_KDFC_TRPL_FIFO_OFFSET_GET_KDFC_RCTR1(bits) \
						vxge_bVALn(bits, 17, 15)
#define	VXGE_HW_KDFC_TRPL_FIFO_OFFSET_GET_KDFC_RCTR2(bits) \
						vxge_bVALn(bits, 33, 15)

#define VXGE_HW_KDFC_TRPL_FIFO_OFFSET_KDFC_VAPTH_NUM(val) vxge_vBIT(val, 42, 5)
#define VXGE_HW_KDFC_TRPL_FIFO_OFFSET_KDFC_FIFO_NUM(val) vxge_vBIT(val, 47, 2)
#define VXGE_HW_KDFC_TRPL_FIFO_OFFSET_KDFC_FIFO_OFFSET(val) \
					vxge_vBIT(val, 49, 15)

#define VXGE_HW_PRC_CFG4_RING_MODE_ONE_BUFFER			0
#define VXGE_HW_PRC_CFG4_RING_MODE_THREE_BUFFER			1
#define VXGE_HW_PRC_CFG4_RING_MODE_FIVE_BUFFER			2

#define VXGE_HW_PRC_CFG7_SCATTER_MODE_A				0
#define VXGE_HW_PRC_CFG7_SCATTER_MODE_B				2
#define VXGE_HW_PRC_CFG7_SCATTER_MODE_C				1

#define VXGE_HW_RTS_MGR_STEER_CTRL_WE_READ                             0
#define VXGE_HW_RTS_MGR_STEER_CTRL_WE_WRITE                            1

#define VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_DA                  0
#define VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_VID                 1
#define VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_ETYPE               2
#define VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_PN                  3
#define VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RANGE_PN            4
#define VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_GEN_CFG         5
#define VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_SOLO_IT         6
#define VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_JHASH_CFG       7
#define VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_MASK            8
#define VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_KEY             9
#define VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_QOS                 10
#define VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_DS                  11
#define VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_IT        12
#define VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_FW_VERSION          13

#define VXGE_HW_RTS_MGR_STEER_DATA0_GET_DA_MAC_ADDR(bits) \
							vxge_bVALn(bits, 0, 48)
#define VXGE_HW_RTS_MGR_STEER_DATA0_DA_MAC_ADDR(val) vxge_vBIT(val, 0, 48)

#define VXGE_HW_RTS_MGR_STEER_DATA1_GET_DA_MAC_ADDR_MASK(bits) \
							vxge_bVALn(bits, 0, 48)
#define VXGE_HW_RTS_MGR_STEER_DATA1_DA_MAC_ADDR_MASK(val) vxge_vBIT(val, 0, 48)
#define VXGE_HW_RTS_MGR_STEER_DATA1_DA_MAC_ADDR_ADD_PRIVILEGED_MODE \
								vxge_mBIT(54)
#define VXGE_HW_RTS_MGR_STEER_DATA1_GET_DA_MAC_ADDR_ADD_VPATH(bits) \
							vxge_bVALn(bits, 55, 5)
#define VXGE_HW_RTS_MGR_STEER_DATA1_DA_MAC_ADDR_ADD_VPATH(val) \
							vxge_vBIT(val, 55, 5)
#define VXGE_HW_RTS_MGR_STEER_DATA1_GET_DA_MAC_ADDR_ADD_MODE(bits) \
							vxge_bVALn(bits, 62, 2)
#define VXGE_HW_RTS_MGR_STEER_DATA1_DA_MAC_ADDR_MODE(val) vxge_vBIT(val, 62, 2)

#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_ADD_ENTRY			0
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_DELETE_ENTRY		1
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_LIST_FIRST_ENTRY		2
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_LIST_NEXT_ENTRY		3
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_READ_ENTRY			0
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_WRITE_ENTRY		1
#define VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_READ_MEMO_ENTRY		3
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_LED_CONTROL		4
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_ALL_CLEAR			172

#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_DA		0
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_VID		1
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_ETYPE		2
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_PN		3
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_GEN_CFG	5
#define	VXGE_HW_RTS_ACS_STEER_CTRL_DATA_STRUCT_SEL_RTH_SOLO_IT	6
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_JHASH_CFG	7
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MASK		8
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_KEY		9
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_QOS		10
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_DS		11
#define	VXGE_HW_RTS_ACS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_IT	12
#define	VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO		13

#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_DA_MAC_ADDR(bits) \
							vxge_bVALn(bits, 0, 48)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_DA_MAC_ADDR(val) vxge_vBIT(val, 0, 48)

#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_VLAN_ID(bits) vxge_bVALn(bits, 0, 12)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_VLAN_ID(val) vxge_vBIT(val, 0, 12)

#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_ETYPE(bits)	vxge_bVALn(bits, 0, 11)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_ETYPE(val) vxge_vBIT(val, 0, 16)

#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_PN_SRC_DEST_SEL(bits) \
							vxge_bVALn(bits, 3, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_PN_SRC_DEST_SEL		vxge_mBIT(3)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_PN_TCP_UDP_SEL(bits) \
							vxge_bVALn(bits, 7, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_PN_TCP_UDP_SEL		vxge_mBIT(7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_PN_PORT_NUM(bits) \
							vxge_bVALn(bits, 8, 16)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_PN_PORT_NUM(val) vxge_vBIT(val, 8, 16)

#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_RTH_EN(bits) \
							vxge_bVALn(bits, 3, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_EN		vxge_mBIT(3)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_BUCKET_SIZE(bits) \
							vxge_bVALn(bits, 4, 4)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_BUCKET_SIZE(val) \
							vxge_vBIT(val, 4, 4)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_ALG_SEL(bits) \
							vxge_bVALn(bits, 10, 2)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_ALG_SEL(val) \
							vxge_vBIT(val, 10, 2)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_ALG_SEL_JENKINS	0
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_ALG_SEL_MS_RSS	1
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_ALG_SEL_CRC32C	2
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_RTH_TCP_IPV4_EN(bits) \
							vxge_bVALn(bits, 15, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_TCP_IPV4_EN	vxge_mBIT(15)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_RTH_IPV4_EN(bits) \
							vxge_bVALn(bits, 19, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_IPV4_EN	vxge_mBIT(19)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_RTH_TCP_IPV6_EN(bits) \
							vxge_bVALn(bits, 23, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_TCP_IPV6_EN	vxge_mBIT(23)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_RTH_IPV6_EN(bits) \
							vxge_bVALn(bits, 27, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_IPV6_EN	vxge_mBIT(27)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_RTH_TCP_IPV6_EX_EN(bits) \
							vxge_bVALn(bits, 31, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_TCP_IPV6_EX_EN vxge_mBIT(31)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_RTH_IPV6_EX_EN(bits) \
							vxge_bVALn(bits, 35, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_IPV6_EX_EN	vxge_mBIT(35)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_ACTIVE_TABLE(bits) \
							vxge_bVALn(bits, 39, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_ACTIVE_TABLE	vxge_mBIT(39)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_REPL_ENTRY_EN(bits) \
							vxge_bVALn(bits, 43, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_REPL_ENTRY_EN	vxge_mBIT(43)

#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_SOLO_IT_ENTRY_EN(bits) \
							vxge_bVALn(bits, 3, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_SOLO_IT_ENTRY_EN	vxge_mBIT(3)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_SOLO_IT_BUCKET_DATA(bits) \
							vxge_bVALn(bits, 9, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_SOLO_IT_BUCKET_DATA(val) \
							vxge_vBIT(val, 9, 7)

#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_ITEM0_BUCKET_NUM(bits) \
							vxge_bVALn(bits, 0, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_ITEM0_BUCKET_NUM(val) \
							vxge_vBIT(val, 0, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_ITEM0_ENTRY_EN(bits) \
							vxge_bVALn(bits, 8, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_ITEM0_ENTRY_EN	vxge_mBIT(8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_ITEM0_BUCKET_DATA(bits) \
							vxge_bVALn(bits, 9, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_ITEM0_BUCKET_DATA(val) \
							vxge_vBIT(val, 9, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_ITEM1_BUCKET_NUM(bits) \
							vxge_bVALn(bits, 16, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_ITEM1_BUCKET_NUM(val) \
							vxge_vBIT(val, 16, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_ITEM1_ENTRY_EN(bits) \
							vxge_bVALn(bits, 24, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_ITEM1_ENTRY_EN	vxge_mBIT(24)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_ITEM1_BUCKET_DATA(bits) \
							vxge_bVALn(bits, 25, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_ITEM1_BUCKET_DATA(val) \
							vxge_vBIT(val, 25, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM0_BUCKET_NUM(bits) \
							vxge_bVALn(bits, 0, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM0_BUCKET_NUM(val) \
							vxge_vBIT(val, 0, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM0_ENTRY_EN(bits) \
							vxge_bVALn(bits, 8, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM0_ENTRY_EN	vxge_mBIT(8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM0_BUCKET_DATA(bits) \
							vxge_bVALn(bits, 9, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM0_BUCKET_DATA(val) \
							vxge_vBIT(val, 9, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM1_BUCKET_NUM(bits) \
							vxge_bVALn(bits, 16, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM1_BUCKET_NUM(val) \
							vxge_vBIT(val, 16, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM1_ENTRY_EN(bits) \
							vxge_bVALn(bits, 24, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM1_ENTRY_EN	vxge_mBIT(24)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM1_BUCKET_DATA(bits) \
							vxge_bVALn(bits, 25, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM1_BUCKET_DATA(val) \
							vxge_vBIT(val, 25, 7)

#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_JHASH_CFG_GOLDEN_RATIO(bits) \
							vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_JHASH_CFG_GOLDEN_RATIO(val) \
							vxge_vBIT(val, 0, 32)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_JHASH_CFG_INIT_VALUE(bits) \
							vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_JHASH_CFG_INIT_VALUE(val) \
							vxge_vBIT(val, 32, 32)

#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_MASK_IPV6_SA_MASK(bits) \
							vxge_bVALn(bits, 0, 16)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_MASK_IPV6_SA_MASK(val) \
							vxge_vBIT(val, 0, 16)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_MASK_IPV6_DA_MASK(bits) \
							vxge_bVALn(bits, 16, 16)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_MASK_IPV6_DA_MASK(val) \
							vxge_vBIT(val, 16, 16)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_MASK_IPV4_SA_MASK(bits) \
							vxge_bVALn(bits, 32, 4)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_MASK_IPV4_SA_MASK(val) \
							vxge_vBIT(val, 32, 4)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_MASK_IPV4_DA_MASK(bits) \
							vxge_bVALn(bits, 36, 4)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_MASK_IPV4_DA_MASK(val) \
							vxge_vBIT(val, 36, 4)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_MASK_L4SP_MASK(bits) \
							vxge_bVALn(bits, 40, 2)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_MASK_L4SP_MASK(val) \
							vxge_vBIT(val, 40, 2)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_MASK_L4DP_MASK(bits) \
							vxge_bVALn(bits, 42, 2)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_MASK_L4DP_MASK(val) \
							vxge_vBIT(val, 42, 2)

#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_KEY_KEY(bits) \
							vxge_bVALn(bits, 0, 64)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_KEY_KEY vxge_vBIT(val, 0, 64)

#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_QOS_ENTRY_EN(bits) \
							vxge_bVALn(bits, 3, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_QOS_ENTRY_EN		vxge_mBIT(3)

#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_DS_ENTRY_EN(bits) \
							vxge_bVALn(bits, 3, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_DS_ENTRY_EN		vxge_mBIT(3)

#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_DA_MAC_ADDR_MASK(bits) \
							vxge_bVALn(bits, 0, 48)
#define VXGE_HW_RTS_ACCESS_STEER_DATA1_DA_MAC_ADDR_MASK(val) \
							vxge_vBIT(val, 0, 48)
#define VXGE_HW_RTS_ACCESS_STEER_DATA1_DA_MAC_ADDR_MODE(val) \
							vxge_vBIT(val, 62, 2)

#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM4_BUCKET_NUM(bits) \
							vxge_bVALn(bits, 0, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM4_BUCKET_NUM(val) \
							vxge_vBIT(val, 0, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM4_ENTRY_EN(bits) \
							vxge_bVALn(bits, 8, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM4_ENTRY_EN	vxge_mBIT(8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM4_BUCKET_DATA(bits) \
							vxge_bVALn(bits, 9, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM4_BUCKET_DATA(val) \
							vxge_vBIT(val, 9, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM5_BUCKET_NUM(bits) \
							vxge_bVALn(bits, 16, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM5_BUCKET_NUM(val) \
							vxge_vBIT(val, 16, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM5_ENTRY_EN(bits) \
							vxge_bVALn(bits, 24, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM5_ENTRY_EN	vxge_mBIT(24)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM5_BUCKET_DATA(bits) \
							vxge_bVALn(bits, 25, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM5_BUCKET_DATA(val) \
							vxge_vBIT(val, 25, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM6_BUCKET_NUM(bits) \
							vxge_bVALn(bits, 32, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM6_BUCKET_NUM(val) \
							vxge_vBIT(val, 32, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM6_ENTRY_EN(bits) \
							vxge_bVALn(bits, 40, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM6_ENTRY_EN	vxge_mBIT(40)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM6_BUCKET_DATA(bits) \
							vxge_bVALn(bits, 41, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM6_BUCKET_DATA(val) \
							vxge_vBIT(val, 41, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM7_BUCKET_NUM(bits) \
							vxge_bVALn(bits, 48, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM7_BUCKET_NUM(val) \
							vxge_vBIT(val, 48, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM7_ENTRY_EN(bits) \
							vxge_bVALn(bits, 56, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM7_ENTRY_EN	vxge_mBIT(56)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM7_BUCKET_DATA(bits) \
							vxge_bVALn(bits, 57, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM7_BUCKET_DATA(val) \
							vxge_vBIT(val, 57, 7)

#define VXGE_HW_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PART_NUMBER           0
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_SERIAL_NUMBER         1
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_VERSION               2
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PCI_MODE              3
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_DESC_0                4
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_DESC_1                5
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_DESC_2                6
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_DESC_3                7

#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_LED_CONTROL_ON			1
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_LED_CONTROL_OFF			0

#define VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_FW_VER_DAY(bits) \
							vxge_bVALn(bits, 0, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_FW_VER_DAY(val) vxge_vBIT(val, 0, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_FW_VER_MONTH(bits) \
							vxge_bVALn(bits, 8, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_FW_VER_MONTH(val) vxge_vBIT(val, 8, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_FW_VER_YEAR(bits) \
						vxge_bVALn(bits, 16, 16)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_FW_VER_YEAR(val) \
							vxge_vBIT(val, 16, 16)

#define VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_FW_VER_MAJOR(bits) \
						vxge_bVALn(bits, 32, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_FW_VER_MAJOR vxge_vBIT(val, 32, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_FW_VER_MINOR(bits) \
						vxge_bVALn(bits, 40, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_FW_VER_MINOR vxge_vBIT(val, 40, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_FW_VER_BUILD(bits) \
						vxge_bVALn(bits, 48, 16)
#define VXGE_HW_RTS_ACCESS_STEER_DATA0_FW_VER_BUILD vxge_vBIT(val, 48, 16)

#define VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_FLASH_VER_DAY(bits) \
						vxge_bVALn(bits, 0, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA1_FLASH_VER_DAY(val) vxge_vBIT(val, 0, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_FLASH_VER_MONTH(bits) \
							vxge_bVALn(bits, 8, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA1_FLASH_VER_MONTH(val) vxge_vBIT(val, 8, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_FLASH_VER_YEAR(bits) \
							vxge_bVALn(bits, 16, 16)
#define VXGE_HW_RTS_ACCESS_STEER_DATA1_FLASH_VER_YEAR(val) \
							vxge_vBIT(val, 16, 16)

#define VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_FLASH_VER_MAJOR(bits) \
							vxge_bVALn(bits, 32, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA1_FLASH_VER_MAJOR vxge_vBIT(val, 32, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_FLASH_VER_MINOR(bits) \
							vxge_bVALn(bits, 40, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA1_FLASH_VER_MINOR vxge_vBIT(val, 40, 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_FLASH_VER_BUILD(bits) \
							vxge_bVALn(bits, 48, 16)
#define VXGE_HW_RTS_ACCESS_STEER_DATA1_FLASH_VER_BUILD vxge_vBIT(val, 48, 16)

#define	VXGE_HW_SRPCIM_TO_VPATH_ALARM_REG_GET_PPIF_SRPCIM_TO_VPATH_ALARM(bits)\
							vxge_bVALn(bits, 0, 18)

#define	VXGE_HW_RX_MULTI_CAST_STATS_GET_FRAME_DISCARD(bits) \
							vxge_bVALn(bits, 48, 16)
#define	VXGE_HW_RX_FRM_TRANSFERRED_GET_RX_FRM_TRANSFERRED(bits) \
							vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_RXD_RETURNED_GET_RXD_RETURNED(bits)	vxge_bVALn(bits, 48, 16)
#define	VXGE_HW_VPATH_DEBUG_STATS0_GET_INI_NUM_MWR_SENT(bits) \
							vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_VPATH_DEBUG_STATS1_GET_INI_NUM_MRD_SENT(bits) \
							vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_VPATH_DEBUG_STATS2_GET_INI_NUM_CPL_RCVD(bits) \
							vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_VPATH_DEBUG_STATS3_GET_INI_NUM_MWR_BYTE_SENT(bits)	(bits)
#define	VXGE_HW_VPATH_DEBUG_STATS4_GET_INI_NUM_CPL_BYTE_RCVD(bits)	(bits)
#define	VXGE_HW_VPATH_DEBUG_STATS5_GET_WRCRDTARB_XOFF(bits) \
							vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_VPATH_DEBUG_STATS6_GET_RDCRDTARB_XOFF(bits) \
							vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_VPATH_GENSTATS_COUNT01_GET_PPIF_VPATH_GENSTATS_COUNT1(bits) \
							vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_VPATH_GENSTATS_COUNT01_GET_PPIF_VPATH_GENSTATS_COUNT0(bits) \
							vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_VPATH_GENSTATS_COUNT23_GET_PPIF_VPATH_GENSTATS_COUNT3(bits) \
							vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_VPATH_GENSTATS_COUNT23_GET_PPIF_VPATH_GENSTATS_COUNT2(bits) \
							vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_VPATH_GENSTATS_COUNT4_GET_PPIF_VPATH_GENSTATS_COUNT4(bits) \
							vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_VPATH_GENSTATS_COUNT5_GET_PPIF_VPATH_GENSTATS_COUNT5(bits) \
							vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_TX_VP_RESET_DISCARDED_FRMS_GET_TX_VP_RESET_DISCARDED_FRMS(bits\
) vxge_bVALn(bits, 48, 16)
#define	VXGE_HW_DBG_STATS_GET_RX_MPA_CRC_FAIL_FRMS(bits) vxge_bVALn(bits, 0, 16)
#define	VXGE_HW_DBG_STATS_GET_RX_MPA_MRK_FAIL_FRMS(bits) \
							vxge_bVALn(bits, 16, 16)
#define	VXGE_HW_DBG_STATS_GET_RX_MPA_LEN_FAIL_FRMS(bits) \
							vxge_bVALn(bits, 32, 16)
#define	VXGE_HW_DBG_STATS_GET_RX_FAU_RX_WOL_FRMS(bits)	vxge_bVALn(bits, 0, 16)
#define	VXGE_HW_DBG_STATS_GET_RX_FAU_RX_VP_RESET_DISCARDED_FRMS(bits) \
							vxge_bVALn(bits, 16, 16)
#define	VXGE_HW_DBG_STATS_GET_RX_FAU_RX_PERMITTED_FRMS(bits) \
							vxge_bVALn(bits, 32, 16)

#define	VXGE_HW_MRPCIM_DEBUG_STATS0_GET_INI_WR_DROP(bits) \
							vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_MRPCIM_DEBUG_STATS0_GET_INI_RD_DROP(bits) \
							vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_MRPCIM_DEBUG_STATS1_GET_VPLANE_WRCRDTARB_PH_CRDT_DEPLETED(bits\
) vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_MRPCIM_DEBUG_STATS2_GET_VPLANE_WRCRDTARB_PD_CRDT_DEPLETED(bits\
) vxge_bVALn(bits, 32, 32)
#define \
VXGE_HW_MRPCIM_DEBUG_STATS3_GET_VPLANE_RDCRDTARB_NPH_CRDT_DEPLETED(bits) \
	vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_MRPCIM_DEBUG_STATS4_GET_INI_WR_VPIN_DROP(bits) \
							vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_MRPCIM_DEBUG_STATS4_GET_INI_RD_VPIN_DROP(bits) \
							vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_GENSTATS_COUNT01_GET_GENSTATS_COUNT1(bits) \
							vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_GENSTATS_COUNT01_GET_GENSTATS_COUNT0(bits) \
							vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_GENSTATS_COUNT23_GET_GENSTATS_COUNT3(bits) \
							vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_GENSTATS_COUNT23_GET_GENSTATS_COUNT2(bits) \
							vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_GENSTATS_COUNT4_GET_GENSTATS_COUNT4(bits) \
							vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_GENSTATS_COUNT5_GET_GENSTATS_COUNT5(bits) \
							vxge_bVALn(bits, 32, 32)

#define	VXGE_HW_DEBUG_STATS0_GET_RSTDROP_MSG(bits)	vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_DEBUG_STATS0_GET_RSTDROP_CPL(bits)	vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_DEBUG_STATS1_GET_RSTDROP_CLIENT0(bits)	vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_DEBUG_STATS1_GET_RSTDROP_CLIENT1(bits)	vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_DEBUG_STATS2_GET_RSTDROP_CLIENT2(bits)	vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_DEBUG_STATS3_GET_VPLANE_DEPL_PH(bits)	vxge_bVALn(bits, 0, 16)
#define	VXGE_HW_DEBUG_STATS3_GET_VPLANE_DEPL_NPH(bits)	vxge_bVALn(bits, 16, 16)
#define	VXGE_HW_DEBUG_STATS3_GET_VPLANE_DEPL_CPLH(bits)	vxge_bVALn(bits, 32, 16)
#define	VXGE_HW_DEBUG_STATS4_GET_VPLANE_DEPL_PD(bits)	vxge_bVALn(bits, 0, 16)
#define	VXGE_HW_DEBUG_STATS4_GET_VPLANE_DEPL_NPD(bits)	bVAL(bits, 16, 16)
#define	VXGE_HW_DEBUG_STATS4_GET_VPLANE_DEPL_CPLD(bits)	vxge_bVALn(bits, 32, 16)

#define	VXGE_HW_DBG_STATS_TPA_TX_PATH_GET_TX_PERMITTED_FRMS(bits) \
							vxge_bVALn(bits, 32, 32)

#define	VXGE_HW_DBG_STAT_TX_ANY_FRMS_GET_PORT0_TX_ANY_FRMS(bits) \
							vxge_bVALn(bits, 0, 8)
#define	VXGE_HW_DBG_STAT_TX_ANY_FRMS_GET_PORT1_TX_ANY_FRMS(bits) \
							vxge_bVALn(bits, 8, 8)
#define	VXGE_HW_DBG_STAT_TX_ANY_FRMS_GET_PORT2_TX_ANY_FRMS(bits) \
							vxge_bVALn(bits, 16, 8)

#define	VXGE_HW_DBG_STAT_RX_ANY_FRMS_GET_PORT0_RX_ANY_FRMS(bits) \
							vxge_bVALn(bits, 0, 8)
#define	VXGE_HW_DBG_STAT_RX_ANY_FRMS_GET_PORT1_RX_ANY_FRMS(bits) \
							vxge_bVALn(bits, 8, 8)
#define	VXGE_HW_DBG_STAT_RX_ANY_FRMS_GET_PORT2_RX_ANY_FRMS(bits) \
							vxge_bVALn(bits, 16, 8)

#define VXGE_HW_CONFIG_PRIV_H

#define VXGE_HW_SWAPPER_INITIAL_VALUE			0x0123456789abcdefULL
#define VXGE_HW_SWAPPER_BYTE_SWAPPED			0xefcdab8967452301ULL
#define VXGE_HW_SWAPPER_BIT_FLIPPED			0x80c4a2e691d5b3f7ULL
#define VXGE_HW_SWAPPER_BYTE_SWAPPED_BIT_FLIPPED	0xf7b3d591e6a2c480ULL

#define VXGE_HW_SWAPPER_READ_BYTE_SWAP_ENABLE		0xFFFFFFFFFFFFFFFFULL
#define VXGE_HW_SWAPPER_READ_BYTE_SWAP_DISABLE		0x0000000000000000ULL

#define VXGE_HW_SWAPPER_READ_BIT_FLAP_ENABLE		0xFFFFFFFFFFFFFFFFULL
#define VXGE_HW_SWAPPER_READ_BIT_FLAP_DISABLE		0x0000000000000000ULL

#define VXGE_HW_SWAPPER_WRITE_BYTE_SWAP_ENABLE		0xFFFFFFFFFFFFFFFFULL
#define VXGE_HW_SWAPPER_WRITE_BYTE_SWAP_DISABLE		0x0000000000000000ULL

#define VXGE_HW_SWAPPER_WRITE_BIT_FLAP_ENABLE		0xFFFFFFFFFFFFFFFFULL
#define VXGE_HW_SWAPPER_WRITE_BIT_FLAP_DISABLE		0x0000000000000000ULL

/*
 * The registers are memory mapped and are native big-endian byte order. The
 * little-endian hosts are handled by enabling hardware byte-swapping for
 * register and dma operations.
 */
struct vxge_hw_legacy_reg {

	u8	unused00010[0x00010];

/*0x00010*/	u64	toc_swapper_fb;
#define VXGE_HW_TOC_SWAPPER_FB_INITIAL_VAL(val) vxge_vBIT(val, 0, 64)
/*0x00018*/	u64	pifm_rd_swap_en;
#define VXGE_HW_PIFM_RD_SWAP_EN_PIFM_RD_SWAP_EN(val) vxge_vBIT(val, 0, 64)
/*0x00020*/	u64	pifm_rd_flip_en;
#define VXGE_HW_PIFM_RD_FLIP_EN_PIFM_RD_FLIP_EN(val) vxge_vBIT(val, 0, 64)
/*0x00028*/	u64	pifm_wr_swap_en;
#define VXGE_HW_PIFM_WR_SWAP_EN_PIFM_WR_SWAP_EN(val) vxge_vBIT(val, 0, 64)
/*0x00030*/	u64	pifm_wr_flip_en;
#define VXGE_HW_PIFM_WR_FLIP_EN_PIFM_WR_FLIP_EN(val) vxge_vBIT(val, 0, 64)
/*0x00038*/	u64	toc_first_pointer;
#define VXGE_HW_TOC_FIRST_POINTER_INITIAL_VAL(val) vxge_vBIT(val, 0, 64)
/*0x00040*/	u64	host_access_en;
#define VXGE_HW_HOST_ACCESS_EN_HOST_ACCESS_EN(val) vxge_vBIT(val, 0, 64)

} __packed;

struct vxge_hw_toc_reg {

	u8	unused00050[0x00050];

/*0x00050*/	u64	toc_common_pointer;
#define VXGE_HW_TOC_COMMON_POINTER_INITIAL_VAL(val) vxge_vBIT(val, 0, 64)
/*0x00058*/	u64	toc_memrepair_pointer;
#define VXGE_HW_TOC_MEMREPAIR_POINTER_INITIAL_VAL(val) vxge_vBIT(val, 0, 64)
/*0x00060*/	u64	toc_pcicfgmgmt_pointer[17];
#define VXGE_HW_TOC_PCICFGMGMT_POINTER_INITIAL_VAL(val) vxge_vBIT(val, 0, 64)
	u8	unused001e0[0x001e0-0x000e8];

/*0x001e0*/	u64	toc_mrpcim_pointer;
#define VXGE_HW_TOC_MRPCIM_POINTER_INITIAL_VAL(val) vxge_vBIT(val, 0, 64)
/*0x001e8*/	u64	toc_srpcim_pointer[17];
#define VXGE_HW_TOC_SRPCIM_POINTER_INITIAL_VAL(val) vxge_vBIT(val, 0, 64)
	u8	unused00278[0x00278-0x00270];

/*0x00278*/	u64	toc_vpmgmt_pointer[17];
#define VXGE_HW_TOC_VPMGMT_POINTER_INITIAL_VAL(val) vxge_vBIT(val, 0, 64)
	u8	unused00390[0x00390-0x00300];

/*0x00390*/	u64	toc_vpath_pointer[17];
#define VXGE_HW_TOC_VPATH_POINTER_INITIAL_VAL(val) vxge_vBIT(val, 0, 64)
	u8	unused004a0[0x004a0-0x00418];

/*0x004a0*/	u64	toc_kdfc;
#define VXGE_HW_TOC_KDFC_INITIAL_OFFSET(val) vxge_vBIT(val, 0, 61)
#define VXGE_HW_TOC_KDFC_INITIAL_BIR(val) vxge_vBIT(val, 61, 3)
/*0x004a8*/	u64	toc_usdc;
#define VXGE_HW_TOC_USDC_INITIAL_OFFSET(val) vxge_vBIT(val, 0, 61)
#define VXGE_HW_TOC_USDC_INITIAL_BIR(val) vxge_vBIT(val, 61, 3)
/*0x004b0*/	u64	toc_kdfc_vpath_stride;
#define	VXGE_HW_TOC_KDFC_VPATH_STRIDE_INITIAL_TOC_KDFC_VPATH_STRIDE(val) \
							vxge_vBIT(val, 0, 64)
/*0x004b8*/	u64	toc_kdfc_fifo_stride;
#define	VXGE_HW_TOC_KDFC_FIFO_STRIDE_INITIAL_TOC_KDFC_FIFO_STRIDE(val) \
							vxge_vBIT(val, 0, 64)

} __packed;

struct vxge_hw_common_reg {

	u8	unused00a00[0x00a00];

/*0x00a00*/	u64	prc_status1;
#define VXGE_HW_PRC_STATUS1_PRC_VP_QUIESCENT(n)	vxge_mBIT(n)
/*0x00a08*/	u64	rxdcm_reset_in_progress;
#define VXGE_HW_RXDCM_RESET_IN_PROGRESS_PRC_VP(n)	vxge_mBIT(n)
/*0x00a10*/	u64	replicq_flush_in_progress;
#define VXGE_HW_REPLICQ_FLUSH_IN_PROGRESS_NOA_VP(n)	vxge_mBIT(n)
/*0x00a18*/	u64	rxpe_cmds_reset_in_progress;
#define VXGE_HW_RXPE_CMDS_RESET_IN_PROGRESS_NOA_VP(n)	vxge_mBIT(n)
/*0x00a20*/	u64	mxp_cmds_reset_in_progress;
#define VXGE_HW_MXP_CMDS_RESET_IN_PROGRESS_NOA_VP(n)	vxge_mBIT(n)
/*0x00a28*/	u64	noffload_reset_in_progress;
#define VXGE_HW_NOFFLOAD_RESET_IN_PROGRESS_PRC_VP(n)	vxge_mBIT(n)
/*0x00a30*/	u64	rd_req_in_progress;
#define VXGE_HW_RD_REQ_IN_PROGRESS_VP(n)	vxge_mBIT(n)
/*0x00a38*/	u64	rd_req_outstanding;
#define VXGE_HW_RD_REQ_OUTSTANDING_VP(n)	vxge_mBIT(n)
/*0x00a40*/	u64	kdfc_reset_in_progress;
#define VXGE_HW_KDFC_RESET_IN_PROGRESS_NOA_VP(n)	vxge_mBIT(n)
	u8	unused00b00[0x00b00-0x00a48];

/*0x00b00*/	u64	one_cfg_vp;
#define VXGE_HW_ONE_CFG_VP_RDY(n)	vxge_mBIT(n)
/*0x00b08*/	u64	one_common;
#define VXGE_HW_ONE_COMMON_PET_VPATH_RESET_IN_PROGRESS(n)	vxge_mBIT(n)
	u8	unused00b80[0x00b80-0x00b10];

/*0x00b80*/	u64	tim_int_en;
#define VXGE_HW_TIM_INT_EN_TIM_VP(n)	vxge_mBIT(n)
/*0x00b88*/	u64	tim_set_int_en;
#define VXGE_HW_TIM_SET_INT_EN_VP(n)	vxge_mBIT(n)
/*0x00b90*/	u64	tim_clr_int_en;
#define VXGE_HW_TIM_CLR_INT_EN_VP(n)	vxge_mBIT(n)
/*0x00b98*/	u64	tim_mask_int_during_reset;
#define VXGE_HW_TIM_MASK_INT_DURING_RESET_VPATH(n)	vxge_mBIT(n)
/*0x00ba0*/	u64	tim_reset_in_progress;
#define VXGE_HW_TIM_RESET_IN_PROGRESS_TIM_VPATH(n)	vxge_mBIT(n)
/*0x00ba8*/	u64	tim_outstanding_bmap;
#define VXGE_HW_TIM_OUTSTANDING_BMAP_TIM_VPATH(n)	vxge_mBIT(n)
	u8	unused00c00[0x00c00-0x00bb0];

/*0x00c00*/	u64	msg_reset_in_progress;
#define VXGE_HW_MSG_RESET_IN_PROGRESS_MSG_COMPOSITE(val) vxge_vBIT(val, 0, 17)
/*0x00c08*/	u64	msg_mxp_mr_ready;
#define VXGE_HW_MSG_MXP_MR_READY_MP_BOOTED(n)	vxge_mBIT(n)
/*0x00c10*/	u64	msg_uxp_mr_ready;
#define VXGE_HW_MSG_UXP_MR_READY_UP_BOOTED(n)	vxge_mBIT(n)
/*0x00c18*/	u64	msg_dmq_noni_rtl_prefetch;
#define VXGE_HW_MSG_DMQ_NONI_RTL_PREFETCH_BYPASS_ENABLE(n)	vxge_mBIT(n)
/*0x00c20*/	u64	msg_umq_rtl_bwr;
#define VXGE_HW_MSG_UMQ_RTL_BWR_PREFETCH_DISABLE(n)	vxge_mBIT(n)
	u8	unused00d00[0x00d00-0x00c28];

/*0x00d00*/	u64	cmn_rsthdlr_cfg0;
#define VXGE_HW_CMN_RSTHDLR_CFG0_SW_RESET_VPATH(val) vxge_vBIT(val, 0, 17)
/*0x00d08*/	u64	cmn_rsthdlr_cfg1;
#define VXGE_HW_CMN_RSTHDLR_CFG1_CLR_VPATH_RESET(val) vxge_vBIT(val, 0, 17)
/*0x00d10*/	u64	cmn_rsthdlr_cfg2;
#define VXGE_HW_CMN_RSTHDLR_CFG2_SW_RESET_FIFO0(val) vxge_vBIT(val, 0, 17)
/*0x00d18*/	u64	cmn_rsthdlr_cfg3;
#define VXGE_HW_CMN_RSTHDLR_CFG3_SW_RESET_FIFO1(val) vxge_vBIT(val, 0, 17)
/*0x00d20*/	u64	cmn_rsthdlr_cfg4;
#define VXGE_HW_CMN_RSTHDLR_CFG4_SW_RESET_FIFO2(val) vxge_vBIT(val, 0, 17)
	u8	unused00d40[0x00d40-0x00d28];

/*0x00d40*/	u64	cmn_rsthdlr_cfg8;
#define VXGE_HW_CMN_RSTHDLR_CFG8_INCR_VPATH_INST_NUM(val) vxge_vBIT(val, 0, 17)
/*0x00d48*/	u64	stats_cfg0;
#define VXGE_HW_STATS_CFG0_STATS_ENABLE(val) vxge_vBIT(val, 0, 17)
	u8	unused00da8[0x00da8-0x00d50];

/*0x00da8*/	u64	clear_msix_mask_vect[4];
#define VXGE_HW_CLEAR_MSIX_MASK_VECT_CLEAR_MSIX_MASK_VECT(val) \
						vxge_vBIT(val, 0, 17)
/*0x00dc8*/	u64	set_msix_mask_vect[4];
#define VXGE_HW_SET_MSIX_MASK_VECT_SET_MSIX_MASK_VECT(val) vxge_vBIT(val, 0, 17)
/*0x00de8*/	u64	clear_msix_mask_all_vect;
#define	VXGE_HW_CLEAR_MSIX_MASK_ALL_VECT_CLEAR_MSIX_MASK_ALL_VECT(val)	\
							vxge_vBIT(val, 0, 17)
/*0x00df0*/	u64	set_msix_mask_all_vect;
#define	VXGE_HW_SET_MSIX_MASK_ALL_VECT_SET_MSIX_MASK_ALL_VECT(val) \
							vxge_vBIT(val, 0, 17)
/*0x00df8*/	u64	mask_vector[4];
#define VXGE_HW_MASK_VECTOR_MASK_VECTOR(val) vxge_vBIT(val, 0, 17)
/*0x00e18*/	u64	msix_pending_vector[4];
#define VXGE_HW_MSIX_PENDING_VECTOR_MSIX_PENDING_VECTOR(val) \
							vxge_vBIT(val, 0, 17)
/*0x00e38*/	u64	clr_msix_one_shot_vec[4];
#define	VXGE_HW_CLR_MSIX_ONE_SHOT_VEC_CLR_MSIX_ONE_SHOT_VEC(val) \
							vxge_vBIT(val, 0, 17)
/*0x00e58*/	u64	titan_asic_id;
#define VXGE_HW_TITAN_ASIC_ID_INITIAL_DEVICE_ID(val) vxge_vBIT(val, 0, 16)
#define VXGE_HW_TITAN_ASIC_ID_INITIAL_MAJOR_REVISION(val) vxge_vBIT(val, 48, 8)
#define VXGE_HW_TITAN_ASIC_ID_INITIAL_MINOR_REVISION(val) vxge_vBIT(val, 56, 8)
/*0x00e60*/	u64	titan_general_int_status;
#define	VXGE_HW_TITAN_GENERAL_INT_STATUS_MRPCIM_ALARM_INT	vxge_mBIT(0)
#define	VXGE_HW_TITAN_GENERAL_INT_STATUS_SRPCIM_ALARM_INT	vxge_mBIT(1)
#define	VXGE_HW_TITAN_GENERAL_INT_STATUS_VPATH_ALARM_INT	vxge_mBIT(2)
#define	VXGE_HW_TITAN_GENERAL_INT_STATUS_VPATH_TRAFFIC_INT(val) \
							vxge_vBIT(val, 3, 17)
	u8	unused00e70[0x00e70-0x00e68];

/*0x00e70*/	u64	titan_mask_all_int;
#define	VXGE_HW_TITAN_MASK_ALL_INT_ALARM	vxge_mBIT(7)
#define	VXGE_HW_TITAN_MASK_ALL_INT_TRAFFIC	vxge_mBIT(15)
	u8	unused00e80[0x00e80-0x00e78];

/*0x00e80*/	u64	tim_int_status0;
#define VXGE_HW_TIM_INT_STATUS0_TIM_INT_STATUS0(val) vxge_vBIT(val, 0, 64)
/*0x00e88*/	u64	tim_int_mask0;
#define VXGE_HW_TIM_INT_MASK0_TIM_INT_MASK0(val) vxge_vBIT(val, 0, 64)
/*0x00e90*/	u64	tim_int_status1;
#define VXGE_HW_TIM_INT_STATUS1_TIM_INT_STATUS1(val) vxge_vBIT(val, 0, 4)
/*0x00e98*/	u64	tim_int_mask1;
#define VXGE_HW_TIM_INT_MASK1_TIM_INT_MASK1(val) vxge_vBIT(val, 0, 4)
/*0x00ea0*/	u64	rti_int_status;
#define VXGE_HW_RTI_INT_STATUS_RTI_INT_STATUS(val) vxge_vBIT(val, 0, 17)
/*0x00ea8*/	u64	rti_int_mask;
#define VXGE_HW_RTI_INT_MASK_RTI_INT_MASK(val) vxge_vBIT(val, 0, 17)
/*0x00eb0*/	u64	adapter_status;
#define	VXGE_HW_ADAPTER_STATUS_RTDMA_RTDMA_READY	vxge_mBIT(0)
#define	VXGE_HW_ADAPTER_STATUS_WRDMA_WRDMA_READY	vxge_mBIT(1)
#define	VXGE_HW_ADAPTER_STATUS_KDFC_KDFC_READY	vxge_mBIT(2)
#define	VXGE_HW_ADAPTER_STATUS_TPA_TMAC_BUF_EMPTY	vxge_mBIT(3)
#define	VXGE_HW_ADAPTER_STATUS_RDCTL_PIC_QUIESCENT	vxge_mBIT(4)
#define	VXGE_HW_ADAPTER_STATUS_XGMAC_NETWORK_FAULT	vxge_mBIT(5)
#define	VXGE_HW_ADAPTER_STATUS_ROCRC_OFFLOAD_QUIESCENT	vxge_mBIT(6)
#define	VXGE_HW_ADAPTER_STATUS_G3IF_FB_G3IF_FB_GDDR3_READY	vxge_mBIT(7)
#define	VXGE_HW_ADAPTER_STATUS_G3IF_CM_G3IF_CM_GDDR3_READY	vxge_mBIT(8)
#define	VXGE_HW_ADAPTER_STATUS_RIC_RIC_RUNNING	vxge_mBIT(9)
#define	VXGE_HW_ADAPTER_STATUS_CMG_C_PLL_IN_LOCK	vxge_mBIT(10)
#define	VXGE_HW_ADAPTER_STATUS_XGMAC_X_PLL_IN_LOCK	vxge_mBIT(11)
#define	VXGE_HW_ADAPTER_STATUS_FBIF_M_PLL_IN_LOCK	vxge_mBIT(12)
#define VXGE_HW_ADAPTER_STATUS_PCC_PCC_IDLE(val) vxge_vBIT(val, 24, 8)
#define VXGE_HW_ADAPTER_STATUS_ROCRC_RC_PRC_QUIESCENT(val) vxge_vBIT(val, 44, 8)
/*0x00eb8*/	u64	gen_ctrl;
#define	VXGE_HW_GEN_CTRL_SPI_MRPCIM_WR_DIS	vxge_mBIT(0)
#define	VXGE_HW_GEN_CTRL_SPI_MRPCIM_RD_DIS	vxge_mBIT(1)
#define	VXGE_HW_GEN_CTRL_SPI_SRPCIM_WR_DIS	vxge_mBIT(2)
#define	VXGE_HW_GEN_CTRL_SPI_SRPCIM_RD_DIS	vxge_mBIT(3)
#define	VXGE_HW_GEN_CTRL_SPI_DEBUG_DIS	vxge_mBIT(4)
#define	VXGE_HW_GEN_CTRL_SPI_APP_LTSSM_TIMER_DIS	vxge_mBIT(5)
#define VXGE_HW_GEN_CTRL_SPI_NOT_USED(val) vxge_vBIT(val, 6, 4)
	u8	unused00ed0[0x00ed0-0x00ec0];

/*0x00ed0*/	u64	adapter_ready;
#define	VXGE_HW_ADAPTER_READY_ADAPTER_READY	vxge_mBIT(63)
/*0x00ed8*/	u64	outstanding_read;
#define VXGE_HW_OUTSTANDING_READ_OUTSTANDING_READ(val) vxge_vBIT(val, 0, 17)
/*0x00ee0*/	u64	vpath_rst_in_prog;
#define VXGE_HW_VPATH_RST_IN_PROG_VPATH_RST_IN_PROG(val) vxge_vBIT(val, 0, 17)
/*0x00ee8*/	u64	vpath_reg_modified;
#define VXGE_HW_VPATH_REG_MODIFIED_VPATH_REG_MODIFIED(val) vxge_vBIT(val, 0, 17)
	u8	unused00fc0[0x00fc0-0x00ef0];

/*0x00fc0*/	u64	cp_reset_in_progress;
#define VXGE_HW_CP_RESET_IN_PROGRESS_CP_VPATH(n)	vxge_mBIT(n)
	u8	unused01080[0x01080-0x00fc8];

/*0x01080*/	u64	xgmac_ready;
#define VXGE_HW_XGMAC_READY_XMACJ_READY(val) vxge_vBIT(val, 0, 17)
	u8	unused010c0[0x010c0-0x01088];

/*0x010c0*/	u64	fbif_ready;
#define VXGE_HW_FBIF_READY_FAU_READY(val) vxge_vBIT(val, 0, 17)
	u8	unused01100[0x01100-0x010c8];

/*0x01100*/	u64	vplane_assignments;
#define VXGE_HW_VPLANE_ASSIGNMENTS_VPLANE_ASSIGNMENTS(val) vxge_vBIT(val, 3, 5)
/*0x01108*/	u64	vpath_assignments;
#define VXGE_HW_VPATH_ASSIGNMENTS_VPATH_ASSIGNMENTS(val) vxge_vBIT(val, 0, 17)
/*0x01110*/	u64	resource_assignments;
#define VXGE_HW_RESOURCE_ASSIGNMENTS_RESOURCE_ASSIGNMENTS(val) \
						vxge_vBIT(val, 0, 17)
/*0x01118*/	u64	host_type_assignments;
#define	VXGE_HW_HOST_TYPE_ASSIGNMENTS_HOST_TYPE_ASSIGNMENTS(val) \
							vxge_vBIT(val, 5, 3)
	u8	unused01128[0x01128-0x01120];

/*0x01128*/	u64	max_resource_assignments;
#define VXGE_HW_MAX_RESOURCE_ASSIGNMENTS_PCI_MAX_VPLANE(val) \
							vxge_vBIT(val, 3, 5)
#define VXGE_HW_MAX_RESOURCE_ASSIGNMENTS_PCI_MAX_VPATHS(val) \
						vxge_vBIT(val, 11, 5)
/*0x01130*/	u64	pf_vpath_assignments;
#define VXGE_HW_PF_VPATH_ASSIGNMENTS_PF_VPATH_ASSIGNMENTS(val) \
						vxge_vBIT(val, 0, 17)
	u8	unused01200[0x01200-0x01138];

/*0x01200*/	u64	rts_access_icmp;
#define VXGE_HW_RTS_ACCESS_ICMP_EN(val) vxge_vBIT(val, 0, 17)
/*0x01208*/	u64	rts_access_tcpsyn;
#define VXGE_HW_RTS_ACCESS_TCPSYN_EN(val) vxge_vBIT(val, 0, 17)
/*0x01210*/	u64	rts_access_zl4pyld;
#define VXGE_HW_RTS_ACCESS_ZL4PYLD_EN(val) vxge_vBIT(val, 0, 17)
/*0x01218*/	u64	rts_access_l4prtcl_tcp;
#define VXGE_HW_RTS_ACCESS_L4PRTCL_TCP_EN(val) vxge_vBIT(val, 0, 17)
/*0x01220*/	u64	rts_access_l4prtcl_udp;
#define VXGE_HW_RTS_ACCESS_L4PRTCL_UDP_EN(val) vxge_vBIT(val, 0, 17)
/*0x01228*/	u64	rts_access_l4prtcl_flex;
#define VXGE_HW_RTS_ACCESS_L4PRTCL_FLEX_EN(val) vxge_vBIT(val, 0, 17)
/*0x01230*/	u64	rts_access_ipfrag;
#define VXGE_HW_RTS_ACCESS_IPFRAG_EN(val) vxge_vBIT(val, 0, 17)

} __packed;

struct vxge_hw_memrepair_reg {
	u64	unused1;
	u64	unused2;
} __packed;

struct vxge_hw_pcicfgmgmt_reg {

/*0x00000*/	u64	resource_no;
#define	VXGE_HW_RESOURCE_NO_PFN_OR_VF	BIT(3)
/*0x00008*/	u64	bargrp_pf_or_vf_bar0_mask;
#define	VXGE_HW_BARGRP_PF_OR_VF_BAR0_MASK_BARGRP_PF_OR_VF_BAR0_MASK(val) \
							vxge_vBIT(val, 2, 6)
/*0x00010*/	u64	bargrp_pf_or_vf_bar1_mask;
#define	VXGE_HW_BARGRP_PF_OR_VF_BAR1_MASK_BARGRP_PF_OR_VF_BAR1_MASK(val) \
							vxge_vBIT(val, 2, 6)
/*0x00018*/	u64	bargrp_pf_or_vf_bar2_mask;
#define	VXGE_HW_BARGRP_PF_OR_VF_BAR2_MASK_BARGRP_PF_OR_VF_BAR2_MASK(val) \
							vxge_vBIT(val, 2, 6)
/*0x00020*/	u64	msixgrp_no;
#define VXGE_HW_MSIXGRP_NO_TABLE_SIZE(val) vxge_vBIT(val, 5, 11)

} __packed;

struct vxge_hw_mrpcim_reg {
/*0x00000*/	u64	g3fbct_int_status;
#define	VXGE_HW_G3FBCT_INT_STATUS_ERR_G3IF_INT	vxge_mBIT(0)
/*0x00008*/	u64	g3fbct_int_mask;
/*0x00010*/	u64	g3fbct_err_reg;
#define	VXGE_HW_G3FBCT_ERR_REG_G3IF_SM_ERR	vxge_mBIT(4)
#define	VXGE_HW_G3FBCT_ERR_REG_G3IF_GDDR3_DECC	vxge_mBIT(5)
#define	VXGE_HW_G3FBCT_ERR_REG_G3IF_GDDR3_U_DECC	vxge_mBIT(6)
#define	VXGE_HW_G3FBCT_ERR_REG_G3IF_CTRL_FIFO_DECC	vxge_mBIT(7)
#define	VXGE_HW_G3FBCT_ERR_REG_G3IF_GDDR3_SECC	vxge_mBIT(29)
#define	VXGE_HW_G3FBCT_ERR_REG_G3IF_GDDR3_U_SECC	vxge_mBIT(30)
#define	VXGE_HW_G3FBCT_ERR_REG_G3IF_CTRL_FIFO_SECC	vxge_mBIT(31)
/*0x00018*/	u64	g3fbct_err_mask;
/*0x00020*/	u64	g3fbct_err_alarm;

	u8	unused00a00[0x00a00-0x00028];

/*0x00a00*/	u64	wrdma_int_status;
#define	VXGE_HW_WRDMA_INT_STATUS_RC_ALARM_RC_INT	vxge_mBIT(0)
#define	VXGE_HW_WRDMA_INT_STATUS_RXDRM_SM_ERR_RXDRM_INT	vxge_mBIT(1)
#define	VXGE_HW_WRDMA_INT_STATUS_RXDCM_SM_ERR_RXDCM_SM_INT	vxge_mBIT(2)
#define	VXGE_HW_WRDMA_INT_STATUS_RXDWM_SM_ERR_RXDWM_INT	vxge_mBIT(3)
#define	VXGE_HW_WRDMA_INT_STATUS_RDA_ERR_RDA_INT	vxge_mBIT(6)
#define	VXGE_HW_WRDMA_INT_STATUS_RDA_ECC_DB_RDA_ECC_DB_INT	vxge_mBIT(8)
#define	VXGE_HW_WRDMA_INT_STATUS_RDA_ECC_SG_RDA_ECC_SG_INT	vxge_mBIT(9)
#define	VXGE_HW_WRDMA_INT_STATUS_FRF_ALARM_FRF_INT	vxge_mBIT(12)
#define	VXGE_HW_WRDMA_INT_STATUS_ROCRC_ALARM_ROCRC_INT	vxge_mBIT(13)
#define	VXGE_HW_WRDMA_INT_STATUS_WDE0_ALARM_WDE0_INT	vxge_mBIT(14)
#define	VXGE_HW_WRDMA_INT_STATUS_WDE1_ALARM_WDE1_INT	vxge_mBIT(15)
#define	VXGE_HW_WRDMA_INT_STATUS_WDE2_ALARM_WDE2_INT	vxge_mBIT(16)
#define	VXGE_HW_WRDMA_INT_STATUS_WDE3_ALARM_WDE3_INT	vxge_mBIT(17)
/*0x00a08*/	u64	wrdma_int_mask;
/*0x00a10*/	u64	rc_alarm_reg;
#define	VXGE_HW_RC_ALARM_REG_FTC_SM_ERR	vxge_mBIT(0)
#define	VXGE_HW_RC_ALARM_REG_FTC_SM_PHASE_ERR	vxge_mBIT(1)
#define	VXGE_HW_RC_ALARM_REG_BTDWM_SM_ERR	vxge_mBIT(2)
#define	VXGE_HW_RC_ALARM_REG_BTC_SM_ERR	vxge_mBIT(3)
#define	VXGE_HW_RC_ALARM_REG_BTDCM_SM_ERR	vxge_mBIT(4)
#define	VXGE_HW_RC_ALARM_REG_BTDRM_SM_ERR	vxge_mBIT(5)
#define	VXGE_HW_RC_ALARM_REG_RMM_RXD_RC_ECC_DB_ERR	vxge_mBIT(6)
#define	VXGE_HW_RC_ALARM_REG_RMM_RXD_RC_ECC_SG_ERR	vxge_mBIT(7)
#define	VXGE_HW_RC_ALARM_REG_RHS_RXD_RHS_ECC_DB_ERR	vxge_mBIT(8)
#define	VXGE_HW_RC_ALARM_REG_RHS_RXD_RHS_ECC_SG_ERR	vxge_mBIT(9)
#define	VXGE_HW_RC_ALARM_REG_RMM_SM_ERR	vxge_mBIT(10)
#define	VXGE_HW_RC_ALARM_REG_BTC_VPATH_MISMATCH_ERR	vxge_mBIT(12)
/*0x00a18*/	u64	rc_alarm_mask;
/*0x00a20*/	u64	rc_alarm_alarm;
/*0x00a28*/	u64	rxdrm_sm_err_reg;
#define VXGE_HW_RXDRM_SM_ERR_REG_PRC_VP(n)	vxge_mBIT(n)
/*0x00a30*/	u64	rxdrm_sm_err_mask;
/*0x00a38*/	u64	rxdrm_sm_err_alarm;
/*0x00a40*/	u64	rxdcm_sm_err_reg;
#define VXGE_HW_RXDCM_SM_ERR_REG_PRC_VP(n)	vxge_mBIT(n)
/*0x00a48*/	u64	rxdcm_sm_err_mask;
/*0x00a50*/	u64	rxdcm_sm_err_alarm;
/*0x00a58*/	u64	rxdwm_sm_err_reg;
#define VXGE_HW_RXDWM_SM_ERR_REG_PRC_VP(n)	vxge_mBIT(n)
/*0x00a60*/	u64	rxdwm_sm_err_mask;
/*0x00a68*/	u64	rxdwm_sm_err_alarm;
/*0x00a70*/	u64	rda_err_reg;
#define	VXGE_HW_RDA_ERR_REG_RDA_SM0_ERR_ALARM	vxge_mBIT(0)
#define	VXGE_HW_RDA_ERR_REG_RDA_MISC_ERR	vxge_mBIT(1)
#define	VXGE_HW_RDA_ERR_REG_RDA_PCIX_ERR	vxge_mBIT(2)
#define	VXGE_HW_RDA_ERR_REG_RDA_RXD_ECC_DB_ERR	vxge_mBIT(3)
#define	VXGE_HW_RDA_ERR_REG_RDA_FRM_ECC_DB_ERR	vxge_mBIT(4)
#define	VXGE_HW_RDA_ERR_REG_RDA_UQM_ECC_DB_ERR	vxge_mBIT(5)
#define	VXGE_HW_RDA_ERR_REG_RDA_IMM_ECC_DB_ERR	vxge_mBIT(6)
#define	VXGE_HW_RDA_ERR_REG_RDA_TIM_ECC_DB_ERR	vxge_mBIT(7)
/*0x00a78*/	u64	rda_err_mask;
/*0x00a80*/	u64	rda_err_alarm;
/*0x00a88*/	u64	rda_ecc_db_reg;
#define VXGE_HW_RDA_ECC_DB_REG_RDA_RXD_ERR(n)	vxge_mBIT(n)
/*0x00a90*/	u64	rda_ecc_db_mask;
/*0x00a98*/	u64	rda_ecc_db_alarm;
/*0x00aa0*/	u64	rda_ecc_sg_reg;
#define VXGE_HW_RDA_ECC_SG_REG_RDA_RXD_ERR(n)	vxge_mBIT(n)
/*0x00aa8*/	u64	rda_ecc_sg_mask;
/*0x00ab0*/	u64	rda_ecc_sg_alarm;
/*0x00ab8*/	u64	rqa_err_reg;
#define	VXGE_HW_RQA_ERR_REG_RQA_SM_ERR_ALARM	vxge_mBIT(0)
/*0x00ac0*/	u64	rqa_err_mask;
/*0x00ac8*/	u64	rqa_err_alarm;
/*0x00ad0*/	u64	frf_alarm_reg;
#define VXGE_HW_FRF_ALARM_REG_PRC_VP_FRF_SM_ERR(n)	vxge_mBIT(n)
/*0x00ad8*/	u64	frf_alarm_mask;
/*0x00ae0*/	u64	frf_alarm_alarm;
/*0x00ae8*/	u64	rocrc_alarm_reg;
#define	VXGE_HW_ROCRC_ALARM_REG_QCQ_QCC_BYP_ECC_DB	vxge_mBIT(0)
#define	VXGE_HW_ROCRC_ALARM_REG_QCQ_QCC_BYP_ECC_SG	vxge_mBIT(1)
#define	VXGE_HW_ROCRC_ALARM_REG_NOA_NMA_SM_ERR	vxge_mBIT(2)
#define	VXGE_HW_ROCRC_ALARM_REG_NOA_IMMM_ECC_DB	vxge_mBIT(3)
#define	VXGE_HW_ROCRC_ALARM_REG_NOA_IMMM_ECC_SG	vxge_mBIT(4)
#define	VXGE_HW_ROCRC_ALARM_REG_UDQ_UMQM_ECC_DB	vxge_mBIT(5)
#define	VXGE_HW_ROCRC_ALARM_REG_UDQ_UMQM_ECC_SG	vxge_mBIT(6)
#define	VXGE_HW_ROCRC_ALARM_REG_NOA_RCBM_ECC_DB	vxge_mBIT(11)
#define	VXGE_HW_ROCRC_ALARM_REG_NOA_RCBM_ECC_SG	vxge_mBIT(12)
#define	VXGE_HW_ROCRC_ALARM_REG_QCQ_MULTI_EGB_RSVD_ERR	vxge_mBIT(13)
#define	VXGE_HW_ROCRC_ALARM_REG_QCQ_MULTI_EGB_OWN_ERR	vxge_mBIT(14)
#define	VXGE_HW_ROCRC_ALARM_REG_QCQ_MULTI_BYP_OWN_ERR	vxge_mBIT(15)
#define	VXGE_HW_ROCRC_ALARM_REG_QCQ_OWN_NOT_ASSIGNED_ERR	vxge_mBIT(16)
#define	VXGE_HW_ROCRC_ALARM_REG_QCQ_OWN_RSVD_SYNC_ERR	vxge_mBIT(17)
#define	VXGE_HW_ROCRC_ALARM_REG_QCQ_LOST_EGB_ERR	vxge_mBIT(18)
#define	VXGE_HW_ROCRC_ALARM_REG_RCQ_BYPQ0_OVERFLOW	vxge_mBIT(19)
#define	VXGE_HW_ROCRC_ALARM_REG_RCQ_BYPQ1_OVERFLOW	vxge_mBIT(20)
#define	VXGE_HW_ROCRC_ALARM_REG_RCQ_BYPQ2_OVERFLOW	vxge_mBIT(21)
#define	VXGE_HW_ROCRC_ALARM_REG_NOA_WCT_CMD_FIFO_ERR	vxge_mBIT(22)
/*0x00af0*/	u64	rocrc_alarm_mask;
/*0x00af8*/	u64	rocrc_alarm_alarm;
/*0x00b00*/	u64	wde0_alarm_reg;
#define	VXGE_HW_WDE0_ALARM_REG_WDE0_DCC_SM_ERR	vxge_mBIT(0)
#define	VXGE_HW_WDE0_ALARM_REG_WDE0_PRM_SM_ERR	vxge_mBIT(1)
#define	VXGE_HW_WDE0_ALARM_REG_WDE0_CP_SM_ERR	vxge_mBIT(2)
#define	VXGE_HW_WDE0_ALARM_REG_WDE0_CP_CMD_ERR	vxge_mBIT(3)
#define	VXGE_HW_WDE0_ALARM_REG_WDE0_PCR_SM_ERR	vxge_mBIT(4)
/*0x00b08*/	u64	wde0_alarm_mask;
/*0x00b10*/	u64	wde0_alarm_alarm;
/*0x00b18*/	u64	wde1_alarm_reg;
#define	VXGE_HW_WDE1_ALARM_REG_WDE1_DCC_SM_ERR	vxge_mBIT(0)
#define	VXGE_HW_WDE1_ALARM_REG_WDE1_PRM_SM_ERR	vxge_mBIT(1)
#define	VXGE_HW_WDE1_ALARM_REG_WDE1_CP_SM_ERR	vxge_mBIT(2)
#define	VXGE_HW_WDE1_ALARM_REG_WDE1_CP_CMD_ERR	vxge_mBIT(3)
#define	VXGE_HW_WDE1_ALARM_REG_WDE1_PCR_SM_ERR	vxge_mBIT(4)
/*0x00b20*/	u64	wde1_alarm_mask;
/*0x00b28*/	u64	wde1_alarm_alarm;
/*0x00b30*/	u64	wde2_alarm_reg;
#define	VXGE_HW_WDE2_ALARM_REG_WDE2_DCC_SM_ERR	vxge_mBIT(0)
#define	VXGE_HW_WDE2_ALARM_REG_WDE2_PRM_SM_ERR	vxge_mBIT(1)
#define	VXGE_HW_WDE2_ALARM_REG_WDE2_CP_SM_ERR	vxge_mBIT(2)
#define	VXGE_HW_WDE2_ALARM_REG_WDE2_CP_CMD_ERR	vxge_mBIT(3)
#define	VXGE_HW_WDE2_ALARM_REG_WDE2_PCR_SM_ERR	vxge_mBIT(4)
/*0x00b38*/	u64	wde2_alarm_mask;
/*0x00b40*/	u64	wde2_alarm_alarm;
/*0x00b48*/	u64	wde3_alarm_reg;
#define	VXGE_HW_WDE3_ALARM_REG_WDE3_DCC_SM_ERR	vxge_mBIT(0)
#define	VXGE_HW_WDE3_ALARM_REG_WDE3_PRM_SM_ERR	vxge_mBIT(1)
#define	VXGE_HW_WDE3_ALARM_REG_WDE3_CP_SM_ERR	vxge_mBIT(2)
#define	VXGE_HW_WDE3_ALARM_REG_WDE3_CP_CMD_ERR	vxge_mBIT(3)
#define	VXGE_HW_WDE3_ALARM_REG_WDE3_PCR_SM_ERR	vxge_mBIT(4)
/*0x00b50*/	u64	wde3_alarm_mask;
/*0x00b58*/	u64	wde3_alarm_alarm;

	u8	unused00be8[0x00be8-0x00b60];

/*0x00be8*/	u64	rx_w_round_robin_0;
#define VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_0(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_1(val) vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_2(val) vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_3(val) vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_4(val) vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_5(val) vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_6(val) vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_7(val) vxge_vBIT(val, 59, 5)
/*0x00bf0*/	u64	rx_w_round_robin_1;
#define VXGE_HW_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_8(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_9(val) vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_10(val) \
						vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_11(val) \
						vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_12(val) \
						vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_13(val) \
						vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_14(val) \
						vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_15(val) \
						vxge_vBIT(val, 59, 5)
/*0x00bf8*/	u64	rx_w_round_robin_2;
#define VXGE_HW_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_16(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_17(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_18(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_19(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_20(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_21(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_22(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_23(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c00*/	u64	rx_w_round_robin_3;
#define VXGE_HW_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_24(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_25(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_26(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_27(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_28(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_29(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_30(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_31(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c08*/	u64	rx_w_round_robin_4;
#define VXGE_HW_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_32(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_33(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_34(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_35(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_36(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_37(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_38(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_39(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c10*/	u64	rx_w_round_robin_5;
#define VXGE_HW_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_40(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_41(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_42(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_43(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_44(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_45(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_46(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_47(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c18*/	u64	rx_w_round_robin_6;
#define VXGE_HW_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_48(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_49(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_50(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_51(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_52(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_53(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_54(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_55(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c20*/	u64	rx_w_round_robin_7;
#define VXGE_HW_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_56(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_57(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_58(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_59(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_60(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_61(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_62(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_63(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c28*/	u64	rx_w_round_robin_8;
#define VXGE_HW_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_64(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_65(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_66(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_67(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_68(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_69(val) \
						vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_70(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_71(val) \
						vxge_vBIT(val, 59, 5)
/*0x00c30*/	u64	rx_w_round_robin_9;
#define VXGE_HW_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_72(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_73(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_74(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_75(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_76(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_77(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_78(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_79(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c38*/	u64	rx_w_round_robin_10;
#define VXGE_HW_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_80(val) \
							vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_81(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_82(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_83(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_84(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_85(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_86(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_87(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c40*/	u64	rx_w_round_robin_11;
#define VXGE_HW_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_88(val) \
							vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_89(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_90(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_91(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_92(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_93(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_94(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_95(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c48*/	u64	rx_w_round_robin_12;
#define VXGE_HW_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_96(val) \
							vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_97(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_98(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_99(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_100(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_101(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_102(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_103(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c50*/	u64	rx_w_round_robin_13;
#define VXGE_HW_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_104(val) \
							vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_105(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_106(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_107(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_108(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_109(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_110(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_111(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c58*/	u64	rx_w_round_robin_14;
#define VXGE_HW_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_112(val) \
							vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_113(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_114(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_115(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_116(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_117(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_118(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_119(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c60*/	u64	rx_w_round_robin_15;
#define VXGE_HW_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_120(val) \
							vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_121(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_122(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_123(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_124(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_125(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_126(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_127(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c68*/	u64	rx_w_round_robin_16;
#define VXGE_HW_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_128(val) \
							vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_129(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_130(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_131(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_132(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_133(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_134(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_135(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c70*/	u64	rx_w_round_robin_17;
#define VXGE_HW_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_136(val) \
							vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_137(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_138(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_139(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_140(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_141(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_142(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_143(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c78*/	u64	rx_w_round_robin_18;
#define VXGE_HW_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_144(val) \
							vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_145(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_146(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_147(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_148(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_149(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_150(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_151(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c80*/	u64	rx_w_round_robin_19;
#define VXGE_HW_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_152(val) \
							vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_153(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_154(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_155(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_156(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_157(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_158(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_159(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c88*/	u64	rx_w_round_robin_20;
#define VXGE_HW_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_160(val) \
							vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_161(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_162(val) \
							vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_163(val) \
							vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_164(val) \
							vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_165(val) \
							vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_166(val) \
							vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_167(val) \
							vxge_vBIT(val, 59, 5)
/*0x00c90*/	u64	rx_w_round_robin_21;
#define VXGE_HW_RX_W_ROUND_ROBIN_21_RX_W_PRIORITY_SS_168(val) \
							vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_21_RX_W_PRIORITY_SS_169(val) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_21_RX_W_PRIORITY_SS_170(val) \
							vxge_vBIT(val, 19, 5)

#define VXGE_HW_WRR_RING_SERVICE_STATES			171
#define VXGE_HW_WRR_RING_COUNT				22

/*0x00c98*/	u64	rx_queue_priority_0;
#define VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_0(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_1(val) vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_2(val) vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_3(val) vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_4(val) vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_5(val) vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_6(val) vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_7(val) vxge_vBIT(val, 59, 5)
/*0x00ca0*/	u64	rx_queue_priority_1;
#define VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_8(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_9(val) vxge_vBIT(val, 11, 5)
#define VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_10(val) vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_11(val) vxge_vBIT(val, 27, 5)
#define VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_12(val) vxge_vBIT(val, 35, 5)
#define VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_13(val) vxge_vBIT(val, 43, 5)
#define VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_14(val) vxge_vBIT(val, 51, 5)
#define VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_15(val) vxge_vBIT(val, 59, 5)
/*0x00ca8*/	u64	rx_queue_priority_2;
#define VXGE_HW_RX_QUEUE_PRIORITY_2_RX_Q_NUMBER_16(val) vxge_vBIT(val, 3, 5)
	u8	unused00cc8[0x00cc8-0x00cb0];

/*0x00cc8*/	u64	replication_queue_priority;
#define	VXGE_HW_REPLICATION_QUEUE_PRIORITY_REPLICATION_QUEUE_PRIORITY(val) \
							vxge_vBIT(val, 59, 5)
/*0x00cd0*/	u64	rx_queue_select;
#define VXGE_HW_RX_QUEUE_SELECT_NUMBER(n)	vxge_mBIT(n)
#define	VXGE_HW_RX_QUEUE_SELECT_ENABLE_CODE	vxge_mBIT(15)
#define	VXGE_HW_RX_QUEUE_SELECT_ENABLE_HIERARCHICAL_PRTY	vxge_mBIT(23)
/*0x00cd8*/	u64	rqa_vpbp_ctrl;
#define	VXGE_HW_RQA_VPBP_CTRL_WR_XON_DIS	vxge_mBIT(15)
#define	VXGE_HW_RQA_VPBP_CTRL_ROCRC_DIS	vxge_mBIT(23)
#define	VXGE_HW_RQA_VPBP_CTRL_TXPE_DIS	vxge_mBIT(31)
/*0x00ce0*/	u64	rx_multi_cast_ctrl;
#define	VXGE_HW_RX_MULTI_CAST_CTRL_TIME_OUT_DIS	vxge_mBIT(0)
#define	VXGE_HW_RX_MULTI_CAST_CTRL_FRM_DROP_DIS	vxge_mBIT(1)
#define VXGE_HW_RX_MULTI_CAST_CTRL_NO_RXD_TIME_OUT_CNT(val) \
							vxge_vBIT(val, 2, 30)
#define VXGE_HW_RX_MULTI_CAST_CTRL_TIME_OUT_CNT(val) vxge_vBIT(val, 32, 32)
/*0x00ce8*/	u64	wde_prm_ctrl;
#define VXGE_HW_WDE_PRM_CTRL_SPAV_THRESHOLD(val) vxge_vBIT(val, 2, 10)
#define VXGE_HW_WDE_PRM_CTRL_SPLIT_THRESHOLD(val) vxge_vBIT(val, 18, 14)
#define	VXGE_HW_WDE_PRM_CTRL_SPLIT_ON_1ST_ROW	vxge_mBIT(32)
#define	VXGE_HW_WDE_PRM_CTRL_SPLIT_ON_ROW_BNDRY	vxge_mBIT(33)
#define VXGE_HW_WDE_PRM_CTRL_FB_ROW_SIZE(val) vxge_vBIT(val, 46, 2)
/*0x00cf0*/	u64	noa_ctrl;
#define VXGE_HW_NOA_CTRL_FRM_PRTY_QUOTA(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_NOA_CTRL_NON_FRM_PRTY_QUOTA(val) vxge_vBIT(val, 11, 5)
#define	VXGE_HW_NOA_CTRL_IGNORE_KDFC_IF_STATUS	vxge_mBIT(16)
#define VXGE_HW_NOA_CTRL_MAX_JOB_CNT_FOR_WDE0(val) vxge_vBIT(val, 37, 4)
#define VXGE_HW_NOA_CTRL_MAX_JOB_CNT_FOR_WDE1(val) vxge_vBIT(val, 45, 4)
#define VXGE_HW_NOA_CTRL_MAX_JOB_CNT_FOR_WDE2(val) vxge_vBIT(val, 53, 4)
#define VXGE_HW_NOA_CTRL_MAX_JOB_CNT_FOR_WDE3(val) vxge_vBIT(val, 60, 4)
/*0x00cf8*/	u64	phase_cfg;
#define	VXGE_HW_PHASE_CFG_QCC_WR_PHASE_EN	vxge_mBIT(0)
#define	VXGE_HW_PHASE_CFG_QCC_RD_PHASE_EN	vxge_mBIT(3)
#define	VXGE_HW_PHASE_CFG_IMMM_WR_PHASE_EN	vxge_mBIT(7)
#define	VXGE_HW_PHASE_CFG_IMMM_RD_PHASE_EN	vxge_mBIT(11)
#define	VXGE_HW_PHASE_CFG_UMQM_WR_PHASE_EN	vxge_mBIT(15)
#define	VXGE_HW_PHASE_CFG_UMQM_RD_PHASE_EN	vxge_mBIT(19)
#define	VXGE_HW_PHASE_CFG_RCBM_WR_PHASE_EN	vxge_mBIT(23)
#define	VXGE_HW_PHASE_CFG_RCBM_RD_PHASE_EN	vxge_mBIT(27)
#define	VXGE_HW_PHASE_CFG_RXD_RC_WR_PHASE_EN	vxge_mBIT(31)
#define	VXGE_HW_PHASE_CFG_RXD_RC_RD_PHASE_EN	vxge_mBIT(35)
#define	VXGE_HW_PHASE_CFG_RXD_RHS_WR_PHASE_EN	vxge_mBIT(39)
#define	VXGE_HW_PHASE_CFG_RXD_RHS_RD_PHASE_EN	vxge_mBIT(43)
/*0x00d00*/	u64	rcq_bypq_cfg;
#define VXGE_HW_RCQ_BYPQ_CFG_OVERFLOW_THRESHOLD(val) vxge_vBIT(val, 10, 22)
#define VXGE_HW_RCQ_BYPQ_CFG_BYP_ON_THRESHOLD(val) vxge_vBIT(val, 39, 9)
#define VXGE_HW_RCQ_BYPQ_CFG_BYP_OFF_THRESHOLD(val) vxge_vBIT(val, 55, 9)
	u8	unused00e00[0x00e00-0x00d08];

/*0x00e00*/	u64	doorbell_int_status;
#define	VXGE_HW_DOORBELL_INT_STATUS_KDFC_ERR_REG_TXDMA_KDFC_INT	vxge_mBIT(7)
#define	VXGE_HW_DOORBELL_INT_STATUS_USDC_ERR_REG_TXDMA_USDC_INT	vxge_mBIT(15)
/*0x00e08*/	u64	doorbell_int_mask;
/*0x00e10*/	u64	kdfc_err_reg;
#define	VXGE_HW_KDFC_ERR_REG_KDFC_KDFC_ECC_SG_ERR	vxge_mBIT(7)
#define	VXGE_HW_KDFC_ERR_REG_KDFC_KDFC_ECC_DB_ERR	vxge_mBIT(15)
#define	VXGE_HW_KDFC_ERR_REG_KDFC_KDFC_SM_ERR_ALARM	vxge_mBIT(23)
#define	VXGE_HW_KDFC_ERR_REG_KDFC_KDFC_MISC_ERR_1	vxge_mBIT(32)
#define	VXGE_HW_KDFC_ERR_REG_KDFC_KDFC_PCIX_ERR	vxge_mBIT(39)
/*0x00e18*/	u64	kdfc_err_mask;
/*0x00e20*/	u64	kdfc_err_reg_alarm;
#define	VXGE_HW_KDFC_ERR_REG_ALARM_KDFC_KDFC_ECC_SG_ERR	vxge_mBIT(7)
#define	VXGE_HW_KDFC_ERR_REG_ALARM_KDFC_KDFC_ECC_DB_ERR	vxge_mBIT(15)
#define	VXGE_HW_KDFC_ERR_REG_ALARM_KDFC_KDFC_SM_ERR_ALARM	vxge_mBIT(23)
#define	VXGE_HW_KDFC_ERR_REG_ALARM_KDFC_KDFC_MISC_ERR_1	vxge_mBIT(32)
#define	VXGE_HW_KDFC_ERR_REG_ALARM_KDFC_KDFC_PCIX_ERR	vxge_mBIT(39)
	u8	unused00e40[0x00e40-0x00e28];
/*0x00e40*/	u64	kdfc_vp_partition_0;
#define	VXGE_HW_KDFC_VP_PARTITION_0_ENABLE	vxge_mBIT(0)
#define VXGE_HW_KDFC_VP_PARTITION_0_NUMBER_0(val) vxge_vBIT(val, 5, 3)
#define VXGE_HW_KDFC_VP_PARTITION_0_LENGTH_0(val) vxge_vBIT(val, 17, 15)
#define VXGE_HW_KDFC_VP_PARTITION_0_NUMBER_1(val) vxge_vBIT(val, 37, 3)
#define VXGE_HW_KDFC_VP_PARTITION_0_LENGTH_1(val) vxge_vBIT(val, 49, 15)
/*0x00e48*/	u64	kdfc_vp_partition_1;
#define VXGE_HW_KDFC_VP_PARTITION_1_NUMBER_2(val) vxge_vBIT(val, 5, 3)
#define VXGE_HW_KDFC_VP_PARTITION_1_LENGTH_2(val) vxge_vBIT(val, 17, 15)
#define VXGE_HW_KDFC_VP_PARTITION_1_NUMBER_3(val) vxge_vBIT(val, 37, 3)
#define VXGE_HW_KDFC_VP_PARTITION_1_LENGTH_3(val) vxge_vBIT(val, 49, 15)
/*0x00e50*/	u64	kdfc_vp_partition_2;
#define VXGE_HW_KDFC_VP_PARTITION_2_NUMBER_4(val) vxge_vBIT(val, 5, 3)
#define VXGE_HW_KDFC_VP_PARTITION_2_LENGTH_4(val) vxge_vBIT(val, 17, 15)
#define VXGE_HW_KDFC_VP_PARTITION_2_NUMBER_5(val) vxge_vBIT(val, 37, 3)
#define VXGE_HW_KDFC_VP_PARTITION_2_LENGTH_5(val) vxge_vBIT(val, 49, 15)
/*0x00e58*/	u64	kdfc_vp_partition_3;
#define VXGE_HW_KDFC_VP_PARTITION_3_NUMBER_6(val) vxge_vBIT(val, 5, 3)
#define VXGE_HW_KDFC_VP_PARTITION_3_LENGTH_6(val) vxge_vBIT(val, 17, 15)
#define VXGE_HW_KDFC_VP_PARTITION_3_NUMBER_7(val) vxge_vBIT(val, 37, 3)
#define VXGE_HW_KDFC_VP_PARTITION_3_LENGTH_7(val) vxge_vBIT(val, 49, 15)
/*0x00e60*/	u64	kdfc_vp_partition_4;
#define VXGE_HW_KDFC_VP_PARTITION_4_LENGTH_8(val) vxge_vBIT(val, 17, 15)
#define VXGE_HW_KDFC_VP_PARTITION_4_LENGTH_9(val) vxge_vBIT(val, 49, 15)
/*0x00e68*/	u64	kdfc_vp_partition_5;
#define VXGE_HW_KDFC_VP_PARTITION_5_LENGTH_10(val) vxge_vBIT(val, 17, 15)
#define VXGE_HW_KDFC_VP_PARTITION_5_LENGTH_11(val) vxge_vBIT(val, 49, 15)
/*0x00e70*/	u64	kdfc_vp_partition_6;
#define VXGE_HW_KDFC_VP_PARTITION_6_LENGTH_12(val) vxge_vBIT(val, 17, 15)
#define VXGE_HW_KDFC_VP_PARTITION_6_LENGTH_13(val) vxge_vBIT(val, 49, 15)
/*0x00e78*/	u64	kdfc_vp_partition_7;
#define VXGE_HW_KDFC_VP_PARTITION_7_LENGTH_14(val) vxge_vBIT(val, 17, 15)
#define VXGE_HW_KDFC_VP_PARTITION_7_LENGTH_15(val) vxge_vBIT(val, 49, 15)
/*0x00e80*/	u64	kdfc_vp_partition_8;
#define VXGE_HW_KDFC_VP_PARTITION_8_LENGTH_16(val) vxge_vBIT(val, 17, 15)
/*0x00e88*/	u64	kdfc_w_round_robin_0;
#define VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_0(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_1(val) vxge_vBIT(val, 11, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_2(val) vxge_vBIT(val, 19, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_3(val) vxge_vBIT(val, 27, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_4(val) vxge_vBIT(val, 35, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_5(val) vxge_vBIT(val, 43, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_6(val) vxge_vBIT(val, 51, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_7(val) vxge_vBIT(val, 59, 5)

	u8	unused0f28[0x0f28-0x0e90];

/*0x00f28*/	u64	kdfc_w_round_robin_20;
#define VXGE_HW_KDFC_W_ROUND_ROBIN_20_NUMBER_0(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_20_NUMBER_1(val) vxge_vBIT(val, 11, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_20_NUMBER_2(val) vxge_vBIT(val, 19, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_20_NUMBER_3(val) vxge_vBIT(val, 27, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_20_NUMBER_4(val) vxge_vBIT(val, 35, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_20_NUMBER_5(val) vxge_vBIT(val, 43, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_20_NUMBER_6(val) vxge_vBIT(val, 51, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_20_NUMBER_7(val) vxge_vBIT(val, 59, 5)

#define VXGE_HW_WRR_FIFO_COUNT				20

	u8	unused0fc8[0x0fc8-0x0f30];

/*0x00fc8*/	u64	kdfc_w_round_robin_40;
#define VXGE_HW_KDFC_W_ROUND_ROBIN_40_NUMBER_0(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_40_NUMBER_1(val) vxge_vBIT(val, 11, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_40_NUMBER_2(val) vxge_vBIT(val, 19, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_40_NUMBER_3(val) vxge_vBIT(val, 27, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_40_NUMBER_4(val) vxge_vBIT(val, 35, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_40_NUMBER_5(val) vxge_vBIT(val, 43, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_40_NUMBER_6(val) vxge_vBIT(val, 51, 5)
#define VXGE_HW_KDFC_W_ROUND_ROBIN_40_NUMBER_7(val) vxge_vBIT(val, 59, 5)

	u8	unused1068[0x01068-0x0fd0];

/*0x01068*/	u64	kdfc_entry_type_sel_0;
#define VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_0(val) vxge_vBIT(val, 6, 2)
#define VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_1(val) vxge_vBIT(val, 14, 2)
#define VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_2(val) vxge_vBIT(val, 22, 2)
#define VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_3(val) vxge_vBIT(val, 30, 2)
#define VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_4(val) vxge_vBIT(val, 38, 2)
#define VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_5(val) vxge_vBIT(val, 46, 2)
#define VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_6(val) vxge_vBIT(val, 54, 2)
#define VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_7(val) vxge_vBIT(val, 62, 2)
/*0x01070*/	u64	kdfc_entry_type_sel_1;
#define VXGE_HW_KDFC_ENTRY_TYPE_SEL_1_NUMBER_8(val) vxge_vBIT(val, 6, 2)
/*0x01078*/	u64	kdfc_fifo_0_ctrl;
#define VXGE_HW_KDFC_FIFO_0_CTRL_WRR_NUMBER(val) vxge_vBIT(val, 3, 5)
#define VXGE_HW_WEIGHTED_RR_SERVICE_STATES		176
#define VXGE_HW_WRR_FIFO_SERVICE_STATES			153

	u8	unused1100[0x01100-0x1080];

/*0x01100*/	u64	kdfc_fifo_17_ctrl;
#define VXGE_HW_KDFC_FIFO_17_CTRL_WRR_NUMBER(val) vxge_vBIT(val, 3, 5)

	u8	unused1600[0x01600-0x1108];

/*0x01600*/	u64	rxmac_int_status;
#define	VXGE_HW_RXMAC_INT_STATUS_RXMAC_GEN_ERR_RXMAC_GEN_INT	vxge_mBIT(3)
#define	VXGE_HW_RXMAC_INT_STATUS_RXMAC_ECC_ERR_RXMAC_ECC_INT	vxge_mBIT(7)
#define	VXGE_HW_RXMAC_INT_STATUS_RXMAC_VARIOUS_ERR_RXMAC_VARIOUS_INT \
								vxge_mBIT(11)
/*0x01608*/	u64	rxmac_int_mask;
	u8	unused01618[0x01618-0x01610];

/*0x01618*/	u64	rxmac_gen_err_reg;
/*0x01620*/	u64	rxmac_gen_err_mask;
/*0x01628*/	u64	rxmac_gen_err_alarm;
/*0x01630*/	u64	rxmac_ecc_err_reg;
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RMAC_PORT0_RMAC_RTS_PART_SG_ERR(val) \
							vxge_vBIT(val, 0, 4)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RMAC_PORT0_RMAC_RTS_PART_DB_ERR(val) \
							vxge_vBIT(val, 4, 4)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RMAC_PORT1_RMAC_RTS_PART_SG_ERR(val) \
							vxge_vBIT(val, 8, 4)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RMAC_PORT1_RMAC_RTS_PART_DB_ERR(val) \
							vxge_vBIT(val, 12, 4)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RMAC_PORT2_RMAC_RTS_PART_SG_ERR(val) \
							vxge_vBIT(val, 16, 4)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RMAC_PORT2_RMAC_RTS_PART_DB_ERR(val) \
							vxge_vBIT(val, 20, 4)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DA_LKP_PRT0_SG_ERR(val) \
							vxge_vBIT(val, 24, 2)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DA_LKP_PRT0_DB_ERR(val) \
							vxge_vBIT(val, 26, 2)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DA_LKP_PRT1_SG_ERR(val) \
							vxge_vBIT(val, 28, 2)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DA_LKP_PRT1_DB_ERR(val) \
							vxge_vBIT(val, 30, 2)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_VID_LKP_SG_ERR	vxge_mBIT(32)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_VID_LKP_DB_ERR	vxge_mBIT(33)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT0_SG_ERR	vxge_mBIT(34)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT0_DB_ERR	vxge_mBIT(35)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT1_SG_ERR	vxge_mBIT(36)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT1_DB_ERR	vxge_mBIT(37)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT2_SG_ERR	vxge_mBIT(38)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT2_DB_ERR	vxge_mBIT(39)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_RTH_MASK_SG_ERR(val) \
							vxge_vBIT(val, 40, 7)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_RTH_MASK_DB_ERR(val) \
							vxge_vBIT(val, 47, 7)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_RTH_LKP_SG_ERR(val) \
							vxge_vBIT(val, 54, 3)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_RTH_LKP_DB_ERR(val) \
							vxge_vBIT(val, 57, 3)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DS_LKP_SG_ERR \
							vxge_mBIT(60)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DS_LKP_DB_ERR \
							vxge_mBIT(61)
/*0x01638*/	u64	rxmac_ecc_err_mask;
/*0x01640*/	u64	rxmac_ecc_err_alarm;
/*0x01648*/	u64	rxmac_various_err_reg;
#define	VXGE_HW_RXMAC_VARIOUS_ERR_REG_RMAC_RMAC_PORT0_FSM_ERR	vxge_mBIT(0)
#define	VXGE_HW_RXMAC_VARIOUS_ERR_REG_RMAC_RMAC_PORT1_FSM_ERR	vxge_mBIT(1)
#define	VXGE_HW_RXMAC_VARIOUS_ERR_REG_RMAC_RMAC_PORT2_FSM_ERR	vxge_mBIT(2)
#define	VXGE_HW_RXMAC_VARIOUS_ERR_REG_RMACJ_RMACJ_FSM_ERR	vxge_mBIT(3)
/*0x01650*/	u64	rxmac_various_err_mask;
/*0x01658*/	u64	rxmac_various_err_alarm;
/*0x01660*/	u64	rxmac_gen_cfg;
#define	VXGE_HW_RXMAC_GEN_CFG_SCALE_RMAC_UTIL	vxge_mBIT(11)
/*0x01668*/	u64	rxmac_authorize_all_addr;
#define VXGE_HW_RXMAC_AUTHORIZE_ALL_ADDR_VP(n)	vxge_mBIT(n)
/*0x01670*/	u64	rxmac_authorize_all_vid;
#define VXGE_HW_RXMAC_AUTHORIZE_ALL_VID_VP(n)	vxge_mBIT(n)
	u8	unused016c0[0x016c0-0x01678];

/*0x016c0*/	u64	rxmac_red_rate_repl_queue;
#define VXGE_HW_RXMAC_RED_RATE_REPL_QUEUE_CRATE_THR0(val) vxge_vBIT(val, 0, 4)
#define VXGE_HW_RXMAC_RED_RATE_REPL_QUEUE_CRATE_THR1(val) vxge_vBIT(val, 4, 4)
#define VXGE_HW_RXMAC_RED_RATE_REPL_QUEUE_CRATE_THR2(val) vxge_vBIT(val, 8, 4)
#define VXGE_HW_RXMAC_RED_RATE_REPL_QUEUE_CRATE_THR3(val) vxge_vBIT(val, 12, 4)
#define VXGE_HW_RXMAC_RED_RATE_REPL_QUEUE_FRATE_THR0(val) vxge_vBIT(val, 16, 4)
#define VXGE_HW_RXMAC_RED_RATE_REPL_QUEUE_FRATE_THR1(val) vxge_vBIT(val, 20, 4)
#define VXGE_HW_RXMAC_RED_RATE_REPL_QUEUE_FRATE_THR2(val) vxge_vBIT(val, 24, 4)
#define VXGE_HW_RXMAC_RED_RATE_REPL_QUEUE_FRATE_THR3(val) vxge_vBIT(val, 28, 4)
#define	VXGE_HW_RXMAC_RED_RATE_REPL_QUEUE_TRICKLE_EN	vxge_mBIT(35)
	u8	unused016e0[0x016e0-0x016c8];

/*0x016e0*/	u64	rxmac_cfg0_port[3];
#define	VXGE_HW_RXMAC_CFG0_PORT_RMAC_EN	vxge_mBIT(3)
#define	VXGE_HW_RXMAC_CFG0_PORT_STRIP_FCS	vxge_mBIT(7)
#define	VXGE_HW_RXMAC_CFG0_PORT_DISCARD_PFRM	vxge_mBIT(11)
#define	VXGE_HW_RXMAC_CFG0_PORT_IGNORE_FCS_ERR	vxge_mBIT(15)
#define	VXGE_HW_RXMAC_CFG0_PORT_IGNORE_LONG_ERR	vxge_mBIT(19)
#define	VXGE_HW_RXMAC_CFG0_PORT_IGNORE_USIZED_ERR	vxge_mBIT(23)
#define	VXGE_HW_RXMAC_CFG0_PORT_IGNORE_LEN_MISMATCH	vxge_mBIT(27)
#define VXGE_HW_RXMAC_CFG0_PORT_MAX_PYLD_LEN(val) vxge_vBIT(val, 50, 14)
	u8	unused01710[0x01710-0x016f8];

/*0x01710*/	u64	rxmac_cfg2_port[3];
#define	VXGE_HW_RXMAC_CFG2_PORT_PROM_EN	vxge_mBIT(3)
/*0x01728*/	u64	rxmac_pause_cfg_port[3];
#define	VXGE_HW_RXMAC_PAUSE_CFG_PORT_GEN_EN	vxge_mBIT(3)
#define	VXGE_HW_RXMAC_PAUSE_CFG_PORT_RCV_EN	vxge_mBIT(7)
#define VXGE_HW_RXMAC_PAUSE_CFG_PORT_ACCEL_SEND(val) vxge_vBIT(val, 9, 3)
#define	VXGE_HW_RXMAC_PAUSE_CFG_PORT_DUAL_THR	vxge_mBIT(15)
#define VXGE_HW_RXMAC_PAUSE_CFG_PORT_HIGH_PTIME(val) vxge_vBIT(val, 20, 16)
#define	VXGE_HW_RXMAC_PAUSE_CFG_PORT_IGNORE_PF_FCS_ERR	vxge_mBIT(39)
#define	VXGE_HW_RXMAC_PAUSE_CFG_PORT_IGNORE_PF_LEN_ERR	vxge_mBIT(43)
#define	VXGE_HW_RXMAC_PAUSE_CFG_PORT_LIMITER_EN	vxge_mBIT(47)
#define VXGE_HW_RXMAC_PAUSE_CFG_PORT_MAX_LIMIT(val) vxge_vBIT(val, 48, 8)
#define	VXGE_HW_RXMAC_PAUSE_CFG_PORT_PERMIT_RATEMGMT_CTRL	vxge_mBIT(59)
	u8	unused01758[0x01758-0x01740];

/*0x01758*/	u64	rxmac_red_cfg0_port[3];
#define VXGE_HW_RXMAC_RED_CFG0_PORT_RED_EN_VP(n)	vxge_mBIT(n)
/*0x01770*/	u64	rxmac_red_cfg1_port[3];
#define	VXGE_HW_RXMAC_RED_CFG1_PORT_FINE_EN	vxge_mBIT(3)
#define	VXGE_HW_RXMAC_RED_CFG1_PORT_RED_EN_REPL_QUEUE	vxge_mBIT(11)
/*0x01788*/	u64	rxmac_red_cfg2_port[3];
#define VXGE_HW_RXMAC_RED_CFG2_PORT_TRICKLE_EN_VP(n)	vxge_mBIT(n)
/*0x017a0*/	u64	rxmac_link_util_port[3];
#define	VXGE_HW_RXMAC_LINK_UTIL_PORT_RMAC_RMAC_UTILIZATION(val) \
							vxge_vBIT(val, 1, 7)
#define VXGE_HW_RXMAC_LINK_UTIL_PORT_RMAC_UTIL_CFG(val) vxge_vBIT(val, 8, 4)
#define VXGE_HW_RXMAC_LINK_UTIL_PORT_RMAC_RMAC_FRAC_UTIL(val) \
							vxge_vBIT(val, 12, 4)
#define VXGE_HW_RXMAC_LINK_UTIL_PORT_RMAC_PKT_WEIGHT(val) vxge_vBIT(val, 16, 4)
#define	VXGE_HW_RXMAC_LINK_UTIL_PORT_RMAC_RMAC_SCALE_FACTOR	vxge_mBIT(23)
	u8	unused017d0[0x017d0-0x017b8];

/*0x017d0*/	u64	rxmac_status_port[3];
#define	VXGE_HW_RXMAC_STATUS_PORT_RMAC_RX_FRM_RCVD	vxge_mBIT(3)
	u8	unused01800[0x01800-0x017e8];

/*0x01800*/	u64	rxmac_rx_pa_cfg0;
#define	VXGE_HW_RXMAC_RX_PA_CFG0_IGNORE_FRAME_ERR	vxge_mBIT(3)
#define	VXGE_HW_RXMAC_RX_PA_CFG0_SUPPORT_SNAP_AB_N	vxge_mBIT(7)
#define	VXGE_HW_RXMAC_RX_PA_CFG0_SEARCH_FOR_HAO	vxge_mBIT(18)
#define	VXGE_HW_RXMAC_RX_PA_CFG0_SUPPORT_MOBILE_IPV6_HDRS	vxge_mBIT(19)
#define	VXGE_HW_RXMAC_RX_PA_CFG0_IPV6_STOP_SEARCHING	vxge_mBIT(23)
#define	VXGE_HW_RXMAC_RX_PA_CFG0_NO_PS_IF_UNKNOWN	vxge_mBIT(27)
#define	VXGE_HW_RXMAC_RX_PA_CFG0_SEARCH_FOR_ETYPE	vxge_mBIT(35)
#define	VXGE_HW_RXMAC_RX_PA_CFG0_TOSS_ANY_FRM_IF_L3_CSUM_ERR	vxge_mBIT(39)
#define	VXGE_HW_RXMAC_RX_PA_CFG0_TOSS_OFFLD_FRM_IF_L3_CSUM_ERR	vxge_mBIT(43)
#define	VXGE_HW_RXMAC_RX_PA_CFG0_TOSS_ANY_FRM_IF_L4_CSUM_ERR	vxge_mBIT(47)
#define	VXGE_HW_RXMAC_RX_PA_CFG0_TOSS_OFFLD_FRM_IF_L4_CSUM_ERR	vxge_mBIT(51)
#define	VXGE_HW_RXMAC_RX_PA_CFG0_TOSS_ANY_FRM_IF_RPA_ERR	vxge_mBIT(55)
#define	VXGE_HW_RXMAC_RX_PA_CFG0_TOSS_OFFLD_FRM_IF_RPA_ERR	vxge_mBIT(59)
#define	VXGE_HW_RXMAC_RX_PA_CFG0_JUMBO_SNAP_EN	vxge_mBIT(63)
/*0x01808*/	u64	rxmac_rx_pa_cfg1;
#define	VXGE_HW_RXMAC_RX_PA_CFG1_REPL_IPV4_TCP_INCL_PH	vxge_mBIT(3)
#define	VXGE_HW_RXMAC_RX_PA_CFG1_REPL_IPV6_TCP_INCL_PH	vxge_mBIT(7)
#define	VXGE_HW_RXMAC_RX_PA_CFG1_REPL_IPV4_UDP_INCL_PH	vxge_mBIT(11)
#define	VXGE_HW_RXMAC_RX_PA_CFG1_REPL_IPV6_UDP_INCL_PH	vxge_mBIT(15)
#define	VXGE_HW_RXMAC_RX_PA_CFG1_REPL_L4_INCL_CF	vxge_mBIT(19)
#define	VXGE_HW_RXMAC_RX_PA_CFG1_REPL_STRIP_VLAN_TAG	vxge_mBIT(23)
	u8	unused01828[0x01828-0x01810];

/*0x01828*/	u64	rts_mgr_cfg0;
#define	VXGE_HW_RTS_MGR_CFG0_RTS_DP_SP_PRIORITY	vxge_mBIT(3)
#define VXGE_HW_RTS_MGR_CFG0_FLEX_L4PRTCL_VALUE(val) vxge_vBIT(val, 24, 8)
#define	VXGE_HW_RTS_MGR_CFG0_ICMP_TRASH	vxge_mBIT(35)
#define	VXGE_HW_RTS_MGR_CFG0_TCPSYN_TRASH	vxge_mBIT(39)
#define	VXGE_HW_RTS_MGR_CFG0_ZL4PYLD_TRASH	vxge_mBIT(43)
#define	VXGE_HW_RTS_MGR_CFG0_L4PRTCL_TCP_TRASH	vxge_mBIT(47)
#define	VXGE_HW_RTS_MGR_CFG0_L4PRTCL_UDP_TRASH	vxge_mBIT(51)
#define	VXGE_HW_RTS_MGR_CFG0_L4PRTCL_FLEX_TRASH	vxge_mBIT(55)
#define	VXGE_HW_RTS_MGR_CFG0_IPFRAG_TRASH	vxge_mBIT(59)
/*0x01830*/	u64	rts_mgr_cfg1;
#define	VXGE_HW_RTS_MGR_CFG1_DA_ACTIVE_TABLE	vxge_mBIT(3)
#define	VXGE_HW_RTS_MGR_CFG1_PN_ACTIVE_TABLE	vxge_mBIT(7)
/*0x01838*/	u64	rts_mgr_criteria_priority;
#define VXGE_HW_RTS_MGR_CRITERIA_PRIORITY_ETYPE(val) vxge_vBIT(val, 5, 3)
#define VXGE_HW_RTS_MGR_CRITERIA_PRIORITY_ICMP_TCPSYN(val) vxge_vBIT(val, 9, 3)
#define VXGE_HW_RTS_MGR_CRITERIA_PRIORITY_L4PN(val) vxge_vBIT(val, 13, 3)
#define VXGE_HW_RTS_MGR_CRITERIA_PRIORITY_RANGE_L4PN(val) vxge_vBIT(val, 17, 3)
#define VXGE_HW_RTS_MGR_CRITERIA_PRIORITY_RTH_IT(val) vxge_vBIT(val, 21, 3)
#define VXGE_HW_RTS_MGR_CRITERIA_PRIORITY_DS(val) vxge_vBIT(val, 25, 3)
#define VXGE_HW_RTS_MGR_CRITERIA_PRIORITY_QOS(val) vxge_vBIT(val, 29, 3)
#define VXGE_HW_RTS_MGR_CRITERIA_PRIORITY_ZL4PYLD(val) vxge_vBIT(val, 33, 3)
#define VXGE_HW_RTS_MGR_CRITERIA_PRIORITY_L4PRTCL(val) vxge_vBIT(val, 37, 3)
/*0x01840*/	u64	rts_mgr_da_pause_cfg;
#define VXGE_HW_RTS_MGR_DA_PAUSE_CFG_VPATH_VECTOR(val) vxge_vBIT(val, 0, 17)
/*0x01848*/	u64	rts_mgr_da_slow_proto_cfg;
#define VXGE_HW_RTS_MGR_DA_SLOW_PROTO_CFG_VPATH_VECTOR(val) \
							vxge_vBIT(val, 0, 17)
	u8	unused01890[0x01890-0x01850];
/*0x01890*/     u64     rts_mgr_cbasin_cfg;
	u8	unused01968[0x01968-0x01898];

/*0x01968*/	u64	dbg_stat_rx_any_frms;
#define VXGE_HW_DBG_STAT_RX_ANY_FRMS_PORT0_RX_ANY_FRMS(val) vxge_vBIT(val, 0, 8)
#define VXGE_HW_DBG_STAT_RX_ANY_FRMS_PORT1_RX_ANY_FRMS(val) vxge_vBIT(val, 8, 8)
#define VXGE_HW_DBG_STAT_RX_ANY_FRMS_PORT2_RX_ANY_FRMS(val) \
							vxge_vBIT(val, 16, 8)
	u8	unused01a00[0x01a00-0x01970];

/*0x01a00*/	u64	rxmac_red_rate_vp[17];
#define VXGE_HW_RXMAC_RED_RATE_VP_CRATE_THR0(val) vxge_vBIT(val, 0, 4)
#define VXGE_HW_RXMAC_RED_RATE_VP_CRATE_THR1(val) vxge_vBIT(val, 4, 4)
#define VXGE_HW_RXMAC_RED_RATE_VP_CRATE_THR2(val) vxge_vBIT(val, 8, 4)
#define VXGE_HW_RXMAC_RED_RATE_VP_CRATE_THR3(val) vxge_vBIT(val, 12, 4)
#define VXGE_HW_RXMAC_RED_RATE_VP_FRATE_THR0(val) vxge_vBIT(val, 16, 4)
#define VXGE_HW_RXMAC_RED_RATE_VP_FRATE_THR1(val) vxge_vBIT(val, 20, 4)
#define VXGE_HW_RXMAC_RED_RATE_VP_FRATE_THR2(val) vxge_vBIT(val, 24, 4)
#define VXGE_HW_RXMAC_RED_RATE_VP_FRATE_THR3(val) vxge_vBIT(val, 28, 4)
	u8	unused01e00[0x01e00-0x01a88];

/*0x01e00*/	u64	xgmac_int_status;
#define	VXGE_HW_XGMAC_INT_STATUS_XMAC_GEN_ERR_XMAC_GEN_INT	vxge_mBIT(3)
#define	VXGE_HW_XGMAC_INT_STATUS_XMAC_LINK_ERR_PORT0_XMAC_LINK_INT_PORT0 \
								vxge_mBIT(7)
#define	VXGE_HW_XGMAC_INT_STATUS_XMAC_LINK_ERR_PORT1_XMAC_LINK_INT_PORT1 \
								vxge_mBIT(11)
#define	VXGE_HW_XGMAC_INT_STATUS_XGXS_GEN_ERR_XGXS_GEN_INT	vxge_mBIT(15)
#define	VXGE_HW_XGMAC_INT_STATUS_ASIC_NTWK_ERR_ASIC_NTWK_INT	vxge_mBIT(19)
#define	VXGE_HW_XGMAC_INT_STATUS_ASIC_GPIO_ERR_ASIC_GPIO_INT	vxge_mBIT(23)
/*0x01e08*/	u64	xgmac_int_mask;
/*0x01e10*/	u64	xmac_gen_err_reg;
#define	VXGE_HW_XMAC_GEN_ERR_REG_LAGC_LAG_PORT0_ACTOR_CHURN_DETECTED \
								vxge_mBIT(7)
#define	VXGE_HW_XMAC_GEN_ERR_REG_LAGC_LAG_PORT0_PARTNER_CHURN_DETECTED \
								vxge_mBIT(11)
#define	VXGE_HW_XMAC_GEN_ERR_REG_LAGC_LAG_PORT0_RECEIVED_LACPDU	vxge_mBIT(15)
#define	VXGE_HW_XMAC_GEN_ERR_REG_LAGC_LAG_PORT1_ACTOR_CHURN_DETECTED \
								vxge_mBIT(19)
#define	VXGE_HW_XMAC_GEN_ERR_REG_LAGC_LAG_PORT1_PARTNER_CHURN_DETECTED \
								vxge_mBIT(23)
#define	VXGE_HW_XMAC_GEN_ERR_REG_LAGC_LAG_PORT1_RECEIVED_LACPDU	vxge_mBIT(27)
#define	VXGE_HW_XMAC_GEN_ERR_REG_XLCM_LAG_FAILOVER_DETECTED	vxge_mBIT(31)
#define	VXGE_HW_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE0_SG_ERR(val) \
							vxge_vBIT(val, 40, 2)
#define	VXGE_HW_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE0_DB_ERR(val) \
							vxge_vBIT(val, 42, 2)
#define	VXGE_HW_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE1_SG_ERR(val) \
							vxge_vBIT(val, 44, 2)
#define	VXGE_HW_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE1_DB_ERR(val) \
							vxge_vBIT(val, 46, 2)
#define	VXGE_HW_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE2_SG_ERR(val) \
							vxge_vBIT(val, 48, 2)
#define	VXGE_HW_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE2_DB_ERR(val) \
							vxge_vBIT(val, 50, 2)
#define	VXGE_HW_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE3_SG_ERR(val) \
							vxge_vBIT(val, 52, 2)
#define	VXGE_HW_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE3_DB_ERR(val) \
							vxge_vBIT(val, 54, 2)
#define	VXGE_HW_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE4_SG_ERR(val) \
							vxge_vBIT(val, 56, 2)
#define	VXGE_HW_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE4_DB_ERR(val) \
							vxge_vBIT(val, 58, 2)
#define	VXGE_HW_XMAC_GEN_ERR_REG_XMACJ_XMAC_FSM_ERR	vxge_mBIT(63)
/*0x01e18*/	u64	xmac_gen_err_mask;
/*0x01e20*/	u64	xmac_gen_err_alarm;
/*0x01e28*/	u64	xmac_link_err_port0_reg;
#define	VXGE_HW_XMAC_LINK_ERR_PORT_REG_XMACJ_PORT_DOWN	vxge_mBIT(3)
#define	VXGE_HW_XMAC_LINK_ERR_PORT_REG_XMACJ_PORT_UP	vxge_mBIT(7)
#define	VXGE_HW_XMAC_LINK_ERR_PORT_REG_XMACJ_PORT_WENT_DOWN	vxge_mBIT(11)
#define	VXGE_HW_XMAC_LINK_ERR_PORT_REG_XMACJ_PORT_WENT_UP	vxge_mBIT(15)
#define	VXGE_HW_XMAC_LINK_ERR_PORT_REG_XMACJ_PORT_REAFFIRMED_FAULT \
								vxge_mBIT(19)
#define	VXGE_HW_XMAC_LINK_ERR_PORT_REG_XMACJ_PORT_REAFFIRMED_OK	vxge_mBIT(23)
#define	VXGE_HW_XMAC_LINK_ERR_PORT_REG_XMACJ_LINK_DOWN	vxge_mBIT(27)
#define	VXGE_HW_XMAC_LINK_ERR_PORT_REG_XMACJ_LINK_UP	vxge_mBIT(31)
#define	VXGE_HW_XMAC_LINK_ERR_PORT_REG_RATEMGMT_RATE_CHANGE	vxge_mBIT(35)
#define	VXGE_HW_XMAC_LINK_ERR_PORT_REG_RATEMGMT_LASI_INV	vxge_mBIT(39)
#define	VXGE_HW_XMAC_LINK_ERR_PORT_REG_XMDIO_MDIO_MGR_ACCESS_COMPLETE \
								vxge_mBIT(47)
/*0x01e30*/	u64	xmac_link_err_port0_mask;
/*0x01e38*/	u64	xmac_link_err_port0_alarm;
/*0x01e40*/	u64	xmac_link_err_port1_reg;
/*0x01e48*/	u64	xmac_link_err_port1_mask;
/*0x01e50*/	u64	xmac_link_err_port1_alarm;
/*0x01e58*/	u64	xgxs_gen_err_reg;
#define	VXGE_HW_XGXS_GEN_ERR_REG_XGXS_XGXS_FSM_ERR	vxge_mBIT(63)
/*0x01e60*/	u64	xgxs_gen_err_mask;
/*0x01e68*/	u64	xgxs_gen_err_alarm;
/*0x01e70*/	u64	asic_ntwk_err_reg;
#define	VXGE_HW_ASIC_NTWK_ERR_REG_XMACJ_NTWK_DOWN	vxge_mBIT(3)
#define	VXGE_HW_ASIC_NTWK_ERR_REG_XMACJ_NTWK_UP	vxge_mBIT(7)
#define	VXGE_HW_ASIC_NTWK_ERR_REG_XMACJ_NTWK_WENT_DOWN	vxge_mBIT(11)
#define	VXGE_HW_ASIC_NTWK_ERR_REG_XMACJ_NTWK_WENT_UP	vxge_mBIT(15)
#define	VXGE_HW_ASIC_NTWK_ERR_REG_XMACJ_NTWK_REAFFIRMED_FAULT	vxge_mBIT(19)
#define	VXGE_HW_ASIC_NTWK_ERR_REG_XMACJ_NTWK_REAFFIRMED_OK	vxge_mBIT(23)
/*0x01e78*/	u64	asic_ntwk_err_mask;
/*0x01e80*/	u64	asic_ntwk_err_alarm;
/*0x01e88*/	u64	asic_gpio_err_reg;
#define VXGE_HW_ASIC_GPIO_ERR_REG_XMACJ_GPIO_INT(n)	vxge_mBIT(n)
/*0x01e90*/	u64	asic_gpio_err_mask;
/*0x01e98*/	u64	asic_gpio_err_alarm;
/*0x01ea0*/	u64	xgmac_gen_status;
#define	VXGE_HW_XGMAC_GEN_STATUS_XMACJ_NTWK_OK	vxge_mBIT(3)
#define	VXGE_HW_XGMAC_GEN_STATUS_XMACJ_NTWK_DATA_RATE	vxge_mBIT(11)
/*0x01ea8*/	u64	xgmac_gen_fw_memo_status;
#define	VXGE_HW_XGMAC_GEN_FW_MEMO_STATUS_XMACJ_EVENTS_PENDING(val) \
							vxge_vBIT(val, 0, 17)
/*0x01eb0*/	u64	xgmac_gen_fw_memo_mask;
#define VXGE_HW_XGMAC_GEN_FW_MEMO_MASK_MASK(val) vxge_vBIT(val, 0, 64)
/*0x01eb8*/	u64	xgmac_gen_fw_vpath_to_vsport_status;
#define	VXGE_HW_XGMAC_GEN_FW_VPATH_TO_VSPORT_STATUS_XMACJ_EVENTS_PENDING(val) \
						vxge_vBIT(val, 0, 17)
/*0x01ec0*/	u64	xgmac_main_cfg_port[2];
#define	VXGE_HW_XGMAC_MAIN_CFG_PORT_PORT_EN	vxge_mBIT(3)
	u8	unused01f40[0x01f40-0x01ed0];

/*0x01f40*/	u64	xmac_gen_cfg;
#define VXGE_HW_XMAC_GEN_CFG_RATEMGMT_MAC_RATE_SEL(val) vxge_vBIT(val, 2, 2)
#define	VXGE_HW_XMAC_GEN_CFG_TX_HEAD_DROP_WHEN_FAULT	vxge_mBIT(7)
#define	VXGE_HW_XMAC_GEN_CFG_FAULT_BEHAVIOUR	vxge_mBIT(27)
#define VXGE_HW_XMAC_GEN_CFG_PERIOD_NTWK_UP(val) vxge_vBIT(val, 28, 4)
#define VXGE_HW_XMAC_GEN_CFG_PERIOD_NTWK_DOWN(val) vxge_vBIT(val, 32, 4)
/*0x01f48*/	u64	xmac_timestamp;
#define	VXGE_HW_XMAC_TIMESTAMP_EN	vxge_mBIT(3)
#define VXGE_HW_XMAC_TIMESTAMP_USE_LINK_ID(val) vxge_vBIT(val, 6, 2)
#define VXGE_HW_XMAC_TIMESTAMP_INTERVAL(val) vxge_vBIT(val, 12, 4)
#define	VXGE_HW_XMAC_TIMESTAMP_TIMER_RESTART	vxge_mBIT(19)
#define VXGE_HW_XMAC_TIMESTAMP_XMACJ_ROLLOVER_CNT(val) vxge_vBIT(val, 32, 16)
/*0x01f50*/	u64	xmac_stats_gen_cfg;
#define VXGE_HW_XMAC_STATS_GEN_CFG_PRTAGGR_CUM_TIMER(val) vxge_vBIT(val, 4, 4)
#define VXGE_HW_XMAC_STATS_GEN_CFG_VPATH_CUM_TIMER(val) vxge_vBIT(val, 8, 4)
#define	VXGE_HW_XMAC_STATS_GEN_CFG_VLAN_HANDLING	vxge_mBIT(15)
/*0x01f58*/	u64	xmac_stats_sys_cmd;
#define VXGE_HW_XMAC_STATS_SYS_CMD_OP(val) vxge_vBIT(val, 5, 3)
#define	VXGE_HW_XMAC_STATS_SYS_CMD_STROBE	vxge_mBIT(15)
#define VXGE_HW_XMAC_STATS_SYS_CMD_LOC_SEL(val) vxge_vBIT(val, 27, 5)
#define VXGE_HW_XMAC_STATS_SYS_CMD_OFFSET_SEL(val) vxge_vBIT(val, 32, 8)
/*0x01f60*/	u64	xmac_stats_sys_data;
#define VXGE_HW_XMAC_STATS_SYS_DATA_XSMGR_DATA(val) vxge_vBIT(val, 0, 64)
	u8	unused01f80[0x01f80-0x01f68];

/*0x01f80*/	u64	asic_ntwk_ctrl;
#define	VXGE_HW_ASIC_NTWK_CTRL_REQ_TEST_NTWK	vxge_mBIT(3)
#define	VXGE_HW_ASIC_NTWK_CTRL_PORT0_REQ_TEST_PORT	vxge_mBIT(11)
#define	VXGE_HW_ASIC_NTWK_CTRL_PORT1_REQ_TEST_PORT	vxge_mBIT(15)
/*0x01f88*/	u64	asic_ntwk_cfg_show_port_info;
#define VXGE_HW_ASIC_NTWK_CFG_SHOW_PORT_INFO_VP(n)	vxge_mBIT(n)
/*0x01f90*/	u64	asic_ntwk_cfg_port_num;
#define VXGE_HW_ASIC_NTWK_CFG_PORT_NUM_VP(n)	vxge_mBIT(n)
/*0x01f98*/	u64	xmac_cfg_port[3];
#define	VXGE_HW_XMAC_CFG_PORT_XGMII_LOOPBACK	vxge_mBIT(3)
#define	VXGE_HW_XMAC_CFG_PORT_XGMII_REVERSE_LOOPBACK	vxge_mBIT(7)
#define	VXGE_HW_XMAC_CFG_PORT_XGMII_TX_BEHAV	vxge_mBIT(11)
#define	VXGE_HW_XMAC_CFG_PORT_XGMII_RX_BEHAV	vxge_mBIT(15)
/*0x01fb0*/	u64	xmac_station_addr_port[2];
#define VXGE_HW_XMAC_STATION_ADDR_PORT_MAC_ADDR(val) vxge_vBIT(val, 0, 48)
	u8	unused02020[0x02020-0x01fc0];

/*0x02020*/	u64	lag_cfg;
#define	VXGE_HW_LAG_CFG_EN	vxge_mBIT(3)
#define VXGE_HW_LAG_CFG_MODE(val) vxge_vBIT(val, 6, 2)
#define	VXGE_HW_LAG_CFG_TX_DISCARD_BEHAV	vxge_mBIT(11)
#define	VXGE_HW_LAG_CFG_RX_DISCARD_BEHAV	vxge_mBIT(15)
#define	VXGE_HW_LAG_CFG_PREF_INDIV_PORT_NUM	vxge_mBIT(19)
/*0x02028*/	u64	lag_status;
#define	VXGE_HW_LAG_STATUS_XLCM_WAITING_TO_FAILBACK	vxge_mBIT(3)
#define VXGE_HW_LAG_STATUS_XLCM_TIMER_VAL_COLD_FAILOVER(val) \
							vxge_vBIT(val, 8, 8)
/*0x02030*/	u64	lag_active_passive_cfg;
#define	VXGE_HW_LAG_ACTIVE_PASSIVE_CFG_HOT_STANDBY	vxge_mBIT(3)
#define	VXGE_HW_LAG_ACTIVE_PASSIVE_CFG_LACP_DECIDES	vxge_mBIT(7)
#define	VXGE_HW_LAG_ACTIVE_PASSIVE_CFG_PREF_ACTIVE_PORT_NUM	vxge_mBIT(11)
#define	VXGE_HW_LAG_ACTIVE_PASSIVE_CFG_AUTO_FAILBACK	vxge_mBIT(15)
#define	VXGE_HW_LAG_ACTIVE_PASSIVE_CFG_FAILBACK_EN	vxge_mBIT(19)
#define	VXGE_HW_LAG_ACTIVE_PASSIVE_CFG_COLD_FAILOVER_TIMEOUT(val) \
							vxge_vBIT(val, 32, 16)
	u8	unused02040[0x02040-0x02038];

/*0x02040*/	u64	lag_lacp_cfg;
#define	VXGE_HW_LAG_LACP_CFG_EN	vxge_mBIT(3)
#define	VXGE_HW_LAG_LACP_CFG_LACP_BEGIN	vxge_mBIT(7)
#define	VXGE_HW_LAG_LACP_CFG_DISCARD_LACP	vxge_mBIT(11)
#define	VXGE_HW_LAG_LACP_CFG_LIBERAL_LEN_CHK	vxge_mBIT(15)
/*0x02048*/	u64	lag_timer_cfg_1;
#define VXGE_HW_LAG_TIMER_CFG_1_FAST_PER(val) vxge_vBIT(val, 0, 16)
#define VXGE_HW_LAG_TIMER_CFG_1_SLOW_PER(val) vxge_vBIT(val, 16, 16)
#define VXGE_HW_LAG_TIMER_CFG_1_SHORT_TIMEOUT(val) vxge_vBIT(val, 32, 16)
#define VXGE_HW_LAG_TIMER_CFG_1_LONG_TIMEOUT(val) vxge_vBIT(val, 48, 16)
/*0x02050*/	u64	lag_timer_cfg_2;
#define VXGE_HW_LAG_TIMER_CFG_2_CHURN_DET(val) vxge_vBIT(val, 0, 16)
#define VXGE_HW_LAG_TIMER_CFG_2_AGGR_WAIT(val) vxge_vBIT(val, 16, 16)
#define VXGE_HW_LAG_TIMER_CFG_2_SHORT_TIMER_SCALE(val) vxge_vBIT(val, 32, 16)
#define VXGE_HW_LAG_TIMER_CFG_2_LONG_TIMER_SCALE(val)  vxge_vBIT(val, 48, 16)
/*0x02058*/	u64	lag_sys_id;
#define VXGE_HW_LAG_SYS_ID_ADDR(val) vxge_vBIT(val, 0, 48)
#define	VXGE_HW_LAG_SYS_ID_USE_PORT_ADDR	vxge_mBIT(51)
#define	VXGE_HW_LAG_SYS_ID_ADDR_SEL	vxge_mBIT(55)
/*0x02060*/	u64	lag_sys_cfg;
#define VXGE_HW_LAG_SYS_CFG_SYS_PRI(val) vxge_vBIT(val, 0, 16)
	u8	unused02070[0x02070-0x02068];

/*0x02070*/	u64	lag_aggr_addr_cfg[2];
#define VXGE_HW_LAG_AGGR_ADDR_CFG_ADDR(val) vxge_vBIT(val, 0, 48)
#define	VXGE_HW_LAG_AGGR_ADDR_CFG_USE_PORT_ADDR	vxge_mBIT(51)
#define	VXGE_HW_LAG_AGGR_ADDR_CFG_ADDR_SEL	vxge_mBIT(55)
/*0x02080*/	u64	lag_aggr_id_cfg[2];
#define VXGE_HW_LAG_AGGR_ID_CFG_ID(val) vxge_vBIT(val, 0, 16)
/*0x02090*/	u64	lag_aggr_admin_key[2];
#define VXGE_HW_LAG_AGGR_ADMIN_KEY_KEY(val) vxge_vBIT(val, 0, 16)
/*0x020a0*/	u64	lag_aggr_alt_admin_key;
#define VXGE_HW_LAG_AGGR_ALT_ADMIN_KEY_KEY(val) vxge_vBIT(val, 0, 16)
#define	VXGE_HW_LAG_AGGR_ALT_ADMIN_KEY_ALT_AGGR	vxge_mBIT(19)
/*0x020a8*/	u64	lag_aggr_oper_key[2];
#define VXGE_HW_LAG_AGGR_OPER_KEY_LAGC_KEY(val) vxge_vBIT(val, 0, 16)
/*0x020b8*/	u64	lag_aggr_partner_sys_id[2];
#define VXGE_HW_LAG_AGGR_PARTNER_SYS_ID_LAGC_ADDR(val) vxge_vBIT(val, 0, 48)
/*0x020c8*/	u64	lag_aggr_partner_info[2];
#define VXGE_HW_LAG_AGGR_PARTNER_INFO_LAGC_SYS_PRI(val) vxge_vBIT(val, 0, 16)
#define	VXGE_HW_LAG_AGGR_PARTNER_INFO_LAGC_OPER_KEY(val) \
						vxge_vBIT(val, 16, 16)
/*0x020d8*/	u64	lag_aggr_state[2];
#define	VXGE_HW_LAG_AGGR_STATE_LAGC_TX	vxge_mBIT(3)
#define	VXGE_HW_LAG_AGGR_STATE_LAGC_RX	vxge_mBIT(7)
#define	VXGE_HW_LAG_AGGR_STATE_LAGC_READY	vxge_mBIT(11)
#define	VXGE_HW_LAG_AGGR_STATE_LAGC_INDIVIDUAL	vxge_mBIT(15)
	u8	unused020f0[0x020f0-0x020e8];

/*0x020f0*/	u64	lag_port_cfg[2];
#define	VXGE_HW_LAG_PORT_CFG_EN	vxge_mBIT(3)
#define	VXGE_HW_LAG_PORT_CFG_DISCARD_SLOW_PROTO	vxge_mBIT(7)
#define	VXGE_HW_LAG_PORT_CFG_HOST_CHOSEN_AGGR	vxge_mBIT(11)
#define	VXGE_HW_LAG_PORT_CFG_DISCARD_UNKNOWN_SLOW_PROTO	vxge_mBIT(15)
/*0x02100*/	u64	lag_port_actor_admin_cfg[2];
#define VXGE_HW_LAG_PORT_ACTOR_ADMIN_CFG_PORT_NUM(val) vxge_vBIT(val, 0, 16)
#define VXGE_HW_LAG_PORT_ACTOR_ADMIN_CFG_PORT_PRI(val) vxge_vBIT(val, 16, 16)
#define VXGE_HW_LAG_PORT_ACTOR_ADMIN_CFG_KEY_10G(val) vxge_vBIT(val, 32, 16)
#define VXGE_HW_LAG_PORT_ACTOR_ADMIN_CFG_KEY_1G(val) vxge_vBIT(val, 48, 16)
/*0x02110*/	u64	lag_port_actor_admin_state[2];
#define	VXGE_HW_LAG_PORT_ACTOR_ADMIN_STATE_LACP_ACTIVITY	vxge_mBIT(3)
#define	VXGE_HW_LAG_PORT_ACTOR_ADMIN_STATE_LACP_TIMEOUT	vxge_mBIT(7)
#define	VXGE_HW_LAG_PORT_ACTOR_ADMIN_STATE_AGGREGATION	vxge_mBIT(11)
#define	VXGE_HW_LAG_PORT_ACTOR_ADMIN_STATE_SYNCHRONIZATION	vxge_mBIT(15)
#define	VXGE_HW_LAG_PORT_ACTOR_ADMIN_STATE_COLLECTING	vxge_mBIT(19)
#define	VXGE_HW_LAG_PORT_ACTOR_ADMIN_STATE_DISTRIBUTING	vxge_mBIT(23)
#define	VXGE_HW_LAG_PORT_ACTOR_ADMIN_STATE_DEFAULTED	vxge_mBIT(27)
#define	VXGE_HW_LAG_PORT_ACTOR_ADMIN_STATE_EXPIRED	vxge_mBIT(31)
/*0x02120*/	u64	lag_port_partner_admin_sys_id[2];
#define VXGE_HW_LAG_PORT_PARTNER_ADMIN_SYS_ID_ADDR(val) vxge_vBIT(val, 0, 48)
/*0x02130*/	u64	lag_port_partner_admin_cfg[2];
#define VXGE_HW_LAG_PORT_PARTNER_ADMIN_CFG_SYS_PRI(val) vxge_vBIT(val, 0, 16)
#define VXGE_HW_LAG_PORT_PARTNER_ADMIN_CFG_KEY(val) vxge_vBIT(val, 16, 16)
#define	VXGE_HW_LAG_PORT_PARTNER_ADMIN_CFG_PORT_NUM(val) \
							vxge_vBIT(val, 32, 16)
#define	VXGE_HW_LAG_PORT_PARTNER_ADMIN_CFG_PORT_PRI(val) \
							vxge_vBIT(val, 48, 16)
/*0x02140*/	u64	lag_port_partner_admin_state[2];
#define	VXGE_HW_LAG_PORT_PARTNER_ADMIN_STATE_LACP_ACTIVITY	vxge_mBIT(3)
#define	VXGE_HW_LAG_PORT_PARTNER_ADMIN_STATE_LACP_TIMEOUT	vxge_mBIT(7)
#define	VXGE_HW_LAG_PORT_PARTNER_ADMIN_STATE_AGGREGATION	vxge_mBIT(11)
#define	VXGE_HW_LAG_PORT_PARTNER_ADMIN_STATE_SYNCHRONIZATION	vxge_mBIT(15)
#define	VXGE_HW_LAG_PORT_PARTNER_ADMIN_STATE_COLLECTING	vxge_mBIT(19)
#define	VXGE_HW_LAG_PORT_PARTNER_ADMIN_STATE_DISTRIBUTING	vxge_mBIT(23)
#define	VXGE_HW_LAG_PORT_PARTNER_ADMIN_STATE_DEFAULTED	vxge_mBIT(27)
#define	VXGE_HW_LAG_PORT_PARTNER_ADMIN_STATE_EXPIRED	vxge_mBIT(31)
/*0x02150*/	u64	lag_port_to_aggr[2];
#define VXGE_HW_LAG_PORT_TO_AGGR_LAGC_AGGR_ID(val) vxge_vBIT(val, 0, 16)
#define	VXGE_HW_LAG_PORT_TO_AGGR_LAGC_AGGR_VLD_ID	vxge_mBIT(19)
/*0x02160*/	u64	lag_port_actor_oper_key[2];
#define VXGE_HW_LAG_PORT_ACTOR_OPER_KEY_LAGC_KEY(val) vxge_vBIT(val, 0, 16)
/*0x02170*/	u64	lag_port_actor_oper_state[2];
#define	VXGE_HW_LAG_PORT_ACTOR_OPER_STATE_LAGC_LACP_ACTIVITY	vxge_mBIT(3)
#define	VXGE_HW_LAG_PORT_ACTOR_OPER_STATE_LAGC_LACP_TIMEOUT	vxge_mBIT(7)
#define	VXGE_HW_LAG_PORT_ACTOR_OPER_STATE_LAGC_AGGREGATION	vxge_mBIT(11)
#define	VXGE_HW_LAG_PORT_ACTOR_OPER_STATE_LAGC_SYNCHRONIZATION	vxge_mBIT(15)
#define	VXGE_HW_LAG_PORT_ACTOR_OPER_STATE_LAGC_COLLECTING	vxge_mBIT(19)
#define	VXGE_HW_LAG_PORT_ACTOR_OPER_STATE_LAGC_DISTRIBUTING	vxge_mBIT(23)
#define	VXGE_HW_LAG_PORT_ACTOR_OPER_STATE_LAGC_DEFAULTED	vxge_mBIT(27)
#define	VXGE_HW_LAG_PORT_ACTOR_OPER_STATE_LAGC_EXPIRED	vxge_mBIT(31)
/*0x02180*/	u64	lag_port_partner_oper_sys_id[2];
#define VXGE_HW_LAG_PORT_PARTNER_OPER_SYS_ID_LAGC_ADDR(val) \
						vxge_vBIT(val, 0, 48)
/*0x02190*/	u64	lag_port_partner_oper_info[2];
#define VXGE_HW_LAG_PORT_PARTNER_OPER_INFO_LAGC_SYS_PRI(val) \
						vxge_vBIT(val, 0, 16)
#define	VXGE_HW_LAG_PORT_PARTNER_OPER_INFO_LAGC_KEY(val) \
						vxge_vBIT(val, 16, 16)
#define	VXGE_HW_LAG_PORT_PARTNER_OPER_INFO_LAGC_PORT_NUM(val) \
						vxge_vBIT(val, 32, 16)
#define	VXGE_HW_LAG_PORT_PARTNER_OPER_INFO_LAGC_PORT_PRI(val) \
						vxge_vBIT(val, 48, 16)
/*0x021a0*/	u64	lag_port_partner_oper_state[2];
#define	VXGE_HW_LAG_PORT_PARTNER_OPER_STATE_LAGC_LACP_ACTIVITY	vxge_mBIT(3)
#define	VXGE_HW_LAG_PORT_PARTNER_OPER_STATE_LAGC_LACP_TIMEOUT	vxge_mBIT(7)
#define	VXGE_HW_LAG_PORT_PARTNER_OPER_STATE_LAGC_AGGREGATION	vxge_mBIT(11)
#define	VXGE_HW_LAG_PORT_PARTNER_OPER_STATE_LAGC_SYNCHRONIZATION \
								vxge_mBIT(15)
#define	VXGE_HW_LAG_PORT_PARTNER_OPER_STATE_LAGC_COLLECTING	vxge_mBIT(19)
#define	VXGE_HW_LAG_PORT_PARTNER_OPER_STATE_LAGC_DISTRIBUTING	vxge_mBIT(23)
#define	VXGE_HW_LAG_PORT_PARTNER_OPER_STATE_LAGC_DEFAULTED	vxge_mBIT(27)
#define	VXGE_HW_LAG_PORT_PARTNER_OPER_STATE_LAGC_EXPIRED	vxge_mBIT(31)
/*0x021b0*/	u64	lag_port_state_vars[2];
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_READY	vxge_mBIT(3)
#define VXGE_HW_LAG_PORT_STATE_VARS_LAGC_SELECTED(val) vxge_vBIT(val, 6, 2)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_AGGR_NUM	vxge_mBIT(11)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_PORT_MOVED	vxge_mBIT(15)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_PORT_ENABLED	vxge_mBIT(18)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_PORT_DISABLED	vxge_mBIT(19)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_NTT	vxge_mBIT(23)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_ACTOR_CHURN	vxge_mBIT(27)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_PARTNER_CHURN	vxge_mBIT(31)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_ACTOR_INFO_LEN_MISMATCH \
								vxge_mBIT(32)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_PARTNER_INFO_LEN_MISMATCH \
								vxge_mBIT(33)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_COLL_INFO_LEN_MISMATCH	vxge_mBIT(34)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_TERM_INFO_LEN_MISMATCH	vxge_mBIT(35)
#define VXGE_HW_LAG_PORT_STATE_VARS_LAGC_RX_FSM_STATE(val) vxge_vBIT(val, 37, 3)
#define VXGE_HW_LAG_PORT_STATE_VARS_LAGC_MUX_FSM_STATE(val) \
							vxge_vBIT(val, 41, 3)
#define VXGE_HW_LAG_PORT_STATE_VARS_LAGC_MUX_REASON(val) vxge_vBIT(val, 44, 4)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_ACTOR_CHURN_STATE	vxge_mBIT(54)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_PARTNER_CHURN_STATE	vxge_mBIT(55)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_ACTOR_CHURN_COUNT(val) \
							vxge_vBIT(val, 56, 4)
#define	VXGE_HW_LAG_PORT_STATE_VARS_LAGC_PARTNER_CHURN_COUNT(val) \
							vxge_vBIT(val, 60, 4)
/*0x021c0*/	u64	lag_port_timer_cntr[2];
#define VXGE_HW_LAG_PORT_TIMER_CNTR_LAGC_CURRENT_WHILE(val) vxge_vBIT(val, 0, 8)
#define VXGE_HW_LAG_PORT_TIMER_CNTR_LAGC_PERIODIC_WHILE(val) \
							vxge_vBIT(val, 8, 8)
#define VXGE_HW_LAG_PORT_TIMER_CNTR_LAGC_WAIT_WHILE(val) vxge_vBIT(val, 16, 8)
#define VXGE_HW_LAG_PORT_TIMER_CNTR_LAGC_TX_LACP(val) vxge_vBIT(val, 24, 8)
#define	VXGE_HW_LAG_PORT_TIMER_CNTR_LAGC_ACTOR_SYNC_TRANSITION_COUNT(val) \
							vxge_vBIT(val, 32, 8)
#define	VXGE_HW_LAG_PORT_TIMER_CNTR_LAGC_PARTNER_SYNC_TRANSITION_COUNT(val) \
	******vxge_vBI*****, 40, 8)
#define	VXGE_HW****/*********************ACTOR_CHANGE**********************************
 *8This software may be used and distributed acco********to the terms of
 * the GNU General Public 56This 	u8	unused02208[0x02700-t
 *1d0];

/*t
 * re*/	u64	rtdma_int_status; software may be RTDMA_INT_STATUS_PDA_ALARMlete INT******m****1s software may be is not
 * a compleCC_ERRORystemd may only be 2sed when the entire operating
 * syLSOm is lidistd may only be 4sed when the entire operating
 * sySMm is liSMfor more inform5)thorship,8copyright and licmask;thorship1 copyrigpda_alarm_regotice.  This file ete prograREG2009 HSC_FIFstribay only be 0s software may be 009 Neterion Inc.
Neteriay only be use        e I/O ViAdapter.
  *           2 Server Adapter.
 ter.
GE_REG_H

 VXGE_REGcc_error * Copyright( *********tem is lion InCC000ULFRM_BUF_SBE(n)ay only be ns softwarloc)		(0x8000000000000000ULL >>TXDO

/*
 * vxge_vBIT(val, loc, sz) - set bits at offset
 */
#d (loc))
D/*
 * vxge_vBIT(val, loc, sz) - set bits at offset
 */
#define vxge_vBIT32(val, loc, sz)	(((u32)(val)) << (32-(loc)-(sz)FNeteri progr
 * vxge_vBIT(val, loc, sz) - set bits at offset
 */
#dSERR
 * vxge_vBIT(valhorship3 Server A*/
#defin *           3 offset
 */
#definc) - set bit a4 copyriglso
#define vxge_mBIT(loc)		(0xdistributi000000ULdistABORT(va vxge_vBIT(val, loc, sz) - setne	VXGE_HW_TITAN_ASIC_ location
 */
#define vxge_bVAID(bits)e I/O Vi		vxge_bVA *           5 \
							vxge_bVAc) - set bit a5e I/O Vism
#define vxge_mBIT(e may be Neterion on ISMs, 48, 8)
#defay only be us 10GbE PC6 copyrigne	VXGE_H *           6)

#define	VXGE_Hc) - ser the GPL and7aust
 * a8tain 77e authorshipae I/O Vitxd_ownership_ctrlotice.  This file TXD_OWNERSHIP_CTRL_KEEP_COMMAND		ay only be 7AN_ASIC_Ib - 1))

#defcfvxge_mBIT(loc)		(0x8000CF0000ULENABL*
 * vxge_vBIT(val, loc, sz) - set bitRPCIM_REG_REG_SPAC_Nine	VXGE_HW_TITAN_ASIC_Ib offset
 */
controne VXGE_HWTITAN_VPMGMT_RONTROL_FEEG_SPACE******************
 6, PL.
 * See the fileMODE_RESERVEEARLY_ASSIGN_EN1(bits) \
					IOV				1
#define VXGE_HW_ASIUNBLOCK_DB***************3**/
#ifndec Server Adapense n1fine VXGE_HW_ASIC_M**** comp1XGE_HWRAP_0		0xefine VXGE_HW_ASIC_MO4, ation.
 *
define	VXGE_HW_TXMAC_GEN_CFG1_1LOCK_BCAST_TO_WIRE		vxge_12BIT(19)
#define	VXGE_HW_TXMAC_GEN_CFG1_BLO2LOCK_BCAST_TO_WIRE		vxge_20BIT(19)
#define	VXGE_HW_TXMAC_GEN_CFG1_BLO3PEND_FCS			vxge_mBIT(31)

8BIT(19)
#define	VXGE_HW_TXMAC_GEN_CFG1_BLO4LOCK_BCAST_TO_WIRE		vxge_36BIT(19)
#define	VXGE_HW_TXMAC_GEN_CFG1_BLO5LOCK_BCAST_TO_WIRE		vxge_mmBIT(19)
#define	VXGE_HW_TXMAC_GEN_CFG1_BLO6LOCK_BCAST_TO_WIRE		vxge_5T(23)
#define	VXGE_HW_TXMAC_GEN_CFG1_HOST_A7LOCK_BCAST_TO_WIRE		vxge_6#defin1_TMAC_Pe I/O Virtualibw_timerfine VXGE_HW_ASIC_is notBW**************TRLAST_TO_SWITCH	vxge_mBIT(23)
r the GPL and900st
 *9retain 7he auhorshi9, copyrigg3cmctd license notice.  This file G3CMCTt
 * a complatioG3IFfor more inform0n(bits, 9Ie I/O ViET_TOTAL_GE *          9  Server ET_TOTAer_HW_VPATH_TO_FUNC_MAP_C							atioon I, 17,*****************ation.
 *
 * vxge-rCTRL_MODE_NON_OFFLOAGDDR3_DECCX3100 Series 1GE_HW_KDFC_TRPL_FIFO_0_CTRL_MODE_MULTI_OP_U_MODE		2

#defin6 VXGE_HW_KDFC_TRPL_FIFO_1_CTRL_MODE_ME	0xA5*****MODE		2

#defin7 VXGE_HW_KDFC_TRPL_FIFO_1_CTRL_MODE_MESSAGESSODE		2

#defin29 VXGE_HW_KDFC_TRPL_FIFO_1_CTRL_MODE_MESSAGES_Oval&~VXGE_HW_TO3*******************IFO_1_CTRL_MODE_MULTI_OP_MODE	DFC_INITIAL_BIR(**/
#ifnd9f VXGE_REdefine VXGEMODE_LEGACY_M
/*
 * vxdefine VXGEc) - se the GPL an30, 5)

C_GEtain 928 authorshC_GEcopyrigmcd license notice.  This file Mnsed  a complMem is_HW_TOCINITIAL_BIR( VXGE_HW_KDFC_TRPLHW_TOC_KDFC_VPGROCRC NeterioE_HW__GET_TOC_KDFC_E_HW_TOC_GET_KDFC_IHW_TOC_KDFC_VPFAU_GENODE_Ndefine	Vd may only be uused when the entir(bits)	bits

#defiES			GE_HW_KES		d may only be us 10GbE 30Ie I/O ViVALn(va *         30  Server m/
#de * Copyright(c) 2002-2ATH_STRon IMC_XFMD_MEM1, 15SGcationT_TOC_KDFC_VPATH_STRIDE(bits)	bit#define	VXGE_HW_KDFC_TRPL_FIFO_B		1
#define VXGE_HW_KDFC_TRPL_					vxge_bVAL, 17,RD*******TRPL_FIFO		2

#define VXGE_HW_KDFC_TRPL					vxge_bVALMIRIge_vBIT(val_0	0
#define VXGE_HW_KDFC_TRPL_FTRPL_FIFO_OFFSET_KDFC_FIFO_NUM1TRIDE_GET_TOC_KDFC_FIFO_STRIDE(bi#define	VXGE_HW_KDFC_TRPVXGE_H_OFFSET_GET_K1*******************			vxge_vBIT(val, 49, 15)

#definefine VXGE_HWET_GET_KDFC_RCTR0(bits)_KDFC_VAPTH_NUM(val) vxge_vBVXGE_HW_TXMAC_GEN1PL.
 * See the fileTRPL_FIFO_OFFSET_KDFC_F#define(val) vxge_v1DFC_RCTR2(bits) \
						vxge_bVALMODE_A				0
#defFSET_KDFC_FI1_KDFC_TRPL_FIFO_OFFSET_KDFC_VAPTH******************HW_KDFC_TRf VXGE_RE						vET_KDFC_RCTR1
/*
 * vx						vc) - set bit30t offset
grocrcpter.
 * Copyright(c) 2002-2XGE_HW_TOC_KDFEGGE_HW_WR********************VPATH_STRIDE(bits)	ine VXGE_HW_RTS_MWDE2MSR(val) vxgeal, 42, 5)
#dCICFGMGM30) - 1))

             ET_KDFC_RCTR1SIC_ID_GE             #define	VXGE_HW_TOC1GET_USD1_INITI304e authorsh    copyrighx_thresh17
#_repefine VXGE_HW_ASIC_RX_THRESH_REG_REPL_PAUSE_LOW    efine VXGE_HW_ASIC_MO This softwarSEL_PN                  3
#define HIGHE_HW_RTS_MGR_STEER_CTRL_Dicense (GPL),SEL_PN                  3
#dRED    _0AST_TO_SWITCH	vxge_mBI underT_SEL_RTH_GEN_CFG         5
#define VXGE_HW1_FCS			vxge_mBIT(31)

4STRUCT_SEL_RTH_GEN_CFG         5
#define VXGE_HW2ET_BMAP_ROOT(bits) \
	2STRUCT_SEL_RTH_GEN_CFG         5
#define VXGE_HW3_BCAST_TO_WIRE		vxge_m This software may be                3
#dGLOBAL_WASIC			2
#define6PL.
 * See the file               3
#dEXACT_VP_MATCH_REQRTS_MGR_STEE3er the GPL an33bust
 ATA_     10BIR(val) \
3REG_SPACEfb						17
#define VXGE_HW_TITFBfineMT_REG_		0
#define VXGE_HW_ASIC_MO3, 5_STEER_CTRL_DA4GET_USD4       3c
#define VX4E_HW_RTS_pcipifd license notice.  This file PCIP17, 15 a complDBVALn(bitA0_GET_DA__GET_TOC_KDFC_VPATH_STRIDE(bits)	_RTS_MGR_STEER_DATS0_GET_DA_HW_RTS_MGRO_STRIDE_GET_TOC_KDFC_FIFO_STRIDE_RTS_MGR_STEER_DATGENERAL(bits,XGE_HW_RTS_TRPL_FIFO_OFFSET_GET_KDFC_RCTR0(bi 48)
#define VXGE_HRPCIM_MSGbits, 0, 48))
#define	VXGE_HW_R_MASK(bits) \
							vxge_bVALn(bMts, 0,SPARE_R1e_vBIT(val, 0, 48INT**********ay only be u9RUCT_SEL4Ie I/O ViERSION     ET_KDFC_RCTR4  Server db VXG			vxge_bVALn(bits, 17, 1A0_GET_DA_000000I_RETRYefine vL_DATA_STRUCT_SEL_DA              DATA1_GET_DA_MAC_ADDR_ADSOTPATH(bits) \
					E_HW_TOC_GET_KDFC_IDATA1_GET_DA_MAC_AP_HDA_MAIVE_BUFFER			2

used when the entir(val) \
							vxge_DATAT(val, 55, 5)
#defiTEER_DATA1_DA_MAC_ADATA1_GET_DA_MAC_ANe_vBIT(val, 55, 5)
#defi_KDFC_INITIAL_BIR(7(bits, 62, 2)
#defineAC_ADDR_ADD_MODE(bits)23AC_ADDR_Af VXGE_RE
#define V
								vxge
/*
 * vx
#define VGR_STEER_CTRL4t offset
s#define VXGE_HW_RTS_MGR_STEER_HW_RTS_MGRDA_MAC_ADDR_ADD_VPIT(val, 42, 5)
#dVPATH_STRIDE(bits)	e	VXGE_HW_RTS_ACCESS_STETS_MIT(val, 42, 5)
#dE_HW_TOC_GET_KDFC_Ie	VXGE_HW_RTS_ACCEe_vBITIT(val, 42, 5)
#dine VXGE_HW_RTS_MGR_ne	VXGE_HW_RTS_ACCESAC_ADEER_CTRL_ACTION_RETEER_DATA1_DA_MAC_Ae	VXGE_HW_RTS_ACCEine VXGEER_CTRL_ACTION_RE_KDFC_INITIAL_BIR(7VXGE_HW_RTS_ACCESS_ST_STEER_CTRL_ACTION_WRIGE_HW_RTS_A) - 1))

L_ACTION_D
								vxgeSIC_ID_GEL_ACTION_DHW_RTS_ACCESS_) \
					general VXGE_HW_KDFC_TRPL_FIFO_0_CXGE_HW_RTS_DA_MAC_ADROPPED_ILLEGAL_REGTA_STRUCT_SEL_DA               UCT_SEL_DA		0
#defiHW_RTS_AKDFCMAP_PROESS_STEER_CTE_HW_TOC_GET_KDFC_INUCT_SEL_DA		0
#defiLINK_RSTs, 48, 8RL_ACTION_READ_ENTRY			0
#defin#define	VXGE_HW_RTS_RX_HW_RTS_ATLP_VPLANE_ACTION_WRITE_ENTRY		1
#define #define	VXGE_HW_RTS_TRAININ   3SET_DEmay only be u_KDFC_INITIAL_BIR(7)UCT_SEL_DA		0
#defiRTS_ACCESDOWNT_SEL_RTH_SOLO_GE_HEL_PN		3
#define	VXGE_HW_RTS_ACCESS_STRUCTAne	VLLE_HW_TITAN_P2TRUCT_SEL4D_GET_INI_HW_RTS_ACCE
								vxges) \
				_HW_RTS_ACCEHW_RTS_ACCESS_8)

#definrpcim_msRL_DETE_ENTRY		1
#definets, 0, 48)PATH_WLOADts, 0,TOe_vBIT(v_STRUC0_Re VXGE_TS_MGR_STEER_DATA1_DA*******************T_SEL_QOS		10
#define	VXGE_HW_RTS_ACCESS_ST1ER_CTRL_DATA_STRUCT_SEL_DS		11AD_ENTRY			0
#define_SEL_QOS		10
#define	VXGE_HW_RTS_ACCESS_ST2ER_CTRL_DATA_STRUCT_SEL_DS		11PL.
 * See the fileTRUCT_SEL_FW_MEMO		13

#define	VXGE_HW_RTS_3ER_CTRL_DATA_STRUCT_SEL_DS		11IRST_ENTRY		2
#defin_SEL_QOS		10
#define	VXGE_HW_RTS_ACCESS_ST4ER_CTRL_DATA_STRUCT_SEL_DS		11ation.
 *
 * vxge-r, 0, 48)

#define	VXGE_HW_RTS_ACCESS_STEER_5ER_CTRL_DATA_STRUCT_SEL_DS		11E_ENTRY		1
#define V_SEL_QOS		10
#define	VXGE_HW_RTS_ACCESS_ST6ER_CTRL_DATA_STRUCT_SEL_DS		11VXGE_HW_KDFC_TRPL_FACCESS_STEER_DATA0_GET_ETYPE(bits)	vxge_bVA7ER_CTRL_DATA_STRUCT_SEL_DS		11NEXT_ENTRY		3
#defin_SEL_QOS		10
#define	VXGE_HW_RTS_ACCESS_ST8ER_CTRL_DATA_STRUCT_SEL_DS		11is software may be _SEL(bits) \
							vxge_bVALn(bits, 3, 1)
9ER_CTRL_DATA_STRUCT_SEL_DS		11EMO_ENTRY		3
#define_ACS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_IEER_CTRL_DATA_STRUCT_SEL_DS		11_CFG4_RING_MODE_ONE_xge_bVALn(bits, 7, 1)
#define	VXGE_HW_RTS_ACIT	12
#define	VXGE_HW_RTS_ACCESSS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO		13

#define	VXGE_HW_RTS_1ACCESS_STEER_DATA0_GET_DA_MAC_A
#define VXGE_HW_PRCxge_bVALn(bits, 7, 1)
#define	VXGE_HW_RTS_AC_ACCESS_STEER_DATA0_DA_MAC_ADDRPRC_CFG7_SCATTER_MODxge_bVALn(bits, 7, 1)
#define	VXGE_HW_RTS_ACDATA0_GET_VLAN_ID(bits) vxge_bVMODE_C				1

#definexge_bVALn(bits, 7, 1)
#define	VXGE_HW_RTS_ACD(val) vxge_vBIT(val, 0, 12)

#TE_ENTRY		1
#define V VXGE_HW_RTS_ACCESS_STEER_DATA0_PN_PORT_NUMLn(bits, 0, 11)
#define VXGE_HW16SK		8
#debVALn(bitsTS_ACCESS_
								vxgeHW_HOST_TYTS_ACCESS_#define	VXGE_HW_TOC6GET_USD6       4) \
							_RTSATA_STRUcmg1AL_GET_KDFC_MAX_SIZE(bits) \
	CMG_HW_R#define VSSALn(bit	VXGE_ 15)

#define VXG				vxge_vBIT(val, 10, 2)
#define	VXG0EER_0	VXGE_H0nd may only be used when the entirENKINS	0
#define	VXGE_HW_R1	VXGE_H_HW_RET_DA_MAC_ADDR(bits) \
							l, 10, 2)
#define	VXG1HW_RTS_ACC1SS_STEER_DATA0_RTCFG	7
#define	VXGE_Hefine	VXGE_HW_RTS_ACCESS_CESS_S1EER_DATA0_RTH_GEN VXGE_HW_KDFC_TRPL_F, 10, 2)
#define	VXG2HW_RTS_ACC2SS_STEER_DATA0_RTe VXGE_HW_KDFC_TRPL_ATA0_RTH_GEN_RTH_TCP_IPV4CESS_S2EER_DATA0_RTH_GENVXGE_HW_KDFC_TRPL_FI, 10, 2)
#defineUQocatioXGE_O_STRIDE_GET_TOC_KDFC_FIFO_STRIDES_STEER_DATA0_GET_QGE_HW_RT
#de_DATA0_RTH_GEN8RUCT_SEL6FC_TRPL_FILG_SEL(va *    TEER_CTRL_DAaGET_USDa       61_STEER_DATAa_SEL_FW_VERG_SEL(val) \
							vxge_vBIT(vaP, 10, 2)
#definePVXGE_HW_RV6_EN	ESS_STEER_DATA0_RTH_GEN_ALG_SEL_JTH_GEN_RTH_TCP_IPV
#define_GEN_d may only be used when the entirEER_DATA0_GET_RTH_GEN_CGE_HW_RVXGE_HW_R_DATA0_RTH_GEN_ALG_SEL_CRC32C	2
#EER_DATA0_GET_RTH_GEN_SW_RTS_ACCESS_HW__GET_TOC_KDFC_VPAs, 23, 1D_PRIVILEGGEN_RTH_TCP_IPV6_EN(bits)4_GET_US_bVA     aLn(bits, 23_bVAcopyrigoned license notice.  This file ONEt
 * a complRXPEne VXGge_mO_STRIDE_GET_TOC_KDFC_FIFO_STRIDECP_IPV6_EX_EN vTge_mBCCACCESL_FIALn(bitN_RTH_IPV6_EX_EN(bitL_DATA_STRUCTs, 3, 1)
#define	VXGE_HW_RTS_AATA0_GET_RTH_GEN_RTH_IPV6_EX(valbits) \
							vxge_EN	vxgeits, 35, 1)
#define	VXGEation.
 *
 * vxge-rATA0_GET_RTH_GEN_RTHs) \
				GE_HW_RTS_MGR_STEER_DATA1_DA_MAC_ACP_IPV6_EX_EN vDLocatio_DAT_DATA0_RTH_GEN__HW_RTS_ACCESS_STEER_DATA0_RTH_GEN					vx_bVALn(bits, 39, NITIA0_RTH_GEN_RTH_TCP_IPV6_EX_EN vxe_mBIT(3_STEER_DATA0_GET_R_KDFC_INITIAL_BIR(7N(bits) \
							vx locatioTA0_RTH_#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_GEOEn(bitsTEERBIT(15)
#defines 10GbE 4RPL_FIFO_OACCESS_Sts) \
							vxge_81ust
 1)
#tain40)
#define	VX8f VXGE_REnoa_wctdefine VXGE_HW_PF_SW_RENOA_WCTTI_OP_VPt
 * NUG1(bits) \
	VXGE_HW_48
/*
 * vxr	17
#2A_STRUCT_SEL_PN    T_REG2oc))F1_SIZdefine VXGE_HW_ASIC_MO0DATARUCT_SEL_RTH_SOLO_I\
							vx2e_bVALn(bits, 9, 7)
#definTA_SXGE_HW_RTS_ACCESS_STEER_DATA0_RT3e_bVALn(bits, 9, 7)
#definDATAXGE_HW_RTS_ACCESS_STEER_DATA0_RT4e_bVALn(bits, 9, 7)
#definLiceESS_STEER48t offset
SOLO_I3_BUCKET_DATA(bits) \
			3			vx5e_bVALn(bits, 9, 7)
#define	VXGE_define	) - 1))

rx_multi_casSOLO_I_mBIT(3)
#EL_RTH_KEY  MULTI_CAS#defin1CT_SEL_SS_STEER_DATA0_GET_PNSEL_PN      EM0_ENTRY_EN(bitsDELAY************CKET_DATA(val) \1
#def 0, 8)
#e I/O Virxdm_dbg_rdA_STRUCT_SEL_PN     Dne	VG(valADDW_RTS_MGR_STEER_CTRL_DATA
#define VXGE_HW_PRCTA0_GET_RTH_) \
							vxge_bNITIAL_OF48) \
					e	VXGE_HW_R_dataTS_ACCESS_STEER_DATA0_GET_RTH_AC_ADRMC	vxge_vBIT(val, 9Ln(bits, 9, 7)
#define	V6Ln(bits,48D_GET_INIrqa_top_prty_for_vh[17]A_STRUCT_SEL_PN    QA_TOSTEETY_FOR_VH8)
#define	VXGE_HW_R or derived from this code fall9
#define VXGE_HW43, 5)

al, XGE_H8he authorshal, _bVALn(bimcense notice.  This file TT(vaRTH_GENI_RTSRUCTINTEER_RESSTS_ACCESS_STEER_DATADFC_TRPL_FEER_definnableGET_RTH_ITEM1_ENTRY_ENES			17
#deVBLS_			2
#defineE_HW_TOC_GET_KDFC_INTRY_EN	vxge_mBBS_ST			2
#define VXGE_HW_ASIC_MODE_MEER_DATA0_GET_RTH_IT 48)			2
#defineGE_HW_RTS4MODE			0
#EER_b#define VXGE_HW_PF_SW_RESIM_B			0xA5RD_XO)
#define	VXGE_HW_RTS_ACCESS_STEER_\
						WRxge_vBIT(val, 2ATA(bits) \
							vxge_\
							E_HW_BYUCT_SEL_RTH_MW_RTS_ACCESGE_HW_RTS_ACresource_assignment			vxge_bVALn(bits, 16, 8\
					OURCEE_SR_IOMENTW_RTTH_ITROO_DATA0_RTH_ITEM0_ENTRY0ER_D_RTS_ACCEaS_STEER_DATA0map_mapping_vpine _RTH_ITEM0_BUCKET_NUM(val) 25, 7APPATA_VP			vxgIM_DES 10
ATA0_RTH_LTI_IT        12
#define VXGE_HW4b, 16, 8b
#defina_BIR(val) \fine_GET_RTH_GE2EL(val) \
							vxge_vBIT(val, 128)

#define VXTfine VXVALn(bCCESS_STEER_DATA0_RTH_GEN_RTH_IPV4_EN							vxge_bVCVXGE_HBUCKGE_HW_RTS_MGR_STEER_DATA1_DA_MAC_A_DATA1_RTH_ITEM0_Cfine VXRTS_EN_ACTIVE_TABLE	vx_ACCESS_0_GET_RTH_GEGET_RT *         4bODE			0
#dxt						vxge_bVALn(biloc)		(0xVALn(bits,TS_MGLn(bBDTefine	VXGRW_RTS_MGR_STEER_CTRL_DATAT(19)
#define	VXGE_CESS_STEER_DATA1_RTH_ITEM1_L_FIFO_BCAST_TO_WIRE		vxge_mBIT(19)
#defal, 9, 7)
ESS_STEER_DATA1_RTRTS_(val, 9,(val, 55, 5)
#defis software may be xge_bVALn(bits, 24,REQ********************_KDFC_INITIAL_BIR(7)EM1_ENTRY_EN	vxge_mBITAC_AD*******************_CFG4_RING_MODE_ONE_DATA1_GET_RTH_ITEM1WR_RSPDATA(bits) \
							vH_GEN_ALG_SEL_MS_RSSEM1_ENTRY_EN	vxge_mDTS_ACCESS_STEER_DATA1_RTH_ALG_SEL_CRC32C	2
#dge_bVALn(bits, 24, 1I_WRACCESS_STEER_DATA1_RTHCFG	7
#define	VXGE_HEER_DATA0_GET_RTH_JHASH_CFCTRL_DATA_STRUCEN(bits) \
							vxge_bVALn(bits, 24, 1I_R_CFG_GOLDEN_RATIO(bits)e VXGE_HW_KDFC_TRPL_H_CFG_GOLDEN_RATIO(val) \
CTRL_DATA_STRUCVXGE_HW_KDFC_TRPL_FIH_CFG_GOLDEN_RATIO(val_STEERR_CTRL_DATA_STRUCE_HW_TOC_GET_KDFC_INH_CFG_GOLDEN_RATIO(valCMC0_IFA0_GET_RTH_JHASHS_STEER_DATA1_RTH_ITEM1_ENTRY_EN	vxge_H_IT(valARB, 32, 32)
#define	VW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM1GE_HW_RTCF_MGR_STEER_CTRL_WE_2xge_bVALn(bits, 25, 7)
#define	VXGE_HWGE_HW_RTDFEfineCREDIT_OVERFLOW***************)
#def_ITEM1_BUCKET_DATA(val) \
							vxge__MASK_IPV6_SA_MASK(valUND\
							vxge_vBIT(val, 0, XGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH__MASK_IPV6_SA_M(bits, 0, 16)
#def \
							vxge_bVALn(bits, 0, 32)
#defGE_HW_RTR	0xA5ASK(val) \
							vxge_vBIT(val, 0, EN(bits) \
							vxge_bVALn(bits, 24,16, 16)
#define	VXGE__DA_MASK(bits) \
							vxge_b, 0, 32)
#define	VXGE_HW_RTS_ACCESS_ST16, 16)
#defin(bits, 0, 16)
#def_CFG_INIT_VALUE(bits) \
							vxge_bVGE_HW_RTWCOMPge_vBIT(val, 32, 4)
XGE_HW_RTS_ACCESS_STEER_DATA0_RTH_JHAS_GET_RTH_MASK_ITACTRL_ACTION_LED_C				vxge_vBIT(val, 32, 32)

#define	VXGE_HW_RTWmBIT(bits, 0, 16)
#defET_RTH_MASK_IPV6_SA_MASK(bits) \
							vxge_bval, _CTRL_DATA_STRUCT_SExge_bVALn(bits, 25, 7)
#define	VXGE_HWCP2H_ITR*****POTA0_GET_RTH_JHASRTH_GEN_REPL_ENTRY_ETEER_DATA0_RTH_JHASne	VXGE_HW_RTDATA0_RTH_MASK_L4SXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_A1_RTH_ITV6_SA_MDATA0_RTH_MASK_L4S \
							vxge_bVALn(bits, 0, 32)
#defts) \
					CTRL_DATA_STRUCT_SESTEER_DATA0_RTH_JHASH_CFG_GOLDEN_RATIO(valS_ACCESS_STEER_DATA0_3vxge_mBITbET(val) \
 16, 8)
#							vxge_bSDC_INITIA 16, 8)
#c) - set bit4bt offset
c						vxge_bVALn(bits, 17, 1RTS_ACCETS_MRTS_RTS_ 32, 32)
#define	TEER_DATAb) - 1))

SS_STEER							vxge_b8)
#definSS_STEERdefine VXGE_HWfine	VXGE_cits)                1

#defiUCKET_Dval, P_H2L2CACCESS_STEER_DATA1_RT0_RTH_GEN_ALG_SEL_JENDATA0_QOS_ENTSTCEN		vxge_mBIT(3)

#defiH_GEN_ALG_SEL_MS_RSS	_STEER_DATA0_GEEEN		vxge_mBIT(3)

#defi_ALG_SEL_CRC32C	2
#deDATA0_QOS_ENTT	VXGE_HW_RTS_ACCESS_STEERRTH_ITEM1_fine	VXGE_fine	VX							vxge_b_DATA_STRUfine	VX#define	VXGE_HW_TO4f, 16, 8f
#definb5W_RTS_ACCESf1)
#define	VXGET_RTH_ITEM0_BUCKET_DATA(bitsTH_G							vxge_bPALn(bits,defineA0_DA_MAC_ADDR(val) vxge_vBIT(val, define	VXGE_HW_RP_EXTS_A_XT	vxgeGE_HW_RTS_MGR_STEER_DATA1_DA_MAC_ADDODE(val) \
							vCCESRTH_ITTEER_DATA1_GET_RTH_ITEM1fH_TCP_IPV6_EXts) \
							vxge_f  Server A 16, 8)
#define	VXGE_HW_RTS_ACdefine VXG_STEER_DATA_S_MGSRA1_BUCKET_NUM(val) \
							vxge_EN_RTH_IPV6_EN	vxge_KET_NUM(val) \
			MPT_mBIT(24)
#define	VXGE_HTS_ACCESS_STEER_DATA1_GET_RTH_ITEM4_ENTRY_PS_ACCESS_STEER_DATA1_RTvxge_bVALn(bits, 0, 1_GET_RTH_ITEM4_ENTRY_WS_ACCESS_STEER_DATA1_RTation.
 *
 * vxge-r(8)
#define	VXGE_HWURY_EN(bits) \
							vxge_TEER_DATA1_DA_MAC_ADA(bits) \
							vxge_SS_STEER_DATA1_RTH_ITEM4VXGE_HW_KDFC_TRPL_FSS_STEER_DATA1_RTH_ITEM_ACCESS_STEER_DATA1_GET_(val) vxge_vBIT(val,define	VXGE_HW_RTS_AY_EN(bits) \
							vxge_is software may be NUM(bits) \
							vxgeSS_STEER_DATA1_RTH_ITEM4_KDFC_INITIAL_BIR(7NUM(bits) \
							vxge_ACCESS_STEER_DATA1_GET__CFG4_RING_MODE_ONE_NUM(bits) \
							mBIT(24)
#define	VXGE_H_ADDR_MASK(bits) \
		ATA1_GET_RTH_ITEM1_BUCKET_DATA(bits) \
							vTS_ACCESS_STEER_DATA1_GET_RTH_ITEM4_EN_RTS_ACCESS_STEER_DATA1_RTH_ENTRY_EN	vxge_mBIT(8)
#define	VXGE_HWvBIT(val, 25, 7)

#define	VRTH_ITEM4_BUCKET_DATA(bits) \
							vTRY_EN(bSHADOW			vxge_vBIT(val, 0, 32)
#define	VXA1_GET_RTH_ITEM4_ENTRY_ESP
							vxge_vBIT(val, 				vxge_vBIT(val, 9, 7)
#define	VXGE_HW_R) \
							vxge_vBIT(val, T_RTH_ITEM5_BUCKET_NUM(bits) \
							xge_bA1_GET_RTH_ITEM6_BUCKET_Nfine	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_IT) \
							vxge_vBIT(val, 					vxge_vBIT(val, 16, 8)
#define	VXGE_HW						vxge_vBIT(val, 32, fine	VXGE_HW_RTS_ACCACCESS_STEER_DATA1_XILGE_HW_RTS_ACCESS_STEER_define	VXGE_HW_RTS_ACCESS_STEER_DATA1_S_ACC_HW_RTS_ACCESS_STEER_TS_ACCESS_STEER_DATA1_GET_RTH_ITEM4_EN_vBIts) \
							vxge_bVAL_ENTRY_EN	vxge_mBIT(8)
#define	VXGE_HWCMW_STEER_DATA1_RTH_ITEM6_RTH_ITEM4_BUCKET_DATA(bits) \
							vCMR_STEER_DATA1_RTH_ITEM6_25, 7)
#define	VXGE_HW_RTS_ACCESS_STEER_DATSK(b_vBIT(val, 32, 4)
#define	VXGE_HW_RTS_HW_RTS_ACCESS_STEER_DATA1_ESS_STEER_DATA1_RTH_STEER_DATA1_RTH_ITEM6_BUCKET_NUM(val) \
		ACCESS_STEER_DATA1_RTHfine	VXGE_HW_RTS_ACCESS_STEER_DATA1_RT \
					ESS_STEER_DATA1_RTH) \
							vxge_bVALn(bits, 40, 1)
#define	ACCESS_STEER_DATA1_RT_bVALn(bits, 40, 2)
Ln(bits, 40, 1)
#define	VXGW_RTS_ACCESS_STEER_define	VXGE_HW_RTS_ACCESS_STEER_DATA1_
#deW_RTS_ACCESS_STEER_TS_ACCESS_STEER_DATA1_GET_RTH_ITEM4_ENS_ACW_RTS_ACCESS_STEER_BUCKET_DATA(val) \
							vxge_vBIT(val, 4W_RTS_ACCESS_STEER_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM7_BUCKEW_RTS_ACCESS_STEER_25, 7)
#define	VXGE_HW_RTS_ACCESS_STEERXP5, 7PROMODE_CCESS_STEER_				vxge_vBIT(val, 9, 7)
#define	VXGE_HDATA0_MEMO_ITEM_VERSION   T_RTH_ITEM5_BUCKET_NUM(bits) \
							vDATA0_MEMO_ITEM_VERSION   fine	VXGE_HW_RTS_ACCESS_STEER_DATA1_RT_DATWR_MEMO_ITEM_VERSION   					vxge_vBIT(val, 16, 8)
#define	VXGSS_S_HW_RTS_ACCESS_STEER_D4DATA1_GET_RTH_ITEM7_BUCKET_DATA(bits) \define VXGE_HW_RTS_ACCESS_define	VXGE_HW_RTS_ACCESS_STEER_DATA1__DATINV_ITEME_HW_RTS_ACCESS_TS_ACCESS_STEER_DATA1_GET_RTH_ITEM4_ENSS_S          7

#define	VXGBUCKET_DATA(val) \
							vxge_vBIT(val_ON			1
#define	VXGE_HW_RTSCESS_STEER_DATA1_RTH_ITEM5_BUCKET_DATA(DATA0_MEMO_IN_STEER_DATA1_GET_Rne	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_I					vxge_bVALn(bits, 0, 8)
#de_ITEM7_BUCKET_NUM(val) \
							vxge_vRTS_ACCESS_SVALn(bits, 0, 8)
#deT_RTH_ITEM5_BUCKET_NUM(bits) \
							VXGE_HW_RTS_VALn(bits, 0, 8)
#de	vxge_bVALn(bits, 56, 1)
#define	VXGE_H_HW_RTS_ACCESS_STEER_DATA0_FW_V					vxge_vBIT(val, 16, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_FW_5DATA1_GET_RTH_ITEM7_BUCKET_DATA(bits)               16, 16)
#define VXGdefine	VXGE_HW_RTS_ACCESS_STEER_DATA1_L_ON			1
#def 16, 16)
#define VXGTS_ACCESS_STEER_DATA1_GET_RTH_ITEM4_EN0

#define VX 16, 16)
#define VXGCFG	7
#def_ITEM4_BUCKET_NUM(val) \
							vxge_vBIEER_DATA1_GET_RTH_ITEM1_ENT54_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM4_EN_ACCESSASK_L4PUSH 16)
#define VXG8)
#define VXGE_HW_RTS_ACCESS_STEER_DATACCESS_STEER_e VXGE_HW_RTS_ACCESCICFGMGM4ff VXGE_REG(bits) \
							vxge_f
/*
 * vxg 0, 64)
#define VXGE_Hf_RTS_ACCESine	VXGE_HW_RTS_ACloc)		(0xvxge_mBIT(3)

CP_DCACHERTS_ACCESS_STEER_DATA0_GET_FATA_STRUCT_SEL_RANGE_PFW_VER_BUILD vxgeIvBIT(val, 48, 16)

#define VXGE_H8_RTS_ACCESS_STEER_DATFW_VER_BUILD vxge_ACCEEER_CTRL_ACTION_RE0_RTH_GEN_ALG_SEL_JA1_GET_FLASH_VER_DDATA1_FLASH_VER_DAY(vaTH_GEN_REPL_ENTRY_EFW_VER_BUILD vxgeTRAC(val, 48N	vxge_mBIT(24)
#define	VXGE_HWvxge_mBIT(3)

DMAEN		e_bVALn(bits, 8, 8CFG	7
#define	VXGE_vxge_mBIT(3)

MPATA1_FLASH_VER_MONTH(vation.
 *
 * vxge-rvxge_mBIT(3)

QCT_DS_R_CTRL_ACTION_WRITE_ENTRY		1STEER_DATA1_GET_FLASH_VGET_DS_EER_DATA1_GET_RTH_ITEM1_ENTTA_SPL.
 * SeeER_DATA0_FW_VER_BUILD vxge_vBIT(vBUCKET_NUM(val) \
							vxATA_STRUCT_SEL_RTH_JHASA1_GET_FLASH_VER_DAY(bitBUCKET_NUM(val) \
							vxDATA 8)
#define VXGE_HW_RTS_ACCESS_STEER_DATAATH(bits) \
						_DATA1_GET_FLASH_VER_YEAR(bits) \XGE_HW_RATH(bits) \
						TEER_DATA1_DA_MAC_AONTH(bits) \
							vxgATH(bits) \
						VXGE_HW_KDFC_TRPL_FTS_ACCESS_STEER_DATA1ATH(bits) \
						E_HW_TOC_GET_KDFC_I8, 8)
#define VXGE_HATH(bits) \
						is software may be R_YEAR(bits) \
						ATH(bits) \
						_KDFC_INITefine VXGE_HW_RTS_ACCESS_STEERBUCKET_NUM(val) \
							vx_DAT)
#define VXGE_HW_RTS_ACCESS_STEERY_EN		vxge_mBIT(3)

#defiVER_MONTH(val) vxge_S_STEER_DATA0_GET_DS_ENTRY_EN(bits) \
		ATA0_GET_FW_VER_YEAR, 3, 1)
#define	VXGE_HW_RTS_ACCESS_STEERGE_HW_RTS_ACCESS_STEvxge_mBIT(3)

#define	VXGE_HW_RTS_ACCESS6, 16)

#define VXGE, 3, 1)
#defineWIFTRANSFERRED_GET_RX_FRM_T 8)
#define VXGE_HW_RTS_ACCESS_STEE2 notSFERRED_GET_RX_FRM_Tal) vxge_vBIT(val, 8, 8)
#define DAMTRANSFERRED_GET_RX_FRM_T_DATA1_GET_FLASH_VER_YEAR(bits) \VXGE_HSFERRED_GET_RX_FRM_Ts) \
							vxge_bVALn(bits, 40, 
						SFERRED_GET_RX_FRM_TSS_STEER_DATA1_FLASH_VER_MINOR vxge_vBITSFERRED_GET_RX_FRM_Tne VXGE_HW_RTS_ACCESS_STEER_DATA1CP_WAKPL_ENTRY_EGRITYDATA(val) \
				l) vxge_vBIT(val, 0, 8)
#define VXGEPMONbits, 0, 32)
#define	VXGE_HW_VRANSFERRED(bits) \
							vxge_bV notRD_STEER_DATA1_RTH_ITEM6ER_CTRL_DATA_STRUCT_vxge_mBIT(3)

PIFTne	VXGE_(bits)	(bits)
#
							vx_STEER_DATAC_ADDR_MASK(bits)fbits) \
		ge_bVALn(bits, 0, 48)
#defe#definf5#definf2
#define V_XOFVXGE_HW_VPAxcR_DATA0_RTH_KEY_KEY vxg		vxge			vxge_bVCfine VALn(RY_EN	vxge_mBIT(43)

#define	VXGE_W_VPATH_GENSTATS_COUNT0CRI		vxgne	VXGE_HW_VPATH_DEBUG_STATS1_GET_IATH_GENSTATS_64-(XOFF(bits) \
							vx8)

#defibits, 32, 32)
#definbVALn(bitbits, 3ts, 48, 16)
#dHW_HOST_Tbits, 3causR_DATA0_RTJOR(bits) \
	xge_n(bitsTATS_COUNT 8)
#define VXGE_HW_RTS_AT_DA the GPL an52, 5)

_VPAF(bits) \
							_VPA VXGE_HWsgLn(val, 61, 3)

#define	VXGE_H VXGE_N(bits) \
		GE_HW_RTO_STRIDE_GET_TOC_KDFC_FIFO_STRIDE(e_bVALn(bits, _GETCOUN_GETIT(val, 62, 2)

#defiVPATH_DEBUG_STATS3_G_GET_PPIF_VPATH_GENSRR3E_HW_VPATHts) \
							vxT_GET_KDFC_RCTR0(bit)
#define	VXGE_HW_VPA2						vxgets) \
							vx#define VXGE_HW_PRC_)
#define	VXGE_HW_VPAD_FRMS_GEts) \
							vxRTH_ITEM52PL_FIFO_OFNSTATS_ *         52SS_STEER_DATAe	VXGE_HW_RTS_ACCESS_STEE32, 32)
TS_MW_RTIT(24e_bVALn(bits, 8, ation.
 *
 * vxge-rC_FAIL_FRMS(bitsTH_ITPEER_CTRL_ACTION_WRITA(bits) \
							vxge_bSTATS_GET_RX_MPA_Bge_bVALn(bits, 0, VXGE_HW_KDFC_TRPL_F_DBG_STATS_GET_RX_MPA 48)
TRL_ACTION_LIST_NEXT_ENTRY		3
#defiC_FAIL_FRMS(bits) vxg_FIVE_BUFFER			2

#define VXGE_HW_PRC_DBG_STATS_GET_RX_MPA_MR(val, 55, 5)
#defiCFG	7
#define	VXGE_n(bits, 16, 16)
#define	(val, 55, 5)
#defi16)
#define	VXGE_HW_DBG_STATS_GET_RX_MPARESEDR_ADD_MODE(bits) \
							vxge_bVALnE_HW_DBG_STATS_GET_RX_EM*****ge_vBIT(val, 32, 4)8)
#define	VXGE_HW_RE_HW_DBG_STATS_GET_RX_FAU

#define	VXGE_HW_MRPCIM_DEB_KDFC_INITIAL_BIR(7C_FAIL_FRMS(bitsMS_MGPCIW   7

#define	VXfine	VXGE_HW_RTS_ACCC_FAIL_FRMS(bits					vxS_STUPDTbits) \
							vxge_b)	vxge_bVALn(bits, 0, 16)
#define	VCREAT_RTH_IRESEits) \
							vxge_bU_RX_VP_RESET_DISCARDED_FRMS(bits) 					IF_MISdefinBIT(val, 0, 8)
#defineCKET_NUM(val)_DBG_STATS_GET_RX_#define	VXGE_ine	VXGE_HW_TITAN_ASIC52fine	VXGE_HW__ADDR_MASK(bits52
/*
 * vxDTARB_NPc) - set bit52t offset
bVAL			vxge_bVALn(bits, 17, 15L_FIFO_TS_MUPefine Ln(bits, 0, 32)
#define	VXGE_HW_ge_bVALn(bits, 0, 32)
##define	VPGE_HW_Ln(bits, 0, 32)
#define	VXGE_HW_F_VPATH_GENSTATS_COUNT5#define	VSG_QUE_DMQM_CPL_EAD_CMDbits, 0, 32)
#defiER_DATA0_GET_DA_MAC_ADDR(bits) \
							TS4_GET_INI_2, 32)
#define	VXG				vxgTS_COUNT01_GET_GENSTATS_COUNT1(bitVPATH_STRIDE(bits)	bALn(bits, 32, 32)
#defiRUCTGENSTATS_COUNT01_ine VXGE_HW_KDFC_TRPL_FIFO_OFFSALn(bits, 32, 32)
#ete p at loc 0, 32)
#define	VXGE_HW_efine VXGE_HW_KDFC_TVXGE_HW_MRPCIM_DEBTE_SENT(bits)	(bits)
#define	VXGE_) \
							vxge_bVALn(bits, 0,WR_VPINTE_SENT(bits)	(bits)
#define	VXGEATH_GENSTATS_COUNT4_GETTATS_COUNT4_GET_DATA1_FLASH_VER_DAY(vis software may be TS4_GET_INI_WR_VPIN_HW_RTS_ACCESS_STEER_DAits, 0, 32)
#define	VXGE_HW_MRPCIM_DEB_DATA1_FLASH_VER_DAY(vas) \
							vxge_bVALn(bits, 0, IM_DEB_HW_RTS_ACCESS_STEER_DA_GENSTATS_COUNT3(bits) \
						WR_VPIN			vxge_bVALn(bits, 8, 82)
#define	VXGE_HW_GENSTATS_COU_STATS0			vxge_bVALn(bits, 8, 8xge_bVALn(bits, 32, 32)
#define2, 32)
#_STE	vxge_bVALn(bits, 32,8)
#define	VXGE_HW_Rts) \
							vxge_bVN_RTALn(bits, 32, 32)
#defi_KDFC_INITIAL_BIR(7ts) \
							vxge_bV31)
ALn(bits, 32, 32)
#deffine	VXGE_HW_RTS_ACCE_HW_DEBUG_STATS3_GETVPLANE_DEPL_PH(bits)	vxgets) \
							vxge_bVALn(bits, 32, 32)
#UMQ_DEPL_PH(bits)	vxgeROP_CLIENT0(bits)	vxge_bVALn(bit, 32)
#B_HW__CTRL_ACTION_LIST_FGE_HW_RTS_ACCESS_STEbVALn(bits, 32, 32)
#defe_vBIT(val, 42, 5)
#dRTH_ITEM7_ENTRY_EN	vbVALn(bits, 32, 32)
#dXGE_HW_e_vBIT(val, 42, 5)
#d57, 7)
#define	VXGE_ts) \
							vxgE_HWQRY	VXGE_HW_GENSTATS_COUNT23_GETTS_ACCESS_STEER_DATAts) \
							vxgFRMT_VPLANE_DEPL_CPLD(bits)	vxge_bVA_GENSTATS_COUNT3(bits) \
							vxge_bVine	WRIT(bits, 0, 32)
#define	VXGE_HW_ts) \
							vxge_bV			vxge_bVALn(bits, 32, n(bits,GENSTATS_COUNT01_GET_GENSTATS_COUNT0(biS3_GET_VPLANE_DEPL_CPLH(bits)	vxge_bVALTS_ME_HW_GER_DATA1_RTH_ITEM4_GENSTATS_COUNT5(bits) \
							vxge_bVge_vBIT(val, 32, 8)
#de16)
#define	VXGE_HW_DEBUG_STATSvxge_bVALn(b(val, 55, 5)
#defS_ACCESS_STEER_DATA0TATS0_GET_RSTDROP_MSG(bi

#define	VXGE_HW_Ln(bits, 32, 16)

#define	VXGE_HSTATS0_GET_

#define	VXGE_HW_Y_FRMS_GET_PORT0_TX_ANY_FRMS(bie	VXGE_HW_DEB

#define	VXGE_HW_XGE_HW_DEBUG_STATS4_GET_VPLANE_Ds, 0, 32)
#d

#define	VXGE_HW_ne	VXGE_HW_DEBUG_STATS2_GET_RSTDROP_CLI_bVALn(b

#define	VXGE_HW_16)
#define	VXGE_HW_DEBUG_STATS4_GET_VPENT2(bits(val, 55, 5)
#defGE_HW_RTS_ACCESS_STEE_HW_DEBUG_STATS3_GET_VPLANE_9abcdefULL
#definets) \
							vxge_bVALn(bits, 32, 32)
#UG_STATS9abcdefULL
#defines) \
							vxge_bVALn(bits, 0, 32)
#dets) \
AMGR_STEER_CTRL_DATA_S48, 16)
#define	VXGEts) \
							vxge_bVMXP2fine0_GET_INI_NUM_MWR_SENT(bits) \
							vts) \
							vxge_bVKDALn(IF32)
#define	VXGE_HW_VPATH_DEBUG_STATS1_Gts, 16, 8)

#define VXP2definits) \
							vxge_bVALn(bits, 0, 32)
#			vxge_bVALn(bits, 32, 9abcdefULL
#defineXGE_HW_DEBUG_STATS4_GET_VPLANE_DEPL_PD(n(bits,9abcdefULL
#definene	VXGE_HW_DEBUG_STATS2_GET_RSTDROP_CLIn(bi00000000000ULL

#define 16)
#define	VXGE_HW_DEBUG_STATS4_GET_VPLAts)	vx(val, 55, 5)
#defxge_bVALn(bits, 0, 32)
#UG_STATS4_GET_VPLANE_D2c480ULL

#define VXGEIF_VPATH_GENSTATS_COUNT5UG_STATS4_GET_VPLANE_DEPL_NPDLAP_ENABLE		0xFFFFs) \
							vxge_bVALn(bits, 0, 32)
#deUXP2				T_FLAP_DISABLE		0x00_FRMS(bits) - 1))

fine	VXGH_CRDT_DEPLET8)
#definfine	VXG#define	VXGE_HW_TO534ATH_GE34TATS_522
#define V_hw_ister and dm, 32, 32)
#define	VXGE_H_GENSTATn(bits, 0, COUNT01_GET_PPIF_VPATH_GEe VXGE_HW_SWAPPER_BYTE_SWwapper_fb;
#define Vs, 0, 32)
#define	VXDBG_STAT_RX_ANY_FRMS_GETwapper_WR_VPINfine VXGE_HW_TOC_SWAPPER_FE_HW_SWAPPER_READ_BYTE_SW_PIFM_RD_SWAP_EN_PIs, 0, 32)
#define	VXGE_HW_VPATH_GENSTATvxge_vBIT(val, 0, 6F_VPATH_GENSTATSs) \
							vxge_bVALn(bW_PIFM_RD_SWAPF_VPATH_GENSTATS_COUNT0(53D_GET_INI0[0x0001H_CRDT_DEPLE3ge_bVALn(0[0x0001bVALn(bits, 338)

#defi0[0x00013_GET_PPIF_VPATH_GENSTA8*/	u64	T3(bitGET_POdefine	VXGE_HW_RTS_ACCESS_STn;
#define VXGE_HW_PIFM_WR_FLIWR_VPI	vxge_bVALn(bits, 0, 32)
#define	VXGE_HW_36ust
 538_legac36reg {

	u8	u8used00010[0x0rr210];

/*0x00010*/	u64	toc_svxge8)

#define VXGE_HW_CISPefineGENSTATS_COUNT01_GET_GENSTAge_bVALn(bits, 0, 32)
#define	VXGE_H
#define VXGE_definCCESS_EN_HOST_ACCESS_EN(val) vxge_vPIN_DROP(bits) \
							vxge_bVALn(bi
#define VXGE_defin

	u8	unused00050[0x00050];

/*0x00050*/	u64	tos) \
							vxge_bVALn(bit
#define VXGE_PI_DAT)

#define	VXGE_HW_DBGET_GENSTATS_COUNT0(bits) \
							vxge_bVALn(bVXGE_HW_TOC_COMMONTS_M, 0, 32)
#define	VXGE_HW_GENE_HW_SWAPPER_READ_BYTE_SWA
#define VXGE_W_RTne VXGE_HW_TOC_MEMREPAIR_POINTER_INITIAL_VALVXGE_HW_SWAPPER_READ_BIT_F
#define VXGE_ine	T_DA 0, 32)
#define	VXGE_HW_GEN2)
#define	VXGE_HW_GENSTAT_POINTER_INITIAge_mcim_pointer;
#define VXGE_HW_TOXGE_HW_DEBUG_STATS4_GET_VP
#define VXGE_31)
#cim_pointer;
#define VXGE_HW_TOne	VXGE_HW_DEBUG_STATS2_GEVXGE_HW_TOC_COMMON_cim_pointer;
#define VXGE_HW_TO16)
#define	VXGE_HW_DEBUG_ vxge_hw_toc_regA00270];

/*0x00278*/	u64	toc_vpmfine	VXGE_HW_DEBUG_STATS0_G
#define VXGE_HP_POINTER_INITIAL_VAL(val) vxge_vBc_common_pointer;
#define VXGE_HW_TOC_COLONGTERM_mrpci16N_HOST_ACCESS_EN(val) vxge_vBIT(val, 0ts, 0, 32)
#define	VXGE_HW__HW_TOC_VPATH_POINTER_INITIAL5VAL(val) vxge_vBIT(val, 0, 64)
	u8	unus(val) vxge_vBIT(val, 0, 64)
/*0x00060*/	POINTER_INITIAL4VAL(val) vxge_vBIT(val, 0, 64)
	u8	unusine VXGE_HW_TOC_PCICFGMGMT_POINTER_INITIPOINTER_INITIAL3VAL(val) vxge_vBIT(val, 0, 64)
	u8	unus0[0x001e0-0x000e8];

/*0x001e0*/	u64	tocPOINTER_INITIAL2VAL(val) vxge_vBIT(val, 0, 64)
	u8	unusC_MRPCIM_POINTER_INITIAL_VAL(val) vxge_vPOINTER_INITIAL1VAL(val) vxge_vBIT(val, 0, 64)
	u8	unussrpcim_pointer[17];
#define VXGE_HW_TOC_POINTER_INITIAL0VAL(val) vxge_vBIT(val, 0, 64)
	u8	unus_vBIT(val, 0, 64)
	u8	unused00278[0x0027POINTER_INITIA9VAL(val) vxge_vBIT(val, 0, 64)
	u8	unusgmt_pointer[17];
#define VXGE_HW_TOC_VPMPOINTER_INITIA8N_HOST_ACCESS_EN(val) vxge_vBIT(val, 0e_bVALn(bits, 0, 16)
#defin];

/*0x00a00*/	u64	prc_stat7s1;
#define VXGE_HW_PRC_STATUS1_PRC_VP__pointer[17];
#define VXGE_HW_TOC_VPATH_POINTER_INITIA_VAL(val) vxge_vBIT(val, 0, 64)
	u8	unu_WRCRDTARB_PH_CRDT_DBIT(n)
/*0x00a08*/	u64	rxdcm_reset_efine VXGE_HW_TOC_KDFC_INITIAL_OFFSET(vXGE_HW_MRPCIM_DEBUG_BIT(n)
/*0x00a08*/	u64	rxdcm_reset_AL_BIR(val) vxge_vBIT(val, 61, 3)
/*0x0_RTS_ACCESS_STEER_DABIT(n)
/*0x00a08*/	u64	rxdcm_reset_FSET(val) vxge_vBIT(val, 0, 61)
#definevxge_bVALn(bits, 48,BIT(n)
/*0x00a08*/	u64	rxdcm_reset_
/*0x004b0*/	u64	toc_kdfc_vpath_stride;TS3_GET_VPLANE_DEPL_CPLH(bioc_kdfc_fifo_stride;
#define	OAD_RESET_IN_PROGRESS_PRC_VP(n)	vxge_mB, 0, 64)
/*0x004b8*/	u64	toc_kdfc_fifo_stride;
#defineVXGE_HW_TOC_KDFC_FIFO_STRIDE_INITIAL_TO		vxge_bVALn(bits, 564	rd_req_in_progressRL_DAine	VXGE_HW_GENSTATS_COUNT23_GET 16)
#define	VXGE_HW_DEBUG_AL(val) vxge_vBIT(HOST_ACCESS_EN_HOST_ACCESS_EN(val) vxge_vBIT(val, 0_bVALn(bits, 40, 2)

#define VXGE_HW_TOC_SRPC0-0x00a48];

/*0x00b00*/	u64	one_cfg_vp;
#define VXGc_common_pointer;
#define VXGE_HW_TOC_CO5b3f7ULL
a48];

/*0x00b00*/	u64	one_cfg_vp;
#define VXG_REPLICQ_FLUSH_IN_PROGRESS_NOA_VPts, 5_GET_ULTISK(valCNTARB_XOFF(bits) ALn(bits, 32, 16)

#define	
#defiWR_VM_VP(n)	vxge_mBIT(n)
/*0x00b88*/	u64eset_in_progress;
#define VXGE_HW_MXP_CMUMQ2defiW_GEN_FLAP_DISABLE		0x00_REPLICQ_FLUSH_IN_PROGRESS_NOA_VW_RTTIMALn(bLR_INT_EN_VP(n)	vxge_mBIGE_HW_PIFM8gister and dma 2_WR_SWAP_EN(va9T(val, 0, 64)
/*0 0, 64)
/*0x009gister and dma 30x00040*/	u64	host_access_en;3_STAT_TX_ANY_vBIT(val, 48(val) vxge_vE_HW_ONE_CFG_VP_RDY(n)	vxgS_TIM_VPATH(n)	vxge_mBIT(nFG7_SCATTER_M	u64	tim_outstanding_bmap;
#define VXGE_HW_TIM_OUTS2al, 0, 64)
/*0x00058*/	u64	toc_memrepS_TIM_VPATH(n)	vxge_mBIT(n3TS_COUNT0(bits) \
							vxge_bVALn(bS_TIM_VPATH(n)	vxge_mBIT(n4r[17];
#define VXGE_HW_TOC_PCICFGMGMTS_TIM_VPATH(n)	vxge_mBIT(n5	unused001e0[0x001e0-0x000e8];

/*0x0S_TIM_VPATH(n)	vxge_mBIT(n6 VXGE_HW_TOC_MRPCIM_POINTER_INITIAL_VS_TIM_VPATH(n)	vxge_mBIT(n7*/	u64	toc_srpcim_pointer[17];
#definS_TIM_VPATH(nDAY(bits) \
	(val) vxge_v0x00a40*/	u64	kdfc_reset_idefine VXGE_HW_MSG_DMQ_NONFSET_KDFC_FI16)
#define	VXGE_HW_DEBUG_S_TIM_TDROP_MS_MSG_DMQ_NONI_RTL_PREFET
#define	VXGE_HW_TOC_KDFC_VRTL_BWR_PREFETCH_DISABLE(nFG7_SCATTER_Mg_dmq_noni_rtl_prefetch;
#defineR_PREFETCH_DISABLE(nx00bb0];

/*C_KDFC_FIFO_STRIDE(val) \
	RSTHDLR_CFG0_SW_RESET_VPAT_MSG_RESET_I {

	u8	unused00a00[0x00a00RSTHDLR_CFG0_SW_RESET_VPAT/*0x00c08*/	e_bVALn(bits, 0, 16)
#definRSTHDLR_CFG0_SW_RESET_VPATDY_MP_BOOTEDT_IN_PROGRESS_PRC_VP(n)	vxgRSTHDLR_CFG0_SW_RESET_VPATdefine VXGE_W_REPLICQ_FLUSH_IN_PROGRESSRSTHDLR_CFG0_SW_RESET_VPATc18*/	u64	ms;
#define VXGE_HW_RXPE_CMDSRSTHDLR_CFG0_W_MSG_DMQ_NONI_RTL_PREFETreset_in_progress;
#define 20*/	u64	cmn_rsthdlr_cfg4;FSET_KDFC_FI*/	u64	noffload_reset_in_prS_TIM_VPATH(n)	vxge_(val, (val) vxge_vint_en;
#define VXGE_HW_TIMd28];

/*0x00d40*/	u64	cmnFSET_KDFC_FIALn(bits, 32, 16)

#define	d28];

/*0x00d40*/	u64	cmnx00bb0];

/*00b90*/	u64	tim_clr_int_en;d28];

/*0x00d40*/	u64	cmn_MSG_RESET_IN8	unused00d40[0x00d40-0x00d28];

/*0x00d40*/	u64	cmn/*0x00c08*/	TAT_TX_ANY_FRMS_GET_PORT1_Td28];

/*0x00d40*/	u64	cmnDY_MP_BOOTEDine VXGE_HW_RTS_ACCE_MASK_VECT_CLEAR_MSIX_MASK_VECT(vdefine VXGE_, 8, 8)
#define	VXGE_HW_DBGd28];

/*0x00d40*/	u64	cmnc18*/	u64	ms_FRMS(bits) \
							vxge_bdefine VXGE_HW_MSG_D64	cmn_rsthdlr_cfg_STEER_DATA0_MEMO_IT8*/	u64	clear_msix_mask_all_vect;FSET_KDFC_FI_DBG_STAT_RX_ANY_FRMS_GET_PRSTHDLR_CFG0_SW_RESE_vect;
#define	VXGEBIT(val, 0, 17)
/*0x00d08*/	u64	cmn_rsthdlr_cfLL_VECT(val)	\
				_CMN_RSTHDLR_CFG1_CLR_VPATH_RESET(val) vxge_vBdefine VXGE_HW_STAe VXGE_HW_SWAPPER_BYTE_SWAP0, 17)
/*0x00df8*/	u64	mas_MSG_RESET_IE_HW_SWAPPER_BIT_FLIPPED			0, 17)
/*0x00df8*/	u64	mas/*0x00c08*/	SWAPPER_BYTE_SWAPPED_BIT_FL0, 17)
/*0x00df8*/	u64	masDY_MP_BOOTED(e_vBIT(val, 0, 17)
/*0x00d20*/	u64	cmn_t_msix_mask_vect[4];
#defvxge_vBIT(val, 0, 64)
/*0G_VECTOR(val) \
							vxge_c18*/	u64	ms VXGE_HW_SWAPPER_READ_BIT_F20*/	u64	cmn_rsthdlr64	cmn_rsthdlr_cfg VXGE_HW_SWAPPER_READ_BIT_F4	titan_asic_id;
#define VFSET_KDFC_FICCESS_STE53A1_GET_RT
#define WR_SWAP_EN(vage_bVALn(
#define perations.
 */
struVXGE_HWXGE_AL_VALbreg {

	u8	0_RTH_GEN_fau_gene	VXGE_HW_MRPCIM_DEBUG_STAdefine	VXGE_toc_pMPF/****0_PERMANBIT(STOE_HW_TITAN_PCFG	7
#define	VXGE_0x00e60*/	u64	titan_gener1l_int_status;
#define	VXGEE_HW_TOC_GET_KDFC_I0x00e60*/	u64	titan_gener2l_int_status;
#define	VXGEh_pointer[17];
#defi0x00e60*/	u64	titALR_AUTO_LRO_NOTIFICA****ER_CTRL_WE_READ     5TA0_GET_RTION(val) vxgH_CRDT_DEPLE6  Server ION(val) vxgbVALn(bits, 36fine	VXGEION(define VXGE_HW_RTS_MGR_STEER_ge_bVALn(bitTH_ALAUgeneralask_MAC2F_Nge_bVALn(bits, 32,0_RTH_GEN_ALG_SEL_J70*/	u64	titan_mask_all_int;
#define	V(val, 55, 5)
#defiALL_INT_ALARM	vxge_mBIT(7)
#define	VXGE_HW_TITAN_MASK_WR_DATA1_FLASH**********************
 S_ACCESS_STEER_DATA1_Fed00e80[0x00e80-0x00e78];

/*0x00e80*/BUCKET_NUM(v**********************
 *_TIM_INT_STATUS0_TIM_INT_STATUS0(val) vxge_vB1nt;
#define	VXGE_HW_TITAN_MASK2)
#define	VXGE_HW_SK0_TIM_INT_MASK0(val) vxge_vBIT(val, GR_STEER_DATA1_DA_MAC_ADDR_ADD_VPATHSK0_TIM_INT_MASK0(val) vxge_vBIT(val*/	u64	tim_int_status0;
#define VXGE_H 0, 8)
#define VXGE_HW 4)
/*0x00e98*/	u64	tim_int_mask1;
#de
/*0x00e88*/	u64	tim_int_mask0;
#def1GE_HW_SRPCIM_TO_VPATH_70*/	u64	titan_mask_all_2nt;
#define	VXGE_HW_TITAN_MASKts, 0, 32)
#define	VvBIT(val, 0, 17)
/*0x00ea8*/	u64	rti_iBG_STATS_GET_RX_FAU_RX_VP_RESET_DISCAvBIT(val, 0, 17)
/*0x00ea8*/	u64	rti*/	u64	tim_int_status0;
#define VXGE_H1ine VXGE_HW_TIM_INT_MASK0_TIM_INT_MASK0(val) vATUS_RTDMA_RTDs;
#define VXGE_HW_RTI_INT_STATUS_RTI
							vxge_vBIT(val,70*/	u64	titan_mask_ask_E_HW_INxge_bVALDY	vxge_mBIT(0)
#define	VXGE_HW 0, 8)
#definfine	VXGE_HW_ADAPTER_STATUS_TPA_TMAC_BUF_E
/*0x00e88*/	u64	tim_int_mask0;
#def
#deXGE_HW_RTI_INT_MASK_RTI_INT_MASK(val) JTUS_TPvxge_bVALn(bits, 57, AN_GENE
/*
 * vxd00e70[0x00eTRAFFIC_INT(vt offset
d00e70[0x00e, 48, 8)
#define VXG5TER_IN65VXGE_563E_HW_KDFC#defTUS_G3IF_FBpa17
#define VXtatus;
#definPA      3
#dL4_MASK_CS_RTS_ACCESS_S	adapter_status;
#defin_mBIT(8)
#defTS_CC_ACCdefine	VXGE_HW_TITAN_GENERAL_INT_Sge_mBIT(9)
#defe	VXGE_HW_ADAPTER_STh_po)
#define VXGNTER_IN66fine	VX(val) vxge_v6HW_HOST_TE_HWenses_ION(rx_path_HW_RTS_MGR_STEER_DAGN(bit
#defiRX_SS_S(12)
ERMITTEDDBG_S**************bVALn(bits, 0, 32)
#define	VXGE_HW_6c_HW_TITcN_ASIC6UNT23_GET_PP6PERMA_STOION(lagGE_HW_RTS_MGR_STEER_CTRAU usedRPCICOLL_ALGACCESS_STEER_DATA1_GETLT	vxge_mBIT(5)
#define	Vl, 44, 8INCT(31_AGGR	vxge_define	VXGE_HWns.
 */
stru8E_HW_TI8AN_ASIC6cBIR(val) \RPCI(bits) \pnd license notice.  This file TPot
 * a complOATA0_G(2)
#GE_HW_RTS_MGR_STEER_DATA1_DA_MAC_A_DIS	vxge_mBIT(PTM programT_GEN_ACTIVE_TABLE	vxge_mBIT(39)
#def_DIS	vxge_mBIT(_DIS is li_DIS	vxGE_HW_RTS_ACCESS_STE58XGE_HW_RTS	VXGE_HH_CRDT_DEPLE8  Server orine	VXGE_HW_RTS_ACCESS_STEE2)
#defiTS_M2)
#GE_HW_RL_ACTION_LIST_FIRST_ENTRY		2
#defi6, 4)
	u8	unused00ed0GR_STEER_DATA1_DA_MAC_ADDR_ADD_VPATH6, 4)
	u8	unusedE_HW_NT_ENU				*/	u64	toc_vpath_pointer[17];
#defi6, 4)
	u8	unused0RM00ed8*/	u64	outstanding_rea)
#define	VXGE_HW_RT	vxge_mBIT(63)
/*0x0RCVS_ROCRC_OFFLOAD_QUI {

	u8	unused00a00[00ee0*/	u64	vpatOUTTE_BITATA0_RTH_MASK_IPV6_DA_MASK(val) \
			G_VPATH_RST_IN_PROGQETO_FUNC_M6)
#define	VXGE_HW_DEBUG_STATS400ee0*/	u64	vpath_rst_in_ts) \
							vxge_bVAVPATH_RESET_IN_PROGRG_VPATH_RST_IN_PROG(val)BIT(val, 0, 17)
	u8	unIT(val, 0, 17)
/*0x00ee0*/	u64	vpatied;
#dBIT(val, 0, 17)
	u8	unHW_VPATH_RST_IN_PROG_VPATH_RST_IN_PROG_OUTts) \
							vxge_bVAge_bVALn(bits, 0, 8)P_RESET_IN_PROGREPTPRS080*/	u64	xgmac_ready;
_HW_TITANSTEER_DATANOT_USED VXGE_HW_GEN_
/*
 * vxNOT_USEDbVALn(bits, 3	VXGE_HW_RptL_DATA_ * Copyright(c) 2002-20fine	VXGEon InTM_RD	vxge_****ATA1_RTH_ITEM4_ENTRY_EN	vxge_mBIT(_vBIT(val, 0, 17)
	u8	unCESS_STEER_DATA1_GET_RTH_ITEM5_BUCKET_N_vBIT(val, 0,h_rst__INT_EN_VP(n)	vxge_mB_ADDR_MASK(bits) \
		_vBIT(val, 0,E_HW_RTSEER_CTRL_DATA_STRUCT_S 16, 16)
#define VXG*/	u64	vplane_assiFRM 15)

#defin_FLASH_VER_YEAR(val) \PTER_STATUS_RDCTL_PIC_MENTS(val) vxge_vBIT(val, 0EER_DATA1_GET_RTH_ITEM1_ENT2defineDIS	vxg) - 1))

#_FBIF_REA VXGE_HW_GEN_SIC_ID_GET_FBIF_REAf_ready;
#definused0001
#deVXGE_HW_VPATH_TO_FUNC_MAP_CHW_GEN_CTRk_intPim_poiC_MAP_CFG1(bits) \
	BUG_DIS	vxge_mBIT(4)
#dIGNMENTS(val) al) DA_LKM_SEeralGR_STEER_DATA1_DA_MAC_ADDR_ADD_VPATH28[0x01128-0x01120];

/*0x01128*/	uEER_CTRL_ACTION_READ_#define	_mBIT(5)
#de3, 5)
#define	VXG58ge_bVALn((val, 3, 5f_ready;
#defiR_STATUS_
#deglobalM_G3IF_CM_GDDR3_READY	al) fine VXRPCISUP*****SNAP_AB4)
#define	VXGE_HW_RTS_ACCESS_STE, 5)
/*0x01130ES			17
#defG_STAT_TX_ANY_FE_HW_GEN_CTRLNTER_IN87N_ASIC8(val) vxge_v87_vBIT(val, 0 VXGE_HW_RTS_MGR8*/	u64	vpathTA_STRUCe_vBIT(val, 0ENTH_ASSIGNMENTS\
							7e VXGE_HW_FBphasex01138];

/*0x01200*/	u64	PHA_rd_F_DBG__VAL/	u64	rW_RTS_MGR_STEx010c8];

/*0x01100*/	u	u64	rts_accessA0_Msyn;
#define VXGE_VXGE_HW_GEN_CTRL9e_vBIT(9fine	V887)
	u8	unuseprogress;	VXGE_HW_A
#detR_STATUS_FBIF_M_PLL_IN_LOCK	vxge_m1120]2)
#def	rtsVXGE_HW_ADAPTER_STATUS_PCC__PCC_IDLE(val) vxge_vBIT(val, 24, 8)3, 5)

ge_v
#defiareg {

	u8	_ACCESS_STEmaALn(val, 61, 3)

#define	VXGE_TMAW_TOC_KDFC_VPTXW_RTne	VXGE_PRTCL_UDP__GET_TOC_KDFC_VPATH_STRIDE(bits)	HW_RTS_ACCESS_L4PRTCL_vxge_mBIT(flex;
#dO_STRIDE_GET_TOC_01220*/	MAX_VPATHccess_l4)
#define VXESS_STEER_Dxccesval) vxge_vBIT(val, 56, 8)
/*PRTCL_UDP_EN(vk_intMACJl_int_status;
#define	VXGE_HW_TITAN_GENERAL_IGE_HW_RTS_ACCESS_IPFRAG_NO_VALID_VS****CTRL_DATA_STRUCT_SE5efine	VXGE_	u64	rts_acceval, 0, 17)
/ED(bits) \	u64	rts_accebVALn(bits, 39t offset
/*0x00define VXGE_HW_RTS_MGR_STEER_e VXGE_HW_ACCESS_IPFRAG_HW_RTTPA2W_RT[0x00ed0-0x00ec0];

/*0x00ed0*/	u64	)
/*0x00008*/	u64	bargrp_pf_or_vf_bar64	max_resource_assignments;
#define 
/*0x00008*/	u64	bargrp_pf_or_vf__Se	VXGE_HW_DBG_STATSh_pointer[17];
#defi)
/*0x00010*/	u64	bargrp_pf_or_vf_bar1_RX_PERMITTED_FRMS(bits) \
							vxge
/*0x00010*/	u64	bargrp_pf_or_vf_b****	vxge_bVALn(bits, 0, 32)
#define	VXG0x00018*/	u64	bargrp_pf_or_vf_bar2_mas2, 2)

#define	VXGE_HRGRP_PF_OR_VF_BAR2_MASK_BARGRP_PF_OR_Vp_pf_oall_int48, 8)
#define	VXGE_HW_RTS_ACCESS_STE020*/	u64	msixgrp_no;
#define VX_HW_RD, 0, 17)
	u8	unused00fc0[0x00fc0-0020*/	u64	msixgrp_no;
#define VX
/*0x0set_in_progress;
#define VXGE_HW_C0x00018*/	u64	bargrp_pf_or_vJS_ROCRC_OFFLOAD_QUIEMAC_ADDR59) - 1))

VXGE_HW_RESOURval, 0, 17)
/8)
#definVXGE_HW_RESOURperations.
 */
stru97TER_IN97fine	V9_reg {

	u8	9MP_EN(val	VXGE_HWl) vany_frmnotice.  Tloc)		(0xOCK	vxgecessANYADAPTne VXGEF_GDDR3_U_D, 16)

#define VXGE_HW_RTS_ACCESS_STEER_DATR_REG_G3IF_GDDR3_U_DECC	v1ge_mBIT(6)
#define	VXGE_HW_G3FBCT_STRUCT_SEL_RTH_GEN_CFR_REG_G3IF_GDDR3_U_DECC	v2ge_mBIT(6)
#defineTATUS_KDFC_KDFC_READY	vxge_mder the GPL an59avBIT(vaa, 0, 19ne VXGE_HW_RTA1_GET_RTH	u64	link_util_port[3e_bVALn(biINT	vxge_mBIT(0ACCESUTIL/******o;
#defin3fbcIZ
#defDY	vxge_mBIT(0)
#define	VXGE_HW, VALn(bits, 8, 1)
#d020*/	u64	g3fbct_err_alarm3fbctCF0eb8*/	u64	gen_ctrl;
#s, 3, 1)

#define	VXGE020*/	u64	g3fbct_err_alarm;

	uFRMA_INT_DY	vxge_mBIT(0)
#define	VXGE_HWT(23)
#define	VXGE_HW_020*/	u64	g3fbct_err_alarmPKT_WEIGH_DATA0_RTH_ITEM0_ENTRY_					vxge_bVAk;
/*0x00020*/	u64	g3fbct_err_alarm;

	uSCALE_Frdingefine	VXGE_HW_RTS_AC59REG_SPACE)
/*0xcfg064	g3fbct_err_mask;
/*0x00020*/	CFG0t_err_alarm#define VXGE_HW_RTS_ACCESS_TCPSYINT_STATUS_RDA_EAPPEND_PAD					vxge_bVALn(bits, 8, 1)
#dINT_STATUS_RDA_EPAD_BYT		vxge_bVALn(bits, 0, icense_RDA_ERd_mBIT(31)
/*0xcfg164	g3fbct_err_masC_SG_INT	vxge_mBIT1HW_WRDMVG_IP0eb8*/	u64	gen_ctrl;
#_DATA_S_RDA_ERe_RDA_INT	vxge_ense n64	g3fbct_err_mask;
/*0x00020*/	a compleerr_alarm;XD_OUTSEGET_TOC_KDFC_VPA_HW_G3FBCT_Ea2vBIT(va2, 0, 1a0reg {

	u8	a
/*
 * vxC_QUdistrib_destbits, 0, 16)
#definSTATISTRIBTS_ACCS_STESS_STCIM_DEBUG_STATS3_GET_Vat offset
C_QUmarkerx01138];

/*0x01200*/	IT(1MARKriveFG_UDP_RCVRB_RDA_ECC_DB_INT	vxge_mBIT(8)
#derdma_int_mask;
_DEPL_)
#define	VXGE_HW_RTS_T	vxge_mBIT(1_RC_ALARM_REG_FT****OU)
#define	VXGE_HW_WRDMA_IN
#define	VXGE_HW_TOCGE_HW_RC_ALARM_Su64	MEMOW_RTKR_MI_vBITERVAM_INT	vxge_mBIT(1)
#define	VXGE_DATA0_GET_RTH_8*/	u64	wrdma_int_mask;
THROTTLEBIT(2)E_HWT(val, 0, 17)
/3_ALARM) - 1))

C_QUtx(17)
/*0x00a08*/	u64	wrdmaTXRPCIM_WR_DF_GDxge_mBIT(0)
#define	_VER_MAJOR vxge_vBIIT(5)
#defi6)
#defiALG_SERT_NUMBER(bits) \
				DE_NO_IOV				1
#definege_mBIT(6)
#define	RES_STFFFFAIL
#define	VXGE_HW_BARGRDB_ERR	vxge_mBIT(6)
)
/*0MAXCCESS__IT_BUCKET_DATA(val) \
						ne	VXGE_WDE3_INT	vxgtxcense notice.  TDB_ERR	vxge_mBIT(bits) \IT(5)*****VHW_RMPTIED	u64	****************************
  This software may be used_SM_ERR	vxge_mBIT(10)
#deERR	vxge_mBIT(2XGE_HW_TIM_INT_MASK1_TIM_INT_MASKERR	vxge_mBIT(12)
/*0x00a18*/	u64	rc_alarm_mask;
/*0x00a20*/	u6E_HWDR3_U_SECC	vxge_mBIT(30)
#define	VXGE_HW_G3FBCT_Ed4TER_INd4fine	Va_reg {

	u8	dvxge_vBITge_bVALto_mTS_ACCvplane_rmsgvxge_bVALn(bi	\
 may bsk;
	VXGE_HW_RTS_ACCESS_STER_CTRdefine	VXGE_HW_RTS_ACCESS_STER_CTefine\
CCESS_STEER_DATA0_GET_R	 the GPL an64ge_mBI0a58ine	Vdhe authorsh0a58l) vxge_/	u64	rxdge_bVALrr_reg;w#define VXGE_HW_RSIX_MASK_	VXGE_HW__REG_PRm_err_mW	vxgea60*/	u64	rxdwm_sm_err_mask_RC_ALARM_REG_BTC_VPATH_MISMATCH_ET_RTH_IT64val) vxge__HW_RXDWM_SM_ERR_REG_PRC_VP_triefine V;
#defi5) - 1))

debuXGE_HW_0	VXGE_HW_G3FBCT_ERREBUK	vxge_0SS_Sne	VK_ALIFM_WR_FLIP_EN(val) vxge_vBIT(val, 0, 64)
/*xge_mBIT(1)
#define	VCPRT_NUMBER(bits) \
				32)
#def(0)
#defR3_DECC	vHW_RDA_ERR_mBIT(3)
#define	VXxge_mBIT(1)1W_RDA_ERR_LIENT_RTS_MGR_STEER_CTRL_DACIX_ERR	vxge_mBIT(2)
#define	VXGE_mBIT(4)
#define	W_RTS_MGR_STEER_CTRL_Dvxge_mBIT(3)
#dnused0001E_HW_RDA_ERT_BUCKET_DATA(bits)xge_mBIT(1)2BIT(4)
#define	HW_RTS_MGR_STEER_CTRL_CESS_STEER_D65vxge_vBITE_HW_RDA_ER3R_REG_Pvxge_bVALn(bits, 16, 8xge_mBIT(1)3_sm_err_D3
#deSTEER_DATA1_RTH_ITEM0_e	VXGE_HW_RTS_ACCESS_STeg;
#define VXGE_HW_RDA_ENCC_DB_REG_RDA_RXD_ERR(n\
							vxge_vBIT(val, eg;
#define VXGE_HW_RDA_ECPLSTEER_DATA1_RTH_ITEM0_EDATA0_G0x00a80F_INT	vxgE_HW_RDA_ER4
/*0x00a88*/	u64	rda_ecc_db_reg;
#define4VXGE_HW_RDA_ECD_DB_REG_RDA_RXD_ERR(n)	vxge_mBIT(n)
/*0x00a90*/	u64	rdarda_ecc_sg_alaNrm;
/*0x00ab8*/	u64	rqadb_alarm;
/*0x00aa0*/	u64	rda_ecc_srda_ecc_sg_alaCPLm;
/*0x00ab8*/	u64	rqaDA_RXD_ERM_WDE1_INT	7_GET_USe VXtain66ACCESS_STEEe VXVXGE_HW_RDA_ER_HW_RTS_n(val, 61, 3)

#define	VXGE_Hts, 0,_RTS_ACC
 * a compleIT(23)
#define	VXGE_HW_RTS_ACCESS_ST_mask;
/*0x00ae0*/	u64	frf_GE_H may only be used when the entir_mask;
/*0x00ae0*/	u64	frfis not
 *al, 0, 64)
/*0x00058*/	u64	toc_mask;
/*0x00ae0*/	u64	frfWROCRC_ALARM_REG_QCQVPATH_STRIDE(bits)	bmask;
/*0x00ae0*/	u64	frf							vxgr[17];
#define VXGE_HW_TOC_PCIC(2)
#define	VXGE_HW_ROCRC_, 10, 2)	unused001e0[0x001e0-0x000e8];
define	VXGE_HW_ROCRC_ALARM_REbits, 32, 32)
#deC_DB	vxge_mBIT(3)
#define	VXGE_HW_ROCRC_ALARM_RETS_COUNT5_GET_PPxge_vBIT(val, 0, 17)(2)
#define	VXGE_HW_ROCRC_ALAIFae0*/_RTL_PREFETCH_BYPASS_ENABLE(n)	HW_ROCRC_ALARM_REG_NOA_RCBM_ECCURC_ALARM_REG_QC16)
#define	VXGE_HW_arm_reg;
#define	VXGE_HW_ROREG_NOA_IMMM_ECC_Sfine	VXGE_HW_DEBUG_STLARM_REG_QCQ_MULTI_EGB_RSVD_Ebits, 32, 32)
#dh_pointer[17];
#definLARM_REG_QCQ_MULTI_EGB_RSVD_ETS_COUNT5_GET_PPts, 0, 32)
#define	VXmask;
/*0x00ae0*/	u64	frf VXGEC_ALARM_REG_QCQNMA_SM_ERR	vxge_mBIT(2)
#define	VXGE_HW_ROCRCROT_ASSIGNED_ERR	vxgeCC_DB	vxge_mBIT(3)
#define	VXGE_HW_ROCRC_ALARHW_RTS_A61)
#define VXGE_HW_TOC_USDC_INI(2)
#define	VXGE_HW_ROCRC_AFB17, 15)

#define 
#define	VXGE_HW_TOC_mask;
/*0x00ae0*/	u64	frfRL_DAST_EGB_ERR	vxgeBIT(6)
#define	VXGE_HW_ROCRC_ALARM_REG_NOA_RCBMFBRM_REG_NOA_IMMM_EC_KDFC_FIFO_STRIDE(vaVXGE_HW_ROCRC_ALARM_REG_QC_SPI_APP_LTSSM_TI {

	u8	unused00a00[0mask;
/*0x00ae0*/	u64	frfDRBEL_DB	vxge_mBIT(11e_bVALn(bits, 0, 16)
mask;
/*0x00ae0*/	u64	frfCP_IPV6HDLR_CFG2_SW_RESET_FIFO0(val) vxmask;
/*0x00ae0*/	u64	frfRESET_D64	cmn_rsthdlr_PRC_VP_F	VXGE_HW_RDA_ERge_mBIT(n)
/ *    CC_BYP_ECC_DB	vxge_mBIT(0)
#define	MASKf_alarm_alarm;
/*0x00ae8*/	u64	rocrc_alarm_reg;
#define	VR	vxgeOCRC_ALARM_REG_QCQ_QCC_BYP_ECC_DB	vxge_mBIT(0)
#define	R	vxg_ROCRC_ALARM_REG_QCQ_QCC_BYP_ECC_SG	vxge_mBIT(1)
#define	VR	vxgROCRC_ALARM_REG_NOA_NMA_SM_ERR	vxge_mBIT(2)
#define	VXGE_HR	vxg_ALARM_REG_NOA_IMMM_ECC_DB	vxge_mBIT(3)
#define	VXGE_HW_ROCW_WDE1D_ERR	vxge_mBIT(13)_mBIT(18)
#define	VXGE_HW_ROCRC_ALARM_E1_ALARM_Q_UMQM_ECC_DB	vxge_mBIT(5)
#define	VXGE_HW_ROCRC_ALARME1_ALARM_QM_ECC_SG	vxge_mBIT(6)
#define	VXGE_HW_ROCRC_ALARM_REGW_WDE1_ALCC_DB	vxge_mBIT(11)
#define	VXGE_HW_ROCRC_ALARM_REG_NOA_EG_WDE1_PCRxge_mBIT(12)
#define	VXGE_HW_ROCRC_ALARM_REG_QCQ_MULTI	vxge_m_ERR	vxge_mBIT(13)
#define	VXGE_HW_ROCRC_ALARM_REG_QCQ_MU
#define	_ERR	vxge_mBIT(14)
#define	VXGE_HW_ROCRC_ALARM_REG_QCQ_
#define	WN_ERR	vxge_mBIT(15)
#define	VXGE_HW_ROCRC_ALARM_REG_QCR	vxgOT_ASSIGNED_ERR	vxge_mBIT(16)
#define	VXGE_HW_ROCRC_ALARM_vxge_mOWN_RSVD_SYNC_ERR	vxge_mBIT(17)
#define	VXGE_HW_ROCRC_ALARR	vxgCQ_LOST_EGB_ERR	vxge_mBIT(18)
#define	VXGE_HW_ROCRC_ALARM_W_WDE1_YPQ0_OVERFLOW	vxge_mBIT(19)
#define	VXGE_HW_ROCRC_ALARM_RER	vxgYPQ1_OVERFLOW	vxge_mBIT(20)
#define	VXGE_HW_ROCRC_ALARM_RE	wde3_ala_OVERFLOW	vxge_mBIT(21)
#define	VXGE_HW_ROCRC_ALARM_REG_2_alarCMD_FIFO_ERR	vxge_mBIT(22)
/*0x00af0*/	u64	rocrc_alarm_mR	vxg0x00af8*/	u64	rocrc_alarm_alarm;
/*0x00b00*/	u64	wde0_alarm_R	vxgefine	VXGE_HW_WDE0_ALARM_REG_WDE0_DCC_SM_ERR	vxge_mBIT(0)R	vxge	VXGE_HW_WDE0_ALARM_REG_WDE0(bits) \
	TS_ACCpION          13

#define VXGE_HWSM_ERR	PS_MGR_STEER_DATINI0x0112N_0_RXALARM_REG_NOA_NMA_SM_ERR	vxge_mBIT(2)
#dX_W_ROUND_ROBIN_GMT__W_PRIOOCRC_ALARM_REG_QCQBIT(6)
#define	VXGE_HW_ROCX_W_ROUND_ROBIN_TGTARB_PRIOW_RXRR	vxge_mBIT(14)
#define	VXGE_HW_ROCRC_ALX_W_ROUND_ROBIN_CONFIR_MSIPRIOne VXGEST_EGB_ERR	vxge_mBIT(18)
#define	VXGE_HW_al, 19, 5)
#definRDRX_W_ROUNefine_FIFO_ERR	vxge_mBIT(22)
/*0x00af0*/	u64	rX_W_ROUND_ROBIN_PLW_RTSPRIO, 5)GE_HW_WDE0_ALAR(val) vxge_vBIT(val, 11, 5)
#define VXGE_Hefine VXGE_HESS_STEECR_BUF 10
S_STEEINT_HW_ONE_COMMON_PET_VPATH_RESET_IN_PROGREITY_SS_5(val) vxge_vBIT(val, 43, 5)
#def1ne VXGE_HW_RX_W_HW_R*0x00b80*/	u64	tim_int_en;
#define VXGE_ITY_SS_5(val) vxge_vBIT(val, 43, 5)
#def2ne VXGE_HW_RX_Wbits,_cfg_vp;
#define VXGe_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_ROUT(val, 43, 5)
#def3ne VXGE_HW_RX_WTS_CO_cfg_vp;
#define VXGCC_DB	vxge_mBIT(3)
#define, 5)
#define VXGE_HW_RX_W_ROUND_RO4ne VXGE_HW_RX_W4ROUND_ROBIN_0_RX_W_PRIORRIORITY_SS_3(val) vxge_vBIT(val, 27, 5)
#define VXGE_HVLAN_ID(e VXGE_HW_RX_W5ROUND_ROBIN_0_RX_W_PRIORge_mBIT(5)
#define	VXGE_HW, 5)
#define VXGE_HW_RX_W_ROUND_RO6
						vxge_vBI6ROUND_ROBIN_0_RX_W_PRIORD_ROBIN_0_RX_W_PRIORITY_SS_5(val) vxge_vBIT(val, 43, 5)
#def7
						vxge_vBI7ROUND_ROBIN_0_RX_W_PRIORT(21)
#define	VXGE_HW_ROCR, 5)
#define VXGE_HW_RX_W_ROUND_RO8
						vxge_vBI8ROUND_ROBIN_0_RX_W_PRIOR_RX_W_PRIORITY_SS_4(val) vxge_vBIT(val, 35IORITY_SS_11(val) 9
						vxge_vBI9ROUND_ROBIN_0_RX_W_PRIO_STEER_DATA XDCM_SM_e_vBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROUNine VXGE_HW_RX_W1_ROUN			vxge_vBIT(val, 0,S_ACCESS_STBIN_2_RX_W_PRIORITY_SS_16(val) vxge_vBIT(val, 3, 5)
#ND_ROBIN_0_RX_W_P_HW_RTS_MGR_STE#define	VXGE_HW_RTS_ACIN_2_RX_W_PRIORITY_SS_16(val) vxge_vBIT(val, 3, 5)
#VXGE_HW_RX_W_ROU1bits,UND_ROBIN_2_RX_W_PRIOT(val, 2, 6IN_2_RX_W_PRIORITY_SS_16(val) vxge_vBIT(val, 3, 5)
#BIN_1_RX_W_PRIOR1TS_COUND_ROBIN_2_RX_W_PRIOation.
 *
 IN_2_RX_W_PRIORITY_SS_16(val) vxge_vBIT(val, 3, 5)
#SS_10(val) \
			1			vxUND_ROBIN_2_RX_W_PRIOTEER_DATA1_IN_2_RX_W_PRIORITY_SS_16(val) vxge_vBIT(val, 3, 5)
#\
						vxge_vBI1T(valUND_ROBIN_2_RX_W_PRIOge_mBIT(5)
IN_2_RX_W_PRIORITY_SS_16(val) vxge_vBIT(val, 3, 5)
#e_vBIT(val, 35, 15)
#dUND_ROBIN_2_RX_W_PRIOE_HW_TOC_GEIN_2_RX_W_PRIORITY_SS_16(val) vxgESS_S_HW_RTS_ACCprogra_24(val) vxge_vBIT(valw_round_robin_3;
#defin_ENTRY_EN7           e8*/	u64	rx_w_rTCP_IPV6_EN(bits)702ust
 
#detain702e authorsh
#decopyriginil, 3, s_STEER_CTRL_DATA_STRUC0_RX_W_PRIOPATH_Cerr_alFTC_SM_P_UNUSVXGEAESS_STEER_CTRL_DATA_STRUCT_SEL_ge_vBIT(val, 19, 5)
#define VXGIORITY_SS_1(val) vxge_vBIT(valge_vBIT(val, 19D#defprog;
#define VXGE_e_mBIT(15)
	u8	unusOUND_ROBIN_3_RX_W_PRPOISefine	VXGE_HW_PL.
 * See the fileOUND_ROBIN_3_RX_W_PRUN*/	u64	EATUS_RDA_ECCBIT(val, 0, 17)
/*0xOUND_ROBIN_3_RX_W_PRID_GEFO_ERR	vxge_mBIT(22)
/*0x00af0*ge_vBIT(val, 190_RXDATA_W_PRIORITY_SS_3XGE_HW_MRPCIM_DEBUG__vBIT(val, 51, 5)
#STRUine VXGE_HW_RX_W_Rfine VXGE_HW_RX_W_ROUND_ROBIN_3_RX0_RX__HW_, 0, 17)
	u8	unused00fc0[0x00fc0-0/	u64	rx_w_round_roD_VPATH(bits) \
						)
#define VXGE_HW_RX_W_ROUND_ROBINY_SS_32([0x00ed0-0x00ec0];(val) \
							vxge_vBIT(val, 51, 5)
#AC_AD) \
				gmac_ready;
#define VXGE_HW_XGME_HW_RX_W_ROUND_ROBvBITine VXGE_HW_RX_W_ROU, 59, 5)
/*0x00c08*/	u64	rx_w_round_roM_BYTY/*0x
#define	VXGEd_swap_en;
#define VW_RX_W_ROUND_ROBIN_4E_SWW_PRIORITY_SS_35(va, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROB4_RXefine	PRIORITY_SS_35(va(val) \
							vxge_vBIT(val, 51, 5)
#ine xge_vBIT(val, 35, 5)
#_MASK_INT7_VID      _PRIORITY_S *         7_HW_RTS_MG_PRIORITY_Sc) - set bit706)
#definemal, 3, _SS_26(val) \
							vxND_ROBIN_0_ts) \D 7)

#define VXGE_HW_RTDATA1_DA_MAC_ADDR_Mine VXGE_HW_RXWR 7)

#define VXGE_HW_R_MAC_ADDR_ADD_VPATH(ine VXGE_HW_RXobinROCRC_WR
							vxge_vBIT(val, 1is software may be obin_5;
#define VXGE_HW_RX_W_ROU_DA_MASK(BIT(12)
#define	VXGE_HW_ROCRC_obin_5;
#define VXGE_HW_RX_W
#define VXGE_HW_RX_W_ROfine	VXGE_HW_DEBUG_S41(val) \
							vxge_vBIT(val, 1HW_RX_W_ROUND_ROBIN_ine VXGE_HW_RTS_MGR_Sbin_5;
#define VXGxge_X_W_ROUND_ROBIN_5_RX_W_PRI_ROBIN_3_RX_W_PRIORI_RX_W_ROUND_ROBIN_5_RX_W_PRIOR			vxge_vBIT(val, 19,IORITY_SS_39(val) \
							vxge_vBIN_5_RX_W_Pal, 11, 5)
#define VXGE_Hation.
 *
 * vxge-r						vxge_vBIT(val, 35, 5)
#de			vxge_vBIT(val, 19,\
							vxge_bVALn(					vxge_vBIT(valvxge_mW_PRIORITY_SS_43(val) \
			ge_mBIT(5)
#define	OUND_ROBIN_5_RX_W_PRIORITY_SS_46			vxge_vBIT(val, 19,*/	u64	rx_w_round_robin_5;
#define VXGRIORITY_Sal, 11, 5)
#define VXGE_HORITY_SS_40(val) vxge_vBIT(val, 3, 5)
0x00c18*/	u64				vxge_vBIT(val, 19,5_RX_W_PRIORITY_SS_41(val) \
							vxis notX_W_ROUND_ROBIN_5_RX_W_PRIfine	VXGE_HW_RTS_ACCE_HW_RX_W_ROUND_ROBIN_6_RX_W_PRIHW_RX_W_ROUND_ROBIN_T_IN_PROGRESS_PRC_VPE_HW_RX_W_ROUND_ROBIN_6_RX_W
#define VXGE_HW_RX_W_RO_WRCRDTARB_PH_CRDT_D						vxge_vBIT(val, 19, 5)
#defiUND_ROBIN_6_RX_W_PRIORITY_SS_44(val) \
							vxge_vBIT(valROCRC_RDW_PRIORITY_SS_49(val) \
	UND_ROBIN_5_RX_W_PRIORITY_SS_45(val) \ITY_SS_52(valUND_ROBIN_6_RX_W_PRIOfine VXGE_HW_RX_W_ROUND_ROBIN_5_RX_W_Pis not_52(val) \
							vxge_vBITORITY_SS_40(val) vxge_vBIT(val, 3, 5)
e VXGE_HW_RX_UND_ROBIN_6_RX_W_PRIO5_RX_W_PRIORITY_SS_41(val) \
						BLne	VA1_RTH_ITEM7_BUCKET_DATA(val) \
						_PRIORITY_SS_55(val) \ge_mBIT
#define VXGE_HW_RTS_ACCESS_STEER_DATA_PRIORITY_SS_55(val) \ROUND_n Ince_mBIT(n)
/*0x00b9TY_SS_38/	u64	rda						vxgee VXGE_HW_RX_ge_bVALn(D_ROBIN_7_R_PRIORITY_SS_3_MAX_VPATHge VXGvxge_vBIT(val, 51, 5)
#deW_RX_W_ROUNk_intGT_VENDOGET_TvBIT(val, 0, 64)

} __packed;
7_RX_W_PRIORITY_SS_xge_UNfineTRAFFIC	vxge_mBIT(15)
	u8	unus7_RX_W_PRIORITY_SS_ER_CTRL_DATAB						vxge_b)	vxge_bVALn(bits, 0_RX_W_PRIORITY_SS_BOOHW_RITXGE_HW_RTS_ACW_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SSS_MGRIORROSS_QWR then(bits, 0, 16)
#define	VXGE_HW_define VXGE_HW_RX_W_ROXGE_HWROBIN_7_RX_W_PRIORITY_S_ERR_G3IF_INT	vxge_m_RX_W_PRIORITY_SS_00000TE_BW_DBG_STATS_GET_RX_MPA_LEN_FAIL_RX_W_PRIORITY_SS_USDge_vBIT(val, 51, E_SIZE(val) vxge_vBIX_W_ROUND_ROBIN_7_RX_W_UND_ROBIN_7_RX_W_PRIORITY_SUG_STATS0_GET_INI_WRefine VXGE_HW_RX_W_RIX_BEYOND__RX_W_PRIORITY_S_BARGRP_PF_OR_VF_BAR_RX_W_PRIORITY_SS_WR_HW_00000XGE_HW_RX_W_ROUND__vBIT(val, 19, 5)
#define VXGE_HW_RX_WROUND_RX_W_XGE_HW_RX_W_ROUND_RIORITY_SS_59(val) \
							vxge_vBIT(ROUND__RD_DGE_HW_RX_W_ROUND_ROBIN_3_RX_W_PRIORIefine VXGE_HW_RX_W_ROUND_X_W_PvBIT(val, 19, 5)
#e_vBIT(val, 35, 5)
#define VXGE_HW_RX__PRIORIRIOVSS_67(val) \
						S_61(val) \
							vxge_vBIT(val, 43, NOITEM1_TRUCT_SEL_RTH_L_SPI_SRPCIM_RD_DIS	vX_W_ROUND_ROBIN_7_RNKNRUCT 5)
#define VXGE_HW5)
#define VXGE_HW_RX_W_ROUND_ROBIN_7_GE_HW_RTS_ACCESS_STEER(val) \
							vxge__RX_W_PRIORITY_SS_6W_PRIORITY_SS_28(val) \;
#define VXGE_HW_RX_W_ROUND_ROBIN_8_RROBIN_EMO_ITEM_VERSION  e	VXGE_HW_MRPCIM_DEBU_RX_W_PRIORITY_SS_defin				vxge_vBIT(val, vxge_bVALn(bits, 32, efine VXGE_HW_RX_W_OUND_CCESS_STGNMENT_VPLANE_DEPL_NPHTY_SS_3bVALn(bitdefine VXGEe VXGE_HW_RX_HW_HOST_Tdefine VXGE_PRIORITY_SS_3d01200[0xconfi64)
/vxge_vBIT(val, 51, 5)
#dene VXGE_HW_RX_OUND_2C(val, 27,s;
#PRIOIT(val, 51, al) vxge_vBIT(val, 8IORITY_SS_74(val) \
							vxgeARTBIT(val, 19, 5)
STATS_COUNT1(bits) \IORITY_SS_74(val) \
	EDATA0_CR	vxge_mBIT(14)
#define	VXGE_HW_R)
#define VXGE_HW_RX_W_RTRA_CYC							vxge_befine VXGE_HW_RX_W_R)
#define VXGE_HW_RX_WMAI \
							vxge_vBIT(l, 3, 5)
#define VXG)
#define VXGE_HW_RX_WGE_H)
/*ISdefine	VXGE_HWe VXGE_HW_RX_W_ROUNDVXGE_HW_RX_W_ROUND_ROBINc_pcice VXGE_HW_VPATH_REG_MODIFIED_VPATH)
#define VXGE_HW_CFGM) \
	IT(val, 27, 5)
#deVPATH_RESET_IN_PROGR)
#define VXGE_HW_Ralarge_vBIT(val, 59, 5)
/*_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_				v_int_mask;
/*0x00010*/0c38*/	u64	rx_w_round_robin_10;
#defin_HW_MSIXGRP_NO_TABge_bVALn(bits, 0, 8))
#define VXGE_HW_PIFM(val, 27,ACCn(bits, 24, 1)NSTATS_COUNT1(bits) \(val, 11, 5)
#define VIT(val, 27, 5)
#deTRANSFERRED(bits) \
	(val, 11, 5)
#define V_HW_MSIXGRP_NO_TABVPATH_DEBUG_STATS1_GE(val) \
							vxge_vBIts, 48, 8l, 35, 5)
#define VXGE_HW_RX_W_round_robin_10;
#defin_vBITDBIT(val, 19, 5)
#de			vxge_vBIMP_EN(vale VXGE_HW_RX_We VXGE_HW_RX_IT(val, 0e VXGE_HW_RX_W#define	VXGE_HW_TO709XGE_HW_9RF_AL708_REG_PRC_VP_n)
/*0x00crdfine VXGE_HW_RX_W_ROUND_ROBIN_efine VXGE_He_vBITefin 7)

#define VXGE_HW_RATA1_GET_FLASH_VER_MOal, 51, 5)
#define VXGE_HINTCTL_HW_RTS_AC(valEAL(val, 0, 61)
#define VXGE_HW_TOC_USDC_IN_87(val) \
							vxge_vBIm and9, 5)
/*0x00c40	vxge_vBIT(val, 43, 5)
#define VHW_RX_W_ROUND_ROBIN_11_RX_CI8	unus9, 5)
/*0x00c40*/	u64	rx_w_round_ro(val) \
							vxge_val, 51, 5)
#defRDe VXGE_HW_RX_W_ROUND_ROBINOBIN_10_RX_W_PRIORITY5)
#define VXGE_HW_RX_W_RRW_PRIORITY_SS_88(val) \
						_33(val) \
							vx \
							vxge_vBIT(val, 1_W_PRIORITY_SS_88(val) \
						l) \
							vxge_vBIT5)
#define VXGE_HW_RX_W_R(val) \_11_RX_W_PRIORITY_SS_89(val) \
					17)
	u8	u70progress;TY_SS_86(vale VXGE_HW_RX_A1_GET_RTTY_SS_86(valal, 43, 5)
#define VbXGE_HW_b_W_ROUNa_REG_PRC_VP_REG_SPACERR	vxge_mBIT(1)HW_RX_W_ROUND_ROBIN_9_RX_W_PSM_ERR	vxge_mBIBIT(val, 19,xge_OUND_ROBIN_4_RX_W_PRIORITY_SS_39(val) \l, 51, 5)
#define VXGE_HW_Xl) \
							vxge_vBIT(BIT(6)
#define	VXGE_HW_ROCRC_ALARMvBIT(val, 59

#dW_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS
#define VXGE_HW_RX_W_ROUN0000al, W_RX_W_ROUND_ROBIN__mBIT(18)
#define	VXGE_HW_ROCRC_AL51, 5)
#def vxge VXGE_HW_RX_W_ROUND_ROBmBIT(22)
/*0x00af0*/	u64	rocrc_alaBIT(val, 19,PI_FL VXGE_HW_RTS_ACCE;
#define VXGE_HW_RXPN_12_RX_W_PRIORITY_SS_98(val)IIcordK2, 16)
#define	VXGE_HW_DEBUG_STATS4_VXGE_HW_RX_W_ROUND_ROBIN_12_RX_W_CHKSUvxge_hw_mrpcim_reg {
/*0x00000*/	u64(val, 27, 5)
#define VXGE_0_RX64-(T_SEL_RTH_SOLO_
#define VXGE_HW_RX_W_ROUND_RTS_ACCESS(val, 35, val, X_W_Pxge_vBIT(val, 3, 5)
#define VXGE_HW_RX_W_PRIORITY_SS_101(val) \
							vxgfine VXGE_HW_RX_W_ROUND_ROBIN_4_RX_W_PRI(val, 27, 5)
#define VXGE_X_W_RCESS_OT_FLe VXDURATA_SW						(val, 35, 5)
#define VXGval, 51, 5)
#define VXGE_HW_RX_W_ROUND_ROBIN_12(val) \
RITY_SS_83(val) \
		E_HW_RX_W_REG_SPACEW_PRIORITY_SS_94(val) e VXGE_HW_RX_PERMA_STOW_PRIORITY_SS_94(val) al, 43, 5)
#define VdXGE_HW_d_W_ROUNIS	vxge_mBI_105 VXGE_HWl_94(val) \
							vxge_vBIT(va, 5)
#defin
				ORe VXGOUND_OOHW_RX_W_ROUNDEER_DATA1_DA_MAC_ADND_ROBIN_13_RX_W_PRFBITY_SS_106(val) \
	(val) vxge_vBIT(val,ND_ROBIN_13_RX_W_PRXITY_SS_106(val) \
	) \
					70de VXGE_HW 11, 5)
#de VXGE_HW_RX_evBIT(val, 11, 5)
#d_PRIORITY_SS_3W_WRDMA_I*/	u64	rxdcm_sm_eter.
 * Copyright(c) 2002-2_REG_PRC_VP(n)	vxIT(val, 0, 	vxge__W_PRIORITY_SS_109(v_RC_ALARM_REG_BTC_VPATH_MISMATCH_(valefine VfTA0_GET_RTH_GEN
#define VXGE_HWe VXGE_HW_RX_fBIT(val, 35, 5)
#define VXGE_HW_PRIORITY_SS_GE_HW_RTS_vSTAT(val) \
							vxSTEER_CTRL_DATA_STRUC_24(val) vxge_vBIT(vall) \
				_24(val) vxge_vBIT(va5)
#define VXGE_HW_RX_W_ROUND_ROBIN_13_RX_W1_PRM_SM_EY_SS_111(val) \
						e VXGE_HW_RX1  Server Y_SS_111(val) \
						al, 43, 5)
#define 1define 1XGE_HW_11
							vxg1_RTS_ACCESY_SS_86(valrr_reg;
eP(n)	vxge_mBIT(n)
/*0x0IORITY_SS_11(val) UND_ROBIN_11_RX__H(valSUMrd_fine VXe_cfg_vp;
#define VXG_HW_RX_W_ROUND_ROBIN_11_RX_W_P \
							vxge_vBIT(valD 19, 5)
#define VXGE_HW_RX_W_ROUND_ROOBIN_9_RX_W_PRIORITYSS_114(val) \
							vxge_vBIT(val, RETURobinfine VXGE_HW_RX_W_ROUND_RO_10_RX_W_PRIORITY_SS_87(val) \
	) \
							vxge_vBIT(val,, 35, 5)
#define VXGE_HW_RX_W_ROUND_in_11;
#define VXGE_HW_RX_W_ROU \
							v_HW_RX_W_Rine  19, 5)
#define VXGE_HW_RX_W_ROUND_RO	vxge_vBIT(val, 3, 5)
#define VXval) \
							vxge_vBIT(va, 35, 5)
#define VXGE_HW_RX_W_ROUND		vxge_vBIT(val, 11, 5)
#define val) \
							vxge_vBACCE19, 5)
#ACCESS_ESS_VP(n)	vxge_mBIT(n)
/*0x00a38*/	u64	rXGE_HW_RX_W_ROUND_ROBIN_15_RX_W_PRIOR, 35, 520(val) \
							vxge_vBIT(NITIAL_OF71_STEER_DATe VXGE_HW_RX_W_ROU *  vxge_b, 11, 5)5)
#define VXGE_HW_RX_W_ROUter.
vxge_b 5)
#define 2fXGE_HW2f_W_ROU2L_DATA_STRUVXGE5)
#define VXGrsTAL__pro							vxge_vBIT(val, 51, S_STxge_bVA_vBIT(val, 27, 5)
#CTRL_DATA_STRUCT_SE7251, 5)
#d_PRIORITeg_modifieRTS_ACCESS				vxge_vBIT(valge_vODIF	VXG5, 5)
#define VXGE_X_W_ROUND_ROBIval, 19, 5)
#3XGE_HWl) \W_ROU3ine	VXGE_HWl) \copyrigwrite_arb_pendinCopyright(c) 2002-2ne VXG			vxENDATA_IT(valROCRCal, 19, 5)
#define VXGE_HW_RX_PRIORITY_SS_126(val) \
	is noX_W_ROUND_ROBIN_14_RX_W_PRIORIPRIORITY_SS_126(val) \
	 \
							vxgeD_ROBIN_8_RX_W_PRIORPRIORITY_SS_126(val) \
	RX_W_RW_RX_W_ROUND_ROBIN_14_RX_W_PRIOPRIORITY_SS_126(val) \
							ER_DATA1_DA_MAC_ADDR7BIT(val, 0readE_HW_RX_W_ROUND_ROBIN_15_RX_W_TE_BITY_SS_126(va_W_ROU						vxge_vBIT(val, 51, 5)
#defineal) \
							vxge_vBIT(N_15_RX_W_PRIORITY_SS_127(val) \
		al) \
							vxge_vBIT((val) (val, 27, 5)
#define DURING_RESdmaif_dmadbl_RX_W_ROUND_ROBIN_15_RX_W_DMAON_PMADBLSS_126(va27, 5)E_HW_RX_vBIT(val, 0, 64)

} __packed;
27, 5)
#define VXGE_HW_RX_W_ROUNDRIT(val, 43, 5_PRIORITY_SS_132(val) \
							vxge_vBIT(val,IN_6_RX_efine VXGE_HW_RX_W_ROUND_ROBININ_16_RX_W_PRIORITY_SS_133(val) \)
#define VXGIORITY_SS_39(val) \
		 5)
#define VXGE_HW_RX_W_RX_W_r[17];
#define VXGE_HW_TOC_PCI				vxge_vBIT(val, 51, 5)
#RIORITY_W_ROUND_ROBIN_7_RX_W_PRIORITY_				vxge_vBIT(val, 51,al) \
	ge_bVADY	vxge_mBIT(0)
#define	VXGE_HW2
#dBIN_16_RX_n)
/*0x00wr VXG_HW_ense n0
/*0x00a88*/	u64	rdaITY_SS_128(e VXGE_H(bits)0_sm_err_mRX_W_ROUNABS_AVAbct__A0_RTH_ITEM1_BUCKET_NUM(val) \
define	VXGE7ACCESS_STE \
							vxge_v1IT(val, 3, 5)
#define VXGE_HW_RX_W_ROUND_ROBI1_17_RX_W_PRIORITY_SS_137(val) m;
/*0x	u64	tim_int_mask0;
#define_ROB 5)
#define 5VXGE_HW5FRF_AL74)
/*0x01220T(va5)
#define VXGE_HW_RX_(12)	VXGE_HW_WDE0_ALARM_REG_WDE0_CP_SS_ROCCLEARap_en;
#define VN_13_RX_W5_PRM_SM_ERR	vxge_mBIT(1)O_IT_BUCKET_DITY_SS_140(val) \
							vxbitsL4PRW_RX_RX_W_PRIORITY_SS_134(val) \
			l) \
							vxge_vBIT(val, l, 3ATUS_RDA_ECC_SG_RDA_ECSS_141(val) \
							vxge_vBIT(val, #defiIT(val, 43, 5IORITY_SS_100(val) \
							vxge_ve_vBIT(Ifine VXGEEN_PRI_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_97(val) \
	vBIT(val, 59, 5)
/*0x)
#define VXGEefine VXGE_HW_RX_W_ROUND_ROBIN_12_e_vBITGNW_PR_24(vaS_STEal) \IX		vxge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_Re_vBIFLASal, AD_MOUND_ROBIN_16ND_ROBIN_0_RX_W_PRIORITY_SS) \
							vxDIS_HOSX_W_PELIX_W_PN_12_RX_W_PRIORITY_SS_100(val) \
							vxge_ve_vBI_vBIT(vaxge_m) \
							vxge_bBIT(val, 51, 5)
#define VXGE_HW_RX__W_ROUND_ROBIN_18_S_STTOCESS_STEER_D**********************
 *7
#def_HW_RX_W_ROUND_ROBIN_17_RX_W_PRIORITEN_efine	X_W_PDITIAHW_T0ULL

#define VXGE_HW_SWAPPER_READ45(val) \
							vxgO				S_126(vaINT_RX_W_PRIORI00000000ULL

#define _ROBIN_18_RX_W_PRIORITY_9;
#definON_RD(0)
#define				vxge_vB5;

/*0x00be8*/	uBIN_17_RX_WESS_STEER_RX_W_ROUND_ROBIN_18_RX_W_PR3#defiEC******A_O5)
#SUPRX_W_ROUND_R00ae8*/	u64	rocrc_alarm_reg;
#defi				v_11_RX_WND_R, 59, 5)
/*0x00c80*/	uXGE_HW_RX_W_ROUND_ROBIN_17_RX_W_PRIOR3OUND	VXG_SWAPC_SM_ERR	vxge_mBIT(0)
#l) \
							vxge_vBIT(val, 3, 5)
;
#dLI VXGE_HW_RX_W_PRIORITY_SS_143(val) \
							vxge_vBI3W_RX
#define VXGE_HW_RX_W_x_w_round_robin_18;
#define VXGE_HW_RXBIN_19							vxge_vBIT(val,CCESS_STEER_DATA1_FvBIT(val, 19, 5)
#deMR	VXGEMVF#define	VXGE_HW_G3FBCT
#demBIT(19)
#define	VXGE_HW_ROCRC_ALAR\
							VF_TBLe_bVALn(bitC_ALARM_REG_BTC_SM_ERR	vx#define	VXGE_HW_RC_ALAR_151(val) \
							vxF0ine VXGE_H_RX_W_ROUND_RROBIN_4_RX__W_PRIORITY_SS_155(val) \
			define VXGE_HTATUS_RC_ALARM_RC_INT	vxl un15)
#define	VXGE_HW_ROCRC_ALARM_RE)
#de#defNTS_PF_VPATH_ASSIGNMENTX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_153vxgeASS_DAISYversI_RTS_MGR_STEE 5)
#defineRIORITY_SS_124(E_HW_Astart_host_add_XMAC_VPAT(n)
/*0x00a60*/	IN_18_SS_75(_SS_1     _W_PRIORITY_SS_160(val) \
		
/*0x00vxge_vBIT(val, 11, 5)
#de

#dl) \
							v95XGE_HW9OFF(bi75X_W_ROUND_RO9ge_bVALn(rd
							mBITA_STRUCT_SEL_PN    HW_RX_W_RATUS_9, 5VXGEOUTSTA		vxge_vTER_STATUS_PCC_PCC_IDLE(val) vrce_GE_HW_RTS_ACCESS_STEN_20_RX_W_PRIOGE_H_SS_162(val) \
							vxge_vBIT(val, 19, 5)
#def26e VXGE_HW_RX_W_ROUND_ROBIN_20_RX_W_Pin_17;
_SS_162(val) \
							vxge_vBIT(val, 19, 5)
#def34e VXGE_HW_RX_W_ROUND_ROBIN_20_RX_W_PWABIT(n)					vxge_bVALn(bits, 0, T(19)
#define	VXGE_ROBIN_20_RX_W_P_SS_162(val) \
							vxgEER_DATA0_GET_FW_VEge_mBIT(5)
#define	ROBIN_20_RX_W_PENxge_vBIT(val, 2GR_STEER_CTRL_D7bRDTARBIN_2
				9M_REG_PRC_VPbW_WRDMA_Ibf_sw_RTSeRM_WDE2_INT	vxge_mBBfine VXGE_Hc90*/	u64	r, 16)

#define VXGE_HW_RTS_67(val)PRIORITY_Se_vBIT(cense notice.  This file ne VXGE_HREG_QCQ_				vCMPL, 27, 5)
#define VXGE_HW_RX_W_R			vxge_vBIT(valINBIT(
#define VXGE_HD_ROW_ROUND_ROBId3XGE_HWd3\
				bf
							vxgdardware byTS_ACC_HW_RDA_ERR_REG_RDA_MISC_ERR	vX_W_PRIxge_mBIT(1)
#val,5)
#ROPIFM_WR_FLIP_EN(val) vxge_vBIT(val, 0, 64)
/*0	vxge_vBIT(val, 19, 5)
(valfine VXGE_HW_WRR_RING_SEvxge_mBIT(3)
7degister an21_RX_W_PRIORITY__W_PRIORITY_SS_138(val) \
				ine VXGE_HW_WRR_RI, 19, 5)
#define VXP_MAS 8)
EPLETEOUND_ROBIN_17_RX_W_PRIORITY_SS__queue_priority, 5)
#define VXGe	VXGE_HW_RD_PRIORITY_0_RX_Q_NUMBER_0(val) vxge_vBIT(val, 32 5)
#define VXGE_HWine QUEUE_PRIORITY_0_RX_Q_NUMBER_1(val) vxge_vBIT(val, 11, e_WR_SWAP_EVXGE_HW_RX_QUEUE_;
/*0x00a88*/	u64	rda(n)
/*0x00a60*/	eg;
#define VXGE_HW_			vxge_vBIT_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_1(val) vxge_vBIT(val, 11, e_vBIT(valVXGE_HW_RX_QUEUE_P4S_170(val) \
							vxge_vBIT(val, 1			v)

#dVPINdefine VXGE_HW_WRR_RING_SERVICE_STATES		_4(val) vxge_vBIT(val, 35, val, 5RD 5)
#define VXGEQ_NUMBER_5(val) vxge_vBIT(val, 43, 5)
#dine	VXGE_HWE_HW_Acount0_mBIT(3)
#define	VXGENIN_18_*****01_vBIT(val, 3, 5W_RTS_MGR_STEER_CTRL_Dxge_vBIT(val, 0, 64)
/*vBIT(val, 3, 5)
#define VXGE_HW_RTS_MGR_STEER_CTRL_DAT(val, 43, 5)
#RIORITY_SRIORITY_1_RX_Q2ESS_STEER_DATA0_RTHvBIT(val, 3, 523_RX_QUEUE_PRIOR_HW_RTS_MGR_STEER_CTRLX_Q_NUMBER_9(val) vxge_vBIT(val, 11, ITY_1_RX_Q_NUMBERHW_RTS_MGR_STEER_CTRL_DATA_NUMBER_10(v_QUEUE_PRIORITY_1_RX_QORITY_0_RX_Q_NUMBERvBIT(val, 3, 54ER_13(val) vxge__RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_10(N_15_RX_WRIORITY_1_RX_Q5ORITY_1_RX_Q_NUMBER_13(val) vxge5)
#define VXGE_H_RX_QUEUE_PRIORITY_1_RX_Q_NUMge_vBIT(val, fmust
 59, 
				eGE_HW_RX_W_Rxge_bVALn(RIORITY_1_fg[6e_bVALn(bits, 16, 8vBIT(val, 6)
#TYP 43,EG_RDA_RXD_ECC_DB_ERR	vval) \
					UEUE_PRIORITY_2_RX_Q_Nxge_vBINHW_TRT_NUMBER(bits) \
				9, _VER_MAJOR vxge_vBIRITY_2_RX_Q_N_RTSDne VX0cc8*/	u64	replication_qW_ADAPTER_STAT;
#define	VXGE_HW_REPL_24(va16(val) vxge_vBIT(val, 3,1OBIN_13_RX_Wne	VXGE_HWRIORITY_64bitx01138];

/*0x01200*/	vBIT(va_64e VXTRUCT_HW_RXvBIT(val_rsthdlr_cfg8FG	7
#define	VXGE_HW_xge_mBIT(n)
#define	VXGE_HW_RXx00bb0];

/*VXGE_HW_GEN_CT8_GET_USL_PR
				s) \
	horshL_PR_GET_RTH_GE3EL(val) \
							vxge_vBIT(val, 1TS_CO
#define	VT_4;
#TS_ATINITSS_STEER_DATA0_RTH_GEN_ALG_SEL_JENKICTRL_WR_XON_DIS	vxge_CESSTCRR	vxge_mBIT(13)
W_RQA_VPBP_CTRL_ROCRC_DIS	vxge_mBIT(H2IORIT0e0*/	5)
#define	VXGE__ALG_SEL_CRC32C	2
#defi(31)
/*0x00ce0*SQ(bits,H2LG_NOA_IMMM_ECC_SRTH_TCP_IPV4_EN(bits) \CAST_CTRL_TIME_OUT_DI2S	vxgbits, 32, 32)
#dW_RTS_ACCESS_STEER_DATA(31)
/*0x00ce0*/	uSM64	rx_mulTS_COUNT5_GET_PPxge_vBIT(val, 9, 7)
#deCAST_CTRL_TIME_OUT_DI3S	vxg			vx
/*0x00e90*/	u0x00cd8*0_GET_RTH_GEbp_ctrTCP_IPV6_EN(bits)9_GET_USctrl-0x8W_RTS_ACCESSctrlC_INITIALifcmd_fbAL_GET_KDFC_MAX_SIZE(bits) \
		IFW_GENBIT(ve_bVALn(bits, 17, 15)

#define VXGE_HW_9val, 32, 32THRESHOLD(val) *         9];

/*0x0018, 14)
#deVXGE_HW_KDFC_TRPL_FIFO_0_CTfine VXGETRL_MODE_MULTIA_STR__ROBIN_7_RX_W_PVXGE_HW_KDFC_TRPL_FIF	VXGE_HW_WDE_PRM_CTRL_/*0x00c48*/	u64	rx_w_round_;
#define	XGE_HW_WDE_PRM_CTRL_FB_RWDQER_D_ON_RO						vxge_vBIT(val, 27, 5)
#dTA_STRUCT_SEL
#define VXGE_HW_WDE_PRM_CTRL_FB_IOCALIT(vdefine VXGE_H_ENTRY_EN9          SPLIT_ON_1ST_RO	VXGE_HW_WDE_SDC_INITIALLIT_ON_1ST_RONMENTS_GET_HOST_TYP9RTS_MGRfinetain90_BIR(val) \fineT(val, 11, 5)
#cmu(val) vxge_vBIT(val, 2, 10)
#define VCMxge_m_WDE_PRM_CTRL_SPLIT_THRESHOLD(val) vxge_ADD_PRIVILE0(val) vxge_vBI	VXGE_HW_WDEe_mBIT(54)E0(val) vxgeT_ROW	vxge_mBIT(32)
#define	VXGE_A_CTWDE_PRM_CTRL_SPLIT_ON_ROW_BNDRY	vxge_mBIT(33)
#define VXGE_HW__HW_NOA_CTRL_MAX_ROW_SIZE(val) vxge_vBIT(val, 46, 2)
/*0x00cf_HW_NOA_CTRL_MAX_;
#define VXGE_HW_NOA_CTRL_FFRM_PRTY_QUOTA(val) vxge_vBIT(val, 3, 5)
#defin_HW_NOA_CTRL_MAX_RL_NON_FRM_PRTY_QUOTA(val) vxge_ACCESS_STE_WDE2(val) vxge_A_CTRL_MAX_JOTRL_IGNORE_KDFC_Il) vxge_S	vxge_mBIT(16)
#defL_SPI_MEN	v_NOA_4TRL_MAX_JOB_(1)
#definne	VXGE_HWT(n)
/*0x00ad8*/	u64	frf_alarmHW_PHASE_Cae0*/	u64	frf_CTRL_SPLIT_THRESHOLD(val) vxge_ge_mBIT(5)FG_UMQM_RD_PHASE	VXGE_HW_WDE_CTRL_SPI_FG_UMQM_RD_Pxge_vBIT(val, 53, 4)
#define VXGE_HEL_DA		0
#_MAX_JOB_CNT_FOR_WDE3(val) vxge_vBIT(val, 60, 4)
/*0x00cEN	vxge_mBIT(31cfg;
#define	VXGE_HW_PHASE_CFG_QCC_WR_PHASE_ENEN	vxge_mBIT(31;
#define VXGE_HW_NOA_CTRL_FRM_PRTY_QUOTA(val) vxge_vBIT(val, 3, 5)
#define_mBIT(39)
#definRL_NON_FRM_PRTY_QUOTA(val) vxge_STEER_DATA(27)
#define	VXG_RD_PHASE_EN	11)
#define	VXGE_HWL_RTH_KEY		9
BIT(16)
#def(8)
#de(val_NOA_8TRL_MAX_JOB_S_STEER_DAY_SS_111(W_ROUND_Rpvxge_bVALn(bits, 16, 8_24(val)  \
				GE_HW_WRDBIT(val, 55, 9ITY_0_RX_Q_NUMBER_1(val) vxge_v
#define VXGE_HW9c1, 5)
tatu_NOA_bD_ROBIN_10_tatucopyrigxgxfine 64	g3f2e_bVALn(bits, 16, 8XGXRX_Q_Nu64	pfIGT_SEEC_HW_RC0*/	#define	VXGE_HW_G3FBCTA_INT_STATUS_RC_INT	vxge_mBIT(7)
#define	VXGE_HW_DOORBg {
	_FCS			vxge_mBIT(31)

#define	VXGE_(n)
/*0x0e_mBIT(7)
#defiEVXGEFOfine VXGE_HW_l, 59, 5)
/C_INT	vxge_mBIT(7)
#defi_ERR_REGW_RTS_MGR_STEER_CTRL_DAueue_priority;
#definee_mBIT(7)
#defTX_S_STEESKEW_RX_QUEUE_PRIORITY_1_RX_QTXDMA_USDC_INT	vxge_mBIT(15)
/*0x_KDFC_E1R_REG_KDFC_KDFC_SM_ERR_ALAR					vxge_bVALn(bits, e	VXGE_HW_KDFC_ERR_RE2R_REG_KDFC_KDFC_SM_ERR_ALA_DATxge_mBIT(23)
#define	VXGE_HW_KDFC_ERR_RE3DFC_PCIX_ERR	vxge_mBIT(39)
mBIT(1OORBELLnused0001US_KDrxbIT(17)ERR_REG_TXDMA_KDFC_INT	vxge_mBIRXB_mask;
	VXGEfine	VXGY_SS_NUM(val) \
							vxge_vBIT(val, eg;
#define	VXefine	VXGE_HW_KRe_mBIDFC_ERR_RORITY_SS_TATUS_KDFC_KDFC_READY	vxge_m
#deOORBELLge_bVALn(EG_ALARM_KDTUS_WDE0_ALAG_TXDMA_KDFIT(15)
#define	VXGE_#define	VXGERR_REGRR_REG
				_ROBIDFC_KDFC_	fine VXGE_HW_RX_W_ROUND_ROBIge_mBIT(5)
#define	(32)
#define	VXGE_HW_KDFC_ERR_REG_ALARM_W_WDE_DFC_PCIX_ERR	vxge_mBIT(39)
	u8	un
#define	VXGE_HW_RC_ALAR(32)
#define	VXGE_HW_KDFC_ERR_REG_ALARM_S_ACCESFC_PCIX_ERR	vxge_mBIT(39)
	u8	unDA_RXD_Ege_mBIT(0)
#define VXGE_HW_KDFC_VP_PARTITION_0_NUMBER_0(Dal) vxge_vBIT(val, 5, 3)
#define VXGE_ 0, 8)
#defin9cbVALn(bitUS_KDM_KDFC_KDFC_MISC_ERR_1	ine	VXGE_HW_KD#define	VXGE VXGJ_PCval, ACTIVITRC_ALARM_REG_RHS_RXD_R/*0x00e18*/	u64	kdfc_err_maENGTH_1(val) vxge_vBITIS	v, 49, 15)
/*0x00e48*/	u64	kdRY_EN(bits) \
							vfine VXGE_HW_KDFC_VP_PARTITCT***********RXD_RHS_ECC_DB_ERR	vxge_fine VXGE_HW_KDFC_VP_PARTIT
#defi****LOS*****************************
 _HW_WRDMA_INT_STATUS_RXHW_KDFC_VP_PARTITION_1_LENGTH_2TA1_FLASH_VER_YEAR(val) \
		5, 3)
#define VXGE_HW_KDFC_VP_PARTITION_1_LENGALge_vBIT(					vxge_vBIT(val, 19, 5DFC_VP_PARTITION_1_LENGTH_3(val) vxgDEvBIT(val, 49, 15)
/*0x00e5ATA_STRUCT_SEL_RTH_JHASfine VXGE_HW_KDFC_VP_PARTITSKI	VXGal, QITY_0_RX_Q_NUMBER_1(val) vxge_vBITvp_partition_1;
#define VXGE_HW_KDFC_VP_PARTITRTITIDEne VUMBER_5(val) vxge_vBIT(val, 37, */	u64xge_vBId01200[0xUS_KDpmaX_W_PRIRR_REG_TXDMA_KDFC_INT	vxge_mBIPANE_DEETne	VXGE_RDEal, 3, , 16)

#define VXGE_HW_RTS_rbell_int_staXGE_HWTH_6_NOA_cne VXGE_HW_TH_6val, 37, 3)
#defiIT(12_KDFC_MISC_ERR_1	vxge_mBIT(32)
IN_1_W_RXGE_HW_KFWTI_OP_al, 5,ATUS_WDE1_ALARM_WDE1_INT	9dt vxgeLENGge_vBI)
/*0x01220LENGon_3;
#defininfo_KDFC_VP_PARTITION_3_NUMBER_6(vR_REG(val) vxge_R_REG_11(val) vxge_vBIT(val, 27, 5)
#define VXGE_Hdefine VXGE_HW_KDFC_VP_PARERR_REG_RDA_IMM_ECC_DB_ERR	vxge_mBI9dal, 11, 5)atemgmX_QUE_KDFC_VP_PARTITION_3_NUMBERATEMGMn)
#de	VXGEMOD		vxge_bVALn(bits, 0, define	VXGE_HW_GEN_CTR
#define VXGE_HW_K
#deS_130(val) \
							vxge_vBIT(v#define VXGE_HW_KFIXED_ine FSne	VXGE_HW_ADAPTl) vxge_vBIT(val, 17, 15)
#define ANTP 49, 15)
/*0x00e70*/l) \
							vxge_vBion_6;
#define VXGE_BE 49, 15)
/*0x00e70*/MAC_ADDR9dbVALn(bit)
/*0x00eM_KDFC_KDFC_MISC_ERR_1	vxge_mBIT_11(val) e	VXGE_HW_KDF#define VASK_E \
							vxge_vBIT(val, 35, 5)
8*/	u64	kdfc_vp_partition_7;
#VXGE_HW_KDFC_VP_PARTITION_5_LENGTH_11(val) dfc_vp_partition_7;
#o;
#definES_PHYe	VXGE_HW_ADAPTER_STATUS_X9d8GTH_7(vTIAL_V9d) \
							8;
#6_LENGTH_13(val)fixedFC_VP_PARTITION_3_NUMBER_7(valtion_7;
#(val, 
#define VESS_75 35, 5)
#define VXG9d)
#define)
/*0x00eantp16(val) vxge_vBIT(val, 17, 15)
/*0x00e8E_HW_4	kdfc_w_round_robin_0;
#defin) vxge_vBIT(val, 3, 5)
#define VXGE_HW_ine PREAM
#deE				)
/*0x00e80*/	u64NUMBER_1(val) vxge_vBIT(val, 11, 5)
#define   1SE, 3, 5)
#defi)
	u8	unused00cc8[0 3, 5)
#define VXGE_HW_w_roR_ADPHY32)
RRC_ALARM_ERR_ALARM	vxge_mBIT(23)
#de_NUMBER_2(val) vxge_vBIT(val, 19, 5)
#defT_W_PRIMDIOR	vxgON			vxge_R_STATUS_XGMAC_NETWORK_FAULTal) vxge_vBIT(val, 35, 5)
#define VXGE_HW_KDLTRUCTREINT	v_ROBIN_0_NUMBER_5(val) vxge_vBIT(val, 4RY_EN(bits) \
							v 3, 5)
#define VXGE_HW_ADVERTISE_10ESS_STEER_CTR0_NUMBER_2(val) vxge_vBIT(val, 19, 5)
#def 59, 5)

	uESS_STEER_CTRal) vxge_dA1_GET_RT_ROUND_ROBIbBIT(vMBER_0(val) vxge_vBIT(val, 3, 5)
#defB	rts_aE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_1(val) vxge_vBIT(valE_HW_KDFC_W_PARALLEL	VXGE_HW10G_KX4_RX_W_P) \
							vxge_bVALn(bits, 8, 16)
#definT(val, 11, 5)
#define VXGE_HW_KDFC_W_ROUD_ROIN_20_NUMBER_2(val) vxge_vBITD_ROBIN_0_NUMBER_3(val) vxge_vE_HW_KDFC_W_T_NUMBEUND_ROB_INT_STATUS_USDC_ERR_REG_TXDMA_USDC_INT	vxgeOUND_ROBIN_20_NUMBER_4(val) vxgD_RO_FCS			vxge_mBIT(31)

#define	VXGE_HW_VPATH_OUND_ROBIN_20_NUMBER_4(vaDMW_KDto theITION_2_LENGTH_4(val) vxgal) vxge_vBIT(val, 35, 5)
#defE_HW_KDFC_W_ 59, 5)

	u8	_ROBl, 59, 5)
/*0x00c38*/	u64	rx_w_R_7(val) vxge_vBIT(val, 59, 5)

#dfineCESS_STEER_DATA0_GET9dREG_SPACEUMBER_0(val) vxge_vBIT(valoc)		(0xXGE_HW_KDFC_W_ROU, 5)M_REGG#define	VXGE_HW_G3FBCT_ERR_REG_G3IF_CTRL_FIFvxge_vBIT(val, P_PAOUND_ROBI) \
RIC_VP_PARTITION_5_LENGTHI_INT_STATUS(v_ROUND_ROBIN_40_NUMBER__vBIT(val,BIT(val, 11, 5)
#define VXGE_HWine VX_round_PERMA_STOUMBERmgrdefin_KDFC_MISC_ERR_1	vxge_mBITvxge_MG_VSPORDFC_W_W\
							vxge_vBIT(val, 35, 5)
ne VXGE_HW_KDFC_W_RSTRO#define VXGE_C_SG_ERR	vxge_mBIT(ne VXGE_HW_KDFC_W_RITEM0_BUCKET_DATA(bits) \15, CCESS_STEER_DATA1_Fne VXGE_HW_KDFC_W_R_HW_RTS_ACCESS_STEER_DATA32)
#define	VXGE_HW9deGTH_7(ve#definehe authorshD_RON_40_NUMBER_fw_mstrxge_vBIT(val, 27, 5)
#define VXFSK_ATRDFC_W_CONNE_HWBEANOUND_ROVXGE_HW_KDFC_VP_Pused1068[0x01068-0x0fd0];

/*0x010TX_ZERTEER_entry_type_sel_0;
#fine VXGE_N_15_RX_WUMBERhwfs_ROUN) vxge_vBIT(val, 49, 15)
/*0x00e7vxge_HW_HW_ne	Vdfc_vp_partition_7;
#dHO

#define _USATA_PDXGE_HW_RX_W_ROUND_ROBIN_14_RX_W_PRIORIT(val, 14, 2)
#define VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBEDMNUMBER_2(va_W_ROUND_ROBIN_14_RX_W_PRIORI(val, 14, 2)
#define VXGE_HW_KDFC_ENTRY_TYPvBIT(_NUMBER_2(val) vxge_vBIT(valPRIORITY_SS_143(val)L_0_NUMBER_4(val) vxge_vBIT(val, 38, 2)
#define VXGE_H 30, 2)
#define VXGE_HW6_LENGTH_12(val) vxg(val, 14, 2)
#define VXGE_HW_KDFC_ENvxge_HW_IN_1)
#def_HW_KDFC_VP_PARTITION_0_ENne VXGE_HW_RX 2)
#define VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUM	kdfcNKDFC_AG				CEIVE_2(val) vxge_vBIT(vall, 59, 5)
/*0x00c08*el_1;
#define VXGE_HW_KDFC_ENTRY_TYPE_SELB64	rMBER_8(val) vxge_vBIT(val, 6, 2)
OBIN_10_RX_W_PRIORITel_1;
#define VXGE_HW_KDFC_ENTRY_TYPE_SELINT	N_RX_Wfine Val) vxge_vBIT(val, 3,
#define VXGE_HW_RX(val, 14, 2)
#define VXGE_HW_KDFC_ENUNEXPECHW_AN)
#dFW_PRBP9(val) \
							vxge_vBIT(val, 27, 5)
#def01100-0x1080];

/*0x01100*/	u64	kdfc_fifo_17_ctA****fine V;
#define VXGE_HW_KDFC_FIFO_17_ VXGE_HW_RX_W_ROUND_Rvxge_vBIT(val, 3, 5)

	u8	unused1600[0x01600-0x1108];

/*0x01600*/N VXG19, 5)
#define VXGE_HWE_HW_RXMAC_INT_STATUS_RXMAC_GEN_ERR_RXMAC_GEN_INT	vxge_mBDFC__WHENx1108];

/*0X_W_ROUND_ROBIN_3_RX_W_PMBER_5(val) vxge_vBIT(val, 46, 2)
#define VXGE_HW_KDF****_Bx00e00-0x00d08];

/*0x00e00*/	ul unal) vxge_vBIT(val, mBIT(11)
/*0x01608*/	u64	rxmac_int_mask;
	Nx00e00-0x00d08];

/*0x00e00*/	u_bVALn(bits,9eNT_FOR_WDW_KDFC_ENTRbpYPE_SEL_0_NUMBER_1(val) vxge_vBIT(val, 14, 2BESS_E_HW_KDFC_ENTRY_TYPEP_FP_PA_20_NUMBER_2(vasthdlr_cfg8;
#define VXGE_HW_C_ECC_ERR_REG_RMAC_PORT0_RMAC_RTS_PART_SG_EABILITYal) vxge_vBIT(val, 3,define VXGE_HW_KDFC_ENTRERR_REG_RMAC_PORT0_RMAC_RTS_PART_definR_CAP(val) \
							vxge_vBIT_STEER_DATA0_MEMO_IT 4)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RMAC_X4ORT1_RMAC_RTS_PART_SG_ERR(val_NUMBER_5(val) vxge_vBIT(val, 	VXGE_HW_RXMAC_ECC_ERR_REG_RvBIT(RT1_RMAC_RTS_PART_SG_ERR(valval, 0, 4)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RMAC_PORT0_RTX_NONCxge_vBIT(val, 62, 2)
/*0x01070*4, 5)
	u8	unus	vxge_vBIT(val, 8, 4)
#define	VXGE_HW_RXMAC_ECCNge_mBIT(4)
#
#define	VXGE_HW_SET 4)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_AOBIN_7_RX_W_Pl) \
							vxge_vBI 4)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_REMO

#dFRM_X_W_ROUND_ROBIN_3_RX_) \
							vxge_vBIT(val, 8, 4)
#define	VXGE_HW_RXMAC_ECCASM_DI
#define VXGE_HW_SWAPPER_BIT_FLI 4)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_efinedefine VXGE_HW_SWAPPER_READ_BYT 4)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_ECHO_ctrXGE_HW_RXed01618[0x01618-0x01610];
4val) \
							vxge_vB 4)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_SELEding FIElarm;
/*ITEM1_BUCKET_NUM(val) \
							larm;
/  Server W_KDFC_ENTRnecc_err_reg;
#define	VXGE_HW_RXMAC_ECC_ERR_REN_RMAC_PORT0_RMAC_RTS_PArl;
ITS_47val)3HW_RTS_HW_KDFC_VP_PARTITION_0_ENABLE	vxge_mBIT(0)
#defiERR	vxge_mBIT(34)
#define	VXGE_HW_RXMAC_ECC31val)TITION_RX_Q_NUMBER_1(val) vxge_vBIT(valVXGE_HW_KDFCetus;
#define	VXeX_W_ROUND_RMAC_e	VXGE_HWtp	rts__0(val) vxge_DB_ERR	, 15)
/*0(37)
C_ENTRY_TYPE_SEL_0_NUMBER_1(val) vxge_vBIT(vTP, 14, 2)
#define VXGE_HW_KDFC_ENTRY_TYPE_ge_vBIT(val, 22, 2)
#define VXGEne	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PNW_ROUND_ROBIN_1	vxge_mBIT(39)
#define	VXGE_HW_RXMAC_ECC_ERR_REG_RTSdefi7(val) vxge_vBIT(val, 62, 2)
/*0x01070*/rr_rRR(val) \
							vxge_vBIT(val, 40, 7)
#define	VXGE_HW_XGE_HW_WRR_FIFO_SERVICE_S	rx_w_round_robin_15;
#define Vdefine	VXGE_HW_RXMAC_ECC_ERR_REG_RTS_fifo_17_ctrO_LP_XHW_RX_vBIT(val, 6, 2)
/*0x01078*/	u64	kdfc_fne	VXGE_HW_RXMAC_ECC_ERR_REG_RTSJ_GOTG_RTSJ_unused0f28[0x0f28-0x0e90];

/*0vBIT(val, 54, 3)
#define	VXGE_HW_RXMAC_ECC_ERR_MESSBER_CODO_SERVICE_STATES			153INT \
								vxge_mBIT(val, 54, 3)
#define	VXGE_HW_RXMAC_ECC_ERR_REGHC_2(val) vxge_vBIT(valge_bVALn(bits, 0, 8))
/*0x01638*/	u64	rxmac_ecc_err_maskFOUND	u64UND_ROBIN_10_RX_W_PRIORITY_SS_8)
/*0x01638*/	u64	rxmac_ecc_err_mask;
/*0x01640INg {
	utionX_W_ROUND_ROBIN_3_RX_C_DS_LKP_SG_ERR \
							vxge_mBIT(60)
#define	VXGE_HW__ERR_REG_R83(val) \
							vxge_vBIT(val,)
/*0x01638*/	u64	rxmac_ecc_err_maskPERSIST_vBIER_6(X_W_ROUND_ROBIN_3_RX_P_PARTITIege_bVALn(_RTSJ_RMAC__ecc_err_reg;
#define	VXGE_HW_RXMAC_Ene	VXGE_H			vxge_vBIT(val, 20, 4)
#define	VXGE_HW) \
							vxge_vBIT(larm;
/*0x01660*/	u64	rxmac_gen_cfg;				vxge_vBIT(C_DS_LKP_SG_ERR \
							vxge_AC_ECC_ERR_REG_RTSJ_RMAC_DA_GE_HW_ADAPTEval, 0, 4)
#define	VXefine VXGE_HW_RXMAC_AUTHORIZE_ALL_ADRR_REG_RTSJ_RMAHW_RXMAC_GEN_CFG_SCALE_RMAC_UTIL	vxge_mBIT(11)
/*0x01668*TS_PARRR_REG_RTSJ_RMAC_VID_LKP_DB_ERR	vxge4*/	u64	wrdmall_addr;
#define VXGE_HW_RXMAC_AUTHORIZE_ALL_ADAC_ECC_ERR_REG_RTSJ_RMAC_VID_LKP_DB_ERR	vxge_EN	vxge_mBI9ebVALn(bit_RTSJ_RMAC_xC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT0_SG_ERne	VXGE_HXIT(34)
#define	VXGE_HW_RE_HW
#define	VXGE_HW_RXMAC_GEN_CFG_SCALE_RMAC_UE_HW_RXMAC_RED_RATE_REPL_QUEU*/	u64	rxmac_authorize_all_addr;
#define VXGE_HW_RXMAC_RED_RATE_REPL_QUEUMUCT_SEL_RTH_Mal) vxge_vBIT(val, 8, 4)
#define VXGE_HW_RXMAC_RED_RATE_REPL_ VXGE_HW_STATTHR3(val) vxge_vBIT(val, 12, 4)
#define VXGE_HW_RXMAC_RED_TOGG_PRIORITY_SS_1THR3(val) vxge_vBIT(val, 12, 4)
#define VXGE_HW_RXMAC_RED_RR_REG_RTSJ_	unused01618[0x01618-0x01610];
 VXGTHR3(val) vxge_vBIT(val, 12, 4)
#define VXGE_HW_RXMAC_RED_UNFRTSJ_RR_REGW_RTS_MRMAC_PN_LKP_PRT0_DB_ERR	vxge_mBIT(35)
#define	VXGE, 4)
#define	VXGE_HW_RXMAC_RED_RATE_REPL_QUEUE_TRICKG_RTSJ_RMAC_PN_LKP_PRT0_DB_ERR	DA_RXD_ERR(n)9ed01200[0xmdio_3(vaaccesC_KDFC_MISC_ERR_1	vxge_mBITROUNDGE_HX_W_RO, 5)
#define0*/	ge_vBal) vxge_vBIT(val, e	VXGE_HW_RXMAC_CFG0_OP_MBER_RX_W_PRIORITY_SS_158(eue_priority;
#definee	VXGE_HW_RXMAC_CFG0_DEVALARM	vxge_mBIT(0)
/*0x0EN	vxg15)
#define	VXGE_HW_RXMAC_CFG0_PORT_I_vBIT(val, 43, 5)
#define 
							vxge_vBIT(val, _HW_RXMAC_CFG0_PORT_IG) vxge_vBIT(val, 51, 5)
#defe	VXGE_HW_RXMAC_CFG0_PORT_IGNORE_LEN_MISMS_14ATTERa00[0x0*************
 *9al) \
							vxge_vBIT	VXGE_HW_RXMAC_CFG0_ VXGE_HW 19, 5)
#define VXGE_HXMAC_CFG0_PORT_IGNORE_LEN_MISMPRTRE_LONG_ERR	vxge_mBIT(155val) \
							vxge_vBI	VXGE_HW_RXMAC_CFG0_PORT_DITWOine VXGE_HW_RX_W_ROUND_ROBaVPATH_G3)
#PN_LKPne VXhorsh3)
#on_3;
#dccesvs4	g3_choices			vxge_bVALn(bits, 16, 8 VXGE64	unuNTRYICESW_RTCFG_PORVECC_E_DB_REG_RDA_RXD_ERR(n)	vxVXGE_HW_GEN_CTaRTS_MGRUSE_taina2D_ROBIN_10_USE_HW_RTS_MGR_STEER_CTRLvYP_OFF_THRESHOLD(val) vSEL_QOS       V_HW_RXM VXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RANGE_PN            4e_vBIT(vaGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_GEN_CFG         5
#dVDA_LXGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_SOLO_IT         6
#e_mBIT(43)
W_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_JHASH_CFG       7
e_mBIT(43)
HW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_MASK            8e_mBIT(43)
_HW_RTS_MGR_STEER_CTRL_DATA_SHW_RXMAC_PAUH_6(val58*/_DUAL4D_ROB} __pack				
/*OBIN_13_RX_W_PRvBIT_H*/
struct0, 14)hwM_SM_ERRreg {IT(0)
#/*0x(bits) \
	vmr2sVXGES_ACCESS_STEER_DATA1TATS_GET_RX_MPA_CRC_FAMR2RTS_\
							vxge_vBIT(val, 0, 8)
#defineVXGE_HW_RX_W_ROUND_ROBIN_20XMAC_ECC_ERR_RE0PE     xge_tain00           xge_IT(val, 35, 5)ERSION          13

#define VXGE_HW			vxgeDDR_MASK(val) vxge_vBIT(vask;
/*0x00e	VXGE_HADDR(val) vxge_vBIT(val, 0, 48_RTS_MGR_STEER_DAT
	u8	uxge__LINK_UTIL/	u64	rxTA0_GET_PN_SRC_DEST_SEL(bi						vxge_bVALn(bits, 0,al, 0, 48
#define VXGE_HWL_DATA_STRUCT_ge_vBIT(v	u64	rxmBIT(val, 35, 5)EGED_MODE \
								vx0VXGE_HW_RXVXGE_HWESS_STEER_CTRL_DATA_STRUC
/*0x017a0*10
#defin
/*0x00a68*/	u64	R_CTRL_D64	rxmacXMAC_LINRIORITY_SS_124(_ALG_SEL(bits) 01efine VXGE_HW_RXn(bits, 10,vxge_vBIal) \
							vESS_STEER_CTRL_DATA_STRUC_LINK_UTIL10
#definESS_S0)
#define VXGE_HW_RXMAC_0_RTH_GEN_ALG_SEL_J	vxge_mBIT(23)
	u8	unuse1_SG_efine VXGE_HW_RXMAC_al) vxge_vBIT(val, 	vxge_mBIT(23)
	u8	unuse2ne	VXGE_HW_RXMAC_STATUS_UE_FRATE_THR0(val) 	vxge_mBIT(23)
	u8	unuse3ne	VXGE_HW_RXMAC_STATUS_PL_QUEUE_FRATE_THR1	vxge_mBIT(23)
	u8	unuse4ne	VXGE_HW_RXMAC_STATUS_E_REPL_QUEUE_FRATE_	vxge_mBIT(23)
	u8	unuse5ne	VXGE_HW_RXMAC_STATUS_INT \
								vxge_	vxge_mBIT(23)
	u8	unuse6ne	VXGE_HW_RXMAC_STATUS_l) \
							vxge_vB	vxge_mBIT(23)
	u8	unuseERR_Refine VXGE_HW_RXMAC_ERR(val) \
							v	vxge_mBIT(23)
	u8	unuse8AC_RX_PA_CFG0_IPV6_STOP_is software may be 	vxge_mBIT(23)
	u8	unuse9AC_RX_PA_CFG0_IPV6_STOP_
#define VXGE_HW_RXmac_status_port[3];
#defid017d0[0x017d0-0x017b8];
SS_65(val) \
							mac_status_port[3];
#defiine	VXGE_HW_RXMAC_STATUS_PPORT_RMAC_RX_FRM_RCVD	vxge_mBIT(3)
	u8	unus1ed01800[0x01800-0x017e8];_ROBIN_3_RX_W_PRIORImBIT(43)
#define	VXGE_HW_	VXGE_HW_RXMAC_RX_PA_CFG0		vxge_vBIT(val, 27,mBIT(43)
#define	VXGE_HW__HW_RXMAC_RX_PA_CFG0_SUPPRITY_SS_68(val) \
		mBIT(43)
#define	VXGE_HW_RXMAC_RX_PA_CFG0_SEARCH_F6_LENGTH_12(val) vxgmBIT(43)
#define	VXGE_HW_RX_PA_CFG0_SUPPORT_MOBILEESS_STEER05)
#defineTIL_PORT_R(val) vxge_vBIORITY_SSTIL_PORT_RG_BYP_ON_THRESHOLD016_mBIT(16)
/*0x12
#define VMAC_IT(val, 35, 5)
#define VC_VP_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_mask;_RX_PA_CFG1_REPL_IPV6r_alarm;
/*0x00a70*/	u64	rda_err_reg;
#def01							vxge_bVALge_mBIT(3)
#deRM	vxfine	VXGE_HW_RXMAC_RX_PA_CFG1_REPL_IPV6_TRI6_TCP_INCL_PH	vxge_mBITP_INCb8];

/*REPL_IPE_HW_RXMAC_HW_RXDWM_SM_ERR
#de	VXGE_HW_WDE0_ALARM_REG_WC_RX_PA_CFG0_IP(val, 12, 4)
#define VXGE_(7)
#define	VXGE_HW_RXMAC_RX_PA_CFG1_REPL_IP5)
#definY_SS_111(NCL_CF	vxge_sene VXGE_HW_PF_SW_RE
	u8	unusMAC_RX_PA_CFGEne	VGR_CFG0_RTS_DP_SP_PRIO
							vxge_vBIT(val, 11, 5)
#deT(33)
#d01_VP_PARTI28*/	u64	rts_mgr_cfgdefine	VXGE_HW_RTS_MGR_CFG0_RTS_DP_SP_PR	u8	unuseIP_VLAN_TAG	vxge_mBIT(23)
	u8	unused01828[0x01828-0x
#define	VXGVPATH_Ge	VXRX_PA_D_ROBIN_10_e	VXIT(val, 35, 5)ge_mBIT(n)
/*0x00ad8*/	u64	frf_alarm			vxge/*0x00ae0*/	u64	frf_alarm_a		11
#define	VXGE_HW_RTS_ACS_Sg;
#define	VXGE_HW_ROCRC_ALADDR(val) vxge_vBIT(val, 0, 48ALARM_REG_QCQ_OWN_NOT_ASSIGNENABLE_HIERARCHICA021XGE_HW_1TS_MGR21788*/	u64	r#define	VXe_mBIT(43)
#define	fine	VXGE_HW_WDE0_ALARCFG0_L4PRTCL_FLEX_TR	vxge_mBIT(2ne	VXGE_HW_RTS_MGR_CFG0_L4PRTCL_UDP_TRASH		vxge_mBIT(3)define	VXGE_HW_RTS_MGR_CFG0_L4PRTCL_FLEX_T_HW_WDE2_ALARM5)
#define	VXGE_HW_Rge_mBI_HW_0_IPFRvBIT(val, 1_HW_IT(val, 8, 4)
#ION          13
IORITY_ETBIT(val, 8, 4)
#fine VXGE_HW_RXMAC_Lhardware be_mBIT(43)94(val) \
							vxge_vBIT(vaCFG0_L4PRT_W_ROUND_ROBICI 27, 5)ts, FG_RCBM_W_TABLE	vxge_mBIT(7)
/*0x01838*/	u6ITERIA_PRIORITY_L4PN(val)UNCO   7

C_UTILIZATION(val) \
							vxS_MGR_CRITERIA_PRIORITY_RANGL4PN(val) vx(bits, 8, 16)
#define VXGE_S_MGR_CRITERIA_					efinCHRIOUSRX_PA_CFG0_TOSS_OFFLD_FRM_l, 21, 3)
#define VXGE_H5)
#define EAD_MEMO_ENTRY		3
#define, 21, 3)
#define VXGE_RX_WFVXGE_HW_RX_W_ROUXGE_HW_RTS_AC0register a) vxge_vBIT(val, 9IORITY_ICMP_T, 15)
/*0) vxge_vBIT(val, 9c) - set bit02
#define VXGE_HW64	rts_mgrter.
 * Copyright(c) 2002-2
/*0x00a68*/	u64	4	rx_w_round_ro40*/	u64	rts_mgr_da_paXMAC_LINK_UTIL2l) vxge_vB_L4PRTCL(val) vxge_vBIIORITY_ICMP_T030*/	u64	_L4PRTCL(val) vxge_vBITS_MGR_CRITERI4_TCP_INC28*/	u64	rts_mgrter.
 * Copdefine VX
/*0x01828*/	u64	rts_mgr(val, 0, 17)
/*0x01d01200[0x							vxge_vBIT(val, a_cfg1;
#define	VXG2ition_   r0_IPFR7A_PRIORITY_E_VP_PARTIpvxge_vBIT(val, 59, 5)
/*0x00ts, e VXGE_H	u64	dbg_stbin_21;
#define VXGE_HW_RX_W_ROU02URING_RESe_mBIT(43)
#def17_RX_W_PRIORITY_SS_14_QOS(val) 						vxge_60(v
#define VXGTS_MGR_CRITERIA_PRIORITY_QOS(val) ne VXGE_HW_DBG_S							vxgound_robin_15;
#define Val) vxge_vBIT(val, 8X_W_P     RX_ANY_FRMSl, 59, 5)
/*0x00c08*S_PORT2_RX_ANY_FRMS(val) \
			ine VXGE_HW0x0f28-0x0e90];

/*0S_PORT2_RX_ANY_FRMS(val) _STEERX_ANY_FRMS
							vxge_mBIT(61vp[17];
#define VXGE_HW_RXMAC_/*0x01a00*/	MAC_ADDR_KDFFRMS_PORT0_RX_interrupX_QUE_mBIT(3)
#define	VX_QOS(vafine	Rxge_S_ROCprograHW_RX_WXGE_HW_RDA_ERR_REG_RDA_P0*/	u64	wrdma_int_statE_HW_RXMAC_RED_RATE_VPTRAFF_W_RLAS#define	VXGE_HW_G3FBCTueue_pine	VXGE_HW_RASSIGNRED_0_IPFR9A_PRIORITY_Ege_bVALn(ge_bVALclear_msixts_mgr_cfg1;
#define	VXGE_HW_R_vBIT(X_W_PR	vxg_VP_FRATE_THR1(val) vxVXGE_HW_RXMAC_2REG_SPACEge_bVAL_PRIfine VXGE_HW_RXMAC_RED_RATE_VP_FRAl) v1(val) vxge_vBIT(#define VXGE_fine VXGE_HW_RXM_vBIT(val, 16, 4)define ACCEshoRM_WDE2_IN_RED_RATE_VP_FRATEHR1(valCP_ISHER_C88];

/*0x01e00*/	u64	xfine VXGE_HW_RXPERMA_STONCL_CF	vY_SS_123(val) \
							vxge_efine VX, 27, 5)
#d(3)
#define	VXGE_Hl) vxge_XGMAC_IN_vBIT(val, 16, val) \
							vxge_vBIT(val, 3HW_RXMAC_Rine VXGE_H)
#define	VXGE_HW_XXMAC_LINK_ERR_PF_INT	vxgegt_pf_illegalORT_STR	VXGE_HW_WRDMA_INT_, 29, 3)
#define VXGge_mBIREG00a00[0x0vBIT(val, 8, 4)
#defiR_PORT1_VXGE_HW_RTS_ACCESiefine	VXGE_HW_RC_GET_RTH_GEN_BUCKETISM_ERR	vx							_ERR_ASIC_NT_VP_FRATEefine	VXGE_HW_RTS_ACCESS_STE_NTWK_INT	vxge_mBIT(19)
#defiS_126(vavxge_vority;
#define VXGE_8 rts_mg8_cbasin_eFG1_REPL_IP8_VP_PARTIxg_STATr(n)
/*0x00ad8*/	u64	frf_alarmXG14)
#P_RES) vxge_vASIC_NTWxge_						ETECTED \
		W_RXMAC_LINK_UTIL8URING_RESfine	VXGE_HW_(val) vxge_v8CRATE_THRasic_ntwkVXGEe	VXGE_HW_RTS_ACCESS_STEEDETECTED \
						ROUND_pair_ED \
USTAINW_ADFRM_P(val, 22, 2)
#define VXGine	VXGE_HW_XMAC_GEN_ERR_REG_LAGC_LAG_PORO_FRATEERR(val) \
							vxine	VXGE_HW_XMAC_GEN_ERR_REG_LAGC_LAG_PORT0_RE_OCCURR vxge_vBIT(vIL_CFG(valACPDU	vxge_mBIT(15)
#define	VXGE_HW_XMAC_GEN_ERR_REG_LAGC_LAAC_GEN_ERX_PA_CFG0RT0_PART)
#definN_DETECTED \
				LAG_PORT0_PARA1_GET_RTN_DETECTED \
				a_cfg1;
#define	VXG8#definFAIL
/*0x8X_W_ROUND_RFAILFG_PORT_RCV_EN	vxge_mBIT(7)sr_clonR_DATA0_RTH_ITEM1_EAUSE_CFG_PORT_ACCEL_SIORICP_ID(val) vxge_vBIT(vafine VXGE_HW_RX_W_ROUND_ROBIN_1ine	VXGE_HW_3, 5)

_STAECTED	IS	vxge_mBI_STA_UTIL_POR_NUM(bits) \
								VXGE_HW_WDE0_ALARM_8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_ITEM1_BUCKET_NUM(val) \
							 \
					)
#definumq_vhKET_D_list_empty	VXGE_HW_WDE0_ALARine	VHRXMAC_LISTefineYGE_HW_M_ERR(val) \
							vxgdefine	VXG_HW_RTS_ACCE0MODE			0
#wdBIT(val, 0, 17)
/*0x012WD	rts_aNS0(val, 4MWUND_RRABLE	vxge_mBIT(3)
#define \
							vxge_vBIT(vaEN_GEN_ERdefine	VXGE_HW_XMAC_GEN_ERR_REG_XSTAQval, 48, 2)
UE_FRATE_THR0(val)  \
							vxge_vBIT(vMAC_STATval, 51, 5)
#define \
							vxge_vBITPar1_, 48, 2)
E_REPL_QUEUE_FRATE_l) \
							vxge_vBIT(vMAC_STATRX_W_PRIORITY_SS_128\
							vxT(vaOPval) BILE_IPV6_HDRS	vxge_mBI \
							vxQ_vBIT(val, 54TY_SS_127(val) \
			\
							vxgIT(vBIT(val, 54is software may be _ERR(val) 1xge_vBIT(val, 48, 2)

#define VXGE_HW_RXe	VXGE_HW_XMAC_GEN_ERRMAC_STATS#define	VXGE_HW_XMAC_GEN_ERR_XMAC_GENT(val, 50, 2)	u64	rx_w_round_robinVXGE_HW_XMAC_GEN_ERRMAC_STATS
#define	VXGE_HW_XMAC_GEN_ERRXMAC_GEN_IT(val, 52, 2)		vxge_vBIT(val, 27,BIT(63)
/*0x01e18*ATS_RMAC_STATRITY_SS_68(val) \
		BIT(63)
/*0xge_vBIT(val, 54_RX_W_PRIORITY_SS_12820*/	u64	xmRR_REG_XSTATS_d00e40[0x00e40-0x00eT_REG_XMACJ_P						vxge_vBIT(val) \
							vxge__HW_XMACDIS7
#deQfineW_RXUN1(val-0x1DDxge_vBICCESS_STEER_DATA1_F_HW_XMACVP_PARTITIPREFERE)
#defineAC_EN	vxge_mBIT(_KDFC_W_ROUND_ROBIN_40_HW_XMAC

#dWO_BYT
							vxge_bVALn(bits, val) \0_port[3];
#define VXGE_VP, 17, _RED_CFG0_PORT_RED_EN_Vvp0x00ege_mBITine	VXGE_HW_0t vxge_mBI
/*0x01FG1_REPL_IP38(val) \
Y_SS_111(funM_WR_0_NURX_W_PRIORITY_SS_14ITY	vxge_FUNval,ne VX, 19fine	VXGE_HW_XMAC_LI00e00-0x00d08];

/*0x00e00*/	u64	dooW_XMAC_L
/*0x01828*/	uis_firARM_WDE2_INBIT(27)
#defineIS_FIfinee_mBIT(35)
#deVXGE_HW_RTS_ACCESS_0_SS_57(valCL_PH	vxgeY_SS_1
#define	VXGE_HW_RXMAC_RX_PA_CFGK_ERR_IPV6_TCP_INCL_P_ERR_PORT_(7)
#define	VXGE_HW_RXMAC_RX_PA_CFG1_REPL_I, 11, 5)
#LASI_INV	vxge_mBIT(3define	VXGE_HW_RXMAC_RX_PA_CFG1_R_ERR_PORT_R_INCL_PH	vxge_merr_port0_alarmATA_STRUCT_SEL_DS		11
#dEPL_QUEUE	vxge_mBIT(11)
/*0x0(val) vxge_rxmac_red_cEER_Y_SS_1S_STEER_DA_ITEM0_BUCKET_NUM(val)K_ERR_	vxge_vBIT(, 0, 8)
#define	VXGE_HW_RTS_ACCESS_ST
#define	VXGEBIT(23)1#define            e_mBBUCKET_NUM(bits) \
						p_bVALn(bits, 16, 8)
#define	VXGE_HW_Pen_err_alarm;
/*0x010_RTH_ITEM1_BUCKET_NUM(val) \
							vxge_vBIT(v01ILOVER_1ETECTED14/*0x01e60*/	
#define	r_RCV_ER_ST_mBIT(J_PORT_G_XSTATS_RMAC_STATS_TIL_ALARM12)
#W_PRIO_HW_XMA	vxge_5)
#defFRAval,BIN_4_RX_W_PRIORITY_SS_39(val) \efine	VXGE_HW_ASIC_NTWK_ERR_RE*/	u64	pf_vpath_assignments;
#define VXGE_HW_PFXGE_HW_ASIC_NTWK_ERR_REG_XMACJ_EAR_EN_OR_HA_CFG_PORT_GE_ROUND_ROBIN_8_RX_W_XGE_HW_ASIC_NTWK_ERR_REG_XMACJ_NTWK_WEMOBILE_IPV6
			STS_MGR_STEER_DATA1_DA_MAC	vxge_mBIT(7)
#define	VXGE_HW_ASIC_NTWK_ERR_REGmBITge_vBFIRMEDINnk_err_port1_reg;
/*0xe VXGE_HW_RX_W_ROUNDefine	VXGE_HW_ASIC_NTWK_ERR_RENO_PS_ALA9(val) val, 6, 2)
/*0x01078*/	u64	kdfcC_NTWK_ERR_REG_XMACJ_NTWK_REAFFIRMED_FAULEMBERKP_DB_ERR \
							vxge_mBIT(61efine	VXGE_HW_ASIC_NTWK_ERR_RETOBINDDR3_U__ALAT(10ND_ROBIGET_GENSTATS_COUNT0(bintwk_err_mask;
/*0x01e80*/	u64	asic_ntwk_err_alarMACJ_OFFL_ADAP	vxge_mBIT(3)
#define	VXGE_HW_XGMAC_ge_bVALn(bits, 0, 8)ine	VXGE_HW_XGMAC_GEN_STATUS_XMACJ_NTWK_OK	vxge4n_fw_memo_status;
#define	VXGE_	asic_gpio_err_mask;
/*0x01e98*/	u64	asic_gpio_er01ea8*/	u64	xgmac_ge_vBIT(val, 0, 17)
/*0x01eb0*/	RR	vxge_mBIT(1)
#define	VXGE_HW_XGMAC_GEN_STATUS_XMACJ_NTWK_OK	vxgR8[0x01ge_vBIT(val, 0, 64)
/*ac_gen_status;
#define	VXGE_HW_XGMAC_GEN_STATUS_XMACJ_*/	u64	xgmac_W_XGMAC_GEN_FW_VPATH_TO_VSPORntwk_err_mask;
/*0x01e80*/	u64	asic_ntwk_err_alarJUMBOpf_vpaW_RTS_MGR_STEELINK_UTIL_, 17)

#defs_3(val_REG_XMACJ_NTWK_UP	vxge_mBIT(7)
#deGE_HE_HW_ASIC_NTWK_ERR_REne VDP_S128*IO2)
#T_DOWN	vxge_mBIT(11)
#define	VXe VXGE_HW_XMAC_GEN_CFG_RAFLEX_L4PRTCLMAC_UAC_RED_RATE_REPL_QUEUE_FRATE_THal) vxge_vBIT(val, 3, 5efine	VXGE_HW_XMAC_GEN_CFGICM, 32)SCRDT_DEPLETEmac_gen_status;
#defie VXGE_HW_XMAC_GEN_CFG_RATCPSYNine VXGE_HW_XMAC_Gntwk_err_mask;
/*0x0e VXGE_HW_XMAC_GEN_CFG_RAZL4PYLDine VXGE_HW_XMAC__HW_XGMAC_GEN_FW_MEMOe VXGE_HW_XMAC_GEN_CFG_RAEAD_DROPT						x01f48*/	u64	x_0_NUMBER_1(val) vxgne	VXGE_HW_XMAC_TIMESTAMP_EN	vxgeUDBIT(3)
#define VXG*0x01eb8*/	u64	xgmac_ne	VXGE_HW_XMAC_TIMESTAMP_EN	vxge_TX_H#define VXGE_HW_XEN_CFG_PERIOD_NTWK_UP(val) vxge_vBIT(val, 28,IPFR0x00define VXGE_HW_XD_RATE_VP1_vBIT(val1ed0];

/ VXGria_priority01f40*/	u64	xmac_gen_cfg;
#define VXGE_Hl) \RIAC_RATE_SEIC_NTWK_ERR_RE0x01eC_RED_RATE_REPL_QUEUE_FRATE_THR3(mac_timestamp;
#define	VXGE_ne VXGE_HW_XMAC_STATS_GEN_CFG_#defin4)
#d****************************
 ueue_priorityine VXGE_HW_XMAC_STATS_GEN_CFG_VPATH_CUM_TIMER(vL4Pa00[0x00a00-0x00028];

/*0x00a003, 4, 4)
#define VXGE_HW_XMAC_STATS_GEN_CFG_VPATH_CUM_TIMER(v_RX_W64	xmac_stats_sys_cmd;
#define VXGE_7W_XMAC_STATS_SYS_CMD_OP(val) vxge_vBIT(val, 5, 3)
#define	VXGBIT(*****************************
 21eue_priority;
#defineHW_XMAC_STATS_GEN_CFG_VPATH_CUM_TIMER(v					vxge_vBIT(ve VXGE_HW_XMAC_STl, 4, 4)
#define VXGE_HW_XMAC_STATS_GEN_CFG_VPATH_CUM_TIMER(vQLL_INT_S)
#define VXGE_HW_XMAC_STXMAC_STATS_GEN_CFG_VLAN_HANDLING	vxge_mBIT(15)
/*0x01f58*/	u6 32, 4)AC_CFG0_PORT_RMAC_EN	vxge_mBIT(HW_XMAC_STATS_SYS_CMD_OP(val) vxge_vBIT(val, 5, 3)
#define	VXEAD_DROAC_CFG0_PORT_RMAC_EN	vxge_mBIT((15)
#OLLOVER_)
#define	xge_mBIT(6)
#G_XMACJ_NTWK_fbct_err_mask;
/*0x00_ALARMATUS_RDA_E3)
#define	VXGC_DB_RDA_ECC_DB_INT	vxge_mBIT(8)
#defo;
#define VXGE_HW_ASIC_NTWK
#deP_FC(0)
#define	VXGP(n)	vxge_mBIT(n)
/*0x01f90*/	u64	asic_ntwk_cDISCAA0_MFFG1(bits) \
		0x01eb8*/	u64	xgmac_gen_fefine VXGE_HW_ASIC_NTWKG_XMACJ_C_vBIT(PARTITION_6_LENGTH_12(val) vxge	VXGE_HW_XMAC_CFG_PORT_XGMII_LOOPBAPOIN, 5)
#define VXGE_HW_RX_W_ROUND_ROBFG_PORT_XGMII_REVERSE_LOOPBACK	vxge_URT_Rine Verr_reg;
#define VXGE_HW_ASIC_GPIO_ERT_XGMII_REVERSE_LOOPBACK	vxge_mIORIRB_PD_CJ_RMAC_RTH_LKP_SG_ERR( VXGE_HW_ASIC_NTWK_CFG_PORT_NUM_VP(n)	vxge_mBIT(VXGE, 4)
LEBIT(val, 8, 4)
#define	VXGE_HW_5 VXGG1_REPL_IPN_15_RX_WIT(15)p_GETfine	VXGE8*/	u64	asic_ntwk_cfg_show_port_info;
#define val, 37, C_NTWK_ERR_RES_MGRHOW_PORT_INFO_VP(n)	vxge_mBIT(n)
/*0xLAG_CFG_MODE(val) vxge_vBIT(_in_VXGE_HW_RX_W_ROUND_ROBIN_19_RX_WX_DISCARD_BEHAV	vxge_mBIT(11)
#defX_W_fineNRL_REQ_TEST_NTWK	vxge_mBIT(3)
#XMAC_STATS_GEN_CFG_VLAN_DISCARD_BEHAV	vxge_mBIT(11)
#defDURL_DHe_mBIT(3)
#define	VXGE_HW_XMAC_CFG_PORLAG_CFG_MODE(val) vxge_vBIT(GE_HWPIT(1ADAPTER_STATUS_XGMAC_NETWORK_FAULTd00e40[0x00e40-0x00etus;
#define	VXGE_HW_LAG_STATUS_XL5)
#defe_bVK	vxgedefine	VXGE_HW_XGMAC_GEN_STATUS_XMACJ_NTWK_DATAlag_active_passive_cfg;
#define	VXGE_Hon_aemo_status;
#define	VXGE_HW_XGMAC_GEN_FW_MEMO_STATLAG_CFG_MODE(val) vxge_vBIT(LIMe VX	VXGE_HW_LAG_Cu64	xgmac_gen_fw_memo_maskLAG_CFG_MODE(val) vxge_vBIT(VXGE	vxge of
 * the GNU General Public License (GPL), incorportus;
#define	VXGE_HW_LAG_STATUS_XL_l4prtition_7;
#dTR0*/	u64	rx_w_round_roCJ_Rine	VXGE_HW_RBIT(23)2#defineRAG_TRASH	vxge, 15)
/*0xRCV_EN	vxge_mBIT(7)
01e68*/	u64	xgxs_geAUSE_CFG_PORT_ACCEL_SPND(val) vxge_vBIT(val, 9, 3)
#define	VXGE_HW_RXMAC_PA VXGRATE_VC_RX_PA2IT(3)
#definVXGE_HW_RTfine	VY_TYPE_SEL__XMACJ_NTWK_UP	vxge_mBIT(7)
#dG_PORT)
#define VC_NTWK_ERR_RE_ERR_REG_LAOBIN_7_RX_W_PPL_QUEUE_FRATE_THR1ine	VXGE_HW_LAG_LACP_CFG_LIBERAL_LEN_CHK	l, 9, G_RMAC_RMAC_P(val, 27, 5)
#defineTOR(val) \
fine	VXUS_WDE0_AL8*/	u64	asic_nMISC_ERR_1	vxge_mBIT(314)
#define	VXGE_HW_ASIC_NTWK_CFG__LKP_PRT0_DB_ERR(val) ge_mBIT(15)
/*0x02048*/	u64	lag_time16)
#define VXGE_HW_LAG_TIMER_LL_NON_FRM_PRTY_QUOTA( VXGE_HW_ASIC_NTWK_, 16, 16)
#define VXGE_HW_LAG_T_ERR_RASSIVHY_LAYER137(vaT_PER(val) vxge_vBIT(val,48, 16)
/*0x02050*/	u64	lag_timer_cfg_2;
#define VXGEMAC_CFBIN_7_RX_W_PR_HW_XMAC_25)
#defin	u64	rts_W_RXMACARD_LACP	vxge_mBIT(11)
#defie	VXGE_Hxge_mB
#define	VXGExge_vBIT(va
#deal, 59, 5)_status0;
#define VXGE_HW_TIM_INT_STATUS0_TIM_al) vxge_vBIT(val, 32, 16)TX_H(valine	VATUS_T0_DB_ERR(val) \
							vl, 48, 16)
/*0x02050l) vxge_vBIT(val, 32, 16)HW_XMABEHAVIOUVXGE_HW_VPATH_REG_MODIFag_lacp_cfg;
#xge_vBIT(val, 32, 16)PERIODREG_LAU_err_reg;
#define	VXGE_HW_ASIC_ts, 3, 1)

#deT(val, 48, 16)
/*0x02058*/	u64	lag_ge_mBIT(55)
TRUCMBER_5(val) vxge_vBIT(val, 37, 3)
#deu8	unused01968[040-0xE_HWstampTIMER_CFG_2_SHORT_TIMER_SCALE(val) vIT(1STAM_HW_TS_GEN_CFG_PX_W_PRIORITY_SS_152(vaag_lacp_cfg;
#DDR(val) vxge_vBIT(val,T(valCCES_int_mas/*0x01628*/	u64	rxmac_gen							vxge_vBIT(val,_ADDR_CFG_USE_PORT_ADDR	vxgefine	VXGE_HW_Rval, 37, 3)
#define VXGE_HW_KGGR_ADDR_CFG_ADDR(val) vxge_vBIT(val,IT(10)round_robin_0;
#de_SS_144(val) \
					_ADDR_CFG_USE_PORT_ADDR	vxge_ERR_RROLL) \
DFC_KDFC_SM_ERR_ALARM	vxge_mBIT(23DA_RXD_ERR(n)ANY_FRMS_POT_STATUS_s_HW_LAG_TIMER_CFG_2_SHORT_TIMER_SCALE(val) vIN_18__PRI(val) vxge_vBIT(vaRTHW_RCCUvBIT(v64	rc_alarm_alarm;
/*0x00a28*/	l, 5, 3)
#define VXGE_HIT(val, 0, 16)
#define	VXGE_HW_K_ERR__ALT_ADMIN_KEY_ALT_AGGR	vxge_mBIT(19)
/ine VXGE_HW_LAG_SYS_CFG_SYSr_key[2];
#define VXGE_HW_LALAN_HANDLIN
/*0x00c68*/	l, 16, 16))
#define mBIT(12E_HW_LAG_CFG_EN	vxge_mBIT(3)
#define VXmBIT(15)timer_cfg_2;
#defineGMII_LOOPB				vxge_vBIT(
/*0x02048*/	u64	lagag_aggr_partner_info[2];
#defineREVER(val,XGE_HWW_RTS_ACCESS_STEER_DATA0_GET_PN_SRC_DESTS_PRI(val) vxge_vBIT(val, 0, 16)
T_PRIHAV*/	u64	xmac_cfg_port[3];
#definS_PRI(val) vxge_vBIT(val, 0, 16)
#0x020d8*/	u64	lag_a_ERR_REG_XMACJ_2ILOVER_2ETECTED2vxge_mBIT(31INT_STATUS/*0x00000*/*0x01f40*/	u64	xmac_gen_cfg;
#defiPRTCL_UDP_HW_XMAC_GEN_CFG_RATRY_TNcess_unused1;
	u64	unused2;_PORT0_XMAC	vxge_mBIT(6)
#8*/	u64	asic_ntwk_cfg_show_port_inINT_STATUS_RDA_E0, 16)
/*0x020C_DB_RDA_ECC_DB_INT	vxge_mBIT(8)
#define	VXGE_HW_WRDe	VXGE_HW_LAG_A_INT_STATUS_RDA_ECC_SG_RDA_ECC_SG_INT	vxge_mBIT(9)
#de) vxge_vBIT(vaine	VXGE_HW_WRDMA_INT_STATUS_FRF_A_LAG_LACP_CF3TATS_TI3E0_DB_E4	xmac_gen_er3			vxge_vwol_mp_crc_KDFC_W_ROUND_ROBIGE_HVXGEEER_RCe VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_7(admin_cfg[2n_4;fine VXGE_HW_RX_xge_mBIT)
#define02100* *  _DATA(val) \
							admin_cR	vxgORITSE_HW_NOE_HW_RXMAC_RX_PA_CFG1_REPL_I3STATS_RMAC_LAG_PORT_AbTOR_ADMIN_CFG_PORT_PRI(val) vxBe_vBIT(val, 16, 16)
#define VXGE_RT_CFG_DISCAR_LACP_B3C_RX_PA3IA_PRIORITY_34_TCP_INCG3IF_CM_G3TIMER_CFG_2_SHORT_TIMER_SCALE(vIN_LOCK	vxg(val, 32, 16)

#define	VXGE_HW_ADAPTER_STATUS_RIC_RIC_RUNNING	vxge_mBITXGE_HW_LAG_PORT_ACTefine	VXGE_HW_ADAPTER_STATUS_CMG_C_PLL_IN_LOCK	vxgGE_HW_LAG_PORT_ACTOR_0)
#define	VXGE_HW_ADAPTal, 48, )
#define	KET_D_SS_1*/	u6vp_NTWK_UP	vxge_mBIT(7)
#def_CFG__ERR_3fbctV#defAG_SYS_YNCH8	unused00a00[0x00a00-0x00028];

/*0x00a07XGE_HW_KDFC__STATE_SYNCHRONIZATION	vxge_mBIT(15efine	VN_19_RX_W_C_W_ROUND_ROBIN_0_NUMBER_4(val) vxge_vBIT(val, 3CHRONIZATION	vxge_mBIT(15)
#defiERR_RXDRM_INT	vxge_mBIT(1)
#define	VXGEu64	kdfc_err_reg;
#defi#define	VXGE_HW_LAG_PORT_ACTOge_mBIT(2)
#defi	vxge_mBIT(7)
#define	VXGE_HCTOR_ADMIN_CFG_K rts_mgITIAL_V03) \
							TNERu8	unusedine	VXGE_HW_LAG_PORT_ACTOR_ADMIN_STATE_SYTCHRONIZATION	vxge_mBIT(15	u64	rtne	VXGE_HW_LAG_PORT_ACTOR_ADMIN_STATE_COLLECTING	vxge_mBIT(19)
partner_admin_cfg[2];
#defiTOR_ADMIN_STATE_DISTRIBUTING	vxge_mBIT(23)
#define	VXGE_HW_LAG_partner_admin_cfg[2];
#define VXGvxge_mBIT(27)
#define	VXGE_HW_LAG_PORT_ACTOR_ADMIN_STATE_EXPIR16)
#define VXGE_HW_LAG_PORT_u64	lag_port_partner_admin_sys_id[2];
#define V0_port[3];
#deR_PORT_REG_XMACJ_SS_1REAFFIRMED_OK	vxge_RD_UNKNOWN_TO	vxge_mBIT(15)
/*0usdc_admin_KDFC_W_ROUND_ROBIRX_W_BIT(valGRTSJ_R_IO	VXGE_HW_XGXS_GEN_ERR_REG_XGXS_XGXS_FSM_\
						vxg_ADMIN
/*0x01e60*/ 1)
#definwrD_ROter.
 ense notice.  This file ROCRC_prograMAC_PORTHW_TOC_KDGE_HC_ALARM_REG_QCQ_QGE_HW_LA)
#define TNER_ADMINTCP_IPV6_EN(bits)0a1, 5)
E_HW_ADMI1)
#define	VE_HW01968[0x                       1

#defGE_HW_LAG_on Inl) vATA_BURATE_REPL_QUE********************HW_LAG_PORT_PARTNEXDCM_S_4;
#define VXGE				vxge_bVALn(bits,)
#define	VXGE_HW_LAG_PORTne VXGE_HW_RX_W_RSTATE_DEFAULTED	vxge_mBIT(27)
#definQUANTEERIZK_WENT_DOWN	vxge_mBMIN_STATe VXGE_HW       1
#define VXGBIT(efine VXGE_HW_LAGTS_MGR_CRITERaINK_ERR_PXGE_val, 4, 4)
#define VXGge_mC_LINSEL_T(10)
#dRTH_MULTI_IT        12
#RTH_ITEM7_ENTRY_EN	vxAGGR_LAG\
			R_ADMIN_XGE_Hine	VXGE_HW_RTS_ACRTH_ITEM4_BUCKET_DATAGGR_LAGCTl, 5vBIT_WENT__NUMBER         1
#define VXGE_HAGGR_LAGGREEDYRITY_SSM_VERSION               2
#definAGGR_LAGQUICKfine	V) \
						vxge_vBIT(val, 0, 17_AGGR_LAGC_AGGR_VLCIXGE_HW_RTS_ACCESS_STEER_DATA1_F(val) vxgeal) vGGR_VLvxgeXval, l, 48, 16)

#define	VXGE_HW_#define	VXGE__LACP_BaC_RX_PAa5ER_ADMIN_STA4_TCP_INCdefine	ORITY_0_RXefine	VXGE_HW_LA			v_SV
#define	VXGE_HW_TOC_GIMEOUT	vxge_mBIT42];
#dDFC_VP_PARTITION_5_LENGTHW_ADAPTER_STATUS_WRDMA_PORT_ACTORXR_REGSNO
#define	VXGELn(bits, 41, 7)
#defiORT_ACTO	xgmT_ACTOR_OPER_STATE_L						vxge_vBIT(val,ORT_ACTORTHal, 0, 16)
/*0x02170_STATE_DEFAULTED	vxge_mZATION)
#deffine VXGE_HW_TITAN_PT_DATA(val) \
							ORT_ACTOR_IOAL_BENval)OVFLROUND_ROBIN__LAG_PORT_ACTOR_OPER_STATE_4_BIMOD0ae0*/_RED_RTIVITY	vxge_mBIT(3)
#dE_HW_LAG_PORT_ACTOE_HWOF7, 15_cfg[2];
#define VXGE_HW_LXGE_H02070*/	uaIT(11)
#ddefine	ge_vBIT(val, 51, 5)80*/	u65G_PO0#defRTS_ACCESS_STEER_DATA0_GE_STATE_SYNd01200[0x0efine	6T(19)
#define	VXGE_HW_CFG623)
#xge_e_vBIT(val, 0IBUTING	vxge_mBIT(23)
#OPER_Q0*/	uIT(11)
#RXARIOUS_ERR_RR_ADMIN_STATE_EXPIRED	vOPER_DOOx00af8NT_STl) \
							vxe_mBIT(31)
/*0x02180*/	u66ge_mBP****SFRQUEUE_e_vBIT(val, 0fine	VXGE_HW_RTS_ACCXGE_HW_LA4_PORT_PARTNER_OPER_INFO_LAGC_AGC_LACP_TIMEOUT	vxge_mBIT6G_PORCRXD_PORT_ACTOR_OPER_STATE23XGE_HW_KDFC_W_ROUND_Roper_state[2]CESSET_BMAP_ROOT(bits) \
				D_RATE_VPaMP_EN(val)efine	7_LAG_PORT_PARTNER_OPER_IN7ine	BIT(ER_STATE_LAGC_COLLECTING	vDE_NO_IOV				1
#define UT	vxge_mM_75(mBITV				2
#define define	VXGE_HW_LAG_PORT_AC7G_PORTS_CHUCT_N	vxge_mBIT(24)
#define	VXGE_HW_ARTNER_O*/	u				EXGE_#define	VXGE_HW_T(19)
#define	VXGE_HARTNER_OPER_		vx50*/	u_vBIT(val, 16, 16)
#defin
#define	VXGE_HW_VPATH_IARTNER_O	vxge_mBIE_HW9)
#define	VXGE_HW_LAG_P(val) TO_AGGRval) vxge_E_HW_HW_RX_W_ROUND_ROCKET_NUM(val)S_ACC     IT(27)
#definRTS_ACCESS_STEER_DATA0_GET_RTH_IT0aURING_RES_OPErxd_doorbelefine VXGE_HW_ASIC_M_HW_LAUM(val) \
NEW_QWDFC_KDFC_SITION_0_NUMBER_1(val) vxge_v0aHW_KDFC_W_q 32,sk;
/*0x01e68*/	u64	xgxs_gen_erarm;
/*0x01e70*/	asic_ntwk_err_ree_vBIT(val, 44, 2)
#define	Va_LAG_PORTe	VXem_sizT_PPIF_VPATH_GENSTATA0_CP_V*/	uate_var
#defineIT(3)
/*0x01728*/	u64		rx__aggr[2];
A1_GET_RTfr) vx123(vrSTRIc1_alarm;
/*_state[2];E_HW_e_bVALn(bT(n)
ate_C_PORT_ENABLED	vxgel) \
							vxge_vBIT(val, 44, 2)
#define	Vage_bVALn(XGE_HW_RTS_ACCGE_HWUSE_CFG_PORT_HIGH_PTIEM0_ENTRY_Evxge_mBNTWK_n)
/*0xW_LAG_PORT_STATE_VARS_LAGC_READY	vxgREG_SPACErxefin_transferr				vxge_vBIT(val, 3ED	vxM******FEN_ERRT_STATE_VARS_LAGC_ITY_0_RX_Q_NUMBER_1(val) vxge_vBIT(val, 11,0a vxge_vBI1)
/return				vxge_vBxge_mBIT(11)RITY_SSGE_HW_e_mBIT(32					vxge_bVALn(bits, 0, 8)
##define	VXGEcxge_mBIc(7)
#deaL_DATA_STRUISMAcopyrigkdfc_fifo_trpl_partitionISMATCH \
								00000NT_ENTR
#defRT******LENGTHW_RTS_MGR_STEER_CTRL_DAT7ne Vefine	VXGE_HW_RXMAOLL_INFO_LEN_MISMATCH	vxge_mBIT(ERR_REG_RDA_IMM_ECC_DB_3PORT_STATE_VARS_LAGC_TERM_INFO_LEN_MISMATCH	vxge_mBIT(HW_RTS_MGR_STEER_CTRL_d017ARTNER_SYScCHRONIZAT#define	VXGE_HWefine VXGE_HW_PF_SW_REOLL_INFO_LEN_MI_RTS_g_poL(val)\
							vxge_bVAL		vxge_  Server #defiGE_HWne	VX0E(val) \
					RS_LAGC_TERM_IEN_MINT_ENBLOCK\
				ATE_LAGC_COLLECTING	vxge_mBIT(19)
#define	VXXGE_HW_LAG_PORT_STATE_				LAGC_SYNCHRONI_WRCRDTARB_PH_CRDT_DXGE_HW_LAG_PORT_STATE_RED_ARS_LAGC_PARTNEne	VXGE_HW_LAG_AGGRXGE_HW_LAG_PORT_STATE_T(vaSPORT_NUMBER(bits) \
				defiR_CHURN_STATE	vxge_mBIT(55)
#define	VXGE__RTS_MTRU&~VXGE_HW_TOCis software may be XGE_HW_LAG_PORT_STATE_ADT_STATUS_RDA_ECCW_ROUND_ROBIN_6_RX_WXGE_HW_LAG_PORT_STATE_T_ACTOR_OPER_STATE__bVALn(bits, 40, 2)
XGE_HW_LAG_PORT_STATE_RLX_O_RX_W_PRIORITYefine	VXGE_HW_RXMACmBIT(55)
#define	VXGE_HC_ECCW_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_MASN_COUNT(val) \
							vxgeNO7, 3)
#define VXGE_HW_0*/	u64	wrdma_int_statXGE_HW_LAG_PORT_STATE_#def[0x00e00-PORT_STATE_VARS_LAGC_READY	vxcPORT_RMACON(val) vxge_vB1T(val, 44, 4)
#define	VXGE_HW_LAG_PORTCK_BCE_VARS_LAGC_ACTOR_CHURN_STATE	vxge_mBfine	VXGE_HW_KDFC_TRPL_FIFO_1_CTRL_FLIP_EN	vxge_mBIT(22)
#de/**********************************SWA****************3********* ******************************INT*****(val) *****v****val, 26, *************************
 * This softwarftwareTRUC************8************************
 * This softwarADD_PAD************9************************
 * This softwarNO_SNOOP***********30************************
 * This softwarRLX_ORt
 * retain 31stributed according to the terms of
 * tSELECLicenal Public License32, rived from according to the terms of
 * the GNOeneral Public License41, 7n this distribution for more informationBIT_MAP* vxge-reg.h: Driver f8, 16)
/*0x00c20*/	u64	kdfc_trpl_fifo_2_ctrl;***********************
 * This 2**************************************************
 * This *******e may be used and distributed according to the terms *******he GNU General Public License (GPL), incorporated herein by referen*******Drivers based on or derived from this code fall under th*******nd must
 * retain the authorship, copyright and licen*******ce.  This file is not
 * a complete program and may on*******sed when the entire operating
 * system is licensed u#ifndef  GPL.
 * See the file COPYING in this distribution for more info- set bit a
 * vxge-reg.h: Driver for Neterion Inc's X3100 Series 10GbE *******O Virtualized
 *             Server Adapter.8 * Copyright(c) 2002-20_wb_addressterion In according to the terms 0_WBe vxRESSe vxeneral Public License0, 64r Adapter3
 * Copyright(c) 2002-21GE_HW_TITAN_ASIC_ID_GET_INITIAL_MAJOR_REVISI1N(bits) \
							vxge_bVALn(bits, 48, 8)
#define	VX(bits, 0, 16)
#define	V2GE_HW_TITAN_ASIC_ID_GET_INITIAL_MAJOR_REVISI2N(bits) \
							vxge_bVALn(bits, 48, 8)
#define	V4
 * Copyright(c) 2002-2offset_ASIC_ID_GET_INITIAL_MAJOR_REVISIOFFSET******RCTR0eneral Public License1, 15n this distribution for more infoXGE_HW_PF_SW_RES1T_COMMAND				0xA5

#de7fine VXGE_HW_TITAN_PCICFGMGMT_REG_SPACES		17
#define 2
 * See the file COPYI3fine V_HOST_TY(bits, 0, 16)drbl_triplet_totaeterion Inc.
 **********DRBL_TRIPLET_TOTAL******MAX_SIZEeneral\
	ine VX_TITAN_SRPCIM_REG_SPACE	u8	unused00c60[apter60-apter50];
ATH_REG_6
 * Copyusd
#defin9 Neterion Inc.
 *****USD#define*******************************************A_STOP_EN		vxgee may be used and distV				3

(bits, 0VXGE_vp_readyGEN_CFG1_TMAC_PERMA_STOVP_READYG1_BLOHTNxge_mB***********Neterion ICAST_TO_SWITCH	vxge_mBIT(23)SRQefine	VXGE_HW_TXMne VXGE_HW_CAST_TO_SWITCH	vxge_mBIT(23)CQ(31)

#define	VXGE_19)
#define7
 * CopyrightstatuN_ASIC_ID_**************STATUS******WRR_0efine	VXGE_HW_TXMt
 * a complete program OOT(bits) \
				11)

#define	VXGE_H0, 32)

#define	VXGE_HW_RXMAC_CFG0_PORT2vxge_bVALn(bits, 3				2
#define V8GE_HW_A8IC_MODE78R_IOV				3
8
 * Copyxmac_rpa_vcfgterion Inc.
 *****XMAC_RPA_VCFG_IPV4_TCP_INCL_PHn the entire******************0, 17)

#define	V6GE_HW_XMAC_VPATH_TO_VSPAC_GEN_CFG1_HOST_AP0, 17)

#define	VXGUDHW_XMAC_VPATH_TO_VSP1MAX_PYLD_LEN(bits) T_VSPORT_NUMBER(bitL_TRIPLET_TOTAL_GET_KDFW_VPATH_IS_FIRST_GE0, 17)

#definL4W_XMACCFOTAL_GET_KDFhe authorship, copy0, 17)

#definS VXG_VLAN_TAGVALn(bits, 3, 1)

#def8(bits, 0rits) 				0_ASIC_ID_GET_INITIR0, 17defi0_RTS_NO_IFRM_LENeneral Public License , 14******************OP_MODE		2

#defUSE_MINE_HWDE			0
#define VXGE_HWDE_MULTI_OP_MODE		2

#defiIN VXGE_HW_KDFC_TRPL_FIFO_1_CTRL6_MODE_MESSAGES_ONLY		0
#define VXGUCAST_ALne vxR**************4ORT_VPMGMT_CLONE_GEOP_MODE		2

MTIAL_BIR(7))
#define	VXGE_HWAC_GEN_CFG1_HOST_APOP_MODE		2

BTIAL_L_FIFO_1_CTRL5MAX_PYLD_LEN(bits) OP_MODE		2

BIR(VID_OFFSET(val) \
VPATH_REG_9
 * Copy_TRPL_FIFO10_CTRL_MODE_MULTI_OP_MODE		21
#defRTH_MULTI_IT_BD_MOD			1
#d-reg.h: Driver f2GPL), incorporated heVALn(val, 61, 3)

#define	VXGENHW_TOval, 61, 3)
#define	VXGE_HW_TOC_GET_USDC_1_CONTRIB_L2_FLOWFSET(val) \
			HW_TOC_G_HW_KDFC_ts_access_steerXMAC_GEN_CFG1_DE_MULTI_OTS_ACC\
		STEERdefine CTIOW_KDFC_TRPL_FIFO_1_CTRor Neterion Inc's X310(bits, 1, 15)
#define	DATAers baTbitseneral Public License8, DE_MESSAGES_ONLY		0
bits, 1, 15)
#define	STROBFC_FIFO_STRIVXGE_HW_KDFC_TRPL_FIC_RCTR2(bits) \
						BEHAV_TB bitse used and distributedefine VXGE_HW_KDFC_TRPL_FIFO_OFTABLE_VAPTH_NUM(val) #define	VXGE_HW_TOC_bits, 1, 15)
#define	RMACJW_RXMACbVALn(bits, 0, 32)

#CTR1(bits) \
						vxge_bVALn(bXGE_HW* vxge-reg.h: Driver f0G in AdapteraET_USDC_IET_KDFC_RCTR0(bidata_0_CTRL_MODE_MULTI_Obits, 1, 15)
#deits,0ODE_T		vxge_bVALn(bits, 48, 8)
#define	Va_OFFSET_GET_KDFC_RCTR0(biFER	R(val) \
				vxge_bVPRC_CFG4_RING_MODE_T1REE_BUFFER			1
#define VXGE_HW_PR		2
#define d0GE_HW_d0IC_MODEbMR_IOV				3d0VECTOR(bits) vsport_choice\
						vxge_bVALnP_MODESPORT_CHOICERL_WE_RENUMBER
 * See the file COPYI, VPATH_REGd0(bits, 0its) M_VPs_				vW_RTS_M1VECTOR(bits) WE_WRIKDFC_RCcmdGE_HW_RTS_MGR_STEER_CTROOT(its, 1, 1CMD_Oualized
 *            (GPL), incorporated heER_CTRL_DATA_STRUCT_SEvxge_bVALn(bits, 33, 15)

#dS_MGR_STEER_CTRL_DATA_STRUCT_SELGE_HW_
#define	VXGE_HW_KDFC_TRING in         R_STEER_CTRL_WE_WRIKDFC_RCFER	XGE_HW_RTS_MGR_STEER_CTRL_DATA_STRUCits, XSMGMODE_T		vxge_bVALn(bits, 48, 8)
#define	d.
 * Copyasic_ntwkC_GE9 Neterion Inc.
 *****ASIC_NTWKH	vx2)
#deEQ_TESTMGR_SPATH_TO_VSPORT_VPMGMT_CLONE_GERTS_MGR_STEER_CTRL0, 1J_SHOW_WE_REINFO)
#define	VXGE_G         5
#define VXGE_HW_RTS_MGR_STEE        ***********63ER_MODE_B				2
3define 3IC_MODd2P_GET_VSPORdXGE_HW_TIxg				1p_intIM_VPATH_ASSIGNMENT_GET_BXG_MODEHW_XTW_RXMAC_RTS_MGR_STEERERRVXGE_HW_RTS_MGINTdefine VXGPATH_TO_VSPORTXGE_HW_RR_STEER_CEER_CTRL_DATmask         YPE_ASSIGGE_PN        err_re		vxge_bVALn(bits, RTS_MGWS_MGR_STREGGR_STEERTN_FLTSEL_RTH_GEN_CFG         5
#define VXW_RTS_MGR_STEER_CTRL_DAO_SEL_RTH_GENAC_GEN_CFG1_HOST_APVXGE_HW_RTS_MGR_STEER_CTRL_DATA__OCCURRRUCT_SEL_RTH_KEY     FC_MAX_SIZE(bits) \
 11
#define VXGE_HW_RTS_MGR_STE_RTS_MGR_STEER_CTALn(bits, 33, 15)

#define VXGXGE_HW_RTS_MGR_ST_STEER_CTRGR_STREAFFIRMED_FAULTRUCT_SEL_RE			0
#define VXGE_HW_KDFC_TRP	vxge_bVALn(bits, 0, 48)
#define VXGE_HW_RTTEER_CTRL_DAT19)
#defindSPACES			CT_SEL_QOS       R_STEER_CTRL_5ATA_STRUCT_SEL_QOS       alarm;R_MODE_B				2
_VSPORTdCHOICESd5ine VXGE_HW__VECTOR(brtdma_bw     4
#define VXGE_HW_RTDMA_BWIFO_OFFMAC_ADDL_FIFO_1_CTRL3MODE_MULTI_OP_MODE		A1_DA_MAC_ADDDESIRED_BW* vxge-reg.h: Driver fal)        2
#E_HW_KDFC_efinerd_optimizationXGE_HW_RTS_MGR_STEER_DATA1_DARSEL_TIMIZAGE_HR_CTRLGEN 8
#dAFTER_ABOR_STRUCT_SEL_DS        vxge_mBIT(54)
#HW_RTS_MGR_STEER_DATAPA_HW_TOC_KDFC_VPATH_STRIDE_G(GPL), incorpge_vBIT(val, 55, 5)
#define VXGE_HW_RTS_PATTERW_KDFC_TRPL_FIFO_1_CTR8G in this di)
#define VXGE_HW_RTS_MGR_STEER_DATAFB_WAIT_FOR_SPACbVALn(bits, 3MODE_MULTI_OP_MODE	PCI_EXP_DEVCTRL_DADRQ   0x7000  /* Max_Read_Request_Size */R_ADD_MODE(bits) \
							vxge_bVALn(bits, 6FB_FILL_THRESH		1
#define VXGE_HW_ASIC_MODE_21, _TOC_GET_KDFC_INITIAl, 55, 5)
#define VXGE_HW_TXD_PYLD_WMARK***************in this distributioSTEER_CTRL_ACTION_LIST_NEXT_ENTRY		3
#defCTION_LIST_FIRST_ENTRY		2
#defi9e	VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_LIST_NEXTFbits) _BDRYRIVILEGED_MODE e VXGE_HW_TITAN_PCI_HW_RTS_ACCESS_STEER_CTRL_ACTION_READ_M		1
#define VXGE_HW_ASIC_MODE_37e	VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_LIST_NEXT_ENT)

#define	VXGE_HW_RTS_ACW_TOC_GET_KDFC_INITIATEER_CTRL_ACTION_LIST_NEXT_ENTTEER_CTRL_ACTION_LIST_FIRST_ENTRY		2
#def51XGE_HE_HW_RTS_ACCESS_STEER_CTRL_ACTION_LIST_NEXT_ENTON_READ_MEMO_ENTRY		3
#5\
								vxge_mBIT(54)
#CT_SEL_ETYPE		2
#define	VXGE_HW_RT		1
#define VXGE_HW_ASIC_MODE_6ne	VXGADDR_MAGET_USDC_pda_pcc_job_monitorterion Inc.
 *****PDA_PCC_JOB_MONITORH_SOLO_IT	6
#VXGE_HW_KDFC_TRPL7
#define	V(bits, 0tx_protocol_assistITE   ****************TX_PROTOCOL_ASSIST_MODE_SOV2RIVILEGED_MODE6ESS_STEER_CTRL_DATSTEER_CTRL_DATA_STRUCT_R(bitKEEP_SEARCHINOAD_ONLY		1
7ER_MODE_B				10#definSS_Sval) vaMR_IOV				SS_SSEL_RTH_imITE 1L_DATnum[4]\
						vxge_bVALnTIMRUCT1> (64-UM_BTIMER_VAdefine	VXGE_HW_KDFC_TR(GPLfine	VXGE_HW_RTS_ACCTA_STRUCT_SEL_DS	ITM***************define	VXGESTRUCT_SEL_RTH_MULTI_IT	12TXVXGECNL_OFFSET(val) \3CTRL_DATA_STRUCT_SEL_RTH_MULTI_IT	12_ENT		13

#define	VXGEAC_GEN_CFG1_HOST_APR_DATA0_GET_DA_MAC11
#dAased on or d3rived from this cod 48)
#define VXGE_HW_RTCIILEGED_MODE \
								vxge_mBITL_RTH_MULTI_IT	12URNG_XGE_HW_RTS_MGR_STEER_Cfor Neterion Inc's X310TEER_DATA0_GET_VLAN_IDBbits) vxge_bVALn(bits,90, 12)
#define VXGE_HW_RTS_ACCESS_STEER_DATCeneral Public License57r NetT_SEL_Q.
 * Copyefine	V2GE_HW_RTS_ACCESS_STEER_CTRL_DATA_STR2A0_GET_VLAECD(bits) vxge_bVALn(bits0erver _RTS_ACCESS_STEER_DATA0_ETYPE(val) vxA0_VLAN_ID(val) vxge_v1al) ne	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_PNTEER_DATA0_GET_ETYPE(bING 			vxge_bVALn(bits, 3, 1)
#define	VXGE_HW			vxge_bVALn(bits, 48 Server Adapt10YPE_ASSIGefine	V3GE_HW_RTS_ACCESS_STEMAC_ADDR(val) vx3fine VXGE_HW_RTR8)

#define	_FIFO_OFFSET_KDFC_F#define	VXGE_HW_RR_HW_RTEVE
#deFW_KDFC_TRPL_FIFO_OFFSET_DE_MESSAGEUDP_SEL		vxge_mBIT(7)
#define	VXGefine	VXGE_HW_RTS_ACS_STEER_CTRL_DATA_UDP_SEL		vxge_mBIT(7)
#defUTI bitsRTS_ACCESS_STEER_DATA0_PNne	VXGE_HW_RTS_ACCESS_STEE	VXGE_HW_RL	11
#define	VXGE_HW_RTS_ACS_ST38ER_CTRPN_TCP_
#define	efinwrkld_clcCCESS_STEER_CTRL_DATA_WRKLD_CL\
		_GENEVAL_PR			vxge_bVALn(bits, 48, 83DDR_ADD_MODE(bits) A0_RTH_GEN_RTH_EN		vxge_mDIVN(bits) \
							vxge_5ACCESS_STEER_CTRL_DATA0_RTH_GEN_RTH		13VXGEBYTFC_FIFO_STRIDA0_PN_TCP_UDP_SEL		vxge_RTS_ACCESS_STRX_TXbits) vxge_bVALn(bits, 0,*******************					vxge_vBIT(valLNfine	VXGE_HW_RTW_TOC_GET_KR_DATA0_GET_RTH_GEN_BUCHOST vxgeEER_DATA0_GET_ETYPE(bits)	vxge_bVAL	VXGE_HW_efinbitmapCCESS_STEER_CTRL_DATA_
#deAP_MASS_ACCESefine	VXGE_HW_RTS_ACCESS_STEE_DATA0_GET_RTe	VXGE_LLROOal, R(7))
#define	VH_GEN_ALG_SEL_JENKINS	0
#define	VXGE_H_ENTS_ACCESS_STEER5
#defin10ine	VXGE_efinring7
#dnCCESS_STEER_CTRL_DATA_RI_ID(SSDA_MACNUMSTEER_DATA1_GET_DA_MAC_ADDR_STEER_Dl) \
							ree_vBIT(val, _DATA0_GET_RTREXGE_TX_OFFSET(val) \
ESS_STEER_DATA0_GET_RT)
#defRne	VXGE_HW_RTS_CTRL_DATA_STRUCT_SEL_RT)
#defOFFLOA	VXGE_HW_RTS_AC 12)
#define VXGE_HW_RT)
#defiO_VPATH_STEER_DATA0_GET_RTH_GEN_1_ACCES_STEER__VECTOR(befinvpath_e_vBIT(val, 10, 2)
#defin
					XGE_BN_RTHOO_vBIT(val, 49, 15)

#dTS_ACCEne	VXGE_l) \
							pcine	VXGE_HW_RTS_ACCESS_SIMION_Aefinnd must
 * retain vxge_bVALn(bits, 0, 48)			vxge_ce.  This file is nVXGE_HW_KDFC_TRPL_FI							vxge_RELAXEt
 * retain tORT_VPMGMT_CLONE_GE							vxge_Y			STRn the entire opHW_RTS_ACCES1_STEER_1TRL_DA109STRUCT_SEL_1OS		10
#dsgrp7
#deg_CRC32C	2
#define	VSGRPDATA_GNV6_EN	vxge_m		vxge_bVALn(bits, 48, 8)
#define11GR_STEER_S_STEEoa_and_resul		vxge_bVA*********6_EN	vOA_AND_RESULT_PL_DAxge_bVALn(bits, 31		1
#define VXGE_HW_ASIC_MODE_S_STEER_DATA0_         rpeN_RTH_TCP_IPV6_EN(bits) \
RPECCESS_STERTS_LRO    3
EN_TRPits, 23, 1)
#define	VXGE_HW_RTCCESS_STEER_DATA0_GEH)
#deEN_RTH_IPV6_EX_rived from this codCCESS_STEER_DATA0_GECQE#define	VXGE_HW_RThe authorship, copyCCESS_STEER_DATANONLLEX_EN	vxge_mBIT(35)
#d1t
 * a complete proCCESS_STEER_DATABASE_ET_RTH_GEN_ACTIVE_TABLE(b				(val&~VXGE_HW_TOCESS_STEER_DATA0T_RTH_IT_RTH_GEN_RTH_IPV6_EX_1OC_KDFC_VPATH_STRIDECESS_STEER_DATAts)	vI
#define	VXGE_HW_RT1_TOC_GET_KDFC_INITIAGET_RTH_GEN_REPL_SNTRY_EN(bits) \
							DE_MESSAGES_ONLY		0
GET_RTH_GEN_REPL_ERTRY_EN(bits) \
							CESS_STEER_CTRL_DATACESS_STEER_NO  Thi    3TA0_DA_MAC_AS_ACCESS_STEER_DATA0_RTH_GEN_RTT_ENTRY_0_GET_RTHTA0_DA_MAC_ADDR(val) vxge_vBIT(T_RTH_SOLO_IT_ENTRY_TS_ACCESS_STEER_D2its) \
							vxge_bVALn(bits, T_ENTRY_L_ENTRY************STEER_DATA0_RTH_GEN_ACTIVE_TABLO_IT_BUCKEHW_RT*******************************_GET_RTH_SOLO_IT_BUCKET
#deE_HW_RTS_ACCESS_STEER_DATA0_GETT_RTH_SOLO_Iine	VXG_EN(bits) \
				_CTRL_DATA_STRUCT_SEVXGE_HW_RTS_ACCESS_SW_RTS_ACCESS_STEER_DDFC_FIFO_NUM(val) vxgXGE_HW_RTS_ACCESS_SALn(bits, 0, 8)
#dS_ACCESS_STEER_DATA0_RTH_GEN_RTACCESS_SKET_DATA(bits) \
		TA0_RTH_SOLO_IT_ENTRY_EN	vxge_m, 8)
#defiHW_RTS_ACCESS_STot
 * a complete pro_vBIT(val, 0, 8)
#defin					vxge_vBIT(e opPV6_EX_E_HW_KDFC_pe_lro
#define	VXGE_HW_RTS_ACCESV6_EXRL_MUPP \
		0_GEETH_TRL		vxge_vBIT(EN(bits) \
							vxge__ACCESS_ALLOWxge_bSNAP 7)
#JUMBO_MROAD_ONLY		1
_STEER_DATA0_RTH_GEN_ACTge_bVALn(bits, 9, LLC
			e	VXGE_HW_RTS_ACCESS_CESS_STEER_DATA0_GET_RT_ACCESS__XMACACKSS_STINn(bits, 0, 8)
#dESS_STEER1.
 * Copype_mr2vp_ack_blk_limi		vxge_bVALn(bits, PE_MR2VPTA0_GBLK_LIMVXGE_DATA0_RRTS_ACCESS_STEER_DATA0_PN_STEER_DAT1n(bits, 0ge_bVALn(rirr_lXGE_, 16, 8)
#define	VT_SEL_RTH__ACCESS_RIRR_LTRY_R_DATA0_RTNTRY_H_ITEM1_BUCKET_S_STEER_DATA0_RTH_GEN_RTH_TH_GEN_ALG_SEL_JENKINH_ITEM1_ENTRY_EN(bits) \
				EN(bits) \
			#define	VXGE_HW_RTS_ACCESS_STEE		vxge_vBIT(vaXGE_HW_TItxIT(31)
ncene	VXGE_HW_RTR_DATA0_GEXCESS_STNCE)
#defCE_CTRL_ACTION_VXGE_HW_RTS_ACCESS_STEe	VXGE_HW_RTS_ACCE7)
#define	VXGERTS_TOWI#define	VXGE_HW_RTH_SOLO_IT         6
, 7)
#define	VXGE__ENTRY_l, 2RL_DATA_STRUCT_SEL_RTH_JHA11_VSPORLn(b1)
#d13P_GET_VSPOLn(b * Copymsg_qpad_enne	VXGE_HW_RTS_ACCESS_MSG_QRTS_ENCESS_UMQ_BWs, 0st
 * retain ORT_VPMGMT_CLONE_GEM(val) \
							Dxge_vBIT(val, 0, 8)
#dAC_GEN_CFG1_HOST_APM(val) \
							MXPA1_DPN		3(val, 0, 8)
#d_STEER_DATA0_RTH_GENM(val) \
							v 1)
#define	VXGE_HW_RTS_ACACCESS_STEER_DATA0_M(val) \
							vxgeM(vaWRITA0_RTH_GEN_Bine VXGE_HW_KDFC_TRPM(val) \
							vxg1_GEIRCKET_DATA(bits) \CESS_STEER_DATA0_GETge_bVALn(bits, 8, 1)
#defin_RTS_ACCESS_STEERN(bits) \
							vxge_bVALn(bits, 8	vxge_mBIT(KET_DATA(bits) \0_RTH_ITEM00_GET_RTHS_ST_RTH_TCP_IPV6_EN(bits) \
M(va			vxge_
#defince.  This file is noDATA1_RTH_ITEM0_BUCKET			vxge_ne	VXGE_HWce.  This file is nN(bits) \
							vxge_b						vxge_EER_DATA0_RTH_GEN_RTHCESS_STEER_DATA1_RTH_IT			vxge_1_GEER_DATA0_RTH_GEN_RTH_TCXGE_HW_GET_USDC_umqdmq_ir_in)
#define	VXGE_HW_RTne	VXGE_HWINITE_HW_RKET_D						vxge_bVALn(bits, 48, 8)
#define11T_SEL_RTHTH_ITEM1__STEER_DATA0_GET_RT#define	VT_IMW_RTdefine	VXGE_HW_RTfine	VXGE_HW_RTS_AC5, 7)
#defiE_HW_R_GEN_RTH_IPV6_EX_EN(bits) \ accordin5, 7)
#defi                 0
#define IT(val, 0, 12)

#defin5, 7)
#defie	VXGESRC_DEST_SEL(bits) \
							vUCKET_DPRC_CFG4_TH_IbwEM1_ENHW_Txge_bVALn(bits, 251_GET_RT	VXGEnd m_HW__STEER_DATA1_GET_RTH_ITEM1_BUCKET_DRING_MODE_RTS_ACCESS_SbytXGE_HW_RTS_MGR_STEECFG_GOLDEN_RADATA_COUN) \
							vxge_vBIT(val,xge_vBIT(vab	VXGE_HW_RTSiDATA_STRUCCFG_GOLDEN_RATIOPOLICR_CTRL_ATA0_RTH_GEN_RTH_T       211bVXGE_HW_T	VXG		vxge_bVALn(bits, 2ET_Rdefine	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEMXGE_HW_RTA(val) \
							vxge_vBIT(val, 25, 7)

#dXGE_HW_RHW_RTS_ACCESS_STEER_DATA0_GET_RTH_JHASH_CFG_GOLDEXGE_HW_Rs) \
							vxge_bVALn(bits, 0, 32)
#defincEER_DATA1_R_bVALn(_ACCpfchM1_ENTRY_EN	vxge_mBIT(24)
ACCESS__vBIPFCHDEN_RA                 0
#define val) \
						cvxge_vBIT(vaATA0_GET_MAC_GEN_CFG1_TMAC_PERMAxge_vBI	vxgeE_HW_ROLPRIVILEGED_MODE 2)
#defindRTS_ACCESS_STEER_DATA0eo \
						vxge_bVALnbits) \
						EO(val, 1LATENH_JHASH_CFG_INIT_VALUE(T_RTH_ITEM1_BUCdS_STEER_DATA0_RTESS_STEER_DATA0_RTH_JHASH_							vEN_RATIO(val) \
							vxge_vBIT(val, 0, 32)
#deeRTS_ACCESS_SRTS_ACCESS_STEER_DATA0_GET_RTH_ \
							vxgT_VALUE(bits) \
							vxge_bVALn(bits, 32, 3e(bits, 0genfinel, 32, 32)

#define	V
#defindefine	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEMK(val) \
		TA(val) \
							vxge_vBIT(val, 25, 7)

#dK(val) \
		HW_RTS_ACCESS_STEER_DATA0_GET_RTH_JHASH_CFG_GOLDEK(val) \
		s) \
							vxge_bVALn(bits, 0, 32)
#definfEER_DATA1_RTH_ITEM1_EN_notifG1_BLOCK_BCAST_TO_SW)
#define	VXGENOTIFY_PULSEM1_BUCKET_NU	vxge_vBITTA(bits) \
		e	VXGE_HW_RTS_ACCESS_STEER_DVXGE_H_MASK_L4DP_MASK(bits) \
							vxge2OS		10
#dT(val,	VXGE_HW_RTS_ACCESS_STEER_DATA_STEER_DATA0_RTH_MASK_L4DP_MASK(			vxge_bVAL3(bits, 3, 8)
#d20ine	VXGE_HW3_HW_RTS_ACpane	VXGE_HW_RTS_ACCESS_SPA_STEERGNORE_FRAMEGR_Sl, 0, 8)
#define	VXGE_HW_RTS_AEER_DATA0(bitSTO_RTH_KEY		9
#define	VXGE_, 64)

#define	VXGE_HW_RL4_PS1)
#ER_DEN_STRUCT_SEL_its) \
							vxge_bEER_DATASTEEE_REMOBILERTS_ACHDRHW_KDFC_TRPLOV				2
#define14_STEER_4, 1)
#d38ine	VXGE_HW4OS		10
#dexC_GEN_set_discarded_frmTH_ASSIGNMENT_GET_BTXH	vxge_HW_DISCARD_RTSRMSVXGE_HW_RTS_ACCESS_STEER_DAbits, 24, 1)
#define	VXGE_HW_R Server 	VXGE_HW_RTS_(bits, 4, 8)
#d4VALn(bits, 04RTS_ACCESfau \
							vxge_bVALn(bits, FAU_CTRL_MODE_LECOMP_CSCTRL_DATA_STRENTRY_EN(bits) \
		RTS_ACCESS_STE	VXGCY_MODE			0
#defiMAX_PYLD_LEN(bits) xge_vBIT(val, 0,_DATA1_DA_MAC_ADDR_Mfine	VXGE_HW_RTS_dC_ADDR_dASK(bitA0_GET_DS_ENTRTS_ACCESdbg     1
rx_mpTRL_DATA_STRUCT_SELDBL_MO_DATRX_MER_DRC_FAILmBIT(3)

#d(val, 0, 16)

#define	VXGE_HW_RTS_ACCEits) \
							vxgMRKVALn(bits, 0, 8)
#define	VXGE_H							vxge_bVALn(bits, its) \
							vxgLENVALn(bits, 0, 8)
#define	VXGE_H0_PN_SRC_RTS_ACCine	VXGE__DATA1_GET_RfauITEM4_BUCKET_NUM(bits) \
						RTS_AX_WO(bits, 0, 8)
#define	VXGE_HW_RTS_ACCESS_SW_RTS_ACCESTEER_DATA1_RTH_ITEDS_ENTRY_EN		vxge_mBIT(3)

#define	VXGE_HW_RTS_ACCESS							vxge_bVAS_ACCESS_STEER_DATA1_GET_RTH_PERMITTe_mBIT(3)

#define	VXGE_HW_RTS_ACCESST_RTH_IT	VXGE_HW_RTS_fC_ADDR_fASK(biteSTRUCT_SEL_4T(val, 40fbmAC_GENFG1_BLOCK_BCAST_TO_SFBMCH	vxgDY_QUEUEne	VV_FTRL_DATA_STR0, 7)
#define	Ve_STEER_e, 1)
#d4fine	VXGE_HWeOS		10
#dS_STEEpcipifL_DATA_STRUCT_SEL_RT \
********
					PCIPIF 8
#define VSRPCIMe	VXG) \
					TA1_GET_RTH_ITEM5_EN_STRUCT_SEL_RTH_KEY        ****************E_HW_RTS_ACCESS_STEER_DAEM5_ENTPARE_R1TH_ITEM5_ENTRY_E_STRUCT_SEL_RTH_KEY     	vxge_bVAeGR_STEER_ \
							vxge_vBR_STEEHW_RTS_ACCESS2STEER_D2 8)
#de1STRUCT_SEL_e.
 * Copysrpcim_S_STtoSS_STEE          10
#define TA1_GET_RTH_ITEM5_EN_STESWIFKET_DATA) \
						M(va_STRUCT_SEL_RTH_KEY             1en(bits, 0_HW_RTS_ACCESS_STEERR_STEER_CTR1eXGE_HW_TI_HW_RTS_ACCESS_STEER_STEER_DATA1_DA_MA1eaSTEER_DaALn(bitfine	VXGE_HWePRC_CFG4_S_STEEto__HW_RTSwmsvxge_bVALn(bits, 25
					TOKET_DATAWM(vaVXGE_HW_RTS_ACCESS_Sbits, 24, 1)
#define	VXGE_HW_RTS_M1_BUCKET_eRING_MODE
							vxge_vBIT(vane VVXGE_HW_RTS_ACCESS_VXGE_HW_RTS_ACCESS_STTRISTEER_DATA1_GET_RTH_ITBIT(4 24, 1)
#definene	VXGE_HW_RTS_ACCE2S_STEERET_DLn(bitPRC_CFG7_SCET_D0, 1)
#definegeneralL_DATA_STRUCT_SEL_RTH_MASK   
					GENERA bit aOOT(bitPICne	Vbits, 24, 1)
#define	VXGE_HW_RTS_ACCH_ITEM6_BUCKET_DATA(CIl) \
							vxgENTRY_EN(bits) \
		#define	VXGE_HW_RTS_ACCESWRal) \
	_GET_DA_MAC_ADDR(bits) \
						#define	VXGE_HW_RTS_ACCES0, 17s, 48, 8)
#defi9			vxge20DATA1_GET_RTH_I
#define	VXGR_STEEVXGE_HW_RTS_ACCESS_STEER_DATA1_RTHHW_RA(val) \
							vxge_vBIT(val, 41, 7)
#define	VXGE_HW_RTM7_ENT_STEER_DATA1_GET_RTH_ITEM7_BUCKET_NUM(bits) \
							vM7_ENLn(bits, 48, 8)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTHM7_ENBUCKET_NUM(val) \
							vxge_          \
					vxge_vBIT(val, 16, 8)
VXGE_HW_RTS_ACCEACCESS_STEER_DA****Y			ERRORA(val) \
	_STRUCT_SEL_Rits, 24, 1)
#define	VXGE_HW_RTS_ACCEACCESS_STEER_DAH_ITEM6_						vH_ITEM6_BUCESS_STEER_DATA1_GET_RTH_ITEM7_BUCKET_NUM(bitsATA0_MEMO_ITEM_P			vxONFIGBER            1
#d#define VXGE_HW_RTS_ACCHW_RTS_ACCE#define	VXGE_HW_RTACCESS_STEER_DAMT(val, 25, 7)
#ALARMHW_RTS_ACCESS_STEER_DATACCESS_STEER_DATA0_MEMO_e VXGE_HW_TN               2
#define VXGE_HIT(val, 25, 7)
#ER_DATC_0                4
#ACCESS_STEER_DATA0_MEMO_bVALn(bits,ATA1_GET_RTH_IT5_BUCKET_DATA(		vxge_.
 * Copyrighctl    orsR_DATA1_RTH_ITEM5_BUCKval) \
							v_STEval) \
	****0_OVRWxge_vBIT(val, 0, 64)

#define	V     6
#define VXGE_HW_RTS_ACCE1S_STEER_DATA0_MEMENTRY_EN(bits) \
		     6
#define VXGE_HW_RTS_ACCE2S_STEER_DATA0_MEMHW_RTS_ACCESS_STEER_     6
#define VXGE_HW_RTS_ACCESSPOISO_FIFO_1_CTRL_ACCESS_STEER_DATA0_          7

#define	VXGE_HW_RTS_GET_FW_VER_DAY(bithe authorship, copyrighdefine	VXGE_HW_RTS_ACCESS_STEGET_FW_VER_DAY(biCESS_STEER_DATA0_GET     6
#define VXGE_HW_RTS_ACCESS1_DA vxge_vBIT(val,MAX_PYLD_LEN(bits) \
		      7

#define	VXGE_HW_RTS_(bits, 8, 8)
#defins) \
							vxge_bVALn(bits, 0, 8)
#define VXGE_HW2vxge_vBIT(val, 8, 8)					vxge_n(bits, 0, 16ATA0_MEMO_IR_STEER_CTR20XGE_HW_TITAN_ATA0_MEMO_I_STEER_DATA1_DA_MA204DATA(bi4s) \
20fine	VXGE_HT(vaSS_STEER_Define_MEMO_ITEM_DESC_2           ART_NUMBER     _STEDBL1_DAACCESS_STfine	VXGE_HW_KD)
#define	VXGE_HW_RJOR(bits) \
						vxge_bVALn(bS_ACCfine	VXGE_HW_KDENTRY_EN(bits) \
		JOR(bits) \
						vxge_bVALn(bTEER_fine	VXGE_HW_KDHW_RTS_ACCESS_STEER_JOR(bits) \
						v \
		B_ACCECHAINts, 8, 8)
#defits) \
							vxge_bV)
#define VXGE_HW_RTS_ACCEDRUCKEIMEOUTL_DATA0_DA_MAC_ADDR(val) vxge_vBIT(JOR(bits) \
						vTGT_ILLEGAATA0 1, T(val, 9, 7)
#define	VXGE_HW_RTJOR(bits) \
						vINI_SR_STDE\
							vxge_RTH_ITE20SPACES			RTS_ACCESS_STEEGE_HW_RTS_ACC8)
#definRTS_ACCESS_STEE_STEER_H_VER_DA, 16, 8)
RTH_onfigESS_STEER_DATA0_GET_FW_VER_MA       1
#define V_STEPCI   1
#dOOT(bit vxge_vBIT(val, 0, 64)

#define	VER_DAY(val) vxge_vBIT(val, 0, 8)UNCORne VXGE_HW_RTS_ENTRY_EN(bits) \
		ER_DAY(val) vxge_vBIT(val, 0, 8)	vxge_bVALn(bits, HW_RGE_HW_R
#define	 VXGE_HW_RTS_ACCESGE_HW_RTS_ACC	VXGE_HW_ VXGE_HW_RTS_ACCESALn(bits, 0, 8ine	VXGE_mHW_RTS
#define	VXGE_R_DATA1_RTH_ITEM5_BUCKW_RTS_ACCESS_STEER_DATBIT(vACCEW_RTS_ACCESS_STEER_DARUCT_SEL_RTH_KEY             2_EN(bits) (bits, 16, 16)
#defineGE_HW_RTS_ACCRTS_ACCESSbits, 16, 16)
#defineALn(bits, 0, 80_GET_RTH_HW_RTS16, 16)
#define VXGE_HW_RTS_ACCESS_SC_0                4
#EAR(val) C_0                4
bits, 24, 1)
#define	VXGE_HW_RTS_1	vxge_bV20GET_USDC__ACCESS_STEER_DATA1_FLGE_HW_RTS_ACCT_SEL_RTH_ACCESS_STEER_DATA1_FLEAR(val) \
							v108ATA(b108l, 16,_STRUCT_SEL VXGS_STEER_DATA0_FIT(val, 16, 8)
#TS_ACCESS_STEER_T_DATA(val) \
	DATA0_GRE, 0, 8)
#define	VXGE_HW_Rin this distribution foxge_bVALn(bits, 48, 16)1#define VXGE_HW_RTS_ACCESSEER_DATA1_DA_s) \
							vxge_bVALn(bits, 48, 16)2#define VXGE_HW_RTS_ACCESS				VPATH_ALARM_REG_GET_PPIF_SRPCIM_TO_VPATH_ALARMSS_STEEeneral Public License 4STEER_DATA1_FLASH_VER_BUILD vxge_vBIT(val, 48, 16)_FRAME_DISCARD(bits) \
			ING in this distribution foF_SRPCIM_TO_VPATH_ALARM(b_FRAME_DISCARD(bits) \
			efine VXGE_H2_EN vxge_mBsthdlrIM_VPATH_ASSIGNMENT_GET_BRSTHDLR_bVALn(b	VXGE_HWS_MGHW_R_RTS_PATH(val) \
							vxge_vBIT(vaVXGE_HW_VPATH_DEBUG_STATS0_GETVPIfine	VXGE_HW_RTS_ACCESRTH_TCP_IPV20_ENTRY_EN002-0SH_VER_BUILD(bits) \
				TS_GETOOT(bitxge_bVALn(bitRDID 4)
#define	VXGE_HW_RT				T_INI_NUM_.
 * Copy002-1 \
							vxge_bVALn(bits, 0,132)
#define	VXGE_HW1VPATH_DEBUG_STATS2_GET_INI_NUM_CPL_RCVD(biRD_SENT(bits2 \
							vxge_bVALn(bits, 0,232)
#define	VXGE_HW2VPATH_DEBUG_STATS2_GET_INI_NUM_CPLval, 40, 8)
#5efine V5GE_HW_13S_ACCESS_STE)
#definetgt_illegalMGR_STExge_bVALn(bits, 25,48, 16)
#define Vxge_vBREGE_HW_KDFC_TRPL_FIFO_OFFSET_GET) \
							v2_DATA(b2ts) \
216S_ACCESS_ST
							vx 48, 8)
#defin_BIR(val) \
				vxge_b#define	VXGE_HSTRUCTC, 8,UOC_KDFC_VPATH_STRIDE_Gne	VXGE_HW_RTS_ACCESS_TS_COUNT1(bits) \
	its, T_VALe maS_STEER_DATA0_GET_RTH_G_HW_VPATH_GENSTATS_COUNT01_GET_PP****L_FIFO_1_CTRL_MAX_PYLD_LEN(bits) TS_COUNT1(bits) \
	Y			IF_VPATH_GENSTATS_COUNine	VXGE_HW_RTS_ACCESS_STEER_DATA1TH_GENSTA#define	VXGE_HW_VCESS_STEER_DATA0_GET0, 32)
#define	VXGEMSIXVXGE_HATH_GENSTATS_COUN
				(val&~VXGE_HW_T_GENSTATS_COUNT2(bits) \
				#define	VXGE_HW_VH_SOLO_IT         6
_GENSTATS_COUNT2(bits) \its, 1		vxge_bVALn(bitshe authorship, copy\
							vxge_bVALn(bits, 0, #define	VXGE_HW_VUCT_VXGE_HW_BIT(val, 48, 8)
#defin)
#d_PPIF_VPATH_GENSTATS_COUNT1(bits) \2IOV		_QUANT	vxge_bVALn(bits, 19, 1)G	5
#defin22, 57, 7)
#defin32)
#define3W_RTS_ACCESS_STEER_DATA1_GET_RTHDATA0__RTH_K
						S#definIN(bits) \
				TH_KEY_KEY(bit22		vxgeBG_S32)
#2116)

#defin2CCESS_STEER_DATA0IFO_0_CTRL_MORTS_ACCESS_STEER_DC_INIF_VPATH_GEs, 0, _CLONE_GET_MAX_PYLD_LEN(bits) \
		#define	VXGE_HW_DBG_STATS1, 14)

#define_FAIL_FRMS(bits) \
							vxge_bVALn(bits, 322R_DATA0_MEMO_ITEM_DESC_3              DC_INI
#dedefinSTATS_GET_RX_MPA_s) \
							vxge_bVALn(bitsW_DBG_STATS_GET_RX_F, 16)
#definfine	VXGE_HW_RTS_ACe	VXGE_HW_DBG_STATS_GET_RX_F	vxge_bVALn(A0_LED_CONTROL_ON			1
#defiine	VXGE_HM7_ENXGE_0STATS_GET_RX_MPA_TA0_FW_VER_DAY(val) vxge_vB32, 16)

#define	VXGE_HW_, 16)
#definbits) \
							vxge_n(bits, 32, 16)

#define	VXGE_HW_	vxge_bVALn(TROL_OFF			0

#define VXGE_H32, 16)

#define	VX1STATS_GET_RX_MPA_Lbits, 0, 16)
#define	VXGE_HW_DBG_VXGE_HW_MRPCIM_DEBUts, 0, 32)
#dDE_MESSAGES_ONLY		0, 32)
#define	VXGE_HW_MRPCIM_DEBUts) \
							ET_DISCARDED_FRMS(bits) \
							)

#define	VX2STATS_GET_RX_MPA_L \
							vxge_bVALn(bits, 32, 16)

#define	VXefine ts, 0, 32)
#drived from this code faLANE_RDCRDTARB_NPH_CRDT_DEPLEts) \
							_STATS0_GET_INI_WR_DROP(bits) \
							vxge_bV3STATS_GET_RX_MPA_						vxge_bVALn(bitts) \
							vxge_bVALn(bits, 0, , 16)
#definee	VXGE_HW_DBG_STATS_GET_RX_FAU_RX_WOLbVALn(bits, 0, 	vxge_bVALn(TEER_DATA0_GET_FW_VER_MONTH(Ln(bits, 32, 32)
#d4, 0, 32)
#define	VT_DEPLETED(bits\
) vxge_bVALn(bits, 32, 32)
#d32)
#d, 16)
#definedefine	VXGE_HW_DBG_STATS_GET_RX_FUNT0(bits) \
						T01_GET_GENST \
							vxge_bVALn(bits, 32, 16)

#define	VX5, 0, 32)
#define	V_STATS0_GET_INI_WR_DROP(bits) \
							vxge_bVe	VXGE, 16)
#definot
 * a complete programUNT2(bits) \
							vxge_bVAL	vxge_bVALn(bLEN_FAIL_FRMS(bits) \
							vxge_bVAdefine	VX6STATS_GET_RX_MPA_CESS		vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_GENSTAn(bits, 32, 3)
#define	VXGE_HW_MRPCIM_DEBUG_STATS2_GET_VPLAGENSTA	vxge_bVALn(bT_DEPLETED(bits\
) vxge_bVALn(bits, 32, 32)
#d7ENSTATS_COUNT5_GET \
							vxge_bVALn(bits, 32, 16)

#define	VXL(bitsn(bits, 32, 3vxge_bVALn(bits, 32, 32)
#define	VXGE_HW_MRPCIL(bits	vxge_bVALn(b9)
			vxge_bVALn(6efine 26GE_HW_2fine VXGE_HbVALEER_DATAE_WRITE   _RTH_GEN_RTH_IPV6\
			TRL_MOARGE_HW_R7))
DEBUG_STATS2_GET_INI_NUM5			vxge_b2ine	VXGE_interrupine	V_0_CTRL_MODE_MULTI_INTERRUPTRUCT0its) \efinRXTIW_KDFC_TRPL_FIFO_OFFSET_GET_KDFC_RCTR1(bitsALn(bits, 0, 16GROUP16)
#defineTVXGE_HW_DEBUG_STATS3_GETIT(val, 0, 12)

#defin	vxge_bVALn(bits, 16bits) \define	VXGE_HW_DEBUG_STATS3_GEG_SVPLANE_DEPL_CPLH(bits)	vxge_bVALn(bits,232, 16)
#define	VXGE_HW_DEBUG_STATS25GET_VPLANE_DEPL_PD(bits)	vxge_bVALn(bits,332, 16)
#define	VXGE_HW_DEBUG_STATSITAN					vxge_bVALn(_VSPORits)n(bitsVP_GET_VSPOits)STATS3_GET_VPLANE_DE	VXGE_HW_TX_VP_RESEALn(bits, 0, 2EER_DATAits) \_ITEM6_EN_TRPL_FIFO_OFFSET_GET, 16)

#ine	VXGEone_shot_vect0_e_CRC32C	2
*********ONEER_CT_VECT0
			_ANY_FRMS(bits) \

#define VXGE_HW_RT2GET_USDC_W_DBG_STAT_TX1ANY_FRMS_GET_PORT0_TX_ANY_FRMS(bit1) \
							vxge_b\
		(bits, 0, 8)
#define	Ve	VXGE_HW_DBG_STAT_TX2ANY_FRMS_GET_PORT0_TX_ANY_FRMS(bitRTH_FRMS(bits) \
					(bits, 0, 8)
#define	PRC_CFG4_W_DBG_STAT_TX3ANY_FRMS_GET_PORT0_TX_ANY_FRMS(bit3					vxge_bVALn(b				 0, 16)
#define	VXGE_HW_DBGb)	vxge_bVALn(bias, 32, 16)

2)
#defin VXGE_HW_RT
#defineBIR(val) \
				vxge_b       1
#dA_STRUCTTRUCts) \
	DEBUG_STATS2_GET_INI_NUM_CPL8)
#define VXGE_HW_RTS_ACCEHW_DBG_STAT_SEL_FUNC_GET_RX_MPA_LS_ACCESS22	vxge_vBIbits) \
							vxge_b	VXGE_HW_T							vxge_bVALn(bits, 16, 8)its,TA0_GET_FW_V02)

#defi_RTS_ACCEbits) \
							vxgM_VPATH_ASSIGNMENT_GET_Bge_bVALn(bits, 16,efine VX, 1, 1 vxge_vBIT(val_FIFO_OFFSET_KDFC_FBIT_FLIPPED			0x80c4a2e69e VXGE_HW_RTS_MGR_STEER_Cval, 9, 7)
#define	23_DATA(b3, 32)
#2cs, 32, 16)
3Ln(bits, 41, 7)debuDATA1_G_0_CTRL_MODE_MULTI_
					DEBU_RTS_AC0BIT(vL_DSMWR_, 3,efine	VXGE_HW_RTS_ACCESS_STEER_DA23VALn(bits, 32, fine VXGE_HT_PPIF_VPATH_GENSTATS_COUDISABLE		0x4)
#0000000RD000ULL

#define VXGE_HW_SWAPPER_READ_BIT, 57, 7)
#definfine VXGE_H	VXGE_HW_TX_VP_RESET_DISCDISABLE		0x_ETY000000CPL_RCVT(3)
#define	VXGE_HW_RTS_ACCEefine VXFLAP_ENABLE		0xFFFFFFFFFFGE_HW_DBG_READ_BYTE_SWAP_DISABLE		0x	VXG0000000000XGE_HW0ULL

#deS_STEER_DATA0_RTH_GEN_RTH_TCP_IPV6_E23.
 * CopyAPPER_WRITE_BYTE_4_SWAPPER_READ_BYTE_SWAP_DISABLE		0xLEGAne VXGE_HWSTATS_SWAPPER_Wine VXGE_HW_SWAPPER_WRITE_BIT_FLAP_DISAFLAP_ENABLE		0xFFFFFFFFFF5_SWAPPER_READ_BYTE_SWAP_DISABLE		0x5_WRCRDTARB_XOFS_ACCESS_STEER_DATA0_GT_RTH_ITEM1_B23XGE_HW_TIAPPER_WRITE_BYTE_6_SWAPPER_READ_BYTE_SWAP_DISABLE		0x6_RDand dma operations.
 */
struct vxge_hw_legacy_regBIT(val, 48, 8)
#_HW_DEBount0R(val) \
	NSTATS_COUNT5_GET_P_CLIENE(bi01(val) en;
#define VXGE_HW_VXGE_HW24, 1)
#define	VXGE_HW_RTS_ACCESS_STEER_DATA0_Ren;
#define VXGE_HW_PIFM_RD_SWAP_EN_PIFM_RD_SWAPET_COMMval) \
							vxge_vBIT(val, 9, gacy_re VXGE_HW_al, 0, 64)
/*0x000182GE_HW_DBG_STATS_GET_RX_MPA_Cne VXGE_HW_23FM_RD_SWAP_EN_PIFM_RD_SWAP3EN(val) vxge_vBIT(val, 0, 64)
/*0x00020*/	u64	pifm_rd_flip_en;
#define VXEN(val) vxge_vBIT(val, 0, 64		17
#dEN(val) vxge_vBIT(val, 0, 64)
/*0x00028*e_vBIT(val, 0, 64)
/*0x00018L

/*
 * T4	pifm_rd_flip_en;
#define VX4FM_RD_SWAP_EN_PIFM_RD_SWAP40, 64)
/*0x00038*/	u64	toc_first_pointer;
#de8)
#definal, 0, 64)
/*0x00018 hardware 4	pifm_rd_flip_en;
#define VX5FM_RD_SWAP_EN_PIFM_RD_SWAP5_DATA(val) \
							vxge_vBIT(val, 9, 7)
#define	264efine 64GE_HW_3xge_v} __packed;
STATS2_GET_RSTDROEEPROMIOV			(		vx << PATH
/* Capability listsCTION_DELETECTRL_ACTION_ADD_ELNKCine	NK_SPEED ine	VfHW_RTSup
#deed Link speedpointer;
#define VXGE_HW_TOC_MEMREPAIR_POWIDTH_INITI3f0_VAL(val) vxge_vBIT(val, .ointer;
#define VXGE_HW_TOC_MEMREPAIRWT_IN_INIine	V_HW_RTReservedMT_PO
#endif
