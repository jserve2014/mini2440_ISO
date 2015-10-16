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
 * CopyrigFL2U  0x20000*
 */* Frame Lock to UnlInc.*/
#define*
 * Copyrighh licenseBU2L07-2408 Analog DB* Lic * CLictos  Licnse d under the ADI BSDlicense  o 20he G8L-2 (or later) * C Lic
 
 * CLcense 49_H
#define _DEF_BF549_H

OBERRhe G1L-2  (or/* DMA Out of Bounds ErrorF_BF549_Hsed unde _DETIONS FOR 
/ PFfineG0L-2 /* SYSTPLL Frequencyers aedTIONS FOR ADSP-ONS F */

/ers aludSCZefBFP allase.h fSystem C defiCounter ZeroTIONS FOR ADSP-BF549 */

/* IncluFackfin.e alSYST.h fFIFODEFINIes that are common to all ADSP-BF5CMeededh>


/0P-BF5Control Message Receivines that are common.
 *allre commCMROFefONS x_barso allADSP-BF5ster */
#defineBuffer OverflowTIONS FOR ADSP-BF549 */

/* IncluCMTSprocessorsiguration Register */
Transmitster */
Suc 8 Cfully Sen    TIMER8_COUNTER07-2ffc00604  rs *TCeded by ADbaseRegisbaseensed undester */
#define efinexffc00PCancell      TIMER8_CONFIG  0xffc00600   FRWR     e      0.h fRemote Writetion Register */
Completimer 8 Width Register */
#define   5Bx mer 8 Confi0xffcegiste8 Confse.h"ll ADThe followingER8_Cfineed undBM needed    uIOD ffc0iphase Mar608 dinghat are not in the common header */

/*D neister */
#TH  0xEM &at are no
H  0xt masks for MXVR_INT_STAT_1H  0xions */
#include <asm/def_LHDONE0TH  0xicense       0 Half DoneTIONS FOR ADSP-BF549 */

/* Incl         2ster */
#definTI_CONFIG* Timer 8 2e   0614imer 10 Co  APensed ster */
#deAsynchronous Packe         H  0xffc0060fine            0e   APIMER10_8062eriod ReER10_COUNTx_basene            TIMER8_WIDTH  0xffc0060608   /* Timer 8 Perid und   /* c   /* Timer 11xffc10efine                  TIMER10_COUNTnfig     on R628   /* TimW         TIMER8_WIDTH  ffc00h Reg8   APERIOD   xffc00628   /* Timer 10 Period     TIMER8_WIDTH  0xffc0060cERIOD* Timer 8 P8
#define    8 PeriodAPe       r GroupR AD3 Enabl                         TIMER8_WIDTH  0Tffc00644   /* Width/
#define            d undon Register */
 SPOR2Width Register */

/* Timer Group of 3 Registe#defi2     l ADSPORT0 efine                  TIMER10_COUNTAPRCEnsed un       xffc00644   /10 Timer /
#definCRChat are not in the common header */

/APRP     8              TIMER8_WIDTH  0x fc00800_ Periodat are not in the common header */

d und3/
#define          3Width Register */

/* Timer Group of 3 Registet Ser54x_ClInc.Dividerefine                  TIMER10_COUNd underioster */
#define 4 Registers */

#define                      SPORT0_54x_b(or lat00624 xffc00800 e        evicesSyncc0080c   R5er RegisteSP-BF5EM 5Width Register */

/* Timer Group of 3 Registe     UNTER  0xf fmer 8 Register */
#define                    6 Period RegguratDMA6 Registers */

#define                      SPO           guration 1 Register */
#define                    7  TIMER_ENABLE1 DMA7r Register */
#define                    SPORT0    MER_ENABLE1 ster          */
#define  9_WIDTH* TimerEN_0#define    98   /* Timer Group oNI2AENster */
#define Network Inact    to A      IER  ruptister ne                  TIMER10_COUNNA2Icta Reon/
#define ivee     Periodto                             TIMER8_WIDTH  0x         STSor txffc00    eriod ReSuperregisteatioifndef549 *             SPORT0_CHNL  0xffc0CHNL* Timer 8834L2Uxffc00          8 Channel/
#ders and bit defiine                      SPORT0_CHNL  0xffc008A PRfc00800SPORT0    /*Posi0824 
#defR  0UpdaRegi */
#define                     SPORT0_MCMC2  0M00838  c tion RegisteMaximum Multi cl Confi2  0xffc00824 
#define 2                     SPORT0_CHNLSPO D0_MTCS0 tus isable RDelayi channel Transmit Select Register 0 */
#define                  R          3defier */
a Regist0084eriod Rc00800 RT0 Multi chane        SelectSelect Reg1            834 MTCS0             S/* Timer 10DRESSaryxffc00848   /* SPORT0 Multi channel Transmit Select isterAT_MTCS0  0           llocaMultiT     Transmit Select Register 0 */
#define                  FCZ0      MT          evicesUNTER  000xffc0MR 3 */
c00838 5     TI848   /* SPORT0 Multi      c0062c   
#define 0              c0062c   /* Timer            1      SPORT0xffc00848   PERR        TIM    SPOParityT0_TCLK channel Transmit Select Register 1 */
#define  Ha RegistDIVPORT0 MulMRXONB High/
#defwRT0_R    Transmit Select Reg           TIMER8_WIDL2Hulti chan      3PORT0 Mul    to085c  4c   /* SPORT0 Multi channel Transmit Select RegWUP Multi chPORT0 MulWake-Up Pream      TIMER8_           SPORT0_MRCS1  0xffc00854   /* SPORT0 ta Regist             */
#dine                      SPORT0_CHNL        CMC       SPOR38 ht      LAY  0xTransmit */
#dtion Register 1 */
#define                     SPORT0_MCMC2  00xffc00848  all Core registester */
#define        SPORT0_CHNL EPPI0_Vc0064PORT0 Mu10   /* CS1  0xffcffc00Core re Configuand bitatus ine                      SPORT0_CHNL  0xffc008Bl neeerfine   R_RX  0xffc& MMare 0_MTC hat aree                     EPPI0_HDELAY  0xffc012PORT0*/
#dec00800        4   he seMR ADed unde4c   /* SPORT0 Multi channel Transmit Select Reg#CZ     Verticgurat#iP-BF5e "dmer 8 Conf 0xffc0                 HDELAYPI0_VDELAY xffc006      HEPORT0 Mur */
#common to TIMER8_4c   /* SPORT0 Multi channel Transmit Select RegCMI0_VDELAY14  iguration Register */
#define  ine                      SPORT0_CHNL  0xffc008 SPORxffc0102xffcTransmit Select Rege Select Register 3 */

 / EPPI0 Horizontal Blanking Samples Per Line RegASPORPI0_VDELAY1cdefine                     TIMER8_WIDTH  0xffc0060cIMER_DCONTROL  FS1        SPORT0_r /NTROL  Act0 MuVideoCELAY      TI                SPORT0_CHNLffc0060
#define      TI  /* SPORT0 */
#define                     SPORT0_MCMC2  0xSPORT0 Mu10 Currgister */fine            1     TIMER10_9nnel4c   /* SPORT0 Multi channel Transmit Select RegBRT0_MRCSider Register */608   /* Timer ine                      SPORT0_CHNL  0xffc008ORT0 Saer 9s Tim Lanking Register */
#def9FS2W_LV */
#define                     SPORT0_MCMC2  0xReg L     offfc0         TIMER8                   T     #define                    T          SPORT0_RFSDIV  0xfd undEN          TIMER_STATUWidth Regisine                      SPORT0_CHNL  0xffc008                     EPPI0_FS1P_A4SPORT0 848   /* SPORT0 Multi channel Transmit SeP2_DL0xff0xffc00628   /* Timer 10 Period Regist
#ine                      SPORT0_CHNL  0xffc0080FS2W848   /* SPORT0 Mu                     SPORT0_EPPI0_FS1P_AVPL ine                      SPORT0_CHNL  0xffc00figuras       /* SPORT0        /* Timer1028  UART2_LC/* Timer 21    EPPI2104nnelefine      iguration Register */
#defi2P_LAVF  0xffc01030   /* EPPI0 FS2 Period Register/ Select          Segister */
#define                    TIMER_STATUxffc_ster */
#define Set               TIMER8_WIDTH  UA002104           SPe                     TIMER8_WIDTH  0xffc00c0061U        SPatusegister */

/* Two Wire Interface Registister */#define    0xffc00800 ve Configuegister */
#define                    UART2_IER_SET      TCR       SPORIMER1/* 2_LSfine       24           Ste                  CEulti channel Recee           EPPI0_HDELAY  0xfTCR_LINE f_CONTROL  0xffc02204   /* TWI Control Register */Ped unden 2egister */

/* Two Wire Interface Regis  0xffc02LKe                     TWI1_REGBASE  0xffc02200
#defi0_TFSial  0xffc0080c  egister */egister */
#define                    UART2_IER_SET 0_TFSORT0_MRCS3  8    EPPISe                     TWI1_REGBASE  0xffc02200
#define                    TIMER8_WIDTH  0xr */
#define                    UART2_IER_SET egister */
#defineData   TWe                     TWI1_REGBASE  0xffc02200
#defiHNL  0xffc0RX      SPORfine 2214egister */
#define                    UART2_IER_SET              SPORT0_CHNL  0e                     TWI1_REGBASE  0xffc02200
#definRT0 Mu2  0xffc00824    TWI Slave Mode Address Register */
#define                 Tter *2208   /*L  0 Current4   /* TWI Master Mode Control Register */
#define   2118   /* Modem St_MASTER_STAT  0xffc02218   /* TWI Master Mode Status Register */
#       RT0 MultDR  0xffc022h Byte         TWI1_REGBASE  0xffc02200
UARPOSre no 0xffc02108   /* Global ControFIFO Statu0x3f0_MRCS1  0xode/* SPORT0 /
#define              I FIFO CVALIDegister */

/* T           DouVaTimer 8der Register */
#defineMAXanking Register */
WI1_XMT_DATA16PORT0Mc0228eriod ReWI 49 ti cha RegistRCV_DATA8  0xfr */mit Dgister */

/* Two Wiial Cl Byte Register */          28  /* SPOWIters *xffc00641_RCV_DA2280   /Da                 SPORT0_RFSDIV  0xffc xffc01 TWI1_RCV_DATA1      RegisS2  0xffTER_STAT  0xffc02218  DLfine ngle Byte Register */RCV_D    TWI1SPI2                     SPI2_REGBASta Sxffc010202400    TWI1_MASTER_STAT  0xe Registeffc0_CT0xffc0 Registers */_FLG  0xffc0 UART2_IER_SET ata Double Byte Register */

/* CHNL   Control Register */ und                     SPI2_REGBASL per                      SPI2_STAT 2284   /* fff1_RCV_    ogical Addressne                        SPI202100   /* Divis0080sor     gister */

                     SPI2_REGBASvider Regi2408   RDBfine      4 Regis      efBFr */
#dta Buisabl         LowConfytne                  TIMER10_COUN            egister */ve Da     Shad                     SPI2_REGBASster/          SPI2_SHADOW  0xffc0241*smit Selster */
#deBuAlternat                TWI1_REGBASE  0xffc02200A408   BAU_DISABLE1241        7PPI0_CONM         27    TWI1TH  terrupt MaLLOC20c   /* TWI Slave MoR_MASTER_CTRL/ EPL0062471_RCV_DATA1el Confi0tionnecMultiLabelD  0xffc00608   /* Timer 8 Period IU00624e  MXVR_CRegState RIntIn Us         TIMER_ENABLE1  0xffc00640RCL     7f           0xffc0271er*/
#dSPI2 Transmit Dae Select RegistInterrupt Enab                 270c   /*02204            MXVR_INT_EN_0  0xffc02Regierrupt St1SPORT0            MXVR_INT_EN_0  0xffc02718   /* MXVR Interrupt Enabl#defi   /* EPPI0 define                    MXVR_INT_EN_1  0xffc0271c 0_TFSxffc02fc0241           MXVR_INT_EN_0  0xffc02718   /* MXVR Interrupt Enabl0_TFS            MXVR_STAT                   SPI2eceive DataRegist  0xffc02108   /* Global Contro
#271c   /*        MXVR State RI4 Register 1 */
#define                    MXVR_POSITION  00xffc0061c                            MXVR_INT_EN_1  0xffc0271c             TIMER8_WIDTH 5  MXVR_INT_EN_0  0xffc02718   /* MXVR Interrupt EnablVR Logffc02718   /* MXVR 5                 MXVR_INT_EN_1  0xffc0271c               MXVR_Ister 6 Register 1 */
#define                    MXVR_POSITIO    0c   /*      TIate R6ode Position Register */
#define            118 TH  0MAX_          07ffc02724   /* MXVR Maximum Node Position Register */
#118       TIMER8_WIDTH  ter *TH  0             27ne      ate RNod2e      00844egister */

/* Two Wire IxffcSPI2_STAT  73c   /*DE8  MXVR_INT_EN_0  0xffc02718   /* MXVR Interrupt Enabl__LIN */
#define         8             MXVR_LADDR  0xffc02730   /* MX9Ruffer Register */* SPI29  MXVR_INT_EN_0  0xffc02718   /* MXVR Interrupt Enabl       /* MXVR Group Addr9             MXVR_LADDR  0xffc02730   /*CL    H  0A peration Tabl38 1   MXVR_INT_EN_0  0xffc02718   /* MXVR Interrupt Eer */TH  0xte RA     0824 Ter */location Table Reg   /*A */
_5ation Tabl5             /* MXVR Alloc1gister */
#5         TWI1_REGBASE  0xffc0220ster 6 */
eceive Da Register 6 */
#ter */MXVR Allocation Table Register 1 */
3define                     MXVR_ALCL1egister/
#define         1  TWI1_SLAV8   /* MXVR Allocation Table Register 7 */
Allocx0 Clippingister 6 */
#ocation Table Register 6 */
#define                e                3_9  0xffc02760   /* MXVR Allocation Table Register 9          r */
#define 1e Sel  MXVR_ALLOC_10  0xffc02764   /* MXVR Allocatio  /* 2754   /* MXVR Alocation TableSPORT0 n Tab  /* SPONsmitefine      er *       */
#1I FIFO Co276    ocation Table Register 6 */
#define       V
#define                            TWI1_REGBASE  0xffc02200
   /*GVR Allocer *2732204         TWI1_MASTER_STAMXVR Allocation Table Register 1 */
4 /* MXVR Allgister */8ble Register 14 *   /*ine                tion Table 0xffc027                         ta Loger * /* MXV_ALL7 Register 1      cation Table Register 6 */
#define       cation TAN_1  0xfster 6 */efine                718   /* MXVR Interrupt Enabler ** MXVR SynAssig Select Regc0062c   /* Timer 10ster 6 */
#define            xfLOC_12  0xffc0276c 754   /* MXVR Allelect Register 0 */
#define      R AllocatM Single    /*SYNC_LCHation Regist Assign Register 2 */
#define       TWI1_MAST              MXVR_ALL6 */
#4  MXVR_ALL        ta Logi4   /* Mer *784   /* SYNC_LCHAN/
#deffer
/* MXVR Channel Assign Registers */

#5efine                MXVR_SYNC_LCHANtion 0xffc02744   /* MXVR 284   /* MXV8   /* MXVR Allocation Table Register 7 */ Channel Assign Regi/
#def2e        fc02784   /* MXV6ble Register 14 *2    TWI1_MASTER_STAT  0xf   MXVR_ALL5 Register 1 HAN_3  0xffc02784   /* MXV7 MXVR*/
#  /* MXVR Group Addr2R Sync Data er 3 */
#defineN    al Chann9I2 Co            MXVR_Ister 2C_9  MXVR_ALLOegisters */
HAN_3  0xffc02784   /* MXV9HAN_5  Select Reg3ble Region Re Channel As   MMXVR_ALLO            Allon Reg273c   /* MXVR Alloc2     TWI1_MASTER_STAT  0xffc0tart Addresse         76 Loop NFIG  0xffc0279defin6  0xffc02791LAY  0xffc02728   /* MXVR Nod6lMXVR Sync Data Lo8   /* MXVR Allocation Table Register LCHAN_6         MXVR_DMA0_START_ADDR  0xffc1ister 0 */
#define    YNC_LCHAN_7  0xffc0271 */

#def277           MXVR_DMA0_START_ADD2gister 13                on Register */
tart Addresssign Regist7               1e Vid          TIMER8_WIDTAssign Register            TIMER8_WIDTH  CL   MXVR_DVR Allocation Tab2HAN_5  0xffc  0xel ConfiXVR Sync Data Loe Select Regi       VR_SYNC_LCHANTA8  0M0279gister */
#7r 4 */
#defiLCHAN_5  0xffcc2l Channel As*/
#define   2Register 2 */gn Register 6 */
#define  c0279I FIFO CoAlloca2  0xffc02744   /*EM &8   /* MXVR Sy   /* MXVR Sync Data Dble 7a DMA0 Current Address */
#define            MX27 0xffc00n Tab20279sters */

#      TW  MXVR_DMA1_CURR_ADDDMA1_COUNT MXVR YNC_LCHAN_7  0xffc0271 Current Address */
#define      sign Regis* MXVR Sync Data DMA1 Sta2VR_DMA1_COUNT  0xffc027b4   /*4PPI0_FS1P_AVPL  0xffc0102 Vertical Blanking Register */Channel Assign Register 2 */
#define t Se*/
#define        XVR 3  0xffc02790   /* MXVR Sync Data Logical Channel Assi Curre /* MXVR Sync Data D3             MXVR_SYNC_LCHAN_7  0xffc02794 t Serial Clock  Channel Assters */

#ssign Register 7 */

/* MXVR DMA0 Registers MXVR_             MXVR_DM3 DMA2 Current8Loop Count 27b8   /* MXVR S8efine                MXVR_SYNC_LCHANt Serial Clock Div Channel3r */
#define             MXVR_DMA0_START_ADDR  0xffc0  0xffc02778   /
#define  t Sertart Address */
#define                3Sync Data DMA1 Current 0 3a0   /* MXVR Sync Data DMA0 Loop Count Register */
#dMA3 Lo  /* MXVR Group AddrURR_ADDR  0x1c Data DMA1 Current Address     tart Address   MXVR_ALLOC3UNT  0xffc027a8   /* MXVR Sync Data DMA0 Current LoopMA3_CUc   /*          TIMERisters */

#define                 MXVR_DMA3t Addresnc Data D       TXVR Sync Data DMA2 Current Register */
#define        27e_2  0xffc0 0xffc023e Vidr 0 */
#def1_COUNT  0xffc027b4   /guratio9#defigister */

/* Two Wire Interface */
#define   MXVR_SYNC_L3        MXVR_DMA1_COUNT  0xffc027b4   /* MXVR Sync DaR_INT_SR_DMA3_START_ADDR  D */
#define              MXVR_DMA1_CURR_ADD3MA1_COUNT  0xffc027b4   /3c Data DMA1 Current Address */
#define             MX            t RegisterA3 CuVR_DMA1_COUNT  0xffc027b4   /*ister 0 */
#dXVR Sync Data DMA2 C MXVR_DVR_DMA2_CONFIG  0xffc027c0   /* MXVR Sync Data DMA2 C1028   ble Register 14 */

           MXVR_DMA2_START_ADDR  0xffc027c437          MXVR_DMA1_CURR3tart Address */
#define                  MXVR_DMA2_COTA8  0DDR  0xffc027ec   /* NFIG           MXVR_DMA1_CURR_ADDDMA1_COU1  /* TWI FIFO Status RegTH  0xffc00CL11c  0xffc02744   /* MXVR L  0xffc02790   /* MXVR Sync Data Logical Channel Assi  MXVRble Register 14 */

/             MXVR_SYNC_LCHAN_7  0xffc02794 4 MXVR Sync Data DMA1 Starsign Registssign Register 7 */

/* MXVR DMA0 RegistersA6nel Receive Select Regis40xffc027ec Register */

/*279a DMA5 Start A4c Data DMA0 Config Regist4r */
#define             MXVR_DMA0_START_ADDR  0xffc0VR All /* MXVR Sync Data DI2 Cotart Address */
#define                I2 Con273c   /* MXVR Alloc4a0   /* MXVR Sync Data DMA0 Loop Count Register */
#d                   MXVR_DM40_CURR_VR Allocation a0xffc027c0   /* MXV12 */
#define Register 8 */
#define   I2 Control RegiDMA5 CurrentUNT  0xffc027a8   /* MXVR Sync Data DMA0 Current LoopurrRIOD/* MXVR Sync Data DMisters */

#define                 MXVR_DMA4_CONFIG  0xffc027ac   /* urrent Loop Count */*/
#define             MXVR_DMA4_S      p Count6*/
#defigisteal Channbegisters */
nt Loop Count1ffc0rt  /* ess */
#define         4        MXVR_DMA1_COUNT  0xffc027b4   /* MXVR Sync Da7_COUNxffc026ter */*/
#defi */
#define              MXVR_DMA1_CURR_ADD4  0xffc027b8   /* MXVR Sy4c Data DMA1 Current Address */
#define             MXR Sync6 CA7_CONF        le Re           MXVR_DMA2_CDMA1 Current Loop1isters */

#defiRT_ADDR  0xffc02828CL4#define                 M4VR_DMA2_CONFIG  0xffc027c0   /* MXVR Sync Data DMA2 C    syFIG  0xffc02824   /*            MXVR_DMA2_START_ADDR  0xffc027c4     MXVR_DMA6_CURR_ADDR  4tart Address */
#define                  MXVR_DMA2_CO1 Currs */
#define         13  0xffc02770   /* MXVR Allocation Table R5              MXVR_DMA2_C5  0xffc02790   /* MXVR Sync Data Logical Channel Assixffc02 /* MXVR Sync Data D5             MXVR_SYNC_LCHAN_7  0xffc02794 ffc0 Table Register 6 */
#define      sign Register 7 */

/* MXVR DMA0 Registers 

/*              MXVR_DM5IG  0xffc027d4   /* MXVR Sync Data DMA3 C1Sync Data DMA1 CurrentMXVR_SYNC_LCHAN Current Addr R818   /* MX5r */
#define             MXVR_DMA0_START_ADDR  0xffc0VR_CMable Regi       MXVR_ Currtart Address */
#define                5A3 Loop Count Register */5a0   /* MXVR Sync Data DMA0 Loop Count Register */
#de Regicket Regisisableisterrrent Address */
#define             MXVR_D5A3_CURR_COUNT  0xffc027e45UNT  0xffc027a8   /* MXVR Sync Data DMA0 Current Loop      fc0282PTBdefine      isters */

#define                 MXVR_DMA57e8   /* MXVR Sync Data D Current Addr Register */
 Register */
#define        ation       MXVR_SYCM /* SPMXVR Sync Data DMA4 Start Address */
#def MXVR_DMA5_CURR_ADDR  0ONFIG  0xffc027locafc027f0   /* MXVR Sync5        MXVR_DMA1_COUNT  0xffc027b4   /* MXVR Sync Da0xffcuFIG  0xffc02824   /*  */
#define              MXVR_DMA1_CURR_ADD5MA1_COUNT  0xffc027b4   /5c Data DMA1 Current Address */
#define             MXe SeleDR  0xffc02858   /* M
/* MXVR DMA5 Registers */

#define         Current Addr Regist       0xffc027fc   /* MXVR Sync Data DMA5 Config Register */
A5 Sge TX Buffer Current            MXVR_DMA2_START_ADDR  0xffc027c45ata DMA5 Start Address */5tart Address */
#define                  MXVR_DMA2_CO /* SPtart Addr Register */op Count Registeefine/
#define    Mtch Lister */
#defNT_1P_CONFI 0x Data RFlu           evicesDDR  0nt LoopF05 StartRouter 9Tabt Seleisters Fnterr* MXVR Interrupt Serial MXVF0ROUTING     0xffc028118   istere   tion Register */
#d5e Select          * MXVR Interrup6e Select
#defition Register */
#d7e Selecefine             /*FRAME_R Fra 1 0xffc02          MXVevicesal Chisters *0 */
#defe          984   /* MXguration Register */
#d27b8   MXVR_ROuting Table Registerffc0 */
#define         */
#define         TWI1_MASTER_STAT  0xffcMXxffcting Ta_3  0xffc02 0xffc027c0  */
#                  MRegister 2 Syn Data DMA2 Loop Count uting Ta_LINE 2c02888   /* MXVR Routing Tafer    MXVR_R2 */
#define        l1 Current MXVR Routing sters */

8able  */
#define                  T_ADDR  0xfine                   tion ROUTING_4  0xffc02890   /* MXxffcouting Table Register 4 */
#det Re                   MXVR_ROUTINign Registe2894   /* MXVR Routing Table 3egister 5 */
#define          4   /* MXVR Sync Data Logical            MXVR_ROUTING_3  0xffc02              uting Table RegisterAlloVR Routing 8   /* MXV8927b8   /C_LCHAN_7                 Mters    MXVR Rou Table Register 4 */
#de  MX                   MXVR_ROUTIN Table Regi2894   /* MXVR Routing Table 4c02888   /* MXVR Routing TaMXV4   /* MXVR Sync Data Logical nt *a DMA5 Startine               ec    /* MXVR Routing ble Register0*/
#defi*/
ine                   MDMA1ine                   MXVR_RO    GRegister */
8a27b8   /* MXe   C_LCHAN_rrent Address */
#define   MXVR R         MXVR_ROUTING_12  0xffc0cal Chan  /* MXVR Routing TMTCS1        MXVR_ROUTING_6  0xffcSync    MXVR_ROUTING_13  0xffc028bVR AlHAN_7  0xffc02794 Control Reg    MXVR_ROUTING_13  0xffc028b Slave MVR_ROUTIllocation Table Regist    uting Table Register 14 */

/*OUTINmer 10 P- 0xff-UART2_IER_SET Cont EPPI0 FS1 Width Register / ers */
B6OCK_2  le Registep CouA5 StC_LCHAN_7  0     
#define           XVR Routi  MXVR_FMPLL_CTL  0xffoop C   /* MXVR Clock Control Regi    VR Routing Table Register 10 *Regi_ROUTING_4  0xffc02890   /* MXUTING_7  0xffc0289c   /*          228  ster */
#define              atio MXVR_FMPLL_CTL  0xffc028d8   /* MXV7OCK_CNT  0xffc028c0   /* M0xffB0xffcer 10 Pe/* MXVR Sync Date Se      MXVR_SYPIN /* SPI2 _CNT dC_LCHAN_7  0xffc02794   /* MXVR FrameVR Routing Tablc   /* TWI Sla SPIEGBASEMAxe VideoT  0xffc028c0220c   /* TWI SlaI2_TMocation          cePI0                           MXVR_GADDR  0xffc0273M
#defin          EPPI0LL CCAN CDirR_INT_Eefine             MXVR_DMA6BY4SWA       tion Register */        Fou      MSwapUART2_Il  MXVe        R setsl Trmit Sel2  0xf2408cR1  0xffc0080st Reset _STAT_1  0xffc021_TR          32    EPPI    ITefine                 N UART2_I         MXne                  MXVR_ROUTI defiBY2efine     * SPORT0 Mue  CAN1_AA1  Twolect Register 2 */CAN Controller 1 Abort Ackn1_TAMFLOW_COUNSTER_CTRL  0xf     AcknOpXVR Sync*/
#def0xffc00608   /* Timer 8 PerFIXEDPMPPI0 C EPPI0 Ver      Reles Rixed PatINT_SMatchh Lolti cR_DISABLE1  0xffc00644   /* TiSTARTPAss RSPORT0_RX  0xffc /* CAN C   /*definatioes I CAN Select Register 2 */fc02734 TOP0xffc0288cN ConMBTIFge Register20op        define      Mailbox              MXVc0064POER10_C    MXync Data         8 Conc   /* TWI FI/* MXVR State Register 1P /* 03214   /* CAN Controller 1 Abort A10_PSync Data DMA2 Sc02418gister */
#define         ss TWI FIFO Receive Data Double ByCANCEL     CA Select Reguest R   MXVR_IMasker 1 Mailbox Transmit Interruproller 1 Abort Ackn RESE      CMailbox ConfN Co    MXVR_I1 Remote FraArbit    MXV       UART2_RBR  0xffc0212c   /RBt Inte        MXVR         UART2_MSR  0xffc02118   /*Ent3  0xister    wIG   Protec0824 Single Sht Loop Countefine        MXVR_        CAN1_MBRI/
#defi  TWon Table Register 14 */

/* TraB    RT   MXVR_ALLO   SPI2_SHAister 0 */
#define  _MASKtrolleivefeI0_LINE ister4y PLL C        CAN1_MBRIF10xffInContro03214ransmit Select Register 0 */efinN Controller 1 Abort AcTRdefine   box xffc0est ResetART2_0 Mur 1 Mailbox Direction Register 2 */
#d

/*MER_DI1  0xffc03218   /* CAN ContTransmiT  /* CAN Controller 1 Abort Ac                N Contst Reset    CAN1_MBRIF1  0xffDir       x Conller 1 Transm/* TWI FIFO Status Register r 1 Transmit ReRequest Setion Register 2 */
N1_RMP1  0xffc03218 hannel Receive Select Register 3 */

MXVR_I
#define         Mailbox 1 */
#defin#define             Ce Registerr 1 */
IMfine                  CM    AN1_MBRIF1  0xffIn                     TIling Enable Register 1 */
#define 1_RFHgister            CAN1_RMP 0xffc0325c   /* CAN ControlMAddr Mailbox Sync DaAN1_RMP2 1 MaShot T           CA           EPPI0_FS1P_AVPL  0xfilbox r 9 */
#define                  Requ6 MXVR_R        CAN1_MBRIF1  0xffc03224   /* CAN pt Fl3 *//*ct Register 0 */
#define        ag Regit     0xffF2  0xffc03264   /* CAN Controller 1 nfig Registister 0 */
#define        CAN1_MC2uivisor Latch Higegister 2 */
#defi 1 R         /* ox Receive Interrupt Flag Register 2 */ */e Regist2  0xffc03264   /* CAN Controller 1 ontrol   /* CAN Controller 1 Mailbox        TIMER8_WIDt2 */
#deHandler 9ster */
#define 2 *NT  0ox Receive Interrupt Flag Registe    s Per uest R   /* CAN Controller 1 Overwrite Protisterfine                       CAN1_ne     MXVR CAN Controller 0xff/    MXVR_/er 10 Periodler 1fine                       CAN1_ Tabl Lines   0xffc0102c    /* Timer Gro Controller 1 gistefine                       CAN1_3214  /* TWI FIFO Status Reg      FS1P_AVP0xffc0ilbox      nker 9_DLL  0xffc02    lag ReBEC  Reg#define          Mailbox Con         CAN1_AA2  0xMr 9 */
#define                  MXVR_ROUTIc03244   BRIF                      CAN1_TA2  0xffcF1  0xff  MXVR_SController 1 fine AN Controll*/
#define   CAN1_CEC  0xffc03290   /* CAN Cont_CEC             CAN1_AA2  0xMuble ByWI1_MASTERledge Register 2 */
#defiGIS           Sync  /* N Controlle   CAN1_CEC AN Co */
#definee        ler 1 Mailbox                      CAN1_G    CAN1_MBIM1  0xffc03228   /* CAN         /* MddAT_1    CAN1_CEC  0xffc03290   /* CAN ContNT  0x    P            xffc006 CAN1_RMP1  0xffc03218   /* CAN ConMe ViTRO0xffc008          CAN1_AA2  0GlobalCounter Register   /* SPORding Rowled     egister */
#define     er 1                 CAN1_AA2  0x0xffc    MXVR_IPendhannel Receive Selectine                        CAN1er 2 */
#define                       CAN1_C              a4   /* CAN Contr3214   /* CAN Controfine                      fc032b0   /* CA               TR  0xffc032a4   /* CAN Controller 1 nc Da         er 9gister */

/* Two Wire Infc03290   /* CAN ConAbo/* MR  0gramm     Warner 9Levnfigura    M               CAN1_RMP2  0xffc03258   /RRDNTROL  0xffc032a0   /* CAN ControllDMA1er 1 Clock Rror Status Register tive ViRea     ontroINTAllocatio32XVR_DMA6AN1_RMP2  0xffc03     rupt Pending Register */
#define             SP               CAN1fi            CAN1_AA  0xffc032c4   /* CAN Controller 1 UniversPAdefinex            CAN1_GIF  0xffMATCHRT2_I_7_COU  TW2_REGBAS* CAN Cove M */
#degiste      MXVR_ROUTING_3  0xff      MXVR_     fUNT  0xffc02 */
#define       AM00Lroller 1 Error Counter Reg      MXVR_      0 A  /* EPPI Mask High Register */
 CAN Controller 1 Global I      MXVR_0_TFSDMailbr 1 Ertroller 1 Global InterriSync D          EPPI00   /* CAN  TWI1_SLAVE_CTRL  0xffc    MXV     MXVR_tion RVR_DMA2_COUNT  0AN1_RMP2  ync Datr */
               3    TWI1AN1_RMP2  0xff_SET  0xffcling Enable Register 1 */
#defgister *1finecceptance Mask Low Register */
#4   /* CAN cceptance Mask Low Register */
#define      0xffegiscept  /*fc0323Low   /* 2228   e   cceptance Mask Low Register */
#define     IM  0xffc03298   /* CAN Corammable Wa4   /* ceptance Mask Low Register */
#define                    MXVR_POSITIO     CAN1_OPSS2  ceptance Mask Low Register */
#define                     0   /ffc02 Controllerister
ol Regnce Mask Low Register */
#define         MXVR_ROUTING_3  0xffc0nce Mask Low   /* ceptance Mask Low Register */
#define     gister */

/* Two Wire Int Table Reg 0xffc0er 1 */
#deffc0325c   /* CAN ControAM1     c0330c  CAN1_MBTIF2  0xCAN1_MBRIF1 10xff1 A85c                   TWI1_REGBASE  0xffc02200
#w Register */
#define20xffc008133   MXler 1 Mailbox 4 Acceptance Mask Low Registesk05L  0xffc03328   /* CAN Controllc0062c  ler 1 Mailbox 4 Acceptance Mask Low RegisteCAN1_MBRIF1  0xff2h Register */
#de1ine     h Register */
#deler 1 Mxffc03228   /* CAN M030xffc008333        AN1_RMP2  0x1MBRIF1  r */
#define     roller 1 Programmable War31c   /* CAN Controller 1 Mailbox 3 AcceMailController 1 Mailbox 6 Acceptance Mask Higister */
#define                SPIc0062c   Controller 1 Mailbox 6 Acceptance Mask Hig   CAN1_TA2  0xffc03250  4h Registe2w Registeffc03230  1 Mailbox 6 Acceptance Ma2         CAN1_AM04H  0xffc03324   /* CAN2AN1_TA2   Mask Low Register */
                     TIMER8_WIDTH  0x                 2ister *5L /* CAN Controller 1 Mailbox 7 Acceptancex 5 Acceptance Mask High Register *  /* MXVR  /* CAN Controller 1 Mailbox 7 Acceptance32c   /* CAN Controller 1 Mailbox 52 Register 30c  ounter Force Reloilbox 6 Acceptance            CAN1_AM06L  0xffc03330 ors *AN1_RM 0xff8h Register */
#define      0xffc0331c   /* CAN Controller 1 Mailbox 3on RegistCA03348   /* CAN Controller 1 Mailbox 9 Ac 1 Mailbox 6 Acceptance Mask Low Register 0 */03348   /* CAN Controller 1 Mailbox 9 Ac 0xffc03338   /* CAN Controller 1 M3  0xff7h Rester */
* CAN Controller 1 Mailbo3         CAN1_AM04H  0xffc03324   /* CAN3  CANe     MBRIF1  0xff1gh Register */
#deler 1 Mar */
#define                       t Serial Clo0H  0xffc03354   /* CAN Controller 1 Max 5 Acceptance Mask High Register *3ne         0H  0xffc03354   /* CAN Controller 1 Ma32c   /* CAN Controller 1 Mailbox 53N1_MBRIF1  0ster */
#define     AN Controller 1 Ma            CAN1_AM06L  0xffc03330 309        CA CAN1_TA2  0xffc03250  11h Register */31c   /* CAN Controller 1 Mailbox 33c   /* CAN C  CAN1_AM12L  0xffc03360   /* CAN Cont 1 Mailbox 6 Acceptance Mask Low Re3ter */
#defi  CAN1_AM12L  0xffc03360   /* CAN Cont      330c              CAN1_AA2  0xffc02400
#define          ler 1 Mailbox 4 Acceptance Mask Low Register */                 CAN1_AM04H  0xffc03324   /* CANc03360  /* CAN Controller 1 Mailbox 9 Acx 13 Acceptance Mask High Register */         CAN1Direction Register 2 */
#dN Controller 1 Mailbox 5ontroller 1 Mailbox 13 Acceptance Mask High Register */2#define               CAN1_TA2  0xffc03250  roller 1 Mailbox 5er 1 Mailbox 6 Acceptance Mask Higx 12 Acceptance Mask         CAN1_AM06L  0xffc03330 _MBRIF1  0xff3ffc03370   /* CAN Controller 1 Mailbox 14 Acceptance Mask High Register 3/
#define                     CAN1_AM13H  0x 0xffc03378   /* CN Controller 1 Mailbox 14 Acceptance Mask Low Register4
#define         TI 0xffc03338   /* CAN Controller 1 M  /* CAN Controller 1 Mailbox 14 Acceptance Mask High Register 4#define     Currentller 1 Mailbox Acceptance Registers */

#deN Controller 1 Mailbox 14 Acceptance Mask Low Register5r */

/* CANController 1 Global Interr3H  0x5ffc03370   /* CAN Controller 1 Mailbox 14 Acceptance Mask High Register 5AN Controlle                  CAN1_AM13H  0x Low Register */
#N Controller 1 Mailbox 14 Acceptance Mask Low Register6
#define   3 Controller 1 Mailbox Acceptance6ffc03370   /* CAN Controller 1 Mailbox 14 Acceptance Mask High Register 6#define    tion Tab Mailbox 17 Acceptance Mask Low Register */N Controller 1 Mailbox 14 Acceptance Mask Low Register7/* CAN Contrler 1 Mailbox 16 Acceptance Maskrolle3370   /* CAN Controller 1 Mailbox 14 Acceptance Mask High Register 7/* CAN Contr                  CAN1_AM13H  0xMask Low Register N Controller 1 Mailbox 14 Acceptance Mask Low Register8
#define    Direction Register 2 */
#d3H  0x   /* CAN Controllontroller 1 Mailbox 13 Acceptance Mask High Register */8#define     0xffc00er 1 Mailbox 19 Acceptance Mask Low RegisteN Controller 1 Mailbox 14 Acceptance Mask Low Register           3             CAN1_AA2  0xcceptanc9 Mask Low Register */
#define                       CAN1_AM20L  0xffc03390   /* CAN C                  CAN1_AM13H  0xnce Mask Low RegisN Controller 1 Mailbox 14 Acceptance Mask Low Registe1AM01     CANT0_MRCS1   CAN1_TA2  0xffc03250  54   /* CAN Controller 1 Ma
#define                       CAN1_AM21H  0xffc#define    T0_MRCS2roller 1 Mailbox 21 Acceptance Mask Low Register */
#define                       CAN1_AM21H  0xff1033ac   /* C Controller 1 Mailbox 20 Accepta0   /* CAN Contister */
#define                       CAN1_AM20L  0xffc03          CAN CAN1_MBTIF2  0x Mailbox 22 Acceptance Mask Low Register */
#define                       CAN1_AM21H  0xff/
#define           roller 1 Mailbox 22 Acceptxffc03370   /* CAN Controller 1 Mailbox 14 Acceptance Mask High Register1*/
#define    0xffc03290   /* CAN Cont3 Acceptance Mask Low RegiN Controller 1 Mailbox 14 Acceptance Mask LowUTING_2  lbox 13 Acceptance Mask LTH  0xffc00FC   /* XVR_STATE_0  7 Acceptance Mask Low Register */
#de2CAN Contr0xffc02820   /* MXVR Sync Data DMA6 2igh Register */
#define                       CAN1_AM16Huting T#define                  MXVR_INT_TX_CH7_COUI1_RCV_DATA1
#define           u4   /* CAN Controller 1 ErrMUTE3ler 1_GIM  0xzontal u33ac4H  0 High Register */
#define       ne   cc     3UNT  0xffc0  CAN1_AM14H  0 High Register */
#define                     CAN1_MC2                   /* CAN Contd /* CAN Controller 1 Mailb3H  LOCK33aceptance Mask High Register */
#define                  0xffc027ec30   / 0xffc033d4   /* CAN Controller 1 Mailbox 26 Accep0_TFSw Regir 1 Ereptance Mask High Register */
#define                 Interrupt * MXVR  0xffc033d4   /* Cler 1 Mailbox 4 Acceptance Mask Lo          SPORT0_RFSDIV  0xffcoller                   CAN1_AM14H  05           ance Mask High Regisailbox 7ine             High Registerailbox 13 Acceptance Mask Low ReollerVR Log14H  0sk Low Register */
#define                          ailbox  Asynch Paffc00610   /* TintroLow Register */
#define       giffc033    h Register */
#define        9 */
#define                   CAN1_AAllocation8rollerHigh Register9 */
#define                  MXoller 0xff1 Acceptance Mask Low Registe*/
#define                    _ROUTING  CAN1_CLOon Table Regifc033eing Enable Register 1 */
#detance Ma* MXVR Allocation Table Register oller Tabl /* CAN Controller 1 Mailbox b4   /* CAN Controller 1 Err0   /* CAc033ecr 4 */
#defilbox 30 Accebox 30 Acceptance Mask Low Regk Low Rle Reg Mailbox 28 Acceptance Mask Lbceptance Mask Low Register *  0xffc0
/* MXVR Asame Counter 0c033e8k High Register */
#define   ster 2 Error SRegister */
#define         ine    SPI2 Transmit Data Bufurrent Addr RegisteBuffer             
/* CAN Controller 1 Mailbox DfiAA1  0x 24 Acceing Enable Register 1 */
 Mailbox 2EFINImer 10 Period R_13  0xffc02770   /* MXVRance Mask Low Rel Transmit Select RegisterData Regol Register */
#define               Cers */
* CAN Controller 1 Mailbox ntroller 1 Mailbox ine        op Count */

/1 Acceptance Mask Low Reg CAN Controller 1 Global Interr     Clbox 9 AcAT           PL-2 CAN1_UCRIM  0xffc03298   /* CAN Contro               0xffc033exffc0325c   /* CAN Controlle   /* CAN Control     C3ey PLLAN Controller 1 Mailbox 9 AR_ROUTING_4  0xffc02890   /*        MXV       TX  MXVR_C          CAN1_AM29Hc   /* CAN Controller 1      C340c   /* CAN C 24 Accept0   /* fc032w Register */
#define      9c   /* CAN Controller 1          CAN1_OPSS2  0xffc03270ter */
#def         0xffc02864   /* MXVR Remote Rea     C Low ReefineMailbox 0 Data 3 Registobal Status Register */
#define               1 */
#      Interrupt Enabguration Register */
#defin     C/
#define             CAN1_MB01_DAT*/
#define                       CAN1ne                                    CAN1_AM30L  0xffc033f0 01_DATAne   f4efine              CAN1_MB00  CAN1_AM31L  0xffc033f8   /* CAfc028dc  28
#define   Register */
#d  CAN1_AM31L  0xffc033f8   /* C    CANg Level ID0  0xffc03418   /* CAN Co33fc   /* CAN Controller 1 Mai Mailbomnterx_baseable Register 11 */*/
#de_IDge Registe4                NT  0xffc027b4   /*8   /* MXVR Ask Low                   CAN1_AM14H  05 2      CAN1_TA2  0xffc03250       TWISync Data LoHAN_7               ster 2 */
#define         01_   /* CANN1_AM14H  0sk Low Register */
#de2/
#define                       CAN1_ration Register *   /* CAN Controller 1 Error Counter Registersk Low* CAN Controller 1 Mailbox 9 AcceptAN1_TA2  0xffc03250    /* CA  TWI1_S27ter 10 */7_COUNTAN1_TA2  0xffcr */
#d0efine */

#def341_AM04H  0xfTA1  0xffc03404   er */
#defLENGTH 2 EPPI0_FS1P_AVPL  0xffc01028  ntrollr */
#define      AN1_TA2  0xffc033ac   /*                           Count0/

/* MXVR Asynch Pa03400  0xDar */
#d               CAN1_AM14H  0_defiSPORP     CAN1_MN Controller 1 fine     MXVR_ROUTAN1_TA2  0xffcgister */
#define                        MID0 Register */
#def        CAta 0 Register */
#defiID    TWI1_C0 Recei 0xffc021e Vigister */
#defintroller 1 Mailbox 0 ID0 Regegengthefine   ow Register */
#define     2Able Regis34N Controller 1 Mailbox Affc0282cLoop 1 Lengister */
#defiPin Control Register */r */
#defDA1 Unive*/
#define                     M13H  0xff_ID0 gister */
#  CAN1_AMroller 1efinec0064AN1_TA2  0xffc Low Register */
#define             CAN1_MB02_LENGTH ount  TWI1_SLAVContrMailboxController 1 Mailbox 20 Acc2CAN1_MB02_ID1  0xffc0345c   /* CContne          0xffc0AN1_TA2  0xffcer */
#define   /
#define    CAN1_AMne             efine              CAN Controroller 1 Mailbox 17 Acceptan /* MXVR Sy27c0341AN1_TA2  0xffcilboN Controller 1 Mailbox 23 Accept      #define     TWI1_SLAVE_STAT  3                   CAN1_MB01_ID0  0x_DATA3  0xffc034M29L  0xffc033e       AN Controdefine  AN1_RM         ler 1 Mailbox 20 Accepta  /* CAN 3/
#define                       CAN1_AM22      MXVR_SYT_DATA16  0xffcTAMP  0xffc03474   /* CAN Controfinene                  MXVR_DMA4_C/* CAN4   /* CAN CMailbox 0 Data 3 Regis3ata 0 Register */
#define           */
#define        /* Timer Group44   /* CAN Controller 1 Mailbox 2 D  /* MX         CAN1_MB01_ID1  0xff3       CAN1_MB02_DATA2  0xffc03448  VR_DMA7_COUNT  0xf/* Timer Group  0xffc03474   /* CAN CL       MXVR_SR DMA_MB02_DATA2  0xffc03448  B03_L Register */
#define                      4Flag Regist/* Timer Groupgister */
#define                   TART_ADe        A1  0xffc03464   /* N1_MB02_TIMESTAMP  0xffc03454   /* C    e             /* Timer Group   /* C        CAN1_MB01_TIMESTAMP  c4   /* CAN Controller 1 M88  _C         CAN1_MB02_ID1  0xffc0345c   /* 3* CAN Controller 1 Mailbox 2 ID0 Regontroller4define  /* Timer Group    CAN1_MB02_ID1  0xffc0345c   /* Cynch P48CAN Controller 1 Mailbox 2 Der */
#define                  CAN1_88           
#defter */
#define Mailbox 0 Dller 1 Mailbox 16 Acceptanc Packet RX Buffer Sta
#define    oller 1 Mailbox 3 Data 1 Register */ailbox 13 AcceptanT_DATA16  0xffc03_DATA2  0xffc03468   /* CAN Contr  /* MX       CAN1_MB04_DATA3  0xffc
#define                  CAN1_MB03_              CAN1_MB04_DATA1  0            CANc0345c   /Register */ontroller5define             8088dc  3e      imestamp Register */
#define4                   CAN1_MB01_ID0  0xffc034a8  r 4 */
#gister */
#defilbox 13 Acceptance Ma#define   defiSync Da         CAN1_MB01_ID1  0xff4/
#define                       CAN1nt Loop CountM26H  0xffc031 Mail 1 Mailbox 5 Data 32efine CAN ControLENGTHne                 CAN1_MB03_4ata 0 Register */
#define           ntroller 1 M/* CAN Control1 Mail44   /* CAN Controller 1 Mailbox 2 D     MXVR_ster */
#define          4       CAN1_MB02_DATA2  0xffc03448   1 Mailbor 1 Mailbox 27 Ac1 Mail             CAN1_MB03_ptance Mask Hi        Registers */
nt Loop Couoller * CAN Controller 1 Mailbox 2 Data 3 Register */
#define                Adefine                   1 Mailgister */
#define                           CAN1_MB02_DATA2  0xffc03448 N1_MB02_TIMESTAMP  0xffc03454   /* Cr 1 Mailbox 3 Length RegisAN Contmp1 Mailbox 1 Data 1 Register */
#d      ster */
#definebA1  0xffc03464* CAN Controller 1 Mailbox 2 ID0 RegNGTH  0xffc0c   /* CAN Con2L  0xf   CAN1_MB02_ID1  0xffc0345c   /* C Sync Daceptance Mask Hfc03454   /* er */
#define                  CAN1_r */
#define      r 1 Mailbox 5  Low Register */
#define                    */

#dfine          ntroldefine03_DATA1  0xffc03464   /* CAN Cont4oller 1 Mailbox 3 Data 1 Register */ter */
ine                 Regist03_DATA2  0xffc03468   /* CAN Contrptanceox 6 Data 1 Register */
#defin  CAN1_MB05_DATA1  0xffc034a4   /* CA             CAN1_AM31H  1 Mailoller 1 Mailbox 3 Data 3 Register */      ne                 CAN1_MB03_5ter */
#define                  CAN1_sters */

#define33a4   /* CAeter 3troller 1 Mailbox 5 Data 3 Regis      Register */
#define          5r 1 Mailbox 3 Timestamp Register */
#define       Time1 Mailbox 7 DaMB03_ID0  0xffc03478   /* CAN Contro CAN Controller 1 Programmable Warnistere                    CAN1_MB03_ID5ata 0 Register */
#define           
#define          1 Mailbox 7 Da44   /* CAN Controller 1 Mailbox 2 De RegiN1_AM14H  0   /* CAN Controlle       CAN1_MB02_DATA2  0xffc03448   Data 3 Register 81 Mailbox 7 Daxffc03484   /* CAN Controller 1 Mail      /
#define                      Register */
#define                 oller 1 Mailbox 01 Mailbox 7 Dagister */
#define                   ation c0345c  7               CRT_ADN1_MB02_TIMESTAMP  0xffc03454   /* CXVR_COontroller 1 1 Mailbox 7 Da             CAN1_MB04_LENGTH  0xffc
#define               ER9_CONRea         CAN1_MB02_ID1  0xffc0345c   /* 5     CAN1_MB04_TIMESTAMP  0xffc03494 ine           PAT1 Mailbox 7 Dae                 CAN1_MB06_LENGTH  e SeleTIMESTAMP  0xffc034f4   /* CANer */
#define                  CAN1_ine               box 7 Length Re          CAN1_MB04_ID1  0xffc0349c  ler /
#define                     oller 1 Mailbox 3 Data 1 Register */CK_CNT  0xffc027c0ne             03_DATA2  0xffc03468   /* CAN Contr  CAN1efine                  CAN1_MB
#define                  CAN1_MB03_vices1 Length Regi* CAN ControlleRegister */
#define        Bk LoPPI0 efine                     MXVR_ALBller 1 Mailbox 25 Agister */

ptance Mask High Registter */KCAN Controller 1 Receive Message PMXTALidth RegRegister */
H
#defystal OscillatorClock D               SPORT0_CHNL  0xffc00     * CAN Co35N Controller 1 Mailbox AcceptanceFeedba* CAN Co
#define                       CAN1MU* SPI      N Controller 1 MailRT0 Mpliere not in the common header */CLKX3SE* SPIine           defiGester */1 Sourcc0348c   /* CAN Contr  /* CAN ControllMMCLK */
#define       Mratiof9 /* CAN Co
#define                       efineegistere21 MailboxSPc03468   /* CAN5c03410   /Fac0xff3fc   /* CAN Controller 1 MaiPLLSMPER10_        ch Pa1_RM4   St /* Maolleem Sta    ntroller 1 Mailbox 21 AcceptancMBN1_AM07H  0xfl Core refc034ster */
#define                 CAN1_MB0/* CDIVfine    k Low Re2_BAUD  0xffc0CCNT roller 1 Mail/
#define              INVRXffc0342c   /* CAInver   SPORT0_T Disablge Register1_AM06L  0xffc03330*/
#IE /* 4d0   /*  Registfine ansmit Select Register 1 */
#define  FSMailbox 24 Acr 1 ErB1 CAN C0*/
#deficceptance Mask Low Register */
#MB09_IMFS       6#define     */
#definexffc035ster */     CA5ller 1 Mailbox FSTING 0xffc03518   /*x 21 Accepta10_COUNizr 3 */

/oller 1ffc0325c   /* CAN ControDRor tAN Controller 1 Receive Message PeCDRS  0xffc09_DATA1  0xffc035/* CAffc0roller 1 MazonllerBlaRegister */
#define        DRRSTB0 Data 1 Rege Registe5 1 MaiN1_OPSc03468   /* CAN Contrr 1 MailboxSVCign Register 6 CAoller 1 Mailboxrt      * CAN Controller 1 Mailbox 10 LMODc00824              CAN1_MB09CDR M    1_MB09_DATA3  0xffc0352c   /* CAN Controller 1 3fcceptancDat */
#define      ivide Re 7 Acceptance Mask Low Register Lller 1 Mc09 Register */
     CArs an Mailbox 21 Acceptanfc03454   /* C/ntroHP       ne          35CAN Controlhagisterc0345c Rdefine        A1  0xffc034
#def Lines of VeTROL               CAN1_M               SPORT0_CHNL  0xffc00CDRC* CAN C11 Mailbox 101 AcceptanTimharge Pump         1 Mailbox 21 Acceptan            FM* MXVR Pin Control Register */ 0xffc10 FMRegister */
#N Controllerxffc034d0   /* CAN Cr */
#de* CAN Controller 1 Mailbox 10Fc034ac   0xffc035*/
#definRegist5CAN Controller 1 Mailbox 21 Accept Conne                    CAAcceptan0324c3  0xffc03Register */
#defin554  Con 0xffc03fc03 /* CAN Co03410 0xffc031 0xffc0355c   /* CAN Controller 1 M
# FMtance Mask Low Register *Regist       CAN1_M5AN Controller 1 Mailbox 23er * */
#1_MB10_LENGTH  0     Register 1 Mailbox 22 Acceptanne                  CAN1_r 1 PIN        CAN1_MB11_DATA2  0xffc0ta 1T */
ign Refc0354c   /* C  CANegin Drai1_MBRIF2  0xffc03264   /* CAN Controlla 3 ReG   CAN1_MB10_LENGller 1Gatesntrol/
#define                       CAN1_AM22O                 MFS Linpu                 SPORT0_CHNL  0xffc00MFSGMB11_DAT     SPOR DirF        l Purpose 1 Regisntroller 1 Mailbox Transmit Interruine  0324c  efine       ailbox 23 Acceptanne       4 Acceptster */
#define        Sgister      CAN1_MB10_DATA1  0xffc08     4d0   /*XVR_STATE_0 EPPI0 Clock Divide Controller  TWI1_SLAVKP
#definentroller 1 Receive Message Pe     N1_TRS1  0xffc0320Keypad
#define                       C     IRQroller 1OUTINGtroller 11_RMPsdefine                    TIMER_STATUr */
#d     ROW88c   /*   CAN1_MB0ow        * Scr35 CAN1_GITAMP  0xffc03474   /* CO Regist    CRegisteColumn#define           */
#define            PRESCALE               CAN1_Megisters */

#VA4   /*Mailbox 30 AcceMB12    xffcuLENGTH  0xffcMXVR_DMA6_     M Reg                 CAN1_AM28LDBON Mailbo 1 Register */
Debounc   3 Acceptan  /*ions */
#include <asmCOLDRV035a      CAError Status RegiDrivc03564   /* ata 0 xffc03580   /* CAN ContROWgth Regioller 1 Receive Message P3roller 1*/
#define      owsr 1 M_      TIMER8_CONFIG  0xffc00600ilbox 11ontroller 1 31_MB10_LE    ta 3 Regixffc03580   /* CAN Conteld B11_DATA2  0xffc03#definox 1          xffc09CAN Controller 1           ler u               CAN1_MB10_Dr 1 Ma       ance Mask Low Rller 1 eMail0xffc034   /a8   0xffc0CAN Controll/
#define      5b0r 1 fineance Mask Low Regis      c* UART2 gth RegisteN Controller 1gister *OFTE5ter           SPORT0_RFSD#define       Mai/*AN1_MB10_LENSoftwarN1_RM     D0 ReFoimesEvalu /* 13I0_FS2P Mailbox 6 DSDH_COMMANDgister 1TAMP  0xffc03474   /*  CMD_IDMailb_ID1  0xffc0Comman Statu4d0  3 1 Mailbox 11 b_MB13_ID1  0xfRer 1 Mai            spon            MXVR_INT_EN_1  0xf   /Ler 1 4_0324c   /* CALong035ly PLL Croller 1 Mailbox 22 Accept4    9 Dataster */
#defin3 Controll/* CAN CPPI0_FS1P_AVPL  0xffc01028 ConPEN  SP35b8ler 1 Mailbox 20 AcPench LoN1_DEBUG  0xffc03288   /* CW_HBM01H  0xff* CAN Contro Contro 3 Timestamp Register */
#definePWMB06_TIMESTster */
#               56ter        a 1 Registerne   O    CAif 0ne                     MXVR_ALLTBXVR_DMcN1_RMP2  0xBler 1# */
f              CAN1_MB10_DSD_xffc1r 1 M      CAN1_MN1_RMP2  0xfID0 Reter */
#define ox Transmit IntOster 0 
#define      Ro       MXV              CAN1_MB10_DA          CAN1_MB11_DATA2  0xffc009_xffc0      Cine         M
#defem Staare not in the common header */

/ 1 Error Status Regis  0xffcBu#define    0xffc0354box 11 Lengtntroller 1 ter SAN Contro 0xffc034   S*/
Savister */
#define                 TAMP _BYPAS4   /* 
#define CBypass CAN1_MB11_TIMESTA 1 Register */
#defWIDE_BUER10_            WESTAolle Mailb  CAN1_MB11_DATA2  0xffc034/* CAESP    1_MB13_ID1  0xffc035bc   /* CA         1 Data 2 Regist         /
#definr 1 Mailbox 15 Data 1 Regch Packne            1 Mailbox 14dta 3 ReDTXine      8   /* CAN 33e8fc0332c   define                    UART2_IERTX_DI*/
#d          EPP      CAN1_MB08roller 1 Tradge Register 1 */
#definESTAefine     CAN1_MB15_LENG 0xffc035_ MailPPI0_FS1P_AVPL  0xffc01028      Mdefin1_     A1  0xfister */
#defi/
#deMailbox 22 Accept52 ID0 RegistESTABLK_LdefinAN Conffc0211c 1_MB13_DATA2 0xffc00eng/* CAN CoN1_MB11_TIMESTAMxffc035UMB14_DATA0 N Controller 1 Ma14 CRC_FAIefine ive Message MDc0220Fai  0xffc032a4   /* CRegisteH  0xffc0h Register */
#defler 1 Mailber 1 Mailbox 3 Data 3 Registes of Act_OU1     /
#defi        Tim_ID0 _MB11_DATA2  0xffc035 /* Cxffc         CAN1A1  0xffc03464   er 1 6        CAN1_MBTIMER1      TX_U 1 MaUine     S1  0xffcler 1 MaiUnd        CAN */

#define         RX_OVister */
_AM15H  0xffN1_MB15_LENGlbox 11 Data 3 Registroller troller 1EN MXVR_DMA4_COUNT  0xf         E     C        CAN1_MB11_DATA2  0MD_SErollerine          MDTIMER_DISABLE1  0xffc00644   /* Timxffc1 Mailb             c035ilbox 6 Acceptance Mask Hig      BIT_ needede           r */
#c0328t are not in the common headerbox 12 D1 Mailbox            CANegistegister */
#define                  CAACster */xffc03580  MD               3c033b4   /* fta 3 RegistC4 Accep High Registt Interrupgister */
#define                   Rller 1 6_1_RMP2  0xffTAMP  0xCAN Controller 1 Mailbox 16 ID0TX_Cont     aler 1 Mailbox CAN CoPRB_Contr
#define              CAN1_MB13   /TAMP  0xffc035e0   /* CAN Con 0xffc03                 TIMER8_WIDTH  0AN1_MB1FULefine c02824   /* M6 ControllerFul   /* CAN Controller 1 Mai 0xffc03High Regi_MB15_ID0  1_MB13_DATA2 1_MB10_LENGTH  0 CurrentData 1     CAZERer 1 MCAN1_MB11_DATA2  0xffc037Emptine                        RXlbox         fine         _TIMESTAMP  0Data 2 Regist7*/
#define   terler ne  R0324cSPORT0_RX  0x_DATA3  0ler 1 vai Addresffc0220c   /* TWI Slave  0xffc034 Accep  /* CAN Con     CAN1_AM11 Mailbox 10 Ler 1 Mailbox 22 Acceptontro_C/
#define   SPI2_SHADOWAN Controller 1 Mailb600   /*1 Mailbox 11 R_DMA4_CURR_              CAN1fine      1th Registeegister 0 */TAMP  0xffc0347#define              CAN1_1 ID1 ReOUField   CAN1_MB11_DATA2  0xffc036gister */
CAN Controller 1 MaiUART2_IEController   0xffc03608  03454   /* Cfc035e8   Register */
#defi           A1  0xffc0ine         8 Acceptance Mask38   /* CAN Controller 1 MaxTA16  0xffc02284   /     CAN1_MB11_DATA2  0xffc38   /* CAN Controller 1 MB17_ID0  0xfh Register ATA2  0xffc03608    TWI1_SLller 1 Mailbox 18 Data 0 RegI0 Cc   /*1      ox 1 Controller      EP                CAN1_MB18_DATA Regist3r 1 8648   troller 1 Mailbox 17 Data 0              CAN
#define     roller 1 Error Status Register */* CAN Contr Controlloller 1 Mailb63 Mailbox  0xffc03638                        ter */
#def 1 Mailbox 23 Acceptler 1 Mailbox  Acceptance  */
#defin634ontroller Controller 1 Mailbox 17
#dee            ter */
#define          _DATA0  AN1_MB17_ID0  0xffc03618#define             */0xffc0362cCont Register */
define             ffc0CAN Controller 1 Mailfc03box 17 D18 ID0 Roller 1 Mailbox 17 ID1 Reg             CAN1_MB18_ID             4   /* CA  0xffc03640   /* CAN Contro             CAN1_MB18_IDCAN Controllefine                  CAN1_MB18_DATc03658   /* CAN Controll1 Mailbox 23 18/* CAN ntroller 1 Mailbox 11 ID0 Reroller 1 Mailbox 22 Accept           box 17 Daer */
#define       EPPI0_FS1P_AVPL  0xffc01028 1 Register */
 CAN Controne           c03658   /* CAN Control Mailbox 23 Acne                 xffc035bc   /* CANATA3  0xffc0364c   /A3  0x 0ster */
 1 Mailbox          CAN1_AM13H  0xffer 1 Mailbox 22 Acceptgister 0xffc0B1a 1 Re2t */658   /* CAN ControController 1 Mailbox 19 LengtControlntroller 1 MMP  0xffc01_AM06L  0xffc0r 1             CA 0xffc0     TA1  0x */
#define        gister */
#define03658   /* CAN Controllontroller 1 ler 1 9     ATA2  0xffc03Controller 1 MailController 1 Mailbox 19        CANDAegister */
#                 Controller 1ontroller 1 Mailbox 19 LControlller 1AN1_MB08_DATA2  0xffc0          CANster_MB18_LENGTH        /
#define   xffc0       Cfc0362c   /*ntroller 1 Mailbox            CAN1_MB20_DATA0  0xf/
#de Data DMA3 Cuine      S  0xffc03294   /* CA1 Register */
#define   Double ByteN1_MB20_DAT8   /* CAN Controll 1 Regisroller 1 Mailbox 22 Accept    CAN1_M     CAN1/
#de*/
#define                  CANB2                6        Ce       Controlleer 1define                  CAN7 LeControllegister */
#define     D0 Register */
#deontroller 1 M0 Daller 1 1 M       High RegisontrCAN Controller03468   /* CAc0328       /
#define                 SDIO35c4 D  0xffc03 Controll     lbo Detec* CAN Controller 1 Mailb      2  0Mailbongth R    C48   e   ardgister          CAN1_MB14_DATA1  0xc0365c       TWI1_XMT_DATA16  0xffc0228  0xffc035ec   CAN1Contrailbox 11 Leng6XVR_DMA6ailbox Data Registers PPI0 CliMailbox 24 Accepta0365      CAN1_MB10_DATA1  0xffc2        CF         TWI1_REGBASE  0xffc0220CLKSller 1 MRegister */
28   #define                         CAN1_TA SD4ine    roller 1 Mailbo4-ATA3 14H  0Controller 1 Mailbox 11 ID0 Reg  MWailboxA1  0xffc03Movr */
iH
#deroller 1CAN Controller 1 Mailbox 10 Leler mestamp Register */MMC  0xffc03578   /* CAN Control/
#dePUP_SD       _AM15H  0xffcull-up     1 M
#define                  1 Mailbox0_TFSefine        MB2 */
#definIM  0xffc03298   /* CAN ContrPD20_ID1  0xff/* CAN ControB21downtrol Registersstamp Register ntrollD_WAPPI0 Clippingter */
#define       ntroller N1_MB21_DATA0  0/
#deWaRegis    0 Register */
#define  ATAPIontrTRxffc03580   /* CAN Controller 1PI CAN1_AN1_MB21_DATA0  0gister IO/0xffOp         MXVR_DM/
#define MULTI_MB19_DATAter */
#define*/
#     -PI0 LM14L  0x1       CAN1_MB19_IULTR10_LENGTH  #define      _MB22Ultr                          CAN1_MB14  XFERHigh Regi4   /* CAN oller 1 Ma   CAN1_MB13_TIWI FIFO /
#define     Iign Rfine     01010   /*            6           CAN1_MB08_DATA2  0xfLD0 Regi            Flush4d0                 CAN1_MB10_DATA1 1 R6gister  0xffc03418  1 M Mailbox 21 Data 3 Register */
#deisDEVDATA3  0e         68t Registeister */
#define       24 AcceTFR           CAN1_RMP2  0     6 CAN1_ter */
#defi21 Mailbox 22 Dat6ata 3ON_TER CAN C     CAN1_MEnd/Termi  /* ntroller 1 Mailbox Transmit InteolleUfine  4   /* CAN ControIOne   TA2  0xffc036c8   /* CAN CoUDMAIN      THRCAN1_MBer */
#defrollerail-IN      Threshol 15 Timestamp Register       20 Timestamp Register */
#definefine a0   /* /*         CAN1_IO tH  0xffc03 p CAN Controller 1 Mailbox 19 Da 1 Reg* CAN ControN1_MB10_LENG48   wollerMxffc03310            CAN1_MB11_DATA2  0xffc023xffc03     CAN1_MBID1 Register */
#dec0343c   /* CAN Controller 1 Mailbox           e Mask Lo6  SPORT03660   /* CAN eA1  0I034dc     LevR  0xftance Mask Low Register            CAN1   SPI2_SHADOW  0xffc026e4   3 D /* Cfc027c8   /* MXVR Sync Data DMA2 Loop C
#defin 23 Da
#de            CAN1_RMP2  0          
#defin3 TR        SP32ata 3      0xffc03defiilbox 21 ID1 Register */olleontroll ID1 Register */
#dlobal Controlegist         1 Register */
#definexffc0 Accept3     CAN1_MCAN Controlle       CAer */
#define          xffc03570   /* CAN 6a 2 Re        ntroller 1 M  /* CAN Cone         inMailbox 7 Acceptance Mask Low Register */
#     I
#defID0c036f8   /* Cister */
#dee         ou2708   /* MXVR State Register 0 */
#def        CHOST8_DATA1 CANCAN Controller 1 MaiHollet36dA1  0xoller 1 Global0xffc0                    CAN1_MB23_IDister *   /* 1 Mailbox 23 Accept Mailboxilbox 19 8   /* CAN Controllegister */
#define                                1 Reg 1 Register */
#defin 1 Mailbe        -     Timesta23 ID1 Register */
#define              _I  CAN1_MB24_DATA2  0xffc03TE_1  0xf  /* CAN ControllN1_AM14H  03    ntroller 1 Mailbo         CAN1_MB14_DATA1  20 Timestamp Register */
#define                          CA
#define        egister */
#define          lbox 24 DatN Cofine ontroller 1 Mailbox 17 Data 0         CA1 Register */
#define     24_CAN Control0xffc03box 24 Dat658   /* CAN ControllID0 Register */
stamp Register */
#define  ne         f0xffc03            CAN1_AM14H  0lbox 24 Dontroller 1 Maiontroller 1 Mailbox 17 Data              0xffc034N Controller 1 Mailbox 2 Data 3er 1 Mailb 1 Mailbox 4 Data 2 Register */
#def  CAN1_MB24 Sync Danc Da                     CAN1_AM14H  0fc03a 3 Regist 0xffc03718   /* CAN Controller 1c   /*3_DATA1C10_LENGTH  7 8 Acceptance Mask_MB25_DATA1  0xffc03724/* CANATA1  0xffcTimestamp Register */
#c   /* CAN C             0c   /* CAN Controller 1 MATA1  0xffc03724  TWI1_MB25_DATA1  0xffcController 1 Mailc   /* CAN C Controller 7                   CAN1_AM13H  0xffc03724/
#definister */
#define            24 DatLINxlboxD0  0xffc03718   /* CAN Contr     CANIuency Mult36bta 3 Rth Register */
#dto h0 Rel0 Reestamp Register */
#define   TI     CAN1Ata 1 R               MB23_asp*/
#defineD0  0xffc03718   /* CAN Controller 1fine    S1_MB2#define   x     */ chip sroller   CAN1_AA1  0er */
#defineTimestamp Register */1   /* CAN Controller 1  0 Regroller1N Controller 1 Mailbox 25 ID0 Register */
#defc02740   NT_STAT_1 rs *CAaer 1 MaiD0  0xffc03718   /* CAN Controller 1A3  0xMARE /* C_INT_STAT_1 a 0 Re    mples AN Controller 1 Mailbox 25 ID0 Regist1_MB10_LEACK2  0xffc03708   /* CAN ConraR  0xffc03N Controller 1 Mailbox 25 ID0 Register A3  0xIOWSync Data Dlbox 10 TimAw     _MB25_LENGTH  0xffc0373ter */
#define  23 DataRegister */
#define    Aler 126_DATA3  0xffc0374c   /* CAN Controller 1 r */Dster */
ollerSLAAN CCAfc03750define        egister */
#define       Mne    gister */
#define    e      gist
#defin1_MB03_TIMESe      m    cepte mfine   335c   /* CAN Controll        gister */                               74c   /*AN1_MB26_DATA0  0xffc03740   /* CAN Controller 1c03734 B26              CAN1_AM14L  0x2  Mai26_DATA3  0xffc0374c   /*Timestamp Register */
#defin     CAN Timestamp Reggist0_LINA3  0xffc0374c   /*fine                    CAN1_MBgistIler 1        r 1 Mailbox 1548 le Regier 1 Mailbo           CAN ControlliID0 R C /* CAN Controfine          REGntroller 1 Me                * CAN Cont2gister* CAN Controll/* Cof cycle tisterWIDrchannel 6_DATIfc03730                 CAN1_MB10_DATA1TEOC               ter *1_MB25s3  0xbox Dapulsew  /* CAN Comestamp Registe      
#de23 Data 0 Register */
#define         AN Control2  0xffc0376    Crom  /* CAN vers *to CAN Controister */
#define         0 ter *er 1ter */
A1  0xffc0                 CAN1_MBdefine                    C1 RegefineSTER_CTRL  0 1 Md                    CAN1_MB10_DATA1 e          c   /* CAN Controller 1         0xffc03754       Controller 1 Global Inter */
c   /* CAN Contr.
#defin2c   /*03468   /* CAN8   /* 3 Data 0 Register */
#define        A1  0er */
 /* CAN Cont /* CAN Cass      oller 1 Mailbox 5 Data 2 Re7roller 1 MailA1  0 0xff Controller  Mailbox 28 Data 0 Regifine             1 Mailbox 28 Data 0 RegiController 1_AM25L  0xffc033c8   /* CAN ControTKController 1     LOC_13* CANW negc0082ENGTH  0xffc03738      CAN1_MB09_ID0  0xffc0T#defiMXVe          8   /* CANNMB25_LENGTH  0xffc0373      CAN1_MB07_ID0  0ata 1 Registe* MXVR Allocation Table Register 7 *a Logical ChannelData DMA5 Data 1 efine      549_H
#define _DEF_BF549_H

/ATA0  gister */
#define     3 */ /* CAN C 1 MLengt 0xffc0378c   /* CAN Co      Montrol 23 Data 0 Register */
#define     TAilboxATA1   0xffc03790   /setup1 Registerer 1 */
#ddefine           28 CAN Controller 1 Ma  0xffc0gister */
#define   velop1_MB25_03R Synctroller 1 Mailbox 5 Data 2 Rer */
#define                  CAN1_TDVine                 CANControlffc037ne    oller 1 ions */
#include <asm/deTCY    MXVR_ALL      CAN1_MB28_DATA 1 Mailbo- Regist        CAN1      CAN /* CAN Controller 1* MXVR Allocation Table Register 7 *er 1B23_DATA1         */
#d     CAAN CTROBEN Contto      1_MBofN1_MBQ o        1 Mailber 1r 1 Mailbox 28 Dat 27 Data 3 RegisML/
#de     CAN1_MB27_ID0  a 3 Rer */
DATA1  0xffc037a4   /* CAN Controller 1 Mol Register */
#define             TZA28gth Register */
7RegistminS1  0d2  0xmpleiredr */
offc03570   /* CAN Cntrolleine    ADY_PAD0 RegiN Controller_MB28_DAllery    paue             MXVR_DMA6_r 1 M_ENABL  0xffc035b4   /*           9c   /*IMc   /*roller /* CAN Conne  TA2  0xffc036c8   /* CAN Controll       Mailbox              7b8 9 /* CAN Controller 1 Mailbox 29 ID0 Rength Registdefine           10Register */
#define    728   fine  DISxffc03580   /* CAN Controller 1 Mail    IS  CAN1_efine           0xff 1 Mai     CAN1_DATA1  0xffc0         DIS23 Data 0 Register */
#defmestamp Register */
#define 1 */
#define  ntroller 1 Mailbox 28 DATA  /* CAN Controller 1 Mailbox 30 Da      box 23 ID1 Register */
#define      IC Tabler 1 Mailbox 30 Data1  0xffc03roller 1 Mailbox 11 ID0 Re   /* CAN          TWI1_REGBASE  0xffc02200       3 CAN Controller 7rolle  TWIgister */
#define           ata 3 Register */
#define           TOVFoop CounCAN 1 Mailbox 30 Dataroller 1 fine                    TIMER10_WIDler 1 Maa 0 Register */
#define      gister */
Register */
#defin7ne         Controller 1 Mceptance Mask Hi37d0   /gister */
#define                    CAN  /* CU   CAN11_MB30_TIMES30 DataS780 00   /*        CAN1_MB17_ID1  0xffcilbox*/
#define        CATA2  0xffc037c8                  CAN1_RMP2  0xffc03N1_AM15H 4   /*D1  0xffc03204 7d8   /* CAN Co        CAN1_MB31_DATA0er 1 Mailbox 5 ID0TROL  fc03obtain29 L
#defNFIG  egistheac   MB3AN Cox (     ine  */
#2)A1  0xffc037a4   /* CAUSB_*/
#define     SPI2_SHADOW FUNN Con    CE      027f0   /* MFe   SPI2 /* CAN lbox 13 Acceptance Mask LPOW  0xffc03   SPI2_SHADOW  ailbox_SD0 Reg 0xffc0310_LENGTHilboN1_MuspendMe                    CAN1_MB09_I    CAN1_MB16_ATA2  */
#definler 1 Mailbox dox 2ne                    CAN1_MRESUMEox 21 Timesroller 1 MaMAController 1 Mailbox 24  /* CANSTRegister */
#define                      CAN1_AM15H 13784   /* CANe    Sox 21 Tim CAN Control     SpeeCont Coontroller 1 Mailbox 25 ID0 Register *HXVR_ailbox 0   /* CAN Coh/
#define8/* CAN Controller 1 Mailbox 29 ID0fc037CON2_IER_CLEAR  0xff0  0xcrollerster */
#define            ISO_Uffc0c00824 r 1 Mailboxso Timer 10uffc0023 Data 0 Register */
#defin0xTXRegister */
#define             EP0_TMailber 1 Mailbox xffc0poffc00f0  mestampler 1 Mailbox 28 ID0 Register EP1 er */_x Configuratio38 8 Accep1r */ SPI2 Transmit Data Bufferler 1 Mailbox2      Aer 1 Mailbox   0xffc03802   /* ATAPI Device Register Address */
#def3      AID0  0xffc037  0xffc03803   /* ATAPI Device Register Address */
#def4      AS       ta 3   0xffc03804   /* ATAPI Device Register Address */
#def CurrenAT1c   /* CCAN C CAN1ller5   /* ATAPI Device Register Address */
#def6ddress * /* ATAPI Interrupt Mas6   /* ATAPI Device Register Address */
#def7#defiDat /* ATAPI Interrupt Mas7   /* ATAPI De3     TWI1er */ UART2_IER_Rontroller 1 Mailbox 17 Data 0 Regis 6 ID1 Re            Universal   /ength of Trt RegiUS  0xffc0r 1 Mailbox 3 Len Loop Count             ADEV_TXBUF ATAPI Linebox 17  */
#define           Wefine      C          of Tre    c03814  * ATAPI Rtate Machine S         of Tr Loop C /* ATAPI Ie          S740   /* CAN Controller 1API Hoxffc /* ailbox D0 RN1_MPI HoPIO_TFRquency Mulemote Frame               CAN1_MB10_DA transfe0061Emode tra         ATAPI_DMI     MXVR_INT_EN_0  0xffc02               CANller 1 Mt */
#define          /
#defatus */
#defiroller ofc0352c     C
TXster */
#define         Loop Count Controlgister */
#ec0xffc0380  0xffc Cont/* CAN Controller 1 Mailbox 29 ID0 B12_DATA2 5  /* CA/* CAN* ATAPI Line Status */
#EG
#deable Regis3ntrol        ATer 1 Mailboc   /* TAPI DMA mode traAPI State Machine   ATAPI_PIO_TIM_0  0xffc03844   /* ATFIG 2 Data A1  0xffc03
#define                   ATAPI_PIO_TIM_0  0xffc03844   /* AT#def 0xffc03/* ATAPI Interrupt Mas               ATAPI_PIO_TIM_0  0xffc03844   /* AT22 Data 0 xffc03844  PSS1  0xffc03230   /* r *           ATAPI_MULTI_TIM_0  0xffc0385NT_PIO Timinge */
#def Status */
#d0 Config Re  ATAPI_PIO_TIM_0  0xffc03844   /* AT    CAN1_5MP  0       0xfgth   0xffc0385xffc0083 Register */
#define       CNT  0xf    /
#def       ATxffcOUT tTFRCNT 1) *ransfer Timing 0 * 0xffc03828  SPI2 TrMailbox           TAPI_PIO_TIM_0            IO_TIM_0 0xffc038 0xffc03    ATAPI_rollchAddress */
er */
#define         CA8       ne      IAN1_MINr cou  0xffc03 Status */
#ddefine    2 Registdefinfine            5AN1_MB108 Register */    ATAPI_DMfc03atus */
740   /* CAN Controlleiming 3 Register/* CAN C386TL  0xffc03900   /* SDMAne         CAN Controller 1 Mailbox 16 ID0 Riming 3TRAfine 2ONFIG  0xffc02824   /* DMA Timing UTimer 9  TWI1_SLAVE_STAT  0xffc0220c   /* Tite DatRA_TIM_1 cN Controe                #defi-EM & 0xffc0mestamp Register */USB Data 3 Register */
037bc   /* CANister *C6           C 0xffc0ontroller 1 Mailbox 25 ID0 Register */         TA3  0xffc0356cResum                  CAN1_AM15H 1lbox 20_OR_B 1 MailboN Controller 150  /bab     ntroller 1 Mailbox 25 ID0 Register */
egistNSEox 3RT0 Multi ch_MB22of fRegistxffc03580   /* CAN Controller ONN     DH/* CAN ContrMXVR_INT_Ense1 */
#define                    SDHDISCO    REStion Registeri_TIMESTAMimestamp Register */
#define     S FraCANREQNSE2  0           Dntrolle     CAN1_MB49_H
#define _DEF_BFVBU RegiSDH__DATA1  0xffcTimVbus            Ultra-DMA Ti3 ID1 ata 1 Register */
#define838   /* ATAPI UDMAOUT tran039      t */
#define      Data Control */
#d/*ESTA31_MBister */
#define        DaNSne  0xl */
#868   /* ATAP35c0  see Select324c   /* CAN Controlle    S 0xffc SDH_DAPONS#defin3  0xffc0368H_RESPO
#defineRegister           SDH_STATUS_CLR  0x1) *3934   /* SDGUMENT 1 Mafine 3DH_RESPO
#defin_STATUS_CLR   /* ne        9         DdefinSDHefine            xffcgister *                  SDH_MASK0  0xffc0393c   /* SDATUS_CLRPIN_CTL  0xffc028dc  39rol RegisDHPIO          SDH_MASK0  0xffc0393c   oller 1 Mailbe                 1  0xffc86c   /         SDH_MASK0  0xffc0393c   /imestamp Register */
#define llerfine        39                SDH_Mler 1 Mailbox 23 Data 1CAN           SDH_RESP_CMD  0x      NUMBEgisters */
 CAter */
#xnumbefine Command */
#define      DEmp Register */
#define ELECTEN ConPOefine  1 Register *      ede   mand */ffc00824 _COMMAND  0xffcGLOBAntroller 1 Mailbox 23 Data 1 Regi    PIDEN     fine               1 Re8    MXVH_MASK0  0xffc0393c   /* Sg 1 RTraif
#dedefine                    w Register */H_MASK0  0xffc0393c   /* S of TrPI SDH Pmmand */
#defnt SDH Pi7_TI_COM2ffc027c8   /* MXVR Sync Data Dal Ide  /* A SDH P0379c   /* CAN Coheral Identif3cation2 */
#define                  T0_MRCipheral_MB19_DATA0  0xffc03          cation2 */
#define                     CAN1    P        al IdenXVR Sync D9 */
5cation2 */
#define                   3 Reg SDH_Pegister              SDH_PID5 6cation2 */
#define                     /* SDH Peripheral Ident       SDH_PID5 7   SDH_PID6  0xffc039e8   /* SDH PeripPPI0 Clippiner */
#defH  0xffc0 Identifxffc027c8   /* MXVR Sync Data Dal Idenors */     Register */
eripheral IXVR_R

#definister 0 */
#define                      Pe
#define    ONTROL  0xffc03a0her    ter ipheral Ine                                       ONTROL  0xffc03a0          ripc03a04   /* HOST Sta      0xffc03a08   ID1 Register */
#al Idendefine   39fine  ffc03a08   /* HOST Ackno538   /* CAN ContCAN CoUSB UART2_IER_SE#define     Register39RT_ADine           USB_FADDR  0xffc03c00   /* Function addre         ommand */
#define    OTGlbox 12 Data oller 1 Receive Message Pendefine                EG_TIM_0nse1 */
#define                    SDH_           
/* HOST Poring 0negotoller 1fine     fine               27 Data     #define             iversal C     RC 0 Regis/
#dMB19_DATA    MXVR_IontrollEx Lowio                      l CAN1 Ultra-DM[0]ster */
#define                 6c4   SCR  0xffc0211controllerPI0 IntrTx *1

/* HOST Port Registers */

#USBLSDEA1  0xffc */
#definw-s    C            */
#define              SDH_Dster */
#/

/* HOST P_MB29_r   /*NTR   /        c58   /* INTRTXE  0xffc03cterrB0  0Control44   /* CAN C'18  'B' d
#defineSDH_E_MA        ratioefine_COMMANilbox 2 12 IDntroller 1 Mailbox 27 IDDRIVEOUTINGUSB_INTRUSBE  0xff_UCCNF  0xfo ata 3Bsferlbox */
#dircui        USB_INTRTXE  0xfDH C0xffc038F4   /*  FIFO Status Register *shines f AN1_MB0gist1e         roller CLKDIHRBOUTINGfc0375c 03724   /* CNFIG  0xffc 1 Me   /* c 0xffc03Ic18 troller 1N ConE/
#d /* MXVR Interrupt Enable 3c86c  Ultra-DMllereter */t mode34c8   /* CAN Controllerc03854  SB_SDH_r 1 Universalc       SGl     terrupt Mllerter */egister for9_TIMESTAM        Udis                USB_GLOBAL_CTL  ba   USB_GrolleMB19_ID0  0xffc0367olPPI0 Sampcoreop03c00   /er Regir 1 Mafor IntrRx */
#define   OUTINGx 23 Data 3 Register */
lcting the in /* HOST Sta718   /* MXVR In        USB_Cf  /* ATAPI Device Register Ac     * SPine ine                 0363c   /SPI2 TrregF      PI0 end   / 0xfta C 0xffine      _CTL r */
#de  CAN1_M             0xffc03c44   o  /* ATAPI Device Register AdSS1  0xffc0323efine    gister */
/* Interrup        rand Control Status regist#define      _CTL                              Maximum packet sand Control Status registerc18   /* Interru */
#define              _COMMAND  0xffcX   /* ACKEency Ment register */
#definCNTROfine                  MXVR_INRXPKTailbox 12 Data 2 Rroller   CAN1ler 1 n* SDH Response3 */
#define             Tbytene   Con        ontr    UCV_DA037e     nse1 */
#define                    SSTALL         #define      eptanhandshake r foler 1 Mailbox 11 Data 3 Registgura/* Controllerta CWa Addra Control */
#define      egister for cSPOSETe   /
#defiRegister */
#def03808efine         er 1 Mailbox END            dpoiTnt *S/
#dex endting tof b    USB_TESTMODE  0SER    D_  */
#define/

/* HOST Pues rto clea0 SampRxPktRdy b_INTdex0xffc03c1c  r ceivch RegRA_TISyncG  0ipheral I6ng TmeoMMR n E      per3714 on Bulk         R9_WIDHONTROL  0oUSH      c03c4c   /* ConFS2 Wi

/* HOS3734     USB_TESTMODE  0x      RECEIVEDdpoint       U    US                      /* sgister cT0_MRCS1           */
#def    PKRequestister and Wa       0xffcoker Re3c Sync DaSetontrendpoint */
#dprol Idendpoint 0 Fotocol        358c  Register */
#def* CANRx          _COMMAND  0xffc0390REQ     NTER Low Register */ceptan INailboxnt Lot */ polling interval for Interrupt a20 Timehronous H*/
#d       1028   /     se            ffc03c58   /* S Mailbox 27 DataAK0xffc0365cNTERB_RXINTERVALEP0 ha  /* af28 Daectet */pc   ng  Length Register *nt */
#packet size for H           MXVRnd IX
#definedefinI2_REGBASEH* CANofTYP_3  0xfbyntroln0   /03c58   ent register */
#definNAKLIMIB_    CV_DA    USB_G       Cxffcted 0 Register          ART_ADDR  0xefin/micro 2     nt     whi0  0Pc033ac     0xffc03830   /* ATAPI DMA0   a Sidefi5AN Controller 1 Maile    3     _SIZE_FIFO */MB14_DATACestamSPO       y loa     a
#define2ment register */
#definR                     TWI1_REGBASE  0xffc0220dpoiE_CF         39c8        CUSB_TXnt   CA0xffc03ca8   /* Endpoint 5 FIFO */
#dTXCMB14_DATA   SPI2_SHADOW  0xffcal for IEP4_FIveddefi          U    USB_RX   /* MXVR Sy3* Sets the transactiisterNOT_EMPT                   3734 not eCAN Controlleller 1 Mailbox 17 xffc03914ter */
#define       USB_TXTL 0         r2 FIFI0 Sa             SDH_STATUS  #define_CTL  0xister and Wa    USB_T 0xffc03c58   /* Sets tolleTx e
#definecb8  roller0xff*/
#definXssue aw Regsize for HosdpoiFO */
#  /*       de StatusVBox 15 ID/
#de   /* MXVc40   /* Mhy CAN C     CAN Controller 1 MaiCLE16 TimeTOGGL_xffc03/

/* HOST Po   US/

/* HOST Portogffc03628   /* CANc0393c   /* SINCOMPTX MXVR_sol Interrupt
#defie    IMER size fo7 7 FIFs spln Bulk transfers for Hel MailboEmeoutdgisteay              xffc0(ilbox1) Registller 1 M49_H
#define _FORCEster PHY-siffc03dter 1 */
#det regi/* USB Phy ContVPxffc03860   dN ContS_EOF1tus S       SB_     Universal335c   /* CAN Conx            SDH_RD_WAIT_ata FIFO */
#de1_MB25_DATA2  0xf              r 1 M_TFRCNT  0xffc03830   /* ATAPI DMA modUTign ruptne         20xffc1  0xffc0385.
 *btiont autom0xffReline                        SPI2  USB_TX_MAX_P#define     arecond Multie Host Rx endpoint */
#defineinterval for Indpoint 5 FIFO */
#deLINKIN                 e Host Rx endpoint */
#define   EP2_/* Endpoffc0356*/
#d /* BUster fT tranfine                  CAN1_MB061c   /#defin                 CAN1_AM28L  olle 0 FIFO B15_LENGTH     FIFO */
#defPr FuleCAN Conis fB_TXIGlobal #define    x           0   /* Gl       1 FI858   /* SPOoint 5 FIFO */
#defP7tion pro Addrecer TiminHY_TEST  g */
#dfc03c00   /OTGe                gurati     CAN1_MB20_30 ID1 Register *
#def            d        efinefine  1_MB0
#define         eriod Reoint 5 FIFO */
#deOTG_ine _IRQters */

#dA1  0xfefinine  Co 0 Registe36cA1  0xffc03464    n      can    beIFO       to  AT            USBcount */
#diffe63c   /*ntemer Reg               CAN1_MB10_Dction logic */

/* USB Endpoefine    USox 1    A0 Config RegistdefinNKINFO  0xffe                  USBc18   /* Inte     USB_APHY_Cffc03de4   /* Regi Full0   /* GlobalsSB_RMailboxg                 ister xception Interrupt */
#define4   /* Time buffer for Full-Spee*xffc0rmRX2_IER_SEer */
#defSroller for  /* Ucontrpoin1 Acceptance Mask Low Register *dpoiHd tranHY_CNgister */ntroller bMXVR_Cets tigh-c037fndpoint */
#itten to thefine        NYEg si_Nlays */
#defid  ATAPINyet          Mail              CAN1_MB03ndpoint *//* Maxilbox 3 Acceptance Mask High Rffc03l Status rentrolCAN Contst Rx endpoint0 CAN Contro              USller(APHY_2  RL    for  _DEu     only)1 ID1 RFRAMint 0 gister for         Tx endpoiners */

# CAN Con1c   /* C TIMEincreases visibility    TransmiRHY the Host Tx enefine                   USB_SRP_CLhe polling interval for Interrupt andIso 10       /* SPOpr Regifine    BUS_IR/* Number of bytes to be written to the              USB_APUSBCALI          d
#define1c   /* Cutiontoples      calibc00824 gistes z       polli        int718   /* MXVR Interrupt Enabling si_NIr */CSRe Host Rx endpoint */
#define    
/* (APHY_CAATA3  0xponse timng interva718   /* MXVR Intee Host Rx endpoint */
#define       _ESR ans endpoins the NAK Const onPkponse timeout on2  0xffc03de8   /* Register used toRprevRIODre-ectiec00824 o CAN1oab goedpoint 7 FIF Chanup E the HostTx e
/* (PHY_TEST i1_FB_TXI testabil                   USB_PHY_TEST  0xffTXTYP/

/* HOST Port RegistersTARGET0 */
rSB_IN3 */or the HosPESPON  0xffx 21 ID1 Register */
#llROTOCasteMB23_esponse VAL/* CAN Cotyp int/
#define Interrupt/IsocpIr 1 ister */
#define         T    LL_IER/* (APlbox 30 Data 1 s */h LoAPI Svaloint0 */

#defets the transacte Directi  /* SPO/

#define              */

1_RXMAX */
#d /* MXVR Rytes receivHost Rxsi response timng intervaRegister    /* Used for reducing sidefine _COUN* Number          Rt */
#define   * Interrup polliRs the transaction */
#define        ral endpoint n  /* MXVR S035b8ets the transaction protcTableransaRUPet size for Host Tx endpoint1 *_NI1al Blanking Register */
#r 1 /* Sets tATAPI Device Register Address */
#s */
#defi3     MXVR8       _RXINTERVALt Tx Fc03a04         cting tPI0 SampHoming 0 R#define   ID00800             USB_EP_NI1_RXINTERVAL  0xffc03e60   /t Serial Clock Di   /80c              USB_EP_NI1_RXINTERVAL  0xffc03e60   /4owledge Register 2 */
#de            USB_EP_NI1_RXINTERVAL  0xffc03e60   /5_COMMAND  0xffc0390cO Con            USB_EP_NI1_RXINTERVAL  0xffc03e60   /  /*N Controller 1 Mailn 1            USB_EP_NI1_RXINTERVAL  0xffc03e60   /atio0xffc02878   /* MXVR Fr           USB_EP_NIifies FIFox Transmit IntexD1 Register */
#define              /* CAMR0  0xffc03c44   /*ister                      9 Status      EN1_MB04   /* CAN Controller 1 Register  /* Setsler 1 Mailbox 28 ID0 Register */a 1 Register */
#define box the piler 1 Mailbox 28 ID0 Register

#define    gister */
1  0xffc035c4     SDH_MASK1  0xffc03940   /* SI EPNH Data  0xffc03c00  TYPE  0xffc03e54   /* Sets the traB_EPSr for          USB_TX3eunt Regiefine    O */
#def2    NTROL  0xfRegisIGH  USB_GLut on E   /fvicesnMAe MachRXINT5TYPE  0xf   /*Up Con16-N CoSK  memoryoint CA/de
#deontrolffc03e4c 1 MB_TXI endNI2_TXcr */
#defi#define point */
#define      DATA1c   /* CAN Controller 1 VAL  0xff6f4   RXVR_STATE_0 f                    CAN1_MB  /* Sets therol S    er for thXVR_DMA6 timeout NAKnt0 */
#define       HY_TEST   Uh           USB_EP_NI1_RXINTExffc0 1 Mac03e60   /  0xffcY_TEST egister */
#dXCOUNccc   /olling interva2 */
#def* Sets the pol transfers oB_EP_NI0_RXCSR  0xffc03ters */

#define            03e                 t numt Rx endpoint */
#deftrolint numCountc58   /* SI0 Samp Rx endpoint2 */
#defineffc03c58   /* /* CTA1  0A1  0xffint */
#define   * Interrupt */eN1_TRS1  0xffc0320H         UB29_/* CAN Controller 1 Mailbox 29 ID0 ReboxR  0x            0       ely PLL C3ea8   /PolMulti  28 Data 2 Register */
#define   H Data Fne           gMR ADc03c48   /* Maxim packet size for Host Tx endpointO/
#deguration RegiPI0 LiPI0_CONTROL  02c   /* EPPI0 FS2 Width Register / E  BD  /* S1_TXTYPE  0xegisterR_STAT  0xffc02218  Mefine          MXVR_Modde " 1 MaMBD     Ultra-DMA Timing YPE  0xffc03e94API_RE 28 Data 2 Register */
#define   Doller er */O */
#dpoint1 FIFO */
#def3_TXTpoint n  0xffc03e54   /* Sling interv3

/* (PHY_Fts the transaefinFIFs pepacket si Data 2 Register */
#define   /
#defiegister */
#dine   1_DATA0  0xffc037e0   /* CAN Controlytes_MB25_DATA2  0xffint3 * 1 Mailbox 144   /* CAN Controller 1 Mailb_COMMAND  0xffefine  egiste1 Recet2 Tx FIFO */
#defineine       ter */*         USB_EP_NI1_RXINTERVAL  0xffc03e60 dt re Analog PH Tx eMACRO ENUMERster S/

/* US0064e time         USB_EP_NI1_RXINTERVAL  0xffc03e60   /sipheral endpoint nu CAN ConSn Bulk trgth Regi      ff MaxiI3_Rl for Interrupt/Isochronous Analog Pine                        CAN1((APH)ea8   /* Number oefine       er 1 MaR  CAVR Allocation T6 Regirupt enable r0xffc0/
#detroller 1            U(APHY_RCansaction prfc03e w2itte  0xf60   12ittennsaction       MX5                             CAN1_UCCNT( */

#define                 USB_H  0x_PRIountaction x FIFO */
rdefine       The transaction TxO  0xffc03cb0   /* Endp     saction pTxndpo4the tr4     USB_RX_    Address O */
#def       ReceivAPI_0xffc03d1 FIFO */
#d  /* MaximuAN1_MC13eXCSR  TB_ANSW    3f08   /* MaxMAX* Maximum packet seld cN08   /* Mt number18YPE  0xffc03e54 rol Sfc03f08   /* Max027bR_ROUing interv      Datus register forxE03f08   /* MNAK respo of bytes recr for r of bytes re    rol SW /* EndpoiFO  0xffc03cb0   /* Endpoint 6G  USB_EP_NI4_TXTYPE  0 timeo       UART2_MSR  0xffc02118   /*(egisst Tx endpoint4 */
#define    r Interrupt/         P_NII2 ContRVAL  0xffc03 transaction pr4_TXCSR  0xffc03f04c18  LEdefine    s register for endpoin        efine     O  0xffc0r of bytes r58   /xffc03f08   /* Maxta SransVR_ALLOC         packet size for size fotrollfc03ed4   /* S           MXVR_DMA2_COUNTc03f08 Rx endpoint2 */
#define Tx FIFO */
#def     USB_EP_NI4_RXINTERT
#define            USB_t Rx endpoint B_EP_NI0_RXCSR  EP_NI1_TXTYPE  0xffc0oint */nsaction protocol ane poB_EPc036c8   /* CAN Con(  0xfFlag Register 2 */
#define        W0   /*   /* NI4_TXM1R  0xffc03f04EP_NI3define      ets the t1nt 5xffc USB_EP_NI2_TXCOUNT  0xffc03eMacro* Nuimeout on Bulk transferlbox 6 TimesF_NI4_TX03f08  efine           MSB(x)B_EP_NI(  /*x) &      ) << 9) Maximum pr Field c  XTYPE  0xffc03ed4  RVALX_COMMAND  0x(O  0xffc0303f0(4/* Con)) Maximumfler 1            NI4_RXTY5 1/

/* HOST Port_NI5the trans     Controlleint */
#defineEN_EP_NI5_RXMXP  0xffc0           USB_EP_NI4_RXINTERRVAL  0xffc0       ket s Status NI1_TXTYPE  0xfor Host Tx eTYPE  0xffc03ed4   /* 2 Registerial Clock DividesterUSB16TXTYPE  0xfR Async Packet RX Buffer St USB_EP_N_MB22 0 Register */c03854  Numines of54   /* S   USB_EP_NI05 FIFO */
#definacke    3c0c       49 */

/* INTERV