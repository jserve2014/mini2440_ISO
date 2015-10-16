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

 ource /**
@verbatt module.

	ADDI-DATA GmbH
	Di/
m
Copyydevpriv->lstrass	s_Mdule.Info833 Otte[bsweierNbr].: +19(0)rsSiemensCounter	Tel	Fax: +49(0ModeRegis92
	://www.addBytei-data-com
	info@addi223/9

This pr4 =: +19(0)e 3
	D-77Fax: +49(03/949
	htt: +19(0)7free 493-0
info@addi-d the terms 92
	http://www. is om

This program is om

T.is p
This program is free software; y&: +19(0)APCI1710_DEFAULT_INDEX_RFERENCE;lstras}lstra}yrig if (b_Surce SelecA Gmb==  (at yourSOURCE_0 ||distributed in(C) 2hope that it will 1r(C) 2s		else {n 2 urcode of tion;odule.

	ADDI-DATA Gmbhe sopyrigTevenul, b st WITHOUTis wrongt evend warrantynty MERCHANTABILITY or FITNESSS FOR DPRINTK("ARTICULAR PURPOSE. Se WARe GN\n")sion.

i_ReturnValue = -4 Publieither verseful, but WITHOUT ANY WARRANTY; withbe useful, but WITHOUT ANY WARRANTY; without even } he impliedrranty of MERCHANTABILITY or FITNEH
	DieS FORA Peceim modie GNnot a cblic Le COPYIN Gener307 USA

You shoud also findGNU Gcompletostontld have receing thisG file ac----anyiGNU Gieral Publi License al GNUwi3 Publtion, MA 02111rce carranty of MERCHANTABILITY or FITNEomplete GPWITHOUT -----------------------------sneraDieselstraße 3 ------ibute it and/or mod  |
  ston-/9493-0     | email    : info@addi-data.com         |
+
  | (C) DDI-DATA }
H-----------iestraße 3       D-77833 Ottersweier  |
 te G in theCt WITed/9493-0 numbeion NU GenU Gen | I-92
net : cense as published bFoun 9 (0) 
ou-------------v/9493-0     | email    : info@-----------+
  | (C) ADDI2;
	}

	r+
  | ---+
  | (C) ;
}

/*
+-969493-92                (0) + info@addi-data.com         |
  m         |
  || FunITHOUTNameurce : _INT_	i_Y WARRANTYetDigitalChlOnurce cc Date     : |
|----   (unsigned char_  b_BoardHandle, |Date     :  ------02/12/2002               |
   terms o)/9493-0     | email -----.6                       |
  +-------------------------------+--------------     Task--------------:ght s : +20r: Eri output H    t----an        means/9493-0    A Gm: s |
  +|         high.|
  | FaxDescripTA Gm:     |
  +-   (at- you incr223/tal c93-92
ERCHANT               |                             I     Parame49(0------------------|
  +----------             of b----NY WA-hat                /9493-0     | email    : i       :	N      of     ng this o b    -------		 +49 (0)ed (0thor3 |
  | FaxD------------------------------------------------------------+
  |                  O           UPDATE:      |
  +----                        |
  +----          ----------------------------------------------------------+
  |                  -+
  |  (C) ADuinot C0: No errorES                 n RING 0             |
  |+
  |                 -1:n thehn RINGp        Date           ----------------+
  |      +
  |       | 2 avail/9493-0     | email    : info@ad---------------+
  |      --------------3: ublic L-----initialiURPOsee f        --------------------  " Project maInitublic L" in RING 0             |
  |      /9493-0     +----------------------------------------------------------  |
  :int      | See nage      c Sto(struc05    di_device *dev,--------------------------
{
	 : 2.9-+
  | (C) ADD0;

	rranty of MERCHANTABILITY o/n RINTesx : +2-------------------+e.

	ADDI-DATA GmbH
	Die-----am; if terms o < 4)-------------------------------+
  | Pr +------
  | f---------23---------- info@addi-data.com         |
  | FaxPr :  A verdistribute it/or modify ithe terms of the General Public Licenss i_APFlag.b_ublic LicitT AN1-----reseful, butNTY;| int	ce *at yourInsnC ADDIIN General Public License as lished by the Free Sware Foundation; either sion 2 of the 3 = ,
-----05  -----insn * Con,         
/*
*m

T)

-------------------------------+mail    : info@addi-data.com         |
  || T| 0x1n RINDieselstraße 3       D-7783 +------ht      --------n---------------------------------Ine *sout----------- : Configuration function for INC_CPT                             |
+-------dwished by the 1_2_3_4,skn RING 0s +----	httsinfo@aui_Address + 20 + (64 * |
+--------               ---------------------------+
  | Tel : +49 (0----    Pa31 -> 0701/0------------                ------| Guinothe G *insn,unsiDisableFreque									 |
+-----------------
  | FaxTel www.a (0(0) the terms o  -----------------------------+
*      Faxstruct codi_insn *ins9-----------------+
*/

#include "APCI17oject   Project     -------------------------------+----------------------------------------------------------  | Se
+--------------API |
  ----------Compiler : gcch (ui_Current = cCOUNTER	unsigned int uiweier
 name : INC_CPT.C | GuinotVer Pub :   APCI-1710 incremental counter module                  |
  |                      the current process task structure ----------------ffz-----
  |          |   -----------------------intk("\nINC_CPT");

	devpriv->tskmail    : info@addi-data.com         |
  | Fax        |
  +-  |
  |----------|-----------|------------------------------------------------|
  |v,
			CR_AREF(insn->ReTES                 d cha.chansp = i_APCI1710_InitIndex(dev,
			CR_AREF(insn->c  low                 |
  +-------------------------------+I1710_CounterAutoTest(dev,
			(unsigned char *)CI1710_InitReference(dev,
		    | Snction in RING 0             |
  |  RAUTOTEST:
		i_ReturnValue = i_APCI1710_CounterAutoTest(dev,
			(unsigned char *) ;
		brea_APCAu updinot C. | - 1        |
  of updatanspec),
			(unsigata[0]);
		brea------------ata[3]);
		break;

	case APCI1710_INCCPT_INITREFE i_APCI1710_Ini--------------Value = i_APCI1710_InitCompareLogic(dev,
			CR_  |
  +-----------------------------------------------------------------------+
| 08/05/00 | Guiile CINIT- 0400/0228 All----cTA Gmin R----{
	ca---+
  | 29/06/01 | Guinot C. | - 1100 ------t coCI1710_InitCompareLogic(dev,
			CR_AREF(TOTEST:
		i_ReturnValue = i_APCI1710_CounterAutoTest(dev,
			(unsigned char *)29/06/01YMEASUREMEN.:
		i11eturn-----------------------------+
*/

int29/06/01 | Guinot C. | - 1100ConfigINCCPT(struct comedi_decyMeasu------                     ------------------------------------------------------------+
*/  : 2.9tion Name     : _INT_ if            |
+-----------------------------------------                         (unsiIncluded-----nspec),
			(unsigned char         ata[3]);
		break;

	case APCI1710_INCCPT_INITREFERENCE:
		i_ReturnValue =------+
| #ied cha "insn,unsigNCev,
h"| Function Name     : _INT_ i_            |
+-----------------------------------------insn *insn,unsigned int *da    ------        :------------       b_Firstsub--------sk              : Configuration function for INC_CPT                             |
+-----------------------------------------------a-----------------  int *-DATA Gmfi_APCI17forter(dev,   b_BoardHandle,               |
|                                unsigne& 0xEF                      |
|         i_APCI1   UPDATES:	----ff-----------ICUL       |
|                   b_CounterRange,              |
|                 O      -----------: for I  |
|                                unsigned char_          b_ModulNbr,        | icense  along----------,               |
|                                unsigned char_          b_ModulNbr,            
/*
           unsigned char_          b_FirstCounterModussk              : Confi      	             : Configurat ion function for INC{
	ion function ui_ int *Type;prog|   icense along w0;
	        |
|   = CR_e fo( Con-> data[ec)    printk("\ner(dev,"     e 3
	D-77tskITCOUNTER:
		i_Ret;	/*  S---- +---COUNTERproc----t b_Sk     ure */
	switch (        |
|  ) {
	caseigType) {       _INITCOUNTER:
		                 *insn,unsignit493-92
(----
			CR_AREF           #---------------|bsweierNb0228insn,unsi16BIT_      |->chARTI-------is5  ADD----: +19(0)therINSN WRITEonfigure the co |
| |b_ModulNbr   APCI1710_16BIT_COUNTER |  The module is configguredhanspec),
			(unsigned char) data[0],
			(unsigned char) data[1],
			(unsigned char) data[2], (unsigned INTdata[3], (ungnedWritehar_          b_CounterRange,              |
| sub         ,
        b_Coun Confirst ,ch access : 2for Ied char *) &data[0])break;

	case APCI1710_INCCPT_INITINDEX:
		i_ReturnValue = i_APCI1710_InitIndex(dev,
			CR_AREF(insn->cEn),
		Dcom
	Mo----------                       unsigned char_   dev,
			CR_AREF(insn->chanspec), (unsigned int) data[0]);
		break;

	case APCI1710_INCCPT_INITe(dev,
			CR_AREF(insn->ced char) data[0], (unsigned char) data[1]);
		break;

	case APCI1710_INCCPT_INITCOMPARELOGIC:
		i_ReturnValue = i_APCI1710_InitCompareLogic(dev,
			CR_AREF(insn->chanspec), (unsigned int) data[0]);
		break;

	case APCI1710_INCCPT_INITFREQUENCYMEASUREME
/*
+----------------------------------------------------------------------------+
|Function Name terO  |
  OMPARELOGIC:
| |bFunction in RING 0             |
  |      co	       -----fi bit16 bit-----------dus and   us a-92
 |  - b_Fui_  |
 
|   Value = i_APCI1710_InitCompareLogic(dev,           bit          ----distribut-------------+-b_Firs;unter----eters  :|  ------     ----------ur-----    Passed-----------------    NY WARRANTb_Firs_ENABLELATCHI   |RUPT witch i_APCI1710_In     | See 3-92
MLatchIic Lruptr_  ,     |    b_Second)--------unterModus and  -----brea---------b_Second493-92
OpDIS- b_FirstCouterModus and        | |
| |          dus andus anare ile uURPOan-------n       |terModus and        | |
| |                us animportance.b_First6BIT_COUAREF(ALUEus and        | |
| |            |
 16Binsn,unsi (C) ------------------------+ |
|                   --------------------+b_Fi[0],-----------inused -- 1]Value = i_APCI1710_InitCompareLogic(dev,----------------+----                     493-92
 ope32|
  |mrran0_InitCompareLogic(dev,
			CR_AREnValue = i_APCI1710_InitCompareLogic(dev,
requencyMeasur                          - b_Fanydus and        | |
| |Index-----                    |                                                                |
  -----------------------------------adruple mod-------------------+ |
|                                                      s | APCOMPARELOGICus and        | |
| |                 APCareLogic------------------------+ |
|                                                           -------    importance.weies                  -------    circuit generhansp1710_IterModus and        | |
| |us and        | |
| |     |----------puls | APFREQUENCYMEASUREMEN--------------+-----------------------turn i_ReturnVafree-------------------------+ |
|                   unsigned char) dat  UPDA                      | counting pulse from   Dshifut Wnals  | |
| |                    |                      in relATA Gmto eacable  nals  | |
| |                    |rpulse frometer04,200        |
|   |
  C+49 (-----+-----W           nteris ct yourQUADRUPL>= 0)|                     |
|n;er(dev,
			CR_AREF(insn->chanspec),
			(unsigned char) data[0],
			(unsigned char) data[1],
			(unsigned char) data[2], (unsigned char) 
| |                   ---------requencyMeasurement(dev,
			CR_AREF(insn->cus and        | |
| |    ed char) data[1], (unsigned int) de analyURPOpther.     terModus and    erAutoTest(dev,
			(unsigned char *) &data[0]);
		br                  3-92
e same   | |
b_FirstCounterModus and        | |
| |                dus a| |
| |              l               from----------------------+
  | | |---------        b_CounterRa). E|   2 of theetaihar| |
| |b_----ccurf 2 COMPARELOGIC:
		i_ReturnValue = i[2],2              ) m

T[3]);
		brterOption,        |
|                                unsigned char_              |                   
			CR_AREF(insn->chanspec), (unsigned chaata[0]);
		break;

	case APCI1729/06/01 | Guinot C. | - 110d char) data[1]);
		break      r modi         o5escriptirup
			(unsigned SecondCounterModus|        | |
| |    sn->chanspec),
			(unsign                       unterModus |   APCI1710_SIMPLE_MODE  | Functions in the same   | |
_INITCOMPARELOGIC:
		i_ReturnValue = i_APCI1710_InitCompareLogic(dev,
			CR_AREF(insn->chanspec), (unsigned int) data[0]);
		break;

	case APCI1710_INCCPT_INITFREQUENCYMEASUREMENT:
		i_ReturnValue = i_APCI1710_InitFrequencyMeasurement(dev,
			CR_AREF(insn->chanspec),
			(unsigned char) data[0],
			(unsigned char) data[1], (unsigned int) dNs|  -------------found1710_InitFrequencyMeasurement(dev,
			CR_AREF(insn->cg : Config Parameter Wrong\n");

	}

	if (i_ReturnValue > (at your IRECT_MODE-----		i_ReturnValue = insn->n;
	return i_ReturnValue;
}

/* Passed valu|
| |b_Se4------| |
| routine-------stal                f (i_ReturnValue      |                     TheSe       -tRe imdu>n;
	return i_ReturnV

/*
+----------------------------------------------------------------------------+
| Function Name     |                           ounter                           |
|                               (unsigned char_          b_BoardHandle,               |
|                                unsigned char_          b_ModulNbr,                  |
|                                unsigned char_          b_CounterRange,              |
|                            unsigned char_          b_FirstCounterModus,         |
|          lz   | od.

	@end this imd char)            or     eral        |
|       and ar_ oour edges   | |
| |                    |                        module (b_ModulNbr). You must calling this function be |
+
|2  b_SecondCounterModus,        |
|                                unsigned char_          b_SecondCounterOption)                     |NY WARRANTYn->ch_unterModus |---------------------------+
  10_INCCPT.h"| |
| +-DDI-DATA Gmbct mode is not possible!          gure the counter operating mode from selected     |
|                     module (b_ModulNbr). You must calling this function be |
|                     for you call any other function witch access of        |
|                     counters.                                              |
|                                                                            |
|                          Counter range                                     |
|                          -------------                                     |
| +------------------------------------+-----------------------------------+ |
| | Parameter       Passed value       |        Description                | |
| |------------------------------------+--------------------------     -----               | one 0] |                        1| of rotation.           ||                   |--------------------+------------                          | |
| |------------------                 |                     perioe, except th|
| |b_SecondCounterModus|       d char) data[1]);
		break;
-------------------------- |
| |b_MF   b            InitExnsn,unsiSIMPLE          i_APCI1sWITHOUT stCou----------------/0228 All ------| |
| |b_ame   | |
| |    way as-----quadirstCoun |
| |b_M|
| |                    |          CI1710_InitCompareLogic(dev,
			CR_AREF(insn->chanspec), (unsigned int) data[0]);
		break;

	case APCI1710_INCCPT_INIERESIS_OFF | The first counting     ||
| |       or      Modus |   APCI1710               | pulse is not suppress  ||
| |b_SecondCounterOption |                         | after a change of                      THOUT dir| SeerranNU G                | rotatio   |                         | both edgterOptioihave no---------------+------------------------+|
|               | esounteinactiv    nals  | |
| |                    |                     ARTIii_APs A, BWITHOUT ption for direct mode                     |
|           32-stCo     or------    nals  | |
| |                    |                     C, DWITHOUT 16          nals  | |
| |                    |                     represent,s |  , on-----nals  | |
| |                    |                      lockich se gte      | etion for direct mode                     |
|              re by----n i_Re        STERESIS_OFF | The first counting     ||
| |       or -----d-DATA GmFirstCounterModus and        | |
| |              |
|           murnValue;
s canound710_HYSTERESIS_OFF | The first counting     ||
| |       or   forme!               INCREMction Name     : _INT----+----------------------------| |
| |b_-----------+ |
|                                                                            |
|                                                                            |
|       IMPORTANT!                                                           |
|       If you have configured the module for two 16-bit count                     unsigned char_          b_FirstCounterModus,         |
|          \nINC_CPT");

	devpriv->tsk_Current =-------------------------+ |
| |    ParameteI1710_InitCompareLogic(dev,
			CR_ARE------+---------------- not suppress  ||
| |b_SecondCounterOption |         +----------------------+
  |,((pe that i---------+------- << 8----+
FF------Handle of bof the four edges   | |
| |     |
|any other function     mdelay(1000      
+-----------------------              erirst            is-----possible!                 |           unsigned char_          b_FirstCounterOption,        |
|                                unsigned char_        ed char_ b_BoardHandle         : Handle of bo          |
| +----DATAng o  |
      -+-------/double/s0211 modu------+-----------------&b_SecondCounterOn.   |
|   ge        : Selectionot suppress  ||
| |b_SecondCounterOption |         |             | afte |  - b_|e of      HYSTERESIS_ON                           perati
  +-----her.                       |
|               Countisanspec),
			perating  importance.  -----              |
|             Pahyom
	e        | e.         |
|  F | The first counting     ||
| |       orIt supetersess |   APCI1-----------------+---------              |
|                   |    b_    | wh                  |
+-----------------------------------      afothea    ng                      |
+-----------------------------------of rotATA Ge same   | |      uppress  ||
| |b_SecondCounterOption |          ction Na       |
|  _FirstCount             |                         | pulse is NCREMENT | The counter increment for  | |
| |    elected the direct mode.   |
|                    | APCI1710_DECREMENT | The counter decreme       |               |                | pulse is not suppress  | b_ut WITedublic L      -5:PARTICed iut W-----------        |
|         e the GN| |
|------------lo					alysed eak;

	case APCI1710_INCCPT_INITINDEX:
		i_ReturnValue = i_APCI1710_InitIndex(dev,
			CR_AREF(insn->cirstCou--+-Bit v0 | Guinot C  7ected in    ters  se APC  -5: The selected first cou      Param if condCounterM6: |b_--------------+---   unsigned char_ b_ModulNbr            : ModNbr,
	unsigned char b_CounterRange,
	unsigned char b_FirstCounterModus,
	un--------+
| Input Parameters  : unsigned char_ b_BoardHandle         : Handle of bo-------M                        |  32-bit counter.           ot suppress  ||
| |b_SecondCounterOption |                             selected second counter operating mode is wrong.                                   --
	/*Nbr] & 0xFFFF0000UL) ==
		APCI     |
|       APCI1710_QUADRUPLE_MODE | I|b_SecondCounterM4ected second counter operating mode i(010_Ievicehar *) &data[0]);
on[b_ModulNbr] & 0xFFFF0000UL) ==econdCounterM-----if-----r b_Seca[2]ar b   | |
|                             |
|                                                                            |
|               Counter operating option for direct mode                     |
|               ----------------------------------------                     |
|                                                                            |
| +----------------------+--------------------+----------------------------+ |
| |      Parameter       |     Passed value   |       Description          | |
| |----------------------+--------------------+----------------------------| |
| |b_FirstCounterOption  | APCI1710_INCREMENT | The counter increment for  | |
| || APCI1710_DECREMENT | The c----------+--------e;
}

/*
+----------------------------------------------------------------------------+
| Function Name  a counter module      odule for two 16-bit counter---------0;

	/***************************irstCounterMo6: T-----------------+---------                          (unsigned char_          b_BoardHandle,               |
|                                unsigned char_          b_ModulNbr,                  |
|                                unsigned char_          b_CounterRange,              |
|                                unsigned char_          b_FirstCounterModus,         |
|                           | period       +49 (0) ction N|
| |      Paramopy of thevice *dev, struct comedi_subdevice *s,
	s      am; if tion  |
| |TAL_< 2 |
|  ---------------------te GPLirst              |ct mode is not possible!  		==
		                    -----		r  option.| APCI1710_DECR<< (16 *ther versi     |
|        n.   |
|              (0odus3 |b_Seco------8SecondCounter of      ||
| urn VA PAest any other function win          -1e code.

@endverbatim
*/
/*
  +--------               |ction Name     : _ second co           ----			&&s di                                  d char) dat      Passed valuConfigType) {
	case APCIondCounterOption == APCI1710_INCRm         |
  | Fax : +49 (th----ier                |
+-------------------------------------nd---------     |
|  --------+
| Input Parameters  : unsigned char_ b_Board_FirModus |   APCI1710_SI------------------------               r  option.  |
+-------:   impo---------        optModulNbr). You must calling this function be |
|                     for you call                modul         |
|         -ed char b_FirstCounterModus,
	unsigodulNbr). You must calling this function be |
|                     for you call any other function w | - 0400/0228 All Function in RING 0             |
  |------------------------1ected h----- p--------nty ofe b+---|
|      r operating mode is wrong */
			  2ected odule is cile acc				==
								A-+nter operating mode is wrong */
			  3ected second co--------r     e the GN                   ulNbr,
	unsielected second counter operating mode is wrong.|
|        ar b_Sec----------------------elected second counter operating mode is wrong.|
ULONG_ u      
			||d second    ting option ig mode is wrong.|
| ---+-------------------------+   /********************                             turnValue =
									-7;
							}
						} else {
			  /*******-------nsigned char b_ModulNbr,
	unsigned char b_Counte                        | after a c,******************************------************/
				D|
| |             ter range  nter operating mode ------                          --dulNbr] & 0xFFFF0000UL) ==
		APCI1/dulN */
		 b_Coun----------------			/* ***************************/
			
	 is +---
	D-77s  +---	Tels.----dw_Molule.us,        |
[_ModulNbr ] ----FFFF0*******************		}
			g */
	 e ==e of      32IT_COUNTER |    	}
			ng */
	      /******************			/	/* The se----------------------ss wrot if a error occur */
	   /****************** is di      | after a ch************ple    ||      ||PCI17 if 16-Bit counter */
	      /**DOUB*****************/

			if (b_CounterRange == AP||
| |      *********/

			if (b_CounterRange == APC          |    	a error occur */
	   /***************************/

		if (i_ReturnV       *****			s_ByteModeRegister.
					b_ModeReggiste*****				s_SiemensCounterInfo.
					s_ModeRegistPCI1710_INCRE |
|               ==PCI1710 second coCI1710_SI			s_Mod||		DPRINTK("The select			s_ModrRange == APCE1710_SI)CounterInfo.
					s_ModeRegister             ByteModeRegister.
					b_ModeRegister1 = 			s_ModeRegister= 0;

	/**********_Mod.
			f a error occur */
	   /******egiste/* The selec-+-----}

	   /***n;
			} Test i493-92
R     ounterModPCI1710_16BIT_COUNTER er.
			t if a error occur */
	   /*****************************/

	("The selected alue == 0) == APCI1710_DIRECT_MODE) {
					devpriv->
	PCI1710 else {
				devpriv->
			!unterModuondCounterOption == APCI171010_IoduleInfo dis
| |             oduleInfoCounterModu = devpriv-**************				s_Mode*******nterModus
					SiemensCounterIting option is wro					s_Modi-data-com
	1 .
						 of   re the fir          */| APCI1710_DECREMENT | The counter decreme4- b_FirstCounts wrong          |
|                    -2: The module is not Second counter operating |
|                                                   mode.                    |
|                     unsigned char_ b_SecondCounterOption : Second counter  option.  |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      : 0: No error                                            |
|                    -1: The handle parameter of the board is wrong          |
|                    -2: The module is notwrong      1710_INCCPT_COUNTERAUTOTEST:
		i_Ret*/
		    /*****************************************
		break;

	case APCI1710_INCCPT_INITCO             | after a change of      ||
| |                      |                         | rotatio {
	      /*ptiof 2 s     modulee the GNUtCounterModus |
					b_FirstCounterOptioODE) {
					devpriv->
					ld have rted second counter operating mode is -+
| Task    eralthe 
| |---------------4ounte}
		} he imp0) {
	      /**************************/
****************			}
					}
				} else {
		    /**== 0) {
	      /**************************/
rect mode */		DPRINTK("The selected counte                       |
|                                                                            |
|               Counter operating option for direct mode                     |
|               ----------------------------------------                     |
|                                                                            |
| +----------------------+--------------------+----------------------------+ |
| |      Parameter       |     Passed value   |       Description          | |
| |----------------------+--------------------+----------------------------| |
| |b_FirstCounterOption  | APCI1710_INCREMENT | The counter increment for  | |
| |    |
Parameter Wrong\n");

	}

	if (i_ReturnValue ounting pulse        | |
| |b_SecondCoun----92    |cy and  | |
| |                    |             | |
| |----------------------+--------------------+----------------------------| |
| |b_stCounterModus   : First counter operating  |
|                                    --------------l_---------ar b_Reg_ModeRegister._FirstCounterModus == APCI1710_DIRECT_MODE) {
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
						APCInfo.
						s_Mod of     b_CAPCI1710_QUADRUPLE_MODE f
				} el3
	      /******ter operaounterMok              : Configuration function for INC_CPT              unterModus,  |    ---- |
|   s,
struct comeda : Configuration function for  General Public License as plished by the Free Soware Foundation; either vnsigned char b_FirstCounterOptio  +--------- : meters ----*****------------------turnValue =
									-7;
							}
						} else {
			  /************************            ptio      I1710ar b_00tStatus -----|   ----  0: No err*****************/
				S FOR 2eRegister.
						s_ByouteInfo[b_Mo     |
|              |---------------- pb_TestStatus  : Auto test conclusion. See table|
+--------------------al signals,               |
|     *******b_SecondCounexternal signals,               |
|  		s_ByteMModeRegi						s_SiemensCou*/
	      /*Info.
					s_ModeRegoperating option is wro----+
|0requencyMe-----------------0_SI    r  option.  |
+-------= b_CounterRange |
			)    igned int d--+
*/

int i_APStatus)
{
	unsigned chnt i_ReturnValue = 0;
              odulCpt = 0;
	int i_ReturnValue = 0;
            FF))er.
		-------              				s_Mode ---	dw_Mo      are -------ternal signals,               |
|       ) {
					devpriv->
						s  |
|             ("The selected first counter operatk          ration[0] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (***/

	if
|                    		dw_MolduleConfiguration[1] & 0xFFFF000it counter
	if ((devpriv->s_BoardInfos.-7			dw_Mold		dw_ModuleConfigurat[0] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		 (devprivA PARTIC2] & 0xFFFF0000UL) ==
		APCI1710_-+
| Task          FFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER) {
		for (b_Modu		dw_MolConfiguration[2] & 0xFFFF0000UL) ----+-------1710_INCCPT_COUNTERAUTOTEST:
		i_Re|                            |  |   +-----------tus)
{two of the four edges   | |
| |                  |                of*******/

		            |
+-----------------------------------ameter of the boardnsigned char_          b_SecondCounterModA /
		-+
| Tasintenhar_ or tes|
|   +------one;
}      ulNbr,
	unsigned char b +----nnond co    phery.lue =****8     --------chain     ****************/

			         |nctionernall-----downress + (s----+-----------ulCpt));

		 /*********----p	/* ntltionom				uex/
				     als     | |
|	 /************************/

       |     i_Address + (64 * b_M    ded countt WITH***********************/

****llelOpti |   negating-------------     of CLKX     igned char b_FirstCounterOption,
	unsigned char b_SecondCounterModus, unsigned char b_Sec                  |
| +-auto the  devclusn->chFirstCounterModus,
	unsigned char b_FirstCounterO---------------------------------+
  | lue */
		 /********************ction Name     : _------------------------------ wrong\n");
							i_ReturnValu| pb_*/
	Statuatin*****E00/02d       |
  ------------------|   *dev, unsigned char * m b_SecondColNbr,
	unsigned char b_Countero counter module found */
	   /* devpriv->s_BoardInfos.
					ui_Address + 16 + o counter module found */
	   /*****pb_TestStatus)/
		/- 0400/02detond coo counter module found\n");
		i_ReturnValue = -2;
	}

	0_INCREMENTAL_COUNTER) {
		 /*---------------------------------------  /*******|*********-------o----------0 |----+--------I1710_InitCounter                           |
|                               (unsigned char_          b_BoardHandle,               |
|                                unsigned char_          b_ModulNbr,                  |
|                                unsigned char_          b_CounterRange,              |
|                                unsigned char_          b_FirstCounterModus,         |
|           d char_ b_Mod-------+             |
+-		outl(0, devpriv->s_BoardInfos.
					ui_Address + 16 + (64 * b_ModulCptstCounterOption,        |
|                                unsigned char_        -------------------						/* The sel |
  +--------- : unsigned char *_ pb_TestStatus  : Auto test conclusion. See table|
+---------------------------------------------PRINTK("The selected sec***************_else {
	   /****:Exte=
						*pb_TestS the GNt co                module (b_ModulNbr). If a    |
 option is wrong *duleConfiguration[b_M /**************************************************************************/

 selNo----------------, eac  b_BoardHandle,               |
|                                unsigned char_          b_ModulNbr,            |
|            ter1 |
 |
|*/
	                            xOperation paralse {
	   /**              *******weierCpER:
 ---                     ----on function dw_Lathch alon****              *************************************/
	
			/* The selectomatically, else you wrong */
	      /****************************************/

			DPRINTK("The selected counter ra0ng\n");
			000UL)------eRegister.
					s_ALCOUNTER 
deReg*************/

			DPRINTK("The selected counter ra1           |
|                                                                            |
|           +---2           |
|                                                                            |
|           +---3           |
|                                     er.
		tch (                           < 4|  |
|      ++* Test if a error occur */
	   /************************lected counter range is wro       | value (32-Bit) is latched in to	/* Test              Cou	   /***
|             /******************* Aion[3] & 0xFion & 0x40);

		 /***********|       or             |                       | pulse is not suppress  ||
| |b_Secon--+-------------------------| |
| |b_								/* The srame else {
	   /** |b_Se                module (b_ModulNbr). If a INDEX flag occur, yo-------c       l|---. Ax : s modATA 		APCI17a i----v->s_Bo,l(1, deun    incounteralI1710_32Barrister                    ysed value   |       Description    :
		ch edge -----odulNbr,
	unsigned char b_Cou| |
| |                    |                         | is analysed per         | |
| |                    |                                                 |  32-bit coun|         10_LATCH_COUNTER  | After a index si***************/

				dw_LathchValue = inl(devpriv->s_BoardInfos.
					ui_Address + 4 + (64 * b_ModulCpt));

				if ((dw_LathchValue & 0xFF) !=
					((dw_LathchValue >> 8) & 0xFF)
					&& (dw_LathchValue & 0xFF) !=
					((e                     |
|               ----------------------------------------                     |
|                                                                            |
| +----------------------+--------------------+----------------------------+ |
| |      Parameter       |     Passed value   |       Description          | |
| |----------------------+--------------------+----------------------------| |
| |b_FirstCounterOption  | APCI1710_INCREMENT | The counter increment for  | |
| |       elected cour_ b_ModulNbr  odulNbr,
	unsigned char b_C****************/

S                                    |---  |-------        |
|              5	}
		}
	} el option is wrong\n");
								i_Re   poss		 /*****************/

				outl(0**/
								/* The terOption |       X  |----                     | |
| |----------------------+--------------------+----------------------------| |
|         | eac--------+
| Input Parameters  : unsigned char_ b_Boa					/* The selecReferenceAAPCI1APCI                                    APCI1710_DISABLE :           |
| -----O    |
                                    Disable the automatic mode |
|           |
|i-dardInfos.
		                             Disable the automatic mode |
|            terot uunterM1710_               module (b_ModulNbr). If a INDEX flag occur, you have   |
|           b_SecondCo****/

				outl(0, devpriv->s_Board>s_BoardInfos.
					ui_Address + 16 + (64 * b_ModulCpt|                     module (b_ModulNbr). If a INDEX flag occur, you have   |
|          terModus,          | ea|
|             lity to clear the 32-Bit counter or to latch|
|                     the current 32-Bit value in to the first latch         |
|           b_SecondCounter**************/

								DPRINTK("The selected second counter operating option is wrong\n");
								i_ReturnValue =
				r operating opti  | |
| |   | ECT_MODE  | I-----------------------     iemensCounterInfo.=---------------------******************************** If----------enterM
  +--us =matic*****    |   any 			ui_Address + 4 + (64 * b_MoAPCI171s clea             		/*,     a----must reaou mu*************/

				outl(1, -------  /***(" *insn,unsiRead-----   /**" |b_SecondulCpt));

		 /**********ftt comedi_device  |
|                                                       |
| le/simple mode          |
|         -    -6:         |
|                               Index action                                 |
|                               ------------                                 |
|                                                                            |
|           +------------------------+------------------------------------+  |
|           |   b_IndexOperation     |         Operation                  |  |
|           |------------------------+------------------------------------|  |
|           |APCI1710_LATCH_COUNTER  | After a index signal, the counter  |  |
|           |                        | value (32-Bit) is latched in to    |  |
|           |                        | the first latch register           |  *******	i_ReturnValue = -2;
	}

	      tion  | APCI1710_HYSTERESIS_OFF | The first countiata[0]);
		break;

	case APCI1710_I-------------------------+tStat : AuC:
		i_ReturnValue = i_APCI1710_InitC|   |u is co       (32     |b_SecondC.
			s_SiemensCour_ b_ModulNbr            :t latch         |
|                    |	ui_Address + 16 + (64 * b_Movpriv-ounterMod1710_Ini-----odus**********                    interrupt.                   |
|**************/*********************************/
								/* The selec                : DUPDAmine    ****r        	b_ReferenceAction == APCI1710_DISABLE) {
		 /*******     set    noatch t----ting option is wrong\n");
					i |
|                    ccep     Value ------			APCI1710_LOW_EDGE_LATCH_COUNTER
					|| b_IndexOperati of      tion  -------| |
| |--------+
| Input Parameters  : unsigned char_ b_Boar          b_IndbModutatch --------+
| Input Parameters  : unsigned char_ b_Boaron ==
  |
|  710_HIGH_EDGE_CLEAR_COUNTER
					|| b_IndexOperation ==
					APCI1710_LODISEDGE_CLEAR_COUNTE
					|| b_IndexOperation ==
					APCI1710_HIGH_EDGE_LATCH_AND_-------ounter ch--------+
| Input Parameters  : unsigned char_ b_Boar          ********/
				/* Test the index parameter */
		 /********           uns    -----+) {
	      /**pulse fro--------+
| Input Parameters  : unsigned char_ b_B   possibModus |   APCI1710_S*******************/
								/* The selec        enceActionunterelecUL) ==
				APCI1                     interrupt.                   |
|                   odulNbr APCI1		    /********************************/
					/* Test the auto mode parameter */
		    /********************************/

					if    APCI
|                ************/
						/* Test the interrupt mo             ********     dbr].
									APCI1710_LOW_EDGE_LATCH_COUNTER
					|| b_IndexOperati**/
	ot u			|| b_InterruptEnable ==
							APCI1710_D_IndexOperation ==
					APCI1710_LOW_EDGE_CLEAR_COUNTER
					|| b_IndexOperation ==
					APCI1710_HIGH_EDunterMoAR_COU
						Nbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
						auto mode parameter */
		    /********************************/

					Nbr].
						LE_INDEX_ACTION;

			     /*********************************************************/

								DPRINTK("The selected second counter operating option is wrong\n");
								i_ReturnValue =
									-7;
							}
						} else {
			  /************************************************  | - 0400/0228 All Function in RING 0             |
  | ting option is wrong */
/*********omatic mode, each INDEX     |
|                     action is clearterRange,
	unsigned char b_FirstCounterModus,
	unsigned char b_FirstCounte;
		_TestStaAPCI232              i_APCI17terModus,
	unsigned char b_FirstCounterOpt |
|        -----------DisableFrequencyMeasurement PCI1710_LOW_EDGE_LATCH_CO4 The mdexOperatition   ****************************						==
								APCI1710_Llected ------) {
	      /***COUNTER)
							{
				/***PCI1710_LOW_EDGE_LATCH_CO****|      the index level to low (DQ26) */ration
								==
								APCI1710_L7 /**_INDEX_ACOUNTER)
							{
				/*******ration
								==
								APCI1710_L8r].
								xOperatio			||
								bModus |   APCI171-------------------------+ |he GxOperatio |
|        Set*/

			tRlse duXDisable      |
|                     counters.                                              |
|              it-----        b_FirstCounterModus                       ****                                                               unsig****************          								/* Set the              		} else {
	      /***	/urn VaAPCI-1710 incremental counter module                  |
  |                     2: MA );

	    ******        | each counting pulse      FirstCounterister.
							****eR          Ind232                  | the first latch register          is *********		dw_MoCounterInfo.
					ersweier
ter operating mode is wrong.|
|       _)7223/9493-92
	TelINTK(egister.
		ata-com
		&
									Aata.PCI1710_SE---------      ed char *) &data[0]				&
		rsweier
	TelodeRegisteange is wron	&
									ARegister4
									&
									APCI17--------  or           |  he direc------------com
	4                       		for (b_ModulCpt = 0; b|
|    l    ***** 1) { range is wro==
		APCI1710_INCREMENTAL_COUNTEthe second 16 bit    | |
|hanspec), (unsigned char) data[0]);
		break;

	case APCI1710o.
									s_ModeRegis(b_IndexOperation ==
					APCI1710_= i_APCI1710_InitExternalStrobe		&
									APCI1710_SET_HIGH_IND**/
							/* T		CR_AREF(insn->chanspec),
			(priv->s_ht (C) 2
								b_IndexflagmbH f								==
				ter1 |
						e same   | 4
									&
									APCI1710_SET_HIGH_IND     or           |                    -6: The a	dw_Moldule						s_ModeRegister.
									/* Te***************------ latch and clear counter */
			     /**********************           |
|        latch and clear counter */
			     /************************a-com
					s_SiemenLEVEL;
	Nbr].
									s_SiemensCounterInfo.
									s_Modof the board_LOW_EDGE_     _ACH_AND_CLEAR_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOW_ED_INITCOMPARELOGIC:
		i_ReturnValue = i_APCI1710_InitCompareLogic(dev,
			CR_A_AND_CLEAR_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOW_EDGFREQUENCYMEASUREMENT:
		i_ReturnValue = i_APCI1710_InitFrequencyMeasurement(						s_SiemensCounterInfochanspec),
			(unsigned char) data[0],
			(unsigned char)terInfo.
	odeRegiste******** data[2], (unsigned int *) &data[0]);
		break;

	defaul	s_SiemensCounterInfo.
				g : Config Parameter Wrong\n");

	}

	if (i_ReturnValue 						s_SiemensCounterInfo.		i_ReturnValue = insn->n;
	return i_ReturnValue;
}

	s_SiemensCounterInfo.
				          ||
					s_Mod	A|
|                     -6: The auto mode parameter is vpriv->
													/* The sele |
| |b_FirstCounter"---------------------------b_I			devpriv->
ModulNbr].
					s_SiemeterModPCI1710_SET_4nterInfo.
	CounterInfo.
				6--------+
| Input Parameters  : unsigned char_ b_B     _AND_CLEAR_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOble the automatic mode  |
|
| |b_FirstCounterMunterOption & 0x40);

		 /***********************/
				/* Test i      								==
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
									APCI1710_ENABLE_****/

				outl(0, devp*/
		     ==								APCI1710_HIvpriv->
									s_ModuleInfo
									[b_ModulNbr].
	Nbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
					gister2
										=
|
|            velfo.
			or/			b_Indexn
								==
		e == APCI*************------ || b_SecondCounterOption == APCduleInfs (r counter */
			 T ANY WARRANT      | **********				/* ode */
		     *********------)  |       -1>
									s_ModuleInfo
									[b_ModulNbSS FOR A P           COUNTER || b= APCI1710 General selstraße 3       D-77833 Ottersweier   Tatch ------------+
OtOUT A*************************** General Public License as pulished by the Free Sofware Foundation; either ve         APCI1710_ENABLE :       _CLEAR;
							}	/*  if (b_IndexOperation == APCI1710_HIGH_HIGH_INDEX_LEVEL;
							}
s_SiemensCoun/*************er.
	 |
|        |
ulCpt));

		 /***-----+---**********----------------------+ |
| | Parameter       Pa				if (b_        or e /******PCI1710_DISA                Mode				s_ModeRegister. any lAUTO     			dw_MolduleCon		/* Test if latch and clear counter */
			     /************************					s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRe
     -1: The hanensCounterInfo.
									s_Moder.
									s_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
								       ooption is wrong *-----+---****|| b_I		|				ND_CLEAR_COUNTER<< 3      ****Nbr].
									s_SiemensCounter				if (b_Aut            action is cleared ed automatically, else you must read                  |
|                     -3: Co			&
									APCI1710_SET_HIGH_INDEX_LEVEL;
						automatically, else you must read ******************lstrass               module (                      ----ter.
									s_ByteModeRegister.
									b_ModeRegister2
									&
									terModus,CI1710_SEgister2
										=
      unlstrass1ESIS_s ***********---------------------------+
  | Pro				if (b_710_ENAB
					alue range is w--------------c---+
  | tch mo & 0xtails.
@endverb---------Info.
									s_ModeRegiste							s_ByteModeReg(C) ADDI5			s_Mo------------------------------------+
*/

int i_APCI1710_Counters_ModeReg_COUNTER || b_IndexO****IGH_EDGER) */
	                                         |
|               					s_ModeRegiss_ByteModeRegister.
									b_ModeRegister------------------------	s_SiemensCounterInfo.
									s_Moder.
									s_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
									s_ModuleInfo
									[b_Modul********************/

						fo.
								s_InitFlag.
								b_IndexInit = 1;

						} else {
			  /********************************/
							/* Interrupt parameter is wrong */
			  /**************Nbr].
									s_SieSET_LOWemensCoLEVEL			dw_MolduleConfigur                    |
|            
		|| (devpriv->s_ht (C) 2------le
			to     mbH f6)ter2
		*************/

						DPRINTK("The auto moSiemensCounterInfo.
								s_InitFlag.
								b_IndexInit = 1;

						} else {
			  /********************************/
							/* Interrupt paramet                     | afte**************************/
							DPRINTK("Interrupt parameter is wrong\n");
							i_ReturnValue = -7;
						}
					} else {
		       /**************************lag.b_CounterInit == 1) {
	 _AND_CLEAR_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOW_ED***************/
			-------CLEAR_COUNTER || bgister1 = devpriv-						b_ModeRegiAND_CLEAR           egister.
						b__CLEAR_COUNTER |    |
|   
	      /***********/
	*************************/
	)		/* Cou******************/

						DPRINTK("The auto  mode parameter is wroo.
									s_ModeRegiste7ue = -6;
					}
				} else {
		    /************************************************/
					/* The index operating mode parameter is**********************/

						DPRINTK("The auto NTK("Counter notCIndextialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /***************************************************/
		/* The selected module number parameter is wrong */

			  /********************************/
							/* Interrupt parameter is wrong */
			  /********************************/
							DPRINTK("Interrupt parameter is wrong\n");
							i_ReturnValue = -7;
						}
					} else {
		       /************************************/
						/* The auto b_ModulNbrondCounterOpto mo********************6;
					}
				} else {
		     (devpriv->s_B					||
					Operation
				_ b_ReferenceLevel)    |
+---**/
									/* EAR_COUNTER || b_IndexO									b_ModeRegisterER | dulC            : Initialise thitCounter"     er a index siter ing */
	      /***********ByteModeRer */
			     /*****************		APCI1710_SET_HIGH_INDEX_nterInfo.
									s_ModeRegis	 -1: The              unsigned	if (b_ReferenceAction ==
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
									APCI1710_ENABLE_                                             |
|           r.
									s_ByteModeR**********/
	s_SiemensCounterInf****/
							/* Test ifs_ByteModeRegister.
				|                                                                            |
|           r.
									s_Bynfo.
					s_ModeRegof the boardgister1 = devpriv->
to mo)     Included files          ATA GmbH
	DCI1710_******************/
							DPRINTK("Interrupt parameter is wrong\n");
							i_ReturnValue = -7;
						}
					} else {
		       /************************************/
						/s_ByteModeR                            mensCouCTIONolduleConfi) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv-dulCpt = 0-----+---			if (b_ABegin CG	default:				onfig : Config Pa		s_MAND_CLEAR			s_Modu IRQ b_Inbr]. 1)     --------- option is wrong *| Task    ch vPubli				En---------****/

								DPRINTK("The selected second counter operating option is               |
|             |   APCI17&nal signals,               |
|            -+
| Task              : Ini	/* "i_APCI1710_InitCounter"     ****/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter**********/
			/* Counter not initialised see function */
			/* "i_Ae (b_ModulNbr). If a INDEX flag occur, you have   |
|                     the possibility to cleathe 32-Bit cou------unsigned char *_ pb_TestStatu*******************/
								/* The selecodulNbr   AeAction == APchar_ b_RINT			dw_Mo*************   |  Refere------_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
									s_ModuleInfo
									[b_Modul3  |
|                     unsigned char_ b_R----------- : Refe   |  Reference occur ***/
							/* Test i latch and clear countr */
			     /****************s					dev,sr not initialise				 = 1d char_tion[3] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTALdulCpt = 0; .
									s_SiemensCounterIn*******************/
			/* Test if increm (devprivld have r.
									s_SiemensCounterINTER
		|| (de License along wiMolduleCofigurat-------------| ion[0] & 0xFFFF0000UL) ==
		APCI1710_INC
						s_Mod**************/

								devprivSiemensCounterInfo.
						s_ModeRegister.
				urn i_Return
|           ***********/

								devpriNTER
		|| (d License along wi6			dw_Mfigura-------------|  |
|           |APCI1710_eModeRegister.
                   |  two 16-bit counter.              | |
| |             o
			ssed value READnctions in the same   | |
| |     twoodulNbr].
				rModus |   APCI1E_MODE  | Functions in the same   | |
| |       or      | after a ch--| |
| |b_Fi-----------------+-------------------------| |_SiemensCounterInfo.
                    | each counting p                                  |    b_FirstCo------+ |
|                                         CounterModus |   APCI1710_SIMPLE_MODE  | Functions in the same   | |
| |       or            			s_Ge          unterInfo.
						s_ModeRegister.
						s_ByteMod option.  |
+-------710_SIMPLE_MODE  | Functions in the same   | |
| |     _ModulNbr].
nfo[b_ModulerInfo.
					s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister2 &
						APCI1710_RE
	int i_ReturnValue = 0;

	/*******************************/
	                         |  two 16-bit ********/
		/mensCounterInfo.
						s_ModeRegi  PaCounterInfo.
						s_ModeRegister.
						s_Byt                            | botCounterInfo.
						s_ModeRegister.
						s_----fo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister2 = devpriv->
		----ddress + 20 +
					(64 * b_ModulNbrister2 =");
				i_ReterModus and        | |
| |                      or           |   ------| |
| |bCounterInfo.
						s_ModeRegi0_LOW_EDGE_L				dw_ModeRegister1_2LEVEEDGE_LEGI    STATUSlNbr,
	unsigned char b_CounterR----Return Vis pr   /*****FirstCounterModus,
	unsigned char b_FirstCounter------------------CR_RANGE        |
|                      *) &le pth	i_ReturnValue = -4;
			}
		 (devprivNTER)
							{
				/*                           |
| nge == APCI1710_16BIT_COUNTERTK("The selected module number parameter is wrong\n");
		i_Retnse along wiame : INCny oth ((devprilecten  : 2.9(b_ReferenceAct      he auto mNCYMEA%********
+--------------------------------------rameter i                                       |
| ----unter module      	DPRINTK("The selected module number parameter is wrong\n");
		i_Ret*******/
														s_Mdevpri------------------------(devpriStrob            ses the ex
			     /**************************************----= 0;

	/*******************************/
	/* Test if incremental cou+ounterInfoPion  : 2.9s  : unsigned char_ b_BoardHandle    GET-----leInfo
									[b_ModulNbr].
							GRegisteurn i_Re
|                    -------I1710_ECOUNTER)
		|					 unsi      rameters  : unsigned char_ b_BoardHandle     : HREGH_Ev****** is wrong                 |
+-------     odul-------------------------------------------CI1710_ENABLE ||
				b_ReferenceAction == APCI1710_DISABLE) {
		 /***********UASeter is wrong                 |
+-------|ed mlStrobe  : External strobe selection    |
|						0 : External strobe A        |
|						1 : External strobe B        |
CBeter is wrong                 |
+-------CB: E(devpriv-to |
);
				ses the extPCI1710_InitCn witch access of---------ccurs if 							t onsH_AN"0"------------------ the s				-------| |
| |b_F--------------------  |
| -------+
| Ou1put rs  : unsigned char_ b_BoardHandle    ------------------------------LE ||
				b_ReferenceAction == AP-------------------------+ |
| |
|yteMode                -ccurs if to |
LeveD occurs if "1" |
+-------------------------------------------------------------------                -4: Reference level pa*******UER)
		e = -2PCI1710_LOW_EDGE_LATCH_CO/**********---------UD64 * ed occurs if "1" |
+----------------------------------------------------------------------------+
| Output Parameters :LEVE-----+-------------------------+-------------------************FirstCounterModus |   APCI1710_DOUBLE_MODE  | Functions in t------------------------all any ----------------------------  /**********ation        -*****/
		ccurs if "1" |
+dulCptng.|
|    *************         b_SecondCo                    |d char_FIFO       -1: The h, u[---------------r b_ExternalStrobeLn	if (/*].b_Oldr modiMask           ****************urnValue = 0;

	/*he module number */
	/******andll***/

								devpriv->******************ulistember */
	             2**/

	     : Module  wrong */
	      /*********************---------------are openg */
	      /********************ublic L      (insn-****/
		            |
|              I             r					ch1710_Inodule (b_Mod			dw_Molnfigurati	}	lNbr < 4) {
	   /******************    *******-------------         | the first latch registe+iste%NY WARRANTYAVised.Counte    |
|						i           | rotati have rC************/
							DP---------+----------                                          |
|           m*************             -1: The handle parameter of the board is wrong          |
|                    -2: The module is not******************/

		       | each counting pulse        | |
| |bvpriv->
									s_ModuleInfo
									[b_ModulNbr].
						ter range is Cpt] b_Modul       |
|                                      -3: Counter notInfo.
			rsweier
	Tel latch and clear cour */
			     /**************rsweiReturn V_SiemensCterInfo.
									s_ModeRe/************** =nsCounterInfo.
			rswerametster.
	struct)SiemensCounterInfo.
						s_ModeR2Info[b_M          REter ver    			dw_mber */----************deRegister.
				       | pu the sel		   ed m--------------+----------------------+--------------------		/**case APCI)t the e      a(ster.
				)Insn        | each counting pulse        | |
| |b_SecondCtion          | |
| |--_HYSTERESIS_OFF | The first counting     ||
| |       or                                                                 |
|       This option are only avaible if you have selected the direct mode.   |
|                                                       ************************************/
	any other    | pul|
+------COUNTER)
		-------------------------------------------0 :-----        bit counter chains  |
|   br].
						s_SiemensCounterInfo.
				1==dulN_Retu("Thebit counter chains  |
|   ((dw_LathchValue & 0xFF) !=
					((dw_LathchValue >> 8) & 0xFF)
					&& (dw_LathchValue & 0xFF) !=
					(.
						s_SiemensCounterInfo.
	MENT:	/******-------------nding to t*********/
		/* Test the counter range */
	   /*| b_No ==
WARRANon        					i_ReturnValue = -6;
						}
					}
				} else {
iste0_HIGH_EDGE

					devpriv-long wi3			d}
       are openg */
	      /*******2vel =       x************* (b_ReferenceLevel == 1) {
					devpri
|            3 from theng */	/**d module ].
						s_SiemensCounterInfo.
						s_ModeRegister.
		testing the component 1710_CounterAut*************************/

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
			 /*****Value;ster.
						b_ModeRegistervel parameter iModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister					(b_FirstCounterOption & 0x20) |
					(b_SecondCounterModus & ---------------------------+					devpriv->
										s_ModuleInfo
										[b_ModulNbr].
				er.
						.
						s_SiemeCounterInfo.
		BoardHandle,       |
|                   dwg        |
|                     -6: T			[b_ModulNbr].
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
									APCI1710_ENABLE_INDEX_ACTION; wrovel parameter i_ b_ModulNbr        : Module number to coice *s,
	struct crs  : unsii_device *d|     ))------any other X_ACTION;
							}

			    S FOR ********/
					APCI1710_SET_HIGH_INDEX_-------       */
					 reference param(( function wi>>             s_ModeReg4)----+
ion -----------------------------------+
*/

int i_APCI1710_CounterAut  |
|        _INCRESec------  |
	****************/

			ir b_ModulCpt = 0;
	int i_ReturnValue = 0;
	unsigned int d--+
*/

int i_APCI1710!******3: Counter not initialised see fu***********************/
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

			if (         -+
  | P
			  /**************/
		/* The selected module number parameter is wroduleInfo
									[b_M*****************		s_ByteModeRegister.
						b_ModeRegister4 = (devpriv->
						s_Module.
									s_Siedev,
	 to    | AR_COUNTER
								||
								b_Index            | the first er.
						b_->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCountPister2 pul******-------		s_ModeReg& (");
 -he handl(0x10    mber */
	/**********|lse {**/

	if (b_ModulNb ^ 1)    (4 +*/
	   /*********** : -
	   +-----I1710_--------------+--------0);

		 /***********************ccurs if "1" |
+------ || b_ReferenceLevel == 1) {   : A test mode is intended for testing the -------+ld have rn i_ReturnValue;
}

/*
+----------------NTER
		|| ( License along wi5Externalrobe ---- is di     -1: The h| b_********_BoardHandle,    1ue = -6;----------************/

						DPRINTK("The auto mode pturn i_ReturnValue;ce *dev,
	unsign            : A test mode is intended for testin------- Function Name     : _INT        |
|				 unit counter chains  |
|            ed char_		 b_BoardHandle,              |
|				 unsigned char_	      are operated internally as down counters.          to    | ER
								||
								b_IndexOperatio*********/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue ask              : Sets the t0; b_ModulC :							s_Moderaler is wrong */
             ----g.b_CompareL------------------| circuit InitExternalStrobe      ter not initialised see fuon[b_ModulCpt] &
					0xFFFF**************/
					      measurement. The pul_RealTimingInterval returnssibility to clear th*************e. You must call up this function before |
|		      you call up any o           | cir |
|  |b_SecondCounterModuent. Thrs  : unsigned char_ b_BoardHandle     rst latch         |
|                           b_SecondCounterModht (C) 2    /*-----    erIni. A*******moue;
}ARRAN3: Counter not initialised see funed counter range isnsig----onter */
	10    |
|easurement. The pul_RealTimingIntervd valu |
+------------_INDEX_Aisach edg*****s_Sie					devpriv->
										s_ModuleInfo
										[b_ModulNbr].
									APCI1710_DIRECT				 (b_ModulNbr).c						{
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
									APCI1710_ENABLE_INDEX_ACTION;ected seco-----------------------------------------------+
| Input Parameters  : unsigned char_  b_Bany otherPC haof 2TAL_bu********/
							/* The selected second counter oned char_  b*/
	 		b_FirstCounFFF0000UL) ==
		APCs cleared automatically, else you must read    |
|            ter not initialised see fuOUNTER
								||
								b_IndexOperation
								==
			. The pul_RealTimingInterval rAR_COUNTER
								||
								b_IndexOperation
						----------------------------------------------+
| Input Parameters  : unsigned char_ ister4
									|
					r the       PCI1710_SET_LOW_INDEX_LEVEL;
							} else {
				/****er range      PCIInputClock  :		} else {
	      /*************************************		/* The seialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounte*****/
				/*iv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
 (4 + b_ExternalSt and clear r */
			     /*******	/* Test if*********ised *ned c{|       l_PCIInputClock  , ************/

			DPRINTK(---------------8+
| Retunction whicelecNTK("******/
					-----------------------------ter4 = (devpriv->
						s_M	/* Test ifeturnValue----------------------|***************/

		DPRINTK("The s
|        e for the frequency measurement. g\n");
							i_ReturnValue = -6;
						}
					}
				}          | the first latch registealTiming-----val                                                    |
|    *******************ule number */
	/**************************/

	if (b_ModulThe PClocpui        /

					DPRIName     : _INT_r_		 b_PCStarTiming					tCounterModus |
					b_Fi-------InitF3ment(struct comedi_device *de--------+-------------------------+---         after e|
| |--------------       parameter is wrong\n

	return i_R						devpr/
				  are operated internally aselected module 	/**(dev,
			CR_AR    umber           Se***********************************************/
				DPRINTK("The selected first counter operating mode is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /***************************************/
			/* The selected counter range is wrong */
	      /***************************************/

			DPRINTK("The selected counter range is wrong\n");
			   |APCI1710_LATCH_COUNTER  | After a index son & 0x40);

		 /*****************  -5: Timing				} else {
	leInfo
									[b_ModulNbr].
				s_Siemement.           |
|		      Configures the selected TOR incremental counter of the |
|		      selected modul**br,                 |
|**********   Paramer is wrong */  | Function Name     : _INT_ i_APCI1710_InitCompareLogic                      |
	   |                               (unsigned char_   b_BoardHandle,                      |
	   |                                unsigned char_   b_ModulNbr,                         |
	   |                                unsigned int_  ui_CompareValue)                     |
	   +----------------------------------------------------------------------------+
	   | Task              : Set the 32-Bit compare value. At that moment that ondCounterOption == APCI1710_INCREncremental counter arrive to the compare value        |
	   |                     (ui_CompareValue) a in----------------------  |
|						clock                        |
|						- APCI1710_30MHZ :   ster2
	***********************************/

			  the possibility to clear -4: Reference leel phhe module ***/

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
						APCInfo.
						s_ModeRegister.CI1710_32BI_TimingInterval <=
								6UL))) {
		       /**********************/
			||
| |     -Bit comompare value. Atf counter          | ---------*****/
							/* Test if firmware      evice ple/double/simple mode          |
|         --f the board is w       -4: Reference leve----------K("The selecterd is wrong        fo[b_ModulNbr             5535
|    /*****;
							}

			     /********************************** else {
	      /***al >rd is wron200******************importaL))
						|| ((bng 			s_ModeRegiste /******figuratnterInfo     FU--------------------------------------+
*/

int i_APCI1710_CounterAut                     -= 0;
	int i_ReturnValue = 0;
CI1710_SIf counter module found */
	/**********          "i_APCI1710_InitCounter"          Status); */
								dw_Status =
									inl
									(*/
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

			if (/
			  /*************
		 /***           			s_ModuleInfo[b_ModulNbr].
			s         -4:l_Timer         ---signed d    e module number */
	/**************************/

	if (b_ModulNbr < 4) {
	      -5: Timing unity selection is wrong                    |
|   =
								873					0xFFFF) >=
							      |
+----------------------------------------------------------------------------+
*/

int i_APCI1         | /
			  /*       parameter is wrong\n");
  - bus clock */
	      /****      |
|				_PCI    -C-----=arz (DQ30) */
************ur, you have   |
|          int s_Sie(bquarz (DQ30) *
| Task              : A test mode is intended for testing the component and  |
|                     the connected periphery. All the 8-bit counter chains  |
|                     are operated internally as down counters.              |
|                     Independently from the external signals,               |
|            ned char_ b_ModulNbr   				} els-----f the board ithe |              ------dulCpt < TORhe module to be   |
|*******HZ_FREQUENCdulNbr        :0x313APCI1710_	/*
| In				&
*******                        | the first latch registe			/* Test i-----------------****************/
		/* Tes  |  Reference occlock,
	unsigned char b_TimingUter4 = (devpriv->
						s_Mo	APCI1710_SET_HIGH_INterInfo.
									s_ModeR--------------1 = ***************Nbr].
	        | after a chaodeRegister4
							------      ************ar b_PCIInputC	s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRege value. At that mome----5)						APCI1710_DISABLE_-----------20ENCY;

					
*/

int i_APCI1710&-----------------------  if (b_PCIInput40gned cbr,                 |
|		egister1 = b_Cif d_ pul************ /*******_INC******dule numb                                      Disable the automatic mode |
|                     unsigned char_ b_InterruptEnable : Enable or disable the        |
|                                               interrupt.                   |
|                                               APCI1710_ENABLE :            |
|                                             ---------                        ------------------------------------1.5CounterInfo.s_InitFlag.b_Co************/

							if ((devpriv-Register4 = (devpriv->
						s_--------range is wrong\n");
		) >(DQ30) *)) >=
	  igned)ue
						     ReferenceLevel)    |
+----------------------40MHz qng */
	      /**************
|       vel ^ 1) << (4 + b_ExternalStar b_TimingUnity,
	unsigned int ul_TiminPCI1710_SET_HIGH_I"The yteModeRegi-------devpriv->
				 comedi_device *dev,
	unsigned c-+
| RetuulNbr,
	unsigned char b_PCIInputClock,
	unsigned char b_TimingUnity,
	unsigned int ul_TimingInterval-----+
*/

int /
	 ********************/

		DPRINTK("The s    selected module (b_ModulNd second counter o, unsigned inleInfo
									[b_ModulNbr].
									urnValue = -2;
	}

	retur025
										*
										(douit count****/
									/* Enable the 40MHz cense alon                                              (rst latch         |
|                     ul_TimingUnity determine the time bas INDEX action. ore |
|		      you call up eInfo
									[b_ModulNbr].
				->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCountr].
						s_SiemensCounterInfo.		le number +---------rameter i			
									br************* |   APCI1710		=
			ou call up )eRegister4 & (0xFF -
							(0x10 << b_ExternalStrobe))) | ((b_ExternalStrobeLevel ^ 1) << (4 + b_Externas wro3MHZngInte       b_ModulCpt));

		 /***********					******/
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
					(
+-----------8738æs_Sieme-----if direct mo					/* Round the valu        _ModulNbr        : Module number to cFFFF0000UL)*********e)ul_TimingInterval * (0.00025 * on
								==
								APCI1710_LOW_EDGE_CTestStatus =
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
|                                                 unsigned char_ b_ModulN                 |
|						clock                        |
|						- APCI1710_30MHZ : REQUE fator */0)
							&& (ul_TimingInterval <=
								655structACTION	fpu_b----(t the exter  Passedb/

				Unity			devpr							
									/* Cnculate the rturn i_ReturnVa    D0:------+].
			s_SiemensCounte								/* C***** 0 facto---------		b_PCIInputClock);

								break;

				/******/
														2;

          int /*******doubl*/

								fp									-
		*			if ((doub0.00025		(uluarz (DQ30) *igned ch----------------------                  : Initialise the index corresponding to the selected     |
|                     module (b_ModulNbr). If a INDEX flag occur, you have   |
|                     the possibili\n");
				i_Re - 0400/0228 All Function in RING 0          1********/

of the board is wrong ulte  b_Inde=e reference paramdulN									(&******IndexStatus")        |
|                     after each INDEX action.                               |
|                                                                            |
|                                                                  ***********************/
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

			if ((           -1: Th*******/
	*******/

						if (i_ReturnValue == 0) {
											s_ModuleInfo
									[b_ModulNbr].
									-------------------+
| Task              : Sets the er operating mode is wrong.|
|      					&
									A* Test if counter gisturn i_ReturnValue;
he PC h		=
			         -1: Th/

									*pul_RealTimingInterval
						&& (ul_TimingInterval <=
					ong\n");
		i_ReturnValue = -2;
	}


								number */************/

									if ((doubleouble)ul_TimingInterval * (250.0 * (double)b_PCIInputClock)) >(double)((double)ul_TimerValue + 0.5))) {
										u							/* CRe you
			erInit********** = -2;
	}

	return i_ReturnVa*******											=
			val * (250.0 * ( * ***/
			tion is le)((double)ul_TirValue
											=
			*************+ 0.5********/

	ifreak;

				/******/
											if ((douity selection is wrong */
+	    /******									brember */
ng */
	      /****************    /*********Calcdouble)ul_TimingIntt
				 **********RealTimingInterval
	ble)b_PCIInputClock)) >= (double)((doubl		 unsign/

						DPRINTK("is wro0MHZ*************				=
							****2***************/

								fp**************1			DPRINTK("The selected PCI in<				if ((d8**********mpare										s_Moduoard is wrong        					***************************/

****************/

								fp***************4				DPRINTK("The selected PCI in -4;
			}
	7943757} else {
	      /****************************************/
			/* Counter not initialises wrong\n");
				i_ReturnValue =put clock is wrong\n");
				i_ReturnValue = -4;
			}
	****} else {
	      /**/
	   /---------------------------------------------------------------| |
| |b_FirstCounterOpr is wron  |-----------------|-----		2;

									break;

				/******/
								   /******************************								2;

2	   /****	s_M*/
	   /*******mingInterval
								m	*
										(250.0
										*
2									b_PCIInps_ModuleInFREQUE				/*******************/
									/* Round the value */
					/*******************/

									if ((dourval * (250.0 * (double)b_PCIInputClock))250.0double)b_PCIInputClock)le)((double)ul_T selection is wrong */
		       /**********************************/

						DPRINTK("Base timing selection is wrong\n");
						i_ReturnValue_APCI6;
					}
				} else {
		    /***********************************/
					/* Timing unity select*****/

	if         e external								devpriv->
									s_ModuleInfo
									[b_ModulNbr								s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
					#########} else {
		    /***********************	/
			/* Test the INC_CP**********nsigned char_ b_ExternalStrob#      |
+---------*************************~e     :INT	i_APCI6;
					}
				} else {
		    the exter			/* Test the INC_CP |   APCI1710_HIGH  selected counter ra*/

								DPRINTK("Counter not initr_ b_ModulNbr        : Modu                                    |
|                                "i_APCI1710_InitCount (double)b_PCIInputClock)-------------------------------------+ion is wrong\n")Parameters : -         **/
				/* Timing uniParameters : -                  */
		    /******= current;	/*  Save the current pro****/

					DPRINTK("Timing unity**********-----------------------------------------------+
| Function Na								2;

									break;

				/******/
								nValue;
}

/*##############################################      the externaAPCI1710_				endul_Time******************************* (devpriv->s_ptioneInfo[*****************k;

	case APCI1710_INCCPT_SETINPUAPCI1710_InitFr************ment(struct comedi_devicv,
	unsigned 32								(ul_TimerValue
				      | value (32-Bit) is latched in to   parameter is wrong2         |
| eReg1710_SetInp
	str   |PCI1710_INCREMENT | The counter increment for  | | -7: 40MHz quartz not on board                          |
+-------------------------------------------------------------------|       or             |  >
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCount		=
			nsigned clag.b_CounterInit == 1) {			}

					/**********************--+
| FunctionER
								||
								bNTER
		|| (devpriv->s_BoardInfos.
			dw_3;
Return*******(UAS)c));
		break;

	case APCI1 */
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
					i_ReturnValue = -5             |C{
		 /UAS:
		ata["0  |--------gister.
					b_ModeRegister1 =	/*  if (b_PTAL_-----ngInte		s_Siemen"									(2
		if (devpriv->
			s_ModuleInMaketurnVa  ADDI-DATA Gmcommandoter2
										&***									devpriv->
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

			   ******struct ############# */

							/* INSN BITS */
/*#############################     Descr############################## */

/*
+----------------------------------------------------------------------------+
| Function Name     :INT	i_APCI1710_InsnBitsINCCPT(struct comedi_device *dev,struct comedi_subdevice *s,
struct comedi_insn *insn,unsigned int *data)                   |
+----------------------------------------------------------------------------+
| Task              : Se-------------------+
| Output Parameters : -    Nbr,
	unsigned char b_CounterRange,
	unsiged second cou				/******/
						/    /******	(2>>    --------counter module found */
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
		for (b_ModulCpt = 0; b_ModulC),
			(unsigned char) data[0], (unsigned char) data[1]);
		break;

	case APCI1710_INCCPT_LATCHCOUNTER:
		i_ReturnValue = i_APCI1710_LatchCounter(dev,
			(u--------APCI1710_INCREMENT | The counter increment for  | |[0f the fo1710_      Description      SET any ANDobe))) | t will |
| |-------------------------Se					AAnd         stribu+--------               | |                                      | one r not initialis--------lag.b_CounterInit == 1) {
	      /********************************/
			/* Test the reference parameter */
	      /***************reak;

	casulNbr). overOpera));
		break;

	case APCI171 */
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
					i_ReturnValue = -5**************{
		 /----------/*****************************/

									*pul_RealTimingInterval
			10_SIMPL            |
|                                unsigned char_          b_ModulNbr,       b_SecondCounterModu("The sel								erInitalue dulNbr        : 										s_ModuleInfo
										[devpriv->
	)Modus |   APCI1710_SIMPLg\n");
								i_ReturnValue =
									-7;
							}
						} else {
			  /****************************            |
|                     -4: Reference: unsigned char *_ pb_TestStatus          |
+-------------------------------------------on == APCI1710_ENABLE ||
				b_						/* Makte the configuration commando */
			  *********************/
				/*
	      /** The selected crs  : ############# */

							/* INSN BITS */
/*#############################*********			s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									&
									APCI1710_DISABLE_INDEX_ACTION;
							}

			     /******************************* -3: Counter not initialised.       utomatic mode, each INDEX     |
|                     action is cleared********/

									*pul_RealTimingInterval
						APCI1710_LOW_EDGE_LATCH_COUNTER
								||
								b_IndexOperation
								==
								AP1                  AR_COUNTER
			*********                   -------------------;
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
			(u     |
+-----sed\n");
								i_Return
									s_Siemens**********/
							/* Test the INC_k stru   |
|                     -5: Timing unity selel_RealTimingInterva----------erIniublic L0nterval
					-----------------------------------------+ of the |
|		      selected module (b_ModulNbr).  /***************************/

								DPRINTK("Counter not initialis -6: The auto mode para(ters  : unsigned chat***************2*-+--ATES------|  |
|                    the possi          -6: The auto mode par			/**********************************************/
				DPRINTK("The *****************/
				DPRINTK("The selected first counter operating mode is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /***************************************/
			/* The selected counter range is wrong */
	      /***************************************/

		erval <=
						 |b_SecondCounterMod         APCI1710_ENABLE :            |
|                                               Enable  |
+----------    -------------         .
					s_ByteModeR******************D-----------lock =0MHZ_F**/
				/* Ref							meter is wrongul_TimingUnity dernValue;OEF(insn
			lTiminsth   b           10_LOW_EDGE_LATI1710_DIrs  : unsigned char_ b_Bo***********putFilGH_EDGk				ted iREF(insnas a PCI buthe selected		- ****TINPUTF   : Initialises the ext	-***************de parameter */
********    _ pul_RealTimin== 1****|
|						  3 				tz		     lNbr).                        |
|               ----------------------------------------                     |
|                                                                            |
| +----------------------+--------------------+----------------------------+ |
| |      Parameter       |     Passed value   |       Description          | |
| |----------------------+--------------------+----------------------------| |
| |b_FirstCounterOption  | APCI1710_INCREMENT | The counter increment for  | |
| |------------------------Clock, |
|	ption          See****************/

ISABLE) {

			    insn->n;
	return i_R.
									b_ModeRegister2
	Firect   revis GmbReturnValue = i_APCI1710_InitFrequency
|                     -2: No counter module found                            |
+------------------    |
+-----------------_ReadIndexStatus")        |
|                     after each     AllC	*pul_Realsue =53590				ENCY|					14:      1      |
+----------3ns (0.46880tion nterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
			INTn *insn,unsignedBitshar_          b_FirstCounterModus,         |
|                                unsigned char_          b_F
|		      unsigned char_  b_ModulNbr	      :	Number of the module to be   |
|						configured (0 to 3)          |
|		     ht (&-------                                 unsigned 				bSiemensCo: |					r------726b_Mo(1.357741MHz**/
				ave enabled the automatic mode, each INDEX     |
|                     action is cleared  (0.577777MHz) |
|					13: Filter from evice 10)subde331ns*********/

							if ((b_SecondCou|
|               0	Fax333Mver0000M             -4: Reference level parterval)  //********----------------------------modi       ATA Gmbstrassthe terms of|| ((bIGH_E>ction */****6ny other>=
								200)
strass - 0400/0228 All Function in RIN					=
										(u								APCI1710_SET_HIGH_INDEX_5	&&  (0.500000MHz) /*****************************   |
+-------0eclear cou---------+ |
| |    Parameter     0000MHz)0:  S        f    |
---------ar b_Ext0000MHz) :ramether ver(p          Vation******      [ed char) data].0/0228 All Fuz) |
|					37MHz) 1815n333M4(0.500000MHz) 5
|						1293|			------MA 02111-1 waPublic License for more detai				if (b_|					12:0.500000MHz)9erna
									s_ByteModeRegister.
										*pul_nitFlag.
								b_IndexI		3 (1.180637MHz) 400					2.                       00000MHz) |
50000MHz)       (0.500000MHz)5rom 1100ns (0.960000MHz1.6|
|		from 8000MHz) |
|00000M(1 |
|					11: Filter 10.000000MHz) |
|100nr modify i from 1200ns 					DP Filter from 1500shed by the F (0.577777MHz) |
|					13: Filter) |
|75131ue =751310000MH------------------------------+
*/

int i_APCI1710_Coun***********(1.2     from 800ns  (6rom 1100ns (0.90.500000       unsigned char_          b_FirstCostCounte                                   |
+------------low (DQ26) */
	range                         *****/
								/* Test th00MHz) |
|31300ns0.7692                3.000000MHz) |
|00000M    14    |
|          2.000000MHz) |
|09090M(0 |
|					12: Firom 600ns  (10MHz) |
|33333M
|  +
| Ofrom 80rs  : u}00MHz) is                 |
|                     -1: The handle parameter of the board is wrong        terModus,         |
|        vpriv-nter module found */
	/********************************/

	if ((devpriv->s_BoardInfos.
			dw_MolduleConfiguration[0] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[1] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTA								APCI1710_LOW_EDGE_LATCH_COUNT0.00025
										*
										(doub   : Initialises the exte*/
		/* Test the cbr)      0MHz   terval                   | period                   -------------                                     |
| +------------------------------------+-----------------------------------+ |
| | Parameter       Passed value       |        Description                | |
| |---------------------------------********************/

	if (b_                        |                 | one 1 not initialised see function */
			           | |
| |-------------------------|    -------+-------- 0) *
+---***********/

						DPRINTK("The auto mode me for the frequency measurement.           |
|		      Configures the selected TOR incremental counter of the |
|		      selected modul_ModeRegister4
		n(dev,
			(unsigned char) CR_ARE License along wi**************  : A test mode is intended for testing the component ******************/

									*pul_RealTimingInterval
					DPRINTK("Counter not initialised\n");
					 eit				/* nsigned char_ b_ModulNbr        : Module number to c****************/
				/* The selected PCI inpute)*pul_RealTimingInterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
											+
									|					14						er functiondulNbr].
									s_SiemensCounterInfo.
						Cpt] &
					0xFFFF00 APCI-1710    |
|		      unsigned char_  b_ModulNbr	      :	Number of the module to be   |
|						configured (0 to 3)          |
|		      unsigned char_									(unsigned int)
					*/

				| ((b_PC             unsigned char_       b_ModulNbr)       clcase APCI171ead tDECREMENT))
					|| (b_FirstCounterModus !=
						APCI1710_DIREC***/
		rtzodeRegist0ue = -6;------------------------------------------------**********&ed cu               Parameters  : unsigned char_ b_BoardHandle : Handle of board APCI-1710        |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
+------------------index lein to the first latch         |
|                     register. The b_Ind								unterValue(struct comedi_device *dev)
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
		|| (devpriv->s_BoardInfos.is wrong         |
|              			(unsigned int)
										(ul_Timong             |
*******/

		DPRlue = -5					(ul_Timal{
		 /***							/* Round the value ------------+
| Output Parameters : -                  						if ((double)((double)ul_TimingInterval *0UL) ==
		APCI1710_INCREMENTAL_ >= ((doub--------------------+
| Return Value      :
+-------------------------------****ion is APCI1710_INCCPT_CLEARALLCOUNTERVALU					[b_ModulNbr].
	double)b_PCIInputClock))
									b_ModeReuct comedi_device *dev,sng selection is wrong\n");
				est if 4/le nr].
									s_SiemensCounterInfo0;
	ui_BitsType = CR_CHAN(insn->chanspec);
	devpriv->tsk_Current = current;	/*  Save the current process task structure */

	switch (ui_BitsType) {
	case APCI1710_INCCPT_CLEARCOUNTERVALUE:
					(ul_TimerValue
				*************/

			DPRINTK(K("The selected counter range is wrong\fo[b_ModulNb /***/****5er.
					s_ByteModeRegister.
					egister1 = b_Counthas a 40MHz   tCounterModus |
					b_FirstCountn;
			} else {**************** APCI1710_40MHZevpriv->

										s_Modu								5);

	****/	devpriv->
								s_ModuleInfo
						
			Zv->tsk_;

	case APCI1710_INCCPT_SETINPUTFILT********/

		ihe se***********************ModulNbr		s_ModafuleIn\n");
					i_ReturnValue unte
									s_Mode***** (unsigned char) data[0]);
		break;

	case APCI1710_INCCPT_INITEXTERNALSTROBE:
		i_ReturnValue = i_APC710_InitReference(dev,
			CR_AREF(insn->chanspec), (unsigne		s_Sieme********************unterInfo.
				& 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_R_COUNTER
								||
						uquarto    ***********************/
			/* Test if increme char) CR_*************/
						nvel ^ 1) <<******/
		/                    v,
	unsignedAPCI1710_36_3_4,
					har) data[1]);
		break;

			     /*************************--------------------+-uartz not on board */
			     /*****************************/

								DPRINTK("40MHRegister1 =lock =;

	case APCI1710_INCCPT_LATCHCOUNTER (devpriv->s_/

							outAPCIl(devpriv->
					******************************/
			w_MolduleConfiguraelected filter va is wrong\n");
		i_ReturnValue = -2;
	}

	returnb_BoN							[b_ModuldulNbr].    unsigned char_  b_Filter	      : Filter selection             |
|                                                                            |
|				30 MHz                                       |
|				------                                       |
|					0:  Software filter not used         |
|					1:  Filter from 266ns  (3.750000MHz) |
|					2:  Filter from 400ns  (2.500000MHz) |
|					3:  Filter from 533ns  (1.876170MHz) |
|					4:  Filter from 666ns  (1.501501MHz) |
|					5:  Filter from 800ns|                                           b_ModulNb     ------ter 10_ENABLE_CLEAR_COUNTER
					|| b_IndexOperation ==
					APCI1710_LOW_EDGE_CLEAR_COUNTER
					|| b_IndexOperation ==
					APCI1710_HIGH_EDGECI1710_ENAB           			0x8) >>
								3);

				Flag.
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
						/* The auto mode parameter is wrCOUNTER.
			_FILTEed char_terval)       |						s_SiemensCounterInfo.
				ounterInf		 b_ModulN/
		       /*************er from 700ns  (1.428500MHz) |ls.

Yo F			(~Ars  : unsignedterval)       *******/
								/* Set the index level to low						s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
					-------Mode ==
*********************************************/

								DPRINTK("The selected second counter) ==
				APCI1710 */
				/*******					NTER
								||
								b_IndexOperation
														b_ModeRegister4
									=
									devpribr].
								s_SiemensCounterInfo.
								s_n board\n")*/
							/* Test if latch and clear counter               |
|				(rval)       |					11: Filter 8rom 1100ns (0.993333MHz) 1.0 * (double)b_Pnity*/
							DPRINTK("Interrupt parameter is wrong\n");
							i_ReturnValue = -7;
						}
					} else {
		       /************************************/
						/* The auto ******************/
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

	ReturnValue;
}

/*
+----				b_IndexInit = 1;

						} else {
			  /******int ul_TimerValue = 
						ed char) data[1], (unsigned				&
									APCI1710_SET_HIGH_INDEX_LEVEL;
							
	      /*************/

			DPRINTK(				&
									APCI1710_SET_HIGH_INDEX_LEVEL;
			truct comedi_pbr from ----+  |
+-------------********************************Nbr).    itialisu      *********************************************        |
|				 ULONG_ 	ul_EDGE_LATCH_AND_CLEAR_COUNTER)
							{
it counter chains  |
& 0xFE) |
				s_Mod(/********evprivn bon boardota[4f th***************/
								---------not on bosametim								APCI|               |
***************/
								) |
|					7:  					b_SecondCoutInputFilter(struct c  |-riv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulNbr] &
					0xFFFF) >= 0x3135) {
		 /**************************/
				/* Test the PCI bus clock */
		 /**************************/

				if ((b_PCIInputClock == APCI1710_30MHZ) ||
				oduleInfo[b_ModulNbr].
			s_SiemensCoun      |
|					(unsigned char_ b_BoardHandle,                |
|					 unsigned char_ b_Module,           ed\n");
								i_ReturnValue =
				_ b_ModulNbr        : Module number to 			- APCI1710_4				######sCount---------------/
		     e you must re********************************/
							DPRI**/
										s_ModuleInfo
		turn i_ReturnValue;
}

/*
+------------------------		/**********cy     o        ***********************/

			if (b_LatchReg < 2) {
					ta	      /**************turn i_ReturnValue;
}

/*
+------------------------le found           R_AREF(i1lNbr].
									s_SiemensCounterInfo.
									s_Modb_Bo	(ul_TimerValue
	opp-----------/

	if (b_ModulNbr < 4) {
	   /******************n boa*/
								*/

		i   |ch r0_HIGH_E || b_ReferenceLevel == 1*/

						DPRINT     tClock, unsign******eRegister4
									&
									APCI171						20 +
								(64 * b_ModulNbr));
						}	/*  if ********/

				DPRINTK("The selected latch register p 16) */
					else {
		       /********/

				DPRINTK("The selected latch register  The selected filter value is we for the frequency measurement.           |
|		      Configures the sPRINTK("The selected fil************=
								87automatick == APCI17						||
								b_In latch register parameter is wrong */
		 /***********							APCI1710_LOdefeturn			DPRINTK("Counter not initialised\n");
								i_Retturn ba{
		   *********/

	ifRealTimingInterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
											+
												LATCH_AND_atus | (1 <<
						b_ModulCpt);
				}

		 /*******	  /*********************************/

	**************/
							/* Makte the configuration commando */
			     /********************************
							/* Makte the configuration commando */
			     /*******************         dulNbr12: Filter from 1733ns (0.577777MHz) |
|					13: Filter from 1le : Handle of board APCI-1710        |
+----				      (b_IndexOperaticorrespond-----nfigurdulCpt < *********************/

gned input Parameters : -                              Value >> 16) & 0xFF)
					&& (dw_LathchValue & 0xFF) !=
					((dw_LathchValue >> 24) & 0xFF))ReturnValue;
}

/*
+-----  |
|						clock                        |
|						- AP**********/
					***/*******						&
									APCI1710_/
							
		       /automaticaBoardHandle,       |
|                    i_******(insn-unterInfo.
									s_Mode: Handle of board APCI-1710    |
	   |                   | Error detected of counter 0 |             | wrong\n");
		_SourceSelection :_Ret              071800MHz) |
*/
		                         |
ourceSelection : APCI1710_SOURCE_0 :          |
|						The index logic is connected |
|						to tAPCI1710_LO-----------LATCH_AND_at onl| Output logic is connected |
|						to sCounterInfo.
				s_Med char_ b_BoardHandle,       |
|       |
|						configuration.               |
|						APCI1710_SOURC----igned char_ b_SourceSelection : APCI1710_SOURCE_0 :          |
|						The index logic is connectrval + 0.5)) {
										*pul_RealTimingIn|
|           ifs_ModeRe---------+
| Output Parameteilter fromtch access of        |
|                     counters.                                              |
|              ie)*pul_Rea      /******************10_*************/		s_Mode
									s_ByteModeRegister.				=
		28from 800ns  (*********       40quartz		     |                                    PCI1710_LOW_EDGE_						b_IndexInit = 1;

						}				if (b_A                 br). the difference i
					*****/

	----------111  /**     :INT	i_APCIrom 20
			dw_MolduleConfiter ----------------: +19(0)z) |
|					2:  Filter from 300ns ---------         ned ************evpriv->
				SS FOR ogran[b_Modul*/
									***********       b_FirstCo.
								dulNbr      .
									CTION;
				ree Softwar: The selected module           ister.
		                            APCI1710_QUADRUPLE_     OUNTE180637MHz) |
|					7:  Filter )********b_ModulNbr   Paramete									80637from 800ns  (7rom 1100ns (0.The selected 94dule insn *insn,unsigned int board is wrong         |
|                     -2: The selected module n              --------dule >=
								266)
							&	s_Moduselstraße 3       D-77833 Ottersweier  |
 ese********----------------0MHZ_FREQU1_Modsebr)WITHOnfigd in to    |  |
|    firm     >= Rev ((doubl== APCI1710_DIm 110e PC PCI input om 600nsU) !->
			s_Mof ((devprTest the quartz fla           ReturnVaINPDWlterE)
						se) {
			  			FFF) >= 0x313*/

		if (dTO_MODE)rating g */ut Parameters :PCI1n;
			} elsdistriLE)
						selFFFF) >= 0x313ourceSelectbuted iER || b_Ind	&& (u latch regis						s_PCI17.
					s_Byte					
		DPRI					             				s_By(b_L---------dInfos.
					dw_MolduleConfiguration[b_ModulN		     /*************************2 4) {
	   /***->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								s_ByteMo	b_ModeRegister1 = b_Count == APCI17*******tCounterModus |
	(turnValue = 0;
t will s wrong		s_Mode	 /*************6_SourceSelec/*****************Ier4
							******PCI input   /*****************	s_ModuleInfo
							[b_Modu							b_PCIInputClock);

									if ((dameter is deRegister.
	cond counter ope******6er4
								=
	lNbr]tch registe*************dulNbr      .590318MHz) |
|					14:terInfo.
									s_ModeRegctio 600nsatch r|      veuleInfo------a					s_Bytalue;
	erbatim}vrbatim

Copyrig} else {Copyrighif (*pb_UDStatus == 1)5  ADD of t/*s module.

	ADDI-DATA GmbH
	Dieselstras/Copyright/* Test if 16-bit counter 2 pu2005occur se 3
	f thhi module.

	ADDI-DATA GmbH
	Dieselstrassse CopyrightA Gm(*pul_ReadV/**
 & 0xFFFF0000UL) != 0e code  redisui_16BitrograCopyright		=/ormoduify (unsigned int)t under the GNU General m

T)722pt andt under the	>>d byeral Fre16 GNU General&t under thes freeU)@/or modifyse as publisheSoftware FoiGNU General (at your option) any laterfo@asion 2of thhe Lation; eithe|tion; eithe Pr opc Licene that  - ribute it and) << 16Y WARse, ort (C) 20ht4,3/94code -DATTA GmbH
 foneral warranty sour2ou can r7223/9493-92
	http://www.addi-data-com
	inf3
	D-778833 Ottersweier
	Tel: +19(017223/9493-0
	Fax: +ation; eitheTHOUT ANY WARse A PAmore detailss.

You shta.cose as publishem iof the are; ySee the disttthout even thet under theed in the .

e termspe that tion; eithe	*d also find at your option) any late MA 02111- 2 pe that  warranty   (atnc.,r option) any late MA 02111-1307This program is distributeeccompanying(0)722so softwa
/*
  +-C) Aeful, but WIARRANTY; -DATA GmbH
 - Dieselstra�hout evens source code.

of MERCHA
of MERCHAN (C) 200CopyriNTABILITYe.

FItra�on.

Tho 1icense, oOR A PARTICso 0icense,C) AD---------------/9493-info@ad(0) f th
  | Tel : +49 i-data.9493-0     | ema)7223/9493-92
	http://www.addi-data-com
	ie details.

Y/*----quency measurement logic not initialisedneral 93-92  m   I+19(net : http://www.addi-dao the ----
CopyrDPRINTK(": info@add                    |
  | Module \n"rece  |i_Returntware = -4icens493-0     | ema tha)7223/9493-92
	http://www.addi-data-com
	inf3
	/* Ca copy                 see func
*/
neral P/* "i_APCI1710_Ini3/9493--"----  ----  	Fax:  | Da2002     | | (C) AD |
  +------------------CopyCompiler  |
  +-----------------ame : INCCPT.C2        Ver3icen}
.96/2002      -----------------+
  | Project     : API APCI171------ould

Thilectedmodule. number parameopy is wrong :  02/1 inc------alel: +19(0                            |  
 |
  +-----                        |
  +-------------------------------+
  | Desc2;
	}

	r----+
-------------;
}
