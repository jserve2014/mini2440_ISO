/*
 *  smctr.c: A network driver for the SMC Token Ring Adapters.
 *
 *  Written by Jay Schulist <jschlst@samba.org>
 *
 *  This software may be used and distributed according to the terms
 *  of the GNU General Public License, incorporated herein by reference.
 *
 *  This device driver works with the following SMC adapters:
 *      - SMC TokenCard Elite   (8115T, chips 825/584)
 *      - SMC TokenCard Elite/A MCA (8115T/A, chips 825/594)
 *
 *  Source(s):
 *  	- SMC TokenCard SDK.
 *
 *  Maintainer(s):
 *    JS        Jay Schulist <jschlst@samba.org>
 *
 * Changes:
 *    07102000          JS      Fixed a timing problem in smctr_wait_cmd();
 *                              Also added a bit more discriptive error msgs.
 *    07122000          JS      Fixed problem with detecting a card with
 *				module io/irq/mem specified.
 *
 *  To do:
 *    1. Multicast support.
 *
 *  Initial 2.5 cleanup Alan Cox <alan@lxorguk.ukuu.org.uk>  2002/10/28
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/mca-legacy.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/trdevice.h>
#include <linux/bitops.h>
#include <linux/firmware.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>

#if BITS_PER_LONG == 64
#error FIXME: driver does not support 64-bit platforms
#endif

#include "smctr.h"               /* Our Stuff */

static const char version[] __initdata =
	KERN_INFO "smctr.c: v1.4 7/12/00 by jschlst@samba.org\n";
static const char cardname[] = "smctr";


#define SMCTR_IO_EXTENT   20

#ifdef CONFIG_MCA_LEGACY
static unsigned int smctr_posid = 0x6ec6;
#endif

static int ringspeed;

/* SMC Name of the Adapter. */
static char smctr_name[] = "SMC TokenCard";
static char *smctr_model = "Unknown";

/* Use 0 for production, 1 for verification, 2 for debug, and
 * 3 for very verbose debug.
 */
#ifndef SMCTR_DEBUG
#define SMCTR_DEBUG 1
#endif
static unsigned int smctr_debug = SMCTR_DEBUG;

/* smctr.c prototypes and functions are arranged alphabeticly 
 * for clearity, maintainability and pure old fashion fun. 
 */
/* A */
static int smctr_alloc_shared_memory(struct net_device *dev);

/* B */
static int smctr_bypass_state(struct net_device *dev);

/* C */
static int smctr_checksum_firmware(struct net_device *dev);
static int __init smctr_chk_isa(struct net_device *dev);
static int smctr_chg_rx_mask(struct net_device *dev);
static int smctr_clear_int(struct net_device *dev);
static int smctr_clear_trc_reset(int ioaddr);
static int smctr_close(struct net_device *dev);

/* D */
static int smctr_decode_firmware(struct net_device *dev,
				 const struct firmware *fw);
static int smctr_disable_16bit(struct net_device *dev);
static int smctr_disable_adapter_ctrl_store(struct net_device *dev);
static int smctr_disable_bic_int(struct net_device *dev);

/* E */
static int smctr_enable_16bit(struct net_device *dev);
static int smctr_enable_adapter_ctrl_store(struct net_device *dev);
static int smctr_enable_adapter_ram(struct net_device *dev);
static int smctr_enable_bic_int(struct net_device *dev);

/* G */
static int __init smctr_get_boardid(struct net_device *dev, int mca);
static int smctr_get_group_address(struct net_device *dev);
static int smctr_get_functional_address(struct net_device *dev);
static unsigned int smctr_get_num_rx_bdbs(struct net_device *dev);
static int smctr_get_physical_drop_number(struct net_device *dev);
static __u8 *smctr_get_rx_pointer(struct net_device *dev, short queue);
static int smctr_get_station_id(struct net_device *dev);
static FCBlock *smctr_get_tx_fcb(struct net_device *dev, __u16 queue,
        __u16 bytes_count);
static int smctr_get_upstream_neighbor_addr(struct net_device *dev);

/* H */
static int smctr_hardware_send_packet(struct net_device *dev,
        struct net_local *tp);
/* I */
static int smctr_init_acbs(struct net_device *dev);
static int smctr_init_adapter(struct net_device *dev);
static int smctr_init_card_real(struct net_device *dev);
static int smctr_init_rx_bdbs(struct net_device *dev);
static int smctr_init_rx_fcbs(struct net_device *dev);
static int smctr_init_shared_memory(struct net_device *dev);
static int smctr_init_tx_bdbs(struct net_device *dev);
static int smctr_init_tx_fcbs(struct net_device *dev);
static int smctr_internal_self_test(struct net_device *dev);
static irqreturn_t smctr_interrupt(int irq, void *dev_id);
static int smctr_issue_enable_int_cmd(struct net_device *dev,
        __u16 interrupt_enable_mask);
static int smctr_issue_int_ack(struct net_device *dev, __u16 iack_code,
        __u16 ibits);
static int smctr_issue_init_timers_cmd(struct net_device *dev);
static int smctr_issue_init_txrx_cmd(struct net_device *dev);
static int smctr_issue_insert_cmd(struct net_device *dev);
static int smctr_issue_read_ring_status_cmd(struct net_device *dev);
static int smctr_issue_read_word_cmd(struct net_device *dev, __u16 aword_cnt);
static int smctr_issue_remove_cmd(struct net_device *dev);
static int smctr_issue_resume_acb_cmd(struct net_device *dev);
static int smctr_issue_resume_rx_bdb_cmd(struct net_device *dev, __u16 queue);
static int smctr_issue_resume_rx_fcb_cmd(struct net_device *dev, __u16 queue);
static int smctr_issue_resume_tx_fcb_cmd(struct net_device *dev, __u16 queue);
static int smctr_issue_test_internal_rom_cmd(struct net_device *dev);
static int smctr_issue_test_hic_cmd(struct net_device *dev);
static int smctr_issue_test_mac_reg_cmd(struct net_device *dev);
static int smctr_issue_trc_loopback_cmd(struct net_device *dev);
static int smctr_issue_tri_loopback_cmd(struct net_device *dev);
static int smctr_issue_write_byte_cmd(struct net_device *dev,
        short aword_cnt, void *byte);
static int smctr_issue_write_word_cmd(struct net_device *dev,
        short aword_cnt, void *word);

/* J */
static int smctr_join_complete_state(struct net_device *dev);

/* L */
static int smctr_link_tx_fcbs_to_bdbs(struct net_device *dev);
static int smctr_load_firmware(struct net_device *dev);
static int smctr_load_node_addr(struct net_device *dev);
static int smctr_lobe_media_test(struct net_device *dev);
static int smctr_lobe_media_test_cmd(struct net_device *dev);
static int smctr_lobe_media_test_state(struct net_device *dev);

/* M */
static int smctr_make_8025_hdr(struct net_device *dev,
        MAC_HEADER *rmf, MAC_HEADER *tmf, __u16 ac_fc);
static int smctr_make_access_pri(struct net_device *dev,
        MAC_SUB_VECTOR *tsv);
static int smctr_make_addr_mod(struct net_device *dev, MAC_SUB_VECTOR *tsv);
static int smctr_make_auth_funct_class(struct net_device *dev,
        MAC_SUB_VECTOR *tsv);
static int smctr_make_corr(struct net_device *dev,
        MAC_SUB_VECTOR *tsv, __u16 correlator);
static int smctr_make_funct_addr(struct net_device *dev,
        MAC_SUB_VECTOR *tsv);
static int smctr_make_group_addr(struct net_device *dev,
        MAC_SUB_VECTOR *tsv);
static int smctr_make_phy_drop_num(struct net_device *dev,
        MAC_SUB_VECTOR *tsv);
static int smctr_make_product_id(struct net_device *dev, MAC_SUB_VECTOR *tsv);
static int smctr_make_station_id(struct net_device *dev, MAC_SUB_VECTOR *tsv);
static int smctr_make_ring_station_status(struct net_device *dev,
        MAC_SUB_VECTOR *tsv);
static int smctr_make_ring_station_version(struct net_device *dev,
        MAC_SUB_VECTOR *tsv);
static int smctr_make_tx_status_code(struct net_device *dev,
        MAC_SUB_VECTOR *tsv, __u16 tx_fstatus);
static int smctr_make_upstream_neighbor_addr(struct net_device *dev,
        MAC_SUB_VECTOR *tsv);
static int smctr_make_wrap_data(struct net_device *dev,
        MAC_SUB_VECTOR *tsv);

/* O */
static int smctr_open(struct net_device *dev);
static int smctr_open_tr(struct net_device *dev);

/* P */
struct net_device *smctr_probe(int unit);
static int __init smctr_probe1(struct net_device *dev, int ioaddr);
static int smctr_process_rx_packet(MAC_HEADER *rmf, __u16 size,
        struct net_device *dev, __u16 rx_status);

/* R */
static int smctr_ram_memory_test(struct net_device *dev);
static int smctr_rcv_chg_param(struct net_device *dev, MAC_HEADER *rmf,
        __u16 *correlator);
static int smctr_rcv_init(struct net_device *dev, MAC_HEADER *rmf,
        __u16 *correlator);
static int smctr_rcv_tx_forward(struct net_device *dev, MAC_HEADER *rmf);
static int smctr_rcv_rq_addr_state_attch(struct net_device *dev,
        MAC_HEADER *rmf, __u16 *correlator);
static int smctr_rcv_unknown(struct net_device *dev, MAC_HEADER *rmf,
        __u16 *correlator);
static int smctr_reset_adapter(struct net_device *dev);
static int smctr_restart_tx_chain(struct net_device *dev, short queue);
static int smctr_ring_status_chg(struct net_device *dev);
static int smctr_rx_frame(struct net_device *dev);

/* S */
static int smctr_send_dat(struct net_device *dev);
static netdev_tx_t smctr_send_packet(struct sk_buff *skb,
					   struct net_device *dev);
static int smctr_send_lobe_media_test(struct net_device *dev);
static int smctr_send_rpt_addr(struct net_device *dev, MAC_HEADER *rmf,
        __u16 correlator);
static int smctr_send_rpt_attch(struct net_device *dev, MAC_HEADER *rmf,
        __u16 correlator);
static int smctr_send_rpt_state(struct net_device *dev, MAC_HEADER *rmf,
        __u16 correlator);
static int smctr_send_rpt_tx_forward(struct net_device *dev,
        MAC_HEADER *rmf, __u16 tx_fstatus);
static int smctr_send_rsp(struct net_device *dev, MAC_HEADER *rmf,
        __u16 rcode, __u16 correlator);
static int smctr_send_rq_init(struct net_device *dev);
static int smctr_send_tx_forward(struct net_device *dev, MAC_HEADER *rmf,
        __u16 *tx_fstatus);
static int smctr_set_auth_access_pri(struct net_device *dev,
        MAC_SUB_VECTOR *rsv);
static int smctr_set_auth_funct_class(struct net_device *dev,
        MAC_SUB_VECTOR *rsv);
static int smctr_set_corr(struct net_device *dev, MAC_SUB_VECTOR *rsv,
	__u16 *correlator);
static int smctr_set_error_timer_value(struct net_device *dev,
        MAC_SUB_VECTOR *rsv);
static int smctr_set_frame_forward(struct net_device *dev,
        MAC_SUB_VECTOR *rsv, __u8 dc_sc);
static int smctr_set_local_ring_num(struct net_device *dev,
        MAC_SUB_VECTOR *rsv);
static unsigned short smctr_set_ctrl_attention(struct net_device *dev);
static void smctr_set_multicast_list(struct net_device *dev);
static int smctr_set_page(struct net_device *dev, __u8 *buf);
static int smctr_set_phy_drop(struct net_device *dev,
        MAC_SUB_VECTOR *rsv);
static int smctr_set_ring_speed(struct net_device *dev);
static int smctr_set_rx_look_ahead(struct net_device *dev);
static int smctr_set_trc_reset(int ioaddr);
static int smctr_setup_single_cmd(struct net_device *dev,
        __u16 command, __u16 subcommand);
static int smctr_setup_single_cmd_w_data(struct net_device *dev,
        __u16 command, __u16 subcommand);
static char *smctr_malloc(struct net_device *dev, __u16 size);
static int smctr_status_chg(struct net_device *dev);

/* T */
static void smctr_timeout(struct net_device *dev);
static int smctr_trc_send_packet(struct net_device *dev, FCBlock *fcb,
        __u16 queue);
static __u16 smctr_tx_complete(struct net_device *dev, __u16 queue);
static unsigned short smctr_tx_move_frame(struct net_device *dev,
        struct sk_buff *skb, __u8 *pbuff, unsigned int bytes);

/* U */
static int smctr_update_err_stats(struct net_device *dev);
static int smctr_update_rx_chain(struct net_device *dev, __u16 queue);
static int smctr_update_tx_chain(struct net_device *dev, FCBlock *fcb,
        __u16 queue);

/* W */
static int smctr_wait_cmd(struct net_device *dev);
static int smctr_wait_while_cbusy(struct net_device *dev);

#define TO_256_BYTE_BOUNDRY(X)  (((X + 0xff) & 0xff00) - X)
#define TO_PARAGRAPH_BOUNDRY(X) (((X + 0x0f) & 0xfff0) - X)
#define PARAGRAPH_BOUNDRY(X)    smctr_malloc(dev, TO_PARAGRAPH_BOUNDRY(X))

/* Allocate Adapter Shared Memory.
 * IMPORTANT NOTE: Any changes to this function MUST be mirrored in the
 * function "get_num_rx_bdbs" below!!!
 *
 * Order of memory allocation:
 *
 *       0. Initial System Configuration Block Pointer
 *       1. System Configuration Block
 *       2. System Control Block
 *       3. Action Command Block
 *       4. Interrupt Status Block
 *
 *       5. MAC TX FCB'S
 *       6. NON-MAC TX FCB'S
 *       7. MAC TX BDB'S
 *       8. NON-MAC TX BDB'S
 *       9. MAC RX FCB'S
 *      10. NON-MAC RX FCB'S
 *      11. MAC RX BDB'S
 *      12. NON-MAC RX BDB'S
 *      13. MAC TX Data Buffer( 1, 256 byte buffer)
 *      14. MAC RX Data Buffer( 1, 256 byte buffer)
 *
 *      15. NON-MAC TX Data Buffer
 *      16. NON-MAC RX Data Buffer
 */
static int smctr_alloc_shared_memory(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_alloc_shared_memory\n", dev->name);

        /* Allocate initial System Control Block pointer.
         * This pointer is located in the last page, last offset - 4.
         */
        tp->iscpb_ptr = (ISCPBlock *)(tp->ram_access + ((__u32)64 * 0x400)
                - (long)ISCP_BLOCK_SIZE);

        /* Allocate System Control Blocks. */
        tp->scgb_ptr = (SCGBlock *)smctr_malloc(dev, sizeof(SCGBlock));
        PARAGRAPH_BOUNDRY(tp->sh_mem_used);

        tp->sclb_ptr = (SCLBlock *)smctr_malloc(dev, sizeof(SCLBlock));
        PARAGRAPH_BOUNDRY(tp->sh_mem_used);

        tp->acb_head = (ACBlock *)smctr_malloc(dev,
                sizeof(ACBlock)*tp->num_acbs);
        PARAGRAPH_BOUNDRY(tp->sh_mem_used);

        tp->isb_ptr = (ISBlock *)smctr_malloc(dev, sizeof(ISBlock));
        PARAGRAPH_BOUNDRY(tp->sh_mem_used);

        tp->misc_command_data = (__u16 *)smctr_malloc(dev, MISC_DATA_SIZE);
        PARAGRAPH_BOUNDRY(tp->sh_mem_used);

        /* Allocate transmit FCBs. */
        tp->tx_fcb_head[MAC_QUEUE] = (FCBlock *)smctr_malloc(dev,
                sizeof(FCBlock) * tp->num_tx_fcbs[MAC_QUEUE]);

        tp->tx_fcb_head[NON_MAC_QUEUE] = (FCBlock *)smctr_malloc(dev,
                sizeof(FCBlock) * tp->num_tx_fcbs[NON_MAC_QUEUE]);

        tp->tx_fcb_head[BUG_QUEUE] = (FCBlock *)smctr_malloc(dev,
                sizeof(FCBlock) * tp->num_tx_fcbs[BUG_QUEUE]);

        /* Allocate transmit BDBs. */
        tp->tx_bdb_head[MAC_QUEUE] = (BDBlock *)smctr_malloc(dev,
                sizeof(BDBlock) * tp->num_tx_bdbs[MAC_QUEUE]);

        tp->tx_bdb_head[NON_MAC_QUEUE] = (BDBlock *)smctr_malloc(dev,
                sizeof(BDBlock) * tp->num_tx_bdbs[NON_MAC_QUEUE]);

        tp->tx_bdb_head[BUG_QUEUE] = (BDBlock *)smctr_malloc(dev,
                sizeof(BDBlock) * tp->num_tx_bdbs[BUG_QUEUE]);

        /* Allocate receive FCBs. */
        tp->rx_fcb_head[MAC_QUEUE] = (FCBlock *)smctr_malloc(dev,
                sizeof(FCBlock) * tp->num_rx_fcbs[MAC_QUEUE]);

        tp->rx_fcb_head[NON_MAC_QUEUE] = (FCBlock *)smctr_malloc(dev,
                sizeof(FCBlock) * tp->num_rx_fcbs[NON_MAC_QUEUE]);

        /* Allocate receive BDBs. */
        tp->rx_bdb_head[MAC_QUEUE] = (BDBlock *)smctr_malloc(dev,
                sizeof(BDBlock) * tp->num_rx_bdbs[MAC_QUEUE]);

        tp->rx_bdb_end[MAC_QUEUE] = (BDBlock *)smctr_malloc(dev, 0);

        tp->rx_bdb_head[NON_MAC_QUEUE] = (BDBlock *)smctr_malloc(dev,
                sizeof(BDBlock) * tp->num_rx_bdbs[NON_MAC_QUEUE]);

        tp->rx_bdb_end[NON_MAC_QUEUE] = (BDBlock *)smctr_malloc(dev, 0);

        /* Allocate MAC transmit buffers.
         * MAC Tx Buffers doen't have to be on an ODD Boundry.
         */
        tp->tx_buff_head[MAC_QUEUE]
                = (__u16 *)smctr_malloc(dev, tp->tx_buff_size[MAC_QUEUE]);
        tp->tx_buff_curr[MAC_QUEUE] = tp->tx_buff_head[MAC_QUEUE];
        tp->tx_buff_end [MAC_QUEUE] = (__u16 *)smctr_malloc(dev, 0);

        /* Allocate BUG transmit buffers. */
        tp->tx_buff_head[BUG_QUEUE]
                = (__u16 *)smctr_malloc(dev, tp->tx_buff_size[BUG_QUEUE]);
        tp->tx_buff_curr[BUG_QUEUE] = tp->tx_buff_head[BUG_QUEUE];
        tp->tx_buff_end[BUG_QUEUE] = (__u16 *)smctr_malloc(dev, 0);

        /* Allocate MAC receive data buffers.
         * MAC Rx buffer doesn't have to be on a 256 byte boundary.
         */
        tp->rx_buff_head[MAC_QUEUE] = (__u16 *)smctr_malloc(dev,
                RX_DATA_BUFFER_SIZE * tp->num_rx_bdbs[MAC_QUEUE]);
        tp->rx_buff_end[MAC_QUEUE] = (__u16 *)smctr_malloc(dev, 0);

        /* Allocate Non-MAC transmit buffers.
         * ?? For maximum Netware performance, put Tx Buffers on
         * ODD Boundry and then restore malloc to Even Boundrys.
         */
        smctr_malloc(dev, 1L);
        tp->tx_buff_head[NON_MAC_QUEUE]
                = (__u16 *)smctr_malloc(dev, tp->tx_buff_size[NON_MAC_QUEUE]);
        tp->tx_buff_curr[NON_MAC_QUEUE] = tp->tx_buff_head[NON_MAC_QUEUE];
        tp->tx_buff_end [NON_MAC_QUEUE] = (__u16 *)smctr_malloc(dev, 0);
        smctr_malloc(dev, 1L);

        /* Allocate Non-MAC receive data buffers.
         * To guarantee a minimum of 256 contigous memory to
         * UM_Receive_Packet's lookahead pointer, before a page
         * change or ring end is encountered, place each rx buffer on
         * a 256 byte boundary.
         */
        smctr_malloc(dev, TO_256_BYTE_BOUNDRY(tp->sh_mem_used));
        tp->rx_buff_head[NON_MAC_QUEUE] = (__u16 *)smctr_malloc(dev,
                RX_DATA_BUFFER_SIZE * tp->num_rx_bdbs[NON_MAC_QUEUE]);
        tp->rx_buff_end[NON_MAC_QUEUE] = (__u16 *)smctr_malloc(dev, 0);

        return (0);
}

/* Enter Bypass state. */
static int smctr_bypass_state(struct net_device *dev)
{
        int err;

	if(smctr_debug > 10)
        	printk(KERN_DEBUG "%s: smctr_bypass_state\n", dev->name);

        err = smctr_setup_single_cmd(dev, ACB_CMD_CHANGE_JOIN_STATE, JS_BYPASS_STATE);

        return (err);
}

static int smctr_checksum_firmware(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        __u16 i, checksum = 0;

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_checksum_firmware\n", dev->name);

        smctr_enable_adapter_ctrl_store(dev);

        for(i = 0; i < CS_RAM_SIZE; i += 2)
                checksum += *((__u16 *)(tp->ram_access + i));

        tp->microcode_version = *(__u16 *)(tp->ram_access
                + CS_RAM_VERSION_OFFSET);
        tp->microcode_version >>= 8;

        smctr_disable_adapter_ctrl_store(dev);

        if(checksum)
                return (checksum);

        return (0);
}

static int __init smctr_chk_mca(struct net_device *dev)
{
#ifdef CONFIG_MCA_LEGACY
	struct net_local *tp = netdev_priv(dev);
	int current_slot;
	__u8 r1, r2, r3, r4, r5;

	current_slot = mca_find_unused_adapter(smctr_posid, 0);
	if(current_slot == MCA_NOTFOUND)
		return (-ENODEV);

	mca_set_adapter_name(current_slot, smctr_name);
	mca_mark_as_used(current_slot);
	tp->slot_num = current_slot;

	r1 = mca_read_stored_pos(tp->slot_num, 2);
	r2 = mca_read_stored_pos(tp->slot_num, 3);

	if(tp->slot_num)
		outb(CNFG_POS_CONTROL_REG, (__u8)((tp->slot_num - 1) | CNFG_SLOT_ENABLE_BIT));
	else
		outb(CNFG_POS_CONTROL_REG, (__u8)((tp->slot_num) | CNFG_SLOT_ENABLE_BIT));

	r1 = inb(CNFG_POS_REG1);
	r2 = inb(CNFG_POS_REG0);

	tp->bic_type = BIC_594_CHIP;

	/* IO */
	r2 = mca_read_stored_pos(tp->slot_num, 2);
	r2 &= 0xF0;
	dev->base_addr = ((__u16)r2 << 8) + (__u16)0x800;
	request_region(dev->base_addr, SMCTR_IO_EXTENT, smctr_name);

	/* IRQ */
	r5 = mca_read_stored_pos(tp->slot_num, 5);
	r5 &= 0xC;
        switch(r5)
	{
            	case 0:
			dev->irq = 3;
               		break;

            	case 0x4:
			dev->irq = 4;
               		break;

            	case 0x8:
			dev->irq = 10;
               		break;

            	default:
			dev->irq = 15;
               		break;
	}
	if (request_irq(dev->irq, smctr_interrupt, IRQF_SHARED, smctr_name, dev)) {
		release_region(dev->base_addr, SMCTR_IO_EXTENT);
		return -ENODEV;
	}

	/* Get RAM base */
	r3 = mca_read_stored_pos(tp->slot_num, 3);
	tp->ram_base = ((__u32)(r3 & 0x7) << 13) + 0x0C0000;
	if (r3 & 0x8)
		tp->ram_base += 0x010000;
	if (r3 & 0x80)
		tp->ram_base += 0xF00000;

	/* Get Ram Size */
	r3 &= 0x30;
	r3 >>= 4;

	tp->ram_usable = (__u16)CNFG_SIZE_8KB << r3;
	tp->ram_size = (__u16)CNFG_SIZE_64KB;
	tp->board_id |= TOKEN_MEDIA;

	r4 = mca_read_stored_pos(tp->slot_num, 4);
	tp->rom_base = ((__u32)(r4 & 0x7) << 13) + 0x0C0000;
	if (r4 & 0x8)
		tp->rom_base += 0x010000;

	/* Get ROM size. */
	r4 >>= 4;
	switch (r4) {
		case 0:
			tp->rom_size = CNFG_SIZE_8KB;
			break;
		case 1:
			tp->rom_size = CNFG_SIZE_16KB;
			break;
		case 2:
			tp->rom_size = CNFG_SIZE_32KB;
			break;
		default:
			tp->rom_size = ROM_DISABLE;
	}

	/* Get Media Type. */
	r5 = mca_read_stored_pos(tp->slot_num, 5);
	r5 &= CNFG_MEDIA_TYPE_MASK;
	switch(r5)
	{
		case (0):
			tp->media_type = MEDIA_STP_4;
			break;

		case (1):
			tp->media_type = MEDIA_STP_16;
			break;

		case (3):
			tp->media_type = MEDIA_UTP_16;
			break;

		default:
			tp->media_type = MEDIA_UTP_4;
			break;
	}
	tp->media_menu = 14;

	r2 = mca_read_stored_pos(tp->slot_num, 2);
	if(!(r2 & 0x02))
		tp->mode_bits |= EARLY_TOKEN_REL;

	/* Disable slot */
	outb(CNFG_POS_CONTROL_REG, 0);

	tp->board_id = smctr_get_boardid(dev, 1);
	switch(tp->board_id & 0xffff)
        {
                case WD8115TA:
                        smctr_model = "8115T/A";
                        break;

                case WD8115T:
			if(tp->extra_info & CHIP_REV_MASK)
                                smctr_model = "8115T rev XE";
                        else
                                smctr_model = "8115T rev XD";
                        break;

                default:
                        smctr_model = "Unknown";
                        break;
        }

	return (0);
#else
	return (-1);
#endif /* CONFIG_MCA_LEGACY */
}

static int smctr_chg_rx_mask(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        int err = 0;

        if(smctr_debug > 10)
		printk(KERN_DEBUG "%s: smctr_chg_rx_mask\n", dev->name);

        smctr_enable_16bit(dev);
        smctr_set_page(dev, (__u8 *)tp->ram_access);

        if(tp->mode_bits & LOOPING_MODE_MASK)
                tp->config_word0 |= RX_OWN_BIT;
        else
                tp->config_word0 &= ~RX_OWN_BIT;

        if(tp->receive_mask & PROMISCUOUS_MODE)
                tp->config_word0 |= PROMISCUOUS_BIT;
        else
                tp->config_word0 &= ~PROMISCUOUS_BIT;

        if(tp->receive_mask & ACCEPT_ERR_PACKETS)
                tp->config_word0 |= SAVBAD_BIT;
        else
                tp->config_word0 &= ~SAVBAD_BIT;

        if(tp->receive_mask & ACCEPT_ATT_MAC_FRAMES)
                tp->config_word0 |= RXATMAC;
        else
                tp->config_word0 &= ~RXATMAC;

        if(tp->receive_mask & ACCEPT_MULTI_PROM)
                tp->config_word1 |= MULTICAST_ADDRESS_BIT;
        else
                tp->config_word1 &= ~MULTICAST_ADDRESS_BIT;

        if(tp->receive_mask & ACCEPT_SOURCE_ROUTING_SPANNING)
                tp->config_word1 |= SOURCE_ROUTING_SPANNING_BITS;
        else
        {
                if(tp->receive_mask & ACCEPT_SOURCE_ROUTING)
                        tp->config_word1 |= SOURCE_ROUTING_EXPLORER_BIT;
                else
                        tp->config_word1 &= ~SOURCE_ROUTING_SPANNING_BITS;
        }

        if((err = smctr_issue_write_word_cmd(dev, RW_CONFIG_REGISTER_0,
                &tp->config_word0)))
        {
                return (err);
        }

        if((err = smctr_issue_write_word_cmd(dev, RW_CONFIG_REGISTER_1,
                &tp->config_word1)))
        {
                return (err);
        }

        smctr_disable_16bit(dev);

        return (0);
}

static int smctr_clear_int(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);

        outb((tp->trc_mask | CSR_CLRTINT), dev->base_addr + CSR);

        return (0);
}

static int smctr_clear_trc_reset(int ioaddr)
{
        __u8 r;

        r = inb(ioaddr + MSR);
        outb(~MSR_RST & r, ioaddr + MSR);

        return (0);
}

/*
 * The inverse routine to smctr_open().
 */
static int smctr_close(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        struct sk_buff *skb;
        int err;

	netif_stop_queue(dev);
	
	tp->cleanup = 1;

        /* Check to see if adapter is already in a closed state. */
        if(tp->status != OPEN)
                return (0);

        smctr_enable_16bit(dev);
        smctr_set_page(dev, (__u8 *)tp->ram_access);

        if((err = smctr_issue_remove_cmd(dev)))
        {
                smctr_disable_16bit(dev);
                return (err);
        }

        for(;;)
        {
                skb = skb_dequeue(&tp->SendSkbQueue);
                if(skb == NULL)
                        break;
                tp->QueueSkb++;
                dev_kfree_skb(skb);
        }


        return (0);
}

static int smctr_decode_firmware(struct net_device *dev,
				 const struct firmware *fw)
{
        struct net_local *tp = netdev_priv(dev);
        short bit = 0x80, shift = 12;
        DECODE_TREE_NODE *tree;
        short branch, tsize;
        __u16 buff = 0;
        long weight;
        __u8 *ucode;
        __u16 *mem;

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_decode_firmware\n", dev->name);

        weight  = *(long *)(fw->data + WEIGHT_OFFSET);
        tsize   = *(__u8 *)(fw->data + TREE_SIZE_OFFSET);
        tree    = (DECODE_TREE_NODE *)(fw->data + TREE_OFFSET);
        ucode   = (__u8 *)(fw->data + TREE_OFFSET
                        + (tsize * sizeof(DECODE_TREE_NODE)));
        mem     = (__u16 *)(tp->ram_access);

        while(weight)
        {
                branch = ROOT;
                while((tree + branch)->tag != LEAF && weight)
                {
                        branch = *ucode & bit ? (tree + branch)->llink
                                : (tree + branch)->rlink;

                        bit >>= 1;
                        weight--;

                        if(bit == 0)
                        {
                                bit = 0x80;
                                ucode++;
                        }
                }

                buff |= (tree + branch)->info << shift;
                shift -= 4;

                if(shift < 0)
                {
                        *(mem++) = SWAP_BYTES(buff);
                        buff    = 0;
                        shift   = 12;
                }
        }

        /* The following assumes the Control Store Memory has
         * been initialized to zero. If the last partial word
         * is zero, it will not be written.
         */
        if(buff)
                *(mem++) = SWAP_BYTES(buff);

        return (0);
}

static int smctr_disable_16bit(struct net_device *dev)
{
        return (0);
}

/*
 * On Exit, Adapter is:
 * 1. TRC is in a reset state and un-initialized.
 * 2. Adapter memory is enabled.
 * 3. Control Store memory is out of context (-WCSS is 1).
 */
static int smctr_disable_adapter_ctrl_store(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        int ioaddr = dev->base_addr;

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_disable_adapter_ctrl_store\n", dev->name);

        tp->trc_mask |= CSR_WCSS;
        outb(tp->trc_mask, ioaddr + CSR);

        return (0);
}

static int smctr_disable_bic_int(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        int ioaddr = dev->base_addr;

        tp->trc_mask = CSR_MSK_ALL | CSR_MSKCBUSY
	        | CSR_MSKTINT | CSR_WCSS;
        outb(tp->trc_mask, ioaddr + CSR);

        return (0);
}

static int smctr_enable_16bit(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        __u8    r;

        if(tp->adapter_bus == BUS_ISA16_TYPE)
        {
                r = inb(dev->base_addr + LAAR);
                outb((r | LAAR_MEM16ENB), dev->base_addr + LAAR);
        }

        return (0);
}

/*
 * To enable the adapter control store memory:
 * 1. Adapter must be in a RESET state.
 * 2. Adapter memory must be enabled.
 * 3. Control Store Memory is in context (-WCSS is 0).
 */
static int smctr_enable_adapter_ctrl_store(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        int ioaddr = dev->base_addr;

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_enable_adapter_ctrl_store\n", dev->name);

        smctr_set_trc_reset(ioaddr);
        smctr_enable_adapter_ram(dev);

        tp->trc_mask &= ~CSR_WCSS;
        outb(tp->trc_mask, ioaddr + CSR);

        return (0);
}

static int smctr_enable_adapter_ram(struct net_device *dev)
{
        int ioaddr = dev->base_addr;
        __u8 r;

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_enable_adapter_ram\n", dev->name);

        r = inb(ioaddr + MSR);
        outb(MSR_MEMB | r, ioaddr + MSR);

        return (0);
}

static int smctr_enable_bic_int(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        int ioaddr = dev->base_addr;
        __u8 r;

        switch(tp->bic_type)
        {
                case (BIC_584_CHIP):
                        tp->trc_mask = CSR_MSKCBUSY | CSR_WCSS;
                        outb(tp->trc_mask, ioaddr + CSR);
                        r = inb(ioaddr + IRR);
                        outb(r | IRR_IEN, ioaddr + IRR);
                        break;

                case (BIC_594_CHIP):
                        tp->trc_mask = CSR_MSKCBUSY | CSR_WCSS;
                        outb(tp->trc_mask, ioaddr + CSR);
                        r = inb(ioaddr + IMCCR);
                        outb(r | IMCCR_EIL, ioaddr + IMCCR);
                        break;
        }

        return (0);
}

static int __init smctr_chk_isa(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        int ioaddr = dev->base_addr;
        __u8 r1, r2, b, chksum = 0;
        __u16 r;
	int i;
	int err = -ENODEV;

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_chk_isa %#4x\n", dev->name, ioaddr);

	if((ioaddr & 0x1F) != 0)
                goto out;

        /* Grab the region so that no one else tries to probe our ioports. */
	if (!request_region(ioaddr, SMCTR_IO_EXTENT, smctr_name)) {
		err = -EBUSY;
		goto out;
	}

        /* Checksum SMC node address */
        for(i = 0; i < 8; i++)
        {
                b = inb(ioaddr + LAR0 + i);
                chksum += b;
        }

        if (chksum != NODE_ADDR_CKSUM)
                goto out2;

        b = inb(ioaddr + BDID);
	if(b != BRD_ID_8115T)
        {
                printk(KERN_ERR "%s: The adapter found is not supported\n", dev->name);
                goto out2;
        }

        /* Check for 8115T Board ID */
        r2 = 0;
        for(r = 0; r < 8; r++)
        {
            r1 = inb(ioaddr + 0x8 + r);
            r2 += r1;
        }

        /* value of RegF adds up the sum to 0xFF */
        if((r2 != 0xFF) && (r2 != 0xEE))
                goto out2;

        /* Get adapter ID */
        tp->board_id = smctr_get_boardid(dev, 0);
        switch(tp->board_id & 0xffff)
        {
                case WD8115TA:
                        smctr_model = "8115T/A";
                        break;

                case WD8115T:
			if(tp->extra_info & CHIP_REV_MASK)
                                smctr_model = "8115T rev XE";
                        else
                                smctr_model = "8115T rev XD";
                        break;

                default:
                        smctr_model = "Unknown";
                        break;
        }

        /* Store BIC type. */
        tp->bic_type = BIC_584_CHIP;
        tp->nic_type = NIC_825_CHIP;

        /* Copy Ram Size */
        tp->ram_usable  = CNFG_SIZE_16KB;
        tp->ram_size    = CNFG_SIZE_64KB;

        /* Get 58x Ram Base */
        r1 = inb(ioaddr);
        r1 &= 0x3F;

        r2 = inb(ioaddr + CNFG_LAAR_584);
        r2 &= CNFG_LAAR_MASK;
        r2 <<= 3;
        r2 |= ((r1 & 0x38) >> 3);

        tp->ram_base = ((__u32)r2 << 16) + (((__u32)(r1 & 0x7)) << 13);

        /* Get 584 Irq */
        r1 = 0;
        r1 = inb(ioaddr + CNFG_ICR_583);
        r1 &= CNFG_ICR_IR2_584;

        r2 = inb(ioaddr + CNFG_IRR_583);
        r2 &= CNFG_IRR_IRQS;     /* 0x60 */
        r2 >>= 5;

        switch(r2)
        {
                case 0:
                        if(r1 == 0)
                                dev->irq = 2;
                        else
                                dev->irq = 10;
                        break;

                case 1:
                        if(r1 == 0)
                                dev->irq = 3;
                        else
                                dev->irq = 11;
                        break;

                case 2:
                        if(r1 == 0)
                        {
                                if(tp->extra_info & ALTERNATE_IRQ_BIT)
                                        dev->irq = 5;
                                else
                                        dev->irq = 4;
                        }
                        else
                                dev->irq = 15;
                        break;

                case 3:
                        if(r1 == 0)
                                dev->irq = 7;
                        else
                                dev->irq = 4;
                        break;

                default:
                        printk(KERN_ERR "%s: No IRQ found aborting\n", dev->name);
                        goto out2;
         }

        if (request_irq(dev->irq, smctr_interrupt, IRQF_SHARED, smctr_name, dev))
                goto out2;

        /* Get 58x Rom Base */
        r1 = inb(ioaddr + CNFG_BIO_583);
        r1 &= 0x3E;
        r1 |= 0x40;

        tp->rom_base = (__u32)r1 << 13;

        /* Get 58x Rom Size */
        r1 = inb(ioaddr + CNFG_BIO_583);
        r1 &= 0xC0;
        if(r1 == 0)
                tp->rom_size = ROM_DISABLE;
        else
        {
                r1 >>= 6;
                tp->rom_size = (__u16)CNFG_SIZE_8KB << r1;
        }

        /* Get 58x Boot Status */
        r1 = inb(ioaddr + CNFG_GP2);

        tp->mode_bits &= (~BOOT_STATUS_MASK);

        if(r1 & CNFG_GP2_BOOT_NIBBLE)
                tp->mode_bits |= BOOT_TYPE_1;

        /* Get 58x Zero Wait State */
        tp->mode_bits &= (~ZERO_WAIT_STATE_MASK);

        r1 = inb(ioaddr + CNFG_IRR_583);

        if(r1 & CNFG_IRR_ZWS)
                 tp->mode_bits |= ZERO_WAIT_STATE_8_BIT;

        if(tp->board_id & BOARD_16BIT)
        {
                r1 = inb(ioaddr + CNFG_LAAR_584);

                if(r1 & CNFG_LAAR_ZWS)
                        tp->mode_bits |= ZERO_WAIT_STATE_16_BIT;
        }

        /* Get 584 Media Menu */
        tp->media_menu = 14;
        r1 = inb(ioaddr + CNFG_IRR_583);

        tp->mode_bits &= 0xf8ff;       /* (~CNFG_INTERFACE_TYPE_MASK) */
        if((tp->board_id & TOKEN_MEDIA) == TOKEN_MEDIA)
        {
                /* Get Advanced Features */
                if(((r1 & 0x6) >> 1) == 0x3)
                        tp->media_type |= MEDIA_UTP_16;
                else
                {
                        if(((r1 & 0x6) >> 1) == 0x2)
                                tp->media_type |= MEDIA_STP_16;
                        else
                        {
                                if(((r1 & 0x6) >> 1) == 0x1)
                                        tp->media_type |= MEDIA_UTP_4;

                                else
                                        tp->media_type |= MEDIA_STP_4;
                        }
                }

                r1 = inb(ioaddr + CNFG_GP2);
                if(!(r1 & 0x2) )           /* GP2_ETRD */
                        tp->mode_bits |= EARLY_TOKEN_REL;

                /* see if the chip is corrupted
                if(smctr_read_584_chksum(ioaddr))
                {
                        printk(KERN_ERR "%s: EEPROM Checksum Failure\n", dev->name);
			free_irq(dev->irq, dev);
                        goto out2;
                }
		*/
        }

        return (0);

out2:
	release_region(ioaddr, SMCTR_IO_EXTENT);
out:
	return err;
}

static int __init smctr_get_boardid(struct net_device *dev, int mca)
{
        struct net_local *tp = netdev_priv(dev);
        int ioaddr = dev->base_addr;
        __u8 r, r1, IdByte;
        __u16 BoardIdMask;

        tp->board_id = BoardIdMask = 0;

	if(mca)
	{
		BoardIdMask |= (MICROCHANNEL+INTERFACE_CHIP+TOKEN_MEDIA+PAGED_RAM+BOARD_16BIT);
		tp->extra_info |= (INTERFACE_594_CHIP+RAM_SIZE_64K+NIC_825_BIT+ALTERNATE_IRQ_BIT+SLOT_16BIT);
	}
	else
	{
        	BoardIdMask|=(INTERFACE_CHIP+TOKEN_MEDIA+PAGED_RAM+BOARD_16BIT);
        	tp->extra_info |= (INTERFACE_584_CHIP + RAM_SIZE_64K
        	        + NIC_825_BIT + ALTERNATE_IRQ_BIT);
	}

	if(!mca)
	{
        	r = inb(ioaddr + BID_REG_1);
        	r &= 0x0c;
       		outb(r, ioaddr + BID_REG_1);
        	r = inb(ioaddr + BID_REG_1);

        	if(r & BID_SIXTEEN_BIT_BIT)
        	{
        	        tp->extra_info |= SLOT_16BIT;
        	        tp->adapter_bus = BUS_ISA16_TYPE;
        	}
        	else
        	        tp->adapter_bus = BUS_ISA8_TYPE;
	}
	else
		tp->adapter_bus = BUS_MCA_TYPE;

        /* Get Board Id Byte */
        IdByte = inb(ioaddr + BID_BOARD_ID_BYTE);

        /* if Major version > 1.0 then
         *      return;
         */
        if(IdByte & 0xF8)
                return (-1);

        r1 = inb(ioaddr + BID_REG_1);
        r1 &= BID_ICR_MASK;
        r1 |= BID_OTHER_BIT;

        outb(r1, ioaddr + BID_REG_1);
        r1 = inb(ioaddr + BID_REG_3);

        r1 &= BID_EAR_MASK;
        r1 |= BID_ENGR_PAGE;

        outb(r1, ioaddr + BID_REG_3);
        r1 = inb(ioaddr + BID_REG_1);
        r1 &= BID_ICR_MASK;
        r1 |= (BID_RLA | BID_OTHER_BIT);

        outb(r1, ioaddr + BID_REG_1);

        r1 = inb(ioaddr + BID_REG_1);
        while(r1 & BID_RECALL_DONE_MASK)
                r1 = inb(ioaddr + BID_REG_1);

        r = inb(ioaddr + BID_LAR_0 + BID_REG_6);

        /* clear chip rev bits */
        tp->extra_info &= ~CHIP_REV_MASK;
        tp->extra_info |= ((r & BID_EEPROM_CHIP_REV_MASK) << 6);

        r1 = inb(ioaddr + BID_REG_1);
        r1 &= BID_ICR_MASK;
        r1 |= BID_OTHER_BIT;

        outb(r1, ioaddr + BID_REG_1);
        r1 = inb(ioaddr + BID_REG_3);

        r1 &= BID_EAR_MASK;
        r1 |= BID_EA6;

        outb(r1, ioaddr + BID_REG_3);
        r1 = inb(ioaddr + BID_REG_1);

        r1 &= BID_ICR_MASK;
        r1 |= BID_RLA;

        outb(r1, ioaddr + BID_REG_1);
        r1 = inb(ioaddr + BID_REG_1);

        while(r1 & BID_RECALL_DONE_MASK)
                r1 = inb(ioaddr + BID_REG_1);

        return (BoardIdMask);
}

static int smctr_get_group_address(struct net_device *dev)
{
        smctr_issue_read_word_cmd(dev, RW_INDIVIDUAL_GROUP_ADDR);

        return(smctr_wait_cmd(dev));
}

static int smctr_get_functional_address(struct net_device *dev)
{
        smctr_issue_read_word_cmd(dev, RW_FUNCTIONAL_ADDR);

        return(smctr_wait_cmd(dev));
}

/* Calculate number of Non-MAC receive BDB's and data buffers.
 * This function must simulate allocateing shared memory exactly
 * as the allocate_shared_memory function above.
 */
static unsigned int smctr_get_num_rx_bdbs(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        unsigned int mem_used = 0;

        /* Allocate System Control Blocks. */
        mem_used += sizeof(SCGBlock);

        mem_used += TO_PARAGRAPH_BOUNDRY(mem_used);
        mem_used += sizeof(SCLBlock);

        mem_used += TO_PARAGRAPH_BOUNDRY(mem_used);
        mem_used += sizeof(ACBlock) * tp->num_acbs;

        mem_used += TO_PARAGRAPH_BOUNDRY(mem_used);
        mem_used += sizeof(ISBlock);

        mem_used += TO_PARAGRAPH_BOUNDRY(mem_used);
        mem_used += MISC_DATA_SIZE;

        /* Allocate transmit FCB's. */
        mem_used += TO_PARAGRAPH_BOUNDRY(mem_used);

        mem_used += sizeof(FCBlock) * tp->num_tx_fcbs[MAC_QUEUE];
        mem_used += sizeof(FCBlock) * tp->num_tx_fcbs[NON_MAC_QUEUE];
        mem_used += sizeof(FCBlock) * tp->num_tx_fcbs[BUG_QUEUE];

        /* Allocate transmit BDBs. */
        mem_used += sizeof(BDBlock) * tp->num_tx_bdbs[MAC_QUEUE];
        mem_used += sizeof(BDBlock) * tp->num_tx_bdbs[NON_MAC_QUEUE];
        mem_used += sizeof(BDBlock) * tp->num_tx_bdbs[BUG_QUEUE];

        /* Allocate receive FCBs. */
        mem_used += sizeof(FCBlock) * tp->num_rx_fcbs[MAC_QUEUE];
        mem_used += sizeof(FCBlock) * tp->num_rx_fcbs[NON_MAC_QUEUE];

        /* Allocate receive BDBs. */
        mem_used += sizeof(BDBlock) * tp->num_rx_bdbs[MAC_QUEUE];

        /* Allocate MAC transmit buffers.
         * MAC transmit buffers don't have to be on an ODD Boundry.
         */
        mem_used += tp->tx_buff_size[MAC_QUEUE];

        /* Allocate BUG transmit buffers. */
        mem_used += tp->tx_buff_size[BUG_QUEUE];

        /* Allocate MAC receive data buffers.
         * MAC receive buffers don't have to be on a 256 byte boundary.
         */
        mem_used += RX_DATA_BUFFER_SIZE * tp->num_rx_bdbs[MAC_QUEUE];

        /* Allocate Non-MAC transmit buffers.
         * For maximum Netware performance, put Tx Buffers on
         * ODD Boundry,and then restore malloc to Even Boundrys.
         */
        mem_used += 1L;
        mem_used += tp->tx_buff_size[NON_MAC_QUEUE];
        mem_used += 1L;

        /* CALCULATE NUMBER OF NON-MAC RX BDB'S
         * AND NON-MAC RX DATA BUFFERS
         *
         * Make sure the mem_used offset at this point is the
         * same as in allocate_shared memory or the following
         * boundary adjustment will be incorrect (i.e. not allocating
         * the non-mac receive buffers above cannot change the 256
         * byte offset).
         *
         * Since this cannot be guaranteed, adding the full 256 bytes
         * to the amount of shared memory used at this point will guaranteed
         * that the rx data buffers do not overflow shared memory.
         */
        mem_used += 0x100;

        return((0xffff - mem_used) / (RX_DATA_BUFFER_SIZE + sizeof(BDBlock)));
}

static int smctr_get_physical_drop_number(struct net_device *dev)
{
        smctr_issue_read_word_cmd(dev, RW_PHYSICAL_DROP_NUMBER);

        return(smctr_wait_cmd(dev));
}

static __u8 * smctr_get_rx_pointer(struct net_device *dev, short queue)
{
        struct net_local *tp = netdev_priv(dev);
        BDBlock *bdb;

        bdb = (BDBlock *)((__u32)tp->ram_access
                + (__u32)(tp->rx_fcb_curr[queue]->trc_bdb_ptr));

        tp->rx_fcb_curr[queue]->bdb_ptr = bdb;

        return ((__u8 *)bdb->data_block_ptr);
}

static int smctr_get_station_id(struct net_device *dev)
{
        smctr_issue_read_word_cmd(dev, RW_INDIVIDUAL_MAC_ADDRESS);

        return(smctr_wait_cmd(dev));
}

/*
 * Get the current statistics. This may be called with the card open
 * or closed.
 */
static struct net_device_stats *smctr_get_stats(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);

        return ((struct net_device_stats *)&tp->MacStat);
}

static FCBlock *smctr_get_tx_fcb(struct net_device *dev, __u16 queue,
        __u16 bytes_count)
{
        struct net_local *tp = netdev_priv(dev);
        FCBlock *pFCB;
        BDBlock *pbdb;
        unsigned short alloc_size;
        unsigned short *temp;

        if(smctr_debug > 20)
                printk(KERN_DEBUG "smctr_get_tx_fcb\n");

        /* check if there is enough FCB blocks */
        if(tp->num_tx_fcbs_used[queue] >= tp->num_tx_fcbs[queue])
                return ((FCBlock *)(-1L));

        /* round off the input pkt size to the nearest even number */
        alloc_size = (bytes_count + 1) & 0xfffe;

        /* check if enough mem */
        if((tp->tx_buff_used[queue] + alloc_size) > tp->tx_buff_size[queue])
                return ((FCBlock *)(-1L));

        /* check if past the end ;
         * if exactly enough mem to end of ring, alloc from front.
         * this avoids update of curr when curr = end
         */
        if(((unsigned long)(tp->tx_buff_curr[queue]) + alloc_size)
                >= (unsigned long)(tp->tx_buff_end[queue]))
        {
                /* check if enough memory from ring head */
                alloc_size = alloc_size +
                        (__u16)((__u32)tp->tx_buff_end[queue]
                        - (__u32)tp->tx_buff_curr[queue]);

                if((tp->tx_buff_used[queue] + alloc_size)
                        > tp->tx_buff_size[queue])
                {
                        return ((FCBlock *)(-1L));
                }

                /* ring wrap */
                tp->tx_buff_curr[queue] = tp->tx_buff_head[queue];
        }

        tp->tx_buff_used[queue] += alloc_size;
        tp->num_tx_fcbs_used[queue]++;
        tp->tx_fcb_curr[queue]->frame_length = bytes_count;
        tp->tx_fcb_curr[queue]->memory_alloc = alloc_size;
        temp = tp->tx_buff_curr[queue];
        tp->tx_buff_curr[queue]
                = (__u16 *)((__u32)temp + (__u32)((bytes_count + 1) & 0xfffe));

        pbdb = tp->tx_fcb_curr[queue]->bdb_ptr;
        pbdb->buffer_length = bytes_count;
        pbdb->data_block_ptr = temp;
        pbdb->trc_data_block_ptr = TRC_POINTER(temp);

        pFCB = tp->tx_fcb_curr[queue];
        tp->tx_fcb_curr[queue] = tp->tx_fcb_curr[queue]->next_ptr;

        return (pFCB);
}

static int smctr_get_upstream_neighbor_addr(struct net_device *dev)
{
        smctr_issue_read_word_cmd(dev, RW_UPSTREAM_NEIGHBOR_ADDRESS);

        return(smctr_wait_cmd(dev));
}

static int smctr_hardware_send_packet(struct net_device *dev,
        struct net_local *tp)
{
        struct tr_statistics *tstat = &tp->MacStat;
        struct sk_buff *skb;
        FCBlock *fcb;

        if(smctr_debug > 10)
                printk(KERN_DEBUG"%s: smctr_hardware_send_packet\n", dev->name);

        if(tp->status != OPEN)
                return (-1);

        if(tp->monitor_state_ready != 1)
                return (-1);

        for(;;)
        {
                /* Send first buffer from queue */
                skb = skb_dequeue(&tp->SendSkbQueue);
                if(skb == NULL)
                        return (-1);

                tp->QueueSkb++;

                if(skb->len < SMC_HEADER_SIZE || skb->len > tp->max_packet_size)                        return (-1);

                smctr_enable_16bit(dev);
                smctr_set_page(dev, (__u8 *)tp->ram_access);

                if((fcb = smctr_get_tx_fcb(dev, NON_MAC_QUEUE, skb->len))
                        == (FCBlock *)(-1L))
                {
                        smctr_disable_16bit(dev);
                        return (-1);
                }

                smctr_tx_move_frame(dev, skb,
                        (__u8 *)fcb->bdb_ptr->data_block_ptr, skb->len);

                smctr_set_page(dev, (__u8 *)fcb);

                smctr_trc_send_packet(dev, fcb, NON_MAC_QUEUE);
                dev_kfree_skb(skb);

                tstat->tx_packets++;

                smctr_disable_16bit(dev);
        }

        return (0);
}

static int smctr_init_acbs(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        unsigned int i;
        ACBlock *acb;

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_init_acbs\n", dev->name);

        acb                     = tp->acb_head;
        acb->cmd_done_status    = (ACB_COMMAND_DONE | ACB_COMMAND_SUCCESSFUL);
        acb->cmd_info           = ACB_CHAIN_END;
        acb->cmd                = 0;
        acb->subcmd             = 0;
        acb->data_offset_lo     = 0;
        acb->data_offset_hi     = 0;
        acb->next_ptr
                = (ACBlock *)(((char *)acb) + sizeof(ACBlock));
        acb->trc_next_ptr       = TRC_POINTER(acb->next_ptr);

        for(i = 1; i < tp->num_acbs; i++)
        {
                acb             = acb->next_ptr;
                acb->cmd_done_status
                        = (ACB_COMMAND_DONE | ACB_COMMAND_SUCCESSFUL);
                acb->cmd_info = ACB_CHAIN_END;
                acb->cmd        = 0;
                acb->subcmd     = 0;
                acb->data_offset_lo = 0;
                acb->data_offset_hi = 0;
                acb->next_ptr
                        = (ACBlock *)(((char *)acb) + sizeof(ACBlock));
                acb->trc_next_ptr = TRC_POINTER(acb->next_ptr);
        }

        acb->next_ptr           = tp->acb_head;
        acb->trc_next_ptr       = TRC_POINTER(tp->acb_head);
        tp->acb_next            = tp->acb_head->next_ptr;
        tp->acb_curr            = tp->acb_head->next_ptr;
        tp->num_acbs_used       = 0;

        return (0);
}

static int smctr_init_adapter(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        int err;

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_init_adapter\n", dev->name);

        tp->status              = CLOSED;
        tp->page_offset_mask    = (tp->ram_usable * 1024) - 1;
        skb_queue_head_init(&tp->SendSkbQueue);
        tp->QueueSkb = MAX_TX_QUEUE;

        if(!(tp->group_address_0 & 0x0080))
                tp->group_address_0 |= 0x00C0;

        if(!(tp->functional_address_0 & 0x00C0))
                tp->functional_address_0 |= 0x00C0;

        tp->functional_address[0] &= 0xFF7F;

        if(tp->authorized_function_classes == 0)
                tp->authorized_function_classes = 0x7FFF;

        if(tp->authorized_access_priority == 0)
                tp->authorized_access_priority = 0x06;

        smctr_disable_bic_int(dev);
        smctr_set_trc_reset(dev->base_addr);

        smctr_enable_16bit(dev);
        smctr_set_page(dev, (__u8 *)tp->ram_access);

        if(smctr_checksum_firmware(dev))
	{
                printk(KERN_ERR "%s: Previously loaded firmware is missing\n",dev->name);                return (-ENOENT);
        }

        if((err = smctr_ram_memory_test(dev)))
	{
                printk(KERN_ERR "%s: RAM memory test failed.\n", dev->name);
                return (-EIO);
        }

	smctr_set_rx_look_ahead(dev);
        smctr_load_node_addr(dev);

        /* Initialize adapter for Internal Self Test. */
        smctr_reset_adapter(dev);
        if((err = smctr_init_card_real(dev)))
	{
                printk(KERN_ERR "%s: Initialization of card failed (%d)\n",
                        dev->name, err);
                return (-EINVAL);
        }

        /* This routine clobbers the TRC's internal registers. */
        if((err = smctr_internal_self_test(dev)))
	{
                printk(KERN_ERR "%s: Card failed internal self test (%d)\n",
                        dev->name, err);
                return (-EINVAL);
        }

        /* Re-Initialize adapter's internal registers */
        smctr_reset_adapter(dev);
        if((err = smctr_init_card_real(dev)))
	{
                printk(KERN_ERR "%s: Initialization of card failed (%d)\n",
                        dev->name, err);
                return (-EINVAL);
        }

        smctr_enable_bic_int(dev);

        if((err = smctr_issue_enable_int_cmd(dev, TRC_INTERRUPT_ENABLE_MASK)))
                return (err);

        smctr_disable_16bit(dev);

        return (0);
}

static int smctr_init_card_real(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        int err = 0;

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_init_card_real\n", dev->name);

        tp->sh_mem_used = 0;
        tp->num_acbs    = NUM_OF_ACBS;

        /* Range Check Max Packet Size */
        if(tp->max_packet_size < 256)
                tp->max_packet_size = 256;
        else
        {
                if(tp->max_packet_size > NON_MAC_TX_BUFFER_MEMORY)
                        tp->max_packet_size = NON_MAC_TX_BUFFER_MEMORY;
        }

        tp->num_of_tx_buffs = (NON_MAC_TX_BUFFER_MEMORY
                / tp->max_packet_size) - 1;

        if(tp->num_of_tx_buffs > NUM_NON_MAC_TX_FCBS)
                tp->num_of_tx_buffs = NUM_NON_MAC_TX_FCBS;
        else
        {
                if(tp->num_of_tx_buffs == 0)
                        tp->num_of_tx_buffs = 1;
        }

        /* Tx queue constants */
        tp->num_tx_fcbs        [BUG_QUEUE]     = NUM_BUG_TX_FCBS;
        tp->num_tx_bdbs        [BUG_QUEUE]     = NUM_BUG_TX_BDBS;
        tp->tx_buff_size       [BUG_QUEUE]     = BUG_TX_BUFFER_MEMORY;
        tp->tx_buff_used       [BUG_QUEUE]     = 0;
        tp->tx_queue_status    [BUG_QUEUE]     = NOT_TRANSMITING;

        tp->num_tx_fcbs        [MAC_QUEUE]     = NUM_MAC_TX_FCBS;
        tp->num_tx_bdbs        [MAC_QUEUE]     = NUM_MAC_TX_BDBS;
        tp->tx_buff_size       [MAC_QUEUE]     = MAC_TX_BUFFER_MEMORY;
        tp->tx_buff_used       [MAC_QUEUE]     = 0;
        tp->tx_queue_status    [MAC_QUEUE]     = NOT_TRANSMITING;

        tp->num_tx_fcbs        [NON_MAC_QUEUE] = NUM_NON_MAC_TX_FCBS;
        tp->num_tx_bdbs        [NON_MAC_QUEUE] = NUM_NON_MAC_TX_BDBS;
        tp->tx_buff_size       [NON_MAC_QUEUE] = NON_MAC_TX_BUFFER_MEMORY;
        tp->tx_buff_used       [NON_MAC_QUEUE] = 0;
        tp->tx_queue_status    [NON_MAC_QUEUE] = NOT_TRANSMITING;

        /* Receive Queue Constants */
        tp->num_rx_fcbs[MAC_QUEUE] = NUM_MAC_RX_FCBS;
        tp->num_rx_bdbs[MAC_QUEUE] = NUM_MAC_RX_BDBS;

        if(tp->extra_info & CHIP_REV_MASK)
                tp->num_rx_fcbs[NON_MAC_QUEUE] = 78;    /* 825 Rev. XE */
        else
                tp->num_rx_fcbs[NON_MAC_QUEUE] = 7;     /* 825 Rev. XD */

        tp->num_rx_bdbs[NON_MAC_QUEUE] = smctr_get_num_rx_bdbs(dev);

        smctr_alloc_shared_memory(dev);
        smctr_init_shared_memory(dev);

        if((err = smctr_issue_init_timers_cmd(dev)))
                return (err);

        if((err = smctr_issue_init_txrx_cmd(dev)))
	{
                printk(KERN_ERR "%s: Hardware failure\n", dev->name);
                return (err);
        }

        return (0);
}

static int smctr_init_rx_bdbs(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        unsigned int i, j;
        BDBlock *bdb;
        __u16 *buf;

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_init_rx_bdbs\n", dev->name);

        for(i = 0; i < NUM_RX_QS_USED; i++)
        {
                bdb = tp->rx_bdb_head[i];
                buf = tp->rx_buff_head[i];
                bdb->info = (BDB_CHAIN_END | BDB_NO_WARNING);
                bdb->buffer_length = RX_DATA_BUFFER_SIZE;
                bdb->next_ptr = (BDBlock *)(((char *)bdb) + sizeof(BDBlock));
                bdb->data_block_ptr = buf;
                bdb->trc_next_ptr = TRC_POINTER(bdb->next_ptr);

                if(i == NON_MAC_QUEUE)
                        bdb->trc_data_block_ptr = RX_BUFF_TRC_POINTER(buf);
                else
                        bdb->trc_data_block_ptr = TRC_POINTER(buf);

                for(j = 1; j < tp->num_rx_bdbs[i]; j++)
                {
                        bdb->next_ptr->back_ptr = bdb;
                        bdb = bdb->next_ptr;
                        buf = (__u16 *)((char *)buf + RX_DATA_BUFFER_SIZE);
                        bdb->info = (BDB_NOT_CHAIN_END | BDB_NO_WARNING);
                        bdb->buffer_length = RX_DATA_BUFFER_SIZE;
                        bdb->next_ptr = (BDBlock *)(((char *)bdb) + sizeof(BDBlock));
                        bdb->data_block_ptr = buf;
                        bdb->trc_next_ptr = TRC_POINTER(bdb->next_ptr);

                        if(i == NON_MAC_QUEUE)
                                bdb->trc_data_block_ptr = RX_BUFF_TRC_POINTER(buf);
                        else
                                bdb->trc_data_block_ptr = TRC_POINTER(buf);
                }

                bdb->next_ptr           = tp->rx_bdb_head[i];
                bdb->trc_next_ptr       = TRC_POINTER(tp->rx_bdb_head[i]);

                tp->rx_bdb_head[i]->back_ptr    = bdb;
                tp->rx_bdb_curr[i]              = tp->rx_bdb_head[i]->next_ptr;
        }

        return (0);
}

static int smctr_init_rx_fcbs(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        unsigned int i, j;
        FCBlock *fcb;

        for(i = 0; i < NUM_RX_QS_USED; i++)
        {
                fcb               = tp->rx_fcb_head[i];
                fcb->frame_status = 0;
                fcb->frame_length = 0;
                fcb->info         = FCB_CHAIN_END;
                fcb->next_ptr     = (FCBlock *)(((char*)fcb) + sizeof(FCBlock));
                if(i == NON_MAC_QUEUE)
                        fcb->trc_next_ptr = RX_FCB_TRC_POINTER(fcb->next_ptr);
                else
                        fcb->trc_next_ptr = TRC_POINTER(fcb->next_ptr);

                for(j = 1; j < tp->num_rx_fcbs[i]; j++)
                {
                        fcb->next_ptr->back_ptr = fcb;
                        fcb                     = fcb->next_ptr;
                        fcb->frame_status       = 0;
                        fcb->frame_length       = 0;
                        fcb->info               = FCB_WARNING;
                        fcb->next_ptr
                                = (FCBlock *)(((char *)fcb) + sizeof(FCBlock));

                        if(i == NON_MAC_QUEUE)
                                fcb->trc_next_ptr
                                        = RX_FCB_TRC_POINTER(fcb->next_ptr);
                        else
                                fcb->trc_next_ptr
                                        = TRC_POINTER(fcb->next_ptr);
                }

                fcb->next_ptr = tp->rx_fcb_head[i];

                if(i == NON_MAC_QUEUE)
                        fcb->trc_next_ptr = RX_FCB_TRC_POINTER(fcb->next_ptr);
                else
                        fcb->trc_next_ptr = TRC_POINTER(fcb->next_ptr);

                tp->rx_fcb_head[i]->back_ptr    = fcb;
                tp->rx_fcb_curr[i]              = tp->rx_fcb_head[i]->next_ptr;
        }

        return(0);
}

static int smctr_init_shared_memory(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        unsigned int i;
        __u32 *iscpb;

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_init_shared_memory\n", dev->name);

        smctr_set_page(dev, (__u8 *)(unsigned int)tp->iscpb_ptr);

        /* Initialize Initial System Configuration Point. (ISCP) */
        iscpb = (__u32 *)PAGE_POINTER(&tp->iscpb_ptr->trc_scgb_ptr);
        *iscpb = (__u32)(SWAP_WORDS(TRC_POINTER(tp->scgb_ptr)));

        smctr_set_page(dev, (__u8 *)tp->ram_access);

        /* Initialize System Configuration Pointers. (SCP) */
        tp->scgb_ptr->config = (SCGB_ADDRESS_POINTER_FORMAT
                | SCGB_MULTI_WORD_CONTROL | SCGB_DATA_FORMAT
                | SCGB_BURST_LENGTH);

        tp->scgb_ptr->trc_sclb_ptr      = TRC_POINTER(tp->sclb_ptr);
        tp->scgb_ptr->trc_acb_ptr       = TRC_POINTER(tp->acb_head);
        tp->scgb_ptr->trc_isb_ptr       = TRC_POINTER(tp->isb_ptr);
        tp->scgb_ptr->isbsiz            = (sizeof(ISBlock)) - 2;

        /* Initialize System Control Block. (SCB) */
        tp->sclb_ptr->valid_command    = SCLB_VALID | SCLB_CMD_NOP;
        tp->sclb_ptr->iack_code        = 0;
        tp->sclb_ptr->resume_control   = 0;
        tp->sclb_ptr->int_mask_control = 0;
        tp->sclb_ptr->int_mask_state   = 0;

        /* Initialize Interrupt Status Block. (ISB) */
        for(i = 0; i < NUM_OF_INTERRUPTS; i++)
        {
                tp->isb_ptr->IStatus[i].IType = 0xf0;
                tp->isb_ptr->IStatus[i].ISubtype = 0;
        }

        tp->current_isb_index = 0;

        /* Initialize Action Command Block. (ACB) */
        smctr_init_acbs(dev);

        /* Initialize transmit FCB's and BDB's. */
        smctr_link_tx_fcbs_to_bdbs(dev);
        smctr_init_tx_bdbs(dev);
        smctr_init_tx_fcbs(dev);

        /* Initialize receive FCB's and BDB's. */
        smctr_init_rx_bdbs(dev);
        smctr_init_rx_fcbs(dev);

        return (0);
}

static int smctr_init_tx_bdbs(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        unsigned int i, j;
        BDBlock *bdb;

        for(i = 0; i < NUM_TX_QS_USED; i++)
        {
                bdb = tp->tx_bdb_head[i];
                bdb->info = (BDB_NOT_CHAIN_END | BDB_NO_WARNING);
                bdb->next_ptr = (BDBlock *)(((char *)bdb) + sizeof(BDBlock));
                bdb->trc_next_ptr = TRC_POINTER(bdb->next_ptr);

                for(j = 1; j < tp->num_tx_bdbs[i]; j++)
                {
                        bdb->next_ptr->back_ptr = bdb;
                        bdb = bdb->next_ptr;
                        bdb->info = (BDB_NOT_CHAIN_END | BDB_NO_WARNING);
                        bdb->next_ptr
                                = (BDBlock *)(((char *)bdb) + sizeof( BDBlock));                        bdb->trc_next_ptr = TRC_POINTER(bdb->next_ptr);
                }

                bdb->next_ptr = tp->tx_bdb_head[i];
                bdb->trc_next_ptr = TRC_POINTER(tp->tx_bdb_head[i]);
                tp->tx_bdb_head[i]->back_ptr = bdb;
        }

        return (0);
}

static int smctr_init_tx_fcbs(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        unsigned int i, j;
        FCBlock *fcb;

        for(i = 0; i < NUM_TX_QS_USED; i++)
        {
                fcb               = tp->tx_fcb_head[i];
                fcb->frame_status = 0;
                fcb->frame_length = 0;
                fcb->info         = FCB_CHAIN_END;
                fcb->next_ptr = (FCBlock *)(((char *)fcb) + sizeof(FCBlock));
                fcb->trc_next_ptr = TRC_POINTER(fcb->next_ptr);

                for(j = 1; j < tp->num_tx_fcbs[i]; j++)
                {
                        fcb->next_ptr->back_ptr = fcb;
                        fcb                     = fcb->next_ptr;
                        fcb->frame_status       = 0;
                        fcb->frame_length       = 0;
                        fcb->info               = FCB_CHAIN_END;
                        fcb->next_ptr
                                = (FCBlock *)(((char *)fcb) + sizeof(FCBlock));
                        fcb->trc_next_ptr = TRC_POINTER(fcb->next_ptr);
                }

                fcb->next_ptr           = tp->tx_fcb_head[i];
                fcb->trc_next_ptr       = TRC_POINTER(tp->tx_fcb_head[i]);

                tp->tx_fcb_head[i]->back_ptr    = fcb;
                tp->tx_fcb_end[i]               = tp->tx_fcb_head[i]->next_ptr;
                tp->tx_fcb_curr[i]              = tp->tx_fcb_head[i]->next_ptr;
                tp->num_tx_fcbs_used[i]         = 0;
        }

        return (0);
}

static int smctr_internal_self_test(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        int err;

        if((err = smctr_issue_test_internal_rom_cmd(dev)))
                return (err);

        if((err = smctr_wait_cmd(dev)))
                return (err);

        if(tp->acb_head->cmd_done_status & 0xff)
                return (-1);

        if((err = smctr_issue_test_hic_cmd(dev)))
                return (err);

        if((err = smctr_wait_cmd(dev)))
                return (err);

        if(tp->acb_head->cmd_done_status & 0xff)
                return (-1);

        if((err = smctr_issue_test_mac_reg_cmd(dev)))
                return (err);

        if((err = smctr_wait_cmd(dev)))
                return (err);

        if(tp->acb_head->cmd_done_status & 0xff)
                return (-1);

        return (0);
}

/*
 * The typical workload of the driver: Handle the network interface interrupts.
 */
static irqreturn_t smctr_interrupt(int irq, void *dev_id)
{
        struct net_device *dev = dev_id;
        struct net_local *tp;
        int ioaddr;
        __u16 interrupt_unmask_bits = 0, interrupt_ack_code = 0xff00;
        __u16 err1, err = NOT_MY_INTERRUPT;
        __u8 isb_type, isb_subtype;
        __u16 isb_index;

        ioaddr = dev->base_addr;
        tp = netdev_priv(dev);

        if(tp->status == NOT_INITIALIZED)
                return IRQ_NONE;

        spin_lock(&tp->lock);
        
        smctr_disable_bic_int(dev);
        smctr_enable_16bit(dev);

        smctr_clear_int(dev);

        /* First read the LSB */
        while((tp->isb_ptr->IStatus[tp->current_isb_index].IType & 0xf0) == 0)
        {
                isb_index       = tp->current_isb_index;
                isb_type        = tp->isb_ptr->IStatus[isb_index].IType;
                isb_subtype     = tp->isb_ptr->IStatus[isb_index].ISubtype;

                (tp->current_isb_index)++;
                if(tp->current_isb_index == NUM_OF_INTERRUPTS)
                        tp->current_isb_index = 0;

                if(isb_type >= 0x10)
                {
                        smctr_disable_16bit(dev);
		        spin_unlock(&tp->lock);
                        return IRQ_HANDLED;
                }

                err = HARDWARE_FAILED;
                interrupt_ack_code = isb_index;
                tp->isb_ptr->IStatus[isb_index].IType |= 0xf0;

                interrupt_unmask_bits |= (1 << (__u16)isb_type);

                switch(isb_type)
                {
                        case ISB_IMC_MAC_TYPE_3:
                                smctr_disable_16bit(dev);

                                switch(isb_subtype)
                                {
                                        case 0:
                                                tp->monitor_state = MS_MONITOR_FSM_INACTIVE;
                                               break;

                                        case 1:
                                                tp->monitor_state = MS_REPEAT_BEACON_STATE;
                                                break;

                                        case 2:
                                                tp->monitor_state = MS_REPEAT_CLAIM_TOKEN_STATE;
                                                break;

                                        case 3:
                                                tp->monitor_state = MS_TRANSMIT_CLAIM_TOKEN_STATE;                                                break;

                                        case 4:
                                                tp->monitor_state = MS_STANDBY_MONITOR_STATE;
                                                break;

                                        case 5:
                                                tp->monitor_state = MS_TRANSMIT_BEACON_STATE;
                                                break;

                                        case 6:
                                                tp->monitor_state = MS_ACTIVE_MONITOR_STATE;
                                                break;

                                        case 7:
                                                tp->monitor_state = MS_TRANSMIT_RING_PURGE_STATE;
                                                break;

                                        case 8:   /* diagnostic state */
                                                break;

                                        case 9:
                                                tp->monitor_state = MS_BEACON_TEST_STATE;
                                                if(smctr_lobe_media_test(dev))
                                                {
                                                        tp->ring_status_flags = RING_STATUS_CHANGED;
                                                        tp->ring_status = AUTO_REMOVAL_ERROR;
                                                        smctr_ring_status_chg(dev);
                                                        smctr_bypass_state(dev);
                                                }
                                                else
                                                        smctr_issue_insert_cmd(dev);
                                                break;

                                        /* case 0x0a-0xff, illegal states */
                                        default:
                                                break;
                                }

                                tp->ring_status_flags = MONITOR_STATE_CHANGED;
                                err = smctr_ring_status_chg(dev);

                                smctr_enable_16bit(dev);
                                break;

                        /* Type 0x02 - MAC Error Counters Interrupt
                         * One or more MAC Error Counter is half full
                         *      MAC Error Counters
                         *      Lost_FR_Error_Counter
                         *      RCV_Congestion_Counter
                         *      FR_copied_Error_Counter
                         *      FREQ_Error_Counter
                         *      Token_Error_Counter
                         *      Line_Error_Counter
                         *      Internal_Error_Count
                         */
                        case ISB_IMC_MAC_ERROR_COUNTERS:
                                /* Read 802.5 Error Counters */
                                err = smctr_issue_read_ring_status_cmd(dev);
                                break;

                        /* Type 0x04 - MAC Type 2 Interrupt
                         * HOST needs to enqueue MAC Frame for transmission
                         * SubType Bit 15 - RQ_INIT_PDU( Request Initialization)                         * Changed from RQ_INIT_PDU to
                         * TRC_Status_Changed_Indicate
                         */
                        case ISB_IMC_MAC_TYPE_2:
                                err = smctr_issue_read_ring_status_cmd(dev);
                                break;


                        /* Type 0x05 - TX Frame Interrupt (FI). */
                        case ISB_IMC_TX_FRAME:
                                /* BUG QUEUE for TRC stuck receive BUG */
                                if(isb_subtype & TX_PENDING_PRIORITY_2)
                                {
                                        if((err = smctr_tx_complete(dev, BUG_QUEUE)) != SUCCESS)
                                                break;
                                }

                                /* NON-MAC frames only */
                                if(isb_subtype & TX_PENDING_PRIORITY_1)
                                {
                                        if((err = smctr_tx_complete(dev, NON_MAC_QUEUE)) != SUCCESS)
                                                break;
                                }

                                /* MAC frames only */
                                if(isb_subtype & TX_PENDING_PRIORITY_0)
                                        err = smctr_tx_complete(dev, MAC_QUEUE);                                break;

                        /* Type 0x06 - TX END OF QUEUE (FE) */
                        case ISB_IMC_END_OF_TX_QUEUE:
                                /* BUG queue */
                                if(isb_subtype & TX_PENDING_PRIORITY_2)
                                {
                                        /* ok to clear Receive FIFO overrun
                                         * imask send_BUG now completes.
                                         */
                                        interrupt_unmask_bits |= 0x800;

                                        tp->tx_queue_status[BUG_QUEUE] = NOT_TRANSMITING;
                                        if((err = smctr_tx_complete(dev, BUG_QUEUE)) != SUCCESS)
                                                break;
                                        if((err = smctr_restart_tx_chain(dev, BUG_QUEUE)) != SUCCESS)
                                                break;
                                }

                                /* NON-MAC queue only */
                                if(isb_subtype & TX_PENDING_PRIORITY_1)
                                {
                                        tp->tx_queue_status[NON_MAC_QUEUE] = NOT_TRANSMITING;
                                        if((err = smctr_tx_complete(dev, NON_MAC_QUEUE)) != SUCCESS)
                                                break;
                                        if((err = smctr_restart_tx_chain(dev, NON_MAC_QUEUE)) != SUCCESS)
                                                break;
                                }

                                /* MAC queue only */
                                if(isb_subtype & TX_PENDING_PRIORITY_0)
                                {
                                        tp->tx_queue_status[MAC_QUEUE] = NOT_TRANSMITING;
                                        if((err = smctr_tx_complete(dev, MAC_QUEUE)) != SUCCESS)
                                                break;

                                        err = smctr_restart_tx_chain(dev, MAC_QUEUE);
                                }
                                break;

                        /* Type 0x07 - NON-MAC RX Resource Interrupt
                         *   Subtype bit 12 - (BW) BDB warning
                         *   Subtype bit 13 - (FW) FCB warning
                         *   Subtype bit 14 - (BE) BDB End of chain
                         *   Subtype bit 15 - (FE) FCB End of chain
                         */
                        case ISB_IMC_NON_MAC_RX_RESOURCE:
                                tp->rx_fifo_overrun_count = 0;
                                tp->receive_queue_number = NON_MAC_QUEUE;
                                err1 = smctr_rx_frame(dev);

                                if(isb_subtype & NON_MAC_RX_RESOURCE_FE)
                                {
                                        if((err = smctr_issue_resume_rx_fcb_cmd(                                                dev, NON_MAC_QUEUE)) != SUCCESS)                                                break;

                                        if(tp->ptr_rx_fcb_overruns)
                                                (*tp->ptr_rx_fcb_overruns)++;
                                }

                                if(isb_subtype & NON_MAC_RX_RESOURCE_BE)
                                {
                                        if((err = smctr_issue_resume_rx_bdb_cmd(                                                dev, NON_MAC_QUEUE)) != SUCCESS)                                                break;

                                        if(tp->ptr_rx_bdb_overruns)
                                                (*tp->ptr_rx_bdb_overruns)++;
                                }
                                err = err1;
                                break;

                        /* Type 0x08 - MAC RX Resource Interrupt
                         *   Subtype bit 12 - (BW) BDB warning
                         *   Subtype bit 13 - (FW) FCB warning
                         *   Subtype bit 14 - (BE) BDB End of chain
                         *   Subtype bit 15 - (FE) FCB End of chain
                         */
                        case ISB_IMC_MAC_RX_RESOURCE:
                                tp->receive_queue_number = MAC_QUEUE;
                                err1 = smctr_rx_frame(dev);

                                if(isb_subtype & MAC_RX_RESOURCE_FE)
                                {
                                        if((err = smctr_issue_resume_rx_fcb_cmd(                                                dev, MAC_QUEUE)) != SUCCESS)
                                                break;

                                        if(tp->ptr_rx_fcb_overruns)
                                                (*tp->ptr_rx_fcb_overruns)++;
                                }

                                if(isb_subtype & MAC_RX_RESOURCE_BE)
                                {
                                        if((err = smctr_issue_resume_rx_bdb_cmd(                                                dev, MAC_QUEUE)) != SUCCESS)
                                                break;

                                        if(tp->ptr_rx_bdb_overruns)
                                                (*tp->ptr_rx_bdb_overruns)++;
                                }
                                err = err1;
                                break;

                        /* Type 0x09 - NON_MAC RX Frame Interrupt */
                        case ISB_IMC_NON_MAC_RX_FRAME:
                                tp->rx_fifo_overrun_count = 0;
                                tp->receive_queue_number = NON_MAC_QUEUE;
                                err = smctr_rx_frame(dev);
                                break;

                        /* Type 0x0A - MAC RX Frame Interrupt */
                        case ISB_IMC_MAC_RX_FRAME:
                                tp->receive_queue_number = MAC_QUEUE;
                                err = smctr_rx_frame(dev);
                                break;

                        /* Type 0x0B - TRC status
                         * TRC has encountered an error condition
                         * subtype bit 14 - transmit FIFO underrun
                         * subtype bit 15 - receive FIFO overrun
                         */
                        case ISB_IMC_TRC_FIFO_STATUS:
                                if(isb_subtype & TRC_FIFO_STATUS_TX_UNDERRUN)
                                {
                                        if(tp->ptr_tx_fifo_underruns)
                                                (*tp->ptr_tx_fifo_underruns)++;
                                }

                                if(isb_subtype & TRC_FIFO_STATUS_RX_OVERRUN)
                                {
                                        /* update overrun stuck receive counter
                                         * if >= 3, has to clear it by sending
                                         * back to back frames. We pick
                                         * DAT(duplicate address MAC frame)
                                         */
                                        tp->rx_fifo_overrun_count++;

                                        if(tp->rx_fifo_overrun_count >= 3)
                                        {
                                                tp->rx_fifo_overrun_count = 0;

                                                /* delay clearing fifo overrun
                                                 * imask till send_BUG tx
                                                 * complete posted
                                                 */
                                                interrupt_unmask_bits &= (~0x800);
                                                printk(KERN_CRIT "Jay please send bug\n");//                                              smctr_send_bug(dev);
                                        }

                                        if(tp->ptr_rx_fifo_overruns)
                                                (*tp->ptr_rx_fifo_overruns)++;
                                }

                                err = SUCCESS;
                                break;

                        /* Type 0x0C - Action Command Status Interrupt
                         * Subtype bit 14 - CB end of command chain (CE)
                         * Subtype bit 15 - CB command interrupt (CI)
                         */
                        case ISB_IMC_COMMAND_STATUS:
                                err = SUCCESS;
                                if(tp->acb_head->cmd == ACB_CMD_HIC_NOP)
                                {
                                        printk(KERN_ERR "i1\n");
                                        smctr_disable_16bit(dev);

                                        /* XXXXXXXXXXXXXXXXX */
                                /*      err = UM_Interrupt(dev); */

                                        smctr_enable_16bit(dev);
                                }
                                else
                                {
                                        if((tp->acb_head->cmd
                                                == ACB_CMD_READ_TRC_STATUS)
                                                && (tp->acb_head->subcmd
                                                == RW_TRC_STATUS_BLOCK))
                                        {
                                                if(tp->ptr_bcn_type)
                                                {
                                                        *(tp->ptr_bcn_type)
                                                                = (__u32)((SBlock *)tp->misc_command_data)->BCN_Type;
                                                }

                                                if(((SBlock *)tp->misc_command_data)->Status_CHG_Indicate & ERROR_COUNTERS_CHANGED)
                                                {
                                                        smctr_update_err_stats(dev);
                                                }

                                                if(((SBlock *)tp->misc_command_data)->Status_CHG_Indicate & TI_NDIS_RING_STATUS_CHANGED)
                                                {
                                                        tp->ring_status
                                                                = ((SBlock*)tp->misc_command_data)->TI_NDIS_Ring_Status;
                                                        smctr_disable_16bit(dev);
                                                        err = smctr_ring_status_chg(dev);
                                                        smctr_enable_16bit(dev);
                                                        if((tp->ring_status & REMOVE_RECEIVED)
                                                                && (tp->config_word0 & NO_AUTOREMOVE))
                                                        {
                                                                smctr_issue_remove_cmd(dev);
                                                        }

                                                        if(err != SUCCESS)
                                                        {
                                                                tp->acb_pending = 0;
                                                                break;
                                                        }
                                                }

                                                if(((SBlock *)tp->misc_command_data)->Status_CHG_Indicate & UNA_CHANGED)
                                                {
                                                        if(tp->ptr_una)
                                                        {
                                                                tp->ptr_una[0] = SWAP_BYTES(((SBlock *)tp->misc_command_data)->UNA[0]);
                                                                tp->ptr_una[1] = SWAP_BYTES(((SBlock *)tp->misc_command_data)->UNA[1]);
                                                                tp->ptr_una[2] = SWAP_BYTES(((SBlock *)tp->misc_command_data)->UNA[2]);
                                                        }

                                                }

                                                if(((SBlock *)tp->misc_command_data)->Status_CHG_Indicate & READY_TO_SEND_RQ_INIT)                                                {
                                                        err = smctr_send_rq_init(dev);
                                                }
                                        }
                                }

                                tp->acb_pending = 0;
                                break;

                        /* Type 0x0D - MAC Type 1 interrupt
                         * Subtype -- 00 FR_BCN received at S12
                         *            01 FR_BCN received at S21
                         *            02 FR_DAT(DA=MA, A<>0) received at S21
                         *            03 TSM_EXP at S21
                         *            04 FR_REMOVE received at S42
                         *            05 TBR_EXP, BR_FLAG_SET at S42
                         *            06 TBT_EXP at S53
                         */
                        case ISB_IMC_MAC_TYPE_1:
                                if(isb_subtype > 8)
                                {
                                        err = HARDWARE_FAILED;
                                        break;
                                }

                                err = SUCCESS;
                                switch(isb_subtype)
                                {
                                        case 0:
                                                tp->join_state = JS_BYPASS_STATE;
                                                if(tp->status != CLOSED)
                                                {
                                                        tp->status = CLOSED;
                                                        err = smctr_status_chg(dev);
                                                }
                                                break;

                                        case 1:
                                                tp->join_state = JS_LOBE_TEST_STATE;
                                                break;

                                        case 2:
                                                tp->join_state = JS_DETECT_MONITOR_PRESENT_STATE;
                                                break;

                                        case 3:
                                                tp->join_state = JS_AWAIT_NEW_MONITOR_STATE;
                                                break;

                                        case 4:
                                                tp->join_state = JS_DUPLICATE_ADDRESS_TEST_STATE;
                                                break;

                                        case 5:
                                                tp->join_state = JS_NEIGHBOR_NOTIFICATION_STATE;
                                                break;

                                        case 6:
                                                tp->join_state = JS_REQUEST_INITIALIZATION_STATE;
                                                break;

                                        case 7:
                                                tp->join_state = JS_JOIN_COMPLETE_STATE;
                                                tp->status = OPEN;
                                                err = smctr_status_chg(dev);
                                                break;

                                        case 8:
                                                tp->join_state = JS_BYPASS_WAIT_STATE;
                                                break;
                                }
                                break ;

                        /* Type 0x0E - TRC Initialization Sequence Interrupt
                         * Subtype -- 00-FF Initializatin sequence complete
                         */
                        case ISB_IMC_TRC_INTRNL_TST_STATUS:
                                tp->status = INITIALIZED;
                                smctr_disable_16bit(dev);
                                err = smctr_status_chg(dev);
                                smctr_enable_16bit(dev);
                                break;

                        /* other interrupt types, illegal */
                        default:
                                break;
                }

                if(err != SUCCESS)
                        break;
        }

        /* Checking the ack code instead of the unmask bits here is because :
         * while fixing the stuck receive, DAT frame are sent and mask off
         * FIFO overrun interrupt temporarily (interrupt_unmask_bits = 0)
         * but we still want to issue ack to ISB
         */
        if(!(interrupt_ack_code & 0xff00))
                smctr_issue_int_ack(dev, interrupt_ack_code, interrupt_unmask_bits);

        smctr_disable_16bit(dev);
        smctr_enable_bic_int(dev);
        spin_unlock(&tp->lock);

        return IRQ_HANDLED;
}

static int smctr_issue_enable_int_cmd(struct net_device *dev,
        __u16 interrupt_enable_mask)
{
        struct net_local *tp = netdev_priv(dev);
        int err;

        if((err = smctr_wait_while_cbusy(dev)))
                return (err);

        tp->sclb_ptr->int_mask_control  = interrupt_enable_mask;
        tp->sclb_ptr->valid_command     = SCLB_VALID | SCLB_CMD_CLEAR_INTERRUPT_MASK;

        smctr_set_ctrl_attention(dev);

        return (0);
}

static int smctr_issue_int_ack(struct net_device *dev, __u16 iack_code, __u16 ibits)
{
        struct net_local *tp = netdev_priv(dev);

        if(smctr_wait_while_cbusy(dev))
                return (-1);

        tp->sclb_ptr->int_mask_control = ibits;
        tp->sclb_ptr->iack_code = iack_code << 1; /* use the offset from base */        tp->sclb_ptr->resume_control = 0;
        tp->sclb_ptr->valid_command = SCLB_VALID | SCLB_IACK_CODE_VALID | SCLB_CMD_CLEAR_INTERRUPT_MASK;

        smctr_set_ctrl_attention(dev);

        return (0);
}

static int smctr_issue_init_timers_cmd(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        unsigned int i;
        int err;
        __u16 *pTimer_Struc = (__u16 *)tp->misc_command_data;

        if((err = smctr_wait_while_cbusy(dev)))
                return (err);

        if((err = smctr_wait_cmd(dev)))
                return (err);

        tp->config_word0 = THDREN | DMA_TRIGGER | USETPT | NO_AUTOREMOVE;
        tp->config_word1 = 0;

        if((tp->media_type == MEDIA_STP_16)
                || (tp->media_type == MEDIA_UTP_16)
                || (tp->media_type == MEDIA_STP_16_UTP_16))
        {
                tp->config_word0 |= FREQ_16MB_BIT;
        }

        if(tp->mode_bits & EARLY_TOKEN_REL)
                tp->config_word0 |= ETREN;

        if(tp->mode_bits & LOOPING_MODE_MASK)
                tp->config_word0 |= RX_OWN_BIT;
        else
                tp->config_word0 &= ~RX_OWN_BIT;

        if(tp->receive_mask & PROMISCUOUS_MODE)
                tp->config_word0 |= PROMISCUOUS_BIT;
        else
                tp->config_word0 &= ~PROMISCUOUS_BIT;

        if(tp->receive_mask & ACCEPT_ERR_PACKETS)
                tp->config_word0 |= SAVBAD_BIT;
        else
                tp->config_word0 &= ~SAVBAD_BIT;

        if(tp->receive_mask & ACCEPT_ATT_MAC_FRAMES)
                tp->config_word0 |= RXATMAC;
        else
                tp->config_word0 &= ~RXATMAC;

        if(tp->receive_mask & ACCEPT_MULTI_PROM)
                tp->config_word1 |= MULTICAST_ADDRESS_BIT;
        else
                tp->config_word1 &= ~MULTICAST_ADDRESS_BIT;

        if(tp->receive_mask & ACCEPT_SOURCE_ROUTING_SPANNING)
                tp->config_word1 |= SOURCE_ROUTING_SPANNING_BITS;
        else
        {
                if(tp->receive_mask & ACCEPT_SOURCE_ROUTING)
                        tp->config_word1 |= SOURCE_ROUTING_EXPLORER_BIT;
                else
                        tp->config_word1 &= ~SOURCE_ROUTING_SPANNING_BITS;
        }

        if((tp->media_type == MEDIA_STP_16)
                || (tp->media_type == MEDIA_UTP_16)
                || (tp->media_type == MEDIA_STP_16_UTP_16))
        {
                tp->config_word1 |= INTERFRAME_SPACING_16;
        }
        else
                tp->config_word1 |= INTERFRAME_SPACING_4;

        *pTimer_Struc++ = tp->config_word0;
        *pTimer_Struc++ = tp->config_word1;

        if((tp->media_type == MEDIA_STP_4)
                || (tp->media_type == MEDIA_UTP_4)
                || (tp->media_type == MEDIA_STP_4_UTP_4))
        {
                *pTimer_Struc++ = 0x00FA;       /* prescale */
                *pTimer_Struc++ = 0x2710;       /* TPT_limit */
                *pTimer_Struc++ = 0x2710;       /* TQP_limit */
                *pTimer_Struc++ = 0x0A28;       /* TNT_limit */
                *pTimer_Struc++ = 0x3E80;       /* TBT_limit */
                *pTimer_Struc++ = 0x3A98;       /* TSM_limit */
                *pTimer_Struc++ = 0x1B58;       /* TAM_limit */
                *pTimer_Struc++ = 0x00C8;       /* TBR_limit */
                *pTimer_Struc++ = 0x07D0;       /* TER_limit */
                *pTimer_Struc++ = 0x000A;       /* TGT_limit */
                *pTimer_Struc++ = 0x1162;       /* THT_limit */
                *pTimer_Struc++ = 0x07D0;       /* TRR_limit */
                *pTimer_Struc++ = 0x1388;       /* TVX_limit */
                *pTimer_Struc++ = 0x0000;       /* reserved */
        }
        else
        {
                *pTimer_Struc++ = 0x03E8;       /* prescale */
                *pTimer_Struc++ = 0x9C40;       /* TPT_limit */
                *pTimer_Struc++ = 0x9C40;       /* TQP_limit */
                *pTimer_Struc++ = 0x0A28;       /* TNT_limit */
                *pTimer_Struc++ = 0x3E80;       /* TBT_limit */
                *pTimer_Struc++ = 0x3A98;       /* TSM_limit */
                *pTimer_Struc++ = 0x1B58;       /* TAM_limit */
                *pTimer_Struc++ = 0x00C8;       /* TBR_limit */
                *pTimer_Struc++ = 0x07D0;       /* TER_limit */
                *pTimer_Struc++ = 0x000A;       /* TGT_limit */
                *pTimer_Struc++ = 0x4588;       /* THT_limit */
                *pTimer_Struc++ = 0x1F40;       /* TRR_limit */
                *pTimer_Struc++ = 0x4E20;       /* TVX_limit */
                *pTimer_Struc++ = 0x0000;       /* reserved */
        }

        /* Set node address. */
        *pTimer_Struc++ = dev->dev_addr[0] << 8
                | (dev->dev_addr[1] & 0xFF);
        *pTimer_Struc++ = dev->dev_addr[2] << 8
                | (dev->dev_addr[3] & 0xFF);
        *pTimer_Struc++ = dev->dev_addr[4] << 8
                | (dev->dev_addr[5] & 0xFF);

        /* Set group address. */
        *pTimer_Struc++ = tp->group_address_0 << 8
                | tp->group_address_0 >> 8;
        *pTimer_Struc++ = tp->group_address[0] << 8
                | tp->group_address[0] >> 8;
        *pTimer_Struc++ = tp->group_address[1] << 8
                | tp->group_address[1] >> 8;

        /* Set functional address. */
        *pTimer_Struc++ = tp->functional_address_0 << 8
                | tp->functional_address_0 >> 8;
        *pTimer_Struc++ = tp->functional_address[0] << 8
                | tp->functional_address[0] >> 8;
        *pTimer_Struc++ = tp->functional_address[1] << 8
                | tp->functional_address[1] >> 8;

        /* Set Bit-Wise group address. */
        *pTimer_Struc++ = tp->bitwise_group_address[0] << 8
                | tp->bitwise_group_address[0] >> 8;
        *pTimer_Struc++ = tp->bitwise_group_address[1] << 8
                | tp->bitwise_group_address[1] >> 8;

        /* Set ring number address. */
        *pTimer_Struc++ = tp->source_ring_number;
        *pTimer_Struc++ = tp->target_ring_number;

        /* Physical drop number. */
        *pTimer_Struc++ = (unsigned short)0;
        *pTimer_Struc++ = (unsigned short)0;

        /* Product instance ID. */
        for(i = 0; i < 9; i++)
                *pTimer_Struc++ = (unsigned short)0;

        err = smctr_setup_single_cmd_w_data(dev, ACB_CMD_INIT_TRC_TIMERS, 0);

        return (err);
}

static int smctr_issue_init_txrx_cmd(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        unsigned int i;
        int err;
        void **txrx_ptrs = (void *)tp->misc_command_data;

        if((err = smctr_wait_while_cbusy(dev)))
                return (err);

        if((err = smctr_wait_cmd(dev)))
	{
                printk(KERN_ERR "%s: Hardware failure\n", dev->name);
                return (err);
        }

        /* Initialize Transmit Queue Pointers that are used, to point to
         * a single FCB.
         */
        for(i = 0; i < NUM_TX_QS_USED; i++)
                *txrx_ptrs++ = (void *)TRC_POINTER(tp->tx_fcb_head[i]);

        /* Initialize Transmit Queue Pointers that are NOT used to ZERO. */
        for(; i < MAX_TX_QS; i++)
                *txrx_ptrs++ = (void *)0;

        /* Initialize Receive Queue Pointers (MAC and Non-MAC) that are
         * used, to point to a single FCB and a BDB chain of buffers.
         */
        for(i = 0; i < NUM_RX_QS_USED; i++)
        {
                *txrx_ptrs++ = (void *)TRC_POINTER(tp->rx_fcb_head[i]);
                *txrx_ptrs++ = (void *)TRC_POINTER(tp->rx_bdb_head[i]);
        }

        /* Initialize Receive Queue Pointers that are NOT used to ZERO. */
        for(; i < MAX_RX_QS; i++)
        {
                *txrx_ptrs++ = (void *)0;
                *txrx_ptrs++ = (void *)0;
        }

        err = smctr_setup_single_cmd_w_data(dev, ACB_CMD_INIT_TX_RX, 0);

        return (err);
}

static int smctr_issue_insert_cmd(struct net_device *dev)
{
        int err;

        err = smctr_setup_single_cmd(dev, ACB_CMD_INSERT, ACB_SUB_CMD_NOP);

        return (err);
}

static int smctr_issue_read_ring_status_cmd(struct net_device *dev)
{
        int err;

        if((err = smctr_wait_while_cbusy(dev)))
                return (err);

        if((err = smctr_wait_cmd(dev)))
                return (err);

        err = smctr_setup_single_cmd_w_data(dev, ACB_CMD_READ_TRC_STATUS,
                RW_TRC_STATUS_BLOCK);

        return (err);
}

static int smctr_issue_read_word_cmd(struct net_device *dev, __u16 aword_cnt)
{
        int err;

        if((err = smctr_wait_while_cbusy(dev)))
                return (err);

        if((err = smctr_wait_cmd(dev)))
                return (err);

        err = smctr_setup_single_cmd_w_data(dev, ACB_CMD_MCT_READ_VALUE,
                aword_cnt);

        return (err);
}

static int smctr_issue_remove_cmd(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        int err;

        if((err = smctr_wait_while_cbusy(dev)))
                return (err);

        tp->sclb_ptr->resume_control    = 0;
        tp->sclb_ptr->valid_command     = SCLB_VALID | SCLB_CMD_REMOVE;

        smctr_set_ctrl_attention(dev);

        return (0);
}

static int smctr_issue_resume_acb_cmd(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        int err;

        if((err = smctr_wait_while_cbusy(dev)))
                return (err);

        tp->sclb_ptr->resume_control = SCLB_RC_ACB;
        tp->sclb_ptr->valid_command  = SCLB_VALID | SCLB_RESUME_CONTROL_VALID;

        tp->acb_pending = 1;

        smctr_set_ctrl_attention(dev);

        return (0);
}

static int smctr_issue_resume_rx_bdb_cmd(struct net_device *dev, __u16 queue)
{
        struct net_local *tp = netdev_priv(dev);
        int err;

        if((err = smctr_wait_while_cbusy(dev)))
                return (err);

        if(queue == MAC_QUEUE)
                tp->sclb_ptr->resume_control = SCLB_RC_RX_MAC_BDB;
        else
                tp->sclb_ptr->resume_control = SCLB_RC_RX_NON_MAC_BDB;

        tp->sclb_ptr->valid_command = SCLB_VALID | SCLB_RESUME_CONTROL_VALID;

        smctr_set_ctrl_attention(dev);

        return (0);
}

static int smctr_issue_resume_rx_fcb_cmd(struct net_device *dev, __u16 queue)
{
        struct net_local *tp = netdev_priv(dev);

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_issue_resume_rx_fcb_cmd\n", dev->name);

        if(smctr_wait_while_cbusy(dev))
                return (-1);

        if(queue == MAC_QUEUE)
                tp->sclb_ptr->resume_control = SCLB_RC_RX_MAC_FCB;
        else
                tp->sclb_ptr->resume_control = SCLB_RC_RX_NON_MAC_FCB;

        tp->sclb_ptr->valid_command = SCLB_VALID | SCLB_RESUME_CONTROL_VALID;

        smctr_set_ctrl_attention(dev);

        return (0);
}

static int smctr_issue_resume_tx_fcb_cmd(struct net_device *dev, __u16 queue)
{
        struct net_local *tp = netdev_priv(dev);

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_issue_resume_tx_fcb_cmd\n", dev->name);

        if(smctr_wait_while_cbusy(dev))
                return (-1);

        tp->sclb_ptr->resume_control = (SCLB_RC_TFCB0 << queue);
        tp->sclb_ptr->valid_command = SCLB_RESUME_CONTROL_VALID | SCLB_VALID;

        smctr_set_ctrl_attention(dev);

        return (0);
}

static int smctr_issue_test_internal_rom_cmd(struct net_device *dev)
{
        int err;

        err = smctr_setup_single_cmd(dev, ACB_CMD_MCT_TEST,
                TRC_INTERNAL_ROM_TEST);

        return (err);
}

static int smctr_issue_test_hic_cmd(struct net_device *dev)
{
        int err;

        err = smctr_setup_single_cmd(dev, ACB_CMD_HIC_TEST,
                TRC_HOST_INTERFACE_REG_TEST);

        return (err);
}

static int smctr_issue_test_mac_reg_cmd(struct net_device *dev)
{
        int err;

        err = smctr_setup_single_cmd(dev, ACB_CMD_MCT_TEST,
                TRC_MAC_REGISTERS_TEST);

        return (err);
}

static int smctr_issue_trc_loopback_cmd(struct net_device *dev)
{
        int err;

        err = smctr_setup_single_cmd(dev, ACB_CMD_MCT_TEST,
                TRC_INTERNAL_LOOPBACK);

        return (err);
}

static int smctr_issue_tri_loopback_cmd(struct net_device *dev)
{
        int err;

        err = smctr_setup_single_cmd(dev, ACB_CMD_MCT_TEST,
                TRC_TRI_LOOPBACK);

        return (err);
}

static int smctr_issue_write_byte_cmd(struct net_device *dev,
        short aword_cnt, void *byte)
{
	struct net_local *tp = netdev_priv(dev);
        unsigned int iword, ibyte;
	int err;

        if((err = smctr_wait_while_cbusy(dev)))
                return (err);

        if((err = smctr_wait_cmd(dev)))
                return (err);

        for(iword = 0, ibyte*
 *; iword < (unsigned int)(aetwo_cnt & 0xff);
 ters.
 	netwo++  smctr.+= 2)pters.
 *{pters.
 *amba.orgtp->misc_command_data[netwo] = (*((__u8 *)n by J smctr) << 8)
			|sed and distributed acco + 1)dapters.
 *}
pters.
 *return (smctr_setup_single_cmd_wtware(dev, ACB_CMD_MCT_WRITE_VALUE, 
		SMC Tokenl Pu}

staticr th hereinissue_write_MC Tokmd(struct net_device *is dpters.
 *short SMC Token, void *etwo)
jschlst@sa (8115T, chlocal *tp =T, cdev_privhis dapters.
 *river for th i, err;nse, incorif((err =     - wait_whince.busy  	- )Schulist <e, incorporated err)ntainer(s):
 *    JS        Jay te  chlst@samba.org>
 *
 * Changes:
 *    07102000for(i.c: A nrk driver for the SMC Token Ring Ada i++@samba.org>
 *
 * 
 *
 *  This software me usd and 16 *)etwor+ i    07102000   JS        by reference.
 *
 *  This device driver works with tsamba.org>
 *
 * ollowing Sntainer(s):Changes:
 *   adapters:
 *      - joinThisplete_ptere  (8115T, chips 825/584 (8115T/A, c *
 Maintainer(s):g a card with
 *				module ihis device driCHANGE_JOIN_STAT *    1. Multicast JS<linuxCOMPLETEx/fcnt *
 *  Initial 2.5 cleanup Alan Cox <alan@lxorlink_tx_fcbs_to_bdbs002/10/28
 */

#include <linux/moduhips 825/594)
 *
 *  Source(s):
 *  	- SMC TokenCard SDK.
 *
 *  japters.
 *FCBlock *fcbapters.
 *BDnux/inibdb;
 *                      NUM_TX_QS_USEDscriptive errorjschlst@samba.org>fcb = *   ude <l_head[i]apters.
 *inux/skbbdinux/etherdbdice.h>
#incvice.h>
#include <or(j.c: A j < *   numlude <li
#in jiptive error msgs.
 jschlst@samba.org>include <li->e <lptrm.h>
#includ=uff.nclude <linux/skbulude <asm/io.trcde <linclude <as= TRC_POINTER(bdbdapters.
 *ice.h>
#include <linux(linux/ini)((char *)<lin+ sizeof
#endif
l Public Liclude <linux/skbuff.h>
( <linux/minclude "sff.h.h"       <linux  /* Our Stuff */

s}ublic License, incorporated 0clude <linux/ioport.h>
#oad_firmwar2002/10/28
 */

#include <linux/moduhips 825/594)
 *
 *  Source(s):
 *  	- SM	constdef CONFmctr";

 *fwapters.
 * Fixedi, checksum.c: Ainux/module.h>
#/* SMCainer(s):
 *hereindebug > 10@samba.org>
 *
 * printk(KERN_DEBUG "%s:dname[] = "smctr";

\n", dev->name *
 	if (requestsmctr";


&fw, "witherei.bi Use& 0 fochlst {
		;
static chaERRctr_mo0x6ec6;
#not found* Use 0 for produ		porated UCODE_NOT_PRESENTBUG ense, incor>
#incluofludebuffs
#error4MC Name of
 *
 ode_bit smctre arr|= UMACprototypes andreceive_mask 64
#errorSMC Name of
 *
 ax_packet_"   smctr.c 177r. */
stati/* Can only up = " thendef SMCTRonce per adapter reset. */ainer(s):
  (
 *
 *crocfuncversion != 0ery ve Adapt
staticigned int s	goto outt smctr_debug /* Verify_shared_memoryexists and is_share in_sharright amounev);

/* B */
stat!fw->ware* Our Stuff */

s|e ternet_devi + staticVERSION_OFFSET) <_mask(struct nt@samba.orgjschlst@samba.org> net_device * unsigned int s */
static blic License, incor/*_mask(sSIZE *de_DEBincludfor t Cpeed;

v);

/* B */
                   Fixed pmctr_chg_rx_mask(sc inet_deviciscr Jay Schulist < struct speed;

/+      Fixed pmctr_chg_rx_2em widapters.
 *statspeed;

struct net_device *);
static int smctr_clear_int smctr_checkAt this po *
 we have a validndef SMCTRimage, lets kick it on upv);

/* B */
hereinenable_et_devi_ram  	- SMC TokenCE */
static i16bitable_16bit(struct netset_pagehis deand dist 
 * am_access    07102000    hereinspeed;

smctr";


chlstce *dev);
static int smctr_chg_rx_mask(struct net_devic		 const struct f> *    0 smctr_bypass_sint smctr_clear_int(struct neE */
static int smctrctrl_sto_adaptedevice.h>
#include /* Zerstati ram space/* Dndef SMCTv);

/* B */
dev);

/* D */
static iCS_RAMuct n *dev,
				 const struct fdev);
st;
static intre(struct net_le_16bapter. */
statictr_get_boardidectr_benable_adapt, fw mca);
static int smic int smctr_bypass_st    ctr_chg_rx_mask(struct net_devic;evice *dev);
static unsigned int smctr_get_nunctionatruct net_devicruct net_device *dev);
sta=atic int smctr_bypass_sting t/* Our Stuff */

sice *dev, short queue);
static int sCHECKSUMt_station_id(struct net_device *dev)~;
static FCBlock *smctr_get_neradevice.h>
#include e *dev)isstruct net_device *dev, int mca);
static int smc char smnt smctr_enable_adapter_ram(struct net_dtruct net_deviHARDWARE_FAILEDr_trc_reset(truct netlseear_int(struct net_devivice *dev);

r. */
statistatic int smc *dev);
statiout:
	releasatic int smet_phlinux/ptrace.h>
#include <linux/ioport.h>
#= "snfuncaddr002/10/28
 */

#include <linux/module.hio *de =e 0 fobruct *deSMC TokenCard SDK.
 *
 *if

static in8 intainer(s):               6ude <linux/netdevice.h>
#include _devinb(t net_d+ LAR0em withuff */

static c* 3 for ;
sta  JS  clude)atic int sme *dev);
s 0 fo *de_le*dev6*
 *  Initial 2.5 cic cons/* Lobe Media Test.
 * During_shartransmisss_stof_sharinitial 1500 lrqremurn_tMAC frames,r_insharphase ux/inlooptatic in805 chip mayt_dev,vice then un-terrupcauferecmd(stru825 to goule.o a PURGE k>  2. Wble_performrupttruct n,(struMCTr_ine *dev);
 willmctr_ irq, vt anyble_int givenue_iit birmwarhostupt_er_in
statmctr1 fontlytaticc intimeouctr_ir_inNOTE 1: Iv_id);monitoruk>  2 *deMS_BEACON_TESTx/fcntl allint smctrr_inqueues ov);
 thatic inone used(strusharmctr_issue_ter_pohould ber_in int smd.!?rx_cmd(struc2 net_device *dev);
static int smctr_issue_insept_enabl  for clearitr_inhaevicy multi-cast or pro *  ous tionsset,
     for clearityneeds tor_inbe changedue_icleasue_ret smctr_issue_remove_cmd( fun(stru,
    ad_ritus_r_inrunupt_enable_ __u16 awor arityset backs_cmd(s originalnt sue net(int e_resumis su net_fultr_i/it_rx_bdbs(struct ne_ring_status_>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/errno.h>perrodapter.C TokenCard SDK.
- SMC saved_rcvearitr. */
static char smctr_name[] = "SMC TokenCard";
static char *smctr_model = "U_ring_status_* Use 0 for produc */
static i_acb_t net_device strumctr_ssuee *dev);

/* E_device *dev)device *dev) 
 * for clearitphabeticly 
 * for clearity, maintainabvice *dev);
stachg_rxearit int mca);
stati/* Sby rue_read_r_issue_e_trc_loopback_cmdt smctr_issue_test_mxed a timit(struct netatic i  Fixed a timi smctr_cMaintainer(s):/* Txt net_device *deble_intice *dev);

/* D */
static iint ; ++iSchulist <jschlst@samba.org>mctr_issusendev, __u16 queue);apter_ram(struct net_dasm/system.h>
#include <asif(sue_teto th(struct netStuff */

static cov);
static int smctr_init_tr_link_tx_fcbs_to_bdbs(struct netkenCard";ue_test_1nt smctr_load_firmware(strate(struct net_deord_cnt, void *byte);
stdev);

static int smctr_load_firmware(stre *dev);
s7/12/00 by jschlst@samba.org\ct net_device da/
static int smcomplete_state(struct net_device ate(structint smctr_lobe_media_t(int ioaddr); net_ netissue_init_ for cld derrupt(_trc_loopback_cm
 *  
 * rdeviccurr[MAC_QUEUE]->le_inuk>  usr_ram(struct net_devictic int smctr_maNON_ke_access_pri(struct net_e_8025_hdr(stru_write_byte_cmd;
static int sto "Pemove_"
statv);

/* B */
 
 * for clearity=et_device *dev);
static in;
static int smctr_issue_	ial 2.5 0;
err:
	ort awe *deint smcted int s
 *
ct net = CLOnclueal(ratedLOBE_MEDIA_issuestruct ninit_rx_bdbs(struct ned_cnt, void *byte)
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/time.h>
#includele.h>
#include <lit smctr_issue_test_hic_cmd(struct net_device *dev);
static int smctr_issue_test_mxed ac_reg_cmd(struct net_device *ssue_TOR        short awot_devicu16 ac_fc);
st and f *dev);
stat!= int smctr_issue_insestatic int __init smctr_get_boardir_make_funct_addk>  20 	- SMC TokenCic int smctr_issue_write_word_cm int smctr_link_tx_fcbs_to_bdbs(struct netrbose debug.
 */
irqreFail_rest smctr_m\n"mctr_init_tx_fcbs(s*  Initial 2.5 cTOR *tsv, __u16 correltr.c: v1.4 7/12/00 by jschlst@samba.org\nux/kernel.h>
#include <linux/types.h>
#inver issu*    1. Multicast  FIXTOR *tsv, __u16de <linux/ptrace.h>
#include <linux/ioport.h>
#d(struct net_device *02/10/28
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/TOR *VECTOR *tsv*
 *  Initial 2.5 cleanup Alan Cox <alan@lxormake_8025_hdev);
static int smctr_in*    1. Muke_aHEADER *rmf,
/* P */
strutt ne Fixedac_fc (8115T/A, ctmf->ac ev, B(nt unier(struct net_dev*devmsbsumetr_get_controlnt smctr_makent _finitLsmctr_probe1(struct net_devicel*dev, le_inaddr);
stati;
static int _sa[0e ustruct net_devi0include <lice *dev,1__u16 rx_status);
1/* R */
static int 2__u16 rx_status);
2/* R */
static int 3__u16 rx_status);
3/* R */
static int 4__u16 rx_status);
4/* R */
static int 5__u16 rx_status);
5rdevice.h>
#switch(ctr_pvcSchulist <js		te_cmnd RQ_INITTOR RPSvice *dev);
static ic netu16 *co:t smctr_load_firmware(strctr_pdv, __u10xcinternal_ro net_device *dev, MAC_HEsmctr0x0f);
static int smctr_rcv_rq_addr_st *devttch(struct net_device *dev,
        Mstructtch(struct net_device *dev,
        M,
   ttch(struct net_device *dev,
        Mt smc0x02nt smctr_load_firmware(strbrea);
s        __uPTay.hFORWARDTOR CRor);
static int smctr_rcv_t*dev);
staticard(struct net_device *dev, MAC_HEADER *rmf);
static int smctr_rcv_rq_addr_state_attch(struct net_device *dev,
        MAC_HEADER *rmf, __u16 *correlator);
static int smctr_rcv_unknown(struct net_device *dev, MAC_HEADER *rmf,
        __u16 *correlator);
st1f);
static int smctr_rcv_rq(struct net_dEverythrupttati goe int viceervice *dev);
static idefaultard(struct net_device *dev, MAC_HEADER re *dev, _nt smctr_load_firmware(str_addr_state_adev, MACst(struct net_device *dev,
        MAC_HEdev, MACsmctr_rcv_ch16 *correlator);
static int sdev, MACdev, MAC_HEAwn(struct net_device *dev, MAdev, MACrrelator);
s
        __u16 *correlator);
dev, MACructt net_device *dev);
static int  jschlst@samba.org\n";
static const char cardname[]c intt net_):
   (8115T, chips 825/584)
/* PSUB_VECTO*smcsNT   20

#ifdef CONFIG_MCA_LEGACY
static unsigned int  R */
statsv->sv    AUTHORIZED_ACCESS_PRIORITY* R */
statstaticl = S_ smctr_send_rq_init(struct nnet_device *dev)v, __u1 smc
 *
authorized_fstatus);
oritytatic int set_devicesmctrs_rxC_HEADER *rmf,
        __u16 *tx_C_SUB_VEC    MAC_HEADER *rmf, __u16 tx_flf_tmo   (8115T, chips 825/584)p(struct net_device *dev, MAC_HE
static int DDR_iniMODIFER net_device *dev);
stati_SUB_VECTOR *rsv(struct net_device *devnability and
static int sdevice *dev)ice *dev,
        MAC_SUB_VECTOR *rsv)uth_funct_clash>
#include <linux/slab.e *dev);

/* Puct net_device *dev, MAC_HEADER *rmf,
        __u16 rcode, __u16 correlator);
static int smctr_sendFUNCTt neCLASS net_device *dev);
static int smct;
static int smc(struct net_device *dev, MAC_HEADER *rmf,
 net_iondevicee_devfstatus);
static int smctr_set_auth_accesd short smctr_set_cevice *dev,
        MAC_HEADER *rmf, __u16 tx_coropen_tr(struct net_device *dev);

/* Puct net_device _probe(ic inelat *device *dev,
        MACORRELATOrsv);
static int smctr_s_device *dev(struct net_device *dev, MAint smctr_s_ctrl_attention(struct net_deed(struct neevice *dev,
        MAC_HEADER *rmf, __u16 tx_ net_d *dev);
static int smctr_insp(struct net_device *dev, MAC_HEADER *rmf,
        __u16 rcode, __u16 correlator);ort awgettatic voialtr_seessu16 correlator);
static int;
staticALset_corr net_device *dev);
statdata(struct net_devi(struct net_device *dev, MAC_HE *  This software 0]t_ctrl_attention(struct net_deviclloc(struct net_device ctrl_attention(str *devsmctr_malloc(struct net_de1ice *dev, __u16 size)structic int smctr_status_chg(seout(evice *dev,
        MAC_HEADER *rmf, __u16 tx_grouptr_set_trc_reset(int ioaddr);
static int smctr_setup_single_cmd(struct net_device *dev,
        __u16 command, __u16 subc     __u16 int smctr_setup_single_cmd_w_GROUPt net_device *dev,
        __u1ct sk_buff *skbcommand);
static char *smctr_malloc(struct net_device *dev, __u16 size);
static int smctr_status_chg(struct net_device *dev);

/* T */
static void smctr_timeout(struct net_device *dev);
static int smctr_trc_send_packet(strue *dev,G     Auct ne Sub-vectssueot_cmdzeros netsmctrthic int smct*);

/* W */
st/Fmmand);
s W */
staIndicctr_sume_ectr_ice *dev)_product_id(stt_device *de;
st80 &&v,
	__u16 *corMAC_HEpters.
 *
((X + 0xff)  *deMAC_HE(((X + 0xff) struMAC_HEtive error msgs.
 *dev, MAC_SUB_Vtch(sevice *dev,
        MAC_HEADER *rmf, __u16 tx_phy_drop_num *dev,
        MAC_SUB_VECTOR *rsv);
static int smctr_set_frame_forward(struct net_device *dev,
        MAC_SUB_VE__u16 subcphysical* Allocatbct net_derelator);
static intPHYSICAL_DROP net_device *dev);
stat*
 *       0. Icommand);
static char *smctr_malloc(struct net_device *dev, __u16 size);
static int smctr_status_chg(struct net_device *dev);

/* T */
static void smctr_timeout(struct net_device *dev);
static int smctr_trc_send_packet(struct net_device *dev, FCBlock *fcb,
   product_iic int smctr_set_auth_funct_class(struct net_device *dev,shared_memory allocation:
 *RODUCT_INSTANCE_It net_devicystem ConfigurC RX BDB'S
 *      1id *word);

/* J */
stati8scriptive error msgs.
 *t_device JS  0xF(X)    smctr_malloc(dev, TO_PARAGRAPH_BOUNDRY(X))pterson. MAC RX FCB'S
 *      10. NON-MAC RX FCB'S
 *      11. MACfunction MUST be mirrored in the
 * function "get_num_rx_bdbata Buffer
er of memory allocation:
 /fcnt neIDENTR *rsv);
static int smctr_s          printk(KEcommand);
static char *smctr_malloc(struct net_device *dev, __u16 size);
static int smctr_status_chg(struct net_device *dev);

/* T */
static void smctr_timeout(struct net_device *dev);
static int smctr_trc_send_packet(struer)
 *
 *,
   smctr_malloc(struct net_de2ice *dev, __u16 size)t smctic int smctr_status_chg(sOCK_Sevice *dev,
        MAC_HEADER *rmf, __u16 tx_rrup;

      ct nett smctr_set_page(struct net_device *dev, __u8 *bu * 0set_phy_drop(struct netRINGalloc_sha/fcnUevice *dev,
        __u1SCLBlock *)smctr_mallt_device *dev, MAC_SUB_VECTOR *rsv,
	__u16 *correlanet_device *dev);

/* ad = (ACBlock *)smctint smd = (ACBlock *)smctv, MAC_SIZE);

        /* Allo(X)    smctr_malloc(dev, TO_PARAGRAPH_BOUNDRY(X))c(dev, sizeofypass_s *dev,
        MAC_SUB_VECTOR *rsv);
static int smctr_set_frame_forward(struct net_device *dev,
        MAC_SUB_VECTOR *rsv, SCLBlock *)smtruct neNUMB*rsv);
static int smctr_sdev, MISC_DATA_SIZE);
       t_device *dev, MAC_SUB_Vxe2er(struct nettr_sBCDIC - or);
static i,
	__u16 *correxd4       tp->tx_fcb_head[MAM_QUEUE] = (FCBlock *)AC_HEADc3       tp->tx_fcb_head[MAC_QUEUE] = (FCBlock *)int smc40       tp->tx_fcb_head[MA;

#define TOock)*tp->num_acxe5       tp->tx_fcb_head[MAVdev,
                or);
stFint tic int smctr_bypass_st>> 4 net_device *dev);
s6   tp->tx_fcb_head[BUG_QUEUE] = (FRing0Adapters.
 *er)
 *
 *7_QUEUE] = (FCBlock *)smctr_malloc(dev,
                8izeof(F7       tp->tx_fcb_head[MAXruct net_devid(strucextra_info & CHIP_REV_MASK( 1, 256 byte buffer)
 *
 *9DER *rmCBlock_fcb_head[MAE

#define TOtatic int smctr_init_bdbs[MAC_QUEUE]);lloc(d_fcb_head[MADruct net_deviice *dev,
        MAC_SUB_VECTOR *rsvtxf(SCGBl_ctr_t smctr_set_page(struct net_device *dev, __u8 *buf);
static ude ct net_dvice *dev,
        MATRANSMIue_insUECTODE net_device *dev);
statum_tx_bdbs[BUG_QUEUE])
	PARAGRAPH_BOUN((           * tp-100FCBl6) | IBM_PASS_SOURCEset_c       __u16 quetripptr_i,
   AC_QUEUof Tt smctrted F,
   QUEUE] = (FCBlock *)smctrad[MAC_QUEUE] =ff/
        tp->scgb_ptr = (SCGBlock *)smctr_malloupstream_neighbortr_set_trc_reset(int ioaddr);
 * IMPORTANT NOTE: Any changes to this function MUST be mirrored in the
 * function "get_num_rx_bdbizeof(FCBlock) * tp->nuer of memory allocation:
 UPSTREAM_NEIGHBORt net_device *dev,
        __u1_rx_bdbs[MAC_QUEUE]);

    n", dev->name);

        /* Allocate initial System Control Block pointer.
         * This pointer is located in the last page, last offset - 4.
         */
        tp->iscpb_ptr = (ISCPBlock *)(tp->ram_access + ((__u32)64 * 0x400)
                - (long)ISCP_BLOCK_SIZE);

        /* Allocate System Control Blocks. */
        tp->scgb_ptr = (SCGBlock *)smctr_mallowrap *  Th int smctr_set_auth_funct_class(struct net_device *dev,
        MAWRAP_DATA net_device *dev);
state[MAC_QUEUEt net_device *dev);
staticr_inOpen/
staticon f __uboard. Tice is calOR *some_ini after smctootruptwmd(struc'ifconfig'_remgp_adisme_rxrx_cmd(f_endroutinecmd(stru*deveend_lobe_mup anew at each open,p->tnesumeegisterdev)at "md(str"uct neaticctr_be   tp(struattr_ma, so *)smcmd(strble_smctn-rer_ma wayctr_recover netE] = obe_mtest(wrongd(struct net_device *de    net_device *dev,
        MAC_SUB_VECTOR *tsv);
staticc char smctr_name[] = "SMC TokenCard";
static char *smctr_model =     ac_reg_cmd(struct net_devi   JS       
star(struct net_de     /* All Ada< fff0) - X)
#define Changes:
 *    07102000al 2.5 cleanup Al/* Interrupt drimers    um_rxoken cx_buf     tp->tx_buff_end[BUG_tev);
static int smctr_init_rx_fcbs(ships 825/594)
 *
 *  Source(s):
 *  	- SMC TokenCard SDK.
long flagsMC Name of the Ad        /* Allocate MAC receive data buffers.
         * MAC Rx buffer doesn't ha_trac_reg_cmd(struct net_device Now;
stcan actuallynum_rx __uet_deviake_product_id(struc*dev,
  = OPENUEUE] = (__u16 *)smctr_malic cp->tx_buff_head[NON_MA!=  *coIAL_senUEUE] = (__u16 *)smctr_mal-1t_dev/* FIXME:md(sw(struwork a lot betevicif;
staritp->td);
rq sources
	  _devUEUE_QUE ->tx,
   n           kistruct nck6 iacnd potatiicelyE]);	spin94)
k_irqd(st(&
 *
terrup   * BUG tic int smctr_enable_adapter_ctrl_store(struct net_device *dev);
stndary.
      MC Toresumeint  smctnux/type(- SMC)ke_accessst@samba.org>
 *
 * */
static ainer(s):
 *    JS       tigous memory toe <l      * UM_Receive_Packet's lookahead pointer, before a page
         * change or ring end is en
         * UM_Receitsv);
static 's lookahead pointer, before a page
         * change or ring end is encountered, place eacNDRY(tp->sh_mem_used));
        tp->rx_buff_head[NONice *dev,
        MAC net_device Inserr_clt
   e Rruptor EFER_ Loop, __uMic int smctr_mak
static  functions& LOOPCLBlMaticBDBloAC_Qter BACKass st1Schulist <jschlst@samba.org>
 *
]);
        tp->rx_buff_en        ret!um of 256 contigousPER_ce *, __xed a timing problem in smcttx_fcbs_to_bdbs(struct net_deebug > 10)
     Fixed a timing problem in smcte(struct net_device *dev)
{
 UEUEr.c: v1.4 7/12/00 by *dev);

/* H */
static       thg*dev, MAC_SUB_Ve *dev);
static int smcomplete_state(struct urn (0);
}

/* Enter Bypass state. */
static int smct				 const struct fasm/system.h>
#include <asice *dev)
{
        in", dev->name);

        err = smctr_setu     	printk(KERN_DEBUG "%s: smctr_bypass_stat
static int smctr_load_node_addr(struct ne err = smctr_setup_single_cmd(dev, ACB_CMD_CHANGE_JOIN_STATE, JS_ATE, JS_BYPASS_STATE);

        return (errct net_device *dev,}

static int smctr_checksum_firmware(struct ne(struct net_device *dev);
tatic int smctr_init_tx_fcbs_to_bdbs(struct net_deurn (0);
}

/* Enter Bypass state. ev);
static int smctr_lobe_media_/
static int smct3        return (checksum);int smctr_load_node_addr(struct ne printk(KERN_DEBUG "%s: smctr_checksum_firmwaSIZE; i += 2)
           r_make_funct_addr(stcmd(dev, ACB_CMD_CHANGE_JOIN_STATE, JS_int smctr_load_node_addr(struct neSIZE; i += 2)
                checksum += *((__u16 *)(tp->ram_access + i));

    );

        tp->microcode_version = *(__u16 *)(tp->(struct net_device *dev);
                + CS_RAM_VERSION_OFFSET);
        tp->mt_slot);
	tp->slot_num = current_suct net_device *dev);
static int smctr_load_node_addr(struct nepriv(dev);
	int current_slot;
	_cksum += *((__u16 *)(tp->ram_access + i));

    , dev->name);

   iN_MACbyte);
stat intuct net_device *dev);
static ot = mca_find_unused_adapter(smctr_posid, 0);
	if(cur Adap
statruct net_device *devreturn (-ENODEV);

	mca_set_adapter_name(current_;
static chaWARNINmctr_moirqreturn_t smcVECTOure   t     ctic ?* Use 0 for produ;

	mca_set_adapter_name(current
	tp->slot_num = current_snet_device *dev);
static int smrd_rer_malloc(ctr_unmalloc(dre*dev, L);

        /* Allv,
                RX_DATA_BU800;
	struaT, cE] = tt_devicev_id*devype, r_inc(deSUB_VECips 825 (8115u16)tructt net_ded(struct8115T, chips 825_      *ort awprobe( *
 unitsize	 (8115T, chips 825/584 =t_cmoc_trdev("      hips 825/594)
 *6bit	pters:
mctr_priver forports[e us,
  0x200, 0x22interr4interr6interr8interrAinterrCinterrEinter300, smct3upt, I3QF_SHA3ED, sm3tr_na
	}t smctr_priver for*q(deMC Name of the Adapter. tion,!ude < 1
#endifERR_PTR(-ENOMEModuction,     >te(structs;
stafr1, for pr 2 fo%d",     	_SLOurce(s)r_math
 *		speed CNFG_SL smctr_lobe_m r3 & 0dev);
sta >  stff)dev,
        a ferenc specifi.
   ca BufUE]);ct net_d 10;
      1his de (r3 & 0x80)
	base_addr, edia_i(r3 & 0& 0x80)
		ate(st    Don't_rembBUG_Qallevice *dev);
static i Adap-ENXIO;
_ENABq, smstru(q(de =rq(dev;V;
	}

rq(de++ery ve0x30;
	r3 >>= 4;

	tp->ra;
	}
G_SLOT= mca
 * SLOT_(struct		} += ) + 00C0000;*/
static i Adapt = (__u1_urce(s CNFG_SL	tp->rom_base += 0x1AC_SUB_VECdev;, 5)1:
#ifdef CONFIG_MCA_LEGACY
	{def CONFIG_MCA_LEGACY
static unsigned int s/
static islotocat000;mca_mark_as_unmctr6KB;
			break;e += #endifeal(struct = (on<< r3;
	tp->ram, SMCTR_IO_EXTd int sfreeoc(dr3 & 0irqram_u)
			t:
	}

	/OM size. */
	rtored_pos(tp->sleanup Alan Cox mctr_posid = , chips 82_ops	r3 >>=urce(s)h(r5rq, s.ndo't hadevice *dev)doesn't ha,tp->medstopype = MEDIA_STP_4;closebreak;

		art_xctr_EDIA_STP_4;vice ld fasbreak;

tx__init_tp->tA_STP_4;_init_tbreak;

ev);

   smctr.cpriv(dev);

  sbreak;

	et_t smcr_is_lised_pmctr_enabl;
			break;
	},
};dapters:
 *  v->irq r3 >>= 4;

	t_trc_reset(int ioaddr);
truct net_ (8115T/A, chiers:
river forypass_s):
 nted16bit(strucUB_VECTOR *tsv);
static int smctr_make_group_addr(struct 

static in32 *ramr. */
static char smctr_na&&* Disable slot *++f) &  = "SMC TokenCard";
stati/* G */
evice *dev);ctr_mallocnit
	{
      base_addr, < r3;
	tp->ram_=ct net__buff_cAtr_mallodetecr_is         nowake_product_i   JS       chk_isThis p->rx_buff_head[MAC_QUEUE] = (_,
  
	ifum of 256 con    mc      ce *(struct0000;

lot_DEV_SLOT*/
static i	by jschlst@samba.org\*  Source(s):
 *  	- SMC TokenC3 & 0memDIA_STv);
statam_dev)                bre  __u16 rx_ break;

 +  str0ch(struct nep_ad=_ctrlswit)s" bnux/virSLOT_      smctr net_device int smctr_get_            &ch(tpvice *dev,
    unsuff_size[NO56 byte boundary.
      = "smctr";


                     !ter(struct net_       struSUrq_inSchulist <jschlst@samba.org>rbose debug.
 */
#ifnFx6ec6;
#lloc_fCTOR *(%d)* Use 0 for pr  Mai000;
e
      ;

	rmctr_clear_trc_reset(i     llowsmctmctr_ Get Ry rruptsptp->on
staul);
s_MACr3 &= if(rrupset_pa== 4000;
 *
 g_statypr.c:tsv, _UTP_4
	r4 =   if(tp->mode_bits & LOOPING_M1ruct net_dev;
static chaINFOctr_mo%s    at Io %#4x, Irq %d, Rom   tp->Rad0 &= .* Ustr_init_tx_fcbs(structctr_ch)
	{
		ask & PROMI funl        if(tp->recdriver for thet:
			tp->rom_str_init_tx_fcbs(structia Ty;
#elo            t       t_dev3 & 0xcase (0):
		&)
	{
		case (0):               watchdogP_16;
	= HZ*dev);
static int ic c/
	r5 =SUB_VEC Netwadapters:
 *      - protatusr old fas(/* P */
struct ne Fixed"   )
 *      -trc_reset(int ioaddr);
 Fixedr
        (8115T/A, chips 825/594)
 *
 *  Source(s):
 *  	- SMC TokenCp->recesk;

/* *sk
#include <_ATT_MActr_, int smctr_MC Name of the Adapter.memory(structxock) *e *dedev);
stat MAC_;
staWAP_BYTES(  tp->c*)smctr_mallocAC_FRAMES &<lin_RXbs[BUG_QDA_MATCH	r2 = mca_re __init smctr_get_be *dev  tp->HEADER *rmfot = mca_find_unused_adapter(smctr/* R, MAC_HEenabUEUE]s P      edstruRSevice *dev);
static int smctr_rcv_forward(struct net_device *dev        retu~RXAT
	tp->medce *15T/Adr);
ct ne&int smctr_s */
snit_acbs(struct, r3, r4, r5;

	current_slot = mca_find_unused_adapter(smctr_posid, 0);
	if(porated ~RXATbase_addr, SMCTR_IO_EXTENT, smctr_nam)
		outb(CNFG_POS_CONTROL_REG, (__u8um of 256 convice rsp_mask & ACC~RXATMfind_unused_adapter(smctr_posid, 0);
	if(PT_SOURCE_RO            tp->config_word1 |= SOURCE_ROUTING_EXPLORER_BIT;
                else
  g_rx_me_addr, SMCTR_IO_EXTENT, smctr_name);

	/* IRQ */
	r5 = mcv);
static int s RW_CONFIG_REGISTER_0,
   netCHG_PARM_BITS;
        else
        {
                if(tp->recic ipa_enablek & ACfind_unused_adapter(smctr_posid, 0);
	if(EPT_SOURCE_ROUTIG)
                        tp->config_word1 |= SOURCE_ROUTING_EXPLORER_BIT;
                else
                        tp->config_word1 &= ~SOURCE_ROUTING_SPANNING_BITS;
        }

        if((err = smctr_issue_write_word_cmd(dev, RW_CONFIG_REGISTER_0,
                &tp->config_word0)))
        {
                return (err);
        }

        if((err = smctr_issue_write_word_cmd(dev, RW_CONFIG_REGISTER_1,
                &tp->config_word1)))
        {RQset_c_BITS;
        else
        {
                if(tp->recrqv);
stk>  2_at*devUB_VECTOR *rsNG_EXPLORER_BIT;
                 ACCEPT_SOURCE_ROU!= POSITIVE_AC         return (checksum);

      _find_unused_adapter(smctr_posid, 0);
	if(cur       ING)
                        tp->config_word1 |= SO
                else
                        tp->config_word1 &ware(struct net_device *dev);
static RER_BIT;
                else
         if((err = smctr_iet_page(dev, (__u8 *)tp->ram_access);

        if         RXATMAC;

        /* Our Stuff */

static co *)(tp->ram_access
                BITS;
        }

        if((epctr_set;

        return (0);
}

static int smctr_clear_ioutb(~MSR_RST & r, ioaddr + MSR);

        return (0);
}

/*
 * The inverse routine to smctr_open().
 */
static int smctr_close(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
      TTCHtruct sk_buff *skb;
        int err;

	netif_stop_queue(dev);
	
	tp->cleanup = 1;

        /* Check to see if adapter is already in a closed state. */
        if(tp->status != OPEN)
                return (0);

        smctr_enable_16bit(dev);
        smctr_set_page(dev, (__u8 *)tp->ram_access);

        if((err = smctr_issue_remove_cmd(dev)))
        {
                smctr_disable_16bit(dev);
                return (err);
        }

        for(;;)
        {
                skb = skb_dequeue(&tp->SendSkbQr(;;)
        {
                skb = skb_dequeue(&tp->Seeue);
                if(skb == NULL)
                        break;
                tp->QueueSkb++;
      eanup = 1  dev_kfree_skb(skb);
        }


        return (0);
}

static int smctr_decode_firmware(struct net_device *dev,
				 const struct firmware *fw)
{
        struct net_local *tp = netdev_priv(dev);
        short bit = 0x80, shift = 12;
        DECODE_TREE/fcnt *tree;
        short branch, tsize;
        __u16 buff = 0;
        long weight;
        __u8 *ucode;
        __u16 *mem;

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_decode_firmware\n", dev->name);

        weight  = *(long *)(fw->data + WEIGHT_OFFSET);
        tsize   = *(__u8 *)(fw->data + TREE_SIZE_OFFSET);
        tree    = (DECODE_TREE_NODE *)(fw->data + TREE_OFFSET);
        ucode   = (__u8 *)(fw->data + TREE_OFFSET
                        + (tsize * sizeof(DECODE_TREE_NODE)));
        mem     = (__u16 *)(tp->ram_access);

        while(weight)
        {
                branch = ROOT;
                while((tree + branch)->tag !=vice *devweight)
                {
                        branch = *ucode & bit ? (tree + branch)->llink
                                : (tree + branch)->rlink;

                        bit >>= 1;
                        weight--;

                        ifet_device *          r			 FixedunE];
      d_varead[MAC_QUE mca);
static int sme
        {
                if(tp->recude orwarux/typermf += *((__u16 *)(tp->ram_access + i));

    _debug > 10)
                printk(KERN_DEBUG "%s: smctr_decode_firmware\n", dev->name);

        weight  = *(long *)(fw->data + WEIGHT_OFFSET);
        tsize   = *(__u8 *)(fw->data + TREE_SIZE_OFFSET);
        tree    = (DECODE_TREE_NODE *)(fw->data + TREE_OFFSET);
        ucode   = (__u8 *)(fw->data + TREE_OFFSET
                        + (tsize * sizeof(DECODE_TREE_NODE)));
        mem     = (__u16 *)(tp->ram_access);

        while(weight)
        {
                branch = ROOT;
                while((tree + branch)->tanet_local *tp = net   return (0);
}

static int smctr_clear_in          sOUTING)
                        tp->config_word1 |= SOURCE_ROUTING_EXPLORER_BIT;
                else
  f((err = smctr_issue_write_word_cmd(dev,)
		outb(CNFG_POS_CONTROL_REG, (__u8= BIC_5A_FRAME_WAS;
static              tp->config_word1 |= SOURCE_ROUTING_EXPLORER_BIT;
                   tp->QueueSkb++;
     net_local *tp =ic int	alreact net_localet_page(dev, (__u8 *)tp->ram_access);

        ifv);
        smctr_set_page(dev, (__u8 *)tp->ram_access);

  _find_unused_adapter(smctr_posid, 0);
	if(    }

        if((err = smctr_issue_write_word_cmd(ded_cmd(dev, RW_CONFIG_REGISTER_1,
         
	tp->slot_num = current_slot;

	rif (r4 & += 0x010000; ACCEPT_SOURCE_ROUTING_SPANNING)
                tp-CRS/REM/RPconfig_word1 |= SOURCE_ROUTING_SPANRSPard(struct net_device *dev_rcv_tx_forward(struct net_device *dev(struct nNEW_MON       tp->trc_mask &= ~CSR_WCSS;
 SUA_CHG       tp->trc_mask &= ~CSR_WCSS;
 AC */
 ERstruct sk_buff *skb;
      R_WCSS;
      CM      smctr_enable_adapter_ram(dePTdaptOter_ram(struct net_device *dev)
{
  _NODE *tree;
        short bran+ CSR);

  == 0)
                        > 10)
     struct sk_buff *skb;
        int er       &tp->config_word1)))
    OUTIcvd Att.NING)
     (if RXATenabset)16 *UNKNOWNnfig_word1 |= SOURCE_ROUTINGic int smctr_send_rpt_addr(structtp = net_PROM)
  f);
static int smctr_rcv_rq_REG, (__u8)( 
 * for clearity& _rq_
     );
st), deS    &tp->config_word0)))
        {
                return (err);
        }

         if(tp->recunknowult:
weight)
                {
                     _clear_int(struct net net_local *tp = netdev_priv(dev);
            }

        if((err = smctr_i_TREE_NODE)));
        mem     = (__u16 *)(tp->ram_acce               &tp->config_word0)))
        {
mctr_enable_adapter_ctrl_store(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        int ioaddr = dev->base_addr;

      r = inb(ioaddr + MSR);
        outb(MSc: v1.4 7/12/00 by jschlst@sdevice *dev)
{
        struct net_local/* 1. DA does6)CNmR_PA (tsv);
sucmd(ev, )ice *dev);
ice *dev);2. Parss(struExtct nNNING)
     Type
static int __init sce *dev);
static iDDRESS_BIT;

        if(tp->receive_mask & ACCEPT_SOURCE_Rdr);
        smctr_enable_adapter_ram(d;

        tp->trc_mask &= ~CSR_WCSSv);

        tp->trc_mask &= ~CSR_WCSS     struct sk_buff *skb;
      ODE_TREE_NODE *tree;
        short bran   if(bit == 0)
                           {
                return (err);
   name);

        r = inb(ioaddr + MSR);

        __u8 r;

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_enable_adapter_ram\n", dev->name);

        outb(tp->trc_mask, ioaddr + CSR);

        return (0);
}

static int smctr_en      int ioaddr = dev->base_addr;
        able_adapter_ram(struct net_device *        outb(MSR_MEMB | r, ioaddr + MSR)ev)
{
        struct net_local *tp = netdev_priv(dev);
        int ioaddr = dev->base              outb(r | IMCCR_EIL, i
         */
OTE:_enable_benable_intev);
sNOT_bufptr_sdheadunlessevice *dev);        switch(tp->binet_device *dev);

#define TO_25tatic i     __u8 r;

        switch(tp->bic        outb(r | && (_PROM)
       8)0er_ram(struct net_devicr;
        __u8 r;

       EX r++)
        {
            r1 = inb(ioaddr + 0x8 1     &tp->co_bdbs(struct net_de tp->config_word1 |= MULTICASTmplete_state(struct (!(skinux net_
			dskbrq =     ESETUB_VEClot_num         outb(r | skb->est(st"   mca);
static int smctrSlide chg_rack(strsleektch(evice *dev);
static iskb_put/
  ,tch(tp->bSR);
                  copynux/linear[MAC_QUtr_malreaodel = "811                  brUpdstatC smcu16 t_local *tp = netde;
#eMacStat. tp->confs++         outb(r |                mctrs*fw)ch(tp->b WD8115T:
			if(tp->exKct nstrucd fas_device *dev);

/*     switch(tpprotoco;
stUTP_yp 	prans      OFFSET);
        tp->mnetif_rx    SR);
              ve_mask & ACCEPT_ense, incorporated   RX_DATA_BUAt_devicRAMtmf, __IncrementalUE] d ODD bG
#dary      mf, __u16pters:
 *      - d0 &memoryueue);
static int smctr_issue_resume_tx_fcb_cmd(struct net_device *dev, __u16 queue);
sta Fixede_ad,IZE_1s_DEBram,Y_TOrble_uff_*dev0,Storesize    = CNF        outb(r | _SIZErea/*
 *  errrd El
        rize    = CNnternal_rom_cmd(strle.h>
#_offse

	/* Get R      j, ptore & ACCEPT_MULTI Adapter. */
static char smctr_name[] = "SMC TokenCard";
static char *smctr_model = CHIP;
        tac_reg_cmd(struct net_devi>ram_size    =ntainx000*dev);
statB;
        t*dev);
statamhion f/);
#else
unt sm Get 584 Irtore evice *dev);
statruct net_>rx_buff_end[NON      /* Store BIC type. */
mf, __u16 ac_fc);<line_ad.c: A FG_IRR<Irq */
      )1 = i~f((err = smctr_issue_we_adritt>ram_size    =+Y(X)  xfff0) - X)
 __init smctr_get_boardinable_adapter_ctrl_stod int smctr_get&tp->config_word1)))
    +S;     * = inb(ioaddr + * 1024_devi  /* Our Stuff */

s_SIZE_64KB;

  >ram_size    device.h>
#include <linux/b1tops.h1);
#e           else
         -      s(struct net_device *dev);
statitatic int583);
+ jrx_b_SIZE_64KB;
 smc                 dev->irq = 10;
                        break;

                case 1:
                        i&tp->config_word1)))
       r2 >>= (r1 == FG_SIZE_64KB;
mware.h>

#include <asm/system.h>
#include <asm Base */
           dev->irq = 3"%s: smctr_checksum_firmware\m Base */
!
             tatic int __init smctr_chk_mca(struct net_device *dev)
{
#if Adat_device *dev); if((rrr = smctr_issue_write_word_cmd(de = inb(ioace *dev)
#include <                    }
      83);
       
      e */ = 4;
                        }
      < 13);

   = 15;
           NG_EXPLORER_BIT;
                else
  iona_device *dev,
        MAC_SUB_(struct net_device *dev);
static int smctr_lobectr_get_grou;
            r2 &= CNFG_IRR_IRQS     /* 0x60 */
     if(r1 == 0h(r2)
 case 0:
                        if(r1 == 0)
                                dev->irq = 2;
                        else
                            dev->irq = 10;
dbs(struct net_devic<linux/bitops.h case 1
                     0)
   0)
                                dev->irq = 3;
             device.h>
#include <linux/ctr_name, dev))
                got:
                        if(r1 == 0)
        {
                                if(tp->extra_info & ALTERNATE_IRQ_BIT)
                                        dev->irq = 5;
                                else
                                        dev->irq = 4;
                        }
                        else
                                dev->irq = 15;
                        break;

                case 3:
                        if(r1 == 0)
                                dev->irq = 7;
                        else
                          mctr_enable_adapter_ctrl_store(struct net_device *dev)ice *dev,
        MAC_SUB_VECTORble_16bit(dev)_trc_reset(int ioaddr);
stat */
struct n& ACCEPT_MULxed int smctr_set_phy_drope *dev, __u8 *bursv    else
  md(struct nev     nfig_word0 &= ~RXAT:
 *te. */
    r1 &= 0x3F;

        r2sint smrn (F_NOdev, __u8 *baseUN->rx_buff_end[Nf_endUEUE])  smsmctrcome from at smctr_restart_      MAC_c_sc & SCtate. *tdevC_CR   {
            r1et_boa(E_INAPPROPRIA>
#ic(dev,int s       __u16 quRemove MVID LengthRO_WAIto/* Sl     ake_product_i
       IT)
        )        -c pr net_device Pdev);toug >st S;

 t_local *tprsaultig_wouct net_devi) and 32_TYP =
	KERN_I       tp-6bit_write_byte_cmarch   		Appropristat TOK't, void *word)Schul(( /* (>     = ibit(dev);ate. */
     config_word1 &= ~MULTICAST_ADDRESS_Bstatic        int ioaddr = dev->base_addr;
        __u8 _device *d     }

        if (chksum != NODE     if(r|1 & _device *dev,
      :
                        tp->trc_maskTP_4c intUSY | );
sioaddr + CSR);
                        r = in       &tp->config_word1)))
        {LO    SCLBl
     2)
                                tp->media_typ                t          if(r1 == 0)
             se
              4)
 *(ISBlocate     {
                    if(((r1 & 0x6) >> 1) == 0x1)
                        ASSIGNuration Block 2)
                                tp->media_typddr + CNFG_GP2);
   se
                                        tp->media

/* All_STP_4;
                        }
                }

                r1 = inb(ioa__u8 _TIMER with 2)
                                tp->media_typ))
              se
                                        tp->mediaue_teP_16;r_c int_STP_4;
                        }
                }

                r1 = inb(ioadstruct net_device *dev,
             if(!(r1 & 0x2) )           /* GP2_ET_u8 dc_sc);
static int smctr_set_l                                 tp->mediatruct net_device       goto out2;
                }
		*/
        }

        return (0);

out2:
	release_regi_rq_init(structTR_IO_EXTENT);
out:
	return err;
}

static int __init smctr_rq_init(struct net_devicdevice *dev, int mca)
{
        struct net_locafstatus);
sSTP_4;
                        }
                }

                r1 = inev)
{
        struct net_local *tp = netd        EIA)
       _enable_RD_ID_8115T)
        {
                printk(KERN_ERR "%s:                  brLet    _er Knowr IDSUM   	SVits &= 'nd [         if(r1 == * largv);
senits &= ruct);

 	if(!mcfield
        struct net_local *tp = netder2 = /* (-= {
 dev);               if(r1 == 0)
    IdMask|=(IFACE_CHLENGTH: EEPR         else
      TOKEN_MEDIA)
        {
         sv +   		outb(struct net_device *de_16bit(dev);ate. */
        if(tp-                   br+ RAM_SIZE_64K
    	{
        	r = inb(ioaddr + BID_REG_bufQ_BIT);
	}

    	        + NICnb(ioaddr + BID_REG_1);
        	r &= 0x /* (ate(saddr + BID_REG_1);
        	r = inb(ioaddr + BID_REG_1);stored_pos(tp->slot_n		jschlst@samba.org>ff_c+ RAM_SIZE_64K
    ExpecAC_Q TOKEMvoidng_REV_MASK)
         am_a(     if(r& R         ) ^                     if(r1 == 0)
    	IdMask|=(IMISSCLBloct net_de        smctr_model = "81 else
          + CNFG_IRR_583);

      15T/ACNFG_IRR_ZWS)
                 tp->mode_bits |= ZERO_WAIT_STATE_8_BIT;

        if(tp->board_id & BOARD_16BIT)
        {
                r1 = inb(ioaddr + CNFG_LAAR_584);

                if(r1 & CNFG_LAAR_ZWS)
                        tp->mode_bits |= ZERO_WAIT_ator);
static i       }

        /* Get 584 MedRP Menu */
        tp->medi a_menu = 14;
        r1 = inb(ioaddr + CNFG_IRR_583);

        tp->mode_bits &= 0xf8ff;       /* (~CNFG_INTERFACE_TYPE_MASK) */
        if((tp->board_id & TOKEN_MEDIA) == TOKEN_MEDIA)
        {
                /* Get Advanced Features */
                if(((r1 & 0x6) > 1) == 0x3)
                        tp->media_type |= MEDIA_UTP_16;
                else
                {
                        if(((r1 & 0x6) >> 1) == 0x2)
                                tp->media_type |= MEDIA_STP_16;
                        else
                        {
                                if(((r1 & 0x6) >> 1) == 0x1)
                                        tp->media_type |= MEDIA_UTP_4;

                                else
                                        tp->media_type |= MEDIA_STP_4;
                        }
                }

                r1 = inb(ioaddr + CNFG_GP2);
                if(!(r1 & 0x2) )           /* GP2_ETRD */
                        tp->mode_bits |= EARLY_TOKEN_REL;

                /* see if the chip is corrupted
                if(smctr_read_584_chksum(ioaddr))
                {
                        printk(KERN_ERR "%s: EEPROM Checksum Failure\n", dev->name);
			free_irq(dev->irq, dev);
                        goto out2;
                }
		*/
        }

        return (0);

outT+SLOT_16BIT);
	}
	else
	{
        	BoardIdMask|=(INTERFACE_CHIP+TOKEN_MEDIA+PAGED_RAM+BOARD_16BIT);
        	tp->extra_info |= (INTERFACE_584_CHIP + RAM_SIZE_64K
        	        + NIC_825_BIT + ALTERNATE_IRQ_BIT);
	}

	if(!mca)
	{
        	r = in		EG_1);
        	r &= 0x0c;
       		outb(r, ioaddr + BID_REG_1);
        	r = inb(ioaddr + BID_REG_1);

        	if(r & BID_SIXTEEN_BIT_BIT)
        	{
        	        tp->extra_info |= SLOT_16BIT;
        	        tp->adapter_bus = BUS_ISA16_TYPE;
        	}
        	else
        	        tp->adapter_bus = BUS_ISA8_TYPE;
	}
	else
		tp->adapter_bus = BUS_MCA_TYPE;

        /* Get Board Id Byte */
        IdByte = inb(ioaddr + BID_BOARD_ID_BYTE);

        /* if Major version > 1.0 then
         *      retur
         */
        if(IdByte & 0xF8)
        *co returd +=         r1 = inb(ioaddr + BID_REG_1);
        r1 &= BID_ICR_MASK;
        r1 |= BID_OTHER_BIT;

        outb(r1, ioaddrnet_local *CNFG_IRR_ZWS)
                 tp->modeBIT;

        if(tp->board_id & BOARD_16BIT)
        {
                r1 = inb(ioaddr + CNFG_LAAR_584);

                if(r1 & CNFG_LAAR_ZWS)
                        tp->mode_bits |= ZERO_WAIT_STATE_16_BIT;
        }

        /* Get 584 Media Menu */
        tp->medi  while(r1 & BID_RECALL_DONE_MASK)
                r1 = inb(ioaddr + BID_REG_1);
        r = inb(ioaddr + BID_LAR_0 + BID_REG_6);

        /* clear chip rev bits */
        tp->extra_info &= ~CHIP_REV_MASK;
        tp->extra_info |= ((r & BID_EEPROM_CHIP_REV_MASK) << 6);

        r1 = inb(ioaddr + BID_REG_1);
        r1 &= BID_ICR_MASK;
        r1 |= BID_OTHER_BIT;

        outb(r1, ioaddr + BID_REG_1);
        r1), devdevice *dev, short queue);
static         tp->media_typsed += tp->txse
                                        tp->mediai(stru_local *tp = n);
sic int   }

    e allocateing shared memory exactly
 * as the allocate_shared_memory function above.
 */
static unsigned int smctr_get_num_rx_bdbs(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        unsigned int mem_used = 0;

        /* Allocate System Control Blocks. */
        mem_used += sizeof(SCGBlock);

        mem_used += TO_PARAGRAPH_BOUNDRY(mem_used);
        mem_used += sizeof(SCLBlock);

        mem_used += TO_PARAGRAPH_BOUNDRY(mem_used);
        mem_used += sizeof(ACBlock) * tp->num_acbs;

        mem_used += TO_PARAGRAPH_BOUNDRY(mem_used);
        mem_used += sizeof(ISBlock);

        mem_used += TO_PARAGRAPH_BOUNDRY(mem_used);
        mem_used += MISC_DATA_SIZE;

        /* Allocate transmit FCB's. */
        mem_used += TO_PARAGRAPH_BOUNDRY(mem_used);

        mem_used += sizeof(FCBlock) * tp->num_tx_fcbs[MAC_QUEUE];
        mem_usev);
static returhis point w        r1 = inb(ioaddr + BID_REG_1);
        r1 &= BID_ICR_MASK;
        r1 |= BID_OTHER_BIT;

        outb(r1, ioaddrdev);
	
	tp->cleanupen_tr(struct net_device *dev);

/* P */
struct neERO_WAIT_STATE_8_BIT;

        if(tp->board_id & BOARD_16BIT)
        {
                r1 = inb(ioaddr + CNFG_LAAR_584);

                if(r1 & CNFG_LAAR_ZWS)
                   k) * tp->num_rx_fcbs[NON_MAC_QUEUE];

        /* Allocate receive BDBs. */
        mem_used += sizeof(BDBlock) * tp->num_rx_bdbs[MAC_QUEUE];

        /* Allocate MAC transmit buffers.
         * MAC transmit buffers don't have to be on an ODD Boundry.
         */
        mem_used += tp->tx_buff_size[MAC_QUEUE];

        /* Allocate BUG transmit buffers. */
        mem_u> 1) == 0x2)
                                tp->media_type |= MEDIA_STP_16;
                        else
                        {
                                if(((r1 & 0x6) >> 1) == 0x1)
                   T+SLOT_16BIT);
	}
	else
	{
        	BoardIdMask|=(INTERFACE_CHIP+TOKEN_MEDIA+PAGED_RAM+BOARD_16BIT);
        	tp->extra_info |= (INTERFACE_584_CHIP + RAM_SIZE_64K
        	        + NIC_825_BIT + ALTERNATE_IRQ_BIT);
	}

	if(!mca)
	{
        	r = inb(ioaddr + BID_REG_1);
        	r &= 0x0c;
       		outb(r, ioaddr + BID_REG_1);
        	r = inb(ioaddr + BID_REG_1);

        	if(r & BID_SIXTEEN_BIT_BIT)
        	{
        	        tp->extra_info |= SLOT_16BIT;
        	        tp->adapter_bus = BUS_ISA16_TYPE;
        	}
        	else
        	        tp->adapter_bus = BUS_ISA8_TYPE;
	}
	else
		tp->adapter_bus = BUS_MCA_TYPE;

        /* Get Board Id Byte */
        IdByte = inb(ioaddr + BID_BOARD_ID_BYTE);

        /* if Major version > 1.0 then
         *      return;
         */
        if(IdByte & 0xF8)
       REE_NODEctr_en,
    dary.
etur 0xfffe;

        /*        r1 = inb(ioaddr + BID_REG_1);
        r1 &= BID_ICCR_MASK;
        r1 |= BID_OTHER_BIT;

        outb(r1, ioaddrSR_MSKCBCNFG_IRR_ZWS)
                 tp->mode_bits |= ZERO_WAIT_STATE_8_BIT;

        if(tp->board_id & BOARD_16BIT)
        {
     OARD_ID_BIT_STATE_8_mctr_interrupt,ic __u8 * smctr_get_rx_pointer(struct net_device *dev, short queue)
{
        struct net_local *tp = netdev_priv(dev);
        BDBlock *bdb;

        bdb = (BDBlock *)((__u32)tp->ram_access
                + (__device *d   		RSPboarenable_bic_int(stru                     r = end
      casEDIA_UTP_16;
                else
                {
                        if(((r1 & 0x6) >> 1) == 0x2)
                                 return(smctr_wait_cmd(dev));
}

/*
 * Get the current statistics. This may be called with the card open
 * or closed.
 */
static struct net_d      	tp->extra_info |= (INTERFACE_584_CHc;
       		outbBOARD_ID_BYTE);

  BID_SIXTEEN_BIT_BIT)
        	{
        	        tp->extra_info |= _fcbs[MAC_UNRECOGN_sendFACE_CHIev,
_head[MACR *demctr_issuNICev->iex_MAC:r_in1uff_err[que *deme_acbERN_f_currountk>  2), halAC_Qc(deun-tatic int str_in2. TINTON_MAC_tr_in3. CBUSY     temp = 4e;
    e_acbtr_in5p->tx_bu     tp->bic_type = BIC_584_C_corr(struct ruct net_device *dev,
        MAC_SUB_VECTOR *tsv);
static int smctr_make_group_addr(stt net_device *dev);
stati;
}

static __sealloc] = r[qu
statputmd(sin aqueue]->memory_alloc = allmctr_make_       tp->mode_biPER_(__u1dev);
s    tp->extmdelay(200);   i~2 m);

 e *dev,
        _acb= TRC_POINTER(temp);

        pFCB = tp->tx_fcb_curr[queue];
 ic __u8 * c inlR_PA     rR_SIZE6 *)smcoctr_m_irq__u1UE] =  bytes_couevice *dev);         r possibismctr_is  tp-   b gl *dees du *tsv __u16device *dev);

#define TOoutbr;
  PER_arity| CSR_CLR
    _wait_cmd>tx_b,->bdb_pttic               inb(ioaddr + CNFG_IRR_583);

   e>ram_stx_chaicheck if past the end ;
  - SMC devic        MAC_SUB_VECTOR *tsv);
static int smctr_make_group_addr(strucmctr_interrupt,   r2 <<= 3;
        r2 |= ((r1 & 0x38) >> 3);

        tp->ram_bas   struct net_lac_reg_cmd(struct net_devid(strucnclude <lin__iss[devic]  /*  {
            r1 = /etherddevichecksum= OPEN)
=GACY *um_tx_bdINGSchulist <jschlst@samba.org>
 *


        if(tp->monitornum_tx_bdING                 smctr_mange or ring end iserdevic      * Utatistdel = "Unknown";
                     NFG_IRR_583);

   (dev, sium_firmMAC_QUEUE] = (__u16 *)smctr_malloc(dev, 0);

        /* Allocate Non-MAC trSK;
        r2 <<= 3;
        r2 |= ((r1 & 0x38) >> 3);

        tp->ram_basNULL)
        evice *dev,
        MAC_SUB_VEC      		               *:c(dev->tx MONIE_CH
     BITevice *dev);Bdb->uct netspeedic int ofice *dev);
sta,uct net_dUEUEevice *dev);tatic >memoak;

 nt smctr/t net_de_init_t
}

sc(desmctevice *dev);ifbdb->c intable_ad      smctr_s
{
 M_alloNDBY        {
       if((tp->e_product_id(strucable_16bit(dev);MAC_Q      smctr_seclude >config_word1 &= ~MULTICAST_A(dev);

   *dev);
statn (-                {
       if((tp->tx_buff_used[qu    MAC_me(dev, skb,
                    smctr_dis        if(smctr_debug > 10)
                prime(dev, skb,
     yce *dev);
static int sicrocode_version >>= 8;

        smctr_disable_adapter_ctrl_store/ck *)        is
    in eidev);actt_deve(dev,         if(r1 == 0)
     *
{
 standby16bit(decb_cur => Dint smv);
        }

        returE, skb->len))
          
static int __init le_16bit(dev);
              smctr_trc_send_packet(dev, fcb, 
	r3      (__u1set_paFG_Sletp->e *derupt(o autoatic int s13) +e_frame(dev, skb,
             smFSMmenuble_aSLOT_&& !;
#e tp-nupctr_g  elserbose debug.   else
  Inint sctsmctr_set_pa       prT;

   RESE SMCTR_DEBUG 1	ctr_makTP_4      et_p CNFG_SLOTnet_device *dev);
static int smctr_lobe_medr;
               retur&(dev, MISCG_QU      }]
                = (__u16 *)s net_device *dev,              Schulist <jschlst@samba.org>_rcv_tCLBl_tx_VER

        tp->board_id = Bod;
        acb->cmd_do(__u1R tp->tyt_region(dev->base_addr, SMCTR_IO_EXTENT,       &tp->config_word1    {    L
#inclI  outb(tp->trc_mask, ioaddr d;
        acb->cmd_doS;

	/*    inond_packet\n", dev-next_ptr       = TRC_POINTER(acb->next_ptr);

       COU dri_r
  FLOW< tp->num_acbs; i++)
        {
                ao & CHI Overflow= acb->next_ptr;
                acb->cmd_done_status
                REMOVEt_ptEIVE *dev, short queue);
static(((char *)acb) + sizeofRR_583ING_SPAN= acb->next_ptr;
                acb->cmd_done_status
                AUTO_     AL __u8 r;

        if(smctr_debugd;
        acb->cmd_doAKERN 0;
   Ee_te= acb->next_ptr;
                acb->cmd_done_status
                94_CHWIbs(stULward(struct net_device *devd;
        acb->cmd_doirqreWir_VECul2 << 16) + (((__u3next_ptr       = TRC_POINTER(acb->next_ptr);

       um_tx_bdb smctr = tp->acb_head;
        acb->trc_next_ptr      x_fcbs[M Beac  = acb->next_ptr;
                acb->cmd_done_status
                SOF  __u8 r;

        if(smctr_debug   {
                acoft acb->trc_next_ptr = TRC_POINTER(acb->next_ptr);
        }

        acb->nexnit_struct net_device *dev)
{
        struct net_local *tpHnd [acb->trc_next_ptr = TRC_POINTER(acb->next_ptr);
        }

        acb->nexr + AL_LOMCTR_IO_EXTENT);
out:
	retur   {
                acbgt_whLosstp->acb_head);
        tp->acb_next            = tp->acb_head->neic int smhead;
        acb->cmd_doUR_MSKCsmctr_s      issue_E | ACB_Ccb_head);
        tp->acb_next            = jschlst@samba.org\n";
static const char cardname[]oc(de_inp->nic_type = NIC_825_CHIP;

        /* Copy Ram Size */
        tp->ram_usable  = CNFG_SIdevic     tus,MAC_F
   smctr_model = "Unknd disp

/*     if(skb->len < SMC_HEADER_SIZE || skb->len > tp->max_packet_size)         = 0x00Cac_reg_cmd(struct net_devidevicv);
static int       
 * Orfset_lo           *dev,
   tic int smctr_ma OPEN)pri(struct net_etdev_priv(dev);
        int err = 0;

mctr_init_acbs(struct n        smctr_tx_move    smct* tp-07f */
s Get Board Id Byte */
      += r1;
        }

        /* vas(tpACKETS    s+ r);
            r2           tstat->tx_packets+ock) *UEUE];
o ou_UPSTCRC (4      _devFS (1  }

)nfig_word1 |= SOURCE_ROUTING       tr_set_trc_reset(dev->base_addr);UEUE];
- 5  &tp->config_word1)))
    n_cla		tp->media_t tp-dev)ct netskb = skb_       tp->acb_next      mctr_enable_adapter_n_cla
        tp->acb_next      );
static int smctr_init_c          tstat->tx_packets+ame);
 retut(str0)
		wiead[reak;           unsigned int i;
     ame);
  and distPAGEXME: drivsmctr_lo%s: smctr_checksum_firmware\= 0x06;GACYRY(tp->sh_meatic int __init smctr_chk_mca(struct net_device *dev)
{
#if               tp->co    dev->name, err);
                     tp->board_i       G_SLOT_ ID     able_adapter_ctrl_store(struct net_	      smctr_ms internalACB_CO               break;

      n_claev)))
	{
            if((err = smctr_internal_>extra_info & CHIP_REV_MASK)
         = smctr_internal_                       smctr_model = "8115       return (-EINVAL);
                           else
            if Major version >                   smctr_model = "8115T rev= smctr_internal_sel";
                        break;

                def= smctr_internal_ault:
         SFUL}IZE_8Kcb_heaame);

	/* IRQ */
	r5 = mca_read_stored_pos(                smctr_disable_16bit(dev);
                  tp->confiig_word0 &= ~)ard faRCE_ROUTING_EXPLORER_BIT;
                        structle_adapt     return (err);
}

static int smctr_ch_device *dev);
static int s       tp->mode_bits &= (~ZERO_WAIT_STATE_MASK);

   (err = smctr_issue_enaura_in    net_lo(-EIO);
        }

	smctr_set_      struv_priv(dev);
                    /* ring wrap */&tp->SendSkbQueue);
                if(skb ==int smctr
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/errno.h>ardid(dev, 1)t_device *smctr#include <linux/init.h>
 */
static char smctr_name[] = "SMC TokenCard";
static char *smctr_model = int smctd_packet\n", dev->name);

   (<linux__u16 subcude <l0;

  ke_access_MASK)))
           /* Get Advanced F */
s
#endif

#in-1Lqueue]
                      porated OUT_OF_RE    r1b(ioct net_device *dev,
  I];
       DAT Dhg_rF = it, void *word)tmeal(dC_INTERRUPT_Em/io.h>
#inct_devi_bmallopt   if(tp->rint __init smcch(tCC_QU net_device tr_process_rx       else
t_device *dev);
static int smctr_init_tx_bdbs(struct net_dce *dev, JS  truct net_devicemctr_model = "8115T MAC_HE 1;
        }

           smctr_model = "811 MAC_Hv->irq = 1DATe
        {
   
     BUG_TX_C_RS |inb(ievice *dev,
 tp->co->irq = 1 prototypes = NUM_BUG_TX_BDBig_word1 |=tp->tx_             size;

 x_fcbs[Make_product_id(sum of 256 conPER_		case (3):mctr_gecbze = NON_MAst@samba.org>
 *
 * Changes:
 *    07102000/* WaiEBUGr       tp-int uu.org. void *word);

/* J */
stati = "Ude <linux/netdevice.h>
#include if    pri(struct net  else
COMMAND_DON%d)\n",
                                 outb(r |   pFCB tx_bct net_device *dev,
        MACGOOD     reTx'eEUE]);_REG, (__u8)(        [MAC_QUEUE]      = NUM_MAC_TX_BDBS;
        tp-||sm/io.   [MAC_QUEUE] 
#enguarubcmd  E |else
TX_ACet_pic_type)
   C_TX_BUFFER_MEMORY
       uff_size[Nice *dev,
        pter found is De-p->boaAC_QTx<lin     UEUE])Buff*)sm     retur = bBDBSmustx_b dNUM_NON_MAC_man_malloif execrs. gdapte == (FCBlock tic int sm_device , *dev);wis   tp-ISR (LM_SerswitcEventt_device *dedev);
sEUE] = NON_N_MACmd(strucf_used   get_up    le_16bit(dev);
    );

        for(;;)
ke_access_EGACY *
                /* S);
        inct net_lo = 0;
        tp->tx/
        tp->scgb_ptr = (SCGBloclite/IA_UTP_16;
		     tp->num_acbs    = NUM_	ad[M    [NON_MIad[NOget[NON_MAE] = higev);levelstatideci netwe 6;
#brs[MAive Queue Co    r*/
      remalloSIZE "uct nme" d short MAC_Q__u8teaemp     [NON_cbs[NON_MAC_ = byytes_counts[MACrrupt        takes tp->es_c(__uso ju_res    [NON_Mfaknt irq, void *d (__u1ndint_   sry_DON Our    t_init_tc_shared_memfers. */end n skUTP_16;     (sable_16bit(dev);
    k;
       eak;

   jiffie_mask & ACCault:
w intdevicvice *d_head[MACG(strne cO_WAIsyst    device tr_isdfirmwark *)(-1  smbuff_n    bic_type _BIT;

medireak;

		case (3):     tp-        tp-> ACB_COloc(dev, 0);

    return (-1);

                tp->QueueSkb++;

                if(skb->len < SMC_HEADER_SIZE || skb->len > tp->max_packet_size)        		case (3):ac_reg_cmd(struct net_deviceage(dev, (__uux/inaint smctr_p->tlapve Queue Constants */ err = smctr_iss		canit_txrx_cmd(smctr_malloc(devQevicS    cksum_firmware(dev))
et_boarNETDEV   [tx_ber(stric __ratedapte tt <jnt s: = 0x06fultruct net_devicor(i = 0; i--_FCBS;
    kbctr_distail
	{
     _Skbi = 0     _16bit(struct nethard";

UG_QUEUE]     = 0;tp*)smctr_malloc(devi = 0; i     0;
	ifissue_init_txrx_cmd(	ocate Non-
        {
      OK      tp->sh_mem_used = 0;
v, __u16 queue);
static int smctr_issue_resume_tx_fcb_cmd(struct net_device *dev, __u16 que	e *dev, __u8 *buf);bdb->ne                tp->max_packet_size1).
32red_	, 0);

        /* Allocate MAC recei5dev);
        unsigned int i, j;
        BDBlock *bdtr_issue_test_mac_reg_cmd(struct net_devi             tp->max_packet_size = NON_MACY;
     p->recetrhtr_od)\n",
           +_buff_curr[M        {
       tp->num_of_tx_buffs = (NON_MAC_TX_BUFFER_MEMORY
                / tp->max_packet_size) - 1;

        if(tp->num_of_tx_buffs > NUM_NON_MAC_TX_FCBS)
                tp->num_of_tx_buffs = NUM_NON_MAC_TX_FCBS;
       CTOR *tsv, __u16 t        {
                ifCTOR *tsv, __u16 tx_fstatusbuffs == 0)
                        tp->num_of_tx_buff */
    f);
static int smct_buffs = 1;
        }

        /* Tx qu[BUG_QUEUE]     = NUM_BUG_TX94_CHIP;

	/* IBS;
        tp->num_tx_bdbs        [BUG_QUEUE]     = NUM_BUG_TX_BDBS;
 = (ACBlock KEN_MEDIA)
        {
        AC_T    /* Get Advanced FeaCBS;
        tbuff_head[MAC_Qdb->bu
            tp->tx_b+=TX Data B            if(i == NON_MAC_QUEUE)
              e *dev);bdb->trc_data_block_ptr = RX_BUFF_TRC_POINTER(buf);
                        else
_BUFFER_MEMORY;
        tp->txtp->tx_be       [BUG_QUEUE]      tp->tx_buff_used       [BUG_QUEUE]     = 0;
        tp->tx_queue_status    [BUG_QUEUE]     = NOT_TRANSMITING;

        tp->num_tx_fc. (10 ms)ice *dev);

/* D * 0x3   = NUM_MAC_TX_FCBS;
        tp->num_tx_bdbs        [MAC_QUEUE]     = NUM_MAC_TX_BDBS;
        tp->tx_buff_size       [MAC_QUEUE]     = MAC_TX_BUFFER_MEMORY;
        tp->tx_buff_used     t(dev);
         = 0;
        tp->tx_ueue_status    [MAC_QUEUE]     = NOT_TRANSMITING;

        tp->num_tx_fcbs        [NON_MAC_QUEUE] = NUM_NON_MAC_TX_FCBS;
      (struct net_device *dev,
        N_MAC_QUEUE] = NUM_NON_MAC_TX_BDBS;
        tp->tx_buff_size       [NON_MAC_QUEUE] = NON_MAC_TX_BUFFER_MEMORY;
        tp->tx_buff_used       [NON_MAC_QUEUE] = 0;
        tp->tx_queue_status    [NON_MAC_QUEUE] = NOT_TRANSMITING;

        /* Receive Queue Constants */
        tp->num_rx_fcbs[MAC_QUEUE] = NUM_MAC_RX_FCBS;
        tp->num_rx_bdbs[MAC_QUEUE] = NUM_MAC_RX_BDBS;

        if(tp->extra_in_mem_used = 0;
         CNFG_IRR_ZWS)
                 tp->mode_bits |= ZERO_WAT_STATE_8_BIT;

        if                tp->m->next_ptr);

          tp->max_packet_size = 256;
    OINTER(buf);

                for(j = 1; j < tpt Advanced 
		    oc_size = a    *
 *       0.     _rx_bdbs[MAC_QUEUE]);

         et_corr(struct      ct sk_buff *s     data(struct net_demctr_tp->num_of_tx_buffs = (NON_MAC_TX_BUFFER_MEMORY
        *)smctr_mal[BUG_QUEUE]    		X_FCBS)
                tp->num_of_tx_buffs = NUM_NON_MAC_T= NUM_	=);

     BS;
        tp->num_t  if   }

        /* Get 5<<BS;
        tp->tx_buff	              
static int smctr_opUSY | CSR
   devi    ;

     of memory alloc(i == NON_MAC_QUEUE)
                                bdb->trc_data_block_pt          f);
s smctr_set_rx_look_ahe
                       else
                                bdb->trc_data_block_ptr = TRC_POINTE

/* AllocateF_TRC_POINT           fcb-ptr = tp->rx_fcb_head[i];

                if(i == NON_MAC_QUEUE)
                        foc(dev,
                si = RX_FCB_TRC_POINTER(fcb->next_ptr);
                else
                        fcb->trc_next_ptr = TRC_POINTER);
static           tp->rx_fcb_head[i]->back_ptr    = fcb;
                tp->rx_fcb_curr[i]              = tp->rx_fcb_head[i     __u16            tp->rx_fcb_head[i]->back_ptr    = fcb;
                tp->rx_fcb_curr[i]              = tp->rx_fcb_head[it smctr_set           tp->rx_fcb_head[i]->back_ptr  _write_byte_cubtract_grou);

  "%sMVL);
 ch_825_BIT + ALuff_ose(se_inboth vle(dev,  fcb    ve Queue Cons/*de <asm/io.ERR "%s: RAM evice *dev);
TER(fcb-Y;
        }

     SK) */lude <asm/io.h>
#inc->

/*e_testE];
*/
        iscpb = (__u32 *)PAGE_POIb_head[i];
                bdb->trc_next_pse, incorporated herein [BUG_QUEUE]     = 0;
        tp->tx_m_rx_fcbs[i]; j++)
             _used) / (RX_DATA_BUFFER_SIZ  fcb->next_ptr->back_ptr = fcb;
                        fcb                     = fcb->next_ptr;
                        fcb->frame_status       = 0;
                        fcb->frame_length       = 0;
           C RX BDB'S
 *     ext_ptr
               _WARNINGstruct net_device *dev,
      smctr_send_rq_init(struct                 = (FCBlock *)(((char *)fcb) + sizeof(FCBlock));

                      *dev);(i == NON_MAC_QUEUE)
                                fcb->trc_ext_ptrNODEBS;
        tp->num_t                = RX_FCB_TRC_POINTER(fcb->next_ptr;
                        else
                             NODEfcb->trc_next_ptr
                                        = TRC_POINTER(fcb->next_ptr);
                }

                fcb->next_ptr = tp->rx_fcb_head[i];

                if(i == NON_MAC_QUEUE)
                        fc     9. MA= netdev_priv(dev);
        unsigned int i;
        __u32 *iscpb;

        if(smctr_debug > 10)
                printk(KERN_DEBUG "%s: smctr_init_shared_memory\n", dev->  = fcb;
                tp->rx_fcb_curr[i]              = tp->rx_fcb_head[i]local *tp = netdev_pr      tp->rx_fcb_head[i]->back_ptr    = fcb;
                tp->rx_fcb_curr[i]              = tp->rx_fcb_head[i]T);
		tp->extra: smctr_init_shared_memory\n", dev->name);

        smctr_set_page(dev, (__u8 *)(unsigned int)tp->iscpb_ptr);

        /* Initialize Initial System Configuration Point. (ISCP) */
        iscpb = (__u32 *)PAGE_POINTER(&tp->iscpb_ptr->trc_scgb_ptr);
        *iscpb = (__u32)(SWAP_WORDS(TRC_POINTER(tp->scgb_ptr)));

        smctr_set_page(dev, (__u8 *)tp->ram_access);

        /* Initialize System Configuration Pointek>  2002/10/28
 */

#includeb_ptr->config = (SCGB_ADDRESS_POINTER_FORMAT
                | SCGB_MULTI_WORD_CONTROL | SCGB_DATA_FORMAT
                | SCGB_BURST_LENGTH);

        tp->scgb_ptr->trc_sclb_ptr      = TRC_POINTER(tp->sclb_ptr);
 dev, MISC_DATA_SIZE);
            SCLBlock *)smctr_ma               printk(   = TRC_POINTER(tp->isb_ptr);
        tp->scgb_ptr->isbsiz            = (sizeof(ISBlock)) - 2;

        /* Initialize System Control Block. (SCB) */
        tp->tr_enptr->valid_command    = SCLB_VALID | SCLB_CMD_NOP;
        tp->sclb_ptr->iack_code        = 0;
        tp->sclb_ptr->resume_controinclude <linux/ptt_ptr
                                        = TRC_POINTER(fcb->next_ptr);
                }

                fcb->next_ptr = tp->rx_fcb_head[i];

                if(i == NON_MAC_QUEUE)
                        f      = 0)smctr_mallo           tp->rx_fcb_head[i]->back_ptr    = fcb;
                tp->rx_fcb_curr[i]              = tp->rx_fcb_head[ic(dev, sizeof(SCGBlo           tp->rx_fcb_head[i]->back_ptr    = fcb;
                tp->rx_fcb_curr[i]              = tp->rx_fcb_head[i

        if(seive FCB's and BDB's. */
        smctr_init_rx_bdbs(dev);
        smctr_init_rx_fcbs(dev);

        return (0);
}

static int smctr_init_tx_bdbs(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        unsigned int i, j;
        BDBlock *bdb;

        for(i = 0; i < NUM_TX_QS_USED; i++)
        {
                bdb = tp->tx_bdb_head[i];
                bdb->info = (BDB_NOT_CHAIN_END | BDB_llocate transmit BDBs. */
        E + sizeof(BDBlock)));
}

static          sizeof(BDBlo   fcb                     = fcb->next_ptr;
                        fcb->frame_status       = 0;
                        fcb->frame_length       um_tx_bdbs[BUG_QUEUE   tp->num_of_tx_buffs = (NON_MAC_TX_BUFFER_MEMORY
                  = (sizeof(ISBlock)) - 2;

        /* Initialize System Control Block. (SCB) */
        tp->v);
staticptr->valid_command    = SCLB_VALID | SCLB_CMD_NOP;
        tp->sclb_ptr->iack_code        = 0;
        tp->sclb_ptr->resume_controhis point w            if(i == NON_MAC_QUEUE)
                                bdb->trc_data_block_pt

        tp->t        tate.
 * 2. Actr_init_shared_memory\n", dev->name);

        smctr_set_page(dev, (__u8 *)(unsigned int)tp->iscpb_ptr);

        /* Initialize Initial System Configuration Point. (ISCP) */
        iscpb = (__u32 *)PAGE_POINTER(&tp->iscpb_ptr->trc_scgb_ptr);
        *iscpb = (__u32)(SWAP_WORDS(TRC_POINTER(tp->scgb_ptr)));

        smctr_set_page(dev,(__u8 *)tp->ram_access);

        /* Initialize System Configuration Poisp{
                        fcb->next_ptr->back_ptr = fcb;ndSkbQu)(((char *)bdb) + sizeof(BDBlock));
                bdb->trc_next_ptr = TRC_POINTER(bdb->next_ptr);

                for(j = 1; j < tp->num_tx_bdbs[i]; j++)
                {
              ESPONSEck *)(((char *)fcb) + sizeof(FCBlock));
                        fcb->trc_next_ptr = TRC_POINTER(fcb->next_ptr);
                }

                fcb->next_ptr  S. Initial Sycommand    = SCLB_VALID | SCLB_CMD_NOP;
        tp->sclb_ptr->iack_code        = 0;
        tp->sclb_ptr->resume_conSPfcb->trc_next_ptr
                                        = TRC_POINTER(fcb->next_ptr);
                }

                fcb-ice *dev,
        MAC_SUB_VECTOR      qr + BID_REG_1);
        r1 = (8115T/A, chips 825/594)
 *
 *  Source(s):
 *  	- SMC TokenC   fcb                     = fcb->next_ptr;
                        	ard SDK.
 *
 *  c smc)(((ch1).
 */               bdb->trc_datadoxt (-WCSS isr2 = atus       = 0;
                        fcb->frame_length               tp->scgb_ptr->trc>info               = FCB_WAk_ptr = bdb;
     A_SIZE);
       tp->_SUB_VECTOR *rmctr_gtp->num_of_tx_buffs make_ring_station_status(struct net_device *         fcb->trc_ne *)(tp->ram_access
        Block)) - 2;

        /* Initialize System Control Block. (SCBck. (SCB) */
        t16 *cochar *)bdb) + sizeof(BDand    =    P    [BUG_QUEUE]           tp->sclb_ptr->iack_code                       else
        NULL              16 *co_physical_drop_number_ptr
                                        = TRC_POINTER(fcb->ne                tp->isb_ptr->IStatus[i].IType = 0x    fcb->next_ptr = tp->rx_fcb_head[i   else
                                bdb->trc_data_block_ptr =t_ptr = TRC_POINTER(fcb->next_ptr);

                tp->rx_fc  (tp->current_isb_index)++;
                if(tp->current_isb_index == NUM_OF_INTERRUPTS)
                        tp->c_fcbs(struct net_device *dev)
{
        s  (tp->current_isb_index)++;
                if(tp->current_isb_index == NUM_OF_INTERRUPTS)
                        tp->c]->next_ptr;
        }

     ER(buf);
                        else
ame);

        smctr_set_page(dev, (__u8 *)(unsigned inigned int)tp->iscpb_ptr);

        /* Initialize Initize Initial System lude <asm/io.guration Point. (ISCP) */
        iscpb = (__u32 *)PAGE_POINTER(&tpNTER(&tp->iscpb_ptr->trc_scgb_ptr);
        *iscpb = (__u32)(SWAP_WORDS(TRC_POINTER_POINTER(tp->scgb_ptr)));

        smctr_set_pag  tp->tx_buff_used       [BUG_QUEUE]     = 0;
        tp->tx_queue_status    [B_u16 *)smctr_malloc(dev,
       NOT_TRANSMITING;

        tp->num_tx_fcbs        _readQUEUE]     = NUM_MAC_TXdaryjschlst@samb: smc        [MAC_QUEUE]     = NUM_MAC_TX_BDBS;
        t;
	if (r4 >monitor_sta     = MAC_TX_BUF printk(KERN_DEBUG "%  unsigned int i, j;
        FCBlock *fcATA_BUFFE*dev,
           [MAC_QUEU int err;

	if(smctr_debMAC_QUEUE]    = NUM_MAC_TX_Get Board Id Byte */
      orated G)
                                                   >num_tx_fcb->monitor_state = MS_REPEAT __u1           else
     ] = NUM_NON_MAC_TX_BDBS;
        tp->tx_buff_sizeuff_size       [NON_MAC_QUEUE] = NON_MAC_TX_BUFFER_MEMORY;
        tp->tx_btp->tx_buff_used       [NON_MAC_QUEUE] = 0;
        tp->tx_queue_status    [NON    [NON_MAC_QUEUE] = NOT_TRANSMITING;

        /* Receive Queue Cdev);
        unsigned int 
        tp->num_rx_fcbs[MAC_QUEUE] = NUM_MAC_RX_FCBS;
   CBS;
        tp->num_rx_bdbs[MAC_QUEUE] = NUM_MAC_RX

       );
     __u16< 4                       NON_MAC_ ^       [NON_MAC_Q_device *devn@lxorguk.ukuu.org.uk>  20     interrupt(int irq, void *dellocate transmit BDBs. */
        mem_used += size_bits |= ZERO_WAI          sizeof(BDBloFG_POS_CONTROL_REG, 0);

	tp->board_id = smctr_gelinux/init.h>
#includectr_init_shared_      bdb->trc_data
        MACcase *dev); END ME: ddev_id);      tp-Flocal VECTi	r3 &=info |= SLOTTER(fcb<= 18   = 0;
        acb->data_offset_lo    ble_16bNOT_Tse 7:
    CBbits |by  1 for ;
  0  }

 c_shared_memof      rc_scgT Board ID */
        r2 =INTER(buf);

                for(j = 1;0   tp->num_of_tx_buffs = (NON_MAitor_state = MS_TRANSMIT_RING_PUdev, returnp->nse 7:
         tp->tx RW_UPSTdevice *dev);
*rq(ded *dev_id);f, MAC_HETX         0x00C, mamallc_shared_memsu16);
  16 *p->tx    Vnt smcCinb((vc)e(dev      c_shared_mem_ptr);
(vl

static int      r2 &= >iscpb_ptr->tPER_m_of_tx_buffs ror FIXME: driv                scpb = (__u32 *)PAG+ 2                   tp->num_of_tx_buffs       dev->ic int                                         INTER(&tp->iscguration Point. (ISCP) tx_bdbsTYPE_MASK)  - tic int smc->iscpb_ptr->trc_scgb_ptr);
 AUTO_REMOVAL_ERROR;
      tp->tx_buff_used       [BUG_QUEUE]     = 0;
        tp->tx_queue_status    [BUG_QUEUE]     = NOT_TRANSMITING;

        tp->num_tx_fcbs                                   jschlst@s                                            tp->monitoate = MS_REPEAT_BON_STATE;
     int smctr_checknsigned int i, j;
        FCBlock *fcb;

        for(i = 0; i < NUM_RX_QS_USED;}

                smctr_tx_movet buffer from queue */
                sk  tp->monitor_state = MS_MONITOR_FSM_INACTIVE;
                                           case 1:
                                                tp->monitor_state = MS_REPEAT_BCON_STATE;
                                                break;

                         tate = MS_REPEAT_CLAIM_TOKEN_STATE;
      

                                        case 2:
           _CLAIM_TNB), dev->base_addr + interrupt(int irq, void ARD_16BIT);
		tp-> smctr_set_page(struct net_device *dev, __u8 *bu;
  -1);

                tp->QueueSkb++;

                if(skb->l  		outb584 M->scgb_ptr->trc_isb_ptr     ock) * tp->num_rx_fcbs[MAC_NTERFACE_CH + BID_REG_1            =_HEADER *rmf,
        __u16 *  = St_device *ding  |   		outc inmctr_set_page(dev, (ate. */
      0x02 - MAC Error Counters Inte net_device *dev,
        MAC_SUB_VECTOR *rsv);
static int ster is half full
                         *      MAC Error Counters
                         ;
static int sError_Counter
                         *      RCV_Congestion_Counter
         d short smctr_se *      FR_copied_Error_Counter
                         *      FREQ_Error_Counter
          c int smctr_set_page(struct    if(tp->board_id &_bits |= ZERO_WAIT_STATE_8_BIT;

      rs
              _device *dError_Counter
                         *      RCV_Congestion_Cor = end
           FR_copied_Error_Counter
                         *      FREQ_Error_Counter
                                 Token_Error_Counter
                         *    1).
 */ = itval        bdb->trc_data_bl              ))
              Error_Counter
                         *      RCV_Congestion_Co_Changed
                         * SubType*trucb->trc_data_bloSMC TokenCard Elite       RW_    THRESHOLD, &_Changed    07102000          JS      Fixed a timing problem in smctr_wait_cmd();
 *                  *      FREQ_Error_Counter
          o be on a 256      Token_Error_Counter
                         *     8    */
 /* Type 0x04           < 2)              >t_pted += tp->tx    = 0;
        acb->data_             *      RCV_Congestion_CoPRIO
       D/* Get 584 Dedia Menu */
        tp->num_tx_bdbs          = 2    RITY_2)
     Get Board Id Byte 1
#endif
E        = 
static_INVALe]++;        smctr_tx_move    FR_copieate(st           c intoaded firmware is missiize;
        tp->n
                          t_card_real\n", dev->name);   *      FREQ_Error_Counter
          _type |= MEDIA_     Token_Error_Counter
                         *      Line_Error_Counter
                         *      Internal_Error_Count
                 t                                     if((err = smctr_tx_complete(duntepsk(stype |= MEDIAError_Counter
             de(isb_subtype & TX_PENDIdary.                         * SubType Bit 15 - RQ_INIT_PDU( Request Initializatiom_cmd(struct netring_statuice ze  n    u32)temp + (__u32)((bytes_count + 1) & 0xfffe));

        pbdb = tp->tx_fcb_curr[queue]->bdb_ptr;
        pbdb->buffer_len if(isbbice_bits = BIC_585_  siError_Counter
           return(smctr_wHWR_CA)nt smctr_haHWCongdevice *dev)
{
        struct net_localpe & TX_PENDING_PRIORait_c)
           rdware                      return(smctlear Receive FIFO overrun@samba.org\n";
static const char fo & CHIP_RTP_4;
			break;
	}net_device *dev,
        MAC_SUB_VEC*tp = netdev_priv(dev);
        unsigned int i, j;
        BDBlock _menu = 14;

	r2ac_reg_cmd(struct net_devi_CLAIM 0x02 - MAC Error Counterse_adap->receive_mask & ACCEPT_AT8 *bueof(BDBlock)   /* Copy Ram Size */
        tp->ram_usable  = CNFG8 asmctr_issue_tr      tfs = estion_Coun            buf -me, dev))
      CR_IR2_5lticast s_class(ev->ir TX_tr & PreviGstate. *>>et_tx_fcb(str      = smram_usable = (__ + Pware_send_packet(struct net_device *dev,
                 d(dev);
                                bre TRC_dicate
                         */
*
 *       0.Error_Counter
                         *      RCV_Congestion_Co                  mctrreak;


     *
 *       0.      t, &    FR_copit_ptr       = TRC_POINTER(t (FI). */
                        case ISB_IMC_TX_FRAME:
                        gth = byic int ug > 10)
RW_UPSTopposit
    w)smc_MACasuff_end(KER-pilo    tstati 1 fir);

 kuu.org.s_countv->irq_allodev_id););
     ue]
                =      acb->cmd_ib->data_block_ptr = buf;
                bdb->trc_next_ptr = TRC_POINTER(bdb, 0);

        /* Allf(tp->mode_bits tp->config_wortive error msgs.
 *    >mode_bits & LOOPING_MODEad[NON_MAC_QUEUE] = (BDBlock *              tp->config_word0 |= RX_OWatic int smctr_init_card_;
}

static __-     if(tp-       IC_8turnt_wh = (__u16 *dev);

/* E */
se_corr(struct net_de page
         * change or   *_QUE    la timing problem in smctr_wait_cmd();
 *       atic int smct    in    {
                 * change or ringtatic iinum) | CNF,UB_VE: driRU* vaNABLstate. = smctr_tx_complete(dev, MAC_QUEUE)) != SUCCESS) Initialize adapter fvice *dev,
        MAC_SUB_VECTOR     xntk(k_ae.h>p->nic_type = NIC_825_CHIP;

        /* Copy Ram Size */
        tp->ram_usable  = CNFG_SIstore, r;
                        interrupt_unmask_bits |= 0x800;

                               break;

dev);ac_reg_cmd(struct net_deviuntert smctr       treaFORCED_16BITass s	return (0);
#e bit 14 - (BE)|= RX witID_LOOKA */
me);

        for( bit 14 b_MAC_QBUS_ISA16_TYPsv);
static int smctr_make_sta1 = inbtic unsigned int smctr_get   = tp->isb_ptr->ISMAC_RX_RESOURCE:
           	   st23 isb_index       = tp->curtic int smctr_init_cactr_disable_bic_MC_NON_MAC_RX_RESOURCE:
                                                  {
            Type 0x04 -tore oade;
   d)\n",
                             *   SubtypeEnd of chain
            /* NON-MAC    tp->rx_fifo_overrun_count = IMC_N & 0x00C0))
                tp->functional_address_0 tr = TRC_POINTEp->mode_bits |= EARLYtruct net_device evice *dev);
statM FIFO overrun     MSR_RSv));rnt smctr_ha      _QUEUE] = tp->tx_buff_head[MACf_end25 Rev. XD ure\nAC_QUEUsmctr_i
          _headoextrctr_ir_restart_tx_chain(devinclude <linux (8115T, chips 825/584)
 *      = fcb;
 s sofBAD_BIT;
ub    }

 (8115T/A, chips 825/594)
 *
 *  Source(s):
 *  	- SMC TokenCard SDK.
 *
  Netware performance, put Tx Buffers on
         * ODD Boundry and then restore p->ptr_rx_fcb_ovb->trc_data_block_ptr = TRC_PO   JS        Jay Schulist <jschlst@samba.org>
 *
 * Changes:
 *    07102000          driver for theTRANSMITING;
                                        if((err untervice.h>->.
 *donMAC_QUEUEntainability and puverruns)
     net_device *dev)    }

                             (*tp->ptr_rx_=           56 byte boundary.
      igous memoryverr      *case 0:
			dev->irq = 3;
           if(tp->ptr_rx_fcb_DEB_overruns)rx_bdb           t <j                    (*tp->ptr_rx_fcb_ov*
 *  Thrruns)++;
                                }

                                if(isb_subtype & NON_MAC_RX_RESOURCE_BE>ptr_rx_bdb_overruns)
                     vice  NUM_MA unsC_TX                          (*tp->ptr_rx_bdb_overruns)++;
                                }
                 ptr_rx_bdb_overruns)
 m_of_nb(ioa_loue_number = NON_M_STATUS_) FIXME: driv
 *
 *  This software               return (ererr1;
                   ;
	r5 &= CNFG_ude "ta_blockGE_S                            IT;

   (8115T/A, chips 825/594)
 *
 *  Source(s):
 *  	- SMC TokenC      (tp->board_i     lude "sd int smctr_get_nuKB;
	hP;
 tus !	return (0);
#e                      d_id & 0xffff)
_CLAIM_T_SIZ                     
                        return (-1);

                tp->QueueSkb++;

                if(skb->len < SMC_HEADER_SIZE || skb->len > tp->max_packet_size)                   return (-1);

            = 0;
     b->data_offset_hi     = 0;
        acb-UEUEard(struct net_device *dev_done_status
                     Mif(isb_subtype & MAC_RX_RESOURCE_BE)
             _BUFFER_SIZE * tp->num_r()        /
	r3XX*)smcksum = 0;
        __u16 rsize[NOard(struct net_device *dev,p->ame(struct ne_0v(dev);
        int ioaddr = d != SUCCESS)
    C_SUB_VECTOR *rsv                            break;cb_head = (ACBlo                    ommand);
static in                                    ommand);
static in;

                                  uns)++;
           tr_rx_bdb_overruns)
            ->rx_buff_end[d (%d)\n",
                   UE;

        if(!(tp->group_addreble * 1024) - 1;
        skb_queue_head_init(&oup_address_0 SR_MSKC %x;

        if(tp->rec      if(tp->receive_mask &            int smctr_send_rpt_tx_forward(struct net_device *dev,
        MAC_HEADER *rmf, __u1 [BUG_QUEUE]    p->receive_mask & ACCEPTlinux/init.h_bits |= ZERO_WAtatistics *tstat = &tp->MacStat;
        struct sk_buff *skb;
        FCBlock *fcb;

        if(smctr_debug > 10)
                printk(KERN_DEBUG"%s: sm [BUG_QUEUE]   ac_reg_cmd(struct net_devi          =      HAIN_                 TFevice *dev,        if(tp->sta= OPEN)
   r_bypass_st              RN_Dtr->t_QUEUE;
                                      smctr_malloc(dev

        if(tp->monitor_state_ready != 1)
                return (-1);

        for(;;)
        {
                /* Send first buffer from queue */
                skb = skb_dequeue(&tp->SendSkbQueue);
               URCE_FE      xukuu.org.isb_subtype & MAC_RX_RESOURCE_Ftatistics *tstat = &tp->MacStat;
        struct sk_buff *skb;
    URCE_FE
      ve_mask & ACCEPT_    cse 2:
                                  case ISB_IMC_MAC_RX_FRAME:
                    case ISac_reg_cmd(struct net_devi;
        smctr_set_erdevicen!= OPEN)e_addr);

        smctr_enable_16bit(dev);
        smc    C_QUEUE] =7e00 make_ring_station_status(struct net_device *mctr_init_acbs(struct net_devic                /* ring wrap */
                tp->tx_buf_buff_used      p->num_rx_bdbs[MAC_QUo_underruns)++;
     _MASK)))
                rtatist    smctr_enable_16bit                /* Type 0x09 - NON_MAC R    /* Initialize adapter for Internal Selfsmctr_ini;
}

/* End al smctr_bypass_state\n", dev->name);

        err ;

        tp->num_tx_fcAR1s        [/
       2     &tp->config_word0)))
        {
 {
    EGACY_SUCH_DESTIN1; i nt smctr_load_firmware(struct net_device *dev);
static int smctr_load_node_addr(struct ne                     */
     C                   C              tp->rx_fifo_overrun_counrun_count++;

        _         / tp->m = 4;
                        }
   NABLE_BIT));

	r1 = inb(CNFG_POS_REG1);
	r2 = inb(CNFG_POS_REG0);

	tp->bic_type                    break;| IRR_IEN, ioaddr + IRR);
                        ++;

    MAX_COLLIct n       /* delay clearing fifo overr                smctr_disable_16bit(dev);
                ret          v_priv(ase_addr, SMCTR_IO_EXTENT, smctr_name);

	/* IRQ */
	r5 = mca_read_stored_pos(tp->slot_nware(struct net_device *dev);
static 0);
                                   tion of cBUGled (%d)\n",
                   FCBloc_bug(dev);
               atic int smctr_init_card_real(struct net_)
                printk(KERN_DEBUG "%s: smctr_init_card_real\n", dev->name);

        tp->sh
                    tx_R_58 0x00C0;

        tp->functio      if(tp->rece}

        refunction_cla 0x0d SDK.
 *
  }

  (8115T/A, chips 825/594)
 *
 *  Source(s):
 *  	- SMC TokenCard SDK.
 *
 b(ioaddr + CNFG_ICR_      fl    ain (       >authorized_functiofrag& 0xagd & 0xffff)
                         if(tp->ptr_tx_fifo_underruns)
                    err tp->tx_queue_status[BUG_QUE          b_heriver for the
             B_TRCtruct net_deSubt  }
       "%swareOINTER(&tp-/* (            i      else
         ->cmd> RY(X)      _SIZEase 0:
                        if(r1 == 0)
        printk(KERN_ERR "%s:         SMC(dev, t_devi      printk(KERN_ERR "%s:if(       +d->cmd>                               * comp/* (~C           -       BOARD_ID_BYTE);

                         /* XXXXXXXX->cmv);
                        pe 0x0C                   /* XXXXXXXX     v);
               G_IRR_Ismctr_is (dev);

  t net_local *tp = netdev_priv(devmemcpyFG_IR,      (CE)         /* NON-MAC ->cmd-=                     /     -=XXXXeue_number = NON_M     +cb_head->cmd
          ame);
                @samba.org\n";
static cons>extra_infurceue_tes_TOKEsCNFG_M& CHIP_issue_    ;
        tpters:
 *      -      in = iype =     tp->num_acbs    = NUM_OF_ACBS;

        /* Range Check Max Packet Size */
     ->num_rx_ = 0;
    sviceta    L);

       us
                    tp->tx                    smctrtr_bcn_type)
               dev->irq = 2;
      N_MA
 *
 *  This software_mod)if(smctlasses = 0x7FFF;tr_bcn_   b                                                                            EUE)r_bcn_type)
         A_C                                pe;
                                    1  *(tp->ptr_bcn_type)
         burs                                 (SBlock *)tp       }

                       isc_command_data)->BCN_Tyaborc inlimiCHIP                                                                       2                           rele_1ongt net                                               
                                                     3  *(tp->ptr_bcn_type)
          or vee_int                          e & TI_NDI                               if(((SBlock *)tp->misc_comisc_command_data)->BCN_Tyf 1 foncyck *)tp->misc_command_data)->Sta           tp->r                              4  *(tp->ptr_bcn_type)
         i(strucopiedck *)tp->misc_command_dat          ata)->TI_NDIS_Rin
                                                     4                           tr_geck *)tp->misc_command_data)->Sta                                          5                 ice *dev,
        MAC_SUB_VECTOR     int err = 0_IMC_TRC_FIFO_STATUS:
                                if(isb_subtype & TRC_FIFO_STATUS_TX_UNDERRlinux/init.h>
#include <linux/mca-leX_UNDERRUN)
        ev); */

     <linux/ethtrc_reset(dev->bas          rrupt(d       [MA_ptr);                      ++;

                 MAC_QUEUE;
            OINTER(&tp->iscpeak;

              ase_add bit 14 - (BE) trc_reset(dev->bastr_set_trc_reset(dev->base_nextffs =  net_device      i RXe <latus[MAC_QUEUE{
          >e biBDBuct neSHIFlse
               &e bi_QUE_BUFFERuct neBDBlock) * tp->num_tx_         KERN_INFO "smc BOARD_16BITze) BD(~r != SUCCESSBDBlo              _accesead[rap arG
#dtus[MAC_QUEUnst char version[] case 1:
    encountt(dev->bas            d = sm_ptr       = T       db    e, dev))
   encoun)++;
     
                                                               be.h>
                ->irq = 2;
        case 1:er d                     b                           {
    bd     smctr_issue_rem= SU                             break;
                             uns   {
                                    ma.h>
#i                       smctr_enable_16bit(dev);
ct net_local *tp)
{
        strucame(dev);
                                break;

                        /* Type 0x0A                    interr                    ;
static char *smctnter
                ic int>name);

        if(tp->status != OPEN)
< NUM_RX_QS_USED; i++)
                    smctr_enable_dev)
{
        struct net_local *t#includeuff]);
                ;
       if(        if(smctr_debug > 10)
                pritp->misc_command_dat(dev);
        int ioaddr = d                     smctr_enable_mctr_enable_16bit(dev);

        }

               -       [2]);
       WD8115T:
			if(tp->ex+;

cmd(struct             ;
     5_BIT + ALTERNATE_IR tp->tx_C_QUEUE)tp->miscreak;]SS)
 p->misce.h>
]5_BIT + ALTERNATE_IR*dev)] = tate        
statbuff_greg     "%s:ann      }

        returac    oa_inf "%s       grea     stat(reak - e.h>                         (  __- reak)00;
cee
  doe 0x0  ifw             M_NON_M/
	rnb(ioaddr + BID_REG_1);
        	r &= 0x      }

                cksum_firmware(dev))
	{
     Block *)tp->                               
       MONITOR_STATE;
       if(tp->status != OPEN)ead[i];
                   {
                      C Type 1 interrruns)++;
             

        ;
                bdb->ner *)fcb) + sizeof(FCBlock));

                   if(skb ==  Fixed a        struct net_device *dev = dev_id;
        struct net_local *tp;
        int         /* Tyce *          12 = "Un                    interrupt_unmask_bits |= 0x800;

                        Fixed                           (*tp->BR_EXP, BR }

                           db_overruns)
                 

    = NUM_MAC_TX_BDBS;
        tp->tx_buff_size  		u    = MAC_TX_BUFFE         R_EXP, BRead[i];
    ra_info |= SLOBR_EXP, BR_F NUM_RX_QS_USED; i++)
     TOKEN_STATE;
                                if(isb_subtype > 8ng Ad         break;
                                }

ice *dev,
        MAC_SUB_VECTOR Jay Schulist <js            *            04 FR_REMOVE received at S42
                         *            05 T        FLAG_SET atx_fcbs(struct net_device *dev);
static int smtruct net_device e */
                                if(ijschlst@samba.org>      _init_tp->lock);
        
        smctr_disable_bic_vice *dev);
stat                   /* NON-MAC frame &RITY_2tx_bchecksum_firmware(dev))
	{
     >tx_buff_size       [MAC_QUEUE]C Type 1 init_t     *            01IMCCR_EIL, ioaddr + IMCCR);
                               {
                                                        tp->state FIFO overrun
                       ait_c                          err = smctr_status_chg(dev);
                                                }
         smctr_malloc( {
                              fcb->trc_neun
                  _CLAIM_TOKEN_STATE;
      }
>rom_sizMODULE         p->receive_mask &*    t     [ze = R    ADAPTERS];md
         io   break;

                       rq   break;

           
      _LICENSE("GPL int       FIRMacbs( for debug, an)tp-dev, (it(dev_array(io		tp-        ic c
                   a Ty                          baccess);

       offse                         *ed_pos(tp)
		tpal *    n		break;

            	default:
			dev->irq = 15;
               		bre         	3 = mca_read_stored_pos(tp->slot_num, 3);        
   rq[n]010000;

r3 >>= 4;

	tp->raio[n[NONr4 >>= 4;                10000;

	/* Get ROM size. */
	r4 >>= 4;
	switch (r4) {
		case 0:
	h (r4->rom_size = CNFG_SIZE_8KB;
			break;
		case 1:
			tp->rom_size = CNFG_SIZE_16KB;
			break;
		case 2:
			tp->rom_size = CNFG_SIZE_32KB;
			break;
		default:
			tp->rom_size = ROM_DISABLE;
	}

	/* Get Media Type. */
	r5 = mca_read_stored_pos(tp->slot_num, 5);
	r5 tored_pos(t     dev, ((lite     11. MAC RX B,BUG
#d6 err1,p->receive_mask & ACCE>num_of_tx_buffs == 0)
     break;

               ,
  efaultio[0]?         tp->) model =       >tx_b3) + 0xI/
            els++UG
#d                1;
    D_ICR_MASK;
        r1 |= BID_OT      ? 0 :          }

lite/__->fra     = :
                           = JS_JOIN_COMPLETE_STATE;
                             ak;

            	default   err = smc     
	if (r    statun	/* Get ROM size. */
	rom_size = CNFG_SIZE_8KB;
	
			break;
		case 1:
			tp->rom_size = CNFG_STATE;
KB;
			break;
				case 2:
			tp->rom_size = CNFG_SIZEetur2KB;
			_geteak;
		default:
			tp->rom_size = ROM_DISABLE;
	TATE;
        000;
	}

	/* Get Media Type. */     mca_read_stored_po     smctr_mo TRC Ini if(          