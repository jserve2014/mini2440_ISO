/*
 * Copyright 2007-2008 Analog Devices Inc.
 *
 * Licensed under the ADI BSD license or the GPL-2 (or later)
 */

#ifndef _DEF_BF54X_H
#define _DEF_BF54X_H


/* ************************************************************** */
/*   SYSTEM & MMR ADDRESS DEFINITIONS COMMON TO ALL ADSP-BF54x    */
/* ************************************************************** */

/* PLL Registers */

#define                          PLL_CTL  0xffc00000   /* PLL Control Register */
#define                          PLL_DIV  0xffc00004   /* PLL Divisor Register */
#define                           VR_CTL  0xffc00008   /* Voltage Regulator Control Register */
#define                         PLL_STAT  0xffc0000c   /* PLL Status Register */
#define                      PLL_LOCKCNT  0xffc00010   /* PLL Lock Count Register */

/* Debug/MP/Emulation Registers (0xFFC00014 - 0xFFC00014) */

#define                           CHIPID  0xffc00014
/* CHIPID Masks */
#define                   CHIPID_VERSION  0xF0000000
#define                    CHIPID_FAMILY  0x0FFFF000
#define               CHIPID_MANUFACTURE  0x00000FFE

/* System Reset and Interrupt Controller (0xFFC00100 - 0xFFC00104) */

#define                            SWRST  0xffc00100   /* Software Reset Register */
#define                            SYSCR  0xffc00104   /* System Configuration register */

/* SIC Registers */

#define                       SIC_IMASK0  0xffc0010c   /* System Interrupt Mask Register 0 */
#define                       SIC_IMASK1  0xffc00110   /* System Interrupt Mask Register 1 */
#define                       SIC_IMASK2  0xffc00114   /* System Interrupt Mask Register 2 */
#define                         SIC_ISR0  0xffc00118   /* System Interrupt Status Register 0 */
#define                         SIC_ISR1  0xffc0011c   /* System Interrupt Status Register 1 */
#define                         SIC_ISR2  0xffc00120   /* System Interrupt Status Register 2 */
#define                         SIC_IWR0  0xffc00124   /* System Interrupt Wakeup Register 0 */
#define                         SIC_IWR1  0xffc00128   /* System Interrupt Wakeup Register 1 */
#define                         SIC_IWR2  0xffc0012c   /* System Interrupt Wakeup Register 2 */
#define                         SIC_IAR0  0xffc00130   /* System Interrupt Assignment Register 0 */
#define                         SIC_IAR1  0xffc00134   /* System Interrupt Assignment Register 1 */
#define                         SIC_IAR2  0xffc00138   /* System Interrupt Assignment Register 2 */
#define                         SIC_IAR3  0xffc0013c   /* System Interrupt Assignment Register 3 */
#define                         SIC_IAR4  0xffc00140   /* System Interrupt Assignment Register 4 */
#define                         SIC_IAR5  0xffc00144   /* System Interrupt Assignment Register 5 */
#define                         SIC_IAR6  0xffc00148   /* System Interrupt Assignment Register 6 */
#define                         SIC_IAR7  0xffc0014c   /* System Interrupt Assignment Register 7 */
#define                         SIC_IAR8  0xffc00150   /* System Interrupt Assignment Register 8 */
#define                         SIC_IAR9  0xffc00154   /* System Interrupt Assignment Register 9 */
#define                        SIC_IAR10  0xffc00158   /* System Interrupt Assignment Register 10 */
#define                        SIC_IAR11  0xffc0015c   /* System Interrupt Assignment Register 11 */

/* Watchdog Timer Registers */

#define                         WDOG_CTL  0xffc00200   /* Watchdog Control Register */
#define                         WDOG_CNT  0xffc00204   /* Watchdog Count Register */
#define                        WDOG_STAT  0xffc00208   /* Watchdog Status Register */

/* RTC Registers */

#define                         RTC_STAT  0xffc00300   /* RTC Status Register */
#define                         RTC_ICTL  0xffc00304   /* RTC Interrupt Control Register */
#define                        RTC_ISTAT  0xffc00308   /* RTC Interrupt Status Register */
#define                        RTC_SWCNT  0xffc0030c   /* RTC Stopwatch Count Register */
#define                        RTC_ALARM  0xffc00310   /* RTC Alarm Register */
#define                         RTC_PREN  0xffc00314   /* RTC Prescaler Enable Register */

/* UART0 Registers */

#define                        UART0_DLL  0xffc00400   /* Divisor Latch Low Byte */
#define                        UART0_DLH  0xffc00404   /* Divisor Latch High Byte */
#define                       UART0_GCTL  0xffc00408   /* Global Control Register */
#define                        UART0_LCR  0xffc0040c   /* Line Control Register */
#define                        UART0_MCR  0xffc00410   /* Modem Control Register */
#define                        UART0_LSR  0xffc00414   /* Line Status Register */
#define                        UART0_MSR  0xffc00418   /* Modem Status Register */
#define                        UART0_SCR  0xffc0041c   /* Scratch Register */
#define                    UART0_IER_SET  0xffc00420   /* Interrupt Enable Register Set */
#define                  UART0_IER_CLEAR  0xffc00424   /* Interrupt Enable Register Clear */
#define                        UART0_THR  0xffc00428   /* Transmit Hold Register */
#define                        UART0_RBR  0xffc0042c   /* Receive Buffer Register */

/* SPI0 Registers */

#define                     SPI0_REGBASE  0xffc00500
#define                         SPI0_CTL  0xffc00500   /* SPI0 Control Register */
#define                         SPI0_FLG  0xffc00504   /* SPI0 Flag Register */
#define                        SPI0_STAT  0xffc00508   /* SPI0 Status Register */
#define                        SPI0_TDBR  0xffc0050c   /* SPI0 Transmit Data Buffer Register */
#define                        SPI0_RDBR  0xffc00510   /* SPI0 Receive Data Buffer Register */
#define                        SPI0_BAUD  0xffc00514   /* SPI0 Baud Rate Register */
#define                      SPI0_SHADOW  0xffc00518   /* SPI0 Receive Data Buffer Shadow Register */

/* Timer Group of 3 registers are not defined in the shared file because they are not available on the ADSP-BF542 processor */

/* Two Wire Interface Registers (TWI0) */

#define                     TWI0_REGBASE  0xffc00700
#define                      TWI0_CLKDIV  0xffc00700   /* Clock Divider Register */
#define                     TWI0_CONTROL  0xffc00704   /* TWI Control Register */
#define                  TWI0_SLAVE_CTRL  0xffc00708   /* TWI Slave Mode Control Register */
#define                  TWI0_SLAVE_STAT  0xffc0070c   /* TWI Slave Mode Status Register */
#define                  TWI0_SLAVE_ADDR  0xffc00710   /* TWI Slave Mode Address Register */
#define                 TWI0_MASTER_CTRL  0xffc00714   /* TWI Master Mode Control Register */
#define                 TWI0_MASTER_STAT  0xffc00718   /* TWI Master Mode Status Register */
#define                 TWI0_MASTER_ADDR  0xffc0071c   /* TWI Master Mode Address Register */
#define                    TWI0_INT_STAT  0xffc00720   /* TWI Interrupt Status Register */
#define                    TWI0_INT_MASK  0xffc00724   /* TWI Interrupt Mask Register */
#define                   TWI0_FIFO_CTRL  0xffc00728   /* TWI FIFO Control Register */
#define                   TWI0_FIFO_STAT  0xffc0072c   /* TWI FIFO Status Register */
#define                   TWI0_XMT_DATA8  0xffc00780   /* TWI FIFO Transmit Data Single Byte Register */
#define                  TWI0_XMT_DATA16  0xffc00784   /* TWI FIFO Transmit Data Double Byte Register */
#define                   TWI0_RCV_DATA8  0xffc00788   /* TWI FIFO Receive Data Single Byte Register */
#define                  TWI0_RCV_DATA16  0xffc0078c   /* TWI FIFO Receive Data Double Byte Register */

/* SPORT0 is not defined in the shared file because it is not available on the ADSP-BF542 and ADSP-BF544 processors */

/* SPORT1 Registers */

#define                      SPORT1_TCR1  0xffc00900   /* SPORT1 Transmit Configuration 1 Register */
#define                      SPORT1_TCR2  0xffc00904   /* SPORT1 Transmit Configuration 2 Register */
#define                   SPORT1_TCLKDIV  0xffc00908   /* SPORT1 Transmit Serial Clock Divider Register */
#define                    SPORT1_TFSDIV  0xffc0090c   /* SPORT1 Transmit Frame Sync Divider Register */
#define                        SPORT1_TX  0xffc00910   /* SPORT1 Transmit Data Register */
#define                        SPORT1_RX  0xffc00918   /* SPORT1 Receive Data Register */
#define                      SPORT1_RCR1  0xffc00920   /* SPORT1 Receive Configuration 1 Register */
#define                      SPORT1_RCR2  0xffc00924   /* SPORT1 Receive Configuration 2 Register */
#define                   SPORT1_RCLKDIV  0xffc00928   /* SPORT1 Receive Serial Clock Divider Register */
#define                    SPORT1_RFSDIV  0xffc0092c   /* SPORT1 Receive Frame Sync Divider Register */
#define                      SPORT1_STAT  0xffc00930   /* SPORT1 Status Register */
#define                      SPORT1_CHNL  0xffc00934   /* SPORT1 Current Channel Register */
#define                     SPORT1_MCMC1  0xffc00938   /* SPORT1 Multi channel Configuration Register 1 */
#define                     SPORT1_MCMC2  0xffc0093c   /* SPORT1 Multi channel Configuration Register 2 */
#define                     SPORT1_MTCS0  0xffc00940   /* SPORT1 Multi channel Transmit Select Register 0 */
#define                     SPORT1_MTCS1  0xffc00944   /* SPORT1 Multi channel Transmit Select Register 1 */
#define                     SPORT1_MTCS2  0xffc00948   /* SPORT1 Multi channel Transmit Select Register 2 */
#define                     SPORT1_MTCS3  0xffc0094c   /* SPORT1 Multi channel Transmit Select Register 3 */
#define                     SPORT1_MRCS0  0xffc00950   /* SPORT1 Multi channel Receive Select Register 0 */
#define                     SPORT1_MRCS1  0xffc00954   /* SPORT1 Multi channel Receive Select Register 1 */
#define                     SPORT1_MRCS2  0xffc00958   /* SPORT1 Multi channel Receive Select Register 2 */
#define                     SPORT1_MRCS3  0xffc0095c   /* SPORT1 Multi channel Receive Select Register 3 */

/* Asynchronous Memory Control Registers */

#define                      EBIU_AMGCTL  0xffc00a00   /* Asynchronous Memory Global Control Register */
#define                    EBIU_AMBCTL0   0xffc00a04   /* Asynchronous Memory Bank Control Register */
#define                    EBIU_AMBCTL1   0xffc00a08   /* Asynchronous Memory Bank Control Register */
#define                      EBIU_MBSCTL  0xffc00a0c   /* Asynchronous Memory Bank Select Control Register */
#define                     EBIU_ARBSTAT  0xffc00a10   /* Asynchronous Memory Arbiter Status Register */
#define                        EBIU_MODE  0xffc00a14   /* Asynchronous Mode Control Register */
#define                        EBIU_FCTL  0xffc00a18   /* Asynchronous Memory Flash Control Register */

/* DDR Memory Control Registers */

#define                     EBIU_DDRCTL0  0xffc00a20   /* DDR Memory Control 0 Register */
#define                     EBIU_DDRCTL1  0xffc00a24   /* DDR Memory Control 1 Register */
#define                     EBIU_DDRCTL2  0xffc00a28   /* DDR Memory Control 2 Register */
#define                     EBIU_DDRCTL3  0xffc00a2c   /* DDR Memory Control 3 Register */
#define                      EBIU_DDRQUE  0xffc00a30   /* DDR Queue Configuration Register */
#define                      EBIU_ERRADD  0xffc00a34   /* DDR Error Address Register */
#define                      EBIU_ERRMST  0xffc00a38   /* DDR Error Master Register */
#define                      EBIU_RSTCTL  0xffc00a3c   /* DDR Reset Control Register */

/* DDR BankRead and Write Count Registers */

#define                     EBIU_DDRBRC0  0xffc00a60   /* DDR Bank0 Read Count Register */
#define                     EBIU_DDRBRC1  0xffc00a64   /* DDR Bank1 Read Count Register */
#define                     EBIU_DDRBRC2  0xffc00a68   /* DDR Bank2 Read Count Register */
#define                     EBIU_DDRBRC3  0xffc00a6c   /* DDR Bank3 Read Count Register */
#define                     EBIU_DDRBRC4  0xffc00a70   /* DDR Bank4 Read Count Register */
#define                     EBIU_DDRBRC5  0xffc00a74   /* DDR Bank5 Read Count Register */
#define                     EBIU_DDRBRC6  0xffc00a78   /* DDR Bank6 Read Count Register */
#define                     EBIU_DDRBRC7  0xffc00a7c   /* DDR Bank7 Read Count Register */
#define                     EBIU_DDRBWC0  0xffc00a80   /* DDR Bank0 Write Count Register */
#define                     EBIU_DDRBWC1  0xffc00a84   /* DDR Bank1 Write Count Register */
#define                     EBIU_DDRBWC2  0xffc00a88   /* DDR Bank2 Write Count Register */
#define                     EBIU_DDRBWC3  0xffc00a8c   /* DDR Bank3 Write Count Register */
#define                     EBIU_DDRBWC4  0xffc00a90   /* DDR Bank4 Write Count Register */
#define                     EBIU_DDRBWC5  0xffc00a94   /* DDR Bank5 Write Count Register */
#define                     EBIU_DDRBWC6  0xffc00a98   /* DDR Bank6 Write Count Register */
#define                     EBIU_DDRBWC7  0xffc00a9c   /* DDR Bank7 Write Count Register */
#define                     EBIU_DDRACCT  0xffc00aa0   /* DDR Activation Count Register */
#define                     EBIU_DDRTACT  0xffc00aa8   /* DDR Turn Around Count Register */
#define                     EBIU_DDRARCT  0xffc00aac   /* DDR Auto-refresh Count Register */
#define                      EBIU_DDRGC0  0xffc00ab0   /* DDR Grant Count 0 Register */
#define                      EBIU_DDRGC1  0xffc00ab4   /* DDR Grant Count 1 Register */
#define                      EBIU_DDRGC2  0xffc00ab8   /* DDR Grant Count 2 Register */
#define                      EBIU_DDRGC3  0xffc00abc   /* DDR Grant Count 3 Register */
#define                     EBIU_DDRMCEN  0xffc00ac0   /* DDR Metrics Counter Enable Register */
#define                     EBIU_DDRMCCL  0xffc00ac4   /* DDR Metrics Counter Clear Register */

/* DMAC0 Registers */

#define                      DMAC0_TCPER  0xffc00b0c   /* DMA Controller 0 Traffic Control Periods Register */
#define                      DMAC0_TCCNT  0xffc00b10   /* DMA Controller 0 Current Counts Register */

/* DMA Channel 0 Registers */

#define               DMA0_NEXT_DESC_PTR  0xffc00c00   /* DMA Channel 0 Next Descriptor Pointer Register */
#define                  DMA0_START_ADDR  0xffc00c04   /* DMA Channel 0 Start Address Register */
#define                      DMA0_CONFIG  0xffc00c08   /* DMA Channel 0 Configuration Register */
#define                     DMA0_X_COUNT  0xffc00c10   /* DMA Channel 0 X Count Register */
#define                    DMA0_X_MODIFY  0xffc00c14   /* DMA Channel 0 X Modify Register */
#define                     DMA0_Y_COUNT  0xffc00c18   /* DMA Channel 0 Y Count Register */
#define                    DMA0_Y_MODIFY  0xffc00c1c   /* DMA Channel 0 Y Modify Register */
#define               DMA0_CURR_DESC_PTR  0xffc00c20   /* DMA Channel 0 Current Descriptor Pointer Register */
#define                   DMA0_CURR_ADDR  0xffc00c24   /* DMA Channel 0 Current Address Register */
#define                  DMA0_IRQ_STATUS  0xffc00c28   /* DMA Channel 0 Interrupt/Status Register */
#define              DMA0_PERIPHERAL_MAP  0xffc00c2c   /* DMA Channel 0 Peripheral Map Register */
#define                DMA0_CURR_X_COUNT  0xffc00c30   /* DMA Channel 0 Current X Count Register */
#define                DMA0_CURR_Y_COUNT  0xffc00c38   /* DMA Channel 0 Current Y Count Register */

/* DMA Channel 1 Registers */

#define               DMA1_NEXT_DESC_PTR  0xffc00c40   /* DMA Channel 1 Next Descriptor Pointer Register */
#define                  DMA1_START_ADDR  0xffc00c44   /* DMA Channel 1 Start Address Register */
#define                      DMA1_CONFIG  0xffc00c48   /* DMA Channel 1 Configuration Register */
#define                     DMA1_X_COUNT  0xffc00c50   /* DMA Channel 1 X Count Register */
#define                    DMA1_X_MODIFY  0xffc00c54   /* DMA Channel 1 X Modify Register */
#define                     DMA1_Y_COUNT  0xffc00c58   /* DMA Channel 1 Y Count Register */
#define                    DMA1_Y_MODIFY  0xffc00c5c   /* DMA Channel 1 Y Modify Register */
#define               DMA1_CURR_DESC_PTR  0xffc00c60   /* DMA Channel 1 Current Descriptor Pointer Register */
#define                   DMA1_CURR_ADDR  0xffc00c64   /* DMA Channel 1 Current Address Register */
#define                  DMA1_IRQ_STATUS  0xffc00c68   /* DMA Channel 1 Interrupt/Status Register */
#define              DMA1_PERIPHERAL_MAP  0xffc00c6c   /* DMA Channel 1 Peripheral Map Register */
#define                DMA1_CURR_X_COUNT  0xffc00c70   /* DMA Channel 1 Current X Count Register */
#define                DMA1_CURR_Y_COUNT  0xffc00c78   /* DMA Channel 1 Current Y Count Register */

/* DMA Channel 2 Registers */

#define               DMA2_NEXT_DESC_PTR  0xffc00c80   /* DMA Channel 2 Next Descriptor Pointer Register */
#define                  DMA2_START_ADDR  0xffc00c84   /* DMA Channel 2 Start Address Register */
#define                      DMA2_CONFIG  0xffc00c88   /* DMA Channel 2 Configuration Register */
#define                     DMA2_X_COUNT  0xffc00c90   /* DMA Channel 2 X Count Register */
#define                    DMA2_X_MODIFY  0xffc00c94   /* DMA Channel 2 X Modify Register */
#define                     DMA2_Y_COUNT  0xffc00c98   /* DMA Channel 2 Y Count Register */
#define                    DMA2_Y_MODIFY  0xffc00c9c   /* DMA Channel 2 Y Modify Register */
#define               DMA2_CURR_DESC_PTR  0xffc00ca0   /* DMA Channel 2 Current Descriptor Pointer Register */
#define                   DMA2_CURR_ADDR  0xffc00ca4   /* DMA Channel 2 Current Address Register */
#define                  DMA2_IRQ_STATUS  0xffc00ca8   /* DMA Channel 2 Interrupt/Status Register */
#define              DMA2_PERIPHERAL_MAP  0xffc00cac   /* DMA Channel 2 Peripheral Map Register */
#define                DMA2_CURR_X_COUNT  0xffc00cb0   /* DMA Channel 2 Current X Count Register */
#define                DMA2_CURR_Y_COUNT  0xffc00cb8   /* DMA Channel 2 Current Y Count Register */

/* DMA Channel 3 Registers */

#define               DMA3_NEXT_DESC_PTR  0xffc00cc0   /* DMA Channel 3 Next Descriptor Pointer Register */
#define                  DMA3_START_ADDR  0xffc00cc4   /* DMA Channel 3 Start Address Register */
#define                      DMA3_CONFIG  0xffc00cc8   /* DMA Channel 3 Configuration Register */
#define                     DMA3_X_COUNT  0xffc00cd0   /* DMA Channel 3 X Count Register */
#define                    DMA3_X_MODIFY  0xffc00cd4   /* DMA Channel 3 X Modify Register */
#define                     DMA3_Y_COUNT  0xffc00cd8   /* DMA Channel 3 Y Count Register */
#define                    DMA3_Y_MODIFY  0xffc00cdc   /* DMA Channel 3 Y Modify Register */
#define               DMA3_CURR_DESC_PTR  0xffc00ce0   /* DMA Channel 3 Current Descriptor Pointer Register */
#define                   DMA3_CURR_ADDR  0xffc00ce4   /* DMA Channel 3 Current Address Register */
#define                  DMA3_IRQ_STATUS  0xffc00ce8   /* DMA Channel 3 Interrupt/Status Register */
#define              DMA3_PERIPHERAL_MAP  0xffc00cec   /* DMA Channel 3 Peripheral Map Register */
#define                DMA3_CURR_X_COUNT  0xffc00cf0   /* DMA Channel 3 Current X Count Register */
#define                DMA3_CURR_Y_COUNT  0xffc00cf8   /* DMA Channel 3 Current Y Count Register */

/* DMA Channel 4 Registers */

#define               DMA4_NEXT_DESC_PTR  0xffc00d00   /* DMA Channel 4 Next Descriptor Pointer Register */
#define                  DMA4_START_ADDR  0xffc00d04   /* DMA Channel 4 Start Address Register */
#define                      DMA4_CONFIG  0xffc00d08   /* DMA Channel 4 Configuration Register */
#define                     DMA4_X_COUNT  0xffc00d10   /* DMA Channel 4 X Count Register */
#define                    DMA4_X_MODIFY  0xffc00d14   /* DMA Channel 4 X Modify Register */
#define                     DMA4_Y_COUNT  0xffc00d18   /* DMA Channel 4 Y Count Register */
#define                    DMA4_Y_MODIFY  0xffc00d1c   /* DMA Channel 4 Y Modify Register */
#define               DMA4_CURR_DESC_PTR  0xffc00d20   /* DMA Channel 4 Current Descriptor Pointer Register */
#define                   DMA4_CURR_ADDR  0xffc00d24   /* DMA Channel 4 Current Address Register */
#define                  DMA4_IRQ_STATUS  0xffc00d28   /* DMA Channel 4 Interrupt/Status Register */
#define              DMA4_PERIPHERAL_MAP  0xffc00d2c   /* DMA Channel 4 Peripheral Map Register */
#define                DMA4_CURR_X_COUNT  0xffc00d30   /* DMA Channel 4 Current X Count Register */
#define                DMA4_CURR_Y_COUNT  0xffc00d38   /* DMA Channel 4 Current Y Count Register */

/* DMA Channel 5 Registers */

#define               DMA5_NEXT_DESC_PTR  0xffc00d40   /* DMA Channel 5 Next Descriptor Pointer Register */
#define                  DMA5_START_ADDR  0xffc00d44   /* DMA Channel 5 Start Address Register */
#define                      DMA5_CONFIG  0xffc00d48   /* DMA Channel 5 Configuration Register */
#define                     DMA5_X_COUNT  0xffc00d50   /* DMA Channel 5 X Count Register */
#define                    DMA5_X_MODIFY  0xffc00d54   /* DMA Channel 5 X Modify Register */
#define                     DMA5_Y_COUNT  0xffc00d58   /* DMA Channel 5 Y Count Register */
#define                    DMA5_Y_MODIFY  0xffc00d5c   /* DMA Channel 5 Y Modify Register */
#define               DMA5_CURR_DESC_PTR  0xffc00d60   /* DMA Channel 5 Current Descriptor Pointer Register */
#define                   DMA5_CURR_ADDR  0xffc00d64   /* DMA Channel 5 Current Address Register */
#define                  DMA5_IRQ_STATUS  0xffc00d68   /* DMA Channel 5 Interrupt/Status Register */
#define              DMA5_PERIPHERAL_MAP  0xffc00d6c   /* DMA Channel 5 Peripheral Map Register */
#define                DMA5_CURR_X_COUNT  0xffc00d70   /* DMA Channel 5 Current X Count Register */
#define                DMA5_CURR_Y_COUNT  0xffc00d78   /* DMA Channel 5 Current Y Count Register */

/* DMA Channel 6 Registers */

#define               DMA6_NEXT_DESC_PTR  0xffc00d80   /* DMA Channel 6 Next Descriptor Pointer Register */
#define                  DMA6_START_ADDR  0xffc00d84   /* DMA Channel 6 Start Address Register */
#define                      DMA6_CONFIG  0xffc00d88   /* DMA Channel 6 Configuration Register */
#define                     DMA6_X_COUNT  0xffc00d90   /* DMA Channel 6 X Count Register */
#define                    DMA6_X_MODIFY  0xffc00d94   /* DMA Channel 6 X Modify Register */
#define                     DMA6_Y_COUNT  0xffc00d98   /* DMA Channel 6 Y Count Register */
#define                    DMA6_Y_MODIFY  0xffc00d9c   /* DMA Channel 6 Y Modify Register */
#define               DMA6_CURR_DESC_PTR  0xffc00da0   /* DMA Channel 6 Current Descriptor Pointer Register */
#define                   DMA6_CURR_ADDR  0xffc00da4   /* DMA Channel 6 Current Address Register */
#define                  DMA6_IRQ_STATUS  0xffc00da8   /* DMA Channel 6 Interrupt/Status Register */
#define              DMA6_PERIPHERAL_MAP  0xffc00dac   /* DMA Channel 6 Peripheral Map Register */
#define                DMA6_CURR_X_COUNT  0xffc00db0   /* DMA Channel 6 Current X Count Register */
#define                DMA6_CURR_Y_COUNT  0xffc00db8   /* DMA Channel 6 Current Y Count Register */

/* DMA Channel 7 Registers */

#define               DMA7_NEXT_DESC_PTR  0xffc00dc0   /* DMA Channel 7 Next Descriptor Pointer Register */
#define                  DMA7_START_ADDR  0xffc00dc4   /* DMA Channel 7 Start Address Register */
#define                      DMA7_CONFIG  0xffc00dc8   /* DMA Channel 7 Configuration Register */
#define                     DMA7_X_COUNT  0xffc00dd0   /* DMA Channel 7 X Count Register */
#define                    DMA7_X_MODIFY  0xffc00dd4   /* DMA Channel 7 X Modify Register */
#define                     DMA7_Y_COUNT  0xffc00dd8   /* DMA Channel 7 Y Count Register */
#define                    DMA7_Y_MODIFY  0xffc00ddc   /* DMA Channel 7 Y Modify Register */
#define               DMA7_CURR_DESC_PTR  0xffc00de0   /* DMA Channel 7 Current Descriptor Pointer Register */
#define                   DMA7_CURR_ADDR  0xffc00de4   /* DMA Channel 7 Current Address Register */
#define                  DMA7_IRQ_STATUS  0xffc00de8   /* DMA Channel 7 Interrupt/Status Register */
#define              DMA7_PERIPHERAL_MAP  0xffc00dec   /* DMA Channel 7 Peripheral Map Register */
#define                DMA7_CURR_X_COUNT  0xffc00df0   /* DMA Channel 7 Current X Count Register */
#define                DMA7_CURR_Y_COUNT  0xffc00df8   /* DMA Channel 7 Current Y Count Register */

/* DMA Channel 8 Registers */

#define               DMA8_NEXT_DESC_PTR  0xffc00e00   /* DMA Channel 8 Next Descriptor Pointer Register */
#define                  DMA8_START_ADDR  0xffc00e04   /* DMA Channel 8 Start Address Register */
#define                      DMA8_CONFIG  0xffc00e08   /* DMA Channel 8 Configuration Register */
#define                     DMA8_X_COUNT  0xffc00e10   /* DMA Channel 8 X Count Register */
#define                    DMA8_X_MODIFY  0xffc00e14   /* DMA Channel 8 X Modify Register */
#define                     DMA8_Y_COUNT  0xffc00e18   /* DMA Channel 8 Y Count Register */
#define                    DMA8_Y_MODIFY  0xffc00e1c   /* DMA Channel 8 Y Modify Register */
#define               DMA8_CURR_DESC_PTR  0xffc00e20   /* DMA Channel 8 Current Descriptor Pointer Register */
#define                   DMA8_CURR_ADDR  0xffc00e24   /* DMA Channel 8 Current Address Register */
#define                  DMA8_IRQ_STATUS  0xffc00e28   /* DMA Channel 8 Interrupt/Status Register */
#define              DMA8_PERIPHERAL_MAP  0xffc00e2c   /* DMA Channel 8 Peripheral Map Register */
#define                DMA8_CURR_X_COUNT  0xffc00e30   /* DMA Channel 8 Current X Count Register */
#define                DMA8_CURR_Y_COUNT  0xffc00e38   /* DMA Channel 8 Current Y Count Register */

/* DMA Channel 9 Registers */

#define               DMA9_NEXT_DESC_PTR  0xffc00e40   /* DMA Channel 9 Next Descriptor Pointer Register */
#define                  DMA9_START_ADDR  0xffc00e44   /* DMA Channel 9 Start Address Register */
#define                      DMA9_CONFIG  0xffc00e48   /* DMA Channel 9 Configuration Register */
#define                     DMA9_X_COUNT  0xffc00e50   /* DMA Channel 9 X Count Register */
#define                    DMA9_X_MODIFY  0xffc00e54   /* DMA Channel 9 X Modify Register */
#define                     DMA9_Y_COUNT  0xffc00e58   /* DMA Channel 9 Y Count Register */
#define                    DMA9_Y_MODIFY  0xffc00e5c   /* DMA Channel 9 Y Modify Register */
#define               DMA9_CURR_DESC_PTR  0xffc00e60   /* DMA Channel 9 Current Descriptor Pointer Register */
#define                   DMA9_CURR_ADDR  0xffc00e64   /* DMA Channel 9 Current Address Register */
#define                  DMA9_IRQ_STATUS  0xffc00e68   /* DMA Channel 9 Interrupt/Status Register */
#define              DMA9_PERIPHERAL_MAP  0xffc00e6c   /* DMA Channel 9 Peripheral Map Register */
#define                DMA9_CURR_X_COUNT  0xffc00e70   /* DMA Channel 9 Current X Count Register */
#define                DMA9_CURR_Y_COUNT  0xffc00e78   /* DMA Channel 9 Current Y Count Register */

/* DMA Channel 10 Registers */

#define              DMA10_NEXT_DESC_PTR  0xffc00e80   /* DMA Channel 10 Next Descriptor Pointer Register */
#define                 DMA10_START_ADDR  0xffc00e84   /* DMA Channel 10 Start Address Register */
#define                     DMA10_CONFIG  0xffc00e88   /* DMA Channel 10 Configuration Register */
#define                    DMA10_X_COUNT  0xffc00e90   /* DMA Channel 10 X Count Register */
#define                   DMA10_X_MODIFY  0xffc00e94   /* DMA Channel 10 X Modify Register */
#define                    DMA10_Y_COUNT  0xffc00e98   /* DMA Channel 10 Y Count Register */
#define                   DMA10_Y_MODIFY  0xffc00e9c   /* DMA Channel 10 Y Modify Register */
#define              DMA10_CURR_DESC_PTR  0xffc00ea0   /* DMA Channel 10 Current Descriptor Pointer Register */
#define                  DMA10_CURR_ADDR  0xffc00ea4   /* DMA Channel 10 Current Address Register */
#define                 DMA10_IRQ_STATUS  0xffc00ea8   /* DMA Channel 10 Interrupt/Status Register */
#define             DMA10_PERIPHERAL_MAP  0xffc00eac   /* DMA Channel 10 Peripheral Map Register */
#define               DMA10_CURR_X_COUNT  0xffc00eb0   /* DMA Channel 10 Current X Count Register */
#define               DMA10_CURR_Y_COUNT  0xffc00eb8   /* DMA Channel 10 Current Y Count Register */

/* DMA Channel 11 Registers */

#define              DMA11_NEXT_DESC_PTR  0xffc00ec0   /* DMA Channel 11 Next Descriptor Pointer Register */
#define                 DMA11_START_ADDR  0xffc00ec4   /* DMA Channel 11 Start Address Register */
#define                     DMA11_CONFIG  0xffc00ec8   /* DMA Channel 11 Configuration Register */
#define                    DMA11_X_COUNT  0xffc00ed0   /* DMA Channel 11 X Count Register */
#define                   DMA11_X_MODIFY  0xffc00ed4   /* DMA Channel 11 X Modify Register */
#define                    DMA11_Y_COUNT  0xffc00ed8   /* DMA Channel 11 Y Count Register */
#define                   DMA11_Y_MODIFY  0xffc00edc   /* DMA Channel 11 Y Modify Register */
#define              DMA11_CURR_DESC_PTR  0xffc00ee0   /* DMA Channel 11 Current Descriptor Pointer Register */
#define                  DMA11_CURR_ADDR  0xffc00ee4   /* DMA Channel 11 Current Address Register */
#define                 DMA11_IRQ_STATUS  0xffc00ee8   /* DMA Channel 11 Interrupt/Status Register */
#define             DMA11_PERIPHERAL_MAP  0xffc00eec   /* DMA Channel 11 Peripheral Map Register */
#define               DMA11_CURR_X_COUNT  0xffc00ef0   /* DMA Channel 11 Current X Count Register */
#define               DMA11_CURR_Y_COUNT  0xffc00ef8   /* DMA Channel 11 Current Y Count Register */

/* MDMA Stream 0 Registers */

#define            MDMA_D0_NEXT_DESC_PTR  0xffc00f00   /* Memory DMA Stream 0 Destination Next Descriptor Pointer Register */
#define               MDMA_D0_START_ADDR  0xffc00f04   /* Memory DMA Stream 0 Destination Start Address Register */
#define                   MDMA_D0_CONFIG  0xffc00f08   /* Memory DMA Stream 0 Destination Configuration Register */
#define                  MDMA_D0_X_COUNT  0xffc00f10   /* Memory DMA Stream 0 Destination X Count Register */
#define                 MDMA_D0_X_MODIFY  0xffc00f14   /* Memory DMA Stream 0 Destination X Modify Register */
#define                  MDMA_D0_Y_COUNT  0xffc00f18   /* Memory DMA Stream 0 Destination Y Count Register */
#define                 MDMA_D0_Y_MODIFY  0xffc00f1c   /* Memory DMA Stream 0 Destination Y Modify Register */
#define            MDMA_D0_CURR_DESC_PTR  0xffc00f20   /* Memory DMA Stream 0 Destination Current Descriptor Pointer Register */
#define                MDMA_D0_CURR_ADDR  0xffc00f24   /* Memory DMA Stream 0 Destination Current Address Register */
#define               MDMA_D0_IRQ_STATUS  0xffc00f28   /* Memory DMA Stream 0 Destination Interrupt/Status Register */
#define           MDMA_D0_PERIPHERAL_MAP  0xffc00f2c   /* Memory DMA Stream 0 Destination Peripheral Map Register */
#define             MDMA_D0_CURR_X_COUNT  0xffc00f30   /* Memory DMA Stream 0 Destination Current X Count Register */
#define             MDMA_D0_CURR_Y_COUNT  0xffc00f38   /* Memory DMA Stream 0 Destination Current Y Count Register */
#define            MDMA_S0_NEXT_DESC_PTR  0xffc00f40   /* Memory DMA Stream 0 Source Next Descriptor Pointer Register */
#define               MDMA_S0_START_ADDR  0xffc00f44   /* Memory DMA Stream 0 Source Start Address Register */
#define                   MDMA_S0_CONFIG  0xffc00f48   /* Memory DMA Stream 0 Source Configuration Register */
#define                  MDMA_S0_X_COUNT  0xffc00f50   /* Memory DMA Stream 0 Source X Count Register */
#define                 MDMA_S0_X_MODIFY  0xffc00f54   /* Memory DMA Stream 0 Source X Modify Register */
#define                  MDMA_S0_Y_COUNT  0xffc00f58   /* Memory DMA Stream 0 Source Y Count Register */
#define                 MDMA_S0_Y_MODIFY  0xffc00f5c   /* Memory DMA Stream 0 Source Y Modify Register */
#define            MDMA_S0_CURR_DESC_PTR  0xffc00f60   /* Memory DMA Stream 0 Source Current Descriptor Pointer Register */
#define                MDMA_S0_CURR_ADDR  0xffc00f64   /* Memory DMA Stream 0 Source Current Address Register */
#define               MDMA_S0_IRQ_STATUS  0xffc00f68   /* Memory DMA Stream 0 Source Interrupt/Status Register */
#define           MDMA_S0_PERIPHERAL_MAP  0xffc00f6c   /* Memory DMA Stream 0 Source Peripheral Map Register */
#define             MDMA_S0_CURR_X_COUNT  0xffc00f70   /* Memory DMA Stream 0 Source Current X Count Register */
#define             MDMA_S0_CURR_Y_COUNT  0xffc00f78   /* Memory DMA Stream 0 Source Current Y Count Register */

/* MDMA Stream 1 Registers */

#define            MDMA_D1_NEXT_DESC_PTR  0xffc00f80   /* Memory DMA Stream 1 Destination Next Descriptor Pointer Register */
#define               MDMA_D1_START_ADDR  0xffc00f84   /* Memory DMA Stream 1 Destination Start Address Register */
#define                   MDMA_D1_CONFIG  0xffc00f88   /* Memory DMA Stream 1 Destination Configuration Register */
#define                  MDMA_D1_X_COUNT  0xffc00f90   /* Memory DMA Stream 1 Destination X Count Register */
#define                 MDMA_D1_X_MODIFY  0xffc00f94   /* Memory DMA Stream 1 Destination X Modify Register */
#define                  MDMA_D1_Y_COUNT  0xffc00f98   /* Memory DMA Stream 1 Destination Y Count Register */
#define                 MDMA_D1_Y_MODIFY  0xffc00f9c   /* Memory DMA Stream 1 Destination Y Modify Register */
#define            MDMA_D1_CURR_DESC_PTR  0xffc00fa0   /* Memory DMA Stream 1 Destination Current Descriptor Pointer Register */
#define                MDMA_D1_CURR_ADDR  0xffc00fa4   /* Memory DMA Stream 1 Destination Current Address Register */
#define               MDMA_D1_IRQ_STATUS  0xffc00fa8   /* Memory DMA Stream 1 Destination Interrupt/Status Register */
#define           MDMA_D1_PERIPHERAL_MAP  0xffc00fac   /* Memory DMA Stream 1 Destination Peripheral Map Register */
#define             MDMA_D1_CURR_X_COUNT  0xffc00fb0   /* Memory DMA Stream 1 Destination Current X Count Register */
#define             MDMA_D1_CURR_Y_COUNT  0xffc00fb8   /* Memory DMA Stream 1 Destination Current Y Count Register */
#define            MDMA_S1_NEXT_DESC_PTR  0xffc00fc0   /* Memory DMA Stream 1 Source Next Descriptor Pointer Register */
#define               MDMA_S1_START_ADDR  0xffc00fc4   /* Memory DMA Stream 1 Source Start Address Register */
#define                   MDMA_S1_CONFIG  0xffc00fc8   /* Memory DMA Stream 1 Source Configuration Register */
#define                  MDMA_S1_X_COUNT  0xffc00fd0   /* Memory DMA Stream 1 Source X Count Register */
#define                 MDMA_S1_X_MODIFY  0xffc00fd4   /* Memory DMA Stream 1 Source X Modify Register */
#define                  MDMA_S1_Y_COUNT  0xffc00fd8   /* Memory DMA Stream 1 Source Y Count Register */
#define                 MDMA_S1_Y_MODIFY  0xffc00fdc   /* Memory DMA Stream 1 Source Y Modify Register */
#define            MDMA_S1_CURR_DESC_PTR  0xffc00fe0   /* Memory DMA Stream 1 Source Current Descriptor Pointer Register */
#define                MDMA_S1_CURR_ADDR  0xffc00fe4   /* Memory DMA Stream 1 Source Current Address Register */
#define               MDMA_S1_IRQ_STATUS  0xffc00fe8   /* Memory DMA Stream 1 Source Interrupt/Status Register */
#define           MDMA_S1_PERIPHERAL_MAP  0xffc00fec   /* Memory DMA Stream 1 Source Peripheral Map Register */
#define             MDMA_S1_CURR_X_COUNT  0xffc00ff0   /* Memory DMA Stream 1 Source Current X Count Register */
#define             MDMA_S1_CURR_Y_COUNT  0xffc00ff8   /* Memory DMA Stream 1 Source Current Y Count Register */

/* UART3 Registers */

#define                        UART3_DLL  0xffc03100   /* Divisor Latch Low Byte */
#define                        UART3_DLH  0xffc03104   /* Divisor Latch High Byte */
#define                       UART3_GCTL  0xffc03108   /* Global Control Register */
#define                        UART3_LCR  0xffc0310c   /* Line Control Register */
#define                        UART3_MCR  0xffc03110   /* Modem Control Register */
#define                        UART3_LSR  0xffc03114   /* Line Status Register */
#define                        UART3_MSR  0xffc03118   /* Modem Status Register */
#define                        UART3_SCR  0xffc0311c   /* Scratch Register */
#define                    UART3_IER_SET  0xffc03120   /* Interrupt Enable Register Set */
#define                  UART3_IER_CLEAR  0xffc03124   /* Interrupt Enable Register Clear */
#define                        UART3_THR  0xffc03128   /* Transmit Hold Register */
#define                        UART3_RBR  0xffc0312c   /* Receive Buffer Register */

/* EPPI1 Registers */

#define                     EPPI1_STATUS  0xffc01300   /* EPPI1 Status Register */
#define                     EPPI1_HCOUNT  0xffc01304   /* EPPI1 Horizontal Transfer Count Register */
#define                     EPPI1_HDELAY  0xffc01308   /* EPPI1 Horizontal Delay Count Register */
#define                     EPPI1_VCOUNT  0xffc0130c   /* EPPI1 Vertical Transfer Count Register */
#define                     EPPI1_VDELAY  0xffc01310   /* EPPI1 Vertical Delay Count Register */
#define                      EPPI1_FRAME  0xffc01314   /* EPPI1 Lines per Frame Register */
#define                       EPPI1_LINE  0xffc01318   /* EPPI1 Samples per Line Register */
#define                     EPPI1_CLKDIV  0xffc0131c   /* EPPI1 Clock Divide Register */
#define                    EPPI1_CONTROL  0xffc01320   /* EPPI1 Control Register */
#define                   EPPI1_FS1W_HBL  0xffc01324   /* EPPI1 FS1 Width Register / EPPI1 Horizontal Blanking Samples Per Line Register */
#define                  EPPI1_FS1P_AVPL  0xffc01328   /* EPPI1 FS1 Period Register / EPPI1 Active Video Samples Per Line Register */
#define                   EPPI1_FS2W_LVB  0xffc0132c   /* EPPI1 FS2 Width Register / EPPI1 Lines of Vertical Blanking Register */
#define                  EPPI1_FS2P_LAVF  0xffc01330   /* EPPI1 FS2 Period Register/ EPPI1 Lines of Active Video Per Field Register */
#define                       EPPI1_CLIP  0xffc01334   /* EPPI1 Clipping Register */

/* Port Interrupt 0 Registers (32-bit) */

#define                   PINT0_MASK_SET  0xffc01400   /* Pin Interrupt 0 Mask Set Register */
#define                 PINT0_MASK_CLEAR  0xffc01404   /* Pin Interrupt 0 Mask Clear Register */
#define                    PINT0_REQUEST  0xffc01408   /* Pin Interrupt 0 Interrupt Request Register */
#define                     PINT0_ASSIGN  0xffc0140c   /* Pin Interrupt 0 Port Assign Register */
#define                   PINT0_EDGE_SET  0xffc01410   /* Pin Interrupt 0 Edge-sensitivity Set Register */
#define                 PINT0_EDGE_CLEAR  0xffc01414   /* Pin Interrupt 0 Edge-sensitivity Clear Register */
#define                 PINT0_INVERT_SET  0xffc01418   /* Pin Interrupt 0 Inversion Set Register */
#define               PINT0_INVERT_CLEAR  0xffc0141c   /* Pin Interrupt 0 Inversion Clear Register */
#define                   PINT0_PINSTATE  0xffc01420   /* Pin Interrupt 0 Pin Status Register */
#define                      PINT0_LATCH  0xffc01424   /* Pin Interrupt 0 Latch Register */

/* Port Interrupt 1 Registers (32-bit) */

#define                   PINT1_MASK_SET  0xffc01430   /* Pin Interrupt 1 Mask Set Register */
#define                 PINT1_MASK_CLEAR  0xffc01434   /* Pin Interrupt 1 Mask Clear Register */
#define                    PINT1_REQUEST  0xffc01438   /* Pin Interrupt 1 Interrupt Request Register */
#define                     PINT1_ASSIGN  0xffc0143c   /* Pin Interrupt 1 Port Assign Register */
#define                   PINT1_EDGE_SET  0xffc01440   /* Pin Interrupt 1 Edge-sensitivity Set Register */
#define                 PINT1_EDGE_CLEAR  0xffc01444   /* Pin Interrupt 1 Edge-sensitivity Clear Register */
#define                 PINT1_INVERT_SET  0xffc01448   /* Pin Interrupt 1 Inversion Set Register */
#define               PINT1_INVERT_CLEAR  0xffc0144c   /* Pin Interrupt 1 Inversion Clear Register */
#define                   PINT1_PINSTATE  0xffc01450   /* Pin Interrupt 1 Pin Status Register */
#define                      PINT1_LATCH  0xffc01454   /* Pin Interrupt 1 Latch Register */

/* Port Interrupt 2 Registers (32-bit) */

#define                   PINT2_MASK_SET  0xffc01460   /* Pin Interrupt 2 Mask Set Register */
#define                 PINT2_MASK_CLEAR  0xffc01464   /* Pin Interrupt 2 Mask Clear Register */
#define                    PINT2_REQUEST  0xffc01468   /* Pin Interrupt 2 Interrupt Request Register */
#define                     PINT2_ASSIGN  0xffc0146c   /* Pin Interrupt 2 Port Assign Register */
#define                   PINT2_EDGE_SET  0xffc01470   /* Pin Interrupt 2 Edge-sensitivity Set Register */
#define                 PINT2_EDGE_CLEAR  0xffc01474   /* Pin Interrupt 2 Edge-sensitivity Clear Register */
#define                 PINT2_INVERT_SET  0xffc01478   /* Pin Interrupt 2 Inversion Set Register */
#define               PINT2_INVERT_CLEAR  0xffc0147c   /* Pin Interrupt 2 Inversion Clear Register */
#define                   PINT2_PINSTATE  0xffc01480   /* Pin Interrupt 2 Pin Status Register */
#define                      PINT2_LATCH  0xffc01484   /* Pin Interrupt 2 Latch Register */

/* Port Interrupt 3 Registers (32-bit) */

#define                   PINT3_MASK_SET  0xffc01490   /* Pin Interrupt 3 Mask Set Register */
#define                 PINT3_MASK_CLEAR  0xffc01494   /* Pin Interrupt 3 Mask Clear Register */
#define                    PINT3_REQUEST  0xffc01498   /* Pin Interrupt 3 Interrupt Request Register */
#define                     PINT3_ASSIGN  0xffc0149c   /* Pin Interrupt 3 Port Assign Register */
#define                   PINT3_EDGE_SET  0xffc014a0   /* Pin Interrupt 3 Edge-sensitivity Set Register */
#define                 PINT3_EDGE_CLEAR  0xffc014a4   /* Pin Interrupt 3 Edge-sensitivity Clear Register */
#define                 PINT3_INVERT_SET  0xffc014a8   /* Pin Interrupt 3 Inversion Set Register */
#define               PINT3_INVERT_CLEAR  0xffc014ac   /* Pin Interrupt 3 Inversion Clear Register */
#define                   PINT3_PINSTATE  0xffc014b0   /* Pin Interrupt 3 Pin Status Register */
#define                      PINT3_LATCH  0xffc014b4   /* Pin Interrupt 3 Latch Register */

/* Port A Registers */

#define                        PORTA_FER  0xffc014c0   /* Function Enable Register */
#define                            PORTA  0xffc014c4   /* GPIO Data Register */
#define                        PORTA_SET  0xffc014c8   /* GPIO Data Set Register */
#define                      PORTA_CLEAR  0xffc014cc   /* GPIO Data Clear Register */
#define                    PORTA_DIR_SET  0xffc014d0   /* GPIO Direction Set Register */
#define                  PORTA_DIR_CLEAR  0xffc014d4   /* GPIO Direction Clear Register */
#define                       PORTA_INEN  0xffc014d8   /* GPIO Input Enable Register */
#define                        PORTA_MUX  0xffc014dc   /* Multiplexer Control Register */

/* Port B Registers */

#define                        PORTB_FER  0xffc014e0   /* Function Enable Register */
#define                            PORTB  0xffc014e4   /* GPIO Data Register */
#define                        PORTB_SET  0xffc014e8   /* GPIO Data Set Register */
#define                      PORTB_CLEAR  0xffc014ec   /* GPIO Data Clear Register */
#define                    PORTB_DIR_SET  0xffc014f0   /* GPIO Direction Set Register */
#define                  PORTB_DIR_CLEAR  0xffc014f4   /* GPIO Direction Clear Register */
#define                       PORTB_INEN  0xffc014f8   /* GPIO Input Enable Register */
#define                        PORTB_MUX  0xffc014fc   /* Multiplexer Control Register */

/* Port C Registers */

#define                        PORTC_FER  0xffc01500   /* Function Enable Register */
#define                            PORTC  0xffc01504   /* GPIO Data Register */
#define                        PORTC_SET  0xffc01508   /* GPIO Data Set Register */
#define                      PORTC_CLEAR  0xffc0150c   /* GPIO Data Clear Register */
#define                    PORTC_DIR_SET  0xffc01510   /* GPIO Direction Set Register */
#define                  PORTC_DIR_CLEAR  0xffc01514   /* GPIO Direction Clear Register */
#define                       PORTC_INEN  0xffc01518   /* GPIO Input Enable Register */
#define                        PORTC_MUX  0xffc0151c   /* Multiplexer Control Register */

/* Port D Registers */

#define                        PORTD_FER  0xffc01520   /* Function Enable Register */
#define                            PORTD  0xffc01524   /* GPIO Data Register */
#define                        PORTD_SET  0xffc01528   /* GPIO Data Set Register */
#define                      PORTD_CLEAR  0xffc0152c   /* GPIO Data Clear Register */
#define                    PORTD_DIR_SET  0xffc01530   /* GPIO Direction Set Register */
#define                  PORTD_DIR_CLEAR  0xffc01534   /* GPIO Direction Clear Register */
#define                       PORTD_INEN  0xffc01538   /* GPIO Input Enable Register */
#define                        PORTD_MUX  0xffc0153c   /* Multiplexer Control Register */

/* Port E Registers */

#define                        PORTE_FER  0xffc01540   /* Function Enable Register */
#define                            PORTE  0xffc01544   /* GPIO Data Register */
#define                        PORTE_SET  0xffc01548   /* GPIO Data Set Register */
#define                      PORTE_CLEAR  0xffc0154c   /* GPIO Data Clear Register */
#define                    PORTE_DIR_SET  0xffc01550   /* GPIO Direction Set Register */
#define                  PORTE_DIR_CLEAR  0xffc01554   /* GPIO Direction Clear Register */
#define                       PORTE_INEN  0xffc01558   /* GPIO Input Enable Register */
#define                        PORTE_MUX  0xffc0155c   /* Multiplexer Control Register */

/* Port F Registers */

#define                        PORTF_FER  0xffc01560   /* Function Enable Register */
#define                            PORTF  0xffc01564   /* GPIO Data Register */
#define                        PORTF_SET  0xffc01568   /* GPIO Data Set Register */
#define                      PORTF_CLEAR  0xffc0156c   /* GPIO Data Clear Register */
#define                    PORTF_DIR_SET  0xffc01570   /* GPIO Direction Set Register */
#define                  PORTF_DIR_CLEAR  0xffc01574   /* GPIO Direction Clear Register */
#define                       PORTF_INEN  0xffc01578   /* GPIO Input Enable Register */
#define                        PORTF_MUX  0xffc0157c   /* Multiplexer Control Register */

/* Port G Registers */

#define                        PORTG_FER  0xffc01580   /* Function Enable Register */
#define                            PORTG  0xffc01584   /* GPIO Data Register */
#define                        PORTG_SET  0xffc01588   /* GPIO Data Set Register */
#define                      PORTG_CLEAR  0xffc0158c   /* GPIO Data Clear Register */
#define                    PORTG_DIR_SET  0xffc01590   /* GPIO Direction Set Register */
#define                  PORTG_DIR_CLEAR  0xffc01594   /* GPIO Direction Clear Register */
#define                       PORTG_INEN  0xffc01598   /* GPIO Input Enable Register */
#define                        PORTG_MUX  0xffc0159c   /* Multiplexer Control Register */

/* Port H Registers */

#define                        PORTH_FER  0xffc015a0   /* Function Enable Register */
#define                            PORTH  0xffc015a4   /* GPIO Data Register */
#define                        PORTH_SET  0xffc015a8   /* GPIO Data Set Register */
#define                      PORTH_CLEAR  0xffc015ac   /* GPIO Data Clear Register */
#define                    PORTH_DIR_SET  0xffc015b0   /* GPIO Direction Set Register */
#define                  PORTH_DIR_CLEAR  0xffc015b4   /* GPIO Direction Clear Register */
#define                       PORTH_INEN  0xffc015b8   /* GPIO Input Enable Register */
#define                        PORTH_MUX  0xffc015bc   /* Multiplexer Control Register */

/* Port I Registers */

#define                        PORTI_FER  0xffc015c0   /* Function Enable Register */
#define                            PORTI  0xffc015c4   /* GPIO Data Register */
#define                        PORTI_SET  0xffc015c8   /* GPIO Data Set Register */
#define                      PORTI_CLEAR  0xffc015cc   /* GPIO Data Clear Register */
#define                    PORTI_DIR_SET  0xffc015d0   /* GPIO Direction Set Register */
#define                  PORTI_DIR_CLEAR  0xffc015d4   /* GPIO Direction Clear Register */
#define                       PORTI_INEN  0xffc015d8   /* GPIO Input Enable Register */
#define                        PORTI_MUX  0xffc015dc   /* Multiplexer Control Register */

/* Port J Registers */

#define                        PORTJ_FER  0xffc015e0   /* Function Enable Register */
#define                            PORTJ  0xffc015e4   /* GPIO Data Register */
#define                        PORTJ_SET  0xffc015e8   /* GPIO Data Set Register */
#define                      PORTJ_CLEAR  0xffc015ec   /* GPIO Data Clear Register */
#define                    PORTJ_DIR_SET  0xffc015f0   /* GPIO Direction Set Register */
#define                  PORTJ_DIR_CLEAR  0xffc015f4   /* GPIO Direction Clear Register */
#define                       PORTJ_INEN  0xffc015f8   /* GPIO Input Enable Register */
#define                        PORTJ_MUX  0xffc015fc   /* Multiplexer Control Register */

/* PWM Timer Registers */

#define                    TIMER0_CONFIG  0xffc01600   /* Timer 0 Configuration Register */
#define                   TIMER0_COUNTER  0xffc01604   /* Timer 0 Counter Register */
#define                    TIMER0_PERIOD  0xffc01608   /* Timer 0 Period Register */
#define                     TIMER0_WIDTH  0xffc0160c   /* Timer 0 Width Register */
#define                    TIMER1_CONFIG  0xffc01610   /* Timer 1 Configuration Register */
#define                   TIMER1_COUNTER  0xffc01614   /* Timer 1 Counter Register */
#define                    TIMER1_PERIOD  0xffc01618   /* Timer 1 Period Register */
#define                     TIMER1_WIDTH  0xffc0161c   /* Timer 1 Width Register */
#define                    TIMER2_CONFIG  0xffc01620   /* Timer 2 Configuration Register */
#define                   TIMER2_COUNTER  0xffc01624   /* Timer 2 Counter Register */
#define                    TIMER2_PERIOD  0xffc01628   /* Timer 2 Period Register */
#define                     TIMER2_WIDTH  0xffc0162c   /* Timer 2 Width Register */
#define                    TIMER3_CONFIG  0xffc01630   /* Timer 3 Configuration Register */
#define                   TIMER3_COUNTER  0xffc01634   /* Timer 3 Counter Register */
#define                    TIMER3_PERIOD  0xffc01638   /* Timer 3 Period Register */
#define                     TIMER3_WIDTH  0xffc0163c   /* Timer 3 Width Register */
#define                    TIMER4_CONFIG  0xffc01640   /* Timer 4 Configuration Register */
#define                   TIMER4_COUNTER  0xffc01644   /* Timer 4 Counter Register */
#define                    TIMER4_PERIOD  0xffc01648   /* Timer 4 Period Register */
#define                     TIMER4_WIDTH  0xffc0164c   /* Timer 4 Width Register */
#define                    TIMER5_CONFIG  0xffc01650   /* Timer 5 Configuration Register */
#define                   TIMER5_COUNTER  0xffc01654   /* Timer 5 Counter Register */
#define                    TIMER5_PERIOD  0xffc01658   /* Timer 5 Period Register */
#define                     TIMER5_WIDTH  0xffc0165c   /* Timer 5 Width Register */
#define                    TIMER6_CONFIG  0xffc01660   /* Timer 6 Configuration Register */
#define                   TIMER6_COUNTER  0xffc01664   /* Timer 6 Counter Register */
#define                    TIMER6_PERIOD  0xffc01668   /* Timer 6 Period Register */
#define                     TIMER6_WIDTH  0xffc0166c   /* Timer 6 Width Register */
#define                    TIMER7_CONFIG  0xffc01670   /* Timer 7 Configuration Register */
#define                   TIMER7_COUNTER  0xffc01674   /* Timer 7 Counter Register */
#define                    TIMER7_PERIOD  0xffc01678   /* Timer 7 Period Register */
#define                     TIMER7_WIDTH  0xffc0167c   /* Timer 7 Width Register */

/* Timer Group of 8 */

#define                    TIMER_ENABLE0  0xffc01680   /* Timer Group of 8 Enable Register */
#define                   TIMER_DISABLE0  0xffc01684   /* Timer Group of 8 Disable Register */
#define                    TIMER_STATUS0  0xffc01688   /* Timer Group of 8 Status Register */

/* DMAC1 Registers */

#define                      DMAC1_TCPER  0xffc01b0c   /* DMA Controller 1 Traffic Control Periods Register */
#define                      DMAC1_TCCNT  0xffc01b10   /* DMA Controller 1 Current Counts Register */

/* DMA Channel 12 Registers */

#define              DMA12_NEXT_DESC_PTR  0xffc01c00   /* DMA Channel 12 Next Descriptor Pointer Register */
#define                 DMA12_START_ADDR  0xffc01c04   /* DMA Channel 12 Start Address Register */
#define                     DMA12_CONFIG  0xffc01c08   /* DMA Channel 12 Configuration Register */
#define                    DMA12_X_COUNT  0xffc01c10   /* DMA Channel 12 X Count Register */
#define                   DMA12_X_MODIFY  0xffc01c14   /* DMA Channel 12 X Modify Register */
#define                    DMA12_Y_COUNT  0xffc01c18   /* DMA Channel 12 Y Count Register */
#define                   DMA12_Y_MODIFY  0xffc01c1c   /* DMA Channel 12 Y Modify Register */
#define              DMA12_CURR_DESC_PTR  0xffc01c20   /* DMA Channel 12 Current Descriptor Pointer Register */
#define                  DMA12_CURR_ADDR  0xffc01c24   /* DMA Channel 12 Current Address Register */
#define                 DMA12_IRQ_STATUS  0xffc01c28   /* DMA Channel 12 Interrupt/Status Register */
#define             DMA12_PERIPHERAL_MAP  0xffc01c2c   /* DMA Channel 12 Peripheral Map Register */
#define               DMA12_CURR_X_COUNT  0xffc01c30   /* DMA Channel 12 Current X Count Register */
#define               DMA12_CURR_Y_COUNT  0xffc01c38   /* DMA Channel 12 Current Y Count Register */

/* DMA Channel 13 Registers */

#define              DMA13_NEXT_DESC_PTR  0xffc01c40   /* DMA Channel 13 Next Descriptor Pointer Register */
#define                 DMA13_START_ADDR  0xffc01c44   /* DMA Channel 13 Start Address Register */
#define                     DMA13_CONFIG  0xffc01c48   /* DMA Channel 13 Configuration Register */
#define                    DMA13_X_COUNT  0xffc01c50   /* DMA Channel 13 X Count Register */
#define                   DMA13_X_MODIFY  0xffc01c54   /* DMA Channel 13 X Modify Register */
#define                    DMA13_Y_COUNT  0xffc01c58   /* DMA Channel 13 Y Count Register */
#define                   DMA13_Y_MODIFY  0xffc01c5c   /* DMA Channel 13 Y Modify Register */
#define              DMA13_CURR_DESC_PTR  0xffc01c60   /* DMA Channel 13 Current Descriptor Pointer Register */
#define                  DMA13_CURR_ADDR  0xffc01c64   /* DMA Channel 13 Current Address Register */
#define                 DMA13_IRQ_STATUS  0xffc01c68   /* DMA Channel 13 Interrupt/Status Register */
#define             DMA13_PERIPHERAL_MAP  0xffc01c6c   /* DMA Channel 13 Peripheral Map Register */
#define               DMA13_CURR_X_COUNT  0xffc01c70   /* DMA Channel 13 Current X Count Register */
#define               DMA13_CURR_Y_COUNT  0xffc01c78   /* DMA Channel 13 Current Y Count Register */

/* DMA Channel 14 Registers */

#define              DMA14_NEXT_DESC_PTR  0xffc01c80   /* DMA Channel 14 Next Descriptor Pointer Register */
#define                 DMA14_START_ADDR  0xffc01c84   /* DMA Channel 14 Start Address Register */
#define                     DMA14_CONFIG  0xffc01c88   /* DMA Channel 14 Configuration Register */
#define                    DMA14_X_COUNT  0xffc01c90   /* DMA Channel 14 X Count Register */
#define                   DMA14_X_MODIFY  0xffc01c94   /* DMA Channel 14 X Modify Register */
#define                    DMA14_Y_COUNT  0xffc01c98   /* DMA Channel 14 Y Count Register */
#define                   DMA14_Y_MODIFY  0xffc01c9c   /* DMA Channel 14 Y Modify Register */
#define              DMA14_CURR_DESC_PTR  0xffc01ca0   /* DMA Channel 14 Current Descriptor Pointer Register */
#define                  DMA14_CURR_ADDR  0xffc01ca4   /* DMA Channel 14 Current Address Register */
#define                 DMA14_IRQ_STATUS  0xffc01ca8   /* DMA Channel 14 Interrupt/Status Register */
#define             DMA14_PERIPHERAL_MAP  0xffc01cac   /* DMA Channel 14 Peripheral Map Register */
#define               DMA14_CURR_X_COUNT  0xffc01cb0   /* DMA Channel 14 Current X Count Register */
#define               DMA14_CURR_Y_COUNT  0xffc01cb8   /* DMA Channel 14 Current Y Count Register */

/* DMA Channel 15 Registers */

#define              DMA15_NEXT_DESC_PTR  0xffc01cc0   /* DMA Channel 15 Next Descriptor Pointer Register */
#define                 DMA15_START_ADDR  0xffc01cc4   /* DMA Channel 15 Start Address Register */
#define                     DMA15_CONFIG  0xffc01cc8   /* DMA Channel 15 Configuration Register */
#define                    DMA15_X_COUNT  0xffc01cd0   /* DMA Channel 15 X Count Register */
#define                   DMA15_X_MODIFY  0xffc01cd4   /* DMA Channel 15 X Modify Register */
#define                    DMA15_Y_COUNT  0xffc01cd8   /* DMA Channel 15 Y Count Register */
#define                   DMA15_Y_MODIFY  0xffc01cdc   /* DMA Channel 15 Y Modify Register */
#define              DMA15_CURR_DESC_PTR  0xffc01ce0   /* DMA Channel 15 Current Descriptor Pointer Register */
#define                  DMA15_CURR_ADDR  0xffc01ce4   /* DMA Channel 15 Current Address Register */
#define                 DMA15_IRQ_STATUS  0xffc01ce8   /* DMA Channel 15 Interrupt/Status Register */
#define             DMA15_PERIPHERAL_MAP  0xffc01cec   /* DMA Channel 15 Peripheral Map Register */
#define               DMA15_CURR_X_COUNT  0xffc01cf0   /* DMA Channel 15 Current X Count Register */
#define               DMA15_CURR_Y_COUNT  0xffc01cf8   /* DMA Channel 15 Current Y Count Register */

/* DMA Channel 16 Registers */

#define              DMA16_NEXT_DESC_PTR  0xffc01d00   /* DMA Channel 16 Next Descriptor Pointer Register */
#define                 DMA16_START_ADDR  0xffc01d04   /* DMA Channel 16 Start Address Register */
#define                     DMA16_CONFIG  0xffc01d08   /* DMA Channel 16 Configuration Register */
#define                    DMA16_X_COUNT  0xffc01d10   /* DMA Channel 16 X Count Register */
#define                   DMA16_X_MODIFY  0xffc01d14   /* DMA Channel 16 X Modify Register */
#define                    DMA16_Y_COUNT  0xffc01d18   /* DMA Channel 16 Y Count Register */
#define                   DMA16_Y_MODIFY  0xffc01d1c   /* DMA Channel 16 Y Modify Register */
#define              DMA16_CURR_DESC_PTR  0xffc01d20   /* DMA Channel 16 Current Descriptor Pointer Register */
#define                  DMA16_CURR_ADDR  0xffc01d24   /* DMA Channel 16 Current Address Register */
#define                 DMA16_IRQ_STATUS  0xffc01d28   /* DMA Channel 16 Interrupt/Status Register */
#define             DMA16_PERIPHERAL_MAP  0xffc01d2c   /* DMA Channel 16 Peripheral Map Register */
#define               DMA16_CURR_X_COUNT  0xffc01d30   /* DMA Channel 16 Current X Count Register */
#define               DMA16_CURR_Y_COUNT  0xffc01d38   /* DMA Channel 16 Current Y Count Register */

/* DMA Channel 17 Registers */

#define              DMA17_NEXT_DESC_PTR  0xffc01d40   /* DMA Channel 17 Next Descriptor Pointer Register */
#define                 DMA17_START_ADDR  0xffc01d44   /* DMA Channel 17 Start Address Register */
#define                     DMA17_CONFIG  0xffc01d48   /* DMA Channel 17 Configuration Register */
#define                    DMA17_X_COUNT  0xffc01d50   /* DMA Channel 17 X Count Register */
#define                   DMA17_X_MODIFY  0xffc01d54   /* DMA Channel 17 X Modify Register */
#define                    DMA17_Y_COUNT  0xffc01d58   /* DMA Channel 17 Y Count Register */
#define                   DMA17_Y_MODIFY  0xffc01d5c   /* DMA Channel 17 Y Modify Register */
#define              DMA17_CURR_DESC_PTR  0xffc01d60   /* DMA Channel 17 Current Descriptor Pointer Register */
#define                  DMA17_CURR_ADDR  0xffc01d64   /* DMA Channel 17 Current Address Register */
#define                 DMA17_IRQ_STATUS  0xffc01d68   /* DMA Channel 17 Interrupt/Status Register */
#define             DMA17_PERIPHERAL_MAP  0xffc01d6c   /* DMA Channel 17 Peripheral Map Register */
#define               DMA17_CURR_X_COUNT  0xffc01d70   /* DMA Channel 17 Current X Count Register */
#define               DMA17_CURR_Y_COUNT  0xffc01d78   /* DMA Channel 17 Current Y Count Register */

/* DMA Channel 18 Registers */

#define              DMA18_NEXT_DESC_PTR  0xffc01d80   /* DMA Channel 18 Next Descriptor Pointer Register */
#define                 DMA18_START_ADDR  0xffc01d84   /* DMA Channel 18 Start Address Register */
#define                     DMA18_CONFIG  0xffc01d88   /* DMA Channel 18 Configuration Register */
#define                    DMA18_X_COUNT  0xffc01d90   /* DMA Channel 18 X Count Register */
#define                   DMA18_X_MODIFY  0xffc01d94   /* DMA Channel 18 X Modify Register */
#define                    DMA18_Y_COUNT  0xffc01d98   /* DMA Channel 18 Y Count Register */
#define                   DMA18_Y_MODIFY  0xffc01d9c   /* DMA Channel 18 Y Modify Register */
#define              DMA18_CURR_DESC_PTR  0xffc01da0   /* DMA Channel 18 Current Descriptor Pointer Register */
#define                  DMA18_CURR_ADDR  0xffc01da4   /* DMA Channel 18 Current Address Register */
#define                 DMA18_IRQ_STATUS  0xffc01da8   /* DMA Channel 18 Interrupt/Status Register */
#define             DMA18_PERIPHERAL_MAP  0xffc01dac   /* DMA Channel 18 Peripheral Map Register */
#define               DMA18_CURR_X_COUNT  0xffc01db0   /* DMA Channel 18 Current X Count Register */
#define               DMA18_CURR_Y_COUNT  0xffc01db8   /* DMA Channel 18 Current Y Count Register */

/* DMA Channel 19 Registers */

#define              DMA19_NEXT_DESC_PTR  0xffc01dc0   /* DMA Channel 19 Next Descriptor Pointer Register */
#define                 DMA19_START_ADDR  0xffc01dc4   /* DMA Channel 19 Start Address Register */
#define                     DMA19_CONFIG  0xffc01dc8   /* DMA Channel 19 Configuration Register */
#define                    DMA19_X_COUNT  0xffc01dd0   /* DMA Channel 19 X Count Register */
#define                   DMA19_X_MODIFY  0xffc01dd4   /* DMA Channel 19 X Modify Register */
#define                    DMA19_Y_COUNT  0xffc01dd8   /* DMA Channel 19 Y Count Register */
#define                   DMA19_Y_MODIFY  0xffc01ddc   /* DMA Channel 19 Y Modify Register */
#define              DMA19_CURR_DESC_PTR  0xffc01de0   /* DMA Channel 19 Current Descriptor Pointer Register */
#define                  DMA19_CURR_ADDR  0xffc01de4   /* DMA Channel 19 Current Address Register */
#define                 DMA19_IRQ_STATUS  0xffc01de8   /* DMA Channel 19 Interrupt/Status Register */
#define             DMA19_PERIPHERAL_MAP  0xffc01dec   /* DMA Channel 19 Peripheral Map Register */
#define               DMA19_CURR_X_COUNT  0xffc01df0   /* DMA Channel 19 Current X Count Register */
#define               DMA19_CURR_Y_COUNT  0xffc01df8   /* DMA Channel 19 Current Y Count Register */

/* DMA Channel 20 Registers */

#define              DMA20_NEXT_DESC_PTR  0xffc01e00   /* DMA Channel 20 Next Descriptor Pointer Register */
#define                 DMA20_START_ADDR  0xffc01e04   /* DMA Channel 20 Start Address Register */
#define                     DMA20_CONFIG  0xffc01e08   /* DMA Channel 20 Configuration Register */
#define                    DMA20_X_COUNT  0xffc01e10   /* DMA Channel 20 X Count Register */
#define                   DMA20_X_MODIFY  0xffc01e14   /* DMA Channel 20 X Modify Register */
#define                    DMA20_Y_COUNT  0xffc01e18   /* DMA Channel 20 Y Count Register */
#define                   DMA20_Y_MODIFY  0xffc01e1c   /* DMA Channel 20 Y Modify Register */
#define              DMA20_CURR_DESC_PTR  0xffc01e20   /* DMA Channel 20 Current Descriptor Pointer Register */
#define                  DMA20_CURR_ADDR  0xffc01e24   /* DMA Channel 20 Current Address Register */
#define                 DMA20_IRQ_STATUS  0xffc01e28   /* DMA Channel 20 Interrupt/Status Register */
#define             DMA20_PERIPHERAL_MAP  0xffc01e2c   /* DMA Channel 20 Peripheral Map Register */
#define               DMA20_CURR_X_COUNT  0xffc01e30   /* DMA Channel 20 Current X Count Register */
#define               DMA20_CURR_Y_COUNT  0xffc01e38   /* DMA Channel 20 Current Y Count Register */

/* DMA Channel 21 Registers */

#define              DMA21_NEXT_DESC_PTR  0xffc01e40   /* DMA Channel 21 Next Descriptor Pointer Register */
#define                 DMA21_START_ADDR  0xffc01e44   /* DMA Channel 21 Start Address Register */
#define                     DMA21_CONFIG  0xffc01e48   /* DMA Channel 21 Configuration Register */
#define                    DMA21_X_COUNT  0xffc01e50   /* DMA Channel 21 X Count Register */
#define                   DMA21_X_MODIFY  0xffc01e54   /* DMA Channel 21 X Modify Register */
#define                    DMA21_Y_COUNT  0xffc01e58   /* DMA Channel 21 Y Count Register */
#define                   DMA21_Y_MODIFY  0xffc01e5c   /* DMA Channel 21 Y Modify Register */
#define              DMA21_CURR_DESC_PTR  0xffc01e60   /* DMA Channel 21 Current Descriptor Pointer Register */
#define                  DMA21_CURR_ADDR  0xffc01e64   /* DMA Channel 21 Current Address Register */
#define                 DMA21_IRQ_STATUS  0xffc01e68   /* DMA Channel 21 Interrupt/Status Register */
#define             DMA21_PERIPHERAL_MAP  0xffc01e6c   /* DMA Channel 21 Peripheral Map Register */
#define               DMA21_CURR_X_COUNT  0xffc01e70   /* DMA Channel 21 Current X Count Register */
#define               DMA21_CURR_Y_COUNT  0xffc01e78   /* DMA Channel 21 Current Y Count Register */

/* DMA Channel 22 Registers */

#define              DMA22_NEXT_DESC_PTR  0xffc01e80   /* DMA Channel 22 Next Descriptor Pointer Register */
#define                 DMA22_START_ADDR  0xffc01e84   /* DMA Channel 22 Start Address Register */
#define                     DMA22_CONFIG  0xffc01e88   /* DMA Channel 22 Configuration Register */
#define                    DMA22_X_COUNT  0xffc01e90   /* DMA Channel 22 X Count Register */
#define                   DMA22_X_MODIFY  0xffc01e94   /* DMA Channel 22 X Modify Register */
#define                    DMA22_Y_COUNT  0xffc01e98   /* DMA Channel 22 Y Count Register */
#define                   DMA22_Y_MODIFY  0xffc01e9c   /* DMA Channel 22 Y Modify Register */
#define              DMA22_CURR_DESC_PTR  0xffc01ea0   /* DMA Channel 22 Current Descriptor Pointer Register */
#define                  DMA22_CURR_ADDR  0xffc01ea4   /* DMA Channel 22 Current Address Register */
#define                 DMA22_IRQ_STATUS  0xffc01ea8   /* DMA Channel 22 Interrupt/Status Register */
#define             DMA22_PERIPHERAL_MAP  0xffc01eac   /* DMA Channel 22 Peripheral Map Register */
#define               DMA22_CURR_X_COUNT  0xffc01eb0   /* DMA Channel 22 Current X Count Register */
#define               DMA22_CURR_Y_COUNT  0xffc01eb8   /* DMA Channel 22 Current Y Count Register */

/* DMA Channel 23 Registers */

#define              DMA23_NEXT_DESC_PTR  0xffc01ec0   /* DMA Channel 23 Next Descriptor Pointer Register */
#define                 DMA23_START_ADDR  0xffc01ec4   /* DMA Channel 23 Start Address Register */
#define                     DMA23_CONFIG  0xffc01ec8   /* DMA Channel 23 Configuration Register */
#define                    DMA23_X_COUNT  0xffc01ed0   /* DMA Channel 23 X Count Register */
#define                   DMA23_X_MODIFY  0xffc01ed4   /* DMA Channel 23 X Modify Register */
#define                    DMA23_Y_COUNT  0xffc01ed8   /* DMA Channel 23 Y Count Register */
#define                   DMA23_Y_MODIFY  0xffc01edc   /* DMA Channel 23 Y Modify Register */
#define              DMA23_CURR_DESC_PTR  0xffc01ee0   /* DMA Channel 23 Current Descriptor Pointer Register */
#define                  DMA23_CURR_ADDR  0xffc01ee4   /* DMA Channel 23 Current Address Register */
#define                 DMA23_IRQ_STATUS  0xffc01ee8   /* DMA Channel 23 Interrupt/Status Register */
#define             DMA23_PERIPHERAL_MAP  0xffc01eec   /* DMA Channel 23 Peripheral Map Register */
#define               DMA23_CURR_X_COUNT  0xffc01ef0   /* DMA Channel 23 Current X Count Register */
#define               DMA23_CURR_Y_COUNT  0xffc01ef8   /* DMA Channel 23 Current Y Count Register */

/* MDMA Stream 2 Registers */

#define            MDMA_D2_NEXT_DESC_PTR  0xffc01f00   /* Memory DMA Stream 2 Destination Next Descriptor Pointer Register */
#define               MDMA_D2_START_ADDR  0xffc01f04   /* Memory DMA Stream 2 Destination Start Address Register */
#define                   MDMA_D2_CONFIG  0xffc01f08   /* Memory DMA Stream 2 Destination Configuration Register */
#define                  MDMA_D2_X_COUNT  0xffc01f10   /* Memory DMA Stream 2 Destination X Count Register */
#define                 MDMA_D2_X_MODIFY  0xffc01f14   /* Memory DMA Stream 2 Destination X Modify Register */
#define                  MDMA_D2_Y_COUNT  0xffc01f18   /* Memory DMA Stream 2 Destination Y Count Register */
#define                 MDMA_D2_Y_MODIFY  0xffc01f1c   /* Memory DMA Stream 2 Destination Y Modify Register */
#define            MDMA_D2_CURR_DESC_PTR  0xffc01f20   /* Memory DMA Stream 2 Destination Current Descriptor Pointer Register */
#define                MDMA_D2_CURR_ADDR  0xffc01f24   /* Memory DMA Stream 2 Destination Current Address Register */
#define               MDMA_D2_IRQ_STATUS  0xffc01f28   /* Memory DMA Stream 2 Destination Interrupt/Status Register */
#define           MDMA_D2_PERIPHERAL_MAP  0xffc01f2c   /* Memory DMA Stream 2 Destination Peripheral Map Register */
#define             MDMA_D2_CURR_X_COUNT  0xffc01f30   /* Memory DMA Stream 2 Destination Current X Count Register */
#define             MDMA_D2_CURR_Y_COUNT  0xffc01f38   /* Memory DMA Stream 2 Destination Current Y Count Register */
#define            MDMA_S2_NEXT_DESC_PTR  0xffc01f40   /* Memory DMA Stream 2 Source Next Descriptor Pointer Register */
#define               MDMA_S2_START_ADDR  0xffc01f44   /* Memory DMA Stream 2 Source Start Address Register */
#define                   MDMA_S2_CONFIG  0xffc01f48   /* Memory DMA Stream 2 Source Configuration Register */
#define                  MDMA_S2_X_COUNT  0xffc01f50   /* Memory DMA Stream 2 Source X Count Register */
#define                 MDMA_S2_X_MODIFY  0xffc01f54   /* Memory DMA Stream 2 Source X Modify Register */
#define                  MDMA_S2_Y_COUNT  0xffc01f58   /* Memory DMA Stream 2 Source Y Count Register */
#define                 MDMA_S2_Y_MODIFY  0xffc01f5c   /* Memory DMA Stream 2 Source Y Modify Register */
#define            MDMA_S2_CURR_DESC_PTR  0xffc01f60   /* Memory DMA Stream 2 Source Current Descriptor Pointer Register */
#define                MDMA_S2_CURR_ADDR  0xffc01f64   /* Memory DMA Stream 2 Source Current Address Register */
#define               MDMA_S2_IRQ_STATUS  0xffc01f68   /* Memory DMA Stream 2 Source Interrupt/Status Register */
#define           MDMA_S2_PERIPHERAL_MAP  0xffc01f6c   /* Memory DMA Stream 2 Source Peripheral Map Register */
#define             MDMA_S2_CURR_X_COUNT  0xffc01f70   /* Memory DMA Stream 2 Source Current X Count Register */
#define             MDMA_S2_CURR_Y_COUNT  0xffc01f78   /* Memory DMA Stream 2 Source Current Y Count Register */

/* MDMA Stream 3 Registers */

#define            MDMA_D3_NEXT_DESC_PTR  0xffc01f80   /* Memory DMA Stream 3 Destination Next Descriptor Pointer Register */
#define               MDMA_D3_START_ADDR  0xffc01f84   /* Memory DMA Stream 3 Destination Start Address Register */
#define                   MDMA_D3_CONFIG  0xffc01f88   /* Memory DMA Stream 3 Destination Configuration Register */
#define                  MDMA_D3_X_COUNT  0xffc01f90   /* Memory DMA Stream 3 Destination X Count Register */
#define                 MDMA_D3_X_MODIFY  0xffc01f94   /* Memory DMA Stream 3 Destination X Modify Register */
#define                  MDMA_D3_Y_COUNT  0xffc01f98   /* Memory DMA Stream 3 Destination Y Count Register */
#define                 MDMA_D3_Y_MODIFY  0xffc01f9c   /* Memory DMA Stream 3 Destination Y Modify Register */
#define            MDMA_D3_CURR_DESC_PTR  0xffc01fa0   /* Memory DMA Stream 3 Destination Current Descriptor Pointer Register */
#define                MDMA_D3_CURR_ADDR  0xffc01fa4   /* Memory DMA Stream 3 Destination Current Address Register */
#define               MDMA_D3_IRQ_STATUS  0xffc01fa8   /* Memory DMA Stream 3 Destination Interrupt/Status Register */
#define           MDMA_D3_PERIPHERAL_MAP  0xffc01fac   /* Memory DMA Stream 3 Destination Peripheral Map Register */
#define             MDMA_D3_CURR_X_COUNT  0xffc01fb0   /* Memory DMA Stream 3 Destination Current X Count Register */
#define             MDMA_D3_CURR_Y_COUNT  0xffc01fb8   /* Memory DMA Stream 3 Destination Current Y Count Register */
#define            MDMA_S3_NEXT_DESC_PTR  0xffc01fc0   /* Memory DMA Stream 3 Source Next Descriptor Pointer Register */
#define               MDMA_S3_START_ADDR  0xffc01fc4   /* Memory DMA Stream 3 Source Start Address Register */
#define                   MDMA_S3_CONFIG  0xffc01fc8   /* Memory DMA Stream 3 Source Configuration Register */
#define                  MDMA_S3_X_COUNT  0xffc01fd0   /* Memory DMA Stream 3 Source X Count Register */
#define                 MDMA_S3_X_MODIFY  0xffc01fd4   /* Memory DMA Stream 3 Source X Modify Register */
#define                  MDMA_S3_Y_COUNT  0xffc01fd8   /* Memory DMA Stream 3 Source Y Count Register */
#define                 MDMA_S3_Y_MODIFY  0xffc01fdc   /* Memory DMA Stream 3 Source Y Modify Register */
#define            MDMA_S3_CURR_DESC_PTR  0xffc01fe0   /* Memory DMA Stream 3 Source Current Descriptor Pointer Register */
#define                MDMA_S3_CURR_ADDR  0xffc01fe4   /* Memory DMA Stream 3 Source Current Address Register */
#define               MDMA_S3_IRQ_STATUS  0xffc01fe8   /* Memory DMA Stream 3 Source Interrupt/Status Register */
#define           MDMA_S3_PERIPHERAL_MAP  0xffc01fec   /* Memory DMA Stream 3 Source Peripheral Map Register */
#define             MDMA_S3_CURR_X_COUNT  0xffc01ff0   /* Memory DMA Stream 3 Source Current X Count Register */
#define             MDMA_S3_CURR_Y_COUNT  0xffc01ff8   /* Memory DMA Stream 3 Source Current Y Count Register */

/* UART1 Registers */

#define                        UART1_DLL  0xffc02000   /* Divisor Latch Low Byte */
#define                        UART1_DLH  0xffc02004   /* Divisor Latch High Byte */
#define                       UART1_GCTL  0xffc02008   /* Global Control Register */
#define                        UART1_LCR  0xffc0200c   /* Line Control Register */
#define                        UART1_MCR  0xffc02010   /* Modem Control Register */
#define                        UART1_LSR  0xffc02014   /* Line Status Register */
#define                        UART1_MSR  0xffc02018   /* Modem Status Register */
#define                        UART1_SCR  0xffc0201c   /* Scratch Register */
#define                    UART1_IER_SET  0xffc02020   /* Interrupt Enable Register Set */
#define                  UART1_IER_CLEAR  0xffc02024   /* Interrupt Enable Register Clear */
#define                        UART1_THR  0xffc02028   /* Transmit Hold Register */
#define                        UART1_RBR  0xffc0202c   /* Receive Buffer Register */

/* UART2 is not defined in the shared file because it is not available on the ADSP-BF542 and ADSP-BF544 processors */

/* SPI1 Registers */

#define                     SPI1_REGBASE  0xffc02300
#define                         SPI1_CTL  0xffc02300   /* SPI1 Control Register */
#define                         SPI1_FLG  0xffc02304   /* SPI1 Flag Register */
#define                        SPI1_STAT  0xffc02308   /* SPI1 Status Register */
#define                        SPI1_TDBR  0xffc0230c   /* SPI1 Transmit Data Buffer Register */
#define                        SPI1_RDBR  0xffc02310   /* SPI1 Receive Data Buffer Register */
#define                        SPI1_BAUD  0xffc02314   /* SPI1 Baud Rate Register */
#define                      SPI1_SHADOW  0xffc02318   /* SPI1 Receive Data Buffer Shadow Register */

/* SPORT2 Registers */

#define                      SPORT2_TCR1  0xffc02500   /* SPORT2 Transmit Configuration 1 Register */
#define                      SPORT2_TCR2  0xffc02504   /* SPORT2 Transmit Configuration 2 Register */
#define                   SPORT2_TCLKDIV  0xffc02508   /* SPORT2 Transmit Serial Clock Divider Register */
#define                    SPORT2_TFSDIV  0xffc0250c   /* SPORT2 Transmit Frame Sync Divider Register */
#define                        SPORT2_TX  0xffc02510   /* SPORT2 Transmit Data Register */
#define                        SPORT2_RX  0xffc02518   /* SPORT2 Receive Data Register */
#define                      SPORT2_RCR1  0xffc02520   /* SPORT2 Receive Configuration 1 Register */
#define                      SPORT2_RCR2  0xffc02524   /* SPORT2 Receive Configuration 2 Register */
#define                   SPORT2_RCLKDIV  0xffc02528   /* SPORT2 Receive Serial Clock Divider Register */
#define                    SPORT2_RFSDIV  0xffc0252c   /* SPORT2 Receive Frame Sync Divider Register */
#define                      SPORT2_STAT  0xffc02530   /* SPORT2 Status Register */
#define                      SPORT2_CHNL  0xffc02534   /* SPORT2 Current Channel Register */
#define                     SPORT2_MCMC1  0xffc02538   /* SPORT2 Multi channel Configuration Register 1 */
#define                     SPORT2_MCMC2  0xffc0253c   /* SPORT2 Multi channel Configuration Register 2 */
#define                     SPORT2_MTCS0  0xffc02540   /* SPORT2 Multi channel Transmit Select Register 0 */
#define                     SPORT2_MTCS1  0xffc02544   /* SPORT2 Multi channel Transmit Select Register 1 */
#define                     SPORT2_MTCS2  0xffc02548   /* SPORT2 Multi channel Transmit Select Register 2 */
#define                     SPORT2_MTCS3  0xffc0254c   /* SPORT2 Multi channel Transmit Select Register 3 */
#define                     SPORT2_MRCS0  0xffc02550   /* SPORT2 Multi channel Receive Select Register 0 */
#define                     SPORT2_MRCS1  0xffc02554   /* SPORT2 Multi channel Receive Select Register 1 */
#define                     SPORT2_MRCS2  0xffc02558   /* SPORT2 Multi channel Receive Select Register 2 */
#define                     SPORT2_MRCS3  0xffc0255c   /* SPORT2 Multi channel Receive Select Register 3 */

/* SPORT3 Registers */

#define                      SPORT3_TCR1  0xffc02600   /* SPORT3 Transmit Configuration 1 Register */
#define                      SPORT3_TCR2  0xffc02604   /* SPORT3 Transmit Configuration 2 Register */
#define                   SPORT3_TCLKDIV  0xffc02608   /* SPORT3 Transmit Serial Clock Divider Register */
#define                    SPORT3_TFSDIV  0xffc0260c   /* SPORT3 Transmit Frame Sync Divider Register */
#define                        SPORT3_TX  0xffc02610   /* SPORT3 Transmit Data Register */
#define                        SPORT3_RX  0xffc02618   /* SPORT3 Receive Data Register */
#define                      SPORT3_RCR1  0xffc02620   /* SPORT3 Receive Configuration 1 Register */
#define                      SPORT3_RCR2  0xffc02624   /* SPORT3 Receive Configuration 2 Register */
#define                   SPORT3_RCLKDIV  0xffc02628   /* SPORT3 Receive Serial Clock Divider Register */
#define                    SPORT3_RFSDIV  0xffc0262c   /* SPORT3 Receive Frame Sync Divider Register */
#define                      SPORT3_STAT  0xffc02630   /* SPORT3 Status Register */
#define                      SPORT3_CHNL  0xffc02634   /* SPORT3 Current Channel Register */
#define                     SPORT3_MCMC1  0xffc02638   /* SPORT3 Multi channel Configuration Register 1 */
#define                     SPORT3_MCMC2  0xffc0263c   /* SPORT3 Multi channel Configuration Register 2 */
#define                     SPORT3_MTCS0  0xffc02640   /* SPORT3 Multi channel Transmit Select Register 0 */
#define                     SPORT3_MTCS1  0xffc02644   /* SPORT3 Multi channel Transmit Select Register 1 */
#define                     SPORT3_MTCS2  0xffc02648   /* SPORT3 Multi channel Transmit Select Register 2 */
#define                     SPORT3_MTCS3  0xffc0264c   /* SPORT3 Multi channel Transmit Select Register 3 */
#define                     SPORT3_MRCS0  0xffc02650   /* SPORT3 Multi channel Receive Select Register 0 */
#define                     SPORT3_MRCS1  0xffc02654   /* SPORT3 Multi channel Receive Select Register 1 */
#define                     SPORT3_MRCS2  0xffc02658   /* SPORT3 Multi channel Receive Select Register 2 */
#define                     SPORT3_MRCS3  0xffc0265c   /* SPORT3 Multi channel Receive Select Register 3 */

/* EPPI2 Registers */

#define                     EPPI2_STATUS  0xffc02900   /* EPPI2 Status Register */
#define                     EPPI2_HCOUNT  0xffc02904   /* EPPI2 Horizontal Transfer Count Register */
#define                     EPPI2_HDELAY  0xffc02908   /* EPPI2 Horizontal Delay Count Register */
#define                     EPPI2_VCOUNT  0xffc0290c   /* EPPI2 Vertical Transfer Count Register */
#define                     EPPI2_VDELAY  0xffc02910   /* EPPI2 Vertical Delay Count Register */
#define                      EPPI2_FRAME  0xffc02914   /* EPPI2 Lines per Frame Register */
#define                       EPPI2_LINE  0xffc02918   /* EPPI2 Samples per Line Register */
#define                     EPPI2_CLKDIV  0xffc0291c   /* EPPI2 Clock Divide Register */
#define                    EPPI2_CONTROL  0xffc02920   /* EPPI2 Control Register */
#define                   EPPI2_FS1W_HBL  0xffc02924   /* EPPI2 FS1 Width Register / EPPI2 Horizontal Blanking Samples Per Line Register */
#define                  EPPI2_FS1P_AVPL  0xffc02928   /* EPPI2 FS1 Period Register / EPPI2 Active Video Samples Per Line Register */
#define                   EPPI2_FS2W_LVB  0xffc0292c   /* EPPI2 FS2 Width Register / EPPI2 Lines of Vertical Blanking Register */
#define                  EPPI2_FS2P_LAVF  0xffc02930   /* EPPI2 FS2 Period Register/ EPPI2 Lines of Active Video Per Field Register */
#define                       EPPI2_CLIP  0xffc02934   /* EPPI2 Clipping Register */

/* CAN Controller 0 Config 1 Registers */

#define                         CAN0_MC1  0xffc02a00   /* CAN Controller 0 Mailbox Configuration Register 1 */
#define                         CAN0_MD1  0xffc02a04   /* CAN Controller 0 Mailbox Direction Register 1 */
#define                        CAN0_TRS1  0xffc02a08   /* CAN Controller 0 Transmit Request Set Register 1 */
#define                        CAN0_TRR1  0xffc02a0c   /* CAN Controller 0 Transmit Request Reset Register 1 */
#define                         CAN0_TA1  0xffc02a10   /* CAN Controller 0 Transmit Acknowledge Register 1 */
#define                         CAN0_AA1  0xffc02a14   /* CAN Controller 0 Abort Acknowledge Register 1 */
#define                        CAN0_RMP1  0xffc02a18   /* CAN Controller 0 Receive Message Pending Register 1 */
#define                        CAN0_RML1  0xffc02a1c   /* CAN Controller 0 Receive Message Lost Register 1 */
#define                      CAN0_MBTIF1  0xffc02a20   /* CAN Controller 0 Mailbox Transmit Interrupt Flag Register 1 */
#define                      CAN0_MBRIF1  0xffc02a24   /* CAN Controller 0 Mailbox Receive Interrupt Flag Register 1 */
#define                       CAN0_MBIM1  0xffc02a28   /* CAN Controller 0 Mailbox Interrupt Mask Register 1 */
#define                        CAN0_RFH1  0xffc02a2c   /* CAN Controller 0 Remote Frame Handling Enable Register 1 */
#define                       CAN0_OPSS1  0xffc02a30   /* CAN Controller 0 Overwrite Protection Single Shot Transmit Register 1 */

/* CAN Controller 0 Config 2 Registers */

#define                         CAN0_MC2  0xffc02a40   /* CAN Controller 0 Mailbox Configuration Register 2 */
#define                         CAN0_MD2  0xffc02a44   /* CAN Controller 0 Mailbox Direction Register 2 */
#define                        CAN0_TRS2  0xffc02a48   /* CAN Controller 0 Transmit Request Set Register 2 */
#define                        CAN0_TRR2  0xffc02a4c   /* CAN Controller 0 Transmit Request Reset Register 2 */
#define                         CAN0_TA2  0xffc02a50   /* CAN Controller 0 Transmit Acknowledge Register 2 */
#define                         CAN0_AA2  0xffc02a54   /* CAN Controller 0 Abort Acknowledge Register 2 */
#define                        CAN0_RMP2  0xffc02a58   /* CAN Controller 0 Receive Message Pending Register 2 */
#define                        CAN0_RML2  0xffc02a5c   /* CAN Controller 0 Receive Message Lost Register 2 */
#define                      CAN0_MBTIF2  0xffc02a60   /* CAN Controller 0 Mailbox Transmit Interrupt Flag Register 2 */
#define                      CAN0_MBRIF2  0xffc02a64   /* CAN Controller 0 Mailbox Receive Interrupt Flag Register 2 */
#define                       CAN0_MBIM2  0xffc02a68   /* CAN Controller 0 Mailbox Interrupt Mask Register 2 */
#define                        CAN0_RFH2  0xffc02a6c   /* CAN Controller 0 Remote Frame Handling Enable Register 2 */
#define                       CAN0_OPSS2  0xffc02a70   /* CAN Controller 0 Overwrite Protection Single Shot Transmit Register 2 */

/* CAN Controller 0 Clock/Interrupt/Counter Registers */

#define                       CAN0_CLOCK  0xffc02a80   /* CAN Controller 0 Clock Register */
#define                      CAN0_TIMING  0xffc02a84   /* CAN Controller 0 Timing Register */
#define                       CAN0_DEBUG  0xffc02a88   /* CAN Controller 0 Debug Register */
#define                      CAN0_STATUS  0xffc02a8c   /* CAN Controller 0 Global Status Register */
#define                         CAN0_CEC  0xffc02a90   /* CAN Controller 0 Error Counter Register */
#define                         CAN0_GIS  0xffc02a94   /* CAN Controller 0 Global Interrupt Status Register */
#define                         CAN0_GIM  0xffc02a98   /* CAN Controller 0 Global Interrupt Mask Register */
#define                         CAN0_GIF  0xffc02a9c   /* CAN Controller 0 Global Interrupt Flag Register */
#define                     CAN0_CONTROL  0xffc02aa0   /* CAN Controller 0 Master Control Register */
#define                        CAN0_INTR  0xffc02aa4   /* CAN Controller 0 Interrupt Pending Register */
#define                        CAN0_MBTD  0xffc02aac   /* CAN Controller 0 Mailbox Temporary Disable Register */
#define                         CAN0_EWR  0xffc02ab0   /* CAN Controller 0 Programmable Warning Level Register */
#define                         CAN0_ESR  0xffc02ab4   /* CAN Controller 0 Error Status Register */
#define                       CAN0_UCCNT  0xffc02ac4   /* CAN Controller 0 Universal Counter Register */
#define                        CAN0_UCRC  0xffc02ac8   /* CAN Controller 0 Universal Counter Force Reload Register */
#define                       CAN0_UCCNF  0xffc02acc   /* CAN Controller 0 Universal Counter Configuration Register */

/* CAN Controller 0 Acceptance Registers */

#define                       CAN0_AM00L  0xffc02b00   /* CAN Controller 0 Mailbox 0 Acceptance Mask High Register */
#define                       CAN0_AM00H  0xffc02b04   /* CAN Controller 0 Mailbox 0 Acceptance Mask Low Register */
#define                       CAN0_AM01L  0xffc02b08   /* CAN Controller 0 Mailbox 1 Acceptance Mask High Register */
#define                       CAN0_AM01H  0xffc02b0c   /* CAN Controller 0 Mailbox 1 Acceptance Mask Low Register */
#define                       CAN0_AM02L  0xffc02b10   /* CAN Controller 0 Mailbox 2 Acceptance Mask High Register */
#define                       CAN0_AM02H  0xffc02b14   /* CAN Controller 0 Mailbox 2 Acceptance Mask Low Register */
#define                       CAN0_AM03L  0xffc02b18   /* CAN Controller 0 Mailbox 3 Acceptance Mask High Register */
#define                       CAN0_AM03H  0xffc02b1c   /* CAN Controller 0 Mailbox 3 Acceptance Mask Low Register */
#define                       CAN0_AM04L  0xffc02b20   /* CAN Controller 0 Mailbox 4 Acceptance Mask High Register */
#define                       CAN0_AM04H  0xffc02b24   /* CAN Controller 0 Mailbox 4 Acceptance Mask Low Register */
#define                       CAN0_AM05L  0xffc02b28   /* CAN Controller 0 Mailbox 5 Acceptance Mask High Register */
#define                       CAN0_AM05H  0xffc02b2c   /* CAN Controller 0 Mailbox 5 Acceptance Mask Low Register */
#define                       CAN0_AM06L  0xffc02b30   /* CAN Controller 0 Mailbox 6 Acceptance Mask High Register */
#define                       CAN0_AM06H  0xffc02b34   /* CAN Controller 0 Mailbox 6 Acceptance Mask Low Register */
#define                       CAN0_AM07L  0xffc02b38   /* CAN Controller 0 Mailbox 7 Acceptance Mask High Register */
#define                       CAN0_AM07H  0xffc02b3c   /* CAN Controller 0 Mailbox 7 Acceptance Mask Low Register */
#define                       CAN0_AM08L  0xffc02b40   /* CAN Controller 0 Mailbox 8 Acceptance Mask High Register */
#define                       CAN0_AM08H  0xffc02b44   /* CAN Controller 0 Mailbox 8 Acceptance Mask Low Register */
#define                       CAN0_AM09L  0xffc02b48   /* CAN Controller 0 Mailbox 9 Acceptance Mask High Register */
#define                       CAN0_AM09H  0xffc02b4c   /* CAN Controller 0 Mailbox 9 Acceptance Mask Low Register */
#define                       CAN0_AM10L  0xffc02b50   /* CAN Controller 0 Mailbox 10 Acceptance Mask High Register */
#define                       CAN0_AM10H  0xffc02b54   /* CAN Controller 0 Mailbox 10 Acceptance Mask Low Register */
#define                       CAN0_AM11L  0xffc02b58   /* CAN Controller 0 Mailbox 11 Acceptance Mask High Register */
#define                       CAN0_AM11H  0xffc02b5c   /* CAN Controller 0 Mailbox 11 Acceptance Mask Low Register */
#define                       CAN0_AM12L  0xffc02b60   /* CAN Controller 0 Mailbox 12 Acceptance Mask High Register */
#define                       CAN0_AM12H  0xffc02b64   /* CAN Controller 0 Mailbox 12 Acceptance Mask Low Register */
#define                       CAN0_AM13L  0xffc02b68   /* CAN Controller 0 Mailbox 13 Acceptance Mask High Register */
#define                       CAN0_AM13H  0xffc02b6c   /* CAN Controller 0 Mailbox 13 Acceptance Mask Low Register */
#define                       CAN0_AM14L  0xffc02b70   /* CAN Controller 0 Mailbox 14 Acceptance Mask High Register */
#define                       CAN0_AM14H  0xffc02b74   /* CAN Controller 0 Mailbox 14 Acceptance Mask Low Register */
#define                       CAN0_AM15L  0xffc02b78   /* CAN Controller 0 Mailbox 15 Acceptance Mask High Register */
#define                       CAN0_AM15H  0xffc02b7c   /* CAN Controller 0 Mailbox 15 Acceptance Mask Low Register */

/* CAN Controller 0 Acceptance Registers */

#define                       CAN0_AM16L  0xffc02b80   /* CAN Controller 0 Mailbox 16 Acceptance Mask High Register */
#define                       CAN0_AM16H  0xffc02b84   /* CAN Controller 0 Mailbox 16 Acceptance Mask Low Register */
#define                       CAN0_AM17L  0xffc02b88   /* CAN Controller 0 Mailbox 17 Acceptance Mask High Register */
#define                       CAN0_AM17H  0xffc02b8c   /* CAN Controller 0 Mailbox 17 Acceptance Mask Low Register */
#define                       CAN0_AM18L  0xffc02b90   /* CAN Controller 0 Mailbox 18 Acceptance Mask High Register */
#define                       CAN0_AM18H  0xffc02b94   /* CAN Controller 0 Mailbox 18 Acceptance Mask Low Register */
#define                       CAN0_AM19L  0xffc02b98   /* CAN Controller 0 Mailbox 19 Acceptance Mask High Register */
#define                       CAN0_AM19H  0xffc02b9c   /* CAN Controller 0 Mailbox 19 Acceptance Mask Low Register */
#define                       CAN0_AM20L  0xffc02ba0   /* CAN Controller 0 Mailbox 20 Acceptance Mask High Register */
#define                       CAN0_AM20H  0xffc02ba4   /* CAN Controller 0 Mailbox 20 Acceptance Mask Low Register */
#define                       CAN0_AM21L  0xffc02ba8   /* CAN Controller 0 Mailbox 21 Acceptance Mask High Register */
#define                       CAN0_AM21H  0xffc02bac   /* CAN Controller 0 Mailbox 21 Acceptance Mask Low Register */
#define                       CAN0_AM22L  0xffc02bb0   /* CAN Controller 0 Mailbox 22 Acceptance Mask High Register */
#define                       CAN0_AM22H  0xffc02bb4   /* CAN Controller 0 Mailbox 22 Acceptance Mask Low Register */
#define                       CAN0_AM23L  0xffc02bb8   /* CAN Controller 0 Mailbox 23 Acceptance Mask High Register */
#define                       CAN0_AM23H  0xffc02bbc   /* CAN Controller 0 Mailbox 23 Acceptance Mask Low Register */
#define                       CAN0_AM24L  0xffc02bc0   /* CAN Controller 0 Mailbox 24 Acceptance Mask High Register */
#define                       CAN0_AM24H  0xffc02bc4   /* CAN Controller 0 Mailbox 24 Acceptance Mask Low Register */
#define                       CAN0_AM25L  0xffc02bc8   /* CAN Controller 0 Mailbox 25 Acceptance Mask High Register */
#define                       CAN0_AM25H  0xffc02bcc   /* CAN Controller 0 Mailbox 25 Acceptance Mask Low Register */
#define                       CAN0_AM26L  0xffc02bd0   /* CAN Controller 0 Mailbox 26 Acceptance Mask High Register */
#define                       CAN0_AM26H  0xffc02bd4   /* CAN Controller 0 Mailbox 26 Acceptance Mask Low Register */
#define                       CAN0_AM27L  0xffc02bd8   /* CAN Controller 0 Mailbox 27 Acceptance Mask High Register */
#define                       CAN0_AM27H  0xffc02bdc   /* CAN Controller 0 Mailbox 27 Acceptance Mask Low Register */
#define                       CAN0_AM28L  0xffc02be0   /* CAN Controller 0 Mailbox 28 Acceptance Mask High Register */
#define                       CAN0_AM28H  0xffc02be4   /* CAN Controller 0 Mailbox 28 Acceptance Mask Low Register */
#define                       CAN0_AM29L  0xffc02be8   /* CAN Controller 0 Mailbox 29 Acceptance Mask High Register */
#define                       CAN0_AM29H  0xffc02bec   /* CAN Controller 0 Mailbox 29 Acceptance Mask Low Register */
#define                       CAN0_AM30L  0xffc02bf0   /* CAN Controller 0 Mailbox 30 Acceptance Mask High Register */
#define                       CAN0_AM30H  0xffc02bf4   /* CAN Controller 0 Mailbox 30 Acceptance Mask Low Register */
#define                       CAN0_AM31L  0xffc02bf8   /* CAN Controller 0 Mailbox 31 Acceptance Mask High Register */
#define                       CAN0_AM31H  0xffc02bfc   /* CAN Controller 0 Mailbox 31 Acceptance Mask Low Register */

/* CAN Controller 0 Mailbox Data Registers */

#define                  CAN0_MB00_DATA0  0xffc02c00   /* CAN Controller 0 Mailbox 0 Data 0 Register */
#define                  CAN0_MB00_DATA1  0xffc02c04   /* CAN Controller 0 Mailbox 0 Data 1 Register */
#define                  CAN0_MB00_DATA2  0xffc02c08   /* CAN Controller 0 Mailbox 0 Data 2 Register */
#define                  CAN0_MB00_DATA3  0xffc02c0c   /* CAN Controller 0 Mailbox 0 Data 3 Register */
#define                 CAN0_MB00_LENGTH  0xffc02c10   /* CAN Controller 0 Mailbox 0 Length Register */
#define              CAN0_MB00_TIMESTAMP  0xffc02c14   /* CAN Controller 0 Mailbox 0 Timestamp Register */
#define                    CAN0_MB00_ID0  0xffc02c18   /* CAN Controller 0 Mailbox 0 ID0 Register */
#define                    CAN0_MB00_ID1  0xffc02c1c   /* CAN Controller 0 Mailbox 0 ID1 Register */
#define                  CAN0_MB01_DATA0  0xffc02c20   /* CAN Controller 0 Mailbox 1 Data 0 Register */
#define                  CAN0_MB01_DATA1  0xffc02c24   /* CAN Controller 0 Mailbox 1 Data 1 Register */
#define                  CAN0_MB01_DATA2  0xffc02c28   /* CAN Controller 0 Mailbox 1 Data 2 Register */
#define                  CAN0_MB01_DATA3  0xffc02c2c   /* CAN Controller 0 Mailbox 1 Data 3 Register */
#define                 CAN0_MB01_LENGTH  0xffc02c30   /* CAN Controller 0 Mailbox 1 Length Register */
#define              CAN0_MB01_TIMESTAMP  0xffc02c34   /* CAN Controller 0 Mailbox 1 Timestamp Register */
#define                    CAN0_MB01_ID0  0xffc02c38   /* CAN Controller 0 Mailbox 1 ID0 Register */
#define                    CAN0_MB01_ID1  0xffc02c3c   /* CAN Controller 0 Mailbox 1 ID1 Register */
#define                  CAN0_MB02_DATA0  0xffc02c40   /* CAN Controller 0 Mailbox 2 Data 0 Register */
#define                  CAN0_MB02_DATA1  0xffc02c44   /* CAN Controller 0 Mailbox 2 Data 1 Register */
#define                  CAN0_MB02_DATA2  0xffc02c48   /* CAN Controller 0 Mailbox 2 Data 2 Register */
#define                  CAN0_MB02_DATA3  0xffc02c4c   /* CAN Controller 0 Mailbox 2 Data 3 Register */
#define                 CAN0_MB02_LENGTH  0xffc02c50   /* CAN Controller 0 Mailbox 2 Length Register */
#define              CAN0_MB02_TIMESTAMP  0xffc02c54   /* CAN Controller 0 Mailbox 2 Timestamp Register */
#define                    CAN0_MB02_ID0  0xffc02c58   /* CAN Controller 0 Mailbox 2 ID0 Register */
#define                    CAN0_MB02_ID1  0xffc02c5c   /* CAN Controller 0 Mailbox 2 ID1 Register */
#define                  CAN0_MB03_DATA0  0xffc02c60   /* CAN Controller 0 Mailbox 3 Data 0 Register */
#define                  CAN0_MB03_DATA1  0xffc02c64   /* CAN Controller 0 Mailbox 3 Data 1 Register */
#define                  CAN0_MB03_DATA2  0xffc02c68   /* CAN Controller 0 Mailbox 3 Data 2 Register */
#define                  CAN0_MB03_DATA3  0xffc02c6c   /* CAN Controller 0 Mailbox 3 Data 3 Register */
#define                 CAN0_MB03_LENGTH  0xffc02c70   /* CAN Controller 0 Mailbox 3 Length Register */
#define              CAN0_MB03_TIMESTAMP  0xffc02c74   /* CAN Controller 0 Mailbox 3 Timestamp Register */
#define                    CAN0_MB03_ID0  0xffc02c78   /* CAN Controller 0 Mailbox 3 ID0 Register */
#define                    CAN0_MB03_ID1  0xffc02c7c   /* CAN Controller 0 Mailbox 3 ID1 Register */
#define                  CAN0_MB04_DATA0  0xffc02c80   /* CAN Controller 0 Mailbox 4 Data 0 Register */
#define                  CAN0_MB04_DATA1  0xffc02c84   /* CAN Controller 0 Mailbox 4 Data 1 Register */
#define                  CAN0_MB04_DATA2  0xffc02c88   /* CAN Controller 0 Mailbox 4 Data 2 Register */
#define                  CAN0_MB04_DATA3  0xffc02c8c   /* CAN Controller 0 Mailbox 4 Data 3 Register */
#define                 CAN0_MB04_LENGTH  0xffc02c90   /* CAN Controller 0 Mailbox 4 Length Register */
#define              CAN0_MB04_TIMESTAMP  0xffc02c94   /* CAN Controller 0 Mailbox 4 Timestamp Register */
#define                    CAN0_MB04_ID0  0xffc02c98   /* CAN Controller 0 Mailbox 4 ID0 Register */
#define                    CAN0_MB04_ID1  0xffc02c9c   /* CAN Controller 0 Mailbox 4 ID1 Register */
#define                  CAN0_MB05_DATA0  0xffc02ca0   /* CAN Controller 0 Mailbox 5 Data 0 Register */
#define                  CAN0_MB05_DATA1  0xffc02ca4   /* CAN Controller 0 Mailbox 5 Data 1 Register */
#define                  CAN0_MB05_DATA2  0xffc02ca8   /* CAN Controller 0 Mailbox 5 Data 2 Register */
#define                  CAN0_MB05_DATA3  0xffc02cac   /* CAN Controller 0 Mailbox 5 Data 3 Register */
#define                 CAN0_MB05_LENGTH  0xffc02cb0   /* CAN Controller 0 Mailbox 5 Length Register */
#define              CAN0_MB05_TIMESTAMP  0xffc02cb4   /* CAN Controller 0 Mailbox 5 Timestamp Register */
#define                    CAN0_MB05_ID0  0xffc02cb8   /* CAN Controller 0 Mailbox 5 ID0 Register */
#define                    CAN0_MB05_ID1  0xffc02cbc   /* CAN Controller 0 Mailbox 5 ID1 Register */
#define                  CAN0_MB06_DATA0  0xffc02cc0   /* CAN Controller 0 Mailbox 6 Data 0 Register */
#define                  CAN0_MB06_DATA1  0xffc02cc4   /* CAN Controller 0 Mailbox 6 Data 1 Register */
#define                  CAN0_MB06_DATA2  0xffc02cc8   /* CAN Controller 0 Mailbox 6 Data 2 Register */
#define                  CAN0_MB06_DATA3  0xffc02ccc   /* CAN Controller 0 Mailbox 6 Data 3 Register */
#define                 CAN0_MB06_LENGTH  0xffc02cd0   /* CAN Controller 0 Mailbox 6 Length Register */
#define              CAN0_MB06_TIMESTAMP  0xffc02cd4   /* CAN Controller 0 Mailbox 6 Timestamp Register */
#define                    CAN0_MB06_ID0  0xffc02cd8   /* CAN Controller 0 Mailbox 6 ID0 Register */
#define                    CAN0_MB06_ID1  0xffc02cdc   /* CAN Controller 0 Mailbox 6 ID1 Register */
#define                  CAN0_MB07_DATA0  0xffc02ce0   /* CAN Controller 0 Mailbox 7 Data 0 Register */
#define                  CAN0_MB07_DATA1  0xffc02ce4   /* CAN Controller 0 Mailbox 7 Data 1 Register */
#define                  CAN0_MB07_DATA2  0xffc02ce8   /* CAN Controller 0 Mailbox 7 Data 2 Register */
#define                  CAN0_MB07_DATA3  0xffc02cec   /* CAN Controller 0 Mailbox 7 Data 3 Register */
#define                 CAN0_MB07_LENGTH  0xffc02cf0   /* CAN Controller 0 Mailbox 7 Length Register */
#define              CAN0_MB07_TIMESTAMP  0xffc02cf4   /* CAN Controller 0 Mailbox 7 Timestamp Register */
#define                    CAN0_MB07_ID0  0xffc02cf8   /* CAN Controller 0 Mailbox 7 ID0 Register */
#define                    CAN0_MB07_ID1  0xffc02cfc   /* CAN Controller 0 Mailbox 7 ID1 Register */
#define       /*
 * CopyrCAN0_MB08_DATA0  0xffc02d00   /*ight Controller 0 Mailbox 8 Data 0 Register */
#define/*
 * CopyriCopyright 2007-2008 1nalog Devic4s Inc.
 *
 * Licensed under the ADI BS1 license or the GPL-2 (or later)
 */

#ifndef _DEF_B2nalog Devic8s Inc.
 *
 * Licensed under the ADI BS2 license or the GPL-2 (or later)
 */

#ifndef _DEF_B3nalog Deviccs Inc.
 *
 * Licensed under the ADI BS3 license or the GPL-2 (or later)
 */
ght 2007-2LENGTHnalog Devi1es Inc.
 *
 * Licensed under the ALength license or the GPL-2 (or later)
 ght 2007-2TIMESTAMP /* PLL Conne _DEF_BF54X_H


/* *************Timestamp license or the GPL-2 (or later)
 */

/* PLL DivisIDAnalog Devi1 ADDRESS DEFINITIONS COMMON TO ALLefin008   /* Voltage Regulator Control Register */
#defF54X_H
#def1************* */

/* PLL Registers    license or the GPL-2 (or later)
 */

#ifndef 92008 Analog Devi2es Inc.
 *
 * Licensed under the9ADI BSD license or the GPL-2 (or later)
 */

#ifndef 0xFFC0F54X_H
#def2ne _DEF_BF54X_H


/* ***********        Register */

/* Debug/MP/Emulation Registers (0xFFC0SYSTEM & MM2 ADDRESS DEFINITIONS COMMON TO A       -BF54x    */
/* *******************************0xFFC0***********2************* */

/* PLL Registe       #define                          PLL_CTL  0xff900000   /* PLL Co300014) */

#define                                PLL_DIV  0xffc00004   /* PLL Div9sor Register */
#def3     CHIPID_VERSION  0xF0000000
#dL  0xffc00008   /* Voltage Regulator Control Register */
9define        3   CHIPID_MANUFACTURE  0x00000FFE
0c   /* PLL Status Register */
#define             ne  F54X_H
#def3

#define                         nt Register */

/* Debug/MP/Emulation Registers 102008 Analog Devi4es Inc.
 *
 * Licensed under the10ADI BSD license or the GPL-2 (or later)
 */

#ifndef       F54X_H
#def4ne _DEF_BF54X_H


/* ***********terrupt  Interrupt Mask Register 2 */
#define                SYSTEM & MM4 ADDRESS DEFINITIONS COMMON TO Aterrupt -BF54x    */
/* ******************************       ***********4************* */

/* PLL Registeterrupt #define                          PLL_CTL  0xf1000000   /* PLL Co5_ISR0  0xffc00118   /* System Inter                PLL_DIV  0xffc00004   /* PLL Di10sor Register */
#def5ffc0011c   /* System Interrupt StatL  0xffc00008   /* Voltage Regulator Control Register */10define        5  /* System Interrupt Status Regist0c   /* PLL Status Register */
#define            AssigF54X_H
#def5em Interrupt Wakeup Register 0 */
#em Interrupt Mask Register 2 */
#define          12008 Analog Devi6_ISR0  0xffc00118   /* System Int1rrupt Status Register 0 */
#define                   ignmenF54X_H
#def6ffc0011c   /* System Interrupt St   SIC_ SIC_IAR2  0xffc00138   /* System Interrupt AssignmenSYSTEM & MM6  /* System Interrupt Status Regi   SIC_*/
#define                         SIC_IWR0  0xignmen***********6em Interrupt Wakeup Register 0 */   SIC_e                         SIC_IWR1  0xffc00128100000   /* PLL Co7 */
#define                                              SIC_IWR2  0xffc0012c   /* 1sor Register */
#def7define                         SIC_             SIC_IAR0  0xffc00130   /* System Interrupt A1define        7e                         SIC_IAR5      SIC_IAR1  0xffc00134   /* System Interrupt Ass    F54X_H
#def7                     SIC_IAR6  0xffem Interrupt Mask Register 2 */
#define          22008 Analog Devi8_ISR0  0xffc00118   /* System Int2rrupt Status Register 0 */
#define                         F54X_H
#def8ffc0011c   /* System Interrupt StInterruerrupt Assignment Register 10 */
#define             SYSTEM & MM8  /* System Interrupt Status RegiInterru*/
#define                         SIC_IWR0  0x      ***********8em Interrupt Wakeup Register 0 */Interrue                         SIC_IWR1  0xffc00128200000   /* PLL Co9IC_IAR11  0xffc0015c   /* System In                      SIC_IWR2  0xffc0012c   /* 2sor Register */
#def9fine                         WDOG_C             SIC_IAR0  0xffc00130   /* System Interrupt A2define        9           WDOG_CNT  0xffc00204   /     SIC_IAR1  0xffc00134   /* System Interrupt AssInteF54X_H
#def9 0xffc00208   /* Watchdog Status Reem Interrupt Mask Register 2 */
#define          32008 Analog Devia_ISR0  0xffc00118   /* System Int3rrupt Status Register 0 */
#define                   fine  F54X_H
#defaffc0011c   /* System Interrupt St /* RTCLARM  0xffc00310   /* RTC Alarm Register */
#define  SYSTEM & MMa  /* System Interrupt Status Regi /* RTC*/
#define                         SIC_IWR0  0xfine  ***********aem Interrupt Wakeup Register 0 */ /* RTCe                         SIC_IWR1  0xffc00128300000   /* PLL Cob           RTC_PREN  0xffc00314   /                      SIC_IWR2  0xffc0012c   /* 3sor Register */
#defb                    UART0_DLL  0xff             SIC_IAR0  0xffc00130   /* System Interrupt A3define        bART0_DLH  0xffc00404   /* Divisor L     SIC_IAR1  0xffc00134   /* System Interrupt Ass    F54X_H
#defb   /* Global Control Register */
#dem Interrupt Mask Register 2 */
#define          42008 Analog Devic_ISR0  0xffc00118   /* System Int4rrupt Status Register 0 */
#define                   erruptF54X_H
#defcffc0011c   /* System Interrupt St    UAR             UART0_IER_SET  0xffc00420   /* InterruptSYSTEM & MMc  /* System Interrupt Status Regi    UAR*/
#define                         SIC_IWR0  0xerrupt***********cem Interrupt Wakeup Register 0 */    UARe                         SIC_IWR1  0xffc00128400000   /* PLL Codster Set */
#define                                      SIC_IWR2  0xffc0012c   /* 4sor Register */
#defd#define                        UART             SIC_IAR0  0xffc00130   /* System Interrupt A4define        d          UART0_RBR  0xffc0042c   /     SIC_IAR1  0xffc00134   /* System Interrupt Ass    F54X_H
#defd          SPI0_REGBASE  0xffc00500
em Interrupt Mask Register 2 */
#define          52008 Analog Devie_ISR0  0xffc00118   /* System Int5rrupt Status Register 0 */
#define                   I0_BAUF54X_H
#defeffc0011c   /* System Interrupt St
#definr Register */
#define                        SPI0_BAUSYSTEM & MMe  /* System Interrupt Status Regi
#defin*/
#define                         SIC_IWR0  0xI0_BAU***********eem Interrupt Wakeup Register 0 */
#define                         SIC_IWR1  0xffc00128500000   /* PLL Cof4   /* SPI0 Baud Rate Register */
#                      SIC_IWR2  0xffc0012c   /* 5sor Register */
#deffData Buffer Shadow Register */

/*              SIC_IAR0  0xffc00130   /* System Interrupt A5define        fthey are not available on the ADSP-     SIC_IAR1  0xffc00134   /* System Interrupt Ass*/
#F54X_H
#deff                     TWI0_REGBASE  nt Register */


nc.
 *
 * Licensed under theDI BSlicense sde Ad                     SIC_IWR0  0x62008 Analog Deveces Inc.
 *
 * Licensed under the16rrupt Status Register 0 */
#define                   MasterF54X_H
#deeine _DEF_BF54X_H


/* ***********      TW Interrupt Mask Register 2 */
#define          MasterSYSTEM & MeR ADDRESS DEFINITIONS COMMON TO A      TW*/
#define                         SIC_IWR0  0xMaster**********e************** */

/* PLL Registe      TWe                         SIC_IWR1  0xffc00128600000   /* PLL Centrol Register */
#define        16                   TWI0_CONTROL  0xffc00704   /* 6sor Register */
#deeine                           VR_16AVE_CTRL  0xffc00708   /* TWI Slave Mode Control Register 6define       e                 PLL_STAT  0xffc0160070c   /* TWI Slave Mode Status Register */
#defineIFO s Register CKCNT  0xffc00010   /* PLL Lock Ce     Interrupt Mask Register 2 */
#define          7aster Mode ContrC00014) */

#define              17rrupt Status Register 0 */
#define                   /* TWIs Register       CHIPID_VERSION  0xF0000000
define                    TWI0_RCV_DATA8  0xffc00788   /* TWI*/
#define     CHIPID_MANUFACTURE  0x00000FFdefine  */
#define                         SIC_IWR0  0x/* TWI           /

#define                       define  e                         SIC_IWR1  0xffc001287     TWI0_FIFO_CT                   SYSCR  0xffc0017                   TWI0_CONTROL  0xffc00704   /* 7STAT  0xffc0072c   /                       SIC_IMASK017AVE_CTRL  0xffc00708   /* TWI Slave Mode Control Register 7FO Transmit Da          SIC_IMASK1  0xffc00110 170070c   /* TWI Slave Mode Status Register */
#define SPOs Register    SIC_IMASK2  0xffc00114   /* SyFrame Interrupt Mask Register 2 */
#define          -2008 Analog DeveC_ISR0  0xffc00118   /* System Int ADI BSD license or the GPL-2 (or later)
 */

#ifndefSPORT1_s Register xffc0011c   /* System Interrupt St*************************************************** *SPORT1_*/
#define    /* System Interrupt Status RegiLL ADSP-BF54x    */
/* ******************************SPORT1_           tem Interrupt Wakeup Register 0 */rs */

#define                          PLL_CTL  0xf1c00000   /* PLL Cerupt Wakeup Register 1 */
#define                   PLL_DIV  0xffc00004   /* PLL Di1isor Register */
#deeup Register 2 */
#define          CTL  0xffc00008   /* Voltage Regulator Control Register */1#define       eter 0 */
#define                  000c   /* PLL Status Register */
#define             Regis Register r 1 */
#define                    ount Register */

/* Debug/MP/Emulation Registers 10xFFC00014 - 0xFe2 */
#define                                   CHIPID  0xffc00014
/* CHIPID Masks */
#dfigurats Register #define                         SI#define                    CHIPID_FAMILY  0x0FFFF000
figurat*/
#define ne                         SIC_IARE

/* System Reset and Interrupt Controller (0xFFC001figurat                                 SIC_IAR6  0x     SWRST  0xffc00100   /* Software Reset Register 1/
#define        e                 SIC_IAR7  0xffc00104   /* System Configuration register */

/* SIC1Registers */

#define            SIC_IAR8  0xffc00150    0xffc0010c   /* System Interrupt Mask Register 0 */
#def1ne            e    SIC_IAR9  0xffc00154   /* Syst  /* System Interrupt Mask Register 1 */
#define    ulti s Register  SIC_IAR10  0xffc00158   /* Systemstem Interrupt Mask Register 2 */
#define         2               SeSIC_IAR11  0xffc0015c   /* System2errupt Status Register 0 */
#define                     /* Ss Register efine                         WDO3 */

/*e                     SPORT1_MRCS3  0xffc0095c   /* S*/
#define             WDOG_CNT  0xffc00204 3 */

/*-BF54x    */
/* ******************************   /* S             0xffc00208   /* Watchdog Status3 */

/*#define                          PLL_CTL  0xf2   /* System InteeTAT  0xffc00300   /* RTC Status R2                        SIC_IWR2  0xffc0012c   /*2System Interrupt Wake Interrupt Control Register */
#d2               SIC_IAR0  0xffc00130   /* System Interrupt 2ssignment Regies Register */
#define            2       SIC_IAR1  0xffc00134   /* System Interrupt As     s Register /
#define                        * Asye                     SPORT1_MRCS3  0xffc0095c ignment Registere            RTC_PREN  0xffc00314 2   SIC_IAR3  0xffc0013c   /* System Interrupt Assignmemory Cs Register                      UART0_DLL  0     EBIous Memory Flash Control Register */

/* DDR Memory C*/
#define UART0_DLH  0xffc00404   /* Diviso     EBIchronous Memory Bank Control Register */
#definr 5 */
#define  e8   /* Global Control Register */     EBIronous Memory Bank Control Register */
#define*/
#define       eRegister */
#define              214c   /* System Interrupt Assignment Register 7 *2
#define            e                      UART0_LSR  2 /* System Interrupt Assignment Register 8 */
#define     2              e0_MSR  0xffc00418   /* Modem Stat2em Interrupt Assignment Register 9 */
#define        Erros Register  /* Scratch Register */
#define       e                     SPORT1_MRCS3  0xffc0095c                 eister Set */
#define             2Interrupt Assignment Register 11 */

/* Watchdog Time 0xffc0s Register 
#define                        Udefine   */

#define                     EBIU_DDRBRC0  0xffc0*/
#define            UART0_RBR  0xffc0042c define  chronous Memory Bank Control Register */
#defin        WDOG_STAe           SPI0_REGBASE  0xffc005define  ronous Memory Bank Control Register */
#define             RTC_el Register */
#define            2gister */
#define                         RTC_ICT2  0xffc00304   /* RTe                    SPI0_STAT  0x2fine                        RTC_ISTAT  0xffc00308   /* RTC2Interrupt StateDBR  0xffc0050c   /* SPI0 Transmi2           RTC_SWCNT  0xffc0030c   /* RTC Stopwatch 7  0xs Register xffc00510   /* SPI0 Receive Data */
#de                     SPORT1_MRCS3  0xffc0095c fine            e14   /* SPI0 Baud Rate Register *2 /* RTC Prescaler Enable Register */

/* UART0 RegistDDR Bans Register  Data Buffer Shadow Register */

                        EBIU_DDRBWC1  0xffc00a84   /* DDR Ban*/
#define  they are not available on the AD        chronous Memory Bank Control Register */
#defin0_GCTL  0xffc004ee                     TWI0_REGBAS        ronous Memory Bank Control Register */
#define  /* Line ControleClock Divider Register */
#define2         UART0_MCR  0xffc00410   /* Modem Control2Register */
#define e*/
#define                  TWI0_2xffc00414   /* Line Status Register */
#define            2           UARe             TWI0_SLAVE_STAT  0xf2s Register */
#define                        UART0_Saa0  s Register    TWI0_SLAVE_ADDR  0xffc00710        e                     SPORT1_MRCS3  0xffc0095c errupt Enable Refces Inc.
 *
 * Licensed under the2    UART0_IER_CLEAR  0xffc00424   /* Interrupt Enableefresh F54X_H
#define _DEF_BF54X_H


/* ***********   EBIU_      EBIU_DDRARCT  0xffc00aac   /* DDR Auto-refresh SYSTEM & MfR ADDRESS DEFINITIONS COMMON TO A   EBIU_chronous Memory Bank Control Register */
#defin#define         f************** */

/* PLL Registe   EBIU_ronous Memory Bank Control Register */
#define0   /* SPI0 Contrfntrol Register */
#define        2            SPI0_FLG  0xffc00504   /* SPI0 Flag R2gister */
#define   fine                           VR_2fc00508   /* SPI0 Status Register */
#define              2         SPI0_f                 PLL_STAT  0xffc02 Data Buffer Register */
#define                     0xff
#define   CKCNT  0xffc00010   /* PLL Lock C Perie                     SPORT1_MRCS3  0xffc0095c I0_BAUD  0xffc00fC00014) */

#define              2
#define                      SPI0_SHADOW  0xffc00518

#defi
#define         CHIPID_VERSION  0xF0000000
c00   /*nts Register */

/* DMA Channel 0 Registers */

#defi               CHIPID_MANUFACTURE  0x00000FFc00   /*chronous Memory Bank Control Register */
#defin(TWI0) */

#defif/

#define                       c00   /*ronous Memory Bank Control Register */
#define  0xffc00700   /*f                   SYSCR  0xffc002                    TWI0_CONTROL  0xffc00704   /*2TWI Control Registerf                       SIC_IMASK02LAVE_CTRL  0xffc00708   /* TWI Slave Mode Control Register2*/
#define    f          SIC_IMASK1  0xffc00110 2c0070c   /* TWI Slave Mode Status Register */
#defin   /*
#define      SIC_IMASK2  0xffc00114   /* Sy     e                     SPORT1_MRCS3  0xffc0095c Master Mode ContfC_ISR0  0xffc00118   /* System In2     TWI0_MASTER_STAT  0xffc00718   /* TWI Master Mod24   /*
#define   xffc0011c   /* System Interrupt Sdefine  ine                   DMA0_CURR_ADDR  0xffc00c24   /*              /* System Interrupt Status Regdefine  chronous Memory Bank Control Register */
#definne              ftem Interrupt Wakeup Register 0 *define  ronous Memory Bank Control Register */
#define      TWI0_FIFO_Cfrupt Wakeup Register 1 */
#define2ol Register */
#define                   TWI0_FIF2_STAT  0xffc0072c   fup Register 2 */
#define         2ine                   TWI0_XMT_DATA8  0xffc00780   /* TWI 2IFO Transmit Dfter 0 */
#define                 2                  TWI0_XMT_DATA16  0xffc00784   /* Tfc00c
#define   r 1 */
#define                    */
#e                     SPORT1_MRCS3  0xffc0095c /* TWI FIFO Recef2 */
#define                     2efine                  TWI0_RCV_DATA16  0xffc0078c   0   /* 
#define   #define                         S        ne                     DMA1_X_COUNT  0xffc00c50   /*            ne                         SIC_IA        chronous Memory Bank Control Register */
#defin            SPORf                      SIC_IAR6  0        ronous Memory Bank Control Register */
#define          SPORT1_f                 SIC_IAR7  0xffc02it Configuration 2 Register */
#define           2       SPORT1_TCLKDIf            SIC_IAR8  0xffc00150 2Serial Clock Divider Register */
#define                  2 SPORT1_TFSDIVf    SIC_IAR9  0xffc00154   /* Sys2rame Sync Divider Register */
#define               ine  
#define    SIC_IAR10  0xffc00158   /* Syste  /* e                     SPORT1_MRCS3  0xffc0095c -2008 Analog Devf channel Receive Select Register 3 ADI BSD license or the GPL-2 (or later)
 */

#ifndef       
#define           EBIU_AMGCTL  0xffc00a00   *************************************************** *                        EBIU_AMBCTL0   0xffc00a04   LL ADSP-BF54x    */
/* ******************************       fine           EBIU_AMBCTL1   0xffc00a08   /*rs */

#define                          PLL_CTL  0xf2c00000   /* PLL Cf    EBIU_MBSCTL  0xffc00a0c   /* A                  PLL_DIV  0xffc00004   /* PLL Di2isor Register */
#def        EBIU_ARBSTAT  0xffc00a10  CTL  0xffc00008   /* Voltage Regulator Control Register */2#define       f        EBIU_MODE  0xffc00a14   /*000c   /* PLL Status Register */
#define                 
#define     EBIU_FCTL  0xffc00a18   /* Asyncount Register */

/* Debug/MP/Emulation Registers 20xFFC00014 - 0xFfsters */

#define                              CHIPID  0xffc00014
/* CHIPID Masks */
#dTR  0xf
#define   ine                     EBIU_DDRCT#define                    CHIPID_FAMILY  0x0FFFF000
TR  0xf                     EBIU_DDRCTL2  0xffc00a28E

/* System Reset and Interrupt Controller (0xFFC001TR  0xffine       _DDRCTL3  0xffc00a2c   /* DDR Memo     SWRST  0xffc00100   /* Software Reset Register 2/
#define        fc00a30   /* DDR Queue Configuratio104   /* System Configuration register */

/* SIC2Registers */

#definf DDR Error Address Register */
#de  0xffc0010c   /* System Interrupt Mask Register 0 */
#def2ne            fgister */
#define                   /* System Interrupt Mask Register 1 */
#define    T_DES
#define   DDR BankRead and Write Count Regisstem Interrupt Mask Register 2 */
#define         3               Sfister Set */
#define             3errupt Status Register 0 */
#define                     DMA3
#define   
#define                        UgurationAddress Register */
#define                      DMA3                      UART0_RBR  0xffc0042c guration-BF54x    */
/* ******************************   DMA3fine                  SPI0_REGBASE  0xffc005guration#define                          PLL_CTL  0xf3   /* System Intefl Register */
#define            3                        SIC_IWR2  0xffc0012c   /*3System Interrupt Wakf                    SPI0_STAT  0x3               SIC_IAR0  0xffc00130   /* System Interrupt 3ssignment RegifDBR  0xffc0050c   /* SPI0 Transmi3       SIC_IAR1  0xffc00134   /* System Interrupt Asdress
#define   xffc00510   /* SPI0 Receive Data RQ_STAddress Register */
#define                    ignment Registerf14   /* SPI0 Baud Rate Register *3   SIC_IAR3  0xffc0013c   /* System Interrupt AssignmMap Reg
#define    Data Buffer Shadow Register */

COUNT  0MAP  0xffc00cec   /* DMA Channel 3 Peripheral Map Reg            they are not available on the ADCOUNT  0                DMA3_Y_COUNT  0xffc00cd8   /* Dr 5 */
#define  fe                     TWI0_REGBASCOUNT  0  DMA3_Y_MODIFY  0xffc00cdc   /* DMA Channel 3*/
#define       fClock Divider Register */
#define314c   /* System Interrupt Assignment Register 7 *3
#define            f*/
#define                  TWI0_3 /* System Interrupt Assignment Register 8 */
#define     3              f             TWI0_SLAVE_STAT  0xf3em Interrupt Assignment Register 9 */
#define       ister
#define      TWI0_SLAVE_ADDR  0xffc00710     0xfI Slave Mode AddreUART#define    TWI0_MASTER_CTRL  0xffc00714     DMAY Cou_DLLnalog De31ces Inc.Divisor Latch Low Byte Voltage Regulator Control RegiDMA4_Y_MODIF  /* PLL 31ine _DEFDMA Channel 4 Highodify Register */
#define              _Y_MODGCTY  0xffc00d1 ADDRESGlobal
 * Lice  /* PLL Status Register */
#define     ister */
#LCR_DESC_PTR  *******LPL-2_CURR_ADDR  0xffc00d24   /* DMA Channel 4 Current AddressMRegister */
ntrol ReModem4_CURR_ADDR  0xffc00d24   /* DMA Channel 4 Current Address SInterrupt/Stne _DEF     Status     DMA4_IRQ_STATUS  0xffc00d28   /* DMA Channel 4 /* DMA Chann ADDRESgisterral Map Register */
#define                DMA4_CURR_X_CS Interrupt/St*******Screl 4 008   /* Voltage Regulator Control Regi_Y_MODIER_SET_DESC_PTR C00014) Interrupt Enable00d38   /*Setor the GPL-2 (or later)
 */

 Register CLEAegister */
      CHRegisters */

#define     Cleafc00d24   /* DMA Channel 4 Current AddressTHDMA Channel  ADDRESTransmit HoldDDR  0xffc00d24   /* DMA Channel 4 Current AddressRBDMA Channel *******Receive Buffer Slave Mode AddreNFCnt Register */
#define                    DMA4 urat_efine       bces Inc.NAND             DMA4_IRQ_STATUS  0xffc00d28   /* DMA Ch5_X_CSTA

/* DMA Cbine _DEF DMA ral Map Register */
#define                DMA4__X_CIRQ DMA5_X_MODIFY  ADDRES DMA Registers   /* DMA Channel 5 X Modify Register */
#define        MASK5_X_MODIFY *******UNT  0xffc00d58Mask 5 X Count Register */
#define                   ECCAnalog De3bntrol Re DMA ECtion Regis 0nnel 5 Y Modify Register */
#define        F54X_H
#d3bnel 4 PeSC_PTR  0xffc00d601nnel 5 Y Modify Register */
#define        SYSTEM & 3bd30   /*SC_PTR  0xffc00d602nnel 5 Y Modify Register */
#define        *********3b_Y_COUNTSC_PTR  0xffc00d603c00d24   /* DMA Channel 4 Current A_X_COOUNA5_X_MODIFYC00014) SC_PTR  0Count 5 X Count Register */
#define                  DMRSERIPHERAL_MA*/
#define        seMA Channel 5 Peripheral Map Register */
#defi_X_CPdefine       b /* DMA  DMA Pag              DMA4_IRQ_STATUS  0xffc00d28   /* DMA Che     EAD
#define   MA Channel 5Read              xffc00d78   /* DMA Channel 5 Current Y CADDegister */bC_ISR0   DMA Addresap Register */
#define                DMA4_CU    DMAMt Register xffc0011 DMA Chmmanart Address Register */
#define            _X_C008 _WR  0xffc00d8            DI BSWritdefine     /* DMA Channel 6 Start Address Register *R           DMA Channel 5       aart Address Reddres* DMA Channel 5er */
#define                    DMACNT_CONFIGnalog De42ces Inc.
onfigurationDDR  0xffc00d24   /* DMA Channel 4 Current A*/
#I            42ine _DEF0xffc00d5c   /* DMA Channel 5 Y Modify Register */
#defin*/
# DMAUS/
#define   ADDRESral Map Register */
#define                DMA4_*/
#deMMDMA 
#define  ********R  0xffc00d84   /* DMA Channel 6 Start Address */
#DEBOUNCEl 6 Y Modintrol ReDebounc6_CONFIG  0xffc00d88   /* DMA Channel 6 Con*/
#deUNTEegister *42ine      NT  0xffc00d90 scriptor Pointer Register */
#define */
#MAX     DMA6_C30   /* aximMA4_C DMA Channel 5 Peripheral Map Register */
#definester IN     DMA6_C*******Min     DMA6_IRQ_STATUS  0ddreOTP/FUSEnt Register */
#define                    DMOTP#defTROY  0xffc043ces Inc.PHERAusA5_CURR_Y_COUNT  0xffc00d78   /* DMA Channel 5 Currentl MapBEStatus Reg3ine _DEF         dify */

#de00d24   /* DMA Channel 4 Current  Mapnt Register */
3define           ral Map   DMA6_CURR_Y_COUNT  0xffc00db8   /*TIMINe         3fy Regis         Accel 6TimingMA6_PERISecuritynt Register */
#define                    SECURE_SYSSW

/* DMA 43C00014) * DMAe SystA Chwitcher */

/* DMA Channel 7 Register/
#defi Register */
#defMA5_CURR_START__CURR_ADVoltage Regulator Control Regi/
#definDMA Channel 6 C /* DMA _START_Aister */
ddreDMA Peripheral Mux        DMA6_P#define                    DMAC1_PERIMU/
#define 3C_ISR0       * Licensed1           DMAltiplexA Channel 5 ConfiguOTP     / DMA6_DI BS* DMA Channel 5er */
#define                    DMA4 Map008 Analog De43SIC_IAR1         DI BS(00dd8   /-3) a_PTR es the f    r    wDMA6_b DMA C00d24   /* DMA Channel 4 Current A00dd8   F54X_H
#d43efine   Count Register */
#define                    DMA7_Y_MODIFY  0xffc00ddc   /* DMA Channel 7 Y Modify RegisteSYSTEM & 43        Count Register */
#define                    DMA7_Y_MODIFY  0xffc00ddc   /* DMA Channel 7 Y Modify Registe*********43  0xffc0Count Register */
#define                    DMA7_Y_MODIFY  0xffc00ddreHandshake M    is not e GPL-d in     shared file beca    itMAP  0xfavail

#deo /* DMADSP-BF542 proPTR o       DM*OUNT  0xffc00df0   /* DMA Channel 7 Current X Count Regisfc00/*/* DMAINGLE BIT MACRO PAIRS (bit m  /*0xffnegated one)rent Ar */
#defi0xffMULTI     ount      Sinter Register */
#define * DMA ChanOUNT  0xffc00df0   /* DMA Channel 7 Current X Count Register *   /*ICster */   /r */

/* DMA nnelUN    _AFY  
#defin0x0       	 4 YnY_COUNll p         Diegistersscriptor Pointer gister */
#definene  Fster */       /*8_START_ADDR  0xffc00e04   /* DMA Channel 8 S(x)	
#defin(1 << (x))#define            D#x0xffc00e04criptor Pointer Regist(x) (gister */
 ^annel 8 Connfigu    DMAn Register */
#define       Channel WRxt Descriptor PoinIWR_DISABLEer */
#defin              Wakeup Dis

#de8_START_ADDR       DMA8_X_MODIFEN0xffc00e14   /* egister */
#defX Modif*/

#deer */
#define                     DMA /* DMA Channel 8 ConfiguDMA Channel 8 n Register */
   DMA8_X_MODIFY  0xff8_X_COUNT  0xffc00e10   /* DMA ChX Modify Regist* DMA Channel 8 Y*/
#BRR_Y_COs forannel AR   /*fc00dd0   /* DMA ChaPLL_WAKEUter *1
#define/*efin_DESC_PT Channel 8 Current DescripWR0,annel 8 Nel 8 CurrSor Pointer Register */
#de X Co0_ERegist2        DMA               0 Err7_CURRhe GPL-2 (or later)
EPPI     DMA8ne _     DMAs Reg DMA Channel 8 Interrupt/StatSPORT     DMA8 ADD     DMAfc00e2     DMA8_PERIPHERAL_MAP  0xffc00e1    DMA8ntrolannel 8 Peri1 DMA Channel 8 Interrupt/Statu SRegister */C0001annel 8 P       DMA8_PERIPHERAL_MAP  0xfxffc0gister */
ount RegistCURR_T  0xffc00e30   /* DMA Channel RegisRTC/* DMount RegistReal-L  0 Clockfc00ddc   /* DMA Channel 7 YDMA1SYSTEd1c   TATUS  0xffhannels Rer the GPL-2 (or later)
 */

    t X CoC_PTR  0xffc00e40   /*   /* DMA Channel 5 Current DeA9_NUNT  0C_PTR  0xffc00e40   /* MA Channel 9 Next Descriptor PoiSYSTE84   /* DMA Channel 9 StDMA Channel 9 Next Descriptor Poi*****104   /* 0xffc00e40   /*atus Register */
#define      DMA4ter Regration Register */
#d4MA Channel 9 Next Descriptor Poi6c00e44 ration Register */
#d6MA Channel 9 Next Descriptor Poi7       ration Register */
#d7MA Channel 9 Next DescriptorPINTnter X MoC_PTRDMA8in              /* DMA Channel 5 Current D_Y_Cfc00eigurces Inc.  /* DMA Channart Address Register */
#defiRAL_nter Reg         Memory  0xfStreamnel 9 Y Count Register */
#defRAL_fc00e44  Register */
#define       art Address Register */
#definWDOe     0xffes Inc.Wel 4dogAVE_Cffc00ddc   /* DMA Channel ffc0            /* DM                  DM    DMA8_PERIPHERAL_MAP  0xffc00e2rrent X Corrent Addr_IRQ_S#define                  DMA9_IRQ_3Y_COUNT  0fc00e68   /* DM3DMA Channel 8 Current Y Count ReMXVR_Sel 6       t Addr  /* Synchronousgisterc00e30   /* DMA Channel 8 Channel 9 Current0gister NT  0xffc00e30   /* DMA Channel 8 CSTATUS  0xffc00e0xffc00eA Channel 9 Interrupt/Status R   /*hannel 9 fine   0xffc  0xf             DMA9_CURR_Y_COUNT  0xSTATUS  0annel 9A ChannelA Channel 9xffc00e24   /* DMA Channe1 8 Current DESC_PTRSRart AL_DIV  0xffc00004   /* PLL         A Channel 8ght DMA Channel 8 Current Y Count Regiffc08DMA8_IRQ_STATUS  0xff40   /* 8e                 DMA10_START_AD9r */
#define    /* DMA Channe9e                 DMA10_START_AC000 DMA Channel 8   DMA9_CONFI   /* DMA Channel 5 Current D Cha        Configuration Registerart Address Register */
#defiffc0*****Count Regist/* DMA Channeatus Register */
#define     ffc0COUNT 0xffc00e38 /* DMA Channeannel 9 X Count Register */
#defi5*/

/* DMA Chann/* DMA Chann5*/
#define                    DM Configu            DMA9_CONFIefine                     DMA9_X_DR  0xe                   DMAl 10 Start Address Register *00e9efine e                   DMA0_CONFIG  0xffc00e88   /* DMA Ch_COU                 DMA9_CONF1 */
#define                    D1         PTR  0xffc00e40   /* art Address Register */
#definTWIY Modify PTR  0xffine rent Address Register */
#defin00e60   /         DMart Address Register */
#deght 2R/
#deX Modify Reg */
#0d48   /t/Status Register */
#define  T/
#deCurreDMA10_PERIPHChannel 5      DMA9_CURR_DESC_PTR  0xffcSYSTExffc0TR  0xf */
#define               DMA9_CURR_DESC_PTR  0xffc*****fine xffc00eb0   /* DMA Channeart Address Register */
#  /* DDMA5_X_ 0xffc00e58 pheralister */

/* DMA Channel 7 Regi  /* CM                phera_CURR_ADMessDMA5 */

/* DMA Channel 11 RegisteAter *ify Register heraAs Map RegistPack         DMA5_NEXT_DESC_PTs Reffc00e78   /* Dne         9 Current Y Count Register */
s ReDMA Channel 10 ne                      DMA9_CURR_Y_COUNT  0xer */
#deCurrent AddrY Coun  0xffc00e30   /* DMA Channel HOSTTATUS  0xffc00e68   Host      ortDMA Channel 8 Current Y Count ReUSBfc00e78   /* DMA ChanSB11 Configuration Register */
#dPIXCMA Channel 10 R0e58   xeA4_CmpositorDMA Channel 8 Current Y Count Re     URR_X_COUNT  0xffcN0xffFlash            DMA Channel 8 Interrupt/StatuATAPIt Register */
#definer */11 Configuration Register */
#dCANffc00e78   /* DMA Cha/* D DMA Channel 11 Y Count RegistDMAR2c   /* DMl 10 Regist  DMA Overflowegister */
#define              DMhannel 9 DESC_PTR  0xffc01ee0   /* DMA Channel 11 Current Descrip      DMAURR_DESC_PTR  0xffc00eBers */

#define               DMA9_Rfc00eegister */
#definerent Addrxffc00e24   /* DMA Channe2 8 Current  11 InteSRDMA C_MODIFY  0xffc00e94   /* DMA 0_Y_Cter Register/* DMA ChanneA Channel 10 Y Count Register */1ne   xffc00e84   /* DMA ChanneMODIFY  0xffc00e54   /* DMA Cl Mael 9                      DMA1fine                     DMA9RR_X      0 Configuration Registerfine                  DMA5_IRQ_CPERIPH_COUNT  0xffR  0xffcVoltage Regulator Control RegKEY#define         KeypadP  0xffc00eac   /* DMA Channe1        0xffc00e38 fy ReERAL_MAP  0xffc00eac   /* DMA Channe1 10 Per/* DMA Channfy Reter */
#define               DMASDH_Iter *SK  /* ne          SDHfine  A10_IRQ_STATUS  0xffc00ea 0xffc00f04 fc00el 10 Y Modiftream 0 Dart Address Register */
#d1 X CIPERIPH_PTR  0xffc0definxcepMODIFMA_D0_CONFIG  0xffc00f08   /Y_COUNT  0xA Stream 0 De DMA Channel 9 Y Count Register */
#/
#defiffc00ea4   /* DMA   MDMA_D0_X_CMDMA_D0_CONFIG  0xffc00f08   /INTX_COUNT  Destination X Count ReDMA Channel 9 Next Descr1 X DMA Memory DM_X_MODIFY  0x    r the GPL-2 (or later)
 */OTPSE */

/*dify Regisl 7 C_PTR  nnellefy Register */
#define       or R   /*    /* DMA Ch_CURR_A10_IRQ_STATUS  0xffc00ea8  nt Re           define      art Address Register */
#defnt ReEXT_DESCel 9 Peri_CURR_DMA Channel 9 Next Descriptont Re
#defin/
#define       atus Register */
#define    nt ReChannel/
#define       annel 9 X Count Register */
nt Re0_Y_COU/
#define       A Channel 10 Y Count Registent Rene   COUNT  0xffc_CURR_MODIFY  0xffc00e54   /* DMA nt Reel 9 er */
#defin_CURR_fine                     DMA9_Y_CSYSTE  /* DMA Cha  /* DMA ChannDMA Channel 9 Next Descriptor_Y_C*****DESC_PTR  0x  /* DMA Channatus annel 8 Current DeDMAx#define,0xffc_Sination PeripheDination r */
#define                    DDMA X CouRIPHERAL_MAP  0xffc00eec          DMA6_CURR_Y_COUNT  0xffc00db8 WN DMA8_IRQ_STATUS  0xfDirection Configuration Register **/
#deSIZE_DR  0COUNT  0DMA ChannMA CWord Size =*/
#define              DMA10_ /* Memap Reg
#define    Destination Current Y       DMA11_CURR_X_COUNT  0xf /* Mem3           DMA11_Destination Current Y3fine                  DMA5_IRQ_STRR_Xel 6 _COUNT  0xffc00egistannel 8 Current Y Count RegisteRegiR

/* Count RegistWork UnitegisteiMODIr */

/* DMA Channel 7 RegistersDI_SEY  0xel 10 X ModifI BSRegisters 0xffc00Selec        DMA5_NEXT_DESC_PTR  0xDMA St X CouOUNT  0xffc00uration Regist          fc00dd0   /* DMA Channel 7 XN/* Mel 6 Y           Flex Descrip X Mrrentr the GPL-2 (or later)
 */

      _0 DMA Ch_PTR  0xfNextMODIFY  0xffc00f5= 0 (Stop/AutoY  0xf)54   /* Memory DMA Stream 0 Source X1y DMne          */
#define             art Address Register */
#definource X2y DMl 10 Y Modif*/
#define             DMA Channel 9 Next Descriptor ource X3y DMfine  Memory DMA Stream 0 Source Y atus Register */
#define      ource X4y DM_PTR  0xffc0*/
#define             annel 9 X Count Register */
#dource X5y DM5y Register */
#define             A Channel 10 Y Count Register  0xffc06y DM6y Register */
#define             MODIFY  0xffc00e54   /* DMA Chource X7y DM7y Register */
#define             fine                     DMA9rce Inte8y DM            */
#define             /
#define              DMA10_Cource X9y DM9y Register */
#define             0_CONfc00dd0   /* DMA Channel 7 DMAFLOW       _X_MODIFY*/
#dOpeX_MODIFADDR  0xffc00e64   /* DMA C X C_STOter *0xffc00ene    opStream 0 Source Start Address NT  0xffAUTOConfiguration ReA_S0_Y_COUStream 0 Source Start Address RegisterRRA/

#d             DIFY  0xffArrayrs */

#define            MDMA_D1_NEXSMr */
0x6Register */Sm8_STgistl80   /* MemoListrs */

#define            MDMA_D1_NEXLARG     7Register */Largeine               MDMA_D1_STARTory DMA Stream 0 DestinIRQnel 7 CPeripheralDMA_D1_CONFIG  0xDfc00f88   /*Memory DMA Stream 0 Source Curmory ON     RIPHERAL_MAP  0xfory DMODIF0xffc00d58   /* D                 MDMA_S0_X_COiphe   DMA8_IRQ_STATUS  0xfDMA ChD1_X_COUNT  0xffc00f90   /* Memory DMA Stream  DFETC  /*                 0   /* MemoFel 4 00f90   /* Memory DMA Stream 1 DeRUxffc00IPHERAL_MAP  0xffc00eecRunnfc00dc0   /DMA Stream 0 Destin RegPHERAL_MAPFIG  0xffc00f88   /* Memory DMA Stream 1 Destination Configuration Reg 11 TYP     el 10 X Modify Register Typ     DMA6_CURR_Y_COUNT  0xffc00db8PMChanne Register */          DMapped To ThisDMA Channdefine                  Cx_TCPERMemory DMA Stream 0 DCB_TRAFFIC 1 DeOel 6 YIPHERAL_MAP CBegisffic      DMA    oMDMA_D0_NEXT_DESC_PTDEister */
#define     COUNT  0xffcEMA_D1_CURR_ADDR  0xffc00fa4   /* Memory DMA Aister */
#define    /Status RegiDAMA_D1_CURR_ADDR  0xffc00fa4   /* MemorripheROUND_ROBIN#define                     Rouxffcobin0xffc00fa4 Stream 1 Destination Curre Curemory DMA Stream 0 Segister */
#A5_PERIPHE           MDMA_D1_CURR_ADDR  0DMA6_Iannel 8 Interrupt/S Stream 1 DeA5_PERIPHEurrent Address Register */
#defOUNT  0xffc00fb0   /* MemoIRQ_STATUS A5_PERIPH8   /* Memory DMA Stream 1 DestiOUNT  0xffc00fb0   /tus Register */
#A5_PERIPHE      MDMA_D1_PERIPHERAL_MAPOUNT  0xfStream 1 Destination Cnt Registeemory DMA Stream 0 Source CurrPMUXtrea            DMA8         Ddefine  0 Dest22 cemory DMA Streurrent X Count Registe ASYNCHRONOUS MEMORY  RegistLER f04 S 

#define               DM */
#EBIU_AMdefint Des	ffc00fc8   /* Meannel 8 IntAMCKEN			   /*1	    */

#deCLKOUT   /* Memory DMA St	AMBEN_NONEource C0nfiguAll Banksfy Regisd  /* Memory DMA Strea    B0ource C2nfiguration tor Peb0   /* 0xf 0 onlyMemory DMA Stream 1 Sour_B1ource C4nt Register */
#define       s 0 & 1        MMA_S1_X_MODIFY  0xffc0_B2urce C6* Memory DMA Stream 1 Source X M, 1, 7 Cu2ster */
#define     ALLource C8* Memory DMA Stream 1 Source X (all) Memory2ry DMA3er *R  0xffc00fc0   /* DMA_S1_BCTLgister */
#define          #defiB0RDY 0xffc00f30   /* M       ARDYtream 0 Destination Current X Count e    Pster *_IRQ_STATUS RR_DESC_PTR PolaA Cha00d24   /* DMA Channel 4 CurrentB0T

/* **** Descriptor PoitDMA_S0_CO tegis*/
#define                MDMA_S1_           Descriptor PoiSetupMA Stream 1 Source Current Address RegisHURR_ADCOUNT  0xff        StarA Stream 1 Source Current Address ReB0RMA5_X_My Register                   pt/Status Register */
#define          WMDMA_S1_P_X_MODIFY       _MODIF0fec   /* Memory DMA Stream 1 Source PeriB1          0xffc00e58      1C_PTR  0xffc00fe0   /* Memory DMA Stream 1/* Mrce CurrMA Stream 1 Source CurrRegister */
#define                MDMA_S11CURR_ADMA Stream 1 Source* Memory DMA Stream 1 Source Current Address Regi1ter */
#           Source    MDMA_S1_IRQ_STATUS  0xffc00fe8   /* Memo1y DMA St_DLL  0xffc03100 errupt/Status Register */
#define         1 MDMA_S1_Pel 9 Peri Sourcexffc00fec   /* Memory DMA Stream 1 Source Periphe1al Map Regi0 Regist Source             MDMA_S1_c   /* Memory DMA Stream 1 So /* DMA Channel 10 Next Desff0   2           MDMA_S1_CURR_DE2C_PTR  0xffc00fe0   /* Memory DMA Stream 1    rce Current Descriptor P 0xffc0Register */
#define                MDMA_S12CURR_ADDR  0xffc00fe4  2/* UART3 Registers */

#define                    2ter */
#define         2   /* Divisor Latch Low Byte */
#define      2y DMA Stream 1 Source I2LH  0xffc03104   /* Divisor Latch High Byte2 MDMA_S1_PERIPHERAL_MAP -BF5fc00fec   /* Memory DMA Stream 1 Source Periphe2al Map Register */
#defi2e             MDMA_S1_CURR_X_COUNT  0xffc00ff0   3* Memory DMA Stream 1 Sour3C_PTR  0xffc00fe0   /* Memory DMA Stream 1        MDMA_S1_CURR_Y_COUNT 0xffc0Register */
#define                MDMA_S13nt Y Count Register */
3/* UART3 Registers */

#define                    3   UART3_DLL  0xffc03103   /* Divisor Latch Low Byte */
#define      3                 UART3_3LH  0xffc03104   /* Divisor Latch High Byte3*/
#define              #deffc00fec   /* Memory DMA Stream 1 Source Periphe3Register */
#define     3                  UART3_LCR  0xffc0310c   /* MBSefinemory DMA Stream 0 Source CurAMSB0efine  3DR  0xffc00*/
#define          sefine                  MDMA_S0_X_Cr Co1efine  DDR  0xffc00*/
#define        1     EPPI1_VDELAY  0xffc01310   /* EPPI12nt RegisCOUNT  0xff*/
#define        2     EPPI1_VDELAY  0xffc01310   /* EPPI13Vertical /* EPPI1 Lines per Frame Reg3     EPPI1_       EPPI1_VCOUNT  0xfODE    MDMA_D1_Y_MODIFY  0xffc00f9cB0     egister */
#define                 C_PTR  tream 0 Source Start Address Regis B11c   /* l Delay Count Register */
#defindefine                    EPPI1_CONTROL  0x21c   /* E /* EPPI1 Lines per Frame Regisdefine                    EPPI1_CONTROL  0x3fc01320   0xffc01318   /* EPPI1 Samples define         c   /* Memory DMA StreaF0130c   /* EPPI1 Vertical TranTESTSETLOC     ter RegisterTest xffcers */

#define               DMOL  0xCL     6DR  0xffc00furst cers *frequencr */
#define                MDMA_SPGWgiste     1_CURR_DDMA5wait statdc4   /* DMA Channel 7 Start Add BlanSZMemory DMA Streadefins00f54   /* Memory DMA Stream 0 SgisteDD Regis/* DMA Cannel d dI BSdelar */
c   /* Memory DMA StreamRBnnel emory DMA Stream 0 Source CurrP  0xffces Per Line RegArbitX_MODIF    er */

/* DMA Channel 7 RegistersBGnnel 10 rent Descriptus gran      ine       /* Memory DMA StreaDDR Source Y Modify Register */
#defin  TREFI /* Effine   annelfresh/
#defva DMA7_CONFIG  0xffc00dc8   /* DM0_MAF */

3     Stream 1 R-r   /* PcR  0xffART_00fa4   /* Memory DMARegister */
#defiter *             reTARTrge-to-act   /UEST  0xffc01408   /* Pin Interrupt 0 InterruptTRAing Re        */
#de A         p#define  MA Stream 1 Source Current Address Reg TRe                 er */
#de       A Stream 1 SourcDDRsterS /* 	((x<<22)&definfiguPTR tupt = (1~15) cycldc4   /* DMA C*/
#dePne        18    P PINT0_EDGEPLEAR  0xffc01414   /* Pin InterrupCne         6    C PINT0_EDGECLEAR  0xffc01414   /* Pin InterrupFINT0_INVERT14    F0xffc01418  F /* Pin Interrupt 0 Inversion Set REFIne    x& /* P             PINT0_INVERT_CLEAR  Interrupt 0 Mask Set Registerontrol Register */
#define      410   e                 0 Edge-seX Mod_MODIF         GN  0xffc0140c   /* Pin InterrMn RegisCOUNT  0xfftreamricense oxffcto               PINT0_EDGE_SET  0xffc01410 /
#def_S0_CURR_DES DMA6_COcoverypt/Status Register */
#define  DDRDATWID   /* T3_DPTR  0xffDRdefinewidton X Modify Register */
#definEXTBANter  Countfine     xternal bce X ASK_SET  0xffc01430   /* Pin EVterrupt 1 Mas        gisteevice/
#define                 PINT1_M#defin        ount RegistePINT1_REQUERegister/ EPPI1 Lines of Active VideoTWTegiste */
#define DMA6    MA7_Y                 */
#dWTRne         sitiWTR PINT0_EDG 0xfEAR  0xffc01414   /* Pin InterruMRDne        efinMRD PINT0_EDG24  EAR  0xffc01414   /* Pin InterruW#define                 PINT_EDGE_SET  0xffc01440   /* Pin IntRCrupt 1 erruCtivity Set upt EAR  0xffc01414   /* Pin Interr InterruourcA_S1nfigugister */
#define         K_CLEAR_00fdnfigu1 ePin Interrup Inversion Set Regis2ourc Poinfigu2fine          4   /* Pin IntEVSZ_64AR  0xff  /* Pin I Pin Inte     fc00f68 4MBrupt 1 Inversion C128ourc_ADDRer */
#define                 128PINT1_PINSTATE  0xff256ourc     er */
#define                 256PINT1_PINSTATE  0xff51EAR er */
#define                 512PINT1_PINSTATE  0WD_4Sour /* Pin ID_REQUEW#defi    Bits* DMA C32-bit) */

#d8SourcDMA S                   PINT28MASK_SET  0xffc01460   /* 1     A_S1_                   PINT216MASK_SET  0xInterrupt 0 Mask Set Registeregister */
#define          BURST0000   /* 7c0132c   /* EPPI1l                  PINT0_LATCH  0CASLATENC/

#d     */
#definS latRegister / EPPI1 Lines of VerticaDLLRE*/

/* ne           LL  0xffc            PINT0_LATCH  0xffc01REMA Striguration Relicense omreame         DMA6_CURCL_1_5Sourc5CLEAR  0xf    L PINT2_= 1.5fc01414   /* Pin InCLLEAR2_MASRegister */
#define    2          PINT2_EDGE_CLvity Se6xffc01474   /* Pin Interr            PINT2_EDGE_C3EAR  3xffc01474   /* Pin Inter3ster */
#define                   PINT0_* Memo / EPPI1 Lines of Vertical BlaA /* DM468   /* PinPartial ay DMAselfPINT0_REQ 2 Mask Clear Register */
#deQU             EPPI1_CLKDIV  0xDEB1_PFL X Couster */
#def
#defationrupt 2  0 DeEB1          ffc01438   /* Pin InterruptEB2/* Pin IntDDR  0xffc00 Status Register */
#def2ne                      PINT2_LATCH  0x3/* Pin IntePPI1 FS2 PerStatus Register */
#def3ne                      PINT2_ StrARB_PRIORITne      Register upt 0 Registbetweenress bu                   PINT2_LATCH0   /URGECurrent ask Set RegefinUrgeT  0xffc00fb0   /* MemLATCH  0xff 3 Mask Cl  0xffc00e50 */

define                    PINT3_REQ3EST  0xffc0               0xdefine    Interrupt 0 Mask Set ReERRMSfc01334   /* EPPI1 Clipping R0   /ERRO Pointer Register */
#         DMA11_CURR_ADDR  0xffc0 0xff
#define _IRQ_STATUS  */

 PINT3_EDGE_SET  0xffc014a0   /* Pier */efine                0x         DMA11_CURR_ADDR  0xffc0CORE Interrupt       MDMA_Core ePINT3_EDGE_SET  0xffc014a0   /* Pi_M
#define  urrent Address   PINT3(2ndNT  0xffc00f58   /* Memory  0xffRT_SET  0xine          tivity Setrupt 3 Inversion Set Register */
#3efine      el 10 X Modif  0xffc014rupt 3 Inversion Set Register *ge-seRT_SET  0xster */
#defier **/
#define     SSIGN  0xffc0149c   /* PinADDc01334   /* EPPI1 Clippin     C_PTRESgister * Pin      MA Chhannel 6Status Register */
#defineRST0130c   /* EPPI1 Vertical TransfDDRSerrupt 2 P            DR soft rter */
#define                PFTCH   PORTA_F             DR pretus Reunction Enable Register */
#defdress RRREQ/* DMA Channel 8 on Clear Regdth Rs */
#define                   PINSRAmples PCOUNT  0xff         PORTacknowledXT_DESC_PTR  0xffc00ec0   /* Mfc01  DMA             PINMobl 7     terrupt 2 Interrupt 0 Mask Set RegisBRC */
#define                 PINT0_ 4d0     /* Pin Inter_DESC_PTR  0xffc00fc0   /*  0xffc014d0PINSTATE  0xffc01420   /* Pin Inter  /* fine                  PORTA_DIR_CLEAR  0xffc014d4   /egister */
#define            r */
#deSYSTEM                  PORTA_DIR_CLEAR  0xffc014d4   /           PINT2_INVERT_CLEAR  0xffltipl                     PORTA_MUX  0xffc014dc   /* Multiannelr Control Register */

/* Port B COUNT                  PORTA_MUX  0xffc014dc   /* MultiA Char Control Register */

/* Port B 0_Y_C                  PORTA_MUX  0xffc014dc   /* MultiMODIFr Control Register */

/* Port B ne                     PORTA_MUX  0xffc014dc   /* Multifine r Control Register */

/* Port B el 9                   PORTA_MUX  0xffc014dc   /* MulW0   /* GPIO Direction Set Register */t Re   PORTB_DIR_SET  0xffc014f0   /* GPIO Direction Set * GPIO Direction Clear Register */
#Regi   PORTB_DIR_SET  0xffc014f0   /* GPIO Direction Set GPIO Input Enable Register */
#definter    PORTB_DIR_SET  0xffc014f0   /* GPIO Direction Set plexer Control Register */

/* Port  */
   PORTB_DIR_SET  0xffc014f0   /* GPIO Direction Set  0xffc014e0   /* Function Enable Regon E   PORTB_DIR_SET  0xffc014f0   /* GPIO Direction Set 14e4   /* GPIO Data Register */
#defiste   PORTB_DIR_SET  0xffc014f0   /* GPIO Direction Set O Data Set Register */
#define      #def   PORTB_DIR_SET  0xffc014f0   /* GPIO Direction Set ear Register */
#define                    PORTB_DIR_SET  0xffc014f0   /* GPIO Direction SACCfc01334   /* EPPI1 Clipping Regiegis       PORTB_DIR_SET  0xffc014f0   /* GPIO Direction STA             PORTC_DIR_CLEAR  0xffc0TE14   /* GPIO Direction Clear Register */
#define     AR             PORTC_DIR_CLEAR  0xffc01       PORTB_DIR_SET  0xffc014f0   /* GPIO Direction SG0   /* GPIO Direction Set Register *  */
ltiplexer Control Register */

/* Port D Registers *PINSTATE  0xffc01420   /* Pin Inter defiltiplexer Control Register */

/* Port D Registers *GPIO Input Enable Register */
#defi     ltiplexer Control Register */

/* Port D Registers *           PINT2_INVERT_CLEAR  0xff        PORTB_DIR_SET  0xffc014f0   /* GPIO Direction SMCENc01334   /* EPPI1 Clipping RB0WCa Clear Re  MDMA_S1_CURR_DESC_MODIFcMA6_Iterrupt 2 Edge-sen* Global Control R Direction rent Descriptor P                        PORTD_DIR_CLEAR  0xffc01234   /* GPIO
#define       /* Interru                PORTD_DIR_CLEAR  0xffc01334   /* GPIO       MDMA_ter */
#defin                PORTD_DIR_CLEAR  0xffc014O Direction Stream 1 Source I4efine                  PORTD_DIR_CLEAR  0xffc01534   /* GPIO tream 1 Source I5efine                  PORTD_DIR_CLEAR  0xffc016NEN  0xffc015tream 1 Source I6efine                  PORTD_DIR_CLEAR  0xffc017    PORTD_MUXtream 1 Source I7efine                  PORTD_DIR_CLEAR  0xffc010Rters */

#defPERIPHERAL_MAP  0MA7_Y                PORTD_DIR_CLEAR  0xffc015   /* GPIO Dl 10 Y Modiflear Re*/
#define                    PORTE_DIR_SET  2   /* GPIO D_PTR  0xffc0e Regis*/
#define                    PORTE_DIR_SET  3   /* GPIO D            amples */
#define                    PORTE_DIR_SET  4   /* GPIO Dataister */
#defi401558   /* GPIO Input Enable Register */
#defi5xffc01550   /* ister */
#defi501558   /* GPIO Input Enable Register */
#defi6DIR_CLEAR  0xffister */
#defi601558   /* GPIO Input Enable Register */
#defi7             POister */
#defi701558   /* GPIO Input Enable Register */
#ROWACT               est RegisterRow*/

/*atiplexer Control Register */

/* Port E ReRWRTF_SET  0xfA_S1_CURR_Y_ GPIO/W Turn arIPHER                PORTD_DIR_CLEAR  0xffc0 /* 01560   /* Fun0156c   /* G    PINT0_REQUEister */
#define                    POGC0F_SET  0xffc015ne      DR G01400n Set 0 Register */
#define                 1F_CLEAR  0xffc0  0xffc01574   /* GPIO 1 Register */
#define                 2_DIR_SET  0xffc  0xffc01574   /* GPIO 2 Register */
#define                 3TF  0xffc01564                 PORTF_M3 Register */
#define                  Register *T3_DLLt Addre1574   /*DMA6_I       DMA7define                    POC30c   /* EPPI1 Vertical TransfCPIO 5_PERIPHter Register  */
#Multiplexer A10_IRQ_STATUS  0xffc00ea8   C15345_PERIPH_IRQ_STATUS e                 art Address Register */
#defiC_INE5_PERIPH
#define    e                 DMA Channel 9 Next DescriptorC    5_PERIPH Clear Regist                 atus Register */
#define     Cgista Registeter */
#defi                 annel 9 X Count Register */
#C EnaGPIO Data PORTG_DIR_CLEAR  0xffc01594 A Channel 10 Y Count RegisterCGPIOc0158c    PORTG_DIR_CLEAR  0xffc01594 MODIFY  0xffc00e54   /* DMA CC* GP_DIR_SET  PORTG_DIR_CLEAR  0xffc01594    MDMA_S0_PERIPHERAL_MAP  0xfCBR            Register CLEAR */
#define        PORTG_SET  0xffc01588   /          l 10 Y Modif  PORTH_FER  0xff                 PORTG_CLEAR  0x          _PTR  0xffc0  PORTH_FER  0xff/
#define                    POR                        PORTH_FER  0xffon Set Register */
#define                     DMA10_PE PORTH_FER  0xff  /* GPIO Direction Clear Registster */
#defi /* GPIO Data Clear RegistG_INEN  0xffc01598   /* GPIO InpData Register /* GPIO Data Clear Regist             PORTG_MUX  0xffc015 /* GPIO Data /* GPIO Data Clear Regist
/* Port H Registers */

#definRA0xffc015ac   ral Map R PORTH Data Set y DMn Set a4   /* Pin Interrupt 3 EdgRWTPIO Input A_S1_CURR_Y_ter */PIO turn-Clear Register the GPL-2 (or later)
 */

#i              lexer Control aPIO Direction Set r the GPL-2 (or later)
 */

#G0IO Input Enables Inc.
 */
#c01400  0xffc015a0   /* Function Enable R CG1c   /* Multipl              PORTI  0xffart Address Register */
#definCG2                            PORTI  0xffDMA Channel 9 Next Descriptor CG3egister */
#d#define                   * Memory DMA Stream 0 D(c00exMAP c00eA -ffc01J) includexffc01x_FER,tion SeSETegister  /* Degister DIr */
                         INRTD_DIR_SET  0xffc01530   /* G*/
#defiDMA ster */
#definGPIO0   /* DMA Channel 5 Current DescripPter ata Set Regist         DMA5_CURR_ADDR  0xffc00d64   /*Pta S   /* GPIO Dat     fine                  DMA5_IRQ_STATPterrET  0xffc01590     atus Register */
#define           P  /*    PORTG_DIR_     annel 9 X Count Register */
#d  PORTJPORTCine              rent Address Register */
#defi  PORTJne                    MODIFY  0xffc00e54   /* DMA Ch  PORTJel 9 X                   MDMA_S0_PERIPHERAL_MAP  0xf  PORTJDR  0                 /
#define              DMA10_C  PORTJefinel 10 Y Modif     0_CONFIG  0xffc00e88   /* DMA/* GPIO gister *nable Register    /* DMA Channel 5 Current DescriPxxffc00            ister     DMA5_CURR_ADDR  0xffc00d64   /PxNEXT_DESC1434   /*ister fine                  DMA5_IRQ_STAPx/
#defindefine          atus Register */
#define          Px Channeldefine          Function Enable Register */
#definPxA11_PE/
#define   ister 14e4  nterrupt 0 Mask Sfc015_iste0   /*      r Register */
#define            PxMfine ster */
#def     MA7_Register */
#define                 M InpuDDR  0xffc00*/
#defin15f4   /* GPIO Direction Clear RegisMRTI_Me           */
#defin             PORTJ_INEN  0xffc015f8 M* Por*/
#define  */
#definegister */
#define                  M_FER _S0_CURR_DES*/
#defin5fc   /* Multiplexer Control RegisteMPORTCcIO Direction SetMA7_    PORTJ  0xffc015e4   /* GPIO DaPxM  0xf Mask Set Re Timer 1                      PORTJ_SET  0xPxM01510fc01434   /* Timer 1 et Register */
#define            PxMDR  0             Timer 1 ffc015ec   /* GPIO Data Clear RegiPxMefineount Registe Timer 1           PORTJ_DIR_SET  0xffc015  0xuratiolear Registmer 0 Coun   /* DMA Channel 5 Current Descr  0xxffc01             TIMER2_C    DMA5_CURR_ADDR  0xffc00d64     0x       e         mer 0 Counfine                  DMA5_IRQ_ST  0x/
#defi4   /* Timer 2 Countatus Register */
#define           0xer 0 WidInterruptmer 0 CounFunction Enable Register */
#defi  0x  0xffc0                   14e4  */

#define        INTxl 8 StSET/        _CONFREQUESRTI__CONFLATCHonfiguraEDGE0xffc01630   /* TiINVERT0xffc01630   /* Tifigunnel             EPPI1_CLKDIV  0xffc0   IBfine             Registers l 8    /* DMA Channel 5 Current DescripIB Input Enable Reg        TIMER3    DMA5_CURR_ADDR  0xffc00d64   /*IBRTI_MUX  0xffc015        TIMER3fine                  DMA5_IRQ_STATIB* Port J Register        TIMER3atus Register */
#define           IB_FER  0xffc015e0         TIMER3Function Enable Register */
#defineIB                         TIMER3    PORTJ  0xffc015e4   /* GPIO DatIBRegister */
#defi        TIMER3                     PORTJ_SET  0xfIB015e8   /* GPIO D        TIMER3et Register */
#define             IB       PORTJ_CLEA        TIMER3ffc015ec   /* GPIO Data Clear RegisIBr */
#define             TIMER3          PORTJ_DIR_SET  0xffc015fIB  /* GPIO Directioefine             /* DMA Channel 5 Current DescriIB PORTJ_DIR_CLEAR  efine              DMA5_CURR_ADDR  0xffc00d64   /IBter */
#define    efine          fine                  DMA5_IRQ_STAIB  /* GPIO Input Enefine          atus Register */
#define          IB      PORTJ_MUX  0efine          Function Enable Register */
#definIBr */

/* PWM Timerefine          ters */

#define       nt Re*/
#define             MDMA_D0_CURR_X_COUT1c   /* EPPI1 Clock _CURR_tream 0 Source Start Address RegiPULSE_H_CLEA
#define    PulseT3_RBR  0xffc0312c   /* Receive Buffdefine_ Curren       MDMA_xffc00fOUNT  0xffc00fb0   /* Memimer 6 CounRQ                TIMER4_COUNTERR_SET  0          DMA6_CURR_Y_COUNT  0xffc00TINtream 0 ine         _CURR_Input#define                  MDMA_S0_X_COOUTFY  ble Register */
Out 0xfPnel  Registxffc00ef8   /* DMA Channel 11 L 0xfinter   /* GPIO D_CURR_ters *define                  MDMA_S0_X_TOGGLWidth Re            Toggln StarDTH  0xffc0167c   /* Timer 7 WEMU        l 10 Y ModifEmul      Behavixffcefine                  MDMA_S0_X_COERR_TYter *fc01434   /* MA Chion Y Mo    TIMER6_PERIOD  0xff    DMA */
#define                 PINT0or RNfine                               DMA6_CURR_Y_COUNT  0xffc00dRegis Input Enable Regy DMA Sts */

#define                      DMAC1_TRTI_MUX  0xffc015        s */

#define                      DMAC1_T* Port J Register DMA Strs */

#define                      DMAC1_T_FER  0xffc015e0 ister */s */

#define                      DMAC1_T                 00f24   s */

#define                      DMAC1_TRegister */
#defiss Regiss */

#define                      DMAC1_T015e8   /* GPIO D 0xffc00 0 Source X       TIMER_STATUS0  0Y  0xffurce Y Modify Register */
#definTIMDISter */

/* DMAC1 RegisterMER7_WIDTH  0xffc0167c   /* Timer 7 Wtion RCPER  0xffc01b0c   /* DMA             DMA12_X_COUNT  0xffc01c10   /*ster */
#define                       DMA12_X_COUNT  0xffc01c10   /*MA Controller 1 Current C             DMA12_X_COUNT  0xffc01c10   /*ters */

#define                      DMA12_X_COUNT  0xffc01c10   /*DMA Channel 12 Next Descr             DMA12_X_COUNT  0xffc01c10   /*        DMA12_START_ADDR              DMA12_X_COUNT  0xffc01c10   /*ess Register */
#define  MER7_WIDTH          DMA12_CONFIG  0el 7 C688   /* Timer Group of 8 Status RegILter */

/* DMAC1 RegisterRegisters define                      DMAC1ILCPER  0xffc01b0c   /* DMA                 DMA12_IRQ_STATUS  0xffc01c28ster */
#define                           DMA12_IRQ_STATUS  0xffc01c28MA Controller 1 Current C                 DMA12_IRQ_STATUS  0xTOVF Int  /* Me
/* DMAC1 RegisterR  0xffce0   /* D12_CURR_X_COUNT  0xffc01c30   /* DM#definexffc01b0c   /* DMAunt Register */
#define               DMA12_CURR_Y_Cstinati
#define          unt Register */
#define               DMA12_CURR_Y_C_PERIPHroller 1 Current Cunt Register */
#define               DMA12_nterruUster */nnel 10 Inte       Slav         egister */

/* DMA Channel 11 Re          #define ART_ADDR  0xffc11c44   /* DMA Channel 13 Start Address Register */
#defistinationdefine          c44   /* DMA Channel 13 Start Address Register */
#defi_PERIPHERller 1 Current Cc44   /* DMA Channel 13 Start Address Register */
01c28_FER  0    PORTI_ister */                 DMA12_IRQ_STATUS  0xffc01c28       A Channel 13 X M5dify Register */
#define                    DMne       Channel 13 X M6dify Register */
#define                    DMel 9 X MoChannel 13 X M7        DMA12_CURR_X_COUNT  0xffc01c30   /* DM  /* DMA C* Memory DMA 4fine              DMA13_NEXT_DESC_PTR  0xffc01c40   /A13_Y_COUN* Memory DMA 5fine              DMA13_NEXT_DESC_PTR  0xffc01c40   /e         * Memory DMA 6fine              DMA13_NEXT_DESC_PTR  0xffc01c40   /odify Regi* Memory DMA 7scriptor Pointer Register */
#define                    /* DMA CTATUS  0xffc4                 DMA13_X_COUNT  0xffc01c50   /* DMA Chan        DMATATUS  0xffc5                 DMA13_X_COUNT  0xffc01c50   /* DMA Chanter */
#def Address Regisc44   /* DMA Channel 13 Start Address Register */
#defi13 InterrupTATUS  0xffc00c44   /* DMA Channel 13         DMA12_CONdefiCOUNTr Register */
#define            WDEV 0xffc0132c   /*     DMA9Evine                    PINT3_0   /* DMAStatus fine       escriptor         DMA6_CURR_Y_COUNT  0xffc00db8WDR/

/*/
#define   ADDR  0xfRcensdisterRegister */

/* Port A*/
#define r Register */
#define            CNTtion Set RegisterR  0xffcfc01c84   /* DMA Channel 14 Start AddreDEB GPIO Direction C Current fc01c84   /* DMA Channel 14 Start AddCDGIN Chan  PORTG_DIR_CDG 0xffRegister Inver         DMA12_IRQ_STATUS  0xffcCUDODIFY  ine          UD DMA Channel 14 X Modify Register */
#define        ZMODIFY  Register */
#ZM DMA Channel 14 X Modify Register */
#define      CNefine    /
#define   unt Regisne    ng                    EPPI1_CONTROL  0  ZMZ_Y_COUNT             ZeroeMemo             DMA14_X_COUNT  0xffc01c90   /BNDfc01324   64   /* GPIIPHEary Latch Reg                   EPPI1_CONTROL  0INP#define/
#define    UG 7 Cu /* D  0xfine                  DMA12_CUegister */r Register */
#define            ICItion Set RegisterIllegal74  y/Bin01caCmer Registers */

#de            PINT0_LATCH  0xffc01Une     _IRQ_STATUS UpI  0xffcac   /* DMA Channel 14 Peripheral Map Register *Dne                  ow        cac   /* DMA Channel 14 Peripheral Map RegisterMINne            MDMA_egisDMA6_Ie               DMA14_CURR_Y_COUNT  0xffc01cb8  AXne      ister */
#deaxnt Y Count Register */

/* DMA Channel 15 Registers *COV31#definetream 1 Sour Conne         T_DESC_PTR  0xffc01cc0   /* DMA Channel 15 Next D15nt X Co Pointer Regist15 */
#define                 DMA15_START_ADDR  0xffc01cc4 ZERO DMA Ch Y Count Registeer * Des4_CURR_X_COUNT  0xffc01cb0   /* DMA Channel 14 Cu*/
#PIO Data Clear Re       Dcac   /* DMA Channel 14 Peripheral Map RegisterCZMEcriptor nel 14 Currentter */
#define  hannel 15 X Count Register */
#define   Z DMA Chanel 14 Current Descriptor PoiDMA Stream 0 Source Xc01ca8   /* DMA Chann          MDMA_D1_Y_MODIFY  0xffc00f9c fineABLE0        DMA14_PERIPHERAL_MAP  0xffc01cac   /* DIdentifixffc00ddc   /* DMA Channel 7 Y ModiU   DMA1e               DMA14_CURR_X_COdify Register */
#define              DMA15D   DMA1ount Register */
#define         dify Register */
#define              DMA  /* _CLEAhannel 14 Current Y Count RegistDDR  0xffc01ce4   /* DMA Channel 15 Curren
#deABLE0             DMA15_NEXT_DESC_PTRdify Register */
#define              DMt DescRR_DES Pointer Register */
#define         #define             DMA15_PERIPHERAL_MAP  015/
#defiannel 15 Start Address Register */
#d#define             DMA15_PERIPHERAL_MAP G  0xdress c8   /* DMA Channel 15 Configuratiodify Register */
#define              DMA1*/
#ABLE0  0xffc01680fc01cd0   /* DMA Cdify Register */
#define              DMA     c01cec        DMA15_X_MODIFY  0xffc01          DMA16_NEXT_DESC_PTR  0xffc01d00   Z_COUNT  ine                    DMA15_Y_COUNT  0dify Register   /* DMA Channel 15 Y  Channelr Register */
#define          W1L Currenine         Loaation RA Channel 5 Con  DMA16_CONFIG  0xffc01d08   /t/Status5 Interruptnfiguegis008   /* Voltage Regulator Control RegiMA16*/
#defDDR  0xffc010   /a7_X_COUNT  0xftor Pointer Register */
#d1ZMOa0   /*_START_ADDR ration RZM       unt Regisent egister */
#define       fc00da0  ister */
#define             PRESCAear Rehannel 16 Configuration Register */
#  /* DMA Channel 1RT   DMA5r Register */
#define         SECONDing Reine     gisterondfc00f90   /* Memory DMA Stream MINUTEgister*/
#defi Curreu           EPPI1_FS2P_LAVF  0xffc01HOURefine1Register    Du      version Set Register */AY             Dfe_PTR  0xfa0xffster */

   /* DMA Channel 16 YIDMA14_NEXT_DESC_c00fWion     ERRUPT    DMAine             MA Swel 4 cac   /* DMA Channel 14 PeripheALARMnel 16 Interrupt/Stat_IRQ_STATUS Alarme             DMA16_PERIPHERAL      Dnel 16 Interrupt/Stat
#define    TR  0xffcac   /* DMA Channel 14 Periprrent Dnel 16 Interrupt/Stathannel 14 Curreegistcac   /* DMA Channel 14 PeripheDMA16nel 16 Interrupt/Statu5 Interruptc01d24cac   /* DMA Channel 14 PeriTWENTY_FOUR_nel 16 Current Y Count Regc   /* DMA C24* DMA Channel 17 Registers */

#defent AP  0xffc01d2c   /* DMA ChSource Configuyripheral Map Register */
#define   WRITRegiMPLETEer */
#define         er */
#definDMA6_mory DMA Y_COUNT  0xffc01cd8   /* DMA Channel 1ATUS  Modify Register */
#d DMA ChannEVENT_FLAe    us Register */
#definer PoinFla00dc0he GPL-2 (or lateAP  0xdefine        annel 16 PeripheraUNT  0xffc01d50   /* DMA Chan        define        T  0xffc01d30   /* DUNT  0xffc01d50   /* DMA Chan Registedefine               DMA16_CURR_Y_UNT  0xffc01d50   /* DMA Channenel 16define         ster */

/* DMA CUNT  0xffc01d50   /* Dine              D Count Registerxffc01d40   /* DMA CUNT  0xffc01d50   /* DMA Chinter Regi54   /* DMA Cha           DMA17_STARUNT  0xffc01d50   /* DMA CChannePENDING_nel 7 ConfiPORTJ_MUX  0      Pend     Channel 13 Start Address RegiChannel 17 Staer */
#define                  d1c   /* DMA Channel 16 Y W1 Destination Peripheralegister */
#A5_PERIPHER 0xffc0140*/
#define_DESC_PTR  0xffc00fc0   /* 16 YAP  0ify Register */
#define              DMA16_CURR_DESC_PTR  0xffc01d20   /* DMA Channel 16 Current Descriptor Pointer Register */
#define                  DMA16_CURR_ADDR  0xffc01d24   /* DMA Channel 16 Curr 16 Current Register */
#define         DMA16_IRQ_STATUSPR/* GPIO Direction Clear Register */
8 Reg            DMA8resca /* D             PORTA_DIR_SET  Map Register Register */
#define      AL_M_F_PTR  0x1 0xffc0DMA7_NEXT_DESCLatch Regi     PORTJ_DIR_SET  0xffc015fFI0xffc00fnel 14 Cur         cac   /* DMA Channel 14 Peripheral Map RegistF VidD0_Y_CO_START_ADDR          isterDecodr */

/* MDMA Stream 0 RegistersFWR Vidffc01498   /* Pi               ister  /* DMA Channel 18 Start AddreFR       PORTJ_MUX  0         */
#dfc01c84   /* DMA Channel 14 Start AddrFWA18_NEXUNT  0xffc00f18Channel 18 X   /* DMA Channel 18 Next Descrit X r Register */
#define            Ft X Count 0xffc0140
#define                Channel 18 Next Descri          MDMA_D1_Y_MODIFY  0xffc00f9c Fl 17_NEXT_DESC_PTR  _NEXT_DESC_PTR  mory DMA Stream 0 Destination Y Cou     Interrupt 3 Edge-sens_NEXT_DESC_PTR           DMA11_CURR_ADDR  0xffc00 MMRGLOnt Reg15 Interrupt/*/
#dexffc00flicense oGassternfiguA18_CURR_ADDR  0xffc01da4   /* DMAmples gister */
#de Address Register */
#define   efine                   EPPI1_FS2FPGM      DMr */
#define       Program8_Y_COUNT  0xffc01d98   /* DMA Chefine  r Register */
#define         USECDI ChanMA18_STAinter cro30   /*0d20 fine                    DMA18_X_CountAC */

7        er */
#dC_PTR  0xfIDTH  0xffc0167c   /* Timer 7 WiPUMPRr Field    PORTI_F      Puc0000leaisteffc01db8   /* DMA Channel 18 Current YSUWidth Regster */

/* DMA Cha    MDRegisters */

#define              DMA1H   /* Pear Registe
/* DMA Cha StarRegisters */

#define             PGMor R    DMARAL_MAP  0A ChannRegisterannel 8 Current Des
#define     r Register */
#define         EMUDABinter5_Y_MODIFY      TIMER_MER7_WI.t X Count Register */
#define  STnnel 19 _IRQ_STATUS  0xffcMER7_WIDTH  0xffc0167c   /* Timer 7 WL1Innel 19 CDDR  0xf  DMterrstruMDMA_D */
#def*/
#define                    DMA19_L1DAnnel 19 14      DMA19_Xgister    Affc01dd4   /* DMA Channel 19 X Modify Register *Bnnel 19 /
#define           DMA19Bffc01dd4   /* DMA Channel 19 X Modify Registe PoinOVter Regi_PTR  0xffc00ffc01dd4C_PTR  e0  rier Group of 8 Enable Register */l MaY Modifiguration Regis1efine              DMA19_CURR_DESC_PTR  0xffc01de0    ChaY ModifPORTJ_MUX  0    TIMER_er Register */
#define                 MA_D0#define             30  rets8_Y_COUfine                    DMA19_X L2egister */
SC_PTR  0L2ffc01dd4   /* DMA Ch#define                  ptor Pointer Register */
#define    DMA Chanfine                   3_PERIOD  0xffc01638   /* Timer        Input Enable Reger */
#    DMA5_CURR_ADDR  0xffc00d64       RTI_MUX  0xffc015er */
#fine                  DMA5_IRQ_      * Port J Registerer */
#* Memory DMA Stream 0 DA Channel 7 Cofy Register */
#define           1c   /* EPPI1 Clock _STARTdMA Cha_CURR_ADegis Port Interrupt 1 Registers (32-bitNM
#define         N0xffask

#de                 DMA12_IRQ_STATUS  0xffAFVALIMA Channel 16 Periuthfy Rc      Firmware Vali             DMA18_IRQ_STATUS MA CEXI       PORTG_DIR_ss Register */
#define  Ex
#define               DMA10_C8   /nnel 10             ter */
#define       #define        LL_  0xDMA Channel 14 Configuration Regis Registe            Intedefine                  MDMA_S0_X_COUN S Registine         ADDR  0xefine           			X_MOer *1MDMA_S1_e14 VB  = VCO / Channel 19 Cc01e18   /*      1annel 20 Y Count RDMA Channel c01e18   /*4urce 2annel 20 Y Count Rannel 9 X Coc01e18   /*8urce 3annel 20 Y Count Rl 10 nel 20 X Count RegisteDMA14_NEXT_DESC_PTR  0xffc01c80   /* M Regist7rol Re /* DM 0xffcgistdefine                  MDMA_S0_X_COUBYPA4b4               8_CUBypa    A Channel 14 Peripheral Map ReOUTP*/
#EL7 CurrCURR_Y_COUNT Pin Intennel 18       D     fc01c84   /* DMA Channel 14 StarINMA20_IRQ_STATAP  0xffc01d28   /* DMA Chan4_IRQ_Snterrupt/Status Register */
#define  ffc015f0DWne     ister */
#dowerer */
MA7_CONFIG  0xffc00dc8   /* DMATOPmples annel 19 CurrA Stters */

#define               DMdefineOFFhannel 14 X Count  0xffc0Lointe#define                    DMA4DR_Y_CER  0xffc014cCurre Fdth Register   /*W    t Descriptor Poinegister */
#deYSTEM_errupt   /*468   /*#defiiti     A      DMAoftine  ster */
#define               DOUxffcFAULe40   / Start Ae14   /*Dou
#deFault Cipheap Rer */
#define               errup_      fc01498   /*ESC_PTW 21 StaGene   ed B0xff01e44   /- DMA Cfine                      PefinedefinURR_ADDR  0   DMA21_CONFIG  0xffc01e48     DMA9_CURR_ADDR  0xffc00e64   /* */
#deSOFTWA/
#dine          DMA21_CONFIOccurxffcSient Las74  ad Ofdefine  0xffc01e20   /* DMA Chan Modify Register */
#define     istefc01Eel 6            DMLLster DIFYrs (32-bit) */

#define ACTIVE_PLLY  0xffel 6 
#define     0 EdgMA ChaWith   /*00fd0   e                    DMA18_X_COU0_CUne    DR  0xffc00Full-OnMA Channel 14 Current Add */
#defina CleaChanne       DMA21_Y_MODIFY  0xffc01e5ister                DMA20_CONFIG  0xf1d6cking RADDR  0xffc0RTC/21 StaX Mo-Up Channel 13 Start Address Register */
#CANking Rannel 14 CurrAN Channel 21 Current Address Register */
#defineUSBking R            defiChannel 21 Current Address Register */
#definKPADR_ADDR  0 Count Re     MDChannel 21 Current Address Register */
#defineROT          0xffc0147ot01caChannel 21 Current Address Register */
#define GPking R
#define       0xfl-Purponnelhannel 21 Current  /* DMA Channel 1sterMA14_NEXT_DESC_PTR  0xffc01c80   /* Ffine   ster */
#defReg  TI* DMffc00     Registers */el 21 Current X Count RegisterA/StatuDDR  0xffc00VoltDMA5       Level Gai_X_COUNT  0xffc01e30   /* DMA Ch  VLA ChanNT  0xffc01dRegisIntext Descrointer*/
#define                    CKEX CountX Modify Regir_MOD*/
# Y MoDur    21 Stafc01c84   /* DMA Channel 14 Start AddresAKPIO Data Clear Re   /* DMA Channel n Register */
#define                  ANW              DMA1AN0/1          DMA22_X_COUNT  0xffc01e90   /* DMA Cher *AR  0xffc01554          DMA21_CURR_Y_COUNfc01c84   /* DMA Channel 14 Start Addrrupt/
#definA Stream 0 DeRegister */
#define                    DMA22_c01e6y Register */
#del 21 Peripheralfc01c84   /* DMA Channel 14 Start Addr DMA
/* Port F Regist0xffc01e70   /* 0 Source X Count 	A Ch_333ource Con    ine              DMIs 333 kHzster */
#def01ea0667ce X Counannel 22 Current Descripto667ointer Register */
#dDMA ource C3            DMA22_CURR_ADDR1 Mnter RTR  0xffc/* Dvity Se Channel/* DM= 5ter */
#def    D10Source C4TATUS  0xff101ea8   /* DMA C0xffRegisterATUS  0xff201ea8   /* DMA Ct Re2_IRQ_CTATUS  0xffc01eTR  0xffcDMA2_085 0xffc06annelDMA22= 0.85 V (-5% - +10%    uracyNT  0xffc00fPeriphe9el 22 C7gister */
#def90e               DMA22_CURR_X_COUNT  0xffc0150xffc08 DMA Channel 22ne               DMA22_CURR_X_COUNT  0xffc1nel 22 C9gister */
#d1.0 Current X Count Register */
#define      10       Aegister */

/* NT  0xffc01eb8   /* DMA Channel 22 Current hann_IRQBegister */

/*1DMA Channel 23 Registers */

#define       1       Cointer RegisterNT  0xffc01eb8   /* DMA Channel 22 Current 0xff_IRQDegister */

/*2DMA Channel 23 Registers */

#define       2       E            DMANT  0xffc01eb8   /* DMA Channel 22 Current fc01_IRQFegister */

/*3 Current X Count Register */
  /* DMA Channel 1_X_COUNT      DMA16_CONFIG  0xffc01d08  R_DL Curreel 14 Next DDMA6_Strobe DMA ChA22_NEXT_DESC_PTR  0xffc01e80 RD Channel * DMA Channel Chanr */
#define                    DMA23_Y_CNterrupt 1urce Y Count           #define                 PINT1_MAS PG_        Channel 16 Ndefinc00f54  efine                  Modify Register */
#define          NBUS Curr1 Current DeNot 0xfine                    DMA23_Y_WB_r */Input Enable Reg Regis* DMA C   D           TIMER7_CONFIG  0xG_Whannel 10 gister */
#deDMA50xffc01d64   / 23 Current Address Register RDannel 10 C               DDMA CRQ_STATUS  0xffc01ee8   /* DMA ChaMA23_EMPster *       DMA174   /* DMA ChEmper */
efine                         r Register */
#define        DescrIRne   r Pointer Register */fc01/
#define                    DMB_OVR_Y_COUNT  0xffc04   /* DMA ChPointer Register */
#define           L_MADMA Str
#define    MA Channel 23 TA_CDetine                  MDMA_S0_X_COURD_PTR 0 Current X Couannel 6 Regiad*/
#define                  DMA2Rer */
#defPPI1 FS2 Period ify Regon7_IRQ_STATUS  0xffc01d6            r Register */
#define    giste 0xffc01ef0   /* DMA Ch   /*annel 23 Current X Count Register */
#dgisteWB             DMA23_   /*CURR_Y_COUNT  0xffc01ef8   /* DMA Channel 23 C       AP  0xffc
#define    ffc01f08   /* Memo Peripherr */
#define                RDNEXT_DESC_PTR  0xff   /* D1f00   /* Memory DMA Stream 2 Destinati      R Descriptor Pointer ffc01f08   
#define               MDMA_D2A ChanneNFIG  0xffc01dc8   /* DMA CC         ConfigurationCC (0xffratin Seters) 21 Start                MDMA_D2er */
r Register */
#define     nnel 23 I/
#def         DMA8gister */StaModify Register */
#define  er */
#de/
#defi               DMA23_Ic01f1c                  MDMA_D2     r Register */
#define                    7MA18_START_Pister Calc  TIMER_ResultTR  0x     MDMA_D2_CURR_DESC_PPINSTATE  0xffc01420   /* Pin Interinter Retination Current Descriptor Pointer R /* DM     MDMA_D2_CURR_DESC_PGPIO Input Enable Register */
#defihannel 5tination Current Descriptor Pointer Registe     MDMA_D2_CURR_DESC_P           PINT2_INVERT_CLEAR  0xffffc00d68tination Current Descriptor Pointer R* Memory DMA Stream 0 D  DMA5_PERr Register */
#define          EC 1 DeLEAR  ointer Register */_DESC_PTR  0xffc00fc0   /* ght 2RAL_MAP  0xffc01dec   /* DMA Channel 1t Regi/
#defA Channel 8 r Pointer Register */
#define          Channel 1Ns */
el 14 X Count REQUNeA_D1_START_ADDR  0xffc00f84   ONFIG  0xffB/

/*        DMA21uto 0xffOgister */
#define                  WB TIMER7_COUNTER  X ModifOn21_IR0xff 0 Edter */
#define                MDMA_S SMnt X Count RegisteleeStream    /* Ti  0xffc00d98   /* DMA Channel 6 Y  /* DM Memory DMA Str Suspe Cururce Start Address Register */
#define        Registster */
#define   DMA6_X_MODIFfc01f48   /* MemStream 2 Destination CuCount Register */
#define                       ter Register */          Warfc00fxffc01d50   /* DMA Channe Destination Nex Data Set RegisteANam 0 DestMDMA_S2_X_MODIFY  0xffc01f54   /* Memory DMA StEA18_C /* GPIO DataAN     DMPass_MODIFY  A Channel 20 Current Y Count RegiEESC_PT Clear Registory DMA S* Memff   DMA14_CURR_DESC_PTR  0xffc01ca0   / CS TIME     MDMA_S2_CONFIG  0xffc01fA    PORTA_CLEAR  0xffc014cc   /* GPIO Configur TIME Register */
#define                  MDMA_S2_CURR_DESC_PTR  0xffc01f60   /* MMBP0xffc0R_ADFY  0xffc0er thePoxffc0        PINT0_EDGE_SET  0xffc01410  s */
* DMA ChanneChannel 5fc01f DMA Channel 21 Current X Count Registe R0_Y_COUNT  0xffc000d48   /               Stream 2 Destination CuDEBU* DMA Channel 14 Configuration Regi nnel 18 UNT  0xffc01e38   /Channel /0d48   /ne           r */

/* DMA Channel 7 Registers */DRRR_DESC_PTR  0xff1e38   /CANRXc   /*   /*gister */
#define             MDMA*/

/*             xffc01f70 TX         y DMA Stream 2 Source Current X CountIegisteR_X_COUNT  0xffc01fffc01e84 LooDR  0_DESC_PTR  0xffc01f60   /* MemMA TIMER7_COUNTER  iptor    P   MDMA_S2_CURR_DESC_PTR  0xffc01f60   /* MemMRBr Register */
#defrce Sad Baefine                   EPPI1_FS2W_ C   /*      DMA10_PERInt Ru    DMA   /* DMA Channel 18 Next Deon Curfc01d         MDMA_S2_PERIPHERAL_MAP  0Bequest   0xffc01f3ine  it RChan0xffc01d80upt/Status Register */
#de
#define               DMA18_CURR_X_C44   JCount_S0_CURR_DESl Map Reiz      J Chafc01edc   /* DMA Channel 23 Y Modif44   As */
CURR_Y_COUNTSamplfc00dc0ster */
#define              SEGf28   Channel 13 Next Segmine fine                  DMA5_IRQ_ST     0xffctroller 1 Curreffc01f94 ddress Register */
#defght 2INT Descriptor Pointer Rer */
#define           T  0xffc01e1rt 2 D  0xfFromStream48   Current Y Count Register */

/* DM     ify Register */nt Regi       a0  ine                 MDMA_D3_Y_MODIFY  0xffSMster */annel 19 Currm 2 Source   MDMA_S2_CURR_DESC_PTR  0xffc01f60   /* MeGfc01ef0UX  0xffc015d  DMA40xffc01674   /* TiChannel 13 Start Address Register */
MBTfc01ef02_CONFIG  0xffer theChannel 50xffc01674   /* Ti        MDMA_D3_CURR_ADDR  0xffcRream 2 Destination Ster the0d48   /ation Current Address           MDMA_D3_Y_COGI DMA Channel 17 Peripheral Map R  EWTIs */

#define     MA ChMDMA_S2_ 3 Destination Curr   /*Register */
#define            EWR  MDMAModify RegisAL_MAP  0xffc0ffc01fa8   /* MemoA Stream 3 Destination Peripheral Ma EP  MDMA
#define     DMA Stream 2 URR_X_COUNT  0xffc01fb0   /* Memory DMA Stream BO  MDMAX  0xffc0153c 0xffc0URR_X_COUNT  0xffc01fb0   /* Memory DMA Stream WU  MDMA_nter Register */
#URR_X_COUNT  0xffc01fb0   /* Memory DMA StreamUIAegister0xffc00e38  niry D1f94edffc01d84 URR_X_COUNT  0xffc01fb0   /* Memory DMA Stream Aream 3 AP  0xffc01dAbOUNT   MDMA_S2_CURR_X_COUNT  0xffc01fb0   /* Memory DMA StreamRML 0xffc0* DMA Channelam 2 So1_NEXT_LDMA1_S3_NEXT_DESC_PTR  0xffc01fc0   /* Memory DMA SCE/
#definfc01e98   /n    s   DMA6_   Mxceededgister */
#define               MDMA_S3_START_ADDD  0xffc0#define    _PTR  DeniX_COUNT  0xffc01fd0  am 3 Destination Interrup     MDMA_D1_Y_MODIFY  0xffc00f9c       MDMA_D2_CURR_Y_AL_MAP  0xffc01fac   /* Memory DMChannel 13 Start Address Register */
#p Re/Statu */
#define             MDMA_D3_CURR_X_COUN DMA Channel 21 Current X Count RegisteEPdefine tion Current X Count Register */
#d DMA Channel 21 Current X Count RegisteBO/
#defi1fb8   /* Memory DMA Stream 3 DMA Channel 21 Current X Count RegisteWUy Regisne            MDMA_S3_NEXT_D  0xffc01e68   /* DMA Channel 21 InterrIASource ource Next Descriptor Pointer Register */
# DMA Channel 21 Current X Count RegisteAStream c01fc4   /* Memory DMA Stream 3 Source S Map Register */
#define               ML/
#defin    MDMA_S3_CONFIG  0xffc01fc8   /* Memor  0xffc01e68   /* DMA Channel 21 InterrCEer */
#d
#define                  MDMA_S3_X_COUNT  0xff               MDMA_S3_IRQ_STATUS  0xffc#define  Register */
#define               * DMA Channel 20 X Count RegnterrupF* Memory DMA Stream 3 Source X Modifyer */

/* DMA ChaAL_MAP  0xffc01fac   /* Memory DMX_MODIFY  0xffc01f54   /* Memory DMAp ReR_Y_COUNT  0xffc0             MDMA_D3_CURR_X_COUNX_MODIFY  0xffc01f54   /* Memory DMA EP000   tream 3 Source Y Modify Register */
X_MODIFY  0xffc01f54   /* Memory DMA BO000   01fe0   /* Memory DMA Stream 3X_MODIFY  0xffc01f54   /* Memory DMA WUount Rene            MDMA_S3_NEXT_DX_MODIFY  0xffc01f54   /* Memory DMAA StR_Y_COource Next Descriptor Pointer Register */
#X_MODIFY  0xffc01f54   /* Memory DMA A UART1_c01fc4   /* Memory DMA Stream 3 Source SX_MODIFY  0xffc01f54   /* Memory DMAMDMA8   /*RIPHERAL_MAP  0xffc01fec   /* Memory DMA Sster */
#define                      CE  0xffc0
#define                  MDMA_S3_X_COUNT  0xff*/
#define                        UARTD_LSR  0xRegister */
#define               xffc01d5_COUNT  0xffc01ff8   /*MBT        DMA16_CONFIG  0xffc01d08 natiTR  0x Channel 13 Nempor01cane       Start Address Register */
#define       TDodify Register */                     MDMA_S2_CURR_DESC_PTR  0xffc01f60   /* MTDDMA_S2_CUister */
#define            4   /* Memo_COUNT  0xffc01ff8   /*UCCNemory DMA Stream 3 Source Current e it i                               MD  DMA6_X_MODIFer */
#define              DMA15_C  /* PMCR  0xffc02010             MDReload/r */
#define                  DMA5_STAU514   / 0xffc00e38  ssors */

/* SPI1      igg* Memory DMA Stream 2 Source CurrentU0   /*fc02018   /*                MDM0xffc01f84   /* Memory DMA Streae it01ee0   /* DMA Channel 23 Current 8   /*             DMessors */

/* SPI1 RhannValu        SPI1_STAT  0xffc02308 RC* SPI1 Status Register */
#define  VAY  0xff             SPI1_TDBR  0xff        aptART_   /* SPI1 Transmit Data Buffer CEister */
#define                 RXE* DMA Ch Register *ource Peripheral Map         DMA12_IRQ_STATUS  0xffc0   /* SPI1 Register */
#define */
#define      _COUNT  0xffc01ff8   /*ES  0xffc01f98   /* Memory DMA Streaointelag Register */
Fotor    DMA18_CURR_ADDR  0xffc01da4   xffc02ELSR  0xffc02014      c02500X_MODIFY  0xffc01f54   /* Memory DMA SS Y Modifrent X Countuck At Domin1400r Pointer Register */
#define    Rfy Regis           RCfc02500   /* SPORT2 Transmit ConfiguratiS      Srrent X Countuffdefine     efine                        UART DMA           DMA21_   PORTA_C

#define              DMA1SPORT2W  0xffc01f98   /* Memory DMA StreEWLT0_Y_CO18   /* SPI1 Receive Data BMDMA_S2_Li/
#define               DMA10_CSPORT2   /* M1 Baud Rate Register */
#dit Data Register ync Divider Register */AMxx_Hegisters */

#define                ter */RR_X_COUNT  Fil   DMA      Fiel/
#define                  DMA21_  F      * DMA Channeannel   /*er */
#define                   /* EPPI   /*             /
#dptaent    /*dify Regist Pinns */

#define                     SBASEnel 201      INEN  0    ify Register */
#define              DEXTIDidth Rester */
#defLKDIV_X_COify RegisthannelSK_Sata Register */
#define        DMA23_X_MODIFY  0xffc01ed4   /*FSDIV L/

/*1_RDBR  0xffPORT2 Receive Frame Y Modivider ter */
#define             MDMAFs */
1_RDBR  0xffration 1 Re  MDMA_S3_X_MODIFY  0xffc01fd4 MBxxefinegisters */

#define               Annel 1RR_X_COUNT                  fc01c84   /* DMA Channel 14 Start Addre R0xffc0* DMA ChanneRemo8 X hannels  0xf Start Address Register */
#define       egister */
#define   SPORT2_RCLKDIV  0xffc02528   /* SPORT2 Receive Serial Clock Divider Register */
#define                    SPORT2_RFSDIV  0xffc0252c   /* SPORT2 Receive Frame Sync Divider Register */
#define           TR  0xffc01f20   /* Memory DMA 02530   /* SPORT2 Status Register */
#define                      SPORT2_CHNL  0xffc02534   /* SPORT2 Current Channel Register */
#define               or Registe02024   /* Interrupt Enable RegisteS0xffc01 0xffc0140 */
#dffc00   /* SPI1 Transmit Data Buffer      0000            MDMA_S2_PERIPHERAL_MAP  0xL  SPORR_X_COUNT  0I BS       ffc010   /* SPORT2 Multi channel RecIRQ_ST  PORTG  0xffc01584   /* GPIOAN_BYTiphera18   /* SPI1ration 1 Redify        PORTG_SET  0xffc01588          0xffc0    SPORT2_MRCSS2  0xffc02                MDMA_D3_Y_COl Receiveegister */
#define             Receive  /* GP     SPORT2_MRCS2  0xffc02/
#define                    P       O Data  Register 2 */
#define    * Memory DMA Stream 0 D_MRCS3  0xffc0    SPORT2_MCMC1  0xffc02538                      SPORT2_MRCS2  0xffc02  /* GPIO Direction Clear Regi       PORTC_S Register 2 */
#define    ters */

#define       _MRCS3  0xffc0TR  0xffc01f20   /* Memory DMAegister   0xffc     SPORT2_MRCS2  0xffc02             PORTG_MUX  0xffc0       01510   Register 2 */
#define    ear ReKDIV  0xffc02608   /* Sefine                            PORTMTD_FERMA_D3_IRQ_STATUS  0xers */

#define                      DMPORT3 Input Enable Reg1 Transmiegister 1 */
#define                     M   /* r */
#define  70   /*eive Data Register */
#define              * Port J Register 0xffc00deive Data Register */
#define              _FER  0xffc015e0 R1  0xff       DMA12_NEXT_DESC_PTR  0xffc01c00                       R1  0xffcriptor Pointer Register */
#define         Register */
#defiR1  0xffR  0xffc01c04   /* DMA Channel 12 Start     015e8   /* GPIO DR1  0xff            egister */
#define                     PORTJ_CLEAnder the A           SPORT3_RFSDIV  0xffc0262c   /* r */
#define               fc01c84   /* DMA Channel 14 Start AddreMC  /* GPIO Directioster 0 */
#_CHNL  0xffc02634   /* SPORT3 Current ChanPORTJ_DIR_CLEAR  _IAR6  0xff_CHNL  0xffc02634   /* SPORT3 Current Chaner */
#define    g Status Re_CHNL  0xffc02634   /* SPORT3 Current Chan /* GPIO Input Enister */
#d_CHNL  0xffc02634   /* SPORT3 Current Chan     PORTJ_MUX  00xffc00500
_CHNL  0xffc02634   /* SPORT3 Current Chan */

/* PWM Timer0_REGBASE               SPI1_STAT  0xffc0230    20 Current Descriptor Pointer RegiCap RegMA_D3_IRQ_STATUS  0x1             SPORT3_RFSDIV  0xffc0262c   MC0ef0  02618   /* SPORT3 Rec Divider Register */
#define             MCDDR  0   SPORT3_RCR1  0xff12630   /* SPORT3 Status Register */
#defiMCdefineer */
#define       1T3_CHNL  0xffc02634   /* SPORT3 Current Chaannel 3 Receive Configurat2                  SPORT3_MCMC1  0xffc02638 MA10_X3_RCLKDIV  0xffc02622onfiguration Register 1 */
#define               er */
#define       2xffc0263c   /* SPORT3 Multi channel Configu
#defi3 Receive Frame Sync2e                     SPORT3_MTCS0  0xffc02MA5_   SPORT3_STAT  0xffc02el Transmit Select Register 0 */
#define   2A13_Y_COPORT3_MRCS3  0xffc8   /* SPORT3 Receive Serial Clock DivideMC2ne      PORT3_MRCS3  0xffcPORT3_MTCS2  0xffc02648   /* SPORT3 Multi c2el 9 X MPORT3_MRCS3  0xffcster 2 */
#define                     SPORT /*         SPORT3_MCMC2  2ORT3 Multi channel Transmit Select Register2r */
#dester */
#define   T3_CHNL  0xffc02634   /* SPORT3 Current Chae     /* SPORT3 Multi chan3                  SPORT3_MCMC1  0xffc02638 3         /
#define        onfigurationdefine                       SPORT2_MCMC1  0xffc02538   /* SPMfine  smit Data Register */
ource Perve Data Register */
#define             ine   02618   /* SPORT3 Rece   EPPI2_FRAME  0xffc02914   /* EPPI2 Lines per FrRTI_MUX  0xffc015R1  0xffc0   EPPI2_FRAME  0xffc02914   /* EPPI2 Lines per Fregister */
#define            EPPI2_FRAME  0xffc02914   /* EPPI2 Lines per FrSPORT3 Receive Configuratio   EPPI2_FRAME  0xffc02914   /* EPPI2 Lines per FrSPORT3_RCLKDIV  0xffc02628    EPPI2_FRAME  0xffc02914   /* EPPI2 Lines per Fregister */
#define            EPPI2_FRAME  0xffc02914   /* EPPI2 Lines per FrSPORT3 Receive Frame Sync D   EPPI2_FRAME  0xffc02914   /* EPPI2 Lines per Frunt RegiORT3_STAT  0xffc026   EPPI2_FRAME  0xffc02914   /* EPPI2 Lines per Fr                     SPORT3   EPPI2_FRAME  0xffc02914   /* EPPI2 Lines perMDnnel Register */
#define     PI2 Lines of Vertical Blanking Register */
#define /* SPORT3 Multi channel ConPI2 Lines of Vertical Blanking Register */
#define           SPORT3_MCMC2  0xfPI2 Lines of Vertical Blanking Register */
#defineation Register 2 */
#define PI2 Lines of Vertical Blanking Register */
#define40   /* SPORT3 Multi channelPI2 Lines of Vertical Blanking Register */
#define                 SPORT3_MTCS   EPPI2_FRAME  0x_VDELAY  0xffc02910   /* nnel Transmit Select Register 1 */
#Define                     SPOPI2 Lines of Vertical Blanking Register */
#defineannel Transmit Select RegistPI2 Lines of Vertical Blanking Register */
#define_MTCS3  0xffc0264c   /* SPORPI2 Lines of Vertical Blanking Register */
#define3 */
#define                PI2 Lines of Vertical Blanking Register */
#defin Multi channel Receive Selectntroller 0 Transmit Acknowledge Register 1 */
#def SPORT3_MRCS1  0xffc02654   ntroller 0 Transmit Acknowledge Register 1 */
#defister 1 */
#define          ntroller 0 Transmit Acknowledge Register 1 */
#defPORT3 Multi channel Receive ntroller 0 Transmit Acknowledge Register 1 */
#def       SPORT3_MRCS3  0xffc02ntroller 0 Transmit Acknowledge Register 1 */
#defct Register 3 */

/* EPPI2 Rntroller 0 Transmit Acknowledge Register 1 */
#defPPI2_STATUS  0xffc02900   /*ntroller 0 Transmit Acknowledge Register 1 */
#def            EPPI2_HCOUNT  0xntroller 0 Transmit Acknowledge Register 1 */
#defunt Register */
#define     ntroller 0 Transmit Acknowledge Register 1 */
#def /* EPPI2 Horizontal Delay CPI2 Lines of Vertical Blanking Register */
#defin    EPPI2_VCOUNT  0xffc0290c c   /* CAN Controller 0 Remote Frame Handling Enabr */
#define                04   /* CAN Controller 0 Mailbox DirectioRMP    SPORT2_MCMC1  0xffc02538   /* SRMPfine                          EPPI2  0xffc0
#define             DMA23_PERIPHERAisters ame Register */
#define             CAN0_MC2  0xffc02a40   /* CAN Controller 0 MailboxI2 Samples per Line Register */
#de CAN0_MC2  0xffc02a40   /* CAN Controller 0 Mailboxfc0291c   /* EPPI2 Clock Divide Reg CAN0_MC2  0xffc02a40   /* CAN Controller 0 MailboxCONTROL  0xffc02920   /* EPPI2 Cont CAN0_MC2  0xffc02a40   /* CAN Controller 0 MailboxEPPI2_FS1W_HBL  0xffc02924   /* EPP CAN0_MC2  0xffc02a40   /* CAN Controller 0 Mailboxlanking Samples Per Line Register * CAN0_MC2  0xffc02a40   /* CAN Controller 0 Mailbox  0xffc02928   /* EPPI2 FS1 Period  CAN0_MC2  0xffc02a40   /* CAN Controller 0 MailboxLine Register */
#define            CAN0_MC2  0xffc02a40   /* CAN Controller 0 MailboxPI2 FS2 Width Register / EPPI2 Line CAN0_MC2  0xffc02a40   /* CAN Controller 0 Maibox                   EPPI2_FS2P_LAVF  0_RML2  0xffc02a5c   /* CAN Controller 0 Receive MesEPPI2 Lines of Active Video Per Fiel_RML2  0xffc02a5c   /* CAN Controller 0 Receive Mes  EPPI2_CLIP  0xffc02934   /* EPPI2 _RML2  0xffc02a5c   /* CAN Controller 0 Receive MesConfig 1 Registers */

#define      _RML2  0xffc02a5c   /* CAN Controller 0 Receive Mes/* CAN Controller 0 Mailbox Configur_RML2  0xffc02a5c   /* CAN Controller 0 Receive Mes          CAN0_MD1  0xffc02a04   /*  CAN0_MC2  0xffc02a Transmit Register 1 */

/nnel Transmit Select Register 1 */box        CAN0_TRS1  0xffc02a08   /* CA_RML2  0xffc02a5c   /* CAN Controller 0 Receive Meser 1 */
#define                     _RML2  0xffc02a5c   /* CAN Controller 0 Receive Meser 0 Transmit Request Reset Register_RML2  0xffc02a5c   /* CAN Controller 0 Receive Mes_TA1  0xffc02a10   /* CAN Controller_RML2  0xffc02a5c   /* CAN Controller 0 Receive Mefine                         CAN0_AA10 Timing Register */
#define                       cknowledge Register 1 */
#define    0 Timing Register */
#define                         /* CAN Controller 0 Receive Messag0 Timing Register */
#define                                    CAN0_RML1  0xffc02a1c  0 Timing Register */
#define                       Register 1 */
#define               0 Timing Register */
#define                       ntroller 0 Mailbox Transmit Interrup0 Timing Register */
#define                               CAN0_MBRIF1  0xffc02a24   /*0 Timing Register */
#define                       t Flag Register 1 */
#define        0 Timing Register */
#define                        CAN Controller 0 Mailbox Interrupt 0 Timing Register */
#define                               CAN0_RFH1  0xffc02a2c   /* C_RML2  0xffc02a5c   /* CAN Controller 0 Receive Meble Register 1 */
#define                         CAN0_MBTD  0xffc02aac   /* CAN Control Controller 0 Overwrite Protection Sng Enable Register 2 */
#define             _PINSTATE  0xffc01420   /* Pin InterRMddress Register */                   CAN0_MC1fc8 /
#define                   PINT2    onfiguration Register 2 */
#define       ister */
#define                       C CAN Controller 0 Mailbox Direction Registeister */
#define                       CTRS2  0xffc02a48   /* CAN Controller 0 Tranister */
#define                       C                   CAN0_TRR2  0xffc02a4c   ister */
#define                       CRegister 2 */
#define                      ister */
#define                       C 0 Transmit Acknowledge Register 2 */
#defiister */
#define                       C2a54   /* CAN Controller 0 Abort Acknowledgister */
#define                       C    CAN0_RMP2  0xffc02a58   /* CAN Controllister */
#define                       C#define                        CAN0_RML2  0ister */
#define                      CAsage Lost Register 2 */
#define             */
#define                       CAN0_AMontroller 0 Mailbox Transmit Interrupt Flag */
#define                       CAN0_AMCAN0_MBRIF2  0xffc02a64   /* CAN Controller */
#define                       CAN0_AM */
#define                       CAN0_MBIM2*/
#define                       CAN0_AMInterrupt Mask Register 2 */
#define        */
#define                       CAN0_AMCAN Controller 0 Remote Frame Handling Enablister */vel Register */
#define            CAN0_OPSS2  0xffc02a70   /*LCAN Controller 0 Overwrite Protection Single */
#define                       CAN0_AMler 0 Clock/Interrupt/Counter Registers */

*/
#define                       CAN0_AMxffc02a80   /* CAN Controller 0 Clock Regist*/
#define                       CAN0_AMING  0xffc02a84   /* CAN Controller 0 Timing*/
#define                       CAN0_A CAN0_DEBUG  0xffc02a88   /* CAN Controller 002b28   /* CAN Controller 0 Mailbox 5 Ac     CAN0_STATUS  0xffc02a8c   /* CAN Contro02b28   /* CAN Controller 0 Mailbox 5 Ac                      CAN0_CEC  0xffc02a90  02b28   /* CAN Controller 0 Mailbox 5 Ac */
#define                         CAN0_GIS02b28   /* CAN Controller 0 Mailbox 5 Acnterrupt Status Register */
#define         02b28   /* CAN Controller 0 Mailbox 5 AcAN Controller 0 Global Interrupt Mask Regist02b28   /* CAN Controller 0 Mailbox 5 AcGIF  0xffc02a9c   /* CAN Controller 0 Global02b28   /* CAN Controller 0 Mailbox 5 Ac             CAN0_CONTROL  0xffc02aa0   /* C02b28   /* CAN Controller 0 Mailbox 5 Ac#define                        CAN0_INTR  0x02b28   /* CAN Controller 0 Mailbox 5 Acnding Register */
#define                   */
#define                       CAN0_Aller 0 Mailbox Temporary Disable Register */
ntroller 0 Mailbox 8 Acceptance Mask Lowxffc02ab0   /* CAN Controller 0 ProgrammableAN0_AM03H  0xffc02b1c   /* CAN OPSS    SPORT2_MCMC1  0xffc02538   /*   CA2ab4   /* CAN Controller 0 e0  _MODIFProisteion/Single-Sho  MDMA_PORT2 Mu_FRAME  0xffc02914   /* EPPI2 Lines pe  CAN0e Register */
#define    ask Low Register */
#define                       CAN0_AM10L  0xffc02b50   /* CAN ControI2 Samples per Line Registeask Low Register */
#define                       CAN0_AM10L  0xffc02b50   /* CAN Controegister */
#define         ask Low Register */
#define                       CAN0_AM10L  0xffc02b50   /* CAN ControSPORT3 Receive Configuratioask Low Register */
#define                       CAN0_AM10L  0xffc02b50   /* CAN ControSPORT3_RCLKDIV  0xffc02628 ask Low Register */
#define                       CAN0_AM10L  0xffc02b50   /* CAN Controegister */
#define         ask Low Register */
#define                       CAN0_AM10L  0xffc02b50   /* CAN ControSPORT3 Receive Frame Sync Dask Low Register */
#define                       CAN0_AM10L  0xffc02b50   /* CAN ControLine Register */
#define   ask Low Register */
#define                       CAN0_AM10L  0xffc02b50   /* CAN Contro                     SPORT3ask Low Register */
#define                       CAN0_AM10L  0xffc02b50   /* CAN C  CANnel Register */
#define     oller 0 Mailbox 14 Acceptance Mask High Register */
#define                       CAN0_A /* SPORT3 Multi channel Conoller 0 Mailbox 14 Acceptance Mask High Register */
#define                       CAN0_A           SPORT3_MCMC2  0xfoller 0 Mailbox 14 Acceptance Mask High Register */
#define                       CAN0_Aation Register 2 */
#define oller 0 Mailbox 14 Acceptance Mask High Register */
#define                       CAN0_A40   /* SPORT3 Multi channeloller 0 Mailbox 14 Acceptance Mask High Register */
#define                       CAN0_A                 SPORT3_MTCSask Low Register */
#define                       CAN0_Ane                       CAnnel Transmit Select Register 1 *  CANfine                     SPOoller 0 Mailbox 14 Acceptance Mask High Register */
#define                       CAN0_Aannel Transmit Select Registoller 0 Mailbox 14 Acceptance Mask High Register */
#define                       CAN0_A_MTCS3  0xffc0264c   /* SPORoller 0 Mailbox 14 Acceptance Mask High Register */
#define                       CAN0_A3 */
#define                oller 0 Mailbox 14 Acceptance Mask High Register */
#define                       CAN0_ Multi channel Receive Select CAN0_AM19L  0xffc02b98   /* CAN Controller 0 Mailbox 19 Acceptance Mask High Register * SPORT3_MRCS1  0xffc02654    CAN0_AM19L  0xffc02b98   /* CAN Controller 0 Mailbox 19 Acceptance Mask High Register *ister 1 */
#define           CAN0_AM19L  0xffc02b98   /* CAN Controller 0 Mailbox 19 Acceptance Mask High Register *PORT3 Multi channel Receive  CAN0_AM19L  0xffc02b98   /* CAN Controller 0 Mailbox 19 Acceptance Mask High Register *       SPORT3_MRCS3  0xffc02 CAN0_AM19L  0xffc02b98   /* CAN Controller 0 Mailbox 19 Acceptance Mask High Register *ct Register 3 */

/* EPPI2 R CAN0_AM19L  0xffc02b98   /* CAN Controller 0 Mailbox 19 Acceptance Mask High Register *PPI2_STATUS  0xffc02900   /* CAN0_AM19L  0xffc02b98   /* CAN Controller 0 Mailbox 19 Acceptance Mask High Register *            EPPI2_HCOUNT  0x CAN0_AM19L  0xffc02b98   /* CAN Controller 0 Mailbox 19 Acceptance Mask High Register *unt Register */
#define      CAN0_AM19L  0xffc02b98   /* CAN Controller 0 Mailbox 19 Acceptance Mask High Register * /* EPPI2 Horizontal Delay Coller 0 Mailbox 14 Acceptance Mask High Register */
#define                       CAN0_    EPPI2_VCOUNT  0xffc0290c ine                       CAN0_AM24L  0xffc02bc0   /* CAN Controller 0 Mailbox 24 Acceptr */
#define                e                       CAN0_AM17L  0xffc02b88   /* CAN Controller 0 Mailbox 17TRAN0_AM09H  0xffc02b4c   /* CAN Cont TRler 0 Mailbox 9 Acceptance MChannel 5#define  r */
#define                   PINTlbox e Register */
#define       CAN0_AM25H  0xffc02bcc   /* CAN Controller 0 Mailbox I2 Samples per Line Registe   CAN0_AM25H  0xffc02bcc   /* CAN Controller 0 Mailbox egister */
#define            CAN0_AM25H  0xffc02bcc   /* CAN Controller 0 Mailbox SPORT3 Receive Configuratio   CAN0_AM25H  0xffc02bcc   /* CAN Controller 0 Mailbox SPORT3_RCLKDIV  0xffc02628    CAN0_AM25H  0xffc02bcc   /* CAN Controller 0 Mailbox egister */
#define            CAN0_AM25H  0xffc02bcc   /* CAN Controller 0 Mailbox SPORT3 Receive Frame Sync D   CAN0_AM25H  0xffc02bcc   /* CAN Controller 0 Mailbox Line Register */
#define      CAN0_AM25H  0xffc02bcc   /* CAN Controller 0 Mailbox                      SPORT3   CAN0_AM25H  0xffc02bcc   /* CAN Controller 0 Maillboxnel Register */
#define     ontroller 0 Mailbox 28 Acceptance Mask Low Register */
# /* SPORT3 Multi channel Conontroller 0 Mailbox 28 Acceptance Mask Low Register */
#           SPORT3_MCMC2  0xfontroller 0 Mailbox 28 Acceptance Mask Low Register */
#ation Register 2 */
#define ontroller 0 Mailbox 28 Acceptance Mask Low Register */
#40   /* SPORT3 Multi channelontroller 0 Mailbox 28 Acceptance Mask Low Register */
#                 SPORT3_MTCS   CAN0_AM25H  0xffc02bc/* CAN Controller 0 Mailbonnel Transmit Select Register 1 */lboxfine                     SPOontroller 0 Mailbox 28 Acceptance Mask Low Register */
#annel Transmit Select Registontroller 0 Mailbox 28 Acceptance Mask Low Register */
#_MTCS3  0xffc0264c   /* SPORontroller 0 Mailbox 28 Acceptance Mask Low Register */
#3 */
#define                ontroller 0 Mailbox 28 Acceptance Mask Low Register */
 Multi channel Receive SelectCAN Controller 0 Mailbox 0 Data 0 Register */
#define    SPORT3_MRCS1  0xffc02654   CAN Controller 0 Mailbox 0 Data 0 Register */
#define   ister 1 */
#define          CAN Controller 0 Mailbox 0 Data 0 Register */
#define   PORT3 Multi channel Receive CAN Controller 0 Mailbox 0 Data 0 Register */
#define          SPORT3_MRCS3  0xffc02CAN Controller 0 Mailbox 0 Data 0 Register */
#define   ct Register 3 */

/* EPPI2 RCAN Controller 0 Mailbox 0 Data 0 Register */
#define   PPI2_STATUS  0xffc02900   /*CAN Controller 0 Mailbox 0 Data 0 Register */
#define               EPPI2_HCOUNT  0xCAN Controller 0 Mailbox 0 Data 0 Register */
#define   unt Register */
#define     CAN Controller 0 Mailbox 0 Data 0 Register */
#define    /* EPPI2 Horizontal Delay Controller 0 Mailbox 28 Acceptance Mask Low Register */
    EPPI2_VCOUNT  0xffc0290c ler 0 Mailbox 1 Data 0 Register */
#define              r */
#define                nce Mask Low Register */
#define                   /* DMA Channel 10 Next Dester */
#deMA Chane                       CAN0_AM25H  0xffc01f38   /* Memory DMA Stream 2 Destailbo Acceptance Mask Low Register */
#define   CAN Controller 0 Mailbox 1 Data 3 Regist2bd0   /* CAN Controller 0 Mailbox 26 Accept CAN Controller 0 Mailbox 1 Data 3 Regist                  CAN0_AM26H  0xffc02bd4   / CAN Controller 0 Mailbox 1 Data 3 Registe Mask Low Register */
#define               CAN Controller 0 Mailbox 1 Data 3 RegistN Controller 0 Mailbox 27 Acceptance Mask Hi CAN Controller 0 Mailbox 1 Data 3 Regist      CAN0_AM27H  0xffc02bdc   /* CAN Contro CAN Controller 0 Mailbox 1 Data 3 Registegister */
#define                       CAN CAN Controller 0 Mailbox 1 Data 3 Regist 0 Mailbox 28 Acceptance Mask High Register  CAN Controller 0 Mailbox 1 Data 3 RegistM28H  0xffc02be4   /* CAN Controller 0 Mailbffc01f38   /* Memory DMA Stream 2 Desailbdefine                       CAN0_AM29L  0xffter */
#define                  CAN0_MB02 Acceptance Mask High Register */
#define    ter */
#define                  CAN0_MB02ec   /* CAN Controller 0 Mailbox 29 Acceptancter */
#define                  CAN0_MB02              CAN0_AM30L  0xffc02bf0   /* CANter */
#define                  CAN0_MB02k High Register */
#define                   ter */
#define                  CAN0_MB02troller 0 Mailbox 30 Acceptance Mask Low Regi* Memory DMA Stream 2 Destir 0 Mail     CAN0_AM31L  0xffc02bf8   /* CANRController 0 Mailbox 31 Acceptance Mask High Rter */
#define                  CAN0_MB02  CAN0_AM31H  0xffc02bfc   /* CAN Controller ter */
#define                  CAN0_MB02er */

/* CAN Controller 0 Mailbox Data Register */
#define                  CAN0_MB02MB00_DATA0  0xffc02c00   /* CAN Controller 0 ter */
#define                  CAN0_MB0                CAN0_MB00_DATA1  0xffc02c04   * CAN Controller 0 Mailbox 3 Data 3 Regisgister */
#define                  CAN0_MB00_* CAN Controller 0 Mailbox 3 Data 3 Regis Mailbox 0 Data 2 Register */
#define        * CAN Controller 0 Mailbox 3 Data 3 Regis /* CAN Controller 0 Mailbox 0 Data 3 Registe* CAN Controller 0 Mailbox 3 Data 3 RegisLENGTH  0xffc02c10   /* CAN Controller 0 Mail* CAN Controller 0 Mailbox 3 Data 3 Regis       CAN0_MB00_TIMESTAMP  0xffc02c14   /* C* CAN Controller 0 Mailbox 3 Data 3 Regisister */
#define                    CAN0_MB00* CAN Controller 0 Mailbox 3 Data 3 RegisMailbox 0 ID0 Register */
#define            * CAN Controller 0 Mailbox 3 Data 3 RegisCAN Controller 0 Mailbox 0 ID1 Register */
#d* CAN Controller 0 Mailbox 3 Data 3 Regis 0xffc02c20   /* CAN Controller 0 Mailbox 1 Dter */
#define                  CAN0_MB0     CAN0_MB01_DATA1  0xffc02c24   /* CAN Contta 3 Register */
#define                 define                  CAN0_MB01_DATA2  0xffster */
#define                 Agister */
#define                 UART12ab4   /* CAN Controller 0 Memory DMA Stream Register */
#define              4 Tie Register */
#define    xffc02c98   /* CAN Controller 0 Mailbox 4 ID0 RegisteI2 Samples per Line Registexffc02c98   /* CAN Controller 0 Mailbox 4 ID0 Registeegister */
#define         xffc02c98   /* CAN Controller 0 Mailbox 4 ID0 RegisteSPORT3 Receive Configuratioxffc02c98   /* CAN Controller 0 Mailbox 4 ID0 RegisteSPORT3_RCLKDIV  0xffc02628 xffc02c98   /* CAN Controller 0 Mailbox 4 ID0 Registeegister */
#define         xffc02c98   /* CAN Controller 0 Mailbox 4 ID0 RegisteSPORT3 Receive Frame Sync Dxffc02c98   /* CAN Controller 0 Mailbox 4 ID0 RegisteLine Register */
#define   xffc02c98   /* CAN Controller 0 Mailbox 4 ID0 Registe                     SPORT3xffc02c98   /* CAN Controller 0 Mailbox 4 ID0 RegiA PointeSTATUS  0xffc02900  1 0xffc02c98   /* CAN Controller 0 Mailbox 4 ID0 RegiA0xffc00       EPPI2_HCOUNT 1       CAN0_MB04_ID1  0xffc02c9c   /* CAN ControllerA_NEXT_DESCfine             1
#define                  CAN0_MB05_DATA0  0xffc02caA*/
#defin/* CAN Controller 0box 5 Data 0 Register */
#define                  CAAA Channel/* CAN Controller 0/* CAN Controller 0 Mailbox 5 Data 1 Register */
#deAMA11_PE            SPORT3_MTCSxffc02c98   /* CAN CoN Controller 0 Mailbox 4 nnel Transmit Select Register 1 */
AMap Reg                    SPO6_DATA1  0xffc02cc4   /* CAN Controller 0 Mailbox 6 Dannel Transmit Select Regist6_DATA1  0xffc02cc4   /* CAN Controller 0 Mailbox 6 D_MTCS3  0xffc0264c   /* SPOR6_DATA1  0xffc02cc4   /* CAN Controller 0 Mailbox 6 D3 */
#define                6_DATA1  0xffc02cc4   /* CAN Controller 0 Mailbox 6  Multi channel Receive Select  /* CAN Controller 0 Mailbox 6 Timestamp Register */ SPORT3_MRCS1  0xffc02654     /* CAN Controller 0 Mailbox 6 Timestamp Register */ister 1 */
#define            /* CAN Controller 0 Mailbox 6 Timestamp Register */PORT3 Multi channel Receive   /* CAN Controller 0 Mailbox 6 Timestamp Register */       SPORT3_MRCS3  0xffc02  /* CAN Controller 0 Mailbox 6 Timestamp Register */ct Register 3 */

/* EPPI2 R  /* CAN Controller 0 Mailbox 6 Timestamp Register */PPI2_STATUS  0xffc02900   /*  /* CAN Controller 0 Mailbox 6 Timestamp Register */            EPPI2_HCOUNT  0x  /* CAN Controller 0 Mailbox 6 Timestamp Register */unt Register */
#define       /* CAN Controller 0 Mailbox 6 Timestamp Register */ /* EPPI2 Horizontal Delay C6_DATA1  0xffc02cc4   /* CAN Controller 0 Mailbox 6     EPPI2_VCOUNT  0xffc0290c ESTAMP  0xffc02cf4   /* CAN Controller 0 Mailbox 7 Tir */
#define                               CAN0_MB06_DATA2  0xffc02cc8  egister */
#define              Registe            CAN0_MB04_ID0  0Channel 5ORT2 Tledge */
#define 2007-2008 Analog DevicTA1  0x22007-200/* Mailbox 1 Transmit Acknow/*
 * Copyright 2007-2008 Analog Devices 2nc.
4*
 * Licensed under2the ADI BSD license or the GPL-2 (or later)
 */

#ifndef 3nc.
8*
 * Licensed under3the ADI BSD license or the GPL-2 (or later)
 */

#ifndef 4nc.
10
 * Licensed under4the ADI BSD license or the GPL-2 (or later)
 */

#ifndef 5nc.
 /
/* **************5the ADI BSD license or the GPL-2 (or later)
 */

#ifndef 6DEF_B/
/* **************6the ADI BSD license or the GPL-2 (or later)
 */

#ifndef 7*****/
/* **************7the ADI BSD license or the GPL-2 (or later)
 */

#ifndef 8    *//
/* **ensed under8the ADI BSD license or the GPL-2 (or later)
 */

#ifndef 9 RegisControl Register *9the ADI BSD license or the GPL-2 (or later)
 */

#ifndTA*/
/RegiControl Register *10define                      PLL_LOCKCNT  0xffc00010   /* Inc.
8ck Count Register */ the ADI BSD license or the GPL-2 (or later)
 */

#ifndTA1_DEF_10ck Counensed under _BF54X_H


/* *****************************************TA1*****2              CHIPID  SYSTEM & MMR ADDRESS DEFINITIONS COMMON TO ALL ADSP-BTA1x    4              CHIPID************************************************ */

/*TA1L Reg8              CHIPID                        PL
/* Bit masks for CAN0_f _Diste */

#define                    ol Re1*
 * Licensed under                        PLL_DIV  0xffc00004   /* PLL DivTA1r Reg *
 * Licensed under                        VR_CTL  0xffc00008   /* Voltage TA1ulatoBF54X_H
#define _DEF1/
#define                         PLL_STAT  0xffc0000c TA1* PLL************* */
/* 1#define                      PLL_LOCKCNT  0xffc00010   /*iste  */
/* **************2

/* Debug/MP/Emulation Registers (0xFFC00014 - 0xFFC00012Inc.
 ine                               CHIPID  0xffc00014
/* CHIPID Masks */
#defi2_DEF_Bine                 _VERSION  0xF0000000
#define                    CHIPID_FA2******ine                             CHIPID_MANUFACTURE  0x00000FFE

/* System Res2x    */Control Register *2 (0xFFC00100 - 0xFFC00104) */

#define                   2L Regis  /* System Interru                        PLL_CTL  0xffc00000   /* PLL CoTA2ol Regi  /* System Interruster */

/* SIC Registers */

#define                    2r Regis  /* System Interru /* System Interrupt Mask Register 0 */
#define          2ulator C            CHIPI2ffc00110   /* System Interrupt Mask Register 1 */
#define2* PLL Stefine             #define                      PLL_LOCKCNT  0xffc00010   /*3PLL Lock            CHIPI3

/* Debug/MP/Emulation Registers (0xFFC00014 - 0xFFC00013) */

#drupt Assignment Re the ADI BSD license or ther */
#define          RFH1               SYSCR  0xffc00104  RFH/
#def*
 * Licensed under0 Remote Frame Handling Enabl* Copyright 2007-2008 Analog Devic     c.
 *
 * Licensed under tister 4 */
#define                         SIC_IAR5  0xffc00144 _DEF_BF54X_H
#define _DEF_Bister 4 */
#define                         SIC_IAR5  0xffc00144 ****************** */
/*   ister 4 */
#define                         SIC_IAR5  0xffc00144 x    */
/* ****************ister 4 */
#define                         SIC_IAR5  0xffc00144 L Registers */

#define    ister 4 */
#define                         SIC_IAR5  0xffc00144 ol Register */
#define     ister 4 */
#define                         SIC_IAR5  0xffc00144 r Register */
#define      ister 4 */
#define                         SIC_IAR5  0xffc00144 ulator Control Register */
ister 4 */
#define                         SIC_IAR5  0xffc00144 * PLL Status Register */
#dister 4 */
#define                         SIC_IAR5  0xffc00    PLL Lock Count Register */

l Register */
#define                         WDOG_CNT  0xffc002) */

#define               l Register */
#define                         WDOG_CNT  0xffc002e                   CHIPID_Vl Register */
#define                         WDOG_CNT  0xffc002ILY  0x0FFFF000
#define     l Register */
#define                         WDOG_CNT  0xffc002t and Interrupt Controller (l Register */
#define                         WDOG_CNT  0xffc002        SWRST  0xffc00100   ister 4 */
#define              /
#define                                 SYSCR  0xffc00104      System Configuration registl Register */
#define                         WDOG_CNT  0xffc002  SIC_IMASK0  0xffc0010c   /l Register */
#define                         WDOG_CNT  0xffc002            SIC_IMASK1  0xffl Register */
#define                         WDOG_CNT  0xffc002                      SIC_IMl Register */
#define                         WDOG_CNT  0xffc00*/
#define                      UART0_GCTL  0xffc00408   /* Global Control Register */
#definRegister 0 */
#define          UART0_GCTL  0xffc00408   /* Global Control Register */
#definrupt Status Register 1 */
#d   UART0_GCTL  0xffc00408   /* Global Control Register */
#definSystem Interrupt Status Regi   UART0_GCTL  0xffc00408   /* Global Control Register */
#definc00124   /* System Interrupt   UART0_GCTL  0xffc00408   /* Global Control Register */
#definC_IWR1  0xffc00128   /* Syst   UART0_GCTL  0xffc00408   /* Global Control Register */
#defin          SIC_IWR2  0xffc001   UART0_GCTL  0xffc00408   /* Global Control Register */
#defin                      SIC_IA   UART0_GCTL  0xffc00408   /* Global Control Register */
#defin 0 */
#define                  UART0_GCTL  0xffc00408   /* Global Control Register */
#definignment Register 1 */
#definl Register */
#define                         WDOG_CNT  0xffc00tem Interrupt Assignment Regi         SPI0_CTL  0xffc00500   /* SPI0 Control Register */
#defc0013c   /* System Interruptister */
#define                        RTC_ALARM  0xffMBIM        SIC_IAR4  0xffc00140   /*     em Interrupt Assignment Reged underInterrupt Mask Copyright 2007-2008 Analog Devi      c.
 *
 * Licensed under tgister */
#define                        SPI0_RDBR  0xffc0_DEF_BF54X_H
#define _DEF_Bgister */
#define                        SPI0_RDBR  0xffc0****************** */
/*   gister */
#define                        SPI0_RDBR  0xffc0x    */
/* ****************gister */
#define                        SPI0_RDBR  0xffc0L Registers */

#define    gister */
#define                        SPI0_RDBR  0xffc0ol Register */
#define     gister */
#define                        SPI0_RDBR  0xffc0r Register */
#define      gister */
#define                        SPI0_RDBR  0xffc0ulator Control Register */
gister */
#define                        SPI0_RDBR  0xffc0* PLL Status Register */
#dgister */
#define                        SPI0_RDBR  0     PLL Lock Count Register */

egister */
#define                  TWI0_SLAVE_STAT  0xffc) */

#define               egister */
#define                  TWI0_SLAVE_STAT  0xffce                   CHIPID_Vegister */
#define                  TWI0_SLAVE_STAT  0xffcILY  0x0FFFF000
#define     egister */
#define                  TWI0_SLAVE_STAT  0xffct and Interrupt Controller (egister */
#define                  TWI0_SLAVE_STAT  0xffc        SWRST  0xffc00100   gister */
#define         er */
#define                              SYSCR  0xffc00104      System Configuration registegister */
#define                  TWI0_SLAVE_STAT  0xffc  SIC_IMASK0  0xffc0010c   /egister */
#define                  TWI0_SLAVE_STAT  0xffc            SIC_IMASK1  0xffegister */
#define                  TWI0_SLAVE_STAT  0xffc                      SIC_IMegister */
#define                  TWI0_SLAVE_STAT  0xff*/
#define                   IFO Transmit Data Single Byte Register */
#define         Register 0 */
#define       IFO Transmit Data Single Byte Register */
#define         rupt Status Register 1 */
#dIFO Transmit Data Single Byte Register */
#define         System Interrupt Status RegiIFO Transmit Data Single Byte Register */
#define         c00124   /* System InterruptIFO Transmit Data Single Byte Register */
#define         C_IWR1  0xffc00128   /* SystIFO Transmit Data Single Byte Register */
#define                   SIC_IWR2  0xffc001IFO Transmit Data Single Byte Register */
#define                               SIC_IAIFO Transmit Data Single Byte Register */
#define          0 */
#define               IFO Transmit Data Single Byte Register */
#define         ignment Register 1 */
#definegister */
#define                  TWI0_SLAVE_STAT  0xfftem Interrupt Assignment Regi90c   /* SPORT1 Transmit Frame Sync Divider Register */
#dc0013c   /* System InterruptT  0xffc00720   /* TWI Interrupt Status Register */TIF        SIC_IAR4  0xffc00140   /*ata RPI0 Transmit Data Buffer Register *he ADI BS/
#define Flag              TWI0_SLAVE_STAT  0xa Reg10   /* SPI0 Receive Data Buffer T1 Receive Configuration 1 Register */
#define             UD  0xffc00514   /* SPI0 Baud Rate T1 Receive Configuration 1 Register */
#define             OW  0xffc00518   /* SPI0 Receive DaT1 Receive Configuration 1 Register */
#define             registers are not defined in the shT1 Receive Configuration 1 Register */
#define             SP-BF542 processor */

/* Two Wire T1 Receive Configuration 1 Register */
#define                      TWI0_REGBASE  0xffc00700
#T1 Receive Configuration 1 Register */
#define             0   /* Clock Divider Register */
#dT1 Receive Configuration 1 Register */
#define                /* TWI Control Register */
#defiT1 Receive Configuration 1 Register */
#define             /* TWI Slave Mode Control Register T1 Receive Configuration 1 Register */
#define             0070c   /* TWI Slave Mode Status Reg */
#define                     SPORT1_MTCS1  0xffc00944    0xffc00710   /* TWI Slave Mode Addr */
#define                     SPORT1_MTCS1  0xffc00944   R_CTRL  0xffc00714   /* TWI Master M */
#define                     SPORT1_MTCS1  0xffc00944   I0_MASTER_STAT  0xffc00718   /* TWI  */
#define                     SPORT1_MTCS1  0xffc00944        TWI0_MASTER_ADDR  0xffc0071c    */
#define                     SPORT1_MTCS1  0xffc00944                   TWI0_INT_STAT  0xffcT1 Receive Configuration 1 8   /* SPORT1 Receive Data R                SYSCR  0xffc0010      K  0xffc00724   /* TWI Interrupt Mas */
#define                     SPORT1_MTCS1  0xffc00944   CTRL  0xffc00728   /* TWI FIFO Contr */
#define                     SPORT1_MTCS1  0xffc00944   _STAT  0xffc0072c   /* TWI FIFO Stat */
#define                     SPORT1_MTCS1  0xffc00944   DATA8  0xffc00780   /* TWI FIFO Tran */
#define                     SPORT1_MTCS1  0xffc00944            TWI0_XMT_DATA16  0xffc00784 Memory Bank Control Register */
#define                   */
#define                   TWI0_RC Memory Bank Control Register */
#define                   gle Byte Register */
#define         Memory Bank Control Register */
#define                   O Receive Data Double Byte Register  Memory Bank Control Register */
#define                   cause it is not available on the ADS Memory Bank Control Register */
#define                   gisters */

#define                  Memory Bank Control Register */
#define                   Configuration 1 Register */
#define  Memory Bank Control Register */
#define                   SPORT1 Transmit Configuration 2 Regi Memory Bank Control Register */
#define                   0xffc00908   /* SPORT1 Transmit Seri Memory Bank Control Register */
#define                         SPORT1_TFSDIV  0xffc0090c   /* */
#define                     SPORT1_MTCS1  0xffc00944  define                        SPORT1_/
#define                      EBIU_DDRQUE  0xffc00a30   /**/
#define                        SP     SPORT1_MRCS2  0xffc00958   /* SPORT1 Multi chanR Register */
#define                 R    SPORT1_RCR1  0xffc00920   /* SPORReceiveve Configuration 1 Register */
#define          DDR E10   /* SPI0 Receive Data Buffer 00a3c   /* DDR Reset Control Register */

/* DDR BankRead UD  0xffc00514   /* SPI0 Baud Rate 00a3c   /* DDR Reset Control Register */

/* DDR BankRead OW  0xffc00518   /* SPI0 Receive Da00a3c   /* DDR Reset Control Register */

/* DDR BankRead registers are not defined in the sh00a3c   /* DDR Reset Control Register */

/* DDR BankRead SP-BF542 processor */

/* Two Wire 00a3c   /* DDR Reset Control Register */

/* DDR BankRead          TWI0_REGBASE  0xffc00700
#00a3c   /* DDR Reset Control Register */

/* DDR BankRead 0   /* Clock Divider Register */
#d00a3c   /* DDR Reset Control Register */

/* DDR BankRead    /* TWI Control Register */
#defi00a3c   /* DDR Reset Control Register */

/* DDR BankRead /* TWI Slave Mode Control Register 00a3c   /* DDR Reset Control Register */

/* DDR BanRead a0070c   /* TWI Slave Mode Status Reg       EBIU_DDRBWC0  0xffc00a80   /* DDR Bank0 Write Count 0xffc00710   /* TWI Slave Mode Addr       EBIU_DDRBWC0  0xffc00a80   /* DDR Bank0 Write CountR_CTRL  0xffc00714   /* TWI Master M       EBIU_DDRBWC0  0xffc00a80   /* DDR Bank0 Write CountI0_MASTER_STAT  0xffc00718   /* TWI        EBIU_DDRBWC0  0xffc00a80   /* DDR Bank0 Write Count     TWI0_MASTER_ADDR  0xffc0071c          EBIU_DDRBWC0  0xffc00a80   /* DDR Bank0 Write Count                TWI0_INT_STAT  0xffc00a3c   /* DDR Reset Contr_ERRMST  0xffc00a38   /* DDR Receive Select Register 2 */
#defDDR K  0xffc00724   /* TWI Interrupt Mas       EBIU_DDRBWC0  0xffc00a80   /* DDR Bank0 Write CountCTRL  0xffc00728   /* TWI FIFO Contr       EBIU_DDRBWC0  0xffc00a80   /* DDR Bank0 Write Count_STAT  0xffc0072c   /* TWI FIFO Stat       EBIU_DDRBWC0  0xffc00a80   /* DDR Bank0 Write CountDATA8  0xffc00780   /* TWI FIFO Tran       EBIU_DDRBWC0  0xffc00a80   /* DDR Bank0 Write Coun          TWI0_XMT_DATA16  0xffc00784ount Register */
#define                      EBIU_DDRGC0 */
#define                   TWI0_RCount Register */
#define                      EBIU_DDRGC0 gle Byte Register */
#define        ount Register */
#define                      EBIU_DDRGC0 O Receive Data Double Byte Register ount Register */
#define                      EBIU_DDRGC0 cause it is not available on the ADSount Register */
#define                      EBIU_DDRGC0 gisters */

#define                 ount Register */
#define                      EBIU_DDRGC0 Configuration 1 Register */
#define ount Register */
#define                      EBIU_DDRGC0 SPORT1 Transmit Configuration 2 Regiount Register */
#define                      EBIU_DDRGC0 0xffc00908   /* SPORT1 Transmit Seriount Register */
#define                      EBIU_DDRGC0       SPORT1_TFSDIV  0xffc0090c   /*       EBIU_DDRBWC0  0xffc00a80   /* DDR Bank0 Write Coundefine                        SPORT1_define                  DMA0_START_ADDR  0xffc00c04   /* D*/
#define                        SPDDRBWC6  0xffc00a98   /* DDR Bank6 Write CouEPPIx_STATUS               SYSCR  0xffc00CFIFO_ERRm Interrupt AssiChroma  Reg Error Copyright 2007-2008 Analog Y Register */ *
 * LicensLu            DMA0_X_MODIFY  0xffc00c14   /LTERR_OVer */BF54X_H
#defLht 2Track Overflow Copyright 2007-2008 Analog0_Y_COUNDer */************* DMA ChannUnd 0 Y Count Register */
#define    F_Y_COUNT  0x*/
/* ****** */
#d Channel 0 Y Count Register */
#define   ister         ter 0 */
#de      DMA0_C00c1c   /* DMA Channel 0 Y Modify Reg Y_CONCOT  0xf/
/* ******Pream        DMNot Corrected Copyright 2007-2008 Analog DeDMA1URQystem Interrupt      Urgent RequestAddress Register */
#define      0       4   /* SysteStatTUS  0xffc00c28   /* DMA Channel 0 Interrupt/Y_CODETm Interrupt Assi   /* DMA ChannDetrent Address Register */
#define    RegiLD       SWRST  0xFiel Addr         DMA0_X_COUNT  0CONTROL               SYSCR  0xffc0010UNT _ENm Interrupt Assi                     SIC_IAR5  0xffMA0_CDIannel 0 X Modify Dirrenion Copyright 2007-2008 Analog DXFR_TYPEnd Wc*
 * LicensOperat    Mod* Copyright 2007-2008 Analog DevFS_CFGnd Wtem escriptor PoinSync ConfiguR  0el 1 Registers */

#define     X_CO_SELADDR  0xffc00c24  /* DSelect/TriggeDMA0_X_MODIFY  0xffc00c14   / ITU       DMm Interrupt ITUve Conlaced or Progressc   Copyright 2007-2008 Analog DBLANKGURR_Y_C           00c4Outputfc00c4withve Connal Blank    GenTR  0        DMA1_START_ADDR  0xffc00ICL        1  0xffc0012   /* DMAClock 1 X Count Register */
#define            IFS            SIC_IWR   /* DMAr */
#defin* DMA Channel 1 X Modify Register */
#def  POLC
#def        ster */
#definand Data Driving/Sampe     dges* DMA Channel 1 Y Count Register */
Snd W60xffc00c30   */
#definPolarityAddress Register */
#define    LENGTH Poinc   /* S DMA_MODLength Copyright 2007-2008 Analog DeSKIPCURR_Y_terrcriptor Skip                      SIC_IAR5  0xffc     DO       SADDR  0xffc00cven ChaOd
#define                DMA0_CURPACK         /
#deferipacnnel/Unpupt/St                      SIC_IAR5  0xffc0SWAP    DMA1_l 1 InterSwa00c64   /* DMA Channel 1 Current AddrSIGN_EXMA Channefc00c6c  ign Extensnt Ror Zero-filled / PointSplit Forma8   /* DMA Channel 0 InteSPLT_EVEN_ODOUNT  0xffc00c6c  * DMA     MA1_    70   /c00c DMA Channel 1 Y Modify RegiSUBisterdefine nnel 100c6c  ub-s* DMA    ffc00c78   /* DMA Channel 1 Currene      tor Poi 0xffc/

/* One_X_CTwofine Channelsfc00c40   /* DMA Channel 1 Next RGB_FMT DMA1_CURR_AR  0xffRGBA Chann 0xff                     SIC_IAR5  0xff RegiRWMdefine        1 S    Regular WatermarkDMA Channel 1 Y Modify Registe RegiUrt Add     Register */
US  0xf                       l 1 _8		(0 << 15)iste    - 8 bitDMA Channel 1     10		(1          DMA1 -*/

UNT  0xffc00c90   /* D2		(2          DM12_X_12ister */
#define       4		(3        DMA2_nt Re4ister */
#define       6		(4          D    -gistUNT  0xffc00c90   /* D    5        DMA2_nt ReCOUNT  0xffc00c90   /* 2 Cha6        DMA2_X_MO2ister */
# Channel 0 Current X CounFS2W_LVB               SYSCR  0xffc0010F1VB_BOUNT ffer RegisteVerticDMA Channel before Start 1 Actc   Point     DMA1_START_ADDR  0xffc00cDMA2ACURR_DE  /* Systeffc00ca0   /* DMA afterl 2 Current Descriptor Pointer Register */
#define 2MA2_CURR_DE          ffc00ca0   /* DMA Channel 2 Cur2 Channel 2 Current Address Register */
#define            Registe_CURR_ADDR  0xffc00ca4   /* DM DMA Channel 2 Inc   /* DMA Channel 2 Y Modify AVF               SYSCR  0xffc00104F1_ACMA ChffDESC_PT/* Number ofODIFYshannnt Descriptoinl 2 Curre0   /* DMA Channel 1 Next Descr2UNT  0xffc00 Registe DMA Channel 2 Current X Count Register       hannel 0 Current X CountLIP               SYSCR  0xffc0010LOWunt RegiDESC_PTR  0xLower LiI BS        Bytes (      )/
#define                DMA2HIGH_NEXT_DESC  /* SysteUppcc0   /* DMA Channel 3 Next Descriptor Pointer Register */
#MA3_ */
               Dc00cc0   /* DMA      nel 3 NRegiscriptor Pointer Register */
definer */
#define egisteDMA3_START_ADDR MA3_CONFIG  0xffc00cc */

/* DMA ChanneST  0BAUD               SYSCR  0xffc001SPI Chann0xffc00cb0   /*Baud Rat             RTC_ALARM  DMA CTster */
#define                D#defin   DMterrupt AssiSPI                      SIC_IAR5  0xffc001 WOt Addnt Register WriteC_PTn Drain    a4  Copyright 2007-2008 Analog DevicMST
#defin  /* System      c00c40   /* DMA Channel 1 Next Desc  CPO/* DM            54   /R_DESC_PTR  0xffc00c60   /* DMA ChanneMA3_CHALL Lock Count Rec00ce0 has* Copyright 2007-2008 Analog DevicLSBF  DMA1_X_MODIFY LSB Fir28   /* DMA Channel 0 Interrupt/isterZ   DM         DMASizehannWordDMA Channel 1 Y Modify Register *EMISisterter 0 */
#de       terruCOUNT   DMA Channel 1 Y Count Register *SS       er RegisteSlave-Addres3_Y_COUNT  0xffc00cd8   /* DMA Channel 3  Gt Add************Ge  0xrscriptor Pointer Register */
#definefine  Z  0xffc00c18   /SendCOUNT          DMA3_PERIPHERAL_MAP  0TIMO     3*
 * Licenshe ADfer Initiount Rc00c40   0xffc00cd4   /* DMA ChFLG               SYSCR  0xffc00104  FLSand Write Count Rannel Addresster */
*/
#define                DMA2_C   DM_DEF_BF54X_H
#def 0xffc00d00   /* DMA     nel 4 Next Descriptor Pointer Re***************** 0xffc00d00   /* DMA3Channel 4 Next Descriptor Pointer RGRegiste       DMA30xffc00d00  ValuMA Channel 4 Next Descriptor Pointer RGrupt St00d08   /* DMA Channel 4 ConfA4_START_ADDR  0xffc00d04   /* DMA CGSystem 00d08   /* DMA Channel 4 Conffine  0xffc00cd4   /* DMA Chxffc               SYSCR  0xffc00104 TXCURR_DE  0xffc00c24he ADI BSColliCURR_    DMA0_X_MODIFY  0xffc00c14   / RegistXefineter 0 */
#deRDBRcriptoBuf3 CuStatuDMA Channel 1 Y Modify Register */RBSY
#define        00a3c   ffc00d18   /* DMA Channel 4 Y Count RegTster *************T                DMA4_Y_MODIFY  0xffc00d1c   /* DMA Chan TX       /* DMA Channel misT  0xffc00d18   /* DMA Channel 4 Y Count ReMODce4   *
 * License00c4Fault#define               DMA4_CURR_DESC_PTSPIce4  terrupt AssiDMA3Finisht Addr 0xffc00cd4   /* DMA ChMA Ch */
#define ter Register */
#defin    0xffc00cb0   /*he ADI BS            rupt/Status Register */
#d           SIC_IAR4  0xffc00140   /* Sy 0xffc00d2c   /* DM00a3c    4 Peripheral Map Register */
#define SHADOW           DMA4_CURR_X_COUNT  0x       0   /* DMA Chann    Shad Count */
*ister */

/* DMA Channel 5 Registers */

#defin     DMAhe TWIT  0define   /* s are from the ADSP-BF538       MA1_they hxffcnot been verified as00d40fi DMAne        o 2 C    0d40Moab processors ... bz 1/19/2007e         ister */

/* DMA Channel 5 Registers */

#define     */

/* DMA ChanneTWount Register */
#define                PRESCAL   DM7ESC_PTR  0xPrescale 4 Conf_START_ADDR  0xffc00d04   /* TW_CURer Rem Interrupt                          SIC_IAR5  0xffc001SCCB  DMA1_X_MODIFY Seri00c5amerane  trol B_Y_MOD */

/* DMA        xffc00LKDIV           DMA4_CURR_X_COUNT  0xCLKL /* DMA define     4   /L*/
#define                   DMffc00dHI                 5 Y CHiggistefy Register */
#define  SLAVEhannel 3 X Modify Register */
#define   URR_Y_COUNT  0xffc DMA Cer */
#define              DMA1_PERIPHTDVA/* DMA_PTR  0xffc00d60 A Channel 4 PeVali
#define                DMA0_CURR_XNAKxffc00d20   /* Del 0D license or the GPL-2 (or later)
 */

#ifnde         _CURR_X_COUNX Co /* ll                   RTC_nnel 5 Y Modify ReADD            DMA4_CURR_X_COUNT  0xfS
#def */
#define     DMA Cr */
Add 1 Cnel 5 Interrupt/Status Register */A Channel 4 X Modify Register */
#def S RegistC_PTR  0xffc00d60 nnel 3 CuA Channel 1 Registers */

#define     DMA5CAL/* DMress Registe00d68   /* DM5c   /* DMA Channel 5 Y ModMASTERhannel 3 X Modify Register */
#define  MURR_Y_COUNT  0xffcodify Regist                     SIC_IAR5  0xffc001M RegistBF54X_H
#defin     rrent X Count Register */
#define                D FASMA Ch************Fas  0xffc*/
#define                    DMATOP
#define        Issue Stopne  diRegister */
#define                RSTARMA Ch/
#define    epeat DMAr           DMA3_PERIPHERAL_MAP  0xDCNMA Ch3fc         Pointnnel 3 CuCoun           DMA3_PERIPHERAL_MAP  SDAUNT  0xf           D54   /Pointel 0rinnel 6 Start Address Register */
SCLUNT  0xc   /* Systed54   /*4   / /* DMA Chan Channel 5 Current Y Count Regi
#define              DMA5_PERIPHERALMMAP  0xffc00d6c   /*odify Registl 5 Peripheral Map Register */
#defnt RegiA Channel 4 X Modify Register */
#defMPROr Poine               DMnnel 3 Cuinannel 1 Cel 6 Start Address Register */LOnfigX_MODI  DMA0_Y_MODost Arbit58   /* DMA Channel 1 Y Count Register AA Channegister */
#l 5 Peri Address Regist
#define                DMA0_CURR_DA Channel 5 CurrentPoint                DMA6_IRQ_STATUS  0xffc00da8  BUFRDster */
ATUS  0xffc       Read#define               DMA4_CURR_DESBUFWRChannel   0xffc00dac   /* 
#defiffc00d18   /* DMA Channel 4 Y Count SDAURR_DES  0xffc00c240xffc00d94  Sen               DMA3_CURR_ADDR  0xSCLURR_DESm Interrupt       DMA6_Y_ne                DMA6_CURR_Y_COUNTBUSBUl 4 Y MoControl Re X MBusPTR  5 Interrupt/Status Regis Regiannel 3 X Modify Register */
#defXMTFLUSrrentterrupt Assihe ADI BS       Flusgister */
#define             RCVscriptor       DMA6_C00a3c   define                  DMA7_START_ADDR XMTINTLrent X ointer Register */
#define /
#define er Register */
#define            RCV          ************nel 7 Start Add DMA Channel 7 Confi        DMA7_NEXT_DESC_PTR  0A Channel 4 X Modify Register */
#dXMTA Chaf8   /* DMA Channel I BSr */
DMA4_Y_MODIFY  0xffc00d1c   /* DMA CRCVFY  0xffA1_NEXT_DESC00a3c   el 7 X Modify R5 Interrupt/Status RegisINT*/
#K     DMA4_CURR_Y_COUNT  0xffc00d3INITt Addr DMA Channel 5 Current X Crrent Yed*/
#define                        SPI0_RDBR  0SCOMPCount  DMA Channel 5 Current X CCoc00ct  /* DDR Res                       SPI0_RDBR  0xSERRt Addegister */
#define     3 Cu    DMt Descriptor Pointer Register */
#define       OVF   DMA3_CURR_X_CO DMA Cel 0 Y Co*/
#define                  TWI0_SLAVE_STAT  0ESC_PTR  */
/* ********nnel 6 Current 7 Current Descriptor Pointer Register */
#define      M        ter 0 */
#definnel 6 Currentde4   /* DMA Channel 7 Current Address Register *ODIFERV                    DMA4_Yel 7 Xervicent Descriptor Pointer Register */
#define       nnel 7 Cm Interrupt NT  0xffc00dd8#define                DM Channel 7 Y Count Register A Channel 4 X Modify Register */
#def_MODIY  0xffc00ddc   /* DMA Channel 7 Y Modifyinter Register */
#define       ESC_4_NEXT_DESC_PTR  0xffc           DMA7_PE*/
#define                    DMAster */    DMA7_CURR_ADDR  0xffc00de4   */
#define                    DMAOVce4                 DMA7_IRQ_STAxffc00dec   /* DMA Channel 7 Per        ister */
#define              DMA7_PE_PTR  0xffc00d80   /* DMA Channelfine                */
#define           _START_ADDR  0xffc00d04   /*  Channe 7 Current X Count Register */
#definey Register */
#define            fine   Channel 7 Current Y Count Registeannel 8 Registers */

#defiXMT_DAegul00dc0   /* DMA Channel 7 Next De* DMA _DESC_PTR  0xffnt Register */8-/
#d Channel 2 Periphera  0xffc00e1c   /* DM16 Channel 8 Y Modify Register /
#defi_DDRBW0d2c   /* DMA Channelr */
16PTR  0xffc00e20   /* DMA Channel 8 CuRCV/* DMA Channel 8 Y Modify Register *RCVdefine               NT  0xffc00dd_PTR  0xffc00e20   /* DMA Channel 8 CuAddress scriptor Pointer Register */
#d      D                 DNT  0xffc00dd 0xffc00e24   /* DMA Channe  /* DMAORTx_TCR        SIC_IAR4  0xffc00140   /* TCKF                 MA6_Y_Falc5c   /* c00d00  _START_ADDR  0xffc00d04   /* DMLATFter */
  /* SysteLMODIMA8_CURR_A */
#defin            DMA3_CURR_ADDR  0xffcCURR_Y_hannel 3 Y Mount   /* DMA Channel 8 Register */
#define                DMADICURR_Y_define      Poin-Independ 0xf*/

#define               DMA9_NEXT_DESC_PTR  0xffc00e40   TFS  DMA6_X         DMA8_CURR_ */
#definfc00iret Addres Current Y Count Register */

/* /* DMA CA1_X_MODIFY  0xffc00c*/

#define               DMA9_NEXT_DESC_PTR  0xffc00e40 TLSBe00   /              DMA4_Y/
#dOrd       DMA3_Y_MODIFY  0xffc00cdc TD      DMA1_NEXT_DESCPoint  DMA2_STARTypt Register */
#define                DMAITCLChann0xffc00de0    /* DMA Channel 96 Curren */
#define                      DMATSAL_MAP Pointer Register */
#                  RTC_ALARM ine                       SYSCR  0xffc00104 TRF  0xff4   /* DMA CLeft/Righ /* DMA Channel 9 X Count Register */
 TSFc00cecfc00d08   /*he ADI BSStereo Start Addre                     SIC_IAR5  0xffc001TXc00cec    /* DMA ChxSEC   /* DMA Channel 5 Current Descriptor     00   /c00d6c   /* ne  TATUS   /* DMA Channel 7 X C_Y_MODIFY  0xR           DMA8_CURR_X_COUNT  0xffc0Re30   /* DMA Channel 8 Current X Count Register */
#define                DMA8_RURR_Y_COUNT  0xffc00e38NT  0xffcChannel 8 Current Y Count Register */

/* Dal Map nel 9 Registers ne                Dine                  DMA9_START_ADDR  0Rffc00e44   /* DMA Cne                Dss Register */
#define                      DMA9_al Map Reg#define           X Count Register */
#define                DMA9_CURR_Y_CR      DMA9_X_COUNT  0nel 7 Sta  /* DMA Channel 9 X Count Register */
Rdefine                    DMA9_X_MODIFY  0xffc00e54   /* DMA Channel 9 X Modify RRgister */
#define           00a3c    DMA9_Y_COUNT  0xffc00e58   /* DMA Channel 9RY Count Register */
ter */
#d                 DMA9_Y_MODIFY  0xRfc00e5c   /* DMA Channel 9 Y Modify Register */
#define               DMA9_CURR_DESC_PTR  0xffc00e60   /*RDMA Channel 9 Curren00a3c   tor Pointer Register */
#define                   DMA9_CURR_ADDR  0xffc00e64 R /* DMA Channel 9 Current Address Register */
#er */
                  DMA9_IRQ_STATUS  0xffc00e68   /* DMA ChA Channel 4 X Modify Register */
#definHR                 DMA8_CURRHold
#dei     EmpDMA Channel 3 Current Descriptor PoTl 8 Con            Stickynt Descripel 0 Y CoDMA4_Y_MODIFY  0xffc00d1c   /* DMA ChanTU 8 Con   /* DMA Chaffc00ea4   /* D00c1c   /*ent Descriptor Pointer Register */
#define8 Configuration RMA8_CURR_ADDR Full DMA4_Y_MODIFY  0xffc00d1c   /* DMA Channl 8 Conegister */
#dffc00e00a3c   MA Channel 10 Current Address Register */
#definR       0xffc00de0   Register */
#d0xffc00ea8   /* DMA Channel 10 Interrupt/Status RRXN       DMA7_Y_COUNT  0xffc00ddel 0er Reg8   /* DMA Channel 7 Y_Y_MODIFY  0xMCMC        SIC_IAR4  0xffc00140   SP_W        0xffister */
inY Co3_IRQ* DMA Channel 1 Current AddresP_WOFce4  3 DMA ChaESC_PTR  s Offse
#defnt Register */

/* DMA Channe                SYSCR  0xffc00104   MF      annel 3 Y Moulti cChanne       DelaMA Channel 3 Current Descriptor PoFS       ter Register */
#definto      Relount ship00ec0   /* DMA Channel 11 NexMC

#defin/
/* *******r */
#define       A6_NEXT_DESC_PTR  0xffc00d80   /* DMA ChaMCDRXDMA1_CO*
 * Licenser */
#define  DMA00a3c   rupt/Stster */
#define                  TDMA11_XBF54X_H
#defic00ed4   /* DMA Che ADI BS1 X Modify Register */
#define           C        /* DMA Cha2X DMA ChRecoveryunt Register */

/* DMA Channe    CHNster */
#define                CUR Registor Pointer RegCurr 0xf Channe IndicatA Chan */

/* DMA ChanneUA    LC       if 0    conflictsc00c50legacy    t DelMA Cs
#define                  DMA6_START_AD Wdefine  /* DMA Cha DMA9_IRQ_STY_COUNT  0xendifpyright 2007-2008 Analog DeviceSTX_MODripheral Map Rop*/
#escriptor Pointer Register */
#defiAL_MAP _MODIFY  0xfPESC_PT_Y_COUNT  0xffc00cd8   /* DMA Channel 3 EPMA Chanatus Regist    L_MAP  ine                  DMA9_START_ADDR  0 ST               DMA10_IRQL_MAP  ap Register */
#define            SDMA Ch   /* DMA Chat BreaI Interrupt Status RegiointerMRegisterright 2007-2008 Analog DevicXcriptorPointer Register */
a4  OffRegister */
#define               RTNFIG  *
 * Licensednu00e8800c28 Toefine e                DMA9_CURR_Y_COUNTe00   DMA7_CONFIG NT  0xffc00ddIRQ Threshcripe                DMA9_CURR_Y_COUNTgurati            DMA7_X_Cr */
#TSTART_ADDR  0xffc00f04   /* Memory DMA SLOO  DMer Reegister */
#LoopbhannA6_NEXT_DESC_PTR  0xffc00d80   /* DMA ChannF_CURR_DEnt DescriptorY Cohannel 5P Des  /* DMA Channel 3 Current Descriptor PoA00f00  11_CURR_Y_COAutomatirent Y Destination Next Descriptor Pointer RegisteAC0f00  A Channel 11Count RegiClearstination Nerrent Descriptor Pointer S            DMA4_CURR_X_COUNT  0xff  P  0xf 0xffc00eb8  Registadount Register */
#define            O Chann1_NEXT_DESC_/* Du0xffc00d18   /* DMA Channel 4 Y Count Reg         DMA11_Y_COL_MAP  0     MDMA_D0_Y_MODIFY  0xffc00f1c   /* 0   /*c00d84   /* Dramgiste     MDMA_D0_Y_MODIFY  0xffc00f1c   /* B     P  0xffc00dac0xffc/
#define rrent Address Register */
#define/* DMA A Channel 11THRter Register */
#define                 EMMA Changisters */

#define   er Register */
#define                 TF     nel 5 X Count DMA4_CURR_Al 4 InterChannel 11 Current Descriptor PointerMdefine                  MDMA_D0_Y_COS 0xffc0xffc00ddc   /ffc00e 0xfCopyright 2007-2008 Analog DeviceERIPHERAion Currenttream 0 Destinati              DMA9_CURR_Y_COUNTCter */
#define     Address Regunt Rerent Y Count Register */

/*ointerIefinET &t Register CLEA            DMA4_CURR_X_COUNT  0ERBRQ_STACOUNT  0xffc00c38  nel 7 Start Addr0_PEriptor Pointer Register */
#define     ETBE     /
#define   _X_COUNster */
#define er Regriptor Pointer Register */
#define      ELS     DMA7_CONFIG tream 0 DestinaDMA4_Y_ Y Count Register */
#define            DSegiste_MODIFY  0xfter */
#odem MDMA_S0_START_ADDR  0xffc00f44   /* Memory DMEDTPTestination Curren_X_COUN8   /* DMA Cha0_ST Stream 0 Source Next Descriptor Pointer IRQ_STAStatus Register */
f28   /* Memory DMA Stre Count Register */
#define            RFCgister00f48   /* Memory Memory DMA Stream 0ory DMA Streaon Current X Count RegisGannel 3 X Modify Register */
#define UCURR_Y_COUNT  0xffcoint 0xffc00eec   /* DMA Channel 11 PeripherIR    DMA/
#define   rDADMA Stream 0 Destination Configuration RegisT*/
#defiDMA7_CONFIG gisteTXD0_X_COUNT Cha * Copyright 2007-2008 Analog DeviR*/
#defi_MODIFY  0xfgisteRDMA Stream 0 Source Y Modify Register */
#define   F      ine          orc 11 Destination Y Modify Register */
#define    F0   /*nt DescriptorinterR_DESC_PTR  0xffc00f20   /* Memory DMA Stream EDBisterine                Divide-by-c00c0xffc00ce8   /* DMA Channel 3 InGRegisteA Channel 11_X_COUNGlobal LS 0xffc00f54   /el 5 Start Address Register */
#define                e    ULTI     MACRO ENUMERATIONS   DMA4_PERInnel 5 Start Address Register */
#define            DMA8_YCODE DMA5DESC_ op */
# (SYStor ror Poin_COUNT Count Rffc00_WAKEUP /* 0x         boot accordModito wake-upfineNFIG  0xffc00d88  nt RegFULLBOO00   02_X_ Regalways perform f0_PE     0f78   /* Memory QUICKStrea 0 SA Che Current Y Count quickster */

/* MDMA StreamNOStream
#defitem e Current Y Count Register */

    CNT_COMMANDf70   /* Memory DMA */
#define W1LiptoZERO/
#defin 0xf/* w#defi10_CUloadriptor UNTERc00c50z              D      MMIN*/
#definDMA7R  0xffc00f84   /* Memory DMA Sxffc0t Addresstination Start AddAX*/
#defin_MOD/
#define                   MDMA_D1_CONFIAXfine            MINMDMA_D1_STARral MR  0xffc00f84   /* Memodrestream 1 Destination Star    xffc0
#defiA ChaT  0xffc00f90   /* Memory DA_D1_CONFry DMA S1 Destination X Comory DMA StA Cha#define                 MDMA_D1_X_MO#define             AXMDMA_D1_STA 0xffcT  0xffc00f90   /* MemorAXDMA Stream 1 Destination XAXount Regist#defin0xffc00f98   /* Memory DMA A_D1_X_MODIFY  0xffc00f94   /* MAXddress Regi4   /*fine                 MDMA_D1_Y_MODIFIG  0xfescriptor NFIGer Register */
#define         CNTMream 1ADEN#defi         quadrature encoDMA m00c40   /* DMA Memory DBINtream_Y_COUNT     inaryt Descriptor Pointer Register */
U0xffc0ster */
#d/* up/down cnt Rriptor Pointer Register */
DIRory DMeam 1 De/* d Channel on Current Address Register */
#defTMR      5        MDMA_D1_Itimriptor Poinent X CounNDory D      /
#define       undMDMAcompR  0tor Pointer Regi*/
#defiDMA_D1_A Channel _D1_PERIPHERAL_MAP  MA1_ 1 De0xffc00fac   /* Memory DCAPy DMAunt Regis_D1_PERIPHERAaprrent0xffc00fac   /* Memory DA
#defD1_CU3 MDMA_D1_PERIPHERauto-eMA1_dt/Status RURR_ory t DeTIMERunt RD1_CURR_DESC_PTR  0xffc00fa0   /* PWM_OUeam 0 S01ination StDTH  MDm 1 De2ent X CouEXT    
#defin3     ointer Regr Register */
#define          LS_5/
#define  _COUN5 d_MODUNT  0xffc00c90 Memo6D1_START_ADDCOUN6urce Next Descriptor PointA Chount Rr */
#d7urce Next Descriptor Point_MOD      r */
#d8urce Next Desc    PINTister PointTR  0ightFIG   /* Memory DMA IQ0m 1 Desestination CuPIQ1fc00fc8   Register *PIQ2fc00fc8   4urce Configu3fc00fc8   8S1_CONFIG  0x4fc00fc8  10urce Configu5fc00fc8  2 0xffc00fd0  6fc00fc8  4 0xffc00fd0  7fc00fc8  80S1_CONFIG  0x8fc00fc8 10 0xffc00fd0  9fc00fc8 2X_MODIFY  0xff1ffc00fc8 4* Memory DMA St Stream 180              1ration Rhann* Memory DMA ne      COUN* Memory DMA S1_X_COU6_X_* Memory DMA  /* Memoess efine         Source nnel * Memory DMA */
#def 0xff* Memory DMA   MDMA_e    * Memory DMA c00fd4 ess RS1_CONFIG  0x2ffc00fnnel 1urce Configur Strea 0xffcurce Configurratione     urce Configurne    ess Reter */
#defineS1_X_ster */urce Configur /* MSC_PTR urce ConfigurSourc       urce Configur*/
#dess Regter */
#define  MDster */fc00fe4   /* Mc00fSC_PTR fc00fe4   /* 3ffc0                      Stress Reg0ter */    A              MDM    DMA50 Sources
 DMAA,y DMAA */
 */
#def     ,gister#deffine        RIPH   MDM*/
#defdefiMA_S1_PEFER/Status Reg* Memory DMA Affc00fc/* Memory DA Streamource ConfiAration ster */
#deAne     8_COUNT  0xfS1_X_C  0xffc00fd0A /* Me DMA Stream ASourceount RegisteA*/
#de8 */
#define   MDM_X_MODIFY  0xAc00fd/* Memory DMA* PLADDR   /* Memory D Stre
#de/* Memory Drati MDMA_S1_Y_COUAMIL
#defi Count RegisS1_Xm 1 Source Y C    fe8   emory DMA Btream 1 Source Interrupt/Status RegisteB   /* Bfine      B    MDMA_S1_BERIPHERAL_MAP T3_DLH00fec   /* Bemory DMA SBream 1 Source Peripheral MapBRegister */
#definB             MDMA_B1_CURR_X_COUNT  0xBfc00ff0   /* MemorB DMA Stream 1 SourBe Current X Count Begister */
#defineB            MDMA_SB_CURR_Y_COUNT  0xfBc00ff8   /* MemoryBDMA Stream 1 SourceBCurrent Y Count RegBster */

/* UART3 RBgisters */

#defineB          emory DMA Ctream 1 Source Interrupt/Status RegisteC   /* Cfine      C    MDMA_S1_CERIPHERAL_MAP 18   /00fec   /* Cemory DMA SCream 1 Source Perip Count RegCRegister */
#definC             MDMA_C1_CURR_X_COUNT  0xCfc00ff0   /* MemorC DMA Stream 1 SourCe Current X Count Cegister */
#defineC            MDMA_SC_CURR_Y_COUNT  0xfCc00ff8   /* MemoryCDMA Stream 1 SourceCCurrent Y Count RegCster */

/* UART3 RCgisters */emory DMA Dtream 1 Source Interrupt/Status RegisteD   /* 4   e      D    MDMA_S1_DERIPHERAL_MAP RBR  000fec   /* Demory DMA SDream 1 Source Peripheral MapDRegister */
#definD             MDMA_D1_CURR_X_COUNT  0xDfc00ff0   /* MemorD DMA Stream 1 SourDe Current X Count Degister */
#defineD            MDMA_SD_CURR_Y_COUNT  0xfDc00ff8   /* MemoryDDMA Stream 1 SourceDCurrent Y Count RegDster */

/* UART3 RDgisters */

#defineD                   D   UART3_DLL  0xffcEtream 1 Source Interrupt/Status RegisteE   /* E          E    MDMA_S1_
#defansfer Count /* Receive BufEemory DMA SE                   UART3_SCR ERegister */
#definE             MDMA_E1_CURR_X_COUNT  0xEfc00ff0   /* MemorE DMA Stream 1 SourEe Current X Count Eegister */
#defineE            MDMA_SE_CURR_Y_COUNT  0xfEc00ff8   /* MemoryEDMA Stream 1 SourceECurrent Y Count RegEster */

/* UART3 REgisters */

#defineE                   E   UART3_DLL  0xffcFtream 1 Source Interrupt/Status RegisteF   /* F          F    MDMA_S1_F
#define      /
#def00fec   /* Femory DMA SF                   UART3_SCR FRegister */
#definF             MDMA_F1_CURR_X_COUNT  0xFfc00ff0   /* MemorF DMA Stream 1 SourFe Current X Count Fegister */
#defineF            MDMA_SF_CURR_Y_COUNT  0xfFc00ff8   /* Memorynt R Stream 1 Source Regrent Y Count Regegisr */

/* UART3 Risteters */

#defineter                 r */UART3_DLL  0xffcGtream 1 Source Interrupt/Status RegisteG   /* G          G    MDMA_S1_G
#define      330   00fec   /* Gemory DMA SG                   UART3_SCR GRegister */
#definG             MDMA_G1_CURR_X_COUNT  0xGfc00ff0   /* MemorG DMA Stream 1 SourGe Current X Count Gegister */
#defineG            MDMA_SG_CURR_Y_COUNT  0xfGc00ff8   /* MemoryGegister */
#define G                 EPGI1_FS2W_LVB  0xffc0G32c   /* EPPI1 FS2 Gidth Register / EPPG1 Lines of VerticalHtream 1 Source Interrupt/Status RegisteH   /* H          H    MDMA_S1_H
#define      1408  00fec   /* Hemory DMA SH                   UART3_SCR HRegister */
#definH             MDMA_H1_CURR_X_COUNT  0xHfc00ff0   /* MemorH DMA Stream 1 SourHe Current X Count Hegister */
#defineH            MDMA_SH_CURR_Y_COUNT  0xfHc00ff8   /* Memory0204 Stream 1 Source/* Wrent Y Count RegT  0r */

/* UART3 R04  RT3_THR  0xffc031Itream 1 Source Interrupt/Status RegisteI   /* I          I    MDMA_S1_nt Re PINT0_INVERerrupt 0 InterrIemory DMA SI                   UART3_SCR IRegister */
#definI             MDMA_I1_CURR_X_COUNT  0xIfc00ff0   /* MemorI DMA Stream 1 SourIe Current X Count Iegister */
#defineI            MDMA_SI_CURR_Y_COUNT  0xfIc00ff8   /* MemoryIine                IPINT0_EDGE_CLEAR  0Iffc01414   /* Pin II32c   /* EPPI1 FS2 Iidth Register / EPPI1 Lines of VerticalJtream 1 Source Interrupt/Status RegisteJ   /* J          J    MDMA_S1_J01418   /* Pinrrupt 00fec   /* Jemory DMA SJ                   UART3_SCR JRegister */
#definJ             MDMA_J1_CURR_X_COUNT  0xJfc00ff0   /* MemorJ DMA Stream 1 SourJe Current X Count Jegister */
#defineJ            MDMA_SJ_CURR_Y_COUNT  0xfJc00ff8   /* MemoryJine                JPINT0_EDGE_CLEAR  0Jffc01414   /* Pin IJterrupt 0 Edge-sort MuxModi/
#d  /* InterrDMA ChaUXfine     efine         MUXffc00fc8   3Interrupt 1 E_ffc00fc8   */

#definRegis Stream 1 Sination CuRegisration RegiRegister *Regisne         3 Interrupt 1  Stream 1 SCtivity Clear ster */
#define          1      PINT1_ster */
#dRT_SEration Regi   /* MemoRT_SEne         C Interrupt 1 ration Reg3ine          2ster */
#define          2      PINT1  0xffc00fdnterrration Reg DMA Streamnterrne        3tream 1 SouMUXne        Cfine         3ster */
#define          3      PINT1ount Registter *ration Reg      MDMA_ter *xffc01450   /ATCH  0xffc0S1_X_COUN3fine          4ster */
#define          4      PINT_X_MODIFY  0 (32-ration Re/* Memory DM (32-ne       sterT2_MASK_SET  /* MemorCfine          5ster */
#define          5      PINTream 1 Sourc    Pration Ret Y Count Re    Pne       k Senterrupt 2 MSource Xurreter */
#definster */
#define          6      PIN MDMA_S1_Y_CO   /*ration Rrs */

#defin   /*ne           ister */
#de*/
#defiC             PI7ster */
#define          7      PINm 1 Source Y t Assration RRT3_D*/
#define   ne      GN  0    PINT2_ED  MDMA_urre     PINT2_ED8ster */
#define          8      PI     MDMA_S1_Ydefinration ffc00fdc   /* definne     upt 2     PINT2_EDc00fd4 GN       PINT2_ED9ster */
#define          9      PIStream 1 Sourc   PIration RegistT  0xffc01478ne      ClearINT1_INVERT_Sffc00fupt 2PINT1_INVERT_Sister */
#define          1       P      MDMA_S1_C   /* rationTR  0xffc00fe0    /* ne           xffc0147c   / Strea Clea0xffc0147c   /             PINT1_INVERT_SSET  0xfy DMA Stream 1  Regisrationrent Dee             ne    0xffc01T2_LATCH  0xfratio      NT2_LATCH  0xf  0xffc0144c   /* Pin Inte1rrupt 1egister */
#defis (32-ratio         MDMA_S1s (32-ne   h RegistT2_LATCH  0xfne   0xffc0NT2_LATCH  0xfrupt 1 Pin Status Register1 */
#deR  0xffc00fe4   PINT3_ratio DMA Strxffc01494   /* 3 Mask Set Rpt 3 Mask Cledth h Regisupt 3 Mask Clet Interrupt 2 Registers (312-bit)ent Address Regis Interfc01define            Intererru         pt 3 Mask Cle Linsk Set upt 3 Mask Cle*/
#define                1 PINT2_S1_IRQ_STATUS  0ter */fc018   /* Me              erru0149c   /ET  0xffc014(b15,b14,b13,b12,b11,b10,b9,b8,b7,b6,b5,b4,b3,bt Re,b0) \
r */(((-sen)&3)    30) |NT3_EDGGE_Cb14  0xffc028a4   /* Pin Inte3rupt 3 Ed6a4   /* Pin Inte2rupt 3 Ed4a4   /* Pin Inte1rupt 3 Ed2a4   /* Pin Inte0rupt 3 Ed4a4   /* Pin Int9)  0xffc01ge-sensitivity C8ster */
#der */
#define   7ster */
#d   PINT3_INVERT_6ster */
#d14a8   /* Pin In5ster */
#d4a4   /* Pin Int4ster */
#8) 4   /* Pin Int3ster */
#6STATE  0xffc014b2ster */
#4STATE  0xffc014b1ster */
#2STATE  0xffc014b0ster )) /* MemorDESC_PTE_SETINT0_ASer *ap Renter1upt 3 LaStatus ReCurrent X Coun0MY Count 000ript*/

#deyte 0       Half       appModify Register

#de_PALer */
#defi      p      Aters to          /* Function EnabBe Register 1/
#define    B                       PORT1#define   FF_NEXT_DESC     1define A_FER  0xffc014c0   /* Functio    _PAH Register */
#define      fc00d        */
#define   #defineB          0xfPIO Data RegisCLEAR  0xffc014cc   /* GPIO2#define  PORORTA_SET  0xffc2   PORTA_FER  0xffc014c0   /* FunctioSET able Register */
#define                  A4_START_ADDR       fc014c4 nnel GPIO Data Register */
#defiIO Direction C3#definc014d0ORTA_SET  0xffc314c8   /* GPIO Data Set Register */
#xffcne                      PORTA_CLEAR  0xffc0fine                Clear ster */
 */
#define                X_MODIFY  0xf   /* Pin Inter2upt 3 Latch Regi3ter */

/* Port A Registers */

#de_PCe Register */
#define    C                       PORTA  0xfDc014c4   /* GPIO Data RegD                       PORTA  0xfEe Register 2Registers */
E                       PORTA  0xfFe Register 3Registers */
F                       PORTA  0xfGe Register 4Registers */
G                       PORTA  0xfHe Register 5Registers */
H                       PORTA  0xfIe Register 6Registers */
I                       PORTA  0xfJe Register 7Registers */
J                     /* GPIO Data C                     PORTC_CLEAR  0xffc014cc   /* GPIO Data Dlear Register */
#define D_CLEAR  0xffc014cc   /* GPIO Data E         #defta Set RegisteCLEAR  0xffc014cc   /* GPIO Data F         3ers */

#defineF                       PORTC_FER  G         4               POCLEAR  0xffc014cc   /* GPIO Data H          1 Dt Register */
CLEAR  0xffc014cc   /* GPIO Data I         6               I      PORTC_SET  0xffc01508   /* GJ         7               J      PORTC_SET  0xirection Clear R                        PORTB  0xffc014eIO Direction Clear Rister */*/
#define                      IO Direction Clear R8   /* G 0xffata Set Register */
#defineIO Direction Clear R  PORTB_upt 2  0xffc014ec   /* GPIO DataIO Direction Clear Rdefine  URR_AD            PORTB_DIR_SET IO Direction Clear RO Direct5er */
#degister */
#define     IO Direction Clear RIR_CLEARMA Chfc014f4   /* GPIO DirectionIO Direction Clear Rdefine  7  /* Multiplexer    PORTB_INEN                       Input Enable Register */
#define        r Control Register *B_MUX Port B Registers */
tiplexer Contror Control Register *rt C RSC_PTR   */

#define               r Control Register *0xffc0h Regis/* Function Enable Register r Control Register *                  PORTC  0xffc01504   /* r Control Register */
#def   /*                      PORTC_SEr Control Register *PIO DaMA ChanRegister */
#define         r Control Register *LEAR   Func0150c   /* GPIO Data Clear RegX_MODI              DAL_MAtibilC_PTR  criptor Poin(x)in Inx)-*/
#fc015    D*/
#define             c   /* Memory DAX Memory DMAegister */BIU_AMCBCTL0rs */

#efine Registers */

#defin1            /* Memory DMerru0_STon EnaREQUEST/* Function E1able Regi1ter */
#define       2able Regi2ter */
#define       3able Regi3ter */
#d      DD_DI_DEF_BF54X_H    