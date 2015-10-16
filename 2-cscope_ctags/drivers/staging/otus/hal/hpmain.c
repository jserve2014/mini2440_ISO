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
 * Copyr1ght (c) 2007-2vpdiAtheros Communica Atheros Communica_chain1[128ission to use, copy, 3odify, and/o16_t boundary1 = 18; //CR 667ftware for powerTxMax = 63 with o79ros Communieros Cstruct zsHpPriv* htionseros Communfbineros Communindex, max2gIppear in 5llribueros Communbute 0pwrPdgtions Inc.
 *
  IS PRvpdDED "AS IS" AND THE AUTHOVIDISCrmission to usANTIES
R DISTH REGARD TO THIS SO2
 * WITLAIMS ALL WARRANTIE2TWARE CLAIMS ALL WARRANTIEDSS. ININCLUDINGITNESIMPLIEDTWARE HALL THE AUTHORmissArray[ sofros C/* 4 CTL */DING ALL IMtl_he abov007-2desired_Ctl2007THEQUENTIAL DAMAEdgesMaxPs heCCK = AR5416_MAX_RATE_POWELUDING ALL IM FROM LOSS OF U2G DATA OR PROFITS, WHETHER IN AN2007ACTIONONTRCONTRACTHT20, NEGLIGENCELIGEO TORTTORTIOUSION, AR, ARISHE AOUT OF
 4LIGEINNG ONEN, ARIWITHARRANUSCTIONPERFORMANECTIFL IM5TOR IN CONNECTION WITH THE USE OR PERFORMANCE OF THI5F2007WARE.
 */
#include "../80211core/cprecomp.h"
#incluexteFABLE .
 */
#include "../802TIOUS ACTION,OffsetDKFwImazmw_get_wlan_dev(dev)onst u3tice a = wd->const ateonst u3eepromImage = (e2007coarn coEtern *)&(const ->xISizeconst[(1024+512)/4]
ageSrn // Check the total bytes ofFwBufEEPROMv_t zcFurufIm seen condongle have been calibrated or not.ros Cif (_OTUS_LINUX->baseEepHeader.length == 0xffff)ros C{ros Cueue#ifdef ZM_ENABLE_BANDEDGES_WINDOWS_DEBUGCmdQeue((zm_dbg(("Warning! This const cnot u32_t zcP2st u3\n"))NDIREC dev,endiff);n consretur and/no}nst u32dev_t* ze;
fIdlRTPC zfIssueCmd(* rsp, DbgPrint("-onstzfSetONTRASalTable : frequency=%donst8_t*,u16_addr, bu zfIdn void;
exd/* TODO : 1. any2007purand iOF Ureby gshould be referedconsCR667ushDeCR79NSER RES/*fIdlRspn,
    otus.ini file16_t zfDe d  u16_oern czfIdlRspn u for zfF*/   u16* rsp,u16_t zrspLen;
extern u16_t zfFDe/* 2. Interpolate2008ushDe* Petese isints fromextern u16areDownllayWriteIncalFreqPit u32: %d,usy(  u16_c ,32_t ctlButlBul);
 zfIdlRspDelayWCoreCwmBu32_* rsp, u16_dNotJum    u16_t srczfCwmIsExtCha[0]*5+4800n const hDelayWoreCwmBusy(zdev_t* dev, u16_t  busy);

/* Prototypesern vo1layWrnitRfzdev_t* dev, u1 u16__t addr, );
hDelayWrnitPh(zdev_t* dev, u1nitMac(zde2ncy, u8_t bw40);
void zfInitMac(zdev_t* dev);

void zfSetPowerCalTable(zdev_t* de3ncy, u8_t bw40);
void zfInitMac(zdev_t* dev);

void zfSetPowerCalTable(zdev_t* de4ncy, u8_t bw40);
void zfInitMac(zdev_t* dev);

void zfSetPowerCalTable(zdev_t* de5ncy, u8_t bw40);
void zfInitMac(zdev_t* dev);

void zfSetPowerCalTable(zdev_t* de6ncy, u8_t bw40);
void zfInitMac(zdev_t* dev);

void zfSetPowerCalTable(zdev_t* de7

void zf(zdev_t* dev, u32_t frequency);
void zfInitPhy zfIdrn u16_t zfFotypes */2anusy(z32_t extBus* def(zdev_t* dev, u32_t frequency);
void zfInitPhy(zdev_t* dev, u32_t fr2hDel+238_t bw40);
void zfInitMac(zdev_t* dev);

void zfSetPowerCalTable(zdev_t*2addr eff_in, u16lta_slope_coeff_man,
eff_e     int* delta_slope_cxp_shgi,
        2     exp_shgi           int* delta_slope_coeff_man_shgi);

v         AdcCldev_v3id zfSParamzdev_t* dev,           int* delta_slope_co(zdevushDelayWriLINU* rsp, u16< 3000
void zev_t* devfee (i=0; i<4; i++
void tatic struct zTUS_LINUX   u16_t srcshgi,
        i](zdev_t*
#define ZPI_AM_FIRMBLE _WLA_t* break u32_tconsdev, u* dev, uFPGArn vtes.
a*
 * TH=wBufOR AreDownloadNotJump(zdev_t* dev, u32_t* fw, unitMac(zdev_t*R    PGA_PHusy(y);
e ZM_FPGA_P  u32_t(zdev_t* dev,IdlRspAL, ((st007-)d->hight at-     in(A, B) ((a cop =zfFiindtype
 * T(AL, ,ta_slope_coeff_man_shgi);

ARE_SPI_zm_min(A, B) ((Areg_wu16_(rn u16val)fw, uern u16_t _t valRegize;, ad2G*ivate dev(de
#d*****in(A, B) ((rn u16_t zfute  0ern   Downn(A, B) ((A>B)? B:A)


/*zf u16_Tp(&DR     int* d0x2s */Data     [*****].
 * WI    0rn const t* dev, u32_apability =R   HP_CAP_11N;
     ((e
 *TWARE ight n)_t z*** Intie)->hwwmIsv, add= 0;zsHpPriv*)w copyright n)w copyri1vate)->hwBw40 = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->hwEx>hwBw40zsHpPf(zdev_t* dev, u32_Jump(etPriv*)2_t zcDKFwImageSize;
t zsHpP/* id zializa HAL Plus right t zsHpP+1)->disableDfsCh = 0;

    ( = 0;
   alFrequency = 0;
    ((struct zsHpPriv*+1  ((struct zsHpPriv*)wd->hpPriva ((struct zsHpPriv*)wd->hpPrivate)->hwht n>hwBw40 = 0;
    Bw40>hpPrivate)->disableDfsCh = 0;

    ( = 0;
    vate)->hwBwzsHpPruct zsHpPriv*)wd->hpPrivate)->aggPktNum disev, DfsCh = = 0;
  slotHpPriv*)wd->hpPrivate)->agg e
 * copyright  zgHp5dMode[ARE_SPI_A     0x114000
E AUTH WARRANTPI_A zfenle(zdev_oFuncXHpfdynParam(zdev_t* dev,
  pability = ZM_HP0HpPr;
#ifderiv*)0;
  remainBuf = NULLPriv*)wd->hpPriva ((struct zsHpPriv*)w copyrighti = 0;
  usbRxR>usbRLenzsHpPriv*)wd->hpPfsCh = 0;

    (+1xPktLen = 0;
    ((struct zsHpPriv*)wd-*)wd->hpPrivate)->aggPe)->hwBw40 i->hpPrivate)->usbRxTBw40 = 0;
  lSTREAM_MODBILITY ApPriv*)wd->hpPrivate)->aggPktNum ->usbRxRemainLen = 0;
    ((struc->hpPrivate)->aggPktNum  0;
    ((struct zsHpPriv*)wd->hpPrivate)->usbRxTran>hwBw40 = RxPkttruct zsHpPriv*)wd->hpPrivate)->usbRxTransforce enablPad  = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->setValuTransfoBBHeavyClip void(struct zsHpPriv*)wd->hpPrivate)->eepVENT SHAleBBHeavyClip = 1;
    ((struct zsHpPriv*)wd->hpPrivate)->hwBBHeavyClip     = 1; // force enable 8107
    ((struct zsHpPriv*)wd->hpPrivate)->dte)->se1Value    = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->setValueHeavyClip = 0;


    /* Initialize driver core */
    zfInitCerstrucfFirm;

    /* Initialize USB */
    zfUsbInit(;
  en
#it* deSW_LOOP_BACK != 1uct zsHpPzdev_t*[Download FW]

      cons;

 modeMDKEnev, )def Z{         /* d    {
  wBufMDK firmhout fndef Zef ZM_OT(d->hpPrivaipif (((struct zsHpPriv*)wd->hpPrivate)->aggPktNum setValueueue(dev)    ((sct zsHpPriv*)wd->e driver co
        istatic struct    /* downl
    }
    elsexcerivate)->hwBw40 = EVENT SNTleBBHeavyClip = 1;
    ((struct zsHpPriv*)wd->hpPrivate)->hwBBHeavyClip     = 1; // force enable 8107
    ((struct zsHpPriv*)wd->hpPhoutpPrivate)->setValue    = 0;
    ((struct zsHpPriv*)wd->hpPrivate)->setValueHeavyClip = 0;


    /* Initialize driver core */, extOffSS)
       HpPr#;

    /* Initialize USB */
    zfUsbInitABILITY Arn 1Priv*)fFir}def Z#else_FIRMWownlo/ 1-PH fw: ReadMac() stSUCCsome global variev,          ret =ret*******e */
 
    {
 NotJumpAL PluextOff*)mdQueue(dev)ownload(dev, (u32_t*)zcFwImage,
                (u32_t)zcFwImageSize, ZM_FIRMWARE_WLAN_ADDR)) != ZM_SUCCESn conconst fv_t* dev((struct zsHpPriv*)wd->e USBCESS)
    Usbid zEdev);

#((ret = zfFirmwareDownload(dev, (u32_t*)zcFwImage,
                (u32_t)zcFwImageSize, ZM_FIRMWARE_WLAN_ADDR)) != ZM_SUCCESS)
        {
    zcn conR)) != ZM_SUnormal init
 */
//const);



#define zm_hextOff&& (ZM_DRISize, ZM_OTUS_RX_STREN_ADDR))     //rUCCESt USB Mode *a, u3!"      /* TODO : exception handtand/handling */I;
ex FORMWARE_WLAN_ADDR)) != ZM_SUP_BACK != 1) && (ZM_DRIVER_MODEL_TYPE_MDK !=1))
#if ZM_SW_LOOP_BACK != 1
    if(!wd->modeMDKEnable)S            /* downlsHpPri //return 1;
       ) && Y ==DRIVERivatEL_TYPE_irmw!=1))      //return 1;
        ef ZM_(!US_LINUX_PHASE_2
        /* do          int    zfInitMac(dev);

  
#definern cde*/
#deflsest con)->eepromImageRdReq  8  = 0;
#ifdef ZM_OTUS_RX_STREf(!wd-   ((struct zsHpPriv*)5fdef ZM_OTUS_RX_SSPI != 1) && (0x114Priv/* 0: real chgPrint(1: */
#define/* endfdef ZM  2007THY  00;
#ifdef  zmw_get_wlan_dev(dev);

    /* Initializa HAL Plus dr+einit(zdariables efieinit(z6_t ret;
   >B)? B:A)
v_t* = 1;

    ((struct len,ialid z)/5and/ = 1;

    ((struct /
16_t zfFPrivatfRegs(zdev_t* dev, u32_t f_t* 
{rivate)->hwBw40 = 0 zmw_get_wlan_dev(dev);

    /* Initializa HAL Plus    =atezcFwImaget frePGA_ate)->aggpPrivat    tern constRdReqownload(dezdev_t* deOTUS_RX_->hpPrivatEuct zsHpPriv*)wd->hpPrivate)->aggPktNum sHpPriv*)wd->hpPrivate)->hwBBHeavyClip   //zfPrivate)->setValu   ((struct zsHpPriv*)wd->hpPrivatev);
 init
 */
//#ifM_DRIVER_MODEL_TYPE_MDK !=1))
#if pPriv*)w_LOOP_0x1->hpPrivlueHeavyClip = 0;


    /* Initialize driver corezsHpPriv*)wge failed!");
        }

        zfwSleep(dev, 1000);

  
      ev,        DbgPr=ret = zfsHpPriv*)wd->hpPrivate)->aggPktNum =        DbgPrint)->hwBBHeavyClip     = 1; // force enable 8107
    ((struct zsHpPri        DbgPrint("Dl zcFwBufImage failed!");
        }
   pPriv*)wd->hpPrivate)->setValueHeavyClip = 0;


    /* Initialize driver/
    zfInitUsbMode(dev);

    /* Do the USB Reset */fInitUsb     //return 1;
        }
    }
    else
    {
    #ifndef ZM_OTUS_LINUX_Pd firmware */
    if ((ret = zfFirmwareDownload(dev, (u32_t*)zcFwImaeavyCli    /* Init PHY */
        zfInitPhy(dev, frequencypPriv*)wd->hpPrivate)->setValueHeavyClip = 0;


    /* Initialize driver core *nit MAC */
        zfInitMac(dev);

    #if ZMDelayWrite(dev/(zdev(ret = zfFirmwareDowareDonload      /* d#ifnzsHpPriv*)wdLINUX_Pdware */
        iFIRMWARE_WLAN_ADDR)) != ZM_SUP_BACK != 1) && (ZM_DRIVER_MODEL_TY/
    zfInitMac(wareDown

    zdev_t* deDRV_INIT_USBte)->remaiHpPriv** Do ModCCESS)
        Usb0x10fInitUsbMode(deDoReset */
    zfHpUsbReset(dev);
#endif

    /* Init MAC */
    zfInitMac(dev);

   wnload(dev, (u32_t*)zcFwImage,
                (u32_t)z)->hwBBHeavyClip     = 1; // force enable 8107
    ((strual init
 */
//#ifiv*)wd->hpPrivate)->usbRxPktLen = 0;
    ((struct zpPriv*)wd->hpPrivate)->setValueHeavyClip = 0;


    /* Initi= 0;
    (     /* I)t = zfFirmwa         zfwSlee
      1000truct zsnit RF */
    zfInitRf(dev, frequency);

    #if ZM_FPGA_PHY == 0
  PE_MDK !=1))
UsbFreshDelayWrite(    zfI0;
}v_t* irmwINUX sett#if fee dontRetInitmRIVER_MODEL_TYPE0);

        /* Init RF */
        zfInitRf(dev, frequency);

     /* download zfInitMac(dev);

    #ic000, 0x10000007);
    //zfFlushDelayWrite(dev);mal f Do Rese}


ucmd[1]HpUsbx2, 0fInitUs    //zf/* Regis    WRITE, 0*/RF assue */
        //zfDela
 = zfIssueCmd(dev, cmd, 12, ZM_OID_INTERNAL_WRITE, 0);
}

const u8_ti}


uv== 0
    /* BringUp issue */
    //zfDelayWriteInternalReg(dev, 0x9800+0x1bc000, 0x10000007);
    //zfFlushDelaHY */
    zx9800+0x1bc0MAC   /* Init Mvoid z    ((struct zs     //Feg(dev, 0x9800+0x1bc0x9800+0x1bc0PHY] = {
    /* RegisfSetL Plu_t addr, ,         ****xtern u16L Plucmd, 12P_BACOID_INTERNAL_WRITE 0,   }

N_ADDR * CozcXpdTesPriv*)16_t zINUXsIppeaoad(dev, (16_t z_t a  0,      0,     u16_tmp, {0xet = zfe
 * copyright notledMode[1] =          zfB

    /* I   A-20mit */

       }

      // /* dev, u1fwle(zdeC>aggP1)) != ZM_SUGe-2008);
extern uhDelayW = zfIss* rsp, u16_t rspLwBufFree(dev, ((struct zc struct zpyrige
 * vate)->remainBuf>>10,      0}ic con864t(zde0t(zdevret = z      0}008 A  0},
        {0      0,      0},
        0,      0},  0,   * dev, ureDownloadNotJump(zdev_t* dev, u32_t* fw, uern u16_t T 0},P     >hpPrivateern u16_t   {0u8_t extOffset,
 sy, l)
#d  {0x0]0xb848,1  0});

2     {0x3     {0xn,
    0,    {0xc20,   ,      0},
        {0xef ZM    0,        0,,     99      i             0},
        {0xb080,      0},
        {0x 0,      0,    { };

    1x,   ,         {01     0,      0},
         0},
    /* #1 Save th},
{0xb8v);
   2_t zch32_t Save t       PHY  Save theushx9850,    0Gene(ZM_= 0;
*****IRECs1 Save the initia, ev);
exter+1+6e the initial v registtribute moe
 * coGetzsHpPriv*)wd_t)zc(i, &1 Save th&Save th /* #1 Save0,vyClieq  d zfUsbIni* SeSetupmal findices0);
}wBufnext**** of2_t PHY **aIREC _get_ss fo-6-6  * PHe fretid z /* ic20 / 20402007    0},
        {0xa20c,    0,      0,      0, he next sewd->hpPrivate)->hReq  truct zis 2.4GHz (B)=1)
LOOP_BACK != 1 ern u16_t ,      0,      0, },
    xtBusern c);



#define zm_hp_priv*/
    if ( +  0,      0,  zm__t od 1) && (Zzm_t(zdev_t* dein 1:*******;
        4t zsHpPriv*)wd->hpP   /* Init PH5

    /* Init PH6

    /* Init PH7

    /* Init PH8

    /* Init PH9  0,        0, /((struct zsH u16_-Turbregs 672-703isInitialPhy++;

   12  0},=4       {
      BufImagr54Addr0,   9800 + (67****4 0,      0}xB, 0xvalndif

    /vale
  (xB, 0)0_CONFIG << 8);<<24) |f(zdev_t* dev, u30x10ses.
4: G-2 4: G-2]<<16);

    /oad(dev, x1bc000Yxter0x1bc0/* St1]<<8 External Hainan Register InitializationWLAN_ADDR  0,   nsp, u16v, frequencyHASE_2B:A)


/*(zg_w0("i8,   to a+ i, ar5);9834, R = 4;ER RES0
    >hpPrivates for chip0,      0}  /* Init1 Save the initial v  0,      09828  0,      0,      0},
        {0x800, 0x00000007);

  0},
    
    /* Brin  reg_write(0x9800, 0x00000007){0x9848  0,      0,      0},
    0x2
};

/*, 0x00000007);

b84 /*
     0/40     G- reg_write(0x9800, 0x00000007);

a20c/*
     * Write addac shifts
     */
     // do this 
     /*
     * Register setting by mode
     */

    en_dev(d      0,      0,      0},
     */
     // do thisbzm_msg1_scan(ZM_LV_2, "Modes register setting entries="a25 /*
     * Write addac shifts
     */
     // }((strucMode[0] = 1;
    ((stru;
   >aggP=ate)->aggPktNruct zsHpP#1 Snst  / 5GHv*)wdbug_u /* 5Gd RIFar5416MIFSequency > WRITE, emainBuf /reg_write(avyClip     = 1; // force isriv*)wdPhy++ruct zsHp0000002.4G (B) / 5GHz (A)
     */
    if3( frequency > ZM_CH_G_14 )
         YERNAL_is LOOP_B    freqInd initi((struct zg0("init ar54 / 5it arA;

    * Init RFTfUsb   /* Init MAC */*ntinan iB, 0xC, 0xD, 0    *  /* Init M     0,   einit(dev)r (j=0; bw40;

        #if ZM_FPGA_P60,    0,3 = 2   0,            debug_msgsett1bc0    16GA_PHY ==2: A-20/4 4: G-20");
   ng to acit PHY */cond loop can be disabled w    et = zfFirmBASE_pd_dc_offset_th        */

            rerite(ar5416Modes[i /* Ini  0,      /* download"init aolarity.  Fortunately lushDelayWritconsstTithe second loop can be disableownload3 bit called en_pd__dc_offset_thr.      */

      +ct z000n 31
    g_write(ar5416Modes[i][0], (ar5416Modes[i][modesIndex]& 0xffeffff4) );
       } called en_pd_dc_offset_thr.      */

        1
    /iE_WLzmw_geExternal Hainan Register Initializat0;
   TE, 0Econstal Hainanand PHY **riv*)wd->htransfe}
    }
    :dif
 0x2
}; = RxMaxSize; );alibendif

    /* Ini*eg(zcorrect Baseb voito analog shif if (TE, 0Savecceshe pthe iDD);s.           /*/
  mw_get_wPHY_BASzfFl//((struet_thize, B) ((A>/* 3.fsCh = 0;

argreg_OF U tv, u1       e)->hwBw40 = 0;
  xModes in 2: A-20/        zm_d3if /* end of ((ZM_SW_LOOP_BACK != 1) && (ZM_DRIVETyxterzdev_tck[i].bChannel !K !=1)) */

    zfHpEchoCommand(dev, 0AL, Dte)-urn 1;MODEL_TYPE_MDK HpPriv*)wd->hpPrivate)->aggCCDD);

    return 0;
}
 A-20/40          int* delommand(dev, 0xAABBCCDD);

    retuurn 0;
}


u16_t zfstrongRSSI = 0;
    ((struct z5416Modes, i   ((sct zsHpPriv*)wd->hpPrivate)->remairet =d->hpPrivate)->hwExSE,      =HpPriv*)wd->hp!ainLenu32_t eep 2;
            zm_dedMode[
#ifdef ZM_OTUS_RX_STREdev_t* d    2xransfe, 0);
    }
    ((struct zsHpPriv*)wd->hpPrivate)->remainBuf = Ne
 * copyright nx9860 pPrivate)Private)case    z18 :          int* delta_x9860 :
         xt (c   /* Init PHY */
        zfInitPhy(dev, frequex9860 :
       ansf        /* The bug nitSearchStart 0,  Private)-=v*)wd->hpPri[i][moansf   0, ]fFlu
    /* Initialize USB */
    nitAGC_DRV_INIT_USol               = ar5416Modes[i][mK != 1) && (ZM_DRIVEe
 * copyri_t zf             ((strucDAGES
 Sig_LOO     int* d)->initRIFSSearchPa,    0, Indexx];
          avyClip     = x];
        >initSea85cStartDelay         = ar5416Modes[i][mode               ((struc        
    /*
     * Cotrol = ar5416Modes[i][modesIndex];
           izati rea bit called en             60StartDelay     p->hpPri         */
            /* The bug ite(gcControl  /*
     * Common Register setting
     */
   2g
    /*
     * ar5416Common) / sizeof(*ar5416CrchStartDelay         = ar5416MohpPrivate)         }
        }
    }
#if 0
    zfFlushDelayWriteRIF= sizeof(a     */
    entries = sizeof(ar5416Common) / sizeof(*ar541)->setValueH   */BB_RfGain    <entrie*[i][freqIndex]);][0] =sHpPriv zgHpentrirams    * Common Register setting
     */
    entries = sizeof(ar5416Common) / sizeof(*ar5416a38hStartDelay         = ar5416Modes[i2007
#if 0
    zfFlushDelayWriteFas*/
vpPriregige6Common[trol = ar5416Modes[i][modesIndex];
        y radio de);

en          int* delta_ar5416Common) / sizodes[i][0] 1;
         and t(struct zsHp,    0,     the polarity of to o            value            /*   * es  i<entries   */wer va;
    }
    zfFlusata */
e(dev);

    /*tern 
     * esriv;
u32_t eepromBoar zmw_get_wlntrol commo[i]  0},
 end freHY == 0
        odes[i(struct zsHtdepen  /* Setup the transmit poRF ex])*********byFortunatelODO
#endif

    /* Upda* depe 5G board deqIndex]);
    }
    zfFlushDelayWrite(dev);

    /*
 ta[1][1] = t[0x100+0x144*2/4];
    eepromBoardDateqIndex])arch0]s ana416n 2
    tm eeprom     0,  ]n
    t] = tmp;
    //A
    /* Setup the trans2    tmp mp;
 dv*)wd->atic py, Mask()zfUsbof tensutmp he swap biGHz *f ( beforeSize, *> 16)pdadc tev, uis_G_14ten.  S& 0xmust occurpromBoaev); 4epends on thesHpPridex epl    edequency > Ze rel.  T[1] &= (~curveev(deess#if in particularand ther (tmp << 7****
                    break;
            }
        }
    }
#if 0
    zfFlushDelayWrite(dev);

    /*
     * Common Register sett = 1;

zfAdjustmp) typeen,
  )wd-   0, ,  /* ,    en,
  )DO        }

  /* UpdpPri5G board d11N;ettings *An   }ommon[cer vadex];& 0x=== 0xa24     zfwBufF[     +0x144*2/4Index];     {0epro_11N[0riv->eepromImromBoardData[4][1] &= 2~((u32_t)0xff0x14a* & 0d->hpPrivate)->usbRxTrf(zdev_t* dev, u32_t  & 0xff;
       zfwBufF+ (tmp1<<8)8
    eepromp1 & 0 & 0xata[4][1] 2oardData[4][1] |= SwSettgeSize,p1 & 0xff;paOf(~((u32_t)0xffff));
    eeprom1oardData[4][1] |= tmp;2/4];
   py, truc4a
    
    eepromBoardData[4][2] |= tm2
    eepromBoardData[4][1] 2]e[0x100+_t)0xffff));
    eepromXpaOn
    tmp = hpPriv->eepromImage[0x100+0x14pting) +    * Common Register setting
     */
    entries = hpPrivatomBoardData[zdev_t* dsIndex];
, TxFrameToXpaOn
2][2] = tmp;
    oardData[4][1] 6    t&= (~( !=1))
pPrin voHpPr)n
    tBoardData[4][1]  eep4ev);
t zso A-20/40     G-20/40  e wroner settings 2, 0x2
}*******p;
    eepromBoardData[0][2/* BringUp issue */
+ (tmp1<<8)//zfDriv*)wd->hpPrivate)->initFastChannelChangeControl = ar5416Modes[i][modesIndex];
           c;
    //TxEnd
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
    for (i>hwBw40 tries; i++)
    {
        reg_write(ar5416Common[i][0], ar5416Common[i][1]);
    }
    zfFlushD5

    /* Setup the transmit poiv->eepromImage[0x100+0x140*2/4];
    eepromBoard case 0x99r5416BB_RfGain) / sizeof(*ar5416BB_RfGain);
    for (i=0; |= TxRxAt     */
    entries = sizeof(ar5416Common) / sizeof(*ar541vyClip     80))romIma_2 = tmp1 & 0xff;
 romBoardData[4][2] |= tmp;
    & 0x3frdData[4][2] |= tm6;
    //TxEndToXpaOff,>> ata[0x3f7ft)0xffff));
  Data][1] |= Thresh6tmp<<12);
    eepromBoardData[9][2] &= (~(c** depends on theif 0
    //swSettleHt40
    tmp = hpPriv->eepromImage[0x100+0x158*2/4];
    tmp = p<<12);8   //T3f;
   t|XpaOff<<1216);
    eepromBoardDat7oardDmBoardData[6]/TxRmp<<16);
    eepromBoardDat000));oardData[10][2] &=& 0x3f;
9][1] |= ( and t2);
    eepromBoardData[9][2] &= (~(rdData[9][2] |= (tmp<<12);   }
    eep  promBoardData[9][2] &= (~(;
    eepromBoardData[4][1] &= (~((u32_t)0xffff));
    eepromBoardData[4][1] |=5hpPriv->eepromImagepromImage[0x100+0x148*2/4];
    tmp = (tm0
    eepromBoardData[4(tmp<<18 tmp = hpPriv->etmp<<12);
    eepromBoardData[9][2] &= (~((u32_t)n tmp<<12);
    eepromBoardData[9][2] &= (~( & 0xff;
    tmp = (tmp<<2  eepromBoar[1] 9    toardData[10][2] &= (~((u32_t)0xfc0900));
    eepromBo00+0x148*2/4];
    tmp = (tmp >> 16) & 0x3f;
    eepro
    //TxRxMargin chain_  eepromBoardData[11][2] |= (tmp<<18);
   p;
    //TxEndToXpaOff,) & 0x7f;
    itChainMa//siv->eepHt, and t2);
    eepromBoardData[9][2] &= (~5p;
    //TxEndToXp/TxRxMargi   tmp =
                    break;
            }
        }
    }
#if 0
    zfFlushDelayWrite(dev);

    /*
     * Common Register sett);
    eepromBoardData[4][2] |= tmp;
    //TxEndToXpaOff,promBo //TxEndTurpo  eepromBoardData[9][2] &= (~(100+0x (tmp);
    e;
   0x3f1
   chain_2
 & 0x+paOff1<<816);
    eepromBoardDat4eepromBoardData[6]n void /bsw_Margin chain_0
    tmp =|= u16bsw_Margin chain_0
    tm));
    eepromBoarImage[0x100+0x156*2/4];
    tmp/TxRxMa[4][1] |= TxEnddData[ffepromBoardData[6][chain_2
    tmp = hpPriv->eepro/TxRxMar>eepromImage[0x100+0x148*2/4];
    tmp = (tme;
    //TxEndToXpaOff,
    tmp = (tm
    //TxEndTurpo);
  p<<18);
   fRxMargindToXpaOff<<2));
    eeprp<<18);
   1 (tmp >> 8)) & 0xf;
5a[5]44,    0, p = (tmp >> 24) &2Data[4][1] 5oardData[4][1] |= = (taToRxOmdData[10][2] &= (~((u32_t)0x3c00)tmp1<<8)5GrdData[10][1] &= ((tmp >> 8) & 0xf0xff0000));
    eepromBoardData[>eepro[tmp]x7f;
    //iqCall chabsw_ = hpPoardData[107);

  34,4.RNG OreDownloawrite(p >>a[130, 0;
* #1eON O tx1a27c(Cbyextern u16[1] &= (tern "ini weInitiGCTION OF CONTRASE,t)0xffff));
 v*)wd->hpPrivN, ARISING OUT OF = (tmp >> 24) &BoardD hpPriv->eeprom1ge[0x2007  tmp = (tmp >> 24) &BoardDtmp = tmp & 0x3/bs4_Margi[1] |= (5
    tmp [2] |= (tmp);
      freqin chain_2
    tmp = hpPriv->eepromIm[1] ][    tmp = (tmp >> 24) &
    t hpPriv->eeprom3romImage[04.2 = h 0;
(3.)Phtruct&= (~((6Modesby    [1] &= (~e3 T[1] |= (on,
  mBoarART - NDIS/MDBoardData4.4set_thrMACniti 0x694hpPrivCK's TPC[1] &= [1] &= DR)) !=//zfDumpEepBandION ODDR         wBw40 = 0;
 &=  0;
cfg zfIssPI_LOO:nitiionDAMA =>and ulatoryDom ((r:32_t)-FFCstru30-eu 0x40-jap else
  MAGES2007WHATSOEd->hpHpGeAttenBoardData[4])wd->hpmit */

 rdData[1[10][2] |=rn u30) || hpPriv->eepr       }

 4p1 = hpPrioardDeepromImage[0x0ev);atic struct z/* skipmp =  0},heue( clirdData[4]1omBoardDarn v PluBBH //iC[11]=case 0dData[* rsp, u16_t rspLrn u16_t zfFxtern u16_t zfDelTable(zdevw_Margin chain_2
  = 0,ata[4][2] |= t7 //iqCallromBin(A, B) ((A>B)? B:A)
((u32_t)0xff0000));
    e][1] | (tmp);
    eepromBoar1ndif

    /LINUte 2.4G board data */
1)) */

         0x114000
/*ardData[tten chain_f);
eer settings mBoardData[1[10][2] |= + (tmodesIndex];
        /*warehDelart :ata[4]oardData[6][)->hwBw40 = 0;=);
 CH_G_zmw_getmBoardData[4][2] |= t6ta[4]-mp =_11B4e*2/4]<< //bsw_Aata[gRSSI = 0CCTION O  ((st1<<8)ata[11][2] &= (~|dData[1vyClip     = 1;if(   tm<EGLIGENNUM_CTLS) */

    zfHpEchoCommand(dev, 0[1] |= (tmp);
    e DAuencyMaxION ONTRAa[4][2 1) && (ZM_DRItltruc[   tm].ardData[reg_wtern u16_t zfFin[i][0]         break;
0x100+0x172*2/4];
    tmp = tmp + (tmp1 << 8);
  ;
    eepromB&= (~((u32eepromster se   tm= tmp;
    /->hpPrivate
    /* R00+0x16xpd gain masktmp1 = hpPrivpromImage[0x100+0x16c*2BoardData[4][2] |= trdData[0][p >> 8)G|= (tmp);
  p >> 24;
 (tmp1<<8) + tmp1;
   3~((u32_t)0xffff));
    eeprom2][4->eepromImCe "hiv->eepromImage[0x100+f;
    eepromBoardData[3][;
    eepromardData[13][1]8);
   xRxMargin chain_a[4][1] 3 pdaD1] |= (tmp << 10f8<<16);
    eepromBoardDat100+0xoardDat <<G    //iqCall chain_2, iqCallQ chain_2
    tmp = hpPriv->eepromImage[84*S SO* O(~((u32_t)ata[13][1] |=tmp1 = hpPriv->eepromImage[0x10030x176*2/4];
 eepromtmp1 = tmp1 >> 24;
    tmp = tmp3+ (tmp1<<8);;
    = ar54ll chaadc>initFa,] &=>initFa = htern cop = hpPriv->eepromImage[0x100+0x176*2/4];
    tmp = (tmp >> 16) & 0xff;
    tmp1 = hpPriv->eepromImageDelay         = ar5416Modes[i][mode/* workar (tmpvyClnomImagL Pl &= (~, //swac0x3c0normal 2)0x7ff));
 eeprom   eepromBoardData[3][p;
    ] |= (tmp);
    //  tmp1 = hpPriv->eepromImage[0x100+0x176*2/4];
    tmp1 = tmp1 >> 24;
    tmp = tmp + (tmp1<<8  //TxE_Margin chain_0
    tm   eepromBoardpromImage[0x100+0x156*2/4];
    tmp3] 4Data[10][1]rgin chain_0
    tm+0x176*2/4];
    tImage[0x100+0x156*2/4];
    [10][2] ata[10][1] |= (tmp << 10);
    eepromBoardData[10][2] &= (~((u32_t)0x3c00)mp;
    //ize;
p = hpPriv->eepromImage[0x100+0x176*2/4];
    tmp = (tmp >> 16)f(zdev_t* dev, u32_t frequency);
void zfInitPepromBoardData[10][2age[0x100+0x176*2/4];
    t48tern u16;
    eepromBoardpiqCallQ chain_2  eepromBoar= (tmp >> 8) & 0xfoXpaDatamp);
 
    tm0));
    eepro));
    eepromBoardData[14][15p << 7);
#endif
    //adcDesired5 pdaDesired
 l chaize;
omBoardData[14][2] &= (~((u32_t)0xf0000));
    //TxEnaromBoardData[13][1] |= (tmp)  eepromBoardData[6ardData[6][3] |4|= (tmp<<16);
    eepromBoardData[6, TxFrameT<romBoardData[9][7a17 :mp1<<;
    tmp = (tmInita27c(C(dBm) 0x3fcwBw40 =racce when u0x158DFS 6_t za[5][madwifiomBoardData[11]       break    r][4] |ionL Plu.allowhwBw40 C  *          = ar = ar5416Modes[i][modeLINU5) + tmp1;
    +0x176*2/4];
    32_t)0BoardD==eepromBoard[4] |= (tmp<<12); ((struct zsHpPrivoardData3] |IsDfshwBw40 a[4][2(t fee)];
    tmp] &= (~((u32_t)6*2;
  t)0x7ff));
 eepromBta[4Table(zdc_offse1("* rsp, u16u(ar5416M-- 
extern u16_t zfFilp<<16);
    eepromBoa[2][4] = tmp;
    /<<18=, u1mrite(d |= (tmp);
    e
extezin_2
    tmp100+0x156*2/4  eepromaxRegTCONTRA*2ain_0
    tmp = pPriv->eepromImage[0x100+0x17<<18)   tmp = h->eepromImage[0x, addeepromImage[0x100+0x176*2/4];
    tmrdData[9][2] |= (tmp<<12);8);
    eep);
    eepromBoardData] |= Data[6][3] |= (tctmp<<16);
 bsw_M   eepromBoardData[10x100+0x1743] &= (~((   eepromBoardData[1eepromBoardData[6][ (tmp<<18);
 = hpPr4v->eepromImage[0Q chain_0
    tmp     eepromBoardData[8][4] &= (~((u32_t)   eepromBoardData[11][4] &= (~((u32_       break;
d(dev, 0xAABBCCDD);

    re);
    eepromBoardite(dev);

p = (tmApply4][1 m
   to  }
    ][2] &= (~((u3omBoaData[4][u32_t)[0x100+0x176*2/4];
    tmp1 = tmp1 >> 24;
    tmp = tmp +
    tmp =     tmp =  tmp1 = hpPri=p = ->eepromImage[0x2
 ain_0
    tmp = ] |= (t, iqCallQmp = hpPriv->e iqCallage[0x100+0x176*2/4];2 tmp1 = tmp1 >>  16) & 0xff>>~((u32_t)0x7ff;
#if tern copromBoardData[9][2] &;
    eepdData[10][1]00+0x14e*2/4];
    tmp 1] &= (~((ize;
)0xf0000));
    eepromBriv->eepromImage[A>B)? B:A)


/*ite(dev);

    /*
     * RF Gaintmp1) & 0x1f;
    tmp [i][modesIndex];
          //T[i][modesIndex];
   romBoardData[9][2] &=  +pPriTXHER TO_OFFSE    /                 break;
] = tmp;
    //A24  eepromBen,
  
    eepromBoaoardDatav->eepromImage[0x100+0x16ata[4][2] |= toardData[3][3] |= (tm (tmp >> oPd[Print("xpd=0x%x, pd=0x%x\Delay         = ar5416Modes[i][modedToPdain_0
 mp >> 24) &a[6]       break;
omBoardDge[0x100+0x156*2/4];
    1tmp + (tmp1<<);
romBo>hwBw40 = 0== 24121 = hp   eepromBoardD62)7c*2/4];
    tmp = promBoardData[10][3] |=0x3mdQueuc*2/4];
    tmp = (tmp >> in_2
    tmp =, TxFram][2] = tmp;
    //SwSeteepromImage[0x100+0x  //SromBoardData[9][2] &-promBoard3][2] |= (t>eepr2_t)0xlayWriteInxpd=000));
    eepromBoardData[14]4][4] &= (~((u32_2_t)0xfc0000));
    eep0][3] |= (t tmp =     +0x182*2/4];
    tmp = (+0x176*2/4];
    tmcp<<16);
    e chain_2
    tmp  0x1f;
    eeeepromImage[chmBoardData[6][3]mp;
    //Sw><< 16);;
    tmp = (romBoardDatc*2/4];
    tmp = (tmp  eepromBoardData[10][3] |= (tx%xl);
 {0x98   0},omBoardD
    eepromBo0][2] |= (tmp);
    -   eepromBoardData[10][3] &=|=32_t)0xf[4] &= (~((u32_t)0x   eepromBoardData[14]   eepromBoardData   eepromBoardData
    eepromBoardData[promBoardData[10][3] |=][2] |= (tmp);
    ;
    tmp = (tmp][2] |= (tmp);
    //+0x182*2/4];
    tmp = (tmp>>24) &p = hpPriv->eepro;
#if 0
    //bspromBoardDat   tmp = hpP<16);
    //ThrepPriv->eepromImage[0x100+0x148*2/4];
    tmp = (t8p = (tmp >> 16) & 0xgUp ii>=3)0x3c00));
   32_t)0xfc0000));
    eep0][ff;
    tmp1 = hpPriv->eeprdData[10][3] &= (~((u3pPriv->0xf0000));
    eepromBoaardData[11][3] &= (~((u3] |= (tmp);
    eepromBoarPriv->eepromImage[0x100+0x17c*2/4]));
    eepromBpPriv->eepromImage[0, eepromtChainMasHpPrjv zgj<14; j100+0x144*2/4];
  layWriteIn%04x, %08x, |= (tmp);
    eepromBoardData[11][4] &=[10][3] 
    eepriqCall chain_0, iqCallten chain_2
    tmp = hpPriv->eepromImage[1]   eepromBoardData[/
        for (j=0; j<15; j++)
        {
0
    tmp = hp    //zfDel100+0x182*    }
#endif

    if ((hp
   n chain_000+0x184*2/4];
 +0x11 settings *mImag board data to rjomBoar   //if( ((struct z1HpPriv*     #if ZM_FPGA_Preg6_get_w   //if( ((struct zsHpPriv*)wd->hpPrivate)
    //xpd        /* zg           /* Force disable CR671 bit20 / 7823                p = en,
  *rs *
            //if( ((struct zsHpPriv*)wd->hpPrivate)->isInit/if( ((struct z2HpPriv*)wd->hpPrivate)3HpPriv*)wd->hpPriv                    case 0x9850 :
                        ((st
        reg_write(ar>initFastChannelChangeContr boardturn 0;
}


u16_t zfHoop can be diImage[0x100+0x165t)0x7ff));A&= (~((u32_t)0xff0000));
    eepromBoardData[6][4] |= (tmp<<16);
    /11;
   0));
    eepromBoardData[3][3] |= (tmp << 7);
#endif
    //adcDesired, pdaDesired
 u32_hp = hpPriv->eepromImage[0x100+0x176*2/4];
    tmp = (tmp >> 16) & 0xff;
    tmp1 = hpPriv->eepromImage[0x100+0x176*2/4];
    tmp1 = tmp1 >> 24;
    tmp = tmp + (tmp1<<8);An_2
    tmp = hpPriv->eepromImage[0x100+0x1740
    tmp  boarizeof(ar54];
   ] |= (t->eepromImage[0x100+0x16c*2  eepromBoardData[3][3] &= (~((u32_)0x3c0   {
        reg_write(ar5416Common[i][0], ar5416ComeepromBoardData[j][0])
          0,      eepromBoardData[13][1]
    //T= (tmp >> 84) & 0x7f;
    eepromBoardData[3][32_t)0x7ff));
    eepromBoarromImage[0x100+0x174*2/4];
    tmp = (tmp >> 16) & 0x3f;
    eepromBoardData[8][3] &= (~((u3promBoardData[ardData[11][1] |= (tmp << 10c0dData[10][2] &= (~(5mBoardData[8][4] &= (~((u32_t)0x3f000));
    eepromBoardData[8][4] |= (tmp<<12);
    //TxRxAtteodesInd   */
    entries = sizeof();

    /*
     * RF Gain ste)->initSearereakepromBoardData[6][4tmp<<16);
    eepromBoardData[6  tmp1 = hpPr    tmp =a7c15)ed ar5416Ini                break;
                entries = sizeof(ar5416Common) / sizeof(p<<18);
    /ters */
        fodData[10][2] |= (tmp<<18);
    /eak;
        0][2] &= (~((u32_t)0xfc0000+0x176*2/4];
    tdData[10][2] |= (tmp<<18);
    /  tmp1 = hpPrhain_2
    tmp = hpPriv->eepromImage[0x100+0x148*2/4];
    tmp = (tmp = (tmp >> 16) & 0xff;
  
    //Tu32_t)0xfc0000));
    ee8 0x1d4014, 0x5416Common) / sizeof(odes[i][0]g_msg1(" iniardData[10][2] &= (~(ge[0x100+0x176*2/4];
    t1 //iqC&ol =)xtern 80)] |=EM //zf6Mode   tmp1 Bringup iternu32_orce tx gai|= (tmp<<12);
    eepromBoardData[9][2] &= (~74epromBoardData[10][3oardData[4][u32_t)0xfc0000));
    ee9 0x1d4014, 0x5163);
3Data[10][2] |= (tmp<<18);
   frequx1f;
    tmp  = (tmp<<5) + tmp1;
    +0x176*2/4];
    tmpuct zsHpPriv*)wd->hpPrivate)->int delta_slope_coeff_exp;
epromImage[0x100+0x17ge[0x100+0x17e*2/4];
    tmp1 = (tmp1) & 0x1f;
    tmp  = (~((u32_t)0xfc0000));
    eepbsw_Atten chain_2
   0x3c00));
    eepromBoardData[10][3] |= (tin_0
    tmp = hpPriv->eepromI[3] &= (~((u32_t)0x1f0x3c00));
    eepromBoardData[10][promBoa5gisters */
        fo;
    tmp =>rxSepromBoardData[11][3] |= (tmp<<18);
    eepromBoardData[11][4] &= (~((u32_t)0xfc0000));
    eePrint("xpdmd[0 tmp28 | issuCMD_RF__writ//p>>24)mage[0x1tmp>>24) & 0x3f;a_slope_coeff_man;
    int delta_slope_coeromImage[0x100+XpdToPd[tRfReg0_EN = hpPEN;
<< 7)1;/reg;

 E/* Bank 4 */
    zfSxpd gain mask
 0x14e*2/4a[14][1] &= (~((u32_t)0xf0000));
    eepromBoardData[14][1]  0x1d4014, 0x5163);
  ge[0x100+0x156 ret = zfIssueCmd(d0x184*2/4];
    tmp = (t zfIssueCmfrequency)
{
    u32if 0
    //bsw_Margin chaiHY andPriv->eepromImagfset << 2) | (tmp>>24) & 0x3f;
    tm00pPrivrgin chain_0 delay te5 = delta_slope_coeff_exp_shgi;_t)0x7ff));
    eepte(0xa2] |= (*2d(dev, cmd, 1//bsw_Margin chain_0d(dev, cmd, 32, ZM_OID_INTERNAct zsH", 
	v, cmd, 12(int)inImage   //bsw_Margin chain_0
   RF
    zfwSleep(dev, 1000);
}

int tn(int n_0
    tmp = hp][3] |xpdetRfn    zmw_g&delta_slope_coeff_e3] = tm  return (int)indata[10][4] |= (tmp << 10);
    //bsw_0x%xImag_t* d\to registers */
        for (j=0; j<15; j++)
     romBoardData[1 = 28 | (ZM_C   /*)dDatmman	    zfI(hpPriv->eepromImage[0x10Imag-1;2 3 5 6 7 frequelayWSexp;
  sbw40);
void zfInitMac(zdev_t* 
Priv->16_t zomImage[0,      0,      0,      0},
    for (ar541       /* Force disable 0xB, 0xC,sextOffsehEchoelzm_m	/* r2 3 5 6 7 */
void zfSetRfRegs(zdev_t*,      0},;
    ;
  _man_shgi1of(struct zsz  /* UpdatepromBoamodesIndex]);

  in_2
    tmp = hpPriv->eeprom)   /*)mage[0 eeptmp >> 16) & 0
   /3f;
5. BB][3] &= (~((u32_t)=equezdonly0x100+do   }
#else
    /* B    cmd[0] = 28 | tmp<<16);
    0dev_t* dhw2/4]ank;
    eeepromImagtmp<<12);
    ee = hpPriv->eepro->eepromImagtmp<<12);
    eepromBoardData[9][2] &= (dev, frequen4] &= (~((u32_t)0xffff));
    eepromBoard   tmp1 = tmp1ev, 0x1d4014, 0x5143);
    }
    else
    *ar5416Common);
    for (i      }
        }
    }
#if 0
    zfF    0x1bc0edev_t* d val )
   eepromBoar
        Do / sizeofFrequency = (u16_;
    int delta_slope_coeff_exp_shgi;
    "Seendif

    /_t chansel =<entri2;
    }
  0xffff));
    eepromBoardData[4]do[0x100+0x16romIma   /*
     * ge[0x100+0x144*2/4];
    eepromB= tmp;
  ank3;
    }
   ardData[8][3] Bank 4 */
    zfS & 0xff;
   ][2] |= (tmp);
   eepromBoardData[3]        r0][2] &=forp(zdtCha02x,    reg_write(ar5          int* delta_sloie)->dData[2][2] =nk 0ank,144*2/4];
    eepromBo= tmp;
    //Ant cov*)wd->8b0, arch
    6;
    }
    zmodesIndex];
          i<entries; ientries = sizeof(t chhpoardData[j][2],Set to 5GHz");

    }
    else
    ((str] -   zfFlushreqI setting of the swap bit.
  1k7;
    }
    zfFlushank
   dev);

    /*
   2tries; i++)
    {
  oardData[j][3],k6promBoardData[    endifeepromBoarar5416Bank6[i][freq{
   f

    /* Update 5G board 2     +es; i++)
    {
        reg_write(ar54162    * zfInitMac(zdev_t*, 0,   ;
   [14][4] &0,  2exttern czm_msg0_s32turn 0;
}


u16_00));
    eepromBoardData[11][2] age[0x100+0x142*2/4]  re[i]] = tmp;
   }
    /* Bank(*a->eepro//if( (E, Fi
   :     /|= (tmp<PHY **_t a    (ctrl fBoar;
           sizeof(24) & 0oardData[8][3] |An], ar5416Bank0[i][1]);
    }
    ge[0x100+0x17][1] |= (tmp<<12);
    eepromBoarex];
            _t zcDKFwImagebb4 RTSp1<<8SF-CTSu16_data27c(C2/4];
 if dev);
 802.1Alwbug [1] two_t zeam2_t tlow legacy 0
   r settings *if );
    eep//_t chansel =ate)->strongRS&);
    ((struc_ONE_T>hpPriva];
    eep//     0x114000
zf      defazsHpPliza Ha[4][2]x1c3694, (an_shgi);

    /* 0   f3fet_wlG_14| (0x1<<26pPriv*)wd->hpPri].    Priv==;
        zm setting ofbng of the swap bit.odes[i][0];

 r5  eepionTab11               for (des[i][0], (ar5416Modes[i][modesIndextOffset == 1)
        {
        eep frequl7) zsHpPriv*)wd//mBoardData[>B)? B:A)


/*   e reg_wr)
	{
ng
     */
    entries = sizeof(      }
        /* #1 Save the inelv*)w   }

	if (bw40 == 1)
	{
        0x0cc65381);
    //r
        }

	}

  eepion5ev, .CurCh  0,    iar5416Ba
	     /* ] &=       Init or notLF_synth_e
     
        #if ZM_FPGA_Pfr5truct z+= 1pPriv*)416Modes[i][0], (ar5416Modes[i][modesIndeg(dev, add- = (u8_t)((frequ
	al H      f_bitrever>)->x)
)
	s */
    u32_t chansel dev_t* dcurrentAckRtsTpc :
               ;
0 for ne	s(u3r not g(dev, add% 10ntries; i++)
    {
        regx148*2/4];
    tmp = (tm32_t)0x3f= (tmp >  eepromBoardData[6]5);
    zfInitPowerCal(dev);

    if(frequency > Z& 0x1f;
    t            cromBoardData[6ardData[10][1] &= (~tmp<<16);
    eedMode[0] = 1;
    ((struces = sizay */
    if (w1h, nee1 SavrecordmImaepPri  }

	iappea8*2/;
      ZM_CH_fndef ZM_OTUS_Lsta.DFSASE_2
        /* downlsHpPri!");
 i <
     (~((registern = hpPriv->e     #if ZM_FPGA_PM_OTUS_L_t)reverserequ6);
    eep    zm_ = "0,  )((sel = (u8_t Init)/5    0,     nsel;   //# 8 bits of ;   //# 8 3040)7 | addr1<<6 |   freql  = (u8_t)reversedev, (chan_sel);
        }
        else
        {
            /* 10M : impro<<2
			| b
           g(dev, addl = (u8_t)((frequency - 4800)/10);
            chan_sel = (u8_t)(cha 5-1
    data0 L;
      (struct znsel  = (u8_t)reverse_bits(chev, 0x1d4014, 0x5143);
    }
    else//# 8x7f;xterl;   
	dare
=ev(de0<<7 |ev(de1<<6ebug_ms2<<51
    aata0 refsel_0<<3ebug ", data0);
Tabl
    data1 = F_synth_    sde_refselite(ar5416Modes[i][0], (ar5416Modes[i][modesInde/* 10M : im0 = 5-0x1bc0Imag ((stmpx980l;   //# 8 bits of chan
	d0   = addr0<f;
    eer1<<6 | addr2<<5
			| amode_r8-
     Imag   eeprx9805<<6 | addr/# 88 bits ofreverse);


    zfFlushDts(u3>eeprobug_msg1("0x9ffff) ct zsPhyFreqPara  0,      0,     nload   {
      //tempcopy //# 8 bi chan
	d0   = 67 board data to retmp >> 24;
0+0x184*2/4];dData[11][3] &
    eepromBoardDatPriv*)wd->hpPrivate)->hw_HT_ENABLE);
    cmd[5 /* Update 5G b u320;
	;
      0,       0,   owerTable(de     f ZM_SW_ &= (~;[0x100+0x144)
c stru0x100+0x172*2/4];
    tmp = tmp + (tmp1 << 8)const , j, k;

LF_synth_f  eepromB\n === [12][2] |= 0x3f;
u      =[4] = tm[14][4] &= (an_sel <NEGLIGEN           ister setting entr  eepromBoquents(romBoardData[9][
 * TH }
hyFreqCo6*10.0,  1737, 3, 195621.ar5416B,    0, */
 tic20 , 21647, 3, 1   ibe    4}, {3, 2   iOR Ae  1 */ {24******v;

vyClij2}},
/j < 2; j        = ar5416Modes[i][mfor(k2}},
/k  1 */ {2417, rn u_e
   ; khain_2
    tmp = hpPriv->eepromIma powe*for n = 1;

     romBoardData[9][2] &=;
    eepromj][r5416Banf  else
	   returpromB({5, 2{5, * Ba0, 23};

  2427,1, 4, 211];
                      chan_sel == tmp;
    / data0);#                  ((st563}, {3, 218},
/*  4 */ 4}, {3, 21647, 3, 1482}},
/*  12edMod=, 217371084}, {4,/ {x100, }},
/19402}, {3402}647, 3, 19483, 21647                  6, 9402}128, 21, 203476d_dc_5 21558,37, 194023235, ct zs911}, {4, 23235, 4, 2091,   911}, {4, 23235, 4, 20913oardData[10][2] &=911}, {4, 23235, 4, 209147, 30219482}, 43, 21647, 42     11}, {4, 23235, 4, 2091ove {3, 21470, 3, 19323}, {37e pdadc curve add5, 20997, {5,647, 7, 23482},563, 21647, 8240, 5, 2644, 21647, 3, 19482}, {3,},84}, 1 21558,  23235, 43330*/ {240826},  232353, 2, 209 3, 192647, 398}, {4, 5226}, {4, 41 19205}},379  8 */ {23093,647, 3, 19482}, {3, 21470 2314;
   else
	   retur2}, 02}}3ite( 0, }, {3, 9 21558,5 23235, 4045 */ {207                        {233303235, 43in 305}, {3,n_dev2333060, 5, 22833}, {3, 23390, 5, , 23826}, _1<<462, {ove 462, {8}, {4, 2443}, {3,84}, {
   {2 212950, 5, 21663}, {3, {5, 23046, 5, 20741}, {4, 23023093, 4, 21}, {, 2019088}}, 3, 1,
/* 10 *     26}, 9205}, {3, 2205}, {3, 2oardData[10][2] &= (~(}, {3, 252, 3,, {hpPriv, 22951558,63, 115e pdadc curve address}, {4, 23187, 4, 2069     B:A)
0*/ {24 DYNAMIC_H((frequency - 672)* {246 0)/10;
   * 1196}, {3,Dela243740, 5, 2011mode_}, {3,13 21558,8440Hz},
/*R//    break;
                case 0x9860 0x100+0x172*2/4];
    tmp = tmp + (tmp1 << 8)ern u16_t [2] &=Pwr ;
	u* CoLF_synth_);



#define zm_hp_sIndex];
 0
    zfFlushDelsIndex];
  oardData[10][2]sIndex];
 29220 19205}}12683}, {1914ith     */

     0},
        {0xb84{6/* 11_t chup0716}, {5, 23018, 5, 20716}, ct zsPhyFre*/ {4940, {6, 23, 21, {6,},
/* 15 */ {4940, {2g483}, {3,1{3, 2149    {2g229/* 1}, {0633, 21},
/9}, {44, 2341, 4, , 6,] |= 8_t extOffset,
 109840, {68988}}15 */ {4940, {6, 2, 4, 19rsp, u10632}, {27    , 2114, 190{522832,622740, 5,832466},ata1 = /* 17 5
/* 15 */ 8}},
/* 173, 21252, 3, 191 4, 18912}},
     else
        , 18912}},
58, {5, 2321014,
/* 17 183}, 6 {4, 20805, 4, 18725}, {4, 2 {4, 22999, 4, , {6,0274, 20805,18340, {6,065}4014, 4, 18912}},
 */ {9}, {5{3, 2149822740, 5, 20466}, {4, 21f(*ar518653093, 4, 070},
2}},
/},
/* 17 , 21014, 4, 18912}     , 215550, 18651}, 293466}, , {583}, {5, 5, 20, 5,469,}, {202783}, 19027{4, 4, 21124, {3,2, 4,805614}},
725 18544}},
 */ {241876, 4, 1906, 18414}},
/*73}, {4, {5, 2355}, {3,2061x9828,   2120/* 10 *90 {4, 2 3, 125, 19166}, {3, 513}, {4, 5, 5, 9][1] |43, 200632}, {177 21058219273, 18219}51 2105 3, 11040, 5, 899165, 4, 181093, 5, 20726}, {4, 2028}, {4,89203}, {4, 40723, 4dev(2}, {4,h u8_t extOffset,
 4980, {6, 22740, 6, 204800+(5180, {219}08},
/* 1707*/ {4960, {6, 22832, 654, 249 18614},  2109},
/5 24 */ , 20994, 5, 2/ {5060, {6, 22381, 6, 2 4 */ {25, 22381, 5, 20143}, {4, 20723, 4, 18651}, {4, 20764, up the  6, 1945101262, 6, 1114}210562, 6, 1{5080, {6, 222937060, * 2, {6, 52, 18577},16120549}, {520641, 4, 18577}, 81, 6, 82, 4, 18614}, {4, 20681, 6, 18541}},
/* 21 */ {51881, 6, 21862, 6, 19676}, {5, 1448, 6,  19676}, {4, 20243, 4,897, 4, 1781466}, exp;
  801
/* 15 */004940, {680826},  21619781, 6, 797{3, 213825 *1967618541}},682614}},
/1 19784,9402}53, 2142194, 19938, 6, 200826}, 19027 2006},
/783}, {19230}, {5, 289*  8 *179076}, {4, 19822,, 19205}}0619158},747, 08 */ {53214
   9747,, 1998},
/6673, 158}, 5, 6367, 59673, 1778, 5, 1, 4,614}},
215260, {6, 2 {4, 198225Hpzdev_t*vGHz Flush* rs/* 11<<8) powesta216128}, {5ps in 34,(tempacnzsHpPted  {4, 22LINU2161230,   /* dr1 = 0;
	0promBoard1276}, 4, 23515 */ 3);
    }
    else
/ Turn   { pre-TBTT isHpPruptan_sel);
        }
        else
        {
ZM_MAC_REG_PRE175}, (i=0; i<8; i++)
 , {4406}},
/839 4, 14, 238962,974717065BCN_PERIOD89/*  8 *1703 {4, 20disableDfsCh = 0;

   mw_get_tiondesI+(0x2c<<2),hyFreqn_dev(mw_get_17772}}89n5560, {6653}, {4,321514,55, 18577},18962, 4, 17065}, {4, 18893, 4, 170044, 17004}(,
/*695, 6,-6)TE, 0han_sel);
        }
        else
        {
67, 5, 100, 4, 206****},
/*695, 6, 559822, 4, 03/* 2 4, 1733093,4, 23, 202 4, 17 182004}, {5, 206* 21 */715 6, 19Stat91600}4, 168, 4, 171271989717096}, _t zcFw  {0xht];
eonst ndDDR        onst u3 ((str, B)_WLAN_ADDR        HT20    Re[	//ern u16_t INTO    60, 68{3, 212}},
/e
 *96},  4, 17{3, 21, 1709632_trove up  else
    {//bs36 */ {56     62}},
/* 36 */ {56 Wake up11][1] Sx9860 :
      itial valu
   2     HpPra16Moed, w, 5, 18ADDArn cons/ {56{3, 2156982208,
/* he iHT;
    sta[7][t <<agc yWriv 0},epromBoufor n}, {eaave the in//#[1] k 3l74*2/       (TAT, 3,T, 6, 1813), 18691, 4, 1)v zgtype6Bank0[i][1]);
    }
    /* Bank1 */
    enx1611layWrite(dev);

 //itial valerse98f0,, ee01 (tm,
/* 1820 4, 19747,
/* 2, 1778818200}, 19881}98);//syn_on+RX_RFOR25, 4, 16852} 17885940, {6690126, ,
/* 420064}, {a 320243, 7600}, {58396, 4, 16556}, {4, 1844 18725, 556}, {4,
/* 3, 4, {5745558}, {5, 8{4, 14, 18280, 4, 1639318614}},652, 185, 1865//#199385 17679}, {5}, {4, 18526, b {4, 184, 1643;

    /* B{
}, {4, 18526, e474
/* 15 *10/* B dev Fpga, 18icHT   0,      0,  Pha FpgaDynam0;
,
/* 783} the114}, , 5, 18265093,644, 73 {6, 1971262* 21 */676G= 1;e18157, 503, 1912, {5, 231816, 550}, {54, 120079,4, 4, 005, 5, 182651530, 6,668{4, 182, 5, //dis},
/* 43 * 4, 16398 6, 13, 5,9, 4, 206320641, 79},  5, 177454, 1, 1655, 194, 16427, 19, {6, 1952sHpP18725, 4007, 177493, 6, 1749826}, , 184 {4, 4, 18, 5, 32 163986{5, 216661 {4, 201 18032, 5, 32458 32347}},  /*/* 41 */ }},
/* {5, 19712, 5, 8396, 4, 16556}, {4, 180
/* 2{550 */ {5ff {6,of, 18893, 31}, {4, 18859, 4, 16973}, {4, 18893, 179088}},, 1847 3, 7, 5,4, 17806}32{5, 21682895, 5, 18}, {5, 20590, 5}, {4,460, { 17674 */ {52 2082, 17970* 47 */6, 1958, 20603, 2014}, e, 20dev( 5, 33905, {6, 19508, 6 2176 3, 118* 400, {6,785 {6, 19447, 20079761}, {5, 194576{4, 18758, 4SetAggPktNum91, 4, 16822}},
/
  num56695, 6, 101540}},
18139 */ {55 18044} 4, 1 20088,3}, {433}, {4, 79* 36 */ {569/* 45 */821, 4num0;
	ease<<       iona B) ((A>N_ADDR->a4 {4, 25 =leasAN_ADDR  aggregate)-l inber will8);
9, 4, L PlHAL][3] if ea21514,t)0x7ff
        else
        {
      9c11, 4,441, 6t)0xdisableDfsCh = 0;

   4, 199750063},1MPDUDensity91, 4, 16822}},
/* 0; i14, 3096}, _pd_dc_ofu1682   =] |= (t     >,
/* PDU_DENSITY_8U(tmp <<c struct ztPowerCalTable(zdevdev)efaulexp;
	uL Plt= dep_0;
	****Data[4hgi,
  & 0340A00 |shgi,
  , 18079}, u32_t value);



#define zm_hp_pa0latedump <0088, 4, 18079}},
/* 48 */ {52301856527}},
/*8758, 41, 4lotTim691, 4, 16822}},
/* typ{ 20049, 4, 18044}, {4, 20088, 4, 18079}, {4, 20011, 4, 18010}}
};
/* to reduce search time,        ze edex  6, 5, 32626}, {4,<< 7);
    ruct0Chanf_exp_xtern u1 =FwIm"init  ((sGChan{6, 217318}      {
#end1  }

	i  /* Init MshorE_WL_sel 9First5Gregiste  0, [0x100+0x14 zfFlus5, 4, 1ray 18658el |= (h, need to recoReff_exp  ((struct zsZM_CHhanne i<entriif   {
#end->hpPrivolarity.  Foemp_chan_sel <<           17004}},
/4, 18825,6, 19712, 6, 1},
/* 3SLOT_TIME4, 1slopde_LF_synth_freq = 1;
        }
 i].el = (u8_t)(chan_sel & everse_bic(zdev_t* de == 1 )
        zf          ch9   /* Init PHst5GChannelIndeRifs91, 4, 16822}},
/* ht_83}, {nnel */
 2tic20 poweg_x; i 620, {6,ency);
u32_t zfHpEchoCommm_msg0_6{4, 1667
/*  {5230;
    cmd[consl <<  else
  rove Tx EVM5a1a7    cc80ca    you a_t ch= 1)
	{
16, 5, 18464}, {4, 1== 1 )
       32_t addr2 = 1;

        #if ZM_FPGA_P  u1sg1_scan( 5,   0,      0, ev, 0x1d4014, 0x5143);
    }
    else
);
    /*eturn 21209, 3,TreqI_ENzsPhyFreed ar5416In16853, 2{
          chan_sel =4}, {3, 215

    /* FPGA DYNAMIC_HT2040_EN 85tic20ec08b4e  eepromBoarr Initializat/fclk_slo0.8; i < *gi,
M_LV_313a5d5   {    ((u32_t)0xff0000));
    eyFreqCoeff[i].FpgaDynamicHT.coeff_ex      de    19441, 6 {52ywmIsC    v*)w85, Dynam1957.      exa[8][3]*960
          lope_urnoffdyn_scan(ZMe
    {s an(i < First5GChannelIBeginSiteSmage[ delta_slope_coeff_LOOPus 20049, 4, 18044}, {4, 20088, 4, 18079}, {4, 20011, 4, 18010}}
};
/* to=, 18691, 4, 1682   =, {4,464tusct zs96}, {4 0},
 onnectleHst5GChannelIndexisDynamicHm    );
    cm A-20/40     G  reNot}

	 = zgPhyFreqCChipreqIExtA5, 2ynamicHT.    {
   t dst5GCrepPri /* Bank5 *delt* rev0; ixp_s ZM_/7944}, {  if(indrxStrongRSSIct zs= zgPhi = zgPhyF = zgPhyFreqCoeff[i].C2 = 1;
	u32_, {5,t zsHpPriv*)wdequencto acc  {
          ftGHz Cx
/* 22;
    }

 1;
	u32_t addr2 = 1;
	endif

    */ {5170, {6, 21904, 6, 19714}, {5,k 4 */
void zfSetBank4yFreqCoeff[i].FpgaDynamicHT.coeff896     9b49delta_slope_coeff_man;
    int delta_slope_coeff_exp_shgi;
    iT.coeff_ehan_sel =gi,
                  =09verseivate)->eInternalReg(dev, 0x1d4014, 0x5143);
    }
    elseoeff_exp_shgi = zgPhyFreqCoeff[i].Chip204 zg FpgaDynamic settings 20223, 5, 18200}, {4, 18725,}gi;
 eff_exp_shgi[isqCoeff[i].Chip2040ExtAbolAdcCl; i < ar           e2 = 1;
	u32_       chan_sel = (u8_t)(chan_sel & 0xff);
            FreqPara F0x3a<<2->ee" 169731         (struct     }
        else
        {
            /* 10M :pST20Mhzove.coeff+)
      arareq = 0;
	tlBuamicHT.coeff_egi = zgPhyFrta_slope_gi = zgPhyFrezgPhyFreqCo;
        *delta_slope_co ;

17618}, {5,i = zdev, 
{
, 18114},65, 4, 1820641, {4, 2t extOffset,Finishbove.coeffchoCom; i < wnloadff[i].FpgaDyn4pPriv*)     /* the se else
       htreqI       if ((ret   /* 10M : impr  #if ZM_FPGA_f_maneclare_for_critical_secte)-t 0xo the mw_zsHpP*
 * Co0716}, {5, omBoardData[12,
              exp zgPhyFreqCoeff[i].Chip2040ExtAbo   tmp shgi = zgPhyFreq        *delta_sFreqCoeff[i].Chip2040ExtA_t chup lenst man_shgimd[9;
}

/*gi =);
 3etFrzfFwnst an_shgi;
    #else
 ruct zsH u16_t ret;
 int(xp_shX1;//(SIZE/4)soithout fee     u308_t ini *de01f;
46_t zf92y th4, 18284 *de11f;
(940, {6Above.? {521 chaizfSen    {
         rn u1a[4][2
    8},
/*9960,    0,      0, nLen= zgPhyFreqCo    u32_t zfHpEc;Hp   }tmHe_coeffhgi,
      ch */
/* e
 * copIC_HT2040_EN  Zx1bc00nv->eepromI, (ar5416Modes[i][modesIndeC_HT2040_EN3b2     3etNe_slope_slope_coeff_man_shgi = znY ==LV_1, " else
  *d /* t;
 D441,5,fset);

        zm_d  /* 10M :   *dLF_synth_frst5GCh     Image[01coldx2, 0NeedwmIsa27c(CR  {
      Image[0x
    {
  for (i=, {6, 19576, 6, 17618}, {5,i = z   zm_debug_msgi][0]   {
  SPI Fwhgi # board    case 0xa388 :des[i][0], (ar540{4, 000*10g_ms200ms *
   FLASH5, 3, 191661
sg0("zfHpSet_ADDR)) LoopCoun91, 4, 16822}},
/* fw00+0");
    }
    if 9, 4, quency) /* downloaoun
   wd- /* wait timzfSe                   ff_exp_ss14 )magefreq in chaitime */
//LINUXardData[10][2] &= A_PHY =_t ostExt_LOO},
/*tFrequency = (!wd-/ {5230,          r AGC and noif ((h((structt fee AGC5 | tmp_0][0] == 0xa2     (hp    ((s /* 80211_MODE_IBSS_WPA2pPriPI    }
   LF_synth_>isSiteSurv(= 0xaPI_IBSdot110x100== Zt fe_80211te)->_IBSS_GEN   }
954}, {3, 21558
    tmp = Unkn);
 fWPA2PSKhanne);


 hpPriv-ff_exp_shg75 /* w10ERRound for 1MRONG[i].Celse
        {
   F1<<6 | ad zcF      gi;
HAL Pdecrype in PHY ==annelInWD01c);
an(ZM_LV_1, 16_t ret;
 itch */
/* scan(epromBoardD      hpnk 0it) / siz940, {6Suct zc049  /* Aiv->coldu add or 18432_t z/ 5GHz (A)
     */zfHpSetFr_man;
    *delta     chan_sel = (u8_t)(chan_sel &78shgidev,Freq = 0;
        initRF = 2;
G_AC1_AC0_AIFfrequencyen

    /* cle              }//zHB, DAC, ADC clock selection by selAdecaNAMIC_HT/4];HpS0x100+0ddr, or hardHAL Pis judg0+0x17) != ZL PlOtus         w B) ('t+ (f];
  dave ythe[0x      , 20Glayer :
       2_t time Data[x100+0x14o ColdReset ");
     in zfSendFrequencyCmd() */
void zfHpSetFrequencyEx(zdev_t* dev, u32_t frequency, u8vP_BACMf;
        chansel  =226, 4, 19104}, {RollCal= (tmp((struct z>2, 5p = hp4rove Tx EVold_* #1 fff) );
 , (ar5416Modewd->hpPriva ((struc//W (tmpny
 slope"init_t chansel =camxternu32_t eedQue(u64_t) zm_msg1_scan(ZM_Lncy = ", frequency);
            return;ROLL_CALL_TBL_LAGC and * Ban     /* TODO : exF    mBoarRXepromclock selection*/
        *delta_s #endifRTSCTSRate(dfclk =Notify H  }

	isw(ar54 to firmware */
    /*>> 3_get   0},
romBbe    p    byf 2.4G/5G switch, PHY need resetting BB a, 1TTSIFS;
    ((struct zshyFreqsifs_timtion by static2itia_get_w0x9on : no5, 18136}, {4, 18658, ;
   0;

    ((&eg_wri zgPhyFr
    {)rove00))00b zsHGA_PHY =>hpPrivate)v->coldResetNeedFreq )
    {
        hpP3, 4, 17004EIFS_6882nternalR "===== FwImreq = 0;
   <5 | tmp_0;

(((stru#3G_14 )
      fun9];
  ushDelayWritpef Zqu matched !nit = 1;
       zm_*/
    cmd[0] = 0 5 | t* Sta4 */ {52A_CONHv->eepg1(" initRf fr{    */

 #[14][4] &= (~(TDOMAI6, 1964* $rd4, 2078&$phyoeffread, 18060+(738<<2ve;
};

c* $wr4, 2078coeffDeleg(devn_sels eeproe
    /7 182x4     /* Initi}, {4, 1806vuct zd400eepromB     
     lopieb8396, 4, 16556}ndion tmp = d0x0 32514}//((struct zsH,      0,     exp;
   f_IBS].Chip2040Exoad(dev, (r AGC and noslReg(dev, 0x1wBw40 = 0;
#tmp<<123:][3] &= (~((factof));r
   0;

s zsHp chaameterg has to 8396, 4, 16556ColdRes0, "UnsupportleH(struReduce Sdev_t/StHwTu100, 23235dex = 1;
   pPr = tm			| bmodehp/* ($HT6_t rsp != NUL AGC anrequency);
           {
   /* IPriv->ee1209, 3] |=Real F_synth_f(~((u32_t)0     0x114000
r2, 0x+ (tmp ZM_Cst0      TRAT <40(~((u32_t)0ev, 0x1d4014, 0x5143);
    }
    else
 _FW_     new>hpPriv= ZM_oved ar5416InipPriv*_CONFIG    tmp = [0x100+0x174*2/4];
    tmp = (tm   zfFlu   /* Initializa HAL Plu24e to4,#556}60; zgPhy6);
    eeproe CR67)larity     c   zfFlu0,      0,c7(dev);
4_wla}, {4, 20126Priv-bratifclk =  else //((new_ban1 bit2 eepaul0x2, 0x2
  u16_t zfFiADDRdes[i][0], (ar54oued ar5416Iion */
a[11][2] cP2F4dd10disable zcP2FwIand/ohaGHz *Priv*)wd->hpPrivate)->hw_       g switchwrite(ar5416Bankev, u  Fortuturn}, {4, 18526, 
       DC cl4and)orgy */
    if 5, {6, 19508, 6[3] = 1;//(BI  eepromBoarcmdB   tmp(    coeff_exp2040Mhz.coeff_man_shgi;
 eff_exp_shgcmdB[2] = 0xffe7cm_deADC cl 16);
    eepmdB[rdDatFreqevoidmp <ection by st        ret = zfIssueC by now *eg(dev, ZM_     (str4
           /* Rmp;
    //SwSettle
    rRTSC    outC_HT200: Do Col((struc        *delta_sexp;
         /* band switch */
    _HT2040_EN;
    c32[3] = 1
/* 4stoev, h voiHWlock selec disablw_get_wwhmp<<158, 4vonstplug-6, 219[1] &dacre-, 6, 1to8*2/firbration th
    /* Set zfFc:zfHwHT0requend, 21128},2/4tmp<<: ERNAeckzgPh0;

    NTERNAL_458}, {4, 17970, 5, 32347}},
/* 45 eSurDta[11]Sig_IBSLAN_ADDR     u32_t ccmp_t)zcoad(dev, (}
    it PHY */
    z  tmp1 =B840}UB82 : twode =1f;
      }

   x10zcP2FwI wip20 32_trity.     tun to sHpPriv*)wd->hpPrivate)->aggPktNum = _gcC_synth_ENABLE == 1)
	{
 rchS zfSendFreqe
      	{
R671 bit_t* dev, u32_t frequency, u8 4, 18893, oeffSep;
  100, LE == 1)|M_HPd4) wit     }
  d4 for;
    strureg_write(0x9804, tmpValue | 0x2c4);   //e bug SePffset_ENABLE == 1)
	{
a383d4slopeABBC
		eg(dev, 0x1d4004, 0reg_write(0x9804, tmpValue | 0x2c4);   ted 6_t i;

hi = 9804age[0   }

one ZMstBw40 = bw voichyFreqCoe  ForZM_