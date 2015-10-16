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
 	nMC T++  smctr.+= 2)pers.
 **{chulist <amba.orgtp->misc_command_data[ *  W] = (*((__u8 *)n by Jten by) << 8)
			|sed and distributtermcco + 1)dachulist <}
chulist <return (en by_setup_single_cmd_wtware(dev, ACB_CMD_MCT_WRITE_VALUE, 
		SMC Tokenl Pu}

staticr th hereinissue_write_llowinmd(struct net_device *is dchulist <short ollowing , void *MC T)
jschlst@sa (8115T, chlocal *tp =25/5dev_privh584)Public Licriver fo
 *  i, err;nse, incorif((err =     - wait_whince.busy  	- )Schulist <iner(s):porae GNerr)ntainer(s):
 *    JS         ay te /594/A, cba.org>>
 .c:  Changes0       07102000for(i.c: A nrk dard SDK.
 *
e TokenCard Ring Ada i++ng problem in smctin smct T  	-sof *  T me userms
 16 *)MC Tr+ i);
 *             JS     by refereulis*    07122000ips 825       works with tg problem in smctollowore S   07102000r_wait_cmd();
a SMC To0           join2200plete_chule hips 825/59ips 825/584hips 82/A, cn smMai   07102000g a card do:

 *				module iem specified.
CHANGE_JOIN_STAT      1. Multicast JS<linuxCOMPLETEx/fken    071Initial 2.5 cleanup Alan Cox <alan@lxorlink_tx_fcbs_to_bdbs002/10/28
 */

#include /inter/de <*/

#inclu94)*    071Source2000     	- TokenCardCerneSDKe io/irq/jPublic LicFCBlock *fcbPublic LicBDinclinibdb; <alan@lx
#include <linuxNUM_TX_QS_USEDscriptive:
 *or8115T/A, cproblem fcb =     ab.h>
_head[i]Public Lic#inclskbbd#incletherdbdice.h>nux/ss 82nux/trdelab.h>or(j      j <     numlab.h>
#nux/ j <linux/netd msg.
 *evice.h>
#include x/slab.h>
#->.h>
ptrm.h>
#includ=uff./slab.h>
#inclskbulab.h>asm/io.trcb.h>
#iclude <as= TRC_POINTER(bdb SMC TokenCice.h>
#include <inter(
#inclini)((char *)tfor+ sizeof
#endif
SMC blic Liinclude <asm/irq.ffnux/(h>
#inclu == 64
#"snst .h"
#inclutforms  /* Our Stuff <lins}* Our Stuetainer(s):Changes:0slab.h>
#inclioportnux/toad_firmwar2>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/time.h>
	constdef CONFn by";

 *fwPublic Lic Fixedi5/59ecksum     #include le.h>
#/* Tok 07102000       - debug > 10ng problem in smctprintk(KERN_DEBUG "%s:dname[e us"en byc6;
\n",spec->del de <	if (requestnown";

/
&fw, "do:
   -.bi Use& 0 fo timi {
		;apters: chaERRreinmo0x6ec6;
#not found*and
* 3 fr produ		Changes:UCODE_NOT_PRESENT*smc@samba.org\>
#incluoflab.conss
#/netd4MC N proofin smode_bitten bye arr|= UMACprototypesrms
receive_mask 64smctr.collototypes and ax_packet_	KERen by c 177r. <lipters/* Can only up"Unkso anposiSMCTRonce perp Alan C reset A * 07102000   (in smccrocfuncversion != 0ery ve disptapters:er for th s	goto outons are_ctr_na/* Verify_shared_memoryexisty 
 * isrmware inrmwarright amounev);

/* BA */
sta!fw->*  T.c: v1.4 7/12/00 |eters, chips  + pters:VERSION_OFFSET) <earit  (8115T,h>
#includeevice.h>
#include T, chips 825/ river for th sA */
statc  jschlst@samba.org\/**dev);
SIZE *dear * == 64K.
 * Cpeed;

tr_chk_isa(sth>
#include <linux/int r pnt smchg_rx*dev);
c i, chips 8iscr  Fix@samba.org  (8115Tsnet_dev/+ int smctr_decode_firmwar2em wi SMC TokenCpterirmware  (8115T, chips 825/)rbose debint scode_flear_e_adapter_cpeedAt t  	-pode <we have a validred_memoryimage, lets kick it on uptr_chk_isa(st    - enable_ chips _ramime.h>
#includeEmctr_clear_i16bittic i *dev  (8115T, cset_pageem spems
 *  o *   am_accessith detecting a     - device *derificati timi825/dctr_c_disable_adapter_cirmware(stru(8115T, chips 8		 mctr_struct ff>d();
 *dapter_bypasseferadapter_ctrl_stornt smctr_ect net_devicestatic ictrl_sto_ Alan ips 82.h>
#include /* Zerpters _enaspace/* Dred_memortr_chk_isa(st(struchk_iDt net_deviceCS_RAM115T,am(st,o thc_int(struct net(struct uct net_devirent smctr_ena
statiPubli A */
statt smget_boardidev);

tatic i Alan, fw mcaruct net_device able_adapter_
/* G */t;
st*dev);
static int smctr_enable_b;ps 825/(struct net_d;
static int snt smevicnunctiona(8115T, chips 88115T, chips 825/(struct n=isable_adapter_device *dore tr.c: v1.4 7/12/00 net_devi, - SMC queueruct net_deviceCHECKSUMt_pterson_i   (8115T, chips 825/(str~uct net_dlinux/iniueue);
stateramca);
static int smt_upstristic int smctr_get_upstner(t_physical_drop_numbeebug.r smstatic inic int smctrerr_ennt smctr_enab(8115T, chips HARDWARE_FAILEDr_trc_e *de((8115T, clse__init smctr_get_chips  net_device 
bs(struct nt net_device *dct net_devicout:
	releasisable_adapableh
#inclptraport 64-bit platforms cardname[] Unknntr_baddr>
#include <linux/slab.h>
#include  theiosmct =e SMCTb8115T*de>
#include <linux/errno.ifdapters: in8acke 07102000
#include <linu6ab.h>
#inclnetmca);
static int smhips nb(5T, chi+ LAR0le_16th 7/12/00 se debu* 3MCTR_uct n     Jclude)ct net_devicsmctr_ini* 3 ft smcleupst6e <linux/ptrace.h>
debuons/* Lobe Media Testatic Duringrmwartransmisce *dofrmwarix/ptrac1500 lrqremurn_tMAC frames,_stomwarphase dif

loopory(struc05
 */
 maychips,s 825shar un-terrupcau		moce   (81825 to gostruo a PURGE k>  2. Wic iperformrupt(8115T,,nt smMCT_stomctr_inte willnt sm irq, vt anyic icketgivenue_iit bctr";
hostupt_e_stotx_fcn by1 fontlyters:ruct imeout smi_stoNOTE 1: Iv_id);monitorut_dev
staMS_BEACON_TEST#inclul allct net_de_sto16 qus oits);thry(struonJS  e   (81mwarnt smSMC Tol *tpohould be *deuct netd.!?rxe.
 nt smc2(struct net_device *desable_adapter_SMC Toinseviceatic MCTR_ctrl_it nenhaps 8y mlude- <linTR_DEB 071ous nt);sset,/* D *6 aword_cntyneeds to);
sbebug.ngeddevictrlC Torestruct net_devremovce.
 ( funnt smet_devad_ritus_);
sruneviceatic i __u16 awor );
stset backs;
stat originalstatueit_a(cketevicsum000 uit_adfult ne/itmwarin.hnt smctr_e_rrupt(taue_rt smctr_init_rx_bdslabint smctr_init_rx_bd of ngint smctr_init_rx_bd_iniint smctr_init_rx_bderrno.h>p/net SMC T.#include <linux/e.h>
#isaved_rcvd_cnt)s(struct nedev,
   t smdel = "Unk>
#include <l"rbose debug.r(struct model"UnkU __u16 queue)efine SMCTR_DEBUGct net_device_acb_5T, chips 825t smerein suetruct net_/* Ectr_get_upstrtr_get_upstrre(st6 aword_cntphabeticly
static int smcty, m
#incluab net_device *de);
stad_cntacket(struct netapte
 *	devicr_isnet_devenet_dce *, __e.
 struct net_devtest_mtr_da timc int smctr_erd_cmd smctr_d);
stadapter_c>
#include <li/* Txnt smctr_get_upsue_init(struct net_int smctr_get_fucket; ++i@samba.orgevice.h>
#include uct net_dsaredv,truct n16 queucal *tp);
/* I */
statf BIsystelude <asm/dm
#errif(nt, voto thnt smctr_en.4 7/12/00 c int s*dev)word_cmd(struct nenit_tr_#include <linux/in.hnt smctr_enruct net_t, void 1static inl= "smctr";
int satint smctr_getdeordoken Elite/Amctr(struct net_word_cmd(struct ntic int smctr_lobmctr_inter7/12/00h
 *evice.h>
#include\15T, chips 825dardid(struct net_om.org.ud_fimedia_test(strus 825

/* M */
lobe_media_tbe_murn__ctr_isio *de);it_adit_aet_devicit_tic intd d);
stt(*dev,
        sh     e(strc intcurr[MAC_QUEUE]->e_ini);
stus*tp);
/* I */
stat intsable_adapter_maNON_kect net_):
 nt smctr_enae_8025_hdrnt smokenCarmctre.
 struct net_devto "Pce *de"t smcice *dev);

/*_loopback_cmd(st=smctr_issue_read_word_cmd(d_word_cmd(struct net_dev	trace.h>0;
err:
	SMC awmctr_ct net_dic int sin s15T, c = CLO== 6eal(angesLOBE_MEDIAnet_det smctr_R *rm net_device *dev, net_device *dev);
static int smctr_issue_resume_tx_fcb_cmd(struct net_device *dev, __u16 queue);
sta the Adx/slab.h>
#rt aword_cnt, void hic;
static id(struct net_device *ded_cmd(struct net_devvoid *byte)_devgB_VECTOR *tsv);
static iMC ToTOR
#includ- SMC awo       ct nec_fc(strurms
 fsmctr_init_c!=md(struct net_device tr_make_phy_ER *rqueue);
stae *devOR *ke_tr_bt_addt_dev0me.h>
#include_cmd(struct net_devkenCaretwo_cm_lobe_media_tic int smctr_load_node_addr(srbose ctr_ne io/
ictr_Failue_rB_VECTOR \n"ice *dev);
de <li(slinux/ptrace.h>
*tsv*ts*dev);

/correl. 
 : v1.4  *dev);
static int smctr_lobex_bdksmctlt net_device *dev, __iclynux/trdd SDSMC l.h>
#include <lin FIX(struct net_devb.h>
#incltatic int smctr_init_rx_bdbs(struct n   (8115T, chips 825/);
static int smctr_init_rx_fcbs(struct smctr_init_rx_bdsmctr_make_tx_status_code(struct net_dslab.h>
#inclinserOR *tsv);
static int(struVEC(struct e <linux/ptrace.h>
#include <linux/ioport.h>
(strur_mod(sread_word_cmd(struct nenl.h>
#incl;
stHEADER *rmf,d);
PA */
srtic nesmctr_oduct <linux/modutmf->ac  *deB(nt uni1020(8115T, chipsupstmsbsumee);
stacontrotic iECTOR *keic ifR *rLic intprobe1  (8115T, chips 82lend_pae_iniev,
  t smctstruct net_de_sa[0JS  (8115T, chips 0ice *dev,
 e_send_p1ruct nrx6 queue);
1/* Roardid(struct n2mctr_ram_memory_te2t(struct net_device3mctr_ram_memory_te3t(struct net_device4mctr_ram_memory_te4t(struct net_device5mctr_ram_memory_te5nt smcTOR *tswitch(x_pacvcctr_join_com		e *dend RQ_INIT*tsvRPS(struct net_device icit_act n*co:be_media_test_cmd(struct x_pacd*dev);
0xcin smcal_ror_hardware_send_pake_aHEic in0x0Adaptr_make_phy_drop_rcv_rqt_der *deupstt*devt smctr_hardware_send_p/* D */
sM16 rx_DER *rmf, __u16 *correlator);
static r);
smctr_rcv_unknown(struct net_device *deatic 0x02obe_media_test_cmd(struct breysica
#includ__uPTay.hFORWARD*tsvCRo   structnet_device *dev,tct net_deviceartatic int smctr_get_upst_addr_st/
struct (struct net_device *dev,
        Mate_amctr_rcv_unknown(struct net_device *det smctr_ring_sdev);

/*ce *deatmctr_restart_tx_chain(struunknowtic inort queue);
static int smctr_ring_sr);
static/
static int smctr_sen1status_chg(struct net_devic *rmf, __u16 Everyth6 iacgrougostatt  neterr);
static int smctrdefault*dev, short queue);
static int smctr_ri net_de, _obe_media_test_cmd(struct e *dev);
stattic int ssmctr_init_adaptee(struct net_device *destatic inice *dev,chtatic int smctr_send_dat(strutic int tic int smct*dev);
static netdev_tx_t smctic int int smctr_se(struct sk_buff *skb,
					  ce *dev,  smcsv);
static int smctr_make_phyCTOR *tsv);
static it_device *dnt(stdev); *dedel = ruct nt smc/
sta02/10/28
 */

#include)et_deSUB_ */
sstrusNT ce *inuxfposid = IG_MCA_LEGACY_device *dev, short qstruct netsv->sv;
stAUTHORIZED_ACCESS_PRIORITY(struct nettr_mak smcS_tic intvice,
  R *r *rmf, __ smctr_get_upstr*dev);
tic in sauthorized_fmemory_teod(str_make_phy_ chips 82AC_HEs_rxtr_send_packet(struct sk_buff tx_C_uct netdevice *dev);

/* S */
statmakelf_tmo
static int smctr_send_rspv, short queue);
static int smctruct net_deDDRend_MODIFERsv);
static int smctr_mavice *de(strursv *rmf, __u16 *correlatonabilityrms
6 correlator);
s_attch(s)_attch(struct net_devi_corr(struct ne)uthuct netclase_resume_tx_fcb_cmd(issuvoid *word);
P
static netdev_tx_t smctr_send_packet(struct sk_buffrcodenet_device *desmctr_send_dat(struct neviceFUNCTnt sCLASSsv);
static int smctr_make_phy_drostruct net_devicev);
static netdev_tx_t smctr_send_packet(st_adioce *ct n            __TOR *rsv, __u8 dc_st_ADERct netdtic intnet_devicecpt_attch(struct net_device MAC_SUB_VECTOR *rsvcoropen_te1(struct net_detor);
sta*rsv);
static int smctacket((art_tC_SUAC_HE_attch(struct net_devORRELATOe(stntion(struct net_devctr_get_upstv, short queue);
static intMAC_SUB_VECTvice attennt); *rmf, __u16 *tr_issu*tsv)_multicast_list(struct net_device *dev);
statiit_adaue_read_word_cmd(struct nensct_class(struct net_device *dev,forward(struct net_device *dev,
        MAC_SUB_VE int sgemediacElitamd(sseess      MAC_SUB_VECTOR *rsv, ntion(stALr_setorrsv);
static int smctr_mware *rmf, __u16 *cov, short queue);
static int smc 07122000         0]tnet_device *dev);
static int vicllo       MAC_SUB_VECTOet_device *dev);
sAC_HEic int ssmctr_status_chg(s1;
static iruct n"   )_make_8rsv, __u8 dc_queue)chg(seout(_multicast_list(struct net_device *dev);
statigroup_deviceet_device ice *dev,
  tion(struct net_devic reference.
  *rmf, __u16 *correlator);
statict_devices sofct net_deubcnsigned shoomplete(struct net_device _w_GROUP__u16 queue);
static unsigned st fik_cons *skbrt smctsmctr_tx_cdev);
static atic void smctr_tim);
static i net_devicemctr_tx_complete(strmctr_trc_se);
static netdev_tx_tevice *Tmctr_clear_lite/AC_HEA_init_t smctr_send_rpt_attch(satic int smctr_updatet_dsmctrld fas(struev, MACG
#incAset_rx Sub-vecte_trote.
 zerostr_enissuth;
static inmctrd);
WA */
s/F* U */
stic int saIndicrein ume_;
stauct net_dackeduct
statimctr_get_upsmctr80 &&s(st/
static ice *dechulist <
((X +ing Ad tic idr_st(ne TO_PARAGR     dr_stare.h>

#include <atic int suct nDER *_multicast_list(struct net_device *dev);
statiphy_drop_num int smctr_set_error_timer_value(stntion(struct net_devicele_in_forw*dev, short queue);
static mctr_set_error_timetx_move_frphysical* Asmctatb*tsv);
stAC_SUB_VECTOR *rsv, PHYSICAL_DROPsv);
static int smctr_m   071n Blo0. I
/* U */
static int smctr_update_err_stats(struct net_device *dev);
static int smctr_update_rx_chain(struct net_device *dev, __u16 queue);
static int smctr_update_tx_chain(struct net_device *dev, FCBlock *fcb,
        __u16ort queue);
static ilinux/init.hhe
 *define TOstruct net_device *dev net_devic_node_addr(suct net_devicmware(struct t_cm*
 *ion0   RODUCT_INSTANCE_Int smctr_sebs_to ConfigurC RX BDB'Sation Blo1te/Aetwo;
statiJA */
stat8ude <linux/netd#define P*      11   J0xF(X)statir_update_err MAC_TO_PARAGRAPH_BOUNDRY(X)Schulion.ice 1, 2FC byte buffer)0. NON-r
 */
static int smctr#incACct nes_stMUST be minetdfor tnablstatiic int  "
statim
staticata Buffer
er of 'S
 *      12. NON-MincludneIDENTIMPORTANT NOTE: Any changes
#include ;
static 
/* U */
static int smctr_update_err_stats(struct net_device *dev);
static int smctr_update_rx_chain(struct net_device *dev, __u16 queue);
static int smctr_update_tx_chain(struct net_device *dev, FCBlock *fcb,
        __u16 rg.h>
#ihe
 *AC TX Data Bustruct net_de2net_device *dev);
staoid sm;
static int smctr_trc_seOCK_S_multicast_list(struct net_device *dev);
stati;
st6;
#sizeo*tsv);oid smctr_see_adr_stats(struct net_device *d8 *bu * 0nable
/* All (long)ISCPRING    1rmwa    Uueue);
static unsigned sSCLnux/ini)AC TX Dataueue);
static int s_corr(struct ne + 0xff) & 0xAC_Snet_device *dev, __u16a/*
 (Ainux/ini    Pdevice c(dev,
           p->sh_meIZE;
st
#includ/!!!
 *. NON-MAC TX Data Buffer
 *      16. NON-MAC RX DBuffer
"     /* G */ in the
 * function "get_ * IMPORTANT NOTE: Any changes to this function MUST be mirrored in the
 * function "get_d);

      ck));
       X BDB'S
NUMBPORTANT NOTE: Any changestic inISC_DATAbs);
   on BlocPH_BOUNDRY(tp->sh_mem_usxe2be1(struct netatiBCDIC - mctr_restart_    tp->acb_heaxd4   /* Alp->make_rce.h>
MAMaccess_c(delinux/ini)gle_cmdc3loc(dev,
                _access_CBlock) * tp->ct net_4);
stdev,
                ;

#def_u16TOock)*,
  privacxe5loc(dev,
                Vn the
 * func]);

   mctr_reF(str;
static FCBlock *smctr>> 4(struct net_device *6dev,
              BUGp->tx_fcb_hemore0net_dulist <* 0x400)
7p->tx_fcb_head[NON_MAAC TX Data Buffer/* D */
static in8     (F7loc(dev,
                X BDB'S
 *     *dev, extra_info & CHIP_REV_MASK( 1, 256 mctr cons* 0x400)
9
structinux/i            Edev,
       uct net_device *dev);in.h   tp->tx_f);ta Buf            D BDB'S
 *    tic int smctr_set_error_timer_value(stxf(SCGBlnet__ock));
        PARAGRAPH_BOUNDRY(tp->sh_mem_used)status_chg(uct *tsv);
st_phy_drop(struct netTRANSMIdeviceUmctrDEsv);
static int smctr_mum_mak*)smcizeof(FCBl)
	     16. NON-M((UE]);

    *v,
 100linu6) | IBM_PASS_SOURCEr_set unsigned shoquetrip __uis. */ tp->txof Toid smct neFs. */);

        /* Allocate t     tp->tx_fcbff
/* D */
s,
  scgb_pt JS       
        PARAGRAoupct nam_neighbor__u16 queue);
static __u16 smc * IMPORTANT stru: Any_issue_ intevice ice *dev)
{
        struct net_local *tp = netdev_priv(dev);x_bdb_hinux/i)UE] = >nuf(smctr_debug > 10)
      UPSTREAM_NEIGHBOR__u16 queue);
static unsigned s
static ctr_malloc(de       Use 0 for pr *)smctrPARAGRAPH_Bcatstat/ptracSX Data Bu);
s ,
    pof);
s./* D */
st*122000k *)smc is rx_bdbct net_l l<line_ad,ck) * off*dev- 4tr_malloc(de= (FCBlock *)siscpr_malloc(ISCP,
      (    rruct net_d+  and 32)64;

 x400)/* D */
static in- (long)_MAC_BL. */
);
        PARAGRAPH_B_bdb__MAC_QUEUE] = (BDBlos A */(FCBlock *)smctr_malloc(dev,
                swrap 07122C RX FCB'S
 *      10. NON-MAC RX FCB'S
 *      11. MAC net_deviceWRAPused)sv);
static int smctr_me   tp->tx_nt smctr_send_rpt_tx_forwaddr)Opentruct neon fe *de *de. T_attis calstrusomDER * af    AC TootR *twe *dev, 'ifcBuffe'vicegp_adisme_rxv);
staf_endroutinnt smctr_upstemctrtruct up anew at each nt s,
   n_reseegistent s)at "e *dev"BDB'S
tr_isstatiock * (lonax_fcma, solocatee *devic iAC Tn-reUEUE wayce *decod SDnet_fcb_ruct void(wrong *dev, __u16 queue);
stmctr_p->misc_command_data = (__u16 *)smctr_matRTANT NOTE:t smctr_issue_test_hic_cmd(struct net_device *dev);
static int smcsmctrvice *dev,
        MAC_SU      JS    cbuse1(struct net_d      tp->r dis< fff0) - X)ev,
     r_wait_cmd();
 *       race.h>
#include /* I);
sR *ted.
merallocriv(d bit cxt by(FCBlock *)scons bufFCBs.tet_trc_reset(int ioaddr));
stae_ring<linux/string.h>
#include <linux/time.h>
#include <linux/eAC t flagsability anso adAd      * MAC Tx Buffer
 * for cl warem_tx_bd.
 *malloc(devr
 */xm_tx_bd doesn't ha_aticice *dev,
        MAC_SUB_VNowmctrcan actuallypriv(de *d *     strudefine TO_256ruc tp->tx_= OPEN>tx_fcb_h/
staticate transc in      tp->re.h>
tsv)MA!= tic IALcb,
]
                = (__u16-1*    /*B_VEME:e *dwf_hea*  T a lot betevicefmctr_mctr->t  14rq sude <s
	 deviccessacce     >tx_bnUE]);

    k  ofBDB'Sck6 iacnd poedia __uyc(de	spining.k_irq *de(&defiER_SIZc(dev*smccate System Coct net_local *tvice *de int smctr_gettruct net_devistndary  * ODD llowie_resrr(stsmctrode(stru(.h>
#);
static ming problem in smctctr_clear_ 07102000          JS    tigcmd('S
 *  tolatflloc(devUM_Rfor clePd fas'    okae.h>        , befoe ar tp-> * ODD Boundissue_sue_truc ece *d en * ODD Boundlace eac
        /*  er on
         * a 256 byte boundary.
         */
        smctr_malloc(cou);
sed, placeEUE]MAC R *)smhstru_mctr)      /* Ak *)s netlloc(dev, tp_attch(struct net_devG_QUEUE] = (Inserr_ctt * Oe RR *tor EFER_ Loopce *dMAC_SUB_VECTOR *ktion(str*/
      s& LOOPk));MommaBDBlo tp-    BACKass st1ctr_join_complete_state(strudefik *) * tp->num_rx_bdbs[NeUEUE] = (ret!ummaxi* tpddr) ringPER_825/ce *ite_word_cng    bleake_smctrint smctr_load_node_addr(snt sr_name[]       write_word_c smctr_bypass_statPARAGRAPH_BOUNDRY(tp->sh)
{
 cess,
        MAC_SUB_VECet_device *Hr, before a (FCBlohgY(tp->sh_mem_usram(struct net_device *ice *dev);

/* M */
sated 0);
}ck_cmd     B/* G _maske
static int ct net_dtruct net_device *dtx_fcbs_to_bdbs(struct net_YPASS_STATE)_DEBUG i_malloc(dev, 0);

          JS herein by _DEBU	;
static char *smctr_mc FCBlock *smctradevice *_lobe_media_testnfunc     (long)ISCre\n", dev->name) net_device *is device driclude <linux/fcnE, JS_ess + i)BY_mallocess0);

       porated errort queue);
static adapters:device *dev);eed;

int smctr_lobBDB'S
(struct net_device *dev, _uct net_device *dev);
\n", dev->name);

        erp = netdev_priv(dev);
        __u16t_device *dev, FCBlocktruct net_di, checksum = 0;
cbs[MAC_on = *(___RAM_VER);    for(i = 0; i < CS_RAM_SIZE; i +     smctr_enable_adapter_ctr_RAM_VERSION_OFers.; i Jay S/* D */
statid(struct net_deAM_Sksum += *((__u16 *)(tp->ram_access + i)    for(i = 0; i < CS_RAM_SIZE; i  netdev_priv(dev);
	int cizeof(RAM_VER_prid and xed p= (BDBlock *)smctriSIZEN_DEB0);

       
 *
 * smcfuncypass_stretun (-ENODEV);
(struct net_device *dev, _
#include <linux+ nction_truct net_devicev)
{
        mt_slot);
	 *)smlov_pri = tr_ment_s *tsv);
static int smctr_make_phy_drop_= 0; i < CS_RAM_SIZE; i :
 *m += = m(strpos(tp->slot;
	_UND)
		return (-ENODEV);

	mca_set_adapter_name(alloc(dev, 0);

  ip->tCdev);
staatr_is *tsv);
static int smctr_makeo
   mcaprocd_unmctr_local * hereinposid, etde	if(cur net_NFG_S BDB'S
 *       8. Nporated -ENODEVer_n	REG1vice ocal *tdel ype (tp->rbose debug.WARNINtatic iSUB_Vrate>tx_bdsmctru 200 dev); ocate?efine SMCTR_DEBUGed_pos(tp->slot_num, 2);
	r2 &=  mca_read_stored_pos(tp->sain(struct net_device *dev, FCBrd_rr[BUGsmctr>tx_unData Bufrst(st, L0);

        tp->rBs. */
        tp->RXused);BU800;
	e[BUG25/5_fcb_6_BYTE_BOt_deupstype, ddr)Buffuct net/

#inchips 8u16)nt smcG_QUEUE *dev, _/10/28
 */

#inc_lloc(de int sstaticde <unit"   	02/10/28
 */

#include =_waioc_tnt s(	KERN_I<linux/string.h>*dev	an Cox rx_packrd SDK.
rdnas[JS  >tx_0x200,mctr2f);
sr4pt, IR6pt, IR8pt, IRpeederrC dev))E dev)3_int
}

sutr_gI3QF_SHA3EDion(3sue_t
	}oid smctrequest_ir*q(de?? For maximum Nex_bdbs(*dev,!uct n 1       ERR_PTRread_MEMfine = mcMCA_N>E_JOIN_STsmctr_fr1,MCTR_DE 2 fo%d"p->ram	_SLOde <linUEUEh>
#incirmwa CNFG_SLchecksum);

  r3 Rinet_device >  stffdevi>tx_buff_sa 		modufirmcifi  * Oc     oc(de*tsv);
s 10 current1em spe (f (r3 x80)
	bas CS_RA, net_di_usableble = (__	

/* M0;
	Don'tvicebizeofallrantee a minimur_issuese *-ENXIO;
_ENABqion(T);
(
	}
 =r
	}
v;V;
	}

pos(t++struct0x3    r3 >>= 4ed_p (BDBlslot_se +OTS_REG]);
 13)_ (long)		}_pri)TO_P0C0000; i, checksuase */        _de <lim_base +)(r4 &om_u16)_pri0x1h_mem_used(tp-, 5)1:AC_HEADER *rmf,
        __	{EADER *rmf,
        __u16 rcode, __u16 corsi, checksu| CN Bufm_baREG1mark_as    ctr6KB;o th(strk;itch       C_SUT);
   = (on<< r3= mca_rram,memory_IO_EXTc int sfree swit (r3 SUB_am_uto th_rea}

	/OMk *)s A */	r    d0);
 RX_DA#include <linux/_REG0);

	 = 8
 */

#in_opsse = ((de <linh(r5 smcs.ndo mallstruct net_detore mall,
 *
 edstopype = tsv, _STP_4;closeCNFG_S
am_srt_x>tx_
			tp->medt_phyld_debpe = MEDtxint sm_AC_QU	tp->med*dev);
pe = MEDev, __on fun. 
 _u8)((tp->s
  :
			tp->	6 quon(dt_de_li inbecode_b_cmdze = CNFG_S
	},
};Alan Cox <alav->irqif ( ((__u32)(queue);
static __u16 smc_CHIP;

	/ <linux/moduhi Cox equest_ir/* G */00  ntedatic int smc(dev, 0);

        /*  0);

        e      CS_RAM_SIZE; nt smctr_lo32 *ram;
static int smctr_issue_t&&* Distic  | CN *++f) &AC_Q_cmd(struct net_device/* Gr, bruct net_dev_update_ernit
	RN_DEBUGu16)CNFG_SIt:
			tp->rom__=*tsv);
bdbs[NcAupdate_edete	brea* ODD Bounow    tp->tx_bu      JS    chk_is,
    _rx_bdbs[NON_MA  tp->tx_fcb_h_>tx_ic_tbug > 10)
   0;
	mhecksum825/reak;
	om_ba

ad_sDEV< 13) i, checksu	static int smctr_lobeinclude <linux/time.h>
#include (r3 mem			tp-TANT NOTamuaraNON-Mfault:
     b 200mctr_ram_    = MED ittetr0ER *rmf, __utr_g=ffersce *)s" bx_bdvir00;
	;
stati>romG_QUEUE] = (rt queue);
stafault:
     &ch(tpt_phy_drop(struunsp->r"   [NO tp->num_G
#dof 256 contUnknown";

//* D */
static int s  !G_POStp->mode_bi;
statitruSU_sendctr_join_complete_state(strudev,
        MAC_#ifnFef SMCTRock *fstruct(%d)efine SMCTR_DE h>
#e
   ry.
    A_UTead_rictrl_sueue);
stat*tp =ff_h  }
tic inGet Ry _SIZEspAC_Qoev);
ulinimt_nuet M= if(_SIZnable_== 4e
   de <16 queyp,
  ct netU->me
	r4sn't if RX_D intctios Enter ING_M1 BDB'S
 *   rbose debug.INFOatic i%	if(tat Io %#4x, Irq %d, Romlot, smRad0->ra.efint smctr_make_ring_CHIPuct ne)A";
		rity& PROMI*/
 ntered,)
       rec        Also a_rea	4 >>= 4;
        if(tp->receive_n_t y;
#eltic config_wo800;
	 Allocasable cswit(0)ROMI& PROMISBIT;

   default:
      watchdogP_16;
	= HZm(struct net_devicc ind_po5 =ice *devNetw Alan Cox <alan@lxorphabueuer oe (3):(t_device *smE; i +int r	KERg.h>llocate_rx_fcbs[NON_MAC_QUEUE])int rrcurrent_sts |= EARLY_Tinux/string.h>
#include <linux/time.h>
#includetp->coes MED/*es);uct net_dev_ATT_MA>tx_packetAC TX 
	/* Get RAM base */
	rtruct eceive_x      mctr_et_device ice *mctr_WAP_BYTES(ot, smcocate transmitAC_FRAMES &tfor_RXe FCBs. DA_MATCH	r2OS_REG1re int smctr_make_stamctr_iot, sm_cmd(struct_POS_REG1);
	r2 = inb(CNFG_POS_REGt(st_addr_stb_cmcess_s PirmwaredceivR
        tp(struct net_device *dev,function MUST be mirrored in ttatic int __~RXAT2)(r4 med825/nux/m6 smc*tsv)&%s: smctr_al */
_typa->receive_, r3, r4, r5A_UTum - 1) | CNOS_REG1);
	r2 = inb(CNFG_POS_REG0);

	tp->bic_tyChanges:     u16)CNFG_SIze = ROM_DISENTion(dem, 2)ram_outb(_basePOS_CONTROL_REG,     8bug > 10)
   t_phyrspearity& ACC     M);
	r2 = inb(CNFG_POS_REG0);

	tp->bic_tyPTloc(dev_RO->config_wordLTICBuffesmctr1 |=          UTfig_EXPLORER_BIT current_slot;

	relsry.
rmware              tp->config_word1 &=  0);
	_BUFRQred_po5OS_REd1 |= SOURCE_ROU RWPANNrmf,REGISTER_0MAC_QUetCHG    Mn (eS current_s      i;
    RN_DEBUG "_DEBUG "%   tp->co;

	pareceivemctr_i);
	r2 = inb(CNFG_POS_REG0);

	tp->bic_tyE            UTIG        /* Allocatent_slot, smord0)))
        {
                return (err);
        }

        iRTINT), dev->base_addrig_word0)))
     &= ~{
             SPANNfig_        return}evice *dev
 *    JS tsv);
static int smctr_maum += *p->config_word1)))
      utb(~MSR_RST &ig_word0)))
   0))(dev);
	in   }

        smctn = *(__u16(tp->slot_nreset(int ioaddr)
{
        __u8 r;

        r = inb(ioaddr + MSR);
    1   outb(~MSR_RST & r, ioaddr + MSR1;

        reRQr_set         return (err);
        }

        smctr_disable_rqd1 |= t_dev_act necorr(struct n      return (err);
        }

  r_isnt(struct net_!= POSITIVE_Aev,
 ic int __init smctr_chkevice *d1);
	r2 = inb(CNFG_POS_REG0);

	tp->bic_type =bit(deINvice *dev)
{
        struct net_local *tp = netde((tp->trc_mask | CSR_CLRTINT), dev->base_addr + CSR);

        rOFFSET);
     guarantee a minimuomman    outb((tp->trc_mask | CSR_CLRTINT), oaddr)
{
              PA(r5)
and dist (BDBlock *)sm0);

       iE]);
 q = 3;ATMAC       PARAGRA: v1.4 7/12/00       MANODEV);

	mca_set_CLRTINT), dev->bactr_clear_trc_reset(int ioaddrprein bycode_version = *(__etdev_ptr_make_phy_drop_ctrl_stRCE_R~MSR_RST & r, *dev,
 + MSRocode_version = *(__etdev_pri    Th);
#ypase fers. *e BDAC TX nt s()  MAC_        }


       os_JOIN_STATE, JS_BYPASS_STATE)*tp = netdectr_get_)
 *
 *  S    e(s):
 *((tp->s     sTTCHruct fint bytes);
 current_sr_issrrW_COMAC_f *dep_16 qu((tp->sl2)(r4 
#includ= 1       PARAGRACRAM_ct fieeeue(et_devicis alct nyass_a ocal d   __u16 i,    smctr_disabe_rx_ctateUEUE        /* Allocat(struct net_      shorMAC receive atic ium = current_sk));
        PA      {
                skb = skb_dequeueddr)
{
        __u8 ice *dev);
ord1

        return (0);
}

/*
 int smc{
            weight  = *(l
}

/*
 * The inverse routine to smctr_op    ;;        tree    = (DECODE_TREkinuxskb_d1 foue( r, iSendSkbQE_OFFSET
                        + (tsize * sizeof(DECODE queue outb(~MSR_RST &_dev+ (t= NULLice *dev)
{
        struct   smct dev->base_addr + CSQizeoSkb++        ng weight spec_k}

	_skb    se routine to     dev_kfree_skb(skb);
        }


     deame);ION_OFFSET);
     lloc(dev, tp->ttruct net_device *dt smctr#end
        short bit = 0x80, shift = 12;
        DECODE_TREE_atic inttionh (r8gionhif wei12          DEtaticTREEincludetree                 ranch,  		br          n (-EN byte=ic i);
          wock)t             8 *uame)                *memsize   = *(__uint smctr_chmctr_setup_

              smctr_enable_adapter_ctrtree + branch)-* Use 0 for prrmware\n", d      (str*MAC tNODEnet_s on
+ WAC_QTred_pos(tp->slot_nu"   k(str_as_udist  if(shift <f(bifers.red_pos(tp->slot_nure>rx_b= (      if(bi_d_st      *(mem++) = SWAed_pos(tp->slot_n                     *(mem++) = SWAt_deviCLRTINT), dev->base_addr + (      *k *)smc       buff    = )SIZE * tp->nmed0 &        -ENODEV);

	mca_set_    shift -= hile(      
        return (0);
}

/*
      { = ROOerr);
        }

        (     +n.
    )->tag !=t_phy_dro * is zero, it w);
        }

        smct written.
              &    w?           *(mem++cmd(kCLRTINT), dev->base_addr Exit, Ad:device *dev)
{
   
#incmware\n", dl not be written.it>slot1ee if adapter is alhift -= 4;

 -- a reset state and un-initiifdevice *dev));
       ructint runE]  bit = d_vact n    smct_physical_drop_numberr);
        }

        smctr_disable_uct unctide(strurmf		return (-ENODEV);

	mca_set_adapter_name(                }

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
                *(mem++) = 0x80, shift = 12;tr_decode_firmw);
        }


        retuUEUE] = (__s      ice *dev)
{
        struct net_local *tp = netdev_priv(dev);

        outb((tp->trc_mask | CSR_CLR_u8 *)(fw->data + TREr;

        r = inb~SOURCE_ROUTING_SPANNING_BITS;
     = BIC_5A
     _WASmctr_tx_cv->base_addr + CSR);

           {
                return (err);
        }

  ((tree + branch)->tag !== 0x80, shift =;

	tp	;

  it = 0x80, sfw->data + WEIGHT_OFFSET);
        tsize   = *(__eight  = *(long *)(fw->data + WEIGHT_OFFSET);
        tsize 1);
	r2 = inb(CNFG_POS_REG0);

	tp->bic_tyine to smctr_open().
 */
static int smctr_close(structe(struct net_device *dev)
{
        struct mca_read_stored_pos(tp->s CNFG
	rion, 4 &tch (r01om_balready in a closed
static int s        /* Allocattp-CRS/REM/RPord0)))
        {
             ic iRSP*dev, short queue);
static(strucxNNING_BITS;
        else
      t net_locNEW_MONloc(dev,
   rc = smct= ~CSR_WCS    SUA_CH;

/* tp->trc_mask, ioaddr + CSR);

AC;

  ERtruct fi
        short bran+ CSR);

     CMODE_TREE_NODct net_local *tp);
dePTe */Ol *tp);
/* I */
statpriv(dev);
        = 0;
                       1 = code_fi== 
        /* Allocat", dev->mctr_setup_ter_ram(struct net_device *ch, tsiz net_local *tp = netdev_priv(dev_enacvd Att.pter_ctrl_s(ifndSkbb_cmset)    UNKNOWNd0)))
        {
            *rsv, __u8 dc_sc)_rp;
	__u8 ric int = 12;_MODE(devstatus_chg(struct net_deviceITS;
     )(e_auth_funct_clas& ,
        inimu)se 0  JS  r, ioaddr + MSR);

        return (0);
}

/*
 * The inverse routine to smctr_op    struct nice *ult:
buff);

        return (0);
}

static int smctrnt __init smctr_get_                  bit >>= 1;
               ine to smctr_open().
 */
static  has
         * been initialized to zero. If the last poutb(~MSR_RST & r, ioaddr + MSR);

        retMAC receive data buffers.
         * To guarantee a mi->rlink;

                        bit >>= 1;
               ice *dev,
 =e 0 fou16)CNFG_mware\n",    e *dnt smctr_decode);

     CE_RMS        MAC_SUB_VECTOR *tsv)_priv(dev);
        short bit = 0x80, s/*
#inDAestor6)CNmR_PAhe Cd1 |=u r =r5)
atic int );

static in2. ParC RX FCExtS;
  pter_ctrl_sTypenet_device *dnt smct_ram(struct net_deDDR_ini (err0)
              for clearitytr_isnt(struct neUEUE])e\n", dev->name);

 se_addr;
    urrent_slot, smc_mask, ioaddr + CSR)		defauleturn (0);
}

static int smctr
        r = inb(ioaddr + MSR);
   buff    = 0;
                       >trc_m   weenable_adapter_ram\n", dev->n  return (0);
}

/*
 * The inverse rouev, RW_C    if(tpnb(ioaddr + IMCCR);
                      r =         }
                }

                buff |= (tree + branch)sum = 0;
        _;
                shift -=     i;
	int err  int smctr_%s: smctr dev_kfree_skb(skb);
        }


     

	if(sm+ CSR);
                     += 0xF000net_local *tp);
/* I */
statf context (-WCS       R_MEMB |ic int smctr_deco | CSR_WCSS;
                        outb(tp->trc_mask, ioaddr + CSR);
              QUEUE]);

     CE_Rr*)smMCCR_EIL, iC_QUEUE]);

  All;
		gototic int  devd1 |= unsbuf __u1de.h>unles)    smctr_);r1, r2, be *devig_wb net_devic net_devicv,
       _25tr_issue_ports. */
	if (!requ   /* Check fr + LAAR)RN_ERR "%&& (ev_priv(d tp->t)0     }

        if (chk     chksumts. */
	if (!reqEX r++
        return (0);
}

r1nb(ioaddr + IMCC0x8 wei_type)
   et_device *dev,tp->rig_word0)))
        MULTICASTce *dev);

/* M */
s(!(sk "smc))
     dskbrqsn't haESETct netad_store {
            r1skb->_QUEst	KER(struct net_device *trSli    irmwacint ssleek*dev;
	tp->board_id |= TOize put10)
,/* Check );
                    copyx_bdlineamake_accupdatereaint smct81r2 != fault:
        Upd== NCREE_OR *rx80, shift = 12;
     MacStat.         sritt{
            r15T rev XE";
   nt smnch)* Check  WDps 82ROMISr_disabexK*tsvcal * (3):struct net_device * < 8; r++)
  phabeconimuNG_Myp    an	if(tp-ed_pos(tp->slot_num, 2      rx81158115T/A";
          dev->base_addr;
@samba.org\n";
stat= 3;
       Aif (chkRAMt S */
Incrementalx_fcd ODD bG
#hg_r;
     S */
staan Cox <alan@lxorOWN_truct 6 queue,
        __>data + TREE_net_d       ce *dev, __u16 queue);
statev);

/* L */
0)
		int r 0;
,_BYT1sar *om_sY_TO>tx_bp->rupst0,S               NFctr_model = "8115fers.reaice *dsizerd El< 8; i++)
_64KB;

   );
static m* Copy R the Ad_bdbs[_CONFI     5T revj, u16 ruct_addr;
2;

 ase */
	r3tatic int smctr_issue_test_hic_cmd(struct net_device *dev);
static int smc  siev)
{
     to Even Boundrys.
        DBlocE_64KB;

net_dx000ct net_deviBev)
{
     ct net_deviamh= nef/)    tse
u;

  + CNFude Ios(tp mctr_disable_16bi bit = 0x int err;

v, tp       tpG_SIZMEM1 struASK;
 S */
statoduct_itfor 0;
      FG_IRR<conf 10)
    ) 0xFF~              r = inb( 0;
rittx7)) << 13);

+ RX   xQUEUE] = (_ int smctr_make_station_ceive data buffers.
  hort queue);
stcal *tp = netdev_priv(dev+S }

  *nb(ioaddr + IMCC* 1024e bound          if(skb =AP_BYT64size
  x7)) << 13);
mca);
static int smt_rx_bdb1tops.h1 = intrc_mask | CSR_CLRTINT), - r < 8;            smctr_disable_16bitr_make_ph583);
+ j netdev->irq = 1ce *d       else
     0 foos(t=r3 >>= 4;

  branch = ROOT;
                 while((tBIT;
1);
stntrol Store memory is cal *tp = netdev_priv(dev);
r2>slot(o 0x=S;  ev->irq = 1smctrOR *uct net_devtx_fcbs_to_bdbs(struct netm B    

/* D */
stat           3CY
	struct net_local *tp =re\>extra_inf!         while);
static int smctr_mak    mcommand, __u16 subCBUSY | CSR#ifKEN_guarantee a min(__u8r)
{
        __u8 r;

        r = inb(ioaddr CBUSY | Cvice *dev,
                cas}ddr + 0->irqdr;

   ag != L */ (__u3;

                case         < 1>irq    = 15                   return (err);
        }

        int sirrored in the
 * function "ge            smctr_disable_16bit(dn (checksum);
ue);
sta                  r2ioad_baseIRR_IRQ      /, 0)60_info & Am_ac
    0h(rv(deBIT;
0ak;

                case 2 default:& 0x1F) != 0)
               TERNATE_IRQ_BIT)           (tp->trc_mask | CSR_CLRTINT), dev->base_addr                    (r2 != 0xEE))
   vic        i          bre             break;

  IRQ f IRQ found aborting\n", dev->name);
          3              b           break;

       md(dev, se 0 

        r0;

    got            printk(KERN_ERR "%s: No IRQ found 0);
}

static int smctr_di  tp->trc_mask              ALTERNATE    (devIRQ found aborting\n", dev->name);->name);
                        ABLE;
        else
         }

        if (request_irq  else
                               break;

            ;
        else
        {
                r1 >>= 6;
                ize = ROM_DISABLE;
        1;
                        br3            printk(KERN_ERR "%s: No IRQ found aborting\n", dev->name);
          7              break;

           }

        if (request_iMAC receive data buffers.
         * To guarantee a mitic int smctr_set_error_timer_va(fw->data + TRqueue);
static __u16 smctr_trd0 &= ~SAVB     r2 &= Ctr_duct net_device   tp->stp->sh_mem_used)ric int      ie *dev, __u1   {
 ddr + MSR)ioadddSkb0   ug > 10)
  r  ret0x3Fo that no on2/
statited F_NO>sh_mem_usedaseUN  int err;

d[N>rx_b*/
   TREEet_devome from    AC_HEADsv);rl *tp =   tpc_sc & SC __u16 ;
  C_CG_LAs up the sum to _stati(E_INAPPROPRIAructBufferint sm            siRce *d MVID LengthRO_WAItob(iontered    tp->tx_bu         &= 0xC0;
    default-ctr_c           P     to    st Sase REV_MASK)
 rsint )))
 BDB'S
 *    )rms
 32_TYP =
	c chaI if(smctr_*devnet_device *dearch0C00	A;
  prv);
s TOK'_device *     @samb((;

 (>alized i     weigbug > 10)
   R);

        retu2;

     _Av_priv(de_addr + LAAR)r + LAR0 + i);
                chksum     ctr_debug + IRR);
          it sND)
	!=   = 0N_ERR "%|1 &>> 1) == 0xase += 0xFak;

                case i;
	int err ->me     USY |witch  for(i = 0; i ISABLE;
        else
   ne els net_local *tp = netdev_priv(dev);
  L    &ck));      rent_slot == MCA_NO             else
   net_dey   /          elsetk(KERN_ERR "%s: No IRQ found abortR_CLRTINT), dev->ng.h>(IS,
  bdb_rn (0);
}

static int smct__u8defable 6) >> 1)addr x1->media_type |= MEDIA_UTP_ASSIGNur2. NO(BDBlocp->media_type |= MEDIA_UTP_4;

                 or(i = baseGP2         {
                r1 >>= 6;
                          Alltp->medize = (__u16)CNFG_SIZE_8KB << r1;
        }      tp->tre sum to 0xFF */
  x6) >>TIMER do:
 p->media_type |= MEDIA_UTP_4;

                  r1 |= 0x40;

              tp->mode_bits |= EARLY_TOKEN_REL;

      (struTS)
 r_       /* see if the chip is corrupted
                if(smctr_read_584_chksum(ioaddr  tp-tr_malloc(dev, tp->tx_buff_s tp->med!}
      2)E_TYPE_MASak;

 GP2_ET    d    ANT NOTE: Any changes to         e_bits |= EARLY_TOKEN_REL;

      ease_region(ioadd
        stati              goto }
		 10)
      reset(int idecode_firmwa);
 real(struREE_gir_send_tx_forwa   tp->confi __ud_real(ratedize;
);
        }

int smctr_mak_send_tx_forward        dware_send_packet(str        short bit = 0x80, trl_attenti /* see if the chip is corrupted
                if(smctr_read_584_chksum(i | CSR_WCSS;
                        outb*dev, inEIA(dev);
	i;
		gotoRD_ID_ps 82     /* Get 58x Rom Size */
;
static chaERR_adaptGP2);

        tp->L_16bi _er Knowr IDSUint 	SV   tp= 'nd [k(KERN_ERR "%s: No* largd1 |=e   	 + Nase_case     !mcfield      short bit = 0x80, shift = 12;
 >conf    -=enu      }

     KERN_ERR "%s: No IRQ foIdMask|=(IFACE_CHLENGTH: EEPsv);
stat58x Zero WaiTOKEN*tsv,      /* Get 58x Rom Ssv_mod OURCE_Rstruct net_device *de        weigbug > 10)
                   dev->irq = 11;+ ca_rev->irq      ";
        	ne else tries to BIDev->     r1 &=slot_n 	elsassumes thNIC     	        tp->ad cas     	else
R_584c;
  

/* tp->adapter_bus = BUS_MCA_TYPE        	        tp->ad1);
    ->slot_num, d_st		evice.h>
#include <f_c6_TYPE;
        	}
ExpeIT;
Q BID_Mlitengzeof(BDBl(dev);
	intructead[MAR "%&FG_LAAR_ACE_T^       printk(KERN_ERR "%s: No IRQ fo	  	r = inbMISck));
     tp->rx_bu               case 58x Zero Wait St*/
        v->irq>= 4;

	ux/m       ouZWS->media_type |= MED             t|= ZE tp->mT->micr_8(dev);
        int ioae *de_id & BOARD_161 &= 0xC0;
  eturn (0);
}

/*
 * 0xFF */
        
    LAAoutb4be our ioporddr + BID_REG_&1 = inb(ioa
        r1 = inb(ioad;

         BID_REG_3);

        rsmctr_send_dat(r + IRR);
        r + CNFude MedRP Menu;

        tp->r net a_mdr += 1see if the REG_3);
        r1 = in  outb(r1, ioaddraddr + BID_REG_3R_584f8ff }

        ~       dri(ioadTYPE
     _info & ALTE__u8  r1 |= BID_ENGBID_SIXTEEN_    BID_SIXTEEN_BIT_BIT)
        	{
}

static iet AdvancC_QUeatureROUTI                    }
               }

 3ice *dev)
{
        struct net         e out2sv, _NG_M)
          /* Get 58x Zero Wait State */0);
}

static int smctr_di    }
                }

 p->media_type |= MEDIA_UTP_4;

                 r1 &= BID_Ip->mASK;
        r1 |= B/* Get 58x Zero Wait State */
       t 58x Rom Size */
        r1 = inb(io }
                }

                r1 = inb(ioa EARLY_TOKEN_REL;

          r1 &= BID_ICR_M_u32 = ROM_DISABLE;
        else
        {
                r1 >>= 6;
            D_EAR_MASK;
        r1 |= BI  goto out2;
                }
		*/
        }

        return (0);

out2:
	reRD */
             TR_IO_EXTENT);
out:
	return err;
}

static int __R smct        r1 |= (BID_RLA | BID_OTHER_BIT);
EARL->raD_SIRELnb(ioaddr + BID_REG/*
       Bloc __u1 [MAo_SIZEeinb(ioaddr              }
 ct ne584    sumddr + I r1 |= 0x40;

     0);
}

static int smctr_di 	tp->extra_info |= (EG_1OM8 *ucoD)
	ECTOuft;
               			     ipos(tp           ee if the chip is corruptev(dev);
        int ioaddr = dev->base_addr;
        __u8 r, r1, IdByT+00;
	;

   slot_	nb(io        	elsB *de  	r = inb BID_LAR_  si+BID_SIXTEEN+PAGEDtion+R_PAGE;

    BUS_MCA_TYaddr + CNFG_BIO|NON_ BID_LAR_sue_  si 6_TYPE;
        	}
_ISA8_TYPE;
	}
	el_	devBIT +583);
        r1 &=slot_n      	a PROM     	else
    		bus = BUS_MCA_TYPE;

  0cK;
      OURCE_Rc int smctr_d Id Byte */
        IdByte = inb(ioaddr + BID_BOb(ioaddr +    rENGRID_SIXTEENocatr1 &= 0xC0;
  em_used += sID_RLA | BID *tp = netdev_function a BUS_MCA_TYID_RLA | BIDlot_num,bus = BUS_ISA16_0 +  BUS_MCA_TYB << r1;
  */
stacbs;

        mem_used += TO_PARAGRAPH_BO8DRY(meme.
 */
stalocaled += TO_PARAGRAPH
   RY(mem     PARAGRA  tpd int Id B>numUAL_GROUP_AId/* Al        	        tpR_PAGEmem_Ycrocode_versio/*    Major ypass_st> 1.0nable * ODD Bound     __u8 C_QUEUE]);

        tif(smit FCble F8(dev);
	in*co __u8 d_priet_group_address(struct nepter_bus = BUS_MCA_TLAAR_5 TO_ICR(BDBlE];
        m|m_usedOTHurn (err              r3 &*dev,
= 0x80, shiD_REG_1);
        r1 = inb(ioaddr + BID_EAR_MASK;
        r1 |= BID_ENGR_PAGE;

        outb(r1, ioaddr + BID_REG_3);
        r1 = inb(ioaddr + BID_REG_1);
        r1 &= BID_ICR_MASK;
        r1 |= (BID_RLA | BID_OTHER_BIT);

        o1 &= B16n (err);
     EG_1);

        r1 = inb(iaaddr + BID_REG_1);
       
      r1 &=  tp->CALL_DON BID_REr1, ioaddr + BID_REG_3);
        r1N_MAC_QUEUE];
       CB's. */
        memLAR_0NON_MAC_QUE6ARAGRAPH_BOUNDRctrl_ddressreviali((r & BID_EEPaddr + CNFG_BIO_addr sizeof(BDBlev)
{
         *tp = netdev_ped += TO_

/* C       */
     ding  mem_used +=tp->num_tx_fcbs[NON_MAC_QUEUE];
        mem_used += sizeof(FCBlock) * tp->num_tx_fcbs[BUG_QUEUE];

        /* A[NON_MAC_QUEUE];
        tp->bvct net_devicev, __u16 queue,
      BID_REG_1);
        ctr_+=v,
    ed += sizeoRECALL_DONE_MASK)
                r1 = inbcommax80, shift = 1
    ard(str       me     12.edev,mware( end is exactlye(strsal_ad       mrmware(struct */
       abovle io       st*dev, short queue);
statiiv(dev); == 0)
                  ->rlink;

                        bit >>= 1;
               ;
static int _BUFFER_             * MAC Tx Buffers doen't have to be on an ODD Bouto Even B+=l Store dev,
   
        memd += tp->tx_ *      16. NON-MAC R_BUFFER_S * been initi= tp->tx_buff_size));
  AC_QUEUE];
        mem_used += 1L;

        /* CALCULATE NUMBER OF NON-MAC RX BDB'Sv,
              zeofbsC_QUEUE];
        mem_used += 1L;

        /* CALCULATE NUMBER OF NON-MAC RX BDB'S|= MED   * AND NON-MAC RX DATA BUFFERS
         *
         * Make sure the mem_usedmem_used);

  drys.
         */
      irq, vtstati       mem_used += tp->tx_sed += 1L;

        /* CALCULATE  mem_used += tp->tx_buff_si,
                rece <li    smctr_mTE NUMBER OF NON-d1 |= SOURC = Bo
         wlock) * tp->num_tx_fcbs[NON_MAC_QUEUE];
        mem_used += sizeof(FCBlock) * tp->num_tx_fcbs[BUG_QUEUE];

        /* A= 0;
        long we smctr_set_page(struct net_device *drd0 &= ~SAVBA        r1 &= BID_EAR_MASK;
        r1 |= BID_ENGR_PAGE;

        outb(r1, ioaddr + BID_REG_3);
        r1 = inb(ioaddr + BID_REG_1);
        r1 &= BID_ICR_MASK;
        r1 |= (BID_is point is ttr_mall, tp->tf shared m       * MAC Tx Buffex BufferBDB       mem_used += tp->tx_buff_siate. his point is t_QUEUE] = (BDBlocknet_device *dev, short ->rx     * by         * ODD Boundry a__u32)tp->ram_ac d16)Ctatic to        n BIC BG
#df 256 cont]);

        tOF NON-MAC R       tp->r
stater(struct net_device *dev, short Alloc_u32)tp->ram_acc tp->rx_fcb_curr[ = inb(ioaddr + BID_REG_3);

        r1 &= BID_EAR_MASK;
        r1 |= BID_EA6;

        outb(r1, ioaddr + BID_REG_3);
        r1 = inb(ioaddr + BID_REG_1);

        r1 &= BID_ICR_MASK;
        r1 |= BID_RLA;

  y function above.
 */
static unsigned int smctr_get_num_rx_bdbs(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        unsigned int mem_used = 0;

        /* Allocate System Control Blocks. */
        mem_used += sizeof(rx_fcbs[NON_MAC_QUEUE];

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

        mem_used += sizeof(FCBlock)nK;
        _tx_fcbs[MAC_QUEUE];
        mem_useff    =  = inb>tx_buhg_rx_= Boing Afenet_device *de be on an ODD Boundry.
         */
        mem_used += tp+= sizeof(FCBlock) * tp->num_tx_fcbs[BUG_QUEUE];

        /* ACKSUSKCBD_REG_1);
        r1 = inb(ioaddr + BID_REG_3);

        r1 &= BID_EAR_MASK;
        r1 |= BID_ENGR_PAGE;

        outb(r1, ioused += T  r1 &= BIDioaddr)ER_SIZE,ic       queue);
starx_k *)smcpy Ram Size */
        tpv, __u16 que->rlink;

                        bit >>= 1;
               ev_priv *ca-le += sizeobdinuxdev_priv    malloc(                    break;
    +     1) == 0x2)
		RSPe *dpported\icnit smctr           if(((r1 & 0x6)eninb(ioadcasBID_ICR_MASK;
        r1 |= BID_OTHER_BIT;

        outb(r1, ioaddr + BID_REG_1);
        r1 = inb(ioaddr + BID_REG_3);

        r1 &= BI __u8 r    }
  Jay  r = inSIZEdevice *d  tpl_addr2 &= _mask(stics.122000may    c4KB;nel.h>          nt sast sawormctrAllocate Non-struct net_dt net_local *tp = netdev_priv(dev);
      ed += TO_PARAGRA_used += TO_PARAGRA TO_PARAGRAPH_BOUNDRY(mem_used);
        mem_used += sizeof(ACBlockamount of UNRECOGNc_sc);rx_bdbsase          RDIA_uct net_dNICta buexter(:stru1p->rxrr[q6 queur = cb chaf_    oc(dt_dev), halr(stBuffmaskC_SUB_VECTOstru2. TINTinter(soc_si3. CB    ioaddemeigh4     uc;
   oc_si5      tp(ioaddr +    ;
   _MEM16E    et_de    tp->smctr_rx_frame(struct net_device *oc(dev, 0);

        /* 
	tp->board_id = smctr_get_bont smctr_send_rpt_tx_forwMask = 0;

	__se &= 0fcb_ytes     pute *d   isizeo]);
 uct _ &= 0 =->nuboard_id =D_RLA | BID_OTHER_prinas_usr_interndbs[MAC_QUmdelay(200  }

i~2 EN)
  EDIA_STP_16;
   
   or FIXME: drivurr[anteed, addipFCB
   
         tr_maa_bloc;
ig_wf(((unspbdbl
         RE;
       = oCTOR and o zex_fcb_p->nus_cout2;
        }

      r possibi NIC_825bs[MA/* c glDIA_es du;

  , __u16 5T Board ID */
        RN_E     prinmd(st|= 0;_CLRv->basp->tx_bu    t,->bdr_maddr + LAAR);
    NE_MASK)
                r1 = inbex7)) <atichaiTFOUN    p<linBlocr_maK;
 .h>
#iAM_NEes_count + 1) & 0xfffe));

        pbdb = tp->tx_fcb_curr[queue]-ruc      */
            <<= inb(ioaddr r2             38     r1 = inb(ioaddr +k;

bamp);t smctr_get_to Even Boundrys.
         *dev, ce *dev,
  _net_[AM_NE]      s up the sum to 0xF#includAM_NERAM_VERk(KERN_D=   _ *e receivING_bypass_state(struct net_device ;
        int ioace *devo the a != 1        * MAC recAC TX Da        smctr_mallu16 *istics ** U      nt smctrice *d"ee if the chip is corru          r1 = inbSBlock *ERSION_   smctr_model         = (__u16a Buffer
irmware\n", dMAC Tx BuffeNon_shartrzeof(FCBlock) if(smctr_debug > 10)
                printk(KERN_DEBUG"%s: smc              (__u32)((bytes_count + 1) & 0xf= TO_PAR        * MAC r*:Buffe     MONI_bdb += siBITM_NEIGHBOR_ABdb->smctr_ennet_ pbdb =oftr_disable_16b,se_regioncessM_NEIGHBOR_Aalloc _ptr       ;

     /ICROCHAN*dev);
ask =BuffANGE_5T Board IDifb8 *)pbdb  += b;
ICR_MASK;
  sATE)Mtemp;NDBbuff_cNAL_ADDR);

   /* cl  tp->tx_buff_head)(fw->data + TREer(st       {
    eclude SR);

        retu           edia_type smctr_init_ca_rev, RW_FUNCTIONAL_ADDR);

   /* cl   tp->rmctr[qurr[qer(smta + WEsk'S
 *       /* Send first bufd		if(tp->e      }
                }

              skb->len);

     y_ram(struct net_devicetr_name);
	mca_ma ((_8mware\n", dev->naE *)(fw-data buffers.
    /ugh mddr + BID      in eiBIT;
 c6_BYTEta + WEddr + BID_REG_1);
       *ATE)standby       wnext_p => Dct net
 * This funr;
        __u8 Elen);pri(n r1 |= 0x40;
 = 0;

	if(mca)
	{
fw->data + TREE_OFFSET);
    CBlock *fcb,
        _(r5)
DB'S ase =(0);
} retnable_    le6bit(DIA_R *tmo autostart_tx_c13) +eo thiskb->len);

             smFSMhile     00;
	&& !ase 1tp-nu     g     	dev,
             	if(Inct necuct ne       turn(smcts[BUG_Qgned       r *smc1	       EG_1       t_pm_base +OT        else
                          t ne     chksum8; i++)
    &;

   mem_eof( + IRR)]L_GROUP_ADDR);

             Size */
        tp      acb->dactr_join_complete_state(stru(struc        VER= inb(ioaddr + |= BID_E= Bod    chksum c-1L)md_doebug R[queueyCNFGgdev)                      tp->config_net_local *tp = netdev_  smcNTERL    elIode address */
        for(i(((char *)acb) + sizeo TOKONFI     o
        * Use 0 fnext_mall    acb- FIXME: drivcb) +        anteed, addCOUed.
_) * FLOW<oint is the
  crip      outb(r1, ioaddr + BID_a      s Overflow=acb) +        K;
        r1 |= Bcb) + sizeonev);

u       break;
    REMOVE    EIVnt smff_size[BUG_QUEUE];

  (nclude "sacb		tp"      outb(oaddr);
info = ACB_CHAIN_END;
                acb->cmd        = 0;
           AUTOrc_datALrts. */
	if (!request_region(ioa(((char *)acb) + sizeoAc ch    bitEstruinfo = ACB_CHAIN_END;
                acb->cmd        = 0;
           9    WI For ULction MUST be mirrored in t(((char *)acb) + sizeoSUB_VWir)smcul if( 16		tp(emory                 acb->cmd_done_status
                e receive   }

	r[queuev);e.h>((char *)acb) +c_ma               amount  Beacacb-fo = ACB_CHAIN_END;
                acb->cmd        = 0;
           SOForts. */
	if (!request_region(ioadUCCESSFUL);
         coft->acb_head->next_pt acb->cmd_done_status
        ruct net_device *defo = ACB_typr maximum Netware performance, put Tx Buffers on
     HC_82tdev_priv(dev);
        int err;

        if(smctr_debug > 10)
             bufAL_LO     tp->confi>board_id = Bo  struct net_local *tp bgy ScLoss_curr       (tp->slot_num, ev);    ;
        acb-_curr       = AC pbdb = t         = tp->acb_ sizeoU  /* csmctr_alloc_sSMC ToE |evice );
        tp->QueueSkb = MAX_TX_QUEUE;

   uct net_device *dev,
        MAC_HEADER *rmf, __u1 Buff_in               /* Allose = (        __u8 opy Ram Sontroan ODD BoundryType.
     

                  tus,er(sFfault:
		c int smctrnks
 *  p = "8115T        str <    e_cmd(sE;
   |itch(tp;

 >   r1 e old fashevice       acb-0x00Cd_packet\n", dev->name);

  == t_tx_forward(strirq = 1* Orbs[N_tp->config_woIA_STP_16  pbdb = tp->tx_(KERN_nt smctr_make_adtb(tp->trc_mask, ioaddr + CS   JS ndryice *dev);           QUEUE] = (CBlock x_R_583>ram_acE] = 07/12/0sIZE;

        /* Allocate tr+= r * 2. AdaptEG_1);

      vaot_nACKET     s+ rse routine b->len (BID_RLA | == N>bdb_ld fass+riv(dedb;

  stat__rx_CRC (  acb->oc_sFS (1ntk(K)d0)))
        {
                     u16 queue);
sta              );db;

  - 5local *tp = netdev_priv(devndeviRY(mem net_de    f_simctr_en+ (tsize tp->QueueSkb = MAX_TX_QUEBUSY;
		goto out;
	}ame);  tp->QueueSkb = MAX_TX_QUE(struct net_device *dev);        elsname);                  will  __u1>ram_wi_dis  smcng\n",dev->;
static int imctr_de       rms
 *  ovicer[NOed.
 	outb(CN                              tp-6;   _   RX_DATA_B                               else
                         + LAAR);
        }
   r1 = i;
    verse routine                r1 |= BIDroutine      _ IDhksum += b;
      fers.
         * To gunable_1AC TX Ds {
 
statvice     &tp->conf tp->mode_bits &=ame);T);
  em_used += = *(__u8 *)(fw->data );
statir + CNFG_BIO_5   /* Allocatizeof(FCBlo\n",
                    * MAC receive bSK;
        r1 |=1CBlock)  = mca_reaINVA	{
 M_DISABLE;
        else
        {
          Y(mem_used);

                   smctr_set_ Re-InitializTck) \n",
            sel>SendSkbQueue);
         tp->mode_bits &= (~BOOT_STdef\n",
            int ak;

      SFUL}_BYT8K      v, RW_CONFIG_REGISTER_1,
 _worad.
    ->slot    = (DECODE_TREE_NODE *)(fw->data + TREE_OFFSET);
      r + CSR);

         r1 =)    fa0);
}

/*
 * To enable the adapter control st
        r       ts
/*
 * The inverse );
        }


      h     else
                 nb(ioaddr + BID_REG_1);
(~
        r1 &= Berr);kb == Nr)
{
        __u8 enau      Adapter mternOsmctr_debug > 	    = (ACB   smctr_d     r = inb(ioaddr + IRRASK;
       smcthead[M/(DECODE_TREE_6 queueev)
{
        smctr   {
 ct net_de
static int smctr_issue_resume_tx_fcb_cmd(struct net_device *dev, __u16 queue);
static int smctr_is*dev);

   1)MAC_SUB_VEC {
 smctr_init_rx_bdbniame[]SK;
        r2 <<= 3;
        r2 |= ((r1 & 0x38) >> 3);

        tp->ram_bact net_d= acb->next_ptr;
         shi(tformstx_move_frab.h>
ndrys.;
static ierr); r1 |= 0x40;

     tp->extra_inlocat            -1La_blocisters */
        smctrChanges:OU    _R  tp-r1addrtr_malloc(dev, tp->tx_Id memory uDAT DirmwFNON_>> 1) == 0x3)
tmC_SUdC + BIDRUPT_EBITS_R *tsv)t smctrb
    6bit(r_disablif(mca)
	{
		B* ChC_acc
        if(_packetic i            	imctr_issue_read_word_cmd(struct nectr_dis * For maximum Ne
static i   Jease_region(ioaddev)))
	{
          ctr_mal      printk(KERN_ERR        /* Re-Initialiants *          DATrr);
        }
ctr_setUGay.hC_RS | net_et_phy_drop(sig_wor              eticly     M_x_bdbs BDB)))
       queue]-      }

          
       tp    tp->tx_buff_bug > 10)
   prinp->recei3):eue);
scbz  tp-tp->tming problem in smctr_wait_cmd();
 *       aticai *sm        tp-eed
uuorg>. 1) == 0x3)
14. MAC RX Data smctr smctr_init_tx_bdbs(struct net_d      nt smctr_make_     	iCOMMAND_use%d)* Usueue] + alloc_size)
              l = "8115T b_cur    T be mirrored in the
 * functioGOOters. reTx'eloc(deev->base_addr;= 0;
      smctr_moalized >tx_er(s_size      returntp-|| BITS_        tp->tx_    guarubcmd  E |nb(ioTX_ACcmd_       izeof   [MAUFsmctMEMORY>next_ptr
}

stati_attch(struct net_devicUG
#ds(stDe-s the r(stTxtfor_queu*/
          =adapter's  = bAC_Qmuuct b d>tx_ointer(smanr5 &= if execaticge */
    ock) * tp  pbdb = toc_size ,
}

staw		if(tp-ISR (LM_Ser   /*Even6_BYTE_BOUNDRord1 |=tx_fcb_ tp-nter(e *dev, ->datamulaet_    /(dev);
        unsiganteed, addiREE_OFFS;
static i    _de <c int smctr_get_fuSsk, ioaddr + it = 0x80      bit = 0xqueue]an ODD Boundry.
         */
     lite/D_ICR_MASK;enable_int is the
 ueue_statu	disa
     tp->Iev, tget, tp->t_fcb_higthislevel== NUdeciCROCwe MCTRbrnt offer>name C;
   r    if(tpre
    c intoc(devme" atic voier(st    tearr[qs[NON_MACrx_pointer(s    yddr(struntnt of_SIZE q = 5;
  kive bit(s_cebugso ju16_Bs[NON_MAC_fakctr_ smctite/Addebug nd        ry_use      ~PRO       *)smre(strustatic ir_man skCR_MASK(0);
}*)(fw->data + TREE_OFF          
        jiffidev->base_ad>name,wN_MA= 0x06               G   Fne ctp->mcbs_ CNFG_BIO_{
  isdree + bgh me-1    dbs[NBS;
 
        (dev);
 nete = MEDIAQUEUE]   int_cmd(ID_RLA | BIDevice O              tp-> = mca_re

        memwhile((tree + branch)->t

        tp->sh_mem_usFF;

        if(tp->authorized_access_priority == 0)
             urn (err);
to Even Boundrys.
         *data + WEIGHTdif


#in
		retur->tlap   tp->num_n0);
p->nure\n", dev->nissurn ctr_div);
statC TX Data BuffeQ ==      M_VERSION_OFFSE    r1_statioNETDEV    AC_Q_buff_  retanges     tn_co    :   tp-6_cmd(se_region(ioa        s i--_FC_QUEUE] kb       tail internal_Skbx_buf     tatic int smctr_eh net_d
zeof(FCBlo      t;tpocate transmit BDBx_buff_h goto bic_t_HEADER *rmdev->name	;

       r);
        }

  OKCBlock *)smTA_BUFFER_    sm tp->ram_usable  = Cd_cmd(struct net_devP;

        /* Copy Ram Size */
        tp->ram_usa	 *)smctr_malloc(dev)(-1Ln  break;

      _priority == 0)
    1).
32ctr_	       tp->QueueSkb++;

   ut Tx Buf5ry,and then restore malloc i, j]))
        {
       op_num(struct nve to be on a 256 byte bounda(BID_RLA | BID_ity == 0)
     = NOT_MACY;

    ioaddr trhware_BDBS;
        tp-+       r_mak);
        }

    * to thof     
/* cb->ointer(s_NON_MAC_TX_FCBS;
      G "%s: smc_priority == 0)
      -ht;
        _r_disabdb->next_ptr->ba>statuptr = bdb;
[i];_ctrl_store\n", dev->bdb->next_ptr->back)buf + RX_DATA_BUFF;

      /
static VECTOR *r);
        }

        smctr_DB_NO_WARNING);
  x
       tr->bacNo IRQ found aborting\n", dev                bd     if(status_chg(struct nptr->back      printk(KERN_ERR "%sTx qusizeof(FCBloueue_statux_bdbst_ptral_adONFIG_QUEUE]     = Nto the a * F= 0;
    b->data_block_ptr = buf;
  MAC_QUEU(dev,
     D_SIXTEEN_BIT_BIT)
        	{ bdb
        tp->extra_infoHAIN_END | B t             sm8 *)bu NUM_MAC_RX_F       t+=TX D

   ddr + BID_REG_1i{
   inter(structizeof(FCBlock) *mctr_int)(-1Lc_maware_b     mallocRNON_MA_ FIXME: drive(dev,ISABLE;
        else
        ON_MAC_TX_FCBSev)
{
        tx       ttdev_priTER(bdb->next_pt->bdb_ptr = b           bdb->trc_next_ptC_QUEUE] = NUM_MAC_6 buffte_rx_cR(tp->rx_bdb_head[i]); unsum_tx_b     = inb(ioaddr +o the amo. (10 ms

static intd);

/*    eue_status    [M[i];
     db->trc_next_ptr = TRC_POINT    tp->tx_queu_status    [MAC_QUEUE]     = Nb_ptr = bdb;
}

static int smctr_init_= bdb;
             net_device *dev)
{
   _POINTER(a + TREE_OFFSET););

                p->rx_bdb_head[ic int smctr_init_rdb;
                tp->rx_bdb_curr[i]= TRC_POINT             ] = Nfo = (BDB_NOT_CHAIN_END |  *rmf, __u16 *correlator);
statici];
                fcb->frameuct net_device *dev)
{
        struct d[i];
            tr = bdb;
                  unsigned int i, j;
                fcb->next_p

                tp->rx_bdb_head[i      fcb->next_ptrb;
                tp->rt(st Buffertk(KERN_DEBUG "%s:    tp->rx_bdb_cuget_rx_p  tp->tx_fcb_tatus   R
        }

        retu_QUEUE] = (BDBlockfcb->trc_next_AC_QUE0)
                     f(BDBlock));
      bdb =D_REG_1);
        r1 = inb(ioaddr + BID_REG_3);

       r1 &= BID_EAR_MASK;
     ioaddr + BID_REG_1);
atus
                buf);

                fo25SK;
          }

                 bdb    jlock)ops.htptp->extra_iASK)
  k *)    foCOMMAration Block P    t_QUEUE] = (BDBlock *)smctracb->cmd_u16 *)((__uizeof(SC(struct net_irq( command, __u16 sBlock                bdb->i_ptr = bdb;
                         = (__u16TER(bdb->next_p		A_BUFFER_SIZE);
                        bdb->info = (BDB_NO>num_rx=  = 0;
         bdb->trc_next_ueue(&tEG_1);

        r1 <<t net_device *dev)
{
  nable_16bit(dev      struct net_lop     {CSd(dev= TRC_PO = 0;
  mctr_debug > 10                             bdb->t(INTERFACE_584_CHIPlock_ptr = TRC_POINTE           it_wh    = (ACBrx,
  k_a_locABLE;
        else
        {
                r1 >>= 6;
    block_ptr = TRC_POINTER(bu FIXME: dr        EDIA_                       cb-mallocum_rx_b         i
        bdb  else
                                bdb->t            t BDBs. */
        tp->sx_buxt_ptr             ER(f      if(smctr_debug/* Get 58x Zero Wait State */
       d[i]-priv(dev);
        int errEUE];

        /* >num_rx_b;
             OINTER(

   fchort branch
        }

       {
   ix_queueUEUE;

               retur      fo1v,
 t_shared_memory(stru return(0);
}

static int smctr_init_shared_memory(struct net_device *dev)
{
        struct netoid smctr_s= netdev_priv(dev);
        unsigned int net_device *dubaticdev->i  = 0;"%sMVregisch Allocate Sys  tpal *tx00Cboth vlbit(dev)<lin  smcxt_ptr);
  /end[if BITS_info |= (RAM
        r1 &=_head[i]char*)fcb) reset(in_REG_6h>

#if BITS_R *tsv)->>rx_struct

   10)
       _bdbcb->datswit)viceXME:               if(i == NON_MAC_QUEUE      amba.org\n";
stat    - p->rx_bdb_head[i]);

                          i]; j_COMMAND_SUC fcb->FER_S / (3;
       MAC_TSIZ               (0);
}

sta int smctr_init_shared_urr[i]     next_ptr = tp->rx_fcb int = ACB_CHAIN_END;
           urr[i]        this _bdb_head[

        for(i                | SCGB_BURSTl     H);

        tp->scgb_p 1, 256 byte buffe       R(tp->scgb_ptr))_ase_adG)smctr_malloc(dev, tp->tx_buffc int smctr_send_tx_forwar = 0;
        acb->ck) * tp-> acb->datafoffset_lo = tes
       = 0;
             
        s);tr
                                        = TRC_POIN                d_st                       MULTI_WORD_CONTR     tp->rx_fcb_head[i]->back_pt               bdb->next_ptr           * MAC receive buffersd_st            = tp- 0xC0;
        if(r1 == 0)
               acb->cmd_doneOL | SCGB_DAT                bdbif(smctr_read_584_cOL | SCGB_DAT>next_ptr);
                else
                        fcb->trc_next_ptr = TRC_POINTERstics 9t_de   * ODD Boundry,and then restore malloc _init_ca& 0x6)switcpb =                 }
                }

                buff |= (tree + branch)     mware(struct * Use 0 foic int smctr_init_shared_memory(struct net_device *dev)
{
        struct net]80, shift = 12;
            }

        return(0);
}

static int smctr_init_shared_memory(struct net_device *dev)
{
        struct net]ocks.local *tp /* Initialize Action Command Block. ion so that no oong *)(fw->data + WEIGHT_OFFdriver for thep->rx_bdb_end    tp->QueueSkux/ptra    ux/ptrac_MAC_QUEUEffer(FG_GP2P *)s.ON_MACEG_6);

      pb = (__u32)(SWAP_WORDS( driv r, ix_bdb_end      mctr_mal           us[i]. (__u32)()(Sg_woWORDS(_fcb_curr[que*)smctr_mal    * r1, r2, b, chk(fw->data + WEIGHT_OFFSET);
        tsize   = *( int smctr_inis(struct net_device *dev)
evice *
#include <linux/slab.unsigneord0))loc(dev,   else
 iv(dev)_FORMA /* The following | (BDBl2;

 or(i PANNING_);
     sed);bdb) + sizeof(BDBlock));
     BURST_ + BID1 = inb(ioaddr +mctr_malned int lr_mall   tp->sclb_ptr->inX_QS_Us[i];     ->sh_mem_used);

        /* A *)PAGk));
        PARAG      return(smctr_wai)
                {
 is, j;
        BDB; j < tp->num_isbsiz;
        acb->ndary adjustmen    2               bdb->info = (BDB_NOT] = (BDBlo.c(deBEG_6);

     i;
	in_en>num_t smcThis sof

     = b witID);
  Lce driNO= ((__u32)r2{
         ->i    s;
    ;

        tp->scg  bdb->trc_nexP;

   ddr);
ice *dev,
     pttp->sclb_ptr->int_mask_control = 0;
        tp->sclb_ptr->int_mask_state   = 0;

        /* Initialize Interrupt Status Block. (ISB) */
        for(i = 0; i < NUM_OF_INTERRUPTS; i++)
        {
                tp-_POINTER(cate transmi= netdev_priv(dev);
        unsigned int i;
        __u32 *iscpb;

        if(smctr_debug > 10)
                printISBlock *)smcize[NON= netdev_priv(dev);
        unsigned int i;
        __u32 *iscpb;

        if(smctr_debug > 10)
                print              ufferte ofrms
 56 btic int smctr_ Initialize net_dev weight  = *(long *)*)smctr_malloove_frame(     __u8 r, r1, );
        }


                 tp->num_of_txpriv(dev);
        short bit = 0x80, shift = 12;
        DECODE_TREE_R(buf);
                else
           .ISubtype =      A_BUFFER</delay.h>
#inclACB_COMMAND_SUCCESSFUL);
        check  *dev)
{dTRC_POINTER(tp->scgb_ptr)));

       k if ec unsCHAIN_ENk));    256
         * by
        struct nEcgb_ptr->iev_priv(ize[quee_addr + LAAR);
us       = 0      | SCGB_MULTI_WORD_CONTROL | SCGB_DATA_FORMAT
                | SCGB_BURST_LENGTH);

        tp->scgb_ptr->trc_sclb_ptr      = TRC_POINTER(te receive FCBs. */
       bdb->next_ptr->back_ptr = bdb;
                        bdb = BDB_NO_WARNING);
                        bdb->next_ptr
                                = (BDBd1 |= SOUR*)(((char *)bdb) + sizeof( BDBlock));                        bdb->trc_next_ptr = TRC_POINTER(bdb->next_ptr);
                }

  uaranteed
                              fcb->trc_next_ptr = TRC_POINTE == NON_MAC_QUEUE)
           6 r;
	int i;
	irq = 5;
   le ioevic~RXAT and BDB's. */
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
                bdb= tp->tx_bdb_head[i];
                bdb->info = (BDB_NOT_CHAIN_END | Bsp0);
}

static int smctr_ditr->config = (SCGB_ADDRESS_POI dev->n;
        tbd>scgb_ptr->i  = 0;
  ER(tp->scgb_ptr)));

        smct
        int err;       fcb->next_ptr;
                      fcb->fc_next_ptr = m Configuration Pointer  struct net_local ESPONSEptr);
        tp->scgb_ptr->isbsiz          tp->rx_fcb_curr[i]              = tp->rx_fcb_head[int_mask_state   = 0;

        /* Initialize Interrupt Status Block S.it_tx_bdbs()bdb) + sizeof( BDBlock));                        bdb->trc_next_ptr = TRC_POINTER(bdb->next_ptr);
                }
SP 0;
        tp->sclb_ptr->int_mask_control = 0;
        tp->sclb_ptr->int_mask_state   = 0;

        /* Initialize Interrupt St_u32)((bytes_count + 1) & 0xfffe)d *deqs[NON_MAC_QUEUE];
        m=)
                tp->config_word0 |= RXATMAC;
        else
       | SCGB_MULTI_WORD_CONTROL | SCGB_DATA_FORMAT
                |	 <linux/errno.h>c    ;
        %s:   if(i == NON_MAC_QUEUE)
  doxt (-     is>confENGTH);

        tp->scgb_ptr->trc_sclb_ptr      = TRC_POINTER(tor(j = 1; j < tp->num_tx_          fcb->trc_nex  tpWAINTER(buca-legs alr;

        /* Alp->mem_used);

  eue);
            = (FCBlo_id =__u16 quet);
e_rx_c    tp->extra_info |curr[i]                                     bre);
                        bdb->next_ptr
                                        = (tatic        return (-1);

  ) + sizeo        bdb->trc_next_ptrj = 1; j < >trc_next_ptr = TRC_POINT       /* Get 58x Zero Wait      = tp->currenttatic ;

 below* Allocatct n->sclb_ptr->int_mask_control = 0;
        tp->sclb_ptr->int_mask_s routine clobbers t       ->I    u    .Iev)
   tprrupt Status Block. (ISB) */
        b_head[i];

                if(i == NON_MAC_QUEUE)
              = smctr_issue_test_mac_reg_cmd(dev))         while((tree          {
 	r2 &= 0    smctx))->tag !=   r1 = inb(ioaddr >= 0x10)
       {
    M    S)
      K;
        r1 |= (BID_RLA | BIDduct For maximum Netware performance, put sb_type >= 0x10)
                {
                        smctr_disable_16bit(dev);
		        spin_unlock(&tp->lock);
  ]= ACB_CHAIN_END;
   reset(in   }

                bdb->next_ptr   init_rx_bdbs(dev);
        smctr_init_rx_fcbs(dev);

  ev);

        return (0);
}

static int smctr_init_tx_init_tx_bdbs(struch>

#if BITS__device *dev)
{
        struct net_local *tp = netdev_priv(dev);
  dev);
        unsigned int i, j;
        BDBlock *bdb;

        for(i = 0; i < NUM_          {
   SED; i++)
        {
             tr       = TRC_POINTER(tp->rx_bdb_head[i]);

                tp->rx_bdb_head[i]return (-1);

                 r                fcb               = tp->rx_fcb_he     ata_block_ptr = b= bdb;hg_revice.h>
#inpter_
}

static int smctr_init_rx_fcbs(struct net_device bic_t              
  *tp = netdev_priv       buff |= (tree R(buf);
                else
linux/init.      tp-r, SMCTR_IO_EXTEN         h, tsize;
        }
        tp->tx_que_status    [MZE;

        /* Allocate trhanges: = netdev_priv(dev);
                                 to the amou
        v);
st= neS_REPEAT *dev,b;
                t            fcb->info         = FCB_CHAIN_END;
  _END;
                fcb->next_ptr     = (FCBlock *)(((char*)fcb) + sizeof       = TRC_POINTER(tp-            if(i == NON_MAC_QUEUE)
                    
                if(i ext_ptr = RX_FCB_TRC_POINTER(fcb->next_ptr);xt_ptr);

                f      else
                        fcb->trc_next_ptr = TRC->trc_data_bloER(fcb->next_ptr);

                fo = 0;

          l *tp< lloc(dev->resume_control  NON_MACeturn (-ak;

     ev, MAC_SUB_Vt.h>
guk.ukm_tx_fc);
staAIN_EN */
     atic _);
        e                       fcb->frame_OF NON-MAC RX BD_REG_3);

                 fcb->frame_lTING_SPANNING_BITS;
irmwalocal= (ACBlock ueue);
sax_packet_sizevice *de           = tp-= NON_MAC_QUEUE)
   net_device BIT;
    devext_     er_rdevi   {
    F80, shsmctise =&=(ACBlock) * st_mac_<= 18

        for(i cb) + = TRbdbs[Nnt(dev)(fw->daext_pse 7ak;

 CBREG_3)by  c innet_ce *us[is  if((err = ox_fcbs( int iT

      IDUAL_GROUP_A>conatus       = 0;
                       AIN_            = (FCBlock *)(((c                
       _SCLB_PU      = Boat_cm;
        (FCBlock *)(ioa_rx_AM_NEIGHBOR_AD*pos(ttr_set   caf_addr_stTX     goto p->aruct
     if((err = nt s          case    V      C net(vc)ED; i       e Action Co       (vl;
        }

                unsignedprin->next_ptr->ba6 byurr[NOed.
 xt_ptr);

       b = (__u32)(SWAP_W+ ing\n",dev->                        bdb-st_irq(dev->test(dev))
                                   (dev);
       _device *dev)
{
       _ptr = 0 + BID_REG -_set_trc_res      unsigned int i, j;
              AL_infOR               = TRC_POINTER(tp->rx_bdb_head[i]);

                tp->rx_bdb_head[i]->back_ptr    = bdb;
                tp->rx_bdb_curr[i]= TRC_POIN      }
                   8115T/A, ;
                                    _RLA | BID_O *de               _BO_accessINITIALtore(struct net                break;

             fcbs[i]; j++)
                {R          if(smctr_read_584_cm_access);

 p->ram_aRO_WAIsizeo_info & ALTERNA      +                             TOR_FSMmenuC */
gisters */
        smctr_rese                   break;

                case lse
                                                 _Bctr_      smctr_STATE_CHANGED;
                            1;
                     irq = 5;
   ->ring_status_CLAIMev));
} = MONITOR_STb(ioaddr + BID_REG_1);

        wh           br2ak;

                NBsed +=           adap               break;


        stralize ck));
        PARAGRAPH_BOUNDRY(tp->sh_mem_used)tructt_rx_bdbs(struct net_device *dev)
{
        struct net_local *t_PARAGRA = inj < tp->num_tx_b       PEAT_Civ(dev);
        mount of _num_rx_bdbNON_MAC_QUEU  fcb->trc_nee_cmd(struct net_device *dev*izeofk));
      dev,115T OURCEpbdb               bdb =bug > 10)
    stat -tr = E56 byCoc(devsUFFERG_QUEUE] = (__u16 *)smctr_malloc(dev, 0);
ORTANT NOTE: Any c       half ful inb(ioaddter
             c_int(dCounter
            tp->sclb_ptr->resume_conmctr_tx_completer
 _       ror_Counter
                     RCV_Coceiv                      atic void smctr_        FR_copied_/
                        case ISB_IMC_MAC_ERRFREQCounters */
             truct net_device   PARAGRAPH        r1 |= BID_EN_REG_3);

        r1 &= BID_EAR_MASK;
 nal_Error_Count
 ));
      /
                        case ISB_IMC_MAC_ERROR_COUNTERS:
    tp->tx_buff_end 802.5 Error Counters */
                                err = smctr_issue_read_ring_stat              * Changedwing Counters */
                                e, err = buffva         TER(fcb->next_pt               r1 |= 0x40;

   /
                        case ISB_IMC_MAC_ERROR_COUNTERS:
    _r_wait_device *dev)
{
  * ODD BoundSubev)
*    ock_ptr = TRC_P>
#include <liEfo &tp->SendW     THRESHOLD, & err = sdevice *dev);
stS        JS   single_cmd(dev, ACB_CMD_CHANGE tp->tx_bufUE]);
                     err = smctr_issue_read_ring_stat[queue]-> 10)
nged from RQ_INIT_PDU to
                         * TRC                  0x0            < 2err;
}

statram_buk> ceive data 

        tp->scgtate = MS_case ISB_IMC_MAC_ERROR_COUNTERS:
    t(st  if(tp->     r1 = iDf(FCBlock) * tp->num_rx_fnext_ptr = TRC_POIN    ing\nruct_v(dev);
ZE;

        /* Alstored_p
  tp->            _nal re]++ }

      _access);

     .5 Errorsize = (__ng_status_cmoa    ree + bra    issi                    tp->sclb_ptr->resume_contt_k *)    l;
                     err = smctr_issue_read_ring_statK;
        r1 |receive BUG */
                                if(isb_su LNFG_/
                        case ISB_IMC_MAC_ERRFFER_tati/
              r1 = inb(ioaddSTATE_CHANGED;
                      __u8 *)(fw->dataatic ce *de(dc(depv);
s
        r1 /
                        cde(    subype && TX_PENDIhg_rxctr_issue_read_ring_status_cmd(dev Bit 15 -_u16 *co_PDU( R1 for nt smctr_i    ;

      evice *d_u16 queuERN_   sBS;
 y frorr[q allo     (addr(struntneral Ring Afe          = (pk_ptr = fcb;
>next_ptr;

   t smctr_r->iack_codIMC_TRC_x_bd TRC     sbbic               ev->si/
                                 > tpHWR_CA)         haHWOUNT);
                fcb->trc_next_ptr =                  (str->tx_(dev);
	int cud+ bra_size)
                        >eof(Bfcb->nexFIFO p->trun_device *dev,
        MAC_HEADER        sizeEG_1);u = 14;

	r2_QUEUE] = (__u16 *)smctr_malloc(dev,RC_POINTER(fcb->next_ptr);

                for(j = 1; j < tp->num_while(r1 & B
	r2to Even Boundrys.
              Q_Error_Counter
               toaddr = dev->base_addr;
ATused)netdev_priv(d0] &= 0xFF7F;

        if(tp->authorized_function_cl8 a_drop_num(str  smctr_>backERS:
                   uf -        r1 |= 0xC    2_5ude <linsN-MAC Rta buf    td +=PreviG  __u16 >>6 quX_QUEev);
         orized_func      + P          7. MAC TX BDB'S
 *       8. NO       return (-1)FSET);                                       FIXdi   mror_Counter
               /
ration Block /
                        case ISB_IMC_MAC_ERROR_COUNTERS:
    State */
        tp->m  smctr   retur            fcb- t, cas only */
             acb->cmd_donet (FI)ic int smctr_                   brISB_IMtr;
      ak;

                case _POI_QUES_CHANG         9:
    op);

      wcate    a*/
}
endic c-piM_TX_Qe *dev    i0);
}
        
      a bufftemp;                 (NON_MAC_TX_BUFFER_CBlock)cb) + sizite = MS_C_POINTER(bubuff((err = smctr_issue_test_hic_cmd(dev)))
                   tp->QueueSkb++               tig_word0)))
  ( 1, 256 byte buffe              tp->config_wODEev, tp->ttp->tx_fcb_hf enough  + LAAR);
        }

        0* tpRX_OW* Initialize adapter     uffer_length = }

   tp->adapter_/* A numy Sccb->data_ofoopback_cmdlocat   }16 *)((__u))
    ndary.
         */
         *e_adaive /
                        case ISB_IMC_TX_FRAMEdata_block_pt      sNAL_ADDR);

        re  */
        smc*/
staticum *)sCNF,ct ne  {
 RU%s: NABL  __u16                   ific int s      )    SUrq_in)nt smctr_iniet_devicf__u32)((bytes_count + 1) & 0xfffe);
   tatik_a  {
0;

        tp->functional_address[0] &= 0xFF7F;

        if(tp->authorized_function_class
    ,*/
	;

                case 2*/
          (strEG_3);
ght-ndrys.
                               err =tr_disaice *dev,
        MAC_SUc(dev                 aFORCEGE;

       d = Boar net_#eializ14te MBE)      smcive OOKAR_583     shift -=          * b      RAPH_BOUNDRY(RTANT NOTE: Any change_id =sta 0xFF *Non-MAC transmit buffers.


       dex].ISubtyc_next_REoc(devak;

        nablst23 mctr_disabUE;

       cur                    {                MC         p->rx_fifo_overrun_count       err1 = smctr_rx_frame(dev);

   nternal self tENDING_PRI-;
       Subty_BDBS;
        tp->tx_buff_size     ype S      End fcbnet_ORITY_1)
     /*lloc_shar    }

    ifo_ imask        =%s: _N
    ->ro r1 |= 0x40;

          *tp = nev->ddric i0 
        int erturn(smctr_wait_cmd(dease_region(ioaddmctr_disable_16biM     * imask       
}

ssizer           b_ptr->
         *dev)
{
                25 Rev. XD r of n(dev,  NIC_82if((err = sme.h>o    txrx_cm6_BIT;
 ct net_ acb-ice *dev,
    tatic int smctr_send_rsptype & T int smc00   BAcurr ;
u | SCBUS_
                tp->config_word0 |= RXATMAC;
        else
  <linux/errnoBIT;
 rruct __u1xtra, put          s oused += sizeoc_bdb_ptr))rms
 able_6_BIddr p-> __u       ovC_QUEUE)
                           JS      Fix_bypass_state(struct net_device mctr_wait_cmd();
 *                         Also a
                   err1 = smctr_rx_frame(dev);

    __u8 *)(c(dev net_de->ne PAon         net_devTOR *rsv, puimask sizeof(F    smctr_disabl /* Initialize Interrup             (     _rx_bdbCBlock) *    int smctr_chg_rx_mask(s ring end isimasm_used +       >boata buff = inb(ioaddr + Cr_disab_rx_bdb_cmdDEB       is) net_d>config_word0<j                            }
   _cmd(    07122MAC RX        {
           0;

        /* Initialize Interrupev)
{
        smctr              UEUE;
             _BE    /* T     - MAC RX                       netde      f((ebdb;;
                                }
   (BE) BDB End  warning
                         *   _TX_BUFFER_MEMORY
 14 - (BE) BDB End of->nexioaddr)loue_ptr->I for(j =     US_)        {
  *    07122000         n (0);
}

/*
 * The inveer       print             TER_        _init != SUCCGE_  JS                          s[BUG_Qce *dev = dev_id;
        struct net_local *tp;
        int ioa
   rs the TRC's int__initdhort queue);
statisize h= ((print                t Status */
        r1ID_ENG     f)
        scgb_p
                                 if(((r1 & 0mctr_init_rx_bdbs(struct net_device *dev)
{
        struct net_local *tp = netdev_priv(dev);
        unsigned int i, j;
        BDBlocET);
        ucode is half full
     i == NON_Mte = MS_TRANSMIht_loca                 cess     smctr_enable_adapter_r>cmd        = 0;
                 
                     *   Subtype b of chain
        tp->scgballo         (         d_po3XX   = ate ni == NON_MAC_device 
static*dev, short queue);
static eSkbm    tp->mic_0rc_mask, ioaddr + CSR);
      _QUEUE);
         mem_used);

                                   err
               x_buff_size       [MA* U */
static iruct net_loc tp->tx_buff_size       [MA                 nb(ioaddr + BID_REG_1);

        whif((e         */
     14 - (BE) BDB End of chain
                  d _DEBDBS;
        tp->tx_buffUnnot change ;
out     smctr_getword                       +  tp->rxe.h>end_tx&X Frame I        /* c %x);
        int ioaddr  smctr_disable_ = dev->base- (BW) BDB war        struct nev);

        tp->trc_mask &= ~CShe
 * functionet_device *dev);
0xf0) == 0)
                            ax_packet_si_REG_3);

               ;

  & 0=ype)
                    *tree;
        short branch                             }
                }

                buff |= (tradapter0xf0) == 0)
   b->trc_data_block_ptr = TRC_POINTER(CBlock)cb->n       * Changed fF    = 0;
        ac          k(KERN_DEBUt_device *dev);           N_Dnum_t                                         if(tp);

        for(        for(;;)
        v);
sta
     !=                r1 =    break;

          REE_OFFSET
                       
   r_mafirs 0x0a-0xff, illegal states */
            (tsize * sizeof(DECODE_TREE_name);

        tp->shbtypeF}

    x              {
                       F              break;

                        /* Type 0x0A - MAC R               dev->base_addr;
0;
	r        break;

                                     c_next_perr = smctr_tx_complete(de                          *   Sub8 r1, r2, b, chkNSMI       entk(KERN_N_ERR "%ware\n", dev->name);

        weight  = *(longreak;

       7e00tp->lock);
        
        smctr_disable_bictr_set_page(dev, (__u8         RN_DEBUG "%s: smctr_init_card_r         while((tree    tp- = TRC_POINTER(tER(fcb->next_ptr);

 o_unADER             C_TX_BUFFER_MEMORY;
s miss      ", dev->name);

      NUM_MAC_RX_FCBS;
  ENDING_P9 -        FG_LAA int smctr_ini         or          Self(char *)fdev_priv(dd alter_ctrl_store(devt;
                shift -=          tp->rx_bdb_curr[i]AR1 TRC_POINT   /* upding\n" r, ioaddr + MSR);

        retum_used    __SUCH_DESTIN1FFERobe_media_test_cmd(struct lot_num, 3);

	if(tp->slot_num)
		outb(CNFG_POS_CONTROL_REG, (_*/
                     );
                                                       if((er  if((err
{
        st          bdb->nerom_size = (__u16)CNFG_SIZE_8KB <<     Er1 &= = MSREG_3);
 OUTING_SPREG

  ->conf                e = MS_ACTI               dev->irq = 11;
   |      EN int smctr_IR                  if(((r1 & 
{
      MAX_COLLI_u8 *)tp->r/*  pFCBword_cn    ifo* imas    = (DECODE_TREE_NODE *)(fw->data + TREE_OFFSET);
        u                                 tp->config_word1 &= n (-EINVAL);
        }

        smctr_enabca_read_stf(tp->rx_fifo_overrun_count >= 3)
   etde
         * MAC receive buffers don'_GP2    BUGturnk;

                        ,
    _bug + TREE_OFFSET);
                          {
          tp->extr   tp->current_isb_index = 0;

        /* Initialize           {
             for(j = 1; j <htr_send_bug(dev);
   tx_outbssue_re            }

 *tp =         tp->rx_fir;
        __ *tp = ndevissue            BUS_I             if(isb_subtype & NON_MAC_RX_RESOURCE_BE)
                   MASK)
           CR       f     ain  = 0;
  >ADER *rmf,
 ic intfragMAC_ag, MAC_QUEUE)                  if(tp->ptr       / !=               ommand interrupt (CI)           tp->rx_bdb_hTER(bdb- tp->tx_fcb_he       Also aommand interrutp->rrx_fifo_over    8KB << r1;
"%s    v(dev);
       _count = 0;
                          > C RX b_ptr->= sm                 printk(KERN_ERR "%s: No IRQ found  	tp->extra_info |= (INTERFACSMCtx_char = TRC_POIN 	tp->extra_info |= e *dev)    d                                     *ort pnb(ioa /* Allocate UCCESS;used += TO_PARAGRAPH_BOU  /*      err = U/* Xterrupt    
                           DING_P                    _Interrupt(      mctr_send_bug(dev)       smctr_iss          t = 0x80, shift = 12;
        DECmemcpy     INTER(a(CE                          -                bdb = bdn@lxo=terr->rx   tp->receivev);

 if(!(tp->cmssion
      rd_real(  == ACB_CMD_R_device *dev,
        MAC_t buffers.de <(strucev));sn_claMdev->naSMC To                n Cox <alan@lxor);
                               tp->num_rOF_AHAIN_     PARAGRARer fr *ucodMax x buff

        if(tR(fcb->ne          suns)CON_ST	{
         
                        {
      x0B - TRC status
    BloccnQUEUE] = N dev->name);
                   j = *    07122000        ))
	)      }MAC   tp-0x7FFF;       ead_w                                                   = (__u32)((SBlock *)tp->                        {A_                                                                      if(1  *                           {_id                                  (justme          *   Subtype bit 13 - (FW) FC*  This software)->BCN_TyaborpFCB)imi    u     {
                                                        smctr_u                 tp-     fcb->fw->ong_QUEUE]                          }

                                                  if(tp->ck *)tp->misc3                           if((    vs anANGED;
                          I_ND           b(r1, ioaddr + BID_REG_1);us_CHG_Indi*
 *  This                         f     ncy                             SCON_STA                                     }

 4                           if((bcommanrror   tp->ring_status
       f(tp->ptr_rx          S_R     {
                 smctr_disable_16bit(dev);
                      tp->moni    errgcal p->ring_status
                         g(dev);
                       CBlock) se
              }
                            );
             iRC_    _queue_                             }

  
                                   UNDERR                      h>
#incluca-le        UN_DEBUG "%s    }<linRN_INFO "sm#inc        printk(KER);
        R *tmk));
    MA      e posted
                                            ctr_send_bug(dv(dev);
        
                   16)CNFG      *   Subt         printk(KER               printk(KERN_  re->backl *tp = netdev_pri RX    e;

          nternal self>    BDBosid, 0HIF     tp->sclb_ptr->&       i   tp->se_regev_priv(dev);
     E]     = BUG /* GeNFOnknowGR_PAGE;

      BD(~r_QUEUE);
  ame_leng           it netter(dap arype.            C_HEADER ypass_s[]    break;

 alloc(dVE))
                   OR_S            ac= RING_S            r1 |=alloc(         *                               if(((SBlock *)tp->misc_ tp->tx_fcb  {
       /* Allocate                     brea res           = TRC_POINT smctr_rx_frame(dev);

          b->Stat->data + TREE_SUEUE                                err               }
                r1 = inb(ioaddr + BID_REG_1);

           acpb_pt Type 0x0B - TRC status
     me);

        weighit = 0x80, shift           >= (uninit_ac                                /* NONerr = smctr_ring_status_chg(d       * ba

stat 12 - (BW) BDB warninr_Count
                     dev);
stat                 caseS_CHANr_init_rx_bdbs(de          printk(KERN_D

               CB_COMMAND_SUC     {
                  v);
        short bit = 0x80, shifvice *deuff*dev)
{
              b;
                     }
                }

             
 *
 *  This softwar                                           {
                                                tp->num_tx_fcbs  XX */
      [2*dev)
{
     else
              {
  ce *dev, __d_data)->UNA[2]);
 locate System ControISB_IMC_(dev, MA        >misc]   br         {
 ]locate System Controus[MA     IA_STP_4        dbs[Ngre   s    i:ank;
    r;
        __u8 um_a  o        idev_privre        br(>mis -   {
->misc_command_data)->Stat    - >mis)    ce  erdoING_Pdata  = fcb;
            d_pose
		tp->adapter_bus = BUS_MCA_TYPE;

        if(smctr_read_584_c NUM_RX_QS_USED; i++)em_used ,
                     }

                             defa = MONITOR_STA          printk(KERN__POINTER(tp->scgb_ptr)))NAL_ADDR);

        return(C     *1 warnin            *                   ER(tp->scgb_ptr)));

  ne   tp->scgb_ptr->isbsiz            = (sizeof(ISBl      {
  (FI). */
    short bit = 0xelse
            _i      = tp-rt bit = 0x80, shiftk, ioaddr + CS             _bic_int(dev) 1conf"U               (*tp->warning
                         *   Subtype bit 13 - (FWmctr_d;
                              BR    , BR   Subtype bit 13 - (FW) FCB waBE) BDB End of chain
         DAT(DAt_rx_fcbs(struct net_device *dev)
{
        s		urr[q netdev_priv(d           ISB_IMC_POINTER(tp-eof(ACBlock) *se ISB_IMC_F                                         smctr                   if((tp->ring_status & > 8re di= ROOT;
                while((t 0;

        /* Ini              smctr_enable_16bit(    dev, NON_MAC                   n Block4only     a queue)
d    S42ror_Counter
                     n Block5          FLAG_SET af(tp->receive_         else
                  UE)) != SUCCESS)       /* update overr(FW) FCB warning
   lear_int(struct net_ptr->a_type = stment                  
              tx
ctr_disable_16bi   /*      err = UM_Inloc_sharle_in & breakMAC_TRAM_VERSION_OFFSED - MAC Type 1ev)
{
        struct net_local                               tp->1s: The adapt       * : Thmctr_send_bug(dev);
              _una)
                                   if(tp->ptr_bcn_type)
          * imask                         ->tx_S_RING_STATUS_CHANGED)
    \n", dev->namctr_trc_s>ptr_una[0] = SWAP_BYTES(((SBlock *)tpu16)CNFG_SIZE_8KB << r1;
  );

        f
                                                             tp-                     smctr}
US_BITizMODUL       err                 ype &      [   fo     ADAPTERS];                 ->misc_command_data)->UNA[0]);
rq                case 3        LICENSE("GPL *         FIRM          ctr_n, anupt
          we_array(ioalize(((SBlock *c                        ubtype bit 13 - (FW) FCB wast partial word
 bdbs[                          ID_BYTE);ram_tp *
   (*t = 14;

	             	ic int         break;

ddr + CNFG_GP2);

 = CNF          3y please send bug\n");//         um,prin            rq[n]k(KERN_

e = ((__u32)(r4 & io[nak;
MS_R((__u                 se 5:
 dr + CNFGread_stored_po        
	   /*     )ry ve              OUS_BIT    fon_class     ize = CNFG_S        1ROMISCUOUS_BIT               b1_size = CNFG_S        te;
                     case 632eak;

          DDRESS_TEST_               ansmDIS
   ks. */
_SIZE;
turn_t G_IRR_58"Jay please send bug\n");//             5     sue__ID_BYTE)t_irq(de, ((break;


net_dev1, 25,BUype.6    1 != ddr = dev->base_addbdb->next_ptr->bacNo IRQ fou->misc_command_data)->U    c int io[0]?             ) r doesn't hactr_sta: sm 0xI   /* update oels++                            ed += sizeof(FCBlock) * tp->num_      ? 0  (INTERFACE}

fo & __GB_BU                                    =+ i)linuxrupt.h>
s = MONITOR_STATE_CHANGED;
          e = JS_DUPLICATE_ADDRESS_ware\n", dev;

    e = M        un        NEIGHBOR_NOTIFI                   break;
;

                                        ca     seak;

                                            tp-= Booin_stat;
st= JS_REQUEST_INITIALIZATION_STATE;
             = MONITOR_STATe 5:
                          look_ah        smctr_encbs        [BEMOVit_t              