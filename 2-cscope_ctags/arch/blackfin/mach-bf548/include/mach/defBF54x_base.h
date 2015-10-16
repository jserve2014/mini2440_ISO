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
#define*
 * Copyriilater)
 *
2007-2008 1nalog Devic4s Inc.
 
 * CLicensed under the ADI BS1 l


/*  o******GPL-2 (or later)
or t
#ifndef _DEF_B254X_H
#defi8e _DEF_BF54X_H


/* ******************2******************************************* */
/*   354X_H
#defice _DEF_BF54X_H


/* ******************3*************************************#ifndef _DLENGTH54X_H
#def1ee _DEF_BF54X_H


/* *************Length**********************************#ifndef _DTIMESTAMP /* PLL
 * ne/
/*   F54X_H


/* *        VR_CTimestamp**************************************r */
#dDivisIDA /* PLL Con ADDRESS DEFINITIONS COMMON TO ALL GPLEF_B Inc Voltage liculator
 * Lice license or the G       he G1        VR_CTol Register license s   ******************************************* */9DEF_Bine        2trol Register */
#define        9******D******************************************* */0xFFC0     PLL_LO2ne                           VR_nt Rfinelicense or tegisDebug/MP/Emter ionL Lock Coun(fine  SYSTEM & MM2                PLL_STAT  0xffc0define -    xfine*/            VR_CTntroller (0xFFC001fine  ntroller (02KCNT  0xffc00010   /* PLL Lock Cdefine he GPL-define 00   /* Software RePLL_CTLnalog 90
#de PLL S/
#def300014)******ST  0xffc00100   /* Software Resre Reset RDIVnalog De0004ine       Div9sor license or the G3stem CHIPID_VERSIONnaloF
#def00
#dister *egist* PLL Status Register */
#define             9ST  0xffc00100           MANUFACTUREnaloIMASKFFE
0cine       Status license or the GPL-00   /* Softw*/
#     PLL_LO3         SYSCR  0xffc00104   /* Synt                    CHIPID_FAMILY  0x0FFFF000
10xFFC00014 - 0xFF4trol Register */
#define        10            CHIPID  0xffc00014
/* CHIPID Masks */
#de           PLL_LO4ne                           VR_terrupt  Intatus ReMask          2gister 1 */
#define                   4                PLL_STAT  0xffc0tatus Reystem Reset and Interrupt Controller (0xFFC001       ntroller (04KCNT  0xffc00010   /* PLL Lock Ctatus ReRST  0xffc00100   /* Software Reset Register 1_IMASKine         5_ISRAnalog De011* PLL SSystemgister4   /* System Configuration register */

/* SIC10egisters */

#define5p Regis* Syste/
#define   us Reterr0xffc0010c   /* System Interrupt Mask Register 0 */
#def10r 1 */
#define5r 2 */
#define            upt Mask /* System Interrupt Mask Register 1 */
#define    Assig     PLL_LO5efine        Wakeup          0or thefine        
#define                         SIC1xFFC00014 - 0xFF6upt Wakeup Register 1 */
#define 1                            ST  0xffc00100   /* Softwignmen     PLL_LO6p Register 2 */
#define          R2  IC_IAR4 IAR2akeup Regi3ter 1 */
#define   us Resignm Reg           6er 0 */
#define                  C_IAR4                  SIC_ISR2 SIC_ISR2  xffcW Wakeugisterntroller (06 1 */
#define                    ignmentxffc00100   /* Software Rement Re1akeup Regi288   /*ine         7                  SIC_ISR2 0xffc0014c   /* System Interrument Re140   /* Sy2* Syste1egisters */

#define7ST  0xffc00100   /* Software ReAR4  rrupt Assignment A Wakeup Regi3efine  errupt Assignment 1r 1 */
#define78   /* System Interrupt AssignmAR5154   /* Systt Register 3er */

errupt Assignment Re    SIC_ISR1  07C_IAR9  0xffc00154   /* Syst6nalog    SIC_IAR2  0xffc00138   /* System Interrupt Ass2xFFC00014 - 0xFF8upt Wakeup Register 1 */
#define 2  SIC_IAR3  0xffc0013c   /* System Interrupt Assignme      SIC_ISR1  08p Register 2 */
#define          ister 1ignment Registerm Interrupt1   /* System Interrupt As           8er 0 */
#define                  ister 1c00144   /* System Interrupt Assignment Registeffc00124   /* Sys8 1 */
#define                    _CTL  0x   /* System Interrupt Assignment Register 6 2
#define         9rupt Ast Register 5ter 2 */
#definec   /* System Interrupt Assignment Register 7 */2egisters */

#define9  0xffc00100   /* Software ReWDOG_C* System Interrupt Assignment Register 8 */
#define      2r 1 */
#define9Register */
#defiNTakeup Reg2ter */ Interrupt Assignment Register 9 */
#define        iste     PLL_LO9          * PLL SWatchdognterrupt M   SIC_IAR2  0xffc00138   /* System Interrupt Ass3xFFC00014 - 0xFFaupt Wakeup Register 1 */
#define 3  SIC_IAR3  0xffc0013c   /* System Interrupt Assignme1 */
#     PLL_LOadefine                         SIC/* RTCLARMakeup Reg31efine  RTC Alarmt Mask Register 1 */
#           ae                         SIC_IAR5h Low c00144   /* System Interrupt Assignment Registe1 */
#ntroller (0a                     SIC_IAR6  0xfh Low r */

/* RTC Registers */

#define                define        b* System InRTC_PRE    Sivisor           * System Interrupt Assignment Register 7 */3egisters */

#defineRegister */
         UART0_DLister ** System Interrupt Assignment Register 8 */
#define      3r 1 */
#definebUART0_LHakeup Reg4ter */

*/
#dor L Interrupt Assignment Register 9 */
#define                       Regi/* Global
#define                  SIC_IAR2  0xffc00138   /* System Interrupt Ass4xFFC00014 - 0xFFcupt Wakeup Register 1 */
#define 4  SIC_IAR3  0xffc0013c   /* System Interrupt Assignmeignmen     PLL_LOcdefine                         SIC_I  UA              UART0IER_SEne        42efine  ister 1 *           ce                         SIC_IAR5    UAc00144   /* System Interrupt Assignment Registeignmenntroller (0c                     SIC_IAR6  0xffc  UAr */

/* RTC Registers */

#define            4 /* Line Control d0013cSet                SIC_IAR7  0xffc0014c   /* System t Assignment Register 7 */4egisters */

#defined       SYSCR  0xffc00104   /* S UAR* System Interrupt Assignment Register 8 */
#define      4r 1 */
#defined           UART0RBR Hold Regis* Sys Interrupt Assignment Register 9 */
#define                       DBR  0xffc0SPI0_REGBAS 0xff0010c500
               UART0_IER_SET  0xffc00420   /* Int5xFFC00014 - 0xFFeupt Wakeup Register 1 */
#define 5  SIC_IAR3  0xffc0013c   /* System Interrupt AssignmeI0_BAU     PLL_LOep Register 2 */
#define          * Systeisters */

#define 0xffc00100   /* Software R /* SBAU           eer 0 */
#define                  * Systec00144   /* System Interrupt Assignment Registeared fntroller (0C_ALARM  0xffc                      isters are not defined in the ssignment Register 6 5 /* Line Control fgister 9PI0 Baud RatRegisense or th          SPI0_FLG  0xffc00504   /* SPI0 Flag Re5egisters */

#definefDI BSBuffegishadow                 * System Interrupt Assignment Register 8 */
#define      5isters are notfthey are not available on*******SP- Interrupt Assignment Register 9 */
#define        r th     PLL_LOffc00414   /* _ADDR  0TW* SPI0 Receiem Interrupt Mas
DEF_BF54X_H


/* ************************sde ADBR  0xffc0rupt Assignment Registe6xFFC00014 - 0xFec_ISR0  0xffc00118   /* System Int6  SIC_IAR3  0xffc0013c   /* System Interrupt AssignmeMa0013     PLL_Le */
  CHIPID_VERSION  0xF0000000
#definTWgister 1 */
#define                         SIC Statu          eR   CHIPID_MANUFACTURE  0x00000FFE

/* TWc00144   /* System Interrupt Assignment Registe Statu*/

#define   SIC_IWR0  0x                       STWr */

/* RTC Registers */

#define            6 /* Line Controleefine                  */
#define16TWI0_SLAVE_ADDR  0xffc00CONTRO0xffc0010c7ter */

6egisters */

#defin */
#WI FIFO Status Register */VR_16AVE_CTR          TW* PLL STWI Slave Mode
#define          6 TWI FIFO ConTWI FIFO Status Reet RSTAne       16  TW* SysteXMT_DATA8  0xffterrupt Mask Register 1 */IFO pt Mask RegCKfine        0 Latch L/
#dLock e      ister 1 */
#define                         SIC7Statu  0xffc0078C                  SYSCR  0xffc0017  SIC_IAR3  0xffc0013c   /* System Interrupt Assignme      pt Mask Regve Dat                 SIC_IMASK0isters are not defined in ffc007CV2008 8         T8  TWI0_XMTister 1 */
#def      SIC_IMASK1  0xffc00110  TWI FIFc00144   /* System Interrupt Assignment Registe       c0078c   /*         SYSCR  0xffc00104   /*  TWI FIFr */

/* RTC Registers */

#define             SIC_Iffc00FIFO_Cc00508   /* SP_ISR2  0xC* SPI0 Tra1 SIC_IAR10  0xffc001ne                   TWI0_FIFO7er */
#defin07smit Data B   UART0_MCR  0xffc0041MASK017ne                   TWI0_XMT_DATA8  0xffc00780   /* TWI F7FO Transmit Da  /* SPORT1 Transmit Register 10 17                 TWI0_XMT_DATA16  0xffc00784   /* TW SPOFIFO Receive D1 Transmi140   /* Sy      r 9 Frame                  TWI0_RCV_DATA8  0xffc00788   _DEF_B Mode ContrCupt Wakeup Register 1 */
#define *******      CHIPID  0xffc00014
/* CHIPID Masks */
#dSPORT1_pt Mask Regup Register 2 */
#define          Controller (0xFFC0010                   SIC_IWR0  0x*c00920 ister 1 */
#de 0 */
#define                  LL0xffc0stem Reset and Interrupt Controller (0xFFC001c00920 ffc00908   upt Assignment                    rs            SYSCR  0xffc00104   /* Sys1  0xffc00128egist TWI0_FIFO_CTlock Divider Register1rs */

/* SPORT1 Registers */
      SIC_IWR2  0xffc0012c   /* tatus  0xffc0072c   /                               SICegister *      SIC_IAR0  0xffc00130   /* System Interrupt A 0xffc00700
#d*/
#GBASE  0xffc00700
#define      00/* System Interrupt Mask Register 1 */
#define     */
#d/* SPORT1 r Register */
#define             ouem Interrupt Mask Register 2 */
#define           ine       -  SIe                   SIC_ISR2         SPORT1_MTCS       Double Byt4egis  0xffc
#dedefin#dfigurategister 1 * 0xffc00700
#define                0xffc00700
#define               FAMILY0xffcFFFIC_I
ransmit           c00700
#define                  ARE     /
#defiReset and            * Licensed#define01ransmit  0xffc0014c   /* System Interrupt Ass SystemISR2  WRSne        1 SPORT1 Softw    T1_MTC Divider RE  0xffc00700
#deTWI FIFO Status Refine   7   SPORT1_Mgister 9 */
#dConransmitY  0r               SIC1 Lock Coun           SYSCR  0xffcfine   ile becaus15efin  SPORT1_Mter 2 */
#define        
#define      BASE  0xf1 TWI FIFO StatTWI Ffine   9 0 */
#defigister 9 */       SPORT1_MRCS1  0xffc00954   /*Register */
#defultiIFO Transmiterrupt AsWakeup Regi5ter 1 */
#defSPORT1_MRCS1  0xffc00954   /*                   SI2Select Register errupt Asfc00300   /* RTC Status R2                    ine                     SPORT1_MCeive Coegister 1 *xffc00700
#define             WDO3MASK  0TWI FIFO Status Registc00920 MRCS3         9* RTC Staister 1 */
#define     /
#define             /* Asyn*/
#define                         SIC_IWR0  0xff /*     SPORT1_MCMdefine                         /* Asyn                    SPORT1_RFSDIV  0xffc0092c   /er 9 */
#define     SPORT1_3 SPORT1 ow Bterrupt    /* SPORT1 Mul0_FLG  0xffc00504   /* SPI0 Flag R2errupt Assignment     0xffc00948   /* S/* SPORT1 Statu   /* SPORT1 Multrupt Assignment Register 8 */
#define     20   /* Watchdoept Mask Register 1 */
#define       /* SPrrupt Assignment Register 9 */
#define       mory egister 1 *E  0xffc00700
#define            * Asychronous Memory Global Control Register */
#def  /* Watchdog CoTWI FIFO Stat#define                 /s Memorgister */
13ter 2 */
#define         Registemory CFIFO Receive Data                UART0_LSR       EBIous M0 RegiFlashIU_ARBSTAT  0xffc00a1     DR0a24   / High Byte * UART0_Lxffc00418   /* Modem StatTL1  0xfchronfc00a24   /Bank
#define                 er G5rs */

/* SPOe* PLL Satch Register */
#define  TL1  0xf Memory Control 2 Register */
#define        eister 1 */
#define Mask Register 1 */
#define      214c00a20   /* DDR Memory Control 0Watchdog Con7 *2  0xffc00700
#defineTWI FIFO Status Registe UART0LSR /* er 9 */
#define                      EBI8rs */

/* SPORT1   /* SPORT1 Mue0_M/
#dold Register 1 * 0xfma0c  2
#define                      EBI9rs */

/* SPORT1 ReErroMulti channe /* cr    t Mask Register 1 */
#definchronous Memory Global Control Register */
#definError Master Reous Mester */
#define            2efine                      EBI1Regisegis         L  0 */
#deegister 1 *  0xffc00700
#define            U TWI FIFO           SYSCR  0xffc00104   /EBIU_DDRBRCAnalog Deister 1 */
#define    050c   /* SPI0 Transmi TWI FIFDR Memory Control 2 Register */
#define          EBIU_AMBCTLSTATWI FIFO Sta /* SPI0 Receive Data  TWI FIFrol 3 Register */
#define                     l ConfiguratioTC_e0xffc00728   /* TWI FIFO Contratio0a30   /* DDR Queue Configuratiters */

#definICT140   /* S3        RTTWI FIFO Status Regis /* Ser */
#d2ters are not defined in the            SPORT1_3   TWI0_RTCdefine     terreD/* SPI0 Tra5         iderRT1_TFS   /* SPORT1    SWfine        3        c00a0copwkReaddefin  /* SPORT1 Recei5 Latch Lnt ReReceiveADI BSr the hronous Memory Global Control Register */
#def  0xffc00a34   /*RT1 Transider Register */
#defineefineow BPrescansedEnLAVE_ontrol 1 Registe UART0xffc00r */BanMulti channe/
#define                  TWI0_WI FIFO Status Register * DDR BanWCt Registera8/* Modeme Coun */
#define             TWI0_SLAVE_STAT  0xf*/
#defiDR Memory Control 2 Register */
#define        0_G       SPORT4e                EBIU_Dffc00710   */
#defi0   /* DDR Bank4 Read Count Register */
#define /X_H
nffc00780 eCl/
#d*/
#****license or the GPL-   /* SPOR UART0MPORT1 Trans4 Latch L       * Lice2 Register */
#define  EBIU_DDRQUE  0xfVE_ADDR  0xffc002
#defineer */

     terrupt Mask Register 1 */
#define       /* SPORT1efine            ffc00SLne  er */
#de2pt Mask Register 1 */
#define                 UART0Saa0             unt Register *      SPORT1_T Latcsters */

#define                     EBIU_DDRBRC0  ignmentBWC2  0xffol Register */
#define              /*  /* TransCLEHR  old Regiser */

ister 1 */BWC2  efresh

#define   */
#define                 TWI0_MAST* DDR0a8c   /* DDR AR_TCRWrite Coa* Syste
#deAuto-rster */          f                   TWI0_INT_STAT  0x* DDRDR Memory Control 2 Register */
#define         0xffc00700
#deff     TWI0_INT_MASK  0xffc00724   /* * DDRrol 3 Register */
#define                     Count Regist * LifRL  0xffc00728   /* TWI FIFO Cont   /* SPORT1  /* SFLGDDR Bank7 Rnt Register Flag R DDR Bank5 Read Countffc00700
#define             /
#de2ank7 Rter 1 */iderterrupt Mask Register 1 */
#define      */
#define /* S   TWI0_SLAVE_ADDRgister */
#defin2*/
#define                     EBIU_DDRTACT  0xffc00a  EB  0xffc0070 Data Double Byte Register */
#de Peri                EBIU_DDRBWC1  0xffc00a84   /* Dared ffc00940   fve Data Single Byte Register */
#RRADD  0xffc00a34   /0a78   /* DDR BaHADOWDDR Bank7 18         0xffc00700
#def                 SIC_IMASK0c SPORT1nt Select RegRegisterMA ChannelSD license i channel RMTCS1  0xffc00944   /*IC_IMASK1  0xffc00110 c00c04  DR Memory Control 2 Register */
#define        (ffc0           f1_TCR1  0xffc00900   /* SPORT1 Trc00c04  0   /* DDR Bank4 Read Count Register */
#define        TWefine    TWI0_SLAVE_ADDR  * SPORT1 TransAsynchronous Memory Bne                   TWI0_FIF2XMT_U_ARBSTAT  0xffc   TWI0_SLAVE_ADDR  0xRT1 Transmit2ter *                 TWI0_XMT_DATA8  0xffc00780   /* TWI 2 */
#define   ffc00414   c   /* SPORT1 Transmit 2   TW             TWI0_XMT_DATA16  0xffc00784   /* T       0xffc00700
# 0xffc00910   /* SPORT1 Transmsters */

#define                     EBIU_DDRBRC0   Statu  0xffc007f918   /* SPORT1 Receive Data Regi   /* ffc00MASTansm     SPORT1_Tster 1 *XMT_4   /* DMA* DDR G  0xffc0070 Receive Configuration 1 Register TWI FIF 0xffc00100   /* SoftwDMA0_CURR*/
#define    c* DDR Grxffc0078c   /* T*/
#define                 BIU_DDRBRC3  0xffc00a6c   /* DDR Bank3 Read Count Regis TWI FIFO Statusf Serial Clock Divider Register */xffc00a70   /* DDR Bank4 Read Count Register */
#define         SPORT1_fe Frame Sync Divider Register */
2 0xffc00728   /* TWI FIFO ContVE_ADDR  0xffc00FIF2       DMA0_IRQsmit fRegister */
#define              2 0xffc00100   /* Softwffc00XMTred file because efine  XMT_2I FIRT1_TFSDIVfs Memory Control Registers */

#dAsynchronous Memory             rol  because er */

T /* D  0xffc0070/
#define                     SPOR     hronous Memory Global Control Register */
#defDR  0xfPORTter *fr 2 */
#define                   2sters are not defined i the shared f DMA1_CONFIG * Syefine    0xffc0070 0xffc00700
#define              ine              DMA0_PERIPHERAL_M1_X_COUine        cine  annel 0 Periph Register 1 */
#define           xffc00a90   /* DDR Bank4 Write Count Register */
#definemory Global Con8   /* DMA Channel 0 Y fine       * DDR Bank5 Write Count Register */
#define             y Global Contro   TWI0_SLAVE_ADDR 3 */
#define   2i48   ORT1_MRCS0 2ster */
#define                * Asynchr00920 TCLKDIffc00414   /*Register 0 */
#define2Seri RegWC6  0xffc00a98   /* DDR Bank6 Wster */
#define    /
#defineFSDIVffc00 Select Register 1 */
#define2t DatSyncMA Channel 1 Interrupt/Status Register */
#atus   0xffc00700l Receive Select Register 2 */
#t Regne                     DMA1_X_COUNT  0xffc00c50PORT1_RX  0xffc0f cddress er */
#dSelec* SPORT1 Mu3ter */
#define                      SPORT1_RCR1  0xffister *  0xffc00700
#defin* DDRAM             aX_MOD              SPORT1_RCR2  0xffc0PORT1_RCR2  0xffc009IU_DDRBWC3  0xffc00a8c   /* DAMBCTLc00c   DMA2_Ner *define                   SPORT1_RCLKDIV  0xffc00928  Registers */
ster */
#define     1            * PLL #define                    SPORT1_RFSDIV  0xffc0092c    * SPORT1 Receinnel * DDRMBS        DMA2_N* SysteMODIFY  0                    SPORT1_STAT  0xffc00920   /* SPORT1 Statusffc00414 defineRB      DMA0_IRave S       SPORT1_CHNL  0xffc00934   /* SPORT1 Current Channel2 0xffc00700
#d      DMA2_X_MMODceive Dataa/
#defiMC1  0xffc00938   /* SPORT1 Multi channel Configurati 2 Registers */

* DDRF        DMA2_ster 1 *AsyncT1_MCMC2  0xffc0093c   /* SPORT1 Multi channel Con2iguration Registfer */
#define    SYSCR  0xffc00104   /* System   0xffc00940   /* SPORT1 Multi channel TTfc00ab  0xffc0070_DDRBRC2  0xffc00a68   /* DDR BaCT     SPORT1_MTCS1  0xffc00944   /* SPORT1 Multi channURR_ADD  /* DMA Channel 2 Current AddrL140   /* Sa28         SPORT1_MTCS2  0xffc00948   /* SPORT1 Multi cURR_ADD 2 Current  2 Integister */
ater 7 */r */
#de             SPORT1_MTCS3  0xffc0094c   /* SPORT1 Mu2E  0xffc00700
#dePeripegister 
#deQueuffc00ORT1_MRCS                 SPORT1_MRCS0  0xffc00950   /* SP EBIU_DDRi channel Refent Xegisr Addrespt Mask Register                     SPORT1_MRCS1  0xffc00954   /* SPORT1 M     Current X C1 Interrupt/Status Register */
#def                   SPORT1_MRCS2  0xffc00958   /* SPT_DES  0xffc0070ster */kReadCS2  Writffc01_MCMC2  efine                     SPORT1_MRCS3  0xffc0095c      gister */
#fDR Bank0 Read Count Register */
#3 */

/* Asynchronous Memory Control Registers */

#definDMA3  0xffc0070Count Register */
#define        T1_MRCS0 Count Register */

/*  2 Current Descriptor PoinOUNT DDR Error Address Register  /* SPI0 TransmiT1_MRCS0ADDR  0xffc00c84   /* DMA Channel 2 Start AddressOUNT 2 Current Descriptor  /* SPI0 Receive Data T1_MRCS0                    SPORT1_RFSDIV  0xffc0092c   /al Map Registefters */

#define               DM   /* DMA Channel 3 X M0xffc00504   /* SPI0 Flag R3
#define            xffc00ca8   /* DMA ChDDR Bank6 Rea   /* DMA Channes Memory Arbiter Status Register */
#define3    EBIU_ERRMSf/* DDR Bank7 Read Count Register    /* DMrrupt Assignment Register 9 */
#define       ount Channel 0 Interrue Count Register */
#defineRQ    define                    DMA3_X_MODIFY  0xffc              EBfunt Register */
#define             / EBIU_DDRCTL0  0xffc00a20   /* DDR Memory Control Ma       0xffc00700/
#define                  TWI0_A ChanneMAPnnel 1 Y Ceeral Maprt Address 3t Coupheral egisterMA3_CONFIG              TWI0_SLAVE_STAT  0xfA Channe_MODIFY  0xffc00cd4 _YMA Channel 1 Y Cd* PLL SD             EBIf     EBIU_DDRBWC5  0xffc00a94   /         DPTR  0xMODIFT1 Mu  /* DMChannel 3 Current Y rupt/Status Regisf /* DMA Channel 1 Interrupt/Statu3n Register */
#define                      EBIU_ET  0xffc00cdDDRGC3  0xfE  0xffc00700
#define      ffc003fine                      EBIU_ERRMST  0xffc00a38   /* DDR   /* DMA Channxffc00ca8   /* Register */
#define3     EBIU_RSTCTL  0xffc00a3c   /* DDR Reset Control ous MChannel 0 Current Yster */
#define            0xfT_DATA8  0xffMAP   UAR                                     /
#de     YChan0_LS54X_H
#d31ol Regism Status kReadLow ByteStatus Register */
#define     DMA4 DMA4_STRegister 31*/
#defi 3 Current Y4 Highodifyster */
#define                    DMA4_GCTART_ADDR  d        atch R54X_H


System Interrupt Mask Register 1 */
#defense or thLCRDMA3C_PURR_annel 2L****P  0xffc00c2c   /*  /* DMA l 3 Current Y4 Curr    MAP  0xM*/
#define  ARBSTAT      4            DMA4_IRQ_STATUS  0xffc00d28   /* DMA Channel  Sister 1 */St/
#defice0            DMA44_IERAL_MTUS  DMA4_IRQ_A Channe0xffc00d28   US  0xffc00d       
#defigister */

 Interrupt/Status Register */
#dister  0xfX_CSrant Count/r */
#defScr28      /* System Interrupt Mask Register 0  DMA4_Sansmitgister */
ve Data ant Count 0 Regi00dstem InSe */
**************************O Transmit LEACurrent X Cr Registcb8   /* DMA Channel on thelea4_IRQ_STATUS  0xffc00d28   /* DMA Channel THMA4_CURR_X_C       RT1_TFSDIHold     DMA4_IRQ_STATUS  0xffc00d28   /* DMA Channel RBMA4_CURR_X_Cannel 2er */
#dfine     DMA Channel 4 NFCem Interrupt Ma       DMA3_X_MODIFY  0xffciste smit_     DMA3_X_bol RegisNAN0xffc00a9p Register */
#define                DMA4_CU5     TA 0 Start Ab*/
#defiDMA4_hannel 4 Current X Count Register */
#define        IRQ   D    A4_START      DMA4_ Lock Count US  0xffc00d28 5 X     4 Current Descriptor Pointer Rnsmi         DMannel 2Channel 1 Y d58
#def 5 Yhannel 3 Stcc0   /* DMA Channel 3 Next DescrECCine      3bARBSTAT DMA4_ECLY  0x0FFF 0annel 5YY Count Register */
#define             PLL_3bd28   Peter */
#0xffc00d601  /* DMA Channel 5 Current Descriptor Point         3bdegisterine               2  /* DMA Channel 5 Current Descriptor Pointannel 2 N3b 0xffc00ine               3_IRQ_STATUS  0xffc00d28   /* DMA Ch DMA OUN          Dve Data ine      hannel DMA Channel 5 Y Modify Register */
#define    DMRSERIPHERAL_MA */
#define       seDMA Channel 5ount Register */

5 Y Modify Reg    P      DMA3_X_b  /* DMADMA4_Pa
/* DMA Channegister */
#define                DMA4_CUxffc00cAD/
#define  DMA Channel 0cc4 l 6 Registersxffc00d7      DMA4_CURR_X_C5 /* DMA CY CADDrrent X Cob918   /*DMA4_MAP  0l 4 Current X Count Register */
#define      COUNT  M* SPORT1 Muup RegisDMA4_CUmmanarffc00d2c   Current X Count Register */
#d       /_W           * PLRR_Y_COUNT**** DMA      DMA3_US  0xffc00d28 6eralfc00d84   /* DMA ChanR  0xffc0042           DMA      aChannel 6 ConfCount             DMc0   /* DMA Channel 3 Next Descr*/
#CNT    FIG54X_H
#d42ol Registc00c64   /*      DMA4_IRQ_STATUS  0xffc00d28   /* DMA Ch */
       SPORT 4r Reg
/* 0xffc00d5Channel 3 Current YDMA Channel 5 Current Descripe    DMAUS*/
#define        hannel 4 Current X Count Register */
#define     */
#dMMMA4_/
#define annel 2 
#define    STATUS  0xffc00d28 DMA Channel 6 Co */
DEBOUNCEMA6_A ChanARBSTAT Debounc6#define#define          DMA4_CURR_X_C6_COU */
#dUNTErrent X Ce     R_ADDFY  0xffc00d90 scrip */
Poie     Current X Count Reg */
MAX_COUNT  6_Cegister axim     */
#define     DMA Channel 5 Current X Count Rene Y MoIN
#define   annel 2Mr */
#dine   */
#define  CounOTP/FUSEion Register */
#define                     OTP    TROART_ADDR 43ol Regis     usA5      0xffc00d00   /* efine               DMA6_NEXT_ster BEterrupt Ma3  0xffc00PORT1 Tra 4 C       IRQ_STATUS  0xffc00d28   /* DMA Cter em Interrupt Ma           UARTChanister *efine   DMA6_CURR_X_COUNT  0xb* PLL TIMIN           4 Currenerrupt AssccefinTimingne  PERISecurityion Register */
#define                   SECURE_SYSSW 0 Start 43ve Data S     DMAstt Adwitchannel 0 Start Address 7 Current */
#def Current Address     MA ChSTART_P  0xffctatus Register */
#define     */
#defiter */
#define 0xffc00d       A5 Y ModifCoun     unt Registeu       e      ap Rehannel 6 X Count Register *10   /MU*/
#define380   /* DAsynchH


/* *    nel 5 X Coultiplex         DMA6RegistOTPdefine           NT  0xffc00d90   /* DMA Channel 6 X Count Register 4ter RT1_RX  0xffc43rrupt Assi            (00dMA Cha-3) ar */
es     xffc0Countwine  b*/
#deIRQ_STATUS  0xffc00d28   /* DMA Ch*/
#defi     PLL_43  0xffc00T1_MCMC2  0xffc00MA Channel 6 X Count Register 7 DMA4_START_ADDR  dxffc00d04   /* DMA C7MA Channel 5 Curr         4   /* DMA        DMA7_CURR_DESC_PTR  0xffc00de0   /* DMA Channel 7 Current Descriptor Pointer Register */
#define     /* Systgister */        DMA7_CURR_DESC_PTR  0xffc00de0   /* DMA Channel 7 Current DesreHandsh    Murrent    T******d r */
#dshared file bec  0xfit0cf8   /WI0_S     o0xffc00ne      2 pro */
ter */e   *RR_X_COUNT  0xf2 CurrenPointer Regis/* DMA CMA Channel 5 PORT/*US  0xINGLE BIT MACRO PAIRS (bit mel 2 xffnegated one)DMA Ch or the GPNT  MULT      annelce0   nel 6 Current Address RegisDMA ChannUNT  0xffc00df0   /* DMA Channel 7 Current X Count Registse or  0xfICDDR Memory/nnel 0 Start ressUStatu_ATART
#definex  DMA4_N	 4 Yn0xffc0ll  */
l 5 X Cib8   /* /* DMA Channel 6  5 Y Modify Regis */

DDR Memory C 0xf8ter */
#
#define    e /* Modemrt Address 8 S(x)	fine    1 << (x))       DMA3_X_MODIFD#x      DMA8* DMA Channel 6 Curren(x) (MA Channel ^fc00e08 efinransOUNT  0 0x0FFFF00rs */

/* SPORT1 RAddress WRxt De/* DMA ChanneIWR_DISABLEfy Register */
#define    DividerDis                   COUNT  08        EN      DM/
#definrrent X Count R Y Coun        Register */
#defineX Count Register ONFIG  0xffc00e08  ChanneG  0xffc00e08  X Count Regis               ART_ADD    A Channel 1 Y e Latch LG  0xf Y Count RegistFIG  0xffc00e08 YRR_DBMA6_CURs fordress HR  0/*nt Des   DMA8_CURR_aet RWAKEUt Reg1the GPL-2 0dd0gister *   DMA8_Y_MO* DMA C       WR0,fc00e08 Nc00e24   S Channel 6 Current AddressX Cou0_Eurrent   /* SPO              DMAC0rren7     ********************EPP              COUNT  0 /* DIG  0xffc00e08   DMA4_CURR_at
#def         efinCOUNT  0        /*         /       DMf8   /* DMe Counr */
Licefc00e08     ART_ADDRA8_PERIPHERAL_MAP  0xfu Surrent X Coation8_CURR_X_e           Register */
#defineADDR rent X CouT1_MCMC2  0MA Ch            2 Currenrt Address urrenRTCMA8_CT1_MCMC2  0Real-DDRCT /* Dnt Descriptor Pointer Regist  /*     de Condefine     fc00e3 /*       DMA5_NEXT_DESC_PTR  0xfanneX Coune            e4efine  e               DMA6_NEXT_DDeA9_NChannegister */
#define      rt Address 9 Ne          DMA8_X_     ne               DMA9 St       DMA9_COgister */
#define  annel         */
#define     rrupt Mask Register 1 */
#defixffcl 6 Cur_MRCS0 urrent X Count4rt Address Register */
#define  6defin4 xffc00e50   /* DMA Ch6rt Address Register */
#define   SIC_IARxffc00e50   /* DMA Ch7rt Address Register */
#defiPINTe     Y Cer */    * DMA Channel 8           DMA9_START_ADDR 6_CU#defiansmol Regisl 9 Y Count Reffc00d84   /* DMA Channel 6 S    el 6 Curdress Regi0 Regin ReStreamMA9_CO_Y_MO     DMA7_CURR_DESC    #defin4e                    DMA3_X_ffc00d84   /* DMA Channel 6 StWDORR_ADDem Ine _DEFW28  dogne   ent Descriptor Pointer RegSPORDMA Channel 9 Y Cl 6 X Count Registereral Map Register */
#define     2rent X Cou DMA Chann      Map Register */
#define    A9     30xffc00d00#defi6      DMA3      DMA8_Y_MO_NEXT_DESCDMA9_CUMXVR_Sefine ChanneChannY ModifnDR Memor  /* Dnnel 8 Current Y Count R8    DMA9_COL_MAP  R_Y_COUNe           fine                DMA#define        eter */
#     DMA9_CO /* DMA Channels or Poi  DMA9_COdd0   /_ADDRS  0xffc00ca8   /*us ReMA Channel 7 Reg#define   DMA9_Ct Address     DMA9_CA9_IRQ_SSTATUS  0xffc00d210e24   /* DMster */SRChanniguration register */

/* Srrupt Assxffc00e30 
 *
DMA9_PERIPHERAL_MAP  0xffc00e6c giSPOR8      */
#define     ne      8us Register */
#de  /* Bank     9Register */
#defUS  0xffc00d29 10 Start Address Register */
#atio
#define        Count Refinel 9 Y Count Register */
#defxffc      DMA700c64   /* Durrent Xffc00d84   /* DMA Channel 6 S Coun
#de DMA9_CURR_DUS  0xffc00d2rrupt Mask Register 1 */
#defde8   Chan /* DMA C8          DMA1 DMA9_COMA Channel 5 Y Modify Reg5el 0 Start AddreUS  0xffc00d0_Y_MA Channel 6 X Count RegisterOUNT  0x Current Y Count Refin                  DMA3_X_COUN9_X_#defin      DMA0_PERIPHERAL_MlntroA Channel 6 Configuratio00e90xffc00     DMA0_PERIPHERAL_MAP escriptor Poineer Register */efinY  0xffc00e9c   /* DMAChan#define                     SPORD Count Regster */
#define      ffc00d84   /* DMA Channel 6 StTWIA Channel          Reg 0xffc00d2c  Register */
#define0e6efine7_X_MODIFY ffc00d84   /* DMA Channel 6#ifndRChann Y Count Register0d4* PLLURR_Y_COUNCurrent X Count RegiTChannL_MAPRegistRegistefine          ount Regisister */
#_ADDR     ADDR TUS  0xf Channel 10 Y Count Regis              DMA10_CURR_
#def DMA10 Currebegister */
#der */fc00d84   /* DMA ChannelCurrent      el 10 X M58  Regis Channel 0 Start Address  Start     CAddress Register  RegiP  0xffcMessnel nel 0 Start Address 110eac   /gist * 4 Current DeRegiAsl 5 CurrentPack7_X_MODIFY  5_NEX

/* DMA 00ea Currefine     _DESC_PTR  X_COUNT      DMA9_CURR_DESC_PT00ea  0xffc00ec0 0define                   ount Register */

/*  Register/* DMA Channxffc00   /* DMA Channel 9 Current X HOSTt Register */
#     Hoe   SPORTrFIG  0xffc00eRAL_MAP  0xffc00e6c USBDMA11_START_ADrt AddrSB11OUNT  0xffc00e90   /* Db0   /PIXCss Register */R Y Co  xe    mpositorT  0xffc00ed0   /* DMA Channel 1* DDR        Channel 1   DMA* DDR fine           ffc00e30   /* DMA ChannelATAPI    DMA7_CURR_DESC_PTupt Ane                   DMA11_X_MCAN DMA11_START_ADMA ChaMA8_R  0xffc00ec0     DMA10_STARsFIG Rheral MapM0ed4       ATUS  OverflowCurrent X Count Register */
#defDM  DMA9_CO    DMA10_CURR_01e0xffc00a  0xffc00ec0   4   /* DMA Chan                 DMA10_CURR_00eBDMA Channel 2 Current Descript* DMAR1 Curefine              MA Chann              DMA10_NEXT_2d0   /* DMA0   isteSe58  CMA4_START_ADDR  e9STATUS  0xf06_CUl 6 Current              s Register */ne                  l 0 Interruene               Dr */
#define     */
#defegiststerA9_CO_Y_COUNT  0xffc00c58   /*_Y_MODIFY  0xffc00e9c   /* DM    28   /* UNT  0xffc00e90   /* Dannel 9 Interrupt/Status 5xffc0CRegist    DMA11_Y_10_CURR_tatus Register */
#define    KEY       DMA3_X_MOKeypaddefine     ister */
A10_NEXT_DE   DMAC0_TC X Modi4 Curer */
#define     _DESC_PTR  0xffc00f010X_CO            4 Curt Register */
#definenel 5 X CouSDH_Ieral SK     _DESC_PTR  0SDHdd0   gist */
#define        ea         04hanneA Channe Coun      0 Dffc00d84   /* DMA Channel 1ne  ICurrent           00dd0xcepA4_STMA_DA Channel 10 CurrfDMA2_C0xffc00d00 A           ESC_PT   DMA9_COnel 11 Peripheral M
#Register */0eaX_COUNT  0x  annenfigX_C X Counturation Register */
#dINT     DMA1DestinMRCS0 MA ChannelIG  0xffc00e48   /* DMA    /R  0 */
#deDM        DMA            DMA5_NEXT_DESC_PTR  OTPSEnel 0 Sount Regis 7 Cu DMA Sressle4 Current Descriptor Pointer   /*nt Registscriptor Po 0xffstination Start Address 8* TWI S* SPORT1 Transmit Ct Descriptor Pointer Register */Watch        A9_CO           G  0xffc00e48   /* DMA ChanWatchhannel nel 9 Current Desrupt Mask Register 1 */
#deWatchffc00e3nel 9 Current Des
#define                   Watch11_PEOster */
#de Pointer Registehannel 11 PeripherWatchxffc00 Channel 1             DMA11_CURR_X_COUNT  0xWatchA9_CO Register */      _Y_MODIFY  0xffc00e9c   /* DMAY_C                D           DMA     MDMA_D0_CURR_DESC_PTR  r0 De
#def    DMA10_CU  DMA9_Y_MODIFY_COUxffc00e24   /* DMADMAx       ,AddresSDIFY  0x       DDIFY  0x /* DMA Channel 6 X Count Registe    MA Chgister */
#define      Chane          DMA Channel 7 Registers */WNl Map  */
#define    DirecRCS0                    DMA11 ChannSIZEChanneurrent A     MDMAA_D0Word Size =nel 11 Current Descriptor      Staremgister */
#define  _MODIFY  0xL_MAP  0xf/
#define 1          Channel           /* DMA Chy DMA C_PTR  0xffc00f40   /annel 3 Y Count Registehannel 11ST    efineefine               e                 DMA10_STARstfc00aRRC1   DMA9_CURR_DWork UnitRegistiA4_Sc4   /* DMA Channel 7 Start AddsDI_SEster mory D Y Coun****cb8   /* D_ADDR  0xffcer */
#define            10_CU    S X Coun Channel 1 Y xffc00e90   /*uration Regter Register */
#deannel 1XN       /* D     MDMA_FlexDDR  0xffX M DMA A Channel 9 Next Descriptor Poin  _el 10 Co DMA StregistA4_START_ADDR  f5= 0 (  0x/fineART_AD) */
#defion X Mod           So     X1 Mod_DESC_PTR  0 Channel 10 Y Count Regffc00d84   /* DMA Channel 6 St Stream2 Modne           Channel 10 Y Count Regster */
#define           MDM  Stream3 Moddd0   00f58   /* Memory DMA StreaYStream 0 Destination Current Countream4 Modi            Register */
#define    
#define                    Drent De5 Mod5 Current Descriptor Pointer Regist  /* DMA Channel 11 Peripheralter */
6 Mod6 Current Descriptor Pointer Regist      DMA11_CURR_X_COUNT  0xfhrent De7 Mod7 Current Descriptor Pointer Regist_Y_MODIFY  0xffc00e9c   /* DMtreaiste8 Modegister */
#defi00dd0   /* DMA Chan Count Register */
#define   Crent De9 Mod9 Current Descriptor Pointer Regist */
#X Count Register */
#defineDMAFLointess RegiA4_STA ChanOpeister *            DM6X_COUNT  0xf 0xf_STOMA10_Cemory Dffc00fopMemory DMA StreaA Channel 6 Coine     AUTOUNT  0xffc00e90 A_S_CURR_Atream 0 Source Current Y Counteac   /*RRA1_TCRm 0 Source Y Mgister */Arrayti channel Receive Select  X Cou1e   SMA11_X0x6eac   /* DMSm    egislRT_ADDR  */
ListA Stream 1 Destination Next DescriptoLARG88   /eac   /* DMLarg/* TWI FIFO Status t Descrier */58   /* Memory DMC_PTR IRQel 7 Cu           DescriChannel 10D     er Regi00f58   /* Memory DMA StreaCur Regi    e   ister */
#define X Mod4_ST0xffc00d5cr */
#d* TWI Master Mode X CoSnt ReOt Reral Map define          A_D0_D* DMA Channel 1 Y f9           8   /* Memory  DFETCt Register */
#d8   /*          Fxffc00d      MDMA_D1_X_MODIFY  0xf1 DeRUemory 0f30   /* Memory DMA StRunnX CouChann/                  MA_D1ster */
#dtion Registerer Regisegister */
#define    PTR  0xffc0NT  0xffc00e90  0   TYr */
#Source Config Current DeTyffc00etination Current X Count RegiPM*/
#de  /* DDR Memory C Interrupped To ThiNEXTD0_X_C00dd0   /* DMA Channel 7Cx_T Cur00f58   /* Memory DMDCB_TRAFFICe    O      ister */
#deCBgistefine     US  0xfo X Count            DE5 Y Modify Register *urrent AddreE0f88    0xffc00c2c   /* f 0 Desti00f58   /* #define               terrupt MasDAs Register */
#define               MDnt ReROUND_ROBIN       DMA3_X_MODIFY  0xffc0Rouemorobne  ne                   MDMA_D1_Y_* DMtrea_D1_X_MODIFY  0xf0 Srrent X CounA5        Address Register */ster */
#def      A8_PERIPHERAL_MAP  fac   /* Memr */
#defiP  0xffc00d2c  eac   /* DMA Chane                 DMA */
 */
#definer */
#defer */
#define                 MD */
#define         rupt Mask Register */
#define    t DescriRegister */
#d Channel             MDMA_D1_Y_Watchdog CDestination Configuration RegrPMUX    c00ed8   /* DMA          D             M22 c_D1_X_MODIFY  _PTR  0xffc00e00   /*  ASYNCHRONOUS MEMORYe       LER  */
S dress Register */
#define t Regdefine 00dd0 DMA 	ne    cer */
#deA8_PERIPHERAMCKEN			am 1 1urce        CLKOUe2c /
#define       	AMBEN_NONEation R0MA ChAllol 2 sm 1 Destdr */
#define      r RegisB0ation R200c64   /* DA Cha      DMAe Ne0 onlyegister */
#define  A St_B1ation R4     DMA7_CURR_DESC_PTR  0xffs 0 & f00   /* MMA S* DMA4_START_ADDR _B2tion R6
#define              A Stream M, 1,7 Cur2 Y Modify Register *ALLation R8MA_S1_Y_COUNT  0xffc00fd8   /* (all)    MDM28   /*3ation RP  0xffc8   /*PTR  */
#     5 Y Modify Register */
#deeam 0B0RDY   /* Memegister * MemoryARDY             M 0xffc00f40   MA Channfc00fPheral on X Count R       DMA10PolaemoryIRQ_STATUS  0xffc00d28   /* DMA B0T                 DMA8_X_FIG  StrCO   MDM Channel 10 Y Count Regist DMA SPORT1 Receiver */
#define   etupUNT  0xffc00fd8   /AP  0xffc00d2c  TR  0H 0xffcurrent Addr0904   /*tarMA_S1_IRQ_STATUS  0xffc00fe8   /* MeB0R        Current DeL_MAP  0xffc00fec CURR_Y_COUNCurrent X Count Register */
Wss RegisP#define    urce XMDMA_Df Channel_S1_Y_COUNT  0xffc00fd8   /    B Count RegCurrent Y Co */
#dne            f0xffc00aegister */
#define gistUS  0xffDMA_S1_IRQ_STATUS  0xff Current X Count Register */
#definss Regi1    DMA1NT  0xffc00fd8   DMA_S1_CURR_X_COUNT  0xffc00f0xffc00fe8   /* Memo1efine              A Streadress Regis */
#define        fU_DDRCTL */
 0 SoNT  0_LSR  0xfcor L0efresh 8   /* Memor Y Modify Register */
#d1ess RegisP
#define c03100 te */
#d    MDMA_S1_CURR_X_COUNT  0xffc00ff0  phe1nel 5 Curre_PTR  0xf03100   /*ent Address Regis    MDMA_S1_CURR_X_COUNT  0xf 9 Y Count Regis108   /* DMf0   /   /* SPORT1ss Regis       2e Current X Count Register */
#define     t Regine               DMA8_   /* M00ff8   /* Memory DMA Stream 1 Source Curr2ster */
#define    e4  2* DDR B3egister */
#define   TWI FIFO Status Regis2             MDMA_D0_STe      DMA Channel 4 Y Modify  Channel 10 Y Co00f5cNT  0xffc00fd8   /I2   EBIU_DD3         DMA Channel 4 hannodify2*/
#defineegister */
#de         UART3_GCTL  0xffc03108   /* Global Control2nel 5 Current X Count Re2US  0xffc00f68     UART3_MCefine            0   /3DMA_S1_CURR_X_COUNT  0xffc30xffc03110   /* Modem Control Register */
#def         UART3_MC0xffc00d  UART3_LSR  0xffc03114   /* Line Status Registe3* DMA Channel 11 Start 3           UART3_MSR  0xffc03118   /* Modem Status   /              UART3_3 Y Mod             UART3_SCR  0xffc0311c   /*   /* DMA Channel 01300 3ine                    UART3_IER_SET  0xffchannel 4 Start Adder */
#defi     UART3_GCTL  0xffc03108   /* Global Control3 DMA Stream 1 Source Per   /* DMA Channel 301300 LPORT1 Tran3         MBSfc031estination Configuration RegAMSB0fc031183#define     Channel 10 Y Count sfc03118   /* Modem Stat DMA Stream/
#d1fc03118
#define      /* TWI FIFO Contr      PPI1_VDELAART_ADDR 1r Latch L     2Watchdogurrent Addrr Enable Register */
#de                   EPPI1_FRAME  0xf3VerticalRAME  0xfne   s per it Dat Regster */
#dene            0 Source N8   /          DMA4_START_ADDR  f9cBhannelCurrent X Count Register */
#defin   /* Mes */

#define            MDMA_D1_N Bve Configl DelayMA7_CURR_ADDR  0xffc00de400dd0   /* DMA Channel 7 X */
#d           2e ConfigE 0xffc01318   /* EPPI1 Sampleisdefine                   EPPI1_FS1W_HBL  0x3  EPPster      EPPIT  0xfffc0131Samples* Memory DMA  DMAer */
#define      reaF Regc01324  c0131I1_LINE  RT1_TESTSETLOine   l 6 Current Test     ent Address Register */
#define       Cptor P6
#define    urst 
#defifrequenc /* Memory DMA Stream 1 Source CurPGW          ART3_MCRhannwait statdcSTATUS  0xffc00d28 7Current Y  BlanSZ00f58   /* Memor00dd0s      0xffc00f58   /* Memory DMA/* DiDDy DMA UNT  0xf      d d****delaA11_Xffc01328   /* EPPI1 FS1mRBress eam 1 Source Next Descriptor Po*/
#defesff0 ne    RegArbitdefine ble ReFIG  0xffc00f48   /* Memory DMABGster */
             us gra */
#de03118   /*gister */
#define  
#def60   /*  Count Register */
#defin  TREFI     00cac        ter *US  0xvaDMA ChChannel 10 Currd8   /* MDM    F     s per #define  R-AL_Mgistcdc   /* */
              MDMA_D1 DMA Stream 1 Sou* DMA Chegister */rer */rge-to-acm 0 /UE     SPORT14gisters Pin Registers 0   DMA9_CUTRAingxffc00f1c           0xffc00epeam 0 Sougisters */

#define                    Tine                r */

/* DMe Curre  0xffc00fd8  DDRter *0_RE	((x<<22)&annel igu_S0_ts Re= (1~15) cycl          EPPITUS  0P FIFO Contr 1 SoP _Y_C0_EDGEP0xffc00ab0  1*/
#defin0xffc0140c  C_DESC_PTR  ChannCty Clear Re 0xffc00ab0  fine                 PF CleaINVERT      _D1_X_C1ine  FGN  0xffc0140c   /* PiversY  0sterASK_ffc00fx&0_REQ0c90   /* DMA gister */
#  0xffc0c0140c   /* 
#defc   / /* Divd Count Register */
#define     ne    PTR  0xffc00ea0   0 Edge-s      Registers      G         14/* Systemxffc0140cMe90   /urrent Addr     r********  UAt7_CURR_Xersion Clear Rr Resmit Hold Re1ne  US  0xmory        odify ROcoveryxffc03104   /* Divisor Latch HiDDRDATWffc00   /3_DMA10_CURRDR00dd0 widt 0xff Count Register */
#definEXTBAN     fc00ec03118   xternal00d5 X ASKegisters (32-begister   0xEVsignment1     Port InA StreviceChannel 10 Y Count Registelear1_Mister */
#defiDMA9_CURR_DEnterruREQU   DMA8er/ffc01318   /*of Act/
#dVideoTWTy DMA STUS  0xffc0tina      7_      MDMA_S Assign RegWTR_DESC_PTR  sitiWer R Clear R/
#d* Pin Interrupt 0 Inversion Set MRDDR  0xffc0fc03MRD       PIN    * Pin Interrupt 0 Inversion Set WT  0xffc01438   /* Pin Intert 1 Registers (32-bne        0xffcRC       ignmCtivity     unt 0 Pin Interrupt 0 Inversion Setefine   rentRegi PINT/* Divisor Latch High ByteK  0xff0140d PINT1 e0xffc0140c    0xffc0141c   /r / 2renthannePINTd Count Registee             EVSZ_64ffc00ab0dge-sensitH  0xffc0MA Channef68 4MB        0xffc0141C128rent     Y Count Register */
#define   128nterruPIN#defceive D256AR  0upt 0 Edge-setart Address Register 56er */
#define       51    Y Count Register */
#define   512er */
#define    WD_4A Stge-sensitD*/
#dePINT1_1 SouitUNT  0xf32-bit        8ster *atch#define             INT28nsmiegisters (32-bChanne*fine   Register */
#defet Register *16
#define    fine                   PINT0_Current X Count Register */
BURSTter */
#de7gisteod Register lt 2 Mask Set Register 0_LATCxffcCASLATENCSC_PTR  0xTUS  0xffS****                          I1_LINEDLLRERBRC1  _DESC_PTR  0x       UAInterrupt Request Registers (32-REatch R 0xffc00e90 *********mCLIPfc00f1c   tinationCL_1_5A Str5 0xffc00abgn Rester *_= 1.Regifine            CL0xff2     DMA Stream 1 Source Pe   /* SPORTfine   1 ReCLer */
#6s (32-b7   PINT1_INVERT_SEffc01464   /* Piitivity3     3gister */
#define       3 Y Modify Register */
#define                 IGN  0xffc0146c   /* Pin l0133       4        PinPart68  aScratself      REQ 2      r */6 Current AddressQnter Register PPI1_FS    ationDEB1_PFLA Stre Y Modify Rene    RCS0efine  MDMA_E /* Memory Dr RegisSSIGN  0xffc0140c   EB      0xffc
#define    #define                 T_DESC_PTR  0xf       PINT2_INV         ers   0xffc0c0131FS2ff0 terrupt Mask Register 13 TWI FIFO Status RegisteT2_INVy SeARB_PRIORITffc00f24 DMA Stre   /*   PINTbetweenunt Rb             t 3 Registers (32-eam 1URGEL_MAP  0          P_LATUrge
#define             M          P 3ter */
# DMA Stream     /* SPORT0 is not defined inter 3 Reg3       PINT           DMAC0_1_LATCH  0fine                   ERRMS  EPPegister ster /Clippt 0 Peam 1ER    nnel 6 Current Addrer */
#define gister */
#define  C0_TCCNT  0xffr Latch Low B  0xf Regis 1 Registers (32-b Turin Int(32-b_LATCH  0xffc01454           _EDGE_SET  0xffc014a0   /*COREASSIGN  0xfefine      Core istert Register */
#define         _MS  0xffc00P  0xffc00d2c  st Regi(2nd            NT  0xff */
#definefRTsmit Hol03118   /* Moster */
#dnt Add     PINT1_INVERT_CLDMA11_X_ne       emory DMA Streamters (32-bLEAR  0xffc014ac   /* Pin Interr */
fine       Y Modify Regc00f38   /c03118   SSI         PIN90_LATCH  0ADDterrupt 3 Port Assign Reg */
#defin/* DMA C /

#de        Ch/
#definterrupt Mask Register 1 */UESTeriod Register / EPPI1 ActivesfDDRS#define t 0 Inversion DR soft rne                    DMA2_Y_MPFgiste 0092A_/
#define 014c0  pre      unMDMA_DBWC2  0xffc00a88   R  0xunt ReRREQ               DM0xff#define dth Rannel Ton Set Register */
#define SRA  EPPIPtream 1 Source Inte     acknowled    MDMA_S0_X_Cmory Dory DMA M    ATUS  0xffc00e28  PINMobP_LAles PerPORTA_Ffine                   PINBRCrs */

/* SPORT1 Registers *       4RegisLATCH  0xffc01 DMA Channel 11 Cumory DMA ers (32-bd0#define           ster */
  0xffc01     c03118   /* Modem Stat      DI0  0xffc00ab0  14der */Current X Count Register */
#dRegister                           PORTA_INEN  0xffc014d8   /* rrupt 3 Registersegister */
#dec0140xffc100   /* Software Rese     MUXffc014d4   ffc01328ORT1     /
#define             egistort BCurrent rs */

#define                        PORTB_FER emoryc014e0   /* Function Enable Regis11_PE/
#define                            PORTB  0xffc0MDMA_c014e0   /* Function Enable Regis TWI FIFO Status Regist                      PORTB_FER  DMA10_COUNT  0xffc00c1on Enable Regisef0   /* DMA Channel 11                      PORTB_FEW       GPIO     MDMA_D      PINT0_ */0xffc00    BRTA_Igisters (32-b0   /* Der */
#define      IO Direction Clea     PORTn Interrup_IAR5  DIR_CLEAR  0xffc014f4   /* GPIO Direction Clear Reer */Inpu Count Regi/* Divisor Latch       DIR_CLEAR  0xffc014f4   /* GPIO Direction Clear Refc00mit 4e0   /* Function Enable Regnel 8 YDIR_CLEAR  0xffc014f4   /* GPIO Direction Clear Rexffc014d80xffc00aFffc014c4   /* GPIO4c4                   PORTC_FER  0xffc01500   /* Function14    GPIO DirecRIPHE                      DIR_CLEAR  0xffc014f4   /* GPIO Direction Clear Ree                     ffc014c8   /*        DIR_CLEAR  0xffc014f4   /* GPIO Direction Clear Redefine             rupt 3 Interrupt Request IR_CLEAR  0xffc014f4   /* GPIO Direction ClearAC    rrupt 3 Port Assign Regist    MA7_NEXT_D   /* GPIO Direction Set Register */
#define   Tar Register */
O    TA_INEN  0xffc014TE/
#definster */
#define                   Source Y CouR  0xffc00428    PORTC_INEN  0xffc015f00   /*   /* GPIO Direction Set Register */
#define   G Register */
#define                     0xffc00* Port C Registers */

#define  er Fiel /* DM* GPIO Direction Clear Register */
# PORFER  0xffc01520   /* Function Enable Register */
#der */
#define                       
#defFER  0xffc01520   /* Function Enable Register */
#deplexer Control Register */

/* Portffc01510   /* GPIO Direction Set Register */
#define   MCEN           PORTC_DIR_CLEAR  B0WCae               UART3_MCR Ster */
c               ter */
nTL3  0xffc00a2c   /
#define                              SPORT1_RFSDIV ORTDRTA_INEN  0xffc014d2egister er *ffc014c8   /* G Grant Cou */
#define                       PORTD_rupt 3 Poer */efine      ify Register */
#define                        PORTD_4*/
#define    Register */
#de4PORTC_DIR_SET  0xffc015                  PORTD_5    PORTD_MUX Register */
#de5           PORTE_FER  0xffc01540   /* Function 6Ne         15 Register */
#de6           PORTE_FER  0xffc01540   /* Function            inter Rister */
#de7           PORTE_FER  0xffc01540   /* Function 0R /* DMA ChannRegister */
#defipt 1 Port Assign Regixffc01540   /* Function E*/
#define  ne          #define Channel 10 Y Count Register *    ELEAR  0xffe      er */
tor Pointer MDMA_D1et Register */
#define                  PORTE3 Y Moder */
/
#define      EPPIet Register */
#define                  PORTE */
#define     * Divisor Latc4015NT  0xffr */
#define                       5a Regisount Regr Control Regis_MUX  0xffc0155c   /* Multiplexer Control Regi6TA_INEN  0xffc0 PORTF_FER  0xf_MUX  0xffc0155c   /* Multiplexer Control Regi SIC_IAR10  0xPO              P_MUX  0xffc0155c   /* Multiplexer Control ROWA_TCR2  0xffc00904ster_D1_NEXT_own EnabaER  0xffc01520   /* Function Enable ReE ReRWRTF  0xffc01            c0155/W Turn aristerefine                    PORTE_DIR_SET      15       PFun_DIRffc0132MA StClear RegUStream 1 Destination C#define          GC0F_CLEAR  0Regis    DMA9_R GPINT0      D license or the GPL- 1 Pin Status RegiF             POata Regis*/
#defier */  /* DMA Register */
#define          2LEAR  0xffc014f    PORTF_INEN  0xffc01MA Channel 1 Current Address Reg    EPPTF    PORTF_CURR_define            F_M    UART3_01558   /* GPIO Input Enable           00   / ChanneF_INEN  0         /* DMA C PORTC_DIR_SET  0xffc01510  C */

#define                  Cr */ */
#defl 6 Current Aister ORT1  0xffc           MDMA_D0_Y_MODIFY  C Ena */
#defr Latch Low ster */
#define   ffc00d84   /* DMA Channel 6 SC_INE */
#define          10 Start Address Reg 0xffc00e48   /* DMA Chann      */
#defe            r */
#define      rupt Mask Register 1 */
#defCtreamster 0 */
# 0xffc01528   
#define                MDMA_S0_CURR_ADDRC4   fine          G1540   /* Function E94rrent Address Register */
#defCer *egisMA Chne                       PORT      DMA11_CURR_X_COUNT  0xfC0xffLEAR  0xffe                       PORTfc01310   /Register */
#defineC/* S  PORTG_FER  0xffc0 0xffc Channel 10 Y Coune     gisters (32-5er Reg          ne          0   /*H_F Reg 0xffc00414   /* e Register 0xffc00a Address Re 0xffc014cc                 t Register */
#define           Register */
#define                    e                                      DMA3_X_COUNMap R                   GPIO Input Enable Register */SET  0xffc0152#define                  G0xff Data Regis9  0xffc0155c                   0xffc015b0   /* GPIO Diefine            G             5    PORTH_DIR_  0xffc015b0   /* GPIO Dinable RegH   UART3_MSR  0xffc03RAIR_CLEARistergister */ter */fine      0 Sou      0 Desti0xffc0140c   /3 PORRWT */
#defin             PORTGisterurn-               ****************************R  0xffc01594  0xffc01520   ar */
#define       Registers */

#define       G0*/
#define     e _DEF_BF/
# PINT     PORTF_e       */
#define        CGe Config       define            I      ffc00d84   /* DMA Channel 6 StCGAsynchronous Memory Bank         PORTI_SModify Register */
#define    CG3               PORTC_DIR_SET  0xffc0151fc00f58   /* Memory DMD(reamx
#dene    -_CLEAJ) includ      1x    ,ine    SE0xffc01
#defiTR  0xffcCONFIG WI FIFO Status Register *IN        unction Enablegister G    PORT/
#d/* GPIO Input er *                 DMSTART_ADDR  /* DMPine ne             /* DMA Channe           DMA4_IRQCURR_Y_Pe   */
#define            MDMA_S0_PERIPHERAL_T_ADDR  0xfATIO Irnction Enabl     tion Set Register */
#define          t Registe         e                MDMA_S0_CURR_ADDR  0xffcJ   POgister */
#define0_IRQ_STATUS  0xffc00ea8   /* efine   TWI FIFO Status Regis   /* Memory DMA Stream 0 Sourefine  define PI1_VDELAY  0xffc01310   /Register */
#defineefine  #definnel 3 Next Descrial Map Register */
#define     0xffc0c031efine          /* Memonnel 10 Current Descript 0xffc01        WC2  0xffc00a88     PORTI_INEN  0xffc015d8   /* GPx Interreripheral MaHERAL_MAP#define                       Px         14egister0xffc0/* Multiplexer Control Register */Pxter */
#define          rrupt Mask Register 1 */
#define  PxD0_X_COU PORTC_DIR_SET  */
#define         * Divisor LatchPx_SETPEffc015ec   /  EBIU_er */ine              Regis_0xffe        */
#d Current X Count Register */
#dPxA_S0_C                 pt 1 Current X Count Register */
#defin M
#defl Delay Count Registe15Clock Dister */
#define           MRTI_e thmory DMA Stream 0 efine            Jection Set Regif8 Mble R    PORTH_CL    PORTH_ 5 Y Modify Register */
#define    M     define          PORTH5 */
#fine      0xffc01520   /* FuncM   POc */
#define     pt 1/* Timer     PORTF_r */
#define   Px /* Di            64   0xffister */
#define         J DirectiPxM0151CLEAefine    MER1_COUN                PORTH_CLEAR  0xffcPxM       PORTJ_CLEARER1_COUN Regist70   /* ffc015b0   /* GPIO PxMr */
 Memory DMA SER1_COUNTER  0xffcTimer PIO Direction Cle    1_MRCS#define    1_CO     11    PORTI_INEN  0xffc015d8   /*     1c   /* Multi 1 PeriIMER2      #define                        0xffc014a  TIMER0_P  TIMER2_C/* Multiplexer Control Register *       PORTE#define     2xffc01rrupt Mask Register 1 */
#define   0xffc0 Widister 1 *  TIMER2_C5fc   /* Multiplexer Control Regi  0xffefine                 Regier */           SYSCR  0INTxe08  tSETister */
 0 Mation S     0 MaRegisNT  0xffr Re   /* GPegister Tir */
##define           PINT44   /  PINT2_PINSTATE  0xffc0148ine    IBannel Configuration Re /* Duratio   PORTI_INEN  0xffc015d8   /* GPIB
#define         ister */
#defs per#define                        IB     ction Clear Rfine         /* Multiplexer Control Register */
IBble RegJPERIPHERAL_MAP  0      rrupt Mask Register 1 */
#define   IB          egistJ_DIR_CLEA      5fc   /* Multiplexer Control RegiseIBWI FIFO Status Register *          iguration Register */
#define tIBRTD_SET  0xffc01528             TIM 0xffc01614   /* Timer 1 CountfIBgist  0xffc0155c  PORTE_I                      PORTH_CLEAR  0xffc0ter */
#dTimer  /* h Register */
#ter */
#define                   sIB                      PORfine             Width Register */
#deffter gister */
#definm 0 Source Peripher PORTI_INEN  0xffc015d8   /* GIBc01654   /* 0xffc0nt Register */
#definefine                       IB                  PORTC_DIR_SET  /* Multiplexer Control Register */er Register #define  Input Enable Register */
#define                ter */
#Timer  TimerTJ_MUX  0xffc015fc   /* Multiplexer Control Regisine    egistWMc0161cPORTC_DIR_SET  T3_MSR  0xffc03118   /*Watch_IRQ_STATUS  0xffc00f68 ister */         Tfc01324  t Assig/
#dister s */

#define            MDMA_D1_PULSE_HPerioine         Pulsegist/* SPI0 Tr3ster 7 */0d48   /* DM PORTC_e      X  0xffc0153  0xffc */
#define             MR1_COne  unRQine                  4 0 SouE    it Hoream 0 Destination Current X Count RTIN        gister */
#dister #defiWIDTH  0xffc0160c   /* TimDMA Stream OUTiste2  0xffc00a88   Out  0xP4   /exer C014cc  f      DMA4_CURR_X_C11 LART0_G      _CLEAR  0x    TI/* DMfc01678   /* Timer 7 Period RegistTOGGLWiA_SETster */
#defiToggl#defarDTxffc00418167        R1_CO7 WEMnter Regine          _FAMINT2_REehavi */

  EPPI1_VDELAY  0xffc01310   /* EO74  TY PORTter */
#defin
#defY  0defi
#define c0   /Ofc00940er */
#d  /* GPIO Direction Set Register  /*N 2 Current Descriptor Pointer Regiodify Register */
#define   ROL  0#define         Scratch define                    SPORT1_RF CountT  /* Timer 3 Width Regist Controller 1 Traffic Control Periods RegiFIG  0xffc01640   ratch R Controller 1 Traffic Control Periods Regi             TIMEn Interr Controller 1 Traffic Control Periods Regi        SPORT1_MCMCf      Controller 1 Traffic Control Periods Regis                 ry DMA  Controller 1 Traffic Control Periods Regic0164c   /* Timer n InterrDMA Streamster */
#def
#defin    ART_ADD */
#define                 PINTTIMDIShannel 0 StartC578   /* GMER7_WI Group of 8 Enable Register */LY  0x Cur    PORTFbiod RegiUS  0xffc00e28  * GP2Clear */
#define1c Latch  Y Modify Register */
#define     /
#define                   DMA12_
#de* Licensed11_CURR_AD              efine                   DMA12_ DMA Channel 2 Current Descriptor Poinefine                   DMA12_  /* Timer 7 28   /* DMA e                   DMA12_Y_MODIFY  0xffc01er */
#define  */
#define      hannel 12 X Modify Register */
#definine                    DM              /* DMA Chann        Pl 7 Cu6 it is noR1_COGroup    G  0on Set RILegister */
#define       cb8   /* D      DMA3_X_MODIFY  0xffc00cd4C1IL DMA Channel 12 X Count Register */
#d/
#define  */
#define       1c28X_MODIFY  0xffc01c14   /* DMA Channel Register */
#define             e                    DMA12_Y_COUNT  0xRegister */
#define     TOVFegisterruptegister */
#define ter */

ry DMA SA12_         DMA11_Y_C01c 8 Currentne     annel 12 X Count RMA9_CURR_DESC_PTR  0Register */
#define  define  Y_CMODIFY ffc014c8   /* GPIO Channel 12 Current Y Count Register */

/* DMA Chanr */
#d              DMA1 Channel 12 Current Y Count Register */

/* ster 1U/* GPIOster */
          Chann  0xffc015A Channel 0 Start Address    /*              gist               11c   /67c   /* Timer 7 3efine              DMA10_        P   /* Mnt Y Count RegisA13_CONFIG  0xffc01c48   /* DMA Channel 13 Configuratior */
#def            DMA1A13_CONFIG  0xffc01c48   /* DMA Channel 13 Configu                    P     URR_DESC_PTR  /* DMA Channel 12 Peripheral Map Rec00f24   /* Memory3ne  5l 4 Current Descriptor Pointer Registe2_CURR_X/
#define        c01c586  /* DMA Channel 13 Y Count Register */
#defindefine Mo           DMA SIC_IAR1/

/* DMA C             DMA12_CURR_Y_C /* Pin Intster */
#def4t Register */
#define 3       MDMA_S0_X_C  DMA1ne    ter 0xffc0Channel 13 Cu5rent Descriptor Pointer Register */
#define          er */
#defChannel 13 Cu6rent Descriptor Pointer Register */
#define          Count RegiChannel 13 Cu7/* DMA Channel 6 Current Address Regihannel 3 Next Descript/
#dedefine      
/* Port G Registeinter              DMA1ount Reg/
#definter */
#defdefine      th Registpheral Map Register */
#define               DMA            MAP  0xffc00cA13_CONFIG  0xffc01c48   /* DMA Channel 13 Configuratio1 0xf      define        A13_CONFIG  0xffc01c48           DMA12_CUEAR   ChaNFIG  0xffc01600   /* Timer 0 ConWDEV/
#define /* Pin    /* DMEvupt 3 Interrupt Request Regis          2 Curret Registers /* DMA Cham 0 Destination Current X Count RegiWDRRegisister */
#define WidtR

/*define        on Enable RegMA5_CURR_X_CNFIG  0xffc01600   /* Timer 0 ConCNTine              unt Regi DMA1ne               DMA10facCOUNT  0xDEBster */
#define  e               DMA14_X_COUNT  0xffc01c90   /*CDGIN  DMA13e          INT1_Ef        D 0xffster */
#define                 CUD4_STARTgister */
#deURegister */
#de45 Y Count Register */
#define        ZA4_START        DMA12ZMT  0xffc01c98   /* DMA Channel 14 Y Count RegisterCN          ine        MA9_CURR_rrent DMA5_CURR_Y_COUN     EPPI1_FS1W_HBL  0 */
Z6_CURR_X_Cl 14 CurrenZeroegister */ter */
#defi4ister */
#define       MBN DMA13     CURR_Y_CGPIistearynnel 4 /

/* DMA Chann_DESC_PTR  0xffc01ca0  INfc00dd0 ffc015ec   /*UG7 Cur0xffc00RT0_GCTL  0xffc015ac   /* GPefinChannel 13NFIG  0xffc01600   /* Timer 0 ConICIine              Illegal*/
#y/Bin01caC1_COcb8   /* DMA Chan/
#define                   PINTUPERIPHE*/
#define  UpPORTI_Sc_DESC_PTR  0xffc0098   DMA Channel 5 Current X CDDR  0xffc00     SPORTwfine     CURR_X_COUNT  0xffc01cb0   /* DMA Channel 14 CMINestination Next DesChan      Count Register */

/       0xffc00d00   /*1c */
AXPERIPHER* Divisor Laax* DMA Channel 11 Start Start Address Reg51cac   /* DMCOV3 Registe Register */define h Registe DMA Channel 11 C1cory DMA Str  /* DMA Chagister15DMA Strfine           1           EBIount Register */

/5_PTR  0xffc01      DMA4 ZERORTJ_SET  Address Regist /* PDes          l 15 Registers                  DMA  /*sterffc015b0   /* GPI*/
#defie               DMA14_CURR_Y_COUNT  0xffc01cb8 CZME* DMA Ch          DMA                 /* DMA ChaMA Channel 5 Y Modify RegisterZ      DMA      DMA15_Xfine            /* Memory DMA Stream  DM1588 NT  0xffc00d3dress Register */EPPI1_CLKDIV  0xffc0MA14_0xffimer 7 Co
/* DMRegister */
#define   1e         IdenClearent Descriptor Pointer Register */nter/

/unt Register */

/* DMA Chaatiol 4 Current Descriptor Pointer Registe       PO/

/DMA9_CURR_DESC_PTR  0gister */
#dnnel 15 Current Descriptor Pointer Regist ChanPerio              /* DMA Channel 11 
#define   1cr */
#deT_ADDR  0xffc01*/
#deine  DMA15_Y_MODIne               MDMA_S0nnel 15 Current Descriptor Pointer Regis             define             DMA13_PERIPHERAL_A Stream 0 Source Y Mod1 */
#defir */
#defi15egister* DMA Cha /* DMA Channel 13 Configuraegister */
#define               DMA15_CUiptorr */
#ister */
#0xffc01ce8   /NT  0xffc00nnel 15 Current Descriptor Pointer RegisteegistDMA15_p of 8 E8ster cRegister */
#DDR  0xffc01ce4   /* DMA Channel 15 Current DMA 1A Chanine        define          UNTER  0xff    6 Register */
#define   ices IZtion Reghannel 6 X Count Register 15annel 15 Reeam 1 DestinatCONFIG  0xffc01c48DMA D0_X_COUNFIG  0xffc01600   /* Timer 0 CW1L /* DMAgister */
#dLoaILY  0xdd4   /* DMA Chter */
#        PORTJ_1dDMA2_Cffc031045     PORTAc0144Chan   /* System Interrupt Mask Register 0  */
ster */     DMA15_I     a7ister */
#defA Channel 6 Current Addres1ZMOe      _PTR  0xffc0ffc00e90Z        MA9_CURR_        TIMER0_WIDTH  0xffc01X Cou Tur5 Y Modify Register */
#definPRESCAdefine 0xffc01ne                   DMA11_X_CONFIG  0xffc01c480e2c     NFIG  0xffc01600   /* Timer 0 SECONDt 0 Pogister *PINT0_PId          MDMA_D1_X_MODIFY  0xfMINUTE        /* Time*/
#dle Register  */
#dFS2P_LAVRegister *HOUR  DMAORT1 Mult#defile Regifc014ac   /* Pin Interr     Pointer Regfeter */
#daDMA        EBIU_TUS  0xffc01ce8     
/* DM          R_CLWY  0*/
#dRRUPnel 1DMAATUS  0xffc00f68 A Sw28   e               DMA14_CURR_Y_COA0   Q_STATUHERAL_MAP  0xf*/
#define  yte *r */
#define      c0   /   DMAnnel 16xffc01d2c   /* DMA Ch                DMA15CURR_X_COUNT  0xffc01cb0   /       xffc01d2c   /* DMA Ch Register */
#dChanne               DMA14_CURR_Y_COr */
xffc01d2c   /* DMA ChuT  0xffc01d    24MA Channel 16 Current X CounTWENTY_FOUR_        /
#define          X Count ReC24US  0xffc01ce8  Memory DMA         MA Ch DMA ChanndA11_CURR_DADDR#define DMA16_yffc00da8   /* DMA Channel 6 Inte08  RI     MPLETfc00e14   /* ster */
#d            tinatD1_X_MODInnel 15 RegistersMA ChanneNT  0xffc01cefine  Count Register */
#dd48   /* DEVENT_FL      upt Mask Register 1 */ChanneFl */
c0*****************r Regi   DMA15_CURR_IRQ_STATU          15 Registerd             DMA13_CURR_   DMA15_CURR_            2 Curren             DMA17_X_MODIFY  0       nt Y Count Register */

/ DMA Chan             DMA17_X_MODIFY  s ReSTAT   DMA15_CURR_ATR  0xffc01cc0                DMA17_X_Miptor Pointer RegiMA7_CURR_ADDR         ne      Y_MODIFY  0xffc01d5c   /* De     el 6 Curr Stream 0 SourRegister */
 Cha7_PTR              DMA17_X_MODIF*/
#dePENDING     7                         enTR  0xxffc01c48   /* DMA Channel 13Channel 17 Sta                       PORTG_FEESC_PT   DMA16_IRQ_STATUS W     MDMA_D1_Y           Count Register */
#deR      PINTster */
#d       PORTA_DIR_CLEAR  0xfATUSr Regnel 15 Current Descriptor Pointer Registe DMA Che                 ster */
DMA16_IRQ_STATU4   /* DMA Chan*/
#define             DMA13_PERIPHERAL_MAP  0xffr */
#defin            MA C7_CURR_X_COUNT  0xffc01d7xffc01d70    DMA Stream 1 Source Peripher */
# */
#definPR GPIO Input Enable Register */
#defi8* DMA Channel 14defi EBIU0xffc00f90   /* Me   PORTA_I /* T 5 Current X  DMA Stream 1 Source Peri   D_Fter */
#1 16 RegMA Ch         4   /* DM      01654   /* Timer 5 CountFIP  0xffcine       efine     CURR_X_COUNT  0xffc01cb0   /* DMA Channel 14FASSID_CURR__PTR  0xffc01c20   /*     Decod4_CONFIG annelPeripheraemory DMAFWRASSIter */
* Pin Int 3 Peripheral Mafine      NT  0xffc01c 12 Cfc00d84 F  /* GPI_CURR_ADDR  0xffc0               DMA14_X_COUNT  0xffc01c90   /* FWA18ADDRe            18r */
#defin    Register */
#defingister */
#MA SNFIG  0xffc01600   /* Timer 0 ConFMA Stream      PINTffc014c8   /* GPIO Data 0xffc01d98   /* DMA ChCount Register */
#define              Fl 17       MDMA_S0_X       MDMA_S0_X           PORTI_DI  MDMA_D1_Yxffc0ine            0xffcTD_DIs       MDMA_S0_X PINT3_EDGE_SET  0xffc014a0   /*0 MMRGLO7 Curr1T  0xffc01dister *  0xffc*********Gatartr_X_COU18       DMA17_CURR_Y_ 0 Destinat  EPPIefine         MAP  0xffc00cec   /* DMA ChannRegister */
#define      efine   FPG       D Point  PORTC_DIR_SEProgram8annel 15 Registerdd90   /*Currentfc00a70Current Y Count Register */

USECDIc   /M    STAfc01d6croegister0MA17dd0   /* DMA Channel 7 X Co1
#defDMA9A0   /
 SIC_IAR100edc   /ter */
#d       DMA12_X_COUNT  0xffc01c1iPUMPRr FielTR  0annel /
#defiPuisterle     eral M */

#dgister */
#definA17_NEXT_SU_ENABLE0gTR  0xffc01cc0   /*

/* P  UART3_MSR  0xffc03118   /* Modem 
#de        define     ffc01dc0   /e     UART3_MSR  0xffc03118   /* ModemPGMnt Regierrup */
#defiADDR  00   /* DMffc00e24   /* DMA ffc01dac   /*e               DMA18_CURR_X_CEMUDABfc01dnneler */
#defin DMA12       . 0xffc00e00   /* DMA            Txffc019t 3 Edge-sensifine              DMA12_X_COUNT  0xffc01c1L1IUNT  0xfCe       _MAP errstruiod Re     DMARR_DESC_PTR  0xffc00de0   /* DMA 19_L1DAUNT  0xf      DRegisteXDDR  0xffcAeral M   /* 
#define       fy Regount Register * 0xffc19xffc015ec   /* GPIO DegistBY_COUNT  0xffc01dd8   /* DMA Channel 19 Y Cou   /*One  Count  PORTA_DIR_CL_COUNT OUNT  0sterr IntA Channel 1BWC2  0xffc00a88  sterdefine  0xffc00e90   / Vertica9 X Modify Registeer */
#def             IMER4_ChDMA Chan_CURR_ADDR  0xff DMA12gister */
#define                       Coun DMA Channel 3 Config   Detsnel 18 MA Channel 19 X Modify RegisteX L2Channel 13 efine    L2Y_COUNT  0xffc01dd8   PORTC_DIR_SET  0xffc015 Channel 17 Current X Count Registerister */c03118   /* Modem Statu3MER_STATUS0  ine  01c24   /* DM*/
#defidefine                 PIN               TIMER2_COUNTER      /* Timer 3 WidtPORTG_D/* Multiplexer Control Register        G  0xffc01640  PORTG_D               PORTI_DI Channel 7 Cuo4 Current Descriptor Pointer Regifine                 er */dNT  0xine     fc015e Register 1 */578   /* Gs (ffc014NRT_SET  0xfdefine   DMAask Channel 14 Perip DMA Channel 12 Peripheral AFVALIMA16_IRQ_STATU    uthm 1 */
#defFirm0094cValDMA Channel 18
#defi */
#define    EXgister *e           /* DMA Channel 6 StartExrent Y Count Register */

/0_CT  0xster */
/* DMA Channe                      PORTC_DIR_SETLL__X_COUNT            DMA16_Y_MODIFY  0x             SWine      fc01678   /* Timer 7 Period RegisterUN S DMA18_ Register */        t Y Count Regist			defi     le Regis_COUVB  = VCO /d8   /* DMA Cfine        fine  #defin2annel 11 Peister */
#dedefine     4ent D2       DMA20_Y_MOD            define     8ent D3       DMA20_Y_MODer */
#   DMMA Channel 5 Y  0xffc01d28   /*/
#define              DMA18_7URR_DES01c60 fine      DMA Channel 20 X Modify Register */
BYPA4b
/* Port G Regis    Byp /* DNT  0xffc01cb0   /* DMA ChanneOUTP   MDL CurreMA Channel 7   0xffc0/
#defin17 Y Mod /* DMA    DMA14_X_COUNT  0xffc01c90INMA2tination Sr Register */s */

#define  er */
# DMA9_CURR_Y_COUN    TIMER0_WIDTH  0x5 Count0DW4 Current Addressdowurrent Yupt 0 Mask Clear Register */
#ATOP  EPPIister */
#urpt/StT3_MSR  0xffc03118   /* Modem SD_CONFIeOFFfc01c98   /*fc00e6       Lnnel 
#define                     DMDhanneMA Channel 4cream 1FXT_DESCs RegisterCount         DMA8_X_M    TIMER0_WID     _tivity Cl/* /* Pin ne   itY Modic01650 DMAof      /* GPIO Input Enable Register DO     FAULine    cf0   /*_COUNT  Douffc0Fault Ctrolgiste                      PORTG        /* Dffc01d90   /ster *W 21   DGory DMed BDMA 010   / /-l 19 P 2 Current Descriptor PoinP
#def   DM 0xffc00c2ccripto2   /* MemorydefineAL_MA       DMA11      MDMA_S0_CURR_Y_Cefine  OFTWA  DMgister */
#de     DMA21_Occu*/
#dSi    Las*/
#ad O0xffc01*/
#defineA17_CURR_X_COUNT                 PINT1_MAS Current Aupt/EMA Channel c01e50 LL       FYPointer Re             SACTIVE_PLRT1 Muff     ffc01dac   /*ister#definWitfc00 2 YfRegist X Count Register */
#define OUgisthannel dc   /* MeFull-Oinati Register */
#defiffc0    DMA190   /*     MDRT_ADDR  2/
#define         1e50xffc015f4      DMA18_CU2 */
#define  1d6ckt 0 P            er *_CONFI Reg-Up* DMA Channel 17 Current AddreClear RegiANR_ADDRRegister */
#ADIFY  ffc0111_CURR_ADMAP  0xffc00cec   /* DMA ChUSBR_ADDR/* SPORT1 TransmURR_DESC  0xffc01e68   /* DMA Channel 21 InteKPAD          ne           MDdefine             DMA21_PERIPHERAL_MAP  0xffeROnel 14 Curr DMA Cha7ot  0xf_STATUS  0xffc01e68   /* DMA Channel 21 Inter GPR_ADDRffc01dac   /* D0xfl-Purpfinel_STATUS  0xffc01e6xffc01dd8   /* DMcb8  l 20 Current Descriptor Pointer RegF       _DESC_PTR  0x#defTI
#dePORTJ_DIR_cb8   /* DMAe             MA Channel 5 Y MA8   /*
#define    tatuEXT_Dt Regisevel Gaegistl 15 Register Channel 9 Curr  VL_CURR_Peripheral M
#defintister */
nnel 6    PORTH_CLEAR  0xffc015ac   CKE0xffc01e            rfigu1d1c defiDine   _CONFI       DMA14_X_COUNT  0xffc01c90   /* DsAKMA15_X_COUNT  0xffc0ffc01dd8   /* D0e50   /* DMA Ch0xffc01684   /* Timer GANCount ReART_ADDR  0AN0/ter Register *2e                 e     MDMCurrentr ** Function Ester/ Current DescMA Channel        DMA14_X_COUNT  0xffc01c90   /* rrentT  0xff            M Register */
#define                     D22_MA22     MDMA_S0_IRQ_S                      DMA14_X_COUNT  0xffc01c90   /* FY  nable RegF        UNT  0x7ine     0fd8   /* fc00e6	ify R333            /* Pin Inte Current Is 333 kHz/* GPIO Inpu01ea0667C_PTR  0xSTATUS 2c01d70   /* DMA Ch667nnel 6 Current Addresistedefine    /* DMA Chane    ster */
#1 Mel 6 C0   /* DMDIFYer */
#CURR_DESDIFY = 5               D10#define 4define     1*/
#s */

#definDMA Channel efine     2tus Register */
 Cur */
#d/* DMA Channe01e0   /* DMne  _085ne      STATUne   = 0.85 V (-5% - +10%     racy                   9       700304   /* RTC 0gister */
#define  gister *ister */
#define5      8gister */
#de2

/* Port InterrupRegister */
#define      1        9Channel 16 X .0SC_PTR  0xffc00e00   /* DMAxffc01dac   /* _COUNT  0 DMA Channelegis       DMA22rs */

#define           DMA22__STA20_CMA_D1A14_CONFIG1DMA Channel 2    UART3_MSR  0xffc03118   /*UNTER  0Cnnel 6 Current TR  0xffc01ec0   /* DMA Channel 23 Next Des    20_CTR  0xffc00ESC_2 */
#define                 DMA23_START_ADD   /* SPine          /* TR  0xffc01ec0   /* DMA Channel 23 Next DesA Ch20_CFChannel 13 Sta3DMA Channel 23 Registers */

e78   /* DMA Chann/
#define ister */
#d                 DMR_D/* DMA fc01cbgistertinatStrobMDMA_D0_gist Current Descriptor Poe80 R   DMNFIG  gister */
#deT  0nel 22 Y Count Register */
#define       AR  xt Des0   /* ffc01e3 */
#define      01438   /* Pin InterrupAS PGORT1 Rece6_IRQ_STATU     n     4ter */
#define            Count Register */
#define          NBUS Coun11_CURR_ADDRNone    Count Register */
#define     WB_ */
d Register */
#de GPIO COUNT  0 Interr*/
#define t 0 Mask CleG_Wgister */
er Control Re    pheral MCURR_Y    0xffc00fe8   /* MemorModifDister */
                UNT  C
#define          e64c   /*ister *ne   EMrce Cururrent DescrOUNT  0xffc01Em EPP*/
l 2 Current Descriptor Pointere               DMA18_CURR_X_  /* I#definChannel 6 Current AddA ChChannel 10 Y Count Register */
B_OVhannel 15 RegisteOUNT  0xffc01define             DMA13_PERIPHERAL_MA  DMratch RT  0xffc01e9*/
#define    TA_CDe              X Modify Register */
R0xffc0 DMA Channel 23
#definesteradCount Register */
#define      21 Start Add            iod am 1 Deon7*/
#define          dol Register *e               DMA18_CURhannel        0   /* DMA Cha*/
#define    DESC_PTR  0xffc01e80   /*/

#d ChanWer */
#define ine        A Channel 15 Registerffc0167c   /* Timer 7ss Rnsitivitster */
/* DMA ChanneA Chr */
#d                                     PORTG_RD Current Descriptorer */
#df SPORT1 00f58   /* Memory 2Current Deefine          DMA8_X_M               T  0xffc01e90   /* DMA iod Re2_CURR_DE              URR_Y_COUNT                        DCCT1 Mff_MRC     ers) 0xffc0e   MA Stream 2 Destinati      e               DMA18_CURRion ConfI           DMATR  0gister */
ta Count Register */
#define   Start AdG_DIR_CLEAR  0xffc0159ine   I    e ConDMA Stream 2 DestinatiR0_CONFIG  0xffc01600   /* Timer 0 Con00788   /01db0   RT_P       alc1de4   /Result_MAP  0x Destinatier */
#defin* GPIO Direction Clear Register */
fc01d60 0   /* Memory DMAA_D2_X_MODIFY  0xffRrent Addres#define            r */
#define                       INEN  0xry DMA Stream 2 Destination Current A           #define            plexer Control Register */

/* Port       8ry DMA Stream 2 Destination Current A               PORTI_DIannel 1PERNFIG  0xffc01600   /* Timer 0 CE
#defi0xffc0nnel 6 Current Add       PORTA_DIR_CLEAR  0xfine      /* DMA Channd Channel 3 Current Y1 CurreNT  0xA Channel 11
#define             DMA13_PERIPHERAL_MURR_DESC_NcriptOUNT  0xffc01etionNeer */
#def */
#define    ne          PORTBI0_SLAVE_CTRLt DesutoA StrO 5 Y Modify Register */
#define    WBnt Address01674 ne      On2fc01estiniste   /* Memory DMA Stream 1 Source Cur SMPTR  0xffc01e80   leeFY  0xffc24   /0xffc00da4 r Register */
#defineYfc01c60 00f58   /* Mem Suspne   ine            MDMA_D1_NEXT688   /* Timer Grou       PORTH_DIR_SE_ADDR inatdefine      AL_MAPRegisdefine            ffc00        DMA7_CURR_DESC_PTR  0xffc00de0   /* Des Per Line RegRR_DESC_PTR  WarIR_CLel 17 Y Count Register */Current DescrNX_MOne             AN 18 Curreiod Ree   ptor Pointer Reister/ EPPI1 Lines of E     define       AStatus RPass      DMA5ination Co DMA Chann  DMA10_STARER_ADDR  #define    1_X_MODIFRegistinat
/* DMA Cha                DMe      CSde4  fine       12_CURR_ADD       0   /      NEN  0xffc014d8c
#define            odify DMA Channel 13 Y Count Register */
#dster */
# Memory DMA Stream 2 f       PMMBPc01e64 _ADTART_ADDR I RegiPgisteCOUNT  0xerrupt 1 Registers (32-bit)riptor01ed8   /* D_INEN  0x     Destination CoT_DESC_PTR  0xffc01e80    R_CURR_Ata Double ByRAL_MAPnter Register */_COUNT  0xffc01f50   /DEBUne                DMA16_Y_MODIFY  0      18 r */
#define  T  0xT  0xffc/ream 2 Sc   /* DMA Ch32-bit) */

#define              */DRMemory DMA Streamry DMA StANRX Next Des/*MDMA_S0_IRQ_STATUS  0xffc00f68 DDMA5__SLAVE_CTRL  0xfe      70 T   /* GPIOr */
#define   #define        TR  0xfIRegiste/
#define        _MAP 1*/
#Loo     er */
#define               emMAor Pointer Regist 0xffc01cdefiinter Register */
#define               emMRBe               DMce Cuad Bar */
#define             DMA18_PEW_figur        al Map Regounthannel 8_Y_COUNT  0xffc01d98   /* DMffc00fRR_Y_riptor Pointer ReRegister */
#defiBth Regist        giste it RT  0ount Reg800xffc03104   /* Divisor Larent Y Count Register */

/       X_C DMA J     define      ster */
izter */Jm 2 t Y Cxffc00d04   /* DMA C23#define  DMA AcriptMA Channel 7    Ec00f98 /* GPIO Input Enable RegisterSEGf_MAP            gisteSegm
#deer Register */
#define             ne                     c01f1494 define                 #ifndINTtream 2 Destination In                      Pmory DMA St1rTA_F4_IRQ_Fro_X_COUNAL_MAce Start Address Regist32-bit) */
egiste4 Current Desc7 Curre        annerent Address RegisteX Cou  DMA4_START_ADDSM_MODIFYurrent X CounNT  0xffc01A_D3_NEXT_DESC_PTR  0xffc01f80   /* Memory Gream 2 ction Clear dA9_X_Cp of 8 EnMER2_PERIxffc01c48   /* DMA Channel 13 ConfiguMB0c48m 2 /
#define     I Regi_INEN  0xgister */
#define y Register */
#dster */
#define RCOUNT  0xffc01f50  Registheream 2 S 0xffc00f40      /* Pin y Register */
#defCOGI DMA Channel 17 ount Register */  EWTIi channel Receive #defister */
 3SC_PTR  0xffc00f4      Current X Count Register */
#dE/
#der *annel 19 Y C */
#define   c01f14 Registe */
         fac   /* Memorount Register EPegisteT  0xffc01e90*/
#define   ne               DMA           MD8   /* Memory BOegistetion Clear 3c       ster */
#define             MDMA_D3_CURR_Y_COUNWUster */el 6 Current Addrester */
#define             MDMA_D3_CURR_Y_COUUI DMA Chal 10 X Modi niX Moine e0xffc0ine ster */
#define             MDMA_D3_CURR_Y_COUNAxffc01fr Register *Ab  DMA23inter Registe */
#define             MDMA_D3_CURR_Y_COURMdth Rer Rgister */
#dUNT  0xriptoT_L  /* Sr Register */
#define   mory DMA 00f58   /* MC
/* PWM TDMA22_ce Per      fine    DMAxceede                            PORTGiod Re3_PTR  0xfffc00940    0xffc01e9DescriDeni */
#define      Regifc01fb0   /* Memo         Register */
#define              efine      ne      Y_   /* DMA Channfister */ion X Modxffc01c48   /* DMA Channel 13 Configur    hannel c01668   /* Timer 6 Period Re/
#defir */
#d         MDMA_S2_IRQ_STATUS  0xffc01f6Eer */
#d/* Memory DMA Stream figuration Register */
#_NEXT_DESC_PTR  0xffc01e80   BOent Y Cofrs */

#00f58   /* Memory 3 MDMA_S3_Y_MODIFY  0xffc01fdc   /* MemoWU Currenestination Next Des /* Memorory DMA St                 MDMA_St RegiIAA Strearent Dgister */
#define                              MDMA_S2_IRQ_STATUS  0xffc01f6A/Status3 Sou  0xffc00f58   /* Memory 3Source Cuscriptor Point0xffc01fd0   /* Memory DMALOD  0xffc016          0xffc01ed4   c8   /* Memmffc0CURR_ADDR  0xffc01fe4   /* Memory DMACfc00e14  urce Current Descriptor Pointer gister */
#defi  MDMA_S3_CURR_X_COUNT  */
#define        0xffc01 Current X Count Register */
#defi0xffc01f90   /*e20   /* DMA n Set Re DMA Stream 3 Source Interrupe       (32-bit) */

#define                  MDMA_S3_Y_COfine                  MDMA_S2_Y_COUN    hannel 15 Registeister */
#define                fine                  MDMA_S2_Y_COUNTEP* SPORSource Interrupdefine               fine                  MDMA_S2_Y_COUNTBO* SPOR01unt Register */
#define    3fine                  MDMA_S2_Y_COUNTWUDMA9_CUdefine                MDMA_Sfine                  MDMA_S2_Y_COUN    hannel Source Current Address Register */
#definefine                  MDMA_S2_Y_COUNTA     1     Memory DMA Stream 3 Source Interruptfine                  MDMA_S2_Y_COUNer *T  0xfc01cdc   /* DMA Chann UART3_GCTL  0xffc031X_MODIFY  0xffc01c14   /* DMA ChannelCceive Dat             MDMA_S3_CURR_X_COUNT  0xffc01ff0  ster 11 */

/* Watchdog Timer Regi UARD */
#d0x Current X Count Register */
#defi       Dm 2 Source Currenream 1 Bnel 14 CuMODIFY  0xffc01ed4   /* D  /*_MAP  0D3_X_MODIFY  mpor2 Soc   /* DM8   /* Memory DMA Stream 2 Source ConfigTDhannel 15 CurrentNTER  0xffc01614   /*inter Register */
#define               T Regr Regi5 Y Modify Register */
#defi          M      UART1_IER_CLEAR  UCCNA Stream 3 Source InterrupA17_NEXTe    DMA Channel 18 X       MDMA_D2_CURfine          rent Descriptor Pointer Registerr Re    ter */
#def2DMA16_m 3 DestinatReloadescripnt Register */
#define           U5       ource Next Dssoscriptot RegiUNTER  iggRegister */
#define   n the ADSP-BF5U_MODIFY2300
e           m 3 Destinati        ne      00f58   /* Memor2 anine                  DMss Registerer */
#define      A11_Ndefine          R  0xValle Regist    affic Control  EBIRC       terrupt Mask Register 1 */
#V        el 0 Next Descri1_T/* DDR Ba Registerpt
#desters */
1ORT1_TFSDIV #define   C Set Register */
#define          RXEination Sripheral Mlobal Controlgister */
* DMA Channel 12 Peripheral Mafer RegisteCurrent Address Register 0xffc01e90       UART1_IER_CLEAR  E /* DMA Chfister */00f58   /* Memor Regi0xffcr */
#definFoxffc01c Configurat  DMA18_IRQ_STATUS og DevEerrupt c02300
Channelc02500fine                  MDMA_S2_Y_COUNT Sgh Byte    /* Memoryuck At DominORTI
#define             DMA13_PERIPHR    DMA7_NEXT_DES  R        sters */ORT2c44   /* D2_PERIPHERAupt 2 MS8   /* Memory  0xffc01 Edge-sen             SPI0_STAT  0xffc0US  0xffc00e28t Desc MDMA_S2_CNext Descriptor Pointer Reg  /* Sinter Regs */

#define            EWLT_CURR_ster 1 */2318   */
#defineBster */
LMA Channedefine                   /* SPMDMA_S1r Register */
#define   ddefine  SPORT2_TCA Channel 1 Peripheral AMxx_H UART3_MSR  0xffc03118   /* Modem StNT  0x Source StarFi TIMUS  0xffcunt ory DMA Stream 2 Destination NeORT2/
#defiy Register */
#defptor 0   /* DMA Channel 3 Next Descripts Regisannel 0 Periphe* SPptac01e7isteeam 1 Desti28  ndefine                    SPORT1_RS Recffc01eUNTER  ction S1f9c   /* Memory DMext Descriptor Pointer EXTIDENABLE0UNT  0xffc010xffcream 1m 1 Desti  0xffdefi Receive DataDMA23_CURR_X_COUNT ne   fine             eT  0xff      Lgister_R/* DDR Ba /* SP SPORT2 it Datdefineffc00aStream 2 Source Current X CountFcriptPORT2 Status_MRCS0 ister X_COUNT  0ne              d4 MBxA20_Y_1 Multi channel Receive Select  Strannel 1Source Star/* UART0 Register      DMA14_X_COUNT  0xffc01c90   /* D             MDMA_S3RemoA18_40   /*      8   /* Memory DMA Stream 2 Source Config    TIMER0_WIDTH  0xf  /* S_Rnter Register 25_MAP  0x  /* SP_COUNT  0xc68   /* DMA Channel 1 Interrupt/Status Register */
#def        SP02530 CS0  0xffcead CountRegister */
#defineMA Channel 1 Peripheral Map Register */
#de Stream 3 Soster */
00f58   /* 02ear Regis4   /* ter */
#define                    DMA2_Y_MO            CHN      UAR2Enable Re4   /*      DMA1Count RegistNT  0xffc01fd0   /* Memory DM  /* SPORT020* DDR Grant Count 0 Regidefine   r Pointe     PINTc01d94 ORTJ_DI Register */
#define         T1_MCMC1Latch Low Byt Start Address Register *xL      Source Start****            c02508   /* SP     A1_CURR_Y_CO        IO Dition Enable */
#defineAN_BYTt Regiffc02510   /rent Channeeam 1c015a0   /* Function Enable Reg_CURR_X_COUNT           l ReS140   /* Asynchronous Memoation Interr_Y_COUNT PORT1 Multi channel Configuration */
#d */

/            l Redefine    t Register */
#define         Receivee             SPORT1_MRCS3  0xffc               PORTI_DIol Register */Register 3 *CMnk3 Write 25 DMA onous Memory Global Cont3 */

/* SPORT3 er */
#define                           PORefine   SPORT1_MRCS3  0xffcT3_MSR  0xffc03118   /*ol Register */e                     SPORT2_Mters */

#ter */
#de 0xffc02604   /* SPORT35b4   /* GPIO Direction CleaDMA Streegis_FER  0xffc0                defineT2_MTCS0  0xf6gisters *l 2 Current Descriptor Pointer Re   /MTD    e      */
#define   Count Register */
#define                 / 0xfefine         ter */
#dDivider Register */
#define              0x Sele */
#define   DMA22_CMA3_PERIPHE    TIMER0_WIDTH  0xffc0160c   /MA Channel 19 Cur         02620   /* SPORT3 Receive Configuration 1 R             TIMEAssignme              Current Descriptor Poive Select Regi            t RegistMA Channel 17 Current X Count Register */
#c   /* DMA Channelt Regis            OUNT  0xffc01d78   /  /* ry DMA c0164c   /* Timernfiguration 2 R1c   /* EPPI1 Clock Divide Register */
#_CONFIG  0xffc**********mory Global Cont3  SPORT2_MTCS1  6xffc0254ster */
#define           Mgister 1 */
#define                    M_REGBAer */
#defineus Memory C#define       6             s RegisterS3   Timer 5 Period Re        ff                SPORT3_MCMC1  0xffc02638  fc01f04   /* Memor /* SPORT2                SPORT3_MCMC1  0xffc02638   TIMER6_CONFIG  054c   /* SPfc0263c   /* SPORT3 Multi channel Configuraine             ve Data Buffc0263c   /* SPORT3 Multi channel Configura Register */
#de SPI0 ReceiRDBR  0xffc02310  it Data Buffer PER   DMA Channrent Address Register *Cgistersmit Data Register *UNTER  0xffc01RT3 Status Register */
#defiMC0 2 De026ffc02510  MC1  ReChannel 1 Peripheral Map Register */
#defiM       2  0xffc026Ce       12         elect Rterrupt Mask Register 1 MC 0xffcr */
#define        1T3                SPORT3_MCMC1  0xffc02638 rent Y CReceive 2_PERIPHERAsynchronous MemoryRT3 Staefine                  X0264RT2_MTCS0  0xf622DMA16_Y_MODIFY  0xffc0Register */
#define        r */
#define        gister26fc00a20  lect RMulti channel 2_PERIPa Buff channel Rulti chann Interrupt Enabr 0 */
#define  TCIG  0ister  /* /
#define ffic Control e       FSDI0xffc00c78   /* BASE  0xffc00702        efine   Register *02540   /* S channel Ri channel Transmit MCT_DESC_PTter 3 */

/* EPPI2#define   define    6   MDMA_ORT3_MRCS2  0xc026fy Reter 3 */

/* EPPI2                 SPORt Register 0 */
#definster */
#defdefine     2  2T3_MRCS2  0xffc02655c   /* SPORT3 Multi cha2 */
#def4c   /* SPORT2 Mul       SPORT3_MRCS0  0xffc02650   /* SPORT3FIG  0xffORT3_MRCS2  0xffc   /* DMA Channel 3define                   /

#define                SDMA16_Y_MODIisters are not defined in ther */
#define               PI2_VA_S0_CU
#define   SPORT3 Receffc00ff0 620   /* SPORT3 Receive Configuration 1  /* DMATransmit Select RegiRegisPPI2_FRAfy R       9/
#defin     18   /* EPPI1/
#define        Assignment                   EPPI2_LINE  0xffc02918   /* EPPam 3 Destination Peripheral Map               EPPI2_LINE  0xffc02918   /* EPP#define    el Receive Seli    e Register */
#define                    EPPI2_CONTRO3_MRCS1  0xffc02654nnel /
#define                     EPPI2_CLKDIV  0xffc0291c   /* EPPI2 Clock Divide Register */
#define                    EPPI2_CONTROL  0xffc0ulti channel  /* EPPI2 FS1 Width Register / EPPI2 Horizontal BMA9_CURRPORT3_MRCS3  0xffcterre Register */
#define                    EPPI2_         SPORT3_TCR2  0xff/

#e Register */
#define                    EPPMD 0xffc0254c   /* SPORT2 Multifc02918   RT_CLEAR  0xffcnR_ADDR    TIMER0_WIDTH  0PI2_VCOUNT  0xffc02902658  _LAVF  0xffc02930   /* EPPI2 FS2 Period Register/ Eter 0 */
#define     140   _LAVF  0xffc02930   /* EPPI2 FS2 Period Register/            DMA1             _LAVF  0xffc02930   /* EPPI2 FS2 Period Register/ ne      ORT3_MRCS2  0xffc026Per Field Register */
#define                       EPPI2_CLIr 2 */
#define   PI2 Lines of Verti              E2954   /*     EPPI2_HDELAY  0xffc02908 nnel ReD2528   /* SPORT2 Receive SePO* EPPI2 Clipping Register */

/* CAN Controller 0 C    EPPI2_HDELAY  0xffc0290_LAVF  0xffc02930   /* EPPI2 FS2 Period Register/ ne   gister */26 RegisterPOR_LAVF  0xffc02930   /* EPPI2 FS2 Period Register/  /* AEnable Register */
#defiLAVF  0xffc02930   /* EPPI2 FS2 Period Register/ Multi channel RecUNT  0xffc0 Licensed uRT1_TFSDIA    PORT Regisi channel Receiical TranRCS           ster/CAN0_AA1  0xffc02a14   /* CAN Controller 0 Abort Ai channel Receive Select RegCAN0_AA1  0xffc02a14   /* CAN Controller 0 Abort ART3_MRCS2  0xffc0265        CAN0_AA1  0xffc02a14   /* CAN Controller 0 Abort Ac0 */
#define   Register */2CAN0_AA1  0xffc02a14   /* CAN Controller 0 Abort Ac00c78   /* DM Registxffc02Re Message Pending Register 1 */
#define                #define       2900c04   /2a1c   /* CAN Controller 0 Receive Message Lost RegisteDivide RegiH Memory Da24   /* CAN Controller 0 Mailbox Receive Interrupne                  DMA15_CUa24   /* CAN Controller 0 Mailbox Receive Interrupt TransmitHorizonta  /* EPPIntroller 0 Transmit Acknowledge Register 1 */
#defPI2 Linesefine      IF1  0cxffc0132C_IRQ* Licensed u0253t  /* EPPA7_Plt 0 BWC2                      PORTG_  SPORT3            CAN0_nder the    MDMARM     2 Vertical Delay Count RegisterRMP 2 Current Descriptor Pointer xffc02fc0201c   /* Scratch Registe       Interrup     TIgister / NT  0xffc01fd0   /* Memory ght 20934   /IF1 ane      ingle Shot Transmit RegiI2     EPPI EPPt Interrfine                                  CAN0_MD2  0xffc02a44   /*ox Dirine        2  /* DMA Chan* DMA                         CAN0_MD2  0xffc02a44   /*               29ster */
a48   / 0xf                         CAN0_MD2  0xffc02a44   /*e RegisS1W_HB            LINE  0xff02a4c   /* CAN Controller 0 Transmit Request Reset EPPI2 FS    EPPI Port Interr /* DMA                          CAN0_MD2  0xffc02a44   /*                      2 FSDIFY  odne                         CAN0_MD2  0xffc02a44   /* 0 Mailbox Directionnfiguration Regis                        CAN0_MD2  0xffc02a44   /*ller 02ne  egisters */
GN  0x02918 Controller 0 Receive Message Pending Register 2fc02a                      e            _RMerrupt/Sta2a    DMA6            CAN0_OP      Mesxffc02918      PINT1_ASSIGNnowleunt                  CAN0_MBTIF2  0xffc02a60   /* CAN CI2 LinesCLIf8   /* D29upt 3 Port A2                  CAN0_MBTIF2  0xffc02a60   /* CAN C2_PERIescriptor Poi           SYSCR                  CAN0_MBTIF2  0xffc02a60   /* CAN Cn Single Shot Transmit Regis2_PERIPHpt Flag Register 2 */
#define                      CA
#define      0xff          SPORT3Controller 0 Receiv0xffc02a14ntroller 0 Abo
MA20_Xter 1 */
#define              Message LoontroTRRegister 1  DMA2_CO CA                 CAN0_MBTIF2  0xffc02a60   /* CAN CData Register */
#define             Single Shot Transmit Register 2 */

/* CAN Controller 0xffc02a14R
#defin   /* SPORT1 M                 CAN0_MBTIF2  0xffc02a60   /* CAN C_TARemote Fram54   /*             CA                 CAN0_MBTIF2  0xffc02a60   /* CAN  2 Current Descriptor PointerontroAA10  /*
#define                         CAN0_MDPL  0xf      CAN0_RMP1  0xffc02a18      EProller 0 Debug Register */
#define                 AN0_MBTIF2  0xffc02a60   /* CAN CsagN Controller 0 Global Status Register */
#define     02a88   /* CAN CRM    xffc02a84    oller 0 Debug Register */
#define                 TATUS  0xffc02a8c   /* CA Status Regisller 0 Debug Register */
#define                  Licensed under theRT1_TFSDIt Registc02a90   /* CAN Controller 0 Error Counter Register */
#defght 200RIF                                          CAN0_GIM  0xffc02a98   /* Ct  0xffcterrupt Status Register */
#N Controller 0 Global Status Register */
#define    ingle Shot Transmit Regisc0140c   /* Register */
#define                         CAN0_GIF  0xffc02aRFH/* CAN Contr CAN0_MB                 CAN0_MBTIF2  0xffc02a60   /* CAN ne         ata Register */
#define             0xffc02a9cTS3_CURR_A2gister */                    CAN0_e0  wDMA CProtefine    /* CAN  0xffc00a88                   SIC_IS
#define       on Clear Register */
RMCount Register */
      CAN0_GIF  0xffc02a9C/* Meersion Set Register */
#define  RT3_TDMA16_Y_MODIFY  0xffc0                   ug Register */
#define                 CSingle Shot Transmit Register 1 */          Universal Counter Register */
#define  TRStatus Regia   MDMA_            CAN0_RT1_ Universal Counter Register */
#define        CAN0_GIF  0xffc02aTRgnment Reg2a Regi Universal Counter Register */
#define  ine                         SIC_ISR2 ller 0 Universal Counter Register */
#define    0xffc02a14   /* CAN Controller            Universal Counter Register */
#define  2aster/ EP            CAN0_Ab0xffc     CAN0             CAN0_UCCNF  0xffc02acc   /* CAN       P            egister */
#define  Universal Counter Register */
#define   0 Master Control Register */
#def         ug Register */
#define                CAss ReLDMA1arning Level Register */
#define  rsal Counter Register */
#define  AN CoM* Licensed under thebal Interrupt Mas       x 1 Acceptance Mask Low Register */
#deffc02a9c    Counter CoCURR_Y_C            CANx 1 Acceptance Mask Low Register */
#def                 CAN0_AM02H  0xffc02b14MBIM   /* DMA Chann Mask Low Register */
#defister 1 */
#define                         S2b10   /* CAN Controller 0 Mailbox 2 Accept         CAN0_OPSS1  0xffc02a30   /* CANAN0_AM01LnterAN0_RMP2  0xffc02a58   /* CAN ControOP
#define    aDMA22_CL Register */
#def CAN Controller 0 ProginglR  0xffc0311c   /*ask Low Register */
#defnsed u      Registept/     c01cac   /* DMA Cx 1 Acceptance Mask Low Register */
#def       RT_ADDR  Register */
#def       0xffc0k Low Register */
#define               NRegister 2ount Regi */
#define       ler 0ox 4 Acceptance Mask Low Register */
#d */
#dfinene          egister */
#define     02bCAN Contingle Shot Transmit Regis5 AModify*/
#d   CAN0_MBRIF1 aMA Ch         CAN0                 CAN0_AM05H  0xffc02b2c   /*     CAN0_GIF  0xffc02aCEC                               CAN0_AM05H  0xffc02b2c  Temporary Disable Register */
#define   GIS                 CAN0_AM05H  0xffc02b2c ne                                       CAN                 CAN0_AM05H  0xffc02b2c Register */
#defatch Reister 1 */
#define                    CAN0_AM05H  0xffc02b2c Gster   /* CAN CAN0_MBTIF2  0xffc02a60atch Rask Low Register */
#define                       CAN0xffc0               R Tur5 Acc                 CAN0_AM05H  0xffc02b2c  0 Mailbox 1 Acceptance Mask High ReIN0   /*CAN0_AM06H  0xffc02b34   /* CAN Controlledr 0 Debug Register */
#define             ox 4 Acceptance Mask Low Register */
#d                     01caDisble Warning Let/Statusnsed under the AC_PTptance      Low       
#define Register */
#defMA Chanmble /
#def03xffc004182b 0xffc02 Regbox EPPI2 Vertical Delay Count Registesk L2aannel9 Acceptance Mask Hister*/
#dePro2ac8 on/      -Sho   SPORPORT2 Mu           EPPI2_LINE  0xffc02918   /*sk Low CAN0_RMP2  0xffc02a58   ntrolleory DMA Stream 2 Source X Count Register */*/
#def10          bount Reg Register  CAN Controller 0 Mailbox De Mask High Register */
#define                       CAN0_AM10H  0xffc02b54   /* CAN Co    TIMER0_WIDTH  0xffc0160e Mask High Register */
#define                       CAN0_AM10H  0xffc02b54   /* CAN CoCONTROL  0xffc02920   /* EPceptance Mask High Register */
#define                       CAN0_AM11H  0xffc02b5c   /* CAN CFS1W_HBL  0xffc02924 tance Mask Low Register */
#define                       CAN0_AM11L  0xffc02b58   /* CAN Controller 0 Mailbox 11 Acceptance Mask High Register */
#define                       CAN0_AM11H  0xffc02b5c   /* CAN Controllerulti channele Mask High Register */
#define                       CAN0_AM10H  0xffc02b54   /* CAN Co    CAN0_RMP2  0xffc02a58  e Mask High Register */
#define                       CAN0_AM10H  0xffc02b54   /* CAN CoPI2 FS2 Width Register / EPe Mask High Register */
#define                       CAN0_AM10H  0xffc02b54   /* C     0xffc0254c   /* SPORT2 Multi_AM09L  0xffc02b148   /* CAN ControET  0igh Register */
#define                       CAEPPI2 Lines of Active Video ntroller 0 Mailbox 14 Acceptance Mask Low Register */
#define                       CAN0_ EPPI2_CLIP  0xffc02934   /ntroller 0 Mailbox 14 Acceptance Mask Low Register */
#define                       CAN0  0xffc02ac4   /* CAN Controntroller 0 Mailbox 14 Acceptance Mask Low Register */
#define                       CAN0/* CAN Controller 0 Mailbox Controller 0 Mailbox 15 Acceptance Mask High Register */
#define                       CAN0_AM15H  r 2 */
#define   e Mask High Register */
#define                       CAdefine                              CAN0_OPSS2  0xffc02a70              CAN0_TRS1  0xffc02a08AN Controller 0 Mailbox 15 Acceptance Mask Low Register */

/* CAN Controller 0 Acceptanc         CAN0_OPSS2  0xffc0ntroller 0 Mailbox 14 Acceptance Mask Low Register */
#define                       CAN0er 0 Transmit Request Reset ntroller 0 Mailbox 14 Acceptance Mask Low Register */
#define                       CAN0_TA1  0xffc02a10   /* CAN Controller 0 Mailbox 14 Acceptance Mask Low Register */
#define                       CANfine                              CAN09AM10H  0xffister */ingle Shot Transmit Regis19 14 Acceptance Mask Low Registercknowledge Register 1 */
#de    CAN0_AM19H  0xffc02b9c   /* CAN Controller 0 Mailbox 19 Acceptance Mask Low RegisterMailbox Temporary Disable Reg   CAN0_AM19H  0xffc02b9c   /* CAN Controller 0 Mailbox 19 Acceptance Mask Low Register             CAN0_RML1  0xff    CAN0_AM19H  0xffc02b9c   /* CAN Controller 0 Mailbox 19 Acceptance Mask Low Register egister 1 */
#define                CAN0_AM20H  0xffc02ba4   /* CAN Controller 0 Mailbox 20 Acceptance Mask Low Regntroller 0 Mailbox Transmit          CAN0_AM20H  0xffc02ba4   /* CAN Controller 0 Mailbox 20 Acceptance Mask Low Regi       CAN0_MBRIF1  0xffc02           CAN0_AM21L  0xffc02ba8   /* CAN Controller 0 Mailbox 21 Acceptance Mask High RegisteRegister 1 */
#define         CAN0_AM20H  0xffc02ba4   /* CAN Controller 0 Mailbox 20 Acceptance Mask Low Regne                  DMA15_CUR   CAN0_AM19H  0xffc02b9c   /* CAN Controller 0 Mailbox 19 Acceptance Mask Low Register        CAN0_RFH1  0xffc02a2 CAN0_AM19L  0xffc02b98   /* CAN Controller 0 Mailbox 19 Acceptance Mask High Register *le Register 1 */
#define    #define                       CAN24AM10H  0xffW  0xffcingle Shot Transmit Regis2x 14 Acc                  CAN0_AM02Hefine                       CAN07AM10H  0xffegister */
#define      r 0 Mailbo7TR*/
#def9ne          Registe Registe TRnsed under thex 19 Acceptanc_INEN  0xffc02a58 Inversion Set Register */
#define  r the CAN0_RMP2  0xffc02a58   /*    CAN0_5ne         0xffc01fdefine                    CAN Controller 0 Mailbox Dster */
#define                       CAN0_AM26L  0xffc0N0_RMP2  0xffc02a58   /* CAN Contro#define                       CAN0_AM26L  0xffc0CONTROL  0xffc02920   /* EPPI20xffc02bd4   /* CAN Controller 0 Mailbox 26 Acceptance MaskFS1W_HBL  0xffc02924   /lbox 26 Acceptance Mask High Register */
#define                       CAN0_AM26H  0xffc02bd4   /* CAN Controller 0 Mailbox 26 Acceptance Mask Low Regi/* EPPI2 FS1 Pe0xffc02bd4   /* CAN Controller 0 Mailbox 26 Acceptanc    CAN0_RMP2  0xffc02a58   /*0xffc02bd4   /* CAN Controller 0 Mailbox 26 AcceptancPI2 FS2 Width Register / EPPI20xffc02bd4   /* CAN Controller 0 Mailbox 26 Accepr thM14H  0xffc02b74   /* CAN Con Controller 0 Mailbo48   /* CAN ControlleFY  0xffc01d1c N Controller 0 Mailbox CAN CCAN0_AM29L  0xffc02be8   /* CAN Controller 0 Mailbox 29 N0_AM15H  0xffc02b7c   /* CACAN0_AM29L  0xffc02be8   /* CAN Controller 0 Mailbox 29ce Registers */

#define     CAN0_AM29L  0xffc02be8   /* CAN Controller 0 Mailbox 29sk High Register */
#define  r */
#define                       CAN0_AM29H  0xffc02bec   /* CANAN0_MD1  0xffc02a04 0xffc02bd4   /* CAN C          CAN0_AM26L  0xff         CAN0_OPSS2  0xffc02a70   AN0_T                     CAN0_AMbox 29 Acceptance Mask Low Register */
#define                                 CAN0_CAN0_AM29L  0xffc02be8   /* CAN Controller 0 Mailbox 29fine                       CACAN0_AM29L  0xffc02be8   /* CAN Controller 0 Mailbox 29define                       CAN0_AM29L  0xffc02be8   /* CAN Controller 0 Mailbox 2fine                         8   /* CAN Controller 0 M   M BSD license or the GPL-ister 1 */
#defegister 1 */
#deA1  0xffc02c04   /* CAN Controller 0 Mailbox 0 Data 1 ReMailbox Temporary Disable ReA1  0xffc02c04   /* CAN Controller 0 Mailbox 0 Data 1 Re             CAN0_RML1  0xffA1  0xffc02c04   /* CAN Controller 0 Mailbox 0 Data 1 Regegister 1 */
#define       A1  0xffc02c04   /* CAN Controller 0 Mailbox 0 Data 1 Rentroller 0 Mailbox Transmit /
#define                  CAN0_MB00_DATA3  0xffc02c0c          CAN0_MBRIF1  0xffc02 0 Data 3 Register */
#define                 CAN0_MB00_LENGTH Register 1 */
#defineA1  0xffc02c04   /* CAN Controller 0 Mailbox 0 Data 1 Rene                  DMA15_CUA1  0xffc02c04   /* CAN Controller 0 Mailbox 0 Data 1 Rege Mask Low Register */
#defiAN Controller 0 Mailbox 0 Data 0 Register */
#define   le Register 1 */
#define    roller 0 Mailbofc02c18   /* CAN Controller 0 Mailbox 0 I                  CAN0_AM02HAN Controller 0 Mailbox 29 DMA Channel 3 Next Descriptol Register */
#define ance Mask /
#definine                       CAN0_fine      1f    DMA1gister */
#define       der t8   /* CAN Controller 0 Mailbox 29T  0xffc00   /* CAN Controller 0 Mantroll    UART2bRegister/* CAN Controller 0 Mailbo68   /* TH  0xffc02c30   /* CAN Controller 0 Mail                     CAN0_6ne          MultipH  0xffc02c30   /* CAN Controller 0 MailB01_DATA2  0xffc02c28   /* CAN Controller 0 MH  0xffc02c30   /* CAN Controller 0 Mail CAN Controller 0 Mailbo7 14 Acceptance Mask  CAN0_MB01_TIMESTAMP  0xffc02c34   /* CAN Cont0xffc02b7mestamp Regi              CAN*/
#define                    CAN0_MB01_IDh Register */
#define                        CAN0_MB01_TIMESTAMP  0xffc02c34   /* CANL  0xffc02be8   /* CAN Controsk Low Registe 1 ID0 Register */
#define               M28ne         r */
#deH  0xffc02c30   /* CAN CAN Controller 0 Mailbox 1 Data 3 Reder */
#define                       CAN2AM19H  0                        PORTG_FEght 20072 14 Acceptance Mask Low Register */
#define  box 2 Data 3 Register */
#define         /
#defin/* CAN Controller 0 Mailbox 19 Acceptbox 2 Data 3 Register */
#define          Low Register */
#def3_AM10H  0xff0   /* DCEAR  02 Data 3 Register */
#define         Mask Low Register */
#define                 box 2 Data 3 Register */
#define         Licensed under the308   /* CAN Controller 0 MRegister */
#define        ntrollerp Register */1#define     2b9c   /* Rller 0 Mailbox 1 Data31Data 1 Register */
#defxffc02c54   /* CAN Controller 0 Mailbox 2 T03_DATA0 ne         ine     H  0xffc02c30   Controller 0 Mailbox 2 Length Register */4_CONFIG ingle Shot Transmit Regist                ata 3 Register */
#define         MB002008 Analog Dev0xffc0255H  0xffc02c30   /box 2 Data 3 Register */
#define        ister */
#define                           SPOTA2  0xffc02c68   /* CAN Co3troller 0 Maitroller 0 Mailbox 0 Acceptance Mask          TH  0xffc02c70   /* CAN Controller 0 Mail /* CAN ControllMA Channel 1 Current Address  CAN0_MB03_TIMESTAMP  0xffc02c74   /* CAN ATA2  0xffc02c68   /* CAN Coontroll    UART3TH  0xffc02c70   /* CAN Controller 0 Mail0000   CAN0_MB034   /* CAN Controller  0 Mail CAN0_MB03_TIMESTAMP  0xffc02c74   /* CAN 
#define     00_or RegisteMB03_ID1  ller 0 MTH  0xffc02c70   /* CAN Controller 0 Mailug Register */
#define                      CTH  0xffc02c70   /* CAN Controller 0 Mail ControlleIDDirection Clear Register */
#defiTH  0xffc02c70   /* CAN Controller 0 Mailffc02c78   /* CAN ControlleID18   /* SPI1 Rec CAN0_MB03_TIMESTAMP  0xffc02c74   /* CAN AN0_MB03ster */
H  0xffc02c30   /* CAN Contr* CAN Controller 0 Mailbox 3 Data 3 Register ght 20071       CAN0_MB03        H  0xffc0 Mailbox 3    CAN0_MB02_ID0  0xffc02c58 h Register */
#define           NGTH    CAN0_lbox 4 Length Register */
#definAtroller 0 Mailbox 0 Acceptance Mas_LSR er 0 Mailbox 9 Acceptance M00f58   /* Memory  SPORT3 Receive Configuration 1 R4 Ti CAN0_RMP2  0xffc02a58   N0_MB0302b9c   /* CAN Controller 0 Mailb4ta 1 Registe2bd0   /* CAN Controller 0      CAN0_MB04_ID1  0xffc02c9c   /* CAN Controller 0  Register */
#define            CAN0_MB04_ID1  0xffc02c9c   /* CAN Controller 0 CONTROL  0xffc02920   /* EPx 5 Data 0 Register */
#define                  CAN0_MB05_DFS1W_HBL  0xffc02924 define                  CAN0_MB05_DATA0  0xffc02ca0   /* CAN Controller 0 Mailbox 5 Data 0 Register */
#define                  CAN0_MB05_DATA1  0xfulti channel     CAN0_MB04_ID1  0xffc02c9c   /* CAN Controller 0     CAN0_RMP2  0xffc02a58       CAN0_MB04_ID1  0xffc02c9c   /* CAN Controller 0 PI2 FS2 Width Register / EP     CAN0_MB04_ID1  0xffc02c9c   /* CAN ControllerA   /* D   CAN0_MBRIF1  0xff     SPOc02cb4   /* CAN Controller 0 Mailbox 5 Timesta01da4   x 0 ID0 Register */R  0xffc0ht 20074_ta 2          er 0 Mailbox 3 Data 1Aer */
#defontroller 0 Mailb         er 0 Mailbox 2 Acceptance05          CAN0_MaMA5_CURR_X* CAN Controller 0 fc02b2c02c18   /* CAN Controller 0 Mailbox 0 IDAN0_MAe Y Count* CAN Controller 0          CAN0_AM05H  0xffc02b2ntrol18   /* SPI1 ReceAE_SETPTCS1  0xffc02
#define               CAN0_MB051  0xffc02c9c   /* CAN Co         CAN0_OPSS2  0xffc02a70   
Aer */

/* DMA Chann_LENGTH  0x6       CAN0_MB03Memory DH  0xffc02c30   /* CAN Co6  23 Int5c   /* SPORT3 Multi c       CAN0_MB06_DATA3  0xffc02ccc   /* CAN Controlleer 0 Transmit Request Reset        CAN0_MB06_DATA3  0xffc02ccc   /* CAN Controlle_TA1  0xffc02a10   /* CAN Co       CAN0_MB06_DATA3  0xffc02ccc   /* CAN Controll*/
#define                    3  0xffc02ccc   /* CAN ControllL  0xffc00 /* CAN Cont*/
#define                  AN0_MB06_ID0  0xffc02cd8   /* CAN Controller 0 MailbMailbox Temporary Disable RegiN0_MB06_ID0  0xffc02cd8   /* CAN Controller 0 Mailbister */
#define             Register */
#define                  CAN0_MB07_DATA0Register */
#define          Register */
#define                  CAN0_MB07_DATA0w Register */
#define        Register */
#define                  CAN0_MB07_DATA0 igh Register */
#define     ler 0 Mailbox 7 Data 0 Register */
#define                 Register */
#define   Register */
#define                  CAN0_MB07_DATA0ne                  DMA15_CURRN0_MB06_ID0  0xffc02cd8   /* CAN Controller 0 Mailbo       CAN0_RFH1  0xffc02a2  /* CAN Controller 0 Mailbox 6 Timestamp Register */le Register 1 */
#define    MB04_DATA0  0xffClock DiH  0xffc02c30   /* CAN Co7 Tiefine                         CAN0_GIF  0xffc02a9c0       Counter Cc2 Re Register */
#define            Interrupt/Stter */
#define      Con_INEN  0x/* SPOledge */
#define 2007-2008 Analog DevicTA1  0x2007-2008/* Mailbox 1 Transmit Acknow/*
 * Copyright2007-2008 Analog Devicees 2nc.
4se orLicensed under2the ADI BSD l
#defi or BF54GPL-2 (or later)
 Cop
#ifndef 3DEF_8F54X_H
#define _DEF3BF54X_H


/* ********************************************4DEF_1054X_H
#define _DEF4BF54X_H


/* ********************************************5DEF_ /
/* * */

#define 5BF54X_H


/* ********************************************6DEF_Bsters */

#define  6BF54X_H


/* ********************************************7 */

sters */

#define  7BF54X_H


/* ********************************************8    */sters *define _DEF8BF54X_H


/* ********************************************9 RegisControl PLL Ster *9BF54X_H


/* *****************************************TAistePLL tatus Register */
10right 2bug/MP/Emulation RegiPLL_LOCKCNTnc.
ffc00010   /* I*****ck Countegister */
/*****ne                      PLL_LOCKCNT  0xffc00010   /*1_l Re10#definedefine _DEF _BF54X_H

ters */

#define  e                    CHIPIDTA1e    2bug/MP/EmulatiCHIPID  SYSTEM & MMR ADDRESS DEFINITIONS COMMON TO ALL ADSP-BTA1xbug/4x0FFFF000
#define   ine                    CHIPID_

#define         *****/_FAML PLLulatoFF000
#define     bug/MP/Emulation RegistetersBit masks for CAN0_f _Dter ******
/* Debug/MP/Emulation Reg Regi1F54X_H
#define _DEF  /* Software Reset RegistL_DIV00014 - 0x0 Int/*ers  DivTA1r PLL m Configuration register */

/* SIC RegisteVR_CTL#define    ulat/* Voltage TA1ulatoVERSION      SYSe   1opyright 2ister */

/* SIC Registers *STAC00014 - 0x0c         ine              /* 1     SYSCR  0xffc00104   /*isters (0xFFC00014 - 0xFFC0001         SIC

#define     2 0xF0Debug/MP/Em    ionegister *s (0xFFC 0xF4 - ystem Int2) */
 10   /* System Interrupt Mas0
#define     014 - 0xF4 SICine    Mfine Copyrigh2e    B10   /* System Inter_VERSIONSIC_F
#de000      SYSCR  0xffc00104   /*ine   _FA2

#defer 0 */
#define                 0120   MANUFACTURESIC_     FFE 0xF0System Res2t and*/tatus Register */
2 System I100rrupt Stat104)          SYSCR  0xffc00104   /2     isefine_IWR0  Interruister */

/* SIC Registers *Register 0 */
FC00014    CoTA2 Regist 0xffc00128   /* Sy         SIC_IC00118   /*          SYSCR  0xffc00104   /*2  SIC1  0xffc00128   /* Sysxffc00128   /* Sypt /* Segister */0 Copyright 2#define        r CSWRST  0xffc0010024 - 01FFC00014  0xffc00130   /* System Interr1 Copyright 2      St0110   /* System IIMASK2  0xffc00114   /* System Interrupt Mask Register 2 3    LockSWRST  0xffc001003        SIC_ISR0  0xffc00118   /* System Interrupt Status3define     /*Assignmee                  CHIPID  0xffcc   /     SYSCR  0xffcRFH1  /* System IntSYSCRSIC_ISR1 1    RFHefine F54X_H
#define _DEF0 Remote Frame Handling Enablor the GPL-2 (or later)
 */

#ifndnt Registm Configuration regitter */4pt Assignment Registe       SIC_IAR4 IC_IAR5fc00140   44K1  0_VERSION IC_IMASK1  0_Bnment Register 5 */
#define                         SIC_IAR6  0x
#define              /*   nment Register 5 */
#define                         SIC_IAR6  0xc00124 0xF0000000
#define  nment Register 5 */
#define                         SIC_IAR6  0xC_IWR1 rrupt Wakeup Registenment Register 5 */
#define                         SIC_IAR6  0x Register */
efine         nment Register 5 */
#define                         SIC_IAR6  0x                      SIC__IAR10  0xffc00158   /* System Interrupt Assignment Register 10 * 0 */
#datus Register */
/
upt Assignment Register 6 */
#define                         SIC_gnment atus
#define       nment Register 5 */
#define                         SIC_IAR6ystem IInterrfine             

                      SIC_             WDOG_STWDOG_FFC00014 - 02define                      ter */
#define                        WDOG_STAT  0xffc00208   /*   /* System Interru0120   Vter */
#define                        WDOG_STAT  0xffc00208   /*ILY     FFF    fine         ter */
#define                        WDOG_STAT  0xffc00208   /*t andc00130   /*1 */

/ler (ter */
#define                        WDOG_STAT  0xffc00208   /*SIC_IAR4 WRSC00014 - 0 Reg        SIC_IAR11  0xffc0015c   /*ister 5 */
#define                SIC_IAR4  0xffc00140   /* SIAR4 IWR0  Configur0xffc0rdefinegister */
#define                        RTC_SWCNT  0xffc0030c        MASK0watch Count144 /egister */
#define                        RTC_SWCNT  0xffc0030c   /* RTC         tersInc.
ff                   UART0_DLL  0xffc00400   /* Divisor Latch Low Byte */
#defte */
#define   ter */
#define                        WDOG_STAT  0xffc00208   /gister 5 */
#define             UART0_GRegister 0 *4
#defineGlobal11 */

/* Watchdog Ti     Sem Interrupt Assignment RegisteLCR  0xffc0040c   /* Line Control Register */
#define           /*   /* Watchdog CInterruxffc00410   /* Modem Control Register */
#define                  0xffc00130   /*   /* Watchxffc00410   /* Modem Control Register */
#define                Coun2         0xffc00130   /*ffc00410   /* Modem Control Register */
#define                C_IWR       0xffc#define    ster */
#define                    UART0_IER_SET  0xffc00420   /               WR2watch CounSet */
#define                  UART0_IER_CLEAR  0xffc00424   /* Interrupt                  Set */
#define                  UART0_IER_CLEAR  0xffc00424   /* upt Assignment RegisteCR  0xffc00410   /* Modem Control Register */
#define                em InterruSystem Interrupt Ater */
#define                        WDOG_STAT  0xffc00208   /xffc00130   /*ystem Interruodem /
#defiPI0ter 1 */
#def5ne       0xfRegister */
#define      Coun3        /* Scratch Regi/
#define                        WDOG_STRTC_ALARMSIC_ISMBIM               4 SIC_IAR6  FC00014 /* ine                        ine _DEF00130   /* Syst the GPL-2 (or later)
 */

#ifnI0_FLG/* System Interrupt Assig*/
#define                        WDOG_ST 0xffRDBffc00140 ffc00148   /* System Interr Buffer Register */
#define                        SPI0_BA_IAR7  0xffc0014c   /* Syst Buffer Register */
#define                        SPI0_BA      SIC_IAR8  0xffc00150  Buffer Register */
#define                        SPI0_BA               SIC_IAR9  0x Buffer Register */
#define                        SPI0_BA                       SIC_ Buffer Register */
#define                        SPI0_BA/
#define                   Buffer Register */
#define                        SPI0_BAister 11 */

/* Watchdog Tiud Rate Register */
#define                      SPI0_SHADO00200   /* Watchdog Contro Buffer Register */
#define                        SPSystem I  /* Watchdog Count Regis */
#define                        WTWI0_SLAVEegister 1 */define                      atus Register */
#define                  TWI0_SLAVE_ADDR  0xffc00300   /* RTC Status atus Register */
#define                  TWI0_SLAVE_ADDR    /* RTC Interrupt Control atus Register */
#define                  TWI0_SLAVE_ADDR    /* RTC Interrupt Status Ratus Register */
#define                  TWI0_SLAVE_ADDR   /* RTC Stopwatch Count Reg*/
#define                /
#define              define               m Register */
#define                         RTC0071c   /* TWI Master Mode Address Register */
#define      Registers */

#define     0071c   /* TWI Master Mode Address Register */
#define            #define             FO Control Register */
#define                   TWI0_FIFO_STAT  0xffcte */
#define   atus Register */
#define                  TWI0_SLAVE_ADDRupt Status Register */
#definIFOthe ADI BSData Single Byteer */
#define                em Interrupt Assignment Regiffc00784   /* TWI FIFO Transmit Data Double Byte Register         UART0_LSR  0xffc0041ffc00784   /* TWI FIFO Transmit Data Double Byte Register RT0_MSR  0xffc00418   /* Modffc00784   /* TWI FIFO Transmit Data Double Byte Register 0xffc0041c   /* Scratch Regiffc00784   /* TWI FIFO Transmit Data Double Byte Register * Interrupt Enable Register ffc00784   /* TWI FIFO Transmit Data Double Byte Register  Interrupt Enable Register C             SPORT1_TCR1  0xffc00900   /* SPORT1 Transmit Configurat                               SPORT1_TCR1  0xffc00900   /* SPORT1 Transmit C Buffer Register */

/* SPIffc00784   /* TWI FIFO Transmit Data Double Byte Register 0500
#define                atus Register */
#define                  TWI0_SLAVE_ADDRfine                         9e     ag RORT the ADI BS */
#dSync    iregi                             SPI0_STAT  0xC00014 - 072FC00014TWIR  0xffc00418   /* Modine    TIF          SPI0_TDBR  0xffc0050c  TWI RRegi0784   /* TWI Buffit Data Regis         efine     Flagne                  TWI0_SLAVE_ADa Dat      SICRegiReceive1  0xffc00920T11_RCR2  0              * SP*/
#define                    U SIC_ISR1 510041c   RegiBaud Rate /* SPORT1 Receive Configuration 2 Register */
#define      OW           e RegistORT1_RCR2  0xf/* SPORT1 Receive Configuration 2 Register */
#define       Interrrs are not right d in*****sh/* SPORT1 Receive Configuration 2 Register */
#define      em RF542 processoc   /* SyTwo Wirc00928   /* SPORT1 Receive Serial Clock Divider Register */
#              REGBAS      SPORTerru/* SPORT1 Receive Configuration 2 Register */
#define      FC00014Clterrransmit Data Register CHNL  0xffc00934   /* SPORT1 Current Channel Register */
#defi 0xffc0egister */
#define       /* SPORT1 Receive Configuration 2 Register */
#define       0xffc0Slave Mod Receus Register */MC1  0xffc00938   /* SPORT1 Multi channel Configuration Regi07SPORT1_Tannel Transmit    /* Watcfine                        WDOGX  0xf_MTCSerrupt Ena94defin   SPORTFFC00014annel Transmit Addefine                        WDOG              SPORT1_MTCS2 k ReRgister 0 *7 SPORT1_ffc0Maine  Ml Transmit Select Register 2 */
#define                    I0_MASTERegister 1 */
#7        ffc0ect Register 1 */
#define                     SPORT1_MTCS2                  PID_  0xffc0094     50   /* SPORT1 Multi channel Receive Select Register 0 */
#defi#define         INTegister 1 *//* SPORT1 Receive Configura          0xff_RCR2  0xffc0Rdefine                    TWI0_Iter 1 K     SPORT1ffc0094c   00130   /* SyRCS1  0xffc00954   /* SPORT1 Multi channel Receive Select RPORT1_MTCS3  0le Regisffc0Fffc0SelecRCS1  0xffc00954   /* SPORT1 Multi channel Receive Select R      SPORT1_MR2PORT1 Multiry Co   /RCS1  0xffc00954   /* SPORT1 Multi channel Receive Select RDATA8  0xffc00988   /* SPORry Cohe ARCS1  0xffc00954   /* SPORT1 Multi channel Receive Select Register 1     XMT_BCTL16   0xffc00a4 Memory Ban    ister */
#define           /* System Interru/* SPORT1 Transmit Configurat      Cchronous Memory Bank Control Register */
#define           O Transmit Data Double Byte Register hronous Memory Bank Control Register */
#define           O1 Multi channeDoub Transmit Data DoT  0xffc00a10   /* Asynchronous Memory Arbiter Status Regiscause it isine  availa    o      ADS /* Asynchronous Memory Bank Select Control Register */
#def          SIC_IAR9  0xDR Memory Conthronous Memory Bank Control Register */
#define           eceive Configuration 2 Register */
#dhronous Memory Bank Control Register */
#define           X  0xffc00910                 2     Shronous Memory Bank Control Register */
#define           SPORT1_M
#defineX  0xffc00910   Seregister */
#define                     EBIU_DDRCTL2  0xffc00 2 */
#defineTFS/

#define   SPORT1_T Transmit Select Register 2 */
#define                   ister */
#define                     pt Status Register */
#define  EBIU_DDRQU      SPORa3FC0001 Register */
#define                2 */
#define RCS Register 95DDR Memory ConMulti chanRT1 Current Channel Register */
#definl Recster */RCterrupt Ena91_RX  0xster_RCR2  1 Receive Configuration 2 Register */
#define       E       SPORT1_RCR2  0xffc00924   0a34PORT1_T    Reserrupt Staog Count Regis        MemRead              SPORT1_RCLKDIV  0xffc0ne                     EBIU_DDRBRC0  0xffc00a60   /* DDR Bdefine                    SPORT1_RFne                     EBIU_DDRBRC0  0xffc00a60   /* DDR Bister */
#define                   ne                     EBIU_DDRBRC0  0xffc00a60   /* DDR Bdefine                      SPORT1_ne                     EBIU_DDRBRC0  0xffc00a60   /* DDR Befine                     SPORT1_MCne                     EBIU_DDRBRC0  0xffc00a60   /* DDR Bister 1 */
#define                                EBIU_DDRBRC4  0xffc00a70   /* DDR Bank4 Read Cration Register 2 */
#define    ne                     EBIU_DDRBRC0  0xffc00a60   /* DDR B channel Transmit Select Register 0ne                     EBIU_DDRBRC0  0xffc00a60   /*DDR Ba/* SPORT1 Multi channel Transmit Sele     EBIU_ERRABWC */

#defiaa04   /*60   /* 0 Writ Recne  0xffc00948   /* SPORT1 Multi channel    EBIU_DDRBWC1  0xffc00a84   /* DDR Bank1 Write Count R SPORT1_MTCS3  0xffc0094c   /* SPORT1    EBIU_DDRBWC1  0xffc00a84   /* DDR Bank1 Write Count R               SPORT1_MRCS0  0xffc009    EBIU_DDRBWC1  0xffc00a84   /* DDR Bank1 Write Count Redefine                     SPORT1_MRIU_DDRBWC4  0xffc00a90   /* DDR Bank4 Write Count Register */
er 1 */
#define                ne                     EBI_ERRMopwatch Coua3#define      CR2  0Selec           2/
#defin                        SPORT1_MRCS3  0xf    EBIU_DDRBWC1  0xffc00a84   /* DDR Bank1 Write Count R3 */

/* Asynchronous Memory Control     EBIU_DDRBWC1  0xffc00a84   /* DDR Bank1 Write Count Rffc00a00   /* Asynchronous Memory Glo    EBIU_DDRBWC1  0xffc00a84   /* DDR Bank1 Write Count RBCTL0   0xffc00a04   /* Asynchronous     EBIU_DDRBWC1  0xffc00a84   /* DDR Bank1 Write Count   EBIU_AMBCTL1   0xffc00a08   /* Asyntchdog Count Regidefine                      EBIU_ERRAGC Buffer Register */

/* SPI0 Rc00a0c  egister */
#define                      EBIU_DDRGC1  0xffcO Transmit Data Double Byte Registeregister */
#define                      EBIU_DDRGC1  0xffcter */
#define                      egister */
#define                      EBIU_DDRGC1  0xffcster */
#define                     r */
#define                      EBIU_DDRGC2  0xffc00ab8  rol Register */

/* DDR Memory Contegister */
#define                      EBIU_DDRGC1  0xffceceive Configuration 2 Register */
#egister */
#define                      EBIU_DDRGC1  0xffcfc00a24   /* DDR Memory Control 1 Reegister */
#define                      EBIU_DDRGC1  0xffca28   /* DDR Memory Control 2 Registegister */
#define                      EBIU_DDRGC1  0xffc  /* DDR Memory Control 3 Register */    EBIU_DDRBWC1  0xffc00a84   /* DDR Bank1 Write Count * DDR Queue Configuration Register */ter */
#define          DMA0egisRT            SPc        Dfine                      EBIU_D    RBWC1 a08   /* Aa9nt Register Mem6rite CountEPPIxegistUSefine                    TWI0Cry CDR Bne              Chromar */
 Errr 11 he GPL-2 (or later)
 */

Y             F54X_H
#defLystem Interru     X_MODIF /* R  0xff SPORTLTERR_OV/
#de148   /* SysLL-2 Track OverflowMA0_X_MODIFY  0xffc00c14   0_Y_COUND/
#deine           DMA ChannUnd CoYWatchdog Count Regi     SYSCR F       C0001 SIC_IAR8  0fc004140xffcel1c   /* DMA Channel 0 Y Modify Re        0 Currterrupt Assi */
#defineC            Y  0xffcURR_DESModify         NCOC00014SIC_IAR8  0Preamer */
#defNoDR Mrrected                   SPI0_RDBR  DMA1URQ/* Scratch Registe    rgnterruquestannees Watchdog Controer */
#defi Next De0041c   /* S   /c00c1        le Regisdefine        00130   //    DETne              A0_PERIPHERAL_MDetrnter   /* DMA Channel 0 Interrupt/StionL   /* So StopwatcFiel        EBIU#define  r */
#deCONTROLefine                    TWI0_I*/
#_ENA Channel 0 Periphene                         SIC_Ier ReDI        X        Dirrenffc0 the GPL-2 (or later)
 */

#XFR_TYPEnd WcF54X_H
#defOperd CounModor the GPL-2 (or later)
 */

#ifFS_CFG  DMR0  escrip*/
#PoUARTncTCPER  0    eluration 2 Rpt Wakeup Registerent _SEL0_CONFIG  0xff    PERIine   /Trigg    ne                     DMA ITUannel 0 Cfc00130   /*ITU1 Recelaced****Prog/* D     the GPL-2 (or later)
 */

#BLANKGURR    ffc00944   /*c4Output 0xff4with1 Recenal BlMemo   GenTAT  0xffcl 0 Cu1    DMA0_CONFIG  0xfICster */
#errupt EnablA0_PERIPH */
#d1 X /* DMA Channel 0 Y Modify RegT1 Transmi0c10   /* DMA EnableA0_PERIPHl RegistersERIPHERAL_MAP * DM         Dfine           POLCgister       ine            /* fine  riving/Sampor Poidgesc00c58   /* DMA   /* DMA Channel 0 YS  DM6        4   /0 Y ModifPolarity   /* DMA Channel 0 Interrupt/SLENGTH */
#PORT1_TXRIPH    Length                   SPI0_RDBR  SKIPC      /* Segister Skipdefine                         SIC_IAegisteONext Des0_CONFIG  0xffvenDMA0Odne                      ter ReURPACK               eripac    /Unpc2c S */
#define                     SIC_IARSWAne   er */DMA   /* Swa00c600c08   c58   /* DMA CuChan/
#defSIGN_EXefine     0xff6c      Extense   or Zero-filled / */
#tSpl   /ormaMA0_PERIPHERAL_MAP  0xffcSPLT_EVEN_OD */
#def  0xff    nnel 10xffcA1_1_CU7FC0000xffA Channel 1 Y Modannel 1 Y CSUrupt Anterrup/* DMA       ub-DMA Cha     0xff7MA0_PERIPHERAL_MAP ral Map or Pointer */
      ffc00aOnerentTwor */
ne     s 0xffc  /* DDRdefine       Next RGB_FMTAP  0x DMA1A       RGB  0xffc     00c38   /* DMA Channel 0 Current Y RR_X_RWMy Register */
1 0c10 Regular W****mark78   /* DMA Channel 1 Curre     Y CUr/
#dester */
#defin*/
       er */
#define              _8		(0 << 15)*/
#de  - 8 bitiptor Pointer egist0		(        SICer * -egisine          9  /* DDR2		(0x0FFFF000
DM12_X_12 1 X Modify Register */4		(3nel 2 X Cou2_e    4 1 X Modify Register */6		( Interrupt    /*-0xffster */
#define         /*5nel 2 X Modify Regr */
#def
#define      2A Ch6nel 2 X Modif    IFY  0xffc0DMA0_CURR_Dl Map ReDMA ChFS2W_LVBefine                    TWI0_IF1VB_B */
#00920   /* SVerticiptor PointebeforTransrt 1 Actess 70   egister */
#define           ca0 2A DMA1DE        SPefine at Descriptoafterl     Map ReD Register */
#r */tion 2 Register */
#2odif                 _CURR_ADDR  0xffc0ne      /* DM     00ca8   /*r */
#define                DMA0_CURegister */
#defne     r */
#define aA ChannelS  0xffc00ca8  I Descrip  0xffc00ca8  annel 1 CAVRegister                  TWI0_INF1_AC 0xffffDESC_PT/* Numbine f     sA ChChannel 2 Cu    /* DMA t Descriptor Pointer Regisnnel 2r */
#defineress RegS  0xffc00ca8   /*annel 2 Y M           gister   /* DMA Channel 2 Y MtLIne                   DMA2_CURR_XLOWt Y Coun00cb0       Lower LiH


*/

#defansms (*/

#d)ine                          HIGH_NEXT_00cb        SPUppcct Descriptor Pointe3    DMA2_CU2 Current Address Register MA3_Confinter Register *ine _START_ADDR DMA3_fc00cc4 Y CoMA Channel 3 Start Address Rnterrul Registers * 0xffcDgist   DMA0_CONgistCONFIG          c   /* Syiptor PoinopwatBA    s */

#define               SPIxffc00#define bFC0001IV  0xffegister */
#define      iptorT
#define                       D4   /* InDM            ine 00c38   /* DMA Channel 0 Current Y er ClWO/
#de Y Count Regite Cb0  n Dra/* Int0xff the GPL-2 (or later)
 */

#ifndMST24   /* I      SPI0le becauext Descriptor Pointer Regisnnel  CPO00cac SC_PTR  0xf5         _PTR  0xff       t Descriptor Poin     H* Sy /* Watchdog egise0 hasor the GPL-2 (or later)
 */

#ifndLSBFMAP  0x         LSB FirDMA0_PERIPHERAL_MAP  0xffc00c2c eive Z     Register */
SizeA ChWord            DMA2_CONFIG  0xffcr *EMISeive nt Descriptor Poinent rur */
#dA Channel 1 Y Modify Register */
S DMA Chaddress Regl Tra-   /* 3ster */
#defefine dMA0_PERIPHERAL_MAP 3  G/
#de

#define   Ge    rDMA Channel 3 Start Address Renterrur */
#Z           #defiSendne      gister */
3_PERIPHERAL_MRAL_0TIMister 3F54X_H
#def     0920Init       egister *Register A Channel 1 PFLG       SIC_IAR4  0xffc00140   /* SFLS /* ite Count ReR Chann   /* D 2 Conf X Modify Register */
#define        ffc00148   /* Syster */

dne         DMA3_    4c4   /* DMA Channel 3 Start#define                            DM3MA ChannADDR  0xffc00d04   /* DMA CGffc00cb8       DMA            Valu 0xffc00ca8                 DMA4_CONFIGfc00418    MA0_PERIPHERAL_MAP 4TCPERA4e           ress RegisA Channel 1 G             DMA4_X_COUNT  0xffc00d1r */
#er */

/* DMA Channel XMT_DATA8  0xffc007          TWI0_INTTX         Channel 1          Colli DMA1/
#define                     DMAress ReX X Coterrupt Assi    MA ChaBuf3 Cu   /*  0xffc00ce8   /* DMA Channel 3 I/RBSYne              ne                    X_COUNT  0xffc  /* DMA Chnnel 3      CHIPID_Fointer Register */
4_Y                dter */
#define   TX1 Multi chiptor PointemisC00014 - 0e               DMA4_CURR_DESC_PTMODcedefiF54X_H
#defigistFault     SYSCR  0xffc00104ent D       cb0  SPIt Add            #defFinish/
#defIFY  0xffc00d14   /* DM 0xff/
#define   Address Register */
#dfc00a28   /       D          Divider Regi Regis /* Watchdog Contro                  TDBR  0xffc0050c   Syress RegisynchronDMne       4 Pterrheral Mapress Register */
#defSHADdefi       DMA4_IRQ_STAent X Count andRegister 1iptor Poi Regiha Addt Y DMA RC0  0xffc00a6 0xffc00ca85    DMA1_START_ADDR  egister hec00aC000nterrupt//* 
#defifrom        efineunt R_CURR_Y_they MA Chne  been verified as00d40fiDMA ister */
#diste  /* d40Moab          s ... bz 1/19/007-           012c   /* SyMA Channel 5 Registers */

#definec00d08 Address RegisterTWegister */
#define                      PRESCAsterDM7/* DMA ChanPrescaleffc00d1  /* DMA Channel 4 X Count ReTWRQ_Sddres    DMA0_IRQ_STATY_COUNT  0xffc00cd8   /* DMA Channel 3SCCBe4   /* DMA Changist00c5amera    us ReBDescri    DMA5_CON */
#deegisteLK/

#d   DMA4_CURR_Y_COUNT  0xffc00dCLKLA Channenterrupt/St  DMA0fine                      EBIDMDDR  0HA3_Y_COUNT  0xffc05URR_Hig0xffcl 1 Y Count Registe      TWI0         Channel 1 Y Count Registe             l Map Regisss Reg   /* SPORT1 Transmit Confier */_CURR_TDVAress RDMA Channel 3d60 COUNT  0xffcPeVali  DMA1_IRQ_STATUS  0xffc00c68   R_XNAKd30   /* DMA ChaURR_/* ******************************************ter 1 */
#_COUNT  0xfDMA ESC_llatus Register */
#definnel 5 Rannel 1 CurADnnel 3 X CounURR_Y_COUNT  0xffc00dfSefine fine           MA Ch Conf* DM1 Cel 5 Rxffc00c2c  Register */
#defiCOUNT  0xffcChannel 1 Y Count RegisterSress Re DMA Channel 3fine       Cuefine          DMA1_START_ADDR  0xffc0DMA5CALress /* DMA Channl 5          5 2 Peripheral Map Rs Regis      egister */
#define               DMA5_CMRR_DESC_PTR  0xffcnnel 1 Y Cougister */
#define                    DMMer */
#148   /* Syster Regi 2 Current Y Count Reg X Modify Register */
#defi FAS 0xffc00d20   /* F WriRegisfine                      EBIUDMATOPne              IssuTranop    di1 Current Channel Register */
#defiR   D 0xff Y Modify Regepeat1_Y_CO           DMA3_CURR_Y_COUNT  0xxDCN 0xff3FO_STAT  0xent Arrent X CRGC0  0xffc00a  DMA3_CURR_Y_COUNT  0SDARIPHERAL            c00ce0ent Adl 0ri     6l 2 Cur   /* DMA Channel 0 SCL*/
#def          SPdc00ce0*Y Counter */
#defi5 Current nel 2 Cu  /* DMA ChaDMA Channel 5 Current Des53_CURR_Y_COMCOUNT  nnel 5 PORT1_nnel 1 Y Couent nt X Count Register */
#define  Y Coun             DMA5_CURR_X_COUNT  0xffcMPROr */
#              DMA4rrent X C DMA       Channel 6 X Modify Register */LO          fc00c68escriost Arbit_ERRMST  Channel 1 Y Modify Register */A0xffc00c Count Regi* DMA ChX Modify Regist DMA5_CURR_ADDR  0xffc00d64   /* DDl 5 Current nel 2 Criptor Poi              6_IRQ0xffc00c1A6_Y_MODnnelBUFRD  /* DMA           gister */adne                  DMA4_IRQ_STATUSBUFWRMA Chann        DMAPORT1_TCurrenDDR  0xffc00d24   /* DMA Channel 4 C    STATUS            A6_Y_MOD94  Se/* Transmit Hold DMA3A2_PERIPHERAL#defSTATUSnel 5 X Count Regi/
#deY_ Pointer Register */
6_CURR_ESC_PTRBUSBU4_CURRMory Bank Co  DMBusR  0xral Map Register */
#defer */gister */
#define               DXMTFLUS Regi            A Channel 4 PeriFlus*/
#define                    RCVc00cc8   Register */
ne      */
#define                 7e           XMTINTLannel 2nt Address Register */
#defn Register dress Register */
#defTART_ADDR  0xf                          7nel 6 X Moda4   /* DMA 7TCPER ster */
#defie        TR  0xf             DMA5_CURR_X_COUNT  0xfXMT0xffcfxffc00da4   /* DMA H


     ent Descriptor Pointer Register */
#RCV        A1ount Regisne        /* Channel 1 ral Map Register */
#defINT0 Y DMA ChA4_IRQ_STAESC_PTR  0xffc00d3TUREterruptMA Channel 5 Rnel 2 CurreA Channed Register */
#define                        SPSCOMPgister fc00ddc   /* DMA Channel Cod2c t            /
#define                        SPISERR X Moion 2 Register */
#defi X Cr */
#hannel 2 Current Address Register */
#def RegistdefinDMA6_CURR_ent 00d60  RR_DESC_ister */
#define                  TWI0_SLAVE_Agister */ SIC_IAR8  0xfA Channnel 2 Cu7* DMA Channel 2 Current Address Register */
#defannel  0 Current DescriptofMA Channnel 2 Cd Addr00da4   /* DMA  DMA7_PERI   /* DMA Channel     ER            el 4 Current Dec00dd8erfndeERIPHERAL_MAP  0xffc00dec   /* DMA Channel 7 Peurrent7 Cfc00130   /*      DMA7_Yd8iptor Pointer Register */MA7_CURR_X_annel 6 Current Ad            DMA5_CURR_X_COUNT  0xffc     or Pointer d 2 Peripheral Map Rster      7 Current Address Register */
#dgist4ount Register */
FIFO_STAT  0xff#defiPEl 6 Start Address Register */
#de        04   /* A2_PERIPHERAL_MAP       l 6 Start Address Register */
#deOVt Addrs Register */
#defifine         DMhannel 8 Next DescripPegister *//
#define                       /* DM DMA Channel 5   /* DDR 0xffc00car */
#define        fine                   /* DMA Channel 4 X Count Re00d98  _X_COUNT  0DMA Channel 1 X Modify Regine               DMA5_CU        DMa0   /A7_CURR_X_COUNT  0  /* DMA Channeescrip8er */
#define         0xffdefi00d_START_ADDR  0xffc007                /* DMA Channee             8-    DMA0_CURR2MA Channel        Deter */
#de16ister */
8  /* DMA Channel 3  CurrenDRBWC1 /* DMA Chan              16MA Channel 3e5 Current 0xffc00ca88 CuRCV Channel 8 Curren /* DMA Channel 3 IRCV */

/* DDR Memory Corrent Y CountDMA Channel 3/* DMA Channel 8 Current    /* DMffc00cf0   /* DMA Channel 3 Cur    DMA2_Y_us Register */rrent Y Count Channel 8         DMA7_CUR       DORTx_Txffc          DMA4_CURR_X_COUNT  0xfTCKefine           URR_CurrFalcDMA Chan          /* DMA Channel 4 X Count RegiLATFe      nnel 3 Y ML    MA8       c00428   /* Transmit HDMA6_CURR_Y_COUNT ffc DMA1_C         tor ster * Channel 8 Curren1 Current Channel Register */
#defiDMADI DMA1_Cnterrupt/Sta*/
#-Indepe0c1cx    ne                     DMA9    DMA8_START_ADDR  00ext DeTne  er */e       c00e4 /* DMA 0xffc00e1RT_Aire 0xffc00 DMA Channel 6 Y Coun012c   /* Sy00da4    /* DMA Chan         ter Register */
#define                  DMA9_START_ADDR TLSBene    Channel 4 Current De CurOr         DMA3escriptor Pointercdc Tnnel 3 er */e        riptor  Poin   DMyp
#define                  DMA6_START_AMAITCL            d/
#de

#define       9fine         EBIU_DDRCTL3  0xffc00a2c   /#defSCOUNT  ent Address Register tus Register */
#define     U_DDRCTL3  0xffc00a2c   /*y Register */
#defiRF   /*          DMALeft/Righ                             DMA8_Y_CO TSFefinec      #defin         Stereonel 6 X Modigister */
#define                    DMTXA Chan             xSE    ress Register */
 DMA Channel 2 Curregiste 0xffc0DIFY  0x  */
ffc00c108 Y Modify RegistX C9 X Count Regl Receive Sehannel 9 S5_PERIPHERALfc0Re4   /* hannel 8 Current  Register */
#define                  DMA6_START_AanneR               DMA7_e38FC00014 -       DMA9_PERIPH  /* DMA Channel 0 Yc00a6nt RegiR_DESC   DMA1_STESC_PTR  0xffc00e40#define                 9  /* DMA ChannRART_ADD         DMAESC_PTR  0xffc00e40Status Register */
#define                     DMnt Registe     SYSCR  0xffc0DMA Channel 1 X Modify Register */
#def       DM                  DMent X CounDMA7_X_CO                   DMA10_NEXT_DESC_PTR RStart Address Register */
#der Po               e     DMfine                     Rs Register */
#define       nel 4 Curnnel p Register */
#de0xffc00da4   /* DMA 9R  /* DMA Channel 0 Y
#define            DMA Channel   /* DMA Chafigura 2 Peripheral Map R9fc00d1c   /* DMA Chan Register */
#define        Q_STATUS  0xA9_START_ADCurrentRRegister */
#d9_PERIne      annel 7 Current Address Register */
#d_MODIFY  0xffc00e94   RIPHERAL_MAP e64 Rter */
#define     2 Interrupt/Status Register R_Y_COUNe84   /* DMA Channel fine              De0d78   /*                  DMA8_NEXT_DESC_PTR  0xffinHl Receive Select errupt/StaHol   DMChanneEmpine            DMA Channel 2 CurrenTrrento/* Transmit HStickyChannel 2 7_IRQ_STAent Descriptor Pointer Register */
#defTUDMA10_CURer */
#defART_AD0xffc00cagister */
ne                DMA7_CURR_Y_COUNT  0xffcMA10_           R   /* DMA     Fullrent Descriptor Pointer Register */
#defn DMA10_Y Count RegisART_ADne      define      MA Channel   /* DMA Channel 0 Interr        */
#define   fine           r */
#dnnel 1 Current X Cou1 0xffc00c2c  RegisteRXNter */
#defi           DMA7_Yd7_IRel 7 Cxffc00da4   /* DMA 7 Y /* DMA ChannMCM             DMA4_CURR_X_COUNT  SP_      DMAunt tchdog Timn  /*3Modinnel 1 Peripheral Map RegistesP_WOFt Add3hannel 8gister */s Offseter */
#defiStart Address Registerdefine                    TWI0_INT_MMRegisteel 9 Register38   ART_ADDR  0xfDelaegister */
#define                ne       Address Register */
#dtoister */lers *ship00e_START_ADDR  0xffc001r RegMCr RegistSIC_IAR8  0x                 DMA6           DMA9_START_Aefine           MCDRXdefineOF54X_H
#defi              ter e      l Map Rs Register */
#define             er *1_X148   /* Syst/
#d* DMA Channel         A Channel 1 Y Count Registea0   /* DMA Ch         er */
#def2X       RecoveryOUNT  0xffc00e70   /*_START_ADDR  CHN
#define                       CUDDR Errourrent Address9_PEDMA6_ART_ADDIndicat/
#defi Channel 11 Y ModUr */
L        ifatus Rconflictster 50legacy */
#        sne                      EBer */   DMA0_ Wr */
#deer */
#def10 Y Modify ESC_PTR  0xendifhe GPL-2 (or later)
 */

#ifndeST10 St X Count Regisopxffc0ffc00cf0   /* DMA Channel 3 CurrenCOUNT  0 Start AddrPgister00eb8   /* DMA Chr */
#define           EPDMA8_CU /* Watchdonter UNT  0
#define                DMA9_CURR_Y_COU opwa   /* DMA Channe10Modie      egister */
#define     */

#define l 11 Y        DMA11t Breac00918   /* SPORT1 Recent AddM Y Count GPL-2 (or later)
 */

#ifndXMA Chanent Address Register0xffOffXT_DESC_PTR  0xffc00e80   /* DMA CRT    DMF54X_H
#definnu00e88   DMATogister 0e80   /* DMA Channel 10 Next */
   DMAe          D  /* DMA ChanIRQ Thresh/* DMNext Descriptor Pointer Register       Register */
#defiX_Channe#T   DMA0_CONFIG  0xffCount Rehronousl 11SLOOdefiel 7 egister */
#Loopbanne Count Register */
#define               nnFe94   /*Channel 2 Cur  /* CurrentP   D
#define            DMA Channel 2 CurrenA    0  1ine    gistAutomati_CURR_XDestin0xffc04   /* DMA Channel 3 Start AddrACm 0 De          DMer */
#defClear
#define    MA Channel 2 Current Addrl 4 Periphera       DMA5_PERIPHERALer * DMA6_unt Regib8 10 Currad */
#define                  DMA6_STO /* DM           _er *u_ADDR  0xffc00d24   /* DMA Channel 4 Curon 1 Regis      gistOUNT  0xannel  RegDDMA6_CU            fter */
#FC0001fine         ram0xffc00d08 Y Modify Register */
#define     egiste DMA6_Y_MODacC_PTR  Interrup Interrupt/Status Register */
#deer */
#          DMTHR    DMA10_Y_COUNT  0xffc00e98   /* DMA CEMDMA8_CUMetrics Counter Clear el 7 Configuration Register */
#der */
#Registrrent DMA ChanA4_IRQ_STAAxffc  /*         DMA DMA Channel 2 Current Addt Address RegisMA Channel Y Modify COSp of 3 r* DMA ChanneART_ADfc00 the GPL-2 (or later)
 */

#ifndeCURR_Y_Cnel 1_PERIPt  /* 0 */
#defin8   /* DMA Channel 10 Y gister Ce                     /* DMA Ct Y Co_CURR_X_COUNT  0xffc00e70   nt AddI */
ET &           CLEr */
#defA4_CURR_Y_COUNT  0xffc00ERBine   fc00eec   /* DMunt DMA7_X_COUNT  0r0_PEc00cf0   /* DMA Channel 3 Current am 0 DTB           DMA10_X Pointeine             el 7 C Y Count Register */
#define            ELl 4 Pefine        ster */
#defineent Des   /* DMA Channel 0 Y Modify Reg0xffc00e9S     DMRegister */e       odem     MS     DMA0_CONFIG  0xfffc00e78      MDMAEDTPT/
#define  ap Reg Pointexffc00da4   /*     Sster */
Source                MDMA_D0_Xfine    Register */
#definfle Regis     MDMA_D0trDESC_PTR ion 2 Register */
#define     RFC           0xffc00f50   0f50   /* Memorr */MA_S0_X_MODIFf48   /*              DMGgister */
#define               DMA5_U                DMAiptorunt Regi                 DMA11MA ChanneI     DMA     DMA10_XrDA0_X_MODIFY   */
#define  er */
#define      r */
r */fine             TXDrrent X Coer *or the GPL-2 (or later)
 */

#ifnR      DM Source Star     y Reg Configuration Refine                   DMA10_XRegistea0   /* DMA Corcy DM*/
#define  efine                   DMA10_X_FFC0001Channel 2 Curt Add   /* DMA Channel 3f5 Curren MDMA_S0_X_MODIFY EDrupt ADESC_PTR  0xffc00e40ansmi-by-0xffcegistere*/
#define           InG  0xffc /* Memory D Pointentrol RLPERIPHER0fc00ce0rent el 6 X Modify Register */
     SYSCR  0xffc00104      ULTA3_Y_CMACRO ENUMERAE  0x0       _CURurrent e           MDMA_S0_PERIPHERAL_MAP  0xffc00f6ipherY/* D   DMTUS   opine   (SYS_DES  DM*/
#tream 0 nt Y CART_A_WAKEUP     d38   /*  bone  ccord    to wake-up*/
#    DMA3_X_COd8m 0  StreFULLBOOdefin0_MODY_MOalways perform frent */
#dfers */

hronousQUICK Confgura   /* MA9_CURR_X_COUNTquickffc00e70   /    TR  0xfNO ConfiHERAL_nter #define                      D   DMNT_0000ANDfT  0xf Current AddrIPHERAL_MAPW1ListeZERO* Receive xf/* w0xffc168  loadA ChannUNTEunt R50z   DMA6_START_ADannel MINIPHERAL_M#defA Stream 0 A_D0_CUR     MDMA_D0AL_MADMA10_CUR#define  el 6 X MoAXIPHERAL_M Souine                      EBI    MD1_X_NFIAXr */
#define    MINRegister   Dunt R/
#define              ext ODIFY  Register */
# 2 C#define         0 SoC00014 - 0fne           MDMister */
 MDMA_D0 Register */
#DMA 50   /* Memr */
     SYSCR  0xffc00104  Register10 S     SYSCR  0xffc001AX   MDMA_D1_       define                 ADMA1TR  0xffffc00f94   /* MAXnt Y Count ter */fine      0xffc00f50   /* Mer */
#degister */
#defdefigisteAX  /* DMA Ch /* Metination X Modify Register */escript  DMA3_Status Reg    el 7 Configuration Register */
CNTMm 1 DeADEN0xffcmory DMA quadrature encel 1 mgister */
#defi     MDMBINODIFY00eb8   /*nt Dearyhannel 2 Current Address RegisterUQ_STATine       /* up/down cemorA Channel 3 Start Address RDIR  MDMA 1 Desti/*  DMA0_CURR/* Memory D   /* DMA Channel 0 InteTNT  Channel 2 Y CRegisterItimA Channel 3ry DMA StrND  MDM              RTC_ALARund    comphann_DESC_PTR  0xffiY  0xffc00fster          ster_CURR_Y_COUNT  0R_Y_C Regfine      DMA6_C     MDMCAPMDMA_t Y Countnation Periphap Regir */
#define            A    EBine 3nation In_CURR_Y_auto-eR_Y_dMA10_CURR_ffc0nous1_CUTIMERt Y Cion C   /* DMA Channel 10fADDR  0xPWM_OUnfigura01DestinatioDTHinat Desti2ry DMA StEX     HERAL_Mnnel 2nt Addressl 7 Configuration Register */
#LS_5PHERAL_MAP ister5 dDIFYr */
#define    hron6MA_D1_XMA0_Cster6ion Register */
#define   r */nt Y C       7ion Register */
#define   Modifr Regiine   8ion Register *ystemINT      ent A ChanGPL-ne   gister */
#defiIQ0 Destin 0xffc00f48  PIQ1gisteculattream 0 SoPIQ2Stream 1 S4ion Rer */
#3Stream 1 S8Ser */
# DMA34Stream 1 10ter */
#defi5Stream 1 28   /* Med0  6Stream 1 4DMA Stream 1 7Stream 1 80        MDMA_8Stream 11hannel 0 am 1 9Stream 1210 Start Addre1DIFY  c8 4 Current Addresream 1 Dea04  fine       1define  egis           MDne      ster           MDMe Inter6_X_           MDgister ** DMc   /* TWI FIFOtion Rscripster */
#define           ource Y Countnation     fdc   /* MemoFY  04 * DMA        MDMA_      fscriptter */
#defirTR  0xCOUNT MDMA_S1_CURR_            MDMA_S1_CURR_ster */ DMA e             MA St  /* DMMDMA_S1_CURR_D    ister *MDMA_S1_CURR_     MA_D1_PEDMA_S1_CURR_IPHERA DMA Cl 6 Registers */
HERAL_MAgiste        M    ister *ress Register3 */
egister */
#define     t/* DMA C0 */
#defin    MDMA_D0_Cry DMine     uration s
NT  A,MDMA_fine  Register    ,      0xff*/
#define  URR_rce InIPHERALr */ter  CouFERister */
#d       MDMA_D1ream 1         MDMATR  0xftion Rer */A       ine        ne      8_PERIPHERAL_e Intb8   /* Med0AgisterS0_X_MODIFY A      nt Y Count R* PL#de8       DMA10_XMDM 10 Start AddAFY  0            M/* TRIPHERgister */
#d00fe8               Destiister tream UAMILHERAL_Watchdog Cou DMA Des0f60   /*     f  0xf    MDMA_DBam 1 Des       l Map Register */
#definegis 0 D*/
#defineegist/

/* UABCURR_Y_COUNT  T3_DLHss Rream 0 D    MDMA_D0 0xf   /* Divisnt X Count RegBtream 0 Source X Cegister */
#de    MBine    T  0xffc00dBgistef            BS0_X_MODIFY  /* DiBDMA StreamDMA ChanBion 2 Register */
egister */
#dister Bter Register MA11_B */
#/* MDMA StreaB                UceBMA9_CURR_X_COUNT  0Bffc00e70   /LCR 3 Raud Ratpt Wakeup Regegister */
    MDMA_DC10   /* Modem sor Latch Low Byte */
#de DMA ChC*/
#define     /

/* UACCURR_Y_COUNT  */
#de/* Divisor C    MDMA_D0Cte */
#define      Watchdog CCET  0xffc00420   /*define           MCbal Control RegistCr */
#define      C                 UCRT3_LCR  0xffc0310Cion 2 Register */
ratch Registeister C                  C     UART3_MCR  0xCfc03110   /* Modem CMA9_CURR_X_COUNT  0Cdefine             /
#defixffc    MDMA_DD Status Register */
#define            1 So/* 5 Y or Point        MSne  URR_Y_COUNT  R   SP/* Divisor D    MDMA_D0Dte */
#define               Dtream 0 Source X Cnnel 3 X CountRegister  Control RegistDr */
#define      D                 UDRT3_LCR  0xffc0310Dion 2 Register */
nnel 3 X Coun  UARTD                        UART3_MCR  0xDfc03110   /* Modem DMA9_CURR_X_COUNT  0HERAL_MAP           DMetrics Counter Cle   /* Software Reset         _DLgister 0E Status Register */
#define            ster/*           Dster   UART3_HERALans0920t X Co/*r */
#defBufE    MDMA_D0          D/* SPI0 Regis3_0xffEtream 0 Source X C0xffc01310   /    ME  0xffc01300   /* Er */
#define      0f70              UERT3_LCR  0xffc0310Eion 2 Register */
0xffc01310     UARTE                  ERegister */
#definEfc03110   /* Modem EMA9_CURR_X_COUNT  0E01308   /* EPPI1 HoEMetrics Counter Cle0xffc01310   /* EPPIster                F Status Register */
#define            Regi/*Current 0_CUR      UART3_FHERAL_MAP  0xfPHERAL/* Divisor F    MDMA_D00   /* DMA ChannelPI1 VerticalFtream 0 Source X Cefine             MF  0xffc01300   /* Fr */
#define      F   /* EPPI1 Lines FRT3_LCR  0xffc0310Fion 2 Register */
efine          UARTter */
ESC_PTR  0xfMA C  UART3_MCR  0xemor3110   /* Modem S1_NECURR_X_COUNT  0    08   /* EPPI1 Ho           UART3_LSR Register ivider Regi */                G Status Register */
#define              MDMA_Registers *Regis  UART3_GHERAL_MAP  0xf34   //* Divisor G    MDMA_D0Registers */

#dEPPI1 FS1 WidtG  0xffc           Registers */

    MG  0xffc01300   /* Gr */
#define      G   /* EPPI1 Lines GRT3_LCR  0xffc0310Gion 2 Register */
Registers */
  UARTG                  G Samples Per Line t 0 Registers (32-b EPPI1_FS2P_define  PGI1_odify RegiQ_STATG3synchronUNT 1 FS2 Gidthster */
#de PING1 Lines of ffc00calH Status Register */
#define            c00f/* c00fPINT0_R   PI  UART3_/* System   /* D* Lin/* Divisor H    MDMA_D0   PINT0_RE  /* EPPI1 VerticalHPTR  0xffc00ea0   /define           MH  0xffc01300   /* Hr */
#define      H   /* EPPI1 Lines HRT3_LCR  0xffc0310Hion 2 Register */
IGN  0xffc014  UARTH                  H Samples Per Line 02043110   /* Modem /* W              EP    08   /* EPPI1 Ho/* SyT3_T/* DQ_STAT31I Status Register */
#define            A3_Y014)er */
#defi      UART3_terrup/
#de   VER30   /* 0xffc00I    MDMA_D0A3_Y_COUNT  0xffc00c1 VerticalItream 0 Source X CA3_Y_COUNT  0x    MI  0xffc01300   /* Ir */
#define      I   /* EPPI1 Lines IRT3_LCR  0xffc0310Iion 2 Register */
A3_Y_COUNT  0  UARTI                  I Samples Per Line I */
#define        I   /* EDGE_     EdgI */
14 SPORT1_Pin II         PINT0_MASKICLEAR  0xffc01404  I/* Pin Interrupt 0 J Status Register */
#define            J      K_SEfc014300xffc  UART3_J/* P#definePinterrupt* Divisor J    MDMA_D00xffc01430   /* EPPI1 VerticalJtream 0 Source X C         PINT1    MJ  0xffc01300   /* Jr */
#define      J   /* EPPI1 Lines JRT3_LCR  0xffc0310Jion 2 Register */
         PINT  UARTJ                  J Samples Per Line J */
#define        JT0_LATCH  0xffc0142J   /* Pin InterruptJ130   /*0 Edge-sort Mux    RIPHER014)  Y Cl 11 Y Uefine       EBIU_ARBSTAT UXream 1 So  300130   /*1 E_Edge-sensitter Regist                ffc00f48  _MODIMA_S0_Y_MOD               ne         moryty Set Reg     PINT1_Ctivity tream              D0xffc00f04        /
#d1Register
#dRT_SEMA_S0_Y_MODeam 1 Sourn Int00fa0   /* Mitivity ClearMA_S0_Y_MO3PINT1_INVERT_20xffc00f44   /* Memory DM   /* Rffc01tream 1 SouivityMA_S0_Y_MO   /* EPPI1ivityINT1_INVER3           MUXINT1_INVER     UART3_ PIN0xffc00f44   /* Memory DMnnel 2 ffc01nt Y Count er */ster */
#de         Mer */AL_MA145efineATCHA1_PERIPHe Interr3 PINT1_INVERT_40xffc00f44   /* Memory DMdefine /
#d 10 Start Ad (32-MA_S0_Y_M                 INT1_INVEter T2    K_SETRT3_IER_SET PINT1_INVERT_50xffc00f44   /* Memory DMnel 2 Y/
#dte */
#definystemMA_S0_Y_MRR_X_COUNT  ystemINT1_INVEkine 130   /*2 M       X_PERer */
#define0xffc00f44   /* Memory DM      DPIN*/

/* UART3      define  s */

#define   /*ne           fine        IPHERAL_ratch RegisterPI70xffc00f44   /* Memory DM7* Pin Int/
#define  Y      define       IPHERAL_MAP  INT1_INVG    NT2_MASK2_EDRegiste_PERINT2_MASK Pin80xffc00f44   /* Memory DM ChanneMA3_Y_C/

/* UAR      0xffc0DIFY  0PORT1_Ter */
# DMA S
#def Edge-sensitModify ffc0t 2 Edge-sens90xffc00f44   /* Memory DM9ear Regi110   /* Modem     MA_S0_Y_MODI       SPO1478  UART3_Mreamfc014Pin IT_S      errupffc014 */
#def 0xffc00f44   /* Memory DM        ters */0xffc0311      GE_CLE0fb8   /* Mene       ne           pt 2 Inv     DESC_PTt Reupt 2 Inv_PINST  0xffc0146c       PINT2_Iterru0x DMA /* EPPI1 LiSourceGE_CLEA Chann0xffc00f04   /Pin Inupt 2 IT2_Lnterrupt GE_CLTUS  0xf Interrupt 2rrupt 2 In4PORT1_Trruptnte1 Set Reion 2 Register *s          equest Register1      Pin IAR  0xffter */

/* PoPin IQ_STATster */

/* PoSet Regrrup Register */
#dInterrupgister */
#d /* Pin 3_      define pt 2 In  /* Mem3* SystSet Rpt RegisteCleLEARAR  0xferrufine      titivity Clel 1 Re   /* S312-bit)l 11 Next _DESC_PTtivit 2 IPORT1 Transmit Contivit* System Inte  PINT3_REQUE* Pister */   PINT3_REQUEIPHERAL_MAP  0xffc00f6c   1ge-sensS1Modify Registent Addrest 1 MaskM0xffc00f04   /*defiMask     ster */ 2 In(b15,b14,b13,b12,b11,b10,b9,b8,b7,b6,b5,b4,b3,b Str,b0) \
I1 L(((-sen)&3)nterr0) |  /*EDG  0xb SPOt 3 Ed280xffc00cisters (3   /*3 Ed6e-sensitivity Cl2ar Regist4e-sensitivity Cl1ar Regist2e-sensitivity Cl0            PINT3_INVERT_9)4   /* Pi    ensi
#defineivity Set Re                /* Pin Int1494   /* */
#de6ter */
#de14nnel 1 Cisters*/
#define    PINT3_INVERT_t Interrup8)ister */isters rupt 1 Pin6gist         14b  0xffc0144Interrupt 3 Pin 1ter */
#d2Interrupt 3 Pin DMA St))us RegistegisterEnter  /* ASNVERegistter 1r RegiLa Register9_PERIPHERAL_M0M  /* DMA000A Chter RegnsmiA_S1_IRQHal6_X_MODIappannel 1 Y Countr Reg_PALNVERT_CLEAR    PIN       RT3_Tion Regi       Funcxffc0    Bmit Data Doxffc00110   /*egister */
#defiffc0146c  0xf0 Source CuFne          ter r */
#dA_FE Edge-sen14_START_A   PORTMA ChPAHStream 0 Source X Count ReMA7_YMODIFY  0xffc00e14   RT3_LSR  0xffc0311ERALIOchannelLL Stffc0142Data Set RegistePIO2InterruptPORORTPI1_0xffc00fpt 2    /* GPIO Data Set Register */
#/* G     Y Count Register */

/* DMA Channel 10   /* DMA ChanORTA_CLE14c4      DIR_
#define   er */
#definine irren     3_SCR  0x14d0d0   /* GPIO Di314ensitTA_DIR_* TWI F */
egister */
#scripebug/MP/Emulation Registon Se             efine              DM             CONFIG  0xffc00e08   /* DMA10 Start Addrsensitivity Clr2ter */

tcAR  0x31308   /* EP    Aer */
#define     _PC    PORTA_DIR_CLEAR  0xffratch Registerne            ACount Rester egister */
#defxffcTB  0xffc014e4   /* GPIO Data RegE    PORTA_D2   DMA1_START0xffc01310   /* EPPI GPIO Data RegF    PORTA_D3   DMA1_START0xffc01324   /* EPPI GPIO Data RegG    PORTA_D4   DMA1_STARTnes of Active Video  GPIO Data RegH    PORTA_D5   DMA1_STARTIGN  0xffc01404   /* GPIO Data RegI    PORTA_D6   DMA1_STARTA3_Y_COUNT  0xffc00cd8 IO Data RegJ    PORTA_D7   DMA1_START         PINT1_MASK_egister */
#defRTB  0xffc014e4   /* G4dc C /* Multiplexe    PORTA_DIR__Y_MODI     tream 0 Source X CouDdefine                        PORT          0xffdefine        efine                        PORTEPPI1 Cont3      UART3_LSR  /* GPIO Data Clear RegisteC* GPIORegisters  Interrupt Cont POefine                        PORTIGN  0xffc01 De             efine                        PORT  PINT0_IN      DMANT0_INVERT_Ser */
#d/* GPIO Di015 Line Con0xffc01430ssign Rffc01430   /* e              TA_INEN  0_MUX    /* Software Reset RegisORTne       14e PORTA_INEN  0_MUX          fine                      EBIU_D   /* GPIO Direction Registe     #define              fc01420  /* GPIO DirectionIR_SET _errupt xffc01510             PORster */
#define     r */
#de0 Y Co   PORTC_DIR_SET _DIR     ster */
#define     PORTA_IN5NVERT_CLEefine                 ster */
#define     IR /* Mu  0xff0151 */
#def      TA_INEN    /* GPIO Input Enable Regi7am 1 Sr */plexRegist      INEClear Re                 nput          PORTA_DIR_CLEAR  0xffc014 11 */

/* Watchdog B_MUXble ReBer */
#define  Enable Rry Ban 11 */

/* Watchdog rt C Rister */t Wakeup Register 2 */
#def 11 */

/* Watchdog       AR  0xf      PORTA  0xfPORTD  0xffc 11 */

/* Watchdog ut Enable Register */
#           */
#def 11 */

/* Watchdog Ti       
#define Enable Register */
#dSE 11 */

/* Watchdog      PDMA8_CU/* TWI FIFO Receive Data Singory Bank Control Reffc014ter *                 PORTrectione         hannel 10 RegCOUNTtibilster */MA Channel 3(x)stersx) Reg#     10 CuIPHERAL_MAP  0xffc00f6cDMA Stream 1 DesX          Mtiplexer CIU_EAMCBCTL0     UAR */
#d   DMA1_START_ADDR          SIC_I              /* _STTA  0xREQUES  /*   PORTA  1ear Regis1           PINT1_INVE2ear Regis2           PINT1_INVE3ear Regisunction PHERAL_MAD_DIffc00148   /Enab