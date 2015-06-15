/*
 * Copyright 2005-2008 Analog Devices Inc.
 *
 * Licensed under the ADI BSD license or the GPL-2 (or later)
 */

#ifndef _DEF_BF534_H
#define _DEF_BF534_H

/* Include all Core registers and bit definitions */
#include <asm/def_LPBlackfin.h>

/************************************************************************************
** System MMR Register Map
*************************************************************************************/
/* Clock and System Control	(0xFFC00000 - 0xFFC000FF)								*/
#define PLL_CTL				0xFFC00000	/* PLL Control Register                                         */
#define PLL_DIV				0xFFC00004	/* PLL Divide Register                                          */
#define VR_CTL				0xFFC00008	/* Voltage Regulator Control Register           */
#define PLL_STAT			0xFFC0000C	/* PLL Status Register                                          */
#define PLL_LOCKCNT			0xFFC00010	/* PLL Lock Count Register                                      */
#define CHIPID				0xFFC00014      /* Chip ID Register                                             */

/* System Interrupt Controller (0xFFC00100 - 0xFFC001FF)							*/
#define SWRST				0xFFC00100	/* Software Reset Register                                      */
#define SYSCR				0xFFC00104	/* System Configuration Register                        */
#define SIC_RVECT			0xFFC00108	/* Interrupt Reset Vector Address Register      */
#define SIC_IMASK			0xFFC0010C	/* Interrupt Mask Register                                      */
#define SIC_IAR0			0xFFC00110	/* Interrupt Assignment Register 0                      */
#define SIC_IAR1			0xFFC00114	/* Interrupt Assignment Register 1                      */
#define SIC_IAR2			0xFFC00118	/* Interrupt Assignment Register 2                      */
#define SIC_IAR3			0xFFC0011C	/* Interrupt Assignment Register 3                      */
#define SIC_ISR				0xFFC00120	/* Interrupt Status Register                            */
#define SIC_IWR				0xFFC00124	/* Interrupt Wakeup Register                            */

/* Watchdog Timer			(0xFFC00200 - 0xFFC002FF)								*/
#define WDOG_CTL			0xFFC00200	/* Watchdog Control Register                            */
#define WDOG_CNT			0xFFC00204	/* Watchdog Count Register                                      */
#define WDOG_STAT			0xFFC00208	/* Watchdog Status Register                                     */

/* Real Time Clock		(0xFFC00300 - 0xFFC003FF)									*/
#define RTC_STAT			0xFFC00300	/* RTC Status Register                                          */
#define RTC_ICTL			0xFFC00304	/* RTC Interrupt Control Register                       */
#define RTC_ISTAT			0xFFC00308	/* RTC Interrupt Status Register                        */
#define RTC_SWCNT			0xFFC0030C	/* RTC Stopwatch Count Register                         */
#define RTC_ALARM			0xFFC00310	/* RTC Alarm Time Register                                      */
#define RTC_FAST			0xFFC00314	/* RTC Prescaler Enable Register                        */
#define RTC_PREN			0xFFC00314	/* RTC Prescaler Enable Alternate Macro         */

/* UART0 Controller		(0xFFC00400 - 0xFFC004FF)									*/
#define UART0_THR			0xFFC00400	/* Transmit Holding register                            */
#define UART0_RBR			0xFFC00400	/* Receive Buffer register                                      */
#define UART0_DLL			0xFFC00400	/* Divisor Latch (Low-Byte)                                     */
#define UART0_IER			0xFFC00404	/* Interrupt Enable Register                            */
#define UART0_DLH			0xFFC00404	/* Divisor Latch (High-Byte)                            */
#define UART0_IIR			0xFFC00408	/* Interrupt Identification Register            */
#define UART0_LCR			0xFFC0040C	/* Line Control Register                                        */
#define UART0_MCR			0xFFC00410	/* Modem Control Register                                       */
#define UART0_LSR			0xFFC00414	/* Line Status Register                                         */
#define UART0_MSR			0xFFC00418	/* Modem Status Register                                        */
#define UART0_SCR			0xFFC0041C	/* SCR Scratch Register                                         */
#define UART0_GCTL			0xFFC00424	/* Global Control Register                                      */

/* SPI Controller			(0xFFC00500 - 0xFFC005FF)								*/
#define SPI0_REGBASE			0xFFC00500
#define SPI_CTL				0xFFC00500	/* SPI Control Register                                         */
#define SPI_FLG				0xFFC00504	/* SPI Flag register                                            */
#define SPI_STAT			0xFFC00508	/* SPI Status register                                          */
#define SPI_TDBR			0xFFC0050C	/* SPI Transmit Data Buffer Register            */
#define SPI_RDBR			0xFFC00510	/* SPI Receive Data Buffer Register                     */
#define SPI_BAUD			0xFFC00514	/* SPI Baud rate Register                                       */
#define SPI_SHADOW			0xFFC00518	/* SPI_RDBR Shadow Register                                     */

/* TIMER0-7 Registers		(0xFFC00600 - 0xFFC006FF)								*/
#define TIMER0_CONFIG		0xFFC00600	/* Timer 0 Configuration Register                       */
#define TIMER0_COUNTER		0xFFC00604	/* Timer 0 Counter Register                                     */
#define TIMER0_PERIOD		0xFFC00608	/* Timer 0 Period Register                                      */
#define TIMER0_WIDTH		0xFFC0060C	/* Timer 0 Width Register                                       */

#define TIMER1_CONFIG		0xFFC00610	/* Timer 1 Configuration Register                       */
#define TIMER1_COUNTER		0xFFC00614	/* Timer 1 Counter Register                             */
#define TIMER1_PERIOD		0xFFC00618	/* Timer 1 Period Register                              */
#define TIMER1_WIDTH		0xFFC0061C	/* Timer 1 Width Register                               */

#define TIMER2_CONFIG		0xFFC00620	/* Timer 2 Configuration Register                       */
#define TIMER2_COUNTER		0xFFC00624	/* Timer 2 Counter Register                             */
#define TIMER2_PERIOD		0xFFC00628	/* Timer 2 Period Register                              */
#define TIMER2_WIDTH		0xFFC0062C	/* Timer 2 Width Register                               */

#define TIMER3_CONFIG		0xFFC00630	/* Timer 3 Configuration Register                       */
#define TIMER3_COUNTER		0xFFC00634	/* Timer 3 Counter Register                                     */
#define TIMER3_PERIOD		0xFFC00638	/* Timer 3 Period Register                                      */
#define TIMER3_WIDTH		0xFFC0063C	/* Timer 3 Width Register                                       */

#define TIMER4_CONFIG		0xFFC00640	/* Timer 4 Configuration Register                       */
#define TIMER4_COUNTER		0xFFC00644	/* Timer 4 Counter Register                             */
#define TIMER4_PERIOD		0xFFC00648	/* Timer 4 Period Register                              */
#define TIMER4_WIDTH		0xFFC0064C	/* Timer 4 Width Register                               */

#define TIMER5_CONFIG		0xFFC00650	/* Timer 5 Configuration Register                       */
#define TIMER5_COUNTER		0xFFC00654	/* Timer 5 Counter Register                             */
#define TIMER5_PERIOD		0xFFC00658	/* Timer 5 Period Register                              */
#define TIMER5_WIDTH		0xFFC0065C	/* Timer 5 Width Register                               */

#define TIMER6_CONFIG		0xFFC00660	/* Timer 6 Configuration Register                       */
#define TIMER6_COUNTER		0xFFC00664	/* Timer 6 Counter Register                             */
#define TIMER6_PERIOD		0xFFC00668	/* Timer 6 Period Register                              */
#define TIMER6_WIDTH		0xFFC0066C	/* Timer 6 Width Register                               */

#define TIMER7_CONFIG		0xFFC00670	/* Timer 7 Configuration Register                       */
#define TIMER7_COUNTER		0xFFC00674	/* Timer 7 Counter Register                             */
#define TIMER7_PERIOD		0xFFC00678	/* Timer 7 Period Register                              */
#define TIMER7_WIDTH		0xFFC0067C	/* Timer 7 Width Register                               */

#define TIMER_ENABLE		0xFFC00680	/* Timer Enable Register                                        */
#define TIMER_DISABLE		0xFFC00684	/* Timer Disable Register                                       */
#define TIMER_STATUS		0xFFC00688	/* Timer Status Register                                        */

/* General Purpose I/O Port F (0xFFC00700 - 0xFFC007FF)												*/
#define PORTFIO					0xFFC00700	/* Port F I/O Pin State Specify Register                                */
#define PORTFIO_CLEAR			0xFFC00704	/* Port F I/O Peripheral Interrupt Clear Register               */
#define PORTFIO_SET				0xFFC00708	/* Port F I/O Peripheral Interrupt Set Register                 */
#define PORTFIO_TOGGLE			0xFFC0070C	/* Port F I/O Pin State Toggle Register                                 */
#define PORTFIO_MASKA			0xFFC00710	/* Port F I/O Mask State Specify Interrupt A Register   */
#define PORTFIO_MASKA_CLEAR		0xFFC00714	/* Port F I/O Mask Disable Interrupt A Register                 */
#define PORTFIO_MASKA_SET		0xFFC00718	/* Port F I/O Mask Enable Interrupt A Register                  */
#define PORTFIO_MASKA_TOGGLE	0xFFC0071C	/* Port F I/O Mask Toggle Enable Interrupt A Register   */
#define PORTFIO_MASKB			0xFFC00720	/* Port F I/O Mask State Specify Interrupt B Register   */
#define PORTFIO_MASKB_CLEAR		0xFFC00724	/* Port F I/O Mask Disable Interrupt B Register                 */
#define PORTFIO_MASKB_SET		0xFFC00728	/* Port F I/O Mask Enable Interrupt B Register                  */
#define PORTFIO_MASKB_TOGGLE	0xFFC0072C	/* Port F I/O Mask Toggle Enable Interrupt B Register   */
#define PORTFIO_DIR				0xFFC00730	/* Port F I/O Direction Register                                                */
#define PORTFIO_POLAR			0xFFC00734	/* Port F I/O Source Polarity Register                                  */
#define PORTFIO_EDGE			0xFFC00738	/* Port F I/O Source Sensitivity Register                               */
#define PORTFIO_BOTH			0xFFC0073C	/* Port F I/O Set on BOTH Edges Register                                */
#define PORTFIO_INEN			0xFFC00740	/* Port F I/O Input Enable Register                                     */

/* SPORT0 Controller		(0xFFC00800 - 0xFFC008FF)										*/
#define SPORT0_TCR1			0xFFC00800	/* SPORT0 Transmit Configuration 1 Register                     */
#define SPORT0_TCR2			0xFFC00804	/* SPORT0 Transmit Configuration 2 Register                     */
#define SPORT0_TCLKDIV		0xFFC00808	/* SPORT0 Transmit Clock Divider                                        */
#define SPORT0_TFSDIV		0xFFC0080C	/* SPORT0 Transmit Frame Sync Divider                           */
#define SPORT0_TX			0xFFC00810	/* SPORT0 TX Data Register                                                      */
#define SPORT0_RX			0xFFC00818	/* SPORT0 RX Data Register                                                      */
#define SPORT0_RCR1			0xFFC00820	/* SPORT0 Transmit Configuration 1 Register                     */
#define SPORT0_RCR2			0xFFC00824	/* SPORT0 Transmit Configuration 2 Register                     */
#define SPORT0_RCLKDIV		0xFFC00828	/* SPORT0 Receive Clock Divider                                         */
#define SPORT0_RFSDIV		0xFFC0082C	/* SPORT0 Receive Frame Sync Divider                            */
#define SPORT0_STAT			0xFFC00830	/* SPORT0 Status Register                                                       */
#define SPORT0_CHNL			0xFFC00834	/* SPORT0 Current Channel Register                                      */
#define SPORT0_MCMC1		0xFFC00838	/* SPORT0 Multi-Channel Configuration Register 1        */
#define SPORT0_MCMC2		0xFFC0083C	/* SPORT0 Multi-Channel Configuration Register 2        */
#define SPORT0_MTCS0		0xFFC00840	/* SPORT0 Multi-Channel Transmit Select Register 0      */
#define SPORT0_MTCS1		0xFFC00844	/* SPORT0 Multi-Channel Transmit Select Register 1      */
#define SPORT0_MTCS2		0xFFC00848	/* SPORT0 Multi-Channel Transmit Select Register 2      */
#define SPORT0_MTCS3		0xFFC0084C	/* SPORT0 Multi-Channel Transmit Select Register 3      */
#define SPORT0_MRCS0		0xFFC00850	/* SPORT0 Multi-Channel Receive Select Register 0       */
#define SPORT0_MRCS1		0xFFC00854	/* SPORT0 Multi-Channel Receive Select Register 1       */
#define SPORT0_MRCS2		0xFFC00858	/* SPORT0 Multi-Channel Receive Select Register 2       */
#define SPORT0_MRCS3		0xFFC0085C	/* SPORT0 Multi-Channel Receive Select Register 3       */

/* SPORT1 Controller		(0xFFC00900 - 0xFFC009FF)										*/
#define SPORT1_TCR1			0xFFC00900	/* SPORT1 Transmit Configuration 1 Register                     */
#define SPORT1_TCR2			0xFFC00904	/* SPORT1 Transmit Configuration 2 Register                     */
#define SPORT1_TCLKDIV		0xFFC00908	/* SPORT1 Transmit Clock Divider                                        */
#define SPORT1_TFSDIV		0xFFC0090C	/* SPORT1 Transmit Frame Sync Divider                           */
#define SPORT1_TX			0xFFC00910	/* SPORT1 TX Data Register                                                      */
#define SPORT1_RX			0xFFC00918	/* SPORT1 RX Data Register                                                      */
#define SPORT1_RCR1			0xFFC00920	/* SPORT1 Transmit Configuration 1 Register                     */
#define SPORT1_RCR2			0xFFC00924	/* SPORT1 Transmit Configuration 2 Register                     */
#define SPORT1_RCLKDIV		0xFFC00928	/* SPORT1 Receive Clock Divider                                         */
#define SPORT1_RFSDIV		0xFFC0092C	/* SPORT1 Receive Frame Sync Divider                            */
#define SPORT1_STAT			0xFFC00930	/* SPORT1 Status Register                                                       */
#define SPORT1_CHNL			0xFFC00934	/* SPORT1 Current Channel Register                                      */
#define SPORT1_MCMC1		0xFFC00938	/* SPORT1 Multi-Channel Configuration Register 1        */
#define SPORT1_MCMC2		0xFFC0093C	/* SPORT1 Multi-Channel Configuration Register 2        */
#define SPORT1_MTCS0		0xFFC00940	/* SPORT1 Multi-Channel Transmit Select Register 0      */
#define SPORT1_MTCS1		0xFFC00944	/* SPORT1 Multi-Channel Transmit Select Register 1      */
#define SPORT1_MTCS2		0xFFC00948	/* SPORT1 Multi-Channel Transmit Select Register 2      */
#define SPORT1_MTCS3		0xFFC0094C	/* SPORT1 Multi-Channel Transmit Select Register 3      */
#define SPORT1_MRCS0		0xFFC00950	/* SPORT1 Multi-Channel Receive Select Register 0       */
#define SPORT1_MRCS1		0xFFC00954	/* SPORT1 Multi-Channel Receive Select Register 1       */
#define SPORT1_MRCS2		0xFFC00958	/* SPORT1 Multi-Channel Receive Select Register 2       */
#define SPORT1_MRCS3		0xFFC0095C	/* SPORT1 Multi-Channel Receive Select Register 3       */

/* External Bus Interface Unit (0xFFC00A00 - 0xFFC00AFF)								*/
#define EBIU_AMGCTL			0xFFC00A00	/* Asynchronous Memory Global Control Register  */
#define EBIU_AMBCTL0		0xFFC00A04	/* Asynchronous Memory Bank Control Register 0  */
#define EBIU_AMBCTL1		0xFFC00A08	/* Asynchronous Memory Bank Control Register 1  */
#define EBIU_SDGCTL			0xFFC00A10	/* SDRAM Global Control Register                                */
#define EBIU_SDBCTL			0xFFC00A14	/* SDRAM Bank Control Register                                  */
#define EBIU_SDRRC			0xFFC00A18	/* SDRAM Refresh Rate Control Register                  */
#define EBIU_SDSTAT			0xFFC00A1C	/* SDRAM Status Register                                                */

/* DMA Traffic Control Registers													*/
#define DMA_TC_PER			0xFFC00B0C	/* Traffic Control Periods Register			*/
#define DMA_TC_CNT			0xFFC00B10	/* Traffic Control Current Counts Register	*/

/* Alternate deprecated register names (below) provided for backwards code compatibility */
#define DMA_TCPER			0xFFC00B0C	/* Traffic Control Periods Register			*/
#define DMA_TCCNT			0xFFC00B10	/* Traffic Control Current Counts Register	*/

/* DMA Controller (0xFFC00C00 - 0xFFC00FFF)															*/
#define DMA0_NEXT_DESC_PTR		0xFFC00C00	/* DMA Channel 0 Next Descriptor Pointer Register               */
#define DMA0_START_ADDR			0xFFC00C04	/* DMA Channel 0 Start Address Register                                 */
#define DMA0_CONFIG				0xFFC00C08	/* DMA Channel 0 Configuration Register                                 */
#define DMA0_X_COUNT			0xFFC00C10	/* DMA Channel 0 X Count Register                                               */
#define DMA0_X_MODIFY			0xFFC00C14	/* DMA Channel 0 X Modify Register                                              */
#define DMA0_Y_COUNT			0xFFC00C18	/* DMA Channel 0 Y Count Register                                               */
#define DMA0_Y_MODIFY			0xFFC00C1C	/* DMA Channel 0 Y Modify Register                                              */
#define DMA0_CURR_DESC_PTR		0xFFC00C20	/* DMA Channel 0 Current Descriptor Pointer Register    */
#define DMA0_CURR_ADDR			0xFFC00C24	/* DMA Channel 0 Current Address Register                               */
#define DMA0_IRQ_STATUS			0xFFC00C28	/* DMA Channel 0 Interrupt/Status Register                              */
#define DMA0_PERIPHERAL_MAP		0xFFC00C2C	/* DMA Channel 0 Peripheral Map Register                                */
#define DMA0_CURR_X_COUNT		0xFFC00C30	/* DMA Channel 0 Current X Count Register                               */
#define DMA0_CURR_Y_COUNT		0xFFC00C38	/* DMA Channel 0 Current Y Count Register                               */

#define DMA1_NEXT_DESC_PTR		0xFFC00C40	/* DMA Channel 1 Next Descriptor Pointer Register               */
#define DMA1_START_ADDR			0xFFC00C44	/* DMA Channel 1 Start Address Register                                 */
#define DMA1_CONFIG				0xFFC00C48	/* DMA Channel 1 Configuration Register                                 */
#define DMA1_X_COUNT			0xFFC00C50	/* DMA Channel 1 X Count Register                                               */
#define DMA1_X_MODIFY			0xFFC00C54	/* DMA Channel 1 X Modify Register                                              */
#define DMA1_Y_COUNT			0xFFC00C58	/* DMA Channel 1 Y Count Register                                               */
#define DMA1_Y_MODIFY			0xFFC00C5C	/* DMA Channel 1 Y Modify Register                                              */
#define DMA1_CURR_DESC_PTR		0xFFC00C60	/* DMA Channel 1 Current Descriptor Pointer Register    */
#define DMA1_CURR_ADDR			0xFFC00C64	/* DMA Channel 1 Current Address Register                               */
#define DMA1_IRQ_STATUS			0xFFC00C68	/* DMA Channel 1 Interrupt/Status Register                              */
#define DMA1_PERIPHERAL_MAP		0xFFC00C6C	/* DMA Channel 1 Peripheral Map Register                                */
#define DMA1_CURR_X_COUNT		0xFFC00C70	/* DMA Channel 1 Current X Count Register                               */
#define DMA1_CURR_Y_COUNT		0xFFC00C78	/* DMA Channel 1 Current Y Count Register                               */

#define DMA2_NEXT_DESC_PTR		0xFFC00C80	/* DMA Channel 2 Next Descriptor Pointer Register               */
#define DMA2_START_ADDR			0xFFC00C84	/* DMA Channel 2 Start Address Register                                 */
#define DMA2_CONFIG				0xFFC00C88	/* DMA Channel 2 Configuration Register                                 */
#define DMA2_X_COUNT			0xFFC00C90	/* DMA Channel 2 X Count Register                                               */
#define DMA2_X_MODIFY			0xFFC00C94	/* DMA Channel 2 X Modify Register                                              */
#define DMA2_Y_COUNT			0xFFC00C98	/* DMA Channel 2 Y Count Register                                               */
#define DMA2_Y_MODIFY			0xFFC00C9C	/* DMA Channel 2 Y Modify Register                                              */
#define DMA2_CURR_DESC_PTR		0xFFC00CA0	/* DMA Channel 2 Current Descriptor Pointer Register    */
#define DMA2_CURR_ADDR			0xFFC00CA4	/* DMA Channel 2 Current Address Register                               */
#define DMA2_IRQ_STATUS			0xFFC00CA8	/* DMA Channel 2 Interrupt/Status Register                              */
#define DMA2_PERIPHERAL_MAP		0xFFC00CAC	/* DMA Channel 2 Peripheral Map Register                                */
#define DMA2_CURR_X_COUNT		0xFFC00CB0	/* DMA Channel 2 Current X Count Register                               */
#define DMA2_CURR_Y_COUNT		0xFFC00CB8	/* DMA Channel 2 Current Y Count Register                               */

#define DMA3_NEXT_DESC_PTR		0xFFC00CC0	/* DMA Channel 3 Next Descriptor Pointer Register               */
#define DMA3_START_ADDR			0xFFC00CC4	/* DMA Channel 3 Start Address Register                                 */
#define DMA3_CONFIG				0xFFC00CC8	/* DMA Channel 3 Configuration Register                                 */
#define DMA3_X_COUNT			0xFFC00CD0	/* DMA Channel 3 X Count Register                                               */
#define DMA3_X_MODIFY			0xFFC00CD4	/* DMA Channel 3 X Modify Register                                              */
#define DMA3_Y_COUNT			0xFFC00CD8	/* DMA Channel 3 Y Count Register                                               */
#define DMA3_Y_MODIFY			0xFFC00CDC	/* DMA Channel 3 Y Modify Register                                              */
#define DMA3_CURR_DESC_PTR		0xFFC00CE0	/* DMA Channel 3 Current Descriptor Pointer Register    */
#define DMA3_CURR_ADDR			0xFFC00CE4	/* DMA Channel 3 Current Address Register                               */
#define DMA3_IRQ_STATUS			0xFFC00CE8	/* DMA Channel 3 Interrupt/Status Register                              */
#define DMA3_PERIPHERAL_MAP		0xFFC00CEC	/* DMA Channel 3 Peripheral Map Register                                */
#define DMA3_CURR_X_COUNT		0xFFC00CF0	/* DMA Channel 3 Current X Count Register                               */
#define DMA3_CURR_Y_COUNT		0xFFC00CF8	/* DMA Channel 3 Current Y Count Register                               */

#define DMA4_NEXT_DESC_PTR		0xFFC00D00	/* DMA Channel 4 Next Descriptor Pointer Register               */
#define DMA4_START_ADDR			0xFFC00D04	/* DMA Channel 4 Start Address Register                                 */
#define DMA4_CONFIG				0xFFC00D08	/* DMA Channel 4 Configuration Register                                 */
#define DMA4_X_COUNT			0xFFC00D10	/* DMA Channel 4 X Count Register                                               */
#define DMA4_X_MODIFY			0xFFC00D14	/* DMA Channel 4 X Modify Register                                              */
#define DMA4_Y_COUNT			0xFFC00D18	/* DMA Channel 4 Y Count Register                                               */
#define DMA4_Y_MODIFY			0xFFC00D1C	/* DMA Channel 4 Y Modify Register                                              */
#define DMA4_CURR_DESC_PTR		0xFFC00D20	/* DMA Channel 4 Current Descriptor Pointer Register    */
#define DMA4_CURR_ADDR			0xFFC00D24	/* DMA Channel 4 Current Address Register                               */
#define DMA4_IRQ_STATUS			0xFFC00D28	/* DMA Channel 4 Interrupt/Status Register                              */
#define DMA4_PERIPHERAL_MAP		0xFFC00D2C	/* DMA Channel 4 Peripheral Map Register                                */
#define DMA4_CURR_X_COUNT		0xFFC00D30	/* DMA Channel 4 Current X Count Register                               */
#define DMA4_CURR_Y_COUNT		0xFFC00D38	/* DMA Channel 4 Current Y Count Register                               */

#define DMA5_NEXT_DESC_PTR		0xFFC00D40	/* DMA Channel 5 Next Descriptor Pointer Register               */
#define DMA5_START_ADDR			0xFFC00D44	/* DMA Channel 5 Start Address Register                                 */
#define DMA5_CONFIG				0xFFC00D48	/* DMA Channel 5 Configuration Register                                 */
#define DMA5_X_COUNT			0xFFC00D50	/* DMA Channel 5 X Count Register                                               */
#define DMA5_X_MODIFY			0xFFC00D54	/* DMA Channel 5 X Modify Register                                              */
#define DMA5_Y_COUNT			0xFFC00D58	/* DMA Channel 5 Y Count Register                                               */
#define DMA5_Y_MODIFY			0xFFC00D5C	/* DMA Channel 5 Y Modify Register                                              */
#define DMA5_CURR_DESC_PTR		0xFFC00D60	/* DMA Channel 5 Current Descriptor Pointer Register    */
#define DMA5_CURR_ADDR			0xFFC00D64	/* DMA Channel 5 Current Address Register                               */
#define DMA5_IRQ_STATUS			0xFFC00D68	/* DMA Channel 5 Interrupt/Status Register                              */
#define DMA5_PERIPHERAL_MAP		0xFFC00D6C	/* DMA Channel 5 Peripheral Map Register                                */
#define DMA5_CURR_X_COUNT		0xFFC00D70	/* DMA Channel 5 Current X Count Register                               */
#define DMA5_CURR_Y_COUNT		0xFFC00D78	/* DMA Channel 5 Current Y Count Register                               */

#define DMA6_NEXT_DESC_PTR		0xFFC00D80	/* DMA Channel 6 Next Descriptor Pointer Register               */
#define DMA6_START_ADDR			0xFFC00D84	/* DMA Channel 6 Start Address Register                                 */
#define DMA6_CONFIG				0xFFC00D88	/* DMA Channel 6 Configuration Register                                 */
#define DMA6_X_COUNT			0xFFC00D90	/* DMA Channel 6 X Count Register                                               */
#define DMA6_X_MODIFY			0xFFC00D94	/* DMA Channel 6 X Modify Register                                              */
#define DMA6_Y_COUNT			0xFFC00D98	/* DMA Channel 6 Y Count Register                                               */
#define DMA6_Y_MODIFY			0xFFC00D9C	/* DMA Channel 6 Y Modify Register                                              */
#define DMA6_CURR_DESC_PTR		0xFFC00DA0	/* DMA Channel 6 Current Descriptor Pointer Register    */
#define DMA6_CURR_ADDR			0xFFC00DA4	/* DMA Channel 6 Current Address Register                               */
#define DMA6_IRQ_STATUS			0xFFC00DA8	/* DMA Channel 6 Interrupt/Status Register                              */
#define DMA6_PERIPHERAL_MAP		0xFFC00DAC	/* DMA Channel 6 Peripheral Map Register                                */
#define DMA6_CURR_X_COUNT		0xFFC00DB0	/* DMA Channel 6 Current X Count Register                               */
#define DMA6_CURR_Y_COUNT		0xFFC00DB8	/* DMA Channel 6 Current Y Count Register                               */

#define DMA7_NEXT_DESC_PTR		0xFFC00DC0	/* DMA Channel 7 Next Descriptor Pointer Register               */
#define DMA7_START_ADDR			0xFFC00DC4	/* DMA Channel 7 Start Address Register                                 */
#define DMA7_CONFIG				0xFFC00DC8	/* DMA Channel 7 Configuration Register                                 */
#define DMA7_X_COUNT			0xFFC00DD0	/* DMA Channel 7 X Count Register                                               */
#define DMA7_X_MODIFY			0xFFC00DD4	/* DMA Channel 7 X Modify Register                                              */
#define DMA7_Y_COUNT			0xFFC00DD8	/* DMA Channel 7 Y Count Register                                               */
#define DMA7_Y_MODIFY			0xFFC00DDC	/* DMA Channel 7 Y Modify Register                                              */
#define DMA7_CURR_DESC_PTR		0xFFC00DE0	/* DMA Channel 7 Current Descriptor Pointer Register    */
#define DMA7_CURR_ADDR			0xFFC00DE4	/* DMA Channel 7 Current Address Register                               */
#define DMA7_IRQ_STATUS			0xFFC00DE8	/* DMA Channel 7 Interrupt/Status Register                              */
#define DMA7_PERIPHERAL_MAP		0xFFC00DEC	/* DMA Channel 7 Peripheral Map Register                                */
#define DMA7_CURR_X_COUNT		0xFFC00DF0	/* DMA Channel 7 Current X Count Register                               */
#define DMA7_CURR_Y_COUNT		0xFFC00DF8	/* DMA Channel 7 Current Y Count Register                               */

#define DMA8_NEXT_DESC_PTR		0xFFC00E00	/* DMA Channel 8 Next Descriptor Pointer Register               */
#define DMA8_START_ADDR			0xFFC00E04	/* DMA Channel 8 Start Address Register                                 */
#define DMA8_CONFIG				0xFFC00E08	/* DMA Channel 8 Configuration Register                                 */
#define DMA8_X_COUNT			0xFFC00E10	/* DMA Channel 8 X Count Register                                               */
#define DMA8_X_MODIFY			0xFFC00E14	/* DMA Channel 8 X Modify Register                                              */
#define DMA8_Y_COUNT			0xFFC00E18	/* DMA Channel 8 Y Count Register                                               */
#define DMA8_Y_MODIFY			0xFFC00E1C	/* DMA Channel 8 Y Modify Register                                              */
#define DMA8_CURR_DESC_PTR		0xFFC00E20	/* DMA Channel 8 Current Descriptor Pointer Register    */
#define DMA8_CURR_ADDR			0xFFC00E24	/* DMA Channel 8 Current Address Register                               */
#define DMA8_IRQ_STATUS			0xFFC00E28	/* DMA Channel 8 Interrupt/Status Register                              */
#define DMA8_PERIPHERAL_MAP		0xFFC00E2C	/* DMA Channel 8 Peripheral Map Register                                */
#define DMA8_CURR_X_COUNT		0xFFC00E30	/* DMA Channel 8 Current X Count Register                               */
#define DMA8_CURR_Y_COUNT		0xFFC00E38	/* DMA Channel 8 Current Y Count Register                               */

#define DMA9_NEXT_DESC_PTR		0xFFC00E40	/* DMA Channel 9 Next Descriptor Pointer Register               */
#define DMA9_START_ADDR			0xFFC00E44	/* DMA Channel 9 Start Address Register                                 */
#define DMA9_CONFIG				0xFFC00E48	/* DMA Channel 9 Configuration Register                                 */
#define DMA9_X_COUNT			0xFFC00E50	/* DMA Channel 9 X Count Register                                               */
#define DMA9_X_MODIFY			0xFFC00E54	/* DMA Channel 9 X Modify Register                                              */
#define DMA9_Y_COUNT			0xFFC00E58	/* DMA Channel 9 Y Count Register                                               */
#define DMA9_Y_MODIFY			0xFFC00E5C	/* DMA Channel 9 Y Modify Register                                              */
#define DMA9_CURR_DESC_PTR		0xFFC00E60	/* DMA Channel 9 Current Descriptor Pointer Register    */
#define DMA9_CURR_ADDR			0xFFC00E64	/* DMA Channel 9 Current Address Register                               */
#define DMA9_IRQ_STATUS			0xFFC00E68	/* DMA Channel 9 Interrupt/Status Register                              */
#define DMA9_PERIPHERAL_MAP		0xFFC00E6C	/* DMA Channel 9 Peripheral Map Register                                */
#define DMA9_CURR_X_COUNT		0xFFC00E70	/* DMA Channel 9 Current X Count Register                               */
#define DMA9_CURR_Y_COUNT		0xFFC00E78	/* DMA Channel 9 Current Y Count Register                               */

#define DMA10_NEXT_DESC_PTR		0xFFC00E80	/* DMA Channel 10 Next Descriptor Pointer Register              */
#define DMA10_START_ADDR		0xFFC00E84	/* DMA Channel 10 Start Address Register                                */
#define DMA10_CONFIG			0xFFC00E88	/* DMA Channel 10 Configuration Register                                */
#define DMA10_X_COUNT			0xFFC00E90	/* DMA Channel 10 X Count Register                                              */
#define DMA10_X_MODIFY			0xFFC00E94	/* DMA Channel 10 X Modify Register                                             */
#define DMA10_Y_COUNT			0xFFC00E98	/* DMA Channel 10 Y Count Register                                              */
#define DMA10_Y_MODIFY			0xFFC00E9C	/* DMA Channel 10 Y Modify Register                                             */
#define DMA10_CURR_DESC_PTR		0xFFC00EA0	/* DMA Channel 10 Current Descriptor Pointer Register   */
#define DMA10_CURR_ADDR			0xFFC00EA4	/* DMA Channel 10 Current Address Register                              */
#define DMA10_IRQ_STATUS		0xFFC00EA8	/* DMA Channel 10 Interrupt/Status Register                             */
#define DMA10_PERIPHERAL_MAP	0xFFC00EAC	/* DMA Channel 10 Peripheral Map Register                               */
#define DMA10_CURR_X_COUNT		0xFFC00EB0	/* DMA Channel 10 Current X Count Register                              */
#define DMA10_CURR_Y_COUNT		0xFFC00EB8	/* DMA Channel 10 Current Y Count Register                              */

#define DMA11_NEXT_DESC_PTR		0xFFC00EC0	/* DMA Channel 11 Next Descriptor Pointer Register              */
#define DMA11_START_ADDR		0xFFC00EC4	/* DMA Channel 11 Start Address Register                                */
#define DMA11_CONFIG			0xFFC00EC8	/* DMA Channel 11 Configuration Register                                */
#define DMA11_X_COUNT			0xFFC00ED0	/* DMA Channel 11 X Count Register                                              */
#define DMA11_X_MODIFY			0xFFC00ED4	/* DMA Channel 11 X Modify Register                                             */
#define DMA11_Y_COUNT			0xFFC00ED8	/* DMA Channel 11 Y Count Register                                              */
#define DMA11_Y_MODIFY			0xFFC00EDC	/* DMA Channel 11 Y Modify Register                                             */
#define DMA11_CURR_DESC_PTR		0xFFC00EE0	/* DMA Channel 11 Current Descriptor Pointer Register   */
#define DMA11_CURR_ADDR			0xFFC00EE4	/* DMA Channel 11 Current Address Register                              */
#define DMA11_IRQ_STATUS		0xFFC00EE8	/* DMA Channel 11 Interrupt/Status Register                             */
#define DMA11_PERIPHERAL_MAP	0xFFC00EEC	/* DMA Channel 11 Peripheral Map Register                               */
#define DMA11_CURR_X_COUNT		0xFFC00EF0	/* DMA Channel 11 Current X Count Register                              */
#define DMA11_CURR_Y_COUNT		0xFFC00EF8	/* DMA Channel 11 Current Y Count Register                              */

#define MDMA_D0_NEXT_DESC_PTR	0xFFC00F00	/* MemDMA Stream 0 Destination Next Descriptor Pointer Register         */
#define MDMA_D0_START_ADDR		0xFFC00F04	/* MemDMA Stream 0 Destination Start Address Register                           */
#define MDMA_D0_CONFIG			0xFFC00F08	/* MemDMA Stream 0 Destination Configuration Register                           */
#define MDMA_D0_X_COUNT			0xFFC00F10	/* MemDMA Stream 0 Destination X Count Register                                         */
#define MDMA_D0_X_MODIFY		0xFFC00F14	/* MemDMA Stream 0 Destination X Modify Register                                        */
#define MDMA_D0_Y_COUNT			0xFFC00F18	/* MemDMA Stream 0 Destination Y Count Register                                         */
#define MDMA_D0_Y_MODIFY		0xFFC00F1C	/* MemDMA Stream 0 Destination Y Modify Register                                        */
#define MDMA_D0_CURR_DESC_PTR	0xFFC00F20	/* MemDMA Stream 0 Destination Current Descriptor Pointer Register      */
#define MDMA_D0_CURR_ADDR		0xFFC00F24	/* MemDMA Stream 0 Destination Current Address Register                         */
#define MDMA_D0_IRQ_STATUS		0xFFC00F28	/* MemDMA Stream 0 Destination Interrupt/Status Register                        */
#define MDMA_D0_PERIPHERAL_MAP	0xFFC00F2C	/* MemDMA Stream 0 Destination Peripheral Map Register                          */
#define MDMA_D0_CURR_X_COUNT	0xFFC00F30	/* MemDMA Stream 0 Destination Current X Count Register                         */
#define MDMA_D0_CURR_Y_COUNT	0xFFC00F38	/* MemDMA Stream 0 Destination Current Y Count Register                         */

#define MDMA_S0_NEXT_DESC_PTR	0xFFC00F40	/* MemDMA Stream 0 Source Next Descriptor Pointer Register                      */
#define MDMA_S0_START_ADDR		0xFFC00F44	/* MemDMA Stream 0 Source Start Address Register                                        */
#define MDMA_S0_CONFIG			0xFFC00F48	/* MemDMA Stream 0 Source Configuration Register                                        */
#define MDMA_S0_X_COUNT			0xFFC00F50	/* MemDMA Stream 0 Source X Count Register                                                      */
#define MDMA_S0_X_MODIFY		0xFFC00F54	/* MemDMA Stream 0 Source X Modify Register                                                     */
#define MDMA_S0_Y_COUNT			0xFFC00F58	/* MemDMA Stream 0 Source Y Count Register                                                      */
#define MDMA_S0_Y_MODIFY		0xFFC00F5C	/* MemDMA Stream 0 Source Y Modify Register                                                     */
#define MDMA_S0_CURR_DESC_PTR	0xFFC00F60	/* MemDMA Stream 0 Source Current Descriptor Pointer Register           */
#define MDMA_S0_CURR_ADDR		0xFFC00F64	/* MemDMA Stream 0 Source Current Address Register                                      */
#define MDMA_S0_IRQ_STATUS		0xFFC00F68	/* MemDMA Stream 0 Source Interrupt/Status Register                                     */
#define MDMA_S0_PERIPHERAL_MAP	0xFFC00F6C	/* MemDMA Stream 0 Source Peripheral Map Register                                       */
#define MDMA_S0_CURR_X_COUNT	0xFFC00F70	/* MemDMA Stream 0 Source Current X Count Register                                      */
#define MDMA_S0_CURR_Y_COUNT	0xFFC00F78	/* MemDMA Stream 0 Source Current Y Count Register                                      */

#define MDMA_D1_NEXT_DESC_PTR	0xFFC00F80	/* MemDMA Stream 1 Destination Next Descriptor Pointer Register         */
#define MDMA_D1_START_ADDR		0xFFC00F84	/* MemDMA Stream 1 Destination Start Address Register                           */
#define MDMA_D1_CONFIG			0xFFC00F88	/* MemDMA Stream 1 Destination Configuration Register                           */
#define MDMA_D1_X_COUNT			0xFFC00F90	/* MemDMA Stream 1 Destination X Count Register                                         */
#define MDMA_D1_X_MODIFY		0xFFC00F94	/* MemDMA Stream 1 Destination X Modify Register                                        */
#define MDMA_D1_Y_COUNT			0xFFC00F98	/* MemDMA Stream 1 Destination Y Count Register                                         */
#define MDMA_D1_Y_MODIFY		0xFFC00F9C	/* MemDMA Stream 1 Destination Y Modify Register                                        */
#define MDMA_D1_CURR_DESC_PTR	0xFFC00FA0	/* MemDMA Stream 1 Destination Current Descriptor Pointer Register      */
#define MDMA_D1_CURR_ADDR		0xFFC00FA4	/* MemDMA Stream 1 Destination Current Address Register                         */
#define MDMA_D1_IRQ_STATUS		0xFFC00FA8	/* MemDMA Stream 1 Destination Interrupt/Status Register                        */
#define MDMA_D1_PERIPHERAL_MAP	0xFFC00FAC	/* MemDMA Stream 1 Destination Peripheral Map Register                          */
#define MDMA_D1_CURR_X_COUNT	0xFFC00FB0	/* MemDMA Stream 1 Destination Current X Count Register                         */
#define MDMA_D1_CURR_Y_COUNT	0xFFC00FB8	/* MemDMA Stream 1 Destination Current Y Count Register                         */

#define MDMA_S1_NEXT_DESC_PTR	0xFFC00FC0	/* MemDMA Stream 1 Source Next Descriptor Pointer Register                      */
#define MDMA_S1_START_ADDR		0xFFC00FC4	/* MemDMA Stream 1 Source Start Address Register                                        */
#define MDMA_S1_CONFIG			0xFFC00FC8	/* MemDMA Stream 1 Source Configuration Register                                        */
#define MDMA_S1_X_COUNT			0xFFC00FD0	/* MemDMA Stream 1 Source X Count Register                                                      */
#define MDMA_S1_X_MODIFY		0xFFC00FD4	/* MemDMA Stream 1 Source X Modify Register                                                     */
#define MDMA_S1_Y_COUNT			0xFFC00FD8	/* MemDMA Stream 1 Source Y Count Register                                                      */
#define MDMA_S1_Y_MODIFY		0xFFC00FDC	/* MemDMA Stream 1 Source Y Modify Register                                                     */
#define MDMA_S1_CURR_DESC_PTR	0xFFC00FE0	/* MemDMA Stream 1 Source Current Descriptor Pointer Register           */
#define MDMA_S1_CURR_ADDR		0xFFC00FE4	/* MemDMA Stream 1 Source Current Address Register                                      */
#define MDMA_S1_IRQ_STATUS		0xFFC00FE8	/* MemDMA Stream 1 Source Interrupt/Status Register                                     */
#define MDMA_S1_PERIPHERAL_MAP	0xFFC00FEC	/* MemDMA Stream 1 Source Peripheral Map Register                                       */
#define MDMA_S1_CURR_X_COUNT	0xFFC00FF0	/* MemDMA Stream 1 Source Current X Count Register                                      */
#define MDMA_S1_CURR_Y_COUNT	0xFFC00FF8	/* MemDMA Stream 1 Source Current Y Count Register                                      */

/* Parallel Peripheral Interface (0xFFC01000 - 0xFFC010FF)				*/
#define PPI_CONTROL			0xFFC01000	/* PPI Control Register                 */
#define PPI_STATUS			0xFFC01004	/* PPI Status Register                  */
#define PPI_COUNT			0xFFC01008	/* PPI Transfer Count Register  */
#define PPI_DELAY			0xFFC0100C	/* PPI Delay Count Register             */
#define PPI_FRAME			0xFFC01010	/* PPI Frame Length Register    */

/* Two-Wire Interface		(0xFFC01400 - 0xFFC014FF)								*/
#define TWI0_REGBASE			0xFFC01400
#define TWI_CLKDIV			0xFFC01400	/* Serial Clock Divider Register                        */
#define TWI_CONTROL			0xFFC01404	/* TWI Control Register                                         */
#define TWI_SLAVE_CTL		0xFFC01408	/* Slave Mode Control Register                          */
#define TWI_SLAVE_STAT		0xFFC0140C	/* Slave Mode Status Register                           */
#define TWI_SLAVE_ADDR		0xFFC01410	/* Slave Mode Address Register                          */
#define TWI_MASTER_CTL		0xFFC01414	/* Master Mode Control Register                         */
#define TWI_MASTER_STAT		0xFFC01418	/* Master Mode Status Register                          */
#define TWI_MASTER_ADDR		0xFFC0141C	/* Master Mode Address Register                         */
#define TWI_INT_STAT		0xFFC01420	/* TWI Interrupt Status Register                        */
#define TWI_INT_MASK		0xFFC01424	/* TWI Master Interrupt Mask Register           */
#define TWI_FIFO_CTL		0xFFC01428	/* FIFO Control Register                                        */
#define TWI_FIFO_STAT		0xFFC0142C	/* FIFO Status Register                                         */
#define TWI_XMT_DATA8		0xFFC01480	/* FIFO Transmit Data Single Byte Register      */
#define TWI_XMT_DATA16		0xFFC01484	/* FIFO Transmit Data Double Byte Register      */
#define TWI_RCV_DATA8		0xFFC01488	/* FIFO Receive Data Single Byte Register       */
#define TWI_RCV_DATA16		0xFFC0148C	/* FIFO Receive Data Double Byte Register       */

/* General Purpose I/O Port G (0xFFC01500 - 0xFFC015FF)												*/
#define PORTGIO					0xFFC01500	/* Port G I/O Pin State Specify Register                                */
#define PORTGIO_CLEAR			0xFFC01504	/* Port G I/O Peripheral Interrupt Clear Register               */
#define PORTGIO_SET				0xFFC01508	/* Port G I/O Peripheral Interrupt Set Register                 */
#define PORTGIO_TOGGLE			0xFFC0150C	/* Port G I/O Pin State Toggle Register                                 */
#define PORTGIO_MASKA			0xFFC01510	/* Port G I/O Mask State Specify Interrupt A Register   */
#define PORTGIO_MASKA_CLEAR		0xFFC01514	/* Port G I/O Mask Disable Interrupt A Register                 */
#define PORTGIO_MASKA_SET		0xFFC01518	/* Port G I/O Mask Enable Interrupt A Register                  */
#define PORTGIO_MASKA_TOGGLE	0xFFC0151C	/* Port G I/O Mask Toggle Enable Interrupt A Register   */
#define PORTGIO_MASKB			0xFFC01520	/* Port G I/O Mask State Specify Interrupt B Register   */
#define PORTGIO_MASKB_CLEAR		0xFFC01524	/* Port G I/O Mask Disable Interrupt B Register                 */
#define PORTGIO_MASKB_SET		0xFFC01528	/* Port G I/O Mask Enable Interrupt B Register                  */
#define PORTGIO_MASKB_TOGGLE	0xFFC0152C	/* Port G I/O Mask Toggle Enable Interrupt B Register   */
#define PORTGIO_DIR				0xFFC01530	/* Port G I/O Direction Register                                                */
#define PORTGIO_POLAR			0xFFC01534	/* Port G I/O Source Polarity Register                                  */
#define PORTGIO_EDGE			0xFFC01538	/* Port G I/O Source Sensitivity Register                               */
#define PORTGIO_BOTH			0xFFC0153C	/* Port G I/O Set on BOTH Edges Register                                */
#define PORTGIO_INEN			0xFFC01540	/* Port G I/O Input Enable Register                                             */

/* General Purpose I/O Port H (0xFFC01700 - 0xFFC017FF)												*/
#define PORTHIO					0xFFC01700	/* Port H I/O Pin State Specify Register                                */
#define PORTHIO_CLEAR			0xFFC01704	/* Port H I/O Peripheral Interrupt Clear Register               */
#define PORTHIO_SET				0xFFC01708	/* Port H I/O Peripheral Interrupt Set Register                 */
#define PORTHIO_TOGGLE			0xFFC0170C	/* Port H I/O Pin State Toggle Register                                 */
#define PORTHIO_MASKA			0xFFC01710	/* Port H I/O Mask State Specify Interrupt A Register   */
#define PORTHIO_MASKA_CLEAR		0xFFC01714	/* Port H I/O Mask Disable Interrupt A Register                 */
#define PORTHIO_MASKA_SET		0xFFC01718	/* Port H I/O Mask Enable Interrupt A Register                  */
#define PORTHIO_MASKA_TOGGLE	0xFFC0171C	/* Port H I/O Mask Toggle Enable Interrupt A Register   */
#define PORTHIO_MASKB			0xFFC01720	/* Port H I/O Mask State Specify Interrupt B Register   */
#define PORTHIO_MASKB_CLEAR		0xFFC01724	/* Port H I/O Mask Disable Interrupt B Register                 */
#define PORTHIO_MASKB_SET		0xFFC01728	/* Port H I/O Mask Enable Interrupt B Register                  */
#define PORTHIO_MASKB_TOGGLE	0xFFC0172C	/* Port H I/O Mask Toggle Enable Interrupt B Register   */
#define PORTHIO_DIR				0xFFC01730	/* Port H I/O Direction Register                                                */
#define PORTHIO_POLAR			0xFFC01734	/* Port H I/O Source Polarity Register                                  */
#define PORTHIO_EDGE			0xFFC01738	/* Port H I/O Source Sensitivity Register                               */
#define PORTHIO_BOTH			0xFFC0173C	/* Port H I/O Set on BOTH Edges Register                                */
#define PORTHIO_INEN			0xFFC01740	/* Port H I/O Input Enable Register                                             */

/* UART1 Controller		(0xFFC02000 - 0xFFC020FF)								*/
#define UART1_THR			0xFFC02000	/* Transmit Holding register                    */
#define UART1_RBR			0xFFC02000	/* Receive Buffer register                              */
#define UART1_DLL			0xFFC02000	/* Divisor Latch (Low-Byte)                             */
#define UART1_IER			0xFFC02004	/* Interrupt Enable Register                    */
#define UART1_DLH			0xFFC02004	/* Divisor Latch (High-Byte)                    */
#define UART1_IIR			0xFFC02008	/* Interrupt Identification Register    */
#define UART1_LCR			0xFFC0200C	/* Line Control Register                                */
#define UART1_MCR			0xFFC02010	/* Modem Control Register                               */
#define UART1_LSR			0xFFC02014	/* Line Status Register                                 */
#define UART1_MSR			0xFFC02018	/* Modem Status Register                                */
#define UART1_SCR			0xFFC0201C	/* SCR Scratch Register                                 */
#define UART1_GCTL			0xFFC02024	/* Global Control Register                              */

/* CAN Controller		(0xFFC02A00 - 0xFFC02FFF)										*/
/* For Mailboxes 0-15																	*/
#define CAN_MC1				0xFFC02A00	/* Mailbox config reg 1                                                 */
#define CAN_MD1				0xFFC02A04	/* Mailbox direction reg 1                                              */
#define CAN_TRS1			0xFFC02A08	/* Transmit Request Set reg 1                                   */
#define CAN_TRR1			0xFFC02A0C	/* Transmit Request Reset reg 1                                 */
#define CAN_TA1				0xFFC02A10	/* Transmit Acknowledge reg 1                                   */
#define CAN_AA1				0xFFC02A14	/* Transmit Abort Acknowledge reg 1                             */
#define CAN_RMP1			0xFFC02A18	/* Receive Message Pending reg 1                                */
#define CAN_RML1			0xFFC02A1C	/* Receive Message Lost reg 1                                   */
#define CAN_MBTIF1			0xFFC02A20	/* Mailbox Transmit Interrupt Flag reg 1                */
#define CAN_MBRIF1			0xFFC02A24	/* Mailbox Receive  Interrupt Flag reg 1                */
#define CAN_MBIM1			0xFFC02A28	/* Mailbox Interrupt Mask reg 1                                 */
#define CAN_RFH1			0xFFC02A2C	/* Remote Frame Handling reg 1                                  */
#define CAN_OPSS1			0xFFC02A30	/* Overwrite Protection Single Shot Xmit reg 1  */

/* For Mailboxes 16-31   																*/
#define CAN_MC2				0xFFC02A40	/* Mailbox config reg 2                                                 */
#define CAN_MD2				0xFFC02A44	/* Mailbox direction reg 2                                              */
#define CAN_TRS2			0xFFC02A48	/* Transmit Request Set reg 2                                   */
#define CAN_TRR2			0xFFC02A4C	/* Transmit Request Reset reg 2                                 */
#define CAN_TA2				0xFFC02A50	/* Transmit Acknowledge reg 2                                   */
#define CAN_AA2				0xFFC02A54	/* Transmit Abort Acknowledge reg 2                             */
#define CAN_RMP2			0xFFC02A58	/* Receive Message Pending reg 2                                */
#define CAN_RML2			0xFFC02A5C	/* Receive Message Lost reg 2                                   */
#define CAN_MBTIF2			0xFFC02A60	/* Mailbox Transmit Interrupt Flag reg 2                */
#define CAN_MBRIF2			0xFFC02A64	/* Mailbox Receive  Interrupt Flag reg 2                */
#define CAN_MBIM2			0xFFC02A68	/* Mailbox Interrupt Mask reg 2                                 */
#define CAN_RFH2			0xFFC02A6C	/* Remote Frame Handling reg 2                                  */
#define CAN_OPSS2			0xFFC02A70	/* Overwrite Protection Single Shot Xmit reg 2  */

/* CAN Configuration, Control, and Status Registers										*/
#define CAN_CLOCK			0xFFC02A80	/* Bit Timing Configuration register 0                  */
#define CAN_TIMING			0xFFC02A84	/* Bit Timing Configuration register 1                  */
#define CAN_DEBUG			0xFFC02A88	/* Debug Register                                                               */
#define CAN_STATUS			0xFFC02A8C	/* Global Status Register                                               */
#define CAN_CEC				0xFFC02A90	/* Error Counter Register                                               */
#define CAN_GIS				0xFFC02A94	/* Global Interrupt Status Register                             */
#define CAN_GIM				0xFFC02A98	/* Global Interrupt Mask Register                               */
#define CAN_GIF				0xFFC02A9C	/* Global Interrupt Flag Register                               */
#define CAN_CONTROL			0xFFC02AA0	/* Master Control Register                                              */
#define CAN_INTR			0xFFC02AA4	/* Interrupt Pending Register                                   */

#define CAN_MBTD			0xFFC02AAC	/* Mailbox Temporary Disable Feature                    */
#define CAN_EWR				0xFFC02AB0	/* Programmable Warning Level                                   */
#define CAN_ESR				0xFFC02AB4	/* Error Status Register                                                */
#define CAN_UCREG			0xFFC02AC0	/* Universal Counter Register/Capture Register  */
#define CAN_UCCNT			0xFFC02AC4	/* Universal Counter                                                    */
#define CAN_UCRC			0xFFC02AC8	/* Universal Counter Force Reload Register              */
#define CAN_UCCNF			0xFFC02ACC	/* Universal Counter Configuration Register             */

/* Mailbox Acceptance Masks 												*/
#define CAN_AM00L			0xFFC02B00	/* Mailbox 0 Low Acceptance Mask        */
#define CAN_AM00H			0xFFC02B04	/* Mailbox 0 High Acceptance Mask       */
#define CAN_AM01L			0xFFC02B08	/* Mailbox 1 Low Acceptance Mask        */
#define CAN_AM01H			0xFFC02B0C	/* Mailbox 1 High Acceptance Mask       */
#define CAN_AM02L			0xFFC02B10	/* Mailbox 2 Low Acceptance Mask        */
#define CAN_AM02H			0xFFC02B14	/* Mailbox 2 High Acceptance Mask       */
#define CAN_AM03L			0xFFC02B18	/* Mailbox 3 Low Acceptance Mask        */
#define CAN_AM03H			0xFFC02B1C	/* Mailbox 3 High Acceptance Mask       */
#define CAN_AM04L			0xFFC02B20	/* Mailbox 4 Low Acceptance Mask        */
#define CAN_AM04H			0xFFC02B24	/* Mailbox 4 High Acceptance Mask       */
#define CAN_AM05L			0xFFC02B28	/* Mailbox 5 Low Acceptance Mask        */
#define CAN_AM05H			0xFFC02B2C	/* Mailbox 5 High Acceptance Mask       */
#define CAN_AM06L			0xFFC02B30	/* Mailbox 6 Low Acceptance Mask        */
#define CAN_AM06H			0xFFC02B34	/* Mailbox 6 High Acceptance Mask       */
#define CAN_AM07L			0xFFC02B38	/* Mailbox 7 Low Acceptance Mask        */
#define CAN_AM07H			0xFFC02B3C	/* Mailbox 7 High Acceptance Mask       */
#define CAN_AM08L			0xFFC02B40	/* Mailbox 8 Low Acceptance Mask        */
#define CAN_AM08H			0xFFC02B44	/* Mailbox 8 High Acceptance Mask       */
#define CAN_AM09L			0xFFC02B48	/* Mailbox 9 Low Acceptance Mask        */
#define CAN_AM09H			0xFFC02B4C	/* Mailbox 9 High Acceptance Mask       */
#define CAN_AM10L			0xFFC02B50	/* Mailbox 10 Low Acceptance Mask       */
#define CAN_AM10H			0xFFC02B54	/* Mailbox 10 High Acceptance Mask      */
#define CAN_AM11L			0xFFC02B58	/* Mailbox 11 Low Acceptance Mask       */
#define CAN_AM11H			0xFFC02B5C	/* Mailbox 11 High Acceptance Mask      */
#define CAN_AM12L			0xFFC02B60	/* Mailbox 12 Low Acceptance Mask       */
#define CAN_AM12H			0xFFC02B64	/* Mailbox 12 High Acceptance Mask      */
#define CAN_AM13L			0xFFC02B68	/* Mailbox 13 Low Acceptance Mask       */
#define CAN_AM13H			0xFFC02B6C	/* Mailbox 13 High Acceptance Mask      */
#define CAN_AM14L			0xFFC02B70	/* Mailbox 14 Low Acceptance Mask       */
#define CAN_AM14H			0xFFC02B74	/* Mailbox 14 High Acceptance Mask      */
#define CAN_AM15L			0xFFC02B78	/* Mailbox 15 Low Acceptance Mask       */
#define CAN_AM15H			0xFFC02B7C	/* Mailbox 15 High Acceptance Mask      */

#define CAN_AM16L			0xFFC02B80	/* Mailbox 16 Low Acceptance Mask       */
#define CAN_AM16H			0xFFC02B84	/* Mailbox 16 High Acceptance Mask      */
#define CAN_AM17L			0xFFC02B88	/* Mailbox 17 Low Acceptance Mask       */
#define CAN_AM17H			0xFFC02B8C	/* Mailbox 17 High Acceptance Mask      */
#define CAN_AM18L			0xFFC02B90	/* Mailbox 18 Low Acceptance Mask       */
#define CAN_AM18H			0xFFC02B94	/* Mailbox 18 High Acceptance Mask      */
#define CAN_AM19L			0xFFC02B98	/* Mailbox 19 Low Acceptance Mask       */
#define CAN_AM19H			0xFFC02B9C	/* Mailbox 19 High Acceptance Mask      */
#define CAN_AM20L			0xFFC02BA0	/* Mailbox 20 Low Acceptance Mask       */
#define CAN_AM20H			0xFFC02BA4	/* Mailbox 20 High Acceptance Mask      */
#define CAN_AM21L			0xFFC02BA8	/* Mailbox 21 Low Acceptance Mask       */
#define CAN_AM21H			0xFFC02BAC	/* Mailbox 21 High Acceptance Mask      */
#define CAN_AM22L			0xFFC02BB0	/* Mailbox 22 Low Acceptance Mask       */
#define CAN_AM22H			0xFFC02BB4	/* Mailbox 22 High Acceptance Mask      */
#define CAN_AM23L			0xFFC02BB8	/* Mailbox 23 Low Acceptance Mask       */
#define CAN_AM23H			0xFFC02BBC	/* Mailbox 23 High Acceptance Mask      */
#define CAN_AM24L			0xFFC02BC0	/* Mailbox 24 Low Acceptance Mask       */
#define CAN_AM24H			0xFFC02BC4	/* Mailbox 24 High Acceptance Mask      */
#define CAN_AM25L			0xFFC02BC8	/* Mailbox 25 Low Acceptance Mask       */
#define CAN_AM25H			0xFFC02BCC	/* Mailbox 25 High Acceptance Mask      */
#define CAN_AM26L			0xFFC02BD0	/* Mailbox 26 Low Acceptance Mask       */
#define CAN_AM26H			0xFFC02BD4	/* Mailbox 26 High Acceptance Mask      */
#define CAN_AM27L			0xFFC02BD8	/* Mailbox 27 Low Acceptance Mask       */
#define CAN_AM27H			0xFFC02BDC	/* Mailbox 27 High Acceptance Mask      */
#define CAN_AM28L			0xFFC02BE0	/* Mailbox 28 Low Acceptance Mask       */
#define CAN_AM28H			0xFFC02BE4	/* Mailbox 28 High Acceptance Mask      */
#define CAN_AM29L			0xFFC02BE8	/* Mailbox 29 Low Acceptance Mask       */
#define CAN_AM29H			0xFFC02BEC	/* Mailbox 29 High Acceptance Mask      */
#define CAN_AM30L			0xFFC02BF0	/* Mailbox 30 Low Acceptance Mask       */
#define CAN_AM30H			0xFFC02BF4	/* Mailbox 30 High Acceptance Mask      */
#define CAN_AM31L			0xFFC02BF8	/* Mailbox 31 Low Acceptance Mask       */
#define CAN_AM31H			0xFFC02BFC	/* Mailbox 31 High Acceptance Mask      */

/* CAN Acceptance Mask Macros				*/
#define CAN_AM_L(x)		(CAN_AM00L+((x)*0x8))
#define CAN_AM_H(x)		(CAN_AM00H+((x)*0x8))

/* Mailbox Registers																*/
#define CAN_MB00_DATA0		0xFFC02C00	/* Mailbox 0 Data Word 0 [15:0] Register        */
#define CAN_MB00_DATA1		0xFFC02C04	/* Mailbox 0 Data Word 1 [31:16] Register       */
#define CAN_MB00_DATA2		0xFFC02C08	/* Mailbox 0 Data Word 2 [47:32] Register       */
#define CAN_MB00_DATA3		0xFFC02C0C	/* Mailbox 0 Data Word 3 [63:48] Register       */
#define CAN_MB00_LENGTH		0xFFC02C10	/* Mailbox 0 Data Length Code Register          */
#define CAN_MB00_TIMESTAMP	0xFFC02C14	/* Mailbox 0 Time Stamp Value Register          */
#define CAN_MB00_ID0		0xFFC02C18	/* Mailbox 0 Identifier Low Register            */
#define CAN_MB00_ID1		0xFFC02C1C	/* Mailbox 0 Identifier High Register           */

#define CAN_MB01_DATA0		0xFFC02C20	/* Mailbox 1 Data Word 0 [15:0] Register        */
#define CAN_MB01_DATA1		0xFFC02C24	/* Mailbox 1 Data Word 1 [31:16] Register       */
#define CAN_MB01_DATA2		0xFFC02C28	/* Mailbox 1 Data Word 2 [47:32] Register       */
#define CAN_MB01_DATA3		0xFFC02C2C	/* Mailbox 1 Data Word 3 [63:48] Register       */
#define CAN_MB01_LENGTH		0xFFC02C30	/* Mailbox 1 Data Length Code Register          */
#define CAN_MB01_TIMESTAMP	0xFFC02C34	/* Mailbox 1 Time Stamp Value Register          */
#define CAN_MB01_ID0		0xFFC02C38	/* Mailbox 1 Identifier Low Register            */
#define CAN_MB01_ID1		0xFFC02C3C	/* Mailbox 1 Identifier High Register           */

#define CAN_MB02_DATA0		0xFFC02C40	/* Mailbox 2 Data Word 0 [15:0] Register        */
#define CAN_MB02_DATA1		0xFFC02C44	/* Mailbox 2 Data Word 1 [31:16] Register       */
#define CAN_MB02_DATA2		0xFFC02C48	/* Mailbox 2 Data Word 2 [47:32] Register       */
#define CAN_MB02_DATA3		0xFFC02C4C	/* Mailbox 2 Data Word 3 [63:48] Register       */
#define CAN_MB02_LENGTH		0xFFC02C50	/* Mailbox 2 Data Length Code Register          */
#define CAN_MB02_TIMESTAMP	0xFFC02C54	/* Mailbox 2 Time Stamp Value Register          */
#define CAN_MB02_ID0		0xFFC02C58	/* Mailbox 2 Identifier Low Register            */
#define CAN_MB02_ID1		0xFFC02C5C	/* Mailbox 2 Identifier High Register           */

#define CAN_MB03_DATA0		0xFFC02C60	/* Mailbox 3 Data Word 0 [15:0] Register        */
#define CAN_MB03_DATA1		0xFFC02C64	/* Mailbox 3 Data Word 1 [31:16] Register       */
#define CAN_MB03_DATA2		0xFFC02C68	/* Mailbox 3 Data Word 2 [47:32] Register       */
#define CAN_MB03_DATA3		0xFFC02C6C	/* Mailbox 3 Data Word 3 [63:48] Register       */
#define CAN_MB03_LENGTH		0xFFC02C70	/* Mailbox 3 Data Length Code Register          */
#define CAN_MB03_TIMESTAMP	0xFFC02C74	/* Mailbox 3 Time Stamp Value Register          */
#define CAN_MB03_ID0		0xFFC02C78	/* Mailbox 3 Identifier Low Register            */
#define CAN_MB03_ID1		0xFFC02C7C	/* Mailbox 3 Identifier High Register           */

#define CAN_MB04_DATA0		0xFFC02C80	/* Mailbox 4 Data Word 0 [15:0] Register        */
#define CAN_MB04_DATA1		0xFFC02C84	/* Mailbox 4 Data Word 1 [31:16] Register       */
#define CAN_MB04_DATA2		0xFFC02C88	/* Mailbox 4 Data Word 2 [47:32] Register       */
#define CAN_MB04_DATA3		0xFFC02C8C	/* Mailbox 4 Data Word 3 [63:48] Register       */
#define CAN_MB04_LENGTH		0xFFC02C90	/* Mailbox 4 Data Length Code Register          */
#define CAN_MB04_TIMESTAMP	0xFFC02C94	/* Mailbox 4 Time Stamp Value Register          */
#define CAN_MB04_ID0		0xFFC02C98	/* Mailbox 4 Identifier Low Register            */
#define CAN_MB04_ID1		0xFFC02C9C	/* Mailbox 4 Identifier High Register           */

#define CAN_MB05_DATA0		0xFFC02CA0	/* Mailbox 5 Data Word 0 [15:0] Register        */
#define CAN_MB05_DATA1		0xFFC02CA4	/* Mailbox 5 Data Word 1 [31:16] Register       */
#define CAN_MB05_DATA2		0xFFC02CA8	/* Mailbox 5 Data Word 2 [47:32] Register       */
#define CAN_MB05_DATA3		0xFFC02CAC	/* Mailbox 5 Data Word 3 [63:48] Register       */
#define CAN_MB05_LENGTH		0xFFC02CB0	/* Mailbox 5 Data Length Code Register          */
#define CAN_MB05_TIMESTAMP	0xFFC02CB4	/* Mailbox 5 Time Stamp Value Register          */
#define CAN_MB05_ID0		0xFFC02CB8	/* Mailbox 5 Identifier Low Register            */
#define CAN_MB05_ID1		0xFFC02CBC	/* Mailbox 5 Identifier High Register           */

#define CAN_MB06_DATA0		0xFFC02CC0	/* Mailbox 6 Data Word 0 [15:0] Register        */
#define CAN_MB06_DATA1		0xFFC02CC4	/* Mailbox 6 Data Word 1 [31:16] Register       */
#define CAN_MB06_DATA2		0xFFC02CC8	/* Mailbox 6 Data Word 2 [47:32] Register       */
#define CAN_MB06_DATA3		0xFFC02CCC	/* Mailbox 6 Data Word 3 [63:48] Register       */
#define CAN_MB06_LENGTH		0xFFC02CD0	/* Mailbox 6 Data Length Code Register          */
#define CAN_MB06_TIMESTAMP	0xFFC02CD4	/* Mailbox 6 Time Stamp Value Register          */
#define CAN_MB06_ID0		0xFFC02CD8	/* Mailbox 6 Identifier Low Register            */
#define CAN_MB06_ID1		0xFFC02CDC	/* Mailbox 6 Identifier High Register           */

#define CAN_MB07_DATA0		0xFFC02CE0	/* Mailbox 7 Data Word 0 [15:0] Register        */
#define CAN_MB07_DATA1		0xFFC02CE4	/* Mailbox 7 Data Word 1 [31:16] Register       */
#define CAN_MB07_DATA2		0xFFC02CE8	/* Mailbox 7 Data Word 2 [47:32] Register       */
#define CAN_MB07_DATA3		0xFFC02CEC	/* Mailbox 7 Data Word 3 [63:48] Register       */
#define CAN_MB07_LENGTH		0xFFC02CF0	/* Mailbox 7 Data Length Code Register          */
#define CAN_MB07_TIMESTAMP	0xFFC02CF4	/* Mailbox 7 Time Stamp Value Register          */
#define CAN_MB07_ID0		0xFFC02CF8	/* Mailbox 7 Identifier Low Register            */
#define CAN_MB07_ID1		0xFFC02CFC	/* Mailbox 7 Identifier High Register           */

#define CAN_MB08_DATA0		0xFFC02D00	/* Mailbox 8 Data Word 0 [15:0] Register        */
#define CAN_MB08_DATA1		0xFFC02D04	/* Mailbox 8 Data Word 1 [31:16] Register       */
#define CAN_MB08_DATA2		0xFFC02D08	/* Mailbox 8 Data Word 2 [47:32] Register       */
#define CAN_MB08_DATA3		0xFFC02D0C	/* Mailbox 8 Data Word 3 [63:48] Register       */
#define CAN_MB08_LENGTH		0xFFC02D10	/* Mailbox 8 Data Length Code Register          */
#define CAN_MB08_TIMESTAMP	0xFFC02D14	/* Mailbox 8 Time Stamp Value Register          */
#define CAN_MB08_ID0		0xFFC02D18	/* Mailbox 8 Identifier Low Register            */
#define CAN_MB08_ID1		0xFFC02D1C	/* Mailbox 8 Identifier High Register           */

#define CAN_MB09_DATA0		0xFFC02D20	/* Mailbox 9 Data Word 0 [15:0] Register        */
#define CAN_MB09_DATA1		0xFFC02D24	/* Mailbox 9 Data Word 1 [31:16] Register       */
#define CAN_MB09_DATA2		0xFFC02D28	/* Mailbox 9 Data Word 2 [47:32] Register       */
#define CAN_MB09_DATA3		0xFFC02D2C	/* Mailbox 9 Data Word 3 [63:48] Register       */
#define CAN_MB09_LENGTH		0xFFC02D30	/* Mailbox 9 Data Length Code Register          */
#define CAN_MB09_TIMESTAMP	0xFFC02D34	/* Mailbox 9 Time Stamp Value Register          */
#define CAN_MB09_ID0		0xFFC02D38	/* Mailbox 9 Identifier Low Register            */
#define CAN_MB09_ID1		0xFFC02D3C	/* Mailbox 9 Identifier High Register           */

#define CAN_MB10_DATA0		0xFFC02D40	/* Mailbox 10 Data Word 0 [15:0] Register       */
#define CAN_MB10_DATA1		0xFFC02D44	/* Mailbox 10 Data Word 1 [31:16] Register      */
#define CAN_MB10_DATA2		0xFFC02D48	/* Mailbox 10 Data Word 2 [47:32] Register      */
#define CAN_MB10_DATA3		0xFFC02D4C	/* Mailbox 10 Data Word 3 [63:48] Register      */
#define CAN_MB10_LENGTH		0xFFC02D50	/* Mailbox 10 Data Length Code Register         */
#define CAN_MB10_TIMESTAMP	0xFFC02D54	/* Mailbox 10 Time Stamp Value Register         */
#define CAN_MB10_ID0		0xFFC02D58	/* Mailbox 10 Identifier Low Register           */
#define CAN_MB10_ID1		0xFFC02D5C	/* Mailbox 10 Identifier High Register          */

#define CAN_MB11_DATA0		0xFFC02D60	/* Mailbox 11 Data Word 0 [15:0] Register       */
#define CAN_MB11_DATA1		0xFFC02D64	/* Mailbox 11 Data Word 1 [31:16] Register      */
#define CAN_MB11_DATA2		0xFFC02D68	/* Mailbox 11 Data Word 2 [47:32] Register      */
#define CAN_MB11_DATA3		0xFFC02D6C	/* Mailbox 11 Data Word 3 [63:48] Register      */
#define CAN_MB11_LENGTH		0xFFC02D70	/* Mailbox 11 Data Length Code Register         */
#define CAN_MB11_TIMESTAMP	0xFFC02D74	/* Mailbox 11 Time Stamp Value Register         */
#define CAN_MB11_ID0		0xFFC02D78	/* Mailbox 11 Identifier Low Register           */
#define CAN_MB11_ID1		0xFFC02D7C	/* Mailbox 11 Identifier High Register          */

#define CAN_MB12_DATA0		0xFFC02D80	/* Mailbox 12 Data Word 0 [15:0] Register       */
#define CAN_MB12_DATA1		0xFFC02D84	/* Mailbox 12 Data Word 1 [31:16] Register      */
#define CAN_MB12_DATA2		0xFFC02D88	/* Mailbox 12 Data Word 2 [47:32] Register      */
#define CAN_MB12_DATA3		0xFFC02D8C	/* Mailbox 12 Data Word 3 [63:48] Register      */
#define CAN_MB12_LENGTH		0xFFC02D90	/* Mailbox 12 Data Length Code Register         */
#define CAN_MB12_TIMESTAMP	0xFFC02D94	/* Mailbox 12 Time Stamp Value Register         */
#define CAN_MB12_ID0		0xFFC02D98	/* Mailbox 12 Identifier Low Register           */
#define CAN_MB12_ID1		0xFFC02D9C	/* Mailbox 12 Identifier High Register          */

#define CAN_MB13_DATA0		0xFFC02DA0	/* Mailbox 13 Data Word 0 [15:0] Register       */
#define CAN_MB13_DATA1		0xFFC02DA4	/* Mailbox 13 Data Word 1 [31:16] Register      */
#define CAN_MB13_DATA2		0xFFC02DA8	/* Mailbox 13 Data Word 2 [47:32] Register      */
#define CAN_MB13_DATA3		0xFFC02DAC	/* Mailbox 13 Data Word 3 [63:48] Register      */
#define CAN_MB13_LENGTH		0xFFC02DB0	/* Mailbox 13 Data Length Code Register         */
#define CAN_MB13_TIMESTAMP	0xFFC02DB4	/* Mailbox 13 Time Stamp Value Register         */
#define CAN_MB13_ID0		0xFFC02DB8	/* Mailbox 13 Identifier Low Register           */
#define CAN_MB13_ID1		0xFFC02DBC	/* Mailbox 13 Identifier High Register          */

#define CAN_MB14_DATA0		0xFFC02DC0	/* Mailbox 14 Data Word 0 [15:0] Register       */
#define CAN_MB14_DATA1		0xFFC02DC4	/* Mailbox 14 Data Word 1 [31:16] Register      */
#define CAN_MB14_DATA2		0xFFC02DC8	/* Mailbox 14 Data Word 2 [47:32] Register      */
#define CAN_MB14_DATA3		0xFFC02DCC	/* Mailbox 14 Data Word 3 [63:48] Register      */
#define CAN_MB14_LENGTH		0xFFC02DD0	/* Mailbox 14 Data Length Code Register         */
#define CAN_MB14_TIMESTAMP	0xFFC02DD4	/* Mailbox 14 Time Stamp Value Register         */
#define CAN_MB14_ID0		0xFFC02DD8	/* Mailbox 14 Identifier Low Register           */
#define CAN_MB14_ID1		0xFFC02DDC	/* Mailbox 14 Identifier High Register          */

#define CAN_MB15_DATA0		0xFFC02DE0	/* Mailbox 15 Data Word 0 [15:0] Register       */
#define CAN_MB15_DATA1		0xFFC02DE4	/* Mailbox 15 Data Word 1 [31:16] Register      */
#define CAN_MB15_DATA2		0xFFC02DE8	/* Mailbox 15 Data Word 2 [47:32] Register      */
#define CAN_MB15_DATA3		0xFFC02DEC	/* Mailbox 15 Data Word 3 [63:48] Register      */
#define CAN_MB15_LENGTH		0xFFC02DF0	/* Mailbox 15 Data Length Code Register         */
#define CAN_MB15_TIMESTAMP	0xFFC02DF4	/* Mailbox 15 Time Stamp Value Register         */
#define CAN_MB15_ID0		0xFFC02DF8	/* Mailbox 15 Identifier Low Register           */
#define CAN_MB15_ID1		0xFFC02DFC	/* Mailbox 15 Identifier High Register          */

#define CAN_MB16_DATA0		0xFFC02E00	/* Mailbox 16 Data Word 0 [15:0] Register       */
#define CAN_MB16_DATA1		0xFFC02E04	/* Mailbox 16 Data Word 1 [31:16] Register      */
#define CAN_MB16_DATA2		0xFFC02E08	/* Mailbox 16 Data Word 2 [47:32] Register      */
#define CAN_MB16_DATA3		0xFFC02E0C	/* Mailbox 16 Data Word 3 [63:48] Register      */
#define CAN_MB16_LENGTH		0xFFC02E10	/* Mailbox 16 Data Length Code Register         */
#define CAN_MB16_TIMESTAMP	0xFFC02E14	/* Mailbox 16 Time Stamp Value Register         */
#define CAN_MB16_ID0		0xFFC02E18	/* Mailbox 16 Identifier Low Register           */
#define CAN_MB16_ID1		0xFFC02E1C	/* Mailbox 16 Identifier High Register          */

#define CAN_MB17_DATA0		0xFFC02E20	/* Mailbox 17 Data Word 0 [15:0] Register       */
#define CAN_MB17_DATA1		0xFFC02E24	/* Mailbox 17 Data Word 1 [31:16] Register      */
#define CAN_MB17_DATA2		0xFFC02E28	/* Mailbox 17 Data Word 2 [47:32] Register      */
#define CAN_MB17_DATA3		0xFFC02E2C	/* Mailbox 17 Data Word 3 [63:48] Register      */
#define CAN_MB17_LENGTH		0xFFC02E30	/* Mailbox 17 Data Length Code Register         */
#define CAN_MB17_TIMESTAMP	0xFFC02E34	/* Mailbox 17 Time Stamp Value Register         */
#define CAN_MB17_ID0		0xFFC02E38	/* Mailbox 17 Identifier Low Register           */
#define CAN_MB17_ID1		0xFFC02E3C	/* Mailbox 17 Identifier High Register          */

#define CAN_MB18_DATA0		0xFFC02E40	/* Mailbox 18 Data Word 0 [15:0] Register       */
#define CAN_MB18_DATA1		0xFFC02E44	/* Mailbox 18 Data Word 1 [31:16] Register      */
#define CAN_MB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word 2 [47:32] Register      */
#define CAN_MB18_DATA3		0xFFC02E4C	/* Mailbox 18 Data Word 3 [63:48] Register      */
#define CAN_MB18_LENGTH		0xFFC02E50	/* Mailbox 18 Data Length Code Register         */
#define CAN_MB18_TIMESTAMP	0xFFC02E54	/* Mailbox 18 Time Stamp Value Register         */
#define CAN_MB18_ID0		0xFFC02E58	/* Mailbox 18 Identifier Low Register           */
#define CAN_MB18_ID1		0xFFC02E5C	/* Mailbox 18 Identifier High Register          */

#define CAN_MB19_DATA0		0xFFC02E60	/* Mailbox 19 Data Word 0 [15:0] Register       */
#define CAN_MB19_DATA1		0xFFC02E64	/* Mailbox 19 Data Word 1 [31:16] Register      */
#define CAN_MB19_DATA2		0xFFC02E68	/* Mailbox 19 Data Word 2 [47:32] Register      */
#define CAN_MB19_DATA3		0xFFC02E6C	/* Mailbox 19 Data Word 3 [63:48] Register      */
#define CAN_MB19_LENGTH		0xFFC02E70	/* Mailbox 19 Data Length Code Register         */
#define CAN_MB19_TIMESTAMP	0xFFC02E74	/* Mailbox 19 Time Stamp Value Register         */
#define CAN_MB19_ID0		0xFFC02E78	/* Mailbox 19 Identifier Low Register           */
#define CAN_MB19_ID1		0xFFC02E7C	/* Mailbox 19 Identifier High Register          */

#define CAN_MB20_DATA0		0xFFC02E80	/* Mailbox 20 Data Word 0 [15:0] Register       */
#define CAN_MB20_DATA1		0xFFC02E84	/* Mailbox 20 Data Word 1 [31:16] Register      */
#define CAN_MB20_DATA2		0xFFC02E88	/* Mailbox 20 Data Word 2 [47:32] Register      */
#define CAN_MB20_DATA3		0xFFC02E8C	/* Mailbox 20 Data Word 3 [63:48] Register      */
#define CAN_MB20_LENGTH		0xFFC02E90	/* Mailbox 20 Data Length Code Register         */
#define CAN_MB20_TIMESTAMP	0xFFC02E94	/* Mailbox 20 Time Stamp Value Register         */
#define CAN_MB20_ID0		0xFFC02E98	/* Mailbox 20 Identifier Low Register           */
#define CAN_MB20_ID1		0xFFC02E9C	/* Mailbox 20 Identifier High Register          */

#define CAN_MB21_DATA0		0xFFC02EA0	/* Mailbox 21 Data Word 0 [15:0] Register       */
#define CAN_MB21_DATA1		0xFFC02EA4	/* Mailbox 21 Data Word 1 [31:16] Register      */
#define CAN_MB21_DATA2		0xFFC02EA8	/* Mailbox 21 Data Word 2 [47:32] Register      */
#define CAN_MB21_DATA3		0xFFC02EAC	/* Mailbox 21 Data Word 3 [63:48] Register      */
#define CAN_MB21_LENGTH		0xFFC02EB0	/* Mailbox 21 Data Length Code Register         */
#define CAN_MB21_TIMESTAMP	0xFFC02EB4	/* Mailbox 21 Time Stamp Value Register         */
#define CAN_MB21_ID0		0xFFC02EB8	/* Mailbox 21 Identifier Low Register           */
#define CAN_MB21_ID1		0xFFC02EBC	/* Mailbox 21 Identifier High Register          */

#define CAN_MB22_DATA0		0xFFC02EC0	/* Mailbox 22 Data Word 0 [15:0] Register       */
#define CAN_MB22_DATA1		0xFFC02EC4	/* Mailbox 22 Data Word 1 [31:16] Register      */
#define CAN_MB22_DATA2		0xFFC02EC8	/* Mailbox 22 Data Word 2 [47:32] Register      */
#define CAN_MB22_DATA3		0xFFC02ECC	/* Mailbox 22 Data Word 3 [63:48] Register      */
#define CAN_MB22_LENGTH		0xFFC02ED0	/* Mailbox 22 Data Length Code Register         */
#define CAN_MB22_TIMESTAMP	0xFFC02ED4	/* Mailbox 22 Time Stamp Value Register         */
#define CAN_MB22_ID0		0xFFC02ED8	/* Mailbox 22 Identifier Low Register           */
#define CAN_MB22_ID1		0xFFC02EDC	/* Mailbox 22 Identifier High Register          */

#define CAN_MB23_DATA0		0xFFC02EE0	/* Mailbox 23 Data Word 0 [15:0] Register       */
#define CAN_MB23_DATA1		0xFFC02EE4	/* Mailbox 23 Data Word 1 [31:16] Register      */
#define CAN_MB23_DATA2		0xFFC02EE8	/* Mailbox 23 Data Word 2 [47:32] Register      */
#define CAN_MB23_DATA3		0xFFC02EEC	/* Mailbox 23 Data Word 3 [63:48] Register      */
#define CAN_MB23_LENGTH		0xFFC02EF0	/* Mailbox 23 Data Length Code Register         */
#define CAN_MB23_TIMESTAMP	0xFFC02EF4	/* Mailbox 23 Time Stamp Value Register         */
#define CAN_MB23_ID0		0xFFC02EF8	/* Mailbox 23 Identifier Low Register           */
#define CAN_MB23_ID1		0xFFC02EFC	/* Mailbox 23 Identifier High Register          */

#define CAN_MB24_DATA0		0xFFC02F00	/* Mailbox 24 Data Word 0 [15:0] Register       */
#define CAN_MB24_DATA1		0xFFC02F04	/* Mailbox 24 Data Word 1 [31:16] Register      */
#define CAN_MB24_DATA2		0xFFC02F08	/* Mailbox 24 Data Word 2 [47:32] Register      */
#define CAN_MB24_DATA3		0xFFC02F0C	/* Mailbox 24 Data Word 3 [63:48] Register      */
#define CAN_MB24_LENGTH		0xFFC02F10	/* Mailbox 24 Data Length Code Register         */
#define CAN_MB24_TIMESTAMP	0xFFC02F14	/* Mailbox 24 Time Stamp Value Register         */
#define CAN_MB24_ID0		0xFFC02F18	/* Mailbox 24 Identifier Low Register           */
#define CAN_MB24_ID1		0xFFC02F1C	/* Mailbox 24 Identifier High Register          */

#define CAN_MB25_DATA0		0xFFC02F20	/* Mailbox 25 Data Word 0 [15:0] Register       */
#define CAN_MB25_DATA1		0xFFC02F24	/* Mailbox 25 Data Word 1 [31:16] Register      */
#define CAN_MB25_DATA2		0xFFC02F28	/* Mailbox 25 Data Word 2 [47:32] Register      */
#define CAN_MB25_DATA3		0xFFC02F2C	/* Mailbox 25 Data Word 3 [63:48] Register      */
#define CAN_MB25_LENGTH		0xFFC02F30	/* Mailbox 25 Data Length Code Register         */
#define CAN_MB25_TIMESTAMP	0xFFC02F34	/* Mailbox 25 Time Stamp Value Register         */
#define CAN_MB25_ID0		0xFFC02F38	/* Mailbox 25 Identifier Low Register           */
#define CAN_MB25_ID1		0xFFC02F3C	/* Mailbox 25 Identifier High Register          */

#define CAN_MB26_DATA0		0xFFC02F40	/* Mailbox 26 Data Word 0 [15:0] Register       */
#define CAN_MB26_DATA1		0xFFC02F44	/* Mailbox 26 Data Word 1 [31:16] Register      */
#define CAN_MB26_DATA2		0xFFC02F48	/* Mailbox 26 Data Word 2 [47:32] Register      */
#define CAN_MB26_DATA3		0xFFC02F4C	/* Mailbox 26 Data Word 3 [63:48] Register      */
#define CAN_MB26_LENGTH		0xFFC02F50	/* Mailbox 26 Data Length Code Register         */
#define CAN_MB26_TIMESTAMP	0xFFC02F54	/* Mailbox 26 Time Stamp Value Register         */
#define CAN_MB26_ID0		0xFFC02F58	/* Mailbox 26 Identifier Low Register           */
#define CAN_MB26_ID1		0xFFC02F5C	/* Mailbox 26 Identifier High Register          */

#define CAN_MB27_DATA0		0xFFC02F60	/* Mailbox 27 Data Word 0 [15:0] Register       */
#define CAN_MB27_DATA1		0xFFC02F64	/* Mailbox 27 Data Word 1 [31:16] Register      */
#define CAN_MB27_DATA2		0xFFC02F68	/* Mailbox 27 Data Word 2 [47:32] Register      */
#define CAN_MB27_DATA3		0xFFC02F6C	/* Mailbox 27 Data Word 3 [63:48] Register      */
#define CAN_MB27_LENGTH		0xFFC02F70	/* Mailbox 27 Data Length Code Register         */
#define CAN_MB27_TIMESTAMP	0xFFC02F74	/* Mailbox 27 Time Stamp Value Register         */
#define CAN_MB27_ID0		0xFFC02F78	/* Mailbox 27 Identifier Low Register           */
#define CAN_MB27_ID1		0xFFC02F7C	/* Mailbox 27 Identifier High Register          */

#define CAN_MB28_DATA0		0xFFC02F80	/* Mailbox 28 Data Word 0 [15:0] Register       */
#define CAN_MB28_DATA1		0xFFC02F84	/* Mailbox 28 Data Word 1 [31:16] Register      */
#define CAN_MB28_DATA2		0xFFC02F88	/* Mailbox 28 Data Word 2 [47:32] Register      */
#define CAN_MB28_DATA3		0xFFC02F8C	/* Mailbox 28 Data Word 3 [63:48] Register      */
#define CAN_MB28_LENGTH		0xFFC02F90	/* Mailbox 28 Data Length Code Register         */
#define CAN_MB28_TIMESTAMP	0xFFC02F94	/* Mailbox 28 Time Stamp Value Register         */
#define CAN_MB28_ID0		0xFFC02F98	/* Mailbox 28 Identifier Low Register           */
#define CAN_MB28_ID1		0xFFC02F9C	/* Mailbox 28 Identifier High Register          */

#define CAN_MB29_DATA0		0xFFC02FA0	/* Mailbox 29 Data Word 0 [15:0] Register       */
#define CAN_MB29_DATA1		0xFFC02FA4	/* Mailbox 29 Data Word 1 [31:16] Register      */
#define CAN_MB29_DATA2		0xFFC02FA8	/* Mailbox 29 Data Word 2 [47:32] Register      */
#define CAN_MB29_DATA3		0xFFC02FAC	/* Mailbox 29 Data Word 3 [63:48] Register      */
#define CAN_MB29_LENGTH		0xFFC02FB0	/* Mailbox 29 Data Length Code Register         */
#define CAN_MB29_TIMESTAMP	0xFFC02FB4	/* Mailbox 29 Time Stamp Value Register         */
#define CAN_MB29_ID0		0xFFC02FB8	/* Mailbox 29 Identifier Low Register           */
#define CAN_MB29_ID1		0xFFC02FBC	/* Mailbox 29 Identifier High Register          */

#define CAN_MB30_DATA0		0xFFC02FC0	/* Mailbox 30 Data Word 0 [15:0] Register       */
#define CAN_MB30_DATA1		0xFFC02FC4	/* Mailbox 30 Data Word 1 [31:16] Register      */
#define CAN_MB30_DATA2		0xFFC02FC8	/* Mailbox 30 Data Word 2 [47:32] Register      */
#define CAN_MB30_DATA3		0xFFC02FCC	/* Mailbox 30 Data Word 3 [63:48] Register      */
#define CAN_MB30_LENGTH		0xFFC02FD0	/* Mailbox 30 Data Length Code Register         */
#define CAN_MB30_TIMESTAMP	0xFFC02FD4	/* Mailbox 30 Time Stamp Value Register         */
#define CAN_MB30_ID0		0xFFC02FD8	/* Mailbox 30 Identifier Low Register           */
#define CAN_MB30_ID1		0xFFC02FDC	/* Mailbox 30 Identifier High Register          */

#define CAN_MB31_DATA0		0xFFC02FE0	/* Mailbox 31 Data Word 0 [15:0] Register       */
#define CAN_MB31_DATA1		0xFFC02FE4	/* Mailbox 31 Data Word 1 [31:16] Register      */
#define CAN_MB31_DATA2		0xFFC02FE8	/* Mailbox 31 Data Word 2 [47:32] Register      */
#define CAN_MB31_DATA3		0xFFC02FEC	/* Mailbox 31 Data Word 3 [63:48] Register      */
#define CAN_MB31_LENGTH		0xFFC02FF0	/* Mailbox 31 Data Length Code Register         */
#define CAN_MB31_TIMESTAMP	0xFFC02FF4	/* Mailbox 31 Time Stamp Value Register         */
#define CAN_MB31_ID0		0xFFC02FF8	/* Mailbox 31 Identifier Low Register           */
#define CAN_MB31_ID1		0xFFC02FFC	/* Mailbox 31 Identifier High Register          */

/* CAN Mailbox Area Macros				*/
#define CAN_MB_ID1(x)		(CAN_MB00_ID1+((x)*0x20))
#define CAN_MB_ID0(x)		(CAN_MB00_ID0+((x)*0x20))
#define CAN_MB_TIMESTAMP(x)	(CAN_MB00_TIMESTAMP+((x)*0x20))
#define CAN_MB_LENGTH(x)	(CAN_MB00_LENGTH+((x)*0x20))
#define CAN_MB_DATA3(x)		(CAN_MB00_DATA3+((x)*0x20))
#define CAN_MB_DATA2(x)		(CAN_MB00_DATA2+((x)*0x20))
#define CAN_MB_DATA1(x)		(CAN_MB00_DATA1+((x)*0x20))
#define CAN_MB_DATA0(x)		(CAN_MB00_DATA0+((x)*0x20))

/* Pin Control Registers	(0xFFC03200 - 0xFFC032FF)											*/
#define PORTF_FER			0xFFC03200	/* Port F Function Enable Register (Alternate/Flag*)    */
#define PORTG_FER			0xFFC03204	/* Port G Function Enable Register (Alternate/Flag*)    */
#define PORTH_FER			0xFFC03208	/* Port H Function Enable Register (Alternate/Flag*)    */
#define BFIN_PORT_MUX			0xFFC0320C	/* Port Multiplexer Control Register                                    */

/* Handshake MDMA Registers	(0xFFC03300 - 0xFFC033FF)										*/
#define HMDMA0_CONTROL		0xFFC03300	/* Handshake MDMA0 Control Register                                     */
#define HMDMA0_ECINIT		0xFFC03304	/* HMDMA0 Initial Edge Count Register                           */
#define HMDMA0_BCINIT		0xFFC03308	/* HMDMA0 Initial Block Count Register                          */
#define HMDMA0_ECURGENT		0xFFC0330C	/* HMDMA0 Urgent Edge Count Threshhold Register         */
#define HMDMA0_ECOVERFLOW	0xFFC03310	/* HMDMA0 Edge Count Overflow Interrupt Register        */
#define HMDMA0_ECOUNT		0xFFC03314	/* HMDMA0 Current Edge Count Register                           */
#define HMDMA0_BCOUNT		0xFFC03318	/* HMDMA0 Current Block Count Register                          */

#define HMDMA1_CONTROL		0xFFC03340	/* Handshake MDMA1 Control Register                                     */
#define HMDMA1_ECINIT		0xFFC03344	/* HMDMA1 Initial Edge Count Register                           */
#define HMDMA1_BCINIT		0xFFC03348	/* HMDMA1 Initial Block Count Register                          */
#define HMDMA1_ECURGENT		0xFFC0334C	/* HMDMA1 Urgent Edge Count Threshhold Register         */
#define HMDMA1_ECOVERFLOW	0xFFC03350	/* HMDMA1 Edge Count Overflow Interrupt Register        */
#define HMDMA1_ECOUNT		0xFFC03354	/* HMDMA1 Current Edge Count Register                           */
#define HMDMA1_BCOUNT		0xFFC03358	/* HMDMA1 Current Block Count Register                          */

/***********************************************************************************
** System MMR Register Bits And Macros
**
** Disclaimer:	All macros are intended to make C and Assembly code more readable.
**				Use these macros carefully, as any that do left shifts for field
**				depositing will result in the lower order bits being destroyed.  Any
**				macro that shifts left to properly position the bit-field should be
**				used as part of an OR to initialize a register and NOT as a dynamic
**				modifier UNLESS the lower order bits are saved and ORed back in when
**				the macro is used.
*************************************************************************************/
/*
** ********************* PLL AND RESET MASKS ****************************************/
/* PLL_CTL Masks																	*/
#define DF				0x0001	/* 0: PLL = CLKIN, 1: PLL = CLKIN/2                                     */
#define PLL_OFF			0x0002	/* PLL Not Powered                                                                      */
#define STOPCK			0x0008	/* Core Clock Off                                                                       */
#define PDWN			0x0020	/* Enter Deep Sleep Mode                                                        */
#define	IN_DELAY		0x0040	/* Add 200ps Delay To EBIU Input Latches                        */
#define	OUT_DELAY		0x0080	/* Add 200ps Delay To EBIU Output Signals                       */
#define BYPASS			0x0100	/* Bypass the PLL                                                                       */
#define	MSEL			0x7E00	/* Multiplier Select For CCLK/VCO Factors                       */
/* PLL_CTL Macros (Only Use With Logic OR While Setting Lower Order Bits)			*/
#define	SET_MSEL(x)		(((x)&0x3F) << 0x9)	/* Set MSEL = 0-63 --> VCO = CLKIN*MSEL         */

/* PLL_DIV Masks														*/
#define SSEL			0x000F	/* System Select                                                */
#define	CSEL			0x0030	/* Core Select                                                  */
#define CSEL_DIV1		0x0000	/*              CCLK = VCO / 1                                  */
#define CSEL_DIV2		0x0010	/*              CCLK = VCO / 2                                  */
#define	CSEL_DIV4		0x0020	/*              CCLK = VCO / 4                                  */
#define	CSEL_DIV8		0x0030	/*              CCLK = VCO / 8                                  */
/* PLL_DIV Macros														*/
#define SET_SSEL(x)		((x)&0xF)	/* Set SSEL = 0-15 --> SCLK = VCO/SSEL  */

/* VR_CTL Masks																	*/
#define	FREQ			0x0003	/* Switching Oscillator Frequency For Regulator */
#define	HIBERNATE		0x0000	/*              Powerdown/Bypass On-Board Regulation    */
#define	FREQ_333		0x0001	/*              Switching Frequency Is 333 kHz                  */
#define	FREQ_667		0x0002	/*              Switching Frequency Is 667 kHz                  */
#define	FREQ_1000		0x0003	/*              Switching Frequency Is 1 MHz                    */

#define GAIN			0x000C	/* Voltage Level Gain   */
#define	GAIN_5			0x0000	/*              GAIN = 5                */
#define	GAIN_10			0x0004	/*              GAIN = 10               */
#define	GAIN_20			0x0008	/*              GAIN = 20               */
#define	GAIN_50			0x000C	/*              GAIN = 50               */

#define	VLEV			0x00F0	/* Internal Voltage Level                                       */
#define	VLEV_085 		0x0060	/*              VLEV = 0.85 V (-5% - +10% Accuracy)     */
#define	VLEV_090		0x0070	/*              VLEV = 0.90 V (-5% - +10% Accuracy)     */
#define	VLEV_095		0x0080	/*              VLEV = 0.95 V (-5% - +10% Accuracy)     */
#define	VLEV_100		0x0090	/*              VLEV = 1.00 V (-5% - +10% Accuracy)     */
#define	VLEV_105		0x00A0	/*              VLEV = 1.05 V (-5% - +10% Accuracy)     */
#define	VLEV_110		0x00B0	/*              VLEV = 1.10 V (-5% - +10% Accuracy)     */
#define	VLEV_115		0x00C0	/*              VLEV = 1.15 V (-5% - +10% Accuracy)     */
#define	VLEV_120		0x00D0	/*              VLEV = 1.20 V (-5% - +10% Accuracy)     */
#define	VLEV_125		0x00E0	/*              VLEV = 1.25 V (-5% - +10% Accuracy)     */
#define	VLEV_130		0x00F0	/*              VLEV = 1.30 V (-5% - +10% Accuracy)     */

#define	WAKE			0x0100	/* Enable RTC/Reset Wakeup From Hibernate       */
#define	CANWE			0x0200	/* Enable CAN Wakeup From Hibernate			*/
#define	PHYWE			0x0400	/* Enable PHY Wakeup From Hibernate			*/
#define	CLKBUFOE		0x4000	/* CLKIN Buffer Output Enable */
#define	PHYCLKOE		CLKBUFOE	/* Alternative legacy name for the above */
#define	SCKELOW		0x8000	/* Enable Drive CKE Low During Reset		*/

/* PLL_STAT Masks																	*/
#define ACTIVE_PLLENABLED	0x0001	/* Processor In Active Mode With PLL Enabled    */
#define	FULL_ON				0x0002	/* Processor In Full On Mode                                    */
#define ACTIVE_PLLDISABLED	0x0004	/* Processor In Active Mode With PLL Disabled   */
#define	PLL_LOCKED			0x0020	/* PLL_LOCKCNT Has Been Reached                                 */

/* CHIPID Masks */
#define CHIPID_VERSION         0xF0000000
#define CHIPID_FAMILY          0x0FFFF000
#define CHIPID_MANUFACTURE     0x00000FFE

/* SWRST Masks																		*/
#define SYSTEM_RESET		0x0007	/* Initiates A System Software Reset                    */
#define	DOUBLE_FAULT		0x0008	/* Core Double Fault Causes Reset                               */
#define RESET_DOUBLE		0x2000	/* SW Reset Generated By Core Double-Fault              */
#define RESET_WDOG			0x4000	/* SW Reset Generated By Watchdog Timer                 */
#define RESET_SOFTWARE		0x8000	/* SW Reset Occurred Since Last Read Of SWRST   */

/* SYSCR Masks																				*/
#define BMODE				0x0007	/* Boot Mode - Latched During HW Reset From Mode Pins   */
#define	NOBOOT				0x0010	/* Execute From L1 or ASYNC Bank 0 When BMODE = 0               */

/* *************  SYSTEM INTERRUPT CONTROLLER MASKS *************************************/

/* SIC_IAR0 Macros															*/
#define P0_IVG(x)		(((x)&0xF)-7)	/* Peripheral #0 assigned IVG #x        */
#define P1_IVG(x)		(((x)&0xF)-7) << 0x4	/* Peripheral #1 assigned IVG #x        */
#define P2_IVG(x)		(((x)&0xF)-7) << 0x8	/* Peripheral #2 assigned IVG #x        */
#define P3_IVG(x)		(((x)&0xF)-7) << 0xC	/* Peripheral #3 assigned IVG #x        */
#define P4_IVG(x)		(((x)&0xF)-7) << 0x10	/* Peripheral #4 assigned IVG #x        */
#define P5_IVG(x)		(((x)&0xF)-7) << 0x14	/* Peripheral #5 assigned IVG #x        */
#define P6_IVG(x)		(((x)&0xF)-7) << 0x18	/* Peripheral #6 assigned IVG #x        */
#define P7_IVG(x)		(((x)&0xF)-7) << 0x1C	/* Peripheral #7 assigned IVG #x        */

/* SIC_IAR1 Macros															*/
#define P8_IVG(x)		(((x)&0xF)-7)	/* Peripheral #8 assigned IVG #x        */
#define P9_IVG(x)		(((x)&0xF)-7) << 0x4	/* Peripheral #9 assigned IVG #x        */
#define P10_IVG(x)		(((x)&0xF)-7) << 0x8	/* Peripheral #10 assigned IVG #x       */
#define P11_IVG(x)		(((x)&0xF)-7) << 0xC	/* Peripheral #11 assigned IVG #x       */
#define P12_IVG(x)		(((x)&0xF)-7) << 0x10	/* Peripheral #12 assigned IVG #x       */
#define P13_IVG(x)		(((x)&0xF)-7) << 0x14	/* Peripheral #13 assigned IVG #x       */
#define P14_IVG(x)		(((x)&0xF)-7) << 0x18	/* Peripheral #14 assigned IVG #x       */
#define P15_IVG(x)		(((x)&0xF)-7) << 0x1C	/* Peripheral #15 assigned IVG #x       */

/* SIC_IAR2 Macros															*/
#define P16_IVG(x)		(((x)&0xF)-7)	/* Peripheral #16 assigned IVG #x       */
#define P17_IVG(x)		(((x)&0xF)-7) << 0x4	/* Peripheral #17 assigned IVG #x       */
#define P18_IVG(x)		(((x)&0xF)-7) << 0x8	/* Peripheral #18 assigned IVG #x       */
#define P19_IVG(x)		(((x)&0xF)-7) << 0xC	/* Peripheral #19 assigned IVG #x       */
#define P20_IVG(x)		(((x)&0xF)-7) << 0x10	/* Peripheral #20 assigned IVG #x       */
#define P21_IVG(x)		(((x)&0xF)-7) << 0x14	/* Peripheral #21 assigned IVG #x       */
#define P22_IVG(x)		(((x)&0xF)-7) << 0x18	/* Peripheral #22 assigned IVG #x       */
#define P23_IVG(x)		(((x)&0xF)-7) << 0x1C	/* Peripheral #23 assigned IVG #x       */

/* SIC_IAR3 Macros															*/
#define P24_IVG(x)		(((x)&0xF)-7)	/* Peripheral #24 assigned IVG #x       */
#define P25_IVG(x)		(((x)&0xF)-7) << 0x4	/* Peripheral #25 assigned IVG #x       */
#define P26_IVG(x)		(((x)&0xF)-7) << 0x8	/* Peripheral #26 assigned IVG #x       */
#define P27_IVG(x)		(((x)&0xF)-7) << 0xC	/* Peripheral #27 assigned IVG #x       */
#define P28_IVG(x)		(((x)&0xF)-7) << 0x10	/* Peripheral #28 assigned IVG #x       */
#define P29_IVG(x)		(((x)&0xF)-7) << 0x14	/* Peripheral #29 assigned IVG #x       */
#define P30_IVG(x)		(((x)&0xF)-7) << 0x18	/* Peripheral #30 assigned IVG #x       */
#define P31_IVG(x)		(((x)&0xF)-7) << 0x1C	/* Peripheral #31 assigned IVG #x       */

/* SIC_IMASK Masks																		*/
#define SIC_UNMASK_ALL	0x00000000	/* Unmask all peripheral interrupts     */
#define SIC_MASK_ALL	0xFFFFFFFF	/* Mask all peripheral interrupts       */
#define SIC_MASK(x)		(1 << ((x)&0x1F))	/* Mask Peripheral #x interrupt         */
#define SIC_UNMASK(x)	(0xFFFFFFFF ^ (1 << ((x)&0x1F)))	/* Unmask Peripheral #x interrupt       */

/* SIC_IWR Masks																		*/
#define IWR_DISABLE_ALL	0x00000000	/* Wakeup Disable all peripherals       */
#define IWR_ENABLE_ALL	0xFFFFFFFF	/* Wakeup Enable all peripherals        */
#define IWR_ENABLE(x)	(1 << ((x)&0x1F))	/* Wakeup Enable Peripheral #x          */
#define IWR_DISABLE(x)	(0xFFFFFFFF ^ (1 << ((x)&0x1F)))	/* Wakeup Disable Peripheral #x         */

/* ************** UART CONTROLLER MASKS *************************/
/* UARTx_LCR Masks												*/
#define WLS(x)		(((x)-5) & 0x03)	/* Word Length Select   */
#define STB			0x04	/* Stop Bits                    */
#define PEN			0x08	/* Parity Enable                */
#define EPS			0x10	/* Even Parity Select   */
#define STP			0x20	/* Stick Parity                 */
#define SB			0x40	/* Set Break                    */
#define DLAB		0x80	/* Divisor Latch Access */

/* UARTx_MCR Mask										*/
#define LOOP_ENA		0x10	/* Loopback Mode Enable         */
#define LOOP_ENA_P	0x04
/* UARTx_LSR Masks										*/
#define DR			0x01	/* Data Ready                           */
#define OE			0x02	/* Overrun Error                        */
#define PE			0x04	/* Parity Error                         */
#define FE			0x08	/* Framing Error                        */
#define BI			0x10	/* Break Interrupt                      */
#define THRE		0x20	/* THR Empty                            */
#define TEMT		0x40	/* TSR and UART_THR Empty       */

/* UARTx_IER Masks															*/
#define ERBFI		0x01	/* Enable Receive Buffer Full Interrupt         */
#define ETBEI		0x02	/* Enable Transmit Buffer Empty Interrupt       */
#define ELSI		0x04	/* Enable RX Status Interrupt                           */

/* UARTx_IIR Masks														*/
#define NINT		0x01	/* Pending Interrupt                                    */
#define IIR_TX_READY    0x02	/* UART_THR empty                               */
#define IIR_RX_READY    0x04	/* Receive data ready                           */
#define IIR_LINE_CHANGE 0x06	/* Receive line status                          */
#define IIR_STATUS	0x06

/* UARTx_GCTL Masks													*/
#define UCEN		0x01	/* Enable UARTx Clocks                          */
#define IREN		0x02	/* Enable IrDA Mode                                     */
#define TPOLC		0x04	/* IrDA TX Polarity Change                      */
#define RPOLC		0x08	/* IrDA RX Polarity Change                      */
#define FPE			0x10	/* Force Parity Error On Transmit       */
#define FFE			0x20	/* Force Framing Error On Transmit      */

/* ***********  SERIAL PERIPHERAL INTERFACE (SPI) MASKS  ****************************/
/* SPI_CTL Masks																	*/
#define	TIMOD		0x0003	/* Transfer Initiate Mode                                                       */
#define RDBR_CORE	0x0000	/*              RDBR Read Initiates, IRQ When RDBR Full         */
#define	TDBR_CORE	0x0001	/*              TDBR Write Initiates, IRQ When TDBR Empty       */
#define RDBR_DMA	0x0002	/*              DMA Read, DMA Until FIFO Empty                          */
#define TDBR_DMA	0x0003	/*              DMA Write, DMA Until FIFO Full                          */
#define SZ			0x0004	/* Send Zero (When TDBR Empty, Send Zero/Last*)         */
#define GM			0x0008	/* Get More (When RDBR Full, Overwrite/Discard*)        */
#define PSSE		0x0010	/* Slave-Select Input Enable                                            */
#define EMISO		0x0020	/* Enable MISO As Output                                                        */
#define SIZE		0x0100	/* Size of Words (16/8* Bits)                                           */
#define LSBF		0x0200	/* LSB First                                                                            */
#define CPHA		0x0400	/* Clock Phase                                                                          */
#define CPOL		0x0800	/* Clock Polarity                                                                       */
#define MSTR		0x1000	/* Master/Slave*                                                                        */
#define WOM			0x2000	/* Write Open Drain Master                                                      */
#define SPE			0x4000	/* SPI Enable                                                                           */

/* SPI_FLG Masks																	*/
#define FLS1		0x0002	/* Enables SPI_FLOUT1 as SPI Slave-Select Output        */
#define FLS2		0x0004	/* Enables SPI_FLOUT2 as SPI Slave-Select Output        */
#define FLS3		0x0008	/* Enables SPI_FLOUT3 as SPI Slave-Select Output        */
#define FLS4		0x0010	/* Enables SPI_FLOUT4 as SPI Slave-Select Output        */
#define FLS5		0x0020	/* Enables SPI_FLOUT5 as SPI Slave-Select Output        */
#define FLS6		0x0040	/* Enables SPI_FLOUT6 as SPI Slave-Select Output        */
#define FLS7		0x0080	/* Enables SPI_FLOUT7 as SPI Slave-Select Output        */
#define FLG1		0xFDFF	/* Activates SPI_FLOUT1                                                         */
#define FLG2		0xFBFF	/* Activates SPI_FLOUT2                                                         */
#define FLG3		0xF7FF	/* Activates SPI_FLOUT3                                                         */
#define FLG4		0xEFFF	/* Activates SPI_FLOUT4                                                         */
#define FLG5		0xDFFF	/* Activates SPI_FLOUT5                                                         */
#define FLG6		0xBFFF	/* Activates SPI_FLOUT6                                                         */
#define FLG7		0x7FFF	/* Activates SPI_FLOUT7                                                         */

/* SPI_STAT Masks																				*/
#define SPIF		0x0001	/* SPI Finished (Single-Word Transfer Complete)                                 */
#define MODF		0x0002	/* Mode Fault Error (Another Device Tried To Become Master)             */
#define TXE			0x0004	/* Transmission Error (Data Sent With No New Data In TDBR)              */
#define TXS			0x0008	/* SPI_TDBR Data Buffer Status (Full/Empty*)                                    */
#define RBSY		0x0010	/* Receive Error (Data Received With RDBR Full)                                 */
#define RXS			0x0020	/* SPI_RDBR Data Buffer Status (Full/Empty*)                                    */
#define TXCOL		0x0040	/* Transmit Collision Error (Corrupt Data May Have Been Sent)   */

/*  ****************  GENERAL PURPOSE TIMER MASKS  **********************/
/* TIMER_ENABLE Masks													*/
#define TIMEN0			0x0001	/* Enable Timer 0                                       */
#define TIMEN1			0x0002	/* Enable Timer 1                                       */
#define TIMEN2			0x0004	/* Enable Timer 2                                       */
#define TIMEN3			0x0008	/* Enable Timer 3                                       */
#define TIMEN4			0x0010	/* Enable Timer 4                                       */
#define TIMEN5			0x0020	/* Enable Timer 5                                       */
#define TIMEN6			0x0040	/* Enable Timer 6                                       */
#define TIMEN7			0x0080	/* Enable Timer 7                                       */

/* TIMER_DISABLE Masks													*/
#define TIMDIS0			TIMEN0	/* Disable Timer 0                                      */
#define TIMDIS1			TIMEN1	/* Disable Timer 1                                      */
#define TIMDIS2			TIMEN2	/* Disable Timer 2                                      */
#define TIMDIS3			TIMEN3	/* Disable Timer 3                                      */
#define TIMDIS4			TIMEN4	/* Disable Timer 4                                      */
#define TIMDIS5			TIMEN5	/* Disable Timer 5                                      */
#define TIMDIS6			TIMEN6	/* Disable Timer 6                                      */
#define TIMDIS7			TIMEN7	/* Disable Timer 7                                      */

/* TIMER_STATUS Masks													*/
#define TIMIL0			0x00000001	/* Timer 0 Interrupt                            */
#define TIMIL1			0x00000002	/* Timer 1 Interrupt                            */
#define TIMIL2			0x00000004	/* Timer 2 Interrupt                            */
#define TIMIL3			0x00000008	/* Timer 3 Interrupt                            */
#define TOVF_ERR0		0x00000010	/* Timer 0 Counter Overflow			*/
#define TOVF_ERR1		0x00000020	/* Timer 1 Counter Overflow			*/
#define TOVF_ERR2		0x00000040	/* Timer 2 Counter Overflow			*/
#define TOVF_ERR3		0x00000080	/* Timer 3 Counter Overflow			*/
#define TRUN0			0x00001000	/* Timer 0 Slave Enable Status          */
#define TRUN1			0x00002000	/* Timer 1 Slave Enable Status          */
#define TRUN2			0x00004000	/* Timer 2 Slave Enable Status          */
#define TRUN3			0x00008000	/* Timer 3 Slave Enable Status          */
#define TIMIL4			0x00010000	/* Timer 4 Interrupt                            */
#define TIMIL5			0x00020000	/* Timer 5 Interrupt                            */
#define TIMIL6			0x00040000	/* Timer 6 Interrupt                            */
#define TIMIL7			0x00080000	/* Timer 7 Interrupt                            */
#define TOVF_ERR4		0x00100000	/* Timer 4 Counter Overflow			*/
#define TOVF_ERR5		0x00200000	/* Timer 5 Counter Overflow			*/
#define TOVF_ERR6		0x00400000	/* Timer 6 Counter Overflow			*/
#define TOVF_ERR7		0x00800000	/* Timer 7 Counter Overflow			*/
#define TRUN4			0x10000000	/* Timer 4 Slave Enable Status          */
#define TRUN5			0x20000000	/* Timer 5 Slave Enable Status          */
#define TRUN6			0x40000000	/* Timer 6 Slave Enable Status          */
#define TRUN7			0x80000000	/* Timer 7 Slave Enable Status          */

/* Alternate Deprecated Macros Provided For Backwards Code Compatibility */
#define TOVL_ERR0 TOVF_ERR0
#define TOVL_ERR1 TOVF_ERR1
#define TOVL_ERR2 TOVF_ERR2
#define TOVL_ERR3 TOVF_ERR3
#define TOVL_ERR4 TOVF_ERR4
#define TOVL_ERR5 TOVF_ERR5
#define TOVL_ERR6 TOVF_ERR6
#define TOVL_ERR7 TOVF_ERR7
/* TIMERx_CONFIG Masks													*/
#define PWM_OUT			0x0001	/* Pulse-Width Modulation Output Mode   */
#define WDTH_CAP		0x0002	/* Width Capture Input Mode                             */
#define EXT_CLK			0x0003	/* External Clock Mode                                  */
#define PULSE_HI		0x0004	/* Action Pulse (Positive/Negative*)    */
#define PERIOD_CNT		0x0008	/* Period Count                                                 */
#define IRQ_ENA			0x0010	/* Interrupt Request Enable                             */
#define TIN_SEL			0x0020	/* Timer Input Select                                   */
#define OUT_DIS			0x0040	/* Output Pad Disable                                   */
#define CLK_SEL			0x0080	/* Timer Clock Select                                   */
#define TOGGLE_HI		0x0100	/* PWM_OUT PULSE_HI Toggle Mode                 */
#define EMU_RUN			0x0200	/* Emulation Behavior Select                    */
#define ERR_TYP			0xC000	/* Error Type                                                   */

/* ******************   GPIO PORTS F, G, H MASKS  ***********************/
/*  General Purpose IO (0xFFC00700 - 0xFFC007FF)  Masks 				*/
/* Port F Masks 														*/
#define PF0		0x0001
#define PF1		0x0002
#define PF2		0x0004
#define PF3		0x0008
#define PF4		0x0010
#define PF5		0x0020
#define PF6		0x0040
#define PF7		0x0080
#define PF8		0x0100
#define PF9		0x0200
#define PF10	0x0400
#define PF11	0x0800
#define PF12	0x1000
#define PF13	0x2000
#define PF14	0x4000
#define PF15	0x8000

/* Port G Masks															*/
#define PG0		0x0001
#define PG1		0x0002
#define PG2		0x0004
#define PG3		0x0008
#define PG4		0x0010
#define PG5		0x0020
#define PG6		0x0040
#define PG7		0x0080
#define PG8		0x0100
#define PG9		0x0200
#define PG10	0x0400
#define PG11	0x0800
#define PG12	0x1000
#define PG13	0x2000
#define PG14	0x4000
#define PG15	0x8000

/* Port H Masks															*/
#define PH0		0x0001
#define PH1		0x0002
#define PH2		0x0004
#define PH3		0x0008
#define PH4		0x0010
#define PH5		0x0020
#define PH6		0x0040
#define PH7		0x0080
#define PH8		0x0100
#define PH9		0x0200
#define PH10	0x0400
#define PH11	0x0800
#define PH12	0x1000
#define PH13	0x2000
#define PH14	0x4000
#define PH15	0x8000

/* *******************  SERIAL PORT MASKS  **************************************/
/* SPORTx_TCR1 Masks															*/
#define TSPEN		0x0001	/* Transmit Enable                                                              */
#define ITCLK		0x0002	/* Internal Transmit Clock Select                               */
#define DTYPE_NORM	0x0004	/* Data Format Normal                                                   */
#define DTYPE_ULAW	0x0008	/* Compand Using u-Law                                                  */
#define DTYPE_ALAW	0x000C	/* Compand Using A-Law t 2005-2008 Analog Devices Inc.
 *
 * Licensed un*/
#define TLSBIT		0x0010	/* Transmit Bit Ordert 2005-2008 Analog Devices Inc.
 *
 * Licensed undder the ADI ITFScense20r theInternale GPL-2 (oFrame Sync Select *
 * Licensed under the ADI BFSRcense4sters efinitions */
#incluRequiredude <asm/def_LPBlackfin.h>

/*******D Core reg8sters Data-Independentdefinitions */
#include <asm/er the ADI LCore re10sters Lowdefinitions */
#include <asm/def_LPBlackfin.h_BF534_H

/* Include allLACore re2********ate******************/
/* Clock and System Control	(0xFFC000er the ADI BCKFEcens4*******Clock Fall CopEdge/* Clock and System Control	(0xFFC0000        
/* SPORTx_TCR2 Masks and Macro	     */
#defer the ADI SLEN(x)		((x)&0x1F) the      TX Word Length (2 - 31) Devices Inc.
 *
 * Licensed under the ADI BXS     01*******X Secondary Enablet 2005-2008 Analog Devices Inc.
 *
 * Licensed under the ADI BSFPLL Staisters  GPL-2 (oStereons */
#inclu                          er the ADI BRFSicense*******Left/Righlater)
 (1 =  /* ChChannel 1st          */
#              RCR1           */
#defifine VR_CTL		RSPENcense 01 theReceive                                   */
#define PLL_LOCKCNT_BF534_H

/* Include all RCLKFFC00102ers and bit dxFFC001F   */
IV				0xFFC00004	/* PLL Divide Registe********
** TYPE_NORMFC00104egister  Format Nefinl */

#ifndef _DEF_BF534_H
#define _DEF_BF534_H

/* Include all      ULAWFC00108     omp     * Copuright 2005-2008 Analog Devices Inc.
 *
 * Licensed under the ADI       Adefine SIC_IMASK			0xFFC0010yright 2005-2008 Analog Devices Inc.
 *
 * Licensed under the ADI RSD license or thexFFC001Fr later)
 */

#ifndef _DEF_BF534_H
#define _DEF_BF534_H

/*               ore registers and bit dxFFC001F********/
/* Clock and System Contro/
#define SI***************Assignment Register**********************************************0xFFC00************Assignment Register 2                      *ol	(0xFFC00000 - 0xFFC000F0xFFC00			*/
#definexFFC00120	/* Interrupt Status Register                  /
#define SI                   */
#define PLL_DIV				0xFFC00004	/* PLL Divide Register                 R         System Interrupt Controlle		0xFFC00008	/* Voltage RegulRtor Control Register           */
#define PLL_STAT			0xFFC0000C	/R PLL Status RegRster                                          */
#define PLL_LOCKCNT			0xFFC000R0	/* PLL Lock Co  */er                                      */
Register                  ID				0xFFC00014 /* C-First  */
#ter)
 */

#ifndef _DEF_BF534_H
#define _DEF_BF534_H
              STAT/

/* System Interrupt ControllerXNLL Sta100 - 0xFFC001FFIFO Not Empty Status* Watchdog Status Register                  UVF            StickyxFFC00124Underflow */
#define RTC_ISTAT			0xFFC00308	/*            OStatus Re     r              Ov       */
#define RTC_SWCNT			0xFFC0030C	/*0xFFC0000C	/* F	rol RegC_IMAefinitions    Full */
#define RTC_ISTAT			0xFFC00308	/* R310	/* RTC Alarm Status Ror ther       GPL-2 (o          */
#define RTC_SWCNT			0xFFC0030C	/er the ADI Bch Count 2ler Enable Register              */
#define RTC_ALARM			0xFFC00310xFFC0000C	/* HRtrol Re4r the GPL-2 (oHold Register                                       */
#define RTCMCMC*/

croontrol Register             P_WOFFFFC00008	 & 0x3FoltageMultic       Window Offset Field                     */
#defOnly use WSIZE       With Logic OR Whil_DIVtt CopLowerus RegiBitontrol UART0_RBR			0xF0xFFFFC0(e UAx)>>0x3)-1	/* F) << 0xCuffer register           Size = (x/8)-1ister                hdog Control Register            REC_BYPASSol Regr theBypass Mode (Noe SYSCRRecovery          */
#define PLL_STAT		0404	/* Divisor2FROM4                   2 MHze SYSCRfrom 4         */C003FF)									*/
#define REC_8dent16ol Reg3n Register  8         */
#def16ne UART0_LCR			0xFFC0040C	er the ADI MCDTXPtrol Reg      register    DMARegister  Pack CopRT0_IIR			0xFFC00408	/* InterrMCDRodem ContrC_IMAgister           xFFC001F                   */
#defiT0_MCR			0xFFC00410	/*M0xFFC001or the register    s */
#                                       */
er the ADI FSD******08xFFC00418	/* Modem Statuincluto  */
#Relationship	0xFFC00410	/FD_0rol RegxFFC00418	/* Modem StatuDelay = 0g Devices Inc.
 *
 * Licensed under the ADI     1ine SIC_ISR	      */
#define UART0_GCTL    xFFC00424	/* Global Control Register         2ine SIC_IWR	      */
#define UART0_GCTL2ontroller			(0xFFC00500 - 0xFFC005FF)								*/
3cens3             */
#define UART0_GCTL3ontroller			(0xFFC00500 - 0xFFC005FF)								*/
4                  */
#define UART0_GCTL4ontroller			(0xFFC00500 - 0xFFC005FF)								*/
5cens5             */
#define UART0_GCTL5ontroller			(0xFFC00500 - 0xFFC005FF)								*/
6cens6             */
#define UART0_GCTL6ontroller			(0xFFC00500 - 0xFFC005FF)								*/
7cens7             */
#define UART0_GCTL7ontroller			(0xFFC00500 - 0xFFC005FF)								*/
8cens8             */
#define UART0_GCTL8ontroller			(0xFFC00500 - 0xFFC005FF)								*/
9cens9             */
#define UART0_GCTL9		0xFFC00424	/* Global Control Register               A                       */

/* SPI C			0xFFC00424	/* Global Control Rgister               B                       */

/* SPI CController			(0xFFC00500 - 0xFFC0gister          #defiC                       */

/* SPI Ce SPI_CTL				0xFFC00500	/* SPI Cogister               D                       */

/* SPI C/
#define SPI_FLG				0xFFC00504	/gister          ter  E                       */

/* SPI C       */
#define SPI_STAT			0xFFgister          atus F                       */

/* SPI C               */
#define SPI_TDB       *fine TIMER1_COUNTER	  ASYNCHRONOUS MEMORY CONTROLLER MASKS efine TIMER1_COUNTER		    /    EBIU_AMGCTLdog Control Register              AMCK0xFFol Register       CLKOUT			*/
#define SWRST				0xFFC00100	/* Software Reset Register         */

#define T	AMBEN_NOntrol Regr theAll Banks Dis                    */
#dester 1                      */
#define SIC_IAR2		      B                    AsncluMemory
#def 0 oT0_D Timer 2 Configuration Register                       */_B     ntrol ReER2_COUNTER		0xFFC00624	s 0 & 1 Timer 2 Counter Register                */
#define _B2IMER2_6ERIOD		0xFFC00628	/* Timer 2 Pe, 1,     e SPI_CTL				0xFFC00500	/                 ALLFC00414	/* LD		0xFFC00628	/* Timer 2 P(all) Regist2ster  /
#defin       #defineBCTL0ER1_PERIOD		0xFFC00618	/* Timer 1 B0RDYod Registeer R0 - 0624	/* (B0) RDYFF)							*/
#define SWRST				0xFFC00100	/* Software ResetFFC00634	/* TimePOER3_CONFter      B0     Act001FHigh			*/
#define SWRST				0xFFC00100	/* Software Reset Register              B0TT    Register       B0Registier   Time (ReadcratWrite)PI Cocyc               th Register     2            C_IMA                */

#define TIMER4_CO2FIG		0efineth Register     3            AR0		                */

#define TIMER4_CO3OUNTER		0xFFC00644	/* Timer4            -Byte)               */

#define TIMER4_CO4OUNTER		0xFFC00644	/* TiS              or theB0ow-Bup   */

AOEcrat#def/TIMER4_CONFIG		0xFFC00640	/* ne TIMER4_WIDTH		ation Registcaler r 4 Width Register                  COUNTER		0xF */

#define TIMER5_CONFI 4 Counter R3	/* Timer 5 Configuration Register       ine TIMER4_P */

#define TIMER5_CONFI8	/* Timer 4 Period R Width Register                     */
#defin */

#define TIMER5_COH              						B0ine UA  */

~          crat~AOE4_CONFIG		0xFFC00640	/* Timer 4 ConfiHFIG		0xFFC0065RT0_SCC	/* Timer 5 Width Register          COUNTER		0xFFC00644	/* TiHOUNTER		0xFFC0CxFFC00660	/* Timer 6 Configuration Registeine TIMER4_PERIOD		0xFFC0HT    * Timer 4 Period R* Timer 5 Width Register          0OUNTER		0xFFC00644	/* TiRA             tus Reg    defiAccessmer 5                */

#defMER3_PERIOD		0xFFC00638	/* Timer 3 PAFIG		0xFFC006isters 
#define TIMER6_WIDTH	                */
#d6 Width Register                        4 Counter 3 Period Refine TIMER6_WIDTH	                   */6 Width Register                       8	/* Timer *******7_COUNTER		0xFFC00674	                     6 Width Register                       5      */
#5 TIMER7_COUNTER		0xFFC00674	5 7 Period Register                              */
#define 6      */
#6 TIMER7_COUNTER		0xFFC00674	6 7 Period Register                              */
#define 7      */
#7 TIMER7_COUNTER		0xFFC00674	7 7 Period Register                              */
#define 8      */
#R Regis7_COUNTER		0xFFC00674	8 7 Period Register                              */
#define 9      */
#9 TIMER7_COUNTER		0xFFC00674	9 7 Period Register                              */
#define s		(0xter  A    */
#define TIMER6_WIDTH		Period Regis	*/
#define PORTFIO					0xFFC00700	/* Port F I/O TIMER2_00B    */
#define TIMER6_WIDTH		NFIG		0             */
#define RTC_FAST			ter               #defiTIMER6_   */
#define TIMER6_WIDTH		00670	/* Timer 7 Configuration Register   ter                    F I/OD Peripheral Interrupt Set Reg/* Timer 7 Counter Register               ter               ter  F I/OE Peripheral Interrupt Set Regr 7 Period Register                       ter               atus F I/OF Peripheral Interrupt Set Regh Register                               *th Register  W             ********B0 TIMERe TIMER6_WIDTH		0xFFC0066C	/* Timer 6 Width Register     ne PORTFIO_MASKA_ation Regi			*/
#d Port F I/O Mask Enableister                 */
#define PORTFIO_TOGGLE			0xFFCSKA_ 4 Counter         Port F I/O Mask Enable                                 */
#define PORTFIO_MASSKA_8	/* Timer         Port F I/O Mask Enableify Interrupt A Register   */
#define PORTFIO_MASKA_CLESKA_TIMER7_WIDregister Port F I/O Mask Enableerrupt A Register                 */
#define PORTFIO_MASKA_NABLE		0xF Transmi Port F I/O Mask Enableer                                        #define PORTFIO_MISABLE		0x Data Bu Port F I/O Mask Enablester                                      #define PORTFIO_MSTATUS		0xte Regis Port F I/O Mask Enableter                                       #define PORTFIO_Mrpose I/O PI_RDBR  Port F I/O Mask EnableF)												*/
#define PORTFIO					0xFFCne PORTFIO_MASKA_S         FFC00600 Port F I/O Mask Enable                    */
#define PORTFIO_CLEne PORTFIO_MASKA_SPort F I/n Regist Port F I/O Mask Enable gister               */
#define PORTFIO_Sne PORTFIO_MASKA_S Port F IRegister Port F I/O Mask Enable 00670	/* Timer 7 Configuration Register  ne PORTFIO_MASKA_S	/* Port  0 Perio Port F I/O Mask Enable /* Timer 7 Counter Register              ne PORTFIO_MASKA_S0xFFC0071C	/* Tim Port F I/O Mask Enable r 7 Period Register                      ne PORTFIO_MASKA_SxFFC00714G		0xFFC Port F I/O Mask Enable h Register                                   Register 1imer 3 Counte****RT0_TC24	/1 (B1                            */
#define th Register     iod Regist			*RT0_TC1                             */
#define TIMER3_me Sync Divid                                     */

#define TIMER4_CONFIG		0xFFC00 TX Data Registation Regte R                                          COUNTER		0xFFC00644	/* Tgist 4 CounteRegi                                          ine TIMER4_PERIOD		0xFFCgist8	/* Timer 4 Period                                       */
#define TIMER4_WID1H		0xFFC006/
#de         Width Register                               Transmit Configuation Re     ster                     */
#define SPORT             Transmit Configu 4 Count    lock Divider                                           Transmit Configu                      Timer 5 Period Register                      Transmit ConfMER5_WIDTH	             * Timer 5 Width Register                                      ation Re SPOR                    */
#define SPORT0_CHNL                         /
#define Tefine                    */
#define SPORT0_CHNLmit Configuration 1 Regis          */
#define T              */
#define SPORT0_CHNLPeriod Register         1            Regis         fine TIMER6_WIDTH		0xFFC0066C	/* Timer 6 Width Register                  #defiation Rve CloS0		0xFFC00840	/* SPORT0 Mu00670	/* Timer 7 Configuration Register                #defi 4 Coun0xFFC0S0		0xFFC00840	/* SPORT0 Mu/* Timer 7 Counter Register                            #defi8	/* Ti      S0		0xFFC00840	/* SPORT0 Mur 7 Period Register                              */
#de#defiTIMER7_regiT0 Multi-Channel Transmit Selh Register                               */

#define TI#defiNABLE		 TraT0 Multi-Channel Transmit Seler                                        */
#define TI#defiISABLE	 DatT0 Multi-Channel Transmit Selster                                       */
#define T#defiSTATUS	nel ReS0		0xFFC00840	/* SPORT0 Muter                                        */

/* Gener#defirpose IPI_RRegister 3       */

/* SPORTF)												*/
#define PORTFIO					0xFFC00700	/* Port#defin      FFC0TCS0		0xFFC00840	/* SPORT0 Mul                   */
#define PORTFIO_CLEAR			0xFFC007#defin TIMERn ReTCS0		0xFFC00840	/* SPORT0 Mulgister               */
#define PORTFIO_SET				0xFFC00#defin Port 38	/* S0		0xFFC00840	/* SPORT0 Mulister                 */
#define PORTFIO_TOGGLE			0xFF#defin	/* Po 0 PPORT1_TFSDIV		0xFFC0090C	/* SP                                 */
#define PORTFIO_MA#defin0xFFC0C	/*PORT1_TFSDIV		0xFFC0090C	/* SPify Interrupt A Register   */
#define PORTFIO_MASKA_CL#definxFFC00G		0PORT1_TFSDIV		0xFFC0090C	/* SPerrupt A Register                 */
#define PORTFIO_M1SKA_SET		0T0_MTCSC0083C	/rt F I/O Mask Enable Interrupt A Register                  */
#define PORTn 1 Ration 		0xFFC               */
#define SPORT1 Transmit Frame Sync Divider                       O_MASKB			0848	/*               */
#define SPT1 TX Data Register                                    B_CLEAR		0SPORT0                */
#define SPC00918	/* SPORT1 RX Data Register                      IO_MASKB_Sulti-Ch               */
#define SPORT1_RCR1			0xFFC00920	/* SPORT1 Transmit Configuration 1 RNABLE	nnel Re               */
#define SP Mask Toggle Enable Interrupt B Register   */
#define isterISABLEeive Se               */
#define SPion Register                                          isterSTATUSect Reg               */
#define SP Port F I/O Source Polarity Register                  isterrpose TCR1			               */
#define SPC00738	/* Port F I/O Source Sensitivity Register      n 1 Re		(0xFFC0                  */
#define SPOH			0xFFC0073C	/* Port F I/O Set on BOTH Edges Registn 1 Reration Re                  */
#define SPORTFIO_INEN			0xFFC00740	/* Port F I/O Input Enable Ren 1 Renter Regi                  */
#define SPO/* SPORT0 Controller		(0xFFC00800 - 0xFFC008FF)						n 1 ReTimer 0 P                  */
#define SPOT0 Transmit Configuration 1 Register                 n 1 ReC0060C	/*                  */
#define SPOT0 Transmit Configuration 2 Register                 n 1 ReCONFIG		0                  */
#define SPOORT0 Transmit Clock Divider                           */
#d*/

/* System Interrup0xFFC00634	/* 2imer 3 Counter Register    2 (B2                            */
#define TIMER3_PERIOD		0xFFC00638	/* Timer 300AFiod Register        2                             */
#define TIMER3_WIDTH		0xFFC0063C	/* Timer 3 Width Register 2                      2                                    */
#define 1		0xFFC00A08	/* Asynchration Register     ontrol Register 1  */
#define EBIU_SCOUNTER		0xFFC00644	/* Tnchr 4 Counter Registerontrol Register 1  */
#define EBIU_Sine TIMER4_PERIOD		0xFFCnchr8	/* Timer 4 Periodontrol Register 1  */
#define EBIU_S   */
#define TIMER4_WID2H		0xFFC0064C	/* Timer2                  */
#define SPORT0_RCLKDIV		0x1		0xFFC00A08	/* AsynNFIG		0xFFC00650	/* Ti00A1C	/* SDRAM Status Register                     */
#define TIMER5    NTER		0xFFC00654	/*00A1C	/* SDRAM Status Register                        */
#define TIM    RC			0xFFC00A18	/* SD Timer 5 Period Register                              */
#define T2MER5_WIDTH		0xFFC0065C2             */
#define SPORT0_CHNL			0xFFC0083(below) provided for bacER6_CONFIG		0xFFC00bility */
#define DMA_TCPER			0xFFC0ne EBIU_SDBCTL			0xFFC00A/
#define TIMER6_COUNTbility */
#define DMA_TCPER			0xFFC0         */
#define EBIU_          */
#define Tbility */
#define DMA_TCPER			0xFFC0Period Register         2                    */
2FC00840	/* SPORT0 Multi-Channel Transmit Select Register 0      */
#define              */

#defineTART_ADDR			0xFFC00C000670	/* Timer 7 Configuration Register                       */
#define TIMER7TART_ADDR			0xFFC00C0/* Timer 7 Counter Register                                 define TIMER7_PERIOTART_ADDR			0xFFC00C0r 7 Period Register                              */
#de     TIMER7_WIDTH		0xFFCTART_ADDR			0xFFC00C0h Register                               */

#define TI     NABLE		0xFFC00680	/TART_ADDR			0xFFC00C0er                                        */
#define TI     ISABLE		0xFFC00684	TART_ADDR			0xFFC00C0ster                                       */
#define T     STATUS		0xFFC00688	TART_ADDR			0xFFC00C0ter                                        */

/* Gener     rpose I/O Port F (0TART_ADDR			0xFFC00C0F)												*/
#define PORTFIO					0xFFC00700	/* Port               e SpecifyTART_ADDR			0xFFC00C04                   */
#define PORTFIO_CLEAR			0xFFC007      Port F I/O PeripheTART_ADDR			0xFFC00C04gister               */
#define PORTFIO_SET				0xFFC00       Port F I/O PeriphTART_ADDR			0xFFC00C04ister                 */
#define PORTFIO_TOGGLE			0xFF      	/* Port F I/O PinTART_ADDR			0xFFC00C04                                 */
#define PORTFIO_MA      0xFFC00710	/* PortTART_ADDR			0xFFC00C04ify Interrupt A Register   */
#define PORTFIO_MASKA_CL      xFFC00714	/* Port TART_ADDR			0xFFC00C04errupt A Register                 */
#define PORTFIO_M2SKA_SET		0xFFC00718	/* 2        */
#define SPORT1_RCR2			0xFFC00924	/* SPORT1 Transmit ConfiguratioC00C4ASKA_TOGGLE	0xFFC00Start Address RegisterMA0_CURR_X_COUNT		0xFFC00C30	/* DMA Channel 0 Current XO_MASKB			0xFFC00720	/*Start Address Register */
#define DMA0_CURR_Y_COUNT		0xFFC00C38	/* DMA ChanneB_CLEAR		0xFFC00724	/* Start Address Register              */

#define DMA1_NEXT_DESC_PTR		0xFFC00C4IO_MASKB_SET		0xFFC0072Start Address Registergister               */
#define DMA1_START_ADDR			0xFFC00C4FIO_MASKB_TOGGLE	0xStart Address Register Mask Toggle Enable Interrupt B Register   */
#define      IO_DIR				0xFFC0073Start Address Registerion Register                                                */
#define PORTFIOStart Address Register Port F I/O Source Polarity Register                                  */
#defiStart Address RegisterC00738	/* Port F I/O Source Sensitivity Register      C00C44                  Start Address Register H			0xFFC0073C	/* Port F I/O Set on BOTH Edges RegistC00C44                  Start Address Register RTFIO_INEN			0xFFC00740	/* Port F I/O Input Enable ReC00C44                  Start Address Register /* SPORT0 Controller		(0xFFC00800 - 0xFFC008FF)						C00C44
#define SPORT0_TCStart Address Register T0 Transmit Configuration 1 Register                 C00C44
#define SPORT0_TCStart Address Register T0 Transmit Configuration 2 Register                 C00C44
#define SPORT0_TCStart Address Register ORT0 Transmit Clock Divider                           3            */
#define SPOR3 (B3                            */
#define TIMER3_PERIOD		0xFFC00638	/* Timer 3iste                    3                             */
#define TIMER3_WIDTH		0xFFC0063C	/* Timer 3 Width Register 3ister                 3ntrol Register 1  */
#define EBIU_SDGCTL			0xFFC00A10	/* SDRAM Glob    C00818	/* SPORT0 RX                 */
#define DMA2_X_MCOUNTER		0xFFC00644	/* T        */
#define SPOR                 */
#define DMA2_X_Mine TIMER4_PERIOD		0xFFC    8	/* Timer 4 Period                 */
#define DMA2_X_M   */
#define TIMER4_WID3iguration 2 Register  30A1C	/* SDRAM Status Register                                      egisT0 Receive Clock Di                              */
#d                */
#define TIMER5egisRFSDIV		0xFFC0082C	                              */
#d                   */
#define TIMegis              */
#def Timer 5 Period Register                              */
#define T3                      3ility */
#define DMA_TCPER			0xFFC00B0C	/* Traffic Control Periods     ent Channel RegisteA2_PERIPHERAL_MAP		0xFFC00CAC	/* DMA        */
#define DMA2_Y1		0xFFC00838	/* SPORTA2_PERIPHERAL_MAP		0xFFC00CAC	/* DMA                                   */
#define TA2_PERIPHERAL_MAP		0xFFC00CAC	/* DMAPeriod Register         3define SPORT0_MTCS0		0x3FC00840	/* SPORT0 Multi-Channel Transmit Select Register 0      */
#define      0_MTCS1		0xFFC00844MA3_NEXT_DESC_PTR		0x00670	/* Timer 7 Configuration Register                     	0xFFC00848	/* SPORMA3_NEXT_DESC_PTR		0x/* Timer 7 Counter Register                                 84C	/* SPORT0 MultiMA3_NEXT_DESC_PTR		0xr 7 Period Register                              */
#de     PORT0 Multi-ChannelMA3_NEXT_DESC_PTR		0xh Register                               */

#define TI     lti-Channel ReceiveMA3_NEXT_DESC_PTR		0xer                                        */
#define TI     nel Receive Select MA3_NEXT_DESC_PTR		0xster                                       */
#define T     ive Select RegisterMA3_NEXT_DESC_PTR		0xter                                        */

/* Gener     SPORT1_TCR1			0xFFCMA3_NEXT_DESC_PTR		0xF)												*/
#define PORTFIO					0xFFC00700	/* Port      PORT1_TCR2			0xFFCMA3_NEXT_DESC_PTR		0xF                   */
#define PORTFIO_CLEAR			0xFFC007      PORT1_TCLKDIV		0xFMA3_NEXT_DESC_PTR		0xFgister               */
#define PORTFIO_SET				0xFFC00      
#define SPORT1_TFMA3_NEXT_DESC_PTR		0xFister                 */
#define PORTFIO_TOGGLE			0xFF      /
#define SPORT1_TMA3_NEXT_DESC_PTR		0xF                                 */
#define PORTFIO_MA                   */
#dMA3_NEXT_DESC_PTR		0xFify Interrupt A Register   */
#define PORTFIO_MASKA_CL                        MA3_NEXT_DESC_PTR		0xFerrupt A Register                 */
#define PORTFIO_M3 1 Register            3        */
#define SPORT1_RCR2			0xFFC00924	/* SPORT1 Transmit ConfiguratioR		0xegister            nnel 4 Next DescriptorPERIPHERAL_MAP		0xFFC00CEC	/* DMA Channel 3 Peripheral r                      nnel 4 Next Descriptor/
#define DMA3_CURR_X_COUNT		0xFFC00CF0	/* DMA Channel me Sync Divider        nnel 4 Next Descriptor            */
#define DMA3_CURR_Y_COUNT		0xFFC00CF8	/*ster                   nnel 4 Next Descriptor                         */

#define DMA4_NEXT_DESC_PTR		0x Current Channel Rennel 4 Next Descriptor Mask Toggle Enable Interrupt B Register   */
#define      * SPORT1 Multi-Channnel 4 Next Descriptorion Register                                                Multi-Channel Confnnel 4 Next Descriptor Port F I/O Source Polarity Register                       hannel Transmit Selnnel 4 Next DescriptorC00738	/* Port F I/O Source Sensitivity Register      R		0xFansmit Select Reginnel 4 Next Descriptor H			0xFFC0073C	/* Port F I/O Set on BOTH Edges RegistR		0xFelect Register 2  nnel 4 Next Descriptor RTFIO_INEN			0xFFC00740	/* Port F I/O Input Enable ReR		0xFgister 3      */
#nnel 4 Next Descriptor /* SPORT0 Controller		(0xFFC00800 - 0xFFC008FF)						R		0xF      */
#define Snnel 4 Next Descriptor T0 Transmit Configuration 1 Register                 R		0xF
#define SPORT1_MRnnel 4 Next Descriptor T0 Transmit Configuration 2 Register                 R		0xF SPORT1_MRCS3		0xFnnel 4 Next Descriptor -Channel Receive Select Register 3       */

/*                         SDRAM Counter Register                                                  */
#defiSD TIMER1_PERIOD		0xFFC00618	              CTLE3 Counter Registe       ster  Signal             */
#define RTC_FAST			0x  */
#define TIMER3_PERIOD		0xFFC00638	/* Timer CLControl Register   ster   ASdefinncefine ster               */
#define PORTFIO_SE                          */
#define  4 Counter RegisteFC00D50	/* DMA Chann/* Timer 7 Counter Register                			0xFFC00810	/* SPORT0 TX Data RPASRTIMER3_CONFMRCS3		0x*/

4xFFC00D#definRefreshed InR			f-el 5 Y                               */
#define Dfine TIMER2_C	/* Time/* DMA Chann0     1 Ar0xFF 5 Y Count Register                                 *		0xFFC00650	/* TRT0_D/* DMA Cha 0 Innel 5 Y Count Register                           #define CHIPIASR5_WIDTH		0xFFC0065ster  tRASTH		0xFFC0066C	/* Timer 6 Width Register      nalog Devices Inc.
 *
 * Licensed under the ADI BointER6_CONFIG		0xFFC0#define DMA5_el 5 X Count Register                                                                          *define TIMER6_COUN#define DMA5_nel 5 X Modify Register                                                                        *8	/* Timer tus Reg#define DMA5_r 7 Period Register                        rent Address Register                               *TIMER7_WIDT1   */
#define DMA5_h Register                               */rent Address Register                               *NABLE		0xFF1A5_IRQ_STATUS			0xFer                                        *rent Address Register                               *ISABLE		0xF1AP		0xFFC00D6C	/* Dster                                       rent Address Register                               *STATUS		0xFisters #define DMA5_ter                                        rent Address Register                               *rpose I/O P2   */
#define DMA5_F)												*/
#define PORTFIO					0xFFC0rent Address Register                               *	/*      */
#A5_IRQ_STATUS			0xF	/* DMA Channel 3 Current Address Register gister                                              */           2AP		0xFFC00D6C	/* D		0xFFC00CE8	/* DMA Channel 3 Interrupt/Stagister                                              */ation Regise TIMER#define DMA5_CFC00D68	/* DMA Channel 5 Interrupt/Status Register                                            */  */
#define   */
#define DMA5_CMA Channel 5 Peripheral Map Register                                */
#define                */8	/* Timer 36_Y_COUNT			0xFFC00Durrent X Count Register                               */
#define DMA5_CURR_Y_CO               */TIMER7_WIDT3		0xFFC00D9C	/* DMA  Register                               */

#define DMA6_NEXT_DESC_PTR		0xFFC00             P            R Regis#define PA5_CURR_ADDR			0xFFC00D64	/* DMA Channel 5 Current Address Register                r               ation Regi********
#define DMAFC00D68	/* DMA Channel 5 Interrupt/Status Register                              r                4 Counter1     */
#define DMAMA Channel 5 Peripheral Map Register                                */
#define Dr               8	/* Timer			*/
#d
#define DMAurrent X Count Register                               */
#define DMA5_CURR_Y_COUr               TIMER7_WID2     */
#define DMA Register                               */

#define DMA6_NEXT_DESC_PTR		0xFFC00Dr               NABLE		0xF        
#define DMAister               */
#define DMA6_START_ADDR			0xFFC00D84	/* DMA Channel 6 Star               ISABLE		0x3     */
#define DMA     */
#define DMA6_CONFIG				0xFFC00D88	/* DMA Channel 6 Configuration Register             C     
#define PORTFI#define CDA5_CURR_ADDR			0xFFC00D64	/* DMA Channel 5 Current Address Register                             C/
#de     */
#define             FC00D68	/* DMA Channel 5 Interrupt/Status Register                              */
#define DMC          */                     MA Channel 5 Peripheral Map Register                                */
#define DMA5_CURR_X_COCister                            urrent X Count Register                               */
#define DMA5_CURR_Y_COUNT		0xFFC00D7CStatu                             Register                               */

#define DMA6_NEXT_DESC_PTR		0xFFC00D80	/* DMA ChaC	/* Sr      _MAP		0xFFC00DEC	/* Dister               */
#define DMA6_START_ADDR			0xFFC00D84	/* DMA Channel 6 Start Address ReCReceir                                */
#define DMA6_CONFIG				0xFFC00D88	/* DMA Channel 6 Configuration Register           WR           SPORT0 R#defineWRDMA6_CURR_Y_COUNT		0xFFC00DB8	/* DMA Channel 6 Current Y Count Register                         WR    */

# Register             PTR		0xFFC00DC0	/* DMA Channel 7 Next Descriptor Pointer Register               */
#define DMWR_START_AD DMA7       */
#define Channel 7 Start Address Register                                 */
#define DMA7_CONFIG				PUPSD0 Receive Clock DP    -Up */
rtART0_GC(15 SCLK Cter   RT0_G          */
#define PLL_ST            SM                  _Y_COUNT		equence (     RT0_THR		Before/AfterRegi 5 Y                     S        nel Regist        */
#define DMA8_Yon Next   */
# TIMER6                           */
#defS0xFFC Status               IG				0egister          define DMA7_Y_MODIFY			0xFFC00DDC	/* DMA Channel 7 Y Modify RegisEBU    TCS1		0xFFC0084       Exd bit dBuffer CopTim                               *                                */
#BBRW4C	/* SPORT0 Mult       Fast Back-To-
#deFC0084Te TIMERDR			0xFFC00E24	/* DMA Channel 8 Current Address ReMRod Regir               nded
#defiRT0_THR			)							*/
#define SWRST				0xFFC00100	/* Software Reset Regi             ****ster           Temp-SK		ensat******ister     Value (85/45* Deg C          */   */
#definDDBGnc Divider       Tristfinester   ontrols DuefineBus Gran	0xFFC00004	/* PLL Divide Register                SD/
#dess Register            t Address Regter egister           ster           * DMAMA8_CURR_ADDR			0xFFC00E24	/* DMA Channel 8 Current Address RegSZ_1NABLE		0xF      */
#DDR			0xFFC00E       16MB* DMA Channel 9 Start Address Register                        3er Registgisterine DMA9_CONFIG				0xFFC00328	/* DMA Channel 9 Configuration Register                       6PERIPHERAegisteine DMA9_CONFIG				0xFFC00648	/* DMA Channel 9 Configuration Register                       12ud raIDTH		0xfine DMA9_CONFIG				0xFFC00E288	/* DMA Channel 9 Configuration Register                      25* SPIster                   */
#define DM2548	/ster          SZ_5DMA1_CURRA                           */
#d512MBister          CAW                             */
#deColumn AddrMER6Width DMA6    rrent Address Reg    rpose I/5_Y_MODIFY		   */
#define DMA9_CURR_DESC_PTR		0xF9C00E60	/* DMA Channel 9 Cu8	/* DMAcaler Er Pointer Register    */
#define DMA9_CURR10C00E60	/FFC00E64	/* DMA Ch TIMER20654	/nt Address Register                         1     */
#deChannel 9 N_ICTL			0xFFC00304	/* RT* DMA ChanneDCI   */
#define             ler Id                              */
#A9_PERIPHERALSRA        */
#define fine DMA8_CUR       Register                        PU      */
         */_Y_COUNT	T		0xFFC00E70	/* DMA Chann0xFFC00E20	/* DMA ChanneDR8 CurreT			0xFFC00D5Will          O*/
#defA8_CURR_D_COUNT		0xFFCEAPLL Prescaler Enr PoinABEnable RError */
#define RTC_ISTAT		th Register G_ICT0 ReceiveT0_TC

#define                  */
#define RTC_FAST			0xA Channel 5 Next Descriptor Pointer Rer Regi                 */
#define DMA5_START_ADDR			0xFFC00D  */
DMAx_CONFIG, MDMA_yyONFIG		dog Control Regist           *MAod Register    ress                              */
#define TIMER3_PERIOD		0xFFC00638	/* Timer WN          */
#d        Direc     (W/R*          */
#define PLL_STAT			090	/* DMA Channel 10 X CoD0xFFaud raegister   ansferor Con                      */
#define SPI_SHADA10_X_MODIFY			0xFFC00E94	/* DMA    MER2_PERIO Modify Register     1       */
#define SPI_RDBR			0xFFC10_X_MODIFY			0xFFC00E94	/* DMA     Register      fy Register     3e SPI_CTL				0xFFC00500	/* SPI Con                        *MA2   */
#dor the          2D/1D                    */
#define DMA10_X_MODIFY		0040C	/* Line ControSTARt Descriptor P    */
#de CleaAssignment Register 1                      */
#define*****************
** S_SEL              */
#and brupte DMA8_IIV				0xFFC00004	/* PLL Divide Registerer            od RegistRT0_SC  */
#define DMne DMA8_CURR_X_COUNT		0xFFC00E30	/* DMA Channel 8 Currer the ADI N/* DMA             
#defDescriptESC_      0 (Stop/Autob/
#de     DMA Channel 10 Pe TIMERtus RegRegister               Controller			(0xFFC00500 - 0xFFC00xFFC00EAC	/* DMA Channel 10 Pe DMA10isters Register                Channel 10 Y Modify Register                  DMA Channel 10 Pe	/* Poe TIMERRegister               /
#define SPI_FLG				0xFFC00504	/*xFFC00EAC	/* DMA Channel 10 Pe0xFFC0*******Register                      */
#define SPI_STAT			0xFFCxFFC00EAC	/* DMA Channel 10 PexFFC00H		0xFFRegister                              */
#define SPI_TDBRxFFC00EAC	/* DMA Channel 10 Pe0xFFC0C00680	Register               t Register                                     DMA Channel 10 PeceiveFFC00684Register                      */
#define SPI_BAUD			0xFFCxFFC00EAC	/* DMA Channel 10 Pe ChannR RegisRegister                                                       */
#de DMA Channel 10 Pe	/* SPort F (Register                                        */

/* TIxFFC00EAC	/* DMA Channel 10 P	        00ED8	/* DMA Channel 11 Y Count             DMAFLOWDMA11_Y_MODI Data BuF                          egis_STOP             topefine                */
#AUTOine SIC_ISR	        */SC_PTR		0xFFC00EE0	/* DMA RRAY            ter        ArraySC_PTR		0xFFC00EE0	/* DMASMIMER3_C TransmiSmallefinelCURR_ADDR			ListSC_PTR		0xFFC00EE0	/* DMALARG      Data BuLargtus Re                           DMA10_COPERIPHERAL_MAP	0xFFC00E8ister         dog Control Re   */
#defin                             Type Indica    (0xFFC0/Peripheral                    MAP RegiG		0xFFCer         MappeRIPHEThis                                                MAP_PPI                      */
PPI PoxFFCMA */
#define TIMER1_WIDTH		0xFFC0061C	/* Timer 1 Width Register              EMACRXine SIC_ISR	     */

#defEthernetter      A11_CURR_Y_COUNT		0xFFC00EF8	/* DMA Channel 11 t Register          T     			*/
#d    */

#define MDMA_D GPL-2 (oA11_CURR_Y_COUNT		0xFFC00EF8	/* DMA Channel 11t Register           0RX                        ess ReD0_NEXT_DESC_PTR	0xFFC00F00	/* MemDMA Stream 0 Destinatim 0 Destination Start Address ReTiste                            DMA_D0_START_ADDR		0xFFC00F04	/* MemDMA Stream 0 Destinm 0 Destination Start Address R1gisteregister                  FFC0
#define MDMA_D0_CONFIG			0xFFC00F08	/* MemDMA Stream 0 Destination Configuration Re1ister Transmi                    /
#define MDMA_D0_X_COUNT			0xFFC00F10	/* MemDMA Stream 0 Destination X Count Regis      Data Bu               fine DMA11_CURR_Y_COUNT		0xFFC00EF8	/* DMA Channel 11 Current Y Count Register        UA Registete RegisY Modify Regiam 0 ine DM       */
#define MDMA_D0_X_MODIFY		0xFFC00F14	/* MemC	/* MemDMA Stream 0 isterPI_RDBR  Modify Register        DMA_D0_START_ADDR		0xFFC00F04	/* MemDMA Stream 0 DestRegister        am 0      FFC00600 Modify Register 1                                      */
#define MDMA_D0_CURR_DESC_PTDR		0xFFC00istern Regist Stream 0 Destination Cuent Descriptor Pointer Register      */
#define MDMA_D0_ DMA10_COIRQC_ICTUS	0xFFC00E8ine MDMA_Ddog Control               _D                    omple     #define DM*/
#define RemDMA Stream 0 ER         */
#dA11_T_DESCster                                  *FETCHount RegisterA11_ter        Fetchripheral Ma                    _RUd RegisteC_IMAMA Channel 1Runn Cop            */
#        t Descriptor   PARALLEL ister      INTERFACE (PPI)gister     */
#define DMA#defONFInterdog Control RegisteC	/* MemDMA ORTus Registen Peridefine DMne DMA8_CURR_X_COUNT		0xFFC00E30	/* DMA Chan0 Source Next DeDIfine UA */
#ddefine DMA         DMA Channel 8 Interrupt/Status RegistXFR_P	0xFFSIC_IAR0		defi Modify R1 PerMDMA_D0_Y_MODIFY		0xFFC00F1C	/* MemDMA t DeCFGATUS			0xFFCdefine DMConfigurter                                 */
#LD                 defi             * Clock and System Control	(    */
#definCKus Register     defin       #define DMA8_CURR_ADDR			0xFFC00E24	/* DM                      tus Regdefi32-b_START_ne DMA8_CURR_X_COUNT		0xFFC00E* DMA ChanneKIPus Regististers defiSkip Elem****ne DMA8_CURR_X_COUNT		0xFFC0                 O4C	/* SPO              ven/Odd       *RR_DESC_PTR	           *LENGTH_S0_CONFI0x        defi */
#trol Re                               #define MDMAxFFC00E60	/*     */
#define MDMA_S0_X_MODIFY		0xF/
#definR Register Stream 0 S       */
dify Register                         egister                     *ster     dify Register                         er Reg			0xFFC              *2Descriptor Pointer Register           */
#defi/
#def			*/
#d              *3Descriptor Pointer Register           */
#defiPERIPHount Reg              *4Descriptor Pointer Register           */
#defi		0xFF                      *5Descriptor Pointer Register           */
#defi* DMA                       *6Descriptor Pointer Register           */POLCnc Divide     */
   */
PolariFFC00400	/* Transmit Holding               8 Currte Regisdefis */
#inclu0_CURR_X_COUNT	0xFFC00F70	/* M     */R	0xAP	0xFFC00F2C	/* M Stream 0 Source NeFL   *          *      ream 0 Destinatio                                      */
#Tfine MDMA_R Regiss */
#TrDMA8T_DESCart Address Register                  OVnt Regi********              T_DESC_PTR	0xFFC00F80	/* MemDMAer the ADI UNefinster                 runer Register         */
#define MDMA_D1_STARERR_DEt Desc        T_DESCDetec
#derce Current Y Count Register  s Register     NCO*****te RegisT_DESC    Cor    fine MDMA_D1_CONFIG	n Current Y Count ReCount RegiTWO-WIRE          */TW#define M4	/* DMA Channel 10 Star  */
TWI_CLKDIV        (Use: *p Register  =    LOW(x)|CLKHI(y);  )              	         	/* Receiv Bufferer  ods MDMA_SIs H    ****                      */
#define/
#defne UyxFFC0F)<<0x8MA Stream 1 D00E1C	 New MDMA_Sodify Register           */ RegPRESCALFC00 Control Register           	tion Y C     7Fs RegCLKstrea and bit de */
ReferMA8_Y_10MHz                 RegENAegister     TWIine DMA8_CURR_X_COUNT		0xFFC00E30	/* DMA Channel 8 Current X Coidth Register         SCCBe registers 	0xFASK			tibilR_X_ne DMA8_CURR_X_COUNT		0xFFC00E30	/* DMA Chm 1 DestinaSLAVE_CTR Descriptor Pointer ReRR_DESC_PTR	od Register    Sla01FF)							*/
#define SWRST				0xFFC00100	/* Software Reset Register   CURR_DESC_PTR	ADD_LENtus RegistertinatRR_DESC_ne MDMA_ster                         */
#define MDMA_D1_IRQ_STATUS	TDVAER3_CONFegistetinatDMA_D0_STA    Vali2_CONFIG		0xFFC00620	/* Timer 2 Confi           NAKime Register NAK/ACK* Gener/
#deAgisteclus    Of/
#define            God Registor the    */l CsterAdrDESC_Matchine P                     *r      */
#define_ICTL			0xFFC00304	/* RTC Interrupt	S0F44	/* Me 1 Destinat Modify R            GPL-2 (/       *)	/* MemDMA SGCIMER3_CONF      1 Destinatiorce Current Y Count Register                   DMA Stream 1 DestinaMASTERe MDMA_D1_CURR_ADDR		0xF	0xFFC00FB8T0_MSemDMA StreaMaTHR		s Register                                e MDMA_D1_CURR_DESC_PTRM	0xFFC00FA8	/* MemTART_ADream 1 Destination Interrupt/Status Register           Register   0F44	/* Meol RegART_ADDMA_D0_STA          RX/TX             	0xFFC00FB8FA				0xFUNT	0xFUs     */     A10_IRQ_pecefine RTC_ISTAT			0xFFC00308     */
#defifine DMAor theIssueRR_DESCond       Register                         */
#define MDMA_DRter Regel 9 CurreRepea     rt ESC_Pop*efinEndCURR_X_COUNT	A_D1_IRQ_STATUSDCN0FD4	3F6_COUN00FACBytes	0xFFer                          */
#define MDMA_D1_IRQ_STATUS	DAtinat          Serial Statusverriefine DMA8_CURR_ADDR			0xFFC00E24	/* DMA ChRR_DESC_PTR	0LSource          egiste   */
                                                escriptor Pointer            */
#define DMA9_PERIPHE	MPROrce ConA_S1_START_AD  */
#defIn ProgDESC_er                         */
#define MDMA_DLOter FFC00F/

#defLost Arbit       ipheral MapXcripAborted9C	/* MemDMA StrAter  MER2_PERIORR_DESC_    AcknowledgR2_CONFIG		0xFFC00620	/* Timer 2 Configuration             am 1 SourcT	0xFF    ress Register                                      */
#defineine MDMA_S1_IRBUFRDine nse or theine DMAC0084T_DESC_PTR	0xFFC00F80	/* MemDMA Sus Register                           WR        ptor PoiscripRAL_MA_PERIPHERAL_MAP	0xFFC00FEC	/* MemDMA Stream 1 Source PermDMA Stream 1 mDMA F)								Register    Sensfine DMA8_CURR_ADDR			0xFFC00E24	/* DMA Channel 8 Cur */
#define MDream 1 SoA5_IRQ_0xFFC00FDC	/nt Register                                      */
#def             SBUSefine      */
us/* PyNEXT_DESC_PTR	0xFFC00FC0	/* MemDMA Stream 1 Source Next Dtream 1 DestinaINT_SRC     CONTROL	ENAB Count Registe	0xFFC00FB8	INlicense  Stream 1 DestinationIniti/
#de1_CURR_DESC_PTR	0OMine DMA1* MemDMA St  */
#defal Map DIV		0xFFC00828	/	Sine MA_D1_PERIPHERAL_MAP	0cripT_DESC_PTR	0xF			0xFFC0100ch Count R		0xFFtinat         1 Source Start Address Register   egister   0xFFC00DESC_PTR	0xFFC0fine PPI_COUN       */
#de	/* PPI ptor P01010	/* PPI FraT_DESC_PTR	0x			0xFFC010XMTSERVm 1 Source C              ServicA_S0_CONFX_MODIFY		0xCV/
#define RT0_SC             01400
#define T                e MDMA_D1_CURR_ADDR		0x14FF)								*FLUSH  */
#defin GPL-2 (or      Flu                          WI_CLKDIV			0x404	/* TWI on Regis/* Intr                                  014FF)								*INTFC00FA8	/98	/* DMA gister     #define DMestination WI_CLKDIV			0x           *C_IMAL		0xFFC01408	/TAT		0xFFC0140C	/* Sl                 fine MDMA_D1_CURR_Y_COUNT	0xFFC00FB8		*/ext D          EGBASE			0xFFC0ster              */
#define DMA10_START_ADDR		0xFter               _EMPTY                    */
#              	0xFFC00400	/* Transmit Holding registe MDMA_D1_CURR_DESC_PTR    HALF  */
#defin*/
#define TWI_MASTER_STAT	Has 1     IPHERAL_MAP		0xFFC00E2C	               FULL           */
#define TWI_MASTER_STAT	     (2          TIMER4_             		0xF          AR0		al Clock Divide TWI_MASTER_CTL		0xFFC01414	/* Master Mode Control Relave Mode Statu                 */
#define TW             	0xFFC01418	/* Master Mode Status Register        ister              */
#def     IFO_CTL		0xFFC01428	/* FIF/* Master ModeC0084#define MDMA_S1_X_MODIFY		0xCV        */
AR0		IFO_CTL		0xFFC01428	/* FIF	/* TWI InterruptC008                     t Y Count RegiCounter RegAREA NETWORK (CAN* MemDMA Stream 1 Destin  */
CANxFFC00F40	/* MemDMA Stream			0xFFC010000E78	/* D 1 DesoftwaDMA C    trol Register                                        */
#DN    MA_D0_CURR_e00
#dNeCOUNT		ER1_WIDTH		0xFFC0061C	/* Timer 1 Width Register          BMA Strerce Curruto-* PaOntination Current Descriptor Pointer Regist MemDMA StreXPRIannelegister  X PrioRR_X_(e Specif/Mailbox                			0xFFC010WB      */or theWakeY CountCAN/* Pa     Destination C			0xFFC0100Me MDMA_Dcaler EleeESC_PTR****esve Data Single Byte Register       *_D1_X_MODIFY       xFFC0065FC01Sus****              */
#define PORTGIO_SET				0x_D1_X_MODIFYCI/O PeriRT0_SCFC01ster                       */
#define POR     */Byteefine MDMA_S0_CURR_Y_COU        */
#t Descri ControX WarMA StFla_IRQ_STATUS			0xFFC00E28	/* DMA Channel        */
#e MDMA_D0_CURROG_C* Port G I/O Mask State Specify Interrupt A Register   */
#dE */
#dR2_PERIOD_DESCPass001Fve Data Double Byte Register   errupt A RegPort G (0xIG		0x_DESC 0xFFffefine PORTGIO_MASKA_SET		0xFFC01518	/* PortSM      */r Register        Register  t Register             */
#define CS      */urce Currupt Set Re	/* Port G I/O Mask Toggle EnablRTGIO_TOGGLE	      */150C	/*t G I/O Pin State 	/* Port G I/O Mas     */
#defBPT/* PP1/* Port        Poind begister                         */
#define MDMA_DTR	0xFFC         GPL-2 (ove Data Double Byte Register       */

/* GeX_MODIFY		0xE      te RegisL		0xFFCve Data Double Byte Register       */

/* Gen           CLOCKount Register              Rister  3F     Bit-RfinePre-Scanel ort G I/O MaTIMINDMA Channel 10 Con       */
#dSEG TIMER2_     D1_Y_Seg   */Controller			(0xFFC00500 - 0xFFCegister         DMA10_7r the             e SPI_CTL				0xFFC00500	/* SPI C_IRQ_STATUS			0xFFC01A5_IRQ_ampfine                                     */
#define MDMA_S1_CJ       FC00DA0	ynchroniz      Jump_PTR		00xFFC01530	/*DEBU G I/O Direction Register    Der     ion Perip TIMER/* Pole IntCound b              	DRMAP		0xFF_CURR_Register   RX Inpuve Data Single Byt            hannnt X Count RRegister   TX Out1540	/* Port G I/O             I        0xFFC00Register   and bit dLoopress Register    efine PORTGIO_r   */00 -	/* Port G Ine DMA8     */
#defisteTwo-Wire Int      ad/
#detination Current Descri_D1_X_MODIFYDxFFC00te RegisFC01Debunt RegisMASKB_TOGGLE	0xFFC0152C	/* Port G I/O MasECount Register   X_MODIFY		0xXE       00/
#defxFFC001FF            IO					0xFFC01500	/* PORTHIO_F/* Port GPL-2 (o8	/* Port H I/O Peri           INTRG I/O Direction Register    MBRIRQe MDMA_S1_STAt G I/L		0xFFC#define Drt H I/O Pin StaF	n State /* legacy_MASKB_CLEAR		Tate Toggl       t G I/ GPL-2 (o                      *T
#defi0	/* RTHIO_MASKA			0xFFC01Gate er         Globa_NEXefine DM       */
#define MDMA_D1_CURR_DESC_PTR	MACTATUS		0xFFC0	0xFFC0151C	/* Port G I/O Mask Toggle_D1_X_MODIFYANointerripheral IntTXterruR_Y_CO                      */
#define MDMA_DCAN      C0150C	/* PoRister                  */
#define PORTHIO_M        MBxx_ID1       */
#definefine TIMER3_COUN0 Destination      FF/
#def */
#diltdefineC     Ift Regist) (ID0          */
#deerrupt A RegXTID_LOate Specify       Map Regof          I****ifiID R PORTHIR		0xFFC01724	/* HI           Uppr    e Interrupt B Register                           ASEIDFC0152Fgisterasegister      t Address Register                         */
#define MDMA_D1_IRQ_STATUSI PORTHI			*/
#dister           _D1_Cion Interrupt/Status Register                        */
#deRxFFCe PORTGIO_MRemotment Reg GPL-2 s_D1_C        */
#define PORTGIO_er           PORTHIO_CLEAR	 TIMptaA8_Yunt           */
#define DMA10_PERIPHERAL_MAP	0xFFC00EAC	/gister   */
#defTIMnterM#define DMA1			0xFFC00010VState SpecifyG I/stamFFC017FFter   */
#def       C00F2C	/* MemDMA Stream                           giste               AMxxH PORTHIO_    ter      */
#define TWI_RCV_D DF	0xFFC Specify Interr    isterr   */
#define                   */
#define PR		0xFFC01724	/* Port H I/O Mask Disable Interrupt B Register       Register          */
#define PORTHIO_MASKB_SET		0xFFC01728	/* Port H I/O Mask Enaber                        ister                  */
#define PORTHIO_MASKB_TOGGLE	0xFFC0172C	/* Port H I/O Mask Toggle Enabh Register           t B Rgister   *olarity RegisterIDPORTHIO_DIRne DMA8_CURR_X_COUNT		0xFFC00E30            M   */
             ister40	/* Port ne DMA8_CURR_X_COUNT		0xFFC00E30	/* DMA Channel             DTime Rte Regisrrupt FFC0                  */
#define UART1_DLH			0xFFC02004	/* Divisorter   */
     e Enable Interrupt B ReMC
#define ne DMA5_CONF Mask St			0xFFC00424	 UART1_LCR			         e TIMER2_COU Mask StController			( UART1_LCR			er RegistPERIOD		0xFF Mask Ste SPI_CTL				0 UART1_LCR			/
#defineIG		0xFFC006 Mask St/
#define SPI_ UART1_LCR			PERIPHERor theFFC02014	/* Lin       */
#def UART1_LCR					0xFFC0ptor PFFC02014	/* Lin               UART1_LCR			* DMA Ch      FFC02014	/* Lin       */
#def UART1_LCR			l 8 NextRT0_SCFFC02014	/* Lin       */
#def UART1_LCR			STATUS	tus RegFFC02014	/* Lin               UART1_LCR			rpose Iisters FFC02014	/* Lin                                  am 0 So#define UART1_MC			0x               TIMERR Regis#define UART1_MCContr               DMA1********#define UART1_MCe SPI                   			*/
#d#define UART1_MC/
#de              ter          #define UART1_MC                   atus ination Cdefine UART1_MC     cation Registhdog Control Reg              0xFFC00EC	/* Line Control Rent Reg              ister     */
#define UART1_MC                    Channelister               1                   FC00EDSR			0xFFC02014	/* Lin1     0	/* Modem Con                      */
#de2boxes 0-15								2 TIMER2m Status Register    2	0xFFC02A00	/* Mai2 DMA10_ine UART1_SCR			0xFFC2                  2	/* Por                     2AN_MD1				0xFFC02A20xFFC0C02024	/* Global Contr2                  2xFFC00       */

/* CAN Cont2#define            0xFFC0									*/
/* For Mai2                 *2ister 		*/
#define CAN_MC1		2C	/* Transmit Requ      onfig reg 1            2                  2	/* S            */
#define 210	/* Transmit Ack3     Mailbox direction reg 13boxes 0-15								3                          */3	0xFFC02ter   */
D*/

/* System InterMemDMA Stream0xFFC0200C	/* Line Control RegiFoMDMA	0xFFC0FC8	/* MemDMA Stream           */
#define UART1_MCR			0xFFC02A2C	/* Remote Frame Hanntrol Register                 			0xFFC02A2C	/* Remote Frame HanUART1_LSR			0xFFC02014	/* Line 			0xFFC02A2C	/* Remote Frame Han                       */
#defi			0xFFC02A2C	/* Remote Frame Han	/* Modem Status Register      			0xFFC02A2C	/* Remote Frame Han */
#define UART1_SCR			0xFFC02			0xFFC02A2C	/* Remote Frame Haner                             			0xFFC02A2C	/* Remote Frame Han			0xFFC02024	/* Global Control			0xFFC02A2C	/* Remote Frame Han              */

/* CAN Contro			0xFFC02A2C	/* Remote Frame Hand2FFF)										*/
/* For Mailbo			0xFFC02A2C	/* Reote Frame Hand								*/
#define CAN_MC1				0	/* Transmit Acknowledge reg 2   box config reg 1               	/* Transmit Acknowledge reg 2                    */
#define CAN	/* Transmit Acknowledge reg 2   4	/* Mailbox direction reg 1   	/* Transmit Acknowledge reg 2                             */
#d	/* Transmit Acknowled Interrupthdog Control Registote Frame Handt Set reg 1                    	/* Transmit Acknowledge reg 2   
#define CAN_TRR1			0xFFC02A0C		/* Transmit Acknowledge reg 2   st Reset reg 1                 	/* Transmit Acknowledge reg 2   /
#define CAN_TA1				0xFFC02A10	/* Transmit Acknowledge reg 2  nowledge reg 1                                              */
#d*/
#define CAN_AA1				0xFFC02A1                            */
#drt Acknowledge reg 1                                       */
#d */
#define CAN_RMP1			0xFFC02A                            */
#dsage Pending reg 1                                         */
#d  */
#define CAN_RML1			0xFFC02                            */
#dssage Lost reg 1                                           */
#d   */
#define CAN_MBTIF1			0xFF                            */
#d Transmit Interrupt Flag reg 1                             */
#d
#define CAN_MBRIF1			0xFFC02A2	/* Transmit Acknowledge reg 2  eive  Interrupt Flag reg 1                                       ine CAN_MBIM1			0xFFC02A28	/* M2                             RMP Mask reg 1                   RMP        0 - 0xX Message PendA Streontrol Register l Interrupt St TIMER2_GIO_MASK                          *Controll Interrupt St DMA10_Y      Global Interrupt Mask Registee SPI_Cl Interrupt St	/* Port      Global Interrupt Mask Registe/
#defil Interrupt St0xFFC00FFC0011Global Interrupt Mask Registe       l Interrupt StxFFC007 MemDMAGlobal Interrupt Mask Registe       l Interrupt St0xFFC00                 */
#define CAN_INTR       l Interrupt St
#defin* SeriaGlobal Interrupt Mask Registe       l Interrupt St Chann         *                          *       l Interrupt StFC00ED                                    *       ne CAN_GIM				02FFF)								 Global Interrupt Mask Register			0xFne CAN_GIM				0								*/
#d Global Interrupt Mask RegisterControne CAN_GIM				0box config re Global Interrupt Mask Registere SPI_ne CAN_GIM				0              Global Interrupt Mask Register/
#defne CAN_GIM				0ter           Global Interrupt Mask Register      ne CAN_GIM				0              Global Interrupt Mask Register      ne CAN_GIS				    */
#define CAN_MBTIF2			0x			0t Set reg 1   Global Interrupt Mask Register      iversal Counter
#define CAN_ Global Interrupt Mask Register      iversal Counterst Reset reg  Global Interrupt Mask Register      iversal Counter/
#define CAN Global Interrupt Mask Register                     owledge reg 1
#define CAN_GIF				0xFFC02A9C	                                  
#define CAN_GIF				0xFFC02A9C	REG			0xFFC02AC0	/* * Overwrite Pr
#define CAN_GIF				0xFFC02A9C	*/
#define CAN_UCCNTuration, Contr
#define CAN_GIF				0xFFC02A9C	                    LOCK			0xFFC02
#define CAN_GIF				0xFFC02A9C	FFC02AC8	/* Universa          */
#
#define CAN_GIF				0xFFC02A9C	*/
#definw Acceptancessage Lost re
#define CAN_GIF				0xFFC02A9C	ailbox Acceptance MaC02A88	/* Debu
#define CAN_GIF				0xFFC02A9C	2B00	/* Mailbox 0 Lo              
#define CAN_GIF				0xFFC02A9C				0xFFC02B04	/* Maibal Status Reg
#define CAN_GIF				0xFFC02A9C	CAN_AM01L			0xFFC02Beive  Interrup                                                                                                REG			0xFCAN_GIS			us Interface Unit (0Mailbox 3 HigLatus Register             */
          */
#defi_AM01L			0xFFC02L0xFFC02A98	/* Global Intceptance Mask   Controllerfine CAN_AM07L           */
#define CAceptance Mask   e SPI_CTL	fine CAN_AM07LFlag Register           ceptance Mask   /
#define fine CAN_AM07LOL			0xFFC02AA0	/* Masteceptance Mask          */
fine CAN_AM07L                        ceptance Mask             fine CAN_AM07Lrrupt Pending Register  ceptance Mask          */
fine CAN_AM07Lfine CAN_MBTD			0xFFC02Aceptance Mask          */
fine CAN_AM07L                   */
#dceptance Mask             fine CAN_AM07Lble Warning Level       ceptance Mask             fine CAN_AM07L	N_ESR				0xFFC02AB4	/* E 7 Low Acceptance			0xFFC0fine CAN_AM07L	                         7 Low AcceptanceControllefine CAN_AM07L	niversal Counter Registe 7 Low Acceptancee SPI_CTLfine CAN_AM07L			0xFFC02AC4	/* Universa 7 Low Acceptance/
#definefine CAN_AM07L	                    */
# 7 Low Acceptance       */fine CAN_AM07L	 Counter Force Reload Re 7 Low Acceptance         ance Mask     			0xFFC02ACC	/* Universal CountLr Configuration Register  7 Low Acceptance       */x 13 High Accepks 												*/
#defin 7 Low Acceptance       */x 13 High Accep Acceptance Mask         7 Low Acceptance         x 13 High Accepbox 0 High Acceptance Ma 7 Low Acceptance             */
#define 8	/* Mailbox 1 Low Accep/* Mailbox 7 High1 Low Acceptance Mask  	0xFFC02B0C	/* Mailbox 1 /* Mailbox 7 HighMailbox 11 High AcceptaN_AM02L			0xFFC02B10	/* M/* Mailbox 7 High02B60	/* Mailbox 12 Lowdefine CAN_AM02H			0xFFC0/* Mailbox 7 HighH			0xFFC02B64	/* Mailb     */
#define CAN_AM03L/* Mailbox 7 High CAN_AM13L			0xFFC02B68e Mask        */
#define /* Mailbox 7 High/
#define CAailbox 17 HiAcceptance Mask       *//* Mailbox 7 HighFFC02B70	/* Mailbox 14 ox 4 Low Acceptance Mask /* Mailbox 7 HighM14H			0xFFC02B74	/* Ma	/* Mailbox 4 High Accept/* Mailbox 7 Highine CAN_AM15L			0xFFC02xFFC02B28	/* Mailbox 5 Lo/* Mailbox 7 High  */
#define CAN_AM15H	AM05H			0xFFC02B2C	/* MaixFFC02B40	/* Mail1 Low Acceptance Mask  fine CAN_AM06L			0xFFC02BxFFC02B40	/* MailMailbox 11 H        OPSSs Interface Unit (0xFFC0N_AM06H			0xFFptanatus Register       RXelay wAL_MAPro
#de    orgisteingle-Shot     ntrol Register  _AM21L			0xFFC00xFFC02A98	/*ox 21 Low Acceptance Mask       */
#define CAN_AM21H			0xFFC0Controll_AM21L			0xFFC0           */ox 21 Low Acceptance Mask       */
#define CAN_AM21H			0xFFC0e SPI_CT_AM21L			0xFFC0Flag Registerox 21 Low Acceptance Mask       */
#define CAN_AM21H			0xFFC0           21L			0xFFC0OL			0xFFC02Aox 21 Low Acceptance Mask       */
#define CAN_AM21H			0xFFC0       *C	/* Mailbox 23             ox 21 Low Acceptance Mask       */
#define CAN_AM21H			0xFFC0        C	/* Mailbox 23rrupt Pendingox 21 Low Acceptance Mask       */
#define CAN_AM21H			0xFFC0       *C	/* Mailbox 23fine CAN_MBTDox 21 Low Acceptance Mask       */
#define CAN_AM21H			0xFFC0       *C	/* Mailbox 23             ox 21 Low Acceptance Mask       */
#define CAN_AM21H			0xFFC0        C	/* Mailbox 23ble Warning Lox 21 Low Acceptance Mask       */
#define CAN_AM21H			0xFFC0        Mailbox 21 High 2FFF)										*/
/*  */
#define CAN_AM22L			0xFFC02BB0	/* Mailbox 22 Low A/
#define CAN_GIM	ptanc								*/
#define C  */
#define CAN_AM22L			0xFFC02BB0	/* Mailbox 22 Low Ar                 ptancbox config reg 1      */
#define CAN_AM22L			0xFFC02BB0	/* Mailbox 22 Low A	/* Global Interruptanc                 */
  */
#define CAN_AM22L			0xFFC02BB0	/* Mailbox 22 Low A */
#define CAN_COptanc4	/* Mailbox directi  */
#define CAN_AM22L			0xFFC02BB0	/* Mailbox 22 Low A                  ptanc                      */
#define CAN_AM22L			0xFFC02BB0	/* Mailbox 22 Low A			0xFFC02igh Acceptanhdog Control Register e CAN_AM21L			0xFFC0C02A60	/* Mailbox Tra  */
#define CAN_AM22L			0xFFC02BB0	/* Mailbox 22 Low A              */

ptanc
#define CAN_TRR1			  */
#define CAN_AM22L			0xFFC02BB0	/* Mailbox 22 Low Arary Disable Featuptancst Reset reg 1        */
#define CAN_AM22L			0xFFC02BB0	/* Mailbox 22 Low AFFC02AB0	/* Prograptanc/
#define CAN_TA1			  */
#define CAN_AM22L			0xFFC02BB0	/* Mailbox 22 Low A         */
#defin2BFC	owledge reg 1       		0xFFC02BB4	/* Mailbox 22 High Acceptance Mask      */ilbox 28 Low Acceptanc                     		0xFFC02BB4	/* Mailbox 22 High Acceptance Mask      */ Mask      */
#define * Overwrite Protectio		0xFFC02BB4	/* Mailbox 22 High Acceptance Mask      */N_AM29H			0xFFC02BEC	/uration, Control, and		0xFFC02BB4	/* Mailbox 22 High Acceptance Mask      */Mailbox 30 Low AcceptaLOCK			0xFFC02A80	/* 		0xFFC02BB4	/* Mailbox 22 High Acceptance Mask      */ce Mask      */
#defin          */
#define 		0xFFC02BB4	/* Mailbox 22 High Acceptance Mask      */			0xFFC02AA4	/* I0xFFCssage Lost reg 1    		0xFFC02BB4	/* Mailbox 22 High Acceptance Mask      */0x8))
#define CAN_AM_HC02A88	/* Debug Regis		0xFFC02BB4	/* Mailbox 22 High Acceptance Mask      */AN_MB00_DATA0		0xFFC02                     		0xFFC02BB4	/* Mailbox 22 High Acceptance Mask      */1		0xFFC02C04	/* Mailbbal Status Register  		0xFFC02BB4	/* Mailbox 22 High Acceptance Mask      */8	/* Mailbox 0 Data Woeive  Interrupt Flag  23 Low Acceptance Mask       */
#define CAN_AM23H			0xilbox 28 Low Acceptanc                      23 Low Acceptance Mask       */
#define CAN_AM23H			0xr         01530	/* R */

/* System Interru       */
#deRatus RegisterDeny But Don't L */
 TIMER6_o        */
#define CAN_GIM	0		0xFFC0148C	/* Fr        */
#define CAN_MB02_DATA1		r                 TRR           */1 [31:16] Register       */
#define CA	/* Global InterruTRRFlag Register1 [31:16] Register       */
#define CA */
#define CAN_COTRROL			0xFFC02A1 [31:16] Register       */
#define CA                  TRR             1 [31:16] Register       */
#define CA			0xFFC02AA4	/* ITRRrrupt Pending1 [31:16] Register       */
#define CA              */

TRRfine CAN_MBTD1 [31:16] Register       */
#define CArary Disable FeatuTRR             1 [31:16] Register       */
#define CAFFC02AB0	/* PrograTRRble Warning L1 [31:16] Register       */
#define CA         */
#defin0		02FFF)								1 [31:16] Register       */
#define CAN                 0		0								*/
#d1 [31:16] Register       */
#define CANREG			0xFFC02AC0	0		0box config re1 [31:16] Register       */
#define CAN*/
#define CAN_UC0		0             1 [31:16] Register       */
#define CAN                 0		0ter          1 [31:16] Register       */
#define CANFFC02AC8	/* Unive0		0             1 [31:16] Register       */
#define CAN*/
#define CAN_UCC02CxFFC02C40	/* Mailbox 2 Data Word 0r Configuratio1 [31:16] Register       */
#define CANailbox Acceptancebox 
#define CAN_1 [31:16] Register       */
#define CAN2B00	/* Mailbox 0box st Reset reg 1 [31:16] Register       */
#define CAN			0xFFC02B04	/* box /
#define CAN1 [31:16] Register       */
#define CANCAN_AM01L			0xFFCC02Cowledge reg 1x 2 Data Word 2 [47:32] Register        1 [31:16] Register               x 2 Data Word 2 [47:32] Register       x 3 Data Word 2 [47:* Overwrite Prx 2 Data Word 2 [47:32] Register       C6C	/* Mailbox 3 Daturation, Contrx 2 Data Word 2 [47:32] Register       ENGTH		0xFFC02C70	/*LOCK			0xFFC02x 2 Data Word 2 [47:32] Register       ine CAN_MB03_TIMESTA          */
#x 2 Data Word 2 [47:32] Register       1C	/* Mailbox 3 HCAN_ssage Lost rex 2 Data Word 2 [47:32] Register       MB03_ID1		0xFFC02C7CC02A88	/* Debux 2 Data Word 2 [47:32] Register       #define CAN_MB04_DAT              x 2 Data Word 2 [47:32] Register       er        */
#definebal Status Regx 2 Data Word 2 [47:32] Register       [31:16] Register    eive  Interrup2C4C	/* Mailbox 2 Data Word 3 [63:48] R 1 [31:16] Register               2C4C	/* Mailbox 2 Data Word 3 [63:48] RAcceptance Mask  TRnce Mask      */
#def           */02BA8	/* Mailb                     21H			0xFFC02BACC02CB0	/* Mail 2 Data Word ngth Code Register          */
#d       CB0	/* Mail           */
gth Code Register          */
#de SPgister        Flag Register gth Code Register          */
#d/
#dgister        OL			0xFFC02AAgth Code Register          */
#d    gister                      gth Code Register          */
#d    gister        rrupt Pending gth Code Register          */
#d    gister        fine CAN_MBTD	gth Code Register          */
#d    gister                      gth Code Register          */
#d    gister        ble Warning Legth Code Register          */
#d    e CAN_MB05_TIMEN_ESR				0xFFCCB4	/* Mailbox 5 Time Stamp Value048] Register                   CB4	/* Mailbox 5 Time Stamp Value148] Register     niversal CountCB4	/* Mailbox 5 Time Stamp Value248] Register     		0xFFC02AC4	/CB4	/* Mailbox 5 Time Stamp Value348] Register                   CB4	/* Mailbox 5 Time Stamp Value448] Register      Counter ForceCB4	/* Mailbox 5 Time Stamp Value5    */
#define CCAN_MB03_ID0		0xFFC0e CAN_MB05_TIME ConfigurationCB4	/* Mailbox 5 Time Stamp Value648] Register     ks 											CB4	/* Mailbox 5 Time Stamp Value748] Register      Acceptance MaCB4	/* Mailbox 5 Time Stamp Value848] Register     box 0 High AccCB4	/* Mailbox 5 Time Stamp Value9Register         8	/* Mailbox 1AN_MB05_ID0		0xFFC02CB8	/* Mailbolbox 6 Data Lengt	0xFFC02B0C	/* AN_MB05_ID0		0xFFC02CB8	/* MailboESTAMP	0xFFC02CD4N_AM02L			0xFFCAN_MB05_ID0		0xFFC02CB8	/* Mailbo */
#define CAN_Mdefine CAN_AM02AN_MB05_ID0		0xFFC02CB8	/* MailboRegister              */
#defineAN_MB05_ID0		0xFFC02CB8	/* Mailbox 6 Identifier Hie Mask        *AN_MB05_ID0		0xFFC02CB8	/* Mailbo		0xFF 7 IdentifieAcceptance MasAN_MB05_ID0		0xFFC02CB8	/* Mailbo/* Mailbox 7 Dataox 4 Low AcceptAN_MB05_ID0		0xFFC02CB8	/* Mailbo07_DATA2		0xFFC02	/* Mailbox 4 HAN_MB05_ID0		0xFFC02CB8	/* Mailbo    */
#define CAxFFC02B28	/* MaAN_MB05_ID0		0xFFC02CB8	/* Mailbo3 [63:48] RegisteAM05H			0xFFC02        */
#define CAN_MB05_ID1		lbox 6 Data Lengtfine CAN_AM06L	        */
#define CAN_MB05_ID1		ESTAMPgister   A   */
#define CAN_AM06H			0xFFAA0xFFC0200C	/* xFFC00Fel       nce Mask       */
#define CAN_AMAA           */
/* Mailbox 8 Time Stamp Vale Mask        */
#defAAntrol Register/* Mailbox 8 Time Stamp Valh Acceptance Mask    AAUART1_LSR			0x/* Mailbox 8 Time Stamp Vallbox 8 Low AcceptanceAA              /* Mailbox 8 Time Stamp ValB44	/* Mailbox 8 HighAA	/* Modem Stat/* Mailbox 8 Time Stamp Val			0xFFC02B48	/* MailAA */
#define UA/* Mailbox 8 Time Stamp Val CAN_AM09H			0xFFC02BAAer            /* Mailbox 8 Time Stamp Val*/
#define CAN_AM10L	AA			0xFFC02024	/* Mailbox 8 Time Stamp Valask       */
#define AA              /* Mailbox 8 Time Stamp Valcceptance Mask      *AA   */
#define B08_ID0		0xFFC02D18	/* Mailb1 Low Acceptance MasAAh Code RegisteB08_ID0		0xFFC02D18	/* MailbMailbox 11 High AcceAA	/* Mailbox 6 B08_ID0		0xFFC02D18	/* Mailb02B60	/* Mailbox 12 AAB06_ID0		0xFFCB08_ID0		0xFFC02D18	/* MailbH			0xFFC02B64	/* MaAA   */
#define B08_ID0		0xFFC02D18	/* Mailb CAN_AM13L			0xFFC02AAgh Register   B08_ID0		0xFFC02D18	/* Mailb/
#define CAN_AM13H	AA    */
#define CAN_MBTIF2			0xAAer        */
#B08_ID0		0xFFC02D18	/* MailbFFC02B70	/* Mailbox AA Word 1 [31:16B08_ID0		0xFFC02D18	/* MailbM14H			0xFFC02B74	/*AACE8	/* MailboxB08_ID0		0xFFC02D18	/* Mailbine CAN_AM15L			0xFFAAN_MB07_DATA3		B08_ID0		0xFFC02D18	/* Mailb  */
#define CAN_AM1AAr       */
#de    */
#define CAN_MB08_ID1	TIMESTAMP	0xFFC02D34	/                  */
#define CAN_MB08_ID1	     */
#define CAN_MB* Overwrite Pr    */
#define CAN_MB08_ID1	 Low Register         uration, Contr    */
#define CAN_MB08_ID1	Mailbox 9 Identifier HLOCK			0xFFC02    */
#define CAN_MB08_ID1	0_DATA0		0xFFC02D40	/*          */
#    */
#define CAN_MB08_ID1	18L			0xFFC02B90	/* AA	/* Mailbox 8     */
#define CAN_MB08_ID1	fine CAN_MB10_DATA2		0C02A88	/* Debu    */
#define CAN_MB08_ID1	] Register      */
#de                  */
#define CAN_MB08_ID1	 10 Data Word 3 [63:48bal Status Reg    */
#define CAN_MB08_ID1			0xFFC02D50	/* Mailboeive  InterrupHigh Register           */

TIMESTAMP	0xFFC02D34	/              High Register           */

Mailbox 20 High AcceT      */
#define CAN0 [15:0] RegiAMP	0xFFC02D14	unt RegistuTIMERfuem Som    */
#define CAN_MB05_T/
#define CAN_M  */
#define CAN_MB11_ID0		0xFFC0e Register      egister          */
#define CAN_MB11_ID0		0xFFC0ox 5 Identifier x 8 Identifier   */
#define CAN_MB11_ID0		0xFFC0	0xFFC02CBC	/* M0		0xFFC02D20	/  */
#define CAN_MB11_ID0		0xFFC0#define CAN_MB06
#define CAN_MB  */
#define CAN_MB11_ID0		0xFFC00] Register     :16] Register    */
#define CAN_MB11_ID0		0xFFC0box 6 Data Word lbox 9 Data Wor  */
#define CAN_MB11_ID0		0xFFC0A2		0xFFC02CC8	/TA3		0xFFC02D2C  */
#define CAN_MB11_ID0		0xFFC0/
#define CAN_MB*/
#define CAN_  */
#define CAN_MB11_ID0		0xFFC03:48] Register  Code Register  ow Register           */
#define Clbox 6 Data Len/* Mailbox 9 Tiow Register           */
#define CESTAMP	0xFFC02CB09_ID0		0xFFC0ow Register           */
#define C */
#define CAN    */
#define ow Register           */
#define CRegister       High Register  ow Register           */
#define Cx 6 Identifier * Mailbox 10 Daow Register           */
#define C		0xFFC02CE0	/*10_DATA1		0xFFC02D44 Stamp Value Rer        */
#ow Register           */
#define C/* Mailbox 7 Da0xFFC02D48	/* Mow Register           */
#define C07_DATA2		0xFFCefine CAN_MB10_ow Register           */
#define C    */
#define 8] Register    ow Register           */
#define C3 [63:48] Regisox 10 Data Lenglbox 11 Identifier High Register  Register                      lbox 11 Identifier High Register  12 Identifier Lo* Overwrite Prlbox 11 Identifier High Register  FFC02D9C	/* Mailuration, Contrlbox 11 Identifier High Register  fine CAN_MB13_DALOCK			0xFFC02lbox 11 Identifier High Register   Register                 */
#lbox 11 Identifier High Register  DATA0		0xFFC02D*/
#define CAN_lbox 11 Identifier High Register  rd 2 [47:32] RegC02A88	/* Debulbox 11 Identifier High Register  	/* Mailbox 13 D              lbox 11 Identifier High Register  MB13_LENGTH		0xFbal Status Reglbox 11 Identifier High Register          */
#defieive  InterrupATA0		0xFFC02D80	/* Mailbox 12 DatRegister                      ATA0		0xFFC02D80	/* Mailbox 12 Datde Register   MBTDMA Channel 10 Configuration RTD0xFFC01001      Mask Stao     orarilyO Port H ister        */
#dDegister   */
#ier Low gister    	/* Port G I/O Mask */
#define C		0xFFC0150C	/xFFC02DDC	/* Mailb        */
#define ance Mask   FHce Mask      */
#define CFC02B90	/* MFH2BA8	/* Mailbox 21 LIO		mati Ass           Handfine 21H			0xFFC02BAC	/* efine CAN_AM15FH Acceptance Mask    FFC02DE4	/* Mailbox 15 Data Word 1 [31:16] Re Mask        */
#defiFH/
#define CAN_AM22H	FFC02DE4	/* Mailbox 15 Data Word 1 [31:16] Rh Acceptance Mask     FHxFFC02BB8	/* MailboxFFC02DE4	/* Mailbox 15 Data Word 1 [31:16] Rlbox 8 Low Acceptance FH High Acceptance MasFFC02DE4	/* Mailbox 15 Data Word 1 [31:16] RB44	/* Mailbox 8 High FH    */
#define CAN_AFFC02DE4	/* Mailbox 15 Data Word 1 [31:16] R			0xFFC02B48	/* MailbFHL			0xFFC02BC8	/* MaFFC02DE4	/* Mailbox 15 Data Word 1 [31:16] R CAN_AM09H			0xFFC02B4FHox 25 High AcceptancFFC02DE4	/* Mailbox 15 Data Word 1 [31:16] R*/
#define CAN_AM10L		FHsk       */
#define FFC02DE4	/* Mailbox 15 Data Word 1 [31:16] Rask       */
#define CFH_AM27L			0xFFC02BD8	FFC02DE4	/* Mailbox 15 Data Word 1 [31:16] Rcceptance Mask      */FHMailbox 27 High AccepC02DE8	/* Mailbox 15 Data Word 2 [47:32] Regi1 Low Acceptance MaskFHe Mask       */
#defiC02DE8	/* Mailbox 15 Data Word 2 [47:32] RegiMailbox 11 High AccepFHCAN_AM29L			0xFFC02BEC02DE8	/* Mailbox 15 Data Word 2 [47:32] Regi02B60	/* Mailbox 12 LFH* Mailbox 29 High AccC02DE8	/* Mailbox 15 Data Word 2 [47:32] RegiH			0xFFC02B64	/* MaiFHnce Mask       */
#deC02DE8	/* Mailbox 15 Data Word 2 [47:32] Regi CAN_AM13L			0xFFC02BFHe CAN_AM31L			0xFFC02C02DE8	/* Mailbox 15 Data Word 2 [47:32] Regi/
#define CAN_AM13H		FHCAN_MB03_ID0		0xFFC02er       */
#definC02A60	/* Mailbox TraC02DE8	/* Mailbox 15 Data Word 2 [47:32] RegiFFC02B70	/* Mailbox 1FH(x)		(CAN_AM00H+((x)*C02DE8	/* Mailbox 15 Data Word 2 [47:32] RegiM14H			0xFFC02B74	/* FHC00	/* Mailbox 0 DataC02DE8	/* Mailbox 15 Data Word 2 [47:32] Regiine CAN_AM15L			0xFFCFHox 0 Data Word 1 [31:C02DE8	/* Mailbox 15 Data Word 2 [47:32] Regi  */
#define CAN_AM15FHrd 2 [47:32] Register2DEC	/* Mailbox 15 Data Word 3 [63:48] Registne CAN_MB16_LENGTH		0xF                     2DEC	/* Mailbox 15 Data Word 3 [63:48] Regist CAN_MB16_TIMESTAMP	0xF* Overwrite Protectio2DEC	/* Mailbox 15 Data Word 3 [63:48] Regist CAN_MB16_ID0		0xFFC02Euration, Control, and2DEC	/* Mailbox 15 Data Word 3 [63:48] RegistMB16_ID1		0xFFC02E1C	/*LOCK			0xFFC02A80	/* 2DEC	/* Mailbox 15 Data Word 3 [63:48] Regist_DATA0		0xFFC02E20	/* M          */
#define 2DEC	/* Mailbox 15 Data Word 3 [63:48] Regist18L			0xFFC02B90	/* MFHilbox 1 Data Word 1 [2DEC	/* Mailbox 15 Data Word 3 [63:48] Regist[47:32] Register      *C02A88	/* Debug Regis2DEC	/* Mailbox 15 Data Word 3 [63:48] Regist:48] Register      */
#                     2DEC	/* Mailbox 15 Data Word 3 [63:48] RegistRegister         */
#debal Status Register  2DEC	/* Mailbox 15 Data Word 3 [63:48] RegistRegister         */
#deeive  Interrupt Flag DF0	/* Mailbox 15 Data Length Code Register  ne CAN_MB16_LENGTH		0xF                     DF0	/* Mailbox 15 Data Length Code Register  Mailbox 20 High Acceer   	/* Mailbox 11 Time Stamp Valueer   atus RegisterTFFC0efine DM       nce Mask       */
#deGIO_MASKB_CLEAR		ilboter      */
# CAN_MB19_DATA3		0xFFC02E6C	/*CR			0xFFC02010	/* Modem C3:48           */   */
#define CAN_MB19_LENGTH		                */
#define3:48Flag Register   */
#define CAN_MB19_LENGTH		e Status Register         3:48OL			0xFFC02A   */
#define CAN_MB19_LENGTH		fine UART1_MSR			0xFFC02013:48                */
#define CAN_MB19_LENGTH		                          3:48rrupt Pending   */
#define CAN_MB19_LENGTH		0201C	/* SCR Scratch Regis3:48fine CAN_MBTD   */
#define CAN_MB19_LENGTH		      */
#define UART1_GCT3:48                */
#define CAN_MB19_LENGTH		ol Register               3:48ble Warning L   */
#define CAN_MB19_LENGTH		roller		(0xFFC02A00 - 0xFF3:48]TAMP	0xFFC02D9  */
#define CAN_MB19_LENGTH		0boxes 0-15							3:48]*/
#define CAN  */
#define CAN_MB19_LENGTH		0	0xFFC02A00	/* Ma3:48] Register       */
#define CAN_MB19_LENGTH		0                 3:48]ox 12 Identifi  */
#define CAN_MB19_LENGTH		0AN_MD1				0xFFC023:48]A0		0xFFC02DA0  */
#define CAN_MB19_LENGTH		0                 3:48]/
#define CAN_  */
#define CAN_MB19_LENGTH		0#define CAN_TRS1	ata Lx 19 Data Word 2 [47:32] Register  CAN_MB13_DATA2	  */
#define CAN_MB19_LENGTH		0                 3:48]ster      */
#  */
#define CAN_MB19_LENGTH		0C	/* Transmit Req3:48]ta Word 3 [63:  */
#define CAN_MB19_LENGTH		0                 3:48]C02DB0	/* Mail  */
#define CAN_MB19_LENGTH		010	/* Transmit Acata Le CAN_MB13_TIMgister         */
#define CAN_Mine CAN_MB20_LENGTH		Register       gister         */
#define CAN_Mster         */
#defi13 Identifier Lgister         */
#define CAN_M Time Stamp Value RegFFC02DBC	/* Maigister         */
#define CAN_ME98	/* Mailbox 20 Idefine CAN_MB14_Dgister         */
#define CAN_MMB20_ID1		0xFFC02E9C	 Register      gister         */
#define CAN_M02A1C	/* Receive  Iden14 Data Word 1gister         */
#define CAN_M_MB21_DATA1		0xFFC02E	0xFFC02DC8	/* gister         */
#define CAN_M     */
#define CAN_Mdefine CAN_MB14gister         */
#define CAN_M2 [47:32] Register   48] Register   gister         */
#define CAN_Milbox 21 Data Word 3 box 14 Data Len Stamp Value Register         *ine CAN_MB20_LENGTH		ESTAMP	0xFFC02D Stamp Value Register         * Mailbox Interrup*/
#ox 19 Data Word 2 [47:32] RegisteR      */
#definxFFC0_MB19_DATA3		0xFFC02E6C	/* Mailbox 19 Data Word 3 [63#defs 												**/
#define CAN_MB19_LENGTH		0xFFC02E70	/* Mailbox 19 DaRa Length Code Railbox 22 Identifier Low RegistMB19_TIMESTAMP	0xFFC02E74	/R Mailbox 19 Timailbox 22 Identifier Low Regist*/
#define CAN_MB19_ID0		0xRFC02E78	/* Mailailbox 22 Identifier Low Regist          */
#define CAN_MBR9_ID1		0xFFC02Eailbox 22 Identifier Low Registh Register          */

#deRine CAN_MB20_DAailbox 22 Identifier Low Registata Word 0 [15:0] Register R     */
#defineailbox 22 Identifier Low RegistMailbox 20 Data Word 1 [31:R6] Register    ailbox 22 Identifier Low RegistFFC02E88	/* Mailbox 20 DataRWord 2 [47:32] ailbox 22 Identifier Low Regist20_DATA3		0xFFC02E8C	/* Mai#defN_ESR				0xFFC02[63:48] Register      */
#define CAN_MB20_LENGT#def                lbox 20 Data Length Code Register         */
#d#defniversal CounterTAMP	0xFFC02E94	/* Mailbox 20 Time Stamp Value #def		0xFFC02AC4	/* #define CAN_MB20_ID0		0xFFC02E98	/* Mailbox 20 #def                ter           */
#define CAN_MB20_ID1		0xFFC02E#def Counter Force Rntifier High Register          */

#define CAN__MB2ine CAN_MB22_TIMESTAMP	0xFFC02ED4	/r Configuration RRegister       */
#define CAN_MB21_DATA1		0xFFCAN_Mks 												*Data Word 1 [31:16] Register      */
#define CAAN_M Acceptance Mask2EA8	/* Mailbox 21 Data Word 2 [47:32] RegisterAN_Mbox 0 High Accep_MB21_DATA3		0xFFC02EAC	/* Mailbox 21 Data Word_MB28	/* Mailbox 1 L     */
#define CAN_MB21_LENGTH		0xFFC02EB0	/* _MB20xFFC02B0C	/* Math Code Register         */
#define CAN_MB21_TI_MB2_AM02L			0xFFC02* Mailbox 21 Time Stamp Value Register         _MB2efine CAN_AM02H	ID0		0xFFC02EB8	/* Mailbox 21 Identifier Low Re_MB2    */
#define C#define CAN_MB21_ID1		0xFFC02EBC	/* Mailbox 21 _MB2 Mask        */
ster          */

#define CAN_MB22_DATA0		0xFFC_MB2Acceptance Mask Data Word 0 [15:0] Register       */
#define CA_MB2x 4 Low Acceptan2EC4	/* Mailbox 22 Data Word 1 [31:16] Register_MB2/* Mailbox 4 Hig_MB22_DATA2		0xFFC02EC8	/* Mailbox 22 Data Word_MB2FFC02B28	/* Mail     */
#define CAN_MB22_DATA3		0xFFC02ECC	/* M#defM05H			0xFFC02B23 [63:48] Register      */
#define CAN_MB22_LEN#define CAN_AM06L			ailbox 22 Data Length Code Register         */
IM Mask reg 1                    BIM2BA8	/* Mailbox 21 Le Stamp Va21H			0xFFC02BAC	/* Mailbox 21 25 IdAcceptance Mask    DATA0		0xFFC02F40	/* MAcceptance Mask       */
#define CAN_AM22H	DATA0		0xFFC02F40	/* M/
#define CAN_AM23L   *xFFC02BB8	/* MailboxDATA0		0xFFC02F40	/* MxFFC02BBC	/* Mailbo   * High Acceptance MasDATA0		0xFFC02F40	/* M Low Acceptance Mas   *    */
#define CAN_ADATA0		0xFFC02F40	/* M    */
#define CAN_   *L			0xFFC02BC8	/* MaDATA0		0xFFC02F40	/* MH			0xFFC02BCC	/* M   *ox 25 High AcceptancDATA0		0xFFC02F40	/* Mox 26 Low Acceptanc   *sk       */
#define DATA0		0xFFC02F40	/* Mask      */
#define   *_AM27L			0xFFC02BD8	DATA0		0xFFC02F40	/* M_AM27H			0xFFC02BDC25 Id2FFF)										*/
/* */
#define CAN_MB26_DA/
#define CAN_GIM	25 Id								*/
#define C */
#define CAN_MB26_DAr                 25 Idbox config reg 1     */
#define CAN_MB26_DA	/* Global Interru25 Id                 */
 */
#define CAN_MB26_DA */
#define CAN_CO25 Id4	/* Mailbox directi */
#define CAN_MB26_DA                  25 Id                     */
#define CAN_MB26_DACAN_AM31H			0xFFC0 Mailentifier High Register          *C02A60	/* Mailbox Tra */
#define CAN_MB26_DA              */

25 Id
#define CAN_TRR1			 */
#define CAN_MB26_DArary Disable Featu25 Idst Reset reg 1       */
#define CAN_MB26_DAFFC02AB0	/* Progra25 Id/
#define CAN_TA1			 */
#define CAN_MB26_DA         */
#defin Mailowledge reg 1       [31:16] Register      *efine CAN_MB27_DATA0		                     [31:16] Register      *Register       */
#def* Overwrite Protectio[31:16] Register      * Data Word 1 [31:16] Ruration, Control, and[31:16] Register      *C02F68	/* Mailbox 27 DLOCK			0xFFC02A80	/* [31:16] Register      *CAN_MB27_DATA3		0xFFC0          */
#define [31:16] Register      *			0xFFC02AA4	/* IF8C	/ssage Lost reg 1    [31:16] Register      *define CAN_MB27_TIMESTC02A88	/* Debug Regis[31:16] Register      *e Register         */
                     [31:16] Register      *27 Identifier Low Regibal Status Register  [31:16] Register      *C02F7C	/* Mailbox 27 Ieive  Interrupt Flag ilbox 26 Data Word 2 [4efine CAN_MB27_DATA0		                     ilbox 26 Data Word 2 [4fine CAN_MB02_DATAGIMox 17 Data Word 1 [31:errupt A RegWTIM  */
#define DMA9_STX        */
# I/O Mask Disable Interrupt A Register sable Interrupt A RegWR9_DATA1		nce Mask      *ailbox 29 Data Word 1 [31:16] Register      */
#define CAN_MB29_DATA2		P9_DATA1		e CAN_AM22H	8	/* -  */
#define I/O Mask Disable Interrupt A Registdefine UART1O9_DATA1		8	/* Mailboxerrupt Ata Word 1 [31:16] Register      */
#define CAN_egister                   WU9_DATA1	cceptance MasCLEAR			Register         */
#define CAN_MB29_TIMESTAMP	0xFFC02FB4	/* Mailbox 29 TUIA9_DATA1	_ID1		0xFFC02E CAN_MB02Uni Map   *#defR_DESC_I/O Mask Disabister       *9_DATA1	A0		0xFFC02E40FFC0box 14 IdentiI/O Mask Disable Interrupt A RegisteFC02B90	/* Mai9_DATA1	igh Acceptance MAN_AM20H			0xFF Word 1 [31:16] Register      */
#define CAN Low RegisteCE9_DATA1   */
#define UniversFC00rt H I/         		0xFFC02FC4	/* MaiR		0xFFC01724	/_DATA1			0xFFC02BD8	         TriggC8	/      		0xFFC02FC4	/* Mailbox 30 Datister       D      *2E50	/* MailboxTIMER6Denifine gister         */
#define CAN_MB29_TIMESTAMP	0ta Word 0 [15 MDMA_S0_CURR_Y_COUNT#define CAN_MB29re regxFFC01510	ailbox 29 DatRQWI_INT_MASK		0xFFC01424	/* TWI Master InterMB29_DATA2		0xF* Mailbo							*e Stamp Value Register         */
#define CAN_MB30_ID0		0xFFC02FD8	/* Pilbox 30           63:48] Register Register         */
#define CAN_MB30_ID00xFFC02FB0	/* * Mailbor     h Code R Register         */
#define CAN_MB30_ID0		0x0xFFC02FB4	/* Mailbox 29 Time* MailbRTGIO_CLEAR			 Register       */
#define CAN_MB31_DATA1		0xFFC02FE4	/* Mailbox 31 Dataer  * MailbFC02D54AN_MB29_ID1		0xFFC02FBC	/* Mailb Register   ier High Regis* Mailbne CAN_MB10CAN_MB30_DATA0 Register         */
#define CAN_MB30_ID0	d 0 [15:0] Regi* Mailb/
#define CAN_AM17L			0 Register         */
#define CAN_MB30_ID0		0 Register      re regtus RegDATA2		0xFFC02FC8	/* Mailbox Register         */
#defR		0xFFC01724	/re registers MB30_DATA3		0xFFC02FCC	/* Register         */
#define8] Register   re regne CAN_M0_LENGTH		0xFFRegister       */
#define CAN_MB31_DATA1		0xFFC02FE4	/ta Word 0 [15Fount Register                   MB29tatus Rex 30 Time Stamp Value ReG I/O Mask State Specify Interrupt A Regisine CAN_MB29_DATA2		0xFtatus Registeifier Low Register x20))
#define CAN_MB_TIMESTAMP(x)	(CAN_MB00_TIMESTAMP+((x)*0x2P)
#definetifier High Register        G I/O Mask State Specify Interrupt A R0xFFC02FB0	/* xFFC0100C	/* 0 [15:0] Regx20))
#define CAN_MB_TIMESTAMP(x)	(CAN_MB00_TIMEST0xFFC02FB4	/* Mailbox 29 TimeC Prescaler ERegister    20))
#define CAN_MB_DATA0(x)		(CAN_MB00_DATA0+((x)*0x20))

/* Pin Control Reer  TC Prescaler    */
#define CAN_MB31_DATA3		0xFFC0G I/O Mask State ier High Registatus R [63:48] Register      */
#dG I/O Mask State Specify Interrupt A Red 0 [15:0] Regitatus Rode Register         */
#dx20))
#define CAN_MB_TIMESTAMP(x)	(CAN_MB00_TIMES Register      tatus alue Register         */
#define CAN_MG I/O Mask State SpeciR		0xFFC01724	/tatus r Low Register           */
#defineG I/O Mask State Specify Interrup8] Register   tatus ifier High Register      20))
#define CAN_MB_DATA0(x)		(CAN_MB00_DATA0+((x)*Register   UCCN(CAN_MB00_ID1+((x)*0x20))
#define4	/*             DATA2		0xFFC02FC8	#define DMA8_CURR_ADDR			0xFFC00E24	/* DMA Channel DMA_D1_STARTC    MP/
#define TWI_MASTER_ADDR         #define DMA8_CURR_ADDR			0xFFC00E24	/* DMA Channel 8 Cock Count RegiWDe MDMA_S1        */
#defineW X Cdo                           */
#define MDMA_MA0 Initial Block Count Regi Chaister*/
#define TWI_INT_STIO			ASKB_SET		0xFFC01528	/* Port G I/O Mask Enable Interrial Block Count RegiERRO      TH		0xA0_BCOUNT		0xr         s */
# 29 Dadefine MDMA_D0_Y_MODIFY		0xFFC00F1C	/* MemDMAUC_OVE/* PPI T7MDMA0_BCOUNT		0xFFC03    load* HMDMA0 Current Block Count Register                     r ReDMA10_Y_MODI HMDMA0_ECOUNTdefine MDMA  */
      *TXA0 Current Block Countrupt Register  Destina09define TWI_INT_STAXefine C0 Current Block Count Register           Overflow Interrupt Register T        A              */
#deine CAN_MB1MDMA1_BCINIT		0xFFC03348	/* HMDMA1 Initial Block C              REJECT */
#dBe TWI_XMT_DATA8		0 CAN_MB30_RejRegist0 Current Block Count Register      shhold RegisterMER3_CONFne TWI_XMT_DATA8		0 CAN_MB30_DATA10 Current Block Count Register                        	0xFFC010Ddefine TWI_INT_STAotale HMDMA1_ECUNT		0xFFC0s/* HMDMA1 Current Edupt Register    ine DMA1Et Register        	0xFFC03358	/W/t X CountID/* HMDMA1 Current Edgrupt Register  MER3_CONF     0_BCOUNT		0xFFon RegCAN_MB30_0xFFC01504	Ldefin Currenount RegisterRCte Specify RDATA2		0xFFC02FC8	Re340	/10_CURR_ADDR			0xFFC00EA4	/* DMA Chaount Register              DATA2		0xFFC02FC8	A Reg		0xFFC Mailbox 30 Data Word 1 [31:16] Register     r            DATA2		0xFFC02FC8	ne DMA8_CURR_X_COUNT		0xFFC00E30	/* DMA Channel 8 CRegister   ES	0xFFC0170C	/* Poister       CKem Control Re		0xFFC01700	_DESC_PTR	0xFFC0			0xFFC01008	ime Register Stuffer Register         */
#define MDMA_D1_STA	CR ordnse or theCRCXT_DESC_PTR	0xFFC00F80	/* MemDMA Stream 1 De	SAMP	0xFFCcaler Enufine    minfineegister anddefine UART1ETime RegFC0065C         G I/O Mask State Specify In            **				modRT0_SCdefi************ PLL AND RESET MASKS the bit-fielW	0xFFC0170C	/* Por#define CAN_MBLter  O_SET				0xfier Low RegisLi2 (o(			0 Mail			*/
#define UARTWLTLKIN, egister  CLKIN/2                    T                    */
#define MDMA  PINe TWI_XM REGIntergister                            */
m 0 SMUXount Register                    J         Pointer RegJ    / Regul
#define MDMA_S0_Y_COUNT			0xFFC00F58	     ress R                     */
#02FA4	/*FS0/DT0PRI       */
#define PDWN			0x0020	/* Enter        ine TWI_MASTER_ADD DMA9_ST
#deSEL3:e SPI_CTL				0xFFC00500egister     PJCdefin000008	/* 3)<<1MA Str      CAN/                       */
			0x0020	/* Cnter Deep Sleep Mode                   DR0SEC    SEC#define PDWN			0x0020	/* CE_CAxFFC0010       */
#define         Port /TX                  */
#define MDMA_D0              O_STAT		0xFFC0142C Delay To EBIU        */
#define SPI_BAU            *F PORTHIta Word 	/* AFegister      ne DMA8_CURR_X_COU			0x0020	/*FDExFFC0ep Sleep Mode                   ter             */
#define	MSEL		/
#define	SET_MSDIO_M        */
#define HMDM		0x0100MAR1:			0xFFC00424	/* Global ConMacros (Only UT          */

R While */
ny
**				macro that shifts left to/
#define	SETTMSEL(x)		(((x)&0x3F) << 0x9)	/* Set MSEL =10-63 --> VCO = CLKIN*MSEL         */

/* TE38	/*D8	/* Map Mode                    MR7:       */
#define SPI_RDBR		Macros (Only US6order biptor PR While    BIU  6              */
#define PDW/
#define	SETS6      CCLK = ep Mode                    MRMA11_CONFIG			0xFFC00EC8	/* DMA Channe*/
#define	CSEL_Dier Selecptor PCCLK/VCO Factors                   */
#define SPI_RDBV2		0x0010	/*  5xFFC00EEC	/* DK = VCO / 2     5                            */
#define	CSE5_DIV4		0x0020	/*              CCLK = VCO /DDR		0xFFC00EC4	/* DMA Channel 11 Star* Set SSEL = 0-15ier Selec      CCLK/VCO Factors                           */
#defineV2		0x0010	/*  4order bits beiK = VCO / 2     4                            */
#define	CSE4_DIV4		0x0020	/*              CCLK = VCO /	0xFFC00EC0	/* DMA Channel 11 Next Desx0001	/*         ier SelecRT0_SCCCLK/VCO Factors                   */
#define SPI_STAMacros (Only Uster    4	/* Mem Whileount Register  ne DMA8_CURR_X_/
#define	SETF_DIV4		0x0020	/*              CCLK = VCO / Channel 10 Y Modify Register                  */

#de        tus Reg									*/
#define ount S/
#define SPI_FLG				0xFFC0            *G                 RegG        er                     			0x0020	/*GS 5        0	/*              CCLK = VC */
#9:               */
#define  20             er Deep Slisters efine BYPASS			0x0100	1* Bypa                           	PG004FA0 Control IN_20			0x0008xFFC001FF)							*/
#de 20          R    */
#define	GAIN_50			0x000C	/*        12:efine TIMER0_CONFIG		0xFF0.85 V (-5% - +1er Deep Slam 0 Soternal Voltage Level   PRI/RFS1/R   *Acceptance                            N_20			0x0008                        20          T+10% Accuracy)     */
#define	VLEV_090		0x05:                     */
#	/*             er Deep SlR Regis									*/
#define STx0080T/*  T           VLEV         */
#define MDM  HANDSHAKEFFC00(HDMA* MemDMA Stream 1 Destination   */
 +10x_xt Descriptor Pointer RA0		0xFFC02HxFFC0xFFC00100 - 0EV_100	Datashak_DESC_0/Current X Count Register              upt B Registeister          +10tting Low0_CURR_X_COUNT	0xFFC00F70	/* MemDMA Port F Function Enable R         t For UrgA ChaTh5 Y e UAne DMA8_CURR_X_COUNT		0xFFC00E30	/* DMA ChaC	/* MailboxI          */

/* Mailbox 30 Data ne DMA8_CURR_X_COUNT		0xFFC00E30	/define UART1Dacy)             */
Doall 			0x0100	/* Enable RTC/Reset Wakeup From#define CAN_MDscillator FreisterANWE			0x020RQ If         PORT#define TWI_RCV_DATA160xFFr            */
#define	MDMA_S0_CONFIG			0xFFC00F48	/* M   */
#define TWI_RCV_DATA16RQ      #define	GAIN_50			0x000NData      */
#define PORTGIO_SET				0xFFC015	CLKBUFOE	/* Alternative legacy naSING              */
#define	G       nnel      efine CLKOE		CLKBUFOE	/* Alternative legacy naMULTHIO_M0	/* Internal Voltag_PLLENABLED	0x000 regi (Defaul          ative legacy naURGEN     e TIMERULL_ON				0x0002	/* Processor In Ful
#defrrent Block FC02B90	/* MB      /
#define 340	/Be			/* DiIfine	egister                         */
#define MDMA_D08 Curr			*/
#d  */
PinWI_INT_MASK		0xFFC01424	/* TWI Master Interrupt Mask Regi-5% - +10% Accura             /* Mailbox 30 Data     */
#de RTC/Reset Wakeup From Hibernate      h-Byte)      ANWE			0x0200	/* Enab0
#define CHIPID_MANUFACTURE           entry aR_DESCenterrtheDLL	r-call Lengtoot ROM fun     sel 11 Y Modif_BOOTROM_RESET 0xET1_MRCS LE_FAULT		0x0008	/FINAL_    e Double F2ult Causes Reset   DO_imer 1L_DIe Double F6LE_FAULT		0x0008	/0x00_DXE_FLASHe Double F8ult Causes Reset   By Core D     Double FA         */
#define RESET_WD Mod Double FCult Causes Reset   GECore DADDRESSDouble-Fault   10           */
#define RESET_SOFTWAOG			0x4000	12           */
#define RESET_SOFTWAdog Timer   1400B0	Aupt nfineDeprec/
#de       Provi    			0
#dewar1 De     Stream 1 Dest 20          _MSEL(x/
#dT_MSEL(xe	NOBOOT				0xA11_CUR* PLL_DInd ORed baKEgist	S      
#    fx000/* _DEF_BF534_Hfine