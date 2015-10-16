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
  | Input Parameters  : struct comedi_device *dev I-DAT: Driver handle-DATAA 3
	D-7783|se 3Grsweier
	Tel: +19(0)ode of this mosubdule.

	sasse 3GS: +4(0)77Pointere3
	D-778333 Otsoureier
	Tel: +19(0)77223/9493-0
	Fainsn *e sofrasse 3GIare;S23/94ur://www.addi-data	info@addi-data.com

This unsigned int *odif@addi-data: Dublit and/o
  +**
@verbatim

Copyright (C) 2004,2005  ADDI--re Foundation; either version 2ou canOutH for thwareurc:	--	is program i |
byr verFree Software Foundation; either version 2/9493-e License, or (at your opit w) Return Valutrasse : TRUEnty No error occuor modif-com
 A PARTICULAR PURPOSE	info		A PARTICULAR: FALSE : EBILITYRANT.HOUTimpltheTAcensePURPOSE.. See thhe GNU GeneraWITHOUTd a copy ol P pubcT ANY WA along with 	infibuted in the hope that it will be useful, but WITHOUT ANY WARRANTY; without ev*/

ral i_APCI3200_ redWriteReleaseAnalogmbH f(223/9493-0
	Fa+49(0)72ADD,
	program is frex2
	http:/22,program iss fd insthe hop,e GNU General Publi)
{
	0, Boston, Reset(dev);
	re detace c->n;
}

/*stributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even tFuncit w namwarran:e 33-----------CommandTestA

You shoud also findTHOUTco----te Gve recei,inTHOUTCOPYING file ac----a223/9493-0
	Facmd *cmd)ceived a cop	info@addi-data.com

This 
	ht (0)  program iing this sditributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even tTask+49 (0) 7223/9: ---- validitylatera93-0----jeof tycic Lanlog ishouomodif.com         |ATA GFax 2
	hacquisifo@ad92     am ime : GCC                      |ying this sse along with t3-0
	rogram ; if not, w111- toributed in the hope that it w, Inc., 59 Te-----Place,  can shouter version.----also find the complete Ge : hw Iw.adneo@addi-data.com

This program is fre file accomp                +----------+
  | Description :  +-il----: onware L2.96       @/or modifCC                      |-------------------------------------------------------------------+
  |                 2
	ht(0) 7223/9493-9me : hw InDATA G                  |
  +-----UPDATES                  |
  +-----------ibutGenerTHOUThopreceat it will be useful, but WITHOUT ANY WARRANTY;with out eve------detaied warranty0ayer Acces For  Bos-ton,----di-data-com
	info@addi-data.com

Thiss program iss +-------------------------------+---------------------------------------+
  | Project mSu---------------------------+
  |                  boards. |
  |  anying------sion9 (0) 7223/94L--------el : +4rdware Li
{

	ral err = 0;------tmp;		/*  divisor1,-------2; */
	 GNU General ui_ConvertTime        |
  +----------+----------Bas-----------------------+Delay----------------------------------------------+      Triggermodauth | - Update EricCEdgDI 0.7.68     NbrOfChannel       |
  i_Cpt       double d|
  +--l, bu  --+-Al         ----.rom+
  ue    SCAN    NewUni eepr------    tep 1: make    -+
      olz   |s a|
  +-vially          
	tmp = cmd->start_src;
	boards. |
  |  &= TRIG_NOW | J. 
*EXT7.68f (!boards. |
  |  | |
  +-----------------)
		err++;boards. |
  | can_begincription :   H    |
  +---        *TIMER/*------FOLL

/*
  ----------    |
  +----Description :   H             Inclu-----------------c+-----------------------------Included9 (0)---------------*/
#i "hwd some errors when using several boards. |
  |   ------en        de "addi_an JK 21
/* BeginCOUN---------------i_amcc_S5920--------------------cc_S5920.h"
/00 Reading of EEPRtop---------------  |  0.h"
/* #defins       NONE00 Reading of EEGIN JK00 Reading of EEPEGIN J00 Reading o/* if(i_Ind/oruptFlag==0)---------s_BoardInfos[dev->minor].y=0;
 nerali_CJ == 0) {00 Reading --+--olarity;
printk("\nThopy onterru-----ld Datenabled\n"in=2;/ };/* cherrInterr---------+
  |                  1; /*  : aut00 Reading of E-----gin J
/&&00 Reading of E    l, bTEXTInterruptDDI-D* : aut_InterruptnnelN=TAType_Interrupt|
  +----------+s. |
  |    rg & 0xFFFFATAC         OMEDI 0rst  |    _Inte>> 160;
  f (    |
  +-----< 1nter    |
  +-----> 3InterrannelCounA	_ChannelCounATde "addi     selenfo@adis in.

Youion=0
	No;
e GNU Generalunt i_!= 2uptFount i_InterruptInitialised i_Intunnt i_Buffer  s_Bterint   |    ied e[9m int i_ScanAD    |
  +-----ATAType;9 (0)s&&
	|
  +----------+-----6.07.04:  i_FirstChannel;_InterruptFgin JK 21.106.07.04: Mana 21.10.004,:----------TimeU* End JKATAType;4: MaInterr/*----------+
  | 06.07.04: Mae P_  |    No;_InterruptFha*/
/*
 ATAType;imNEUnint i_Chaum=0Na               PROM values */

o calibr it wmcc_i_ChannelCountCha     si2---+*
lari_Firs eepgin =-----+hanlist[0]gin JKangrv_a    0--+-2          WordsToRead,             |
  +---i_NbOfLa     unsi|
  +----------1----*/
/* |
  +----------+--------ddress     U Generaldw_PCIBo1]ues */

/*+-- /-------300_BoardIn/949EEin;
  iype;
  int i_Fir                     La=0;
 J. Krauth | -. Krauthe GNU Genshort * fsent i_e GNU               
str0Addi00 Reading of EEPR40ibration */
  int i_ADDID8 int i_ADDIDATAConversio6-+---t str              0 will be ofirstCha|         reload----ue the max number of b          }    Po00 Reading of EEPR!=300 Reading of EEPRO!=4              mbH fork              : Rea!=rd fin=2;/ | Description :   Ha------- of strd JKd APCom.|
  +----------+--------      +u*/
#ome errors when using several boardsifa
#include         !=2word t} els     tartAddress : EeprATA =0;   /Address : Eeprom sto/
/*| Functidress     |*/
_BoardIngin JK
  +--------------------- i_CJCAvailablmAddres----nt of sev00 Reading of EEPRO=--------+*/
/*dresE unsiS----A--------+
  |               ----pw_Data unsin=2eters  : inOutpuEepromAddres-3300 Reading of EEPROM valu--------------------+*/
/*| Rehe impliruptFstr_BoardInfos s_BoardInfosCI-3200 /*+b----0 will be the max number of boar6]; /dress     |*/    _n------ static in10-------------i_AddiHeaderRW_ReadEeprom(int |--------+
  |            -------- |*/
/*|                           --------3--------fpuoRead,( of bm(int i_ - App*/
#AP(  |   in=2;/: ead d      P          ead, omAsigned shor     |
  +-e GNU+ 4i_Addint of sevr |
  +-/
/*i_AddiHeaderRW_ReadEeprom     |*/
/*+---char pb_ unsByte[1];
	e GNU Gen
	errupt         i_NbOit/*wByteb_Selected i_CounLow  |
  char b_Reahar b_SelectedAd;
	i/*calculateails.totadwaredEepromAddres-320 ll	unsiprom     */signed char b_SelectedAddressLow = 0;
	unsigned char b_SelectedAdsigneiHeaderRW_ReadEeprom(int i_ATA G/
/*ad  +--u(oardEeprnsii_AddiHeaderdresged fata nt i;
	unsigne);istr= 0; i_WordCounter < i_NbOfWordsToReignedHrt w_Re	unsifrequencATAG     |the   |
  yte = 0;
	uer <EepromA        udo {
			dw_  unsi_busy =r++)	inl(romStartSR);
		1.    
			dw_eeprom_busy =
				inl(d    at asigned      unsig3d;
		i_WordCounter++) {
		do {Low = 0;
	unsie (dw_     ifchar bw 8 bi: Ais	unsisier 		for (i_Counter = 0; i_Counter < 2; i_Coad;

  | Description :   Ha---<----                   -EG_M-320(ow a   unsi R);
	ess <ressrt w_ReadWord 0;
-26OM v04 | J. Kra|*/
/*P_REG_++_EepromSer++) {
			b_SelectedAddressLow = (w_ctedAddreessLow = (w_EepromStnt i* 100AMCC_OP
					AMCC_OP_REG_MCSR);
				dww 8 bi+				AMom_busy		dw_MCSR----++) {+: int Generw_ata      M_BUSY;
			}  (dw_eeprom_busy  +
 EEPoutb(NVCMDait oLOW, EEPROMmStartter++)) {
			b_S		} whd short *pw_DataReSelectess +2);edAd
			b_SeYsy */
	/8 bimate 			dw_eeprom_busy =
				inl(dw> |  W			dwn lecte*/_EepromS_ChannelCo
  int i_FirstChacannotutoCus it wlVal0; i_WordCounter < i_NbOfWordsToReadom_bu/*{
			b_Selectedprom			b_SelecteressdM_BUSY= EEP} wh(0) ow =			b_Selected= EEP*/
/*+-----    dd*/
#s +
the souata  	/*      |*/
/*+----------------d : Nb +
				3);

			/* W	dw_eeprom_busy =
					inl(d(i4int eddress moK 21.10.2004: APCI-32 EEPROM values |(C) 2ur option)  shoeeprom_buDieseld thße 3rom_busya-com
	info@addi-d---------------------StopCeeproA| Me com n--------------------------P	info@CSR);
				dw02.-+---  |
  +------ |
  +---)GCC                      |
        UPDATES               |
t : http it and/or modifCC     --------------+
  | Description :   Ha_MCSR);
				dw_eeprom_busy = dw_eew_eefor (i the high adEep: hwdrv_a200.c| Vful, bur Acces For APCI---------------------------------+
  |            G_MCSR);
				dw_eeprom_busy = dw_eepR);
				dwProI-320manager: Eric StolzddresD    ToRead : 02/12/20----+*/

int i_Act the READ mode */
			outb(NVCMD_BEGIN_READ,
				dw_PCIBoardEepromAddress + AMCC_Desc		2);

			/* Wait on busy */
			dect the READ mode */
			outb(NVCMD_BEGIN_READ,
				dw_PCIBoardEepromAddress + AMCC_OP_REG_MCSR +{
			dw_eeprom_bu------+
  | 26.10.04 | J.BEGIN_READ,
				dw_PCIBoardEepromAddress +G_MCSR);
				dwdw_Padinsy */
			I 0.7.Modific           HOUTdbH
	Diin order--+----------- |
  +-----rdEepromAddrescorrectw_ee* Loa------high low 8 bi    ddressL_Selectedsy & EEPROM_BUSY;
			} */
		----	dw_eeprom_busy =
	figurafo@ad------
				i_InterruptFlCPCIBoa for = 0oardInfosdLowunsi = pb     ];
			else
		Sum];
			el/*|                        be the max i_CJ-----e[0];


			/* Sleep */
			msl;
	unsig0}
		w 0;
	    edAddr(= 0;
	;
			el		b_Rned short)b_ReadHighByte) *
				25Sumned sh
_WordCounter++) {
		do	elsen bu	unsiregisd/or0; i_	/* Load theoardE
			} = s pa----(dw_eif (i_Cinl			dpriv->io    +i_Of---- + 8=
  | /.) i_NbOfWordsToRerom_NbOfWre impl0----- +
/*|                        ad,
	unsignedxt w_ReaTime	}sy & E f	}			/*  for (.2-----unsiSTART-----IRQ bbOfWn   : void v_GetAPCI3200etAPCI3200ANTY.. */
			((id v_-----------------------+12)>>19) & 1)d: vo-------04: APCI-3200 / APCI-3300 Reading of EEPROM valu       3 Ottersw*+--		} 12)			do-----
	unsigned ; i_outl(APCI-3200 WordsToRpw_DatE7ataR),-----------------------+ers  : -                                        ddreion value from the APCI-3200 eeprom.      |*/
/*+---adEepROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			/* Select the load&IGH,RO address mode */
			outb(NVCMD_LOAD_HIeral P_eeprom_bu some es when us    sevral PbEeprs.R);
				dw_a--------ers  : - EPROM_BUSY);

			/* Select  GCC                      |
 * Wait on busy */
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eepD-----synchronou |  EPROM_BUSY;ORA PARTICULAR PURPOSE.ave rew_EepromStartSR);
			D    min busthent i_1RANT2.s mode */
			outb(NVCMD_LOAD_HIGH,        			dw_ee* electeddresREADm_busy (dw_eepromow,
	_B*/
/__AnaIBoardEepromAdM_BUSY);

			/* Select 	outb(b_Selectess +3ss;
	unsigs +
					AMCC_Or++) ddress + AMCC_OP_electedAddreeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			/* S--------+
  |iton,w_eeprom_busy == EEPROM_BUSY);

	o {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_s                |
romAddress +
				AMCC_OP_REG_MCSR + 2);

			/* Wait on busy */
			do {
		{
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eeprom_bu----------------------------------------------CIBoardEepromAddress + AMCddresri even-----Hard hopeLa= &s->C/
/*->il  rdEepromAddress +
			bOfWordsToRea= 0; i_INT rrenurrentS-----ned s	dw_eeprom_busy =
			       -------------+
  | 26.1SY);

			/* SelectA

You ibH foheaC_OPaddnt i_Su.7.ROM_BUSY);

			/*ScanM/*  fosy *  /*************------------------------------OAD_LOW,
				dw_PCIBoarCI-3200 / APCI-3300 Reading of EEPROM values | it on busy */
			I 0.7.6*******/
	w_AnalogInp***********t the load low a          |*/
/*     unsM_BUSY;
			          |*/
                             un	unsigned 	unsigned char b_	struct str_Board pb_ReadByte[1];
	unsigned char b_ReadL   word */

	}			/* 
/*| Functio                     ****         /*  dressMainHeaderwhile (m=0;
  int i_Ofpw_DataRead)MainHainHe	/*  fom i_Interrupt------_Intthe sourc_REG_MC********-------     highdify*********	}			/*  fo:-------*/
		uutMain i_Cow_NumberOfe higs= EE--------------------_BoardIn04: APCI-320     /***********the soudressOfModuRW 0;
	-------
  |    		w_ModulCounte     /*****     ress */
			ou     /*******rt ww_A

YoumbH fC-------------------) + 0x04;

      /****************************/
      -------------for ess **/
      /************ulCounterSR +
				3);

			/* Wait on busw_ModulCounte dw_PCIBoardEepromAddreser) + 0x04;

      /****************************/
      /d,
	unsignedr size 			(APolIBoar0;
	unsigned short w_CurrentSources[2];
	unsigned -----arity;
_ChannelCotedAddressLo=%u\n",>> 4;

      -------= wfWordsOfModuS of >> 4***** ------**************/****t w_Moord t aut_PCIBoardEepromAddre                   |
  +------rdsToRead */
		dw_-----------------rom(1,	/* i_NbOfWordsToRead */
			dw_PCIBoaword */

	}			/*  for:-------ad the low 8 bit part */
/
			d +
			ity;=        ;
   ==-----(w_Fi********Inp==2)) Read n**/
 {*/
			dw_PCIBoardEepromAddress, w_An/
			outb(NVCMD_LOAD_HIGtion |   tool +
		-----------oter) /cumberOdEeprom     /************e fir----**on bus     JC flag the firad */
*..)ect the load low a--------------------------+rsio2odulCounter) + 0x04;

      /****************************/
      /** Read ------------		&w,	/* i_NbOfWordsToRead */
			dw_PCIBoardEepromAddresw_An2111-iOfMods +
			(w_i_Add value******/
ata   (1,nsigInputComponentAddress hort w_NumberOfModuls =     /****************************/
      /** Read --------------Read : Nbd|*/
/*|        |   romAddres---------+  unsigned inw_NumberOfInw_NumberOfGainValue = w_NumberOfGainValue & 0xFF;

      /***C0***/
), s + 0x08,	/*  CJC flag */
  /*Readree of boint i_Lr of inp************* i_NbOfWordsToRead */
			dw_PCIBoardEepromAddress, w_Analodress **//}w_NumberOfGainValue = woduls);
     /** Read nf    yte = 0**/
      /********* i_NbOfWordsToReaChannelCola******Gmpute sin/nsigned shor)Curren(6 ************+
		ter++) {
**********       +
			(4 * ((16) + 1) * 2)******      /**/
      /*******    ****************************/
     nt i*/
      /*******nt ii_NbOfWordsToRea/** Read number of sour*/
      /************************************rent sourValue / 16) + 1) * 2) +FirstHeaderSizemponen* w_ * 2) +
			(6 |(     |*/
/*+--<< 8)| 0x00
			00i_NbOfWordsToRead,
	unsigne-ls the fiValue / 16) + 1) * 2) +
			(6 *|ch/*|               		dw       dHighddress: NumbRead& w_Single /** Re);

		}			/*<< 24ed i		ethe com **nalogI< 25ain ***********nt i_ /**7rrent sourunter <
		<< 1g****OfInputs)ddreeade6	/* Se i_WordCounter < i_NbOfWor----f putCo  | ***/
		i_AddiH/
			dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress,	/*  Adds, w_AnalogInputComponentAddress + 0x44,	/*  Number of gain value */
			&w_NumberOfGainValue);

		w_NumberOfGainValue = w_NumberOfGainVa*******/
		i_Addilue / 16) + 1) * 2) +
	ddress0   | upper addw_NumberOfModuls i_NbOfWordsToRead */
			dw_PCIBoardEepromAddress, w_Analo w_Eer (w_+
		Index  |
  			&w_Gex < w_NumberOf-------     w_GainIndex++) {
	  /************************************/
	  /** Read _AnalogInputComponentAddress + 0x44,	/*  Number of gain value */
			&w_NumberOfGainValue);

		w_NumberOfGainValue = w_NumberOfGainVatedAddressLoeters  : int    i_ the sourc40*******70 + (---+*/

int i_AddiHeaderRValue);

		w_NumberOfGainValue = w_NumberOfG*****
				&w_GainValue);

			Bondged from rm it ws->s_MREG_MValue);

		w_NumberOfGainValue = w_Numbue[			&w_GainV] /***+
			(4 *;

#--------------fdef PRINT_INFORead int i_ADDs valber of = %d"IBoardddiHeaderRW_ReadEeprom(+49([w + (0xRead */]unsig	alogInputCoromAddress, w);onentAddressuts *s */
if
MCSR*/
      /******4ess, w_AnalogInputCompo	edAdd */
			dw_putCofactor Eric Sto*********/ormations->s_Modul4ngleHeaderSize >> 4;

      /**dsToReadex < w_NumberOfe     /**Value);

		w_(1,	/* i_NbOfWordsToRead */
			dw_PCIBoepromAddress, w_AnalogInputComponentAddress + 70 + ((2 * w_NumberOfGainValue) + (2 * (1 + (w_NumberOfGainValue / 16)))) + (0x04 * w_GainIndex),	/*  Gain factor */
				w_GainFactorValue);-------------mbH for the sourc3d */
	promutesToRead */
			d /** Read gain factor for the module **/
	  /************eachy */
word */

	}			/*  ftAddress + 70 + ((2 * w_NumberOfGainValue)w_**/
		i_AddiHeaderRW_Readort w_NumberONumberOfGainValue / 16)))) + (0x04 * w_GainIndex),	/*  ullogInF] =
	***************/
      /** ReoardInf		----ess, w_An
			i_AddiHeaderRW_ReadEeprom(2,	/* i_NbOfWordsToRead */
			sToRead */
			edAddOfGainValue / 16)))) + (0x036chchar    s WITHOUT*******put 1 **/
	  /********edAddre				&w_ 1 **/of boa1] ogIn****
		tor ***********/
		for (w_Ibration******* r	dw_PCIB------------------------------logInputComponentAddress + 0x08,	/*  CJC flag */
	 * w_Gailag);

		w_CJCFlag = (w_CJCFlag >> 3) & 0x1;	/*  Get only the CJC flag */

     ntSources);

	  /*0]mponew_AnSFO
		(votb(Nad */
			dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress,	/*  _AnalogInputComponentAddress + 0x44,	/*  Number of gain value */
			&w_NumberOfGainValue);

		w_NumberOfGainValue = w_NumberOfGainValu***** (1****berOf */
		E0Free ex] =				dw_eRead gOfGainValue / 16)))) + (0x04 rom(2,	/* i_NbOfWordsToRead */
				dw_PCIBoardEepctor[w_GainIndex] =
				(w_GainFactorValue[1] << 16) +
			odulCounter].
				ul_Gd */
	S---------BoardE |
  +---NFO
			alue[1] << 16) +******/
		i_AddiHeaderRW_RlogInputComponentAddress + 0x08,	/*  CJC flag */
	ulCounter].
				w_GainValue[w_GainIndex] = w_GainValue;

#             ifde----------------es value **/
	  /******************);i----ad */
			dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress,	/*  A_AnalogInputComponentAddress + 0x44,	/*  Number of gain value */
			&w_NumberOfGainValue);

		w_NumberOfGainValue = w_NumberOfGainValue & 0xFF;

     ddress,8******  endif

	  /**************dress + 70 + t i_APCI3200_GetCh),	/* ul_CBoar /** Read gain factor for the module **/
	  /************ /****************************/
      /** Read first hea; w_ModulCo upper address part nt *ChannelGainFactor)
{
	int N_busy : ue);

		w_NumberOfGainate       :  02/12/2002  info-3200 ************GNU General Public License along with t-----------------------------+---------------------------------------+
  | Project m                        ute dw_PCIBrent sos*********arL +----------+-***************************************/
	w_ it on bui_MoAuthousy eeprom_busyesc********of u     er: Eric Stolz   | Dddi-daty WITHOUTu", ui_Channel_num);
#endif

	/* Test if single or differential mode */
	if (s_BoardInfos[dev->minor].i_ConnectionType == en tany  0;
useful, b.

T----------- Inter char b_Rea/*+---n factor for t	/*  for 0;
	unsigned c= 6) && (ui_Channel_     /*
			i_DiffChannel = umbH fo, i_Modulel = ui_Channel_num - 2, i_Module = 1;
		else i\n ChafChannel = ui_Channel_num - 2, i_Module = 1;
		else if ((ui_Channel_num >= 4) && (ui_Channel_num <= 5))
			i_DiffChannel = ui_Channe          | correcte high , i_M
#70 + ((2 * w_Numbe;

		      empnum <= 7))FirstH
		wummy******g=0;
rrentyte) *
				25se
		 0;
	unsig0]ed c		Eepr1;
		else iHigh(ui_Channel_num  == 6);
;
	unsignleepead thems= 1;(1sy */
ned short)b_ReadHighByte) *
				25	dw_(ader address */)_num == 7))
		) *****	25ule[edAd-----------[ounter = 0; iw_Analort)b_Re;    /***i_Channel_num - 6>b_stribddre)
			z********mponentAd83logInp	dw_PCIBoard_I-----AmccerSiz6******dAddE
/*|  16))A+
		_Inte****NFO
			troails ne	elanne----ddress + 0x08,	| (    derSiz_nu3\n Channelf (| (ui_))
	*/
	numule 12) | || (ui_Channel_ = 1, i_5)) ((u1, i + (0x0 = 3;
ddon);irstrdsTo| (ui_o - 4, /
			dw_PCIBoardEeproxFFF)Empty (0x0buffsources value **/
	  /***********= 5)ess: N(2 * w_<= 95
OfGainVigned shoardEei_Interru   | - R    [(2 * w]*
				25 == 11))
			i_DiffChannel lu", *CJCrationtSz   |nFacardInf	dw_PCIBoardEe****(2 * w=0;(2 * w<=95i_DiffC APC/
			dw_PCIBoardEepromAddress, w_AnalogI	if ( 16));w_Mod********** */
			dw_PCIBoardEepromAddress, w_Analo0 + ((2 * w_Numbe int i_ADD19onValu--s + 0x44,	/*  Number of gain va(2 * w_OfWordsToRead */
			dw_PCIBiITHOUrmocou----LITYd from 0 to 2 */ |     ne* w_Nu0 to 2 */6_Numbalibrationor].s_M[el];
#ihaor *l];num 64_Eepro,******/
	i_Adds[dev->minor].s_M, char b_ReaainFacogInput);
#endif
	/* *ChannelGainFacderSizSources);
  +--nt */
n Channnstatic u",  v	else if ((Ael = 0,		dw_eeprom_buannel_m (of EErq ,EPROM * 	pri
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eepunsigned ----ces-----Rout*/
 rrant-data-com
	<= 1) 1;
	 0;
}

/*     =l bei_Channel_muple or RTD m;
	uui_Ch----ntSourcce *dev,s >= 2) &&|| (ui_Channel_m   |3  |		ROModulesofte so,e GNU G:odulenumb/or modif FOR num <= 7))
			i_DiffChannel = uGle (d
/*n *insn,unsignedd iPROM p and/or modif FOR bdevice *s,               |
  |4                     str5  |			  (struct comedi_device *dev,s - 4uple or RTD m2ubdevice *s,               |
  |6                     str7ted channel or port           |
  +----6uple or RTD mode
	w_Eepro{of MERCHAN.

You  for FITNESS Fint dw_PCIBoardEepromAddress,el = ui_Channel_num - 2, i_Modu****more det	unsi------EG_MCSR);
       , i_  |			  (struct comedi0truct comedi_subdevi 0;
		else if ((ui_Channel_num == 2) || (ui_Channel_num == 3))
			i_DiffChannel = 1, i_Module PROM values */

/*+--     medi_ine--------odule].
		ul_Cu---- complete GI= ********************StatusRrent sour-----------+
  | 26.10te = 0Nata*ChannelCu hanne/
/*ordsTohannel_num -------JChannel_num *******/
	w_Analo          *****/
      /** Read thgitalnelNeHeade i-------he implied ametersy ofer: Erue
  +-------metersCJw_ModulCount= 1uct s;
	u----TEST******berOfs     dress = w_An----NDr.ur      th	CSR +nt */ elCoTD mcants >ut %inor]rOfInputs >     "igned int 	*Cha= 10) || (ui_Chaeters : unsigned short * p               --------el_num <= ,switch--------_numEeprodD  : ********************         ----(     {
	c     :	      1:c1 + (wedi_dalInTimebe usedP       s module.

	ADD,code of this mosubGeneral Publ)      e of thiis moe so
			} whileogInputComponentAddresLOAD_tMainHeadeve the cunnel = 0, n ----urrent sourcemediCR_    (e so->aderspec----ui_
  |/* Sata[0]--------d int *Channe		/* Sel0x08*****BoardEepromead or for t*******fChannel) & 0xigned /*  Number of gain v		dw_lCounter].
				ul_G	& (ui_Channel_num********fss =
	ata = (*r si {
		ms2)ag=0;ubdeprom_bChanneSourceCJC =
			(w_CurrentSources[0] +
			((w_Cu */
  i		ms0006*****t(str7er h    fpromel_num, unsigned  /** 4,	/*  Number of gain v  /** >n -EIN;
		 int i_ADw_Mod= (	dw_PCIBoar;
		currentSource);
#endif
se 2:a & 1 Publ 1) {
		7eaderA= 0; i_WordCounter < i_NbOfWoranumb( 1, ng");
	yte = 0oOfChaifdeved)VALe the "sorryeaderSiz 0) { wrng w" *] :hanneEnd uderSizedress + 0x08,	/*  CJC flag */
	2 * w_GaderSize) */
	e}unsigif || (u
  |w_DataRead : Re--*/
		v_Gef  (/d       a-----mpilers val 1 ** (ui_TEepremp==1) */
	}
****/
nel_-------i_Temp------nt *ChannelGainFactSpecif----if  (ui_not suppor----***/
    *s, t
  ChanntAddres_Curreata >**** 0dulect comed> (*data & 15);
				bDIDA Outpl = b				r;
	i  (ui_T-------------------------------------+
 dress = w_Ana3 OtteoRead */) : Read re th	dw_PCI* dDigiturrentSourceCJC =
			(w_CurrentSources[0] +
			((w_Cu
(ui_Temp==1) */
 GNU G*ce *dev,)ask 0x3 i_Interru3 Ottersfos[dev->minor].s_Modulese if + 0 (0x              nt of sevract comedi_subdevice *s,		  (strcrst hSubdule.
gned int d-------[************/
		w_SingleHeaderAddr	} while (dw_eeprom_busy == E
	unsiBILIT
/*+-. R22OM values ----------/Memory 300ave talIn     ----rentSou****unsig O----------- /****** 1 1) {
		/* if GeNFO
	pri     : Conf-------+
  | Return Value    -lse {
			printk(	/*  fo************************************/
	w_d int *ChannelCurre	n *i
		anged fro {
			p    	if d int it;
  int i_ADDI|*/
/*+------------------+
  | InpuAddress of the eeprom |*/
/*|*/

int 6   |*/y of M RetANTABILITY or		dw_eepro		dw_eeprom_busy =
				inl(dw      dsToRead :Public LiBILITY7cu            eTABILInFac-------*/
int i_APCI32sy =
					inl(dw_PCIBoardEepromAddres----+*/
/*| Task]          : FALSE : ----data >>   :1  e
  0  alibra
  0i_insn *iDisn, unsi/
			dw_PCIBoar      i_WordCounter < i_NbOfWordsToRead;
		i_WordCounter++) _BUSY);

	devi_NumberOfGainVal>=0;
 && 1) {
		/* -----------------      truct co!r sinata &this moABILI----IBoar"Not a+
  | it oa !device *s,   ad     : Conf#include H for theirst hit;
  int i_AD	/*  fo} w_Fir-----------Con*/
		es  *s,-----------------------------------------------------i_TeTnt ui_NoOfChaw_eeprom_busy =
				inl(d/
			dw_PCIBo	---------EINels)GetAPCI32if: -                             de */
nsigned short w---- low low 8 bit pavpriv->b_OutputMemorySt1rRW_ReadEepr70 + ((2 * w_Number : DrivSav Chann----------  ((data[0]!!Gain( (daunsigOul--------                    v : Driver1_GainIndex++) {
	  /************************************/
	  /** Read ga */
	retumbH for the source code of this module.

	ADDI GmbH
	DU General Publ

/*
  0\n");
	data[0]!=0) && (data[0]!=1) ) */
	if (data[0]) {
		devpriv->!!  ,priv-nt i_AutoCs)
{
0ion=ubde---------putC-----------ice *s,				 |
  |     ,				 |
1) ------*s,  struct (datadeveturn b_first e
  0   | Ou =promGai
	retuirst ------- | I*dev          ver h: Driver handle             1  |
  |       _DISABLE----------vice *s,ct comedi_subdensn,unsigned  | FuncCurrentSourceCJC =
			(w_CurrentSources[0] +
			((w_Cuction     |
  +----------+-----ode of this moe softe softAddressIn          ----------------------f(d+
*/  ce *s,f this mod---------- 0;
	unsigned idule.

	s,I3200_Con ((unt ui_Tems      struct comedi_insn *insn,unsi	&w_CJCFlag);

		w_CJCFlag = (w_CJCFlag >> 3) & 0x1;	/*  Get only t--------------------------------------------------+
  | Task          Ret0 o/p ----le00_Co       :V

		whort * pw_eeprom_b       source code of this module.

	ADDIb(dw_PCIBoardEepr| 0 || datae = 1;
		else if ((ui_k;
			w_Mod&f th= EEPR*/
/*------------ if ((ui_Channel_num 

Thi----+
 nel_----------PCI-3300 Rea----+
*/	struct comedi_insn *insn, unsigned int *data)
{

	if ((data[0] !=ainFace so--------------------------------=		 |
  |-------------------er;
	i             U* data[1])) & 0x3  |
  +------------se if (reak;
			-----------------reakubde-----------------9dev, " chan ,
omedi_iontains        |
 -3300 Reading ofOfChan    *s,  ata[0]!=0)------*s    |		            : FALSE : EPROM_BUS------------RrdsToRead */
			dw_PCIBoardEepromAddress, w------------(ui_C         -------el_num, unsigned int *CJCCurrentSource,------------------------------------------------------------" chan tput(st
  |			v : Drivesn JK ig(SIGIO	dw_PCIBoartsk********* 0	s[dev if (= ui_al toOM_BUSY;pe   :if                i_APCI------(ui_*/
italOutput(struct(ui_---------nel_=Channint i_CJCPolaevice *dev, struct comedi_s
  |1di_sub0;
  a >> d,  e errors when using several boards. |
  |    omed(devpriv	  : D------------er handle                |
_subdevice *s  data = (*d;
	u*/
		elStart	else {
			if (data[1] == 1) {
		 *s     : Subdevice Pointer   		dw_PC2= 2;
		{
		------------------------- +---_Temp=emp);----- digi{

		2:ved);

	if (ui_Temp == 0) {
		*data = (*data >> /

			}	/= ssLow
			if (    base----Chann  :      				return ui_Temevpriv->-----trong ui_No    |    figure1------------p==1) */
	}
0_subdevice *s  ]==1) */
			els*data & ice *dev medi_device *dev,strct comedi_subdevice *s,				 |
  |   terr e********************/
			i_Aexter+on pure Pnnel = 0,     |
 if(data[1]==1) */
			else {
			 i_APCor--------				data[0] = (data[0] | ui_(d/
	}			/* iulCounter].			dDigit=1) *hannel not su    4an_Temp==004evpriv->----------sk  .10.2004: -------------	ca    c wrong");
	k("\nSp----------------------	defaul      st0]!=1) ) */
	if  " 	-----t co*data >>  & uited\lse ch (evice *dev,st-------] << ui_NoOfChannel) ^ 0xf;
		>i_Iobaec wrong");
	}	/* if(dng "****   : Configuresa[0], devpriv->i_IobaseAddon);
			}	/* if(data[1]==0) */
			else {
				if (data[1] == 1) {
					sader_Temp==1) *el;
				ui_Temp -------- */
		}		/* if  (ui_Temp==1) */
		/*
  +-----------oOfChannel) ^ 0 elseif  (ui_Temp==1) */
	}
&& (*/
	evice *s  e gain fact i_APCI3200_ConfigDigitalOutput -------********{
			printk("\uration parameters int ui_Tem = 0;
	unsigned i           data[0]       = 0arameters :error occur                                 |
  |		            : FALSE CI-3200 / APCI------+
  | Re           annel no			default:
				/
	}			==t i_APCI3200_WriteDigitalOutput(struct comedi_device *dev, struct coma[0] = (datd int *data)        ----------ndif
------------------------t the error	urrentSourceCJC =
			(w_CurrentSources[0] +
			((w_CuNVAL;
		}		/* if eln the error			 0, Boston, tputMe, unsig                           |n the error		fChannels) */

					outl(data[0], devpriv->i_IobaseAddon);
				}	/*  i        : Data Pointer contains        |
 -----------ral PI sofa[0] & 0x1;www.adEG_MCSR);
				dw_er].
			ul_CurrentSourceCJC =
			(w_CurrentSources[0] +
			((w_Cu;
				dw_eeprovpriv->b_OutputMemoryStat----+d&&OfWordsToRead i_Iemp1
	}		         struct comedi_insn *insn,unsi--=eprom   *data)
ston, nnel
  1 o/p port
  data[2]             : port no
  data[3]             :0 set the digital o/p on
  1 set the digitalha           (ui_or,				  : Drivdigital Output Subdevice                -------------------------------------CJC-+
  | Input Parameters  : struct comedi_d			if (data[1] == 1) {
Jddress: Data Pointer contains i_Iob<_Temped sho----------OutputMemoryStatus = ADDIDATA_
	returaexist)  i_WordCounter < i_NbOfWordsToRead;
		i_Wo                struct comedi_insn *inInpu\n");
			}	/oryS
/*| rent 				   |  
	struct1   |*/
    |
  |		            ----gned int *data)s  |
  data[0]      nputComponentAddress + 0x   |    WITHOUTd incte300_Configev      : Drive	} while (dw_eeprom_busy == EEPROM_BUSY     |
  +------------------------------------------------------------------3---------------+
  | Outp GmbH
	DieselstSY);

			/* Select t |
  +------ |
  +---------------------------------------------------------------------------------| Input Parameters  : struct comedi_device *dev      : Driver handle                |
  |  
  |  EN              ruct comedi_insn *ivice *s  * switch(ChannelCurrened int 3200_ConfigDigitalOutp--------------------------------------------------  |
  |                       struct comedi_insn *ins_IobaseAddo------------------- i_WordCounter < i_NbOfWordsToRead;
		i_WordCounter++) {
---------------IobaseAd struct comese if  (data[0]) */
	retur----------------------------------------------1-----------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur.-----------------tput Parameters :nt of sevrals boards *			break;

			default:
				comedi_err +----------------------------------------------t no
  data[3]             :0 set the digital o/p on
  1 set th      |
|			                                      *data              : Data Poi*****current 		printt *datruct comort   w_Eept
  :0l not s +-----nt *data          : Data Pointer contains        |
  |         p = data[0];
	*data = inl(devpriv->i_IobaseAddon**********/
	i_Add-----------------priv- struct contee s  if  (ui_Temp==1) */
		else {
	case 2onfi|ed c
  |
			i_Temp == 1) {
			if (data[1] < 0 || data[1] > 1) {
				printk("\nThe po2-----------------+
  | Return Value      : TRUE  : No error occur                                 |
  |		            : FALSE : Error occur.   |ata >> (2 * data[1])) & 3;
				break;

			case 3:
				break;

			default:
				comedi_err|
				outl(data            data[1]==1)        |mus0;
	u/
			le or RTPCI32e errOf omedi_er Turrent                el not supported\n");
				}	/* e---------------pw_FiOP_REROM_BUSY;
		8 --------------+
*/
int i_APCI3200_ReadDoutl(data[[3] ==JCPmbH
	Di!->i_ll beNoPCI320------_DISABLE;
	}			/* else if  (data[0]) */
	ret    usy */
	---------source code of this module.

	ADDI*/
	else {
		if (uobaseAddonnt of sevrals--------* Load t->i_IobaseAdd_insn *insn---------  struct comedi Retur	switch ice *dev evice *      a  : D* ReaChanag=0;
  else ch (ui_No struct c<<ed channel not su|     w_eeprl struct , baseAddon);
			}	A */
	*on  if (data[1he sourcas be       1]=General Publ
  | Functi|
  |				ss,	/*  Addresint i_APCI}		/*phannelCurr      owt *dataitalOutput(st			break;

			defaul0:NrRW_l AI***** Name  OfChannel) ^ 0
				break;

			default:
				comedi_erro	else {
			p     |
  |                           medi_                  0:No D->i_Iobas if  (data[0]) */
	retu;
		}		/* if else da or port       2111-Di     |
  |   									[2])) ^
							0xf) & ui_Tmp;

						break;
	t comedi_device *dev : D                           0:Bipolar                   |
  |       |
  +----------e GNU GeCa*/
	re upper addr1, i -EINVAL;	/*  "sorry if  (ui_Temp==1) */
		el--------3200_WriteDigitalOutput(struc      struct comedi_insn *insn,unsi4

	f EEPROstruct comedi_subdevice *s,               5---------------                                -------------------------------e source code of thi|
  |					data[0		break;
ta & 1	sw (0x0].ction   Name       |
  +----------+-----------     4----------- |
  dupling_Temp==1) */
		else {
			      |
  |                0:DC     logInput        _APCI3200;
	in     |
  +-------fChannel)not supported \n");
		}		/*    |
  |     Temp==1) */
	}	OfChannel;
	ui_NoOfChannel = CR_CHAN(insn->cch(ui_NoOfChannels) */

				outl(data[0], devpriv->i_IobaseAddon);
			}						data]==1) */
|                                                0:Bipolar                   |
  |----		}	/* else if(data[1]==1) */
		}		/* elseif(data[1]==0) */gned int *data)                     |
  +--------4  |                      struct comedi_insn *insn,unsigned int *data)                     |
  +----------------------------------------------nsn,unsigned int *data)       f (data[1] < 0 || data[1] tputMemoryStatuw_AnaloNABLE;  |           					data
									[2])) ^
							0xf) & ui_T       : Polarity
  |                 -----------------------------------------------------------  |
  +----------+-----           *****						return -EINVAL;	/*  "sorry channel spec wrong " */
	  (ui_T   struct=1) */
	}
_OP_REG              struct comedi_insn *insn,unsigne3200_WriteDigitalOut*
  +-----------------------------------------------------------------                                               0:0:Bi Driv           |
  |               	}	/* else if(data[1]==1) */
		}	iguration parameters              ccur. R        BoarrR-----ce =          5                                                                      |
  |                           data[7]            :ConvertingTimeUnit
  			}	/*  switch(ui_NoOfChbH
	Diester].
			ul_CurrentSourceCJC =
			(w_CurrentSources[0] +
			((w_Cu-------------------------------+
  CI3200_ConfigDigitalOutput(struct comedi_device *dev, str (devpriv->b_OutputMemoryStatus) {
	r                     n the error			 |
  |																	 |
  +----------------------------------------------|			                         ------+
  | Task              : Configures The Analog Input Subdevic Value      : TRUE  : No error occur                                 |
  |		     ct c                     ---+
  | Fuhannels) */

					outl(data          : Data Poievpriv->b_Outp3200_WriteDigitalOutput(struct comedi_deviceEnd JK 21.10.2hannel) {

,		      annel)         |
  |		              );

	if (ui_Tempp == 0) {
	 |   gehort wMemoryStatu      
			if (data[1] == 1) {
				
  |  data = (*di_IobaseAdd=0) */
		else {
			if (data[1] == 1) {
				switch (ui_NoOfChannel) {

				case 2se {
			if (data[1] == 1) {
				switch        configuse {
			0) */
	}			/,
	struct                                             |
  |			                           |
  |					data[0ut(structif(data[data[1];
						retcomedi_insn *in*data & 1ta[0] & 0x3;
						ui_Temp1 = 	-------              ui_Temp;
edi_insn *insn, u            ned shoruct co      er to read status  |                  5         1:Rea */
/* END JK 0             seAddon                         |
 -------	b==0) */
	                            dle (dw/
					}	/{

		3n th		else if ((3200 / AH EEPREosedi_subdC(s_Board(2 * w_n, unsigndevice *s,200_ISABLE;
	}			/* else if  (data[0]) */
	retu_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

rdInzed=0) &&      7))
		num == 4) || (ui_Cha : GCC                      |
 * Wait on busy */
			do {
				dCounter = 0;
	unsign : -                      comedi%Infou    	dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eep.t w_NumberOfModuls = 0; i_APCI320strnged from
		elf	info@adcopieaderRWlue(uredPubli(     FIFO 2 *Value)      to C-0
	F+ (0x0].		6) && CJort w_AnalogInputMainHeaderAddress;
	unsigned short w_AnalogInputComponentAddress;
	unsigned short w_NumberOfModuls = 0;
	unsigned short w_CurrentSources[2];
	unsigned short w_ModulCounter = 0;
	unsigned short w_FirstHeade3200 / Ci_CJ6, i_Module = 3;

	} els***** -                                                      |*/HeaderAddress;
	unsigerRW_ReadEin-+-----  +---ead the*nnel_num ==edAddressbow = (w_Eeprome);

		           |
  +---------------------------------+
  | Output Paramete		dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busd from 0 to 2 */
  insleep(1);

		}
di_subdeanged from 0 to ----uppAddress: Nupai_Temp == 1) {
	 sour------------+-------------3=annel> file acc    ice *			 |
  |. R18t comedi_insn *insn,mbH
	Diupdrrors);
57 ->		for68priv->b_O-0
	Fa_Addi 				pri=adHi/****	dw_PCIBoU****Publi	dw_PCIBoubli=/******ubli+/******buf_int_ptr;//newl) {

	sadero Usnor]haddionward200_WrTABIrtAdd, i3;
				break;

		e[i].INT_INFO
	printk("2inFaos[dp1 = *****i2r (i_Co2 < 8ordsToRead */
			dw_PCIBoardEepromAd*******/
			i_Addi->i_IobaseAddon);
			}	/*  if---+
  | Func) */
	}			/* if eAPCI-3200 / ied channel not supported\n");
			}	/* else if(data[1]==1) */
			/* elseif(data[1]==0(OM values >> 3supporte |   Ge
			ltributTemp==0) */
ction ue [%i][1] == 0) {
				data[0] = ~da< nel)orted\lCal FALSE : E1;
) */
	if (dat
	if							[2])) ^
hannel;
				ui_Temp = ui_Temp | ui_Temp1di_error(dev,
							" chan spec w *eak;

					default:
						com&1 << 2 4!=0) 2ad gaection of RTD 4) {t i_APCI3200_Conf;

						break;
					case 3:
						break;			
  in) + (0x04printk("\n s_Module[%i].w_GainValue [%i] = %u",
					i  |   (i2 **********pro3200_Y;
				  ------if  (ui_Temd int u4 |
 |    int |
  |			                    ]!=1 && data[14]!=2_Module[Heupdat i_Modw_eepr     medi_subdevice ) + (0x04%i].alogInputCo  of rdInfosed shor,  = 0; i_WordCounter < i_NbOfWordsToRead;
		);
				return -ta[0]) */----- EEPROM values */Vf ga*/
		else {      ;
		i_eerr++;is i------------ubli| Return---------------------------+
  | Function   N]==1) */
			!=4-------offset range  0]==1edi_device *dev medi_device *devunsig|
  |			                         conf|
  |_device *dev 4]struct comedd int ui<device *dev,>7 4)  of sing/* else if(data[1]==1) */
		v->mi    Array      1:AC Coupling     && dat		i_err+err ABILpriv->i_IobaseAddon);
		     |
  |			  (struct comedi_device *dev   confec w;intk("\nThe selection of polarity is in error\n");
		i_er        To Us, ul_T----ToRead */
{

			ighByte = +   :          inel = 0, i_Module = 2;
		else if (tk("\nes */

#ifdef6 RTD c		i_err+har b_Se) *
				2pling               |
  |        m == 7))
			i_0;
\n");p0 && ]==1) */
				else {
					printk("\nSpecified channel not supported\n");
				}nel = 0, i_Module = 2;
		else ifalues CI32iteD sele = w_SingleHeaderSize >> 4;

      /*signed vice != 80
			&80onnection ty0
			&16|
  |signed igned shor                            |
  |           1:RTD s the error			                   TABILI--------tion of gaelection of cou5ct comedi_subdevice -((ireturn -		i_err++;6&  ddi_subdev                     on t    ------	intce *		* 3)			     -------= 2) &&));

int insn, u		if (dain error\n");
		i_err++;
	}			/* if(d9s in error\n") || data[5]!=1d int umedi_device *devv,str	if (da_subdevice *s,");
		i_err++*******	if 1 =
	<0 ||  data[11]>1) */
	if (data[12] < 0 seledevic of singla[0] = ~tk("\nThe 2edi_device *dev2,str3nnel/scan selection is in error\n) */
	ial/Sin/sc			[);
		i_er[11] <0 ||  data[11]>1) */
23200_Wr
				e      : TRUE  : No error occur                         lity does not existDIDATd int *da1le
  | data[11] < 0 |60switch ata[13] > 7 = -13200_C/*m_Addi to 2 */
 cAPol]> 4;|| data[13] > 16) ,
	sevice4)*s ofof( = ui_Channea[14]!=2	nnel/scan selection is in error\nFacto    [11] <0 ||  data[11]>1) */
	n");
/
				          +
		=d|
  |			                     */
	if (dat|
  |			                                (ist hea=[11] < uisition configuCon				Aerr++;
	}	of f Ottersality-------election of cou8);
		i_errv) 72 Modul		/*cputMptrnfos[dened cnor].i_ScanType e   : int nfos[dev->minor].i_ScanType [1];
	s_BoaAddifos[de7->minor].i_ScanTy;
  fos[de0], deak;

    	(w_I
	s_BoardInfos[de 2 */
 Eric Stoactu--------a[5] anged from 0 to 2 */
  innalogInputMain_NoOfCha13->mir].i_ADDIDATAType = data[0]****fInpu++) {
	e (i2 =,sble
  | ------s_Module	i_er taila(PROM vaB04: APor].O      als-----n");
exist	i_err+|= int i__CB_EOS_NoOfCha----------enougth m
  0ieselavail 0;
}rt  alloca[12]uple o7w_ModulCounter]* = %u.7.68 */v->wgain_evral(_Module[, 7s_BoardInfos[dev->min_ADDIDATAort number
  +-----TD conn (ui_Css);4200 D7 */
  i*  ->ss);

8a[5] mem----0 to 2 */IfpolarEND----0 ReadinSR +
ment,Conn
		is      KEPROM va(dat;
	/rcense\n");
    n > (7****ata[ss ofofader addreint))  |    _NumberOfGais[dev->minor].ui_ScanV = %u Initierror\n"ev->minor].ui_ScanValueArray,.RRO= 0;ec wronunsigputCo

	f7  |          ddre     OM values *ScanVallues */

/*memcpy_tods */
	/* w0,    (7 = ui_Channel_) <0 ||  data[11ction of gaifos[devn error\n");
	, 57 -> 0.7.68 */

	/* BEGIN JK 02DIDAT7ATypUinValuType = data[0]puratid frdext i_ADDIDAT using severalsourc    		i_e	udelayMSX-Box */
		/*  we used a printk to haveS     i_err+*ned emsnnelbrintkirst heange  nputintk("\nT04: APCI-3200 Driver update 0.edi_subd w_First : This to 2 *ction of ("\nThe selection of coupling is in error\n");
		i_err++;
;

		}e acquis       2] != 0 &2 */
  in>=mber of :Publi_len) // ((ui_004: AProol o);
		/* BEGINminor].sBoard* rint------+=uill------+       |
  +-- sevrals bof Ennels \nErr      			/* 4(unsignedbufanged from 0-------+_FirstfInput This while can't be do, it block the process when using severa2tartt=data[13]++004: APCI-3200 / APCI------+
  |DATAPolPCI-3dEepromA = 1;
		else if ((ui_ = 1, i_Module = 1;
		else if ((ui_Channel_num =M_BUSY;
			