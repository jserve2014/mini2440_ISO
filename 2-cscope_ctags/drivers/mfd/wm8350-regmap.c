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
	0x0000,*
 ** wm8353-regmap.c  --  Wolfson 4icroelectronics WM8350 5 - VCC_FAULT Masksicroelect1Fnics WM8350 6 - Main Bandgap Controlicroelectronics WM8350 7 - OSCess Wolstatus o9tronics WM8350 8 - RTC Ticksince they are f the WM8350 reg9 - Security1icroele4tronics WM835020icroelectronics WM83502   Wol Tware; you can reree software; you can reMicroelectronics Microe24e GNignal overridee tables dhe GNU GeneralfileDCDC/LDOthey a published byfsonstribFrdefaCharger Ovee as /oundation;  either version 2 istemiscicens, or ion;  either version 2  WolSupply.
 */

#in(att anrdistribute  it and/or mohe GN.h>

#ifdef CONFIG_MFD_ree softEther version 230 - GPIO Pin St your
 *  option) any late31 - comparotor.
 */

#include <linux/mfd/wm8353ree software; you can r33coretate MachineFat your
 *  o12R1   - ID-regmreg verr map
distriThis35icroele8ol 1-regmap.c 46emess
 * s   - System C7ntrol 2 */
	0x0000,    8ntrol 2 */
	0x0000,    9icroelectr3nics WM83504ree softte  it and/or m4WM8350_CONFIG_MODE_0

#4ree softwar4 * wm88* R3 erms of  3  1) */
	0x000tem Control 1 */
	0x0004,R9   /*01) */
 R9 - Sy4l 2 */
0-regmap.c  -   4* wm85  1/* R111) */
11  Wolx0001 Testsince th publish5mgmt (4) */
	0x6* R3  n mgmt (4)-regma5FIG_x0003) */
	2gmt PowntrogmtR13,     /* R14 ] =0,     - Po00,     /* R14    Power mgmt (7)ree soft- Power mgmt (7)ap.cx000600,     /* R15 */
};
#endif350_HAVE_CONFIG_MODEMicro2_x0101,  ODE_3

#unegma0,   0_HAVEgmt CopyDate
#AVE_R2 0x14mgmt (4) */
	9 /* R1
const u16 wm R18 mode3_of tults[ */
{oele614faceess
 * 0   * Ceset/IDicroelectronics WM8351urs/D     /* R11 2nics WM8350larm ayviy lat* R19 SCregmap.320, 3larm Syverssince theWM8350_CO2 (1ower mgx04te/Month ** wm834  -ree software; you can 50x0000,     Hibern/* R10   /*Aronics WM8356larm Dnter  - t 2007, 2008 Wolfson Microele - Power R4mgmt (3vers8larm R15 */
	0x0(1)icroelectronics WM8359   /* R8gmt Under2Voltage I0000ruptthey 10 */
	0x0000,    3/* R29  2enser Current 1 */
	0x0000,    4/* R29  -E     /* R30  2 */
	0x0000,    5 wm83 R20      /* R30  
	0x000x0000,    6r Interrupt Status */
	  - Po0x0000,    7r Interrupt Status */
	0x1400rrupt Status */
	6  /* TCGNU onds/Minutinclude <l0000,     /* 17 - InterHo   /* Rt Sey are21t th-regmap	0x0   /* - R/MonthRTC Time42ut th-regmap9atus MasYearicroelectronics WM8350- OveA*/
	0R325   - Over CurPower tronics WM8350 ate/Month *ower 5,     /* Rther version 2     /* R11  - Undee/Month *032   - System Catus MasTim    /* R7,     /* RUp - Over C236  -/* R10 sk */r Cu   /* R30  - Over Curre5egmap.c   - Carm SecoWM8350_CONFIG_MODE_0

#26  - InterR41* R3Clocree software; you can rt Status x0000,     /* ey ar,     /* R29  R42  0,   at your
 *  option) any late26,  nse, /* R30 42  /* R3FLLrol 2 */
3000,     /* R11 3- OveMODE
/* R42  - FLL Con  /* R42  6-regmap.c y areCs */1aFF, R47 /* R46 */
	0x00* R43FFscribing th3 Over  /* R42   1 MaClot th /* R42  x0000DACess
 e L */* R42  - FLL Conk Mask Cgmt (4) */
5- FLLDAC36  - R44  - FL
	0x0000,0000,     /* R52 */
	0x004ey are1 R36  - Unde0003
	0x00xC226/* R10  rol 4 */
	0x0000,0000,     /* R52 */
	0x004.c  -000, 
	0x0000,   4-regmap.c  -Rate */
	0x0000,     /* R54  - D/* R55  /* R49 */
	0x0000,     /* R52 */
	0x00426,  * R49 */
	0x00C0, R49-regmap0000,     /* 4onics WM8354- OveCloght 2007, 2WM8350_CONFIG_MODE_0

4 */
*/- DAC 6  /* R4icroele3Over Curren 24 Over
	0x*/
	0x000x8 /* R708665-regmap.c Digital Vol665  - InterrC22l6,  ume L *36  -00C0,     /*erms of  the GNU Gener0x0000,     /   /*tem Control 1 */
	0x00rupts */2 2 Mask */
	0x,     /* */
	0ADC LR Ratey arDA sincefsony are 0000,     /* R7     /* teC Power U  - - OveontrDigital,   tal VC Con R49 */
	0x00C0, y are- IN3 Inputess
 * sR37  - Over Curren  /* ree softwarR60-regmap.000, Contr /* 2  /*/
	0x     /* R75 */
36  -trol trol */
	0x00x0000,     /* R75 */
4  - DAC Clock Control5 */
	0x2000,     /* R15 - Powerhis program is5  - SysteMu
	0x0000,     /* R74R77 26,  igital */ss
 * scroelectronics WM8356R7PowerINSidInput
	0x0040,     /* RWM8350_CONFIG_MODE_0

6ree software; you can 6erms of  */
	0x0000,  636  -ADx03l */
	0 /* R80,   s Control64  - DAC C */
	0x00C0,,     / /*ias Control */
 */
	0x0040,     /* R86.c  -0000,     /* 8 R20 	0x0040,     /* R8Volu - Lef /* R1vi000,x0000,     /* 9  - Syrol */
	0x   /* R76  -7ontrolDR89  - Right Mixer Control */77WM8350_CO30,   ess
 * 7 Overs Co    /* R1 R89  - Right Mixer Con3 InpN3ixerVolume L * - OUT3 Mixer */
	Outp36  -Mic Bia - DAC rrupt Status Mask */
	071 */
DAC Clock Control7/* R1Outer Volume */
	0x0000,     /* R97  */
	Jack Detec Statu9x0000s ContMixenterrupnti Poct 2007, 2008 Wolfson Microele7- Right Mixer Control 8- OveLef
 * er VDigital     /* R83    /* R74  - MRigh	0x0040,     /* R10  /*   /* R83 */
ree software; you can 8erms of  the GNU Gener8tem Control 1 */
	0x008  /* R98  - Input Mixe8 */
	0x2000,     /* R18t Status DC Dgmt (4) */ey ar    /Mix*/
	nce they are R13  - Power m826,   04 -atus Mask */) */
	00 - Over Currentu*/
	0x0000,     /* R7 9WM8350_CONFIG_MODE_0

9 OverOUT3 BEEP     /* R103 */Amgmt (4) */
	00,   COUT4*/
	0x0020,     /* R114 - AI ADC Contem Control 1 */
	0x009  /* R98  - Input Mixe9Digital Vol/ R103 */
	0     /* R103 */
	0     /* R9R52 *7/* R1111] = /
	0x0gmt (4) */
	08     /* R83 */ey are */
	*/
	0x0000,   R103 */
	    /* R83 */26,    - System 
	0x00000x0000,     /* R91 */
10FIG_23 */
	0x0000,          /* R58  - DAatus WM8350_CONFIG_MODE_0

10ree software; you can 10erms of  tEerrupts */
s 1 PuOUT1Lume */
	020 R103 T1 Volume */
ee S_MODR

conpull ucess
 * s R103 */3Fof tOUT2E

con - GPIO Pu70  - ADC LR Ra ContUT2 */
NFIG_MODE

ulIG_MODE
Deboun,   6  - AD114 - AI AD10rol */
	0xigitaower */
	*/
	0x0000,     /* R7 11 */
/
	0xPin pull up Co* R65 */
	0x01170  0I Formating*/
	01 - MODE
Configura */
	0x00ntroOMPin pull 60,     /* R31129 -AI9   - Po  /* R49 8  -- /
	0  /* R49ee SAIControR87 */
	0x0800,     /* R88 11 */
	0x2000,     /* R11- GPIO3  - FLL Control 11trol */
	0x03F33e */
	01Pull down ContrFFC000, f/* R134 - GPIO Configure   /* R127 */
	0x1FFF, odify itstribuxC226fson1term Wolffson  the
 *  ol */
	0x0000,     /* R112  /* R98  - Input Mixe02Sof  t   - System ask */
	0x43  /* R/* R1*/
	010/co   /* /
	0x0    0,       - System 2, he G  /*  Ala - GPIxer Volume */
	0x0000,     /* 1NFIG_M0,    R14down    /* R83 */
	0x00015up Contx0 */
- DAC Mute VoluMo0,     /* R90  /* R46 * mgmt (4) */
	* R3   - S1ontro      2007, 2008 WolBFB     /* R1520000,UX3 Rea0,    tRTC (i/o/* R29  -FFD     /* R152ee S00,     /PolaLC.
 / Typ /* R152x000AUX24 RedbacControl (2) */
	0x0000,  /* R141 - GPIO Functio0000,     /* R133 - GPIORrol */
	0etect *14
	0x04000,     F/
	0    Sel /* WM8350_CONFI7  - Over C4 Voltak *    /* R83 */
	000,     2 Rr Vo    /* 14ODE
1   /* R148 *
	0x0000erms of  th  - Alarm H141 -  AUX4    /* R83 */
	0Pull dowerrupts */
 */
429 -ias Ci00,    ,    26,     /*Intontrol */
	14ee So16file
 *  ic {
	0* wm83errupt Status */
	    /* R70  - ADC LR Ra100,     /000,     /* R154geNE Voltaatus Mask */
l down ContR37  - Over Cun Control164 - Gener3, 5,    3 Inp- Battery he  ODE

UX1NE Vol6,  rparator 10-regmap.c  0000, LINE17* R42 */
	0x0000LicePull dowic CoAUX4 00,     /* R171 */
	0x0000,   64 - UX42 - Current Sink  - SysteB0,    of tUSB,     /* R/
	0x0x000CSA Flash cull down  verLINE Mask */
	0x01 - /* R30  Sink Dri    BdistriBATT0,     /* R175 - CSB Flash control */
	0he GChip Temp* R175 - CSB Flash control */
	6  /* R169 - Battery Cha0,     /* R90 /* R58  - mgmt (4) */
8Volume L *16   /*neric cregmap.c  --	0x7000,     /* R152 - AUX1 65file
	0x1at{
	0x0c ComSleep opur
 stor */
	025 R29  */
	0xA00F,     /Mask */
	0x0roni  /* -chof trol */x0000, 82	0x00F,     /*1 - 3 Inpiser6 ver   /* R183 - DCDC2 tem ContA0 - AUX1 Rc Co6 /* R */
	0son  Lice* R66  - ADC Dig B0 Digital Vo16he G	0x7000,     /* R158defaCDC1 Low Power */
	0x007FIG_  /* R187 - DCDC3 TimeouControl */
	0x0000,    UT3 Mixer4DC3 Tironioftw/* R1
	0x0000h contro000, 187 - DCDC3 Ti0000,    000, ent Sink D	0x7000,     /* R152 - AUX1 729 -R19* R4,    arateoutBROUT2 Vol000,     /* 2 ee SCSBC4 Low   /* R ROUT2 Vol R193 - DCDC5 of tx0000,  Frequ* R1d    /* R83R15defaUSB V7 verx000 Active op,   ,     /* R/* R51  - DA170E Compar81 - D/
	0x00imeouts *25,     /* R51he G/
	0x0check,     /* R15croelectr- Right Mixe8t (6ower100E7 - DCDC3 Ti	0x143 */
	,   
	0x0000,10000oou*/
	0x/* Ritch Pull down ODE
c Comp/
	0/
	0x0ic Compar8 Pull down C0x0000, 2     /* R200 - LControl1x0000, R108      /* R42 20] = LD2 Pull down Cont  /* R98  00,         /*0x00C0,       /* R200 - LCPull down Cont   /* 6	0x3FFF,11 - LDO2 PowTimeouts */
	97 - DCD R9 /
	0x0CDC2LDO1 L0Ell down Conthe G */
	0 /* R204 -2 Co  /* Pull down 9
	0x001C4    /* R83 */
	0x00020isteLDO3 61 */
	0000Power-cFuncti	0x001	0x0000,    901C,    5  /* R203 - LDO2 Control */
	090x0000, 40,   x0000,     /* er */
	0x000 Right Mixe29wm835 /* R58/* R1x010Power-c00,     /*/
	0xA00F,  * R2/* R2160000,     /* R210 1   /* R */
	0 Control601C,     /* R211 0x00* wm835defaul00,     /*    /* R83 *9he GLimit Sw1 Ti  /* R203 - LDO2 1	0x0000,   * R31 LDO   /* R210 -2 CoR13  - Power mgReadbT Flas */
	0x0000,    1CR22  - SysteODE
     
	0x001C,     /* Ro7     /* R223   /re F
	0x001Cx000re F/* R2   /* R22329 -225 -s */
	2000,     /* R11    /* 23ee S* R22x001C,     /* R211  Digital Vo2 - LDLDO0,     /* 
	0x001s */
	0x0000,  */
	0x00E6 */
	0x0000,     /* R227 */
	0strib    00,     /* R224 */
*/
	0x0000,  /* R1DO00,     /* R23 /R13  - Power mg1 BIST 1x0000,     /* R210 -/* R227 */
	our
  /* Rull down Cont9ontrol */
	0x00croeicroelectronics WM8350 erms of  the GNU Generamgmt (2) */
	0x0000,     Time splits ou	0x0 publishede /* R50  - e,    x000MODE acxer Volume */
	0x0000,     /* em Contsx0303,     /* R7ration)l*/
	.1 */
	0Copyri/* R63 */
	008R239 */
	0x0000,     /* R2P
	0x0istribu* R80  - Left ion Select 3 */
	0x0003,r    /* R143 - GPIO Funcmion Select 4 */
	0x0000      /* R144 - Digitiseal29 -blic   /**/

#include <linux/mfd/wm8350_FAULT MR195 at your
 *  option) any laterolfson 
	0x0000, orNFIG_MFD_stribCDC1 T) any la*/
	 any la/* R/350_nclude <linux/mfd/fson      eWM8350_HAVE_CONFIG_MODEMicroe_CONFIG_Mate/0onth AVE_   - Re    eset/ID */
	TC Yeinn Microe   - ID */
	0x0000 Alarm Second2 */
utes0*/
	0x0000,    
	0x0700,  R227 */
	0x- Ry */
	0- LDO2 n Select 2 */
	* R3   - Sys000,     /* R210 0x0000,   2* R137 */
	  - FLL Contol 2 */
	0x0000,    0,     /* Re Control */
	0x00000x0000,     /* R227 */3  - FLL Control 20000,     /* R133 - GPI      /* R100,   09 - LDO4 C
	0x0040,     /* R886 - DCDC3 Con9   - Power mR28  - Under V(3) */
	0x2000,     /0x0000,    R13  - 2* R12  - Power mgmt (5s 1 MaR28  - Under3/* R167 - Generic compar*/
	0x0000,     Power mgx97 - DCDC00,     /* R15 */
	0x0(5* R13  - Power mgmt (6)Power mgmt (7)
	0x001C4 */
	0xA00F,     /**/
	0x0000,     7* R13  - Power mgmt (6)r Volume */
	0x0000,  1 */
	 Timeec  - Interreimeouts * mgmt (4) */
	rupt Copy /* R35 mparhemgmt (2) *rs/Day * for* R24Powfig 0  - used must b
	0xmpiled in buect 4due to5 */
impact on kernel sizeIG_M R80ossibler mgdisblis
O st#if0x0000, 0x0000,     /* R20  -Cwarnity NoR103 */
	0x00E4,   s suppor6 Co- ser mgmat leas
	0xe ofDO2 L/* R2000, mgmt (6) */
	R19  - R/n Power */from5 */
board d  /* .trol */
	0xntroA    / m* R2.- FLL 00,   struct  - In0_reg_a0,    ours/Day */io_map36 */
	0/*  read*
 *write volatil0,    { 0xF0,   ,     */
	0xIn },/
	0 /* R35  ours/Day */O1 C7CInterrystem Cx7    /* R75*00,   k */
	0FC,  07	0x000nth */ - FLpt Statu0000,  OM/
	0x00x3F   - SyBE3B00, 3 */
	0n00, pt Statu000,     /* R210    /*000, FC,  EF7*/
	0 Over Cu8 /* R98  -rnate */
	0x8A00, 000, 10  O1 CO2 CoGPI- Over Cur /* R98  -bernate */
	0x8A00,  */
	0xFC,  B208   - Over Cm	0x0000,  imeouts */     /* R210 rupt S34 */
	LL ContInerrupt sk */
	0x000,  O1 CE53ver C0FFC, 8      /* R75R */
	0x0x0000,    6,     r mgmFF300,  3 R20 CInterrupt Stt Stbernate */
	0x8A00, Main Ba8 R3tem lume LInterrupt Sta00,     /* R112tatus MasO1 CoD3Cer CPull dowl 2 */
	0x000nterrupt 000,     /* R75*/
	0Flume L08,     l 2 */
	0x000*/
	0arato	0x0000or
	0x00FLL F* R38 t R190 -l 2 */
	0x0000,     /* R0FFC, tem Inyontrol /
	0xCernate l 2 */
	0x00045 - Digitiserer mgmt (4ontrol 1 */
	0x00/* R4R36  - UndPower U */
	FFLL Con- DAC Ml 2 */
	0x000* R137 */
	/* R37  - Over Curr mgm7
	0x0000,     l 2 */
	0x0000 R47 */
	0x000000,    192 08
	0x00     /*2l 2 */
	0x000- DAC Control */    /* Rwer mgm* R200,FLL Control 1 */
	226,     /*FLL Cont00,     /* R61 */
	0 Mask */
	0x0rnate */
	0x8A00,3rupt      /* 0FFC,       /* 000,   Mask */
	0x3  - DAC LR Rate */
	0x uts */
	0x050-regmap.C MCONFtus MaLL Control 1 */
R36  - Und    /* R42  EAtus Mask     /* R5paratoC226,     /* R Over Ctal VB0,     /* - DA0x0000,    2 00400,     /* Rme L */
1 -  Mute E R38  - GP6   /*    larm Ho00,     /* Rme R */
	0xss
 /* R57	0x0000,    66  - ADC Digit,     /* RR53  - DAC LR */
	*/
	0x0000,   /* R66  - ADC Digital00,ask */Up,     /* R61 */
	0,   Divi  /* R29  - Ov0x0000,    61 * /* R60000,     /* R55 */
	0x0000,       FLL C  /* R48  - DA - ADC Contro /* R7 */
	0x4000,     /* R58  - DAAC M,       - ADC Digital Volume R *al Volume L *  /* R49 */
	0x00C0000,     */
	00,     /* R91 */

	 - Right Mixer Control *- ADC LR Rate0000,     /*er mgmt (4 /* R98  - A Controa Digital Volx0000,     0000,     /*0,     /* R52 77  - J R79 */
	0x0040,/* R122 */
ias CoFC,  rupt ADVolume *0x0000,     /*ute Volume Power1  - /* R7  /* R6    /* R83 */* R51  -     /* R63 */
	 */
	0x0000, 70000,     /*p options */
	0x005,     /* R179 - Power-
	0x0000,     8  - DAC5     /* R85 */
	02000,     /* 
	0x080Powetus Mask    /*     /* 4     /* R/* R90 */
	104  /*  - LDO2  Right Mixertus Mask R5 R20 * R85 */
,     /* R118 */
	0x0000, */
	0xt (1) */
	0x0000,  6 Sid /* R118 */
	0x0er mgmt (4)  /* R49 C9Over C000,    ntrol 1 */
	  /* R71 */
	0R66  - ADC D    /*    /* R1* R97rnate */
	0x ADC Digit Right Mixer ConFC,     R97 utut Mixer Lef /* R10     /* R52    /*ger Co29  - Interru */
	0x0000x0000, umx0000,     /* R62 R678  - InpuR42  - FLL Co /* R10    /*Right Input Volume */Contr  /3te */
	/* R10  - */
	0x000	0x0000,     /* R71 */
00,     /* R48  - DAC Control */* R12  /* R103 */
E */
	0x800010fileROUT6 */
	0x000 */
	0x0x0000,E*/
	0x00E4,    - Syste,     /* R14me */
	0x00E4,     /* R106 - LOUT     /0,   1Over Cu/* R97 ntrol 1 */
	0x2 */
	0x     /* R89  - Right  */
	0xA00F,     /100,     /* R10x0000, t Righ/
	0x0000,     /*00,     /* R48  - DAC Control */ours/Day   /*90000,   R112 - AI Formatin Mixer /
	0x0000,    0x0000Rateernate */
	0x8(4) */
	0x- r Volume R *l */
	0x0000C51  - 0x00eouts */
gmt (4) */
	0ate */
	0xx0000,     /* R118 */
	0x00001    /* R93  - OUE4,     /* R10defaLOU*/
	0x0  /** R80 l */
	0x001R218 AIF /*  Cont    /* R103 * */
	0 */
	0x0000,    123 */
	0x0
	0x0000,s Cont    /* R10/* R1    /* 0000,  7 DAC LR Rate 80x0000,     /* R2* R112eouts */
	0- RTC Year *20C Sleep  R93  - OUT4 Mixer Con12roniMODE
   /*Iume * Control */
	0x03FC,     /* 0-regmap.c
	0x00,    st *Gut Left Mixer * R95 */
	0x0000,   ll up Control */
	0x03FC,     /* ate */
	0x00,     /* R112 - AI Formati0000,     /* R5   - */
	0x0000,     /* R000,     /* Rype */
	0x0  - Power mgmt (5)00,     /* Rume */
	0x0000,     03FC,     /*  - Power mgmt (5)tem ll up Control */
	0x03FC,     /* 0 ADC LR R,     /* R113 - A118 */
	0x009Power*/
	0 R101 ss
 *0,     /* R48  - DAC Control */x0,     /* R70000,   4CDC2GControl (2) * 1 */
	0r Volume */
	0x/
	0x* R14    /* FunControl (2) *e L */
	0x000     /* R124 */     Main/*rator 1 tion Select 3 * R97 ut0x0000,  /* R144  GPIO Function Select 2 */
	0x133l */
	0x03FF10x000 */
	047tion Select 3 0x0040,     /* R8eouts */    * R64 l */
	0xtion Select 3 r Control ight Input140 -3000,     //
	0x1DAC LR Rate 00,     /* R10*/
	0x0000, GPIO Function Select 2 */
	0x133  /* R140 E1F0x000 /* R16RC LR Rate */ R100x0000,     /* R227 */1015   /* R1668 - Battery Ch142 ing dback 000,  GPI     /* R124e */
	0x0000,     /* R129 - GPIO106 - L1400,    LDO4INE6,     /*68 - Bat0-regmap.c       /* R158 - BATT Voltage/
	0x0000,     /* R121 */
	0x006 - LOUC,     /68 - Battery ChargerRTC Year */0/
	0x0000,     /* R121 */
	0x0000,    ack 8 *DC Co8O Func,     Coal Volume L */R1  /* R140 00,     /* R112	0x0000 Volta */
	0xal Volume L */R120 */
x0000,     /* R103 *e */
	0x0000,     /* R129 - GPIOCDC2,    C Year */defaeneric compa6,     t 1 */
	0790,     00,   A   /* R63 */
	0x0000,     /* R118 */
	0x0eneriErol */n,     /   /* R63 */
IO Function Select 1 */
	1R155x0000,     /* R167 - Generic cop Temp Readback */
	0x0000,     /* R165 */
	0
	0x7EE  /* R97CDC2Cu7  - System */
	0x0000,   /* R49 */
	0x00C0, R*/
	00rent Siver B */control */Aounce */
	0x0000,     /* R129 - GPIOtage Reonds/Minutes 5nk Driver A x03FCback */
	0x0000,     /122k Drive* R97O - DCDC Sink Driver 6 - DC164 - Generic comp9 - GPI/
	0x0 R63     /* R119
	0x0000, tions *r Volume */
	0x0000, al Volume L */* R51  - DACCDC Sleeoptionw Power */
	0x001eck Generic com117 */
	0x000- CSA Flash 
 * 0,  6,     u03, */
*/
	0x0000,     /* Rl 2 */
	0x0000optionGPI    000,     /* R161 */
	0x0000,l 2 */
	0x0000ee SR     194 */
	0x008149 oftwTimeouts */
	0x0eneric c61 */odeR149 rent Sink Dri7 AUX4 R38 - DCD 2 */
	0x0000 vers/Min DCDC3 TimeouDC Control *8mparntrol    /* R214  /* R    /* R11  /* R183 - D
 * oftw3 L,     /*     0006,     /* R188 - DCDC3 0100,   	0x00C8 - DCDRate *3B     /* R181     0x00* R12  - FF   /*FFoundat
	0x040  /* 2 Contro/* R1
conk */LC.
0000,     /F3,     /58 - DCDC5 Control *0,     - DAC Cont    9 - DC8* R52 */9fileD5 Control */es */
	0x0100,  
	0x000     /*00,   /* R214x0000,     /* R000,     87 */
	0x0814 */
	03  /* R    /*  mgmt (3) */
	of t*/
	Flash mgmt (3) */
	0x0000,     /* ontrol */ric c0x000tch Control */
	0x0006,     /*
	0x0000,     /* 6  - Interfa     /* R1
	0x040 /* R209 - LDO4 Cch Control */
	0on SeleR224 */
	0x0000,     /* R,     /*  Timeouts */
	0x001C2 Control */
	00000ion Sel000,     /* R214 */
	C,     /* R21x0000,     /* R161 */
	0x0000, 5roniCh0x0000, DO4 Timr mgmt (3) */
	0x2000,       /* R19  - RTC Year */ - Generic Comp0x0000, O4 Tim Control */
	0x040
	0x001F,  R200 - LD* R138 */
	03,  3  /* R141 -w Power */
	  /* R49 */
     /* R2Power */
	0x001B,     /**/
	0xA00F,     /*R164 - Generic c/
	0x0000,     /* R212 */
	0x000trol */
	0x0000,  - SystUSB VoltR215 - VCC Control */
	0x00file sp    /* R156 - USB VoltR202 - LDO1 Low Power */
	0x0006,     /     /* /* R2ion Sex0000,    238 */* R27 
	0x0000,     /* R156 R216 - Main Bandgap Control */
	0l */
A     000,     /* R161 */
	0 R216 - Main Bandgap Control */
	0- BATT Voltageol */
	0x0000,     7 */
	0x0000,     /* R238 */* R27  R2/
	0x    /imeouts */
	0x001C,     /* R  /* R90000,     /* R227 */
	  /* R49 *R201 - LDO1 Timeouts */
	0x001C,     /*     /*/* R93  - OUT4 Mixer Con10  - Sypx0000,     /* R161 */
	0x0000,       /* R37  - Ov3 - Generic Compaower */
	0x001B,     0x0000,  233 */
	0x0000,     /* R234 */
	0x0ar */149 *neric c0x0000,    Con /* R20 CSBt Si    /* R234 */
	0x00x0000,  pe */R167 - Generic coFC,  R5   /*Dx0000, A- GPIO Debounce */
6 - Generic compa6,     /* R1IO Func24 /* R5* R234 */
	0x0000,    /* R167 - Generic com3 LDO1 Low Power */
	0x0006,     /*/
	0x0000,     /* R121 */
	0x0000,    ,     R126 */
	0x0000,  ,     R164 - GenerR201 - LDO1 Timeouts */
	0x001C,     /** R164 - Ge79 - Power-check comparator *	DC/LDO stat0x0000,     /* R10x0000,    /
	0x00    /*x0400,R185 */
1  - 0C0,    onds/Minutes ,     /* R   - Reset/ID */
	021000,     /* R - DCDC Active optionsfine WM8350_HAVE_CONFIG_MODE

consMinutes ent SiB Flash fine WM8350_HAVE_CONFIG_MODE

conseneric comparat- Curreol */
ction S Digital Volume R *urs/Day */
	0x0101,  75 - CSB Fwer mgmt (3ol */
	0x0000,     /* R014,     /* R4   - Syfile /* Rrol 2 */
	0x0000,     /* R5   - Sy     /* R4   - Syuts */
ware Frol 2 */
	0x0000,     /* R5   - Sy2R156 - USB V7iste  /*  DCDtus Mask */
	0x0040, 5 /* R58  - D26,        /* R13  - Power mgmt (6) */
	R204 - LDO2 Timeouts */
	0x001C,     /*/
	0x0000,    000,    R164 - Generic com- DCDC1 Control */ Main B   /* R200 33 */
	0x0000,     /* R234 */
	0x- Right Mixer Control *     /*1,     /* R18  - RTC Date/Month */0,     /* R183 - DCDC2 trol L1,     /* R18  - RTC Date/Month */- DCDC2 Timeouts */
	0x0000, 1,     /* R18  - RTC Date/Month */x0000,
	0x0/
	0x   /* R214 */
194 *w Power */
	0xC LR Rate */1- Left Mixer87 - DCDC3 Timeou- DCDCume */
	0x00
	0x0000, wm835 - Inl - Sys0x0000,9   - Po3rm Seconds/Mi45  - FLL ContFLL Contro2

#u3 InpR189 - DCDC4Control */
	0x0400
	0x0mt (2) */
	0x0000,     /* R10  -3DCDC1 Low 90
	0x00FLL Cont/
	0x004m Inte
	0x0006,     /*rm Seconds/Min*/
	087 */
	l 1 */
	 2 */
	0x0000,   /* R1 */
	0x0000,     /* R37  - Over Currenr mgmt (1) */
   /* 50x0000,     /* R227 */
ntrol 1 */
	bernate */
	0x8A00, 8 Low Power */
	0x0ay */
	0xl 4 */
	0x0000,     /* R46 */
	0x000,     /* 6 ConCDC0,      /* R1 /* R7 - FLL Co  /* R15 */
	0x00009     /* 6rm Seconds/Mi     /FLL ContMask */
	0xR36  - Un99   - Po6	0x0000,     /* RR */
1USB Vox0000,    */
	0x0320,ck Control */
	0	0x0000226 *         4FLL Con/
	0x000 R34  - Mask*/
	0x001B,     /*00,  FC,       ContControl */
	0x009  R204 -     /* R210 -mpar */
	
	0x004936  - Uontrol */0,   ge Interrupt statu,     * R3635 */
	O1 Timeou    /* R38  -0000,     /* DC/LDO st     /* R59  - DAC M    /* R57 */
urrent Interrupt000,   nterrupt Status 1 */
	0x0000,    00,     /* Rtal Volume R *//* R39  - Comp Control */
	0x0000,  s Mask */
	0x0040,     /* R40  - C0x0000,     /* R227 */
000,     /* R59  - DAC M - GPIO  -000,     /* R212 */
	0x0/* R191 - DCDC     /* R68  - ADC  ntrol */
	0x0000,     s Mask */
	0x0040,     /* R40  -  mgmt (3) */
	0x2000,    */
	0x0000,     /* R45  - FLL Co0,     /* R212 */
	0x00000x0000,     /* R45  - FLL */
	0x00,     /* R25  - I5  - R254 */
	0x0000ct */
	0x0000,     */
	0x9000164 - Genericnterrupt Status 1 */
	0x0000,    0,     /* R4     /* R42  - FLL Control 1plits out the - DCDC6 L53  - DAC LR Rate */
	0x0000,     lts and access
 * s */
	*/
	0x0000,/
	0x0040,     /* R80 1isters 0x0000,     /* 90x0000,     /* R98 x1000,  ,     /Control */FFD0x0000, - ADC /
	0x1000, 7   /* R36* R37  - Ov
	0x0000,      400,    /* R80  -,     /* R9*R235 */,     /* R30  s Mask */
	0x0040,     /* R40  -     /* R227 */
	     /*L ContR RateR6x0000Aer Contro/
	0x /* R228 */
	0x0000,   Volume L *0,     /* R99  - Input ix0040,  
	0x8000,  st - Input Mixer Vol */
	0x0	0x0000, 230 - GP  /* R228 */
	ixer  Dividr Con/
	0x0000,     /*     /* R227 */
	
	0x00     /* R70  - ADC LR Rate */
	0x000,     Ex0025,     /* 303,     /* R72  - Input Control/* R41  - Cl R102 */
	0  - IN3 Input Control */
	0x0000,0x0000,    0-regmap.c       /* R70  - ADC LR Rate */
	0x033 */
	0x0000,    4xer 303,     /* R72  - Input Contro    r Volume */
	0x0000*/
	0x0000,     /* R78  - Anti Pop Control */  /* R49 */
nterrupt Status 1 */
	0x0000,   
	0x0000 Rate */
	0x0000,  arm Hours/Day * Power Up0,     /* R75 */
4,     /* R10* R245 l 4 */
	0x0000,     0,     /* R60   /*00,     /* R242 */
	FC,   
	0x00E    /* ight Mixer Con3 */
	0x0000,     /* R244 */
	  - F2R251 */
me */ight Mixer Con45 */
	0x0000,     /     /Pop CoBx0000, c* R141 - x000     /* R247 */
	0x0000, 0x0000,     /* R231 */
	0x0000, 0x0000,     /*,     /* R9000,     
	0x00nput Mixer 0 FIG_RAMR235 *30 - GP 226 */8x00000,     /* RA */
	0x01 */
	0x00*/
	0x0000,     ntrol 1 */
	/* R252 */
	0x0000,    R20  */
	0x7000,   0x000r mgmt (4) - Input Mixer Volume L */
	 LDO4 T) */
	0x04FE,* R100 - Input Mi
#ifdef CONFIG_MFD_WM8350*/
	0x0000,     /* */
	0x7000,   undef WM8350 R231 */
	0xt St4008, 170,  * R100 - Input Mi
	0x0000,    - AI Formating 0000,   1 ault/* R103 */2E4,     /* R10isteLOUT2 2 *0,     /* R30  - G Power U - ADC Contro*     /* R3   - System Co /* R164 - Generic comparato7 - DC00,     /*terrupt   /* R82,     /* R112  /* R151 */Jack DeA00,     /* R112 - AI F  /* R64  - A	0x0000,     /* R101 */
	ol */
	- GPIO Function Select 1 */
	,   -R208 - LDO3 Low Power */
	0x001B,     Volume L/* Rours/Day */
	0x0101,       /*   /* R103 */00,     /* R108 */
	0x0000,	0x0000,     /* R76 R2NFIG_MODE

cons   /09 */
	*/
	0x0000,     /* R10148 */
	0x- LDO2 Low Powe177 - DCDC Active options R204 - LDO2 Timeouts */
	0x001C,     /
	0x0000    x0000,     /* R176 - DC */
	0xAI ADC Control */
	0x0020,     /* R115 ,     / */
	0x03FC,     /* 0,     /* R1313 */
	0x0  /* R118 */
	0x0000,     /* R* R248 */
	0x0000,     /* R249 */
	0x00 */
	0x0ume */
	0x000,    66  - ADC Digit /* R93  - OUT4 Mixer Conric CMixer Cont* R164 - Getes */
	0x0100,  x0000,  WM8350_HAVE_CONPower */
	0xCF   /* R200 -316 -x0000,    /* R128 - GPIO Debounce */
	1R130 - GPIPolarity / Type */
	0x0000,   AI ADC Control */
	0x0020,     /* R115 5ll down Control    /* R211	0x0000,      Volum};
