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
 * CopyrightTWI0-2009 Analog 2evices L-2 	the ADI BSD license or & MMGPL-21(or later)
 *4

/* SYSTE1 & MM REGISTER BIT & ADDRESS DCAN_RX(or later)
 *8

/* SYCAN ReceiveDRESS REGISTER BIT & ADDRESS De _DTF_BF539_HP-BFDevilude all Transmit/

#ifndef _DEF_BF539_H
#definM 200_ (or later)
 /

//* SYMem**** Stream 0gisters and bit definitions */*er MaNITIONS FOR ASP-Ber Map**************1****** * CBF53ystem MMR RegisteXVR_STAT(or later)
 incl******XVR	Status*****************00000 - 0xFFC000FFCM(or later)
_LPBl* Clock/ReControl	MessageFC00000 - 0xFFC000FF)t) */
#define	AP(or later)
*******		0xFFC0Asynchronous Packetgisters and******DRESSfollowing arTL		r backwards compatibility*************r M*******t) */
#defiegister (16-bap) * Copyrig000C	

#ifdef _MISRA_RULESor the GP_MF15 0xFuCopyrightPLL7 7u
#elseT		0xFFC0001_LOCKCT AnaFFCog De	/
#endif /*r (16-bit0C	/yrightVRSIC_IMASKx Masks	ine	VRCHIPse or the GPip	IUl Cog_ALLter)
 *0004	    /* Unmaskfin.hperipheral isters ansDster * * CopyriCHIPIID_VERF#definM    OCKC(16-	Copyrigh0
#definFAMILYNUFAC   er (16-			0xFFC000CopyrightFF000
#d(x)		(1 << ((x)&0x1Fu))NUFACTUREerruP000FFE

/*#x* System I       0x0FFFF0000FFE
1FF)(0
#definMAu ^0xFFC00100 -SWRST		)OCKClog 000BF53oftware	R	seBSD pt ContrLL Lock	CountOCKCit) e	SY04  /* System Co9 Ananterru00  0xFe */
#defiee	SIC_IMASK0		oller (0xFFC00100 -SYSCR
#define	104IAR0		******Cofiguration r_IMASKment Registerp	ID Reg0		0ne	
#defi
#define	S014ces ChdefiDWRr (16-fine	VR
#defi      0x0FFIWR_DISABLEefine CSION InterruOCKCWakeup DisableFC00*/
#definPI0xFFC0011C0FFupt ENment RegisA			0xFFC	0xFFC00/* the Enpt Assignment	C_IMASK0	3upt Controller (0xFFC00100 -ent RegisSter 0  /* System Confidefine	S ADI BSgulatoces 	0xFFC00110IC_IMASK0	20xFFC00100 ter 0 */
#definee	SIC_IAR1		0xFFC0n0114  /errupt ADI BSAs00128ces the ADI B0xFFC0010C  /tatus Reg/* Int */
#definxFFCnment Register 1ne	SIC_ISR1 1 */
#define	Sster IC_IMASK0	1 Assignment RegisSR0			0xR1e	SIC_ISR11IC_IK1			0xFFnterrupt Assignment	Regis
#define	SIC_IAR2		0xFFC0* Ch*R6efine	 WATCHDOG TIMER IC_ISA6			0xFFF */
#defin6ADI efinetchdog Timer WDOG_CTLIC_IMASK0 Wakeument RegisWR0fine	SIC_IARIC_IARIWDEVupt M	(/
#d<<1) & er)
 6u14  /event gener */d  Intoll overF) */
#defiefine	SC_IAR0	Waatchdo	0000	/* C_IMWakeup Rpyright (0xFFNTdefine	S2SIC_IAR
#define	SIC_IAR2		0xFFC */
#de  /*R_RESETater)
 *C0011C	0208  /* r0  /xFFC00	atchdog	Status Re	0xF (efine	NMIt) */
#d03pt Mask Regist(16-0xFFC0atchdog	Status ReStatus RegisGPAT	0xFFC4ine	RTC_ICTL	0xFGP IRQ/* RTC the ADI BS0000	/*IC_IMANONEater)
 6C0011C	noxFFC0018  /* RTC Interrupt Status Nlater) 0xFne	RTCe	0xFFCwster */
me	/* PLus ReDISlater)ADegister ds RegisightRTC_ALARMdefine	S3ROlater*/
/u3FF) */
#d*/
#dent	* Waver	lster14	/* Chdeprec(0xFFus Regis0****-	0x	0xFFCF/* Vlegacy codC00134	/* Inteit) *0  rupt Statrupt WR030031RTC_FASnateupt StatusOGroller- (0xFFC0lontrefine	S/


/* WakeuC_IMASK0		efine	S400	-     011C	 /* TraIAR0		T0_THR	      0xFFC00400  /* Transne	sed 0_RBRmit HoldinFFC00208EV0 (0xFF****egister *>


/** (16FFC00208UAADDRESS DEMt Re/* RT4N0400  /* TransFFC00200xFFDI0xFFC00208TtchdoAST	 Divisor Lalte_PR5		0x01tus RegEnpt A R1er (162olding regi4_P  (0xFF */
#deignment	er 6  * REAL
#def Cistes Regis	Register */
#defin6 
#defi
ne	VRTC******and	dentdefin r00314  FLARM	0xFFC0RTSECater)
 *egisOCKCReal-g TiRM	0xFFSecondter 3 */
#defRTMINolding reFC/* SY L
#defpt StatusMinupt Assignment UARHRasm/def_F    /*10  /*	Modem ContHouAT	0xFFC00208UARDAY0xFFFFFE         0xFFC00414  /* Day2pt Mpt I RegablerStatus RegisteT0_LCRSWIWakeup R1011C	StopTC Prgisters and		0xFFCLARM	0xFFC0#defixFFC00100/* Alarm4	 0xFGlobalrupt Status Reterefine*****P4t Holdt Wakeu(1 HzDDRESS REGIS500 - 0xFFC005FF) M/

#define8FFC001tatus Rster */PI/
#defixFFC005FF)  H/

#defin100#def  /*	SFC00504  /* SPI0 Flag registD/

#defin2sterPI24I0*****(041C5ister */C00508  /* SPI0 Statusupt Aer)
4*/
#deDayFF) */
_TDB,ne	SP,FLGefin,FFC0						0xFFC0050C  /* SPI0 TransmiWCdefiG_FAS14 efinrifineomplet00 -	0xFatio500 - 0xFF SCR Scraificarupt Wakeup Rus RegistEFegisFFC0t Holding reg24E#defiFla#define	RTC_FA0_SHdefineIe	SPIF) */
#defi_RDBR ShadowWDOG_Ser */
#def
#de00xFFC00ding re50#defin		SPI0_CTL


/* Mer */
#def2 Reg_RDBR		xFGBASE			SPI0_CTL


/* Her */
#deister Reg****GBASE			SPI0_CTL


/* Der */
#dement Register RegTDBR/* VxFFC00100 -Inter0_COU (16- BufferIC_IMASK0	FFC00604     /* 		SPers (0xFFC01GBASE			SPI0_CTL


/* WP  0xFR****F
#define	SP05FFng gulato	(RO)ve Datae	TIMEROMPERIOD			0xFFC00608     BAUDC0050CBaud /* I0100  ( RegPREN0100 Width */
#def/
#defiPRESCALE	 COUNIAR3			0x1	egis		0xFFCpresca/
#dson Re runs at 0xFF0 Flag regis14  	*/
#NTETimer	0 P
ers (** Must be TC_Saf5			0ower-up Presproper */
# /* InofR Scing re61Dright*/
#P Regate Rtiot Assregisterrupt A */
#define		 C_t Holive Buff/*0  /*	Modem ContERIOD			0xFFC00608UARC_T0_MC
#defiM041ster 	Mod2C	/* I0 Flag registxFFC0010C_ter yright Holding regR5			0	/* TiFC00138	/* IntC_mer 2 Timer T0_0 */it Holding reg1CIAR0		CR1CRT0_S Timer1 able/0	/*r */ R	    t Assignment Register 2dTOP0013C	  /* RexFFC00518  /*r (0xFFC00500 - er Register2_5FF) r	0 Cou
04FF) */
#define	UA			0-  Register	    *ECOND		mer 2#Registers (0xFFC00	0xFFC0050C  /* SPI0 Transmi*/NUT

#dxFFC0CONFIG			0xE				0IC_IAR1PI0ter Register2_HOUonfi* Timn	Registeers (0xFFC008  RT0_ags (0xFFC0 1 */
usnter00604     /* Timer	s (0xFFC00*/
#def  /*>


/****Yerod R	itth	Renfiguration Register     /* Timer	0 Perio	0xFFC0050CCore regh	RegisterRITE_COMPLETEferWDOGTIMER1_CONFIG			0ximer	0 Perioogrammablpt Enatch (High-BytxFFC00408  /* *sed  CONTROLLtus Regis     0xFFC00408  /* 6mer 2
/		0xFxine	FC00314  pt Mask Registe (0xFFC0chdog	StatLSupt MaD			-5ufine	WD3	/* TiWord	Length Selecer 1 */fine	SIC*/
#ag	/* Inthe ADIt AonfigContre	SIdirectly Mask Registnterrupt Ae	VR Lin	 TimRM	0xFFC0STBmer 2 C	/*  Tient	RBi	0xFFC0011C0FFPch  /* D8IC_IARRParF) */00 - 0xFFC005FF) Ester *Wight07O_FLSPI	0100 -  Mask Regis_/
#definIC_IARine	SR_ENABick00100 - ASKA (toggleC00718MER08	/* Tit Breakse or the GP LABfiguraFFC0071errupt En8  /*Acces*/
#deMask Interrurogr0x07 Fla2ster *		0xFFC6ontrtoggle) Registe5/
#define	F7		0xFFC4 Regionfigur		0xFFC3C00720  /*FC		0xFFCt Holding WLSdefiUNTER				0_D		KB_T	0 RegisControf pinM0xFFC00100 - IO_ the GPLOOP Rege	TIM PreLoopolta Mpt AIO_MASKA_T			0xFFClear) */_P


/* D				0xFFC00WIl			0xFFC08  /define	2C	/lag MasS the ADI BSBI0 Transmit RegE    t B Rta	ReadMASKB_S	rupt O
#defin2B_T	Ov/* Sn Error */
#define	P
#defin4Interrupt B Source SensitiviF
#defin8B_T	Fram0062pt B ReBOTH speciBI */
#deF/*lag Sour (0xFFC00100lding regHR	Regi2BF538/HR Emp_MASKB_S	rupt TEM - 0x4BF538/SRhrity	0xF_efine	UARTegiregist*/
#de#define	r (clear)00620terrupt B Re RegBBIO_MASKB_S	2_FLA* F specifythe ADI BSP specifyt Holding Or */
#defxFFC00100DRegister (Confr */
#IE34  /* Flag Source PolERBFmer 2 od Reg		0xFFCCorenter	TIMER0FullIO_INEN	T0 TMASKB_S	40ETBEmer 2 fine S		0xFFC>#defi**e	FIO_M8e	UAR/

#ifndef 


/****		0xLS
#defin     /T0_TCLKXLL	 imeer (0xFFC00100 s */
#definfine	/* PeripheORTck DiTCR20 Transmit 80imer  of piORTs */
#definI34  /* Flag Source PolNIN/* SP/* SPORT0 Tt) *egisteORT0 TX Data RChdog		000800  /* _Tle RegRX			0xFFC00818  /*G0xFFC00314  Fransmit CloUCR1_PT0 TX Data ider lag M	0 Flaefine	 SPORT0 E0824dD			0xFFC00608 IrDAIy spTransmit Clo POLHoldinSync D0_RC	TX Pol100 - 
 *
gV/
#define	S2Re of piORR0_CONe regR	0xFFerru*/
#define	SPORT0xFFFty - 0xFripherForceG_D					pt B ROnPORT0 TrFS lag e Sync    *73		0xFFC0080e	Sr */Set	on BOTdefine	S3ster *		0xFFC0gula		0xFFC00800  /* Fimer 2 Coansmit Con0_RFS14  /* In1onfigurit Con SPORT0 TX Data 5FF)ter */
#define	SPne	SRX			0xFFCer (set) */
#d  SERIAL 0xFFT0_IIiod Register	   O_FLA/
#d/
#defiS B Rx_TCR1	0xFFCpt MaL			0xFFS5FF) ADOW			0SYSX* SPORT000134	/* Inteer ** SPORT0 

/*  /*nalt B 0 Flag r SPORdefine	SPORT8DTYDivier)
 00620TX* Tim* SPmattORT0   /* Fla (clear) 	TIMT_NORMlding reister (/
		0xFF N Synl
 * L>


/****) */ULAWegister */
Npins */
#sORT0u-LawCS2			0xFFC00848  A			0xFFC0Culti-
 *
 * L>


/*A0848  cSIC_IMASKTLSBI
#defi			0xFFTX Bit Ordeiver */
#defiITF Frame UART0_FC00848  ct r */ORT0 Sel T0* SPORT0 Multi-CFS0  /* 4-BF538/fine	S5ster *TER ired SPORT0 Multi-ChanneDPORT0 TX 8
/* SYI#defIndependKB_Tefine	Ser 0 */	0xFFC0MORT0_MTCS3	Ldefine	_LPB* SYSowore regSe	/* SPORT0 M Assignment	ReAdefine	ine	VRSYL2C	/Clove Select Register 1 */
#definlCK* Fl_WIDTH	

/***0 FlagFall	SPORdgeRT0_MTCS3		0114  /* IR- 0xe	TIMER2_WIerrupt Assign
#define	SPORt Reg0084-ChannSPRT0_MTCS3			0xFFC0CS0			0xFFPORT0 MuPORT0sk Register RT0 TMTCS3/
#define	S4) */
# Interru* InRe2ister 2 */
0xFFC00208  /* W0710  /* FlSLEN0xFFC00stem Conf0 Mul


/*TX Inte rupt A	(2 - 31Registe	FIO_MASKA_C*0114  /* In2Flag Mask SPORT0 TX Data 1der *DI#define	SPO9ear) */
#define	FIO_MASKA_S			0xTXSTCS1		od RORTX0xFFC00ar00800  /*Afigu0908  TSFcSPORT0	0xFFCKB_TtereoRegister 1 *1_TXIV			0xFFC0 RegRFS
#defin) */
TX Re GP-First4IC_IA/* SPOxFFC00118uration 2
* TimeRegister 1 *RPORT0 Tatione	WDRgister lect Register 1Rne	SPORT1_TCR1				0xFFC0R SPORT0 MulRT0 Multi-Channelransmitefine	SIC_R8ster *e	SPORT0_MRCS3			0xFF		0xFFC00848  * SPORT0 Multi-Cno* TimeFC0062S */
Register  */
#def
#define	SPORT1_RCLKDIV			0xFFC00928  /* SPORegister 1 *nsmit Confign 1 Register */
#dePORT1_RFRRT0_MTCS3			0xFFCR	0xFFC00928  /* SPOion 2Rync Divider */
Rit ConfiFrame Sync Divider */
Rdefine	SP****FC00858 C009			0xFFC1 CurrRT1 Current Channel efine	SIL	SPORCHnnel ivider 			0xFFCter */
#define	SPORT1_RCLRennel CoFC00928  /* SRT1_MCMC1			0x#defRT1_MCMC2	05FF) ve SeTAT			0xFFC	0xFFC024T1_MCMC2	
#define	SPOr */
#define	SP#defineCMC1		4FF) */
#define	UA90xFFCfigurat9pt Mask Registere	SPORT1ORT0 Tr Register ter *RCMC1	>


/****/* Interru1			0xFFC008vider */
#define	SPORter */
#define	9rogrammabhannel Transmit Select Regislock D*/
#define	SRefine	SPORT1_TFSDIV			0xFFC0i-Chann1 Multi-Channel T /* SPORT0 Receiv
#define	SPORation 1 Registe0090C  /* SPORT1 Transmit Frame SRn SPORT1vider 0090define	SPORTPORT1_RX			0xFFterr1 Multi-X Da00904gister 2 */
#ddefine	SPORRPORT1_RXR		0xC  /* SPORT_MRCtaonfiguration RegisterSPOMultCR12	TIMEefinlect Regist1 M*/
#defin     0xFSIFO Notne	UART* Timeterrupt /* SPOV_SHADOW	fine RX Underf0xFFive Frame Sync DiviOter 3 RCSSync RXMultiTAT			0xFFC1r 1 */
#defTXr Register */
 Fra0858O_FLA Multi-ChaExternal0xFFCeceiv DividTe	SPORer 2 */
#def
/* External CS0			0xF	    */  /* SPORT1 Multi-Cha00AFF) */BT0 TranTIMER0_PTX Holde	SPORT0_R08er 0 *xFeceive2		MCMC /* SPORTrupt B SPOWO20  /gister 1defi/ 1 */c ense LWindow OfferioField0  /* SPORT1 M/* SP	Macro */
#define	UA /* SPORT1 Multi-Channel TraET_gistit Select0	/* 3F Interrune	WDOG_ste	/* SPORT1 TranEBIU_AMBCTL1Only	useAssignSIZEMultiA With Logic OR While	Seter 1	Ler *00904   Status RegiratixFFC00624	er 00090x)>>0x3)-1utem IU_S<< 0xCL			0xFFC00A10  SDADOW			Size = (x/8)-gister94C  /* SPORT1 er 2 WiSDRAMransmit Sel0A18  /* SDRAM Refresh Ra/* SPORA Registe			0xFFC00500 - 0xFFC005FF) TL			0xFFC00A10  SD SDRer */



/* IC_IAR1			0xBankraffic Control Registers (0xFFC00SDRRC)PORT


/* C  /* S			0xRefresh RisterFFC00624	/* Time */
#defin5 Assignment Regisr */



/*0r */
#definet A RegiCC_F_VERSIO3define	WDOG_#def0 FlagRec	UARy_TFSDIV			0xFFC82REC_BYPASSORT0 Multi-CBypassLKDIV	(No Atch n2C	/efinRegister   */ames2FROM Selecefine Spatibil 2 MHzode comfrom 4ight 20ider * Copyright 208_TCP16 (0xFs Rider_PERPORT8DMAC0_TC_CNWatc	16DMAC00C00CNTti-ChaInc.MCDTXnfigurati     ster */

/* Inc.FC00830  upt /* SPORT0 ReceiMCDRr */



/C  /*  Inc.
 *
 * L0 NKDIV			0iptor Poitaturation RMit ConfiSPORT* DMA Channelve SelKDIV	MASKB_S	FC00834PerFSarit00508onfiguration RegisterDMxFFC00oster *RelsterWship Channel 0 StF	UAR0xSiod Re 0 Configuration ReDelayERIORIOD_CONFIG			_ransmC dges Res (0xFFC00B000xFFC*/

/* = 0 Copyright 200_Xpt B _MRCS
#definexFFC00START_ADDR			0X M/

/* Register */2ne	SPOR3g MasY_COUNT			0xFFC00C18	/* DMA2dify Register */3/
#d3Y_CONFIG			OUNT			0xFFC00C18	/* DMA3dify Register */4C  //* SPCONFfy Register */
#define	DMA0ter */
#defter */5/
#d50 Y Modify Register */
#define	DMA05dify Register */6/
#d60 Y Modify Register */
#define	DMA06dify Register */7/
#d70 Y Modify Register */
#define	DMA07dify Register */8egister 2 */
fy Register */
#define	DMA08dify Register */9/
#d90 Y Modify Register */
#define	DMA09difivider*/
#defiransmAnter RegYPERIOD Y Modify Re1 */
Inc.oX_COUNT		0xFFC00Cpt B BDMA Channel 0 Current X Count RegistRT_ADDR			0Y  /* 1hanneCDMA Channel 0 Current X Count Regist
#defi	TIMEInc.
 1*
 * DDMA Channel 0 Current X Count Regist_CURR_DESC_PT */
1*/
#dter  Channel 0 Current X Count Regist/* DMA Channel 0 1#defi* DMAChannel 0 Current X Count Regist2e	SICel ReLatch (HiC PARALLEL	PERIPHER/
#dOD		FACE (PPI) Flag Mask to directly der */_RCLPPIX_MODne	DMA*/
#ERIODO_BOTH			0xFtORTh (Lonsmit ConfPPI Port Flag Reright 200_X_MODIFDIRRT1_RCLKDIVces Inc.
Drupt	DMA*/
#dransmit CloXFR_ansmiefine	SIC_RT0_PORT0xFFCTypl 1 DMAer */
#definCFG Coun31 Mulces Inc.
/* InteMe	DMAI Modify R08	LD_SE  /*gister/* DMA ActnterA10  /ne	SPORT1_TCR1				PACKF0 Y Modiannel nc.
 /* DMA TFSDIV		/*TimevxFFC0verster  of	defde <a.h * Alneously 00314dedt 2032_X_CO 320xFF			0xgulatoRegister   */
KIr) * Transmit CoAddrSkip ElemKB_T	0xFFC/


/* Ppt MaQgistO
#define	SPOC6nt Regisven/Oddnc.
 *
 rame Sync DivDLENGTH
 * L	0xFFC0C68	IC_IArupt A */
#def640	/* DMTX			0x*/
#de/*	 CHIPIDMaerrupt	n registV* DM=START_ADDR			nter C3egisnchrossignment	upt A 	0xFFCSDRAM Bank ContChanneify Register *X Modify
#defY_1 0 CurModify R7nt Regishanne1* Copyright 200Count Re2ister */

#define	DMA2_*
 * fine	DMA0_CUFFC00C80	/* 3ister */

#define	DMA2_*/
#d2ESC_PTR		0xFFC00C80	/* 4ister */

#define	DMA2_FC008FY Co_Y_MRegister */
#de5ister */

#define	DMA2_ 	SPOR 1 PPTR		0xFFC00C80	/* 6ister */

pt Controller (0xFFC00100 -* DM0xFFC007/* A9ank Cont7u s */11)ERIOe CHIPIDMFC00C80	(only worTC  */
x=10-->x=16xFFC0094C  /* SPORT*
 * L2 X  /* DiR		0xFFC0Copyright 20nt Aegist0C70	/*sfy R9NT			0xFFC00C18	/	DMA_Y_MODIF	0xFFC00Cdefine	FIO_MASKA_S			0xPOLC/
#dRegispyright igne	SP SPORTiesSPORT0er */
#define	m In0x0C20	/* DC68	0 Flag* SPORT0 C_PTR		0xFFC00L	0xFFC0000904C68	ve Select Rnnel DescripTUS		MA2_SPORT0ansmit Seleefine	DMA1_STxFFFL0 Modifine	SPOA10  /Inddth oannel C68	es38	/T_ER* DMA 	0xFFC0ve SelTrO_Dt	ReBOTH	Edges RegOVriDivi_MRCS2		0400 */
#definStatus Register *UN/* DMAelect Reter *emoryransme	FIO_BOTH			0xFEdefi1 InteIDTH	538StatusDetec0xFF2-200giste* Copyright 202NCOr */annel 1		0xFFCe	SPCorCurrster */
#define	Sync Divider */
C 0 N
nt RegT0 Transmi/* SPORT0 Multi-Channel
 * Co		0x(0xFFIG,PORT0_yyne	DMA100A04  /* AsynchroDMAATU/
#dit Confr */sterclear) */
#define	FN#defineRCLKDIVer Regis0 Current (W/R*Register   */rDtatu_8gist0 Y sk Interer */8 bSDRAM Bank ContFFC00CA /* 400  Regiyright 20316	ONFIDBR	 */

#defC1 */
30738 nfigurait Select 32T1_TCgister 2 */
X CoDRT1_RCLKDIV	2D/1D*MA ChanneCopyright STAR Interrg InpuRestarnse or the GP IR		0xFFC00C80	/Dr 1 */
 /* SPI0ne	SPORT1_TCR1				0ITscripto		0xFtoX_COUNT		nnel 0IO_MASKA_T			0xFFCNFC00Cter *9r */
#Next	R			0xF****er */ount RegistegistY_


/*	UART0_40	/* DMA C*
 * L3 Y	MA2_400  /Autob/
#deRegister   */*/
#def/* SPter */A3/
#define	DMA1_ST*/
#d Current Y Co*/
#defA3_X_smit Coegister */
#define	DMA3FFC00C40	/* D*/
#def3 RegisR_DESC_PTR		0xChannel 	0xFFC/
#define	DMA*/
#def			0xine	SPOegister */
#define	DMA3/* DMA Channe*/
#def5X_COU0138	/* Inter* Copyright 203_MA Ch_IRQ_STA*/
#defMA Ch6	/* DMA Channel 3 Peripheral A0A ChannelUS*/
#def7xFFC0	/* DMA Channel 3 Peripheral /* DMA Channe*/
#def DMA CPORT1_egister */
#define	DMA3START_ADDR			*/
#def9ter */
#deD3_CURR_DESC_PTR		0xFFRRt AddE4	/egisEXT_FLO* Infy R28	ine ent ernap	Re_Y_MODIFY	_RCL4 Ne

/*
ster *			r */
#deripheral A Chan_X_COUNT	AUhanner_MRCS2		Inc.
 *
 *00DUART0_Inc.
 *
 * L4ORT0RRnel 3 _COUNT		Inc.
 *
 * ArrarecDOG_	 */
#defi_X_COUNT	4Mfine0xFFC00C0114mPORTy splIG				0x/* IListORT1_TC0xFFC00954  /* SPOLARG0 Y Mel 3 Y rLargter *CMA4_CONFIG				0xMA2_Y_COUADDR3 C	/*DR					0xFFC* SPORR0_PERIOD			0xFFC00608eral S		0xFFod Reg */
#defe	DMA4_CONFIG				3ORT0rCE8	/MA2_YChannel  /* el 3 Peripheral  Channe0 Y Modir */5er */0	ESC_PTR		0xA1_Y_MODIFY	ster 6ount Regis Register  Current X CoDnt Regist		0xFF70D2ices Inc.
 *
 * LFlag Regist0 BXT_DE00308  Con,nterruptCER	  MA Chr */
#define	Dister *_D	0xF/* DMA Cha 0 NDon Reg*/
#define	Channel nnelDMA4_CONRCLKDIV5FF) *	FIOster */
#dD 1 */
or PoFE*/
#defC_PTR		0Inc.
 *
 * Fe		0xFFster */
#define	DMA4hanRUConfiguraConfigCKCNnnORT0nel 4 Cue	DMA3_t Register */
#ster Dount R Channelgister */
#define	DMA400D2C	/r */od Reg/FC00138	/* Inter* Copyright 2040	- IMA4_Cfine Ster */
#d3_CURR_DESC_PTR		40C082_X_MODIF0C70	MA4_Cter */MA Channel 
#def Add_X_COUr */OUNT*/
#define_MAPter */
#deChannel 0 Star00A04  /* iRegi 0x0Fansmit*/
#define	Sel 2r RegisMA Chaefine	DMA5_NEXT_DESC_ ConfIC_IAR/
#d0	/			0xFFC00CAgister 2 */
#d_BF5POS*/
/*ODIFY		MA ChaCAPFC00_PTR		0xF CMA 838	/*OInterfy RMA Channel X Count RegC/* D* InDMA Channelfine	6D5ices Inc.
 *T0_M0xFFC00D28	/* DMA /* DMX_COe	DMA4_CON/
#d Channe */
#5e	DMA4_CONFIG				5RR_DESC_PTRWSTART_Aine	SPOine	DX_COUxFFC00D58	/* DMA CCo* DMA Channel 5DIFY			0	0xFFC00t DescdX_COUNT	5egister *gister */
#5C	/MAIC_IAR1iod Reg_Y_M00128	/* InPTR	A10  /* Ster ADMA1Enc Cloc 0x0Fefine	er Reg (0xer */
#defineFY		_PD38	/* DMnnel 5DMA1FFC00nel 0MAA5nel 5 _H
#tor PMRCS20RXDMA Channeler */38	/* ister */4TUS			0xFFC00CAgister 2 */TPI0	CP-BF538 5 X Count RPORT0_TF*68	/* DMA Channel 5 Inter1			0xFF00CAMAP		0xFFC00Dster */
#ne	DMA1_P0D58	/* DMAthe A1upt/S_COUNT		0/* DMA_NEXT_X_COUNT	5 4 Cu */
#dster */
#dIransm00904_X_COUNT		Iap	Register */
#defin	0xF0xFFCel 4 Curreer */OUNT	heral Map	Register */
#defin
#defiDine	MODIFY		8	/* DMACuext Descri00CE8	MODIFY		MA Channel 0 Sta38	/* DMA Chan Current Y Co		0xFFC00C2RXChann00C98	/* DMAT_ADA6gisthannel Count   /* SPORT1Regis DMA CT4	/*Regi_LPB */
#8e	DMA4_CONFr */
#8ices Inc.* Copyright 20DMA 3Rdefine	DtatuCONFIG				0xFFC3C_PTR		6DMA4_Y_MODA Channel 5 Inte3nter Regis0 Y ConfiguratiNext D8	Register */
#def6it Select RI1efine	DanneCONFIG				0xI1ne	DMA5_CURR_Register */2efine	Dscrier */
#define2 Start Address Regist /* D6C	fine	D*/
/CONFIG				00090hannel 6 Start Address Regist /*  Addfine	DODIF#define	DMA5_CUR6_X_MODIFY			0xFFC00D94	/* DM	0xFuration RegDMA#define	DMA5_CUUNT			0xFFC00D90	/* DMA Channannel		0xFFC005 CuChannel 6 Next DD88	/* DMA ChannC
 * Liit Selecster *GENSPORTPURPOSConfiguratioAnsmit Cfine	SPORT1_RCL/* IntPWMog TimerReg the ier Reor Pointhe Arrupt W	nsmit Coegister 3 */
nteNtion Regt ConfTUS			0xefin	er */
#defineNFIG		_DLH	 RCLKDIV		0xFFC00C70	 Current Y Coor Poi	0xFFCdefine	T_DESC_PTR		0x
#def00DA40	/* DMA ter */
#d38	/* DMA Chanter */
#define	SPxFFC00Del 6 ConfiguratiPEter 1 */
#MAP		0xFFC00DAC	/*TIMDISer Registe_NEXus RegisR2_WID0xFFC00954  /* SDISuratiel 5 Nexter Regis7gisteBices Inc.
 *
 *DIS68	/* DMA Y			define	DMA7_STel 6 ConfiguratiCigurnnel 0 Curter */
#B8C008FIFY			0xFFC00D9hann00DC6 Start Address RegSPORT0urrent X Couefine	DMA6_CILC00DC0	/* DMA MA7_ST D8ate of pixFFC00818  tor P Register */
#MA7gistR	DMA 4 X Modifgister 2 */
C4	/* DMA Chan00D90	/*0xFFC00DD4	/* DMA ChanOVF0xFFCMODIFYel Trangister Coural ister (16-bA7nnel 0 Current X0xFFC00e	DMA4_CIFY			7 6 Start Address Regiopyright 207_YNEXT_D/
#defi00D90	/*/* DMA Channel 7 Y Modify RegRU		0xFFC0nclude aC_PTR		Slav7_CONFIGester 2 Eices Inc.
 *
 DMA6_Ctatus ReSC00DDC	 6 Next Descriptor Pointer Regiter 05FF) *COUNT		/0D90	/* 6 Next Descriptor Poin7 Next Desc_CONfine	DMA7_CONFIG				ILA7_X_fine	Dster */
#dILit Select nnel 0 Current X */
#define	SPORT1gister *_		0xFFC00800  /* 		0xFFC00Sync Divider */TCR/
#deIC_IARDCent Address RCxFFC00DFDUS			0xFFC00CAxFFC00DFE* DMAAl00848tider */
*/
#egistes Proviegis6 CuBltag0 - 0Cpt Aescrir */
#define	DMAC0_(TOVLDMA7_X_ster */
#6 Y Modify Rne	MD1A0_D0_NEX#define ChanxFFC00E20	/* MemDM Current X CoFC00EMg MaskMA7_X_EC	ine	DMA1_ster */E0_P 75_NEXT_DES***********Destinadefine	D0xF2_P* DMA Chanine	DMA1FC00314  RAM Bank ContiWM_OUT	0xFFC00D */
#defineTH_CMA1_ister egister 10XT_T0_MT0 (0xFFFFC00D94	/ULSE_H38	/* DMAster 1 */
#RIOD_ss Regfigist 1 */
#defQ) */
0Dcripto7_CONFIG			IC_ISRDModify  DMA Chann#defineister04e	DMA0_PERILKMA2_Y_CWDOG8fine	DMA1_0xGGFFC0Y Coun1MA7_CONFIG	EMUerrupt Destin8	/* DMA Ch/
#def9ices Inc.
 dresTYP0xFFC00714 & Regiodify R4)er */
#define	E0Eunt ReMemURR_A********DMA0 StRegi
#define	SIC_IAR2		0xFFC0rrupt EnatODE TransmC  	/* MemDMA0 SttFC00800  /* figu DMA Chader */
#define	SPD0 Next De Channe	SPORT1_TCdefine3_CURR_DESC_PTR		7n	Y					0xFFC00800  /* 0xFFC00e	DMA7_CURR_X_COU0_D0_Y_		0xFFC00714  pt /* MemDMA		0xFFC00E/
#d* riod Flag Mask 900E14	/* 	0xFFC0ation	Y 
#defiDLL  0x00000STAT			rrupt /Mask to directly toInterurat- ChannelI/OChannel 6 Y Modify Rne	*
 * Li 		SPIulat(B Re)00A04  /* Asynchroation	 Regist Modify PF Register Register *iC4	/* DMA ral Map Reg3 Register al Map Reg 1 */fy Register *iPFupt rrupt Wakeup RdPF_X_COUN0	/* Modify PF7ExFFC00
#define	MDPF5ne	DMAioor P_Y_MODPF9or Pointer 0_Y_MODIFY00DD8	4xt Des*/



/*Descriptoefine	MDMA0_emDM_D0m 0 Destinatio	/* DMA Cefine	MDMA0__D0_Y_C2Counne	MDMA0_ptor */
/FC00CAddressPORT1_TC/
#define	/
#defineFE40	/* DA0 Stream 0 escriptxFFC00Cnter 		0xFF0xFFC00Cnter3		0xFFe	SPORT1_TCF4OUNT		0x
#define	MDChannel0800  /* PF6		0xFF	SPORT0_TCPF7Channelinter	0	/* M		0xFF Next Des	0xFmDMA0 S		0xFFC00****		0xFFARegi	Channel			0xFFB* DMA Channe
		0xFFices Inc.
emDMmDMA0 SMA ChanunE4050	/* MD0_IRQ_STA 0  Channerrupt 0xFFC00954  /* SPOM  GPIO* DMA Channel 6 Y Modify RxFFCatus ort	FC00A04  Eter */
#deCunt	Regi0xFFC4FFC001unt	Reg	0xFFC00C Re/
#der Regis#define	CCURR_D0_Y_MODIFY		CCnfiguFC00E14	/* 		0x_D0_Y	0xFFC00Eurce CC00C80ion RegistFC0Ce CHI	0xFFCSannel 0 ine	Po6-bit) */
#0E5er */
#d ChannE30	/*e	FIO_hannel/* DMA ChannMA4_CY Count Regnel 4DESCCn Regist SourcE4er */
#defCne	Mce Current D Inter	SPORT1_TC CouDMA3_X_Mfine	DMA1DMA5_CURR_DESCntroine	MDMA0_DrrentDe 6 Start Address RDodify RTR	0xFFC00EMD0D2C	/0xFFCion	CurPeam 0 SoModify ReS0_Dunt	RegisT		0xFFC5CD */
#define	MDMA0_D0e	FIO_BDMA2_CCOUNT	DMA0 Stream 0 SourDMDMDMA0_#def X C0_D0_Y_


/**FFC00E5ChannD1MA7_Y_m 0 DestinatD1l 4 ne	MDMA0_S0_PND1t Reg	MDMA0_D0_STAD1			0#define	MDMA0_D1ter egistptor Point		0xFFC6fine	MDMA0_S0_****egistream 0 hannelDR			0xFel 6 Next ent YQ_STATUS		0xFDam 0C00D90	/* DMAarityister 2 */
#d 1 NDMA0_S0_P
#defim 0 DestMA0 St4	/DC	/* MemDMA0 Stream 0 Sou Regist AdDA Channe	Next DescXtream DMA7_CONFIGDE	0xF/
#define	MDMDS		0xFIO_BMA2_Y_CODA0 St38	/* DMA Chanfy Re Count	Regis0D

/* PART0DESC_PTREfine	DMA1_X_My DMA ChannelS0_CURRE0E68	/* MemDMA0 StrEMA0 Stream 0the ADIEtCOUNT		0xFFC00D38	ERegister */
#0_S0_PERIPHERAL_MAP	0xFFC0E */
#define	MDMA0_DEm 0 Source PeripherEl Map Register */
#PERIPHation	Next DesEtion Regist0xFFC70	E*/
#define	MDMA0_D EA0_D1_START_ADMA2_YEhannel 7 Y Modify REnation	Next DescY_CEtion	CurrenEine	DME		0xFFC00E98	/* MA0_D1_STAREnt Y Count RegistEr */

#define	MDME0_D1MemDMfine	DMAE_MAP	0xFF DMA CMeEannel 5 ConfiguraEination	Next DescEiptor	Pointer	RegED1_X_MODIFY		0xFFC00E94	/START_ADDR_DESC_PTR	0/
#defiEA0 Stream 0 1FY		0EFFC00E5FC00D90	/* E	0xFFC00D38	/* DMAam 1 DestiD10_S0_CUEL_MAP	0xFF_X_MODC00EA4	/* Memer (set) */
#de	MCivider 7 Current Address1_XSel 4 Curr	_D0_Y_C	0xFFC00D54hannel 0Iister */095C  /* SPORT1TIMOart Addster */TER				0xICurre_CUion Register */naDB*/
#R_X_COUNe	DMA	0 Coueriphen	_NEXT_s,trollWhen  MemDO_FLASync Divider Register */
#od Re	C00E5Modify D1_Y_MODIFART_ADMDMAefine	800 - 0xCURR_DESA3_NEXTFC00el Regis	ChannCOUN,p	RegUntilt ChanUNT	0xFFC00EB8	/*

#define	MDMA*/
#dMA0 Stfine	MxFFC00E56 Start	 MemDMA0 StrT	0xSZER 0, 1,ct Regisnd	Zero (efine	MDM	UNT	0r	Poid A0_S/L */
ne	DMA4_Y_MODGdefiORT0 Multi-GCA4	/rrrupt Ma0 Stream ,MDMA0wve SeDiscardRT_ADDR		0xFFCPS SPORT1t Regi	4	/* D-T0_MTC	/* Dnnel 3 Peripheral Y_EMISPORT0ister */6		0xFFC14  FFC0Outce Cerrupt/Statu/ne	DMA5DestS0_CURR_Xof		0xFDMA06/8*FC007Register   */LSBD				0unt  /* SSB Channct Register 0 *CPHA0DD4	/* D4 6 S28	/* hase */
#defin/
#defiurceor Pointer D28	/* DMA Des/* SPORT1 M- 0MST/
#deegister *Ya14  / 6 NeFSDIV			ter 0 *Weam el 6 Next Deodify Regn Drain0 SourcNext Descr(Higivider0C20	/* DdifyIOnt CG__POLAegisl 5 CFL_IRQ_STATUS			0C	/*FLSDMA0_ 5 Next Descrs (=1)ine	 4 NUT1 as f	SPIo0_S1_/
#dV* Fla0EC8	si-Channel TransmiFLSl 4 YCOFC00DD4	/*  MemDMA0
#define2OUNT		0xF/

#define	MDMR0_PERIOD			0xFFC00608Count	ion	IestinatL_MAP	0xFFE/
#define	M3 Stream 1 Source Current Address Register */
#defin			0xeam 0 SRQ_STATUS		0xFFC00EE8	4 Stream 1 Source Current Address Register */
#definter ister *DRQ_STATUS		0xFFC00EE8	5 Stream 1 Source Current Address Register */
#definMA Cha
#defiRQ_STATUS		0xFFC00EE8	6 Stream 1 Source Current Address Register */
#defineam 11 SourcRQ_STATUS		0xFFC00EE8	7 Stream 1 Source Current Address Registefine	MDMA0_GFC00DECent AdA1_S#def (=0)Chandefine	MDM	DDR			0xFel 6 NMA ChaMA0 Stream 1MA0 Stream 1 DGl 4 Yine	SPOMA/* MY			define Source C StNT MDMA0signment	Coun/* M_S0_CU MD0	/* M_S0_CU
#ion	Iount ReD0_X_COUNT MDMA0_D0_X_COD1_C#define MDMA_D0_X_MODIFY MDMA0_D0_urce CoFY
#defi	MDMAChannel 0_X_COUNT MDMA0_D0_X_COPChanne MDMA_D0_Y_MODIFY MDMA0_D0_Y_MODIFY
#define MDter l 2 Peripe	DMA1_MODIFY
#de#definMemDTR
#define MDMA_D0_CURR_ADDR MDMA0_D0_CURR_ADDR
#MA Cr */
#deD0_IRQ_STATUS MDMA0_D0_IODIFTR
#define MDMA_D0_CURR_ADDR MDMA0_D0_CURR_ADDR
#Sourister */D0_IRQ_STATUS MDMA0_D0_IodifDMA_D0_X_MODIFY ount	Reg_MODIFY
#dX_Mine	MDMA0_S01Bitr Retream 0 Source CurdressChann/* DMA  CHIPIC00EE4	/* MemDMACOUNT		0xF Source Current Address Register */
#definSefine MD*/
#define	MDMA0_	/* MemDMA_MODIFY MDMA0_S0_X_FFC00CE8	/* D	0xFFC00D38	/* DMA Chefine MD_D0_tion Register */
US		er */
#define	MDMA0_	/* MemDMthe ADI BCOUNT		0xFFC00D38efine MDeam 0 DestATUS		0xFFC00EE8	SC_P0_CURR_0E58	/* Mine MDMA_S0_CURR_DES_NEXT_DESC_PTefine MD5UNT MDMA0_S0_X_COUNT
#fine	DMdefine	MDMA0_D1_Fream 0 Source Curr#define MDMA_S0efine MD6A Channel 7 Y Modify RegiERIPHERAL_MAP nel 0 Cu_PERIPHERDIFY
#define MDMA_S0_CUefine MD700D9C	/* DMA Channel 6 Y Modify R0_X_MODIFY DMA0_D1_CURR_STATUS MDM_D1_START_ADefine MD9_X_MODIFY OUNT MDMA0_D0_X_COUONFIG MDMA0_D0_Y_MODIFY MDMA0_D0_Y_MODIFY
#define MDdfine MDMASC_PTR MDxt De_NEXT_DESC_Pxt DeMDMA_D0_X_MODIFY MDMA0_S0_NEXT_DESC_PTModifdify Refine MDMBDannel 0 C_MODIFY
#dnel 0 CMDMA_D0_X_MODIFY gister *_MODIFY
#deffine MDMA_D1__X_MOfine MDMxFFC0D0_IRQ_STATUS MDMA0_D0_Ie	DMA1MDMA_D0_X_MODIFY Mdress ReSTATUS MDMA0_D0ne MDMAfine MDM			0NFIGl 5 X Coun_MODIFY
#dl 5 X CounMDMA_D0_X_MODIFY nnel 5 Current_MODIFY
#d4 Cufine MDMEMAPDMA_D1_IRQ_STATUS MDMADMA0_D1_X_MODIFY
OUNT MDMA0_DDMA_D1_IRQ_STATUS MDMAnel 0 Cefine Mrent SC_PTR MDMA0SC_PTR
#define  Cou_D1_START_ADDR MDMA_ADDR MDMA0_S1_Sdify Regi/* SPORTxFFC00EAC	/* #defA Channel IMemDDMA_ wMDMAdify


/le-wunt tY_MODIFY Time	DMA7_Y_Mt Y CouOD_START_el TrD1_Y_MODIin a mSourc	dMRCS2PHERALsome oth/
#d 5 Curtr2_CUto beter *_MODIFchronous Memorgister */0	/_D1_Y_MODIFYS1__MODImis MDM occ2_PEwModino new dptor fy RCRegist10  / SPORT0 S0 Stream 1 DMDMA0_S1_*
 * e	SIC_Ister 2 (0=dify Re1=O_FLtyY Count RegiBSYModify Regi_PTR MDMA0_S1_#defins rDIV			dne MDMUNT Mf Stream 1 Sourcfine	M MD4	/* DMODIUNT M0_D1_PERS1HERAL_MAP
#define MDMA_D1RAL_MAP ne MDMA_COC_IAR3*/
#defiem	erioDMA_, co REGISParalmay have	beRAL_MAP DEt_PTR#define MDMA_S0_CURR_Y Assignment Register 2T_ADTR
#ion	NUdefine MDMA_D1_IRQ_STAT Control Registe2TR
#dBX CoX Count Registe1	UART02 Control Registe3TR
#d7#define	PPI_COUNT			0xFFC03 Control Registe4TR
#E120  ine	PPI_COUNT			0xFFC04 Control Registe5Point	/* giste Start Address Reg5 Control Registe6TR
#
#de01010	/* PPI Frame Lengt6 Control Registe7TR
#unt 01010	/* PPI Frame Lengt7/
#defask Int(0xFFC00100 - gist  ASYNCHRONOModifMORYD28	/* DMA Count 0DAer */
#defidefiPERA10  /*
#defi0_S1_CONFIG
#defAMCKG
#define MDMART0_TCLKLKOUTencrrupt Mask BE_MCR			pyright 20AllDMAC0sine	DMAS /* S01401 */
r PoeBegister #defineodify-Chael 0 Staremor****fine	nt ReL-2 _SLAVEF) */
#0x_BUS		tionC00DD4	/* DER		PERIOD			0xFFC028	/*  0 &	1_D				0WI0_SLAVE_e MDMA_S0_C_BS1_CURR84	/* MemDMde* DMA Channel 5 Configurai	DesrolleFFC00C40	/* DSTAT		SIC_IAR3	COUNT MDMA0l Register */
#define	TWI0_(aT


MASTE	2E*****/
#define	DMACDPRIegister _CURfine has pri ModiFStatucMA_S Poiex00848  aCPeripFC00EB8the AnFIO_MASKmDMA0 Stream 0 Source Curestinater */
#r */
#dLAVE_CTRL_COUNT			SLAVE_STAT		tion	Y Mt ConfMode Status _D0_CURR0	Mast,FC00A- bdify R-3MASKMA0 dConf1 -38	/* DMA0_D0_NASTER_CTRL	0) */
#0xFFC08140	TIMWI0_FIFOter Interrupt M 1 */
1IFO Channel&1ter */ST,	 0TWI0_define	WDO	/* FIFDR		0x	TWI0_X	TIMEFemDMA0 SESC_Pgister */
#define	TWI0_XMT_D1xx -nt AddFFC00(a Do 0,	1, 14C40	/*)ngle ByC	/*Interrupt Re SDRgist /* H	Edgeine MD0RDYster (seS0_CURR X Co00D64	/*DYWI0_XMT_DA=gister UNT
_SWCNT	0xer *				0xFF	C00854  /ta Dout Register 2 */
#define	highWI0_aA1_ST	lowUNT
0_REG
#defi0148
#defiC	/*Ton	Cu_IAR3			0x4_D0_CONFIG MPORT0eup Regist			 */COUN	todefine	Regicycupt Mhe following rt A_IAR3			0x8Regulator Contro Counter RegiWI0_e	TIMER1_C	 TWIC2070C 
 followin			0xFT0_CON_IAR3			0xCSTAT
#define	TWI0_INT_ENABLE	 TWI0_INT_MASK


/* 3ne	WDl-Purpose DMAt_PERI0_Y_Mistere X STAT
#define	TWI0_INT_ENABLE	 TWI0_INT_MASK


/* 4efine	GPIO_C_CNFG			0S arTL		 Voltag1O	PinNFG		 Chaetu0	/* DM	 TWIAOE asser_PTRtoI0_IN014	/* m 0 Dest=0070C NT_ENABLE	 TWIIST_SRCSK


/*IN2egis 0 Destination	fine	M X M_C_I0	Maister 5ister CRT0_e X M /*  G_C_D			0xFFC01510	/* G400  /*1	0xFF	3	0xFFC01530	/* Set GPIO Pin	Port C Register */
#define	GPIO_C_T		 GPIO Pin	Port NFDBR		 Regi Por/* GPIOData	Register* Set GPIO Pin	Port C Register */
#define	GPIO_C_T		Port0_D0_NEXT_1ster * HPIO	Pin Port C4Data	Registerster NABLE	 TWI0_INefine		dr GPegister *ort  Dit Selectdefirt C Register */
#dHfine	GPIO_C_S	8IO_Dter */
#dO Pin PoART0_INEN			0xFFC01gister */
r Pointer RegCONFIG
#efine	GPIO_C_CNFG			0HPIO Pin Port	CCin Port D Data	Register */
#define	GPIO_D_C			0xFFC01524	/* Clear GPIOrection Register */
#dHTr */GPIO_C_INEN			0xFFC0156a	Register */
#define	GPIO_D_C			0xFFC01524	/* Clear GPIO0efine	GPIO_C_CNFG			0RAPIO	Pin Port 1EN			0xFFC0156COUNTt C Regine	M#define	GPIO_D_D			0xFFINENne	GPIO_C_SR2			15	Reg*/
#define	GPIO_D_/* Dt Pin Port D Register *INENO Pin Port	3egister	Names */
#define	GPIO_E_Crection Register */
#dINENne	GPIO_C_I4egister	Names */
#define	GPIO_E_CIO	Port D Register	NamINEN	/* MemD#defegister	Names */
#define	GPIO_E_C5 C Register *1 */
fine	GPFC00E5X 16-begister	Names */
#define	GPIO_E_C60xFFC01538	/* Set GPIO PinY CountDR Megister	Names */
#define	GPIO_E_C70xFFC01538	/* Set GPIO Pi_CURR_YN******ister	Names */
#define	GPIO_E_C80xFFC01538	/* Set GPIO PiE31 */
fine930	/* Set GPIO Pin	PoE SPORT0 	0xFFC01530	/* Set GPIO Pin	Po0E88	/* MeAegister	Names */
#define	GPIO_E_C unt Register */
#definin	PoC	/*gula00B0 - 0xFFC01BFF) */

#define	DMAC1_efine	G		0xFFC01B0C	/* DMA emDMA0 Sn Po - 0xFFC01BFF) */

#define	DMAC1_t D Data	Regist1 */
INEN		A0_D1_01B10
#dester 4FF) */
#dnel Tfficeprecat DMA Cort D Registes Regison	Cur AlteA0_De deprecated	register names (bet C Register * Set GPIO Pi/* Mem		istetibility */
#define	DMA1_TCPER			Data	RegisInput SetgistWn	PoD SPORT0 er */n ReMA0_D0_NEXT_Define	GPIO_ral Crt C Register */
#d GPIne	GPIO_C_Modify ReDR MDMA0_S1_SData	RegiR_DES Pin Port D Register * GPIO Pin Port48	/* Timeter */
#define	DMA8_START_rection Register */
#d GPIne	GPIO_C_r */Register */
#define	TWI0_Mtion	IIO	Port D Register	Nam GPIIO_C_T		PC_SWCConfiguration Register */
#definA8_STARTInterrup1FFF)DIV		n	egistE RRR_DESC_Pess Register */
#define	DMA015xFFC00Tar) *GPIO_C_ GPIY_COA0	/* e	DM1 */
ess Register */
#define	DMA150 StreINEN			0xFFC01 GPIupt RT1_TCA6_DESC_Pess Register */
#define	DMAgisterA8_Y_MODIFY			0x GPI	DMAC1_Enaet GPIO PY Count Register */
#define	ister names (beatus RDIV		unt	RegisDDR			0xFess Register */
#define	DMA*unt Register */
#defin */
#precated	efine	NamFC01C24	/* DMA Channel 8 CurY Count RegisteCORT1t C RData	RegiB1ear 5 X Count Registe1crip */
#defines RegisteMA8_STAe	DMA4_CONer 2 WiAlt 1 Nne	DMA8_PERIPHERAL_MAP		0xFFC01C280_S0_CURR_MA ChannelAP		0eguldegulater ne	DMA8_PERIPHERAL_MAP		0xFFC01C2 Contro	/* DMA ChannERAL_1			 */on	IFFC0ne	DMA8_PERIPHERAL_MAP		0xFFC01C2FC01C00	- 0xFFC01FFF)t RegistSidefiLL_CT5 X Count 1TWI0_FIFO_SWI0_RCV_DATA16nterrupt Mhe followin0xC0082xFFC0100FF) C_PTR		9gister DESC_PTR
#de	 TWICLKDASE	e	GPIO1_TFSDne	VR_CTL			0xFF10000	Pin Por		0xFFC01C44	/* D	TWI0_INT_ENABLE	 TWI0_INT_MASK


/* efine	GPIO_D_D			0xFDBR	ne	GPIO_C******ster */
#def9it Select Regis_PER			0xFFC01B0C	/* efine	GPIO_C_CNFG			DBR	O Pin Porear unt ReC00DD4	/* DMA Channel */
#defin94 Y Modify Rrection Register */
#URR_Ae	GPIO_C_INEN			0xFFC01nt Register */
#define	DMA9_X_MODIFY			IO	Port D Register	Na1INEN			0xFFLL1 Tra0xFFCDESC_P* Set GPIO Pin	Port C Register */
#deorefine MR			0xFFC01B0C	/* DMA Dprovided Divie	GPIO_terna4#define	DMA31Ctream l 9 X Count Reg_S0_CONFIG
#define MDMA_S0_X_COUNTC38	/* D			0xFFC00D58	PIO_C_T		PogistCr	Nameurrent Address Registe0	/* DMA Channel 6 _COUNT			0xFFCl 5 X Cou9 X Count Re6 Start AdGPIO Pin	Portontroller A8_STAnnel 9 Current Address Register */
#define	DMA9_IRQ_STATUX_COUNT		0xFFC00CCOUNTe 0x0FFFF00t) */upt/Status Rea	Register */
#defi#define	MD_D_C			0xFFC01524	/* Clear Gices Inc.
 *
 * L8TR	0xxFFCDR			0xF*****MA Channennel 2 Connt Address Registeodify Register */
#define	MDMt/St0_NS			0xFFC01C68	/* DMA FC01B0C	/* COUNTel 10 Next Descriptor Pointer Register */
#define	DMA10_START_ADDR		0xFFCChannel 9 Peripheral Mhannel 9 Irt	mDMA0 Stream 1ces Inc.
 *
 * Ler Register */
#define	DMA10_START_ADDR		0xFFCefine	DMA9_X_MODIFY		ine	D		 */
#R_DESC	DMA9_0 SourDM */

#define	DMAC1_ine	UArrupt Wakeup tor PDR			0xFC01C6gister */
#define	DMDDR		0xFFY_DDR		0xFFC01C68	/* DMA EN			0xFFC0A0_S0_Count Register */
#define	DMA10_Channel 9 Peripheral MY_MODIFY			t DescCount Register */
#define	DMA10_Register */
#define	DMe	GPIO_C_T	efin
 *
 * Li	SPOS0_CONFIG
#define MDM		0xFFCess Register */
1OUNT			0xanneDR			0xFFC01CA4	/* DMA Channel 10 e	DMA8_Y_COUNT			0xFFlear irectionC28DR			0xFFC01CA4	/* DMA Channel 10 	DMA8_Y_MODIFY			0xFFlear _CURR_YFC01C4Count Register */
#define	DMA10_e	DMA8_CURR_DESC_PTR	1 DestInput ODIF0E44	/* Mel 7 Current Address1_COUnter Register */
#def	DMA7_CU Chane	DMegister */
#define	DMefine	DMA10_Oe	DMA5_CUR1 Register */
#deStatus ointer Register */
#definent Y Count Rnel 8 Interrupt/Statutor Po00840  ess	R DMA Channel 10 Current Y Count Re#define	DMA5_CUR1C940	/* er 2 WigistA Channel1ONFIG MDMA0	/* DMA Chegis */
#dter */
#define	DMA318 Curreter  r */
#define	DMA11_CONFIG			0xFFC0ent Address Register */
DMA ChannFFC0rrent X Count Register *R MDMt DescCurrent Address RegistChanneAL_MNT	0xFF0nel 11 Sta0
#define	DMA8_STAnel 0 Current X 1Cnel 8 FIG
#define1Y Couupt/Status Re
/* DMA Controne	DMA11_CONMA Channel 8 Periphee	DMA9	0xFFC0A Channel 7 Y Modify Registe1 5 CChannel 9 Peripheral MP		0xFFC01ster */ Modify Register */
#define	DMA11/* DMA Channel 10 Currer *ices Iess Reg Modify Register */
#define	DMA11FY			0xFFC0Y			0xFFCtor Poi	0xFFC00ne	D Modify Register */
#define	DMA11.
 *
 * Li0SC_PTR MDMAer *AP		0xClear G Modify Register */
#define	DMA11e	DMA9_e	DMA6_CURRMA7_C DMAP		0xX_MODIF Modify Register */
#define	DMA11 5 Next Descre	DMA9_B0IPHERAL_Murrent10 Modify Register */
#define	DMA11Register */
#d_PTR MDMR_Y_COUNCUCBAP		0ister */
#define	DMA11_CURR_X_COUNDMA0 Stream COUNT			0xL_MAP		0xFe	DMA1_ * Lii-Channel 6 Start Address Regter */
Source Current t RegisterMA12_NE00	/* DMA Channel 12 Next DescriptDMA Channel 11 StNEXT_DESC *
 * Li Nine	DMA11_CONFIG			0ine	DMA4_CONFIG		8	/* DMA Channel 11 ConCET MDMA0_D		ne	DMA12_CONFIG			0xFFC01D08	/* DMA Cfy Register */
#define	DMA38	/* DMA ne	DMA12_CONFIG			0xFFC01D08	/* DMA CFC01C00	- 0xPointer 81CF0	/_FLA_S1_CONFIG
#defB20xFFC01C40	/*ne	TWI0_ceive FrINT_ENABLE	 TWIPointer Register */
#define	DMMA12TART_ADDR	Dou	UAR0xFFC01Der */
hannel 9 Start Address Register */
#define	DMA9_CONFI2				0xFFC01Ctage Regulator  Streamnfiguration Register */
#define	DMA9_X_COUNT			0xFFCefinne	GPIO_C_S	TByte ine	DMAl 1_Y_COUNT		0xF Source Current Addres0xFFC01C54	/* DMA ChaefinO Pin Port	Cta	RegisxFFCA Channel 12 Current Address Register *rection Register */
#efinter */
#define	DMA9_Y_MODIFhannel 12 Current Address Register *IO	Port D Register	Na2INEN			0xFFC0156	0xFFC015302CURR_X_COUNT		0xFFC01C70	/* DMA Channel 9 Current X Count AL_MAP		0xFFC01C6CUCONFIO Pin	Port_PERIPHERAL530hannel 12 Cr */
#define	DMA9_Y_COUNT			0xF1IFY _Y_COUNT MD#defC_PTR		0xFFC0 ChannChannel 9 I_COUt Register *ister */

#define	DMA13_NEXT_DESC_PTR		0xFFC01D40	/* DMA Ch_PTR		0xFFC0ster */
#dap	Register */
#define	DMA9ister */

#define	DMA13_NEXT_DESC_PTR		0xFFC01D40	/* DMA ChR		0xFFC01D40	_X_MODIFA9_CURR_Y_C
#define	Der */
g In* DMA Channel 10 X Count Register */
#define	DMA10_X_MODIFter */
#define	DMA8_ Cur5xFFC00
#define	GPIO_D_DMA13_NEXT_DESC_PTR		0xFF 4 Y Modify Regis	0xF			0xFFC00D58	/* annel 13 Next Descriptster */
#define	DX Count Re	0xFFC01D58	/* DMA Channel 13 Y Count Register */
#define	DMA1dress	Register */
#defne	DMA10_X_COUNT			0xFFC01C	0xFFC01D58	/* DMA Channel 13 Y Count Register */
#define	DMA1efine	DMA9_X_MODIFY				0xF		 */
#defta	Regisr	Nameefinedefine	DMA10_Y_COUNT			0xFFC01C98	/*nel 1NEN		* DMA C1D18	/*	Namer *1ap	Register */
#dannel 13 Next DescriptEN			0xFFC01Eit Select RegisERIPHERAL_MAP	0xFFC01dress	Register */
#define	DMA13_Y_COUNEDMA ChannelERIPHERAL_MAP	0xFFC013_X_COUNT			0xFFC01D50e	GPIO_C_T		r */
#define	DM#define	DMA13_CURR_X_CFC01C00	- 0xFFC01FFF)RT_ADDer */
#define	DM	/* DMA RIPHERAL_MAP	0xFFC01e	DMA8_Y_COUNT			0xFFRegis8	/* DMA Cptor Pointer RDMA Channel 14 Next D	DMA8_Y_MODIFY			0xFFRegisC	/* DMA Cine	DMA14_STARDMA Channel 14 Next De	DMA8_CURR_DESC_PTR		DMA3_0_Y_COUNT			0xFFC01C98	ADDR		0xFFC01D84	/* DMter Register */
#def				0xFPIO Pin B 1 Interrup1BDMA Channel 14 Next Dline	DMA9_PERIPHERAL_MAP		0xprecated	register namesdefine	DMA14_X_MODIFY		el 8 Interrupt/Statu* MemDMter */
#ices Inc.
FFC0define	DMA14_X_MODIFY	#defes Inc.
 *
 * Li3 MDMAral Map	RecompatibiliecNEXTine	DMA14_X_MODIFY	FFC01CF0	/* DD7ices Inc.
 8 Current define	TWI0_IN*/
#define	DMA14_Y_MODIannel1D40	/* DMA Chne	MDMA Channel /StatusA0_NEXTer */
#define	DMA14_CUMA10_START_ADDR		0x4_RT0 Trl 11 /* DMA Control 13 20xFFC01C28	/* DMA ChannT_DESC_PTR		0xFFC0OUNT	Source Current Address RR		04	/* TWI0	Ma*
 * Lannel 13 Next Descript_DESC_PTR		0xFC00D90	/* DMA IPHERAL_MAP	0xFFC01DACdress	Register */
#def_NEXT_DESC_PTR		0x/* InterruIPHERAL_MAP	0xFFC01DAC3_X_COUNT			0xFFC01D50A11_CURR_A_DESC_PTR		0xMA2_YIPHERAL_MAP	0xFFC01DACne	DMA9_PERIPHERAL_MAP		0xpt/Status Registe CurModine	DMA14_CURR_X_COUNT	rent Address Register Register FFC01DC0	/* DCountIPHERAL_MAP	0xFFC01DACnc.
 *
 * Li0xFFCrt	Aderipheral MA15_START_ADDR Channel 15 Next Descript_X_MODIFY			0xFFC014 _Y_MODIFYister */
#define	D8RIPHERAL_MAP	0xFFC01DACC_PTR		0OUNT		0xFFC0gisteF8	/* Dress RegisteG			0xFPHERAL_MAP	0xFFC01DAC1

#define	DMA15_NEXT_DMA14_IRQ_STATUS	l 5 X Count 15_X_MODIFY			0xFFC01DD			0xFFC01Cnnel 8 el 0 COUNT			0xFFC00D18	/* D8_PE15_X_MODIFY			0xFFC01DDhannel 15 Configura_NEXT_DESC_PTR		0ine	DMA14_STARMA15_Y_MODIFY			0xFFC01D/* DMA ChART_ADDR			0xFFC8ter */

#define	DMA13_N15_X_MODIFY			0xFFC01DD */
#define	DMA14_CUR1DB8	#define	DMA15A Channel 1MA15_Y_MODIFY			0xFFC01DC01C00	- 0xFFC01FFF)3fine	DMA4ices Inc.
 *
 * L9 3ster */
Source Current Address Register */
DMByte 1_Y_MODIFY			0xFFC00 Sou_COUNT			nel 9 Start Address Register */
#define	DMA9_CONFIn 1 L_MAP	0xFFAL_MAP	0xFFC01efine	DESC_PTR		0n Register */
#define	DMA9_X_COUNT			0xFFC15_Regist1DE8	/* DMA ChMA2_Yes Inc.
 *
 * Li		0x */

#define	DMA13_0xFFC01C54	/* DMA Cha_CUR01DF8_Y_MODIFY			001D80	 DMA Channel 15 Current Y Count Registerection Register */
#_CURter */
#define	DMA9_Y_MO DMA Channel 15 Current Y Count RegisteIO	Port D Register	Na38	/* DMA Cher */
#define	DM32 Current X Count Register */
#define	DMA12_CURR_Y_COUNT		0xFFC01D38	/* DMA Ch Reg/
#define	DMAupt/Status TART_A0D94	/* DMA 0xFFCrent X Count Register *ine	DMA CurreA10_START_ADDR		0x6_NEFC01C44	/*C_PTR MDMA0_S0_CU#define	DMA16_X_MODIFY			0xFFC01E14	/* DMA Channel 16 X Mod			0xFFC01E14	/* DMA Cap	Register */
#define	DMA9#define	DMA16_X_MODIFY			0xFFC01E14	/* DMA Channel 16 X Modify Rgister */
#defCON	0xFFC015_CURR_ADDR			0xC783egister */
#define	DMA13_X_MODIFY			0xFFC01D54	/* DMA Channel 13 X Modify Registeel 6			0xFFC01D9C	/* 0gisterrent Descrister */
#define	DMEMap	Register #define	D/
#define	Dfy Register */
#definPORT0ss	Re/* DMY			0xFFC01E01E28	/* DMA Channel 16 Interrupt/Status Register */
#define	D			0xFFC01E14	/* DMA Cne	DMA10_X_COUNT			0xFFC01C01E28	/* DMA Channel 16 Interrupt/Status Register */
#define	Define	DMA9_X_MODIFY		3RQ_STATUS		C_PTR		0DR			0xFEC	PHERAL_MAP	0xFFC0	T MDMA0_S0_Y_COUNT
#d16_IRQ_STATUS		6 Start Addreefine	DMA8_S	MDMCurre
#defnnel 5 Current 0xDMA16_IRQ_STATUS		rce Periphnel 13 Next IFY			T_A			0xFFC01E14	/* DMA C	/* DMices Inc.
 *
 * Li	SPOgister */
#define	DMAel 16 Current DescriptURR_Y_COUNT		MODIFY			0xFFC0gister */
#define	DMA * Copyright 2005 */
#d Count Register */
 Channelgister */
#define	DMAe	DMA8_Y_COUNT			0xFFefine		0xFFC00D38	/* DMA Changister */
#define	DMA	DMA8_Y_MODIFY			0xFFg	Coun 15 Y Modify Re Count Rgister */
#define	DMAe	DMA8_CURR_DESC_PTR	CURR_nnel 17 Configura */

#dl 17 Y Count Register ter Register */
#defDMA Ch0	/* DMAAL_MAP	0xFFC01Dgister */
#define	DMARegister */
#def15 CuXDMA Chter */
#define	DMA8_ST0	/*annel 16 Interr	RegT			0xFFC01D98	/* DMADMA Chgister */
#define	DMASTne	DMA17_IRQ_STATUS		01_CONFIG			0 0 Sour
#defin DMA Channel 16 * DMA Chae	DMA17_IRQ_STATUS		0170_S0_CURR_ERAL_MAP	AP		00114  /* IY			0xFFC01E14ral Map Register */
#der */
#defT		0xFFC01C38	/*/* DC00DD4	/* DMA Channene	DMA17_IRQ_STATUS		0xFFC01E68	/* DMODIFY		RegisterDMA1_Y_MODIFY			0er3er */
#define	DMA11OUNT			0xFFC01CDcriptoDMA C/

#define6 Start Addres#define	DMA17 DMA C */
#def0xFFC01E44	/* DMA Inc.
 *
 * Lier R_Y_MODIFY	pyright 200DMA11_Y_MODIrent X Count Registern	Re			0xFFC01D9C	/* i-Channster */
#define	DMA18_el 16 Current Descripto1
/* Paralle_CURR_X_COU	0xFster */
#define	DMA18_ister */
#define	DMA10_STA#define	DMAtion Registerster */
#define	DMA18_16 Current Des*/
#defit Register */
#define	DMA11on Register DMA11_Y_MOD * Copyright 200ipheMO0xFFC01E5C	/* DMA Cha_D_T			0xFFC01Register */
#de_NEXT_DESC_PTR		0xFF7_ 18 Configuratio

#define	DM Count Register */
#deADDR			0xFel 6 Next De01CURR_Y_nnel 18 Configuratiodify Register */
#defiine	DMA18_CURR_7FC01E6A Channel 11 ConDvices Inc.
_CURR_X_CQ_STAne	DMA18_Q_STATUSAL_MAP	0xFFC01nnel 13 Next Dene	DMA11_CONFRQ_STATUS		0xFFC01EA8	/fine	DMA18_Spt/Status RegiXT_DESC_PTR		0xFFC01n ReRQ_STATUS		0xFFC01EA8	/ne	DMA17_CURR_X_C_X_MODIFY13_NEXT_DESC_PTR		0xFFC0RQ_STATUS		0xFFC01EA8	/RegistT		0xFFC01C38	/* E90 DMA Channel 18 Current X Count Register */
#deine	DMA18_CURR_2r */
#define	SPORT1_RCLol PeriCONTROL		0xFFC01define	DMA10_START_ADDR		01428	Interrupt SD_MASKR1 TrxFFC01408	/*SCT_IEC	 TWI0_INT106xFFC00 TiSCLK[0], /SRASs	ReC
#defiWE, SDQM[3:0]er Regie AddALe	DMA17_IRQ_STATTA16STATUS	SCONFencyADDR		0xFFC01E84	/* DMACLMDMA0_S0_Y1#defic.
 *
 * Lice/* Interrurection Register */
P* Fla01D30	/* DMAc.
 *
 * LiegistprT	0xRegistnnel 19 NeIC_I2 Current Y tus COUNT	D0C	/*er */
#dFC01D2AMC rER BIT_Y_COUNT		0xFFASR Regi_IAR3			0xFFatus Re4ine	DMA Registds Reg01ED46 X f-		0xFFCEcriptor PoNext Deine	D01D30	/* DMdefin Register */rolle1 Ar1_TF* DMA Channel 11 SENEXT_DESC_PTR		0xF1Perient C_PTR		0x738  ler nel 13 Nexl 17s			0xFFC0FC01EE0	/* DMA Channel 19 CurTRASxFFC01B0C	/* DMA17 PeriphtRASY_COUNT			0xFFC01C98	/egist
#define	DMA13_T MDMA0_S0_Y_COU0xFFC01C54	/* DMA Chegist* DMA Channel 1T MDMA0_S0_Y_COUrection Register */
	0xFFCe	GPIO_C_Iine	T MDMA0_S0_Y_COUIO	Port D Register	Negist1D78	/* DMA10	/* DMA Channel 6  */
#define	DMA15_IRrrent er */
#defunt Regist
 *
 * Lice	DMA8_Y_COUNT			0xFegist */
#define1t 2008-nnel 5 Curre	DMA8_Y_MODIFY			0xF	0xFFC	Register CONFT MDMA0_S0_Y_COUe	DMA8_CURR_DESC_PTRegiste0_Y_COUNT	20	/* DMA Channel 6 nt Descriptor Pointescriptor Pointer MAP		0xFFC01C2 * Lic/* DMA Channel 17 CurmDMA1 O	Pin Port 2ent Y Count Registennel 18 Current 8_COUcriptore	GPIO_C_Sdefin MDMA0_S0_Y_COUNSC_PTR MDMA0_S0_CURR_DESCA0_DgisteDMACne	DMA9_PERIPHERAL_M_S0_CURR_1DDR
#de
#defineA Che	MDMA1_D01Fl 8 PeMDMA0_1e Cu* Copyright 2008-			0xFFC/* Me	MDMA1_DOUNT
#define MDMA_1_ED0	/rent Y Count RegiPIO	Pin Port DESC_ MDMA0_S0_ds codT_DESC_PTR		0xFFC_ADDRe	GPIO_C_er */Dest
#define MFC01PTR MDMA0_S0_CURR_DFFC00D64	/fidefiFFC00E5* MemDMAR0	/* MemDMA1 Stream 0 P
 * LiceP  /* SDation	Y Count	Rine	MDMA1_D0_X_MODIFY	Pine	DMA14_C0C84ine	DMA14_START_T	0xFFC0C_PTR		0xFFC01E0ter */
#de0 Startion	Y Count	RC01CF0	/* DCURR_Y_
#dePFFC01ED0	/uratint Address RegisESC_PTR	0xFFC00Er RegiCurren	0xFFC01F18	MA1_D0_Y_COCefine	DMA18_Y_MODC6C	/* DCnnel 	0xFFC01DE8	NT
#define MDMA_egister */
#define	MDMMA Cha ChanneX_COUNT
#define MDMA_0	/* MemDMA1 Stream 0 IRQ_ST	DMA17_IRQ_SNT
#define MDMA_ine	MDMA1_D0_X_MODIFY	DMA CddreMDMAX_COUNT
#define MDMA_ination	X Modify Regisnter/
#d(0xFF/* DMA C#define MDMA_define	MDMA1_D0_CURR_AIRQ_STADestinX_COUNT
#define MDMA_m 0 Destination	CurreWR	/* DMA Ch X CounxFFC00E5XWRC01CF0	/* DFter */
#defWRe	MDMA0_Define	DDMA0 Stream 0COUNT		0xFFCCOUNT		0xFFW0_RC17 Per		0xFF/

#def_STARTMem
 * LiceC00DD4	/* DMAUPration ne	MD Reg/*B0	/od Reg RegidChannel 0 RR_Y_COUourcnter Registe Count	Ree	DMAter RelicncCURRPrechPoin, modene	MDMA1_Dset, 8	CBR rDMA Cha
#define	MDMA1_S0_SSFC00DF4 Current Addres_MODA1 Stream 0 DestMA0_S0_Con	nextDMA14_CWI0142	0xFFC01CF0	//* As8_CURR_DESl 13 FC00DCDMA1 Stpheram 0 SouAP		0D28	/* DMA XBUatus Reg 11 Y CoI0_SLAVE_C14ister T X Counstinie	SPO#defiine	DMABBRster *DMA8_CURR_ripa	regack-toC004u	rPHERMA5_IRX_COUNTtream 0 Sourgster ine	DMA18_NEXT_ExtisterMDMA0_17 Periph01F50	/* Mem* DMA TC			0Pointer RegiA0 Sempr */
ens(0xFFegiseam 0 Souvalue 85 degModify_CONFIG		dDBG		0xFFC01F18	/A0 Stistiphenel 13cer Regs duster bus granOUNT			Vext	Descam 0 e	DMA19_START_ADDEB		0xFFC01D38	ne	DMA18_CURR_egistefine MDMA_MODMA0 Stream 1 SZ/* InGPIO_C_INEN	 Count	Re50	/* Mem Chansr */
#16MBIFY MDMA0_S0_Y1F6ster */
		0xFFC0MemDMA0 Streagister */
#define32_X_COUNT
#define MD60 DestinatCge RAL_MAP	0xFFC01F6C	/* MemDMA1 S6* MemDMA1 Stream e	M12C00C80******6 MemDMA0 Streagister */
#define	28NT	0xFFC01F70	/* Me25gister 01F04	/* MemDMA1*/
#ine	SCOUNTer */
#25R_X_COUNT
#define MD5nfigura Reg00isterFine	DMter */
#defMemDMA0 S51C_PTR	0xFFTUS MDMCAW_D1_START_ADDESC_PTR	0xFFrent X Count Rcolumn a	DMACouwidD40	/* D Register */
#dtion	Next Descripht 2008nel 15 Interr	A ChannY			0xFFC01E14	/* DMAM9r Regfine	DMA11_CONFIG	1el 4 X ModCOUNT		* MemDMA0 StrT	0xFFC0FC00D90	/* DMA Channel 6 estination	Currefine	M/* DMA Chan	0xF_DESC_PTRA1 Stream 1 Destination	Configuration RegisterFr	NameMDMAY_MODIFYRAL_efine MDODIFY_MODI*Eegister */
stination	Cur0_CUis idInterrupt/StatuDSation	NexxFFC01E44	/* DMARAL_MAP	0xeam 0 Sours er */
#_PTR		0xFFC01PU	0xFFC01D38	/T		0xFFC01Dine	MDupter */
#xFFC01F90	/* MRMODIFY
#01F04	/* MemDMA1MA0 Y_COUNT	on	Conf_Y_MODIFY			EA SPOF		0xFFC0NEXT_DESC_PTEAB	s RegyRegisrT		0nterr W1define	MDMA0_BG*****criptor PoinCONFB1_CONFIGl 12 Y MDMA0_S0_Y_COUNT
#n RegisteTWO-WIR Modifyt/StatTWIx* DMA Chafine	DMA10_START_ADDR		03_NE1FA4MA0 DIVrent De (Use: *p00E5	/* DMA =er ILOW(x)|CLKHI(y);	 Registpt Controller (0xFFC00100 -C	/* DMA C0xFFC00A1xt	Det	DeserioAddr/
#deIs H0  /_Y_CT		0xFFC01D38e	DMA3_*/
yollerFu)<<0x	0xFF RegisterBef1F90Newource Sl 5 Curr0xFFC0010C  /	gister */
#define	MD_COUNT
#define MDMA_efine0xFFC01E44	/* DMAefine	e	DMA6e	MDMA1_FC01F90	/* MemDMA1 S	0xFFC01E5C	/* DMA Chay Regdefine	SIC_IAR2		0xFFC0011fine	MA1_CUR1ansmit Select Register 0 *on	Curre_01F97fine	T19 SstreaFFC00934  xer */
ferD1_X_(10M			0& ADDRESS DEFI0 Stredefine	F538/9 S0_CURR_X60 Sour TimerCCupt Ane	MDMA1_	0xFTnnel 12 Next DMA4_CURR_ADDR	fine	er */
#nation	Ne Width Register	    *TUS		0xFFC0_CURR_ADCodify R
#define	MDMA1_SADD_LENR			DMAC0_TC 6 NexA DestinA1_CURRFFC00720  /*FDVAnt Degist2 RegiMA ChD88	/* DMADMA1ValiTAT		0xFFC014NA00E5C* InR0_CONNAK/ACK Str		0xFFC0A 17 CcluisterOf0_Y_MODIFY11_CONFIG			FC01C40	/ */
#defy Regl Call	ADMA16_	Mster	SPORannel 12 Y CouO_BOA4_Y_MO	0xFFC0095 FraRegiAddress	RegiisI*/
#defin01FDfine	MDMA0_1MODI	0xFFC01EA8PORT0_TF/KDIV			DMA11_CONFIG			Crent DeCoufine SP Destination DMAister */
intefine	0xFFCR	/* DMA Channel 6
#dept Mask ReA1_D1_X_M17_CURX CountA0_CONFIG				0xFFC00C08MStream 0 SourxFFC01X CountX_COUNT
#define MDMA_0xFFCsk InterrupC01FD0Stream 088	/* DMA0xFFC01EA8RX/TXRT_ADDR		0xFFCnel egisteMDMA0_SUsMDMA0_YA ChaT6 X MoSpecannel 19 Next gister */ */
#deIssu Y ModiCond Regisdefine	SPine	Mrent Desc		0x2odifpeater */
inatioop** MeEnd	_D1_NEXT_DESC_PTR	/* MeD Slave onfiguonfiguratyine	Doe	MDMA1_D1_CURe	MDMA1SDAFC01F Source PerierialChanneldefiTR	0_DESC_PTR	0xCLtion	X MTER				0x	EBIU_SDMDM*/
#dine	MDMA	MDMA0_D0_S1FE0	tream 1 Source Y 16 Current DePROGptor Pointer RegisteStream 1 De00DFgFF)			0xFFC00C08ipDR
#ine	DMChannel Lost	Arbit	DMA5_Y_MODIFY			(X1 Strbo/* MDescripC	/* DAne	MDM#define	DM01E00	/* */
#cknowledgyte Register */
#defiCurrR0_CON0E10	iguration Register   T1Q_STABUFRannelescriptorF*/
#def8CONFIRT0_CH* FIFO	ReceiUFW/
#dI			0	MDMA1_2004	/*A Channtatus Register *SD 1 Sou00500 - 0xt Address RegOTH	tream 1 SouMA Ch) */
#defrent AdURR_Y_COUNT
1 */
#LKDIVn	Curr2BUSBU*/
#d*/
#e	MDMASC_PBusy* DMACURR_DESC_PTR	0xFFMODIStreaT	0xFFC01FB0/
#defght 2008-OUNT MDMA0IN_MTCS3		Stream 1 Sou* DMA ChanMA1_D1_CUAT		0xFFC0142AG_el 19 CurrenA_S1efine	UART1_L	DMA17_CURR_ Stream 1 	DMA5gistent MA1_S1_Y_MODIr */
2	UART0_errupt EnCS0			0xFFR0_CONE8	/* hannel 7 Y Modify Retch C00138	 */
#de2rnatInterrup20FT1Perio */
#defi01C	Mdefinaud 	MDMA1_		(0xFFC02100 - te) */
#dgister *XMTSERV/
#definee) *FC00830  /400 Ser* DMAer */
#defineVnel 5 Startent AdKDIV			0nel 502ine	FC020CONFIG			0xFF	/* DMA Channel RegisFC00terrupFLUSHdefine MDMA_FC00830  FSDIV		Flu	0xFFC01D10	/* CVed u
#defi	efine	DPE#define	SPOR0FC00800 - 0xxFFCXMTINTam 0 SourC01FD02104	/* Interrup Register mDMA1 Stream 1 SouRCVdefine	UAR/
R0_CONxFFC00900 2104	e	UART21 */
#define			0xFFCerrupt  MDMA_S0_CURR_X Channel 19 Nexnt DesinaxFFC01F90	/* Mem021FF)	 */ptor Pointer RegiXMT_EMPTMA0_D1/
#definT2eceiv */
#deUNT	0xFFC00EB8	/*2_LSHALCOUNT


/	Namee	DMAFC00138	/Has 1 		0xART_AMA Chaon Register   FUIC_IAR3	MA0_D1_Line	Status ReO_FLA(2 LatcTART_AR		0xescrire reg	TIMERUARTCount mit CoByte) */
2Q_STve Frame Sync DiviCVS021FF)	 */
1xFFC00xFFC009	0xFFC01E58	/* DMA ChanSDIVT2FFC021FFC01FDRegisannel 	Divt) *FO_CTRLTR			0xFFine	DMA14_STARxFFC02200	nt ReL		0xFFC02204	//

#define	DMefinee	TWrnt XNT		0xFFC00E98	/* Sister */
#defi6 *xFFC0ransmitsk toerrupt A  ear) *COUNT
#define MDMA_Seam 00pt MA Channel 18 CoFC00904  /* SEN	OD			0xFFC00l	DMA14_CURMMSM22Count r Po2rrent DedreACTIV1_D101F1C	/* Mefine	TWI1_SDELAYter Introl Regfine	TWI1_NCMRXC0egister */
10ister */
#dRWm 0 TWI10  /TER_2te Rine	UAR2MT* Master Mode 4C00138	/* InterOTWI1_MASTER_80lu /*1 Coune	MDMA1_r */ Regiegistpheral428	Current X CARITdr */
#defi10C00138	/* IntRT1_LC* DMA E1ter RegisterAP/* Master Modtatu1xFFC01D24	WAKEUPe Controlr */02224	/* TWLMECHe Control*/
/lunterruptupt Assignment Registe10  MSBR		0am 0 Destller		s */
f9FFC01F1C	/* Mentrol RegY			0xFFC01E14	/*/* Mastepose PoCNT			0xFFC00B10	/* DMA CRegisPPLify R_defiasteCTRL	0xFFC0141TALCENgister */
#eister */
#ddefiRCOUNT			FFC01Fnel 15 Cur1_MPLLMSr */
#define	DMAC0_    */nterMUL Control R3C00138	/* IntPLF54	nnel 15 Curter Mode finee	UAR	0xF	/* FIFO	Rece Re1 CounE DMA */
#	/* 	UARAP		0efine	TWI1_er288	/* FIFO	RePointerMCL1_LC2fine	MDWI */
#define	Trine	annel 15 Cu		0xFFC01D38	/Tr ReRSTB0  /* */
#defi2Map	ReTW		0xFFC00are for backw	/* FIFO	RecY			0xFFC01	0xFFC01OUNT		0xFFC0MWI1_PRESCALE	  TWI1_CON0xFFC01D18	/* LL_CTine	TWI1_XMTTwo-Wss FIF22e Reltage Regulator CC_CONFIG		FC01C6FF) */
#		400  /*		(0xFFC02100 3FO	Ree	TIMER1_C */
#e	DMA9_CDR1#defin16 X M	0xFFMER_STATUSTransmit INV Point19_CURR_Yine	TWI1_XMTF */
1TranC_PTR		T		0xFFC01D38	DIViste0xFF	CoL			0xFF4C  /* S1 DeStreaX /

#define	TIMER1_COYNCI1_TD	/* DMA */
#d Count RegisterMA CFS	Control Reter */
FLG			0xFFC02304  	/* FFC0138fer Reg1 /* Tyte 			0xFF Register 0xFFar) */
#de51 */
#deegistertaAddresine	SPI1_BAUD			0xFFC02314102TIMERfine	TWI Doub Start 1ddress ReOW21FF)	 */
3 Conel 9 Start  Shadow	Reogrammabl1610	/2/* SPddress RegSPI1 BC Peripheregist4troller		(0xFFC02400 -12	0xF024FF)	 6troller		(0xFFC02400 -6	0xFFC02318 8troller		(0xFFC02400 -ine	* SPI2	Coe CurrPeripher2gisters (xFFC024FF)	 Chanller		(0xFFC02400 -egisgister *
#defe	VR_CTL		C02400 -	0xFFC023181 De)	 */
#define	Sgist- 4ate of pin3FF)	 */
#define	Sffer atuse of pin4  /* Peripherne	SPI2_ST50_D0_024FF)	1FC00704  /* Peripher2_S76ister */
#d Mase of pin2PERIOD		regce 1	(0fine	Tti-ChannelGBASE9_Y_ Reg
_DIV2ster */t) */SPI1 Contr02418  /*08  4_RDBR ShadowY			0xFFC01E14	/* DMne	Sster */	StatSPI2_CTL

/* SPORT2 Con16ster */el 6 SPI2_CTL

/* SPORT2 Con32			SPI0
#defSPI2_CTL

/* SPORT2 Con6I2ddressrent	egisl 1 Interrup2A Cha		ontro */
s reg3FF)	 */
#de4ceive SelI2xFFC00B1R_DESC2el Transmit Select RegLatcine	TW_CTL


/* CTL

/* SPORT2 Conisteine	TWxFFC0240r */
#define SPI		0xF		SPIter Reter */
#definePI1EGBASisterFlag reOd undC00810  / SPORT2504	/	Status/
#define	SPORT2_TX		fine	UAX_MODIF* SPORT2 TX Data Regisynchro
#defin/
#define	SPORT2_TX		PORT2 TSPORT0_Rount RundeRegister 1 ne	S4	/er */RT2 Transmit Clomit Select RegC_PTR		0xt Select Register 1  */
#dents Re2CS2		21FF)	 */
 */
#deS02ync DFC01FEegister WI1	Masgiste_er   2400  /* Statu* SPORT1 M2 TrLKsmit Confakeup Re 1 */
#define	SPORT1FC02204LOChan9_CURR_Y0xFFC022 DMA ClowingrC00A04  /* UNT	0xe	MDMAOxFFC01 */
#define	TWI1_XMTSTOMemDMAT			0xFFC02AVE_eceive	Data Double HOGGDFC00138	/* In22*/
#defasDIFY MDPERIOD			0xFFC4ider */
#define	SP2_CHN0xFFC00emDMAFFC00624	/* efine	SPORT2_MCMC1		0xFSHAPEFFC0ter Mode yte Rr backwaefine	DMAI0_INT_Eel 16 P_MTCS3		er */
xFFC01524	/* Cleel 15 Cu0062C	efine	SPORT2_MCMC1		0xFine	NT Multi-Chater */
2nsmit Cotrol ReT			0egist Registe*/
#define	TWI22MAP		0lowinRT2 Tn	CurremDM/
#define	DMA_ADDR	GBAS10gle Byte Register222PORT2 50 SourR1			0x 1 */
 Multi-fine	S/* FIFO	Re2_LS X Modify Rceive Frame	Sync r */
#definT2_MCMC10xFF2
#deELfine	SPORT7r 1 */
#defiCPster *fine	SPORETlockeive Fra	Addre***

/****2_Y_COUNT			0NI2A) */
#define	250	/* GSPNA2Controlle22C	/* De	Sync SBU2ation 1xFFCORT2 CurrenteBL2UxFFC00624	/* Timer 2 CPRUve Select RegORT2_MCMC1MCfine	_CONFIG	FC00138	/* InxFFC	SPORT2_MT	Receive	Data  */
#8	/* FIFO	Re224	/* TWSBine	SPORT2_
#define TWI1_AMDMA0PORT2_M3FF)	 */
#defineZ400 -TL)fine	UART2_GC#defCZT1_MCR		Syncster */


/*PERRsmit DCLK#define	SPORConHA3_X_COUTW01D20	/* DMA ChaL2HCOUNT		0xFFC0/* InterruWUfine	SPORTdefine	SPRT2 MuF40  ulti-ChIO_M	Register *F /* S	SPI0_CTL


/* xFFC01C0092er		(0xFFC0240ne	SP

/nation	Current	2_MCMC1C0094PCZSPORT3 r */
#deransmit Sel	 Multi-FIFO_CTRL	#define	930  /* S0		0xFFC025/
#defCMROFne	SY			0R1			0xF


/*CMTSntrolIMER 0, 1, 	SPORT2_60Cntroline	SPORT2_MCMC1RXRWRT0 Stivider */
#defineCRBion	Confefine	SPORT2_MCMC1BChan/
#define	SPORster */
#NV		0xF */
#def	TIM			0xFFC009	RENdefine	SPConfiguration 2MEN0xFFC	0xFFCine	SPO0262A2od Registe#definN1			0xPORT3_RCR1			0ine	TL2urce25			0xFSENC026ulti- FIFO	ReENTART_r	ssignment	ENr */	0xFFC025558LAG_CRT2 TransmitSB#def3_#define	MDATuratel 31D80	/* DR		0N04	/ir 1 */
#definhannelster 2 */
#de-Chan62C		0xF2504	/* 	SPO			 hannel Tra)FrameL2Hine	S32 RegiENransRT3 Tr3	0xFFChann#define	MDMART3_hannegisterMA7_Y_	0xFFC*/
#define	DMAration0xFFC00Dne	MDMAZ	0xFFZSPORT3__TFSDIhannFC02630		0xFCAL_MACMe	SPORT0xFF1	ORCR2			Oupt Assignme SC_PTR	Stream 1 DeMTer nregiORT3_RCR1			0ENratio2 0 CuORT3 Tne	SMA1 Stream 			0xFSPOter *it Framegist- 0xFgur				0xFFC009ider */PORT0 Receive Frame AP Controne	DMART0 Multi-ChA/* SratiDouble Byte RegistAPORT3_TF0xFFCCTL0		lti-ChaAPRT0 St
#def Multi-ChannelAPRCE024r */		0xFFC00608   APRPine	SPORT	Selecs0400  /* 0 Traff253Y			efine	TWI1_XM6OUNT	ulti-Channel	Receive	S2	TraMA1 Streamfine	SPORT2_MPORT3_FC00928  /* SPguration*/
#d2ART_00940  /* SPORT	2 0xFFC03_RCR1			0xFFCel Reg0x*/
#d3aud rate Rfine	SPORDte Regisdefine	S6(0xFFC02100 6*/
#dChannenfiguration 1 Regnnel	T */
#define	SPORT3_TCR*/
#disteefine	SPORT2_RFSDIV	ext De_CTL			0xFFCect	Regis*/
#d6Transmi SPORT0 ReceivexFFC02	SPORT3_TFSDIV		0xFFC0*/
#d73 Register 0 FFC0263C	RegistxFFC02520	/* Sration3T3_MRCXR		0(SCURR_X_COU<<	(4 * (x)))00940  /* SPOR2 Multi-		0xFFCPORT3_hannel 0 Xster 1 */
#defineannel	Tra26_STARTe	SPORT3_MPFFC01524	/ne	SPescr  RT2 Multi-Cct	R  /* #define	DMA001280	/* /

#def3C					0  /* M_D0_IRQ_STAnel Tr /* MP1_GCTL			0x*/
#dnel	*/
#d/SPORT3 Multi-T			0
#define	SP
#d0	/* TWIEBIU_SDdefine	SP4n	Curr******ter RegistFFC0062er */
e01E5C	/* DM	M**********01E5C	/* DFFC00623PORT3_xFFC01524	*************xFFC01524	FFC00624errupt Assignmenel	Tr71****** Data RegiFFC0062ce Curr0_CONFIG		PORT3_R******0_CONFIG		FFC0062MDMA1_S0_PORT0_TCC027 In******t Status RFFC00627PORT3_ext DescriVRti-Ch******7XVR_CONF 1 */
ENdia Transceivermit VRTCR1400  /Register****INTdiaPORT3 re reegisteine	#define	S7(0xFFC0Multi-Chtion 2 RegigurationACT0xFFC02528	ster */
#defSBSDIVt Configu8MA ChannGBASEPFFSDIVde PositionORT2 Current* SPI0	ConInterrup4FFC01D18	/* SPORT3_RCR1		DD0xFFC02528	 */
#defiurren 17 fine	TWI1_0 Maximum NurrePo			0ne	MVC252C	4	/* SP33 */
#define	UDAYod Regist			0xFFC00608   1eral Inte					0xFReceive Frame2728  /* MXVR Node Frameg1R_MAX_DELAT		0xFFC01D38	/Data Doublee	MX1	     LADD148C	/* FIFO	RecY			0xFFC0APMDMA */
#deSelect	Register*Prce Cdefine	SPORT3_MRCS0		A0C   e	SPORT3_MTCSMulti-ChannMDMA1_S0_CUSelect RegistCMGfine	r (setFC026303 ReceiCegistGroup	0xFFC00ards compCMPORT3_RCR1efine	SPORT3_TC		0xFFC00820600	/* SPORT3 Tnext	DesORT3_RCR1			0x Multi1RGSIP			SPI0_CTL


/* eam 0 SAL	/* egist604	/* SPORT3 TrPORT0 Multiiguration 2 RegiRRDgister 1/
#define	SPORT1_RWRgister 1define	SPORT3_MRCFefine	MXVRC_1	      0xFer *VR* MXVR No02608	/* SPORT3 TRemDM 4 X Modif/
#define	SP8	/NUMdefine	MDMA1egister 1 */
Multi-#define	SPO (clear) */XNUDESC_			0xFFC009F02rrupt B MXntrol     ALLOC_5	 FDMA Channel 1628	/MXVR Allocatfine	MXVR_AADDRC021ModeXUNT	0xSPORT3_MTAlloc6od Register				0xFF1	      0xFFC027r	    */
64	SPORTAlloth Ren TxFFC00HoldingPORT3_RC604	/* SPORT3 Trgist0xFFCransceiviguration 2 Regi/
#define

#def5 0 */
#de40  /* SPVR Allocguration 2 	      0xFFC0ine	TWI1_emDMA1 Enableefine	S    m 0 Destinefin58  /* MX */
#definFC02MENMXVR AlSPORT3 FFC00CD_EN_1	     A0XVR All260 StreInterrupt E* MXVR1XVR AllFSDIV		0xFFC0ntrolC	/*_STAXVR AllC02630	/* SSPORT2_  0xFFC3XVR Allivider */
#define	  0xFFC4onfiguratiHolding r276IC_IAR    5RT2 TransmFFC0276C  /* MXVR Allo6XVR Al   0xFti-Channene	SP* MXVR7nfiguratiY3_RX				0xFFC0T2_T Allocati */
#defin42554	/*VALIFFC00CRegister 
#define	ion TabXVR AlloInput Mr 6 *PMAXPORT0 Mult#define	MXVR_     M Alloc1      FC0276C  /74  1_D0egist0xFF02758  /* MXine	TCONFIdefine	MXVR_SYNC_LC2618     0xFFC02778  /* MXVCONFIG	nnelogical Channel AsL_MAPigmum Node FrRT0 Multi-M_SYNC_LCer *_LCHAN_1xFFC027giste2772410  er 1 *PORT3gistddreRdefine	MXVR_SYNC_LCLr 1 */
#define	MXFFC0276C  /* MX2780 PERIOD			0xFSPORegister 1 */
G780  /* MXVRefin0xFFMA Cr 1 */
#define	MXVR_SYNC_L0xFFC 1 */0275IC_Iable RegistA* MXVR Sync Data LogicaA*/

Mask um Node Frame /* DMA Chass Redefine	MXV3xFFC02778  /*t) * AlloC_PTR	0xFFC00**** RegCI		0xFFC0errupt Ena2 Multi-TraCIUrrent AFC00924  /* SPORT1 TrCIRegisteReceive	S Table RegisTrIU3     0xF
#defineh Regine	MDMA02780  /* MXVRog7FFC02778  /*CLata Logical C7Fine	MXVR_SYNC_Lster 4 */
#AN_7_r 6 *C0276C  /FFC02780 780  / Sync Data Lcate	MXV5r 9 */
#define	MXVR_CIU4PERIOD			0xFFCta LogicaFY			ss5PERIOD			0xFxFFC00B10	/* 278C 6     0xFFCdefinC0276C  /9 Mult7FC02780  /* MXVRData Logica1DD0	/* DM279Baud efinedify Register 	SPORT2	R SynxFFC02778  /*definee Curr MXVR Sync Data DM9   0xFFC Data Logical C7VR_ALLOC_1	2A0  /* MXVRMDMA0_DC0278 Address RegisFFC02780  /* MXV9XVR_DMA0_COU	0xFFC01C94	/* DMA 2780  /* MXSTART_ADDR  0xFFC0271ata LogicVR Sync Data DMA0 Star MXVR Allo Syncter */
#define	efine	DMA9_PENT	      0xFFC027CURR_ADDR  U Sync Data DMA0 LooSync Data egister */
#define	MXVR_DMA3_CURR_ADDR   0xFFC027AefinMXVR Sync Data DMA0 Startc DaFFC02780 	0xFFC00D38	/* DMA ChaRegisterss Rr */D1_Y_1_COUNT	 A8	Regisr */VR Sync Data DMA0 Starnc Data DMA0 Curter */
#define	PLL 	DMA14_PERI 0xFFC027A			0xFF1_COUNT	   unt Reg/* MXVne	MDCoR Sync Daegister */
#define	MXVR_DMA4NT MDMA0_1_COUNT	 B MulA0  /* MXVR Data DMA1 Start Addrp Count Reer */
#define	MXVR_DMA Loop Count   0xFFC027B4  /* MXVefine	DMAdress Rene	F	/* DMA Chant Address Regu	MXVR_DMA1_CURR_/

#define	DM27B8  /* MXVR Sync Loop CountCurrent Address Regiefine	DMA9gister */
#define	MXVR_DMA5_CURR_ADDR   0xFFC027A2/* MXVR Sync Data DMA0 StartAss2ata Logical Cegister */
#defineSc Data DMA0START_ADDR  0xFFC0272FFC02780  /* MXVRData DMA0 StarRegisfig	Registeter */
#define	/
#define* DMANT	      0xFFC02799 Loop Count Regs Register */
#define	DMA9ister */
#define	MXVR_DMA6C Sync Data DMA0 CurreDt Address RegisFFC02780  /* MXV2MXVR_DMA0_COUdress Register */
#A0  /* MXVROUNT MDMA0_1_COUNT	 2p Count RCurrent Address FFC01E		0xFFC0_ADDR  00xFFC027B4  /* DCURR_ADDR   nt	RexFFC02778  /*er */
#definnel 19 Next Descript	Register */
#define	MXVR_DMA* MXVR 7 /* MXVR Sync Data DMA3t Loop Count RegisterR Sync01E62efine	DMA9_PERIPHERAL_MAP		0xFF327AC  /* MX   0xFFC027B4  /* MX3ister */
#define	MXVR_DMA3_CUnxFFC025* SPORT3 TrFC027DC  /* MXV0  /* MXVR 1_D1_X_M Loop Countnt Address 3 Sync Data DMA0 LooCUNT	      0x7E0  /* MXVR Sync Dat1_S8 Sync Dat 0xFFC027B0  /	Recfig	Register */
#define	MXV3guration Register */
#deSync Da3trolDMA3 Loop Count ReB MXVR Al3urrent Address Refig	Register ** MXVR Sync Datater */
#define	1_COUNT	 F Mul_COUNT	      0xanneefine	MXVR_DM#define	DMA9_PERync Data DDMA3_CONFIG      0xFFC027D49C00D90	/* DMA Channel 6
#define	MXVR_DMEA0	/fig	Regist3/

#define	DMA12_NEXTSync Dat2_3Config	Register finexFFC0275VR 3ent Address nel ster */
#define02764  /* MXVDMer *e	MXVR_4MA3_Cster */
#t Loop Count RegisterDR  0xFFC028XVR_DMA4_START_ADDR VR Sync Data DMA5 Loop Count Registe1er 3 */
#define	SPORG			0A2 Loop Count Register */
#def4ne	MXVR_DMA2_CURR_ADDR   0xFFC047C   0xFFC02780  /* MXVT_ADDRur4ent Address Register */
#defineMDMA0_D1_Start Ater */
#define	ent Address 5 NT	      0xFFC027FFC027E0  /* Sync Data DMA0 Loo 0xFFC027BDMA3_CONFIG      0xFFC027D4efine*/
#define	SPORT3 Cu Config	Register */
#define	MXV4_DMA3_START_ADDR  0xFFC027D8  /4 MXVR Sync Data DMA3 Start Addr4ss Register */
#define	MXVR_DMAt4 Loop Count RegisterD   0xFFCDMA5xFFC00D90	R Sync Data DMcripFC027E0  /* MXVR Sync Datr */
er */8 ConDMA6 Config	Register */
#defReceive Frame Sync D			0egister */
#define	MXVR_DMA3_CU4efine	DMA9_PERIPHERAL_MAP		0xFF5c Data DMA3 Current Loop Count 5egister */

#define	MXVR_DMA4_C80xFFC0275 RegistC  /* MXVR Syncync Data DMA4 Config	Register Sync Data DM Sync Data DMA0 Loo280xFFC00XVR Sync Data DMA4 Current Le3egister 027E8  /* MXVR24	/* DMA CSync Dat 0xFFC03 Loop50xFFC027F0  /* MXVR Sync Data D5A4 Loop Count Register */
#defi5e	MXVR_DMA4_CURR_ADDR   0xFFC02Current Address C  /* MXVR Sync define	DMA9_PERIPHERAL_MAP		0xa DMA5 Curre_DMA57Register */
#dRegis*/
#define	MXVR_DMA5 AddreCONFI*/

RT_ADDR  08Y Mo */

#define	MXVR_DMA5_CONFIG      0x5FC027FC  /* MXVR Sync Data DMA55Config	Register */
#define	MXVR5DMA5_START_ADDR  0xFFC02800  /*	r (16-b*/
#defiC  /* MXVR Sync ss Register */
#define	MXVR_DM28 Mod */

#rent Loop Count RegiVR Sync Data DMA5 Loop Count RI1 Tine	MOC_1	      0xFSPORT0 002844Pl 13VR AllocatixFFC02778  /*txFFCBation	Data Logical Channel A
#defin  /*8s RegistT	      0xFFC0
#definxFFC04 */
#de134	/* Interrup#defin1Conf  /* Mdress Register *al ChanLogic	MXVR_e Frame Sync DivM#definData 02764  /* MXVRAlloc4define	SPent Y   0xFs Register 0xFFC0278  /* M44SPORT0 Receive Frame ADDR  0 MaskFC00D90	/* Count Register */
9/
#define	MXVData DMA5 Currege R MXVRync Datvideine	MXVRdefinIMER0C1 0xFFC027Lddr RADDR  0xFFC027D8  /  /* M /MTB_STtrol Messagfer Curr02708  r l PLL Con */
#define	DMAC1XVR_C e	DMA4#definMRB Data DMA3 S1/* MXVRSelect Rster */
#defiddr RegiRX Logical Channel Assiessaer 2_CURR_Ane	MXVR_CMRB_CURR_ADDR   0  0xFFMX Loop Countage TX Buffer Cu 0 */
#d		0xFFCADDR  0xFFC027D8  /    0_ADDRT  0xFFC0285C rol Messa		0xFF1 Tradr Register *age TX Buf */
#definAddr Register */
#define28  /* MXB_CURR_ADDR   0xFFC0285C _STATE_r Control Message TX Buffer Currer Current AddrXVR AsnRegisteRR_ADDRTe	MXVR_CMRB_CURR_ADDR   02/* MXVRPTBS MDMA0_D1_trol Message RRD  0xFFC02RR_ADDR   0xMXVR A3 C2   0emo_DATAddr* MXVR Alloine	MXVRxFFC027E0 r*/
#define	DMAADDR  0xF   0xFFC0Addr Register */
#definen	Current	X CoR_ADDR   0xFFC0285C X Count  Control Message TX Buffer Curre Messaer R Sync Datsamek (0x87C  /* MXMXVR_CMRB_CURR_ADDR   03
#define	 Read Buffer Start Addr	3nFC00800 - 0rrent Address */
#defuR_PAMulti-*/

#define	DMA12_NEXTM3MXVR Patter Current Addr Register3  0xFFC02RR_ADDR   0xFRAME1DA4_0 874  /* MAddr787C  /* MXVT2_TXk (0x   0xFF Control Message TX Buffer Curre5nt Addr    0xFFC027	Registefindefine	MXVR_CMRB_CURR_ADDR   0fer n Enableol Message TX Buffer C4uion	SBIU_SD Data Register	1 */
#  0xFFC02778FC027FC  /* MXVR Sync D Table	Regr Current Addr Register4ROUTING_1Addr Register */
#define4MXVR PatC02878  /* MXVR Frame	Cou MXVR Al Control Message TX Buffer Curre6288s Register RoutFC00 /* Mhannel 15 X Modify RegiegisteOUTIN   0xFFC028ol Message TX Buffer C5Refine	MXVR_RR_COUNT  0xFF*/
#def5G_4	      0C02864  /* MXVR RemotexFFC00Ed Buffer Start Addr	Ress */5
#define	MXVR_CMRB_CURR_ADDR   0e5VR_ROUTING1	      0xFFC02870  5C RR_Y_ Routing Table	Register 9  Table	Re7288C  /* MXVR Routing Tabl5MXVR Pattern* MXVR Remote6ontrol 5  0xFFC028#defs */
#deFC00D90	/*	ce Current Y Count Register	1 */
#5   0xFFC02870  64ster */
#def1_START_ADSync Data ata LogicalurrenRIOD			0xFFC00R Control Mo    0xFr */

#definon RegistSPI2__ADDR  0xF13C* MXVR Alloc ITSWAB0	/*2764  /* MXVRe AddressBYrguraR_ALBAUD			0xFster */


/*2e	DMx0xFFC028UR0  /* MXVR AllocIXEDationel Transmit Select RegiDR
#dPATfer Curree Frame Sync DivY_COUAxFFC00B10	
#defiine	SPORT2_TLoopPOSA Chann02610	/* S02764  /* MRegisteL_MAP	0xF08  /* SPI2 SDD__Y_MODIFY			0E70	/* XVR RemoteB8  UNT		0xceive Frame  AssigAN  CurreB/* MXVR Routi	Receive	Datable	Reg/* MXVR RoutiMulti-Channel0er Cur/* MXVR Routiuting TaT	    ControxFFC02764  /* MXVRAllocatio01FAC	/* M02764  /*0xFFCAN_ SPOgnment	Re	Reggister (cle InterAssignmentp Countnation	X iguration 2 FSannel Sl 2 	RecoFC00ilboxes 0-r */
#d  0xFulti-Channel er */

#dllocatPVCN_TRansmi01D20	/* DMA ChaocatiAS5			0xl 6 Nextssignment	RegFC02AF */
#def* MXVR Aler Current AddrC0_				0xFFC009Watc */
#define	CAN_R7	     ster */
#dDest  /* MXVR SRMP1est Reset reg 1 */
#demit AboLock C  /*ontrol M8	/* Receive Mes4RML121FF)	 */
Agisterion C02ABTIF17	    * MXVR Allocation mTransmit	CAN_Tivider */
#define	CAN_TRS1	ransce02610	/* S	0xFFC02FC00100 - 0C0     0xFF276fer CurrRegister *UNT	#define	MXVIM	0xFFC02A20	28	0 Sta	0xFFCTL_ConfiguratiN_MBTIF_le ReegisPX ModLegisine	MXVPstination	Curr8	/* Receive M
#defCS0		0xFFC02550	/* SPOCANCELA21FF)	 */
E70	/* DMA Chanoller	0xFF0	/* GOver0xFFC025400848 BEgister	1 */efine	SPORT3_TCChann 0xFFC0#def600	/* SPORT3 TChann

/* Afine	CAN_RR_ROUG_130xFFM4	/* Transmit AboOPS Multi-CMXVR SMailboxeswfine	Prot	/* DM	Sefin		0xFFC  0xFFC02778  /CDESC_PTR		0x	SPORT3_MTe	SPO 1 *istegister	1 *figuration 1 Reg2A4C	40	/* MailAllocation Table2A4C	0xFF0RR_ADDR   0xF
#define2A4C		CAN_XVR Alloc88	/* Receiv2A4C	urreister 0 */
#de40  /* S2A4C	smit */
#define	SPORT3_MRC2A4C	smit ost	reg 1 */
R Contro2A4C	p CouHAN_6     0xFFC02790  anBEIC_IAhannel Transmit SeleRR_ABEefinee	SPORT3 Interr*/

#def02A4gister	1rupt Flag reg 1 */
C02A440	/* Masmit AboRFH	0xFFC02CAN_R	0xFFCTnsmit AboMBrupt MaskCAN_Rister *ter 9 */
#defin12 		0xFFCA			0xF_ALLOC_13	      0xFr	NamegV			0nc Data Logical ChannelTING_1	 IG      Frame Sync Didefiskr *//* SPORT3 xFFC0rupt Flag Current CxFFC0273borttble Ree	HC005
#defin 


/**FC02TR3 Current Cndling reg XVR 8	/* Receivx */
#define	SPEN02A4C	 Transmit AboHandlingRQ_ST2AFY			0Reefine	TWI1_XMT/*t SelCont    0xFFCnel	Receive	Saer 0 */
#d0xFFC0084ine	SPOle	Regiser 0 */
#dXVR 0263T
#define	TWI1_Ier 0 */
#dLOCK			RT0_MTCS3		nsceiver 0 */
#dster		ve Frame			0xFF DMA C0 */
#dModifyTCS1		0xFFC02644	RR_Y84	/* Bi	2 VR_DMA5_CON	backwards compati1ming Confi
#define TWI1_R	backwarfine	Uknowl3FF)	 */
#definr */
#defi0xFFC0084	SPI2_BAUD			0xr */
#definR_ALLOC_ /* MXVR Phase	r */
#defi	/* DMAbFFC02A10	/* Tranr */
#defi	0xFFC0001D20	/* DMA Chafine	CAN_GITWI0_INefine	SPORT3_TC0050gister GALLOC_1ter */
#dCLOCK			0xFFC02A2ming Configuration 1 RegNt X Count _D				008	/* SPORT2 Trant X Count 


/***604	/* SPORT3 Tr/
#define	S */

#diguration 2 RegiRegister */T0 Receive Frame Sync2_steratus Rester		define	SPORT3_MRCnt X Count pt MasFFC02748  /* MXVRyerrupt A FF				0FIFO_CTRLNamesFC0050		0xFFC3ming Clock Divider */
#d*ning	Lev ChanneMXVR_ALLOC_11	    nxFFC02AB4	


/*g 1  */
#define	SPL_MAP	0xFFC0*/

ivider */
#define		xFFC02AB4		MXVRine	CAN_MBIM1			0xt AboUCefiniste */
#define	CAN_MBTDxFFC02AB4	FFC0_ALLOC_13	      0xFLOCK			0xFFG

#	/* Mailbox InterruptTransmiRT3 Multi-C_COUNT			0xUnt CHster (cle_UCCNc  0xF retaFFC0MC01410	#define	SPIO_POLARAM00LS1_CUR02608	/* SPORT3 Ti 0 Low		0xVR Sync Data DMA0 StTXee	SPent Register */
#defin reg /* DMA ChNT	      0xFFC0 reg MDMA0_S1dress Register * reg  StarDMA3_CONFIG      0xFFDMA0 StreORT0 Receive Frame S 0 Low */
#dMe	DMA14_IRQ_ST AborM00L/* DMAdefinr Regisreg 1  0		0xMA ChaN_AM00L_STALOCK			0xFFC02Y_MODIF */
#defin	Registreg DMA Channe_C		pceptance Mask  Channel e	CAN_10xFFC00BFF2Bgist_S1_XMailbR_ADw gh Accepta/* Memsk */
#define	CAN_1xFFC00900Receive Frame Sync Di 0 Low */
#defiask */
#define	CAN_AMCONFIG MDMA02 Count /* Mailb2emDMA0 h Acceptance Mask 02764  B18	/CAN_2#define	UARBT2 Curree	MDMA1_S1_h Acceptance Mask MDMA0_#define	CAN_30xFFC00BFFFIG			0x Mailbox 3 Low AcceptaA1 Stre Mask */
#de	CAN_AM03H			0x  0xFFC0286C  /* MXVR Sync Da1_CURR Mask */
#define	CAN_AM	urce St */
#definister /* Mailb */
#dgh Acceptance Mask r Regise	SPORT2H			0xFFC02B14	/* Ma#define	DMA1eptance Mask */
#deal Map	Reefine	CAN_50xFFC00BFF Currenailbox 6 5x 3 Low Accep1_S0_P Mask */
#define	N_AM0#define	UAR0287C  /* MXVR NT  0xFFC0MXVR_ce Mask */
#define	CAN_Aire In*/
#defin0	/* G/* Mailb6Start Aceptance Mask */
#define	nc DaM064H			0xFFC0232B14	/* MY_COUNT MDMeptance Mask */
#deDescripine	CAN_7M03L			0xFFC38e	MDMA1lbox 7 Low Acceptance Mne MDM
#define	CAN_AM07H			0xFFC020FC0289C  /* MXVR Rolbox 2 Ler */ine	DMA14_IRQ_STne	CAN_AM Current CFC02B10	/* Mailbox 2 LLow Acceptance Mask */
#define2	CAN_A4H			0xFFC0202B14	/* Ma	MDMghtance Mask */
#define	CA2*/
#define	CAN_AM03L			0xFFC0TATUS		* Mailbox 3 Low Accepta2nce Mask */
#define	CAN_AM03H			0xFC02A2ss Register Rilbox 2 Lcceptance Mask */
#defiAM10L		202M03L			0xFFC02B20	/* Mailbox2 3 Low Acceptance Mask */
#def2AM10L			04H			0xFFC02B24	/* M2 Mailbox
#dee Mask */
#define0C84	/* DMA 	CAN_AM05L			0xFFC22Bter */
* Mailb3x 3 Low Acce2sk */
#define	CAN_AM107H			0xFFC02ontrol MessaAfer Curlbox 2 Lnce Mask */
#define	CAN_AM0xFF2CAN_4M03L			0xFFC	/* Mailbox 634x 3 Low Acceptance Mask */
#d3		0xFFC0244H			0xFFC02Map	Ren	Curtion	Nex */
#define	CAN_AM1AN_AM13H			0xFFC02B	0xFFC02B332BMAP		0box 7 Low Acceptance 3
#define	CAN_AM12	0xFFC02B780	/* D8 Mailbo3tance Mask */
#defw AccepOdefine	CAN_AM13H			0xFF3M06xFFC02B48	/* Mailbox 8 Low 3ance Mask */
#define	CAN_AM12H3xFFC02B0xFFC02B44	/* Mailbox XVR S*/
#define	CAN_AM14L			0xX Count RM09L			0xFFC02B48	/* 3 Low Accepta7ce Mask */
#defi3fine	CAN_AM09H			0xFFC7#define	UAR9ow Acc5tance Mask */
#defineptance Mask */
#define	CAN_8Lgister		/* Registe* Mailb8x 3 L3k */
#define	CAN_AM14L			0xFF_3M084H			0xFFC020 Sourw Acceptanance Mask */
#define	
#definNFIGL			0xF9M03L			0xFFCOUNT		3* Mailb9ce Mask */
#define	CA_CURR_DESC_19L			0xFC02B94	/* MaC	ter */ MXVR Sync Data nce MaskMask */
#define	CAN_AM10L			0x4FC02B50	/* Mailbox 10 Low Acce4ask */
#define	CAN_AM14L			0xF4			0xFFC02B54	/* Mailbox 10 Hfineceptance Mask */
#define	C4CAN_AM11L			0xFFC02B58	/* MaixFFC00E Low Acceptance Mask */4#define	CAN_AM11H			0xFFC02B5C	/* 0* Maine	TWI Mailboxance Maskask */
#define	CAN_AM12L			0xF4C02B60	/* Mailbox 12 Low Accep4ance Mask */
#define	CAN_AM12H4		0xFFC02B64	/* Mailbox 12 Hiih Acceptance Mask */
#define	4N_AM13H			0xFFC02B60xFFC00BFF4ox 13 Low Acceptance Mask */
4define	CAN_AM13H			0xFFC02B6C	/* M Register 027E8  /* Mnce Masksk */
#define	CAN_AM14L			0xFF402B70	/* Mailbox 14 Low Accept5nce Mask */
#define	CAN_AM14H	5	0xFFC02B74	/* Mailbox 14 Hige Mask */
#define	CAN_AM20H			e	MXVR_L			0xFFC02B78	/* Mailb5x 15 Low Acceptance Mask */
#E			0_AM19H		1C	/* Ma	/* Ma7ine	TWI8	/* Receive MeXVR Asdefine	Cdefin0xFFC02BC8	/* M1B80	/* Ma502B80	/* Mailbox 16 Low Accept5ance Mask */
#define	CAN_AM12H5	0xFFC02B84	/* Mailbox 16 Hig5ask */
#define	CAN_AM14L			0x5AN_AM17H			0xFFCxFFC02B48	/* 5x 17 Low Acceptance Mask */
#5efine	CAN_AM17H			0xFFC02B8C	/* Ma2B3C	/* Mailbox 7 HigBCC	/* Mk */
#define	CAN_AM18L			0xFFC52B90	/* Mailbox 18 Low Accepta5ce Mask */
#define	CAN_AM18H		50xFFC02B94	/* Mailbox 18 High_Y_COUNT MDMAN_AM12L			0xFFMail_AM19L			0xFFC02B98	/* Mailbo5 19 Low Acceptance Mask */
#d24	/* DMA CM19H			0xFFent AddrPLA7_PELow-Byte) */
2_L(	Reg)OUTING_ion	Regi0xFFC0282C  /* R   0xRIDRe Control R1	/* UniversaCAow ADST_O 0xF - Mask reg 1lbox 18 Low Aow AcRCFFC02BE3FFC02B54	/*u/*
 	0xFFC02A4C0xFFC0L			0x			0xFFC02T52BDR
#def* Mailbo9ce Mas2104	/* Interrup(CM17 X Modigh AccepM03L			0xFFCRAL_MAPTR	Stre/* MXVR 4H			0xFFC02Filbox 18 Hiupt/S		0xFFC02BH


/****D li*/
#define	CAT 30Acceptance M Control ReORT2 Current(8H		escriAcceptance Mask 
/*N_AMask */
#define3FC02B58	/* C02BF8	/* MCNF	ptance00 -	0xFANSW			0xFFC00* CA5 Low AMB00ulti-	0xFFC02	CAN_13_IRQ_STAx 0 Data0_DLH	 Registe    0xFFCFC	/* Mailboxnsmitccept
#deSet	reg l 8 Per/
#define	CAN_MB0ME1 [31:16] DMA Channel 10	/* MailboxHADOW	0xFFC0_H1FF) 	(ister W Data Word  set  2 [47:32FC02C08	/* Ma
#Gefine	CAN_MB00_DATA3		xFFC02ister */
#deupt Assgh Acceptance Ma */
FFC02BC8	/* M2BFC	/* Mailbox 3h Co Acceptance Mask 0253C	/a irectlyCodT2 Rech Acceptance Mask CAN AcceptaIMEST		0xAcceptance Mask */
#definegisterurreAcceptance Mask */
#de16-38	/* fier	LAM								 */
x 0 Data N_AM0FC02C0C	/* MailFFC02BC8	efine	DMAMA ChChannew Re14	/* Mailboxox 31 Low Acc3r	ance Mask */
#deilbox 18 High0MDMA10xFFfine	CA /* A*/
#Node PositionIF2			00_ITp	V StreADDR  0xFFC027D8 16-31								e	CAN_AM+fine*0xx8))	CAN_cceptance	MDM0_DA0Register */
#define	CY_COU00610e	MXe MaaS0_NEXT_	CRRDreg 02B20	/* Mailb	CAN_MBirectlyCHIPWilbo0xFF1:16ble	WarnFC00138	/* In#definATA1		0xFF 19 High Ac */
#FC01		0xFFC02204		1EC0	/* DMA AREA NETWORK 			(A1_D1_NEXT_DESC_PTRA0 Steup Re _Digurati) */Lowion Registion Tab	Curren9C	11_X_M */
008	R*/
#S) */
#defin Map1FE8	/*stinati	MDMA1 Curree	MDMA1_S1_CUABw Accepus RegiData-SC_PO
#define	MXVR_DMAMA6 Btion	Next 5ceptanc-Uper */ll SC_PefineFC00800  /*0xFFC00910 SMOUNT MDMgnment	leA8_IR    bit

#defC00D44	/* al Chanarning	LntifSus Sta2	0xFFC0/* MailboxAN_MB00_	/* [15:0 Interntif1DD4Address	Reine	CAN_MB02_DATfine	CANster */
#define	CuLARM	0xFFC00A0_S0_CURRORT1_RFWar Loop#define	MXVR_DMATA11_Y_MODIFY		r 0 *_DATA3		0xFFC02C0C	/* L2			0 MA1_S1Slbal InPass_TFSx 11 Low AcceIdeExFFCer	LowCOUNT CC	/*34	/*ff	0xFFC02C24	/* MaCSTART_ADDR/
#defs Register *  /* MXVR Snter RCOUNT		0xESC_PTR Low AcN_MB00_DATA2		0xFFFFC02BC8	/*MB02_InteSTAMBPMA0_S0_Sl 8 PePAcceptaPo Syst*/
#define	MXVource r */
#defC00830  ral Map Register m Y Modier *nel	TraSPORTC_PTR		0ine	CAN_  /* RA1		0x*/
#define	DMRilbox 2n010CMemDM-RiphePre-SRegiuel 18 PeripheralNer RegidefiT1_RX			0xFFC/EG/* DMA Cfer Ce	DMASeg0 St	/* DMA ChannelSEG5 Low Ac94	/* DMA t Address Register */SACAN_MB01 InterSampMCMC2F)	 */
#def/Jr */
#d3rrupt/MDMA0_egizA ChanJump	Wefine	Sptor PointEBUd2C44	/* ]_ADDR  0xFFC0D2_ID	0xF		0xFFC01F18	/*ntifntifie/* DMA  */
#define	DXGTH		0 Hig */
#define	MntifCXne	D S2AACPORT3 Multe	CAN_MB07:32]xFFC01 Low AcFO	Trs Regine	DMA14_DI0E14	/* FFC028FF)e	CAN_MB0								STAT	 Register */
#t	reg /* RemMae	CAN_ */

#definFC02AX_COUNT
#define MDMCC	/* 0 [15:0]	e	CAN_DATA3ream dify Register */
	MDMA1_Dister */ntifDebue	DMA14_Value Regis3MA0_D1	0xnter Register2_CX_MB04_DAFF03HNL		ceptancIG      0xFFCR
#define MDMA_ Slave bort */
#de#defin0xFFC02C6C	/* MaiR ScrifiINTRegisterailboxes_RCV_DATBRor lN_MB00_DA3	0xFFC02FFC00C04			0xFFC00C  /* MXVR SyncF	Mask MacStrea	0xFFC02411D24	/* Tine	CAN_4CAN_R:0]	Register fine	 Low Acc4A1		0xFFCDATAT		0xFF4ultifine	SPORT2_MCMC1		0xGor laDATA1		0xFle	WaRX			0xFFC00818  /* CloSMAA Chan		(021FF)	 */
	0xFFmp	Value Register */
#deAxFFC016FC02C6C	/* M				us	/* Mne	CAN_MRegistMa0	DMA6AN_MB02_DATA0xFF1		0xFFC02C2ta Word MBxx 3 Dster ta Word erPORT0 MXVR 
#defin Low AF 3 Dat0120 tatus RegiID1	3 Cue	Reg(I/* Rine	FIO(IDDIFY			0xFTA1		XTIegis5tart	AdMaTrans0Register2oscricceptanICMC2r */
#e	CAN_AM26L			0MB05HNL		I_START_ADY_Up#defi19_CUC_PT_MB00_DA		0xFFC02C6)

/_CURR_AR_DESC_1ASEInnel 1 0xF620Biste	0xFFC02C6Cdefine	SPORT3ptanIdSelect Relbox 2 Timeta Woistern Register   trol Re0_Y_MODIFN_MB0ter */
05FF) *r */
#ER BIT & ADDRESS DAMData Wter */
#t C ptaA1_D	regiMA4_CURR_ADDR	#define	C		0x	/* MA Channel ORT3_RCR1	V */
#xFFC02CAnel 8tamTUS			CR Sdefine	C	/* DMA Channess Register0CA0#define	CA_CURR_ADDR			e	Region	Curr2CBAMxxHR_STAT	02
#de/
#deD0		0xFFC0 Mailbox Dointer	B04_ID1		0xxFFC0U_iste1 CTWI1	MasMXVRrectLow I	0xFFC01efine	CAN_MB		0xFFC02B3DMA Chabox 7 Low AN_MB00_DA		0xFFC02C6C	ntifefine	CANance     0xFFC0ster */1:16] Regis		0xFF6_DATA0		0xFFC02CC0	TA2		0lbox 5 I/_RBR			0xFFC0er */
#define	CAN_MBAxFFC02644	CA8rOUNT			0xFFC0M	0xFFR
#define MD	CANDMA ChgistI
#define	DMRT1_SCR	egister */
#Mister r */
#de Bau	CAN_gister CAN_r */
#define	C6ulti-Dter */
#ter */FC02CB2C1C60xFFC02CC0	3 [63:48]xFFC02CB4ller 04el 2ter */
#defiP	0xWI0_MASTER_CTRL	0C	0xFFC02er */
#define	efinrupt Ma	SPORT#defi	0xFFC02 Current Y CouCC4	/* DMA Channnel 19	0xFFC02FFC00C40	/* DMCR_Y_COUNT
6_ID

#define	CUNT	8	/* DMA Channster 7 */
#definddres */
#defi/* DMA ChannelY			* DMA Channel 18efine#defiupt/Status RegART_A

#define	DMA13_AN_MB07_D* DMA Channel	Regi#define	MDMA0_ SPON_MB07_D0xFFC01E58	/* 18  /R2_C01B0ine	C7
#define	CASTART_ADDR			0nc D#define	CANd 1 [31:16] Reg		0xFFC01CF0	/*MemDMA0 StrData Wom_DATA1		0xFster */
#d_DATA11F1C	/* M[47:32] Register */
FFC027E0  /* MX	CAN_AM26L[47:32] Register */
lbox 8 Low AIdTHR	0xFFC02A47:32] Register */
[63:48] RegiID02B20	/	MDMx 17 Low Accep* MXVRWord 0 [15:0]	R/* MemDMN01CE0e	CAN_MB07_TIMES19_CUR Low ADatr */
#dTIMESTefine	CAN_Tce Mask 63:48] Regir */
#dePxFFC02C6C	/* Maiask Macro1 Data WoRegister */
ATA1		0xFFC0ail Low Accde Reggister */
#defi#define	SPI2_BAUFC02284	1 Data Word 3   0xFFC027 Time Stamp	Vnce Mask2ss	RegiMB07_LENGTH		2#define	CAN_MB0L			0xFFister */
#define	C7_Dream 0 Source Perdify Register ister */
#defi  TiB07_LENGTH		us Regiailbox 8 Datadefine	CA */
th Code Regie Mask *ailbox 2 Time Sta]	Re2Word 0 [15:0]	R			0xFF 1 [31:1N_MB06_DATAE8	2 */

#define	CEox 15 HATA3		0xFFC02C0C	/* Ma2FFC02C6C	/* Mair Point_DESC_Pefine	CAN_MB08_D02CFC	/* Mailboata Wourrent Define	CAN_MB08_D /* MXVR Sync DaFFC02D04ct Reefine	CAN_MB08_Dbox 8 Data Wordox 17 P	0xFFC02FC027E0  /* MXV#define	CAN_MB0er (clear) */
#FC027E0  /* MXVent Descript
#DB01_DATA3egister */
#defintrold 3 [63:48] Regi Identifi	0For		0xFFC0:0]	Register /A2		0xFFC0[47:32] Register */
i02gister A2		0xFF90xFFC02:32] Register efine	CAN_MB08_D e	CAN_MB09_DATA1		0xFFC02Dne	CAN_MB0TART_ADDR  0xFFC02858 CAN_MB09_DATA1		0xFFC02ister */
#degister */
#definentMailbox 9 Data Word 2 [47
#define	DMA12_NEXTox 8 Data WoMailbox 9 Data Word 2 [47me Stamp	VaMB07_TIME2CC0	/* Mailailbox 9 Data Word 2 [47OCK			0xFFC02Ad 1 [31:16] RegATA1CAN_MB09_DATA1		0xFFC02	0xFFC02D30	/* MTA2		0xFFC02C08MP	0xFFC02D34	/* Mailbox lbox 8 Data Wor3 [63:48] Regi/*MP	0xFFC02D34	/* Mailbox oMB00_DATA3		0xFFC02C0C	/* Mail	0xFF Register */
#define	Ca Wor02B90	/* MCbox 31 Low Acc7TA1		0ta Word 0 [15:0]	Reg/* Mai Count Register */
#d0xFF		0xFFC02D40	/* Mailbox 10#defineRAL_MAPTH		0xFFC02D30Len		0xFFC02D40	/* Mailbox 10 [15:0] Register */8 Datr */
#d		0xFFC02D40	/* Mailbox 10box 8 Data Wo[47:32] Register *		0xFFC02D40	/*8 Low Accannel 4ngth15:0]	Register MA1 2
#definMailbox 10ta Word 0 [48	/* Mailbox 10 Data Word		CAN_MB10_DATA2		03 DatMESTAMP		0xFFC02D40	/* Mailbox 10 */
#define	CAox 10 Data Word	
		0xFFC02D40	/* Mailbox 10B08er */

#define	fine	DMMailbo		0xFFC02D40	/* Mailbox 1	/* Mailbox 6 0 [15:0] Register #define	C0 StreaAcceptanc Data DMine	DMA18 Low Accefine	Cine	CAN_MB10_ID1		0xFFC02Drd	0 [15:0] Register */Mgister ine	CAN_MB10_ID1		0xFFC02Dbox 10 Identifier Hig_DATA3		0xine	CAN_MB10_ID1		0xFFC02D0		0xFFC02D60	/* Mail */

#defiine	CAN_MB10_ID1		0xFFC02D/
#define	CAN_D0	/* MaiF8	/* Memne	CAN_MB10_ID1		0xFFC02DD60	/* M [47:32] RSTAMP	02B20	/a Word	2 [47:32] Register MB00_TIMEST Count Register */
#dne	CAN_MB10_ID1		0xFFC02Dentifier	Low DHigh AcceLow Acclbox 11 Data Word	0 [15:0] :48] Register */
#defi2D60	/* M		0xFFC02D40	/* Mailbox 1 13 Low Accept8/
#define	CA9_DATID1		0xFFC01egister */
 VaxFFC02D60	/* MLength Code Dailb2D60	/1 0 [1502B90	/* MRMer 1 WidP	0xFFC02D54	/* MRM0 Counter RegiRXceptance 0241ConfIter dentifier	Low RAN_Mine			0xFFC02D60	/*lbox 9 Data Word 3 [63:12_ister */
#defiR		0xF	 TWIegister02D30	/* 			0xFFC0annel 13 Curr00C40	/* DRMPam 0 SourLow Regist		0xFFCAcceptanFFC02D84	8	/* DMA ChannMP Register *efine	CAN_MB12_DATA2		0xFFC02D88/* DMA ChannelMP#define	DM [47:32] Register */
#define	CAN_Map	Register 2 Daine	SPOR Mailbox 12 Data Word	3 [63:48] Re* DMA ChannelRMP/
#define Mailbox 12 Data Word	3 [63:48] Re0xFFC01E58	/*Risteral InterrupFC02D84	/* Mailbox 12 Data Wo		0xFFC01DD0	RMP	Next Des78	/* MailbA1		0xFFC02C24	/* Ma SouMP	0xFFC02C7 [47MB00_DATA3		0tamp Value Register */
#define*r */
#define	C80	e	CAN_MB11_ID0xFFC02D9C	/* Mailbox 12 IdentiTA0		0xFFC02D60		CAN_MB00_D	0ATA1		0xFFC	/* Mailbox 12 Identird	TA2		0xFFC02CB11_DATA0 Sour0xFFC02D9C	/* Mailbox 12 IdentiMB12_DATA2		0xFF		0xFFC02D60	/0xFFC02D9C	/* Mailbox 12 Identiister CAN_MB11_Id	TA3		0xFFC020xFFC02D9C	/* Mailbox 12 Identi	0xFFC02C24	/60	/egistailbox 20		0xFFC0AN_M3 [63:48] RegiAN_MB13_DATA3		0xFFC02DAC	/* MaiN_MB01_DATA3Cod0_ID1		0xFFC020xFFC02D9C	/* Mailbox 12 IdentiSTAMP	#define	ilfineATA0		0xFFData Word	1 [31:16] Register */
 Registerox 9 D Low Register 0xFFC02D9C	/* Mailbox 12 Identi#define	MXVR_DMA	/* Mailbox 6 ine	CAN_MB12_DATA2		0xFFC02D88rfier High Regist_DESC	CAN_MB03_TIMBC	/* Mailbox 13 IdentifierTA0		0xFFC02D60	/er */
#define	MB14_DATA0		0xFFC02DC0	/* MailTA1		0xFFC02DA4		0xFFC02C24	/*ster */
#define	CAN_MB14_DATA1	MB12_DATA2		0xFFxFFC002D30	/* ster */
#define	CAN_MB14_DATA1	 [47:32] Registelbox 9 IdenD68ster */
#define	CAN_MB14_DATA1	Low Register */
ask */
#definester */
#define	CAN_MB14_DATA1	13 Data Length Cer */
D0	/* Master */
#define	CAN_MB14_DATA1	FFC02DB4	/* Mail	3 [6me Stamp	 Word	1 [31:16] Register */
#defN_MB13_ID0		0xFe Regist1 13 Tster */
#define	CAN_MB14_DATA1	#define	MXVR_DMA 13 Low Accept5:0] Register ailbox 9 Iden6_X_fier High Registister */
#dxFFC00e	CAN_MB14_ID1		0xFFC02DDC	/ue Register *RMify R [63:48] Register */
#Regise Regist3ine	CAN_MB1llocister */

#defir High RegisCAN_R Low Register */
#de0] Regist/
#defiLOCK			0xFFC02AxFFC1302D78	/* MailbA1		0x1:16] Register TA1		0xFFC02DA4 4 X_TIMESTAMP	0xFFC02D71 Mailbox 15 DataMB12_DATA2		0xFLister */
TA3		0xFFC02C0C Mailbox 15 Data [47:32] RegistL02DDCe	CAN_MB05_ID0xFFC0 Mailbox 15 DataLow Register */Lister */

#ne	CAN_MB11_D Mailbox 15 Data13 Data Length LCAN_MB11_TIMESTAMP	0xFFC Mailbox 15 DataFFC02DB4	/* MaiLF0	/* MTA2		0xFFC02DE8	/MB15_DATA3		0xFFCN_MB13_ID0		0xne	MDM2nnel 8 _DATA2		0xF Mailbox 15 Dataegister */
#defAN_MData Word	0 [15:0] Reg		0xFFC02C08	/* M Mailbox 1	0xFFC0 Word	0 [15:0] Regist3_D		0xFFC02C08	/* MTA0		0xFFC02D60	r */
#define	CAN_MB13_DA/		0xFFC02C08	/* M5 Data Word	3 [61		0xFFC02DF3FC02D84	/* 1		0xFFC02C08	/* M Mail
#define	CA Data1		0xFFC02DD4	/* Ts			0xFFC02C08	/* M#define	CAN_ 2 [ster */
#define	CAN_ Dataine	CAN_MB16_DATA	CAN_MB16_DATA2Lbox 14 Data Word	0 [15:0] x 8 /* Mailbox 16 D */
#define	CAN_FFC02DF4B16_DATAirectlyFC02 Mailbo	0xFFC02CC01		0xFFC02DA4	Registester *ox 9 Data AN_MB16TA2		0xFFC02DE8	/*	0xFFC02C24	/* Magister */
#defin_MB15#definne	CAN_MB16I3:48] Register */
#efine	CAce MasReWord	er Low RegistB
#defiister */
#	0xFFC0E00	/* Mailbox 1rd	0 [15:0] Regist*/
#dAC027ster */

#defiTA0		0xFFC02D60	/*E04	/* Mailbox 16 Drary 7 Data Word	0 [15 Data Word	3 [6 Data Wod 3 [63:48]1		0xF0xFFC02E24	/* Maix 16 Data Word	2:32] Registerfine	D7 Data0xFFC02E24	/* Mai3		0xFFC02E0C	/*	0xFFC02E20	/* Mailbox3		0xFFC02E24	/* Mai	3 [63:48] RegistNGTH		0xFFC02E10	/* Mefin7 Data Word	0 [1egister */
_DATAd	1 [31:16] Reg02DD0	/* Ma7 Data Word	0 [1 Value Register 2D70	/* MailbAN_MB07Word	2 [47:32] Registe15 Data Word	1 [3FC02E20	/* Maiox 9 Data ster */
#define	CxFFC02E1C	/* Mai_MB11_TIMESTAMP	0xFFC02D7ter */
#d02B90	/*E00	/* Mailbox 1FFC02E1C	/* MaiLow Regist_MB17_ID1		0xFFC02ox 10 Identiion a Word 0 [15#define	UART2_MCR			 [47lbox 12 Data ne	MXVR_DMMA1_D0_Y_	 */ART_DMA_Sr TX	Sine MDShotCAN_Megister Low Register rd 0 [ 0 [15:0]	Register#define	CAN_ster *ngth Code  0 Sourx 17 Data#define	CA Current Y Co Regir */
#dxFFC0_DATA2		0xFFC02xFFC02644	0xFFC00EData Word	2 [47:32ta Word	3 rd	1 [31:16] r */
_MB16_ID1		0xDATA3		0xFFC02E4C	/* Mailbox 18 Data Word	3 [63:48] Register8	/* DMA Channe	CA[63:48] RegixFFC02E50	/* Mailbox 18 Data Length Code Register */
#define	ter Register   /* FTH		0xFFC02xFFC02E50	/* Mailbox 18 Data Length Code Register */
#define	Map	Register A9_ST*/
#define	CxFFC02E50	/* Mailbox 18 Data Length Code Register */
#define	5:0]	Register atior High RegisxFFC02E50	/* Mailbox 18 Data Length Code Register */
#define	0xFFC01E58	/*5:0]02DF8	/* MailxFFC02E50	/* Mailbox 18 Data Length Code Register */
#define			0xFFC01DD0	5:0]AN_MB15_ID1		xFFC02E50	/* Mailbox 18 Data Length Code Register */
#define	Cter */
#define	CMB18_LENGTFO_STAATA3		0xFFC02E4C	/* Mailbox 18 Data Word	3 [63:48] Register] Mailbox 16 Time S:32] Registefine	CAN Mailbox 19 Data Length Code Register */
#define	CAN_MB Word	0 [15:0] ReWord	0 [15:0] ilbox 1 Mailbox 19 Data Length Code Register */
#define	CAN_MB15:0] Register *//* Mailbox 13  Data W Mailbox 19 Data Length Code Register */
#define	CAN_MBATA2		0xFr */
#de*/
#define	CAN_ 0 [15 Mailbox 19 Data Length Code Register */
#define	CAN_MBx 9 Data WoA0 Strta Word	3 [63:48]/* Slister */
#define	CAN_MB19_ID1		0xFFC02E7C	/* Mailbox 19/

#define	DMA12_m 1 Source Y Ch Rord	3 [63:48] er L MailboTH		0xF
#definlbox 11 Low Ac2Word	0 [15:0_ID1		0xFFC02D3C	/* Mail	TIMERter */
#define	0_ID1		0xFFC02D* MXVR88	/* Mailbox 20 Data Word	2 [47:32] Register */
#defin2		0xFFC02C08	/* box 13 Time St	/* Mailbailbox 19 Data Length Code Register */
#define	CAN_MBLow Register */
# Low Register */
#defFC02E98	/* Mailbox 20 Identifier Low Register */
#defin*/
#define	CAN_9rren0xFFC02C6C	/* MailC	/* Mailb	0xFFC02DF0	/*d	3 [63:48] LENGTH		0xFFC02E10	 Mailbox 16 Time /

#define	CANterru Regist] Register */
#define	CAN_MB21_DATA1		0xFFC02EA4	/ Word	0 [15:0] Reg Low Register */
#de[15:0] Register */
#define	CAN_MB21_DATA1		0xFFC02EA4	/15:0] Register */ent Address Register *15:0] Register */
#define	CAN_MB21_DATA1		0xFFC02EA4	/15:0] Register */TA2		0xFFC02DC1		0xFFLENGTH		0xFFC02EB0	/* Mailbox 21 Data Length Code Regiser */
#define	CAN
#define	CAN_MCAN_MB05e	CAN_MB21_DATA3		0xFFC02EAC	/* Mailbox 21 Data Word	3  Data Word	AN_MBength Code Register1 X CountRegister */
#define	CAN_MB21_DATA1		0xFFC02EA4	/02D84	/* Mailbox 1MB21_DATA1		0xFFC02E 21 Identifier Low Register */
#define	CAN_MB21_ID1		0x2		0xFFC02C08	/* TAMP	0xFFC02DD	0xFFC0 21 Identifier Low Register */
#define	CAN_MB21_ID1		0xMB11_TIMESTAMP	0xFFC02D7AN_MB14entifie 21 Identifier Low Register */
#define	CAN_MB21_ID1		0x	0xFFC02EA0	/* Maalue Register */
#def	0xFFC Data Word	3 [63Word	3 [63:48] Register */
#defiC Mailbox 16 Time ntifier High Re/* Mow	/* Mailbox 22 Data Length Code Register */
#define	CANN_MB18_DATA0	fine	DMA8_Select Register 0 */* Flream Allocatiny	Butyte 't LMC2	e	GPIO_DoB15_DATA1		0xFFC02DE4	 Dat60	/*1_ID0		0x0xFFC0x 3 DatRegisterter */
#define2ART_ADDR			0xFfine	CAN_Mter */
#define	CAN_MB23_DATA0		0xFFC02EE0	/rd	1 [31:16] TDMA Chann	CAN_Mx 16 Data Length Code Registe2 [47:31DMA0 Stdefine TR/* Fl:48] Regia Word	1 [31:16] Register */
#define	Cter Register TRC02AH		0xFFC02a Word	1 [31:16] Register */
#define	CMap	Register TR* Mailbox ne	Ca Word	1 [31:16] Register */
#define	C* DMA Channeler *#define	DMA10 Word	0 [15:0] Register *//
#define	C0xFFC01E58	/*TR	CAN_8	/* Mail/
#define	CAN_MB23_TIMESTAMP	0xFFC02EF		0xFFC01DD0	TR#definennel 8 a Word	1 [31:16] Register */
#define	CAMP	0xFFC02C7 DatENGTH		0xFFC0ster */
#define	#definer */
#definMA Ch2ED4	/* Mailb
#de02E74	/* Mail3 Identifier High Register */

#define	AN_MB02_DAT16_DAWord	0 [15:0] 3 Identifier High Register */

#define	r */

#def */

#/* Mailbox 13 3 Identifier High Register */

#define	TAMP	0xFFC02xFFCB23_TIMESTAMP	3 Identifier High Register */

#define	TIMESTAMP	0xFFC0	0xFFC02E90	/*3 Identifier High Register */

#define	e	CAN_MB20_DAegister 1 */
#defCode Registerrd	 [63:48] Regi3 Identifier High Register */

#define	ne	CAN_MB16_TICo_MB20_ID0		0xF3 Identifier High Register */

#define	ox 18 Data Wo23fine	CAN_MB20_I3 Identifier High Register */

#define	Register:48] Reg Low Register N_MB24_TIMESTAMP	0xFFC02F14	/* Mailbox 2 16 IdentifieEFilbox 21 Data /
#define	CAN_MB23_TIMESTAMP	0xFFC02EF	CAN_MB24_DATA0		/

#define	CANord	0 [15:0] Register */
#define	CAN_MB*/
#define	CAN_Madify Register rd	0 [15:0] Register */
#define	CAN_MB6] Register */
#	0xFFC02C24	/*28	/* Mailbox 25 Data Word	2 [47:32] ReWord	2 [47:32] RTA2		0xFFC02DC28	/* Mailbox 25 Data Word	2 [47:32] ReTIMESTAMP	0xFFC0
#define	CAN_M28	/* Mailbox 25 Data Word	2 [47:32] Rex 21 Data Word	3 [63:48] Regis28	/* Mailbox 25 Data Word	2 [47:32] Re24 Time Stamp Va	0 [15:0] Regi28	/* Mailbox 25 Data Word	2 [47:32] Reailbox 24 IdentiTAMP	0xFFC02DD28	/* Mailbox 25 Data Word	2 [47:32] Re	/* Mailbox 24 Iefine	CAN_MB22e Register */
#define	CAN_MB25_ID0		0xFFFFC02F20	/* Mai 13 Low Accept14	/* Mailbox 16 Time Stampine	CAN_AM10HAN_MB24_DATA0		ntifier High R44	/* Mailbox 26 Data Word	1 [31:16] Re23_DATA0 Mailbo */
#deme Stamnt Y Count Regisox 12 Data W2DMA Cha Datad 3 [634	/*  24 Idengister */
#defird 1 [31:16] ReAN_MB02_DATTH		0xFFC02_DATA1		0x*/
#define	CAN_efine	CAN_MB1xFFCNGTH		0xFFC02F50	/* Mailbox 266] Register */
#FC02 [47:32] 
#define	CAN_MB26_TIMESTAMP	0xFFCWord	2 [47:32] 0_ID0		0xFE			0xFNGTH		0xFFC02F50	/* Mailbox 26rd	1 [31:16] ReMailbox 22 Da Register */FFC02F50	/* Mailbox 26GTH		0xFFC02E10FC02E1C	/* Maix 26 Identifier Low Register */
#24 Time Stamp Vne	CAN_MB20_DAx 26 Identifier Low Register */
#ailbox 24 Identi	1 [31:16] Rex 26 Identifier Low Register */
#	/* Mailbox 24 02EA0	/* Ma
#deDATA0		0xFFC02F60	/* Mailbox 27 DFFC02F20	/* Maine	CAN_MB11_DAine	CAN_MB11_DATF	/* Mailbox 2226START_ADDR  0xFFC0MA16_PERIPHERA0xFFC02F6C	/* Mailbox 27 Data Wordta Length Code r */
#define	CA0xFFC02F6C	/* Mailbox 27 Data Wor Maibox 26 Identefine	CAN_MB16_0xFFC02F6C	/* Mailbox 27 Data WorB2w Register */
 [47:32] Regist0xFFC02F6C	/* Mailbox 27 Data Wor23_TIMESTAMP	06egister */
gistester */
#define	CAN_MB27_TIMESTAMP	002B20	/* Mai_MB26_DATA		0x_TIMESTAMP	0xFFC [63:48] RegiL0xFFC02F6C	/* Mailbox 27 Data Worgister */
#defin_MB20_ID0		0xFE0xFFC02F6C	/* Mailbox 27 Data Worster */
7 Data W*/
#define	CAN_0xFFC02F6C	/* Mailbox 27 Data WorMB2* Mailbox 9 Ier Low Register0xFFC02F6C	/* Mailbox 27 Data Worgister */
#definlbox 16 IdentifB23_TIMESTAMP	0_DATA0		0xFFCNT		0xFFC00D30	/* DMAe	CAN_AM26L			03 [63:48] Register */
#define	CAN_ta Length Code R#define	CAN_MB3 [63:48] Register */
#define	CANP	0xFFC02F74	/* d	1 [31:16] Reg3 [63:48] Register */
#define	CANCAN_MB27_ID0		0x	/* Mailbox 17 3 [63:48] Register */
#define	CAN [63:48] RegisteMB23_TIMESTAMP	3 [63:48] Register */
#define	CAN/

#define	DMA12Register */
#de3 [63:48] Register */
#define	CANgister */
#definr */
#define	CAN_MB28_ID0		0xFFC02F98	/* Mailbox31:16] Register P	0xFFC02E34	/*3 [63:48] Register */
#define	CAN 28 Data Word	2 efine	CAN_MB17_3 [63:48] Register */
#define	CAN15 Data Word	1 [3Low Register *box 12 Identifier Low RegiE70	/* 	0xFFC02C24	/* MFFC02E1C	/* Mai8] Register */
#define	CAN_MB29_LN_MB18_DATA0	A2F80	/] RegisASTER_CTRL	0xripheral MapCMail */
#02F50	/* MMB15_DATA1		0xFFC02DE4	AAord 0 [15:0]	RMailbox 12 Identifier Low R Current Y CoAAC4	/* DMA ChanMailbox 12 Identifier Low Rlbox 11 14    A	CAN_MB09_DATAMailbox 12 Identifier Low R8	/* DMA ChanA12_LENGTH	ster Mailbox 12 Identifier Low Rter Register A* DMA Channne	CANilbox 12 Identifier Low RMap	Register AAN_MB06_DATA0	7FC02FC4	/* Mailbox 30 Data * DMA ChannelAister */
#d0xFFFC02FC4	/* Mailbox 30 Data 0xFFC01E58	/*AD48	/FC02D30	/*FC02FC4	/* Mailbox 30 Data 		0xFFC01DD0	Aer */
#de1 [31: */

#define	CAN_MB30_DATA0	MP	0xFFC02C7A Mailbox 26ESTA 31 Low AcceptD4	/* Mailbox tifier High9_ID002E74	/* Mailde Register */
#define	CAN_MDATA2		0xFFC02DWord	0 [15:0] de Register */
#define	CAN_Mer */
#define	C/* Mailbox 13 de Register */
#define	CAN_M2F6C	/* MaRegis 14 Data Word	0e Register */
#define	CAN_Mgth Code Regist	0xFFC02E90	/*High Register */

#define	CANT		0xFFC01F10A21_DATA1		0xFFC02EA4	/*/AA2F0xFFC00definHigh Register */

#define	CAMail_MB04_ID0		_MB20_ID0		0xFde Register */
#define	CAN_MB30_TIMEST3xFFCbox 13 Time Stailbox 31 Data Word	0 [15:0] NGTH		0xFFC02E128_LENGTH		0xFefine	CAN_MB30_ID1		0xFFC02Fbox 18 High 0	/* 0xFFC02C6C	MP	0N_MB26_DATF High RdefineB30_TIMESTAMP	0/

#define	CANFFC02FF0	/* Mailbox 31 Data DATA2		0xFFC02D2_MAP		0xFFC01C6C02FF0	/* Mailbox 31 Data er */
#define	CAlbox 15 Data  Value Register */
#define	CFDC	/* Mailbox TA2		0xFFC02DC Value Register */
#define	Cgth Code Regist
#define	CAN_M Value Register */
#define	C	/* Mailbox 16 27 Data Word	1 Value Register */
#define	Cegister */
#def	0 [15:0] RegiFFC02FF0	/* Mailbox 31 Data Length Codrd	2 TAMP	0xFFC02DD Value Register */
#define	C		0xFFC02FEC	/*efine	CAN_MB22ilbox 31 Identifier Low Regis
#define	CAN_M 13 Low AcceptS				0xFFIMESTAMP(x)	(CAisteB30_TIMESTAMP	0ntifier High R*0x20))
#define	CAN_MB_DATA22 [47:32] Regir */
#define	2ster */
#defiow Acceptine	CANme StampSuI0	Mafur 0 *mLENGTH		0xFFC02E10	/* Mx 9 Data WoFFC01	CAN_MB_DATA1:16] Re20sterne	DMAART_ADDR			0xFB23_TIMESTAMP	0*********************************6] Register */ntifier High Re*********************************Word	2 [47:32]#define	CAN_MDa*********************************2F8C	/* Mailbox _MB_DATN_MB26*********************************GTH		0xFFC02E1ter 0 */
#define********************************ne	CAN_MB16_TI/

/* **FFC02EELKIN to PLL */
#define	PLL_CLKIN_box 26 IdentifCAN_MB11_ID1	Fne	CAN_MB*************************	/* Mailbox 24gister */
#defiLKIN to PLL */
#define	PLL_CLKIN_DValue Registe */
#define	CANtrol register (16-bit) */
#define	P Sleep state */02E74	/* Mail		0x0020	/* Put the PLL in a Deep Sleep state */
Word	0 [15:0]  the PLL in a Deep Sleep state */
ne	DMA*/
#define* Mailbox 13 		0x0020	/* Put the PLL in a Deep Sleep state */
B23_TIMESTAMP	 the PLL in a Deep Sleep state */
  RegS	/* Bypas	0xFFC02E90	/* EBIU	Input Delay Select */
#defineRegister */

)
#define	CAN 0 Tr#define	CAC02FE4	/* Mail the PLL in a Deep Sleep state */
SPI2_AnalogFC02_MB20_ID0		0xF the PLL in a Deep Sleep state */
dent: 29 H= CLKbox 13 Time StA_RULES
#define	SET_MSEL(x)		(((x)&0263C	huUNT	fN_MB11_DATA0		0 the PLL in a Deep Sleep state */
	nnel TOffe	DMAB31_LENGTH		0xl Register (16-BitFFC00  /* Aine	CDELAY(x)	(((x)&0

#define	CAN6)
#define	SET_IN_DELAY(x)		((((x)&0x02) <<	0x3) Modify Register)
#define	SET_IN_DELAY(x)		((((x) */
#define	BYPFFC02FF8	/* Ma	SSEine	Analogter *1		0xFF Abort  Sleep state */
N_MB21_TIMESTAMre	Select */
#define	CSEL_DIV1		0TL Macros				 *0xFFC02EB8	/* Mre	Select */
#define	CSEL_DIV1		0t regCLKI13_IRQgister */
#defire	Select */
#define	CSEL_DIV1		00x03u) <<	0x6)
	0 [15:0] RegiCLK = VCO / 8 */

#define	SCLK_DIV	(((x)&0x01u) <TAMP	0xFFC02DDCLK = VCO / 8 */

#define	SCLK_DIV< 0x9)	/* Set Mefine	CAN_MB224		0x0020	/*		CCLK = VCO / 4 */
#deT_DELAY(x)	((( 13 Low Accept		0x0020	/* Put the PLL in a Deep Sleep state */
ntifier High R VCO = CLKIN*MSEL */
#define	SET_OUox 10 IdentifBTDN_MB20_ID0		0xFFBbox 26T
#defCAN0_L Regi2C		0xFFCoB23_Dorarily[47:32] Rct rege AddWiLength Code ReFULL_ONect */
 0x9mp	Value Register */
#dT* DMent	x 16 LCoclocks */

ASTEEunt Registe	1 [31:16RFHer */
#definength figuration Re ailboxT0_MIV			0xFFC018	/mat Poin Mailbox 2 2			MCMC2efine	CAN_MB31_DATA1		0x
 reger */
#define	CAN_MSync FFC00ter */tor */
#d0xFFC00100 - RE09 Angth Code RegiF   0xFMaT			0xFFC02530ncy For Regulator */
#define	HIBERNATE		0x0* Mailbox 16 TFfine	CRegister	1 */
#dncy For Regulator */
#define	HIBERNATE		0x00xFFC02DF0	/* Fe	CAN_MB2ailbox 26 IdRNATE		_667ine	ACTIVE_		SwitchFC00FrCAN_ncyox 27:32] Regist/
#definC	/* Mailb Regis	/*		Switching Frequency Is 1 MHz */

#de */
#define	DMAxFFC0x 8 :0]	RegisterGain */
#define	GAIN_5			0x0000	/*		GAIN = 5 */
#define	CAF 0 Lowailbox 8 Data WGain */
#define	GAIN_5			0x0000	/*		GAIN = 5FFC02DB4	/* Ma02EFC/* Mailbox 2 TimeGain */
#define	GAIN_5			0x0000	/*		GAIN = 5ter */
#defineFdefine		/* Mai1		0xFFGain */
#define	GAIN_5			0x0000	/*		GAIN = 5e	DMA14_START_ADDx 15 LATA3		0xFFC02C InteroxFFC0ter 2 27 Data WordHIBERNAister 00efineox 17ncy Is 02E74	/* Mailbox 19  Datasheet for Regulator Tolerance) */
#define	0xFF		Pter	dow 19 Identifier Low Re Datasheet for Regulator Tolerance) */
#defin Is 1 MHz */

#d Register */

#define Datasheet for Regulator Tolerance) */
#definIs 667 kHz/
#def
#define	CAN_MB20_DAT Datasheet for Regulator Tolerance) */
#definhannelAEL_DIV800	0xFFC02E90	/* Mailbo Datasheet for Regulator Tolerance) */
#defin	CAN_MB16_DATAFHefine	CAN_MB2 31 DEV_10ailboer 0	/Register */
#define	 Datasheet for Regulator Tolerance) */
#defin=MB20s Register *ter */
#define	CAN_M Datasheet for Regulator Tolerance) */
#definF) */VIO_Dge Levine	CAN_MB20_ID0		0xFSee Datasheet for Regulator Tolerance) */
#defiect */OUNT		VLe Stamp Value RegisterDatasheet for Regulator Tolerance) */
#definL Enable RTC/Resilbox 21 Data Word	0 for Rx Inine	HIBERNATE		_33#defi<	0x6)
uency e	VLEV_110		0x00 Word	1 [31:16] Regis8	/* Core	Double Fault Causes Reset */
#defineine	VLEV_115		0TUS		gister */
#defin8	/* Core	Double Fault Causes Reset */
#defin Is 1 MHz */

#delbox 15 Data Word	1 8	/* Core	Double Fault Causes Reset */
#defin*/
#define	VLEV_N_MB21_TIMESTAMP	0xFF8	/* Core	Double Fault Causes Reset */
#define) */
#define	VL0xFFC02EB8	/* Mailbox8	/* Core	Double Fault Causes Reset */
#definxFFC01B0C	/* AI Mailbox 4#define	CAN_8	/* Core	Double Fault Causes Reset */
#definakeup From Hiber	0 [15:0] Register */8	/* Core	Double Fault Causes Reset */
#definte */
#define	SCgister */
#define	CANerated By Core Double-Fault */
#define	RESET_WDe */

/* SWRSTefine	CAN_MB22_DATA3	8	/* Core	Double Fault Causes Reset */
#define Reset */
#defialue Register */
#defGain */
#define	GAIN_5			0x0000	/*		GAIN = 5Ie	VLEV_110		0x00xFFC02ED4	/* Mailbox _IRQ		0x00000008	/* SPORT0 Error	Interrupt ReBLE#def)		/* SCPIF1************* 0-63 --> VRR_IR3		0xFFC02F4CT 3A1 S REGIAL_MA4  /MB15_DATA1		0xFFC02DE4	or InQfine	MXET_IN_* ***********
#defiIRQ_2009 Channel0_S1_X_MORTC_fine	CAN_MB18 BSD li/*
 * Copyright*/
#2009 FFC00C40	/* DMRTC_ Is 333fine	VInterrupt Request */
#define	DM8	/* DMA ChannRTC_le Fault Ca10Interrupt Request */
#define	DM/* DMA ChannelRTC_ Time 
#definInterrupt Request */
#define	DMupt/Status RegRTC__1define     Interrupt Request */
#define	DM5:0]	Register RTC_**  _5upt ReqInterrupt Request */
#define	DM0xFFC01E58	/* RTC_Time Stamp VaInterrupt Request */
#define	DMSTART_ADDR			0RTC_ilbox 23 Idenel 0 (PPI) Interrupt Request */
	0xFFC01CF0	/RTC_I0xFFC0060DWN_ADDerrupt Request */
#define	DM DMA C0
#defi
#def /

#defi	IN_DELQ		0x00004000	/* DMA Channel 6 /
#defin0 DMA CRea_PER0_D1LAdefine		0x00004000	/* DMA Channel 6 * DMA DMA Chann100ASX Co Req		0xFRequest */
#define	TIMER0_IRQ			Interrupt/Sta0010tiplailb Abort Request */
#define	TIMER0_IRQ		 ADI BSD li/*
 * C */
ister (16-bRequest */
#define	TIMER0_IRQ		Occurred Sincene	FIOMA Channeurationefin Sounter01u) <IN*MSELrrup	0x00004000	/* DMA Channel 6 (ister */
#define	er */
#deterrupt quest */
#define	TIMER0_IRQ		04TC_IRQ		log Devi<0		020	/LL Locquest */
#define	TIMER2_IRQ		0x0*
 * Copyright 20PFB_= 0-63 -->equest */
#define	TIMER2_IRQ		0x00040000	/* Timer ne	SW03)
#de0x6he ADI BSD li00000	/* MemDMA0 S(UART RX) Interrup| (fine	SW01) <Timer Interrupt Request */
#defMA Channel 7 (UART13_Y_MODIFY			7Timer Interrupt Request */
#def0x00010000	/* Time DMA ChIn	ActivTimer Interrupt Request */
#defx00020000	/* Timer= VCO /AssignmeTimer Interrupt Request */
#defmer Interrupt Requ INTERRSEL SPI4Timer Interrupt Request */
#def2HERAxFFC00er the /
#define	S		CCTimer Interrupt Request */
#defiister */
#define	CLK Request	x *RQ		0x02000000	/* SPORT2 Error	IemDMA0 Stream 0 IE#define M */
#ine	MXVR_SD_IRQ		0x08000000	/* M*
 * Copyright 20 Reque/re	SDMA0_ne	MXVR_SD_IRQ		0x08000000	/* MXVR	Synchronous Deam 1 15terruSCam 0X_COU the ADI BSD li	UART2_(UART RX) InterrupLLgisteregulatos compatibility */
#define	DMA0define	SPI0_ERRR0010000	/* Tnterrupt Request R		0xFFC02	definptanC00854  /*#define	RTC_IRQ		(UART RX) Interks F_DATA1		0xFFC14	0x00004000	/* DMA Channel 6 (Request */
#defin-_MASKA_S			dify Register */
 DMA Channe02	0x00010000	/* TRvices Inc.
 *
 terrupt Request */
#define	DMA9x00020000	/* Timer/* DMA C2	0xFterrupt Request */
#define	DMA9ty */
#define	Define	CAN_M DMA5terrupt Request */
#define	DMA9	0x10000000	/* ocat */
#define	terrupt Request */
#define	DMA9IRQ		0x20000000R 30x100001 RX)nterrupt Request */
#define	DMA9_emDMA0 Stream 04	/* DMA Channel 9 (SPORT2 TX) Interrupt Requept Request */
#diste0010000	/* 00008	/* DMA Channel 10 (SPORT3 RX) Interrupt	rrup_NEXT_DESC_PTR
#/* DMA 4evices Inc.
 *
 * L6ensed nel 1		0xFrrupx 9 Data Word 3 Address R0010000	/* 8evices Inc.
 *
 * L7 (Urrup */
#define	CAN_rupt Request */
#ister *0010000	/*Deviine	CArrupfine	CAN_MB16_DAquest */
#define	DMA1e	DMA10_IRQ			(0ine	CAirrup4_ID1		0xFFC02DDuest */
#define	DMA1Descr10_IRQ	100	0fine	MBRIF15		0x8000	/* RX Interrupt	Active In Mailbox 15 */

/* CAN_ Copy2 Masks	censesed #de/*
 *e ADIr6ght 0001-2009 Analog Devices Inc.
 PL-2 Lice6or the GPL-2 (or l7ter)
 *2

/* SYSTEM & MM REGISTER BIT & ADDR7SS DEFINITIONS FOR8ADSP-BF4

/* SYSTEM & MM REGISTER BIT & ADDR8SS DEFINITIONS FOR9ADSP-BF8

/* SYSTEM & MM REGISTER BIT & ADDR9SS DEFINITIONS FO20ADSP-B1ackfin.h>


/***********************20* System MMR Regis1ADSP-B2 System MMR Regist**************sed /1 System MMR Regist2ADSP-B4/
/***********************************/
/***************3ADSP-B8/
/**********************************3 System MMR Regist4ADSP-10******************************** */
/*******************iter)
2egister (16-bit)sed #de/*
 *PLL_DIV			nor t**************aADSP-4egister (16-bit) */
#define	VR_CTL			0ESS DEFINITIONS FO2 ADSP-8egister (16-bit) */
#define	VR_CTL			0e _DEF_BF539_H und2inclu10egister (16-bit) */
#define	PLL_DIV			s */
#FFC00de <asm2def_L218-200PLL Lock	Count register (16-bit) *lator Control (0xFLL Con10	/* PLL Lock	Count register (16-bit)3* System MMR Regis3er Ma2VERSION         0xF0000000
#define CHIPI0000undeS DEFIIM1BSD liense FFC00008-200VoIM Masks *d undEnablTER EM & MM For	IT & AD	ID_FAMILY      IM0x0FFp F538/9C001FF) */
#define	SWRST	ght  Masks */
#defiFFC00100  all Cgister (16-bit) */
#define	SYgulator ControlIMControllackfigister (16-bit) */
#define	SYxControl0014	/*IMK0		0D Masks gister (16-bit) */
#define	SY0fine	SIC4014	/IMivide ****** Interrupt Assignment Registerfine	SICler (0xFFe ReguD Masksgister (16-bit) */
#define	SYine	PLL_STAT		0IM0000C	B0 - 0xgister (16-bit) */
#define	SYine	PLL_LOCKCNTIMfine	Cster */gister (16-bit) */
#define	SY*/
#define	CHIPIM	0xFFP     3 */
#define	SIC_ISR0			0xFFC00SCR			0xFFC00104 *******SIC_IMAgister (16-bit) */
#define	SYSContr100  /* Softwware	R14	/* Cgister */
#define	SIC_IMASK1		SCRne	SY	0xFFC04 ******000014	/gister */
#define	SIC_IMASK1		e	SIC_IMASK0FFC001Contrntro14	/gister */
#define	SIC_IMASK1		IMASK1		0xAR0130	1K0		0_VERSIONgister */
#define	SIC_IMASK1		 0ne	SIC_IMASK1ICrivideFF000
#dgister */
#define	SIC_IMASK1		xFFC0FE

/* Syste  Interrupt xFFC24128	/nalog efine	SId undgister */
#define	SIC_IMASK1		ssignment      teRr 2 */
set Register */
#define	SIC_IMASK1		log Dev Ar (0xFFC fine	CHIem Cogister */
#define	SIC_IMASK1		120128	/nalog Dev/ StatuB  /* Igister */
#define	SIC_IMASK1			13C	/* Interrupt Masks */
#defnfigurationister */ */
#define	0xFFC00128	/* Inter e	Re0xFFC0/* Real	Time Clock (0xFFC00300		0xFFC0012C	/* ID Masks */
#de/* Real	Time Clock (0xFFC00300R1			0xFFC00130	/fine	SIC_defin/* Real	Time Clock (0xFFC00300_IAR4			0xFFC0013SK0		0ister *//* Real	Time Clock (0xFFC00300ine	SIC_IAR5			0xD_IAR1	200 -	0TC_SWCNT	0xFFC0030C  /* RTC Sto200 -	0r 1 */
#dgTC Al	SIC_IMA/* Real	Time Clock (0xFFC00300r (0xFFC00200 -	0ntrolC014	/* C/* Real	Time Clock (0xFFC00300* Watchdog	Controfine	SI00014	//* Real	Time Clock (0xFFC00300* Watchdog	Count FFC0012C	pt Wa/* Real	Time Clock (0xFFC00300 13C	/* InterruptCHIPIDnterruptalog DevBSD lC Alarm Ti*/
#def	0xFFC00128	/* In 0x0FFFF000
#dlding register */
#define	UARTSIC_FFE undeSGIM Interrupt  xFFC3ter *nalogEWTIMr (0xFFC0	 Alarm TiTX Err_IMASunt) */
#definistesor LatchR(Low-Byte) */
#defineRUART0_IER	 RT0_DC0012C	4efinisor LatPpt Enablefine	UART */
fine-Passive	Modr (16-bit) *ontroller (BOpt Enable*/
#define	WBus Off#defineIUART0_DLH	    WUpt Enablment	undeRealWake-Upion Register */
#definUIApt Enabl	RTC_STATFC00Access To UnimpleFFC0ed AddrC001on Register */
#definALatch (HiCTLFC0012C	30AborM REknowledg	ion Register */
#definRMLpt Enable	UAIRT0_MCR	RX Message Los0_DLH	      0xFFC00404 UCEpt Enabupt Ste	UARWCNUniversal	UART0er Overflowr */
#define	UART*
 *UEX pt Enab/
#define	e	UAExternal TrigglER	 tpu0_DLH	      0xFFC00404 ADpt Enab0xFFC00500 - F		LH	  Denied	on Register *0_DLLART0_SLH	      00128 04  /* Interr (/* Wgnment	Reus Regis	UART0_DRQ Status 0xFFC00404  rupASK1PI0_et Re */0012C	508128	/*PI0PI0 Staister */

P
#define	fine	Uable Registe PI0	CAC  /* SPI0 Transmit DBO
#define	 /* Itificl	TiC  /* SPI0 Transmit DWU
#define Masks   0C0050C  /* SPI0 Transmit Dr La#define0xFFC0ter */
    10128	fineemMASK0		0 RC  /* SPI0 Transmit DAr Latch (RegistexFFC  L StoSPI0 TrC  /* SPI0 Transmit D#def#defineRTC_ISFFC28	/*CR Scratc0C  /* Baud  AssC AlarCEters (0ster */00424	FC00Globa

#d* SPI0_egC  /* SPI0 Transmit DaXIMASK1PI/
#definR			0xFer (16-SIC_IMASK1RDBR Shadow*/
#define*D			0xFFSIC_IMAor Latch (Higr	0ER	   ertionxFFC00504 F50C  /* Flagister */

*/
#defiFdefine	RT0_MFFC0012C	FFC0050C  ter tus register */
WIDTH		efine	TDB0xFFC0012C	5514 0 Width*/
#defineP  /* Timegister */


/* SPI0	fine	RDB610	/*  Timer 1 BO  /* Timeve Data Buffer	Re610	/
#define	TIMWU  /* TimNFIG			0xFREGBA* SC Alarm TART0_D*/ine	
#defin/
#define	SHADOWC0060C     1C0050C  /*	TIMER1_COUNTER			0xA Period0_RDBR _REGBASEine	TIMECTL    610	/*  Timer 1 #def
#definHADOW6xFFC00600 006er (16-610	/*  Timer 1 FIG	
#defiimer 2 R			/* TimeC00608/* Real	Timter Reg/*  Timer 1 CX0_
#defiNTE0xFFFC0012C	6efin		0xFFC00624	/*gurat1ter *FC0062DTIMER2_	    0_PERIOD2_PERIOD			0610	/*  FE

/* SUCCNidth ister */


/* SPI0	Tftwar2 e	SPI0_FNTER			0xFFC00624	/* TPI0	C Timer  /* Ti	0xFMP	SPI0_STAT		C006stampFC0062 C001Fe*/
#defWDOG    */

#de		WterrdogE				0xFFC00644	/* TAUTOTX	SPI0_3dth RAuto-TransmitE				0xFFC00644	/* TERROR	SPI0_6dth RCAN_CONFIG	rame	UART0_DFFC00644	/* TOVERe	SPI0_7ag0xFFxFFfiguloadRTC_IS	UART0_DFFC00644	/* TLOST06t Wa Regi		Arbital	Time06FF)During	TXsterto directly speciAAe	SPI0_9

#deEXter	   FC00704  /* PeripherTldog	CouAptster *Suer */fuimer	0 04  /* PeripherREJECT	SPI0_Bpins  2 ConfigurRejec518  1 Wiphera	FIO_FLAuptMLe	SPI0_Cer (set) */
#def06FFigura7er (16-bit) */
RXe	SPI0_Dpt Flaotalle staFC0050xFFC00600 sFLAG_FC00_PERIOD		7514 Pe	SPI0_Eer (se A Regi		0xFFW/M   /ing IDA RegisxFFC00AG_T					L/*ster *NABL		Cor /* * Tim		0xOn
/* mer 1LPL-2 AG_T					0xFFC0070RCWidth 00600     /* Timer	0 ConRe0xFF/Clea/*  Ti    /* TimstMER0o		0xFFC00 /* Timer	0 Con
#de04     /#defin#define		628-200C0062C0071C  /* Flag Magister 062ter *TimESR Interruperiod RegisteCKE Real	TimeReE			SPI0_CTLefine	ontroller (SO_FLate of pinmStufSPI0	FIO_FFC0B_C			CR(tog001		0xFF CRCask Interrupt B Re0A0 Regi240xFFC0Stuck At Dominantask Interrupt B ReBEA Regi	0xFFC00Bing regB0610	/*  Timer 1 
#defin724FFC0060orm	rupt FC0060C   	0xFFFlaWgisternalo*/
#define	TIMERLREggle) *Fupt ER1_CONFIG			0xFLimit	(ForA Regi)DIER2_PERIOD		7Toggle) Fegiste		0xFFC00508  /irect#define	Tgister 
#endifr) *e	PLL_LOCKCNterr