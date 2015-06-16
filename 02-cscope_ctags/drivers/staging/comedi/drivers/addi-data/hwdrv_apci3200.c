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
  +-------------------------------+---------------------------------------+
  | Project     : APCI-3200       | Compiler   : GCC                      |
  | Module name : hwdrv_apci3200.c| Version    : 2.96                     |
  +-------------------------------+---------------------------------------+
  | Project manager: Eric Stolz   | Date       :  02/12/2002              |
  +-------------------------------+---------------------------------------+
  | Description :   Hardware Layer Acces For APCI-3200                    |
  +-----------------------------------------------------------------------+
  |                             UPDATES                                   |
  +----------+-----------+------------------------------------------------+
  |   Date   |   Author  |          Description of updates                |
  +----------+-----------+------------------------------------------------+
  | 02.07.04 | J. Krauth | Modification from the driver in order to       |
  |          |           | correct some errors when using several boards. |
  |          |           |                                                |
  |          |           |                                                |
  +----------+-----------+------------------------------------------------+
  | 26.10.04 | J. Krauth | - Update for COMEDI 0.7.68                     |
  |          |           | - Read eeprom value                            |
  |          |           | - Append APCI-3300                             |
  +----------+-----------+------------------------------------------------+
*/

/*
  +----------------------------------------------------------------------------+
  |                               Included files                               |
  +----------------------------------------------------------------------------+
*/
#include "hwdrv_apci3200.h"
/* Begin JK 21.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */
#include "addi_amcc_S5920.h"
/* #define PRINT_INFO */

/* End JK 21.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */

/* BEGIN JK 06.07.04: Management of sevrals boards */
/*
  int i_CJCAvailable=1;
  int i_CJCPolarity=0;
  int i_CJCGain=2;/* changed from 0 to 2 */
  int i_InterruptFlag=0;
  int i_ADDIDATAPolarity;
  int i_ADDIDATAGain;
  int i_AutoCalibration=0;   /* : auto calibration */
  int i_ADDIDATAConversionTime;
  int i_ADDIDATAConversionTimeUnit;
  int i_ADDIDATAType;
  int i_ChannelNo;
  int i_ChannelCount=0;
  int i_ScanType;
  int i_FirstChannel;
  int i_LastChannel;
  int i_Sum=0;
  int i_Offset;
  unsigned int ui_Channel_num=0;
  static int i_Count=0;
  int i_Initialised=0;
  unsigned int ui_InterruptChannelValue[96]; /* Buffer */
*/
struct str_BoardInfos s_BoardInfos[100];	/*  100 will be the max number of boards to be used */
/* END JK 06.07.04: Management of sevrals boards */

/* Begin JK 21.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */

/*+----------------------------------------------------------------------------+*/
/*| Function   Name   : int i_AddiHeaderRW_ReadEeprom                          |*/
/*|                               (int    i_NbOfWordsToRead,                   |*/
/*|                                unsigned int dw_PCIBoardEepromAddress,             |*/
/*|                                unsigned short   w_EepromStartAddress,                |*/
/*|                                unsigned short * pw_DataRead)                          |*/
/*+----------------------------------------------------------------------------+*/
/*| Task              : Read word from the 5920 eeprom.                        |*/
/*+----------------------------------------------------------------------------+*/
/*| Input Parameters  : int    i_NbOfWordsToRead : Nbr. of word to read        |*/
/*|                     unsigned int dw_PCIBoardEepromAddress : Address of the eeprom |*/
/*|                     unsigned short   w_EepromStartAddress : Eeprom strat address     |*/
/*+----------------------------------------------------------------------------+*/
/*| Output Parameters : unsigned short * pw_DataRead : Read data                          |*/
/*+----------------------------------------------------------------------------+*/
/*| Return Value      : -                                                      |*/
/*+----------------------------------------------------------------------------+*/

int i_AddiHeaderRW_ReadEeprom(int i_NbOfWordsToRead,
	unsigned int dw_PCIBoardEepromAddress,
	unsigned short w_EepromStartAddress, unsigned short *pw_DataRead)
{
	unsigned int dw_eeprom_busy = 0;
	int i_Counter = 0;
	int i_WordCounter;
	int i;
	unsigned char pb_ReadByte[1];
	unsigned char b_ReadLowByte = 0;
	unsigned char b_ReadHighByte = 0;
	unsigned char b_SelectedAddressLow = 0;
	unsigned char b_SelectedAddressHigh = 0;
	unsigned short w_ReadWord = 0;

	for (i_WordCounter = 0; i_WordCounter < i_NbOfWordsToRead;
		i_WordCounter++) {
		do {
			dw_eeprom_busy =
				inl(dw_PCIBoardEepromAddress +
				AMCC_OP_REG_MCSR);
			dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
		} while (dw_eeprom_busy == EEPROM_BUSY);

		for (i_Counter = 0; i_Counter < 2; i_Counter++) {
			b_SelectedAddressLow = (w_EepromStartAddress + i_Counter) % 256;	/* Read the low 8 bit part */
			b_SelectedAddressHigh = (w_EepromStartAddress + i_Counter) / 256;	/* Read the high 8 bit part */

			/* Select the load low address mode */
			outb(NVCMD_LOAD_LOW,
				dw_PCIBoardEepromAddress + AMCC_OP_REG_MCSR +
				3);

			/* Wait on busy */
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			/* Load the low address */
			outb(b_SelectedAddressLow,
				dw_PCIBoardEepromAddress + AMCC_OP_REG_MCSR +
				2);

			/* Wait on busy */
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			/* Select the load high address mode */
			outb(NVCMD_LOAD_HIGH,
				dw_PCIBoardEepromAddress + AMCC_OP_REG_MCSR +
				3);

			/* Wait on busy */
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			/* Load the high address */
			outb(b_SelectedAddressHigh,
				dw_PCIBoardEepromAddress + AMCC_OP_REG_MCSR +
				2);

			/* Wait on busy */
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			/* Select the READ mode */
			outb(NVCMD_BEGIN_READ,
				dw_PCIBoardEepromAddress + AMCC_OP_REG_MCSR +
				3);

			/* Wait on busy */
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			/* Read data into the EEPROM */
			*pb_ReadByte =
				inb(dw_PCIBoardEepromAddress +
				AMCC_OP_REG_MCSR + 2);

			/* Wait on busy */
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			/* Select the upper address part */
			if (i_Counter == 0)
				b_ReadLowByte = pb_ReadByte[0];
			else
				b_ReadHighByte = pb_ReadByte[0];


			/* Sleep */
			msleep(1);

		}
		w_ReadWord =
			(b_ReadLowByte | (((unsigned short)b_ReadHighByte) *
				256));

		pw_DataRead[i_WordCounter] = w_ReadWord;

		w_EepromStartAddress += 2;	/*  to read the next word */

	}			/*  for (...) i_NbOfWordsToRead */
	return 0;
}

/*+----------------------------------------------------------------------------+*/
/*| Function   Name   : void v_GetAPCI3200EepromCalibrationValue (void)        |*/
/*+----------------------------------------------------------------------------+*/
/*| Task              : Read calibration value from the APCI-3200 eeprom.      |*/
/*+----------------------------------------------------------------------------+*/
/*| Input Parameters  : -                                                      |*/
/*+----------------------------------------------------------------------------+*/
/*| Output Parameters : -                                                      |*/
/*+----------------------------------------------------------------------------+*/
/*| Return Value      : -                                                      |*/
/*+----------------------------------------------------------------------------+*/

void v_GetAPCI3200EepromCalibrationValue(unsigned int dw_PCIBoardEepromAddress,
	struct str_BoardInfos *BoardInformations)
{
	unsigned short w_AnalogInputMainHeaderAddress;
	unsigned short w_AnalogInputComponentAddress;
	unsigned short w_NumberOfModuls = 0;
	unsigned short w_CurrentSources[2];
	unsigned short w_ModulCounter = 0;
	unsigned short w_FirstHeaderSize = 0;
	unsigned short w_NumberOfInputs = 0;
	unsigned short w_CJCFlag = 0;
	unsigned short w_NumberOfGainValue = 0;
	unsigned short w_SingleHeaderAddress = 0;
	unsigned short w_SingleHeaderSize = 0;
	unsigned short w_Input = 0;
	unsigned short w_GainFactorAddress = 0;
	unsigned short w_GainFactorValue[2];
	unsigned short w_GainIndex = 0;
	unsigned short w_GainValue = 0;

  /*****************************************/
  /** Get the Analog input header address **/
  /*****************************************/
	i_AddiHeaderRW_ReadEeprom(1,	/* i_NbOfWordsToRead */
		dw_PCIBoardEepromAddress, 0x116,	/* w_EepromStartAddress: Analog input header address */
		&w_AnalogInputMainHeaderAddress);

  /*******************************************/
  /** Compute the real analog input address **/
  /*******************************************/
	w_AnalogInputMainHeaderAddress = w_AnalogInputMainHeaderAddress + 0x100;

  /******************************/
  /** Get the number of moduls **/
  /******************************/
	i_AddiHeaderRW_ReadEeprom(1,	/* i_NbOfWordsToRead */
		dw_PCIBoardEepromAddress, w_AnalogInputMainHeaderAddress + 0x02,	/* w_EepromStartAddress: Number of conponment */
		&w_NumberOfModuls);

	for (w_ModulCounter = 0; w_ModulCounter < w_NumberOfModuls;
		w_ModulCounter++) {
      /***********************************/
      /** Compute the component address **/
      /***********************************/
		w_AnalogInputComponentAddress =
			w_AnalogInputMainHeaderAddress +
			(w_FirstHeaderSize * w_ModulCounter) + 0x04;

      /****************************/
      /** Read first header size **/
      /****************************/
		i_AddiHeaderRW_ReadEeprom(1,	/* i_NbOfWordsToRead */
			dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress,	/*  Address of the first header */
			&w_FirstHeaderSize);

		w_FirstHeaderSize = w_FirstHeaderSize >> 4;

      /***************************/
      /** Read number of inputs **/
      /***************************/
		i_AddiHeaderRW_ReadEeprom(1,	/* i_NbOfWordsToRead */
			dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress + 0x06,	/*  Number of inputs for the first modul */
			&w_NumberOfInputs);

		w_NumberOfInputs = w_NumberOfInputs >> 4;

      /***********************/
      /** Read the CJC flag **/
      /***********************/
		i_AddiHeaderRW_ReadEeprom(1,	/* i_NbOfWordsToRead */
			dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress + 0x08,	/*  CJC flag */
			&w_CJCFlag);

		w_CJCFlag = (w_CJCFlag >> 3) & 0x1;	/*  Get only the CJC flag */

      /*******************************/
      /** Read number of gain value **/
      /*******************************/
		i_AddiHeaderRW_ReadEeprom(1,	/* i_NbOfWordsToRead */
			dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress + 0x44,	/*  Number of gain value */
			&w_NumberOfGainValue);

		w_NumberOfGainValue = w_NumberOfGainValue & 0xFF;

      /***********************************/
      /** Compute single header address **/
      /***********************************/
		w_SingleHeaderAddress =
			w_AnalogInputComponentAddress + 0x46 +
			(((w_NumberOfGainValue / 16) + 1) * 2) +
			(6 * w_NumberOfGainValue) +
			(4 * (((w_NumberOfGainValue / 16) + 1) * 2));

      /********************************************/
      /** Read current sources value for input 1 **/
      /********************************************/
		i_AddiHeaderRW_ReadEeprom(1,	/* i_NbOfWordsToRead */
			dw_PCIBoardEepromAddress, w_SingleHeaderAddress,	/* w_EepromStartAddress: Single header address */
			&w_SingleHeaderSize);

		w_SingleHeaderSize = w_SingleHeaderSize >> 4;

      /*************************************/
      /** Read gain factor for the module **/
      /*************************************/
		w_GainFactorAddress = w_AnalogInputComponentAddress;

		for (w_GainIndex = 0; w_GainIndex < w_NumberOfGainValue;
			w_GainIndex++) {
	  /************************************/
	  /** Read gain value for the module **/
	  /************************************/
			i_AddiHeaderRW_ReadEeprom(1,	/* i_NbOfWordsToRead */
				dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress + 70 + (2 * (1 + (w_NumberOfGainValue / 16))) + (0x02 * w_GainIndex),	/*  Gain value */
				&w_GainValue);

			BoardInformations->s_Module[w_ModulCounter].
				w_GainValue[w_GainIndex] = w_GainValue;

#             ifdef PRINT_INFO
			printk("\n Gain value = %d",
				BoardInformations->s_Module[w_ModulCounter].
				w_GainValue[w_GainIndex]);
#             endif

	  /*************************************/
	  /** Read gain factor for the module **/
	  /*************************************/
			i_AddiHeaderRW_ReadEeprom(2,	/* i_NbOfWordsToRead */
				dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress + 70 + ((2 * w_NumberOfGainValue) + (2 * (1 + (w_NumberOfGainValue / 16)))) + (0x04 * w_GainIndex),	/*  Gain factor */
				w_GainFactorValue);

			BoardInformations->s_Module[w_ModulCounter].
				ul_GainFactor[w_GainIndex] =
				(w_GainFactorValue[1] << 16) +
				w_GainFactorValue[0];

#             ifdef PRINT_INFO
			printk("\n w_GainFactorValue [%d] = %lu", w_GainIndex,
				BoardInformations->s_Module[w_ModulCounter].
				ul_GainFactor[w_GainIndex]);
#             endif
		}

      /***************************************************************/
      /** Read current source value for each channels of the module **/
      /***************************************************************/
		for (w_Input = 0; w_Input < w_NumberOfInputs; w_Input++) {
	  /********************************************/
	  /** Read current sources value for input 1 **/
	  /********************************************/
			i_AddiHeaderRW_ReadEeprom(2,	/* i_NbOfWordsToRead */
				dw_PCIBoardEepromAddress,
				(w_Input * w_SingleHeaderSize) +
				w_SingleHeaderAddress + 0x0C, w_CurrentSources);

	  /************************************/
	  /** Save the current sources value **/
	  /************************************/
			BoardInformations->s_Module[w_ModulCounter].
				ul_CurrentSource[w_Input] =
				(w_CurrentSources[0] +
				((w_CurrentSources[1] & 0xFFF) << 16));

#             ifdef PRINT_INFO
			printk("\n Current sources [%d] = %lu", w_Input,
				BoardInformations->s_Module[w_ModulCounter].
				ul_CurrentSource[w_Input]);
#             endif
		}

      /***************************************/
      /** Read the CJC current source value **/
      /***************************************/
		i_AddiHeaderRW_ReadEeprom(2,	/* i_NbOfWordsToRead */
			dw_PCIBoardEepromAddress,
			(w_Input * w_SingleHeaderSize) + w_SingleHeaderAddress +
			0x0C, w_CurrentSources);

      /************************************/
      /** Save the current sources value **/
      /************************************/
		BoardInformations->s_Module[w_ModulCounter].
			ul_CurrentSourceCJC =
			(w_CurrentSources[0] +
			((w_CurrentSources[1] & 0xFFF) << 16));

#          ifdef PRINT_INFO
		printk("\n Current sources CJC = %lu",
			BoardInformations->s_Module[w_ModulCounter].
			ul_CurrentSourceCJC);
#          endif
	}
}

int i_APCI3200_GetChannelCalibrationValue(struct comedi_device *dev,
	unsigned int ui_Channel_num, unsigned int *CJCCurrentSource,
	unsigned int *ChannelCurrentSource, unsigned int *ChannelGainFactor)
{
	int i_DiffChannel = 0;
	int i_Module = 0;

#ifdef PRINT_INFO
	printk("\n Channel = %u", ui_Channel_num);
#endif

	/* Test if single or differential mode */
	if (s_BoardInfos[dev->minor].i_ConnectionType == 1) {
		/* if diff */

		if ((ui_Channel_num >= 0) && (ui_Channel_num <= 1))
			i_DiffChannel = ui_Channel_num, i_Module = 0;
		else if ((ui_Channel_num >= 2) && (ui_Channel_num <= 3))
			i_DiffChannel = ui_Channel_num - 2, i_Module = 1;
		else if ((ui_Channel_num >= 4) && (ui_Channel_num <= 5))
			i_DiffChannel = ui_Channel_num - 4, i_Module = 2;
		else if ((ui_Channel_num >= 6) && (ui_Channel_num <= 7))
			i_DiffChannel = ui_Channel_num - 6, i_Module = 3;

	} else {
		/*  if single */
		if ((ui_Channel_num == 0) || (ui_Channel_num == 1))
			i_DiffChannel = 0, i_Module = 0;
		else if ((ui_Channel_num == 2) || (ui_Channel_num == 3))
			i_DiffChannel = 1, i_Module = 0;
		else if ((ui_Channel_num == 4) || (ui_Channel_num == 5))
			i_DiffChannel = 0, i_Module = 1;
		else if ((ui_Channel_num == 6) || (ui_Channel_num == 7))
			i_DiffChannel = 1, i_Module = 1;
		else if ((ui_Channel_num == 8) || (ui_Channel_num == 9))
			i_DiffChannel = 0, i_Module = 2;
		else if ((ui_Channel_num == 10) || (ui_Channel_num == 11))
			i_DiffChannel = 1, i_Module = 2;
		else if ((ui_Channel_num == 12) || (ui_Channel_num == 13))
			i_DiffChannel = 0, i_Module = 3;
		else if ((ui_Channel_num == 14) || (ui_Channel_num == 15))
			i_DiffChannel = 1, i_Module = 3;
	}

	/* Test if thermocouple or RTD mode */
	*CJCCurrentSource =
		s_BoardInfos[dev->minor].s_Module[i_Module].ul_CurrentSourceCJC;
#ifdef PRINT_INFO
	printk("\n CJCCurrentSource = %lu", *CJCCurrentSource);
#endif

	*ChannelCurrentSource =
		s_BoardInfos[dev->minor].s_Module[i_Module].
		ul_CurrentSource[i_DiffChannel];
#ifdef PRINT_INFO
	printk("\n ChannelCurrentSource = %lu", *ChannelCurrentSource);
#endif
	/*       } */
	/*    } */

	/* Channle gain factor */
	*ChannelGainFactor =
		s_BoardInfos[dev->minor].s_Module[i_Module].
		ul_GainFactor[s_BoardInfos[dev->minor].i_ADDIDATAGain];
#ifdef PRINT_INFO
	printk("\n ChannelGainFactor = %lu", *ChannelGainFactor);
#endif
	/* End JK 21.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */

	return 0;
}

/* End JK 21.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */

/*
  +----------------------------------------------------------------------------+
  | Function   Name   : int i_APCI3200_ReadDigitalInput                       |
  |			  (struct comedi_device *dev,struct comedi_subdevice *s,               |
  |                      struct comedi_insn *insn,unsigned int *data)                     |
  +----------------------------------------------------------------------------+
  | Task              : Read  value  of the selected channel or port           |
  +----------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev      : Driver handle                |
  |                     unsigned int ui_NoOfChannels    : No Of Channels To read  for Port
  Channel Numberfor single channel
  |                     unsigned int data[0]            : 0: Read single channel
  1: Read port value
  data[1]              Port number
  +----------------------------------------------------------------------------+
  | Output Parameters :	--	data[0] :Read status value
  +----------------------------------------------------------------------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur. Return the error          |
  |			                                                         |
  +----------------------------------------------------------------------------+
*/

int i_APCI3200_ReadDigitalInput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_Temp = 0;
	unsigned int ui_NoOfChannel = 0;
	ui_NoOfChannel = CR_CHAN(insn->chanspec);
	ui_Temp = data[0];
	*data = inl(devpriv->i_IobaseReserved);

	if (ui_Temp == 0) {
		*data = (*data >> ui_NoOfChannel) & 0x1;
	}			/* if  (ui_Temp==0) */
	else {
		if (ui_Temp == 1) {
			if (data[1] < 0 || data[1] > 1) {
				printk("\nThe port number is in error\n");
				return -EINVAL;
			}	/* if(data[1] < 0 || data[1] >1) */
			switch (ui_NoOfChannel) {

			case 2:
				*data = (*data >> (2 * data[1])) & 0x3;
				break;
			case 3:
				*data = (*data & 15);
				break;
			default:
				comedi_error(dev, " chan spec wrong");
				return -EINVAL;	/*  "sorry channel spec wrong " */

			}	/* switch(ui_NoOfChannels) */
		}		/* if  (ui_Temp==1) */
		else {
			printk("\nSpecified channel not supported \n");
		}		/* elseif  (ui_Temp==1) */
	}
	return insn->n;
}

/*
  +----------------------------------------------------------------------------+
  | Function   Name   : int i_APCI3200_ConfigDigitalOutput                     |
  |			  (struct comedi_device *dev,struct comedi_subdevice *s,				 |
  |                      struct comedi_insn *insn,unsigned int *data)                     |
  +----------------------------------------------------------------------------+
  | Task              : Configures The Digital Output Subdevice.               |
  +----------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev : Driver handle                     |
  |			  data[0]  :1  Memory enable
  0  Memory Disable
  +----------------------------------------------------------------------------+
  | Output Parameters :	--													 |
  +----------------------------------------------------------------------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur. Return the error			 |
  |																	 |
  +----------------------------------------------------------------------------+
*/
int i_APCI3200_ConfigDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{

	if ((data[0] != 0) && (data[0] != 1)) {
		comedi_error(dev,
			"Not a valid Data !!! ,Data should be 1 or 0\n");
		return -EINVAL;
	}			/* if  ( (data[0]!=0) && (data[0]!=1) ) */
	if (data[0]) {
		devpriv->b_OutputMemoryStatus = ADDIDATA_ENABLE;
	}			/*  if  (data[0]) */
	else {
		devpriv->b_OutputMemoryStatus = ADDIDATA_DISABLE;
	}			/* else if  (data[0]) */
	return insn->n;
}

/*
  +----------------------------------------------------------------------------+
  | Function   Name   : int i_APCI3200_WriteDigitalOutput                      |
  |			  (struct comedi_device *dev,struct comedi_subdevice *s,				 |
  |                      struct comedi_insn *insn,unsigned int *data)                     |
  +----------------------------------------------------------------------------+
  | Task              : writes To the digital Output Subdevice                 |
  +----------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev      : Driver handle                |
  |                     struct comedi_subdevice *s     : Subdevice Pointer            |
  |                     struct comedi_insn *insn       : Insn Structure Pointer       |
  |                     unsigned int *data          : Data Pointer contains        |
  |                                          configuration parameters as below |
  |                     data[0]             :Value to output
  data[1]             : 0 o/p single channel
  1 o/p port
  data[2]             : port no
  data[3]             :0 set the digital o/p on
  1 set the digital o/p off
  +----------------------------------------------------------------------------+
  | Output Parameters :	--													 |
  +----------------------------------------------------------------------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur. Return the error	     	 |
  |			                                                         |
  +----------------------------------------------------------------------------+
*/
int i_APCI3200_WriteDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_Temp = 0, ui_Temp1 = 0;
	unsigned int ui_NoOfChannel = CR_CHAN(insn->chanspec);	/*  get the channel */
	if (devpriv->b_OutputMemoryStatus) {
		ui_Temp = inl(devpriv->i_IobaseAddon);

	}			/* if(devpriv->b_OutputMemoryStatus ) */
	else {
		ui_Temp = 0;
	}			/* if(devpriv->b_OutputMemoryStatus ) */
	if (data[3] == 0) {
		if (data[1] == 0) {
			data[0] = (data[0] << ui_NoOfChannel) | ui_Temp;
			outl(data[0], devpriv->i_IobaseAddon);
		}		/* if(data[1]==0) */
		else {
			if (data[1] == 1) {
				switch (ui_NoOfChannel) {

				case 2:
					data[0] =
						(data[0] << (2 *
							data[2])) | ui_Temp;
					break;
				case 3:
					data[0] = (data[0] | ui_Temp);
					break;
				}	/* switch(ui_NoOfChannels) */

				outl(data[0], devpriv->i_IobaseAddon);
			}	/*  if(data[1]==1) */
			else {
				printk("\nSpecified channel not supported\n");
			}	/* else if(data[1]==1) */
		}		/* elseif(data[1]==0) */
	}			/* if(data[3]==0) */
	else {
		if (data[3] == 1) {
			if (data[1] == 0) {
				data[0] = ~data[0] & 0x1;
				ui_Temp1 = 1;
				ui_Temp1 = ui_Temp1 << ui_NoOfChannel;
				ui_Temp = ui_Temp | ui_Temp1;
				data[0] = (data[0] << ui_NoOfChannel) ^ 0xf;
				data[0] = data[0] & ui_Temp;
				outl(data[0], devpriv->i_IobaseAddon);
			}	/* if(data[1]==0) */
			else {
				if (data[1] == 1) {
					switch (ui_NoOfChannel) {

					case 2:
						data[0] = ~data[0] & 0x3;
						ui_Temp1 = 3;
						ui_Temp1 =
							ui_Temp1 << 2 * data[2];
						ui_Temp = ui_Temp | ui_Temp1;
						data[0] =
							((data[0] << (2 *
									data
									[2])) ^
							0xf) & ui_Temp;

						break;
					case 3:
						break;

					default:
						comedi_error(dev,
							" chan spec wrong");
						return -EINVAL;	/*  "sorry channel spec wrong " */
					}	/* switch(ui_NoOfChannels) */

					outl(data[0], devpriv->i_IobaseAddon);
				}	/*  if(data[1]==1) */
				else {
					printk("\nSpecified channel not supported\n");
				}	/* else if(data[1]==1) */
			}	/* elseif(data[1]==0) */
		}		/* if(data[3]==1); */
		else {
			printk("\nSpecified functionality does not exist\n");
			return -EINVAL;
		}		/* if else data[3]==1) */
	}			/* if else data[3]==0) */
	return insn->n;
}

/*
  +----------------------------------------------------------------------------+
  | Function   Name   : int i_APCI3200_ReadDigitalOutput                       |
  |			  (struct comedi_device *dev,struct comedi_subdevice *s,               |
  |                      struct comedi_insn *insn,unsigned int *data)                     |
  +----------------------------------------------------------------------------+
  | Task              : Read  value  of the selected channel or port           |
  +----------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev      : Driver handle                |
  |                     unsigned int ui_NoOfChannels    : No Of Channels To read       |
  |                     unsigned int *data              : Data Pointer to read status  |
  data[0]                 :0 read single channel
  1 read port value
  data[1]                  port no

  +----------------------------------------------------------------------------+
  | Output Parameters :	--													 |
  +----------------------------------------------------------------------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur. Return the error          |
  |			                                                         |
  +----------------------------------------------------------------------------+
*/
int i_APCI3200_ReadDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_Temp;
	unsigned int ui_NoOfChannel;
	ui_NoOfChannel = CR_CHAN(insn->chanspec);
	ui_Temp = data[0];
	*data = inl(devpriv->i_IobaseAddon);
	if (ui_Temp == 0) {
		*data = (*data >> ui_NoOfChannel) & 0x1;
	}			/*  if  (ui_Temp==0) */
	else {
		if (ui_Temp == 1) {
			if (data[1] < 0 || data[1] > 1) {
				printk("\nThe port selection is in error\n");
				return -EINVAL;
			}	/* if(data[1] <0 ||data[1] >1) */
			switch (ui_NoOfChannel) {
			case 2:
				*data = (*data >> (2 * data[1])) & 3;
				break;

			case 3:
				break;

			default:
				comedi_error(dev, " chan spec wrong");
				return -EINVAL;	/*  "sorry channel spec wrong " */
				break;
			}	/*  switch(ui_NoOfChannels) */
		}		/*  if  (ui_Temp==1) */
		else {
			printk("\nSpecified channel not supported \n");
		}		/*  else if (ui_Temp==1) */
	}			/*  else if  (ui_Temp==0) */
	return insn->n;
}

/*
  +----------------------------------------------------------------------------+
  | Function   Name   : int i_APCI3200_ConfigAnalogInput                       |
  |			  (struct comedi_device *dev,struct comedi_subdevice *s,               |
  |                      struct comedi_insn *insn,unsigned int *data)                     |
  +----------------------------------------------------------------------------+
  | Task              : Configures The Analog Input Subdevice                  |
  +----------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev      : Driver handle                |
  |                     struct comedi_subdevice *s     : Subdevice Pointer            |
  |                     struct comedi_insn *insn       : Insn Structure Pointer       |
  |                     unsigned int *data          : Data Pointer contains        |
  |                                          configuration parameters as below |
  |                                                                            |
  |					data[0]
  |                                               0:Normal AI                  |
  |                                               1:RTD                        |
  |                                               2:THERMOCOUPLE               |
  |				    data[1]            : Gain To Use                 |
  |                                                                            |
  |                           data[2]            : Polarity
  |                                                0:Bipolar                   |
  |                                                1:Unipolar                  |
  |															    	 |
  |                           data[3]            : Offset Range
  |                                                                            |
  |                           data[4]            : Coupling
  |                                                0:DC Coupling               |
  |                                                1:AC Coupling               |
  |                                                                            |
  |                           data[5]            :Differential/Single
  |                                                0:Single                    |
  |                                                1:Differential              |
  |                                                                            |
  |                           data[6]            :TimerReloadValue
  |                                                                            |
  |                           data[7]            :ConvertingTimeUnit
  |                                                                            |
  |                           data[8]             :0 Analog voltage measurement
  1 Resistance measurement
  2 Temperature measurement
  |                           data[9]            :Interrupt
  |                                              0:Disable
  |                                              1:Enable
  data[10]           :Type of Thermocouple
  |                          data[11]           : 0: single channel
  Module Number
  |
  |                          data[12]
  |                                             0:Single Read
  |                                             1:Read more channel
  2:Single scan
  |                                             3:Continous Scan
  data[13]          :Number of channels to read
  |                          data[14]          :RTD connection type
  :0:RTD not used
  1:RTD 2 wire connection
  2:RTD 3 wire connection
  3:RTD 4 wire connection
  |                                                                            |
  |                                                                            |
  |                                                                            |
  +----------------------------------------------------------------------------+
  | Output Parameters :	--													 |
  +----------------------------------------------------------------------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur. Return the error          |
  |			                                                         |
  +----------------------------------------------------------------------------+
*/
int i_APCI3200_ConfigAnalogInput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{

	unsigned int ul_Config = 0, ul_Temp = 0;
	unsigned int ui_ChannelNo = 0;
	unsigned int ui_Dummy = 0;
	int i_err = 0;

	/* Begin JK 21.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */

#ifdef PRINT_INFO
	int i = 0, i2 = 0;
#endif
	/* End JK 21.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */

	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/*  Initialize the structure */
	if (s_BoardInfos[dev->minor].b_StructInitialized != 1) {
		s_BoardInfos[dev->minor].i_CJCAvailable = 1;
		s_BoardInfos[dev->minor].i_CJCPolarity = 0;
		s_BoardInfos[dev->minor].i_CJCGain = 2;	/* changed from 0 to 2 */
		s_BoardInfos[dev->minor].i_InterruptFlag = 0;
		s_BoardInfos[dev->minor].i_AutoCalibration = 0;	/* : auto calibration */
		s_BoardInfos[dev->minor].i_ChannelCount = 0;
		s_BoardInfos[dev->minor].i_Sum = 0;
		s_BoardInfos[dev->minor].ui_Channel_num = 0;
		s_BoardInfos[dev->minor].i_Count = 0;
		s_BoardInfos[dev->minor].i_Initialised = 0;
		s_BoardInfos[dev->minor].b_StructInitialized = 1;

		/* Begin JK 21.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */
		s_BoardInfos[dev->minor].i_ConnectionType = 0;
		/* End JK 21.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */

		/* Begin JK 21.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */
		memset(s_BoardInfos[dev->minor].s_Module, 0,
			sizeof(s_BoardInfos[dev->minor].s_Module[MAX_MODULE]));

		v_GetAPCI3200EepromCalibrationValue(devpriv->i_IobaseAmcc,
			&s_BoardInfos[dev->minor]);

#ifdef PRINT_INFO
		for (i = 0; i < MAX_MODULE; i++) {
			printk("\n s_Module[%i].ul_CurrentSourceCJC = %lu", i,
				s_BoardInfos[dev->minor].s_Module[i].
				ul_CurrentSourceCJC);

			for (i2 = 0; i2 < 5; i2++) {
				printk("\n s_Module[%i].ul_CurrentSource [%i] = %lu", i, i2, s_BoardInfos[dev->minor].s_Module[i].ul_CurrentSource[i2]);
			}

			for (i2 = 0; i2 < 8; i2++) {
				printk("\n s_Module[%i].ul_GainFactor [%i] = %lu", i, i2, s_BoardInfos[dev->minor].s_Module[i].ul_GainFactor[i2]);
			}

			for (i2 = 0; i2 < 8; i2++) {
				printk("\n s_Module[%i].w_GainValue [%i] = %u",
					i, i2,
					s_BoardInfos[dev->minor].s_Module[i].
					w_GainValue[i2]);
			}
		}
#endif
		/* End JK 21.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */
	}

	if (data[0] != 0 && data[0] != 1 && data[0] != 2) {
		printk("\nThe selection of acquisition type is in error\n");
		i_err++;
	}			/* if(data[0]!=0 && data[0]!=1 && data[0]!=2) */
	if (data[0] == 1) {
		if (data[14] != 0 && data[14] != 1 && data[14] != 2
			&& data[14] != 4) {
			printk("\n Error in selection of RTD connection type\n");
			i_err++;
		}		/* if(data[14]!=0 && data[14]!=1 && data[14]!=2 && data[14]!=4) */
	}			/* if(data[0]==1 ) */
	if (data[1] < 0 || data[1] > 7) {
		printk("\nThe selection of gain is in error\n");
		i_err++;
	}			/*  if(data[1]<0 || data[1]>7) */
	if (data[2] != 0 && data[2] != 1) {
		printk("\nThe selection of polarity is in error\n");
		i_err++;
	}			/* if(data[2]!=0 &&  data[2]!=1) */
	if (data[3] != 0) {
		printk("\nThe selection of offset range  is in error\n");
		i_err++;
	}			/*  if(data[3]!=0) */
	if (data[4] != 0 && data[4] != 1) {
		printk("\nThe selection of coupling is in error\n");
		i_err++;
	}			/* if(data[4]!=0 &&  data[4]!=1) */
	if (data[5] != 0 && data[5] != 1) {
		printk("\nThe selection of single/differential mode is in error\n");
		i_err++;
	}			/* if(data[5]!=0 &&  data[5]!=1) */
	if (data[8] != 0 && data[8] != 1 && data[2] != 2) {
		printk("\nError in selection of functionality\n");
	}			/* if(data[8]!=0 && data[8]!=1 && data[2]!=2) */
	if (data[12] == 0 || data[12] == 1) {
		if (data[6] != 20 && data[6] != 40 && data[6] != 80
			&& data[6] != 160) {
			printk("\nThe selection of conversion time reload value is in error\n");
			i_err++;
		}		/*  if (data[6]!=20 && data[6]!=40 && data[6]!=80 && data[6]!=160 ) */
		if (data[7] != 2) {
			printk("\nThe selection of conversion time unit  is in error\n");
			i_err++;
		}		/*  if(data[7]!=2) */
	}
	if (data[9] != 0 && data[9] != 1) {
		printk("\nThe selection of interrupt enable is in error\n");
		i_err++;
	}			/* if(data[9]!=0 &&  data[9]!=1) */
	if (data[11] < 0 || data[11] > 4) {
		printk("\nThe selection of module is in error\n");
		i_err++;
	}			/* if(data[11] <0 ||  data[11]>1) */
	if (data[12] < 0 || data[12] > 3) {
		printk("\nThe selection of singlechannel/scan selection is in error\n");
		i_err++;
	}			/* if(data[12] < 0 ||  data[12]> 3) */
	if (data[13] < 0 || data[13] > 16) {
		printk("\nThe selection of number of channels is in error\n");
		i_err++;
	}			/*  if(data[13] <0 ||data[13] >15) */

	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/*
	   i_ChannelCount=data[13];
	   i_ScanType=data[12];
	   i_ADDIDATAPolarity = data[2];
	   i_ADDIDATAGain=data[1];
	   i_ADDIDATAConversionTime=data[6];
	   i_ADDIDATAConversionTimeUnit=data[7];
	   i_ADDIDATAType=data[0];
	 */

	/*  Save acquisition configuration for the actual board */
	s_BoardInfos[dev->minor].i_ChannelCount = data[13];
	s_BoardInfos[dev->minor].i_ScanType = data[12];
	s_BoardInfos[dev->minor].i_ADDIDATAPolarity = data[2];
	s_BoardInfos[dev->minor].i_ADDIDATAGain = data[1];
	s_BoardInfos[dev->minor].i_ADDIDATAConversionTime = data[6];
	s_BoardInfos[dev->minor].i_ADDIDATAConversionTimeUnit = data[7];
	s_BoardInfos[dev->minor].i_ADDIDATAType = data[0];
	/* Begin JK 19.10.2004: APCI-3200 Driver update 0.7.57 -> 0.7.68 */
	s_BoardInfos[dev->minor].i_ConnectionType = data[5];
	/* End JK 19.10.2004: APCI-3200 Driver update 0.7.57 -> 0.7.68 */
	/* END JK 06.07.04: Management of sevrals boards */

	/* Begin JK 19.10.2004: APCI-3200 Driver update 0.7.57 -> 0.7.68 */
	memset(s_BoardInfos[dev->minor].ui_ScanValueArray, 0, (7 + 12) * sizeof(unsigned int));	/*  7 is the maximal number of channels */
	/* End JK 19.10.2004: APCI-3200 Driver update 0.7.57 -> 0.7.68 */

	/* BEGIN JK 02.07.04 : This while can't be do, it block the process when using severals boards */
	/* while(i_InterruptFlag==1) */
	while (s_BoardInfos[dev->minor].i_InterruptFlag == 1) {
#ifndef MSXBOX
		udelay(1);
#else
		/*  In the case where the driver is compiled for the MSX-Box */
		/*  we used a printk to have a little delay because udelay */
		/*  seems to be broken under the MSX-Box. */
		/*  This solution hat to be studied. */
		printk("");
#endif
	}
	/* END JK 02.07.04 : This while can't be do, it block the process when using severals boards */

	ui_ChannelNo = CR_CHAN(insn->chanspec);	/*  get the channel */
	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* i_ChannelNo=ui_ChannelNo; */
	/* ui_Channel_num =ui_ChannelNo; */

	s_BoardInfos[dev->minor].i_ChannelNo = ui_ChannelNo;
	s_BoardInfos[dev->minor].ui_Channel_num = ui_ChannelNo;

	/* END JK 06.07.04: Management of sevrals boards */

	if (data[5] == 0) {
		if (ui_ChannelNo < 0 || ui_ChannelNo > 15) {
			printk("\nThe Selection of the channel is in error\n");
			i_err++;
		}		/*  if(ui_ChannelNo<0 || ui_ChannelNo>15) */
	}			/* if(data[5]==0) */
	else {
		if (data[14] == 2) {
			if (ui_ChannelNo < 0 || ui_ChannelNo > 3) {
				printk("\nThe Selection of the channel is in error\n");
				i_err++;
			}	/*  if(ui_ChannelNo<0 || ui_ChannelNo>3) */
		}		/* if(data[14]==2) */
		else {
			if (ui_ChannelNo < 0 || ui_ChannelNo > 7) {
				printk("\nThe Selection of the channel is in error\n");
				i_err++;
			}	/*  if(ui_ChannelNo<0 || ui_ChannelNo>7) */
		}		/* elseif(data[14]==2) */
	}			/* elseif(data[5]==0) */
	if (data[12] == 0 || data[12] == 1) {
		switch (data[5]) {
		case 0:
			if (ui_ChannelNo >= 0 && ui_ChannelNo <= 3) {
				/* BEGIN JK 06.07.04: Management of sevrals boards */
				/* i_Offset=0; */
				s_BoardInfos[dev->minor].i_Offset = 0;
				/* END JK 06.07.04: Management of sevrals boards */
			}	/* if(ui_ChannelNo >=0 && ui_ChannelNo <=3) */
			if (ui_ChannelNo >= 4 && ui_ChannelNo <= 7) {
				/* BEGIN JK 06.07.04: Management of sevrals boards */
				/* i_Offset=64; */
				s_BoardInfos[dev->minor].i_Offset = 64;
				/* END JK 06.07.04: Management of sevrals boards */
			}	/* if(ui_ChannelNo >=4 && ui_ChannelNo <=7) */
			if (ui_ChannelNo >= 8 && ui_ChannelNo <= 11) {
				/* BEGIN JK 06.07.04: Management of sevrals boards */
				/* i_Offset=128; */
				s_BoardInfos[dev->minor].i_Offset = 128;
				/* END JK 06.07.04: Management of sevrals boards */
			}	/* if(ui_ChannelNo >=8 && ui_ChannelNo <=11) */
			if (ui_ChannelNo >= 12 && ui_ChannelNo <= 15) {
				/* BEGIN JK 06.07.04: Management of sevrals boards */
				/* i_Offset=192; */
				s_BoardInfos[dev->minor].i_Offset = 192;
				/* END JK 06.07.04: Management of sevrals boards */
			}	/* if(ui_ChannelNo >=12 && ui_ChannelNo <=15) */
			break;
		case 1:
			if (data[14] == 2) {
				if (ui_ChannelNo == 0) {
					/* BEGIN JK 06.07.04: Management of sevrals boards */
					/* i_Offset=0; */
					s_BoardInfos[dev->minor].i_Offset = 0;
					/* END JK 06.07.04: Management of sevrals boards */
				}	/* if(ui_ChannelNo ==0 ) */
				if (ui_ChannelNo == 1) {
					/* BEGIN JK 06.07.04: Management of sevrals boards */
					/* i_Offset=0; */
					s_BoardInfos[dev->minor].i_Offset = 64;
					/* END JK 06.07.04: Management of sevrals boards */
				}	/*  if(ui_ChannelNo ==1) */
				if (ui_ChannelNo == 2) {
					/* BEGIN JK 06.07.04: Management of sevrals boards */
					/* i_Offset=128; */
					s_BoardInfos[dev->minor].i_Offset = 128;
					/* END JK 06.07.04: Management of sevrals boards */
				}	/* if(ui_ChannelNo ==2 ) */
				if (ui_ChannelNo == 3) {
					/* BEGIN JK 06.07.04: Management of sevrals boards */
					/* i_Offset=192; */
					s_BoardInfos[dev->minor].i_Offset = 192;
					/* END JK 06.07.04: Management of sevrals boards */
				}	/* if(ui_ChannelNo ==3) */

				/* BEGIN JK 06.07.04: Management of sevrals boards */
				/* i_ChannelNo=0; */
				s_BoardInfos[dev->minor].i_ChannelNo = 0;
				/* END JK 06.07.04: Management of sevrals boards */
				ui_ChannelNo = 0;
				break;
			}	/* if(data[14]==2) */
			if (ui_ChannelNo >= 0 && ui_ChannelNo <= 1) {
				/* BEGIN JK 06.07.04: Management of sevrals boards */
				/* i_Offset=0; */
				s_BoardInfos[dev->minor].i_Offset = 0;
				/* END JK 06.07.04: Management of sevrals boards */
			}	/* if(ui_ChannelNo >=0 && ui_ChannelNo <=1) */
			if (ui_ChannelNo >= 2 && ui_ChannelNo <= 3) {
				/* BEGIN JK 06.07.04: Management of sevrals boards */
				/* i_ChannelNo=i_ChannelNo-2; */
				/* i_Offset=64; */
				s_BoardInfos[dev->minor].i_ChannelNo =
					s_BoardInfos[dev->minor].i_ChannelNo -
					2;
				s_BoardInfos[dev->minor].i_Offset = 64;
				/* END JK 06.07.04: Management of sevrals boards */
				ui_ChannelNo = ui_ChannelNo - 2;
			}	/* if(ui_ChannelNo >=2 && ui_ChannelNo <=3) */
			if (ui_ChannelNo >= 4 && ui_ChannelNo <= 5) {
				/* BEGIN JK 06.07.04: Management of sevrals boards */
				/* i_ChannelNo=i_ChannelNo-4; */
				/* i_Offset=128; */
				s_BoardInfos[dev->minor].i_ChannelNo =
					s_BoardInfos[dev->minor].i_ChannelNo -
					4;
				s_BoardInfos[dev->minor].i_Offset = 128;
				/* END JK 06.07.04: Management of sevrals boards */
				ui_ChannelNo = ui_ChannelNo - 4;
			}	/* if(ui_ChannelNo >=4 && ui_ChannelNo <=5) */
			if (ui_ChannelNo >= 6 && ui_ChannelNo <= 7) {
				/* BEGIN JK 06.07.04: Management of sevrals boards */
				/* i_ChannelNo=i_ChannelNo-6; */
				/* i_Offset=192; */
				s_BoardInfos[dev->minor].i_ChannelNo =
					s_BoardInfos[dev->minor].i_ChannelNo -
					6;
				s_BoardInfos[dev->minor].i_Offset = 192;
				/* END JK 06.07.04: Management of sevrals boards */
				ui_ChannelNo = ui_ChannelNo - 6;
			}	/* if(ui_ChannelNo >=6 && ui_ChannelNo <=7) */
			break;

		default:
			printk("\n This selection of polarity does not exist\n");
			i_err++;
		}		/* switch(data[2]) */
	}			/* if(data[12]==0 || data[12]==1) */
	else {
		switch (data[11]) {
		case 1:
			/* BEGIN JK 06.07.04: Management of sevrals boards */
			/* i_Offset=0; */
			s_BoardInfos[dev->minor].i_Offset = 0;
			/* END JK 06.07.04: Management of sevrals boards */
			break;
		case 2:
			/* BEGIN JK 06.07.04: Management of sevrals boards */
			/* i_Offset=64; */
			s_BoardInfos[dev->minor].i_Offset = 64;
			/* END JK 06.07.04: Management of sevrals boards */
			break;
		case 3:
			/* BEGIN JK 06.07.04: Management of sevrals boards */
			/* i_Offset=128; */
			s_BoardInfos[dev->minor].i_Offset = 128;
			/* END JK 06.07.04: Management of sevrals boards */
			break;
		case 4:
			/* BEGIN JK 06.07.04: Management of sevrals boards */
			/* i_Offset=192; */
			s_BoardInfos[dev->minor].i_Offset = 192;
			/* END JK 06.07.04: Management of sevrals boards */
			break;
		default:
			printk("\nError in module selection\n");
			i_err++;
		}		/*  switch(data[11]) */
	}			/*  elseif(data[12]==0 || data[12]==1) */
	if (i_err) {
		i_APCI3200_Reset(dev);
		return -EINVAL;
	}
	/* if(i_ScanType!=1) */
	if (s_BoardInfos[dev->minor].i_ScanType != 1) {
		/* BEGIN JK 06.07.04: Management of sevrals boards */
		/* i_Count=0; */
		/* i_Sum=0; */
		s_BoardInfos[dev->minor].i_Count = 0;
		s_BoardInfos[dev->minor].i_Sum = 0;
		/* END JK 06.07.04: Management of sevrals boards */
	}			/* if(i_ScanType!=1) */

	ul_Config =
		data[1] | (data[2] << 6) | (data[5] << 7) | (data[3] << 8) |
		(data[4] << 9);
	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* END JK 06.07.04: Management of sevrals boards */
  /*********************************/
	/* Write the channel to configure */
  /*********************************/
	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* outl(0 | ui_ChannelNo , devpriv->iobase+i_Offset + 0x4); */
	outl(0 | ui_ChannelNo,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 0x4);
	/* END JK 06.07.04: Management of sevrals boards */

	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* END JK 06.07.04: Management of sevrals boards */
  /**************************/
	/* Reset the configuration */
  /**************************/
	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* outl(0 , devpriv->iobase+i_Offset + 0x0); */
	outl(0, devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 0x0);
	/* END JK 06.07.04: Management of sevrals boards */

	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* END JK 06.07.04: Management of sevrals boards */

  /***************************/
	/* Write the configuration */
  /***************************/
	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* outl(ul_Config , devpriv->iobase+i_Offset + 0x0); */
	outl(ul_Config,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 0x0);
	/* END JK 06.07.04: Management of sevrals boards */

  /***************************/
	/*Reset the calibration bit */
  /***************************/
	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* ul_Temp = inl(devpriv->iobase+i_Offset + 12); */
	ul_Temp = inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);
	/* END JK 06.07.04: Management of sevrals boards */

	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* END JK 06.07.04: Management of sevrals boards */

	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* outl((ul_Temp & 0xFFF9FFFF) , devpriv->iobase+.i_Offset + 12); */
	outl((ul_Temp & 0xFFF9FFFF),
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);
	/* END JK 06.07.04: Management of sevrals boards */

	if (data[9] == 1) {
		devpriv->tsk_Current = current;
		/* BEGIN JK 06.07.04: Management of sevrals boards */
		/* i_InterruptFlag=1; */
		s_BoardInfos[dev->minor].i_InterruptFlag = 1;
		/* END JK 06.07.04: Management of sevrals boards */
	}			/*  if(data[9]==1) */
	else {
		/* BEGIN JK 06.07.04: Management of sevrals boards */
		/* i_InterruptFlag=0; */
		s_BoardInfos[dev->minor].i_InterruptFlag = 0;
		/* END JK 06.07.04: Management of sevrals boards */
	}			/* else  if(data[9]==1) */

	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* i_Initialised=1; */
	s_BoardInfos[dev->minor].i_Initialised = 1;
	/* END JK 06.07.04: Management of sevrals boards */

	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* if(i_ScanType==1) */
	if (s_BoardInfos[dev->minor].i_ScanType == 1)
		/* END JK 06.07.04: Management of sevrals boards */
	{
		/* BEGIN JK 06.07.04: Management of sevrals boards */
		/* i_Sum=i_Sum+1; */
		s_BoardInfos[dev->minor].i_Sum =
			s_BoardInfos[dev->minor].i_Sum + 1;
		/* END JK 06.07.04: Management of sevrals boards */

		insn->unused[0] = 0;
		i_APCI3200_ReadAnalogInput(dev, s, insn, &ui_Dummy);
	}

	return insn->n;
}

/*
  +----------------------------------------------------------------------------+
  | Function   Name   : int i_APCI3200_ReadAnalogInput                         |
  |			          (struct comedi_device *dev,struct comedi_subdevice *s,       |
  |                     struct comedi_insn *insn,unsigned int *data)                      |
  +----------------------------------------------------------------------------+
  | Task              : Read  value  of the selected channel			         |
  +----------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev      : Driver handle                |
  |                     unsigned int ui_NoOfChannels    : No Of Channels To read       |
  |                     unsigned int *data              : Data Pointer to read status  |
  +----------------------------------------------------------------------------+
  | Output Parameters :	--													 |
  |				data[0]  : Digital Value Of Input             |
  |				data[1]  : Calibration Offset Value           |
  |				data[2]  : Calibration Gain Value
  |				data[3]  : CJC value
  |				data[4]  : CJC offset value
  |				data[5]  : CJC gain value
  | Begin JK 21.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values
  |				data[6] : CJC current source from eeprom
  |				data[7] : Channel current source from eeprom
  |				data[8] : Channle gain factor from eeprom
  | End JK 21.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values
  +----------------------------------------------------------------------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur. Return the error          |
  |			                                                         |
  +----------------------------------------------------------------------------+
*/
int i_APCI3200_ReadAnalogInput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_DummyValue = 0;
	int i_ConvertCJCCalibration;
	int i = 0;

	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* if(i_Initialised==0) */
	if (s_BoardInfos[dev->minor].i_Initialised == 0)
		/* END JK 06.07.04: Management of sevrals boards */
	{
		i_APCI3200_Reset(dev);
		return -EINVAL;
	}			/* if(i_Initialised==0); */

#ifdef PRINT_INFO
	printk("\n insn->unused[0] = %i", insn->unused[0]);
#endif

	switch (insn->unused[0]) {
	case 0:

		i_APCI3200_Read1AnalogInputChannel(dev, s, insn,
			&ui_DummyValue);
		/* BEGIN JK 06.07.04: Management of sevrals boards */
		/* ui_InterruptChannelValue[i_Count+0]=ui_DummyValue; */
		s_BoardInfos[dev->minor].
			ui_InterruptChannelValue[s_BoardInfos[dev->minor].
			i_Count + 0] = ui_DummyValue;
		/* END JK 06.07.04: Management of sevrals boards */

		/* Begin JK 25.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */
		i_APCI3200_GetChannelCalibrationValue(dev,
			s_BoardInfos[dev->minor].ui_Channel_num,
			&s_BoardInfos[dev->minor].
			ui_InterruptChannelValue[s_BoardInfos[dev->minor].
				i_Count + 6],
			&s_BoardInfos[dev->minor].
			ui_InterruptChannelValue[s_BoardInfos[dev->minor].
				i_Count + 7],
			&s_BoardInfos[dev->minor].
			ui_InterruptChannelValue[s_BoardInfos[dev->minor].
				i_Count + 8]);

#ifdef PRINT_INFO
		printk("\n s_BoardInfos [dev->minor].ui_InterruptChannelValue[s_BoardInfos [dev->minor].i_Count+6] = %lu", s_BoardInfos[dev->minor].ui_InterruptChannelValue[s_BoardInfos[dev->minor].i_Count + 6]);

		printk("\n s_BoardInfos [dev->minor].ui_InterruptChannelValue[s_BoardInfos [dev->minor].i_Count+7] = %lu", s_BoardInfos[dev->minor].ui_InterruptChannelValue[s_BoardInfos[dev->minor].i_Count + 7]);

		printk("\n s_BoardInfos [dev->minor].ui_InterruptChannelValue[s_BoardInfos [dev->minor].i_Count+8] = %lu", s_BoardInfos[dev->minor].ui_InterruptChannelValue[s_BoardInfos[dev->minor].i_Count + 8]);
#endif

		/* End JK 25.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */

		/* BEGIN JK 06.07.04: Management of sevrals boards */
		/* if((i_ADDIDATAType==2) && (i_InterruptFlag == FALSE) && (i_CJCAvailable==1)) */
		if ((s_BoardInfos[dev->minor].i_ADDIDATAType == 2)
			&& (s_BoardInfos[dev->minor].i_InterruptFlag == FALSE)
			&& (s_BoardInfos[dev->minor].i_CJCAvailable == 1))
			/* END JK 06.07.04: Management of sevrals boards */
		{
			i_APCI3200_ReadCJCValue(dev, &ui_DummyValue);
			/* BEGIN JK 06.07.04: Management of sevrals boards */
			/* ui_InterruptChannelValue[i_Count + 3]=ui_DummyValue; */
			s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[s_BoardInfos[dev->
					minor].i_Count + 3] = ui_DummyValue;
			/* END JK 06.07.04: Management of sevrals boards */
		}		/* if((i_ADDIDATAType==2) && (i_InterruptFlag == FALSE)) */
		else {
			/* BEGIN JK 06.07.04: Management of sevrals boards */
			/* ui_InterruptChannelValue[i_Count + 3]=0; */
			s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[s_BoardInfos[dev->
					minor].i_Count + 3] = 0;
			/* END JK 06.07.04: Management of sevrals boards */
		}		/* elseif((i_ADDIDATAType==2) && (i_InterruptFlag == FALSE) && (i_CJCAvailable==1)) */

		/* BEGIN JK 06.07.04: Management of sevrals boards */
		/* if (( i_AutoCalibration == FALSE) && (i_InterruptFlag == FALSE)) */
		if ((s_BoardInfos[dev->minor].i_AutoCalibration == FALSE)
			&& (s_BoardInfos[dev->minor].i_InterruptFlag == FALSE))
			/* END JK 06.07.04: Management of sevrals boards */
		{
			i_APCI3200_ReadCalibrationOffsetValue(dev,
				&ui_DummyValue);
			/* BEGIN JK 06.07.04: Management of sevrals boards */
			/* ui_InterruptChannelValue[i_Count + 1]=ui_DummyValue; */
			s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[s_BoardInfos[dev->
					minor].i_Count + 1] = ui_DummyValue;
			/* END JK 06.07.04: Management of sevrals boards */
			i_APCI3200_ReadCalibrationGainValue(dev,
				&ui_DummyValue);
			/* BEGIN JK 06.07.04: Management of sevrals boards */
			/* ui_InterruptChannelValue[i_Count + 2]=ui_DummyValue; */
			s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[s_BoardInfos[dev->
					minor].i_Count + 2] = ui_DummyValue;
			/* END JK 06.07.04: Management of sevrals boards */
		}		/* if (( i_AutoCalibration == FALSE) && (i_InterruptFlag == FALSE)) */

		/* BEGIN JK 06.07.04: Management of sevrals boards */
		/* if((i_ADDIDATAType==2) && (i_InterruptFlag == FALSE)&& (i_CJCAvailable==1)) */
		if ((s_BoardInfos[dev->minor].i_ADDIDATAType == 2)
			&& (s_BoardInfos[dev->minor].i_InterruptFlag == FALSE)
			&& (s_BoardInfos[dev->minor].i_CJCAvailable == 1))
			/* END JK 06.07.04: Management of sevrals boards */
		{
	  /**********************************************************/
			/*Test if the Calibration channel must be read for the CJC */
	  /**********************************************************/
	  /**********************************/
			/*Test if the polarity is the same */
	  /**********************************/
			/* BEGIN JK 06.07.04: Management of sevrals boards */
			/* if(i_CJCPolarity!=i_ADDIDATAPolarity) */
			if (s_BoardInfos[dev->minor].i_CJCPolarity !=
				s_BoardInfos[dev->minor].i_ADDIDATAPolarity)
				/* END JK 06.07.04: Management of sevrals boards */
			{
				i_ConvertCJCCalibration = 1;
			}	/* if(i_CJCPolarity!=i_ADDIDATAPolarity) */
			else {
				/* BEGIN JK 06.07.04: Management of sevrals boards */
				/* if(i_CJCGain==i_ADDIDATAGain) */
				if (s_BoardInfos[dev->minor].i_CJCGain ==
					s_BoardInfos[dev->minor].i_ADDIDATAGain)
					/* END JK 06.07.04: Management of sevrals boards */
				{
					i_ConvertCJCCalibration = 0;
				}	/* if(i_CJCGain==i_ADDIDATAGain) */
				else {
					i_ConvertCJCCalibration = 1;
				}	/* elseif(i_CJCGain==i_ADDIDATAGain) */
			}	/* elseif(i_CJCPolarity!=i_ADDIDATAPolarity) */
			if (i_ConvertCJCCalibration == 1) {
				i_APCI3200_ReadCJCCalOffset(dev,
					&ui_DummyValue);
				/* BEGIN JK 06.07.04: Management of sevrals boards */
				/* ui_InterruptChannelValue[i_Count+4]=ui_DummyValue; */
				s_BoardInfos[dev->minor].
					ui_InterruptChannelValue[s_BoardInfos
					[dev->minor].i_Count + 4] =
					ui_DummyValue;
				/* END JK 06.07.04: Management of sevrals boards */

				i_APCI3200_ReadCJCCalGain(dev, &ui_DummyValue);

				/* BEGIN JK 06.07.04: Management of sevrals boards */
				/* ui_InterruptChannelValue[i_Count+5]=ui_DummyValue; */
				s_BoardInfos[dev->minor].
					ui_InterruptChannelValue[s_BoardInfos
					[dev->minor].i_Count + 5] =
					ui_DummyValue;
				/* END JK 06.07.04: Management of sevrals boards */
			}	/* if(i_ConvertCJCCalibration==1) */
			else {
				/* BEGIN JK 06.07.04: Management of sevrals boards */
				/* ui_InterruptChannelValue[i_Count+4]=0; */
				/* ui_InterruptChannelValue[i_Count+5]=0; */

				s_BoardInfos[dev->minor].
					ui_InterruptChannelValue[s_BoardInfos
					[dev->minor].i_Count + 4] = 0;
				s_BoardInfos[dev->minor].
					ui_InterruptChannelValue[s_BoardInfos
					[dev->minor].i_Count + 5] = 0;
				/* END JK 06.07.04: Management of sevrals boards */
			}	/* elseif(i_ConvertCJCCalibration==1) */
		}		/* if((i_ADDIDATAType==2) && (i_InterruptFlag == FALSE)) */

		/* BEGIN JK 06.07.04: Management of sevrals boards */
		/* if(i_ScanType!=1) */
		if (s_BoardInfos[dev->minor].i_ScanType != 1) {
			/* i_Count=0; */
			s_BoardInfos[dev->minor].i_Count = 0;
		}		/* if(i_ScanType!=1) */
		else {
			/* i_Count=i_Count +6; */
			/* Begin JK 22.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */
			/* s_BoardInfos [dev->minor].i_Count=s_BoardInfos [dev->minor].i_Count +6; */
			s_BoardInfos[dev->minor].i_Count =
				s_BoardInfos[dev->minor].i_Count + 9;
			/* End JK 22.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */
		}		/* else if(i_ScanType!=1) */

		/* if((i_ScanType==1) &&(i_InterruptFlag==1)) */
		if ((s_BoardInfos[dev->minor].i_ScanType == 1)
			&& (s_BoardInfos[dev->minor].i_InterruptFlag == 1)) {
			/* i_Count=i_Count-6; */
			/* Begin JK 22.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */
			/* s_BoardInfos [dev->minor].i_Count=s_BoardInfos [dev->minor].i_Count-6; */
			s_BoardInfos[dev->minor].i_Count =
				s_BoardInfos[dev->minor].i_Count - 9;
			/* End JK 22.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */
		}
		/* if(i_ScanType==0) */
		if (s_BoardInfos[dev->minor].i_ScanType == 0) {
			/*
			   data[0]= ui_InterruptChannelValue[0];
			   data[1]= ui_InterruptChannelValue[1];
			   data[2]= ui_InterruptChannelValue[2];
			   data[3]= ui_InterruptChannelValue[3];
			   data[4]= ui_InterruptChannelValue[4];
			   data[5]= ui_InterruptChannelValue[5];
			 */
#ifdef PRINT_INFO
			printk("\n data[0]= s_BoardInfos [dev->minor].ui_InterruptChannelValue[0];");
#endif
			data[0] =
				s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[0];
			data[1] =
				s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[1];
			data[2] =
				s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[2];
			data[3] =
				s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[3];
			data[4] =
				s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[4];
			data[5] =
				s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[5];

			/* Begin JK 22.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */
			/* printk("\n 0 - i_APCI3200_GetChannelCalibrationValue data [6] = %lu, data [7] = %lu, data [8] = %lu", data [6], data [7], data [8]); */
			i_APCI3200_GetChannelCalibrationValue(dev,
				s_BoardInfos[dev->minor].ui_Channel_num,
				&data[6], &data[7], &data[8]);
			/* End JK 22.10.2004: APCI-3200 / APCI-3300 Reading of EEPROM values */
		}
		break;
	case 1:

		for (i = 0; i < insn->n; i++) {
			/* data[i]=ui_InterruptChannelValue[i]; */
			data[i] =
				s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[i];
		}

		/* i_Count=0; */
		/* i_Sum=0; */
		/* if(i_ScanType==1) */
		s_BoardInfos[dev->minor].i_Count = 0;
		s_BoardInfos[dev->minor].i_Sum = 0;
		if (s_BoardInfos[dev->minor].i_ScanType == 1) {
			/* i_Initialised=0; */
			/* i_InterruptFlag=0; */
			s_BoardInfos[dev->minor].i_Initialised = 0;
			s_BoardInfos[dev->minor].i_InterruptFlag = 0;
			/* END JK 06.07.04: Management of sevrals boards */
		}
		break;
	default:
		printk("\nThe parameters passed are in error\n");
		i_APCI3200_Reset(dev);
		return -EINVAL;
	}			/* switch(insn->unused[0]) */

	return insn->n;
}

/*
  +----------------------------------------------------------------------------+
  | Function   Name   : int i_APCI3200_Read1AnalogInputChannel                 |
  |			          (struct comedi_device *dev,struct comedi_subdevice *s,       |
  |                     struct comedi_insn *insn,unsigned int *data)                      |
  +----------------------------------------------------------------------------+
  | Task              : Read  value  of the selected channel			         |
  +----------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev      : Driver handle                |
  |                     unsigned int ui_NoOfChannel    : Channel No to read            |
  |                     unsigned int *data              : Data Pointer to read status  |
  +----------------------------------------------------------------------------+
  | Output Parameters :	--													 |
  |			          data[0]  : Digital Value read                   |
  |
  +----------------------------------------------------------------------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur. Return the error          |
  |			                                                         |
  +----------------------------------------------------------------------------+
*/
int i_APCI3200_Read1AnalogInputChannel(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_EOC = 0;
	unsigned int ui_ChannelNo = 0;
	unsigned int ui_CommandRegister = 0;

	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* ui_ChannelNo=i_ChannelNo; */
	ui_ChannelNo = s_BoardInfos[dev->minor].i_ChannelNo;

	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
  /*********************************/
	/* Write the channel to configure */
  /*********************************/
	/* Begin JK 20.10.2004: Bad channel value is used when using differential mode */
	/* outl(0 | ui_Channel_num , devpriv->iobase+i_Offset + 0x4); */
	/* outl(0 | s_BoardInfos [dev->minor].ui_Channel_num , devpriv->iobase+s_BoardInfos [dev->minor].i_Offset + 0x4); */
	outl(0 | s_BoardInfos[dev->minor].i_ChannelNo,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 0x4);
	/* End JK 20.10.2004: Bad channel value is used when using differential mode */

  /*******************************/
	/* Set the convert timing unit */
  /*******************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;

	/* outl(i_ADDIDATAConversionTimeUnit , devpriv->iobase+i_Offset + 36); */
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTimeUnit,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 36);

  /**************************/
	/* Set the convert timing */
  /**************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;

	/* outl(i_ADDIDATAConversionTime , devpriv->iobase+i_Offset + 32); */
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTime,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 32);

  /**************************************************************************/
	/* Set the start end stop index to the selected channel and set the start */
  /**************************************************************************/

	ui_CommandRegister = ui_ChannelNo | (ui_ChannelNo << 8) | 0x80000;

  /*********************************/
	/*Test if the interrupt is enable */
  /*********************************/

	/* if (i_InterruptFlag == ADDIDATA_ENABLE) */
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_ENABLE) {
      /************************/
		/* Enable the interrupt */
      /************************/
		ui_CommandRegister = ui_CommandRegister | 0x00100000;
	}			/* if (i_InterruptFlag == ADDIDATA_ENABLE) */

  /******************************/
	/* Write the command register */
  /******************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;

	/* outl(ui_CommandRegister, devpriv->iobase+i_Offset + 8); */
	outl(ui_CommandRegister,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);

  /*****************************/
	/*Test if interrupt is enable */
  /*****************************/
	/* if (i_InterruptFlag == ADDIDATA_DISABLE) */
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_DISABLE) {
		do {
	  /*************************/
			/*Read the EOC Status bit */
	  /*************************/

			/* ui_EOC = inl(devpriv->iobase+i_Offset + 20) & 1; */
			ui_EOC = inl(devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 20) & 1;

		} while (ui_EOC != 1);

      /***************************************/
		/* Read the digital value of the input */
      /***************************************/

		/* data[0] = inl (devpriv->iobase+i_Offset + 28); */
		data[0] =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 28);
		/* END JK 06.07.04: Management of sevrals boards */

	}			/*  if (i_InterruptFlag == ADDIDATA_DISABLE) */
	return 0;
}

/*
  +----------------------------------------------------------------------------+
  | Function   Name   : int i_APCI3200_ReadCalibrationOffsetValue              |
  |			          (struct comedi_device *dev,struct comedi_subdevice *s,       |
  |                     struct comedi_insn *insn,unsigned int *data)                      |
  +----------------------------------------------------------------------------+
  | Task              : Read calibration offset  value  of the selected channel|
  +----------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev      : Driver handle                |
  |                     unsigned int *data              : Data Pointer to read status  |
  +----------------------------------------------------------------------------+
  | Output Parameters :	--													 |
  |			          data[0]  : Calibration offset Value   |
  |
  +----------------------------------------------------------------------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur. Return the error          |
  |			                                                         |
  +----------------------------------------------------------------------------+
*/
int i_APCI3200_ReadCalibrationOffsetValue(struct comedi_device *dev, unsigned int *data)
{
	unsigned int ui_Temp = 0, ui_EOC = 0;
	unsigned int ui_CommandRegister = 0;

	/* BEGIN JK 06.07.04: Management of sevrals boards */
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
  /*********************************/
	/* Write the channel to configure */
  /*********************************/
	/* Begin JK 20.10.2004: This seems not necessary ! */
	/* outl(0 | ui_Channel_num , devpriv->iobase+i_Offset + 0x4); */
	/* outl(0 | s_BoardInfos [dev->minor].ui_Channel_num , devpriv->iobase+s_BoardInfos [dev->minor].i_Offset + 0x4); */
	/* End JK 20.10.2004: This seems not necessary ! */

  /*******************************/
	/* Set the convert timing unit */
  /*******************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(i_ADDIDATAConversionTimeUnit , devpriv->iobase+i_Offset + 36); */
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTimeUnit,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 36);
  /**************************/
	/* Set the convert timing */
  /**************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(i_ADDIDATAConversionTime , devpriv->iobase+i_Offset + 32); */
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTime,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 32);
  /*****************************/
	/*Read the calibration offset */
  /*****************************/
	/* ui_Temp = inl(devpriv->iobase+i_Offset + 12); */
	ui_Temp = inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);

  /*********************************/
	/*Configure the Offset Conversion */
  /*********************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl((ui_Temp | 0x00020000), devpriv->iobase+i_Offset + 12); */
	outl((ui_Temp | 0x00020000),
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);
  /*******************************/
	/*Initialise ui_CommandRegister */
  /*******************************/

	ui_CommandRegister = 0;

  /*********************************/
	/*Test if the interrupt is enable */
  /*********************************/

	/* if (i_InterruptFlag == ADDIDATA_ENABLE) */
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_ENABLE) {

      /**********************/
		/*Enable the interrupt */
      /**********************/

		ui_CommandRegister = ui_CommandRegister | 0x00100000;

	}			/* if (i_InterruptFlag == ADDIDATA_ENABLE) */

  /**********************/
	/*Start the conversion */
  /**********************/
	ui_CommandRegister = ui_CommandRegister | 0x00080000;

  /***************************/
	/*Write the command regiter */
  /***************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(ui_CommandRegister, devpriv->iobase+i_Offset + 8); */
	outl(ui_CommandRegister,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);

  /*****************************/
	/*Test if interrupt is enable */
  /*****************************/

	/* if (i_InterruptFlag == ADDIDATA_DISABLE) */
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_DISABLE) {

		do {
	  /*******************/
			/*Read the EOC flag */
	  /*******************/

			/* ui_EOC = inl (devpriv->iobase+i_Offset + 20) & 1; */
			ui_EOC = inl(devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 20) & 1;

		} while (ui_EOC != 1);

      /**************************************************/
		/*Read the digital value of the calibration Offset */
      /**************************************************/

		/* data[0] = inl(devpriv->iobase+i_Offset+ 28); */
		data[0] =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 28);
	}			/* if (i_InterruptFlag == ADDIDATA_DISABLE) */
	return 0;
}

/*
  +----------------------------------------------------------------------------+
  | Function   Name   : int i_APCI3200_ReadCalibrationGainValue                |
  |			          (struct comedi_device *dev,struct comedi_subdevice *s,       |
  |                     struct comedi_insn *insn,unsigned int *data)                      |
  +----------------------------------------------------------------------------+
  | Task              : Read calibration gain  value  of the selected channel  |
  +----------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev      : Driver handle                |
  |                     unsigned int *data              : Data Pointer to read status  |
  +----------------------------------------------------------------------------+
  | Output Parameters :	--													 |
  |			          data[0]  : Calibration gain Value Of Input     |
  |
  +----------------------------------------------------------------------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur. Return the error          |
  |			                                                         |
  +----------------------------------------------------------------------------+
*/
int i_APCI3200_ReadCalibrationGainValue(struct comedi_device *dev, unsigned int *data)
{
	unsigned int ui_EOC = 0;
	int ui_CommandRegister = 0;

	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
  /*********************************/
	/* Write the channel to configure */
  /*********************************/
	/* Begin JK 20.10.2004: This seems not necessary ! */
	/* outl(0 | ui_Channel_num , devpriv->iobase+i_Offset + 0x4); */
	/* outl(0 | s_BoardInfos [dev->minor].ui_Channel_num , devpriv->iobase+s_BoardInfos [dev->minor].i_Offset + 0x4); */
	/* End JK 20.10.2004: This seems not necessary ! */

  /***************************/
	/*Read the calibration gain */
  /***************************/
  /*******************************/
	/* Set the convert timing unit */
  /*******************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(i_ADDIDATAConversionTimeUnit , devpriv->iobase+i_Offset + 36); */
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTimeUnit,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 36);
  /**************************/
	/* Set the convert timing */
  /**************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(i_ADDIDATAConversionTime , devpriv->iobase+i_Offset + 32); */
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTime,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 32);
  /*******************************/
	/*Configure the Gain Conversion */
  /*******************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(0x00040000 , devpriv->iobase+i_Offset + 12); */
	outl(0x00040000,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);

  /*******************************/
	/*Initialise ui_CommandRegister */
  /*******************************/

	ui_CommandRegister = 0;

  /*********************************/
	/*Test if the interrupt is enable */
  /*********************************/

	/* if (i_InterruptFlag == ADDIDATA_ENABLE) */
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_ENABLE) {

      /**********************/
		/*Enable the interrupt */
      /**********************/

		ui_CommandRegister = ui_CommandRegister | 0x00100000;

	}			/* if (i_InterruptFlag == ADDIDATA_ENABLE) */

  /**********************/
	/*Start the conversion */
  /**********************/

	ui_CommandRegister = ui_CommandRegister | 0x00080000;
  /***************************/
	/*Write the command regiter */
  /***************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(ui_CommandRegister , devpriv->iobase+i_Offset + 8); */
	outl(ui_CommandRegister,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);

  /*****************************/
	/*Test if interrupt is enable */
  /*****************************/

	/* if (i_InterruptFlag == ADDIDATA_DISABLE) */
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_DISABLE) {

		do {

	  /*******************/
			/*Read the EOC flag */
	  /*******************/

			/* ui_EOC = inl(devpriv->iobase+i_Offset + 20) & 1; */
			ui_EOC = inl(devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 20) & 1;

		} while (ui_EOC != 1);

      /************************************************/
		/*Read the digital value of the calibration Gain */
      /************************************************/

		/* data[0] = inl(devpriv->iobase+i_Offset + 28); */
		data[0] =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 28);

	}			/* if (i_InterruptFlag == ADDIDATA_DISABLE) */
	return 0;
}

/*
  +----------------------------------------------------------------------------+
  | Function   Name   : int i_APCI3200_ReadCJCValue                            |
  |			          (struct comedi_device *dev,struct comedi_subdevice *s,       |
  |                     struct comedi_insn *insn,unsigned int *data)                      |
  +----------------------------------------------------------------------------+
  | Task              : Read CJC  value  of the selected channel               |
  +----------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev      : Driver handle                |
  |                     unsigned int *data              : Data Pointer to read status  |
  +----------------------------------------------------------------------------+
  | Output Parameters :	--													 |
  |			          data[0]  : CJC Value                           |
  |
  +----------------------------------------------------------------------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur. Return the error          |
  |			                                                         |
  +----------------------------------------------------------------------------+
*/

int i_APCI3200_ReadCJCValue(struct comedi_device *dev, unsigned int *data)
{
	unsigned int ui_EOC = 0;
	int ui_CommandRegister = 0;

  /******************************/
	/*Set the converting time unit */
  /******************************/

	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;

	/* outl(i_ADDIDATAConversionTimeUnit , devpriv->iobase+i_Offset + 36); */
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTimeUnit,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 36);
  /**************************/
	/* Set the convert timing */
  /**************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;

	/* outl(i_ADDIDATAConversionTime , devpriv->iobase+i_Offset + 32); */
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTime,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 32);

  /******************************/
	/*Configure the CJC Conversion */
  /******************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;

	/* outl( 0x00000400 , devpriv->iobase+i_Offset + 4); */
	outl(0x00000400,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 4);
  /*******************************/
	/*Initialise dw_CommandRegister */
  /*******************************/
	ui_CommandRegister = 0;
  /*********************************/
	/*Test if the interrupt is enable */
  /*********************************/
	/* if (i_InterruptFlag == ADDIDATA_ENABLE) */
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_ENABLE) {
      /**********************/
		/*Enable the interrupt */
      /**********************/
		ui_CommandRegister = ui_CommandRegister | 0x00100000;
	}

  /**********************/
	/*Start the conversion */
  /**********************/

	ui_CommandRegister = ui_CommandRegister | 0x00080000;

  /***************************/
	/*Write the command regiter */
  /***************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(ui_CommandRegister , devpriv->iobase+i_Offset + 8); */
	outl(ui_CommandRegister,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);

  /*****************************/
	/*Test if interrupt is enable */
  /*****************************/

	/* if (i_InterruptFlag == ADDIDATA_DISABLE) */
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_DISABLE) {
		do {

	  /*******************/
			/*Read the EOC flag */
	  /*******************/

			/* ui_EOC = inl(devpriv->iobase+i_Offset + 20) & 1; */
			ui_EOC = inl(devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 20) & 1;

		} while (ui_EOC != 1);

      /***********************************/
		/*Read the digital value of the CJC */
      /***********************************/

		/* data[0] = inl(devpriv->iobase+i_Offset + 28); */
		data[0] =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 28);

	}			/* if (i_InterruptFlag == ADDIDATA_DISABLE) */
	return 0;
}

/*
  +----------------------------------------------------------------------------+
  | Function   Name   : int i_APCI3200_ReadCJCCalOffset                        |
  |			          (struct comedi_device *dev,struct comedi_subdevice *s,       |
  |                     struct comedi_insn *insn,unsigned int *data)                      |
  +----------------------------------------------------------------------------+
  | Task              : Read CJC calibration offset  value  of the selected channel
  +----------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev      : Driver handle                |
  |                     unsigned int *data              : Data Pointer to read status  |
  +----------------------------------------------------------------------------+
  | Output Parameters :	--													 |
  |			          data[0]  : CJC calibration offset Value
  |
  +----------------------------------------------------------------------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur. Return the error          |
  |			                                                         |
  +----------------------------------------------------------------------------+
*/
int i_APCI3200_ReadCJCCalOffset(struct comedi_device *dev, unsigned int *data)
{
	unsigned int ui_EOC = 0;
	int ui_CommandRegister = 0;
  /*******************************************/
	/*Read calibration offset value for the CJC */
  /*******************************************/
  /*******************************/
	/* Set the convert timing unit */
  /*******************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(i_ADDIDATAConversionTimeUnit , devpriv->iobase+i_Offset + 36); */
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTimeUnit,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 36);
  /**************************/
	/* Set the convert timing */
  /**************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(i_ADDIDATAConversionTime , devpriv->iobase+i_Offset + 32); */
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTime,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 32);
  /******************************/
	/*Configure the CJC Conversion */
  /******************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(0x00000400 , devpriv->iobase+i_Offset + 4); */
	outl(0x00000400,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 4);
  /*********************************/
	/*Configure the Offset Conversion */
  /*********************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(0x00020000, devpriv->iobase+i_Offset + 12); */
	outl(0x00020000,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);
  /*******************************/
	/*Initialise ui_CommandRegister */
  /*******************************/
	ui_CommandRegister = 0;
  /*********************************/
	/*Test if the interrupt is enable */
  /*********************************/

	/* if (i_InterruptFlag == ADDIDATA_ENABLE) */
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_ENABLE) {
      /**********************/
		/*Enable the interrupt */
      /**********************/
		ui_CommandRegister = ui_CommandRegister | 0x00100000;

	}

  /**********************/
	/*Start the conversion */
  /**********************/
	ui_CommandRegister = ui_CommandRegister | 0x00080000;
  /***************************/
	/*Write the command regiter */
  /***************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(ui_CommandRegister,devpriv->iobase+i_Offset + 8); */
	outl(ui_CommandRegister,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);
	/* if (i_InterruptFlag == ADDIDATA_DISABLE) */
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_DISABLE) {
		do {
	  /*******************/
			/*Read the EOC flag */
	  /*******************/
			/* ui_EOC = inl(devpriv->iobase+i_Offset + 20) & 1; */
			ui_EOC = inl(devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 20) & 1;
		} while (ui_EOC != 1);

      /**************************************************/
		/*Read the digital value of the calibration Offset */
      /**************************************************/
		/* data[0] = inl(devpriv->iobase+i_Offset + 28); */
		data[0] =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 28);
	}			/* if (i_InterruptFlag == ADDIDATA_DISABLE) */
	return 0;
}

/*
  +----------------------------------------------------------------------------+
  | Function   Name   : int i_APCI3200_ReadCJCGainValue                        |
  |			          (struct comedi_device *dev,struct comedi_subdevice *s,       |
  |                     struct comedi_insn *insn,unsigned int *data)                      |
  +----------------------------------------------------------------------------+
  | Task              : Read CJC calibration gain value
  +----------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev      : Driver handle                |
  |                     unsigned int ui_NoOfChannels    : No Of Channels To read       |
  |                     unsigned int *data              : Data Pointer to read status  |
  +----------------------------------------------------------------------------+
  | Output Parameters :	--													 |
  |			          data[0]  : CJC calibration gain value
  |
  +----------------------------------------------------------------------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur. Return the error          |
  |			                                                         |
  +----------------------------------------------------------------------------+
*/
int i_APCI3200_ReadCJCCalGain(struct comedi_device *dev, unsigned int *data)
{
	unsigned int ui_EOC = 0;
	int ui_CommandRegister = 0;
  /*******************************/
	/* Set the convert timing unit */
  /*******************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(i_ADDIDATAConversionTimeUnit , devpriv->iobase+i_Offset + 36); */
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTimeUnit,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 36);
  /**************************/
	/* Set the convert timing */
  /**************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(i_ADDIDATAConversionTime , devpriv->iobase+i_Offset + 32); */
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTime,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 32);
  /******************************/
	/*Configure the CJC Conversion */
  /******************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(0x00000400,devpriv->iobase+i_Offset + 4); */
	outl(0x00000400,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 4);
  /*******************************/
	/*Configure the Gain Conversion */
  /*******************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(0x00040000,devpriv->iobase+i_Offset + 12); */
	outl(0x00040000,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);

  /*******************************/
	/*Initialise dw_CommandRegister */
  /*******************************/
	ui_CommandRegister = 0;
  /*********************************/
	/*Test if the interrupt is enable */
  /*********************************/
	/* if (i_InterruptFlag == ADDIDATA_ENABLE) */
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_ENABLE) {
      /**********************/
		/*Enable the interrupt */
      /**********************/
		ui_CommandRegister = ui_CommandRegister | 0x00100000;
	}
  /**********************/
	/*Start the conversion */
  /**********************/
	ui_CommandRegister = ui_CommandRegister | 0x00080000;
  /***************************/
	/*Write the command regiter */
  /***************************/
	/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	/* outl(ui_CommandRegister ,devpriv->iobase+i_Offset + 8); */
	outl(ui_CommandRegister,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);
	/* if (i_InterruptFlag == ADDIDATA_DISABLE) */
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_DISABLE) {
		do {
	  /*******************/
			/*Read the EOC flag */
	  /*******************/
			/* ui_EOC = inl(devpriv->iobase+i_Offset + 20) & 1; */
			ui_EOC = inl(devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 20) & 1;
		} while (ui_EOC != 1);
      /************************************************/
		/*Read the digital value of the calibration Gain */
      /************************************************/
		/* data[0] = inl (devpriv->iobase+i_Offset + 28); */
		data[0] =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 28);
	}			/* if (i_InterruptFlag == ADDIDATA_DISABLE) */
	return 0;
}

/*
  +----------------------------------------------------------------------------+
  | Function   Name   : int i_APCI3200_InsnBits_AnalogInput_Test               |
  |			  (struct comedi_device *dev,struct comedi_subdevice *s,               |
  |                      struct comedi_insn *insn,unsigned int *data)                     |
  +----------------------------------------------------------------------------+
  | Task              : Tests the Selected Anlog Input Channel                 |
  +----------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev      : Driver handle                |
  |                     struct comedi_subdevice *s     : Subdevice Pointer            |
  |                     struct comedi_insn *insn       : Insn Structure Pointer       |
  |                     unsigned int *data          : Data Pointer contains        |
  |                                          configuration parameters as below |
  |
  |
  |                           data[0]            : 0 TestAnalogInputShortCircuit
  |									     1 TestAnalogInputConnection							 														                        |

  +----------------------------------------------------------------------------+
  | Output Parameters :	--													 |
  |			        data[0]            : Digital value obtained      |
  |                           data[1]            : calibration offset          |
  |                           data[2]            : calibration gain            |
  |			                                                         |
  |			                                                         |
  +----------------------------------------------------------------------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur. Return the error          |
  |			                                                         |
  +----------------------------------------------------------------------------+
*/

int i_APCI3200_InsnBits_AnalogInput_Test(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_Configuration = 0;
	int i_Temp;		/* ,i_TimeUnit; */
	/* if(i_Initialised==0) */

	if (s_BoardInfos[dev->minor].i_Initialised == 0) {
		i_APCI3200_Reset(dev);
		return -EINVAL;
	}			/* if(i_Initialised==0); */
	if (data[0] != 0 && data[0] != 1) {
		printk("\nError in selection of functionality\n");
		i_APCI3200_Reset(dev);
		return -EINVAL;
	}			/* if(data[0]!=0 && data[0]!=1) */

	if (data[0] == 1)	/* Perform Short Circuit TEST */
	{
      /**************************/
		/*Set the short-cicuit bit */
      /**************************/
		/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
		while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].
						i_Offset + 12) >> 19) & 1) !=
			1) ;
		/* outl((0x00001000 |i_ChannelNo) , devpriv->iobase+i_Offset + 4); */
		outl((0x00001000 | s_BoardInfos[dev->minor].i_ChannelNo),
			devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
			4);
      /*************************/
		/*Set the time unit to ns */
      /*************************/
		/* i_TimeUnit= i_ADDIDATAConversionTimeUnit;
		   i_ADDIDATAConversionTimeUnit= 1; */
		/* i_Temp= i_InterruptFlag ; */
		i_Temp = s_BoardInfos[dev->minor].i_InterruptFlag;
		/* i_InterruptFlag = ADDIDATA_DISABLE; */
		s_BoardInfos[dev->minor].i_InterruptFlag = ADDIDATA_DISABLE;
		i_APCI3200_Read1AnalogInputChannel(dev, s, insn, data);
		/* if(i_AutoCalibration == FALSE) */
		if (s_BoardInfos[dev->minor].i_AutoCalibration == FALSE) {
			/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
			while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].
							i_Offset +
							12) >> 19) & 1) != 1) ;

			/* outl((0x00001000 |i_ChannelNo) , devpriv->iobase+i_Offset + 4); */
			outl((0x00001000 | s_BoardInfos[dev->minor].
					i_ChannelNo),
				devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 4);
			data++;
			i_APCI3200_ReadCalibrationOffsetValue(dev, data);
			data++;
			i_APCI3200_ReadCalibrationGainValue(dev, data);
		}
	} else {
		/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
		while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].
						i_Offset + 12) >> 19) & 1) !=
			1) ;
		/* outl((0x00000800|i_ChannelNo) , devpriv->iobase+i_Offset + 4); */
		outl((0x00000800 | s_BoardInfos[dev->minor].i_ChannelNo),
			devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
			4);
		/* ui_Configuration = inl(devpriv->iobase+i_Offset + 0); */
		ui_Configuration =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 0);
      /*************************/
		/*Set the time unit to ns */
      /*************************/
		/* i_TimeUnit= i_ADDIDATAConversionTimeUnit;
		   i_ADDIDATAConversionTimeUnit= 1; */
		/* i_Temp= i_InterruptFlag ; */
		i_Temp = s_BoardInfos[dev->minor].i_InterruptFlag;
		/* i_InterruptFlag = ADDIDATA_DISABLE; */
		s_BoardInfos[dev->minor].i_InterruptFlag = ADDIDATA_DISABLE;
		i_APCI3200_Read1AnalogInputChannel(dev, s, insn, data);
		/* if(i_AutoCalibration == FALSE) */
		if (s_BoardInfos[dev->minor].i_AutoCalibration == FALSE) {
			/* while (((inl(devpriv->iobase+i_Offset+12)>>19) & 1) != 1); */
			while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].
							i_Offset +
							12) >> 19) & 1) != 1) ;
			/* outl((0x00000800|i_ChannelNo) , devpriv->iobase+i_Offset + 4); */
			outl((0x00000800 | s_BoardInfos[dev->minor].
					i_ChannelNo),
				devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 4);
			data++;
			i_APCI3200_ReadCalibrationOffsetValue(dev, data);
			data++;
			i_APCI3200_ReadCalibrationGainValue(dev, data);
		}
	}
	/* i_InterruptFlag=i_Temp ; */
	s_BoardInfos[dev->minor].i_InterruptFlag = i_Temp;
	/* printk("\ni_InterruptFlag=%d\n",i_InterruptFlag); */
	return insn->n;
}

/*
  +----------------------------------------------------------------------------+
  | Function   Name   : int i_APCI3200_InsnWriteReleaseAnalogInput             |
  |			  (struct comedi_device *dev,struct comedi_subdevice *s,               |
  |                      struct comedi_insn *insn,unsigned int *data)                     |
  +----------------------------------------------------------------------------+
  | Task              :  Resets the channels                                                      |
  +--------------------------------/**
@verbatim

Copyright (C) 2004,2005  ADDI+
  | Input Parameters  : struct comedi_device *dev I-DAT: Driver handleI-DATA 3
	D-7783|ATA Grsweier
	Tel: +19(0)ode of this mosubdule.

	s-DATA GS: +49(0)7Pointere 3
	D-77833 Ottersweier
	Tel: +19(0)7223/9493-0
	Fainsn *e sofI-DATA GI sofSde ofur://www.addi-data Ottersweier
	Tel: +19(0)7unsigned int *datarsweier
	T: Dubli/www.ad
  +/**
@verbatim

Copyright (C) 2004,2005  ADDI-
Copyright (C) 2004,2005  ADDI-DATA GOutH for the sourc:	--	is program i |
by the Free Software Foundation; either version 2 of the License, or (at your option) Return Valutrasse : TRUEnty No error occuddi-data-com
 A PARTICULAR PURPOSE Otte		 A PARTICULA: FALSE : EBILITY or .the impltheTABILIT PURPOSE. See thhe GNU Generaof the GNU General Public License along with  Ottthe Free Software Foundation; either version 2 of the License, or (at your opti*/

erali_APCI3200_ redWriteReleaseAnalogmbH f(ode of this module.

	ADD,
	223/9493-0
	Fax: +49(0)722, program is free software,he GNU General Publ)
{
	0, Boston, Reset(dev);
	re imple so->n;
}

/*by the Free Software Foundation; either version 2 of the License, or (at your option) Function namtrasse:e 330, Boston, CommandTestA

You shoud also find the complete Gve recei,in the COPYING file accompaode of this mocmd *cmd)ceived a cop Ottersweier
	Tel: +19(0)7 +49 (0) 7223/9493-rogram is diy the Free Software Foundation; either version 2 of the License, or (at your option) Task +49 (0) 7223/: ---- validity for a thi----ject cyclic anlog ibH fo-data.com         |
  | Fax : +4acquisitersw92    |493-92  ta.com         |
  | Fax : program is nse along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, TA GmbH for the source code of this module.

	ADD2    | Interneersweier
	Tel: +19(0)7223/9493-0
	Fax: +49(0)722            |
  +-------------------------------+-il    : on    : 2.96       @addi-data.com         |
  | Fax :                    |
  +-------                |
  +-------------------------------------: +49 (0) 7223/9493-92    | In+
  |                             UPDATES                                  ibuted in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty0ayer Acces For APCI-3200    e 3
	D-77833 Ottersweier
	Tel: +19(0)is program is th this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, ------------------------------------------------anying this sour file accompaL in the COPYINGil    : i
{

	eralerr = 0;      tmp;		/*  divisor1,       2; */
	e GNU Generalui_ConvertTime  |
  |                           Bas        |
  +----------+Delay           |
  +----------+---------+----------e 330,Triggermodauth | - Update for CEdgauth | - UpdatNbrOfChannel  |
  |    i_Cpt  |
  |double d       sion    ForAll  |    s  |
.rom value    SCAN    NewUni eepr     |   step 1: make ste it for C sources a      vially+
  |     
	tmp = cmd->start_src;
	-------------- &= TRIG_NOW-----+
*EXT| - f (!---------------||     !----------------)
		err++;--------------can_begin-------------+
  |        ------+
*TIMER/*
  +--FOLLOW--------------              ----------------+
  |        -----------------------c      ----------------------- Included file-------------+
*/
#includ--------------------------------------------------+
  end                 */
#inc------+
*COUN---------------i_amcc_S5920--------------------*/
#inc------------------------top-------------- value0.h"
/* #defin/*
  +--NONE---------------EGIN JK----------------- value------------/* if(i_Iw.adruptFlag==0)     ----s_BoardInfos[dev->minor].y=0;
  int i_CJ == 0) {-----------00   olarity;
printk("\nThenera  int  should be enabled\n")      };/* cherr
  int--------------------------------1; /* ;/* ch---------------!----+
*/

/&&---------------nversionTEXT
  int i_ADDID* : aut;
  int i_ADDID=TAType;
  int i_                -----------arg & 0xFFFFATACate for COMEDI 0rstChannel;
  i>> 16tChanf (              < 1  in              > 3
  intt i_ADDIDA	 int i_ADDIDAT        e    seletterswis inTABILIion=0
	No;
unsigned int uOMEDI!= 2 i_Count=0;
  int i_Initialised=0;
  unOMEDId int ui_InterruptChannelValue[9me;
  int i_AD              versionT files&&
	                     versionT      -----------;
  int i_C+
*/
#includ6.07.04: Mana 21.10.2004: APCI-3200*/

/* End JKversionTdefin
  int/*+---------------h"
/* #define P_ChannelNo;;
  int i_ChaEGIN JKversionTimNEUnit;
  intn   Name   : in------21.10.2004: APCIo calibration */
  int i_ADDIDATAConversi2elNo;
olar_First  |    =-3200 hanlist[0]      anged from 0 to 2 */
  int WordsToRead,---------            i_NbOfLadsToRead,               1   |*/
/*|                        ddress,    gned int dw_PCIBo1]04: APCI-3200 / APCI-3300t=0;
  i of EE  int                 -3200 / APCI-
  int i_LastCha--+-----------+------unsigned short * fset;
  unsig               
str0Unit------------------40 int---------------------8-----------------------160    _Coun int i_ADDIDATd int ui_Iof ------     t    reload+
  ueInterruptChannelValut i_ADDIDA}DATAPo------------------!=--------------------!=4------+*/
/*| Input Pk              : Rea!=160 in=2;/-------------------+----
struct str5920 eeprom.                        |*/
/*+uend ------------------------------------ifad        |*/
/*|  !=2in=2;/} else                      
  |)                     
  |o;
  int i_Ch              t=0;
  igin JK         ----------------------------- |*/
/*+----	        -------------------==-------------    EepromStartA----------------------------t * pw_DataRead)   ---+*/
/*| Outpu     |*/
/*+---------------------------+*/
/*| Out
str--------+*/
/*| Return Vt i_Count=0;
  int i_Initialised-----*/
/*+b----d int ui_InterruptChannelValue[96]; /             nnel_nu             > 102                                          |-------------------------e[96]; /o calibraation */
  int i_ADDIDATAC--------3Value[96fpu |    (lValu       | - Append AP(value )     : -    DIDATAP         |    =omAddress,    -          unsig+ 4      	         |      d sh                         unsigned short-char pb_ReadByte[1];
	unsigned 
	int i_WordCount
olarit/*char b_SelectedAddressLow = 0;
	unsigned char b_SelectedA;
	i/*calculateails.total          |*/
/*+ect  llails. dw_|
  |*/nsigned char b_SelectedAddressLow = 0;
	unsigned char b_SelectedAddres                          |
  | d shad)
{
	u(ad)
{
	unsi            ;
	iBoardEepr         |    );istrgned char b_SelectedAddressLow = 0;
dressH       ails.frequencATAG*/
/*+nter = 0; i_WordCounter < i_NbOfWordsToRedo {
			dw_eeprom_busy =
				inl(dw_PCIBoardEepr1.0 /                           |
   strat address     |*/
/*3unsigned char b_SelectedAddressLow = 0;
	unsdressH----+if	unsigdress : Aisails.sier nter = 0; i_WordCounter < i_NbOfWordsToRead;
---------------------+----<=---+-----------+-------EG_Mect (Read eeprom rdEepad e<ead        |*/
/*|   -26.10.04 | J. Kraned shP_REG_++BoardEep{
			dw_eeprom_busy =
				inl(dw_PCIBsy =
					inl(dw_PCIBoardEepromAd* 100AMCC_OP =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw+ionTined inw_EepromStapromAddress + AMCC_OP_REG_MCSR +

			outb(NVCMDait oLOW,
				dw_PCIBo {
				dw_eepromss +
	     | - Append APMCSR +
				2);

		_eeprom_Y);

			/ess mode                          |
  |>	/* Wait on busy */BoardEep int i_ADD           ------cannotutoCusation=0
	ed char b_SelectedAddressLow = 0;
	uREG_M/*w_eeprom_busy = dw_eeprom_busy ead dM_BUSY;
			} while (dw_eeprom_busy == EEP----------tartAddend unsirameterEepros : unsigned short * pw_DataRead : Read        |*/
/*|                               (i4nTime;
-------0-------------------------------+
  | (C) ADDI-DATA GmbH          Dieselstraße 3       D-77833 Ottersweier  |
  +--------------StopC     A| Module nd also find the complete GP Otter------+
  | 02.0     |           |        )a.com         |
  | Fax : +49 (0) 7223/9493-92    | Internet : http://www.addi-data.com   |
  +-------------------------------+-----------------------------------			/ails.  | Module name : hwdrv_a200.c| Version    : 2.96                     |
  +-------------------------------+---------------------------------------+
  | Project manager: Eric Stolz   | Date       :  02/12/2002              |
  +-------------------------------+---------------------------------------+
  | Desc | Fax : +49 (0) 7223/9493-92     |
  +-------------------------------+---------------------------------------+
  | Project manags                |
  +----------+-----------+------------------------------------------------+
  | 02.07.04 | J. Krauth | Modification from the driver in order to       |
  |          |           | correct			/* Load the high address */
			outb(b_SelectePCIBoardEepromAddress + AMCC +--                   figuratersw/*+---eprom0;
  int i_CJC0      er == 0itialiseddLowByte = pbCountdLowByte = pbSumdLowByteanged from 0 to 2 */
  int i_InterruptFlag*+---anged from 0 to 2 */
  int i_eadByte[0}
		w_ReadWord =
			(b_ReadLowByte		b_R}
		w_ReadWord =
			(b_ReadLowByteSum}
		w_
ed char b_SelectedAddryte =Readails.regis.addnter 	w_EepromStartAddress += s part */
			if (i_Cinl----priv->io*/
/+i_Offset + 8=0;   / part */
			if (i_ss m */
	return 0;
}

 + anged from 0 to 2 */
  int ------------ext word */

	}			/*  ftartAddress += 2;----ils.START APCIIRQ b*/
	next word */

	}			/*  f	}			/*  for (..while (((d */
	return 0;
}

/*+------+12)>>19) & 1)d wor=0;   /-----------------------------------------------+*/
/*| Function   ress +12)-----Task          unter outl(-------*/
			if (int i_LE7_Las),
	return 0;
}

/*+------------------------------------------------------o {
---------------------------------+*/
/*| Function   Name EepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == neral  | correct some es when using several boards. |
  |     ave recei---------CIBoardEepromAddress + AMCCta.com         |
  | Fax : +49 (0) 7223/9493-92    | Internet : http://www.addi-data.com   |
  +-------------------------------+-----------------------------------Do----synchronous  | Module namOR A PARTICULAR PURPOSE. See  int dw_PCIBoardEeproD souminead theOMEDI1 or 2.
			} while (dw_eeprom_busy == EEPROM_BUSY);

			/* Select the READ mode */
			outb(NVCMD_BEGIN_READ,
				dw_PCIBoardEepromAddress + AMCC_OP_REG_MCSR +
				3);

			/* Wait on busy */
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dis program isi3200.c| Version    : 2.96            | Internet : http://www.addi-data.com   |
  +-------------------------------+---------------he implied warranty +----------+-----------+----------------------------------------------------+
  | 02.07.04 | J. Krauth | Modification from the driver in order to       |
  |          |           | correct some es when using several boards. |
  |          |           |         the ription :   Hardware La= &s->Calib->cmd   |
  +----------+---*/
			if (i_Counter INT 
			urrentS
  +-}
		w                     e for C       |
  +----------+                |
Analog input header addOMEDI 0.7.6                ScanMddress);

  /*************                  |
  +----------+-----------+------------------------------------------------+
  | 26.10.04 | J. Krauth | -------+
  | 26.10.04 ***********_NbOfWordsToRead,                   |*/
promAddress,             |*/
/*|                                un        unsigned int dw_PCIBoardEe                         unsigned short   w_EepromStartAddr
  int i_ChannelCount=0;
  int i_ScanromStartAddreUSY);header address */rstChannel;
  int i_LastChaMainHeaderAddressm=0;
  int i_Offset;
  rameters : unsig--------w_DataR
  inad data         StartAddress: ress + AMCCulCounter < w_NumberOfModuls;
		/
/*| Function   Namt=0;
  i-----------******************rameter-----HeaderRW_Readw_DataR-------ad data         ***********nt   ess + AMCC_OP*************/
		w_AnalogInputCo------------------------------------------------------------------------+*/
/*| Output Pa---------******************s : unsigned short * pw_DataRead : Read data                          |*/
/*+--------------------------------------------------------------------er size **/
   onTimess + AMCC_OP_REG_MCSR +
				3);

			/* Wait on busATAPolarity; int i_ADDctedAddressL=%u\n",ctedAddressL=0;   / = w_FirstHeaderSize >> 4;

     +--- /*****************
			don=2;/* ch              |*/
/*|                                unsigned short * pw_DataRead)                          |*/
/*+-------------w_EepromStartAddress : Eeprom strat address     |*/
/*+-----*****/rity=ADDIDATAType ==el_nu(		w_NumberOfInp==2))erSize = w_F{*+----------------------------------ile (dw_eeprom_busy == E= 2;	/*  toold    */
			if (ioter) /cBoardE |
  +-******************/
      /** Read the CJC flag **/
      /***..) i_NbOfWordsToRead */
	return 0;
}

/*+---------12------------------------------------------------------------------+*/
/*| Function   N		&w***************/
      /** Read the CJC flag **/
   /***2111-iHeade******/
		i_AddiHeaderRW_ReadEeprom(1,	/* i_NbOfWordsToRead */
			dw_PCIBoardEepromAddre------------------------------------------+*/
/*| Task              : Read calibration value from the APCI-3200 eeprom.      |*/
/*+------------------------------------------------------------------C0**** ), 
	return 0;
}

/*+--------		&w_CJCFainValue & 0xFF;

      /**********               |*/
/*+-------------------------------------  /******/}-----------------------derAddreirstHeaderSize fordsi_WordC /*****************************/
    int i_ADDlamberOfGainValue / ddress,    ) +
			(6 * w_NumberOfGainter++) {
 /********e for CGainValue / 16) + 1) * 2));

          /****************    GainValue / 16) + 1) * 2));

      OMED/****************OMED**********/
    eaderSize >> 4;

 **** /*****************GainValue / 16) + 1) * 2));
******** /**********************w_AnalogInputComponentAdd**************|(unsigned short<< 8)| 0x00epro00 |--------------------------ls **/
  /******************************|changed from 0 to 2 _REG*/
  in
	intr address */
			& w_SingleHeaderS----StartAdd<< 24: - 		e module **i_Chan< 25ain e module **OMEDI****7********* i_NbOfW
		<< 1gain ----*********ogIn6y = dw char b_SelectedAddressLower of gain valu* i_NbOfWordsT+------------------------------------------------------------------------+*/
/*| Task              : Read calibration value from the APCI-3200 eeprom.      |*/
/*+------------------------------------------1,	/* i_NbOfWords***********************/
    0x : -         PCIBoardEepromAdd             |*/
/*+-------------------------------------Addrer (w_GainIndex = 0; w_GainIer of gain valu      ied wa+----------------------------------------------------------------------+*/
/*| Task              : Read calibration value from the APCI-3200 eeprom.      |*/
/*+------------------------------------------ctedAddressL---+*/
/*| Input Parameters 40ress + 70 + (2                        |*/
/*+-------------------------------------****r (w_GainIndex = 0; w_GainIndBoardInformations->s_Mo    |*/
/*+--------------------------------ue[w_GainIndex] = w_GainValue;

#             ifdef PRINT_INFO
			printk("\n Gain value = %d",
				BoardInformations->s_Module[w_ModulCounter].
				w_GainValue[w_GainIndex]);
#          +---   endif

	  /***************4*********************/
	

			 /** Read gain factor for the module **/
	  /*************4/*******************************/
     er of gain value *      |*/
/*+------*****************/
      /** Read the Cue[w_GainIndex] = w_GainValue;

#             ifdef PRINT_INFO
			printk("\n Gain value = %d",
				BoardInformations->s_Module[w_ModulCounter].
				w_GainValue[w_GainIndex]);
#            ---+*/
/*| Input Parameters 3  /** Compute/
      /** Rea             |*/
/*+-------------------------------------each;

		w_EepromStartAddres           ifdef PRINT_INFO
			printk("\n w_ i_NbOfWordsToRead */
				dw_PCIBoardE,
				BoardInformations->s_Module[w_ModulCounter].
				ul_GainFactor[w_GainIndex]);
#             endif
		}

      /***************************************************************/
      /** Re

			BoardInformations->s_Module36ch channels of the module ainFactor[w_GainIndex] =
				(w_GainFactorValue[1] << 16) +
		gleH

		w_EepromStartAddress += 2;	/*  to read the next word */

	}			/*  for (...) i_NbOfWordsToRead */
	return 0;
}

/*+---------w_ModulC-----------------------------------------------------------+*/
/*| Function   N	w_GainFactorValue[0];

# /***Salue (vo_eep/
/*+-----------------------------------------------------------------+*/
/*| Task              : Read calibration value from the APCI-3200 eeprom.      |*/
/*+-------------------------------------------- (2 * (1 + (w_Num-----1E0FFain fact002     */
			BoardInformations->s_Module[w***********************************/
      /** Re /** Read gain factor for the module **/
	  /********************************/
	  /** Save the current sources value **/
	  /*********,	/* i_NbOfWordsToRead */
.) i_NbOfWordsToRead */
	return 0;
}

/*+----------------------------------------------------------------------------+*/
/*| Function   Name ext word */

	}			/*  for (. << 16));id)  /
/*+------------------------------------------------------------------+*/
/*| Task              : Read calibration value from the APCI-3200 eeprom.      |*/
/*+------------------------------------------------------------/
     8
     --+*/
/*| Input Parameters  : -                           ].
			ul_Curre             |*/
/*+----------------------------------------------------------------------------+*/
/*| Output Parameters : -                                                 Nier  |: 
/*+------------------ode of this module.

	ADDinfo@addi------------e GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, -----------------------onValad the read thsAddiHeadearLayer Acces For--------------------------------------+
  |   Date   |   Author  |          Description of updatembH for the source cd a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, on) any later version.

This program is di
	unsigned short w_SingleHeaderAddress = 0;
	unsigned short w_SingleHeaderSize = 0;
	unsigned short w_Input = 0;
	unsihe GNU General Public License along with this ------py of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, Module = 0;

#ifdef PRINT_INFO
	 +---UpdateempAddress = w_Analdw_Dummyunter == 0)
				b_ReadLowByte = pb_ReadByte[0]];
			else
				b_ReadHighByte = pb_ReadByyte[0];


			/* Sleep */
			msleep(1);

		}
		w_ReadWord =
			(b_ReadLowByte | (((unsigned short)b_ReadHighByte) *
				256));

		pw_DataRead[i_WordCounter] = w_ReadWord;ize = w_SingleHeaderSize >b_stribu | (((unzigned sh*/
    0x83Channe************_I;
}

Amcc valu6****s =
		Elibraue (vAGain;
  iect value *trolthe ne	el = 0, Read */
	return 0| (ui_Channel_nu3----------f ((ui_Chw_Si/** num == 12) || (ui_Channel_nunum == 15))l = 1, i_Module = 3;
ddon);_Ana	if ((ui_Chony lat+-------------------- /***Emptyodulebuffthe next word */

	}			/
promAddr= 5)ddressPRINT_I<= 95
	printkCC_OP_RErrentS0;
  int   |    ied w[PRINT_]dLowByteize = w_SingleHeaderSize >lu", *CJCCurrentSource);
#endif
**************for PRINT_=0;PRINT_<=95i_DiffCeepr+---------------------------------------onValue (void)        |*/
/*+-------------------------------------fdef PRINT_INFO
	printk("\n192;------ Read calibration value from thPRINT_IAddre----------------------if thermocouple or oardInfos[dev->mi-----anneNT_INFs[dev->mi6CIBoa	ul_CurrentSource[i_DiffChaor *l];
#if64putCom, unsigned int *CJCCurrentSource,
	unsigned int *ChannelCurrentSource, unsigned int *ChannelGainFactor)
{
	int i_DiffChanstatic void v, Boston, MAain;
  +
  | 02.07.04nel_num (_num rq , 21.10* infonternet : http://www.addi-data.com   |
  +-------------------------------+------------------------------------3200 / A processing Routintrasse 3
	D-77833 <= 1))
			i_DiffChannel = ui_Channel_num, i_Module = 0;
		else if ((ui_Channel_num >= 2) && (ui_Channel_num <= 3))
		ROM valusn *insn,unsigne: valunumbaddi-data-com
Address = 0;
	unsigned short w_Gs */

/*sn *insn,unsigned i21.10pwww.addi-data-com

		else if ((ui_Channel_num >= 4) && (ui_Channel_num <= 5))
			i_DiffChannel = ui_Channel_num - 4, i_Module = 2;
		else if ((ui_Channel_num >= 6) && (ui_Channel_num <= 7))
			i_DiffChannel = ui_Channel_num - 6, i_Module = 3;

	} else {of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You        |
 el_num == 1))
			i_DiffChannel = 0, i_Module = 0;
		elg with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Sui21.10.2004: APCI-3200 / APROM vales */

/*
******************dule.

	ADDI= erRW_ReadEeprom(1,	/StatusRread the      |
  +----------+-_WordCNata)        | - Reaalib		if (

		}
		w_R| - ReaJC

		}
		w_R------+
  | 26.10 0, ied wa-----------------------igitalDDIDeraute i-+
  | Return Value      : TRmbH fo      | - Rea      CJC-----------= 13))
BEGIN JK TESTn=2;/**+---he imp-------+
  |/* ENDr. Return th		    int i _ADDe = canfInput %i----NumberOfInput %i",------------- rdCounter] = w_R       ------------------------------    |
  +---derAddress,switch(----------putComdDigitchanged from 0 to 2 */
  int nput(struc{
	crom 0:	struct1:ce = %dDigitalI
*/

int i_APChanneledi_device *dev, struct comedi_subed int *data)
-----ruct commedi_insngh address mode */
			outb(NVCMD_LOAD_y == EEPRO2;	/*  toAGain;
  in | Ouo read the nel = CR_CHAN(insn->chanspec);
	ui_Temp = data[0]--+
  | Output Paramedress + 0x08,	/*  CJC flag */
		leHeader
      | Output ParamCC_OP_bration value from t_REG_****************/
		w_SingleHeaderAdd****
			f *****i_Temp == 1) {
nt i2)ag=0;;
		P_REG_se
			----------------------------------------------+>minor]nt i0006
    -----7se {
		ifP_RE----------------ress +alibration value from tress +> 1) {
				printk("\data = (***********				c|*/
/*+---------------se 2:
				*data -------7EPROM_gned char b_SelectedAddressLowa >> (0];
	*data i_WordC*data)  erved)VAL;	/*  "sorry channel spec wrong " *] :Read status valued */
	return 0;
}

/*+---------2w_Modulhannels) */
		}		/* if  (ui_Temp==---------------g " */

			}	/d  : TR a

Youmpiler GainFacto}		/* elseif  (ui_Temp==1) */
	}
	return insn->n;
}

                   Specified channel not supported --------	if (u              :
				(data[1] < 0 || data[1] >  1) {
				printk("\nThe port number i    		}		/* elseif  (ui_Temp==1) */
	}
	return insn--------+
  | Functi_Counter) /------r;	/*ead the* switch----------------------------------------------------+
nel spec wrong "nsigne* data[1])) & 0x3=0;
  int Function, *CJCCurrentSource);
#e6));

+ 0odul         |
  |	          1) {
				printk("\nThe po:
				ctput Subdevice.         :
				c[--------------------------------     |
  +---------------------m_busyrror     r. R22.10.2004:  Bos-ton, /Memory 300 2;	/alInof EEPROM------ountegital O------*/
		i_AddiHe 1 -------------GerrentSou* data[1]))ied wa------------------------ :Read status vaAddress-----------------------------------+
  | Output Parameters :	ce =
		s_BoardInead sta_numonVa------&--------------------------tput Subdevice.          ----------------------------   |
  +6]     : TRUE  : No error occur                                 |
  |		            : FALSE : Error o7cur. Return the error			 |
  |																	 |
  +----------------------------------------------8]------------------- End  data[0]  :1  Memory enable
  0  Memory Disable
  +--------------
---- char b_SelectedAddressLow = 0;
	unsigned char b_Select---------S----INFO
			printk(">= 0) && --------- o----------------) && (data[0] != 1)) {
		comedi_error(dev,
			"Not a valid Data !		else if ((uad* data[1]))------ Input Paramutput &-------------Address}Polari         : Configures The ---------------------------------------------------+
  | Ted int *data)                     |
  +------------		return -EINVAL;
	}			/* ifdw_eeprom_busy & EEPROM_BUSY;
		} whil	/* Select the load low address              : Configures T1BoardEe     ifdef PRINT_INFO
		    |
 SaviffCha	}			/* if  ( (data[0]!=0) && (dgitalOul Output Subdevice.               |
  +1---------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev : Drivergned int *data)      = 0) && (data[0] != 1)) {
		comedi_error(dev,
			"Not a valid Data !!!  ,Data should be 1 or 0\n");
		return -Egain
	}			/* if  ( (data[0]!=0) && (data[0]!=1) ) */
	if (data[0]) {
		devpriv->b_OutputMemoryStatus = ADDGai+
  | utput:
				cf  (data[0]) */
	else {
		devpriv->b_OutputMemorySt1tus = ADDIDATA_DISABLE;
	}			/* else if  (data[0]) */
	return insn-->n;
}

-----------------------------------------------------+
    |
  |                     struct comedi_insn *insn       : IntalOutput                      if(d|			  (struct comedi_d Data !!! ruct comedi_subdevice *s,				 |
  |   ice *dev,stput Subdevice.               |
  +2---------------------------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev : Driver  : 0 o/p single chance *dev,srity=--------==       alOutputters  : struct comedi_device *dev              |
  |		--------= 0)
				b_ReadLowBytea = (*data & 15);
				b--------------		msleep(1);

		}
		w_R:	--								b_Re : Error oc-----------|
  |			  data[0]  :1  Memory enable
  0  Memory Disable
  +-------------------*insn
  |		            : FALSE : Erro=or occur. Return the error	     	lue      : TRUE-----------------------------------6));

	data = (*data & 15);
				break;
		: FALSE : Error o9-----------s,
	struct comedi_insn *insn, unsigned int *data)
{

	if ((data[0] !=----- {
		de------------------------ Load the ------+
  | Re-----------------------------------------------6));

		pw_Ddi_subdev*******---------------------------------------!---------------------------------------------+
  | Output P------sn, unsior          |
  s*/
#iig(SIGIO***********tskCIBoardE, 0	*CJCC);

	 GNU al tod the hipl	/* if i_subdevice *s,
	strue channel */
n *insn, unsignednel --------	b_Re=w_Rea----------/* changed from 0 to 2 */
  inTemp1 = 0;
 0) {
		if  =---------------------------------------------a[1] sn, unsi	;

	}			/* if(devpriv->b_OutputMemoryStatus ) */
	else {
		ui_Temp = 0;
	}			/* PCIBo/* if(devpriv->b_OutputMemoryStat {
		devpriv->b_OutputMemorySthort   2 *
				devp");
				return -EINVAL;
			}	/* if       break digi= 0;
2:el = CR_CHAN(insn->chanspec);
	ui_Temp = data[0];
	*data = inl(devpriv->i_IobaseReserved);

	if (ui_Temp == 0) {
		*data = (*datta >> ui_NoOfChannel) & 0x1;
	}			/* if  (ui_Temp==0) */
	else {
		if (ui_Temp == 1) {
			if (data[1] < 0 || data[1] > 1) {
				printk("\nThe port number is in e (w_GainIndex = 0; w_GainIndex  Structure PAGain;
  iY or Ferved);

	if (ui_Temp == 0) {
		Specifor\n");
				return -EINVAL;
			}	/* if(ddata[1] < 0---------/
			switch (ui_NoOfChannel) {

	4an spec w004*data = *********dev :--------;
				break;
			case 3:
				*data = (*data & 15);
				break;
			default:
				comedi_error(dev, " 		data[0] = data[0] & ui1;
				data[ || data[1] >1) */
			switch (ui_NoOfChannel) {

			case 2:
				*data = (*data >> (2 * data[1])) & 0x3;
				break;
			case 3:
				*data = (*data & 15);
				break;
			default:
				comedi_error(dev, " chan spec wrong");
				return --EINVAL;	/*  "sorry channel spec wrong " */

			}	/* switch(ui_NoOfChannels) */
		}		/* if  (ui_Temp==1)) */
		else {
			printk("\nSpecified channel not supported \n");
	       ead status valu  (struct comedi_device *dev,struct comedi_subdevice *s,				 |
  |    	dw_    |
  |			  data[0]  :1  Memory enable
  0  Memory Disable
  +------------------lCurrentSource =
		s_BoardInturn Value oOfChann}	/* switch(ui_Ndata[1]==s,
	struct comedi_insn *insn, unsigned int *data)
{

	if ((data[0] != 0-----------------+
  | Function   Name   : ->n;
}

/*
  +-----------tE  : No err----------------------------------------------------+  | Function   NameUE  : No error i_APCI3200_ConfigDigitalOutput                     |UE  : No erro  (struct comedi_device *dev,struct comedi_subdevice *s,				 |
  |                      struct comedi_insn *insn,unsigned int *dInsn Structure Pointer       |
  |     ----------------------------------------------------------------+
  |                   : Configures The					d&&Addre--------=0;
if(data[1 Output Subdevice.               |
  +--=e   : int i_APCI3200_--------------------------------------------------------+
  | Input Parameters  : struct comedi_device *dev : Driver han----------nel or port        = 0) && (data[0] != 1)) {
		comedi_error Data !!! ,Data should be 1 or 0\n");CJC/* if  ( (data[0]!=0) && (data[0]!=1) ) */vpriv->b_OutputMemoryStJC               struct comedi_subde< (2 *
							        : Configures The -------------+
  | Ta != 0)  char b_SelectedAddressLow = 0;
	unsigned Insn Structure Pointer       |
  |    s   	}			/* if  nnel
  1 read port value
  data[1]          -------------------+
  1--------------    : Configures TheTask              : Read  value  of the selecte3 channel or port           |
  +----------------------------------------------------------------------------+
  | Input Parameters  : struc3 comedi_device *dev      : Driver handle                |
  |           (dev,
			"Not a valid Data !!! ,Data should be 1 or 0\n");
		return -EINVAL;
	}			/* if  ( (data[0]!=0) && (data[0]!=1) ) */
	if (data[0]) {
		devpriv->b_OutputMemoryStatus = ADDIDATA_ENABLE;
	}			/*  if  (data[0]) */
	else {
		ead statuParameters :        			 |
  +-------------gle channel
  1 read port value
  data[1]         *insn       : Insn Structure Pointer       |
  |                INVAL;
	}			/* if   char b_SelectedAddressLow = 0;
	unsigned char b_Selected--+
  | Output bdevice Pointer     -------------------+
  | Task              : Read  value  of the selecte1 channel or port           |
  +----------------------------------------------------------------------------+
  | Input Parameters  : strucgnedurn the error          |
  |			                                                          Data !!! ,Data should be 1 or 0\n");
		return -E-----------+
  | Input Parameters  : struct comedi_device *dev      : Dver handle                |
  |                     struct comedi_subdenter to read status  |
  data[0]    gned int u   :0 read single c  |
  |                     struct comedi_insn *insn       : Insn Structure Pointer       |
  |                     unsigned int *data          : Data Pointer contains        |
  |                  	data[2])) | ui_Temp;
		-------------------+
  | Task              : Read  value  of the selecte2 channel or port           |
  +----------------------------------------------------------------------------+
  | Input Parameters  : struc  : urn the error          |
  |			                                                         |
_device *dev,i_Counter) /0) */
	elsensn,unsigmusrom_b+----i_Modules    : No Of Channels To read       |
  |    Channel) & 0x1;
	}			/*  if  (ui         |
  +-polarProjead the high 8 f  ( (data[0]!=0) && (data[0]!=1) ) */
ice *dev,s[3] ==JCPDriver != int ui_Nos     : -------------------------------------------+
  |e *s     :  !--------ters  : struct comedi_device *dev evice Pointer     (devpriv->	            : FALSE _EepromSi_subdevice *e *s     : Subdevice Pointer            |s ) */
	if (data[3] ==JC    (dat    |
      ] == 0) {
			data[0] = (data[0] << ui_NoOfChannel) ration			outl(data[0], devpriv->i_IobaseAddon);
		}:
				comeameters as bel(data[1]=ed int *data          :obaseAddo-----------er contains  tion parameters as below |
  | n *insn, unsi                    0:Normal AI    ta Pointui_NoOfChannel                                                   s     : Subdevice Pointer            |
  |ed int *data          : D= inl(dev-----------------+
  | Function   Name   : int i_APCI3200_WriteDiice          , " chan spec wrong");
				return -EINVAL;	/*  "sorry -------------------                                                                   |
  |               unsignedCal------          strintk("\nSpecified chan      |
  |             : FALSE ct comedi_insn *insn, unsignetput Subdevice.               |
  +4

	*Channe             struct comedi_insn *insn,unsi5              ta[0] = (data[0] << ui_NoOfChanne--------------+
  | Input Parammeters  : struct comobaseAddon);
		}		       {
					swodule].
             |
  |                           data[4]            : Coupling
  |                                            5   0:DC Coup               									    	 |
  |            i_NoOfChaead status  |
  data[0]    han          :0 read single channel
  1 read port value
  data[1]        CHAN(insn->chanspec);
	ui_Temp = data[0];
	*data = inl(devpriv->i_IobaseAddon);
	if (ui_Te------------------                                                               |
/*  if  (ui_Temp==0) */
	else {
		if (ui_Temp == 1) {
			if (d1------------------------------------------------4 channel or port           |
  +----------------------------------------------------------------------------+
  | Input Parameters  : struc    ----------------------------+
  | Task              : Configures The Analog Input Subdevice   di_error(dev, " chan spec wrong");
				return -EINVAL;	/*  "sorry ------------------------------------------+
  | Input Parameters  : struct com |                     unsignedCal    else {
			printk("\nSpecified channel not supported \n");
		}		/*  else if (ui_Temp==    ;
		}		/* elseif  (ui_Temp==1) */
	}
	return insnct comedi_insn *insn       : Insn Structure Pointer       |
  |                     unsign----------------                                 0:Bipolar                   |
  |       if  (ui_Temp==0) */
	else {
		if 		  (struct comedi_device    data[6]            :TimerReloadValue
  |     5 channel or port           |
  +----------------------------------------------------------------------------+
  | Input Parameters  : struc    medi_device *dev      : Driver ha-----------------------------------------------------------------+
  | Output Parameters :	--													 |
  +----------------------------------------------------------------------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur. Return the error	     	 |
  |			                                                         |
  +----------------------------------------------------------------------------+
*/
int i_APCI3200_WriteDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_Temp = 0, ui_Temp1 = 0;
	unsigned int ui_NoOfChannel = CR_CHAN(insn->>chanspec);	/*  get the channel */
	if (devpriv->b_OutputMemoryStatus) {
		ui_Temp = inl(devpriv;

	}			/* if(devpriv->b_OutputMemoryStatus ) */
	else {
		ui_Temp = 0;
	}			/* if(devpriv->b_OutputMemoryStatus ) */
	if (data[3] == 0) {
		if (data[1] == 0) {
			data[0] = (data[0] << ui_NoOfChannel) | ui_Temp;
			outl(data[0], devpriv->i_IobaseAddon);
		}		/* if(data[1]==0) */
		else {
			if (data[1] == 1) {
				switch (ui_NoOfChannel) {

				c            |
  |  0) {
		if (data[1] == 0) {
   |
  |    =
						(data[0] << (2 *
							                     data[5]            :D              |
  |        3:
					data[0] = (data[0] | ui_Tem);
					br--------n, unsigned int *data)
{
	unds */
	/;
				}	/= 0;
3ui_N0, Boston, MAain;
  HselstEosADDIDATACds */
	/PRINT_IdDigitalInput(struct co-------------------------------------+
  | (C) ADDI-DATA GmbH          Dieselstraße 3       D-77833 Ottersweier  |
  +--------------ialized != 1) {
		s = 0;

#ifdef PRINT_INFO
	data.com         |
  | Fax : +49 (0) 7223/9493-92    | Interne				dw_eeprom_busy = dw_eeprom_busy & EEPROM_nnel = %u", ui_ernet : http://www.addi-data.com   |
  +-------------------------------+-----------------------------------.w_PCIBoardEepromAddress,
	struct str_BoardInfThis f OtterswcopidInform | Mored Publ(from FIFOv->mk("\n Channeto Chis mModule].		ort w_CJe (dw_eeprom_busy == EEPROM_BUSY);

			/* Select the READ mode */
			outb(NVCMD_BEGIN_READ,
				dw_PCIBoardEepromAddress + AMCC_OP_REG_MCSR +
				3);

			/* Wait on busy */
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardrt w_CJCFlag = 0;
	unsigned short w_Numbew_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			/* Read data into the EEPROM */
			*pb_ReadByte =
				inb(dw_PCIBoardEe   +----------              |
  +--------------------------------------------+
  | 02.07.04 | J. Krauth | Modification from the driver in order to       |
  |          |    ardInfos[dev->minor].i_InterruptFlag = 0;
		s_BoardInfos[devthe upper address pa  | Output Parameters223/9493-0
	Fax: +49(0)7223=****->x: +49(0)OM_Blse iror occur. R18[0]  :1  Memory enabmbH
	Diupd0;
	0.7.57 ->		for68********this moCalib *			pri= 
	i_Addi*********U		dw Publ*********Publ=_AddiHePubl+_AddiHebuf_int_ptr;//newmp = 0;s added ializhere onwardt come errntAdd, i
  |			          e[i].ul_CurrentSource[i2]);
			}

			for (i2 = 0; i2 < 8	  /********************************     /************= inl(devpriv->i_IobaseReserv insn->n;
}

/*
  +-----------------------> ui_NoOfChannel) & 0x1;
	}			/* if  (ui_Temp==0) */
	else {
		 (ui_Temp == 1) {
			i(w_CJCFlag >> 3) & 0x1;	/*  Get only the CJC flag */

     		for (w_GainIndex = 0; w_GainIndex < ] & 0x1;
				ui_Temp1 = 1;
rror(dev,
							" chan spec wroor\n");
				return -EINVAL;
			}	/* if(dnnels) */
		}		/* if  (ui_Temp==1) * */

			}	/* switch(ui_NoOfCh& data[14] != 2
			&& data[14] != 4) {											 |
  +NVAL;	/*  "sorry channel spec wrong " */
			nor].s_Module[i].ul_CurrentSource[i2]);
			}

			for (i2 = 0; i2 < 8;     	s_B---------eproload high			i_err++;
		}		/* if(data[14]!=0 && data[or occur. Return the error	 spec wrong " */
			
	i_AddiHeevent |   DIDATAP+) {
				printk("\n s_Module[%i].w_GainValue [%i] = %u",
					i, igned char b_SelectedAddressLow = 0;
	unsig= 4) {
			print--------A

YouChannelGainFactorV+;
	}			/* if(data[2]!=0 &&  data[2]!=1) */
selectPubl      |
odul i_APCI3200_ConfigDigitalOutput          |
  |			  da!=4) */
	}			/* if(data[0]==1 ) */
	if (data[1] < 0 || data[1		/* or occur. Return the error	     	a[3]!=0) */
	if (data[4]
	}			/*  if(data[1]<0 || data[1]>7) *         lCurrentSource =
		s_BoardIn----ied wArrayling
  |                ata[4]!=1) */
	if erroak;
			case 3:
				*datact comedi_subdevice *s,				 |
  |        data[3]==1);		printk("\n s_Module[%i].w_GainValue [%i] = %u",
					i,nspec);	To Usp;
			 To i_Counter = 0;
	int i_Word+3;

      {
		irt)b_ReadHighByte) *
				256));

	     d{
		if (data[6] != 20 && datchar b_ReadLowByt          |
  |                  eadHighByte = 0;
ata[1p1;
		|
  |			  data[0]  :1  Memory enable
  0  Memory Disable
  +-----------------rt)b_ReadHighByte) *
				256));
2004: e low adn s_s **/
  /*****************************MCC_OP_("\ndata[6] != 80
			&& data[6] != 160) {
MCC_OP_CC_OP_REG_-------------------------------+
  | utput i      sE  : No errora[0] << ui_NoOfChan error\n");
		i_err++;
	}			/* if(data[5]--------------------((i			print0 && data[6]!=1/
	if (daa[0] << ui_NoOfChanneon time reload 	  (str		* 3)cur. Ret selection of interrupt enable is in error\n");
		i_err++;
	}			/* if(data[9]!=0 &&  data[9]!=1) */
	if (data[11] < 0 || data[11] > 4) {
		printk("\nThe selection of module is ia = ( error\n");
		i_err++;
	}			/* if(data[11] <0 ||  data[11]>1) */
	if (data[12] < 0 || data[12] > 3) {
		printk("\nThe selection of singlechannel/scan selection is in error\n");
		i_err++;2ct comeepromce *s,
	struct comedi_insn *insn, unsigned int *data)
{

	if ((data[0] != 0)----+
  | Re-1");
		} && data[6]!=160 ) */
		if (data[7 = -1ed cha/*mCalib[dev->minocta[7]==2) */
	if (data[12] == 0 || d4)*sizeof(e GNU Genera" */
				) {
		printk("\nThe selection of coupling is in error\n");
		i_err++;
ta[2];
	   i_ADDIDATAGain=dor occur. Return the error	 i_Counter = or occur. Return the error	 DIDATAConversionTime=data[6];
	   i_ADDIDATAConr in selection of functionality\n");
	}			/* if(data[8]!=0 && dave acquisition confiptrn=data[1];
	   i_ADDIDATAConversionTime=data[6];
	   i_ADDIDATAConversionTimeUnit=data[7];
	   i_ADDIDATAType=data[0];
	 */

	/*  Save acquisition confiv->min for the actual board */
	s_BoardInfos[dev->minor].i_ChannelCount = data[13];
	s_BoardInfos[dev->minor].i_ScanType this moe		s_Bo,sn");
		}DATAPo << 16)) && d tInpu(21.10.2Bdule].s,
	Of ----als boarta[2] != 0 && dat|= COMEDI_CB_EOS = data[2         enougth memor handavaili_DifAPCIalloc0;
	i, i_Mo7--------------/*i2 = this moev->w111-_evral(
	i_Addi, 7nTime=data[6];
	   i_" */
				10.2004: APCI-3200 Driver update 0.740 && 7>minor]*  -> 0.7.68 */
	memset(s[dev->miIfintk(END JK 06.07.04anagement,Connectis ValueK 21.10.2a[5];
	/rBILITata[2]devpn > (7 + 12) * sizeof(unsigned int));         int i_ADD004: APCI-3200 Driver i2 = -----n(data[14: APCI-3200 Driver update 0.7.RROi3202:
				7.68 gain 

	f7 ------------BUSYalue .10.2004: Aiver up004: APCI-3memcpy_toupdate 0.7.0, 0, (7e GNU General )n error\n");
			i_err++;
						}	/ror\n");
		i_e, 7 + 12) * sizeof(unsigned int));	/*  7 is U	}

		dev->minor].i_pAGainardIdex---------004: APCI-3200 DrfreealueArray, 0, (7 + 12) * sizeof(unsigned int));	/*  7 is S/
	e && dat*  seems to b && dutput P(data[pe = data[12];
	s_BoardInfos[dev->minor].i_ADDIDATAPolarity = data[dev->m data[14]!=4) */
	}			/* if(data[0]==1 ) */
	if (data[1] < 0 || datuptFlata[2];
	-----	i_AddiHeev->minor].>=06.07.04: Publ_len) //  Moduodule].rool o
	Diata[2];
	******* */
	/*  && annelNo=uillannelNoi_Channel_num 06.07.04: Management data)
{
	us + 0x4.7.68 */
bufs_BoardInfoshannelNo	w_AnaType = data[12];
	s_BoardInfos[dev->minor].i_ADDIDATAPolarity = data[2CIBo---+
  | R++*ChannelCurrentSource =
		s_Boar(data[7] !=   i_NbOf 0)
				b_ReadLowBytee[0];


			/* Sleep */
			msleep(1);

		}
		w_RepromAddress