/*
 * Copyright (c) 2007-2008 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "../80211core/cprecomp.h"
#include "hpani.h"
#include "hpusb.h"
#include "otus.ini"

extern const u32_t zcFwImage[];
extern const u32_t zcFwImageSize;
extern const u32_t zcDKFwImage[];
extern const u32_t zcDKFwImageSize;
extern const u32_t zcFwImageSPI[];
extern const u32_t zcFwImageSPISize;

#ifdef ZM_OTUS_LINUX_PHASE_2
extern const u32_t zcFwBufImage[];
extern const u32_t zcFwBufImageSize;
extern const u32_t zcP2FwImage[];
extern const u32_t zcP2FwImageSize;
#endif
extern void zfInitCmdQueue(zdev_t* dev);
extern u16_t zfIssueCmd(zdev_t* dev, u32_t* cmd, u16_t cmdLen,
        u16_t src, u8_t* buf);
extern void zfIdlRsp(zdev_t* dev, u32_t* rsp, u16_t rspLen);
extern u16_t zfDelayWriteInternalReg(zdev_t* dev, u32_t addr, u32_t val);
extern u16_t zfFlushDelayWrite(zdev_t* dev);
extern void zfUsbInit(zdev_t* dev);
extern u16_t zfFirmwareDownload(zdev_t* dev, u32_t* fw, u32_t len, u32_t offset);
extern u16_t zfFirmwareDownloadNotJump(zdev_t* dev, u32_t* fw, u32_t len, u32_t offset);
extern void zfUsbFree(zdev_t* dev);
extern u16_t zfCwmIsExtChanBusy(u32_t ctlBusy, u32_t extBusy);
extern void zfCoreCwmBusy(zdev_t* dev, u16_t busy);

/* Prototypes */
void zfInitRf(zdev_t* dev, u32_t frequency);
void zfInitPhy(zdev_t* dev, u32_t frequency, u8_t bw40);
void zfInitMac(zdev_t* dev);

void zfSetPowerCalTable(zdev_t* dev, u32_t frequency, u8_t bw40, u8_t extOffset);
void zfInitPowerCal(zdev_t* dev);

#ifdef ZM_DRV_INIT_USB_MODE
void zfInitUsbMode(zdev_t* dev);
u16_t zfHpUsbReset(zdev_t* dev);
#endif

/* Bank 0 1 2 3 5 6 7 */
void zfSetRfRegs(zdev_t* dev, u32_t frequency);
/* Bank 4 */
void zfSetBank4AndPowerTable(zdev_t* dev, u32_t frequency, u8_t bw40,
        u8_t extOffset);
/* Get param for turnoffdyn */
void zfGetHwTurnOffdynParam(zdev_t* dev,
                            u32_t frequency, u8_t bw40, u8_t extOffset,
                            int* delta_slope_coeff_exp,
                            int* delta_slope_coeff_man,
                            int* delta_slope_coeff_exp_shgi,
                            int* delta_slope_coeff_man_shgi);

void zfSelAdcClk(zdev_t* dev, u8_t bw40, u32_t frequency);
u32_t zfHpEchoCommand(zdev_t* dev, u32_t value);



#define zm_hp_priv(x) (((struct zsHpPriv*)wd->hpPrivate)->x)
static struct zsHpPriv zgHpPriv;

#define ZM_FIRMWARE_WLAN_ADDR           0x200000
#define ZM_FIRMWARE_SPI_ADDR      0x114000
/* 0: real chip     1: FPGA test */
#define ZM_FPGA_PHY  0

#define reg_write(addr, val) zfDelayWriteInternalReg(dev, addr+0x1bc000, val)
#define zm_min(A, B) ((A>B)? B:A)


/******************** Intialization ********************/
u16_t zfHpInit(zdev_t* dev, u32_t frequency)
{
    u16_t ret;
    zmw_get_wlan_dev(dev);

    /* Initializa HAL Plus private variables */
    wd->hpPrivate = &zgHpPriv;

    ((struct zsHpPriv*)wd->hpPrivate)->halCapability = ZM_HP_CAP_11N;

    ((struct zsHpPriv*)wd->hpPrivate)->hwFrequency = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->hwBw40 = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->hwExtOffset = 0;

    ((struct zsHpPriv*)wd->hpPrivate)->disableDfsCh = 0;

    ((struct zsHpPriv*)wd->hpPrivate)->ledMode[0] = 1;
    ((struct zsHpPriv*)wd->hpPrivate)->ledMode[1] = 1;
    ((struct zsHpPriv*)wd->hpPrivate)->strongRSSI = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->rxStrongRSSI = 0;

    ((struct zsHpPriv*)wd->hpPrivate)->slotType = 1;
    ((struct zsHpPriv*)wd->hpPrivate)->aggPktNum = 0x10000a;

    ((struct zsHpPriv*)wd->hpPrivate)->eepromImageIndex = 0;


    ((struct zsHpPriv*)wd->hpPrivate)->eepromImageRdReq     = 0;
#ifdef ZM_OTUS_RX_STREAM_MODE
    ((struct zsHpPriv*)wd->hpPrivate)->remainBuf = NULL;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxRemainLen = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxPktLen = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxPadLen = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxTransferLen = 0;
#endif

    ((struct zsHpPriv*)wd->hpPrivate)->enableBBHeavyClip = 1;
    ((struct zsHpPriv*)wd->hpPrivate)->hwBBHeavyClip     = 1; // force enable 8107
    ((struct zsHpPriv*)wd->hpPrivate)->doBBHeavyClip     = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->setValueHeavyClip = 0;


    /* Initialize driver core */
    zfInitCmdQueue(dev);

    /* Initialize USB */
    zfUsbInit(dev);

#if ZM_SW_LOOP_BACK != 1

    /* TODO : [Download FW] */
    if (wd->modeMDKEnable)
    {
        /* download the MDK firmware */
        if ((ret = zfFirmwareDownload(dev, (u32_t*)zcDKFwImage,
                (u32_t)zcDKFwImageSize, ZM_FIRMWARE_WLAN_ADDR)) != ZM_SUCCESS)
        {
            /* TODO : exception handling */
            //return 1;
        }
    }
    else
    {
    #ifndef ZM_OTUS_LINUX_PHASE_2
        /* download the normal firmware */
        if ((ret = zfFirmwareDownload(dev, (u32_t*)zcFwImage,
                (u32_t)zcFwImageSize, ZM_FIRMWARE_WLAN_ADDR)) != ZM_SUCCESS)
        {
            /* TODO : exception handling */
            //return 1;
        }
    #else

        // 1-PH fw: ReadMac() store some global variable
        if ((ret = zfFirmwareDownloadNotJump(dev, (u32_t*)zcFwBufImage,
                (u32_t)zcFwBufImageSize, 0x102800)) != ZM_SUCCESS)
        {
            DbgPrint("Dl zcFwBufImage failed!");
        }

        zfwSleep(dev, 1000);

        if ((ret = zfFirmwareDownload(dev, (u32_t*)zcFwImage,
                (u32_t)zcFwImageSize, ZM_FIRMWARE_WLAN_ADDR)) != ZM_SUCCESS)
        {
            DbgPrint("Dl zcFwBufImage failed!");
        }
    #endif
    }
#endif

#ifdef ZM_DRV_INIT_USB_MODE
    /* Init USB Mode */
    zfInitUsbMode(dev);

    /* Do the USB Reset */
    zfHpUsbReset(dev);
#endif

/* Register setting */
/* ZM_DRIVER_MODEL_TYPE_MDK
 *  1=>for MDK, disable init RF, PHY, and MAC,
 *  0=>normal init
 */
//#if ((ZM_SW_LOOP_BACK != 1) && (ZM_DRIVER_MODEL_TYPE_MDK !=1))
#if ZM_SW_LOOP_BACK != 1
    if(!wd->modeMDKEnable)
    {
        /* Init MAC */
        zfInitMac(dev);

    #if ZM_FW_LOOP_BACK != 1
        /* Init PHY */
        zfInitPhy(dev, frequency, 0);

        /* Init RF */
        zfInitRf(dev, frequency);

        #if ZM_FPGA_PHY == 0
        /* BringUp issue */
        //zfDelayWriteInternalReg(dev, 0x9800+0x1bc000, 0x10000007);
        //zfFlushDelayWrite(dev);
        #endif

    #endif /* end of ZM_FW_LOOP_BACK != 1 */
    }
#endif /* end of ((ZM_SW_LOOP_BACK != 1) && (ZM_DRIVER_MODEL_TYPE_MDK !=1)) */

    zfHpEchoCommand(dev, 0xAABBCCDD);

    return 0;
}


u16_t zfHpReinit(zdev_t* dev, u32_t frequency)
{
    u16_t ret;
    zmw_get_wlan_dev(dev);

    ((struct zsHpPriv*)wd->hpPrivate)->halReInit = 1;

    ((struct zsHpPriv*)wd->hpPrivate)->strongRSSI = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->rxStrongRSSI = 0;

#ifdef ZM_OTUS_RX_STREAM_MODE
    if (((struct zsHpPriv*)wd->hpPrivate)->remainBuf != NULL)
    {
        zfwBufFree(dev, ((struct zsHpPriv*)wd->hpPrivate)->remainBuf, 0);
    }
    ((struct zsHpPriv*)wd->hpPrivate)->remainBuf = NULL;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxRemainLen = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxPktLen = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxPadLen = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxTransferLen = 0;
#endif

    zfInitCmdQueue(dev);
    zfCoreReinit(dev);

    #ifndef ZM_OTUS_LINUX_PHASE_2
    /* Download firmware */
    if ((ret = zfFirmwareDownload(dev, (u32_t*)zcFwImage,
            (u32_t)zcFwImageSize, ZM_FIRMWARE_WLAN_ADDR)) != ZM_SUCCESS)
    {
        /* TODO : exception handling */
        //return 1;
    }
    #else
    if ((ret = zfFirmwareDownload(dev, (u32_t*)zcP2FwImage,
            (u32_t)zcP2FwImageSize, ZM_FIRMWARE_WLAN_ADDR)) != ZM_SUCCESS)
    {
        /* TODO : exception handling */
        //return 1;
    }
    #endif

#ifdef ZM_DRV_INIT_USB_MODE
    /* Init USB Mode */
    zfInitUsbMode(dev);

    /* Do the USB Reset */
    zfHpUsbReset(dev);
#endif

    /* Init MAC */
    zfInitMac(dev);

    /* Init PHY */
    zfInitPhy(dev, frequency, 0);
    /* Init RF */
    zfInitRf(dev, frequency);

    #if ZM_FPGA_PHY == 0
    /* BringUp issue */
    //zfDelayWriteInternalReg(dev, 0x9800+0x1bc000, 0x10000007);
    //zfFlushDelayWrite(dev);
    #endif

    zfHpEchoCommand(dev, 0xAABBCCDD);

    return 0;
}


u16_t zfHpRelease(zdev_t* dev)
{
    /* Free USB resource */
    zfUsbFree(dev);

    return 0;
}

/* MDK mode setting for dontRetransmit */
void zfHpConfigFM(zdev_t* dev, u32_t RxMaxSize, u32_t DontRetransmit)
{
    u32_t cmd[3];
    u16_t ret;

    cmd[0] = 8 | (ZM_CMD_CONFIG << 8);
    cmd[1] = RxMaxSize;          /* zgRxMaxSize */
    cmd[2] = DontRetransmit;     /* zgDontRetransmit */

    ret = zfIssueCmd(dev, cmd, 12, ZM_OID_INTERNAL_WRITE, 0);
}

const u8_t zcXpdToPd[16] =
{
 /* 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF */
    0x2, 0x2, 0x2, 0x1, 0x2, 0x2, 0x6, 0x2, 0x2, 0x3, 0x7, 0x2, 0xB, 0x2, 0x2, 0x2
};

/******************** RF and PHY ********************/

void zfInitPhy(zdev_t* dev,  u32_t frequency, u8_t bw40)
{
    u16_t i, j, k;
    u16_t entries;
    u16_t modesIndex = 0;
    u16_t freqIndex = 0;
    u32_t tmp, tmp1;
    struct zsHpPriv* hpPriv;

    u32_t eepromBoardData[15][6] = {
    /* Register   A-20        A-20/40     G-20/40     G-20        G-Turbo    */
        {0x9964,    0,      0,      0,      0,      0},
        {0x9960,    0,      0,      0,      0,      0},
        {0xb960,    0,      0,      0,      0,      0},
        {0x9844,    0,      0,      0,      0,      0},
        {0x9850,    0,      0,      0,      0,      0},
        {0x9834,    0,      0,      0,      0,      0},
        {0x9828,    0,      0,      0,      0,      0},
        {0xc864,    0,      0,      0,      0,      0},
        {0x9848,    0,      0,      0,      0,      0},
        {0xb848,    0,      0,      0,      0,      0},
        {0xa20c,    0,      0,      0,      0,      0},
        {0xc20c,    0,      0,      0,      0,      0},
        {0x9920,    0,      0,      0,      0,      0},
        {0xb920,    0,      0,      0,      0,      0},
        {0xa258,    0,      0,      0,      0,      0},
    };

    zmw_get_wlan_dev(dev);
    hpPriv=wd->hpPrivate;

    /* #1 Save the initial value of the related RIFS register settings */
    //((struct zsHpPriv*)wd->hpPrivate)->isInitialPhy++;

    /*
     * Setup the indices for the next set of register array writes
     * PHY mode is static20 / 2040
     * Frequency is 2.4GHz (B) / 5GHz (A)
     */
    if ( frequency > ZM_CH_G_14 )
    {
        /* 5GHz */
        freqIndex  = 1;
        if (bw40)
        {
            modesIndex = 2;
            zm_debug_msg0("init ar5416Modes in 2: A-20/40");
        }
        else
        {
            modesIndex = 1;
            zm_debug_msg0("init ar5416Modes in 1: A-20");
        }
    }
    else
    {
        /* 2.4GHz */
        freqIndex  = 2;
        if (bw40)
        {
            modesIndex = 3;
            zm_debug_msg0("init ar5416Modes in 3: G-20/40");
        }
        else
        {
            modesIndex = 4;
            zm_debug_msg0("init ar5416Modes in 4: G-20");
        }
    }


#if ZM_FPGA_PHY == 1
    /* Starting External Hainan Register Initialization */
    /* TODO: */

    zfwSleep(dev, 10);
#endif

    /*
     *Set correct Baseband to analog shift setting to access analog chips.
     */
    //reg_write(PHY_BASE, 0x00000007);
//    reg_write(0x9800, 0x00000007);

    /*
     * Write addac shifts
     */
     // do this in firmware



    /* Zeroize board data */
    for (j=0; j<15; j++)
    {
        for (k=1; k<=4; k++)
        {
            eepromBoardData[j][k] = 0;
        }
    }
     /*
     * Register setting by mode
     */

    entries = sizeof(ar5416Modes) / sizeof(*ar5416Modes);
    zm_msg1_scan(ZM_LV_2, "Modes register setting entries=", entries);
    for (i=0; i<entries; i++)
    {
#if 0
        if ( ((struct zsHpPriv*)wd->hpPrivate)->hwNotFirstInit && (ar5416Modes[i][0] == 0xa27c) )
        {
            /* Force disable CR671 bit20 / 7823                                            */
            /* The bug has to do with the polarity of the pdadc offset calibration.  There */
            /* is an initial calibration that is OK, and there is a continuous             */
            /* calibration that updates the pddac with the wrong polarity.  Fortunately    */
            /* the second loop can be disabled with a bit called en_pd_dc_offset_thr.      */

            reg_write(ar5416Modes[i][0], (ar5416Modes[i][modesIndex]& 0xffefffff) );
            ((struct zsHpPriv*)wd->hpPrivate)->hwNotFirstInit = 1;
        }
        else
        {
#endif
            /* FirstTime Init or not 0xa27c(CR671) */
            reg_write(ar5416Modes[i][0], ar5416Modes[i][modesIndex]);
//        }
        /* Initialize board data */
        for (j=0; j<15; j++)
        {
            if (ar5416Modes[i][0] == eepromBoardData[j][0])
            {
                for (k=1; k<=4; k++)
                {
                    eepromBoardData[j][k] = ar5416Modes[i][k];
                }
            }
        }
        /* #1 Save the initial value of the related RIFS register settings */
        //if( ((struct zsHpPriv*)wd->hpPrivate)->isInitialPhy == 1 )
        {
            switch(ar5416Modes[i][0])
            {
                case 0x9850 :
                    ((struct zsHpPriv*)wd->hpPrivate)->initDesiredSigSize           = ar5416Modes[i][modesIndex];
                    break;
                case 0x985c :
                    ((struct zsHpPriv*)wd->hpPrivate)->initAGC                      = ar5416Modes[i][modesIndex];
                    break;
                case 0x9860 :
                    ((struct zsHpPriv*)wd->hpPrivate)->initAgcControl               = ar5416Modes[i][modesIndex];
                    break;
                case 0x9918 :
                    ((struct zsHpPriv*)wd->hpPrivate)->initSearchStartDelay         = ar5416Modes[i][modesIndex];
                    break;
                case 0x99ec :
                    ((struct zsHpPriv*)wd->hpPrivate)->initRIFSSearchParams         = ar5416Modes[i][modesIndex];
                    break;
                case 0xa388 :
                    ((struct zsHpPriv*)wd->hpPrivate)->initFastChannelChangeControl = ar5416Modes[i][modesIndex];
                default :
                    break;
            }
        }
    }
#if 0
    zfFlushDelayWrite(dev);

    /*
     * Common Register setting
     */
    entries = sizeof(ar5416Common) / sizeof(*ar5416Common);
    for (i=0; i<entries; i++)
    {
        reg_write(ar5416Common[i][0], ar5416Common[i][1]);
    }
    zfFlushDelayWrite(dev);

    /*
     * RF Gain setting by freqIndex
     */
    entries = sizeof(ar5416BB_RfGain) / sizeof(*ar5416BB_RfGain);
    for (i=0; i<entries; i++)
    {
        reg_write(ar5416BB_RfGain[i][0], ar5416BB_RfGain[i][freqIndex]);
    }
    zfFlushDelayWrite(dev);

    /*
     * Moved ar5416InitChainMask() here to ensure the swap bit is set before
     * the pdadc table is written.  Swap must occur before any radio dependent
     * replicated register access.  The pdadc curve addressing in particular
     * depends on the consistent setting of the swap bit.
     */
    //ar5416InitChainMask(pDev);

    /* Setup the transmit power values. */
    // TODO
#endif

    /* Update 5G board data */
    //Ant control common
    tmp = hpPriv->eepromImage[0x100+0x144*2/4];
    eepromBoardData[0][1] = tmp;
    eepromBoardData[0][2] = tmp;
    //Ant control chain 0
    tmp = hpPriv->eepromImage[0x100+0x140*2/4];
    eepromBoardData[1][1] = tmp;
    eepromBoardData[1][2] = tmp;
    //Ant control chain 2
    tmp = hpPriv->eepromImage[0x100+0x142*2/4];
    eepromBoardData[2][1] = tmp;
    eepromBoardData[2][2] = tmp;
    //SwSettle
    tmp = hpPriv->eepromImage[0x100+0x146*2/4];
    tmp = (tmp >> 16) & 0x7f;
    eepromBoardData[3][1] &= (~((u32_t)0x3f80));
    eepromBoardData[3][1] |= (tmp << 7);
#if 0
    //swSettleHt40
    tmp = hpPriv->eepromImage[0x100+0x158*2/4];
    tmp = (tmp) & 0x7f;
    eepromBoardData[3][2] &= (~((u32_t)0x3f80));
    eepromBoardData[3][2] |= (tmp << 7);
#endif
    //adcDesired, pdaDesired
    tmp = hpPriv->eepromImage[0x100+0x148*2/4];
    tmp = (tmp >> 24);
    tmp1 = hpPriv->eepromImage[0x100+0x14a*2/4];
    tmp1 = tmp1 & 0xff;
    tmp = tmp + (tmp1<<8);
    eepromBoardData[4][1] &= (~((u32_t)0xffff));
    eepromBoardData[4][1] |= tmp;
    eepromBoardData[4][2] &= (~((u32_t)0xffff));
    eepromBoardData[4][2] |= tmp;
    //TxEndToXpaOff, TxFrameToXpaOn
    tmp = hpPriv->eepromImage[0x100+0x14a*2/4];
    tmp = (tmp >> 24) & 0xff;
    tmp1 = hpPriv->eepromImage[0x100+0x14c*2/4];
    tmp1 = (tmp1 >> 8) & 0xff;
    tmp = (tmp<<24) + (tmp<<16) + (tmp1<<8) + tmp1;
    eepromBoardData[5][1] = tmp;
    eepromBoardData[5][2] = tmp;
    //TxEnaToRxOm
    tmp = hpPriv->eepromImage[0x100+0x14c*2/4] & 0xff;
    eepromBoardData[6][1] &= (~((u32_t)0xff0000));
    eepromBoardData[6][1] |= (tmp<<16);
    eepromBoardData[6][2] &= (~((u32_t)0xff0000));
    eepromBoardData[6][2] |= (tmp<<16);
    //Thresh62
    tmp = hpPriv->eepromImage[0x100+0x14c*2/4];
    tmp = (tmp >> 16) & 0x7f;
    eepromBoardData[7][1] &= (~((u32_t)0x7f000));
    eepromBoardData[7][1] |= (tmp<<12);
    eepromBoardData[7][2] &= (~((u32_t)0x7f000));
    eepromBoardData[7][2] |= (tmp<<12);
    //TxRxAtten chain_0
    tmp = hpPriv->eepromImage[0x100+0x146*2/4];
    tmp = (tmp >> 24) & 0x3f;
    eepromBoardData[8][1] &= (~((u32_t)0x3f000));
    eepromBoardData[8][1] |= (tmp<<12);
    eepromBoardData[8][2] &= (~((u32_t)0x3f000));
    eepromBoardData[8][2] |= (tmp<<12);
    //TxRxAtten chain_2
    tmp = hpPriv->eepromImage[0x100+0x148*2/4] & 0x3f;
    eepromBoardData[9][1] &= (~((u32_t)0x3f000));
    eepromBoardData[9][1] |= (tmp<<12);
    eepromBoardData[9][2] &= (~((u32_t)0x3f000));
    eepromBoardData[9][2] |= (tmp<<12);
    //TxRxMargin chain_0
    tmp = hpPriv->eepromImage[0x100+0x148*2/4];
    tmp = (tmp >> 8) & 0x3f;
    eepromBoardData[10][1] &= (~((u32_t)0xfc0000));
    eepromBoardData[10][1] |= (tmp<<18);
    eepromBoardData[10][2] &= (~((u32_t)0xfc0000));
    eepromBoardData[10][2] |= (tmp<<18);
    //TxRxMargin chain_2
    tmp = hpPriv->eepromImage[0x100+0x148*2/4];
    tmp = (tmp >> 16) & 0x3f;
    eepromBoardData[11][1] &= (~((u32_t)0xfc0000));
    eepromBoardData[11][1] |= (tmp<<18);
    eepromBoardData[11][2] &= (~((u32_t)0xfc0000));
    eepromBoardData[11][2] |= (tmp<<18);
    //iqCall chain_0, iqCallQ chain_0
    tmp = hpPriv->eepromImage[0x100+0x14e*2/4];
    tmp = (tmp >> 24) & 0x3f;
    tmp1 = hpPriv->eepromImage[0x100+0x150*2/4];
    tmp1 = (tmp1 >> 8) & 0x1f;
    tmp  = (tmp<<5) + tmp1;
    eepromBoardData[12][1] &= (~((u32_t)0x7ff));
    eepromBoardData[12][1] |= (tmp);
    eepromBoardData[12][2] &= (~((u32_t)0x7ff));
    eepromBoardData[12][2] |= (tmp);
    //iqCall chain_2, iqCallQ chain_2
    tmp = hpPriv->eepromImage[0x100+0x150*2/4];
    tmp = tmp & 0x3f;
    tmp1 = hpPriv->eepromImage[0x100+0x150*2/4];
    tmp1 = (tmp1 >> 16) & 0x1f;
    tmp  = (tmp<<5) + tmp1;
    eepromBoardData[13][1] &= (~((u32_t)0x7ff));
    eepromBoardData[13][1] |= (tmp);
    eepromBoardData[13][2] &= (~((u32_t)0x7ff));
    eepromBoardData[13][2] |= (tmp);
    //bsw_Margin chain_0
    tmp = hpPriv->eepromImage[0x100+0x156*2/4];
    tmp = (tmp >> 16) & 0xf;
    eepromBoardData[10][1] &= (~((u32_t)0x3c00));
    eepromBoardData[10][1] |= (tmp << 10);
    eepromBoardData[10][2] &= (~((u32_t)0x3c00));
    eepromBoardData[10][2] |= (tmp << 10);
    //xpd gain mask
    tmp = hpPriv->eepromImage[0x100+0x14e*2/4];
    tmp = (tmp >> 8) & 0xf;
    eepromBoardData[14][1] &= (~((u32_t)0xf0000));
    eepromBoardData[14][1] |= (zcXpdToPd[tmp] << 16);
    eepromBoardData[14][2] &= (~((u32_t)0xf0000));
    eepromBoardData[14][2] |= (zcXpdToPd[tmp] << 16);
#if 0
    //bsw_Atten chain_0
    tmp = hpPriv->eepromImage[0x100+0x156*2/4];
    tmp = (tmp) & 0x1f;
    eepromBoardData[10][1] &= (~((u32_t)0x1f));
    eepromBoardData[10][1] |= (tmp);
    eepromBoardData[10][2] &= (~((u32_t)0x1f));
    eepromBoardData[10][2] |= (tmp);
    //bsw_Margin chain_2
    tmp = hpPriv->eepromImage[0x100+0x156*2/4];
    tmp = (tmp >> 24) & 0xf;
    eepromBoardData[11][1] &= (~((u32_t)0x3c00));
    eepromBoardData[11][1] |= (tmp << 10);
    eepromBoardData[11][2] &= (~((u32_t)0x3c00));
    eepromBoardData[11][2] |= (tmp << 10);
    //bsw_Atten chain_2
    tmp = hpPriv->eepromImage[0x100+0x156*2/4];
    tmp = (tmp >> 8) & 0x1f;
    eepromBoardData[11][1] &= (~((u32_t)0x1f));
    eepromBoardData[11][1] |= (tmp);
    eepromBoardData[11][2] &= (~((u32_t)0x1f));
    eepromBoardData[11][2] |= (tmp);
#endif

    /* Update 2.4G board data */
    //Ant control common
    tmp = hpPriv->eepromImage[0x100+0x170*2/4];
    tmp = tmp >> 24;
    tmp1 = hpPriv->eepromImage[0x100+0x172*2/4];
    tmp = tmp + (tmp1 << 8);
    eepromBoardData[0][3] = tmp;
    eepromBoardData[0][4] = tmp;
    //Ant control chain 0
    tmp = hpPriv->eepromImage[0x100+0x16c*2/4];
    tmp = tmp >> 24;
    tmp1 = hpPriv->eepromImage[0x100+0x16e*2/4];
    tmp = tmp + (tmp1 << 8);
    eepromBoardData[1][3] = tmp;
    eepromBoardData[1][4] = tmp;
    //Ant control chain 2
    tmp = hpPriv->eepromImage[0x100+0x16e*2/4];
    tmp = tmp >> 24;
    tmp1 = hpPriv->eepromImage[0x100+0x170*2/4];
    tmp = tmp + (tmp1 << 8);
    eepromBoardData[2][3] = tmp;
    eepromBoardData[2][4] = tmp;
    //SwSettle
    tmp = hpPriv->eepromImage[0x100+0x174*2/4];
    tmp = (tmp >> 8) & 0x7f;
    eepromBoardData[3][4] &= (~((u32_t)0x3f80));
    eepromBoardData[3][4] |= (tmp << 7);
#if 0
    //swSettleHt40
    tmp = hpPriv->eepromImage[0x100+0x184*2/4];
    tmp = (tmp >> 24) & 0x7f;
    eepromBoardData[3][3] &= (~((u32_t)0x3f80));
    eepromBoardData[3][3] |= (tmp << 7);
#endif
    //adcDesired, pdaDesired
    tmp = hpPriv->eepromImage[0x100+0x176*2/4];
    tmp = (tmp >> 16) & 0xff;
    tmp1 = hpPriv->eepromImage[0x100+0x176*2/4];
    tmp1 = tmp1 >> 24;
    tmp = tmp + (tmp1<<8);
    eepromBoardData[4][3] &= (~((u32_t)0xffff));
    eepromBoardData[4][3] |= tmp;
    eepromBoardData[4][4] &= (~((u32_t)0xffff));
    eepromBoardData[4][4] |= tmp;
    //TxEndToXpaOff, TxFrameToXpaOn
    tmp = hpPriv->eepromImage[0x100+0x178*2/4];
    tmp = (tmp >> 16) & 0xff;
    tmp1 = hpPriv->eepromImage[0x100+0x17a*2/4];
    tmp1 = tmp1 & 0xff;
    tmp = (tmp << 24) + (tmp << 16) + (tmp1 << 8) + tmp1;
    eepromBoardData[5][3] = tmp;
    eepromBoardData[5][4] = tmp;
    //TxEnaToRxOm
    tmp = hpPriv->eepromImage[0x100+0x178*2/4];
    tmp = (tmp >> 24);
    eepromBoardData[6][3] &= (~((u32_t)0xff0000));
    eepromBoardData[6][3] |= (tmp<<16);
    eepromBoardData[6][4] &= (~((u32_t)0xff0000));
    eepromBoardData[6][4] |= (tmp<<16);
    //Thresh62
    tmp = hpPriv->eepromImage[0x100+0x17a*2/4];
    tmp = (tmp >> 8) & 0x7f;
    eepromBoardData[7][3] &= (~((u32_t)0x7f000));
    eepromBoardData[7][3] |= (tmp<<12);
    eepromBoardData[7][4] &= (~((u32_t)0x7f000));
    eepromBoardData[7][4] |= (tmp<<12);
    //TxRxAtten chain_0
    tmp = hpPriv->eepromImage[0x100+0x174*2/4];
    tmp = (tmp >> 16) & 0x3f;
    eepromBoardData[8][3] &= (~((u32_t)0x3f000));
    eepromBoardData[8][3] |= (tmp<<12);
    eepromBoardData[8][4] &= (~((u32_t)0x3f000));
    eepromBoardData[8][4] |= (tmp<<12);
    //TxRxAtten chain_2
    tmp = hpPriv->eepromImage[0x100+0x174*2/4];
    tmp = (tmp >> 24) & 0x3f;
    eepromBoardData[9][3] &= (~((u32_t)0x3f000));
    eepromBoardData[9][3] |= (tmp<<12);
    eepromBoardData[9][4] &= (~((u32_t)0x3f000));
    eepromBoardData[9][4] |= (tmp<<12);
    //TxRxMargin chain_0
    tmp = hpPriv->eepromImage[0x100+0x176*2/4];
    tmp = (tmp) & 0x3f;
    eepromBoardData[10][3] &= (~((u32_t)0xfc0000));
    eepromBoardData[10][3] |= (tmp<<18);
    eepromBoardData[10][4] &= (~((u32_t)0xfc0000));
    eepromBoardData[10][4] |= (tmp<<18);
    //TxRxMargin chain_2
    tmp = hpPriv->eepromImage[0x100+0x176*2/4];
    tmp = (tmp >> 8) & 0x3f;
    eepromBoardData[11][3] &= (~((u32_t)0xfc0000));
    eepromBoardData[11][3] |= (tmp<<18);
    eepromBoardData[11][4] &= (~((u32_t)0xfc0000));
    eepromBoardData[11][4] |= (tmp<<18);
    //iqCall chain_0, iqCallQ chain_0
    tmp = hpPriv->eepromImage[0x100+0x17c*2/4];
    tmp = (tmp >> 16) & 0x3f;
    tmp1 = hpPriv->eepromImage[0x100+0x17e*2/4];
    tmp1 = (tmp1) & 0x1f;
    tmp  = (tmp<<5) + tmp1;
    eepromBoardData[12][3] &= (~((u32_t)0x7ff));
    eepromBoardData[12][3] |= (tmp);
    eepromBoardData[12][4] &= (~((u32_t)0x7ff));
    eepromBoardData[12][4] |= (tmp);
    //iqCall chain_2, iqCallQ chain_2
    tmp = hpPriv->eepromImage[0x100+0x17c*2/4];
    tmp = (tmp>>24) & 0x3f;
    tmp1 = hpPriv->eepromImage[0x100+0x17e*2/4];
    tmp1 = (tmp1 >> 8) & 0x1f;
    tmp  = (tmp<<5) + tmp1;
    eepromBoardData[13][3] &= (~((u32_t)0x7ff));
    eepromBoardData[13][3] |= (tmp);
    eepromBoardData[13][4] &= (~((u32_t)0x7ff));
    eepromBoardData[13][4] |= (tmp);
    //xpd gain mask
    tmp = hpPriv->eepromImage[0x100+0x17c*2/4];
    tmp = tmp & 0xf;
    DbgPrint("xpd=0x%x, pd=0x%x\n", tmp, zcXpdToPd[tmp]);
    eepromBoardData[14][3] &= (~((u32_t)0xf0000));
    eepromBoardData[14][3] |= (zcXpdToPd[tmp] << 16);
    eepromBoardData[14][4] &= (~((u32_t)0xf0000));
    eepromBoardData[14][4] |= (zcXpdToPd[tmp] << 16);
#if 0
    //bsw_Margin chain_0
    tmp = hpPriv->eepromImage[0x100+0x184*2/4];
    tmp = (tmp >> 8) & 0xf;
    eepromBoardData[10][3] &= (~((u32_t)0x3c00));
    eepromBoardData[10][3] |= (tmp << 10);
    eepromBoardData[10][4] &= (~((u32_t)0x3c00));
    eepromBoardData[10][4] |= (tmp << 10);
    //bsw_Atten chain_0
    tmp = hpPriv->eepromImage[0x100+0x182*2/4];
    tmp = (tmp>>24) & 0x1f;
    eepromBoardData[10][3] &= (~((u32_t)0x1f));
    eepromBoardData[10][3] |= (tmp);
    eepromBoardData[10][4] &= (~((u32_t)0x1f));
    eepromBoardData[10][4] |= (tmp);
    //bsw_Margin chain_2
    tmp = hpPriv->eepromImage[0x100+0x184*2/4];
    tmp = (tmp >> 16) & 0xf;
    eepromBoardData[11][3] &= (~((u32_t)0x3c00));
    eepromBoardData[11][3] |= (tmp << 10);
    eepromBoardData[11][4] &= (~((u32_t)0x3c00));
    eepromBoardData[11][4] |= (tmp << 10);
    //bsw_Atten chain_2
    tmp = hpPriv->eepromImage[0x100+0x184*2/4];
    tmp = (tmp) & 0x1f;
    eepromBoardData[11][3] &= (~((u32_t)0x1f));
    eepromBoardData[11][3] |= (tmp);
    eepromBoardData[11][4] &= (~((u32_t)0x1f));
    eepromBoardData[11][4] |= (tmp);
#endif

#if 0
    for (j=0; j<14; j++)
    {
        DbgPrint("%04x, %08x, %08x, %08x, %08x\n", eepromBoardData[j][0], eepromBoardData[j][1], eepromBoardData[j][2], eepromBoardData[j][3], eepromBoardData[j][4]);
    }
#endif

    if ((hpPriv->eepromImage[0x100+0x110*2/4]&0xff) == 0x80) //FEM TYPE
    {
        /* Update board data to registers */
        for (j=0; j<15; j++)
        {
            reg_write(eepromBoardData[j][0], eepromBoardData[j][modesIndex]);

            /* #1 Save the initial value of the related RIFS register settings */
            //if( ((struct zsHpPriv*)wd->hpPrivate)->isInitialPhy == 1 )
            {
                switch(eepromBoardData[j][0])
                {
                    case 0x9850 :
                        ((struct zsHpPriv*)wd->hpPrivate)->initDesiredSigSize           = eepromBoardData[j][modesIndex];
                        break;
                    case 0x985c :
                        ((struct zsHpPriv*)wd->hpPrivate)->initAGC                      = eepromBoardData[j][modesIndex];
                        break;
                    case 0x9860 :
                        ((struct zsHpPriv*)wd->hpPrivate)->initAgcControl               = eepromBoardData[j][modesIndex];
                        break;
                    case 0x9918 :
                        ((struct zsHpPriv*)wd->hpPrivate)->initSearchStartDelay         = eepromBoardData[j][modesIndex];
                        break;
                    case 0x99ec :
                        ((struct zsHpPriv*)wd->hpPrivate)->initRIFSSearchParams         = eepromBoardData[j][modesIndex];
                        break;
                    case 0xa388 :
                        ((struct zsHpPriv*)wd->hpPrivate)->initFastChannelChangeControl = eepromBoardData[j][modesIndex];
                    default :
                        break;
                }
            }
        }
    } /* if ((hpPriv->eepromImage[0x100+0x110*2/4]&0xff) == 0x80) //FEM TYPE */


    /* Bringup issue : force tx gain */
    //reg_write(0xa258, 0x0cc65381);
    //reg_write(0xa274, 0x0a1a7c15);
    zfInitPowerCal(dev);

    if(frequency > ZM_CH_G_14)
    {
        zfDelayWriteInternalReg(dev, 0x1d4014, 0x5143);
    }
    else
    {
        zfDelayWriteInternalReg(dev, 0x1d4014, 0x5163);
    }

    zfFlushDelayWrite(dev);
}


void zfInitRf(zdev_t* dev, u32_t frequency)
{
    u32_t cmd[8];
    u16_t ret;
    int delta_slope_coeff_exp;
    int delta_slope_coeff_man;
    int delta_slope_coeff_exp_shgi;
    int delta_slope_coeff_man_shgi;

    zmw_get_wlan_dev(dev);

    zm_debug_msg1(" initRf frequency = ", frequency);

    if (frequency == 0)
    {
        frequency = 2412;
    }

    /* Bank 0 1 2 3 5 6 7 */
    zfSetRfRegs(dev, frequency);
    /* Bank 4 */
    zfSetBank4AndPowerTable(dev, frequency, 0, 0);

    /* stroe frequency */
    ((struct zsHpPriv*)wd->hpPrivate)->hwFrequency = (u16_t)frequency;

    zfGetHwTurnOffdynParam(dev,
                           frequency, 0, 0,
                           &delta_slope_coeff_exp,
                           &delta_slope_coeff_man,
                           &delta_slope_coeff_exp_shgi,
                           &delta_slope_coeff_man_shgi);

    /* related functions */
    frequency = frequency*1000;
    cmd[0] = 28 | (ZM_CMD_RF_INIT << 8);
    cmd[1] = frequency;
    cmd[2] = 0;//((struct zsHpPriv*)wd->hpPrivate)->hw_DYNAMIC_HT2040_EN;
    cmd[3] = 1;//((wd->ExtOffset << 2) | ((struct zsHpPriv*)wd->hpPrivate)->hw_HT_ENABLE);
    cmd[4] = delta_slope_coeff_exp;
    cmd[5] = delta_slope_coeff_man;
    cmd[6] = delta_slope_coeff_exp_shgi;
    cmd[7] = delta_slope_coeff_man_shgi;

    ret = zfIssueCmd(dev, cmd, 32, ZM_OID_INTERNAL_WRITE, 0);

    // delay temporarily, wait for new PHY and RF
    zfwSleep(dev, 1000);
}

int tn(int exp)
{
    int i;
	int tmp = 1;
    for(i=0; i<exp; i++)
        tmp = tmp*2;

    return tmp;
}

/*int zfFloor(double indata)
{
   if(indata<0)
	   return (int)indata-1;
   else
	   return (int)indata;
}
*/
u32_t reverse_bits(u32_t chan_sel)
{
	/* reverse_bits */
    u32_t chansel = 0;
	u8_t i;

	for (i=0; i<8; i++)
        chansel |= ((chan_sel>>(7-i) & 0x1) << i);
	return chansel;
}

/* Bank 0 1 2 3 5 6 7 */
void zfSetRfRegs(zdev_t* dev, u32_t frequency)
{
    u16_t entries;
    u16_t freqIndex = 0;
    u16_t i;

    //zmw_get_wlan_dev(dev);

    if ( frequency > ZM_CH_G_14 )
    {
        /* 5G */
        freqIndex = 1;
        zm_msg0_scan(ZM_LV_2, "Set to 5GHz");

    }
    else
    {
        /* 2.4G */
        freqIndex = 2;
        zm_msg0_scan(ZM_LV_2, "Set to 2.4GHz");
    }

#if 1
    entries = sizeof(otusBank) / sizeof(*otusBank);
    for (i=0; i<entries; i++)
    {
        reg_write(otusBank[i][0], otusBank[i][freqIndex]);
    }
#else
    /* Bank0 */
    entries = sizeof(ar5416Bank0) / sizeof(*ar5416Bank0);
    for (i=0; i<entries; i++)
    {
        reg_write(ar5416Bank0[i][0], ar5416Bank0[i][1]);
    }
    /* Bank1 */
    entries = sizeof(ar5416Bank1) / sizeof(*ar5416Bank1);
    for (i=0; i<entries; i++)
    {
        reg_write(ar5416Bank1[i][0], ar5416Bank1[i][1]);
    }
    /* Bank2 */
    entries = sizeof(ar5416Bank2) / sizeof(*ar5416Bank2);
    for (i=0; i<entries; i++)
    {
        reg_write(ar5416Bank2[i][0], ar5416Bank2[i][1]);
    }
    /* Bank3 */
    entries = sizeof(ar5416Bank3) / sizeof(*ar5416Bank3);
    for (i=0; i<entries; i++)
    {
        reg_write(ar5416Bank3[i][0], ar5416Bank3[i][freqIndex]);
    }
    /* Bank5 */
    reg_write (0x98b0,  0x00000013);
    reg_write (0x98e4,  0x00000002);
    /* Bank6 */
    entries = sizeof(ar5416Bank6) / sizeof(*ar5416Bank6);
    for (i=0; i<entries; i++)
    {
        reg_write(ar5416Bank6[i][0], ar5416Bank6[i][freqIndex]);
    }
    /* Bank7 */
    entries = sizeof(ar5416Bank7) / sizeof(*ar5416Bank7);
    for (i=0; i<entries; i++)
    {
        reg_write(ar5416Bank7[i][0], ar5416Bank7[i][1]);
    }
#endif

    zfFlushDelayWrite(dev);
}

/* Bank 4 */
void zfSetBank4AndPowerTable(zdev_t* dev, u32_t frequency, u8_t bw40,
        u8_t extOffset)
{
    u32_t chup = 1;
	u32_t bmode_LF_synth_freq = 0;
	u32_t amode_refsel_1 = 0;
	u32_t amode_refsel_0 = 1;
	u32_t addr2 = 1;
	u32_t addr1 = 0;
	u32_t addr0 = 0;

	u32_t d1;
	u32_t d0;
	u32_t tmp_0;
	u32_t tmp_1;
	u32_t data0;
	u32_t data1;

	u8_t chansel;
	u8_t chan_sel;
	u32_t temp_chan_sel;

    u16_t i;

    zmw_get_wlan_dev(dev);


    /* if enable 802.11h, need to record curent channel index in channel array */
    if (wd->sta.DFSEnable)
    {
        for (i = 0; i < wd->regulationTable.allowChannelCnt; i++)
        {
            if (wd->regulationTable.allowChannel[i].channel == frequency)
                break;
        }
        wd->regulationTable.CurChIndex = i;
    }

	if (bw40 == 1)
	{
        if (extOffset == 1)
        {
            frequency += 10;
        }
        else
        {
            frequency -= 10;
        }

	}


	if ( frequency > 3000 )
	{
	    if ( frequency % 10 )
	    {
	        /* 5M */
            chan_sel = (u8_t)((frequency - 4800)/5);
            chan_sel = (u8_t)(chan_sel & 0xff);
            chansel  = (u8_t)reverse_bits(chan_sel);
        }
        else
        {
            /* 10M : improve Tx EVM */
            chan_sel = (u8_t)((frequency - 4800)/10);
            chan_sel = (u8_t)(chan_sel & 0xff)<<1;
            chansel  = (u8_t)reverse_bits(chan_sel);

	        amode_refsel_1 = 1;
	        amode_refsel_0 = 0;
        }
	}
	else
	{
        //temp_chan_sel = (((frequency - 672)*2) - 3040)/10;
        if (frequency == 2484)
        {
   	        temp_chan_sel = 10 + (frequency - 2274)/5 ;
   	        bmode_LF_synth_freq = 1;
        }
        else
        {
            temp_chan_sel = 16 + (frequency - 2272)/5 ;
            bmode_LF_synth_freq = 0;
        }
        chan_sel = (u8_t)(temp_chan_sel << 2) & 0xff;
        chansel  = (u8_t)reverse_bits(chan_sel);
	}

	d1   = chansel;   //# 8 bits of chan
	d0   = addr0<<7 | addr1<<6 | addr2<<5
			| amode_refsel_0<<3 | amode_refsel_1<<2
			| bmode_LF_synth_freq<<1 | chup;

    tmp_0 = d0 & 0x1f;  //# 5-1
    tmp_1 = d1 & 0x1f;  //# 5-1
    data0 = tmp_1<<5 | tmp_0;

    tmp_0 = d0>>5 & 0x7;  //# 8-6
    tmp_1 = d1>>5 & 0x7;  //# 8-6
    data1 = tmp_1<<5 | tmp_0;

    /* Bank4 */
	reg_write (0x9800+(0x2c<<2), data0);
	reg_write (0x9800+(0x3a<<2), data1);
	//zm_debug_msg1("0x9800+(0x2c<<2 =  ", data0);
	//zm_debug_msg1("0x9800+(0x3a<<2 =  ", data1);


    zfFlushDelayWrite(dev);

    zfwSleep(dev, 10);

    return;
}


struct zsPhyFreqPara
{
    u32_t coeff_exp;
    u32_t coeff_man;
    u32_t coeff_exp_shgi;
    u32_t coeff_man_shgi;
};

struct zsPhyFreqTable
{
    u32_t frequency;
    struct zsPhyFreqPara FpgaDynamicHT;
    struct zsPhyFreqPara FpgaStaticHT;
    struct zsPhyFreqPara ChipST20Mhz;
    struct zsPhyFreqPara Chip2040Mhz;
    struct zsPhyFreqPara Chip2040ExtAbove;
};

const struct zsPhyFreqTable zgPhyFreqCoeff[] =
{
/*Index   freq  FPGA DYNAMIC_HT2040_EN  FPGA STATIC_HT20    Real Chip static20MHz     Real Chip 2040MHz   Real Chip 2040Mhz  */
       /* fclk =         10.8                21.6                  40                  ext below 40       ext above 40       */
/*  0 */ {2412, {5, 23476, 5, 21128}, {4, 23476, 4, 21128}, {3, 21737, 3, 19563}, {3, 21827, 3, 19644}, {3, 21647, 3, 19482}},
/*  1 */ {2417, {5, 23427, 5, 21084}, {4, 23427, 4, 21084}, {3, 21692, 3, 19523}, {3, 21782, 3, 19604}, {3, 21602, 3, 19442}},
/*  2 */ {2422, {5, 23379, 5, 21041}, {4, 23379, 4, 21041}, {3, 21647, 3, 19482}, {3, 21737, 3, 19563}, {3, 21558, 3, 19402}},
/*  3 */ {2427, {5, 23330, 5, 20997}, {4, 23330, 4, 20997}, {3, 21602, 3, 19442}, {3, 21692, 3, 19523}, {3, 21514, 3, 19362}},
/*  4 */ {2432, {5, 23283, 5, 20954}, {4, 23283, 4, 20954}, {3, 21558, 3, 19402}, {3, 21647, 3, 19482}, {3, 21470, 3, 19323}},
/*  5 */ {2437, {5, 23235, 5, 20911}, {4, 23235, 4, 20911}, {3, 21514, 3, 19362}, {3, 21602, 3, 19442}, {3, 21426, 3, 19283}},
/*  6 */ {2442, {5, 23187, 5, 20868}, {4, 23187, 4, 20868}, {3, 21470, 3, 19323}, {3, 21558, 3, 19402}, {3, 21382, 3, 19244}},
/*  7 */ {2447, {5, 23140, 5, 20826}, {4, 23140, 4, 20826}, {3, 21426, 3, 19283}, {3, 21514, 3, 19362}, {3, 21339, 3, 19205}},
/*  8 */ {2452, {5, 23093, 5, 20783}, {4, 23093, 4, 20783}, {3, 21382, 3, 19244}, {3, 21470, 3, 19323}, {3, 21295, 3, 19166}},
/*  9 */ {2457, {5, 23046, 5, 20741}, {4, 23046, 4, 20741}, {3, 21339, 3, 19205}, {3, 21426, 3, 19283}, {3, 21252, 3, 19127}},
/* 10 */ {2462, {5, 22999, 5, 20699}, {4, 22999, 4, 20699}, {3, 21295, 3, 19166}, {3, 21382, 3, 19244}, {3, 21209, 3, 19088}},
/* 11 */ {2467, {5, 22952, 5, 20657}, {4, 22952, 4, 20657}, {3, 21252, 3, 19127}, {3, 21339, 3, 19205}, {3, 21166, 3, 19050}},
/* 12 */ {2472, {5, 22906, 5, 20615}, {4, 22906, 4, 20615}, {3, 21209, 3, 19088}, {3, 21295, 3, 19166}, {3, 21124, 3, 19011}},
/* 13 */ {2484, {5, 22795, 5, 20516}, {4, 22795, 4, 20516}, {3, 21107, 3, 18996}, {3, 21192, 3, 19073}, {3, 21022, 3, 18920}},
/* 14 */ {4920, {6, 23018, 6, 20716}, {5, 23018, 5, 20716}, {4, 21313, 4, 19181}, {4, 21356, 4, 19220}, {4, 21269, 4, 19142}},
/* 15 */ {4940, {6, 22924, 6, 20632}, {5, 22924, 5, 20632}, {4, 21226, 4, 19104}, {4, 21269, 4, 19142}, {4, 21183, 4, 19065}},
/* 16 */ {4960, {6, 22832, 6, 20549}, {5, 22832, 5, 20549}, {4, 21141, 4, 19027}, {4, 21183, 4, 19065}, {4, 21098, 4, 18988}},
/* 17 */ {4980, {6, 22740, 6, 20466}, {5, 22740, 5, 20466}, {4, 21056, 4, 18950}, {4, 21098, 4, 18988}, {4, 21014, 4, 18912}},
/* 18 */ {5040, {6, 22469, 6, 20223}, {5, 22469, 5, 20223}, {4, 20805, 4, 18725}, {4, 20846, 4, 18762}, {4, 20764, 4, 18687}},
/* 19 */ {5060, {6, 22381, 6, 20143}, {5, 22381, 5, 20143}, {4, 20723, 4, 18651}, {4, 20764, 4, 18687}, {4, 20682, 4, 18614}},
/* 20 */ {5080, {6, 22293, 6, 20063}, {5, 22293, 5, 20063}, {4, 20641, 4, 18577}, {4, 20682, 4, 18614}, {4, 20601, 4, 18541}},
/* 21 */ {5180, {6, 21862, 6, 19676}, {5, 21862, 5, 19676}, {4, 20243, 4, 18219}, {4, 20282, 4, 18254}, {4, 20204, 4, 18183}},
/* 22 */ {5200, {6, 21778, 6, 19600}, {5, 21778, 5, 19600}, {4, 20165, 4, 18148}, {4, 20204, 4, 18183}, {4, 20126, 4, 18114}},
/* 23 */ {5220, {6, 21695, 6, 19525}, {5, 21695, 5, 19525}, {4, 20088, 4, 18079}, {4, 20126, 4, 18114}, {4, 20049, 4, 18044}},
/* 24 */ {5240, {6, 21612, 6, 19451}, {5, 21612, 5, 19451}, {4, 20011, 4, 18010}, {4, 20049, 4, 18044}, {4, 19973, 4, 17976}},
/* 25 */ {5260, {6, 21530, 6, 19377}, {5, 21530, 5, 19377}, {4, 19935, 4, 17941}, {4, 19973, 4, 17976}, {4, 19897, 4, 17907}},
/* 26 */ {5280, {6, 21448, 6, 19303}, {5, 21448, 5, 19303}, {4, 19859, 4, 17873}, {4, 19897, 4, 17907}, {4, 19822, 4, 17840}},
/* 27 */ {5300, {6, 21367, 6, 19230}, {5, 21367, 5, 19230}, {4, 19784, 4, 17806}, {4, 19822, 4, 17840}, {4, 19747, 4, 17772}},
/* 28 */ {5320, {6, 21287, 6, 19158}, {5, 21287, 5, 19158}, {4, 19710, 4, 17739}, {4, 19747, 4, 17772}, {4, 19673, 4, 17706}},
/* 29 */ {5500, {6, 20590, 6, 18531}, {5, 20590, 5, 18531}, {4, 19065, 4, 17159}, {4, 19100, 4, 17190}, {4, 19030, 4, 17127}},
/* 30 */ {5520, {6, 20516, 6, 18464}, {5, 20516, 5, 18464}, {4, 18996, 4, 17096}, {4, 19030, 4, 17127}, {4, 18962, 4, 17065}},
/* 31 */ {5540, {6, 20442, 6, 18397}, {5, 20442, 5, 18397}, {4, 18927, 4, 17035}, {4, 18962, 4, 17065}, {4, 18893, 4, 17004}},
/* 32 */ {5560, {6, 20368, 6, 18331}, {5, 20368, 5, 18331}, {4, 18859, 4, 16973}, {4, 18893, 4, 17004}, {4, 18825, 4, 16943}},
/* 33 */ {5580, {6, 20295, 6, 18266}, {5, 20295, 5, 18266}, {4, 18792, 4, 16913}, {4, 18825, 4, 16943}, {4, 18758, 4, 16882}},
/* 34 */ {5600, {6, 20223, 6, 18200}, {5, 20223, 5, 18200}, {4, 18725, 4, 16852}, {4, 18758, 4, 16882}, {4, 18691, 4, 16822}},
/* 35 */ {5620, {6, 20151, 6, 18136}, {5, 20151, 5, 18136}, {4, 18658, 4, 16792}, {4, 18691, 4, 16822}, {4, 18625, 4, 16762}},
/* 36 */ {5640, {6, 20079, 6, 18071}, {5, 20079, 5, 18071}, {4, 18592, 4, 16733}, {4, 18625, 4, 16762}, {4, 18559, 4, 16703}},
/* 37 */ {5660, {6, 20008, 6, 18007}, {5, 20008, 5, 18007}, {4, 18526, 4, 16673}, {4, 18559, 4, 16703}, {4, 18493, 4, 16644}},
/* 38 */ {5680, {6, 19938, 6, 17944}, {5, 19938, 5, 17944}, {4, 18461, 4, 16615}, {4, 18493, 4, 16644}, {4, 18428, 4, 16586}},
/* 39 */ {5700, {6, 19868, 6, 17881}, {5, 19868, 5, 17881}, {4, 18396, 4, 16556}, {4, 18428, 4, 16586}, {4, 18364, 4, 16527}},
/* 40 */ {5745, {6, 19712, 6, 17741}, {5, 19712, 5, 17741}, {4, 18252, 4, 16427}, {4, 18284, 4, 16455}, {4, 18220, 4, 16398}},
/* 41 */ {5765, {6, 19644, 6, 17679}, {5, 19644, 5, 17679}, {4, 18189, 5, 32740}, {4, 18220, 4, 16398}, {4, 18157, 5, 32683}},
/* 42 */ {5785, {6, 19576, 6, 17618}, {5, 19576, 5, 17618}, {4, 18126, 5, 32626}, {4, 18157, 5, 32683}, {4, 18094, 5, 32570}},
/* 43 */ {5805, {6, 19508, 6, 17558}, {5, 19508, 5, 17558}, {4, 18063, 5, 32514}, {4, 18094, 5, 32570}, {4, 18032, 5, 32458}},
/* 44 */ {5825, {6, 19441, 6, 17497}, {5, 19441, 5, 17497}, {4, 18001, 5, 32402}, {4, 18032, 5, 32458}, {4, 17970, 5, 32347}},
/* 45 */ {5170, {6, 21904, 6, 19714}, {5, 21904, 5, 19714}, {4, 20282, 4, 18254}, {4, 20321, 4, 18289}, {4, 20243, 4, 18219}},
/* 46 */ {5190, {6, 21820, 6, 19638}, {5, 21820, 5, 19638}, {4, 20204, 4, 18183}, {4, 20243, 4, 18219}, {4, 20165, 4, 18148}},
/* 47 */ {5210, {6, 21736, 6, 19563}, {5, 21736, 5, 19563}, {4, 20126, 4, 18114}, {4, 20165, 4, 18148}, {4, 20088, 4, 18079}},
/* 48 */ {5230, {6, 21653, 6, 19488}, {5, 21653, 5, 19488}, {4, 20049, 4, 18044}, {4, 20088, 4, 18079}, {4, 20011, 4, 18010}}
};
/* to reduce search time, please modify this define if you add or delete channel in table */
#define First5GChannelIndex 14

void zfGetHwTurnOffdynParam(zdev_t* dev,
                            u32_t frequency, u8_t bw40, u8_t extOffset,
                            int* delta_slope_coeff_exp,
                            int* delta_slope_coeff_man,
                            int* delta_slope_coeff_exp_shgi,
                            int* delta_slope_coeff_man_shgi)
{
    /* Get param for turnoffdyn */
    u16_t i, arraySize;

    //zmw_get_wlan_dev(dev);

    arraySize = sizeof(zgPhyFreqCoeff)/sizeof(struct zsPhyFreqTable);
    if (frequency < 3000)
    {
        /* 2.4GHz Channel */
        for (i = 0; i < First5GChannelIndex; i++)
        {
            if (frequency == zgPhyFreqCoeff[i].frequency)
                break;
        }

        if (i < First5GChannelIndex)
        {
        }
        else
        {
            zm_msg1_scan(ZM_LV_0, "Unsupported 2.4G frequency = ", frequency);
            return;
        }
    }
    else
    {
        /* 5GHz Channel */
        for (i = First5GChannelIndex; i < arraySize; i++)
        {
            if (frequency == zgPhyFreqCoeff[i].frequency)
                break;
        }

        if (i < arraySize)
        {
        }
        else
        {
            zm_msg1_scan(ZM_LV_0, "Unsupported 5G frequency = ", frequency);
            return;
        }
    }

    /* FPGA DYNAMIC_HT2040_EN        fclk = 10.8  */
    /* FPGA STATIC_HT20_             fclk = 21.6  */
    /* Real Chip                     fclk = 40    */
    #if ZM_FPGA_PHY == 1
    //fclk = 10.8;
    *delta_slope_coeff_exp = zgPhyFreqCoeff[i].FpgaDynamicHT.coeff_exp;
    *delta_slope_coeff_man = zgPhyFreqCoeff[i].FpgaDynamicHT.coeff_man;
    *delta_slope_coeff_exp_shgi = zgPhyFreqCoeff[i].FpgaDynamicHT.coeff_exp_shgi;
    *delta_slope_coeff_man_shgi = zgPhyFreqCoeff[i].FpgaDynamicHT.coeff_man_shgi;
    #else
    //fclk = 40;
    if (bw40)
    {
        /* ht2040 */
        if (extOffset == 1) {
            *delta_slope_coeff_exp = zgPhyFreqCoeff[i].Chip2040ExtAbove.coeff_exp;
            *delta_slope_coeff_man = zgPhyFreqCoeff[i].Chip2040ExtAbove.coeff_man;
            *delta_slope_coeff_exp_shgi = zgPhyFreqCoeff[i].Chip2040ExtAbove.coeff_exp_shgi;
            *delta_slope_coeff_man_shgi = zgPhyFreqCoeff[i].Chip2040ExtAbove.coeff_man_shgi;
        }
        else {
            *delta_slope_coeff_exp = zgPhyFreqCoeff[i].Chip2040Mhz.coeff_exp;
            *delta_slope_coeff_man = zgPhyFreqCoeff[i].Chip2040Mhz.coeff_man;
            *delta_slope_coeff_exp_shgi = zgPhyFreqCoeff[i].Chip2040Mhz.coeff_exp_shgi;
            *delta_slope_coeff_man_shgi = zgPhyFreqCoeff[i].Chip2040Mhz.coeff_man_shgi;
        }
    }
    else
    {
        /* static 20 */
        *delta_slope_coeff_exp = zgPhyFreqCoeff[i].ChipST20Mhz.coeff_exp;
        *delta_slope_coeff_man = zgPhyFreqCoeff[i].ChipST20Mhz.coeff_man;
        *delta_slope_coeff_exp_shgi = zgPhyFreqCoeff[i].ChipST20Mhz.coeff_exp_shgi;
        *delta_slope_coeff_man_shgi = zgPhyFreqCoeff[i].ChipST20Mhz.coeff_man_shgi;
    }
    #endif
}

/* Main routin frequency setting function */
/* If 2.4G/5G switch, PHY need resetting BB and RF for band switch */
/* Do the setting switch in zfSendFrequencyCmd() */
void zfHpSetFrequencyEx(zdev_t* dev, u32_t frequency, u8_t bw40,
        u8_t extOffset, u8_t initRF)
{
    u32_t cmd[9];
    u32_t cmdB[3];
    u16_t ret;
    u8_t old_band;
    u8_t new_band;
    u32_t checkLoopCount;
    u32_t tmpValue;

    int delta_slope_coeff_exp;
    int delta_slope_coeff_man;
    int delta_slope_coeff_exp_shgi;
    int delta_slope_coeff_man_shgi;
    struct zsHpPriv* hpPriv;

    zmw_get_wlan_dev(dev);
    hpPriv = wd->hpPrivate;

    zm_msg1_scan(ZM_LV_1, "Frequency = ", frequency);
    zm_msg1_scan(ZM_LV_1, "bw40 = ", bw40);
    zm_msg1_scan(ZM_LV_1, "extOffset = ", extOffset);

    if ( hpPriv->coldResetNeedFreq )
    {
        hpPriv->coldResetNeedFreq = 0;
        initRF = 2;
        zm_debug_msg0("zfHpSetFrequencyEx: Do ColdReset ");
    }
    if ( hpPriv->isSiteSurvey == 2 )
    {
        /* wait time for AGC and noise calibration : not in sitesurvey and connected */
        checkLoopCount = 2000; /* 2000*100 = 200ms */
    }
    else
    {
        /* wait time for AGC and noise calibration : in sitesurvey */
        checkLoopCount = 1000; /* 1000*100 = 100ms */
    }

    hpPriv->latestFrequency = frequency;
    hpPriv->latestBw40 = bw40;
    hpPriv->latestExtOffset = extOffset;

    if ((hpPriv->dot11Mode == ZM_HAL_80211_MODE_IBSS_GENERAL) ||
        (hpPriv->dot11Mode == ZM_HAL_80211_MODE_IBSS_WPA2PSK))
    {
        if ( frequency <= ZM_CH_G_14 )
        {
            /* workaround for 11g Ad Hoc beacon distribution */
            zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC0_CW, 0x7f0007);
            //zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC1_AC0_AIFS, 0x1c04901c);
        }
    }

    /* AHB, DAC, ADC clock selection by static20/ht2040 */
    zfSelAdcClk(dev, bw40, frequency);

    /* clear bb_heavy_clip_enable */
    reg_write(0x99e0, 0x200);
    zfFlushDelayWrite(dev);

    /* Set CTS/RTS rate */
    if ( frequency > ZM_CH_G_14 )
    {
        //zfHpSetRTSCTSRate(dev, 0x10b010b);  /* OFDM 6M */
	    new_band = 1;
	}
    else
    {
        //zfHpSetRTSCTSRate(dev, 0x30003);  /* CCK 11M */
        new_band = 0;
    }

    if (((struct zsHpPriv*)wd->hpPrivate)->hwFrequency > ZM_CH_G_14)
        old_band = 1;
    else
        old_band = 0;

    //Workaround for 2.4GHz only device
    if ((hpPriv->OpFlags & 0x1) == 0)
    {
        if ((((struct zsHpPriv*)wd->hpPrivate)->hwFrequency == ZM_CH_G_1) && (frequency == ZM_CH_G_2))
        {
            /* Force to do band switching */
            old_band = 1;
        }
    }

    /* Notify channel switch to firmware */
    /* TX/RX must be stopped by now */
    cmd[0] = 0 | (ZM_CMD_FREQ_STRAT << 8);
    ret = zfIssueCmd(dev, cmd, 8, ZM_OID_INTERNAL_WRITE, 0);

    if ((initRF != 0) || (new_band != old_band)
            || (((struct zsHpPriv*)wd->hpPrivate)->hwBw40 != bw40))
    {
        /* band switch */
        zm_msg0_scan(ZM_LV_1, "=====band switch=====");

        if (initRF == 2 )
        {
            //Cold reset BB/ADDA
            zfDelayWriteInternalReg(dev, 0x1d4004, 0x800);
            zfFlushDelayWrite(dev);
            zm_msg0_scan(ZM_LV_1, "Do cold reset BB/ADDA");
        }
        else
        {
            //Warm reset BB/ADDA
            zfDelayWriteInternalReg(dev, 0x1d4004, 0x400);
            zfFlushDelayWrite(dev);
        }

        /* reset workaround state to default */
        hpPriv->rxStrongRSSI = 0;
        hpPriv->strongRSSI = 0;

        zfDelayWriteInternalReg(dev, 0x1d4004, 0x0);
        zfFlushDelayWrite(dev);

        zfInitPhy(dev, frequency, bw40);

//        zfiCheckRifs(dev);

        /* Bank 0 1 2 3 5 6 7 */
        zfSetRfRegs(dev, frequency);
        /* Bank 4 */
        zfSetBank4AndPowerTable(dev, frequency, bw40, extOffset);

        cmd[0] = 32 | (ZM_CMD_RF_INIT << 8);
    }
    else //((new_band == old_band) && !initRF)
    {
       /* same band */

       /* Force disable CR671 bit20 / 7823                                            */
       /* The bug has to do with the polarity of the pdadc offset calibration.  There */
       /* is an initial calibration that is OK, and there is a continuous             */
       /* calibration that updates the pddac with the wrong polarity.  Fortunately    */
       /* the second loop can be disabled with a bit called en_pd_dc_offset_thr.      */
#if 0
        cmdB[0] = 8 | (ZM_CMD_BITAND << 8);;
        cmdB[1] = (0xa27c + 0x1bc000);
        cmdB[2] = 0xffefffff;
        ret = zfIssueCmd(dev, cmdB, 12, ZM_OID_INTERNAL_WRITE, 0);
#endif

       /* Bank 4 */
       zfSetBank4AndPowerTable(dev, frequency, bw40, extOffset);


        cmd[0] = 32 | (ZM_CMD_FREQUENCY << 8);
    }

    /* Compatibility for new layout UB83 */
    /* Setting code at CR1 here move from the func:zfHwHTEnable() in firmware */
    if (((struct zsHpPriv*)wd->hpPrivate)->halCapability & ZM_HP_CAP_11N_ONE_TX_STREAM)
    {
        /* UB83 : one stream */
        tmpValue = 0;
    }
    else
    {
        /* UB81, UB82 : two stream */
        tmpValue = 0x100;
    }

    if (1) //if (((struct zsHpPriv*)wd->hpPrivate)->hw_HT_ENABLE == 1)
	{
        if (bw40 == 1)
		{
			if (extOffset == 1) {
            	reg_write(0x9804, tmpValue | 0x2d4); //3d4 for real
			}
			else {
				reg_write(0x9804, tmpValue | 0x2c4);   //3c4 for real
			}
			//# Dyn HT2040.Refer to Reg 1.
            //#[3]:single length (4us) 1st HT long training symbol; use Walsh spatial spreading for 2 chains 2 streams TX
            //#[c]:allow short GI for HT40 packets; enable HT detection.
            //#[4]:enable 20/40 MHz channel detection.
        }
        else
	    {
            reg_write(0x9804, tmpValue | 0x240);
		    //# Static HT20
            //#[3]:single length (4us) 1st HT long training symbol; use Walsh spatial spreading for 2 chains 2 streams TX
            //#[4]:Otus don't allow short GI for HT20 packets yet; enable HT detection.
            //#[0]:disable 20/40 MHz channel detection.
        }
    }
    else
	{
        reg_write(0x9804, 0x0);
		//# Legacy;# Direct Mapping for each chain.
        //#Be modified by Oligo to add dynanic for legacy.
        if (bw40 == 1)
		{
            reg_write(0x9804, 0x4);     //# Dyn Legacy .Refer to reg 1.
        }
        else
		{
            reg_write(0x9804, 0x0);    //# Static Legacy
        }
	}
	zfFlushDelayWrite(dev);
	/* end of ub83 compatibility */

    /* Set Power, TPC, Gain table... */
	zfSetPowerCalTable(dev, frequency, bw40, extOffset);


    /* store frequency */
    ((struct zsHpPriv*)wd->hpPrivate)->hwFrequency = (u16_t)frequency;
    ((struct zsHpPriv*)wd->hpPrivate)->hwBw40 = bw40;
    ((struct zsHpPriv*)wd->hpPrivate)->hwExtOffset = extOffset;

    zfGetHwTurnOffdynParam(dev,
                           frequency, bw40, extOffset,
                           &delta_slope_coeff_exp,
                           &delta_slope_coeff_man,
                           &delta_slope_coeff_exp_shgi,
                           &delta_slope_coeff_man_shgi);

    /* related functions */
    frequency = frequency*1000;
    /* len[36] : type[0x30] : seq[?] */
//    cmd[0] = 28 | (ZM_CMD_FREQUENCY << 8);
    cmd[1] = frequency;
    cmd[2] = bw40;//((struct zsHpPriv*)wd->hpPrivate)->hw_DYNAMIC_HT2040_EN;
    cmd[3] = (extOffset<<2)|0x1;//((wd->ExtOffset << 2) | ((struct zsHpPriv*)wd->hpPrivate)->hw_HT_ENABLE);
    cmd[4] = delta_slope_coeff_exp;
    cmd[5] = delta_slope_coeff_man;
    cmd[6] = delta_slope_coeff_exp_shgi;
    cmd[7] = delta_slope_coeff_man_shgi;
    cmd[8] = checkLoopCount;

    ret = zfIssueCmd(dev, cmd, 36, ZM_CMD_SET_FREQUENCY, 0);

    // delay temporarily, wait for new PHY and RF
    //zfwSleep(dev, 1000);
}


/******************** Key ********************/

u16_t zfHpResetKeyCache(zdev_t* dev)
{
    u8_t i;
    u32_t key[4] = {0, 0, 0, 0};
    struct zsHpPriv* hpPriv;

    zmw_get_wlan_dev(dev);
    hpPriv=wd->hpPrivate;

    for(i=0;i<4;i++)
    {
        zfHpSetDefaultKey(dev, i, ZM_WEP64, key, NULL);
    }
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_ROLL_CALL_TBL_L, 0x00);
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_ROLL_CALL_TBL_H, 0x00);
    zfFlushDelayWrite(dev);

    hpPriv->camRollCallTable = (u64_t) 0;

    return 0;
}


/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfSetKey                    */
/*      Set key.                                                        */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      0 : success                                                     */
/*      other : fail                                                    */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen Chen        ZyDAS Technology Corporation    2006.1      */
/*                                                                      */
/************************************************************************/
/* ! please use zfCoreSetKey() in 80211Core for SetKey */
u32_t zfHpSetKey(zdev_t* dev, u8_t user, u8_t keyId, u8_t type,
        u16_t* mac, u32_t* key)
{
    u32_t cmd[(ZM_MAX_CMD_SIZE/4)];
    u16_t ret;
    u16_t i;
    struct zsHpPriv* hpPriv;

    zmw_get_wlan_dev(dev);
    hpPriv=wd->hpPrivate;

#if 0   /* remove to zfCoreSetKey() */
    zmw_declare_for_critical_section();

    zmw_enter_critical_section(dev);
    wd->sta.flagKeyChanging++;
    zm_debug_msg1("   zfHpSetKey++++ ", wd->sta.flagKeyChanging);
    zmw_leave_critical_section(dev);
#endif

    cmd[0] = 0x0000281C;
    cmd[1] = ((u32_t)keyId<<16) + (u32_t)user;
    cmd[2] = ((u32_t)mac[0]<<16) + (u32_t)type;
    cmd[3] = ((u32_t)mac[2]<<16) + ((u32_t)mac[1]);

    for (i=0; i<4; i++)
    {
        cmd[4+i] = key[i];
    }

    if (user < 64)
    {
        hpPriv->camRollCallTable |= ((u64_t) 1) << user;
    }

    //ret = zfIssueCmd(dev, cmd, 32, ZM_OID_INTERNAL_WRITE, NULL);
    ret = zfIssueCmd(dev, cmd, 32, ZM_CMD_SET_KEY, NULL);
    return ret;
}


u32_t zfHpSetApPairwiseKey(zdev_t* dev, u16_t* staMacAddr, u8_t type,
        u32_t* key, u32_t* micKey, u16_t staAid)
{
    if ((staAid!=0) && (staAid<64))
    {
        zfHpSetKey(dev, (staAid-1), 0, type, staMacAddr, key);
                if ((type == ZM_TKIP)
#ifdef ZM_ENABLE_CENC
         || (type == ZM_CENC)
#endif //ZM_ENABLE_CENC
           )
            zfHpSetKey(dev, (staAid-1), 1, type, staMacAddr, micKey);
        return 0;
    }
    return 1;
}

u32_t zfHpSetApGroupKey(zdev_t* dev, u16_t* apMacAddr, u8_t type,
        u32_t* key, u32_t* micKey, u16_t vapId)
{
    zfHpSetKey(dev, ZM_USER_KEY_DEFAULT - 1 - vapId, 0, type, apMacAddr, key);	// 6D18 modify from 0 to 1 ??
            if ((type == ZM_TKIP)
#ifdef ZM_ENABLE_CENC
         || (type == ZM_CENC)
#endif //ZM_ENABLE_CENC
           )
        zfHpSetKey(dev, ZM_USER_KEY_DEFAULT - 1 - vapId, 1, type, apMacAddr, micKey);
    return 0;
}

u32_t zfHpSetDefaultKey(zdev_t* dev, u8_t keyId, u8_t type, u32_t* key, u32_t* micKey)
{
    u16_t macAddr[3] = {0, 0, 0};

    #ifdef ZM_ENABLE_IBSS_WPA2PSK
    struct zsHpPriv* hpPriv;

    zmw_get_wlan_dev(dev);
    hpPriv = wd->hpPrivate;

    if ( hpPriv->dot11Mode == ZM_HAL_80211_MODE_IBSS_WPA2PSK )
    { /* If not wpa2psk , use traditional */
      /* Because the bug of chip , defaultkey should follow the key map rule in register 700 */
        if ( keyId == 0 )
            zfHpSetKey(dev, ZM_USER_KEY_DEFAULT+keyId, 0, type, macAddr, key);
        else
            zfHpSetKey(dev, ZM_USER_KEY_DEFAULT+keyId, 1, type, macAddr, key);
    }
    else
        zfHpSetKey(dev, ZM_USER_KEY_DEFAULT+keyId, 0, type, macAddr, key);
    #else
        zfHpSetKey(dev, ZM_USER_KEY_DEFAULT+keyId, 0, type, macAddr, key);
    #endif
            if ((type == ZM_TKIP)

#ifdef ZM_ENABLE_CENC
         || (type == ZM_CENC)
#endif //ZM_ENABLE_CENC
           )
    {
        zfHpSetKey(dev, ZM_USER_KEY_DEFAULT+keyId, 1, type, macAddr, micKey);
    }

    return 0;
}

u32_t zfHpSetPerUserKey(zdev_t* dev, u8_t user, u8_t keyId, u8_t* mac, u8_t type, u32_t* key, u32_t* micKey)
{
#ifdef ZM_ENABLE_IBSS_WPA2PSK
    struct zsHpPriv* hpPriv;

    zmw_get_wlan_dev(dev);
    hpPriv = wd->hpPrivate;

    if ( hpPriv->dot11Mode == ZM_HAL_80211_MODE_IBSS_WPA2PSK )
    { /* If not wpa2psk , use traditional */
        if(keyId)
        {  /* Set Group Key */
            zfHpSetKey(dev, user, 1, type, (u16_t *)mac, key);
        }
        else if(keyId == 0)
        {  /* Set Pairwise Key */
            zfHpSetKey(dev, user, 0, type, (u16_t *)mac, key);
        }
    }
    else
    {
        zfHpSetKey(dev, user, keyId, type, (u16_t *)mac, key);
    }
#else
    zfHpSetKey(dev, user, keyId, type, (u16_t *)mac, key);
#endif

            if ((type == ZM_TKIP)
#ifdef ZM_ENABLE_CENC
         || (type == ZM_CENC)
#endif //ZM_ENABLE_CENC
           )
    {
        zfHpSetKey(dev, user, keyId + 1, type, (u16_t *)mac, micKey);
    }
    return 0;
}

/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfHpRemoveKey               */
/*      Remove key.                                                     */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      0 : success                                                     */
/*      other : fail                                                    */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Yuan-Gu Wei         ZyDAS Technology Corporation    2006.6      */
/*                                                                      */
/************************************************************************/
u16_t zfHpRemoveKey(zdev_t* dev, u16_t user)
{
    u32_t cmd[(ZM_MAX_CMD_SIZE/4)];
    u16_t ret = 0;

    cmd[0] = 0x00002904;
    cmd[1] = (u32_t)user;

    ret = zfIssueCmd(dev, cmd, 8, ZM_OID_INTERNAL_WRITE, NULL);
    return ret;
}



/******************** DMA ********************/
u16_t zfHpStartRecv(zdev_t* dev)
{
    zfDelayWriteInternalReg(dev, 0x1c3d30, 0x100);
    zfFlushDelayWrite(dev);

    return 0;
}

u16_t zfHpStopRecv(zdev_t* dev)
{
    return 0;
}


/******************** MAC ********************/
void zfInitMac(zdev_t* dev)
{
    /* ACK extension register */
    // jhlee temp : change value 0x2c -> 0x40
    // honda resolve short preamble problem : 0x40 -> 0x75
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_ACK_EXTENSION, 0x40); // 0x28 -> 0x2c 6522:yflee

    /* TxQ0/1/2/3 Retry MAX=2 => transmit 3 times and degrade rate for retry */
    /* PB42 AP crash issue:                                                  */
    /* Workaround the crash issue by CTS/RTS, set retry max to zero for      */
    /*   workaround tx underrun which enable CTS/RTS */
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_RETRY_MAX, 0); // 0x11111 => 0

    /* use hardware MIC check */
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_SNIFFER, 0x2000000);

    /* Set Rx threshold to 1600 */
#if ZM_LARGEPAYLOAD_TEST == 1
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_RX_THRESHOLD, 0xc4000);
#else
    #ifndef ZM_DISABLE_AMSDU8K_SUPPORT
    /* The maximum A-MSDU length is 3839/7935 */
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_RX_THRESHOLD, 0xc1f80);
    #else
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_RX_THRESHOLD, 0xc0f80);
    #endif
#endif

    //zfDelayWriteInternalReg(dev, ZM_MAC_REG_DYNAMIC_SIFS_ACK, 0x10A);
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_RX_PE_DELAY, 0x70);
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_EIFS_AND_SIFS, 0xa144000);
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_SLOT_TIME, 9<<10);

    /* CF-END mode */
    zfDelayWriteInternalReg(dev, 0x1c3b2c, 0x19000000);

    //NAV protects ACK only (in TXOP)
    zfDelayWriteInternalReg(dev, 0x1c3b38, 0x201);


    /* Set Beacon PHY CTRL's TPC to 0x7, TA1=1 */
    /* OTUS set AM to 0x1 */
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_BCN_HT1, 0x8000170);

    /* TODO : wep backoff protection 0x63c */
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_BACKOFF_PROTECT, 0x105);

    /* AGG test code*/
    /* Aggregation MAX number and timeout */
    zfDelayWriteInternalReg(dev, 0x1c3b9c, 0x10000a);
    /* Filter any control frames, BAR is bit 24 */
    zfDelayWriteInternalReg(dev, 0x1c368c, 0x0500ffff);
    /* Enable deaggregator */
    zfDelayWriteInternalReg(dev, 0x1c3c40, 0x1);

    /* Basic rate */
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_BASIC_RATE, 0x150f);
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_MANDATORY_RATE, 0x150f);
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_RTS_CTS_RATE, 0x10b01bb);

    /* MIMO resposne control */
    zfDelayWriteInternalReg(dev, 0x1c3694, 0x4003C1E);/* bit 26~28  otus-AM */

    /* Enable LED0 and LED1 */
    zfDelayWriteInternalReg(dev, 0x1d0100, 0x3);
    zfDelayWriteInternalReg(dev, 0x1d0104, 0x3);

    /* switch MAC to OTUS interface */
    zfDelayWriteInternalReg(dev, 0x1c3600, 0x3);

    /* RXMAC A-MPDU length threshold */
    zfDelayWriteInternalReg(dev, 0x1c3c50, 0xffff);

	/* Phy register read timeout */
	zfDelayWriteInternalReg(dev, 0x1c3680, 0xf00008);

	/* Disable Rx TimeOut : workaround for BB.
	 *  OTUS would interrupt the rx frame that sent by OWL TxUnderRun
	 *  because OTUS rx timeout behavior, then OTUS would not ack the BA for
	 *  this AMPDU from OWL.
	 *  Fix by Perry Hwang.  2007/05/10.
	 *  0x1c362c : Rx timeout value : bit 27~16
	 */
	zfDelayWriteInternalReg(dev, 0x1c362c, 0x0);

    //Set USB Rx stream mode MAX packet number to 2
    //    Max packet number = *0x1e1110 + 1
    zfDelayWriteInternalReg(dev, 0x1e1110, 0x4);
    //Set USB Rx stream mode timeout to 10us
    zfDelayWriteInternalReg(dev, 0x1e1114, 0x80);

    //Set CPU clock frequency to 88/80MHz
    zfDelayWriteInternalReg(dev, 0x1D4008, 0x73);

    //Set WLAN DMA interrupt mode : generate int per packet
    zfDelayWriteInternalReg(dev, 0x1c3d7c, 0x110011);

    /* 7807 */
    /* enable func : Reset FIFO1 and FIFO2 when queue-gnt is low */
    /* 0x1c3bb0 Bit2 */
    /* Disable SwReset in firmware for TxHang, enable reset FIFO func. */
    zfDelayWriteInternalReg(dev, 0x1c3bb0, 0x4);

    /* Disables the CF_END frame */
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_TXOP_NOT_ENOUGH_INDICATION, 0x141E0F48);

	/* Disable the SW Decrypt*/
	zfDelayWriteInternalReg(dev, 0x1c3678, 0x70);
    zfFlushDelayWrite(dev);
    //---------------------

    /* Set TxQs CWMIN, CWMAX, AIFS and TXO to WME STA default. */
    zfUpdateDefaultQosParameter(dev, 0);

    //zfSelAdcClk(dev, 0);

    return;
}


u16_t zfHpSetSnifferMode(zdev_t* dev, u16_t on)
{
    if (on != 0)
    {
        zfDelayWriteInternalReg(dev, ZM_MAC_REG_SNIFFER, 0x2000001);
    }
    else
    {
        zfDelayWriteInternalReg(dev, ZM_MAC_REG_SNIFFER, 0x2000000);
    }
    zfFlushDelayWrite(dev);
    return 0;
}


u16_t zfHpSetApStaMode(zdev_t* dev, u8_t mode)
{
    struct zsHpPriv* hpPriv;

    zmw_get_wlan_dev(dev);
    hpPriv = wd->hpPrivate;
    hpPriv->dot11Mode = mode;

    switch(mode)
    {
        case ZM_HAL_80211_MODE_AP:
            zfDelayWriteInternalReg(dev, 0x1c3700, 0x0f0000a1);
            zfDelayWriteInternalReg(dev, 0x1c3c40, 0x1);
            break;

        case ZM_HAL_80211_MODE_STA:
            zfDelayWriteInternalReg(dev, 0x1c3700, 0x0f000002);
            zfDelayWriteInternalReg(dev, 0x1c3c40, 0x1);
            break;

        case ZM_HAL_80211_MODE_IBSS_GENERAL:
            zfDelayWriteInternalReg(dev, 0x1c3700, 0x0f000000);
            zfDelayWriteInternalReg(dev, 0x1c3c40, 0x1);
            break;

        case ZM_HAL_80211_MODE_IBSS_WPA2PSK:
            zfDelayWriteInternalReg(dev, 0x1c3700, 0x0f0000e0);
            zfDelayWriteInternalReg(dev, 0x1c3c40, 0x41);       // for multiple ( > 2 ) stations IBSS network
            break;

        default:
            goto skip;
    }

    zfFlushDelayWrite(dev);

skip:
    return 0;
}


u16_t zfHpSetBssid(zdev_t* dev, u8_t* bssidSrc)
{
    u32_t  address;
    u16_t *bssid = (u16_t *)bssidSrc;

    address = bssid[0] + (((u32_t)bssid[1]) << 16);
    zfDelayWriteInternalReg(dev, 0x1c3618, address);

    address = (u32_t)bssid[2];
    zfDelayWriteInternalReg(dev, 0x1c361C, address);
    zfFlushDelayWrite(dev);
    return 0;
}


/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfHpUpdateQosParameter      */
/*      Update TxQs CWMIN, CWMAX, AIFS and TXOP.                        */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      cwminTbl : CWMIN parameter for TxQs                             */
/*      cwmaxTbl : CWMAX parameter for TxQs                             */
/*      aifsTbl: AIFS parameter for TxQs                                */
/*      txopTbl : TXOP parameter for TxQs                               */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      none                                                            */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen             ZyDAS Technology Corporation    2006.6      */
/*                                                                      */
/************************************************************************/
u8_t zfHpUpdateQosParameter(zdev_t* dev, u16_t* cwminTbl, u16_t* cwmaxTbl,
        u16_t* aifsTbl, u16_t* txopTbl)
{
    struct zsHpPriv* hpPriv;

    zmw_get_wlan_dev(dev);
    hpPriv = wd->hpPrivate;

    zm_msg0_mm(ZM_LV_0, "zfHalUpdateQosParameter()");

    /* Note : Do not change cwmin for Q0 in Ad Hoc mode              */
    /*        otherwise driver will fail in Wifi beacon distribution */
    if (hpPriv->dot11Mode == ZM_HAL_80211_MODE_STA)
    {
#if 0 //Restore CWmin to improve down link throughput
        //cheating in BE traffic
        if (wd->sta.EnableHT == 1)
        {
            //cheating in BE traffic
            cwminTbl[0] = 7;//15;
        }
#endif
        cwmaxTbl[0] = 127;//1023;
        aifsTbl[0] = 2*9+10;//3 * 9 + 10;
    }

    /* CWMIN and CWMAX */
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC0_CW, cwminTbl[0]
            + ((u32_t)cwmaxTbl[0]<<16));
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC1_CW, cwminTbl[1]
            + ((u32_t)cwmaxTbl[1]<<16));
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC2_CW, cwminTbl[2]
            + ((u32_t)cwmaxTbl[2]<<16));
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC3_CW, cwminTbl[3]
            + ((u32_t)cwmaxTbl[3]<<16));
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC4_CW, cwminTbl[4]
            + ((u32_t)cwmaxTbl[4]<<16));

    /* AIFS */
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC1_AC0_AIFS, aifsTbl[0]
            +((u32_t)aifsTbl[0]<<12)+((u32_t)aifsTbl[0]<<24));
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC3_AC2_AIFS, (aifsTbl[0]>>8)
            +((u32_t)aifsTbl[0]<<4)+((u32_t)aifsTbl[0]<<16));

    /* TXOP */
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC1_AC0_TXOP, txopTbl[0]
            + ((u32_t)txopTbl[1]<<16));
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC3_AC2_TXOP, txopTbl[2]
            + ((u32_t)txopTbl[3]<<16));

    zfFlushDelayWrite(dev);

    hpPriv->txop[0] = txopTbl[0];
    hpPriv->txop[1] = txopTbl[1];
    hpPriv->txop[2] = txopTbl[2];
    hpPriv->txop[3] = txopTbl[3];
    hpPriv->cwmin[0] = cwminTbl[0];
    hpPriv->cwmax[0] = cwmaxTbl[0];
    hpPriv->cwmin[1] = cwminTbl[1];
    hpPriv->cwmax[1] = cwmaxTbl[1];

    return 0;
}


void zfHpSetAtimWindow(zdev_t* dev, u16_t atimWin)
{
    zm_msg1_mm(ZM_LV_0, "Set ATIM window to ", atimWin);
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_ATIM_WINDOW, atimWin);
    zfFlushDelayWrite(dev);
}


void zfHpSetBasicRateSet(zdev_t* dev, u16_t bRateBasic, u16_t gRateBasic)
{
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_BASIC_RATE, bRateBasic
                            | ((u16_t)gRateBasic<<8));
    zfFlushDelayWrite(dev);
}


/* HT40 send by OFDM 6M    */
/* otherwise use reg 0x638 */
void zfHpSetRTSCTSRate(zdev_t* dev, u32_t rate)
{
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_RTS_CTS_RATE, rate);
    zfFlushDelayWrite(dev);
}

void zfHpSetMacAddress(zdev_t* dev, u16_t* macAddr, u16_t macAddrId)
{
    if (macAddrId == 0)
    {
        zfDelayWriteInternalReg(dev, ZM_MAC_REG_MAC_ADDR_L,
                (((u32_t)macAddr[1])<<16) | macAddr[0]);
        zfDelayWriteInternalReg(dev, ZM_MAC_REG_MAC_ADDR_H, macAddr[2]);
    }
    else if (macAddrId <= 7)
    {
        zfDelayWriteInternalReg(dev, ZM_MAC_REG_ACK_TABLE+((macAddrId-1)*8),
                macAddr[0] + ((u32_t)macAddr[1]<<16));
        zfDelayWriteInternalReg(dev, ZM_MAC_REG_ACK_TABLE+((macAddrId-1)*8)+4,
                macAddr[2]);
    }
    zfFlushDelayWrite(dev);
}

void zfHpSetMulticastList(zdev_t* dev, u8_t size, u8_t* pList, u8_t bAllMulticast)
{
    struct zsMulticastAddr* pMacList = (struct zsMulticastAddr*) pList;
    u8_t   i;
    u32_t  value;
    u32_t  swRegMulHashValueH, swRegMulHashValueL;

    swRegMulHashValueH = 0x80000000;
    swRegMulHashValueL = 0;

    if ( bAllMulticast )
    {
        swRegMulHashValueH = swRegMulHashValueL = ~0;
    }
    else
    {
        for(i=0; i<size; i++)
        {
            value = pMacList[i].addr[5] >> 2;

            if ( value < 32 )
            {
                swRegMulHashValueL |= (1 << value);
            }
            else
            {
                swRegMulHashValueH |= (1 << (value-32));
            }
        }
    }

    zfDelayWriteInternalReg(dev, ZM_MAC_REG_GROUP_HASH_TBL_L,
                            swRegMulHashValueL);
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_GROUP_HASH_TBL_H,
                            swRegMulHashValueH);
    zfFlushDelayWrite(dev);
    return;
}

/******************** Beacon ********************/
void zfHpEnableBeacon(zdev_t* dev, u16_t mode, u16_t bcnInterval, u16_t dtim, u8_t enableAtim)
{
    u32_t  value;

    zmw_get_wlan_dev(dev);

    /* Beacon Ready */
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_BCN_CTRL, 0);
    /* Beacon DMA buffer address */
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_BCN_ADDR, ZM_BEACON_BUFFER_ADDRESS);

    value = bcnInterval;

    value |= (((u32_t) dtim) << 16);

    if (mode == ZM_MODE_AP)
    {

        value |= 0x1000000;
    }
    else if (mode == ZM_MODE_IBSS)
    {
        value |= 0x2000000;

		if ( enableAtim )
		{
			value |= 0x4000000;
		}
		((struct zsHpPriv*)wd->hpPrivate)->ibssBcnEnabled = 1;
        ((struct zsHpPriv*)wd->hpPrivate)->ibssBcnInterval = value;
    }
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_PRETBTT, (bcnInterval-6)<<16);

    /* Beacon period and beacon enable */
    zfDelayWriteInternalReg(dev, ZM_MAC_REG_BCN_PERIOD, value);
    zfFlushDelayWrite(dev);
}

void zfHpDisableBeacon(zdev_t* dev)
{
    zmw_get_wlan_dev(dev);

    ((struct zsHpPriv*)wd->hpPrivate)->ibssBcnEnabled = 0;

    zfDelayWriteInternalReg(dev, ZM_MAC_REG_BCN_PERIOD, 0);
    zfFlushDelayWrite(dev);
}

void zfHpLedCtrl(zdev_t* dev, u16_t ledId, u8_t mode)
{
    u16_t state;
    zmw_get_wlan_dev(dev);

    //zm_debug_msg1("LED ID=", ledId);
    //zm_debug_msg1("LED mode=", mode);
    if (ledId < 2)
    {
        if (((struct zsHpPriv*)wd->hpPrivate)->ledMode[ledId] != mode)
        {
            ((struct zsHpPriv*)wd->hpPrivate)->ledMode[ledId] = mode;

            state = ((struct zsHpPriv*)wd->hpPrivate)->ledMode[0]
                    | (((struct zsHpPriv*)wd->hpPrivate)->ledMode[1]<<1);
            zfDelayWriteInternalReg(dev, 0x1d0104, state);
            zfFlushDelayWrite(dev);
            //zm_debug_msg0("Update LED");
        }
    }
}

/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfHpResetTxRx               */
/*      Reset Tx and Rx Desc.                                           */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      0 : success                                                     */
/*      other : fail                                                    */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Chao-Wen Yang         ZyDAS Technology Corporation    2007.3    */
/*                                                                      */
/************************************************************************/
u16_t zfHpUsbReset(zdev_t* dev)
{
    u32_t cmd[(ZM_MAX_CMD_SIZE/4)];
    u16_t ret = 0;

    //zm_debug_msg0("CWY - Reset Tx and Rx");

    cmd[0] =  0 | (ZM_CMD_RESET << 8);

    ret = zfIssueCmd(dev, cmd, 4, ZM_OID_INTERNAL_WRITE, NULL);
    return ret;
}

u16_t zfHpDKReset(zdev_t* dev, u8_t flag)
{
    u32_t cmd[(ZM_MAX_CMD_SIZE/4)];
    u16_t ret = 0;

    //zm_debug_msg0("CWY - Reset Tx and Rx");

    cmd[0] =  4 | (ZM_CMD_DKRESET << 8);
    cmd[1] = flag;

    ret = zfIssueCmd(dev, cmd, 8, ZM_OID_INTERNAL_WRITE, NULL);
    return ret;
}

u32_t zfHpCwmUpdate(zdev_t* dev)
{
    //u32_t cmd[3];
    //u16_t ret;
    //
    //cmd[0] = 0x00000008;
    //cmd[1] = 0x1c36e8;
    //cmd[2] = 0x1c36ec;
    //
    //ret = zfIssueCmd(dev, cmd, 12, ZM_CWM_READ, 0);
    //return ret;

    struct zsHpPriv* hpPriv;

    zmw_get_wlan_dev(dev);
    hpPriv=wd->hpPrivate;

    zfCoreCwmBusy(dev, zfCwmIsExtChanBusy(hpPriv->ctlBusy, hpPriv->extBusy));

    hpPriv->ctlBusy = 0;
    hpPriv->extBusy = 0;

    return 0;
}

u32_t zfHpAniUpdate(zdev_t* dev)
{
    u32_t cmd[5];
    u16_t ret;

    cmd[0] = 0x00000010;
    cmd[1] = 0x1c36e8;
    cmd[2] = 0x1c36ec;
    cmd[3] = 0x1c3cb4;
    cmd[4] = 0x1c3cb8;

    ret = zfIssueCmd(dev, cmd, 20, ZM_ANI_READ, 0);
    return ret;
}

/*
 * Update Beacon RSSI in ANI
 */
u32_t zfHpAniUpdateRssi(zdev_t* dev, u8_t rssi)
{
    struct zsHpPriv* hpPriv;

    zmw_get_wlan_dev(dev);
    hpPriv=wd->hpPrivate;

    hpPriv->stats.ast_nodestats.ns_avgbrssi = rssi;

    return 0;
}

#define ZM_SEEPROM_MAC_ADDRESS_OFFSET   (0x1400 + (0x106<<1))
#define ZM_SEEPROM_REGDOMAIN_OFFSET   (0x1400 + (0x104<<1))
#define ZM_SEEPROM_VERISON_OFFSET   (0x1400 + (0x102<<1))
#define ZM_SEEPROM_HARDWARE_TYPE_OFFSET   (0x1374)
#define ZM_SEEPROM_HW_HEAVY_CLIP          (0x161c)

u32_t zfHpGetMacAddress(zdev_t* dev)
{
    u32_t cmd[7];
    u16_t ret;

    cmd[0] = 0x00000000 | 24;
    cmd[1] = ZM_SEEPROM_MAC_ADDRESS_OFFSET;
    cmd[2] = ZM_SEEPROM_MAC_ADDRESS_OFFSET+4;
    cmd[3] = ZM_SEEPROM_REGDOMAIN_OFFSET;
    cmd[4] = ZM_SEEPROM_VERISON_OFFSET;
    cmd[5] = ZM_SEEPROM_HARDWARE_TYPE_OFFSET;
    cmd[6] = ZM_SEEPROM_HW_HEAVY_CLIP;

    ret = zfIssueCmd(dev, cmd, 28, ZM_MAC_READ, 0);
    return ret;
}

u32_t zfHpGetTransmitPower(zdev_t* dev)
{
    struct zsHpPriv*    hpPriv;
    u16_t               tpc     = 0;

    zmw_get_wlan_dev(dev);
    hpPriv  = wd->hpPrivate;

    if (hpPriv->hwFrequency < 3000) {
        tpc = hpPriv->tPow2x2g[0] & 0x3f;
        wd->maxTxPower2 &= 0x3f;
        tpc = (tpc > wd->maxTxPower2)? wd->maxTxPower2 : tpc;
    } else {
        tpc = hpPriv->tPow2x5g[0] & 0x3f;
        wd->maxTxPower5 &= 0x3f;
        tpc = (tpc > wd->maxTxPower5)? wd->maxTxPower5 : tpc;
    }

    return tpc;
}

u8_t zfHpGetMinTxPower(zdev_t* dev)
{
    struct zsHpPriv*    hpPriv;
    u8_t               tpc     = 0;

    zmw_get_wlan_dev(dev);
    hpPriv  = wd->hpPrivate;

    if (hpPriv->hwFrequency < 3000)
    {
        if(wd->BandWidth40)
        {
            //40M
            tpc = (hpPriv->tPow2x2gHt40[7]&0x3f);
        }
        else
        {
            //20M
            tpc = (hpPriv->tPow2x2gHt20[7]&0x3f);
        }
    }
    else
    {
        if(wd->BandWidth40)
        {
            //40M
            tpc = (hpPriv->tPow2x5gHt40[7]&0x3f);
        }
        else
        {
            //20M
            tpc = (hpPriv->tPow2x5gHt20[7]&0x3f);
        }
    }

    return tpc;
}

u8_t zfHpGetMaxTxPower(zdev_t* dev)
{
    struct zsHpPriv*    hpPriv;
    u8_t               tpc     = 0;

    zmw_get_wlan_dev(dev);
    hpPriv  = wd->hpPrivate;

    if (hpPriv->hwFrequency < 3000)
    {
        tpc = (hpPriv->tPow2xCck[0]&0x3f);
    }
    else
    {
        tpc =(hpPriv->tPow2x5g[0]&0x3f);
    }

    return tpc;
}

u32_t zfHpLoadEEPROMFromFW(zdev_t* dev)
{
    u32_t cmd[16];
    u32_t ret=0, i, j;
    zmw_get_wlan_dev(dev);

    i = ((struct zsHpPriv*)wd->hpPrivate)->eepromImageRdReq;

    cmd[0] = ZM_HAL_MAX_EEPROM_PRQ*4;

    for (j=0; j<ZM_HAL_MAX_EEPROM_PRQ; j++)
    {
        cmd[j+1] = 0x1000 + (((i*ZM_HAL_MAX_EEPROM_PRQ) + j)*4);
    }

    ret = zfIssueCmd(dev, cmd, (ZM_HAL_MAX_EEPROM_PRQ+1)*4, ZM_EEPROM_READ, 0);

    return ret;
}

void zfHpHeartBeat(zdev_t* dev)
{
    struct zsHpPriv* hpPriv;
    u8_t polluted = 0;
    u8_t ackTpc;

    zmw_get_wlan_dev(dev);
    hpPriv=wd->hpPrivate;

    /* Workaround : Make OTUS fire more beacon in ad hoc mode in 2.4GHz */
    if (hpPriv->ibssBcnEnabled != 0)
    {
        if (hpPriv->hwFrequency <= ZM_CH_G_14)
        {
            if ((wd->tick % 10) == 0)
            {
                if ((wd->tick % 40) == 0)
                {
                    zfDelayWriteInternalReg(dev, ZM_MAC_REG_BCN_PERIOD, hpPriv->ibssBcnInterval-1);
                    polluted = 1;
                }
                else
                {
                    zfDelayWriteInternalReg(dev, ZM_MAC_REG_BCN_PERIOD, hpPriv->ibssBcnInterval);
                    polluted = 1;
                }
            }
        }
    }

    if ((wd->tick & 0x3f) == 0x25)
    {
        /* Workaround for beacon stuck after SW reset */
        if (hpPriv->ibssBcnEnabled != 0)
        {
            zfDelayWriteInternalReg(dev, ZM_MAC_REG_BCN_ADDR, ZM_BEACON_BUFFER_ADDRESS);
            polluted = 1;
        }

        //DbgPrint("hpPriv->aggMaxDurationBE=%d", hpPriv->aggMaxDurationBE);
        //DbgPrint("wd->sta.avgSizeOfReceivePackets=%d", wd->sta.avgSizeOfReceivePackets);
        if (( wd->wlanMode == ZM_MODE_INFRASTRUCTURE )
            && (zfStaIsConnected(dev))
            && (wd->sta.EnableHT == 1) //11n mode
            && (wd->BandWidth40 == 1) //40MHz mode
            && (wd->sta.enableDrvBA ==0) //Marvel AP
            && (hpPriv->aggMaxDurationBE > 2000) //BE TXOP > 2ms
            && (wd->sta.avgSizeOfReceivePackets > 1420))
        {
            zfDelayWriteInternalReg(dev, 0x1c3b9c, 0x8000a);
            polluted = 1;
        }
        else
        {
            zfDelayWriteInternalReg(dev, 0x1c3b9c, hpPriv->aggPktNum);
            polluted = 1;
        }

        if (wd->dynamicSIFSEnable == 0)
        {
            if (( wd->wlanMode == ZM_MODE_INFRASTRUCTURE )
                && (zfStaIsConnected(dev))
                && (wd->sta.EnableHT == 1) //11n mode
                && (wd->BandWidth40 == 0) //20MHz mode
                && (wd->sta.enableDrvBA ==0)) //Marvel AP
            {
                zfDelayWriteInternalReg(dev, 0x1c3698, 0x5144000);
                polluted = 1;
            }
            else
            {
                zfDelayWriteInternalReg(dev, 0x1c3698, 0xA144000);
                polluted = 1;
            }
        }
        else
        {
            if (( wd->wlanMode == ZM_MODE_INFRASTRUCTURE )
                && (zfStaIsConnected(dev))
                && (wd->sta.EnableHT == 1) //11n mode
                && (wd->sta.athOwlAp == 1)) //Atheros AP
            {
                if (hpPriv->retransmissionEvent)
                {
                    switch(hpPriv->latestSIFS)
                    {
                    case 0:
                        hpPriv->latestSIFS = 1;
                        zfDelayWriteInternalReg(dev, ZM_MAC_REG_EIFS_AND_SIFS, 0x8144000);
                        break;
                    case 1:
                        hpPriv->latestSIFS = 2;
                        zfDelayWriteInternalReg(dev, ZM_MAC_REG_EIFS_AND_SIFS, 0xa144000);
                        break;
                    case 2:
                        hpPriv->latestSIFS = 3;
                        zfDelayWriteInternalReg(dev, ZM_MAC_REG_EIFS_AND_SIFS, 0xc144000);
                        break;
                    case 3:
                        hpPriv->latestSIFS = 0;
                        zfDelayWriteInternalReg(dev, ZM_MAC_REG_EIFS_AND_SIFS, 0xa144000);
                        break;
                    default:
                        hpPriv->latestSIFS = 0;
                        zfDelayWriteInternalReg(dev, ZM_MAC_REG_EIFS_AND_SIFS, 0xa144000);
                        break;
                    }
                    polluted = 1;
                    zm_debug_msg1("##### Correct Tx retransmission issue #####, ", hpPriv->latestSIFS);
                    hpPriv->retransmissionEvent = 0;
                }
            }
            else
            {
                hpPriv->latestSIFS = 0;
                hpPriv->retransmissionEvent = 0;
                zfDelayWriteInternalReg(dev, 0x1c3698, 0xA144000);
                polluted = 1;
            }
        }

        if ((wd->sta.bScheduleScan == FALSE) && (wd->sta.bChannelScan == FALSE))
        {
#define ZM_SIGNAL_THRESHOLD  66
        if (( wd->wlanMode == ZM_MODE_INFRASTRUCTURE )
            && (zfStaIsConnected(dev))
            && (wd->SignalStrength > ZM_SIGNAL_THRESHOLD))
        {
                /* remove state handle, always rewrite register setting */
                //if (hpPriv->strongRSSI == 0)
            {
                hpPriv->strongRSSI = 1;
                /* Strong RSSI, set ACK to one Tx stream and lower Tx power 7dbm */
                if (hpPriv->currentAckRtsTpc > (14+10))
                {
                    ackTpc = hpPriv->currentAckRtsTpc - 14;
                }
                else
                {
                    ackTpc = 10;
                }
                zfDelayWriteInternalReg(dev, 0x1c3694, ((ackTpc) << 20) | (0x1<<26));
                zfDelayWriteInternalReg(dev, 0x1c3bb4, ((ackTpc) << 5 ) | (0x1<<11) |
                                                       ((ackTpc) << 21) | (0x1<<27)  );
                polluted = 1;
            }
        }
        else
        {
                /* remove state handle, always rewrite register setting */
                //if (hpPriv->strongRSSI == 1)
            {
                hpPriv->strongRSSI = 0;
                if (hpPriv->halCapability & ZM_HP_CAP_11N_ONE_TX_STREAM)
                {
                    zfDelayWriteInternalReg(dev, 0x1c3694, ((hpPriv->currentAckRtsTpc&0x3f) << 20) | (0x1<<26));
                    zfDelayWriteInternalReg(dev, 0x1c3bb4, ((hpPriv->currentAckRtsTpc&0x3f) << 5 ) | (0x1<<11) |
                                                       ((hpPriv->currentAckRtsTpc&0x3f) << 21) | (0x1<<27)  );
                }
                else
                {
                    zfDelayWriteInternalReg(dev, 0x1c3694, ((hpPriv->currentAckRtsTpc&0x3f) << 20) | (0x5<<26));
                    zfDelayWriteInternalReg(dev, 0x1c3bb4, ((hpPriv->currentAckRtsTpc&0x3f) << 5 ) | (0x5<<11) |
                                                       ((hpPriv->currentAckRtsTpc&0x3f) << 21) | (0x5<<27)  );
                }
                polluted = 1;
            }
        }
#undef ZM_SIGNAL_THRESHOLD
        }

        if ((hpPriv->halCapability & ZM_HP_CAP_11N_ONE_TX_STREAM) == 0)
        {
            if ((wd->sta.bScheduleScan == FALSE) && (wd->sta.bChannelScan == FALSE))
            {
    #define ZM_RX_SIGNAL_THRESHOLD_H  71
    #define ZM_RX_SIGNAL_THRESHOLD_L  66
                u8_t rxSignalThresholdH = ZM_RX_SIGNAL_THRESHOLD_H;
                u8_t rxSignalThresholdL = ZM_RX_SIGNAL_THRESHOLD_L;
    #undef ZM_RX_SIGNAL_THRESHOLD_H
    #undef ZM_RX_SIGNAL_THRESHOLD_L

                if (( wd->wlanMode == ZM_MODE_INFRASTRUCTURE )
                    && (zfStaIsConnected(dev))
                    && (wd->SignalStrength > rxSignalThresholdH)
                    )//&& (hpPriv->rxStrongRSSI == 0))
                {
                    hpPriv->rxStrongRSSI = 1;
                    //zfDelayWriteInternalReg(dev, 0x1c5964, 0x1220);
                    //zfDelayWriteInternalReg(dev, 0x1c5960, 0x900);
                    //zfDelayWriteInternalReg(dev, 0x1c6960, 0x900);
                    //zfDelayWriteInternalReg(dev, 0x1c7960, 0x900);
                    if ((hpPriv->eepromImage[0x100+0x110*2/4]&0xff) == 0x80) //FEM TYPE
                    {
                        if (hpPriv->hwFrequency <= ZM_CH_G_14)
                        {
                            zfDelayWriteInternalReg(dev, 0x1c8960, 0x900);
                        }
                        else
                        {
                            zfDelayWriteInternalReg(dev, 0x1c8960, 0x9b49);
                        }
                    }
                    else
                    {
                        zfDelayWriteInternalReg(dev, 0x1c8960, 0x0900);
                    }
                    polluted = 1;
                }
                else if (( wd->wlanMode == ZM_MODE_INFRASTRUCTURE )
                    && (zfStaIsConnected(dev))
                    && (wd->SignalStrength > rxSignalThresholdL)
                    )//&& (hpPriv->rxStrongRSSI == 1))
                {
                    //Do nothing to prevent frequently Rx switching
                }
                else
                {
                    /* remove state handle, always rewrite register setting */
                    //if (hpPriv->rxStrongRSSI == 1)
                    {
                        hpPriv->rxStrongRSSI = 0;
                        //zfDelayWriteInternalReg(dev, 0x1c5964, 0x1120);
                        //zfDelayWriteInternalReg(dev, 0x1c5960, 0x9b40);
                        //zfDelayWriteInternalReg(dev, 0x1c6960, 0x9b40);
                        //zfDelayWriteInternalReg(dev, 0x1c7960, 0x9b40);
                        if ((hpPriv->eepromImage[0x100+0x110*2/4]&0xff) == 0x80) //FEM TYPE
                        {
                            if (hpPriv->hwFrequency <= ZM_CH_G_14)
                            {
                                zfDelayWriteInternalReg(dev, 0x1c8960, 0x9b49);
                            }
                            else
                            {
                                zfDelayWriteInternalReg(dev, 0x1c8960, 0x0900);
                            }
                        }
                        else
                        {
                            zfDelayWriteInternalReg(dev, 0x1c8960, 0x9b40);
                        }
                        polluted = 1;
                    }
                }

            }
        }

        if (hpPriv->usbAcSendBytes[3] > (hpPriv->usbAcSendBytes[0]*2))
        {
            zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC1_AC0_TXOP, hpPriv->txop[3]);
            polluted = 1;
        }
        else if (hpPriv->usbAcSendBytes[2] > (hpPriv->usbAcSendBytes[0]*2))
        {
            zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC1_AC0_TXOP, hpPriv->txop[2]);
            polluted = 1;
        }
        else if (hpPriv->usbAcSendBytes[1] > (hpPriv->usbAcSendBytes[0]*2))
        {
            zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC0_CW, hpPriv->cwmin[1]+((u32_t)hpPriv->cwmax[1]<<16));
            polluted = 1;
        }
        else
        {
            if (hpPriv->slotType == 1)
            {
                if ((wd->sta.enableDrvBA ==0) //Marvel AP
                   && (hpPriv->aggMaxDurationBE > 2000)) //BE TXOP > 2ms
                {
                    zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC0_CW, (hpPriv->cwmin[0]/2)+((u32_t)hpPriv->cwmax[0]<<16));
                }
                else
                {
                    zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC0_CW, hpPriv->cwmin[0]+((u32_t)hpPriv->cwmax[0]<<16));
                }
                polluted = 1;
            }
            else
            {
                /* Compensation for 20us slot time */
                //zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC0_CW, 58+((u32_t)hpPriv->cwmax[0]<<16));
                zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC0_CW, hpPriv->cwmin[0]+((u32_t)hpPriv->cwmax[0]<<16));
                polluted = 1;
            }

            if ((wd->sta.SWEncryptEnable & (ZM_SW_TKIP_ENCRY_EN|ZM_SW_WEP_ENCRY_EN)) == 0)
            {
                zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC1_AC0_TXOP, hpPriv->txop[0]);
                polluted = 1;
            }
            else
            {
                zfDelayWriteInternalReg(dev, ZM_MAC_REG_AC1_AC0_TXOP, 0x30);
                polluted = 1;
            }

        }
        hpPriv->usbAcSendBytes[3] = 0;
        hpPriv->usbAcSendBytes[2] = 0;
        hpPriv->usbAcSendBytes[1] = 0;
        hpPriv->usbAcSendBytes[0] = 0;
    }

    if (polluted == 1)
    {
        zfFlushDelayWrite(dev);
    }

    return;
}

/*
 *  0x1d4008 : AHB, DAC, ADC clock selection
 *             bit1~0  AHB_CLK : AHB clock selection,
 *                               00 : OSC 40MHz;
 *                               01 : 20MHz in A mode, 22MHz in G mode;
 *                               10 : 40MHz in A mode, 44MHz in G mode;
 *                               11 : 80MHz in A mode, 88MHz in G mode.
 *             bit3~2  CLK_SEL : Select the clock source of clk160 in ADDAC.
 *                               00 : PLL divider's output;
 *                               01 : PLL divider's output divided by 2;
 *                               10 : PLL divider's output divided by 4;
 *                               11 : REFCLK from XTALOSCPAD.
 */
void zfSelAdcClk(zdev_t* dev, u8_t bw40, u32_t frequency)
{
    if(bw40 == 1)
    {
        //zfDelayWriteInternalReg(dev, 0x1D4008, 0x73);
        zfDelayWriteInternalReg(dev, ZM_MAC_REG_DYNAMIC_SIFS_ACK, 0x10A);
        zfFlushDelayWrite(dev);
    }
    else
    {
        //zfDelayWriteInternalReg(dev, 0x1D4008, 0x70);
        if ( frequency <= ZM_CH_G_14 )
        {
            zfDelayWriteInternalReg(dev, ZM_MAC_REG_DYNAMIC_SIFS_ACK, 0x105);
        }
        else
        {
            zfDelayWriteInternalReg(dev, ZM_MAC_REG_DYNAMIC_SIFS_ACK, 0x104);
        }
        zfFlushDelayWrite(dev);
    }
}

u32_t zfHpEchoCommand(zdev_t* dev, u32_t value)
{
    u32_t cmd[2];
    u16_t ret;

    cmd[0] = 0x00008004;
    cmd[1] = value;

    ret = zfIssueCmd(dev, cmd, 8, ZM_CMD_ECHO, NULL);
    return ret;
}

#ifdef ZM_DRV_INIT_USB_MODE

#define ZM_USB_US_STREAM_MODE               0x00000000
#define ZM_USB_US_PACKET_MODE               0x00000008
#define ZM_USB_DS_ENABLE                    0x00000001
#define ZM_USB_US_ENABLE                    0x00000002

#define ZM_USB_RX_STREAM_4K                 0x00000000
#define ZM_USB_RX_STREAM_8K                 0x00000010
#define ZM_USB_RX_STREAM_16K                0x00000020
#define ZM_USB_RX_STREAM_32K                0x00000030

#define ZM_USB_TX_STREAM_MODE               0x00000040

#define ZM_USB_MODE_CTRL_REG                0x001E1108

void zfInitUsbMode(zdev_t* dev)
{
    u32_t mode;
    zmw_get_wlan_dev(dev);

    /* TODO: Set USB mode by reading registery */
    mode = ZM_USB_DS_ENABLE | ZM_USB_US_ENABLE | ZM_USB_US_PACKET_MODE;

    zfDelayWriteInternalReg(dev, ZM_USB_MODE_CTRL_REG, mode);
    zfFlushDelayWrite(dev);
}
#endif

void zfDumpEepBandEdges(struct ar5416Eeprom* eepromImage);
void zfPrintTargetPower2G(u8_t* tPow2xCck, u8_t* tPow2x2g, u8_t* tPow2x2gHt20, u8_t* tPow2x2gHt40);
void zfPrintTargetPower5G(u8_t* tPow2x5g, u8_t* tPow2x5gHt20, u8_t* tPow2x5gHt40);


s32_t zfInterpolateFunc(s32_t x, s32_t x1, s32_t y1, s32_t x2, s32_t y2)
{
    s32_t y;

    if (y2 == y1)
    {
        y = y1;
    }
    else if (x == x1)
    {
        y = y1;
    }
    else if (x == x2)
    {
        y = y2;
    }
    else if (x2 != x1)
    {
        y = y1 + (((y2-y1) * (x-x1))/(x2-x1));
    }
    else
    {
        y = y1;
    }

    return y;
}

//#define ZM_ENABLE_TPC_WINDOWS_DEBUG
//#define ZM_ENABLE_BANDEDGES_WINDOWS_DEBUG

/* the tx power offset workaround for ART vs NDIS/MDK */
#define HALTX_POWER_OFFSET      0

u8_t zfInterpolateFuncX(u8_t x, u8_t x1, u8_t y1, u8_t x2, u8_t y2)
{
    s32_t y;
    s32_t inc;

    #define ZM_MULTIPLIER   8
    y = zfInterpolateFunc((s32_t)x<<ZM_MULTIPLIER,
                          (s32_t)x1<<ZM_MULTIPLIER,
                          (s32_t)y1<<ZM_MULTIPLIER,
                          (s32_t)x2<<ZM_MULTIPLIER,
                          (s32_t)y2<<ZM_MULTIPLIER);

    inc = (y & (1<<(ZM_MULTIPLIER-1))) >> (ZM_MULTIPLIER-1);
    y = (y >> ZM_MULTIPLIER) + inc;
    #undef ZM_MULTIPLIER

    return (u8_t)y;
}

u8_t zfGetInterpolatedValue(u8_t x, u8_t* x_array, u8_t* y_array)
{
    s32_t y;
    u16_t xIndex;

    if (x <= x_array[1])
    {
        xIndex = 0;
    }
    else if (x <= x_array[2])
    {
        xIndex = 1;
    }
    else if (x <= x_array[3])
    {
        xIndex = 2;
    }
    else //(x > x_array[3])
    {
        xIndex = 3;
    }

    y = zfInterpolateFuncX(x,
            x_array[xIndex],
            y_array[xIndex],
            x_array[xIndex+1],
            y_array[xIndex+1]);

    return (u8_t)y;
}

u8_t zfFindFreqIndex(u8_t f, u8_t* fArray, u8_t fArraySize)
{
    u8_t i;
#ifdef ZM_ENABLE_TPC_WINDOWS_DEBUG
    DbgPrint("f=%d ", f);
    for (i=0; i<fArraySize; i++)
    {
        DbgPrint("%d ", fArray[i]);
    }
    DbgPrint("\n");
#endif
    i=fArraySize-2;
    while(1)
    {
        if (f >= fArray[i])
        {
            return i;
        }
        if (i!=0)
        {
            i--;
        }
        else
        {
            return 0;
        }
    }
}




void zfInitPowerCal(zdev_t* dev)
{
    //Program PHY Tx power relatives registers
#define zm_write_phy_reg(cr, val) reg_write((cr*4)+0x9800, val)

    zm_write_phy_reg(79, 0x7f);
    zm_write_phy_reg(77, 0x3f3f3f3f);
    zm_write_phy_reg(78, 0x3f3f3f3f);
    zm_write_phy_reg(653, 0x3f3f3f3f);
    zm_write_phy_reg(654, 0x3f3f3f3f);
    zm_write_phy_reg(739, 0x3f3f3f3f);
    zm_write_phy_reg(740, 0x3f3f3f3f);
    zm_write_phy_reg(755, 0x3f3f3f3f);
    zm_write_phy_reg(756, 0x3f3f3f3f);
    zm_write_phy_reg(757, 0x3f3f3f3f);

#undef zm_write_phy_reg
}



void zfPrintTp(u8_t* pwr0, u8_t* vpd0, u8_t* pwr1, u8_t* vpd1)
{
    #ifdef ZM_ENABLE_TPC_WINDOWS_DEBUG
    DbgPrint("pwr0 : %d, %d, %d, %d ,%d\n", pwr0[0], pwr0[1], pwr0[2], pwr0[3], pwr0[4]);
    DbgPrint("vpd0 : %d, %d, %d, %d ,%d\n", vpd0[0], vpd0[1], vpd0[2], vpd0[3], vpd0[4]);
    DbgPrint("pwr1 : %d, %d, %d, %d ,%d\n", pwr1[0], pwr1[1], pwr1[2], pwr1[3], pwr1[4]);
    DbgPrint("vpd1 : %d, %d, %d, %d ,%d\n", vpd1[0], vpd1[1], vpd1[2], vpd1[3], vpd1[4]);
    #endif
}


/*
 * To find CTL index(0~23)
 * return 24(AR5416_NUM_CTLS)=>no desired index found
 */
u8_t zfFindCtlEdgesIndex(zdev_t* dev, u8_t desired_CtlIndex)
{
    u8_t i;
    struct zsHpPriv* hpPriv;
    struct ar5416Eeprom* eepromImage;

    zmw_get_wlan_dev(dev);

    hpPriv = wd->hpPrivate;

    eepromImage = (struct ar5416Eeprom*)&(hpPriv->eepromImage[(1024+512)/4]);

    //for (i = 0; (i < AR5416_NUM_CTLS) && eepromImage->ctlIndex[i]; i++)
    for (i = 0; i < AR5416_NUM_CTLS; i++)
    {
        if(desired_CtlIndex == eepromImage->ctlIndex[i])
            break;
    }
    return i;
}

/**************************************************************************
 * fbin2freq
 *
 * Get channel value from binary representation held in eeprom
 * RETURNS: the frequency in MHz
 */
u32_t
fbin2freq(u8_t fbin, u8_t is2GHz)
{
    /*
     * Reserved value 0xFF provides an empty definition both as
     * an fbin and as a frequency - do not convert
     */
    if (fbin == AR5416_BCHAN_UNUSED) {
        return fbin;
    }

    return (u32_t)((is2GHz==1) ? (2300 + fbin) : (4800 + 5 * fbin));
}


u8_t zfGetMaxEdgePower(zdev_t* dev, CAL_CTL_EDGES *pCtlEdges, u32_t freq)
{
    u8_t i;
    u8_t maxEdgePower;
    u8_t is2GHz;
    struct zsHpPriv* hpPriv;
    struct ar5416Eeprom* eepromImage;

    zmw_get_wlan_dev(dev);

    hpPriv = wd->hpPrivate;

    eepromImage = (struct ar5416Eeprom*)&(hpPriv->eepromImage[(1024+512)/4]);

    if(freq > ZM_CH_G_14)
        is2GHz = 0;
    else
        is2GHz = 1;

    maxEdgePower = AR5416_MAX_RATE_POWER;

    /* Get the edge power */
    for (i = 0; (i < AR5416_NUM_BAND_EDGES) && (pCtlEdges[i].bChannel != AR5416_BCHAN_UNUSED) ; i++)
    {
        /*
         * If there's an exact channel match or an inband flag set
         * on the lower channel use the given rdEdgePower
         */
        if (freq == fbin2freq(pCtlEdges[i].bChannel, is2GHz))
        {
            maxEdgePower = pCtlEdges[i].tPower;
            #ifdef ZM_ENABLE_BANDEDGES_WINDOWS_DEBUG
            zm_dbg(("zfGetMaxEdgePower index i = %d \n", i));
            #endif
            break;
        }
        else if ((i > 0) && (freq < fbin2freq(pCtlEdges[i].bChannel, is2GHz)))
        {
            if (fbin2freq(pCtlEdges[i - 1].bChannel, is2GHz) < freq && pCtlEdges[i - 1].flag)
            {
                maxEdgePower = pCtlEdges[i - 1].tPower;
                #ifdef ZM_ENABLE_BANDEDGES_WINDOWS_DEBUG
                zm_dbg(("zfGetMaxEdgePower index i-1 = %d \n", i-1));
                #endif
            }
            /* Leave loop - no more affecting edges possible in this monotonic increasing list */
            break;
        }

    }

    if( i == AR5416_NUM_BAND_EDGES )
    {
        if (freq > fbin2freq(pCtlEdges[i - 1].bChannel, is2GHz) && pCtlEdges[i - 1].flag)
        {
            maxEdgePower = pCtlEdges[i - 1].tPower;
            #ifdef ZM_ENABLE_BANDEDGES_WINDOWS_DEBUG
            zm_dbg(("zfGetMaxEdgePower index=>i-1 = %d \n", i-1));
            #endif
        }
    }

    zm_assert(maxEdgePower > 0);

  #ifdef ZM_ENABLE_BANDEDGES_WINDOWS_DEBUG
    if ( maxEdgePower == AR5416_MAX_RATE_POWER )
    {
        zm_dbg(("zfGetMaxEdgePower = %d !!!\n", AR5416_MAX_RATE_POWER));
    }
  #endif
    return maxEdgePower;
}

u32_t zfAdjustHT40FreqOffset(zdev_t* dev, u32_t frequency, u8_t bw40, u8_t extOffset)
{
    u32_t newFreq = frequency;

	if (bw40 == 1)
	{
        if (extOffset == 1)
        {
            newFreq += 10;
        }
        else
        {
            newFreq -= 10;
        }
	}
	return newFreq;
}

u32_t zfHpCheckDoHeavyClip(zdev_t* dev, u32_t freq, CAL_CTL_EDGES *pCtlEdges, u8_t bw40)
{
    u32_t ret = 0;
    u8_t i;
    u8_t is2GHz;
    struct zsHpPriv* hpPriv;

    zmw_get_wlan_dev(dev);

    hpPriv = wd->hpPrivate;

    if(freq > ZM_CH_G_14)
        is2GHz = 0;
    else
        is2GHz = 1;

    /* HT40 force enable heavy clip */
    if (bw40)
    {
        ret |= 0xf0;
    }
#if 1
    /* HT20 : frequency bandedge */
    for (i = 0; (i < AR5416_NUM_BAND_EDGES) && (pCtlEdges[i].bChannel != AR5416_BCHAN_UNUSED) ; i++)
    {
        if (freq == fbin2freq(pCtlEdges[i].bChannel, is2GHz))
        {
            if (pCtlEdges[i].flag == 0)
            {
                ret |= 0xf;
            }
            break;
        }
    }
#endif

    return ret;
}


void zfSetPowerCalTable(zdev_t* dev, u32_t frequency, u8_t bw40, u8_t extOffset)
{
    struct ar5416Eeprom* eepromImage;
    u8_t pwr0[5];
 /*
 * Copyr1ght (c) 2007-2vpdight (c) 2007-2vpd Atheros Communica_chain1[128ission to use, copy, 3odify, and/o16_t boundary1 = 18; //CR 667ftware for powerTxMax = 63 with o79(c) 2007-2i (c) 2struct zsHpPriv* hight  (c) 2007-2fbin (c) 2007-2index, max2gIppear in 5ll cop (c) 2007-2opy, 0pwrPdgight (c) 2007-2 IS PRvpdDED "AS IS" AND THE AUTHOVIDED Atheros CommunE AUTHOR DISTH REGARD TO THIS SO2OVIDED "AS IS" AND THE AUT2OR DISCLAIMS ALL WARRANTIED WARRAINCLUDING ALL IMPLIEDTWARE INCLUDING ALL ImissArray[fy, (c) 2/* 4 CTL */GARD TO THItl_he abov * Codesired_Ctl
 * THEQUENTIAL DAMAEdgesMaxPs heCCK = AR5416_MAX_RATE_POWEREGARD TO THI FROM LOSS OF U2G DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACTHT20, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 4 OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THI5T, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT 5F
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIexteFTWARE.
 */
#include "../802 IN AN
 * ACTOffsetDKFwImazmw_get_wlan_dev(dev)DKFwImatice a = wd->tice aateDKFwImaeepromImage = (e
 * coar OR Etern *)&(tice a->xtern const[(1024+512)/4]
extern // Check the total bytes ofFwBufEEPROMve
 * curufIm seeFwBufdongle have been calibrated or not.(c) 2if (xtern const->baseEepHeader.length == 0xffff)(c) 2{(c) 2ueue#ifdef ZM_ENABLE_BANDEDGES_WINDOWS_DEBUGCmdQueue(zm_dbg(("Warning! Thisextern cnot u32_t zcP2FwIma\n")) (c) 2eue(zendiff);
exterretursion no}KFwImazdev_t* dev);
extTPC zfIssueCmd(zdev_t*DbgPrint("-ternzfSet OF USalTable : frequency=%dterna\n",2_t addr, buf);
en void zfId/* TODO : 1. any
 * purand is hereby gshould be referedImagCR667n voiCR79NSEQUENT/*;
exterern cn otus.ini filed(zdev_t* du32_t offset);
extern u16_t zfF*/ u32_t* rsp, u16_t rspLen);
extern u16_t zfDe/* 2. Interpolateopyrn voivpd tese isints from2_t addr, NSEQUENTlayWriteIncalFreqPilude : %d,usy(u32_t c ,sy(u32_t ctlBul);
f);
exteroid zfCoreCwmBusy(zdev_t* dev, u16_tt u32_t zcP2FzfCwmIsExtCha[0]*5+4800
extern void zfCoreCwmBusy(zdev_t* dev, u16_t busy);

/* Prototypes */
vo1d zfInitRf(zdev_t* dev, u32_t frequency);
void zfInitPhy(zdev_t* dev, u32_t frequ2d zfInitRf(zdev_t* dev, u32_t frequency);
void zfInitPhy(zdev_t* dev, u32_t frequ3d zfInitRf(zdev_t* dev, u32_t frequency);
void zfInitPhy(zdev_t* dev, u32_t frequ4d zfInitRf(zdev_t* dev, u32_t frequency);
void zfInitPhy(zdev_t* dev, u32_t frequ5d zfInitRf(zdev_t* dev, u32_t frequency);
void zfInitPhy(zdev_t* dev, u32_t frequ6d zfInitRf(zdev_t* dev, u32_t frequency);
void zfInitPhy(zdev_t* dev, u32_t frequ7d zfInitextern void zfCoreCwmBusy(zdev_t* dev, u16_t buf);
eern u16_t zfCwmIsExtC2anBusy(u32_t ctlBuy);
extern void zfCoreCwmBusy(zdev_t* dev, u16_t busy);

/* Prototypes */2void+23itRf(zdev_t* dev, u32_t frequency);
void zfInitPhy(zdev_t* dev, u32_t fr2quen     int* delta_slope_coeff_man,
                            int* delta_slope_2oeff_exp_shgi,
                            int* delta_slope_coeff_man_shgi);

v3oeff_eParam(zdev_t* dev,
                            u32_tn void zfIdconszdev_t* de< 3000 zfInitCmdQueue(for (i=0; i<4; i++ zfInifInitCmdQueue(ern const u32_t zcP2F* delta_slope_i]xtern vo
#define Zne ZM_FIRMWARE_WLA
/* breakuf);
exter dev, oid zfCoFPGA test in all cop =the abov32_t* rsp, u16_t rspLen);
extern u16_t zfDe32_t frequency ZM_FPGA_PHBusyl);
ein all copbuf);
extern void zfIdlRspmiss u32 * C)d->hpPrivat-     buf);
exterappea = zfFindwmIsl cop(miss,           int* delta_slope#define zm_min(A, B) ((Areg_write(addr, val) zfDelayWriteInternalReg(dev, ad2G*******, val)
#dappeabuf);
exterern u16_t zpy,  0 */
  _t* uf);
extern void zfIdlRspzfWriteTp(&DR           0x2ExtCData     [appea].OVIDED
   0]
extern void zfCoreCapability = ZM_HP_CAP_11N;

    ((struOR DISpPriv*)wd->hpPrivate)->hwFrequency = 0;
    ((struct zsHpPriv*)wct zsHp1riv*)wd->hpPrivate)->hwFrequency = 0;
    ((struct zsHpPriv*)wd->hpPr = 0;extern void zfCoreC6_t ret;
    zmw_get_wlan_dev(dev);

    /* Initializa HAL Plus HpPriv;

    +1((struct zsHpPriv*)wd->hpPrivate)->halCapability = ZM_HP_CAP_11N;

    ((st+1ruct zsHpPriv*)wd->hpPrivate)->hwFrequency = 0;
    ((struct zsHpPriv*riv*d->hpPrivate)->hwBw40 = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->hwriv*)wd->hp = 0;

    ((struct zsHpPriv*)wd->hpPrivate)->disableDfsCh = vate)->slot(struct zsHpPriv*)wd->hpPri struct zsHpPriv zgHp5riv;

#define ZM_FIRMWARE_WLA IS PROVIDED "ne Z zfen, u32_t oFuncXHpInitextern void zfCoreCwmDR           0x200000
#defin((strte)->remainBuf = NULL;
    ((struct zs_CAP_11N;

    ((struct zsHpPriiivate)->usbRxRemainLen = 0;
    ((strucHpPriv*)wd->hpPr+1ivate)->usbRxRemainLen = 0;
    ((struct zsHpPriv*)wd->hpPriv*)wd->hpPriiuct zsHpPriv*)wd->hphpPrivate)->lSTREAM_MODOR DISCLstruct zsHpPriv*)wd->hpPrivate)->remainBuf = NULL;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxRemainLen = 0;
    ((struct zsHpPriv*)wd->hpPrid->hpPrivaRxPktLen = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxPadLen = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxTransfoBBHeavyClipendif

    ((struct zsHpPriv*)wd->hpPrivatVENT SHAstruct zsHpPriv*)wd->hpPrivate)->remainBuf = NULL;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxRemainLen = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->1sbRxPktLen = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxPadLen = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxTransferLen =fFirmendif

    ((struct zsHpPriv*)wd->hpPrivate)->en
#if ZM_SW_LOOP_BACK != 1

    /* TODO : [Download FW] */
    if (wd->modeMDKEnable)
    {
        /* download the MDK firmware */
        if ((  ((structip     = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->setValueHeavyClip = 0;


    /* Initialize driver core */
    zfInitCmdQueue {
            /* TODO : excepPriv*)wd->hpPrivaD WARRANTstruct zsHpPriv*)wd->hpPrivate)->remainBuf = NULL;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxRemainLen = 0;
    ((struct zsHpPriv*ware>hpPrivate)->usbRxPktLen = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxPadLen = 0;
    ((struct zsHpPriv*)wd->hpPrivate)-, (u32_tansferLen = 0;
#endif

    ((struct zsHpPriv*)wd->hpPrivaABILITY Arn 1;
        }
    #else

        // 1-PH fw: ReadMac() store some global variable
        if ((ret = zfFirmwareDownloadNotJump(dev, (u32_t*)oBBHeavyClip     = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->setValueHeavyClip = 0;


    /* Initialize driver core */FwBufImage fmdQueue(dev);

    /* Initialize USB */
    zfUsbInitEVENT SHArn 1;
        }
    #else

        // 1-PH fw: ReadMac() store some global variable
        if ((ret = zfFirmwareDownloadNotJump(dev, (u32_t*)zcFwBufwareDownload(dev, (u32_t*)zcFwImage,
                (u32_t)zcFwImageSize, ZM_FIRMWARE_WLAN_ADDR)) != ZM_SUCCESFwBufImage failed!" {
            /* TODO : exception handling */IABLE FOR((ret = zfFirmwareDownload(dev, (u32_t*)zcFwImage,
                (u32_t)zcFwImageSize, ZM_FIRMWARE_WLAN_ADDR)) != ZM_SUCCESS)
        {
            ((ZM_SW_LOOP_BACK != 1) && (ZM_DRIVER_MODEL_TYPE_MDK !=1))
#if ZM_SW_LOOP_BACK != 1
    if(!wd->modeMDKEnable)
    {
     
            /* TODO : exception hanGA test */
#deFPGA telsestatic struct zsHpPriv zgHp8riv;

#define ZM_FIRMWARE_WLAN_ADDR           0x200000
#d5fine ZM_FIRMWARE_SPI_ADDR      0x114000
/* 0: real chip     1: FPGA test */
#define ZM *
 * THY  0

#define reg_write(addr, val) zfDelayWriteInternalReg(dev, addr+einit(zd, val)
#defi *
 * Tin(A, B) ((A>B)? B:A)


/********************* IntialInit)/5ion ********************/
u16_t zfHpInit(zdev_t* dev, u32_t frequ5ncy)
{pPriv*)wd->hpPrivatreg_write(addr, val) zfDelayWriteInternalReg(dev, ad5rivate variables */
    wd->hpPrin void )->eepromImageRdReq     = 0;
#ifdef ZM_OTUS_RX_STREAM_MODE
    ((struct zsHpPriv*)wd->hpPrivate)->remainBuf = NULL;
    ((struct zsHpPriv*)TYPEhpPrivate)->usbRxRemainLen = 0;
    ((struct zsHpPr
void(u32_t*)zcFwBufImage,
                (u32_t)zcFwBufImageSize, 0x1 ((strucRxPadLen = 0;
    ((struct zsHpPriv*)wd->hpPrivat
    ((struansferLen = 0;
#endif

    ((struct zsHpPriv*)wd->hpPrivate)->enableBBHeavyClip = 1;
    ((struct zsHpPriv*)wd->hpPrivate)->hwBBHeavyClip    ((struct zsHpPriv*)wd->hpPrivate)->usbRxRemainLen = 0;
    ((structoBBHeavyClip     = 0;
    ((struct zsHpPriv*)wd->hpPrivatesHpPriv*)wd->hpPrivate)->usbRxPadLen = 0;
    ((struct zsHpPriv*)wd->hpPmdQueue(dev);

    /* Initialize USB */
    zfUsbInit(dev);

#if ZM_SW_LOOP_BACK != 1

    /* TODO : [Download FW] */
    if (wd->modeMD ((struct zsHpPriv*)wd->hpPrivate)->usbRxRemainLen = 0;
    ((struct zsHpPrwareDownload(dev, (u32_t*)zcFwImage,
              sHpPriv*)wd->hpPrivate)->usbRxPadLen = 0;
    ((struct zsHpPriv*)wd->hpPrivate) {
            /* TODO : exception handling */
            //return 1;
        }
    }
    else
    {
    #ifndef ZM_OTUS_LINUX_Pd firmware */
    if ((ret = zfFirmwareDownload(dev, (u32_t*)zcFwImage,
          //return 1;
    }
    #endif

#ifdef ZM_DRV_INIT_USB_MODE
    /* Init USB Mode */
    zfInitUsbMode(dev);

    /* Do
            /* TODO : exception handling */
            //return 1;
        }
    #else

        // 1-PH fw: ReadMac() store some global ((struct zsHpPriv*)wd->hpPrivate)->usbRxRemainLen = 0;
  , (u32_t*)zcFwBufImage,
                (u32_t)zcFwBufImageSize, 0x1sHpPriv*)wd->hpPrivate)->usbRxPadLen = 0;
    ((struct zsHpPFwBufImage failed!");
        }

        zfwSleep(dev, 1000);

        if ((ret = zfFirmwareDownload(dev, (u32_t*)zcFwImage,
                (u32_t)UsbFree(dev);

    return 0;
}

/* MDK mode setting for dontRetransmge,
            (u32_t)zcFwImageSize, ZM_FIRMWARE_WLAN_ADDR)) != ZM_SUCCESS)
    {
        /* TODO : exception handlint USB Mode */
    zfInitUsbMode(dev);

    /* Do the USB Reset */
    zfHpUsbReset(dev);
#endif

/* Register setting */
/* ZM_DRIVER_MODEL_TYPE_MDK
UsbFree(dev);

    return 0;
}

/* MDK mode setting for dontRetransmit */
v       //return 1;
    }
    #endif

#ifdef ZM_DRV_INIT_USB_MODE
    /* Init USB Mode */
    zfInitUsbMode(dev)
    {
        /* Init MAC */
        zfInitMac(dev);

    #if ZM_FW_LOOP_BACK != 1
        /* Init PHY */
        zfInitPhy(dev, frequency, 0);

   = zfIssueCmd(dev, cmd, 12, ZM_OID_INTERNAL_WRITE, 0);
}

const u8_t zcXpdTes;
    u16_t modesIndex = 0;
    u16_t freqIndex = 0;
    u32_t tmp, tmp1;
    struct zsHpPriv* hpPriv;

    u32_t eepromBDelayWrite(dev);
        #endif

    #endif /** dev, ufw, u32_CpPriv1areDownloadGeCopyrt);
extern void zfUsbFree(zdev_t* dev);
extImageRdReq     = 0;
#ifdtCmdQueue(pyrigstrucAM_MODE
    ((st>>1   #endif icatic864,    0,     if ((   #endif 008 Ac864,    0,      0,      0,      0,  0,      0,             t* dev, 32_t* rsp, u16_t rspLen);
extern u16_t zfDelayWriteInT voiP zfUs((struct zlayWriteInpyrinBusy(u32_t ctlBusy, l);
epyrig0]  0,   1  0},
  2  0},
  3  0},
  ern c        {0xc20icat  0,      0,      0,      icati   0icati    icati9920icati0,  icati,      0,      0,  008   0,      0,      0,      0, 1    0},
1       {1x9920,  1 0,     10,      0,      0,     ,      0,      0,      0},
{0xb8 0,    zmw_gehe re,      ev);
 giste,      0,ushDelayWrite(GeneFwImFwBufxteraIRECs0,      0,      0,  any
 * pu+1+6   0,      0},
      e, copy, mostruct Geten, u32_t odValue(i, &0,      0&      0     0,    0,sHpPrzgHpis hereby * SeSetup the indices for the next set of register array writes
   -6-6  * PHe of t is e ofic20 / 2040
 * rsp, u16_t rspLen);
extern u16_t zfDelayWriteIne, copy, m((struct zsHpPriv zgHpquency is 2.4GHz (B)=1)
static struct zlayWriteInsy(u32_t ctlBusy, u32_t extBusffset,
                      he next set + 0,           zm_lated R       zm_,             zm_r setti       zm_4ivate)->usbRxRemain  }
    else
5
    }
    else
6
    }
    else
7
    }
    else
8
    }
    else
9       freqIndeshDelayWrite(Write PHY regs 672-7030,      0,      0,  12dif /=4 zfInitCmdQueue(u32_tar54Addr ern 9800 + (672 * 4    #endif   {
  valandling */
val u32(  {
 )0");
        }
<<24) |extern void zfCorModes in 4: G-20");
 2]<<16 }
    }


#if ZM_FPGA_PHY == 1
    /* St1]<<8 }
    }


#if ZM_FPGA_PHY == 1
    /* Srn const qIndex nv_t* deOTUS_LINUX_PHASE_2 zfIdlRsp(zg_w0("i(        + i, ar5);9834, R = 4;EQUENT    //((struct ,
     34,    0,     */
      0,      0,      0},
        {0x9828,    0,      0,      0,      0,      0},
        {0xc864,    0   //return    0,      0,      0},
          if ((,    0,      0,      0,  
    zfHpUs 0},
        {0xb848,    0, _FW_LOOP_BA  0,      0,      0},
        {0xa20c,    0,      0,      0,      0,      0},
        {0xc20c,    0,      0,      0,      0,      0},
        {0x9920,    0,      0,      0,      0,      0},
        {0xb920,    0,      0,      0,      0,      0},
        {0xa258,    0,      0,      0,      0,      0},
    };

    zmw_get_wlan_dev(dev);
    hpPriv=wd->hpPrivate;

    /* #1 Save the initial value of the related RIFS register settings */
    //((struct zsHpPriv*)wd->hpPrivate)->isInitialPhy++;

    /*
     * Setup the indices for the next s3t of register array writes
     * PHY mode is static20 / 2040
     * Frequency is 2.4GHz (B) / 5GHz (A)
     */
    ifThere */
            /* is an i {
        /* 5GHz */
        freqIndex  = 1;
        if (bw40)
        {
            modesInde3 = 2;
            zm_debug_msg0("init ar5416Modes in 2: A-20/40");
        }
        else
        {
            modesIndex = 1;
           3zm_debug_msg0("i    ar5416Modes i     A-20");
        }
    }
    e        {
        /* 2.4GHz */
        freqIndex      
        if (    )
        {
            modesI     = 3;
            zm_ddebug_msg0("init ar5416Modes in+ 0x1000n 3: G-20/40");
        }
        else
        {
            modesIndex = 4;
          }          zm_debug_msg0("init ar5416Modes in 4: G-20"it = 1     }
    }


#if ZM_FPGA_PHY == 1
    /te)->hting External Hainan Register InitializatPriv*)
    /* TODO: */

    zfwSleep(dev, 10);Therhandling */
     *Set correct Baseband to analog shift setting to access analog chips.
     */
    //reg_write(PHY_BASzfFlushDelayg0("iize;
extern /* 3.HpPriv*)wd-arg 0, s he tev, u   */
 *)wd->hpPrivate)->x)
static struct zsHpPriv zgHp3riv;

#define ZM_FIRMWARE_WLAN_ADDR           0x2Ty ==  OF USck[i].bChannel !M_FIRMWARE_SPI_ADDR      0x114000
/* 0AL, DIRECTstruc              ((struct zsHpPriv*)wd->hpPril chip     1: FPGA test f ZM_FW_LOOP_             0x114000
/* 0: real chip     1: FPPGA test */
#define*************/
u16_t zfHpInit(AL, DIREC, i= 0;

#ifdef ZM_OTUS_RX_STREAM_MODE
    if (((struct zsHpPriv*)wSE, struc=e)->remainBuf != NULL)
    {
 struct zsHpPriv zgHpPriv;

#define ZM_FIRMWARE_WLAifdef ZMt zs2xPriv*)ruct zsHpPriv*)wd->hpPrivate)->remainBuf = NULL;
    ((struct zsstruct zsHpPriv*((strud->hpPri         case 0x9918 :
                    ((struct zsHpPrivx];
  reDownload(dev, (u32_t*)zcFwImage,
           ((struct zsHpPrriv**)wd->hpPrivate)->initSearchStartDelay         = ar5416Modes[i][moriv*sIndex];
endif

    ((struct zsHpPriv*)wdnitAGC     sHpPriv zgHpPriv;

#define ZM_FIRMWARE_WLAN_ADDR           0x2struct zsHpefinewd->hpPrivate)->initDesiredSigSize           = ar5416Modes[i][modesIndex];
                zsHpPriv*)wd->            case 0x985c :
                    ((struct zsHpPriv*)wd->hpPrivate)->initAGC                      = ar5416Modes[i][modesIndex];
                    break;
                case 0x9860 :
            private (struct zsHpPriv*)wd->hpPrivate)->initAgcControl               = ar5416Modes[i][modesIndex];
  2g               break;
                case 0x9918 :
                    ((struwd->hpPri                ((struct zsHpPriv*)wd->hpPrivate)->initRIF= sizeof(asIndex];
                    break;
                case 0te)->usbRxPar5416BB_RfGain) / sizeof(*ar5416BB_RfGain);
    for (i=0; i<entrirams         = ar5416Modes[i][modesIndex];
                    break;
                case 0xa388 :
                    ((struct zs
 * HpPriv*)wd->hpPrivate)->initFastChannelChangeControl = ar5416Modes[i][modesIndex];
             y radio dependen
                    break;
            }
        }
    }
#if 0
    zfFlushDelayWrite(dev);

    /*
     * Common Register setting
     */
    entries = sizeof(ar5416Common) / sizeof(*ar5416Common);
    for (i=0;
 * O i<entries; i++)
    {
        reg_write(ar5416Common[i]dif /* end of ((ZM_SW_LOOP_BAC}
    zfFlushDelHtradiorite(dev);

    /*
     * RF Gain setting by freqIndex
     */
    entries = y radizeof(ar5416BB_RfGain) / sizeof(*ar5416BB_RfGain);
    for (i=0; ita[1][1] = t; i++)
    {
        reg_write(ar5416BB_RfGain[i][0], ar5416ta[1][1] =ain[i][freqIndex]);
    }
    zfFlushDelayWrite(dev);

    /*
 2][1] = tmp;
 d ar5416InitChainMask() here to ensure the swap bit is set before
     * the pdadc table is written.  Swap must occur before any 4adio dependent
     * replicated register access.  The pdadc curve addressing in particular
     *  (tmp << 7);
#            case 0x985c :
                    ((struct zsHpPriv*)wd->hpPrivate)->initAGC                      = ar5416Modes[i][*******zfAdjustmp) wmIstern cize;sIndex, bw40, exttern c)DO
#endif

    /* Update 5G board data */
    //Ant control common
    tmp = hpPriv4>eepromImage[0x100+0x144*2/4];
    eepromBoardData[0][1] = tmp;
    eepromBoardData[0][2] = tmp;
    //Ant (tmpruct zsHpPriv*)wd->hpPextern void zfCoreCwmp = hpPriv->eepromImage[0x100+0x148*2/4];
    tmp = (tmp BoardData[2][2] = tmp;
    //SwSettle
    tmp = hpPr (tm1] = tmp;
    eepromBoardData[1][2] = tmp;
    //Ant control chain 0x14a*2/4]= hpPriv->eepromImage[0x100+0x142*2/4];
    eepromBoardData[2]0x14a*2/p;
    eepromBoardData[2][2] = tmp;
    //SwSettle
    tmp = hpPr (tmp<<16) +         = ar5416Modes[i][modesIndex];
             ate)->ha][0], ar5416#ifdef ZMx];
     , TxFrameToXpaOn
 }
    zfFlushDelepromBoardData[6][1] &= (~((u32_t)nt c0xff0000));
    eepromBoardData[6][14tion *** of ZM_FW_LOOP_BACK != 1 /* 5G    */
    /*/
    }
#endif /* end of ((ZM_SW_LOOP_BACK != 1) && (ZM_DRIVE[0x100+0x14TYPE_wd->hpPrivate)->initDesiredSigSize           = ar5416Modes[i][modesIndex];
                c*2/4];
    tm            case 0x985c :
                    ((struct zsHpPriv*)wd->hpPrivate)->initAGC                      = ar5416Modes[i][modesIndex];
                    break;
                case 0x9860 :
            d->hpPri(struct zsHpPriv*)wd->hpPrivate)->initAgcControl               = ar5416Modes[i][modesIndex];
  5layWrite(dev);

    /*
     * RF Gain setting by freqIndex
     */
    entries  ((struct                ((struct zsHpPriv*)wd->hpPrivate)->initRIF  //TxRxAtsIndex];
                    break;
                case 0sHpPriv*)wdtten chain_2
    tmp = hpPriv->eepromImage[0x100+0x148*2/4] & 0x3fomImage[0x100+0x146*2/4];
    tmp = (tmp >> 16) & 0x7f;
    eepromBo<16);
    //Thresh62
    tmp = hpPriv->eepromImage[0x100+0x14c*y radio dependent
     * replicated register access.  The pdadc curve addressing in particular
   tmp >> 8) & 0x3f;
[1] |= (tmp<<12);
    eepromBoardData[7][2] &= (~((u32_t)0x7f000));
    eepromBoardData[7][2] |= (tmp<<12);
    //TxRxAtten chain_0
    tmp = hpPriv->eepromImage[0x100+0x146*2/4];
    tmp = (tmp >> 24) & 0x3f;
  iv->eepromImage[0x100+0x144*2/4];
    eepromBoardData[0][1] = tmp;
    eepromBoardData[0][2] = tmp;
    /5Ant control chain 0
    tmp = hpPriv->eepromImage[0x100+0x140*2/4];
    eepromBoardtmp >> 8TxRxAtten chain_2
    tmp = hpPriv->eepromImage[0x100+0x148*2/4] n 2
    tmp = hpPriv->eepromImage[0x100+0x142*2/4];
    eepromBoardDattmp >> 24) &ata[9][1] |= (tmp<<12);
    eepromBoardData[9][2] &= (~((u32_t)Priv->eepromImage[0x100+0x146*2/4];
    tmp = (tmp >> 16) & 0x7f;
    eepromBo0
    tmp = hpPriv->eepromImage[0x100+0x148*2/4];
    tmp = (tmp  (tmp << 7);
#if 0
    //swSettleHt40
    tmp = hpPriv->eepromImage[0x100+0x158*2/4];
    tmp = 2] |= (tmp);
    /            case 0x985c :
                    ((struct zsHpPriv*)wd->hpPrivate)->initAGC                      = ar5416Modes[i][p = hpPriv->eepromImage[0x100+0x148*2/4];
    tmp = (tmp >> 24);
    tmp1 = hpPriv->eepromImage[0x100+0x14a*2/4];
    tmp1 = tmp1 & 0x11][1   tmp = tmp + (tmp1<<8);
    eepromBoardData[4][1] &= (~((u32_t)0xffff));
    eepromBoardData[4][1] |=32_t;
    eepromBoardData[4][2] &= (~((u32_t)0xffff));
    eepromBoardData[4][2] |= tmp;
    //TxEndToXpaOff, TxFrameToXpaOn
    tmp = hpPriv->eepromImage[0x2] |= (tn_0
    tmp = hpPriv->eepromImage[0x100+0x14e*2/4];
    tmp = (tmp e[0x100+0x14c*2/4];
    tmp1 = (tmp1 >> 8) & 0xff;
    tmp = (tmp<<2mp = hpPriv- >> 8) & 0x1f;
    tmp  = (tmp<<5) + tmp1;
    eepromBoardData[12mBoardData[5][2] = tmp;
    //TxEnaToRxOm
    tmp = hpPriv->eepromImage[0x100+0x145G] &= (~((u32_t)0xf;
    tmp = (tmpepromBoardData[6][1] &= (~((u32_32_t)0[tmp] << 16);
#if 0
    //bsw_Atten [2] &= (~((  {0x9834,4.R CONSEQUENT/*e)->in   t1   0,wBufbandeOM L tx1 )
   by2_t addr, x1f;
   eprom2.4G we == 1G FROM LOSS OF USE,;
    eepromBOTUS_RX_STREACTION OF CONTRACTepromBoardData[10][2] &= (~((u32_t)0x1f));

 *     eepromBoardData[10][2] |= (tmp);
    //bs4_Margin chain_5dData[10]= (~((u32_t)0x1f))5
    eepromBoardData[10][2] |= (tmp);
    a[12][;
    eepromBoardData[11][1] &= (~((u32_t)0x30x100+0x154.2 Upd*)wd(3.)Phy == 1 )
        {by  eex1f;
    e3 T &= (~((oern c][2] ART - NDIS/MDeepromBoa4.4sg0("inMACshif 0x694] |= (CK's TPCx1f;
  x1f;
  mwareDo//zfDumpEepBandROM Lst u32_t zcP>hpPrivate)== 1wBufcfgbFree(PISize:shifionCode => RegulatoryDomPriv:     -FFC  0x30-eu 0x40-jap{
      MAGES
 * WHATSOEruct HpGet    eepromBoardDize;
ex        (ata[11][2] &= (~((ern 30) || ] |= (tmp);
#endif

   4/* Update 2.4G board data */
0) zfInitCmdQueue(/* skipR CON voiheavy cliromBoardD14][2] |= (zenev, BBH0*2/C;
  =  if (ar5416zdev_t* dev);
extern u16_t zfIssueCmd(zdev_t* dev, u32_t*     eepromBoardDa= 0,mage[0x100+0x170*2/4];
 _t* buf);
extern void zfId of ZM_FW_LOOP_BACK != 1  >> 24;
    tmp1 = hpPriv->e1andling */
cons |= (tmp);
#endif

   RMWARE_SPI_AM_FIRMWARE_WLA/*Data[11]           fny
     */
    /ardData[11][2] &= (~((u    Index];
             /* firvoidart :BoardD   */
    /*)wd->hpPrivate=* deCH_G_1      eepromImage[0x100+0x16oardD-R CO_11B (tmp1 << 8);
  AMAGE********/C FROM L_t zfH+0x14MAGES
 * WHATSOE|
    tmsHpPriv*)wd->hpif(AMAGE<TA OR PNUM_CTLSWARE_SPI_ADDR      0x114000
/* 0G FROM LOSS OF USE, DAegistMaxROM  OF Uge[0x1DR           0tl_11N[AMAGE].(~((u32_ 0,  xtern u16_t zfFl      case 0x985c :
  zdev_t* dev);
extern u16_t zfIssueCmd(zdev_t* dev dev, u32_t* 
    tmp   tmp1 ,      AMAGE buf);
exterLL)
    {
        zf00+0x16e*2/4];
    t
    //Ant co4;
    tmp1 = hpPriv->eepromImage[0x100+0x170*2/4];
    tmp G tmp + (tmp1 << 8);
    eepromBoardData[2][3] = tmp;
    eepromBoardData[2][4] = tmp;
 CT, NwSettle
    tmp = hpPriv->eepromImage[0x100+0x174*2/4];
    tmp = (tmp >> 8) & 0x7f;
    eepromBoardData[3][4] &= (~((u32_t)0x3f80));
    eepromBoardData[3][4] |= (tmp <<G7);
#if 0
    //swSettleHt40
    tmp = hpPriv->eepromImage[0x100+0x184*OF
 * O;
    tmp = (tmp >> 24) & 0x7f;
    eepromBoardData[3][3] &= (~((u32_//bsw_x3f80));
    eepromBoardData[3][3] |= (tmp << 7);
#endif
    //adcDesired, pdaDesired
   
 * OR wSettle
    tmp = hpPriv->eepromImage[0x100+0x174*2/4];
    tmp = (tmp >> 8) & 0x7f;
    eepromBoardDa                  ((struct zsHpPriv/* workarmp + sHpPno datadev,PISize, replac0x3c0normal 24];
    tmp = (tmeepromImage[0x100+0x178*2/4];(~((u32_t)0x1f));
8) & 0x7f;
    eepromBoardData[3][4] &= (~((u32_t)0x3f80));
    eepromBoardData[3][4] |= (tmp 178*2/4   eepromBoardData[4][3] &= (~((u32_t)0xffff));
    eepromBoardData[4][3] 4= tmp;
    eepromBoardData[4][4] &= (~((u32_t)0xffff));
    eepromBoardData[2] &= (~ tmp;
    //TxEndToXpaOff, TxFrameToXpaOn
    tmp = hpPriv->eepromImage[0x100+0x178*FTWARwSettle
    tmp = hpPriv->eepromImage[0x100+0x174*2/4];
    tmpextern void zfCoreCwmBusy(zdev_t* dev, u16_t (u32_t)0x7f000));
  Priv->eepromImage[0x100+0x148t addr, 4];
    tmp = (tmpleHt40
    tmp  = tmp1 & 0xff;
    tmp = (tmp << 24) + (tmp << 16) + (tmp1 << 8) + tmp1;
    eepromBoardData[5][3] = tmp;
    eepromBoardData[5][4] = tmp;
    //FTWARToRxOm
    tmp = hpPriv->eepromImage[0x100+0x178*2/4];
    tmp = (tmp >> 24);
    eepromBoardData[6][3] &= (~((u32_t40xff0000));
    eepromBoardData[6][3] |= (tmp<v->eepromImage[07a17 :00+0x + (tmp << 16) by g )
   (dBm) 8) +c>hpPrivrange when using DFS defin0x3c0madwifi 24) & 0x3f;
  sHpPriv zgHp_t zr   eepiondev, .allow->hpPriCntriv;

#define Z     ((struct zsHpPrivconsomBoardData[9][4] &= (~((u32_t)02/4] &= (~((== = (tmp >>   eepromBoardData     0x114000
/* 0|= (tmp<32_tIsDfs->hpPrige[0x1(e for)xtern u16_mage[0x100+0x176*276*2/4];
    tmp = (tmp) & dev, u3ebug_msg1("zdev_t* deuse
     -- ;
extern u16_t zfFl00));
    eepromBoarG FROM LOSS OF USE, <<18=v, umin);
 ROM LOSS OF USE,,2_t z;
    //TxRxMargin chain_0
    tmaxRegTS OF U*2leHt40
    tmp   //adcDesired, pdaDesired
    <<18);
    //TxRxMargin chain_ency tmp = hpPriv->eepromImage[0x100+0x176*2/4];
    tmp = (tmp >> 8) & 0x3f;
    eepromBoardData[11
 * O&= (~((u32_t)0xfc0000));
   
 *  eepromBoardData[11][3] |= (tmp<<18);
    eepromBoardData[11][4] &= (~((u32_t)0xfc0000));
    eeprom4oardData[11][4] |= (tmp<<18);
       tmp = hpPriv->eepromImage[0x100+0x176*2/4];
    tmp = (tmp >> 8) & 0x3f;
se 0x985c :
  000
/* 0: real chip     1: + tmp1;
    eepromnitAGC     << 16) ApplydDat mx1f)to correctPhy == 1 )
   ][2] 24) & 0x3f;
  ta[3][4] &= (~((u32_t)0x3f80));
    eepromBoardData[3][4]18);
    eeData[10][4] |= (tmp<<18=&= (xRxMargin chain_2
 leHt40
    tmp  chain_2, iqCallQ chain_2
    t11][3] Priv->eepromImage[0x120x3f80));
    ee tmp = (tmp>>24) & 0x3f;
    tmp1
 * OR iv->eepromImage[0x100ta[4][4] |= tmp;
    tmp1 = (tmp1 >> 8) & 0x1f;
    tmFTWAR(tmp<<5) + tmp1;
    eeesh62
    tmp = hn void zfIdlRsp           break;
              76*2/4];
    tmp = (tmodesIndex];
               //TodesIndex];
        v->eepromImage[0x100+0 + HALTXWHETHE_OFFSET            case 0x985c :
  }
    zfFlushDel24 = hpPrivtern c->eepromImage[|= (tmp< >> 24;
    tmp1 = hpPrivmage[0x100+0x1    eepromBoardData[2   eeprom2            case 0x985c :
                    ((struct zsHpPrivdToPd[tmp] <BoardData[14][3se 0x985c :
  2_t)0x7ff));
    eepromBoardData[13][4] |= (tmp);
1][2]d->hpPrivat== 2412* UpdaeepromImage[0x162)32_t)0xfc0000));
  /4];
    tmp = (tmp) & 0x3ivate)2_t)0xfc0000));
    eepromBoardData[10][3] |= (t}
    zfFlushDelayWritetmp = hpPriv->eepromelayWv->eepromImage[0x100-dToPd[tmp tmp = tmp & 0xf;
    DbgPrint("xpd=<5) + tmp1;
    eepromBoardDat                 ;
    eepromBoardData[10][3] |= (tmp << 10);
    eepromBoardData[10][4] &= (~((u32_t)0x3c00));
    eepta[10][4] |= (tmp << 10);
    //bsw_Atten ch4][3] &= (~((u32FlushDelayWr> 2_t)0xfc0000));
   romBoardDat2_t)0xfc0000));
    ee  eepromBoardData[10][3] |= (tx%x\n", tmp, zcXpdToPd[tmp]);
    eeprom &= (~((u32_t)0x1f))-
    eepromBoardData[10][4] |=  = (tmp >> 8) & 0x3f;
    p1;
    eepromBoardDatp1;
    eepromBoarp1;
    eepromBoar->eepromImage[0x100+0/4];
    tmp = (tmp) & &= (~((u32_t)0x1f))oardData[10][3] &= (~((u32_t)0x1f));
    eepromBoardData[10][3] |= (tmp);
    eepromBoarx7ff));
    eeprdToPd[tmp] << 16);
#if 0dif /* end of ((n chain_0
    tmp = hpPriv->eepromImage[0x100+0x184*2/4];
    tmp = (t && (i>=3tmp >> 8) & 0xf;
    eepromBoardData[10][] = tmp;
    //Ant control ardData[10][4] &= (~((nt conttmp<<5) + tmp1;
    eeprromBoardData[10][4] |= (tmp << 10);
    //bsw_AtteomBoardData[11][4] &= (~((u32_t)0x3c00));
    eepromBoardData[11][4] |= (tmp f 0
    for (j=0; j<14; j++)
    {
        DbgPrint("%04x, %08x,promImage[0x100+0x184*2/4];
    tmp = (tmp) & 0x1f;
    eepromBoardData[11][3] &= (~((u32_t)0x1f));
    eepromBoardData[11]eepromImage[0x100+0(~((u32_t)0x1f));
    eepromBoardData[11][4] |= (tmp);
#endif

#if ;
    eep (j=0; j<14; j++)
    {
 (tmpData[13][3] |= (tmp);
   %08x, %08x, %08x\n", eepromBoardData[j][0], eepromBoardData[j][1], eepr    {
            reg6write(eepromBoardData[j][0], eepromBoardData[j][modesIndex]);

            /* #1 Save the initial value of the related RIFS register );
 tern c* &=  %08x\n", eepromBoardData[j][0], eepromBoardData[j][1], eepromBoardData[j][2], eepromBoardData[j][3], eepromBoardData     /* #1 Save the initial value of the related RIFS register)wd->hpPrivate)->initDesiredSigSize           = eepromFPGA test */
#define             mImage[0x100+0x165/4];
    tA tmp;
    eepromBoardData[4][4] &= (~((u32_t)0xffff));
    eepromBoard11A tmp + (tmp1 << 8);
    eepromBoardData[2][3] = tmp;
    eepromBoardData[2][4] = tmp;
 de "hwSettle
    tmp = hpPriv->eepromImage[0x100+0x174*2/4];
    tmp = (tmp >> 8) & 0x7f;
    eepromBoardData[3][4] &= (~((u32_t)0x3f80));
    eepromBoardData[3][4] |= (tmp <<Axff0000));
    eepromBoardData[6][3] |= (tmp<<16);
    eepro      breextern  = tmp >> 24;
    tmp1 = hpPriv->eepromImage[0x100+0x170*2/4];
    tma[12][riv*)wd->hpPrivate)->initAgcControl               = eepromBoardData[j][modesIndex];
     8*2/4];
    tmp = (tmp >> 16) & 0xff;
    tmp1 = hpPriv->eepromImage[0x100+0x17a*2/4];
    tmp1 = tmp1 & 0xff;
    tmp = (tmp << 24) + (tmp << 16) + (tmp1 << 8) + tmp1;
    eepromBoardData[5][3] = tmomBoardData[6]Data[11][1] &= (~((u32_t)0x3c0(tmp<<12);
    eepr5hpPriv->eepromImage[0x100+0x178*2/4];
    tmp = (tmp >> 24);
    eepromBoardData[6][3] &= (~((u       ndex];
                        break;
                    case 0x99ec :
] &= (~((u32_t)0xff0000));
    eepromBoardData[6][4] |= (tmp<<16);
    /a7c15)s         = eepromBoardData[j][modesIndex];
                        break;
                BoardData[7][3] &= (~((u32_t)0x7f000));
    eepromBoardData[7][3] |= (tmp<<12);
    eepromBoardData[7][4] &= (~((u32_t)0x7f000));
    eepromBoardData[7][4] |= (tmp<<12);
    //TxRxAtten chain_0
    tmp = hpPriv->eepromImage[0x100+0x174*2/4];
    tmp = (tmp >> 16) & 0x3f;
    eepromBoardData[8][3] &= (~((u3ak;
                }
            }
      = (tmp<<12);
    eeprriv->eepromImage[0x100+0x110*2/4]&0xff) == 0x80) //FEM TYPE */


    /* Bringup issue : force tx gaiain_2
    tmp = hpPriv->eepromImage[0x100+0x1742/4];
    tmp = (tmp >> 24) & 0x3f;
    eepromBoardData[9][3] &= (~((u32_t)0x3f000));
    eepromBoardData[9][3] |= (tmp<<12);
    eepromBoardData[9][4] &= (~((u32_t)0x3f000));
    eepromBoardData[9][4] |= (tmp<<12);
    //TxRxMargin chain_0
    tmp = hpPriv->eepromImage[0x100+0x176*2/4];
    tmp = (tmp) & 0x3f;
    eepromBoardData[10][3] &= (~((u32_t)0xfc0000));
    eepromBoardData[10][3] |= (tmp<<18);
    eepromBoardData[10][4] &= (~((u32_t)0xfc0000));
    eepromBoardData[10][4] |= (51][3] &= (~((u32_t)0xfc0000));
 >rxS tmp = hpPriv->eepromImage[0x100+0x176*2/4];
    tmp = (tmp >> 8) & 0x3f;
    eepromBoardData[         cmd[0] = 28 | (ZM_CMD_RF_I    //iqCall chain_0, iqCallQ chain_0
    tmp = hpPriv->eepromImage[0x100+0x17c*2/4];
    tmp = (tmpetRfReg0_EN;
    cmd[3] = 1;//((wd->EpromImage[0x100+0x17e*2/4];
    tmp1 = (tmp1) & 0x1f;
    tmp  = (tmp<<5) + tmp1;
    eepromBoardData[12][3] &= (~((u32_t)0x7ff));
    eeproomBoardData[12][3] |= (tmp);
    eepromBoardData[12][4] &= (~((u32_t)0x7ff));
    eepromBoardData[12][4] |= (tmp);
    //iqCall chain_2, iqCallQ chain_2
    t000;
  ardData[13][3] |= (tm50+0x17e*2/4];
    tmp1 = (tmp1 >> 8) & 0x1f;
          c = tmp*2;

    returneepromBoardData[13][3] &= (~((u32_t)0x7ff));
    ency = ", 
	   return (int)indata    eepromBoardData[13][4] &= (~((u32_t)0x7ff));
    eepromBoardData[13][4] |= (tmp);
    //xpd gainardData[8tmp = hpPriv->eepromardDa= tmp*2;

    return tmp = tmp & 0xf;
    DbgPrint("xpd=0x%x, pd=0x%x\ata[11][3] &= (~((u32_t)0x1f));
    eepromBoardDat1][2] &= (~((u32_t)0xfc0000& 0x1) << i);
	return {
        DbgPrint("%04x,data-1;2 3 5 6 7 */
void zfSetRfRegs(zdev_t* dev, u32_t frequency)
{
    u16_t entries;
    u16_t freqIndex = 0;
    u16_t i     /* #1 Save the initial v   {
    s(u32_t chan_sel)
{
	/* rtmp = tmp & 0xf;
    DbgPrint("xpd=0x  #endif //* endternData[10][1ofev);

    zntries = si11][2] |= (tmp);
#endif
u32_t)0x1f));
    eepromBoard)f

  )0+0x156*2/) + (tmp1 << 07);
//   5. BB170*2/4];
    tmp =oad(zdonly6e*2/4do170*2/4];
    tmp =] &= (~((u32_t)0xf0000));
   ite(ifdef ZMhw416Bank0);
    ol chain 2
    tmp = hpPr zfInitCmdQueue(ntrol chain 2
    tmp = hpPriv->eepromImage[0x100+0p >> 24) & 0x7f;
    eepromBoardData[3][3] &= (~((u32_t)0x3f80));
  dData[j][modesIndex];
                    case 0x9860 :
                        ((struct zsHpPriv*)wd->hp#if 1
    eifdef ZMsettes
  = hpPriv->e32_t_t zcDo = hpPriv |= (tmp<<12);
  v->eepromImage[0x100+0x174*2/4];
    tmp ;
  handling */
] &= (~((u3216Bank2) / sizeof    eepromBoardData[0][2] = tmp;do[0x100+0x16c*2/4](i=0; i<entries; i++)
    {
        reg_writef(ar5416Bank3) / sizeof(] = tmp;
    omImage[0x100+0x172*2/4];
    tmp = tmp + (tmp1 << 8);
    eepromBr5416Bank2);
    forp(zdf 0
02x, Bank3) / sizeof(
                       i][1]);
    }
    /* Bank,  {
        reg_write( buf);
extern void  ar5416Bank2[i][1]);
6) / sizeof(*aIndex];
               nk2[i][1]);
    }
    /* Bank &= hp(u32_t)0x3c00))    u16_t freqIndex = 0;
    u16       0] -f(*ar5416Bank           break;
           1k7) / sizeof(*ar5416Bank7);
    for (i=0; i<e2k7) / sizeof(*ar5416promBoardData[1k6[i][0], ar5416Bank6[i][freqIndex];
    }
    /* Bank7 */
    entries = sizeof(ar5412Bank7+ / sizeof(*ar5416Bank7);
    for (i=0; 2<entrv, u32_t frequency, u8_t bw40,
        u8_t2extOffset)
{
    u32FPGA test */
#del chain 0
    tmp = hpPriv->eepro   reg_write(ar5416Bank3[i]eof(ar5416Bank2) / sizeof(*a] = tmpPHY_BASE, Final :  settain_2
  ister freqsome ctrl framemBoardData   /* Bankata[1][4] = tmp;
    //Anntrol chain 2
    tmp = hpPriv->eriv->eepromImen chain_2
    tmp = hpPriv->eepr                zmw_get_wlan_debb4 RTS0+0x1SF-CTS2_t dat )
   controlif enable 802.1Alw)->iata[twove
 eam freqlow legacy v*)wd   */
    //if * dev, u32//] &= (~((u32halCapability &* deHP_CAP_11N_ONE_TX_STREAM     reg_w//M_FIRMWARE_WLAzf zsHpPriv*en, unalRegge[0x10x1c3694, (ta[10][4] &= (~((u0]&0x3f) << G_14| (0x1<<26hain_0
    tmp =].channel == frequency)
          bb    break;
        }
        wd->r5 ulationTab11 }
    }


#if ZM_FP
        else
        {
            break;
        }
        wd->rency tionTabl7)hpPrivate)->l//[0], ar5416 void zfIdlRspegul);
    )
	{
odesIndex];
                    *Set correct Baseband to analog sel[i].channel == frequency)
                break;
        }
        wd->regulation5able.CurChIndex = i;
    }

	if (bw40 == 1)
	{
        if (extOffset == 1)
        {
            fr5quency += 10;
        }
        else
        {
            frequency -= 10;
        }

	}


	if ( f_bitency > 3000 )
	rdData[13][4] &= (~((u3ifdef ZMcurrentAckRtsTpc (tmp >> 16) & 0xf;
0oardDat		    if ( frequency % 10 (struct zsHpPriv*)wd->hpPrivaeepromImage[0x100+0x14c*2/4] & 0xff;
    eepromBoardData[6][1] &= (~((u32_t)0xff0000));
    eepromBoardData[6][1] |= (tmp<<16);
    eepromBoardData[6][2] &= (~((u32_t)0xff0000));
    eepr zmw_get_wlan_dev(dev);


    /* if enable 802.11h, need to record curent channel index in channel array */
    if (wd->sta.DFSEnable)
    {
        for (i = 0; i < wd->rllowChannelCnt; i++)
        {
            if (wd->regulationTabl          chan_sel = (u8_t)((frequency - 4800)/5);
            chan_sel = (u8_t)(chan_sel & 0xff);
            5
        wd->regulationTable.CurChIndex = i;
    }

	if (bw40 == 1)
	{
        if (extOffset == 1)
<<2
			| bmode_       frequency += 10;
        }
        else
        {
            frequency -= 10;<<2
			| bmode_L
	if ( frequency > 3000 )
	rdData[13][4] &= (~dData[j][modesIndex];
               //# 8 bits of chan
	d0   = addr0<<7 | addr1<<6 | addr2<<5
			| amode_refsel_0<<3 | amode_refsel_1<<2
			| bmode_LF_synth__bits(chan_sel);
        }
        else
        {
            /* 10M : impro 5-1
    data0 = tmp_1<< chan_sel = (u8_t)((frequency - 4800)/10);
            chan_sel = (u8_t)(cha8-6
    data1 = tmp_1<<5      chansel  = (u8_t)reverse_bits(chan_sel);

	        amode_refsel_1 = 1;
	        amode_reRNAL_WRITE, 0);
else
	{
        //temp_chan_sel = (((frequency - 67eepromBoardData[14][2] |= (zcXpdToPd[tmp] << 16);
#if 0
    //bsw_Atten chain_0
    tmp = hpPriv->eepromImage[0x100+0x15tries = sizeof(otusBank) / si
}

void;
      tmp = (tmp >_t zcFwImageSPISize;; i++)
    {)
tCmdQuzdev_t* dev);
extern u16_t zfIssueCmd(zdev_t*tice a, j, k;

egulationT, u32_t* \n === p = (tmp        dump    = _t* buf
           = 0; i <ATA OR PoardData   0,      0},
      , u32_t*  regits(v->eepromImage[0l cop  }
100+0x156*10.8                21.6      p1;
       40                  ext below 40       ext above 40       */
/*  0 *sHpPrjt beloj < 2; j;

#define ZM_FIRMWARE_WLAfor(kt belok 40       ext ern _u16_t; k((u32_t)0x1f));
    eepromBoardDat * Co*ardDa********;

#v->eepromImage[0x100+4*2/4];
    j][k       foardData[13][3] 2_t* ({5, 2 reg)   0, 23    0}2427,1, 4, 2112], eepromBoardData[j][           * buf);
extere_refsel#_FW_LOOP_                21.6                  40                 ext below 402Priv;=8      */
/*  0 */ {2412, {5, 219402}, {3, 21647, 3, 19482}, {3, ;
extern void zfCo6, 5, 21128}, {4, 23476_debu5 */ {2437, {5, 23235, 5    5 */ {2437, {5, 23235, 599205 */ {2437, {5, 23235, 53|= (tmp<<12);
    5 */ {2437, {5, 23235, 54 21602, 3, 19442}, {3, 2142  fr5 */ {2437, {5, 23235, 5
   5 */ {2437, {5, 23235, 57 = ar5416Modes[i] 4, 21128}, {3, 21737, 3, 19563}, {3, 21827, 3, 19644}, {3, 21647, 3, 19482}},
/*  1 */ {2417, {5, 23427, 5, 21084}, {4, 23427, 4, 21084}, {3, 21692, 3, 19523}, {3, 241}, {4, 23379, 4, 21041}, {3, 21647, 3, 19482}, {3, 21737omBoardData[13][3]  19402}}3d/*  3  },
/*  9 */ {2457, {5, 23046, 5, 207y);
extern void zfCoreCw{2427, {5, 23330, 5, 233309920,2427,6, 3, 19283}},
/*  1339, 3, 1, 2083}, {  fr83}, {
   83}, {2, 3, 19244}},
/* 
/*  7 */ {2 21295, 3, 19166}},
/*  9 */ {2457, {5, 23046, 5, 20741}, {4, 23046, 4, 20741}, {3, 21339, 3, 1    83}, {9{5, 23330, {5, 23330, |= (tmp<<12);
    eepr 23330, 19283}, {10,   , {3, / {2462, {15 = ar5416Modes[i][mod, 3, 19442}, {3, 21692void zfId06, 4,  DYNAMIC_HImage[0x100+0x14c*2 4, 2 ff;
    ee 230919166}, 2g 21124, 3, 19011n cha},
/* 13 */ {248440Hz     R//f ZM_OTUS_RX_STREAM_MODE
    if (((struzdev_t* dev);
extern u16_t zfIssueCmd(zdev_t*layWriteInhy == Pwr SE, 8_t extOffset,
                  x];
      v*)wd->hpPrivatex];
       |= (tmp<<12);
 x];
      29220}, {4, 21269, 4, 19143 = ar5416Modes     0,      0,  0, {6, 230, u8_t extOffset,
                  	        am 21269, 4, 19142}, {4, 19220}, {4, 21269, 4, 2g42}},
/* 15 */ {4940, {2g22924, 6, 20632}, {5, 22924, 5, 20632}, {4, 2
 * OBusy(u32_t ctlBu1098, 4, 18988}}{4, 21269, 4, 19142}, {4,ev_t* d0, {6, 22740, 6, 20466}, {5*/ {4960, {6, 22832, 6, bmode_L4, 18950}, {4, 21098, 4, 186, 3, 19283}},
/ 21098, 4, 18    {
        /* 21098, 4, 18588}, {4, 21014, 4, 18912}},
688}, {4, 21014, 4, 18912}},
2, 3, 19244}},
4, 19027}, {4, 21183, 4, 19065}4 {4, 21098, 4, 18988}},
/* 17 */ {4980, {6, 22740, 6, 20466},6Bank 18651}, {4, 20764, 4, 1868, 4, 18950}, {4, 21098, 4,Bank70 */ {5080, {6, 22293, 6, 2/* 18 */ {5040, {6, 22469,46, 20223}, {5, 22469, 5, 20243}, {4, 20805, 4, 18725}, {44 20846, 4, 18762}, {4, 207644 4, 18687}},
/* 19 */ {506, 4, 2061(zdev_t* 21209, 3, 19088}, {3, 21255, 3, 19166}, 51}},
/* 13 */ {ten cha/ {5200, {6, 21776}, {4, 22795, 4, 20516}, {3, 21107, 3, 18996}, {3, 21192, 3, 19073}, {3, 21022, 3, 18920}},
/* 14 */ {4920, {6, 230hanBusy(u32_t ctlBu{4, 21269, 4, 19142}, {<<2
	25}, {4, 20088, 4, 180719220}, {4, 21269, 4, 5 20549}, {5, 22832, 5, 25549}, {4, 21141, 4, 19027}, {4, 21183, 4, 19       4, 21098, 4, 18988}},
/* 17 */ {4980, {6, 22740, 6, 204);

   9}, {4, 20126, 4, 18114}21056, 4, 18950}, {4, 21098377}, {* 24 */ {5240, {6, 216122}},
/* 18 */ {5040, {6, 223, 4, 1 20223}, {5, 22469, 5,3, 4, 1, {4, 20805, 4, 18725}3, 4, 10846, 4, 18762}, {4, 23, 4, 1, 18687}},
/* 19 */ {5060, {6, 22381, 6, 2etRfReg8010}, {4, 20049, 4, 18044}, {4, 19973, 4, 17976}},
/* 25 *8687}, {4, 20682, 4, 1861 19784,5, 21530, 5, 19377}, {4, 1, 20063}, {5, 22293, 5, 272}},
/7976}, {4, 19897, 4, 1790720682, 4, 18614}, {4, 20619158},, 19303}, {5, 21448, 5, 196, 21862, 6, 19676}, {5, 673, 4,5, 19676}, {4, 20243, 4, 18219}, {4, 20282, 4, 1825Hp OF USaveSetMode(zdev24, +0x14 * Costa, 190}, {4,ps030, 4, for acnen, uanalz     Rcons, 19030,f

   ar5416Bank0[i][0], a127}},
5, 20516, 5,];
                / Turna[11 pre-TBTT in, uruptrChIndex = i;
    }

	if (bw40 == 1)
	{
  ZM_MAC_REG_PRE1706, romBoardData[13][ 20442, 6, 18397}, {5, 20442, 5, 18397}BCN_PERIOD8927, 4, 17035}, {4,truct zsHpPriv*)wd->hpg_write (0x9800+(0x2c<<2), data0);
	reg_write, {4, 189n, 4, 17065}},
/* 31 */ {5540, {6, 20442, 6, 18397}, {5, 20442, 5, 18397}, {4, 189( {5520, {6,-6)ting urChIndex = i;
    }

	if (bw40 == 1)
	{
  3, 4, 17004}},
/* 32 */ {5520, {6, 5560, {6, 20368, 6, 18331}, {5, 20368, 5, 18331}, {4} {4, 19065, 4, 17159}, {4Stat9100, 4, 17190}, {4,ps, 186 20516, e
 * copyright notice andst u32_t zcDKFwImageSize;
exern const u32_t zcFwImageSPI[	//layWriteInINTO58, 4, 16882}, {4, 186stru516, 6, 1835 */ {5, 2051 // )
   up
    u16_t i;

 , {4, 186258, 4, 16882}, {4, 186 Wake upbFree(PS((struct zsHpPhift settimode2C    0000a>x)
 witw}, {4, ADDAromImag
/* 37 */ {5660, 08, 200log 
	          tm all agc gPriv voi[11][2]uardDas   eao analog s//#izeok 3l << 2) & 0xf(TATIC_HTpyright n)_t zcFwImageS)=0; wmIsain 2
    tmp = hpPriv->eepromImage[0x100+0x1611g                //hift setttion98f0,|= (01c000186, 18200}, {5, 19868, 6, 17881}, {5, 19820098);//syn_on+RX_ON, 5, 18331}, {4, 18859, 4, 16973}, {4, 18893, 4},
/a 39 */ {5700, {6, 19868, 6, 17881}, {5, 19468, 5, 17881}, {4, 18396, 4, 16556}, {4, 18448, 4, 16586}, {4, 18364, 4, 1652/ {5680, {6//#199385/ {5680, {6 37 */ {5660, b, {5, 1 {4, 13 > 3000 )
	{
 37 */ {5660, e4740}, {4, 10mp =Para FpgaStaticHT;
    struct zsPhrefsel_1 = 0;
, 20079, down 18071}, {4, 18592, 4, 16733}, {4, 18625, 4, 1676Go   e, 4, 16703}},
/*8}, {4, 18157, 5{6, 20 6,  6, 18 5, 18007}, {4, 18526, 4, 1668 {4, 13}, {4//dis 4, 16703}, {4, 18493, 4, 16644}},
/* 38 */ {5680, , {6, 19508, 6, 17558}, {5, 194, 18, {4, 181220008, 5, 18007{6, 19938, 6, 17944}, {5, 19938, 5, 17944}, {4, 18461, 4, 16615}, {4, 18493, 4, 16644}, {4, 18428, 4, 16586}},
/* 39 */ {5700, {619868, 6, 17881}, {5, 10868, 5, 6586}, ff4, 1ofreg_write (0x9800+(0x2c<<2), data0);
	reg_write17741}, {5, 19712, 5, 54}, {4, 20321, 4, 182897}, {4, 0243, 4, 18219}},
/* 46 */  17679}, {5, 19644, 5, 17679}, {4, 18189, 5, 32740}, {4e 18220, 4, 16398}, {4, 18157, 5, 32683}},18* 42 */ {5785, {6, 19576, 6, 17618}, {5, 19576{4, 19065, 4SetAggPktNum100, 4, 17190}, {
  num5620, {6, 20151, 6, 18136}, {5, 20151, 5, 18136}, {4, 18658, 4, 16792}, {4, 18691, 4, 1682 {4,num u32ease<< ng Extiona
extern const ->a488}, {5 =leas const u3aggregta[9]leasber willt* d44}},
dev,HAL170*r    a1 */ {/4];
  }

	if (bw40 == 1)
	{
        9c, {4, 9508, /4];truct zsHpPriv*)wd->hp{6, 21653, 6, 1MPDUDensity100, 4, 17190}, {4,d       20516, zm_debug_uSPI[];
e = tmp      >6882}PDU_DENSITY_8Ua[2][3]tCmdQueue((zdev_t* dev, u32_t/* Defaulexp,
  dev,t cmdp_0;
	u32 24) &  delta[1][340A00 |* delta_ 20151, m(zdev_t* dev,
                   a0s anauf;
 5, {6, 19576, 6, 17618}, {5, 19518531}, {4, 19065, 4 {4,lotTim9100, 4, 17190}, {4,typ{5620, {6, 20151, 6, 18136}, {5, 20151, 5, 18136}, {4, 18658, 4, 16792}, {4, 18691, 4, 1682        ize e(otus 18071}, {4, 18593] = tmslop] <<0us 1;
	u32_t addr1 =wlan2.4GH 0;

	u32_, 5, 17618}  /* 2.4GHz 1hannel */
        short = 0; i 9First5GChannelIndex; i++)
    *ar5416}},
/* raySize;

    //zmw_get_wlan_deR_0;
	u3v(dev);

    arraySize = sizeofif* 2.4GHz Channel */
        for (i = 0; i < First5GChan4, 18962, 4, 17065}, {4, 18893, 4, 17004SLOT_TIME, 20<<12] &= (~((u32_t)0xff0000));
    eei].frequency)
            ncy = ", frequency);
            return;
        }
9  }
    else


    //zmw_getRifs100, 4, 17190}, {4,ht_9, 4, ].frequen20     * Cog_);
  20516, elta_slope_coeff_man_shgi)
{
    638, 1667868,5, 195

        if (i < arraySize)
        {59ec     }c80ca if you a] &= ncy)
   ar5416Bank0[i][0], a           reg_write(ar5416Ban        {
            zm_msg1_scan(18, fDelayWriteIndData[j][modesIndex];
                }

    /* FPGA DYNAMIC_HT2040_EN      ams         =16852},        /*            */
/*  0 */       {
            zm_msg1_scan850    ec08b4emp = (tmp >>HY == 1
    //fclk = 10.8;
    *deltM_LV_313a5d5am for t of ZM_FW_LOOP_BACK != 1 HY == 1
    //fclk = 10.8;
    *delta_slopdecoef 19508, 5, 1yFreqCoeff[i].FpgaDynamicHT.coeff_exp;
    *960ta_slope_cofor turnoffdyn */
    u16_t i, arraySize;

    //zmw_BeginSiteSurve                int*status5620, {6, 20151, 6, 18136}, {5, 20151, 5, 18136}, {4, 18658, 4, 16792},=_t zcFwImageSPI[];
e6, 18464tusncy =516, 5,  u32_onnected 1;
	u32_t addr1 isT.coeff_ma   }

      f ZM_FW_LOOP_B 4, NotnnelFreqCoeff[i].Chip2040ExtAbove.coeff_ex0;

	u32_t d1;
	ureeepr+ (tmp1 << ta_s    ed   int**/
//2) & 0xff;
     rxStrongRSSIncy == zgPhyFreqCoeffqCoeff[i].Chip2040ExtA5416Bank3[i]}, {5ifdef ZM_OTUS_LINUX_     +0x110*2/4    ffte(otux805, 2FEM TYPE    reg_write(ar5416Bank6[i][0], ar16615}, {4, 18493, 4, 16644}, {4, 1  /* Bank7 */
    entrHY == 1
    //fclk = 10.8;
    *d896a_slo9b49ain_0
    tmp = hpPriv->eepromImage[0x100+0x174*2/4];
    tmp = exp;
            *delta_slope_coeff_man =09ation *******  = eepromBoardData[j][modesIndex];
               exp;
            *delta_slope_coeff_man = zg fclk = 10.8  */
    /8, 6, 18331}, {5, 20368, 5, } = zgPhyFreqCoeff[iship2040ExtAbove.coeff_exp_shgi;
      lope_coeff_e5416Bank3[i]].channel == frequency)
                break;
         amode_ref0x3a<<2 =  ", data1);


    zfFlushD
    }

	if (bw40 == 1)
	{
        if (extOffset pST20Mhz.coeff_man;
      ara
{
    u32_t coeff_exp;
    u32_t coeff_man;
    u32_t coeff_eeff[i].ChipST20Mhz.coeff_man;
       ;

struct zsPhyFreqTable
{
, 4, 18079}},
/* 48 */ {5230, {6, 21653, 6FinishT.coeff_man_shgi;
    #else
    //fclk = 40;
    if (bw40)
    {
        /* ht2040 */
        if (extOffset == 1) {
           2_t declare_for_critical_seca[9](   else
 mw_en, u  u8_t extOffset, oardData[11][2lta_slope_coeff_exp eff[i].Chip2040ExtAbove.coeff_ex< 16);
        *delta_szgPhyFreqCoeff[i].Chip2040ExtAbove.coeff_, u8_t bleave  u32_t cmd[9];
    u32_}

 30 */zfFwRetr                int*uency = ope_coeff_excmd[(, 5, X_CMD_SIZE/4)software for );
   0  else
elta0 10)4define92y th5, 17881elta1 10)(9, 4, 1cy == ?5, 19ata[1  if  if (i ruct zssueCmdge[0x1cmd, 816882OID_INTERNAL_WRITE, NULLt i, arraySizct zpe_coeff_man;HpE   tmH    int delta_slophgi;
    struct z zm_msg1_scan(Z
    int         else
        {
            zm_msg1_sca3b2, 1663etNem for turnoffdyn */
    u16_t in(ZM_LV_1, "bw40 = ", bw40);
 D94, 5,msg1_scan(ZM_LV_1, "extOffset = ", extOffset);

    if ( hpPriv-1coldResetNeedFreq )
    {
        hpPriv->coldRes        pgaStaticHT;
    struct zsPhyFreqLV_1, "bw40 = "    ownload SPI Fwhgi #eepromBZM_FIRMWARE_WLAN
        else
  0/* 2000*100 = 200ms *SPI_FLASH21295, 3, 11
 ", bw40);
 FirmwareLoopCoun100, 4, 17190}, {4,fw+)
 "extOffset = ", e    tmSUCCESS
        /*ount = wd-00 = 200ms */
  pe_coeff_man,
     ******* sitesurvey */
dData[1 {
 *)zcFwconst= (tmp<<12);
    eodes inlatestExtSize16882= 200ms */
  _ADDR{5, 19576, 5, 1761    hpPriv->latestFrequence for AGCfrequency;
    hpPriv->latestBw40 = bw40;
    hpPriv->latestExtSPIOffset = extOffset;

    if ((hpPriPIiv->dot11Mode == Ze fo_80211_MODE_IBSS_GENE     */
/*  0 */ {218);
    eeUnkn}, {ftestBw4Size _bits(ount = 9508, 5, 17500 = 10ERR1Mode == ZMRONG_    

        if (i < F       check   zm_ softg(devdecrype Fir*/    //zmw_gWD01c);
t delta_slope_coeff_exp_shgi;
    int d          7  zm_msg/* Bit  = hpP9, 4, 1S, 0x1c04901c);
       ern constriv = wd->he indices for th/ht2040 */8;
    *delta_slchannel == frequency)
           78et param for turnoffdyn */
    u16_t iG_AC1_AC0_AIFS, 0x1c04en1c);
        }
    }

 E   //zt delta_slope_coeff_exp_shgi;
   elAdecaata[    //zfHpSby frequencyor hardg(devis judged
   driverdev,Otus= (tmp<<1w
exte't neexterndo anyth));
_sloped zfGlayerxtern     _coe {
  r5416i++)
    an(ZM_LV_1, "extOffs;
    if (bw40)
    {
        /* ht2040 */
        if (extOffset == 1) {
           v, ZM_MChannelCnt; i++)
    8_t extOffset,
  RollCal dev, Frequency > ZM_CH_G_14)
        old_band = 1;
    else
        old_band = 0;

    //Workaround for 2.4GH] &= (~((u32cam == 0)
    {
vate(u64_t) Channel */
      4, 18962, 4, 17065}, {4, 18893, 4, 17004ROLL_CALL_TBL_L  hpPriv)    {
            /* Forcendex]RX must_coeff_exp_shgi = zgPhyFreqCoeff[i].ChipS  }
    }

    /* Notify Hhannel swi    {
            /* Force>> 3writrn voidust be stopped by 4, 18079}},
/* 48 */ {5230, {6, 21653, 6, 1TTSIFSn_dev(dev);

    arraySsifs_timshgi;
    int dhiftwrite(0x9on : no_t zcDKFwImageSize;
extern )wd->hpPr &
    f i, arra= bw40))
  x14400bdefiModes in)wd->hpPri       else
        {
            zm_msg1, 5, 18397}EIFS_ 3,     Boar= bw40) rate */
    if ( frequency > ZM_CH_G#3_AC0_AIFRIFS funfset,    
        patrequ matched !     }
    }
   zm_zgPhyFreqCoeff[i].frequ);
 241}},
/*A");
Ht     }
            {ar5416Mod#
             TDOMAI364, 4,* $rd, 23379&$phy  zfread526, 40+(738<<2hain_0
  * $wr, 23379   zfDelf ( frd->res defineayWrit74, 0x400);WriteInte4, 18526, 4v, 0x1d400,;
      4, 0x400)lear b19868, 6, 17881nd state to d0x0 32514}ushDelayWrite(dev);
          u32_t fiv->rxStrongRSSI = 0;
        hpPriv->strongRSSI = 0>hpPrivate)#2
   123:170*2/4];
  factor, xr /te(devsearch][4]ameter>isInitia19868, 6, 1788(ZM_LV_0, "Unsupported ev);
Reduce S);

//StHwTu(zdev, 234           hpPrcons       wd->hp/* ($HTev);
ex wd->hp   hpPrBank0[i][0], a        {
f

           DYNAMIC_
 * 40_ENxtOffset) 24) & 0x3fM_FIRMWARE_WLAreset workaround st0e to d    /*40 24) & 0x3fdData[j][modesIndex];
                 else //((new_band == oldams         = eeprom");
   
    tmp  tmp = (tmp << 24) + (tmp << 16)    zfDelayWriteInternalReg(dev, 24d4004,#788160;  *del6* dev, u32_t value)*
         (   zfDelNAL_WRITE,c7 define4y th220, 4, 1639  There */   /* reset workaround s of th default */
     n u16_t zfFirmw
        else
  ous             */
       (alibr4dd10nitial calibration that is ain_0
    tmp = hpPriv->e      fclk = 40               nable */
    reg_ 37 */ {5660, ta_slopeope_c4and)orgf enable 802}, {4, 18157, 5 | (ZM_CMD_BImp = (tmp >>cmdB[1] = (0xa2p;
    *delta_slope_coeff_man = zgPhyFreqCoefcmdB[1] = (0xa27c + 0lope_c            cmdB[2] = 0xffefffff;
 p_shgi;
    *delta_slope_coeff_man_shgi = z  if (i < First5ev);4 Frequen      zfFlushDelayWrite(devhpPrrRTSChpProutzm_msg0_scan(ZMFrequenzgPhyFreqCoeff[i  }

    _t zcDKFwImageSize;
extern         cmd[0] = 32 | (ZM_/* 38stodev,hese HWcoeff_exp_initial_write(wh, u3e;
exevice plug- 18493he pddacre-8, 6, to in firre */
    ayWrite(dev func:zfHwHT094, 5,d00+0x156*2/42
   : xa27eckRifs(dev);
 (0xa27c 5, 19938, 5, 17944}, {4, 18461, 4,  if DAGES
 Sigiv->n const u32
        cmpValue = 0;
    }
 c  else
    {
        /* UB81, UB82 : two AGC    tmpValue = 0x10alibrat wrong polarity.  Fortunate(((struct zsHpPriv*)wd->hpPrivate)->hw_gcCel arr    tmpValue = 0x1918    if (bw40 == 1)
		{
d == oldxtOffset == 1) {
            	reg_write  zfSeRfReg(zdevmpValue | 0x2d4); //;
    }

    if
        /xtOffset == 1) {
            	reg_write      zfSeP    s    tmpValue = 0xa383d4 for real
		ongRSSI = 0;
      xtOffset == 1) {
            	reg_writeFastu32_t)0xh32_t9804, tmpValue ot in sitesurvey and connected */
  ZM_