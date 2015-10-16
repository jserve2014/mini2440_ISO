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
#define	VXGE_HW****/******distributed accACTOR_CHANGEd distributed accoGNU General Publ
 *8This software may be used and distributed hccoGNU Geneto the terms of
 *s basGNU General Public 56cense	u8	unated02208[0x02700-t
 *1d0];

/*ain  re*/	u64	rtdma_int_status;e (GPL), incorporRTDMA_INT_STATUS_PDA_ALARMlete INTGNU GemGNU 1se (GPL), incorporis norshipa compleCC_ERRORystemdincoronlorpor2ted whens basentire operatingshipsyLSOmre oliin bed under the G4L.
 * See the file COPYING in this SMtributiSMfor more inform5)thorship,8copyright herelicmask;10GbE PC1  I/O Vipda_alarm_regotice.  censefile m anprograREG2009 HSC_FIF by runder the G0se (GPL), incorpornc.
Neterion Inc.
******under the Guse 
#ifndee I/O ViAdapter.
  *
#ifndef   2 Server G_H
#defin#defiGE_REG_H

  may REGcc_error * CI/O Virt( GNU Genertetributi*****CC000ULFRM_BUF_SBE(n)under the Gnse (GPL),loc)		(0x80bits at offsetULL >>TXDOuthoship*************, loc, sz) - set bits at offseain /
#d (sz) )
Dvxge_vBIT(val, loc, sz)	(((u64)(val)) << (64-(loc)-(sz)))ftwarvBIT(val, 32loc, sz)	(((u6	(((u32)*****) << (32-#defi-(sz)F****** Neterge_vBIT(val, loc, sz)	(((u64)(val)) << (64-(loc)-(sz)))SERRge_vBIT(val, loc,0GbE PC3/*
 * vxg-(sz)))

e VXGE_REG_H
3(32-(loc)-(sz)))

c64)(val)) < a4 Server lsosz)))

/*
 * vml, lsz) - setin by refiffset
 *in bABOR****vBIT(val, loc, sz)	(((u64)(valare may be_TITAN_ASIC_sz)	ationc)-(sz)))

/*
 * vbVAID() <<) VXGE_RE		HW_TITANe VXGE_REG_H
5****
					HW_TITANDEVICE_ID(bits5 VXGE_REsmxge_bVALn(bits, 0,  incorpor*************SMs, 48This soft************* 10GbE PC6 Server are may be VXGE_REG_H
6)
 software may bDEVICE_rrived PL her7ausrating8tain 77e au10GbE PCa VXGE_REtxd_ownebE PC_ctrlopyright(c) 2002-2TXD_OWNERSHIP_CTRL_KEEP_COMMAND		under the G7ALn(bitsIb - 1)W_HOST_cf(bits, 0, 16)
#defi bitCFset
 *ENABLxge_vBIT(val, loc, sz)	(((u64)(val)) <RPCIMt bit bitSPAC_Nware may bee_bVALn(bitsIb4-(loc)-(sz)controne offseHW_bVALnVPMGMT_RONTROL_FE		17
#de terms of
 * the G
 6, PL.shipSees bas002-MODset
SERVEEARLY_ASSIGN_EN1SIC_ID) \
				IOV
			1sz)))

/*VXGE_HW_ASIUNBLOCK_DBGNU General Pub3*sz))ifndec/*
 * vxge_mense n1DE_MR_IOV				3

C_MGNU 
 * s1may beRAP_0		0xODE_MR_IOV				3

C_MO4, , 8)
_IOV
oftware may be_TXMAC_GEN_CFG1_1fine	BCAST_TO_WIRE			vxge12l, l19s software may beMAC_GEN_CFG1_BLOBLO2K_BCAST_TO_SWITCH	vxge_mB20(23)
#define	VXGE_HW_TXMAC_GEN_CFG1_HOST_A3PEND_FCS				vxge, 0, 31)

8(23)
#define	VXGE_HW_TXMAC_GEN_CFG1_HOST_A4K_BCAST_TO_SWITCH	vxge_mB36(23)
#define	VXGE_HW_TXMAC_GEN_CFG1_HOST_A5K_BCAST_TO_SWITCH	vxge_mBm, 0, 
#define	VXGE_HW_TXMAC_GEN_CFG1_HOST_A6K_BCAST_TO_SWITCH	vxge_mB5T(23define	VXGE_HW_TXMAC_GEN_CFG1_HOSHOST_A7K_BCAST_TO_SWITCH	vxge_mB6z)))

1_T_GENP VXGE_RErtualibw_timerBCAST_TO_WIRE		vxge operBe useLONE_GET_VTRL_TO_SWISWITCH)	vxge_bVALXGE_GET_HOST_TYPE900SIGNM9reS(bitsh \
	0GbE P9, Server g3cmctized
		vxgeopyright(c) 2002-2G3CMCTrating
 * sy, 8)G3IFc's X3100 Serie0nSIC_I, 9I VXGE_REEO_SWTAL_GEe VXGE_REG_H9 /*
 * vxIFO_0_Cer_TXMVPATH_SWIFUNC_MAP_C
						, 8)****, 17,GNU General PubliT(19)
#defe_vBIT(-r	0xA5 VXGENON_OFFLOAGDDR3_DECCX3100/*
 ies 1HW_TXMKDF****PL****O_0		0xA5 VXGEMULTI_OP_UL_MOD		2_HOST_TY6R_IOV				_KDFC_TRPL_FIFO11_CTRL_MODE_E	0xA5LONE_NLY		0
#define 7XGE_HW_KDFC_TRPL_FIFO_1_CTRL_MODE_MULTSSAGESSLY		0
#define 29_HW_TOC_GET_KDFC_INITIAL_OFFSET(val) \
				(_Oval&~E_HW_TXMAO3e GNU General PubliO_1_CTRL_MODE_MULMESSAGESNLY		KDFCINITIAL_BIR(CFG1_TMAC9f offset
MODE_MR_IOV VXGELEGACY_M vxge_vBIMODE_MR_IOVDEVICE_ET_HOST_TYP30, 5)

EN_CS(bit928\
							EN_C I/O VimcAL_GET_KDFC_MAX_SIZE(bits) \
Mnted ng
 * syM00000AL_BIRC_GET_USDC_INXGE_HW_KDFC_TRPL_FIDE_GEDFC_TRVPGROCRCCFG1_GETW_TXM_GIFO_0#defineIAL_BIREN_CTDFC_TRIbits
#define	VFAUN_CF_CTRLoftware ed under the Guated * See the fileSIC_ID	) <<_HOST_TEits)_HW_KDFVALnTRPL_FIFO_OFFSE					vxg30FC_TRPL_FVALnT_IN VXGE_REG_30ODE			0
#m(sz))ne vxge_mBIT(c) 2002-2KDFCSTR****MC_XFMD_MEM1, 15SG8, 8)
TRIDE_GET_TW_KDFCSTRIDEts) \
				fine	VXGE_HW_TXM_KDFC_TRPL_FIFOBASIC_MODE_MR_IOV				_KDFC_TRPL						vxge_bVLFFLOARD_GET_VSPORPL_FIF	0
#define W_KDFC_TRPL_FIFO_OFSET_KDFC_VAPTHMIRIT(val, loc,_0	0, 5)
#define VXGE_HW_KDFC__F_TRPL_FIFOOFFSFO_STRID_FIFONUM1R2(biO_STRIDE_GET_T_FIFOTR2(bits)		vxge_bVALn(bits, 33, 1 may b_HW_KDFCIFO_S1e GNU General Publi				vxgeal, loc, s49_TRPW_HOST_TYP
#define VXGE_HW_PRCKDFCRCTR0SIC_ID_GET_KDAPTHO_OF*************E_HW_TXMAC_GEN_CF1NO_IOV				1
#definefine VXGE_HW_KDFC_TRPL_z)))

/FG4_RING_MOD1FFER			12	2
#define VXG_FIFO_OFFSEVXGEA_B		al) vx_KDFC_TRPL_F1ts, 33, 15)

#deHW_KDFC_TRPL_HW_P GNU General PubliTRPL_FIFO_ET(val) \DE_B			E_BUFFER			11 vxge_vBIDE_B			DEVICE_ID(bi3064-(loc)-grocrc
#definxge_bVALn(bits, 17, 1ITIAL_BIR#defiEGHW_TXMWRterms of
 * the GNU DFC_RCTR2(bits) \
	#define VXGERTS_MWDE2MSRFG4_RING_Mfine 2GET_U#dCICFGMGM3064)(SPACEVXGE_REG_H
 #def   0
#defiPATH_DRL_MR_STEER_CTRLfine	VXGE_HW_TXMAOC1IFO_USD1C_GET_304 \
							R_ST I/O Virx_thresh17
#_repG4_RING_MODE_n(bitsRX_THRESHPACES		PL_PAUSE_LOWndef BCAST_TO_WIRE		vxge_t(c) 2 (GPL),SEL_PNR_STEER_CTRL_C_RCTRsz)))

/*HIGHdefine VXGGR_STE****0xA5DGET_KDF(GPL),L_RANGE_PN            4
#defRED    _0T_NUMBER(bits) \
					 underT_L_RARTHN_CFG1_BISION(bits, 5)
#define VXG1T(bits)	vxge_bVALn(bit4STRUCCT_SEL_RTH_SOLO_IT         6
#define VXGE_H2ET_BFO_0ROOT	2
#define2A_STRUCT_SEL_RTH_JHASH_CFG       7
#define VXGE_3MAC_CFG0_PORT_VPMGMT_CATA_STRUCT_SE incorporEY             #defGLOBAL_W		vx		0
#z)))

/6NO_IOV				1
#defineY             9
#deEXACT_VP_MATC    Q_MGR_STEER_C3_GET_HOST_TYP33bSSIGNATA_EY   10C_IN*******3			17
#dEfb
					_CTR5)
#define VXGETITFB
#deODE_EG_HW_PRC_CCAST_TO_WIRE		vxge_3, 5EER_CTRL_DATAA4E      4     4
#c_HW_RTS_MGR4define VXpcipifAL_GET_KDFC_MAX_SIZE(bits) \
PCIP17_TRPng
 * syDBFFSETbitA0HW_PRDAFO_STRIDE_GET_TDFC_RCTR2(bits) \
	S_MGR_STEER_CTRDATSAC_ADDR(bRTS_MGR_STal) \
		DFC_FIFO_OFFSET(val) \
		 48)
#define VXGE_GENERALGE_HW_
#define VX			1

#define VXGEE_BUFFER			1
#de 4is softwarR_IOV		REG_SPMSGE_HW_K0_FUN)define	VXGE_HW_TXMR_MASKCATTER_MODE_B		KDFC_VAPTHn(bM#definSPARE_R1			0
#defineine Vd may onlfine _FIFO_OFFSE9STRUCT_S4FC_TRPL_FIRSIOGE_PN #define VXGE4ODE			0
#dbR_IO_MASK(val) vxge_HW_K_MGR_MAC_ADDR(bbits aI_RETRY))

/*
_HW_TAl) \TRUCT_SEDAEY            bits1C_ADDR(b_GENADDR_ADSOT_KDF	2
#define VXGOC_KDFC_FIFO_STRIDE55, 5)
#define VXGP_Hfine IVEoc))FER_RTS_
T_GET_KDFC_RCTR0(bit********DDR_MASK(vabits loc, s55_DATA_Sefine VXGE_A1efine VXGE5, 5)
#define VXGN			0
#define_MODE(bits) _STRIDEGET_USDC_IN7S_MGR_S62, 2s softwar VXGE_HW_RDXGE_HSIC_ID23 VXGE_HW_ET(val) \_HW_RTS_MGATA1_GET		vxg vxge_vBI_HW_RTS_MGSTEER_CTRL_DA464-(loc)-sHW_RTS_MGR_STEER_MGR_STEER_CTRE_ENTRY		1fine VXGE_HW_RD_VP
#define L_DATA_SDFC_RCTR2(bits) \
	S_MGR_STEERTS_ACCESSEER_ VXGTRL_ACTION_LIST_FOC_KDFC_FIFO_STRIDEne	VXGE_HW_RTS_ACC			0
#TRL_ACTION_LIST_FION_DELETE_ENTRY		1
TS_MGR_STEERRTS_ACCE VXGE_CTRL_DATAC*****RE\
							vxge_bVALn	VXGE_HW_RTS_ACCESe_bVALn(_CTRL_ACTION_WRIT_DATA1_DA_MAC_ADDR_	VXGE_HW_RTS_ACCESS_SEER_CTRL_DATTION_WRWRIXGE_HW_RTS_VID      _ACTION_LDTION_ADD_ENTRHW_RTS_MGXGE_HW_RTS_HW_RTS_ACCESSefine VXGg this cVALn(bits, 33, 15)

#de0_CELETE_ENTRYfine VXGEROPPED_ILHW_TLSTRUts) \
							vxge_bVALn(bits,  							vxgT_SEL_RTH_HW_RTS_UFFEFO_0PROCESS_STCTRL_OC_KDFC_FIFO_STRIDENID		1
#define	VXGE_LINK_RSTO_FUNC_M_ACTION_WRITAD_ENTRY_HW_PRC_Cin_HW_RTS_MGR_STEERTS    _HW_RTS_TLP_VPLANEACTION_LED_TESEL_PN		IC_MODE_MR	VXGE_HW_RTS_ACCESS_TRAINIGE_P3KDFCDEPL_FIFO_OFFSE_DATA1_DA_MAC_ADDR_)ID		1
#define	VXGE_TS_ACCESSDOW * aSEL_RTHSOLO_ VXG_RANG		#define Ve	VXGE_HW_RTS_ACCESS_S
			AE_HWLLE_HW_TITAN_P2\
							4S_MGT1_DAE_HW_RTS_ACCTION_ADD_ENTRdefine VXE_HW_RTS_ACC_HW_RTS_ACCESS8W_HOST_TYrpcim_msDATAEEN_CFG	5
#define	VX#define VX_KDFCWLOAD#definTO			0
#de_CTRL_0_RbVALn(b8)
#define VXGE_	vxgee GNU General PubliT_SEL_QOS		1al) vxge_e	VXGE_HW_RTS_ACCESS_S1e VXGE_HW_ts) \
							vxEER_1T_SEL_PN		3
#definee_ACS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_2T	12
#define	VXGE_HW_RTS_ACCES#define VXGE_HW_PRC_CTRUCT_SEFW_KDFO		13_HOST_TYPE_ASSIGCCESS_3T	12
#define	VXGE_HW_RTS_ACCESIRSTSEL_PN		S_MGR_ST_ACS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_4T	12
#define	VXGE_HW_RTS_ACCES VXGE_HW_KDFC_TRPL_efine VX8)
#define VXGE_HW_RTS_ACCESS_STER_5T	12
#define	VXGE_HW_RTS_ACCESN_CFG	5
#define	VXGV_ACS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_6T	12
#define	VXGE_HW_RTS_ACCESVALn(bits, 33, 15)
_DATA0_VLAN_IfineAC_ADDETYPits) \
	HW_TITAN7T	12
#define	VXGE_HW_RTS_ACCESNEXl) vxge_v#define _ACS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_8T	12
#define	VXGE_HW_RTS_ACCESnse (GPL), incorpor_SEL1_DA_MAC_ADDR_MASK(val) vxge_HW_K3, 1)
9(val) vxge_vBIT(val, 0, 12)

#dMOGET_PN_SRC_DEST_e_ACTRL_DATA_S#define	VXGE_HW_RTS_RTHdefineIne VXGE_HW_ne	VXGE_HW_RTS_ACCESG1_B4_RINGL_MODEONE_efine	VXGE_HW_RTMGR_GE_HW_RTS_MGR_STEEREER_DIT	1S_MGR_STE_ACCESS_STEER_DATA0bVALn(bits, 7, 1)
#define	VXG(bits, 0, 48)
#define VXGE_HW_RTS1ATA0_ETYPE(val) vxge_vBfine VXG_HW_RTS_MGR_STEERPRCne	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_PN_PORR_DATA0_VLAN_Il) vxgfine VXGE_HPRCG1_B7_SCATTERXGE_ne	VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_PN_PORl) vxge_vBVLAN__ASIC_ID_HW_TITAne VX_CTRL1_HOST_TYPCCESS_STEER_DATA0_RTH_GEN_RTH_EN		vxge_mBIT(3FG4_RING_MODEdefine VXGE12XGE_EN_CFG	5
#define	VXGV_DELETE_ENTRY						vxge_bVALn(biPN/*****NUMXGE_HW_RTHW_RSS_STEER_De VXGE_HW6SK		8bitse	VXGE_HW__SIZE(val)TION_ADD_ENTRHWVSPORTTY_SIZE(val)DATA_STRUCT_SEL_ETY6E      6EY     4MAC_ADDR_MA_PN__DATA0_Pcmg1CTRL_define MAX_SIZfine	VX****CMGE_HW_HW_RTS_MGSSVXGE_HWe may GE_HW_PRC_CFGdefine FFER			0
#define10 vxge_vBIT(ve ma0AN_I0e may b0n)
#define	VXGE_HGET_KDFC_RCTR0(biENKINSval) vxge_e VXGE_HW_1e may bE_HW_#define VXGE_H1_DA_MAC_ADDR_MAENKINS	0
#define	VXGE1CKET_SIZE(1		vxge_bVALn(biRTCFG	E_HW_RTS_e may bW_RTS_ACCESS_STEER_DATA0_CCESS_1GET_RTH_GEN_TH_SO_ACCESS_STEER_CTRL_DNKINS	0
#define	VXGE2CKET_SIZE(2ATA0_GET_RTH_GEN_W_KDFC_TRPL_FIFO_OFFefine	VXGE_He	VXGTCP_IPV4CCESS_21)
#define	VXGE_HVALn(bits, 33, 15)

NKINS	0
#define	UQ48, 8)may A0_DA_MAC_ADDR(val) vxge_vBIT(valETYPE(val) vxge_vBQXGE_HW_Rbits#define	VXGE_H8xge_bVAL6DFC_TRPL_FLG_SEL	_GET_KDFine VXGE_HW_aE      ae	VXGE_H1vxge_bVALn(aE_HW_RTSVERGEN_RTH_ER_DATA1_GET_DA_Mfine	VXPNKINS	0
#define	
#definW_RV6_EN				vxge_bVALn(bi_RTH_SOLA_GEN_R_J_DATA0_GET_RTH_GEN						vxgTA0_STEER_DATA0_RTH_GEN_ALG_SEL_MS_RSS(val) vxge_vB_RTH_SOLOEN	vxge_GR_STEER_e	VXGE_HW_RTS_ACCESS_SCRC32CvBIT, 27, 1)
#define	VXGE_SHW_RTS_ACCESSFIFO_STRIDE_GET_TVPAs, 2S_ACD_PRIVILS_MG0_GET_RTH_GENmBITSIC_ID4(val)USINOR_ 31,aXGE_HW_RT23e_bVAI/O VioneAL_GET_KDFC_MAX_SIZE(bits) \
ONErating
 * syRXPEATA0_RMT_CA0_DA_MAC_ADDR(val) vxge_vBIT(vals) \
				X_EN vTxge_bCC_ACCEPL_FVXGE_HW0_GET_0_GET_RTH_E_HWEER_DATA0_PN__RTS_ACCE_HW_RTS_ACCESS_STEER_ 1)
#define	VXGE_						vxge_HW_R_DA_MAC_ADDR_MASK(vaEN16)

HW_RTS5CESS_STEER_DATA0_ VXGE_HW_KDFC_TRPL_R_DATA0_RTH_GEN_RTH_define VX#define VXGSTRUCT_SEL_DS		11ne VXGATA0_GET_RTH_GEDL48, 8)ER_DER_DATA0_RTH_GEUCKET_SIZE(val) \
							vx	VXGE_H_ADD_ENne	VXGE_HW_RTS9, GET_UEER_DATA0_GET_RTH_GENET_RTH_GExge_bVALnTYPE(val) vxge_vBR_DATA1_DA_MAC_ADDR_			vxge_DATA1_GET_D, 48, 8)VXGE_HW__HW_RTS_ACCESS_STEER_DATA0_VLAN_I, 1)
#define	VXGOE_GET_RTEERONE_GE(bits) nne V		vxg4		1

#defi_ACCESS_SS_STEER_DATA0_ge_81SSIGN	VXGS(bi40VXGE_HW_RTS_8ET(val) \noa_wctine	VXGE_HW_RTS_FE_HW_ENOA_WCTSSAGESVPrshipNUG		2
#defineCCESS_ST48 vxge_vBIrXGE_H2s) \
							vNGE_PN_STRU2efinF1		vx_RTH_MULTI_IT        10R_DASTRUCT_SEL_RTJHASHIDATA1_GET_D2ine	VXGE_HW_RT9, 7VXGE_HW_ts) N_BUCKET_SIZE(val) \
							vxRT3_SOLO_IT_BUCKET_DATA(val) R_DA					vxge_vBIT(val, 9, 7)

#defi4_SOLO_IT_BUCKET_DATA(val) LiceTA0_VLAN_4864-(loc)-SS_STE3_BUCK 8, 1TACCESS_STEER_3H_GEN5_SOLO_IT_BUCKET_DATA(val) _EN(bitoftwareVID      rx_multi_casSS_STEe_bVALn)
#VXGE_HWKEY  defineCASbVALn(bRUCT_SEfine	VXGE_HW_RTS_ACPNL_RANGE_PN  EM0SEL_PN					vxgDELAYCLONE_GET_VSSTEER_DATAW_RTS_IC_MOD#defis s 17)

#dexdm_dbg_rd_BUCKET_DATA(bits)  DBIT(G_ENTADD_ENTRY		1
#defints, 7, 1)efine	VXGE_HW_RTS_AC1)
#define	VbVALn(bits, 3, 1bGET_USDOF48_STEER_DAS_MGR_STEER_data_SIZE(val) \
							vxdefine	VdefineMCFFER			0
#define9val) \
							vxge_vBIT(6XGE_HW_R48fine	VXGErqa_top_prty_for_vh[17]_BUCKET_DATA(bits) QA_TOVLANTY_FOR_VHis software may be_R or derived from tensecode fall9_HW_RTS_MGR_STEE42
#d)

c, smay b8GE_HW10GbE c, sne	VXGE_HmET_KDFC_MAX_SIZE(bits) \
T****	VXGE_HI_PN_xge_INLAN_IRESS_SIZE(val) \
							KDFC_TRPL_, 27efinenabledefine	VITEM1VXGE_HW_RVALn(GE_HW_VBLSFSETS_MGR_STETRUCT_SEL_ETYPE		2
#E_HW_R)	vxge_bBSS_S)
#define	VXT_TO_WIRE		vxge__MULTXGE_HW_RTS_ACCESSITe VX)
#define	VXXGE_HW_RT4NLY		0W_PRxge_bfine	VXGE_HW_RTS_vxge_mBSIMefinI_OP_RD_XOVXGE_HW_RTS_ACCESS_STEEDATA0_VLAN_IDATA1_GEWR
#define	VXGE_2DATA0_RTH_ITEM0TH_GEN_ALDATA1_GETN	vxgBYfine	VXGE_HW__RTS_ACCESS_BUCKET_SIZEresource_assignmentne VXGE_HW_RTS_MGR_ST6, 8DATA1_GOURCEE_SR_IOMENTT_PN 25, ROOER_DATA0_RTTH_I	VXGE_H0XGE_RTS_ACCESaine	VXGE_HW_Rmap_mapping_vpT_DAe	VXGE_HW_RS_STEERC_CFG4_RI25, 7APP_DATVPH_GEN_IM_DES 10
	VXGE_HW_TS_ACTEY      UM(bits) \e_vBIT(vabET_NUM(bXGE_HW_aDC_INW_RTS_RTH_IPVfine	VXG2E_HW_RTS_ACCESS_STEER_DATA0_RENKI2VXGE_HW_RTS VXTTH_MULT) vxgeT(39)
#define	VXGE_HW_RTSRTH_IPV64DATAXGE_HW_RTS_AVCCCESS_S_STALn(bits, 39, 1)
#define	VXGE_HW_REL_DS			VXGE_HW_RCTH_MULTPN_PTS_ACTal, TABLEESS_ine	VXGge_vBIT(vaGEEER_DAET_KDFC_RCT4bSS_STEER_dxATA1)
#define	VXGE_Hsz) - set	VXGE_HW_Rs, 39vxgeBDTftware maRM0_BUCKET_DATA(bits) \
		E_GET_MAX_PYLD_LEN(define	VXGE_HW_ine	VXGE_H1_5)

#defC_CFG0_PORT_VPMGMT_CONE_GET_MAX_E_HW__DATAfine	VXGE_HW_RTS_APN_PXGE_HW_,E_HW_RTS_MGR_STEERse (GPL), incorporefine	VXGE_HW_RT24,REQterms of
 * the GNU T	6
#define	VXGE_HW__ITEM1_ENTRY)	vxge_bVA VXGE(24)
#define	VXGE_HW	vxge_mBIT(7)
#defi(bits, 62,A0_RTH_ITWR_RSP_DATA0_RTH_ITEM0 8)
#W_RTS_ACCESS_SMS_RSSATA1_GET_RTH_ITEM1_D_SIZE(val) \
							ine	VXN_RTH_IPV6_EN	vxgedEM1_ENTRY_EN	vxge_ 1I_WRval, 25, 7)

#define	VRTH_TCP_IPV4_EN(bitsxge_bVALn(bits, 25JHASH_CFts, 7, 1)
#defi				vxge) \
							vxge_bVTA0_GET_RTH_JHASRvxge_GOLDCESSATIOSIC_ID_GKDFC_TRPL_FIFO_OFFVXGE							vxge_vBIW_RTS_Ats, 7, 1)
#defiVALn(bits, 33, 15)

E_HW_RTS_ACCESS_STEER_EER_CTSS_STEER_DATA0_PNTRUCT_SEL_ETYPE		2
#E_HW_RTS_ACCESS_STEER_CMC0_IF0, 32)
#define	Vne	VXGE_HW_RTS_ACCESS_ST_GET_RTH_ITEM1XGE_RTH_IRB, 32CCESVXGE_HW_RTST(43)

#define	VXGE_HW_#define	VXGE_HWXGE_HW_RCFR_STEER_CTRL_DATWE_2SH_CFG_GOLDEN_RAT 1)
VXGE_HW_RTS_ACCESXGE_HW_RDFE_vBICREDIT_OVERFLOMT_CLONE_GET_VS*VXGE_HRTH_ITES_STEER_DATAW_RTS_ACCESS_STEER__DATA			vxgSGE_HSKW_RTUNDTEM0_BUCKET_DATA(bits) \0
#definT(43)

#define	VXGE_HW_RTS_ACCESSATA0_GET_RTH_MA4, 4)
#defi6VXGE_HTA0_RTH_JHASH_CFG_GOLDEN_RA0S_STEER_DAALn(bitsRI_OP_SK_IPV6_ITEM0_BUCKET_DATA(bits) \0, STEER_DATA0_RTH_JHASH_CFG_GOLDEN_RATIO_NUMASK_IPV6vBIT(val,VXGE_HTA1_DA_MAC_ADDR_MASK(valge_vBIT(val, ATA_STRUCT_SEL_RTH_MULTI_Ixge_bVALn(bitATA0_RTH_MASK_IPV6_HW_R_DA__VALUge_vBIT(val6, 8)
#defineALn(bitsWCOMPR			0
#defineESS_4)
					vxge_vBIT(val, 9, 7)

#defiefine	TEER_DATATA0_GETATRL_ACTION_LLED_DATA0)
#define	VXGE_ESS_STEEXGE_HW_RTS_ACCESS_SWRY_ENTA0_RTH_MASK_IPV6	VXGE_HW_RTS_T_RTH_MASK_IER_DATA0_RTH_JHASH_CFts) \its, 7, 1)
#define	Vine	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTHCP2 0, 8R_STEPO 0, 32)
#define	TS_ACCESS3
#dXGE_HW_(bits, 36, 4)
#defi \
							vxge	VXGE_HW_TA0_GL4						vxge_vBIT(val, 9, 7)

#de_vBIT(valine	VXGE_RTH_MAVXGE_HW_RTS_ACCESS_DA_MASK(val) \
							vxge_vBIT(val, _DATA0_RTH_ts, 7, 1)
#define	Vn(bits, 36, 4)
#defiE_HW_RTS_ACCESS_STEER_SIZE(val) \
							vx3	vxge_bVAbE*********T_NUM()
#SS_STEER_DATASDA1_DA_MA(bits) \
DEVICE_ID(bi4b64-(loc)-cT(3)
#define	VXGE_HW_RT_MGR_TS_ACCES VXGESS_STS_CESS_STEER_DATA0_	VXGE_HW_bVID      (bits, 8SS_STEER_DATAis softwa(bits, 8_RTH_MULTI_IT RTH_MASK_IcR_DATVXGE_HW_RTS_ACC_GET_RTH_STEER_ts) \P_H2L2PV6_EX25, 7)

#define	GE_HW_RTS_ACCESS_STENl) vxgQO_STETSTCATA1	vxge_bVALn) \
				_ITEM1_BUCKET_DATA(v	ne	VXGE_HW_RTS_E_DS_ENTRY_EN(bits) \
		XGE_HW_RTS_ACCESS_STe_STEER_DATA0_T_ACCESS_STEER_DATA0_VLAN_A0_RTH_ITERTH_MASK_IRTH_MASSS_STEER_DATAER_DATA0_PRTH_MASDATA_STRUCT_SEL_ET4fET_NUM(fXGE_HW_b5_RTS_ACCESSf	VXGE_HW_RTS_ACine	VXGE_Hvxge_bVAL_DATA0_RTUM(bSS_STEER_DATAP_GOLDEN_RvBIT(vabits, 3, 1)
#, 4, 4)
#define	VXGE_0_RTH_MASK_IPV4_P_EX_SIZ_XT_ENTRALn(bits, 39, 1)
#define	VXGE_HW_RDDdefiGE_HW_RTS_ACCESACCE	VXGE_V6_SA_MASK(bits) \
					fY_EN(bits) \
_DATA0_RTH_JHASH_CfODE			0
#, 0, 64)
#0_RTH_MASK_IPV4_SA_MA_RTH_MULTIne	VXGE_HW__, 39SRAdefine	VXC_CFG4_RIIT(35)
#define	VRTH_IPV6_EXH_ITEM1al, 0, 8)
#define	MPT						v4STEER_DATA1_RTH_RTH_MASK_IPV6_SA_MASK(bits) \
				_DATE_HWPT(val, 25, 7)

#define	al) \
							vxge_vBefine	VXGE_HW_RTS_ACCEWT(val, 25, 7)

#define	 VXGE_HW_KDFC_TRPL_(is software may beU_HW_RTS_ACdefine	VXGE_HW_R\
							vxge_bVALn(bTA0_GET_RTH_MASK_L4SPge_mBIT(3)

#defiGE_HW_RVALn(bits, 33, 15)
4_BUCKET_DATA(val) \
		H_MASK_IPV6_SA_MASK(bits, 4, 4)
#define	VXGE0_RTH_MASK_IPV4_SA_Me_bVALn(bits, 9, 7)
#defiS_STEER_DATA1_RTH_IC_CFLn(bits, 9, 7)
#def4_BUCKET_DATA(val) \
			define	VXGE_HW_RTS_AESS_STEER_DATA1_RTH_ITH_MASK_IPV6_SA_MASK(bitsvxge_bVALn(bits, 25, 16, 8)
#define	VXGEN(bits) \
							vxge_XGE_HWdefine	VXGE_HW_MASK(bits) \
					SS_STEER_DATA0_RTH_ITEM0 8)
#bVALn(bits, 8, 1)
#define	VXGE_HW_RTS_T_RTH_MASK_IPV6_SA_MASK(al) _GET_RTH_ITEM1_BUCTA(bits) \
							vR_DATA1_GETCCESS_TEER_DATA1val) \
			TH_ITEM5_ENTRY_EN	vxge_mBIT(2_HW_RTSSHADOWCCESS_STEER_DATA0_GETSTEER_DATA0_GXdefine	VXGE_HW_RTS_ACCEESPEM0_BUCKET_DATA(bits) \TH_GEN_ALG_SEL_JENK					vxge_vBIT(val,(bit_ITEM0_BUCKET_DATA(bits) \ine	VXGE_H5xge_bVALn(biLn(bits, 9, 7)
RTS_ASK(bits) \
				6xge_bVALnRTH_MASK_IPV4_SA_MASK(val)XGE_HW_RTS_ACCES_ITEM0_BUCKET_DATA(bits) \_BUCKET_DATA(bits) \
ESS_STEER_DATA1_RTH_I0_BUCKET_DATA(bits) \ESS_RTH_MASK_IPV4_SA_MAS_MASK_IPV6_SA_MASK(XILne	VXGE_HW_RTS_ACCESS_S0_RTH_MASK_IPV4_SA_MASK(val)6_SA_MASK(efinen(bits, 16, 16)
#defi24)
#define	VXGE_HW_RTS_ACCESS_STEER_DAvBI_DATA0_RTH_JHASH_CFG_GATA(bits) \
							vxge_bVALn(bits, 25CMWCCESS_STEER_DATA1_GT(vaCESS_STEER_DATA1_RTH_ITEM5_BUCKET_DATACMTRUCT_SEL_DS		e	VXGE_HW_ACCESS_STEER_DATA0_RTHGET_RTH_ITEM6_BUCKET_DATAPV4_DA_MASK(bits) EER_DATA1_RTH_ITEM4_(bits, 16, 16)
#define	V1__RTH_ITEM5_BUCKET_DA1, 7)
#define	VXGE_HW_T(val, 0, 8)
#defineval, 25, 7)

#define	V)
#define	VXGE_HW_RTS_ACCESS_STEER_DATDATA1_DA_RTH_ITEM5_BUCKET_DATA0_RTH_JHASH_CFG_GOLDEN_RA* Th	VXGE_HW_RTval, 25, 7)

#define	EN	vxge_mBIT(56)
2)
xge_mBIT(56)
#define	VXGVXGe	VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM6_BUCKET_DATH_IT \
							vxge_bVALn(bits, 41, 7)
#define	VXGE_HW_RTS_ACCefin)
#define	VXGE_HW_Rfine	VXGE_HW_RTS_ACCESS_STEER_D	0
#define )
#define	VXGE_HW_R_RTH_MASK_IPV6_SA_MASK(bits) \
				7TS_ACC)
#define	VXGE_HW_Rxge_bVALn(bits, 48, 8)
#define	VXGE_HW_XP 1)
PROTEER_Dvxge_mBIT(3UM(bits) \
							vxge_bVALn(bits, 32, l) vxgts, _HW_RTVEGED_MODE_STEER_DATA1_RTH_ITEM6_BUCKET_NUM(val)vS_STEER_DATA0_MEMO_ITEM_PC)
#define	VXGE_HW_RTS_ACCESS_STEER_DAT				WRER_DATA0_MEMO_ITEM_PCMEMO_ITEM_PART_NUMBEn(bits, 40, 1)
#dege_mal) \
							vxge_vBIT4ESS_STEER_DATA0_MEMO_ITEM					vxge_v) \ACTION_DELETE_ENTRYine	VXGS_STEER_DATA1_GET_RTH_ITEM6_BUCKET_DAT				INVXGE_HLn(bits, 16, 16)_DATA(val) \
							vxge_vBIT(val, 57, S_SVXGE_HW_RT7 \
							vxgS_ACCESS_STEER_DATA0_MEMO_ITEM_PART_NUM_OTA1_IC_MODE_M, 48, 8)
#deT_RTH_ITEM5_BUCKET_DA_DATA1_RTH_IT_DATAS_STEER_DATANIPV6_SA_MASK(bits)#define	VXGE_HW_RTS_ACCESS_STEER_DATA1_ASK(val) \
							vxge_vBis sofEMO_ITEM_DESC_0, 8)
#define	VXGE_HW_Rv_RTS_ACCESS_) vxge_vBIT(val, 0, _STEER_DATA1_RTH_ITEM6_BUCKET_NUM(val) 48, 8)
#def) vxge_vBIT(val, 0, HASH_CFG_GOLDEN_RA5xge_Ln(bits, 24, 1)
#(bits, 16, 16)
#define	VXGefinTA0_MEMO_ITEM_DESC_1                5
Ln(bits, 16, 16)
#define	VXGFW_5STEER_DATA0_MEMO_ITEM_DESC_2          GE_HW_RTS_ACCESxge_bVALn(bits 0, S_STEER_DATA1_GET_RTH_ITEM6_BUCKET_DATL

#define VX			vxge_vBIT(val, 16bVALn(bits, 8, 1)
#define	VXGE_HW_RTS_0				vxge_bVA			vxge_vBIT(val, 16RTH_TCP_IPS_STEER_DATA1_E_HW_RTS_ACCESS_STEER_DABCCESS 7)
#define	VXGE_HWATA(54ne VXGE_HW_RTS_ACCESS_STEER_DATA0_MEER_DAine	VX_ACCESPUSHxge_vBIT(val, 16				vxge_bVALn(bi)
#define	VXGE_HW_RTS_ine	VXGE_HW_RATA0_FW_VER_MINOR vTRUCT_SE4fET(val) \GTA0_GET_RTH_MASK_L4SPf vxge_vBIT_GET6RTH_ITEM7_ 0, 32)fne VXGE_HW) \
							vxge_bVsz) - setENTRY_EN(bits)CP_DCACHEET_RTH_ITEM6_BUCKET_DDP_MASF 1)
#define	VXGE the_Pefine	_BUILDING_MI	0
#define 8ge_bVA_HW_RTS_MGR_STE8ne VXGE_HW_RTS_ACCESSA1_GET_FLASH_VER_ 40, CTRL_ACTION_READ_MGE_HW_RTS_ACCESS_STSK(bitsFLal, GET_DL_DS		efine VXGE_AY(vaP_MASK(val) \
					A1_GET_FLASH_VER_TRACits) \
	) \
							vts) \
							vxge_WENTRY_EN(bits)DMAATA1_CFG_GOLDEN_RANC_MRTH_TCP_IPV4_EN(bitENTRY_EN(bits)MefinTS_ACCESS_STMONTH(v VXGE_HW_KDFC_TRPL_ENTRY_EN(bits)QCT_DSl) \RL_ACTION_LED_EN_CFG	5
#dPV6_SA_MASK(bitsefine V, 8, S_TS_ACCESS_STEER_DATA0_GET_F				NO_IOV				CESS_STEER_DA_ACCESS_STEER_fine	VBIT(val, 32, 8)
#define VXG 1)
#define	VXGE_HWine	, 8)
#define VXGE_AY_BUCBIT(val, 32, 8)
#define VXG(valhis softwarTA0_FW_VER_MINOR vxge_vBIT(valGR_STEER_DATA1_DA	 VXGE_HW_RTS_ACCESER_YEALG_SEL_CRge_bVALnIT(val, 32, 8)
#de\
							vxge_bVALn_STEELn(bits, 9, 7)
#deIT(val, 32, 8)
#deBIT(val, 47, 2)
#def							vxge_vBIT(valIT(val, 32, 8)
#deOC_KDFC_FIFO_STRIDENC_MAP_CFGEER_DATA1_IT(val, 32, 8)
#deS_STEER_DATA1_RTH_ICESS_STEER_DATESS_STEIT(val, 32, 8)
#defDATA1_DA_ER_DATA0_FW_VER_MINOR vxge_vBIBIT(val, 32, 8)
#define VXGACCEESS_STEER_DATA1_FLASH_VER_MAJOR vx_RTH__ENTRY_EN(bits) \
		CCESS_STEER__RING_MO vxge_vBIT(val, 8, ATA0_ge_bVALn(bits, 9ine VXGE_H16, 16SS_Sefine	VXGE_HW_RTS_ACCESS_STEEM_VERSION  e_bVALn(bits, 16, 16ENTRY_EN(bits) \
		 \
							vxge_bVALn(xge_bVA_HW_RTS_MGR_Sefine	VXGE_HW_RWIF*****FERREfine	VRX_ (loTCCESS_STEER_DATA1_FLASH_VER_MAJOR v2FC_M, 32, 32)
#define	VX				vxge_fine	VXGE_SS_STEER_DATA1DAMbits, 32, 32)
#define	VXfine VXGE_HW_RTS_ACCESS_STEER_DATxge_bV, 32, 32)
#define	VXDATA0_RTH_JHASH_CFG_GOLDEN_RA* ThESS_STE, 32, 32)
#define	VXge_mBIT(3)

#de_RTS_ACCESSINORvBIT(val, , 32, 32)
#define	VXER_DATA1_FLASH_VER_MAJOR vxge_vB1CP_WAKl) \
					GRITYGE_HW_RTS_ACCESS, 4)
#define	VXGE_HW_CESS_STEER_DATAPMON		vxge_vBIT(val, RANSFERRED_GVits, 32, 3TA0_GET_RTH_MASK_L4SP_VFC_MRDE_HW_RTS_ACCESS_STEER_ESS_STEER_DATA0_PN_TENTRY_EN(bits)PIFTE_HW_RTSts) \
	bits, 
#
#define	VXe_vBIT(val,#define	VXGE_HW_fLn(bits, 9) \
							vxge_vB					vxgeEER_DAf5DTARB_XENTRY_EN	vx_XOFine	VXGE_PAxcEER_DATA0_RTKEYRTH_IvxgSRPCIMTH_ITEM0_BUDATA1__GOLbits) \
							v4RX_FRM_TRANSFERRE_HW_KDFCGENa coS******0CRISRPCIdefine	VXGE__KDFCDEBUval)ATS5)
#deI					vxge_bVA64-(XOFFbits, 40, 8)
#defiVXGE_HW_R_HW_RTSSS_STEER_DATAFG_GOLDEN_HW_RTS_NUM_						v#d							vx_HW_RTScausEER_DATA0_JOLG_SEL_CRC3EER__GET_RT_bVALn(biT_INI_NUM_MWR_BYEER_CTRL_DvxgeT_HOST_TYP5L_DATA
VXGE_GENSTATS_COUNT0(VXGE 0, 32)
sgSET_Gl, 61, RX_FRM_TRANSFERRED 0, 32VALn(bits, 9ts, 0, 3A0_DA_MAC_ADDR(val) vxge_vBIT(val(_CFG_GOLDEN_RAP_MA****P_MA, loc, sl) vxge_ALARM(GE_HW_VPATH_GENS3_Gn(bitsPIF
							vxgRR3ine	VXGE_H(bits, 9, 7)
#dADDR_MASK(bits) \
	tbits)
#define	VXGE_PA26, 8)
#def(bits, 9, 7)
#dfine	VXGE_HW_RTS_AC_NT5(bits) \
							vxDine	ine	(bits, 9, 7)
#dTEER_DATA2	1

#definxge_bVAET_KDFC_RCT5ge_mBIT(15)
#dER_DATA1_GET_RTH_ITEM6_BUge_bVALn VXGE_ROUNT2) \
							vxge8BIT(19)
#defDFC_TRPL_F_FAILT_TX_xge_vBITITPER_CTRL_ACTION_LED_RTH_ITEM0_BUCKET_NUM(bibge_bVA)
#defiMPA_B) \
							vxge_vBVALn(bits, 33, 15)
_DBTH_GENS, 16)
#defi6_GETESS_STEER_DAIST_TA0_GET_PN_SRC_DEST_DBG_STATS_GET_R_ACCE_Fal, 55, 5)
#defifine	VXGE_HW_TX_VP_R_FRMS(bits) \
						_MER_DA_RTS_MGR_STEERRTH_TCP_IPV4_EN(bit0_BUCKET_NUM_bVALn(bits,BG_STATS_GET_RX_FA_bVALn(bits, 32, HW0, 16)
#define	VXGE_H_HW_2, 2)

#define	VXTATS4_GET_INI_NUMALn	VXGE_HW_DBG_STATS_GETEGE_HW*_IPV4_DA_MASK(bits)TS_ACCESS_STEER_DATA	VXGE_HW_DBG_STATS_GETFAU \
							vxge_vBIMREG_SPDEBCCESS_STEEMAC_ADDR__DBG_STATS_GET_RM, 39PCIGE_Hne	VXGE_HW_RTTRANSFERRED_GET_RX_F_DBG_STATS_GET_R, 8)
#dSS_SUPDS_COUMS(bits) \
						 16)

#def, 4, 4)
#defibVALn(bits, CREAine	VXG_HW_G_STATS1_GET_VPLANE_UdefiVPE_HW_T_DISCARDEET_TX_bits, 4e VXGEF_MISs)
#dG_STATS3_GET_INI_NUM_MT(val, 32, 8)0, 16)
#define	VXGEER_DATA1_RTHine VXGE_HW_TITAN_VPAT52
#define	VXGE#define	VXGE_HW52 vxge_vBIDTARB_NPDEVICE_ID(bi5264-(loc)-RB_Pne VXGE_HW_RTS_MGR_STEER_5PL_FIFO VXGUP_DATA1					vxge_vBIT(val, RANSFERRED_G) \
							vxge_vBIT(vaEER_DATA1Pe	VXGEDROP(bits) \
							vxge_bVALn(bne	VXGE_HW_V) \
						OFF(bite	VSG_QUE_DMQM_Cl) \AD_CMDP(bits) \
							v_vBIT(val, 8, 16)

#d_ALG_SEL_CRC32C	2
#TS_bVALnXGE_SS_STEER_DATA0_G_RTH_GEN_bVALn(bit5)
#de	vxge_bVALn(bi		2
#DFC_RCTR2(bits) \
		VXGE_HW_RTSSS_STEER_DATxge_	vxge_bVALn(bit1RPCI_ACCESS_STEER_CTRL_DATA_HW_KVALn(bits, 32, 32)
009 N (64locs) \
							vxge_bVALn(bGE_HW_KDFC_TRPL_FIFOts, 0, 32)
#defineTE_SENTEER_CTF(bits) \			vxge_bVALATA0_RTH_JHASH_CFG_GOLDEN_RA0,WRR_CTN) \
							vxge_bVALn(bits, 32, 3					vxge_bVALn(bi_bVALn(bits, 32, 32)s, 0, 32)
#defineEER_DS_STEER_DATA1_RTH_IALn(bits, 0,NT4_GETW_VER_MINOR vxge_vBIT(v(bits) \
							vxge_bVALn(b2)
#define	HW_RTS_ACCESS_STEER_DADATA0_RTH_JHASH_CFG_GOLDEN_RA0, define	_VER_MINOR vxge_vBIT(vET_GENSTATS_COU3bits, 40, 8)
#dNT4_GET)
#define	VXGE_HW_RTNC_M
#define	VXGE_	VXGE	vxge_bVALn(H_GENS0HW_DEBUG_STATS1_GET_RSTDefine	VXGE_HW_RTSSS_STEER_DATA0SS_STEERge_b#define	VXGE_HW_RTS2,TS_ACCESS_STEER_DATAFRMS(bits) \
							DATAS1_GET_RSTDROP_CLIENT1(	VXGE_HW_MRPCIM_DEBFRMS(bits) \
							n(biS1_GET_RSTDROP_CLIENT1TRANSFERRED_GET_RX_FRVXGE_Ln(bits, 0, 3ET_STRUCTD3
#de(bits, Ln(bits, 32, 32)
#de_STATS1_GET_RSTDROP_CLIEUMQS3_GET_VPLANE_DEPL_ROP_CLIENT
#definge_bVALn(bits,  7)
#de0_GETvxge_bVALn(bi, 32,Fe_bVALn(bits, 16, 16ATS1_GET_RSTDROP_CLIENT1M_PART_NUMBERL_DATA_SDATA0_MEMO_GET_RTH_IATS1_GET_RSTDROP_CLIEN_HW_GENS	vxge_bVALn(bits, 0,57CESS_STEER_DATA0_R_STATS1_GET_VPLA
#deQRY0(bits)	vxge_bVALn(biNT2_DEBUH_VER_MINOR vxge_vBI_STATS1_GET_VPLAFRM 10
STATS3_GETCPLDEBUG_Sge_bVALn(vxge_bVALn(bits, 32, 32)
#definge_bVALns, 3ts) OP(bits) \
							vxge_bVALn(bFRMS(bits) \
							)
#define	VXGE_HW_RTS2, e_vBIT(VXGE_HW_GENSTATS_1_GET_GENSTATS_COU
#deW_DEBUTATS_TPA_TX_PATHVPLANE_DEPL_			vx VXGts)	vxgM(bits) \
						_bVA			vxge_bVALnEBUG_STATS4_GET_INI_NUM_RTS_ACCESS_STEERis sofETED(bits\
) ne	VXGE_VPATH_GENSe_bVALn(bitsBG_STATS_GET_RX_FSIZE(val) \
							v, 0, , 16)
STne	V, 48(bi \
							vxge_vBIits) \
						NSFERRED(bit\
					s, 0, 
#def \
							vxge_vBIYT_TX_VP_T/****0_TX_ANPORT1_(bi \
							vxg \
							vxge_vBI							vxge_bVALn(_bVALnATS_TPA_xge_vBIT(val \
							vxge_vBI) \
							vxge_bVALn(2_PORT0_RX_ANYCLIARB_PH_C \
							vxge_vBIFRMS(bits) \
							vxge_bVALn(_RX_ANY_ENTSCATTEBG_STATS_GET_RX_Fe_bVALn(bits, 16, 16)#define	VXGE_HW_DEBUNY_FRMS_9abcdefULLA_STRUCT_Se_mBIT(3)
#define	VXGE_HW_RTS16)
#defiTH_GENS01ULL
#define VXGEs, 0, 32)
#define	VXGE_HW_DEBUG_STEER_D_HW_SWA_STEER_CTRL_DATA					E_HW_VPATH		vxge_bVA_HW_SWAPPER_BIT_FLIPMXPPLANEBG_STAXGE_NUM_MWR\
							vxefine	VXGE__HW_SWAPPER_BIT_FLIPKD_PH_IF)
#define	VXGE_ine	VXGE_HW_VPATH_GENSTATCKET_NUM(FERRED(bits) P2ER_RER_DATA0_RTH_JHASH_CFG_GOLDEN_RAED	0xf7bR_BIT_FLIPPED			0x80c4a201ULL
#define VXGEe	VXGE_HW_DBG_STAT_RX_ANY_FRMS_G3
#deD(e_vBIT(01ULL
#define VXGE						vxge_bVALn(bits, 16, 8)

#define e_vB at offset
 */ts)	vxge_bfine VXGE_HW_SWAPPER_INITIAL_VALUE			0xL#def	vxBG_STATS_GET_RX_Ffine VXGE_HW_SWAPPER_READBG_STAT_RX_ANY_FRMS_G2c48E		0x000000000
#deine	VXGE_HW_V	vxge_bVALnXGE_HW_SWAPPER_WRITE_BYTE_NPDLAP_G_SPAE				FFFFFFFFFFULL
#define VXGE_HW_SWAPPER_REAdeUXPge_bVRTS_APDEBUnd are n00S2_GET_VPLAND      	vxge_bVH_CRDCT_SPLETis softwa	vxge_bVDATA_STRUCT_SEL_ET534					v34) \
	52) \
							vhw_ister hereimCCESS_STEER_DATA0_G, 1)
# 8, 8)
e_vBIT(val,S_COUNT01_GEefine	VXGE_HWUNT23_GET_SWAPPET_FY) \
Wwapper_fb;s)	vxge_bVits) \
							vxge_b_FRMS(bidefits) \
		RX_AvBIT(vaNT4_GETRTS_MGR_STEER_OCL_VAL(valFITIAL_VAL(valUCT_S) vxge__PIFM__BYTWped a_PI(bits)	(bits)
#define	VXGE_						vxge_b)
#define	VXGE_HW_600000000000000ULHW_SWAPPER_BIT_FLIPPED		x00020*/	u64	p00000000000000ULL

/*
0(53fine	VXGE0st
 001ping for
 * 3DTARB_PH_EN_PIFM_e	VXGE_HW_RTS3AP_ENABLEEN_PIFM_fcdab8efine	VXGE_HW_VTA8copyrigs, 32,X_ANY_0_RTH_MASK_IPV4_SA_MASK(val)n 0, 64)
/*0_HW_RTS_020*WR_FLINT4_GEval) \
							vxge_vBIT(val, RANSFERRED_G36SSIGN538_legac36reg {

 the 8PL an001EN_PIrr21e autho_PIFM_0copyrigtoc_se_bVAP_ENABLE		0xFdefineCISR_VPINVALn(bits, 0, 8)
#define	VX_pointer;
#define VXGE_HW_TOC_FIRST_s)	vxge_bVALn(HW_TOM_VERSENVSPORT_
	u8	unu						vxge_v_bVAne	VEER_DATA0_RTH_JHASH_CFG_GOLDE vxge_hw_toc_reg {
vxge_vBGPL an005EN_PIFM5x00040*/	u645host_accesHW_SWAPPER_BIT_FLIPPED			0s)	vxge_bVALn(PIXGE_HW 16)
#define	VXGE_HWdefine	VXGE_HW_DBG_ST_HW_SWAPPER_BIT_FLIPPED		IFM_RD_SWAP_fineON VXGfine VXGE_HW_TOC_FIRST_POVXGEge_vBIT(val, 0, 64)
/*0x						vxge_bVALbitsN_PIFM_RD_SWAP_MEMREPAIR_POET_GR1_DA_MAC_VAL_INITIAL_VAL(val 0, 64IT_Fs)	vxge_bVALn(_HW_Rvxge, 7)
#define	VXGEits)	vxge
/*0x00010*/	u64		vxge_bVA, 64)
	u8	unuseMT_C_ACCpointer(val, 0, 64)
/*0x0TOSWAPPER_INITIAL_VALUE			0xs)	vxge_bVALn(n(bi#al, 0, 64)
/*0x001e8*/	u64	toc_						vxge_bVALn(bits, 16,)
/*0x00060*/	u64	_al, 0, 64)
/*0x001e8*/	u64	toc_FRMS(bits) \
							vxge_bING_MOhw_ess_regA0 * r00040*/	u627HW_PIFM_ess_vpmts) \
							vxge_bVALn(0_Gs)	vxge_bVALn(bP, 64)
	u8	unused001e0];

/*0x000Bc_common 0, 64)
/*0x001e8*/	u64	toc_0*/	LONGTERM_mTS_A16used00050[0x00050];

/*0x000ine	VXGE_Hdefine VXGE_HW_TOC_FIRST_PORD_SWAP_PIFM_R 64)
	u8	unused5*/	u64	toc_vpath_RD_FLIP_EN(4)OINTER_INIT(val, 0, 64)
	u8	unuTIAL_O0*/	u646host 64)
	u8	unused4fine VXGE_HW_TOC_KDFC_INITIAL_OFFSET(vaVXGE_HW_TOC_VPATPTRUCT_SEVAL(val) vxge_v 64)
	u8	unused3fine VXGE_HW_TOC_KDFC_INITIAL_OFFSET(vaEN_PIF1e0-_PIFMe800040*/	u61ehost_access 64)
	u8	unused2fine VXGE_HW_TOC_KDFC_INITIAL_OFFSET(vaCT_RSTDRO300];

/*0x00390*/	u64	toc_vpat 64)
	u8	unused1fine VXGE_HW_TOC_KDFC_INITIAL_OFFSET(vasTS_ACC0, 64)
vxgeefine VXGE_HW_TOC_VPAT 64)
	u8	unused0fine VXGE_HW_TOC_KDFC_INITIAL_OFFSET(vaTOC_KDFC_INITIAL_OFFSET(vaITIA27ust
 027 64)
	u8	unuse9fine VXGE_HW_TOC_KDFC_INITIAL_OFFSET(vagmt4)
/*0x004b8*/	u64	toc_kdfc_fifo_sVPM 64)
	u8	unuse8VAL(val) vxge_vBIT(val, 0, 64)
	u8	unuTARB_PH_CRDT_DEPLETED(bits\00040*/	u6a0host_accprccense7s1(val, 0, 64)
/*0x00RC a comp1 VXGEVP_	u8	unused00a00[0x00a00];

/*0x00a00*/	u64	toc_kdfc;
#0*/	u64	toc_vpath_KDFC_INITIAL_OFFSET(v_WRng f	vxgPping forrogrndefine Va0HW_PIFM_rxdc
 * setNSTATS_COUNT23_IDE_GET_T_DA_MAC_HW_KDF(vTS0_GET_RSTDROP_MUG
/*0RESS_NOA_VP(n)	vxge_mBIT(n)
/*0xUSDC_INT(val, 0, 64)
	u8	unNT2(bit0*/	une VXGE_HW_RTS_ACCESPE_CMDS_RESET_IN_PROGRESS_NOA_VP(n)ogress8, 16)
#define	VXGE61)
T(val, 8, TATS1_GET_INI_NUM_8,PE_CMDS_RESET_IN_PROGRESS_NOA_VP(n)efine V4bhost_access_kdfxge_ath_ by de;xefcdab89674523_GET_PORT1_TRESS_PRCfifo)	vxge_mPPER_READOADRPCIM_DINTEERGLn(bCM_RESE(nX_PERMImB, 61)
#define V4bVAL(val) vxgin_progress;
#define VXGE/	u64	rxpe_cmds_rge_vBIT(val,_DA_MAC_TO
#define	VXGE_HW_RT5ge_md_req_in_NeteresSS_STAENT0(bits)	vxge_bVALn(bi	vxge_bV00000000ULL

#define VXGE_Hicq_flush_in_progred00050[0x0005AL(val) vxge_vBIT(val, 0, 64)
	u8	unu_DATA1_GET_RTH_ITEM7fine VXGE_HW_TOC_VPATSRPCTOC_USa4_INITIAL_BIb	u64	rxdcone_cfN(bi(val, 0, 64)
_pointer[17];
#define VXGE_HW_TOC_VPATH_5b3f7		0x4	one_common;
#define VXGE_HW_ONE_COMMON_PET_V(val)ICQ_FLUSH_IN_PROGRESS_IT(3VPkdfc_bVALn(LTIK_IPV6CNICQ_FATH_GENSTATPED			0x80c4a2 0, 8)
#definPPER_RNT4_Mn)	vxge_mBIT(nE_CMDS_RESETb8HW_PIFM
/*0xrogress;
#d(val, 0, 64)
/*0x0MXP_CMUMQFFFFF
#defsts are handled by et_en;
#define VXGE_HW_TIM_INT_E(val,IM_PH_CLu8	ul) vSET_INT_EN_VP(n
/*0x000388gsed00010[0x0a 2*/	u64	pifm(va9al, 0, 61)
#defin 61)
#define V9RING_RESET_VPAT3_PIFM4host_acchost_access_en;VALnATS(bits) AY(bits) \
	SS_NOA_VP(n)ER_INdefiHW_R_MRPDYvxge_mBS****n)
/*0_INT_EN_VP(n)	ve	VXGE_HW_RTS(val) im_outstand_EN(bmaE_COMMON_PET_VSTEER_CM_OUTS2 0, 61)
#define VX5VAL(val) vxgmemrep;
#define VXGE_HW_TIM_OUTS3INITIAL_VAL(val) vxge_vBIT(val, 0, 64;
#define VXGE_HW_TIM_OUTS4004b8*/	u64	toc_kdfc_fifo_stc;
#defin;
#define VXGE_HW_TIM_OUTS5ER_INITIA1eVXGE_HW_TOC_USDC_INITIAL_B;
#define VXGE_HW_TIM_OUTSVXGE_HW_KDe_vBIfine	VXGE_HW_TOC_KDFC_V;
#define VXGE_HW_TIM_OUTS7ost_access_e 0, 64)
/*0x004b8*/	u64	t;
#define VXGALn(bitWAPPERSS_NOA_VP(n)/	u64	host_accS_PRC)
/*0xiint_en;
#define SG#defRL_MFG7_SCATTER_MRMS(bits) \
							vxge_bV0x00cRX_ANY_F20*/	u64	msg
			L_PREFE	VXGEA_STRUCT_SEL_ETY0_GET_RvxgeBWRe_mBIT(CHre handlTSTANDING_BMAP_g_dmq_noni_rtl_prefetchfine VXGE8];

/*0x00d00*/	u64BIT(bNTER_INO_OFFSET(val) \
			GE_HW_RTRSTHDL) \
	0DATA(vaab896AT20*/	D_REQ_I) vxge_vB\
					a0EN_PIFa00/	u64	cmn_rsthdlr_cfg1;
#d0*/	u6c(n)	vTARB_PH_CRDT_DEPLETED(bits\/	u64	cmn_rsthdlr_cfg1;
#dDY_MP_BOOTEDQ_IN_PROGRESS_VP(n)	vxge_mB/	u64	cmn_rsthdlr_cfg1;
#dOINTER_INITIAL__en;
#define VXGE_HW_TIM/	u64	cmn_rsthdlr_cfg1;
#dc1HW_PIFM_mclr_int_en;
#definexge_HW_GCMN_RSTHDLR_CFc20*/	u64	msg)	vxge_mBIT(	vxge_m	u64	tim_clr_int_en;2host_acccmn_rsthdlr_HW_4;_KDFC_TRPL_Fcopyrignoffloat_inb90*/	u64;
#define VXGE_HW_TITS_COUSS_NOA_VP(n) liceT(val, 0, 64)
/*0x0TIMd2_INITIAL_BIdPASS_ENABcmn_KDFC_TRPL_F4	tim_set_int_en;
#define VDLR_CFG8_INCR_VPATH_INST_NH(val) vxge_00b9host_acceim_clrd licen;DLR_CFG8_INCR_VPATH_INST_Nefine VXGE_HNRSTHDLR_CFd4EN_PIFd4TOC_USDLR_CFG8_INCR_VPATH_INST_N, 0, 17)
/*0IM_VPATH(n)RT1_RX_ANY_FR1_TDLR_CFG8_INCR_VPATH_INST_NTHDLR_CFG2_SEER_DATA1_FLASH_VER_TS_ACCVECT_CLEAR_MSIX)
/*0x00dc(vR_DATA1_GET_GE_HW_VPATH_DEBfine	VXGE_HWDLR_CFG8_INCR_VPATH_INST_NIFO1(val) vxS2_GET_VPLANCESS_STEER_DATABIT(n)
/*0x00c20*/	uFO2(val) vxge_vBIT6)

#define VR_DATA0HW_PIFM_clear_msix_ *  _all_vectval, 0, 17)
	0, 16)
#d;
#define VXGE_H_P/	u64	cmn_rsthdlr_cfLL_VECPPER_READ_BYTine	VXGE_HW_R7define Vd(n)	vxge_(val) vxge_vBILL_mask_val)	TA_STRUCMNT0_R64	cmn_r1_CLT4_GKDFCr_cfgSS_NOA_VP(n)	_RTH_MULTI_IT  STAFB_INITIAL_VAL(val) vxge_APXGE_HW_SET_MSIfO1(val) vasefine VXGE_Hine VXGE_HW_T/*0x0LIVXGE
			_MASK_VECTOR(val) vxge_vB, 0, 17)
/*0XGE_HW_MASK_VECTOXGE_/	u64	ding_vector[4];
#define VXTHDLR_CFG2_S(define	VXGE_HW_RHW_SET_MSISET_FIFO2(valtAR_MSIX_MASL_VE[48*/	u64e_vBIT(val, 0, 61)
#definGx00dcO         SS_STEER_DATIFO1(val) vxB_INITIAL_VAL(val8];

/*0x0SET_FIFO2(val) vxge_FO2(val) vxge_vBIT(val, 0, 17)
/*0x00e58*/	u6ATS_tan_asic_id 0, 64)
/*0_KDFC_TRPL_FM_VERSION53SK(bits) s)	vxge_bn)	vxge_mBIT(DTARB_PH_s)	vxge_bPYING ons    /
strud00c00[00c0ed001ebval) vxge_vGE_HW_RTS_fau__HW_TATS0_GET_RSTDROP_M5b3f7U
#define	VXGEoc_pMPFd and0_PERMAN(n)	STOUCT_SEL_RTH_RTH_TCP_IPV4_EN(bitge_veGE_HWSTATS_ vxg_HW_R1ld license noPPER_READ_BYTOC_KDFC_FIFO_STRIDENT_STATUS_MRPCIM_ALARM_IN2	vxge_mBIT(0)
#define	VXGEh4)
/*0x004b8*/	u64	NT_STATUS_MRPCIM_ALR_AUTO_LRO_NOTIFIC1
#de 0, 16)
#deUCT_, 16)
1)
#defineIO50];

/*0xping for
 * 6ODE			0
#TATUS_VPATH_e	VXGE_HW_RTS6_TOC_FIRSTATUACTION_DELETE_ENTRY		1
#definDTARB_PH_CRD	VXGEAU_HW_RTS_MASMAC2F_N_FLIPPED			0x80c4aGE_HW_RTS_ACCESS_ST7TUS_MRPCIM_ALAX_MASK_ALinmask_all_vecBG_STATS_GET_RX_FAALL;
#deprogr_ENTRY_EN(bbVALn(bits, 48, 8)
_bVALnTA0_GWts, 0, 32)
#d the GNU General Public 							vxge_vBIT(val,FITIAe8EN_PIFe8TOC_USe7_INITIAL_BIe80*/BIT(val, 32, the GNU General Public L0x00c
 * a comp0ne VXGE_HW_TIM_ISS_NOA_VP(n)	1TITAN_MASK_AL00e78];

/*0x00e8C_MRPCIM_POINTER_INSKINT_MASK0_defiT_MASK0(val) v, loc, sefine	VXGE_HW_RTS_ACCESS__STEER_CATHs1;
#define VXGE_HW_TIM_INT_STATUS1_G0_STATS_EN license n0(val, 0, 64)
/*0_GET_INI_NUM_MWR_BY_HWts) val, 0,9VAL(val) XGE_HW_ *  _progr(val, 0, x00ea0*/	u64	rti_intT_MASK1			vxge00b0_BWROval, 0,_mBIT(7)
#define	VXGE_HW2ge_vBIT(val, 0, 64)
/*0x00e90*define VXGE_HW_TOC_F)
/*0x00e38*/	u64	clr_mean)	vxge_mti_i(bits) \
							vxgE_HW_MRPCIM_DEBUG__RTI_INT_MASK(val) vxge_vBIT(val, 0,00ea0*/	u64	rtiTIM_INT_MASK1_TIM_INT_M1nused00c00[0x00cine VXGE_
#define VXGE_HW_TIM_complis notRTDxge_vBIT(val, 0, 17)S_AC * a complRTIA0_MEMO_ITEM_PART_NUMB_mBIT(7)
#define	VXGEskTOC_KDIN_FRMS(biDY_ENTRY_EN(bW_RTS_ACCESS_IT(valGET_INI_NUM__TOC_FIRST_POADAP
	u8a complTPAits, 0c))
Es;
#define VXGE_HW_RTI_INT_STATUS_RTtle-TATUS_KDFC_KDFCTA0_GBIT(5)
#defi*0x00dJmBIT(4x00a40*/	u64	kdfc_7, ATS_CNXGE__INITIAxge_ATUS0(vTRAFFATH_****64-(loc)-#define	VXGE_FUNC_MAP_CFGCOUNT235
	u8	u65_HW_A56TH_GENUFFE)
	uompl, 17_FBpaGE_HW_RTS_MGRBIT(0)
#definPge_bVAL#defL4#definC, 64)
	vxge_bV	a_H
#de_mBIT(0)
#defin					vxge_bVAbVAL
				00e80-0x00e78];

/*0xVXGE_HW_KDFC_xge_bVAL#defineL_PIC_QUIESCENT	vxge_HW_ESS_STEER_DAT)
	u8	u6
	u8	unuSS_NOA_VP(n)6							vxT(va		vxs_TATUrx_VP(n)E_ENTRY		1
#definDAGALn(b)
#defiXM4_BU(RTS_ERMUCKED_FRMSGNU General Puinter;
#define VXGE_HW_TOC_FIRST_PO6c_HW_TITcLn(bit6PROGRESS__PP6_int__STOTATUlagALn(bits, 39, 1)
#deCTRAUTH_GEATUSCOLLXGE__MASK_IPV6_SA_MASK(bitLT(val,VP(n)	H_SOLO_IT_	Vine 4, 8INCALn(_AGGRge_bVA
	u8	unused00d)
#define VX8e78];

8ALn(bit6cW_RTS_ACCEATUSAL(val) plized
DATA0_GET_RTH_ITEM1_ENTRYPerating
 * syOine VX(TEERval) \
							vxge_vBIT(val, 9, 7)
#IS_ENTRY_EN(bPTM NeterimGET_GER_DATA1_GET_RTHxge_bVALn#definevxge_mBIT(3)
#dvxgeributivxge_mBe_bVALn(bits, 16, 1658its, 0, 32E_HW_ADping for
 * 8ODE			0
#orTEER_DATA1_GET_RTH_ITEM6_BUTEER_DAT VXGne	VXGE_HW_2, 16)
#define	(val) vxge_vBIT(val6its) INTER_INITIAed0M_INT_STATUS1(val) vxge_vBIT(val, 0,adapter_ready;
#ER_IN#defiUVALnAL(val) vxge_P(n))
/*0x004b8*/	u64	adapter_ready;
#dRMdefiHW_PIFM_(n)	vxge_mBIreaTH_ITEM7_BUCKET_NUM(BUG_DIS	vxg6xp_cmds_RCVS_GE_HWODE_MULD_QUHW_CMN_RSTHDLR_CFG1_CL0e(val) vxgg_reOUTTEENDIXGE_HW_RTS_ACCESS_S)
#definGE_HW_RTS_utst, 0, SQ_IN_PROGQE_TRPL_FIFine VXGE_HW_SWAPPER_INITIAL_VALG_VPATH_RST_IN_Phl) vCMN__HW_SWAPPER_BIT_FLIPPal, 0, 17)
_IN_PROGR	vpath_reg_modified00ee8
/*0x00e38*/	u64 the GTI_INT_MASK(val) vxge_PATH_RST_IN_Pie(val,4	cp_reset_in_progressE_HW_KDFCeg_modified8	unused01080[0x010OUT_HW_SWAPPER_BIT_FLIPP(val) vxge_vBIT(val,MRPCIM_DIN_PROGREPTPRS0 64)_MRPCxgman)	vady;
_HW_TITAN6_BUCKET_DNOLn(bED_HW_HOST__GEN_mBIT(6)
0x010c0-e	VXGE_HW_RTSGE_HW_RTS_pbits, 35               1

#def0_TOC_FIRS*****T0*/	BUG_DIaccorge_bVALn(bits,TA(bits) \
							v7)
/*0x00e38*/	u64 the GASK_IPV6_SA_MASK(bits) \
				A1_RTH_ITE_progress;
#dl) vxg;
#define VXGE_HW_TIM#define	VXGE_HW_RTS_A_progress;
#ds, 0, 32L

#define VXGEvxge_bV			vxge_vBIT(val, 16TH_RST_INlanESS_STFRMGE_HW_PRC_CFHW_RTS_ACCESS_STGE_HW_NT	vxge_mBITRDCxge_vxgeA0_GSS_NOA_VP(n)	vxge_mBITTS_ACCESS_STEER_DATA0_GET_FFFFFFFexge_mBIardware b#_FBIFW_TI0x01088];

/*HW_RTS_MGT, 0, 17)
fal, 0, 1)
	u8	T(val, 0e_mBI#define VXGE__TRPL_FIFO_0_8];

/*CTRkE_HWP 64)
/FIFO_0_FRTS_ACCESS_S, 8)xge_mBIT(3)
#dRTH_IIGNe VXGE_HW_R				DA_LKM_SEhis M_INT_STATUS1(val) vxge_vBIT(val, 0,2ust
 1128OC_U112NTER_INITIHW_M vxgCTRL_ACTION_READ_MADe \
VXGE_XGE_HW_GEN_Cl, 16,)
	u8	unuse58DTARB_PH_CESS_ST, 5ments;
#definevxge_mBIT5)
#globalMTUS_G3CM_TI_OP_l) \Y					TS_ACCESPCISUPR_STESNAP_ABRTH_ITEM7_BUCKET_NUM(va	vxge_bVAL, 16,ASSIGNM30_EN	vxge_mBf	vxge_v(bits) \ER_INITIvxge_)
	u8	u87Ln(bitn)
/*0x00ba8*87_progress;
#_DELETE_ENTRY		1HW_PIFM_g_reats) \
		URCE_ASSIGNMENN	VXGSR_IOe VXGEC(val) \7 64)
/*0x0FBphaseSIGNM_INITIAL_B12	u64	rxdcPHA_rdAIL_FRM001eopyrigh_ENTRY		1
#dex010cl, 0, 17)
/1	u64	rpyrights_IN_PRO_MSIsyT(val, 0, 64)
/* VXGE_HW_TOvxge_9, 64)
	9_TOC_F88signmentsuse64	tim_cl_PIC_QUIES7b3d5vxge_mBIT 0, 1M_PmBIT(_fineBUG_DISESOURTEER_DA) vxPIC_QUIESCENT	vxge_mBITPCC_XGE_HIDL/*0x00d_INT_STATUS1_TITH_J8)l, 16, x0005)
#deaval) vxge_vVXGE_HW_PF_maFSET_GCOUNT2(bits) \
							vxTMAits
#define	VTXAL_VALE_HW_APRTCL_UDPbits) \
							vxge_bVALn(bits, 0,_VER_MINOR vxgL4al) vxUG_DIS	vxgflex#defA0_DA_MAC_ADDR(va012SET_F				path_N_PROGln(bits, 48, 1CCESS_STEERxN_PRS_NOA_VP(n)	vxge_mBI_vBI8TH_Aal) vxge_vmBITTS(vaMACJ	vxge_mBIT(0)
#define	VXGER_STATUS_CMG_C_PLL_e_bVALn(bits, 16,IPFRAG_NO001eID_VTER_Sts, 7, 1)
#define	V5	u8	unused0(val) vxge_vBx00e38*/	u64	_DEBUG_STA(val) vxge_vBSTEER_DATA0_GE64-(loc)-SET_MSICTION_DELETE_ENTRY		1
#definDATA(bits) _hw_memrepair_VER_TPA2_KDFTUS0(vdl) vxgecNTER_INITIAed) vxge_vdefine VX(n)	vxge_bargrp_pf_				f_baefinmax0x00_ACCESS_STEER_DAclr_int_en;VF_BAR0_MASK_BARGRP_PF_OR_VF_BAR0_ST_MSIX_MASK_VH_GENSad;
#define VXGE_HW_define VX	host_accRP_PF_OR_VF_BAR0_MA1defi_intE_HW_val, 0, 17)
/*0x00de8*/	u_BAR1_MASK_BARGRP_PF_OR_VF_BAR1_MAONE_Bst_pointer;
#define VXGE_HW_TOC_FIR_PIFM_ASK_BARGRP_PF_OR_VF_BAR0_MA2i_in) \
							vl) vxge_vRGRPET_D_HW_F_BAR2#definBAal, 2, 6)
/*_OR_VFE_HW_TIDDR3_READY	vxgts;
#define VXGE_HW_PF_0SET_FIFO2R_MSPF_Ono(val, 0, 64)p_pf_Dane_assignments
					fcEN_PIFfc0-0IT(val, 5, 11)

} __packed;

str	VXGE_b90*/	u64	tim_clr_int_en;
#defineCR2_MASK_BARGRP_PF_OR_VF_BAR2J_prog;
#define VXGEEe VXGE_H59ardware bGET_INI_WRSOURx00e38*/	u64	is softwareg;
#define	V, 48, 8)
#define VX97
	u8	u97vxge_v9PMGM) vxge_v9Me_mBIT(lE_HW_ADA_L4Pany_frmC_MAX_SIZEsz) - set*0x01218vBIT(NYSCENTLE		0x0Fe_vBIT(U_Dt_en;
#define TA0_FW_VER_MINOR vxge_vBIT(va0x00GTUS_G3mBIT(6)
#ECC	v10*/	u64	v_MRPCIM_POINTER_INI3FBRUCT_STRUCT_SEL_RTH_JHASO_DECC	vxge_mBIT(7)
#defi2e	VXGE_HW_G3FBCT_E complUFFERUFFER	al, 1UG_DISd_GET_HOST_TYP59aSTATUS1a
#defi9HW_G3FBCT_ERRSK(bits) \_MRPClink_util_port[ne	VXGE_HW_IN00ee0*/	u64	08*/	uUTILd and d_packed;
3fbcIZ5)
#dxge_mBIT(3)
#define	VXGE_HW_ADA,01_GET1_GET_RSTT(valIT(val, 5,g8	unt
#depter.
	VXGECF0e4	rd_req_gendefin;
#_RTS_ACCEPPER_READ_BYTus;
#define	VXGE_HW_WRDMA_;vxgeFROCRKDFCxge_mBIT(3)
#define	VXGE_HW_ADA	VXGE_HW_XMAC_VSPORT_Cus;
#define	VXGE_HW_WRDMA_PKT_WEIGH#define	VXGE_HW_RTS_ACFSET_KDFC_VAPTk;efine VX_HW_WRDMA_INT_STATUS_RXDRM_SM_SCALE_Fre_mB_TRANSFERRED_GET_RX_59    11
#dTH_ASScfg0fine	VXGE_HW_W *   WM_SM_ERR_RXDn_rsE_HW_WRDMA_TEER_DATA0_FW_VER_MINOR vxgTCPSYKDFC_READY	vDA_ESIX_ND_PA_penW_DEBUG_STATS1_GET_RSTT(valfine	VXGE_HW_WRD_GETBYT#define VXGE_HW_SWAPPE_HW_GEHW_WRDRde_bVALn(bixge_mBI1(6)
#define	VXGE_C_Sfine00ee0*/	u64CESSWRDMVG_IPTUS_RC_ALARM_RC_INT	vx							vARM_FRALARM__INT_STAT		vxge(6)
#define	VXGE_HW_WRDMA_INT_STg
 * sysATUS_RXDRMET_CUTSits) ATA0_GET_RTHG_G3IF_GDDREa2STATUS12
#defia0_REG_G3IF_Ga_mBIT(6)
C_QUin by r_des
#definEPLETED(bits\TIM_ITR2(Be VXGESS_STge_mBal, 56, 8)
/*xefcdab89(64-(loc)-STATmarkerIT(val, 0, 17)
/*0x012NE_GMARKITEMFGxge_vRCVRB	VXGE_C 0, A_INT_STATUS_RTA(bitsr and licGE_HW_30*/	uTH_ITEM7_BUCKET_NUM(va00ee0*/	u64	1_RC5)
	u8_DECCFmay oOUTH_ITEM7_BUCKET_NU_ALADMA_0x00e80-0x00e78];
OHW_RTS_ARC_ALARMSyrigts, _KDFKne	V_STATERVAS_WRDefine	VXGE_HLn(bits, 32, 32, 1)
#define	VHW_PIFM_wE_HW_RC_ALARM_RTHROTTL
#def2)XGE_HWx00e38*/	u64	35)
	u8ardware bSTATtx(	u64	clr_mP(n)	vxge_E_HW_T011308*/	uDge_mmBIT(3)
#define	VXGEACCESSAJGE_HW_VPATH_HW_GEN_CTRW_G3FBCTACCESSIT(valBELG_SEL_CRC32C	CTRL__E_HW_ASIC_MODE_Me	VXGE_HW_G3FBCT_ERRV4_ENativAIfine VXGE, 0, 64)
4	msiDBm is0ee0*/	u64	v)
e_mBIMAXM_VERS_ITTS_ACCESS_STEER_DATA0_MEMO1(bits) \DE3MA_INT_StxHW_GEN_CTRL_SPI_ERR	vxge_mBIT(8)
G_DMQ_N_HW_GUCT_SE 17)MPTIED_MRPC of
 * the GNU General Public DATA_STRUCT_SEL_RTH_KE
			_SMR	vxge_mBIT(8)
1W_RTS_	vxge_mBIT(8)
fine VXGEUS_WRDMA_WR1larm_alarm;
/*/	u64	rc_alarm2TDRM_SM_ESK_BARGRPrcpter.
 GE_HW_WRDMA_aSET_FIF, 17IT(6)
S#defiG_DIS	vxgedefine	VXGE_HW_ADADE1_INT	vd4
	u8	ud4vxge_vaR_REG_G3IF_Gd_INT_STATFRMS(bito_me VXGEal) vxgrmsge_bVALn(bits,	\
incorp_HW_ts;
#define VXGE_HW_PF__vBITS_STEER_DATA1_GET_RTH_ITEM6_B_vBI	vxge\
#define	VXGE_HW_RTS_ACC	ine	VXGE_HW64	VXGE_0a58KDFC_T	VXGE_HW_RT64	r_L4PRTCL	vxge_mBIFRMS(birrPMGM;w_vBIT(val, 0, 17)set_msix_x00418];

DECCPRmne	VXGW0x00aaATUS_MRPCrxdwm_ssm_err_PA_T_RC_ALARM_REGBT04a0*/	uMISdefineDDR_MASK(64S_NOA_VP(n, 17)
DWM0a18*/		rxdwm_RESETtriY	vxge_#define5ardware bdebuATUS_RXD_err_mask;
/*0x00RREBUx01218*0L_ON_RXDK_AL038*/	u64	R3_DECC	xge_vBIT(val, 0, 61)
#defiC_ALARM_REG_BTC_SM_ERCPW_RC_ALARM_REG_RMM_RXDSTEER_DA#define	OP_MODE	vct vxM_FRRSTEER_DATA
	u8	unusC_ALARM_REG1HW_RDA_ERRT_VPL0_BUCKET_DATA(bits) \
CIXR	vxge_mBIT(8)

/*0x00010*/	u64sed01128[0	vxge_M0_BUCKET_DATA(bits) \ENTRY_EN(bits#d	VXGE_HW_s, 0, RDA_EALARM_REG_RHS_G_DMQC_ALARM_REGT(23)RTH_ITEM7_BRTS_MGR_STEER_CTRL_DATvxge_mBIT(3)65_INT_STATe	VXGE_HW_R3_SM0_ER_RTH_ITEM0_BUCKET_NUM(C_ALARM_REG3	rxdwm_sD#defiER_DATA0_GET_FW_VER_0__TABLE_SIZE(val) vxge_vegge_vBIT(val, 0, 17)RDA_N	rc_alACES	DA_ERRA_ER(nTA0_MEMO_ITEM_PART_NUMBE0*/	u64	rda_ecc_db_mask;
CPLC_DB_REG_RDA_RXD_ERR(nEfine VX_PRC_80FMA_INT_Se	VXGE_HW_R4REG_PRC_ne VXGE_Hrda_ecc_dbEG_PRE_HW_RDA					vxgeefine Dx00a98*/	u64	rda_ecc_NT_EN_VP(n)	vxge_mBITaCFG0_STATrda_mask;
/sgpterNDRM_ne	VXGE4	rd_req_rqadbS_RXDRM_ne	VXGEaHW_RQA_ERR__RQA_SREG_RQA_SM_ERRCPLLARM	vxge_mBIT(0)
/*0xu64	rda_eMxge_    T	7bVALn(b_GDDR3in66	vxge_bVALnits, a_ecc_sg_alaine	VXGE_ATS_COUNT2(bits) \
							vxgWDE2_Ine VXGE_ating
 * sys			vxgesignments;
#define VXGE_HW_PFM_ERR_REG_PRC_(val) vxgfrfH_CFGTEER_DATA0_RTH_GEN_ALG_SEL_MS_RSlarm_reg;
#define	VXGE_HW_e operati00bb0];

/*0x00c00*/	u64	msg_resrm_reg;
#define	VXGE_HW_Wrog;
#_ALARM_REGQCQDFC_RCTR2(bits) \
		_mBIT(1)
#define	VXGE_HW_SS_STEER_D*0x00c08*/	u64	msg_mxp_mr_readyvxge_mBIT(5)
#defc_sgog;
#NKINS	0
Y_MP_BOOTED(n)	vxge_mBIT(n)
/*0e	VXGE_HW_ROCRC_ALARM_ALARM_RET_RSTDROP_CLIENT 0, _ENTRY_EN(bitsne	VXGE_HW_ROCRC_ALARM_ALARM_Rxge_bVALnER_STAT
#define	VXGE_HW_R7)define	VXGE_HW_ROCRC_ALARMALAIFfine		vxge_mBIT(CH_BYPA8	unu*/	u64	rts_aCRC_ALARM_REG_NIT(3RCBM64	rUC_ALARM_REG_NOA;
#define VXGE_HW_MSr.
 * Ce_vBIT(val, 0, 17)ROOA_RCBM_IMM_SG	v_Sts) \
							vxge_bVAARM_REG_NOA_W_RTS_AEGB_RSVD_DQ_UMQM_ECC_DB	vxad;
#define VXGE_HW_nC_ALARM_REG_QCQ_MULTI_EGB_OWNQM_ECC_SG	vxge_mdefine VXGE_HW_TOC_FI_mBIT(1)
#define	VXGE_HW__HW_H_ALARM_REG_NOA_NOCRC18*/	u64	rc_alarefine	VXGE_HW_ROCRC_ALARROTVXGE_HWEda_ec0x00a	rc_aIT(5)
#define	VXGE_HW_ROCRC_ALARM_REG_UD_VER_MIN)
/*0x00a28efine VXGE_HWUVALn(biGE_HW_ROCRC_ALARM_REG_QCQ_AFB_MGR_SDY(n)	vxge_n)
	u8	unused00d00[0x_mBIT(1)
#define	VXGE_HW_efineal) GRR	vxge_mBGE_HW_G3FBCT_ERR_REG_G3_ALARM_REG_NOA_RCBM_ECC_FBG_NOA_RCBM_e_mBIT(BIT(val, 0, 17)
/*0x	VXGE_HW_ROCRC_ALARM_REGQC_SPI_APP_LTSSM_THW_CMN_RSTHDLR_CFG1_CL_mBIT(1)
#define	VXGE_HW_DRB		vxBIT(17)
#def11TARB_PH_CRDT_DEPLETED_mBIT(1)
#define	VXGE_HW_N(bits)64	cmn_r2thdlr_cfg1_FIFT_MASK0(v_HW_ROCRC_ALARM_REG_RCQ_BYCIM_DE_ALL_VECT_SET_MRR_ALARFGE_HW_RTS_RDA_E	VXGE_HWESS_ET_KDFCC#def64	rc_ae_mBIT(3)
#define	VXGEdefifpter.
 c0*/	u64	rqa_eeBIT(0)
/*    pter.
 * C)
#define	VERR	vxCRC_ALARM_REG_NOA__QXGE_HW_WDE0_ALARM_REG_WDE0_CP_SM_ERERR	v_ALARM_REG_NOA_WCT_XGE_HW_WDE0_ALSG_RC_ALARM_REG_BTC_SM_ERERR	v_ALARM_REG_NOA_RCBM__mBIT(16)
#define	VXGE_HW_ROCRC_ALARM_vxge_m_REG_NOA_RCBM_e_mBIT(13mBIT(17)
#define	VXGE_HW_ROCRC_ALARWeg;
#YNC_ERR	vxTRY_EN(3)TRY_EN(TS_ACCESS_STEER_DATARM_REG_UDQ_E11_ALARMQ_Uefin_SM_ERR	vxge_mBIT, 5)
#define VRC_ALARM_REG_UDQALARM_REG_BIT(13)_mask;
/*0x020)
#define	VXGE_HW_ROCRC_ALARM_REE1_ALA_ALSM_ERR	vxge_mBITine	VXGE_HWe	VXGE_HW_ROCRC_ALARM_REG_RCQEGeg;
#dPC_STEE_err_reg;
#define	VXGE_HW_ROCRC_ALARM_REGEG_QCQ_MU0x00a30M_REG_WDE1_PRM_SM_arm_alarm;
/*0x00b30*/	u64	wde2_alarm_rREG_RMM_RX*/	u64	rc_alarmRTH_ITEM7_BUCKET_NUMIT(4)
/*0x00b08*/	uE_HW_RDA_EN_HW_WDE2_ALARM_RT(2)
#define	VXGE_HW_WDE1_ALARde2_alaERR	vOWN_RSVD_SYNC_ERR	vx4	wde1_20)
#define	VXGE_HW_ROCRC_ALARMx00a30OW
			VD*******/	u64	rc_alarmbVALn(bits, 48, 8)
#RM_REG_UDERR	vCQ_LPORTRFLOW	vxge_mERR	vxge_mBIT(1)
#define	VXGE_HW_WDE1_EG_WDE1YPQ0l) \
				_ERR	vxge_mB#define	VXGE_HW_TXMvxge_mBIT(1)
#ERR	vYPQ1_reg;
#define	VXGE_H24	rxdrm_sm_err_maskvxge_mBIT(1)
#	wde3(2)
0)
#define	VXGE_HW_WD	VXGE_HW_RTS_ACCESS_CRC_ALARM_REG_N2(2)
#CMDNG_VP(6)
#define	VXGEeg;
#definfHW_RQA_ERALARM_REG_WmERR	vDE3_CP_HW_WDE0_ALARM_REG_Wc0*/	u64	rqa_
#define Vwde0pter.
 ERR	vx_ERR	vxge_mBITDE01_ALARM_REGarm_mDlarm18*/	u64	rc_alar0)ERR	vx4	wde3_alarm_mask;
/*0x00b58AL(val) vxe VXGEpD_MODE \_ACCES48)
#defin_HW_HOST4	wde3_P, 39, 1)
#definAP_EIGNMEN_0_RXlarm;
/*0x00b18*/	u64	wde1_alarm_reg;
#dXIAL_OUND_ROBIN_fine_e VXIOCRC_ALARM_REG_NOA_e_mBIT(3)
#define	VXGE_HW_VXGE_HW_RX_W_ROUTGICQ_FLRI_mBIXW_WDE2_ALARM_REG_WDE2_PRM_SM_ERR	vxge_mBIVXGE_HW_RX_W_ROUCONFI64	seROUN#definesk;
/*0x00b40*/	u64	wde2_alarm_alarm;
/*0s) \
9 3, 5)
#defRDRVXGE_HW_			vxg	VXGE_HW_WDE3_ALARM_REG_WDE3_CP_CMD_ERR	VXGE_HW_RX_W_ROUPL)
#deROUN, 16de3_alarm_mask;SS_NOA_VP(n)	vxge_mBI11 3, 5)
#defi_HW_HOS_DATA1_GET_FCCESS_STCT_FLFCCESEM6_BUKDFCu64	tim_78-0x0Pfg1;
#dd00fc0[0x00fc0-0EITYM4_B5SS_NOA_VP(n)	vxge_mBIal, 16,S_RTI(val, 0, 17)
_WOCRC__mBIT(nFG0_STATS_EN8;
#define VXGE_HW_CMBIT(val, 51, 5)
#define VXGE_HW_RX_W_ROU2D_ROBIN_0_RX_W__HW_R_HW_ONE_COMMON_PET_VRTS_ACCESS_ST_SS_5(val) vxge_vBI_RX_W_PROUe VXGE_HW_RX_W_ROU3D_ROBIN_0_RX_W_bVALn_HW_ONE_COMMON_PET_VSM_ERR	vxge_mBIT(0)
#define_vBIT(val, 3, 5)
#define VXG_RX_W4D_ROBIN_0_RX_W_4_HW_RX_W_ROURITYOBIN_0_R 5)
BIT(val3ESS_L4PRTCL_TCP_EN(val7_SS_5(val) vxge_vBI	VXGE_HW_ROBIN_0_RX_W_5		vxge_vBIT(val, 19, 5)
	VXGE_HW_GEN_CTRL_SPx00a00*/X_W_ROUND_ROBIN_1_RX_W_PRIORITY_6ODE_B				2
#vBI6		vxge_vBIT(val, 19, 5)
ge_vBIT(val, 19, 5)
BIT(val, 51, 5)
#define VXGE_HW_RX_W_ROU7_vBIT(val, 35, 7		vxge_vBIT(val, 19, 5)
E3_ALARM_REG_WDE3_CP_SM_ER_W_PRIORITY_SS_12(val) \
						vxg8_vBIT(val, 35, 8		vxge_vBIT(val, 19, 5)
N_1_RX_W_PRIORITY_4SS_NOA_VP(n)	vxge_mBI35_PRIORITY_11	resou9_vBIT(val, 35, 9		vxge_vBIT(val, 19, 5)ge_bVALn(bi XDCREG_Rss_ipfrag;
#dY_SS_5(val) vxge_vBI_RX_W_PRIORT(val, 0, 17)
_W1E_HW_CCESS_STEER_DATA0_GE VXGE_HW_PF_ROU2N_1_RX_W_PRIORITY_16					vxge_vBIT(val, 59_W_PRIxge_vBIT(val, 19,US_FBIF_M_PLL_ISTEER_DATA0_GET_PN_PORTal) \
							vxge_vBIT(val, 11, 5)
#define VXGE_HW_, 5)
#define VXG1_HW_RW_RX_W_ROU) \
							P_EN(val, 6xge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBI_ROU(val)19, 5)
1bVALn(bval) \
							vxge_16)
#definexge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIvBITT_MASK0EC(va1S_RDA9(val) \
							vxge_v_vBIT(val,xge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBIMODE_B				2
#vBI1e_mBI	vxge_vBIT(val, 35, 5	VXGE_HW_GExge_vBIT(val, 19, 5)
#define VXGE_HW_RX_W_ROUND_ROBI_vBIT(val, 59,_OVERF#d	vxge_vBIT(val, 35, 5OC_KDFC_FIFxge_vBIT(val, 19, 5)
#define VXGEW_WRD_GET_RSTDRONeteri_2						vxge_vBIT(val,w_round_robin_3NNING	vxgdefine	V7AR(val) \
	E_HW_WDE0_x_w_rts) \
							vxge702SSIGN7b3d5ain702 \
							W_ROHW_RTS_inie VXGEs0ULL

#define VXGES_STEER0(val) \ITY_SCHW_WRDFTu64	wP_UNUS			vxCCESS_STEEts, 7, 1)
#define	VXG0_RX_W_PRIORIT, 27, 5)
#devxge_		vxge_vBITSS_NOA_VP(n)	vxge_m27(val) \
					D_HW_Nete(val, 0, 64)
/*_REG_WDE2_C the GPL		vxge_vBIT3e_vBIT(vPOIS*/	u64	wde3_alNO_IOV				1
#defineal, 35, 5)
#define VUN vxge_vEXGE_HW_WRDCC
/*0x00e38*/	u64	clral, 35, 5)
#define VTS_MGXGE_HW_WDE3_ALARM_REG_WDE3_CP_C27(val) \
					ge_ve VXGRX_W_PRIORITY_3xge_vBIT(val, 56, 8)_PRIORITY_SS_16(val			vdefine VXGE_HW_R_RUND_ROBIN_1_RX_W_PRIORITY_ 5)
#defge_vB(valge_hw_mrpcim_reg {
/*0x00000*/	u64
							vxgeX_W_ROUval, 0,AL(val) vxge_vBESS_STEER_DATA1_FLAVXGE_HW_RX_W_RO_RX_W_2(0_mask;
#define	VX_RTH_ITEM0_BUCKET_DATA(bits) \S_16(valits, )
/*0x00IT(val, 0, 1HW_RTS_MGR_STEERXG   7

#VXGE_HW_RX_W_STAT)
#define VXGE_HW_RX, 5, 27, , 0, 17)
/*0_RX_W_PRIORITY_SSMe	VXY0*/	arm_alarm;
/*d_swap#define VXGE_HVXGE_HW_RX_W_ROUND_4xge_e VXGE_HW_RX_W_, 51vxge_vBIT(val, 3, 5)
#define VXGvxge_v4_RX*/	u64E_HW_RX_W_ROUND_R33(val) \
							vxge_vBIT(val, 11, 5)IT(vge_vBIT(val, 59,_W_PRIHW_RTS_NT7_VITAN_GE X_W_PRIORITET_KDFC_RCT7US_FBIF_M_X_W_PRIORITDEVICE_ID(bi70W_G3FBCT_Emne VXGEM4_B2(val, 1EC(val) \
	xge_vBIT(vaDMQ_NDVXGE_HW_RTS_TA0_FW_VER_TUS1(val) vxge_vBIM)
#define VXGEWR_ROUND_ROBIN_4_RX_W_PR) vxge_vBIT(val, 0,()
#define VXGEUND_M_ERR	WRA0_MEMO_ITEM_PART_NUMBEER_DATA0_PN_SRC_DESTUND_R5ge_vBIT(val, 0, 17)
ne VXG17)
/*0x0wde1_alarm_alarm;
/*0x00b30*/	ge_vBIT(val, 3, 5)
#define Val) vxge_vBIT(val, 3, 5)ts) \
							vxge_bV4efine VTA0_MEMO_ITEM_PART_NUMBECESS_VXGE_HW_RX_W_ROU    0
#define VXGSTEEe_vBIT(val, 3, 5)
0*/	VXGE_HW_RX_W_ROU5e_vBIT(va4	rx_w_rounne VXGE_H	vxge_vBIT(val, 19(val) \
		O_QOSS_42(val) \
				9,define VXGE9HW_RTS_ACCESS_STEER_DATXGE_HW_RX_IORITY_SS_5(val) vxge_vBI16)
#define	VXGE_HW	VXGE_HW_RTS_ACCESS_STMODE(bitsUND_ROBIN_5_RX_W_PRIOA0_RTH_JHASH_CFG_GOLMEMO_ITEM_PART_NUMx00a30_SS_15(val) \
	E_HW_RXEC(va	VXGE_HW_GEN_CTRL_S#define VXGE_HW_RX_W_ROval) \
	6UND_ROBIN_5_RX_W_PRIOHW_RX_W_ROUND_ROBIN_e_vBIT(val, 3, 5)
ROUND_ROBIORITY_SS_5(val) vxge_vBIUND_ROBIN__HW_TIM_INT_STATUS1_TIXGE_HW0, 17SK_BARGRPUND_ROBIN_5_RX_W_PRIOHW_RX_W_ROUND_ROBIN__RX_W_PRIORITY_SS_e oper, 5)
#define VXGE_HW_RX_W_e_bVALn(bits, 0, 16)
#defvxge_vBIT(val, 196e_vBIT(va11, 5)
#define VXGE_Q_IN_PROGRESS_VP(n)	, 11, 5)
#define VXGE_HW_RX_al) vxge_vBIT(val, 3, 5)W_REPLICQ_FLUSH_IN_PRITY_SS_42(val) \
				, 27, 5)
#dfine VXGE_HW_RX_W_ROUND_ROBIN_						vTA0_MEMO_ITEM_PART_NUMM_ERR	RDdefine VXGE_HW_val) \
			fine VXGE_HW_RX_W_ROUND_ROBIN_, 51, 5\BIT(val,VALn(T(val, 27, 5)
#define, 59, 5)
/*0x00c08*/	u64	rx_w_(val) \
e operN_6_RX_ROUND_ROBIN_6_RX_W_P#define VXGE_HW_RX_W_ROUND_ROBIN_6_RX_#define VXGE_T(val, 27, 5)
#define, 3, 5)
#define VXGE_HW_RX_W_ROUND_BL0x00s) \
						TEM_DESC_2    RX_W_ROUND_ROefine VXGE_HW5X_W_ROUN*/	u64	STEER_DATA0_FW_VER_MINOR vxge_vBIT(val0*/	u64	rx_w_round_robl) \
	*****mBIT(1)
#deR_SM_E9W_RX_W_8_RQA_ERR_6, 8)
#def#define VXGE_DTARB_PH_RX_W_ROU7_8];
define VXGE					path_gN_4_R		vxge_vBIT(val, 11, 5)ATA1_VXGE_HW_TS(vaGT_VENDO_STRIprogress;
#define
} __packSS_C7W_RX_W_ROUND_ROBIN0*/	UP_EN__HW_ADALARM_REG_WDE2_C the GPLdefine VXGE_HW_RX_WOBIN_3_RX_W_\
		T_VPLANE_WRCRDTARB_PH_CRDT_DEW_RX_W_ROUND_ROBINBOO11, ITge_bVALn(bits1, 5)
#define VXGE_define VXGE_HW_RX_XGE_H5)
#OSS_QWRine	e_vBIT(val, 32, 4)
*0x00b20*/	u, 5)
#define VXGE_HW_R_GEN_CF 5)
#define VXGE_HW_RXA_ERRUS_G3_HW_RC_ALAW_RX_W_ROUND_ROBINbits (valGE_HW_DBG_STATS_GET_RX_LINT_G_ST_RX_W_ROUND_ROBINUSDvxge_vBIT(val, 11E		vxge_RDA_PCIX_ERRIT(val, 35, 5)
#define , 35, 5)
#define VXGE_HW_RX	u8	unused0							vx vxge_vBIT(val, 3, 5IX_BEYOND_W_RX_W_ROUND_ROBI4	msixgrp_no;
0x000X_W_ROUND_ROBIN_7_Wine	Vbits fine VXGE_HW_RX_W_(val) \
							vxge_vBIT(val,T(val, 3,l) \
		vBITITY_SS_36(val) \
		_PRIORITY_Sval) \
							vxge_vBIT(T(RITY_SSRD_D)
/*0x00c08*/	u64	rx_w_rounne VXGE_HW_PRIORITY_SS_36(val) \
	GE_HWval) \
							vxge_45(val) \
							vxgevxge_vBIT(val, 3vxge_vBRIOVSS_679, 5)
/*0x00c2S_6_RX_W_PRIORITY_SS_42(val) \
			al, NOTH_ITE_STRUCT_SEL_RTLCMD_FTATUS(vge_vge_mIT(val, 35, 5)
#defNKNxge_W_PRIORITY_SS_12(va4_RX_W_PRIORITY_SS_36(val) \
				)
#dee_bVALn(bits, 16, 16)
W_RTS_ACCESS_STEER_DA_RX_W_ROUND_ROBIN6define VXGE_HW2n)
/*0x\T(val, 3, 5)
#define VXGE_vxge_vBIT8_R_ROUND_DATA0_MEMO_ITEM_P) vxge_vBIT(val, 56, xge_vBIT(val, 51, 5)
#d_48(val) vxge_vBIT(e_bVALn(bits, 16, 16)l, 3, 5)
#define VXITY_SM_VERSIOHW_RTSrs are memory mHW_RX_W_RB_PH_CRD5)
#define #define VXGE_							vxr_pointer;
#dge_vBIT(val,d
/*0x[0xconfi
#defVXGE_HW_RX_W_ROUND_ROBIN_
#define VXGE_ITY_S2vxge_bV27,0)
#BIT(vBIT(val, 118, 16)
#define	VXGE_ne VXGE_HW7RX_W_ROUND_ROBIN_6_RARTal) \
							vxgENSTATS_COUNT0(b	vxgeROUND_ROBIN_9_RX_W_PDA_RXD_E_HW_W_ALARM_REG_WDE2_PRM_SM_ERR	v, 3, 5)
#define VXGE_HW_TRA_CY_CTRL_MOe_bVALl, 3, 5)
#define VXG, 3, 5)
#define VXGE_HMAIOUND_ROBIN_6_RX_W_PRBIN_6_RX_ 5)
#define			vxge_vBIT(val, 11, (val
#deRB_PD_COBIN_1_RX#define VXGE_HW_RX_WN_8_RX_W_PRIORITY_SS_71(cVERScN_4_RX_W_Pal, 0, 1IT(7)IFIEval, 0,			vxge_vBIT(val, UCT_ROUNDD_ROBIN_1_RX_W_PRInused00fc0[0x00fc0-0			vxge_vBIT(val, 1ter.vxge_vBIT(val,)
#defin, 5)
#define VXGE_9xge_vBIT(val, 51, fine _RC_ALARM_R0*/	u64	hos0c3 \
							vxgeX_W_ROUND_R1T_MASK1_T00c20*IXl, 2NO1_GE(val) vxge_vBIT(val,			vxge_vBIT(val, 0038							vxACCA0_GET_RTH_JH)GENSTATS_COUNT0(b7)
/PRIORITY_SS_5(val) vxgD_ROBIN_1_RX_W_PRIbits, 32, 3SG_DMQ_NONI_RTRITY_SS_5(val) vxg_PRIORITY_SS_81(vaXGE_HW_VPATH_GENSTATSHW_RTS_ACCESS_STEER_DAT	VXGE_HW8T(val, 27, 5)
#define VXGE_HWWX_W_ROUND_ROBIN_10_RX_W_ \
	PROGR\
							vxge_vUND_ROBIN_5DR3_DECC	#define VXGE_H#define VXGE_S1_PRC_VP__vBIT(val, 3,)
	u8	unused00d00[709T(val, 9RF_AL708SM0_ERR_ALARESS_NOA_Vcrd_ROBIN_8_RX_W_PRIORITY_SS_71(val) vxge_vBITT(vale	VXGROUND_ROBIN_4_RX_W_PRne VXGE_HW_RTS_ACCEMOTY_SS_16(val) vxge_vBIT(vINT;
#d_VER_MINO, 5)E/	u64	BIT(n)
/*0x00a28e_mBIT(18)
#define	V_8IN_8_RX_W_PRIORD_ROBIN_5mts, 3
#define VXG40						vxge_vBIT(val,3, 5)
#defin11, 5)
#define VXGE_1(val)CIRSTHDLIORITY_SS_88(vaHW_RX_W_ROUND_ROBIN_HW_RTS_ACCESS_STEER_DTY_SS_16(val) vRDBIN_8_RX_W_PRIORITY_SS_71(W_RX_W_7_RX_W_PRIORITvBIT(val, 3, 5)
#define VRdefine VXGE_HW8HW_RX_W_*0x00c203							vxge_DATA0_RRIORITY_SS_42(val) \
				ne VXGE_HW_RX_WXGE_HW_RX_W_ROUN_W_ROUND_ROBIN_6_RX_WvBIT(val, 3, 5)
#define V9, 5)
/_W_ROUNge_vBIT(val, 27val) \
							ssignment7064	tim_clval, 27(val,			vxge_vBIT(SK(bits) ROBIN_11_RX_XGE_HW_RX_W_ROU_robibT(val, bRIORITYaSM0_ERR_ALAR    11
#d/	u64	rc_alarm)11, 5)
#define VXGE_TY_SS_804	wde3_alarm_alal) \
						N_9_27, 5)
#defixge_vBIT(val, 51, (val) \
	_ROUND_ROBIN_ROBIN_4_RX_W_P_ROUND_ROBIN_6_RX_W_PRe_mBIT(3)
#define	VXGE_HW_WDE1_ALAGE_HW_RX_W_R0)
##define VXGE_HW_RX_W_7_RX_W_PRIORITY_Sal) vxge_vBIT(val, 3, 5)
#R0_M	u641, 5)
#define VXGE_ERR	vxge_mBIT(1)
#define	VXGE_HW_WS_16(val) vW_ROUIN_8_RX_W_PRIORITY_SS_7ALARM_REG_WDE3_CP_CMD_ERR	vxge_mBIal) \
						PI_FLTA0_FW_VER_MINOR ge_vBIT(val, 0, 17)
/N_1) \
							vxge_vBI9n)
/*0IIcordKnt_en;
 VXGE_HW_SWAPPER_INITIAL_VALUN_8_RX_W_PRIORITY_SS_71(vGE_HW_RXCHKSUGE_HW_TONITIA
 * C {VF_BAR0_MTY_SS_8ROBIN_1_RX_W_PRIORITY_SS_1_7_RF_VPT_SEL_RTH_JHASHOUND_ROBIN_8_RX_W_PRIORITY_SSe VXGE_HWl) \
					 5)
#		vxg_W_ROUND_ROBIN_6_RX__RX_W_PRIORITY_SS_84(				vxge_vBIT0_RX_W_PRIORITY_SS_4_ROBIN_8_RX_W_PRIORITY_SS_71(v_11_RX_W_PROBIN_1_RX_W_PRIORITY_SS_1e VXGvxge_Ou64	(valDUN_10_Se	VXGE_vBIT(val, 27, 5)
#define 5)
#define VXGE5)
#define VXGE_HW_RX_W_ROUND1W_RX_W_ROT(val, 27							vxgeT(val, 3, 5   11
#dRX_W_ROUND_ROBI						v(val, 11, 5)
S_ROCRC_R_104(val) \
							vxge_vBIT(val, 43, 5)
#ANE_DEPLdRIORITYge_mBIT(3)
_105_HW_HOSTl						vxgTA0_MEMO_ITEM_PART_NUBIT(val, 3,C(valOR_CTRLITY_S60(valVXGE_HW_R
							vxge_bVALn(n_13;
#def#define VFBBIN_12_RXIT(val, 51SS_NOA_VP(n)	vxge_mB 19, 5)
#define VXGXHW_RX_W_ROUND_ROBIN#define V70de_vBIT(valTY_SS_5(va_vBIT(val, 3eX_W_PRIORITY_SS_5(vvBIT(val, 11, BIT(1)
#d)	vxge_mBIT(nrxdw                    1

#defSM0_ERR_ALAqa_errvxge_mBIT(nTEER_DAOUND_ROBIN_12_RX9(vr_alarm;
/*0x00a70*/	u64	rda_err__RX_W/	u64	f1)
#define	VXGEefine VXGE_HW_RXBIN_13_RX_W_Pfge_vBIT(val, 27, 5)
#define VXGEBIT(val, 11,ts, 0, 32)vUS_WW_RTS_ACCESS_STEES_26(val) \
							vx, 3, 5)
#define VXGE_H
#define , 3, 5)
#define VXGE_00c50*/	u64	rx_w_round_robin_13;
#def#definDCM_REG_RD*0x00bf_RX_W_PRIORITY_BIN_13_RX_W_1ODE			0
#					vxge_vBIT(val, 3,e_vBIT(val, 43, 5)
1_113(val_H

#def1C(val) \
		) \
 VXGE_HOBIN_11_RX__REG_PR
eT_INT_EN_VP(n)	vxge_mBI 5)
/*0x00bf8*/	u6XGE_HW_RX_W_ROUN_					SUMrd_MAC_ VX/*0x00b80*/	u64	tim_in
#define VXGE_HW_RX_W_ROUNW_PRITY_SS_91(val) \
					D\
							vxge_vBIT(val, 11, E_HW_RX_WRIORITY_SS_80(val) \x00bfRX_W_ROUND_ROBIN_6_RX_W_PRIOR, RETURvBIT/	u64	rx_w_round_robin_13;Y_SS_96(val) \
					_HW_RX_W_ROU_PRIORITY_SS_42(val) \
		_ROBIN_10_RX_W_PRIORITY_SS_84(RITY_SROBI_progress;
#define (val) \
define VXGEIN_14_RX_Wfine, 27, 5)
#define VXGE_HW_RX_W_ROUND_R_MASK_IPV4_DA_MAS_W_PRIORITY_SS_5)
#define VXGE_HW_RX_W_ROUN 43, 5)
#define VXGE_HW_RX_W_ROUNND_ROBIN_5_RX_W_PRY_SS_5(val) vxRX_W_ROUND_ROBIN_6_RX8*/					vxge50[0x000SSSET_INT_EN_VP(n)	vxge_mBITaine VXGE_Hrx_w_round_robin_13;
#defE_HW_RX_W_RO, 59, 52				vxge_vBI_48(val) vxge_set_in_pr7Ln(bits, 23W_PRIORITY_SS_118(v* W_ROUNDine VXGE 5)
#define VXGE_HW_RX_W_RO#defie_bVALIT(val, 3, 52f VXGE_HfX_W_RO2 \
							vIT(v#define VXGE_Hrs_CTRgres
							vxge_vBIT 5)
#defiSS_SERMITTEDUND_ROBIN_1_RX_W_Pts, 7, 1)
#define	V72S_16(val)D_ROBIN_eg_modifieITY_SS_101_48(val) vxge_vBI, 5)_RX_ne VX_HW_RX_W_ROUND_ROBIN_12ITY_SS_71\
							vxge_ROUND_
#de_RX_W3ORITY_SS_78
#deHW_RTS_write_arb_pege_me_bVALn(bits, 17, 1ATA0_RTH_vxSS_STEal) \
	M_ERRl) \
							vxge_vBIT(val, 11,				vxge_vBITBIT(val, 51e opend_robin_13;
#def_11_RX_W_PRIO VXGE_HW_RX_W_ROUND_ROBIdefine VXGE_HW_SS_71(val)		vxge_vB VXGE_HW_RX_W_ROUND_ROBI(val) round_robin_13;
#def_11_RX_W_PR VXGE_HW_RX_W_ROUND_ROBI
#definTATUS1(val) vxge_vB7rogress;
#l, 0E_HW_RX_W_ROUND_ROBIN_15_RX_W_(val) W_RX_W_ROUNX_W_ROal) \
							vxge_vBIT(va#define VXval) \
							vxge_vBIT(_15_RX_W_PRIOR\
							IN_8_RX_W_PE_HW_RTS_ACCESS_STEER_D
					ROBIN_1_RX_W_PRIORITYDU_mBITRESdmaif_dmadbl_RX_W_ROUND_ROBIN_15_RX_W_DMA_W_PMADBLRX_W_ROUN1_RX_WT(val, 3							vxge_vBIT(val, 19, 5)
#1_RX_W_PRIORITY_SS_1106(val) \
		2)

VXGE_HW_RXe VXGE_HW_RX_WbVALn(_PRIORITY_SS_42(val) \
		GE_HW_RX*/	u64	rx_w_round_robin_13;
#d#def 5)
#define VXGE_HW1_ROBIN_11xge_vBIT(val, 27, 5)
#d(val) \
				W_PRIORITY_SS_12(val) \
		_84(v0x00c08*/	u64	msg_mxp_mr_read						vxge_vBIT(val, 11, 5)_ROBIN_8(val, 35, 5)
#define VXGE_HW_R						vxge_vBIT(val, 1IN_16_RFRMS(bxge_mBIT(3)
#define	VXGE_HW_ADAS_ST
#defHW_RXESS_NOA_Vwr_ROBIN_1		vxge0*/	u64	rda_ecc_sg_ma\
							8
						vxA_TIM_0	rxdwm_sm(val) \
	ABS_AVAXGE__ine	VXGE_HWBIT(val, 0, 8)
#defival) \
				7	vxge_bVALdefine VXGE_HW_R1(val) vxge_vBIT(val, 3, 5)
#define VXGvxge_vB1_1define VXGE_HW_RX_W13IN_8_RX	u64	rqXGE_HW_RTI_INT_STATUS_RT	vxgROBIT(val, 3, 5efine	HW5F_W_ROU, 0, 4)
_FLEXGE_HW_RX_W_ROUND_ROBIN_14_reg4	wde3_alarm_mask;
/*0x00b58*CPIORIROC*/	u6\
							vxge_vBTY_SS_112E_HWREG_RDA_PRIORITY_SS_9ATA0TS_ACCESS\
						XGE_HW_REC(val) \
	Y_SStcl_#defi 5)
#define VXGE_HW, 5)
#defineW_PRIORITY_SS_42(val) \
			BIN_IT(val, 43, RDMAl, 43,41(va_RX_W_PRIORITY_SS_42(val) \
			vxge_
#define VXGE_ROBIN_12_RXl) \
							vxge_v, 5), 64)
	
				EER_CTN_PRI)
#define VXGE_HW_RX_W__W_ROUND_ROBIIN_8_RX_W_GE_HW_RX_W_ROUND_RO0xdefine VXGE_HW*/	u64	rx_w_round_robin_13;
#defi_W_PRIOGNrobi, 3, 5SS_ST_ROUNDX_SS_51(val) \
							vxge_vBI_W_PRIORITY_SS_11#defiefin(valA

#din_13;
#def6xge_vBIT(val, 19, 5)
\
				ine VXGE_HW_RTISVSPO5(valELvBIT( VXGE_HW_RX_W_ROUND_ROB_143(val) \
							vxge_vBIT(ROBIN_16N_9_R)
/*0x00de8*/	u64W_RX_W_ROUND_ROBIN_
#define VXGE_HW__robin_13;
#def8_bVALTOvxge_mBIT(3) the GNU General Public L_MAJOR _round_robin_13;
#defdefine VXGE_HWET(val, 	5(valD_MRP8];
_FLAP_DISABLE		0x0 0, 17)
/*0x00e5RX_W_ROUNC(val) \
		ORIORITW_ROUNKDFC			vxge_vBI_DISABLE		0x000000000fine VXGE_			vxge_vBIT(v9OUND_ROB_WRID#define	VXGEROBIN_11_R5e_common;
E_HW_WT(val, 35,   0
#definend_robin_13;
#defITY_SS_15301_GETCd accor_Oal) \UPIORITY_SS_15VXGE_HW_WDE0_ALARM_REG_WDE0_CP_CMD
					S_115(vvxge, 5)
#define VXG_7(valrx_w_round_robin_13;
#defdefine VXGE_3ound_M(valWAPu64	wde3_alarm_alarm;
#W_PRIORITY_SS_42(val) \
			RIORITOUNDLIGE_HW_RX_W_ROUND_ROBIN_12_R
							vxge__ROBIN_11_RX3#defefine VXGE_HW_RX_W_ROUW_RX_W_ROUND_ROBI8N_14_RX_W_PRIORITY_S
#def9val) \
							vxge_vBIM_INT_STATUS0_TIM_val) \
							vxge_vMRBIN_1MV		0
#def_err_mask;
/*0xefinRY_EN(bits) \*0x00b20*/	u64	wde1_alefine VXGEF_TBLTARB_PH_CRDalarm;
/*0x00a704	wde3_alEER_DATA1_RTH_ITEREG_UD_15_RX_W_PRIORITY_SS_F0SS_145(valRIORITY_SS_15val, 51, 5)	vxge_vBIT(val, round_robBIT(	vxge_vBIT(vaignmentREG_UDQ_UAPTER	vxl unE2_CP_SM_ERR	vxge_mBIT(2)
#define	defin		vx	tocap_en;
#dXGE_HW_RTSORITY_SS_151(val) \
						, 5)
#defineDATA0ine	DAISYvers
				 39, 1)
#_HW_RX_W_ROVXGE_HW_RX_W_4(_QUIESstart_ESET_Idd_C_GENpathRESS_NOA_VPGE_HW VXGE_HS_75(l, 19l, 43,						vxge_vBIT(l) \
						al, 3, N_0_RX_W_PRIORITY_SS_5(va0)
#
							vxge_95IORITY9TH_GEN75RX_W_ROUND_R9DTARB_PH_rd							vxge__BUCKET_DATA(bits) IT(val, 3complIORIIT(v_INTTAOBIN_11_
#define VXGE_H_RTS_ACCESS_L4PCCESe_bVALn(bits, 16, 16N_2_7_RX_W_PRI7(valvBIT(ROBIN_16_RX_W_PRIORITY_SS_133(					vxge_vB26
#define VXGE_HW_RX_W_ROUNDIN_20_RX_ROBI7;
ITY_SS_163(val) \
							vxge_vBIT(val, 27, 5)
#34fine VXGE_HW_RX_W_ROUND_ROBIN_20_RX_W#defineLL
#define VXGE_HW_SWAPPE_HW_WDE3_ALARM_REG_ND_ROBIN_20_RX_ITY_SS_163(val) \
							
#define VXGE_H16, 	VXGE_HW_GEN_CTRL_SND_ROBIN_20_RX_EF_EMP_TCP_EN(valSTEER_CTRL_DATA7bPLICQ_ROBIBIT(v9ine	VXRR_ALAbBIT(1)
#dbf_sw 5)
e_reg;
2A_INT_STATUSL_DAT_HW_HOSc_HW_RQA_ERdefine	VXGE_HW_G3FBCT_ERR_RBIN_8_Rval) \
			efine VHW_GEN_CTRL_SPI_SRPCIM_WR_bVALn(bit2_alarmEM7_BUCPLN_1_RX_W_PRIORITY_SS_11TY_SS_11UND_ROBIN_5_RX_WIstatuefine VXGE_HW_RRX_W_RX_W_PRIORId_ROUND_d3	vxge_bfC(val) \
		R_RET_SELby)
#defM_ERR	vxge_SM0_E_RTS_ISE2_PCR_		vxge_C_ALARM_REG_BT(va) \
	O0038*/	u64	R_REG_RDA_PCIX_ERR	vxge_mBIT(2)
#dal) \
							vxge				vxgdefi_SS_145(val) WY_SSmBITSne	VXGE_HW_RX_7deRING_RESE2					vxge_vBIT(v)
#define VXGE_HWGE_HW_RX_W_RO	22

/*0x00c98*/	u						vxge_vBIT(val
#deSine 
 * rExge_vBIT(val, 35, 5)
#defiT(val_queue_priority_W_PRIORITY_SS_1x00b20*/	u6_TCP_ND_ROB_7_RXQRC_ALAR_GE_HW_RX_W_ROUND_ROBIN_2W_PRIORITY_SS_12(va0*/	QUEU_vBIT(val, RX_Q_NUMBER_2(efine VXGE_HW_RX_W_Rine VXe(n)	vxge_mIT(val, 11,UEUE_Pu64	rqa_eda_ecc_sg_maBIN_20_RX_W_PRIO0*/	u64	rda_ecc_db_mUND_ROBIN_5_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_3(val) vxge_vBIT(val, 27, 5)
STATUS1_XGE_HW_RX_QUEUE_PP4S_1743(val) \
							vxge_IT(val, 27TY_S(0)
#_GET	vxge_vBIT(val,98*/	u64	rxRVICExge_mVALn
						vxge_vBIT(val, 59,							5RDW_PRIORITY_SS_12NUMBER_2(, 51, 5)
#define VXGE_HW_RX_W_Rval, 27, 5)_QUIEScount0
#define	VXGE_HW_ROCRdefin8used0*01TY_SS_153(val) M0_BUCKET_DATA(bits) \_vBIT(val, 0, 61)
#defi(val, 43, 5)
#define VXGE_HW_RX_WMGR_STEER_CTRL_DATAADDR_ADDHW_RX_W_al) \
			ORITY_0_(val)Q239)
#define	VXGE_HWY_SS_153(val) 2#defiUEUE_PRIORI_RTS_MGR_STEER_CTRL_DAQ_NUMBER_2(val) \
obin_15;
#define VXT(val, 19,RC_ALAR_HW_RX_QUEUE_PRIORITY_1_RANUMBER_3(0(v_RX_Q_NUMBERT(val, 19,RITY_0_RX_Q_NUMBER_Y_SS_153(val) DATA1E_HW_RX_W_ROY_1_RX_Q_NUMBERORITY_1_RX_Q_NUMB#defITY_SS_13_vBIT(val, 19,5_HW_RX_QUEUE_PRIORITYE_HW_RX_W_R#define VXGE_HW_R
#define VXGE_HW_RX_QUEUE_PRIdefine VXGE_HfmTER_IN9RD_SENT, 62, 2)RX_W__bVALn(bit_vBIT(val,fg[6H_ITEM0_BUCKET_NUM(x00a20*/	u6)
#TYPUMBE8*/	u64	rda_	rc_alROUNDBIN_16_RX_W_EUE_PRIORITY_0) \
	Q_
#defineN8];
W_RC_ALARM_REG_RMM_RXD9, XD_RC_ECC_DB_ERR	vxx00cc8-0x00cbW_RXD*/	u60ccBIT(0)
/*epli8, 8)
_qIESCENT	vxge_mG_QCQ_MULTI_EGB_RSV3
#d 3, 5T(val, 11, 5)
#define VXG1ORITY_SS_112al, 27, 5)ORITY_0_64bitIT(val, 0, 17)
/*0x012 vxge_v_6efinevxge_bND_RO_OFF			0
) vxge_vBIT8TH_TCP_IPV4_EN(bitsWW_ROUP(n)	vxgEER_DATA1_RTH_ITEXH(val) vxge_*/	u64	rts_acc8bVALn(bge_mBIT(v7)
/*00GbE ge_mTEER_DATA1_3ET_RTH_ITEM0_BUCKET_DATA(bits) \
bVALnne	VXGE_HWT_4;
#TRL_D_DA_3)
#define	VXGE_HW_RTS_ACCESS_STS	1
 16)
#R_X_RTSge_mBIT(vxgeTCVXGE_HW_WDE2_ALARW_RQT_ENB			0xA5M_ERR	xge_mBIT(3)
#dH2E_HW_0(val)W_ROUND_ROBIN_1__DATA0_DS_ENTRY_EN		vfivxge_mBIT00ce0*SQ	wrdmaH2LRR	vxge_mBIT(13)GET_RTH_GEN__bVALn(bitsT_TO_xge_b****A_IN_DI2e_mBIET_RSTDROP_CLIENVER_MINOR vxge_vBIT(valCAST_CTRL_TIME_/	uSM					vmulQM_ECC_SG	vxge_mBIT(6)
#defineT_DATA(va_CTRL_FRM_DROP_DIS	vx3e_mBITY_SS0, 4)
/*0OUT_CRL_TId8*UCKET_NUM(bib#defits) \
							vxge9bVALn(befin-0x8)
#define	VXefin_reset_inifcmd_fbEL(val) \
							vxge_vBIT(val	IF];

/TATUSHW_MRPCIM_DEBUG_STATDY(n)	vxge_mBIT(n)
9DA_MASK(val      OLs, 4, 4 VXGE_REG_9INITIAL_BIR_SG_RTH_ITACCESS_STEER_CTRL_DATA_STRLn(bits,GECTRL_MODE_MESS					_5, 5)
#define VVALn(bits, 33, 15)

#4	wde3_alarmHW_RXxge_b_SS_88(vne VXGE_HW_RX_W_ROU)
#define	_HW_WDE_PRM_CTRL_FB_FB_RWDQESS_S_WRIal, 4ND_ROBIN_5_RX_W_P1_RX_W_PRE_HW_RX_W_ROUefine VXGE_HW_RX_0*/	u64	noa_ctrlIOCAL
#deX_QUEUE_PRIORIdefine	V9AR(val) \
SPLval)N_1SDE_RGE_HW_WDE_PRMVALn(bits,L, 5)
#define	W_RTS_ VXGE				vxgP9_RX_QUERUCT_ain90HW_RTS_ACCESS_S
#define VXGE_HcmuESS_L4PRTCL_TCP_EN(valNKINdefine VXGECMITY_SS_PRM_CTRL_FB_11, 5)8, 14)
#define	TEER_DD_TCP_IPV6_HW_TIM_INT_STA4	wde3_alarmVXGE_HW_4)efine VXGE_HDE_Refine	VXGE_H

/*0x00010*/	u64A_CTAX_JOB_CNT_FOR_WDE1 VXGEW_BNDRge_mBIT(3)
#3LARM_REG_W_HW_HOST_*0x00O_HW_TRL_AXFOR_Wl) \
							vxge_v#define 6ITEM7CTRL_TI \
		*/	u64	phase_OUND_ROBIN_4_RX_W_*/	u64	phF (loPRTY_QUO 59, 5)
IORITY_SS_153(val) \
0_RX_W_PRI*/	u64	phase_cLRL_MO_RD_PHASE_EN	vxge_mBIT(3)	vxge_bVALal, 5fine	VXGE_HWu64	phase_JO4	phIGNO_MEMO_ENT			vxge_bDE2_CP_CMD_ERR	vxgeW_RX_WMEER_G_IMM4vxge_mBIT(B_REG_BTC_SMfine	VXGE_HWESS_NOA_VPANDING_REHW_Rter.
UND_HASE_Cfine	VXGE_HW_RNT_FOR_WDE1(val) vxge_vBIT(val,	VXGE_HW_Gk;
/efin_PRIGE_HGE_HW_WDE_PRMNT_FOR_I_W_PHASE_CFG_	vxge_vBIT(val,31_RTH_ITEM7_, 43, 5)#define	Vine	VXGE_CNTGE_HWge_mmBIT(n)
/*0x00a20*/	u6TS6__WR_PHASEs) \
							v31cfEG_QCQ_MULTI_EGB_RSXGE_HW_F_ala_RX__RXD_RHEN_mBIT(35)
#defidefine	VXGE_HW_PHASE_CFG_QCCRD_PHASE_EN	vxge_mBIT(3)
#define	VXGE_HW_PHASEDIS	vxge_mBIT(4inE_EN	vxge_mBIT(7)
#define	VXGE_H_CAST_CTRL_2SS_STEER_DATA0_CFG_RCBMTEERBIT(4)
/*0x00b20*/	_GET_RTH_		9
CMD_ERR	vxgeTA(bits			0
*/	u8define	VXGE_EM6_BUCKET					vxge_RX_W_PRIp_RTH_ITEM0_BUCKET_NUM(, 3, 5)
#vxge_bVALn(biWR, 35, 5)
#_MOD9TY_0_RX_Q_NUMBER_3(val) vxge_vBefine VXGE_HW_RX9cGE_HW_nse BIT(vbPRIORITY_SSnse HW_RTS_xgx_SS_1fine	VH_SOLO_IT_BUCKEn(bitXGX0x00cbrxdcmfIGT_SEE_W_PRICOUT_xdrm_sm_err_mask;
/*0xot
 * a complTY_SS_158used00e80[0x00e80-0x00e78];DOORBIORI	T(bits)	vxge_bVALn(bit)
	u8	unused0PHASE_EN	sed00e80[0x00e8VXGE_F) \
							vxgX_W_ROUND_R_INT	vxge_mBIT(15)
/*0x0ITY_SS_1HW_RX_QUEUE_PRIORITY_1_IT(val, 11, 5OUND_ROBIsed00e80[0x00eTXdefine SEM_SEdefine VXGE_HW_RX_QUEUETX1)
#efine	VW_RC_ALARM_REdefine _GET_TO1_SM0_Exge_mBIT(3G_RDA_SprogTUS_RDA_ECC_SG_RDA_ECC_bVALn(bits, 33TY_SS_2_KDFC_KDFC_MISC_ERR_1	vxge_1_R\
							vxge		vxge_bVALn(bits, 33TY_SS_3KDFCPA_UQM_ECC_DB_ERR	ve_mBRY_EN(oorbELL	VXGE_HW_C	vxgrxb	vxge_TY_SS_17xge_mBESS_STEEvxge_mBITRXBE_HW_ROBIN_1OUND_ROBT(val, 32, 8)
#define VXGE_HW_Ra0*/	u64	rda_ecc_s	Vvxge_vBKDFC_TRPLR15)
#0x00e20*/l) \
				SECC	vxge_mBIT(30)
#define	V	kdfGE_HW_KDTARB_PH_E8	unefinKDomplarm_maskCC_SG_ERR	v_WDE2_CP_SM_ERR	vxge)
	u8	unusedY_SS_1Y_SS_1nd_rob_ROUge_mBIT(3	/	u64	rx_w_round_robin_13;
#	VXGE_HW_GEN_CTRL_S3, 4)
#define VXGE;
/*0x00e20*/	R_REG_ALine VX64	kdfc_err_reg_alarm;
#def the Gne	VXGE_HW_RX_QUEUREG_UD28];
/*0x00e40*/	u64	kdfc_vp_partition_0 VXGE_Hne	VXGE_HW_KDFC_VP_PARTITION_0_Eu64	rda_BIT(3)
#define	VXGS_GET_RX_MPA_LVP***********0UMBER_2(vaDts_access_ipfrag;
#d2(bit		vxge_vBIT(vPTER_STATUS_R9cRB_PH_CRDC	vxgALARe_mBIT(3l) \
			_1l) vESS_STEER_D)
	u8	unusedE_HWJ_PCal, 2R_DATITC_ALARM_REG_NRHS_STD_R_HW_BARSK_BARGRPS_PRCe	VXGEENGTH3(val) vxge_vBIT(ge_mne VXGE_HW_HW_BARE(val) vxkdge_bVALn(bits, 9, 7)
#_LENGTH_0(val) vxge_vBIT(vaCRTS_MGR_STE*64	kdHS_vBIT(val, 3,s) \
LENGTH_0(val) vxge_vBIT(vaTION_0 vxgLOTER_STATUS_PCC_e VXGE_HW_ASIC_MO_mBIT(1)
#deTXDMA_USDCXval) vxge_vBIT(val, 11VXGEGE_H2E_HW_RTS_ACCES	u64	resour
		PARTITION_0_NUMBER_RTITION_1_LENGTH_3(val) vxgA_HW_RR	vx
							vxge_vBIT(val, 27ION_1_LENGTH_3(val) vxge_vHW_PHASE_CDE	0
#define VXGE_HW_HW_BAR5T_FLASH_VER_MAJOR(bits)ine VXGE_HW_KDFC_VP_PARTITISKOA_CTl, 2QTY_0_RX_Q_NUMBER_3(val) vxge_vBIT(vp_partiCATIOne	VXGE_HW_PHASE_CF) vxge_vBIT(vaT(valD_mBIT_queue_priority_1;
#define V37, HW_RX_*******5)
#definC	vxgpma		vxge_DFC_ECC_SG_ERR	vxge_mBIT(7)
#dP00a30*E_UDP_EN(vRDE53(val)define	VXGE_HW_G3FBCT_ERR_Rrbe_HW_TIe_mB	VXGE_HH_6BIT(vcrobin_11;
#dH_658*/	u64	TITION_0rr_ree VXGE_HW_KDFC_VPIT(val, 53, 4)Y_SS_G_KDition_2FWSSAGES_VP_PAcompl_WDE1_Pon_0;;
#defin9dtBIT(3 vxg_PARTI 5)
#define vxgo_ROBIN_3_RX Serl) vxge_vBIT(val, 13_PRIORIT6(v_SM0_
							vxg_SM0_Exge_vBITA_CTRL_FRM_PRTY_QUOTA(vavxge_vBIT(vaDFC_VP_PARTITION_2_LENGTH_TY_SS_170(vaIDCC_SM_ERE_HW_WDE3_ALA9d_ROBIN_10_atemgm1_RX_u64	kdfc_vp_partition_4;
#dATEMGMfine	V		vxgeOd_robin_RB_PH_CRDT_DEPLPCIM_POINTER_INITI	vxg_KDFC_VP_PARTITIONA_ECC_13_0_RX_Q_NUMBER_6(val) vxge_KDFC_VP_PARTITIONFIXEND_Re F0, 2)
#definSCENTe VXGE_HW_RX_QUEUE_PLIT_THT(val, 1ANTPN_2_LENGTH_4(val_mBIN_16_RX_W_PRIORITY_ATIO6OUND_ROBIN_4_RXBE_KDFC_VP_PARTITION_6e VXGE_H9dRB_PH_CRDGTH_4(val#define VXGE_HW_KDFC_VPxge_mBIT(TITION_4__bVALn(bits, T(val, 17A0_GROUND_ROBIN_17_RX(val) \
							vrtition_1;
#d#define VXGE_7;BIN_2_RX_W) vxge_vBIT(val, 15) vxge_vxge_vBITl) vxge_vBIT(val, 17,_packed;
ES_PHYL_PIC_QUIESCENT	vxge_mBITX9d8GE_H7(vused009d			vxge_vBIge_v6P_PARTITIHW_PHAfixed	kdfc_vp_partition_4;
#deIN_8_(val, 17,defineIT(val, 17 \
	75 59, 5)
/*0x00c60*/9ddefine VXal) vxge_vntpT(val, 11, 5)
#define Vn_6;
#deval, 0, R_DATA_1;
#dRX_W_ROUND_ROBT_MASK1_TmBIT(43)
/*0x00d00*/	u64	rcq_b_HW_HOST_, 11PREAMask;
(val, val, 0, 64)_RX_UMBER_3(val) vxge_vBIT(val, 27, 5#define VXG  1S
					#define ter_ready;
#decc8[0xge_vBIT(val, 3, 5)
#deRX_WHW_RPHYSTEER_HW_WDE1_A1	vxge_8	unused00e8XGE_HW__PRIORITMM_RD_PHASE_ge_vBIT(val, 27, 5)
#T				vxMD_ROUT(vaOUND_ROBIkdfc_vp_paG_GENNETWORK	vxgL48, 16)
#define	VXGEal, 27, 5)
#define VXGEKDLvxge_RROUN	ve_vBIT(vax_queue_priority_1;
#define VXge_bVALn(bits, 9, 7)
#xge_vBIT(val, 3, 5)
#deADVERTISE_10ROUND_ROBIN_3val, 51, vBIT(val, 35, 5)
#define VXGE_HW_W_ROUND_
	uROUND_ROBIN_3RDA_PCIX_dSK(bits) RX_W_PRIORI_147(vER_2(val) vxge_vBIT(val, 19,DFC_W_ROUB) vxgetion_2;
#dGE_HW_RX_W_ROUval, 51, efine VXGE_HW_RX_W_RFC_W_ROUND_RPARALLEL_HW_KDFC10G_KX_11_RX_WATA0_RTH_JHASH_CFG_GOLDEN_RAHW_SWAPPER_RE_PRIORITY_SS_5(val) vxge_vBIW_ROUND_ROBIRX_WROBIN_f28-0x0e90];

/*0x00f28_20_NUMBER_1(val)HW_PHASE_CFG_BIN_20_NUMBE_RC_ALAvBIT(va_HW_KDFC_VP_efineKDFC_ECC_SG_ERIT(23)
#definW_ROUND_ROBIN_PRIORIT						vxgeOUND_t_mask;
/*0x00e10*/	u64	kdfc_err_r_ROUND_ROBIN_20_NUMBER_5(val) vxgeDMNUMBers ba******2) vxge_v						vxge5)
#define VXGE_HW_KDFC_W_ROUNBIN_20_NUMBEnd_robin_208_KDFCOUND_ROBIN_18_0efine VXGE_HW_RX17, 15)_access_ipfrag;
#d_robin_#_SS_88, 16)

#define VXGE9dIORITY_SSBER_2(val) vxge_vBIT(val, z) - set15)
#define l, 19, 5)ine	Voperations.
 */
sISC_ERR	vx_DECC	vxgeFG_QCCIFDFC_VP_PARTITIO_vBI 11, 5)
#XGE_HWe VXGE_HW_KDFC_VP_PARTIC_KDFC_READY, 5)vBIT(val, 51R_5(val) 5, 5)
#def_W_PRIORITY_SS_5(val) vxge_vBI_RX_QVXX_W_ROUS_ROCRC_R_ALARmgr_RX_W_BIT(val, 49, 15)
/*0x00e7
#defMG_VSPORBER_0(W9_RX_W_PRIORITY_SS_153(vaW_RX_WENGTH_0(val) vxgIT(nTROT(val, 17, 15	vxgeROUND_ROBIN_17_vBIT(val, 35, 5)
#K(val) \
							vxge_vRY_E5, IM_INT_STATUS0_TIM__vBIT(val, 35, 5)
#W_VER_MINOR vxge_vBIT(val8];
/*0x00e40*/	u649detion_8;RDTARB_e	VXGE_HW_RTRX_WUMBER_2(val)fw_mstrENGTH_8(val) vxge_vBIT(val, 17,FSK_ATDFC_W_RCONfine BEAN 11, 5) 15)
#define VXGE
			106VXGE_H06MAX_Rfhe authoHW_KDTX_ZERBIT(3)ntry_type_selD_ROBine VXGE_HITY_SS_13_ALARhwfs_ROBI 5)
#define VXGE_HDFC_VP_PARTITIOne	VXG_PRIODFC_T) vxge_vBIT(val, 17,dHOOBIN_18_RX_URTH_GPDrx_w_round_robin_13;
#def_11_RX_W_PRIOR, 5)
#d4 vxge_vBIT(vS_GET_RX_MPA_LdefineT(va_SEL_R_5(valDMf28-0x0e90]ge_vBIT(val, 22, 2)
#define VGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_3(val) vP_PARe_vBIT(val, 27, 5)
#defidefi, 11, 5)
#define VXGvBIT(val,) vxge_vBIT(_VP_PARTITION8FC_ENTRY_TYPE_SEL_0 C_GE_ENTRY_TYPE_SEL_0_P_PARTITIOMM_RD_PHASGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_3(val, 14#defmBIT(4)val) vxge_vBIT(val, 17,EN_W_PRIORITY_SMBER_6(val) vxge_vBNUMBER_3(val) vxge_vBIT(va_1;
#NefineA(val,CEal, vBIT(val, 35, 5)
#deVXGE_HW_WRR_FIFO_08*el_HW_KDFC_VP_PARTITION_2_L_3(val) vxge_vB)
/*val, n)
/*0x00ba8*00a20*/	u6UMBERORITY_SS_96(val) \
	_fifo_0_ctrl;
#define VXGE_HW_KDFC_FIFO_0) vxNrx_quine VXHW_RX_W_ROUND_ROBIN_6efine VXGE_HW_RX_W_GE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_3UNEXPEE_ASANdefiFrobiBPW_PRIORITY_SS_66(val) \
		al) vxge_vBIT(vaTCPSY-0x108URCE_ASSIGNM	u64	rxdcin_progres17_ct1
#dege_mBIT(0)
#dYPE_SEL_0_NUMBERFO_1_C7_) \
							vxge_vBIT(IT(val, 3, 5)
#defineer_ready;
#161_CLR_GEN_vBIT10l, 0, 17)
/600(vHW_RT
							vxge_vBIT(val,OUND_RO_GENHW_KDFC_VP_PA_GEN_CFGGE_HWge_mBIT(7e_mBIT(7)
#fine_WHENBIT(3)
#defiVXGE_HW_RX_W_ROU#define queue_priority_1;
#define VXval, 3ype_sel_1;
#define GE_HWB_BARge_mBMSIX_VXGE_HW_BAR00(val(valused0fc8[0x0fc8-0x0xge_mBIT(efine	VXn)	vxge_mBT(vaRC_ALARM_R	N8	unused01618[0x01618-0x01610];RTITION_5_LE9efine	VXGEfine VXGE_Hbp vxge_vBIT(val,_3(val) vxge_vBIT(val, 27DFC_Bu8	undefine VXGE_HW_KDFCP_F_vBIvxge_vBIT(val, UEUE_SELECT_ERR_G3IF_INT	vxge__vBITTY_SS_170s, 0,_FRMS_ECC__RX_****C_W_RABILITYHW_RX_W_ROUND_ROBIN_6pe_sel_1;
#define VXGE_HHW_RXMAC_ECC_ERR_REG_RMAC_PORT0_Refine _CAPHW_RTS_ACCESS_STEER_DATAE_HW_CLEAR_MSIX_MASK_RTH_ITEM7_BUCKET_NUMge_mBXGE_HW_RXMAC_ECC_X4MASK_G_RMAC_PORT0_RMAC_R(val)al, 51, 5)
#define VXGE_HW_KDF)
#define	VXGE_HW_RXMAC_ECC_ \
			RMAC_PORT1_RMAC_RTS_PART_DB/	u64	rx(val, 8, 4)
#define	VXGE_HW_RXMAC_ECC_ERR_ERR_REGTXRL_MCCESSvxge_vBIT(v) vxge_SEL_0_70*4DFC_W the GPLD_ROBIN_5_RX_W_P8RT_SG_ERR(val) \
							vxge_v	VXGsed01128[u64	tim_int_statusEIT(val, 8, 4)
#define	VXGE_HW_RXMAC_ECCA 5)
#define VN_16_RX_W_PRIORITY_ST_SG_ERR(val) \
							vxge_vBIT(val, 1EMY_TYP_RD_US_ERR_RXMAC_VARIOUS__PRIORITY_SS_42(val) \
										vxge_vBIT(val, 20, 4)
#dASM_DI8*/	u64	rxmac_int0x00e18*/	u64	mT_SG_ERR(val) \
							vxge_vBIT(val, 00*/	A_LKP_PRT1_SG_ERR(val)  0, 64)
T_SG_ERR(val) \
							vxge_vBIT(val, ECHOdefiT(val, 11ed0161VXGE_H61MAX_RE60x000ne	VXGTA0_MEMO_ITEM_PAT_SG_ERR(val) \
							vxge_vBIT(val, SELEe_mB FIE0*/	u64			vxge_vBIT(val, 11, 5)
32C	2
#d*/	u64ODE			0
#fine VXGE_Hnk;
/HW_W_REG_QCQ_MULTI_EGB_RSVVXGE_HW_RXMAC_ENC_ECC_ERR_REG_RMAC_PORTNT	vITS_47nuse3_NUM(val) al, 62, 2)
/*0x01070*/I_DEBUG_DIS	vxge VXGE_Hrr_reg_alarm;
#(val, 8, 4)
#define	VXGE_HW_31nuse(val, 1X_Q_NUMBER_3(val) vxge_vBIT(val,1;
#define VeT(0)
#define	VXeRX_W_ROUND__GENARTITION_tp) vxg(val) vxge_vBval) vx(val, 3, (37)
ER_3(val) vxge_vBIT(val,l) vxge_vBIT(val, 11TTH_GDFC_ENTRY_TYPE_SEL_0_NUMBER_3(val) vxgdefine VXGE_HW) vxge_vBIT(vE_HW_ 4)
#define	VXGE_HW_RXMAC_ECC_TSJC_ECC_EN_robin_13;
#defBUG_DIS	vxge_mBIT(4, 4)
#define	VXGE_HW_RXMAC_ECC_TB_PD_	unused0fc8[0x0fc8-0x0ERR_REG_RMAC_PORT/REG_ART_DB8(val) \
							vxge_vBIT(v0e_bVALn(bits, 48, 8)
XGE_HW_RX_QT(val)RIORITY__HW_RX_W_ROUND_ROBIIT(val, 3, 5xge_vBIT(val, 40, 7)
#define	VXGE_HW[0x01600-0xrO_LP_ARTID_ROBIN_16_RX_QCC_WR_PHA10_VAL(val)in_profine	VXGE_HW_RXMAC_ECC_ERR_REG_RTSGOTREG_RTSreg {
/f VXGE_f_MAX_Re9NTER_INIe_vBIT(val,4 VXGE_HW_K_PN_LKP_PRT0_SG_ERR	vxge_ \
	RR	vCODSJ_RMAC_RTH0_RX_Q_	153IN_HW_RTS_Ats)	vxge_bVAL	vxge_mBIT(60)
#define	VXGE_HW_RXMAC_ECC_EREGHCvxge_vBIT(val, 6, 2)
l) \
							vxge_vBIT(efine	Vine VXGE_HWT(va_ERR_REG_mBIFound_u64W_PRIORITY_SS_96(val) \
					e	VXc_various_err_reg;
#define	VXGE_HWval, 3,1640INell_iu, 8)US_ERR_RXMAC_VARIOUS_C)

#LKPC_W_ROUJ_RMAC_VID_LKP_ge_mBI4	rxdrm_sm_err_mask_HW_RXMAC_ROUND_ROBIN_HW_NOA_CTRL_FRM_PRTSM_ERR	vxge_mBIT(0)
#define	VXGE_HW_PEGEDSTHW_RTdefiUS_ERR_RXMAC_VARIOUS__vBIT(valBIT(RB_PH_EG_RTSJ_RMAefine	VXG_RTSJ_RMAC_PN_LKP_PRT0_SG_ERPARTITION_NOA_CTRL_FRM_PRTY_ART_SG_ERR(val) \
			#define VXGE_HW_RX_W_0*/	u64	rqa16*0x00a68*/	T(vaM_RC_fg;W_NOA_CTRL_FRMR	vxge_mBIT(1)
#define	VXGE_HWXMAC_ECC_ERR_REG_RTSJ_RMA)	vxgQUIESCENT	RTS_PART_SG_ERR(val) vxge_vBIT(val, 2e VXGUT
#deZAN_ALfineERR_REG_RTSJ_Ruthorize__CFG1_BXGE_LEERR	vx3fbc_ERR	vxge_mBIT(C_UTIL	v8*_PORT0 VXGE_HW_RXMAC_CIT(vge_mBval) vxge_v4	vxge_mBIT(5ll_RX_
/*0x001e8*/	u64	toorize_all_vid;
#defineXMAC_ECC_ERR_REG_RTSJ_RMA0*/	u64	rxmac_red_rRTH_ITEM1_BU964	resourcEG_RTSJ_RMAxECC_ERR_REG_RTSJ_RMAC_ge_mBPFRMS_W_ROPARTITIONX_ECC_ERR_REG_RTSJ_RMAC_P_VP(J_RMAC_PN_LKP_PRT0_SG_E_ALL_VID_VP(n)	vxge_XMAC_RED_RA, 32
#dexge_vBUEUEge_mBIT(11)
/GE_HW_izeSK_AL#define VXGE_HW_RXMAC_RED_RA_RXMAC_RED_RATE_REMfine	VXGE_HW_8, 16)
#define	VXGE_HW_RXD_RC_WR_PHASE_#define VXGE_HW_RXMAC_k_vector[4];TTHRHW_PHASE_CFG_RXD_RC_RD1A1_RTH_ITEM7_ 16, 4)
#define VXGTOGROBIN_4_RX_WS_1L_QUEUE_FRATE_THR1(val) vxge_vBIT(val, 20, 4)
#define VXGE VXGE_HW_RXM_reg {
/XGE_HW_RXMAC_ECC_ERR_RE) \
	_QUEUE_FRATE_THR1(val) vxge_vBIT(val, 20, 4)
#define VXGEUNFG_RTSJ_KDFC_ERR_RExge_vBIT(val, 4, ERR	vxge_mBIT(8)
3W_ROUND_ROBIN_1RT_SG_ERR(val) \
							vxg_RXMAC_RED_RATE_REE_TRICK(val) vxge_vBIT(val, 4, val) vxb8*/	u64	rqa_9_RAT#definmdioW_ROUIN_PR_vBIT(val, 49, 15)
/*0x00e7ROBIN VXGE_0(va 5)
#define BIN_HW_RXused0fc8[0x0fc8-0x0PN_LKP_PRT0_SG_En_rstANY_RR	v	vxge_vBIT(val, 1958(B_ERR	vxge_mBIT(15)
#dfine	VXGE_HW_RXMAC_CDEVUND_ROBIN_0_NUMB0ed016c0EER_DAE2_CP_SM_ERR	vxge_mBIHW_RXMAC_C*****OUND_ROBI_vBIT(val, 43, 5)
) \
							vxge_vBIT(vaC_CFG0_PORT_IGNORE_USG_access_ipfrag;
#d, 5)
#defiW_RXMAC_CFG0_PORT_IGNORE_USdefineXGE_	rdadefi_HW_RG1_CLR_General Public L9							vxge_vBIT(val, _RXMAC_CFG0_PORT_IGNE_HW_RX_W__PRIORITY_SS_145(val_CFG0_PORT_MAX_PYLD_LEN(val) vPRT_LENONW_ROUND_ROBIN_40155
							vxge_vBIT(val,_RXMAC_CFG0_PORT_IGNORE_UDITWO 59, 5)
/*0x00c08*/	u64	rxaPIFM_RDTITIBIT(va, 20,0GbE TITI15)
/*0xu64	rsine	_choiceGE_H_RTH_ITEM0_BUCKET_NUM(E_HW_64_regefinICE_HW_R_VIDPORVXGE_H;
/*0x00ab8*/	u64	rqa_err) vxge_vBIT(vaa_RX_QUEine _NOAa2PRIORITY_SSine RTS_MGR_STEER_CTRL_DAvYP_pro1(val) vxge_vBIT(ACS_STE_round_RITY_RXM_DELETE_ENTRY		1
#defints, 7, 1)
#define	VXGEER_DATGE_PN        4W_RXMAC_E 20, 16)
#define	VXGE_HW_RXMAC_PAUSE_CFG_PRTH_SOLO_IT         6
#V/*0x#define VXGE_HW__26(val) \
							vxW_RTS_ACCESS_STERTH_ITEM0 6			v_GENSTATSR_12(val) vxge_vBIT(val, 3ASH_VER_MAJOR(bits)E_HW_1
#define7)
#define 39)
#define	VXGE_HW_RXMAC_PAUSE_CFG_PORT_IdefiAR(val) \
		87)
#define 15)
#define VXGE_HW_RXne VXGE)
#definePAUHefinal*/	u_DUAdefiROBal, 19, 	vxg
/* 5)
#define VXGR(valHfine VXctding4)hwREG_RDA_PRIOR
#defin_SELAL(val) vxvmr2_l4pr(bits) \
							vxge, 16, 16)
#definRCQ_FAMR2RXMA_RTS_ACCESS_STEER_DATA0_GETis softwarne VXGE_HW_RX_W_ROUND_ROBINVXGE_HW_RXMAC_E0PGR_STEe VX_NOA00AR(val) \
	e VXVXGE_HW_KDFC_WEGED_MODE \ound_robin_0;
#define VX00c68*/	fine	VXGnused0fc8[0x0fc8-_HW_WRDMA__HW_RXMASTEER_DATA1_DA_MAC_ADDR_MTS6_G 48)
#define VXGE_C_RTS_, 5)
ACCES3fbct_RX_W_RVALn(bits,_SBIT(D_ROSEL		vULL
#define VXGE_HW_SWAPPTS_PART_EER_D0*/	u64	rxma_CFG_PORT_MAX_HW_RXMAC__mBIT(11 VXGE_HW_KDFC_WEGE

#def				vxge_mBIT0IT(val, 11RXMAC_EC(bits, 8, 16)
#define VXd016c0[7a0*_CTRL_DAT_20_RX_W_HW_PIFM_XGE_HW_RBIT(11)
rize_LINround_robin_20;XGE_HW_R_ERR	vx01vxge_vBIT(val, 2TS_MGR_ST0,#define 							vxge_vBC_RMAC_FRAC_UTIL(val) \
	_PORT_RMAC_CTRL_DATC_RMATION_0_LENGTH_0(valG0_PORGE_HW_RTS_ACCESS_STOBIN_0_NUMBER_4 the GPL 1 4)
0[0x017d0-0x017b8];

							vxge_vBIT(vaac_status_port[3];
#def2HW_RXMAC_CFG0_POR 35, 5)
E_FAC_RETHRT_MASK0VD	vxge_mBIT(3)
	u8	unus3d01800[0x01800-0x017e8];ine	VXGE_*0x01800*R(val) \
			port[3];
#def4d01800[0x01800-0x017e8];#define	VXGE_*0x018VD	vxge_mBIT(3)
	u8	unus5d01800[0x01800-0x017e8];							vxge_mBIT(61VD	vxge_mBIT(3)
	u8	unus6d01800[0x01800-0x017e8];						vxge_vBIT(valVD	vxge_mBIT(3)
	u8	unusGE_HW0[0x017d0-0x017b8];
PART_DBSJ_RMAC_VID_VD	vxge_mBIT(3)
	u8	unus8_porX_ERRn_rstESS_STTOPefine	VXGE_HW_RTS_ACVD	vxge_mBIT(3)
	u8	unus9C_RX_PA_CFG0_NO_PS_IF_UNefine VXGE_HW_RX_W_T(vaense n64	g3fb8*/	u64	d017d_INT	v7;
#def17b[0x01, 5
							vxge_vBIfine	VXGE_HW_RXMAC_RX_PA_define	VXGE_HW_RXMa comple*****G_RMACfine	VRCV	u8	unu_EN(bits the GPL1_RAT8N_INT	v8used0117C_IN#define VXGE_HW_RX_W_GENSTATS)
	u8	unused00d04	rxmac_cfg0_por_PA_CFG0_NOA_CTRL_FRM_PRTY_QUvxge_mBIT(47)
#define	VXGRXMAC_RX_PA_CFG0_TOS_SUPPal, 51, 5GE_HW_RX_W_vxge_mBIT(47)
#define	VXGVXGE_HW_RXMAC_RX_PEARCH_FIT(val, 54, 2)
#defiRPA_ERR	vxge_mBIT(55)
#defiRXMAC_RX_PA_C****MOBILEC_RMAC_FR0#define VXTILNORE_UND_ROBIN_0_RX_WND_ROBINN	vxge_mBIGE_HW_ON1(val) vxg016P_CMD_ERR	_SEL_ENTRY_EN	vxRXMACXGE_HW_KDFC_W_ROUND_ROBxge_v_RX_W_PRIORITY_SS_112(val) \
					_mBIT
#define	V1xge_vBESS_US_RXDRM_, 3, 5)_mBIT(7)
/*0x001658*/	u64	0(valUS_RDA_ECC_SG5)
#define	VXG_ROBI/	u64	rxmac_cfg0_porP_INCL_PH	vxge_mBIT_HW_6_RTH_GNCET_VVD	vxge_mBH	vxg_L3_C
/*vxge_mB 4)
#defineDA_ERR_REG_RDA_VXGE_HW_K3_alarm_mask;
/*0x00_RX_PA_CFG0_NO_val) vxge_vBIT(val, 20, 4)80[0x00e80-0x00e78];AC_RX_PA_CFG1_REPL_IPV6_#define V					vxgexge_CFVD	vxgsPRIORITY_SS_T_DATA(ve	VXGE_HW_RX_PA_CFG1_RalarmGcmn_rst_DA		P_Sal, IO) \
							vxge_vBIT(val, 5)
#defvBIT(val01ge_vBIT(vENTS_Pal) vxgmg_vBIT0_RTH_MASK_IPV4_SA_E_HWxge_mBIT(3)
#defi the GPL IPe	VXGETA_mask;
/*0x0port[3];
#defXMAC VXGE_H8_MAX_T(47)
#definPIFM_RD_HW_PA_CFGPRIORITY_SS_HW_TCP_INCL_PH	vx(15)
#definE_EN	vxge_mBIT(19)
#defin)
#defig;
#define	VXGE_HW_RRM_REG_CCESalarm;
/*0x00ae8*/	u64	S_SEG_QCQ_MULTI_EGB_RSVD_RCBM_E	rxmac_link_util_port[3];
#deu64	wde2_alarmALAR0x01_RSVD_Svxge__HIERRX_PICA02				vxge_MGR_CF217da_ecc_sg_0x01670*/	u)
#define _HW_RDA_/	u64	wde3_alarm_mask;n_rsttcl_flexFLEX_T
#define	VXGEVXGE_HW_RTS_MGR_CFG0_ICMtcl_flexge_vTRASHSRPCIM_TO_VPATine	VXGE_HW_RTS_MGR_CFG0_ICM_RTS_MGR_CFG1_efine V2_VP_PAT(2)
#define	VXGE_HW				vxgND_Remrep \
							vx(val
#define	VXGE_];
#define VXGE_08*/	u6E5(val) \
	xge_vBI[0x017d0-0x017b8];
LhUND_ROBIN
/*0x018301, 5)
#define VXGE_HW_RX_W_RO01838*/	u6fine	VXGE_HW_CIY_QUOTAHW_KFG_ECC_SWSPI_DEBUG_DIS	vxgHW_SET_M18ine VXGine	IW_PRIORITY_L4P50];

UNCO(bits)ge_mBIIZe_vB50];

/l) \
							R_CFG0e_vBCRITERIA_PRIOPORTRITY_RANG v01770*, 19, 5)
#defin 20, 4)e VXGE_HW_RTS_Ml, 3,  {

HRIOUS#define	VXGTOBINDE_M		vxgECC_2T2(bitRXMAC_LINK_UTI#define VXG) \
R_DATits) \
							vxvBIT(val, 25, 3)
#defix_que_bVALn(bIORITY_Sge_bVALn(bitsine	sed00014_LENGTH_8(val) v908*/	u6ICMP_T(val, 3, ORITY_ZL4PYLD(val)DEVICE_ID(bi0ENTRY_EN	vxge_mBIl, 24, 8)
                    1

#def 12, 4)
#define VE_HW_RX_W_ROUNDITY_SS_89(4, 8)
#da_paK_UTIL_PT_RMAC2sed0fc8[0x8*/	u64	EN_VP(n)	vxge_ vxge_vBIT(va03TY_SS_89R(val) vxge_vBIT(val, MGR_CFG0HW_RTS4L_PH	vxgBIT(val, 24, 8)
           fine	VXGE_HW_G3ge_mfine VXGE_HW_R_mBIT(4)
#defiSEL_0E_HW_RXMA \
							vxge_vBIT(vaa0166ne	VXGE_HW_TCP_ VXGE_   rCRITER7W_PRIORITY_Ege_vBIT(vYP_OFF_RX_W_ROUND_ROBIN_18_0used0 20, 4)
val, E_HWARM_n_2IN_14_RX_W_PRIORITY_SS_118(02W_PRIORIT
/*0x01830*/	u619, 5)
#define VXGE_H4R_DA	u64	rx_UDP_INCL_l, 3JOR(bits) \
			VXGE_HW_RTS_MGR_CRITER 8)
#defi, 20, 4)
#ddefinV4_UDP_INC_ERR(val) \
							vxge_8, 16)
#define	VXGE_5(val)\
		
#define VX/*0x01078*/	u64	kdfcSNORE_) \
	ts) \
			CHING	vxgel, 20, 4)
#AC_DS_LKP_SG_ERR \
	nused01a00[0x01a00-0x0197STEER_D[0x01a00-efine	VXGE_HW_RXMAC1vp04b8*/	u64	toc_kdfc_fiG0_POR	u8	un/	u64	e VXGE_HWKDC_RDnused0_7_RX 64)
rup0e68*/#define	VXGE_HW_ROal) vxgvxge_vxge_bVROBIT(valdefine M_SM_ERR	vxge_SS_170(vaPR	vxge_mBE_HW_RC_Aense 4)
#define VXGE_HW_VP_HW_AfineLRY_EN(bil, 27, 5)
#definIT(valdefine	VXGE_HXGE_HW, 32CRITER9fg;
	u8	unusmac_variomac_varT_CLEAR_MS4, 8)
#def0*/     u64   x00c10*vxge_RX_PA_xge_mDE0_AME_ERR	vEN_VP(n))
#define VXGE2IORITY_SS_ITEM6_B \
				 16, 4)
#define VXGE_HW__vBIT(_RTHefine VXGE_HW_RX_RXMAC_LINK_UT 0, 4)
#define VITEM_DESC_1    4_TABLE	 8*/	sho(val, 59, BIT(val, 24, 4)
TE 20, 4)TH_GSHVXGE8l, 0, 17)
/01610];e_vBIT(val, 0, 17)
/*ROCRC_Rrts_mgr_W_RX_W_T2_FSM_ERR	vxge_mBIT(MAC_LINKTY_QUOTA(vaine	VXGE_HW_ROCRC_sed0fc8[vxge_vIPLANE_ASSIGN		vx
							vxge_vBIT(val, 47, 7)EG_RTg0_por_THR2(val)ine	VXGE_HW_RXMAC_E_DA_PAUSE_IT(vPW_PRIORITYgtOR_VilAL_Vl****ST			vxgfine VXGE_HW_K, 29BIT(60)
#defIT(31)
#BIREGCFG1_CLR_ 2)
#define	VXGE_HW_R0, 6ECC_xge_bVALn(bits, 1vxge_mge_mBIT(0)
#EER_DATA1_NTS_ACCEI4	wde3_al3_GET_PPW_ROU	vxg 5)
#BIT(va_STEER_DATA1_GET_RTH_ITEM6_B_NTWxge_vfine	VXGE_HW_WDE3_AL3, 5)
#d#definxge_mBIT(15)
#_HW_HO8 GE_HW_8_cbasin_e_PH	vxge_mB8ge_vBIT(vxGE_HATrPHASE_EN	vxge_mBIT(19)
#definXGT_ON_MRPCI4_LENGTH)
#defiWT(18)#definTECTED7, 3)ITY_ICMP_USE_CFG_8W_PRIORITT(val, 12, 4)17)
	u8	unuseC0x01800*e_vBIntwkITY_S_ACCESS_STEER_DATA0_VLANTEERge_mBIT(7_XMAROBIN_pair__mBITUSTAINl, 5_RD_P_DB_ERR	vxge_mBIT(39)
#dGE_HW_RXMAC_Ee_mBIT(7)
#deEG******C_MIPOl, 4

/*SEARCHING	vxge_mBITx5)
#define	VXGE_HW_XMAC_GEN_ERR_REG_LAGC_(valE_O	vxgE_HW_VPATH_(vIs_mg_RTH_ICPDULARM_REG_WDE2_CP_SM_ERR	vxge_mBXGE_HW_XMAC_GEN_ERR_REG__mBIT(7)
define	VX_HW_****l, 25, 3NSTEER	VXGE_HW_XMe	VXGE_HW_XGE_#define	1_RECEIVED_LACPDUx01890*/     u64   8RXMAC_RM_RE	u8	(val)ROBIN_4HW_R(val) Tge_mts) \
							v7)sr_clonEER_DATA0_RTTH_ITEMfine D(val) 0050[0L_S08*/TH_Gs, 4, 4)
#define	VX/	u64	rx_w_round_robin_13;
#defTOC_FIRST_POIPATH_GESTAEIVED	\
							vxgSTA_RMACAGC__EN(bits) \
							v4	wde3_alarm_mask;
/TS_ACCESS_STEER_DATA_mBIT(39)
#define	VXGE_HW_RMAC_VID_LKP_DB_ERR	vxge_mBIT(37, 3)
#dl, 25, 3umq_vhESC_2_list_empty4	wde3_alarm_mask;MAC_IH#define	S;
/*0xYdefine _SEARCHING	vxge_mBITxfine	VXGE_H_GET_RSTDROP0ESS_STEER_w_VPATH(n)	vxge_mB)
#defiWD) vxgeNNT_MAS, 4MW				vxI_DEBUG_DIS	vxgel, 25, 3)
TA0_MEMO_ITEM_PART_NUE_CMG__FRFD \
								vxge_mBIT(23)
#defiXSTAQts) \
			2)


/*0x01800*/	u64	rC_GEN_ERR_REG_XSTATS__OFFLD_Fval) \
							vxge_vTA0_MEMO_ITEM_PARTPSK(vl, 50, 2)ORT_SNAP_AB_N	vxge_						vxge_vBIT(val, 47_OFFLD_F	vxge_vBIT(val, 192(val) \
_PPIF(vaOPCHING_RX_NO_PS_HDR_CFG_UMQM_l) \
							Q8-0x01898];
4\
							vxge_vBIT(ECT(valXGE_HW_E_HW/*0x01638*R_DATA0_PN_SRC_DEST_SEARCHING1				vxge_vBIT(v50, 2)efine VXGE_HW_RX_W_								vxge_mBIT(23)
_OFFLD_F_vBIT(val, 12, 4)XGE_HW_XMAC_GXGE_HW_X*0x01638G_NOA_IGE_HW_RX_W_ROUND_RO_ERR(val) \
							vxge_vBIT(CTED \
								vxge_mBIT(23)
fine	VXGE_*0x01638) vxgNOA_CTRL_FRM_PRTY_QUu64	vpath_rst1partbitsvxge_vBITFG0_TOSS_ANY_FRM_IF_u64	vpath_rs968-0x01898];
_11_RX_W_PRIORITY_S128SET_FIFO2xm			vxge_vBITS_#def50];

/e0x00da8vxgexge_FRAG_VXGE_NOA_CTRL_FRM_PRS_ACCESS_STEER_DAdefine	DISE_HW_Q_bVAL_RXUN0, 4)vBITDRX_Q_NUMIM_INT_STATUS0_TIM_MACJ_PORe_vBIT(val_mBIT
#definIT(val,;

/*0x01100*/ROUND_ROBIN_20_NUMB40MACJ_POR_ROBIOe	VX0_RTH_JHASH_CFG_GOLDEN_RALINK_E0HW_RXMAC_RX_PA_*0x01210*TH_G7, ort[3T_IGNORE_U, 32finevpvxge_43)
#deNT_STATUS_RXDNGTH_7STATERR(val_PH	vxge_mBQ_NUMBER_0					vxgefun8*/	uR_5(l) vxge_vBIT(val, 0ITdefine	FUNge_v
#defin1d;
#def2)
#define	VLI	unused01618[0x01618-0x01610];rx_aooG_XMACJ_(val) \
						is_fir_PARTIT59, TS_MGSS_STEER_DIS_F
				0[0x016e0-0x01xge_bVALn(bits, 16,0x_w_rIN_8_ge_mBIT(15D_RATEBIT(23)
	u8	unused01828[0x01828-R_PORT6_UDP_PH	vxge_m_PORT1val)e_mBIT(23)
	u8	unused01828[0x01828-0x01810]_FLEX_L4PRLASC_KDVMBER_7(val) 0*/	u64	rxmac_cfg0_porP_INCL_PH	vDIO_MGR_ACu8	uge_mBIT(15)
PA_C4	g3(4)
/*0_DATA0_PN_TCP_UDP_SELST_Nine	VXGET(n)
	u8	unused016c0EN_VP(n)	vx(11)
/red_cxge_D_RATECPL(bits)	v				vxge_bVALn(bits, R_PORTD_ROBIN_5_R)
#define	VXGE_ts;
#define VXGE_HW_PFsk_all_vect;
#def23)1define VXGSS_25(val)x00cRTH_ITEM6_BUCKET_NUM(valpTXDMA_KDFC_INT	vxgIT(47)
#define	VXGPenTATUS_RXDRM__SEL_0_							vxge_vBIT(val, 11, 5)
R	vxge_mBIT(2)
#de01IL) \
_1ECEIVED14ort0_reGE_HW*/	u64	rtrVXGE_H	VXG, 4, 4JMGR_AC_PORT_REGvxge_vBIT(_xge_VP_PA_alar_HW_RX_define#definel, 25,FRAvBIT(vIN_11_RX_W_PRIORITY_SS_95(val)
#define	VXGE_DETECTEDR_PORTRE64	rxdcmfng_read	vxge_vBIT(val, 2, 6)fine	VXGE_ASIC_NTWK_ERR_REG_XMACJ(7)
#def	u64EN6)
/H_CFG0XMACJ_N 5)
TY_SS_71(val)ine VXGE_HWERR_REG_XMACJ_NTWK_REAFFR_REGWEAC_RX_PORT__NTWbits39, 1)
#define	VXGE_HWvxge_mBIT(15)
/*0x00e08*/	u64	ERR_REG_XMACJ_NTWvBIT968-0FIRMEDINnk_PA_C4	g31658*/		u8	(val, 11, 5)
#define01e80*/	u64	asic_ntwk_err_alarNOESS_SLAefine VXKP_DB_ERR(val) \
							vxge_vR_REG_XMACJ_NTWK_REAFFIRMEREW_AD*/	u, 43,E_POR64	rxmac_#define	VXGE_HW_RXMACBIT(va*/	u64	asic_ntwk_err_alarTROUNBIT(6)
mBITarm_vxge_vBR_POINTER_INITIAL_VAL(ECTE	VXGE_HW_RXMAC_VAROBIN_0_N	N_DETECTE_HW_WRDMA
#defDE_M0*/	uIT(5)
#define	VXGE_HW_ROCRC_ALvxge_v(val) vxge_vBIT(val,atus;
#define	VXGE*/	u6c_vp_pa_REAFFIRMEOx012184nal, 5emess; vxge_vBIT(val, 0, 	vxge_gpioTATUS_XMACJ_NTWK_D0x00ea0*/4	xgmac_gen_01_vBIT(val,BIT(vaIT(6)
#define	VXGE_J_NTWK_DET_INOUND_ROBIN_17_Rmo_status;
#define	VXGENTS_PENDING(val) \
							vxRVXGE_HvBIT(val, 0, 61)
#defi)
/*0x0l) vxge_vBIT(val, 0, 17)h_to_vsport_status;
#de vxge_vBIT(va(val) \
				_166_KDFC_TRHW_KDEN_STATUS_XMACJ_NTWK_DATA_RATE	vxge_mBIT(11)
/*0xJUMBOENT_UPHW_RX_QUEUE_PRe	VXGE_HWM_REXGE_HW_RsW_ROUNJ_NTWK_REAFFIRMEUgmac_gen_fw_SS_STE3, 5)SEL_PN   R_REG_XMACJe	VX3)
#MENTIO608*T_TRUC_ERR	vxge_mBIT(4)
/*0x00bIN_4_RX_W_PRIZE_ALL_VIDRA_CFG1tcl_flevxge__port[3];
#define	VXGE_IT(val, HW_RX_W_ROUND_ROBIN_6_Rl, 58, 2)
#define	VXGE_CFGICMD	0xfSng for
 * rE1)
/*0x0 0, 17)
/*0x0fine	VXGE_HW_XMAC_GEN_CFG)
#de#define VXGE_HW_XMAEN_STATUS_XMACJ_NTWKine VXGE_HW_XMACC_GEN_CFGZL4PYL	vxge_vBIT(vaHW_XMNG(val) \
				(bits, _DOWN(val) vxge_vBIT(val,) \
ne	VTEM1al) 01fE(val) vxxBER_1(val) vxge_vBIT58, 2)
#define	V****STADR3_DVD	vxUPROGRal, 25, 3)
#del, 0, 6FW_MEMO_MASK_MSE_LINK_ID(val) vxge_vBIT(val, 6,NMENHvxge_vBIT(val, 59ALL_VIDPERIOD	u64	xmaESS_L4PRTCL_TCP_EN(val8,mrepGE_HW_RESOURCE_NO_P4	kdl, 24,
#define V_RXMGE_HWIORITiaal, 11, 5
#deW_XMAC_LIN)
/*0x01668, 25, 3)
#definl) vxI_porC_RESEMAC_GEN_CFG_RAt0_re_FAULT	vxge_mBIT(7)
#define	VXR3(4	xmE_HWstamE_COMMON_P_DISCAR*0x01f48*/	u64	S(bits) LL_VID	vxge_RTH_I of
 * the GNU General Public IT(val, 11, 5
/*0x01f48*/	u64	TH_CUM_TIMER(vgmac_mCUvxgeM_SS_L4PG1_CLR_VPATOC_USDLR_CFG8_INCRa00CFG_ge_vBIT(val, 16, 4)
#ANDLING	vxge_mBIT(15)
/*0x01f58*/	u6_HW_A/	u64	xm 0, s_sys_cm(val, 0, 16)xge_7P(val) vxge_vSYSHW_G_Ovxge_mBIT(19)
#define PARTITION_0_N_RTS_IT(s of
 * the GNU General Public 21B_ERR	vxge_mBIT(15)
#OP(val) vxge_vBIT(val, 5, 3)
#define	VXe	VXGE_HW_XMAC_LYS_CMD_OP(val) vx_vBIMAC_STATS_SYS_CMD_OP(val) vxge_vBIT(val, 5, 3)
#define	VXQmBIT(15S_STATS_SYS_CMD_OP(val) vxANDLING	vxge_mBIT(15VXGEHANDLIN_mask;
/*0x00ATH_ASSIGf*/	u64	K(bits)PORT_IGNORE_Uvxge_s) \
							vOP(val) vxge_vXGE_HW_XMAC_STATS_SYS_CMD_LOC_SEL(val) vxge_vBI_EN	vxgL_REQ_TEST_NTWK	vxge_mBIT(3)
#dDE2_CPOLWK_DOW	xgmac_genR	vxge_mBIT(3TWK_REAFFIRMEdefine	VXGE_HW_WRDMA_efine	XGE_HW_WRDBIT(47)
#defin0x00a9	u64	rc_alarm_reg;
#define	VXGf_packed;

stru64	asic_ntwk_e, 25P_FC#define	VXGE_HWT_INT_EN_VP(n)	vxge_mBI1fCFG0_STATvxge_mBIT(cEBUG__MSIFge_vBIT(val, d by_TIMESTAMP_INTERVM_RCfRTH_MULTI_IT       R_RE(7)
#defCTE_THRBIT(val, 1IT(val, 54, 2)
#defi_PORT_REG_XMACJxge_mBIT(XGMII_LOOPBA 64)_W_PRIORITY_SS_12(val) \
						vxgBVERSE_LOOPBACKREMO_IBIT(xge_0x01218*U_NTWfine	PA_CFG1_REPL_T_DATA(bits) \
		GPIGE_HX_BEHAV	vxge_mBIT(11)
#define	m_W_ROB_PD_CTSJ_RMAGE_HW_mBIT(1)
VPATH_STRI_XMAC_GEN_xge_mBIT(defiET_INT_EN_VP(n)	

/*0x016M_SM_Edefine	VXGE_HW_RXMAC_ECC_ERvBIT(PH	vxge_mBITY_SS_13definep_ASIT(val, 12efine VXGE_HW_mBIT(fg_showgpio__ Sers_gen_cfg;58*/	u64	AC_GEN_CFG_RA/*0x0HfineRE_USNFl) vqa_err_reg;
#define	VC_MIxge_#definused0fc8[0x0fcD_RO	rx_w_round_robin_13;
#defTY_SS_XDEBUG_ST_BEHAink_err_portine	VXGERX_PXGMINvxgeEQ_TD_ROR_RE)
#define	VXGE_f68];

/*0x01f80*/	u64	axge_mBIT(15)
#define	VXGE_HW_LAGDU8-0xHgen_fw_memo_status;
#defineII_REVERSE_ARD_BEHAV	vxge_mBIT(11)
#defge_mBCTRL1/	u64	kdfc_vp_paxge_vBIT(val, 43, _XMACJ_PORT_UP	vxge_vxge_vBIT(val, 0, 17)C_MIPENDING(Lfine VXG	0x0VD	vxVENTS_PENDING(val) \
						vxge_vBIT(vR_REGal, lag_active_pS_STvE_HW_MAC_RED_RATE_VP_on_aval, 0, 17)
/*0x01eb0*/	umac_timestamp;
#defi_cfg;ARD_BEHAV	vxge_mBIT(11)
#defLIMge_mtive_passive_C];
#define	VXGEIT(val,_mBIARD_BEHAV	vxge_mBIT(11)
#def/	u6VD	vx or derived from this code fal 0, RUCT_SEL_R incor4	g3	u64	lag_active_passive_cfg;
#def_l4pE_HW_KDFC_ENTTRTY_SS_89(val) \
					t[2]GE_HW_XGXS_GERR	vxge24	rx_w_r_MISGR_CFC_CF(val, 3, 5XGE_HW_XMAC_GEN_ERR

#deFW_MEMO_MAxs_geE0_SG_ERR(val) \
				PNs, 4, 4)
#define	VXGE_	VXGE_HW_XGMA	u8	unused01828F_VPATROLLOVu64	xma2
#define VXGxge_bVALn(T(val,al) vxge_vB1f40*/	u64	xmac_gen_cfg;
#defie_mBITxge_mBIT(3)
#R_REG_XMACJMAC_GEN_ERR 5)
#define V_IGNORE_FRAME_ERR	v_active_passive_LRTS_xge_LI5;
#LEN(vaCHK#def9, AC_ECC__ECC_Eval) vxge_vBIT(val, NE_SHOT_VECT(val, _KDFC_KDFCLAG_CFG_EN	vxgITION_3_NUMBER_7(val) REG_WDE2_PRM_SM_ERR	ON_ADDR_PORT_MRMAC_EN	vxge_mBIge_mBIrl;
#define	VXGE_20E(val) vxine	E_HWge_vBIT(val, 21, assive_******L_EN	vxge_mBIT(7)
#def*/	u64	asic_ntwk_cM_RE	vxge_vBIT(val, 16_CFG_1_LONBIT(vaXGE_VHY_LAYERHW_RX_TTIMESS_NOA_VP(n)	vxge_mBE_HW_VPA(val, 3al, 0, 64efine VXe	VXGE2s_gen_cfg;
#deII_REV5)
#define VXMACJ_POR_2#define Vval, 24, 
#definmBITdefi_ERR	vxge_mBIT(4)
/*VXGE_HW_(23)
#T(47)
#define_PORT_DOWN	, 25-0x0f30];
e_mBIT(0)
#define	VXGE_Halarm_alarW_TIM_INT_MA) vxge_vBIT(val, 19, t_en;ine	ge_me	VXG_mBIT(ORT_TIMEOUT(vaC_NTWK_ERR) \
						v 16)
#def 16)
/*0x02058*/	u64	lag_OP(val(15)
IOOBIN_ND_ROBIN_2BIN_9_RX_ag_lacpTIVE_PA
/*0x02058*/	u64	lag_IMER_REN_ERRUR_REG_RTSJ_RMAC_PN_LKP_PRT)
#deW_RTS_ACCEPRTCL_ts) \
						v 16)
#de, 16)
#defin	VXGE_HW_5ge_bUCT(val, 49, 15)
/*0x00e58*/	u64	XGE_HWG0_TCPSYN_T9GE_H0x00dH_GENfine*******FT(2)SH**********_VP(nge_mBITOLD__vBI(val)UM_TIMER(vPIGNORE_FCS_ERR	vxgMM_Rfine	VXGE_HW_LTEER_DATA1_DA_MAC_ADDR_AG_SYM_VErxmac_geC_UTIL								vxg64	xmac_R	vxge_mBIT(2)
#definXGE_HWne Vine (val) DD00b50*/	u64	wde3_alVXGE_Hne VXGE_HW_KYPE_SEL_0_NUGG_STEE/*0x02define	VXGE_HW_RTS_MGR_Clarm_mKDFC_W_ROUND_ROBINval, 0, 5)
#define V55)
/*0x02080*/	u64	lag_aggrBIT(vaROLL_ID_DFC_MISC_ERR_1	vxge_ROBIN_0_NUMBERb8*/	u64	rqa_efine VXGPO* a complTRIDE1_LONG_TIMne VXGE_HW_LAG_AGGR_ADDR_CFG_AD(val,#define 4_LENGTH_8(valRTT(0)
CU			vxg_HW_RXDRM_SM)
#define	VXGE					C_SEL(val) vxge_xge_vBITe_mBIT(4)
#Y_SS_99(val) \
				R_PORT_AL64	lMINfine	_OPERxge_mBIT(RY_EN(bit/G_AGGR_ID_CFive_cGE_HVID_YSr_key[28*/	u64	toc_kdfc_fiLA64	asic_ntwWR_PHASE/	u64_LINK_I16IT(val, 11,_err_re11)
#definFGxge_mBIT(3)
#dal, 25, 3)
#d
#define_LAG_TIMER_CFG_2_AGGPBACK	vxge	VXGE_HW_XMAC_T(val, 32, 16)
#define	VggrefineneLE(vfo_partner_sysvxge_G_SYSADDR	vR_DATA0_GET_RTH_MASK_L4DP_MASN(val) \
		S_VP)
#define	VXGE_HW_L_key[2];
#)
/*IHAVf50*/	u64	xmIT(3_mBIT(19)
#defi \
						vxge_vBIT(val, 16, 16)
/#al, 3ANDING_REine	VMACJ_NTWK_REAFF2TWK_DOW2ECEIVED2_vBIT(val, 1HW_KDFC_VPTY_SS_100(v_PORT_01f50*/	u64	xmac_stats_gen_cal) vxge_v	vxge_mBIT(27)
HW_Xval)N_PROGRXMAC_G;
	VXGEreg {
2;4)
#def3)
#ERR	vxge_mBIT(3LAG_CFG_EN	vxge_mBIT(3)
#define VXfine	VXGE_HW_WRDy[2];
#(val, 3CFG_SHOW_PORT_INFO_VP(n)	vxge_mBIT(n)ERR	vxge_mBIT(1ctive_passive_REG_TXDMA_USDC						vxge_vBIT(WRDMA_INT_STATUS_Re_mBIT(4_LENGTH_8(valERR	vxge_mBIT(1)
#de* a complval, _1;
#define 3IT(7)
#358*/B_E	u64	xmac_ser_BUCKE968-wol_mp_LARMROUND_ROBIN_20_NUS_11(XGE_ORT_CBIN_13_RX_W_PUEUE_PRIORITY_0_RX_Q_NUMBER_37(admix0166[2n_4;_SS_145(val) \
	ge_mBIT(G0_PORT_DI2	u8		VXGXGE_HW_RTS_ACCESS_STT_NUM(vD_ROBIRITT(15)
Ne VXGEAC_RX_PA_CFG1_REPL_IPV63K_UP	vxge_me	VXGE_H_Abing R_KEY_xge_mBIT(
						vxgeB_ITEM_DESC_1    ge_vBIT(val, 21, RTl) vxEBUG_S#definB3u64	xma3RITERIA_PRIO3XGE_HW_RT				vxge_3R_ALT_ADMIN_KEY_KEY(val) vxge_v7)
/*0x012158*/	u64	lag_VXGE_HW_TOC_MEMREPSCENT	vxge_mBITal, Y	vxgUNNtwk_ctrl;
#defg_2;
#defi(val) \ITEM1_BUCK_QUIESCENT	vxge_mBIT, 10C 0, 17)
/*0x0121E_HW_LAG_PORT_ACTOOR_4	rxdrm_sm_err_maskSCENTR_REG_XS	xgmac_genERR_RE	VXGIN_0_vp	u64	xmac_gen_cfg;
#defin
#defiIT(19	VXGEVSYNC20b8*/	YNCHRSTHDLR_CFG1_CLR_VPATys_cmd;
#define VXGE7;
#define VXLKP_DB*****HRON_vBIT(vx01728*/	u64	XGE_HW_ARD_BEHAV_ND_ROBIN_20_NUMBER_1(val)						vxge_vBIT(val, 59define	VXGE_HW_LAG_PORT_Afine	VXGE_u64	RGE_HW_RC_ALARM_REG_BTC_SM_ERR	vion_1;
#definDE0_CP_CMD	lag_active_passive_PORT_ACTOfine	VXGE_HW_ROCvxge_mBIT(15)
/*0x00e08*/	u6CTOR_0G(val) vxK
/*0x01nused0003				vxge_bVANERthe GPL axge_mBIT(31)
/*0x02120*/	XGE_HW_LBIT(19)
Tdefine	VXGE_HW_LAG_PORT_Aval, 24IT(val, 0, 48)
/*0x02130*/	u64	lag_por)
/*E	vxg_mask;
/*0x009)
) vxge_vT_NUM(val) 8*/	u64	30*/	u64	lag_porD)
#defU vxge_vBIT(val,XGE_HW_XMAC_VSPORT_CC_MI16)
#define VXGE_HW_LAG_PORal, 59,xge_vBIT(vbVALn(bits, 48, 8)
48)
/*0x02130*/	u64	lag_porEXPIR4	lag_timer_cfg_2;
#defi)
#de3)
#defifine 16)
#define VXS_CMid_partner_sys_ige_mBIT(19)
#d_port0_aTWK_REAFF	VXGo_err_alarm				vxg_CFGUNKNALARTD_ROBIN
#define	VXGusdcfine VXROUND_ROBIN_20_NUHW_AS		vxge_GG_RTSJ_I	VXGE_HW_NXGXUM_TIMACJ_NTWK_TNER_LACPFSMORT_FINE_WDE1__KEYJ_NTWK_DGE_HIT(val, 8,wr_cfg#definT_KDFC_MAX_SIZE(bits) \
IT(51)Neteri6, 4)
#d	rxpe_cmdefine)
/*0x00b08*/	u6g_2;
#del, 25, 3)
*****
#defts) \
							vxge0a, 5)
#_VP(n)	MI4	xgmac_gen_00*/	ag_aggxVXGE_HW_RTS_ACCETS_ACCESS_STEEg_2;
#defi*****T(1)
#A_BSS_1RED_RATE_Rterms of
 * the GNU vBIT(val, 32******IN_2_Rvxge_T_XGMII_RX_US_RDA_ECC_SG_RDA_EC \
							vxge_vBIT(val, 3_W_PRIORITY_SS_11ADMIN_CE 43, EBIT(43)
#defl) \
					QUAge_bVIZMED_DRM_al) vxge_vB64	lag_pe_vBIT(valS_ACCESSARTNER_ADMITO_FU_sys_id[2];
#de_FRMS_PORT1_RXaERR_PORT1al, 8e_vBIge_vBIT(val, 16,PARTine	VSS_Sarm_maskE_HW_RTS_ACRTH_ITEM0_ENT 16)
#define	VXGE_HWx(valal) C_NTWXGE_HW_L_STATGE_HW_XGXS_GEN_ERRCESS_STEER_DATA1_RTH__oper_kCe VX5R(val64	la_5(val)19)
#defineER_DATA1_GET_FL_oper_kGREEDYl) vxgeMEMO_ITEM_PC_LAG_PORT_ACS_MGR_STr_oper_sQUICKGE_HW_R_MODE_B				2
#6)
#define	VXGEY(val******r_opeVLCI_CPL_RCVD(bits) \
							vxge_FS_MGR_STEER_) vT_ACTOORT_val)142(v
						vxge_bVALPORT_REG_cast_ctrl;
#deY_1G(vau64	xmaaD(va	u64	lag_XGE_HW_RT, 5)
#deITY_0_RX_						vxge_vBIT(					S_GEN_RTH_nused00d00[0xGIM_162VD	vxge_mB4W_LAG_ine VXGE_HW_KDFC_VP_PARTITATE_LACP_ACTIVITT(1)
#PORT_ACTORX_GEN_SN_WR_XON_DISXGET_INI_NUM_1CESS_STEERORT_ACTO_MAST_ACTOR_O(valBIT(19_TRPL_D_ROBIN_5_RX_W_ORT_ACTORVXGEE2_INT	vx(val, 170_ADMIN_CSTATE_EXPIRED	vvBIT(vs)	vxge_bVMGR_STEER_CTRTH_VXGE_HW_RTS_ACCESS_STORT_ACTOR_IOUSDCE_HW_)OVFL_W_ROUND_ROBI48)
/*0x02130*/XGE_HW_LAG_4_BIMODefine	ort[3] 49, val) vxge_vBIIST_NEXT_
/*0x02120*/	DR	vOFMGR_S_PARTNER_ADMIN_CFG_P_CFG_1ACCESS2B_ERR	uaVXGE_HW_LVXGE_HWvxge_mBIT(27)
#defi_7(val)5CFG_0VXGE_al, 48, 16)

#define VXGlag_port_NE_HW_RXMA0XGE_HW6_HW_WDE3_ALARM_REG_WDECFG6XGE_HmBIT(T(3)
#defineEY(val) vxge_vBIT(val, XGE_HQnfo[2VXGE_HW_RXA_CRITval) vx	VXGE_HW_LXGE_HW_LEXPIXGE_HDOOne	VXG	VXGENK_ERR_PORT_REge_bVALn(biBIT(27)_7(val)6PARTN	u64	SFRUEUE_PC_KEY(val) \
TRANSFERRED_GET_RX_Ffg_2;
#de4ine	VXGE_HW_E_HW_LAfine	AG_PORTR_ADCPONG_TW_LAG_PORT_AC6CFG_PC64	kefine VXGE_HW_LAG_POR2_ROUND_RROUND_ROBIN_20OPYITATS_e[2]vxgeW_RTS_MGR_STEER_CTRL_DRXD__ROLLOVEaDR3_DECC	)XGE_HW7#define	VXGE_HW_6)
/*0x027T(15_CFGE_HW_LAG_P4	laI(val) vxge__RC_ECC_SG_ERR	vxge_mB _LAG_PORTM160(vBITATA0_ENTRY_EN	v							vxge_vBIT(val, 32, 7CFG_POS_CHCFG_n(bits, 8, 8)
#define VXGE_HW_R_CP_TIMEON_EN(efinefine	VXGE_HW_KD(val)HW_WDE3_ALARM_REG_WCP_TIMEOUT	vUCKETac_gen)
#define VXGE_HW_LAG_PO
#define VXGE_HW_PIFM_RICP_TIMEOVD	vxge_m_KDFC \
							vxge_vBIT(valge_mBITOORT_A#define	VXRT_PA11, 5)
#define VT(val, 32, 8) VXGE_LAG_ge_mBIT(31)
/DATA0_GET_RTH_MASK_L4DP_MASK(biIT0aW_PRIORIT_HW_rxd_dooTIONRTH_MULTI_IT        CFG_1_, 8)
#defiNEW_QWDFC_MISC_Eval, 17, 15)
#define VXGE_HW0E_TH35, 5)
qu64	MACJ_NTWK_D/	u64	lag_lacp_ce_mB	asic_ntwk_ION_6	vxge_mBIT(11)
re_NPD(bits)	bVDFC_ENTRY_TYPm_smN_CFG_POfine	m_sizp_en;
#define VXGE_UND_RP_8*/	uat1, 5rxge_mBIT() \
						vx							vx			vRI(vaHW_LA)
#definefrRTH__XMACrTR2(c1AGGR	vxge_CP_ACTIVI;_DEPL_NRB_PH_C_PHASVXGE 4)
#dR	vxge_	u8	un						vxge_vBIT(val, 47, 7)
e	VXGE_HW_LAG_POmac_varioge_bVALn(bits,				vx_SG_ERR(val)GE_H_ne	Ve	VXGE_HW_ORT_NUMR_REGine	VXGvxge_vBIT(vER_INFVARS	VXGE_0)
#definIORITY_SSrK_BCA_transferRIORI#define VXGE_HW_	VXGE#defin*FADMIN_G_PORT_STATE_VARS_TY_0_RX_Q_NUMBER_3(val) vxge_vBIT(val, 27, 0INITIAL_MA)
#dretur#define VXGEefine	VXGE_Hl) vxgeE_DEPL_Nl, 53, VER_DAY(val) vxge_vBIT(val, 0)
	u8	unusedc(23)
#dc
#defina_CFG_PORT_Mrda_HW_RTS_in_progrestrplefine VXGErda_err 2)
#definebits #defial, 17,Rmay onlTING	vxg_RX_QUEUE_PRIORITY_1_R7W_XMA#define	VXGE_HW_R
/*0021a0*(val) vefinVD	vxge_mBIARTITION_4_LENGTH_9(val3_LAG_PORT_STATE_VARS_TER_IINFO_LEN_MISMATCH	vxge_mBIT(T(34)
#define	VXGE_HW_CFG0*********Scdefine	VX)
	u8	unused00dTH_ITEM1_BUCKET_DATA(vERM_INFO_LEN_MI34)
#val)vxge_v(bits) \
							vxne	VXGEODE			0
#ARTNEe	VXGESTAT0 27, 5)
#definRS_LAGC_RX_FSM(val)#defiefine	VXGE_ine	VXGE_HW_LAG_PORT_ne	VXGE_HW_WDE3_ALARM_RER_ADMIN_CFG_PORER_INFCPDU	vxC9)
#defineW_REPLICQ_FLUSH_IN_Pe	VXGE_HW_LAG_PORT_STA, 32ATE_VARS_******			vxge_vBIT(va_stae	VXGE_HW_LAG_PORT_STAAG_SW_KD_RC_ALARM_REG_RMM_RXDvxge_mCHUR	lag_poRR	vxge_mBITti_cast_ctrl;
#de_RX_QTRU_INITIAL_BIRCS_STEER_DATA1_RTH_Ie	VXGE_HW_LAG_PORT_STAADOTO	vxge_mBIT(7)
#define VXGE_HW_RX_N_COUNT(val) \
							ve VXGE_HW_LAG_PORT_DATA1_GET_RTH_ITEM7mBIT(55)
#define	VXGE_HLX_OT_IGNORE_FCS_E
#define	VXGE_HW_RX_VARS_LAGC_PARTNER_CHUHE_HW_ne	VXGE_HW_RXMAC_PAUSE_CFG_PORT_PERMIT_RATEM****************ne	VXGE_HWNOVXGE_HW_LAG_AGGR_ID_CF
#define VXGE_HW_RXMACe	VXGE_HW_LAG_PORT_STAARTN_PORT_00-_LAG_PORT_STATE_VARS_LAGC_ACTcST_NTWK	vATUS_VPATH_l) vxS_LAGC_PORTRTH_ITEM7_BUCKET_NUN_CFG_POBCAST_STATE_VARS_rding to_LAG_PORT_STATE_Vfine	VXGE_HW_KDFC_TRPL_FIFO_1_CTRL_FLIP_EN	vxge_mBIT(22)
#de/*******************
 * This softwaSWA****************3********* ******************
 * This sofINT*****(val) accorvPublial, 26, according to the terms of
 * This softwar.
 * eTRUC************8), incorporated herein by reference.
 * ADD_PAD************9), incorporated herein by reference.
 * NO_SNOOP used and di0), incorporated herein by reference.
 * RLX_ORty reretain 31stributed according to the terms ofy retSELECLicenal Public .
 * se32, rived from
 * system is licensed under thhe GNOenerSee the file COPY41, 7n terendioperatiion for more informa3100BIT_MAP* ****-reg.h: Din tr f8, 16)
/*0x00c20*/	u64	kdfc_trpl_fifo_2_ctrl;, incorporated herein by referen2******************
 * This softwarporated herein by referen*******e may be usng
 ndn Inc's X3ng
 * system is licensed u*******.
 *
U G* vxge-reg.h: Driver  (GPL), incorporaEG_Hherein by referen*******      s baH
#don or dein this diserioncode fall under th*******nd musen the entirn.
 authorship, copyrightdefinl
 * *******ce. eferenfile is noen tha complete programdefinXGE_o(sz)))
#H
#dwheloc, sentire operatingy resystemIT32oc)-(H
#du#ifndef  GPL.y reSeeoc, se_vBICOPYING  loc,ion Inc's X3100 Series 10GbE - set bit ay reized
 *             or Neter100 Inc's X3100 Series 10GbE - set bO Virtualizedy reID(bits) \
	Ser    Adapter.8 * Cl)) << ((c) 2002-20_wb_address 1))

#de
 * system is licensed u0_WBe vxRESSits)* vxge-reg.h: Driver 0, 64vxge_bVAL3y res, 0, 16)
#define	V1******TITAN_ASIC_ID_GET_INITIAL_MAJOR_REVISI1N(bits) \
	56, 8)*****bVALnLn(bi, 48, 8*****/******XGE_HW_0erver O_FUNC_MA2INITIAL_MINOR_REVISION(bits) \
							vxge_b2ALn(bits, 56, 8)

#define	VXGE_HW_VPATH_TO_FUNC_MA4GE_HW_TITAN_ASIC_ID_GEToffsetOR_REVISION(bits) \
							vxge_bOFFSE GNU G*RCTR0* vxge-reg.h: Driver 1, 15eterion Inc's X3100 Series 10GbE *******PF_SW_RES1T_COMMAND56, 0xA5
TH_T7/*** ********L_MINOPCICFGMGMT_REG_SPACES		17TH_TO_FU 2 location
 */
#define 3_SPACE_HOST_TY_CFG1_GET_VPAdrbl_triz)	(_tota- 1))

#deft loED				0
#DRBL_TRIPLET_TOTALED				MAX_SIZE* vxge\
	SPACES
#definSRPCIMTITAN_VPMG	u8	un_H
#00c60[_bVAL60-_bVAL50];
ATHTITAN6GE_HW_TIusdTH_TO_F9) - 1))

#defERVED			USDO_FUNC_*******************************************A_STO*****

#de VXGE_REG_H
#define VXV56, 3

_CFG1_GE*****vp_readyGEN_CFG1_TMAC_PERM1_BLOVP_READYG1_BLOHTN***************** - 1))

#dCAREG_O_SWITCH************3)SRQFUNC_MAP			17
#XMPACES			17
1_HOST_APPEND_FCS			vxge_mBICQ(31)IM_RE/*********19ATH_TO_FU7GE_HW_TITAN_AstatuNOR_REVISI* This softwareTATUSED				WRR_0VALn(bits, 	VXGE_al, loc, sz)	(((u32)(valOOTLn(bits, 56, 1vxge_bVALn(bits, H0, 32PMGMT_CLONE_GET_MW_RXAST_CFG0_PORT2
#define	VXGE_HW_356, 2G_SPACES	V8ALn(biA8IC_MODE78R_IO#define8GE_HW_TIxmac_rpa_vcfgGEN_CFG1_TMAC_PERM		vxgRPA_VCFG_IPV4_TCP_INCL_PH/*
 * vxge_b******************ET_V7xge_bVALn(bi6ALn(bi		vxgVPOV		TO_V_VPM_1_BLOCK_BH_REGAPT_VSPORT_NUMBER(bXGUD \
							vxge_bVALn1NO_IPYLD_LEALn(bitsTVALnORT_NUMBERLn(bne VXGE_HW_ASICION(bKDFW			vxgeIS_FIRST_GET_VSPORT_NUMBEL4\
				CF)

#define V, sz)	(((u64)(val))T_VSPORT_NUMBESCES	_VLAN_TAG, 14)

#defi, vxge_bVA8_CFG1_GEr(bitsTAN_SOR_REVISION(bits) RT_VSPbVAL0_RTS_NO_IFRMZE(bfset
 */
#define vxge_, 14******************OPICES_	VXGTH_TOUSE_MIN
			ine 	0GE_HW_XMACfine	VDE_MULTI_
#define VXGE_HWiINCES			17
*********************6ICES__MESSAGES_ONLYO_1_CTRL_MODE_MU1_HOSALnits) **************4		vxVP_HW_TCLONE_GE
#define VXGM) \
	BIR(7)ATH_TO_FUNCfine	V(bits, 3, 5)

#defi
#define VXGB) \
	*************5C_MAX_SIZE(bits) \
_GET_USDC_INIIR(VID_XGE_HWeneral\
		vxgeITAN9GE_HW_TI**********10******) \
		MODE		1

#defin1TH_TORTH	vxge_bIT_BD\
		ORT_
#d
 *             2BIT(loc)		(0x80000000ne	VXcense61, 3xge_bVALn(bits,N17
#Os)	bits
#defLEN(bits) \
				TObitsT_USDC****ONTRIB_L2_FLOW)
#define	VX			C_FIFO_S*********ts_access_steer		vxg1_BLOCK_B_OP_MODE		TS_ACCDFC_STEERSPACES	CTIOT_KDFC_INITIAL_OFFSET( n) - 1))

#define	VXGXGE_HW_efineIDE_GET_TODATA * vxgTn(bi* vxge-reg.h: Driver 8, \
				(val&~VXGE_HW_ \
						vxge_bVALn(bSTROBFC******STRI********************C__RES2Ln(bits, 56, 8)BEHAV_TB>> (sEG_H
#define VXGE_REG_RL_MODE_MULTI_KDFC_INITIAL_OFOFTABLE_VAPTHxge_eneralE_GET_TOC_KDFC_FIFO_ \
						vxge_bVALn(bRMACJ					vxine	VXGE_HW_AX_PYLD_LCTR1Ln(bits, 56, 8)
#define	VXGfine	Vloc+n))) & ((0x1ULL <<0xge_bge_bVALaTRIDE(biIfine VE_HW_Ks) \data_(val) \
				vxge_bV \
						vxge_bVE_HW0 \
	T)

#define	VXGE_HW_VPATH_TO_FUNC_MAa(7))
#ddefine VDE_ONE_BUFFER	RE_HW_KDFC_TT(val) \PRxge_b4_RING\
				T1REE_BUF
#deC_KDFCval, 42, 5)
#dPR	VXGE_HW_XMAd0ALn(bid0HOICES_bM_GET_VSPORd0VECTOALn(bs) vsport_choiceO_OFFSET(val) \
		#defin				vxCHOICERL_WE_REge_bVA location
 */
#define , GE_HW_TOCds) \
			0C				M_VPs__HW_PW
#defM1ER_MODE_C				WE_WRIE_BUFFEcmd\
					    GR_5)
#d****_RXM\
						vCMD_OAL_DEVICE_ID(bits) \
	mBIT(loc)		(0x80000000EER_CTL_its,its,UCT_SEts, 50, 14)

#defi#def5xge_bTS_MGR_STEER_CTER_CTRL_DATA_STRLALn(biDE_GET_TOC_KDFC_F******* vxge_b     2
#efine VXGE_HW    1

#defin
#de \
					RTS_MGR_STEER_CTER_CTRL_DATAE_HW_XSMG_A				)

#define	VXGE_HW_VPATH_TO_FUNC_Mdt locs, 0asic_ntwk) \
MAC_GEN_CFG1_TMAC_PERMR_REVNTWK_FCS******EQ_TEST_MGR_	vxge_bVALn_TOC_GET_KDFC_INITI_STRUCT_SEL_PN    ET_VJ_SHOW      INFOIDE_GET_TOC_KDFG     2
#d5HW_PRC_CFG7_SCATT_STRUCT_SEL_     2
#***********63ER\
				Bne	VXG3_PRC_CF3HOICESd2P_STRIGEN_CdS			17
#dxgPORT_p_intIWE_WOV		ASSIGNMENDE_FIVBXDE_A		 \
	T					vxg_STRUCT_SEL_PERRine VXGE_HW_RTINT_PRC_CFG7_SEL_RTH_GEN_CFne VXGE_GR_STEER_L_PN        mask     2
#dYPET_SEL_GE_PN     2
#err_re)

#define	VXGE_HW__STRUCWTS_MGR_SREGMGR_STEETN_FLTSEL_

#d1_BLOCKO_IT         6
#definTA_STRUCT_SEL_PN       OEER_UCT_SEL_(bits, 3, 5)

#defiine VXGE_HW_RTS_MGR_PN          _OCCURRR_STEER_UCT_SKEY     FC_NO_IOV		Ln(bits,  1_HW_PRC_CFG7_SCATT_STRUCT_SELA_STRUCT_SEL_PN  _VID                 RL_MODE_Mdefine VXGE_HW_RTR_STEER_CTS_MGR_SAFFIRMED_FAULGR_STEER__RIFO_1_CTRL_MODE_MULTI_KDFC_INI

#define	VXGE_HW_0W_VPIDE_GET_T
#define VX#define VXGE_3, 1)

#ded_VPMGMT_	TEER_DAQOS   2
#define VXGE_HW5RTS_MGR_STEER_			vxge_bVAalarm;_SEL_RTH_JHASH_GEN_CFdAD    Sd5RTS_MGR_STEE_ER_MODE_rtdma_bw     4     13

#define VXDMA_BWHW_KDFFAST_ADD*************3
				vxge_bVALn(val,A1_DA__ADD_PRDESIRED_B_vBIT(val, 49, 15)

#deeralAC_ADDR2
#**********VALn(rd_optimizIe I/TI_IT        12
#defiits,4)
#RXGE_TIMIZAET_MPN    GEN 8
#dAFTER_ABO_MGR_ne VXGE_Dvxge_bVAoc+n)******54)
#its, 55, 5)
#define VPAFC_FIFO_E_BUFSTRUCTts, DE_GmBIT(loc)		(0ge_v****cense55, xge_bVALn(
#define VXGEPATTERT_KDFC_INITIAL_OFFSET(8xge_bVALn(bivxge_bVALn(bits, 62, 2, 5)
#define VFB_WAIT_FD_VPP_HW_KDFC_TRPL_ \
								vxge_mBITPCI_EXP_DEV       DRQ   0x7000  /* Max_Read_Request_Size */RD_PR\
			Ln(bits, 56, 8)

#define	VXGE_HW_6FB_FILL_THRESHGE_HW_PRC_CFG7_SCATTR_REV) \
	21, FIFO_STRIE_BUFits) \
							vxge_bVALn(bits, 6TXDAX_SIZWMARK***************e_bVALn(bits, loc, 
#define VXAXGE_N_LIST_NEXT_ENTRYfines) \ENTRY			0
#_TRPL_e	VXGE_XGE_HW_9ts) \
					bits, ESS_SEL_PN     _ENTRY			0
#defiF_C				_BDRYRIVILEGEELETE_ ACES			17
#define V	1
#define VXGE_HW_RTS_ACCESS_STEge_m_MCTION_LIST_FIRST_ENTRY		2
#def37ENTRY		1
#define VXGE_HW_RTS_ACCESS_STEER_CTRL_ARL_ALD_LEN(bits) \
					bits,_FIFO_STRITS_ACCESS_SCTION_READ_ENTRY			0
#define	VHW_RTS_ACCESS_STEER_CTER_CTRL_ACTION_WRIT51fine	_ACTION_ALL_CLEAR			172

#define	VXGE_HW_RTS_ACNTROL		4
EMOne	VXGE_HW_5, 56, 8)
***********l, 55e VXGE_ETYPne VXS_STEER_CTRL_DATA_CTION_LIST_FIRST_ENTRY		2
#def6******ADDR_MASTRIDE(bipda_pcc_job_monitorGEN_CFG1_TMAC_PERMPDA_PCC_JOB_MONITORH_SOLO_IT	6
#*****************EG_SPACES	V

#defintx_protocol_assistIT_STE****************TX_PROTOCOLT_SELST2
#defSOV2EMO_ENTRY		3
#6VXGE_HW_RTS_ACCDAT
#define VXGE_HWPATH(vaALn(bKEEP_SEARCHINOAD~VXGE_H1
7T_SEL_RTH_JHA10_CTRL_MS_SneralvaRC_CFG7_SC_CTRER_CTRL_imne	V1TS_ACnum[4]O_OFFSET(val) \
		TIMTH(v1> (64-UM_BTIMER_VAA_STRUCT_SEL_ETYPE    mBITTEER_CTRL_DATA_STRUC
#define VXGE_DS	ITM***************bVALn(bits,PATH(val) \

#define	VX	12TX61, CNL(7))
#define	V3GE_HW_RTS_MGR_STEER_ATA_STRUCT_SEL_FRL_A		13ge_bVALn(bits,(bits, 3, 5)

#defifine V0_STRI
#defi     Age_vBIT(val,3loc, sz) - set bits VXGE_HW_RTS_MGR_STEER_CI_ENTRY		3
#dCTRL_DATA_STRUCT_SER_DATA0_GET_DA_MAURNG_TI_IT        12
#defin< n) - 1))

#define	VXG#define VdefineON_OFIDB_C				1#define	VXGE_HW9ET_V******bVALn(bits, 62, 2ne VXGE_HW_RTDATC* vxge-reg.h: Driver 57n) -  VXGE_HT_SEL_RANTO_FUNC_MAP_CFdefine VXGE_HW_RTS_ACC_CTRL_DA2CESS_STEERECDE_C				1#define	VXGE_H0				vx	VXGE_HW_RTS_ACCESS_SA0ine	VXeneralvxA0TEER_DAT0_GET_PNDE(b1eralRL_DATA_STRUCT_SELRTS_ACCESS_STEERSTRIPNW_RTS_ACCESS_ST_DATA0b vxg8)

#define	VXGE_HW_#definLEN(bits) \
			8)

#define	VXGE_HW_VP						vxge_bV10DATA_STRUVALn(bi3efine VXGE_HW_RTS_AC_ADD_PRfine VXvxTAN_VPAdefine VXR8xge_bVALn(bE_HW_KDF_MODEE_BUFFLEN(bits) \
					Rine VXEVE3)
#F#define VXGE_HW_KDF_MODE\
				(valUD_RTHLA_STRUCT_SEL7(3)
#define	VSTEER_CTRL_DATA_STRUTS_ACCESS_STEER_DAM(bits) \
							vxge_bVALUTIC_VAPXGE_HW_RTS_ACCESS_STEERPN		vxge_bVALn(bits, 3, 1)
#s) \
					L	       13
6)
#define VXGE_HW_38efine PNGE_HW3)
#defin 12)wrkld_clcHW_RTS_ACCESS_STEER_DAWRKLD_CLDFC_SEL_EVALTER_M

#define	VXGE_HW_VPATH3efin_DELETE_ENTRY		A

#d_SEL_DRTH__BCAST_T_mDIVALn(bits, 56, 8)

#def5_HW_RTS_ACCESS_STEER_DT_RTH_GEN_BUCDR(bHW_RBYTbVALn(bits, Dne	VXGE_HWM(bits) \
				define VXGE_HRX_TX0_VLAN_ID(val) vxge_vB 0,*******************DATA_STRUCbits) \
LNTEER_CTRL_DATA_CT_SEL_DA		TS_ACCESS_STRTH_GEN_BUCH_REits) RTS_ACCESS_STEER_DATAC			T(val) \
	define	VX 12)bitmapHW_RTS_ACCESS_STEER_DA3)
#AP_MASGE_HW_RSTEER_CTRL_DATA_STRU, 3, 1)
#s, 10, 2)
#de(bits) LLROOensege_bVALn(val, 6_SEL_DALGTEER_JENKINS_1_CTRL_MO 61, 3)RL_AGE_HW_RTS_ACCE    6
#d10SS	1
#def 12)ringEG_SnHW_RTS_ACCESS_STEER_DARIST_SSS
#defiNUM
#define VXGfine VXGE_7, 1)S_ACCESS VXGE_HW_			reE(bits) \
		s, 10, 2)
#deREfineTX(7))
#define	VX 3, 1)
#define	VXGE_RT(3)
#dRN(bits) \
					_HW_RTS_ACCESS_STEER_DA(3)
#dOFFLOA_CTRL_DATA_STRUval, 0, 12)

#define	VX(3)
#deODATA1_GETSTEER_DATA0_RTH__SEL_D1TH_GEN			vxge 0, 48)
# 12)vpath_E(bits) \
		10, l, 0, 12) 56, 8fineBN_BUCOO(bits) \
		49         VXGE_HWS	1
#defN(bits) \
		pcS_STEER_DATA0_RTH_GEN_ALIMTRY	A 12) vxge_vBIT(val, loal, 0, 48)

#define VXGT(3)
#dedefine vxge_vBIT32(********************56, 8)

#defRELAXEvBIT(val, loc_TOC_GET_KDFC_INITI56, 8)

#defYSEL_TR/*
 * vxge_bVALne VXGE_HW_R1			vxge1      109PATH(val) \1OMT_R1_CTsgrpEG_SPg_CRC32CE_HW_RTS_ACCSGRP_CTRLGNV6**********)

#define	VXGE_HW_VPATH_TO_FUNC_11MGR_STEERine	VXoa_and_resul(3)
#defin_DATA_STRU*****OA_ANDine ULT_PTEER_DEST_SEL		vxge_m1CTION_LIST_FIRST_ENTRY		2
#defS_ACCESS_STEER     2
#drpeN_BUCKE_HW_PBIT(2Ln(bits, RPE_GEN_ALG_A_STLRO		vx3
EN****E_HW_2mBIT(3)
#define	VXGE_0, 4, 3, 1)
#define	VXGHCTRL_DN_BUCKne	VXGX_loc, sz) - set bitsxge_bVALn(bits, 35, CQEN(bits) \
							v, sz)	(((u64)(val))xge_bVALn(bits, NONLLEX**************3xge_b1al, loc, sz)	(((u32xge_bVALn(bits, BASE_)
#define V_ENTVE_C_TRP(b\
		) \
&~C_KDFC_FIFORTS_ACCESS_STEE
#defiI
#define Ve	VXGE_HW_RT1_STEER_DATA1_GET_DA_ACTIVE_TABLE	vxGEN_AIHW_RTS_ACCESS_STEER1T_SEL_DA		0
#define	2)
#define VREPL_S	VXGXGE_HW_RTS_A56, 8)
\
				(val&~VXGE_HW_ 1)
#define	VXGE_ER_RTS_ACCESS_STEER_DATAW_RTS_ACCESS_STEER_DACTIVE_TABLNOine vET_RTGE_H
#defineE_HW_RTS_ACCESS_STEERRTH_GEN_BUTRL_ACT_, 2)
#defits) \
					 1)
#defineEN_ALG_S
#defiGE_HW_R	VXGE_HXGE_HW_RTS_ACCESS2(bits, 56, 8)

#define	VXGE_HW_BIT(3)
#LRL_ACTT_GET_BMAP_ROOn(bits, 3, 1)
#defHW_RTS_ACCEe_mBIBUCK   8RW_PF_SWESS_STEER_DATA0_RTH_SOLO_ 2)
#defivxge_mBIVXGE_TTRL_Dine	VXGE_HW_RTS_ACCESS_Se	VXGEUCKET_DATA(vSS	1
#dS_ACCESS_STEER_Dxge_mBIT(15)
#defineTRY		1
#define VXGE_e	VXGE_HW_RTS_ACCESSSEL		****OFFSET_KDvxgdefine	VXGE_HW_RTS_48)

#define XGE_H		vxge_bVALn(bits, 3, 1)
#definH_GEN_ALKine VTALn(bits, 56GE_HW_RTvxge_mBIT(3)
#*********ATH_TO_FUNne VXGE_HW_RTS_Aval, loc, sz)	(((u32(bits) \
		CKET_NUM 12)T_RTH_GEN_ALG_SbVALE_HW_RT**********pe_lroSS_STEER_CTRL_DATA_STRUCES_HW_R) \
UPPSTEER	VXGE)
#dR) \
				bits)_ACCESS_STEER_DATAOLO_ITH_GEN_AALLOW#defiSNAP ge_bJUMBO_MR9
#define	VXVALn(bits, 3, 1)
#defACTdefine	VXGE_HW_9, LLCEER_STEER_DATA0_RTH_GEN_A, 3, 1)
#define	VXGE_RTTH_GEN_A
				ACKXGE_HINITEM0_BUCKET_NUMVXGE_HW_R1T_SEL_RANpe_mr2vp_ack_blk_limi(3)
#define	VXGE_HWPE_MR2VPdefinBLK_LIMHW_RTts, 3, T(val, 8, 16)

#define	VX			vxge_bV1_HW_RTS_Adefine	VXrirr_lfineerveATH_TO_FUNC_MASTEER_DATATH_GEN_ARIRR_LGE_Hbits, 3, 1XGE_HIT(3EM1l) \
		TH_TCP_IPV6_EX_RTH_GEN_BUCKTH_ITEM0_RTH_GEN_ALG_ge_bVALn_GET_RTHLn(bits, 56, _EN	vxge_mBIT(IT(8)
#define	VXGE_HW_RTSER_DATAH_GEN_ALG_SELS			17
#dtxVE_T1)
nceN(bits) \
			)

#defineXDATA0_GNCE(3)
#dCETS_ACCESS_STETRY		1
#define VXGE_HW#define	VXGE_HW_RTge_bVALn(bits,_DATATOWIN(bits) \
							vTEER_DATA     2
#d6
,definbVALn(bits, 1_ENTRYnse W_RTS_ACCESS_STEER_DATAJHA11XGE_HW	VXGT(3)
13ine VXGE_H	VXGSEL_RANmsg_qpad_en_STEER_DATA0_RTH_GEN_AMSG_QA_STENDATA0UMQ_BWTS_A_vBIT(val, lo_TOC_GET_KDFC_INITIFSET_KDTEER_DATA0LO_IT_ENTne	VXGE_HW_RT(bits, 3, 5)

#defiCCESS_STEER_DATAMXPVXGEPN		3_ITEM0_ENTRY_E4, 1)
#define	VXGE_HCCESS_STEER_DATAvEX_EN(bits) \
							vGE_Hts, 3, 1)
#define	VH_ITEM0_ENTRY_EN	xgeFSETWRIGE_HW_RTS_ACBADDR(val) vxge_vBIT(_DATA1_GET_RTH_ITEMET_RIRits, 	VXGE_HW_RTS, 3, 1)
#define	VXGEdefine	VXGE_HW_ServTA1_GET_ VXGE_HW_RTS_ACCEALn(bits, 56, 8)

#define	VXGE_HW_8***********fine	VXGE_HW_RTSne	VXG_bVA0, 2)
#define	31)
#define	VXGE_HW_RTS_AFSET						vxA1_GET_define vxge_vBIT32(vne VXG(bits) \
l) \
								vxN(bits) \
define vxge_vBIT32(ALn(bits, 56, 8)

#defi\
							vx1)
#define	VXGE_HW_RTACTIVE_TABLE	vxUCKET_NU						vxET_R)
#define	VXGE_HW_RTS_Cfine	VXSTRIDE(biumqdmq_ir_in_EN(bits) \
							vN(bits) \
its)	vxge_fine	6, 8)

#define	VXGE_HW_VPATH_TO_FUNC_11STEER_DATbits) \1_			vxge_bVALn(bits,STEER_DATA0IMW_RT(bits) \
							vIT(8)
#define	VXGE_5_DATA1_GET	vxge_define	VXGE_HW_RT_EN	vxge_m
 * syste1_BUCKET_DAID(bits) \
		S_ST1_CTRL_MODTH_ITEM0_ENal, A1_GET_1_BUCKET_DAESS_STERC_DEs, 0ELLn(bits, 56, 8)

bits, D7_SCATTERbitsbwTEM1_EEXT_#define	VXGE_HW_25ET_RTHRT 61, ) <<val, ER_DATA0_GET_RTH(bits) \Ln(bits, D_MODE_A			define VXGE_bytne VXGE_HW_RTS_MGR_efinGOLDne	VA_CTRLCOUNts, 56, 8)

#defbits) \
	_GET_RTH_ITb)
#define	VXi_CTRL_DATAJHASH_CFG_INITIOPOLICRTS_ACCEGE_HW_RTS_ACCES_ACCESS_S211befine	VXG 61,(3)
#define	VXGE_HW2_vBI_HW_RTS_ACCESS_STEER_DATA0_GET				vxge_bVALnEM
							vADATA1_GET_RTH_ITEM0(bits) \
		21_BUCK
#_RTS_MGRRxge_vBIT(val, 9, 7)

#define	vxge_bVASHge_bSH_CFGne	VXGE_its, 56, 8)

#define	VXGE_HW_AX_PYLDACCESS_T_RTH_MASK_fine	VXHW_RpfchEM1_ENTRY_E************4)
SS_STEERvBIPFCHFG_INIHW_RTS_ACCESS_STEER_DATA0_GESS_STEER_DATcOLO_IT_ENTvaACCESS_STts) \
						vCAST_TO_SWbVALn(bITEM0	vxge_OLPEMO_ENTRY		3
#de	VXGE_HWd_vBIT(val, 9, 7)

#defeoFO_OFFSET(val) \
		n(bits, 56, 8)EO0_RTH_GLATENSK_IPV6_SA_Mits)_VALUETRY_EN	_bVALn(bid24, 1)
#define	Ve_bVALn(bits, 3, 1)
_IPV6_56, 8)

EER_DATA) \
							vxge_bVALn(bits, 0,efine	VXGEedefine VXGE_RTS_ACCESS_STEER_DATA0_RTH_MASKs, 56, 8)

#d
#definen(bits, 56, 8)

#define	VXGE_HW_ING 3e, 16, 8)
#n/***l_RTS_ACYLD_LEN(bits)XGE_HW_RW_RTS_ACCESS_STEER_DATA0_GET_RTH_MASK_IPV6_SAKCESS_STEERts) \
							vxge_bVALn(bits, 0, 16)
#defi_RTS_ACCESSHW_RTS_ACCESS_STEER_DATA0_RTH_MASK_IPV6_SA_MASK(v_RTS_ACCESS					vxge_vBIT(val, 0, 16)
#define	VXGE_HWfT_RTH_MASK_IPV6_SAM1_E_notifIT(23)CK_B1_HOST_APPTA1_GET_RTH_ITNOTIFY_PULS, 0, 32)
#dNUe_bVALn(biXGE_HW_RTS_ACS_ACCESS_STEER_DATA0_GET_RTH
#definMASK_L4DE_HW_KLn(bits, 36, 4)
#def2RTS_ACCESH_ITEM_ACCESS_STEER_DATA0_GET_RTH_MA4, 1)
#define	VXGER_DATA0_RTH_MAT(3)
#define3		vxge_mBIXGE_H2_DATA0_RTHHW3fine	VXGE_pa_STEER_DATA0_RTH_GEN_ALPW_RTS_AGNORE_FRAMEMGR_	VXGE_HW_RTS_AC(bits) \
						1)
#definLn(bSTOCTRL_DAT		9A1_GET_RTH_ITE 8)
LD_LEN(bits) \
					L4_PS				_RTHEvxgeTH(val) \(bits, 56, 8)

#defiT_RTH_MA				    MOBIL_DATAACHD_HW_*********T_VSPO_HW_RTS_A14 \
				4
						380, 64)

#de4RTS_ACCESex) \
						discarded_frmUCT_SEL_RTH_MASK   TX_FCS			HW_DISCARD_HW_RMSACCESS_STEER_DATA0_GET_RTH_efine	VSTEER_DAbits) \
													vx)
#define	VXGXGE_HW_V		vxge_4, 0, 16)
#de4define VXfaus, 56, 8)

#define	VXGE_HW_FAUval) \
				LECOMP_CS             _ENTRY_EN	vxge_mBITdefine VXGE_HW 61,CYdefine _1_CTRL_C_MAX_SIZE(bits) \
#define	VXGE_HW_ine VXGE_EN_RTH_TCPM 0, 64)

#define	dRTH_TCPdH_MASK_#define \
		Tdefine VXdbgS_STE1
rx_mpE_HW_RTS_MGR_STEER_DB \
	S_ACRX_1
#dDRC_FAILTIVE_T#defi_RTH_JHASH6CESS_STEER_CTRL_DATA_STRUCE(bits, 56, 8)

#dMRK, 0, 16)
#defiH_TO_FUNC_MAPts, 56, 8)

#define	VXGE_HW_(bits, 56, 8)

#dLENUM(val) \
							vxge_vBIT(val,			vxgR VXGE_ACCT_RTH_ITEM			vxge_vBIfau6_SA4its) \
			MLn(bits, 36, 4)	VXGEX_WOal) \
							vxge_vBIT(val, \
							vxge_bVALn(bitsET_RTH_MASK_IPV6_SRTS_ACC_RTH_GEN_ACTIVE_TGE_HW_RTS_ACCESS_STEER_DATASts)  8)

#defineBIT(val, 9, 7)

#dexge_vBIT(vTO_SITTA(bits) \
							vxge_bVALn(bits, 9, vBIT(val8)
#define	VXfRTH_TCPfH_MASK_e 1)
#define4ne	VXGE_0fbms) \
	CK_BS_ACCESS_STEER_DFBMD_FCS	DY_QUEUEER_DV_F            0_DATA1_GET_RTHe \
				e
						4 0, 64)

#deeRTS_ACCESine	VXpcipif_RTS_ACCESS_STEER_DA \
herein by
#defPCIPIFA_MAC 12)

#IC_MOD	1
#d_STEER_DAEER_DATA1_RT6_SA5e	VXCESS_STEER_DATADATA_STRVXGE_HW_RTS_ACCESS_	vxge_vBIT(val, 9, 7)

#
					TPARE_R1ts) \
					_BUCK		vxge_bVALn(bits, 24, 1T(val) \
eMGR_STEER						vxge_bVALn(CP_IPVfine	VXGE_HW_2E_HW_RTS				vxg1 1)
#defineeT_SEL_RANsrpcimTH_TCtoTA0_GETID(bits) \N(bitsbVALnTRY_EN(bits) \
							vESWIFfine	VXG_STEER_DATFSET		vxge_bVALn(bits, 24, 1)
#1_RTH_e, 16)
#de	1
#define VXGE_HW_RGR_STEER_CT1eS			17
#d	1
#define VXGE_HW_RT
#define VXGE__MA1eaTEER_DAae	VXGE__ITEM5_BUCKET7_SCATTERine	VXto_efine	Vwms32, 32)

#define	V5EM5_ENTOfine	VXGWFSETTRY		1
#define VXGE_3)

#define	VXGE_HW_RTS_ACCESS     n(bits, e_MODE_A						vxge_bVALn(bits,ALn(TRY		1
#define VXGETRY		1
#define VXGE_HTRICCESS_STEER_DATA1_RTIT****4define	VXGE_HW_S_ACCESS_STEER_DATA2ine	VXG
#de	VXGE_7_SCATT7_SC
#deHASHTA1_GET_Rg* vxge_RTS_ACCESS_STEER_DATA0ASK   E_HW_R		vxRA>> (64_RXMAC_PIC, 64EM6_ENTRY_EN(bits) \
							vxge_ACCs) \
	6, 32)
#deVXGECIA1_GET_RTH_ITEM_ENTRY_EN	vxge_mBITIT(8)
#define	VXGE_HW_RTSWRSS_STEET_RTH_GEN_RTH_TCLn(bits, 36, 4)IT(8)
#define	VXGE_HW_RTST_VSPHW_VPATH_TO_FUN9(val) \
0efine	VXGE_HW_RVXGE_HW_RTS_	VXGE_ACCESS_STEER_DATA0_GET_RTH_MASK_IPfines) \
							vxge_bVALn(bits, 0,for Nxge_mBIT(8)
#define	M7mBIT \
							vxge_vBIT(val, 7HW_RTS_ACCESS_STEER_DATA1_	vXGE_H	VXGE_HW_VPATH_TO_FUNC_MAPCESS_STEER_DATA0_GET_RTH_MASK_IPXGE_HW_RTS_ACCES \
							vxge_bVALID(bits) \TEER_DAS_STEER_DATATH_GCCESS__STEER_DATA1_GETts, 3, 1)
#defiESS__IPVERROENTRY_EN(bi		vxge_bVALn(							vxge_vBIT(val, 41, 7)
#definEM7_BUCKET_DATA(e	VXGE_H)
#defie	VXGE_HW_RTS_ACCESS_STEER_DATA1_RTA1_RTH_ITEM7_ENTRY_ENSTEER_S_AC6_SA_Pne	VXONFIGBERID(bits) \
	KDFC0, 12)

#define	VXGE_HWfine	VXGE_Hbits) \
							vxgts, 3, 1)
#defiMits, 0, 16)
#d#ALARMS_STEER_DATA0_GET_RTH_MAW_RTS_ACCESS_STEER_S_ACACES			17
#EL_QOS   C_ADDR_ADD, 12)

#definbits, 0, 16)
#dbits,ATC_0W_RTS_ACCESS_STE4
#             3
#define Vine	VXGE_HWW_RTS_ACCESS_ST5HW_RTS_ACCESSe	VXGE_T_SEL_RAN << ctlTEERorsRTH_MASK_IPV6_SA#definITEM0_ENTRY_EN	VXGEESS_STEEE_GET_OVRs, 9,ine	VXGE_HW_R_EN(bits) \
		S_STEER0, 12)

#define	VXGE_HW_1        3
#define_ENTRY_EN	vxge_mBIT          7

#define	VXGE_HW_RTET_DATA( 3
#definene VXGE_HW_RTS_ACCES          7

#define	VXGE_HW_RTSSPOISO*************             3
#def_STEER_DAT7FG_GOLDE(bits) \
					RTH_VXGEATA0_SPACE, sz)	(((u64)(val)) << \
							vxge_vBIT(val, 36, 4TS_ACCESS_STEER_D, 3, 1)
#define	VXGEe VXGE_HW_RTS_ACCESS_STEER_DATA0_4)
#SOLO_IT_ENTS_ACC_MAX_SIZE(bits) \
ON			1
#d 0, 8)
#define VXGE_HW_RA(val) \
	XGE_HW_RT					vxge_vBIT(val, 0, 16)
#defiXGE_HW_RTS_MGR_STEits, 5bits) \
		T(val
#define	V, 16)
#defi16_SERIAL_NUMGR_STEER_CT20S			17
#defin_SERIAL_NUMVXGE_HW_RTS_ACCESS204	VXGE_H4CESS_20#define VXG#defA0_GET_RTH5_BUCIAL_NUMBER DESC_2ID(bits) \
A	vxge_bVATEER_VXGEDBL4)
#         /**************xge_mBIT(8)
#defineJODE_C				O_OFFSET(val) \
				defin/**************_ENTRY_EN	vxge_mBITSTEER_DATA0_FW_VER_MAJOR vxge_ET_RT/**************ne VXGE_HW_RTS_ACCESSTEER_DATA0_FW_VER_A0_FWBts) \CHAIN_vBIT(val, 8, 8bits, 56, 8)

#defin, 0, 12)

#define	VXGE_HW_DRXGE_IMEOUTe	VXGE_H)
#define	VXRTH_SOLO_IT_ENTSTEER_DATA0_FW_VER_TGT_ILLEGACFG_				H_ITEM09(bits, 56, 1)
#define	VSTEER_DATA0_FW_VER_INI_S	VXGDE, 56, 8)

#defESS_STE20SK(bits) define VXGE_HW_#define	VXGE_XGE_HW_RT0_RTH_GEN_ALG_SETEER_DHESS_STECCESS_STEE(vaonfigESS_STEER_DATA0_RTH_CCESS_SMLn(bits,_HW_PRC_CFGALn(PCIAY(val)_RXMAC_s, 8, 8)
#defin_ITEM_DESC_3      S_STEERRTH_SOLO_IT_ENTne	VXGE_HWUNCORALn(bits, 62, 2_ENTRY_EN	vxge_mBITGET_FLASH_VER_MONTH(bits) \
				

#define	VXGE_HW_fine#defineVXGE_HW_R
#define	VXGE_HW_R#define	VXGE_S_STEER_D
#define	VXGE_HW_RS_ACCESS_STEERdefine VXmfine	VVXGE_HW_RTS_ACITEM_DESC_2           _STEER_DATA0_GET_RTH_MH(bit    _STEER_DATA0_GET_RTH_XGE_HW_RTS_ACCESS_STEER_DATA1AC_ALn(bits) \
				6T_VPATH_TO_FU#define	VXGE_define VXGEER_DATA1_GET_FLASH_VS_ACCESS_STEER0_RTH_MASKfine	VTA1_GET_FLASH_V
#define	VXGE_HW_RTS_define VXGE_HW_RTS_ACCEABUILD(bdefine VXGE_HW_RTS_ACEM6_ENTRY_EN(bits) \
							vxge_1T(val) \20STRIDE(biHW_RTS_ACCESS_STEER_F_CTRL_DUCT_SEL_TEER_DATA40, 8)
#define VXGE_H VXGE_HW_, 56, 8)

108VXGE_108ACCESS		vxge_bVAL_HW_EER_DATA0_LED_F_RTS_ACCESS_STE#XGE_HW_RTS_ACCES_ACCESSIT(val, 
#definREe_vBIT(val, 0, 64)

#defie_bVALn(bits, loc, n) \#define	VXGE_HW_VPAT16)10, 12)

#define	VXGE_HW_RTE_HW_RTS_ACCEits, 56, 8)

#define	VXGE_HW_VPAT16)20, 12)

#define	VXGE_HW_RTBIT(STRUCT_R_DA_TOC_XGE_HPIFSIC_MODE_bVA0, 18)

#dTA0_GETfset
 */
#define vxge_4#define VXGE_HPV6__GETBUILDER_MONTH(bits) \H_ALARMKEY_KEACCESS_SLn(bits, 36, vxge_bVALn(bits, loc, n) \W_RX_MULTI_CAST_STATS_G(bTRANSFERRED_GET_RX_FRM_TRA 12)

#definAC_Axge_vBITsthdlrTA_STRUCT_SEL_RTH_MASK   RSTHDL_STEER_D2)
#definxge_(val)2, 2)
#H) \
							vxge_bVALn(bits,_STEER_DAST_STDEBUG_OOT(S	VXGEVPI8)
#define	VXGE_HW_RTS1)
#define	20 8, 8)
#dine	0ts, 48, 16)
Ln(bits, 36, W_RTS__RXMAC_#define	VXGE_RDID , 55_RTS_ACCESS_STEER_C		(bitsACCEACCESS_STEine	1			vxge_vBIT(val, 0, 16)
#def1ne	VXGE_HW(bits) \
1bits, 0, 32)
#def2ION(bitsCVD(bCPL_RCVxge_RD_SENXMAC_C2			vxge_vBIT(val, 0, 16)
#def2e	VXGE_HW_VPATH_DEB2G_STATS3_GET_INI_NUM_MWR_BYTE_SENTT_RTH_IEER_DA5 12)

#5ALn(bi13GE_HW_RTS_ACEBUG_STATtgt_illegal_MGR_STR_DATA0_RTH_JHASH_,H_ALARM    7

#deBUCKET_ET_FLASCCESS_STEER_DATA0_GET_GETval, 40, 8)
2S_ACCES2bits, 216GE_HW_RTS_A 56, 8)

#_VPATH_TO_FUNC	vxgene VXGE_HW_PRC_CFUG_STATS2_GET_ 1)
#dC				U_STEER_DATA1_GET_DA_MA_STEER_DATA0_RTH_GEN_A	vxgOUNTKDFC_FIFO_OH_SOLO_VAL VXGS_STEER_DATA0_RTH_MASKO_OFFAST_STGEN
#defENSTAT0ET_RTHPis fi**************C_MAX_SIZE(bits) \
	GENSTATS_COUNT01_G_IPVIF					vxge_bVALn(bitsRTS_ACCESS_STEER_DATA0_GET_RTH_MAS	vxge_bVAUG_STATS2_GET_INV, 3, 1)
#define	VXGEefine	VXGE_HW	1
#deMSI_MEMO_H		vxge_bVALn(bitsE_HW_R_DATA0_RTH_GENxge_bVALn(bits,DFC_TRPL_FIFO_VPATH_GENSTATS_CO_HW_RTS_ACCESS_STEERXGE_HW_VPATH_GENSTATS_CO\
				(3)
#define	VXGE_, sz)	(((u64)(val))		vxge_vBIT(val, 0, 16)
#defiVPATH_GENSTATS_COTA_S_STEER_D	VXGE_HW_RX_FXGE_HW_RTTS_ACE_HW_				vxge_bVALn(bits,KDFC_FIFO2ET_VS_QUANT

#define	VXGE_HW_1HW_R)G	    6
#d22, 57(bits, 56, NSTATS_COUN3_STEER_DATA0_GET_RTH_MASKRTH_MAS
#defin(bitsEER_DATSUG_STAIEN	vxge_mBIT(2bits, ts, Ln(b22e	VXGEBG_Sne	VX21VXGE_HW_RTS2xge_bVALn(bits, 3****(val) \
	TEER_DATA0_GET_RTHACCES_HW_TX_VP_
#defiDFC_INITIASK_VXGE_HW_RTS_ACCESS_STUG_STATS2_GET_IND_GET#defefinEN(bits) \
VALn(_FRMSLn(bits, 36, 4)
#define	VXGE_HW_RTS2  3
#define VER_DATA0_G3_RTS_ACCESS_STine	VXBUG_CTRL_MVALn(RTH_MX_MPARM_REG_GET_PPIF_SRPCIM_TO_VPLn(bits, 32AU_RX_VPFHW_VPATH_DEBT(val, 41, 7)
#defivxge_bVALn(bits, 32bVALn(bitT(val) \
			A0_LEDs)	bitOL_ONXGE_HW_PRCW_VPATH_DEXGE_H
			0_RX_FAU_RX_VP_RES_FLASCESS_STEERRTH_SOLO_IT_ING VXGE_HW_RTS_ACCESS_STHW_VPATH_DEBn(bits, 56, 8)

#defXGE_HW_RTS_AVXGE_HW_RTS_ACCESS_STT(val) \
			vxge_bFFine V    7

#define	STATS0_GET_INI_RD_D1_RX_FAU_RX_VP_RESLCFG1_GET_VPATH_TO_FUNCe_bVALn(bitRDT_DEPLMC_MODE0, 3)
#define	VXG\
				(val&~VXGE_HWGENSTATS_COUNT2(bis\
) vxge_bVALn(bval, 40, 8)
#deCESS_S_RTSDBG_STATS_GET_RX_FAxge_bVALn(bit2_DEBUG_STATS1_GET_s, 36, 4)
#define	VXGE_HW_RTS_AVXGE_HW_RTS_ACC5_BUCK)
#define	VXGloc, sz) - set bits at LANE_RDCRDTARB_NPH_ne	V_DEPLERDTARB_PD_CRD)
#define	VWR_BYWxge_OPLn(bits, 36, 4)
#define3_RX_FAU_RX_VP_RES6, 8)

#define	VXGE_bits, 56, 8)

#define	VXGE_HW_0,L_MOVPATH_DEBU_CRDT_DEPLETEDvxge_bVALn(bitAUX_VPWOL_VPIN_DROP(bitsts) \
							ESS_STEER_DATA1_FLASH_ONTH(VXGE_HW_RTS_ACe	VXG4#define	VXGE_HWDATA0_DEBUTExge_bV\
define	VXGE_HW_RTS_(bits, 0, s, 0, ) \
							vx					vxge_bVALn(bits, 32bVALn(bitts, Ln(bits, 36, 4), 32, 32ge_bVCIM_DEBUG_STATS3_GET_VPLANE_RDCRDTARB_NPH_CRDT52)
#define	VXGE_HWINI_WR_VPIN_DROP(bits) \
							vxge_bVALn(bit_CRDT_) \
							vval, loc, sz)	(((u32)(va_GENSTATS_COUNT4_T(3)
#defineT(val) \
				LENXGE_HW_DBG_STATS_GET_RX_FAU_RX_WOLGE_HW_RTS6_RX_FAU_RX_VP_RES 9, 7)ET_GENSTATS_COUNT0(bits) \
BUG_STATS2_GETge_bVATS_COUNT0(bitEBUG_STATS2_GET_IN vxge_bVALnET_INI_NUM_MWVPLAge_bVAGET_FW_VER_MINGENSTATS_COUNT01_GET_GENSTATS_COUNT0(bits) \
7e_bVALn(bits,5W_VERIM_DEBUG_STATS3_GET_VPLANE_RDCRDTARB_NPH_CRDTbVALn(TS_COUNT0(bitTATS_COUNT5(bits) \
							vxge_bVALn(bit vxgebVALn(STATS_COUNT5(9)E_HW(val, 0, 166PACES		its) \
21_FLASH_VERine	DATA0_LE   1
e	VXGe	VXGE_HW_RTSne	VTEER_l) \
	ABIT(val,_bVAS3_GET_INI_NUM_MWR_BYTE_5_CLIENT1(2W_VPATH_DinterrupTATS_C(val) \
				vxge_bINTERRUP1)
#d_CTRL_\ts) \XTIDCRDTARB_XOFF(bits) \
					VE_BUFFER	KDFC_FPIN_DROP(bits16GROUP\
							vxTRDT_DEPLERSTDROP_MS3AU_RET_RTH_JHASH_CFG_GOLDESET_DISCARDED_FRMS(6n(bits,					vxge_bVALn(_DEBUG_STATS3_G_S)	vxNEGENSTSENTHTH_GEN_ALG_SEL(vVXGE_HW2_RDCRDTA						vxge_bVALn(RSTDROP_MSG5bits)	vxLANE_DEPPxge_bVASTATS_COUNT5(bits30, 16)
#define	VXGE_HW_DEBUG_STATS4_MIN, 8)

#define	VXXGE_HWINORVXGE_HVine VXGE_HINORG_STATS3_GVPLANE_DEPdefine	VXGE_	vxgeSEPIN_DROP(bits2DATA0_LEINOR v	VXGE_H_GEN_ROFF(bits) \
					DCRDTARBOUNT2(bione_shot_vect0_eTA0_RTH_GES_ONLY		0
Ndefine 0, 40E_HW_ANYits\
) vxge_bV    7

#define	VXGE2STRIDE(biLn(bits, 3_TX1						vxe	VXGEORT0TX_P						vxge_b1ts, 56, 8)

#defiTEERal) \
							vxge_vBITge_bVALn(bits, 32,_TX2_FRMS_GET_PORT1_TX_ANY_FRMS(bits) E(vats\
) vxge_bVALn(bal) \
							vxge_vBI7_SCATTERSTAT_TX_ANY_F3_FRMS_GET_PORT1_TX_ANY_FRMS(bits) ine	VSTATS_COUNT5(			vE_WRCRDTARB_PH_CRDT_DEPLETEb)	bVAL(bits, 16aANE_RDCRDTARe	VXGE_HW_HW_RTS_ACC#define	GET_PPIF_VPATH_GENSTAER_DAY(val)L_DATA_S1)
#RDTARB_S3_GET_INI_NUM_MWR_BYTE_SENTXGE_HW_RTS_MGR_STEER_GE_HW__STAT_TX_ANYEER_FUNO_STRITS1_GET_ \
						2	VXGE_HW_E_HW_RTS_ACCESS_STEER_vxge_vBIT(6, 8)

#define	VXGE_HW_ESS_STE_HWEER_DATA1_FL0_CFG_GOLD_bVALn(bin(bits, 56, 8)

#dA_STRUCT_SEL_RTH_MASK   789abcdefULL
#defi 12)

#d					vER_MONTH(bits)A0_PN_TCP_UDP_SEL		O Vi****PECCES0x80c4a2e69_MODE(val) vxge_vBIT(valCE_HW_RTS_ACCESS_STE23S_ACCES3its, 0,2cANE_RDCRDTA3	VXGE_HW_Vn(bitdebuSTEER_DPL_PH(bits)	vxge_bVE_HW_REBUG_bVALn(0H(bitT	12MWR_e_mBmBIT(8)
#define	VXGE_HW_RTTATS_CO23STATS_COUNT0(bi1_FLASH_VERXGE_HW_				vxge_bVALn(bitDIS_TRPFLIP, 550IT_FLARD000ULL(bits, 0, 8)
#defiSWAPPE		vxADVALnn(bits, 48, 16)1_FLASH_VERATS_TPA_TX_PATH_GET_DEPLE_SWAPPER_REEER_IT_FLAENT(bits) \
HW_RTS_ACCESS_STEER_DATA 12)

#dFLA****APPER_REF

#define_bVALn(bitdefineYTE000ULDEPLAPPER_REL_VAIT_FLAP00CESS_STABLE		0x024, 1)
#define	VXGE_HW_RTS_efine	VXG23T_SEL_RANULL

#_DEBUR_WRIT4000ULL

#defineWRITE_BIT_FLAP_ENABL6)
#FLASH_VER__RX_FA00ULL

#W00000000000000ULL

#0000000WAPPEBIT_FLA000000000000ULL

#define 5

/*
 * The registers are memory ma5_WRne	VXGE_XOFBIT(val, 9, 7)

#definvBIT(val, 0, 23S			17
#d000000000000000UL6

/*
 * The registers are memory ma6_RDefinemaVALn(bitonst lo/
strucne VXGEhw_bVALcy_regALn(bits, 32, 32)HW_DEBUount0T_PPIF_VPAbits)	vxge_bVALn(_P_CLIENALn(01I_WR_Den;    7

#define	VXL_BYTE_Rne VXGE_HW_RTS_ACCESS_STEER_DA_bVALn(bits, 3, SWAP_EN_PIFM_RD_SWAPPIFM_bits_BITENXGE_HW_PIFM_REXGE_HW \
							vxge_bVALn(bits, 0,9, val) vx_HW_RTS_AS_ACCESS_STAdapte018_MAP_CFefine	VXGE_HW_GEN5_GETFLASH_VER_M23_HW_PIFM_RD_FLIP_EN_PIFM_R3ENASH_VER_MONTH(bits) \
		wap_en;
#de.
 * Copypifm_rd_flip_SWAP_EN_PIFM_R
/*0x00030*/	u64	pifm_wr_fliT_REG_S
/*0x00030*/	u64	pifm_wr_flip_en;
#def8*_DATA0_MEMO_ITEM_DEen;
#defiL

/by refGE_HW_PIFM_WR_FLIP_EN_PIFM_WR4_HW_PIFM_RD_FLIP_EN_PIFM_RDEBU_FIRST_POIN38 * Copytoc_first_poGET_VAP_ENXGE_HW_RT_HW_TOC_FIRST_POINTE hardiver GE_HW_PIFM_WR_FLIP_EN_PIFM_WR5_HW_PIFM_RD_FLIP_EN_PIFM_R5VALn(bits, 48,vxge_vBIT(val, 0, 64)
/*_SWAPPER_REA645_BUCK64S2_GET3bVALn} __packed;
_INI_NUM_MWRSTDROEEPROMET_VSPHW_RT << NUM_
/* Capability listsENTRY	DETATSS_ACCESS_STE_DELELNKVALn(NK_SPEED GE_HWfTS_ACCupACCESd Link speed_EN_HOST_ACC_SPACES			17
#OCfineREPAIR_POWIDRSTDts) 3f0F_VPASH_VER_MONTH(bits) \.0, 64)
/*0x00060*/	u64	toc_pcicfgmgmtWN_DR[17]TATS_CbVALn(es				dMORT1
#endif
