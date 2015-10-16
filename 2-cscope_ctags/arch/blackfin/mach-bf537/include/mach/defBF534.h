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
#define TLSBIT		0x0010	/* Transmit Bit Order 2005-2008 Analog Devices Inc.
 *
 * Licensed unddder the ADI ITFSensed20ncludInternale GPL-2 (oFrame Sync Selecte _DEF_BF534_H

/*Include allBFSRensed4sters e ADitions er tincluRequiredude <asm/def_LPBlackfin.h>

/*****
*D Core reg8******Data-Independenthe AD************************Include allLystem M10******Low*********************************************_BF534_H****Inc.*****allLAystem M2****
***ate		*/
#deTL				0xFF/0000Clock and System Control	(0xFFC000>

/********CKFEense4TL				0	/* PLFall CopEdge000	/* PLL Control Register          0 er     0000SPORTx_TCR2 MasksLL CoMacro	er   er the Include allSLEN(x)		((x)&0x1F)clude ReguTX Word Length (2 - 31)BF534_H
#define _DEF_BF534_H

/*>

/********XS Regu01TL				0X Secondary Enable */

#ifndef _DEF_BF534_H
#define _DEF_BF534_H

/*>

/********SFPLL Stai******efinitionStere*************er                      */>

/********RFScensedTL				0Left/Righlater)
 (1 =  000	hChannel 1st          er t              RCR1           er the A ADI VR_CTL		RSPENensed 01cludReceive Regul					*/
#define SWRST				0xFFer the ADI PLL_LOCKCNTol	(0xFFC00000 - 0xFFC00 RCLK     102****L Cobit d      1FFFC001IV		cens Regist4 the/* PDivide RegLockTL				0x
** TYPE_NORM      4rationr  Format Ne ADlC001
#ifndef _DEFol	(0xFFCthe ADI 108	/* Interrr                  */ULAW      8   */omp0xFFC0ine urigh                            */
#define PLL_LOCKCNT			0xFFC000 */
#dAhe ADI SIC_IMASK	0xFFC001010y	/* Interrupt Mask Register                                      RSD lcensed onclud
#definer Chip ID CT			0xFFC00108	/* Interrupt Reset Vector Address ne SIC_IAR2			stem MMLock Co       */
#define			0xFFC00000	/* PLL Control Register the ADI SI_CTL				0xFFC**AssignmentgurationrC_IAR3			0xFFC0ster 3                      */
FFC0010AR3			0xFFC0011C	/* Interrupt A 2#define SWRST				0xFFC0Divide Regist0 - FFC00104FFFC0010			er the ADI00110	/2r theand bruptPLL tus/* Interru                 /
#define SIfine SWRST				0xFFC00100	/* SoftwaD				0xFFC00104	/* System Configuration                  R         ntrol Rnterrupt Wegisterle0xFFC00104	8 theVoltagiguraulRtor        WDOG_CTL			0xFFC002000100	/* SoftwaSTAT	0xFFC00104	C	/Rystemakeup RegiRCTL			0xFFC00200	/* W*/
#define SWRST				0xFFC00100	/* Software Rese	0xFFC00104Rr the/* PL* PLCog Co/
#define WDOG_STAT			0xFFC00208	/* Watc*/
DOG_CTL			0xFFC00200	/* W ID		0xFFC0010414ter  -Fir    er tupt Assignment Register 1                      */
#dT			0xFFC00208    /ress ntrol Register            rXN      1          */1FFIFO Not Empty       * Watchdog           F)									*/
#define RUVe SYefine RTC_tickyFFC001244UNT		flow Count RegisRTC_I              3       RTC StopwaOakeup Reg RTC                Ovtchdog Count RegisRTC_SW             3    #define      * F	0xFFC00AR0		***********   FulVECT	#define RTC_SWCNT			0xFFC0030C	/* R3or theRTC Alarm         FFC001        finition Watchdog Count Regisne RTC_ALARM			0xFFC0031>

/********ch Count 2ler       WDOG_CTL			0xFFC00200	/               ALARMARM			0xFFC1	/* RTC Alarm HR	0xFFC04ncludefinitionHoldWDOG_CTL			0xFFC00200	/* W#define SWRST				0xFFC00100	/* SoRTCMCMCCT		cro			0xFFC00204	/* Watchdog   P_WOFF          & 0x3F/
#defMulticTransmiWindow Offset Fielddefine SWRST				0xFFC00100	/Only use WSIZEter      th Logic OR Whil- 0xt    pLowerp RegisBit			0xFFUART0_RBR	0xFFCFFC0    (e UAx)>>0x3)-1arm T) << 0xCufine 00118	/*tus Registerize = (x/8)-1_CTL			0xFFC00200	/* TC_IST			0xFFC00204	/* Watchdog  REC_BYPASSxFFC00ncludBypass Mode (Noe SYSCRRecovery Watchdog Count Register       04	/* Sy Consor2FROM4Transmit Holding re2 MHz       from ification */xFFCFF)		0x040C	            EC_8****16xFFC003nWDOG_CTL			C_IMAS       */

16n UAR    LC      *    40C	Include allMCDTXP	0xFFC00      upt Enable RDMAxFFC00308	Pa     p    IIne UART0_MCR	     nterruMCDRodl RegistAR0		FC00308	/* RTC In
#define SYe SWRST				0xFFC00100	/*T0_Mine UART0_MCRor thMr       FFC001gister       ******							*/
#define SWRST				0xFFC          Include allFSD  */
#d8
#define          RTC Pr*****t    *
#Rela*****hip*/
#define UAFD_00xFFC00T0_SCR			0xFFC0041C	/* SDelay = 0gister                                      */
#1 SIC_IAR0SR     *    */

/* U*/
#deGCTLgister    42/* SyGlobafine		0xFFC00204	/* Watchdo2 SIC_IAR0W                  */

/* SPI 2terrupt C		         5          */50xFFC0040C	*/
3ense3e Macro         */

/* U/

/* SPI 3 SPI_CTL				0xFFC00500	/* SPI Control Register  ification Register                     *4 SPI_CTL				0xFFC00500	/* SPI Control Register  5ense5                                  5 SPI_CTL				0xFFC00500	/* SPI Control Register  6ense6                                  6 SPI_CTL				0xFFC00500	/* SPI Control Register  7ense7                                  7 SPI_CTL				0xFFC00500	/* SPI Control Register  8ense                        */

/* SPI 8 SPI_CTL				0xFFC00500	/* SPI Control Register  9ense9                                  9 UART0_MCR		(0xFFC00500 - 0xFFC005FF)								*/
*/
#defTransmit Holding register       I Ce UART0_MCR		(0xFFC00500 - 0xFFC0FC00308	/* RTC InterrBFC00600 - 0xFFC006FF)								*/
#denterrupt C			0xFFC00500	/* SPI CoFC00308	/* RTC Ithe ACFC00600 - 0xFFC006FF)								*/
#dee*/
#trolle0xFFC001050r the/
#deoFC00308	/* RTC InterrDFC00600 - 0xFFC006FF)								*/
#de/
#define SPI_FLG*/
#define TI4	/FC00308	/* RTC InableFC00400	 - 0xFFC006FF)								*/
#dester                             FC00308	/* RTC Ieup R                    egister                                       TDn Regist* ADI BIMER1_COUNTE    ASYNCHRONOUS MEMORY CONTROLLER 			0S***** TIMER1_COUNTER		     /isteEBIU_AMSPI ne UART0_DLH			0xFFC00404	/* Div  AMCKART0xFFC00204	/* WatchCLKOU                WRS    FC00110	/*MER0_Poftwarle A    xFFC00308	/* RTC CT			he ADI B	AMBEN_NO		0xFFC00ncludAll Banks Di                           CTL		*/

/* Systeegister                IAR0AR2      ion Register           As - 0Memoryer    0 oT0_D TimrruptConfigurter   TIMER0-7 Registers		(0x          _n Regi		0xFFC0ER2          ART0_MC624	s 0 & 1 2 Counter und b                                    _B2MER12_6ERIOD28	/* Timer     2 CountePe, 1,  */
#        */
#define TIMERdefine TIMER2_COUNLLdefine/* SyLFFC0062C	/* Timer 2 Width (all) Timer 2CTL			        r 0 Counter neBCTL0R1_CP		0xFFC0062C	/*		0xFF2 Coun1 B0RDYoUART0_THR         er 2/* (B0) RDY0xFFC0040C          */
#define TIMER1_WIDTH		0xFFC0061C	/** Time3     2 CoPOER3_CONFnable RegBter   Act    High             */
#define TIMER1_WIDTH		0xFFC0061C	/* Timer 1 Width Reg       TT DiviFFC00308	/* RTB0      iable 2 Co (ReadcratWrite)ERIODcyster    TH		0xFFCh              pt Status RegAR0		 - 0xFFC006FF)						r        MER14_CO2FIG		0    *Timer 4 Configur             AR0                     */
#define TIMER4_3FFC00628	/* Time4mer 3 Perrification Reg-Byte)                  */
#define TIMER4_4ne TIMER4_PERIOD		0xFFC0PLL St_IAR2			0xFByte)0ow-Bu		0x				AOEine r   /e TIMER4_NOUNTER_PERIOD	r thefine TIMERWIDTH		er          caalerr 4 WidTimer 4 Configuregister      xFFC00628	/*    */
#define TIM5       4r         3xFFC00634	5er Register                   efine TIMERP    */
#define TIMER5_COU0xFFC00634	4 Perir 3 r 5 Configuration Register        */
#define     */
#define TIMER5_Hegister       040C	/B0       Regis~egister   ine ~AOE                 */

#deD		0xFFCr RegH            *5    SClarm /* Timer 5 Configuration RegistexFFC00628	/* Time		0xFFC0HFFC00628	/* TiC/* Time6      */

#6er Register                       		0xFFC0062C	H     IOD		0xFFC00658	/*0	/* Timer 6 Configuration Registe0                       *R              up Regi    he AAccess Timer                  */
#dMd Re TIMER3_COUNTER	30xFFC00634	3 PA            *Lock Co*/
#define TIM6R5_CONFTH		0xFFC0066C	/* T#d6 Timer 5 Period Register              NTER		0xFFC3C00658	/*IMER7_CONFIG		0xFFC00670	/* Timer 7h Regiguration Register                     0xFFC00634	ster 3 70xFFC00628	/* Time74	/* Timer 7 Counter   iguration Register                     DTH		0x Con5ne TIMOD		0xFFC00678	/* Time5 7 TIMER7_CO0_THR			0xFFC00400	/* Transmit Holdi            Transm Con6		0xFFC0067C	/* Timer 7 Widt6 Register                               */

#define TIMER_E Data B Con7		0xFFC0067C	/* Timer 7 Widt7 Register                               */

#define TIMER_E       h Reg      OD		0xFFC00678	/* Time8 Register                               */

#define TIMER_EPI_RDBR Con9		0xFFC0067C	/* Timer 7 Widt9 Register                               */

#define TIMER_Es		0xFnable     er the ADI BONFIG		0xFFC	gister                      FIO040C	       7WIDTH	Port F I/Oer    2_00n Regify Register                    olding register            _FAdefinTL			0xFFC00200	/*      ONFIG	eripheral Interrupt Clear Re* Ti      */

#7er Register                                   */
4	/* DC0065pheralInterrupt Wa/* Tim                                         *         */
#define POR4	/* EI/O Pin State Toggle Register    gister                               *TL			0xFFC00200	/*CONFIG	I/OFI/O Pin State Toggle Registerion Register                      Timer 1 Timer 4 ConfiW              00718	/B0er    nterrupt Set Regi_COUNTER0660	/* Timiguration Register   
#define P_			0A_er                FC00704	/* P           _CTL			0xFFC00200	/* W      */
#define P_TOGGLETFIO_CLEIO_MNTER		0xFFC0_RBR			01C	/* Port F I/O Mask)							*/
#define SWRST				0xFFC00100	/* Sofine PORTFIIO_M0xFFC00634	00720	/* Port F I/O Mask StateifyRegister   A            pt A Register   */
TFIO_MCLEIO_M	0xFFC0WIDupt Enabl Port F I/O Mask Staterupt B Register     B Register   */
#define PORTFIO_MASKKA_NABine      GPL-2 28	/* Port F I/O Mask Enas Register                                    *PORTFIO_MAISIO_MASKBster  Bu28	/* Port F I/O Mask ETHR			0xFFC00400	/* Transmit Holding regis */
#define PORTF    USASKBtle Alte28	/* Port F I/O Mask EHR			0xFFC00400	/* Transmit Holding regist */
#define PORTFrpose	/* PPI_RDBR/* Port F I/O Mask StatexFFC0040C	/              efine PORTFIO_CLE    */
#define POH		0xFFC00* Time00* Port F I/O Mask State Specify Interrupt Bpt A Register   */
CLEgister             Port F I        * Port F I/O Mask State FC00308	/* RTC Interrpt A Register   */
Sgister              Port F           Port F I/O Mask State ister                 */
#define PORTFIO_gister            0xFFC0070 0 Inter                    */

                                 */
#defigister            O_CLEAR	10660	/*                     */

ify Interrupt A Register   */
#define PORgister            #define 4                            */

errupt A Register                 */
#def             1             0071    TC    1 (Ber 2 Configuration Regiter              Timer 4 Configurter           efine FSDIV		0xFFC0080C	/* SPORT0 Tipheral Interrupt3_/
#inclu ConfRegister                                */
#define TIMER4_             latoxFFC0      er       e POask Toggle Enable Interrupt B Register  ter                            TER		0xF    RT0 RX Data Register                      gister                  FC00PERIOD		0xFFC00658	/
/* Real Time Clock		(0xFFC00300 - 0xFFC0/
#define TIMERWID1le Interrup SPOR  */
#defuration Register                      KDIV		0 GPL-2 (or Regiser      KDIV	                     */
#             OR     LKDIV		0xFFC00828	/* SPOR    */
#    /* PL ConfiMask Toggle Enable Interrupt B Register   */
#define SPORT0_Rne SPORT0_RCLKDIV		0xF* Timergister                               FFC00828	/* STIMER	0xFFC00670	/* Time0	/* Timer 6 Configuration RegisteL			0xFFC00834	/* SPORT0 CurT0 Receiv    ivider                              0_CHN Cont*/
#define SPORT0_MCM SPORT0 Trae ADI                                          828	/* SPORter    1PORTFIO SPORT0 Transmit Fra            */                       gister                  er 2 Configurne SPORT0_MCMCl Interrupt Set Reginterrupt A Register                  */
egister           er     ve0	/*S0ulti-Chan8/

#de       Muister                 */
#define PORTFIO_Tegister               */
ti-ChaC00844	/* SPORT0 Multi-Chan                                 */
#definegister           0xFFC00ne RTC_00844	/* SPORT0 Multi-Chanify Interrupt A Register   */
#define PORTPORT0 Transmi Port F I/7_0011-Chanlti-         GPL-2 (oSelerrupt A Register                 */
#defi             r    FIO_MAS Sel-Channel Receive Select RegisSPORT0 Receive Frame Sync Divider         * SPORT0 Tranr    IO_DIR	T0_R-Channel Receive Select RegisTHR			0xFFC00400	/* Transmit Holding register         Tr     */
#de    Re Multi-Channel Transmit Sel*/
#define WDOG_STAT			0xFFC00208	/* Watchd						*Generr            */
                 						*/
fine00738	/* Port F I/O Source Sensitivity ReAR			0xFFC007                TC Multi-Channel Transmit Sel    */
FFC0073C	/* Port F I/O Set on BOTH Ane UART0_MC7        F I/    			0xFFC00904	/* SPORT1 TransmRTFIO_INEN			0xFFC00740	/* Port F I/O InpEefine TIMER1        Port      	0xFFC00904	/* SPORT1 Transm Toggle Enable Interrupt A Register   */
#define PORTFr     0xFFC0SPORrans1_TFS 0xFFti-Chan9Alarm SP Specify Interrupt B Register   */
#define PORTFIO_MASr      TIMER6	/*SPORT1_TX			0xFFC00910	/* SPORe Interrupt B Register                 */
#define PORTr     RT0_MCNTERSPORT1_TX			0xFFC00910	/* SPORnable Interrupt B Register                  */
#define1         0    TCS* SP3C	/ort F I/O Mask State terrupt B Register     onfiguration 2 Register       definer    PORT0_TCiguration Register        SPORT Select Res */
#incluC	/* SPORT0 Receive Frame Syncefine BTFIO84C	/* RTC StopwatMulti-Channel CoT1_RRT0_RX			0xFFSPORT0 Receive Frame Sync Divider     B        0ulti-Ch               */
#define SP0910		0xFFSPORT1_Rine SPORT1_RFSDIV		0xFFC0092C	/* SPORdefine B_Snnel Re               */
#define SPORT1_   *TFIO_CLEAR94	/* ISPORT1_RCLKDIV		        */
#definFIO_MA     eive Clgister                 t F I/Togglefine SPORT1_RCR2		Begister                _RFSDIO_DIRC001FS* SPORT0 Multi-C                                          */
#de820	/* SPORT0 TransmRFSD */
#d<asmntrol Reggister                  Port F I/OSource Polarity                           PORT1      T                                */
#d7           */
#define SPSensitivMTCS0		0xFFC00940	define		0xFFC004	/* SPORT1 Transmit ConfigurSPOHTFIO_CLEAR	     SPORT1_MTCS1	et on BOTH PLL_ Registedefineter      ider                               */
INENTFIO_CLEAR	/

#de Port F I/OInputr Enable Adefine         lect Register 1      */
#define 0 Multi-Ch#define TIME        8          */80xFFC0040define			0xF0 RT1 TX Data Registe   */
#define T0xFFC00828	/* SPOR */
#define SPFSDIV		0xFFC0092C	/*define     	0xFFdefine SPORT1_MRCS1		0xFFC00954	/* SPORT1 Multi-Channel R2ceive Select Register 1       */
  */
#deSPORT1_MRCS2		0xFFC00958	/* SPORti-ChHNL			0xFFC0082C	/* SPORT0 Receive Frame Sync Div* SPO						*/trol Register ter           2            *              2 (Bpt Status Register     ORT0 Multi-Channel SPORTth Register                  00AFter                 GCTL			0xFFC00A00	/* Asynchro0xFFC00810	/* SPORTRT0 Multi-Chann		0xFF         5 Configuratiopt Status Register     4	/* Asynchronous Memory Bank synchronous Memory10xFFC0091Aine UAAsynchter                 - 0xFFC005FF)			1k Control Reg#defiSr                       al CNTER		0xFFC0                                   */
#defigister                  al CPERIOD		0xFFC00658	                            */
#defink Control Registert Con2Multi-Chann4U_AMBCTL1	pt Status Register                       			0xFFC00A10	/* SDRAM Globa             *5      *00A SPORTSDRAMSTAT			0xFFC00308	/* RTC Interruptnk Control RegisterDTH		C00628	/* Time5    affic Control Registers													*/
#define DMA_nk Control Regist      TFIO_CLEARAfine SPr 2 CounFC00830	/* SPORT0 Status Register      Asynchronous Memory 2           ffic ContrCpt Status Regi                      TFIO_CLEAR83(below) proonfid for bacFIG	   */
#define SbilMTCS* SPORT0 TrDMA_TCP    xFFC009 */
#defiD/
#drrent Count SPORT0 Transmier		UNTMA_TCCNT			0xFFC00B10	/* Traffic Con (below) provided fo#defis (below) provided forMA_TCCNT			0xFFC00B10	/* Traffic Congister                  pt Status Register   */
20904	/* SPORT1 Transmve Select Register 1    Channee Sele							* SPORT0 Tr                        TART_ADDraffic Con0RTC l Transmit Select Register 1      */
#define SPORT0_Msynchronous Memory Glob7efine DMA0_CONFIG				smit Select Register 2      */
#define SPORT0_MTCS3		0x                  l Contefine DMA0_CONFIG				ect Register 3      */
#define SPORT0_MRCS0		0xFFC00850      SKB_SET		s code coefine DMA0_CONFIG				ter 0       */
#define SPORT0_MRCS1		0xFFC00854	/* SPOR     FIO_MASKB_ Time80	/efine DMA0_CONFIG				    */
#define SPORT0_MRCS2		0xFFC00858	/* SPORT0 Multi RegisO_DIR				 0 Y Co4	efine DMA0_CONFIG				RFSDIV		0xFFC0092C	/* SPORT1 Receive FraMC2		0xFFC0083C	/* S */
#defin 0 Y Co8A Channel 0 Y Modify Rgister                                    							*/
#del Regi          00704	(0efine DMA0_CONFIG				mit Configuration 1 Register                     */
#de            */
# Specifyefine DMA0_CONFIG				ification Register  Register                     */
#de720	/* Port F I/O/O Pin RQ_STATUS			0xFFC00C28smit Clock Divider                                    0720	/* Port F I/O/O PinRQ_STATUS			0xFFC00C28 Toggle Enable Interrupt A Register   */
#define PORTFH		0xFFT1 Multi-ChannPinRQ_STATUS			0xFFC00C28	/* DMA Channel 0 Infiguration 2 Register                   
#define ORT1 Mult          */
#define DM Interrupt B Register                 */
#define PORTRegister    7NFIG		R_ADDRQ_STATUS			0xFFC00C28nable Interrupt B Register                  */
#define2 1 RegisteDMA Chan     pt Status                     2     */
#def     PORT1_CHNL			0xFFC00934	/* SG			4 PORT#defineDMA ChaStart Addres Register MA0_CURR_Xontrol_CONFIG			3r theDMA         0 Curr IntXr          DMA Cha4	/* hannel 1 ConfigurationNT			0xFFC00B10egisterY                                me Sync Di		0xFFC0      /* DMA Channel 1 X Cou                         DMA1_NEXT_DESC_PT628	/* TimC4ster      	/* DMA Chan2ODIFY			0xFFC00C54	/* RTFIO_INEN			0xFFC00740	/* Port isterSefine DMA0_CONFIG			4 PORTFIOBC00C48	/* ODIFY			0xFFC00C54	/* D                     */
#define SPORT1_MCMC1		0xFFC00 RegisteD     RT1_MTCS2	ODIFY			0xFFC00C54	/* ster 1        */
#define SPORT1_MCMC2		0xFFC0093C	/* Ser   */
#define PORTFIO_ODIFY			0xFFC00C54	/* D     */
#define SPORT1_MTCS0		0xFFC00940	/* SPORT1 MuCURR_Y_COUNT		0xFFC00C38ODIFY			0xFFC00C54	/* 
#define SPORT1_MTCS1		0xFFC00944	/* SPORT1 Multi-Chan     ification Register ODIFY			0xFFC00C54	/* DSPORT1_MTCS2		0xFFC00948	/* SPORT1 Multi-Channel TranC00C68	/* DMA Channel 1 Interrupt/Status RegistTCS3		0xFFC0094C	/* SPORT1 Multi-Channel Transmit SelC00C68	/* DMA Channel 1 Interrupt/Status RegistFFC00950	/* SPORT1 Multi-Channel Receive Select RegisC00C68                TCODIFY			0xFFC00C54	/* D	/* SPORT1 Multi-Channel Receive Select Register 1          */
#define DMA1_CURR_Y_COUNT		0xFFC00C78	/* DMA Channel 1 Current elect Register 2       */
#       */
#define DMA1_CURR_Y_COUNT		0xFFC00C78-Channel Receive Select Register 3       */

/* Extern             */
#define SPOR3 (B              A00	/* Asynchronous Memory Global Control Register  */
#definee Se 2 Start Address Register        nous Memory Bank Control Register 0  */
#define EBIU_AMBCTL1		0xFFC00A08	/* AsyegistMask Toggle Enable3                           */
#defiDSPI rrent Countsine SProl RFC00Pointer8fine SPORT10STATine DMA2_CONFIG				0xFFC00CDMA2_X_Mr                               */
#define SPOR                                    gister                  */
#define TIME          */
#define SPOR                                    */
#def3/* DMA Channel 2 Next 3riods Register			*/
#define DMA_TC_CNT			0xFFC00B10	/           */
#gister FFC001Fve Selec					*/
#define SWRST				0xFFC00100	*/
#define DMA_TC_PER			0xFFC00BCA0	RTX			0xFFC009182Cer 7 Period Register  ter Register    */
#define DMAnk Control Registe SPORT0_MCMC2       */
#er	*/

/* Alternate deprecated register names (below) provided for               */
#defi3A_TCCNT			0xFFC00B10	/* Traffic Con0Bfine STraffic0 - 0xFFC00658	PORT0_ Int        ress ReA2l ConPHERAL_MAP          A	0xFF                           YC00A10	/* 8ne SPORrans           */
#define DMA2_CURR_X_COUNT		0xFCS3		0xFFC0085C	/* SPORT0 Multi-Channel           */
#define DMA2_CURR_X_COgister                  3                  008443ART_ADDR			0xFFC00C04	/* DMA Channel 0 Start Address Register                   

#defannel 2 Cur44MA3r                 0xFFC00C08	/* DMA Channel 0 Configuration Register          844	/* SPOine SPORTfine DMA3_START_ADDR	_COUNT			0xFFC00C10	/* DMA Channel 0 X Count Register       8TAT			0xFFC00C04	/fine DMA3_START_ADDR	*/
#define DMA0_X_MODIFY			0xFFC00C14	/* DMA Channel 0 X ModxFFC00C04	/* DMA Chfine DMA3_START_ADDR	                  */
#define DMA0_Y_COUNT			0xFFC00C18	/* DM4	/* DMA ChaDMA Chafine DMA3_START_ADDR	                                   */
#define DMA0_Y_MODIFY	Modify Regiude <asmfine DMA3_START_ADDR	egister                                              */
#def DMA Channeguration Rne DMA3_START_ADDR	A Channel 0 Current Descriptor Pointer Register    */
#defin        Transmefine0CDC	/* DMA Channel 3nnel 0 Current Address Register                                 */
#de         _CURR_DESC_PTR		0xFFC0	/* DMA Channel 0 Interrupt/Status Register                    */
#d         F_CURR_DESC_PTR		0xFFC0MAP		0xFFC00C2C	/* DMA Channel 0 Peripheral Map Register                    Tefine DMA3_IRQ_STATUS		MA0_CURR_X_COUNT		0xFFC00C30	/* DMA Channel 0 Current X Coun                 TURR_ADDR			0xFFC00CE4	/* DMA Channel 3 CuRR_Y_COUNT		0xFFC00C38	/* DMA Channel 0 Cl Register              */
#define DMA3_P             */

#define DMA1_NEXT_DESC_PTR		0xFFC00C40	/*  Channel 3 Current_CURR_DESC_PTR		0xFFC0nable Interrupt B Register                  */
#define3Receive Select Register         ress Register                                 */
#define DMA1_CONFI      ve Select Register     4 Next Descriptor        */
#define DMA2_EURR_X_COU        e TIMEin Staturation 2 Register      Channel 4 Start Addre             3gister                 F               */FC00928	/* SPORT1 Recei08	/* DMA Channel 4 Co                          egister                 F    RFSDIV		0xFFC0092C	/* S	0xFFC00D10	/* DMA Channel 4 X CouMA Channel 1 X Modify Regist4r                 #define D3 X Modify	0xFFC00D10	/* DMA Chanine DMA1_Y_MODIFY			0xFFC00C5C	/* DMA Channel 1 Y Modify R         C04	/* DMA Channel 4 Start Addre            */
#define DMA1_CURR_DESC_PTR		0xFFC00C60	/* DMAC04	/* DMA Char Re	0xFFC00D10	/* DMA Chanister    */
#define DMA1_CURR_ADDR			0xFFC00C64	/* DMA Chaeceive Select Regis                      
#define SPORT1_MTCS1		0xFFC00944	/* SPORT1 Multi-Chan628	/*el 0 Start Address	0xFFC00D10	/* DMA ChanSPORT1_MTCS2		0xFFC00948	/* SPORT1 Multi-Channel Tran       t Address Regi2fine DMA4_X_MODIFY			0xFFTCS3		0xFFC0094C	/* SPORT1 Multi-Channel Transmit SelATUS			0xFFC0       * SP	0xFFC00D10	/* DMA ChanFFC00950	/* SPORT1 Multi-Channel Receive Select Regis00CE4	/* DMA            	0xFFC00D10	/* DMA Chan	/* SPORT1 Multi-Channel Receive Select Register 1   628	/*                MRR_X_COUNT		0xFFC00D30	/* DMA Channel 4 Current X elect Register 2       */
#00CE4	/   */
#deCS3ne DM	0xFFC00D10	/* DMA Chan 3 X Modify Regi_MODIFY			0xFFC000900	/* SPORT1 t Descriptor Pointer RegDMA ChFFC00C10	/* DMA Channel 0 X Count Register       MA Channel 1 Current AddreDTIMER1_C TIMER3_COUNTER		0xegister        TLE								*/
#defin0D44	/*      Signamit Configurat */
#define PORTFIO_SE0xronous Memory Global Control Register  */
#definCL - 0xFFC005FF)					       AShe ADnct F I/Ot Clock Divider                        T			0xFFC00208	/* Watchdog Status Reg/* SDRAM Bank ContDMA D* DMA          smit Select Register 2      */
#define SPOR0B0C	/* Tra94	/* Dgisterfine SPORPASR Global SPO         C0084CURR_DDr     Refreshed Inrafff-el 5 Y						*/
#define SWRST				0xFFC00100	/* SoD          2_U_AMBCTL1	/* DMA Chanter   1 Arent X    C Pres	/* DMA Channel 0 X Count Register       *     */

/* DMA T    D	/* DMA Ch 0 I     annel 5 Y Modify Register                    r       CHIPIASckwards code compat 5 X CtRASs code compael Transmit Select Register 0                    */
#define PLL_LOCKCNT			0xFFC0001ointster			*/
#define            5_R_DESXel 5 Y Modify Register                                                     */
#define DMA*	*/

/* DMA Contro_STATUS			0xFRR_DESXFC00    A Channel 5 Interrupt/Status Register                              */
#define DM*0xFFC00634	up Regi_STATUS			0xFect Register 3      */
#define SPORT0_MRCS0ine D		0xFFC00C54	/* DMA Channel 1 URR_X_COUNT		0xFFCify Registe*/

              5_ter 0       */
#define SPORT0_MRCS1		0xFFC0           */
#define DMA5_CURR_Y_COUNT		0xFFC00D78	/A Channel 01A5_IRQ     #def                   */
#define DMA5_CURR_X_COUNT           */
#define DMA5_CURR_Y_COUNT		0xFFC00D78	/		0xFFC00C11define DMA2Dt A ReDegister                                               */
#define DMA5_CURR_Y_COUNT		0xFFC00D78	/ine DMA0_CULock Corrent Y CountA Channel 0 Current Descriptor Pointer Regi           */
#define DMA5_CURR_Y_COUNT		0xFFC00D78	/e DMA0_CURRpt S Current Y Countmit Configuration 1 Register                          */
#define DMA5_CURR_Y_COUNT		0xFFC00D78	/ne SPORT1* SPcriptor Pointer Reg                  define DA Channel 1 X Cou0_THR			0xFFC00400	/* Transmit Holding registATUS			0xmory Bank Co                                                 nterrupt /Sta                                         */
#define DMer         ry Globrrent Y CountC      Y Modify Register5                          */
#define DMA2_CURR_DESC_PTR		0xFFC00efine DMA6nel 1 Y Modif Current Y CountCDescriptor Po*/
#define Maperipheral Map Register                                           /* D            6                   efine DMel 5 Y Modify Register                        ess Register              DMA Channel 4 X ify Registe           9URR_X_COUNr 0       */
#define SPORT0_MRCS1		0xFFC00854	/* SPDMA6r                       UART0_RBR			0FC00200	/* Watress I/O Sourc 6 Perip DMA0_CONFIG		D6* InteDescriptor Po 6 Y Count Register      MA Channel 6 X C	/* SPORT0 Current Changi Register Channel 6 l 6 Current Descriptor Pointer Register    */
#define DMA6_CURR_ADDR			0xFFC00DAister            TER		0xFF*/

/*                                        */
#define DMA6_IRQ_STATUS			0xFFC00DA8	/* DMA ChanneD                0xFFC00634        7_NEXT_DESC_          */
#define DMA6_PERIPHERAL_MAP		0xFFC00DAC	/* DMA Channel 6 PeripheraU                ify Registpt Sta              #define DMA6_CURR_X_COUNT		0xFFC00DB0	/* DMA Channel 6 Current X Count RegisterA7_CONFIG				0xFFFIO_MASKB_Tr        Channel 6 		0xFFC00C58	/* DMA Channel 1 Y Co6nt Register          D800DB8	/* DMA Chan6 InteRT1 Status RegisO_DIR				      egister                        */
# SPORT1__Y_COUNT			Y Modify Register4	/* Timer 6 Counter Renel 7 Y Count        I/O Source SenDescriptoDDMA6_CURR_Y_COUNT		0xFFC00DB8	/* DMA Channel 6 Current Y Count Register            odify Registerel 0 X Mod                       l 6 Current Descriptor Pointer Register    */
#define DMA6_CURR_ADDR			0xFFC00DA                        DMA6_Y_MODIFY3 Current Y hannel 7 Start Address Register                                 */
#define DMAl 6 Perip    C_RFSDIV		0xFFC0092C	/* SPORT1 Recuration Register                                 */
#define DMA7_X_COUNT			0xFFC           D7CURRt                        */
ORT0_MTC                                  */
#define DMA7_X_MODIFY			0xFFC00DD4	/*ountB8	/* DMA      nel 7 Y
#define DMA2D                                       */
#define DMA7_Y_COUNT			0xFFC00DD8	/* DMA Channount RegisteCDMA CStatus Register                              */    */
#define DMA7_Y_MODIFY			0xFFC00DDC	/* DMA Channel 7 Y Modify RegisW* DMA Channeegister r      WR    */#define DMA7_CURR_X_CB_MODIFY			0xFFC00DDCefine Dnel 5 Y Modify Register                  egisteC0085/
#define UART0_RBR			0xFFC00DD4	/*CT		0xFFC00DFister7                   i			*/
#define EBI                        WRnt Regist  */ Data Bu* SPORT0 TrE10	/* DMAODIFY			0xFFC00C54	/* DMA Channel 1 r                              */OD		*/
#defiPUPSD* DMA Channel 2 C     -Upne Drt

/* SP(15 SCLK CY Modi
/* S Watchdog Count Register   atus Registertrol                         equence ( 7 CurMA1_HR		Before/Afterr 3                          H		0xFFC0          UNT		0xFFC00CB0	/* DMA8_Yonel 4 S        F I/OAT			0xFFC00208	/* Watchdog Status/
#def        T1 Status Regis#defin		0xFFC00D04	/* D            Y_MODIFYefine DMA7_D                 7 Ynel 5 PeripheEBU              */
#dDescripEx     */BInterrCop0_TC*/
#define DMA2_CURR_Y_COUNT		0							*/
#define SWRST				0xFFC0010BBRW                 e DMA7_Cast Back-To-    /* SPOTry GlobMA0_CONFIG		EA1_X_M	/* DMA Chan8el 6 Current Y CounMRr 3 Counel 7 Y Count Rendennel 7 Y			0xFFC	                     */
#define TIMER3_PERIOD		0xFFC0063er 3      */
#defi0071t X Count RegisTemp-	0xFensat00718	nt X CountValue (85/45* Degtion RegisteDMA6_* SPORT0 DDBG28	/* SPORT1 ReceTristigurt X Cou- 0xFFs Du    */us Gran002FF)								*/
#define WDOG_CTL			0xFFC00200	/* SD620	/* Register             ount RegisterY Mo		0xFFC00D04	/* DM04	/* Watchdog C DMA C8A6_CURR_Y_COUNT		0xF2C	/* DMA Channel 8 Peripheral Map RegigSZ_1 Modify Register* SPChannel 9 Star    */
#6MB DMA Channel 89FFC00E14	/* DMA Channel 8 X Modify Register    3	*/
#defi       nel 6 9   */
#define DMA73 Timer DMA Channel 9r Register                             */
#de6        *ter Regist DMA9_X_COUNT			0xFFC06       DMA Channel 9 X Count Register                            12ud raster    annel 6 A9_X_COUNT			0xFFC0E2Y_MODIFY			0xFFC00 9 X Count Register                           250_PERt X Count Register                  25    t X Count RegiSZ_5ister    FFC00600 - 0xFFC006FF)		       */512MBuration RegisterASKA_SET		0xFFCRR_X_COUNT		0xFFC00CF0Columnunt RMA C 5 Con        *Y Count Registerefine DMA0_C5R_ADDR			0x                 9                   9 DMAR		0xFNT			0xFFC00E58u		0xFFC00	/* TEount Register         r Register    */
#de1			0R			0ne DMAC00DB8	/* DM#definentrol          */
#define DMA5_CURR_Y_COUNT		0xFFC			0xFFC00DC	0xFFC00E5N_IIFY			0xFFC003	/* SyRTxFFC00E64	/*DCODIF                           I   */
#define SPORT0_RCR2			0xF DMA69         */S	/* DMA Ch* SPORT0 Tr          CU* DMA Ch                              */Pister/
#de   */
#defin                  Eer    Y			0xFFC0ine DMA9		0xFFC00E10	/*DR Periph            5Wi     */
vider* SPORT4	/* DMADne DMA7_CURR_EA/* PPres Currennount RAB Enable Erroount RegisterRTC_SWCNT		Timer 4 ConfG    * DMA ChaMA1_C          it Holding register            guration RC00DE8	/* DM Channel 8 X Count Registeter 3      */
#define S            */
#define DMA7_Y_COUNT		     DMAx       , MB10	yy       ne UART0_DLH			0xFegister     MAr 3 Countenel 7Confi/
#define DMA2_CURR_Y_COUNT		0xFFC00CB8	/Global Control Register  */
#definWNgister                 Direster  (W/RMA Channel Count Register         9		0xFFC00E10	/* D10    *D DMAa     		0xFFC00DansfeDESCCo                       */
#define SPI_SHADA10    DR			0xFFC00E24E9	/* DMA C 4 Yine      nel 5 Peripheral Map */

/* S               
#deefine DM     */
#define DMA10_Y_COUNT			0xFFC00E70	/* DMA  Peripheral Map 3         */
#define TIMER0_PERIODter                     Regi#define D0xFFC00          D/1 0 Period Register   Channel 1 Y Cou    */
#defiMCR			G		0X_MOD- 0xFt Re              Channel 0 XCleaxFFC00120	/* Interruer 2 Configuration Register      ster 3           r   S_SE/* DMA Channel * SP     upt        I0xFFC002FF)								*/
#define WDOG_CTL	 Modify Registr 3 Count0xFFC0FC00EA0	/* DMA CURR_X_COUNer                E                */ Periphnclude allN           */
#defi     tart Add    ster    (Stop/Autobel 0 X ModODIFY			0xFFC00PNT			0x                              #define TIMER0_COUNTER		0xFFC0060 DMA9_CUCURR_X_COUDMA10_CURR_X_CA ChaLock Co                        Y			0xFFC00rent Address RFSDIV		0xFFC0092C	/* ine DMA10_CURR_X_t RegiNT			0x0EB0	/* DMA Channel 10                     */
#define TI*                    */
#define DMA9_00718	/                                                     */
6_COUNT              */
#define       code c                              */

#define TIMER_E       R Descriptor Pointer Register         Y Coun              */
#defineeripheral Map Register                        ine DMA10_CURR_X_FC0011C	/* DM/
#define DMA11_START_ADDR		0xFFC00EC4	/* DMABAUC_STl 11 Start Address Register        MA Cha     */A Channel 5 Interrupt/Status Register                          nel 0 X                             (A Channel 5 Interrupt/Status Register           						*TIStart Address Register                00ED		0xFFC00E54	/* D11Channel 8/* DMA Channel FLOWiste1R_ADDR	0xFFC007*/
#define DMA3_CURR_X_COUN		0x_STORT1 TX Data RetopChannel 6 Interrupt/Sta
#AUTO                                     EET		0xFFC0RRA             Y Modify ReArrayptor Pointer Register   *SMMA5_Y_Cnnel RecSmallfigurl6_CURR_Y_COUListptor Pointer Register   *LARCount RxFFC007Largount RegistxFFC00ED0	/* DMA Channel 10_CO        */
#def DMA9_CU8Count Registerne UART0_DLH		             /
#define SPORT0_RCLKDIV		0xFypPORTdica      DMA9_/*/
#define  Channel 3 Current Y PY_COU         Modify RegMappe     Th TIMER2_CONFIG		0xFFC00xFFC00D1C	/* DMA Channel 4 YAP_PP

/* Watchdog Timer	          PoA3_CURipheral Interrupt10  */
#define EB SPORT0_T0EA4	 5 Configuration Register   EMACRX                 C00854	/Er EnnetY Modify r        */
#define DMA8_E     0xFFC00EDC	/* DM#define DMA11_X_COUN      E	0xFFC00Register       xFFC0D	*/
#defiESC_PTR	0xFFC00F00	/* MemDMA Stream 0 Destinat#define DMA11_X_COUNT0                           escriD0r               DMA9_CFWIDTH	Mem    Stream 0StartinatiMemDMA StreamonFFC00E14	/* DMA CTgister                  MA Channel _D0             D0_CONFIG     FFC00F08	/* MemDMA Str 0 Destination Configuration Re1l 10 In Count Register                */
#define M Reg*/
#defD0_CONFIG	0xFFCFC00F08	/* MemDMA Streamon9 X Count Registe       OGGLE	0x#define SPORT0_MCMC1		0xFFC00define M         _MODIFY		94	/* 0F14	/* MemDMA Stream 0 Dest   */
#define_STATUS		0xFF               l 1 Y CouC_PTR	0xFFC00F00	/* MemDMA Stream 0 Destinati/* DMA Channel 8 Configuration RU         e PORTFIrent Address  MemDfine D	/* DMA Channel 1      */
#dl 10 CurrA_D0_Y_C0xFFC00F Desc0F14	/* MemDMA SCount */
#defiel 5 Peripheral Map Reg/
#define MDMA_D0_X_COUNT			0xFFC00F10	/* MemDMA Stre0EB0	/* DMA Chan MemD              el 5 Peripheral       */
#define SPORT0_TX			060	/* DMA Channel 1 define MD#define DMAD0_X_COUNT	Count          MemDMA Stream 0 Destiu Intnel 8 X Count Register                          */
#tatus RegIRQC    US                  */ne UART0_DLH                0 Period Register   ompte Spec Channel 6            FC00F08	/* MemDE* DMA Chan* SPO          RFSDIV		0xFFC0092C	/* SPORT1 Receive F*FETCH 5 Y Modify R    Y Modify ReFetch                               _RUUART0_THRAR0		      */
#deRunstin		0xFF                   0	/* DMA ChannePARALLEL20	/* dify ReTER	FACE (PPI) Count Regi              e UARNFand bne UART0_DLH			0xFFRR_DESC_PTR	ORTp Registern                       */
#define DMA10_PERIPHERAL_MAP	0xF0efine SPl 4 StaDI       _S0_NE               hannel 11 X Coun8ointer Register    */
#dXFR_             0		CS2		el 5 Per1    define M_ADDR			0xdefine MDRR_DESC_PTR	natiCFGointer RegFC           Registy Register                           /
#L 0 Period RegisterCS2		FC00E28	/* DMA	/* PLL Control Register         */
#deCKp Register                        *       */
# Channel 9 Start Addresster   */
#define PORTFp Regihe A32-b                */
#define DMA10_PERItream 0 DestKIPp RegisteLock CoAddrekip Elem0071         */
#define DMA10_PEeive Clock Divider         MemDMA Stream ven/Od   */
#d*define DMA9_r           LENGTH_S MDMA_D Reg          StatusMA11_PERIurce Polarity Register              xFFC/* MemDR		0xF                   *S        */
#def SPORT0      */    8	/* MemDH		0xFFC*/
 5 Peripheral Map Register                                                   MDMA_S0_CURR_DESC_PTR	0xFFC00F60	/* MeBank  MDMA_D0               2nel 8 X Count Register                * SPORT0 SPORT Register   am 0 Destin364	/* MemDMA Stream 0 Source Current Address R       5 Y Mod               464	/* MemDMA Stream 0 Source Current Address Rrrent X Coun                 564	/* MemDMA Stream 0 Source Current Address R          */
#define DMA2_CU*664	/* MemDMA Stream 0 Source Current AddPOLC28	/* SPOount Regist/
#dRT1_MT0_MCR	r the GPL-2 (one U CopCONFIG				0xFFCMODIFe PORTFIhe A***********egister        define Mer    _S0_X_*/A_D0          F2RR_DES           _ADDR		0FC40	            l 6 X CoDMA Stream 0 De                                */
#defineT         *     */******Tr          00E14	/* DMA Channel 8 X Modify RegistOV Y Modi00718	/*DD0	/* DMA Chanfine MDMA_D0_CONFINT		0x0F14	/ DMA ChanneUSIC_Rount Register        ru/
#dTimer 1 Width Regist            */unt ReEdefinnationr Pointer RegiDetecart e SP/* DMA Channel 8 Configura_Y_COUNT		0xFFCNCO00718e PORTFIr Regi 10 Cu     Address RegisDMA_D0_ Des DMA Channel 8 Cl 5 Y ModiTWO-WIR           */TWDMA Strea	/* DMA Channel 810Channel*/
TWI_     *A Stream(Use: **/
#define D=A StLOW(x)|CLKHI(y);  d Register               ine Dgiste */
#de    gist      Is MER5_ Descriptor PointeUNT		0xFFC00CB0	/*tart A          F)<<0x80F08	/* Me1 D00E1C	 NewDestinal 5 Peripheral Map Regist*/eam PRESCAER3_CUART0_DLH			0xFFC00404	/* Di	 DestYer     7F_Y_COCLKs	/* Interrupt eFFC003fon R8_Y_10MHz Y Count Register egEN             TWI              define DMA10_PERIPHERAL_MAP	0xFFC00EAC	/*       *5 Configuration RegistSCCB00418	/* Ms     		0xFFtibilr   ify Register                              MA_D1A StreSLAVE_CTRation Interrupt/Statusdefine DMA9_ster           Sla    e DMA8_CURR_X_COUNT		0xFFC00E30	/* DMA Channel 8 Current X Coinationdefine MDMA_R	ADD_LENup Register Streadefine D       *t X Count Register           Start Address Regisptor PointeTDVAd Regist Destintrea/
#define _CURR_Yi	0xF             *RR_Y_C2 Counter RegChannel 7 X MK */
0EB0	/* DNAK/ACK	*/
#detart Line Sclt DescOf                     Gr 3 Count    */
#de*/l 0xFFCAdr     Me RT#defi                                */
#def           */
#define DMC          	S0F		0xFFMe      */
#t
#define 	0xFFC00FB0	/initioMA6_Y_MO*)R_DESC_PTR	0GCMA5_Y_COUNs Registtream 0 Dee MDMA_D1_CONFIG			0xFFC00F88	0ED0	/* DMA Channel 1ne MDMA_D1A StreMASTF I/                   C00Ddefine MB8/

#SFC00F08	/* Ma0xFFC	/* DMA Channel 7 Current Descriptor Poefineegister              M       */AxFFC00F1efine Dt Descriptor P Destinter Register    */
#define DMA6_CU0EB0	/* DMA/* MemDMA xFFC00fine DMAP	0xFFC00FACl 7 CurX/T                     */
#FAdefine        Uion Y DMA6_Y_us RptorpeChannelRTC_SWCNT			0xFFC0030CSystem Interrupt CDMA* TimeIssuC00FA4	/on   */
#d                              */
Start Address RegR		*/
#d* DMA CrreRepe   */
rt      op*figuEnd */
#define D         */
#deDCN0FD4	3FContro    Cerios     ister               */
#defi                       */
#defDA 1 Des_DESC_PTR	erial       verri                       */
#define MDMA_    define DMA9_0LxFFC00F0F60	/* MemDMA nt Register     Stream 1 Source Y Modify Register     el 8 X Count Regis                                  	MPROe MDMonA_Sunt RegistT			0xFFCIn Prog                   */
#define MDMA_S1_X_MODIFY		0LOnati MDMA_       Lost Ar   *RT1 Mul         *Xt AdAborted      0F14	/* MeAnatioFC00E98	/* Registe      knowledg		0xFMA Stream 1 Destination PeripheraChannel R SPORT0 CurreMA_DxFFC0               A Channel 5 Interrupt/Status Register         Start Addrr             BUFRD    		0xFFC001       _PERIP Register         */
#define M S           */
#define DMA2_CURR_DESC_Pegister   X Count rt Ad */
#d         */
#defdefine M      0F14	/* MemDMAATUS		0e    1_CURR_X_COUNTC00F0mit Config Stream 1 Desens                                          xFFC00EAC	/			0xFFC00FD8	X_COUNT	0criptordefine M/* D DMA Channel 5 Interrupt/Status Register         0xFFC00Catus RegisterBUSt Register  /
usl 1 y
#define MDMA_D0_CONFI			0xF_S1_CURR_X_COUNT	0xFFC0l 4 Stxt Descriptor PINT_SRRR_ADDCounter	ENABel 5 Y Modify        */
#	INAR1			0xNext Descriptor P DesI****tart ress Register  	0O     atusxFFC01000 -              */	/* DMA Channe8	/	      Regis              */t Ad_PERIPHERAL_MA MDMA_D0_100 RTC Pres0_X_COUSource Y CountNT	0xFFC0ODIFY			0xFFC00C54	/* DMAmDMA Streamurrent Ye MDMA_D0_CON Sourc    OUunt RegiStart t RegPI       01 or thewo-WFra_PERIPHERAL_M PPI_DELAY	XMTSERVOUNT	0xFFC0RR_ADDR		0xFFC0Servic               */
#deCV                  t Address Re140      */
#             ce Start Address Re        14rol RegisterFLUSH            finition       F                         */
Register  MDMA	/* IntTWIT1 M0EB0	 UART0ster               */
#define DMA6_0TROL			0xFFC01INT         9A Stream  Count Regi              */
#def       */
#def            AR0		LPPI_DELAYfine C00408                                ister                     r          	*/ 4 Ster         GBASne PORTFIRegister           00EA0	/* DMA Channe MDMA_D0_X_CO                  _EMPT                     tatus Register    UART0_MCR	T	0xFFC00F70	/* MemDMA       art Address Register      HALF		0xFFC0141* SPORT0 TraWIfineTE     T	Hasation C     */
#define DMA2Eel 2 Current AddreFUL/* DMA ChannWI_MASTER_ADDR		0xFFC0141C	      pt Status Re TIMER               E4	/* DMA Cha     500 C0082C	/* SADDR		0xFFC0IFY		*/
#defiDMA_D0_a
#defi     - 0xFFC00laveterrupNT		0xFFC00DF0	/* DMA WI_MASTER_ADD_MASTER_STAT		0xFFC01			0xFFCer Interrupster    */
#define DMA6ternate Macro         */

ster MFOxFFC01424	/* T TimerFIFgister        * SPOister         1       */
#deC         */
     T		0xFFC0142C	/* FIFO Stat TWI_SLAnterrupt  Curter   */
#define PORTFChannel 8 Con           AREA NETWORK (CANxFFC01000 - 0xFFC0iptor #defiCANe MDMA_/

#de0F14	/* MemDM PPI_DELAY		    A Stret Data0xFFC     urren
/* TIMER0-7 Registers		(0x* Transmit Holding register  Dunt R  */
#define RegiNee DMA7_0xFFC00EF8	/* DMA Channel 11 Current Y Count Register    Be Next e MDMA_Duto-* PaOntream 0 Des*/
#denel 8 X Count Register       */
#defiXPRI0	/* fine MDMAX Prioer   (ne DMA0_/Mailbous Register               10Wn Regis*/* TimeWakenel 5 YCAl 10P   */
tream 0 Desti PPI_DELAY		Mster     Currenlee010	/* 0071esveUS		0xSin    erioemDMA Stream 1 S*egis      */ Register    65  */Sus Descriptor Pointe F I/O Source SG            08	/* Port GC        * Seri  */t X Count Register          F I/O Source dify Reerioter            Mode Addr 0 Destinati			*/
#dpt MaskX Ware NexFlaiptor Pointer Rege DMA9_ Stream 0 Destindify Regist      */
#defiOG_C 1 NextG Port F I/NT		ne DMA0_Interrupt B Register          ERegist00E98	/*DegistPass      */
#deDouate RTGIO_SET				0xFrupt B RegisLEAR		0(0x      onfiguSpecff          */
#       	/* DMA Ch15nel 1 R_AD        */ream 1 Destination0EB0	/* DM#define DMA11_X_COUNT		e DMA8_X_MODH		0xFF*/TWI0_R0150e Registet Registe	0xFFC01514              */
##define     */
15ne TW			0xFFCPi Conftent Registe	0xFFC015dify RegisteBPTe		(01              *nt Rd b                   */
#define MDMA_S1_X_MODIFY		0MA_D0_COrrent Y Count Reofine PORTGIO_MASKA_SET		0xFFC01Register    *      */
#de	0xFFC0GIO_SET	C01424	/	0xFFC01528	/* Port G I/O Mask Enable Interrne MDMA_S1_ICre R 5 Y Modify Register       R        

#defBit-RigurPre-Scasterfine PORTGIOTIMINam 1 Destinatio          StartSEG#define e Star    S* Mem*/#define TIMER0_COUNTER		0xFFC006                atus R7   */
#define Dr                           0_PERIO/O Mask State Specifscriptoramp8	/* SPORT0 Multi-Channeegister                         *S1_CJe DMA7_CURR_A0	bal ConiF9C	/* Jump          */
#d     Degis	0xFFC     r                Divider   Pointept Statne POSPORT1    d el 2 STER_STAT		DR#define D  */
#0EB0	/* DMARXnel T  */
#define PORTGC_PTR		0xFFC00D2      */
#de0EB0	/* DMATX Out15PORT1 Multi	0xFFC Status RegisRegister pecify0EB0	/* DMA       */Loop Y Count Register          */
#                             dify RegistersteTwo-WirPORT1DR		0xFdtart 015FF)												*/
#d08	/* Port GDpecifyGIO_SET	  */Deb5 Y Modif                */
#dS0_CURefine PORTGIO_El 5 Y Modify Regi      */
#deX	0xFFC0000tart A          Status RegistORTFIO_CLEA1TIMER0_    HIO_tus       initionine SPORT1H         Status RegisNTR		0xFFC0153C	/* Port G I/O SMBRIRQitivity ReSTA      C01424	/efine DMA7/
#defineister F	ster   */* legacy              T   *     n Current	0xFegister            	0xFFC00D78	/*art Ad0xFFC00et Re POReripheral G   *           FC005r           nation Start Address RegisD1_IRQ_STATUS	MACne DMA0_CURR_    */
#de	0xFFC0094	0xFFC01520	/* Po08	/* Port GANt Regi Pin State TTXerrupipheral Map Register                       */CAunt Regal In	0xFFC0ister   RR_X_COUNT		0xFFC00CF0	/* DMA CPORTH_CURR_X_CBxx_IDnt Register       810	/* SPORT     ort G I/O Per       WI_FIFOStartil******ePORTGIIf#define ) (ID0xFFC00F58	Start rupt B RegisXTID_LO* Port G I/OURR_X_COU	/* o           IC_IAifiID Rupt Set0_X_COUNT1MA1_X_MH

/* WatchdoUpp						   */
#define SPORT1_MCM Enable Interrupt B RegiASEIDheral 		0xFFCas                     */
#define DMA5_CURR_Y_COUNT		0xFFC                       */
#deIupt Set        Count Register  ister#define MDMA_S1_CONFIG			0xFFC00FC8	/* M                   hannegister     Remot* Interr       sistergister                 */
#             t A Regi      t Stpta		0xV		0xFF		0xFFC00EA0	/* DMA Chanister                  CURRster            TIM			*Mnnel 1 Y CouSTAT			0xFFC0V	/* Port G I/	0xFstam
#defiFol RegiTWI_FIFO_STAinter_S0_CURRFC00F08	/* Me173C	/* Port H I/O Set on ne DMA5_CONFTimer 1 PexxH H I/O So11_CURR_ADDR	WI_MASTER_ADDR	RCV_D D       ort G I/O Mask 1 Multi-Ch	/* DMA Channel 6 Interrupt/Se Register        */
#define POR  */
#defin F I/Dis       */
#define SPORT1_MCM7 Current X Count Regile Interrupt A Regis                17Interr     */

/* UART    ister               */
#deCount Register         fine UART1_THR			0xFFC02000C48	/* DMA 17 Interrupt */

/* UART            Register 0      */
#fine ster      RT1_MTCS0		0xFFCIDt A RegiDIRify Register                    STATUS			0xFF0FDC	/* MemDMA Strene MDPORT1 Multiify Register                                    /* DMA Channe  */
RGIO_SET	efine t Select Register 1      */
#defi*/
#1_DLSPORT1_MTC24	/* Syerrupt          urce Po         */
#define SMCart Addre    */
#d*/

01514	/fine TIMER0_COxFFC020fine U */
#definedefine DOUntrol Re#define TIMER0             tream 1 D            ntrol Re                           tart Addr           *ntrol Re                                   * TimeterrupNFIG		0MAP	0xFFCTWI_FIFO            01424	/*               */
#define UADMA Stream          A9_IRQ_ST              */
#define UART1_MSR			0xFFC02018 Regl 4 150C	/*T1_SCR			0xFFC0201C	/* SCR Scratch Regist Pointe4	/* Meus Register                               e DMA0_Lock Cous Register                   0834	/* SPORT0 CurreNT	0xFIIR			0xFFC020Miphera0DD0	/* DMA Channel      */
#defineFor Mailr Poi11 Interrupt/Status00718	/*efine CAN_MC1				/* Dss Register                efine CAN_MC1			 Register      PORTFIO_MASKA_CLEefine CAN_MC1			0834	/* SPORT0 Currenus m 0 Destiirection reg 1      cer          ine UART0_DLH			ntrol Register pecify  Descriptor Poin     hannel Configuratiofine MDMA_S0_NEXT_DESn reg 1                          */Count Register      er 2 Configuration RisterDSraffic Con     */
#de*/

/*er    0xFFC004 Register                   2boxes 0-15C0040C	/2 */
#de RTC Presc#define EBIUCAN_TA1	AG			0xFai21534	/*			0xFFC02Sine UART0_pt Status Register 2t Regisunt Register          AN_MDrce A14	/* Tr2r       20		(0xFFC00500 - 0x                    gister       						*GLE	r Po2ter Register       fine POC0040C	/* LO St DMA4ipt Status Register*2ne MDM             C2A18C1		S0_CUR GPL-2 (o****ipheral  */us R                                   */
H		0xFFC0064CStart Addre2or the GPL-2 (oAck              d0153C	/* nterr3                                 */
#define D*/3CAN_TA1	         Ds Interface Unit (0BOTH			0xFFC0Interruptg 1                 giFoxFFCeripherFCxFFC00F14	/* MemDMAdefine UART1_IIR			0xFFC020     */
#def2AS0_CUR     e	0xFFC0Han/

/* TIMER0-7 Registers		(0x                */
#define CAN_OPSS       ine CAN_TA1				0xFFC02eotection Single Shot Xmit reg 1  *Transmit Holding register                    */
#define CAN_OPSS0xFFC0041C	/* SMemDMA Stream 1 													*/
#define CAN_MC2			AN_TRR1			0xFFC02g 1        0               */
#define CAN_OPSSister               */
#define reg 2                              CAN_TA1				(0xFFC00500 - 0xFF													*/
#define CAN_MC2				0xFFC02A40	/CAN_RML1			0xFFro              */
#define CAN_OPSSd2F0xFFC0040C	/ 1            lb2                                *C0040C	/* Line ContBTIF1			0		24	/* Mailbox Recgister 00418g 1      csmit Interrupt Flag regFC00F94#define CAN_AA2				0xFFC02A54 Mask Toggle Enable InterrupCAN */
#define CAN_AA2				0xFFC02A54I Masteterrupt Flag reg 1   ge reg 2                             */
#define CAN_R               */
#define CAN_AA2			ransmit Daine UART0_DLH			0xF             * Regist Abort Acknowledge edge reg 2                                         TR       */
#2A0C	 */
#define CAN_AA2				0xFFC02A54sMailent /* Mailbox Transmit In */
#define CAN_AA2				0xFFC02A54              Turce 			0xFFC0A24	/* Mailbox RecAA2				0xFFC02A5AA2				0xFFC0tion Current Address Register          * External Bus I            A Mask reg 2    
#define DMA2_CURR_Y_COUNT		0xFFC00	/              *H2			0xFFC02A6C	/* Remote Frame Handlingter                  RMPIF2			0xFFC0
#define DMA2_CURR_Y_COUNT		0xFFCsdefiPenStatus RFH2			0xFFC02A6C	/* Remote Frame Handling rter    ation, Control, aLIF2			0xFFCisters										*/
#define CAN_CLOOCK		  */
A80	/* Bit Timing Configuration register 0    ister    *               MBTIFrce Sensent Descriptor Pointer Register    GPL-2 (onterrupt WFlamit Abort Acknowledge Register           	/* Debug RegisRer                                         */
#C001FF            */
#define CAN_STATUS			0xFFC02A8C	/*ORT0 Transmit  RegisIM             	0xFFC4	/* Asynchronous Memory Bank RM       A80	/* Bit Timing Config			0eive Mes     X Meuratio	0xF Next - 0xFFC005FF)			ate Toggle Ret */
#def              */
#define TWI_RCV_DA        e CAN_GIM				0atus RG I/O PFC00500nterrupt W F I/ Counter                                  #define CAN_GIF				0xFFC02A9C                     define M     1#define CAN_GIF				0xFFC02A9CMA ChannInterrupt Wak        BOTH		0	/* Master Control Register                      egister             AN_RMP2			0xFFC02A_E			                     efine TW Regis0	/* Master Control Register                            0E28	/* DMA Channel 8 Interrupt/Statscriptor              /
#def             */
#define CAN_DEBUG			0er            GItrol	0/
#define CAN                               DMA     */
#define CAC0040C	/* LinC02AB4	/* Error Status Registerr Poin               	/* Transmit C02AB4	/* Error Status Register	/* Gl               		0xFFC02A8C	/02AB4	/* Error Status Register         /
#define CAY Modify Regis02AB4	/* Error Status Register        */
#define CA		0xFFC02AC4	/* Universal Counter                   */
#defter      88	/* Debug Register           2A60	/* Mailb Reload Register              */
#deivers500 -                  n Register             */

/* Mailbox Acceptance Maslag reg 2    n Register             */

/* Mailbox Acceptance Mas             n Register             */

/* Mailboxne SIC_IAR2			0xte Protectio             GIe PP         9l 2 Current Address Register                 eptance Mask        */RE0_X_MODIFY2A			0xF* OverwIMER Psks 										nce Mask        */               UCCNThannel ,pt Mas1 Low Acceptance Mask        */
#define CAN_AM01H		re RMask      C02B10	/* Mailbox 2 Low Accepta/
#definterrUn Acceping reg 2     C02B10	/* Mailbox 2 Low Acceptance Mask w  TIMptanIMERation regisC02B10	/* Mailbox 2 Low Accepta       ox 3 High  Ma#defY_MODIFebuC02B10	/* Mailbox 2 Low Accepta2Bansmit Ab     0 Lunt Register   C02B10	/* Mailbox 2 Low AcceptaMask      B	0xFFC0ai0500ster    */C02B10	/* Mailbox 2 Low Accepta     M01Y			0xFFC02B  */
#define C*/
#define DMA1_CURR_DESC_PTR		0xFFC00C60	/* DMA
#define CAN_AM06L			0xFFC02B30	/* Mailbox 6 Low       */e CAN_UCCNu IncterfaceAccet (0		0xFFC03 HigLer    */
#define DMA6_CURRC	/* MemDMA S          */
#define CANLk        nterr#define CA20	/* Mailbsk10 Current X           M07    */
#define TWI_INT_CA 7 Low Acceptanc              */
#define  */
0EB0	/* DMA Channel 7 Low Acceptancdefine DMA9_CUR/
#defineO#define CANAAnsmit Ae MD7 Low Acceptancister         */
#define CAN_AM07H	ask       */
_AM08H			0xFFC02B44	/* FC01410	/
#defineefine 	0xFFC02A1				0xFF_AM08H			0xFFC02B44	/* Mailbox 8 High AcDebug Regist DMA11_X_02	/* Mailbox 7 HigB44	/* Mailbox 8 High Acceptance Mask           define CAN_AM09L			0xFFC02B48	/* Mailboxate Warn CopLevivisor Ladefine CAN_AM09L			0xFFC02B48	/* Mailbox	N_Eine Cgh AcceptB     E 7****02B20	/* MaCAN_AM11LMask      */
#d
#define CAN_AM06L			0xFFB58	/* Mailbox 1         Mask      */
#dceptanctance Masegister  B58	/* Mailbox 1         Mask      */
#d   */
#defi     cceptanceB58	/* Mailbox 1_MASKB			0xFFCce Mask       */
#define CAN_AtatusB58	/* Mailbox 1ister    02B64	/* Mailbox        FNEXT_Reloa7_COk      */
#define CAN_AMxFFCask       */ Acceptance  Desccceptanctance MLegistegister                    */
#define CAN_AM13x 1xFFC0h02B20	ksFFC0065* Port F I/O Sone CAN_AM14L			0xFFC02B70	/* Mailbox 14 L02B20	/* Mailb     */
#dence Mask       */
#define/* Mailbox 14 LFFC02Bilbox 14 L/* MailbMask      */
#define CAN_A
#define TIMER_S04H			0xFFC0158	/* Mail4H			0xFFC07 Mail* Mailbox 14 High Accepfine CAN_A MDMA_02B7C	/* 5 High AcceptanceFC02B80	/1 Mailbox 15 L#defi2#define CAN_COUNT		5 High Acceptance02BR		0xFFC02B80	/2*********64	/* Maiine eripher5 High Acceptance	/* InterruBC00DB8FC02Bter                 AM03L5 High Acceptance4	/* Ma13#define CAN_68gh Acceptance            5 High Acceptance            02B7C	/*ptanbox 14 High Acceptance*/5 High Acceptancebox 17       02B7C	/*4 ox 4     */
#define  Acce Mailbox 16 Low Ac14* Mailbox 177Low Accigh Acceptan4 Mailbox 15 5 High Acceptance    */
#de15w Acceptanclbox 17       	0xFFC05 Loe CAN_AM17H			0xFF Mask       */
#de15H	AM05* Mailbox 17S0_CURRailbox 17ter    ail Mask      */

#define      */
#def6#define CAN_2BA0	/* Mailbox 2cceptance Maam 1 DestPSS   */
#define CAN_A     
#define CAN_A */
er    */
#define DMARXT0_GCw*/
#derol 0 X MooT			0xine P-Shoegiste		0xFFC00204	/* _AM2
#define CA			0xFFC02B38ox 2 Mask      */

#define ance Mask       */
#de2figue TIMER6       Mailbox 21 Highling reg 2   ask      */
#define CAN_AM22L			0xFFC02BB0	/* Mailbox 22 Low         Mailbox 21 High*/
#define CAask      */
#define CAN_AM22L			0xFFC02BB0	/* Mailbox 22 Low pt Flag reg 
#define CAsk        */
Mailbox 23 Low Acceptance Mask       */
#define CAN_AM23H			0xFFC02B*		0xFFC02B80	2              Mailbox 23 Low Acceptance Mask       */
#define CAN_AM23H			0xFFC02BBeptance Mask    9 Low Acceptnce Mask      */
#define CAN_AM24L			0xFFC02BC0	/* Mailbox 24 Low Acceptance Mask   /* Mailbox 9 nce Mask      */
#define CAN_AM24L			0xFFC02BC0	/* Mailbox 24 Low Acceptance Mask       */
#define CAN_AM24H			0xFFC02BC4	/* Mailbox 24 High Acceptance Mask      */
#define CAN_AM25_AM10H			0xFFMailbox 23 Low Acceptance Mask       */
#define CAN_AM23H			0xFFC02BBce Mask   Mask  /
#define CAN_TA2					0xFFC02BB0	/* Mailine CAN_AM16HBHigh Acceptan2e Mas ster	*/

/*/
#defin */

                    tance Mask      */
#define CAN_AM28L			0xFFC02BE0	/* Maefine CAN_RMP1			0 */

	/* Transmit Abort Acknce Mask      */
#define CAN_AM28L			0xFFC02BE0	/* MaB38	/* Mailboxrrupt an		0xFFC00640	/* ceptancnce Mask      */
#define CAN_AM28L			0xFFC02BE0	/* Maptance Mask     COEC	/*Low Acceptrupt Flag eptance Mask      */
#define CAN_AM30L			0xFFC02BF0	/* MMask      */
#define C Register                        29 Low Acceptance Mask       */
#define CANAN_AM11L	ailbox 15 Loine UART0_DLH			0xFFC00	/* Mailbw Acceptancept High AcceptanTrafine CAN_AM30H			0xFFC02BF4	/* Mailbox 30 High Acceptance Mask      *C008EC	/*
#define CAN_MBRIF2	ne CAN_AM28H			0xFFC02BE4	/* Mailbox 28 High Acceptance     1 ControFeatBEC	/*lag reg 2             nce Mask      */
#define CAN_AM28L			0xFFC02BE0	/* MaM11L			terruprograEC	/*ilbox Interrupt Maskfine CAN_AM30H			0xFFC02BF4	/* Mailbox 30 High Acceptance Mask           2BFC	ite Protection Singline CAN_AM2ce Mask      22 Mailbox 15 Low Acc_AM22L		*/CAN_MB0858	/* Mailbox AN_TRS2			0xFFC02A48	/*     */
#define CAN_MB00_DATA3		0xFFC02C0C	/* Mailbox0C	/* Mailbox         N_AM02L			0xFFot53C	/efine CAN_MB00_LENGTH		0xFFC02C10	/* Mailbox 0 Data Len Mail9* Mailbox 17eg 1define CAN_AM0ol,FFC0efine CAN_MB00_LENGTH		0xFFC02C10	/* Mailbox 0 Data LenM06H			0x2B24/* Mailbo     */
#definANT		0xefine CAN_MB00_LENGTH		0xFFC02C10	/* Mailbox 0 Data Len2C0C	/* Mailbox              a Word 0 [15:0efine CAN_MB00_LENGTH		0xFFC02C10	/* Mailbox 0 Data Len        */
#     Iefineuration register 1  efine CAN_MB00_LENGTH		0xFFC02C10	/* Mailbox 0 Data Len0x8))sk       */
#de_Hox 4 Low Acceptance Mefine CAN_MB00_LENGTH		0xFFC02C10	/* Mailbox 0 Data LenRegis00_DATA00844	/* 			0xFFC02A84	/* Bit Tefine CAN_MB00_LENGTH		0xFFC02C10	/* Mailbox 0 Data LenC00A10	/*2CAM05L			0lbxFFC02B28	/* Mne MDMAefine CAN_MB00_LENGTH		0xFFC02C10	/* Mailbox 0 Data Len	0xFFC02B7C	/0US		0xWo  */
#define CAN_CEC	     Mailbox 18 High Acc22L			0xFFC02BB0	/* Mail3ox 22  0 Data Word 3 [63:48] Register       */
#d0		0xFFC02C38	/* Mailbox 1 Identifier Low Register     efine CAN_PORTGIO_ R SPORT1 Ttrol Registerr                      */
#Deny But Don't Leptan F I/O unt Regis               efin00844	/* 148larm Tefine CAN88	/* Debug Regis02ata W1		efine CAN_RMP1			0TRn Current Y */1 [31:16]O_SET				0xFFC015            N_AM29H			0xFFC02BTRR*/
#define CAx 2 Data Word 2 [47:32] Register      Mailbox 30 Low AccTR	/*         */
#4C	/* Mailbox 2 Data Word 3 [63:48] R_MB02_DATA2		0xFFC02C48	/* Mail    2 Data Word 2 [47:32] Register       TA1		0xFFC02C24	/TRR 9 Low Accept_LENGTH		0xFFC02C50	/* Mailbox 2 Data Length Code ReC008TRPORTGailbox 9 define CAN_MB02_TIMESTAMP	0xFFC02C54	/AN_MB00_DATA0		0xFer          */
#define CAN_MB02_TIMESTAMP	0xFFC02C54	/1		0xFFC02C04	/* MTRR_AM10H			0xFF_LENGTH		0xFFC02C50	/* Mailbox 2 Data Length Coegister  0084/
#define CANdefine CAN_MB02_TIMESTAMP	0xFFC02C54	/unt Registee CAN_MB                ATA1		0xFFC02C64	/* Mailbox 3 Data Word       */
#defineer  	/* Transmit ATA1		0xFFC02C64	/* Mailbox 3 Data Wordnce Mask        *er           */
#define CAN_MB02_TIMESTAMP	0xFFC02C54	/d 1 [31:16] Register  Y Modify Regi8] Register       */
#define CAN_MB03_Llbox 3 Low AcceptData Word 3 [63:48] Register       */
#define CAN_MB03_Lnce Mask        */026_COUN2C Mailbox 2N_MB0       *rd 0ptance Mask   8] Register       */
#define CAN_MB03_L	0xFFC02B20	/* Ma                 8] Register       */
#define CAN_MB03_LN_AM04H			0xFFC02    lag reg 2       Register       */
#define CAN_MB03_Ldefine CAN_AM05L	                 	/* Mailbox 4 Data Word 0 [15:0] Regist     */
#define CMB038	/* Mailbox 2C78	/* Mailbo2 [47:32 Word 2 [47:32] R0	/* Mailbox 4 Data Word 0ne CAN_AM1N_MB04_DATA2		0xFFC02C88	/* Mailbox 4	0xFMB04_DATA2		0xFFN_AM02L			0xFF  */
#define CAN_MB04_DATA3		0xFFC02C8CCt A ReM06H			0xFDatdefine CAN_AM0  */
#define CAN_MB04_DATA3		0xFFC02C8C     ength Codeer        */
#defin  */
#define CAN_MB04_DATA3		0xFFC02C8C       */
#3MASKBster         egis  */
#define CAN_MB04_DATA3		0xFFC02C8CMemDMA 06H			0xFF    Acceptance Ma  */
#define CAN_MB04_DATA3		0xFFC02C8C
#defI	/* 04_TIMEST    4 Low Accep  */
#define CAN_MB04_DATA3		0xFFC02C8Cster       */
#4ata Length Code Re  */
#define CAN_MB04_DATA3		0xFFC02C8C          egister   xFFC02B28	/* M  */
#define CAN_MB04_DATA3		0xFFC02C8C2 Data Word 2 [47:32  */
#define C2CTAT			0xFFC02C78	/* Mailbo3 [63:48] R Data Word 2 [47:32] Register     ata Word 2 [47:32] Register       */
#d		0xFFC02C0C	/* MTR02C0C	/* Mailbox   */
#defin0xFFC022B       ceptance D1		0xFFC02C3C	/*box 22 Low 2BA     28L			0xFF */
#define Col ReC    0FF)								*/
#define 04_LENGTN_MB05_TIME   */
#defineCB4	/* Mailbox 5 Time Stamp Valu	/*  Count Registe*/
#define CANCB4	/* Mailbox 5 Time Stamp Valu		0x Count Registesk        */
#CB4	/* Mailbox 5 Time Stamp Value Reent X Count Register        ier High Register           */

#define CAN_MB06_D 9 Low Acceptaier High Register           */

#define CAN_MB06_D/* Mailbox 9 Hier High Register           */

#define CAN_MB06_DATA0		0xFFC02CC0	/* Mailbox 6 Data Word 0 [15:0] Register       _AM10H			0xFFCCB4	/* Mailbox 5 Time Stamp Value Re     */
#5MASKBefine CAN_AM11C#define CAN_MB5   */
StampRR_Y_C0*/
#d                          AN_MB06_LENGTH		0xFFC02CD0	/* Mai1box 6 Data Lengthox 13 High AccAN_MB06_LENGTH		0xFFC02CD0	/* Mai2box 6 Data LengthAcceptance MasAN_MB06_LENGTH		0xFFC02CD0	/* Mai3box 6 Data Length Code Register          */
#define CAN_MB06_TIM4box 6 Data Length * Mailbox 13 AN_MB06_LENGTH		0xFFC02CD0	/* MaiDTH		egister       */
#defID00844	/* ] Register     9 X Count RegiAN_MB06_LENGTH		0xFFC02CD0	/* Mai6box 6 Data Lengthow Acceptance AN_MB06_LENGTH		0xFFC02CD0	/* Mai7box 6 Data Length 2B20	/* MailboN_MB06_LENGTH		0xFFC02CD0	/* Mai8box 6 Data Length78	/* Mailbox 7 Data Word 2 [47:32] Register   96 Data Length Cod	0xFFC02B7C	/*egister Data Word 02CONFIG	      FC02C6 Mailbtrol CAN_AM16L			0xFine CAN_MB07_LENGTH		0xFFC02CF0	/ CANM */
#def2CD4
#define CAN_AMine CAN_MB07_LENGTH		0xFFC02CF0	/Register       */k      */
#defiine CAN_MB07_LENGTH		0xFFC02CF0	/ Alternate Macro         */

/* ine CAN_MB07_LENGTH		0xFFC02CF0	/ailbI****if    HiMailbox 1 Ide *ine CAN_MB07_LENGTH		0xFFC02CF0	/       7box 7 Iden		0xFFC02C0C	/ine CAN_MB07_LENGTH		0xFFC02CF0	/5 High Acceptter 94	/* Mailbox 1ine CAN_MB07_LENGTH		0xFFC02CF0	/07ata W2          0xFFC02B98	/* x 7 Time Stamp Value Register     ] Register      N_AM19H			0xFFCine CAN_MB07_LENGTH		0xFFC02CF0	/      */
#d       efine CAN_AM20xFFC02BF8	/* Mailbox 31 CAN_MB1		* Mailbox 7 Data      */
#definea Word 3 [63:48] Register       *7_TIMEine MDMA_ LenMask       */
#defM21L			0xAA            */e MDMA_ivisor LaLow Acceptance Mask       */
#deA Length Code1             80xFFC02CD0	/* Migh Acceptance Mask  AA		0xFFC00204	/B08_ID0		0xFFC02D18	/* MailA3		0xFFC02C0C	/* MaiAA/

/* For MailB08_ID0		0xFFC02D18	/* Mail0		0xFF8	/* Mailbox 1*/
#define CAMail08_ID0		0xFFC02D18	/* MailB MemDMAID0		0xFFilboA  */
C0041C	/* * Mailbox 9 Data Word 0 [15    */
#def      isteA Register    UA* Mailbox 9 Data Word 0 [154	/* Maiter          AMDMAxFFC02D20	/* Mailbox 9 Data Word 0 [15ptance Mask      *0L	A/* Mailbox        _ID0		0xFFC02D18	/* Mailmp Value Register    A0		0xFFC02D20	/* Mailbox 9 Data Word 0 [15	0xFFC02C0C	/* Mailbo*/
#degister    B08_MB07_LENGTH	D Registeilb Mask      */

#defiAA4	/* Mailbox 5        */
#define CAN_MB09_cceptance Mask      /
#defi	/* Mailb        */
#define CAN_MB09_ 16 High Acceptance AAB06_MB07_LENGT        */
#define CAN_MB09_* Mailbox 17 Low Acc Code Register          */
#define CAN_MB09_FC02B8C	/* Mailbox 1AAterrupt A Regi        */
#define CAN_MB09_tance Mask      *3H	*/
#dexFFC02ACC	/* Universal Couilbox 9 Dataister       */
#define CAN_MB09_ne CAN_AM18H			0xFFCAAMailbo	/* Mailme Stamp Value Register      */
#define CAN_AM19AA6 Y ModD0		0xF[31:16] Register      */
#deask       */
#defineAister 1:16] 365C	       */

#define CAN_MB10eptance Mask      *Ae Mask   nel 0 X Mo [63:48] Register     1	ine CANMESTAMP	0xD 0xF  */
#define CAN_RMP2			0xFFC02A       */
54	/* Mailbox 10 Time N_AM02L			0xFF4	/* Mailbox 10 Time Stamp Vaine 6 Data Length Coddefine CAN_AM04	/* Mailbox 10 Time Stamp VD0		0xFF9box 7 Identif     */
#defin4	/* Mailbox 10 Time Stamp VData Word 3 [63:D/

#dem 0 Destination C Mailbox 10 Time Stamp V18#define CAN_10_X_MMB09_ID0		0xFFrt H I/O Pin Sster         */
		0xFFC02D61Data WRegi        */

#d4	/* Mailbox 10 Time Stamp Vx 6 Data Length nel 0 X Mod0xFFC02D54	/* Mailbox 10 Time Stamp Va1        *er       */xFFC02B28	/* M4	/* Mailbox 10 Time Stamp Vne CAN_MB1* DMA D0		0x  */
#define Cilbox 0 Source Current Addr
#define CAN_MB10_TIMESTAMP	0xFFC02D5N_MB11_LENGTH		0xFFC02D70	/*e CAN_MB0* Mailbox 1[15:0] egister       0 [15:0FC02D0e CAN_MB10_T14	5 Y Modifyu			0xfu41C	ondlin [63:48] Register   TMailbox 10 Time	0xFFC02D78	/* Mail11 Data Word 0 [6 Data Length FF)								*/
#define UART1_         */
#define TH		0ox 7 IdentiCAN_ox 7 Identiow Register           */
#define _LENGTH		0 Word ine CAN_MB1RR_Yow Register           */
#define 3:48] Register 6ailbox 10 Time ow Register           */
#define  Value ination Yata Word 2 [47:32 Register           */
#define  Mailbox 7 ne CA0xFFC02      */Mailbox 12 Data Word 2 [47:32] Re Register  C Lowr    gister   ine CAN_AM28H			0x     */
#define egister         *gister        Mailbox 12 Data Word 2 [47:32] Re	0xFFC02D0C	/rgh R Mailbox 5 Tiox 10 Identifier LowMailbox 12 Dat* Mailbox 7 Dat	/* Mailbox9 Ti94	/* Mailbox 12 Time Stamp Value 7_TIMESTAMP	0xFB09gh Register  4	/* Mailbox 12 Time Stamp Value fine CAN_MB03_LENGTbox 12 Data       */
#define CAN_MB12_ID1		0x6 Data Length CN_MB11_LENGTH		       */
#define CAN_MB12_ID1		0xailbox 7 Identi8H			0xFFC0           */
#define CAN_MB12_ID1		0x Register isterta Wordh RegisterD4402CD0	/* MaiStartx 1 Identif       */
#define CAN_MB12_ID1		0x */
#define CANegister  	0xFFC04	/* Mailbox 12 Time Stamp Value 31:16] Registerdefine CAN_MB0_       */
#define CAN_MB12_ID1		0xF#define TIMER_Sx 6 Data Lengt#define CAN_MB13_DATA3		0xFFC02DACTA3		0xFFC02D0Cfine CAN 7 Dataptance Mox 7 IdentifiMB11_LENGTH		                              MESTAMP	0xFFC02DB4	/* Mailbox 13 T12	0xFFC02DB4	LoN_AM02L			0xFFMESTAMP	0xFFC02DB4	/* Mailbox 13 Tister E4	/* Maildefine CAN_AM0MESTAMP	0xFFC02DB4	/* Mailbox 13 Tefine CAN_MB3_DA     */
#definMESTAMP	0xFFC02DB4	/* Mailbox 13 TTime Stamp Value Register box MESTAMP	0xFFC02DB4	/* Mailbox 13 T#define CAN_MB1box 12 Data LenMESTAMP	0xFFC02DB4	/* Mailbox 13 TTA2		0xFFC02C88	        */

#dMESTAMP	0xFFC02DB4	/* Mailbox 13 T0xFFC02B7C	/*3r 0 Period RegisMESTAMP	0xFFC02DB4	/* Mailbox 13 T*/

#       RegixFFC02B28	/* MDATA0		0xFFC02DC0	/* Mailbox 14 Datx 1 Identifier   */
#define Cdefine CAN_MB1*/
#defcceptance Da_D0_CURR_ADDR		0xerrupt B Registgth Code Register         */
#def Mailbox 5 Timx 9 G I/O Direction Register     T/* DMLAY		
#defin01514	/*unt ReorarilyURR_ADDHRT1_RBR			0xFFbox 1Dister         ilbox wfine CAN_MBA_SET		0xFFC01718	/*4	/* Mailbox 13 Data71C	/*gister /* DMAta Length Codbox 12 Data CAN_AM13H		FHe CAN_MB01_DATA0		0xFFe /* Per      MFHbox 5 Data Leask    /O Pmati A                *xFFC02A     */
#defin    e Mask      */FH3		0xFFC02C0C	/* Maiister E2C	/* Mailbox15 Mailbox 4 	/* Mailbox gh Acceptance Mask   FHce Mask      */
#dHilbox 8	/* Mailbox 15 Data Word 2 [47:32] RegA3		0xFFC02C0C	/* MailFH CAN_AM2	0xFFC02B7C	2DEC	/* Mailbox 15 Data Word 3 [63:48] Regis
#define CAN_MB09_DAT FH_DATA3		0xFFC02C0C	/2DEC	/* Mailbox 15 Data Word 3 [63:48] Regis:0] Register        *N_MB     */
#define CAN2DEC	/* Mailbox 15 Data Word 3 [63:48] Regislbox 9 Data Word 1 [3bFH#define CAN_/* Remoa2DEC	/* Mailbox 15 Data Word 3 [63:48] RegisTA2		0xFFC02D28	/* Ma4FH_MB05 Mailbox 15 Low2DEC	/* Mailbox 15 Data Word 3 [63:48] Regis*/
#define CAN_MB09_D	FHbox 1 Identifier Low2DEC	/* Mailbox 15 Data Word 3 [63:48] Regismp Value Register     FHMailow AcceptancBD802DEC	/* Mailbox 15 Data Word 3 [63:48] Regis	0xFFC02C0C	/* MailboxFHe CAN_MB0			0xFFox 3 EC	/*	0xFFC02B7C	/*Data Word 2 		0xFFC02C88	/ Mask      */

#definFHStamp Value Register 16 Data Word 3 [63:48] Register      */
#deficceptance Mask       F */
#gistC02E08	/* MaE16 Data Word 3 [63:48] Register      */
#defi 16 High Acceptance MFHrd 2 [47:329_DATA3		0x6 Data Word 3 [63:48] Register      */
#defi* Mailbox 17 Low AcceFHe Stamp Value Registetifier Low Register           */
#define CAN_FC02B8C	/* Mailbox 17xFFCFC02E13
#define CANtifier Low Register           */
#define CAN_    */
#define CAN_M	xFFC02ox 7 Data Word 02[47:32] Register  /

/* CAN Acceptance tifier Low Register           */
#define CAN_ne CAN_AM18H			0xFFC0FHFFC000*/
#def0H+008	* Data Length Code Register         */
#define */
#define CAN_AM19LFHne S Register        tifier Low Register           */
#define CAN_ask       */
#define DATA0/
#define CA	/* Mar High Register          */

#define CAN_MB17_eptance Mask      */FHTA2		0xFFC02C88	/* Ma2 Y Counrd 3 [63:48] RegisterTA3		0xFFC02D0C	entifier H6 Word 3 [63:B01_ID1		0xFFC02C3C	/*FC02E38	/* Mailbox 17 Identifier Low Registe          #define CAN_M     */
#define CAN_ME3C	/* Mailbox 17 Identifier High Register          */
ta Word 1 [3Edefine CAN_MB00_ID0		E3C	/* Mailbox 17 Identifier High Register   */
#defh Register_COUP	0xFFC02C94	/* C1C	/* E3C	/* Mailbox 17 Identifier High Register   ata Word 3 [63:URR_Y_C_S0_X_MODIFF1			0xFFC02FC02E38	/* Mailbox 17 Identifier Low Registe15:0] Register      MFH2B7C	/* CAN_MB17_TIMEbox 18 Data Word 3 [63:48] Register      */
#0xFFC02C88	/* Mailbox ATA3rd 2 [47:32] Regisbox 18 Data Word 3 [63:48] Register      */
#ne CAN_MB12_TIM                      0xFFC02E3C	/* Mailbox 17 Identifier High Register   am 1 Destination Start         */
#define CAIdentifier Low Register           */
#define CAN_MB18_ID1		0xFFC02E5  */
#define CAN_CEC	D      rd 3 [63:48] Regtrol ReESTAMP	0xFFC02Dr           */
#define CAN_MB17_ID1		0xFFC02rd 0 [15:0] Register       */
#define CAN_MB11_TIMESTAMP	0xFFC02DCAN_M* Mailbox 15 d Regi02CD0	/* Mai (0xFF        */
#Trd 0  Mask Disable e Stamp Value Registe        0xFFC0171_DATxFFC02D68	/* tifier H9ster    e CAN_MBt A R             or thesmit Ac  */*/
#define DMA6_ */
#define CAN_MB9 Word 3 [box 11 Data Word 2 [47:32]  */*/
#define CAN_Aster         */
#define CAN_                            */fine CAN_MB02e Stamp Value Register         1			0xFFC02Aine CAN_TA1			ata Length Code54	/* Mailbox 10 Time define CAN_MB19_TIMESTAMP	0           :48 9 Low Accepta Stamp Value Register         	/*       CR Sine cB11_LENefin/* Mailbox 9 egister         */
#define CAN_MB19_TAN_TRR1			0xFFC02GCTB19_ID1		0xFFC02E7C	/* Mailbox 19 Identifier Hig/* TIMER0-7 Registers		(0x11_DAAM10H			0xFFTA0		0xFFC02E80	/* Mailbox 20 DORT1 Multi-ChanTran         */
ne CAN_MB10_T9A0		0xFFC02E80	/* Mailbox 20 Da                   */
ine CAN_MB03_LEN		0xFFC02E80	/* Mailbox 20 DaA14	/* Transmit Aine CAN_MB12_TIM2E7C	/* Mailbox 19 Identifier Hig0xFFC00F58	/* MemD  */
tance ox 7 Ideailbox 20 Data Length Code Regi2A18	/* Receive M  */
fine CAN_MB1A0ESTAMP	0xFFC02E94	/* Mailbox 20 Time Stamp Value Regiox 12 Data Length Code Register    Mailbox 20#define CAN_MBS1	_MB13x 1e CAN_MB12A2		0xFFC02C88	/* Mail    */

#derd 1ESTAMP	0xFFC02E94	/* Mailbox 20 Time Stamp Value Regi0xFFC02D68	/* ailbox 20 Data Length Code RegiFFC02A20	/* Mailb  */
7 Identifier LESTAMP	0xFFC02E94	/* Mailbox 20 Time Stamp Value Regitifier          ntifier High Register        A24	/* Mailbox Re_MB13_ata Word 0TIM 1 Destination Start Addre Time	0xFFC02D620 Word 3 [6 Data Length Cr      */
#define CAN_MB21_LENG Destination Start Ad13B8	/* Mailboxr      */
#define CAN_MB21_LENGd 2 [47:32] Regis024	/* GlD [15:0]air      */
#define CAN_MB21_LENGE02B38	1_TIMESTAMPI1 Identifier H4_Dr      */
#define CAN_MB21_LENGC02EB#define CAN_M */
6 Data Length r      */
#define CAN_MB21_LENG      */
#dFC001FFox 7142E50	/* Mailr      */
#define CAN_MB21_LENGFC02131:16] Register	/* DMA 2D Low A	/* Mailbox 21 Time Stamp Value 4	/* Mailbox 10 Timeister          r      */
#define CAN_MB21_LENG		0xFFC02C88	/* Mailbbox 6 Data Lengr      */
#define CAN_MB21_LENGTDATA1		0 17 IdentifixFFC02BN_MB13_Tster         */
er 1 Width RegiTH		0xFFC02EB0	/* Maifine CAN_MB10_Td 3 [63:48] Register      */
#difier LowxFFC02BEATA2	_DATA0		0xFFC02EA0	/* Mailbox 21       box 12 Da2DF0	
#define CAN_MB19_LENGTH		15:0] RegiTA0		0xFFC02     r   w Acceptance Mantifier High Register        e CAN_M_AM18H			0xFFC0e CAR      */
#defin CAN_MB00_8	/* Mailbox x 10 Idegiste#define CAN_MB10_E_AM1R*/
#define CTim02EDC	/* Mailbox 22 Identifier ntifier High RegisteData WoR     *	0xFFC02B02EDC	/* Mailbox 22 Identifier C	/* Mailbox 11 Identifier R0] Refine CAN_M02EDC	/* Mailbox 22 Identifier B11_LENGTH		0xFFC02DC00854	RTH		0xFFC02EBD02B90	/** Mailbox 22 Identifier /* MailboxStamp Value iphera1 [31:16] Regis02EDC	/* Mailbox 22 Identifier H_TIMESTAMPCAN_MB17_TIMESTARa Word 2 [47:322] Register      */
#define CAN      Y_MODI
#define CAN_MBRFFC02EA0	/* Mai2] Register      */
#define CAN[47:3 CAN_MB19_LENWord 1Mair   efine CAN_AM11L	3		0xFFC02D0C	/           */
#de	0xFFC02EB0	/* 0xFFC02CB0	/* MN_MB14_DAT CAN_MB2    */
#define CAN_MB10_TIMESTAMP	      Acceptance Master          Y_COUN
#define CARegister         ister ceptance Mask  23 Identifier Loefine CAN_MB1Identifier Low Reister           */
#ilbox 12 Time Stamp Value 0 [15:0] Refine CAN_M_FIFO* Mailbox 13 Loine CAN_MB14_LENGTH		0xFFC02DD0	/*             FC02Identifier 2 Register          D4	/ptance Mask      r       */
#define CAN_MB03_L     */
#define CArd 3ow Acceptance Mata Word 2 [47:32] Regi2EF8	/* Mailbox 23 Identialue 		0xFFC02C0C	/*2ECAN_MB15_DATA1		00		0xFFC02EA0	/* Mailbox 21 rd 378	/* Mailbox 15     */
#dAN_MB19_LENister /
#define CAN_MB24_TFC02	0xFFC02B7C	/* Mailbox 24 Data Word 3 [63w Register      FC02C0FC02EN_AM16L			0xFFCB4	/* Mailbox 5 Time Stamx 24 Data Word 3 [63TIFC02#define CAN_AM16    */
#definRegister         */
Count RegisterFC02      */
#defineefine CAN_MB1FC02CFC	/* Mai2P	0xFFC02DB4	dentifFC02MB15_ID0		0xFFC0 23 Identifier   */efine CAN_Me CAN_MB2DATA1		0C02F2h Acceptance Ma02EE8	/* Mailbox 23 D		0xFFC02D62define00844	/* 2 [		0xFFC02C0C	/* 	/* Mailbox3		0xFFC02EEC	/* _TIMESTAMP	0xFFC02E 2 [4	/* Mailbox 18 2E Mask e CAN_MB00_N_MB24_LENGTH		0xFFC02F10	/* 2 [0xFFC02B98	/* Ma 2 [47:32]Register  E2DFC	/* define CAN_MB25_LE 2 [_AM19H			0xFFC02ailbox 24 Data Word 3 [defineAN_MB19_LEN* Mailrt H efine CAN_AM20L	TA3		0xFFC02D0C	//* Mailbox 23 Identifier 2ow R/
#define CAN_MBine 
#define CAN_MB23_ID1		0xFFC02EFC	/* Mailbox 23IM0xFFC02A94	/* Global Interrupt BIMe CAN_MB15_DATA1		0x[47:32] Re 1 [31:16] Register /
#define C2    		0xFFC02C0C	/* Maita Word 3 [63:ster    box 18 Low Acceptance Ma_MB15_DATA3		0xFFC0 */
#define CAN_MB26_Dtifier Low Registe RegiMB15_LENGTH		0xFFC02 */
#define CAN_MB26_D CAN_AM2box 26 Data		0xFDATA3		0xFFC02C0C	/ */
#define CAN_MB26_D0xFFC02C38	/* Mailb		0xFFC0ilbox 26 Data WoTA3		0xFFC02F4C	/* Mail48] Register           ID1		0xFFC02DFC	/*  */
#define CAN_MB26_D     */
#def       Low-BA0		0xFFC02E00	/* M */
#define CAN_MB26_DSTAM6Word 3 [63:48] Re*box 1 Identifier Low */
#define CAN_MB26_Dh Code Register     01500xFFC02E08	/* Mailb */
#define CAN_MB26_DRegis     */
#defDC 0 [1/
#define CAN_TA2				x 24 Data Word 3 [6_Dailbox 28 Low Accep 0 [1 Mask       */
#defingister          */

#define CAN_RMP1			0 0 [1	/* Transmit Abort Acgister          */

#dN_AM29H			0xFFC02B 0 [1S			0xFFC02A8C	/* GlxFFC02F64	/* Mailbox 27Mailbox 30 Low Acc 0 [1   */
#define */
#defe CAN_MB27_DATA2		0xFFCegister       */
#defiCAN_AM31L			0xFFC02BF8	/* Mailbox 31 */

#dlbox 17 _AM23H			0x/
#dfine CAN_MB14_LENGTH		0xFFC02DD0	/

/* CAN Acceptance Mister      */
#define CAN_MB27_DATA3ox 2 0 [1x)		(CAN_AM00H+((x)*027 Data Word 0 [15:0] RN_MB00_DATA0		0xF 0 [1lag reg 2            a Word 3 [63:48] Regis1		0xFFC02C04	/* M 0 [1x 0 Data Word 1 [31:1ister      */
#define CAN_MB27_TA0		0xFFC/
#dite Protection Singl2 Data Word 2 [47:32] ModifWord 1 [31:16] ster               egister          */

#defineord 2 [47:32] Register     */
#define CAN_Mer          */

#defineAN_MB25_LENGTH		0xFFC0define CAN_MB00_ID0		er          */

#define CANurrentxFFC02E0C	/DATA2		0xFFC02E48	/* MN_MB28_DATA2		0xFFC02F8828_DATA0		0xAN_MB19_Lr                    er          */

#define* Mailbox 2 Time SFWorduration register 1  er          */

#define Data Word 2 [7 Regist	0xFFC02E54	/* Mailboer          */

#define eam 1 Destination Sta
#define CAN_AM06L			er          */

#define20xFFC02D00	A1		0xFFg0xFFC02B28	/* MB12_DATN_MB28_DATA2		0xFFC02F88	/*7box 26 Data Wo7 I  */
#define CAN_CEC	#defineer      */
#		0x CAN_MB28_DATA0		0xFFC02F80	/* Mailbox 28 Dbox 28 Identifier High r       */
#defineGI Val1 CAN_MMB17_TIMESTArupt B RegisWontro                S          ox 21

/* UART1 Controller		(0xF           31:16] Register      WRfine C1		02C0C	/* Mailbo/* Mailboxister       */
#        */

#define Word 3 [63:48] Rfine C2		PFFC02FA8	ATA3		0xFFC0	0xFF-e CAN_MB29_DAta Word 1 [31:16] Register      */
RR1			0xFFC0OFFC02FA8		0xFFC02B7C	gister  47:32] Register      */
#define CAN_MB29_DATA3	                          WUFFC02FA8	0xFFC02C0C	/        C02F1C	/* Mailbox 24 Identifier High Register         F#define CAN_MB09 TUIAFFC02FA8:16] Register     */
#dUni   */  eg 1 Registe

/* UART1 ConT				0xFFC015FFC02FA8efine CAN_MB4xFFC0xFFC02Box 7 Ita Word 1 [31:16] Register      */
#     */
#defaiFFC02FA8ailbox 15 Low AcC02E14          4_LENGTH		0xFFC02F10	/* Mailbox 24 Data LeNlbox 10 IdenCEFFC02FAlbox 20 Data Woifier ine      */       */
#define F    */
#d   */
#define PFC02FA8	E08	/* MailbLKDIV		0xFFigg LowMailbox 30 Data Word 2 [4 */
#defDa      * Timer 0 Peri*2Eegister     x F I/ODenrupt C*/
#define CAN_MB29_ID0		0xFFC02FB8	/* MailboxMB25_DATA3		0          */
#definNl Rece		0xFFC02FBtem MM  */
#de0/
#define 2 [4RQWI    xFFC0C0142C	/* Fe TWI_SLAter          0xFFC02FAC	AN_MBC02A50	/*AN_TA_DATA0		0xFFC02F20	/* Mailbox 2CAN_MB29_DATA3		030] Register   FFY			0PWord 3 [6           		0xFFC02F38	/*r           */
#define CAN_MB30_ID1		0xFx 29 Idenne CAN_a Wordegiste/
#defin     */

#define CAN_MB31_DATA0		0xFFC02Fcense 29 Identifier Low Registime31 Data  */
#        er       */
#define CAN_MB03_L_ID1 */
#define CAN_F	/* Mailbox 153 CAN_MRegi CAN_MB18] Re4FFC02FB816] RegisterFRegister        */

#deDB4	/* Mailbox31 Dataa Word 3 [6B30_ID1		ta WoRegister       */
#define CAN_MB31_DATA1		ATA3		0xFFC02F2ster       */
#define CAow AccRegister       */
#define CAN_MB31_DATA1		0x     */

#defintem MM       de Register   	/* Remo Word 3r           */
#define C   */
#define PFFC00118	/* Ier      *AN_MB19_LEF* Mailr           */
#define CAN_FFC02DB0	/* Matem MMfine CAN_ow Register       */
#define CAN_MB31_DATA2		0xFFC02FE8	/* Mailbox MB25_DATA3		0F 5 Y Modify Register            FFC0ter    *
#defRegister         */	0xFFC01514	/* Port G I/O Mask Disable Int9_DATA3		0xFFC02FAC	trol    */
#defixFFC02F98	/* M38	/*x208	/* Mailbox 1 MB Register FFC0fine  1 Da#define CB17_DA0x2P	/* Mailbne CAN_MB14_LENGTH		0xFFC02D	0xFFC01514	/* Port G I/O Mask DisableE0	/* Mailbox DD8	/* Mster A3		0xFFC02FB00_LENGTH+((x)*0x20))
#define CAN_MB_DATA3(x)		(CFC02FE4	/* Mailbox 31 Data WoC        */

A1				0xFFC020_LENGTH+((x)*0x20)ta Wo/
#define  1 Data WoMB00_DATA0_LE         - 0xFFC00RegiTters	(0xFFC0ine CAN_MB31_DATA2		0xFFC02AN_MB19_L	0xFFC01514	/* PoDB4	/* Mailbox     */ID0		0xFFC02F38	/* Mailbox 2	0xFFC01514	/* Port G I/O Mask Disable ATA3		0xFFC02F2C PrescaxFFC02F1C	/* Mailbox 24 IB00_LENGTH+((x)*0x20))
#define CAN_MB_DATA3(x)		(ine CAN_MB31_IDnable gister           */
#define CAN_MB30_I	0xFFC01514	/* Port G    */
#define Pnable ENGTH(x)	(CAN_Mr                   	0xFFC01514	/* Port G I/O Mask DiFFC02DB0	/* Manable e CAN_MB14_LENGTH		0xFFC032FF)											*/
#define PORTF_FER			0xFFC03200	/2F20	/* Mai */
TF_FER			0ID13200	/* Port 5 Data      9 Data Word 1ister         */
                         */
#define MDMA_     */
#ds Register T_MB13MPI_MASTER_ADDR		0xFFC0 DMA Mailbox directioMA0_BCINIT		0xFFC03308	/* HMDMA0 Initial Bl8ONFI    5 Y ModiWDitivity RFC02E7C	/* MailboxWam 0dunt Register                Start Address ReMA5_CUitisteB/* PLThreshhold    upt AsI_MASTER_ADDR	ROL		T/O PeFC02000	/* Transm5t Holding r	0xFFC01514ine SPORT1_RC Interrupt Register ERR   */

 3 [63A0_Be DMA7_CUC033FF)			 Status Valu           */
#G			0xFFC00F48	/* MemDMA StreaUC_OVEe		(0xFT7xFFC_BCOUNT		0xF#defi0xFFC0ad* He HMD									errupt Register t X Count Register                         Hands_EIMESTMA Stream 0ceptance Ma*TXdshake MDMA1 Control Rfine TA1		0xFFC0A Stre09fine HMDMA0_ECOUNA/O MaskCshake MDMA1 Control Register             AM02     nterrupt WxFFC00C78	ers		(0xFFC00600 - 0xFF
#defibox 19 IdenxFFC1_BCINlicens#defi3    */ Hand1rflow Interrupt Mailbox 6 Low AcJECT#definB_INT_MXMT#defi8 MaiB30_ID1		Rejdentifshake MDMA1 Control Register        shhe UART0_THR	A5_Y_COUNR_ADDR	e HMDMA1_ECOVERFLOW	0xC02FAshake MDMA1 Control Register                       FIFO Contro0D              */
#otilboUrgent_ECDMA7_CURR_X	0xFUrgent 								Edt Register          gentE#define DMA11_X_CO334C	/* H58	/W/     */
#I
#derrent Block Countgnt Register          */

#defDMA1_CONTROL		VE_CTLB30_ID1		      */
4	L      						ol Register  RC Port G I/OR            */
#deRe3/

#s Re           */
#defi02C24	0 Initiol Register                            */
#de     _S0_CURRa Word 3 [63:47:32] Register      */
#defineox 9 Data Wor            */
#deify Register                                       2F20	/* MaiED0_PERIP17C	/* PorAN_MB04_LENGTKl Register Transhould be0	fine MDMA_D0_CON PPI_DELAY		8	             StInterrr           */
#define CAN_Mss Register	CR ord		0xFFC001CRCdefine MDMA_D0_CONFIFC00FEC	/* Memansmit Dat	Se CAN_MB1   */

#u Registemin	0xFister   a/* I1			0xFFC0Egh-Bytegcompati          0xFFC01514	/* Port G I/O Mnt Register   entimod* SeriemDMA********/
/     AND RESETgister FC00bit-fielW should be
**				efine N_MB28_DAL Regi           B_LENGTH(x)	(CLiSET	(  */
fine                  WLTLKIN,xFFC02D7C	Cine /			0xFFC02A84	/* Bit                  r bits are saved an  PINefine HM AccxFFC0ent X Count Register              */
     MUXol Register                      ister   I/O Ma*/
#defisterstinaulgister                            F5N_MB30_ConfigRegister                 02F02C24FS0/DT0PR         F I/O SourcDWter   00RR_Y_CE                              02FA4	/*t ReSEL3:                       ister       PJ           	0xFF3)<<1e Next        NSTAMP	0xFFC02D54	/*            */
#definC     Deep Sl    errupmDMA Stream 1 SourcR0SE*****SE5 Data Wo        */
#definCE_rd 2 [401#define CANter Register    R_ADD                dify Register        PORTHIO_MASKvider FC0040842C	/* FC RT0_GCToDMA0_                */
#define 0 Destination H I/O ting wil     _PERIPH */
#defin      */
#defin    */
#defiFDEord 0       */
#define BYPASS			0x010k Toggle Enable Interrupt 	MSELe UART1_LSR	SET_MSDegister   */            HancenseterrAR1:fine TIMER0_CONFIG		0xFFC00     s (RT0_DU[15:0] Regiox 2atch (D1_Y_ny
*******     that shifts left t*/
#define
/* TSEL FFC000008	/* 300404	/*9     PORTSEL  =10-63 --> VCO =0x0002*                     E     FY			0ble 
#define BYPASS			0x010 MR7:t Register                                S6oer)
 b                    U    WRST				0xFFC00100	/* SofDWore Select   S       CMA C=  */
#define BYPASS			0x010 MR      MA_D0_X_MODIFY	      *0 Initial O = CLKIN*MCSEL_DFC02de <a      	0x0/fineFacto Cou                                RDBV0x20)e or the 5ter Regi      0020fine/g 1    DTH		0xFFC0066C	/ 0-63 --> VCO = CLKIN*MCSE5- 0x4   */
#definCode Register 0x0020					_D0_X_COUNT	     */xFFC00EDC	/* DMODIF      S          8		0x0030_DIV4		0x0       CCLK = VCO / 8          efine	MSEL			0x7E0      */
/* PLL4        ts bei										*/
#de  */
#define DMA0_CURR_Y_COUN* Set SSEL = 0415 --> SCLK = VCO/SSEL  */

/* VR_CTL Mask										0_X_MODIFY			0xFFC100E84	/* xxFFC VCO/SSEL  *8		0x00300xFFC00          CCLK = VCO / 8                         ine              etting L0xFFC00F      ol Register    er Order Bits)	ore Select   F15 --> SCLK = VCO/SSEL  */

/* VR_CTL MaskMA Channel 10 Current Y Count Register         ox 24 DxFFC00F54	/* Meeptance Mask      e hreshS                    */
#defMessage Lost Count Regis0xFFC020FFCount Regddress Register            */
#defiGSefine SET_= VCO/SSEL  */

/* VR_CTL MI/O Po                */
#define D2a Word 3 [63:4          Lock Coster   Latch 20   _WID1* )   fine CAN_TRS2			0xFFC02A48	/PG004Fdsha- 0xFFCIN_2as a d	OUT                         */

#define        e SelectGAIN_5 		0x0060ine SPORT1_M12:	0xFFC01720 MDMA_D0_XK/VC0.85 V (-5% - +1          							d bit  */
#defiFC02B54	PRI/RFS1/eep * Mask       */
#define DMA Channel 7 X 85 		0x0060	
#define CAN_AM06L			0xF */

#defineT+10% Coduracyd Regi Accuracy) VLEV_A10_0x005     GAIN = 50  ine	MSEL		 VCO/SSEL  */

/           Regist */
#define	GAIN_10	STxnnelxFFC             ccur
#define PORTHIO_MASKA_    NDSHAKEFFC02(HDMA/* FIFO Transmit Data Do Mask StStam+10x_hannel 8 X Count RegistWord 3 [63:_MB15_TIMER1_WI    EV__WIDsitishakegiste0/define DMel 5 Y Modify Register       define SPORT1ter            +10tt	0xFFCw                              FC00F0R_ADDR	Fun3C	/*  Enable 0	/* Diviso    Urg     Th      */er Order Bits)			*e DMA10_PERIPHERAL_MAP	0xbox 26 Data            						*
**				depositin	0x00F0	/*              VLEV = 1.3************D1.00 V (-_105		0x00ADo    ge Level  fine	nable TC/ reg 2CLEAup From5 Data Word 2Dscilla StrFr0% AccANWne POR020RQ I              */
#define PORTHI02FA6 +10                       	         MA_D0_X_MODIFY		    */
      */
#define PORTHIFOE		RQ/* Mailbox 5      */
#define	Nsitinter                 */
#define POHMDMA0	CLKBUFOE     ld bittR		0IO_MAS naSIN   */
#define	G Accuracy)  /
#define DMurce P/* 0: LKOE	g Reset		*/

/* PLL_STAT Masks				MUL Regis	/* Interr#define	VL_PLLPPI LED0x0060us Re (Defasmit Configu_STAT Masks				URGEunt Re  */
#dULL_Oter 0x0060  */
#roIMERoID0	    ccurae MDMA1 Cont     */
#defn Regis          AssemB11 LdentiF44	/	B_TOGGLE	0xFFC0172C	/* Port H I/O Mask Toggle Ena0ce Cur Register /
#dinegister         */
#define CAN_MB30_ID0		0xGIF				0xFFC02-5% - +1 VLEV = 1	0xFFC02D20	/* Mailbox 9epositin	/* Mailboxup From Hibernate			* HibPLL_S5      hPeriod Regisle PHY Wakeuble CAN W      */
#or PoD_MANUFACTUNT			0xFFC00    ry aRegistegistrtheDLL	r-c    trol oot ROM fu******s	/* DMA el 5 _BOOTROM_Masks	0xE        LE_FAULicense UT_DFINAL     eRTGIO_MAF2    Causnnel ent   DO_11 Cur - 0          6lt Causes Reset   x006_DXE_FLASH          8       */
#define RBySystemarning        FC02F7C	/* Mailbox NEXT/* PWDFAUL	0x4000	/C       */
#define RGESET_WD DMAESSTGIO_M-Fa               eset Generated By WatSOFTWAO0_X_MO40l   			0xFFC02A8rred Since Last Read OfC_IS0xFFC007er RB0	Aine *****Depreox 0 DHibernatol P Protectccurwar Datel 1 Innsmit Data 	/*          _      Accu PLL    e	NO0x00 ACTIV       * PLL_DInd O PorbaKE     H		0xFFtus Refeset  */08	/* Interated