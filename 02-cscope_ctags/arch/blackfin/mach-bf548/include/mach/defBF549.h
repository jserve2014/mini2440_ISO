/*
 * Copyright 2007-2008 Analog Devices Inc.
 *
 * Licensed under the ADI BSD license or the GPL-2 (or later)
 */

#ifndef _DEF_BF549_H
#define _DEF_BF549_H

/* Include all Core registers and bit definitions */
#include <asm/def_LPBlackfin.h>


/* SYSTEM & MMR ADDRESS DEFINITIONS FOR ADSP-BF549 */

/* Include defBF54x_base.h for the set of #defines that are common to all ADSP-BF54x processors */
#include "defBF54x_base.h"

/* The following are the #defines needed by ADSP-BF549 that are not in the common header */

/* Timer Registers */

#define                    TIMER8_CONFIG  0xffc00600   /* Timer 8 Configuration Register */
#define                   TIMER8_COUNTER  0xffc00604   /* Timer 8 Counter Register */
#define                    TIMER8_PERIOD  0xffc00608   /* Timer 8 Period Register */
#define                     TIMER8_WIDTH  0xffc0060c   /* Timer 8 Width Register */
#define                    TIMER9_CONFIG  0xffc00610   /* Timer 9 Configuration Register */
#define                   TIMER9_COUNTER  0xffc00614   /* Timer 9 Counter Register */
#define                    TIMER9_PERIOD  0xffc00618   /* Timer 9 Period Register */
#define                     TIMER9_WIDTH  0xffc0061c   /* Timer 9 Width Register */
#define                   TIMER10_CONFIG  0xffc00620   /* Timer 10 Configuration Register */
#define                  TIMER10_COUNTER  0xffc00624   /* Timer 10 Counter Register */
#define                   TIMER10_PERIOD  0xffc00628   /* Timer 10 Period Register */
#define                    TIMER10_WIDTH  0xffc0062c   /* Timer 10 Width Register */

/* Timer Group of 3 Registers */

#define                    TIMER_ENABLE1  0xffc00640   /* Timer Group of 3 Enable Register */
#define                   TIMER_DISABLE1  0xffc00644   /* Timer Group of 3 Disable Register */
#define                    TIMER_STATUS1  0xffc00648   /* Timer Group of 3 Status Register */

/* SPORT0 Registers */

#define                      SPORT0_TCR1  0xffc00800   /* SPORT0 Transmit Configuration 1 Register */
#define                      SPORT0_TCR2  0xffc00804   /* SPORT0 Transmit Configuration 2 Register */
#define                   SPORT0_TCLKDIV  0xffc00808   /* SPORT0 Transmit Serial Clock Divider Register */
#define                    SPORT0_TFSDIV  0xffc0080c   /* SPORT0 Transmit Frame Sync Divider Register */
#define                        SPORT0_TX  0xffc00810   /* SPORT0 Transmit Data Register */
#define                        SPORT0_RX  0xffc00818   /* SPORT0 Receive Data Register */
#define                      SPORT0_RCR1  0xffc00820   /* SPORT0 Receive Configuration 1 Register */
#define                      SPORT0_RCR2  0xffc00824   /* SPORT0 Receive Configuration 2 Register */
#define                   SPORT0_RCLKDIV  0xffc00828   /* SPORT0 Receive Serial Clock Divider Register */
#define                    SPORT0_RFSDIV  0xffc0082c   /* SPORT0 Receive Frame Sync Divider Register */
#define                      SPORT0_STAT  0xffc00830   /* SPORT0 Status Register */
#define                      SPORT0_CHNL  0xffc00834   /* SPORT0 Current Channel Register */
#define                     SPORT0_MCMC1  0xffc00838   /* SPORT0 Multi channel Configuration Register 1 */
#define                     SPORT0_MCMC2  0xffc0083c   /* SPORT0 Multi channel Configuration Register 2 */
#define                     SPORT0_MTCS0  0xffc00840   /* SPORT0 Multi channel Transmit Select Register 0 */
#define                     SPORT0_MTCS1  0xffc00844   /* SPORT0 Multi channel Transmit Select Register 1 */
#define                     SPORT0_MTCS2  0xffc00848   /* SPORT0 Multi channel Transmit Select Register 2 */
#define                     SPORT0_MTCS3  0xffc0084c   /* SPORT0 Multi channel Transmit Select Register 3 */
#define                     SPORT0_MRCS0  0xffc00850   /* SPORT0 Multi channel Receive Select Register 0 */
#define                     SPORT0_MRCS1  0xffc00854   /* SPORT0 Multi channel Receive Select Register 1 */
#define                     SPORT0_MRCS2  0xffc00858   /* SPORT0 Multi channel Receive Select Register 2 */
#define                     SPORT0_MRCS3  0xffc0085c   /* SPORT0 Multi channel Receive Select Register 3 */

/* EPPI0 Registers */

#define                     EPPI0_STATUS  0xffc01000   /* EPPI0 Status Register */
#define                     EPPI0_HCOUNT  0xffc01004   /* EPPI0 Horizontal Transfer Count Register */
#define                     EPPI0_HDELAY  0xffc01008   /* EPPI0 Horizontal Delay Count Register */
#define                     EPPI0_VCOUNT  0xffc0100c   /* EPPI0 Vertical Transfer Count Register */
#define                     EPPI0_VDELAY  0xffc01010   /* EPPI0 Vertical Delay Count Register */
#define                      EPPI0_FRAME  0xffc01014   /* EPPI0 Lines per Frame Register */
#define                       EPPI0_LINE  0xffc01018   /* EPPI0 Samples per Line Register */
#define                     EPPI0_CLKDIV  0xffc0101c   /* EPPI0 Clock Divide Register */
#define                    EPPI0_CONTROL  0xffc01020   /* EPPI0 Control Register */
#define                   EPPI0_FS1W_HBL  0xffc01024   /* EPPI0 FS1 Width Register / EPPI0 Horizontal Blanking Samples Per Line Register */
#define                  EPPI0_FS1P_AVPL  0xffc01028   /* EPPI0 FS1 Period Register / EPPI0 Active Video Samples Per Line Register */
#define                   EPPI0_FS2W_LVB  0xffc0102c   /* EPPI0 FS2 Width Register / EPPI0 Lines of Vertical Blanking Register */
#define                  EPPI0_FS2P_LAVF  0xffc01030   /* EPPI0 FS2 Period Register/ EPPI0 Lines of Active Video Per Field Register */
#define                       EPPI0_CLIP  0xffc01034   /* EPPI0 Clipping Register */

/* UART2 Registers */

#define                        UART2_DLL  0xffc02100   /* Divisor Latch Low Byte */
#define                        UART2_DLH  0xffc02104   /* Divisor Latch High Byte */
#define                       UART2_GCTL  0xffc02108   /* Global Control Register */
#define                        UART2_LCR  0xffc0210c   /* Line Control Register */
#define                        UART2_MCR  0xffc02110   /* Modem Control Register */
#define                        UART2_LSR  0xffc02114   /* Line Status Register */
#define                        UART2_MSR  0xffc02118   /* Modem Status Register */
#define                        UART2_SCR  0xffc0211c   /* Scratch Register */
#define                    UART2_IER_SET  0xffc02120   /* Interrupt Enable Register Set */
#define                  UART2_IER_CLEAR  0xffc02124   /* Interrupt Enable Register Clear */
#define                        UART2_RBR  0xffc0212c   /* Receive Buffer Register */

/* Two Wire Interface Registers (TWI1) */

#define                     TWI1_REGBASE  0xffc02200
#define                      TWI1_CLKDIV  0xffc02200   /* Clock Divider Register */
#define                     TWI1_CONTROL  0xffc02204   /* TWI Control Register */
#define                  TWI1_SLAVE_CTRL  0xffc02208   /* TWI Slave Mode Control Register */
#define                  TWI1_SLAVE_STAT  0xffc0220c   /* TWI Slave Mode Status Register */
#define                  TWI1_SLAVE_ADDR  0xffc02210   /* TWI Slave Mode Address Register */
#define                 TWI1_MASTER_CTRL  0xffc02214   /* TWI Master Mode Control Register */
#define                 TWI1_MASTER_STAT  0xffc02218   /* TWI Master Mode Status Register */
#define                 TWI1_MASTER_ADDR  0xffc0221c   /* TWI Master Mode Address Register */
#define                    TWI1_INT_STAT  0xffc02220   /* TWI Interrupt Status Register */
#define                    TWI1_INT_MASK  0xffc02224   /* TWI Interrupt Mask Register */
#define                   TWI1_FIFO_CTRL  0xffc02228   /* TWI FIFO Control Register */
#define                   TWI1_FIFO_STAT  0xffc0222c   /* TWI FIFO Status Register */
#define                   TWI1_XMT_DATA8  0xffc02280   /* TWI FIFO Transmit Data Single Byte Register */
#define                  TWI1_XMT_DATA16  0xffc02284   /* TWI FIFO Transmit Data Double Byte Register */
#define                   TWI1_RCV_DATA8  0xffc02288   /* TWI FIFO Receive Data Single Byte Register */
#define                  TWI1_RCV_DATA16  0xffc0228c   /* TWI FIFO Receive Data Double Byte Register */

/* SPI2  Registers */

#define                     SPI2_REGBASE  0xffc02400
#define                         SPI2_CTL  0xffc02400   /* SPI2 Control Register */
#define                         SPI2_FLG  0xffc02404   /* SPI2 Flag Register */
#define                        SPI2_STAT  0xffc02408   /* SPI2 Status Register */
#define                        SPI2_TDBR  0xffc0240c   /* SPI2 Transmit Data Buffer Register */
#define                        SPI2_RDBR  0xffc02410   /* SPI2 Receive Data Buffer Register */
#define                        SPI2_BAUD  0xffc02414   /* SPI2 Baud Rate Register */
#define                      SPI2_SHADOW  0xffc02418   /* SPI2 Receive Data Buffer Shadow Register */

/* MXVR Registers */

#define                      MXVR_CONFIG  0xffc02700   /* MXVR Configuration Register */
#define                     MXVR_STATE_0  0xffc02708   /* MXVR State Register 0 */
#define                     MXVR_STATE_1  0xffc0270c   /* MXVR State Register 1 */
#define                  MXVR_INT_STAT_0  0xffc02710   /* MXVR Interrupt Status Register 0 */
#define                  MXVR_INT_STAT_1  0xffc02714   /* MXVR Interrupt Status Register 1 */
#define                    MXVR_INT_EN_0  0xffc02718   /* MXVR Interrupt Enable Register 0 */
#define                    MXVR_INT_EN_1  0xffc0271c   /* MXVR Interrupt Enable Register 1 */
#define                    MXVR_POSITION  0xffc02720   /* MXVR Node Position Register */
#define                MXVR_MAX_POSITION  0xffc02724   /* MXVR Maximum Node Position Register */
#define                       MXVR_DELAY  0xffc02728   /* MXVR Node Frame Delay Register */
#define                   MXVR_MAX_DELAY  0xffc0272c   /* MXVR Maximum Node Frame Delay Register */
#define                       MXVR_LADDR  0xffc02730   /* MXVR Logical Address Register */
#define                       MXVR_GADDR  0xffc02734   /* MXVR Group Address Register */
#define                       MXVR_AADDR  0xffc02738   /* MXVR Alternate Address Register */

/* MXVR Allocation Table Registers */

#define                     MXVR_ALLOC_0  0xffc0273c   /* MXVR Allocation Table Register 0 */
#define                     MXVR_ALLOC_1  0xffc02740   /* MXVR Allocation Table Register 1 */
#define                     MXVR_ALLOC_2  0xffc02744   /* MXVR Allocation Table Register 2 */
#define                     MXVR_ALLOC_3  0xffc02748   /* MXVR Allocation Table Register 3 */
#define                     MXVR_ALLOC_4  0xffc0274c   /* MXVR Allocation Table Register 4 */
#define                     MXVR_ALLOC_5  0xffc02750   /* MXVR Allocation Table Register 5 */
#define                     MXVR_ALLOC_6  0xffc02754   /* MXVR Allocation Table Register 6 */
#define                     MXVR_ALLOC_7  0xffc02758   /* MXVR Allocation Table Register 7 */
#define                     MXVR_ALLOC_8  0xffc0275c   /* MXVR Allocation Table Register 8 */
#define                     MXVR_ALLOC_9  0xffc02760   /* MXVR Allocation Table Register 9 */
#define                    MXVR_ALLOC_10  0xffc02764   /* MXVR Allocation Table Register 10 */
#define                    MXVR_ALLOC_11  0xffc02768   /* MXVR Allocation Table Register 11 */
#define                    MXVR_ALLOC_12  0xffc0276c   /* MXVR Allocation Table Register 12 */
#define                    MXVR_ALLOC_13  0xffc02770   /* MXVR Allocation Table Register 13 */
#define                    MXVR_ALLOC_14  0xffc02774   /* MXVR Allocation Table Register 14 */

/* MXVR Channel Assign Registers */

#define                MXVR_SYNC_LCHAN_0  0xffc02778   /* MXVR Sync Data Logical Channel Assign Register 0 */
#define                MXVR_SYNC_LCHAN_1  0xffc0277c   /* MXVR Sync Data Logical Channel Assign Register 1 */
#define                MXVR_SYNC_LCHAN_2  0xffc02780   /* MXVR Sync Data Logical Channel Assign Register 2 */
#define                MXVR_SYNC_LCHAN_3  0xffc02784   /* MXVR Sync Data Logical Channel Assign Register 3 */
#define                MXVR_SYNC_LCHAN_4  0xffc02788   /* MXVR Sync Data Logical Channel Assign Register 4 */
#define                MXVR_SYNC_LCHAN_5  0xffc0278c   /* MXVR Sync Data Logical Channel Assign Register 5 */
#define                MXVR_SYNC_LCHAN_6  0xffc02790   /* MXVR Sync Data Logical Channel Assign Register 6 */
#define                MXVR_SYNC_LCHAN_7  0xffc02794   /* MXVR Sync Data Logical Channel Assign Register 7 */

/* MXVR DMA0 Registers */

#define                 MXVR_DMA0_CONFIG  0xffc02798   /* MXVR Sync Data DMA0 Config Register */
#define             MXVR_DMA0_START_ADDR  0xffc0279c   /* MXVR Sync Data DMA0 Start Address */
#define                  MXVR_DMA0_COUNT  0xffc027a0   /* MXVR Sync Data DMA0 Loop Count Register */
#define              MXVR_DMA0_CURR_ADDR  0xffc027a4   /* MXVR Sync Data DMA0 Current Address */
#define             MXVR_DMA0_CURR_COUNT  0xffc027a8   /* MXVR Sync Data DMA0 Current Loop Count */

/* MXVR DMA1 Registers */

#define                 MXVR_DMA1_CONFIG  0xffc027ac   /* MXVR Sync Data DMA1 Config Register */
#define             MXVR_DMA1_START_ADDR  0xffc027b0   /* MXVR Sync Data DMA1 Start Address */
#define                  MXVR_DMA1_COUNT  0xffc027b4   /* MXVR Sync Data DMA1 Loop Count Register */
#define              MXVR_DMA1_CURR_ADDR  0xffc027b8   /* MXVR Sync Data DMA1 Current Address */
#define             MXVR_DMA1_CURR_COUNT  0xffc027bc   /* MXVR Sync Data DMA1 Current Loop Count */

/* MXVR DMA2 Registers */

#define                 MXVR_DMA2_CONFIG  0xffc027c0   /* MXVR Sync Data DMA2 Config Register */
#define             MXVR_DMA2_START_ADDR  0xffc027c4   /* MXVR Sync Data DMA2 Start Address */
#define                  MXVR_DMA2_COUNT  0xffc027c8   /* MXVR Sync Data DMA2 Loop Count Register */
#define              MXVR_DMA2_CURR_ADDR  0xffc027cc   /* MXVR Sync Data DMA2 Current Address */
#define             MXVR_DMA2_CURR_COUNT  0xffc027d0   /* MXVR Sync Data DMA2 Current Loop Count */

/* MXVR DMA3 Registers */

#define                 MXVR_DMA3_CONFIG  0xffc027d4   /* MXVR Sync Data DMA3 Config Register */
#define             MXVR_DMA3_START_ADDR  0xffc027d8   /* MXVR Sync Data DMA3 Start Address */
#define                  MXVR_DMA3_COUNT  0xffc027dc   /* MXVR Sync Data DMA3 Loop Count Register */
#define              MXVR_DMA3_CURR_ADDR  0xffc027e0   /* MXVR Sync Data DMA3 Current Address */
#define             MXVR_DMA3_CURR_COUNT  0xffc027e4   /* MXVR Sync Data DMA3 Current Loop Count */

/* MXVR DMA4 Registers */

#define                 MXVR_DMA4_CONFIG  0xffc027e8   /* MXVR Sync Data DMA4 Config Register */
#define             MXVR_DMA4_START_ADDR  0xffc027ec   /* MXVR Sync Data DMA4 Start Address */
#define                  MXVR_DMA4_COUNT  0xffc027f0   /* MXVR Sync Data DMA4 Loop Count Register */
#define              MXVR_DMA4_CURR_ADDR  0xffc027f4   /* MXVR Sync Data DMA4 Current Address */
#define             MXVR_DMA4_CURR_COUNT  0xffc027f8   /* MXVR Sync Data DMA4 Current Loop Count */

/* MXVR DMA5 Registers */

#define                 MXVR_DMA5_CONFIG  0xffc027fc   /* MXVR Sync Data DMA5 Config Register */
#define             MXVR_DMA5_START_ADDR  0xffc02800   /* MXVR Sync Data DMA5 Start Address */
#define                  MXVR_DMA5_COUNT  0xffc02804   /* MXVR Sync Data DMA5 Loop Count Register */
#define              MXVR_DMA5_CURR_ADDR  0xffc02808   /* MXVR Sync Data DMA5 Current Address */
#define             MXVR_DMA5_CURR_COUNT  0xffc0280c   /* MXVR Sync Data DMA5 Current Loop Count */

/* MXVR DMA6 Registers */

#define                 MXVR_DMA6_CONFIG  0xffc02810   /* MXVR Sync Data DMA6 Config Register */
#define             MXVR_DMA6_START_ADDR  0xffc02814   /* MXVR Sync Data DMA6 Start Address */
#define                  MXVR_DMA6_COUNT  0xffc02818   /* MXVR Sync Data DMA6 Loop Count Register */
#define              MXVR_DMA6_CURR_ADDR  0xffc0281c   /* MXVR Sync Data DMA6 Current Address */
#define             MXVR_DMA6_CURR_COUNT  0xffc02820   /* MXVR Sync Data DMA6 Current Loop Count */

/* MXVR DMA7 Registers */

#define                 MXVR_DMA7_CONFIG  0xffc02824   /* MXVR Sync Data DMA7 Config Register */
#define             MXVR_DMA7_START_ADDR  0xffc02828   /* MXVR Sync Data DMA7 Start Address */
#define                  MXVR_DMA7_COUNT  0xffc0282c   /* MXVR Sync Data DMA7 Loop Count Register */
#define              MXVR_DMA7_CURR_ADDR  0xffc02830   /* MXVR Sync Data DMA7 Current Address */
#define             MXVR_DMA7_CURR_COUNT  0xffc02834   /* MXVR Sync Data DMA7 Current Loop Count */

/* MXVR Asynch Packet Registers */

#define                      MXVR_AP_CTL  0xffc02838   /* MXVR Async Packet Control Register */
#define             MXVR_APRB_START_ADDR  0xffc0283c   /* MXVR Async Packet RX Buffer Start Addr Register */
#define              MXVR_APRB_CURR_ADDR  0xffc02840   /* MXVR Async Packet RX Buffer Current Addr Register */
#define             MXVR_APTB_START_ADDR  0xffc02844   /* MXVR Async Packet TX Buffer Start Addr Register */
#define              MXVR_APTB_CURR_ADDR  0xffc02848   /* MXVR Async Packet TX Buffer Current Addr Register */

/* MXVR Control Message Registers */

#define                      MXVR_CM_CTL  0xffc0284c   /* MXVR Control Message Control Register */
#define             MXVR_CMRB_START_ADDR  0xffc02850   /* MXVR Control Message RX Buffer Start Addr Register */
#define              MXVR_CMRB_CURR_ADDR  0xffc02854   /* MXVR Control Message RX Buffer Current Address */
#define             MXVR_CMTB_START_ADDR  0xffc02858   /* MXVR Control Message TX Buffer Start Addr Register */
#define              MXVR_CMTB_CURR_ADDR  0xffc0285c   /* MXVR Control Message TX Buffer Current Address */

/* MXVR Remote Read Registers */

#define             MXVR_RRDB_START_ADDR  0xffc02860   /* MXVR Remote Read Buffer Start Addr Register */
#define              MXVR_RRDB_CURR_ADDR  0xffc02864   /* MXVR Remote Read Buffer Current Addr Register */

/* MXVR Pattern Data Registers */

#define                  MXVR_PAT_DATA_0  0xffc02868   /* MXVR Pattern Data Register 0 */
#define                    MXVR_PAT_EN_0  0xffc0286c   /* MXVR Pattern Enable Register 0 */
#define                  MXVR_PAT_DATA_1  0xffc02870   /* MXVR Pattern Data Register 1 */
#define                    MXVR_PAT_EN_1  0xffc02874   /* MXVR Pattern Enable Register 1 */

/* MXVR Frame Counter Registers */

#define                 MXVR_FRAME_CNT_0  0xffc02878   /* MXVR Frame Counter 0 */
#define                 MXVR_FRAME_CNT_1  0xffc0287c   /* MXVR Frame Counter 1 */

/* MXVR Routing Table Registers */

#define                   MXVR_ROUTING_0  0xffc02880   /* MXVR Routing Table Register 0 */
#define                   MXVR_ROUTING_1  0xffc02884   /* MXVR Routing Table Register 1 */
#define                   MXVR_ROUTING_2  0xffc02888   /* MXVR Routing Table Register 2 */
#define                   MXVR_ROUTING_3  0xffc0288c   /* MXVR Routing Table Register 3 */
#define                   MXVR_ROUTING_4  0xffc02890   /* MXVR Routing Table Register 4 */
#define                   MXVR_ROUTING_5  0xffc02894   /* MXVR Routing Table Register 5 */
#define                   MXVR_ROUTING_6  0xffc02898   /* MXVR Routing Table Register 6 */
#define                   MXVR_ROUTING_7  0xffc0289c   /* MXVR Routing Table Register 7 */
#define                   MXVR_ROUTING_8  0xffc028a0   /* MXVR Routing Table Register 8 */
#define                   MXVR_ROUTING_9  0xffc028a4   /* MXVR Routing Table Register 9 */
#define                  MXVR_ROUTING_10  0xffc028a8   /* MXVR Routing Table Register 10 */
#define                  MXVR_ROUTING_11  0xffc028ac   /* MXVR Routing Table Register 11 */
#define                  MXVR_ROUTING_12  0xffc028b0   /* MXVR Routing Table Register 12 */
#define                  MXVR_ROUTING_13  0xffc028b4   /* MXVR Routing Table Register 13 */
#define                  MXVR_ROUTING_14  0xffc028b8   /* MXVR Routing Table Register 14 */

/* MXVR Counter-Clock-Control Registers */

#define                   MXVR_BLOCK_CNT  0xffc028c0   /* MXVR Block Counter */
#define                     MXVR_CLK_CTL  0xffc028d0   /* MXVR Clock Control Register */
#define                  MXVR_CDRPLL_CTL  0xffc028d4   /* MXVR Clock/Data Recovery PLL Control Register */
#define                   MXVR_FMPLL_CTL  0xffc028d8   /* MXVR Frequency Multiply PLL Control Register */
#define                     MXVR_PIN_CTL  0xffc028dc   /* MXVR Pin Control Register */
#define                    MXVR_SCLK_CNT  0xffc028e0   /* MXVR System Clock Counter Register */

/* CAN Controller 1 Config 1 Registers */

#define                         CAN1_MC1  0xffc03200   /* CAN Controller 1 Mailbox Configuration Register 1 */
#define                         CAN1_MD1  0xffc03204   /* CAN Controller 1 Mailbox Direction Register 1 */
#define                        CAN1_TRS1  0xffc03208   /* CAN Controller 1 Transmit Request Set Register 1 */
#define                        CAN1_TRR1  0xffc0320c   /* CAN Controller 1 Transmit Request Reset Register 1 */
#define                         CAN1_TA1  0xffc03210   /* CAN Controller 1 Transmit Acknowledge Register 1 */
#define                         CAN1_AA1  0xffc03214   /* CAN Controller 1 Abort Acknowledge Register 1 */
#define                        CAN1_RMP1  0xffc03218   /* CAN Controller 1 Receive Message Pending Register 1 */
#define                        CAN1_RML1  0xffc0321c   /* CAN Controller 1 Receive Message Lost Register 1 */
#define                      CAN1_MBTIF1  0xffc03220   /* CAN Controller 1 Mailbox Transmit Interrupt Flag Register 1 */
#define                      CAN1_MBRIF1  0xffc03224   /* CAN Controller 1 Mailbox Receive Interrupt Flag Register 1 */
#define                       CAN1_MBIM1  0xffc03228   /* CAN Controller 1 Mailbox Interrupt Mask Register 1 */
#define                        CAN1_RFH1  0xffc0322c   /* CAN Controller 1 Remote Frame Handling Enable Register 1 */
#define                       CAN1_OPSS1  0xffc03230   /* CAN Controller 1 Overwrite Protection Single Shot Transmit Register 1 */

/* CAN Controller 1 Config 2 Registers */

#define                         CAN1_MC2  0xffc03240   /* CAN Controller 1 Mailbox Configuration Register 2 */
#define                         CAN1_MD2  0xffc03244   /* CAN Controller 1 Mailbox Direction Register 2 */
#define                        CAN1_TRS2  0xffc03248   /* CAN Controller 1 Transmit Request Set Register 2 */
#define                        CAN1_TRR2  0xffc0324c   /* CAN Controller 1 Transmit Request Reset Register 2 */
#define                         CAN1_TA2  0xffc03250   /* CAN Controller 1 Transmit Acknowledge Register 2 */
#define                         CAN1_AA2  0xffc03254   /* CAN Controller 1 Abort Acknowledge Register 2 */
#define                        CAN1_RMP2  0xffc03258   /* CAN Controller 1 Receive Message Pending Register 2 */
#define                        CAN1_RML2  0xffc0325c   /* CAN Controller 1 Receive Message Lost Register 2 */
#define                      CAN1_MBTIF2  0xffc03260   /* CAN Controller 1 Mailbox Transmit Interrupt Flag Register 2 */
#define                      CAN1_MBRIF2  0xffc03264   /* CAN Controller 1 Mailbox Receive Interrupt Flag Register 2 */
#define                       CAN1_MBIM2  0xffc03268   /* CAN Controller 1 Mailbox Interrupt Mask Register 2 */
#define                        CAN1_RFH2  0xffc0326c   /* CAN Controller 1 Remote Frame Handling Enable Register 2 */
#define                       CAN1_OPSS2  0xffc03270   /* CAN Controller 1 Overwrite Protection Single Shot Transmit Register 2 */

/* CAN Controller 1 Clock/Interrupt/Counter Registers */

#define                       CAN1_CLOCK  0xffc03280   /* CAN Controller 1 Clock Register */
#define                      CAN1_TIMING  0xffc03284   /* CAN Controller 1 Timing Register */
#define                       CAN1_DEBUG  0xffc03288   /* CAN Controller 1 Debug Register */
#define                      CAN1_STATUS  0xffc0328c   /* CAN Controller 1 Global Status Register */
#define                         CAN1_CEC  0xffc03290   /* CAN Controller 1 Error Counter Register */
#define                         CAN1_GIS  0xffc03294   /* CAN Controller 1 Global Interrupt Status Register */
#define                         CAN1_GIM  0xffc03298   /* CAN Controller 1 Global Interrupt Mask Register */
#define                         CAN1_GIF  0xffc0329c   /* CAN Controller 1 Global Interrupt Flag Register */
#define                     CAN1_CONTROL  0xffc032a0   /* CAN Controller 1 Master Control Register */
#define                        CAN1_INTR  0xffc032a4   /* CAN Controller 1 Interrupt Pending Register */
#define                        CAN1_MBTD  0xffc032ac   /* CAN Controller 1 Mailbox Temporary Disable Register */
#define                         CAN1_EWR  0xffc032b0   /* CAN Controller 1 Programmable Warning Level Register */
#define                         CAN1_ESR  0xffc032b4   /* CAN Controller 1 Error Status Register */
#define                       CAN1_UCCNT  0xffc032c4   /* CAN Controller 1 Universal Counter Register */
#define                        CAN1_UCRC  0xffc032c8   /* CAN Controller 1 Universal Counter Force Reload Register */
#define                       CAN1_UCCNF  0xffc032cc   /* CAN Controller 1 Universal Counter Configuration Register */

/* CAN Controller 1 Mailbox Acceptance Registers */

#define                       CAN1_AM00L  0xffc03300   /* CAN Controller 1 Mailbox 0 Acceptance Mask High Register */
#define                       CAN1_AM00H  0xffc03304   /* CAN Controller 1 Mailbox 0 Acceptance Mask Low Register */
#define                       CAN1_AM01L  0xffc03308   /* CAN Controller 1 Mailbox 1 Acceptance Mask High Register */
#define                       CAN1_AM01H  0xffc0330c   /* CAN Controller 1 Mailbox 1 Acceptance Mask Low Register */
#define                       CAN1_AM02L  0xffc03310   /* CAN Controller 1 Mailbox 2 Acceptance Mask High Register */
#define                       CAN1_AM02H  0xffc03314   /* CAN Controller 1 Mailbox 2 Acceptance Mask Low Register */
#define                       CAN1_AM03L  0xffc03318   /* CAN Controller 1 Mailbox 3 Acceptance Mask High Register */
#define                       CAN1_AM03H  0xffc0331c   /* CAN Controller 1 Mailbox 3 Acceptance Mask Low Register */
#define                       CAN1_AM04L  0xffc03320   /* CAN Controller 1 Mailbox 4 Acceptance Mask High Register */
#define                       CAN1_AM04H  0xffc03324   /* CAN Controller 1 Mailbox 4 Acceptance Mask Low Register */
#define                       CAN1_AM05L  0xffc03328   /* CAN Controller 1 Mailbox 5 Acceptance Mask High Register */
#define                       CAN1_AM05H  0xffc0332c   /* CAN Controller 1 Mailbox 5 Acceptance Mask Low Register */
#define                       CAN1_AM06L  0xffc03330   /* CAN Controller 1 Mailbox 6 Acceptance Mask High Register */
#define                       CAN1_AM06H  0xffc03334   /* CAN Controller 1 Mailbox 6 Acceptance Mask Low Register */
#define                       CAN1_AM07L  0xffc03338   /* CAN Controller 1 Mailbox 7 Acceptance Mask High Register */
#define                       CAN1_AM07H  0xffc0333c   /* CAN Controller 1 Mailbox 7 Acceptance Mask Low Register */
#define                       CAN1_AM08L  0xffc03340   /* CAN Controller 1 Mailbox 8 Acceptance Mask High Register */
#define                       CAN1_AM08H  0xffc03344   /* CAN Controller 1 Mailbox 8 Acceptance Mask Low Register */
#define                       CAN1_AM09L  0xffc03348   /* CAN Controller 1 Mailbox 9 Acceptance Mask High Register */
#define                       CAN1_AM09H  0xffc0334c   /* CAN Controller 1 Mailbox 9 Acceptance Mask Low Register */
#define                       CAN1_AM10L  0xffc03350   /* CAN Controller 1 Mailbox 10 Acceptance Mask High Register */
#define                       CAN1_AM10H  0xffc03354   /* CAN Controller 1 Mailbox 10 Acceptance Mask Low Register */
#define                       CAN1_AM11L  0xffc03358   /* CAN Controller 1 Mailbox 11 Acceptance Mask High Register */
#define                       CAN1_AM11H  0xffc0335c   /* CAN Controller 1 Mailbox 11 Acceptance Mask Low Register */
#define                       CAN1_AM12L  0xffc03360   /* CAN Controller 1 Mailbox 12 Acceptance Mask High Register */
#define                       CAN1_AM12H  0xffc03364   /* CAN Controller 1 Mailbox 12 Acceptance Mask Low Register */
#define                       CAN1_AM13L  0xffc03368   /* CAN Controller 1 Mailbox 13 Acceptance Mask High Register */
#define                       CAN1_AM13H  0xffc0336c   /* CAN Controller 1 Mailbox 13 Acceptance Mask Low Register */
#define                       CAN1_AM14L  0xffc03370   /* CAN Controller 1 Mailbox 14 Acceptance Mask High Register */
#define                       CAN1_AM14H  0xffc03374   /* CAN Controller 1 Mailbox 14 Acceptance Mask Low Register */
#define                       CAN1_AM15L  0xffc03378   /* CAN Controller 1 Mailbox 15 Acceptance Mask High Register */
#define                       CAN1_AM15H  0xffc0337c   /* CAN Controller 1 Mailbox 15 Acceptance Mask Low Register */

/* CAN Controller 1 Mailbox Acceptance Registers */

#define                       CAN1_AM16L  0xffc03380   /* CAN Controller 1 Mailbox 16 Acceptance Mask High Register */
#define                       CAN1_AM16H  0xffc03384   /* CAN Controller 1 Mailbox 16 Acceptance Mask Low Register */
#define                       CAN1_AM17L  0xffc03388   /* CAN Controller 1 Mailbox 17 Acceptance Mask High Register */
#define                       CAN1_AM17H  0xffc0338c   /* CAN Controller 1 Mailbox 17 Acceptance Mask Low Register */
#define                       CAN1_AM18L  0xffc03390   /* CAN Controller 1 Mailbox 18 Acceptance Mask High Register */
#define                       CAN1_AM18H  0xffc03394   /* CAN Controller 1 Mailbox 18 Acceptance Mask Low Register */
#define                       CAN1_AM19L  0xffc03398   /* CAN Controller 1 Mailbox 19 Acceptance Mask High Register */
#define                       CAN1_AM19H  0xffc0339c   /* CAN Controller 1 Mailbox 19 Acceptance Mask Low Register */
#define                       CAN1_AM20L  0xffc033a0   /* CAN Controller 1 Mailbox 20 Acceptance Mask High Register */
#define                       CAN1_AM20H  0xffc033a4   /* CAN Controller 1 Mailbox 20 Acceptance Mask Low Register */
#define                       CAN1_AM21L  0xffc033a8   /* CAN Controller 1 Mailbox 21 Acceptance Mask High Register */
#define                       CAN1_AM21H  0xffc033ac   /* CAN Controller 1 Mailbox 21 Acceptance Mask Low Register */
#define                       CAN1_AM22L  0xffc033b0   /* CAN Controller 1 Mailbox 22 Acceptance Mask High Register */
#define                       CAN1_AM22H  0xffc033b4   /* CAN Controller 1 Mailbox 22 Acceptance Mask Low Register */
#define                       CAN1_AM23L  0xffc033b8   /* CAN Controller 1 Mailbox 23 Acceptance Mask High Register */
#define                       CAN1_AM23H  0xffc033bc   /* CAN Controller 1 Mailbox 23 Acceptance Mask Low Register */
#define                       CAN1_AM24L  0xffc033c0   /* CAN Controller 1 Mailbox 24 Acceptance Mask High Register */
#define                       CAN1_AM24H  0xffc033c4   /* CAN Controller 1 Mailbox 24 Acceptance Mask Low Register */
#define                       CAN1_AM25L  0xffc033c8   /* CAN Controller 1 Mailbox 25 Acceptance Mask High Register */
#define                       CAN1_AM25H  0xffc033cc   /* CAN Controller 1 Mailbox 25 Acceptance Mask Low Register */
#define                       CAN1_AM26L  0xffc033d0   /* CAN Controller 1 Mailbox 26 Acceptance Mask High Register */
#define                       CAN1_AM26H  0xffc033d4   /* CAN Controller 1 Mailbox 26 Acceptance Mask Low Register */
#define                       CAN1_AM27L  0xffc033d8   /* CAN Controller 1 Mailbox 27 Acceptance Mask High Register */
#define                       CAN1_AM27H  0xffc033dc   /* CAN Controller 1 Mailbox 27 Acceptance Mask Low Register */
#define                       CAN1_AM28L  0xffc033e0   /* CAN Controller 1 Mailbox 28 Acceptance Mask High Register */
#define                       CAN1_AM28H  0xffc033e4   /* CAN Controller 1 Mailbox 28 Acceptance Mask Low Register */
#define                       CAN1_AM29L  0xffc033e8   /* CAN Controller 1 Mailbox 29 Acceptance Mask High Register */
#define                       CAN1_AM29H  0xffc033ec   /* CAN Controller 1 Mailbox 29 Acceptance Mask Low Register */
#define                       CAN1_AM30L  0xffc033f0   /* CAN Controller 1 Mailbox 30 Acceptance Mask High Register */
#define                       CAN1_AM30H  0xffc033f4   /* CAN Controller 1 Mailbox 30 Acceptance Mask Low Register */
#define                       CAN1_AM31L  0xffc033f8   /* CAN Controller 1 Mailbox 31 Acceptance Mask High Register */
#define                       CAN1_AM31H  0xffc033fc   /* CAN Controller 1 Mailbox 31 Acceptance Mask Low Register */

/* CAN Controller 1 Mailbox Data Registers */

#define                  CAN1_MB00_DATA0  0xffc03400   /* CAN Controller 1 Mailbox 0 Data 0 Register */
#define                  CAN1_MB00_DATA1  0xffc03404   /* CAN Controller 1 Mailbox 0 Data 1 Register */
#define                  CAN1_MB00_DATA2  0xffc03408   /* CAN Controller 1 Mailbox 0 Data 2 Register */
#define                  CAN1_MB00_DATA3  0xffc0340c   /* CAN Controller 1 Mailbox 0 Data 3 Register */
#define                 CAN1_MB00_LENGTH  0xffc03410   /* CAN Controller 1 Mailbox 0 Length Register */
#define              CAN1_MB00_TIMESTAMP  0xffc03414   /* CAN Controller 1 Mailbox 0 Timestamp Register */
#define                    CAN1_MB00_ID0  0xffc03418   /* CAN Controller 1 Mailbox 0 ID0 Register */
#define                    CAN1_MB00_ID1  0xffc0341c   /* CAN Controller 1 Mailbox 0 ID1 Register */
#define                  CAN1_MB01_DATA0  0xffc03420   /* CAN Controller 1 Mailbox 1 Data 0 Register */
#define                  CAN1_MB01_DATA1  0xffc03424   /* CAN Controller 1 Mailbox 1 Data 1 Register */
#define                  CAN1_MB01_DATA2  0xffc03428   /* CAN Controller 1 Mailbox 1 Data 2 Register */
#define                  CAN1_MB01_DATA3  0xffc0342c   /* CAN Controller 1 Mailbox 1 Data 3 Register */
#define                 CAN1_MB01_LENGTH  0xffc03430   /* CAN Controller 1 Mailbox 1 Length Register */
#define              CAN1_MB01_TIMESTAMP  0xffc03434   /* CAN Controller 1 Mailbox 1 Timestamp Register */
#define                    CAN1_MB01_ID0  0xffc03438   /* CAN Controller 1 Mailbox 1 ID0 Register */
#define                    CAN1_MB01_ID1  0xffc0343c   /* CAN Controller 1 Mailbox 1 ID1 Register */
#define                  CAN1_MB02_DATA0  0xffc03440   /* CAN Controller 1 Mailbox 2 Data 0 Register */
#define                  CAN1_MB02_DATA1  0xffc03444   /* CAN Controller 1 Mailbox 2 Data 1 Register */
#define                  CAN1_MB02_DATA2  0xffc03448   /* CAN Controller 1 Mailbox 2 Data 2 Register */
#define                  CAN1_MB02_DATA3  0xffc0344c   /* CAN Controller 1 Mailbox 2 Data 3 Register */
#define                 CAN1_MB02_LENGTH  0xffc03450   /* CAN Controller 1 Mailbox 2 Length Register */
#define              CAN1_MB02_TIMESTAMP  0xffc03454   /* CAN Controller 1 Mailbox 2 Timestamp Register */
#define                    CAN1_MB02_ID0  0xffc03458   /* CAN Controller 1 Mailbox 2 ID0 Register */
#define                    CAN1_MB02_ID1  0xffc0345c   /* CAN Controller 1 Mailbox 2 ID1 Register */
#define                  CAN1_MB03_DATA0  0xffc03460   /* CAN Controller 1 Mailbox 3 Data 0 Register */
#define                  CAN1_MB03_DATA1  0xffc03464   /* CAN Controller 1 Mailbox 3 Data 1 Register */
#define                  CAN1_MB03_DATA2  0xffc03468   /* CAN Controller 1 Mailbox 3 Data 2 Register */
#define                  CAN1_MB03_DATA3  0xffc0346c   /* CAN Controller 1 Mailbox 3 Data 3 Register */
#define                 CAN1_MB03_LENGTH  0xffc03470   /* CAN Controller 1 Mailbox 3 Length Register */
#define              CAN1_MB03_TIMESTAMP  0xffc03474   /* CAN Controller 1 Mailbox 3 Timestamp Register */
#define                    CAN1_MB03_ID0  0xffc03478   /* CAN Controller 1 Mailbox 3 ID0 Register */
#define                    CAN1_MB03_ID1  0xffc0347c   /* CAN Controller 1 Mailbox 3 ID1 Register */
#define                  CAN1_MB04_DATA0  0xffc03480   /* CAN Controller 1 Mailbox 4 Data 0 Register */
#define                  CAN1_MB04_DATA1  0xffc03484   /* CAN Controller 1 Mailbox 4 Data 1 Register */
#define                  CAN1_MB04_DATA2  0xffc03488   /* CAN Controller 1 Mailbox 4 Data 2 Register */
#define                  CAN1_MB04_DATA3  0xffc0348c   /* CAN Controller 1 Mailbox 4 Data 3 Register */
#define                 CAN1_MB04_LENGTH  0xffc03490   /* CAN Controller 1 Mailbox 4 Length Register */
#define              CAN1_MB04_TIMESTAMP  0xffc03494   /* CAN Controller 1 Mailbox 4 Timestamp Register */
#define                    CAN1_MB04_ID0  0xffc03498   /* CAN Controller 1 Mailbox 4 ID0 Register */
#define                    CAN1_MB04_ID1  0xffc0349c   /* CAN Controller 1 Mailbox 4 ID1 Register */
#define                  CAN1_MB05_DATA0  0xffc034a0   /* CAN Controller 1 Mailbox 5 Data 0 Register */
#define                  CAN1_MB05_DATA1  0xffc034a4   /* CAN Controller 1 Mailbox 5 Data 1 Register */
#define                  CAN1_MB05_DATA2  0xffc034a8   /* CAN Controller 1 Mailbox 5 Data 2 Register */
#define                  CAN1_MB05_DATA3  0xffc034ac   /* CAN Controller 1 Mailbox 5 Data 3 Register */
#define                 CAN1_MB05_LENGTH  0xffc034b0   /* CAN Controller 1 Mailbox 5 Length Register */
#define              CAN1_MB05_TIMESTAMP  0xffc034b4   /* CAN Controller 1 Mailbox 5 Timestamp Register */
#define                    CAN1_MB05_ID0  0xffc034b8   /* CAN Controller 1 Mailbox 5 ID0 Register */
#define                    CAN1_MB05_ID1  0xffc034bc   /* CAN Controller 1 Mailbox 5 ID1 Register */
#define                  CAN1_MB06_DATA0  0xffc034c0   /* CAN Controller 1 Mailbox 6 Data 0 Register */
#define                  CAN1_MB06_DATA1  0xffc034c4   /* CAN Controller 1 Mailbox 6 Data 1 Register */
#define                  CAN1_MB06_DATA2  0xffc034c8   /* CAN Controller 1 Mailbox 6 Data 2 Register */
#define                  CAN1_MB06_DATA3  0xffc034cc   /* CAN Controller 1 Mailbox 6 Data 3 Register */
#define                 CAN1_MB06_LENGTH  0xffc034d0   /* CAN Controller 1 Mailbox 6 Length Register */
#define              CAN1_MB06_TIMESTAMP  0xffc034d4   /* CAN Controller 1 Mailbox 6 Timestamp Register */
#define                    CAN1_MB06_ID0  0xffc034d8   /* CAN Controller 1 Mailbox 6 ID0 Register */
#define                    CAN1_MB06_ID1  0xffc034dc   /* CAN Controller 1 Mailbox 6 ID1 Register */
#define                  CAN1_MB07_DATA0  0xffc034e0   /* CAN Controller 1 Mailbox 7 Data 0 Register */
#define                  CAN1_MB07_DATA1  0xffc034e4   /* CAN Controller 1 Mailbox 7 Data 1 Register */
#define                  CAN1_MB07_DATA2  0xffc034e8   /* CAN Controller 1 Mailbox 7 Data 2 Register */
#define                  CAN1_MB07_DATA3  0xffc034ec   /* CAN Controller 1 Mailbox 7 Data 3 Register */
#define                 CAN1_MB07_LENGTH  0xffc034f0   /* CAN Controller 1 Mailbox 7 Length Register */
#define              CAN1_MB07_TIMESTAMP  0xffc034f4   /* CAN Controller 1 Mailbox 7 Timestamp Register */
#define                    CAN1_MB07_ID0  0xffc034f8   /* CAN Controller 1 Mailbox 7 ID0 Register */
#define                    CAN1_MB07_ID1  0xffc034fc   /* CAN Controller 1 Mailbox 7 ID1 Register */
#define                  CAN1_MB08_DATA0  0xffc03500   /* CAN Controller 1 Mailbox 8 Data 0 Register */
#define                  CAN1_MB08_DATA1  0xffc03504   /* CAN Controller 1 Mailbox 8 Data 1 Register */
#define                  CAN1_MB08_DATA2  0xffc03508   /* CAN Controller 1 Mailbox 8 Data 2 Register */
#define                  CAN1_MB08_DATA3  0xffc0350c   /* CAN Controller 1 Mailbox 8 Data 3 Register */
#define                 CAN1_MB08_LENGTH  0xffc03510   /* CAN Controller 1 Mailbox 8 Length Register */
#define              CAN1_MB08_TIMESTAMP  0xffc03514   /* CAN Controller 1 Mailbox 8 Timestamp Register */
#define                    CAN1_MB08_ID0  0xffc03518   /* CAN Controller 1 Mailbox 8 ID0 Register */
#define                    CAN1_MB08_ID1  0xffc0351c   /* CAN Controller 1 Mailbox 8 ID1 Register */
#define                  CAN1_MB09_DATA0  0xffc03520   /* CAN Controller 1 Mailbox 9 Data 0 Register */
#define                  CAN1_MB09_DATA1  0xffc03524   /* CAN Controller 1 Mailbox 9 Data 1 Register */
#define                  CAN1_MB09_DATA2  0xffc03528   /* CAN Controller 1 Mailbox 9 Data 2 Register */
#define                  CAN1_MB09_DATA3  0xffc0352c   /* CAN Controller 1 Mailbox 9 Data 3 Register */
#define                 CAN1_MB09_LENGTH  0xffc03530   /* CAN Controller 1 Mailbox 9 Length Register */
#define              CAN1_MB09_TIMESTAMP  0xffc03534   /* CAN Controller 1 Mailbox 9 Timestamp Register */
#define                    CAN1_MB09_ID0  0xffc03538   /* CAN Controller 1 Mailbox 9 ID0 Register */
#define                    CAN1_MB09_ID1  0xffc0353c   /* CAN Controller 1 Mailbox 9 ID1 Register */
#define                  CAN1_MB10_DATA0  0xffc03540   /* CAN Controller 1 Mailbox 10 Data 0 Register */
#define                  CAN1_MB10_DATA1  0xffc03544   /* CAN Controller 1 Mailbox 10 Data 1 Register */
#define                  CAN1_MB10_DATA2  0xffc03548   /* CAN Controller 1 Mailbox 10 Data 2 Register */
#define                  CAN1_MB10_DATA3  0xffc0354c   /* CAN Controller 1 Mailbox 10 Data 3 Register */
#define                 CAN1_MB10_LENGTH  0xffc03550   /* CAN Controller 1 Mailbox 10 Length Register */
#define              CAN1_MB10_TIMESTAMP  0xffc03554   /* CAN Controller 1 Mailbox 10 Timestamp Register */
#define                    CAN1_MB10_ID0  0xffc03558   /* CAN Controller 1 Mailbox 10 ID0 Register */
#define                    CAN1_MB10_ID1  0xffc0355c   /* CAN Controller 1 Mailbox 10 ID1 Register */
#define                  CAN1_MB11_DATA0  0xffc03560   /* CAN Controller 1 Mailbox 11 Data 0 Register */
#define                  CAN1_MB11_DATA1  0xffc03564   /* CAN Controller 1 Mailbox 11 Data 1 Register */
#define                  CAN1_MB11_DATA2  0xffc03568   /* CAN Controller 1 Mailbox 11 Data 2 Register */
#define                  CAN1_MB11_DATA3  0xffc0356c   /* CAN Controller 1 Mailbox 11 Data 3 Register */
#define                 CAN1_MB11_LENGTH  0xffc03570   /* CAN Controller 1 Mailbox 11 Length Register */
#define              CAN1_MB11_TIMESTAMP  0xffc03574   /* CAN Controller 1 Mailbox 11 Timestamp Register */
#define                    CAN1_MB11_ID0  0xffc03578   /* CAN Controller 1 Mailbox 11 ID0 Register */
#define                    CAN1_MB11_ID1  0xffc0357c   /* CAN Controller 1 Mailbox 11 ID1 Register */
#define                  CAN1_MB12_DATA0  0xffc03580   /* CAN Controller 1 Mailbox 12 Data 0 Register */
#define                  CAN1_MB12_DATA1  0xffc03584   /* CAN Controller 1 Mailbox 12 Data 1 Register */
#define                  CAN1_MB12_DATA2  0xffc03588   /* CAN Controller 1 Mailbox 12 Data 2 Register */
#define                  CAN1_MB12_DATA3  0xffc0358c   /* CAN Controller 1 Mailbox 12 Data 3 Register */
#define                 CAN1_MB12_LENGTH  0xffc03590   /* CAN Controller 1 Mailbox 12 Length Register */
#define              CAN1_MB12_TIMESTAMP  0xffc03594   /* CAN Controller 1 Mailbox 12 Timestamp Register */
#define                    CAN1_MB12_ID0  0xffc03598   /* CAN Controller 1 Mailbox 12 ID0 Register */
#define                    CAN1_MB12_ID1  0xffc0359c   /* CAN Controller 1 Mailbox 12 ID1 Register */
#define                  CAN1_MB13_DATA0  0xffc035a0   /* CAN Controller 1 Mailbox 13 Data 0 Register */
#define                  CAN1_MB13_DATA1  0xffc035a4   /* CAN Controller 1 Mailbox 13 Data 1 Register */
#define                  CAN1_MB13_DATA2  0xffc035a8   /* CAN Controller 1 Mailbox 13 Data 2 Register */
#define                  CAN1_MB13_DATA3  0xffc035ac   /* CAN Controller 1 Mailbox 13 Data 3 Register */
#define                 CAN1_MB13_LENGTH  0xffc035b0   /* CAN Controller 1 Mailbox 13 Length Register */
#define              CAN1_MB13_TIMESTAMP  0xffc035b4   /* CAN Controller 1 Mailbox 13 Timestamp Register */
#define                    CAN1_MB13_ID0  0xffc035b8   /* CAN Controller 1 Mailbox 13 ID0 Register */
#define                    CAN1_MB13_ID1  0xffc035bc   /* CAN Controller 1 Mailbox 13 ID1 Register */
#define                  CAN1_MB14_DATA0  0xffc035c0   /* CAN Controller 1 Mailbox 14 Data 0 Register */
#define                  CAN1_MB14_DATA1  0xffc035c4   /* CAN Controller 1 Mailbox 14 Data 1 Register */
#define                  CAN1_MB14_DATA2  0xffc035c8   /* CAN Controller 1 Mailbox 14 Data 2 Register */
#define                  CAN1_MB14_DATA3  0xffc035cc   /* CAN Controller 1 Mailbox 14 Data 3 Register */
#define                 CAN1_MB14_LENGTH  0xffc035d0   /* CAN Controller 1 Mailbox 14 Length Register */
#define              CAN1_MB14_TIMESTAMP  0xffc035d4   /* CAN Controller 1 Mailbox 14 Timestamp Register */
#define                    CAN1_MB14_ID0  0xffc035d8   /* CAN Controller 1 Mailbox 14 ID0 Register */
#define                    CAN1_MB14_ID1  0xffc035dc   /* CAN Controller 1 Mailbox 14 ID1 Register */
#define                  CAN1_MB15_DATA0  0xffc035e0   /* CAN Controller 1 Mailbox 15 Data 0 Register */
#define                  CAN1_MB15_DATA1  0xffc035e4   /* CAN Controller 1 Mailbox 15 Data 1 Register */
#define                  CAN1_MB15_DATA2  0xffc035e8   /* CAN Controller 1 Mailbox 15 Data 2 Register */
#define                  CAN1_MB15_DATA3  0xffc035ec   /* CAN Controller 1 Mailbox 15 Data 3 Register */
#define                 CAN1_MB15_LENGTH  0xffc035f0   /* CAN Controller 1 Mailbox 15 Length Register */
#define              CAN1_MB15_TIMESTAMP  0xffc035f4   /* CAN Controller 1 Mailbox 15 Timestamp Register */
#define                    CAN1_MB15_ID0  0xffc035f8   /* CAN Controller 1 Mailbox 15 ID0 Register */
#define                    CAN1_MB15_ID1  0xffc035fc   /* CAN Controller 1 Mailbox 15 ID1 Register */

/* CAN Controller 1 Mailbox Data Registers */

#define                  CAN1_MB16_DATA0  0xffc03600   /* CAN Controller 1 Mailbox 16 Data 0 Register */
#define                  CAN1_MB16_DATA1  0xffc03604   /* CAN Controller 1 Mailbox 16 Data 1 Register */
#define                  CAN1_MB16_DATA2  0xffc03608   /* CAN Controller 1 Mailbox 16 Data 2 Register */
#define                  CAN1_MB16_DATA3  0xffc0360c   /* CAN Controller 1 Mailbox 16 Data 3 Register */
#define                 CAN1_MB16_LENGTH  0xffc03610   /* CAN Controller 1 Mailbox 16 Length Register */
#define              CAN1_MB16_TIMESTAMP  0xffc03614   /* CAN Controller 1 Mailbox 16 Timestamp Register */
#define                    CAN1_MB16_ID0  0xffc03618   /* CAN Controller 1 Mailbox 16 ID0 Register */
#define                    CAN1_MB16_ID1  0xffc0361c   /* CAN Controller 1 Mailbox 16 ID1 Register */
#define                  CAN1_MB17_DATA0  0xffc03620   /* CAN Controller 1 Mailbox 17 Data 0 Register */
#define                  CAN1_MB17_DATA1  0xffc03624   /* CAN Controller 1 Mailbox 17 Data 1 Register */
#define                  CAN1_MB17_DATA2  0xffc03628   /* CAN Controller 1 Mailbox 17 Data 2 Register */
#define                  CAN1_MB17_DATA3  0xffc0362c   /* CAN Controller 1 Mailbox 17 Data 3 Register */
#define                 CAN1_MB17_LENGTH  0xffc03630   /* CAN Controller 1 Mailbox 17 Length Register */
#define              CAN1_MB17_TIMESTAMP  0xffc03634   /* CAN Controller 1 Mailbox 17 Timestamp Register */
#define                    CAN1_MB17_ID0  0xffc03638   /* CAN Controller 1 Mailbox 17 ID0 Register */
#define                    CAN1_MB17_ID1  0xffc0363c   /* CAN Controller 1 Mailbox 17 ID1 Register */
#define                  CAN1_MB18_DATA0  0xffc03640   /* CAN Controller 1 Mailbox 18 Data 0 Register */
#define                  CAN1_MB18_DATA1  0xffc03644   /* CAN Controller 1 Mailbox 18 Data 1 Register */
#define                  CAN1_MB18_DATA2  0xffc03648   /* CAN Controller 1 Mailbox 18 Data 2 Register */
#define                  CAN1_MB18_DATA3  0xffc0364c   /* CAN Controller 1 Mailbox 18 Data 3 Register */
#define                 CAN1_MB18_LENGTH  0xffc03650   /* CAN Controller 1 Mailbox 18 Length Register */
#define              CAN1_MB18_TIMESTAMP  0xffc03654   /* CAN Controller 1 Mailbox 18 Timestamp Register */
#define                    CAN1_MB18_ID0  0xffc03658   /* CAN Controller 1 Mailbox 18 ID0 Register */
#define                    CAN1_MB18_ID1  0xffc0365c   /* CAN Controller 1 Mailbox 18 ID1 Register */
#define                  CAN1_MB19_DATA0  0xffc03660   /* CAN Controller 1 Mailbox 19 Data 0 Register */
#define                  CAN1_MB19_DATA1  0xffc03664   /* CAN Controller 1 Mailbox 19 Data 1 Register */
#define                  CAN1_MB19_DATA2  0xffc03668   /* CAN Controller 1 Mailbox 19 Data 2 Register */
#define                  CAN1_MB19_DATA3  0xffc0366c   /* CAN Controller 1 Mailbox 19 Data 3 Register */
#define                 CAN1_MB19_LENGTH  0xffc03670   /* CAN Controller 1 Mailbox 19 Length Register */
#define              CAN1_MB19_TIMESTAMP  0xffc03674   /* CAN Controller 1 Mailbox 19 Timestamp Register */
#define                    CAN1_MB19_ID0  0xffc03678   /* CAN Controller 1 Mailbox 19 ID0 Register */
#define                    CAN1_MB19_ID1  0xffc0367c   /* CAN Controller 1 Mailbox 19 ID1 Register */
#define                  CAN1_MB20_DATA0  0xffc03680   /* CAN Controller 1 Mailbox 20 Data 0 Register */
#define                  CAN1_MB20_DATA1  0xffc03684   /* CAN Controller 1 Mailbox 20 Data 1 Register */
#define                  CAN1_MB20_DATA2  0xffc03688   /* CAN Controller 1 Mailbox 20 Data 2 Register */
#define                  CAN1_MB20_DATA3  0xffc0368c   /* CAN Controller 1 Mailbox 20 Data 3 Register */
#define                 CAN1_MB20_LENGTH  0xffc03690   /* CAN Controller 1 Mailbox 20 Length Register */
#define              CAN1_MB20_TIMESTAMP  0xffc03694   /* CAN Controller 1 Mailbox 20 Timestamp Register */
#define                    CAN1_MB20_ID0  0xffc03698   /* CAN Controller 1 Mailbox 20 ID0 Register */
#define                    CAN1_MB20_ID1  0xffc0369c   /* CAN Controller 1 Mailbox 20 ID1 Register */
#define                  CAN1_MB21_DATA0  0xffc036a0   /* CAN Controller 1 Mailbox 21 Data 0 Register */
#define                  CAN1_MB21_DATA1  0xffc036a4   /* CAN Controller 1 Mailbox 21 Data 1 Register */
#define                  CAN1_MB21_DATA2  0xffc036a8   /* CAN Controller 1 Mailbox 21 Data 2 Register */
#define                  CAN1_MB21_DATA3  0xffc036ac   /* CAN Controller 1 Mailbox 21 Data 3 Register */
#define                 CAN1_MB21_LENGTH  0xffc036b0   /* CAN Controller 1 Mailbox 21 Length Register */
#define              CAN1_MB21_TIMESTAMP  0xffc036b4   /* CAN Controller 1 Mailbox 21 Timestamp Register */
#define                    CAN1_MB21_ID0  0xffc036b8   /* CAN Controller 1 Mailbox 21 ID0 Register */
#define                    CAN1_MB21_ID1  0xffc036bc   /* CAN Controller 1 Mailbox 21 ID1 Register */
#define                  CAN1_MB22_DATA0  0xffc036c0   /* CAN Controller 1 Mailbox 22 Data 0 Register */
#define                  CAN1_MB22_DATA1  0xffc036c4   /* CAN Controller 1 Mailbox 22 Data 1 Register */
#define                  CAN1_MB22_DATA2  0xffc036c8   /* CAN Controller 1 Mailbox 22 Data 2 Register */
#define                  CAN1_MB22_DATA3  0xffc036cc   /* CAN Controller 1 Mailbox 22 Data 3 Register */
#define                 CAN1_MB22_LENGTH  0xffc036d0   /* CAN Controller 1 Mailbox 22 Length Register */
#define              CAN1_MB22_TIMESTAMP  0xffc036d4   /* CAN Controller 1 Mailbox 22 Timestamp Register */
#define                    CAN1_MB22_ID0  0xffc036d8   /* CAN Controller 1 Mailbox 22 ID0 Register */
#define                    CAN1_MB22_ID1  0xffc036dc   /* CAN Controller 1 Mailbox 22 ID1 Register */
#define                  CAN1_MB23_DATA0  0xffc036e0   /* CAN Controller 1 Mailbox 23 Data 0 Register */
#define                  CAN1_MB23_DATA1  0xffc036e4   /* CAN Controller 1 Mailbox 23 Data 1 Register */
#define                  CAN1_MB23_DATA2  0xffc036e8   /* CAN Controller 1 Mailbox 23 Data 2 Register */
#define                  CAN1_MB23_DATA3  0xffc036ec   /* CAN Controller 1 Mailbox 23 Data 3 Register */
#define                 CAN1_MB23_LENGTH  0xffc036f0   /* CAN Controller 1 Mailbox 23 Length Register */
#define              CAN1_MB23_TIMESTAMP  0xffc036f4   /* CAN Controller 1 Mailbox 23 Timestamp Register */
#define                    CAN1_MB23_ID0  0xffc036f8   /* CAN Controller 1 Mailbox 23 ID0 Register */
#define                    CAN1_MB23_ID1  0xffc036fc   /* CAN Controller 1 Mailbox 23 ID1 Register */
#define                  CAN1_MB24_DATA0  0xffc03700   /* CAN Controller 1 Mailbox 24 Data 0 Register */
#define                  CAN1_MB24_DATA1  0xffc03704   /* CAN Controller 1 Mailbox 24 Data 1 Register */
#define                  CAN1_MB24_DATA2  0xffc03708   /* CAN Controller 1 Mailbox 24 Data 2 Register */
#define                  CAN1_MB24_DATA3  0xffc0370c   /* CAN Controller 1 Mailbox 24 Data 3 Register */
#define                 CAN1_MB24_LENGTH  0xffc03710   /* CAN Controller 1 Mailbox 24 Length Register */
#define              CAN1_MB24_TIMESTAMP  0xffc03714   /* CAN Controller 1 Mailbox 24 Timestamp Register */
#define                    CAN1_MB24_ID0  0xffc03718   /* CAN Controller 1 Mailbox 24 ID0 Register */
#define                    CAN1_MB24_ID1  0xffc0371c   /* CAN Controller 1 Mailbox 24 ID1 Register */
#define                  CAN1_MB25_DATA0  0xffc03720   /* CAN Controller 1 Mailbox 25 Data 0 Register */
#define                  CAN1_MB25_DATA1  0xffc03724   /* CAN Controller 1 Mailbox 25 Data 1 Register */
#define                  CAN1_MB25_DATA2  0xffc03728   /* CAN Controller 1 Mailbox 25 Data 2 Register */
#define                  CAN1_MB25_DATA3  0xffc0372c   /* CAN Controller 1 Mailbox 25 Data 3 Register */
#define                 CAN1_MB25_LENGTH  0xffc03730   /* CAN Controller 1 Mailbox 25 Length Register */
#define              CAN1_MB25_TIMESTAMP  0xffc03734   /* CAN Controller 1 Mailbox 25 Timestamp Register */
#define                    CAN1_MB25_ID0  0xffc03738   /* CAN Controller 1 Mailbox 25 ID0 Register */
#define                    CAN1_MB25_ID1  0xffc0373c   /* CAN Controller 1 Mailbox 25 ID1 Register */
#define                  CAN1_MB26_DATA0  0xffc03740   /* CAN Controller 1 Mailbox 26 Data 0 Register */
#define                  CAN1_MB26_DATA1  0xffc03744   /* CAN Controller 1 Mailbox 26 Data 1 Register */
#define                  CAN1_MB26_DATA2  0xffc03748   /* CAN Controller 1 Mailbox 26 Data 2 Register */
#define                  CAN1_MB26_DATA3  0xffc0374c   /* CAN Controller 1 Mailbox 26 Data 3 Register */
#define                 CAN1_MB26_LENGTH  0xffc03750   /* CAN Controller 1 Mailbox 26 Length Register */
#define              CAN1_MB26_TIMESTAMP  0xffc03754   /* CAN Controller 1 Mailbox 26 Timestamp Register */
#define                    CAN1_MB26_ID0  0xffc03758   /* CAN Controller 1 Mailbox 26 ID0 Register */
#define                    CAN1_MB26_ID1  0xffc0375c   /* CAN Controller 1 Mailbox 26 ID1 Register */
#define                  CAN1_MB27_DATA0  0xffc03760   /* CAN Controller 1 Mailbox 27 Data 0 Register */
#define                  CAN1_MB27_DATA1  0xffc03764   /* CAN Controller 1 Mailbox 27 Data 1 Register */
#define                  CAN1_MB27_DATA2  0xffc03768   /* CAN Controller 1 Mailbox 27 Data 2 Register */
#define                  CAN1_MB27_DATA3  0xffc0376c   /* CAN Controller 1 Mailbox 27 Data 3 Register */
#define                 CAN1_MB27_LENGTH  0xffc03770   /* CAN Controller 1 Mailbox 27 Length Register */
#define              CAN1_MB27_TIMESTAMP  0xffc03774   /* CAN Controller 1 Mailbox 27 Timestamp Register */
#define                    CAN1_MB27_ID0  0xffc03778   /* CAN Controller 1 Mailbox 27 ID0 Register */
#define                    CAN1_MB27_ID1  0xffc0377c   /* CAN Controller 1 Mailbox 27 ID1 Register */
#define                  CAN1_MB28_DATA0  0xffc03780   /* CAN Controller 1 Mailbox 28 Data 0 Register */
#define                  CAN1_MB28_DATA1  0xffc03784   /* CAN Controller 1 Mailbox 28 Data 1 Register */
#define                  CAN1_MB28_DATA2  0xffc03788   /* CAN Controller 1 Mailbox 28 Data 2 Register */
#define                  CAN1_MB28_DATA3  0xffc0378c   /* CAN Controller 1 Mailbox 28 Data 3 Register */
#define                 CAN1_MB28_LENGTH  0xffc03790   /* CAN Controller 1 Mailbox 28 Length Register */
#define              CAN1_MB28_TIMESTAMP  0xffc03794   /* CAN Controller 1 Mailbox 28 Timestamp Register */
#define                    CAN1_MB28_ID0  0xffc03798   /* CAN Controller 1 Mailbox 28 ID0 Register */
#define                    CAN1_MB28_ID1  0xffc0379c   /* CAN Controller 1 Mailbox 28 ID1 Register */
#define                  CAN1_MB29_DATA0  0xffc037a0   /* CAN Controller 1 Mailbox 29 Data 0 Register */
#define                  CAN1_MB29_DATA1  0xffc037a4   /* CAN Controller 1 Mailbox 29 Data 1 Register */
#define                  CAN1_MB29_DATA2  0xffc037a8   /* CAN Controller 1 Mailbox 29 Data 2 Register */
#define                  CAN1_MB29_DATA3  0xffc037ac   /* CAN Controller 1 Mailbox 29 Data 3 Register */
#define                 CAN1_MB29_LENGTH  0xffc037b0   /* CAN Controller 1 Mailbox 29 Length Register */
#define              CAN1_MB29_TIMESTAMP  0xffc037b4   /* CAN Controller 1 Mailbox 29 Timestamp Register */
#define                    CAN1_MB29_ID0  0xffc037b8   /* CAN Controller 1 Mailbox 29 ID0 Register */
#define                    CAN1_MB29_ID1  0xffc037bc   /* CAN Controller 1 Mailbox 29 ID1 Register */
#define                  CAN1_MB30_DATA0  0xffc037c0   /* CAN Controller 1 Mailbox 30 Data 0 Register */
#define                  CAN1_MB30_DATA1  0xffc037c4   /* CAN Controller 1 Mailbox 30 Data 1 Register */
#define                  CAN1_MB30_DATA2  0xffc037c8   /* CAN Controller 1 Mailbox 30 Data 2 Register */
#define                  CAN1_MB30_DATA3  0xffc037cc   /* CAN Controller 1 Mailbox 30 Data 3 Register */
#define                 CAN1_MB30_LENGTH  0xffc037d0   /* CAN Controller 1 Mailbox 30 Length Register */
#define              CAN1_MB30_TIMESTAMP  0xffc037d4   /* CAN Controller 1 Mailbox 30 Timestamp Register */
#define                    CAN1_MB30_ID0  0xffc037d8   /* CAN Controller 1 Mailbox 30 ID0 Register */
#define                    CAN1_MB30_ID1  0xffc037dc   /* CAN Controller 1 Mailbox 30 ID1 Register */
#define                  CAN1_MB31_DATA0  0xffc037e0   /* CAN Controller 1 Mailbox 31 Data 0 Register */
#define                  CAN1_MB31_DATA1  0xffc037e4   /* CAN Controller 1 Mailbox 31 Data 1 Register */
#define                  CAN1_MB31_DATA2  0xffc037e8   /* CAN Controller 1 Mailbox 31 Data 2 Register */
#define                  CAN1_MB31_DATA3  0xffc037ec   /* CAN Controller 1 Mailbox 31 Data 3 Register */
#define                 CAN1_MB31_LENGTH  0xffc037f0   /* CAN Controller 1 Mailbox 31 Length Register */
#define              CAN1_MB31_TIMESTAMP  0xffc037f4   /* CAN Controller 1 Mailbox 31 Timestamp Register */
#define                    CAN1_MB31_ID0  0xffc037f8   /* CAN Controller 1 Mailbox 31 ID0 Register */
#define                    CAN1_MB31_ID1  0xffc037fc   /* CAN Controller 1 Mailbox 31 ID1 Register */

/* ATAPI Registers */

#define                    ATAPI_CONTROL  0xffc03800   /* ATAPI Control Register */
#define                     ATAPI_STATUS  0xffc03804   /* ATAPI Status Register */
#define                   ATAPI_DEV_ADDR  0xffc03808   /* ATAPI Device Register Address */
#define                  ATAPI_DEV_TXBUF  0xffc0380c   /* ATAPI Device Register Write Data */
#define                  ATAPI_DEV_RXBUF  0xffc03810   /* ATAPI Device Register Read Data */
#define                   ATAPI_INT_MASK  0xffc03814   /* ATAPI Interrupt Mask Register */
#define                 ATAPI_INT_STATUS  0xffc03818   /* ATAPI Interrupt Status Register */
#define                   ATAPI_XFER_LEN  0xffc0381c   /* ATAPI Length of Transfer */
#define                ATAPI_LINE_STATUS  0xffc03820   /* ATAPI Line Status */
#define                   ATAPI_SM_STATE  0xffc03824   /* ATAPI State Machine Status */
#define                  ATAPI_TERMINATE  0xffc03828   /* ATAPI Host Terminate */
#define                 ATAPI_PIO_TFRCNT  0xffc0382c   /* ATAPI PIO mode transfer count */
#define                 ATAPI_DMA_TFRCNT  0xffc03830   /* ATAPI DMA mode transfer count */
#define               ATAPI_UMAIN_TFRCNT  0xffc03834   /* ATAPI UDMAIN transfer count */
#define             ATAPI_UDMAOUT_TFRCNT  0xffc03838   /* ATAPI UDMAOUT transfer count */
#define                  ATAPI_REG_TIM_0  0xffc03840   /* ATAPI Register Transfer Timing 0 */
#define                  ATAPI_PIO_TIM_0  0xffc03844   /* ATAPI PIO Timing 0 Register */
#define                  ATAPI_PIO_TIM_1  0xffc03848   /* ATAPI PIO Timing 1 Register */
#define                ATAPI_MULTI_TIM_0  0xffc03850   /* ATAPI Multi-DMA Timing 0 Register */
#define                ATAPI_MULTI_TIM_1  0xffc03854   /* ATAPI Multi-DMA Timing 1 Register */
#define                ATAPI_MULTI_TIM_2  0xffc03858   /* ATAPI Multi-DMA Timing 2 Register */
#define                ATAPI_ULTRA_TIM_0  0xffc03860   /* ATAPI Ultra-DMA Timing 0 Register */
#define                ATAPI_ULTRA_TIM_1  0xffc03864   /* ATAPI Ultra-DMA Timing 1 Register */
#define                ATAPI_ULTRA_TIM_2  0xffc03868   /* ATAPI Ultra-DMA Timing 2 Register */
#define                ATAPI_ULTRA_TIM_3  0xffc0386c   /* ATAPI Ultra-DMA Timing 3 Register */

/* SDH Registers */

#define                      SDH_PWR_CTL  0xffc03900   /* SDH Power Control */
#define                      SDH_CLK_CTL  0xffc03904   /* SDH Clock Control */
#define                     SDH_ARGUMENT  0xffc03908   /* SDH Argument */
#define                      SDH_COMMAND  0xffc0390c   /* SDH Command */
#define                     SDH_RESP_CMD  0xffc03910   /* SDH Response Command */
#define                    SDH_RESPONSE0  0xffc03914   /* SDH Response0 */
#define                    SDH_RESPONSE1  0xffc03918   /* SDH Response1 */
#define                    SDH_RESPONSE2  0xffc0391c   /* SDH Response2 */
#define                    SDH_RESPONSE3  0xffc03920   /* SDH Response3 */
#define                   SDH_DATA_TIMER  0xffc03924   /* SDH Data Timer */
#define                    SDH_DATA_LGTH  0xffc03928   /* SDH Data Length */
#define                     SDH_DATA_CTL  0xffc0392c   /* SDH Data Control */
#define                     SDH_DATA_CNT  0xffc03930   /* SDH Data Counter */
#define                       SDH_STATUS  0xffc03934   /* SDH Status */
#define                   SDH_STATUS_CLR  0xffc03938   /* SDH Status Clear */
#define                        SDH_MASK0  0xffc0393c   /* SDH Interrupt0 Mask */
#define                        SDH_MASK1  0xffc03940   /* SDH Interrupt1 Mask */
#define                     SDH_FIFO_CNT  0xffc03948   /* SDH FIFO Counter */
#define                         SDH_FIFO  0xffc03980   /* SDH Data FIFO */
#define                     SDH_E_STATUS  0xffc039c0   /* SDH Exception Status */
#define                       SDH_E_MASK  0xffc039c4   /* SDH Exception Mask */
#define                          SDH_CFG  0xffc039c8   /* SDH Configuration */
#define                   SDH_RD_WAIT_EN  0xffc039cc   /* SDH Read Wait Enable */
#define                         SDH_PID0  0xffc039d0   /* SDH Peripheral Identification0 */
#define                         SDH_PID1  0xffc039d4   /* SDH Peripheral Identification1 */
#define                         SDH_PID2  0xffc039d8   /* SDH Peripheral Identification2 */
#define                         SDH_PID3  0xffc039dc   /* SDH Peripheral Identification3 */
#define                         SDH_PID4  0xffc039e0   /* SDH Peripheral Identification4 */
#define                         SDH_PID5  0xffc039e4   /* SDH Peripheral Identification5 */
#define                         SDH_PID6  0xffc039e8   /* SDH Peripheral Identification6 */
#define                         SDH_PID7  0xffc039ec   /* SDH Peripheral Identification7 */

/* HOST Port Registers */

#define                     HOST_CONTROL  0xffc03a00   /* HOST Control Register */
#define                      HOST_STATUS  0xffc03a04   /* HOST Status Register */
#define                     HOST_TIMEOUT  0xffc03a08   /* HOST Acknowledge Mode Timeout Register */

/* USB Control Registers */

#define                        USB_FADDR  0xffc03c00   /* Function address register */
#define                        USB_POWER  0xffc03c04   /* Power management register */
#define                       USB_INTRTX  0xffc03c08   /* Interrupt register for endpoint 0 and Tx endpoint 1 to 7 */
#define                       USB_INTRRX  0xffc03c0c   /* Interrupt register for Rx endpoints 1 to 7 */
#define                      USB_INTRTXE  0xffc03c10   /* Interrupt enable register for IntrTx */
#define                      USB_INTRRXE  0xffc03c14   /* Interrupt enable register for IntrRx */
#define                      USB_INTRUSB  0xffc03c18   /* Interrupt register for common USB interrupts */
#define                     USB_INTRUSBE  0xffc03c1c   /* Interrupt enable register for IntrUSB */
#define                        USB_FRAME  0xffc03c20   /* USB frame number */
#define                        USB_INDEX  0xffc03c24   /* Index register for selecting the indexed endpoint registers */
#define                     USB_TESTMODE  0xffc03c28   /* Enabled USB 20 test modes */
#define                     USB_GLOBINTR  0xffc03c2c   /* Global Interrupt Mask register and Wakeup Exception Interrupt */
#define                   USB_GLOBAL_CTL  0xffc03c30   /* Global Clock Control for the core */

/* USB Packet Control Registers */

#define                USB_TX_MAX_PACKET  0xffc03c40   /* Maximum packet size for Host Tx endpoint */
#define                         USB_CSR0  0xffc03c44   /* Control Status register for endpoint 0 and Control Status register for Host Tx endpoint */
#define                        USB_TXCSR  0xffc03c44   /* Control Status register for endpoint 0 and Control Status register for Host Tx endpoint */
#define                USB_RX_MAX_PACKET  0xffc03c48   /* Maximum packet size for Host Rx endpoint */
#define                        USB_RXCSR  0xffc03c4c   /* Control Status register for Host Rx endpoint */
#define                       USB_COUNT0  0xffc03c50   /* Number of bytes received in endpoint 0 FIFO and Number of bytes received in Host Tx endpoint */
#define                      USB_RXCOUNT  0xffc03c50   /* Number of bytes received in endpoint 0 FIFO and Number of bytes received in Host Tx endpoint */
#define                       USB_TXTYPE  0xffc03c54   /* Sets the transaction protocol and peripheral endpoint number for the Host Tx endpoint */
#define                    USB_NAKLIMIT0  0xffc03c58   /* Sets the NAK response timeout on Endpoint 0 and on Bulk transfers for Host Tx endpoint */
#define                   USB_TXINTERVAL  0xffc03c58   /* Sets the NAK response timeout on Endpoint 0 and on Bulk transfers for Host Tx endpoint */
#define                       USB_RXTYPE  0xffc03c5c   /* Sets the transaction protocol and peripheral endpoint number for the Host Rx endpoint */
#define                   USB_RXINTERVAL  0xffc03c60   /* Sets the polling interval for Interrupt and Isochronous transfers or the NAK response timeout on Bulk transfers */
#define                      USB_TXCOUNT  0xffc03c68   /* Number of bytes to be written to the selected endpoint Tx FIFO */

/* USB Endpoint FIFO Registers */

#define                     USB_EP0_FIFO  0xffc03c80   /* Endpoint 0 FIFO */
#define                     USB_EP1_FIFO  0xffc03c88   /* Endpoint 1 FIFO */
#define                     USB_EP2_FIFO  0xffc03c90   /* Endpoint 2 FIFO */
#define                     USB_EP3_FIFO  0xffc03c98   /* Endpoint 3 FIFO */
#define                     USB_EP4_FIFO  0xffc03ca0   /* Endpoint 4 FIFO */
#define                     USB_EP5_FIFO  0xffc03ca8   /* Endpoint 5 FIFO */
#define                     USB_EP6_FIFO  0xffc03cb0   /* Endpoint 6 FIFO */
#define                     USB_EP7_FIFO  0xffc03cb8   /* Endpoint 7 FIFO */

/* USB OTG Control Registers */

#define                  USB_OTG_DEV_CTL  0xffc03d00   /* OTG Device Control Register */
#define                 USB_OTG_VBUS_IRQ  0xffc03d04   /* OTG VBUS Control Interrupts */
#define                USB_OTG_VBUS_MASK  0xffc03d08   /* VBUS Control Interrupt Enable */

/* USB Phy Control Registers */

#define                     USB_LINKINFO  0xffc03d48   /* Enables programming of some PHY-side delays */
#define                        USB_VPLEN  0xffc03d4c   /* Determines duration of VBUS pulse for VBUS charging */
#define                      USB_HS_EOF1  0xffc03d50   /* Time buffer for High-Speed transactions */
#define                      USB_FS_EOF1  0xffc03d54   /* Time buffer for Full-Speed transactions */
#define                      USB_LS_EOF1  0xffc03d58   /* Time buffer for Low-Speed transactions */

/* (APHY_CNTRL is for ADI usage only) */

#define                   USB_APHY_CNTRL  0xffc03de0   /* Register that increases visibility of Analog PHY */

/* (APHY_CALIB is for ADI usage only) */

#define                   USB_APHY_CALIB  0xffc03de4   /* Register used to set some calibration values */
#define                  USB_APHY_CNTRL2  0xffc03de8   /* Register used to prevent re-enumeration once Moab goes into hibernate mode */

/* (PHY_TEST is for ADI usage only) */

#define                     USB_PHY_TEST  0xffc03dec   /* Used for reducing simulation time and simplifies FIFO testability */
#define                  USB_PLLOSC_CTRL  0xffc03df0   /* Used to program different parameters for USB PLL and Oscillator */
#define                   USB_SRP_CLKDIV  0xffc03df4   /* Used to program clock divide value for the clock fed to the SRP detection logic */

/* USB Endpoint 0 Control Registers */

#define                USB_EP_NI0_TXMAXP  0xffc03e00   /* Maximum packet size for Host Tx endpoint0 */
#define                 USB_EP_NI0_TXCSR  0xffc03e04   /* Control Status register for endpoint 0 */
#define                USB_EP_NI0_RXMAXP  0xffc03e08   /* Maximum packet size for Host Rx endpoint0 */
#define                 USB_EP_NI0_RXCSR  0xffc03e0c   /* Control Status register for Host Rx endpoint0 */
#define               USB_EP_NI0_RXCOUNT  0xffc03e10   /* Number of bytes received in endpoint 0 FIFO */
#define                USB_EP_NI0_TXTYPE  0xffc03e14   /* Sets the transaction protocol and peripheral endpoint number for the Host Tx endpoint0 */
#define            USB_EP_NI0_TXINTERVAL  0xffc03e18   /* Sets the NAK response timeout on Endpoint 0 */
#define                USB_EP_NI0_RXTYPE  0xffc03e1c   /* Sets the transaction protocol and peripheral endpoint number for the Host Rx endpoint0 */
#define            USB_EP_NI0_RXINTERVAL  0xffc03e20   /* Sets the polling interval for Interrupt/Isochronous transfers or the NAK response timeout on Bulk transfers for Host Rx endpoint0 */

/* USB Endpoint 1 Control Registers */

#define               USB_EP_NI0_TXCOUNT  0xffc03e28   /* Number of bytes to be written to the endpoint0 Tx FIFO */
#define                USB_EP_NI1_TXMAXP  0xffc03e40   /* Maximum packet size for Host Tx endpoint1 */
#define                 USB_EP_NI1_TXCSR  0xffc03e44   /* Control Status register for endpoint1 */
#define                USB_EP_NI1_RXMAXP  0xffc03e48   /* Maximum packet size for Host Rx endpoint1 */
#define                 USB_EP_NI1_RXCSR  0xffc03e4c   /* Control Status register for Host Rx endpoint1 */
#define               USB_EP_NI1_RXCOUNT  0xffc03e50   /* Number of bytes received in endpoint1 FIFO */
#define                USB_EP_NI1_TXTYPE  0xffc03e54   /* Sets the transaction protocol and peripheral endpoint number for the Host Tx endpoint1 */
#define            USB_EP_NI1_TXINTERVAL  0xffc03e58   /* Sets the NAK response timeout on Endpoint1 */
#define                USB_EP_NI1_RXTYPE  0xffc03e5c   /* Sets the transaction protocol and peripheral endpoint number for the Host Rx endpoint1 */
#define            USB_EP_NI1_RXINTERVAL  0xffc03e60   /* Sets the polling interval for Interrupt/Isochronous transfers or the NAK response timeout on Bulk transfers for Host Rx endpoint1 */

/* USB Endpoint 2 Control Registers */

#define               USB_EP_NI1_TXCOUNT  0xffc03e68   /* Number of bytes to be written to the+H102 endpoint1 Tx FIFO */
#define                USB_EP_NI2_TXMAXP  0xffc03e80   /* Maximum packet size for Host Tx endpoint2 */
#define                 USB_EP_NI2_TXCSR  0xffc03e84   /* Control Status register for endpoint2 */
#define                USB_EP_NI2_RXMAXP  0xffc03e88   /* Maximum packet size for Host Rx endpoint2 */
#define                 USB_EP_NI2_RXCSR  0xffc03e8c   /* Control Status register for Host Rx endpoint2 */
#define               USB_EP_NI2_RXCOUNT  0xffc03e90   /* Number of bytes received in endpoint2 FIFO */
#define                USB_EP_NI2_TXTYPE  0xffc03e94   /* Sets the transaction protocol and peripheral endpoint number for the Host Tx endpoint2 */
#define            USB_EP_NI2_TXINTERVAL  0xffc03e98   /* Sets the NAK response timeout on Endpoint2 */
#define                USB_EP_NI2_RXTYPE  0xffc03e9c   /* Sets the transaction protocol and peripheral endpoint number for the Host Rx endpoint2 */
#define            USB_EP_NI2_RXINTERVAL  0xffc03ea0   /* Sets the polling interval for Interrupt/Isochronous transfers or the NAK response timeout on Bulk transfers for Host Rx endpoint2 */

/* USB Endpoint 3 Control Registers */

#define               USB_EP_NI2_TXCOUNT  0xffc03ea8   /* Number of bytes to be written to the endpoint2 Tx FIFO */
#define                USB_EP_NI3_TXMAXP  0xffc03ec0   /* Maximum packet size for Host Tx endpoint3 */
#define                 USB_EP_NI3_TXCSR  0xffc03ec4   /* Control Status register for endpoint3 */
#define                USB_EP_NI3_RXMAXP  0xffc03ec8   /* Maximum packet size for Host Rx endpoint3 */
#define                 USB_EP_NI3_RXCSR  0xffc03ecc   /* Control Status register for Host Rx endpoint3 */
#define               USB_EP_NI3_RXCOUNT  0xffc03ed0   /* Number of bytes received in endpoint3 FIFO */
#define                USB_EP_NI3_TXTYPE  0xffc03ed4   /* Sets the transaction protocol and peripheral endpoint number for the Host Tx endpoint3 */
#define            USB_EP_NI3_TXINTERVAL  0xffc03ed8   /* Sets the NAK response timeout on Endpoint3 */
#define                USB_EP_NI3_RXTYPE  0xffc03edc   /* Sets the transaction protocol and peripheral endpoint number for the Host Rx endpoint3 */
#define            USB_EP_NI3_RXINTERVAL  0xffc03ee0   /* Sets the polling interval for Interrupt/Isochronous transfers or the NAK response timeout on Bulk transfers for Host Rx endpoint3 */

/* USB Endpoint 4 Control Registers */

#define               USB_EP_NI3_TXCOUNT  0xffc03ee8   /* Number of bytes to be written to the H124endpoint3 Tx FIFO */
#define                USB_EP_NI4_TXMAXP  0xffc03f00   /* Maximum packet size for Host Tx endpoint4 */
#define                 USB_EP_NI4_TXCSR  0xffc03f04   /* Control Status register for endpoint4 */
#define                USB_EP_NI4_RXMAXP  0xffc03f08   /* Maximum packet size for Host Rx endpoint4 */
#define                 USB_EP_NI4_RXCSR  0xffc03f0c   /* Control Status register for Host Rx endpoint4 */
#define               USB_EP_NI4_RXCOUNT  0xffc03f10   /* Number of bytes received in endpoint4 FIFO */
#define                USB_EP_NI4_TXTYPE  0xffc03f14   /* Sets the transaction protocol and peripheral endpoint number for the Host Tx endpoint4 */
#define            USB_EP_NI4_TXINTERVAL  0xffc03f18   /* Sets the NAK response timeout on Endpoint4 */
#define                USB_EP_NI4_RXTYPE  0xffc03f1c   /* Sets the transaction protocol and peripheral endpoint number for the Host Rx endpoint4 */
#define            USB_EP_NI4_RXINTERVAL  0xffc03f20   /* Sets the polling interval for Interrupt/Isochronous transfers or the NAK response timeout on Bulk transfers for Host Rx endpoint4 */

/* USB Endpoint 5 Control Registers */

#define               USB_EP_NI4_TXCOUNT  0xffc03f28   /* Number of bytes to be written to the endpoint4 Tx FIFO */
#define                USB_EP_NI5_TXMAXP  0xffc03f40   /* Maximum packet size for Host Tx endpoint5 */
#define                 USB_EP_NI5_TXCSR  0xffc03f44   /* Control Status register for endpoint5 */
#define                USB_EP_NI5_RXMAXP  0xffc03f48   /* Maximum packet size for Host Rx endpoint5 */
#define                 USB_EP_NI5_RXCSR  0xffc03f4c   /* Control Status register for Host Rx endpoint5 */
#define               USB_EP_NI5_RXCOUNT  0xffc03f50   /* Number of bytes received in endpoint5 FIFO */
#define                USB_EP_NI5_TXTYPE  0xffc03f54   /* Sets the transaction protocol and peripheral endpoint number for the Host Tx endpoint5 */
#define            USB_EP_NI5_TXINTERVAL  0xffc03f58   /* Sets the NAK response timeout on Endpoint5 */
#define                USB_EP_NI5_RXTYPE  0xffc03f5c   /* Sets the transaction protocol and peripheral endpoint number for the Host Rx endpoint5 */
#define            USB_EP_NI5_RXINTERVAL  0xffc03f60   /* Sets the polling interval for Interrupt/Isochronous transfers or the NAK response timeout on Bulk transfers for Host Rx endpoint5 */

/* USB Endpoint 6 Control Registers */

#define               USB_EP_NI5_TXCOUNT  0xffc03f68   /* Number of bytes to be written to the H145endpoint5 Tx FIFO */
#define                USB_EP_NI6_TXMAXP  0xffc03f80   /* Maximum packet size for Host Tx endpoint6 */
#define                 USB_EP_NI6_TXCSR  0xffc03f84   /* Control Status register for endpoint6 */
#define                USB_EP_NI6_RXMAXP  0xffc03f88   /* Maximum packet size for Host Rx endpoint6 */
#define                 USB_EP_NI6_RXCSR  0xffc03f8c   /* Control Status register for Host Rx endpoint6 */
#define               USB_EP_NI6_RXCOUNT  0xffc03f90   /* Number of bytes received in endpoint6 FIFO */
#define                USB_EP_NI6_TXTYPE  0xffc03f94   /* Sets the transaction protocol and peripheral endpoint number for the Host Tx endpoint6 */
#define            USB_EP_NI6_TXINTERVAL  0xffc03f98   /* Sets the NAK response timeout on Endpoint6 */
#define                USB_EP_NI6_RXTYPE  0xffc03f9c   /* Sets the transaction protocol and peripheral endpoint number for the Host Rx endpoint6 */
#define            USB_EP_NI6_RXINTERVAL  0xffc03fa0   /* Sets the polling interval for Interrupt/Isochronous transfers or the NAK response timeout on Bulk transfers for Host Rx endpoint6 */

/* USB Endpoint 7 Control Registers */

#define               USB_EP_NI6_TXCOUNT  0xffc03fa8   /* Number of bytes to be written to the endpoint6 Tx FIFO */
#define                USB_EP_NI7_TXMAXP  0xffc03fc0   /* Maximum packet size for Host Tx endpoint7 */
#define                 USB_EP_NI7_TXCSR  0xffc03fc4   /* Control Status register for endpoint7 */
#define                USB_EP_NI7_RXMAXP  0xffc03fc8   /* Maximum packet size for Host Rx endpoint7 */
#define                 USB_EP_NI7_RXCSR  0xffc03fcc   /* Control Status register for Host Rx endpoint7 */
#define               USB_EP_NI7_RXCOUNT  0xffc03fd0   /* Number of bytes received in endpoint7 FIFO */
#define                USB_EP_NI7_TXTYPE  0xffc03fd4   /* Sets the transaction protocol and peripheral endpoint number for the Host Tx endpoint7 */
#define            USB_EP_NI7_TXINTERVAL  0xffc03fd8   /* Sets the NAK response timeout on Endpoint7 */
#define                USB_EP_NI7_RXTYPE  0xffc03fdc   /* Sets the transaction protocol and peripheral endpoint number for the Host Rx endpoint7 */
#define            USB_EP_NI7_RXINTERVAL  0xffc03ff0   /* Sets the polling interval for Interrupt/Isochronous transfers or the NAK response timeout on Bulk transfers for Host Rx endpoint7 */
#define               USB_EP_NI7_TXCOUNT  0xffc03ff8   /* Number of bytes to be written to the endpoint7 Tx FIFO */
#define                USB_DMA_INTERRUPT  0xffc04000   /* Indicates pending interrupts for the DMA channels */

/* USB Channel 0 Config Registers */

#define                  USB_DMA0CONTROL  0xffc04004   /* DMA master channel 0 configuration */
#define                  USB_DMA0ADDRLOW  0xffc04008   /* Lower 16-bits of memory source/destination address for DMA master channel 0 */
#define                 USB_DMA0ADDRHIGH  0xffc0400c   /* Upper 16-bits of memory source/destination address for DMA master channel 0 */
#define                 USB_DMA0COUNTLOW  0xffc04010   /* Lower 16-bits of byte count of DMA transfer for DMA master channel 0 */
#define                USB_DMA0COUNTHIGH  0xffc04014   /* Upper 16-bits of byte count of DMA transfer for DMA master channel 0 */

/* USB Channel 1 Config Registers */

#define                  USB_DMA1CONTROL  0xffc04024   /* DMA master channel 1 configuration */
#define                  USB_DMA1ADDRLOW  0xffc04028   /* Lower 16-bits of memory source/destination address for DMA master channel 1 */
#define                 USB_DMA1ADDRHIGH  0xffc0402c   /* Upper 16-bits of memory source/destination address for DMA master channel 1 */
#define                 USB_DMA1COUNTLOW  0xffc04030   /* Lower 16-bits of byte count of DMA transfer for DMA master channel 1 */
#define                USB_DMA1COUNTHIGH  0xffc04034   /* Upper 16-bits of byte count of DMA transfer for DMA master channel 1 */

/* USB Channel 2 Config Registers */

#define                  USB_DMA2CONTROL  0xffc04044   /* DMA master channel 2 configuration */
#define                  USB_DMA2ADDRLOW  0xffc04048   /* Lower 16-bits of memory source/destination address for DMA master channel 2 */
#define                 USB_DMA2ADDRHIGH  0xffc0404c   /* Upper 16-bits of memory source/destination address for DMA master channel 2 */
#define                 USB_DMA2COUNTLOW  0xffc04050   /* Lower 16-bits of byte count of DMA transfer for DMA master channel 2 */
#define                USB_DMA2COUNTHIGH  0xffc04054   /* Upper 16-bits of byte count of DMA transfer for DMA master channel 2 */

/* USB Channel 3 Config Registers */

#define                  USB_DMA3CONTROL  0xffc04064   /* DMA master channel 3 configuration */
#define                  USB_DMA3ADDRLOW  0xffc04068   /* Lower 16-bits of memory source/destination address for DMA master channel 3 */
#define                 USB_DMA3ADDRHIGH  0xffc0406c   /* Upper 16-bits of memory source/destination address for DMA master channel 3 */
#define                 USB_DMA3COUNTLOW  0xffc04070   /* Lower 16-bits of byte count of DMA transfer for DMA master channel 3 */
#define                USB_DMA3COUNTHIGH  0xffc04074   /* Upper 16-bits of byte count of DMA transfer for DMA master channel 3 */

/* USB Channel 4 Config Registers */

#define                  USB_DMA4CONTROL  0xffc04084   /* DMA master channel 4 configuration */
#define                  USB_DMA4ADDRLOW  0xffc04088   /* Lower 16-bits of memory source/destination address for DMA master channel 4 */
#define                 USB_DMA4ADDRHIGH  0xffc0408c   /* Upper 16-bits of memory source/destination address for DMA master channel 4 */
#define                 USB_DMA4COUNTLOW  0xffc04090   /* Lower 16-bits of byte count of DMA transfer for DMA master channel 4 */
#define                USB_DMA4COUNTHIGH  0xffc04094   /* Upper 16-bits of byte count of DMA transfer for DMA master channel 4 */

/* USB Channel 5 Config Registers */

#define                  USB_DMA5CONTROL  0xffc040a4   /* DMA master channel 5 configuration */
#define                  USB_DMA5ADDRLOW  0xffc040a8   /* Lower 16-bits of memory source/destination address for DMA master channel 5 */
#define                 USB_DMA5ADDRHIGH  0xffc040ac   /* Upper 16-bits of memory source/destination address for DMA master channel 5 */
#define                 USB_DMA5COUNTLOW  0xffc040b0   /* Lower 16-bits of byte count of DMA transfer for DMA master channel 5 */
#define                USB_DMA5COUNTHIGH  0xffc040b4   /* Upper 16-bits of byte count of DMA transfer for DMA master channel 5 */

/* USB Channel 6 Config Registers */

#define                  USB_DMA6CONTROL  0xffc040c4   /* DMA master channel 6 configuration */
#define                  USB_DMA6ADDRLOW  0xffc040c8   /* Lower 16-bits of memory source/destination address for DMA master channel 6 */
#define                 USB_DMA6ADDRHIGH  0xffc040cc   /* Upper 16-bits of memory source/destination address for DMA master channel 6 */
#define                 USB_DMA6COUNTLOW  0xffc040d0   /* Lower 16-bits of byte count of DMA transfer for DMA master channel 6 */
#define                USB_DMA6COUNTHIGH  0xffc040d4   /* Upper 16-bits of byte count of DMA transfer for DMA master channel 6 */

/* USB Channel 7 Config Registers */

#define                  USB_DMA7CONTROL  0xffc040e4   /* DMA master channel 7 configuration */
#define                  USB_DMA7ADDRLOW  0xffc040e8   /* Lower 16-bits of memory source/destination address for DMA master channel 7 */
#define                 USB_DMA7ADDRHIGH  0xffc040ec   /* Upper 16-bits of memory source/destination address for DMA master channel 7 */
#define                 USB_DMA7COUNTLOW  0xffc040f0   /* Lower 16-bits of byte count of DMA transfer for DMA master channel 7 */
#define                USB_DMA7COUNTHIGH  0xffc040f4   /* Upper 16-bits of byte count of DMA transfer for DMA master channel 7 */

/* Keypad Registers */

#define                         KPAD_CTL  0xffc04100   /* Controls keypad module enable and disable */
#define                    KPAD_PRESCALE  0xffc04104   /* Establish a time base for programing the KPAD_MSEL register */
#define                        KPAD_MSEL  0xffc04108   /* Selects delay parameters for keypad interface sensitivity */
#define                      KPAD_ROWCOL  0xffc0410c   /* Captures the row and column output values of the keys pressed */
#define                        KPAD_STAT  0xffc04110   /* Holds and clears the status of the keypad interface interrupt */
#define                    KPAD_SOFTEVAL  0xffc04114   /* Lets software force keypad interface to check for keys being pressed */

/* Pixel Compositor (PIXC) Registers */

#define                         PIXC_CTL  0xffc04400   /* Overlay enable, resampling mode, I/O data format, transparency enable, watermark level, FIFO status */
#define                         PIXC_PPL  0xffc04404   /* Holds the number of pixels per line of the display */
#define                         PIXC_LPF  0xffc04408   /* Holds the number of lines per frame of the display */
#define                     PIXC_AHSTART  0xffc0440c   /* Contains horizontal start pixel information of the overlay data (set A) */
#define                       PIXC_AHEND  0xffc04410   /* Contains horizontal end pixel information of the overlay data (set A) */
#define                     PIXC_AVSTART  0xffc04414   /* Contains vertical start pixel information of the overlay data (set A) */
#define                       PIXC_AVEND  0xffc04418   /* Contains vertical end pixel information of the overlay data (set A) */
#define                     PIXC_ATRANSP  0xffc0441c   /* Contains the transparency ratio (set A) */
#define                     PIXC_BHSTART  0xffc04420   /* Contains horizontal start pixel information of the overlay data (set B) */
#define                       PIXC_BHEND  0xffc04424   /* Contains horizontal end pixel information of the overlay data (set B) */
#define                     PIXC_BVSTART  0xffc04428   /* Contains vertical start pixel information of the overlay data (set B) */
#define                       PIXC_BVEND  0xffc0442c   /* Contains vertical end pixel information of the overlay data (set B) */
#define                     PIXC_BTRANSP  0xffc04430   /* Contains the transparency ratio (set B) */
#define                    PIXC_INTRSTAT  0xffc0443c   /* Overlay interrupt configuration/status */
#define                       PIXC_RYCON  0xffc04440   /* Color space conversion matrix register. Contains the R/Y conversion coefficients */
#define                       PIXC_GUCON  0xffc04444   /* Color space conversion matrix register. Contains the G/U conversion coefficients */
#define                       PIXC_BVCON  0xffc04448   /* Color space conversion matrix register. Contains the B/V conversion coefficients */
#define                      PIXC_CCBIAS  0xffc0444c   /* Bias values for the color space conversion matrix */
#define                          PIXC_TC  0xffc04450   /* Holds the transparent color value */

/* Handshake MDMA 0 Registers */

#define                   HMDMA0_CONTROL  0xffc04500   /* Handshake MDMA0 Control Register */
#define                    HMDMA0_ECINIT  0xffc04504   /* Handshake MDMA0 Initial Edge Count Register */
#define                    HMDMA0_BCINIT  0xffc04508   /* Handshake MDMA0 Initial Block Count Register */
#define                  HMDMA0_ECURGENT  0xffc0450c   /* Handshake MDMA0 Urgent Edge Count Threshhold Register */
#define                HMDMA0_ECOVERFLOW  0xffc04510   /* Handshake MDMA0 Edge Count Overflow Interrupt Register */
#define                    HMDMA0_ECOUNT  0xffc04514   /* Handshake MDMA0 Current Edge Count Register */
#define                    HMDMA0_BCOUNT  0xffc04518   /* Handshake MDMA0 Current Block Count Register */

/* Handshake MDMA 1 Registers */

#define                   HMDMA1_CONTROL  0xffc04540   /* Handshake MDMA1 Control Register */
#define                    HMDMA1_ECINIT  0xffc04544   /* Handshake MDMA1 Initial Edge Count Register */
#define                    HMDMA1_BCINIT  0xffc04548   /* Handshake MDMA1 Initial Block Count Register */
#define                  HMDMA1_ECURGENT  0xffc0454c   /* Handshake MDMA1 Urgent Edge Count Threshhold Register */
#define                HMDMA1_ECOVERFLOW  0xffc04550   /* Handshake MDMA1 Edge Count Overflow Interrupt Register */
#define                    HMDMA1_ECOUNT  0xffc04554   /* Handshake MDMA1 Current Edge Count Register */
#define                    HMDMA1_BCOUNT  0xffc04558   /* Handshake MDMA1 Current Block Count Register */


/* ********************************************************** */
/*     SINGLE BIT MACRO PAIRS (bit mask and negated one)      */
/*     and MULTI BIT READ MACROS                              */
/* ********************************************************** */

/* Bit masks for PIXC_CTL */

#define                   PIXC_EN  0x1        /* Pixel Compositor Enable */
#define                  OVR_A_EN  0x2        /* Overlay A Enable */
#define                  OVR_B_EN  0x4        /* Overlay B Enable */
#define                  IMG_FORM  0x8        /* Image Data Format */
#define                  OVR_FORM  0x10       /* Overlay Data Format */
#define                  OUT_FORM  0x20       /* Output Data Format */
#define                   UDS_MOD  0x40       /* Resampling Mode */
#define                     TC_EN  0x80       /* Transparent Color Enable */
#define                  IMG_STAT  0x300      /* Image FIFO Status */
#define                  OVR_STAT  0xc00      /* Overlay FIFO Status */
#define                    WM_LVL  0x3000     /* FIFO Watermark Level */

/* Bit masks for PIXC_AHSTART */

#define                  A_HSTART  0xfff      /* Horizontal Start Coordinates */

/* Bit masks for PIXC_AHEND */

#define                    A_HEND  0xfff      /* Horizontal End Coordinates */

/* Bit masks for PIXC_AVSTART */

#define                  A_VSTART  0x3ff      /* Vertical Start Coordinates */

/* Bit masks for PIXC_AVEND */

#define                    A_VEND  0x3ff      /* Vertical End Coordinates */

/* Bit masks for PIXC_ATRANSP */

#define                  A_TRANSP  0xf        /* Transparency Value */

/* Bit masks for PIXC_BHSTART */

#define                  B_HSTART  0xfff      /* Horizontal Start Coordinates */

/* Bit masks for PIXC_BHEND */

#define                    B_HEND  0xfff      /* Horizontal End Coordinates */

/* Bit masks for PIXC_BVSTART */

#define                  B_VSTART  0x3ff      /* Vertical Start Coordinates */

/* Bit masks for PIXC_BVEND */

#define                    B_VEND  0x3ff      /* Vertical End Coordinates */

/* Bit masks for PIXC_BTRANSP */

#define                  B_TRANSP  0xf        /* Transparency Value */

/* Bit masks for PIXC_INTRSTAT */

#define                OVR_INT_EN  0x1        /* Interrupt at End of Last Valid Overlay */
#define                FRM_INT_EN  0x2        /* Interrupt at End of Frame */
#define              OVR_INT_STAT  0x4        /* Overlay Interrupt Status */
#define              FRM_INT_STAT  0x8        /* Frame Interrupt Status */

/* Bit masks for PIXC_RYCON */

#define                       A11  0x3ff      /* A11 in the Coefficient Matrix */
#define                       A12  0xffc00    /* A12 in the Coefficient Matrix */
#define                       A13  0x3ff00000 /* A13 in the Coefficient Matrix */
#define                  RY_MULT4  0x40000000 /* Multiply Row by 4 */

/* Bit masks for PIXC_GUCON */

#define                       A21  0x3ff      /* A21 in the Coefficient Matrix */
#define                       A22  0xffc00    /* A22 in the Coefficient Matrix */
#define                       A23  0x3ff00000 /* A23 in the Coefficient Matrix */
#define                  GU_MULT4  0x40000000 /* Multiply Row by 4 */

/* Bit masks for PIXC_BVCON */

#define                       A31  0x3ff      /* A31 in the Coefficient Matrix */
#define                       A32  0xffc00    /* A32 in the Coefficient Matrix */
#define                       A33  0x3ff00000 /* A33 in the Coefficient Matrix */
#define                  BV_MULT4  0x40000000 /* Multiply Row by 4 */

/* Bit masks for PIXC_CCBIAS */

#define                       A14  0x3ff      /* A14 in the Bias Vector */
#define                       A24  0xffc00    /* A24 in the Bias Vector */
#define                       A34  0x3ff00000 /* A34 in the Bias Vector */

/* Bit masks for PIXC_TC */

#define                  RY_TRANS  0xff       /* Transparent Color - R/Y Component */
#define                  GU_TRANS  0xff00     /* Transparent Color - G/U Component */
#define                  BV_TRANS  0xff0000   /* Transparent Color - B/V Component */

/* Bit masks for HOST_CONTROL */

#define                   HOST_EN  0x1        /* Host Enable */
#define                  HOST_END  0x2        /* Host Endianess */
#define                 DATA_SIZE  0x4        /* Data Size */
#define                  HOST_RST  0x8        /* Host Reset */
#define                  HRDY_OVR  0x20       /* Host Ready Override */
#define                  INT_MODE  0x40       /* Interrupt Mode */
#define                     BT_EN  0x80       /* Bus Timeout Enable */
#define                       EHW  0x100      /* Enable Host Write */
#define                       EHR  0x200      /* Enable Host Read */
#define                       BDR  0x400      /* Burst DMA Requests */

/* Bit masks for HOST_STATUS */

#define                 DMA_READY  0x1        /* DMA Ready */
#define                  FIFOFULL  0x2        /* FIFO Full */
#define                 FIFOEMPTY  0x4        /* FIFO Empty */
#define              DMA_COMPLETE  0x8        /* DMA Complete */
#define                      HSHK  0x10       /* Host Handshake */
#define                   TIMEOUT  0x20       /* Host Timeout */
#define                      HIRQ  0x40       /* Host Interrupt Request */
#define                ALLOW_CNFG  0x80       /* Allow New Configuration */
#define                   DMA_DIR  0x100      /* DMA Direction */
#define                       BTE  0x200      /* Bus Timeout Enabled */

/* Bit masks for HOST_TIMEOUT */

#define             COUNT_TIMEOUT  0x7ff      /* Host Timeout count */

/* Bit masks for MXVR_CONFIG */

#define                    MXVREN  0x1        /* MXVR Enable */
#define                      MMSM  0x2        /* MXVR Master/Slave Mode Select */
#define                    ACTIVE  0x4        /* Active Mode */
#define                    SDELAY  0x8        /* Synchronous Data Delay */
#define                   NCMRXEN  0x10       /* Normal Control Message Receive Enable */
#define                   RWRRXEN  0x20       /* Remote Write Receive Enable */
#define                     MTXEN  0x40       /* MXVR Transmit Data Enable */
#define                    MTXONB  0x80       /* MXVR Phy Transmitter On */
#define                   EPARITY  0x100      /* Even Parity Select */
#define                       MSB  0x1e00     /* Master Synchronous Boundary */
#define                    APRXEN  0x2000     /* Asynchronous Packet Receive Enable */
#define                    WAKEUP  0x4000     /* Wake-Up */
#define                     LMECH  0x8000     /* Lock Mechanism Select */

/* Bit masks for MXVR_STATE_0 */

#define                      NACT  0x1        /* Network Activity */
#define                    SBLOCK  0x2        /* Super Block Lock */
#define                   FMPLLST  0xc        /* Frequency Multiply PLL SM State */
#define                  CDRPLLST  0xe0       /* Clock/Data Recovery PLL SM State */
#define                     APBSY  0x100      /* Asynchronous Packet Transmit Buffer Busy */
#define                     APARB  0x200      /* Asynchronous Packet Arbitrating */
#define                      APTX  0x400      /* Asynchronous Packet Transmitting */
#define                      APRX  0x800      /* Receiving Asynchronous Packet */
#define                     CMBSY  0x1000     /* Control Message Transmit Buffer Busy */
#define                     CMARB  0x2000     /* Control Message Arbitrating */
#define                      CMTX  0x4000     /* Control Message Transmitting */
#define                      CMRX  0x8000     /* Receiving Control Message */
#define                    MRXONB  0x10000    /* MRXONB Pin State */
#define                     RGSIP  0x20000    /* Remote Get Source In Progress */
#define                     DALIP  0x40000    /* Resource Deallocate In Progress */
#define                      ALIP  0x80000    /* Resource Allocate In Progress */
#define                     RRDIP  0x100000   /* Remote Read In Progress */
#define                     RWRIP  0x200000   /* Remote Write In Progress */
#define                     FLOCK  0x400000   /* Frame Lock */
#define                     BLOCK  0x800000   /* Block Lock */
#define                       RSB  0xf000000  /* Received Synchronous Boundary */
#define                   DERRNUM  0xf0000000 /* DMA Error Channel Number */

/* Bit masks for MXVR_STATE_1 */

#define                   SRXNUMB  0xf        /* Synchronous Receive FIFO Number of Bytes */
#define                   STXNUMB  0xf0       /* Synchronous Transmit FIFO Number of Bytes */
#define                    APCONT  0x100      /* Asynchronous Packet Continuation */
#define                  OBERRNUM  0xe00      /* DMA Out of Bounds Error Channel Number */
#define                DMAACTIVE0  0x10000    /* DMA0 Active */
#define                DMAACTIVE1  0x20000    /* DMA1 Active */
#define                DMAACTIVE2  0x40000    /* DMA2 Active */
#define                DMAACTIVE3  0x80000    /* DMA3 Active */
#define                DMAACTIVE4  0x100000   /* DMA4 Active */
#define                DMAACTIVE5  0x200000   /* DMA5 Active */
#define                DMAACTIVE6  0x400000   /* DMA6 Active */
#define                DMAACTIVE7  0x800000   /* DMA7 Active */
#define                  DMAPMEN0  0x1000000  /* DMA0 Pattern Matching Enabled */
#define                  DMAPMEN1  0x2000000  /* DMA1 Pattern Matching Enabled */
#define                  DMAPMEN2  0x4000000  /* DMA2 Pattern Matching Enabled */
#define                  DMAPMEN3  0x8000000  /* DMA3 Pattern Matching Enabled */
#define                  DMAPMEN4  0x10000000 /* DMA4 Pattern Matching Enabled */
#define                  DMAPMEN5  0x20000000 /* DMA5 Pattern Matching Enabled */
#define                  DMAPMEN6  0x40000000 /* DMA6 Pattern Matching Enabled */
#define                  DMAPMEN7  0x80000000 /* DMA7 Pattern Matching Enabled */

/* Bit masks for MXVR_INT_STAT_0 */

#define                      NI2A  0x1        /* Network Inactive to Active */
#define                      NA2I  0x2        /* Network Active to Inactive */
#define                     SBU2L  0x4        /* Super Block Unlock to Lock */
#define                     SBL2U  0x8        /* Super Block Lock to Unlock */
#define                       PRU  0x10       /* Position Register Updated */
#define                      MPRU  0x20       /* Maximum Position Register Updated */
#define                       DRU  0x40       /* Delay Register Updated */
#define                      MDRU  0x80       /* Maximum Delay Register Updated */
#define                       SBU  0x100      /* Synchronous Boundary Updated */
#define                       ATU  0x200      /* Allocation Table Updated */
#define                      FCZ0  0x400      /* Frame Counter 0 Zero */
#define                      FCZ1  0x800      /* Frame Counter 1 Zero */
#define                      PERR  0x1000     /* Parity Error */
#define                      MH2L  0x2000     /* MRXONB High to Low */
#define                      ML2H  0x4000     /* MRXONB Low to High */
#define                       WUP  0x8000     /* Wake-Up Preamble Received */
#define                      FU2L  0x10000    /* Frame Unlock to Lock */
#define         /*
 * CopyrigFL2U  0x20000/*
 /* Frame Lock to UnlInc.*/
#define/*
 * Copyrigh licenseBU2L07-2408 Analog DB* Lic
 * Lictos Inc.ensed under the ADI BSD license o 2007-28L-2 (or later)
 * Inc.
 *
 * Licensed under the ADI BSD licenseOBERR07-2108 AAnal/* DMA Out of Bounds ErrorF_BF549_H
#define _DEF_BF549_H

/ PFthe G008 A/* SYSTPLL Frequencyers aedTIONS FOR ADSP-BF549 */

/* IncludSCZhe GPL-2 /* SYSTSystem C* LicCounter ZeroF_BF549_H
#define _DEF_BF549_H

/Fackfin.e all/* SYSTFIFODEFINITIONS FOR ADSP-BF549 */

/* IncludCMkfin.h>


/0 SYSTControl Message Receivines that are common to all ADSP-BCMROFefBF54x_bars */

#define                Buffer OverflowF_BF549_H
#define _DEF_BF549_H

/CMTSprocessorsrs */

#define        Transmit        Successfully Sent   TIMER8_COUNTER  0xffc00604   /* TCeded by ADter Register */
#define                    TIMER8_PCancellines that are common to all ADSP-BFRWRgisteegister0SYSTRemote Write

#define        Completines that are common to all ADSP-BF5Bx proc8 Confi  TIMer)
 *54x_base.h"

/* The following are the #definBMackfin.8 Count  TIMEiphase MarCOUNdingDEFINITIONS FOR ADSP-BF549 */

/* IncluD needed by AD      EM &EFINITION
     t masks for MXVR_INT_STAT_1     ed under the ADI BSD licensHDONE0      licenseine   0 Half DoneF_BF549_H
#define _DEF_BF549_H

efine    2              TI_CONFIG  0xffc00620   /* Timer 10 Co  AP*/
#de           Asynchronous Packet             TIMER8_CONFIG  0xffc00600   AP Timer 80624   /* Timer 10 Counter Register */                   TIMER8_COUNTER  0xffc00604  defin1                   1IMER10_CONFIG  0xffc00620   /* Timer 10 Configc00622   /* Timer 10 Wefine                  TIMER10_COUNTEAPTimer 8 624   /* Timer 10 Counter Regie                    TIMER8_PERIOD  0xffc00608   /* Timer 8 PeriodAPegister r Group of 3 Enable Register */
#define                   T   /* Timer 8 Width Register */
#define defin2                   2IMER10_CONFIG  0xffc00620   /* Timer 10 Configegist2r */

/* SPORT0 _CONFIG  0xffc00620   /* Timer 10 CoAPRCE/
#defiffc00628   /* Timer 10 Period RegisteCRCDEFINITIONS FOR ADSP-BF549 */

/* InclAPRPister8*/
#define                      SPORT0_er RegiEFINITIONS FOR ADSP-BF549 */

/* Incdefin3                   3IMER10_CONFIG  0xffc00620   /* Timer 10 Configt Ser008 Clock Divider_CONFIG  0xffc00620   /* Timer 10 Cdefin4                   4IMER10_CONFIG  0xffc00620   /* Timer 10 Configegist008 Analog De     /* SPORT0 Transmit Frame Sync Divider R5fin.h>


/* SYSTEM 5IMER10_CONFIG  0xffc00620   /* Timer 10 Config    S54x_base.h fffc00 /* SPORT0 Transmit Frame Sync Divider R6er Registers */
DMA6IMER10_CONFIG  0xffc00620   /* Timer 10 Configeceiv8 Configuration 1 /* SPORT0 Transmit Frame Sync Divider R7             TIMDMA7IMER10_CONFIG  0xffc00620   /* Timer 10 Configter *         TIM     _CONFIG         TIMER9_WIDTH  0xffcEN_0  /* Timer 9 Width Register */
#NI2AEN                Network Inactisteto Aync DiI_basrupt EnablNFIG  0xffc00620   /* Timer 10 CNA2Ic   /*on Register ive Framer Regitome Sync Dister */
#define                      SPORT0_STSor tc   /*00624   /* TSuperter)
 */

#ifndef _DEFine                      SPORT0_CHNL  0xffc00834L2Uc   /* 0xffc00628 Channel Regi Inc.
 *
 * Licster */
#define                      SPORT0_STA PR SPORT0c   /* TimerPosition RegisbaseUpda Conster */
#define                      SPORT0_STAMffc0083c 

#define   Maximum Multi channel Configuration Register 2 */
#define                     SPO Dfc0083c mer Group ofDelayannel Configuration Register 2 */
#define                     SPOR  SPORT0_ 3 Disable R  /* SPO00844   /* SPORT0 Multi channel Transmit Select Register 1 */
#define 834 0083c   /
#define Ser 10 CountDRESSary   /* SPORT0 Multi channel Transmit Select Register 2 */
AT_MTCS0  0/
#define  llocai chaTine  iguration Register 2 */
#define                     SPOFCZ0PORT0_MT/
#define evices54x_base0e.h"

MRCS0  0xffc00850   /* SPORT0 Multi channel Receiv1        Register 0 */
#define   1                 SPORT0_MRCS1  0xffc00854   /* SPORT0PERRfine       0xffc00ParityT0_TCLKn Register 2 */
#define                     SPORH  /* SPODIV  0xffc00MRXONB Highndef _weceive Select Register 2 */
#define              L2H Select R0_MRCS3  0xffc00 SPOto085c  RT0 Multi channel Transmit Select Register 2 */
WUPti channe  0xffc00Wake-Up Preamne           MRCS0  0xffc00850   /* SPORT0 Multi channel Rece   /* SPOer */
#definevicesster */
#define                     SPORT0_MCMC1  0xffc00838 ht 20    SPORT0Analog Devices Inc.
 *
 * Licster */
#define                      SPORT0_STA4   /* SPORL-2 (or later)
 */

#ifndef _DEF                    EPPI0_VCOUNT  0xffc0100c   //* SPORT0  all Core registers and bit defister */
#define                      SPORT0_STBlackfer Count R/* SYSTEM & MMR ADDRESS DEFINIT */
#define                     SPORT0_MCMC2  0xfF     SPORT0base.h for the set of #defineRT0 Multi channel Transmit Select Register 2 */
#CZPPI0 Vertics */
#include "defBF54x_base.h"

           EPPI0_HDELAY  0xffc01008   /* EPPI0 HE  0xffc0 by ADSP-BF549 that areRT0 Multi channel Transmit Select Register 2 */
CM 0xffc01014  rs */

#define                 ster */
#define                      SPORT0_ST/* Tixffc01018   iguration Register */
#define                 ster */
#define                      SPORT0_STA* Ti  0xffc0101cter Register */
#define                    TIMER8_PERIOD /* EPPI0 FS1 Period Register / EPPI0 Active VideoCc01020   /* define                     TIMER8_WIDTH  0xffc0060c   /* Timester */
#define                      SPORT0_STA    0xffc01024     TIMER9_CONFIG  0xffc00610   /* Timer 9 ConRT0 Multi channel Transmit Select Register 2 */
BIV  0xff         TIMER9_COUNTER  0xffc0ster */
#define                      SPORT0_STter *Samples Per L                  TIMER9_PERIODster */
#define                      SPORT0_STA Reg Lines of Verefine          ster */
#define     der Register */
#define          /* Timer 9 Width Register */definENe                   TIMER10_CONFster */
#define                      SPORT0_ST Register *on Register */
#define4c   /* SPORT0 Multi channel Transmit Select RegiP2_DLL  00624   /* Timer 10 Counter Register */
#ster */
#define                      SPORT0_ST0_PERSPORT0 Multi chann  /* Timer 10 Period Register */
#define    ster */
#define                      SPORT0_Sl Regisc0062c   /* Timer 10 Width Regis      UART2_LCR  0xffc0210c   /* Line Control Registrs */

#define             ster */
#define                      SPORT0_STA   /PORT0_MTCS1  0xffc 3 Enable Register */
#define                   TIMER_Enable Register Set */
#define                  UA0 Lines o Disable Register */
#define                    TIMER_STATUS1  0xffc0atus Register */
#define                        UARTegister */

/* SPORT0 Registers       UART2_LCR  0xffc0210c   /* Line Control RegistORT0_TCR1  0xffc00800   /* 2_LSR  0xffc02114   /* Line Status Register */
#dCE Select Register 0                     SPORT0_TCR2  0xff2_LSR  0xffc02114   /* Line Status Register */
#dP#definen 2 Register */
#define                   SPORT0_TCLKatus Register */
#define                        UARTt Serial Clock Divider Register       UART2_LCR  0xffc0210c   /* Line Control Regist0_TFSDIV  0xffc0080c   /* Satus Register */
#define                        UARTegister */
#define                    UART2_LCR  0xffc0210c   /* Line Control Regist* SPORT0 Transmit Data Regiatus Register */
#define                        UART    SPORT0_RX  0xffc00818   /* S      UART2_LCR  0xffc0210c   /* Line Control Registine                      SPatus Register */
#define                        UARTeceive Configuration 1 Register       UART2_LCR  0xffc0210c   /* Line Control RegistRT0_RCR2  0xffc00824   /* Satus Register */
#define                        UARTter */
#define                        UART2_LCR  0xffc0210c   /* Line Control RegistORT0 Receive Serial Clock Dh Byte */
#define                       UARPOSITION  /* Timer 9 Width Register */          0x3f0   /* SPORode Multi cha            SPORT0_MRCS2  0xffcVALID Register */
#densmit Data DouValines t        TIMER9_WIDTH  0MAX                  TWI1_XMT_DATA16  0xfMc02284   /* TWI FIFO Tra  /* SPOnsmit Data Double Byte Register */
#define M                 TWI1_6  0xffc0228c   /* TWIfc02288   /* TWI FIFO Receive DaDELAY  /* Timer 9 Width Register */
# E  0xf/* TWI FIFO Transmitevices00844                   UART2_DLH  0x                TWI1_RCV_D00   /* SPI2fc02288   /* TWI FIFO Receive Data SE  0xffc02400
#define                M        SPI2_CTL  0xf6  0xffc0228c00   /* SPI2 Control Register */
#define M                      Status Register */
#definfc02288   /* TWI FIFO Receive DaLADDRffc02400
#define                        0xfffWI FIF/* Logical Address2 Control Register */
#define  L              Divisorffer Register */fc02288   /* TWI FIFO Receive DaG           SPI2_RDBR  0xffc02410   /     the Gve Datata BuGroupgister */LownnelytNFIG  0xffc00620   /* Timer 10 CG                TWI1_ta Buffer Shadfc02288   /* TWI FIFO Receive DaA           SPI2_RDBR  0xffc02410   /*ation Receive Data BuAlternategister */
#define                      A SPI2_BAUD  0xffc0241 0xffc02708   /* MG  0xffc02700   /* MXVR ConfiguraLLOC              SPORT0_RFSDIV  0xffc VidLe    7WI FIFO TraChannel 0

#dneci chaLabel   TIMER8_COUNTER  0xffc00604   /*IUe    e Buffer Reg* MXVR IntIn Usne                  TIMER10_COUNTERCLc00627fter */
#de* MXVR Interrupt Status Register 0 */
#define                 c0062ister */
#de 0xffc02714   /* MXVR Interrupt Status Register 1 */egistine 1c   /* * MXVR Interrupt Status Register 0 */
#define                 egist by ADSP-BF5 0xffc02714   /* MXVR Interrupt Status Register 1 */t SerMXVR Iivisor* MXVR Interrupt Status Register 0 */
#define                 t SerD  0xffc0241 0xffc02714   /* MXV/* MXVR State Register 1 */
#   /* Timer 9 Width Register */
# 1 */egistffc02710   /* MXVR I4terrupt Status Register 0 */
#define                 egist_INT_STAT_1  0xffc02414   /* MXVR Interrupt Status Register 1 */    Sine                 5terrupt Status Register 0 */
#define                     Sister 0 */
#define  514   /* MXVR Interrupt Status Register 1 */eceivMXVR Interrupt Enabl6terrupt Status Register 0 */
#define                 eceivxffc02720   /* MXVR 614   /* MXVR Interrupt Status Register 1 */ter *MXVR_MAX_POSITION  07terrupt Status Register 0 */
#define                 ter *ne                  7    MXVR_DELAY  0xffc02728   /* MXVR Nod2 Frame Delay Register */
#define     8             MXVR_MAX_DE8terrupt Status Register 0 */
#define                 _2  0_INT_STAT_1  0xffc02814   /* MXVR Interrupt Status Register 1 */9R Logical Address Regist9terrupt Status Register 0 */
#define                 /
#deister 0 */
#define  914   /* MXVR Interrupt Status Register 1CLc   XVR_AADDR  0xffc02738 1nterrupt Status Register 0 */
#define             able MXVR_IXVR Allocation Table                   MXVR_ALLOC_5  0xffc02750
#define _MAX_POSITION  011 Register 5 */
#define                     MXVR_ALLOCe Registe     MXVR_ALLOC_7     MXVR_DELAY  0xffc02728   /* MXVR Nod3 Frame Delay Register */
#define  CL1  /* MXNT_STAT_1  0xffc0212 Register 5 */
#define                     MXVR_ALLOCON  0xf          MXVR_ALLOC_                  MXVR_ALLOC_5  0xffc02750     MXVR        MXVR_ALLO3 Register 5 */
#define                     MXVR_ALLOC#define  ble Register 10 */
                  MXVR_ALLOC_5  0xffc02750       XVR Allocation TablLAY  0xffc0272c   /* MXVR Maximum Node Frame Delayableister *LOC_12  0xffc0276c                     MXVR_ALLOC_5  0xffc02750VR Logica     MXVR_ALLOC_7er */
#define                       MXVR_GADDR  0xable2734   /* */
#define           MXVR_DELAY  0xffc02728   /* MXVR Nod4ation Table Register 8 */
#define      MXVR_          MXVR_ALLO  /* MXVR Alternate Address Register */

/* MXVR Aableation Tc02778   /* MXVR Sync                  MXVR_ALLOC_5  0xffc02750xffc0273c        MXVR_ALLOation Table Register 0 */
#define                 ableMXVR_ALLO Assign Register 1                   MXVR_ALLOC_5  0xffc02750_2  0xfXVR Allocation TablAllocation Table Register 2 */
#define            able     MX       MXVR_SYNC_LCHA                  MXVR_ALLOC_5  0xffc02750/
#define     MXVR_ALLOC_7     MXVR_ALLOC_4  0xffc0274c   /* MXVR Allocationablele RegistMXVR Sync Data Logi   MXVR_DELAY  0xffc02728   /* MXVR Nod5ation Table Register 8 */
#define   

#d           MXVR_MAX_DE2e Register 5 */
#define                     MXVR_ALLO               MXVR_SYNC_L2llocation Table Register 6 */
#define      2
#define                 2  0xffc02758   /* MXVR Allocation Table Register 7 */  /* Mister 0 */
#define  2  MXVR_ALLOC MXVR_SYNC_LCHAN_7  0xffc02794    /* MXVR Interrupt Enabl2C_9  0xffc02760   /* MXVR Allocation Table Register 9nc Datn Register 3 */
#def2    MXVR_ALLOC_10  0xffc02764   /* MXVR All2     MXVR_MAX_POSITION  02/
#define                    MXVR_ALLOC_11  0xffc0276  MXVRXVR_SYNC_LCHAN_5  0x2e Register 1/* MXVR State Register 1 */
#6l Assign Register 5 */
#define                     MXVR_MAX_DE2  /* MXVR Allocation Table Register 12 */
#define          ister 6 */
#define   13  0xffc02770   /* MXVR Allocation Table R2VR Logical Address Regist2              MXVR_ALLOC_14  0xffc02774   /* MXVR All1_CONFdefine               ess Register */
#define                  CL2  MXVR_AADDR  0xffc02738 2nc Data Logical Channel Assign Register 0 */
#define ddress /* MXVR Sync Data DMHAN_1  0xffc0277c   /* MXVR Sync Data Logic2xffc0273c   /* MXVR Alloc21 */
#define                MXVR_SYNC_LCHAN_2  0xffc0R  0xf             MXVR_DMA  /* MXVR Allocation Table Register 1 */
7l Assign Register 5 */
#define       _2  0xffc02744   /* MXVR 2HAN_3  0xffc02784   /* MXVR Sync Data Logical Channel
#defiister 6 */
#define   ne                MXVR_SYNC_LCHAN_4  0xffc02/
#define                2gical Channel Assign Register 4 */
#define              /* define                                   MXVR_ALLOC_5  0xffc02753   /* MXVR Allocation Tab3e Register 5 */
#define                     MXVR_ALLOe     n Register 3 */
#def3llocation Table Register 6 */
#define      3              MXVR_ALLOC_3  0xffc02758   /* MXVR Allocation Table Register 7 */XVR SyXVR_SYNC_LCHAN_5  0x3  MXVR_ALLOC_8  0xffc0275c   /* MXVR Allo8ation Table Register 8 */
#define   3                 MXVR_ALL3C_9  0xffc02760   /* MXVR Allocation Table Register 9 MXVR_         MXVR_SYNC_L3    MXVR_ALLOC_10  0xffc02764   /* MXVR All3cation Table Register 10 3/
#define                    MXVR_ALLOC_11  0xffc0276MA3 Loister 0 */
#define  3e Register 11 */
#define                   3MXVR_ALLOC_12  0xffc0276c3  /* MXVR Allocation Table Register 12 */
#define    MA3_CUs */
#define         13  0xffc02770   /* MXVR Allocation Table R3gister 13 */
#define     3              MXVR_ALLOC_14  0xffc02774   /* MXVR All27e8         MXVR_DMA3_CONF/

/* MXVR Channel Assign Registers */

#9nfig Register */
#define             0  0xffc02778   /* MXVR S3nc Data Logical Channel Assign Register 0 */
#define  0xffc               MXVR_DHAN_1  0xffc0277c   /* MXVR Sync Data Logic3l Channel Assign Register31 */
#define                MXVR_SYNC_LCHAN_2  0xffc0ess */XVR Sync Data DMA3 Cugical Channel Assign Register 2 */
#define 3              MXVR_SYNC_L3HAN_3  0xffc02784   /* MXVR Sync Data Logical Channel      s */
#define         ne                MXVR_SYNC_LCHAN_4  0xffc03788   /* MXVR Sync Data L3gical Channel Assign Register 4 */
#define           Data D       MXVR_DMA3_CONFfc0278c   /* MXVR Sync Data Logical Chann1define                  MXVR_INT_STCLmer            MXVR_MAX_DELe Register 5 */
#define                     MXVR_ALLOMXVR S */
#define          llocation Table Register 6 */
#define      4
#define                 4  0xffc02758   /* MXVR Allocation Table Register 7 */A6 Register 0 */
#define  4  MXVR_DMA0_CONFIG  0xffc02798   /* MXVR Sy4  /* MXVR Interrupt Enabl4C_9  0xffc02760   /* MXVR Allocation Table Register 9ADDR  n Register 3 */
#def4    MXVR_ALLOC_10  0xffc02764   /* MXVR All4     MXVR_MAX_POSITION  04/
#define                    MXVR_ALLOC_11  0xffc0276gisterXVR_SYNC_LCHAN_5  0x40_CURR_ADDR  0xffc027a4   /* MXVR Sync Da1e Frame Delay Register */
#define  CL4              MXVR_MAX_DEL  /* MXVR Allocation Table Register 12 */
#define    urrent */
#define          13  0xffc02770   /* MXVR Allocation Table R4VR Logical Address Regist4              MXVR_ALLOC_14  0xffc02774   /* MXVR Alline   ata DMA6 Config Regis0xffc027b0   /* MXVR Sync Data DMA1 Start A4  MXVR_AADDR  0xffc02738 4nc Data Logical Channel Assign Register 0 */
#define XVR SyVR_DMA6_COUNT  0xffc0HAN_1  0xffc0277c   /* MXVR Sync Data Logic4xffc0273c   /* MXVR Alloc41 */
#define                MXVR_SYNC_LCHAN_2  0xffc0      6 Current Address */
  /* MXVR Allocation Table Register 1 */
1#define                     MXVR_ALCL4_2  0xffc02744   /* MXVR 4HAN_3  0xffc02784   /* MXVR Sync Data Logical ChannelVR Asy */
#define          ne                MXVR_SYNC_LCHAN_4  0xffc04/
#define                4gical Channel Assign Register 4 */
#define           ne    ata DMA6 Config Regis                    MXVR_ALLOC_5  0xffc02755   /* MXVR Allocation Tab5e Register 5 */
#define                     MXVR_ALLOffc028n Register 3 */
#def5llocation Table Register 6 */
#define      5              MXVR_ALLOC_5  0xffc02758   /* MXVR Allocation Table Register 7 */ CurreXVR_SYNC_LCHAN_5  0x5  MXVR_ALLOC_8  0xffc0275c   /* MXVR Allo1cation Table Register 8 */
#define   5                 MXVR_ALL5C_9  0xffc02760   /* MXVR Allocation Table Register 9VR_CMR         MXVR_SYNC_L5    MXVR_ALLOC_10  0xffc02764   /* MXVR All5cation Table Register 10 5/
#define                    MXVR_ALLOC_11  0xffc0276VR Con  /* MXVR Group Addree Register 11 */
#define                   5MXVR_ALLOC_12  0xffc0276c5  /* MXVR Allocation Table Register 12 */
#define    gisterMXVR_APTB_CURR_ADDR  13  0xffc02770   /* MXVR Allocation Table R5gister 13 */
#define     5              MXVR_ALLOC_14  0xffc02774   /* MXVR All/

#de        MXVR_CM_CTL  /

/* MXVR Channel Assign Registers */

#1define                MXVR_SYNC_LCHAN50  0xffc02778   /* MXVR S5nc Data Logical Channel Assign Register 0 */
#define fer Cu */
#define          HAN_1  0xffc0277c   /* MXVR Sync Data Logic5l Channel Assign Register51 */
#define                MXVR_SYNC_LCHAN_2  0xffc00 */
#  /* MXVR Group Addregical Channel Assign Register 2 */
#define 5              MXVR_SYNC_L5HAN_3  0xffc02784   /* MXVR Sync Data Logical Channel  /* MMXVR_APTB_CURR_ADDR  ne                MXVR_SYNC_LCHAN_4  0xffc05788   /* MXVR Sync Data L5gical Channel Assign Register 4 */
#define           r Regi        MXVR_CM_CTL  fc0278c   /* MXVTH  0SYNC_LCHA    MIMER9 /* Timer 9 CNT_1PC MXVR 0xVR_SYNCFlu   /* MXVR Frame      er 1 */
F0* MXVR Routing Tabon Reger 1 */F0

#define          3        MXVF0ROUTING_0  0xffc0288er */
er 1 Rout

#define          50 */
#defne   

#define          60 */
#de_1  0x

#define          70 */
#dTable R

#d    MXVR_FRAME_CNT_1 10xffc0287c   /* MXVR Frame 0xffcer 1 */

/* MXVR Routing Tab9e Registers */

#define          c   /*   MXVR_ROUTING_0  0xffc0288
#de/* MXVR Routing Table Register */
/
#define                   MX8   OUTING_1  0xffc02884   /* MXVR    ting Table Register 1 */
#defiocat                 MXVR_ROUTING_2  0xf20xffc0287c   /* MXVR Frame    nter 1 */

/* MXVR Routing Tablne        MXVR_ROUTING_3  0xffc028 0xff/* MXVR Routing Table Registe      MXVR Routing Table Register

#def#define                   MX  /*OUTING_1  0xffc02884   /* MXVRnc Dting Table Register 1 */
#defi  MX                 MXVR_ROUTING_2  0xf30xffc0287c   /* MXVR Frame     Register 2 */
#define        1_COegisters */

#define                       _ROUTING_0  0xffc0288R  0VR_ROUTING_7  0xffc0289c   /* Mter 6 */
#g Table Register 7 */
#d MXVR_ROU_1  0xffc02884   /* MXVRe   ting Table Register 1 */
#defiXVR                  MXVR_ROUTING_2  0xf40xffc0287c   /* MXVR Frame MXV Register 2 */
#define        MA3 8   /* MXVR Routing Table RegiMA3_     MXVR_ROUTING_0  0xffc02880ister 9 */
Routing Table Register 0xf Routing Table Register 7 */
#ess G_11  0xffc028ac   /* MXVR Routter 6 */e Register 11 */
#define   MXVR_R             MXVR_ROUTING_2  0xf50xffc0287c   /* MXVR Framemer Grer 1 */

/* MXVR Routing TabA6 R8   /* MXVR Routing Table RegiADDR 6 */
#define                 gist/* MXVR Routing Table Register    /* MXVRne                   MXine G_11  0xffc028ac   /* MXVR RouXVR R Counter-Clock-Control Regist    */

#define                   MXVR_B6OCK_CNT  0xffc028c0   /* MXter 6 */
#denter */
#define        MXVR_ROUTI */

#define          ffc0 6 */
#define                  CurVR_ROUTING_7  0xffc0289c   /* VR_C/
#define                   MXVR Routing Table Register 4 */
#degistR Counter-Clock-Control Regist/

#*/

#define                   MXVR_B70xffc0287c   /* MXVR Framefer Block Counter */
#define      0 */        MXVR_PIN_CTL  0xffc028dter 6 */
#define                 r ReVR_ROUTING_7  0            SPI2_REGBASEMAx_CONFIG287c   /* MXV               SPI2_TM2c   /* SPORT0 ReceEM &* MXVR Igister 0 */
#define                    MAD     on Register */
 /* CAN CDirpt StatA0_CONFIG  0xffc02798   /* BY4SWAtus Reg

#define       /* CAN CFouegisterSwapController 1 Transmit Request Set RegisCNT_1   SPc */

/* SPORT /* CAN Cffer Reg* MXVR I1_TRR1  0xffc0320c   /* CAN ITntroller 1t Register 0N Controller     */
#define                         CANBY2ntroller 1nnel ReceiveN ControllerTwoster 1 */
#define                         CAN1_TAMFLOWVR SyIV  0xffc0080csmit AcknOper      F      TIMER8_COUNTER  0xffc00604 FIXEDPMclude all Core rquest Reset Rixed Patxffc MatchER9_SelecOD  0xffc00608   /* Timer 8 PeSTARTPAT   SP>


/* SYSTEM &* MXVR IStartr 1 Receie Lost Register 1 */
#define        STOP       c  CAN1_MBTIF1  0xffc03220op/* CAN Controller 1 Mailbox Transmit InterruCOUNTPOimer 1ster 1efine    * MXVR I54x_bt Data Double2700   /* MXVR ConfiguraP_CTL*/
#define                          AP                20   / 3 Enable Register */
#defiss Double Byte Register */
#defineCANCEL Contron Register    /* nterrupt Mask Register 1 */
#define                        CAN RESEN Contrer */
#definReseInterrupt Mask RegisteArbitster 1 et */
#define                  URBne    Registers */  /* Timer 10 Period Register */
#dEntry      1 Overwrite Protection Single Shync Data DMA6 Conter 1 */

/* CAN Controller 1 Config 2 Re   /* */
#define              RBc006RT_           SPI2_RDBR  2 */
#define        _MASKer 1 eivefeMC2  0xffc03240   /* CAN Controller 1 Mabox Intster */
#dguration Register 2 */
#defiCURR                      CTRS2  0xffc03248   /  /* CAN ontroive 2  0xffc03240   /* CAN Controller 1 MaCurrRIOD                       CAN1_TRS2  0xTfine                         Cransmit Request Reset  /* CAN Controller 1 Mailbox Direction Re                fine                        CAN1_TRS2  0xTfc03248   /* CAN Controller 1 ne                  t Register 2 */
#define                  e                 0xffc0324c   /* CAN Controller 1 TransmiCM         CAN1_MBIM1  0xffc03228   /* CANCML1  oller 1 Mailbox Ingister */
#define      #define                        CAN1_RFH1 Regisc0322c   /* CAN Con                      CAN1_RML2  0xffc0325c   /* CAN ContegistShot Trer */
#definion Register */
#define        g 2 Registers */

#define             c0326rs */

CAN Controller 1 Mailbox Transmit Interrupt Fla   /*ister 2 */
#define              egistertical Tranller 1 Mailbox Transmit Interrupt Fla#defiister 2 */
#define              a DMA6 Curefine                       CAN1_MBIM2  0xffcatioister 2 */
#define              egister */2720   /ler 1 Mailbox Transmit Interrupt Fladefinister 2 */
#define              ine              te Frame Handling Enable Register 2 *el Asister 2 */
#define              eceivc0101c   /* te Frame Handling Enable Register 2 *ta DMister 2 */
#define              ount */

/*N Controller 1 Clock/Interrupt/Counter Regi Counister 2 */
#define              _2  01024   /* EPPI0 FS1 Width Register  Interrupt Flaonfigister 2 */
#define              /
#deefine                  EPPI0_FS1P_AVPL  0xfg 2 Reine  nking Samples Per Line RegistBEC_6  0s Per Line Register */
#defi /* CAN Controller 1 Mgisters */

#define                              _MBRIF2  0xffc03264   /* CAN Controller 1 MMailbox Receive Interrupt Flag       egister _SYNC_LCHA2  0xffc03264   /* CAN Controller 1 Mc03268   /* CAN Controller 1 Ma      0_TFSDIV                      CAN1_GIS  0xffc03294   /*      CAN1_RFH2  0xffc0326c   /      egist0xffc02100               CAN1_GIS  0xffc03294   /*/
#define                            fer Start Addr Reg2  0xffc03264   /* CAN Controller 1 Mel Ass1_RMP2  0xffc03258   /* efine                         CAN1_M_CONTROL  0xffc  /* CAN Controller 1Global Interrupt Flag Register it Acknowledge Register 2 */
#define    _CONT03248   /* CAN Controller 1 TransInterrupt Pendt Register 2 */
#defiGlobal Interrupt Flag Register                  CAN1_RMP2  0xffc03258   /* Request Reset Register 2 */
#de/
#define           trol Register */
#define             troller 1 Transmit Acknowledge Register 2 */
#define    fine rupt Pending Register */
#define      /* CAN Controller 1 Abort Acknogrammable Warning Level Registe  0xffc0324c   /* CAN Controller 1 TransmiRRDfine                         CAN1_Mal Counter Regi  /* CAN Controller 1ER9_CONRead    CAN1_INTR  0xffc032a4   /* CAN Controller 1 al Co03248   /* CAN Controller 1 Transster */
#defint Register 2 */
#defi8   /* CAN Controll  0xffc0324c   /* CAN Controller 1 TransmiPAT_DATAx*/
#define                MATCHntrol_XVR S2 Receive Da 1 Receive Me Datagistergisters */

#define       gisters */
c0062fnel Assign               CAN1_AM00LMailbox Receive Interrupt gisters */
egist 0 Abase.h fo             CAN1_AM00Lc03268   /* CAN Controllergisters */
t Ser0xffc0ox Rec /* CAN Controller 1 Maicationon Register */

/* CAN Con                SPORT0_RFSDIV gister    

#def             /* CAN Controefine  AM00L        0xffc03300   /* CAN Controlleregistrs */
#define                       CAN1_AM01H  0#define                       CAegister 2 *#define                       CAN1_AM01H  0lbox 0 Acceptance Mask Low RegisegistRT_ADD#define                       CAN1_AM01H  0      CAN1_RFH2  0xffc0326e          egisterdefine                       CAN1_AM01H  0/
#define                 e          ine    define                       CAN1_AM01H  0ection Single Shot Transmie          2 */

/define                       CAN1_AM01H  0sters */

#define         e          e Regisdefine                       CAN1_AM01H  0Register */
#define       e        1ter */
nel Receive                   CAN1_AM11H  0xffc0330c   /* CAN Controller 1 Mai1box 1 AHigh Register */
#define                                     CAN1_AM02L  0xffc13310   High Register */
#define                   sk High Register */
#define        1       High Register */
#define                   roller 1 Mailbox 2 Acceptance Mask 1ow Regis Acceptance Mask High Re                  AM03L  0xffc03318   /* CAN Controll1r 1 Mailce Mask Low Register */
#define           fine                       CAN1_AM01H  0xffcce Mask Low Register */
#define           ptance Mask Low Register */
#define1        ce Mask Low Register */
#define           CAN Controller 1 Mailbox 4 Acceptan2e Mask Hit Mask Rew Register */
#define     21H  0xffc0330c   /* CAN Controller 1 Mai2Controllecceptance Mask High Register */
#define                    CAN1_AM02L  0xffc2AN1_AM05Lcceptance Mask High Register */
#define  sk High Register */
#define        2
#define cceptance Mask High Register */
#define  roller 1 Mailbox 2 Acceptance Mask 2Acceptancec03304   /* CAN Controister */
#define  AM03L  0xffc03318   /* CAN Controll2 /* CAN Colbox 8 Acceptance Mask Low Register */
#fine                       CAN1_AM02        CAlbox 8 Acceptance Mask Low Register */
#ptance Mask Low Register */
#define2ister */
#lbox 8 Acceptance Mask Low Register */
#CAN Controller 1 Mailbox 4 Acceptan3ilbox 7 Acc       cceptance Mask Low Registe31H  0xffc0330c   /* CAN Controller 1 Mai3fc0333c   /r 1 Mailbox 10 Acceptance Mask High Reg                  CAN1_AM02L  0xffc3           r 1 Mailbox 10 Acceptance Mask High Regsk High Register */
#define        3 High Regisr 1 Mailbox 10 Acceptance Mask High Regroller 1 Mailbox 2 Acceptance Mask 3ller 1 Mailb                    ance Mask High RegAM03L  0xffc03318   /* CAN Controll309L  0xffc03N Controller 1 Mailbox 11 Acceptance Mfine                       CAN1_AM03ne          N Controller 1 Mailbox 11 Acceptance Mptance Mask Low Register */
#define3ance Mask LoN Controller 1 Mailbox 11 Acceptance M Countfc03308   /* CAN Controller 1   /* Timer 9 Width RegisteHigh Register */
#define                       CAN1_AM01H  0xffc0330c   /* CAN Controller 1 Mailbox 1 Acceptance Mask Low Register */
#define                       CAN1_AM02L  0xffc03310   /* CAN Controller 1 Mailbox 2 Acceptance Mask High Register */
#define                       CAN1_AM02H  0xffc03314   /* CAN Controller 1 Mailbox 2 Acceptance Mask Low Register */
#define                       CAN1_AM03L  0xffc03318   /* CAN Controller 1 Mailbox 3 Acceptance Mask High Register */
#define                       CAN1_AM03H  0xffc0331c   /* CAN Controller 1 Mailbox 3 Acceptance Mask Low Register */
#define                       CAN1_AM04L  0xffc03320   /* CAN Controller 1 Mailbox 4 Acceptance Mask High Register */
#define                       CAN1_AM04H  0xffc03324   /* CAN Controller 1 Mailbox 4 Acceptance Mask Low Register */
#define                       CAN1_AM05L  0xffc03328   /* CAN Controller 1 Mailbox 5 Acceptance Mask High Register */
#define                       CAN1_AM05H  0xffc0332c   /* CAN Controller 1 Mailbox 5 Acceptance Mask Low Register */
#define                       CAN1_AM06L  0xffc03330   /* CAN Controller 1 Mailbox 6 Acceptance Mask High Register */
#define                       CAN1_AM06H  0xffc03334   /* CAN Controller 1 Mailbox 6 Acceptance Mask Low Register */
#define                       CAN1_AM07L  0xffc03338   /* CAN Controller 1 Mailbox 7 Acceptance Mask High Register */
#define                       CAN1_AM07H  0xffc0333c   /* CAN Controller 1 Mailbox 7 Acceptance Mask Low Register */
#define                       CAN1_AM08L  0xffc03340   /* CAN Controller 1 Mailbox 8 Acceptance Mask High Register */
#define                       CAN1_AM08H  0xffc03344   /* CAN Controller 1 Mailbox 8 Acceptance Mask Low Register */
#define                       CAN1_AM09L  0xffc03348   /* CAN Controller 1 Mailbox 9 Acceptance Mask High Register */
#define                       CAN1_AM09H  0xffc0334c   /* CAN Controller 1 Mailbox 9 Acceptance Mask Low Register */
#define                       CAN1_AM10L  0xffc03350   /* CAN Controller 1 Mailbox 10 Acceptance Mask High Register */
#define                       CAN1_AM10H  0xffc03354   /* CAN Controller 1 Mailbox 10 Acceptance Mask Low Register */
#define                       CAN1_AM11L  0xffc03358   /* CAN Controller 1 Mailbox 11 Acceptance Mask High Register */
#define                       CAN1_AM11H  0xffc0335c   /* CAN Controller 1 Mailbox 11 Acceptance Mask Low Register */
#define                       CAN1_AM12L  0xffc03360   /* CAN Controller 1 Mailbox 12 Acceptance Mask High Register */
#define                       CAN1_AM12H  0xffc03364   /* CAN Controller 1 Mailbox 12 Acceptance Mask Low Register */
#define                      FRAME_CNT#define                  MXVR_INT_STFCN     eive Data Bu */
#define                    CAN1_AM24H  0xffce Frame Delay Register */
#define   24 Acceptance Mask Low Register */
#define               ROUTING              SPORT0_RFSDIV  0xffcTX_CHXVR STWI FIFO Trae        * MXVR Intug Register */
#define      MUTE3cc   /*          SPOu  0xbox 25 Acceptance Mask Low Register *fc033ccc00623nel Assign ler 1 Mailbox 25 Acceptance Mask Low Register */
#defiync Data DMA6 Con        CAN1_AM26L  0xffc033d0   /* CAN ControlleregistilboLOCK  0xler 1 Mailbox 25 Acceptance Mask Low Register */
#defi   MXVR_DMA6_COUN        CAN1_AM26L  0xffc033d0   /* CAN Controllert Ser Mask ox Recler 1 Mailbox 25 Acceptance Mask Low Register */
#defia DMA6 Current Ad        CAN1_AM26LHigh Register */
#define          /* Timer 9 Width Register */
#033ccegist CAN Controller 1 Mailbox 25 /
#define                     /
#defi7_CONFIG  0xffc02        CAN1_/
#define                       033cc    Silbox 26 Acceptance Mask Highection Single Shot Transmit Re/
#defiddress */
#define        CAN1_ection Single Shot Transmit Regi033cceceiv Mask Low Register */
#definesters */

#define             /
#defiR  0xffc02830   /        CAN1_sters */

#define               033ccter **/
#define                   Register */
#define           /
#defiount */

/* MXVR         CAN1_define                      e       #define                     MXVR_033cc_2  0 CAN Controller 1 Mailbox 25 ng Register */
#define        /
#defifc0283c   /* MXVR        CAN1_ng Register */
#define          033cc/
#deilbox 26 Acceptance Mask Highebug Register */
#define      /
#defiCurrent Addr Regi        CAN1_ebug Register */
#define       oller    /* Cask Low Register */
#define Global Status Register */
#de             MXVR_APTB_CU        CAN1_ Global Status Register */
#defioller  1 Mailbodefine                   troller 1 Error Counter Regist                     MXVR              Configuration Register 2 */
e       cation Table Register 8 */
#defioller tance MAN Controller 1 Mailbox 25 * CAN Controller 1 Global Inte        ister */
#define               c03268   /* CAN Controller 1 Maoller ster */
#ATA2  0xffc03408   /* CAN       CAN1_RFH2  0xffc0326c          RT_ADDR  0xffc028                    CAN1_RFH2  0xffc0326c   /oller 3e0   /tance Mask Low Register */

/
#define                            Message TX Buffer              /
#define                      oller ller 1 Maoller 1 Mailbox 0 Data 0 Rection Single Shot Transmit R        fer Start Addr Re              ine                     CAN1e       define                MXVR_SYNC_oller ceptanc_DATA2  0xffc03408   /* CAN sters */

#define                             MXVR_PAT              sters */

#define              oller egister *ATA2  0xffc03408   /* CAN Register */
#define                  e Register 0 */
#              Register */
#define            oller fc033f4tance Mask Low Register */

ng Register */
#define               ffc02874   /* MXV              ng Register */
#define         oller troller 1oller 1 Mailbox 0 Data 0 Rebug Register */
#define             ame Counter 0 */
              ine   _ID1  0xffc0341c   /* CAN Contel Assign Register 5 */
#define Accept   /* CAN Controller 1 Mailbox 25 2/* CAN Controller 1 Mailbox Data Reggn Register 6 */
#        CAN1_                    CAN1_MB01_ 1 Mailb 1 Mailbox 26 Acceptance Mask High2egister */
#define                   */

#define      Controller 1 MMailbox Receive Interrupt Flag Accepttance Mask Low Register */
#define2Controller 1 Mailbox 0 Data 2 Regist279c   /* MXVR SynController 1 MAN1_MB00_DATA3  0xffc0340c   /* CAN   MXVR            CAN1_MB01_LENGTH 2er */
#define                 CAN1_Mefine             Controller 1 ML  0xffc03308   /* CAN Contre       ta DMA0 Current Address */
#defix 2 Da3e0   /* CAN Controller 1 Mailbox 2_TIMESTAMP  0xffc03414   /* CAN Cont Count */

/* MXVRController 1 Mister */
#define                    1_CONF          CAN1_MB01_ID1  0xffcontroller 1 Mailbox 0 ID0 Register *     MXVR_DMA1_STAController 1 Mection Single Shot Transmit Regength ceptance Mask High Register */
#de2A0  0xffc03420   /* CAN Controller 1ta DMA1 Loop CountController 1 Mefine                  CAN1_MB01_DATR  0xfRegister */
#define            Mailbox 1 Data 1 Register */
#definVR_DMA1_CURR_COUNTController 1 Mptance Mask High Register */
#define Count */

/* MXVR DMA2 Register /* CAfc033f4   /* CAN Controller 1 Mail2e                  CAN1_MB01_DATA3  onfig Register */
Controller 1 M Mailbox 1 Data 3 Register */
#defin   /*           CAN1_MB01_ID1  0xffc0xffc03430   /* CAN Controller 1 MaiUNT  0xffc027c8   Controller 1 M033fc   /* CAN Controller 1 Mailbox e     ox 1 Data 2 Register */
#defi3/* CAN Controller 1 Mailbox Data RegAddress */
#define        CAN1_LENGTH  0xffc03470   /* CAN Coster */
CAN Controller 1 Mailbox 0 Data 0 3egister */
#define                               MXVR_th Register */ CAN Controller 1 Mailbox 0 Data 1 Ronfig Register */
#define       er */
N1_MB00_DATA2  0xffc03408   /* CAN3Controller 1 Mailbox 0 Data 2 Registine               th Register */AN1_MB00_DATA3  0xffc0340c   /* CAN MA3 Loilbox 26 Acceptance Mask High3er */
#define                 CAN1_M  /* MXVR Sync Datth Register */Controller 1 Mailbox 0 Length RegistMA3_CUne                 CAN1_MB03_L_TIMESTAMP  0xffc03414   /* CAN ContVR DMA4 Registers th Register */ister */
#define                    27e8   0xffc03474   /* CAN Controlleontroller 1 Mailbox 0 ID0 Register *TART_ADDR  0xffc02th Register */1_MB00_ID1  0xffc0341c   /* CAN Contine                  MXVR_DMA4_Ce     fine                  CAN1_MB01_DA3A0  0xffc03420   /* CAN Controller 1 MXVR_DMA4_CURR_ADth Register */efine                  CAN1_MB01_DATess */480   /* CAN Controller 1 Mail Mailbox 1 Data 1 Register */
#definDMA4 Current Loop th Register */A2  0xffc03428   /* CAN Controller 1      ne                 CAN1_MB03_Le                  CAN1_MB01_DATA3  /
#define         th Register */ Mailbox 1 Data 3 Register */
#definData D 0xffc03474   /* CAN Controlle0xffc03430   /* CAN Controller 1 Mai  /* MXVR Sync Datth Register */e              CAN1_MB01_TIMESTAMP   MXVR_DMA5_CURR_ADDR  0xffc02808 ffc033   /* CAN Controller 1 Mailbox 25 4/* CAN Controller 1 Mailbox Data Reg 0xffc0280c   /* M            CA
#define              CAN1_MB03_TIMEA6 Regilbox 26 Acceptance Mask High4egister */
#define                  Sync Data DMA6 Con            CA           CAN1_MB02_DATA0  0xffc034ADDR  ox 1 Data 2 Register */
#defi4Controller 1 Mailbox 0 Data 2 Regist    MXVR_DMA6_COUN            CAAN1_MB00_DATA3  0xffc0340c   /* CAN gister            CAN1_MB01_LENGTH 4er */
#define                 CAN1_Mta DMA6 Current Ad            CAata 2 Register */
#define            0xffc02820   /* MXVR Sync Data D CAN C3e0   /* CAN Controller 1 Mailbox 2_TIMESTAMP  0xffc03414   /* CAN ContA7_CONFIG  0xffc02            CAister */
#define                    ine   
#define                 CAN1_ontroller 1 Mailbox 0 ID0 Register *Address */
#define            CAamp Register */
#define             XVR SySTAMP  0xffc034b4   /* CAN ConA0  0xffc03420   /* CAN Controller 1DR  0xffc02830   /            CAefine                  CAN1_MB01_DAT      ller 1 Mailbox 5 ID0 Register  Mailbox 1 Data 1 Register */
#definCount */

/* MXVR             CAptance Mask High Register */
#define       MXVR_AP_CTL  0xffc02838   GTH  0fc033f4   /* CAN Controller 1 Mail4e                  CAN1_MB01_DATA3  ffc0283c   /* MXVR            CA Mailbox 1 Data 3 Register */
#definne    
#define                 CAN1_0xffc03430   /* CAN Controller 1 Mai Current Addr Regi            CA033fc   /* CAN Controller 1 Mailbox ffc028ox 1 Data 2 Register */
#defi5/* CAN Controller 1 Mailbox Data Reg      MXVR_APTB_CU9L  0xffc033e8
#define              CAN1_MB03_TIME Curre            CAN1_MB01_LENGTH 5egister */
#define                                MXVR9L  0xffc033e8 CAN Controller 1 Mailbox 0 Data 1 Rrol Register */
#define                N1_MB00_DATA2  0xffc03408   /* CAN5Controller 1 Mailbox 0 Data 2 Registgister */
#define 9L  0xffc033e8AN1_MB00_DATA3  0xffc0340c   /* CAN VR Con1 Mailbox 28 Acceptance Mask Ler */
#define                 CAN1_MART_ADDR  0xffc0289L  0xffc033e8Controller 1 Mailbox 0 Length Registgisteregister */
#define            _TIMESTAMP  0xffc03414   /* CAN Cont Message TX Buffer9L  0xffc033e8ister */
#define                    /

#deCAN1_MB07_DATA2  0xffc034e8   ontroller 1 Mailbox 0 ID0 Register *ffer Start Addr Re9L  0xffc033e81_MB00_ID1  0xffc0341c   /* CAN Cont  0xffc02864   /* MXVR Remote Reaister fine                  CAN1_MB01_DA5A0  0xffc03420   /* CAN Controller 1          MXVR_PAT9L  0xffc033e8efine                  CAN1_MB01_DAT0 */
#1 Mailbox 28 Acceptance Mask L Mailbox 1 Data 1 Register */
#definle Register 0 */
#9L  0xffc033e8A2  0xffc03428   /* CAN Controller 1  /* Megister */
#define            e                  CAN1_MB01_DATA3  xffc02874   /* MXV9L  0xffc033e8 Mailbox 1 Data 3 Register */
#definr RegiCAN1_MB07_DATA2  0xffc034e8   0xffc03430   /* CAN Controller 1 Mairame Counter 0 */
9L  0xffc033e8e              CAN1_MB01_TIMBLOCK0xff Frame Delay Register */
#define  B24 Acceptance Mask ER9_COUNTERne                     CAN1_LK         CAN1_MBIM1  0xffc03228   MXTAL/ EPPI0            TH   Crystal Oscillator "defBFefine                      SPORT0_SDATA0r */
#de3520   /* CAN Controller 1 Mailbox Feedbaata 0 Register */
#define                  MUthe Gting T  /* CAN ControllerMultiplieITIONS FOR ADSP-BF549 */

/* ICLKX3SEthe G         MXVR* LicGenister 1 Sourceontroller 1 Mailbox Transmit InterrupMMCLKfine              Ma Conf9 Data 0 Register */
#define              trol       1e240c   /* SP 3 Registerfc03528        Facbox ug Register */
#define       PLLSMPimer eegisters */
N Coor tSt0270MaMesse    scal  /* CAN Controller 1 Mailbox 9 MBroller 1 Mai2 (or lateMail Data 0 Register */
#define              er *DIVH  0xffcLOCK  0x          Diviiste Length Register */
#define          INVRXfc02874   /* MXVInverod RegisteCAN1_RMP1  0xffc03218   /* CAN ControlRT2_IERroller 1 Maievices    r 2 */
#define                     SPORFSroller 1 Mailox RecB10_DATA0  er */
#define                    CAN1_MB09_IMFS 2 Regi6egister */
B10_DATA0  1_MB09_DATA3  0xffc0352c   /* CAN ConFSRAMEme Counter 0 */
1 Mailbox 10er 10 Ciz        Lost Reg                   CAN1_DRPLL         CAN1_MBIM1  0xffc03228   /CDRSM 0xffc03520   /* CAN Coner */c03534   /* CANzontal Blanking Samples Per Line RegiDRRSTB1_MB09_DATA1  0xffc035ontrolN1_OPS 3 Register */
#define          SVCO              CA CAN Controllerrt ngth 3 Register */
#define          MODrationister */
#define       CDR Msmitntroller 1 Mailbox Transmit Interrup0 Le24 Acce3filbox 9 Datfine             54x_base */
#define                    CL24 Accepc09_TIMESTAMP  00xffc03 Inc.roller 1 Mailbox 10 ID0 Register */0 LeHP 2 Regiox 1 Data 2 3558   /* CANhaanne  CAN1_MBRIF1  0xffc03224   /* CAN Cilboxc01020   /* EPPI0 
#define           efine                      SPORT0_SCDRCx 10 ID1fine        ailbox 10 Timharge Pumpl Countertroller 1 Mailbox 10 Data 2 RegisFM */
#define                  CAN1_MB10 FMTA3  0xffc0354c   /* CAN      ller 1 Mailbox 10 Data 3 Register */
#define        FM       CAN1_MB10_LENGTH  lbox 1550   /* CAN Controller 1 Mailbox 168  ngth Register */
#definelbox 11 D    CAN1_MB10_TIMESTAMP  0xffc03554 68  N1_MB10_ID0  0xffc03558    CAN1_MB11roller 1 Mailbox 10 ID0 Register */
# FMine                    CAlbox 1_ID1  0xffc0355c   /* CAN Controller 1 MAMP _MB11_DATA1  0xffc03564   lbox 1Controller 1 Mailbox 11 Data 1 Register */
#define   PINdefine                  CAN1_MB09_DTffc0O     03520   /* CAN1_MBRegin DraiController 1 Mailbox Transmit Interrup   /* G1_MB09_DATA1  0xf  /* CGatesAN1_Register */
#define                       Oisterister */
#deMFS& MMpu#define                      SPORT0_SMFSGx 10 ID1 0xffc00840  FS       l Purposedefine  e Lost Register 1 */
#define       ffc03D      fc0331c   /*ller 1 Mailbox 12 Data 1 ReMailbox           CAN1_MB01_TIMS/
#dene                    CAN1_MB08_ID1 ler 1 Maeive Data Bunclude "defBF54x_box 12 Data 2 Register KPAD         CAN1_MBIM1  0xffc03228   /
#defc   /* SPORT0 ReceKeypadgister */
#define               
#defIRQ* CAN Co Routi/* CAN CoN Consgister */
#define                   TWI1_XMT
#defROWc   /*            Rowgister *Width3594   /* CAN Controller 1 MailbCO /* SPOMB09_TIMESTAColumn */
#define      Data 3 Register */
#defPRESCALE*/
#define           define       _VA0 ID1 R         CAN1_MB12trolfc02uADDR  0xffc027a4   /* M
#defM 2 R /* Timer 9 Width Register DBON_      define         DebounCAN1ailbox 12 ID1 ed under the ADI BSD COLDRV035a0   /* C   /* CAN ControlDriv    ailbox 12 ID1 Register */
#define    ROWCO      CAN1_MBIM1  0xffc03228   3 Data 1  /* CAN ControlRows_MB12_ines that are common to all ADS_ID0  0x    CAN1_MB13_DATA1  0035a8   /* CARegister */
#define    0061          CAN1_MB13_DATA0  0ster */
#dfc03590   /* CAN Controster */
#d0353u*/
#define                e     a 1 Reg              Cfc03528exffc/Control ConB12_T        CAN1_MB13_LENGTH  0xffc035b0e   SE                CAN1_ter */c 0xffc0s       CA  CAN1_MB13_DATA3  0xffcOFTE59c  /* Timer 9 Width Regi */
#define  _   /*09_DATA1  0xSoftwarAN Cogrammine  Fo CANEvalu027013 Timestamp RegisterSDH_COMMANDac   /* CAN Controller 1 Mailb CMD_IDxffc0* CAN ControCommanine deler 13_ID1  0xffc035bc   /* CAN ConRS                 Respon/* MXVR Interrupt Status Regis ConL1_MB14_D            Long035c0   /* CAN Controller 1 Mailbox 14 Da     Mask High Regist3 ID1 Regier */
#d */
#define                ConPENDfc035b8AN Controller 1 MaiPenMER9_                  EPPI0_FS1W_HBL        05L  0xffc033 ID1 Re#define                         PWR          CAN1_MB11_DATA2  0xffc0356A3     /* T#define     w RegOller 1if 0me Delay Register */
#define   TB     3cAN ControllBN1_MB#ffc0f/
#define                SD_ 14 11_ID1ATA0  0xffc0AN Controllefine  nable Register 1 */
#define   Oefine egister */
#defRod         /
#define                 
#define                  CAN1_MB09_ 9 Daroller 2 Receive DaMC    e    sNITIONS FOR ADSP-BF549 */

/* Incl      /* CAN Controllbox 14 Bus        CAN1_MB09_ID0  0xffc03538   /* CANA3  SV        CAN1_MB14_er */
Savx 10 Data 3 Register */
#define  CAN C_BYPASimer 8 C        CBypass ID0 Register */
#define             WIDE_BUimer ne          W*/
#c   amp Re                  CAN1_MB14_DATRESP */
ac   /* CAN Controller 1 Mailbister */
ler 1 Mailbox 135c0   /*3 ID1 Re            CAN1_MB14_DATs */
     CAN1_MB14_ID0  0xffc035d8   /* DTX CAN1_M            AN1_e    rollerLCR  0xffc0210c   /* Line Control RTX_DI    Mon Register *  /* CAN Cont       CAN1_TRR1  0xffc0320c   /* CAN */
#* CAN Co /* CAN Cont    CAN1_MB15_amp R */
#define                */
#dMA CAN1_c035b4   /*     CAN1_MB15_L    oller 1 Mailbox 15 Data 3 Regi*/
#BLK_LGTH  /*    /* Timer   /* CAN Contfiguratengbox 12 ID0 Register */
#    0061US          CAN1_MB13_DATA0  14 CRC_FAITH  0x0322c   /* CMD_TCR2Faiknowledge Register 1 */
#dDAT           CA                Cfc035fc   /* CAN Controller 1 Mai     TIME_OU12_DAT1_MB15_ID1  0xfTimData             CAN1_MB15_TIMElbox
#define    f4   /* CAN Contro1_MB16_DATA0  0xffc03600   /* CANTX_UN RegU083c   /* SPORT0 e        Undr */ller 1 Overwrite Protection RX_OV_MB16_DATilbox 3 AcceCAN Cont    ntroller 1 Mailbox 16 Data 1CAN1_sterENefine             0xf35c0   /*Eoller define                  CAMD_SE4 Acce         MXVRMDPERIOD  0xffc00608   /* Timer 8 Perlboxntrolleer */

/* SPOAN1_ister */
#define           ne    BIT_ackfin.  CAN1_MB14_1_MB11    EFINITIONS FOR ADSP-BF549 */

/lbox    ntroller ter */
#define er)
 *ister */
#define                   CAACMB16_DARegister */MDtus RegiCAN1_AM31L  0xffc033f8   /* CAN CMailboxance Mask Loe        gister */
#define                    RAN1_MB16_N ControllerCAN Contgister */
#define              TX_49 tfc035a Transmit Regise        49 th        CAN1_MB13_LENGTH  0xffcR/* CAN Controll*/
#define  CAN Contegister */
#define                   /* CAN FULTH  0xne          16 ID1 RegistFulknowledge Register 1 */
#dCAN1_MB1#define c   /* CAN C  /* CAN Cont_DATA1  0xffc03624   /* CAN C /* CAN ZERth Reg                CAN1_MB17EmptI2 Control Register */
#defRXegisDATA2  0xRegister */
  /* CAN Conter 1 Mailbox 17 Data 2 RegisterN1_M
#deRD    Sh>


/* SYSTe        fine Availine                      SPORTCAN1_MB1Mailboxxffc03348   oller 1 Mailbfine          Controller 1 Mailbox 15 ID0_CL        SPI2_RDBR  0xf             ControllN1_MB15_ID1  0xffc035fc          CAN1_MB13_LENGTH  lbox 15 ID1  Controllster */

/* CAN Controller        CAN1_MB13_LENGTH  0 */

#deOUfc0061                  CAN1_MB16_D        CAN1_MB13_LENGTH  0 Control ID0 Registeilbox 16 Data 0 Register *ailbox 13 ed under the ADI   CAN1_MB164   /* CAN1  0xffc03604   /* CAN Contro        CAN1_MB13_LENGTH  0xegister */          ine                  CAN1_M        CAN1_MB13_LENGTH  /* CAN Contr Controller1 Mailbox 16 Data 2 Registe        CAN1_MB13_LENGTH  0xff  CAN1_M17_DATA0  0TA3  0xffc0360c   /        CAN1_MB13_LENGTH  0xffc Data 3_MB18_DATA0ter */
#define             CAN1_MB13_LENGTH ENGTH  0xffc0define      /* CAN Controller 1 Mailb       CAN1_MB17_ID1  0xffc0363ine     Controller    CAN1_MB16_TIMESTAMP          CA Controller 1 Mailbox  /*              SPORT0_RFP  0xffc03634 /* CAN C1_MB15_ID1  0xffc035fc  xffctamp Register */
#define          /* CAN ster */

/* CAN Controller18 Timestamp Register */ailbox 17 ID0 /* CAN               CAN1_MB16_Dc03658   /* CAN Controlle0363c   /* C /* CAN ilbox 16 Data 0 Register *c03658   /* CAN Controlle       CAN1_ /* CAN C1  0xffc03604   /* CAN Controc03658   /* CAN Controllegister */
#d         ine                  CAN1_M18 Timestamp Register */ntroller 1 Ma18 ID0 ReData 1 Register */
#define  CAN Controller 1 Mailbox 1TA2  0xffc0c   /* CA CAN Controller 1 Mar */
#define                efine         CAN1_ter */
#define     18 Timestamp Register *troller 1 Mailgister */
 /* CAN Controller 1 Mailb             CAN1_MB18_ID1  0 0xffc03ontroller AN Controller 1 Mailbox 1 Controller 1 Mailbox 19 Datar 1 MaiB19_DATA2  6 Timestamp Register  Controller 1 Mailbox 19 Data CAN1_Mster */
#def  0xffc03618   /* CAN Con_MB19_TIMESTAMP  0xffc03674   ter */CAN Controlle                    CAN18 Timestamp Register */ /* CAN ContN1_MB19_LENG1 Mailbox 16 ID1 Register */
#18 Timestamp Register */CAN1_MB17_DAter */
#defiffc03620   /* CAN Controller Controller 1 Mailbox 19 ID0 Regi#def Controller 1             CAN1_MB17_DATAD1  0xffc0367c   /* CAN Controllfc03680   /* box 17 Data 1 Register */
#defController 1 Mailbox 19 ID0 RegiATA2 */
#define  3628   /* CAN Controller 1 Mc03658   /* CAN Controlle */
#define Mailbox 19 I          CAN1_MB17_DATA3  CAN Controller 1 Mailbox 1ller 1 Mai680   /* CAN Cata 3 Register */
#define      B20_DATA2  0xffc03688   /* C 0xffc03 CAN1_MB19_ID CAN Controller 1 Mailbox 17 Le18 Times Controller 1 Mailbox  CAN                   CAN1_MB12_DATngth RontrVR Synance Mask L9 thilbox 12 Data 3 Register */    E 15 ID0 Register */
#define      SDIO35c4 DE        CAN1_MB17    ilbo Detec Configuration Register */
#der *ARD        1_MB19_DATA0SD Cardxffc036/* CAN Controller 1 Mailbox  /* C /* Timer 9 Width Register */     M             CAN1_18 Ti0_ID0  0xffc03698   /* CAN Controller 1 Mai  0xff    roller 1 Mailbox 18 Tie                    CAN1_MB20_ID1  0CF */
#define                     CLKS0xffc03590   /* CAN     sController 1 Transmit Request Set Regis SD4box 15 Length Reg0_ID04-er 1 lbox 21 Data 1 Register */
#define     MWfc035f4   /* CAN MovER9_Winder */
#def Register */
#define          SN1_M0 Register */
#defiMMCController 1 Mailbox 11 Data 3 RegPUP_SDMB12_DAilbox 3 Accepull-upc036MB12r */
#define              fine     t Serfc0331c   /* MB21_LENGTH        CAN1_RFH2  0xffc0326c  PD 1 Mailbox              MB21down*/
#define   Mailbox 15 Data 1 RegiD_WA0xff          TWI1_XMT_DATA16  0xffRegister* CAN ControllerCAN CWaigistques           CAN1_MB20_IDATAPIsterTRRegister */
#define            PIN ConR/* CAN Controller20   /*IO/Reg Op0xffc036b0   /* CAN ControMULTI1  0xffc03    CAN1_MB20_MB11fc035-EM & ailbox 21 ID1 Register */
#ULTRA1  0xffc03      CAN1_MB_MB11Ultra0  0xffc036c0   /* CAN Controller   XFER#define c035b4   /* AN1_MB15_LENGTH  0xffc035f0   /* CAN ControllerIO    0083c   /* SPORT0       0xffc036a8   /* CAN Controller 1       LUS     /
#define   Flushler 1*/
#define                    defi6ac   /*r 1 Mailbox ontrController 1 Mailbox 11 Data 3 RegisDEV6ac   /*  0xffc03668Devic* CA0   /* CAN Controller 1 MailboTFRxffcac   /* CAN Controle    694   /    CAN1_MB22_LENGTH  0xffc036dfc03ON_TER1 Receoller 1 MaiEnd/Termic0270e Lost Register 1 */
#define    _ID1USE  0x1_AM05L  0xffc033IO0  0x0xffc036a8   /* CAN ControlUDMAINength THRimer            C#defiMail-INffc036Thresholller 1 Mailbox 15 Data er */
15 ID0 Register */
#define      mesta 0xff   /* 
#define     IO tCAN Contin p1 Mar */
#define               definetroller 1 M09_DATA1  0xfDATA0wo    MAegister */
#define                  CAN1_MB23 1 Maitroller 1 M            Mailbox 2egister */
#define                  CAN1_MB23        036d Regis CAN Controc036e4   /Inne  fc036Levcknowline                    Cgist           SPI2_RDBR  0xffc02410  lbox 23 DAN Cofine                                    CAN1_Ter */
xffcfc0369c   /* CAN Control 1 Mailboxilbox 23 TRS1  0xffc03208      ibox 14 DaTIME0xffc036b0   /* CAN Cont_ID1efin                CAN1_1 Register */dCONF 0xffc036f0   /* CAN Controller 1 Madefine23 Length Reggister */
#deDATA0  0x              CAN1_MB23_TIMESTAMP  0xffc036f4   /xffc03623 Length Regc035b4   /* #define   inegister */
#define                    CAN1_MB23_Ixffc ID023 Length Reg1_MB19_DATA0#define   outegister */
#define                    CAN1_MB23HOSTAN1_M   CANgister */
#define   Host t36d4   /*ller 1 Mgister */
#N1_MB23_TIMESTAMP  0xffc036f4   /* CAN CAN1_Montroller 1 Mailbox LENGTH  Controllemestamp Register */ine                    CAN1_MB23_ID0  0xffCAN1_MB24_DATdefine              Controlle#define  -x 23 ID0 Reg */
#define                    CAN1_MB23_ICAN1_MB24_DATdefine       708   /* CAN Controller 1 1 Mailbox 23 ta 0 Register */
#  /* CAN Controller 1 Mailbo15 ID0 Register */
#define     #define            CAN1_MB23_LENGTH  0xffc036fister */
#define           Mailbox 23 Ler 1 Maiister */
#define              CAN1_MB23_efine              CAN1_MB24_ CAN Contror 1 Maiailbox 23 Timestamp Register */
#define       efine              CAN1_MB240  0xffc036fr 1 MaiAN Controller 1 Mailbox 23 ID0 Register */
#definister */
#define           1_MB23_ID1  0r 1 Mail   /* CAN Controller 1 Mailbox 23 ID1 Register */ister */
#define             CAN1_MB24_D0xffc037fc03700   /* CAN Controller 1 Mailbox 24 Data 0 Regisestamp Register */
#define         AN1_M      CA1  0xffc03704   /* CAN Controller 1 Mailbox 24 Data 1 Reg Mailbox 24 ID0 Register */
#defineegister */
#ATA2  0xffc03708   /* CAN Controller 1 Mailbox 24 Data 2 Regiller 1 Mailbox 24 ID1 Register */
#egister */
#TA3  0xffc0370c   /* CAN Controller 1 Mailbox 24 Data 3 Registailbox 13 Timestamp Registerer */
LINx 20 Timestamp Register */
#defineAN ContrINT  0xffc036b8   /*LENGTH  0xffc036fto h CANl CANefine              CAN1_MB24_TIAN ControAMB14_Don Register *      daspailbox 25 Timestamp Register */
#define              CS0_MB23_DATA1  0xffer */ chip s Lost 0N Controller 1 Mailbox 25 ID0 Register */
#defi1ne                   CAN1_MB25_ID1 15 Timestamp Register */
#define                      7 Buffer Regi   CAaster */ Timestamp Register */
#define       1 MailMAREata 3e Buffer Regi   CA   /e set 25 Timestamp Register */
#define     _DATA1  0ACKine              Controlleracknowledge5 Timestamp Register */
#define        1 MailIOWr 3 */
#define       CAwIG  0 Controller 1 Mailbox 26 Data 2 Register */
#dRefine                CArAN C Controller 1 Mailbox 26 Data 2 Register */B22_DA       TWI1_SLAV   CAB22_DA Timestamp Regiine                    CAMMB24_              CAN1_MB1MB24_TIMESTC      N Coster */
#definemsmitailbe m /* CANller 1 Mailb          CAN1_MB21_DATA3  0 0xf   /* CAN CTransmit Requeslbox 26 Timestamp Register */
#define                   xffc036B26_ID0  0xfAN ControlMailbox 22 nCAN Controller 1 Mailbox 26 ID0 Register */
#define     1_MB23_I       CAN1_MAN1_MC2  0oller 1 Mailbox 26 Length Register */
#define     N1_MIN    CAN1_MB26_TIMESTAMP  0er */
    CAN1_      CAN1_MB23_* CAN Controlio CAN C 1 Mailbox 25 Length RegisteREGntro                 CAN1_AM25H  0xffc02gist  /* CAN Controlisteof cycle t1_MB_WIDrnel Confa   TIlbox 24 D*/
#define                    TEOC_DATA2  0x1_MB27_DATtrolles Mail/
#de pulsewilbox 12 ID0 Register */
#er */
_ID1r */
#define                  CAN1_MB21_DATA2  0 /* CAN Contller romegister *vc0228to* CAN ContrN1_MB22_LENGTH  0xffc036d0 7_DAT
#de7_DATA34   /* CANCAN Controller 1 Mailbo1_AM31L  0xffc033f8   /* CAN4_DATA2  0IV  0xffc008ontrdAN1_ */
#define                    C#define    /* Timer 9 Width RegisteCAN1_MB2B27_TIMESTffc03768   /* CAN Controller 1 M27_T Data 2 Register.ilbox 27 Data 3 Register */
ta 1 Re */
#define                  CAN1_MB4   /ATA2  CAN ControllCAN Contrasser Con CAN Controller 1 Mailbox 27 Timestamp Re4   /MB27_TA3  0xffc03ontroller 1 Mailbox 27 Length Register *Controller 1 Mailbox 27 ID1 Registe Frame Delay Register */
#define   TKN1_MB13_DATA2  0x76c   /* CAW negratioller 1 Mailbox 28 Data 0 Register */
#defineTK    MXV3  0xffc0376c   /* CANController 1 Mailbox 28Controller 1 Mailbox 27 ID1 Regist#define                     MXVR_ALL        2  0xffc03788   /* CAN C          CANed under the ADI BSD license CAN1            CAN1_MB28_DAe/* CAN Contr_WID   / Controller 1 Mailbox 28 Da 1 Maigister */
#define                  CAN1TACgister   0xffc03788   /* setup 1 Re */
#olleR9_WID     r 1 Mailbox 28 Length Register */
NControl             CAN1_MBveloptroller03794   /* CAN Controller 1 Mailbox 2e Frame Delay Register */
#define  TDVCAN Co       CAN1_MB28_ID     x 27 L0  0xf_ID1  0xed under the ADI BSD licTCYC_          3  0xffc0376c   /* Controller-      _ID1  0xffc0379c   /* CAN Controller 1 Mai#define                     MXVR_ALLTer 1 M      CAN1_MB29_DATAoller ler STROBE * CANtoControtrolof0xffRQ orCAN Conntrollet Flller 1 Mailbox 28 ID0 Register */
MLIster */
#define         ta 0 
#ifndID1  0xffc0379c   /* CAN Controller 1 Maication Table Register 8 */
#define TZA28_LENGTH  0xffc03790   /min* SPOd0844 e seiredMP  0oTIMESTAMP  0xffc035d4   /* CAN READY_PAUS     CAN1_MB13_DA76c   /**/
#y/* Cpau_ADDR  0xffc027a4   /* M
#deR_ENABLE                   CAN1_AM28L  0xfIMEN    CA   /* CAN Contrr 8 0xffc036a8   /* CAN Controller 1 Mail  CAN      _ID0  0xffc037b8 9 0xffc036a8   /* CAN Controller 1 Mai  CAN       _ID0  0xffc037b8 10ontroller 1 Mailbox 15 Data 1mestamDISRegister */
#define                 TIMDIS1_MB29_ID0  0xffc037b8   Dis     CAN1_MB29_ID1  0xffc037bc   /* CDISr */
#define              0 Register */
#define                 CAN Controller 1 Mailbox 29 ID 0 Register */
#define             15 ID0ster */
#define                    CIC_2  09_ID0  0xffc037b8   lbox 14 Data 1 Register */
#define  ata 2 Regr */
#define                      CAN1_MB30_DATA3  0xffc037cc   2 Reg Controller 1 Mailbox 29 ID         CAN1_MB30_DATA3  0xffc037cc TOVFffc0    CANID0  0xffc037b8   54x_base           TIMER8_COUNTER  0xffc0060gth Regi       efine                 CAN1_MB30_TIMESTAMP  0xffc037d4   /* CAgth Regi        ler 1 Mailbox 29 ID    CAN1_MB30_TIMESTAMP  0xffc037d4   /* CANN1_MBRUN1_MB29e           37b8   SlCAN1_MB15_D        CAN1_MB13_LENGTH  0xffcfine               Cine             1  0xffc037dc   /* CAN Controller 1 Mailbox 30 ID1              r 1 Mailbox 29 ID1  0xffc037dc   /* CAN           CAN1_MB0EPPI0 llerobtain29 Ller common b    header_MB31_DATx (_DAT1ffc03_DAT2)  0xffc0379c   /* CAN USB_F           SPI2_RDBR  0xffFUNC    ller Eer 1 Mffc02778   /Funt Statgister *#define                  POWE        SPI2_RDBR  0xffcp Regi_SUS    MB27_DATA1  0xffcec037dc uspendMgth Register */
#define         * CAN CMailbox 1    CAN1_MB20ta 3 Rine   indMailngth Register */
#define    RESUMEMailbox 15 Length RegiMAine              CAN1_MB15_TIMESTne                   CAN1    CAontroller 1 Mailbox 31 Length Regis5 DatSMailbox 1*/
#define  85c  SpeedCAN Coimestamp Register */
#define         Ha4  Regier 1 Mailbox 30 h0xffc037f80xffc036a8   /* CAN Controller 1 MB22_DCONORT0_MTCS1  0xffc/* CAcrrupt ller 1 Mailbox 28 ID0 RegisISO_U_MB1ration     CAN1_MBso 10 Countuguratr */
#define                0xTXfine                    CAN1_MB2EP0_Txffc09_ID0  0xffc0x  /*poin1  0ta 0 Regisr 1 Mailbox 28 Length RegisterEP1 ATAPI_#define      03804   /* 1TAPI Status Register */
#define            2 ATAPI_oller 1 Mailb03804   /* 2TAPI Status Register */
#define            3 ATAPI_  /* CAN Cont03804   /* 3TAPI Status Register */
#define            4 ATAPI_Sffc037d8   /03804   /* 4TAPI Status Register */
#define            5      ATRegister Read Data */
#5TAPI Status Register */
#define            6ne      Register Read Data */
#6TAPI Status Register */
#define            7rite DatRegister Read Data */
#7TAPI Status Re3800   /* ATAPI Control ReRister */
#define                   1_0xffc0#define     R  0xffc03808   /* ATAPI Device Register Address */
#defin0xffc0          CANATAPI_DEV_TXBUF  0xffc0380c   /* ATAPI Device Register Wr0xffc03fc03820   /* ATAPI Line       ATAPI_DEV_RXBUF  0xffc03810   /* ATAPI 0xffc0 Register Re  /* ATAPI Sdefine                   ATAPI_INT_MASK  0xffcUS  0xfATAPI_PIO_TFRCNT  0xffck Register */
#define                 ATAPI_INSTATE  ATAPI_PIO_TFRCNT  0xffcI Interrupt Status Register */
#define        0xffc035TAPI_PIO_TFRCNT  0xffcfc0381c   /* ATAPI Length of Transfer */
TX  CAN1_MB26_TIMESTAMP  0xffc0      AT_DATA3  0xffc035ec804   /* ATAPI Status0xffc036a8   /* CAN Controller 1 Ma      fc035b8   /* CAN CR  0xffc03808   /* ATAPIEG_TIM_0  0xffc03840   /* ATAPI Registine  Data 2            ATAPI_DEV_TXBUF  0xffc0EG_TIM_0  0xffc03840   /* ATAPI RegistWritffc035f4   /* CAN e                  ATAPIEG_TIM_0  0xffc03840   /* ATAPI RegistI De CAN1_MBegister Read Data */
#define      EG_TIM_0  0xffc03840   /* ATAPI Registfc03        /* ATAPI Interrupt Mask Register *EG_TIM_0  0xffc03840   /* ATAPI RegistINT_ Data 2   0xffc03818   /* ATAPI Interrupt EG_TIM_0  0xffc03840   /* ATAPI Regist    ffc035f    ATAPI_XFER_LEN  0xffc0381c   /*ontroller 1 Mailbox 15 Data 1sfer */
#d03838   /* ATAPI UDMAOUT transferATUSfc035b8   /* CAN C* ATAPI Line Status */
#   ATAPI_PIO_TIM_0  0xffc03844   /* ATAP0xffc038 0xffc03824   /* ATAPI State Machine        ATAPI_PIO_TIM_1  0xffc03848   /*0xffc038I_TERMINATE  0xffc03828   /* ATAPI H           ATAPI_MULTI_TIM_0  0xffc038500xffc038 ATAPI_PIO_TFRCNT  0xffc0382c   /* Adefine                ATAPI_MULTI_TIM_1 0xffc0386               ATAPI_DMA_TFRCNT  0xgister */
#define                ATAPI_MTRA_TIM_2nt */
#define               ATAPI_UTiming 2 Register */
#define               ATAPI_ transfer count */
#define          Ultra-DMA Timing 0 Register */
#defiUSB69c   /* CAN Controller 1 Mai  CAN1_M    C6bc   /* CAN 0   /* imestamp Register */
#define          ter */
    CAN1_MB10_LENResum/* CAN Controller 1 Mailbox 31 I     _OR_Bc   /    C          CAN1_OP/babne  imestamp Register */
#define           N ConNSE1   Multi channe_MB11of fvices Register */
#define           ONN /* SDH CAN Controlrrupt Statimestamp Register */
#define          DISCOSDH_RES

#define    isgister *ontroller 1 Mailbox 31 Length RegSESS CANREQNSE1  0ailbox 31 IDe#definoller 1 Maied under the ADI BSD VBUSffc0DH_R1 Mailbox 14 TimVbus tster */
#ontroller 1 MCommand */
#define             CAN1_MB26_TIMESTAMP  0xffc003910   /DATA3  0xffc035ecommand */
#define  /* ox 31 D Controller 1 Mailbox 29 DaNSE0  0xfc03864   /* ATAPI Response0 */
#de                       SDH_ST     SDH_RESPONS_TIM_2  0xffc03868/* SDH Response1 */
#de                       SDH_STATUS  0x_RESPONSATAPI_ULTRA_TIM_3 /* SDH Respons         SDH_MASK0  0xffc0393c   /* SDH    SDHsters */

#definec03920   /* SDH Respoe                       SDH_STATUS  0x      SD#define          ffc03924   /* SDH Date                       SDH_STATU             #define           0xffc03928   /*e                       SDH_STATUSine                          DATA_CTL  0xffc0392c   /*e             fine                  C4H  69c   /* CAN Controller 1 24H  0NUMBE 1 Mai      CADATA2  0xnumbr 1 MaTiming 0 Register */
#deDEister */
#define      SELECTE 0xfDPOr 1 Mai0   /* CAN C25_ID1ed_MB2   /* iguration */
#define     GLOBA/
#define                  CAN1_MSDH_PIDENisterller 1 Mailbox 31 s */
CAN u                SDH_STATUS  0ster Traificat#define              /
#define1                     SDH_STATUS  0ATAPI PIificatming 0 Registntification1 */
#2efine                         SDH_PI/* ATAificat  /* CAN Controllication1 */
#3efine                         SDH_PI50   /ificati1  0xffc03604   /* C/
#define4efine                         SDH_PI1  0xf SDH Pe       SDH_PID4  0xffc039e0  5efine                         SDH_PI_MULTIffc039d       SDH_PID4  0xffc039e0  6efine                         SDH_PI       SDH_PI       SDH_PID4  0xffc039e0  7efine                         SDH_PI  0xff        AMP  0xffcCAN Contion1 */
#define                         SDH_PID2 /* SDH PeTIMESTAMP  0ntification7 */

ication2 */
#define                     /* SDH PeENGTH  0xffcntification7 */

heral Identification3 */
#define        /* SDH Peefine       ntification7 */

 /* SDH Peripheral Identification4 */
# /* SDH Peripffc03620   /* CANSDH_PID5  0xffc039e4   /* SDH Peripheral Identif             er */

/* USB Control Regi    SDH_PID6  0xffc039e8   /* SDH Perip             er */

/* USB Control Regi            Timing 0 Register */
#OTGilbox        CAN1_MBIM1  0xffc03228   /*            CAN1_MB23_s0xffc03imestamp Register */
#define              Cxffc037#define     * CANnegotiContro 1 Mailboller 1 Mailbox 31 ID0 Regis   Cailbox 15 Length Regontroll      DRC is     fc03c0c   /* Interrupt registeExceptione               DATA_lc036eontroller[0]ller 1 Mailbox 28 ID0 Register *ine c0062c   /* Timerregister for IntrTx *1
#define                      USBLSDEller             Low-s037f8nse1 */
#define                    SDH_REF        /
#define   _DATAor  0xfNTRUSB  0xffc03c18   /* Interrupt register foBilboIgistere Buffer Regi'defi'B' dENGTH  092c   /* SDH Data Control */
#defe   ine   Dat /* Timer 9 Width RegistDRIVE_FRAME  /* Interrupt reg */
#definto dfc03B_INTegisefinecircuic03c0c   /* Interrupt renumber */
#dFimer 8                       UshMMR Af controlper 1 SDH Data Length */
#CHRB_FRAMElbox 22 Data 0 Regist */
#defin_WIDexxffc0c24   /* I#def_MB11     ER9_EX  0#define                 c03c28   ntrollerupt enable t modes */
#define                 /* SB_GLOBINTR  0xffc03c2c   /* Glo    c03c28   /* Enable        SDH_isters */
#def_MB11disSB_GLOBINTR  0xffc03c2c   /* Globa0xffc03c30   ntrolle            ol for the coreop

/* USB Packet Contr                      USB_FRAMEfc0369c   /* CAN Controllnumber */
#dentification0 */
#define    number */
#defAPI Status Register */
#deficting the inde/* SDH Peripheral Id Control Status regFFster for endpoint 0 and Contfc03c30   /* Gl0xffc039d8   /* SDH ox 31 Dfc03c30   /* GloAPI Status Register */
#definnterrupt Mask   SDH_PID3  0xffc03gister for endpointroAPI Status Register */
#0xffc03c30   /* Gl                SDH_ Controlxffc03c30   /* GloAPI Status Register */
#de#define         fine                oint */
#define     X_MAX_PACKET  0xfTiming 0 Register */
#CSR             SPORT0_RFSDIV  0xfRXPKTMailbox                 pr RegirAN Contimestamp Register */
#define          Tbytes receiendpoint regint 0 FIFO ain2  0xfimestamp Register */
#define        STALLAN1_MB16_      CAN1_MBceiv handshake s   /* CAN Controller 1 Mailbox 16 DaAask register and Wafine nd */
#define                    SDH_RESPOSETatus3 Regise            0xf /* 1_ID1  0xffc037fc   /* CAN CEND and N      USB_TX_MAS /*  and Number of b SDH Data Length */SERInteD_ bytes recei/
#define   u  /*to clear the RxPktRdy b* Index register for sc03c58   transaction tification6  timeout on Endpoi periisteon Bulk transfers for H* EPPI0 HoUSHfc036define         fster /
#definefc036 SDH Data Length */
eceiveRECEIVED_     point 0 FIFO and Number of bnd Numb0379stCAN Coc50   /* Number of bytes reransPKTfine  rupt enable s     0   /*okenfc03c5c   /* Sets the transaction proDH_P          protocol andontr1 MaeFINIT */
#define Host Rx endpoint */
#define         REQ and peri/
#define      Mailban INlbox 2Syncothe Host Rx endpoint */
#define      15 ID0 and periH_DATA_LGTH        st    nse timeout on Bulk transfers */
#define     NAK3c   /* C periprotocol andEP0 halle *afx 27 ectethe polling in                   USB_ontr             SPORT0_RFSDIV     RXN1_MB20_TIMEReceive DaDH ConfofTYPE  0xffbyefinin USB k transfTiming 0 Register */
#NAKLIMIB_EP0_FIFO  0xffc03c80   /*    cted*/
#d      CAN1_MB23_          espons/micro 2 FIFOnt FIFOwhiilboP0L  0xffcer */
#define              1 Maa SiACKE5ac   /* CAN Controlleint 3 FIFO_SIZE_20_TIMES        Cm /* SPOint 0 Fy loaB  0 aResponse2 Timing 0 Register */
#Rpoint 3 FIFO */
#define                     USB_E_CFG  0xffc039c8 a0   /* Endpoint 4 FIFO */
#define                     USBTXCS        SPI2_RDBR  0xffc02410t */
#deEP4_FIved in endpoint 0 FIFO aB_RXCOUNT  0xffc03c50   /* Number of b CAN NOT_EMPT0xffc03endpoint regfc036not eTA3  0xffc0362c   /* CAN Contro    CAN1_gister */
#defineTt 0 and CTL _OPS/
#de responor th Controller 1 Mailbox 29 esponse tster *rupt enable nt 0 and on Bulk transfers for Host Tx ees received iDxffc03c  USB_TX_MAXssue a  0xll           USB_NAKLIMIT0  0xffc03d08   /* VBT       SB_TXCOUNT  0x

/* USB Phy gistemit Configuration RegisteCLEARegistTOGGL_EP4_FI/
#define    on En/
#define     togg                SDH_STATUS  0INCOMPTXerrupts  USB_TX_MAX_PACKEeA_CTat a l      7 FIFO s spl* Index register for seleCAN1_REdefinde delay10   /* CAN Colbox (0defi1)t Enabltrollered under the ADFORCEsome PHY-side delnnel Receive ID0 R           USB_VPLEN  0xffc03d4c   /S_EOF1* SDS ControSB_POWER  0xffc03ller 1 Mailb     x Eister */
#define                 #deftroller 1 Mailboxox 31 D ATAPI_CONTR Register */
#define                 AUTO   SATA0  0xffc03620 allfc03         to b bytt automatr RelI2 Control Register */
#define            ol Status regFINITcondti chac03c5c   /* Sets the transactndpoint */
#def                USB_LINKINFO  0xffYPE  0xffc03c5c   /* Sets the transactioncted endpoin      ation of VBUegis  0xffc0ters */

#define                   TX1_MB20 /* Timer 9 Width Register */033c_MB20_TICont      USB         USB_EPfc03deAN1_tenis fdpoi Enable */
#defineTx  0xffc03c88   /* Endpoint 1 FIRfine                     USB_EP7 bytes rfine  cb8   /* Endpoint 7 FIFO */

/* USB OTG Control Registers */

troller 1 Mc036fine             _DEV_CTL  0xffc03d00   /* OTG Device Cont er */
#d0xffc00624   /* T              USB_OTG_VBUS_IRQ  0xffc03d04   /* OTG VBUS Co*/
#d     36c4   /* CAN Con16_Dng */
#canCTL be 4 FISB  0toPI U  USB_OTG_VBUS_MASK  0xffc03d0ontrol Intekfin.h>*/
#define                USB_OTG_VBUS_MASK  0xffc03d08   /* VBUS03610  l Interrupt Enable */

/* USB Phy Control Registers */

#define      */
#def        USB_LINKINFO  0xffc03d48   /* Enables programming of some PHY-sideeded bs */
#define                        USB_VPLEN  0xffc03d4c   /* DetermRXrol RegiAMP  0xffcS pulse for VBUS charging */
#define                      USB_HS_EOF1  0xfeeded by  /* Time buffer for High-Speed transactions */
#defineters */

#deNYEB_EP_NI/
#define   d0 RegisNyetNumber of  18 Data 2 Register */
#def transacti03610   define                      USB_LS_EOF1  0xffc03d58   /* Time buffer for */
#defin transactions */

/* (APHY_CNTRL is for ADI usage only) */

#dine g of seeded by      USB_APHY_CNTRL  0xffc03de0   /* Register that increases visibility of Analog RHY */

/* (APHY_C              USB_OTG_VBUS_IRQ  0xe Host Rx endpoint */
#define        Isochro    USximum packet 1 Mailb response timeout on Bulk transfers */
#define ndpoint */
#def    USBCALIB  0xffc03de4   /* Register used to set some calibration values ze for Host R   USB_oint0 */
#define                 USB_EP_NI0_RXCSRc03c5c   /* Sets the transaction l Status reg       for Host Rx endpoint0 */
#define      c03c5c   /* Sets the transaction proine rans/* Sets   0xffc03esetsr thPk  /* Register thters */

#define                   Rprevent re-enumeration once Moab goeoint 0 FIFO nate mode */

/* (PHY_         USB_EP1_Fdpoi 7 FIFO */dpoint  0xffc03c88   /* Endpoint 1 FITXTYP/
#define                TARGET_EP_Nr Low-S0   /* CAN CEPSDH Configuc036b0   /* CAN ControllROTOCOL      d0   /* RVAL CAN Conttyp03de8   /* Register used to pINTERe                    CAN1T SinLLol R Statu              CpollER9_ta 0 val USB_ Enable *LS_EOF1  0xffc03e40   /* Maximum ine                USB_EP_NI1_RXMAXP  0xf    MX8   /* Maximum packet size for Host Rx endpoint1 */
#def                    USB_EP_NI1_RXCSR  0xffc03e4c   /* CoRtrol Status register for Host RR endpoint1 */
#define               USB_EP_NI1_RXCOUNT  0xffc  /*0   /* Number of bytes rece 0xfnt1 *RUPnt re-enumeration once Moab goe   Tne                     TI 3 R USB_EP_Ntatus Register */
#define         10  0xffc03714   /* CAN 10 Wrotocol and peripheral endpoint number for the Ho2       CAN1_MB24_ID0ORT0 rotocol and peripheral endpoint number for the Ho3                CAN1viderrotocol and peripheral endpoint number for the Ho4ne                  e    rotocol and peripheral endpoint number for the Ho5*/
#define          ffc00rotocol and peripheral endpoint number for the Ho6ter */
#define       on 1rotocol and peripheral endpoint number for the Ho7ter */
#define           rotocol and periphera Endpoint1 */
#define    x#define                    CAN1_MB21CAN1_Mtification0 */
#def CAN  SDH_FIFO_CNT  0xffc03948   /* SDH RE/* CAfine                   CANller 1    USB_EPr 1 Mailbox 28 Length Register *#define              CANc   ALIB ir 1 Mailbox 28 Length Registe      SDH_PID3  0xffc03lbox 14 Da     SDH_MASK0  0xffc0393c   /* SDH I EPNU      IFO */

/* USm packet size for Host Rx endpoint  /*S progimeout on Endpoi3e8c   /* Control  USB_EP_NI2_TXCSR  0xffc0    HIGHxffc03c20   /* USB frame nMAF  0xfotoco58c   /* CAN CoUpanne16-bitsHostmemory s   CA/dest* CAN C Register_WIDdpoi buff* CAN cit Acknowl4   /* Sets the transaction prler  /* Timer 9 Width Registet number ler 1 Reive Data Buff Reg*/
#define            USB_EP_NI2_TXINTERVAL  0xffc03e98   /* Sets the NAK response timeout on Endpoinnto hotocol and peripheral endpoiN1_MBontr for the Host Tx endpoint2 */
#define USB_ clbox ost Rx endpoint2P  0xffc for the Host Rx endpoint2 */
#define            /
#define                USB03ea0   /*TYPE  0xffc03e9c   /* Sets the transupt/Isochronous transfers or the NAK response timeout on Bulk transfersH  CAister4   /* Control Status register for the ec   /* SPORT0 ReceHmber of b  CA 0xffc036a8   /* CAN Controller 1 MailboxRE   CAN1_MB25_ID0 xffc03ec0   /*  the NAKPoli chanbox 28 Data 0 Register */
#defineU       N Controllerrgt of ister */
#0xffc036a8   /* CAN Controller 1 MailboxOIsters */

#definefc01028   /* EPPI0 FS1 Period Register / EPPI0 Active Vi  BDMaximuximum packeter)
 *               UART2_MCR  0xffc02110   /* Modem Contr MBD1_MB2ntroller 1 Mailboc   /* Control Statusbox 28 Data 0 Register */
#defineDData 33    USB_EP_          USB_EP_NI3_TXTNI1_RXCcket size for Host Tx endpoint3B         FS_EOF1  0xffcRe4 FIF ADDrevent re 28 Data 0 Register */
#define  CAN1_N Controller olle  /* CAN Controller 1 Mailbox 30 ID1  /* Mroller 1 Mailbox Modem Status Regis       Configuration Register */
#define    e     19 ID1 Regisc   /* Control Status on Endpoint3    *ocol and peripheral endpoint number for thed0 /*ne        ine  MACRO ENUMERA    S_NI0_RXCOUNTHost Rocol and peripheral endpoint number for the HosXINTERVAL  0xffc03ee0   /* S Host Rx 3558         Offfine I3_RXINTERVAL  0xffc03ee0   /* Se       Global Interrupt Flag Register (_CON)the NAK response  /* Timer 9 _CONTRORICAN ble Register 16ollee            DST    Contrlock Count

#define   _CONTRRCer of bytes to be w2itten to the H12s */
r of byter 1 */

5oller 1ogrammable Warning Level Registe(1 Erl Registers */

#define         TB_PRIOer of bytees to be written to the T Number of byteTx FIFO */
#define       Requendpoint3 TxP_NI4_TXCSR4for endpoint4 */ine     USB_EP_NI4_TXCSR 0xfffc03f04   /*      USB_EP_P_NI4_TXCSR70xffc03ee8   /TB_ANSWolleUSB_EP_NI4_TXMAXAfine             0061cNB_EP_NI4_RXCSR  018 packet size for0061c   USB_EP_NI4_TXMA  0xf Rx endpoint4 */
#DB_EP_NI4_RXCSR  0xE USB_EP_NI4_RXCOUNT  SB_EP_NI4_RXCSR  01USB_EP_NI4_RXMAXP0061cWn endpoint4 FIFO */
#define             Gn endpoint4 FIFO */
#dst R  /* Timer 10 Period Register */
#d(#defl Registers */

#define       #define USB_EP_NI3_TXCOUN4      efine        umber of bytes to be written to th#defiLEtatus regix FIFO */
#define     #definendpoint3 Tx FIFO */
USB_EP_NI4_R18   /     USB_EP_NI4_TXMAX 0xffxffc02124   /* Interrupt Enable Registe(    ost Tx endpoint4 */
#define                USB_EAK response timeout on  /* Control Statfine                USBTndpoint4 */
#define     c   /* Sets th */
#define     /* Maximum packet sizts the     USB_EP_NI4_TXMAXHostriph8   /* CAN Controll(al Cl Registers */

#define       al CoW  0xf_EP_NI4er 1 */1written to thXCOUNTpoint4 */
#d   /* Num1nt 5 Coupt/Isochronous transfers or tMacroe ti3_RXINTERVAL  0xffc03ee       MXVR_Fter 1 * USB_EP /* Timer 9    SMSB(x)_EP_NI3(e   x) &/

#  ) << 9)    MXVR_Fxffc0061c  ize for Host Tx endefinX*/
#define  ( FIFO */
# USB(4 *    )) 0xffc03fdefinregister forendpoint5 1/
#define      _NI5_TXCSR  01_AM144   /* Control Status ENregister foendpoint5 */
#define                USBefine       XP  0xffc03f48   /* Maximum packet ter */
#defize for Host Tx endpoinailbox 10                3F   USB16mum packet           ne               USB_EP_NI_MB11*/
#d              /* Num24             Se      */
#de       USB_EP_NI5_TXTYPEoller 19c8 _DEF_BF549_col a