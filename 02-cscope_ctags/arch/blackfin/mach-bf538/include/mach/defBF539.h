/*
 * Copyright 2008-2009 Analog Devices Inc.
 *
 * Licensed under the ADI BSD license or the GPL-2 (or later)
 */

/* SYSTEM & MM REGISTER BIT & ADDRESS DEFINITIONS FOR ADSP-BF538/9 */

#ifndef _DEF_BF539_H
#define _DEF_BF539_H

/* include all Core registers and bit definitions */
#include <asm/def_LPBlackfin.h>


/*********************************************************************************** */
/* System MMR Register Map */
/*********************************************************************************** */
/* Clock/Regulator Control (0xFFC00000 - 0xFFC000FF) */
#define	PLL_CTL			0xFFC00000	/* PLL Control register (16-bit) */
#define	PLL_DIV			0xFFC00004	/* PLL Divide Register (16-bit) */
#define	VR_CTL			0xFFC00008	/* Voltage Regulator Control Register (16-bit) */
#define	PLL_STAT		0xFFC0000C	/* PLL Status register (16-bit) */
#define	PLL_LOCKCNT		0xFFC00010	/* PLL Lock	Count register (16-bit) */
#define	CHIPID			0xFFC00014	/* Chip	ID Register */

/* CHIPID Masks */
#define CHIPID_VERSION         0xF0000000
#define CHIPID_FAMILY          0x0FFFF000
#define CHIPID_MANUFACTURE     0x00000FFE

/* System Interrupt Controller (0xFFC00100 - 0xFFC001FF) */
#define	SWRST			0xFFC00100  /* Software	Reset Register (16-bit) */
#define	SYSCR			0xFFC00104  /* System Configuration registe */
#define	SIC_IMASK0		0xFFC0010C  /* Interrupt Mask Register */
#define	SIC_IAR0		0xFFC00110  /* Interrupt Assignment Register 0 */
#define	SIC_IAR1		0xFFC00114  /* Interrupt Assignment Register 1 */
#define	SIC_IAR2		0xFFC00118  /* Interrupt Assignment Register 2 */
#define	SIC_IAR3			0xFFC0011C	/* Interrupt Assignment	Register 3 */
#define	SIC_ISR0			0xFFC00120  /* Interrupt Status	Register */
#define	SIC_IWR0			0xFFC00124  /* Interrupt Wakeup	Register */
#define	SIC_IMASK1			0xFFC00128	/* Interrupt Mask Register 1 */
#define	SIC_ISR1			0xFFC0012C	/* Interrupt Status Register 1 */
#define	SIC_IWR1			0xFFC00130	/* Interrupt Wakeup Register 1 */
#define	SIC_IAR4			0xFFC00134	/* Interrupt Assignment	Register 4 */
#define	SIC_IAR5			0xFFC00138	/* Interrupt Assignment	Register 5 */
#define	SIC_IAR6			0xFFC0013C	/* Interrupt Assignment	Register 6 */


/* Watchdog Timer (0xFFC00200 -	0xFFC002FF) */
#define	WDOG_CTL	0xFFC00200  /* Watchdog	Control	Register */
#define	WDOG_CNT	0xFFC00204  /* Watchdog	Count Register */
#define	WDOG_STAT	0xFFC00208  /* Watchdog	Status Register */


/* Real	Time Clock (0xFFC00300 - 0xFFC003FF) */
#define	RTC_STAT	0xFFC00300  /* RTC Status Register */
#define	RTC_ICTL	0xFFC00304  /* RTC Interrupt Control Register */
#define	RTC_ISTAT	0xFFC00308  /* RTC Interrupt Status Register */
#define	RTC_SWCNT	0xFFC0030C  /* RTC Stopwatch Count Register */
#define	RTC_ALARM	0xFFC00310  /* RTC Alarm Time Register */
#define	RTC_FAST	0xFFC00314  /* RTC Prescaler Enable Register */
#define	RTC_PREN		0xFFC00314  /* RTC Prescaler Enable Register (alternate	macro) */


/* UART0 Controller (0xFFC00400	- 0xFFC004FF) */
#define	UART0_THR	      0xFFC00400  /* Transmit Holding register */
#define	UART0_RBR	      0xFFC00400  /* Receive Buffer register */
#define	UART0_DLL	      0xFFC00400  /* Divisor Latch (Low-Byte) */
#define	UART0_IER	      0xFFC00404  /* Interrupt Enable Register */
#define	UART0_DLH	      0xFFC00404  /* Divisor Latch (High-Byte) */
#define	UART0_IIR	      0xFFC00408  /* Interrupt Identification Register */
#define	UART0_LCR	      0xFFC0040C  /* Line Control Register */
#define	UART0_MCR			 0xFFC00410  /*	Modem Control Register */
#define	UART0_LSR	      0xFFC00414  /* Line Status Register */
#define	UART0_SCR	      0xFFC0041C  /* SCR Scratch Register */
#define	UART0_GCTL		     0xFFC00424	 /* Global Control Register */


/* SPI0	Controller (0xFFC00500 - 0xFFC005FF) */

#define	SPI0_CTL			0xFFC00500  /* SPI0 Control Register */
#define	SPI0_FLG			0xFFC00504  /* SPI0 Flag register */
#define	SPI0_STAT			0xFFC00508  /* SPI0 Status register */
#define	SPI0_TDBR			0xFFC0050C  /* SPI0 Transmit Data Buffer Register */
#define	SPI0_RDBR			0xFFC00510  /* SPI0 Receive Data Buffer	Register */
#define	SPI0_BAUD			0xFFC00514  /* SPI0 Baud rate Register */
#define	SPI0_SHADOW			0xFFC00518  /* SPI0_RDBR Shadow	Register */
#define SPI0_REGBASE			SPI0_CTL


/* TIMER 0, 1, 2 Registers (0xFFC00600 - 0xFFC006FF) */
#define	TIMER0_CONFIG			0xFFC00600     /* Timer	0 Configuration	Register */
#define	TIMER0_COUNTER				0xFFC00604     /* Timer	0 Counter Register */
#define	TIMER0_PERIOD			0xFFC00608     /* Timer	0 Period Register */
#define	TIMER0_WIDTH			0xFFC0060C     /* Timer	0 Width	Register */

#define	TIMER1_CONFIG			0xFFC00610	/*  Timer 1 Configuration Register   */
#define	TIMER1_COUNTER			0xFFC00614	/*  Timer 1 Counter Register	     */
#define	TIMER1_PERIOD			0xFFC00618	/*  Timer 1 Period Register	     */
#define	TIMER1_WIDTH			0xFFC0061C	/*  Timer 1 Width Register	     */

#define	TIMER2_CONFIG			0xFFC00620	/* Timer 2 Configuration Register   */
#define	TIMER2_COUNTER			0xFFC00624	/* Timer 2 Counter Register	    */
#define	TIMER2_PERIOD			0xFFC00628	/* Timer 2 Period Register	    */
#define	TIMER2_WIDTH			0xFFC0062C	/* Timer 2 Width Register	    */

#define	TIMER_ENABLE				0xFFC00640	/* Timer Enable	Register */
#define	TIMER_DISABLE				0xFFC00644	/* Timer Disable Register */
#define	TIMER_STATUS				0xFFC00648	/* Timer Status	Register */


/* Programmable	Flags (0xFFC00700 - 0xFFC007FF) */
#define	FIO_FLAG_D				0xFFC00700  /* Flag Mask to directly specify state of pins */
#define	FIO_FLAG_C			0xFFC00704  /* Peripheral Interrupt Flag Register (clear) */
#define	FIO_FLAG_S			0xFFC00708  /* Peripheral Interrupt Flag Register (set) */
#define	FIO_FLAG_T					0xFFC0070C  /* Flag Mask to directly toggle state of pins */
#define	FIO_MASKA_D			0xFFC00710  /* Flag Mask Interrupt A Register (set directly) */
#define	FIO_MASKA_C			0xFFC00714  /* Flag Mask Interrupt A Register (clear) */
#define	FIO_MASKA_S			0xFFC00718  /* Flag Mask Interrupt A Register (set) */
#define	FIO_MASKA_T			0xFFC0071C  /* Flag Mask Interrupt A Register (toggle) */
#define	FIO_MASKB_D			0xFFC00720  /* Flag Mask Interrupt B Register (set directly) */
#define	FIO_MASKB_C			0xFFC00724  /* Flag Mask Interrupt B Register (clear) */
#define	FIO_MASKB_S			0xFFC00728  /* Flag Mask Interrupt B Register (set) */
#define	FIO_MASKB_T			0xFFC0072C  /* Flag Mask Interrupt B Register (toggle) */
#define	FIO_DIR				0xFFC00730  /* Peripheral Flag Direction Register */
#define	FIO_POLAR			0xFFC00734  /* Flag Source Polarity Register */
#define	FIO_EDGE			0xFFC00738  /* Flag Source Sensitivity Register */
#define	FIO_BOTH			0xFFC0073C  /* Flag Set	on BOTH	Edges Register */
#define	FIO_INEN					0xFFC00740  /* Flag Input Enable Register  */


/* SPORT0 Controller (0xFFC00800 - 0xFFC008FF) */
#define	SPORT0_TCR1				0xFFC00800  /* SPORT0 Transmit Configuration 1 Register */
#define	SPORT0_TCR2				0xFFC00804  /* SPORT0 Transmit Configuration 2 Register */
#define	SPORT0_TCLKDIV			0xFFC00808  /* SPORT0 Transmit Clock Divider */
#define	SPORT0_TFSDIV			0xFFC0080C  /* SPORT0 Transmit Frame Sync Divider */
#define	SPORT0_TX			0xFFC00810  /* SPORT0 TX Data Register */
#define	SPORT0_RX			0xFFC00818  /* SPORT0 RX Data Register */
#define	SPORT0_RCR1				0xFFC00820  /* SPORT0 Transmit Configuration 1 Register */
#define	SPORT0_RCR2				0xFFC00824  /* SPORT0 Transmit Configuration 2 Register */
#define	SPORT0_RCLKDIV			0xFFC00828  /* SPORT0 Receive Clock Divider */
#define	SPORT0_RFSDIV			0xFFC0082C  /* SPORT0 Receive Frame Sync Divider */
#define	SPORT0_STAT			0xFFC00830  /* SPORT0 Status Register */
#define	SPORT0_CHNL			0xFFC00834  /* SPORT0 Current Channel Register */
#define	SPORT0_MCMC1			0xFFC00838  /* SPORT0 Multi-Channel Configuration Register 1 */
#define	SPORT0_MCMC2			0xFFC0083C  /* SPORT0 Multi-Channel Configuration Register 2 */
#define	SPORT0_MTCS0			0xFFC00840  /* SPORT0 Multi-Channel Transmit Select Register 0 */
#define	SPORT0_MTCS1			0xFFC00844  /* SPORT0 Multi-Channel Transmit Select Register 1 */
#define	SPORT0_MTCS2			0xFFC00848  /* SPORT0 Multi-Channel Transmit Select Register 2 */
#define	SPORT0_MTCS3			0xFFC0084C  /* SPORT0 Multi-Channel Transmit Select Register 3 */
#define	SPORT0_MRCS0			0xFFC00850  /* SPORT0 Multi-Channel Receive Select Register 0 */
#define	SPORT0_MRCS1			0xFFC00854  /* SPORT0 Multi-Channel Receive Select Register 1 */
#define	SPORT0_MRCS2			0xFFC00858  /* SPORT0 Multi-Channel Receive Select Register 2 */
#define	SPORT0_MRCS3			0xFFC0085C  /* SPORT0 Multi-Channel Receive Select Register 3 */


/* SPORT1 Controller (0xFFC00900 - 0xFFC009FF) */
#define	SPORT1_TCR1				0xFFC00900  /* SPORT1 Transmit Configuration 1 Register */
#define	SPORT1_TCR2				0xFFC00904  /* SPORT1 Transmit Configuration 2 Register */
#define	SPORT1_TCLKDIV			0xFFC00908  /* SPORT1 Transmit Clock Divider */
#define	SPORT1_TFSDIV			0xFFC0090C  /* SPORT1 Transmit Frame Sync Divider */
#define	SPORT1_TX			0xFFC00910  /* SPORT1 TX Data Register */
#define	SPORT1_RX			0xFFC00918  /* SPORT1 RX Data Register */
#define	SPORT1_RCR1				0xFFC00920  /* SPORT1 Transmit Configuration 1 Register */
#define	SPORT1_RCR2				0xFFC00924  /* SPORT1 Transmit Configuration 2 Register */
#define	SPORT1_RCLKDIV			0xFFC00928  /* SPORT1 Receive Clock Divider */
#define	SPORT1_RFSDIV			0xFFC0092C  /* SPORT1 Receive Frame Sync Divider */
#define	SPORT1_STAT			0xFFC00930  /* SPORT1 Status Register */
#define	SPORT1_CHNL			0xFFC00934  /* SPORT1 Current Channel Register */
#define	SPORT1_MCMC1			0xFFC00938  /* SPORT1 Multi-Channel Configuration Register 1 */
#define	SPORT1_MCMC2			0xFFC0093C  /* SPORT1 Multi-Channel Configuration Register 2 */
#define	SPORT1_MTCS0			0xFFC00940  /* SPORT1 Multi-Channel Transmit Select Register 0 */
#define	SPORT1_MTCS1			0xFFC00944  /* SPORT1 Multi-Channel Transmit Select Register 1 */
#define	SPORT1_MTCS2			0xFFC00948  /* SPORT1 Multi-Channel Transmit Select Register 2 */
#define	SPORT1_MTCS3			0xFFC0094C  /* SPORT1 Multi-Channel Transmit Select Register 3 */
#define	SPORT1_MRCS0			0xFFC00950  /* SPORT1 Multi-Channel Receive Select Register 0 */
#define	SPORT1_MRCS1			0xFFC00954  /* SPORT1 Multi-Channel Receive Select Register 1 */
#define	SPORT1_MRCS2			0xFFC00958  /* SPORT1 Multi-Channel Receive Select Register 2 */
#define	SPORT1_MRCS3			0xFFC0095C  /* SPORT1 Multi-Channel Receive Select Register 3 */


/* External Bus	Interface Unit (0xFFC00A00 - 0xFFC00AFF) */
/* Asynchronous	Memory Controller  */
#define	EBIU_AMGCTL			0xFFC00A00  /* Asynchronous Memory Global Control Register */
#define	EBIU_AMBCTL0		0xFFC00A04  /* Asynchronous Memory Bank	Control	Register 0 */
#define	EBIU_AMBCTL1		0xFFC00A08  /* Asynchronous Memory Bank	Control	Register 1 */

/* SDRAM Controller */
#define	EBIU_SDGCTL			0xFFC00A10  /* SDRAM Global Control Register */
#define	EBIU_SDBCTL			0xFFC00A14  /* SDRAM Bank Control Register */
#define	EBIU_SDRRC			0xFFC00A18  /* SDRAM Refresh Rate Control Register */
#define	EBIU_SDSTAT			0xFFC00A1C  /* SDRAM Status Register */



/* DMA Controller 0 Traffic Control Registers (0xFFC00B00 - 0xFFC00BFF) */

#define	DMAC0_TC_PER			0xFFC00B0C	/* DMA Controller 0 Traffic Control Periods Register */
#define	DMAC0_TC_CNT			0xFFC00B10	/* DMA Controller 0 Traffic Control Current Counts Register */

/* Alternate deprecated	register names (below) provided	for backwards code compatibility */
#define	DMA0_TCPER			DMAC0_TC_PER
#define	DMA0_TCCNT			DMAC0_TC_CNT


/* DMA Controller 0 (0xFFC00C00	- 0xFFC00FFF)							 */

#define	DMA0_NEXT_DESC_PTR		0xFFC00C00	/* DMA Channel 0 Next Descriptor Pointer Register */
#define	DMA0_START_ADDR			0xFFC00C04	/* DMA Channel 0 Start Address Register */
#define	DMA0_CONFIG				0xFFC00C08	/* DMA Channel 0 Configuration Register */
#define	DMA0_X_COUNT			0xFFC00C10	/* DMA Channel 0 X Count Register */
#define	DMA0_X_MODIFY			0xFFC00C14	/* DMA Channel 0 X Modify Register */
#define	DMA0_Y_COUNT			0xFFC00C18	/* DMA Channel 0 Y Count Register */
#define	DMA0_Y_MODIFY			0xFFC00C1C	/* DMA Channel 0 Y Modify Register */
#define	DMA0_CURR_DESC_PTR		0xFFC00C20	/* DMA Channel 0 Current Descriptor Pointer Register */
#define	DMA0_CURR_ADDR			0xFFC00C24	/* DMA Channel 0 Current Address Register */
#define	DMA0_IRQ_STATUS			0xFFC00C28	/* DMA Channel 0 Interrupt/Status Register */
#define	DMA0_PERIPHERAL_MAP		0xFFC00C2C	/* DMA Channel 0 Peripheral Map	Register */
#define	DMA0_CURR_X_COUNT		0xFFC00C30	/* DMA Channel 0 Current X Count Register */
#define	DMA0_CURR_Y_COUNT		0xFFC00C38	/* DMA Channel 0 Current Y Count Register */

#define	DMA1_NEXT_DESC_PTR		0xFFC00C40	/* DMA Channel 1 Next Descriptor Pointer Register */
#define	DMA1_START_ADDR			0xFFC00C44	/* DMA Channel 1 Start Address Register */
#define	DMA1_CONFIG				0xFFC00C48	/* DMA Channel 1 Configuration Register */
#define	DMA1_X_COUNT			0xFFC00C50	/* DMA Channel 1 X Count Register */
#define	DMA1_X_MODIFY			0xFFC00C54	/* DMA Channel 1 X Modify Register */
#define	DMA1_Y_COUNT			0xFFC00C58	/* DMA Channel 1 Y Count Register */
#define	DMA1_Y_MODIFY			0xFFC00C5C	/* DMA Channel 1 Y Modify Register */
#define	DMA1_CURR_DESC_PTR		0xFFC00C60	/* DMA Channel 1 Current Descriptor Pointer Register */
#define	DMA1_CURR_ADDR			0xFFC00C64	/* DMA Channel 1 Current Address Register */
#define	DMA1_IRQ_STATUS			0xFFC00C68	/* DMA Channel 1 Interrupt/Status Register */
#define	DMA1_PERIPHERAL_MAP		0xFFC00C6C	/* DMA Channel 1 Peripheral Map	Register */
#define	DMA1_CURR_X_COUNT		0xFFC00C70	/* DMA Channel 1 Current X Count Register */
#define	DMA1_CURR_Y_COUNT		0xFFC00C78	/* DMA Channel 1 Current Y Count Register */

#define	DMA2_NEXT_DESC_PTR		0xFFC00C80	/* DMA Channel 2 Next Descriptor Pointer Register */
#define	DMA2_START_ADDR			0xFFC00C84	/* DMA Channel 2 Start Address Register */
#define	DMA2_CONFIG				0xFFC00C88	/* DMA Channel 2 Configuration Register */
#define	DMA2_X_COUNT			0xFFC00C90	/* DMA Channel 2 X Count Register */
#define	DMA2_X_MODIFY			0xFFC00C94	/* DMA Channel 2 X Modify Register */
#define	DMA2_Y_COUNT			0xFFC00C98	/* DMA Channel 2 Y Count Register */
#define	DMA2_Y_MODIFY			0xFFC00C9C	/* DMA Channel 2 Y Modify Register */
#define	DMA2_CURR_DESC_PTR		0xFFC00CA0	/* DMA Channel 2 Current Descriptor Pointer Register */
#define	DMA2_CURR_ADDR			0xFFC00CA4	/* DMA Channel 2 Current Address Register */
#define	DMA2_IRQ_STATUS			0xFFC00CA8	/* DMA Channel 2 Interrupt/Status Register */
#define	DMA2_PERIPHERAL_MAP		0xFFC00CAC	/* DMA Channel 2 Peripheral Map	Register */
#define	DMA2_CURR_X_COUNT		0xFFC00CB0	/* DMA Channel 2 Current X Count Register */
#define	DMA2_CURR_Y_COUNT		0xFFC00CB8	/* DMA Channel 2 Current Y Count Register */

#define	DMA3_NEXT_DESC_PTR		0xFFC00CC0	/* DMA Channel 3 Next Descriptor Pointer Register */
#define	DMA3_START_ADDR			0xFFC00CC4	/* DMA Channel 3 Start Address Register */
#define	DMA3_CONFIG				0xFFC00CC8	/* DMA Channel 3 Configuration Register */
#define	DMA3_X_COUNT			0xFFC00CD0	/* DMA Channel 3 X Count Register */
#define	DMA3_X_MODIFY			0xFFC00CD4	/* DMA Channel 3 X Modify Register */
#define	DMA3_Y_COUNT			0xFFC00CD8	/* DMA Channel 3 Y Count Register */
#define	DMA3_Y_MODIFY			0xFFC00CDC	/* DMA Channel 3 Y Modify Register */
#define	DMA3_CURR_DESC_PTR		0xFFC00CE0	/* DMA Channel 3 Current Descriptor Pointer Register */
#define	DMA3_CURR_ADDR			0xFFC00CE4	/* DMA Channel 3 Current Address Register */
#define	DMA3_IRQ_STATUS			0xFFC00CE8	/* DMA Channel 3 Interrupt/Status Register */
#define	DMA3_PERIPHERAL_MAP		0xFFC00CEC	/* DMA Channel 3 Peripheral Map	Register */
#define	DMA3_CURR_X_COUNT		0xFFC00CF0	/* DMA Channel 3 Current X Count Register */
#define	DMA3_CURR_Y_COUNT		0xFFC00CF8	/* DMA Channel 3 Current Y Count Register */

#define	DMA4_NEXT_DESC_PTR		0xFFC00D00	/* DMA Channel 4 Next Descriptor Pointer Register */
#define	DMA4_START_ADDR			0xFFC00D04	/* DMA Channel 4 Start Address Register */
#define	DMA4_CONFIG				0xFFC00D08	/* DMA Channel 4 Configuration Register */
#define	DMA4_X_COUNT			0xFFC00D10	/* DMA Channel 4 X Count Register */
#define	DMA4_X_MODIFY			0xFFC00D14	/* DMA Channel 4 X Modify Register */
#define	DMA4_Y_COUNT			0xFFC00D18	/* DMA Channel 4 Y Count Register */
#define	DMA4_Y_MODIFY			0xFFC00D1C	/* DMA Channel 4 Y Modify Register */
#define	DMA4_CURR_DESC_PTR		0xFFC00D20	/* DMA Channel 4 Current Descriptor Pointer Register */
#define	DMA4_CURR_ADDR			0xFFC00D24	/* DMA Channel 4 Current Address Register */
#define	DMA4_IRQ_STATUS			0xFFC00D28	/* DMA Channel 4 Interrupt/Status Register */
#define	DMA4_PERIPHERAL_MAP		0xFFC00D2C	/* DMA Channel 4 Peripheral Map	Register */
#define	DMA4_CURR_X_COUNT		0xFFC00D30	/* DMA Channel 4 Current X Count Register */
#define	DMA4_CURR_Y_COUNT		0xFFC00D38	/* DMA Channel 4 Current Y Count Register */

#define	DMA5_NEXT_DESC_PTR		0xFFC00D40	/* DMA Channel 5 Next Descriptor Pointer Register */
#define	DMA5_START_ADDR			0xFFC00D44	/* DMA Channel 5 Start Address Register */
#define	DMA5_CONFIG				0xFFC00D48	/* DMA Channel 5 Configuration Register */
#define	DMA5_X_COUNT			0xFFC00D50	/* DMA Channel 5 X Count Register */
#define	DMA5_X_MODIFY			0xFFC00D54	/* DMA Channel 5 X Modify Register */
#define	DMA5_Y_COUNT			0xFFC00D58	/* DMA Channel 5 Y Count Register */
#define	DMA5_Y_MODIFY			0xFFC00D5C	/* DMA Channel 5 Y Modify Register */
#define	DMA5_CURR_DESC_PTR		0xFFC00D60	/* DMA Channel 5 Current Descriptor Pointer Register */
#define	DMA5_CURR_ADDR			0xFFC00D64	/* DMA Channel 5 Current Address Register */
#define	DMA5_IRQ_STATUS			0xFFC00D68	/* DMA Channel 5 Interrupt/Status Register */
#define	DMA5_PERIPHERAL_MAP		0xFFC00D6C	/* DMA Channel 5 Peripheral Map	Register */
#define	DMA5_CURR_X_COUNT		0xFFC00D70	/* DMA Channel 5 Current X Count Register */
#define	DMA5_CURR_Y_COUNT		0xFFC00D78	/* DMA Channel 5 Current Y Count Register */

#define	DMA6_NEXT_DESC_PTR		0xFFC00D80	/* DMA Channel 6 Next Descriptor Pointer Register */
#define	DMA6_START_ADDR			0xFFC00D84	/* DMA Channel 6 Start Address Register */
#define	DMA6_CONFIG				0xFFC00D88	/* DMA Channel 6 Configuration Register */
#define	DMA6_X_COUNT			0xFFC00D90	/* DMA Channel 6 X Count Register */
#define	DMA6_X_MODIFY			0xFFC00D94	/* DMA Channel 6 X Modify Register */
#define	DMA6_Y_COUNT			0xFFC00D98	/* DMA Channel 6 Y Count Register */
#define	DMA6_Y_MODIFY			0xFFC00D9C	/* DMA Channel 6 Y Modify Register */
#define	DMA6_CURR_DESC_PTR		0xFFC00DA0	/* DMA Channel 6 Current Descriptor Pointer Register */
#define	DMA6_CURR_ADDR			0xFFC00DA4	/* DMA Channel 6 Current Address Register */
#define	DMA6_IRQ_STATUS			0xFFC00DA8	/* DMA Channel 6 Interrupt/Status Register */
#define	DMA6_PERIPHERAL_MAP		0xFFC00DAC	/* DMA Channel 6 Peripheral Map	Register */
#define	DMA6_CURR_X_COUNT		0xFFC00DB0	/* DMA Channel 6 Current X Count Register */
#define	DMA6_CURR_Y_COUNT		0xFFC00DB8	/* DMA Channel 6 Current Y Count Register */

#define	DMA7_NEXT_DESC_PTR		0xFFC00DC0	/* DMA Channel 7 Next Descriptor Pointer Register */
#define	DMA7_START_ADDR			0xFFC00DC4	/* DMA Channel 7 Start Address Register */
#define	DMA7_CONFIG				0xFFC00DC8	/* DMA Channel 7 Configuration Register */
#define	DMA7_X_COUNT			0xFFC00DD0	/* DMA Channel 7 X Count Register */
#define	DMA7_X_MODIFY			0xFFC00DD4	/* DMA Channel 7 X Modify Register */
#define	DMA7_Y_COUNT			0xFFC00DD8	/* DMA Channel 7 Y Count Register */
#define	DMA7_Y_MODIFY			0xFFC00DDC	/* DMA Channel 7 Y Modify Register */
#define	DMA7_CURR_DESC_PTR		0xFFC00DE0	/* DMA Channel 7 Current Descriptor Pointer Register */
#define	DMA7_CURR_ADDR			0xFFC00DE4	/* DMA Channel 7 Current Address Register */
#define	DMA7_IRQ_STATUS			0xFFC00DE8	/* DMA Channel 7 Interrupt/Status Register */
#define	DMA7_PERIPHERAL_MAP		0xFFC00DEC	/* DMA Channel 7 Peripheral Map	Register */
#define	DMA7_CURR_X_COUNT		0xFFC00DF0	/* DMA Channel 7 Current X Count Register */
#define	DMA7_CURR_Y_COUNT		0xFFC00DF8	/* DMA Channel 7 Current Y Count Register */

#define	MDMA0_D0_NEXT_DESC_PTR	0xFFC00E00	/* MemDMA0 Stream 0 Destination	Next Descriptor	Pointer	Register */
#define	MDMA0_D0_START_ADDR		0xFFC00E04	/* MemDMA0 Stream 0 Destination	Start Address Register */
#define	MDMA0_D0_CONFIG			0xFFC00E08	/* MemDMA0 Stream 0 Destination	Configuration Register */
#define	MDMA0_D0_X_COUNT		0xFFC00E10	/* MemDMA0 Stream 0 Destination	X Count	Register */
#define	MDMA0_D0_X_MODIFY		0xFFC00E14	/* MemDMA0 Stream 0 Destination	X Modify Register */
#define	MDMA0_D0_Y_COUNT		0xFFC00E18	/* MemDMA0 Stream 0 Destination	Y Count	Register */
#define	MDMA0_D0_Y_MODIFY		0xFFC00E1C	/* MemDMA0 Stream 0 Destination	Y Modify Register */
#define	MDMA0_D0_CURR_DESC_PTR	0xFFC00E20	/* MemDMA0 Stream 0 Destination	Current	Descriptor Pointer Register */
#define	MDMA0_D0_CURR_ADDR		0xFFC00E24	/* MemDMA0 Stream 0 Destination	Current	Address	Register */
#define	MDMA0_D0_IRQ_STATUS		0xFFC00E28	/* MemDMA0 Stream 0 Destination	Interrupt/Status Register */
#define	MDMA0_D0_PERIPHERAL_MAP	0xFFC00E2C	/* MemDMA0 Stream 0 Destination	Peripheral Map Register */
#define	MDMA0_D0_CURR_X_COUNT	0xFFC00E30	/* MemDMA0 Stream 0 Destination	Current	X Count	Register */
#define	MDMA0_D0_CURR_Y_COUNT	0xFFC00E38	/* MemDMA0 Stream 0 Destination	Current	Y Count	Register */

#define	MDMA0_S0_NEXT_DESC_PTR	0xFFC00E40	/* MemDMA0 Stream 0 Source Next	Descriptor Pointer Register */
#define	MDMA0_S0_START_ADDR		0xFFC00E44	/* MemDMA0 Stream 0 Source Start Address Register */
#define	MDMA0_S0_CONFIG			0xFFC00E48	/* MemDMA0 Stream 0 Source Configuration Register */
#define	MDMA0_S0_X_COUNT		0xFFC00E50	/* MemDMA0 Stream 0 Source X Count Register */
#define	MDMA0_S0_X_MODIFY		0xFFC00E54	/* MemDMA0 Stream 0 Source X Modify Register */
#define	MDMA0_S0_Y_COUNT		0xFFC00E58	/* MemDMA0 Stream 0 Source Y Count Register */
#define	MDMA0_S0_Y_MODIFY		0xFFC00E5C	/* MemDMA0 Stream 0 Source Y Modify Register */
#define	MDMA0_S0_CURR_DESC_PTR	0xFFC00E60	/* MemDMA0 Stream 0 Source Current Descriptor Pointer Register */
#define	MDMA0_S0_CURR_ADDR		0xFFC00E64	/* MemDMA0 Stream 0 Source Current Address Register */
#define	MDMA0_S0_IRQ_STATUS		0xFFC00E68	/* MemDMA0 Stream 0 Source Interrupt/Status Register */
#define	MDMA0_S0_PERIPHERAL_MAP	0xFFC00E6C	/* MemDMA0 Stream 0 Source Peripheral Map Register */
#define	MDMA0_S0_CURR_X_COUNT	0xFFC00E70	/* MemDMA0 Stream 0 Source Current X Count Register */
#define	MDMA0_S0_CURR_Y_COUNT	0xFFC00E78	/* MemDMA0 Stream 0 Source Current Y Count Register */

#define	MDMA0_D1_NEXT_DESC_PTR	0xFFC00E80	/* MemDMA0 Stream 1 Destination	Next Descriptor	Pointer	Register */
#define	MDMA0_D1_START_ADDR		0xFFC00E84	/* MemDMA0 Stream 1 Destination	Start Address Register */
#define	MDMA0_D1_CONFIG			0xFFC00E88	/* MemDMA0 Stream 1 Destination	Configuration Register */
#define	MDMA0_D1_X_COUNT		0xFFC00E90	/* MemDMA0 Stream 1 Destination	X Count	Register */
#define	MDMA0_D1_X_MODIFY		0xFFC00E94	/* MemDMA0 Stream 1 Destination	X Modify Register */
#define	MDMA0_D1_Y_COUNT		0xFFC00E98	/* MemDMA0 Stream 1 Destination	Y Count	Register */
#define	MDMA0_D1_Y_MODIFY		0xFFC00E9C	/* MemDMA0 Stream 1 Destination	Y Modify Register */
#define	MDMA0_D1_CURR_DESC_PTR	0xFFC00EA0	/* MemDMA0 Stream 1 Destination	Current	Descriptor Pointer Register */
#define	MDMA0_D1_CURR_ADDR		0xFFC00EA4	/* MemDMA0 Stream 1 Destination	Current	Address	Register */
#define	MDMA0_D1_IRQ_STATUS		0xFFC00EA8	/* MemDMA0 Stream 1 Destination	Interrupt/Status Register */
#define	MDMA0_D1_PERIPHERAL_MAP	0xFFC00EAC	/* MemDMA0 Stream 1 Destination	Peripheral Map Register */
#define	MDMA0_D1_CURR_X_COUNT	0xFFC00EB0	/* MemDMA0 Stream 1 Destination	Current	X Count	Register */
#define	MDMA0_D1_CURR_Y_COUNT	0xFFC00EB8	/* MemDMA0 Stream 1 Destination	Current	Y Count	Register */

#define	MDMA0_S1_NEXT_DESC_PTR	0xFFC00EC0	/* MemDMA0 Stream 1 Source Next	Descriptor Pointer Register */
#define	MDMA0_S1_START_ADDR		0xFFC00EC4	/* MemDMA0 Stream 1 Source Start Address Register */
#define	MDMA0_S1_CONFIG			0xFFC00EC8	/* MemDMA0 Stream 1 Source Configuration Register */
#define	MDMA0_S1_X_COUNT		0xFFC00ED0	/* MemDMA0 Stream 1 Source X Count Register */
#define	MDMA0_S1_X_MODIFY		0xFFC00ED4	/* MemDMA0 Stream 1 Source X Modify Register */
#define	MDMA0_S1_Y_COUNT		0xFFC00ED8	/* MemDMA0 Stream 1 Source Y Count Register */
#define	MDMA0_S1_Y_MODIFY		0xFFC00EDC	/* MemDMA0 Stream 1 Source Y Modify Register */
#define	MDMA0_S1_CURR_DESC_PTR	0xFFC00EE0	/* MemDMA0 Stream 1 Source Current Descriptor Pointer Register */
#define	MDMA0_S1_CURR_ADDR		0xFFC00EE4	/* MemDMA0 Stream 1 Source Current Address Register */
#define	MDMA0_S1_IRQ_STATUS		0xFFC00EE8	/* MemDMA0 Stream 1 Source Interrupt/Status Register */
#define	MDMA0_S1_PERIPHERAL_MAP	0xFFC00EEC	/* MemDMA0 Stream 1 Source Peripheral Map Register */
#define	MDMA0_S1_CURR_X_COUNT	0xFFC00EF0	/* MemDMA0 Stream 1 Source Current X Count Register */
#define	MDMA0_S1_CURR_Y_COUNT	0xFFC00EF8	/* MemDMA0 Stream 1 Source Current Y Count Register */

#define MDMA_D0_NEXT_DESC_PTR MDMA0_D0_NEXT_DESC_PTR
#define MDMA_D0_START_ADDR MDMA0_D0_START_ADDR
#define MDMA_D0_CONFIG MDMA0_D0_CONFIG
#define MDMA_D0_X_COUNT MDMA0_D0_X_COUNT
#define MDMA_D0_X_MODIFY MDMA0_D0_X_MODIFY
#define MDMA_D0_Y_COUNT MDMA0_D0_Y_COUNT
#define MDMA_D0_Y_MODIFY MDMA0_D0_Y_MODIFY
#define MDMA_D0_CURR_DESC_PTR MDMA0_D0_CURR_DESC_PTR
#define MDMA_D0_CURR_ADDR MDMA0_D0_CURR_ADDR
#define MDMA_D0_IRQ_STATUS MDMA0_D0_IRQ_STATUS
#define MDMA_D0_PERIPHERAL_MAP MDMA0_D0_PERIPHERAL_MAP
#define MDMA_D0_CURR_X_COUNT MDMA0_D0_CURR_X_COUNT
#define MDMA_D0_CURR_Y_COUNT MDMA0_D0_CURR_Y_COUNT

#define MDMA_S0_NEXT_DESC_PTR MDMA0_S0_NEXT_DESC_PTR
#define MDMA_S0_START_ADDR MDMA0_S0_START_ADDR
#define MDMA_S0_CONFIG MDMA0_S0_CONFIG
#define MDMA_S0_X_COUNT MDMA0_S0_X_COUNT
#define MDMA_S0_X_MODIFY MDMA0_S0_X_MODIFY
#define MDMA_S0_Y_COUNT MDMA0_S0_Y_COUNT
#define MDMA_S0_Y_MODIFY MDMA0_S0_Y_MODIFY
#define MDMA_S0_CURR_DESC_PTR MDMA0_S0_CURR_DESC_PTR
#define MDMA_S0_CURR_ADDR MDMA0_S0_CURR_ADDR
#define MDMA_S0_IRQ_STATUS MDMA0_S0_IRQ_STATUS
#define MDMA_S0_PERIPHERAL_MAP MDMA0_S0_PERIPHERAL_MAP
#define MDMA_S0_CURR_X_COUNT MDMA0_S0_CURR_X_COUNT
#define MDMA_S0_CURR_Y_COUNT MDMA0_S0_CURR_Y_COUNT

#define MDMA_D1_NEXT_DESC_PTR MDMA0_D1_NEXT_DESC_PTR
#define MDMA_D1_START_ADDR MDMA0_D1_START_ADDR
#define MDMA_D1_CONFIG MDMA0_D1_CONFIG
#define MDMA_D1_X_COUNT MDMA0_D1_X_COUNT
#define MDMA_D1_X_MODIFY MDMA0_D1_X_MODIFY
#define MDMA_D1_Y_COUNT MDMA0_D1_Y_COUNT
#define MDMA_D1_Y_MODIFY MDMA0_D1_Y_MODIFY
#define MDMA_D1_CURR_DESC_PTR MDMA0_D1_CURR_DESC_PTR
#define MDMA_D1_CURR_ADDR MDMA0_D1_CURR_ADDR
#define MDMA_D1_IRQ_STATUS MDMA0_D1_IRQ_STATUS
#define MDMA_D1_PERIPHERAL_MAP MDMA0_D1_PERIPHERAL_MAP
#define MDMA_D1_CURR_X_COUNT MDMA0_D1_CURR_X_COUNT
#define MDMA_D1_CURR_Y_COUNT MDMA0_D1_CURR_Y_COUNT

#define MDMA_S1_NEXT_DESC_PTR MDMA0_S1_NEXT_DESC_PTR
#define MDMA_S1_START_ADDR MDMA0_S1_START_ADDR
#define MDMA_S1_CONFIG MDMA0_S1_CONFIG
#define MDMA_S1_X_COUNT MDMA0_S1_X_COUNT
#define MDMA_S1_X_MODIFY MDMA0_S1_X_MODIFY
#define MDMA_S1_Y_COUNT MDMA0_S1_Y_COUNT
#define MDMA_S1_Y_MODIFY MDMA0_S1_Y_MODIFY
#define MDMA_S1_CURR_DESC_PTR MDMA0_S1_CURR_DESC_PTR
#define MDMA_S1_CURR_ADDR MDMA0_S1_CURR_ADDR
#define MDMA_S1_IRQ_STATUS MDMA0_S1_IRQ_STATUS
#define MDMA_S1_PERIPHERAL_MAP MDMA0_S1_PERIPHERAL_MAP
#define MDMA_S1_CURR_X_COUNT MDMA0_S1_CURR_X_COUNT
#define MDMA_S1_CURR_Y_COUNT MDMA0_S1_CURR_Y_COUNT


/* Parallel Peripheral Interface (PPI) (0xFFC01000 - 0xFFC010FF) */
#define	PPI_CONTROL			0xFFC01000	/* PPI Control Register */
#define	PPI_STATUS			0xFFC01004	/* PPI Status Register */
#define	PPI_COUNT			0xFFC01008	/* PPI Transfer	Count Register */
#define	PPI_DELAY			0xFFC0100C	/* PPI Delay Count Register */
#define	PPI_FRAME			0xFFC01010	/* PPI Frame Length Register */


/* Two-Wire Interface 0	(0xFFC01400 - 0xFFC014FF)			 */
#define	TWI0_CLKDIV			0xFFC01400	/* Serial Clock	Divider	Register */
#define	TWI0_CONTROL		0xFFC01404	/* TWI0	Master Internal	Time Reference Register */
#define	TWI0_SLAVE_CTRL		0xFFC01408	/* Slave Mode Control Register */
#define	TWI0_SLAVE_STAT		0xFFC0140C	/* Slave Mode Status Register */
#define	TWI0_SLAVE_ADDR		0xFFC01410	/* Slave Mode Address Register */
#define	TWI0_MASTER_CTRL	0xFFC01414	/* Master Mode Control Register */
#define	TWI0_MASTER_STAT	0xFFC01418	/* Master Mode Status Register */
#define	TWI0_MASTER_ADDR	0xFFC0141C	/* Master Mode Address Register */
#define	TWI0_INT_STAT		0xFFC01420	/* TWI0	Master Interrupt Register */
#define	TWI0_INT_MASK		0xFFC01424	/* TWI0	Master Interrupt Mask Register */
#define	TWI0_FIFO_CTRL		0xFFC01428	/* FIFO	Control	Register */
#define	TWI0_FIFO_STAT		0xFFC0142C	/* FIFO	Status Register */
#define	TWI0_XMT_DATA8		0xFFC01480	/* FIFO	Transmit Data Single Byte Register */
#define	TWI0_XMT_DATA16		0xFFC01484	/* FIFO	Transmit Data Double Byte Register */
#define	TWI0_RCV_DATA8		0xFFC01488	/* FIFO	Receive	Data Single Byte Register */
#define	TWI0_RCV_DATA16		0xFFC0148C	/* FIFO	Receive	Data Double Byte Register */

#define TWI0_REGBASE		TWI0_CLKDIV

/* the following are for backwards compatibility */
#define	TWI0_PRESCALE	 TWI0_CONTROL
#define	TWI0_INT_SRC	 TWI0_INT_STAT
#define	TWI0_INT_ENABLE	 TWI0_INT_MASK


/* General-Purpose Ports  (0xFFC01500 -	0xFFC015FF)	 */

/* GPIO	Port C Register	Names */
#define	GPIO_C_CNFG			0xFFC01500	/* GPIO	Pin Port C Configuration Register */
#define	GPIO_C_D			0xFFC01510	/* GPIO	Pin Port C Data	Register */
#define	GPIO_C_C			0xFFC01520	/* Clear GPIO Pin Port C Register */
#define	GPIO_C_S			0xFFC01530	/* Set GPIO Pin	Port C Register */
#define	GPIO_C_T			0xFFC01540	/* Toggle GPIO Pin Port	C Register */
#define	GPIO_C_DIR			0xFFC01550	/* GPIO	Pin Port C Direction Register */
#define	GPIO_C_INEN			0xFFC01560	/* GPIO	Pin Port C Input Enable	Register */

/* GPIO	Port D Register	Names */
#define	GPIO_D_CNFG			0xFFC01504	/* GPIO	Pin Port D Configuration Register */
#define	GPIO_D_D			0xFFC01514	/* GPIO	Pin Port D Data	Register */
#define	GPIO_D_C			0xFFC01524	/* Clear GPIO Pin Port D Register */
#define	GPIO_D_S			0xFFC01534	/* Set GPIO Pin	Port D Register */
#define	GPIO_D_T			0xFFC01544	/* Toggle GPIO Pin Port	D Register */
#define	GPIO_D_DIR			0xFFC01554	/* GPIO	Pin Port D Direction Register */
#define	GPIO_D_INEN			0xFFC01564	/* GPIO	Pin Port D Input Enable	Register */

/* GPIO	Port E Register	Names */
#define	GPIO_E_CNFG			0xFFC01508	/* GPIO	Pin Port E Configuration Register */
#define	GPIO_E_D			0xFFC01518	/* GPIO	Pin Port E Data	Register */
#define	GPIO_E_C			0xFFC01528	/* Clear GPIO Pin Port E Register */
#define	GPIO_E_S			0xFFC01538	/* Set GPIO Pin	Port E Register */
#define	GPIO_E_T			0xFFC01548	/* Toggle GPIO Pin Port	E Register */
#define	GPIO_E_DIR			0xFFC01558	/* GPIO	Pin Port E Direction Register */
#define	GPIO_E_INEN			0xFFC01568	/* GPIO	Pin Port E Input Enable	Register */

/* DMA Controller 1 Traffic Control Registers (0xFFC01B00 - 0xFFC01BFF) */

#define	DMAC1_TC_PER			0xFFC01B0C	/* DMA Controller 1 Traffic Control Periods Register */
#define	DMAC1_TC_CNT			0xFFC01B10	/* DMA Controller 1 Traffic Control Current Counts Register */

/* Alternate deprecated	register names (below) provided	for backwards code compatibility */
#define	DMA1_TCPER			DMAC1_TC_PER
#define	DMA1_TCCNT			DMAC1_TC_CNT


/* DMA Controller 1 (0xFFC01C00	- 0xFFC01FFF)							 */
#define	DMA8_NEXT_DESC_PTR		0xFFC01C00	/* DMA Channel 8 Next Descriptor Pointer Register */
#define	DMA8_START_ADDR			0xFFC01C04	/* DMA Channel 8 Start Address Register */
#define	DMA8_CONFIG				0xFFC01C08	/* DMA Channel 8 Configuration Register */
#define	DMA8_X_COUNT			0xFFC01C10	/* DMA Channel 8 X Count Register */
#define	DMA8_X_MODIFY			0xFFC01C14	/* DMA Channel 8 X Modify Register */
#define	DMA8_Y_COUNT			0xFFC01C18	/* DMA Channel 8 Y Count Register */
#define	DMA8_Y_MODIFY			0xFFC01C1C	/* DMA Channel 8 Y Modify Register */
#define	DMA8_CURR_DESC_PTR		0xFFC01C20	/* DMA Channel 8 Current Descriptor Pointer Register */
#define	DMA8_CURR_ADDR			0xFFC01C24	/* DMA Channel 8 Current Address Register */
#define	DMA8_IRQ_STATUS			0xFFC01C28	/* DMA Channel 8 Interrupt/Status Register */
#define	DMA8_PERIPHERAL_MAP		0xFFC01C2C	/* DMA Channel 8 Peripheral Map	Register */
#define	DMA8_CURR_X_COUNT		0xFFC01C30	/* DMA Channel 8 Current X Count Register */
#define	DMA8_CURR_Y_COUNT		0xFFC01C38	/* DMA Channel 8 Current Y Count Register */

#define	DMA9_NEXT_DESC_PTR		0xFFC01C40	/* DMA Channel 9 Next Descriptor Pointer Register */
#define	DMA9_START_ADDR			0xFFC01C44	/* DMA Channel 9 Start Address Register */
#define	DMA9_CONFIG				0xFFC01C48	/* DMA Channel 9 Configuration Register */
#define	DMA9_X_COUNT			0xFFC01C50	/* DMA Channel 9 X Count Register */
#define	DMA9_X_MODIFY			0xFFC01C54	/* DMA Channel 9 X Modify Register */
#define	DMA9_Y_COUNT			0xFFC01C58	/* DMA Channel 9 Y Count Register */
#define	DMA9_Y_MODIFY			0xFFC01C5C	/* DMA Channel 9 Y Modify Register */
#define	DMA9_CURR_DESC_PTR		0xFFC01C60	/* DMA Channel 9 Current Descriptor Pointer Register */
#define	DMA9_CURR_ADDR			0xFFC01C64	/* DMA Channel 9 Current Address Register */
#define	DMA9_IRQ_STATUS			0xFFC01C68	/* DMA Channel 9 Interrupt/Status Register */
#define	DMA9_PERIPHERAL_MAP		0xFFC01C6C	/* DMA Channel 9 Peripheral Map	Register */
#define	DMA9_CURR_X_COUNT		0xFFC01C70	/* DMA Channel 9 Current X Count Register */
#define	DMA9_CURR_Y_COUNT		0xFFC01C78	/* DMA Channel 9 Current Y Count Register */

#define	DMA10_NEXT_DESC_PTR		0xFFC01C80	/* DMA Channel 10 Next Descriptor Pointer Register */
#define	DMA10_START_ADDR		0xFFC01C84	/* DMA Channel 10 Start	Address	Register */
#define	DMA10_CONFIG			0xFFC01C88	/* DMA Channel 10 Configuration	Register */
#define	DMA10_X_COUNT			0xFFC01C90	/* DMA Channel 10 X Count Register */
#define	DMA10_X_MODIFY			0xFFC01C94	/* DMA Channel 10 X Modify Register */
#define	DMA10_Y_COUNT			0xFFC01C98	/* DMA Channel 10 Y Count Register */
#define	DMA10_Y_MODIFY			0xFFC01C9C	/* DMA Channel 10 Y Modify Register */
#define	DMA10_CURR_DESC_PTR		0xFFC01CA0	/* DMA Channel 10 Current Descriptor Pointer Register */
#define	DMA10_CURR_ADDR			0xFFC01CA4	/* DMA Channel 10 Current Address Register */
#define	DMA10_IRQ_STATUS		0xFFC01CA8	/* DMA Channel 10 Interrupt/Status Register */
#define	DMA10_PERIPHERAL_MAP	0xFFC01CAC	/* DMA Channel 10 Peripheral Map Register */
#define	DMA10_CURR_X_COUNT		0xFFC01CB0	/* DMA Channel 10 Current X Count Register */
#define	DMA10_CURR_Y_COUNT		0xFFC01CB8	/* DMA Channel 10 Current Y Count Register */

#define	DMA11_NEXT_DESC_PTR		0xFFC01CC0	/* DMA Channel 11 Next Descriptor Pointer Register */
#define	DMA11_START_ADDR		0xFFC01CC4	/* DMA Channel 11 Start	Address	Register */
#define	DMA11_CONFIG			0xFFC01CC8	/* DMA Channel 11 Configuration	Register */
#define	DMA11_X_COUNT			0xFFC01CD0	/* DMA Channel 11 X Count Register */
#define	DMA11_X_MODIFY			0xFFC01CD4	/* DMA Channel 11 X Modify Register */
#define	DMA11_Y_COUNT			0xFFC01CD8	/* DMA Channel 11 Y Count Register */
#define	DMA11_Y_MODIFY			0xFFC01CDC	/* DMA Channel 11 Y Modify Register */
#define	DMA11_CURR_DESC_PTR		0xFFC01CE0	/* DMA Channel 11 Current Descriptor Pointer Register */
#define	DMA11_CURR_ADDR			0xFFC01CE4	/* DMA Channel 11 Current Address Register */
#define	DMA11_IRQ_STATUS		0xFFC01CE8	/* DMA Channel 11 Interrupt/Status Register */
#define	DMA11_PERIPHERAL_MAP	0xFFC01CEC	/* DMA Channel 11 Peripheral Map Register */
#define	DMA11_CURR_X_COUNT		0xFFC01CF0	/* DMA Channel 11 Current X Count Register */
#define	DMA11_CURR_Y_COUNT		0xFFC01CF8	/* DMA Channel 11 Current Y Count Register */

#define	DMA12_NEXT_DESC_PTR		0xFFC01D00	/* DMA Channel 12 Next Descriptor Pointer Register */
#define	DMA12_START_ADDR		0xFFC01D04	/* DMA Channel 12 Start	Address	Register */
#define	DMA12_CONFIG			0xFFC01D08	/* DMA Channel 12 Configuration	Register */
#define	DMA12_X_COUNT			0xFFC01D10	/* DMA Channel 12 X Count Register */
#define	DMA12_X_MODIFY			0xFFC01D14	/* DMA Channel 12 X Modify Register */
#define	DMA12_Y_COUNT			0xFFC01D18	/* DMA Channel 12 Y Count Register */
#define	DMA12_Y_MODIFY			0xFFC01D1C	/* DMA Channel 12 Y Modify Register */
#define	DMA12_CURR_DESC_PTR		0xFFC01D20	/* DMA Channel 12 Current Descriptor Pointer Register */
#define	DMA12_CURR_ADDR			0xFFC01D24	/* DMA Channel 12 Current Address Register */
#define	DMA12_IRQ_STATUS		0xFFC01D28	/* DMA Channel 12 Interrupt/Status Register */
#define	DMA12_PERIPHERAL_MAP	0xFFC01D2C	/* DMA Channel 12 Peripheral Map Register */
#define	DMA12_CURR_X_COUNT		0xFFC01D30	/* DMA Channel 12 Current X Count Register */
#define	DMA12_CURR_Y_COUNT		0xFFC01D38	/* DMA Channel 12 Current Y Count Register */

#define	DMA13_NEXT_DESC_PTR		0xFFC01D40	/* DMA Channel 13 Next Descriptor Pointer Register */
#define	DMA13_START_ADDR		0xFFC01D44	/* DMA Channel 13 Start	Address	Register */
#define	DMA13_CONFIG			0xFFC01D48	/* DMA Channel 13 Configuration	Register */
#define	DMA13_X_COUNT			0xFFC01D50	/* DMA Channel 13 X Count Register */
#define	DMA13_X_MODIFY			0xFFC01D54	/* DMA Channel 13 X Modify Register */
#define	DMA13_Y_COUNT			0xFFC01D58	/* DMA Channel 13 Y Count Register */
#define	DMA13_Y_MODIFY			0xFFC01D5C	/* DMA Channel 13 Y Modify Register */
#define	DMA13_CURR_DESC_PTR		0xFFC01D60	/* DMA Channel 13 Current Descriptor Pointer Register */
#define	DMA13_CURR_ADDR			0xFFC01D64	/* DMA Channel 13 Current Address Register */
#define	DMA13_IRQ_STATUS		0xFFC01D68	/* DMA Channel 13 Interrupt/Status Register */
#define	DMA13_PERIPHERAL_MAP	0xFFC01D6C	/* DMA Channel 13 Peripheral Map Register */
#define	DMA13_CURR_X_COUNT		0xFFC01D70	/* DMA Channel 13 Current X Count Register */
#define	DMA13_CURR_Y_COUNT		0xFFC01D78	/* DMA Channel 13 Current Y Count Register */

#define	DMA14_NEXT_DESC_PTR		0xFFC01D80	/* DMA Channel 14 Next Descriptor Pointer Register */
#define	DMA14_START_ADDR		0xFFC01D84	/* DMA Channel 14 Start	Address	Register */
#define	DMA14_CONFIG			0xFFC01D88	/* DMA Channel 14 Configuration	Register */
#define	DMA14_X_COUNT			0xFFC01D90	/* DMA Channel 14 X Count Register */
#define	DMA14_X_MODIFY			0xFFC01D94	/* DMA Channel 14 X Modify Register */
#define	DMA14_Y_COUNT			0xFFC01D98	/* DMA Channel 14 Y Count Register */
#define	DMA14_Y_MODIFY			0xFFC01D9C	/* DMA Channel 14 Y Modify Register */
#define	DMA14_CURR_DESC_PTR		0xFFC01DA0	/* DMA Channel 14 Current Descriptor Pointer Register */
#define	DMA14_CURR_ADDR			0xFFC01DA4	/* DMA Channel 14 Current Address Register */
#define	DMA14_IRQ_STATUS		0xFFC01DA8	/* DMA Channel 14 Interrupt/Status Register */
#define	DMA14_PERIPHERAL_MAP	0xFFC01DAC	/* DMA Channel 14 Peripheral Map Register */
#define	DMA14_CURR_X_COUNT		0xFFC01DB0	/* DMA Channel 14 Current X Count Register */
#define	DMA14_CURR_Y_COUNT		0xFFC01DB8	/* DMA Channel 14 Current Y Count Register */

#define	DMA15_NEXT_DESC_PTR		0xFFC01DC0	/* DMA Channel 15 Next Descriptor Pointer Register */
#define	DMA15_START_ADDR		0xFFC01DC4	/* DMA Channel 15 Start	Address	Register */
#define	DMA15_CONFIG			0xFFC01DC8	/* DMA Channel 15 Configuration	Register */
#define	DMA15_X_COUNT			0xFFC01DD0	/* DMA Channel 15 X Count Register */
#define	DMA15_X_MODIFY			0xFFC01DD4	/* DMA Channel 15 X Modify Register */
#define	DMA15_Y_COUNT			0xFFC01DD8	/* DMA Channel 15 Y Count Register */
#define	DMA15_Y_MODIFY			0xFFC01DDC	/* DMA Channel 15 Y Modify Register */
#define	DMA15_CURR_DESC_PTR		0xFFC01DE0	/* DMA Channel 15 Current Descriptor Pointer Register */
#define	DMA15_CURR_ADDR			0xFFC01DE4	/* DMA Channel 15 Current Address Register */
#define	DMA15_IRQ_STATUS		0xFFC01DE8	/* DMA Channel 15 Interrupt/Status Register */
#define	DMA15_PERIPHERAL_MAP	0xFFC01DEC	/* DMA Channel 15 Peripheral Map Register */
#define	DMA15_CURR_X_COUNT		0xFFC01DF0	/* DMA Channel 15 Current X Count Register */
#define	DMA15_CURR_Y_COUNT		0xFFC01DF8	/* DMA Channel 15 Current Y Count Register */

#define	DMA16_NEXT_DESC_PTR		0xFFC01E00	/* DMA Channel 16 Next Descriptor Pointer Register */
#define	DMA16_START_ADDR		0xFFC01E04	/* DMA Channel 16 Start	Address	Register */
#define	DMA16_CONFIG			0xFFC01E08	/* DMA Channel 16 Configuration	Register */
#define	DMA16_X_COUNT			0xFFC01E10	/* DMA Channel 16 X Count Register */
#define	DMA16_X_MODIFY			0xFFC01E14	/* DMA Channel 16 X Modify Register */
#define	DMA16_Y_COUNT			0xFFC01E18	/* DMA Channel 16 Y Count Register */
#define	DMA16_Y_MODIFY			0xFFC01E1C	/* DMA Channel 16 Y Modify Register */
#define	DMA16_CURR_DESC_PTR		0xFFC01E20	/* DMA Channel 16 Current Descriptor Pointer Register */
#define	DMA16_CURR_ADDR			0xFFC01E24	/* DMA Channel 16 Current Address Register */
#define	DMA16_IRQ_STATUS		0xFFC01E28	/* DMA Channel 16 Interrupt/Status Register */
#define	DMA16_PERIPHERAL_MAP	0xFFC01E2C	/* DMA Channel 16 Peripheral Map Register */
#define	DMA16_CURR_X_COUNT		0xFFC01E30	/* DMA Channel 16 Current X Count Register */
#define	DMA16_CURR_Y_COUNT		0xFFC01E38	/* DMA Channel 16 Current Y Count Register */

#define	DMA17_NEXT_DESC_PTR		0xFFC01E40	/* DMA Channel 17 Next Descriptor Pointer Register */
#define	DMA17_START_ADDR		0xFFC01E44	/* DMA Channel 17 Start	Address	Register */
#define	DMA17_CONFIG			0xFFC01E48	/* DMA Channel 17 Configuration	Register */
#define	DMA17_X_COUNT			0xFFC01E50	/* DMA Channel 17 X Count Register */
#define	DMA17_X_MODIFY			0xFFC01E54	/* DMA Channel 17 X Modify Register */
#define	DMA17_Y_COUNT			0xFFC01E58	/* DMA Channel 17 Y Count Register */
#define	DMA17_Y_MODIFY			0xFFC01E5C	/* DMA Channel 17 Y Modify Register */
#define	DMA17_CURR_DESC_PTR		0xFFC01E60	/* DMA Channel 17 Current Descriptor Pointer Register */
#define	DMA17_CURR_ADDR			0xFFC01E64	/* DMA Channel 17 Current Address Register */
#define	DMA17_IRQ_STATUS		0xFFC01E68	/* DMA Channel 17 Interrupt/Status Register */
#define	DMA17_PERIPHERAL_MAP	0xFFC01E6C	/* DMA Channel 17 Peripheral Map Register */
#define	DMA17_CURR_X_COUNT		0xFFC01E70	/* DMA Channel 17 Current X Count Register */
#define	DMA17_CURR_Y_COUNT		0xFFC01E78	/* DMA Channel 17 Current Y Count Register */

#define	DMA18_NEXT_DESC_PTR		0xFFC01E80	/* DMA Channel 18 Next Descriptor Pointer Register */
#define	DMA18_START_ADDR		0xFFC01E84	/* DMA Channel 18 Start	Address	Register */
#define	DMA18_CONFIG			0xFFC01E88	/* DMA Channel 18 Configuration	Register */
#define	DMA18_X_COUNT			0xFFC01E90	/* DMA Channel 18 X Count Register */
#define	DMA18_X_MODIFY			0xFFC01E94	/* DMA Channel 18 X Modify Register */
#define	DMA18_Y_COUNT			0xFFC01E98	/* DMA Channel 18 Y Count Register */
#define	DMA18_Y_MODIFY			0xFFC01E9C	/* DMA Channel 18 Y Modify Register */
#define	DMA18_CURR_DESC_PTR		0xFFC01EA0	/* DMA Channel 18 Current Descriptor Pointer Register */
#define	DMA18_CURR_ADDR			0xFFC01EA4	/* DMA Channel 18 Current Address Register */
#define	DMA18_IRQ_STATUS		0xFFC01EA8	/* DMA Channel 18 Interrupt/Status Register */
#define	DMA18_PERIPHERAL_MAP	0xFFC01EAC	/* DMA Channel 18 Peripheral Map Register */
#define	DMA18_CURR_X_COUNT		0xFFC01EB0	/* DMA Channel 18 Current X Count Register */
#define	DMA18_CURR_Y_COUNT		0xFFC01EB8	/* DMA Channel 18 Current Y Count Register */

#define	DMA19_NEXT_DESC_PTR		0xFFC01EC0	/* DMA Channel 19 Next Descriptor Pointer Register */
#define	DMA19_START_ADDR		0xFFC01EC4	/* DMA Channel 19 Start	Address	Register */
#define	DMA19_CONFIG			0xFFC01EC8	/* DMA Channel 19 Configuration	Register */
#define	DMA19_X_COUNT			0xFFC01ED0	/* DMA Channel 19 X Count Register */
#define	DMA19_X_MODIFY			0xFFC01ED4	/* DMA Channel 19 X Modify Register */
#define	DMA19_Y_COUNT			0xFFC01ED8	/* DMA Channel 19 Y Count Register */
#define	DMA19_Y_MODIFY			0xFFC01EDC	/* DMA Channel 19 Y Modify Register */
#define	DMA19_CURR_DESC_PTR		0xFFC01EE0	/* DMA Channel 19 Current Descriptor Pointer Register */
#define	DMA19_CURR_ADDR			0xFFC01EE4	/* DMA Channel 19 Current Address Register */
#define	DMA19_IRQ_STATUS		0xFFC01EE8	/* DMA Channel 19 Interrupt/Status Register */
#define	DMA19_PERIPHERAL_MAP	0xFFC01EEC	/* DMA Channel 19 Peripheral Map Register */
#define	DMA19_CURR_X_COUNT		0xFFC01EF0	/* DMA Channel 19 Current X Count Register */
#define	DMA19_CURR_Y_COUNT		0xFFC01EF8	/* DMA Channel 19 Current Y Count Register */

#define	MDMA1_D0_NEXT_DESC_PTR	0xFFC01F00	/* MemDMA1 Stream 0 Destination	Next Descriptor	Pointer	Register */
#define	MDMA1_D0_START_ADDR		0xFFC01F04	/* MemDMA1 Stream 0 Destination	Start Address Register */
#define	MDMA1_D0_CONFIG			0xFFC01F08	/* MemDMA1 Stream 0 Destination	Configuration Register */
#define	MDMA1_D0_X_COUNT		0xFFC01F10	/* MemDMA1 Stream 0 Destination	X Count	Register */
#define	MDMA1_D0_X_MODIFY		0xFFC01F14	/* MemDMA1 Stream 0 Destination	X Modify Register */
#define	MDMA1_D0_Y_COUNT		0xFFC01F18	/* MemDMA1 Stream 0 Destination	Y Count	Register */
#define	MDMA1_D0_Y_MODIFY		0xFFC01F1C	/* MemDMA1 Stream 0 Destination	Y Modify Register */
#define	MDMA1_D0_CURR_DESC_PTR	0xFFC01F20	/* MemDMA1 Stream 0 Destination	Current	Descriptor Pointer Register */
#define	MDMA1_D0_CURR_ADDR		0xFFC01F24	/* MemDMA1 Stream 0 Destination	Current	Address	Register */
#define	MDMA1_D0_IRQ_STATUS		0xFFC01F28	/* MemDMA1 Stream 0 Destination	Interrupt/Status Register */
#define	MDMA1_D0_PERIPHERAL_MAP	0xFFC01F2C	/* MemDMA1 Stream 0 Destination	Peripheral Map Register */
#define	MDMA1_D0_CURR_X_COUNT	0xFFC01F30	/* MemDMA1 Stream 0 Destination	Current	X Count	Register */
#define	MDMA1_D0_CURR_Y_COUNT	0xFFC01F38	/* MemDMA1 Stream 0 Destination	Current	Y Count	Register */

#define	MDMA1_S0_NEXT_DESC_PTR	0xFFC01F40	/* MemDMA1 Stream 0 Source Next	Descriptor Pointer Register */
#define	MDMA1_S0_START_ADDR		0xFFC01F44	/* MemDMA1 Stream 0 Source Start Address Register */
#define	MDMA1_S0_CONFIG			0xFFC01F48	/* MemDMA1 Stream 0 Source Configuration Register */
#define	MDMA1_S0_X_COUNT		0xFFC01F50	/* MemDMA1 Stream 0 Source X Count Register */
#define	MDMA1_S0_X_MODIFY		0xFFC01F54	/* MemDMA1 Stream 0 Source X Modify Register */
#define	MDMA1_S0_Y_COUNT		0xFFC01F58	/* MemDMA1 Stream 0 Source Y Count Register */
#define	MDMA1_S0_Y_MODIFY		0xFFC01F5C	/* MemDMA1 Stream 0 Source Y Modify Register */
#define	MDMA1_S0_CURR_DESC_PTR	0xFFC01F60	/* MemDMA1 Stream 0 Source Current Descriptor Pointer Register */
#define	MDMA1_S0_CURR_ADDR		0xFFC01F64	/* MemDMA1 Stream 0 Source Current Address Register */
#define	MDMA1_S0_IRQ_STATUS		0xFFC01F68	/* MemDMA1 Stream 0 Source Interrupt/Status Register */
#define	MDMA1_S0_PERIPHERAL_MAP	0xFFC01F6C	/* MemDMA1 Stream 0 Source Peripheral Map Register */
#define	MDMA1_S0_CURR_X_COUNT	0xFFC01F70	/* MemDMA1 Stream 0 Source Current X Count Register */
#define	MDMA1_S0_CURR_Y_COUNT	0xFFC01F78	/* MemDMA1 Stream 0 Source Current Y Count Register */

#define	MDMA1_D1_NEXT_DESC_PTR	0xFFC01F80	/* MemDMA1 Stream 1 Destination	Next Descriptor	Pointer	Register */
#define	MDMA1_D1_START_ADDR		0xFFC01F84	/* MemDMA1 Stream 1 Destination	Start Address Register */
#define	MDMA1_D1_CONFIG			0xFFC01F88	/* MemDMA1 Stream 1 Destination	Configuration Register */
#define	MDMA1_D1_X_COUNT		0xFFC01F90	/* MemDMA1 Stream 1 Destination	X Count	Register */
#define	MDMA1_D1_X_MODIFY		0xFFC01F94	/* MemDMA1 Stream 1 Destination	X Modify Register */
#define	MDMA1_D1_Y_COUNT		0xFFC01F98	/* MemDMA1 Stream 1 Destination	Y Count	Register */
#define	MDMA1_D1_Y_MODIFY		0xFFC01F9C	/* MemDMA1 Stream 1 Destination	Y Modify Register */
#define	MDMA1_D1_CURR_DESC_PTR	0xFFC01FA0	/* MemDMA1 Stream 1 Destination	Current	Descriptor Pointer Register */
#define	MDMA1_D1_CURR_ADDR		0xFFC01FA4	/* MemDMA1 Stream 1 Destination	Current	Address	Register */
#define	MDMA1_D1_IRQ_STATUS		0xFFC01FA8	/* MemDMA1 Stream 1 Destination	Interrupt/Status Register */
#define	MDMA1_D1_PERIPHERAL_MAP	0xFFC01FAC	/* MemDMA1 Stream 1 Destination	Peripheral Map Register */
#define	MDMA1_D1_CURR_X_COUNT	0xFFC01FB0	/* MemDMA1 Stream 1 Destination	Current	X Count	Register */
#define	MDMA1_D1_CURR_Y_COUNT	0xFFC01FB8	/* MemDMA1 Stream 1 Destination	Current	Y Count	Register */

#define	MDMA1_S1_NEXT_DESC_PTR	0xFFC01FC0	/* MemDMA1 Stream 1 Source Next	Descriptor Pointer Register */
#define	MDMA1_S1_START_ADDR		0xFFC01FC4	/* MemDMA1 Stream 1 Source Start Address Register */
#define	MDMA1_S1_CONFIG			0xFFC01FC8	/* MemDMA1 Stream 1 Source Configuration Register */
#define	MDMA1_S1_X_COUNT		0xFFC01FD0	/* MemDMA1 Stream 1 Source X Count Register */
#define	MDMA1_S1_X_MODIFY		0xFFC01FD4	/* MemDMA1 Stream 1 Source X Modify Register */
#define	MDMA1_S1_Y_COUNT		0xFFC01FD8	/* MemDMA1 Stream 1 Source Y Count Register */
#define	MDMA1_S1_Y_MODIFY		0xFFC01FDC	/* MemDMA1 Stream 1 Source Y Modify Register */
#define	MDMA1_S1_CURR_DESC_PTR	0xFFC01FE0	/* MemDMA1 Stream 1 Source Current Descriptor Pointer Register */
#define	MDMA1_S1_CURR_ADDR		0xFFC01FE4	/* MemDMA1 Stream 1 Source Current Address Register */
#define	MDMA1_S1_IRQ_STATUS		0xFFC01FE8	/* MemDMA1 Stream 1 Source Interrupt/Status Register */
#define	MDMA1_S1_PERIPHERAL_MAP	0xFFC01FEC	/* MemDMA1 Stream 1 Source Peripheral Map Register */
#define	MDMA1_S1_CURR_X_COUNT	0xFFC01FF0	/* MemDMA1 Stream 1 Source Current X Count Register */
#define	MDMA1_S1_CURR_Y_COUNT	0xFFC01FF8	/* MemDMA1 Stream 1 Source Current Y Count Register */


/* UART1 Controller		(0xFFC02000 - 0xFFC020FF)	 */
#define	UART1_THR			0xFFC02000	/* Transmit Holding register */
#define	UART1_RBR			0xFFC02000	/* Receive Buffer register */
#define	UART1_DLL			0xFFC02000	/* Divisor Latch (Low-Byte) */
#define	UART1_IER			0xFFC02004	/* Interrupt Enable Register */
#define	UART1_DLH			0xFFC02004	/* Divisor Latch (High-Byte) */
#define	UART1_IIR			0xFFC02008	/* Interrupt Identification Register */
#define	UART1_LCR			0xFFC0200C	/* Line	Control	Register */
#define	UART1_MCR			0xFFC02010	/* Modem Control Register */
#define	UART1_LSR			0xFFC02014	/* Line	Status Register */
#define	UART1_SCR			0xFFC0201C	/* SCR Scratch Register */
#define	UART1_GCTL			0xFFC02024	/* Global Control Register */


/* UART2 Controller		(0xFFC02100 - 0xFFC021FF)	 */
#define	UART2_THR			0xFFC02100	/* Transmit Holding register */
#define	UART2_RBR			0xFFC02100	/* Receive Buffer register */
#define	UART2_DLL			0xFFC02100	/* Divisor Latch (Low-Byte) */
#define	UART2_IER			0xFFC02104	/* Interrupt Enable Register */
#define	UART2_DLH			0xFFC02104	/* Divisor Latch (High-Byte) */
#define	UART2_IIR			0xFFC02108	/* Interrupt Identification Register */
#define	UART2_LCR			0xFFC0210C	/* Line	Control	Register */
#define	UART2_MCR			0xFFC02110	/* Modem Control Register */
#define	UART2_LSR			0xFFC02114	/* Line	Status Register */
#define	UART2_SCR			0xFFC0211C	/* SCR Scratch Register */
#define	UART2_GCTL			0xFFC02124	/* Global Control Register */


/* Two-Wire Interface 1	(0xFFC02200 - 0xFFC022FF)			 */
#define	TWI1_CLKDIV			0xFFC02200	/* Serial Clock	Divider	Register */
#define	TWI1_CONTROL		0xFFC02204	/* TWI1	Master Internal	Time Reference Register */
#define	TWI1_SLAVE_CTRL		0xFFC02208	/* Slave Mode Control Register */
#define	TWI1_SLAVE_STAT		0xFFC0220C	/* Slave Mode Status Register */
#define	TWI1_SLAVE_ADDR		0xFFC02210	/* Slave Mode Address Register */
#define	TWI1_MASTER_CTRL	0xFFC02214	/* Master Mode Control Register */
#define	TWI1_MASTER_STAT	0xFFC02218	/* Master Mode Status Register */
#define	TWI1_MASTER_ADDR	0xFFC0221C	/* Master Mode Address Register */
#define	TWI1_INT_STAT		0xFFC02220	/* TWI1	Master Interrupt Register */
#define	TWI1_INT_MASK		0xFFC02224	/* TWI1	Master Interrupt Mask Register */
#define	TWI1_FIFO_CTRL		0xFFC02228	/* FIFO	Control	Register */
#define	TWI1_FIFO_STAT		0xFFC0222C	/* FIFO	Status Register */
#define	TWI1_XMT_DATA8		0xFFC02280	/* FIFO	Transmit Data Single Byte Register */
#define	TWI1_XMT_DATA16		0xFFC02284	/* FIFO	Transmit Data Double Byte Register */
#define	TWI1_RCV_DATA8		0xFFC02288	/* FIFO	Receive	Data Single Byte Register */
#define	TWI1_RCV_DATA16		0xFFC0228C	/* FIFO	Receive	Data Double Byte Register */
#define TWI1_REGBASE		TWI1_CLKDIV


/* the following are for backwards compatibility */
#define	TWI1_PRESCALE	  TWI1_CONTROL
#define	TWI1_INT_SRC	  TWI1_INT_STAT
#define	TWI1_INT_ENABLE	  TWI1_INT_MASK


/* SPI1	Controller		(0xFFC02300 - 0xFFC023FF)	 */
#define	SPI1_CTL			0xFFC02300  /* SPI1 Control Register */
#define	SPI1_FLG			0xFFC02304  /* SPI1 Flag register */
#define	SPI1_STAT			0xFFC02308  /* SPI1 Status register */
#define	SPI1_TDBR			0xFFC0230C  /* SPI1 Transmit Data Buffer Register */
#define	SPI1_RDBR			0xFFC02310  /* SPI1 Receive Data Buffer	Register */
#define	SPI1_BAUD			0xFFC02314  /* SPI1 Baud rate Register */
#define	SPI1_SHADOW			0xFFC02318  /* SPI1_RDBR Shadow	Register */
#define SPI1_REGBASE			SPI1_CTL

/* SPI2	Controller		(0xFFC02400 - 0xFFC024FF)	 */
#define	SPI2_CTL			0xFFC02400  /* SPI2 Control Register */
#define	SPI2_FLG			0xFFC02404  /* SPI2 Flag register */
#define	SPI2_STAT			0xFFC02408  /* SPI2 Status register */
#define	SPI2_TDBR			0xFFC0240C  /* SPI2 Transmit Data Buffer Register */
#define	SPI2_RDBR			0xFFC02410  /* SPI2 Receive Data Buffer	Register */
#define	SPI2_BAUD			0xFFC02414  /* SPI2 Baud rate Register */
#define	SPI2_SHADOW			0xFFC02418  /* SPI2_RDBR Shadow	Register */
#define SPI2_REGBASE			SPI2_CTL

/* SPORT2 Controller		(0xFFC02500 - 0xFFC025FF)			 */
#define	SPORT2_TCR1			0xFFC02500	/* SPORT2 Transmit Configuration 1 Register */
#define	SPORT2_TCR2			0xFFC02504	/* SPORT2 Transmit Configuration 2 Register */
#define	SPORT2_TCLKDIV		0xFFC02508	/* SPORT2 Transmit Clock Divider */
#define	SPORT2_TFSDIV		0xFFC0250C	/* SPORT2 Transmit Frame Sync Divider */
#define	SPORT2_TX			0xFFC02510	/* SPORT2 TX Data Register */
#define	SPORT2_RX			0xFFC02518	/* SPORT2 RX Data Register */
#define	SPORT2_RCR1			0xFFC02520	/* SPORT2 Transmit Configuration 1 Register */
#define	SPORT2_RCR2			0xFFC02524	/* SPORT2 Transmit Configuration 2 Register */
#define	SPORT2_RCLKDIV		0xFFC02528	/* SPORT2 Receive Clock	Divider */
#define	SPORT2_RFSDIV		0xFFC0252C	/* SPORT2 Receive Frame	Sync Divider */
#define	SPORT2_STAT			0xFFC02530	/* SPORT2 Status Register */
#define	SPORT2_CHNL			0xFFC02534	/* SPORT2 Current Channel Register */
#define	SPORT2_MCMC1		0xFFC02538	/* SPORT2 Multi-Channel	Configuration Register 1 */
#define	SPORT2_MCMC2		0xFFC0253C	/* SPORT2 Multi-Channel	Configuration Register 2 */
#define	SPORT2_MTCS0		0xFFC02540	/* SPORT2 Multi-Channel	Transmit Select	Register 0 */
#define	SPORT2_MTCS1		0xFFC02544	/* SPORT2 Multi-Channel	Transmit Select	Register 1 */
#define	SPORT2_MTCS2		0xFFC02548	/* SPORT2 Multi-Channel	Transmit Select	Register 2 */
#define	SPORT2_MTCS3		0xFFC0254C	/* SPORT2 Multi-Channel	Transmit Select	Register 3 */
#define	SPORT2_MRCS0		0xFFC02550	/* SPORT2 Multi-Channel	Receive	Select Register	0 */
#define	SPORT2_MRCS1		0xFFC02554	/* SPORT2 Multi-Channel	Receive	Select Register	1 */
#define	SPORT2_MRCS2		0xFFC02558	/* SPORT2 Multi-Channel	Receive	Select Register	2 */
#define	SPORT2_MRCS3		0xFFC0255C	/* SPORT2 Multi-Channel	Receive	Select Register	3 */


/* SPORT3 Controller		(0xFFC02600 - 0xFFC026FF)			 */
#define	SPORT3_TCR1			0xFFC02600	/* SPORT3 Transmit Configuration 1 Register */
#define	SPORT3_TCR2			0xFFC02604	/* SPORT3 Transmit Configuration 2 Register */
#define	SPORT3_TCLKDIV		0xFFC02608	/* SPORT3 Transmit Clock Divider */
#define	SPORT3_TFSDIV		0xFFC0260C	/* SPORT3 Transmit Frame Sync Divider */
#define	SPORT3_TX			0xFFC02610	/* SPORT3 TX Data Register */
#define	SPORT3_RX			0xFFC02618	/* SPORT3 RX Data Register */
#define	SPORT3_RCR1			0xFFC02620	/* SPORT3 Transmit Configuration 1 Register */
#define	SPORT3_RCR2			0xFFC02624	/* SPORT3 Transmit Configuration 2 Register */
#define	SPORT3_RCLKDIV		0xFFC02628	/* SPORT3 Receive Clock	Divider */
#define	SPORT3_RFSDIV		0xFFC0262C	/* SPORT3 Receive Frame	Sync Divider */
#define	SPORT3_STAT			0xFFC02630	/* SPORT3 Status Register */
#define	SPORT3_CHNL			0xFFC02634	/* SPORT3 Current Channel Register */
#define	SPORT3_MCMC1		0xFFC02638	/* SPORT3 Multi-Channel	Configuration Register 1 */
#define	SPORT3_MCMC2		0xFFC0263C	/* SPORT3 Multi-Channel	Configuration Register 2 */
#define	SPORT3_MTCS0		0xFFC02640	/* SPORT3 Multi-Channel	Transmit Select	Register 0 */
#define	SPORT3_MTCS1		0xFFC02644	/* SPORT3 Multi-Channel	Transmit Select	Register 1 */
#define	SPORT3_MTCS2		0xFFC02648	/* SPORT3 Multi-Channel	Transmit Select	Register 2 */
#define	SPORT3_MTCS3		0xFFC0264C	/* SPORT3 Multi-Channel	Transmit Select	Register 3 */
#define	SPORT3_MRCS0		0xFFC02650	/* SPORT3 Multi-Channel	Receive	Select Register	0 */
#define	SPORT3_MRCS1		0xFFC02654	/* SPORT3 Multi-Channel	Receive	Select Register	1 */
#define	SPORT3_MRCS2		0xFFC02658	/* SPORT3 Multi-Channel	Receive	Select Register	2 */
#define	SPORT3_MRCS3		0xFFC0265C	/* SPORT3 Multi-Channel	Receive	Select Register	3 */


/* Media Transceiver (MXVR)   (0xFFC02700 - 0xFFC028FF) */

#define	MXVR_CONFIG	      0xFFC02700  /* MXVR Configuration	Register */
#define	MXVR_PLL_CTL_0	      0xFFC02704  /* MXVR Phase	Lock Loop Control Register 0 */

#define	MXVR_STATE_0	      0xFFC02708  /* MXVR State	Register 0 */
#define	MXVR_STATE_1	      0xFFC0270C  /* MXVR State	Register 1 */

#define	MXVR_INT_STAT_0	      0xFFC02710  /* MXVR Interrupt Status Register 0 */
#define	MXVR_INT_STAT_1	      0xFFC02714  /* MXVR Interrupt Status Register 1 */

#define	MXVR_INT_EN_0	      0xFFC02718  /* MXVR Interrupt Enable Register 0 */
#define	MXVR_INT_EN_1	      0xFFC0271C  /* MXVR Interrupt Enable Register 1 */

#define	MXVR_POSITION	      0xFFC02720  /* MXVR Node Position	Register */
#define	MXVR_MAX_POSITION     0xFFC02724  /* MXVR Maximum Node Position	Register */

#define	MXVR_DELAY	      0xFFC02728  /* MXVR Node Frame Delay Register */
#define	MXVR_MAX_DELAY	      0xFFC0272C  /* MXVR Maximum Node Frame Delay Register */

#define	MXVR_LADDR	      0xFFC02730  /* MXVR Logical Address Register */
#define	MXVR_GADDR	      0xFFC02734  /* MXVR Group	Address	Register */
#define	MXVR_AADDR	      0xFFC02738  /* MXVR Alternate Address Register */

#define	MXVR_ALLOC_0	      0xFFC0273C  /* MXVR Allocation Table Register 0 */
#define	MXVR_ALLOC_1	      0xFFC02740  /* MXVR Allocation Table Register 1 */
#define	MXVR_ALLOC_2	      0xFFC02744  /* MXVR Allocation Table Register 2 */
#define	MXVR_ALLOC_3	      0xFFC02748  /* MXVR Allocation Table Register 3 */
#define	MXVR_ALLOC_4	      0xFFC0274C  /* MXVR Allocation Table Register 4 */
#define	MXVR_ALLOC_5	      0xFFC02750  /* MXVR Allocation Table Register 5 */
#define	MXVR_ALLOC_6	      0xFFC02754  /* MXVR Allocation Table Register 6 */
#define	MXVR_ALLOC_7	      0xFFC02758  /* MXVR Allocation Table Register 7 */
#define	MXVR_ALLOC_8	      0xFFC0275C  /* MXVR Allocation Table Register 8 */
#define	MXVR_ALLOC_9	      0xFFC02760  /* MXVR Allocation Table Register 9 */
#define	MXVR_ALLOC_10	      0xFFC02764  /* MXVR Allocation Table Register 10 */
#define	MXVR_ALLOC_11	      0xFFC02768  /* MXVR Allocation Table Register 11 */
#define	MXVR_ALLOC_12	      0xFFC0276C  /* MXVR Allocation Table Register 12 */
#define	MXVR_ALLOC_13	      0xFFC02770  /* MXVR Allocation Table Register 13 */
#define	MXVR_ALLOC_14	      0xFFC02774  /* MXVR Allocation Table Register 14 */

#define	MXVR_SYNC_LCHAN_0     0xFFC02778  /* MXVR Sync Data Logical Channel Assign Register 0 */
#define	MXVR_SYNC_LCHAN_1     0xFFC0277C  /* MXVR Sync Data Logical Channel Assign Register 1 */
#define	MXVR_SYNC_LCHAN_2     0xFFC02780  /* MXVR Sync Data Logical Channel Assign Register 2 */
#define	MXVR_SYNC_LCHAN_3     0xFFC02784  /* MXVR Sync Data Logical Channel Assign Register 3 */
#define	MXVR_SYNC_LCHAN_4     0xFFC02788  /* MXVR Sync Data Logical Channel Assign Register 4 */
#define	MXVR_SYNC_LCHAN_5     0xFFC0278C  /* MXVR Sync Data Logical Channel Assign Register 5 */
#define	MXVR_SYNC_LCHAN_6     0xFFC02790  /* MXVR Sync Data Logical Channel Assign Register 6 */
#define	MXVR_SYNC_LCHAN_7     0xFFC02794  /* MXVR Sync Data Logical Channel Assign Register 7 */

#define	MXVR_DMA0_CONFIG      0xFFC02798  /* MXVR Sync Data DMA0 Config	Register */
#define	MXVR_DMA0_START_ADDR  0xFFC0279C  /* MXVR Sync Data DMA0 Start Address Register */
#define	MXVR_DMA0_COUNT	      0xFFC027A0  /* MXVR Sync Data DMA0 Loop Count Register */
#define	MXVR_DMA0_CURR_ADDR   0xFFC027A4  /* MXVR Sync Data DMA0 Current Address Register */
#define	MXVR_DMA0_CURR_COUNT  0xFFC027A8  /* MXVR Sync Data DMA0 Current Loop Count Register */

#define	MXVR_DMA1_CONFIG      0xFFC027AC  /* MXVR Sync Data DMA1 Config	Register */
#define	MXVR_DMA1_START_ADDR  0xFFC027B0  /* MXVR Sync Data DMA1 Start Address Register */
#define	MXVR_DMA1_COUNT	      0xFFC027B4  /* MXVR Sync Data DMA1 Loop Count Register */
#define	MXVR_DMA1_CURR_ADDR   0xFFC027B8  /* MXVR Sync Data DMA1 Current Address Register */
#define	MXVR_DMA1_CURR_COUNT  0xFFC027BC  /* MXVR Sync Data DMA1 Current Loop Count Register */

#define	MXVR_DMA2_CONFIG      0xFFC027C0  /* MXVR Sync Data DMA2 Config	Register */
#define	MXVR_DMA2_START_ADDR  0xFFC027C4  /* MXVR Sync Data DMA2 Start Address Register */
#define	MXVR_DMA2_COUNT	      0xFFC027C8  /* MXVR Sync Data DMA2 Loop Count Register */
#define	MXVR_DMA2_CURR_ADDR   0xFFC027CC  /* MXVR Sync Data DMA2 Current Address Register */
#define	MXVR_DMA2_CURR_COUNT  0xFFC027D0  /* MXVR Sync Data DMA2 Current Loop Count Register */

#define	MXVR_DMA3_CONFIG      0xFFC027D4  /* MXVR Sync Data DMA3 Config	Register */
#define	MXVR_DMA3_START_ADDR  0xFFC027D8  /* MXVR Sync Data DMA3 Start Address Register */
#define	MXVR_DMA3_COUNT	      0xFFC027DC  /* MXVR Sync Data DMA3 Loop Count Register */
#define	MXVR_DMA3_CURR_ADDR   0xFFC027E0  /* MXVR Sync Data DMA3 Current Address Register */
#define	MXVR_DMA3_CURR_COUNT  0xFFC027E4  /* MXVR Sync Data DMA3 Current Loop Count Register */

#define	MXVR_DMA4_CONFIG      0xFFC027E8  /* MXVR Sync Data DMA4 Config	Register */
#define	MXVR_DMA4_START_ADDR  0xFFC027EC  /* MXVR Sync Data DMA4 Start Address Register */
#define	MXVR_DMA4_COUNT	      0xFFC027F0  /* MXVR Sync Data DMA4 Loop Count Register */
#define	MXVR_DMA4_CURR_ADDR   0xFFC027F4  /* MXVR Sync Data DMA4 Current Address Register */
#define	MXVR_DMA4_CURR_COUNT  0xFFC027F8  /* MXVR Sync Data DMA4 Current Loop Count Register */

#define	MXVR_DMA5_CONFIG      0xFFC027FC  /* MXVR Sync Data DMA5 Config	Register */
#define	MXVR_DMA5_START_ADDR  0xFFC02800  /* MXVR Sync Data DMA5 Start Address Register */
#define	MXVR_DMA5_COUNT	      0xFFC02804  /* MXVR Sync Data DMA5 Loop Count Register */
#define	MXVR_DMA5_CURR_ADDR   0xFFC02808  /* MXVR Sync Data DMA5 Current Address Register */
#define	MXVR_DMA5_CURR_COUNT  0xFFC0280C  /* MXVR Sync Data DMA5 Current Loop Count Register */

#define	MXVR_DMA6_CONFIG      0xFFC02810  /* MXVR Sync Data DMA6 Config	Register */
#define	MXVR_DMA6_START_ADDR  0xFFC02814  /* MXVR Sync Data DMA6 Start Address Register */
#define	MXVR_DMA6_COUNT	      0xFFC02818  /* MXVR Sync Data DMA6 Loop Count Register */
#define	MXVR_DMA6_CURR_ADDR   0xFFC0281C  /* MXVR Sync Data DMA6 Current Address Register */
#define	MXVR_DMA6_CURR_COUNT  0xFFC02820  /* MXVR Sync Data DMA6 Current Loop Count Register */

#define	MXVR_DMA7_CONFIG      0xFFC02824  /* MXVR Sync Data DMA7 Config	Register */
#define	MXVR_DMA7_START_ADDR  0xFFC02828  /* MXVR Sync Data DMA7 Start Address Register */
#define	MXVR_DMA7_COUNT	      0xFFC0282C  /* MXVR Sync Data DMA7 Loop Count Register */
#define	MXVR_DMA7_CURR_ADDR   0xFFC02830  /* MXVR Sync Data DMA7 Current Address Register */
#define	MXVR_DMA7_CURR_COUNT  0xFFC02834  /* MXVR Sync Data DMA7 Current Loop Count Register */

#define	MXVR_AP_CTL	      0xFFC02838  /* MXVR Async	Packet Control Register */
#define	MXVR_APRB_START_ADDR  0xFFC0283C  /* MXVR Async	Packet RX Buffer Start Addr Register */
#define	MXVR_APRB_CURR_ADDR   0xFFC02840  /* MXVR Async	Packet RX Buffer Current Addr Register */
#define	MXVR_APTB_START_ADDR  0xFFC02844  /* MXVR Async	Packet TX Buffer Start Addr Register */
#define	MXVR_APTB_CURR_ADDR   0xFFC02848  /* MXVR Async	Packet TX Buffer Current Addr Register */

#define	MXVR_CM_CTL	      0xFFC0284C  /* MXVR Control Message Control Register */
#define	MXVR_CMRB_START_ADDR  0xFFC02850  /* MXVR Control Message RX Buffer Start Addr Register */
#define	MXVR_CMRB_CURR_ADDR   0xFFC02854  /* MXVR Control Message RX Buffer Current Address */
#define	MXVR_CMTB_START_ADDR  0xFFC02858  /* MXVR Control Message TX Buffer Start Addr Register */
#define	MXVR_CMTB_CURR_ADDR   0xFFC0285C  /* MXVR Control Message TX Buffer Current Address */

#define	MXVR_RRDB_START_ADDR  0xFFC02860  /* MXVR Remote Read Buffer Start Addr	Register */
#define	MXVR_RRDB_CURR_ADDR   0xFFC02864  /* MXVR Remote Read Buffer Current Addr Register */

#define	MXVR_PAT_DATA_0	      0xFFC02868  /* MXVR Pattern Data Register	0 */
#define	MXVR_PAT_EN_0	      0xFFC0286C  /* MXVR Pattern Enable Register 0 */
#define	MXVR_PAT_DATA_1	      0xFFC02870  /* MXVR Pattern Data Register	1 */
#define	MXVR_PAT_EN_1	      0xFFC02874  /* MXVR Pattern Enable Register 1 */

#define	MXVR_FRAME_CNT_0      0xFFC02878  /* MXVR Frame	Counter	0 */
#define	MXVR_FRAME_CNT_1      0xFFC0287C  /* MXVR Frame	Counter	1 */

#define	MXVR_ROUTING_0	      0xFFC02880  /* MXVR Routing Table	Register 0 */
#define	MXVR_ROUTING_1	      0xFFC02884  /* MXVR Routing Table	Register 1 */
#define	MXVR_ROUTING_2	      0xFFC02888  /* MXVR Routing Table	Register 2 */
#define	MXVR_ROUTING_3	      0xFFC0288C  /* MXVR Routing Table	Register 3 */
#define	MXVR_ROUTING_4	      0xFFC02890  /* MXVR Routing Table	Register 4 */
#define	MXVR_ROUTING_5	      0xFFC02894  /* MXVR Routing Table	Register 5 */
#define	MXVR_ROUTING_6	      0xFFC02898  /* MXVR Routing Table	Register 6 */
#define	MXVR_ROUTING_7	      0xFFC0289C  /* MXVR Routing Table	Register 7 */
#define	MXVR_ROUTING_8	      0xFFC028A0  /* MXVR Routing Table	Register 8 */
#define	MXVR_ROUTING_9	      0xFFC028A4  /* MXVR Routing Table	Register 9 */
#define	MXVR_ROUTING_10	      0xFFC028A8  /* MXVR Routing Table	Register 10 */
#define	MXVR_ROUTING_11	      0xFFC028AC  /* MXVR Routing Table	Register 11 */
#define	MXVR_ROUTING_12	      0xFFC028B0  /* MXVR Routing Table	Register 12 */
#define	MXVR_ROUTING_13	      0xFFC028B4  /* MXVR Routing Table	Register 13 */
#define	MXVR_ROUTING_14	      0xFFC028B8  /* MXVR Routing Table	Register 14 */

#define	MXVR_PLL_CTL_1	      0xFFC028BC  /* MXVR Phase	Lock Loop Control Register 1 */
#define	MXVR_BLOCK_CNT	      0xFFC028C0  /* MXVR Block	Counter */
#define	MXVR_PLL_CTL_2	      0xFFC028C4  /* MXVR Phase	Lock Loop Control Register 2 */


/* CAN Controller		(0xFFC02A00 - 0xFFC02FFF)				 */
/* For Mailboxes 0-15											 */
#define	CAN_MC1				0xFFC02A00	/* Mailbox config reg 1	 */
#define	CAN_MD1				0xFFC02A04	/* Mailbox direction reg 1 */
#define	CAN_TRS1			0xFFC02A08	/* Transmit Request Set	reg 1 */
#define	CAN_TRR1			0xFFC02A0C	/* Transmit Request Reset reg 1 */
#define	CAN_TA1				0xFFC02A10	/* Transmit Acknowledge	reg 1 */
#define	CAN_AA1				0xFFC02A14	/* Transmit Abort Acknowledge reg 1 */
#define	CAN_RMP1			0xFFC02A18	/* Receive Message Pending reg 1 */
#define	CAN_RML1			0xFFC02A1C	/* Receive Message Lost	reg 1 */
#define	CAN_MBTIF1			0xFFC02A20	/* Mailbox Transmit Interrupt Flag reg 1 */
#define	CAN_MBRIF1			0xFFC02A24	/* Mailbox Receive  Interrupt Flag reg 1 */
#define	CAN_MBIM1			0xFFC02A28	/* Mailbox Interrupt Mask reg 1 */
#define	CAN_RFH1			0xFFC02A2C	/* Remote Frame	Handling reg 1 */
#define	CAN_OPSS1			0xFFC02A30	/* Overwrite Protection	Single Shot Xmission reg 1 */

/* For Mailboxes 16-31											 */
#define	CAN_MC2				0xFFC02A40	/* Mailbox config reg 2	 */
#define	CAN_MD2				0xFFC02A44	/* Mailbox direction reg 2 */
#define	CAN_TRS2			0xFFC02A48	/* Transmit Request Set	reg 2 */
#define	CAN_TRR2			0xFFC02A4C	/* Transmit Request Reset reg 2 */
#define	CAN_TA2				0xFFC02A50	/* Transmit Acknowledge	reg 2 */
#define	CAN_AA2				0xFFC02A54	/* Transmit Abort Acknowledge reg 2 */
#define	CAN_RMP2			0xFFC02A58	/* Receive Message Pending reg 2 */
#define	CAN_RML2			0xFFC02A5C	/* Receive Message Lost	reg 2 */
#define	CAN_MBTIF2			0xFFC02A60	/* Mailbox Transmit Interrupt Flag reg 2 */
#define	CAN_MBRIF2			0xFFC02A64	/* Mailbox Receive  Interrupt Flag reg 2 */
#define	CAN_MBIM2			0xFFC02A68	/* Mailbox Interrupt Mask reg 2 */
#define	CAN_RFH2			0xFFC02A6C	/* Remote Frame	Handling reg 2 */
#define	CAN_OPSS2			0xFFC02A70	/* Overwrite Protection	Single Shot Xmission reg 2 */

#define	CAN_CLOCK			0xFFC02A80	/* Bit Timing Configuration register 0 */
#define	CAN_TIMING			0xFFC02A84	/* Bit Timing Configuration register 1 */

#define	CAN_DEBUG			0xFFC02A88	/* Debug Register		 */
/* the following is for	backwards compatibility */
#define	CAN_CNF		 CAN_DEBUG

#define	CAN_STATUS			0xFFC02A8C	/* Global Status Register */
#define	CAN_CEC				0xFFC02A90	/* Error Counter Register */
#define	CAN_GIS				0xFFC02A94	/* Global Interrupt Status Register */
#define	CAN_GIM				0xFFC02A98	/* Global Interrupt Mask Register */
#define	CAN_GIF				0xFFC02A9C	/* Global Interrupt Flag Register */
#define	CAN_CONTROL			0xFFC02AA0	/* Master Control Register */
#define	CAN_INTR			0xFFC02AA4	/* Interrupt Pending Register */
#define	CAN_MBTD			0xFFC02AAC	/* Mailbox Temporary Disable Feature */
#define	CAN_EWR				0xFFC02AB0	/* Programmable	Warning	Level */
#define	CAN_ESR				0xFFC02AB4	/* Error Status	Register */
#define	CAN_UCCNT			0xFFC02AC4	/* Universal Counter	 */
#define	CAN_UCRC			0xFFC02AC8	/* Universal Counter Reload/Capture Register */
#define	CAN_UCCNF			0xFFC02ACC	/* Universal Counter Configuration Register */

/* Mailbox Acceptance Masks					 */
#define	CAN_AM00L			0xFFC02B00	/* Mailbox 0 Low Acceptance Mask */
#define	CAN_AM00H			0xFFC02B04	/* Mailbox 0 High Acceptance Mask */
#define	CAN_AM01L			0xFFC02B08	/* Mailbox 1 Low Acceptance Mask */
#define	CAN_AM01H			0xFFC02B0C	/* Mailbox 1 High Acceptance Mask */
#define	CAN_AM02L			0xFFC02B10	/* Mailbox 2 Low Acceptance Mask */
#define	CAN_AM02H			0xFFC02B14	/* Mailbox 2 High Acceptance Mask */
#define	CAN_AM03L			0xFFC02B18	/* Mailbox 3 Low Acceptance Mask */
#define	CAN_AM03H			0xFFC02B1C	/* Mailbox 3 High Acceptance Mask */
#define	CAN_AM04L			0xFFC02B20	/* Mailbox 4 Low Acceptance Mask */
#define	CAN_AM04H			0xFFC02B24	/* Mailbox 4 High Acceptance Mask */
#define	CAN_AM05L			0xFFC02B28	/* Mailbox 5 Low Acceptance Mask */
#define	CAN_AM05H			0xFFC02B2C	/* Mailbox 5 High Acceptance Mask */
#define	CAN_AM06L			0xFFC02B30	/* Mailbox 6 Low Acceptance Mask */
#define	CAN_AM06H			0xFFC02B34	/* Mailbox 6 High Acceptance Mask */
#define	CAN_AM07L			0xFFC02B38	/* Mailbox 7 Low Acceptance Mask */
#define	CAN_AM07H			0xFFC02B3C	/* Mailbox 7 High Acceptance Mask */
#define	CAN_AM08L			0xFFC02B40	/* Mailbox 8 Low Acceptance Mask */
#define	CAN_AM08H			0xFFC02B44	/* Mailbox 8 High Acceptance Mask */
#define	CAN_AM09L			0xFFC02B48	/* Mailbox 9 Low Acceptance Mask */
#define	CAN_AM09H			0xFFC02B4C	/* Mailbox 9 High Acceptance Mask */
#define	CAN_AM10L			0xFFC02B50	/* Mailbox 10 Low Acceptance Mask */
#define	CAN_AM10H			0xFFC02B54	/* Mailbox 10 High Acceptance Mask */
#define	CAN_AM11L			0xFFC02B58	/* Mailbox 11 Low Acceptance Mask */
#define	CAN_AM11H			0xFFC02B5C	/* Mailbox 11 High Acceptance Mask */
#define	CAN_AM12L			0xFFC02B60	/* Mailbox 12 Low Acceptance Mask */
#define	CAN_AM12H			0xFFC02B64	/* Mailbox 12 High Acceptance Mask */
#define	CAN_AM13L			0xFFC02B68	/* Mailbox 13 Low Acceptance Mask */
#define	CAN_AM13H			0xFFC02B6C	/* Mailbox 13 High Acceptance Mask */
#define	CAN_AM14L			0xFFC02B70	/* Mailbox 14 Low Acceptance Mask */
#define	CAN_AM14H			0xFFC02B74	/* Mailbox 14 High Acceptance Mask */
#define	CAN_AM15L			0xFFC02B78	/* Mailbox 15 Low Acceptance Mask */
#define	CAN_AM15H			0xFFC02B7C	/* Mailbox 15 High Acceptance Mask */

#define	CAN_AM16L			0xFFC02B80	/* Mailbox 16 Low Acceptance Mask */
#define	CAN_AM16H			0xFFC02B84	/* Mailbox 16 High Acceptance Mask */
#define	CAN_AM17L			0xFFC02B88	/* Mailbox 17 Low Acceptance Mask */
#define	CAN_AM17H			0xFFC02B8C	/* Mailbox 17 High Acceptance Mask */
#define	CAN_AM18L			0xFFC02B90	/* Mailbox 18 Low Acceptance Mask */
#define	CAN_AM18H			0xFFC02B94	/* Mailbox 18 High Acceptance Mask */
#define	CAN_AM19L			0xFFC02B98	/* Mailbox 19 Low Acceptance Mask */
#define	CAN_AM19H			0xFFC02B9C	/* Mailbox 19 High Acceptance Mask */
#define	CAN_AM20L			0xFFC02BA0	/* Mailbox 20 Low Acceptance Mask */
#define	CAN_AM20H			0xFFC02BA4	/* Mailbox 20 High Acceptance Mask */
#define	CAN_AM21L			0xFFC02BA8	/* Mailbox 21 Low Acceptance Mask */
#define	CAN_AM21H			0xFFC02BAC	/* Mailbox 21 High Acceptance Mask */
#define	CAN_AM22L			0xFFC02BB0	/* Mailbox 22 Low Acceptance Mask */
#define	CAN_AM22H			0xFFC02BB4	/* Mailbox 22 High Acceptance Mask */
#define	CAN_AM23L			0xFFC02BB8	/* Mailbox 23 Low Acceptance Mask */
#define	CAN_AM23H			0xFFC02BBC	/* Mailbox 23 High Acceptance Mask */
#define	CAN_AM24L			0xFFC02BC0	/* Mailbox 24 Low Acceptance Mask */
#define	CAN_AM24H			0xFFC02BC4	/* Mailbox 24 High Acceptance Mask */
#define	CAN_AM25L			0xFFC02BC8	/* Mailbox 25 Low Acceptance Mask */
#define	CAN_AM25H			0xFFC02BCC	/* Mailbox 25 High Acceptance Mask */
#define	CAN_AM26L			0xFFC02BD0	/* Mailbox 26 Low Acceptance Mask */
#define	CAN_AM26H			0xFFC02BD4	/* Mailbox 26 High Acceptance Mask */
#define	CAN_AM27L			0xFFC02BD8	/* Mailbox 27 Low Acceptance Mask */
#define	CAN_AM27H			0xFFC02BDC	/* Mailbox 27 High Acceptance Mask */
#define	CAN_AM28L			0xFFC02BE0	/* Mailbox 28 Low Acceptance Mask */
#define	CAN_AM28H			0xFFC02BE4	/* Mailbox 28 High Acceptance Mask */
#define	CAN_AM29L			0xFFC02BE8	/* Mailbox 29 Low Acceptance Mask */
#define	CAN_AM29H			0xFFC02BEC	/* Mailbox 29 High Acceptance Mask */
#define	CAN_AM30L			0xFFC02BF0	/* Mailbox 30 Low Acceptance Mask */
#define	CAN_AM30H			0xFFC02BF4	/* Mailbox 30 High Acceptance Mask */
#define	CAN_AM31L			0xFFC02BF8	/* Mailbox 31 Low Acceptance Mask */
#define	CAN_AM31H			0xFFC02BFC	/* Mailbox 31 High Acceptance Mask */

/* CAN Acceptance Mask Macros */
#define	CAN_AM_L(x)			(CAN_AM00L+((x)*0x8))
#define	CAN_AM_H(x)			(CAN_AM00H+((x)*0x8))

/* Mailbox Registers									 */
#define	CAN_MB00_DATA0		0xFFC02C00	/* Mailbox 0 Data Word 0 [15:0]	Register */
#define	CAN_MB00_DATA1		0xFFC02C04	/* Mailbox 0 Data Word 1 [31:16] Register */
#define	CAN_MB00_DATA2		0xFFC02C08	/* Mailbox 0 Data Word 2 [47:32] Register */
#define	CAN_MB00_DATA3		0xFFC02C0C	/* Mailbox 0 Data Word 3 [63:48] Register */
#define	CAN_MB00_LENGTH		0xFFC02C10	/* Mailbox 0 Data Length Code Register */
#define	CAN_MB00_TIMESTAMP	0xFFC02C14	/* Mailbox 0 Time Stamp	Value Register */
#define	CAN_MB00_ID0		0xFFC02C18	/* Mailbox 0 Identifier	Low Register */
#define	CAN_MB00_ID1		0xFFC02C1C	/* Mailbox 0 Identifier	High Register */

#define	CAN_MB01_DATA0		0xFFC02C20	/* Mailbox 1 Data Word 0 [15:0]	Register */
#define	CAN_MB01_DATA1		0xFFC02C24	/* Mailbox 1 Data Word 1 [31:16] Register */
#define	CAN_MB01_DATA2		0xFFC02C28	/* Mailbox 1 Data Word 2 [47:32] Register */
#define	CAN_MB01_DATA3		0xFFC02C2C	/* Mailbox 1 Data Word 3 [63:48] Register */
#define	CAN_MB01_LENGTH		0xFFC02C30	/* Mailbox 1 Data Length Code Register */
#define	CAN_MB01_TIMESTAMP	0xFFC02C34	/* Mailbox 1 Time Stamp	Value Register */
#define	CAN_MB01_ID0		0xFFC02C38	/* Mailbox 1 Identifier	Low Register */
#define	CAN_MB01_ID1		0xFFC02C3C	/* Mailbox 1 Identifier	High Register */

#define	CAN_MB02_DATA0		0xFFC02C40	/* Mailbox 2 Data Word 0 [15:0]	Register */
#define	CAN_MB02_DATA1		0xFFC02C44	/* Mailbox 2 Data Word 1 [31:16] Register */
#define	CAN_MB02_DATA2		0xFFC02C48	/* Mailbox 2 Data Word 2 [47:32] Register */
#define	CAN_MB02_DATA3		0xFFC02C4C	/* Mailbox 2 Data Word 3 [63:48] Register */
#define	CAN_MB02_LENGTH		0xFFC02C50	/* Mailbox 2 Data Length Code Register */
#define	CAN_MB02_TIMESTAMP	0xFFC02C54	/* Mailbox 2 Time Stamp	Value Register */
#define	CAN_MB02_ID0		0xFFC02C58	/* Mailbox 2 Identifier	Low Register */
#define	CAN_MB02_ID1		0xFFC02C5C	/* Mailbox 2 Identifier	High Register */

#define	CAN_MB03_DATA0		0xFFC02C60	/* Mailbox 3 Data Word 0 [15:0]	Register */
#define	CAN_MB03_DATA1		0xFFC02C64	/* Mailbox 3 Data Word 1 [31:16] Register */
#define	CAN_MB03_DATA2		0xFFC02C68	/* Mailbox 3 Data Word 2 [47:32] Register */
#define	CAN_MB03_DATA3		0xFFC02C6C	/* Mailbox 3 Data Word 3 [63:48] Register */
#define	CAN_MB03_LENGTH		0xFFC02C70	/* Mailbox 3 Data Length Code Register */
#define	CAN_MB03_TIMESTAMP	0xFFC02C74	/* Mailbox 3 Time Stamp	Value Register */
#define	CAN_MB03_ID0		0xFFC02C78	/* Mailbox 3 Identifier	Low Register */
#define	CAN_MB03_ID1		0xFFC02C7C	/* Mailbox 3 Identifier	High Register */

#define	CAN_MB04_DATA0		0xFFC02C80	/* Mailbox 4 Data Word 0 [15:0]	Register */
#define	CAN_MB04_DATA1		0xFFC02C84	/* Mailbox 4 Data Word 1 [31:16] Register */
#define	CAN_MB04_DATA2		0xFFC02C88	/* Mailbox 4 Data Word 2 [47:32] Register */
#define	CAN_MB04_DATA3		0xFFC02C8C	/* Mailbox 4 Data Word 3 [63:48] Register */
#define	CAN_MB04_LENGTH		0xFFC02C90	/* Mailbox 4 Data Length Code Register */
#define	CAN_MB04_TIMESTAMP	0xFFC02C94	/* Mailbox 4 Time Stamp	Value Register */
#define	CAN_MB04_ID0		0xFFC02C98	/* Mailbox 4 Identifier	Low Register */
#define	CAN_MB04_ID1		0xFFC02C9C	/* Mailbox 4 Identifier	High Register */

#define	CAN_MB05_DATA0		0xFFC02CA0	/* Mailbox 5 Data Word 0 [15:0]	Register */
#define	CAN_MB05_DATA1		0xFFC02CA4	/* Mailbox 5 Data Word 1 [31:16] Register */
#define	CAN_MB05_DATA2		0xFFC02CA8	/* Mailbox 5 Data Word 2 [47:32] Register */
#define	CAN_MB05_DATA3		0xFFC02CAC	/* Mailbox 5 Data Word 3 [63:48] Register */
#define	CAN_MB05_LENGTH		0xFFC02CB0	/* Mailbox 5 Data Length Code Register */
#define	CAN_MB05_TIMESTAMP	0xFFC02CB4	/* Mailbox 5 Time Stamp	Value Register */
#define	CAN_MB05_ID0		0xFFC02CB8	/* Mailbox 5 Identifier	Low Register */
#define	CAN_MB05_ID1		0xFFC02CBC	/* Mailbox 5 Identifier	High Register */

#define	CAN_MB06_DATA0		0xFFC02CC0	/* Mailbox 6 Data Word 0 [15:0]	Register */
#define	CAN_MB06_DATA1		0xFFC02CC4	/* Mailbox 6 Data Word 1 [31:16] Register */
#define	CAN_MB06_DATA2		0xFFC02CC8	/* Mailbox 6 Data Word 2 [47:32] Register */
#define	CAN_MB06_DATA3		0xFFC02CCC	/* Mailbox 6 Data Word 3 [63:48] Register */
#define	CAN_MB06_LENGTH		0xFFC02CD0	/* Mailbox 6 Data Length Code Register */
#define	CAN_MB06_TIMESTAMP	0xFFC02CD4	/* Mailbox 6 Time Stamp	Value Register */
#define	CAN_MB06_ID0		0xFFC02CD8	/* Mailbox 6 Identifier	Low Register */
#define	CAN_MB06_ID1		0xFFC02CDC	/* Mailbox 6 Identifier	High Register */

#define	CAN_MB07_DATA0		0xFFC02CE0	/* Mailbox 7 Data Word 0 [15:0]	Register */
#define	CAN_MB07_DATA1		0xFFC02CE4	/* Mailbox 7 Data Word 1 [31:16] Register */
#define	CAN_MB07_DATA2		0xFFC02CE8	/* Mailbox 7 Data Word 2 [47:32] Register */
#define	CAN_MB07_DATA3		0xFFC02CEC	/* Mailbox 7 Data Word 3 [63:48] Register */
#define	CAN_MB07_LENGTH		0xFFC02CF0	/* Mailbox 7 Data Length Code Register */
#define	CAN_MB07_TIMESTAMP	0xFFC02CF4	/* Mailbox 7 Time Stamp	Value Register */
#define	CAN_MB07_ID0		0xFFC02CF8	/* Mailbox 7 Identifier	Low Register */
#define	CAN_MB07_ID1		0xFFC02CFC	/* Mailbox 7 Identifier	High Register */

#define	CAN_MB08_DATA0		0xFFC02D00	/* Mailbox 8 Data Word 0 [15:0]	Register */
#define	CAN_MB08_DATA1		0xFFC02D04	/* Mailbox 8 Data Word 1 [31:16] Register */
#define	CAN_MB08_DATA2		0xFFC02D08	/* Mailbox 8 Data Word 2 [47:32] Register */
#define	CAN_MB08_DATA3		0xFFC02D0C	/* Mailbox 8 Data Word 3 [63:48] Register */
#define	CAN_MB08_LENGTH		0xFFC02D10	/* Mailbox 8 Data Length Code Register */
#define	CAN_MB08_TIMESTAMP	0xFFC02D14	/* Mailbox 8 Time Stamp	Value Register */
#define	CAN_MB08_ID0		0xFFC02D18	/* Mailbox 8 Identifier	Low Register */
#define	CAN_MB08_ID1		0xFFC02D1C	/* Mailbox 8 Identifier	High Register */

#define	CAN_MB09_DATA0		0xFFC02D20	/* Mailbox 9 Data Word 0 [15:0]	Register */
#define	CAN_MB09_DATA1		0xFFC02D24	/* Mailbox 9 Data Word 1 [31:16] Register */
#define	CAN_MB09_DATA2		0xFFC02D28	/* Mailbox 9 Data Word 2 [47:32] Register */
#define	CAN_MB09_DATA3		0xFFC02D2C	/* Mailbox 9 Data Word 3 [63:48] Register */
#define	CAN_MB09_LENGTH		0xFFC02D30	/* Mailbox 9 Data Length Code Register */
#define	CAN_MB09_TIMESTAMP	0xFFC02D34	/* Mailbox 9 Time Stamp	Value Register */
#define	CAN_MB09_ID0		0xFFC02D38	/* Mailbox 9 Identifier	Low Register */
#define	CAN_MB09_ID1		0xFFC02D3C	/* Mailbox 9 Identifier	High Register */

#define	CAN_MB10_DATA0		0xFFC02D40	/* Mailbox 10 Data Word	0 [15:0] Register */
#define	CAN_MB10_DATA1		0xFFC02D44	/* Mailbox 10 Data Word	1 [31:16] Register */
#define	CAN_MB10_DATA2		0xFFC02D48	/* Mailbox 10 Data Word	2 [47:32] Register */
#define	CAN_MB10_DATA3		0xFFC02D4C	/* Mailbox 10 Data Word	3 [63:48] Register */
#define	CAN_MB10_LENGTH		0xFFC02D50	/* Mailbox 10 Data Length Code Register */
#define	CAN_MB10_TIMESTAMP	0xFFC02D54	/* Mailbox 10 Time Stamp Value Register */
#define	CAN_MB10_ID0		0xFFC02D58	/* Mailbox 10 Identifier Low Register */
#define	CAN_MB10_ID1		0xFFC02D5C	/* Mailbox 10 Identifier High Register */

#define	CAN_MB11_DATA0		0xFFC02D60	/* Mailbox 11 Data Word	0 [15:0] Register */
#define	CAN_MB11_DATA1		0xFFC02D64	/* Mailbox 11 Data Word	1 [31:16] Register */
#define	CAN_MB11_DATA2		0xFFC02D68	/* Mailbox 11 Data Word	2 [47:32] Register */
#define	CAN_MB11_DATA3		0xFFC02D6C	/* Mailbox 11 Data Word	3 [63:48] Register */
#define	CAN_MB11_LENGTH		0xFFC02D70	/* Mailbox 11 Data Length Code Register */
#define	CAN_MB11_TIMESTAMP	0xFFC02D74	/* Mailbox 11 Time Stamp Value Register */
#define	CAN_MB11_ID0		0xFFC02D78	/* Mailbox 11 Identifier Low Register */
#define	CAN_MB11_ID1		0xFFC02D7C	/* Mailbox 11 Identifier High Register */

#define	CAN_MB12_DATA0		0xFFC02D80	/* Mailbox 12 Data Word	0 [15:0] Register */
#define	CAN_MB12_DATA1		0xFFC02D84	/* Mailbox 12 Data Word	1 [31:16] Register */
#define	CAN_MB12_DATA2		0xFFC02D88	/* Mailbox 12 Data Word	2 [47:32] Register */
#define	CAN_MB12_DATA3		0xFFC02D8C	/* Mailbox 12 Data Word	3 [63:48] Register */
#define	CAN_MB12_LENGTH		0xFFC02D90	/* Mailbox 12 Data Length Code Register */
#define	CAN_MB12_TIMESTAMP	0xFFC02D94	/* Mailbox 12 Time Stamp Value Register */
#define	CAN_MB12_ID0		0xFFC02D98	/* Mailbox 12 Identifier Low Register */
#define	CAN_MB12_ID1		0xFFC02D9C	/* Mailbox 12 Identifier High Register */

#define	CAN_MB13_DATA0		0xFFC02DA0	/* Mailbox 13 Data Word	0 [15:0] Register */
#define	CAN_MB13_DATA1		0xFFC02DA4	/* Mailbox 13 Data Word	1 [31:16] Register */
#define	CAN_MB13_DATA2		0xFFC02DA8	/* Mailbox 13 Data Word	2 [47:32] Register */
#define	CAN_MB13_DATA3		0xFFC02DAC	/* Mailbox 13 Data Word	3 [63:48] Register */
#define	CAN_MB13_LENGTH		0xFFC02DB0	/* Mailbox 13 Data Length Code Register */
#define	CAN_MB13_TIMESTAMP	0xFFC02DB4	/* Mailbox 13 Time Stamp Value Register */
#define	CAN_MB13_ID0		0xFFC02DB8	/* Mailbox 13 Identifier Low Register */
#define	CAN_MB13_ID1		0xFFC02DBC	/* Mailbox 13 Identifier High Register */

#define	CAN_MB14_DATA0		0xFFC02DC0	/* Mailbox 14 Data Word	0 [15:0] Register */
#define	CAN_MB14_DATA1		0xFFC02DC4	/* Mailbox 14 Data Word	1 [31:16] Register */
#define	CAN_MB14_DATA2		0xFFC02DC8	/* Mailbox 14 Data Word	2 [47:32] Register */
#define	CAN_MB14_DATA3		0xFFC02DCC	/* Mailbox 14 Data Word	3 [63:48] Register */
#define	CAN_MB14_LENGTH		0xFFC02DD0	/* Mailbox 14 Data Length Code Register */
#define	CAN_MB14_TIMESTAMP	0xFFC02DD4	/* Mailbox 14 Time Stamp Value Register */
#define	CAN_MB14_ID0		0xFFC02DD8	/* Mailbox 14 Identifier Low Register */
#define	CAN_MB14_ID1		0xFFC02DDC	/* Mailbox 14 Identifier High Register */

#define	CAN_MB15_DATA0		0xFFC02DE0	/* Mailbox 15 Data Word	0 [15:0] Register */
#define	CAN_MB15_DATA1		0xFFC02DE4	/* Mailbox 15 Data Word	1 [31:16] Register */
#define	CAN_MB15_DATA2		0xFFC02DE8	/* Mailbox 15 Data Word	2 [47:32] Register */
#define	CAN_MB15_DATA3		0xFFC02DEC	/* Mailbox 15 Data Word	3 [63:48] Register */
#define	CAN_MB15_LENGTH		0xFFC02DF0	/* Mailbox 15 Data Length Code Register */
#define	CAN_MB15_TIMESTAMP	0xFFC02DF4	/* Mailbox 15 Time Stamp Value Register */
#define	CAN_MB15_ID0		0xFFC02DF8	/* Mailbox 15 Identifier Low Register */
#define	CAN_MB15_ID1		0xFFC02DFC	/* Mailbox 15 Identifier High Register */

#define	CAN_MB16_DATA0		0xFFC02E00	/* Mailbox 16 Data Word	0 [15:0] Register */
#define	CAN_MB16_DATA1		0xFFC02E04	/* Mailbox 16 Data Word	1 [31:16] Register */
#define	CAN_MB16_DATA2		0xFFC02E08	/* Mailbox 16 Data Word	2 [47:32] Register */
#define	CAN_MB16_DATA3		0xFFC02E0C	/* Mailbox 16 Data Word	3 [63:48] Register */
#define	CAN_MB16_LENGTH		0xFFC02E10	/* Mailbox 16 Data Length Code Register */
#define	CAN_MB16_TIMESTAMP	0xFFC02E14	/* Mailbox 16 Time Stamp Value Register */
#define	CAN_MB16_ID0		0xFFC02E18	/* Mailbox 16 Identifier Low Register */
#define	CAN_MB16_ID1		0xFFC02E1C	/* Mailbox 16 Identifier High Register */

#define	CAN_MB17_DATA0		0xFFC02E20	/* Mailbox 17 Data Word	0 [15:0] Register */
#define	CAN_MB17_DATA1		0xFFC02E24	/* Mailbox 17 Data Word	1 [31:16] Register */
#define	CAN_MB17_DATA2		0xFFC02E28	/* Mailbox 17 Data Word	2 [47:32] Register */
#define	CAN_MB17_DATA3		0xFFC02E2C	/* Mailbox 17 Data Word	3 [63:48] Register */
#define	CAN_MB17_LENGTH		0xFFC02E30	/* Mailbox 17 Data Length Code Register */
#define	CAN_MB17_TIMESTAMP	0xFFC02E34	/* Mailbox 17 Time Stamp Value Register */
#define	CAN_MB17_ID0		0xFFC02E38	/* Mailbox 17 Identifier Low Register */
#define	CAN_MB17_ID1		0xFFC02E3C	/* Mailbox 17 Identifier High Register */

#define	CAN_MB18_DATA0		0xFFC02E40	/* Mailbox 18 Data Word	0 [15:0] Register */
#define	CAN_MB18_DATA1		0xFFC02E44	/* Mailbox 18 Data Word	1 [31:16] Register */
#define	CAN_MB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32] Register */
#define	CAN_MB18_DATA3		0xFFC02E4C	/* Mailbox 18 Data Word	3 [63:48] Register */
#define	CAN_MB18_LENGTH		0xFFC02E50	/* Mailbox 18 Data Length Code Register */
#define	CAN_MB18_TIMESTAMP	0xFFC02E54	/* Mailbox 18 Time Stamp Value Register */
#define	CAN_MB18_ID0		0xFFC02E58	/* Mailbox 18 Identifier Low Register */
#define	CAN_MB18_ID1		0xFFC02E5C	/* Mailbox 18 Identifier High Register */

#define	CAN_MB19_DATA0		0xFFC02E60	/* Mailbox 19 Data Word	0 [15:0] Register */
#define	CAN_MB19_DATA1		0xFFC02E64	/* Mailbox 19 Data Word	1 [31:16] Register */
#define	CAN_MB19_DATA2		0xFFC02E68	/* Mailbox 19 Data Word	2 [47:32] Register */
#define	CAN_MB19_DATA3		0xFFC02E6C	/* Mailbox 19 Data Word	3 [63:48] Register */
#define	CAN_MB19_LENGTH		0xFFC02E70	/* Mailbox 19 Data Length Code Register */
#define	CAN_MB19_TIMESTAMP	0xFFC02E74	/* Mailbox 19 Time Stamp Value Register */
#define	CAN_MB19_ID0		0xFFC02E78	/* Mailbox 19 Identifier Low Register */
#define	CAN_MB19_ID1		0xFFC02E7C	/* Mailbox 19 Identifier High Register */

#define	CAN_MB20_DATA0		0xFFC02E80	/* Mailbox 20 Data Word	0 [15:0] Register */
#define	CAN_MB20_DATA1		0xFFC02E84	/* Mailbox 20 Data Word	1 [31:16] Register */
#define	CAN_MB20_DATA2		0xFFC02E88	/* Mailbox 20 Data Word	2 [47:32] Register */
#define	CAN_MB20_DATA3		0xFFC02E8C	/* Mailbox 20 Data Word	3 [63:48] Register */
#define	CAN_MB20_LENGTH		0xFFC02E90	/* Mailbox 20 Data Length Code Register */
#define	CAN_MB20_TIMESTAMP	0xFFC02E94	/* Mailbox 20 Time Stamp Value Register */
#define	CAN_MB20_ID0		0xFFC02E98	/* Mailbox 20 Identifier Low Register */
#define	CAN_MB20_ID1		0xFFC02E9C	/* Mailbox 20 Identifier High Register */

#define	CAN_MB21_DATA0		0xFFC02EA0	/* Mailbox 21 Data Word	0 [15:0] Register */
#define	CAN_MB21_DATA1		0xFFC02EA4	/* Mailbox 21 Data Word	1 [31:16] Register */
#define	CAN_MB21_DATA2		0xFFC02EA8	/* Mailbox 21 Data Word	2 [47:32] Register */
#define	CAN_MB21_DATA3		0xFFC02EAC	/* Mailbox 21 Data Word	3 [63:48] Register */
#define	CAN_MB21_LENGTH		0xFFC02EB0	/* Mailbox 21 Data Length Code Register */
#define	CAN_MB21_TIMESTAMP	0xFFC02EB4	/* Mailbox 21 Time Stamp Value Register */
#define	CAN_MB21_ID0		0xFFC02EB8	/* Mailbox 21 Identifier Low Register */
#define	CAN_MB21_ID1		0xFFC02EBC	/* Mailbox 21 Identifier High Register */

#define	CAN_MB22_DATA0		0xFFC02EC0	/* Mailbox 22 Data Word	0 [15:0] Register */
#define	CAN_MB22_DATA1		0xFFC02EC4	/* Mailbox 22 Data Word	1 [31:16] Register */
#define	CAN_MB22_DATA2		0xFFC02EC8	/* Mailbox 22 Data Word	2 [47:32] Register */
#define	CAN_MB22_DATA3		0xFFC02ECC	/* Mailbox 22 Data Word	3 [63:48] Register */
#define	CAN_MB22_LENGTH		0xFFC02ED0	/* Mailbox 22 Data Length Code Register */
#define	CAN_MB22_TIMESTAMP	0xFFC02ED4	/* Mailbox 22 Time Stamp Value Register */
#define	CAN_MB22_ID0		0xFFC02ED8	/* Mailbox 22 Identifier Low Register */
#define	CAN_MB22_ID1		0xFFC02EDC	/* Mailbox 22 Identifier High Register */

#define	CAN_MB23_DATA0		0xFFC02EE0	/* Mailbox 23 Data Word	0 [15:0] Register */
#define	CAN_MB23_DATA1		0xFFC02EE4	/* Mailbox 23 Data Word	1 [31:16] Register */
#define	CAN_MB23_DATA2		0xFFC02EE8	/* Mailbox 23 Data Word	2 [47:32] Register */
#define	CAN_MB23_DATA3		0xFFC02EEC	/* Mailbox 23 Data Word	3 [63:48] Register */
#define	CAN_MB23_LENGTH		0xFFC02EF0	/* Mailbox 23 Data Length Code Register */
#define	CAN_MB23_TIMESTAMP	0xFFC02EF4	/* Mailbox 23 Time Stamp Value Register */
#define	CAN_MB23_ID0		0xFFC02EF8	/* Mailbox 23 Identifier Low Register */
#define	CAN_MB23_ID1		0xFFC02EFC	/* Mailbox 23 Identifier High Register */

#define	CAN_MB24_DATA0		0xFFC02F00	/* Mailbox 24 Data Word	0 [15:0] Register */
#define	CAN_MB24_DATA1		0xFFC02F04	/* Mailbox 24 Data Word	1 [31:16] Register */
#define	CAN_MB24_DATA2		0xFFC02F08	/* Mailbox 24 Data Word	2 [47:32] Register */
#define	CAN_MB24_DATA3		0xFFC02F0C	/* Mailbox 24 Data Word	3 [63:48] Register */
#define	CAN_MB24_LENGTH		0xFFC02F10	/* Mailbox 24 Data Length Code Register */
#define	CAN_MB24_TIMESTAMP	0xFFC02F14	/* Mailbox 24 Time Stamp Value Register */
#define	CAN_MB24_ID0		0xFFC02F18	/* Mailbox 24 Identifier Low Register */
#define	CAN_MB24_ID1		0xFFC02F1C	/* Mailbox 24 Identifier High Register */

#define	CAN_MB25_DATA0		0xFFC02F20	/* Mailbox 25 Data Word	0 [15:0] Register */
#define	CAN_MB25_DATA1		0xFFC02F24	/* Mailbox 25 Data Word	1 [31:16] Register */
#define	CAN_MB25_DATA2		0xFFC02F28	/* Mailbox 25 Data Word	2 [47:32] Register */
#define	CAN_MB25_DATA3		0xFFC02F2C	/* Mailbox 25 Data Word	3 [63:48] Register */
#define	CAN_MB25_LENGTH		0xFFC02F30	/* Mailbox 25 Data Length Code Register */
#define	CAN_MB25_TIMESTAMP	0xFFC02F34	/* Mailbox 25 Time Stamp Value Register */
#define	CAN_MB25_ID0		0xFFC02F38	/* Mailbox 25 Identifier Low Register */
#define	CAN_MB25_ID1		0xFFC02F3C	/* Mailbox 25 Identifier High Register */

#define	CAN_MB26_DATA0		0xFFC02F40	/* Mailbox 26 Data Word	0 [15:0] Register */
#define	CAN_MB26_DATA1		0xFFC02F44	/* Mailbox 26 Data Word	1 [31:16] Register */
#define	CAN_MB26_DATA2		0xFFC02F48	/* Mailbox 26 Data Word	2 [47:32] Register */
#define	CAN_MB26_DATA3		0xFFC02F4C	/* Mailbox 26 Data Word	3 [63:48] Register */
#define	CAN_MB26_LENGTH		0xFFC02F50	/* Mailbox 26 Data Length Code Register */
#define	CAN_MB26_TIMESTAMP	0xFFC02F54	/* Mailbox 26 Time Stamp Value Register */
#define	CAN_MB26_ID0		0xFFC02F58	/* Mailbox 26 Identifier Low Register */
#define	CAN_MB26_ID1		0xFFC02F5C	/* Mailbox 26 Identifier High Register */

#define	CAN_MB27_DATA0		0xFFC02F60	/* Mailbox 27 Data Word	0 [15:0] Register */
#define	CAN_MB27_DATA1		0xFFC02F64	/* Mailbox 27 Data Word	1 [31:16] Register */
#define	CAN_MB27_DATA2		0xFFC02F68	/* Mailbox 27 Data Word	2 [47:32] Register */
#define	CAN_MB27_DATA3		0xFFC02F6C	/* Mailbox 27 Data Word	3 [63:48] Register */
#define	CAN_MB27_LENGTH		0xFFC02F70	/* Mailbox 27 Data Length Code Register */
#define	CAN_MB27_TIMESTAMP	0xFFC02F74	/* Mailbox 27 Time Stamp Value Register */
#define	CAN_MB27_ID0		0xFFC02F78	/* Mailbox 27 Identifier Low Register */
#define	CAN_MB27_ID1		0xFFC02F7C	/* Mailbox 27 Identifier High Register */

#define	CAN_MB28_DATA0		0xFFC02F80	/* Mailbox 28 Data Word	0 [15:0] Register */
#define	CAN_MB28_DATA1		0xFFC02F84	/* Mailbox 28 Data Word	1 [31:16] Register */
#define	CAN_MB28_DATA2		0xFFC02F88	/* Mailbox 28 Data Word	2 [47:32] Register */
#define	CAN_MB28_DATA3		0xFFC02F8C	/* Mailbox 28 Data Word	3 [63:48] Register */
#define	CAN_MB28_LENGTH		0xFFC02F90	/* Mailbox 28 Data Length Code Register */
#define	CAN_MB28_TIMESTAMP	0xFFC02F94	/* Mailbox 28 Time Stamp Value Register */
#define	CAN_MB28_ID0		0xFFC02F98	/* Mailbox 28 Identifier Low Register */
#define	CAN_MB28_ID1		0xFFC02F9C	/* Mailbox 28 Identifier High Register */

#define	CAN_MB29_DATA0		0xFFC02FA0	/* Mailbox 29 Data Word	0 [15:0] Register */
#define	CAN_MB29_DATA1		0xFFC02FA4	/* Mailbox 29 Data Word	1 [31:16] Register */
#define	CAN_MB29_DATA2		0xFFC02FA8	/* Mailbox 29 Data Word	2 [47:32] Register */
#define	CAN_MB29_DATA3		0xFFC02FAC	/* Mailbox 29 Data Word	3 [63:48] Register */
#define	CAN_MB29_LENGTH		0xFFC02FB0	/* Mailbox 29 Data Length Code Register */
#define	CAN_MB29_TIMESTAMP	0xFFC02FB4	/* Mailbox 29 Time Stamp Value Register */
#define	CAN_MB29_ID0		0xFFC02FB8	/* Mailbox 29 Identifier Low Register */
#define	CAN_MB29_ID1		0xFFC02FBC	/* Mailbox 29 Identifier High Register */

#define	CAN_MB30_DATA0		0xFFC02FC0	/* Mailbox 30 Data Word	0 [15:0] Register */
#define	CAN_MB30_DATA1		0xFFC02FC4	/* Mailbox 30 Data Word	1 [31:16] Register */
#define	CAN_MB30_DATA2		0xFFC02FC8	/* Mailbox 30 Data Word	2 [47:32] Register */
#define	CAN_MB30_DATA3		0xFFC02FCC	/* Mailbox 30 Data Word	3 [63:48] Register */
#define	CAN_MB30_LENGTH		0xFFC02FD0	/* Mailbox 30 Data Length Code Register */
#define	CAN_MB30_TIMESTAMP	0xFFC02FD4	/* Mailbox 30 Time Stamp Value Register */
#define	CAN_MB30_ID0		0xFFC02FD8	/* Mailbox 30 Identifier Low Register */
#define	CAN_MB30_ID1		0xFFC02FDC	/* Mailbox 30 Identifier High Register */

#define	CAN_MB31_DATA0		0xFFC02FE0	/* Mailbox 31 Data Word	0 [15:0] Register */
#define	CAN_MB31_DATA1		0xFFC02FE4	/* Mailbox 31 Data Word	1 [31:16] Register */
#define	CAN_MB31_DATA2		0xFFC02FE8	/* Mailbox 31 Data Word	2 [47:32] Register */
#define	CAN_MB31_DATA3		0xFFC02FEC	/* Mailbox 31 Data Word	3 [63:48] Register */
#define	CAN_MB31_LENGTH		0xFFC02FF0	/* Mailbox 31 Data Length Code Register */
#define	CAN_MB31_TIMESTAMP	0xFFC02FF4	/* Mailbox 31 Time Stamp Value Register */
#define	CAN_MB31_ID0		0xFFC02FF8	/* Mailbox 31 Identifier Low Register */
#define	CAN_MB31_ID1		0xFFC02FFC	/* Mailbox 31 Identifier High Register */

/* CAN Mailbox Area Macros */
#define	CAN_MB_ID1(x)		(CAN_MB00_ID1+((x)*0x20))
#define	CAN_MB_ID0(x)		(CAN_MB00_ID0+((x)*0x20))
#define	CAN_MB_TIMESTAMP(x)	(CAN_MB00_TIMESTAMP+((x)*0x20))
#define	CAN_MB_LENGTH(x)	(CAN_MB00_LENGTH+((x)*0x20))
#define	CAN_MB_DATA3(x)		(CAN_MB00_DATA3+((x)*0x20))
#define	CAN_MB_DATA2(x)		(CAN_MB00_DATA2+((x)*0x20))
#define	CAN_MB_DATA1(x)		(CAN_MB00_DATA1+((x)*0x20))
#define	CAN_MB_DATA0(x)		(CAN_MB00_DATA0+((x)*0x20))


/*********************************************************************************** */
/* System MMR Register Bits and	Macros */
/******************************************************************************* */

/* ********************* PLL AND RESET MASKS ************************ */
/* PLL_CTL Masks */
#define	PLL_CLKIN			0x0000	/* Pass	CLKIN to PLL */
#define	PLL_CLKIN_DIV2		0x0001	/* Pass	CLKIN/2	to PLL */
#define	DF					0x0001	 /* 0: PLL = CLKIN, 1: PLL = CLKIN/2 */
#define	PLL_OFF				0x0002	/* Shut	off PLL	clocks */

#define	STOPCK				0x0008	/* Core	Clock Off		 */
#define	PDWN				0x0020	/* Put the PLL in a Deep Sleep state */
#define	IN_DELAY			0x0014	/* EBIU	Input Delay Select */
#define	OUT_DELAY			0x00C0	/* EBIU	Output Delay Select */
#define	BYPASS				0x0100	/* Bypass the PLL */
#define	MSEL			0x7E00	/* Multiplier Select For CCLK/VCO Factors */

/* PLL_CTL Macros				 */
#ifdef _MISRA_RULES
#define	SET_MSEL(x)		(((x)&0x3Fu) <<	0x9)	/* Set MSEL = 0-63 --> VCO = CLKIN*MSEL */
#define	SET_OUT_DELAY(x)	(((x)&0x03u) <<	0x6)
#define	SET_IN_DELAY(x)		((((x)&0x02u) << 0x3) |	(((x)&0x01u) <<	0x2))
#else
#define	SET_MSEL(x)		(((x)&0x3F) << 0x9)	/* Set MSEL = 0-63 --> VCO = CLKIN*MSEL */
#define	SET_OUT_DELAY(x)	(((x)&0x03) << 0x6)
#define	SET_IN_DELAY(x)		((((x)&0x02) <<	0x3) | (((x)&0x01) << 0x2))
#endif /* _MISRA_RULES */

/* PLL_DIV Masks */
#define	SSEL				0x000F	/* System Select */
#define	CSEL				0x0030	/* Core	Select */
#define	CSEL_DIV1		0x0000	/*		CCLK = VCO / 1 */
#define	CSEL_DIV2		0x0010	/*		CCLK = VCO / 2 */
#define	CSEL_DIV4		0x0020	/*		CCLK = VCO / 4 */
#define	CSEL_DIV8		0x0030	/*		CCLK = VCO / 8 */

#define	SCLK_DIV(x)			(x)		/* SCLK	= VCO /	x */

/* PLL_DIV Macros							 */
#ifdef _MISRA_RULES
#define	SET_SSEL(x)			((x)&0xFu)	/* Set SSEL = 0-15 --> SCLK = VCO/SSEL */
#else
#define	SET_SSEL(x)			((x)&0xF)	/* Set SSEL = 0-15 --> SCLK = VCO/SSEL */
#endif /* _MISRA_RULES */

/* PLL_STAT Masks										 */
#define	ACTIVE_PLLENABLED	0x0001	/* Processor In	Active Mode With PLL Enabled */
#define	FULL_ON				0x0002	/* Processor In	Full On	Mode */
#define	ACTIVE_PLLDISABLED	0x0004	/* Processor In	Active Mode With PLL Disabled */
#define	PLL_LOCKED			0x0020	/* PLL_LOCKCNT Has Been	Reached */

/* VR_CTL Masks										 */
#define	FREQ			0x0003	/* Switching Oscillator	Frequency For Regulator */
#define	HIBERNATE		0x0000	/*		Powerdown/Bypass On-Board Regulation */
#define	FREQ_333		0x0001	/*		Switching Frequency Is 333 kHz */
#define	FREQ_667		0x0002	/*		Switching Frequency Is 667 kHz */
#define	FREQ_1000		0x0003	/*		Switching Frequency Is 1 MHz */

#define	GAIN			0x000C	/* Voltage Level Gain */
#define	GAIN_5			0x0000	/*		GAIN = 5 */
#define	GAIN_10			0x0004	/*		GAIN = 10 */
#define	GAIN_20			0x0008	/*		GAIN = 20 */
#define	GAIN_50			0x000C	/*		GAIN = 50 */

#define	VLEV			0x00F0	/* Internal Voltage Level - Only Program Values	Within Specifications */
#define	VLEV_100		0x0090	/*	VLEV = 1.00 V (See Datasheet for Regulator Tolerance) */
#define	VLEV_105		0x00A0	/*	VLEV = 1.05 V (See Datasheet for Regulator Tolerance) */
#define	VLEV_110		0x00B0	/*	VLEV = 1.10 V (See Datasheet for Regulator Tolerance) */
#define	VLEV_115		0x00C0	/*	VLEV = 1.15 V (See Datasheet for Regulator Tolerance) */
#define	VLEV_120		0x00D0	/*	VLEV = 1.20 V (See Datasheet for Regulator Tolerance) */
#define	VLEV_125		0x00E0	/*	VLEV = 1.25 V (See Datasheet for Regulator Tolerance) */
#define	VLEV_130		0x00F0	/*	VLEV = 1.30 V (See Datasheet for Regulator Tolerance) */

#define	WAKE			0x0100	/* Enable RTC/Reset Wakeup From	Hibernate */
#define	CANWE			0x0200	/* Enable CAN Wakeup From Hibernate */
#define	MXVRWE			0x0400	/* Enable MXVR Wakeup From Hibernate */
#define	SCKELOW			0x8000	/* Do Not Drive	SCKE High During Reset After Hibernate */

/* SWRST Mask */
#define	SYSTEM_RESET	0x0007	/* Initiates A System Software Reset */
#define	DOUBLE_FAULT	0x0008	/* Core	Double Fault Causes Reset */
#define	RESET_DOUBLE	0x2000	/* SW Reset Generated By Core Double-Fault */
#define	RESET_WDOG		0x4000	/* SW Reset Generated By Watchdog Timer */
#define	RESET_SOFTWARE	0x8000	/* SW Reset Occurred Since Last	Read Of	SWRST */

/* SYSCR Masks													 */
#define	BMODE			0x0006	/* Boot	Mode - Latched During HW Reset From Mode Pins */
#define	NOBOOT			0x0010	/* Execute From	L1 or ASYNC Bank 0 When	BMODE =	0 */


/* *************  SYSTEM INTERRUPT CONTROLLER MASKS ***************** */

/* Peripheral Masks For	SIC0_ISR, SIC0_IWR, SIC0_IMASK */
#define	PLL_WAKEUP_IRQ		0x00000001	/* PLL Wakeup Interrupt	Request */
#define	DMAC0_ERR_IRQ		0x00000002	/* DMA Controller 0 Error Interrupt Request */
#define	PPI_ERR_IRQ		0x00000004	/* PPI Error Interrupt Request */
#define	SPORT0_ERR_IRQ		0x00000008	/* SPORT0 Error	Interrupt Request */
#define	SPORT1_ERR_IRQ		0x00000010	/* SPORT1 Error	Interrupt Request */
#define	SPI0_ERR_IRQ		0x00000020	/* SPI0	Error Interrupt	Request */
#define	UART0_ERR_IRQ		0x00000040	/* UART0 Error Interrupt Request */
#define	RTC_IRQ			0x00000080	/* Real-Time Clock Interrupt Request */
#define	DMA0_IRQ		0x00000100	/* DMA Channel 0 (PPI) Interrupt Request */
#define	DMA1_IRQ		0x00000200	/* DMA Channel 1 (SPORT0 RX) Interrupt Request */
#define	DMA2_IRQ		0x00000400	/* DMA Channel 2 (SPORT0 TX) Interrupt Request */
#define	DMA3_IRQ		0x00000800	/* DMA Channel 3 (SPORT1 RX) Interrupt Request */
#define	DMA4_IRQ		0x00001000	/* DMA Channel 4 (SPORT1 TX) Interrupt Request */
#define	DMA5_IRQ		0x00002000	/* DMA Channel 5 (SPI) Interrupt Request */
#define	DMA6_IRQ		0x00004000	/* DMA Channel 6 (UART RX) Interrupt Request */
#define	DMA7_IRQ		0x00008000	/* DMA Channel 7 (UART TX) Interrupt Request */
#define	TIMER0_IRQ		0x00010000	/* Timer 0 Interrupt Request */
#define	TIMER1_IRQ		0x00020000	/* Timer 1 Interrupt Request */
#define	TIMER2_IRQ		0x00040000	/* Timer 2 Interrupt Request */
#define	PFA_IRQ			0x00080000	/* Programmable	Flag Interrupt Request A */
#define	PFB_IRQ			0x00100000	/* Programmable	Flag Interrupt Request B */
#define	MDMA0_0_IRQ		0x00200000	/* MemDMA0 Stream 0 Interrupt Request */
#define	MDMA0_1_IRQ		0x00400000	/* MemDMA0 Stream 1 Interrupt Request */
#define	WDOG_IRQ		0x00800000	/* Software Watchdog Timer Interrupt Request */
#define	DMAC1_ERR_IRQ		0x01000000	/* DMA Controller 1 Error Interrupt Request */
#define	SPORT2_ERR_IRQ		0x02000000	/* SPORT2 Error	Interrupt Request */
#define	SPORT3_ERR_IRQ		0x04000000	/* SPORT3 Error	Interrupt Request */
#define	MXVR_SD_IRQ		0x08000000	/* MXVR	Synchronous Data Interrupt Request */
#define	SPI1_ERR_IRQ		0x10000000	/* SPI1	Error Interrupt	Request */
#define	SPI2_ERR_IRQ		0x20000000	/* SPI2	Error Interrupt	Request */
#define	UART1_ERR_IRQ		0x40000000	/* UART1 Error Interrupt Request */
#define	UART2_ERR_IRQ		0x80000000	/* UART2 Error Interrupt Request */

/* the following are for backwards compatibility */
#define	DMA0_ERR_IRQ		DMAC0_ERR_IRQ
#define	DMA1_ERR_IRQ		DMAC1_ERR_IRQ


/* Peripheral Masks For	SIC_ISR1, SIC_IWR1, SIC_IMASK1	 */
#define	CAN_ERR_IRQ			0x00000001	/* CAN Error Interrupt Request */
#define	DMA8_IRQ			0x00000002	/* DMA Channel 8 (SPORT2 RX) Interrupt Request */
#define	DMA9_IRQ			0x00000004	/* DMA Channel 9 (SPORT2 TX) Interrupt Request */
#define	DMA10_IRQ			0x00000008	/* DMA Channel 10 (SPORT3 RX) Interrupt	Request */
#define	DMA11_IRQ			0x00000010	/* DMA Channel 11 (SPORT3 TX) Interrupt	Request */
#define	DMA12_IRQ			0x00000020	/* DMA Channel 12 Interrupt Request */
#define	DMA13_IRQ			0x00000040	/* DMA Channel 13 Interrupt Request */
#define	DMA14_IRQ			0x00000080	/* DMA Channel 14 (SPI1) Interrupt Request */
#define	DMA15_IRQ			0x00000100	/* DMA Channel 15 (SPI2) Interrupt Request */
#define	DMA16_IRQ			0x00000200	/* DMA Channel 16 (UART1 RX) Interrupt Request */
#define	DMA17_IRQ			0x00000400	/* DMA Channel 17 (UART1 TX) Interrupt Request */
#define	DMA18_IRQ			0x00000800	/* DMA Channel 18 (UART2 RX) Interrupt Request */
#define	DMA19_IRQ			0x00001000	/* DMA Channel 19 (UART2 TX) Interrupt Requ/*
 * CopyrightTWI0-2009 Analog 2evices L-2 	the ADI BSD license or the GPL-21-2009 Analog 4

/* SYSTE1 & MM REGISTER BIT & ADDRESS DCAN_RX-2009 Analog 8evices CAN Receive the ADI BSD license or the GPe _DTF_BF539_H

/*Devilude all Transmit & MM REGISTER BIT & ADDRESS DM 200_ (or later)
 /

/ices Mem 200 Stream 0 the ADI BSD license or the GP******NITIONS FOR ASP-B********************1****** */
/* System MMR RegisteXVR_STAT-2009 Analogincl******XVR	Status***********************************CM-2009 Analo_LPBl* Clock/ReControl	Message***********************************AP-2009 Analo****** Clock/ReAsynchronous Packet the ADI BS*/

/* the following arTL		r backwards compatibilitym MMR Register M (or la***********MR Register Map) */
#defin000C	

#ifdef _MISRA_RULESCopyright_MF15 0xFu
#define	PLL7 7u
#else
#define	PLL_LOCKCT		0xFFC00010	/
#endif /*r (16-bit) */efine	VRSIC_IMASKx Masks	/

/* CHIP * Copyrightip	IUN Reg_ALLAnalog 0004	    /* Unmask all peripheral ihe ADI BsD Masks */
#defi CHIPID_VERFHIPID_M     0xFter 	
#define CHIPID_FAMILY        gister (16-bit) */
#define	FF000
#d(x)		(1 << ((x)&0x1Fu))NUFACTURE    Pine CHIPID#xD_FAMILY  D Masks */
#define CHI1FF)( CHIPID_MAu ^ */
#define	SWRST		) 0xF0000000/* Software	R	set Register (LL Lock	Count 0xFFC001FF) */
#define	SWRS			0xFFC00100  /* Software	Reset Register (16-bit) */
#define	SYSCR			0xFFC0104  /* System Cofiguration registe */
#define	SIC_IMASK0		0ne	CHIPID			0xFFC00014	/* Chip	IDWRister */

/* CHIPID Masks */
#IWR_DISABLEPID_VERSION         0xFWakeup Disable0
#define CHIPI        0x0FF */
ENfine	SIC_IAHIPID_MANUFACTUR/* InteEnpt Assignment	Register 3 gister (16-bit) */
#define	ine	SIC_ISe	SYS/
#define	SWRST			0xFFC00errupt Status	/* Software	Re Register 2 */
#definee	SYSCR			0xFFC00104  /* System Configurat/* Interrupt As00128	/* InterruptLL Lock	Countnterrupt Wakeup	Register */
#efine	SIC_IMASK1			0xFFC00128	/* Interrupt Mask Register 1 */
#define	SIC_Ie	SIC_IAR1		0xFFC00114  /pt Status Register 1 */
#define	SIne	CHIPID			0xFFC00014	/*e	VR*R6			0xF WATCHDOG TIMER  RegSAR6			0xFF	Register 6rrup			0xtchdog Timer WDOG_CTL Registerster *fine	SIC_IWR0			0xFFC00124  /* IWDEV1FF) 	(fine<<1) & nalog6uguratevent generated on roll overine	SIC_IWR1			0xF00  /* Watchdog	Control	Regiter */
#define	WDOG_CNT	0xFFC00204  /* ne	CHIPID			0xFFC00014	/hdog	Count R_RESET Analog     0xFine	WDOG reset*/
#de	NT	0xFF	00204  /*lock (0xFFC0NMI********03FF) */
#defineter /
#defNT	0xFFC00204  /*lock (0xFFC0GPr */
#de403FF) */
#defineGP IRQ/* RTC Interrupt Control RegisNONE Analog6    0xFno*/
#def* RTC Interrupt Control RegN9 AnalFF003FF) *etatus	wWatchdogme Clock (0xFDIS9 AnalAD003FF) *drupt Asine	RTC_ALARM	0xFFC003RO9 Anainclu   0xFine	RTC_A0xFFG_CNver	lWatcefine	VRdeprecDOG_C (0xFFC00200 -	0x	FC002F/* Vlegacy coderrupt Mask ReFC0000   Control SIC_ISR0300 -xFFC00300  Control ReOG00300 - Controller (0xFFC00SIC_ISRter *Register (0xFFC00400	-	      0xFFC00400  /* SIC_ISRter *Register (0xFFC00400	-ne	UART0_RBR	      0xF#define	EV0 Contrgist0400  /* Transmister#define	UAor the GPLMe	SI	FC004N (0xFFC00400	-#defineFC00DI/
#define	TT	0xFAST	 Divisor Lalte_P14  0x01errupt Enable R1gister2  0xFFC00404_P Count SIC_IAR6			0xFFter 6 * REAL Inte CLOCKrupt Asssignment	Register 6  Inter


/*RTC***** and	dentALARM r00 -	0xFme Clock (0RTSEC Analog 003F 0xFReal- Tim Clock Second        0x0FFRTMIN  0xFFC00FCices  Line Control ReMinute */
#define	UARHRasm/def_F004	/*  Line Control ReHour */
#define	UARDAYisteFFFE0004	/*  Line Control ReDay2FF) pt Identalter*/
#define	UART0_LCRSWIter */
#1  0xFStopine	R the ADI BSStatus	me Clock (0ART0_*/
#defin/* Alarm4	 /* Global Control RegisterS*/


/* SP4     0ister *(1 Hzr the ADI BSl Control RegisterM*/


/* SP8ACTUREol Regi0  /* SPI0 Control Register H*/


/* S100	ConLine S0  /* SPI0 Control Register D*/


/* S2ne	SPI24I0_STAT(041C500  /* SPI0 Control Register D */

nalo4ne	SPIDayntrolle_TDB,I0_ST,FLG			0,_CTL			500  /* SPI0 Control Register WCRT0_GC00314 			0rite Completegisters andl Control  SCR ScraificaRegister */
#define	UAEF0_GCTL		     0xFFC00424E
#defFla_ALARM	0xFFC0A0_SH
/* SPI0	Controlle SPI0_RDBR Shadow	RegiSter */
#de	SPI0_CTL			0xFFC0050 SPI0	RDBR Shadow	RegiMter */
#deSPI0_FLG			0xF SPI0_RDBR Shadow	RegiHter */
#dine	SPI0_STAT SPI0_RDBR Shadow	RegiDter */
#d */
#define	SPI0_TDBR		r */
#define	TIMER0_COUster  Buffer Register */
#define	SPI0_RDBR			0xFFC0051 SPI0_RDBR Shadow	RegiWP  /* RSP-BF/
#define	Pgistng gulato	(RO)ve Data BufferOMegister */
#define	SPI0_BAUD SPI0 Baud FASTRE    (dentPRENRE   Width	RegisteSIC_ISRPRESCALE	 COUNERSION   1	HIPIStatus	prescaler son Re runs at FC00rol Registergura	_COUNTER			0xFFC
			0x** Must be TC_Saf4  /power-up Presproper efination ofIdenxFFC0061Dfine	RTC_Pdentificatioer 1 ister ter */

ne	UART0_LCR	 C_     R	      /* Line Control Register */
#define	UARC_T0_MCUART0_M0410  /*	Modem Control Register */
#defiC_ne	Uefine	    0xFFC00414  /* Line Status RegisteC_r */
ster */T0_SCR	      0xFFC0041C  /* SCR1C	/*  Timer 1 alte/aud rate Rter */

/* CHIPID Masks */
#dTOP0013C		UART0_   0xFFC00424	 /* Global Contefine	TIMER2_ister		r */

Controller (0xFFC00500 - PID Masks */
#dECOND		 */

#I0_CTL			0xFFC00500  /* SPI0 Control Register */NUT/


*/
#d0_FLG			0xFFC00504  /* SPI0define	TIMER2_HOU Reger */SPI0_STAT			0xFFC00508  /define	TIMER2_28	/* us reg#define	SPI0_TDBR			0xFFC0050C  /* SPI0 Transmit Yer	   	it DataRegister */
#define	SPI0_RDBR			0xFFC00510  /* SPI0 Receive Data BuffeRITE_COMPLETEfer	Reg
#define	SPI0_BAUD			0xFFC00514  /* SPIsor Latch (High-Bytnt	Register 6 *sed  CONTROLLerrupt Assignment	Register 6 6 */


/*sed x_LCR0200 -	0xFF) */
#define	WDOG_CTL	0xFFC0020LS1FF) *fine-5untrol	R3ster *Word	Length SelecASK0		0xFFC0010C  /ag Mask Interrut A Regiter (set directly) */
#defineister */


/* Real	Time Clock (0STBr */
#fine	RTC0xFF	Bi         0x0FFPch Count84  /* RPartroll Control RegisterEIMER0_Wine	0708  SPI	efine	F) */
#defin_S			0xFF4  /*  */
ER_ENABickdefine	FASKA_S			0xFC00718fer ister *t Break * Copyright LABegistesk InteDivisor L00424AccesC  /* ter (set dir04  0x07C00720  /* g Mask 6er (toggle) g Mask 5T			0xFFC007g Mask 4upt A Registg Mask 3KA_S			0xFFCg Mask      0xFFCWLSRT0_Dter */
#defin#defi0ine	TIter (of pinM */
#define	FIO_pyrightLOOP	SIC  /* F/* Loopolta Mablel Control Register(toggle)_PxFFC00fine	TIMER2_WIlag Mask /* RT#defineate of pinsS Interrupt B Register D Regter 	0xFFCta	Read0xFFC00720  /O/


/* 2defiOv ADIn Erroine	UART0_LCRP/


/* 4/
#define	FISource SensitiviF/


/* 8defiFram   /ne	FIO_BOTH			0xFBI1C  /* F/*pt B Reit) */
#defin 0xFFC004HRata B2/* SYSHR Emp	0xFFC00720  /TEM - 0x4/* SYSSRh Regsed _t Enable Regiister  */



#definer (clear)   /*#define	FIO_MASKBBI		0xFFC00728  /* F			0xFFCInterrupt P			0xFFC     0xFFCO			0xFFC */
#definDRg Mask   /* Flag MaIE Interrupt B Register ERBFr */
#	     Status	Core regBuffer FullIO_INEN					0xFFC00740ETBEr */
#I0	ConStatus	>


/***0xFFC008able  & MM REGISransmit ClocLSr */
#	SPI0_RT0_TCLKX/* Timebit) */
#define0 Transmit F */
#define	SPORTck DiTCR2				0xFFC0080r */
 /* SPORT0 Transmit I Interrupt B Register NIN - 0xF */
#defin****Uefine#define	SPORTRCR1				0	FIO_MASKB_T		 */
 /* SPORT0 TransmitGFC00200 -	0xF	0xFFC00740UCR1_Pefine	SPORT0_TCLof pi	rol Rter 3 */
#defiER1_Pder */
#define	SIrDAIR					0xFFC00740 POL   0xF	SPI0_0_RC	TX Polfine	F
 *
gV			0xFFC0082R  /* SPORSPI0_Feive Rlock Divider */
#define	SPORTFty Regiine	SPIForcedefine		ne	FIOOnSPORT0_TFS Frame Sync FC0073C */
#define	Slag Set	on BOT0xFFC00830  /*  SPORT0 Stat#define	FIO_MASKBFer */
#de00728  /* 0_RFSguration 1 Regist8  /* */
#define	SPORTisteTCR2				0xFFC0080C008 /* SPORT0sor Latch (Hig  SERIAL PORTT0_IIR	      0xFFC00408  /* In*/


/*SxFFCx_TCR1FFC002FF) *xFFC0083SisterGCTL		  SYSX */
#defrrupt Mask ReTCLK */
#defiFFC0he Anal0xFFrol Regi*/
#d			0xFFC00828DTYty Rnalog00620TXer */ */
matt   /*A Register (togglT0_MT_NORM 0xFFC003ster */
 SPORT NSPORlnnel Transmit SeleULAWe	TIMER0_CONSPI0ollers   /u-Lawnnel Transmit SeleA* SPORT0 Culti-Channel TransmAt Select RegisteTLSBI - 0xFF/
#defTX Bit Ord0_RCR2				0xFFITFSr */
#04	/* smit Select lag e Syncter 0 */
#define	SPORFSe	UART4
/* SYSFFC00850  /* D liiredter 0 */
#define	SPOD
#define	8vices I */
Independ#defxFFC00850  /* SPORT0 Multi-ChanneL#defineDevices Loweceive Se	ect Register 1 */
#defineA#define/

/* SYLate CloC00850  /* SPORT0 Multi-ChannelCKFC00_WIDTH	ansmitrol ReFallSet	odgelti-ChannelfigurationRRegi1C	/*  Timeter */

/* CH			0xFFC00828/* SP00848  /* SPlti-Channel Transmit Select RegisteT
#defer 2 */
#defORT0_MTCS3			0xFFC0084C  /* nfiguration Re2FFC002FF) */
#define	WDOG_CTL	0xFFC002SLENMask Inne	SWRST	ster xFFC0TX(set  irectl	(2 - 31Width	0xFFC0010C  /*figuration 2 Regiter */
#define	SPORT1_TCLKDIV			0xFFC009ister */


/* Real	Time Clock (0TXSTCS1		1 SPORTX_CTL			ar	FIO_MASKA_T			0xFFCTSFc DividSPORT0#deftereo2 */
#define1_TX			0xFFC00910  RFS - 0xF SelecTX Right-First44  /*ect Re14	/* Chiter 3 */


ster 2 */
#defineRSPORT0_MTCS0			0RFFC00840  /* SPORT0 MuRti-Channel Transmit SeleRt Register 0 */
#define	SPORT0_MTCS1			0xFFC0R844  /* SPORT0 Multi-Channel Transmit Select Register 1 */nogulato     /S2			0xFFC00848  /* SPORT0 Multi-Channel Transmit Select Register 2 */
#define	SPORT0_MTCS3			0xFFC0084C  /* SPORT0 MRlti-Channel TransRit Select Register 3 */
Rdefine	SPORT0_MRCS0			0R 2 */
#define	SPORT0_MRCS3			0xFR Receive Select/* SPORT1 Curr
#define	SPORT0_MRCS1			0xFFLORT1_CH_MRCS2			0xF/* SPORT /* SPORT0 Multi-Channel ReORT1_CHelect Registe/* SPORT1 Current Channel RegisterC0085C  /* SPORTxFFC00924hannel Receive Select Register 3 */


/* SPORT1 Controller (0xFFC00900 - 0RFFC009FF) */
#define	SPORT1_TCR1				0xFFC00900  /* SRORT1 Transmit Configuration 1 Register */
#define	SPORT1RTCR2				0xFFC00904  /* SPORT1 Transmit Configuration 2 Register */
#defRne	SPORT1_TCLKDIV			0xFFC00908  /* SPORT1 Transmit Clock Divider */
#efine	SPORT1_MTCS3			0xFFC009ister */


/* Real	Time Clock (0Rnc Divider */
*/
#ine	SPORT1_TX			0xFFC00910 R/* SPORT1 TX Da*/
#dgister */
#define	SPORT1_RX			0xFFR00918  /* SPORTX Data Register */
#define	SPOT1_RCR12C	/* Time0  /* SPORT1 Mster */
#	    /* SIFO Notnable Rgulatoregister 0 */
V_SHADOW	I0	CoRX Underflowister 2 */
#define	SOORT1_MRCS	SPI0RX  /* C  /* SPORT1 Multi-ChanTXfine	TIMER0_COr 2 ve S08  /3 */


/* External PORT1_MRCine	SPTFFC0095C  /* SPORT1 Multi-ChanTel Receiv */
#dTt Register 3 */


/* External B /* Flauffer ReTX Hold0200 -	0xF0800 - 0xF1_MRCS2		MCMC	0xFFC00920  /* SPOWOMANUF0xFFC003FC  /Multic *
 * LWindow OffTC_SFieldct Register 3 0xFFC	Macrooller (0xFFC000904  /* SPORT1 Transmit ConET_nousuration 2trol	3Fister *trol	Registe	 0 */
#define	EBIU_AMBCTL1Only	use 1 */
SIZE  /* A With Logic OR While	SeT0 Mu	Lter	*/
#def Interrupt A RegControl Re	SYS*/
#x)>>0x3)-1ue	SWiste<< 0xC*/
#define	EBIU_SDGCTL			Size = (x/8)-1FFC00908  /* SPORT1 */

/* SDRAM Controller*/
#define	EBIU_SDGCTL			0xFFC00A10  /* SDRAM Global Control Register */
#define	EBIU_SDBCTL			0xFFC00A14  /* SDRAM Bank Control Register */
#define	EBIUSDRRC)
#deFC00A18  /* SDRAM Refresh Rate Control Register 	Register 5 */
#define	SIC_I		0xFFC00A0R2				0xFFC0F) */
#dCC_FLAnalog3Control	Registerrol ReRecble yLKDIV			0xFFC0082REC_BYPASSegister 1 */BypassIR				(No Alternate deprWidth	Registeames2FROM4
/* SPI0	Conate dep 2 MHz Alternfrom 4ine	DMA0_TC */
#define	DMA8_TCP16Counts R0_TC_PER
#de8ine	DMA0_TCCNT		16DMAC0_TC_CNT


/* DMA MCDTXMTCS1			0ter trol	RegisterInc.PORT0_TFSr (1ock Divider */
MCDR		0xFFC00C /* * DMA Channel 0 NCore regiptor Pointer RegisteMORT0_MTC/
#detrol	RegisterC00850R				0xFFC00730  /* PerFSaritbal 8 Register */
#define	DM  /* to44  /*RelER1_Wshipointer RegistFble 0xSR	    ister */
#define	DMDelayCOUNOUNT			0xFFC00_C0072C TH			0xefine	EBIU_SDunt Register= 0/
#define	DMA0_Xne	FIDevic0xFFC00C14	/* DMA Channel 0 X Mgisterfine	DMA0_X2xFFC0093C0xFFC00C14	/* DMA Channel 0 X M2/
#define	DMA0_X310	/3Y			0xFFC00C14	/* DMA Channel 0 X M3/
#define	DMA0_X4R0_WIDTH			0xC00C14	/* DMA Channel 0 X M4/
#define	DMA0_X510	/5Y			0xFFC00C14	/* DMA Channel 0 X M5/
#define	DMA0_X610	/6Y			0xFFC00C14	/* DMA Channel 0 X M6/
#define	DMA0_X710	/7Y			0xFFC00C14	/* DMA Channel 0 X M7/
#define	DMA0_X8egister */
#dC00C14	/* DMA Channel 0 X M8/
#define	DMA0_X910	/9Y			0xFFC00C14	/* DMA Channel 0 X M9dify Register */
C0072Ane	DMA0_Y_COUNT			0xFFC00C18	/* DMA odify Register */
ne	FIBne	DMA0_Y_COUNT			0xFFC00C18	/* DMA  Channel 0 Y Coun1t RegCne	DMA0_Y_COUNT			0xFFC00C18	/* DMA FC00C1C	/* DMA Ch1annelDne	DMA0_Y_COUNT			0xFFC00C18	/* DMA _CURR_DESC_PTR		01xFFC0ne	UDMA0_Y_COUNT			0xFFC00C18	/* DMA ptor Pointer Regi1ster SR	  MA0_Y_COUNT			0xFFC00C18	/* DMA 24	/*efineAR6			0xFFC PARALLEL	PERIPHERAL INTERFACE (PPI)rupt Assignment	Registe */
#dnel PPI_C0070C /* RTC_COUNTce SensitivitORTh (Lo_MTCS0			0PPI Por00514  /*fine	DMA1_X_MODIFDIRhannel Tran	/* DMA CDirecR1_W_COUN	0xFFC00740XFR_0_MTC1			0xFFC0	/* >


/fer	Typl 1 X Modify RegistCFG Y Co3SPORT	/* DMA CConfiguMER1_WI	0xFFC00C08	LD_SELine	TIMER* DMA CAct regEBIU_Ati-Channel TransmiPACKFY			0xFFannelDMA Chptor PoKDIV			0/*imerviisteversDMA0 of	defBF539.h erroneously includedt 2032_X_CO 32-bitl 0 NStatusWidth	RegisteSKIgglefine	SPORT0_AddrSkip Elem#def500 - 0xFFC005FF) *Q_STAOceive SelectC68	/* DMAven/OddMA Chann2 */
#define	DLENGTHnnel  SPORT0Addr4  /*irectlyxFFC00C6C	/* DMAdefine0_COUNT/*	pheral Map	Regi	00000/* V DMA=DMA Channel 0ne	DMC30	/** SPO1 */
#defirectlyister Interrupt A Regnt Reg
#define	DMA0_ne	DMA1_CURR_Y_1OUNT		0xFFC00C78	/* DMAt Reg1*/
#define	DMA1_CURR_Y_2OUNT		0xFFC00C78	/* DMAannelister */
ne	DMA1_CURR_Y_3OUNT		0xFFC00C78	/* DMAxFFC02*/
#define	DMA1_CURR_Y_4OUNT		0xFFC00C78	/* DMAster  0 Y Modine	DMA1_CURR_Y_5OUNT		0xFFC00C78	/* DMA 0 Cul 1 Pdefine	DMA1_CURR_Y_6OUNT		0xFFgister (16-bit) */
#define	 DMAMask Intterr9pt A Reg7u 0 Tr11)COUNipheral MMA1_CURR(only worTC Presx=10-->x=16xFFC00908  /* SPORTannel 2 X Count Rgister *
#define	DMA2_X_MODIFap	RegisC00C94	/* DMA Channel 2 X Modify Rister */


/* Real	Time Clock (0POLC10	/er */define	DigSeleck Diviies/
#defDMA Channel 2 Y   0xIDTH			0xAddrrol Reck DivideMA Channel 2 Yefineter */
#dAddrC00850  /* rent DescripefineX CoRCR1		Controller R_DESC_PTR		0xFFFL0C10	/e SelectEBIU_AIndicatorrent Address ReT_ER DMA C SPORT0C00850TrO_DIne	FIO_BOTH			0xFOVrity RDevices  (0x	Register ne	FIO_BOTH			0xFUN* DMA /

/* SYERIPHC0095Flag Source SensitiviERR_D0 - 0xSP-BF538ne	FIODetecOG_C2_IRQ_STAT*/
#define	DMA2NCO */
include ne	FIOelecCorT			* DMA Channel 2 #define	SPORT0_MCInc.
0070C  /* Flag M      0xFFC00408  /* Int */
#DMAxountFIG,
#def_yyESC_PTR0xFFC00920  /* SPODMAATUS			CS0			0
 *
 * L Receive Data BuffeNrity Reel Tran
 *
 * LUNT			0xF (W/R*Width	RegisterDol R_8ODIFY			r (set dte Co8 bInterrupt A Regress Rer 0 (0xFter *efine	DMA316	ONFIG				0xFFC00CC8	/* 32define	DMA3 Configura32on Register */
#dDMA2Dhannel Trans2D/1D*egister *
#define	DSTAR - 0xFFg InpuRestar
 * Copyright Ifine	DMA1_CURR_D Multi-e ADI BSti-Channel TransmitITATUS			scriptofine	DMA3_Y_COUl Control RegisterNess R		0xF9 */
#dNext	DescripSTATte Coine	DMA3_Y_MODIFY_ransm004	/* C	/* DMA Channel 3 Y	 Mod(0xFF/AutobFFC00Width	Registester */0xFFCer */
A3_CURR_DESC_PTR		0xFFC0 Channel 0 Y ster */A3_X_SPORT0_3_CURR_DESC_PTR		0xFFC0FC00C1C	/* DMster */3ation DMA Channel 3 Current Addres_CURR_DESC_PTster */ER			 Select3_CURR_DESC_PTR		0xFFC0ptor Pointer ster */5fine	tus Register */
#define	DMA3_24	/* DMA Chaster */ 0 (06tus Register */
#define	DMA3_A0_IRQ_STATUSster */7ask Itus Register */
#define	DMA3_us Register *ster */giste SPORT03_CURR_DESC_PTR		0xFFC0DMA Channel 0ster */9		0xFFC00CDC	/* DMA Channel 3 Y	RR_X_CO3_X_MODIEXT_FLOonfiC00C28	erip  /*0000	/* Modify Regnel 4 Ne_*/

MODIFY			lag Masfine	DMA3_X_MODIefine	DMAAUTisterDevices DMA Channe00D04	/* DMA Channel 4 StaRR */
#dSP-BF538DMA ChannelArrarecated	register efine	DMA4MID_V Currentfigum
#deR			lannel 4 ConfListtion Register */
#define	LARGY			0scriptorLargDMA0_C* DMA Channel 4 X Count Reel 3 Next Descg Mask fine	Sr Register */
#define	DMA3_Sg Mask	     xFFC00CC4	/* DMA Channel 3 Start AddX Coug Mask	SPI0_*/
#define	DMA3_X_MODIFY			0xFFlag 5C00C00	 Channel 3 X Modify RegFlag 6on RegisteDMA3_Y_COUNT			0xFFC00CD8	/* DMA g Mask70D20	/* DMA Channel 514  /* SPI0 BXT_DEIRQ/* DMA , 0xFFC00CFC00D24	/*DMA Channel 3 Next De_Disteptor PointInc.Donegis Channel 2 Current r *//* DMA Cel TranisterSourc			0xFFC00D28	/* DMA CFEefine Channel 3DMA ChannelFe0424	 	0xFFC00D28	/* DMA ChanRU			0xFFC /* F		0xFunn   /_COUNT		0xFFC0 Register */
#deFlag Don RegQ_STATUS			0xFFC00D28	/* DMA ChannelFlag 	     /Status Register */
#define	DMA4_PERIFlag I0	Con0xFFC00D2C	/* DMA Channel 4 Peripheral Map	ReFlag FC00C0efine	DMA4_CURR_X_COUNT		0DR			0xFster */
#d_MAP		0xFFC00Cinter Register0xFFC00920itions */
0_MTCSster */
#defnc.
 *
 * Lfine	 Channel 4 Peripheral DMA 04  /* 00D20	/t Address Register */
#defBIT POSITIONcriptor PointCAPt) *annel 3 Y CMA 8s RegOTIMER1_WI2_IRQ_STATUS			0xFFC00C	DMAonfir Pointer RFFC016D50	/* DMA Chati-CChannel 2 Current 	DMA3fine4	/* DMA CFFC0ess RegFC00D54	/* DMA Channel 5 X Modify RWrity Re SelectFFC0efine	* DMA Channel 5 X Coel 5 X Modify RRister * SPORT0 	0xFFadfine	DMA5_Y_MODIFY			0xFFC00D5C	/MA4  /* SR	     Modi/* SoftwareMap	EBIU_AMBCC00CA_PTREnco    s */
8	/* D000	/*Counodify Registeptor_Per */
#deRegist_PTRDMA1_Y_COUMAA5_CURR_ADDR			0xT1_RC0RXddress Regi Chanr */
#xFFC00C04rent Address Register */
#Tefin/

/* SYRQ_STATUS			>


/****rent Address Register */
1defin 0 Y  Register */
1	0xFFC00D68	/* DMA Channel 5 Inter1upt/SSP-BF538nnel 5 Peripefine	DMA5_PERIPHERAL_MAP		0xFFIC0072*/
#dUNT		0xFFCID68	/* DMA Channel 5 sed #def_COUNT			0x Chan	0xFF0xFFC00D68	/* DMA Channel 5 xFFC0TD78	/scriptorannel 5 CuPORT0_TFSrent Adscriptor Pointer Register */
#define	D Channel 0 Y gister */
2RXCurredefine	DMA2_T_ADA6_STAurrent Y Count Register */

#A6_STATT_ADDR		DeviFC00D84	/* DMA ChFFC00D80	/* DMA */
#define	DMA6_ST3RT_ADDR		/

/FC00D84	/* DMA 3hannel 6 Start Address Register */3#define	D 0 Ydefine	DMA6_X_COUD88	/* DMA Channel 6 ConfiguratI1_ADDR		rrenFC00D84	/* DI1FY			0xFFC00D94	/* DMA C2_ADDR		0C28odify Registe2 Count Register */

#defiD6C	ADDR		inclFC00D84	/* */
#urrent Y Count Register */

#defi5_CUADDR		al M_MODIFY			0xFFC0D88	/* DMA Channel 6 Configursed udefine	DM DMA_MODIFY			0xFFChannel 6 Start Address Registsed ugister */_Y_Cscriptor PointerFFC00D80	/* DMA Cnnel 1 ConfiguDMA ChGEN/
#dePURPOSE Interrupt A	SPORT0 Multi-Channel ConfigPWMg Timer Regpyrigi	DMA0er */
#Inter	SIC_IS		SPORT0_RCR2				0xFFInteNegisterCS0			0efine	SPTime	odify Registe DMA 0_DLH	 el Tranpheral Map	Re Channel 0 Y  DMA gister annel 3pheral Map	ReFC00CC00DAC	/* DMA C /* SPORTr */
#define	DTCR2				0xFFC0080nnel 6 */
#define	DMA6_PE#definee	SPORT0_RCR2				0xFFTIMDISChannel 6 Perirrupt As Timergister */
#definDISDMA6_CURR_X_COChannel 7 NextB0	/* DMA ChanneDIS6 Current X CoChannel 7 Next*/
#define	DMA6_CxFFC_Y_COUNT		0xFFC00DB8ter  DMA Channel 6 Curre00DCY Count Register */RCR1		AL_MAP		0xFFC00DAC	/* DMILChannel 6 Peri7 Next D80C  /* SPORT0 Transm			0xDMA6_CURR_X_COMA7_STARl 7 X Count Register */
#d6 Current X Coart Addrl 7 X Count Register *OVFnnelC0072C  Transm Next DCouMA3_ Register MA7_Y_COUNT			0xFFCne	FIOD4	/* DMA7_STAR7 Y Count Register */
#define	DMA7_Yt Reg1_CURR_Dart Addr7 Y Count Register */
#defineRU ChanneDevices Channel Slav		0xFFC0egulatorE0	/* DMA Chann0_DLH	/

/* SYSA7_STARr Pointer Register */
#define	DMA7_gisterSP-BF538/rt Addrr Pointer Register */
#7_X_COUNT			0xF_Y_COUNT		0xFFC00DB8ILFC00DC8	/* DMA ChanneIL Configura_Y_COUNT			0xFFC0ster */
#define	Se	DMA7_Y_#define	FIO_MASKBgister */#define	SPORT0_TCRanne04  /* DC#define	DMA7_C04  /* DDent Address Re04  /* DERegisAlit SetRegiine	RTC_  /* As Provi 1 CgistBltage RegCableChannr Control Register (TOVLxFFC00D			0xFFC0er */

#define	MD1A0_D0_NEXTannel 6 Curfine	MD2A0_D0_NEXTERAL_MAP		0xFne	MDM04  0xFFC00DEC	DESC_PTR	0xFFC00E0_P 7 Periphera0 Stream 0 DestinaT_ADDR		0xF2_PRegister *DESC_PTR0200 -	0xterrupt A RegiWM_OUThannel 6  Control ReTH_CPTR	_CURR_0xFFC00810XT_ti-ChCounts nel 6 ConfULSE_Hr */
#def 1 RegisterRIOD_C/
#defi008 Multi-ChanQgle) 0DD8	/* 		0xFFC00DBNfine	DODIFY		egister */Uster  /* RT04ster */
#dCLKX Count	Reg8_DESC_PTR	0xGGLter */
#d1NT		0xFFC00EMUp	Regiter */
_COUNT			0xFFC00C90	/* DMA Che	DMTYPMask Inter &ation/
#defi4)00908  /* SPORT0E18	/* MemDMA0 Stream 0Destinationne	CHIPID			0xFFC00014	/*ivisor LatODEFFC0072C  ream 0 Destinatne	FIO_MASKB_T		egister  */
#define	SPORTD0_X_COUNTFlag Dguration RemDMA0 C	/* DMA Channel 7n	X Co#define	FIO_MASKBefine	M#define	SPORT0_TC0xFFC00g Mask Interrupt eam 0 Desg Mask 00E10	/* M Registg Mask 9Count	Register *FFC0072CUART0_DLL	
#define	ne	FIOivisor/ssignment	Register 	ine	DMA6-IRQ_STATI/Ot Register */

#define	annel 1 RDBR tatu(FIO_)0xFFC00920  /* SPOPRegisNFIG			0xFFC00EPFDMA6_CURR_0 Destinati6 Current 0 Destinati3ine	TIMER0 Destinatifineeam 0 DestinatiPF5unt	Register */
#dPFefine	D0_X_MODIFY		PF7E14	/* MemDMA0 StrPF5_X_COUion	X ModifyPF9er */
#defiDestinatioC0072C4COUNT	0xFFC00A Channel DMA0 Stream NEXT_D0t	Register */
iptor PoiDMA0 Stream xFFC00C20DMA0 Stream ster incless RexFFC0Fation Registe DMA Channel 2FEC	/* DMDMA0 Stream Channelral Map Regig Maskeral Map Reg3g Maskguration ReF4	0xFFC0 MemDMA0 Strg Mask	FIO_MASKBPF6g Maskr (clear) PF7inter Rne	MDMA0_D0_g Mask_X_COUNT	0xF9g Maskegister */am 0g MaskAtion	Current	g MaskBRegister */

g Mask0	/* DMA CNEXTg Maskent X CounE40	g MaskUART0_DLL	 0 Sg Maskivisorgister */
#define	M  GPIOY Count Register */

#define	nterruort	C	0xFFC00E2C	/* MemDC_MODIFY	FC00E44	/* C_MODIFYheral Map ReCxFFC00	/* MemDMA0 SCster  Destination	CC 0 CuX Count	RegistCxFFC0define	MDMA0_DCA1_CUR_COUNT	0xFFC0Cipher
#def_S0_Y_COUN SelPos */
#defin0E58	/* MemFlag Deam 0 SourceMA4_CU Register */Flag  */
#defineCFC00_S0_CONFIG			CxFFC0E48	/* MemDMC0 Steam 0 Source Confiuration RegisCer *
#de_DESC_PTRD	0xFFC00D5C	/ AlaA0 Stream 0 SourDe Y Count Register D/
#defi
#define	MDMDannelFFC00E10	/* MPD/
#define	MDMA0_S0_D_MODIFY		0xFFC00E5CD/* MemDMA0 Stream 0DSource Y Modify RegDster */
#define	MDMD0_S0_CURR_US		0xFFC00ransmination	CurreD10xFFCnt	Register *D1A3_Xine	MDMA0_S0_ND1e	DMC_PTR	0xFFC00ED1ER		emDMA0 Stream D1C00Ce Nexster */
#dxFFC00E60	/* MemDMA0 StreaD 0 Source CurrentDDescriptor PointeD 0 S0 Stream 0 SoDgisttart Address D Register */
#defDne	MDMA0_S0_CURR_DDDR		0xFFC00E64	/D MemDMA0 Stream 0DSource Current AdDress Reg0_S0_CURR_X_0xFFCUNT		0xFFC0DE50	/ MemDMA0 StreDm 0 Surce X Count Degistr */
#define	DDMA0_0_X_MODIFY		0DFFC0054	/*_S0_Y_COEce Sensitivity0_IRQ_STATUS		0xFFCEe Y Count Register Eam 0 Source InterruEt/Status Register *E/
#define	MDMA0_S0_E_MODIFY		0xFFC00E5CE/* MemDMA0 Stream 0ESource Y Modify RegEster */
#define	MDMEefine	MDMA0_S0_CURREX_COUNT	0xFFC00E70	E* MemDMA0 Stream 0 Eource Current X CouEt Register */
#defiEe	MDMA0_S0_CURR_Y_CEUNT	0xFFC00E78	/* EemDMA0 Stream 0 Source CurrE 0 Source CurrentEDescriptor PointeE0_D1_NEXT_DESC_PTE	0xFFC00E80	/* MeE Register */
#defEne	MDMA0_S0_CURR_EDDR		0xFFC00E64	/E MemDMA0 Stream 0ESource Current AdE	0xFFC00E84	/* MeEDMA0 Stream 1 DestEnation	Start AddreEs Register */
#defEne	MDMA0_D1_CONFIGE		0xFFC00E88	/* MeEDMA0 Streasor Latch (High	MC2			0xter */
#define	DMA1_XSCOUNT			0	xFFC00C50	/* DMA Ct RegistIxFFC00C00958  /* SPORT1TIMOunt RegFFC00C0er */
#d Ir */7_CUcated	register naDBR_CORefine	Dnel 4	r */
ify Ren	Periphs,C0030When URR_X08  /#define	SPORT/
#define	MDM	    	tion	*/
#defT	0xFFC00EB0	/* MemDgisteable Register  */ */
#DM) */RCS3			0	A5_PEfy R,	/* DUntilt (0xFable Register  */
MemDMA0 StreaFC00Cestinaefine	Current	Y Count	tream 1 DestinatSZER 0, 1, 2 Regisnd	Zero (_CURR_Y_C	able 	0xFnd nter/Lastel 3 Start AddG */
	TIMER0_CONGeX Core RegistMA0 Strea,  /* wfine/Discardel 3 Start AddPSc Dividration	Rr Poi-ti-Cha	Inpur */
#define	DMA3_Y_EMISister 0xFFC006Status	gura As Out0 StxFFC005FF) */FY			0xer *		0xFFC00of3 Con0xFF6/8*k IntWidth	RegisteLSBfine	TSPORdefinSB  Regi(0xFFC00900 - 0CPHACount Reg4 Y C2 Currhase	0xFFC00ED4	/* Me Y Mer */
#defi 2 Current Des0xFFC00900 - 0MST DMA ine	DMA0_Ya-	0x/r Poi*0xFFC00900 - 0W*/

tor Pointer /
#definen Drainurce Y X_COUNT		0xFFty RegiIDTH			0xine	IO_FLAG__POLAR			IPHERFL DMA Channel 3 NextFLS0xFFCRR_X_COUNT		0s (=1)fine_FLOUT1 as fDBR o0_S1_X/* Vine	00EC8	sA Register (togglFLSA3_X_CO X Count Retream 1 Source C2rrent Descriptor Pointer Register */
#define	MDMA0_e	DMAr */
#d		0xFFC00EE4	/* MemDMA3rrent Descriptor Pointer Register */
#define	MDMA0_ER			D/
#def		0xFFC00EE4	/* MemDMA4rrent Descriptor Pointer Register */
#define	MDMA0_C00CED4	/* D		0xFFC00EE4	/* MemDMA5rrent Descriptor Pointer Register */
#define	MDMA0_ 0 (0xCURR_D		0xFFC00EE4	/* MemDMA6rrent Descriptor Pointer Register */
#define	MDMA0_ent Xscripto		0xFFC00EE4	/* MemDMA7rrent Descriptor Pointer Register */
#deefine	MDMA0G0xFFC0s RegisTR		FC00 (=0)inteurce Curre	t Descriptor Pofine	 Register */
Register */
#dGA3_X_ SelectMA_D0_START_ADDR MDMA0_D0 StART_ADDR
#define MDMA_D0_CONFIG MDA0_D0_CONFIG
#e	DMA SPORT0MA_D0_START_ADDR MDMA0_D/* MART_ADDR
#define MDMA_D0_CONFIG MDMA0_D0_CONFIG
#ER		ess RegisA_D0_START_ADDR MDMA0_DP	0xART_ADDR
#define MDMA_D0_CONFIG MDMA0_D0_CONFIG
#C00C/

/* SYESC_PTR MDMA0_D0_CURR_DERR_XART_ADDR
#define MDMA_D0_CONFIG MDMA0_D0_CONFIG
# 0 (SP-BF538ESC_PTR MDMA0_D0_CURR_DEDMA0ART_ADDR
#define MDMA_D0_CONFIG MDMA0_D0_CONFIG
#ent include ESC_PTR MDMA0_D0_CURR_DE

#d#define MDMA_D0_X_MODIFY MDMA0_D0_X_Mfine	MDMA0_S1Bit	00E60	/* MemDMA0 StreFFC0Flag D 6 Peripheraltream 1 Source Current Descriptor Pointer Register */
#define	MDMA0_ST_ADDR
#* MemDMA0 Stream 1 Source C0 Stream 1 Source Current Address Register */
#defineT_ADDR
# RegIRQ_STATUS		0xFFC00EE8	/* MemDMA0 Stream 1 Source Interrupt/Status Register T_ADDR
#_ADDR		0xFFC00EE4	/* MemDMAP	0xFFC00EEC	/* MemDMA0 Stream 1 Source Peripheral MaT_ADDR
#5ter */
#define	MDMA0_S1_CURR_X_COUNT	0xFFC00EF0	/* MemDMA0 Stream 1 Source CurrT_ADDR
#6ount Register */
#define	MDMA0_S1_CURR_Y_COUNT	0xFFC00EF8	/* MemDMA0 Stream 1 ST_ADDR
#7urrent Y Count Register */

#define MDMA_D0_NEXT_DESC_PTR MDMA0_D0_EXT_DESC_PTRT_ADDR
#9e MDMA_D0_START_ADDR MDMA0_D0_START_ADDR
#define MDMA_D0_CONFIG MDMA0_D0_CONFIG
#dT_ADDR
#ADMA_D0_X_COUNT MDMA0_D0_X_COUNT
#define MDMA_D0_X_MODIFY MDMA0_D0_X_MODIFY
#definT_ADDR
#BD0_Y_COUNT MDMA0_D0_Y_COUNT
#define MDMA_D0_Y_MODIFY MDMA0_D0_Y_MODIFY
#define MDMT_ADDR
#00620ESC_PTR MDMA0_D0_CURR_DESC_PTR
#define MDMA_D0_CURR_ADDR MDMA0_D0_CURR_ADDR
#dT_ADDR
#DDMA_D0_IRQ_STATUS MDMA0_D0_IRQ_STATUS
#define MDMA_D0_PERIPHERAL_MAP MDMA0_D0_PERIT_ADDR
#EMAP
#define MDMA_D0_CURR_X_COUNT MDMA0_D0_CURR_X_COUNT
#define MDMA_D0_CURR_Y_COUNT_ADDR
#C  /*CURR_Y_COUNT

#define MDMA_S0_NEXT_DESC_PTR MDMA0_S0_NEXT_DESC_PTR
#define	0xFFC00958  /* SPORT1SPIFfine	DMA4_Irrupeam  wMemDine	ransle-wSPORtstinatioulat			0xFFCnel 0 Y COD MDMA_S TranOUNT MDMAin a mce Y 	deviceA0_S1_some other S1_Y_COtr2_CUto becefin MDMA0
/* External B0xFFC00C00	/COUNT MDMA0_S1_MDMA_misCURR occne Sw*/
#no new dnneli1_X_C_gistemit Frame Syncgister */
#dMDMA0_S1_annelxFFC00egulator(0=
#defin1=08  ty */
#define	BSY*/
#define	OUNT MDMA0_S1_RR_ADDs rore redne MDMURR_Xfream 1 DestinatRefine MDg InpuMDMAURR_XS MDMA0_S1_IRQ_STATUS
#define MDMA_S1_CURR_ADDR
#deCO_VERSICURR_D Mem	TC_Seam , coADI BSRR_ADmay have	beS1_CURR_DEtRR_Y
#define	MDMA0_S1_CURR

/* CHIPID Masks */
#_PTRefinFDMANUDESC_PTR
#defR MDMA0_D0ol Register */
#2efineBPPI_STATUS			0xFFC01004	/*2ol Register */
#3efine7PPI_STATUS			0xFFC01004	/*3ol Register */
#4efinE_MANUSTATUS			0xFFC01004	/*4ol Register */
#5efinDPPI Delay Count Register */5ol Register */
#6efinBPPI Delay Count Register */6ol Register */
#7efin7PPI Delay Count Register */7_POLARer (set) */
#define	FIO_F  ASYNCHRONOrentEMORY 2 Current Y Coun0DA8	/* DMA Ch_D1_PEREBIU_AMefineC00958  /* SPORT1AMCKscriptor PointStatus	CLKOUTence Register BEN  0xFFdefine	DMAAll Banksnnel 7 S_AMBC01408	/* SlaveB
#defin_X_COUNT		0xivide RegisteMemoryster  0 00C94TWI0_SLAVE_STAT		0x_B00EE0	/ X Count RegMode Status Register */
s 0 &	1efine	TWI0_SLAVE_ADDR		0xFFC_BA3_X_CO_S0_CURR_X_de Address Register */
#defi, 1,h RegFC00C1C	/* DM SlaveID_VERSIO_S1_IRQ_STAde Address Register */
#def(aA_S1MASTE	2ER_STA_CURR_DESC_PTCDPRIister MA5_X_MODIFhas prioine	F00204cam 1/* Vexit SeleaC			0egister Internal	Time xFFC00E60	/* MemDMA0 Strer */
# /* SPORter */
LAVE_CTRL		0xFFC01408	/* SlaveFC0072C S0			0ivide Regist	egister0	Mast, 00DA- b
#defi-3ime Regid/* F1 -r */
#de*/
#dee	TWI0_SLAVE_STAT		FC00E28140C	/*WI0_FIFO_CTRL		0xFFC01428	/* 1IFO	Control&1_FIFO_ST,	 01efinControl	ReFIFO_STAT		0xFFC0142C	/* Fam 0 SouS0_Y_WI0_FIFO_CTRL		0xFFC01428	/*1xx -RegisContro(Cont 0,	1, 141C	/* )ngle Byte Rer Internal	BCTL0/* RTC H	Edges Regi0RDYch Count		0xFFC	DMA2 */
#deRDYC01428	/* =me Regine M*/
#def* FIFO	Receive	D
#defineta Dou2 Register */
#dePTR		0xhighWI0_aTR		0	lowne M0_REGBefine0148C	/* FIFO	TT/
#dVERSION   4 Register */>


/ */
#efineCCNT		fy R	toMODIFY	A3_CcycFC0148C	/* FIFO	TTnt RVERSION   8ds compatibility */
#define	TWI0_PRESCALE	 TWI0_C2NTROL
	/* FIFO	ReceivTThannVERSION   Cds compatibility */
#define	TWI0_PRESCALE	 TWI0_C3eneral-Purpose Ports  (0FC00E30	/* GPIds compatibility */
#define	TWI0_PRESCALE	 TWI0_C4eneral-Purpose Ports S are for backw1O	Pin Port C Setupefine	TWI0_AOE asserRR_Yto_PRES/efine				0xFFC=ONTROL
#define	TWI0_IST_SRC	 TWI0_IN2Data	Register */
#define	GPIO_C_C			0xFFC01520	/* Clear GPIO Pin General-Purpose Ports S(0xFFC01500 -	3Data	Register */
#define	GPIO_C_C			0xFFC01520	/* Clear GPIO Pin define	GPIO_C_CNFG			0SFFC01500	/* GPIO	Pin Port C  */
#define	GPIO_C_C			0xFFC01520	/* Clear GPIO Pin O_C_D			0xFFC01510	/* H are for backw4O	Pin Port C egistfine	TWI0_PRES Clear	de			0xFFC0152_C_C D Configur_CONTROL
#define	TWI0_IHT_SRC	 TWI0_IN8IO_D_CNFG			0xFFC01504	/* GPIO	Pin Port D Configuration Register */
#dGeneral-Purpose Ports H(0xFFC01500 -	CIO_D_CNFG			0xFFC01504	/* GPIO	Pin Port D Configuration Register */
#ddefine	GPIO_C_CNFG			0HTX_MO500	/* GPIO	Pin Port C xFFC01504	/* GPIO	Pin Port D Configuration Register */
#d0eneral-Purpose Ports RA are for back1IO	Pin Port C fy Re_C			0xfine	_CONTROL
#define	TWI0_IGPIOSRC	 TWI0_I2FFC01564	/* GPIO	Pin Port D InputGeneral-Purpose Ports GPIOxFFC01500 -3FFC01564	/* GPIO	Pin Port D Inputdefine	GPIO_C_CNFG			0GPIOC01500	/* G4FFC01564	/* GPIO	Pin Port D InputO_C_D			0xFFC01510	/* GPIOream 0 DS
#dFFC01564	/* GPIO	Pin Port D Input5C			0xFFC01528	/* Clear Grrent	X er MFFC01564	/* GPIO	Pin Port D Input6C			0xFFC01528	/* Clear Gr */
#deNEXTFFC01564	/* GPIO	Pin Port D Input7C			0xFFC01528	/* Clear G5_X_COUN** */
C01564	/* GPIO	Pin Port D Input8C			0xFFC01528	/* Clear GE38	/*  Ena9ster */
#define	GPIO_E_INEN			0xF9 Register */
#define	GPIO__MODIFY		0AFFC01564	/* GPIO	Pin Port D Input n Register */
#define	GPIO_IFO	Stat00BFFC01564	/* GPIO	Pin Port D Input ONTROL
ter */
#define	GPIO_am 0 SouO_D_FC01564	/* GPIO	Pin Port D Input NFG			0xFFC01508	/* GPIO	PXT_DES01B10D/* DMA Controller 1 Traffic ControPIO_E_D			0xFFC01518	/* GP0	/* M01B10E/* DMA Controller 1 Traffic Contro_C			0xFFC01528	/* Clear G Sourc		 0xF/* DMA Controller 1 Traffic Contro	0xFFC01538	/* Set GPIWPIO_D_INEN			MA6_CONFI */
#deefine	n Port D InMA3_CTROL
#define	TWI0_I				SRC	 TWI0_*/
#definNEXT_DESC_PTR		0xFFC01C00	/General-Purpose Ports 				xFFC01500 gister */NEXT_DESC_PTR		0xFFC01C00	/define	GPIO_C_CNFG			0				C01500	/* SP-B Address Register */
#define	DMAO_C_D			0xFFC01510	/* 				PIO Pin Po*/
# Address Register */
#define	DMAFC01C00	- 0xFFC01FFF)					n	Port E R X ModifyNEXT_DESC_PTR		0xFFC01C00	/01548	/* Toggle GPIO P				ort	E Regi00D98	/* NEXT_DESC_PTR		0xFFC01C00	/1558	/* GPIO	Pin Port 				rection ReA6_Y_MODINEXT_DESC_PTR		0xFFC01C00	/FC01568	/* GPIO	Pin Po				 Input Ena
#define	NEXT_DESC_PTR		0xFFC01C00	/1 Traffic Control Regi					_MODIFY		t DescripNEXT_DESC_PTR		0xFFC01C00	/*n Register */
#define						ControlleC00DA4	/*NEXT_DESC_PTR		0xFFC01C00	/* */
#define	DMAC1_TC_C					0xFFC01B1er *Q_STATUS			0xFFC01C28	/* DMA ChanADDR			0xFFC01C04	/* DMA C*/

/* Alt 1 NQ_STATUS			0xFFC01C28	/* DMA Chan8_CONFIG				0xFFC01C08	/* s code comne	UQ_STATUS			0xFFC01C28	/* DMA Chane	DMA8_X_COUNT			0xFFC01C1CCNT			DMASR	 Q_STATUS			0xFFC01C28	/* DMA Chan	0xFFC01538	/* Set GP1e	Data Single_LPBlQ_STATUS		1/
#de*/
#deWI0_RCV_DATA16		0xFFC0148C	/* FIFO	0xFeive	Data *****Channel 9 Next D*/

#define TWI0_REGBASE		TWI0_CLKDIV

/* the followi1g are for ba*****Channel 9 Nelity */
#define	TWI0_PRESCALE	 TWI0_CONTROL
#define	TWI0_G			SRC	 TWI0*/
/* DMA Channel 9 Configuration Register */
#define	General-Purpose PortsG			xFFC01500er *nel 9 X Count Register */
#define	DMA9_X_MODIFY			define	GPIO_C_CNFG			G			C01500	/* GPIO	Pin Port el 9 Configuration Register */
#define	O_C_D			0xFFC01510	/*1GPIO	Pin PoLL_CTL	DMA9_Y_MODI */
#define	GPIO_C_C			0xFFC01520	/* orESC_PTRister */
#define	GPIO_D_D			0xFine	SRC	 TWIC00004_PTR		0xFFC01C60	/* DMA Channel 9 Current Descriptor Pointer Register0xFFC01C54	/* DMA ChanGPIO Pin Po 0 YC64	/* DMA Channel 9 Current Address Register */
#define	DMA9_IRQ_STATUA Channel 9 Y Count Reefine	GPIO_C_INEN			0xFFC01C01C60	/* DMA Channel 9 Current Descriptor Pointer Registerdify Register */
#defies */
#defi*****64	/* DMA ChaxFFC01504	/* GPIO	Pptor Point D Configuration Register */0	/* DMA Channel 8 NextMA9_SRC	 TWI*/
/*	0xFFC01C78	/* DMA Channel 9 Current Y Count Register */

#define	DMA10_N0xFFC01C54	/* DMA Chan/
#define	Gfy Re	0xFFC01C78	/* DMA Channel 9 Current Y Count Register */

#define	DMA10_NA Channel 9 Y Count RePIO Pin Port	D Register */
	/* DMA Channel 9 Current Y Count Register */

#define	DMA10_Nn Register */
#define1GPIO_D_INENLL_CTLFFC01C44	/* DMO	Pin Port D Input Enable	Register */
 ChanSRC	 TWC00004fy Register */
#define	DMA10_Y_C0xFFC01C54	/* DMA ChanIO	Pin Portnterrufy Register */
#define	DMA10_Y_CA Channel 9 Y Count Re* GPIO	Pin OUNT		fy Register */
#define	DMA10_Y_Cdify Register */
#defiar GPIO Pin*/
#Channel 10 Current Descriptor Poine	DMA9_NEXT_DESC_PTR		01C14	/* DMrrenChannel 10 Current Descriptor Poin01548	/* Toggle GPIO ter *r */
#d0C28Channel 10 Current Descriptor Poin1558	/* GPIO	Pin Portter *5_X_COUChannefy Register */
#define	DMA10_Y_CFC01568	/* GPIO	Pin Pter *E38	/* al Ml Map Register */
#define	DMA10_CU1 Traffic Control Reg Chann0_IRQ_ DMAdify Register */
#define	DMA10_Y_COY			0xFFC01C94	/* DMA Channne	FIO_Y_Cdify Register */
#define	DMA10_Y_CO */
#define	DMAC1_TC_ Chann/
#defress	Rfy Register */
#define	DMA10_Y_COY_MODIFY			0xFFC01C9C	/* D*/

/*  1 Nfine	DMA11_START_ADDR		0xFFC01CC4	/A10_CURR_DESC_PTR		0xFFC01s code ne	UAine	DMA11_START_ADDR		0xFFC01CC4	/ter Register */
#define	DMCCNT			SR	 Register */
#define	DMA11_X_COUNT		e	DMA9_NEXT_DESC_PTR								 */
X Modif0	0xFFC01C90SC_PTR		0xFFC01C0Y_COUNT			0xFFC01C98	/* Descriptor 10 Y C64	/* DMA Cha
#define	DMA8_START_ADDR			0xFFC01C04	/* DMA CxFFC01nel 10 ount Register */
#define	DMA11_Y_A Channel 9 Y Count Re	/* DMA ChDMA Chaount Register */
#define	DMA11_Y_dify Register */
#defi01C10	/* DR_ADDR	ount Register */
#define	DMA11_Y_			0xFFC01CD4	/* DMA Channefine	ne	DMA1ount Register */
#define	DMA11_Y_ Channel 10 Interrupt/01C18	/* Dster */ount Register */
#define	DMA11_Y_xFFC01CAC	/* DMA Chann01C1C	/* Dheral Mount Register */
#define	DMA11_Y_RR_X_COUNT		0xFFC01CB00xFFC01C20nnel 10ount Register */
#define	DMA11_Y_efine	DMA10_CURR_Y_COUne	DMA8_CUCB8	/* ount Register */
#define	DMA11_Y_Register */

#define	DM*/
#defineSC_PTR	nel 11 Current Y Count Register */xt Descriptor Pointer  Register 
#definnel 11 Current Y Count Register */MODIFY			0xFFC01CDC	/* DMAhannel 1 N_START_ADDR		0xFFC01D04	/* DMA Channe_CURR_DESC_PTR		0xFFC01CE0ART_ADDR		_START_ADDR		0xFFC01D04	/* DMA ChanneRegister */
#define	DMA11_r */
#defi_START_ADDR		0xFFC01D04	/* DMA Channe	0xFFC01538	RCV_DATA8		0xFF8  /0958  /* SPORT1B2e	Data Single Byte Register 2/
#define	TWI0_RCV_DATA16		0xFFC0148C	/* FIFOMA12eive	Data Double Byte RegDMA Ch*/

#define TWI0_REGBASE		TWI0_CLKDIV

/* the followi2g are for backwards compatinel 6 C*/
#define	TWI0_PRESCALE	 TWI0_CONTROL
#define	TWI0_PTR	SRC	 TWI0_INT_STAT
#defiel 12 Current Descriptor Pointer RegistGeneral-Purpose PortsPTR	xFFC01500 -	0xFFC015FF)	el 12 Current Descriptor Pointer Registdefine	GPIO_C_CNFG			PTR	C01500	/* GPIO	Pin Port el 12 Current Descriptor Pointer RegistO_C_D			0xFFC01510	/*2GPIO	Pin Port C Data	Regist201C60	/* DMA Channel 9 Current Descriptor Pointer Register */
#define	DMA9_CUR		0xne	GPIO_C_S			0xFFC0153012 Current X Count Register */
#define	DMA12_CURR_Y_COUNT		/
#define	DMA12_IRQ_STGPIO Pin Port	C Register */12 Current X Count Register */
#define	DMA12_CURR_Y_COUNT		efine	DMA12_PERIPHERALefine	GPIO_C_INEN			0xFFC0112 Current X Count Register */
#define	DMA12_CURR_Y_COUNT		ne	DMA12_CURR_X_COUNT	es */
#define	GPIO_D_CNFG		2/* DMA Channel 9 Current Y Count Register */

#define	DMA10_NEXT_DESC_PTR		0xFFC00	/*514	/* GPIO	Pin Port D DRegister */
#define	DMA13_X_MODIFY			0xFFC01D54	/* DMA Channel/
#define	DMA12_IRQ_ST/
#define	GPIO_D_S			0xFFC0Register */
#define	DMA13_X_MODIFY			0xFFC01D54	/* DMA Channelefine	DMA12_PERIPHERALPIO Pin Port	D Register */
Register */
#define	DMA13_X_MODIFY			0xFFC01D54	/* DMA Channeln Register */
#define2GPIO_D_INEN			0xFFC01564	/* Chan	Pin Port D Input Enable	Register */
#defiPIO	Port E Register	NameFC01D68	/* DMA Channe/
#define	DMA12_IRQ_STIO	Pin Port E Configuration FC01D68	/* DMA Channeefine	DMA12_PERIPHERAL* GPIO	Pin Port E Data	RegisFC01D68	/* DMA Channene	DMA12_CURR_X_COUNT	ar GPIO Pin Port E Register FC01D68	/* DMA Channe	0xFFC01538	/* Set GPxFFC0n	Port E Register */
#deFC01D68	/* DMA Channe01548	/* Toggle GPIO xFFC0ort	E Register */
#definFC01D68	/* DMA Channe1558	/* GPIO	Pin PortxFFC0rection Register */
#defFC01D68	/* DMA ChanneFC01568	/* GPIO	Pin PxFFC0 Input Enable	Register *FC01D68	/* DMA Channe1 Traffic Control Reg#defin (0xFFC01B00 - 0xFFC01BFC01D68	/* DMA Channel Address Register */
#definController 1 Traffic CoFC01D68	/* DMA Channel */
#define	DMAC1_TC_#defin0xFFC01B10	/* DMA ContrFC01D68	/* DMA ChannelD6C	/* DMA Channel 13 Peri*/

/* Alternate deprecaFC01D68	/* DMA ChannelOUNT		0xFFC01D70	/* DMA Chs code compatibility */
FC01D68	/* DMA ChannelDMA13_CURR_Y_COUNT		0xFFC0CCNT			DMAC1_TC_CNT


/*FC01D68	/* DMA Channeler */

#define	DMA14_N						 */
#define	DMA8_NEXT_2ESC_PTR		0xFFC01C00	/* DMA Channel 8 Nextdefinriptor Pointer Register C01DA8	/* DMA Channel /
#define	DMA12_IRQ_STMA Channel 8 Start Address RC01DA8	/* DMA Channel efine	DMA12_PERIPHERAL	/* DMA Channel 8 ConfiguratC01DA8	/* DMA Channel ne	DMA12_CURR_X_COUNT	01C10	/* DMA Channel 8 X CouC01DA8	/* DMA Channel Address Register */
#defin4	/* DMA Channel 8 X ModC01DA8	/* DMA Channel escriptor Pointer Regi01C18	/* DMA Channel 8 Y CouC01DA8	/* DMA Channel MA Channel 14 Start	Ad01C1C	/* DMA Channel 8 Y ModC01DA8	/* DMA Channel 88	/* DMA Channel 14 C0xFFC01C20	/* DMA Channel 8 C01DA8	/* DMA Channel 0xFFC01D90	/* DMA Channe	DMA8_CURR_ADDR			0xFFC01CC01DA8	/* DMA Channel 1 Address Register */
#*/
#define	DMA8_IRQ_STATUS		C01DA8	/* DMA Channel 1T			0xFFC01D98	/* DMA  Register */
#define	DMA8_PEC01DA8	/* DMA Channel 1	/* DMA Channel 14 Peripheral Map	Register */
#defC01DA8	/* DMA Channel 1		0xFFC01DB0	/* DMA Channe8 Current X Count RegistC01DA8	/* DMA Channel 1_CURR_Y_COUNT		0xFFC01DB8	A Channel 8 Current Y CoC01DA8	/* DMA Channel 1	0xFFC01538	/* Set GP30xFFC01C40	/* DMA Channel 9 3ext Descriptor Pointer Register */
#define	DM_STATART_ADDR			0xFFC01C44	/*annel */

#define TWI0_REGBASE		TWI0_CLKDIV

/* the followi3				0xFFC01C48	/* DMA ChannMA6_X_M*/
#define	TWI0_PRESCALE	 TWI0_CONTROL
#define	TWI0_15_C0	/* DMA Channel 9 X Cou/* DMA Channel 15 Current X Count RegisGeneral-Purpose Ports15_C 9 X Modify Register */
/* DMA Channel 15 Current X Count Regisdefine	GPIO_C_CNFG			15_CC01500	/* GPIO	Pin Port /* DMA Channel 15 Current X Count RegisO_C_D			0xFFC01510	/*3ne	DMA9_CURR_DESC_PTR		0xFF301C60	/* DMA Channel 9 Current Descriptor Pointer Register */
#define	DMA9_CURNFIGDR			0xFFC01C64	/* DMA Cnnel 16 Configuration	Register */
#define	DMA16_X_COUNT			0r */

#define	DMA16_NEChannel 9 Interrupt/Status nnel 16 Configuration	Register */
#define	DMA16_X_COUNT			0Register */
#define	DMefine	GPIO_C_INEN			0xFFC01nnel 16 Configuration	Register */
#define	DMA16_X_COUNT			0r */
#define	DMA16_CONA9_CURR_Y_COUNT		0xFFC01C783/* DMA Channel 9 Current Y Count Register */

#define	DMA10_NEXT_DESC_PTR		0xFFC0tor 	/* DMA Channel 10 Next ine	DMA16_CURR_ADDR			0xFFC01E24	/* DMA Channel 16 Current Addr */

#define	DMA16_NE0 Start	Address	Register */ine	DMA16_CURR_ADDR			0xFFC01E24	/* DMA Channel 16 Current AddRegister */
#define	DMPIO Pin Port	D Register */
ine	DMA16_CURR_ADDR			0xFFC01E24	/* DMA Channel 16 Current Addn Register */
#define3Channel 10 X Modify0xFFC01DEC	1D68	/* DMA Chann	ress Register */
#defDMA Channel 10 Y Count RegisPTR		0xFFC01E40	/* DDMA16_PERIPHERAL_MAP	0x/* DMA Channel 10 Y Modify R#define	DMA17_START_ARegister */
#define	DMFC01CA0	/* DMA Channel 10 Cu#define	DMA17_START_Ar */
#define	DMA16_CONe	DMA10_CURR_ADDR			0xFFC01C#define	DMA17_START_A */
#define	DMA15_IRQ_Sr */
#define	DMA10_IRQ_STAT#define	DMA17_START_A01548	/* Toggle GPIO 		0xFus Register */
#define	D#define	DMA17_START_A1558	/* GPIO	Pin Port		0xF0 Peripheral Map Registe#define	DMA17_START_AFC01568	/* GPIO	Pin P		0xFDMA Channel 10 Current X#define	DMA17_START_A1 Traffic Control Reg*/

#dxFFC01CB8	/* DMA Channe#define	DMA17_START_AD4	/* DMA Channel 15 X*/

#dEXT_DESC_PTR		0xFFC01CCRR_ADDR			0xFFC01E64	/ */
#define	DMAC1_TC_*/

#dter */
#define	DMA11_STRR_ADDR			0xFFC01E64	/DDR		0xFFC01E44	/* DMA Charess	Register */
#defineRR_ADDR			0xFFC01E64	/17_CONFIG			0xFFC01E48	/* figuration	Register */
#RR_ADDR			0xFFC01E64	/ine	DMA17_X_COUNT			0xFFC0 11 X Count Register */
RR_ADDR			0xFFC01E64	/ */
#define	DMA15_IRQ_hannel 11 X Modify Register3*/
#define	DMA11_Y_COUNT			0xFFC01CD8	/* */

#hannel 11 Y Count RegistPTR		0xFFC01E80	/* e	DMA16_PERIPHERAL_MAP	0x DMA Channel 11 Y Modify Regdefine	DMA18_START_ADDRegister */
#define	DM1CE0	/* DMA Channel 11 Curredefine	DMA18_START_ADDr */
#define	DMA16_CONA11_CURR_ADDR			0xFFC01CE4	/define	DMA18_START_ADDent Y Count Register */

#fine	DMA11_IRQ_STATUS		0define	DMA18_START_ADD/
#define	DMA17_Y_COUN Register */
#define	DMA11_Pdefine	DMA18_START_ADD */
#define	DMA17_Y_MOeripheral Map Register */
#ddefine	DMA18_START_ADDster */
#define	DMA17_Channel 11 Current X Count Rdefine	DMA18_START_ADDnt Descriptor Pointer 01CF8	/* DMA Channel 11 Currdefine	DMA18_START_ADD/* DMA Channel 17 CurreESC_PTR		0xFFC01D00	/* DMA C	0xFFC01EA4	/* DMA ChanxFFC01E68	/* DMA Chann
#define	DMA12_START_ADDR		0	0xFFC01EA4	/* DMA ChanR		0xFFC01E84	/* DMA Channr */
#define	DMA12_CONFI	0xFFC01EA4	/* DMA ChanCONFIG			0xFFC01E88	/* DMAgister */
#define	DMA12_	0xFFC01EA4	/* DMA ChanDMA18_X_COUNT			0xFFC01E90gister */
#define	DMA12_	0xFFC01EA4	/* DMA Chan* DMA Channel 12* SPORT0 Multi-Channel  SDRAM 2 Current Y Count Register */

#define	DM0	Master InternaSDTime Reference RegisteSCT_IERVERSION   10614	/*  TiSCLK[0], /SRASt	AdCress	RWE, SDQM[3:0]Master Mode ALR_ADDR			0xFFC01ine FFC01EASRegiencye	DMA16_PERIPHERAL_MAP	CLUS		0xFFC01D28	/A Channel 19 Configuratdefine	GPIO_C_CNFG		PFC007in Port C DaA Channel 1FFC01pref* DMA Count Regist4  /GPIO_C_S			0erruFFC01ED4efine	TWI0_INT_STAAMC r licen X Count RegisASRster VERSION     l Regis4IFY			0ter */Refresh* DMAUNT	f-9_Y_MODED8	/* DMA Channe0xFFC0in Port C DDESC_define	DMA10h Reg1 ArCLKDY_MODIFY			0xFFC01EDC	/* DMA Channel 19 Y  19 X Modify 738   Glo#define	DMnnels	9_Y_MODIFY			0xFFC01EDC	/* DMA ChanneTRAS */
#define	GPIO*/
#defintRASput Enable	Register */annel514	/* GPIO	Pinress Register */General-Purpose Portanneldefine	GPIO_D_Sress Register */define	GPIO_C_CNFG		annelC01500	/* GxFFCress Register */O_C_D			0xFFC01510	/annelPIO Pin Por1Address Register */	0xFFC01538	/* Set Ganneln	Port E Re18	/* DMA Channel 1901548	/* Toggle GPIOannelort	E Regis1	DMA19_PERIPHERAL_M1558	/* GPIO	Pin Porannelrection RegRegiress Register */FC01568	/* GPIO	Pin annel Input Enab2Address Register */1 Traffic Control Reannel escriptor P28	/* DMA Channel 19D4	/* DMA Channel 15 annel re for back2	DMA19_PERIPHERAL_M/* DMA Channel 18 Intannel SRC	 TWI0_Ionfiress Register */
 Interrupt/Status RegisteXT_Demory BanAddress Register */
AP	0xFFC01EEC	/* DMA Chan0	/*emory Ban01F04	/* MemDMA1 Str*/
#define	DMA19_CURR_X_C Souemory Baner */
#define	MDMA1_ 19 Current X Count RePare for backisteress RegistP0	/* DMA Channel 8 NexisteSRC	 TWI0_MA6_CMA1_D0_Y_COUNT	 Interrupt/Status RegiPer */
#defiDESCnation	Y Count	RAP	0xFFC01EEC	/* DMA CPnnel 19 Per*/
#dMA1_D0_Y_COUNT	*/
#define	DMA19_CURR_PCOUNT		0xFF0C84gister */
#definination	X Modify Registen	Port E RgisteMA1_D0_Y_COUNT	NT		0xFFC01EF8	/* DMA Pannel 19 Cul 1 er Register */
#r */

#define	MDMA1_D0C*/
#d* DMA Channeress RegistCD */
#define	DMA19_IRQ_STCunt RC40	/* DMA C */
#define	MDMA Interrupt/Status RegiCChannIRQ_STAister */
#define	MDMAAP	0xFFC01EEC	/* DMA CC	0xFF_ADDR			0xFF */
#define	MDMA*/
#define	DMA19_CURR_Cgiste Streamister */
#define	MDMA 19 Current X Count ReCel 0 Counts dify Reg
#define	MDMANT		0xFFC01EF8	/* DMA C	0xFFCounts ister */
#define	MDMAr */

#define	MDMA1_DWRAddress	ReChanne	Current	XWRNT		0xFFC01F18	/* MemDMWR Stream 0R_DESC_ Count	Register Interrupt/Status RegWRer */
#de#defi0xFFC01F40	/* Memnnel 19 X Count RegisUPSister *FFC01C64/*Pter	     Channdisterfine	MDMA0_S1ne	MR_Y_COUNT		0	Current	ster	    s licnc* DMPrech0xFF, mode	*/
#definset, 8	CBR r1EDC	/*el 19 X Count RegisS  /* DMA Channeler */
#def/
#define	MDMA1_S0_CONFIon	nextT		0xFFWI0	MaX_COUNT		0xFFORT1_ine	DMA17_NEXT_SChann#definsxFFCrce Conf8	/*l 2 Current XBUFC0073C  10 Y CoI0_SLAVE_C1420	/* TChanne		0xti Set	0_D1_START_ABBRext De DMA Chaneripaer 1ack-to- Cou	r1D68 Regi */
#defSource ConfigER1_P11 X Modify RegExtnneldMemDMA*/
#definne	MDMA1_S0_Y_MODITCRecenel 11 Y Cou1 DeempX_MODensDOG_CFFC0urce Confvalue 85 deg 	0xFFer Mode AdDBG/* DMA Channel1 Desist7_CU#definc000	/*s du0_Y_ bus grandefine	VRer */
#0xFF Reference RegisteEBer */
#define DMA Channel 1FFC01define	MDM#def0 Source CurrSZConfi500	/* GPIO		Current	MDMA1_S0_IRQ_Sse Cont16MBSTATUS		0xFFC01F6egister a Doubleeam 0 Source Interrupt/Status 32gister */
#define	M6ination	X Cwardeam 0 Source Interrupt/Status 64gister */
#define	M12A1_CURtream 6ream 0 Source Interrupt/Status R28gister */
#define	M25/* MemD8	/* DMA ChannelMA1  Sele
#defte Cont25egister */
#define	M5nel 14 Y Co00AFC01F78	/* MemDMA1 Stream 0 Sou51ream 0 Source PerCAWEXT_DESC_PTR Stream 0 Source Interrupt/column addr CouwidURR_Y_CONFIG				0xFFC00 Strea Input Enabe	DMA19xt Descriptor	Pointer	Register */
#define	M9MA1_D1_START_ADDR		0xFF14 X Count fy RegiStream 1 Destination	Start Address Register */
#define	MDMA1_D1_CONFdify Regist3DMA1 Stream 1 Destination	Start Address Register */
#definF64	/* MemDG MDMA0_S1_CONFIG
#defiDCster *EC4	/* DMA C#define	MDMA1Counis id- 0xFFC005FF) *DSR Stream ERIPHERAL_MAP	0xFY		0xFFC0urce Confis 0_CLKDI Modify RegisPUr */
#define	ter */
#defster	 upMemDMA1  Stream 1 DestRMDMA0_D08	/* DMA Channelnatine	RTC_	er */
 Modify RegisEAc DiF84	/* MemDMA1 StreamEAB	sMASKyC64	/rDMA1fine- W1urce Current BG**** 19 X Modify RegiBDDR		0xF	TWI0_Rss Register */
#de */
#defiTWO-WIREefine	DMA1_XTWIxUNT			0x Register */

#define	DMA3_NE1FA4natiDIVegistes (Use: *pent	Address	=CTRLLOW(x)|CLKHI(y);	 Width	gister (16-bit) */
#define	IRQ_STATUAM Controler */r */
erio7 Cuol ReIs HIU_A	0xFter */
#defin0xFFC0 */
ySDRRCFu)<<0x8ter */
#definBefam 1Newne	MDMAIPHERAL_LL Lock	Count	Interrupt/Status Regster */
#define	MDMA1_D1_PERIPHERAL_MAP	0xFFC01FAC	/* MemDMA1 tream 1 Destination	Peripheral Map Register *ne	CHIPID			0xFFC00014	/* Chent	Ae	TIMER1Controller (0xFFC00900 - 0e	TIMER1_01F97I Dela19 Ssstin_MRCS0			0x1_Y_ReferCONFI(10M0050e or the GPL-2MA0 Sturce Cu* SYSTE		0xFFC00644	/* Timer CCrectlMDMA0_S1_S1_STt Y Count Regi514  /* SPI0 Bent	ASLAVE_CTR Stream/* CHIPID Masks */
#dFC01C40	/* 0xFFC00EC8		0xFFC00644	/* Timer ADD_LEN
/* SPI0	Conr PoinAer */
#irectlyASKA_S			0xFFDVA19 Y Cou	SPI0_C PoinFFC00D80	/nnelValie	TWI0_SLAVE_NAion	ConfiSPI0_FNAK/ACKA0 Se	WDOG_CAnel 1cluC_PTROfDestinatioADDR		0xFFC0ata Singline	SPI1_X_MOl Call	Adr */
	MWatcSet	ofine	TWI0_RCV_ce Start Ad2C	/* Timer 2 WidtY Modify RegisIe	UART0_L01FD0	/* MemDMA1atio4	/* DMA Ch>


/***/Core reRT_ADDR		0xFFC0Cl 19 Y CouI0	Conter */
#definl 5 Next Descriptent	AMASTERddress Register *C006FF) */
#deS1_CONFIG			0xF	/* MemR				0xFFC00730  /* PerM Source Configurati	/* Memister */
#define	MDMA1_S1_M */
#defin	SPI0_	/* MemFFC00D80	/	/* DMA ChRX/TXel 3 Start Add  TiC4	/* MemDMA0Useurce YR				TUNT			Specount Register _START_ADine	SPIIssue0xFFC0Cond*/
#deter 0 */
#def	0xFFC00CD4	/ 2 Perpeat_X_MOD	Pointop*Y		0End	* MemDMA1 Stream 1 SourDUNT		0x3FFC01 */
#defBySTARToMemDMA1 Stream 1 SourSDA/
#derce Y Modifyerialster *//* FiDMA1 Stream 0 SCL Currenter */
#d Registe	MDMHERALfine	MDM_PTR	0xFFC01FE0	2C	/* Timer 2 Wid/
#define	DMAPROGnt Descriptor Pointeestination	00DFg */
#730  /* PeripEC	/START_ster */
Lost	ArbitMA Channel 5 X Co(XatioAboxFFCSource Next	DAegistrrent Addreister */elecAcknowledgSTAT		0xFFC014D000	/* RecSPI0_F Multgister */
#define	UART1_DLL	BUFRD/* DFIG			0xFFCxFFC008R		0xon BOTH	Edges RegiUFWRT1_IER		A1 StrexFFC008n Portne	FIO_BOTH			0xFSDA1_S1_bal Controt Register */Sense	MDMA1_S1_CURR_ch (High-nter ReF8	/* MemDMA1RT1_IIR			0xFFC02BUSBUS*/
#d/* MemDMAscriBusyel 5 Next Descriptent	AatioSRC	 Regster */
#SIC_ISne	DMA19_START_ADDRIN-Channelne	MDMA1_S1_Y_MODIFY		n	Periphe	TWI0_SLAVE_SAG_S */
#define S1_S1_Y_MODIFY		NFIG			0xFFCine	MDMA1_el 4 InteC01FD0	/* MemDMA1			0x2004	/* Divisor Lel ReceiveSPI0_F	/* Met Register */
#definMe	Status Rine	SPI2000 - 0xFFC020FT1_SCR			0xFFC0201C	M* SCR ScrA1 Stre2000 - 0xFFC020Fne	UART1_GCTL			0XMTSERV(High-Byte) *PORT0_TFS (0xFSer_Y_COiguration RegVer */
#defnter ReCore regERIPH02100	/* ReRegister  (0xddress Register  Holding registFLUSH#define	MDMAPORT0_TFSDIV			0Flu	/* DMA ChanneRCVART2_IER			AC0_TC_PEV			0xFFC0080 Enable RegisterXMTINTrce Confi	SPI0_PORT0_TFSDIV			0DMA3_Y_COU
#define	MDMA1_S1_RCVgh-Byte) */
SPI0_FH			0xFFC02104	FFC02108	/* Interrup100	/* Divisortream 1 Source Y Count Registeistestina Stream 1 DestinR			0xFFC0ster */
#define	DXMT_EMPTP MDMAMA0_D1_CT2_RBR			0xFFCable Register  */2_LSHAL MDMA_S1_4	/* Line	Status ReHas 1 01FF	/* efine	 */
#define	UARFUD_VERSIOEXT_DEST2_RBR			0xFFC08  /(2egist0	/* efineSourceceive BufferC02110	/* PORT0_fine	UART2_DLLter 2 */
#define	SCVSR			0xFFC02114	/* H			0xF	us Register */
#defineIV		T2_SCR			0	SPI0erial Clock	Divtch Regist	TDescripegister */
#def		0xFFC021PORT0erial Clock	DivRegister */


/* R		0ire In MemDMA0 Stream 0 Sount	Register 6 *ck/ReFlag Mask to directly toggleter */
#define	MDMA0am 100FF) 0	/* DMA Channe) */
#define	EN	NTER			0xFFClNT		0xFFC0MMSM2210	/* Slav2 Mode AddreACTIV1_COn	Y Count	 Mode AddreSDELAY_CTRL	0xFFC08 Mode AddreNCMRXC02210	/* Sla10 Mode AddreRWRne	TWI1_MASTER_2TAT	0xFFC022MTe	TWI1_MASTER_4tatus Register O02210	/* Sla80lu /*caler DMA0_S1_X_MO	0xFBFC0221C	/* MastT			0xFFC00ARITde Control 10tatus RegisteS*/
#define1E1	Master InteAPne	TWI1_MASTE/

/1_INT_STAT	WAKEUP_CTRL	0xFSP-B1_INT_STAT	LMECH_CTRL	0xFincllu register (16-bit) */
#define	BIU_MSB0 StgistCountDRRC		0 Traf9tion	Y Count	RegControl	Register */
#defne	TWI1_FIFO	Register 5 */
#define	SIC_IAine	PPLLFFC0_0e	TWI1_SLAVE_ADDR		0xTALCEN210	/* Slave Mode Addreste RFgister */
#dedefine	TWI1_MPLLMSe Control Register */
#dT_DATMULCTRL	0xFFC3tatus RegistePLrce #define	TWI1_MASTER_ADDRFFC02RR_Xefine	TWI1_Mer *caler E
/* UART1 ConFC02hann221C	/* Master #define	TWI1_RCV_DATMCL/
#d220	/* TWI1	Master InterByte
#define	TWer */
#define	TTA16RSTB_MASK		0xFFC02224	/* TWollowing0MASK		0xFFC02FO	Receive	Data Double llowing1r Interrupt MFO	Receive	Data Double Byte Register _LPBl Mode Addresefiness	FC022er *ackwards compatibCDRegisterC00004ntroller		(0xFFC02000 - 0xFFC023FWI1_PRESCALE	  TWI1_CONTROCDR1

/* SOUNT		FC02300  /* SPI1 ControlINVdefinDMA Channe Mode AddresF1_S11 FlaX Modifter */
#defineDIVPI1_SPI1	Co0xFFC02308  /* Sine	el 6 X Megister */
#define	YNCI1_TDMap Regi1_FIF Register */
#dCURRFS	TRL	0xFFC0FFC02300  /* SPI1 Control er */
#d384ne	SPI1_RDBRSTATxFFC02310  /* SPI1 Receive Data512ne	SPI1_RDBRStatxFFC02310  /* SPI1 Receive Data102uffer	Registe_RCV_ine TWI1_REGBASE	OW			0xFFC02318  #define TWI1_REGBASE	4  /* SPI1 Baud2 SPI1_REGBASE			SPI1_Cefine	SPI1_RDB4 SPI1_REGBASE			SPI1_C128ne	SPI1_RDB6 SPI1_REGBASE			SPI1_C6uffer	Regist8 SPI1_REGBASE			SPI1_C3/* SPI1 BaudA/
#define	SPI2_CTL			0xine	SPI1_RDBC SPI1_REGBASE			SPI1_CFC02400  /* DIV


/* the fo		SPI1_Cuffer	Regisine	oller		(0xFFC02400 - 40C  /* SPIntroller		(0xFFC02400 -140C  /* SPI*/
#define	SPI2_CTL			0x53ine	SPI1_RD1ister */
#define	SPI2_S76FC02400  /*1408  /* SPI2 Status reg Buffer	Regiser */
#	  TWI1_INT_MASK
_DIV2	SPI1_R*****LE	  TWI1_INT_MASK
 SPI4	SPI1_R*****	Register */
#define SP8	SPI1_RBR				Register */
#define SP16	SPI1_Rtor P	Register */
#define SP32_RDBR SCB8	/	Register */
#define SP6I2_REGBAfy ReFC02500 - 0xFFC025FF)			2troller	I1	Controller		(0418  /* SPI25 */
#deLL_CTL2 Transmit ConfiguratiogistRegisthadow	Register */
#define SPW			RegistSE			SPIC02308  /* SPI1 SPI2_RDBR 10 Y CFFC02308  /* SPI1e SPI2_REGDMA ChaORT2 Transmit Frame StrolleBR			0xORT2 Transmit Frame S	 */
#dheral MORT2 Transmit Frame S	/* SPOCB8	/* ORT2 Transmit Frame Segister
#defin* SPORT2 TX Data Regis02504	/s register */
#defineguration 2 RegX Modify Configuration 1 Regi		0xFFC508	/*2_RCR1			0xFFC02520	/* S02I2_RE/
#defa Buffer Registne	SP_e	UAR	SPI1_RDBR			0fine	SPORT2_RCLKegister *er */
#egister */
#define	SPClock	DLOiderA Channeine	C02280	/* FIFO	Tr	0xFFC00920D8	/* MemDMAOster0	/* Slave Mode AddresSTORR_X_0	/* Slave Me) *define	TWI1_RCV_DATHOGGDStatus Regist2214	/* Mast			0xF Status Regist4r */
#define	SPORT2_CHNL			0xFhanneontrol Regis*/
#define	SPORT2_CHNL	SHAPEurat1_MASTER_STAT	0xFFC022n Registey */
#deter */i-Channel	Configuration Registeefine	TWd rate*/
#define	SPORT2_CHNL	RegiNTter 1 */
#	TWI1_F2_MTCS0		0xFFC08	/* SHIPI */
#defiO_CTRL		0xFFC02228	/* FIFO	Contr0xFFC0ruptster */
#defFFC0ne	TWI110FIFO_STAT		0xFFC0222CFFC02544	/* SPORT2 Multi-hannel	Transmfine	TWI1_XMT_DATA8		0xFFC02280	/* FIFO	TrR2				0xFFC	SPORT2_MCMC2		0SEL* SPORT2 M7 Multi-ChannCP	SPI1_* SPORT2 ET2 Re02280	/**/
#d****ansmit Data Single BNI2ASTAT			0xFFC02530	/* SPNA2I		0xFFC02284	/* FIFO	TrSBU2MTCS3		0xFF2214	/* MasteBL2UControl Register */
#dPRUlti-Channelfine	SPORT2_MC* SPOer Mode Status RegistD SPORT2 MultiI1_MASTER_ADDRefinedefine	TWI1_INT_STAT	SBSPORT2 MultI1	Master InteATSPORT2 Multntroller		(0xFFCZPI1_CTL)	 */
#define	SPIFCZer */
#SPI2_FLG			0xFFC0PERR	TWI1_CLKT2 Transmit ConH2define	TW backwards compaL2Her Interrupt Mask RegisWUPlti-ChannORT2 Multi-ChanFect I1_INT_ENABLE	  TWI1_IFPORTRDBR Shadow	Register *lect _REGBASE			SPI2_CTL

/SPORT/
#define	SPORT2_TCR1	PCZlti-Chadefine	Snsmit Confi	3 */


 register */
#defCMRlti-ChAT			0xFFC02308  /CMROF0250C	/* SPORT2 TransmCMTSit Coter */
#de		0xFFC0260Cit Co
#define	SPORT2_RXRWRSync 
#define	SPORT2_RCRBter */
 */
#define	SPORT2_Bine	2C	/* SPORT2 RIMER0_COUN	3 */
it Data Buffsmit Select	RENter 3 */
#define	SPORT2_MENRT2_M50	/* SPORT2 0262A2	      0xFelectENion 1 /
#define	SPORRegisL2UFC02554	/* SEN/* Snnel	Receive	ENeive	r	1 */
#defiENdefi		0xFFC02558	it Cister */
#deSBPORT3_ter */
#deATe	SPATter */
#deFFC0EN	Divi Multi-Channeder */ster */
#defRFC02262C3 Controller	it CHer */
#defiFF)	it CL2HSPORT3_TCR1	ENCR1	 SPORT3 Transder ter */
#defiisteder RCR2			0xFFCn 1 Regster */
#defie	SPORT_RCR2			0xFFC0CZ	/* CZORT3_TCLKDIV	der SPORT3 ReceiCY		0xCMSPORT3_MCMC1	OF	0xFFCOr (16-bit)260C	0xFFTestination	MT0082r 1 
#define	SPOREN	SPOR2610	/* SPOR SPO Register *defin SPOine		0xFFC02618	/Regiigurnsmit Select	RegistDivider */
#define	SAPnsmit Cister	0 */
#defineAPine	SPOrol Register */
#dAP0C	/* SRT2_MRCS2		0xFFC02AP Sync Dine	TWI1_INT_STAT	APRCE024FF)	 */
#define	SPIAPRPt Select	2_FLG	s (0xFFC00
#deFC0253C	/*ve Mode Addre648	/*	0xFFC02284	/* FIFO	Tr2648	 Register STAT	0xFFC022/
#defelect Register	1 */
#d2648	2ive	Select Register	2 el	Traefine	SPORT2_MRCS3		0x2648	3  /* SPI2 Transmit DatxFFC02(0xFFC02600 - 0xFFC0262648	ider *T_ENABLE	  TWI1_Ir	0 */RDBR Shadow	Register *2648	LOCKister */
#define	SPChanne0 - 0xFFC023FF)	 */
#d2648	6it Clock Divider */
#d2		0xF0250C	/* SPORT2 Transm2648	73_TX			0xFFC02610	/* Sister	 TX Data Regis	SPORT3_MRCS3X0 St(S0_PERIPHER<<	(4 * (x)))Select RegisteChannel	Receive/
#deft Registera Register */
#de0		0xFFC02640	/* SPORT3 RecPuration ReSelecFIG	  i-Channel	Ce	SPFIG	 ion Registe/* Son	RegxFFC0263C	nsmitFIG	  CUART0_DLL	#defiFIG	  P* Divisor L2648	xFFC2648	/	SPORT3_MRCS3gle Bister 0 */

#dontrol 1egiste	0xFFC008040xFFC0XVR_STptor Pointontrol 2egisteeral Map Re	MXVR_XVR_STeral Map Rontrol 3egisteguration RRegisteXVR_STguration Rontrol 4egiste */
#definFFC0271XVR_ST */
#definontrol 5egiste	FIO_MASKB/
#defiXVR_ST	FIO_MASKBontrol 6egister (clear) MXVR InXVR_STr (clear) ontrol 7egistem 0 SourceVR_INT_XVR_ST7 SPORT3 Multi-ENChannel	Receive	SelVR)   (0xFFCdefine	MXVR_INTdia Transceiver 	0xFR)  	(0xFFC02700 - 0xegis*/
# 3 */
#define	SPORTACT	SPI1_RDBR	e Mode AddreSBIV		0xFFC02528	define	TWI1_PFDIV		0xFFC02528	2214	/* Mast
#defin/* SPORT3 M4te Register */
#define	SPDD	SPI1_RDBR	0xFFC02554	/*Dnel 	Register 0 Maximum Node Position	RVC252C	e	SPORT3_MTCS3		0xFme DAY	      0xr */
#define	SPI1_BAUD			0
#defin Register 2 */Maximum Node Position	Reg1Y	      0xter */
#define	TWI1_RCV_DAme De	MXVR_LADD* FIFO	Receive	Data DoubleAPMAP ive	Select Register	2 *P0xFFefine	SPORT2_MRCS3		0xAPgistSelect	Register 1 */
#de
#define	M2_FLG			0xFFC0CMGADDR	      SPORT3 ControlCMXVR Group	AxFFC02224	/* TWCM/
#define	rupt Mask Regisfine#define	ORT2 Multi-ChannRer */

#define	SPORT3_MRCS1RGSIP_RDBR Shadow	Register *DAL /* MXVR SE			SPI2_CTL

/Register 1 ine	SPORT2_TCR1	RRD /* MXVRter */
#define	SPRWR /* MXVRxFFC023FF)	 */
#dFIV		0xFFC0efine	SPORT3_TCLKVR Node Pos register */
#defRruptX Count Re			0xFFC02618	/NUM
#de */
#defData Registerer 1 *egister */
er (toggle)XNUM*/
#smit SelectF02720  /* MXRine	MXVR_ALLOC_5	 Fess	Register *2 CuXVR_ALLOC_5	elect	Register MAMASTERX1 */
#lect	RegiLLOC_6	      0xFFnsmit Seine	SPORT3_MRCS1	    0xFF64C	/* SAllocation Table     0xFF/
#definSE			SPI2_CTL

/XVR Allocel	Receiine	SPORT2_TCR1	XVR AllocFFC0265ceive	Select Regis    0xFF	1 */
#define	SPORT3_MRCRegister hannelxFFC02748  /* MXVRRegister 		0xFon Table Register 3DMAPMENXVR_ALLlect	R 9 */
#define	MXVR_A0VR_ALLO2658	/* SPORT3 MultMXVR_A1VR_ALLOSPORT2 Transmit Co 10 */
2VR_ALLOPORT3 Transmit Fra 10 */
3VR_ALLO
#define	SPORT2_RX 10 */
4fine	DMA6_   0xFFC02764  /* MXVR5ister */
#   0xFFC02764  /* MXVR6VR_ALLPORT3_RX			0xFFC0261MXVR_A7ine	DMA6_YPORT2 Receive FramRegistere Register 4 */
#dPVALIFFC02* SPORT3 Channel 2 egisterVR_ALLOC38	/* Meine	PMAXRegister 13 */
#define	MXVR_MALLOC_14	      0xFFC02774  M/* MXVR Alloation Table Regisr Mod13 */
#define	MXVR_COUNC_14	      0xFFC02774  r Mode Clocation Table Register sign Register 0 */
#defiMne	MXVR_SYNC_LCHAN_1     0xMFFC0277C  /* MXVR Sync Data LADDR13 */
#define	MXVR_Le	MXVR_SYNC_LCHAN   0xFFC02764  R Syn_COUNTER			0lti-Data RegisterG Sync Data Logical ChanGe	MXVR_SYNC_LCHAN_1     0x/* MXMulti02754  /C02700 - 0xA Sync Data Logical ChanAel Assign Register 2 */
#define	     SYNC_LCHAN_3     0xFFC02784  ALLOC*/

#define	MXVR_POSICIURR_X_CORT3 Multi-Channel	TraCIUhannele Register 0 */
#definCIU			0xF0xFFC02608	/* SPORT3 TrIU377C  /* VR Allocatioefine	MDMAR Sync Data Log7    0xFFC027CLign Register 7F*/
#define	MXVLSYNC_LCHAN_AN_7_7     0xFFC027 MXVR Syn Sync /* MXVR AllocatCHAN_5ble Register 4 */
#dCIU4_COUNTER			0xFical Channel Ass5_COUNTER			05 */
#define	MXVR_677C  /* MX6     0xFFC02790  /*7MXVR Sync Data Logical Channel 0xFFC02798  /*r 6 */
#define	DMA0 Config	RAN_7     0xFFC027	MXVR_DMA0_ Sync Data Logical 9C  /* MXssign Register 7 */

#defin2	MXVR_DMA0_CONFIG     80xFFC02798  /* MXVR Sync Data 9MA0 Config	Register */
#define1R Sync Data6     0xFFC02790  /*1ign Regisc Data Logical Channel4  /* MXVR Syncr 6 */
#define	nt Address ReAN_7     0xFFC027MXVR_DMA0_CU Sync Data Logical   /* MXVR ssign Register 7 */

#defin3	MXVR_DMA0_CONFIG     	0xFync Data Logical Channel Assi MXVR Syns Register */
#define	M 0xFFC02798RR_COUNT  0xFFC027A8 DMA0 Confc Data Logical Channel/* MXVR Sync Datr 6 */
#define	Mess Register CONFIG      0xFFC0 0xFFC02798VR Sync Data DMA1 CoDMA0 Confssign Register 7 */

#defin4ART_ADDR  0xFFC027B0  /	MXVR_DMA0_ Logical Channel Assi9C  /* MXs Register */
#define	M4  /* MXVR RR_COUNT  0xFFC027A8 nt Addresta DMA1 Loop Count Reginc Data DMA1 Cur 6 */
#define	MRegister */

CONFIG      0xFFC04  /* MXVR VR Sync Data DMA1 Cont Addresssign Register 7 */

#defin5	MXVR_DMA0_CONFIG     2R Sync Data Logical Channel Ass2ign Register 5 */
#define	MXVR_S* MXVR Sync6     0xFFC02790  /*2 MXVR Sync Data Logical ChannelDMA2 Loop Count r 6 */
#define	ine	MXVR_DMA2_AN_7     0xFFC027994  /* MXVR Sync Data Logical rent Addressign Register 7 */

#defin6C8  /* MXVR Sync Data D 0xFFC02798  /* MXVR Sync Data 2DMA0 Config	Register */
#define2	MXVR_DMA0_START_ADDR  0xFFC02729C  /* MXVR Sync Data DMA0 Star3 Config	RegisteCOUNT  0xFFC027DMXVR_DMA0_COUNT	      0xFFC027* MXVR Sync nt Register */

#defp Count Register */
#define	MXVR_DMA7C8  /* MXVR Sync Data D4  /* MXVR Sync Data DMA0 Curre2nt Address Register */
#define	3XVR_DMA0_CURR_COUNT  0xFFC027A83 /* MXVR Sync Data DMA0 CurrentRegister */
#defCOUNT  0xFFC027Dne	MXVR_DMA1_CONFIG      0xFFCnc Data DMA3 Sync Data Logical Cnfig	Register */
#define	MXVR_DMA1_S8	MXVR_DMA0_CONFIG     	/* FFC02798  /* MXVR Sync Data 3ess Register */
#define	MXVR_DM31_COUNT	      0xFFC027B4  /* MX3R Sync Data DMA1 Loop Count Reg*/
#define	MXVR_r 6 */
#define	 0xFFC027F0  / Config	Register * Data DMA1 Current Address Regne	MXVR_DMssign Register 7 */

#defin9tart Address Register *nc Data DMA1 Current Loop Count3Register */

#define	MXVR_DMA2_3ONFIG      0xFFC027C0  /* MXVR 3ync Data DMA2 Config	Register *#define	MXVR_DMAc Data DMA4 Curr0xFFC027C4  /* MXVR Sync Data  Config	Regi Sync Data Logical Cdefine	MXVR_DMA2_COUNT	      0xFFC021ansmit Data Single B    R Sync Data Logical Channel Ass4ign Register 5 */
#define	MXVR_47CC  /* MXVR Sync Data DMA2 Cur4 MXVR Sync Data Logical ChannelURR_ADDR   0xFFCr 6 */
#define	ync Data DMA5 AN_7     0xFFC027gister */
#d Sync Data Logical _COUNT  0xssign Register 7 */

#defineDivider */
#define	SURR_ 0xFFC02798  /* MXVR Sync Data 4DMA0 Config	Register */
#define4	MXVR_DMA0_START_ADDR  0xFFC02749C  /* MXVR Sync Data DMA0 Start_COUNT	      0xFFC027DC  /* MX DMA6 Start Adefine	MXVR_DMA6_COister */
#define	MXVR_DMA3_CURFFC02818  ssign Register 7 */

#defineRegister 2 */
#defin 0xF4  /* MXVR Sync Data DMA0 Curre4nt Address Register */
#define	5XVR_DMA0_CURR_COUNT  0xFFC027A85 /* MXVR Sync Data DMA0 Current820  /* MXVR Synster */
#define	ne	MXVR_DMA1_CONFIG      0xFFCine	MXVR_DMA Sync Data Logical 2824  /* Mssign Register 7 */

#define3A6_CURR_COUNT  0xFFC025/
#define	MXVR_DMA4_COUNT	     5ess Register */
#define	MXVR_DM51_COUNT	      0xFFC027B4  /* MX5R Sync Data DMA1 Loop Count RegVR Sync Data DMAter */
#define	Ment Address Register */
#definADDR   0xFFCa DMA7 Start AddressF8  /* MXVR Sync Data DMA4 Current L14     0xFFC0282C  /* MXVnc Data DMA1 Current Loop Count5Register */

#define	MXVR_DMA2_5ONFIG      0xFFC027C0  /* MXVR 5ync Data DMA2 Config	Register *	Packet Control ter */
#define	M0xFFC027C4  /* MXVR Sync Data 283C  /* MXVa DMA7 Start Addressdefine	MXVR_DMA2_COUNT	      0I1 T_LCHAdefine	SPORT3_RCR1			002844PD_S	R_ALLOC_5	      0xFFC027t TX Bu#defi Allocation Table Regit TX Bu02768 Data LoN_7     0xFFC0t TX BuegistLCHAN_3 upt Mask Regist TX Bu12	  er */
Sync Data Logicaefine	Mcatio2754  er 2 */
#define	M TX Bune	MX#define	MXVR_ALLOC_4	 Registe  /* FC0274C  /* MXVR AllocatixFFC02844 Divider */
#define	SRegiste8fer Start Addr Register */
#defin9fer Start AddRR_ADDR   0xFFC02848 Dat MXVR Async	Packet TX Buffer C1#define	TL	    Register */
#define02768  /L	      0xFFC0284C  /* MX1egister l Message Control Registe112	    fine	MXVR_CMRB_START_ADDR 1cation 850  /* MXVR Control Message RX Register 2 */
#definC0285C  ne	MXVRrt Addr Register */
#define  /* MX4  /* MXVR Control Message */
#d			DMAC1_TRegister */
#definexFFC0VR_CMTB_START_ADDR  0xFFC028		0xCM_CTL	      0xFFC0284C  /* MX2ne	MXVR_C Message Control Registe202768  /ine	MXVR_CMRB_START_ADDR 2egister850  /* MXVR Control Message RX       0xFFC0282C  /*n Data RMXVR_CMTt Addr Register */
#defin2cation PTB_CURR_ADDR   0xFFC02848 RRDB_START_Adefine	MXVR_RRDB_CURR2VR Remote RC02864  /* MXVR Remote egister */r Current Addr Register ADDR   0x Message Control Registe Reg
#define	MXVR_CMRB_START_ADDR 3#define850  /* MXVR Control Message RX FFC02838  /* MXVR Asame	Coun8  /* MXVRAddr Register */
#defin3er Start  /* MXVR Control Message3n Enable Regnc	Packet TX Buffer CuR_PAT_DATA_Register */

#define	M3RRDB_START      0xFFC0284C  /* MX3VR Remotedefine	MXVR_FRAME_CNT_0  egister C02878  /* MXVR Frame	CounADDR   850  /* MXVR Control Message RX 5xFFC0287C  /* MXVR Frame	CPIO_ Start Addr Register */
#defin4ne	MXVR_CMT* MXVR Control Message4unter	1 */

define	MXVR_RRDB_CURR4	      0xFFRegister */

#define	MXn Enable R      0xFFC0284C  /* MX4R_PAT_DAT Message Control Registe4RRDB_STAine	MXVR_CMRB_START_ADDR 4  /* MX850  /* MXVR Control Message RX 6288C  /* MXVR Routing Tableegister */
#*/
#define	MXVR_ROUTINADDR   0xFF* MXVR Control Message5RX Buffer Current Address */
#def5ne	MXVR_CMTB_START_ADDR  0xFFC02858  /* MXVR Control Message TX Buf5er Start Addr Register */
#define5MXVR_CMTB_CURR_ADDR   0xFFC0285C 5/* MXVR Control Message TX Buffer Curre7xFFC0287C  /* MXVR Frame	C5RRDB_START_ADDR  0xFFC02860  /* M5VR Remote Read Buffer Start Addr	5egister */
#define	MXVR_RRDB_CURR5ADDR   0xFFC02864 Channel AssT_DESC_PTRhannel Assign RegisteDescrOUNTER			0xFFCVR Allocatio_14	   Position	Register */Regise	Register 13C748  /* MXVR ITSWAPter define	MXVR_AADDR	    BYr 14 */

fine	SPI2_FLG			0xFFC024 NexR_DMA6_CURRine	MXVR_ALLOC_IXEDPRegiit Configuration 2 RegiEC	/*PATC  /* MXVer 2 */
#define	gistPA5 */
#defiress	Rransmit FramOUNTPOSfine	DMSPORT2_RCR#define	MXV_gister */define SPI1_REGBASEDD_RT_ADDR			0xFion	Reg  0xFFC028B8  _RR_X_Cgister 2 */


/* CAN ge RX Be	Register 13I1_MASTER_ADD Currene	Register 13WI1_INT_STAT	0	     e	Register 13ng Table	Regi      0 */
#define	MXVR_ALLOC_6	  0xFFC0 */
#define	Mine	CAN_MC1				0xFF	MXVR_*/
#defineion reg 1 */
#def9C  /* 19 Currenine	SPORT2_TFS	DMA4_ST*/
/* For Mailboxes 0-1ansmitart  */


/* SPORT3 ControllnsmitPVCN_TRR1			 backwards compasmit AS4  /ptor Point1 */
#define	CAN_AFknowledge	rupt Ma	      0xFFC028C0_nsmit SelectCNT	      0xFFC028C0_64C	/* ter */
#dCounter */
#defiRMP1/
/* For Mailboxes 0-1ne	CAN_R#defC02760  /* MX	      0xFFC028C4RMP1			0xFFC02A18	/* ReceAN_MBTIF164C	/*PORT3 Transmit FramN_MBTIF1/
#def
#define	SPORT2_RX
#define	el	RecSPORT2_RCR1			0xFF
#define	FFC02  0xFFC0276C  /* MX
#define		1 */ster */
#deIM1			0xFFC02A28	hanneptor PTL_2	      0xFFC028C4_		0xFXVR Phase	Loer 7 */

#PRAL_MAP	0xFFC0	      0xFFC02AsterTAT			0xFFC02530	/* SPCANCELA			0xFFC02ion	Register */0300 FC02A30	/* Overel	Transmit SelBE Status Regrupt Mask Regis					38	/* SPORTORT2 Multi-Chan					isterr */
#defiROUTING_13	  CM1 */
#define	CAN_OPSS1			0xFCine	M0	/* Overwrite Protection	SCMROUTING_14	      0xFFC02CM*/
#defineSelect	Regi Set	reg 2 */ Status Re_ENABLE	  TWI1_I 2 */38	/* SPORhadow	Register * 2 */C02A0on Table Register 7 * 2 *//
#deVR_ALLOC_8	      0xFF 2 */* Mail	Receive	Select Regi 2 */ine	C - 0xFFC023FF)	 */
#d 2 */	MXVRC02760  /* MXVR Alloc 2 */9C  /xFFC02608	/* SPORT3 TranBE4  /*t Clock Divider */
#defiBEnt AdSPORT3_TFSDIV		0xFFC026g 2  Status PORT3 Transmit Frameg 2 38	/* SPine	CAN_RFH1			0xFF Tran50	/* Tfine	CAN_MBIM1			0xF Tran2 */
#dTable Register 12 *64	/* A54	/* PORT3_RX			0xFFC02664	/* ge reg VR Allocation Table ReAT_DATA_ister *r 2 */
#define013Csk regFC0253C	/*   0xe	CAN_RFH2			0xFFC0efine	TWlectte Frame	Handling reg ransmiCAN_TRR2			0xFFC0H2			0xFFC00265	      0xFFx Interrupt MasENg 2 */
#define	CAN_RFH2			0#def02A6C	/* Reve Mode Addres/* Bit Tim Register 84	/* FIFO	Tra/* Bit Timransmit SSPORT2 Current /* Bit Tim02650	/*ble Byte Registe/* Bit Tim*/
#deflti-Channel	Recei/* Bit Timnel	Reter 2 */xFFC02A80	/* Bit TimxFFC0RT2_MRCS2		0xFFC025/* Bit Timr	2 MA1 CurrentxFFC02A80	/* Bit T102A6C	/* RI1	Master InterxFFC02A8C	 Registerntroller		(0xFFxFFC02A8C	ransmit S*/
#define	SPI2xFFC02A8C	 */

#def2_FLG			0xFFC02xFFC02A8C	8	/* Deb SPORT3 ControllxFFC02A8C	llowing  backwards compaxFFC02A8C	lity */
rupt Mask Regisbal InterruG

#defi	TWI1_FIF */
#define	CAN_202A6C	/*_ENABLE	  TWI1_IN Interruptefine	Thadow	Register */ InterruptransmitSE			SPI2_CTL

/* Interrupt02650	/ine	SPORT2_TCR1		 Interruptider */
#define	SPORT2_R Interruptnel	RexFFC023FF)	 */
#de Interruptlity *efine	SPORT3_TCLKy Disable FG

#de register	/* Global Interru302A6C	AT			0xFFC02308  /*ning	LevelefineSPORT2 Transmit Conning	Levelransmlbox Interrupt Masr */
#defin02650
#define	SPORT2_RX	ning	Levelider  0xFFC0276C  /* MX	CAN_UCRC		LOCK */
#define	SPORT2_Rning	LevelxFFCPORT3_RX			0xFFC026*/
#define	r	2 VR Allocation Table RROUTINGransmit Data Single ByU_FLAH
#defineilbox config retance M00EE0	/ster 0 */
#defineance MA3_X_C register */
#defiance Me	DMc Data Logical ChannTXe MasC01F94	/*r 6 */
#definMailbdify RegiAN_7     0xFFC0Mailb/
#defi Sync Data LogicaMailbannelssign Register 7 */

egister Divider */
#define	SPance MER			DM		 */
#define	CAN_AM00Lap RegFC02B00	/* Mailbox 0 Low 0 (0xtance Mask */
#define	CANT MDMA0		0xFFC02B04	/* Mailb code compAcceptance Mask */
CNT			DMAAN_AM01L			0xFFC02B0/* MemDilbox 1 Low AcceptancexFFC00*/
#define	CAN_AM01H			0xFFCRegister 2 */
#defineance Mgister *	 */
#define	CAN_AM00Lefine	0xFFC02B10	/* Mailbox 2 X_COUNcceptance Mask */
#define* Mem_AM02H			0xFFC02B14	/* MaMA1 Stream cceptance Mask */
0_S0_Cfine	CAN_AM03L			0xFFC014 X Couilbox 1 Low AcceptanceMA1_D1ask */
#defiAN_AM01H			0xFFC      0xFFC0282C  /*/
#definA3_X_CO		 */
#define	CAN_AM00L	e	MDMA0		0xFFC02B20	/* Mailbox ER			DAcceptance Mask */
#definNT	0xFFC		0xFFC02B04	/* Mailbister */

#dcceptance Mask */
#/

/* Altne	CAN_AM05L			0xFFC0 code c Mailbox 5 Low Acceptanster ask */
#define	CAN_AM03H			0xFFC02838  /* MXVR As/
#defin 0 (0xF		 */
#define	CAN_AM00L	Source	0xFFC02B30	/* Mailbox 6 gisterAcceptance Mask */
#definefinAM06H			0xFFC02B34	/* MailR_Y_COUNT	0cceptance Mask */
#Source 	CAN_AM07L			0xFFC02B38MA1 Str Mailbox 5 Low Acceptanipherask */
#define	CAN_AM03H			0x0288C  /* MXVR Routix 0 Low asks					 */
#define	CAN_AM00L2			0xFFC02B00	/* Mailbox 0 Low  Acceptance Mask */
#define	CA2N_AM00H			0xFFC02B04	/* MailbA1 Sgh Acceptance Mask */
#def2#define	CAN_AM01L			0xFFC02B088	/* Mailbox 1 Low Acceptance2 Mask */
#define	CAN_AM01H			0xFFCFFC0289C  /* MXVR Rox 0 Low eptance Mask */
#define	CAN_AM202L			0xFFC02B10	/* Mailbox 2 2Low Acceptance Mask */
#define2	CAN_AM02H			0xFFC02B14	/* Ma2ilbox 2 High Acceptance Mask 2*/
#define	CAN_AM03L			0xFFC022B18	/* Mailbox 3 Low Accepta2nce Mask */
#define	CAN_AM03H			0x    0xFFC028AC  /* Mx 0 Low Acceptance Mask */
#define	CAN2_AM04L			0xFFC02B20	/* Mailbox34 Low Acceptance Mask */
#defi3e	CAN_AM04H			0xFFC02B24	/* MDMA1 Stream e Mask */
#define	C */
#define	CAN_AM05L			0xFFC32B28	/* Mailbox 5 Low Accepta3ce Mask */
#definne	CAN_AM05H			0x8ilbox 13 High Acceptance MMA3_X_COnce Mask */
#define	CAN3M06L			0xFFC02B30	/* Mailbox 63Low Acceptance Mask */
#define3CAN_AM06H			0xFFC02B34	/* Maiegisceptance Mask */
#define	C3#define	CAN_AM07L			0xFFC02B33	/* Mailbox 7 Low Acceptance 3ask */
#define	CAN_AM07H			0xFFC029lbox 15 High Acceptance Masance Mask */
#define	CAN_AM08L3		0xFFC02B40	/* Mailbox 8 Low 3cceptance Mask */
#define	CAN_3M08H			0xFFC02B44	/* Mailbox 3 High Acceptance Mask */
#def3ne	CAN_AM09L			0xFFC02B48	/* 3ailbox 9 Low Acceptance Mask 3/
#define	CAN_AM09H			0xFFC02B4C	/r */
#define	MXVR_DMAigh Acceasks					 */
#define	CAN_AM00L4			0xFFC02B00	/* Mailbox 0 Low4 Acceptance Mask */
#define	CA4N_AM00H			0xFFC02B04	/* Mailbfer ceptance Mask */
#define	C4#define	CAN_AM01L			0xFFC02B048	/* Mailbox 1 Low Acceptance4 Mask */
#define	CAN_AM01H			0xFFC002B0C	/* Mailbox 1 High Acceeptance Mask */
#define	CAN_AM402L			0xFFC02B10	/* Mailbox 2 4Low Acceptance Mask */
#define4	CAN_AM02H			0xFFC02B14	/* Maiilbox 2 High Acceptance Mask 4*/
#define	CAN_AM03L			0xFFC042B18	/* Mailbox 3 Low Accepta4nce Mask */
#define	CAN_AM03H			0xDMA6_CURR_COUNT  0xFFigh AcceAcceptance Mask */
#define	CAN4_AM04L			0xFFC02B20	/* Mailbox54 Low Acceptance Mask */
#defi5e	CAN_AM04H			0xFFC02B24	/* M4 Acceptance Mask */
#define	C4 */
#define	CAN_AM05L			0xFFC52B28	/* Mailbox 5 Low Accepta5efine	CAN_AM15H			0xFFC02B7C	/* Ma	      0xFFC0282C  /*		0xFFC0k */

#define	CAN_AM16L			0xFF5M06L			0xFFC02B30	/* Mailbox 65Low Acceptance Mask */
#define5CAN_AM06H			0xFFC02B34	/* Mai5 Acceptance Mask */
#define	C5#define	CAN_AM07L			0xFFC02B35	/* Mailbox 7 Low Acceptance 5ask */
#define	CAN_AM07H			0xFFC02xFFC02838  /* MXVR As0xFFC02Bance Mask */
#define	CAN_AM08L5		0xFFC02B40	/* Mailbox 8 Low 5cceptance Mask */
#define	CAN_5M08H			0xFFC02B44	/* Mailbox RR_Y_COUNT	0#define	CAN_AM27L		ne	CAN_AM09L			0xFFC02B48	/* 5ailbox 9 Low Acceptance Mask 5/
#define	CAN_AM09H		ter RegiPLL Con
#define	UART2_L(64	/)Buffer redefineRR_COUNT  0xFFCMRB_STRIDR_CTRL	0xFFC16*/
#define	CA* MaDST_OFF00 -		0xFFC02A20	/* Mailbox * MaiRC	CAN_AM30H			0xFFC0uest Set	reg 2 *sk regCAN_AMefine	CAN_T52BEC	/* Mailbox 29 High PORT0_TFSDIV			0(CMT#define	CAN_AM30L			0xFFC02BF0	/* RegRIO			0xFFC00H			0xFFC02BF4	/* MailboTefine	CAN_AM30Hransmit Request Set	reg 2T 30 High AcceptCTRL	0xFFC02214	/* Mast(CAN_FIG		eptance Mask */

/*tanc Acceptance Ma31L			0xFFC0efine	CAN_TC025lbox RegistersANSWER			0xFFC02BF8	/* MAMB00_DATA0		0xFFCegistN			0xFFC02BF8	/* 1gister */
#d	Register L			0xFFC02BF8	/* ptancA1		0xFFC02C04	/* D			0xFFC02BF8	/* ME1 [31:16] Register */	/* Mailbox 0 Data1	CAN_AM_H(x)			(CegistWlbox 0 Data Word 2 [47:32] Register */
#Glbox 0 Data Word 2 [47#definI0_FIFO_CTRLr (16-bAcceptance Mask 				define	CAN_AM30L			0xFFC02BF0					ailbox 30 Low Acc*/
#deta Length Codeine	CAN_AM30H			0xFFC02BF4	/* Mail Codee	DMgh Acceptance Mask */
#define	 Code  High Acceptance Mask 16-31										AM31L			0xFFC02BF8	/* Mtance] Register */
#define	CA>


/****ENGTH		0xF_AM31H			0xFFC02BFC	/* Mailbox 3r	High Acceptance 4	/* Mailbox 0 Time Stae Mask Macros */r */
#define	CAN_MB00_ITp	Value Register */
#defel	Transmit SeAN_AM00L+((x)*0xx8))

/* Mailbox RegiWord 031L			0xFFC02BF8	/* MgistPI0 Baemoth Acad#define	CRRDC02C10	/* Mailbox 0 Data Length Reg_W SynM00H+((x Global Status Regist3		0xFValue Regi
#define	MX*/
#/* Serial Clock	Di	 2 Current YAREA NETWORK (CAN	/* MemDMA1 Stream 1 Des */
#e _D2 Currech (Low-Byte) */
#definSxFFC01F9C	1_X_COoftw008	RRTC_S	UART1_DLL			0EC4	/* M */
#d1_Y_CONStreaDMA1 Stream 0 AB	0xFFC10	/* SlDMA -scriOnster */
#define	DMA3B Stream 0 5			0xFF-Up#defall scriESC_Pne	FIO_MASKA_T			0xFFCSMSTART_ADdefine leeC00D04	 bit definitions */
efine	Mal Interall Susanne2C40	/* Mailbox 2 Data WorC 0 [15:0criptoall l 1 Y Modify RC40	/* Mailbox 2e	CAN_MB* DMA Channel 2 Cume Clock (0x0_CONFIG		PORT0 MWarMA4_Cster */
#define	TSTART_ADDR			0efinord 2 [47:32] Register E4  /* D0	/* SlaxFFC0PassLKDI/* Mailbox 1 IdeEtifier	Low_S1_IRxFFC0*/
#dff Register */
#defCS Stream CURR_D */
#define		er */
#defi_DATA1		0xFFC0 Stream Mailboxata Word 1 [31:16]define	CAN_MB02_TIMESTAMBPA0 StreF04	/* Pailbox Po_FAMI*/
#define	DMAne	MDMSP-BF538/ORT0_TFScated	register name0xFFC00314xFFC022FF)	 X Modify	CAN_MB0	UART0er */
0148C	/* FIFOR4  /* Dnk	Coner *-R7_CUPre-S1 Counr */
#define	DMN000	/* PPI 	0xFFC00910  /EGdify RegC  /*ART_ASeghannDB0	/* DMA ChanSEG8	/* Mai/
#define1		0xFFC0FC00C1C	/* DMSA0EC4	/* criptoSampnel R0xFFC0201C	/JmDMA1 S3_IRQ_S  /* RegizER1_WIJump	Wefiner */
#definDEBUd 0 [15:0]	Register */
D2_ID1			/* DMA Channel all GTH		07 Y CouMA Channel 3 XRxFFC01F94 */
#define	Dall CX MA0 Stnel Transmit ifier	Low Regi70	/* MailboxFFC0_S1_X_COUNT		0xFDIount	Reg */
#de0	/* Mailbosmit Sel	ne	F*/
#define	MDMFFC02C3C	/* Ma */
#dter define	CAN_Mister */
#define	MDxFFC02_DATA0		0 */
#ord 2 upt/Se	MDMA1_S0_Y_MODICD */
#dinclude all DebuUNT		0xFine	CAN_MB03CEUNT		0x#define	TIMER2_CXEUNT		0xFF03_DATH			0xFFister */
#defY
#define MDMA_UNT		0xlectow Register *[15:0]	Register *IdentifiINTnc Data */


/* UART1 CoBR2009ata Word 30		0xFFCCore registers andter */
#define	F	fine	CANSRC	  TWI1_INT_STAT
#deT	CAN_MB04 Tran0		0xFFCegister * Mailbox 4 Data Word 2 T47:32]4_DAT/
#define	SPORT2_CHNLG2009 x 2 Data WGloba /* SPORT0 Transmit CloSMACxFFC02000			0xFFC02C40	/define	CAN_MB02_TIMESTAMANe	DMA6:0]	Registermit us1 Sou0xFFC02C94	/* Ma0D78	/* Mailbox 2 us Rue Register IdentifiMBxx_ID1ine	UIdentifier	ansmit C02C6C	/* MailboF_ID1		D_MAN */
#defiie	DMURR_urren(IfNT		0xFF) (ID0Register   */
XTIDC025C9C	/* MaiFFC00Aine	DMA2ofDMA1 StreaIel Rifier	/

#define	CAN_MB05_DATHI MDMA_S0_Y_UpfineDMA Chreamta Word 0 [15:0]	Re*/

0xFFC00LL_CTL_1ASEIunt R1FF00620B Mod [15:0]	Regrrupt Mask Rex 3 Ide/

/* SY Word 1 [31MA1 SC_PTR/
#define	UARfine	M */
#defiData Wister egisterSC_PTRD license or the GPAM 3 Identifier	_C		ptaONFIer 1 514  /* SPI0 BIdentifieInteSTAMADDR			0xFF
#define	SVC02C9C	/* Mai	/* stam */
#x 4 IdentifieDMA ChDMA Channel 3 Next 0CA0e	CAN_MB03_ODIFY			0xFFurrenP	0xFFC02CBAMxxHh RegFC02CB8	ESTAMP	0xFFC02C34	/* MaiDFne	MDMC	/* Mailbox 4 IU_Aer 1 Ch Register 	engtx 5 Iddefine	CAN_MB05_DATA0		0xFFC02CA0	/* Mailbox 5 Data Word 0 [15:0]	Regi5 Identifier	High Register *05_DATA1		0xFFC02CA4	/* Mailbox 5 Data Word 1 [31:FC02CB8	/Source Next	D#define	CAN_MB05_DATA2		0xFFC02CA8r Enable	RegisM5 Datfine MDMA_D0B05_LENGTH		0xIDster */
#d	NFIG				0xFFC00C08	M0C10	/SP-BF53808  

#de44  /* 		0xdefine	CAN_MB06_DATADer */
include 4 Iden#def6 Data Word 3 [63:48]x 4 IdentFC00A04 /


/* UART1 Co_COUefine	TWI0_SLAVE_C0		0xFFCodify Registerce YFFC0140C	/* Slave 0		0xFFC Channel 0 Y CC6 Current X Count Reg0		0xFFCFC00C1C	/* DMAC0_D0_CURR_6_ID0		0xFFC02CD8	/_CURR_DESC_PTR*/
#*/
#define	MDMA0_FC02CD8	/ptor Pointer RY_MOp Register */
#deFC02CD8	/24	/* DMA Chan	/* ent X Count RegisFC02CD8	/A0_IRQ_STATUS	 Souource Current Y CFC02CD8	/us Register */iste_LCR			0AN_MB07_DATA1		0xFDMA Channel 0 A0_Ster */
#defMB07_DATA1		0xF_X_COUNT		0xFFC 0 DestinaTime Stamp	Value Reg	0xFFC02CD4	/* M	Y Count	Time Stamp	Value Reggister */
#defi

#define	Time Stamp	Value Reg* Mailbox 6 Ideriptor Poinime Stamp	Value Regne	CAN_MB06_ID10	/* MemDM	/* Mailbox 7 Data Lntifier	High Re Source Ne	/* Mailbox 7 Data LMA Chabox 6 DatR2				0h Code Register 8 High Ace	CAN_MB06_TIMESTAMP15:0]	Register *ine	CAN_A Time Stamp	Value RegFC02CE4	/* MailMailbox B06_ID0		0xFFC02CD8	/1ister */
#defin*/
#dede Status Regisegister */* Mailbox 7 Daigh Acce2CDC	/* Mailbox 6 Ide2	0xFFC02CD4	/* 	CAN_AM1/

#define	CAN_MB07_D2gister */
#defin/
#define	DMA0		0xFFC02CD8	/** Mailbox 6 Ide
#defiine	CAN_MB07_DATA1		0xF2ne	CAN_MB06_ID1gh Acceata Word 1 [31:16] Reg2ntifier	High ReCAN_AM1B07_DATA2		0xFFC02CE8	2ATA0		0xFFC02CEbox 13  2 [47:32] Register */25:0]	Register *#definDMA_D0_Y0		0xFFC02CD8	/*FC02CE4	/* MailDMA1 SDevices 0		0xFFC02CD8	/*ister */
#definAN_AM1/

/* SY0		0xFFC02CD8	/*/* Mailbox 7 Daox 15 _COUNT		0ister */
#defin	0xFFC02CD4	/* defineister */
ister */
#defingistester */
#Da Length */


/* UART1 Co Alardefine	CAN_MB06_TIMESTAMP	0Forrial Clogh Register */Mailbox 6 Time Stamp	Value Regi02D20	/* Mailbox 9 Data Wne	CAN_MB06_ID0		0xFFC02CD8	/* 02D20	/* Mailbox 9 Data Wntifier	Low Register */
#define02D20	/* Mailbox 9 Data W		0xFFC02CDC	/* Mailbox 6 Ident02D20	/* Mailbox 9 Data Wgister */

#define	CAN_MB07_DAT02D20	/* Mailbox 9 Data W0	/* Mailbox 7 Data Word 0 [15:02D20	/* Mailbox 9 Data W/
#define	CAN_MB07_DATA1		0xFFC02D20	/* Mailbox 9 Data Wbox 7 Data Word 1 [31:16] Regis02D20	/* Mailbox 9 Data We	CAN_MB07_DATA2		0xFFC02CE8	/*02D20	/* Mailbox 9 Data Woa Word 2 [47:32] Register */
#dgist0	/* Mailbox 9 Data Wo_DATA3		0xFFC02CEC	/* Mailbox 7box 9 Identifier	High Regi63:48] Register */
#define	CAN_box 9 Identifier	High RegixFFC02CF0	/* Mailbox 7 Data Lenbox 9 Identifier	High Regiter */
#define	CAN_MB07_TIMESTAbox 9 Identifier	High Regi/* Mailbox 7 Time Stamp	Value Rbox 9 Identifieailbox 8 Iefine	CAN	High Register */FC02CF8	/* Mailbox 7 Identifier	box 9 Identifier	High Regi/
#define	CAN_MB07_ID1		0xFFC02box 9 Identifier	High Regi 7 Identifier	High Register */
box 9 Identifier	High RegiB08_DATA0		0xFFC02D00	/* Mailbobox 9 Identifier	High Reg 0 [15:0]	Register */
#define	CA	0xFFC02D58	/* Mailbox 10 0xFFC02D04	/* Mailbox 8 Data Wo	0xFFC02D58	/* Mailbox 10 egister */
#define	CAN_MB08_DAT	0xFFC02D58	/* Mailbox 10 	/* Mailbox 8 Data Word 2 [47:3	0xFFC02D58	/* Mailbox 10 
#define	CAN_MB08_DATA3		0xFFC0	0xFFC02D58	/* Mailbox 10 x 8 Data Word 3 [63:48] Registe	0xFFC02D58	/* Mailbox 10 AN_MB08_LENGTH		0xFFC02D10	/* M	0xFFC02D58	/* Mailbox 10 Length Code Register */
#define	0xFFC02D58	/* Mailbox 10 STAMP	0xFFC02D14	/* Mailbox 8 T	0xFFC02D58	/* Mailbox 10 e Register */
#define	CAN_MB08_box 9 Identifier	High Reg18	/* Mailbox 8 Identifier	Low R* Mailbox 11 Time Stamp Vafine	CAN_MB08_ID1		0xFFC02D1C	/CAN_MB10_DATA3		0xFFC02RMPdentifier	High Register RMr */
#define	TRX 29 High 0C     /IDC	/ESTAMP	0xFFC02CD4	/	0xF#define	CAN_MB0ster */

#define	CAN_MB12_D Channel 0 Y RMine	TWI0_ter *2 Data Word	0 [15:0] Register FC00C1C	/* DMRMPt/Status 		0xFFC02D84	/* Mailbox 12 Data Wo_CURR_DESC_PTRMP/
#define		0xFFC02D84	/* Mailbox 12 Data Woptor Pointer RMP_MODIFY				0xFFC02D84	/* Mailbox 12 Data Wo24	/* DMA ChaRMP/* MemDMA		0xFFC02D84	/* Mailbox 12 Data WoA0_IRQ_STATUSRMPSource Y 		0xFFC02D84	/* Mailbox 12 Data Wous Register *RMA5_Xefine	CAN_2 Data Word	0 [15:0] Register DMA Channel 0RMP0_S0_CURRme Stamp Value Register */
#define_X_COUNT		0xF	0xFa Word 2 [47:2 Data Word	0 [15:0] Register *ATA0		0xFFC02D80	_DATA3		0xFFC2 Data Word	0 [15:0] Register **/
#define	CAN_M0 Data Word	0 2 Data Word	0 [15:0] Register *rd	1 [31:16] Reg0xFFC02D44	/* 2 Data Word	0 [15:0] Register *	/* Mailbox 12 D#define	CAN_MB2 Data Word	0 [15:0] Register *MB12_DATA3		0xFFd	2 [47:32] Re2 Data Word	0 [15:0] Register * Register */
N_MBFC02D7C	/* Mailbox 11 IdenFC02CF8	/* Mai2 Data Word	0 [15:0] Register * Data Length Cod	/* Mailbox 102 Data Word	0 [15:0] Register *FFC02D94	/* MailB10_TIMESTAMP	2 Data Word	0 [15:0] Register *	CAN_MB12_ID0		0egister */
#de2 Data Word	0 [15:0] Register *ster */
#define	 0 [15:0]	RegiFFC02D84	/* Mailbox 12 Data WorATA0		0xFFC02D80D5C	/* MailboxFFC02D84	/* Mailbox 12 Data Wor*/
#define	CAN_MB/
#define	DMAFFC02D84	/* Mailbox 12 Data Worrd	1 [31:16] Reg Register */
#FFC02D84	/* Mailbox 12 Data Wor	/* Mailbox 12 Dx 11 Data WordFFC02D84	/* Mailbox 12 Data WorMB12_DATA3		0xFFA2		0xFFC02D68FFC02D84	/* Mailbox 12 Data Worgister */
#defin */
#define	CAFFC02D84	/* Mailbox 12 Data Wor Data Length Cod Word	3 [63:48FFC02D84	/* Mailbox 12 Data WorFFC02D94	/* MailC02D70	/* MailFFC02D84	/* Mailbox 12 Data Wor	CAN_MB12_ID0		0	CAN_MB11_TIMEFFC02D84	/* Mailbox 12 Data Worster */
#define	18	/* Mailbox ine	CAN_MB12_DATA2		0xFFC02D88	ATA0		0xFFC02D80box 11 Identifine	CAN_MB12_DATA2		0xFFC02D88		/* Mailbox 8RMLFFC02D7C	/* Mailbox 11 IdexFFCe	CAN_MB13_LENGTH		0xansme	CAN_MB12_DATA0		0xFFC02D8defiRegister */
#define	Ce	CAN_MB15_DATA1*/
#define	CAN_ 7 X13 Time Stamp Value Re	CAN_MB15_DATA1rd	1 [31:16] ReLister */
#define	CAN_MB1e	CAN_MB15_DATA1	/* Mailbox 12 Lata Word	2 [47:32] Regise	CAN_MB15_DATA1MB12_DATA3		0xFLC02D8C	/* Mailbox 12 Date	CAN_MB15_DATA1gister */
#defiLe	CAN_MB12_LENGTH		0xFFCe	CAN_MB15_DATA1 Data Length CoLe Register */
#define	CAe	CAN_MB15_DATA1FFC02D94	/* MaiLbox 12 Time Stamp Value e	CAN_MB15_DATA1	CAN_MB12_ID0		LxFFC02D98	/* Mailbox 12 e	CAN_MB15_DATA1ster */
#defineC0273:48] Register */
#def31:16] Register *		0xFFC02DE4	/* Mr */

#define	CAN_MB13_D31:16] Register **/
#define	CAN_MWord	0 [15:0] Register */31:16] Register * Word	2 [47:32] /* Mailbox 13 Data Word	131:16] Register *C02DEC	/* Mailbo3_DATA2		0xFFC02DA8	/* Ma31:16] Register *fine	CAN_MB15_LEr */
#define	CAN_MB13_DAT31:16] Register *ilbox 13 Data WLrd	3 [63:48] Register */
#*/
#:0] Register */
#define	CAN_MB15_DATA1 13 Data Length   0xilbox 15 Data Word	1 [31:16] Register *_MB15_ID0		0xFFCbox 13 Time Stamp Value R Register */
#defter */
#define	CFC02DB8	/* Mailbox 13 Ide Register */
#defentifier High ReMB13_ID1		0xFFC02DBC	/* M Mailbox 15 Data 		0xFFC02DE4	/* /

#define	CAN_MB14_DATA0 Mailbox 15 Data */
#define	CAN_MB0 [15:0] Register */
#de Mailbox 15 Data  Word	2 [47:32] ailbox 14 Data Word	1 [31 Mailbox 15 Data C02DEC	/* MailboTA2		0xFFC02DC8	/* Mailbo Mailbox 15 Data fine	CAN_MB15_LE
#define	CAN_MB14_DATA3		 Mailbox 15 Data th Code Register [63:48] Register */
#def Mailbox 15 Data ine	CAN_MB16_TIMailbox 14 Data Length Cod Mailbox 15 Data _MB15_ID0		0xFFCTAMP	0xFFC02DD4	/* Mailbo Mailbox 15 Data ter */
#define	Cefine	CAN_MB14_ID0		0xFFC Mailbox 15 Data entifier High Reegister */
#define	CAN_MBMB15_DATA3		0xFFC		0xFFC02DE4	/* ntifier High Register */
MB15_DATA3		0xFFC	/* Mailbox 8OPSSdentifier	Hiurce Y Count Registe	0xF15:0] Registeivider */
ress Regi00DFB0	/_PTR
r TX	SNT
#deShotr	LowN_MB12_egister */
#de	0xFFilbox 6 Time Stamp	fine	CAN_MB18_DATA1		0xFFC02E44	/* Mailbox 18 Data Wor Channel 0 Y 	0xFB15_DATA2		0xe	CAN_MB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32FC00C1C	/* DM	0xFister */
#defe	CAN_MB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32_CURR_DESC_PT	0xFata Word	2 [4e	CAN_MB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32ptor Pointer 	0xFC02D8C	/* Maie	CAN_MB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:3224	/* DMA Cha	0xFe	CAN_MB12_LEe	CAN_MB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32A0_IRQ_STATUS	0xFe Register */e	CAN_MB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32us Register *	0xFbox 12 Time Se	CAN_MB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32DMA Channel 0	0xFxFFC02D98	/* e	CAN_MB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32_X_COUNT		0xF	0xFFister */

#dee	CAN_MB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32]d	1 [31:16] Regist_DATA3		0xFFC02CEC	/B18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32]] Register */
#de0 Data Word	0 [15:0] B18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32] */
#define	CAN_M0xFFC02D44	/* MailboxB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32]CAN_MB18_TIMESTAM#define	CAN_MB10_DATAB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32]D0		0xFFC02E58	/*d	2 [47:32] Register B18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32] Register */

#def* Timer 2 Width R 18 Data Word	0 [1ilbox 16 Data xFFC02E88	/* Mailbox 20 Data Word	2 [47:32] Register */
#definata Word	0 [15:0]	/* Mailbox 10 Data LB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32][31:16] Register B10_TIMESTAMP	0xFFC02B18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32]gister */
#defineegister */
#define	CAB18_DATA2		0xFFC02E48	/* Mailbox 18 Data Word	2 [47:32]#define	CAN_MB19_ 0 [15:0]	Register */	0xFFC02E4C	/* Mailbox 18 Data Word	3 [63:48] Register d	1 [31:16] RegisD5C	/* Mailbox 10 Ide	0xFFC02E4C	/* Mailbox 18 Data Word	3 [63:48] Register ] Register */
#defegister */
#define	C	0xFFC02E4C	/* Mailbox 18 Data Word	3 [63:48] Register  */
#define	CAN_M Register */
#define		0xFFC02E4C	/* Mailbox 18 Data Word	3 [63:48] Register CAN_MB18_TIMESTAMx 11 Data Word	1 [31:	0xFFC02E4C	/* Mailbox 18 Data Word	3 [63:48] Register D0		0xFFC02E58	/*A2		0xFFC02D68	/* Mai	0xFFC02E4C	/* Mailbox 18 Data Word	3 [63:48] Register  Mailbox 18 Ident */
#define	CAN_MB11_	0xFFC02E4C	/* Mailbox 18 Data Word	3 [63:48] Register ata Word	0 [15:0] Word	3 [63:48] Regis	0xFFC02E4C	/* Mailbox 18 Data Word	3 [63:48] Register [31:16] Register C02D70	/* Mailbox 11 	0xFFC02E4C	/* Mailbox 18 Data Word	3 [63:48] Register gister */
#define	CAN_MB11_TIMESTAMP	0	0xFFC02E4C	/* Mailbox 18 Data Word	3 [63:48] Register #define	CAN_MB19_18	/* Mailbox 8 Ident50	/* Mailbox 18 Data Length Code Register */
#define	Cd	1 [31:16] Regisbox 11 Identifier Low50	/* Mailbox 18 Data Length Code Register */
#define	C	/* Mailbox 8TR		0xFFC0ler (0xFFC00900 - 0xFC00DD8	/egistereny	ButSTAT't Ll Ren Port DoAN_MB12_DATA0		0xFFC02MailN_MB01_ID0		0xAN_MB22_ID1		0xFFC02EDC	/* Mailbox 2B0	/* DMA Chan */
#defi_MAP		0xAN_MB22_ID1		0xFFC02EDC	/* Mailbox 2FC00C1C	/* DMTRRister */
#def0] Register */
#define	CAN_MB23_DATA1	_CURR_DESC_PTTRRata Word	2 [40] Register */
#define	CAN_MB23_DATA1	ptor Pointer TRRC02D8C	/* Mai0] Register */
#define	CAN_MB23_DATA1	24	/* DMA ChaTRRe	CAN_MB12_LE0] Register */
#define	CAN_MB23_DATA1	A0_IRQ_STATUSTRRe Register */0] Register */
#define	CAN_MB23_DATA1	us Register *TRRbox 12 Time S0] Register */
#define	CAN_MB23_DATA1	DMA Channel 0TRRxFFC02D98	/* 0] Register */
#define	CAN_MB23_DATA1	_X_COUNT		0xFMailister */

#de

#define	CAN_MB23_DATA0		0xFFC02EE0	/*2 Identifier High_DATA3		0xFFC

#define	CAN_MB23_DATA0		0xFFC02EE0	/** Mailbox 23 Dat0 Data Word	0 

#define	CAN_MB23_DATA0		0xFFC02EE0	/*	0xFFC02EE4	/* M0xFFC02D44	/* 

#define	CAN_MB23_DATA0		0xFFC02EE0	/*AN_MB23_DATA2		0#define	CAN_MB

#define	CAN_MB23_DATA0		0xFFC02EE0	/*r */
#define	CANd	2 [47:32] Re

#define	CAN_MB23_DATA0		0xFFC02EE0	/* Register */
Data Register */


/* UART1 Cord	lbox 16 Data 

#define	CAN_MB23_DATA0		0xFFC02EE0	/*3 Data Length CoESTAMP	0xFFC02

#define	CAN_MB23_DATA0		0xFFC02EE0	/*4	/* Mailbox 23 B10_TIMESTAMP	

#define	CAN_MB23_DATA0		0xFFC02EE0	/*0xFFC02EF8	/* Maegister */
#de

#define	CAN_MB23_DATA0		0xFFC02EE0	/*3_ID1		0xFFC02EF 0 [15:0]	Regi0] Register */
#define	CAN_MB23_DATA1		2 Identifier HigD5C	/* Mailbox0] Register */
#define	CAN_MB23_DATA1		* Mailbox 23 Data/
#define	DMA0] Register */
#define	CAN_MB23_DATA1			0xFFC02EE4	/* M Register */
#0] Register */
#define	CAN_MB23_DATA1		AN_MB23_DATA2		0x 11 Data Word0] Register */
#define	CAN_MB23_DATA1		r */
#define	CANA2		0xFFC02D680] Register */
#define	CAN_MB23_DATA1		63:48] Register  */
#define	CA0] Register */
#define	CAN_MB23_DATA1		3 Data Length Co Word	3 [63:480] Register */
#define	CAN_MB23_DATA1		4	/* Mailbox 23 C02D70	/* Mail0] Register */
#define	CAN_MB23_DATA1		0xFFC02EF8	/* Ma	CAN_MB11_TIME0] Register */
#define	CAN_MB23_DATA1		3_ID1		0xFFC02EF18	/* Mailbox a Word	1 [31:16] Register */
#define	CA2 Identifier Higbox 11 Identifa Word	1 [31:16] Register */
#define	CAxFFC02ED8	/* MaFFC02E40	/* Ma*/
#define	DMA15:0] Register2CAC	/* MailbD licenDMA Cailbox 22 Identifier Hister */
#defin/* Mailbox 26 Data Word	3 [63:48]* Mailbox 23 Daefine	CAN_MB18/* Mailbox 26 Data Word	3 [63:48]	0xFFC02EE4	/* MB18_LENGTH		0/* Mailbox 26 Data Word	3 [63:48]AN_MB23_DATA2		MP	0xFFC02E54	/* Mailbox 26 Data Word	3 [63:48]r */
#define	CA* Mailbox 18 I/* Mailbox 26 Data Word	3 [63:48]63:48] Registertifier High Re/* Mailbox 26 Data Word	3 [63:48]3 Data Length C] Register */
/* Mailbox 26 Data Word	3 [63:48]4	/* Mailbox 23 */
#define	CA/* Mailbox 26 Data Word	3 [63:48]0xFFC02EF8	/* Me	CAN_MB19_DAT/* Mailbox 26 Data Word	3 [63:48]3_ID1		0xFFC02E_LENGTH		0xFFC0_LENGTH		0xFFC02F50	/* Mailbox 26  Register */
#defr */

#define	LENGTH		0xFFC02F50	/* Mailbox 26  Data Length CodWord	0 [15:0] RLENGTH		0xFFC02F50	/* Mailbox 26 02F54	/* Mailbox/* Mailbox 13 DLENGTH		0xFFC02F50	/* Mailbox 26 B26_ID0		0xFFC023_DATA2		0xFFC0LENGTH		0xFFC02F50	/* Mailbox 26 define	CAN_MB26_r */
#define	CALENGTH		0xFFC02F50	/* Mailbox 26 02F10	/* Mailbo		0xFFC02E8C	/er */
#define	CAlbox 16 Data LLENGTH		0xFFC02F50	/* Mailbox 26 Data Word	0 [15:ESTAMP	0xFFC02ELENGTH		0xFFC02F50	/* Mailbox 26 F64	/* Mailbox 2box 13 Time StaLENGTH		0xFFC02F50	/* Mailbox 26 MB27_DATA2		0xFFFC02DB8	/* MailLENGTH		0xFFC02F50	/* Mailbox 26 ter */
#define	CMB13_ID1		0xFFC#define	CAN_MB26_TIMESTAMP	0xFFC0 Register */
#de/

#define	CAN_#define	CAN_MB26_TIMESTAMP	0xFFC0 Data Length Code0 [15:0] Regis#define	CAN_MB26_TIMESTAMP	0xFFC002F54	/* Mailboxailbox 14 Data #define	CAN_MB26_TIMESTAMP	0xFFC0B26_ID0		0xFFC02TA2		0xFFC02DC8#define	CAN_MB26_TIMESTAMP	0xFFC0define	CAN_MB26_
#define	CAN_MB#define	CAN_MB26_TIMESTAMP	0xFFC0Register */

#de [63:48] Regist#define	CAN_MB26_TIMESTAMP	0xFFC0Data Word	0 [15:ailbox 14 Data #define	CAN_MB26_TIMESTAMP	0xFFC0F64	/* Mailbox 2TAMP	0xFFC02DD4#define	CAN_MB26_TIMESTAMP	0xFFC0MB27_DATA2		0xFFefine	CAN_MB14_#define	CAN_MB26_TIMESTAMP	0xFFC0ter */
#define	Cegister */
#def Value Register */
#define	CAN_MB Register */
#dentifier High Re Value Register */
#define	CAN_MB	/* Mailbox 8AAlbox 22 Iden	TWI0_SLAVE_AA*/
#define	CANe	UART1Data Word	CAN_MB12_DATA0		0xFFC02AAMailbox 6 Timetamp Value Register */
#def Channel 0 Y AA6 Current X Cotamp Value Register */
#defT	0xFFC01418	/Antifier	Low Retamp Value Register */
#def_CURR_DESC_PTAA		0xFFC02CDC	/tamp Value Register */
#defptor Pointer AAgister */

#detamp Value Register */
#def24	/* DMA ChaAA0	/* Mailbox 7tamp Value Register */
#defA0_IRQ_STATUSAA/
#define	CAN_tamp Value Register */
#defus Register *AAbox 7 Data Wortamp Value Register */
#defDMA Channel 0AAe	CAN_MB07_DATtamp Value Register */
#def_X_COUNT		0xFAAC	/* Mailbox 2/* Mailbox 29 Identifier Lowine	CAN_MB29_ID0_DATA3		0xFFC/* Mailbox 29 Identifier Loww Register */
#0 Data Word	0 /* Mailbox 29 Identifier Lowx 29 Identifier0xFFC02D44	/* /* Mailbox 29 Identifier Low		0xFFC02FC0	/*#define	CAN_MB/* Mailbox 29 Identifier Low/
#define	CAN_Md	2 [47:32] Re/* Mailbox 29 Identifier Low Register */
AArd	3 [63:48] Register */AA2F80	/* Mailbo/* Mailbox 29 Identifier Low02FC8	/* MailboESTAMP	0xFFC02/* Mailbox 29 Identifier Lowine	CAN_MB30_DAB10_TIMESTAMP	/* Mailbox 29 Identifier Low [63:48] Registegister */
#de/* Mailbox 29 Identifier Low	/* Mailbox 30  0 [15:0]	Regi9_ID1		0xFFC02FBC	/* Mailboxine	CAN_MB29_IDD5C	/* Mailbox9_ID1		0xFFC02FBC	/* Mailboxw Register */
#d/
#define	DMA9_ID1		0xFFC02FBC	/* Mailboxx 29 Identifier Register */
#9_ID1		0xFFC02FBC	/* Mailbox		0xFFC02FC0	/*x 11 Data Word9_ID1		0xFFC02FBC	/* Mailbox/
#define	CAN_MA2		0xFFC02D689_ID1		0xFFC02FBC	/* MailboxWord	1 [31:16]  */
#define	CA9_ID1		0xFFC02FBC	/* Mailbox02FC8	/* Mailbo Word	3 [63:489_ID1		0xFFC02FBC	/* Mailboxine	CAN_MB30_DAC02D70	/* Mail9_ID1		0xFFC02FBC	/* Mailbox [63:48] Regist	CAN_MB11_TIME9_ID1		0xFFC02FBC	/* Mailbox	/* Mailbox 30 18	/* Mailbox  */

#define	CAN_MB30_DATA0	ine	CAN_MB29_IDbox 11 Identif */

#define	CAN_MB30_DATA0	xFFC02ED8	/* MTIMESTAMP	0xF28_DATA0		0xFFilbox 29 Time Segister *SuC			0fuefinom3 [63:48] Register */
#D0		0xFFC02FB8	AN_MB00_DATA0+((x)*0x20))


/****B0	/* DMA Chan#define	CAN_MB2AN_MB00_DATA0+((x)*0x20))


/****	0xFFC02EE4	/*r High RegisterAN_MB00_DATA0+((x)*0x20))


/****AN_MB23_DATA2	* Mailbox 30 DaAN_MB00_DATA0+((x)*0x20))


/****r */
#define	CMB30_DATA1		0xFAN_MB00_DATA0+((x)*0x20))


/****63:48] Registe Register */
#dAN_MB00_DATA0+((x)*0x20))


/****3 Data Length ox 30 Data WordAN_MB00_DATA0+((x)*0x20))


/****4	/* Mailbox 2ATA3		0xFFC02FCAN_MB00_DATA0+((x)*0x20))


/****0xFFC02EF8	/* ter */
#define	AN_MB00_DATA0+((x)*0x20))


/****3_ID1		0xFFC02 Data Length Co**************************************************_DATA3		0xFFC*************************************************0 Data Word	0 **********************************
/*************0xFFC02D44	/* *************************************************#define	CAN_MB********************************** MASKS ********d	2 [47:32] Re**********************************02F10	/* Mailbdefine	CAN_MB)
#define	CAN_M2F80	/* Mailbo**********************************DIV2		0x0001	/*ESTAMP	0xFFC02**********************************/* 0: PLL = CLKB10_TIMESTAMP	**********************************02	/* Shut	off egister */
#de**********************************	Clock Off		 */ 0 [15:0]	Regim MMR Register Bits and	Macros */
***************D5C	/* Mailboxm MMR Register Bits and	Macros */
****************/
#define	DMA7 MMR Register Bits and	Macros */

/************* Register */
#	SSEL				0x000F	/* System Select ****************x 11 Data Word	SSEL				0x000F	/* System Select * MASKS ********A2		0xFFC02D68	SSEL				0x000F	/* System Select *	PLL_CLKIN			0x */
#define	CA	SSEL				0x000F	/* System Select *DIV2		0x0001	/* Word	3 [63:48	SSEL				0x000F	/* System Select */* 0: PLL = CLKC02D70	/* Mail	SSEL				0x000F	/* System Select *02	/* Shut	off 	CAN_MB11_TIME	SSEL				0x000F	/* System Select *	Clock Off		 */18	/* Mailbox *************************************************box 11 Identif**********************************	/* Mailbox 8 BTDMESTAMP	0xFFC02FB4	/* MTDine	CAN0_LSFFC02C8C	/* MoR_DESorarilyTime Stamctive Mode Wia Length Code FULL_ON		0x0002	/*define	CAN_MB02_TIMESTATarity Re30	/* Co/
#define	ACTIVE_ Register */
#definRFHFC02E40	/* Mailboxnnel Register box 0 Hig Register */78	/mati0xFFAC	/* Mail Handnel Rrd	3 [63:48] Register */
PLL_ilbox 6 Time Stamp	/* VR_CTL Masks										 */
#define	FREQ			*/
#define	CANF08	/* Ma	/* Slave Mode* VR_CTL Masks										 */
#define	FREQ			rd	1 [31:16] RFe Maskde Status Regist* VR_CTL Masks										 */
#define	FREQ				/* Mailbox 12Failbox 202E54	/* Mailne	FREQ_667		0x0002	/*		Switching Frequency MB12_DATA3		0xF */
#defx 18 Identifine	FREQ_667		0x0002	/*		Switching Frequency gister */
#defF02B18	/*igh Register ne	FREQ_667		0x0002	/*		Switching Frequency  Data Length CFance Mine	CAN_MB07_DAne	FREQ_667		0x0002	/*		Switching Frequency FFC02D94	/* MaFMailboxata Word 1 [31ne	FREQ_667		0x0002	/*		Switching Frequency 	CAN_MB12_ID0	Fk */
#dB19_DATA3		0xFne	FREQ_667		0x0002	/*		Switching Frequency ster */
#definFC02B28	/ 2 [47:32] Regency For Regulator */
#define	HIBERNATE		0x000x0003	/* Switchi_DATA3		0xFFC02CEC	/ency For Regulator */
#define	HIBERNATE		0x00000	/*		Powerdow0 Data Word	0 [15:0] ency For Regulator */
#define	HIBERNATE		0x00tching Frequency0xFFC02D44	/* Mailboxency For Regulator */
#define	HIBERNATE		0x00Is 667 kHz */
#d#define	CAN_MB10_DATAency For Regulator */
#define	HIBERNATE		0x00fine	GAIN			0x00d	2 [47:32] Register ency For Regulator */
#define	HIBERNATE		0x00ilbox 13 Data FH	0xFFC02E8C	/* MaiEV_105		0x00A0	/ [63:48] Register */ency For Regulator */
#define	HIBERNATE		0x00= 20 */
#define		/* Mailbox 10 Data Lency For Regulator */
#define	HIBERNATE		0x00rnal Voltage LevB10_TIMESTAMP	0xFFC02ency For Regulator */
#define	HIBERNATE		0x0000		0x0090	/*	VLegister */
#define	CAency For Regulator */
#define	HIBERNATE		0x00LEV_105		0x00A0	 0 [15:0]	Register */gulation */
#define	FREQ_333		0x0001	/*		Swit0x0003	/* SwitchD5C	/* Mailbox 10 Idegulation */
#define	FREQ_333		0x0001	/*		Swit000	/*		Powerdownegister */
#define	Cgulation */
#define	FREQ_333		0x0001	/*		Swittching Frequency Register */
#define	gulation */
#define	FREQ_333		0x0001	/*		SwitIs 667 kHz */
#dx 11 Data Word	1 [31:gulation */
#define	FREQ_333		0x0001	/*		Switfine	GAIN			0x00A2		0xFFC02D68	/* Maigulation */
#define	FREQ_333		0x0001	/*		Swit */
#define	GAIN */
#define	CAN_MB11_gulation */
#define	FREQ_333		0x0001	/*		Swit= 20 */
#define	 Word	3 [63:48] Regisgulation */
#define	FREQ_333		0x0001	/*		Switrnal Voltage LevC02D70	/* Mailbox 11 gulation */
#define	FREQ_333		0x0001	/*		Swit00		0x0090	/*	VL	CAN_MB11_TIMESTAMP	0gulation */
#define	FREQ_333		0x0001	/*		SwitLEV_105		0x00A0	18	/* Mailbox 8 Identne	FREQ_667		0x0002	/*		Switching Frequency I0x0003	/* Switchbox 11 Identifier Lowne	FREQ_667		0x0002	/*		Switching Frequency IBLED	0x0001	/* PIF1+((x)*0x20))
#define	CANRR_IR15:0] RegisteT 3 De ADI B	PTR		0x	CAN_MB12_DATA0		0xFFC02RR_IRQefine	SET_IN_t */
#define	UART0_ERR_IRQ		0x Channel 0 Y CR_IRB15_DATA2		0xpt Request */
#define	RTC_IRQ		FC00C1C	/* DMAR_IR Is 333 kHz *pt Request */
#define	RTC_IRQ		_CURR_DESC_PTRR_IRefine	FREQ_10pt Request */
#define	RTC_IRQ		ptor Pointer RR_IR0C	/* Voltagept Request */
#define	RTC_IRQ		24	/* DMA ChanR_IR_10			0x0004	pt Request */
#define	RTC_IRQ		A0_IRQ_STATUS	R_IRGAIN_50			0x0pt Request */
#define	RTC_IRQ		us Register */R_IRbox 12 Time Spt Request */
#define	RTC_IRQ		DMA Channel 0 R_IRxFFC02D98	/* pt Request */
#define	RTC_IRQ		_X_COUNT		0xFFR_IRQ#define	PDWN		t Request */
#define	RTC_IRQ			00000040	/* UART0 E#define	IN_DELt Request */
#define	RTC_IRQ				0x00000080	/* Reae	OUT_DELAY			0t Request */
#define	RTC_IRQ			A0_IRQ		0x00000100ASS				0x0100	/t Request */
#define	RTC_IRQ			
#define	DMA1_IRQ	tiplier Select t Request */
#define	RTC_IRQ			errupt Request */
/
#ifdef _MISRAt Request */
#define	RTC_IRQ			 Register */
# Real		0x00000020	/* SPI0	Error In = CLKIN*MSEL *t Request */
#define	RTC_IRQ			0	/* DMA Channel 3#define	SET_IN_t Request */
#define	RTC_IRQ			A4_IRQ		0x00001000<	0x2))
#else
#t Request */
#define	RTC_IRQ			est */
#define	DMASEL = 0-63 --> t Request */
#define	RTC_IRQ			errupt Request */
x)&0x03) << 0x6nterrupt Request */
#define	DMA00000040	/* UART0 | (((x)&0x01) <nterrupt Request */
#define	DMA	0x00000080	/* Real/
#define	DMA7nterrupt Request */
#define	DMAA0_IRQ		0x00000100			0x0030	/* Conterrupt Request */
#define	DMA
#define	DMA1_IRQ	= VCO / 1 */
#dnterrupt Request */
#define	DMAerrupt Request */
efine	CSEL_DIV4nterrupt Request */
#define	DMA2 (SPORT0 TX) Inte		0x0030	/*		CCnterrupt Request */
#define	DMA0	/* DMA Channel 3CLK	= VCO /	x *nterrupt Request */
#define	DMAA4_IRQ		0x00001000ES
#define	SET_nterrupt Request */
#define	DMAest */
#define	DMA = VCO/SSEL */
nterrupt Request */
#define	DMAerrupt Request */
L = 0-15 --> SCl 0 (PPI) Interrupt Request */
00000040	/* UART0 LL_STAT Masks		l 0 (PPI) Interrupt Request */
BLED	0x0001	/* RIRQ		0x00000020	/* SPI0	ErrorRInterrupt	Requex 3 D
#define	UART0_ERR_IRQ		0x00000040	/* UARks FTAMP	0xFFC02E14Request */
#define	RTC_IRQ			0x00000080	/* Real-Time Clock 
#define	DMA8_IRQ			0x00000002	A0_IRQ		0x00000R00	/* DMA Chann
#define	DMA8_IRQ			0x00000002	
#define	DMA1_IRQ		0x00000200	/
#define	DMA8_IRQ			0x00000002	errupt Request R/
#define	DMA2_
#define	DMA8_IRQ			0x00000002	2 (SPORT0 TX) IRterrupt Request
#define	DMA8_IRQ			0x00000002	0	/* DMA ChanneR 3 (SPORT1 RX) 
#define	DMA8_IRQ			0x00000002	A4_IRQ		0x00001R00	/* DMA Chann
#define	DMA8_IRQ			0x00000002	est */
#define	RMA5_IRQ		0x0000
#define	DMA8_IRQ			0x00000002	errupt Request ks Fister */

#defin	0x00004000	/* DMA Channel 6 (UART RX) Interks Fr */

#define	CAine	DMA7_IRQ		0x00008000	/* DMA Channel 7 (Uks Ford	0 [15:0] Regquest */
#define	TIMER0_IRQ		0x00010000	/* Tks F* Mailbox 13 Datuest */
#define	TIMER1_IRQ		0x00020000	/* Tiks F_DATA2		0xFFC02Dest */
#define	TIMER2_IRQ		0x00040000fine	MBRIF15		0x8000	/* RX Interrupt	Active In Mailbox 15 */

/* CAN_ Copy2 Masks	censesed #de/*
 * Copyr6ght 0001-2009 Analog Devices Inc.
 *
 * Lice6or the GPL-2 (or l7ter)
 *2-2009 Analog Devices Inc.
 *
 * Lice7or the GPL-2 (or l8ter)
 *4-2009 Analog Devices Inc.
 *
 * Lice8or the GPL-2 (or l9ter)
 *8-2009 Analog Devices Inc.
 *
 * Lice9or the GPL-2 (or 20ter)
 18-2009 Analog Devices Inc.
 *
 * Lic20******************1ter)
 2******************************** */
/1******************2ter)
 4******************************** */
/2******************3ter)
 8******************************** */
/3******************4ter)
108-2009 Analog Devices Inc.
 *
 * Lic24******************ight 02egister (16-bit) */
#define	PLL_DIV			nsed **************ater)
4egister (16-bit) */
#define	PLL_DIV			ESS DEFINITIONS FO2 ADSP-8egister (16-bit) */
#define	PLL_DIV			e _DEF_BF539_H

/*2inclu1008-2009 Analog Devices Inc.
 *
 * Lic2s */
#include <asm2def_L210	/* PLL Lock	Count register (16-bit) ******************3*****4008-2009 Analog Devices Inc.
 *
 * Lic3* System MMR Regis3er Ma2008-2009 Analog Devices Inc.
 *
 * Lic3*****under the IM1BSD licenseFFC00008	/* VoIM********/

/*Enablnc.
log Dev For	*
 * Li	* System MMR ReIMer Map F538/9C001FF) */
#define	SWRST			0x***************IM******* all CC001FF) */
#define	SWRST			0xgulator ControlIMFFC0000lackfiC001FF) */
#define	SWRST			0xxFFC00000	/* PLIMntrol *******C001FF) */
#define	SWRST			0x0xFFC00004	/* PIMivide  */
/**C001FF) */
#define	SWRST			0xxFFC00008	/* VoIMater)
 ******C001FF) */
#define	SWRST			0xESS DEFINITIONSIM ADSP-B0 - 0xC001FF) */
#define	SWRST			0xe _DEF_BF539_H
IMincludregisteC001FF) */
#define	SWRST			0xs */
#include <IMdef_LPRegisteC001FF) */
#define	SWRST			0x***************IM1******lator Cgister (16-bit) */
#define	SYSFFC00100  /* Softwer Map/* PLL gister (16-bit) */
#define	SYSSCR			0xFFC00104 1*****010	/* Pgister (16-bit) */
#define	SYSe	SIC_IMASK0		0xF1FFC00C00014	/gister (16-bit) */
#define	SYSfine	SIC_IAR0		0x1ntrol_VERSIONgister (16-bit) */
#define	SYS 0 */
#define	SICright 2008-200gister (16-bit) */
#define	SYSnsed under the IM BSD licenseC00124  /* Interrater)
 */

/*gister (16-bit) */
#define	SYSssignment RegisteR ADSP-BF538/9gister (16-bit) */
#define	SYSterrupt Assignmen include all Cgister (16-bit) */
#define	SYS120  /* Interrupt/def_LPBlackfigister (16-bit) */
#define	SYSFC00124  /* Inter**************nfiguration registe */
#defineFFC00100  /* Softter Map */
/**nfiguration registe */
#defineSCR			0xFFC00104  *************nfiguration registe */
#definee	SIC_IMASK0		0xFxFFC00000 - 0xnfiguration registe */
#definefine	SIC_IAR0		0xontrol registenfiguration registe */
#define 0 */
#define	SICDivide Registenfiguration registe */
#define Register 1 */
#dge Regulator Cnfiguration registe */
#definessignment RegisteC0000C	/* PLL nfiguration registe */
#defineterrupt AssignmenxFFC00010	/* Pnfiguration registe */
#define120  /* Interrupt		0xFFC00014	/nfiguration registe */
#defineFC00124  /* InterCHIPID_VERSIONnterrupt Mask Register */
#defFFC00100  /* Soft 0x0FFFF000
#dnterrupt Mask Register */
#def0000FFE

/* SGIMBSD license C0013C	/* InterEWTIMssignment	Register TX Error Count) */
#definDivisor LatchR(Low-Byte) */
#defineRUART0_IER	      0xFFC00404  /* InterP(Low-Byter */
#defineRT0_I-Passive	ModF) */
#definFC00008	/* BO(Low-Byte*/
#define	WBus OffUART0_IIR	      0xFFC0WU(Low-Byt */


/* RealWake-UpUART0_IIR	      0xFFC0UIA(Low-Byt	RTC_STAT	0xFAccess To Unimplemented Addr0xFFART0_IIR	      0xFFC0A */
#defiCTL	0xFFC0030Aborviceknowledg	UART0_IIR	      0xFFC0RML(Low-BytRTC_ISTAT	0xFRX Message Los    0xFFC00404  /* InteUCE(Low-Byefine	RTC_SWCNUniversalER	   er Overflow Register */
#define	UEX (Low-By/
#define	RTC_External Triggl Coutpu    0xFFC00404  /* InteAD(Low-By
#define	RTC_F		 0xFFDenied	ART0_IIR	    0_DLL	    S 0xFFC00400  / Divisor Latch (Ster)
 */

/*	UART0_IER	     RQ Status404  /* Interrupne	SPI0_538/9 */xFFC00508  /* SPI0 Status register P
#define	r */
#Byte) */
#de ine	UA SPI0 Status registerBO
#define	ackfitificatio SPI0 Status registerWU
#define******0040C  /* SPI0 Status registerer *ne	SPI0*/
/**		 0xFFC00410  /*	Modem Control R SPI0 Status registerAer */
#de******14  /  Line Status  SPI0 Status registerUARTne	SPI00 - 0xFFC /* SCR Scratc* SPI0 Baud rate RegisCEters (0registe00424	 /* Global Control Reg SPI0 Status register Xfine	SPI/
#definxFFC005FF) */

#define	SRDBR Shadow	Register *Dters (0lator Cter */
#definr	0 Counter Reg0_DLL	    F/* SPI0 Flag register */
#defiFe	SPI0_STAT			0xFFC00508  /* SPFlag404  /* InterrupWIDTH			SPI0_TDBR			0xFFC0050C  0 Width	Register P

#defineRegister */
#define	SPI0_RDB0 Width	RegisterBO

#defineve Data Buffer	Re0 Wiregister */
#WU

#defin		0xFFC00514  /* S Register	     */er *WIDTH		fine	SPI0_SHADOW			0xFFC00518  /* SPI0_RDB0 Width	RegisterA Period R SPI0_REGBASE			SPI0_CTL


/0 Width	RegisterUARTWIDTH		FFC00600 - 0xFFC006FF) */
0 Width	RegisterFIG	WIDTH	C00600     /* Timer	0 Configuration	Re0 Width	Register X0_WIDTH	NTER				0xFFC00604     /* Timer	0 CFC0061C	/*  TimerDTIMER2_TIMER0_PERIOD			0xFFC00600 Width	under thUCCNiod Register */
#define	TIMer 2 ter)
 *F600     /* Timer	0 Conine	U    */

#defi_STAMPer)
 */

/		Timestamp Timer Enable	RegistWDOG#define	TIM		Watchdog Timer Enable	RegistAUTOTXer)
 *3 RegiAuto-Transmit Timer Enable	RegistERRORer)
 *6 RegiCANBR			0xFrame	R	     Enable	RegistOVERter)
 *7ags (0xFFontrload0 - 0xER	     Enable	RegistLOST0614	/*  Ti		Arbitration ScratDuring	TXask to directly speciAAter)
 *9e	TIMEX_REGBASsk to directly speciTl InterrApt Flag Su	 0xFfu* Globa directly speciREJECTer)
 *Bpins 00 - 0xFFC0Rejecdem  Peripheral InterruptMLter)
 *Cer (set) */
#defScraFFC007FF) */
#define	RXter)
 *De	TIMEotalFFC00708  /*00 - 0xFFCsFLAG_T					0xFFC0070C  Pter)
 *Epins 0xFFC00710  /*W/Mter ing IDFFC00704  /* PeripheraL/* Flag NABL		Correct	 /* SCR	Oner tata BL*
 * Peripheral InterruRCriod Registe00424	 /* Global CRe /* /Clear	    */

#definstate o******00424	 /* Global C
#deF) */

#   */
#define		620	/* Timer00424	 /* Global CC001FF)062C	/* TimESRBSD licen61C	/*  TimerCKEiguration Re Line StatusRT0_IEFC00008	/* SO_FL614	/*  TimStuffine	FIO_MASKB_C			CR(tog00110  /* CRCine	FIO_MASKB_C			0A0FC00724*/
/**Stuck At Dominantine	FIO_MASKB_C			BEFFC00724******Birrupt B 0 Width	RegisterFxFFC007240 - 0xForm	ASKB_T			0xFFC0  /* FlaW Mask Inte register */
#deLRE  /* FlFNABLETDBR			0xFFC005Limit	(For  */
#)DIR				0xFFC007T0  /* FF08-200	UART0_IER	    on Register *T
#defin
#endif /* _DEF_BF539_HO_MA