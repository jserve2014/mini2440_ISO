/**
@verbatim

Copyright (C) 2004,2005  ADDI-DATA GmbH for the source code of this module.

	ADDI-DATA GmbH
	Dieselstrasse 3
	D-77833 Ottersweier
	Tel: +19(0)7223/9493-0
	Fax: +49(0)7223/9493-92
	http://www.addi-data-com
	info@addi-data.com

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

You shoud also find the complete GPL in the COPYING file accompanying this source code.

@endverbatim
*/
/*
  +-----------------------------------------------------------------------+
  | (C) ADDI-DATA GmbH          Dieselstraße 3       D-77833 Ottersweier  |
  +-----------------------------------------------------------------------+
  | Tel : +49 (0) 7223/9493-0     | email    : info@addi-data.com         |
  | Fax : +49 (0) 7223/9493-92    | Internet : http://www.addi-data.com   |
  +-----------------------------------------------------------------------+
  | Project     : API APCI1710    | Compiler : gcc                        |
  | Module name : INC_CPT.C       | Version  : 2.96                       |
  +-------------------------------+---------------------------------------+
  | Project manager: Eric Stolz   | Date     :  02/12/2002                |
  +-----------------------------------------------------------------------+
  | Description :   APCI-1710 incremental counter module                  |
  |                                                                       |
  |                                                                       |
  +-----------------------------------------------------------------------+
  |                             UPDATES                                   |
  +-----------------------------------------------------------------------+
  |   Date   |   Author  |          Description of updates                |
  +----------+-----------+------------------------------------------------+
  |          |           |                                                |
  |----------|-----------|------------------------------------------------|
  | 08/05/00 | Guinot C  | - 0400/0228 All Function in RING 0             |
  |          |           |   available                                    |
  +-----------------------------------------------------------------------+
  | 29/06/01 | Guinot C. | - 1100/0231 -> 0701/0232                       |
  |          |           | See i_APCI1710_DisableFrequencyMeasurement     |
  +-----------------------------------------------------------------------+
*/

/*
+----------------------------------------------------------------------------+
|                               Included files                               |
+----------------------------------------------------------------------------+
*/

#include "APCI1710_INCCPT.h"

/*
+----------------------------------------------------------------------------+
| int	i_APCI1710_InsnConfigINCCPT(struct comedi_device *dev,struct comedi_subdevice *s,
struct comedi_insn *insn,unsigned int *data)

+----------------------------------------------------------------------------+
| Task              : Configuration function for INC_CPT                             |
+----------------------------------------------------------------------------+
| Input Parameters  :														 |
+----------------------------------------------------------------------------+
| Output Parameters : *data
+----------------------------------------------------------------------------+
| Return Value      :                 |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_InsnConfigINCCPT(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_ConfigType;
	int i_ReturnValue = 0;
	ui_ConfigType = CR_CHAN(insn->chanspec);

	printk("\nINC_CPT");

	devpriv->tsk_Current = current;	/*  Save the current process task structure */
	switch (ui_ConfigType) {
	case APCI1710_INCCPT_INITCOUNTER:
		i_ReturnValue = i_APCI1710_InitCounter(dev,
			CR_AREF(insn->chanspec),
			(unsigned char) data[0],
			(unsigned char) data[1],
			(unsigned char) data[2], (unsigned char) data[3], (unsigned char) data[4]);
		break;

	case APCI1710_INCCPT_COUNTERAUTOTEST:
		i_ReturnValue = i_APCI1710_CounterAutoTest(dev,
			(unsigned char *) &data[0]);
		break;

	case APCI1710_INCCPT_INITINDEX:
		i_ReturnValue = i_APCI1710_InitIndex(dev,
			CR_AREF(insn->chanspec),
			(unsigned char) data[0],
			(unsigned char) data[1], (unsigned char) data[2], (unsigned char) data[3]);
		break;

	case APCI1710_INCCPT_INITREFERENCE:
		i_ReturnValue = i_APCI1710_InitReference(dev,
			CR_AREF(insn->chanspec), (unsigned char) data[0]);
		break;

	case APCI1710_INCCPT_INITEXTERNALSTROBE:
		i_ReturnValue = i_APCI1710_InitExternalStrobe(dev,
			CR_AREF(insn->chanspec),
			(unsigned char) data[0], (unsigned char) data[1]);
		break;

	case APCI1710_INCCPT_INITCOMPARELOGIC:
		i_ReturnValue = i_APCI1710_InitCompareLogic(dev,
			CR_AREF(insn->chanspec), (unsigned int) data[0]);
		break;

	case APCI1710_INCCPT_INITFREQUENCYMEASUREMENT:
		i_ReturnValue = i_APCI1710_InitFrequencyMeasurement(dev,
			CR_AREF(insn->chanspec),
			(unsigned char) data[0],
			(unsigned char) data[1], (unsigned int) data[2], (unsigned int *) &data[0]);
		break;

	default:
		printk("Insn Config : Config Parameter Wrong\n");

	}

	if (i_ReturnValue >= 0)
		i_ReturnValue = insn->n;
	return i_ReturnValue;
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_InitCounter                           |
|                               (unsigned char_          b_BoardHandle,               |
|                                unsigned char_          b_ModulNbr,                  |
|                                unsigned char_          b_CounterRange,              |
|                                unsigned char_          b_FirstCounterModus,         |
|                                unsigned char_          b_FirstCounterOption,        |
|                                unsigned char_          b_SecondCounterModus,        |
|                                unsigned char_          b_SecondCounterOption)       |
+----------------------------------------------------------------------------+
| Task              : Configure the counter operating mode from selected     |
|                     module (b_ModulNbr). You must calling this function be |
|                     for you call any other function witch access of        |
|                     counters.                                              |
|                                                                            |
|                          Counter range                                     |
|                          -------------                                     |
| +------------------------------------+-----------------------------------+ |
| | Parameter       Passed value       |        Description                | |
| |------------------------------------+-----------------------------------| |
| |b_ModulNbr   APCI1710_16BIT_COUNTER |  The module is configured for     | |
| |                                    |  two 16-bit counter.              | |
| |                                    |  - b_FirstCounterModus and        | |
| |                                    |    b_FirstCounterOption           | |
| |                                    |    configure the first 16 bit     | |
| |                                    |    counter.                       | |
| |                                    |  - b_SecondCounterModus and       | |
| |                                    |    b_SecondCounterOption          | |
| |                                    |    configure the second 16 bit    | |
| |                                    |    counter.                       | |
| |------------------------------------+-----------------------------------| |
| |b_ModulNbr   APCI1710_32BIT_COUNTER |  The module is configured for one | |
| |                                    |  32-bit counter.                  | |
| |                                    |  - b_FirstCounterModus and        | |
| |                                    |    b_FirstCounterOption           | |
| |                                    |    configure the 32 bit counter.  | |
| |                                    |  - b_SecondCounterModus and       | |
| |                                    |    b_SecondCounterOption          | |
| |                                    |    are not used and have no       | |
| |                                    |    importance.                    | |
| +------------------------------------+-----------------------------------+ |
|                                                                            |
|                      Counter operating mode                                |
|                      ----------------------                                |
|                                                                            |
| +--------------------+-------------------------+-------------------------+ |
| |    Parameter       |     Passed value        |    Description          | |
| |--------------------+-------------------------+-------------------------| |
| |b_FirstCounterModus | APCI1710_QUADRUPLE_MODE | In the quadruple mode,  | |
| |       or           |                         | the edge analysis       | |
| |b_SecondCounterModus|                         | circuit generates a     | |
| |                    |                         | counting pulse from     | |
| |                    |                         | each edge of 2 signals  | |
| |                    |                         | which are phase shifted | |
| |                    |                         | in relation to each     | |
| |                    |                         | other.                  | |
| |--------------------+-------------------------+-------------------------| |
| |b_FirstCounterModus |   APCI1710_DOUBLE_MODE  | Functions in the same   | |
| |       or           |                         | way as the quadruple    | |
| |b_SecondCounterModus|                         | mode, except that only  | |
| |                    |                         | two of the four edges   | |
| |                    |                         | are analysed per        | |
| |                    |                         | period                  | |
| |--------------------+-------------------------+-------------------------| |
| |b_FirstCounterModus |   APCI1710_SIMPLE_MODE  | Functions in the same   | |
| |       or           |                         | way as the quadruple    | |
| |b_SecondCounterModus|                         | mode, except that only  | |
| |                    |                         | one of the four edges   | |
| |                    |                         | is analysed per         | |
| |                    |                         | period.                 | |
| |--------------------+-------------------------+-------------------------| |
| |b_FirstCounterModus |   APCI1710_DIRECT_MODE  | In the direct mode the  | |
| |       or           |                         | both edge analysis      | |
| |b_SecondCounterModus|                         | circuits are inactive.  | |
| |                    |                         | The inputs A, B in the  | |
| |                    |                         | 32-bit mode or A, B and | |
| |                    |                         | C, D in the 16-bit mode | |
| |                    |                         | represent, each, one    | |
| |                    |                         | clock pulse gate circuit| |
| |                    |                         | There by frequency and  | |
| |                    |                         | pulse duration          | |
| |                    |                         | measurements can be     | |
| |                    |                         | performed.              | |
| +--------------------+-------------------------+-------------------------+ |
|                                                                            |
|                                                                            |
|       IMPORTANT!                                                           |
|       If you have configured the module for two 16-bit counter, a mixed    |
|       mode with a counter in quadruple/double/single mode                  |
|       and the other counter in direct mode is not possible!                |
|                                                                            |
|                                                                            |
|         Counter operating option for quadruple/double/simple mode          |
|         ---------------------------------------------------------          |
|                                                                            |
| +----------------------+-------------------------+------------------------+|
| |       Parameter      |     Passed value        |  Description           ||
| |----------------------+-------------------------+------------------------||
| |b_FirstCounterOption  | APCI1710_HYSTERESIS_ON  | In both edge analysis  ||
| |        or            |                         | circuits is available  ||
| |b_SecondCounterOption |                         | one hysteresis circuit.||
| |                      |                         | It suppresses each     ||
| |                      |                         | time the first counting||
| |                      |                         | pulse after a change   ||
| |                      |                         | of rotation.           ||
| |----------------------+-------------------------+------------------------||
| |b_FirstCounterOption  | APCI1710_HYSTERESIS_OFF | The first counting     ||
| |       or             |                         | pulse is not suppress  ||
| |b_SecondCounterOption |                         | after a change of      ||
| |                      |                         | rotation.              ||
| +----------------------+-------------------------+------------------------+|
|                                                                            |
|                                                                            |
|       IMPORTANT!                                                           |
|       This option are only avaible if you have selected the direct mode.   |
|                                                                            |
|                                                                            |
|               Counter operating option for direct mode                     |
|               ----------------------------------------                     |
|                                                                            |
| +----------------------+--------------------+----------------------------+ |
| |      Parameter       |     Passed value   |       Description          | |
| |----------------------+--------------------+----------------------------| |
| |b_FirstCounterOption  | APCI1710_INCREMENT | The counter increment for  | |
| |       or             |                    | each counting pulse        | |
| |b_SecondCounterOption |                    |                            | |
| |----------------------+--------------------+----------------------------| |
| |b_FirstCounterOption  | APCI1710_DECREMENT | The counter decrement for  | |
| |       or             |                    | each counting pulse        | |
| |b_SecondCounterOption |                    |                            | |
| +----------------------+--------------------+----------------------------+ |
|                                                                            |
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_ b_BoardHandle         : Handle of board APCI-1710|
|                     unsigned char_ b_ModulNbr            : Module number to         |
|                                                   configure (0 to 3)       |
|                     unsigned char_ b_CounterRange        : Selection form counter   |
|                                                   range.                   |
|                     unsigned char_ b_FirstCounterModus   : First counter operating  |
|                                                   mode.                    |
|                     unsigned char_ b_FirstCounterOption  : First counter  option.   |
|                     unsigned char_ b_SecondCounterModus  : Second counter operating |
|                                                   mode.                    |
|                     unsigned char_ b_SecondCounterOption : Second counter  option.  |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      : 0: No error                                            |
|                    -1: The handle parameter of the board is wrong          |
|                    -2: The module is not a counter module                  |
|                    -3: The selected counter range is wrong.                |
|                    -4: The selected first counter operating mode is wrong. |
|                    -5: The selected first counter operating option is wrong|
|                    -6: The selected second counter operating mode is wrong.|
|                    -7: The selected second counter operating option is     |
|                        wrong.                                              |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_InitCounter(struct comedi_device *dev,
	unsigned char b_ModulNbr,
	unsigned char b_CounterRange,
	unsigned char b_FirstCounterModus,
	unsigned char b_FirstCounterOption,
	unsigned char b_SecondCounterModus, unsigned char b_SecondCounterOption)
{
	int i_ReturnValue = 0;

	/*******************************/
	/* Test if incremental counter */
	/*******************************/

	if ((devpriv->s_BoardInfos.
			dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER) {
	   /**************************/
		/* Test the counter range */
	   /**************************/

		if (b_CounterRange == APCI1710_16BIT_COUNTER
			|| b_CounterRange == APCI1710_32BIT_COUNTER) {
	      /********************************/
			/* Test the first counter modus */
	      /********************************/

			if (b_FirstCounterModus == APCI1710_QUADRUPLE_MODE ||
				b_FirstCounterModus == APCI1710_DOUBLE_MODE ||
				b_FirstCounterModus == APCI1710_SIMPLE_MODE ||
				b_FirstCounterModus == APCI1710_DIRECT_MODE) {
		 /*********************************/
				/* Test the first counter option */
		 /*********************************/

				if ((b_FirstCounterModus == APCI1710_DIRECT_MODE
						&& (b_FirstCounterOption ==
							APCI1710_INCREMENT
							|| b_FirstCounterOption
							== APCI1710_DECREMENT))
					|| (b_FirstCounterModus !=
						APCI1710_DIRECT_MODE
						&& (b_FirstCounterOption ==
							APCI1710_HYSTERESIS_ON
							|| b_FirstCounterOption
							==
							APCI1710_HYSTERESIS_OFF)))
				{
		    /**************************/
					/* Test if 16-bit counter */
		    /**************************/

					if (b_CounterRange ==
						APCI1710_16BIT_COUNTER) {
		       /*********************************/
						/* Test the second counter modus */
		       /*********************************/

						if ((b_FirstCounterModus !=
								APCI1710_DIRECT_MODE
								&&
								(b_SecondCounterModus
									==
									APCI1710_QUADRUPLE_MODE
									||
									b_SecondCounterModus
									==
									APCI1710_DOUBLE_MODE
									||
									b_SecondCounterModus
									==
									APCI1710_SIMPLE_MODE))
							|| (b_FirstCounterModus
								==
								APCI1710_DIRECT_MODE
								&&
								b_SecondCounterModus
								==
								APCI1710_DIRECT_MODE))
						{
			  /**********************************/
							/* Test the second counter option */
			  /**********************************/

							if ((b_SecondCounterModus == APCI1710_DIRECT_MODE && (b_SecondCounterOption == APCI1710_INCREMENT || b_SecondCounterOption == APCI1710_DECREMENT)) || (b_SecondCounterModus != APCI1710_DIRECT_MODE && (b_SecondCounterOption == APCI1710_HYSTERESIS_ON || b_SecondCounterOption == APCI1710_HYSTERESIS_OFF))) {
								i_ReturnValue =
									0;
							} else {
			     /*********************************************************/
								/* The selected second counter operating option is wrong */
			     /*********************************************************/

								DPRINTK("The selected second counter operating option is wrong\n");
								i_ReturnValue =
									-7;
							}
						} else {
			  /*******************************************************/
							/* The selected second counter operating mode is wrong */
			  /*******************************************************/

							DPRINTK("The selected second counter operating mode is wrong\n");
							i_ReturnValue = -6;
						}
					}
				} else {
		    /********************************************************/
					/* The selected first counter operating option is wrong */
		    /********************************************************/

					DPRINTK("The selected first counter operating option is wrong\n");
					i_ReturnValue = -5;
				}
			} else {
		 /******************************************************/
				/* The selected first counter operating mode is wrong */
		 /******************************************************/
				DPRINTK("The selected first counter operating mode is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /***************************************/
			/* The selected counter range is wrong */
	      /***************************************/

			DPRINTK("The selected counter range is wrong\n");
			i_ReturnValue = -3;
		}

	   /*************************/
		/* Test if a error occur */
	   /*************************/

		if (i_ReturnValue == 0) {
	      /**************************/
			/* Test if 16-Bit counter */
	      /**************************/

			if (b_CounterRange == APCI1710_32BIT_COUNTER) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister1 = b_CounterRange |
					b_FirstCounterModus |
					b_FirstCounterOption;
			} else {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister1 = b_CounterRange |
					(b_FirstCounterModus & 0x5) |
					(b_FirstCounterOption & 0x20) |
					(b_SecondCounterModus & 0xA) |
					(b_SecondCounterOption & 0x40);

		 /***********************/
				/* Test if direct mode */
		 /***********************/

				if (b_FirstCounterModus == APCI1710_DIRECT_MODE) {
					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister1 = devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister1 |
						APCI1710_DIRECT_MODE;
				}
			}

	      /***************************/
			/* Write the configuration */
	      /***************************/

			outl(devpriv->s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				dw_ModeRegister1_2_3_4,
				devpriv->s_BoardInfos.
				ui_Address + 20 + (64 * b_ModulNbr));

			devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_CounterInit = 1;
		}
	} else {
	   /**************************************/
		/* The module is not a counter module */
	   /**************************************/

		DPRINTK("The module is not a counter module\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_CounterAutoTest                       |
|                                               (unsigned char_     b_BoardHandle,    |
|                                                unsigned char *_   pb_TestStatus)     |
+----------------------------------------------------------------------------+
| Task              : A test mode is intended for testing the component and  |
|                     the connected periphery. All the 8-bit counter chains  |
|                     are operated internally as down counters.              |
|                     Independently from the external signals,               |
|                     all the four 8-bit counter chains are decremented in   |
|                     parallel by each negative clock pulse edge of CLKX.    |
|                                                                            |
|                       Counter auto test conclusion                         |
|                       ----------------------------                         |
|              +-----------------+-----------------------------+             |
|              | pb_TestStatus   |    Error description        |             |
|              |     mask        |                             |             |
|              |-----------------+-----------------------------|             |
|              |    0000         |     No error detected       |             |
|              |-----------------|-----------------------------|             |
|              |    0001         | Error detected of counter 0 |             |
|              |-----------------|-----------------------------|             |
|              |    0010         | Error detected of counter 1 |             |
|              |-----------------|-----------------------------|             |
|              |    0100         | Error detected of counter 2 |             |
|              |-----------------|-----------------------------|             |
|              |    1000         | Error detected of counter 3 |             |
|              +-----------------+-----------------------------+             |
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_   b_BoardHandle : Handle of board APCI-1710      |  |
+----------------------------------------------------------------------------+
| Output Parameters : unsigned char *_ pb_TestStatus  : Auto test conclusion. See table|
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_CounterAutoTest(struct comedi_device *dev, unsigned char *pb_TestStatus)
{
	unsigned char b_ModulCpt = 0;
	int i_ReturnValue = 0;
	unsigned int dw_LathchValue;

	*pb_TestStatus = 0;

	/********************************/
	/* Test if counter module found */
	/********************************/

	if ((devpriv->s_BoardInfos.
			dw_MolduleConfiguration[0] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[1] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[2] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[3] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER) {
		for (b_ModulCpt = 0; b_ModulCpt < 4; b_ModulCpt++) {
	      /*******************************/
			/* Test if incremental counter */
	      /*******************************/

			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulCpt] &
					0xFFFF0000UL) ==
				APCI1710_INCREMENTAL_COUNTER) {
		 /******************/
				/* Start the test */
		 /******************/

				outl(3, devpriv->s_BoardInfos.
					ui_Address + 16 + (64 * b_ModulCpt));

		 /*********************/
				/* Tatch the counter */
		 /*********************/

				outl(1, devpriv->s_BoardInfos.
					ui_Address + (64 * b_ModulCpt));

		 /************************/
				/* Read the latch value */
		 /************************/

				dw_LathchValue = inl(devpriv->s_BoardInfos.
					ui_Address + 4 + (64 * b_ModulCpt));

				if ((dw_LathchValue & 0xFF) !=
					((dw_LathchValue >> 8) & 0xFF)
					&& (dw_LathchValue & 0xFF) !=
					((dw_LathchValue >> 16) & 0xFF)
					&& (dw_LathchValue & 0xFF) !=
					((dw_LathchValue >> 24) & 0xFF)) {
					*pb_TestStatus =
						*pb_TestStatus | (1 <<
						b_ModulCpt);
				}

		 /*****************/
				/* Stop the test */
		 /*****************/

				outl(0, devpriv->s_BoardInfos.
					ui_Address + 16 + (64 * b_ModulCpt));
			}
		}
	} else {
	   /***************************/
		/* No counter module found */
	   /***************************/

		DPRINTK("No counter module found\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_InitIndex (unsigned char_ b_BoardHandle,       |
|                                                 unsigned char_ b_ModulNbr,          |
|                                                 unsigned char_ b_ReferenceAction,   |
|                                                 unsigned char_ b_IndexOperation,    |
|                                                 unsigned char_ b_AutoMode,          |
|                                                 unsigned char_ b_InterruptEnable)   |
+----------------------------------------------------------------------------+
| Task              : Initialise the index corresponding to the selected     |
|                     module (b_ModulNbr). If a INDEX flag occur, you have   |
|                     the possibility to clear the 32-Bit counter or to latch|
|                     the current 32-Bit value in to the first latch         |
|                     register. The b_IndexOperation parameter give the      |
|                     possibility to choice the INDEX action.                |
|                     If you have enabled the automatic mode, each INDEX     |
|                     action is cleared automatically, else you must read    |
|                     the index status ("i_APCI1710_ReadIndexStatus")        |
|                     after each INDEX action.                               |
|                                                                            |
|                                                                            |
|                               Index action                                 |
|                               ------------                                 |
|                                                                            |
|           +------------------------+------------------------------------+  |
|           |   b_IndexOperation     |         Operation                  |  |
|           |------------------------+------------------------------------|  |
|           |APCI1710_LATCH_COUNTER  | After a index signal, the counter  |  |
|           |                        | value (32-Bit) is latched in to    |  |
|           |                        | the first latch register           |  |
|           |------------------------|------------------------------------|  |
|           |APCI1710_CLEAR_COUNTER  | After a index signal, the counter  |  |
|           |                        | value is cleared (32-Bit)          |  |
|           +------------------------+------------------------------------+  |
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_ b_BoardHandle     : Handle of board APCI-1710    |
|                     unsigned char_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
|                     unsigned char_ b_ReferenceAction : Determine if the reference   |
|                                               must set or no for the       |
|                                               acceptance from index        |
|                                               APCI1710_ENABLE :            |
|                                                  Reference must be set for |
|                                                  accepted the index        |
|                                               APCI1710_DISABLE :           |
|                                                  Reference have not        |
|                                                  importance                |
|                     unsigned char_ b_IndexOperation  : Index operating mode.        |
|                                               See table.                   |
|                     unsigned char_ b_AutoMode        : Enable or disable the        |
|                                               automatic index reset.       |
|                                               APCI1710_ENABLE :            |
|                                                 Enable the automatic mode  |
|                                               APCI1710_DISABLE :           |
|                                                 Disable the automatic mode |
|                     unsigned char_ b_InterruptEnable : Enable or disable the        |
|                                               interrupt.                   |
|                                               APCI1710_ENABLE :            |
|                                               Enable the interrupt         |
|                                               APCI1710_DISABLE :           |
|                                               Disable the interrupt        |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4  The reference action parameter is wrong            |
|                     -5: The index operating mode parameter is wrong        |
|                     -6: The auto mode parameter is wrong                   |
|                     -7: Interrupt parameter is wrong                       |
|                     -8: Interrupt function not initialised.                |
|                         See function "i_APCI1710_SetBoardIntRoutineX"      |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_InitIndex(struct comedi_device *dev,
	unsigned char b_ModulNbr,
	unsigned char b_ReferenceAction,
	unsigned char b_IndexOperation, unsigned char b_AutoMode, unsigned char b_InterruptEnable)
{
	int i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /********************************/
			/* Test the reference parameter */
	      /********************************/

			if (b_ReferenceAction == APCI1710_ENABLE ||
				b_ReferenceAction == APCI1710_DISABLE) {
		 /****************************/
				/* Test the index parameter */
		 /****************************/

				if (b_IndexOperation ==
					APCI1710_HIGH_EDGE_LATCH_COUNTER
					|| b_IndexOperation ==
					APCI1710_LOW_EDGE_LATCH_COUNTER
					|| b_IndexOperation ==
					APCI1710_HIGH_EDGE_CLEAR_COUNTER
					|| b_IndexOperation ==
					APCI1710_LOW_EDGE_CLEAR_COUNTER
					|| b_IndexOperation ==
					APCI1710_HIGH_EDGE_LATCH_AND_CLEAR_COUNTER
					|| b_IndexOperation ==
					APCI1710_LOW_EDGE_LATCH_AND_CLEAR_COUNTER)
				{
		    /********************************/
					/* Test the auto mode parameter */
		    /********************************/

					if (b_AutoMode == APCI1710_ENABLE ||
						b_AutoMode == APCI1710_DISABLE)
					{
		       /***************************/
						/* Test the interrupt mode */
		       /***************************/

						if (b_InterruptEnable ==
							APCI1710_ENABLE
							|| b_InterruptEnable ==
							APCI1710_DISABLE) {

			     /************************************/
							/* Makte the configuration commando */
			     /************************************/

							if (b_ReferenceAction ==
								APCI1710_ENABLE)
							{
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									|
									APCI1710_ENABLE_INDEX_ACTION;
							} else {
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									&
									APCI1710_DISABLE_INDEX_ACTION;
							}

			     /****************************************/
							/* Test if low level latch or/and clear */
			     /****************************************/

							if (b_IndexOperation ==
								APCI1710_LOW_EDGE_LATCH_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOW_EDGE_CLEAR_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOW_EDGE_LATCH_AND_CLEAR_COUNTER)
							{
				/*************************************/
								/* Set the index level to low (DQ26) */
				/*************************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									|
									APCI1710_SET_LOW_INDEX_LEVEL;
							} else {
				/**************************************/
								/* Set the index level to high (DQ26) */
				/**************************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									&
									APCI1710_SET_HIGH_INDEX_LEVEL;
							}

			     /***********************************/
							/* Test if latch and clear counter */
			     /***********************************/

							if (b_IndexOperation ==
								APCI1710_HIGH_EDGE_LATCH_AND_CLEAR_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOW_EDGE_LATCH_AND_CLEAR_COUNTER)
							{
				/***************************************/
								/* Set the latch and clear flag (DQ27) */
				/***************************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									|
									APCI1710_ENABLE_LATCH_AND_CLEAR;
							}	/*  if (b_IndexOperation == APCI1710_HIGH_EDGE_LATCH_AND_CLEAR_COUNTER || b_IndexOperation == APCI1710_LOW_EDGE_LATCH_AND_CLEAR_COUNTER) */
							else {
				/*****************************************/
								/* Clear the latch and clear flag (DQ27) */
				/*****************************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									&
									APCI1710_DISABLE_LATCH_AND_CLEAR;

				/*************************/
								/* Test if latch counter */
				/*************************/

								if (b_IndexOperation == APCI1710_HIGH_EDGE_LATCH_COUNTER || b_IndexOperation == APCI1710_LOW_EDGE_LATCH_COUNTER) {
				   /*********************************/
									/* Enable the latch from counter */
				   /*********************************/

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister2
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister2
										|
										APCI1710_INDEX_LATCH_COUNTER;
								} else {
				   /*********************************/
									/* Enable the clear from counter */
				   /*********************************/

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister2
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister2
										&
										(~APCI1710_INDEX_LATCH_COUNTER);
								}
							}	/*  // if (b_IndexOperation == APCI1710_HIGH_EDGE_LATCH_AND_CLEAR_COUNTER || b_IndexOperation == APCI1710_LOW_EDGE_LATCH_AND_CLEAR_COUNTER) */

							if (b_AutoMode ==
								APCI1710_DISABLE)
							{
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									|
									APCI1710_INDEX_AUTO_MODE;
							} else {
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									&
									(~APCI1710_INDEX_AUTO_MODE);
							}

							if (b_InterruptEnable ==
								APCI1710_ENABLE)
							{
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister3
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister3
									|
									APCI1710_ENABLE_INDEX_INT;
							} else {
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister3
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister3
									&
									APCI1710_DISABLE_INDEX_INT;
							}

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_InitFlag.
								b_IndexInit = 1;

						} else {
			  /********************************/
							/* Interrupt parameter is wrong */
			  /********************************/
							DPRINTK("Interrupt parameter is wrong\n");
							i_ReturnValue = -7;
						}
					} else {
		       /************************************/
						/* The auto mode parameter is wrong */
		       /************************************/

						DPRINTK("The auto mode parameter is wrong\n");
						i_ReturnValue = -6;
					}
				} else {
		    /***********************************************/
					/* The index operating mode parameter is wrong */
		    /***********************************************/

					DPRINTK("The index operating mode parameter is wrong\n");
					i_ReturnValue = -5;
				}
			} else {
		 /*******************************************/
				/* The reference action parameter is wrong */
		 /*******************************************/

				DPRINTK("The reference action parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_InitReference                         |
|                                                (unsigned char_ b_BoardHandle,       |
|                                                 unsigned char_ b_ModulNbr,          |
|                                                 unsigned char_ b_ReferenceLevel)    |
+----------------------------------------------------------------------------+
| Task              : Initialise the reference corresponding to the selected |
|                     module (b_ModulNbr).                                   |
|                                                                            |
|                               Reference level                              |
|                               ---------------                              |
|             +--------------------+-------------------------+               |
|             | b_ReferenceLevel   |         Operation       |               |
|             +--------------------+-------------------------+               |
|             |   APCI1710_LOW     |  Reference occur if "0" |               |
|             |--------------------|-------------------------|               |
|             |   APCI1710_HIGH    |  Reference occur if "1" |               |
|             +--------------------+-------------------------+               |
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_ b_BoardHandle     : Handle of board APCI-1710    |
|                     unsigned char_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
|                     unsigned char_ b_ReferenceLevel  : Reference level.             |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: The selected module number parameter is wrong      |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: Reference level parameter is wrong                 |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_InitReference(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_ReferenceLevel)
{
	int i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /**************************************/
			/* Test the reference level parameter */
	      /**************************************/

			if (b_ReferenceLevel == 0 || b_ReferenceLevel == 1) {
				if (b_ReferenceLevel == 1) {
					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister2 = devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister2 |
						APCI1710_REFERENCE_HIGH;
				} else {
					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister2 = devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister2 &
						APCI1710_REFERENCE_LOW;
				}

				outl(devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					dw_ModeRegister1_2_3_4,
					devpriv->s_BoardInfos.ui_Address + 20 +
					(64 * b_ModulNbr));

				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_InitFlag.b_ReferenceInit = 1;
			} else {
		 /**************************************/
				/* Reference level parameter is wrong */
		 /**************************************/

				DPRINTK("Reference level parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_	i_APCI1710_InitExternalStrobe                |
|					(unsigned char_ b_BoardHandle,                |
|					 unsigned char_ b_ModulNbr,                   |
|					 unsigned char_ b_ExternalStrobe,             |
|					 unsigned char_ b_ExternalStrobeLevel)        |
+----------------------------------------------------------------------------+
| Task              : Initialises the external strobe level corresponding to |
|		      the selected module (b_ModulNbr).                      |
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_ b_BoardHandle     : Handle of board APCI-1710    |
|                     unsigned char_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
|		      unsigned char_ b_ExternalStrobe  : External strobe selection    |
|						0 : External strobe A        |
|						1 : External strobe B        |
|		      unsigned char_ b_ExternalStrobeLevel : External strobe level    |
|						APCI1710_LOW :               |
|						External latch occurs if "0" |
|						APCI1710_HIGH :              |
|						External latch occurs if "1" |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: The selected module number is wrong                |
|                     -3: Counter not initialised.                           |
|			  See function "i_APCI1710_InitCounter"              |
|                     -4: External strobe selection is wrong                 |
|                     -5: External strobe level parameter is wrong           |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_InitExternalStrobe(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_ExternalStrobe, unsigned char b_ExternalStrobeLevel)
{
	int i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /**************************************/
			/* Test the external strobe selection */
	      /**************************************/

			if (b_ExternalStrobe == 0 || b_ExternalStrobe == 1) {
		 /******************/
				/* Test the level */
		 /******************/

				if ((b_ExternalStrobeLevel == APCI1710_HIGH) ||
					((b_ExternalStrobeLevel == APCI1710_LOW
							&& (devpriv->
								s_BoardInfos.
								dw_MolduleConfiguration
								[b_ModulNbr] &
								0xFFFF) >=
							0x3135))) {
		    /*****************/
					/* Set the level */
		    /*****************/

					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister4 = (devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister4 & (0xFF -
							(0x10 << b_ExternalStrobe))) | ((b_ExternalStrobeLevel ^ 1) << (4 + b_ExternalStrobe));
				} else {
		    /********************************************/
					/* External strobe level parameter is wrong */
		    /********************************************/

					DPRINTK("External strobe level parameter is wrong\n");
					i_ReturnValue = -5;
				}
			}	/*  if (b_ExternalStrobe == 0 || b_ExternalStrobe == 1) */
			else {
		 /**************************************/
				/* External strobe selection is wrong */
		 /**************************************/

				DPRINTK("External strobe selection is wrong\n");
				i_ReturnValue = -4;
			}	/*  if (b_ExternalStrobe == 0 || b_ExternalStrobe == 1) */
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

	/*
	   +----------------------------------------------------------------------------+
	   | Function Name     : _INT_ i_APCI1710_InitCompareLogic                      |
	   |                               (unsigned char_   b_BoardHandle,                      |
	   |                                unsigned char_   b_ModulNbr,                         |
	   |                                unsigned int_  ui_CompareValue)                     |
	   +----------------------------------------------------------------------------+
	   | Task              : Set the 32-Bit compare value. At that moment that the  |
	   |                     incremental counter arrive to the compare value        |
	   |                     (ui_CompareValue) a interrupt is generated.            |
	   +----------------------------------------------------------------------------+
	   | Input Parameters  : unsigned char_  b_BoardHandle    : Handle of board APCI-1710    |
	   |                     unsigned char_  b_ModulNbr       : Module number to configure   |
	   |                                               (0 to 3)                     |
	   |                     unsigned int_ ui_CompareValue   : 32-Bit compare value         |
	   +----------------------------------------------------------------------------+
	   | Output Parameters : -
	   +----------------------------------------------------------------------------+
	   | Return Value      :  0: No error                                           |
	   |                     -1: The handle parameter of the board is wrong         |
	   |                     -2: No counter module found                            |
	   |                     -3: Counter not initialised see function               |
	   |                         "i_APCI1710_InitCounter"                           |
	   +----------------------------------------------------------------------------+
	 */

int i_APCI1710_InitCompareLogic(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned int ui_CompareValue)
{
	int i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {

			outl(ui_CompareValue, devpriv->s_BoardInfos.
				ui_Address + 28 + (64 * b_ModulNbr));

			devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_CompareLogicInit = 1;
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_InitFrequencyMeasurement              |
|				(unsigned char_		 b_BoardHandle,              |
|				 unsigned char_		 b_ModulNbr,                 |
|				 unsigned char_		 b_PCIInputClock,            |
|				 unsigned char_		 b_TimingUnity,              |
|				 ULONG_ 	ul_TimingInterval,           |
|				 PULONG_       pul_RealTimingInterval)       |
+----------------------------------------------------------------------------+
| Task              : Sets the time for the frequency measurement.           |
|		      Configures the selected TOR incremental counter of the |
|		      selected module (b_ModulNbr). The ul_TimingInterval and|
|		      ul_TimingUnity determine the time base for the         |
|		      measurement. The pul_RealTimingInterval returns the    |
|		      real time value. You must call up this function before |
|		      you call up any other function which gives access to   |
|		      the frequency measurement.                             |
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_  b_BoardHandle    : Handle of board APCI-1710    |
|		      unsigned char_  b_ModulNbr	      :	Number of the module to be   |
|						configured (0 to 3)          |
|		      unsigned char_  b_PCIInputClock  :	Selection of the PCI bus     |
|						clock                        |
|						- APCI1710_30MHZ :           |
|						  The PC has a PCI bus clock |
|						  of 30 MHz                  |
|						- APCI1710_33MHZ :           |
|						  The PC has a PCI bus clock |
|						  of 33 MHz                  |
|		      unsigned char_  b_TimingUnity    : Base time unit (0 to 2)      |
|						  0 : ns                     |
|						  1 : æs                     |
|						  2 : ms                     |
|		      ULONG_ ul_TimingInterval: Base time value.             |
+----------------------------------------------------------------------------+
| Output Parameters : PULONG_ pul_RealTimingInterval : Real base time value. |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: The selected module number is wrong                |
|                     -3: Counter not initialised see function               |
|			  "i_APCI1710_InitCounter"                           |
|                     -4: The selected PCI input clock is wrong              |
|                     -5: Timing unity selection is wrong                    |
|                     -6: Base timing selection is wrong                     |
|		      -7: 40MHz quartz not on board                          |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_InitFrequencyMeasurement(struct comedi_device *dev,
	unsigned char b_ModulNbr,
	unsigned char b_PCIInputClock,
	unsigned char b_TimingUnity,
	unsigned int ul_TimingInterval, unsigned int *pul_RealTimingInterval)
{
	int i_ReturnValue = 0;
	unsigned int ul_TimerValue = 0;
	double d_RealTimingInterval;
	unsigned int dw_Status = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /**************************/
			/* Test the PCI bus clock */
	      /**************************/

			if ((b_PCIInputClock == APCI1710_30MHZ) ||
				(b_PCIInputClock == APCI1710_33MHZ) ||
				(b_PCIInputClock == APCI1710_40MHZ)) {
		 /************************/
				/* Test the timing unit */
		 /************************/

				if (b_TimingUnity <= 2) {
		    /**********************************/
					/* Test the base timing selection */
		    /**********************************/

					if (((b_PCIInputClock == APCI1710_30MHZ)
							&& (b_TimingUnity == 0)
							&& (ul_TimingInterval >=
								266)
							&& (ul_TimingInterval <=
								8738133UL))
						|| ((b_PCIInputClock ==
								APCI1710_30MHZ)
							&& (b_TimingUnity == 1)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								8738UL))
						|| ((b_PCIInputClock ==
								APCI1710_30MHZ)
							&& (b_TimingUnity == 2)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								8UL))
						|| ((b_PCIInputClock ==
								APCI1710_33MHZ)
							&& (b_TimingUnity == 0)
							&& (ul_TimingInterval >=
								242)
							&& (ul_TimingInterval <=
								7943757UL))
						|| ((b_PCIInputClock ==
								APCI1710_33MHZ)
							&& (b_TimingUnity == 1)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								7943UL))
						|| ((b_PCIInputClock ==
								APCI1710_33MHZ)
							&& (b_TimingUnity == 2)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								7UL))
						|| ((b_PCIInputClock ==
								APCI1710_40MHZ)
							&& (b_TimingUnity == 0)
							&& (ul_TimingInterval >=
								200)
							&& (ul_TimingInterval <=
								6553500UL))
						|| ((b_PCIInputClock ==
								APCI1710_40MHZ)
							&& (b_TimingUnity == 1)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								6553UL))
						|| ((b_PCIInputClock ==
								APCI1710_40MHZ)
							&& (b_TimingUnity == 2)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								6UL))) {
		       /**********************/
						/* Test if 40MHz used */
		       /**********************/

						if (b_PCIInputClock ==
							APCI1710_40MHZ) {
			  /******************************/
							/* Test if firmware >= Rev1.5 */
			  /******************************/

							if ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF) >= 0x3135) {
			     /*********************************/
								/* Test if 40MHz quartz on board */
			     /*********************************/

								/*INPDW (ps_APCI1710Variable->
								   s_Board [b_BoardHandle].
								   s_BoardInfos.
								   ui_Address + 36 + (64 * b_ModulNbr), &dw_Status); */
								dw_Status =
									inl
									(devpriv->
									s_BoardInfos.
									ui_Address
									+ 36 +
									(64 * b_ModulNbr));

			     /******************************/
								/* Test the quartz flag (DQ0) */
			     /******************************/

								if ((dw_Status & 1) != 1) {
				/*****************************/
									/* 40MHz quartz not on board */
				/*****************************/

									DPRINTK("40MHz quartz not on board\n");
									i_ReturnValue
										=
										-7;
								}
							} else {
			     /*****************************/
								/* 40MHz quartz not on board */
			     /*****************************/
								DPRINTK("40MHz quartz not on board\n");
								i_ReturnValue =
									-7;
							}
						}	/*  if (b_PCIInputClock == APCI1710_40MHZ) */

		       /***************************/
						/* Test if not error occur */
		       /***************************/

						if (i_ReturnValue == 0) {
			  /****************************/
							/* Test the INC_CPT version */
			  /****************************/

							if ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF) >= 0x3131) {

				/**********************/
								/* Test if 40MHz used */
				/**********************/

								if (b_PCIInputClock == APCI1710_40MHZ) {
				   /*********************************/
									/* Enable the 40MHz quarz (DQ30) */
				   /*********************************/

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister4
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister4
										|
										APCI1710_ENABLE_40MHZ_FREQUENCY;
								}	/*  if (b_PCIInputClock == APCI1710_40MHZ) */
								else {
				   /**********************************/
									/* Disable the 40MHz quarz (DQ30) */
				   /**********************************/

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister4
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister4
										&
										APCI1710_DISABLE_40MHZ_FREQUENCY;

								}	/*  if (b_PCIInputClock == APCI1710_40MHZ) */

			     /********************************/
								/* Calculate the division fator */
			     /********************************/

								fpu_begin();
								switch (b_TimingUnity) {
				/******/
									/* ns */
				/******/

								case 0:

					/******************/
									/* Timer 0 factor */
					/******************/

									ul_TimerValue
										=
										(unsigned int)
										(ul_TimingInterval
										*
										(0.00025 * b_PCIInputClock));

					/*******************/
									/* Round the value */
					/*******************/

									if ((double)((double)ul_TimingInterval * (0.00025 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
										ul_TimerValue
											=
											ul_TimerValue
											+
											1;
									}

					/*****************************/
									/* Calculate the real timing */
					/*****************************/

									*pul_RealTimingInterval
										=
										(unsigned int)
										(ul_TimerValue
										/
										(0.00025 * (double)b_PCIInputClock));
									d_RealTimingInterval
										=
										(double)
										ul_TimerValue
										/
										(0.00025
										*
										(double)
										b_PCIInputClock);

									if ((double)((double)ul_TimerValue / (0.00025 * (double)b_PCIInputClock)) >= (double)((double)*pul_RealTimingInterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
											+
											1;
									}

									ul_TimingInterval
										=
										ul_TimingInterval
										-
										1;
									ul_TimerValue
										=
										ul_TimerValue
										-
										2;

									break;

				/******/
									/* æs */
				/******/

								case 1:

					/******************/
									/* Timer 0 factor */
					/******************/

									ul_TimerValue
										=
										(unsigned int)
										(ul_TimingInterval
										*
										(0.25 * b_PCIInputClock));

					/*******************/
									/* Round the value */
					/*******************/

									if ((double)((double)ul_TimingInterval * (0.25 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
										ul_TimerValue
											=
											ul_TimerValue
											+
											1;
									}

					/*****************************/
									/* Calculate the real timing */
					/*****************************/

									*pul_RealTimingInterval
										=
										(unsigned int)
										(ul_TimerValue
										/
										(0.25 * (double)b_PCIInputClock));
									d_RealTimingInterval
										=
										(double)
										ul_TimerValue
										/
										(
										(double)
										0.25
										*
										(double)
										b_PCIInputClock);

									if ((double)((double)ul_TimerValue / (0.25 * (double)b_PCIInputClock)) >= (double)((double)*pul_RealTimingInterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
											+
											1;
									}

									ul_TimingInterval
										=
										ul_TimingInterval
										-
										1;
									ul_TimerValue
										=
										ul_TimerValue
										-
										2;

									break;

				/******/
									/* ms */
				/******/

								case 2:

					/******************/
									/* Timer 0 factor */
					/******************/

									ul_TimerValue
										=
										ul_TimingInterval
										*
										(250.0
										*
										b_PCIInputClock);

					/*******************/
									/* Round the value */
					/*******************/

									if ((double)((double)ul_TimingInterval * (250.0 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
										ul_TimerValue
											=
											ul_TimerValue
											+
											1;
									}

					/*****************************/
									/* Calculate the real timing */
					/*****************************/

									*pul_RealTimingInterval
										=
										(unsigned int)
										(ul_TimerValue
										/
										(250.0 * (double)b_PCIInputClock));
									d_RealTimingInterval
										=
										(double)
										ul_TimerValue
										/
										(250.0
										*
										(double)
										b_PCIInputClock);

									if ((double)((double)ul_TimerValue / (250.0 * (double)b_PCIInputClock)) >= (double)((double)*pul_RealTimingInterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
											+
											1;
									}

									ul_TimingInterval
										=
										ul_TimingInterval
										-
										1;
									ul_TimerValue
										=
										ul_TimerValue
										-
										2;

									break;
								}

								fpu_end();
			     /*************************/
								/* Write the timer value */
			     /*************************/

								outl(ul_TimerValue, devpriv->s_BoardInfos.ui_Address + 32 + (64 * b_ModulNbr));

			     /*******************************/
								/* Set the initialisation flag */
			     /*******************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_InitFlag.
									b_FrequencyMeasurementInit
									= 1;
							} else {
			     /***************************/
								/* Counter not initialised */
			     /***************************/

								DPRINTK("Counter not initialised\n");
								i_ReturnValue =
									-3;
							}
						}	/*  if (i_ReturnValue == 0) */
					} else {
		       /**********************************/
						/* Base timing selection is wrong */
		       /**********************************/

						DPRINTK("Base timing selection is wrong\n");
						i_ReturnValue = -6;
					}
				} else {
		    /***********************************/
					/* Timing unity selection is wrong */
		    /***********************************/

					DPRINTK("Timing unity selection is wrong\n");
					i_ReturnValue = -5;
				}
			} else {
		 /*****************************************/
				/* The selected PCI input clock is wrong */
		 /*****************************************/

				DPRINTK("The selected PCI input clock is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

/*########################################################################### */

							/* INSN BITS */
/*########################################################################### */

/*
+----------------------------------------------------------------------------+
| Function Name     :INT	i_APCI1710_InsnBitsINCCPT(struct comedi_device *dev,struct comedi_subdevice *s,
struct comedi_insn *insn,unsigned int *data)                   |
+----------------------------------------------------------------------------+
| Task              : Set & Clear Functions for INC_CPT                                          |
+----------------------------------------------------------------------------+
| Input Parameters  :
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :
+----------------------------------------------------------------------------+
*/

int i_APCI1710_InsnBitsINCCPT(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_BitsType;
	int i_ReturnValue = 0;
	ui_BitsType = CR_CHAN(insn->chanspec);
	devpriv->tsk_Current = current;	/*  Save the current process task structure */

	switch (ui_BitsType) {
	case APCI1710_INCCPT_CLEARCOUNTERVALUE:
		i_ReturnValue = i_APCI1710_ClearCounterValue(dev,
			(unsigned char) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_CLEARALLCOUNTERVALUE:
		i_ReturnValue = i_APCI1710_ClearAllCounterValue(dev);
		break;

	case APCI1710_INCCPT_SETINPUTFILTER:
		i_ReturnValue = i_APCI1710_SetInputFilter(dev,
			(unsigned char) CR_AREF(insn->chanspec),
			(unsigned char) data[0], (unsigned char) data[1]);
		break;

	case APCI1710_INCCPT_LATCHCOUNTER:
		i_ReturnValue = i_APCI1710_LatchCounter(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned char) data[0]);
		break;

	case APCI1710_INCCPT_SETINDEXANDREFERENCESOURCE:
		i_ReturnValue = i_APCI1710_SetIndexAndReferenceSource(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned char) data[0]);
		break;

	case APCI1710_INCCPT_SETDIGITALCHLON:
		i_ReturnValue = i_APCI1710_SetDigitalChlOn(dev,
			(unsigned char) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_SETDIGITALCHLOFF:
		i_ReturnValue = i_APCI1710_SetDigitalChlOff(dev,
			(unsigned char) CR_AREF(insn->chanspec));
		break;

	default:
		printk("Bits Config Parameter Wrong\n");
	}

	if (i_ReturnValue >= 0)
		i_ReturnValue = insn->n;
	return i_ReturnValue;
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_ClearCounterValue                     |
|                               (unsigned char_      b_BoardHandle,                   |
|                                unsigned char_       b_ModulNbr)                     |
+----------------------------------------------------------------------------+
| Task              : Clear the counter value from selected module           |
|                     (b_ModulNbr).                                          |
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_ b_BoardHandle : Handle of board APCI-1710        |
|                     unsigned char_ b_ModulNbr    : Module number to configure       |
|                                           (0 to 3)                         |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: The selected module number parameter is wrong      |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_ClearCounterValue(struct comedi_device *dev, unsigned char b_ModulNbr)
{
	int i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*********************/
			/* Clear the counter */
	      /*********************/

			outl(1, devpriv->s_BoardInfos.
				ui_Address + 16 + (64 * b_ModulNbr));
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_ClearAllCounterValue                  |
|                               (unsigned char_      b_BoardHandle)                   |
+----------------------------------------------------------------------------+
| Task              : Clear all counter value.                               |
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_ b_BoardHandle : Handle of board APCI-1710        |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_ClearAllCounterValue(struct comedi_device *dev)
{
	unsigned char b_ModulCpt = 0;
	int i_ReturnValue = 0;

	/********************************/
	/* Test if counter module found */
	/********************************/

	if ((devpriv->s_BoardInfos.
			dw_MolduleConfiguration[0] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[1] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[2] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[3] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER) {
		for (b_ModulCpt = 0; b_ModulCpt < 4; b_ModulCpt++) {
	      /*******************************/
			/* Test if incremental counter */
	      /*******************************/

			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulCpt] &
					0xFFFF0000UL) ==
				APCI1710_INCREMENTAL_COUNTER) {
		 /*********************/
				/* Clear the counter */
		 /*********************/

				outl(1, devpriv->s_BoardInfos.
					ui_Address + 16 + (64 * b_ModulCpt));
			}
		}
	} else {
	   /***************************/
		/* No counter module found */
	   /***************************/

		DPRINTK("No counter module found\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_SetInputFilter                        |
|					(unsigned char_ b_BoardHandle,                |
|					 unsigned char_ b_Module,                     |
|					 unsigned char_ b_PCIInputClock,              |
|					 unsigned char_ b_Filter)     		     |
+----------------------------------------------------------------------------+
| Task              : Disable or enable the software filter from selected    |
|		      module (b_ModulNbr). b_Filter determine the filter time|
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_  b_BoardHandle    : Handle of board APCI-1710    |
|		      unsigned char_  b_ModulNbr	      :	Number of the module to be   |
|						configured (0 to 3)          |
|		      unsigned char_  b_PCIInputClock  :	Selection of the PCI bus     |
|						clock                        |
|						- APCI1710_30MHZ :           |
|						  The PC has a PCI bus clock |
|						  of 30 MHz                  |
|						- APCI1710_33MHZ :           |
|						  The PC has a PCI bus clock |
|						  of 33 MHz                  |
|						- APCI1710_40MHZ :           |
|						  The APCI1710 has a 40MHz    |
|						  quartz		     |
|		      unsigned char_  b_Filter	      : Filter selection             |
|                                                                            |
|				30 MHz                                       |
|				------                                       |
|					0:  Software filter not used         |
|					1:  Filter from 266ns  (3.750000MHz) |
|					2:  Filter from 400ns  (2.500000MHz) |
|					3:  Filter from 533ns  (1.876170MHz) |
|					4:  Filter from 666ns  (1.501501MHz) |
|					5:  Filter from 800ns  (1.250000MHz) |
|					6:  Filter from 933ns  (1.071800MHz) |
|					7:  Filter from 1066ns (0.938080MHz) |
|					8:  Filter from 1200ns (0.833333MHz) |
|					9:  Filter from 1333ns (0.750000MHz) |
|					10: Filter from 1466ns (0.682100MHz) |
|					11: Filter from 1600ns (0.625000MHz) |
|					12: Filter from 1733ns (0.577777MHz) |
|					13: Filter from 1866ns (0.535900MHz) |
|					14: Filter from 2000ns (0.500000MHz) |
|					15: Filter from 2133ns (0.468800MHz) |
|									     |
|				33 MHz                                       |
|				------                                       |
|					0:  Software filter not used         |
|					1:  Filter from 242ns  (4.125000MHz) |
|					2:  Filter from 363ns  (2.754820MHz) |
|					3:  Filter from 484ns  (2.066115MHz) |
|					4:  Filter from 605ns  (1.652892MHz) |
|					5:  Filter from 726ns  (1.357741MHz) |
|					6:  Filter from 847ns  (1.180637MHz) |
|					7:  Filter from 968ns  (1.033055MHz) |
|					8:  Filter from 1089ns (0.918273MHz) |
|					9:  Filter from 1210ns (0.826446MHz) |
|					10: Filter from 1331ns (0.751314MHz) |
|					11: Filter from 1452ns (0.688705MHz) |
|					12: Filter from 1573ns (0.635727MHz) |
|					13: Filter from 1694ns (0.590318MHz) |
|					14: Filter from 1815ns (0.550964MHz) |
|					15: Filter from 1936ns (0.516528MHz) |
|									     |
|				40 MHz                                       |
|				------                                       |
|					0:  Software filter not used         |
|					1:  Filter from 200ns  (5.000000MHz) |
|					2:  Filter from 300ns  (3.333333MHz) |
|					3:  Filter from 400ns  (2.500000MHz) |
|					4:  Filter from 500ns  (2.000000MHz) |
|					5:  Filter from 600ns  (1.666666MHz) |
|					6:  Filter from 700ns  (1.428500MHz) |
|					7:  Filter from 800ns  (1.250000MHz) |
|					8:  Filter from 900ns  (1.111111MHz) |
|					9:  Filter from 1000ns (1.000000MHz) |
|					10: Filter from 1100ns (0.909090MHz) |
|					11: Filter from 1200ns (0.833333MHz) |
|					12: Filter from 1300ns (0.769200MHz) |
|					13: Filter from 1400ns (0.714200MHz) |
|					14: Filter from 1500ns (0.666666MHz) |
|					15: Filter from 1600ns (0.625000MHz) |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: The selected module number is wrong                |
|                     -3: The module is not a counter module                 |
|					  -4: The selected PCI input clock is wrong              |
|					  -5: The selected filter value is wrong                 |
|					  -6: 40MHz quartz not on board                          |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_SetInputFilter(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_PCIInputClock, unsigned char b_Filter)
{
	int i_ReturnValue = 0;
	unsigned int dw_Status = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if incremental counter */
	   /*******************************/

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_INCREMENTAL_COUNTER) {
	      /******************************/
			/* Test if firmware >= Rev1.5 */
	      /******************************/

			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulNbr] &
					0xFFFF) >= 0x3135) {
		 /**************************/
				/* Test the PCI bus clock */
		 /**************************/

				if ((b_PCIInputClock == APCI1710_30MHZ) ||
					(b_PCIInputClock == APCI1710_33MHZ) ||
					(b_PCIInputClock == APCI1710_40MHZ)) {
		    /*************************/
					/* Test the filter value */
		    /*************************/

					if (b_Filter < 16) {
		       /**********************/
						/* Test if 40MHz used */
		       /**********************/

						if (b_PCIInputClock ==
							APCI1710_40MHZ) {
			  /*********************************/
							/* Test if 40MHz quartz on board */
			  /*********************************/

							dw_Status =
								inl(devpriv->
								s_BoardInfos.
								ui_Address +
								36 +
								(64 * b_ModulNbr));

			  /******************************/
							/* Test the quartz flag (DQ0) */
			  /******************************/

							if ((dw_Status & 1) !=
								1) {
			     /*****************************/
								/* 40MHz quartz not on board */
			     /*****************************/

								DPRINTK("40MHz quartz not on board\n");
								i_ReturnValue =
									-6;
							}
						}	/*  if (b_PCIInputClock == APCI1710_40MHZ) */

		       /***************************/
						/* Test if error not occur */
		       /***************************/

						if (i_ReturnValue == 0) {
			  /**********************/
							/* Test if 40MHz used */
			  /**********************/

							if (b_PCIInputClock ==
								APCI1710_40MHZ)
							{
			     /*********************************/
								/* Enable the 40MHz quarz (DQ31) */
			     /*********************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									|
									APCI1710_ENABLE_40MHZ_FILTER;

							}	/*  if (b_PCIInputClock == APCI1710_40MHZ) */
							else {
			     /**********************************/
								/* Disable the 40MHz quarz (DQ31) */
			     /**********************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									&
									APCI1710_DISABLE_40MHZ_FILTER;

							}	/*  if (b_PCIInputClock == APCI1710_40MHZ) */

			  /************************/
							/* Set the filter value */
			  /************************/

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								s_ByteModeRegister.
								b_ModeRegister3
								=
								(devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								s_ByteModeRegister.
								b_ModeRegister3
								& 0x1F) |
								((b_Filter &
									0x7) <<
								5);

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								s_ByteModeRegister.
								b_ModeRegister4
								=
								(devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								s_ByteModeRegister.
								b_ModeRegister4
								& 0xFE) |
								((b_Filter &
									0x8) >>
								3);

			  /***************************/
							/* Write the configuration */
			  /***************************/

							outl(devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								dw_ModeRegister1_2_3_4,
								devpriv->
								s_BoardInfos.
								ui_Address +
								20 +
								(64 * b_ModulNbr));
						}	/*  if (i_ReturnValue == 0) */
					}	/*  if (b_Filter < 16) */
					else {
		       /**************************************/
						/* The selected filter value is wrong */
		       /**************************************/

						DPRINTK("The selected filter value is wrong\n");
						i_ReturnValue = -5;
					}	/*  if (b_Filter < 16) */
				}	/*  if ((b_PCIInputClock == APCI1710_30MHZ) || (b_PCIInputClock == APCI1710_33MHZ) || (b_PCIInputClock == APCI1710_40MHZ)) */
				else {
		    /*****************************************/
					/* The selected PCI input clock is wrong */
		    /*****************************************/

					DPRINTK("The selected PCI input clock is wrong\n");
					i_ReturnValue = 4;
				}	/*  if ((b_PCIInputClock == APCI1710_30MHZ) || (b_PCIInputClock == APCI1710_33MHZ) || (b_PCIInputClock == APCI1710_40MHZ)) */
			} else {
		 /**************************************/
				/* The module is not a counter module */
		 /**************************************/

				DPRINTK("The module is not a counter module\n");
				i_ReturnValue = -3;
			}
		} else {
	      /**************************************/
			/* The module is not a counter module */
	      /**************************************/

			DPRINTK("The module is not a counter module\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_LatchCounter (unsigned char_ b_BoardHandle,    |
|                                                    unsigned char_ b_ModulNbr,       |
|                                                    unsigned char_ b_LatchReg)       |
+----------------------------------------------------------------------------+
| Task              : Latch the courant value from selected module           |
|                     (b_ModulNbr) in to the selected latch register         |
|                     (b_LatchReg).                                          |
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_ b_BoardHandle : Handle of board APCI-1710        |
|                     unsigned char_ b_ModulNbr    : Module number to configure       |
|                                           (0 to 3)                         |
|                     unsigned char_ b_LatchReg    : Selected latch register          |
|                               0 : for the first latch register             |
|                               1 : for the second latch register            |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: The selected latch register parameter is wrong     |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_LatchCounter(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_LatchReg)
{
	int i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*************************************/
			/* Test the latch register parameter */
	      /*************************************/

			if (b_LatchReg < 2) {
		 /*********************/
				/* Tatch the counter */
		 /*********************/

				outl(1 << (b_LatchReg * 4),
					devpriv->s_BoardInfos.ui_Address +
					(64 * b_ModulNbr));
			} else {
		 /**************************************************/
				/* The selected latch register parameter is wrong */
		 /**************************************************/

				DPRINTK("The selected latch register parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_	i_APCI1710_SetIndexAndReferenceSource        |
|					(unsigned char_ b_BoardHandle,                |
|					 unsigned char_ b_ModulNbr,                   |
|					 unsigned char_ b_SourceSelection)            |
+----------------------------------------------------------------------------+
| Task              : Determine the hardware source for the index and the    |
|		      reference logic. Per default the index logic is        |
|		      connected to the difference input C and the reference  |
|		      logic is connected to the 24V input E                  |
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_ b_BoardHandle     : Handle of board APCI-1710    |
|                     unsigned char_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
|		      unsigned char_ b_SourceSelection : APCI1710_SOURCE_0 :          |
|						The index logic is connected |
|						to the difference input C and|
|						the reference logic is       |
|						connected to the 24V input E.|
|						This is the default          |
|						configuration.               |
|						APCI1710_SOURCE_1 :          |
|						The reference logic is       |
|						connected to the difference  |
|						input C and the index logic  |
|						is connected to the 24V      |
|						input E                      |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|		      -2: The selected module number is wrong                |
|		      -3: The module is not a counter module.                |
|		      -4: The source selection is wrong                      |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_SetIndexAndReferenceSource(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_SourceSelection)
{
	int i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if incremental counter */
	   /*******************************/

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_INCREMENTAL_COUNTER) {
	      /******************************/
			/* Test if firmware >= Rev1.5 */
	      /******************************/

			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulNbr] &
					0xFFFF) >= 0x3135) {
		 /*****************************/
				/* Test the source selection */
		 /*****************************/

				if (b_SourceSelection == APCI1710_SOURCE_0 ||
					b_SourceSelection == APCI1710_SOURCE_1)
				{
		    /******************************************/
					/* Test if invert the index and reference */
		    /******************************************/

					if (b_SourceSelection ==
						APCI1710_SOURCE_1) {
		       /********************************************/
						/* Invert index and reference source (DQ25) */
		       /********************************************/

						devpriv->
							s_ModuleInfo
							[b_ModulNbr].
							s_SiemensCounterInfo.
							s_ModeRegister.
							s_ByteModeRegister.
							b_ModeRegister4 =
							devpriv->
							s_ModuleInfo
							[b_ModulNbr].
							s_SiemensCounterInfo.
							s_ModeRegister.
							s_ByteModeRegister.
							b_ModeRegister4 |
							APCI1710_INVERT_INDEX_RFERENCE;
					} else {
		       /******************************/**
@verba/
	

Cop/* Set the default configuration (DQ25) tim

 ource //**
@verbat module.

	ADDI-DATA GmbH
	Di/
m

Copydevpriv->m

Copy	s_ModuleInfo833 Otte[bsweierNbr].833 OttersSiemensCounter	Tel	Fax: +49(0ModeRegister	Fax: +49(0Bytei-data-com
	info@addi223/9ata-com
	4 =833 Ottee 3
	D-77833 Ottersweier
	Tel: +19(0)7223/9493-0
	Fax: +49(0)7223/9493-92
	http://www.addi-data-com
	info@addi-data.com

This program is free software; y&833 OtteAPCI1710_DEFAULT_INDEX_RFERENCE;m

Cop}m

Co}yrig if (b_SourceSelecTA Gm==  (at yourSOURCE_0 ||distributed in the hope that it will 1r the s		else {e sourcode of this module.

	ADDI-DATA Gmim

Cop/* The stribu sed in theis wrong the sourcrranty of MERCHANTABILITY or FITNESSm

CopDPRINTK("ARTICULAR PURPOSE. See the GN\n")sion.

i_ReturnValue = -4sion.
rogram is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even } he impliedode of this module.

	ADDI-DATA GmbH
	Dim

CoA PARTImeier
ee tnot a c93-92
e COPYINthe so307 USA

You shoud also find the completeven tld have recei COPYING file accompanying thieral Publi License along wi3sion.This, MA 02111urce code of this module.

	ADDI-DATA GmbH
	Dim

C in the COPYING file accompanying this souDieselstraße 3       D-77833 Ottersweier  |
  ven -------------------------------------------------------+
  | (C) ADDI-DATA}
H          Dieode of this module.

	ADDI-DATA GmbH
	Diim

A PARTICed ined--------numberee the GNU Gen | Internet : http://www.addi-data.com   |
  
ould have receiv------------------------------eral Pub License along wi2;
	}

	rcense  License alon;
}

/*
+-96                       |
  +-------------------------------+-------------+
| Funin theNameource: _INT_	i_pe that itetDigitalChlOnource c Date     : |
|en t   (unsigned char_  b_BoardHandle, | Date     :    :  02/12/2002                |
 3/9493-0)--------------------:  02.96                       |
  +-------------------------------+------------------Task| Date     :  :ght s(C) 20r: Eri output Hght ting an        means-----------ion : s  |
  |         high.---+
  | Description : ription :   APCI-1710 incremental counter module                  |
  |                      In    Parameters                   |
  +---------             of b+---ope t- you               --------------------------       :	N------of(C) 2 COPYINto b    02/12/2		  ADDI-Ded (0thor3----+
  | D  APCI-1710 incremental counter module                  |
  |                      O           UPDATE: -                 |
  +-----------                       APCI-1710 incremental counter module                  |
  |                      icense  along        0: No error                                             |                     -1:PARTIh      p    UPDA Date   ------e the GNU    |
  |          |           | 2 avail--------------------------------           |
  |          |           | 3: 493-92
file initialised see f--------| Date     :  02/12/  " Project maInit493-92
"                             |
  |----------|-----------|------------------------------------------------|
    :int  Project manager: Eric Sto(struc05  medi_device *dev,--------------------------
{
	
/*
+-icense along w0;

	ode of this module.

	ADDI-/     Tes (C) 2--------------      le.

	ADDI-DATA GmbH
	Diesels is di3/9493-0 < 4)2    | Internet : http://www.addi-data.  +-----    ifccompanyi232         -------------------------------+
  | Pr  : A is e 3
	D-77833 rsweier
	Tel7223/9493-0
	Fax:(0)7223/9493-92
	https i_APFlag.b_493-92
	hite ho1-----redistribute it | int	i_APCI1710_InsnConfigIN9(0)7223/9493-92
	http://wwddi-data-com
	info@a-data.com

This program free software;3 = ,
struct comedi_insn *insn,unsigned int *data)

+-----------------------------------------------------------------------+
| T| 0x1     urce code of this module.

	  +-----ht (C) 2       On-----------------------------+
| Ine *soutl----------i_insn *insn,unsigned int *data)

+-----------------------------------------dwdi-data-com
	1_2_3_4,sk        s  +---	Tels	Fax: ui_Address + 20 + (64 *------------l Pub          Dieselstraße 3       D-77833 Ottersweier  |
| Input Pa31 -> 0701/0232                       Input Pa      | See i_APCI1710_DisableFrequen-------------------------------+
  | Tel : +49 (0(0) 7223/9493-0  31 -> 0701/0232                 |
  | Fax : +49 (0) 7223/9493-92    | Internet : http://www.addi-data.com   |
a.com   |
  +-------------------------------          ----------------------------------------+
  | Project   Project     : API APCI1710    | Compiler : gcc     _Current = current           |
  | Module name : INC_CPT.C       | Version  : 2.96                       |
  +-------------------------------+---------------------------------------+
  | Project manager: Eric Stffz   | Date     :  02/12/2002                |
  +-----------------------------------------------------------------------+
  | Description :   APCI-1710 incremental counter module                  |
  |                                          Res                        .chansp|
  |                                            low                  |
  +------------------------------------------------------------------------+
  |                             UPDATES                                   |
  +-----------------------------------------------------------------------+
  |   Date   |   Author  |          Description of updates                |
  +----------+-----------+------------------------------------------------+
  |          |           |                                                |
  |----------|-----------|------------------------------------------------|
  | 08/05/00 | Guinot C  | - 0400/0228 All Function in RING 0             |
  |          |           |   available                                    |
  +-----------------------------------------------------------------------+
  | 29/06/01 | Guinot C. | - 1100/0231 -> 0701/0232                       |
  |          |           | See i_APCI1710_DisableFrequencyMeasurement     |
  +-----------------------------------------------------------------------+
*/

/*
+------------------------ff---------------------------------------------------+
|                               Included files                               |
+----------------------------------------------------------------------------+
*/

#include "APCI1710_INCCPT.h"

/*
+----------------------------------------------------------------------------+
| int	i_APCI1710_InsnConfigINCCPT(struct comedi_device *dev,struct comedi_subdevice *s,
struct comedi_insn *insn,unsigned int *data)

+----------------------------------------------------------------------------+
| Task              : Configuration function for INC_CPT                             |
+------------------------------------------& 0xEF----------------------------+
|  Input Parameters  :					ffevice *dev, struct comedi_subdevic-------------------------------------------------------+
| Output Parameters : *data
+----------------------------------------------------------------------------+
| Return Value      :                 |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_InsnConfigINCCPT(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_ConfigType;
	int i_ReturnValue = 0;
	ui_ConfigType = CR_CHAN(insn->chanspec);

	printk("\nINC_CPT");

	devpriv->tsk_Current = current;	/*  Save the current process task structure */
	switch (ui_ConfigType) {
	case APCI1710_INCCPT_INITCOUNTER:
		i_ReturnValue = i_APCI1710_InitCounter(dev,
			CR_AREF(insn->chan#---------| |
| |b_ModulNbr   APCI1710_16BIT_COUNTER |  The module is confievic833 OttegramINSN WRITEevic-----------| |
| |b_ModulNbr   APCI1710_16BIT_COUNTER |  The module is configured : 2.96                       |
  +-------------------------------+-------------------------------------INT| Project maInsnWriteINCCPT-----------------------------------------sub--------s,
--------------insn *rst ,         
/*
*data----+
  | Descriptio  APCI-1710 incremental counter module                  |
  |                                          Enable DisterMo        s for INC_CPT                                         |
  |----------|-----------|------------------------------------------------|
  |           UPDATES        |
  +----------+-----------+------------------------------------------------+
  |          |           |                                                |
  |----------|-----------|------------------------------------------------|
  | 08/05/00 | Guinot    |
  +-----------------------------------------------------------------------+
*/
/*
+----------terOption           | |
| |                                     |    co	nfigure the first 16 bit ---------- |        |   nterOption   ui_ptionType;                                        = CR_CHAN(rst ->chanspecl Pue 3
	D-77tsk_Current = c      ;  | |Save(C) 2   |  - proc----t    ------uris so
	switch (            -----caseope that i      _ENABLELATCHINTERRUPT:      |
  | Module  Project maunterMLatchI-92
rupt----,e *s              )counAREF  | |
| |       l Pubbreak        b_SecondCounterOpDISon          | |
| |                              dus and|    are not used and have no       | |
| |                                    |    importance.       16BITCOU  | VALUE|                              ption16BAPCI1710_ alonsed and have no       | |
| |                   and have no       | |    [0],2          int------ 1]                                        32               |
|                      Counter ope32ting mode                                |
|                                         0                   |    importance.  tion   any |                    Indexsed a2               |
| |                                    |    importance.         iption     ------------------------------------------nd have no       | |
| |                                    |    importance.  tion  COMPARELOGIC|                                    CompareLogicsed and have no       | |
| |                                    |    importance.         |
| |b_SecondCounterModus|                 dus and  | circuit generates a     | |
| |                    |                         | counting pultion  FREQUENCYMEASUREMEN |                                    FrequencyMeasur223/used and have no       | |
| |                   --------+---------ameter                  |    importance.         ase shifted | |
| |                    |           dus and        | in relation to each     | |
| |                    |      r.            004,200|   printk("ption C ADDI           W_ReturnValu: IN is I1710_QUADRUPL>= 0)                    | |
|n;INC_CPT.C       | Version  : 2.96                       |
  +-------------------------------+---------------------------------------+
                    |    are not u0             |
  |          |           | |                        
  +------------------------------e analysed per        | |
| |         ------------------------------+
  | Description :              |    counter.                       | |
| |                                    |  - b_SecondCounterMoC) 2l               from-----------------                | |
| |-------------------). Each software or hard           occur a  |          |           |         [2], (unsigned char) data[3]);
		br-----------------------------------------------------------------------+
  |                             UPDATES                               |
  +--------------------------|
  |          |           |-+-----------------------      weier
--------to5  ADDI-Druple            | |
| |------------------- |          f updates                                |    counter.                       | |
| |                               -+
  |          |           |                                                |
  |----------|-----------|------------------------------------------------|
  | 08/05/00 | Guinot C  | - 0400/0228 All Function in RING 0             |
  |          |           |   available                                    |
  +------------------------------N| |
mpanying thisfoundn in RING 0             |
  |          |           | /0231 -> 0701/0232                       |
  |          |APCI1710_DIRECT_MODE  | I      | See i_APCI1710_DisableFrequencyMeasurement            |           | 4:           routine701/023stall                 |
  |         |                         | TheSet-------tRlse duDisableFrequencyMeasu    |
  +-----------------------------------------------------------------------+
*/

/*
+----------      |    are not us--------------------------------------------------+
|                               Included files                               |
+----------------------------------------------------------------------------+
*/

#include "APCI1710_INCCPT.h"

/*
+--------------------------------------------------------------------------| int	i_APCI1710_InsnConfigINCCPT(struct comedi_device *dev,struct comedi_subdeviceource code.

@endverbatim  +-----unterMo           source code.

@endverbatimed for--------------------------------------------+
| Output Parameters : *data
+---------------------------------------------+
|2Task              : Configuration function for INC_CPT                             |
+--------------------------                     |ope that ition  _     -+
       Internet : http://www.addi-d  +-----    | unter ADDI-DATA Gm source code.

@endverbatim
*/
/*
-------------------------------------------------------+
| Output Parameters : *data
+----------------------------------------------------------------------------+
| Return Value      :                 |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_InsnConfigINCCPT(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_ConfigType;
	int i_ReturnValue = 0;
	ui_ConfigType = CR_CHAN(insn->chanspec);

	printk("\nINC_CPT");

	devpriv->tsk_Current = current;	/*  Save the current process task structure */
	switch (ui_ConfigType) {
	case APCI1710_INCCPT_INITCOUNTER:
		i_ReturnValue = i_APCI1710_InitCounter(dev,
			CR_AREF(insn->chanspec),
			(unsigned char) data[0],
			(unsigned char) data[1],
			(unsigned char) data[2], (unsigned char) ---------------------+-----------         | are analysed per        | |
| |                    |                         | period                  | |
| |--------------------+-------------------------+-------------------------| |
| |b_FirstCounterModus |   APCI1710_SIMPLE_MODE  | Functions in the same   | |
| |       or        dus and                       | way as the quadruple   | |
| |b_SecondCounterModus|                                                     |
  |----------|-----------|------------------------------------------------|
  || |                    |                         | period.                 | |
| |--------------------+-------------------------+-------------------------| |
| |b_FirstCounterModus |   APCI1710_DIRECT_MODE  | In the direct mode the  | |
| |       or           |                         | both edge analysis      | |
| |b_SecondCounterModus|                         | circuits are inactive.  | |
| |                    |                         | The inputs A, B in the  | |
| |                    |                         | 32-bit mode or A, B and | |
| |                    |                         | C, D in the 16-bit mode | |
| |                    |                         | represent, each, one    | |
| |                    |                         | clock pulse gate circuit| |
| |                    |                         | There by frequency and  | |
| |                    |                         | pulse duration          | |
| |                    |                         | measurements can be     | |
| |                    |                         | performed.              | |
| +--------------------+--------------------+--------------------------------------------------------------+
|                               Included files                               |
+----------------------------------------------------------------------------+
*/

#include "APCI1710_INCCPT.h"

/*
+----------------------------------------------------------------------------+
| int	i_APCI1710_InsnConfigINCCPT(struct comedi_device *dev,struct comedi_subdevice---------------------------          |
|                                                                            |
| +----------------------+-------------------------+------------------------+|
| |       Parametericense,(( (at your       ---------- << 8)-----FF    --        |
| --------------------------+
   |
|Return Value      :e *smdelay(1000 char_-------------------+
| Input Padus and er in direct mode is not possible!                 |
comedi_insn *insn,unsigned int *data)

+----------------------------------------------------------------------------+
|                                          |
|         Counter operating option for quadruple/double/simple mode          |
|         -&ope that i                 char_----------------+-------------------------+------------------------||
| |b_FirstCounterOption  | APCI1710_HYSTERESIS_ON  | In both edge analysis  ||
| |        or            |                         | circuits is available  ||
| |b_SecondCounterOption |                         | one hysteresis circuit.||
| |                      |                         | It suppresses each     ||
| |                      |                         | time the first counting||
| |                      |                         | pulse after a change   ||
| |                      |                         | of rotation.           ||
| |----------------------+-------------------------+-------r operating mode       period                  | |
| |------------------- |
| |                    |                      CounterModus |   APCI1710_DIRECT_MODE  | In the di------------------------------------------         |                  
| |--------------------+------------- b_ted ined493-92
----  -5: The selected first counter operating option is wrong|
int_          alon----+
  |  APCI-1710 incremental counter module                  |
  |                                              | a 16-Bit valong         7: The in--| C) 2  | Comp         |                         | re dis           -6:)   | way as the quadr---------------+------------------------+|
|                                                                            |
|                                                                            |
|       IM    |
  +--------------------------------------------+-------------------------+-----------------------------| |
| |b_First  -5: The selected first counter operating optionrect mode the  | |
| |       oe = 0;

	/*******************************
*/

int i_APCI     ----------------------                -4: The selected first counter operatin(0  | 1----+
  | Description e = 0;

	/***********************            -7: Thif incr       w   |   |
+-      or           |                         | both edge analysis      | |
| |b_SecondCounterModus|                         | circuits are inactive.  | |
| |                    |                         | The inputs A, B in the  | |
| |                    |                         | 32-bit mode or A, B and | |
| |                    |                         | C, D in the 16-bit mode | |
| |                    |                         | represent, each, one    | |
| |                    |                         | clock pulse gate circuit| |
| |                    |                         | There by frequency and  | |
| |                    |                  ----------------------------_Current = current;ent     |
  +-----------------------------------------------------------------------+
*/

/*
+----------r operating mode       --------------------------| |
| |    --------------+------------------            -6: T|
| |                7: The|                               Included files                               |
+----------------------------------------------------------------------------+
*/

#include "APCI1710_INCCPT.h"

/*
+----------------------------------------------------------------------------+
| int	i_APCI1710_InsnConfigINCCPT(struct comedi_device *dev,struct comedi_subdevice -----------------------------eier  |
  +------            | reURPOSE. Se-------------------------------+
  | Tel ed for is dis10_INCREMENTAL_< 2vice * Internet : http://wwte GPL in            |
+- source code.

@endverbatied for -----               							b_SecondCo---------------<< (16 *gram is fr
*/

int i_APCI1            configure (0 to 3)       |
|   8       						APCI1710_SIMPL*----/* Test Return Value      :  n, MA 02111-1307 USA

You shoud also find the compleanspec);

	print+-----------------	APCI1710_DIRECT_MODE
						&& (b_ source code.

@endverbatim
*/
/*
  +--------*/
	switch (ui_Co API APCI1710    | Compi	APCI1710_DIRECT_MODE
						&& (b-------------+
  | (C) ADDIth thiGmbH          Dieselstraße 3       D-77833 Ottersweier  |
nd counter operating |
|                                                   mode.                    |
|                     unsigned char_ b_SecondCounterOption : Second counter  option.  |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      : 0: No error                                            |
|                    -1: The handle parameter of the board is wrong          |
|                    -2: The module is not a co----------------+             |
|                    -3: The selected counter range is wrong.                 |
|          -5: The selected first counter operating option is wrong|
|       |
|                   -5: The selected first counter operating option iULONG_ ul     -7: The selectd second counter operating option is     |
|                        wrong.                                     32       |
+----------------------------------------------------------- COPYIN----------------+|
|                            
	unsigned char b_FirstCounterModus,
	unsigned char b_FirstCounterOption,
	unsigned char b_SecondCounterModus, unsigned char b_SecondCounterOption)
{
	int i_ReturnValue = 0;

	/*******************************/
	/* Test if incremental counter */
	/*******************************/

	if ((devpriv->s_BoardInfos.
			dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF0***************/

		      *******e == APCI1710_32BIT_COUNTER) {
	      /********************************/
			/* Test the first counter modus */
	      /********************************/

			if (b_FirstCounterModus == APCI1710_QUADRUPLE_MODE ||
				b_FirstCounterModus == APCI1710_DOUBLE_MODE ||
				b_FirstCounterModus == APCI1710_SIMPLE_MODE ||
				b_FirstCounterModus == APCI1710_DIRECT_MODE) {
		 /*********************************/
				/* Test the first counter option */
		 /*********************************/

				if ((b_FirstCounterModus == APCI1710_DIRECT_MODE
						&& (b_FirstCounterOption ==
							APCI1710_INCREMENT
							|| b_FirstCounterOption
							== APCI1710_DECREMENT)ounterOption ==
							APCI1710_HYSTERESIS_ON
							|| b_FirstCounterOption
							==
							APCI1710_----------------+-				{
		    /**************************/
					/* Test if 16-bit     *******/

					if (b_CounterRange ==
						APCI1710_16BIT_COUNTER) {
		       /*********************************/
						/* Test the second counter modus */
		       /*********************************/

						if ((b_FirstCounterModus !=
								APCI1710_DIRECT_MODE
								&&
								(b_SecondCounterModus
									==
									APCI1710_QUADRUPLE_MODE
									||
									b_SecondCounterModus
				     |
|          =
								ModeRegister1 |
						APCI17---------irect mode */------------------------------------------4on           ||
| |----------------------+-------------------------+------------------------||
| |b_FirstCounterOption  | APCI1710_HYSTERESIS_ON  | In both edge analysis  ||
| |        or            |                         | circuits is available  ||
| |b_SecondCounterOption |                         | one hysteresis circuit.||
| |                      |                         | It suppresses each     ||
| |                      |                         | time the first counting||
| |                      |                         | pulse after a change   ||
| |                      |                         | of rotation.           ||
| |----------------------+-------------------------+-------| |--------2                |
  +--------------  -5: The selected first counter operating option i---------------------------------+
  | |
| |b_FirstCounterModus |   APCI1710_SIMPLE_MODE  | Functions in the same   | |
| |       or           |        any  a   | |
g mode is wrong */
		 /******************************************************/
				DPRINTK("The selected first counter operating mode is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /***************************************/
			/* The selected counter range is wrong */
	      /***************************************/

		Infos.
			dw_MolduleConfigurat         |                         | both edge analysis      | |
| |b_SecondCounterModus|                         | circuits are inactive.  | |
| |                    |                         | The inputs A, B in the  | |
| |                    |                         | 32-bit mode or A, B and | |
| |                    |                         | C, D in the 16-bit mode | |
| |                    |                         | represent, each, one    | |
| |                    |                         | clock pulse gate circuit| |
| |                    |                         | There by frequency and  | |
| |                    |                      |
01/0232                       |
  |                   |                         | meaedi_     DisableFrequencyMeasurement            |                         | performed.              | |
| +--------------------+-------------------------------------------------------------------------+
|                           |
| |         l_are not u|    Reg					APCI1710_16BIT_COUNTER) {
		       /*********************************/
						/* Test the second counter modus */
		       /*********************************/

						if ((b_FirstCounterModus !=
								APCI1710_DIRECT_MODE
								&&
								(b_SecondCounterModus
									==
									APCI1710_QUADRUPLE_MODE
									||
									b_SecondCounterModus
									==
								APCI171if i     -------------------f counter 3 |             |
|      ==
					,
struct comedi_insn *insn,unsigned int *data)

+----------------vice *dev,str     edi_vice *sedistribute it ai_insn *insn,unsigned int *dat9(0)7223/9493-92
	http://wwwddi-data-com
	info@ad-data.com

This program i  |
|                           _BoardHandle : Handle of board APCI-1710      |  |
+--------------------------------------------------------------------------------------- any char_ 	    |    0010       you can in-------------re (0 to 3)       |
|  m

Cop2***************/

			out					&&
		---------------------------------------APCI-1710      |  |
+---------------------------------------------------**********************************/
					         			  /**********************************/

							if ((b_SecondCounterModus == APCI1710== APCI1710_DIRECT_M      |
|              |    0000         |
|              MENT || b_SecondCounterOption == APCI1710_DECREMENT)) || EMENT)) || (b_SecondCounter      |
|             terOption == APCI1710_HYSTERESIS_ON || b_SecondCounterOption == APCI1710_HYSTERESIS_OFF))) {
								i_ReturnValue =
									0;
							} else {
			     /*********************************************************/
								/* The selected second counter operating option is wrong */
			     /*********************************************************/

								DPRINTK("The selected second counter operating option is wrong\n");
								i_ReturnValue =
									-7;
							}
						} else {
			  /*******************************************************/
							/* The selected second counter operating mode is wrong */
			  /*******************************************************/

							DPRINTK("The selected second counhe quadruple2                |
  +-------------             (unsigned char_     b_BoardHandle,    |
|-------------------------------+
                | after a change of      ||
| |                      |                         | rotation.          -----------------+
| Task              : A est mode is intended for testing the component and  |
|                     the connected periphery. All the 8-bit counter chains  |
|                     are operated internally as down counters.              |
|                     Independently from the external signals,               |
|                     all the four 8-bit counter chains are decremented in   |
|                     parallel by each negative clock pulse edge of CLKX.    |
|                                                                            |
|                       Counter auto test conclusion                         |
|                       ----------------------------                         |
|              +-----------------+-----------------------------+             |
|              | pb_TestStatus   |    Error description        |             |
|              |     mask        |                             |             |
|              |-----------------+-----------------------------|             |
|              |    0000         |     No error detected       |             |
|              |-----------------|-----------------------------|             |
|              |    0001         | Error detected of counter 0 |he quadruple --------------------------------------------------+
|                               Included files                               |
+----------------------------------------------------------------------------+
*/

#include "APCI1710_INCCPT.h"

/*
+----------------------------------------------------------------------------+
| int	i_APCI1710_InsnConfigINCCPT(struct comedi_device *dev,struct comedi_subdevice d of counter 3 |             |
|              +-----------------+-----------------------------+             |
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_   b_BoardHandle : Handle of board APCI-1710      |  |
+----------------------------------------------------------------------------+
| Output Parameters : unsigned char *_ pb_TestStatus  : Auto test conclusion. See table|
+--------------------------------------cense, counter  option. ------------         -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_CounterAutoTest(struct comedi_device *dev, unsigned char *pb_TestStatus)
{
	unsigned char b_ModulCpt = 0;
	int i_ReturnValue = 0;
	unsigned int dw_LathchValue;

	*pb_TestStatus = 0;

	/*********************************/
	/* Test if counter module found */
	/********************************/

	if ((devpriv->s_BoardInfos.
			dw_MolduleConfiguration[0] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[1] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[2] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[3] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER) {
		for (b_ModulCpt = 0; b_ModulCpt < 4; b_ModulCpt++) {
	      /*******************************/
			/* Test if incremental counter */
	      /*******************************/

			if (        | circuit ****************/

		if (b_CounterRange == A	} else {
		    /************************        | period                | |
| |--------------------+-----------------------------+
  | Description :                 unsigned char *_   pb_TestStatus)     |
+-----------------------------------------------------------*******c | cir luit . A (C)at moatio------r a index signal, the counC) 2incelatioalOUBLE_MODarridCou--------------  |
+-y  | |
| |                    |     = cugenerated----+|
|                         ------------------------------------------------------------+
  |                             UPDATES                                   |
  +-----------------------------0UL) ==
				APCI1710_INCREMENTAL_COUNTER) {
		                  Independently from the external signals,               |
|                     all the four 8-bit counter chains are decremented in   |
|                     parallel by each negative clock pulse edge of         |                         | The inputs A, B in the  | |
| |                    |                         | 32-bit mode or A, B and | |
| |                    |                         | C, D in the 16-bit mode | |
| |                    |                         | represent, each, one    | |
| |                    |                         | clock pulse gate circuit| |
| |                    |                         | There by frequency and  | |
| |                    |                    | cir------ |
|            ----+|
|                       |                  Shar *pb_TestStatus)
{
	unsig  | circuit DisableFre         |               5         | p                                  See tab                 |
|                     unsigned chasurements can be  XDisable         |                         | performed.              | |
| +--------------------+-----------------  | circuit g|
|                                                 unsigned char_ b_ReferenceAction,   |
|                                                 unsigned char_ b_IndexOperation,    |
|                                                 unsigned char_ b_AutoMode,          |
|                                                 unsigned char_ b_InterruptEnable)   |
+----------------------------------------------------------------------------+
| Task        |
|              +-------------------------+-----------------------------+             |
      |
+--------------------------------------------------------------------------      |ce *dev,struc| circuit edi_subdevice *signed char_   b_BoardHandle : Handle of board APCI-1710      |  |
+------------------------------------------------------------------- Task             --------------------------+
| Output Parameters : -                                                      |
+-------------|       |
|     -------|
| |b_---------- | Internet : http://www.addi-dnterModus
								=                    dle parameter of the board is wro If you have enabled the automatic mode, each INDEX     |
|                     action is cleared automatically, else you must read    |
|                     the index status ("i_APCI1710_ReadIndexStatus")        |
|                     aft_DIRECT_M                             ar b_ModulCpt = 0;
	int i_ReturnValue = 0----------------                             ***********************/
	/* Test if counter module found */
	/********************************/

	if ((devpriv->s_BoardInfos.
			dw_MolduleConfiguration[0] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[1] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[2] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[3] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER) {
		for (b_ModulCpt = 0; b_ModulCpt < 4; b_ModulCpt++) {
	      /*******************************/
			/* Test if incremental counter */
	      /*******************************/

			if ((devpri        |----------------------alysed per        | |
| |                    |     |
  +------------------------------r a index signal, the counter  |  |
|           |                        | value is cleared (32-Bit)          |  |
|           +------------------------+------------------------------------+  |n.          ---------------------r        : Module number to configure   |
|                                               (0 to 3)                     |
|                     unsigned char_ b_ReferenceAction : Determine if the reference   |
|                                               must set or no for the       |
|                                               acceptance from index        |
|                                               APCI1710_ENABLE :            |
|                                                  Reference must be set for |
|                                                  accepted the index        |
|                                               APCI1710_DISABLE :           |
|                                                  Reference have not        |
|                                                  importance                |
|                     unsigned char_ b_IndexOperation  : Index operating mode.        |
|                                               See table.                   |
|                     unsigned char_ b_AutoMode        : Enabl_ b_BoardHandle,       |
|                                                 unsigned char_ b_ModulNbr,    
|                                               APCI1710_DISABLE :           |
|                                                 Disable the automatic mode |
|                     unsigned char_ b_InterruptEnable : Enable or disable the        |
|                                               interrupt.                   |
|                                               APCI1710_ENABLE :            |
|                                               Enable the interrupt         |
|                                               APCI1710_DISABLE :           |
|                                               Disable the interrupt        |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                              |
|                   -1: The han parameter of the board is wrong         |
|                     -2                                                  |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4  The reference action parameter is wrong            |
|                     -5: The index operating mode parameter is wrong        |
|                     -6: The auto mode parameter is wrong                   |
|                     -7: Interrupt parameter is wrong                       |
|                     -8: Interrupt function not initialised.                |
|                         See function "i_APCI1710_SetBoardIntRoutineX"      |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_InitIndex(struct comedi_device *dev,
	unsigned char b_ModulNbr,
	unsigned char b_ReferenceAction,
	unsigned char b_IndexOperation, unsigned char b_AutoMode, unsigned char b_InterruptEnable)
{
	int i_ReturnValue 	/*-----.96                       |
  +-------------------------------+-----------------2: he four edges   | |
| |                    |         | in relatioour edges   | _ModeRt if counter initialised */
	   /*******************************/

		if (devpriv->
						devpriv->
									s_Modulerst counter operating option is wrong _SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegistare not uunterM----+
  | Descriptinfo.
			s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeR     |  - b_SecondCounterM      f       |    gister4
	          |
  |      ***********/
							/* Test if latch and clear counter */
			     /************************           UPDATES                                   |
  +----------------------------						devpriv->
							 the reference   |
|               
  |   Date   |   Author  |    o.
									s_ModeRegister.
						_ModuleInfo
			escription of updates          					/* Set the latch and clear flag (DQ2
			     /*****CounterMoor d.          ounterInfo.
									s_ModeRegister.
						                  |                         ****/

								devpriv->
									s_ModuleInfo
				|              :					[b_ModulNbr].
									s_SiemensCounterInfo.
									s_Mod   |                 [b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister       					s_ByteMo****/

								devpriv->
									s_ModuleInfo
				n.          10_ENABLE_LATCH_A***********/
							/* Test if latch and clear counter */
			     /************************-+
  |          |           |                                                *********/
							/* Test if latch and clear counter */
			     /************************ 08/05/00 | Guinot C  | - 0400/0228 All Function in RING 0             |
  | 						devpriv->
									s   available                                    |
  +----									=
									devpriv--------------------------------------------+
  | 29/06/									=
									devpriv/0231 -> 0701/0232                       |
  |          						devpriv->
									s_      | See i_APCI1710_DisableFrequencyMeasurement   									=
									devpriv4:         | ==
								A                             ***********************/
				             unsigned char_ b        | in relatio"************/

								if (b_I |
|         T_MODE
						&& (b_Firs				b_ModeRegister4
									=
									devpriv6|
|                                               autom*********/
							/* Test if latch and clear counter */
			     /********************-------+-----------------        | in relation				{
		    /**************************/
					/* Test if 16-bit counte
			     /****** :           |
|                                                 Disable the automatic mode |
|                     unsigned char_ b_InterruptEnable : Enable or disable the        |
|                                               interrupt.                   |
|                                               APCI1710_ENABLE :            |
|                                               Enable the int|
|              +-----Operation ==gister4
	                                   APCI1710_DISABLE :                  |
|                                               Disable the interrupt        |
+-------        | in relatioedi_subdevice *vel latch or/and clear */
			     /***CI1710_DO          modis source code.

@endverbatim
*/
/*
        is (								s_Siemense hope that i       ) |     |
 if (b_IndexOperation == APCI17tion  ) |b_		------                   APCI1710_DISABLE :      SS FOR A P   |       Operation ==
								Athe source code of this module.

	ADDI-DATA GmbH
ed for  e 3
	D-77833 Otthe handle parameter of the boar9(0)7223/9493-92
	http://www.ddi-data-com
	info@add-data.com

This program is|
+------------------------------br].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									=     |

|               ase shifthe handle Save the current process task structure */
	switSS FOR A Pdus and or eutoMode ==
								Aer in direct mod			|
									APCI1710_INDEX_AUTO_MODE;
							} else fo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									=
	---------------v->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRecense, ocounter  option. ase shift-+
 n == A		|st c (b_IndexOperati<< 3nge    			|
									APCI1710_INDEX_AUTO_SS FOR A P    |
|                     -2: N: No counter module found             you have enabled the automatic mode, each INDEfo.
									s_ModeRegister.
									s_ByteModeRego counter module found            configure (0 to 3)m

Copy
+---------------------          unsigned char_ 								[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
		ce *dev,sodeRegist        | in relatioOperatiom

Copy1h this  MA 02111-1 | Internet : http://www.addi-data.cSS FOR A Pble the latch from counter */ General Public License for more details.
You should have rble the latch from counter *eral Public License along wi5h this ********************/

							if ((b_SecondCounterModus == APCI1_DIRECT_MxOperation == APCI1710_HIGH_EDGE_LATCH_COUNar b_ModulCpt = 0;
	int i_ReturnValue = 0;
	unsigned in----------------xOperation == APCI1710_HIGH_EDGE_LATCH_COUN**************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									|
									APCI1710_SET_LOW_INDEX_LEVEL;
							} else {
				/**************************************/
								/* Set the index level to high (DQ26) */
				/**************************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.------| |
| |b_FirstCounter								=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
				         | value is cleared *********/
							/* Test if latch and clear counter */
			     /***********************************/

						dus and_IndexOperation ==
								APCI1710_HIGH_EDGE_LATCH_AND_CLEAR_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOW_EDGE_LATCH_AND_CLEAR_COUNTER)
							{
				/***************************************/
								/* Set the latch and clear flag (DQ27) */
				/***************************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_Siemelse {
				/*****************************************/
								/* Clear the latch and clear flag (DQ27) */
				/*****************************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									&
									APCI1710_DISABLE_LATCH_AND_CLEAR;

				/*************************/
								/* Test if latch counter */
				/*************************/

								if (b_IndexOperation == APCI1710_HIGH_EDGE_LATCH_COUNTER || b_IndexOperation == APCI1710_LOW_EDGE_LATCH_COUNTER) {
				   /************************										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_------| |
| |b_FirstCounterM                                    APCI1710_DISABLE :           |
|                                                 Disable the automatic mode |
|                     unsigned char_ b_InterruptEnable : Enable or disable the        |
|                                               interrupt.                   |
|                                               APCI1710_ENABLE :            |
|                                               Enable the int	s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister2
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister2
		= APCI1710_DIRECT_Mn.          
								APCI1710_DISABLE)
	ode of this module.

	ADDI-DATA GmbH
	odeRegi=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									&
									APCI1710_DISABLE_INDEX_ACTION;
							}

			     /****************************************/
							/ase shiftS FOR A P Begin CG 29/06/01----1100/0231 -> 0701----2	s_ByteMoe ==
				 IRQ mustr  |cleaion mensCount counter  option. 		if (b_Intersion.
----End------------------------+
| Output Parameters : -                               er.
										b_ModeRegister2
										&*************************************/

							if (b_IndexOperation ==
								APCI1710_LOW_EDGE_LATCH_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOW_EDGE_CLEAR_COUNTER
								||
								b_IndexOperation
								==
							----------------------------------------------------------+
| Input Parameters  : unsigned char_ b_BoardHandle     : Handle of board APCI-1710    |
|                     unsigned char_ b_ModulNbr        : ModuleLE_INDEX_INT;
						} else {
								devpriv-     Nbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister3
									&
									APCI1710_DISABLE_INDEX_INT;
							}

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_InitFlag.
								b_IndexInit = 1;

						} else {
			  /********************************/
							/* Interrupt parameter is wrong */
			  /********************************/
							DPRINTK("Interrupt parameter is wrong\n");
							i_ReturnValue = -7;
						}
					} else {
		       /************************************/
						/* The auto mode parameter is wrong */
		       /************************************/

						DPRINTK("The auto mode parameter is wrong\n");
						i_ReturnValue = -6;
					}
				} else {
		    /********************************-----------| |
| |b_ModulNbr   APCI1710_16BIT_COUNTER |  The module is configured for     | |
| | READ                               |  two 16-bit counter.              | |
| |                                    |  - b_FirstCounterModus and        | |
| |                                    |    b_FirstCounterOption           | |
| |                                    |    configure the first 16 bit     | |
| |                                    |    counter.                       | |
| |                                    |  - b_SecondCoRead and Ge	APCI1710_|
| |                                    |    b_SecondCounterOption          | |
| |                                    |    configure the second 16 bit    | |
| |                                    |    counter.                       | |
| |------------------------------------+-----------------------------------| |
| |b_ModulNbr   APCI1710_32BIT_COUNTER |  The module is configured for one | |
| |                                    |  32-bit counter.                  | |
| |                                    b_Mob_FirstCounterModus and        | |
| |                                    |    b_FirstCounterOption           | |
| |           b_Mo                    |    configure ********t counter.  | |
| |                                     |  - b_SecondCounterModus and       | |
| |                      ********    |    b_SecondCounterOps_By     REGISTERSTATUS|                              b_Mo0       com
	Status                          |
|                      ------------------CR_RANGE         |                       *) & | other.                  | |
| |--------rameter is wrong      |
|                      Counte*****************                               |
|                      ------------------urnValue = -2;
	}

	return i_ReturnV |   }

/*
+--------r       |     ********/00 | G%lNbr,  | other.                  | |
| |--------s_By                 |
|                      Counteb_Morating mode                                |
|                      ------------------Handle,                |
|					 unsigned char_ b_ModulNbxternalStrobe,             |
|				                                            |
| b_Mo----------------+-------------------------+-------------------------+ |
| |    P;
}

/*
+--------------------------------------GET any */
	   /*****************************Ge      ***/

		DPRINTK("The selected module number parameter in i_ReturnValue;
}

/*
+--------------------------------------GETREter ver of board APCI-1710    |
|              Refers :   unsigned char_ b_ModulNbr        : Module number to configure   |
|                                               (0 tUAS of board APCI-1710    |
|              |		  unsigned char_ b_ModulNbr        : Module number to configure   |
|                                               (0 tCB of board APCI-1710    |
|              CB: External strobe level    |
|						APCI1710_LOW :               |
|						External latch occurs if "0" |
|						APCI1710      HIGH :              |
|						External latratingh occurs if "1" |
+-------------------------------------nd have no       |d char_ b_Moconfigure   |
|                  |
|                            |
|D      unsigned char_ b_ExternalStrobeLeveD: External strobe level    |
|						APCI1710_LOW :               |
|						External launsigned char_ b_BoardHandle     : Han | |
| Ueter iE     |
|                     -1: The handare not uUD|    ed: External strobe level    |
|						APCI1710_LOW :               |
|						External latch occurs if "0" |
|						APCI1s_Byase shifted | |
| |                    |           b_Mo        | in relation to each     | |
| |                    |       -----------------------+
| Retu-----------------------+
| R1                d char_ b_2    -4: External strobe selection is wro   | |
| |   ----   Task        ----            UPDATfigINCCFIFOb_ExternalStrobe, u[-----------+
| b_ExternalStrobe, un*/
		/*].b_Oldweier
Maskefer
*/

inunsigned char b_ExternalStrobe, unsigned char b_ExternalStrobeLevel)
{
	int i_ReturnValue = 0;

	/**********ul****b_Externa*****/
	/* Te2t the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
493-92
|     Versio +------+
*/

#include "APCI1710_INCCIput Para+
| Ir_Mod cha      TCH_COUNTER);
								}
							}	vel)
{
	int i_ReturnValue = 0;

	/*fere*/
		/*				s_Siemens	      /**************************+****%ope that itAV-1: T| |
| odule number i| |       or       RINTK("Co          v->
								 as the quadruple    | |
| |b_SecondCounterModus|                         | mo            |                         | of rotation.           ||
| |----------------------+-------------------------+-------********************/


| |                    |                   if counter initialised */
	   /****************************ation[b_ModulCpt] &
					0xFFFF0000UL) ==
				APCI1710_INCREMENTAL_COUN |
|             ->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_Mod0       r.
						s_ByteModeRegister.
						b_ModeRegister4 = (devpriv->
						s_Mo *_ pInfo[b_: Exte)odeRegister.
						b_ModeRegister2 &
						APCI1710_REFERENCE_LOW;
				}

				outl(devpriv->s_ModuleInfo[b_Mod          r      |
s						  | way as the quadruple| |
| |b_SecondCounterModus|        dulNb  | Compi));
				} else (Info[b_Mod). | |
| |                    |                         | is analysed per         | |
| |                    |                         | period.                 | |
| |--------------------+-------------------------+-------------------------| |
| |b_FirstCounterModus |   APCI1710_DIRECT_MODE  | In the direct mode the  | |
| |       or    = 0;

	/*******************************Return Va       robe level parameter i----------|--------------------------------0 :
| | 
| Inirst\n");
				i_ReturnValue =						s_ByteModeRegister.
						b_Mod1== 0 || b_Esecong\n");
				i_ReturnValue =counter chains are decremented in   |
|                     parallel by each negative clock pulse edge of			s_ByteModeRegister.
						b_t C        		} else {
		  TER || b_I         -4: The selected first counter operatin == Nocept that onl       |
|                    -3: The selected counter range****A          **************alue = -3;
		}
	} else {
	   /********************2g */
mode, except that onl*********************************/

		DPRINTK("The s3g */
	   /****ulNbmode, exc				s_ByteModeRegister.
						b_ModeRegister4 = (devpri**********************dus == APCI1710_QUADRUPLE_MODE ||
				b_FirstCounterModus == APCI1710_DOUBLE_MODE ||
				b_FirstCounterModus == APCI1710_SIMPLE_MODE ||
				b_FirstCounterModus == APCI1710_DIRECT_MODE) {
		 /*********************************/
				/* Test the first counter option */
		 /*********************************/

				if ((b_FirstCounterModus == APCI1710_DIRECT_MODE
						&& (b_FirstCounterOption ==
							APCI1710_INCREMENT
							|| b_FirstCounterOption
							== APCI1710_DECREMENT))
					|| (b_FirstCounterModus !=
					));
				} else T_MODE
						&& (b_FirstCounterOption ==
							APCI1710_HYSTERESIS_ON
							|| b_FirstCounterOption
							==
							APCI********************/

					{
		    /**************************/
					/* Test if 16-bit countefo[b_Modul			s_ByteModeRester.
						b_M--------------|             |
|          dw---------                                        Disable the automatic mode |
|                     unsigned char_ b_InterruptEnable : Enable or disable the        |
|                                               interrupt.                   |
|                                               APCI1710_ENABLE :            |
|                                               Enable the interrupt       robe));
				} else           -------------------------------+
  | Tel : +49 (+---------trobe selecE_MODE))

				Return Val   :  0: No error          m

Cop-------------	s_ModeRegister.
									   | Input Parv->
				------+---------((lue      :  >>-------+
	   _DIRECT_M4))-----NABL*******************/

							if ((b_SecondCounterModus == APCI1710_DIRECT_MODE && (b_Secthe  |
	   |                     iMENT || b_SecondCounterOption == APCI1710_DECREMENT)) || (b_SecondCounterModus != APCIthe  |
	   |                     terOption == APCI1710_HYSTERESIS_ON || b_SecondCounterOption == APCI1710_HYSTERESIS_OFF))) {
								i_ReturnValue =
									0;
							} else {
			     /*********************************************************/
								/* The selected second counter operating option is wrong */
			     /*********************************************************/

								DPRINTK("The selected second counter operating option is wrong\n");
								i_ReturnValue =
									-7;
							}
						} else {
			  /*******************************************************/
							/* The selected second counter operating mode is wrong */
			  /*******************************************************/

							DPRINTK("The selected second counme     : _INT_	i_APCI1             |
|                    -3: The selected counter ranged */
	   /************ |
  +----------->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_M_ModeRegister.
	ction */
			/* "i_APCI1710_InitCounter"             */
	      /*************fo[b_ModulNbation[b_ModulCpt] &
					0xFFFF0000UL) ==
				APCI1710_P*******pul = -3;7: The eRegister4 & (0xFF -
							(0x10 << b_ExternalStrobe))) | ((b_ExternalStrobeLevel ^ 1) << (4 + b_ExternalStrobe));
				} else   |
+-  | way as the quadruple **********************/
					/* External strobe level parameter is wrong */
		    /********************************************/

					DPRINTK("External strobe level parameter is wrong\n");
					i_ReturnValue = -5;
				}
			}	/*  if (b_ExternalStrobe == 0 || b_ExternalStrobe == 1) */
			else {
		 /**************************************/
				/* External strobe selection is wrong */
		 /**************************************/

				DPRINTK("External strobe selection is wrong\n");
				i_ReturnValue = -4;
			}	/*  if (b_ExternalStrobe == 0 || b_ExternalStrobe == 1) */
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /**************************************/
		/* The sele : initialised\n")PCI1710_32BIT_C  | Function Name     : _INT_ i_APCI1710_InitCompareLogic                      |
	   |                               (unsigned char_   b_BoardHandle,                      |
	   |                                unsigned char_   b_ModulNbr,                         |
	   |                                unsigned int_  ui_CompareValue)                     |
	   +----------------------------------------------------------------------------+
	   | Task              : Set the 32-Bit compare value. At that moment that the  |
	   |                     incremental counter arrive to the compare value        |
	   |                     (ui_CompareValue) a interrupt is generaFF)))
				{
		    /**************************/
					/* Test if 16-bit counte-------------------_Init
		/* The selec :           |
|                                                 Disable the automatic mode |
|                     unsigned char_ b_InterruptEnable : Enable or disable the        |
|                                               interrupt.                   |
|                                               APCI1710_ENABLE :            |
|                                               Enable the interrupt       meters : -
	   +----------------------------------------------------------------------------+
	   | Return VaPC has a PCI bu 0: No error                                     ------+
	    b_Ex option */
			  /***************     -2: No counter module found                            |
	   |                     -3: Counter not initialised see function               |
	   |                         "i_APCI1710_InitCounter"                           |
	   +----------------------------------------------------------------------------+
	 */

int i_APCI1710_InitCompareLogic(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned int ui_CompareValue)
{
	int i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {

			outl(ui_CompareValue, devpriv->s_BoardInfos.
				ui_Address + 28 + (64 * b_ModulNbr));

			devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.unter module                  |
|} else {
	   /*****************************/* Counter not initialised see fun         |
|                    -3: The selected counte	      /*************************ealTimingInterval;
	unsigned int dw_Status = 0;

	/*************************            -6: ThengInterval;
	unsigned int dw_Status = 0;

	/*************_Init	s_Mpuiruct com7: The selecte**************/
				/* Start the test */
		 /******************/

				outl(3, devpriv->s_BoardInfos.
				      ------------------------------+
*/

int i_APCI17
		i_ReturnValue = -2;
	}  | way as the quadru					/* Exter----------xternalse {
	      /***************));
				} else ulNbC_CPT.C         ederval ----+|
|    
	unsigned char b_FirstCounterModus,
	unsigned char b_FirstCounterOption,
	unsigned char b_SecondCounterModus, unsigned char b_SecondCounterOption)
{
	int i_ReturnValue = 0;

	/*******************************/
	/* Test if incremental counter */
	/*******************************/

	if ((devpriv->s_BoardInfos.
			dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER) {
	   /**************************/
		/* Test the counter range */
	   /**************************/

		isee function */
			/* "i_APCI1710_InitCounter"             */
	      /**************************************** /*********************ounterRang    | rePCI1710_32BIT_ == APCI1710_QUADRUPLE_MODE ||
				b_FirstCounterModus == APCI1710_DOUBLE_MODE ||
				b_FirstCounterModus == APCI1710_SIMPLE_MODE ||
				b_FirstCounterModus == APCI1710_DIRECT_MODE) {
		 /*********************************/
				/* Test the first counter option */
		 /*********************************/

				if ((b_FirstCounterModus == APCI1710_DIRECT_MODE
						&& (b_FirstCounterOption ==
							APCI1710_INCREMENT
							|| b_FirstCounterOption
							== APCI1710_DECREMENT))
					|| (b_FirstCounterModus !=
						APCI1710_DIRECT_MODE
						&& (b_FirstCounterOption ==
							APCI1710_HYSTERESIS_ON
							|| b_FirstCounterOption
							==
							APCI
+--------------------				{
		    /**************************/
					/* Test if 16-bit counter */
		    /************************************meters  : unsigned char_  b_BoardHandle    : HahalTimingIn						APCI1710_16BIT_COUNTER) {
		       /*********************************/
						/* Test the second counter modus */
		       /*********************************/

						if ((b_FirstCounterModus !=
								APCI1710_DIRECT_MODE
								&&
								(b_SecondCounterModus
									==
									APCI1710_QUADRUPLE_MODE
									||
									b_SecondCounterModus
									==
									APCI1710_DOUBLE_MODE
									||
									b_SecondCounterModus
									==
									APCI1710_SIMPLE_MODE))
							|| (b_FirstCou.
									      /***    | re                                  you ha1-----------------------------                     |
	   |          char_ b_BoardHandle     :_DIRECT_MnalStrobe));
		=
								APCI1710_DIRECT_MODE
	of board APCI553500UL))
					 No error                                                  |
|            al >=
								200he handle parametecondCou***************ong 710_DIRECT_MODE))
						{
			  UTO_MODE    FFU**********************/

							if ((b_SecondCounterModus == APCI1710_DIRECT_MODE && (b_SecondCounterOption == APCI1710_INCREMENT || b_SecondCounterOption == APCI1710_DECREMENT)) || (b_SecondCounterModus != APCI1710_DIRECT_MODE && (b_SecondCounterOption == APCI1710_HYSTERESIS_ON || b_SecondCounterOption == APCI1710_HYSTERESIS_OFF))) {
								i_ReturnValue =
									0;
							} else {
			     /*********************************************************/
								/* The selected second counter operating option is wrong */
			     /*********************************************************/

								DPRINTK("The selected second counter operating option is wrong\n");
								i_ReturnValue =
									-7;
							}
						} else {
			  /*******************************************************/
							/* The selected second counter operating mode is wrong */
			  /*******************************************************/

							DPRINTK("The selected second coun---------------------COUNTER  | After a index signal, the counter  |  |
|signed int ul_TimerValue = 0;
	double d_RealTimingInterval;
	unsigned int dw_Status = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if*******/
		/********************/


		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /***---------ENTAL_  | way as the quadruple    ;
		i_ReturnValue = -2;
	}

	return i_R_PCIInputClock =CIInputClock == APCI1710_3-----------------------+
| I) ||
				(b_PCIInputClockde is wrong */
		 /******************************************************/
				DPRINTK("The selected first counter operating mode is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /***************************************/
			/* The selected counter range is wrong */
	      /***************************************/

			DPRINTK("The selected counter rement.           |
|		      Configures the selected TOR incremental counter of the |
|		      selected module0x3131) {

				/*

	   /***Clock ==
								APC*/
	      /**************************/

			if (b_CounterRange == APCI1710_32BIT_COUNTER) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister1 = b_CounterRange |
					b_FirstCounterModus |
					b_FirstCounterOption;
			} else {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister1 = b_CounterRange |
					(b_FirstCounterModus & 0x5) |
					(b_FirstCounterOption & 0x20) |
					(b_SecondCounterModus & ----------------------unterOption & 0x40);

		 /***********************/
				/* Test if dPC ha
								200)
							&& (ul_TimingIntervction,   |
|                                                 unsigned char_ b_IndexOperation,    |
|                                                 unsigned char_ b_AutoMode,          |
|                                                 unsigned char_ b_InterruptEnable)   |
+----------------------------------------------------------------------------+
| Tas  +------*********************			 |
+--------------------------------1.5 */
			  /*****************                    |
	   |        						s_SiemensCounterInfo.
			  +-----n[b_ModulNbr] & 0xFFFF) >putClock)) >= ((double)((double)u    ***********************/
								/* Test if 40MHz q/***************************/

			outl(devpriv->s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				dw_ModeRegister1_2_3_4,
				devpriv->s_BoardInfos.
				ui_Address + 20 + (64 * b_ModulNbr));

			devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_CounterInit = 1;
		}
	} else {
	   /**************************************/
		/* The module is not a counter module */
	   /**************************************/

		DPRINTK("The module is not a counter module\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_CounterAutoTest                       unsi/
	   /**************************ation[b_ModulCpt] &
					0xFFFF0000UL) ==
				APCI1710_					s_ByteModeRegister.
								gInterval
										-
										1;
									ul_TimerValue
										=
Registe       unsi)eRegister.
						b_ModeRegister2 &
						APCI1710_REFERENCE_LOW;
				}

				outl(devpriv->s_ModuleInfo[b_710_33MHZ------{
		   ----+
| Task              : A test mode is intended for testing the component and  |
|                     the connected periphery. All the 8-bit counter chains  |
|                     are operated internally as down counters.              |
|                     Independently from the external signals,               |
|                     all the four 8-bit counter chains are decremented in   |
|                     parallel by each negative clock pulse edge of***/
									/* æs */
				/**     *******------****************/
		/* The selected module number parameter is wrong */
/
					/**************                                   |
|                       Counter auto test conclusion                         |
|                       ----------------------------                         |
|              +-----------------+-----------------------------+             |
|              | pb_TestStatus   |    Error description        |             |
|              |     mask        |                             |             |
|              |-----------------+-----------------------------|             |
|              |    0000         |     No error detected       |             |
|              |-----------------|-----------------------------|             |
|              |    0001         | Error detected of counter 0 |          unsig				{
		    /**************************/
					/* Test if 16-bit coun	    */
				/***meters  : unsigned char_  b_BoardHandle    :: Exte :  0:	fpu_begin();
								switch (b_TimingUnity) {
				/******/
									/* ns */
				/******/

								case 0:

					/******************/
									/* Timer 0 factor */
					/******************/

									ul_TimerValue
										=
										(unsigned int)
										(ul_TimingInterval
										*
										(0.00025 * b_PCIInputClock));

					/*******************/
	 |
|              +-----------------+-----------------------------+             |
+----------------------------------------------------------------------------+
| Input Parameters  : unsig************/
 No error                                    12          |
	   |               ulate the re=-------+---------PCI1**********& 1--------------------------------------+
*/

int i_APCI1710_CounterAutoTest(struct comedi_device *dev, unsigned char *pb_TestStatus)
{
	unsigned char b_ModulCpt = 0;
	int i_ReturnValue = 0;
	unsigned int dw_LathchValue;

	*pb_TestStatus = 0;

	/********terOption == APCI1710_HYSTERESIS_ON || b_SecondCounterOption == APCI1710_HYSTERESIS_OFF))) {
								i_ReturnValue =
									0;
							} else {
			     /*********************************************************/
								/* The selected second counter operating option is wrong */
			     /*********************************************************/

								DPRINTK("The selected second counter operating option is wrong\n");
								i_ReturnValue =
									-7;
							}
						} else {
			  /*******************************************************/
							/* The selected second counter operating mode is wrong */
			  /*******************************************************/

							DPRINTK("The selected second counhar_ b_ExternalStrAR_COUNTER  | After a index signal, the counter  |  |
|r initialised */
	   /*******************************
	      /****************************************/
	st counter operating option is wrongInfo.
									s_InitFlag.
									b_FrequencyMeasurementInit
		Registe_ b_ExternalStrcted module number parameter is wrong */
	   /*************************************************/

		DPRINTK710_33MHZr b_ExterimerValue
										=
										(uned int)
										(ul_TimingInterval
										*
										(0.25 * b_PCIInputClock));

					/*******************/
									/* Round the value */
					/*******************/

									if ((double)((double)ul_TimingInterval * (0.25 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
										ul_TimerValue
											=
											ul_TimerValue
											+
											1;
									}

					/*****************************/
									/* Calc/***************** timing *o 3)     r parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return wrong */
******************1710_30MHZ)
							&& (b_TimingUnity == 2)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								8UL))
						|| ((b_PCIInputClock ==
								APCI1710_33MHZ)
							&& (b_TimingUnity == 0)
							&& (ul_TimingInterval >=
								242)
							&& (ul_TimingInterval <=
								7943757UL))
						|| ((b_PCIInputClock ==
								APCI1710_33MHZ)
							&& (b_TimingUnity == 1)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								7943UL))
						|| ((b_ b_Exter |
|              |    0000         |              |                         | There b_ b_ExterDisableFrequencyMeasuremen						1;
									ul_TimerValue
										=
										ul_TimerValue
										-
										2;

									_ b_ExternalStro/******/
									/* ms */
				/******/

								case 2:

					/********lised */
			    									/* Timer 0 factor */
					/******************/

									ul_TimerValue
										=
										ul_TimingInterval
										*
										(250.0
										*
										b_PCIInputClock);

					/*******************/
									/* Round the value */
					/*******************/

									if ((double)((double)ul_TimingInterval * (250.0 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
										ul_TimerV {
								iPT.h"

/							}
	                                   APCI1710_DISABLE :       |
|                                               Disable the interrupt        |
+-------_ b_Exter****************************/

									*pul_RealTimingInterval
								               |
|            ###################he handle parameter of th~
										(250.0 * (double)b_PCIInputClock));
									d_RealTimingInterval
										=
							lduleConfiguration[is wrong */
	   /********************K("The selected module numbr b_ModulCpt = 0;
	int i_ReturnValue = 0;
	unsigned int dw_LathchValNT)) || (b_SecondCounter-------------------------250.0 * (double)b_PCIInputClock)) >= (double)((double)*pul_RealTimingInterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
											+
											1;
									}

									ul_TimingInterval
										=
										ul_TimingInterval
										-
										1;
									ul_TimerValue
										=
										ul_TimerValue
										-
										2;

									break;
								}

								fpu_end();
			     /*************************/
								/* Write the timer value */
			     /*************************/

								outl(ul_TimerValue, devpriv->s_BoardInfos.ui_Address + 32 + (64 * b_ModulNbr));

			     /*******************************/
								/* Set the initialisation flag */
			     el : Extecy and  | |
| |                    |              ialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitF        | period          a index signal, the counter  |  |
|           | Registeel : Exte         | value is clear/***************************/

								DPRINTK("Counter not initialised\n");
								i_ReturnValue =
									-3;
400/02    al (UAS)imerValue
										=
				ed int)
										(ul_TimingInterval
										*
										(0.25 * b_PCIInputClock));

					/*******************/
									/* Round the value */
					/*******************/

									if ((double)((double)ul_TimingInterval * (0.25 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
										ul_TimerValue
											=
											ul_TimerValue
											+
											1;
									}

					/*****************************/
									/* Calcnsigned char) C timinUAS= culow "0DisableFreq********************/
				/* The selected PCI input****  |
|       "1****************************/
							/* Makte the configuration commando */
			     /******OUNTER) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister1 = b_CounterRange |
					b_FirstCounterModus |
					b_FirstCounterOption;
			} else {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister1 = b_CounterRange |
					(b_FirstCounterModus & 0x5) |
					(b_FirstCounterOption & 0x20) |
					(b_SecondCounterModus & Level : Exter/******/
									/* ms */
				/******/

								case 2:

					/********	case APCI									/* Timer 0 factor */
					/******************/

									ul_TimerValue
										=
										ul_TimingInterval
										*
										(250.0
										*
										b_PCIInputClock);

					/*******************/
									/* Round the value */
					/*******************/

									if ((double)((double)ul_TimingInterval * (250.0 * (double)b_PCIInputClock)) >= ((double)((double)ul_Timer******************/

									*pul_RealTimingInt                                          ers : -      merValue
										//
										(2>>****50.0 * (| b_SecondCounterOption == APCI1710_HYSTERESIS_OFF))) {
								i_ReturnValue =
									0;
							} else {
			     /*********************************************************/
								/* The selected second counter operating option is wrong */
			     /*********************************************************/

								DPRINTK("The selected second counter operating option is wrong\n");
								i_ReturnValue =
									-7;
							}
						} else {
			  /*******************************************************/
							/* The sele, devpriv->s_BoardInfos.ui_Address + 32 + (64 * b_ModulNbr));

			     /*******************************/
								/* Set the initialisation flag */
			      |
+----ncy and  | |
| |                    |              [0]);
		break;

	case APCI1710_INCCPT_SETINDEXANDREFERENCESOURCE:
		i_ReturnValue = i_APCI1710_SetIndexAndReferenceSource(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned char) data[0]);
		break;

 |
+----         | value is cleared (32-Bit)          |  |
|           +------------------------+------------------------------------+  |							-3;
 {
				 overf    merValue
										=
						ed int)
										(ul_TimingInterval
										*
										(0.25 * b_PCIInputClock));

					/*******************/
									/* Round the value */
					/*******************/

									if ((double)((double)ul_TimingInterval * (0.25 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
										ul_TimerValue
											=
											ul_TimerValue
											+
											1;
									}

					/*****************************/
									/* Calc     /******** timin31 -> 0701***************/
		/* The selected module number parameter is wrong *EMENTAL_         |
+-----------------------------------------------------------------------+
| Task              : Clear the counter value from selected module           |
|                     (b_ModulNbr).                                          |
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_ b_BoardHandle : Handle of board APCI-1710        |
|                     unsigned char_ b_ModulNbr    : Module number to configure       |
|                                           (0 to 3)                         |
+---------------- |
+-----/******/
									/* ms */
				/******/

								case 2:

					/******** {
	   /*                                               |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: The selected module number parameter is wrong      |
|                     -3: Counter not initialised see function               |
|      16                 "i_APCI1710_In     /***erValue
										/
										(250.0 *>= (double)((double)*pul_RealTimingInterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
											+
											1;
									}

									ul_TimingInterval
										=
										ul_TimingInterval
										-
										1;
									ul_TimerValue
										=
										ul_TimerValue
										-
										2;

									break;
								}

								fpu_end();
			     /*************************/
								/* Write the timer value */
			     /*************************/

								outl(ul_TimerValue, devpriv->s_BoardInfos.ui_Address + 32 + (64 * b_ModulNbr));

			     /*******************************/
								/* Set the initialisation flag */
			           |
+----*************************handle parameter oflue = 0;
	double d_RealTimingInterv					***********************/
	/* Test the module numburnValue;
}

/*
+--l counter value493-92
0 is wrong */
---------------------------------------+
**********************************/
		/* The selected module number parameter is wrong */
	   /*************************      *****************(*
+-----------------to              2*16-brs  {
		    /************nput Parameters  : u               ****************odulNbr,
	unsigned char b_CounterRange,
	unsigned char b_FirstCounterModus,
	unsigned char b_FirstCounterOption,
	unsigned char b_SecondCounterModus, unsigned char b_SecondCounterOption)
{
	int i_ReturnValue = 0;

	/*******************************/
	/* Test if incremental counter */
	/*******************************/

	if ((devpriv->s_BoardI  b_BoardHandle)                   |
+----------------------------------------------------------------------------+
| Task              : Clear all counter value-----+
| {
		 /************at onl| |         D------+
| Ixterna  |
|	-------------|						cl*************Function Name    				****Oion of the PCI busthct c  |
|						-              	     |
+------------------------I1710_SetInputFilter   k  :	Selection of the PCI bus     |
|						clock ***/
		                |
|						- APCI1710_30MHZ :           |
|						  The PC has a PCI bus clock |
|						  3 MHz          {
				          |                         | The inputs A, B in the  | |
| |                    |                         | 32-bit mode or A, B and | |
| |                    |                         | C, D in the 16-bit mode | |
| |                    |                         | represent, each, one    | |
| |                    |                         | clock pulse gate circuit| |
| |                    |                         | There by frequency and  | |
| |                    |                  31 -> 0701/0232         to     |
|	710_I----+|
|     |                  unsigned char_ b_A1710_DisableFrequencble or disable the        |
Firm     revisA Gm400/0228 All Function in RING 0       |
|                                           (0 to 3)                         |
+----------------      |
+-------------------------------------------------------+
*/

int i_APCI1710_ClearAllC-----+
| Fs (0.535900MHz) |
|					14: Filte1############################### */

/*
+----------------------------------------------------------------------------+
| Function Name     :INT	i_APCI1710_InsnBitsINCCPT(struct comedi_device *dev,struct comedi_subdevice *s,
struct comedi_insn *insn,unsigned int *data)                   |
+----------------------------------------------------------------------------+
| Task              : Set & Clear Functions for INC_CPT                     |    10_INDEX_:  Filter from 726ns  (1.357741MHz==
					     -1: The handle parameter of the board is wrong         |
|                     -2: N---------------------------------------1-----10)e ho1331ns111-1307 USA

You shoud also find t								&
								0.833333Mver|
|		igned char_ b_BoardHandle     : Hand			}	/*  //  */
			  /******************						lier
      -DATA Gm

Copy7223/9493-0
-----	   ) >->
				0x3136eturn Va***************m

Copy No error                       >s_BoardInfos.
											s_ModeRegister.
									500000MHz) |
|					1								 e parameter of the bo    |
+------0e].
						.0 * (d                     0     |
|					0:  Software filter not used  ----   |
|					1:  rogram is (ps      | |VariterM--------- [
  +---------].ror          		14: Filter from 1815n |
|4MHz) |
|					15: Filt    936ns even the implied warranty of MERCHANTABILITY or SS FOR A P0.833333MHz) |
|					9:   General Public License for more detaiodeRegisoduleInfo
									[b_Mod		3:  Filter from 400ns  (2.500000MHz) |
|					4:  Filter from 500ns  (2.000000MHz) |
|					5:  Filter from 600ns  (1.666666MHz) |
|er from 1000ns (1.000000MHz) |
|					10: Filter from 1100nweier
	Tel	5:  Filter f.-----------------------i-data-com
	i---------------------------------rom 1331ns (0.75131|
|			**************/

							if ((b_SecondCounterModus == AProng       (1.250000MHz) |
|					6:  Filter from Hz) |
|	CI1710_InsnConfigINCCPT(struct comedi_dei_device------------------------------------------------- wrong        |
signed int *data)
{
	unsigned  == APCI1710_HYSTERESIS_Oter from 1300ns (0.769200MHz) |
|					13: Filter from 1400ns (0.714200MHz) |
|					14: Filter from 1500ns (0.666666MHz) |
|					15: Filter from 1600ns (0.625000MHz) |
+------}------ is 300ns (0.769200MHz) |
|					13: Filter from 1400ns (0.714200MHz) |
|					14: Filter from 1500nce *dev,struct comedi_subdevi-----b_SecondCounterOption == APCI1710_HYSTERESIS_OFF))) {
								i_ReturnValue =
									0;
							} else {
			     /*********************************************************/
								/* The selected second counter operating option is wrong */
			     /****************         |
|                     -3: The module is not a counter module                 |
|					  -4: The selected PCI input clock is wro}-------------------------------eturnValue = 0;
	ui_ConfigType = CR_CHAN(insn->chanspec);

	printk("\nINC_CPT");

	devpriv->tsk_Current = current;	/*  Save the current process task structure */
	switch (ui_ConfigType) {
	case APCI1710_INCCPT_INITCOUNTER:
		i_ReturnValue = i_APCI1710_InitCounint dw_Status = 0;

	/********		(unsigned char) data[0], (unsigned char) data[1]);
		break;

	case APCI1710_INCCPT_LATCHCOUNTER:
		i_ReturnValue = i_APCI1710_LatchCounter(dev,
			(une param**************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else*********************************************************/
		/* The selected module number parameter is wrong */
	   /**************************************prog-----**/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_ClearAllCounterValue                  |
|                               (unsigned char_      b_BoardHandle)                   |
+----------------------------------------------------------------------------+
| Task              : Clear all count**************               /*******----				********************/
				/* The selected PCI input cl  | Compilere dow                 |                  ----------------------------------artz flag (DQ0) */
			  /******************************/

							if ((dw_Status & 1) up---------------------------------------------------------------------------+
| Task              : Clear the counter value from selected module           |
|                     (b_ModulNbr).                                          |
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_ b_BoardHandle : Handle of board APCI-1710        |
|                     unsigned char_ b_ModulNbr    : Module number to configure       |
|                                           (0 to 3)                         |
+-----------------e parame--------------------------------------------------+
| Output Parameters :(b_PCIIn                                               |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: The selected module number parameter is wrong      |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_Ini********                           |
+------2--------*****/
									/* Calculate the real timing */
					/*****************************/

									*pul_RealTimingInterval
										=
										(unsigned int)
										(ul_TimerValue
										/
										(0.00025 * (double)b_PCIInputClock));
									d_RealTimingInterval
										=
										(double)
										ul_TimerValue
										/
										(0.00025
										*
										(double)
										b_PCIInputClock);

									if ((double)((double)ul_TimerValue / (0.00025 * (double)b_PCIInputClock)) >= (double)((double)*pul_RealTimingInterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
											+
											1;
									}

									ul_Tim                  |
|	(devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulNbr] &
					0xFFFF) >= 0x3135) {
		 /**************************/
				/* Test the PCI bus clock */
		 /**************************/

				if ((b_PCIInputClock == APCI1710_30MHZ) ||
					(b_PCIInputClock == APCI1710_33MHZ) ||
					(b_PCIInputClock == APCI1710_40MHZ)) {
		    /*************************/
					/* Test the filter value */
		    /*******) ||
			{
		   afk == ***********/
									/* Enab--------        at on                  |
  +-----------------------------------------------------------------------+
  |                             UPDATES                          |
|      nputClock ==
							APCI1710_40MHZ) {
			  /*********************************/
							/* Test if 40MHz quartz on board */
			  /*********************************/

							dw_Status =
								inl(devpriv->
								s_BoardInfos.
								ui_Address +
								36 +
								(64 * b_ModulNbr));

			  /******************************/
							/* Test the quartz flag (DQ0) */
			  /******************************/

							if ((dw_Status & 1) !=
								1) {
			     /*****************************/
								/* 40MHz quartz not on board */
			     /*****************************/

								DPRINTK("40MHz quartz not *****************************/

		DPRINTK("The selecNodulNbr].
								s_Sie          |                         | The inputs A, B in the  | |
| |                    |                         | 32-bit mode or A, B and | |
| |                    |                         | C, D in the 16-bit mode | |
| |                    |                         | represent, each, one    | |
| |                    |                         | clock pulse gate circuit| |
| |                    |                         | There by frequency and  | |
| |                    |                         | p                                      automatic index reset.       |
|                                               APCI1710_ENABLE :            |
|                                                 Enable the automatic m                       |
|		leInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									|
									APCI1710_ENABLE_40MHZ_FILTER;

							}	/*  if (b_PCIInputClock == APCI1710_40MHZ) */
							else {
			     /******************arranty of MERCHANTABILITY or FITNESS FO     +-------------			}	/*  if (b -5: The index operating mode parameter is wro|                                               Disable the interrupt        |
+-------aramete       Ot onl----------------------------------------------------+
| Output Parameters : -          ardHandle,    |
|               

				: Counter not initialised see function           erval
										=
										(unsigned int)
					*************/

								devpriv->
									s_-----------ModuleInfo
									[b_ModulNbr].
									s_         ue = -5;
					}	/*  if (b0000MHz) |
|					8:  Filter from 900ns  (1.11---------------ame v->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									|
									APCI1710_SET_LOW_INDEX_LEVEL;
							} else {
				/**************************************/
								/* Set the index level to high (DQ26) */
				/**************************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.rnal strobe level parame						[b_ModulNbr].
									s_SiemensCounterInfo/* Counter not initi   |
+-
  +-----------------------nfo.
									s_ModeRegister.
									s_ByteModeRegi    |
+----rce(dev,
			(unsigned chanfo.
									s_ModeRegister.
									s_ByteMode******       pbter not-------------------****/

								devpriv->
									s_
				   /     -1ul-----			/****************************************************/

				DPRINTK("The reference action parameter is wrong\n");
				i_ReturnVal            {
		   (1: The haernal Q0) -------ota[4]);
***********/

								if nput Paraag (DQ0) st (Cim							s_ModuleInfo
										***********/

								if              u              		   /**************** Disa*****************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter 			- APCI1710_33MHZ 					(devpr              Operation ound         ter4
									=
									devpriv->
									s==
								AmerValue
								****/

								devpriv->
									s_ModuleInfo
				         
  |cyc----oput Para****/

								devpriv->
									s_ModuleInfo
				 ((dwtar             unsigned****/

								devpriv->
									s_ModuleInfo
				---------**********			outl(1 ****/

								devpriv->
									s_ModuleInfo
				elec* b_ModulNbr));
	opp else {
		 /************************************************-----=
								A******iar) ch register parameter is wrong */
		 /***************----le             unsign_SiemensCounterInfo.
									s_ModeRe************/
							/* Test the quartz flag (DQ0) */
ch register parameter is wrong */
		 /***************((dw_Status & 1) !=
								1)ch register parameter is wrong */
		 /**************			/* 40MHz quartz not on board* Counter not initialised see function */
			/* "i_APCI1710_InitCountez quartz not ***********/
			/* Test*******/
		/o counter{
	      /*nitCounter"          se {
		 /********************************************                -4defi    */
	   /**********************************************e se bas							s_ModuleInfo
	nValue;
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_InitReference                         |
|                                                (unsigned char_ b_BoardHandle,       |
|                                                 unsigned char_ b_ModulNbr,          |
|                                                 unsigned char_ b_ReferenceLevel)    |
+----------------------------------------------------------------------------+
| Task              : Initialise the reference corresponding to the selected |
|                     module (b_ModulNbr).                                   |
|                                                                            |
|           rnal strobe level paramet				{
		    /**************************/
					/* Test*************/
		   The handrInfo.
									s_ModeRegior */
			     /*****o counter --------------|             |
|           i_ratin Versio*/
					/*****************---------                              |
|             +--------------------+-------------------------+               |
|             | b_ReferenceLevel   |         Operation       |               |
|             +--------------------+-------------------------+               |
|             |   APCI1710_LOW     |  Reference occur if "0" |               |
|             |--------------------|-------------------------|               |
|             |   APCI1710_HIGH    |  Reference occur if "1" |               |
|             +--------------------+-------------------------+               |
+-----------------------------------------								&
				if>
						-------------------------			}	/*  //                 |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_Inii_ReturnVaeRegister1 |
						APCI1710__TimerValue + {
		    General Public License for modeRegist28MHz) |
|									     |
|				40 MHz                             32        |
|				------           								[b_ModulNbr].
									SS FOR A PA
										s_Bytesele to the selected latch register        (1.1111    										(250.0s (0.516                 (b_                 833 Otte				0:  Software filter not used 833 Otte	-----NABLE)
							{
								devpriv->im

CopyrignalStrobe=
								APenceSource(struct comedi_de--------- selected mod---------  :  0: No ernfo@addi-daz                     |
|       		/* Test			s_ModeRegister.
									     --------------|
|  -3: Cilter from 726ns  (1.357741MHz) |						&
								    | repre        180637MHz) |
|					7:  Filter fromilter from 1694ns (0int	i_APCI1710_InsnConf		14: Filter from 1500ns (0.666666MHz) |
|					15: Filter from 1600ns (0. (at your     _       ns (0NCREMENTAL_COUNTER) {
	 < 4) {
e code of this module.

	ADDI-DATA GmbH
	Diese1710_Set           		     |
|		   1-2: sebr) in to th*****/
			/* Test if firmware >= Rev1.5 */
	       /****** Filtction)
{
	int i				15: U) !lNbr < 4) **/
	/* T            /******		---------						/*INPDW (pse source se_DIRECT_M			tion)
{
	int ***********cense, or						   s--------							u***/

				if (b_Sourhe source selction)
{
	int 	if (b_SourceSelection == APC0000UL /**********gister.
e sel{
		 /*******/
			-******/
			*********/
		l Public(b_La********/
			/* Test if firmware >= Rev1.5 */
	      /******************************/
2			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulNbr] &
					0xFFFF) >= 0x3135) {
		 /**********************************/
				/* Test the source selection */
		 /**********(on == APCI1710_SOURCE_1)
				{
		    	PCI1710_SetSetI6 /**********ceSelection == APCI*/
		    /*ection)
{
	int ourceSelection == APCI1710_SOURCE_1)
				{
		    /**************************************/
				******/
					/* Tes -              	APCI16*/
		    /*****/
			oduleInfo
	**********/
 selected mod*****************						s_ByteModeRegister.
							b_Mo		15: Modulet if invert the index a				b_Modealue;
	erbatim}verbatim
Copyrig} else {verbatimif (*pb_UDStatus == 1)5  ADD of t/*s module.

	ADDI-DATA GmbH
	Dieselstras/verbatim
/* Test if 16-bit counter 2 pu2005occur se 3
	of this module.

	ADDI-DATA GmbH
	Dieselstrasse verbatim
TA G(*pul_ReadV/**
 & 0xFFFF0000UL) != 0e code  redisui_16Bitrograverbatim
		=/or modify (unsigned int)t under the t under the m

This progra/or modify 	>>d by the Fre16 GNU General&/or modify s freeU)@verbatim
	se as published by the Frit under the se as published by the Frr version 2 of the L GNU General|GNU General Public Licenof the  - ribute it and) << 16License, or
Copyright4,2005  ADDI-DAATA GmbH for the cense, or == 2ou can ris module.

	ADDI-DATA GmbH
	Dieselstrasse 3
	D-778833 Ottersweier
	Tel: +19(017223/9493-0
	Fax: + GNU General Public License for more detailss.

You shta.com

This program is freeUare; you can redisttribute it and/or modify er version.

e terms of the GNU General	*d also find e as published by the Frer version 2  of the icense, or  (at your option) any late MA 02111-1307 se as published by the Freccompanying this so softwa
/*
  +-----eful, but WIARRANTY; I-DATA GmbH -I-DATA GmbH hout even the icense, or 
Copyrigh

Copyright (C) 200verbatNTABILITY or FImbH r the so 1@verbatimbH for the so 0@verbat------NTABILITY or F-------------(0) 7223
  | Tel : +49 (0) 722-----NTABILITY ohis module.

	ADDI-DATA GmbH
	Dieselstrasslstrasse 3
	D/* Frequency measurement logic not initialised	Fax: 93-92    | Internet : http://www.addi-data.com   |
verbaDPRINTK("-------------------------------------------\n"eier  |i_Returnrogram= -4@verb----NTABILITY of thhis module.

	ADDI-DATA GmbH
	Dieselstrasse 3
	/* C: +19(0----------------see function	Fax: +/* "i_APCI1710_Init-------"      ate    se 3               |
  +-----------------------------verbCompiler -----------------------ame : INCCPT.C       | Ver3@ver}
.96           3-92    | Internet : http://www.addi-data.com   sse 3
33 Ohe selected module number parame19(0is wrong :  02/1 incremental counter module                  |
  |  
-----------                                             ame : INCPT.C       | Ver2;
	}

	r.C    CPT.C       |;
}
