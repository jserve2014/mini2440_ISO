/*
 * wm8350-regmap.c  --  Wolfson Microelectronics WM8350 register map
 *
 * This file splits out the tables describing the defaults and access
 * status of the WM8350 registers since they are rather large.
 *
 * Copyright 2007, 2008 Wolfson Microelectronics PLC.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/mfd/wm8350/core.h>

#ifdef CONFIG_MFD_WM8350_CONFIG_MODE_0

#undef WM8350_HAVE_CONFIG_MODE
#define WM8350_HAVE_CONFIG_MODE

const u16 wm8350_mode0_defaults[] = {
	0x17FF,     /* R0   - Reset/ID */
	0x1000,     /* R1   - ID */
	0x0000,     /* R2 */
	0x1002,     /* R3   - System Control 1 */
	0x0004,     /* R4   - System Control 2 */
	0x0000,     /* R5   - System Hibernate */
	0x8A00,     /* R6   - Interface Control */
	0x0000,     /* R7 */
	0x8000,     /* R8   - Power mgmt (1) */
	0x0000,     /* R9   - Power mgmt (2) */
	0x0000,     /* R10  - Power mgmt (3) */
	0x2000,     /* R11  - Power mgmt (4) */
	0x0E00,     /* R12  - Power mgmt (5) */
	0x0000,     /* R13  - Power mgmt (6) */
	0x0000,     /* R14  - Power mgmt (7) */
	0x0000,     /* R15 */
	0x0000,     /* R16  - RTC Seconds/Minutes */
	0x0100,     /* R17  - RTC Hours/Day */
	0x0101,     /* R18  - RTC Date/Month */
	0x1400,     /* R19  - RTC Year */
	0x0000,     /* R20  - Alarm Seconds/Minutes */
	0x0000,     /* R21  - Alarm Hours/Day */
	0x0000,     /* R22  - Alarm Date/Month */
	0x0320,     /* R23  - RTC Time Control */
	0x0000,     /* R24  - System Interrupts */
	0x0000,     /* R25  - Interrupt Status 1 */
	0x0000,     /* R26  - Interrupt Status 2 */
	0x0000,     /* R27  - Power Up Interrupt Status */
	0x0000,     /* R28  - Under Voltage Interrupt status */
	0x0000,     /* R29  - Over Current Interrupt status */
	0x0000,     /* R30  - GPIO Interrupt Status */
	0x0000,     /* R31  - Comparator Interrupt Status */
	0x3FFF,     /* R32  - System Interrupts Mask */
	0x0000,     /* R33  - Interrupt Status 1 Mask */
	0x0000,     /* R34  - Interrupt Status 2 Mask */
	0x0000,     /* R35  - Power Up Interrupt Status Mask */
	0x0000,     /* R36  - Under Voltage Interrupt status Mask */
	0x0000,     /* R37  - Over Current Interrupt status Mask */
	0x0000,     /* R38  - GPIO Interrupt Status Mask */
	0x0000,     /* R39  - Comparator Interrupt Status Mask */
	0x0040,     /* R40  - Clock Control 1 */
	0x0000,     /* R41  - Clock Control 2 */
	0x3B00,     /* R42  - FLL Control 1 */
	0x7086,     /* R43  - FLL Control 2 */
	0xC226,     /* R44  - FLL Control 3 */
	0x0000,     /* R45  - FLL Control 4 */
	0x0000,     /* R46 */
	0x0000,     /* R47 */
	0x0000,     /* R48  - DAC Control */
	0x0000,     /* R49 */
	0x00C0,     /* R50  - DAC Digital Volume L */
	0x00C0,     /* R51  - DAC Digital Volume R */
	0x0000,     /* R52 */
	0x0040,     /* R53  - DAC LR Rate */
	0x0000,     /* R54  - DAC Clock Control */
	0x0000,     /* R55 */
	0x0000,     /* R56 */
	0x0000,     /* R57 */
	0x4000,     /* R58  - DAC Mute */
	0x0000,     /* R59  - DAC Mute Volume */
	0x0000,     /* R60  - DAC Side */
	0x0000,     /* R61 */
	0x0000,     /* R62 */
	0x0000,     /* R63 */
	0x8000,     /* R64  - ADC Control */
	0x0000,     /* R65 */
	0x00C0,     /* R66  - ADC Digital Volume L */
	0x00C0,     /* R67  - ADC Digital Volume R */
	0x0000,     /* R68  - ADC Divider */
	0x0000,     /* R69 */
	0x0040,     /* R70  - ADC LR Rate */
	0x0000,     /* R71 */
	0x0303,     /* R72  - Input Control */
	0x0000,     /* R73  - IN3 Input Control */
	0x0000,     /* R74  - Mic Bias Control */
	0x0000,     /* R75 */
	0x0000,     /* R76  - Output Control */
	0x0000,     /* R77  - Jack Detect */
	0x0000,     /* R78  - Anti Pop Control */
	0x0000,     /* R79 */
	0x0040,     /* R80  - Left Input Volume */
	0x0040,     /* R81  - Right Input Volume */
	0x0000,     /* R82 */
	0x0000,     /* R83 */
	0x0000,     /* R84 */
	0x0000,     /* R85 */
	0x0000,     /* R86 */
	0x0000,     /* R87 */
	0x0800,     /* R88  - Left Mixer Control */
	0x1000,     /* R89  - Right Mixer Control */
	0x0000,     /* R90 */
	0x0000,     /* R91 */
	0x0000,     /* R92  - OUT3 Mixer Control */
	0x0000,     /* R93  - OUT4 Mixer Control */
	0x0000,     /* R94 */
	0x0000,     /* R95 */
	0x0000,     /* R96  - Output Left Mixer Volume */
	0x0000,     /* R97  - Output Right Mixer Volume */
	0x0000,     /* R98  - Input Mixer Volume L */
	0x0000,     /* R99  - Input Mixer Volume R */
	0x0000,     /* R100 - Input Mixer Volume */
	0x0000,     /* R101 */
	0x0000,     /* R102 */
	0x0000,     /* R103 */
	0x00E4,     /* R104 - LOUT1 Volume */
	0x00E4,     /* R105 - ROUT1 Volume */
	0x00E4,     /* R106 - LOUT2 Volume */
	0x02E4,     /* R107 - ROUT2 Volume */
	0x0000,     /* R108 */
	0x0000,     /* R109 */
	0x0000,     /* R110 */
	0x0000,     /* R111 - BEEP Volume */
	0x0A00,     /* R112 - AI Formating */
	0x0000,     /* R113 - ADC DAC COMP */
	0x0020,     /* R114 - AI ADC Control */
	0x0020,     /* R115 - AI DAC Control */
	0x0000,     /* R116 - AIF Test */
	0x0000,     /* R117 */
	0x0000,     /* R118 */
	0x0000,     /* R119 */
	0x0000,     /* R120 */
	0x0000,     /* R121 */
	0x0000,     /* R122 */
	0x0000,     /* R123 */
	0x0000,     /* R124 */
	0x0000,     /* R125 */
	0x0000,     /* R126 */
	0x0000,     /* R127 */
	0x1FFF,     /* R128 - GPIO Debounce */
	0x0000,     /* R129 - GPIO Pin pull up Control */
	0x03FC,     /* R130 - GPIO Pull down Control */
	0x0000,     /* R131 - GPIO Interrupt Mode */
	0x0000,     /* R132 */
	0x0000,     /* R133 - GPIO Control */
	0x0FFC,     /* R134 - GPIO Configuration (i/o) */
	0x0FFC,     /* R135 - GPIO Pin Polarity / Type */
	0x0000,     /* R136 */
	0x0000,     /* R137 */
	0x0000,     /* R138 */
	0x0000,     /* R139 */
	0x0013,     /* R140 - GPIO Function Select 1 */
	0x0000,     /* R141 - GPIO Function Select 2 */
	0x0000,     /* R142 - GPIO Function Select 3 */
	0x0003,     /* R143 - GPIO Function Select 4 */
	0x0000,     /* R144 - Digitiser Control (1) */
	0x0002,     /* R145 - Digitiser Control (2) */
	0x0000,     /* R146 */
	0x0000,     /* R147 */
	0x0000,     /* R148 */
	0x0000,     /* R149 */
	0x0000,     /* R150 */
	0x0000,     /* R151 */
	0x7000,     /* R152 - AUX1 Readback */
	0x7000,     /* R153 - AUX2 Readback */
	0x7000,     /* R154 - AUX3 Readback */
	0x7000,     /* R155 - AUX4 Readback */
	0x0000,     /* R156 - USB Voltage Readback */
	0x0000,     /* R157 - LINE Voltage Readback */
	0x0000,     /* R158 - BATT Voltage Readback */
	0x0000,     /* R159 - Chip Temp Readback */
	0x0000,     /* R160 */
	0x0000,     /* R161 */
	0x0000,     /* R162 */
	0x0000,     /* R163 - Generic Comparator Control */
	0x0000,     /* R164 - Generic comparator 1 */
	0x0000,     /* R165 - Generic comparator 2 */
	0x0000,     /* R166 - Generic comparator 3 */
	0x0000,     /* R167 - Generic comparator 4 */
	0xA00F,     /* R168 - Battery Charger Control 1 */
	0x0B06,     /* R169 - Battery Charger Control 2 */
	0x0000,     /* R170 - Battery Charger Control 3 */
	0x0000,     /* R171 */
	0x0000,     /* R172 - Current Sink Driver A */
	0x0000,     /* R173 - CSA Flash control */
	0x0000,     /* R174 - Current Sink Driver B */
	0x0000,     /* R175 - CSB Flash control */
	0x0000,     /* R176 - DCDC/LDO requested */
	0x002D,     /* R177 - DCDC Active options */
	0x0000,     /* R178 - DCDC Sleep options */
	0x0025,     /* R179 - Power-check comparator */
	0x000E,     /* R180 - DCDC1 Control */
	0x0000,     /* R181 - DCDC1 Timeouts */
	0x1006,     /* R182 - DCDC1 Low Power */
	0x0018,     /* R183 - DCDC2 Control */
	0x0000,     /* R184 - DCDC2 Timeouts */
	0x0000,     /* R185 */
	0x0000,     /* R186 - DCDC3 Control */
	0x0000,     /* R187 - DCDC3 Timeouts */
	0x0006,     /* R188 - DCDC3 Low Power */
	0x0000,     /* R189 - DCDC4 Control */
	0x0000,     /* R190 - DCDC4 Timeouts */
	0x0006,     /* R191 - DCDC4 Low Power */
	0x0008,     /* R192 - DCDC5 Control */
	0x0000,     /* R193 - DCDC5 Timeouts */
	0x0000,     /* R194 */
	0x0000,     /* R195 - DCDC6 Control */
	0x0000,     /* R196 - DCDC6 Timeouts */
	0x0006,     /* R197 - DCDC6 Low Power */
	0x0000,     /* R198 */
	0x0003,     /* R199 - Limit Switch Control */
	0x001C,     /* R200 - LDO1 Control */
	0x0000,     /* R201 - LDO1 Timeouts */
	0x001C,     /* R202 - LDO1 Low Power */
	0x001B,     /* R203 - LDO2 Control */
	0x0000,     /* R204 - LDO2 Timeouts */
	0x001C,     /* R205 - LDO2 Low Power */
	0x001B,     /* R206 - LDO3 Control */
	0x0000,     /* R207 - LDO3 Timeouts */
	0x001C,     /* R208 - LDO3 Low Power */
	0x001B,     /* R209 - LDO4 Control */
	0x0000,     /* R210 - LDO4 Timeouts */
	0x001C,     /* R211 - LDO4 Low Power */
	0x0000,     /* R212 */
	0x0000,     /* R213 */
	0x0000,     /* R214 */
	0x0000,     /* R215 - VCC_FAULT Masks */
	0x001F,     /* R216 - Main Bandgap Control */
	0x0000,     /* R217 - OSC Control */
	0x9000,     /* R218 - RTC Tick Control */
	0x0000,     /* R219 */
	0x4000,     /* R220 - RAM BIST 1 */
	0x0000,     /* R221 */
	0x0000,     /* R222 */
	0x0000,     /* R223 */
	0x0000,     /* R224 */
	0x0000,     /* R225 - DCDC/LDO status */
	0x0000,     /* R226 */
	0x0000,     /* R227 */
	0x0000,     /* R228 */
	0x0000,     /* R229 */
	0xE000,     /* R230 - GPIO Pin Status */
	0x0000,     /* R231 */
	0x0000,     /* R232 */
	0x0000,     /* R233 */
	0x0000,     /* R234 */
	0x0000,     /* R235 */
	0x0000,     /* R236 */
	0x0000,     /* R237 */
	0x0000,     /* R238 */
	0x0000,     /* R239 */
	0x0000,     /* R240 */
	0x0000,     /* R241 */
	0x0000,     /* R242 */
	0x0000,     /* R243 */
	0x0000,     /* R244 */
	0x0000,     /* R245 */
	0x0000,     /* R246 */
	0x0000,     /* R247 */
	0x0000,     /* R248 */
	0x0000,     /* R249 */
	0x0000,     /* R250 */
	0x0000,     /* R251 */
	0x0000,     /* R252 */
	0x0000,     /* R253 */
	0x0000,     /* R254 */
	0x0000,     /* R255 */
};
#endif

#ifdef CONFIG_MFD_WM8350_CONFIG_MODE_1

#undef WM8350_HAVE_CONFIG_MODE
#define WM8350_HAVE_CONFIG_MODE

const u16 wm8350_mode1_defaults[] = {
	0x17FF,     /* R0   - Reset/ID */
	0x1000,     /* R1   - ID */
	0x0000,     /* R2 */
	0x1002,     /* R3   - System Control 1 */
	0x0014,     /* R4   - System Control 2 */
	0x0000,     /* R5   - System Hibernate */
	0x8A00,     /* R6   - Interface Control */
	0x0000,     /* R7 */
	0x8000,     /* R8   - Power mgmt (1) */
	0x0000,     /* R9   - Power mgmt (2) */
	0x0000,     /* R10  - Power mgmt (3) */
	0x2000,     /* R11  - Power mgmt (4) */
	0x0E00,     /* R12  - Power mgmt (5) */
	0x0000,     /* R13  - Power mgmt (6) */
	0x0000,     /* R14  - Power mgmt (7) */
	0x0000,     /* R15 */
	0x0000,     /* R16  - RTC Seconds/Minutes */
	0x0100,     /* R17  - RTC Hours/Day */
	0x0101,     /* R18  - RTC Date/Month */
	0x1400,     /* R19  - RTC Year */
	0x0000,     /* R20  - Alarm Seconds/Minutes */
	0x0000,     /* R21  - Alarm Hours/Day */
	0x0000,     /* R22  - Alarm Date/Month */
	0x0320,     /* R23  - RTC Time Control */
	0x0000,     /* R24  - System Interrupts */
	0x0000,     /* R25  - Interrupt Status 1 */
	0x0000,     /* R26  - Interrupt Status 2 */
	0x0000,     /* R27  - Power Up Interrupt Status */
	0x0000,     /* R28  - Under Voltage Interrupt status */
	0x0000,     /* R29  - Over Current Interrupt status */
	0x0000,     /* R30  - GPIO Interrupt Status */
	0x0000,     /* R31  - Comparator Interrupt Status */
	0x3FFF,     /* R32  - System Interrupts Mask */
	0x0000,     /* R33  - Interrupt Status 1 Mask */
	0x0000,     /* R34  - Interrupt Status 2 Mask */
	0x0000,     /* R35  - Power Up Interrupt Status Mask */
	0x0000,     /* R36  - Under Voltage Interrupt status Mask */
	0x0000,     /* R37  - Over Current Interrupt status Mask */
	0x0000,     /* R38  - GPIO Interrupt Status Mask */
	0x0000,     /* R39  - Comparator Interrupt Status Mask */
	0x0040,     /* R40  - Clock Control 1 */
	0x0000,     /* R41  - Clock Control 2 */
	0x3B00,     /* R42  - FLL Control 1 */
	0x7086,     /* R43  - FLL Control 2 */
	0xC226,     /* R44  - FLL Control 3 */
	0x0000,     /* R45  - FLL Control 4 */
	0x0000,     /* R46 */
	0x0000,     /* R47 */
	0x0000,     /* R48  - DAC Control */
	0x0000,     /* R49 */
	0x00C0,     /* R50  - DAC Digital Volume L */
	0x00C0,     /* R51  - DAC Digital Volume R */
	0x0000,     /* R52 */
	0x0040,     /* R53  - DAC LR Rate */
	0x0000,     /* R54  - DAC Clock Control */
	0x0000,     /* R55 */
	0x0000,     /* R56 */
	0x0000,     /* R57 */
	0x4000,     /* R58  - DAC Mute */
	0x0000,     /* R59  - DAC Mute Volume */
	0x0000,     /* R60  - DAC Side */
	0x0000,     /* R61 */
	0x0000,     /* R62 */
	0x0000,     /* R63 */
	0x8000,     /* R64  - ADC Control */
	0x0000,     /* R65 */
	0x00C0,     /* R66  - ADC Digital Volume L */
	0x00C0,     /* R67  - ADC Digital Volume R */
	0x0000,     /* R68  - ADC Divider */
	0x0000,     /* R69 */
	0x0040,     /* R70  - ADC LR Rate */
	0x0000,     /* R71 */
	0x0303,     /* R72  - Input Control */
	0x0000,     /* R73  - IN3 Input Control */
	0x0000,     /* R74  - Mic Bias Control */
	0x0000,     /* R75 */
	0x0000,     /* R76  - Output Control */
	0x0000,     /* R77  - Jack Detect */
	0x0000,     /* R78  - Anti Pop Control */
	0x0000,     /* R79 */
	0x0040,     /* R80  - Left Input Volume */
	0x0040,     /* R81  - Right Input Volume */
	0x0000,     /* R82 */
	0x0000,     /* R83 */
	0x0000,     /* R84 */
	0x0000,     /* R85 */
	0x0000,     /* R86 */
	0x0000,     /* R87 */
	0x0800,     /* R88  - Left Mixer Control */
	0x1000,     /* R89  - Right Mixer Control */
	0x0000,     /* R90 */
	0x0000,     /* R91 */
	0x0000,     /* R92  - OUT3 Mixer Control */
	0x0000,     /* R93  - OUT4 Mixer Control */
	0x0000,     /* R94 */
	0x0000,     /* R95 */
	0x0000,     /* R96  - Output Left Mixer Volume */
	0x0000,     /* R97  - Output Right Mixer Volume */
	0x0000,     /* R98  - Input Mixer Volume L */
	0x0000,     /* R99  - Input Mixer Volume R */
	0x0000,     /* R100 - Input Mixer Volume */
	0x0000,     /* R101 */
	0x0000,     /* R102 */
	0x0000,     /* R103 */
	0x00E4,     /* R104 - LOUT1 Volume */
	0x00E4,     /* R105 - ROUT1 Volume */
	0x00E4,     /* R106 - LOUT2 Volume */
	0x02E4,     /* R107 - ROUT2 Volume */
	0x0000,     /* R108 */
	0x0000,     /* R109 */
	0x0000,     /* R110 */
	0x0000,     /* R111 - BEEP Volume */
	0x0A00,     /* R112 - AI Formating */
	0x0000,     /* R113 - ADC DAC COMP */
	0x0020,     /* R114 - AI ADC Control */
	0x0020,     /* R115 - AI DAC Control */
	0x0000,     /* R116 - AIF Test */
	0x0000,     /* R117 */
	0x0000,     /* R118 */
	0x0000,     /* R119 */
	0x0000,     /* R120 */
	0x0000,     /* R121 */
	0x0000,     /* R122 */
	0x0000,     /* R123 */
	0x0000,     /* R124 */
	0x0000,     /* R125 */
	0x0000,     /* R126 */
	0x0000,     /* R127 */
	0x1FFF,     /* R128 - GPIO Debounce */
	0x0000,     /* R129 - GPIO Pin pull up Control */
	0x03FC,     /* R130 - GPIO Pull down Control */
	0x0000,     /* R131 - GPIO Interrupt Mode */
	0x0000,     /* R132 */
	0x0000,     /* R133 - GPIO Control */
	0x00FB,     /* R134 - GPIO Configuration (i/o) */
	0x04FE,     /* R135 - GPIO Pin Polarity / Type */
	0x0000,     /* R136 */
	0x0000,     /* R137 */
	0x0000,     /* R138 */
	0x0000,     /* R139 */
	0x0312,     /* R140 - GPIO Function Select 1 */
	0x1003,     /* R141 - GPIO Function Select 2 */
	0x1331,     /* R142 - GPIO Function Select 3 */
	0x0003,     /* R143 - GPIO Function Select 4 */
	0x0000,     /* R144 - Digitiser Control (1) */
	0x0002,     /* R145 - Digitiser Control (2) */
	0x0000,     /* R146 */
	0x0000,     /* R147 */
	0x0000,     /* R148 */
	0x0000,     /* R149 */
	0x0000,     /* R150 */
	0x0000,     /* R151 */
	0x7000,     /* R152 - AUX1 Readback */
	0x7000,     /* R153 - AUX2 Readback */
	0x7000,     /* R154 - AUX3 Readback */
	0x7000,     /* R155 - AUX4 Readback */
	0x0000,     /* R156 - USB Voltage Readback */
	0x0000,     /* R157 - LINE Voltage Readback */
	0x0000,     /* R158 - BATT Voltage Readback */
	0x0000,     /* R159 - Chip Temp Readback */
	0x0000,     /* R160 */
	0x0000,     /* R161 */
	0x0000,     /* R162 */
	0x0000,     /* R163 - Generic Comparator Control */
	0x0000,     /* R164 - Generic comparator 1 */
	0x0000,     /* R165 - Generic comparator 2 */
	0x0000,     /* R166 - Generic comparator 3 */
	0x0000,     /* R167 - Generic comparator 4 */
	0xA00F,     /* R168 - Battery Charger Control 1 */
	0x0B06,     /* R169 - Battery Charger Control 2 */
	0x0000,     /* R170 - Battery Charger Control 3 */
	0x0000,     /* R171 */
	0x0000,     /* R172 - Current Sink Driver A */
	0x0000,     /* R173 - CSA Flash control */
	0x0000,     /* R174 - Current Sink Driver B */
	0x0000,     /* R175 - CSB Flash control */
	0x0000,     /* R176 - DCDC/LDO requested */
	0x002D,     /* R177 - DCDC Active options */
	0x0000,     /* R178 - DCDC Sleep options */
	0x0025,     /* R179 - Power-check comparator */
	0x0062,     /* R180 - DCDC1 Control */
	0x0400,     /* R181 - DCDC1 Timeouts */
	0x1006,     /* R182 - DCDC1 Low Power */
	0x0018,     /* R183 - DCDC2 Control */
	0x0000,     /* R184 - DCDC2 Timeouts */
	0x0000,     /* R185 */
	0x0026,     /* R186 - DCDC3 Control */
	0x0400,     /* R187 - DCDC3 Timeouts */
	0x0006,     /* R188 - DCDC3 Low Power */
	0x0062,     /* R189 - DCDC4 Control */
	0x0400,     /* R190 - DCDC4 Timeouts */
	0x0006,     /* R191 - DCDC4 Low Power */
	0x0008,     /* R192 - DCDC5 Control */
	0x0000,     /* R193 - DCDC5 Timeouts */
	0x0000,     /* R194 */
	0x0026,     /* R195 - DCDC6 Control */
	0x0800,     /* R196 - DCDC6 Timeouts */
	0x0006,     /* R197 - DCDC6 Low Power */
	0x0000,     /* R198 */
	0x0003,     /* R199 - Limit Switch Control */
	0x0006,     /* R200 - LDO1 Control */
	0x0400,     /* R201 - LDO1 Timeouts */
	0x001C,     /* R202 - LDO1 Low Power */
	0x0006,     /* R203 - LDO2 Control */
	0x0400,     /* R204 - LDO2 Timeouts */
	0x001C,     /* R205 - LDO2 Low Power */
	0x001B,     /* R206 - LDO3 Control */
	0x0000,     /* R207 - LDO3 Timeouts */
	0x001C,     /* R208 - LDO3 Low Power */
	0x001B,     /* R209 - LDO4 Control */
	0x0000,     /* R210 - LDO4 Timeouts */
	0x001C,     /* R211 - LDO4 Low Power */
	0x0000,     /* R212 */
	0x0000,     /* R213 */
	0x0000,     /* R214 */
	0x0000,     /* R215 - VCC_FAULT Masks */
	0x001F,     /* R216 - Main Bandgap Control */
	0x0000,     /* R217 - OSC Control */
	0x9000,     /* R218 - RTC Tick Control */
	0x0000,     /* R219 */
	0x4000,     /* R220 - RAM BIST 1 */
	0x0000,     /* R221 */
	0x0000,     /* R222 */
	0x0000,     /* R223 */
	0x0000,     /* R224 */
	0x0000,     /* R225 - DCDC/LDO status */
	0x0000,     /* R226 */
	0x0000,     /* R227 */
	0x0000,     /* R228 */
	0x0000,     /* R229 */
	0xE000,     /* R230 - GPIO Pin Status */
	0x0000,     /* R231 */
	0x0000,     /* R232 */
	0x0000,     /* R233 */
	0x0000,     /* R234 */
	0x0000,     /* R235 */
	0x0000,     /* R236 */
	0x0000,     /* R237 */
	0x0000,     /* R238 */
	0x0000,     /* R239 */
	0x0000,     /* R240 */
	0x0000,     /* R241 */
	0x0000,     /* R242 */
	0x0000,     /* R243 */
	0x0000,     /* R244 */
	0x0000,     /* R245 */
	0x0000,     /* R246 */
	0x0000,     /* R247 */
	0x0000,     /* R248 */
	0x0000,     /* R249 */
	0x0000,     /* R250 */
	0x0000,     /* R251 */
	0x0000,     /* R252 */
	0x0000,     /* R253 */
	0x0000,     /* R254 */
	0x0000,     /* R255 */
};
#endif

#ifdef CONFIG_MFD_WM8350_CONFIG_MODE_2

#undef WM8350_HAVE_CONFIG_MODE
#define WM8350_HAVE_CONFIG_MODE

const u16 wm8350_mode2_defaults[] = {
	0x17FF,     /* R0   - Reset/ID */
	0x1000,     /* R1   - ID */
	0x0000,     /* R2 */
	0x1002,     /* R3   - System Control 1 */
	0x0014,     /* R4   - System Control 2 */
	0x0000,     /* R5   - System Hibernate */
	0x8A00,     /* R6   - Interface Control */
	0x0000,     /* R7 */
	0x8000,     /* R8   - Power mgmt (1) */
	0x0000,     /* R9   - Power mgmt (2) */
	0x0000,     /* R10  - Power mgmt (3) */
	0x2000,     /* R11  - Power mgmt (4) */
	0x0E00,     /* R12  - Power mgmt (5) */
	0x0000,     /* R13  - Power mgmt (6) */
	0x0000,     /* R14  - Power mgmt (7) */
	0x0000,     /* R15 */
	0x0000,     /* R16  - RTC Seconds/Minutes */
	0x0100,     /* R17  - RTC Hours/Day */
	0x0101,     /* R18  - RTC Date/Month */
	0x1400,     /* R19  - RTC Year */
	0x0000,     /* R20  - Alarm Seconds/Minutes */
	0x0000,     /* R21  - Alarm Hours/Day */
	0x0000,     /* R22  - Alarm Date/Month */
	0x0320,     /* R23  - RTC Time Control */
	0x0000,     /* R24  - System Interrupts */
	0x0000,     /* R25  - Interrupt Status 1 */
	0x0000,     /* R26  - Interrupt Status 2 */
	0x0000,     /* R27  - Power Up Interrupt Status */
	0x0000,     /* R28  - Under Voltage Interrupt status */
	0x0000,     /* R29  - Over Current Interrupt status */
	0x0000,     /* R30  - GPIO Interrupt Status */
	0x0000,     /* R31  - Comparator Interrupt Status */
	0x3FFF,     /* R32  - System Interrupts Mask */
	0x0000,     /* R33  - Interrupt Status 1 Mask */
	0x0000,     /* R34  - Interrupt Status 2 Mask */
	0x0000,     /* R35  - Power Up Interrupt Status Mask */
	0x0000,     /* R36  - Under Voltage Interrupt status Mask */
	0x0000,     /* R37  - Over Current Interrupt status Mask */
	0x0000,     /* R38  - GPIO Interrupt Status Mask */
	0x0000,     /* R39  - Comparator Interrupt Status Mask */
	0x0040,     /* R40  - Clock Control 1 */
	0x0000,     /* R41  - Clock Control 2 */
	0x3B00,     /* R42  - FLL Control 1 */
	0x7086,     /* R43  - FLL Control 2 */
	0xC226,     /* R44  - FLL Control 3 */
	0x0000,     /* R45  - FLL Control 4 */
	0x0000,     /* R46 */
	0x0000,     /* R47 */
	0x0000,     /* R48  - DAC Control */
	0x0000,     /* R49 */
	0x00C0,     /* R50  - DAC Digital Volume L */
	0x00C0,     /* R51  - DAC Digital Volume R */
	0x0000,     /* R52 */
	0x0040,     /* R53  - DAC LR Rate */
	0x0000,     /* R54  - DAC Clock Control */
	0x0000,     /* R55 */
	0x0000,     /* R56 */
	0x0000,     /* R57 */
	0x4000,     /* R58  - DAC Mute */
	0x0000,     /* R59  - DAC Mute Volume */
	0x0000,     /* R60  - DAC Side */
	0x0000,     /* R61 */
	0x0000,     /* R62 */
	0x0000,     /* R63 */
	0x8000,     /* R64  - ADC Control */
	0x0000,     /* R65 */
	0x00C0,     /* R66  - ADC Digital Volume L */
	0x00C0,     /* R67  - ADC Digital Volume R */
	0x0000,     /* R68  - ADC Divider */
	0x0000,     /* R69 */
	0x0040,     /* R70  - ADC LR Rate */
	0x0000,     /* R71 */
	0x0303,     /* R72  - Input Control */
	0x0000,     /* R73  - IN3 Input Control */
	0x0000,     /* R74  - Mic Bias Control */
	0x0000,     /* R75 */
	0x0000,     /* R76  - Output Control */
	0x0000,     /* R77  - Jack Detect */
	0x0000,     /* R78  - Anti Pop Control */
	0x0000,     /* R79 */
	0x0040,     /* R80  - Left Input Volume */
	0x0040,     /* R81  - Right Input Volume */
	0x0000,     /* R82 */
	0x0000,     /* R83 */
	0x0000,     /* R84 */
	0x0000,     /* R85 */
	0x0000,     /* R86 */
	0x0000,     /* R87 */
	0x0800,     /* R88  - Left Mixer Control */
	0x1000,     /* R89  - Right Mixer Control */
	0x0000,     /* R90 */
	0x0000,     /* R91 */
	0x0000,     /* R92  - OUT3 Mixer Control */
	0x0000,     /* R93  - OUT4 Mixer Control */
	0x0000,     /* R94 */
	0x0000,     /* R95 */
	0x0000,     /* R96  - Output Left Mixer Volume */
	0x0000,     /* R97  - Output Right Mixer Volume */
	0x0000,     /* R98  - Input Mixer Volume L */
	0x0000,     /* R99  - Input Mixer Volume R */
	0x0000,     /* R100 - Input Mixer Volume */
	0x0000,     /* R101 */
	0x0000,     /* R102 */
	0x0000,     /* R103 */
	0x00E4,     /* R104 - LOUT1 Volume */
	0x00E4,     /* R105 - ROUT1 Volume */
	0x00E4,     /* R106 - LOUT2 Volume */
	0x02E4,     /* R107 - ROUT2 Volume */
	0x0000,     /* R108 */
	0x0000,     /* R109 */
	0x0000,     /* R110 */
	0x0000,     /* R111 - BEEP Volume */
	0x0A00,     /* R112 - AI Formating */
	0x0000,     /* R113 - ADC DAC COMP */
	0x0020,     /* R114 - AI ADC Control */
	0x0020,     /* R115 - AI DAC Control */
	0x0000,     /* R116 - AIF Test */
	0x0000,     /* R117 */
	0x0000,     /* R118 */
	0x0000,     /* R119 */
	0x0000,     /* R120 */
	0x0000,     /* R121 */
	0x0000,     /* R122 */
	0x0000,     /* R123 */
	0x0000,     /* R124 */
	0x0000,     /* R125 */
	0x0000,     /* R126 */
	0x0000,     /* R127 */
	0x1FFF,     /* R128 - GPIO Debounce */
	0x0000,     /* R129 - GPIO Pin pull up Control */
	0x03FC,     /* R130 - GPIO Pull down Control */
	0x0000,     /* R131 - GPIO Interrupt Mode */
	0x0000,     /* R132 */
	0x0000,     /* R133 - GPIO Control */
	0x08FB,     /* R134 - GPIO Configuration (i/o) */
	0x0CFE,     /* R135 - GPIO Pin Polarity / Type */
	0x0000,     /* R136 */
	0x0000,     /* R137 */
	0x0000,     /* R138 */
	0x0000,     /* R139 */
	0x0312,     /* R140 - GPIO Function Select 1 */
	0x0003,     /* R141 - GPIO Function Select 2 */
	0x2331,     /* R142 - GPIO Function Select 3 */
	0x0003,     /* R143 - GPIO Function Select 4 */
	0x0000,     /* R144 - Digitiser Control (1) */
	0x0002,     /* R145 - Digitiser Control (2) */
	0x0000,     /* R146 */
	0x0000,     /* R147 */
	0x0000,     /* R148 */
	0x0000,     /* R149 */
	0x0000,     /* R150 */
	0x0000,     /* R151 */
	0x7000,     /* R152 - AUX1 Readback */
	0x7000,     /* R153 - AUX2 Readback */
	0x7000,     /* R154 - AUX3 Readback */
	0x7000,     /* R155 - AUX4 Readback */
	0x0000,     /* R156 - USB Voltage Readback */
	0x0000,     /* R157 - LINE Voltage Readback */
	0x0000,     /* R158 - BATT Voltage Readback */
	0x0000,     /* R159 - Chip Temp Readback */
	0x0000,     /* R160 */
	0x0000,     /* R161 */
	0x0000,     /* R162 */
	0x0000,     /* R163 - Generic Comparator Control */
	0x0000,     /* R164 - Generic comparator 1 */
	0x0000,     /* R165 - Generic comparator 2 */
	0x0000,     /* R166 - Generic comparator 3 */
	0x0000,     /* R167 - Generic comparator 4 */
	0xA00F,     /* R168 - Battery Charger Control 1 */
	0x0B06,     /* R169 - Battery Charger Control 2 */
	0x0000,     /* R170 - Battery Charger Control 3 */
	0x0000,     /* R171 */
	0x0000,     /* R172 - Current Sink Driver A */
	0x0000,     /* R173 - CSA Flash control */
	0x0000,     /* R174 - Current Sink Driver B */
	0x0000,     /* R175 - CSB Flash control */
	0x0000,     /* R176 - DCDC/LDO requested */
	0x002D,     /* R177 - DCDC Active options */
	0x0000,     /* R178 - DCDC Sleep options */
	0x0025,     /* R179 - Power-check comparator */
	0x000E,     /* R180 - DCDC1 Control */
	0x0400,     /* R181 - DCDC1 Timeouts */
	0x1006,     /* R182 - DCDC1 Low Power */
	0x0018,     /* R183 - DCDC2 Control */
	0x0000,     /* R184 - DCDC2 Timeouts */
	0x0000,     /* R185 */
	0x002E,     /* R186 - DCDC3 Control */
	0x0800,     /* R187 - DCDC3 Timeouts */
	0x0006,     /* R188 - DCDC3 Low Power */
	0x000E,     /* R189 - DCDC4 Control */
	0x0800,     /* R190 - DCDC4 Timeouts */
	0x0006,     /* R191 - DCDC4 Low Power */
	0x0008,     /* R192 - DCDC5 Control */
	0x0000,     /* R193 - DCDC5 Timeouts */
	0x0000,     /* R194 */
	0x0026,     /* R195 - DCDC6 Control */
	0x0C00,     /* R196 - DCDC6 Timeouts */
	0x0006,     /* R197 - DCDC6 Low Power */
	0x0000,     /* R198 */
	0x0003,     /* R199 - Limit Switch Control */
	0x001A,     /* R200 - LDO1 Control */
	0x0800,     /* R201 - LDO1 Timeouts */
	0x001C,     /* R202 - LDO1 Low Power */
	0x0010,     /* R203 - LDO2 Control */
	0x0800,     /* R204 - LDO2 Timeouts */
	0x001C,     /* R205 - LDO2 Low Power */
	0x000A,     /* R206 - LDO3 Control */
	0x0C00,     /* R207 - LDO3 Timeouts */
	0x001C,     /* R208 - LDO3 Low Power */
	0x001A,     /* R209 - LDO4 Control */
	0x0800,     /* R210 - LDO4 Timeouts */
	0x001C,     /* R211 - LDO4 Low Power */
	0x0000,     /* R212 */
	0x0000,     /* R213 */
	0x0000,     /* R214 */
	0x0000,     /* R215 - VCC_FAULT Masks */
	0x001F,     /* R216 - Main Bandgap Control */
	0x0000,     /* R217 - OSC Control */
	0x9000,     /* R218 - RTC Tick Control */
	0x0000,     /* R219 */
	0x4000,     /* R220 - RAM BIST 1 */
	0x0000,     /* R221 */
	0x0000,     /* R222 */
	0x0000,     /* R223 */
	0x0000,     /* R224 */
	0x0000,     /* R225 - DCDC/LDO status */
	0x0000,     /* R226 */
	0x0000,     /* R227 */
	0x0000,     /* R228 */
	0x0000,     /* R229 */
	0xE000,     /* R230 - GPIO Pin Status */
	0x0000,     /* R231 */
	0x0000,     /* R232 */
	0x0000,     /* R233 */
	0x0000,     /* R234 */
	0x0000,     /* R235 */
	0x0000,     /* R236 */
	0x0000,     /* R237 */
	0x0000,     /* R238 */
	0x0000,     /* R239 */
	0x0000,     /* R240 */
	0x0000,     /* R241 */
	0x0000,     /* R242 */
	0x0000,     /* R243 */
	0x0000,     /* R244 */
	0x0000,     /* R245 */
	0x0000,     /* R246 */
	0x0000,     /* R247 */
	0x0000,     /* R248 */
	0x0000,     /* R249 */
	0x0000,     /* R250 */
	0x0000,     /* R251 */
	0x0000,     /* R252 */
	0x0000,     /* R253 */
	0x0000,     /* R254 */
	0x0000,     /* R255 */
};
#endif

#ifdef CONFIG_MFD_WM8350_CONFIG_MODE_3

#undef WM8350_HAVE_CONFIG_MODE
#define WM8350_HAVE_CONFIG_MODE

const u16 wm8350_mode3_defaults[] = {
	0x17FF,     /* R0   - Reset/ID */
	0x1000,     /* R1   - ID */
	0x0000,     /* R2 */
	0x1000,     /* R3   - System Control 1 */
	0x0004,     /* R4   - System Control 2 */
	0x0000,     /* R5   - System Hibernate */
	0x8A00,     /* R6   - Interface Control */
	0x0000,     /* R7 */
	0x8000,     /* R8   - Power mgmt (1) */
	0x0000,     /* R9   - Power mgmt (2) */
	0x0000,     /* R10  - Power mgmt (3) */
	0x2000,     /* R11  - Power mgmt (4) */
	0x0E00,     /* R12  - Power mgmt (5) */
	0x0000,     /* R13  - Power mgmt (6) */
	0x0000,     /* R14  - Power mgmt (7) */
	0x0000,     /* R15 */
	0x0000,     /* R16  - RTC Seconds/Minutes */
	0x0100,     /* R17  - RTC Hours/Day */
	0x0101,     /* R18  - RTC Date/Month */
	0x1400,     /* R19  - RTC Year */
	0x0000,     /* R20  - Alarm Seconds/Minutes */
	0x0000,     /* R21  - Alarm Hours/Day */
	0x0000,     /* R22  - Alarm Date/Month */
	0x0320,     /* R23  - RTC Time Control */
	0x0000,     /* R24  - System Interrupts */
	0x0000,     /* R25  - Interrupt Status 1 */
	0x0000,     /* R26  - Interrupt Status 2 */
	0x0000,     /* R27  - Power Up Interrupt Status */
	0x0000,     /* R28  - Under Voltage Interrupt status */
	0x0000,     /* R29  - Over Current Interrupt status */
	0x0000,     /* R30  - GPIO Interrupt Status */
	0x0000,     /* R31  - Comparator Interrupt Status */
	0x3FFF,     /* R32  - System Interrupts Mask */
	0x0000,     /* R33  - Interrupt Status 1 Mask */
	0x0000,     /* R34  - Interrupt Status 2 Mask */
	0x0000,     /* R35  - Power Up Interrupt Status Mask */
	0x0000,     /* R36  - Under Voltage Interrupt status Mask */
	0x0000,     /* R37  - Over Current Interrupt status Mask */
	0x0000,     /* R38  - GPIO Interrupt Status Mask */
	0x0000,     /* R39  - Comparator Interrupt Status Mask */
	0x0040,     /* R40  - Clock Control 1 */
	0x0000,     /* R41  - Clock Control 2 */
	0x3B00,     /* R42  - FLL Control 1 */
	0x7086,     /* R43  - FLL Control 2 */
	0xC226,     /* R44  - FLL Control 3 */
	0x0000,     /* R45  - FLL Control 4 */
	0x0000,     /* R46 */
	0x0000,     /* R47 */
	0x0000,     /* R48  - DAC Control */
	0x0000,     /* R49 */
	0x00C0,     /* R50  - DAC Digital Volume L */
	0x00C0,     /* R51  - DAC Digital Volume R */
	0x0000,     /* R52 */
	0x0040,     /* R53  - DAC LR Rate */
	0x0000,     /* R54  - DAC Clock Control */
	0x0000,     /* R55 */
	0x0000,     /* R56 */
	0x0000,     /* R57 */
	0x4000,     /* R58  - DAC Mute */
	0x0000,     /* R59  - DAC Mute Volume */
	0x0000,     /* R60  - DAC Side */
	0x0000,     /* R61 */
	0x0000,     /* R62 */
	0x0000,     /* R63 */
	0x8000,     /* R64  - ADC Control */
	0x0000,     /* R65 */
	0x00C0,     /* R66  - ADC Digital Volume L */
	0x00C0,     /* R67  - ADC Digital Volume R */
	0x0000,     /* R68  - ADC Divider */
	0x0000,     /* R69 */
	0x0040,     /* R70  - ADC LR Rate */
	0x0000,     /* R71 */
	0x0303,     /* R72  - Input Control */
	0x0000,     /* R73  - IN3 Input Control */
	0x0000,     /* R74  - Mic Bias Control */
	0x0000,     /* R75 */
	0x0000,     /* R76  - Output Control */
	0x0000,     /* R77  - Jack Detect */
	0x0000,     /* R78  - Anti Pop Control */
	0x0000,     /* R79 */
	0x0040,     /* R80  - Left Input Volume */
	0x0040,     /* R81  - Right Input Volume */
	0x0000,     /* R82 */
	0x0000,     /* R83 */
	0x0000,     /* R84 */
	0x0000,     /* R85 */
	0x0000,     /* R86 */
	0x0000,     /* R87 */
	0x0800,     /* R88  - Left Mixer Control */
	0x1000,     /* R89  - Right Mixer Control */
	0x0000,     /* R90 */
	0x0000,     /* R91 */
	0x0000,     /* R92  - OUT3 Mixer Control */
	0x0000,     /* R93  - OUT4 Mixer Control */
	0x0000,     /* R94 */
	0x0000,     /* R95 */
	0x0000,     /* R96  - Output Left Mixer Volume */
	0x0000,     /* R97  - Output Right Mixer Volume */
	0x0000,     /* R98  - Input Mixer Volume L */
	0x0000,     /* R99  - Input Mixer Volume R */
	0x0000,     /* R100 - Input Mixer Volume */
	0x0000,     /* R101 */
	0x0000,     /* R102 */
	0x0000,     /* R103 */
	0x00E4,     /* R104 - LOUT1 Volume */
	0x00E4,     /* R105 - ROUT1 Volume */
	0x00E4,     /* R106 - LOUT2 Volume */
	0x02E4,     /* R107 - ROUT2 Volume */
	0x0000,     /* R108 */
	0x0000,     /* R109 */
	0x0000,     /* R110 */
	0x0000,     /* R111 - BEEP Volume */
	0x0A00,     /* R112 - AI Formating */
	0x0000,     /* R113 - ADC DAC COMP */
	0x0020,     /* R114 - AI ADC Control */
	0x0020,     /* R115 - AI DAC Control */
	0x0000,     /* R116 - AIF Test */
	0x0000,     /* R117 */
	0x0000,     /* R118 */
	0x0000,     /* R119 */
	0x0000,     /* R120 */
	0x0000,     /* R121 */
	0x0000,     /* R122 */
	0x0000,     /* R123 */
	0x0000,     /* R124 */
	0x0000,     /* R125 */
	0x0000,     /* R126 */
	0x0000,     /* R127 */
	0x1FFF,     /* R128 - GPIO Debounce */
	0x0000,     /* R129 - GPIO Pin pull up Control */
	0x03FC,     /* R130 - GPIO Pull down Control */
	0x0000,     /* R131 - GPIO Interrupt Mode */
	0x0000,     /* R132 */
	0x0000,     /* R133 - GPIO Control */
	0x0A7B,     /* R134 - GPIO Configuration (i/o) */
	0x06FE,     /* R135 - GPIO Pin Polarity / Type */
	0x0000,     /* R136 */
	0x0000,     /* R137 */
	0x0000,     /* R138 */
	0x0000,     /* R139 */
	0x1312,     /* R140 - GPIO Function Select 1 */
	0x1030,     /* R141 - GPIO Function Select 2 */
	0x2231,     /* R142 - GPIO Function Select 3 */
	0x0003,     /* R143 - GPIO Function Select 4 */
	0x0000,     /* R144 - Digitiser Control (1) */
	0x0002,     /* R145 - Digitiser Control (2) */
	0x0000,     /* R146 */
	0x0000,     /* R147 */
	0x0000,     /* R148 */
	0x0000,     /* R149 */
	0x0000,     /* R150 */
	0x0000,     /* R151 */
	0x7000,     /* R152 - AUX1 Readback */
	0x7000,     /* R153 - AUX2 Readback */
	0x7000,     /* R154 - AUX3 Readback */
	0x7000,     /* R155 - AUX4 Readback */
	0x0000,     /* R156 - USB Voltage Readback */
	0x0000,     /* R157 - LINE Voltage Readback */
	0x0000,     /* R158 - BATT Voltage Readback */
	0x0000,     /* R159 - Chip Temp Readback */
	0x0000,     /* R160 */
	0x0000,     /* R161 */
	0x0000,     /* R162 */
	0x0000,     /* R163 - Generic Comparator Control */
	0x0000,     /* R164 - Generic comparator 1 */
	0x0000,     /* R165 - Generic comparator 2 */
	0x0000,     /* R166 - Generic comparator 3 */
	0x0000,     /* R167 - Generic comparator 4 */
	0xA00F,     /* R168 - Battery Charger Control 1 */
	0x0B06,     /* R169 - Battery Charger Control 2 */
	0x0000,     /* R170 - Battery Charger Control 3 */
	0x0000,     /* R171 */
	0x0000,     /* R172 - Current Sink Driver A */
	0x0000,     /* R173 - CSA Flash control */
	0x0000,     /* R174 - Current Sink Driver B */
	0x0000,     /* R175 - CSB Flash control */
	0x0000,     /* R176 - DCDC/LDO requested */
	0x002D,     /* R177 - DCDC Active options */
	0x0000,     /* R178 - DCDC Sleep options */
	0x0025,     /* R179 - Power-check comparator */
	0x000E,     /* R180 - DCDC1 Control */
	0x0400,     /* R181 - DCDC1 Timeouts */
	0x1006,     /* R182 - DCDC1 Low Power */
	0x0018,     /* R183 - DCDC2 Control */
	0x0000,     /* R184 - DCDC2 Timeouts */
	0x0000,     /* R185 */
	0x000E,     /* R186 - DCDC3 Control */
	0x0400,     /* R187 - DCDC3 Timeouts */
	0x0006,     /* R188 - DCDC3 Low Power */
	0x0026,     /* R189 - DCDC4 Control */
	0x0400,     /* R190 - DCDC4 Timeouts */
	0x0006,     /* R191 - DCDC4 Low Power */
	0x0008,     /* R192 - DCDC5 Control */
	0x0000,     /* R193 - DCDC5 Timeouts */
	0x0000,     /* R194 */
	0x0026,     /* R195 - DCDC6 Control */
	0x0400,     /* R196 - DCDC6 Timeouts */
	0x0006,     /* R197 - DCDC6 Low Power */
	0x0000,     /* R198 */
	0x0003,     /* R199 - Limit Switch Control */
	0x001C,     /* R200 - LDO1 Control */
	0x0000,     /* R201 - LDO1 Timeouts */
	0x001C,     /* R202 - LDO1 Low Power */
	0x001C,     /* R203 - LDO2 Control */
	0x0400,     /* R204 - LDO2 Timeouts */
	0x001C,     /* R205 - LDO2 Low Power */
	0x001C,     /* R206 - LDO3 Control */
	0x0400,     /* R207 - LDO3 Timeouts */
	0x001C,     /* R208 - LDO3 Low Power */
	0x001F,     /* R209 - LDO4 Control */
	0x0400,     /* R210 - LDO4 Timeouts */
	0x001C,     /* R211 - LDO4 Low Power */
	0x0000,     /* R212 */
	0x0000,     /* R213 */
	0x0000,     /* R214 */
	0x0000,     /* R215 - VCC_FAULT Masks */
	0x001F,     /* R216 - Main Bandgap Control */
	0x0000,     /* R217 - OSC Control */
	0x9000,     /* R218 - RTC Tick Control */
	0x0000,     /* R219 */
	0x4000,     /* R220 - RAM BIST 1 */
	0x0000,     /* R221 */
	0x0000,     /* R222 */
	0x0000,     /* R223 */
	0x0000,     /* R224 */
	0x0000,     /* R225 - DCDC/LDO status */
	0x0000,     /* R226 */
	0x0000,     /* R227 */
	0x0000,     /* R228 */
	0x0000,     /* R229 */
	0xE000,     /* R230 - GPIO Pin Status */
	0x0000,     /* R231 */
	0x0000,     /* R232 */
	0x0000,     /* R233 */
	0x0000,     /* R234 */
	0x0000,     /* R235 */
	0x0000,     /* R236 */
	0x0000,     /* R237 */
	0x0000,     /* R238 */
	0x0000,     /* R239 */
	0x0000,     /* R240 */
	0x0000,     /* R241 */
	0x0000,     /* R242 */
	0x0000,     /* R243 */
	0x0000,     /* R244 */
	0x0000,     /* R245 */
	0x0000,     /* R246 */
	0x0000,     /* R247 */
	0x0000,     /* R248 */
	0x0000,     /* R249 */
	0x0000,     /* R250 */
	0x0000,     /* R251 */
	0x0000,     /* R252 */
	0x0000,     /* R253 */
	0x0000,     /* R254 */
	0x0000,     /* R255 */
};
#endif

#ifdef CONFIG_MFD_WM8351_CONFIG_MODE_0

#undef WM8350_HAVE_CONFIG_MODE
#define WM8350_HAVE_CONFIG_MODE

const u16 wm8351_mode0_defaults[] = {
	0x6143,     /* R0   - Reset/ID */
	0x0000,     /* R1   - ID */
	0x0001,     /* R2   - Revision */
	0x1C02,     /* R3   - System Control 1 */
	0x0004,     /* R4   - System Control 2 */
	0x0000,     /* R5   - System Hibernate */
	0x8A00,     /* R6   - Interface Control */
	0x0000,     /* R7 */
	0x8000,     /* R8   - Power mgmt (1) */
	0x0000,     /* R9   - Power mgmt (2) */
	0x0000,     /* R10  - Power mgmt (3) */
	0x2000,     /* R11  - Power mgmt (4) */
	0x0E00,     /* R12  - Power mgmt (5) */
	0x0000,     /* R13  - Power mgmt (6) */
	0x0000,     /* R14  - Power mgmt (7) */
	0x0000,     /* R15 */
	0x0000,     /* R16  - RTC Seconds/Minutes */
	0x0100,     /* R17  - RTC Hours/Day */
	0x0101,     /* R18  - RTC Date/Month */
	0x1400,     /* R19  - RTC Year */
	0x0000,     /* R20  - Alarm Seconds/Minutes */
	0x0000,     /* R21  - Alarm Hours/Day */
	0x0000,     /* R22  - Alarm Date/Month */
	0x0320,     /* R23  - RTC Time Control */
	0x0000,     /* R24  - System Interrupts */
	0x0000,     /* R25  - Interrupt Status 1 */
	0x0000,     /* R26  - Interrupt Status 2 */
	0x0000,     /* R27 */
	0x0000,     /* R28  - Under Voltage Interrupt status */
	0x0000,     /* R29  - Over Current Interrupt status */
	0x0000,     /* R30  - GPIO Interrupt Status */
	0x0000,     /* R31  - Comparator Interrupt Status */
	0x3FFF,     /* R32  - System Interrupts Mask */
	0x0000,     /* R33  - Interrupt Status 1 Mask */
	0x0000,     /* R34  - Interrupt Status 2 Mask */
	0x0000,     /* R35 */
	0x0000,     /* R36  - Under Voltage Interrupt status Mask */
	0x0000,     /* R37  - Over Current Interrupt status Mask */
	0x0000,     /* R38  - GPIO Interrupt Status Mask */
	0x0000,     /* R39  - Comparator Interrupt Status Mask */
	0x0040,     /* R40  - Clock Control 1 */
	0x0000,     /* R41  - Clock Control 2 */
	0x3A00,     /* R42  - FLL Control 1 */
	0x7086,     /* R43  - FLL Control 2 */
	0xC226,     /* R44  - FLL Control 3 */
	0x0000,     /* R45  - FLL Control 4 */
	0x0000,     /* R46 */
	0x0000,     /* R47 */
	0x0000,     /* R48  - DAC Control */
	0x0000,     /* R49 */
	0x00C0,     /* R50  - DAC Digital Volume L */
	0x00C0,     /* R51  - DAC Digital Volume R */
	0x0000,     /* R52 */
	0x0040,     /* R53  - DAC LR Rate */
	0x0000,     /* R54  - DAC Clock Control */
	0x0000,     /* R55 */
	0x0000,     /* R56 */
	0x0000,     /* R57 */
	0x4000,     /* R58  - DAC Mute */
	0x0000,     /* R59  - DAC Mute Volume */
	0x0000,     /* R60  - DAC Side */
	0x0000,     /* R61 */
	0x0000,     /* R62 */
	0x0000,     /* R63 */
	0x8000,     /* R64  - ADC Control */
	0x0000,     /* R65 */
	0x00C0,     /* R66  - ADC Digital Volume L */
	0x00C0,     /* R67  - ADC Digital Volume R */
	0x0000,     /* R68  - ADC Divider */
	0x0000,     /* R69 */
	0x0040,     /* R70  - ADC LR Rate */
	0x0000,     /* R71 */
	0x0303,     /* R72  - Input Control */
	0x0000,     /* R73  - IN3 Input Control */
	0x0000,     /* R74  - Mic Bias Control */
	0x0000,     /* R75 */
	0x0000,     /* R76  - Output Control */
	0x0000,     /* R77  - Jack Detect */
	0x0000,     /* R78  - Anti Pop Control */
	0x0000,     /* R79 */
	0x0040,     /* R80  - Left Input Volume */
	0x0040,     /* R81  - Right Input Volume */
	0x0000,     /* R82 */
	0x0000,     /* R83 */
	0x0000,     /* R84 */
	0x0000,     /* R85 */
	0x0000,     /* R86 */
	0x0000,     /* R87 */
	0x0800,     /* R88  - Left Mixer Control */
	0x1000,     /* R89  - Right Mixer Control */
	0x0000,     /* R90 */
	0x0000,     /* R91 */
	0x0000,     /* R92  - OUT3 Mixer Control */
	0x0000,     /* R93  - OUT4 Mixer Control */
	0x0000,     /* R94 */
	0x0000,     /* R95 */
	0x0000,     /* R96  - Output Left Mixer Volume */
	0x0000,     /* R97  - Output Right Mixer Volume */
	0x0000,     /* R98  - Input Mixer Volume L */
	0x0000,     /* R99  - Input Mixer Volume R */
	0x0000,     /* R100 - Input Mixer Volume */
	0x0000,     /* R101 */
	0x0000,     /* R102 */
	0x0000,     /* R103 */
	0x00E4,     /* R104 - OUT1L Volume */
	0x00E4,     /* R105 - OUT1R Volume */
	0x00E4,     /* R106 - OUT2L Volume */
	0x02E4,     /* R107 - OUT2R Volume */
	0x0000,     /* R108 */
	0x0000,     /* R109 */
	0x0000,     /* R110 */
	0x0000,     /* R111 - BEEP Volume */
	0x0A00,     /* R112 - AI Formating */
	0x0000,     /* R113 - ADC DAC COMP */
	0x0020,     /* R114 - AI ADC Control */
	0x0020,     /* R115 - AI DAC Control */
	0x0000,     /* R116 */
	0x0000,     /* R117 */
	0x0000,     /* R118 */
	0x0000,     /* R119 */
	0x0000,     /* R120 */
	0x0000,     /* R121 */
	0x0000,     /* R122 */
	0x0000,     /* R123 */
	0x0000,     /* R124 */
	0x0000,     /* R125 */
	0x0000,     /* R126 */
	0x0000,     /* R127 */
	0x1FFF,     /* R128 - GPIO Debounce */
	0x0000,     /* R129 - GPIO Pin pull up Control */
	0x0000,     /* R130 - GPIO Pull down Control */
	0x0000,     /* R131 - GPIO Interrupt Mode */
	0x0000,     /* R132 */
	0x0000,     /* R133 - GPIO Control */
	0x0FFC,     /* R134 - GPIO Configuration (i/o) */
	0x0FFC,     /* R135 - GPIO Pin Polarity / Type */
	0x0000,     /* R136 */
	0x0000,     /* R137 */
	0x0000,     /* R138 */
	0x0000,     /* R139 */
	0x0013,     /* R140 - GPIO Function Select 1 */
	0x0000,     /* R141 - GPIO Function Select 2 */
	0x0000,     /* R142 - GPIO Function Select 3 */
	0x0003,     /* R143 - GPIO Function Select 4 */
	0x0000,     /* R144 - Digitiser Control (1) */
	0x0002,     /* R145 - Digitiser Control (2) */
	0x0000,     /* R146 */
	0x0000,     /* R147 */
	0x0000,     /* R148 */
	0x0000,     /* R149 */
	0x0000,     /* R150 */
	0x0000,     /* R151 */
	0x7000,     /* R152 - AUX1 Readback */
	0x7000,     /* R153 - AUX2 Readback */
	0x7000,     /* R154 - AUX3 Readback */
	0x7000,     /* R155 - AUX4 Readback */
	0x0000,     /* R156 - USB Voltage Readback */
	0x0000,     /* R157 - LINE Voltage Readback */
	0x0000,     /* R158 - BATT Voltage Readback */
	0x0000,     /* R159 - Chip Temp Readback */
	0x0000,     /* R160 */
	0x0000,     /* R161 */
	0x0000,     /* R162 */
	0x0000,     /* R163 - Generic Comparator Control */
	0x0000,     /* R164 - Generic comparator 1 */
	0x0000,     /* R165 - Generic comparator 2 */
	0x0000,     /* R166 - Generic comparator 3 */
	0x0000,     /* R167 - Generic comparator 4 */
	0xA00F,     /* R168 - Battery Charger Control 1 */
	0x0B06,     /* R169 - Battery Charger Control 2 */
	0x0000,     /* R170 - Battery Charger Control 3 */
	0x0000,     /* R171 */
	0x0000,     /* R172 - Current Sink Driver A */
	0x0000,     /* R173 - CSA Flash control */
	0x0000,     /* R174 */
	0x0000,     /* R175 */
	0x0000,     /* R176 - DCDC/LDO requested */
	0x032D,     /* R177 - DCDC Active options */
	0x0000,     /* R178 - DCDC Sleep options */
	0x0025,     /* R179 - Power-check comparator */
	0x000E,     /* R180 - DCDC1 Control */
	0x0000,     /* R181 - DCDC1 Timeouts */
	0x1006,     /* R182 - DCDC1 Low Power */
	0x0018,     /* R183 - DCDC2 Control */
	0x0000,     /* R184 - DCDC2 Timeouts */
	0x0000,     /* R185 */
	0x0000,     /* R186 - DCDC3 Control */
	0x0000,     /* R187 - DCDC3 Timeouts */
	0x0006,     /* R188 - DCDC3 Low Power */
	0x0000,     /* R189 - DCDC4 Control */
	0x0000,     /* R190 - DCDC4 Timeouts */
	0x0006,     /* R191 - DCDC4 Low Power */
	0x0008,     /* R192 */
	0x0000,     /* R193 */
	0x0000,     /* R194 */
	0x0000,     /* R195 */
	0x0000,     /* R196 */
	0x0006,     /* R197 */
	0x0000,     /* R198 */
	0x0003,     /* R199 - Limit Switch Control */
	0x001C,     /* R200 - LDO1 Control */
	0x0000,     /* R201 - LDO1 Timeouts */
	0x001C,     /* R202 - LDO1 Low Power */
	0x001B,     /* R203 - LDO2 Control */
	0x0000,     /* R204 - LDO2 Timeouts */
	0x001C,     /* R205 - LDO2 Low Power */
	0x001B,     /* R206 - LDO3 Control */
	0x0000,     /* R207 - LDO3 Timeouts */
	0x001C,     /* R208 - LDO3 Low Power */
	0x001B,     /* R209 - LDO4 Control */
	0x0000,     /* R210 - LDO4 Timeouts */
	0x001C,     /* R211 - LDO4 Low Power */
	0x0000,     /* R212 */
	0x0000,     /* R213 */
	0x0000,     /* R214 */
	0x0000,     /* R215 - VCC_FAULT Masks */
	0x001F,     /* R216 - Main Bandgap Control */
	0x0000,     /* R217 - OSC Control */
	0x9000,     /* R218 - RTC Tick Control */
	0x0000,     /* R219 - Security1 */
	0x4000,     /* R220 */
	0x0000,     /* R221 */
	0x0000,     /* R222 */
	0x0000,     /* R223 */
	0x0000,     /* R224 - Signal overrides */
	0x0000,     /* R225 - DCDC/LDO status */
	0x0000,     /* R226 - Charger Overides/status */
	0x0000,     /* R227 - misc overrides */
	0x0000,     /* R228 - Supply overrides/status 1 */
	0x0000,     /* R229 - Supply overrides/status 2 */
	0xE000,     /* R230 - GPIO Pin Status */
	0x0000,     /* R231 - comparotor overrides */
	0x0000,     /* R232 */
	0x0000,     /* R233 - State Machine status */
	0x1200,     /* R234 - FLL Test 1 */
	0x0000,     /* R235 */
	0x8000,     /* R236 */
	0x0000,     /* R237 */
	0x0000,     /* R238 */
	0x0000,     /* R239 */
	0x0003,     /* R240 */
	0x0000,     /* R241 */
	0x0000,     /* R242 */
	0x0004,     /* R243 */
	0x0300,     /* R244 */
	0x0000,     /* R245 */
	0x0200,     /* R246 */
	0x0000,     /* R247 */
	0x1000,     /* R248 - DCDC1 Test Controls */
	0x1000,     /* R249 */
	0x1000,     /* R250 - DCDC3 Test Controls */
	0x1000,     /* R251 - DCDC4 Test Controls */
};
#endif

#ifdef CONFIG_MFD_WM8351_CONFIG_MODE_1

#undef WM8350_HAVE_CONFIG_MODE
#define WM8350_HAVE_CONFIG_MODE

const u16 wm8351_mode1_defaults[] = {
	0x6143,     /* R0   - Reset/ID */
	0x0000,     /* R1   - ID */
	0x0001,     /* R2   - Revision */
	0x1C02,     /* R3   - System Control 1 */
	0x0204,     /* R4   - System Control 2 */
	0x0000,     /* R5   - System Hibernate */
	0x8A00,     /* R6   - Interface Control */
	0x0000,     /* R7 */
	0x8000,     /* R8   - Power mgmt (1) */
	0x0000,     /* R9   - Power mgmt (2) */
	0x0000,     /* R10  - Power mgmt (3) */
	0x2000,     /* R11  - Power mgmt (4) */
	0x0E00,     /* R12  - Power mgmt (5) */
	0x0000,     /* R13  - Power mgmt (6) */
	0x0000,     /* R14  - Power mgmt (7) */
	0x0000,     /* R15 */
	0x0000,     /* R16  - RTC Seconds/Minutes */
	0x0100,     /* R17  - RTC Hours/Day */
	0x0101,     /* R18  - RTC Date/Month */
	0x1400,     /* R19  - RTC Year */
	0x0000,     /* R20  - Alarm Seconds/Minutes */
	0x0000,     /* R21  - Alarm Hours/Day */
	0x0000,     /* R22  - Alarm Date/Month */
	0x0320,     /* R23  - RTC Time Control */
	0x0000,     /* R24  - System Interrupts */
	0x0000,     /* R25  - Interrupt Status 1 */
	0x0000,     /* R26  - Interrupt Status 2 */
	0x0000,     /* R27 */
	0x0000,     /* R28  - Under Voltage Interrupt status */
	0x0000,     /* R29  - Over Current Interrupt status */
	0x0000,     /* R30  - GPIO Interrupt Status */
	0x0000,     /* R31  - Comparator Interrupt Status */
	0x3FFF,     /* R32  - System Interrupts Mask */
	0x0000,     /* R33  - Interrupt Status 1 Mask */
	0x0000,     /* R34  - Interrupt Status 2 Mask */
	0x0000,     /* R35 */
	0x0000,     /* R36  - Under Voltage Interrupt status Mask */
	0x0000,     /* R37  - Over Current Interrupt status Mask */
	0x0000,     /* R38  - GPIO Interrupt Status Mask */
	0x0000,     /* R39  - Comparator Interrupt Status Mask */
	0x0040,     /* R40  - Clock Control 1 */
	0x0000,     /* R41  - Clock Control 2 */
	0x3A00,     /* R42  - FLL Control 1 */
	0x7086,     /* R43  - FLL Control 2 */
	0xC226,     /* R44  - FLL Control 3 */
	0x0000,     /* R45  - FLL Control 4 */
	0x0000,     /* R46 */
	0x0000,     /* R47 */
	0x0000,     /* R48  - DAC Control */
	0x0000,     /* R49 */
	0x00C0,     /* R50  - DAC Digital Volume L */
	0x00C0,     /* R51  - DAC Digital Volume R */
	0x0000,     /* R52 */
	0x0040,     /* R53  - DAC LR Rate */
	0x0000,     /* R54  - DAC Clock Control */
	0x0000,     /* R55 */
	0x0000,     /* R56 */
	0x0000,     /* R57 */
	0x4000,     /* R58  - DAC Mute */
	0x0000,     /* R59  - DAC Mute Volume */
	0x0000,     /* R60  - DAC Side */
	0x0000,     /* R61 */
	0x0000,     /* R62 */
	0x0000,     /* R63 */
	0x8000,     /* R64  - ADC Control */
	0x0000,     /* R65 */
	0x00C0,     /* R66  - ADC Digital Volume L */
	0x00C0,     /* R67  - ADC Digital Volume R */
	0x0000,     /* R68  - ADC Divider */
	0x0000,     /* R69 */
	0x0040,     /* R70  - ADC LR Rate */
	0x0000,     /* R71 */
	0x0303,     /* R72  - Input Control */
	0x0000,     /* R73  - IN3 Input Control */
	0x0000,     /* R74  - Mic Bias Control */
	0x0000,     /* R75 */
	0x0000,     /* R76  - Output Control */
	0x0000,     /* R77  - Jack Detect */
	0x0000,     /* R78  - Anti Pop Control */
	0x0000,     /* R79 */
	0x0040,     /* R80  - Left Input Volume */
	0x0040,     /* R81  - Right Input Volume */
	0x0000,     /* R82 */
	0x0000,     /* R83 */
	0x0000,     /* R84 */
	0x0000,     /* R85 */
	0x0000,     /* R86 */
	0x0000,     /* R87 */
	0x0800,     /* R88  - Left Mixer Control */
	0x1000,     /* R89  - Right Mixer Control */
	0x0000,     /* R90 */
	0x0000,     /* R91 */
	0x0000,     /* R92  - OUT3 Mixer Control */
	0x0000,     /* R93  - OUT4 Mixer Control */
	0x0000,     /* R94 */
	0x0000,     /* R95 */
	0x0000,     /* R96  - Output Left Mixer Volume */
	0x0000,     /* R97  - Output Right Mixer Volume */
	0x0000,     /* R98  - Input Mixer Volume L */
	0x0000,     /* R99  - Input Mixer Volume R */
	0x0000,     /* R100 - Input Mixer Volume */
	0x0000,     /* R101 */
	0x0000,     /* R102 */
	0x0000,     /* R103 */
	0x00E4,     /* R104 - OUT1L Volume */
	0x00E4,     /* R105 - OUT1R Volume */
	0x00E4,     /* R106 - OUT2L Volume */
	0x02E4,     /* R107 - OUT2R Volume */
	0x0000,     /* R108 */
	0x0000,     /* R109 */
	0x0000,     /* R110 */
	0x0000,     /* R111 - BEEP Volume */
	0x0A00,     /* R112 - AI Formating */
	0x0000,     /* R113 - ADC DAC COMP */
	0x0020,     /* R114 - AI ADC Control */
	0x0020,     /* R115 - AI DAC Control */
	0x0000,     /* R116 */
	0x0000,     /* R117 */
	0x0000,     /* R118 */
	0x0000,     /* R119 */
	0x0000,     /* R120 */
	0x0000,     /* R121 */
	0x0000,     /* R122 */
	0x0000,     /* R123 */
	0x0000,     /* R124 */
	0x0000,     /* R125 */
	0x0000,     /* R126 */
	0x0000,     /* R127 */
	0x1FFF,     /* R128 - GPIO Debounce */
	0x0000,     /* R129 - GPIO Pin pull up Control */
	0x0000,     /* R130 - GPIO Pull down Control */
	0x0000,     /* R131 - GPIO Interrupt Mode */
	0x0000,     /* R132 */
	0x0000,     /* R133 - GPIO Control */
	0x0CFB,     /* R134 - GPIO Configuration (i/o) */
	0x0C1F,     /* R135 - GPIO Pin Polarity / Type */
	0x0000,     /* R136 */
	0x0000,     /* R137 */
	0x0000,     /* R138 */
	0x0000,     /* R139 */
	0x0300,     /* R140 - GPIO Function Select 1 */
	0x1110,     /* R141 - GPIO Function Select 2 */
	0x0013,     /* R142 - GPIO Function Select 3 */
	0x0003,     /* R143 - GPIO Function Select 4 */
	0x0000,     /* R144 - Digitiser Control (1) */
	0x0002,     /* R145 - Digitiser Control (2) */
	0x0000,     /* R146 */
	0x0000,     /* R147 */
	0x0000,     /* R148 */
	0x0000,     /* R149 */
	0x0000,     /* R150 */
	0x0000,     /* R151 */
	0x7000,     /* R152 - AUX1 Readback */
	0x7000,     /* R153 - AUX2 Readback */
	0x7000,     /* R154 - AUX3 Readback */
	0x7000,     /* R155 - AUX4 Readback */
	0x0000,     /* R156 - USB Voltage Readback */
	0x0000,     /* R157 - LINE Voltage Readback */
	0x0000,     /* R158 - BATT Voltage Readback */
	0x0000,     /* R159 - Chip Temp Readback */
	0x0000,     /* R160 */
	0x0000,     /* R161 */
	0x0000,     /* R162 */
	0x0000,     /* R163 - Generic Comparator Control */
	0x0000,     /* R164 - Generic comparator 1 */
	0x0000,     /* R165 - Generic comparator 2 */
	0x0000,     /* R166 - Generic comparator 3 */
	0x0000,     /* R167 - Generic comparator 4 */
	0xA00F,     /* R168 - Battery Charger Control 1 */
	0x0B06,     /* R169 - Battery Charger Control 2 */
	0x0000,     /* R170 - Battery Charger Control 3 */
	0x0000,     /* R171 */
	0x0000,     /* R172 - Current Sink Driver A */
	0x0000,     /* R173 - CSA Flash control */
	0x0000,     /* R174 */
	0x0000,     /* R175 */
	0x0000,     /* R176 - DCDC/LDO requested */
	0x032D,     /* R177 - DCDC Active options */
	0x0000,     /* R178 - DCDC Sleep options */
	0x0025,     /* R179 - Power-check comparator */
	0x000E,     /* R180 - DCDC1 Control */
	0x0C00,     /* R181 - DCDC1 Timeouts */
	0x1006,     /* R182 - DCDC1 Low Power */
	0x0018,     /* R183 - DCDC2 Control */
	0x0000,     /* R184 - DCDC2 Timeouts */
	0x0000,     /* R185 */
	0x0026,     /* R186 - DCDC3 Control */
	0x0400,     /* R187 - DCDC3 Timeouts */
	0x0006,     /* R188 - DCDC3 Low Power */
	0x0062,     /* R189 - DCDC4 Control */
	0x0800,     /* R190 - DCDC4 Timeouts */
	0x0006,     /* R191 - DCDC4 Low Power */
	0x0008,     /* R192 */
	0x0000,     /* R193 */
	0x0000,     /* R194 */
	0x000A,     /* R195 */
	0x1000,     /* R196 */
	0x0006,     /* R197 */
	0x0000,     /* R198 */
	0x0003,     /* R199 - Limit Switch Control */
	0x0006,     /* R200 - LDO1 Control */
	0x0000,     /* R201 - LDO1 Timeouts */
	0x001C,     /* R202 - LDO1 Low Power */
	0x0010,     /* R203 - LDO2 Control */
	0x0C00,     /* R204 - LDO2 Timeouts */
	0x001C,     /* R205 - LDO2 Low Power */
	0x001F,     /* R206 - LDO3 Control */
	0x0800,     /* R207 - LDO3 Timeouts */
	0x001C,     /* R208 - LDO3 Low Power */
	0x000A,     /* R209 - LDO4 Control */
	0x0800,     /* R210 - LDO4 Timeouts */
	0x001C,     /* R211 - LDO4 Low Power */
	0x0000,     /* R212 */
	0x0000,     /* R213 */
	0x0000,     /* R214 */
	0x0000,     /* R215 - VCC_FAULT Masks */
	0x001F,     /* R216 - Main Bandgap Control */
	0x0000,     /* R217 - OSC Control */
	0x9000,     /* R218 - RTC Tick Control */
	0x0000,     /* R219 - Security1 */
	0x4000,     /* R220 */
	0x0000,     /* R221 */
	0x0000,     /* R222 */
	0x0000,     /* R223 */
	0x0000,     /* R224 - Signal overrides */
	0x0000,     /* R225 - DCDC/LDO status */
	0x0000,     /* R226 - Charger Overides/status */
	0x0000,     /* R227 - misc overrides */
	0x0000,     /* R228 - Supply overrides/status 1 */
	0x0000,     /* R229 - Supply overrides/status 2 */
	0xE000,     /* R230 - GPIO Pin Status */
	0x0000,     /* R231 - comparotor overrides */
	0x0000,     /* R232 */
	0x0000,     /* R233 - State Machine status */
	0x1200,     /* R234 - FLL Test 1 */
	0x0000,     /* R235 */
	0x8000,     /* R236 */
	0x0000,     /* R237 */
	0x0000,     /* R238 */
	0x0000,     /* R239 */
	0x0003,     /* R240 */
	0x0000,     /* R241 */
	0x0000,     /* R242 */
	0x0004,     /* R243 */
	0x0300,     /* R244 */
	0x0000,     /* R245 */
	0x0200,     /* R246 */
	0x1000,     /* R247 */
	0x1000,     /* R248 - DCDC1 Test Controls */
	0x1000,     /* R249 */
	0x1000,     /* R250 - DCDC3 Test Controls */
	0x1000,     /* R251 - DCDC4 Test Controls */
};
#endif

#ifdef CONFIG_MFD_WM8351_CONFIG_MODE_2

#undef WM8350_HAVE_CONFIG_MODE
#define WM8350_HAVE_CONFIG_MODE

const u16 wm8351_mode2_defaults[] = {
	0x6143,     /* R0   - Reset/ID */
	0x0000,     /* R1   - ID */
	0x0001,     /* R2   - Revision */
	0x1C02,     /* R3   - System Control 1 */
	0x0214,     /* R4   - System Control 2 */
	0x0000,     /* R5   - System Hibernate */
	0x8A00,     /* R6   - Interface Control */
	0x0000,     /* R7 */
	0x8000,     /* R8   - Power mgmt (1) */
	0x0000,     /* R9   - Power mgmt (2) */
	0x0000,     /* R10  - Power mgmt (3) */
	0x2000,     /* R11  - Power mgmt (4) */
	0x0E00,     /* R12  - Power mgmt (5) */
	0x0000,     /* R13  - Power mgmt (6) */
	0x0000,     /* R14  - Power mgmt (7) */
	0x0000,     /* R15 */
	0x0000,     /* R16  - RTC Seconds/Minutes */
	0x0100,     /* R17  - RTC Hours/Day */
	0x0101,     /* R18  - RTC Date/Month */
	0x1400,     /* R19  - RTC Year */
	0x0000,     /* R20  - Alarm Seconds/Minutes */
	0x0000,     /* R21  - Alarm Hours/Day */
	0x0000,     /* R22  - Alarm Date/Month */
	0x0320,     /* R23  - RTC Time Control */
	0x0000,     /* R24  - System Interrupts */
	0x0000,     /* R25  - Interrupt Status 1 */
	0x0000,     /* R26  - Interrupt Status 2 */
	0x0000,     /* R27 */
	0x0000,     /* R28  - Under Voltage Interrupt status */
	0x0000,     /* R29  - Over Current Interrupt status */
	0x0000,     /* R30  - GPIO Interrupt Status */
	0x0000,     /* R31  - Comparator Interrupt Status */
	0x3FFF,     /* R32  - System Interrupts Mask */
	0x0000,     /* R33  - Interrupt Status 1 Mask */
	0x0000,     /* R34  - Interrupt Status 2 Mask */
	0x0000,     /* R35 */
	0x0000,     /* R36  - Under Voltage Interrupt status Mask */
	0x0000,     /* R37  - Over Current Interrupt status Mask */
	0x0000,     /* R38  - GPIO Interrupt Status Mask */
	0x0000,     /* R39  - Comparator Interrupt Status Mask */
	0x0040,     /* R40  - Clock Control 1 */
	0x0000,     /* R41  - Clock Control 2 */
	0x3A00,     /* R42  - FLL Control 1 */
	0x7086,     /* R43  - FLL Control 2 */
	0xC226,     /* R44  - FLL Control 3 */
	0x0000,     /* R45  - FLL Control 4 */
	0x0000,     /* R46 */
	0x0000,     /* R47 */
	0x0000,     /* R48  - DAC Control */
	0x0000,     /* R49 */
	0x00C0,     /* R50  - DAC Digital Volume L */
	0x00C0,     /* R51  - DAC Digital Volume R */
	0x0000,     /* R52 */
	0x0040,     /* R53  - DAC LR Rate */
	0x0000,     /* R54  - DAC Clock Control */
	0x0000,     /* R55 */
	0x0000,     /* R56 */
	0x0000,     /* R57 */
	0x4000,     /* R58  - DAC Mute */
	0x0000,     /* R59  - DAC Mute Volume */
	0x0000,     /* R60  - DAC Side */
	0x0000,     /* R61 */
	0x0000,     /* R62 */
	0x0000,     /* R63 */
	0x8000,     /* R64  - ADC Control */
	0x0000,     /* R65 */
	0x00C0,     /* R66  - ADC Digital Volume L */
	0x00C0,     /* R67  - ADC Digital Volume R */
	0x0000,     /* R68  - ADC Divider */
	0x0000,     /* R69 */
	0x0040,     /* R70  - ADC LR Rate */
	0x0000,     /* R71 */
	0x0303,     /* R72  - Input Control */
	0x0000,     /* R73  - IN3 Input Control */
	0x0000,     /* R74  - Mic Bias Control */
	0x0000,     /* R75 */
	0x0000,     /* R76  - Output Control */
	0x0000,     /* R77  - Jack Detect */
	0x0000,     /* R78  - Anti Pop Control */
	0x0000,     /* R79 */
	0x0040,     /* R80  - Left Input Volume */
	0x0040,     /* R81  - Right Input Volume */
	0x0000,     /* R82 */
	0x0000,     /* R83 */
	0x0000,     /* R84 */
	0x0000,     /* R85 */
	0x0000,     /* R86 */
	0x0000,     /* R87 */
	0x0800,     /* R88  - Left Mixer Control */
	0x1000,     /* R89  - Right Mixer Control */
	0x0000,     /* R90 */
	0x0000,     /* R91 */
	0x0000,     /* R92  - OUT3 Mixer Control */
	0x0000,     /* R93  - OUT4 Mixer Control */
	0x0000,     /* R94 */
	0x0000,     /* R95 */
	0x0000,     /* R96  - Output Left Mixer Volume */
	0x0000,     /* R97  - Output Right Mixer Volume */
	0x0000,     /* R98  - Input Mixer Volume L */
	0x0000,     /* R99  - Input Mixer Volume R */
	0x0000,     /* R100 - Input Mixer Volume */
	0x0000,     /* R101 */
	0x0000,     /* R102 */
	0x0000,     /* R103 */
	0x00E4,     /* R104 - OUT1L Volume */
	0x00E4,     /* R105 - OUT1R Volume */
	0x00E4,     /* R106 - OUT2L Volume */
	0x02E4,     /* R107 - OUT2R Volume */
	0x0000,     /* R108 */
	0x0000,     /* R109 */
	0x0000,     /* R110 */
	0x0000,     /* R111 - BEEP Volume */
	0x0A00,     /* R112 - AI Formating */
	0x0000,     /* R113 - ADC DAC COMP */
	0x0020,     /* R114 - AI ADC Control */
	0x0020,     /* R115 - AI DAC Control */
	0x0000,     /* R116 */
	0x0000,     /* R117 */
	0x0000,     /* R118 */
	0x0000,     /* R119 */
	0x0000,     /* R120 */
	0x0000,     /* R121 */
	0x0000,     /* R122 */
	0x0000,     /* R123 */
	0x0000,     /* R124 */
	0x0000,     /* R125 */
	0x0000,     /* R126 */
	0x0000,     /* R127 */
	0x1FFF,     /* R128 - GPIO Debounce */
	0x0000,     /* R129 - GPIO Pin pull up Control */
	0x0110,     /* R130 - GPIO Pull down Control */
	0x0000,     /* R131 - GPIO Interrupt Mode */
	0x0000,     /* R132 */
	0x0000,     /* R133 - GPIO Control */
	0x09FA,     /* R134 - GPIO Configuration (i/o) */
	0x0DF6,     /* R135 - GPIO Pin Polarity / Type */
	0x0000,     /* R136 */
	0x0000,     /* R137 */
	0x0000,     /* R138 */
	0x0000,     /* R139 */
	0x1310,     /* R140 - GPIO Function Select 1 */
	0x0003,     /* R141 - GPIO Function Select 2 */
	0x2000,     /* R142 - GPIO Function Select 3 */
	0x0000,     /* R143 - GPIO Function Select 4 */
	0x0000,     /* R144 - Digitiser Control (1) */
	0x0002,     /* R145 - Digitiser Control (2) */
	0x0000,     /* R146 */
	0x0000,     /* R147 */
	0x0000,     /* R148 */
	0x0000,     /* R149 */
	0x0000,     /* R150 */
	0x0000,     /* R151 */
	0x7000,     /* R152 - AUX1 Readback */
	0x7000,     /* R153 - AUX2 Readback */
	0x7000,     /* R154 - AUX3 Readback */
	0x7000,     /* R155 - AUX4 Readback */
	0x0000,     /* R156 - USB Voltage Readback */
	0x0000,     /* R157 - LINE Voltage Readback */
	0x0000,     /* R158 - BATT Voltage Readback */
	0x0000,     /* R159 - Chip Temp Readback */
	0x0000,     /* R160 */
	0x0000,     /* R161 */
	0x0000,     /* R162 */
	0x0000,     /* R163 - Generic Comparator Control */
	0x0000,     /* R164 - Generic comparator 1 */
	0x0000,     /* R165 - Generic comparator 2 */
	0x0000,     /* R166 - Generic comparator 3 */
	0x0000,     /* R167 - Generic comparator 4 */
	0xA00F,     /* R168 - Battery Charger Control 1 */
	0x0B06,     /* R169 - Battery Charger Control 2 */
	0x0000,     /* R170 - Battery Charger Control 3 */
	0x0000,     /* R171 */
	0x0000,     /* R172 - Current Sink Driver A */
	0x0000,     /* R173 - CSA Flash control */
	0x0000,     /* R174 */
	0x0000,     /* R175 */
	0x0000,     /* R176 - DCDC/LDO requested */
	0x032D,     /* R177 - DCDC Active options */
	0x0000,     /* R178 - DCDC Sleep options */
	0x0025,     /* R179 - Power-check comparator */
	0x001A,     /* R180 - DCDC1 Control */
	0x0800,     /* R181 - DCDC1 Timeouts */
	0x1006,     /* R182 - DCDC1 Low Power */
	0x0018,     /* R183 - DCDC2 Control */
	0x0000,     /* R184 - DCDC2 Timeouts */
	0x0000,     /* R185 */
	0x0056,     /* R186 - DCDC3 Control */
	0x0400,     /* R187 - DCDC3 Timeouts */
	0x0006,     /* R188 - DCDC3 Low Power */
	0x0026,     /* R189 - DCDC4 Control */
	0x0C00,     /* R190 - DCDC4 Timeouts */
	0x0006,     /* R191 - DCDC4 Low Power */
	0x0008,     /* R192 */
	0x0000,     /* R193 */
	0x0000,     /* R194 */
	0x0026,     /* R195 */
	0x0C00,     /* R196 */
	0x0006,     /* R197 */
	0x0000,     /* R198 */
	0x0003,     /* R199 - Limit Switch Control */
	0x001C,     /* R200 - LDO1 Control */
	0x0400,     /* R201 - LDO1 Timeouts */
	0x001C,     /* R202 - LDO1 Low Power */
	0x0010,     /* R203 - LDO2 Control */
	0x0C00,     /* R204 - LDO2 Timeouts */
	0x001C,     /* R205 - LDO2 Low Power */
	0x0015,     /* R206 - LDO3 Control */
	0x0000,     /* R207 - LDO3 Timeouts */
	0x001C,     /* R208 - LDO3 Low Power */
	0x001A,     /* R209 - LDO4 Control */
	0x0000,     /* R210 - LDO4 Timeouts */
	0x001C,     /* R211 - LDO4 Low Power */
	0x0000,     /* R212 */
	0x0000,     /* R213 */
	0x0000,     /* R214 */
	0x0000,     /* R215 - VCC_FAULT Masks */
	0x001F,     /* R216 - Main Bandgap Control */
	0x0000,     /* R217 - OSC Control */
	0x9000,     /* R218 - RTC Tick Control */
	0x0000,     /* R219 - Security1 */
	0x4000,     /* R220 */
	0x0000,     /* R221 */
	0x0000,     /* R222 */
	0x0000,     /* R223 */
	0x0000,     /* R224 - Signal overrides */
	0x0000,     /* R225 - DCDC/LDO status */
	0x0000,     /* R226 - Charger Overides/status */
	0x0000,     /* R227 - misc overrides */
	0x0000,     /* R228 - Supply overrides/status 1 */
	0x0000,     /* R229 - Supply overrides/status 2 */
	0xE000,     /* R230 - GPIO Pin Status */
	0x0000,     /* R231 - comparotor overrides */
	0x0000,     /* R232 */
	0x0000,     /* R233 - State Machine status */
	0x1200,     /* R234 - FLL Test 1 */
	0x0000,     /* R235 */
	0x8000,     /* R236 */
	0x0000,     /* R237 */
	0x0000,     /* R238 */
	0x0000,     /* R239 */
	0x0003,     /* R240 */
	0x0000,     /* R241 */
	0x0000,     /* R242 */
	0x0004,     /* R243 */
	0x0300,     /* R244 */
	0x0000,     /* R245 */
	0x0200,     /* R246 */
	0x0000,     /* R247 */
	0x1000,     /* R248 - DCDC1 Test Controls */
	0x1000,     /* R249 */
	0x1000,     /* R250 - DCDC3 Test Controls */
	0x1000,     /* R251 - DCDC4 Test Controls */
};
#endif

#ifdef CONFIG_MFD_WM8351_CONFIG_MODE_3

#undef WM8350_HAVE_CONFIG_MODE
#define WM8350_HAVE_CONFIG_MODE

const u16 wm8351_mode3_defaults[] = {
	0x6143,     /* R0   - Reset/ID */
	0x0000,     /* R1   - ID */
	0x0001,     /* R2   - Revision */
	0x1C02,     /* R3   - System Control 1 */
	0x0204,     /* R4   - System Control 2 */
	0x0000,     /* R5   - System Hibernate */
	0x8A00,     /* R6   - Interface Control */
	0x0000,     /* R7 */
	0x8000,     /* R8   - Power mgmt (1) */
	0x0000,     /* R9   - Power mgmt (2) */
	0x0000,     /* R10  - Power mgmt (3) */
	0x2000,     /* R11  - Power mgmt (4) */
	0x0E00,     /* R12  - Power mgmt (5) */
	0x0000,     /* R13  - Power mgmt (6) */
	0x0000,     /* R14  - Power mgmt (7) */
	0x0000,     /* R15 */
	0x0000,     /* R16  - RTC Seconds/Minutes */
	0x0100,     /* R17  - RTC Hours/Day */
	0x0101,     /* R18  - RTC Date/Month */
	0x1400,     /* R19  - RTC Year */
	0x0000,     /* R20  - Alarm Seconds/Minutes */
	0x0000,     /* R21  - Alarm Hours/Day */
	0x0000,     /* R22  - Alarm Date/Month */
	0x0320,     /* R23  - RTC Time Control */
	0x0000,     /* R24  - System Interrupts */
	0x0000,     /* R25  - Interrupt Status 1 */
	0x0000,     /* R26  - Interrupt Status 2 */
	0x0000,     /* R27 */
	0x0000,     /* R28  - Under Voltage Interrupt status */
	0x0000,     /* R29  - Over Current Interrupt status */
	0x0000,     /* R30  - GPIO Interrupt Status */
	0x0000,     /* R31  - Comparator Interrupt Status */
	0x3FFF,     /* R32  - System Interrupts Mask */
	0x0000,     /* R33  - Interrupt Status 1 Mask */
	0x0000,     /* R34  - Interrupt Status 2 Mask */
	0x0000,     /* R35 */
	0x0000,     /* R36  - Under Voltage Interrupt status Mask */
	0x0000,     /* R37  - Over Current Interrupt status Mask */
	0x0000,     /* R38  - GPIO Interrupt Status Mask */
	0x0000,     /* R39  - Comparator Interrupt Status Mask */
	0x0040,     /* R40  - Clock Control 1 */
	0x0000,     /* R41  - Clock Control 2 */
	0x3A00,     /* R42  - FLL Control 1 */
	0x7086,     /* R43  - FLL Control 2 */
	0xC226,     /* R44  - FLL Control 3 */
	0x0000,     /* R45  - FLL Control 4 */
	0x0000,     /* R46 */
	0x0000,     /* R47 */
	0x0000,     /* R48  - DAC Control */
	0x0000,     /* R49 */
	0x00C0,     /* R50  - DAC Digital Volume L */
	0x00C0,     /* R51  - DAC Digital Volume R */
	0x0000,     /* R52 */
	0x0040,     /* R53  - DAC LR Rate */
	0x0000,     /* R54  - DAC Clock Control */
	0x0000,     /* R55 */
	0x0000,     /* R56 */
	0x0000,     /* R57 */
	0x4000,     /* R58  - DAC Mute */
	0x0000,     /* R59  - DAC Mute Volume */
	0x0000,     /* R60  - DAC Side */
	0x0000,     /* R61 */
	0x0000,     /* R62 */
	0x0000,     /* R63 */
	0x8000,     /* R64  - ADC Control */
	0x0000,     /* R65 */
	0x00C0,     /* R66  - ADC Digital Volume L */
	0x00C0,     /* R67  - ADC Digital Volume R */
	0x0000,     /* R68  - ADC Divider */
	0x0000,     /* R69 */
	0x0040,     /* R70  - ADC LR Rate */
	0x0000,     /* R71 */
	0x0303,     /* R72  - Input Control */
	0x0000,     /* R73  - IN3 Input Control */
	0x0000,     /* R74  - Mic Bias Control */
	0x0000,     /* R75 */
	0x0000,     /* R76  - Output Control */
	0x0000,     /* R77  - Jack Detect */
	0x0000,     /* R78  - Anti Pop Control */
	0x0000,     /* R79 */
	0x0040,     /* R80  - Left Input Volume */
	0x0040,     /* R81  - Right Input Volume */
	0x0000,     /* R82 */
	0x0000,     /* R83 */
	0x0000,     /* R84 */
	0x0000,     /* R85 */
	0x0000,     /* R86 */
	0x0000,     /* R87 */
	0x0800,     /* R88  - Left Mixer Control */
	0x1000,     /* R89  - Right Mixer Control */
	0x0000,     /* R90 */
	0x0000,     /* R91 */
	0x0000,     /* R92  - OUT3 Mixer Control */
	0x0000,     /* R93  - OUT4 Mixer Control */
	0x0000,     /* R94 */
	0x0000,     /* R95 */
	0x0000,     /* R96  - Output Left Mixer Volume */
	0x0000,     /* R97  - Output Right Mixer Volume */
	0x0000,     /* R98  - Input Mixer Volume L */
	0x0000,     /* R99  - Input Mixer Volume R */
	0x0000,     /* R100 - Input Mixer Volume */
	0x0000,     /* R101 */
	0x0000,     /* R102 */
	0x0000,     /* R103 */
	0x00E4,     /* R104 - OUT1L Volume */
	0x00E4,     /* R105 - OUT1R Volume */
	0x00E4,     /* R106 - OUT2L Volume */
	0x02E4,     /* R107 - OUT2R Volume */
	0x0000,     /* R108 */
	0x0000,     /* R109 */
	0x0000,     /* R110 */
	0x0000,     /* R111 - BEEP Volume */
	0x0A00,     /* R112 - AI Formating */
	0x0000,     /* R113 - ADC DAC COMP */
	0x0020,     /* R114 - AI ADC Control */
	0x0020,     /* R115 - AI DAC Control */
	0x0000,     /* R116 */
	0x0000,     /* R117 */
	0x0000,     /* R118 */
	0x0000,     /* R119 */
	0x0000,     /* R120 */
	0x0000,     /* R121 */
	0x0000,     /* R122 */
	0x0000,     /* R123 */
	0x0000,     /* R124 */
	0x0000,     /* R125 */
	0x0000,     /* R126 */
	0x0000,     /* R127 */
	0x1FFF,     /* R128 - GPIO Debounce */
	0x0010,     /* R129 - GPIO Pin pull up Control */
	0x0000,     /* R130 - GPIO Pull down Control */
	0x0000,     /* R131 - GPIO Interrupt Mode */
	0x0000,     /* R132 */
	0x0000,     /* R133 - GPIO Control */
	0x0BFB,     /* R134 - GPIO Configuration (i/o) */
	0x0FFD,     /* R135 - GPIO Pin Polarity / Type */
	0x0000,     /* R136 */
	0x0000,     /* R137 */
	0x0000,     /* R138 */
	0x0000,     /* R139 */
	0x0310,     /* R140 - GPIO Function Select 1 */
	0x0001,     /* R141 - GPIO Function Select 2 */
	0x2300,     /* R142 - GPIO Function Select 3 */
	0x0003,     /* R143 - GPIO Function Select 4 */
	0x0000,     /* R144 - Digitiser Control (1) */
	0x0002,     /* R145 - Digitiser Control (2) */
	0x0000,     /* R146 */
	0x0000,     /* R147 */
	0x0000,     /* R148 */
	0x0000,     /* R149 */
	0x0000,     /* R150 */
	0x0000,     /* R151 */
	0x7000,     /* R152 - AUX1 Readback */
	0x7000,     /* R153 - AUX2 Readback */
	0x7000,     /* R154 - AUX3 Readback */
	0x7000,     /* R155 - AUX4 Readback */
	0x0000,     /* R156 - USB Voltage Readback */
	0x0000,     /* R157 - LINE Voltage Readback */
	0x0000,     /* R158 - BATT Voltage Readback */
	0x0000,     /* R159 - Chip Temp Readback */
	0x0000,     /* R160 */
	0x0000,     /* R161 */
	0x0000,     /* R162 */
	0x0000,     /* R163 - Generic Comparator Control */
	0x0000,     /* R164 - Generic comparator 1 */
	0x0000,     /* R165 - Generic comparator 2 */
	0x0000,     /* R166 - Generic comparator 3 */
	0x0000,     /* R167 - Generic comparator 4 */
	0xA00F,     /* R168 - Battery Charger Control 1 */
	0x0B06,     /* R169 - Battery Charger Control 2 */
	0x0000,     /* R170 - Battery Charger Control 3 */
	0x0000,     /* R171 */
	0x0000,     /* R172 - Current Sink Driver A */
	0x0000,     /* R173 - CSA Flash control */
	0x0000,     /* R174 */
	0x0000,     /* R175 */
	0x0000,     /* R176 - DCDC/LDO requested */
	0x032D,     /* R177 - DCDC Active options */
	0x0000,     /* R178 - DCDC Sleep options */
	0x0025,     /* R179 - Power-check comparator */
	0x000E,     /* R180 - DCDC1 Control */
	0x0400,     /* R181 - DCDC1 Timeouts */
	0x1006,     /* R182 - DCDC1 Low Power */
	0x0018,     /* R183 - DCDC2 Control */
	0x0000,     /* R184 - DCDC2 Timeouts */
	0x0000,     /* R185 */
	0x0026,     /* R186 - DCDC3 Control */
	0x0800,     /* R187 - DCDC3 Timeouts */
	0x0006,     /* R188 - DCDC3 Low Power */
	0x0062,     /* R189 - DCDC4 Control */
	0x1400,     /* R190 - DCDC4 Timeouts */
	0x0006,     /* R191 - DCDC4 Low Power */
	0x0008,     /* R192 */
	0x0000,     /* R193 */
	0x0000,     /* R194 */
	0x0026,     /* R195 */
	0x0400,     /* R196 */
	0x0006,     /* R197 */
	0x0000,     /* R198 */
	0x0003,     /* R199 - Limit Switch Control */
	0x0006,     /* R200 - LDO1 Control */
	0x0C00,     /* R201 - LDO1 Timeouts */
	0x001C,     /* R202 - LDO1 Low Power */
	0x0016,     /* R203 - LDO2 Control */
	0x0000,     /* R204 - LDO2 Timeouts */
	0x001C,     /* R205 - LDO2 Low Power */
	0x0019,     /* R206 - LDO3 Control */
	0x0000,     /* R207 - LDO3 Timeouts */
	0x001C,     /* R208 - LDO3 Low Power */
	0x001A,     /* R209 - LDO4 Control */
	0x1000,     /* R210 - LDO4 Timeouts */
	0x001C,     /* R211 - LDO4 Low Power */
	0x0000,     /* R212 */
	0x0000,     /* R213 */
	0x0000,     /* R214 */
	0x0000,     /* R215 - VCC_FAULT Masks */
	0x001F,     /* R216 - Main Bandgap Control */
	0x0000,     /* R217 - OSC Control */
	0x9000,     /* R218 - RTC Tick Control */
	0x0000,     /* R219 - Security1 */
	0x4000,     /* R220 */
	0x0000,     /* R221 */
	0x0000,     /* R222 */
	0x0000,     /* R223 */
	0x0000,     /* R224 - Signal overrides */
	0x0000,     /* R225 - DCDC/LDO status */
	0x0000,     /* R226 - Charger Overides/status */
	0x0000,     /* R227 - misc overrides */
	0x0000,     /* R228 - Supply overrides/status 1 */
	0x0000,     /* R229 - Supply overrides/status 2 */
	0xE000,     /* R230 - GPIO Pin Status */
	0x0000,     /* R231 - comparotor overrides */
	0x0000,     /* R232 */
	0x0000,     /* R233 - State Machine status */
	0x1200,     /* R234 - FLL Test 1 */
	0x0000,     /* R235 */
	0x8000,     /* R236 */
	0x0000,     /* R237 */
	0x0000,     /* R238 */
	0x0000,     /* R239 */
	0x0003,     /* R240 */
	0x0000,     /* R241 */
	0x0000,     /* R242 */
	0x0004,     /* R243 */
	0x0300,     /* R244 */
	0x0000,     /* R245 */
	0x0200,     /* R246 */
	0x0000,     /* R247 */
	0x1000,     /* R248 - DCDC1 Test Controls */
	0x1000,     /* R249 */
	0x1000,     /* R250 - DCDC3 Test Controls */
	0x1000,     /* R251 - DCDC4 Test Controls */
};
#endif

#ifdef CONFIG_MFD_WM8352_CONFIG_MODE_0

#undef WM8350_HAVE_CONFIG_MODE
#define WM8350_HAVE_CONFIG_MODE

const u16 wm8352_mode0_defaults[] = {
	0x6143,     /* R0   - Reset/ID */
	0x0000,     /* R1   - ID */
	0x0002,     /* R2   - Revision */
	0x1C02,     /* R3   - System Control 1 */
	0x0004,     /* R4   - System Control 2 */
	0x0000,     /* R5   - System Hibernate */
	0x8A00,     /* R6   - Interface Control */
	0x0000,     /* R7 */
	0x8000,     /* R8   - Power mgmt (1) */
	0x0000,     /* R9   - Power mgmt (2) */
	0x0000,     /* R10  - Power mgmt (3) */
	0x2000,     /* R11  - Power mgmt (4) */
	0x0E00,     /* R12  - Power mgmt (5) */
	0x0000,     /* R13  - Power mgmt (6) */
	0x0000,     /* R14  - Power mgmt (7) */
	0x0000,     /* R15 */
	0x0000,     /* R16  - RTC Seconds/Minutes */
	0x0100,     /* R17  - RTC Hours/Day */
	0x0101,     /* R18  - RTC Date/Month */
	0x1400,     /* R19  - RTC Year */
	0x0000,     /* R20  - Alarm Seconds/Minutes */
	0x0000,     /* R21  - Alarm Hours/Day */
	0x0000,     /* R22  - Alarm Date/Month */
	0x0320,     /* R23  - RTC Time Control */
	0x0000,     /* R24  - System Interrupts */
	0x0000,     /* R25  - Interrupt Status 1 */
	0x0000,     /* R26  - Interrupt Status 2 */
	0x0000,     /* R27 */
	0x0000,     /* R28  - Under Voltage Interrupt status */
	0x0000,     /* R29  - Over Current Interrupt status */
	0x0000,     /* R30  - GPIO Interrupt Status */
	0x0000,     /* R31  - Comparator Interrupt Status */
	0x3FFF,     /* R32  - System Interrupts Mask */
	0x0000,     /* R33  - Interrupt Status 1 Mask */
	0x0000,     /* R34  - Interrupt Status 2 Mask */
	0x0000,     /* R35 */
	0x0000,     /* R36  - Under Voltage Interrupt status Mask */
	0x0000,     /* R37  - Over Current Interrupt status Mask */
	0x0000,     /* R38  - GPIO Interrupt Status Mask */
	0x0000,     /* R39  - Comparator Interrupt Status Mask */
	0x0040,     /* R40  - Clock Control 1 */
	0x0000,     /* R41  - Clock Control 2 */
	0x3A00,     /* R42  - FLL Control 1 */
	0x7086,     /* R43  - FLL Control 2 */
	0xC226,     /* R44  - FLL Control 3 */
	0x0000,     /* R45  - FLL Control 4 */
	0x0000,     /* R46 */
	0x0000,     /* R47 */
	0x0000,     /* R48  - DAC Control */
	0x0000,     /* R49 */
	0x00C0,     /* R50  - DAC Digital Volume L */
	0x00C0,     /* R51  - DAC Digital Volume R */
	0x0000,     /* R52 */
	0x0040,     /* R53  - DAC LR Rate */
	0x0000,     /* R54  - DAC Clock Control */
	0x0000,     /* R55 */
	0x0000,     /* R56 */
	0x0000,     /* R57 */
	0x4000,     /* R58  - DAC Mute */
	0x0000,     /* R59  - DAC Mute Volume */
	0x0000,     /* R60  - DAC Side */
	0x0000,     /* R61 */
	0x0000,     /* R62 */
	0x0000,     /* R63 */
	0x8000,     /* R64  - ADC Control */
	0x0000,     /* R65 */
	0x00C0,     /* R66  - ADC Digital Volume L */
	0x00C0,     /* R67  - ADC Digital Volume R */
	0x0000,     /* R68  - ADC Divider */
	0x0000,     /* R69 */
	0x0040,     /* R70  - ADC LR Rate */
	0x0000,     /* R71 */
	0x0303,     /* R72  - Input Control */
	0x0000,     /* R73  - IN3 Input Control */
	0x0000,     /* R74  - Mic Bias Control */
	0x0000,     /* R75 */
	0x0000,     /* R76  - Output Control */
	0x0000,     /* R77  - Jack Detect */
	0x0000,     /* R78  - Anti Pop Control */
	0x0000,     /* R79 */
	0x0040,     /* R80  - Left Input Volume */
	0x0040,     /* R81  - Right Input Volume */
	0x0000,     /* R82 */
	0x0000,     /* R83 */
	0x0000,     /* R84 */
	0x0000,     /* R85 */
	0x0000,     /* R86 */
	0x0000,     /* R87 */
	0x0800,     /* R88  - Left Mixer Control */
	0x1000,     /* R89  - Right Mixer Control */
	0x0000,     /* R90 */
	0x0000,     /* R91 */
	0x0000,     /* R92  - OUT3 Mixer Control */
	0x0000,     /* R93  - OUT4 Mixer Control */
	0x0000,     /* R94 */
	0x0000,     /* R95 */
	0x0000,     /* R96  - Output Left Mixer Volume */
	0x0000,     /* R97  - Output Right Mixer Volume */
	0x0000,     /* R98  - Input Mixer Volume L */
	0x0000,     /* R99  - Input Mixer Volume R */
	0x0000,     /* R100 - Input Mixer Volume */
	0x0000,     /* R101 */
	0x0000,     /* R102 */
	0x0000,     /* R103 */
	0x00E4,     /* R104 - OUT1L Volume */
	0x00E4,     /* R105 - OUT1R Volume */
	0x00E4,     /* R106 - OUT2L Volume */
	0x02E4,     /* R107 - OUT2R Volume */
	0x0000,     /* R108 */
	0x0000,     /* R109 */
	0x0000,     /* R110 */
	0x0000,     /* R111 - BEEP Volume */
	0x0A00,     /* R112 - AI Formating */
	0x0000,     /* R113 - ADC DAC COMP */
	0x0020,     /* R114 - AI ADC Control */
	0x0020,     /* R115 - AI DAC Control */
	0x0000,     /* R116 */
	0x0000,     /* R117 */
	0x0000,     /* R118 */
	0x0000,     /* R119 */
	0x0000,     /* R120 */
	0x0000,     /* R121 */
	0x0000,     /* R122 */
	0x0000,     /* R123 */
	0x0000,     /* R124 */
	0x0000,     /* R125 */
	0x0000,     /* R126 */
	0x0000,     /* R127 */
	0x1FFF,     /* R128 - GPIO Debounce */
	0x0000,     /* R129 - GPIO Pin pull up Control */
	0x0000,     /* R130 - GPIO Pull down Control */
	0x0000,     /* R131 - GPIO Interrupt Mode */
	0x0000,     /* R132 */
	0x0000,     /* R133 - GPIO Control */
	0x0FFC,     /* R134 - GPIO Configuration (i/o) */
	0x0FFC,     /* R135 - GPIO Pin Polarity / Type */
	0x0000,     /* R136 */
	0x0000,     /* R137 */
	0x0000,     /* R138 */
	0x0000,     /* R139 */
	0x0013,     /* R140 - GPIO Function Select 1 */
	0x0000,     /* R141 - GPIO Function Select 2 */
	0x0000,     /* R142 - GPIO Function Select 3 */
	0x0003,     /* R143 - GPIO Function Select 4 */
	0x0000,     /* R144 - Digitiser Control (1) */
	0x0002,     /* R145 - Digitiser Control (2) */
	0x0000,     /* R146 */
	0x0000,     /* R147 */
	0x0000,     /* R148 */
	0x0000,     /* R149 */
	0x0000,     /* R150 */
	0x0000,     /* R151 */
	0x7000,     /* R152 - AUX1 Readback */
	0x7000,     /* R153 - AUX2 Readback */
	0x7000,     /* R154 - AUX3 Readback */
	0x7000,     /* R155 - AUX4 Readback */
	0x0000,     /* R156 - USB Voltage Readback */
	0x0000,     /* R157 - LINE Voltage Readback */
	0x0000,     /* R158 - BATT Voltage Readback */
	0x0000,     /* R159 - Chip Temp Readback */
	0x0000,     /* R160 */
	0x0000,     /* R161 */
	0x0000,     /* R162 */
	0x0000,     /* R163 - Generic Comparator Control */
	0x0000,     /* R164 - Generic comparator 1 */
	0x0000,     /* R165 - Generic comparator 2 */
	0x0000,     /* R166 - Generic comparator 3 */
	0x0000,     /* R167 - Generic comparator 4 */
	0xA00F,     /* R168 - Battery Charger Control 1 */
	0x0B06,     /* R169 - Battery Charger Control 2 */
	0x0000,     /* R170 - Battery Charger Control 3 */
	0x0000,     /* R171 */
	0x0000,     /* R172 - Current Sink Driver A */
	0x0000,     /* R173 - CSA Flash control */
	0x0000,     /* R174 - Current Sink Driver B */
	0x0000,     /* R175 - CSB Flash control */
	0x0000,     /* R176 - DCDC/LDO requested */
	0x032D,     /* R177 - DCDC Active options */
	0x0000,     /* R178 - DCDC Sleep options */
	0x0025,     /* R179 - Power-check comparator */
	0x000E,     /* R180 - DCDC1 Control */
	0x0000,     /* R181 - DCDC1 Timeouts */
	0x1006,     /* R182 - DCDC1 Low Power */
	0x0018,     /* R183 - DCDC2 Control */
	0x0000,     /* R184 - DCDC2 Timeouts */
	0x0000,     /* R185 */
	0x0000,     /* R186 - DCDC3 Control */
	0x0000,     /* R187 - DCDC3 Timeouts */
	0x0006,     /* R188 - DCDC3 Low Power */
	0x0000,     /* R189 - DCDC4 Control */
	0x0000,     /* R190 - DCDC4 Timeouts */
	0x0006,     /* R191 - DCDC4 Low Power */
	0x0008,     /* R192 - DCDC5 Control */
	0x0000,     /* R193 - DCDC5 Timeouts */
	0x0000,     /* R194 */
	0x0000,     /* R195 - DCDC6 Control */
	0x0000,     /* R196 - DCDC6 Timeouts */
	0x0006,     /* R197 - DCDC6 Low Power */
	0x0000,     /* R198 */
	0x0003,     /* R199 - Limit Switch Control */
	0x001C,     /* R200 - LDO1 Control */
	0x0000,     /* R201 - LDO1 Timeouts */
	0x001C,     /* R202 - LDO1 Low Power */
	0x001B,     /* R203 - LDO2 Control */
	0x0000,     /* R204 - LDO2 Timeouts */
	0x001C,     /* R205 - LDO2 Low Power */
	0x001B,     /* R206 - LDO3 Control */
	0x0000,     /* R207 - LDO3 Timeouts */
	0x001C,     /* R208 - LDO3 Low Power */
	0x001B,     /* R209 - LDO4 Control */
	0x0000,     /* R210 - LDO4 Timeouts */
	0x001C,     /* R211 - LDO4 Low Power */
	0x0000,     /* R212 */
	0x0000,     /* R213 */
	0x0000,     /* R214 */
	0x0000,     /* R215 - VCC_FAULT Masks */
	0x001F,     /* R216 - Main Bandgap Control */
	0x0000,     /* R217 - OSC Control */
	0x9000,     /* R218 - RTC Tick Control */
	0x0000,     /* R219 - Security1 */
	0x4000,     /* R220 */
	0x0000,     /* R221 */
	0x0000,     /* R222 */
	0x0000,     /* R223 */
	0x0000,     /* R224 - Signal overrides */
	0x0000,     /* R225 - DCDC/LDO status */
	0x0000,     /* R226 - Charger Overides/status */
	0x0000,     /* R227 - misc overrides */
	0x0000,     /* R228 - Supply overrides/status 1 */
	0x0000,     /* R229 - Supply overrides/status 2 */
	0xE000,     /* R230 - GPIO Pin Status */
	0x0000,     /* R231 - comparotor overrides */
	0x0000,     /* R232 */
	0x0000,     /* R233 - State Machine status */
	0x1200,     /* R234 */
	0x0000,     /* R235 */
	0x8000,     /* R236 */
	0x0000,     /* R237 */
	0x0000,     /* R238 */
	0x0000,     /* R239 */
	0x0003,     /* R240 */
	0x0000,     /* R241 */
	0x0000,     /* R242 */
	0x0004,     /* R243 */
	0x0300,     /* R244 */
	0x0000,     /* R245 */
	0x0200,     /* R246 */
	0x0000,     /* R247 */
	0x1000,     /* R248 - DCDC1 Test Controls */
	0x5000,     /* R249 */
	0x1000,     /* R250 - DCDC3 Test Controls */
	0x1000,     /* R251 - DCDC4 Test Controls */
	0x5100,     /* R252 */
	0x1000,     /* R253 - DCDC6 Test Controls */
};
#endif

#ifdef CONFIG_MFD_WM8352_CONFIG_MODE_1

#undef WM8350_HAVE_CONFIG_MODE
#define WM8350_HAVE_CONFIG_MODE

const u16 wm8352_mode1_defaults[] = {
	0x6143,     /* R0   - Reset/ID */
	0x0000,     /* R1   - ID */
	0x0002,     /* R2   - Revision */
	0x1C02,     /* R3   - System Control 1 */
	0x0204,     /* R4   - System Control 2 */
	0x0000,     /* R5   - System Hibernate */
	0x8A00,     /* R6   - Interface Control */
	0x0000,     /* R7 */
	0x8000,     /* R8   - Power mgmt (1) */
	0x0000,     /* R9   - Power mgmt (2) */
	0x0000,     /* R10  - Power mgmt (3) */
	0x2000,     /* R11  - Power mgmt (4) */
	0x0E00,     /* R12  - Power mgmt (5) */
	0x0000,     /* R13  - Power mgmt (6) */
	0x0000,     /* R14  - Power mgmt (7) */
	0x0000,     /* R15 */
	0x0000,     /* R16  - RTC Seconds/Minutes */
	0x0100,     /* R17  - RTC Hours/Day */
	0x0101,     /* R18  - RTC Date/Month */
	0x1400,     /* R19  - RTC Year */
	0x0000,     /* R20  - Alarm Seconds/Minutes */
	0x0000,     /* R21  - Alarm Hours/Day */
	0x0000,     /* R22  - Alarm Date/Month */
	0x0320,     /* R23  - RTC Time Control */
	0x0000,     /* R24  - System Interrupts */
	0x0000,     /* R25  - Interrupt Status 1 */
	0x0000,     /* R26  - Interrupt Status 2 */
	0x0000,     /* R27 */
	0x0000,     /* R28  - Under Voltage Interrupt status */
	0x0000,     /* R29  - Over Current Interrupt status */
	0x0000,     /* R30  - GPIO Interrupt Status */
	0x0000,     /* R31  - Comparator Interrupt Status */
	0x3FFF,     /* R32  - System Interrupts Mask */
	0x0000,     /* R33  - Interrupt Status 1 Mask */
	0x0000,     /* R34  - Interrupt Status 2 Mask */
	0x0000,     /* R35 */
	0x0000,     /* R36  - Under Voltage Interrupt status Mask */
	0x0000,     /* R37  - Over Current Interrupt status Mask */
	0x0000,     /* R38  - GPIO Interrupt Status Mask */
	0x0000,     /* R39  - Comparator Interrupt Status Mask */
	0x0040,     /* R40  - Clock Control 1 */
	0x0000,     /* R41  - Clock Control 2 */
	0x3A00,     /* R42  - FLL Control 1 */
	0x7086,     /* R43  - FLL Control 2 */
	0xC226,     /* R44  - FLL Control 3 */
	0x0000,     /* R45  - FLL Control 4 */
	0x0000,     /* R46 */
	0x0000,     /* R47 */
	0x0000,     /* R48  - DAC Control */
	0x0000,     /* R49 */
	0x00C0,     /* R50  - DAC Digital Volume L */
	0x00C0,     /* R51  - DAC Digital Volume R */
	0x0000,     /* R52 */
	0x0040,     /* R53  - DAC LR Rate */
	0x0000,     /* R54  - DAC Clock Control */
	0x0000,     /* R55 */
	0x0000,     /* R56 */
	0x0000,     /* R57 */
	0x4000,     /* R58  - DAC Mute */
	0x0000,     /* R59  - DAC Mute Volume */
	0x0000,     /* R60  - DAC Side */
	0x0000,     /* R61 */
	0x0000,     /* R62 */
	0x0000,     /* R63 */
	0x8000,     /* R64  - ADC Control */
	0x0000,     /* R65 */
	0x00C0,     /* R66  - ADC Digital Volume L */
	0x00C0,     /* R67  - ADC Digital Volume R */
	0x0000,     /* R68  - ADC Divider */
	0x0000,     /* R69 */
	0x0040,     /* R70  - ADC LR Rate */
	0x0000,     /* R71 */
	0x0303,     /* R72  - Input Control */
	0x0000,     /* R73  - IN3 Input Control */
	0x0000,     /* R74  - Mic Bias Control */
	0x0000,     /* R75 */
	0x0000,     /* R76  - Output Control */
	0x0000,     /* R77  - Jack Detect */
	0x0000,     /* R78  - Anti Pop Control */
	0x0000,     /* R79 */
	0x0040,     /* R80  - Left Input Volume */
	0x0040,     /* R81  - Right Input Volume */
	0x0000,     /* R82 */
	0x0000,     /* R83 */
	0x0000,     /* R84 */
	0x0000,     /* R85 */
	0x0000,     /* R86 */
	0x0000,     /* R87 */
	0x0800,     /* R88  - Left Mixer Control */
	0x1000,     /* R89  - Right Mixer Control */
	0x0000,     /* R90 */
	0x0000,     /* R91 */
	0x0000,     /* R92  - OUT3 Mixer Control */
	0x0000,     /* R93  - OUT4 Mixer Control */
	0x0000,     /* R94 */
	0x0000,     /* R95 */
	0x0000,     /* R96  - Output Left Mixer Volume */
	0x0000,     /* R97  - Output Right Mixer Volume */
	0x0000,     /* R98  - Input Mixer Volume L */
	0x0000,     /* R99  - Input Mixer Volume R */
	0x0000,     /* R100 - Input Mixer Volume */
	0x0000,     /* R101 */
	0x0000,     /* R102 */
	0x0000,     /* R103 */
	0x00E4,     /* R104 - OUT1L Volume */
	0x00E4,     /* R105 - OUT1R Volume */
	0x00E4,     /* R106 - OUT2L Volume */
	0x02E4,     /* R107 - OUT2R Volume */
	0x0000,     /* R108 */
	0x0000,     /* R109 */
	0x0000,     /* R110 */
	0x0000,     /* R111 - BEEP Volume */
	0x0A00,     /* R112 - AI Formating */
	0x0000,     /* R113 - ADC DAC COMP */
	0x0020,     /* R114 - AI ADC Control */
	0x0020,     /* R115 - AI DAC Control */
	0x0000,     /* R116 */
	0x0000,     /* R117 */
	0x0000,     /* R118 */
	0x0000,     /* R119 */
	0x0000,     /* R120 */
	0x0000,     /* R121 */
	0x0000,     /* R122 */
	0x0000,     /* R123 */
	0x0000,     /* R124 */
	0x0000,     /* R125 */
	0x0000,     /* R126 */
	0x0000,     /* R127 */
	0x1FFF,     /* R128 - GPIO Debounce */
	0x0000,     /* R129 - GPIO Pin pull up Control */
	0x0000,     /* R130 - GPIO Pull down Control */
	0x0000,     /* R131 - GPIO Interrupt Mode */
	0x0000,     /* R132 */
	0x0000,     /* R133 - GPIO Control */
	0x0BFB,     /* R134 - GPIO Configuration (i/o) */
	0x0FFF,     /* R135 - GPIO Pin Polarity / Type */
	0x0000,     /* R136 */
	0x0000,     /* R137 */
	0x0000,     /* R138 */
	0x0000,     /* R139 */
	0x0300,     /* R140 - GPIO Function Select 1 */
	0x0000,     /* R141 - GPIO Function Select 2 */
	0x2300,     /* R142 - GPIO Function Select 3 */
	0x0003,     /* R143 - GPIO Function Select 4 */
	0x0000,     /* R144 - Digitiser Control (1) */
	0x0002,     /* R145 - Digitiser Control (2) */
	0x0000,     /* R146 */
	0x0000,     /* R147 */
	0x0000,     /* R148 */
	0x0000,     /* R149 */
	0x0000,     /* R150 */
	0x0000,     /* R151 */
	0x7000,     /* R152 - AUX1 Readback */
	0x7000,     /* R153 - AUX2 Readback */
	0x7000,     /* R154 - AUX3 Readback */
	0x7000,     /* R155 - AUX4 Readback */
	0x0000,     /* R156 - USB Voltage Readback */
	0x0000,     /* R157 - LINE Voltage Readback */
	0x0000,     /* R158 - BATT Voltage Readback */
	0x0000,     /* R159 - Chip Temp Readback */
	0x0000,     /* R160 */
	0x0000,     /* R161 */
	0x0000,     /* R162 */
	0x0000,     /* R163 - Generic Comparator Control */
	0x0000,     /* R164 - Generic comparator 1 */
	0x0000,     /* R165 - Generic comparator 2 */
	0x0000,     /* R166 - Generic comparator 3 */
	0x0000,     /* R167 - Generic comparator 4 */
	0xA00F,     /* R168 - Battery Charger Control 1 */
	0x0B06,     /* R169 - Battery Charger Control 2 */
	0x0000,     /* R170 - Battery Charger Control 3 */
	0x0000,     /* R171 */
	0x0000,     /* R172 - Current Sink Driver A */
	0x0000,     /* R173 - CSA Flash control */
	0x0000,     /* R174 - Current Sink Driver B */
	0x0000,     /* R175 - CSB Flash control */
	0x0000,     /* R176 - DCDC/LDO requested */
	0x032D,     /* R177 - DCDC Active options */
	0x0000,     /* R178 - DCDC Sleep options */
	0x0025,     /* R179 - Power-check comparator */
	0x0062,     /* R180 - DCDC1 Control */
	0x0400,     /* R181 - DCDC1 Timeouts */
	0x1006,     /* R182 - DCDC1 Low Power */
	0x0018,     /* R183 - DCDC2 Control */
	0x0000,     /* R184 - DCDC2 Timeouts */
	0x0000,     /* R185 */
	0x0006,     /* R186 - DCDC3 Control */
	0x0800,     /* R187 - DCDC3 Timeouts */
	0x0006,     /* R188 - DCDC3 Low Power */
	0x0006,     /* R189 - DCDC4 Control */
	0x0C00,     /* R190 - DCDC4 Timeouts */
	0x0006,     /* R191 - DCDC4 Low Power */
	0x0008,     /* R192 - DCDC5 Control */
	0x0000,     /* R193 - DCDC5 Timeouts */
	0x0000,     /* R194 */
	0x0026,     /* R195 - DCDC6 Control */
	0x1000,     /* R196 - DCDC6 Timeouts */
	0x0006,     /* R197 - DCDC6 Low Power */
	0x0000,     /* R198 */
	0x0003,     /* R199 - Limit Switch Control */
	0x0002,     /* R200 - LDO1 Control */
	0x0000,     /* R201 - LDO1 Timeouts */
	0x001C,     /* R202 - LDO1 Low Power */
	0x001A,     /* R203 - LDO2 Control */
	0x0000,     /* R204 - LDO2 Timeouts */
	0x001C,     /* R205 - LDO2 Low Power */
	0x001F,     /* R206 - LDO3 Control */
	0x0000,     /* R207 - LDO3 Timeouts */
	0x001C,     /* R208 - LDO3 Low Power */
	0x001F,     /* R209 - LDO4 Control */
	0x0000,     /* R210 - LDO4 Timeouts */
	0x001C,     /* R211 - LDO4 Low Power */
	0x0000,     /* R212 */
	0x0000,     /* R213 */
	0x0000,     /* R214 */
	0x0000,     /* R215 - VCC_FAULT Masks */
	0x001F,     /* R216 - Main Bandgap Control */
	0x0000,     /* R217 - OSC Control */
	0x9000,     /* R218 - RTC Tick Control */
	0x0000,     /* R219 - Security1 */
	0x4000,     /* R220 */
	0x0000,     /* R221 */
	0x0000,     /* R222 */
	0x0000,     /* R223 */
	0x0000,     /* R224 - Signal overrides */
	0x0000,     /* R225 - DCDC/LDO status */
	0x0000,     /* R226 - Charger Overides/status */
	0x0000,     /* R227 - misc overrides */
	0x0000,     /* R228 - Supply overrides/status 1 */
	0x0000,     /* R229 - Supply overrides/status 2 */
	0xE000,     /* R230 - GPIO Pin Status */
	0x0000,     /* R231 - comparotor overrides */
	0x0000,     /* R232 */
	0x0000,     /* R233 - State Machine status */
	0x1200,     /* R234 */
	0x0000,     /* R235 */
	0x8000,     /* R236 */
	0x0000,     /* R237 */
	0x0000,     /* R238 */
	0x0000,     /* R239 */
	0x0003,     /* R240 */
	0x0000,     /* R241 */
	0x0000,     /* R242 */
	0x0004,     /* R243 */
	0x0300,     /* R244 */
	0x0000,     /* R245 */
	0x0200,     /* R246 */
	0x0000,     /* R247 */
	0x1000,     /* R248 - DCDC1 Test Controls */
	0x5000,     /* R249 */
	0x1000,     /* R250 - DCDC3 Test Controls */
	0x1000,     /* R251 - DCDC4 Test Controls */
	0x5100,     /* R252 */
	0x1000,     /* R253 - DCDC6 Test Controls */
};
#endif

#ifdef CONFIG_MFD_WM8352_CONFIG_MODE_2

#undef WM8350_HAVE_CONFIG_MODE
#define WM8350_HAVE_CONFIG_MODE

const u16 wm8352_mode2_defaults[] = {
	0x6143,     /* R0   - Reset/ID */
	0x0000,     /* R1   - ID */
	0x0002,     /* R2   - Revision */
	0x1C02,     /* R3   - System Control 1 */
	0x0204,     /* R4   - System Control 2 */
	0x0000,     /* R5   - System Hibernate */
	0x8A00,     /* R6   - Interface Control */
	0x0000,     /* R7 */
	0x8000,     /* R8   - Power mgmt (1) */
	0x0000,     /* R9   - Power mgmt (2) */
	0x0000,     /* R10  - Power mgmt (3) */
	0x2000,     /* R11  - Power mgmt (4) */
	0x0E00,     /* R12  - Power mgmt (5) */
	0x0000,     /* R13  - Power mgmt (6) */
	0x0000,     /* R14  - Power mgmt (7) */
	0x0000,     /* R15 */
	0x0000,     /* R16  - RTC Seconds/Minutes */
	0x0100,     /* R17  - RTC Hours/Day */
	0x0101,     /* R18  - RTC Date/Month */
	0x1400,     /* R19  - RTC Year */
	0x0000,     /* R20  - Alarm Seconds/Minutes */
	0x0000,     /* R21  - Alarm Hours/Day */
	0x0000,     /* R22  - Alarm Date/Month */
	0x0320,     /* R23  - RTC Time Control */
	0x0000,     /* R24  - System Interrupts */
	0x0000,     /* R25  - Interrupt Status 1 */
	0x0000,     /* R26  - Interrupt Status 2 */
	0x0000,     /* R27 */
	0x0000,     /* R28  - Under Voltage Interrupt status */
	0x0000,     /* R29  - Over Current Interrupt status */
	0x0000,     /* R30  - GPIO Interrupt Status */
	0x0000,     /* R31  - Comparator Interrupt Status */
	0x3FFF,     /* R32  - System Interrupts Mask */
	0x0000,     /* R33  - Interrupt Status 1 Mask */
	0x0000,     /* R34  - Interrupt Status 2 Mask */
	0x0000,     /* R35 */
	0x0000,     /* R36  - Under Voltage Interrupt status Mask */
	0x0000,     /* R37  - Over Current Interrupt status Mask */
	0x0000,     /* R38  - GPIO Interrupt Status Mask */
	0x0000,     /* R39  - Comparator Interrupt Status Mask */
	0x0040,     /* R40  - Clock Control 1 */
	0x0000,     /* R41  - Clock Control 2 */
	0x3A00,     /* R42  - FLL Control 1 */
	0x7086,     /* R43  - FLL Control 2 */
	0xC226,     /* R44  - FLL Control 3 */
	0x0000,     /* R45  - FLL Control 4 */
	0x0000,     /* R46 */
	0x0000,     /* R47 */
	0x0000,     /* R48  - DAC Control */
	0x0000,     /* R49 */
	0x00C0,     /* R50  - DAC Digital Volume L */
	0x00C0,     /* R51  - DAC Digital Volume R */
	0x0000,     /* R52 */
	0x0040,     /* R53  - DAC LR Rate */
	0x0000,     /* R54  - DAC Clock Control */
	0x0000,     /* R55 */
	0x0000,     /* R56 */
	0x0000,     /* R57 */
	0x4000,     /* R58  - DAC Mute */
	0x0000,     /* R59  - DAC Mute Volume */
	0x0000,     /* R60  - DAC Side */
	0x0000,     /* R61 */
	0x0000,     /* R62 */
	0x0000,     /* R63 */
	0x8000,     /* R64  - ADC Control */
	0x0000,     /* R65 */
	0x00C0,     /* R66  - ADC Digital Volume L */
	0x00C0,     /* R67  - ADC Digital Volume R */
	0x0000,     /* R68  - ADC Divider */
	0x0000,     /* R69 */
	0x0040,     /* R70  - ADC LR Rate */
	0x0000,     /* R71 */
	0x0303,     /* R72  - Input Control */
	0x0000,     /* R73  - IN3 Input Control */
	0x0000,     /* R74  - Mic Bias Control */
	0x0000,     /* R75 */
	0x0000,     /* R76  - Output Control */
	0x0000,     /* R77  - Jack Detect */
	0x0000,     /* R78  - Anti Pop Control */
	0x0000,     /* R79 */
	0x0040,     /* R80  - Left Input Volume */
	0x0040,     /* R81  - Right Input Volume */
	0x0000,     /* R82 */
	0x0000,     /* R83 */
	0x0000,     /* R84 */
	0x0000,     /* R85 */
	0x0000,     /* R86 */
	0x0000,     /* R87 */
	0x0800,     /* R88  - Left Mixer Control */
	0x1000,     /* R89  - Right Mixer Control */
	0x0000,     /* R90 */
	0x0000,     /* R91 */
	0x0000,     /* R92  - OUT3 Mixer Control */
	0x0000,     /* R93  - OUT4 Mixer Control */
	0x0000,     /* R94 */
	0x0000,     /* R95 */
	0x0000,     /* R96  - Output Left Mixer Volume */
	0x0000,     /* R97  - Output Right Mixer Volume */
	0x0000,     /* R98  - Input Mixer Volume L */
	0x0000,     /* R99  - Input Mixer Volume R */
	0x0000,     /* R100 - Input Mixer Volume */
	0x0000,     /* R101 */
	0x0000,     /* R102 */
	0x0000,     /* R103 */
	0x00E4,     /* R104 - OUT1L Volume */
	0x00E4,     /* R105 - OUT1R Volume */
	0x00E4,     /* R106 - OUT2L Volume */
	0x02E4,     /* R107 - OUT2R Volume */
	0x0000,     /* R108 */
	0x0000,     /* R109 */
	0x0000,     /* R110 */
	0x0000,     /* R111 - BEEP Volume */
	0x0A00,     /* R112 - AI Formating */
	0x0000,     /* R113 - ADC DAC COMP */
	0x0020,     /* R114 - AI ADC Control */
	0x0020,     /* R115 - AI DAC Control */
	0x0000,     /* R116 */
	0x0000,     /* R117 */
	0x0000,     /* R118 */
	0x0000,     /* R119 */
	0x0000,     /* R120 */
	0x0000,     /* R121 */
	0x0000,     /* R122 */
	0x0000,     /* R123 */
	0x0000,     /* R124 */
	0x0000,     /* R125 */
	0x0000,     /* R126 */
	0x0000,     /* R127 */
	0x1FFF,     /* R128 - GPIO Debounce */
	0x0000,     /* R129 - GPIO Pin pull up Control */
	0x0110,     /* R130 - GPIO Pull down Control */
	0x0000,     /* R131 - GPIO Interrupt Mode */
	0x0000,     /* R132 */
	0x0000,     /* R133 - GPIO Control */
	0x09DA,     /* R134 - GPIO Configuration (i/o) */
	0x0DD6,     /* R135 - GPIO Pin Polarity / Type */
	0x0000,     /* R136 */
	0x0000,     /* R137 */
	0x0000,     /* R138 */
	0x0000,     /* R139 */
	0x1310,     /* R140 - GPIO Function Select 1 */
	0x0033,     /* R141 - GPIO Function Select 2 */
	0x2000,     /* R142 - GPIO Function Select 3 */
	0x0000,     /* R143 - GPIO Function Select 4 */
	0x0000,     /* R144 - Digitiser Control (1) */
	0x0002,     /* R145 - Digitiser Control (2) */
	0x0000,     /* R146 */
	0x0000,     /* R147 */
	0x0000,     /* R148 */
	0x0000,     /* R149 */
	0x0000,     /* R150 */
	0x0000,     /* R151 */
	0x7000,     /* R152 - AUX1 Readback */
	0x7000,     /* R153 - AUX2 Readback */
	0x7000,     /* R154 - AUX3 Readback */
	0x7000,     /* R155 - AUX4 Readback */
	0x0000,     /* R156 - USB Voltage Readback */
	0x0000,     /* R157 - LINE Voltage Readback */
	0x0000,     /* R158 - BATT Voltage Readback */
	0x0000,     /* R159 - Chip Temp Readback */
	0x0000,     /* R160 */
	0x0000,     /* R161 */
	0x0000,     /* R162 */
	0x0000,     /* R163 - Generic Comparator Control */
	0x0000,     /* R164 - Generic comparator 1 */
	0x0000,     /* R165 - Generic comparator 2 */
	0x0000,     /* R166 - Generic comparator 3 */
	0x0000,     /* R167 - Generic comparator 4 */
	0xA00F,     /* R168 - Battery Charger Control 1 */
	0x0B06,     /* R169 - Battery Charger Control 2 */
	0x0000,     /* R170 - Battery Charger Control 3 */
	0x0000,     /* R171 */
	0x0000,     /* R172 - Current Sink Driver A */
	0x0000,     /* R173 - CSA Flash control */
	0x0000,     /* R174 - Current Sink Driver B */
	0x0000,     /* R175 - CSB Flash control */
	0x0000,     /* R176 - DCDC/LDO requested */
	0x032D,     /* R177 - DCDC Active options */
	0x0000,     /* R178 - DCDC Sleep options */
	0x0025,     /* R179 - Power-check comparator */
	0x000E,     /* R180 - DCDC1 Control */
	0x0800,     /* R181 - DCDC1 Timeouts */
	0x1006,     /* R182 - DCDC1 Low Power */
	0x0018,     /* R183 - DCDC2 Control */
	0x0000,     /* R184 - DCDC2 Timeouts */
	0x0000,     /* R185 */
	0x0056,     /* R186 - DCDC3 Control */
	0x1800,     /* R187 - DCDC3 Timeouts */
	0x0006,     /* R188 - DCDC3 Low Power */
	0x000E,     /* R189 - DCDC4 Control */
	0x1000,     /* R190 - DCDC4 Timeouts */
	0x0006,     /* R191 - DCDC4 Low Power */
	0x0008,     /* R192 - DCDC5 Control */
	0x0000,     /* R193 - DCDC5 Timeouts */
	0x0000,     /* R194 */
	0x0026,     /* R195 - DCDC6 Control */
	0x0C00,     /* R196 - DCDC6 Timeouts */
	0x0006,     /* R197 - DCDC6 Low Power */
	0x0000,     /* R198 */
	0x0003,     /* R199 - Limit Switch Control */
	0x001C,     /* R200 - LDO1 Control */
	0x0000,     /* R201 - LDO1 Timeouts */
	0x001C,     /* R202 - LDO1 Low Power */
	0x0006,     /* R203 - LDO2 Control */
	0x0400,     /* R204 - LDO2 Timeouts */
	0x001C,     /* R205 - LDO2 Low Power */
	0x001C,     /* R206 - LDO3 Control */
	0x1400,     /* R207 - LDO3 Timeouts */
	0x001C,     /* R208 - LDO3 Low Power */
	0x001A,     /* R209 - LDO4 Control */
	0x0000,     /* R210 - LDO4 Timeouts */
	0x001C,     /* R211 - LDO4 Low Power */
	0x0000, /*
 /* R212 */
	0x0000,/*
 * wm8353-regmap.c  --  Wolfson 4-regmap.c  --  Wolfson 5 - VCC_FAULT Masks-regmap.c1F--  Wolfson 6 - Main Bandgap Control-regmap.c  --  Wolfson 7 - OSCess
 * status o9c  --  Wolfson 8 - RTC Tickess
 * status of the WM8350 reg9 - Security1-regmap4c  --  Wolfson20-regmap.c  --  Wolfson2 *
 *  Tware; you can re0-regmap.c  --  Wolfson2Microelectronics WM835024onicignal overridee tables dhe GNU GeneralfileDCDC/LDO statu published by the
 *  FrdefaCharger Ovee as /oundation;  either version 2 istemiscicense as published by the
 *  Fr
 * Supplyicense as (at yourdistribute  it and/or moronic.h>

#ifdef CONFIG_MFD_0-regmapEd by the
 *  F30 - GPIO Pin Sundation;  either version 231 - comparotoricense as published by the
 *  F30-regmap.c  --  Wolfson33coretate MachineFoundation;  e12R1   - ID */
	register map
 *
 * This35-regmap8ol 1 */
	0x00046em Control 1 */
	0x00047em Control 1 */
	0x00048em Control 1 */
	0x00049-regmap.c 3--  Wolfson4ree software; you can r4distribute  it and/or m40-regmap.c 4  /* R8   - Microelec3    /* R8   - register map
 *
 * This4,     /*0  /* R3   - Sy4ontrol 2 */
	0x0000,   4 /* R5  100,     /* R11 
 * oftw1 Testess
 * se tables500,     /* R11 6   - In mgmt (4) */
	05FIG_oftw3 /* R12  - Power mgmtR13  - Power mgm] = oftw4 /* R12  - Power mgmt 13  - Power mgm0-regmapR13  - Power mgm0x00oftw6 /* R12  - Power m};
#endif

#ifdef CONFIG_MFD_WM8352_x0101,  ODE_3

#un/
	0/* R10_HAVE  - RTC Date
#def R2 0x1400,     /* R19  - RT
const u16 wm R18 mode3_defaults[] = {gmap614face Contro0   * Ceset/ID-regmap.c  --  Wolfso1urs/D	0x0000,    2--  Wolfsonurs/Dayvision - RTC SC*/
	0x0320, 3urs/DSystemess
 * stdistribut2 (1) */
	0x04x0000,     /* R24  -0-regmap.c  --  Wolfso5x0000,     /Hibern,       /* A  --  Wolfso6larm Dnterfaceess
 * status of the WM8350 r /* R5   R4   - System8urs/DPower mgmt (1)-regmap.c  --  Wolfso9  /* R28  - Under2Voltage Interrupt stat10 /* R28  - Under3Voltage 2 Over Current 1 /* R28  - Under4Voltage IEOver Current 2 /* R28  - Under5/* R29  - Over Current 3 /* R28  - Under6/* R29  - Over Current 4 /* R28  - Under7/* R29  - Over Current /* R10  - Over Current 6rs/DaTCics onds/Minuts publishe000,     /* R17     /* Hours/Dayt Status 21Mask */
	0x8     /* Date/Month - RTC S42 Mask */
	0x9     /* Year-regmap.c  --  WolfsonInterAlarm R34  - Interrupt Statusc  --  Wolfson 0x0000,    /* R35  - Power d by the
 *  Fr0x0000,   k */
	0x0000,    032 1 */
	0x0004     /* Tim   /* R27  - Power Up Interrup2ask *,     /x0000rupter Current Interrupt st5/
	0x0000 - Cnst u16 distribute  it and/or m,       /* R41  - Cloc0-regmap.c  --  Wolfson /* R5   - System HibertatusUnder Voltage /* R42  - oundation;  either version 2r Volnse, Current* R44  - FLL Control 3 */
	0x0000,    3InterMODE
   /* R41  - Cloc     /* R46 */
	0x000atus C
	0x1aFF, R47 */
	0x0000,     /* 3FFscribing th3nterr     /* R40  - Clot th    /* R48  - DAC Cont  - C   /* R41  - Clock 	0x00C0,     /* R51  - DACask */* R42  - FLL Contr	0x00C0,     /* R51  - DACtatus 1 Mask */
	0x0003,    0xC226,     /* R44  - FLL Contr	0x00C0,     /* R51  - DAC0000,5  - FLL Control 4 */
	0x0000,	0x00C0,     /* R51  - DACtatus  /* R47 */
	0x0000,  	0x00C0,     /* R51  - DACr Vol/
	0x0000,     /* R49 */
	0x	0x00C0,     4 --  Wolfso4InterCloght 2007, 2distribute  it and/or 4rol */  /* R63 */
	0-regmap3rupt Status 24nterrFLL R63 */
	0x8000, 708665 */
	0x00  - C   /* R66  -0-regmapC22l Volume L *ask *   /* R66  -Microelectronics WM835400,     /* R66  -register map
 *
 * Thi) */
	0x2000,     /* R1  - Power0  - ADC LR RattatusDA since they are 0  - ADC LR Rat6   - InteC Status 1 */Inter	0x0Digital6,  ume Lol */
	0x0000,     /* atus - IN3 Input ControlR Interrupt Status 1 */0-regmap.c R62 */
	0x05  - C	0x0LR R26  - Inteupt Status 1 */ask *	0x03  /* R63 */
	Interrupt Status 1 */tatus 1 Mask */
	0x0005ontrol 2 */
	0x0000,  5 /* R5  his program is51 */
	0x0Mu */
	0x0000,     /* R77 r VolVolume */Controlregmap.c  --  Wolfso6R73  - INSidme */
	0x0000,     /* Rdistribute  it and/or 60-regmap.c  --  Wolfso6Microele R4   - System6ask *ADx0303,     /* R72  - Input Con6tatus 1 Ma0000,     /*6,      /*3 Input Control */
	0x0000,     /* R760000,000,     /* R89  - 	0x0000,     /* R75 */6tatus000,  viC2260000,     /* R91 */
	6   - Inte R62 */
	0x07	0x000DControl */
	0x0000,     /* R777distribut3rface Contro7nterrInpuR12  - Po Control */
	0x0000,   DigitN3* R95 */
	0x0000,     /* R96  - Outpask *Mic Bias  /* R27  - Power Up Interrupttatus 1 Mask */
	0x0007,    Out95 */
	0x0000,     /* R96  - Outp0000,Jack Detect /* R98  - Input Mixer0x0000nti Pocess
 * status of the WM8350 r7ol */
	0x0000,     /* 8InterLefntro95 * Volume */
	0x00000,     /* atus Righ	0x0000,     /* R103 *//
	0x0000,   0-regmap.c  --  Wolfso8Microelectronics WM8358register map
 *
 * Thi8tatus 1 Mask */
	0x0008ontrol 2 */
	0x0000,  8 /* R5   80000,     /* tatus*/
	0Mix - Fs
 * status o mgmt (4) */
	8r Volt04 - 0x0000,     /* R110 Interrupt staturee software; you can 9distribute  it and/or 9nterrOUT3 BEEP Volume */
	0x0A00,     /* R112  - COUT4 BEEP Volume */
	0x0A00,     /* R112register map
 *
 * Thi9tatus 1 Mask */
	0x0009 Volume L */ */
	0x0000 Volume */
	0x0000,     /* 9/* R57e L */111 - BEEP V0,     /* R118 */
	0x0000,  tatusx0000,000,     /* R */
	0x000*/
	0x0000,  r Vol1 */
	0x0000,     /	0x0000,     /* R75 */10FIG_1 */
	0x0000,     / R124 */
	0x0000,     distribute  it and/or 100-regmap.c  --  Wolfso10MicroelectE(1) */
	0x010  PuOUT1L    /* R120 */
	000,     /* R1file GPIR Pin pull up Control */
	0x03FdefaOUT2O Pin pull up Co2000,     /* R1isterUT2* R130 - GPIO Pul - GPIO Debounate */
	0x8A00,     /*106   - Inter    /* R30  -ree software; you can 11] = BEEP    /* R120 */
rupt Status 21120000I Formating* R134 - GPIO Configura0x00000, 0x03OMP  /* R1260,     /* R311  PuAI   /* R87 */
	0x0800,/* R137 */
	0xfileAI
	0x0303,     /* R72  - Input Con11ontrol 2 */
	0x0000,  11 /* R5   - System Hibe11000,     /* R133 - GPIO1Control */
	0x0FFC,    free software; you can 1edistribute  it and/or 1odify it
 *  under  the1terms of  the GNU Gener12register map
 *
 * Thi12tatus 1 Mask */
	0x00002Select 1 */
	0x0000,    /* R43  1   /* R50  - 10/corMODE
Debounc   /* R1261 */
	0x0002, roniMODE

conpull ucess
 * status of the WM8350 r1NFIG_MODE

 R14down*/
	0x0000,     /* R150 */
	0x0] =   /* R47 */
	0xMo00,     /* R83 */
	0x00100,     /* R1   - ID */1	0x00MODE
ss
 * status ofBFB,     /* R15  PuAUX3 Reafigurat - R(i/oVoltage IFFD,     /* R15fileMODE

conPolaLC.
 / Typ  /* R153 - AUX2 ReadbacSelect 1 */
	0x0000,     /* R5   - System Hibe1nate */
	0x8A00,     /*1R6   - Int3  /* R148 */
4FIG_MODE
Func */
	Sel R *distribute  Interrupt S4eadback **/
	0x0000,     0-regmap2 R9   - Power14PIO 161 */
	0x0000,     /Microelectrface Contro144 - AUX3 */
	0x0000,     Control (1) */
	0x0002,4  Pu3 Inpis000,     /*r Voltage Int/
	0x0320, 14ee So165 - Generic comp /* R29  - Over Current ) */
	0x2000,     /* R11te */
	0x0000,     /* R14ge Readback */
	0x0000,trol */
	0x- Interrupt Stn Select 3 */
	0x0003, 5 ADC Digit- Battery CharPIO PUX1 Readb Volr Control 2 */
	0x0000 /* RUX2 R170 - Battery Charger Control 0000,UX3 R170 - Battery Charger Control  */
	UX4 R170 - Battery 1 */
	0x0B06,   defaUSB6,     /* /* R173 - CSA Flash control */isteLINE0,     /* R174 - Current Sink Driver B *
 * BATT0,     /* R174 - Current Sink Driver B *roniChip Temp* R174 - Current Sink Driver B 6n Select 3 */
	0x0003, 00,     /* R84 */
	0x00100,     /* R85 */
	0x00164 - Aeneric */
	0x0000,/
	0x0000,     /* R150 */
	0x655 - omparat{
	0x1000, Sleep options */
	0x0025ltage	0x0000,     /* R1,     /* R179 - Power-chdefa06,     /* R182 - D    /* R144 - Digitiser6iste06,     /* R182 - DregisterA00 */
	0x0000,6x0000atterythe  LiceR63 */
	0x8000,  B0l Volume L 16roni
	0x0000,     /* R186 - ,     /* R179 - Power-c7FIG_
	0x0000,     /* R186 -     /* R144 - Digitiser    /* R94/* R189 - DCDC4PIO FLL ContSink Dri  - A00,     /* R189 - DCDC40x00CSA Flash c
	0x0000,     /* R150 */
	0x7  PuR190 - DCDC4 TimeoutB/
	0x0008,     /* R192 fileCSBC4 Low Power */
	0x0008,     /* R192 defaoftware Frequested*/
	0x0000R156 - USB V7isteoftw Active op */
     /* R48  - DAC Con170E00,    Sleep Timeouts */
	0x0256,     /* R1roniR28  -check0,     /* R1regmap.c ol */
	0x0008t (6) */100E,     /* R180/* R36  - Unde8 */
	0x01aratoouClock Con1itch Control */PIO 000,  Low R28  -0000,    8 Control */
*/
	0x01200E,     /* R180 - DCDC1 Contr8 /* R,       /* R201 - LD2 Control */
	0xtatus 1 Ma5ontrol */
	0x     /* 300E,     /* R180CControl */
	0x/* R196 3 /* R204 - LDO2 Timch Control */0E00,   3   /* R202 - LDO1 L0Entrol */
	0xroni	0x000    /* R200 - LDO1 Control */9t (6) */4*/
	0x0000,     /* R207 - LDO3 9 */
	0x000
	0x001C,     /* R2w Power */
	90x001C, 500E,     /* R180 - DCDC1 Contr9*/
	0x015 /* R204 - LDO2 Timeouts */
	0xl */
	0x00029* R214 */
	0ee Softw6
	0x001B,     /*0x0000,     /19     /* 6	0x0000,     /* R210 - LDO4 Time/* R196 60x001C,     /* R211   /* R216 - Mate */
	0x8*/
	0x0000, 9roniLimit Switch00E,     /* R180 1R156 - USB 2 /* RLDO,     /* R200 -  mgmt (4) */
	00] = T 1 */* R204 - LDO2 Ti1CR221 */
	0x0PIO T 1 *  /* R202 - LDO1 Lo7R221 */
	0x00x00LDO   /* R203 - LDO /* R221 */
	0x0  Pu225 -/* R222 */
	0x0000,     /* R223file225 -
	0x001C,     /* R2l Volume L 2l */
LDOr */
	0x001B,    /* R221 */
	0x0/
	0x	0xE/* R222 */
	0x0000,     /* R223
 *  */
	  /* R202 - LDO1 Lo R221 */
	0x00000,DO/
	0x001B,     / mgmt (4) */
	01 BIST 1
	0x0000,     /* R20,     /* R22ation */
	ontrol */
	0x9000,     /* R218350-regmap.c  --  Wolfson Microelectronics WM8350 register map
 *
 * This file splits out the tables describing the defaults and access
 * status of the WM8350 registers since they are rather large.
 *
 * Copyright 2007, 2008 Wolfson Microelectronics PLC.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/mfd/wm8350/core.h>

#ifdef CONFIG_MFD_WM8350_CONFIG_MODE_0

#undef WM8350_HAVE_CONFIG_MODE
#define WM8350_HAVE_CONFIG_MODE

const u16 wm8350_mode0_defaults[] = {
	0x17FF,     /* R0   - Reset/ID */
	0x1000,     /* R1   - ID */
	0x0000,     /* R2 */
	0x1002,     /* R3   - System Control 1 */
	0x0004,     /* R4   - System Control 2 */
	0x0000,     /* R5   - System Hibernate */
	0x8A00,     /* R6   - Interface Control */
	0x0000,     /* R7 */
	0x8000,     /* R8   - Power mgmt (1) */
	0x0000,     /* R9   - Power mgmt (2) */
	0x0000,     /* R10  - Power mgmt (3) */
	0x2000,     /* R11  - Power mgmt (4) */
	0x0E00,     /* R12  - Power mgmt (5) */
	0x0000,     /* R13  - Power mgmt (6) */
	0x0000,     /* R14  - Power mgmt (7) */
	0x0000,     /* R15 */
	0x0000,     /* R16  - RTC Seconds/Minutes */
	0x0100,     /* R17  - RTC Hours/Da/* The register /
	0x000 for the Powfig utes used must bx000mpiled in but
 * due to
	0x0impact on kernel size it is possibl,    disable
 - L#if */
	0x1400,     /* R19  - RTCwarnity No*/
	0x00000,     /*s suppor6 Co- s,     at leas- RTe of
	0x /* R25  -    /* R10  - RTC Date/n Timeoutsfrom
	0x0board dimeou.000,     /*0x03Access m the. - Sy Alarm structonds/M0_reg_a Under */
	0x0000io_map0,     //*  read/*
 write volatil   /* { 0xF   /*R30  - GPIO In },arm Hours/Day */
	0x0000* R37C - GPI  /* R0x7pt Status *Alarm Date/Mo* R3007ompara,    2  - SStatus *    /* OM0x00000x3FFF,    BE3Byste33  - In R4 Status *0x0000,     /* R24  - Syste* R30EF7 GPIOrrupt St8atus 1 Mas	0x0000,     /* R25  - Inte* R380 - GPIerrupt Staatus 1 Mas/
	0x0000,     /* R26  - In* R30B208 e Interrupm Interrupts */
	0x0000,     /* R27  - P     / System Inystem Interrupts /* R5* R3E53upt S /* R38 upt Status *   /* R28  - Under Voltag     FF3ystem39  - Cupt Status *us */
	0x0000,     /* R29     /8 R32  -R40  -upt Status * Interrupt status */
	0x0* R36D3CrrupControl ontrol 1 */
	- GPIO Interrupt Status * R31FR40  -086,    ontrol 1 */
	* R31  - Comparator Inte InteF3upt St FLL Coontrol 1 */
	0x3FFF,     /* R32  - Syrent In  - Co
	0x000ontrol 1 */
	ask */
	0x0000,     /* Rrent Interrupt status Mask */
	0Status 31  -F* R32    /* R4ontrol 1 */
	,     /* R34  - Interrupt Sta     7FLL Cox00C0,  ontrol 1 */
	0000,     /* R35  - Pow	0x708FLL Co  /* R52ontrol 1 */
	tatus Mask */
	0x0000,  * R3    /*000,Interrupt Status *er Voltage Interrup0,     /* R49 */
	0xm Interrupts 	0x0000,     /* R37  - Over Cur /* R30C0,      /* R5 Interrupts atus Mask */
	0x0000,    ,     /* R52 */
	0x0C Mute */
	0xnterrupt Status Mask */
	0       /* R4EA*/
	0x00C0,     /*9  - Comparator Interrupt S0000,B    /* R statu,     /* R62 0040,     /* R40  - Clock C InterEupt S /* R64  - ADC Contro00,     /* R41  - Clock Cont0000,50,     /* R64  - ADC Contro     /* R42  - FLL Control 100,     /* R5/* R64  - ADC Contro0000,R28  -Up     /* R49 */
	0x00C0Divider */
	0x0000,     /* R69 */ */
	0xC226,     /* R44  - FLL Control 3 Internterrupt statu,     /* R62 /* R45  - FLL Control 4 */
	0x0000,    	0x7080,     /* R64  - ADC Contr00,     /* R47 */
	0x0000,     /   /* R61 */0000,     /* R75 */
	rol */
	0x0000,     /* R49 */
	0x00C00000,     /*,     /* Ratus 1 MaskAC Digital Volume L */
	0x00C0, 
	0x00C0,   00C0,     /* R75 */
	 Digital Volume R */
	0x0000,    * R307  - AD0000,   	0x0000,     0,     /* R53  - DAC LR Rate */
	00,     /* R58  - DAC Mute */
	0300,  040,     /* R70  - ADC LR    /* R84 */
	0x0000,     /* R85 */
	0x000*/
	0x0000,     /* R5558  - DAC Mute */0,     /* R7   /* R73  -*/
	0x000/* R57 */
	0x4000,      /* R89  - Right Mil */
	0xl */
	0x0000*/
	0x000R59  - DAC Mute Volume */
	0x0000,     /* R61 */
	0x0000,     /* R56 Side */
	0x0000,   ,     /* R87 */
	0x0C9rupt S  /* R96upt Status *000,     /* R63 */
	0x80000,     1Control- Out	0x0000,    ADC Control */
	0x0000,   * R30   Output  Output Left MixerC0,     /* R66  - ADC Dage In - GPIO00,     /* xer Volum/
	0x00C0,     /* R67  * R30  - GPIO Interrt Mixer Volumume R */
	0x0000,     /     /30x0000,,     /*000,     /*der */
	0x0000,     /* rent Interrupt status Mask */
	0) */
	0me */
	0x00E4,     /* R105 - ROUTx0000,    3* R103 /
	0x02E*/
	0x00E4,  1 */
	0x0303,     /* me */
	0x00E4,     /* R105 - ROUT6   -  Inte1rupt St00,    upt Status * R73  - IN3 Input Control */
	0	0x0000,     /* R110 */
	0x0000,   - Mic Bias Control */
	0x000rent Interrupt status Mask */
	0
	0x000 /* R39 /* R58* R110 */
	0x0000, Output Control */
	0x     /
	0x
	0x0000,        /* R11 - Jack Detect */
	0x0000, C DAC COMP */
	0x0020,     /* R11	0x0000,  C COMP */
	0x0020,     /* R111 Volume */
	0x00E4,     /* R106 - LOU40,    * R3his pr*/
	0x0000116 - AIF Tesnput Volume */
	0x31  -  /* R7x0000,  116 - AIF Tes - Right Input Volume */
me */tor Int /* R127 Mask */
	0x082 */
	0x0000,    /* R119 */
	0x0000,     /* R120 00,    e */
	0x0000,     /* R129 - GPIO 4 - AI ADC 	0x0000,     /* R129 - GPIO 2 */
	0x008 */
	* R131 - Gupt Status *00,     /* R87 */
	0x0e */
	0x0000,     /* R129 - GPIO 	0x0000,  000,     /* R110 */
	0x0000,/
	0x1000,     /* R89  - Right  */
	0x00FB,     /* R134 - GPIO C	0x0000,     /* R90 */
	0x0000 ADC Control */
	0x0 R129 - GPIO 0x0000,     /* R92  -e */
	0x0000,     /* R129 - GPIO 09 */
	0x0 Control */
	0x0020,     /* R93  - OUT4 Mixer Contrent Interrupt status Mask */
	0xPin pull up70000,  142 - GSelect 1 */
	   /* R95 */
	0x0000,  ,    xer C- GPIO FunSelect 1 */
	ut Left Mixer Volume */
	0x3 - GP9   /*ontrol (Select 1 */
	 - Output Right Mixer Volurent Interrupt status Mask */
	0x,     /* R1F15/* R58* R147Select 1 */
	 Volume L */
	0x0000,   3 - GP statu*/
	0x00Select 1 */
	 Mixer Volume R */
	0140 -3ontrol 000,     Mask */
	0x0 - Input Mixer Volume */
rent Interrupt status Mask */
	0x09 */
	0x0E1Ftrol - AUX3 Rsk */
	0x000 R102 */
	0x0000,     /* R10154 - AUX3 Readback */
	0x7000,   /* R104 - LOUT1 Volume */
	/* R119 */
	0x0000,     /* R120 105 - R  /* R157 - LINE Voltage Readback2 */
	0x0000157 - LINE Voltage Readback1 Volume */
	0x00E4,     /* R105 - ROU001C,   Readback */
	0x0000,     /* R1601 Volume */
	0x00E4,     /* R106 - LOU R108 ** R39842 - Grator CoInterrupt Sta R109 */
	0x0000,     /* R11omparat - In*/
	0x00Interrupt Sta /* R111 - BEEP Volume */
	0x/* R119 */
	0x0000,     /* R120 2 - AI      /* R166 - Generic comparator     /* R97909 - GP*/
	0xAr Interrupt SDAC COMP */
	0x0020,     /* omparEtor Con6,     r Interrupt Strol */
	0x0020,     /* R115     /* R166 - Generic comparator1 Volume */
	0x00E4,     /* R105 - ROU16 - AI140 -EEtatus 172 - Cu71 */
	0x000,     /* R117 */
	0x0000,     /* R154 -072 - Cntrol */ink Driver A    /* R119 */
	0x0000,     /* R120  /* R100,     /* R17571 */
	0x000* R121 */
	0x0000,     /* R122h contr- OutO requesFlash control/* R123 */
	0x0000,     /* R120,     errupx0000,  C Control */
 /* R125 */
	0x0000,     /* 00,     /* R48  - DAC Control */
    /* * R179 - Power-check comparator */ */
	0x0000,     /* R158 - BATT Voltagunce */
154 - AUX3 Readback ontrol 1 */
	0   /* GPIX4 Readback */
	0x0000,     /* Rontrol 1 */
	0fileR0x0018,     /* R183 - DCDC2 Control */
	0x0000,   9 */
ode 3 - CSA Flash con7AUX3 Re3 Controntrol 1 */
	0iste R18    /* R186 - DCD    /* R181 - DCDC1 Timeouts *ate */ */
	0x0006,     /* R188 - DCDC3 L  /* R140 -   /* R181 - DCDC1 Timeouts  /* R13 InterE Contro
	0x003B00,     /* Rtion (i/o) */
	0x0FF/* R3F statu    /* R* R190 - DCDC4PIO Pin Polarity / Ty */
	0F */
	0xC5 TimeoR190 - DCDC4 /* R136 */
	0x0000,  - DCDC8  /* R5195 - DR190 - DCDC40000,     /* R138 */
	02 */
	000,Timeouts *ontrol */
	0x0 */
	0x0013,     /* R1431  - 3r Inte0,     ontrol */
	0x0defaAIF /* R1ontrol */
	0x0400,     /* R190 - DCDC4eric Compa/
	0x0400,     /* R190 - DCDC4ow Power */
	0x0062,     /* R189 - DCDC   /* R- LDO1 Control */
	0x0400,     /*free so LDO1 Low Power */
	0x0006,     /*
	0x0062,     /* R180 - DCDC1 Control *odify i LDO2 Timeouts */
	0x001C,     /*  Readback */
	0x0000,     /* R159 - Chr Contro- LDO3 Control */
	0x0000,     /*  */
	0x0000,     /* R161 */
	0x0000,  r Contro LDO3 Low Power */
	0x001B,     /*eric Compa   /* R92  - OUT3 Mixer Cont,     /* R147 */
	0x0000 LDO4 Timeouts */
	0x001C,     /* 0x0000,     /* R149 */
	0x0000,   LDO4 Timeouts */
	0x001C,     /*0000,     /* R151 */
	0x7000,   R214 */
	0x0000,     /* R215 - VCCeadback */
	0x7000,     /* R- LDO1 Control */
	0x0400,     /*00,    - LDO1IO Funrol */
	0x* R218 - RTC4 - AUX3 Readback */
R214 */
	0x0000,     /* R215 - VCC55 - AUX4 Readback */
	0x0000,   R214 */
	0x0000,     /* R215 - VCCltage Readback */
	0x0000,     /*trol */
	0x9000,     /* R218 - RTC R209 - LDO4 Control */
	0x0000,     /*  /* R56 */
	0x0000,     /* R227 */
	0x00ow Power */
	0x0062,     /* R189 - DCDCR6   - ume */
	0x0000,     /* R101 */
	p Readback */
	0x0000,     /* R160x0000,     /* R231 */
	0x0000,   /* R161 */
	0x0000,     /* R1620x0000,     /* R231 */
	0x0000,   * R163 - Generic Comparator Con- LDO1  CSB FlaR231 */
	0x0000,      /* R164 - Generic comparator* R30R54  - DR54  - A/
	0x0000,    /* R165 - Generic comparator me */142 - G244 */
	*/
	0x0000,   /* R166 - Generic comparator 3- LDO1 Control */
	0x0400,     /*T1 Volume */
	0x00E4,     /* R106 - LOUrator 4 */
	0x0000,     /* R249 */
	0x0000ow Power */
	0x0062,     /* R189 - DCDC109 */
	0x0    /* R48  - DAC Control */
	 R203 - LDO2 Control */
	0x0400,     /*rger Co0,    ontrolx0000,  DAC Clock Con0,     /* R170 - BatterWM8350_CONFIG_MODE_2

#undef WM8353 */
	0x0000,     /* RWM8350_CONFIG_MODE_2

#undef WM835  /* R172 - Current SiWM8350_CONFIG_MODE_2

#undef WM8350000,     /* R173 - CS140 - GPIO Fu/* R64  - ADC Contr*/
	0x0000,     /* R174 - Curre* R3   - System Control 1 */
	0x00/
	0x0000,     /* R175 - CSB F* R3   - System Control 1 */
	0x00x0000,     /* R176 - DCDC/LDO * R3   - System Control 1 */
	0x002D,     /* R177 - DCDC Acti*/
	0x0000,     /* R254 */
	0x0000000,  gmt (2) */
	0x0000,     /* R10  - 
	0x0062,     /* R180 - DCDC1 Control * R130 - GPIO P  /* R239 */
	0x0000,     heck comparator */
	0x000E,     /* R0x0000,     /* R231 */
	0x0000,  ol */
	0x0000,     /* R181 - D* R14  - Power mgmt (7) */
	0x00000x1006,     /* R182 - DCDC1 L* R14  - Power mgmt (7) */
	0x00008,     /* R183 - DCDC2 Contro* R14  - Power mgmt (7) */
	0x0000/* R184 - DCDC2 Timeouts */
	08,   ontrol */
	0x0sk */
	0x000185 */
	0x0000,     /* R186 - DCDC3 * R30  - GPI4*/
	0x00/* R21  - Ala,     /* R187 - DCDC3 Timeouts */
     /* R32  -* R32  - S DAC Digit88 - DCDC3 Low Power */
	0x0000,  */
	0x0000,     /* R254 */
	0x00031,     /* 90FLL CoInterrup1  - DAC Digit /* R190 - DCDC4 Timeouts */
	154 -3,     Status *1  - DAC Digit1 - DCDC4 Low Power */
	0x  - Interrupt Status 2 */
	0x0000,- DCDC5 Control */
	0x0000,   upt Status */
	0x0000,     /* R28 Timeouts */
	0x0000,     /44  - FLL Control 3 */
	0x0000,  ,     /* R195 - DCDC6 Controme *//
	0x2  - Syste
	0x0000,     /* R196 - DCDC6 Timeouts */
*/
	0xInterrupInterrupts Mask */
	97 - DCDC6 Low Power */
	0x* R3517000, sk */
	0 /* R21  - Al */
	0x0003,     /* R199 - Limit*/
	04* R32  0000,   - System Int/
	0x001C,     /* R200 * R30 ion Sele0000,     /* R219 *
	0x0000,     /* R201 - 31  -3 /* R49ask */
6  - Under Vol0x001C,     /* R202 - LD2 Mas35uts **/
	0x006  - Under Volx001B,     /* R203 - LMask */
	0x0000,     /* R37  - Ove0000,     /* R204 - LDO*/
	0x0000,     /* R254 */
	0x0000 */
	0x00000000,     /* R36  - Under VolLow Power */
	0x001B, Mask */
	0x0000,     /* R37  - Ove Control */
	0x0000,   tus Mask */
	0x0000,     /* R38  -imeouts */
	0x001C,     3B00,     /* R42  - FLL Control 1  Power */
	0x001B,    Mask */
	0x0000,     /* R37  - Ovontrol */
	0x0000,     /tus Mask */
	0x0000,     /* R38  eouts */
	0x001C,     /* ask */
	0x0000,     /* R39  - Comower */
	0x0000,     /*AC Control */
	0x0000,     /* R49     /* R213 */
	0x0000,*/
	0x0000,     /* R254 */
	0x000trol 3 */
	00000,     /* R36  - Under VoC_FAULT Masks */
	0x001AC Control */
	0x0000,     /* R49 ain Bandgap Control */
 - DAC Digital Volume L */
	0x00C017 - OSC Control */
	0x9trol */
	0x0000,     /* R55 */
	0xow Power *FFD0x0000,
	0x800,     /* R57 0000,     /* R219 */
	0x4000,2 */
4escr*/
	0x00C0,     /* R56 * BIST 1 */
	0x0000,  Mask */
	0x0000,     /* R37  - Ox0000,     /* R222 */
	0R32  -0x00C0R68  - AADC Digital Vo */
	0x0000,     /* R225 */
	0x00C0,     /* R66  - ADC Di/* R225 - DCDC/LDO st00C0,     /* R67  - ADC Digital Vo   /* R226 */
	0x0000, /* R68  - ADC Divider */
	0x0000,x0000,     /* R228 */
	5 */
	0x00C0,     /* R66  - ADC Di9 */
	0xE000,     /* 00C0,     /* R67  - ADC Digital Voatus */
	0x0000,     / /* R68  - ADC Divider */
	0x0000,     /* R232 */
	0x00005 */
	0x00C0,     /* R66  - ADC Di0x0000,     /* R234 *00C0,     /* R67  - ADC Digital V R235 */
	0x0000,     / /* R68  - ADC Divider */
	0x0000,     /* R237 */
	0x0000*/
	0x0000,     /* R254 */
	0x00350-regm/
	0x0000,     /* R85 */
	0x0000, 5  - Interrupt Status 1 */
	0x0000,   0 regist44  - FLL Control 3 C Mute */
	0x0file splits out the tabl* R30 FLL CoEnterrupC Mute */
	0x0defaults and access
 * status   /* 2	0x0000ol */
C Mute */
	0x0isters since they ar* R30000,   B000,   cxer Control *
 * Copyright 2007, 2008 ume */
	0x0000,     /* R101 */
	ctronics PLC.
Limit Swit9ontrol 00,    0,     /* R60 FIG_RAM BIST  /* R22  -/* R88  - Left Mixer Control  R204 - LDO0  - GPIO Interrupt Status *modify i000,     /* R99  - Input Mixer Vol0800,     /* R88  - Left Mixer Control  R207 - LDOR89  - Right - Input Mixer Volee Software Foundation;   /* R98  - Input Minput Mixer Volof the  Lice,     /* R104 -34F/* R178 - - Input Mixer Vol,     /* R250 */
	0x0000,     /* R251 50/cme */
	0x02E4,     /* R107 - ROUT2 Vols */
	0x0000,     l */
	0x,     /* R62 *FIG_MODE

const u16 wm83 R109 */
	0x0000,     /* R110 */
	 /* R22  - Alarm D0000,     /* R110 */
	ume R */
	0x0000, 0000,     /* R110 */
	0800,     /* R88  - Left Mixer Control stem Co Control */
	0x0020,     /* R115 - */
	0x0000,     /* R161 */
	0x0000,   ControlTest */
	0x0000,     /* R117 */
	0lume */
	0x02E4,     /* R107 - ROUT2 Vo */
	0xE000,     /* R230 - GPIO Pin Sta R6   -   /* R98  - Input Mixer Volume L  */
	0x     /* R123 */
	0x0000,     /* R1
	0x0062,     /* R180 - DCDC1 Control - Power R126 */
	0x0000,     /* R127 */
	00800,     /* R88  - Left Mixer Control mgmt (2     /* R129 - GPIO Pin pull up Co- AIF Test */
	0x0000,     /* R117 */
	T1 Volume */
	0x00E4,     /* R106 - LOU1  - Pow* R30  - GPI    /*4  - ADC Controlume */
	0x0000,     /* R108 */
	0x0000,109 */
	0x0x0000,     /* R133 - GPIO Con0,     /* Ri/o) */
	0x0CFE,     /* R135 -   /* R126 */
	0x0000,     /* R127 */
	14 - AI ADCi/o) */
	0x0CFE,     /* R135 -0800,     /* R88  - Left Mixer Control 5ntrol */
	0x03FC,     /* R130 - GPIO Pu0,    };
