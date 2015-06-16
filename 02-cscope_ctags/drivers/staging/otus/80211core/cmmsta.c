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

#include "cprecomp.h"
#include "ratectrl.h"
#include "../hal/hpreg.h"

/* TODO : change global variable to constant */
u8_t   zgWpaRadiusOui[] = { 0x00, 0x50, 0xf2, 0x01 };
u8_t   zgWpaAesOui[] = { 0x00, 0x50, 0xf2, 0x04 };
u8_t   zgWpa2RadiusOui[] = { 0x00, 0x0f, 0xac, 0x01 };
u8_t   zgWpa2AesOui[] = { 0x00, 0x0f, 0xac, 0x04 };

const u16_t zcCwTlb[16] = {   0,    1,    3,    7,   15,   31,   63,  127,
                            255,  511, 1023, 2047, 4095, 4095, 4095, 4095};

void zfStaStartConnectCb(zdev_t* dev);

/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfStaPutApIntoBlockingList  */
/*      Put AP into blocking AP list.                                   */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      bssid : AP's BSSID                                              */
/*      weight : weight of AP                                           */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      none                                                            */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen Chen        Atheros Communications, INC.    2006.12     */
/*                                                                      */
/************************************************************************/
void zfStaPutApIntoBlockingList(zdev_t* dev, u8_t* bssid, u8_t weight)
{
    u16_t i, j;
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    if (weight > 0)
    {
        zmw_enter_critical_section(dev);
        /*Find same bssid entry first*/
        for (i=0; i<ZM_MAX_BLOCKING_AP_LIST_SIZE; i++)
        {
            for (j=0; j<6; j++)
            {
                if(wd->sta.blockingApList[i].addr[j]!= bssid[j])
                {
                    break;
                }
            }

            if(j==6)
            {
                break;
            }
        }
        /*This bssid doesn't have old record.Find an empty entry*/
        if (i == ZM_MAX_BLOCKING_AP_LIST_SIZE)
        {
            for (i=0; i<ZM_MAX_BLOCKING_AP_LIST_SIZE; i++)
            {
                if (wd->sta.blockingApList[i].weight == 0)
                {
                    break;
                }
            }
        }

        /* If the list is full, pick one entry for replacement */
        if (i == ZM_MAX_BLOCKING_AP_LIST_SIZE)
        {
            i = bssid[5] & (ZM_MAX_BLOCKING_AP_LIST_SIZE-1);
        }

        /* Update AP address and weight */
        for (j=0; j<6; j++)
        {
            wd->sta.blockingApList[i].addr[j] = bssid[j];
        }

        wd->sta.blockingApList[i].weight = weight;
        zmw_leave_critical_section(dev);
    }

    return;
}


/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfStaIsApInBlockingList     */
/*      Is AP in blocking list.                                         */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      bssid : AP's BSSID                                              */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      TRUE : AP in blocking list                                      */
/*      FALSE : AP not in blocking list                                 */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen Chen        Atheros Communications, INC.    2006.12     */
/*                                                                      */
/************************************************************************/
u16_t zfStaIsApInBlockingList(zdev_t* dev, u8_t* bssid)
{
    u16_t i, j;
    zmw_get_wlan_dev(dev);
    //zmw_declare_for_critical_section();

    //zmw_enter_critical_section(dev);
    for (i=0; i<ZM_MAX_BLOCKING_AP_LIST_SIZE; i++)
    {
        if (wd->sta.blockingApList[i].weight != 0)
        {
            for (j=0; j<6; j++)
            {
                if (wd->sta.blockingApList[i].addr[j] != bssid[j])
                {
                    break;
                }
            }
            if (j == 6)
            {
                //zmw_leave_critical_section(dev);
                return TRUE;
            }
        }
    }
    //zmw_leave_critical_section(dev);
    return FALSE;
}


/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfStaRefreshBlockList       */
/*      Is AP in blocking list.                                         */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      flushFlag : flush whole blocking list                           */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      none                                                            */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen Chen        Atheros Communications, INC.    2006.12     */
/*                                                                      */
/************************************************************************/
void zfStaRefreshBlockList(zdev_t* dev, u16_t flushFlag)
{
    u16_t i;
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    zmw_enter_critical_section(dev);
    for (i=0; i<ZM_MAX_BLOCKING_AP_LIST_SIZE; i++)
    {
        if (wd->sta.blockingApList[i].weight != 0)
        {
            if (flushFlag != 0)
            {
                wd->sta.blockingApList[i].weight = 0;
            }
            else
            {
                wd->sta.blockingApList[i].weight--;
            }
        }
    }
    zmw_leave_critical_section(dev);
    return;
}


/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfStaConnectFail            */
/*      Handle Connect failure.                                         */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      bssid : BSSID                                                   */
/*      reason : reason of failure                                      */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      none                                                            */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen Chen        Atheros Communications, INC.    2006.12     */
/*                                                                      */
/************************************************************************/
void zfStaConnectFail(zdev_t* dev, u16_t reason, u16_t* bssid, u8_t weight)
{
    zmw_get_wlan_dev(dev);

    /* Change internal state */
    zfChangeAdapterState(dev, ZM_STA_STATE_DISCONNECT);

    /* Improve WEP/TKIP performace with HT AP, detail information please look bug#32495 */
    //zfHpSetTTSIFSTime(dev, 0x8);

    /* Notify wrapper of connection status changes */
    if (wd->zfcbConnectNotify != NULL)
    {
        wd->zfcbConnectNotify(dev, reason, bssid);
    }

    /* Put AP into internal blocking list */
    zfStaPutApIntoBlockingList(dev, (u8_t *)bssid, weight);

    /* Issue another SCAN */
    if ( wd->sta.bAutoReconnect )
    {
        zm_debug_msg0("Start internal scan...");
        zfScanMgrScanStop(dev, ZM_SCAN_MGR_SCAN_INTERNAL);
        zfScanMgrScanStart(dev, ZM_SCAN_MGR_SCAN_INTERNAL);
    }
}

u8_t zfiWlanIBSSGetPeerStationsCount(zdev_t* dev)
{
    zmw_get_wlan_dev(dev);

    return wd->sta.oppositeCount;
}

u8_t zfiWlanIBSSIteratePeerStations(zdev_t* dev, u8_t numToIterate, zfpIBSSIteratePeerStationCb callback, void *ctx)
{
    u8_t oppositeCount;
    u8_t i;
    u8_t index = 0;

    zmw_get_wlan_dev(dev);

    zmw_declare_for_critical_section();

    zmw_enter_critical_section(dev);

    oppositeCount = wd->sta.oppositeCount;
    if ( oppositeCount > numToIterate )
    {
        oppositeCount = numToIterate;
    }

    for(i=0; i < ZM_MAX_OPPOSITE_COUNT; i++)
    {
        if ( oppositeCount == 0 )
        {
            break;
        }

        if ( wd->sta.oppositeInfo[i].valid == 0 )
        {
            continue;
        }

        callback(dev, &wd->sta.oppositeInfo[i], ctx, index++);
        oppositeCount--;

    }

    zmw_leave_critical_section(dev);

    return index;
}


s8_t zfStaFindFreeOpposite(zdev_t* dev, u16_t *sa, int *pFoundIdx)
{
    int oppositeCount;
    int i;

    zmw_get_wlan_dev(dev);

    oppositeCount = wd->sta.oppositeCount;

    for(i=0; i < ZM_MAX_OPPOSITE_COUNT; i++)
    {
        if ( oppositeCount == 0 )
        {
            break;
        }

        if ( wd->sta.oppositeInfo[i].valid == 0 )
        {
            continue;
        }

        oppositeCount--;
        if ( zfMemoryIsEqual((u8_t*) sa, wd->sta.oppositeInfo[i].macAddr, 6) )
        {
            //wd->sta.oppositeInfo[i].aliveCounter++;
            wd->sta.oppositeInfo[i].aliveCounter = ZM_IBSS_PEER_ALIVE_COUNTER;

            /* it is already stored */
            return 1;
        }
    }

    // Check if there's still space for new comer
    if ( wd->sta.oppositeCount == ZM_MAX_OPPOSITE_COUNT )
    {
        return -1;
    }

    // Find an unused slot for new peer station
    for(i=0; i < ZM_MAX_OPPOSITE_COUNT; i++)
    {
        if ( wd->sta.oppositeInfo[i].valid == 0 )
        {
            break;
        }
    }

    *pFoundIdx = i;
    return 0;
}

s8_t zfStaFindOppositeByMACAddr(zdev_t* dev, u16_t *sa, u8_t *pFoundIdx)
{
    u32_t oppositeCount;
    u32_t i;

    zmw_get_wlan_dev(dev);

    oppositeCount = wd->sta.oppositeCount;

    for(i=0; i < ZM_MAX_OPPOSITE_COUNT; i++)
    {
        if ( oppositeCount == 0 )
        {
            break;
        }

        if ( wd->sta.oppositeInfo[i].valid == 0 )
        {
            continue;
        }

        oppositeCount--;
        if ( zfMemoryIsEqual((u8_t*) sa, wd->sta.oppositeInfo[i].macAddr, 6) )
        {
            *pFoundIdx = (u8_t)i;

            return 0;
        }
    }

    *pFoundIdx = 0;
    return 1;
}

static void zfStaInitCommonOppositeInfo(zdev_t* dev, int i)
{
    zmw_get_wlan_dev(dev);

    /* set the default rate to the highest rate */
    wd->sta.oppositeInfo[i].valid = 1;
    wd->sta.oppositeInfo[i].aliveCounter = ZM_IBSS_PEER_ALIVE_COUNTER;
    wd->sta.oppositeCount++;

#ifdef ZM_ENABLE_IBSS_WPA2PSK
    /* Set parameters for new opposite peer station !!! */
    wd->sta.oppositeInfo[i].camIdx = 0xff;  // Not set key in this location
    wd->sta.oppositeInfo[i].pkInstalled = 0;
    wd->sta.oppositeInfo[i].wpaState = ZM_STA_WPA_STATE_INIT ;  // No encryption
#endif
}

int zfStaSetOppositeInfoFromBSSInfo(zdev_t* dev, struct zsBssInfo* pBssInfo)
{
    int i;
    u8_t*  dst;
    u16_t  sa[3];
    int res;
    u32_t oneTxStreamCap;

    zmw_get_wlan_dev(dev);

    zfMemoryCopy((u8_t*) sa, pBssInfo->macaddr, 6);

    res = zfStaFindFreeOpposite(dev, sa, &i);
    if ( res != 0 )
    {
        goto zlReturn;
    }

    dst = wd->sta.oppositeInfo[i].macAddr;
    zfMemoryCopy(dst, (u8_t *)sa, 6);

    oneTxStreamCap = (zfHpCapability(dev) & ZM_HP_CAP_11N_ONE_TX_STREAM);

    if (pBssInfo->extSupportedRates[1] != 0)
    {
        /* TODO : Handle 11n */
        if (pBssInfo->frequency < 3000)
        {
            /* 2.4GHz */
            if (pBssInfo->EnableHT == 1)
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, (oneTxStreamCap!=0)?3:2, 1, pBssInfo->SG40);
            else
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, 1, 1, pBssInfo->SG40);
        }
        else
        {
            /* 5GHz */
            if (pBssInfo->EnableHT == 1)
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, (oneTxStreamCap!=0)?3:2, 0, pBssInfo->SG40);
            else
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, 1, 0, pBssInfo->SG40);
        }
    }
    else
    {
        /* TODO : Handle 11n */
        if (pBssInfo->frequency < 3000)
        {
            /* 2.4GHz */
            if (pBssInfo->EnableHT == 1)
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, (oneTxStreamCap!=0)?3:2, 1, pBssInfo->SG40);
            else
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, 0, 1, pBssInfo->SG40);
        }
        else
        {
            /* 5GHz */
            if (pBssInfo->EnableHT == 1)
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, (oneTxStreamCap!=0)?3:2, 0, pBssInfo->SG40);
            else
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, 1, 0, pBssInfo->SG40);
        }
    }


    zfStaInitCommonOppositeInfo(dev, i);
zlReturn:
    return 0;
}

int zfStaSetOppositeInfoFromRxBuf(zdev_t* dev, zbuf_t* buf)
{
    int   i;
    u8_t*  dst;
    u16_t  sa[3];
    int res = 0;
    u16_t  offset;
    u8_t   bSupportExtRate;
    u32_t rtsctsRate = 0xffffffff; /* CTS:OFDM 6M, RTS:OFDM 6M */
    u32_t oneTxStreamCap;

    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    sa[0] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A2_OFFSET);
    sa[1] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A2_OFFSET+2);
    sa[2] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A2_OFFSET+4);

    zmw_enter_critical_section(dev);

    res = zfStaFindFreeOpposite(dev, sa, &i);
    if ( res != 0 )
    {
        goto zlReturn;
    }

    dst = wd->sta.oppositeInfo[i].macAddr;
    zfCopyFromRxBuffer(dev, buf, dst, ZM_WLAN_HEADER_A2_OFFSET, 6);

    if ( (wd->sta.currentFrequency < 3000) && !(wd->supportMode & (ZM_WIRELESS_MODE_24_54|ZM_WIRELESS_MODE_24_N)) )
    {
        bSupportExtRate = 0;
    } else {
        bSupportExtRate = 1;
    }

    if ( (bSupportExtRate == 1)
         && (wd->sta.currentFrequency < 3000)
         && (wd->wlanMode == ZM_MODE_IBSS)
         && (wd->wfc.bIbssGMode == 0) )
    {
        bSupportExtRate = 0;
    }

    wd->sta.connection_11b = 0;
    oneTxStreamCap = (zfHpCapability(dev) & ZM_HP_CAP_11N_ONE_TX_STREAM);

    if ( ((offset = zfFindElement(dev, buf, ZM_WLAN_EID_EXTENDED_RATE)) != 0xffff)
         && (bSupportExtRate == 1) )
    {
        /* TODO : Handle 11n */
        if (wd->sta.currentFrequency < 3000)
        {
            /* 2.4GHz */
            if (wd->sta.EnableHT == 1)
            {
                //11ng
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, (oneTxStreamCap!=0)?3:2, 1, wd->sta.SG40);
            }
            else
            {
                //11g
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, 1, 1, wd->sta.SG40);
            }
            rtsctsRate = 0x00001bb; /* CTS:CCK 1M, RTS:OFDM 6M */
        }
        else
        {
            /* 5GHz */
            if (wd->sta.EnableHT == 1)
            {
                //11na
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, (oneTxStreamCap!=0)?3:2, 0, wd->sta.SG40);
            }
            else
            {
                //11a
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, 1, 0, wd->sta.SG40);
            }
            rtsctsRate = 0x10b01bb; /* CTS:OFDM 6M, RTS:OFDM 6M */
        }
    }
    else
    {
        /* TODO : Handle 11n */
        if (wd->sta.currentFrequency < 3000)
        {
            /* 2.4GHz */
            if (wd->sta.EnableHT == 1)
            {
                //11ng
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, (oneTxStreamCap!=0)?3:2, 1, wd->sta.SG40);
                rtsctsRate = 0x00001bb; /* CTS:CCK 1M, RTS:OFDM 6M */
            }
            else
            {
                //11b
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, 0, 1, wd->sta.SG40);
                rtsctsRate = 0x0; /* CTS:CCK 1M, RTS:CCK 1M */
                wd->sta.connection_11b = 1;
            }
        }
        else
        {
            /* 5GHz */
            if (wd->sta.EnableHT == 1)
            {
                //11na
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, (oneTxStreamCap!=0)?3:2, 0, wd->sta.SG40);
            }
            else
            {
                //11a
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, 1, 0, wd->sta.SG40);
            }
            rtsctsRate = 0x10b01bb; /* CTS:OFDM 6M, RTS:OFDM 6M */
        }
    }

    zfStaInitCommonOppositeInfo(dev, i);

zlReturn:
    zmw_leave_critical_section(dev);

    if (rtsctsRate != 0xffffffff)
    {
        zfHpSetRTSCTSRate(dev, rtsctsRate);
    }
    return res;
}

void zfStaProtErpMonitor(zdev_t* dev, zbuf_t* buf)
{
    u16_t   offset;
    u8_t    erp;
    u8_t    bssid[6];

    zmw_get_wlan_dev(dev);

    if ( (wd->wlanMode == ZM_MODE_INFRASTRUCTURE)&&(zfStaIsConnected(dev)) )
    {
        ZM_MAC_WORD_TO_BYTE(wd->sta.bssid, bssid);

        if (zfRxBufferEqualToStr(dev, buf, bssid, ZM_WLAN_HEADER_A2_OFFSET, 6))
        {
            if ( (offset=zfFindElement(dev, buf, ZM_WLAN_EID_ERP)) != 0xffff )
            {
                erp = zmw_rx_buf_readb(dev, buf, offset+2);

                if ( erp & ZM_BIT_1 )
                {
                    //zm_debug_msg0("protection mode on");
                    if (wd->sta.bProtectionMode == FALSE)
                    {
                        wd->sta.bProtectionMode = TRUE;
                        zfHpSetSlotTime(dev, 0);
                    }
                }
                else
                {
                    //zm_debug_msg0("protection mode off");
                    if (wd->sta.bProtectionMode == TRUE)
                    {
                        wd->sta.bProtectionMode = FALSE;
                        zfHpSetSlotTime(dev, 1);
                    }
                }
            }
        }
		//Check the existence of Non-N AP
		//Follow the check the "pBssInfo->EnableHT"
			if ((offset = zfFindElement(dev, buf, ZM_WLAN_EID_HT_CAPABILITY)) != 0xffff)
			{}
			else if ((offset = zfFindElement(dev, buf, ZM_WLAN_PREN2_EID_HTCAPABILITY)) != 0xffff)
			{}
			else
			{wd->sta.NonNAPcount++;}
    }
}

void zfStaUpdateWmeParameter(zdev_t* dev, zbuf_t* buf)
{
    u16_t   tmp;
    u16_t   aifs[5];
    u16_t   cwmin[5];
    u16_t   cwmax[5];
    u16_t   txop[5];
    u8_t    acm;
    u8_t    ac;
    u16_t   len;
    u16_t   i;
   	u16_t   offset;
    u8_t    rxWmeParameterSetCount;

    zmw_get_wlan_dev(dev);

    /* Update if WME parameter set count is changed */
    /* If connect to WME AP */
    if (wd->sta.wmeConnected != 0)
    {
        /* Find WME parameter element */
        if ((offset = zfFindWifiElement(dev, buf, 2, 1)) != 0xffff)
        {
            if ((len = zmw_rx_buf_readb(dev, buf, offset+1)) >= 7)
            {
                rxWmeParameterSetCount=zmw_rx_buf_readb(dev, buf, offset+8);
                if (rxWmeParameterSetCount != wd->sta.wmeParameterSetCount)
                {
                    zm_msg0_mm(ZM_LV_0, "wmeParameterSetCount changed!");
                    wd->sta.wmeParameterSetCount = rxWmeParameterSetCount;
                    /* retrieve WME parameter and update TxQ parameters */
                    acm = 0xf;
                    for (i=0; i<4; i++)
                    {
                        if (len >= (8+(i*4)+4))
                        {
                            tmp=zmw_rx_buf_readb(dev, buf, offset+10+i*4);
                            ac = (tmp >> 5) & 0x3;
                            if ((tmp & 0x10) == 0)
                            {
                                acm &= (~(1<<ac));
                            }
                            aifs[ac] = ((tmp & 0xf) * 9) + 10;
                            tmp=zmw_rx_buf_readb(dev, buf, offset+11+i*4);
                            /* Convert to 2^n */
                            cwmin[ac] = zcCwTlb[(tmp & 0xf)];
                            cwmax[ac] = zcCwTlb[(tmp >> 4)];
                            txop[ac]=zmw_rx_buf_readh(dev, buf,
                                    offset+12+i*4);
                        }
                    }

                    if ((acm & 0x4) != 0)
                    {
                        cwmin[2] = cwmin[0];
                        cwmax[2] = cwmax[0];
                        aifs[2] = aifs[0];
                        txop[2] = txop[0];
                    }
                    if ((acm & 0x8) != 0)
                    {
                        cwmin[3] = cwmin[2];
                        cwmax[3] = cwmax[2];
                        aifs[3] = aifs[2];
                        txop[3] = txop[2];
                    }
                    cwmin[4] = 3;
                    cwmax[4] = 7;
                    aifs[4] = 28;

                    if ((cwmin[2]+aifs[2]) > ((cwmin[0]+aifs[0])+1))
                    {
                        wd->sta.ac0PriorityHigherThanAc2 = 1;
                    }
                    else
                    {
                        wd->sta.ac0PriorityHigherThanAc2 = 0;
                    }
                    zfHpUpdateQosParameter(dev, cwmin, cwmax, aifs, txop);
                }
            }
        }
    } //if (wd->sta.wmeConnected != 0)
}
/* process 802.11h Dynamic Frequency Selection */
void zfStaUpdateDot11HDFS(zdev_t* dev, zbuf_t* buf)
{
    //u8_t    length, channel, is5G;
    u16_t   offset;

    zmw_get_wlan_dev(dev);

    /*
    Channel Switch Announcement Element Format
    +------+----------+------+-------------------+------------------+--------------------+
    |Format|Element ID|Length|Channel Switch Mode|New Channel Number|Channel Switch Count|
    +------+----------+------+-------------------+------------------+--------------------+
    |Bytes |   1      |  1   |	     1           |       1          |          1         |
    +------+----------+------+-------------------+------------------+--------------------+
    |Value |   37     |  3   |       0 or 1      |unsigned integer  |unsigned integer    |
    +------+----------+------+-------------------+------------------+--------------------+
    */

    /* get EID(Channel Switch Announcement) */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_CHANNEL_SWITCH_ANNOUNCE)) == 0xffff )
    {
        //zm_debug_msg0("EID(Channel Switch Announcement) not found");
        return;
    }
    else if ( zmw_rx_buf_readb(dev, buf, offset+1) == 0x3 )
    {
        zm_debug_msg0("EID(Channel Switch Announcement) found");

        //length = zmw_rx_buf_readb(dev, buf, offset+1);
        //zfCopyFromRxBuffer(dev, buf, pBssInfo->supportedRates, offset, length+2);

        //Chanell Switch Mode set to 1, driver should disable transmit immediate
        //we do this by poll CCA high
        if (zmw_rx_buf_readb(dev, buf, offset+2) == 0x1 )
        {
        	//use ZM_OID_INTERNAL_WRITE,ZM_CMD_RESET to notice firmware flush quene and stop dma,
        	//then restart rx dma but not tx dma
        	if (wd->sta.DFSDisableTx != TRUE)
        	{
                /* TODO : zfHpResetTxRx would cause Rx hang */
                //zfHpResetTxRx(dev);
                wd->sta.DFSDisableTx = TRUE;
                /* Trgger Rx DMA */
                zfHpStartRecv(dev);
            }
        	//Adapter->ZD80211HSetting.DisableTxBy80211H=TRUE;
        	//AcquireCtrOfPhyReg(Adapter);
        	//ZD1205_WRITE_REGISTER(Adapter,CR24, 0x0);
        	//ReleaseDoNotSleep(Adapter);
        }

        if (zmw_rx_buf_readb(dev, buf, offset+4) <= 0x2 )
        {
        	//Channel Switch
        	//if Channel Switch Count = 0 , STA should change channel immediately.
        	//if Channel Switch Count > 0 , STA should change channel after TBTT*count
        	//But it won't be accurate to let driver calculate TBTT*count, and the value of
        	//Channel Switch Count will decrease by one each when continue receving beacon
        	//So we change channel here when we receive count <=2.

            zfHpDeleteAllowChannel(dev, wd->sta.currentFrequency);
        	wd->frequency = zfChNumToFreq(dev, zmw_rx_buf_readb(dev, buf, offset+3), 0);
        	//zfHpAddAllowChannel(dev, wd->frequency);
        	zm_debug_msg1("CWY - jump to frequency = ", wd->frequency);
        	zfCoreSetFrequency(dev, wd->frequency);
        	wd->sta.DFSDisableTx = FALSE;
            /* Increase rxBeaconCount to prevent beacon lost */
            if (zfStaIsConnected(dev))
            {
                wd->sta.rxBeaconCount = 1 << 6; // 2 times of check would pass
            }
        	//start tx dma to transmit packet

        	//if (zmw_rx_buf_readb(dev, buf, offset+3) != wd->frequency)
        	//{
        	//	//ZDDbgPrint(("Radar Detect by AP\n"));
        	//	zfCoreSetFrequency();
        	//	ProcessRadarDetectEvent(Adapter);
        	//	Set_RF_Channel(Adapter, SwRfd->Rfd->RxBuffer[index+3], (UCHAR)Adapter->RF_Mode, 1);
        	//	Adapter->CardSetting.Channel = SwRfd->Rfd->RxBuffer[index+3];
        	//	Adapter->SaveChannel = Adapter->CardSetting.Channel;
        	//	Adapter->UtilityChannel = Adapter->CardSetting.Channel;
        	//}
        }
    }

}
/* TODO : process 802.11h Transmission Power Control */
void zfStaUpdateDot11HTPC(zdev_t* dev, zbuf_t* buf)
{
}

/* IBSS power-saving mode */
void zfStaIbssPSCheckState(zdev_t* dev, zbuf_t* buf)
{
    u8_t   i, frameCtrl;

    zmw_get_wlan_dev(dev);

    if ( !zfStaIsConnected(dev) )
    {
        return;
    }

    if ( wd->wlanMode != ZM_MODE_IBSS )
    {
        return ;
    }

    /* check BSSID */
    if ( !zfRxBufferEqualToStr(dev, buf, (u8_t*) wd->sta.bssid,
                               ZM_WLAN_HEADER_A3_OFFSET, 6) )
    {
        return;
    }

    frameCtrl = zmw_rx_buf_readb(dev, buf, 1);

    /* check power management bit */
    if ( frameCtrl & ZM_BIT_4 )
    {
        for(i=1; i<ZM_MAX_PS_STA; i++)
        {
            if ( !wd->sta.staPSList.entity[i].bUsed )
            {
                continue;
            }

            /* check source address */
            if ( zfRxBufferEqualToStr(dev, buf,
                                      wd->sta.staPSList.entity[i].macAddr,
                                      ZM_WLAN_HEADER_A2_OFFSET, 6) )
            {
                return;
            }
        }

        for(i=1; i<ZM_MAX_PS_STA; i++)
        {
            if ( !wd->sta.staPSList.entity[i].bUsed )
            {
                wd->sta.staPSList.entity[i].bUsed = TRUE;
                wd->sta.staPSList.entity[i].bDataQueued = FALSE;
                break;
            }
        }

        if ( i == ZM_MAX_PS_STA )
        {
            /* STA list is full */
            return;
        }

        zfCopyFromRxBuffer(dev, buf, wd->sta.staPSList.entity[i].macAddr,
                           ZM_WLAN_HEADER_A2_OFFSET, 6);

        if ( wd->sta.staPSList.count == 0 )
        {
            // enable ATIM window
            //zfEnableAtimWindow(dev);
        }

        wd->sta.staPSList.count++;
    }
    else if ( wd->sta.staPSList.count )
    {
        for(i=1; i<ZM_MAX_PS_STA; i++)
        {
            if ( wd->sta.staPSList.entity[i].bUsed )
            {
                if ( zfRxBufferEqualToStr(dev, buf,
                                          wd->sta.staPSList.entity[i].macAddr,
                                          ZM_WLAN_HEADER_A2_OFFSET, 6) )
                {
                    wd->sta.staPSList.entity[i].bUsed = FALSE;
                    wd->sta.staPSList.count--;

                    if ( wd->sta.staPSList.entity[i].bDataQueued )
                    {
                        /* send queued data */
                    }
                }
            }
        }

        if ( wd->sta.staPSList.count == 0 )
        {
            /* disable ATIM window */
            //zfDisableAtimWindow(dev);
        }

    }
}

/* IBSS power-saving mode */
u8_t zfStaIbssPSQueueData(zdev_t* dev, zbuf_t* buf)
{
    u8_t   i;
    u16_t  da[3];

    zmw_get_wlan_dev(dev);

    if ( !zfStaIsConnected(dev) )
    {
        return 0;
    }

    if ( wd->wlanMode != ZM_MODE_IBSS )
    {
        return 0;
    }

    if ( wd->sta.staPSList.count == 0 && wd->sta.powerSaveMode <= ZM_STA_PS_NONE )
    {
        return 0;
    }

    /* DA */
#ifdef ZM_ENABLE_NATIVE_WIFI
    da[0] = zmw_tx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET);
    da[1] = zmw_tx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET + 2);
    da[2] = zmw_tx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET + 4);
#else
    da[0] = zmw_tx_buf_readh(dev, buf, 0);
    da[1] = zmw_tx_buf_readh(dev, buf, 2);
    da[2] = zmw_tx_buf_readh(dev, buf, 4);
#endif

    if ( ZM_IS_MULTICAST_OR_BROADCAST(da) )
    {
        wd->sta.staPSList.entity[0].bDataQueued = TRUE;
        wd->sta.ibssPSDataQueue[wd->sta.ibssPSDataCount++] = buf;
        return 1;
    }

    // Unicast packet...

    for(i=1; i<ZM_MAX_PS_STA; i++)
    {
        if ( zfMemoryIsEqual(wd->sta.staPSList.entity[i].macAddr,
                             (u8_t*) da, 6) )
        {
            wd->sta.staPSList.entity[i].bDataQueued = TRUE;
            wd->sta.ibssPSDataQueue[wd->sta.ibssPSDataCount++] = buf;

            return 1;
        }
    }

#if 0
    if ( wd->sta.powerSaveMode > ZM_STA_PS_NONE )
    {
        wd->sta.staPSDataQueue[wd->sta.staPSDataCount++] = buf;

        return 1;
    }
#endif

    return 0;
}

/* IBSS power-saving mode */
void zfStaIbssPSSend(zdev_t* dev)
{
    u8_t   i;
    u16_t  bcastAddr[3] = {0xffff, 0xffff, 0xffff};

    zmw_get_wlan_dev(dev);

    if ( !zfStaIsConnected(dev) )
    {
        return ;
    }

    if ( wd->wlanMode != ZM_MODE_IBSS )
    {
        return ;
    }

    for(i=0; i<ZM_MAX_PS_STA; i++)
    {
        if ( wd->sta.staPSList.entity[i].bDataQueued )
        {
            if ( i == 0 )
            {
                zfSendMmFrame(dev, ZM_WLAN_FRAME_TYPE_ATIM,
                              bcastAddr,
                              0, 0, 0);
            }
            else if ( wd->sta.staPSList.entity[i].bUsed )
            {
                // Send ATIM to prevent the peer to go to sleep
                zfSendMmFrame(dev, ZM_WLAN_FRAME_TYPE_ATIM,
                              (u16_t*) wd->sta.staPSList.entity[i].macAddr,
                              0, 0, 0);
            }

            wd->sta.staPSList.entity[i].bDataQueued = FALSE;
        }
    }

    for(i=0; i<wd->sta.ibssPSDataCount; i++)
    {
        zfTxSendEth(dev, wd->sta.ibssPSDataQueue[i], 0,
                    ZM_EXTERNAL_ALLOC_BUF, 0);
    }

    wd->sta.ibssPrevPSDataCount = wd->sta.ibssPSDataCount;
    wd->sta.ibssPSDataCount = 0;
}


void zfStaReconnect(zdev_t* dev)
{
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    if ( wd->wlanMode != ZM_MODE_INFRASTRUCTURE &&
         wd->wlanMode != ZM_MODE_IBSS )
    {
        return;
    }

    if ( (zfStaIsConnected(dev))||(zfStaIsConnecting(dev)) )
    {
        return;
    }

    if ( wd->sta.bChannelScan )
    {
        return;
    }

    /* Recover zero SSID length  */
    if ( (wd->wlanMode == ZM_MODE_INFRASTRUCTURE) && (wd->ws.ssidLen == 0))
    {
        zm_debug_msg0("zfStaReconnect: NOT Support!! Set SSID to any BSS");
        /* ANY BSS */
        zmw_enter_critical_section(dev);
        wd->sta.ssid[0] = 0;
        wd->sta.ssidLen = 0;
        zmw_leave_critical_section(dev);
    }

    // RAY: To ensure no TX pending before re-connecting
    zfFlushVtxq(dev);
    zfWlanEnable(dev);
    zfScanMgrScanAck(dev);
}

void zfStaTimer100ms(zdev_t* dev)
{
    zmw_get_wlan_dev(dev);

    if ( (wd->tick % 10) == 0 )
    {
        zfPushVtxq(dev);
//        zfPowerSavingMgrMain(dev);
    }
}


void zfStaCheckRxBeacon(zdev_t* dev)
{
    zmw_get_wlan_dev(dev);

    if (( wd->wlanMode == ZM_MODE_INFRASTRUCTURE ) && (zfStaIsConnected(dev)))
    {
        if (wd->beaconInterval == 0)
        {
            wd->beaconInterval = 100;
        }
        if ( (wd->tick % ((wd->beaconInterval * 10) / ZM_MS_PER_TICK)) == 0 )
        {
            /* Check rxBeaconCount */
            if (wd->sta.rxBeaconCount == 0)
            {
                if (wd->sta.beaconMissState == 1)
                {
            	/*notify AP that we left*/
            	zfSendMmFrame(dev, ZM_WLAN_FRAME_TYPE_DEAUTH, wd->sta.bssid, 3, 0, 0);
                /* Beacon Lost */
                zfStaConnectFail(dev, ZM_STATUS_MEDIA_DISCONNECT_BEACON_MISS,
                        wd->sta.bssid, 0);
                }
                else
                {
                    wd->sta.beaconMissState = 1;
                    /* Reset channel */
                    zfCoreSetFrequencyExV2(dev, wd->frequency, wd->BandWidth40,
                            wd->ExtOffset, NULL, 1);
                }
            }
            else
            {
                wd->sta.beaconMissState = 0;
            }
            wd->sta.rxBeaconCount = 0;
        }
    }
}



void zfStaCheckConnectTimeout(zdev_t* dev)
{
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    if ( wd->wlanMode != ZM_MODE_INFRASTRUCTURE )
    {
        return;
    }

    if ( !zfStaIsConnecting(dev) )
    {
        return;
    }

    zmw_enter_critical_section(dev);
    if ( (wd->sta.connectState == ZM_STA_CONN_STATE_AUTH_OPEN)||
         (wd->sta.connectState == ZM_STA_CONN_STATE_AUTH_SHARE_1)||
         (wd->sta.connectState == ZM_STA_CONN_STATE_AUTH_SHARE_2)||
         (wd->sta.connectState == ZM_STA_CONN_STATE_ASSOCIATE) )
    {
        if ( (wd->tick - wd->sta.connectTimer) > ZM_INTERVAL_CONNECT_TIMEOUT )
        {
            if ( wd->sta.connectByReasso )
            {
                wd->sta.failCntOfReasso++;
                if ( wd->sta.failCntOfReasso > 2 )
                {
                    wd->sta.connectByReasso = FALSE;
                }
            }

            wd->sta.connectState = ZM_STA_CONN_STATE_NONE;
            zm_debug_msg1("connect timeout, state = ", wd->sta.connectState);
            //zfiWlanDisable(dev);
            goto failed;
        }
    }

    zmw_leave_critical_section(dev);
    return;

failed:
    zmw_leave_critical_section(dev);
    if(wd->sta.authMode == ZM_AUTH_MODE_AUTO)
	{ // Fix some AP not send authentication failed message to sta and lead to connect timeout !
            wd->sta.connectTimeoutCount++;
	}
    zfStaConnectFail(dev, ZM_STATUS_MEDIA_DISCONNECT_TIMEOUT, wd->sta.bssid, 2);
    return;
}

void zfMmStaTimeTick(zdev_t* dev)
{
    zmw_get_wlan_dev(dev);

    /* airopeek */
    if (wd->wlanMode != ZM_MODE_AP && !wd->swSniffer)
    {
        if ( wd->tick & 1 )
        {
            zfTimerCheckAndHandle(dev);
        }

        zfStaCheckRxBeacon(dev);
        zfStaTimer100ms(dev);
        zfStaCheckConnectTimeout(dev);
        zfPowerSavingMgrMain(dev);
    }

#ifdef ZM_ENABLE_AGGREGATION
    /*
     * add by honda
     */
    zfAggScanAndClear(dev, wd->tick);
#endif
}

void zfStaSendBeacon(zdev_t* dev)
{
    zbuf_t* buf;
    u16_t offset, seq;

    zmw_get_wlan_dev(dev);

    zmw_declare_for_critical_section();

    //zm_debug_msg0("\n");

    /* TBD : Maximum size of beacon */
    if ((buf = zfwBufAllocate(dev, 1024)) == NULL)
    {
        zm_debug_msg0("Allocate beacon buffer failed");
        return;
    }

    offset = 0;
    /* wlan header */
    /* Frame control */
    zmw_tx_buf_writeh(dev, buf, offset, 0x0080);
    offset+=2;
    /* Duration */
    zmw_tx_buf_writeh(dev, buf, offset, 0x0000);
    offset+=2;
    /* Address 1 */
    zmw_tx_buf_writeh(dev, buf, offset, 0xffff);
    offset+=2;
    zmw_tx_buf_writeh(dev, buf, offset, 0xffff);
    offset+=2;
    zmw_tx_buf_writeh(dev, buf, offset, 0xffff);
    offset+=2;
    /* Address 2 */
    zmw_tx_buf_writeh(dev, buf, offset, wd->macAddr[0]);
    offset+=2;
    zmw_tx_buf_writeh(dev, buf, offset, wd->macAddr[1]);
    offset+=2;
    zmw_tx_buf_writeh(dev, buf, offset, wd->macAddr[2]);
    offset+=2;
    /* Address 3 */
    zmw_tx_buf_writeh(dev, buf, offset, wd->sta.bssid[0]);
    offset+=2;
    zmw_tx_buf_writeh(dev, buf, offset, wd->sta.bssid[1]);
    offset+=2;
    zmw_tx_buf_writeh(dev, buf, offset, wd->sta.bssid[2]);
    offset+=2;

    /* Sequence number */
    zmw_enter_critical_section(dev);
    seq = ((wd->mmseq++)<<4);
    zmw_leave_critical_section(dev);
    zmw_tx_buf_writeh(dev, buf, offset, seq);
    offset+=2;

    /* 24-31 Time Stamp : hardware will fill this field */
    offset+=8;

    /* Beacon Interval */
    zmw_tx_buf_writeh(dev, buf, offset, wd->beaconInterval);
    offset+=2;

    /* Capability */
    zmw_tx_buf_writeb(dev, buf, offset++, wd->sta.capability[0]);
    zmw_tx_buf_writeb(dev, buf, offset++, wd->sta.capability[1]);

    /* SSID */
    offset = zfStaAddIeSsid(dev, buf, offset);

    if(wd->frequency <= ZM_CH_G_14)  // 2.4 GHz  b+g
    {

    	/* Support Rate */
    	offset = zfMmAddIeSupportRate(dev, buf, offset,
                                  		ZM_WLAN_EID_SUPPORT_RATE, ZM_RATE_SET_CCK);

    	/* DS parameter set */
    	offset = zfMmAddIeDs(dev, buf, offset);

    	offset = zfStaAddIeIbss(dev, buf, offset);

        if( wd->wfc.bIbssGMode
            && (wd->supportMode & (ZM_WIRELESS_MODE_24_54|ZM_WIRELESS_MODE_24_N)) )    // Only accompany with enabling a mode .
        {
      	    /* ERP Information */
       	    wd->erpElement = 0;
       	    offset = zfMmAddIeErp(dev, buf, offset);
       	}

       	/* TODO : country information */
        /* RSN */
        if ( wd->sta.authMode == ZM_AUTH_MODE_WPA2PSK )
        {
            offset = zfwStaAddIeWpaRsn(dev, buf, offset, ZM_WLAN_FRAME_TYPE_AUTH);
        }

        if( wd->wfc.bIbssGMode
            && (wd->supportMode & (ZM_WIRELESS_MODE_24_54|ZM_WIRELESS_MODE_24_N)) )    // Only accompany with enabling a mode .
        {
            /* Enable G Mode */
            /* Extended Supported Rates */
       	    offset = zfMmAddIeSupportRate(dev, buf, offset,
                                   		    ZM_WLAN_EID_EXTENDED_RATE, ZM_RATE_SET_OFDM);
	    }
    }
    else    // 5GHz a
    {
        /* Support Rate a Mode */
    	offset = zfMmAddIeSupportRate(dev, buf, offset,
        	                            ZM_WLAN_EID_SUPPORT_RATE, ZM_RATE_SET_OFDM);

        /* DS parameter set */
    	offset = zfMmAddIeDs(dev, buf, offset);

    	offset = zfStaAddIeIbss(dev, buf, offset);

        /* TODO : country information */
        /* RSN */
        if ( wd->sta.authMode == ZM_AUTH_MODE_WPA2PSK )
        {
            offset = zfwStaAddIeWpaRsn(dev, buf, offset, ZM_WLAN_FRAME_TYPE_AUTH);
        }
    }

    if ( wd->wlanMode != ZM_MODE_IBSS )
    {
        /* TODO : Need to check if it is ok */
        /* HT Capabilities Info */
        offset = zfMmAddHTCapability(dev, buf, offset);

        /* Extended HT Capabilities Info */
        offset = zfMmAddExtendedHTCapability(dev, buf, offset);
    }

    if ( wd->sta.ibssAdditionalIESize )
        offset = zfStaAddIbssAdditionalIE(dev, buf, offset);

    /* 1212 : write to beacon fifo */
    /* 1221 : write to share memory */
    zfHpSendBeacon(dev, buf, offset);

    /* Free beacon buffer */
    //zfwBufFree(dev, buf, 0);
}

void zfStaSignalStatistic(zdev_t* dev, u8_t SignalStrength, u8_t SignalQuality) //CWYang(+)
{
    zmw_get_wlan_dev(dev);

    /* Add Your Code to Do Works Like Moving Average Here */
    wd->SignalStrength = (wd->SignalStrength * 7 + SignalStrength * 3)/10;
    wd->SignalQuality = (wd->SignalQuality * 7 + SignalQuality * 3)/10;

}

struct zsBssInfo* zfStaFindBssInfo(zdev_t* dev, zbuf_t* buf, struct zsWlanProbeRspFrameHeader *pProbeRspHeader)
{
    u8_t    i;
    u8_t    j;
    u8_t    k;
    u8_t    isMatched, length, channel;
    u16_t   offset, frequency;
    struct zsBssInfo* pBssInfo;

    zmw_get_wlan_dev(dev);

    if ((pBssInfo = wd->sta.bssList.head) == NULL)
    {
        return NULL;
    }

    for( i=0; i<wd->sta.bssList.bssCount; i++ )
    {
        //zm_debug_msg2("check pBssInfo = ", pBssInfo);

        /* Check BSSID */
        for( j=0; j<6; j++ )
        {
            if ( pBssInfo->bssid[j] != pProbeRspHeader->bssid[j] )
            {
                break;
            }
        }

		/* Check SSID */
        if (j == 6)
        {
            if (pProbeRspHeader->ssid[1] <= 32)
            {
                /* compare length and ssid */
                isMatched = 1;
				if((pProbeRspHeader->ssid[1] != 0) && (pBssInfo->ssid[1] != 0))
				{
                for( k=1; k<pProbeRspHeader->ssid[1] + 1; k++ )
                {
                    if ( pBssInfo->ssid[k] != pProbeRspHeader->ssid[k] )
                    {
                        isMatched = 0;
                        break;
                    }
                }
            }
            }
            else
            {
                isMatched = 0;
            }
        }
        else
        {
            isMatched = 0;
        }

        /* Check channel */
        /* Add check channel to solve the bug #31222 */
        if (isMatched) {
            if ((offset = zfFindElement(dev, buf, ZM_WLAN_EID_DS)) != 0xffff) {
                if ((length = zmw_rx_buf_readb(dev, buf, offset+1)) == 1) {
                    channel = zmw_rx_buf_readb(dev, buf, offset+2);
                    if (zfHpIsAllowedChannel(dev, zfChNumToFreq(dev, channel, 0)) == 0) {
                        frequency = 0;
                    } else {
                        frequency = zfChNumToFreq(dev, channel, 0);;
                    }
                } else {
                    frequency = 0;
                }
            } else {
                frequency = wd->sta.currentFrequency;
            }

            if (frequency != 0) {
                if ( ((frequency > 3000) && (pBssInfo->frequency > 3000))
                     || ((frequency < 3000) && (pBssInfo->frequency < 3000)) ) {
                    /* redundant */
                    break;
                }
            }
        }

        pBssInfo = pBssInfo->next;
    }

    if ( i == wd->sta.bssList.bssCount )
    {
        pBssInfo = NULL;
    }

    return pBssInfo;
}

u8_t zfStaInitBssInfo(zdev_t* dev, zbuf_t* buf,
        struct zsWlanProbeRspFrameHeader *pProbeRspHeader,
        struct zsBssInfo* pBssInfo, struct zsAdditionInfo* AddInfo, u8_t type)
{
    u8_t    length, channel, is5G;
    u16_t   i, offset;
    u8_t    apQosInfo;
    u16_t    eachIElength = 0;
    u16_t   accumulateLen = 0;

    zmw_get_wlan_dev(dev);

    if ((type == 1) && ((pBssInfo->flag & ZM_BSS_INFO_VALID_BIT) != 0))
    {
        goto zlUpdateRssi;
    }

    /* get SSID */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_SSID)) == 0xffff )
    {
        zm_debug_msg0("EID(SSID) not found");
        goto zlError;
    }

    length = zmw_rx_buf_readb(dev, buf, offset+1);

	{
		u8_t Show_Flag = 0;
		zfwGetShowZeroLengthSSID(dev, &Show_Flag);

		if(Show_Flag)
		{
			if (length > ZM_MAX_SSID_LENGTH )
			{
				zm_debug_msg0("EID(SSID) is invalid");
				goto zlError;
			}
		}
		else
		{
    if ( length == 0 || length > ZM_MAX_SSID_LENGTH )
    {
        zm_debug_msg0("EID(SSID) is invalid");
        goto zlError;
    }

		}
	}
    zfCopyFromRxBuffer(dev, buf, pBssInfo->ssid, offset, length+2);

    /* get DS parameter */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_DS)) != 0xffff )
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
        if ( length != 1 )
        {
            zm_msg0_mm(ZM_LV_0, "Abnormal DS Param Set IE");
            goto zlError;
        }
        channel = zmw_rx_buf_readb(dev, buf, offset+2);

        if (zfHpIsAllowedChannel(dev, zfChNumToFreq(dev, channel, 0)) == 0)
        {
            goto zlError2;
        }

        pBssInfo->frequency = zfChNumToFreq(dev, channel, 0); // auto check
        pBssInfo->channel = channel;


    }
    else
    {
        /* DS parameter not found */
        pBssInfo->frequency = wd->sta.currentFrequency;
        pBssInfo->channel = zfChFreqToNum(wd->sta.currentFrequency, &is5G);
    }

    /* initialize security type */
    pBssInfo->securityType = ZM_SECURITY_TYPE_NONE;

    /* get macaddr */
    for( i=0; i<6; i++ )
    {
        pBssInfo->macaddr[i] = pProbeRspHeader->sa[i];
    }

    /* get bssid */
    for( i=0; i<6; i++ )
    {
        pBssInfo->bssid[i] = pProbeRspHeader->bssid[i];
    }

    /* get timestamp */
    for( i=0; i<8; i++ )
    {
        pBssInfo->timeStamp[i] = pProbeRspHeader->timeStamp[i];
    }

    /* get beacon interval */
    pBssInfo->beaconInterval[0] = pProbeRspHeader->beaconInterval[0];
    pBssInfo->beaconInterval[1] = pProbeRspHeader->beaconInterval[1];

    /* get capability */
    pBssInfo->capability[0] = pProbeRspHeader->capability[0];
    pBssInfo->capability[1] = pProbeRspHeader->capability[1];

    /* Copy frame body */
    offset = 36;            // Copy from the start of variable IE
    pBssInfo->frameBodysize = zfwBufGetSize(dev, buf)-offset;
    if (pBssInfo->frameBodysize > (ZM_MAX_PROBE_FRAME_BODY_SIZE-1))
    {
        pBssInfo->frameBodysize = ZM_MAX_PROBE_FRAME_BODY_SIZE-1;
    }
    accumulateLen = 0;
    do
    {
        eachIElength = zmw_rx_buf_readb(dev, buf, offset + accumulateLen+1) + 2;  //Len+(EID+Data)

        if ( (eachIElength >= 2)
             && ((accumulateLen + eachIElength) <= pBssInfo->frameBodysize) )
        {
            zfCopyFromRxBuffer(dev, buf, pBssInfo->frameBody+accumulateLen, offset+accumulateLen, eachIElength);
            accumulateLen+=(u16_t)eachIElength;
        }
        else
        {
            zm_msg0_mm(ZM_LV_1, "probersp frameBodysize abnormal");
            break;
        }
    }
    while(accumulateLen < pBssInfo->frameBodysize);
    pBssInfo->frameBodysize = accumulateLen;

    /* get supported rates */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_SUPPORT_RATE)) == 0xffff )
    {
        zm_debug_msg0("EID(supported rates) not found");
        goto zlError;
    }

    length = zmw_rx_buf_readb(dev, buf, offset+1);
    if ( length == 0 || length > ZM_MAX_SUPP_RATES_IE_SIZE)
    {
        zm_msg0_mm(ZM_LV_0, "Supported rates IE length abnormal");
        goto zlError;
    }
    zfCopyFromRxBuffer(dev, buf, pBssInfo->supportedRates, offset, length+2);



    /* get Country information */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_COUNTRY)) != 0xffff )
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_COUNTRY_INFO_SIZE)
        {
            length = ZM_MAX_COUNTRY_INFO_SIZE;
        }
        zfCopyFromRxBuffer(dev, buf, pBssInfo->countryInfo, offset, length+2);
        /* check 802.11d support data */
        if (wd->sta.b802_11D)
        {
            zfHpGetRegulationTablefromISO(dev, (u8_t *)&pBssInfo->countryInfo, 3);
            /* only set regulatory one time */
            wd->sta.b802_11D = 0;
        }
    }

    /* get ERP information */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_ERP)) != 0xffff )
    {
        pBssInfo->erp = zmw_rx_buf_readb(dev, buf, offset+2);
    }

    /* get extended supported rates */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_EXTENDED_RATE)) != 0xffff )
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_SUPP_RATES_IE_SIZE)
        {
            zm_msg0_mm(ZM_LV_0, "Extended rates IE length abnormal");
            goto zlError;
        }
        zfCopyFromRxBuffer(dev, buf, pBssInfo->extSupportedRates, offset, length+2);
    }
    else
    {
        pBssInfo->extSupportedRates[0] = 0;
        pBssInfo->extSupportedRates[1] = 0;
    }

    /* get WPA IE */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_WPA_IE)) != 0xffff )
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_IE_SIZE)
        {
            length = ZM_MAX_IE_SIZE;
        }
        zfCopyFromRxBuffer(dev, buf, pBssInfo->wpaIe, offset, length+2);
        pBssInfo->securityType = ZM_SECURITY_TYPE_WPA;
    }
    else
    {
        pBssInfo->wpaIe[1] = 0;
    }

    /* get WPS IE */
    if ((offset = zfFindWifiElement(dev, buf, 4, 0xff)) != 0xffff)
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_WPS_IE_SIZE )
        {
            length = ZM_MAX_WPS_IE_SIZE;
        }
        zfCopyFromRxBuffer(dev, buf, pBssInfo->wscIe, offset, length+2);
    }
    else
    {
        pBssInfo->wscIe[1] = 0;
    }

    /* get SuperG IE */
    if ((offset = zfFindSuperGElement(dev, buf, ZM_WLAN_EID_VENDOR_PRIVATE)) != 0xffff)
    {
        pBssInfo->apCap |= ZM_SuperG_AP;
    }

    /* get XR IE */
    if ((offset = zfFindXRElement(dev, buf, ZM_WLAN_EID_VENDOR_PRIVATE)) != 0xffff)
    {
        pBssInfo->apCap |= ZM_XR_AP;
    }

    /* get RSN IE */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_RSN_IE)) != 0xffff )
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_IE_SIZE)
        {
            length = ZM_MAX_IE_SIZE;
        }
        zfCopyFromRxBuffer(dev, buf, pBssInfo->rsnIe, offset, length+2);
        pBssInfo->securityType = ZM_SECURITY_TYPE_WPA;
    }
    else
    {
        pBssInfo->rsnIe[1] = 0;
    }
#ifdef ZM_ENABLE_CENC
    /* get CENC IE */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_CENC_IE)) != 0xffff )
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_IE_SIZE )
        {
            length = ZM_MAX_IE_SIZE;
        }
        zfCopyFromRxBuffer(dev, buf, pBssInfo->cencIe, offset, length+2);
        pBssInfo->securityType = ZM_SECURITY_TYPE_CENC;
        pBssInfo->capability[0] &= 0xffef;
    }
    else
    {
        pBssInfo->cencIe[1] = 0;
    }
#endif //ZM_ENABLE_CENC
    /* get WME Parameter IE, probe rsp may contain WME parameter element */
    //if ( wd->bQoSEnable )
    {
        if ((offset = zfFindWifiElement(dev, buf, 2, 1)) != 0xffff)
        {
            apQosInfo = zmw_rx_buf_readb(dev, buf, offset+8) & 0x80;
            pBssInfo->wmeSupport = 1 | apQosInfo;
        }
        else if ((offset = zfFindWifiElement(dev, buf, 2, 0)) != 0xffff)
        {
            apQosInfo = zmw_rx_buf_readb(dev, buf, offset+8) & 0x80;
            pBssInfo->wmeSupport = 1  | apQosInfo;
        }
        else
        {
            pBssInfo->wmeSupport = 0;
        }
    }
    //CWYang(+)
    if ((offset = zfFindElement(dev, buf, ZM_WLAN_EID_HT_CAPABILITY)) != 0xffff)
    {
        /* 11n AP */
        pBssInfo->EnableHT = 1;
        if (zmw_rx_buf_readb(dev, buf, offset+1) & 0x02)
        {
            pBssInfo->enableHT40 = 1;
        }
        else
        {
            pBssInfo->enableHT40 = 0;
        }

        if (zmw_rx_buf_readb(dev, buf, offset+1) & 0x40)
        {
            pBssInfo->SG40 = 1;
        }
        else
        {
            pBssInfo->SG40 = 0;
        }
    }
    else if ((offset = zfFindElement(dev, buf, ZM_WLAN_PREN2_EID_HTCAPABILITY)) != 0xffff)
    {
        /* 11n AP */
        pBssInfo->EnableHT = 1;
        pBssInfo->apCap |= ZM_All11N_AP;
        if (zmw_rx_buf_readb(dev, buf, offset+2) & 0x02)
        {
            pBssInfo->enableHT40 = 1;
        }
        else
        {
            pBssInfo->enableHT40 = 0;
        }

        if (zmw_rx_buf_readb(dev, buf, offset+2) & 0x40)
        {
            pBssInfo->SG40 = 1;
        }
        else
        {
            pBssInfo->SG40 = 0;
        }
    }
    else
    {
        pBssInfo->EnableHT = 0;
    }
    /* HT information */
    if ((offset = zfFindElement(dev, buf, ZM_WLAN_EID_EXTENDED_HT_CAPABILITY)) != 0xffff)
    {
        /* atheros pre n */
        pBssInfo->extChOffset = zmw_rx_buf_readb(dev, buf, offset+2) & 0x03;
    }
    else if ((offset = zfFindElement(dev, buf, ZM_WLAN_PREN2_EID_HTINFORMATION)) != 0xffff)
    {
        /* pre n 2.0 standard */
        pBssInfo->extChOffset = zmw_rx_buf_readb(dev, buf, offset+3) & 0x03;
    }
    else
    {
        pBssInfo->extChOffset = 0;
    }

    if ( (pBssInfo->enableHT40 == 1)
         && ((pBssInfo->extChOffset != 1) && (pBssInfo->extChOffset != 3)) )
    {
        pBssInfo->enableHT40 = 0;
    }

    if (pBssInfo->enableHT40 == 1)
    {
        if (zfHpIsAllowedChannel(dev, pBssInfo->frequency+((pBssInfo->extChOffset==1)?20:-20)) == 0)
        {
            /* if extension channel is not an allowed channel, treat AP as non-HT mode */
            pBssInfo->EnableHT = 0;
            pBssInfo->enableHT40 = 0;
            pBssInfo->extChOffset = 0;
        }
    }

    /* get ATH Extended Capability */
    if ( ((offset = zfFindElement(dev, buf, ZM_WLAN_EID_EXTENDED_HT_CAPABILITY)) != 0xffff)&&
        ((offset = zfFindBrdcmMrvlRlnkExtCap(dev, buf)) == 0xffff))

    {
        pBssInfo->athOwlAp = 1;
    }
    else
    {
        pBssInfo->athOwlAp = 0;
    }

    /* get Broadcom Extended Capability */
    if ( (pBssInfo->EnableHT == 1) //((offset = zfFindElement(dev, buf, ZM_WLAN_EID_EXTENDED_HT_CAPABILITY)) != 0xffff)
         && ((offset = zfFindBroadcomExtCap(dev, buf)) != 0xffff) )
    {
        pBssInfo->broadcomHTAp = 1;
    }
    else
    {
        pBssInfo->broadcomHTAp = 0;
    }

    /* get Marvel Extended Capability */
    if ((offset = zfFindMarvelExtCap(dev, buf)) != 0xffff)
    {
        pBssInfo->marvelAp = 1;
    }
    else
    {
        pBssInfo->marvelAp = 0;
    }

    /* get ATIM window */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_IBSS)) != 0xffff )
    {
        pBssInfo->atimWindow = zmw_rx_buf_readh(dev, buf,offset+2);
    }

    /* Fit for support mode */
    if (pBssInfo->frequency > 3000) {
        if (wd->supportMode & ZM_WIRELESS_MODE_5_N) {
#if 0
            if (wd->supportMode & ZM_WIRELESS_MODE_5_54) {
                /* support mode: a, n */
                /* do nothing */
            } else {
                /* support mode: n */
                /* reject non-n bss info */
                if (!pBssInfo->EnableHT) {
                    goto zlError2;
                }
            }
#endif
        } else {
            if (wd->supportMode & ZM_WIRELESS_MODE_5_54) {
                /* support mode: a */
                /* delete n mode information */
                pBssInfo->EnableHT = 0;
                pBssInfo->enableHT40 = 0;
                pBssInfo->apCap &= (~ZM_All11N_AP);
                pBssInfo->extChOffset = 0;
                pBssInfo->frameBodysize = zfRemoveElement(dev, pBssInfo->frameBody,
                            pBssInfo->frameBodysize, ZM_WLAN_EID_HT_CAPABILITY);
                pBssInfo->frameBodysize = zfRemoveElement(dev, pBssInfo->frameBody,
                            pBssInfo->frameBodysize, ZM_WLAN_PREN2_EID_HTCAPABILITY);
                pBssInfo->frameBodysize = zfRemoveElement(dev, pBssInfo->frameBody,
                            pBssInfo->frameBodysize, ZM_WLAN_EID_EXTENDED_HT_CAPABILITY);
                pBssInfo->frameBodysize = zfRemoveElement(dev, pBssInfo->frameBody,
                            pBssInfo->frameBodysize, ZM_WLAN_PREN2_EID_HTINFORMATION);
            } else {
                /* support mode: none */
                goto zlError2;
            }
        }
    } else {
        if (wd->supportMode & ZM_WIRELESS_MODE_24_N) {
#if 0
            if (wd->supportMode & ZM_WIRELESS_MODE_24_54) {
                if (wd->supportMode & ZM_WIRELESS_MODE_24_11) {
                    /* support mode: b, g, n */
                    /* do nothing */
                } else {
                    /* support mode: g, n */
                    /* reject b-only bss info */
                    if ( (!pBssInfo->EnableHT)
                         && (pBssInfo->extSupportedRates[1] == 0) ) {
                         goto zlError2;
                    }
                }
            } else {
                if (wd->supportMode & ZM_WIRELESS_MODE_24_11) {
                    /* support mode: b, n */
                    /* 1. reject g-only bss info
                     * 2. if non g-only, delete g mode information
                     */
                    if ( !pBssInfo->EnableHT ) {
                        if ( zfIsGOnlyMode(dev, pBssInfo->frequency, pBssInfo->supportedRates)
                             || zfIsGOnlyMode(dev, pBssInfo->frequency, pBssInfo->extSupportedRates) ) {
                            goto zlError2;
                        } else {
                            zfGatherBMode(dev, pBssInfo->supportedRates,
                                          pBssInfo->extSupportedRates);
                            pBssInfo->erp = 0;

                            pBssInfo->frameBodysize = zfRemoveElement(dev,
                                pBssInfo->frameBody, pBssInfo->frameBodysize,
                                ZM_WLAN_EID_ERP);
                            pBssInfo->frameBodysize = zfRemoveElement(dev,
                                pBssInfo->frameBody, pBssInfo->frameBodysize,
                                ZM_WLAN_EID_EXTENDED_RATE);

                            pBssInfo->frameBodysize = zfUpdateElement(dev,
                                pBssInfo->frameBody, pBssInfo->frameBodysize,
                                pBssInfo->supportedRates);
                        }
                    }
                } else {
                    /* support mode: n */
                    /* reject non-n bss info */
                    if (!pBssInfo->EnableHT) {
                        goto zlError2;
                    }
                }
            }
#endif
        } else {
            /* delete n mode information */
            pBssInfo->EnableHT = 0;
            pBssInfo->enableHT40 = 0;
            pBssInfo->apCap &= (~ZM_All11N_AP);
            pBssInfo->extChOffset = 0;
            pBssInfo->frameBodysize = zfRemoveElement(dev, pBssInfo->frameBody,
                        pBssInfo->frameBodysize, ZM_WLAN_EID_HT_CAPABILITY);
            pBssInfo->frameBodysize = zfRemoveElement(dev, pBssInfo->frameBody,
                        pBssInfo->frameBodysize, ZM_WLAN_PREN2_EID_HTCAPABILITY);
            pBssInfo->frameBodysize = zfRemoveElement(dev, pBssInfo->frameBody,
                        pBssInfo->frameBodysize, ZM_WLAN_EID_EXTENDED_HT_CAPABILITY);
            pBssInfo->frameBodysize = zfRemoveElement(dev, pBssInfo->frameBody,
                        pBssInfo->frameBodysize, ZM_WLAN_PREN2_EID_HTINFORMATION);

            if (wd->supportMode & ZM_WIRELESS_MODE_24_54) {
#if 0
                if (wd->supportMode & ZM_WIRELESS_MODE_24_11) {
                    /* support mode: b, g */
                    /* delete n mode information */
                } else {
                    /* support mode: g */
                    /* delete n mode information */
                    /* reject b-only bss info */
                    if (pBssInfo->extSupportedRates[1] == 0) {
                         goto zlError2;
                    }
                }
#endif
            } else {
                if (wd->supportMode & ZM_WIRELESS_MODE_24_11) {
                    /* support mode: b */
                    /* delete n mode information */
                    if ( zfIsGOnlyMode(dev, pBssInfo->frequency, pBssInfo->supportedRates)
                         || zfIsGOnlyMode(dev, pBssInfo->frequency, pBssInfo->extSupportedRates) ) {
                        goto zlError2;
                    } else {
                        zfGatherBMode(dev, pBssInfo->supportedRates,
                                          pBssInfo->extSupportedRates);
                        pBssInfo->erp = 0;

                        pBssInfo->frameBodysize = zfRemoveElement(dev,
                            pBssInfo->frameBody, pBssInfo->frameBodysize,
                            ZM_WLAN_EID_ERP);
                        pBssInfo->frameBodysize = zfRemoveElement(dev,
                            pBssInfo->frameBody, pBssInfo->frameBodysize,
                            ZM_WLAN_EID_EXTENDED_RATE);

                        pBssInfo->frameBodysize = zfUpdateElement(dev,
                            pBssInfo->frameBody, pBssInfo->frameBodysize,
                            pBssInfo->supportedRates);
                    }
                } else {
                    /* support mode: none */
                    goto zlError2;
                }
            }
        }
    }

    pBssInfo->flag |= ZM_BSS_INFO_VALID_BIT;

zlUpdateRssi:
    /* Update Timer information */
    pBssInfo->tick = wd->tick;

    /* Update ERP information */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_ERP)) != 0xffff )
    {
        pBssInfo->erp = zmw_rx_buf_readb(dev, buf, offset+2);
    }

    if( (s8_t)pBssInfo->signalStrength < (s8_t)AddInfo->Tail.Data.SignalStrength1 )
    {
        /* Update signal strength */
        pBssInfo->signalStrength = (u8_t)AddInfo->Tail.Data.SignalStrength1;
        /* Update signal quality */
        pBssInfo->signalQuality = (u8_t)(AddInfo->Tail.Data.SignalStrength1 * 2);

        /* Update the sorting value  */
        pBssInfo->sortValue = zfComputeBssInfoWeightValue(dev,
                                               (pBssInfo->supportedRates[6] + pBssInfo->extSupportedRates[0]),
                                               pBssInfo->EnableHT,
                                               pBssInfo->enableHT40,
                                               pBssInfo->signalStrength);
    }

    return 0;

zlError:

    return 1;

zlError2:

    return 2;
}

void zfStaProcessBeacon(zdev_t* dev, zbuf_t* buf, struct zsAdditionInfo* AddInfo) //CWYang(m)
{
    /* Parse TIM and send PS-POLL in power saving mode */
    struct zsWlanBeaconFrameHeader*  pBeaconHeader;
    struct zsBssInfo* pBssInfo;
    u8_t   pBuf[sizeof(struct zsWlanBeaconFrameHeader)];
    u8_t   bssid[6];
    int    res;

    zmw_get_wlan_dev(dev);

    zmw_declare_for_critical_section();

    /* sta routine jobs */
    zfStaProtErpMonitor(dev, buf);  /* check protection mode */

    if (zfStaIsConnected(dev))
    {
        ZM_MAC_WORD_TO_BYTE(wd->sta.bssid, bssid);

        if ( wd->wlanMode == ZM_MODE_INFRASTRUCTURE )
        {
            if ( zfRxBufferEqualToStr(dev, buf, bssid, ZM_WLAN_HEADER_A2_OFFSET, 6) )
            {
                zfPowerSavingMgrProcessBeacon(dev, buf);
                zfStaUpdateWmeParameter(dev, buf);
                if (wd->sta.DFSEnable)
                    zfStaUpdateDot11HDFS(dev, buf);
                if (wd->sta.TPCEnable)
                    zfStaUpdateDot11HTPC(dev, buf);
                /* update signal strength and signal quality */
                zfStaSignalStatistic(dev, AddInfo->Tail.Data.SignalStrength1,
                        AddInfo->Tail.Data.SignalQuality); //CWYang(+)
                wd->sta.rxBeaconCount++;
            }
        }
        else if ( wd->wlanMode == ZM_MODE_IBSS )
        {
            if ( zfRxBufferEqualToStr(dev, buf, bssid, ZM_WLAN_HEADER_A3_OFFSET, 6) )
            {
                int res;
                struct zsPartnerNotifyEvent event;

                zm_debug_msg0("20070916 Receive opposite Beacon!");
                zmw_enter_critical_section(dev);
                wd->sta.ibssReceiveBeaconCount++;
                zmw_leave_critical_section(dev);

                res = zfStaSetOppositeInfoFromRxBuf(dev, buf);
                if ( res == 0 )
                {
                    // New peer station found. Notify the wrapper now
                    zfInitPartnerNotifyEvent(dev, buf, &event);
                    if (wd->zfcbIbssPartnerNotify != NULL)
                    {
                        wd->zfcbIbssPartnerNotify(dev, 1, &event);
                    }
                }
                /* update signal strength and signal quality */
                zfStaSignalStatistic(dev, AddInfo->Tail.Data.SignalStrength1,
                        AddInfo->Tail.Data.SignalQuality); //CWYang(+)
            }
            //else if ( wd->sta.ibssPartnerStatus == ZM_IBSS_PARTNER_LOST )
            // Why does this happen in IBSS?? The impact of Vista since
            // we need to tell it the BSSID
#if 0
            else if ( wd->sta.oppositeCount == 0 )
            {   /* IBSS merge if SSID matched */
                if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_SSID)) != 0xffff )
                {
                    if ( (wd->sta.ssidLen == zmw_buf_readb(dev, buf, offset+1))&&
                         (zfRxBufferEqualToStr(dev, buf, wd->sta.ssid,
                                               offset+2, wd->sta.ssidLen)) )
                    {
                        capabilityInfo = zmw_buf_readh(dev, buf, 34);

                        if ( capabilityInfo & ZM_BIT_1 )
                        {
                            if ( (wd->sta.capability[0] & ZM_BIT_4) ==
                                 (capabilityInfo & ZM_BIT_4) )
                            {
                                zm_debug_msg0("IBSS merge");
                                zfCopyFromRxBuffer(dev, buf, bssid,
                                                   ZM_WLAN_HEADER_A3_OFFSET, 6);
                                zfUpdateBssid(dev, bssid);
                            }
                        }
                    }
                }
            }
#endif
        }
    }

    /* return if not channel scan */
    if ( !wd->sta.bChannelScan )
    {
        goto zlReturn;
    }

    zfCopyFromRxBuffer(dev, buf, pBuf, 0, sizeof(struct zsWlanBeaconFrameHeader));
    pBeaconHeader = (struct zsWlanBeaconFrameHeader*) pBuf;

    zmw_enter_critical_section(dev);

    //zm_debug_msg1("bss count = ", wd->sta.bssList.bssCount);

    pBssInfo = zfStaFindBssInfo(dev, buf, pBeaconHeader);

    if ( pBssInfo == NULL )
    {
        /* Allocate a new entry if BSS not in the scan list */
        pBssInfo = zfBssInfoAllocate(dev);
        if (pBssInfo != NULL)
        {
            res = zfStaInitBssInfo(dev, buf, pBeaconHeader, pBssInfo, AddInfo, 0);
            //zfDumpSSID(pBssInfo->ssid[1], &(pBssInfo->ssid[2]));
            if ( res != 0 )
            {
                zfBssInfoFree(dev, pBssInfo);
            }
            else
            {
                zfBssInfoInsertToList(dev, pBssInfo);
            }
        }
    }
    else
    {
        res = zfStaInitBssInfo(dev, buf, pBeaconHeader, pBssInfo, AddInfo, 1);
        if (res == 2)
        {
            zfBssInfoRemoveFromList(dev, pBssInfo);
            zfBssInfoFree(dev, pBssInfo);
        }
        else if ( wd->wlanMode == ZM_MODE_IBSS )
        {
            int idx;

            // It would reset the alive counter if the peer station is found!
            zfStaFindFreeOpposite(dev, (u16_t *)pBssInfo->macaddr, &idx);
        }
    }

    zmw_leave_critical_section(dev);

zlReturn:

    return;
}


void zfAuthFreqCompleteCb(zdev_t* dev)
{
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    zmw_enter_critical_section(dev);

    if (wd->sta.connectState == ZM_STA_CONN_STATE_AUTH_COMPLETED)
    {
        zm_debug_msg0("ZM_STA_CONN_STATE_ASSOCIATE");
        wd->sta.connectTimer = wd->tick;
        wd->sta.connectState = ZM_STA_CONN_STATE_ASSOCIATE;
    }

    zmw_leave_critical_section(dev);
    return;
}

/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfProcessAuth               */
/*      Process authenticate management frame.                          */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      buf : auth frame buffer                                         */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      none                                                            */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen Chen        ZyDAS Technology Corporation    2005.10     */
/*                                                                      */
/************************************************************************/
/* Note : AP allows one authenticating STA at a time, does not          */
/*        support multiple authentication process. Make sure            */
/*        authentication state machine will not be blocked due          */
/*        to incompleted authentication handshake.                      */
void zfStaProcessAuth(zdev_t* dev, zbuf_t* buf, u16_t* src, u16_t apId)
{
    struct zsWlanAuthFrameHeader* pAuthFrame;
    u8_t  pBuf[sizeof(struct zsWlanAuthFrameHeader)];
    u32_t p1, p2;

    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    if ( !zfStaIsConnecting(dev) )
    {
        return;
    }

    pAuthFrame = (struct zsWlanAuthFrameHeader*) pBuf;
    zfCopyFromRxBuffer(dev, buf, pBuf, 0, sizeof(struct zsWlanAuthFrameHeader));

    if ( wd->sta.connectState == ZM_STA_CONN_STATE_AUTH_OPEN )
    {
        if ( (zmw_le16_to_cpu(pAuthFrame->seq) == 2)&&
             (zmw_le16_to_cpu(pAuthFrame->algo) == 0)&&
             (zmw_le16_to_cpu(pAuthFrame->status) == 0) )
        {

            zmw_enter_critical_section(dev);
            wd->sta.connectTimer = wd->tick;
            zm_debug_msg0("ZM_STA_CONN_STATE_AUTH_COMPLETED");
            wd->sta.connectState = ZM_STA_CONN_STATE_AUTH_COMPLETED;
            zmw_leave_critical_section(dev);

            //Set channel according to AP's configuration
            //Move to here because of Cisco 11n AP feature
            zfCoreSetFrequencyEx(dev, wd->frequency, wd->BandWidth40,
                    wd->ExtOffset, zfAuthFreqCompleteCb);

            /* send association frame */
            if ( wd->sta.connectByReasso )
            {
                zfSendMmFrame(dev, ZM_WLAN_FRAME_TYPE_REASOCREQ,
                              wd->sta.bssid, 0, 0, 0);
            }
            else
            {
                zfSendMmFrame(dev, ZM_WLAN_FRAME_TYPE_ASOCREQ,
                              wd->sta.bssid, 0, 0, 0);
            }


        }
        else
        {
            zm_debug_msg1("authentication failed, status = ",
                          pAuthFrame->status);

            if (wd->sta.authMode == ZM_AUTH_MODE_AUTO)
            {
                wd->sta.bIsSharedKey = 1;
                zfStaStartConnect(dev, wd->sta.bIsSharedKey);
            }
            else
            {
                zm_debug_msg0("ZM_STA_STATE_DISCONNECT");
                zfStaConnectFail(dev, ZM_STATUS_MEDIA_DISCONNECT_AUTH_FAILED, wd->sta.bssid, 3);
            }
        }
    }
    else if ( wd->sta.connectState == ZM_STA_CONN_STATE_AUTH_SHARE_1 )
    {
        if ( (zmw_le16_to_cpu(pAuthFrame->algo) == 1) &&
             (zmw_le16_to_cpu(pAuthFrame->seq) == 2) &&
             (zmw_le16_to_cpu(pAuthFrame->status) == 0))
              //&& (pAuthFrame->challengeText[1] <= 255) )
        {
            zfMemoryCopy(wd->sta.challengeText, pAuthFrame->challengeText,
                         pAuthFrame->challengeText[1]+2);

            /* send the 3rd authentication frame */
            p1 = 0x30001;
            p2 = 0;
            zfSendMmFrame(dev, ZM_WLAN_FRAME_TYPE_AUTH,
                          wd->sta.bssid, p1, p2, 0);

            zmw_enter_critical_section(dev);
            wd->sta.connectTimer = wd->tick;

            zm_debug_msg0("ZM_STA_SUB_STATE_AUTH_SHARE_2");
            wd->sta.connectState = ZM_STA_CONN_STATE_AUTH_SHARE_2;
            zmw_leave_critical_section(dev);
        }
        else
        {
            zm_debug_msg1("authentication failed, status = ",
                          pAuthFrame->status);

            zm_debug_msg0("ZM_STA_STATE_DISCONNECT");
            zfStaConnectFail(dev, ZM_STATUS_MEDIA_DISCONNECT_AUTH_FAILED, wd->sta.bssid, 3);
        }
    }
    else if ( wd->sta.connectState == ZM_STA_CONN_STATE_AUTH_SHARE_2 )
    {
        if ( (zmw_le16_to_cpu(pAuthFrame->algo) == 1)&&
             (zmw_le16_to_cpu(pAuthFrame->seq) == 4)&&
             (zmw_le16_to_cpu(pAuthFrame->status) == 0) )
        {
            //Set channel according to AP's configuration
            //Move to here because of Cisco 11n AP feature
            zfCoreSetFrequencyEx(dev, wd->frequency, wd->BandWidth40,
                    wd->ExtOffset, NULL);

            /* send association frame */
            zfSendMmFrame(dev, ZM_WLAN_FRAME_TYPE_ASOCREQ,
                          wd->sta.bssid, 0, 0, 0);

            zmw_enter_critical_section(dev);
            wd->sta.connectTimer = wd->tick;

            zm_debug_msg0("ZM_STA_SUB_STATE_ASSOCIATE");
            wd->sta.connectState = ZM_STA_CONN_STATE_ASSOCIATE;
            zmw_leave_critical_section(dev);
        }
        else
        {
            zm_debug_msg1("authentication failed, status = ",
                          pAuthFrame->status);

            zm_debug_msg0("ZM_STA_STATE_DISCONNECT");
            zfStaConnectFail(dev, ZM_STATUS_MEDIA_DISCONNECT_AUTH_FAILED, wd->sta.bssid, 3);
        }
    }
    else
    {
        zm_debug_msg0("unknown case");
    }
}

void zfStaProcessAsocReq(zdev_t* dev, zbuf_t* buf, u16_t* src, u16_t apId)
{

    return;
}

void zfStaProcessAsocRsp(zdev_t* dev, zbuf_t* buf)
{
    struct zsWlanAssoFrameHeader* pAssoFrame;
    u8_t  pBuf[sizeof(struct zsWlanAssoFrameHeader)];
    u16_t offset;
    u32_t i;
    u32_t oneTxStreamCap;

    zmw_get_wlan_dev(dev);

    if ( !zfStaIsConnecting(dev) )
    {
        return;
    }

    pAssoFrame = (struct zsWlanAssoFrameHeader*) pBuf;
    zfCopyFromRxBuffer(dev, buf, pBuf, 0, sizeof(struct zsWlanAssoFrameHeader));

    if ( wd->sta.connectState == ZM_STA_CONN_STATE_ASSOCIATE )
    {
        if ( pAssoFrame->status == 0 )
        {
            zm_debug_msg0("ZM_STA_STATE_CONNECTED");

            if (wd->sta.EnableHT == 1)
            {
                wd->sta.wmeConnected = 1;
            }
            if ((wd->sta.wmeEnabled & ZM_STA_WME_ENABLE_BIT) != 0) //WME enabled
            {
                /* Asoc rsp may contain WME parameter element */
                if ((offset = zfFindWifiElement(dev, buf, 2, 1)) != 0xffff)
                {
                    zm_debug_msg0("WME enable");
                    wd->sta.wmeConnected = 1;
                    if ((wd->sta.wmeEnabled & ZM_STA_UAPSD_ENABLE_BIT) != 0)
                    {
                        if ((zmw_rx_buf_readb(dev, buf, offset+8) & 0x80) != 0)
                        {
                            zm_debug_msg0("UAPSD enable");
                            wd->sta.qosInfo = wd->sta.wmeQosInfo;
                        }
                    }

                    zfStaUpdateWmeParameter(dev, buf);
                }
            }


            //Store asoc response frame body, for VISTA only
            wd->sta.asocRspFrameBodySize = zfwBufGetSize(dev, buf)-24;
            if (wd->sta.asocRspFrameBodySize > ZM_CACHED_FRAMEBODY_SIZE)
            {
                wd->sta.asocRspFrameBodySize = ZM_CACHED_FRAMEBODY_SIZE;
            }
            for (i=0; i<wd->sta.asocRspFrameBodySize; i++)
            {
                wd->sta.asocRspFrameBody[i] = zmw_rx_buf_readb(dev, buf, i+24);
            }

            zfStaStoreAsocRspIe(dev, buf);
            if (wd->sta.EnableHT &&
                ((wd->sta.ie.HtCap.HtCapInfo & HTCAP_SupChannelWidthSet) != 0) &&
                (wd->ExtOffset != 0))
            {
                wd->sta.htCtrlBandwidth = 1;
            }
            else
            {
                wd->sta.htCtrlBandwidth = 0;
            }

            //Set channel according to AP's configuration
            //zfCoreSetFrequencyEx(dev, wd->frequency, wd->BandWidth40,
            //        wd->ExtOffset, NULL);

            if (wd->sta.EnableHT == 1)
            {
                wd->addbaComplete = 0;

                if ((wd->sta.SWEncryptEnable & ZM_SW_TKIP_ENCRY_EN) == 0 &&
                    (wd->sta.SWEncryptEnable & ZM_SW_WEP_ENCRY_EN) == 0)
                {
                    wd->addbaCount = 1;
                    zfAggSendAddbaRequest(dev, wd->sta.bssid, 0, 0);
                    zfTimerSchedule(dev, ZM_EVENT_TIMEOUT_ADDBA, 100);
                }
            }

            /* set RIFS support */
            if(wd->sta.ie.HtInfo.ChannelInfo & ExtHtCap_RIFSMode)
            {
                wd->sta.HT2040 = 1;
//                zfHpSetRifs(dev, wd->sta.EnableHT, 1, (wd->sta.currentFrequency < 3000)? 1:0);
            }

            wd->sta.aid = pAssoFrame->aid & 0x3fff;
            wd->sta.oppositeCount = 0;    /* reset opposite count */
            zfStaSetOppositeInfoFromRxBuf(dev, buf);

            wd->sta.rxBeaconCount = 16;

            zfChangeAdapterState(dev, ZM_STA_STATE_CONNECTED);
            wd->sta.connPowerInHalfDbm = zfHpGetTransmitPower(dev);
            if (wd->zfcbConnectNotify != NULL)
            {
                if (wd->sta.EnableHT != 0) /* 11n */
            	{
    		        oneTxStreamCap = (zfHpCapability(dev) & ZM_HP_CAP_11N_ONE_TX_STREAM);
    		        if (wd->sta.htCtrlBandwidth == 1) /* HT40*/
    		        {
    					if(oneTxStreamCap) /* one Tx stream */
    				    {
    				        if (wd->sta.SG40)
    				        {
    				            wd->CurrentTxRateKbps = 150000;
    						    wd->CurrentRxRateKbps = 300000;
    				        }
    				        else
    				        {
    				            wd->CurrentTxRateKbps = 135000;
    						    wd->CurrentRxRateKbps = 270000;
    				        }
    				    }
    				    else /* Two Tx streams */
    				    {
    				        if (wd->sta.SG40)
    				        {
    				            wd->CurrentTxRateKbps = 300000;
    						    wd->CurrentRxRateKbps = 300000;
    				        }
    				        else
    				        {
    				            wd->CurrentTxRateKbps = 270000;
    						    wd->CurrentRxRateKbps = 270000;
    				        }
    				    }
    		        }
    		        else /* HT20 */
    		        {
    		            if(oneTxStreamCap) /* one Tx stream */
    				    {
    				        wd->CurrentTxRateKbps = 650000;
    						wd->CurrentRxRateKbps = 130000;
    				    }
    				    else /* Two Tx streams */
    				    {
    				        wd->CurrentTxRateKbps = 130000;
    					    wd->CurrentRxRateKbps = 130000;
    				    }
    		        }
                }
                else /* 11abg */
                {
                    if (wd->sta.connection_11b != 0)
                    {
                        wd->CurrentTxRateKbps = 11000;
    			        wd->CurrentRxRateKbps = 11000;
                    }
                    else
                    {
                        wd->CurrentTxRateKbps = 54000;
    			        wd->CurrentRxRateKbps = 54000;
    			    }
                }


                wd->zfcbConnectNotify(dev, ZM_STATUS_MEDIA_CONNECT, wd->sta.bssid);
            }
            wd->sta.connectByReasso = TRUE;
            wd->sta.failCntOfReasso = 0;

            zfPowerSavingMgrConnectNotify(dev);

            /* Disable here because fixed rate is only for test, TBD. */
            //if (wd->sta.EnableHT)
            //{
            //    wd->txMCS = 7; //Rate = 65Mbps
            //    wd->txMT = 2; // Ht rate
            //    wd->enableAggregation = 2; // Enable Aggregation
            //}
        }
        else
        {
            zm_debug_msg1("association failed, status = ",
                          pAssoFrame->status);

            zm_debug_msg0("ZM_STA_STATE_DISCONNECT");
            wd->sta.connectByReasso = FALSE;
            zfStaConnectFail(dev, ZM_STATUS_MEDIA_DISCONNECT_ASOC_FAILED, wd->sta.bssid, 3);
        }
    }

}

void zfStaStoreAsocRspIe(zdev_t* dev, zbuf_t* buf)
{
    u16_t offset;
    u32_t i;
    u16_t length;
    u8_t  *htcap;
    u8_t  asocBw40 = 0;
    u8_t  asocExtOffset = 0;

    zmw_get_wlan_dev(dev);

    for (i=0; i<wd->sta.asocRspFrameBodySize; i++)
    {
        wd->sta.asocRspFrameBody[i] = zmw_rx_buf_readb(dev, buf, i+24);
    }

    /* HT capabilities: 28 octets */
    if (    ((wd->sta.currentFrequency > 3000) && !(wd->supportMode & ZM_WIRELESS_MODE_5_N))
         || ((wd->sta.currentFrequency < 3000) && !(wd->supportMode & ZM_WIRELESS_MODE_24_N)) )
    {
        /* not 11n AP */
        htcap = (u8_t *)&wd->sta.ie.HtCap;
        for (i=0; i<28; i++)
        {
            htcap[i] = 0;
        }
        wd->BandWidth40 = 0;
        wd->ExtOffset = 0;
        return;
    }

    if ((offset = zfFindElement(dev, buf, ZM_WLAN_EID_HT_CAPABILITY)) != 0xffff)
    {
        /* atheros pre n */
        zm_debug_msg0("atheros pre n");
        htcap = (u8_t *)&wd->sta.ie.HtCap;
        htcap[0] = zmw_rx_buf_readb(dev, buf, offset);
        htcap[1] = 26;
        for (i=1; i<=26; i++)
        {
            htcap[i+1] = zmw_rx_buf_readb(dev, buf, offset + i);
            zm_msg2_mm(ZM_LV_1, "ASOC:  HT Capabilities, htcap=", htcap[i+1]);
        }
    }
    else if ((offset = zfFindElement(dev, buf, ZM_WLAN_PREN2_EID_HTCAPABILITY)) != 0xffff)
    {
        /* pre n 2.0 standard */
        zm_debug_msg0("pre n 2.0 standard");
        htcap = (u8_t *)&wd->sta.ie.HtCap;
        for (i=0; i<28; i++)
        {
            htcap[i] = zmw_rx_buf_readb(dev, buf, offset + i);
            zm_msg2_mm(ZM_LV_1, "ASOC:  HT Capabilities, htcap=", htcap[i]);
        }
    }
    else
    {
        /* not 11n AP */
        htcap = (u8_t *)&wd->sta.ie.HtCap;
        for (i=0; i<28; i++)
        {
            htcap[i] = 0;
        }
        wd->BandWidth40 = 0;
        wd->ExtOffset = 0;
        return;
    }

    asocBw40 = (u8_t)((wd->sta.ie.HtCap.HtCapInfo & HTCAP_SupChannelWidthSet) >> 1);

    /* HT information */
    if ((offset = zfFindElement(dev, buf, ZM_WLAN_EID_EXTENDED_HT_CAPABILITY)) != 0xffff)
    {
        /* atheros pre n */
        zm_debug_msg0("atheros pre n HTINFO");
        length = 22;
        htcap = (u8_t *)&wd->sta.ie.HtInfo;
        htcap[0] = zmw_rx_buf_readb(dev, buf, offset);
        htcap[1] = 22;
        for (i=1; i<=22; i++)
        {
            htcap[i+1] = zmw_rx_buf_readb(dev, buf, offset + i);
            zm_msg2_mm(ZM_LV_1, "ASOC:  HT Info, htinfo=", htcap[i+1]);
        }
    }
    else if ((offset = zfFindElement(dev, buf, ZM_WLAN_PREN2_EID_HTINFORMATION)) != 0xffff)
    {
        /* pre n 2.0 standard */
        zm_debug_msg0("pre n 2.0 standard HTINFO");
        length = zmw_rx_buf_readb(dev, buf, offset + 1);
        htcap = (u8_t *)&wd->sta.ie.HtInfo;
        for (i=0; i<24; i++)
        {
            htcap[i] = zmw_rx_buf_readb(dev, buf, offset + i);
            zm_msg2_mm(ZM_LV_1, "ASOC:  HT Info, htinfo=", htcap[i]);
        }
    }
    else
    {
        zm_debug_msg0("no HTINFO");
        htcap = (u8_t *)&wd->sta.ie.HtInfo;
        for (i=0; i<24; i++)
        {
            htcap[i] = 0;
        }
    }
    asocExtOffset = wd->sta.ie.HtInfo.ChannelInfo & ExtHtCap_ExtChannelOffsetBelow;

    if ((wd->sta.EnableHT == 1) && (asocBw40 == 1) && ((asocExtOffset == 1) || (asocExtOffset == 3)))
    {
        wd->BandWidth40 = asocBw40;
        wd->ExtOffset = asocExtOffset;
    }
    else
    {
        wd->BandWidth40 = 0;
        wd->ExtOffset = 0;
    }

    return;
}

void zfStaProcessDeauth(zdev_t* dev, zbuf_t* buf)
{
    u16_t apMacAddr[3];

    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    /* STA : if SA=connected AP then disconnect with AP */
    if ( wd->wlanMode == ZM_MODE_INFRASTRUCTURE )
    {
        apMacAddr[0] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A3_OFFSET);
        apMacAddr[1] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A3_OFFSET+2);
        apMacAddr[2] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A3_OFFSET+4);
  	if ((apMacAddr[0] == wd->sta.bssid[0]) && (apMacAddr[1] == wd->sta.bssid[1]) && (apMacAddr[2] == wd->sta.bssid[2]))
        {
            if (zfwBufGetSize(dev, buf) >= 24+2) //not a malformed frame
            {
                if ( zfStaIsConnected(dev) )
                {
                    zfStaConnectFail(dev, ZM_STATUS_MEDIA_DISCONNECT_DEAUTH, wd->sta.bssid, 2);
                }
                else if (zfStaIsConnecting(dev))
                {
                    zfStaConnectFail(dev, ZM_STATUS_MEDIA_DISCONNECT_AUTH_FAILED, wd->sta.bssid, 3);
                }
                else
                {
                }
            }
        }
    }
    else if ( wd->wlanMode == ZM_MODE_IBSS )
    {
        u16_t peerMacAddr[3];
        u8_t  peerIdx;
        s8_t  res;

        if ( zfStaIsConnected(dev) )
        {
            peerMacAddr[0] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A2_OFFSET);
            peerMacAddr[1] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A2_OFFSET+2);
            peerMacAddr[2] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A2_OFFSET+4);

            zmw_enter_critical_section(dev);
            res = zfStaFindOppositeByMACAddr(dev, peerMacAddr, &peerIdx);
            if ( res == 0 )
            {
                wd->sta.oppositeInfo[peerIdx].aliveCounter = 0;
            }
            zmw_leave_critical_section(dev);
        }
    }
}

void zfStaProcessDisasoc(zdev_t* dev, zbuf_t* buf)
{
    u16_t apMacAddr[3];

    zmw_get_wlan_dev(dev);

    /* STA : if SA=connected AP then disconnect with AP */
    if ( wd->wlanMode == ZM_MODE_INFRASTRUCTURE )
    {
        apMacAddr[0] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A3_OFFSET);
        apMacAddr[1] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A3_OFFSET+2);
        apMacAddr[2] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A3_OFFSET+4);

        if ((apMacAddr[0] == wd->sta.bssid[0]) && (apMacAddr[1] == wd->sta.bssid[1]) && (apMacAddr[2] == wd->sta.bssid[2]))
        {
            if (zfwBufGetSize(dev, buf) >= 24+2) //not a malformed frame
            {
                if ( zfStaIsConnected(dev) )
                {
                    zfStaConnectFail(dev, ZM_STATUS_MEDIA_DISCONNECT_DISASOC, wd->sta.bssid, 2);
                }
                else
                {
                    zfStaConnectFail(dev, ZM_STATUS_MEDIA_DISCONNECT_ASOC_FAILED, wd->sta.bssid, 3);
                }
            }
        }
    }
}



/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfProcessProbeReq           */
/*      Process probe request management frame.                         */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      buf : auth frame buffer                                         */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      none                                                            */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen Chen        ZyDAS Technology Corporation    2005.10     */
/*                                                                      */
/************************************************************************/
void zfStaProcessProbeReq(zdev_t* dev, zbuf_t* buf, u16_t* src)
{
    u16_t offset;
    u8_t len;
    u16_t i, j;
    u16_t sendFlag;

    zmw_get_wlan_dev(dev);

    /* check mode : AP/IBSS */
    if ((wd->wlanMode != ZM_MODE_AP) || (wd->wlanMode != ZM_MODE_IBSS))
    {
        zm_msg0_mm(ZM_LV_3, "Ignore probe req");
        return;
    }

    /* check SSID */
    if ((offset = zfFindElement(dev, buf, ZM_WLAN_EID_SSID)) == 0xffff)
    {
        zm_msg0_mm(ZM_LV_3, "probe req SSID not found");
        return;
    }

    len = zmw_rx_buf_readb(dev, buf, offset+1);

    for (i=0; i<ZM_MAX_AP_SUPPORT; i++)
    {
        if ((wd->ap.apBitmap & (i<<i)) != 0)
        {
            sendFlag = 0;
            /* boardcast SSID */
            if ((len == 0) && (wd->ap.hideSsid[i] == 0))
            {
                sendFlag = 1;
            }
            /* Not broadcast SSID */
            else if (wd->ap.ssidLen[i] == len)
            {
                for (j=0; j<len; j++)
                {
                    if (zmw_rx_buf_readb(dev, buf, offset+1+j)
                            != wd->ap.ssid[i][j])
                    {
                        break;
                    }
                }
                if (j == len)
                {
                    sendFlag = 1;
                }
            }
            if (sendFlag == 1)
            {
                /* Send probe response */
                zfSendMmFrame(dev, ZM_WLAN_FRAME_TYPE_PROBERSP, src, i, 0, 0);
            }
        }
    }
}

void zfStaProcessProbeRsp(zdev_t* dev, zbuf_t* buf, struct zsAdditionInfo* AddInfo)
{
    /* return if not channel scan */
    // Probe response is sent with unicast. Is this required?
    // IBSS would send probe request and the code below would prevent
    // the probe response from handling.
    #if 0
    zmw_get_wlan_dev(dev);

    if ( !wd->sta.bChannelScan )
    {
        return;
    }
    #endif

    zfProcessProbeRsp(dev, buf, AddInfo);
}

void zfIBSSSetupBssDesc(zdev_t *dev)
{
#ifdef ZM_ENABLE_IBSS_WPA2PSK
    u8_t i;
#endif
    struct zsBssInfo *pBssInfo;
    u16_t offset = 0;

    zmw_get_wlan_dev(dev);

    pBssInfo = &wd->sta.ibssBssDesc;
    zfZeroMemory((u8_t *)pBssInfo, sizeof(struct zsBssInfo));

    pBssInfo->signalStrength = 100;

    zfMemoryCopy((u8_t *)pBssInfo->macaddr, (u8_t *)wd->macAddr,6);
    zfMemoryCopy((u8_t *)pBssInfo->bssid, (u8_t *)wd->sta.bssid, 6);

    pBssInfo->beaconInterval[0] = (u8_t)(wd->beaconInterval) ;
    pBssInfo->beaconInterval[1] = (u8_t)((wd->beaconInterval) >> 8) ;

    pBssInfo->capability[0] = wd->sta.capability[0];
    pBssInfo->capability[1] = wd->sta.capability[1];

    pBssInfo->ssid[0] = ZM_WLAN_EID_SSID;
    pBssInfo->ssid[1] = wd->sta.ssidLen;
    zfMemoryCopy((u8_t *)&pBssInfo->ssid[2], (u8_t *)wd->sta.ssid, wd->sta.ssidLen);
    zfMemoryCopy((u8_t *)&pBssInfo->frameBody[offset], (u8_t *)pBssInfo->ssid,
                 wd->sta.ssidLen + 2);
    offset += wd->sta.ssidLen + 2;

    /* support rate */

    /* DS parameter set */
    pBssInfo->channel = zfChFreqToNum(wd->frequency, NULL);
    pBssInfo->frequency = wd->frequency;
    pBssInfo->atimWindow = wd->sta.atimWindow;

#ifdef ZM_ENABLE_IBSS_WPA2PSK
    if ( wd->sta.authMode == ZM_AUTH_MODE_WPA2PSK )
    {
        u8_t rsn[64]=
        {
                    /* Element ID */
                    0x30,
                    /* Length */
                    0x14,
                    /* Version */
                    0x01, 0x00,
                    /* Group Cipher Suite, default=TKIP */
                    0x00, 0x0f, 0xac, 0x04,
                    /* Pairwise Cipher Suite Count */
                    0x01, 0x00,
                    /* Pairwise Cipher Suite, default=TKIP */
                    0x00, 0x0f, 0xac, 0x02,
                    /* Authentication and Key Management Suite Count */
                    0x01, 0x00,
                    /* Authentication type, default=PSK */
                    0x00, 0x0f, 0xac, 0x02,
                    /* RSN capability */
                    0x00, 0x00
        };

        /* Overwrite Group Cipher Suite by AP's setting */
        zfMemoryCopy(rsn+4, zgWpa2AesOui, 4);

        if ( wd->sta.wepStatus == ZM_ENCRYPTION_AES )
        {
            /* Overwrite Pairwise Cipher Suite by AES */
            zfMemoryCopy(rsn+10, zgWpa2AesOui, 4);
        }

        // RSN element id
        pBssInfo->frameBody[offset++] = ZM_WLAN_EID_RSN_IE ;

        // RSN length
        pBssInfo->frameBody[offset++] = rsn[1] ;

        // RSN information
        for(i=0; i<rsn[1]; i++)
        {
            pBssInfo->frameBody[offset++] = rsn[i+2] ;
        }

        zfMemoryCopy(pBssInfo->rsnIe, rsn, rsn[1]+2);
    }
#endif
}

void zfIbssConnectNetwork(zdev_t* dev)
{
    struct zsBssInfo* pBssInfo;
    struct zsBssInfo tmpBssInfo;
    u8_t   macAddr[6], bssid[6], bssNotFound = TRUE;
    u16_t  i, j=100;
    u16_t  k;
    struct zsPartnerNotifyEvent event;
    u32_t  channelFlags;
    u16_t  oppositeWepStatus;

    zmw_get_wlan_dev(dev);

    zmw_declare_for_critical_section();

    /* change state to CONNECTING and stop the channel scanning */
    zfChangeAdapterState(dev, ZM_STA_STATE_CONNECTING);
    zfPowerSavingMgrWakeup(dev);

    /* Set TxQs CWMIN, CWMAX, AIFS and TXO to WME STA default. */
    zfUpdateDefaultQosParameter(dev, 0);

    wd->sta.bProtectionMode = FALSE;
    zfHpSetSlotTime(dev, 1);

    /* ESS bit off */
    wd->sta.capability[0] &= ~ZM_BIT_0;
    /* IBSS bit on */
    wd->sta.capability[0] |= ZM_BIT_1;
    /* not not use short slot time */
    wd->sta.capability[1] &= ~ZM_BIT_2;

    wd->sta.wmeConnected = 0;
    wd->sta.psMgr.tempWakeUp = 0;
    wd->sta.qosInfo = 0;
    wd->sta.EnableHT = 0;
    wd->BandWidth40 = 0;
    wd->ExtOffset = 0;

    if ( wd->sta.bssList.bssCount )
    {
        //Reorder BssList by RSSI--CWYang(+)
        zfBssInfoReorderList(dev);

        zmw_enter_critical_section(dev);

        pBssInfo = wd->sta.bssList.head;

        for(i=0; i<wd->sta.bssList.bssCount; i++)
        {
            // 20070806 #1 Privacy bit
            if ( pBssInfo->capability[0] & ZM_BIT_4 )
            { // Privacy Ibss network
//                zm_debug_msg0("Privacy bit on");
                oppositeWepStatus = ZM_ENCRYPTION_WEP_ENABLED;

                if ( pBssInfo->rsnIe[1] != 0 )
                {
                    if ( (pBssInfo->rsnIe[7] == 0x01) || (pBssInfo->rsnIe[7] == 0x05) )
                    { // WEP-40 & WEP-104
//                        zm_debug_msg0("WEP40 or WEP104");
                        oppositeWepStatus = ZM_ENCRYPTION_WEP_ENABLED;
                    }
                    else if ( pBssInfo->rsnIe[7] == 0x02 )
                    { // TKIP
//                        zm_debug_msg0("TKIP");
                        oppositeWepStatus = ZM_ENCRYPTION_TKIP;
                    }
                    else if ( pBssInfo->rsnIe[7] == 0x04 )
                    { // AES
//                        zm_debug_msg0("CCMP-AES");
                        oppositeWepStatus = ZM_ENCRYPTION_AES;
                    }
                }
            }
            else
            {
//                zm_debug_msg0("Privacy bit off");
                oppositeWepStatus = ZM_ENCRYPTION_WEP_DISABLED;
            }

            if ( (zfMemoryIsEqual(&(pBssInfo->ssid[2]), wd->sta.ssid,
                                  wd->sta.ssidLen))&&
                 (wd->sta.ssidLen == pBssInfo->ssid[1])&&
                 (oppositeWepStatus == wd->sta.wepStatus) )
            {
                /* Check support mode */
                if (pBssInfo->frequency > 3000) {
                    if (   (pBssInfo->EnableHT == 1)
                        || (pBssInfo->apCap & ZM_All11N_AP) ) //11n AP
                    {
                        channelFlags = CHANNEL_A_HT;
                        if (pBssInfo->enableHT40 == 1) {
                            channelFlags |= CHANNEL_HT40;
                        }
                    } else {
                        channelFlags = CHANNEL_A;
                    }
                } else {
                    if (   (pBssInfo->EnableHT == 1)
                        || (pBssInfo->apCap & ZM_All11N_AP) ) //11n AP
                    {
                        channelFlags = CHANNEL_G_HT;
                        if(pBssInfo->enableHT40 == 1) {
                            channelFlags |= CHANNEL_HT40;
                        }
                    } else {
                        if (pBssInfo->extSupportedRates[1] == 0) {
                            channelFlags = CHANNEL_B;
                        } else {
                            channelFlags = CHANNEL_G;
                        }
                    }
                }

                if (   ((channelFlags == CHANNEL_B) && (wd->connectMode & ZM_BIT_0))
                    || ((channelFlags == CHANNEL_G) && (wd->connectMode & ZM_BIT_1))
                    || ((channelFlags == CHANNEL_A) && (wd->connectMode & ZM_BIT_2))
                    || ((channelFlags & CHANNEL_HT20) && (wd->connectMode & ZM_BIT_3)) )
                {
                    pBssInfo = pBssInfo->next;
                    continue;
                }

                /* Bypass DFS channel */
                if (zfHpIsDfsChannelNCS(dev, pBssInfo->frequency))
                {
                    zm_debug_msg0("Bypass DFS channel");
                    continue;
                }

                /* check IBSS bit */
                if ( pBssInfo->capability[0] & ZM_BIT_1 )
                {
                    /* may check timestamp here */
                    j = i;
                    break;
                }
            }

            pBssInfo = pBssInfo->next;
        }

        if ((j < wd->sta.bssList.bssCount) && (pBssInfo != NULL))
        {
            zfwMemoryCopy((u8_t*)&tmpBssInfo, (u8_t*)(pBssInfo), sizeof(struct zsBssInfo));
            pBssInfo = &tmpBssInfo;
        }
        else
        {
            pBssInfo = NULL;
        }

        zmw_leave_critical_section(dev);

        //if ( j < wd->sta.bssList.bssCount )
        if (pBssInfo != NULL)
        {
            int res;

            zm_debug_msg0("IBSS found");

            /* Found IBSS, reset bssNotFoundCount */
            zmw_enter_critical_section(dev);
            wd->sta.bssNotFoundCount = 0;
            zmw_leave_critical_section(dev);

            bssNotFound = FALSE;
            wd->sta.atimWindow = pBssInfo->atimWindow;
            wd->frequency = pBssInfo->frequency;
            //wd->sta.flagFreqChanging = 1;
            zfCoreSetFrequency(dev, wd->frequency);
            zfUpdateBssid(dev, pBssInfo->bssid);
            zfResetSupportRate(dev, ZM_DEFAULT_SUPPORT_RATE_ZERO);
            zfUpdateSupportRate(dev, pBssInfo->supportedRates);
            zfUpdateSupportRate(dev, pBssInfo->extSupportedRates);
            wd->beaconInterval = pBssInfo->beaconInterval[0] +
                                 (((u16_t) pBssInfo->beaconInterval[1]) << 8);

            if (wd->beaconInterval == 0)
            {
                wd->beaconInterval = 100;
            }

            /* rsn information element */
            if ( pBssInfo->rsnIe[1] != 0 )
            {
                zfMemoryCopy(wd->sta.rsnIe, pBssInfo->rsnIe,
                             pBssInfo->rsnIe[1]+2);

#ifdef ZM_ENABLE_IBSS_WPA2PSK
                /* If not use RSNA , run traditional */
                zmw_enter_critical_section(dev);
                wd->sta.ibssWpa2Psk = 1;
                zmw_leave_critical_section(dev);
#endif
            }
            else
            {
                wd->sta.rsnIe[1] = 0;
            }

            /* privacy bit */
            if ( pBssInfo->capability[0] & ZM_BIT_4 )
            {
                wd->sta.capability[0] |= ZM_BIT_4;
            }
            else
            {
                wd->sta.capability[0] &= ~ZM_BIT_4;
            }

            /* preamble type */
            wd->preambleTypeInUsed = wd->preambleType;
            if ( wd->preambleTypeInUsed == ZM_PREAMBLE_TYPE_AUTO )
            {
                if (pBssInfo->capability[0] & ZM_BIT_5)
                {
                    wd->preambleTypeInUsed = ZM_PREAMBLE_TYPE_SHORT;
                }
                else
                {
                    wd->preambleTypeInUsed = ZM_PREAMBLE_TYPE_LONG;
                }
            }

            if (wd->preambleTypeInUsed == ZM_PREAMBLE_TYPE_LONG)
            {
                wd->sta.capability[0] &= ~ZM_BIT_5;
            }
            else
            {
                wd->sta.capability[0] |= ZM_BIT_5;
            }

  /*
 * Copywd->sta.beaconFrameBodySize = pBssInfo->f008 Athesos C+ 12;

/*
 * Copyr if (ight (c) 2007-2008 Atheros C> ZM_CACHED_FRAMEBODY_SIZE)ion to use, c{ion to use, copyright (c) 2007-2008 Atheros Coms software for any
 * pu;ion to use, c}sion to use, cfor (k=0; k<8; k++rpose with or without fee is hereby granted, provided t[k]CommunicationtimeStamp[k]ce and this permut fee is heDED "AS IS" AND THE AUTH8R DISCLAIMS AL 2007-Interval[0S
 * WITH REGARDED "AS IS" AND THE AUTH9D WARRANTIES OF
 * MERCHANTAB1LITY AND FITNESS. IN NO EVENT SHALL THE10R DISCLAIMS ALcapabilityBILITY AND FITNESS. IN NO EVENT SHALL THE11 DAMAGES OR ANY DAMAGES
 PECIAL, DIRECT, // appear12in aight (c) 2007-2008 Atheros l copies.
 *
 * THE appear in amunications Inc.
 *
 * l copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHO+12R DISCLAIMS ALs Inc.
 *IES
 * WITH REGARD  TO THIS SOFTzmw_eRCHA_critical_section(dev)ce and this peres = zfStaSetOppositecatiFromBSScaticons,mmunicatitant */
u8_t   opy, zgWpa= 0 rpose with or without fee is herzfMemoryCopy(event.bssid, (u8_t *)(RRANTIES OF 0x0), 6tant */
u8_t   pa2RadiusOui[] = { 0x00peerMacAddrf, 0xac, 0x01 };
u8_tmacaddrWpa2AesOui[] = { 0xD TO THIS SOFTchanleaveobal variable to constanN
 * ACTION OF zfwIbssPartnerNotify, 0x011, &{ 0x0tant */
u8_t   goto connect_donece and thiD TO T.h"

/*/* IBSS not found */ TO T] = {bssNotF*****rpose {
#ifdefe abENABLE_****_WPA2PSK TO THIS u16_t offset ;
#endif TO THIS ] = {ight (c)ibssJoinOnlyx50, 0xf2, without fee iszm_debug_msg0("*****join only...retrking"oid zfStaStartConnecList _*/
/dev);

/****           , modify, **********Count<2TION DESCRIPTION          ange global variable to constant */
u8_t             zf1taPutAp*********, do  { 0survey!! ************/
/* =",SS. IN NO                tant */
u8_t        */
/*      dev : de++            */
/7,
                                 Put AP into blocking AP list.     /*
 * Coelse                                                                  *//* Fail******       TODO create*************inter                            = 0               */
/*      bssid : AP's BSSID           }
             (zfHpIsDfsChannel, 0x01ightfrequency)TION DESCRIPTION                     paRadHpFindFirstNon   none                       > 3000                              ws.autoSetF        */, 0x50, 0xf2, {*/
/Auto                              
/*    INPUTS  C      Ad Hoc Network Band        UTHOdhocModevice pointer                */
/*    Clean                   .    2006.12     */
/*         UTHOR                 0xffdev);

/********  */
/*    INPUTS                        d one in cone   ons, INC            255,  511        */
/unics     or = 1  255,  511//ight (c)flag    nonegingj;
    zgWpa2RadCore                                _for_critiopy, modify, DesiredB 0x0    TRUurpose withwithout fee is appear in a6l copies.
 *
 * THE SOFTWARE IS PROVI   zgHOR DIight (c)ditical_sectIES
 * WITH REGARD TO THIS       */
/*      weight : weight of A#if 1             mst u16[L DAM, modgApList[i].&*****               gApList[DATA dr[j]!= bssid[j]>> 8            {
         cludedr[j]!= bssid1j])
                {
         3

            if(j==             }
            4

            if(2j])
                {
         5 bssid doesn't have              }
    zfGenerateRandx50, 0D, 0x01 0xac, 0r[j]!= bssi        fo   zgW              #*      weighte bssid entry first*/
        for (i=0; i<ZM_MAX_BLOCKING_AP_LIST 0xac)_LISTt
     Number, 0x01             15,   31,   63,  ING_AP[j])= ~ZM_BIT_    OUTPUTS    
        |he ab    e_for_critiE; i++                      */zfUpdate_sect, 0x01KING_AP_LIST_SIZwlan_dev(datimWindow*****0a   zmw_get_w* IZE) informa to        */
/*                 <he aboH_G_14)LIST 2.4 GHz  b+g
        /*Find same bs         wfc.b047,G6.12sOui[] = { 0x00,  &&y, modiupport6.12 & (ZM_WIRELESS_MODE_24_54|st[i].addr[j] = bssiN))x50, 0xf2, 0x04 };
u8_t   zgWpa2RadResetSa.blocRate, 0x01ZM_DEFAULT_SUPPORT_RAT       AG 3,    7,   15,   31,   63,  +)
            {
ApList[i].weight = weight;
        zmw_leave_critical_section(dev);
   B 3,    7,   15,   31,   6}*****RIPTION         weight;
        zmw_leave_critical_section(dev);
    }

    retur         */
/           wepStatux00, /
/* CRYPTION_WEP_DIS    DCTION DESCRIPTION        ight (c)Y DAMAGES
 *      }
     4                */
/*      weight : weight of A/*                    list is fu             sid, u8_t weipreambleTypeInUsedIST_SIZ        */
/  {
        zmw_en        */
/*      ding lPREAM    TYPE_LONG                     */
/*                               5                                      */
/*    INPU        */
/*      de      */
/*      SHORTice pointer                                      */
/*     for_critica****SetumuniDesc       2*******/
/*                   ING_AP_LIST 20070411 Add         (ZM_MAX_BLOCto its*******Communi!!!for_criti       , 0x    i = bssid[L WAsRANT       */
/*       += 8/*              2007- iRCHANTA       */
/*DED "AS IS" AND THE AUTH      ++)
        , modF
 * MERCHANTA) _for_criti                                                           */
/*     /*             Y DAMAGES
 (ZM_MAX_BLOCKING_AP_LIS                                                                Stephen Chen        Atheros Communicatio                   1************if(w0  i = bssid[secti           //
    uelement id                          */
/***************st[iLAN_EID_  {
***********    zmw_length                          */
/*********************** 0x0Len_enter_critical_sec(ZM_MAX_BLOfor_critifor(ir iniGLIGENCE +)
    ; iopies.
 *
 *     */
/*    INPUTS  ; i<ZM_MAX_BLOCKING_AP_LIST_SIZE; i++)
 [i************ */
/*     
{
 a.bloc[5] &            IZE)S   */ZMn(dev)SET_CCK***********/
/* (                     brOFDM)&&C.    g   z])
          wd->sta.blocwithout fee is          
/* j=0; j<6; j++)
            {
                    [j])
       get_wlan_dev(dev);
v);
    //zmw_declare_for_critical_section();

    //section(dev/*                      }
      tion(dev);
    ****lenO      */          255,  511, 1023      }
      ta.blockingApList[i]e bssid eght != 4          for (**********************/
/*       6)
   (0x1<<i)     hBlockLissOui[] = { 0x00, without fee is her                     */
/**************sOui[] = { 0x00, 	
    g11taRefTbl[i]+   zfStaRefBasicreshBlockLis<<(7-ListesOui[] = { 0x00, *******                 15,   31,   63,  .h"

/* TODO : ***********************************DED "AS IS" AND THE AUTH*********R DIl  {
        iddr[j] != bssiDS pa008 ter     16_t i, j;
    whole blocking lget_wan_dev(dev);
    //zmw_declare_for_critical_section();

    /DS/*                             *tion(dev);
    for (i=0; i<ZM_MAX_BLOCKING_AP_LIST_1      */
/*    OUTPUTS          ta.blockingApList[i]                               */
/*         	zfCh    ToNumSIZE-1);
     , NULL                *****ole blocking list           */
/*    AUTHOR    get_wlan_dev(dev);
    //zmw_declare_for_critical_section();

    /*****     */
/*    */
/*    AUTHOR    tion(dev);
    for (i=0; i<ZM_MAX_BLOCKING_AP_LIST_2tions, INC.    2006.12     */
/*                                          */
/*      zfStaIsApIn       {
  section(de                        * ERP catiMAX_BLOCatioExtended ;
     ed    zsCKING_AP_LIST_for (j=0; j<6; j++)
        {
        wd->sta.blockingApList[i].addr[j] = bssid[j];
        }

        wd->sta.blocurn TRUE;
     lushFlag)
{
    u1                   erpEet_wlan/*    OUTPUTS    //ushFl  }
    }
    //zmw_leave_critical_section(dev);
    return FALSE;
}


shFl                  shFlvice pointer                                         */
/*      none         if                        *************************/
u16_t zfStaIsight != 0)
  255,  511, 102* i;
    zmw_get_wlan_dev(dev);
    zm           }
            }
            if (j == 6)
            {
           n blocking list.      ve_critical_section(den;
}


/**********************************************/
/*/
/*          
           ;
    zmw_get_wlan_dev(d  }
    }
    //zmw_lea           {
                wd->sta.blockingApList[iXTENDED*********************ndle Connect failure.           *********************************/
/*                       */
/*    INPUTS                                                     */
8*    FUNCTION DESCRn blocking list.                zfS= 6)
   hBlockList       */
/*      Is AP in bln blocking list.                                  */
/*                                                    = 6)
             = 6)
                      */
*    INPUTS                                       15,   31,   63,                  */
       e;
    zm********************************	                                       */
/*      flu for (j=0; j<6; j++)
   try forr[j] != bssiRSN : imblocalan_g)
{
    u1infl    e the zgWul    PutApIdeclan              ev);
    zmw_declare/
voiuthkingA      AUTHj] = b : AP noLIST_SIZE; i++)
    {
  0xac,s Inc*/
/ction();

  for           /*    INPUTS   0xac, {
 sn[64]/*                                      /***et_wlanIDblockingApList[iason, u16_t*0x30,********/
void zfStaConnectFLion(d* dev, u16_t reason, u16_t* bs14d, u8_t weight)
{
    zmw_getVersBLOCKING_AP_LIS reason, u16_t* bs01,T);
id, u8_t weight)
{
    zmw_getGroup Cipher Suite, default=TKIPdev, ZM_STA_STATE_DISCONNECT);
0    /f    ac    /al state */
    zfChangeAdaptePairwiseHT AP, detail    */
ev, ZM_STA_STATE_DISCONNECT);

    /* Improve WEP/TKIP performace er of connection stat information please look bug#32495 */
    //zfHpSetTTSIFSTime(d2d, u8_t weight)
{
    zmw_getAuthen var   u16_t Key Managt_wlan status changes */
    if (wd->zfcbConnectNotify != NULL)
    {
        wd->zfgList(dev, (u8_typ reason, bsP no
    }

    /* Put AP into internal blocking list */
    zfStaPutApIntoBlockin               ease look bug#32495 */
    //zfHpSet_t* bssid) flus   return;
}


/**Overwrtatuwith HT AP, detail by AP's    Ather      */
/*     diusOui[] = rsn+4, zgWpa2AesOui, 4   255,  511, 10/
/*      Is AP in blocking list.       AESx50, 0xf2, 0x04 };
u8_t   zgWpa2RrStationsCouner of connection statu  zmE                    return wd->sta.opp10siteCount;
}

u8_t z                        */
/*       }
    }
    //zmw_leave_critical_section(dev);
    return FALSE;
}


RSN_I********************     }
            else
            {
                wd->s****t(zdea.oppositeCount;
                                   */
siteCosid : BSSID             > numToIterate )
    {
        oppositei+2ount = numToIteraeturn wd->staight (c)rsnIe,ositoppos[1]+2 list                             eerStationCb caIf     use;
  A , run tradi to /*    AUTHOR  O : change global variable to constant */
u8_t           */
/CounPsklare_for_criti3,  127,
                            try for replacement */
   , u8_t* bssid)
{
HT C DAMAGESieslag)
dev(dev);

                    OUI     b{T);
     9 zmw_g4C }unt = numToItera    //zmw_declare_for_critical_section();

    /WPAwd->sta.oppositeCou*************************/
u16_t zfStaIsApInHTCap.Data._wlan_d+ 4unt = numToItera      
     i < 3*    FUNCTION DESCRIPTION            
            {
                wd->s  inngApList[i].a                */
OUNT; i++)
    {
        if ( oppositeCount == 0 )
    ail(zdemw_en     break;
        }

      26 if ( wd->sta.oppositeInfo[i].valid == 0 )
        {
            continueteCount == 0 )Byte                 for (j=0; j<6; v, u16_t flus;
    zm_t *sa, int *pFoundIdx)
{
    int oppositeCount;
    int i;

    zmw_get_wlan_dev(dev);

    oppositeCount = wd->sta.oppositeCount;

    for(i=0; i < ZM_MAX_OPPOSITE_COUNT; i++)
    {
        if ( oppositeCount Ext== 0 )
        {
            break;
        }

        if ( wd->sta.oppositeInfo[i].valid == 0 )
        {
            continue;
        }

        oppositeCount--;
        if ( zfMemoryIsEqual((u8_t*) sa, wd an unused slsiteInfo[i].macAddr, 6) )
        {
      2     //wd->sta.oppositeInfo[i].aliveCounter++;
            wd->sta.oppositeInfo[id an unuseCounter = ZM_IBSS_PEER_ALIVE_COUNTE                        ight (c) 2007-2008 Atheros Com          {
        zmw_enter_cand/or distribute this software for any
 * purpose withj=0; j<6; j++)
            {
        hat the above
 * copyright notice and thiTER;

      /
/*     6 Let                     could ge gltephe, 2047, 4095, 4095,  fune to nter_critica************= FALS*************printk("The                 1 = %02x\n        */
Y DAMAGES
 * 
/*    INPUT      *pFoundIdx = (u8_t)i;
2            return 0;
        }1    }

    *pMAX_ar in aLIGENCE OR OTHER TORTIOUS ACTION, ARISING {
     pFoundIdx    ons, INC"AS IS" AND THE AUTHOR
/*    INPUT.oppositeCo u8_t* bssid)                                         adiusOui[] = { 0x00, 0x0f, 0xac, 0, 0x0f,2AesOui[] = 0x0f, 0xac, 0x04 };

const u16_t zcCwTlbr (i=0; i<ZM_ppositeCount+*/
/*      bssid : AP's BSSID           try for                 /                                         wlan_dev(d*/
/ 4095, in block         PARTNER_LOS           = 0xff/*      bssid : AP's BSSID       .oppos*      wewithout fe weight)
{
    u16_t i, j;
    OUT}

tCb(zdev_t* :oppos/*  EnableB2007-mw_leave_] = b****      F
 * MERCHANTA      dtim       /* Se/
void zfStaRe_dev(deadiusOund zsBssInfo);unt;
efresh  zsBssctCbtwlan appZD1211B HalPlus_t* dev, SetA      {
           ];
    int res;
 ******/ Starttephe*****L WArng lmonii, j appnew st, (u8((u8_t*                                    zfTsa, Schedulzmw_leave_EVENTalled MONITOReave_TICKddr;
    zfMe  dst = 27,
                            2*********ightzfcbCCb(zdesa, wd-!=       No encryption
#endM_HP_CAP_11N_ONE_mw_leave_STATUS_MEDIA_CONNECT       */
/* G_AP_LIST.oppos    angeAdaptnfo[i]zmw_leave_STA= 0)
E      /*ED_dev(dereturn 0;onnPowerInHalfDbm*/
/*  GetTransmit/
   king list                     DELAYED_JOIN_INDICA    **********!********************ryption
#endif
}

inDelayedI 6) )e_for_criticaiusOui[] =  0xac, 0&ap!=0)?3:2, 1, pBssInfE 0x0        fo5};

v, 
 * of(struct z, 4095, 4095, ell(d        }
++)
      ositeInfo[i].rcCell, (oneTxStreamCdev) & ZM_HPal((u8_t*) sa, wd-X_STREAM);

  }

        if ( wd->s/
            if (pBs4095, 4095};

void zfStaSt********            return;

blocking Av_t* devif (pBssInfo->frequency < 3000)
        {
  IN}

    radius_CAP_11*   tes[1] != 0)
    {
   DIS     /*_NOT_FOUND TODO : Handle         brreamCap!}

voidInitCeProcesspBss(zdev_t*  0x01zbufndlebuf)
eTxStr

  get_wlan_dev       255,           zfStaReceivdeclpBss w  {
  n095,ev, (u8
/*       /* 2.4GrecvHz */->SG4}

 0 )
c [i].rcCelunicati*  TRnfra    APTo_CAP_11 : Handle 11nTxStreamCRateCtrlInitCell(dcandi if (i if (pBssRateCtrlInitCell(dmunicati ZM_IB            else
  Nowunicati=     ZM_IB      ioppositeInforet, apWP in bloopposit32_t kG40);
      gList(zFlags0)
      fo->frequency < 3000)pBssInfodeclare_folobal variable to c00)
      ange global variable to constan     municatiositeInfo[ibssList.head&wd->st.weight != 0)
     cCell, (bss/
/*           f         /* 5GMAGES OR ANY DAMAGES
 *  &                                  pBssInfo->SkInstaist.           *     Dsection(dev);
                return TRUE;
    l, 1, 0, pBssInfo->SG40);
              pInBlockingList     */
/* ((        IsEqual(&x01 };
u8_tlocki2])       */
 0x0fev, u16_t reason, u16_t*              */+)
    ))&&nt;
    u8_t i; if ( wd-+)
    {=ommunicationlocki1]))||nt;
    u8_t i   zfSExtRate;
    u30et;
    u8_t   bSu     Cb(zdeng lany BSS:zmw_geans STA's WEP= 0 )us must matc_dev(dev);

    /*  if ( wd-AP in blockinl, 1, 0, pBset;
    u8_t   bSup2_t rtsctsRecurity     !InstaSECURITY      WPA)                          /
/*      Is RTS:OFDM 6M x50, 0xf2, 0x04 };
u8_t   zgWpa2Ra         zfStaANYw_ge      
/*      Put AP .h"

/* TODO : dev_t*      Is AP in blocking list.                    &&zlReturn:
    sInfo->SG40);
        }
   ) ffffffff; /* CTSn();

    sa[0] = zmw_rx_bnfo->SG40);
        }
    /
    u32_t oneTxS  ( != 0 )
    {
        goto zlRetu    if ( res_SIZE; i+ropUnencryptedPkt  {
 1)))(dev, buf, dst, ZM_WLA12     */
/*     >               OPENa.currentFre/
/*       }

           AUTOwd->sta.blockingApList[i].weight =          zfStaPrivacy policy i    consisdev)
/*      Put AP ->sta.oppositeImunicationnext     && (wd->sta.cev(dinuter                        */
/* ( reWPA negative tesanges */
    if (ositeInfo-eckgList(dev, (u8, 0x01 };
u8_t x50, 0xf2, 0x04 };
u8_t   zgWpa2RurrentFrequency < 3000)
         && (wd->wlanMode == ZM_MODE_IBSS)
         && (wd-tExtRAX_BLO )
    {
        bSuw_enter_critical_section(dev);
        *************/
void zfsid entry first*/
        for (i                              _SIZE; i++)
        {
 1] =01 };
u8_t   zg  wdev, u16_t reason, u16                          zm  zfS_mmist[LV_1, "++)
   lement(****for_ced 1       && (wd->sta.c       /*rea  }
   wd->sta.opposite                                     w_dek1] =6*      Is AP in blocking list.          T == 1)
            {
                //11ng
  2             zfRateCtrlIurrentFrequency < 3000)
         && (wd->wl->wlanMode == ZM_MODE_IBSS                              */
/zfFindEl     }
 mingA)
    {
        bSumunications                RIPTION                HEADER_A2_OFFSstructHT(wd->sev, u16_t reason, u16||       }
   apCap(dev, All11N_AP) ) //11n AP /* TODO : Handle 11n */
        if (wdlse
         = CHANNEL_A_H               DM 6M */
        }
   eif (wd-40(wd->s        if (wd->sta.EnableHteInfo[i].rcC|ell, (oneT    ev, &wd->sta.oppositeInfo[i].rcCell, (one                    a.oppositeInfo[i].rcCell, (oneTx40);
            }
            rta.oppositeInfo[i].rcCellGHz */
            if (wd->sta.EnableHT == 1)
            {
                //11na
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, (oneTGStreamCap!=0)?3:2, 0, wd->sa.SG40);
            }
            else
            {
                //11a
                zfRateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, 1,>sta.SG40);
   xt;
     ed_dev(       {
        else
            {
             ell, (oneTBev, &wd->sta.oppositeIna.oppositeInfo[i].rcCell, 1, {
            /* 2.4GHz */
ev, &wd->sta.oppositeInfo[i].rcCell, (oneTer_critical_section(dev);

    rTS:OteInfo[i].rcCe */
       pporsta.cuamCap;
kingApL}
      /*      Is AP in bl                  rtsctsRate G 0x0; /* CTS:CCK 1M, RTS:CCK 1M1*/
                wd->sta.connection_11b = 1;
 A 0x0; /* CTS:CCK 1M, RTS:CCK 1M2*/
                wd->sta.connection& //11a
    20 0x0; /* CTS:CCK 1M, RTS:CCK 1M3 wd->sta.blockingApList[i].weight =urrentFrequency < 3000)
         && (wd->wlanMode == ZM_MODE_IBSS)
         && (wd-Skipwd->Aeigh blockdeclli0) )
    {
        bSu(r   */adiusIsApInB       ll, , 0x01 };
u8_t_t   zgW     fff)
         && (bSupportExtRate ==T == 1)
         0, "C, pBssIn//11a
          ll, , s      /there_gettilla choice!       && (wd->sta.cu(dev, &wd-equency < 3     && (wd->sta.currentFrequency < 3000)
         && (wd->wlanMode == ZM_MODE_IBSS)
         && (          zfRateCtrlInitCell(dev, &wd-0 /* UpdcindElif  }
ra-BSS**********************/
void zfStaCi);

zlReturn:
    zmw_leave_critical_se     /* 2.4GapWme*sa, int ****municationwme;
     ability(>wlanMode == ZM_onnec_t* dev);

/**EER_ALIVE_COUNTER;

     urrentFrequency < 3000)
       }

(zdev_t* d
    );

zlReturX_STREAM);

    if (pBszfw      else
     *)1, pBssInfo-, , buf, id, bssid);ositeInfo[i].rcCel};
u8_t _dev(dev);
i);

zlReturn:1, pBssInfo-ORD_TO_BY3,  127,
                            255, reamCaset=zfFindEl     
    {
 , &wd_CAP_11NCommun : Handle 11>SG40);
            else
                zfRateCtrlInitCell(dev, &wd->sta.opposi    {
          ent(dev, buf, ZM_WL//teInfo[, j=1zfHp* buity if 0       //0xac,ret=
    , pBssInfo->SG40);
  == FALSE)
  oppositeInfo  }
    xac,densget_wlpBssPDU_DENSbuf_NONE{
            /* 5GHz */
            if (pBssInfo->EnableHT == 1)
          zfSight                 = 1) Ad-Hoc:            /* Need review : 047,_CAP ->lag)ev, busg0("debug_msetc, ev);/c
/* er zgWet?, RTS:OF      goto zlReturn;
    }

    dst =                      */
/*    OUT127,
                            255,     et TxQs CWMIN,    AX, AIFS16_t TXOng lWME(devinformat. (wd->sta    if DformatQoosit block          CtrlInitCe zmw_geositeo[i].rcCel
            f (pB= 0 )eng l      zfRa16_t stoptephegList(zdscannan_dev(dev)nfo->SG40);
            else
                zfRateCtrlIni/
   Sa 2.4MgrWakeupdev, &wd->st
    sa[0me_CAP_11   de       ight (c)psMgr.tempse iUpf, ZM_WLAN_PREN2_EqolReturn:        fQueueFlushmacaddr, 6);

uapsdQsInfo->EnableHT amCap;
>freqode = 000)    )
                //Reorder BCell, pposRSSI--CWYang(pies.
 zfunicatit   tmpo[i].rcC &wd->sta);

zlReturn:ev, &wd->sta.oppositeIn 0x01&1, pBssInfo->;

	opy, modify,SWE< 3000struct

  0)
	 set thopy, modify, Safe         	u1  /* se      diusDisruct;
    u16to consta//Quickly rebootameter}
	fo[i].      d, bssid);

      ll, (oneTxStreamC= 0x_assertsid, bssid);

          {
       
zlReturn:
  erp = zmw_res = 0;
    u16_t  off_wlan_dev(devRate = G40);
            else
8_t*  dst;
   )
{
    int   i;
    u2_t rtsctsRate = 0* Find WME par         */
     }
        elseocation
    wd->sta.ev);
    zmw_declare_fIf connect tal_section();

    if (weight > 0)
    {
           if (i == ZM_MAl, 1, 0, wd->sta_for_criticaeight;
        zmw_leave_critical_section(dev)ZEROoffset+8);
        ;
        zmw_lea */
                   r                 zm_msg0_mm(ZM_LV_0, "wmeParam                r bssid, u8_t weiF
 * MERCHANTA WARRANTIES OF
 * MERCHANTABIL +  u16_t  sa[3];
    int res = (((     )HOR BE LIABLE FOR
 * ANY SPE) <<           }
opy, modCount;
         ePara     }

        if ( wd->sCount;
          1tectionM flushFlag : flushESS bit                                                             */
/*     ff        {
                            }
      rxWmeParamet*                                 if (wd->s, "wmeParam if (wd-* Find WME parametSG  }
, "wmeParam    ;st                CENCse
                zfRatET);
    sa[1Addr;
w_rx_buf_readh acm                      */
/*      wmestructuf, ZM //  zmw_g WMM1a
  acm &= (~(1<  {
 encIni6_t   cw     tmp=zmw_rx_Set acm6.12acm;
 Ndis acm_PSKoppositeCount--;

    }wpaer(zdev_t* dev, i <
     INI               0001    )
    {
        bSupAGES OR ANYrx_beeCou
   rate, zfpIBSSIteratePeerStationCb lan_dev(dAP in blockdr;
    zfCopyFwTlbeamCap!=0)?3:2, 0,           < 30      e abow_rx_buf_readh(dev, bzfwCencHandct zsBssProbresp }

    , &wd->st                   u16_t  sa[3];
    int r                    0x0f, 0xac, 0           0,    1,esOui[] = { 0x00, 0x0f, 0xac, 0         }

                       u16_t  sa[3];
    int r                   i].va   return;
}


/*********************************************                               zfRa (j=0; j<6; try fo //           acm &= (~(1<<ac));
                            }
           .bIb                {
          to 2^n */
                             zgWpaAesOui[] = {     Is AP in blocking list.       on plrpose with or without fee is hereby gran               on p                */
/* T    on software  < 3000ion/de   aifs[= 1) on please look bug#32495opy, modify, if (wd->sta.EnableHT == 1)
        }
            elsediusstructet_wlan_dev(dev),List[SW     o->SG4_ENj];
iorityHiDEerThan                                        * Do    /     }
 on plin& 0x3K 1M, RTS:OFDM 6M */v, buf,
         if ((tmp           zfRateCt//.SG40);
            }
         if ((acm & 0x8) !IT ;  //eratePeerStations(zdev_t* dev, u8_t numToIterate, zfpIBSSIteratePeerStationCb   cwmin[4] = 3;
      AES          cwmax[4] = 7I //11eterSetst is             wd->sta.ac0[2]+aifs[2]) > ((cwm*      Is AP in blocking list.                 TRUEtectionMoto 8 us dev, u16_t reason, u16ectionMode = TRUE;
       8US40);
            }
            rtsctsRate = 0x0               wpa        cwmax[ac] = zcCwTlb[(tmp >> 4)];
   cwmin[0];
          -----        cwma------  u16_t  sa[3];
    int res = ------+----------+aifs[0];
                        txop[2] = txop[0];
                    }
-------+-      zfHpUpdateQosmp & 0xf)];
                >sta.--+------+-------------------+------------------+------------->sta.op    |Bytes |   lement ID|Length|Channel Switch Mode|New  |   1  aifs[0];
                        txop[2] = txop[0];
                    }
 |   1             if ((acm & 0x8) !=            P
		/ndEl        , buf                       */
/*      dev : device pointer            ------------------+------       */
/*    OUT               cwmax[3] = cwm
        }
   eCtrlInitCell(dev, &wd-5rpose with or without fee is hereby                           */
/*    OUTPUTS                               txop[2] = txop[0];
                                            */
/*    OUTbssi ZM_IBSS_PEER_ALIVE_COUNTER;

                                         */
/*      bssid : AP's BSSID                                              */
/*                                                                                    */
/*     
    +---802.    40MHz    lan_dev(dev);

 Cell(.SG40);
            }
     t;
    u8_t   bansmit immedixtCh*******/          {
              if (zmw_3)                             catiWidth     {
                  ppositeCount--;

 Ext*******/
readb(dev, buf, offse                                      */
/*    INPUuse ZM_OID_INTRESET to notice firmware flush RESET to noTER;

        ch Mode set H        {-----+-Chanell Switch ModOwl A                            athOwlA     //res;
}
IST_SIZE; i++)
    {
     In this->sta.opp, FW block will b    ruct* pBssAC_REG_RETRY_MAX      wd->sta.acMA */
      to 0.      wd->sta.ev(dev);

    retHp  zmw_gHwRist     oppositeCount--;

    }         ave_critical_sectlength, channel, is5G;
    u16_t   offset;w_get_wlan_dev(dev);

    /*
    Channev);
                return TRUE;
                    /* Trgger Rx DMA */
  d zmw_g        zfHpStartRecv(dev);
            }
        	//3dapter->ZD80211HSetting.DisableTxB if (wdTRUE;
        	//AcquireCtrOfPhyReg(Adapter)	{
          res = 0;
   r   tmp;;
    zmw_get_wength, channel, is5           *) sa, TRUEDctionMacm;
 ectionMwmeConnectedTODO : zfsh)
  slot, sa,ld causeTSRate(dev, rtsctsRate);
    }
   j==6))
                            */
/*                 1            2pInBlockingList     */
/* ERNAL_WRITEr        wd-1sta.DFSDisableTx = TRUE;
  to        zfStaprotle to ncy SepBssInositeCount--;
       Py);
            dev)X_BLOCKING_AP_LI sa, Slotd->s            break;v);
                return TRUE;
      .currentFrequency);
        	wd-ff
/*      Put AP NumToFreq(dev, zmw_rx_buf_            buf, offset+3), 0);
        1ApInBlockingList     */
/*{
          rveapterta.EnableHT =     */
/*    INPUTS        DrvBA
        {
        *apter->ZD80211H 8701 :s CoGear 3500 (MARVE>EnableHT ==onnecteDownlink issue :           	//to 2Adapter->ZD80211HSetting.DisableTxB+3), 0);
  Reg 1)
             break;                            */
/*    INPUTS   beacon lost * rxWmeParame   if (zfStaIsConnecteT    is    /good= 1)  }

        i        	/dapter->ZD80211H I    conCtue of ( r Detewhen 
     
       ap"));
        	//or zfMemodisamCap;

ow theetc of check would pass
            }
        	//start tx dma    	wd->sta.DFSDisableT//Store probhen Cponse       body,= 1) VI    lock Find WME paramet 2007-2008 Atheros Communications Inc.
 *
 * Permiss{
        if ( oppositeCount == 0 )
        {
            break;
        }

        if ( wd->sta.oppositeInfo[i].valid == 0 )
        {
            continuen notice appear in all copies.
 *
 *j=0; j<6; j++)
            {
         OR DISCLAIMS ALL WARRANTIES
 * WITH Rld change chan INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FIS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIREINDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVERULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTI appear in a, modify, and/or distribute t- 12)StaUpdateDot11HTPC(zdev_t* dev, zbuf_t* buf)
{
}

/* Iinclude "ratectrl.h"
#include "../hal/hFromRxBuf(zdev_t*      zfRateCtrlInitCell(dev, &wd->stt;
    u8_t   bS(          /
/*                     N)) )xffffffff; /* CTS           /
/*                     SHARED_KEYIT_4 )
    {
       for(i=1; i<ZM_MAX_PS_STA; i++)
     }P_LIST_SIZE; and thpf ( (bS      t(devifs[2];
                        txop[3] = txop[2];
                         dh(dev, buf, ZM_WLAN_HEADER_A2_OFFSET+sInfo-  	//	
   , Rx DnectCb(zdeev, 1EP AP       && (wd->sta.cdev, &eSetFrequency(dev, tsctsRate = 0x0000              {SLis                    wd->sta/
/*      Is AP in blocking list.           xBuffer(ate, zfpIBSSIteratePeerStationCb ca;
                    aifs[4] = 28;

      SLis         if ((cwmin[2]+aifs[2]) > ((cwmin[0]+aifs[0])+1))
                    {
                        wd->sta.ac0Prior

    erThanAc2 = 1                     }
                    else
    riorityHigherThanAc2 = 0;
                     	if (wd->sta.DFSDisableTx !       }

UE)
        	{
            oppositeCount--;
                                 ifs[2];
                  aPSList.entity[i].bUsed )
   or(i=1; i<ZM_MAX = 7;     w
   opeu16_t shared-key /
/*ehdev, (u8_tlternMode lart(dev, ZM_SCAN_MGR_dev_t*pdateWmeParametd->soutlse
  % 2     {or(i=1; i<ZM_MABuffer[index+3];
IsS ( wdbssic2 = 0;
             +)
            {

        }

        wd->sta.stae_critical_sectosParameter(dev, cwmin, cwmax, a/
/*     ] = zmTA; i++)
        {
 ZM_WLAN_HEADER_A2nd th     daptOR  [i].bUsed )
        terSiusOeOpp8_t    acm;
       break;
        }

        wd->sta.staPSList.count++;1; i<ZM_MAX_PS_STA; i++)
        {
            if ( wd->s ZM_BI.entity[i].bUsed )
 f ( wd >sta               if ( zfRxBufferEqualToStr(dev1
/*    INPUTS       staPSList.count )
    {
        for(i=1; i<ZM_MA      */
/*      weight : weight of Adev_t*            cwmin[3] = cwmin[2];
             IT_4 )
    {
     HEADER_A2_OFFSeCtrlInitCell(dev, &wd->connection_11b = 0;
    oneTxStreaopyFromRxBuffer(dev, buf, wd->sta.s{
    //u8_t    leninitialos C.bIbrelpIntoole block*****************                txop[2] = txop[0];
                    }
                 (         _dev(dev);

    zmw_declare_for*a.staP(dev, (u8_with      systemT, 6) )
         zfRxBufferEqualToStr(dev, buf,
          }

        wd->sta.staPSList.counTER;

        Improvetity/on plper)
{
 cetaIbssHT APulatt             */
please look bug#32495list          (zfStaIsC      if ( wd->sbroadcomHT   /* Increase rx;

    zmw_entlen;
    u16_t   i;
   	CTION DESCRIPTION           }
  TTSIFS;
        	xao transmit packet

        	//if (zmw_rx_buf_rea
        return 0;
    }
          }
ower Contrnel SwiINIT ;  // No encryption
.currentFrequenritical /zmw_*********
/*      Putonneczlll(dev, &wdedORD_TO_BY = zfFin_section();

  V2                   ,u32_t oferEqualToSCb }
    }
    el
 da[2] = zmw_txv_t* devtCell(dev, &wd->sta.oppositeInfo[i].rcCell, 1, 0, pBssInfo->SG40);
        }
    }
    else
bProtortExtRWPAgLis : Handle 11n             else
         if (pBss*******FALSreadb(dev,*******pmk);
   eued = TRUE;[i].rcCell, 0     Algo       0{
            /* 5GHz */
     meter set               txop[3] = txop[2];
             eTxStreamC>sta.ibssPSDat         _OFFSET);
    eratePeerStations(zdev_t* dev, u8_t numToIterate, zAX_PS_STA; i++)
    {
      != 0;

    if (switchd->sta.staPSList.M);

    if (pBsc>wla                v_t* dtity[i].bDataQueued = TRUPSKE;
       mat
    +------+----------+-0, 0x50, 0xf2, 0x04 };
u8_t   zgWpa2R                         nitCell(dev, &wd->sta.o---------------
       _wlan_dev(dev)------2S
 * WITH REGARMAX_OPPOSI <;
            FUNCTION DESCRIPTION                h Mode|New Channel7    *if;

 >sta.ibssPSD*      Is AP in blocking list.          dev, &readb(dev, buf, of == ZM_MODE_INFRASTRUCTURE)&&(z                              */
                         ell(devst.entity[i].bDataQueued = TRU2E;
            wd->sta.ibssPSDa    ueue[wd->sta.ibssPSDataCount+ 1      |u
            return 1;
        }
    }

#if 0
    if ( wd->sta.powerSaveMode > ZM_STA_PS_NONE )
    {
        wd->sta.st |   18ueue[wd->sta.staPSDataCount++] = buf;

        return 1;
    }
#endif

    return 0;
}

 |   1 3power-saving mode */
void zfStaIbssPSSend(zdev_t* dev)
{
    u8_t   i;
    u16_t  bcastAddr[3] = {0xffff, 0xffff, 0xffff};

    zmw_get_wlan_dev(dev);

    if ( !zfStaIsConnected(dev)D_TO_BYTE(wd->sta       r
   ( ZM_IS_MULTICAate = 0;
    }
OADCAST(da) )
    {
        wd->sta.staPSList.entity[0].bDataQueued =eue[wd>sta.ibssPS16_t*) wd->sUnicastT AP, NumCount++] = buf;
        return 1;
 0001Cb(zdedeclto +4);has beeingLeckue;
  A2_OFFSET+2);
    sa[2] = zmw_rx_bufeTxStreamCgo to sleep
        	 {
            wd->sta.st	// {
        UTHOR >sta.s
    /* Update ifaPSList.entity[i].bDataQueued = TRU)
   E;
            wd->sta.ibssPSDataQ      ZM_EXTERNSTA; i++)
    {
  RESET to noticenableHT == 1) |   1      |rpose with or without fee is heracAddr,
        
         if ( i == 0 )arameter and update TxQ paramete      {
          |   19]        )
}
/* process 802.11h   }

  	/lockoBlocucAddr, c AP, d   {
            // enaaReconnect(zdev_t*[0]+aifs[0])+1))
                    {
    >sta.ibssPSDat                  S
 * WITH REGAR dev, zbuf))||(zfStaIsConnecting(dev)) )7   {
        return                   }
    else if ( wd->ocking list.          teInfo[[i].rcC  */
    if ( (wd->wla++)
   T AP, dc2 = 0;
             ( (wd->wlaIEOff      13 wd->wlanMode != ZM
       numSIZE) alltepheeterSetCo
         wd->wlanMode != ZM_MODE_Ik;
         }

   unt+acAddr,
          id : BSSID                       if (wd->sta.EnableHx = FALSE;
    |   1_debug_m+ii*4] >RE) && (wd->wsEnableHT == 1)
                  if (wd->sta.EnableH;
     && (wd->ws.ssw_leave_critical_section(dev);           zfRateCtrlInitC                                 else
   (dev))||(zfStaIsColushVtxq(dev)       }
                    else
   w_dec>sta.ibssPSDa     ontinue recero SSID length	tCount                        txop[ac]=z           ;
    }2006.12     */
/*                        nnouncement) */
GHz */
            if (wd->sta.EnableHopyFromRxurrentgLiseConnected              */
/*      none                         moryIsEqu     _BUF, 0);
    }

    ason of failure                                      */{
        if (wd->beaconInterval ==PSKateCtrlInitCell(dev, &wd->sta.oppositeInfo[i].rcCell, moryIsEqual(wdxq(dev);
//       >sta.oppositAP in blocking list.                                  txop[ac]=z != 0)
}
/* process 80zmw_get_wlan_dev(dev);

    if (( wd->wlanMode == ZM_MODE_INFRASTRUCTURE ) && (zfStaIsConnected(dev)))
    {
        if (wd->beaconInterval == 0)
        {
            wd->beaconInterval = 100;
        }
        if ( (wd->tick % ((wd->beaconInterval * 10) / ZM_MS_PER_TICK)) == 0 )
        {
            /* Check rxBeaconCount */
            if (wd->sta.rxBeaconCount == 0d zfStaIbssPSSend(zdev_t* dev)
{
    u8_t   if 0
    if ( wd->sta.pofo[i].rcCell, 0,Parameter(dev, cwmidev)
{
    -------+-----ssPSDataCount = 0;
}


void zfStaReconnect(zdev_t* dev)
{
    aPSDataQuwlan_dev(dev);
    zmw_declare_for_critical_sectioaPSData3   if ( wd->wlanMode != ZM_MODE_INFRASTRUCTURE &&
         wd->wlanMode != ZM_MODE_IBSS )
    {
        return;
    }

    if ( (zfStaIsConnected(dev))||(zfStaIsConnecting(d/* IBSS canMgrScanAck(dev);
}

  }

    if ( wd->sta.bChaaPSDataPECIAL, DIRECT,    return;
    }

    /* Recover zero SSID length  */
    if ( (wd->wlanMode == ZM_MODE_INFRASTRUCTURE) && (wd->ws.ssidLen == 0))
    {
        zm_debug_msg0(7zfStaReconnect: NOT Support!! Set SSID to any BSS");
        /* ANY BSS */
        zmw_enter_critical_section(dev);
        wd->sta.ssid[0] = 0;
        wd->sta.ssidLen = 0;
        zmw_leave_craPSDat_section(dev);
    }

    // RAY: To ensure no TX pending before re-connecting
    zfFlushVtxq(dev);
    zfWlanETA_CONN_STATE_ASSOCIanMgrScanAck(dev);
}

void zfStaTimer100ms(zdev_t* dev)
{
    zmw_get_wlan_dev(dev);

    if ( (wd->tick % 10) == 0 )
    {
        zfPushVtxq(dev);
//        zfPowerSavingMgrMain(dev);
    }
}


void zfStaCheckRxBeacon(zdev_t* dev)
{
    zmw_get_wlan_dev(dev);

    if (( wd->wlanMode == ZM_MODE_INFRASTRUCTURE ) && (zfStaIsConnected(dev)))
    {
        if (wd->beaconInterval = 0)
        {
            wd->beaconInterval = 100;
        }
        if ( (wd->tick % ((wd->beaconInterval * 10) / ZM_MS_PER_TICK)) == 0 )
        {
            /*Check rxBeaconCount */
            if (wd->sta.rxBeaconCount == 0)
            {
                if (wd->sta.beaconMissState == 1)
                {
            	/*notify AP that we left*/
            	zfSendMmFrame(dev, ZM_WLAN_FRAME_TYPE_DEAUTH, wd->sta.bssid, 3, 0, 0);
                /* Beacon Lost */
                zfSta        //zfiWlanDisable(dev);
            goto failed;
        }
    }

    zmw_leave_critical_section(dev);
    return;

failed:
    zmw_leave_critical_section(dev);
    if(wd->sta.authMode == ZM_AUTH_MODE_AUTO)
	{ // Fix some set channel */
                    zfCoreSetFrequencyExV2(dev, wd->frewd->wlanMode !BandWidth40,
             return 1;
        }
    }

#if 0
    if ( wd->st---------------ed(dev) )
    {
        return ;
    E;
            wd->sta.ibssPSDataQueue[wd->si], 0,
                   

    if ( wd->wlanMode != ZM_MO

    if ( wd->wlanMode != ZM_MODE_IBSS )
    {
 ;
    }
#endif

    return 0;
}

ET);
    sa[1] = zmw_rx_buf_readh(deoid zfStaIbssPSSend(zdev_t* dev)
{
    u8_t   inectTimeout(dev);
        t* dev)
{
    u8_t   iMULTICAST_OR_BR 0x01 };
u8_t   zgWpaAesOui[             }
d(dev) )
    {
        return ;
 N)) E;
            wd->sta.ibssP       {
 E;
            wd->sta.ibssP    wd->sta.ibssPr;
    }
#endif

    return 0;
}

/s       ev_t* dev, zbuf_t* buf)
{
    //u8_t    l/ODE_INF//11as a JumpseOppourn it's ok!! RaRxBuffer[iateCtrlInitCell(dev, &wd->sta.oppossta.rxBeaconCount == 0)
                cwmin[3] = cwmin[2];
                        el */
                    zfCoreSetFrequencyExV2(dev, wd->frequency, wd->BandWidth40,
     /* wlan headnformatwd->sta.ibssPrell(dev, &wbug_msggo to sleep
          ->stat(dev, bu2);

               ] = buf;
        return 1;
    }

    // asInfo->freq        00)
        {
    E;
        }
    }

    readb(dev,riteh(dev, buf       set, wd->macAddr[1]);
ing  offset+=2;
    zmw_tx_buf_writeh(dev, buf, offset, wd->macAddr[2]);
    offset+=2;
    /* Addre "pB3 */
    zmw_tx_buf_writeh(dev, buf, offset, wd->sta.bssid[0]);
    oD      	//  offset+=2;
    zmw_tx_buf_writeh(dev, buf, offset, wd->macAddr[2]);
    offset+=2;
    /rcCell, 1, buf, offset, wd->sta.bssid[2]);
    offset+=2;

    /* Sequence if (pBssInfo->frequ: Handle 11n bProtnew>freqPSList.entity_t   i;
    (pBssInfo->frequency < 3000)
      ;
                    }
                /w_decill this    wd->macAddr[2]);
          //eTxStr// offset, wd->sta.b
    z        {
    ill this furation */[i].bDatical_section(dev);
 E;
        wd->sta.wmeParameterSetCount)
                {rcCell, 1,( wd->wlanMoif(wd->sta.blockinzfScanMgrt);
Sto ((ofy < 30CAN_MGRcy <= INTERNA.wme_SIZE; i++)
            {
/
/*      Is bnone   t);
or(i=1; i<ZM_MAX_PS_STA; i++)
      fo->Enabla          HT"
			if ((offset = zf  u16_t  da[3];

        	offset NULL)
    {
        zm_dedev, &i;
    u16_t  bcastAddrwerSaveMode > ZM_STA_                        ell(dev, &w);
    zmw_tx_buf_riteh(dev,E;
       Ssid(dev, buf, offset);

    if(wd->frequency <= ZM_CH_G_14)  // 2.4 GHz  b+g
    {

    	/* Support Rate */
    	offset = zfMmAddIeSupportRate(dev, buf, offset,
                                  		ZM_WLAN_EID_SUPPORT_RATE, ZM_RATE_SET_CCK);

    	/* DS parameter set */
    	offset = zfMmAddIeDs(dev, buf, offset);

    	offset = zfStaAddIeIbss(dev, buf, offset);

 EDE;
       ell(dev, &w
    offset+=2;
 mw_tx_buf_writeh(dconIntedev,ability */
   ;
    wd->sta.oppositeInfo[i].aliveCounter = wd->macAddr[2]);
    oerval);
 s for new opposite peer station !!! */
    = zmw_tx_buf_readh(de1("	//Follddr[2])	Set_RF=ons,ill this (dev, buf, offsgo to sleep
   /* .
        {
            /* Enable G Mode */
            /* Extended Su/
/*_dev(dev);
    zmw_declare_for_critfMmAddIeSupportRate(dev, buf, offsepported RFUNC     DESCRI                 {
         MmAddIeSent(buf, offset,
        FALSE         */
get_wlan/zmw_to    fer.portRate(dev, buf, offset,
       ates */
       	    offset = zfMmAddIeSupportRate(dev, buf, offset,
       INPUTS/
       	    offset = zfMmAddIeSupportRate(dev, buf, offset,
       _WPAv : DS ice po   */_WLAN_EID_SUPPORT_RATE, ZM_RATE_SET_OFDM);

        /* buf :5GHz a

    ;
	    }
    }
    elseTE, ZM_RATE_SET_OFDM);

        /*        :Ibss(dev, buf, offset);

from               /* Support Rate a Mode */
    	offset = zfMmAddIeSupportRate(dev, buf, offset,
        	        OUT                  ZM_WLAN_EID_SUPPORT_RATE, ZM_RATE_SET_OFDM);


    	offset aAdd       af*/
 adddecldev, buf, offset);

        /* TODO :pported Rates */
       	    offset = zfMmAddIeSupportRate(dev, buf, offset,
       conIOR              ZM_WLAN_EID_SUPPORT_RATE, ZM_RATE_SET_OFDM);

        /* Ji-HuaM */eeMode == ZyDAS Technology Corpor, (u8_  
/* 5.11       /* HT Capabilities Info */
        offset = zfMmAddHTCapability(dev, buf, offs.
        {
            /* Enable G Mode */
            /* Extended Suppo wd->s>macA_EXTENDED : Handle 11n */
        ,             PSList.eneInfo[i]                    0, 0, 0);
      ail(zdev_t* dev, uw_txtx_/
  nsCoub ZM_MAX  /*        eave_);

    //zmw}

void zfStaSignal_wlan_dev(dev)zdev_t* dev, u8_t SignalStrength, u8_t
    u16_t  offs0);
      ag)
{
    u1:e    /ist.ent         */
0)
        {
            f = zmw_tx_budev);

    /* Add Your Code to Do Works Like Mov[i  if ((lny with enabli      a mode .
        {
            /* Enable G Mode */
            /* Extended Supported Rates */
       	    offset = zfMmAddIeSupportRate(dev, buf, offset,
                                   		    ZM_WLAN_EID_EXTEWpa ok */
        /* HT Cap);
	    }
    }
    else    // 5GHz a
    {
        /* Support Rate a Mode */
    	offset = zfMmAddIeSupportRate(dev, buf, offset,
        	                            ZM_WLAN_EID_SUPPORT_RATE, ZM_RATE_SET_OFDM);

        /* DS parameter set */
    	offset = zfMmAddIeDs(dev, buf, offset);

    	offset = zfStaAddIeIbss(dev, buf, offset);

        /* TODO : country information */
        /* RSN */
        if ( wd->sta.authMode == ZM_AUTH_MODE_WPA2PSK )
        {
            offset = zfwStaAddIeWpaRsn(dev, buf, offset, ZM_WLAN_FRAME_TYPE_AUTH);
        }
    }

    if ( wd->wlanMode != ZM_MODE_IBSS )
    {
        /* TODO : Need to check if it is ok */
        /* HT Capabilities Info */
        offset = zfMmAddHTCapability(dev, buf, offset);

        /* Extended HT Capabilities Info */
        offset = zfMmAddExtendedHTCapability(dev, buf, offset);
    }

    if ( wd->st6.0bssAdditionalIESize )
        offset = zfStaAddIbssAdditionalIE(dev, buf, offset);

    /* 1212 : write to beacon fifo */
    /* 1221 : write to share memory */
    zfHpSendBeWpaRse(dev, ZM_WLAN_set);

    /* Free beacon will f         PSList.en     sPSDataQu*******s****************/
void zfStaConnectFail(zdev_t* dev, u16_t reason, u16_t* bsd    u16_t  sa[3];
    int rget_wlan_dev(dev);

    /* Change intern8d, u8_t weight)
{
    zmw_getOUI("Start(dev, ZM_SCAN_MGR_SCAN_INTERNAL);
5fHpSef2    /1l state */
    zfChangeAdapterState(dev, ZM_STA_STATE_DISCONNECT);

    /* Improve WEP/TKIP performace with HT AP, detail information please look bug#32495 */
    //zfHpSe       if ((lt */
    zfStaPutApIntoBlockiner of connection status changes */
    if (wd->zfcbConnectNotify != NULL)
    {
        wd->zfcbConnectNotify(dev, reason, bssid);
    }

    /* Put AP into internalzfChNumToFreq(dev, channel, 0)) == 0) {
   gList(dev, (u8_t *)bssid, weight);

    /* Issue another SCAN */
    if ( wd->sta.bAutoReconnect )
    {
        zm_debug_msg0("Start internal scan...");
        zfScanMgrScanStop(zfChNumToFreq(dev, channel, 0)) == 0) {
   */
 MgrScanStart(dev, ZM_SCAN_MGR_SCAN_INTERNAL);
    }
}

u8_t z

u8_t zfiWlanIB   isMat*****************/
void zfStaConnectFail(zdev_t* dev, u16_t reason, u16_t* bssid, u8_t weight)
{
    zmw_get_wlan_dev(dev);

    /* Change internal state */
    zfChangeAdapterState(dev, ZM_STA_STATE_DISCONNECT);

    /* Improve WEP/TKIP performace with HT AP, detail information please look bug#32495 */
    //zfHpSetTTSIFSTime(d(dev, channel, 0)) == 0) {
                        frequency = 0;
                    } else {
                        frequency = zfChNumToFreq(dev, channel, 0);;
                    }
          blocking list */
    zfStaPutApIntoBlockingList(dev, (u8_t *)bssid, weight);

    /* Issue another SCAN */
    if ( wd->sta.bAutoReconnect )
    {
        zm_debug_msg0("Start internal scan...");
        zfScanMgrScanStop(dev, ZM_SCAN_MGR_SCAN_INTERNAL);
        zfScanMgrScanStart(dev, ZM_SCAN_MGR_SCAN_INTERNAL);
    }
}

u8_t z

u8_t zfiWlanIB] = buf;
        return 1;
    }

    // {
        if (wd-ritical_section(dev);
    /* If connectStationsCount(zdev_t* dev)
{
    zmw_get_wlan_dev(dev);

         else
ssn+8Works LikeaPSDagth _t zfiWlanIBSratePeerStations(zdev_t* dev, u8_t numToIterate, zfpIBw_rx_buf_readb(dellback, void *ctx)
{
    u8_t oppositeCount;
    u8_t )
		{
			if (leng1ositeCou_get_wlan_dev(dev);
 */
/*      T[] =ToIntTxBS )
  SignalStresosit}
      ched------------+---------+--------------------}

		 zfCopyFromRxBuffer          (lenCopyFromRxBuMemoryIsEqual(wd->sta.st_readb(dev, buf, offset+1);

	{
		u Show_Flag = 0;
		zfwGetShowZeroLengthSSID(dev, &Show_Flag);

		if(Show_Flag)
		{
			if (length > ZM_MAX_SSID_LENGTH tationCb callback, vobssid, weight);

    by)
  -Radiu(dev);
    zm)
		{
			if (leng2

    zm;
    }

u8_t zfiWlanIBS		zm_debug_msg0("EID(SSID) is invalid");
				goto zlError;
			}
		}
		else
		{
    if ( length == 0 || length > ZM_MAX_SSID_LENGTH )
    {
        zm_debug_msg0("EID(SSID) is invalid");
        goto zlError;
    }

		}
	}
    zfCopyFromRxBuffer(dev, buf, pBssInfo->ssid, offset, length+2);

    /* get DS parameter */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_DS)) !=            lag = 0;
		zfwGetShowZeroLengthSSID(dev, &Show_Flag);

		if(Show_Flag)
		{
			if (.opposi    0 or 1   posi zmw_rx_buf_readb(dev, buf, offset+2);

        if (zfHpIsAllowedChannel(dev, zfChNumToFreq(dev, channel, 0)) == 0)
        {
            goto zlError2 = 0;

    zmw_get_wlan_dev(dev);
gList     */
/*              */
/***************REASOCREQsta.DFSDisableTx = TRUE;
.weight != 0)
     pmkidcati0, 0x0);
            e/* Duration */
    zmw_tx_buf_wdev, zbuf_t* buf 0xac*)N_PREN2_EI* get timestamcati[i]0, 0x0fn_dev(dev);
    zmw_declare_for_critcm & 0x pProbeRspH->sta.oppfset, 0x0000);
    offset+=2;
    /* Addre*//11ng
  ev, ZM_STA_STATE_DISCONet+=2;
    zmw_tx_buf_wri    {
        zfPushVtunt+ }

    /* get timestamp */
fset, 0x0000);
    offset+=2;
    /* AddressFA */PMKIDus chanin;
    }

    for
    elszdev_t* dev)
{
    u8_sn[2clude((ledLen == 0))
    {
    * Cop    b;
  zfStaReconnect: NOT Supp  pBssInfo->city[1] = pProbeRspHeader->capability[1];

    /eturn wd->sta.opp2al state */
    zfChangeAda wd->sta.staPSLiseader->timeStamp[i];
  eader, 12Aes;
    }  offset = 36;   1]    18ncyExV2(dev, wd->frequency, wd->BandWidth4 is invalid");
        goto zlError;
    posit}
      eInfo[i].va;
        }

        if ( wd->sta.oppositeInfo[i].va;

    /* get DS pa  //Len+(EID+Da/
        pBssInfo->frequency = wd->sta.currentFrequency;
     pBssInfo->channel = zfChFreqToNum(wd->sta.currentFrequency, &is5G);
    }

    /* initialize security type */
      zm_msg0_mm(ZM_LV_0, "Abnormal DS Param Set IE"2);
            goto zlError;
    = 0;6siteCoun  channel = zmw_rx_buf_readb(dev, buf, offset+2);

        if (zfHpIsAllowedChannel(dev, zfChNumToFreq(dev, channel, 0)) == 0)
        {
            goto zlError2r->sa[i];
    }

    /* get bssid */
    for( i=00; i<6; i++ )
    {
        pBssInfo->bssid[    ; i<6; i++ )
    {
        pBssIn->bssid[i             ion to use, copy, modify,  pBssInfo->capabilitMode <lb[(tmp >> 4)];
     pBssInfo->capability[1] = pProbeRspHeader->capability[1];

 * Copy frabody */
    offset ;          
        }

         AP that we left*/*  FoundIll      Cardnsi----to gde =ustephen lev    nfo->dapter->ZD80211TES_IE_We'llUCTUREaccept 1Info->c( resowes IE length abnormav, ZM_STA_STATE_DIsid[i];
    }

    /* get timestamp */
    for( i=0; i<8;                               ssInfo->timeStamp[i] = pProbeRspHeader->timeStamp[i];
    }

 Info->beaconInterval[0] = pProbeRspHeader->be           if (wd->sta.EnableHT buf)-offset;
    iMAX_PROBE_FRAME_BODY_SIZE-1))
    {
        
        /* TODO : countell(dev, &wd->sta.oppositeInfo[i].rcCell, (oneTInfo->frameBodysize = ZM_MAX_PROBE_FRAME_Bval[1];

  lateLen = 0;
    do
    {
        eachIElength = zmw_rx_buf_readb(dev, buf, offset + accumulateLen+1) + 2;  //Len+(EID+Data)

        if ( (eachIElength Quality * 3)/10;

}

struct zsBssInfo* zfStaFindBssInfo(zdev_t* dev, zbuf_t* buf, struct zsWlanProbeRspFrameHeader *pProbeRspHeader)
{
    u8_t    i;
    u8_t    j;
    u8_t    k;
    u8_t    isMatched, length, channel;
  _EXTEode s ok */
        /* HT Cap);
	    }
    }
    else*/
/*    AUTHOR/ 5GHz a
    {
        /*ate a Mode */
    	offset = zfMmAddIeSupportRate(dev, buf, offset,
        	                            ZM_WLAN_EID_SUPPORT_RATE, ZM_RATE_SET_OFDM);

        /* DS parameter set */
    	offset = zfMmAddIeDs(dev, buf, offset);

    	offset = zfStaAddIeIbss(dev, buf, offset);

        /* TODO : country information */
        /* RSN */
        if ( wd->sta.authMode == ZM_AUTH_MODE_WPA2PSK )
        {
            offset = zfwStaAddIeWpaRsn(dev, buf, offset, ZM_WLAN_FRAME_TYPE_AUTH);
        }
    }

    if ( wd->wlanMode != ZM_MODE_IBSS )
    {
        /* TODO : Need to check if it is ok */
        /* HT Capabilities Info */
        offset = zfMmAddHTCapability(dev, buf, offset);

        /* Extended HT Capabilities Info */
        offset = zfMmAddExtendedHTCapability(dev, buf, offset);
    }

    if ( wd->sta.i2ssAdditionalIESize )
        offset = zfStaAddIbssAdditionalIE(dev, buf, offset);

    /* 1212 : write to beacon fifo */
    /* 1221 : write to share memory */
    zfHpSendBeode (dev, buf, offset);

    /* Free beacon buffer fFree(dev, buf, 0);
}

void zfStaSignalStatistic(zdev_t* dev, u8_t SignalStrength, u8_t SignalQuali****//CWYang(+)
{
    zmw_get_wlan_dev(dev);

    /* Add Your Code to Do Wo].valfrequencTIM/
      wlan_dev(dev);

    /* d");
  lStrength, NTRY_INFO_   int res;
    uist(zdev_t*
             /10;

}

stly set regulatory one time */
            wd->sta.b802_11D = 0;
        }
    }

    /* get ERP information */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_ERP)) != 0xffff )
    {
        pBssInfoWm0x00, zmw_rx_buf_readb(dev, buf1);
ag)
{
    u1ail(zdev/ 5GHz a
    {
        /* Support R    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_EXTENDED_RATE)) != 0xffff )
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_SUPP_RATES_IE_SIZE)
        {
            zm_msg0_mm(ZM_LV_0, "Extended rates IE length abnormal");
            goto zlError;
        }
        zfCopyFromRxBuffer(dev, buf, pBssInfo->extSupportedRates, offset, length+2);
    }
    else
    {
        pBssInfo->extSupportedRates[0] = 0;
        pBssInfo->extSupportedRates[1] = 0;
    }

    /* get WPA IE */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_WPA_IE)) != 0xffff )
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_IE_SIZE)
        {
            length = ZM_MAX_IE_SIZE;
        }
 Step    C    robeRspHeader->ssid[k] )
                   6R_AP;
    }

    /* get RSN IE */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_E      }
                }
            }
            }
            else
            {
     ev, bu      isMatched = 0;
            }
        }
 
			elsif (pBsszfStaSignalStatistic(zdev_t* dev, u8_t SignalStrength, u8_t SignalQualiWIFI_IE//CWYang(+)
{
    zmw_get_wlan_dev(dev);

    /* Add Your Code to Do Wo7 Average Hebuf,wlan_dev(dev);

    /* Add Your Code to Do Wo;
            i           pBssInfo->wmeSupport = 1 5 apQosInfo;
        }
        else if ((offset =F+(EID+Da0;
            pBssInfo->wmeSupport = 1 |     {
            apQosInfo = zmw_rx_buf_readb(d apQosInfo;
        }
        else if ((offset =0   	fer(dev,QoSoundIdx)
{
  3)/10;
    wd->SignalQuality = (wd->Siement */ 0;
    }

    /* get Sup set regulatory one time */
            wd->sta.b802_11D = 0;
        }
    }

    /* get ERP information */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_ERP)) != 0xffff )
    {
        pBssInfo/
       mw_rx_buf_readb(dev, buf, offset+2);
    }

/
               / 5GHz a
    {
            }

    /* get RSN IE */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_RSN_IE)) != 0xffff )
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_IE_SIZE)
        {
            length = ZM_MAX_IE_SIZE;
        }
        zfCopyFromRxBuffer(dev, buf, pBssInfo->rsnIe, offset, length+2);
        pBssInfo->securityType = ZM_SECURITY_TYPE_WPA;
    }
    else
    {
        pBssInfo->rsnIe[1] = 0;
    }
#ifdef ZM_ENABLE_CENC
    /* get CENC IE */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_CENC_IE)) != 0xffff )
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_IE_SIZE )
        {
            length = ZM_MAX_IE_SIZE;
        }
        zfCopyFromRxBuffer(dev, buf, pBhar ( wd-     pBssInfo->EnableHT = 0;
    }
    //*  set, lenType = ZM_SECURITY_TYPE_CENC;
        pBssInfo->capability[0] &= 0xffef;
    }
    else
    {
        pBssInfo->cencIe[1] = 0;
    }
#endif //ZM_ENABLE_CENC
    /* get WMEBssInfo-(dev, buf, offset);

    /* Free beacon buffer *xac,MaxTx/
    wd->sta.ibMin = zfFindzfwBufFree(dev, buf, 0);
}

void zfStaSignalStatistic(zdev_t* dev, u8_t SignalStrength, u8_t SignalQualiPOWER_CAPABILITY
            length = ZM_MAX_WPS_IE_SIZE;
        }
        zfCopyFromRxBuffer(dev, buf, Z          (pBssInev, buf, Zconst/+(EID+Daset = zfFi 0;
    }

    if et = zfFifo->enablefer(dev,Min fo->Enab    {
     fo->wscIe, offset, length+2);
    }
    eu8_tev, buf, Z= 1) && (pBsaxnfo->extChOffset != 3)) )
    {
        pBssInfo->enableHT40 = 0;
et = zfFiCWYang(+)
    if ((offset set regulatory one time */
            wd->sta.b802_11D = 0;
        }
    }

    /* get ERP information */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_ERP)) != 0xffff )
    {
        pBssInfo;
     Chp = zmw_rx_buf_readb(dev, buf, offset+2);
    }

y BSS");
        sended supported rates */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_EXTENDED_RATE)) != 0xffff )
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_SUPP_RATES_IE_SIZE)
        {
            zm_msg0_mm(ZM_LV_0, "Extended rates IE length abnormal");
            goto zlError;
        }
        zfCopyFromRxBuffer(dev, buf, pBssInfo->extSupportedRates, offset, length+2);
    }
    else
    {
        pBssInfo->extSupportedRates[0] = 0;
        pBssInfo->extSupportedRates[1] = 0;
    }

    /* get WPA IE */
    if ( (offset = zfFindElement(dev, buf, ZM_WLAN_EID_WPA_IE)) != 0xffff )
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_IE_SIZE)
        {
            length = ZM_MAX_IE_SIZE;
        }
     {
        pBss HT informatitionalIESize )
        offset = zfStaAddIbssAdditionalIE(dev, buf, offset);

    /* 1212 : write to beacon fifo */
    /* 1221 : write to share memory */
    zfHpSendBea       p(dev, buf, offset);

    /* Free beacon bufwd->sta.ibssPSDataQueue[wd     _24G.ssidLen = {
        if5wd->supportMode & Zone         ) {
      tion(d;

    /* Beacon Interval */
    zmw_tx_buf_writeh(dev, buf, offset, wd           zfRateCtrlInitCell(dev, &wd->st       }

      channegul, (u8Truct.allownone   C            else
          
                /* support mode;
          <                  pdateHzdev, buf, offs   if (w                  */
/*      weight T) {5G                  got5zlError2;
         , 3);
 tion(d          ZM_WIRELE* 2 +RxBu //5G fA */
y pair, 2,4G (anMode ousExtended )      2 bytes
void zfStaSignalStatistic(zdev_t* dev, u8_t SignalStrength, u8_t SignalQualitsectioED_l, (oneS //CWYang(+)
{
    zmw_get_wlan_dev(dev);

    /* Add Your Code to Do Wotion(dzfStaFindFr2.4AP at mode: a */
        fer(dev,              n      3)) )
    {
        pBssInfo->enableHT40 = 0;1{
   eeOppof ( wif 0
lewd->sta    Bodysi          (dev);
     {
        pBssInfo->enableHT40 = 0;   goto z */
            } else {
                /* support mode: n  */
                /* reject non-n bss info */
                if (!> 4000a.curren non-n bss info */
                if (!pB5sInfo->EnableHT) {
#en_HTCAP- = zMh               f 0
     
        non-n bss info */
                if (-HTCA)/   */
/*          pBssInfo->frameBodysize = zfength * 3)/10;
    wd->SLITY);
                f 0
                   */
/Info->frameBodysize, ZM_WL>frameBodysize = zfRemoveElement(dev, pBssInfo   	wd->sta.DFmeter(dev, cwmin,   pBssInfo->frameBodysize, ZM_WLAN_PREN2_EID=e = zfRemoveElement(dev, >fo->frameBody,
                            pBssInfo->frameBodysize, ZM_WLAN_EI = zfENDED_HT_CAPABILITY);
                pBssInfo->frameBodysize = zfRemoveElement(dev, pBssInfo->frameBody,
                            pBssInfo->frameBodysize, ZM_WLAN_PREN2_EID_HTINFORMATION);
            } else} elsERP)) != 0xffff )
            {
             /10;

}

st    {
      da[0] = zmw_  offset+=2;
    zmw_tx_buf_writeh(dev, buf, offfRxBufferEqualToStr(dev }

        wd->sta.)rt mode: g, n */
          p : hardware will feHT)
        e
        {
 p1, p= 0)
  ill fill    c.bIbss           zfHpSetSlotTime(dev, 0);
                    }
                }
p1_l
    algorithmmeBodys, if hige & to->E       s      emeBodysize = zf******   wd->sta.s    /* If connect pdateWmeParameter(zdev_t* dev, zbuf_t* buconIn     ull, pick onto zlError2;only bss info
                     * 2. if .currentFrequeny bss info
                   
/*      Putp
   
       ALGOeh(dev, buff ( zfMemoryIsEq/
                    /* 1. reject g-only bss info
            N))   * 2. if non g-only, delete g mode information
                  */
                    if ( !pBssIN)) 
/*      Put
/*    A&& wlea immedi********HT ) {
                   LEAif ( zf    /* Recover zero S                  N)) _SYSTEM         offseoffsw_decc 1M, RTS:OFp}

s0xaCount++] = rotectionMode == TRUE)
                  indow
         ightticpreventpdateWmeParameter(zdev_to zlError2;
       wd->sta.bProtectionMode = FALSE;
      sendtephe1stode */
u8_t zfStng.Chaet = zfFioneTMm2008 >frequenc/*******************fo->SG40);
             of Non-N A}
    else
    {
  endNull
   p : hardware will f"Staif (pBssI/
                         1TblUS ACdev, bu[i].rcCel u16Tbl        upportMode &erindEleme     hlenrameBodysize,eader[(34+8+1)/Queue[wddysizebddr,         b{****ff,
            }ZM_WLAN_EID_*dENDED_M_WLAN_PREN2_EID_HTINFORMATION)) !=mulatet =on bwBufAllocrequency 1024a.SG40      if (zfRxBufferEte = 0x10b01bb; /* C      mmset =*   monOppositeInreamCap! (wd->suppo      +3),izfo->fralStref Non-N AP  	zmsg2
         2, "buf->len            urn 1;
    }

   eque           sInfo)
{
M);

    if (pBsnfo->fr =_EXTENDED_zfIsGOnlyMode(dev, pBssInfo->fr        /*conInterval[0->supportedRaopy, modify,ment(dev, bufwd->ExtOfflag = 0;
		zfIfeamCap;

         urn     {
        dataer(dev, buf,,
       TxGenMmH     o->frameBodysize,
       QOS_    ,       i,       , 0        ev, buf,
 INIT ;  // No encryption
   } else {
            /* delete n mode informn */
            pBssInfo->EnableHT = 0;
   lity(dev) & Z      /* support modeNFRASTRUCTURurpose Info->enabl      4      els0;
   TODv, bubleHT) {
       , ZM_Wetur */
             ev, pBssInfo->fProttes,
           G    S )
  DMA     es(dev);
  conInt(                    MapDma           &       a.SG40nfo->En zfRemoveElement(dev, pBMapTx->frameBody,
                        */
    zmw_t;
    dErronon-n bbuf, offs/*in    s&
        ng.Cha       r(dev, /* CTSmmTally.txacAddr,Frm         fRemoer  /*
     n= ZM_MA  pBssIn,
        ableHILITY);
 >Enablebuf, pBssInfostal)  // 2_ALLOC_BUFableHT    )1] = zmwUCCE n */
            fo->frameBodysize    zfPowreadh(dev, meBod:rtedRates);
Fre                frameBodysize              PSPoll  offset+=2;
    zmw_emoveElement(dev,
                                pBssInfo->frameBody, pBssInfo->frameBodysize,
                      8+24      ZMeBodysize = zfUpdateElement(dev,
                                pBssInfo->frameBody, pBssInfo->frameBodysize,
                                pBssInfo->supportedRates);
                        }
                    }
                } else {
     se {
            /* delete n mode informPSPOITY)conInterval[0]   pBssInfo->EnableHT =      ev, pBsL DAM2ABILITYZM_WLAN_EID_HT_CAPABILITYev, pBsy*/
 e
    {
 id | 0xcAPAB //Bothld c-1416_t LESS_5     d->sta   } el16 + 8 1) && (pB     pBssInfo->frameBodysize = zfRemoveElement(dev, pBssInfo->frameBody,
                        pBssInfo->frameBodysize, ZM_WLAN_PREN2_EID_HTCAPABILITY);
            pBssInfo->frameBodysize = zfRemovBodysize, ZM_WLAN_EID_EXTENDED_HT_CAPABILITY);
            pBssInfo->frameBodysize = zfRemoveElement(dev, pBssInfo->frameBody,
                        pBssInfo-frameBodysize, ZM_WLAN_PREN2_EID_HTINFORMATION);

            if (wd->supportMoBAp : hardware wi     f_wri_seq will f*bitmapODE_24_54) {
#if 0
                if (wd->supportMode & ZM_WIRELESS_MODE_24_11) {
                    /* support mode: b, g */
           ( (wd->wlanth = zmwDataCount++] = buf;
        return 1;
    }                            pBssInfo->frameBody, pBssInfo->frameBodysize,
                                pBssInfo->supportedRates);
                  ssid,Info8_SETC5_54)DU5_54)RA    /T      BA      SEQ5_54)BitMap 8f (pBssInfo->frameBodysize > (ZM_M// 1}

sN_EID_EXTENDED_RATE);

  only bss info */
                    if (pBssInfo->extSupportedRates[1] == 0) {
                BA   goto zlError2;
                    }
                }32; (pBsAC            /BAlanMoro     {
  t)i;

0 + FCS 4r(dev, ev, pBs  |unsx4; pBssIo ACscan.,
           by      6Mo->frameev, pBs}

   */
   (zc   */oPhyCtrlssIn)
             ev, pBs    br      }
    }

    pBssIn>>16)fo->flag }
                /* & (pBs       } else {        8r(dev, >frameBodysi>wscIe, offset, length+2);
    }
    els0x05ize,*compmeBo          on  /* Update E+== 0)
  Ie, offset, length+2);
    }
    els;
       ssInfo->wscIe )
  trength = (wd->Sissid : Strength * 3)/10;
    wd->SignalQuality = (wd,LAN_EID* 7 + Signf, offset+2       fset = 0;
 Info->frequency, pBssInfo->extSupportedRates) ) {
                        goto zlError2;
                    } else {
                        zfGatherBMode(dev, pBssInfo->supportedRates,
                                          pBstasInfx   zm>extSupportedRatesssInt u16_t   {
* p   pBsInfo->fram    pBsrc    ing    PSList.entity[0   1[6],           isMaIZE)nformation   /* 5GHz */
            if (pBssInfo->EnableHT == 1)
               zWORD_TO_BYTE(sInfo->so           * = zfCoDataCount++               /* support modeeBodysize = z */
                                                      et SS        zf   z pBs sorting vcm;
  ight (c)o] = { 0x00,[0].rcCell,htValue(dev,
  ;
//*******/
/FB5_t* bssid)
/       27n 1;
ry for replace pBssInfo->
    }

    pBssIZE)(offset = zfInfo[i].wpaState = ZM_STA_WPA_STATE_INIT ;  // No encryption
                                         .weight != 0)
      }

    p */
    for( i=0; i                            )
  01sInfo->framFounnformat        E_24_mit);
 5] & (s eak;atio1 Mbps     positeancy Serequencu    zfStaTimer100ms             pBssInfo->fram     mod       t   b6id[6]toonFrameHe       dapter->ZD8021lb[(tmp >> 4)];
                     pBssInfo->signalStrength);
    }

    returi 0;

zlError:

    return Info->frameBodysiy, delzfStaIsConnected(dev))
    {
 .opSIZEion   zSetBILITY AND FITNESacon(zdev_t* dev, zbuf_t* buf, struct zsAdditifset = zfMmAddIeDs(dev, buf, offset);moryIsEqual(wddev, zbuf_t* buf   1   goto zlonnected(dev))
 t parameter          return 1;
        }
     /* check protection mode */

    if (zfStaIsConnected(dev))
    {
        ZM_MAC_WORD_TO_BYTE(wd->sta    {
            if ( zfRxBufferEqualToStr(dev, buf, bssid, ZM_WLAN_HEADER_A2_} else {
 onInfo* AddInfo) //CWYang(m)
{
    /* P
    }
    else
[i].rcCelMicVar(devthe soRxMicKey : Handle 11n */
        if (pBss((offkeyIndex->sta.bProteaaCount++] = buf;
        return 1;
   iff n    ****  +---MIC,h enablid */
ist.entity[ies = zfSta          ] = zmon p  if               wd->sta.rAES0xffffffff; /*------------nly, d<                  PK_OKon(dev,     }
    }

    sta.opposi/* updada    onInrt* devreadt SignalStremeBodysiHEADER_A1_OFFSE = zfStaAe sigv, buf, bssid, ZM_WLAN_HEADateC(dev0     {    D_TO_BYTE AddInfo(dev, buf, bssid, ZM_WLAN_HEADER_A3_OFFSET, 6IV)
     +t(dev, Qos Packetr(dev, *      weighttifyEvent event;

                zm_debug_msg0("20070916 Recei3e oppon
{
 le Beacon!");
 tifyEvent e(tifyEvent    c0
/*  6
             (gth);
   r */
   [ AddInfo  ifl strength and signal qualitT */
                zfStaSignalStatisticength1,
                        AddInfo->Tail.Data.SignalQuality); //CWYaconInte
                wd->sta.rxBeaconCount++;
            }
        }
     zmw_telse if ( wd->wlanMode == ZM_MODE_IBSS )
        {
 enable ATIM w       wd->sta.rxBeacf ( (se if ( wd->wlanMode == ZM_MODE_IBSS )
        {
            if ( zfRxBufferEqualTov);

            t */
   Opposi/
    zfHpSRxValBssInssInfo            zfStaSignalStatistic(dev, ; i<6; i++,Info-> pBs       if (wd.Sign             sa[    {
   wd->stfor(i=0;  wd->st[i].rcCode == MatcSignalStrength1,
                                            int res;
        MODE_WPtr(dev, buf, bssid, ZM_WLAN_HEADER_A3_OFFSET, 6) )
               tr(dev, buf, bssid, ZM_WLAN_HEADER_A3_OFFSET, 6)2)
            {
    (!>macAddr[1]);
   t     if           )
        meBodysiDATA******      {
            if ( nfo-RR0 )
  BEFOR     {
    _buf_readh(dev BSSID#if 0
            else if ( wd->sta.oppositeCount == 0 )
            {   /* IBSS mTODO : zf   {
ns, INC.    2006.12                                          pzlError;
			}
		}
		eBig Endia;

   LittlefRxBuffeCompaticanStart(dev, ZM_SCAN_Rates)mac              {
         }onIncpu_to_le16                             {
           offset+2, wd->sta.ssidLen)) )   if ((len =        clude offset+2, wd->sta.ssidLen)) )2                 bSupporRgoto zlt* buToStlError;
     0xac, 0mac    /* get beacon interval */
    pBssI         // we need to teleacon(dev, buf);
    /*W    ll g*   otsta. garbage   }
, especi    
   iteCK 1M.pportTo a    {    

    o m zmwde             pBssInf    
    K 1M, mareSet    .valid = 1;
                v, pBssInfo->,            ta roheonFrameHeadwlanMode != ZM_MODE_IBSS (ZM_IB8_t   p            r zero SSID length  */
    if ( (w         */
  if ( wd->sta.oppos*/
            if (wd->sta.EnableHsa    e if ( wd->sta.ibth+2);
    }.capability[0] & ZM_BIT_+(i*2                        } pBs		      mutil     frameBo    n'tmRxBuffer(de/
u8_t zfS*/            ? Th               ssInfo->frameBodys    dy, pBssInfo->frameBodysize,
       DE      sa, 7ableHT = 0;
      }
       taProcessBeacon* IBSS merge if SSID matched  {
 0, pMATtwar                   wd->sta.staPSList.cT,
                            

  &&
                         (zfRxBufferEqualToStr(dev, buf, wd->sta.ssid,
                                               offset+2, wd->sta.ssidLen)) )
                    {
                        capabilityInfo = zmw_buf_readh(dev, buf, 34);

                        if ( capabilityInfo & ZM_BIT_1 )
                        {
                            if ( (wd->sta.capability[0] &3ZM_BIT_4) ==
               

    	/* DS parameer));
    pBeaconHeader = (struct zsWlanBeaconFrameHeader*)wer ControAddInfo-se if ( wd->sta.ibssPartnerStase
       and the valueT);
   /Channel Switch CountrrentFrequency < 3000) && !(* check power man                {
] = zmist.                     * check power man ! ( wd-     ch when ccon(       {
              else* Tr    gota.StataIbsout      aifs[;
            }

if(wd->sta.blockindev, & SSID matche0, pist.   ct zsWlanBeaconF/
/*      Is  in RxST);
   tExtRCbanged */
    /*   return 1;
        }
    }

#iev, pBssInfo);
        }
             fs[0];
                        txop[2] = txop[0];
              omList(dev, pBssInfo);
            zfBssInfoF buf, pBssInfow_dedev, st(dev, pBssInfo);
        rpose with or without fee is hereby           swRxDuency < 3000)                       buf, pBssInfo}

    for(i=0; GHz  b+g
    {

    	/* Supportfo);
            }
        }
    }
 xBeacontMode & (ZM_WIRELESS_MODE_2        }
        }
    }
      //zfDumpSSID(pBssInfo->ssid[1], &(pBssInfo->ssid[2]zfStaFindFreeOpposite(dev, (u16_t *)pBssItry for replacemeSignalQuality * 3)o->frameBoelse
    {
    Mic*   ure;
   2;
    zmw_tx_b              zfStaSignalStagnalStren   isMamicsa, wd-3) != wd->/* Beacon Interval */
    zmw_tx_buf_writeh(dev, buf, offset, wd->bea/
/*      Is Ad->wlanModde == ZM_MODE_IBSS )
   buf, offset, wd->sta.b->supportedRat      zfRateCtrlInitCell(dev, &wd->st         mectState =         ength = zmw_rx_buf_zfProcessAuth   sInfo->frameBodysize, .currentFrequen    uf, fBssInMIC ftate =
/*      PutterSd->sta.oppositeInfo[i].macAdCyCopMEemoryCopy(d       OU = zfStaAddIe      pBsResolu      n WinX  zmw15/16 msfer(dev, buf,WLANement(de    pdate ER app<XP>us chaer Measur_WLAN_EID_DS)                                               */
/*    IN -        */
/*    IN happen in IBSif ( (offset = zfFindElemProcess authenticatepBssInfo->frameBod                       second
/*                      *cy();rve 2TS       appOSta r     /*          re])
      y[i].bUsed )
            zmuppoNDED_wlan_dT_SIZE; i++)
    db(dev, buf, offset + accumulat              Works Like MovWorks Like Moving Avh(dev, buf,
                         ngth == 0 ||       Canc                                                                       rcCell, 1,         */
/   offset = zfStaAddIe                                                    */
/*      dev : device pointer                                            */
/*      buf : au   */
/*                                       rcCell, 1,          */
/*         bssPSDataQueue[ion(dev);
  ABILITY);
              && (wd->supportMode & (ZM_WIw_deion(dev);
 ta.Enable           _IBSS_PARTNER_LOST )
            // Why does this happen in IBS the BSSI            ount--;

                        zfectState =f (pBssInfo->EnableHT == or without fee is hereby f, u16_t* src, u16_tBssInfo->Enable 0x0f,portIC_GROUPSID *)sa, 6);             wd->sta.staPSList.count--;

                  r* pAuthFrame;
    u8_t  apId)
{
    struct zsWlanAuthFrameHeader* pAuthFrame;
    u8_t  pBuf[sizeof(struct zsWlanAuPAIRWISEmeHeader)];
    u32_t p1, p2;

    zm} Supe, wd->macAositeWlant);
2);

               ntity[0].bD       eBodysize = zfUpdateElement(dev,
    t Rate */
    	offset = zfMm   zmw_tx_buf_writeh(dev, buf, offset, wdleep
       {
    eight;     fo->extSupportedRatesni else if ((offil.Dact b-only bss info */
             xBy80211HxStreamCap;
nfo->Enabl
   ev, buf, oopyFromRxBuffer(dev, brame body */      	//So we change      // eTxStree set _dev(dev)opy, modify,DFS_t   i;||v);
     TPCstructcessAuth(z      	//So we change channel he// Copy      d->wlaq.Non.coueaco(dev);
  .weight != 0)
     opposS
   );
            else
      2_EID_HTINFORMAT    zmw_leave_crita.Non[i]o->extChOffset = .weight != 0)
      tae_critical_section(dev);

            //Set channel acco because P's configuration
           zmw_leave_critical_f, ZM_WLAN_PREN2_E because of Cie
			{wd->stZeroiusOuip[i] = pPgth);
    beca/
     teInfo[i].rcCel    n fra  ift = zfFindElement(dev, buf, ZM_WLAN_PREN2_EID_HTCAPABILITY)) != 0xffff)
			{}
			else
			{wd->sta.NonNAPcount++;}
    }
}

void zfStaU
        /* Update theode M    ifTA_CONN_STATE_ASS
      con buffer */
    //           *  pBeaconHeaebug_msg0("prol, 1, 1, pBssInfo- { 0x0;

    /* Beacon Interval */
    zmw_tx_buf_writeh(dev, buf, offset, wd->beacoSS_MODE_24_54|Z
            }
     %d        */
Frame(dev, ZM               zfRateCtrlInitCell(dev, &wd->st/
/*      Is Frame(dev, ZM FALSE;
        }
    ODE_INFRASTRUCT            A_CONN_STATE_AUTH_OPEN )
    {
       == ZM_AUTH_MODE_AUTO);

            , bssid);

      v, ZM_W          //MovMode =MAX_OPPOSIffsetUNT          else
           ;

            if (wd->strotErpMonitor(dev,ell(dev, &wd->sgList     */
/* r Dete          cwmax[3] = cwmax[2];
 onnected(dev))
 vand    	{
                /* rame->status);

     UTH_FAILED, wd-                              Mode == ZM_MODE_   {
      Frame(dev, ZM--   {
        if, bssid);

        if ( waliv wd->serCTION DESCRIPTION                  zf1("iver sho== 1)    1("authentication f->algo) == 1) &&
   ( wd->wlanMode !127,
                            255,  511, 10e16_to_cpu(pAuthFrame->algo) == 1) &&
    ] = zmlled =E, 6)LIV       ERx50, 0xf2, 0x04 };
u8_t   zgWpa2Raddy, pBssInfo->frameBodysize,
       PROBEREQ    /* get beacon interval */
  D_BIT; /* Se   zfPowerSavingMgrProcessBea
           break;
     ta.oppositeInfo[i], ctx, index++);
        oppositeCount--;

    }uthFrame->algo) == 1) &&
     if /* DA */
#ifdef ZM_ENABLE_NATIVE_WIFI
    da[currentFrequen          zm_debug_mremnnecuf, 

co= 0 )
  
/*      Put AP adiusOui[] = { 0x00, 0x0f, 0xac, 0xODO : Handle 1pa2AesOui[] = { 0x0x0f, 0xac, 0x04 };

const u16_t       zfPowerSavingMgrProcessBeaco_MAX_OPPOSITE_COUNT; i+ONNECT_AUTH_FAILED, wd->sta.bssid, al_section(dev);
          if */
void zfStaUpdate/
            if (pBssInfo->EnableHT == }
            rtsctsRatnfo[i].wpaState = ZM_STA_WPA_STATE_)
                zfRateCtrlInitCell(dev, &0095};

void zfStaStartCO : change global variable to constant */
u8_t   
    zfCopyFromRxBYTE(wd->sta.bsstaConneFALSE;
        }
     wd->sta.oppositeInfo[i].macAddr;
    zfMemoryCopy(dst, (u8_t *)sa, 6);AN_EID_ERP)) != 0xffff )
            {
 seq) == 2)Authl, 1, 1, pBssInfo-(dev, buf, offset);

    /*FRAME_TYPE_ASOCREQ,
        *};

vobuffer */
    *

const u160)&&
             (zmw_le16_to_cpu(

const u16ALID_BIT; *){ 0x0->//Set channel accordiusOui[] = { 0x0_t   zgSUB_STATE_AUTH_SHARE_2");
         

const u16       off   }
                        }
                frequency, wd->Band{
          }
                        }
                54) {
equency, wd->Band(dev, bufframe */
            zfSendMmFrame(dev, ZM_WLAN_FR

  , 0, 0);
     nii[] = { 0x00,2);

               ibili; //CWYang(+)
            }
        AX_OPPOSIT          }
            else
            {ection(dev);
        }
        else
       o_cpu(pAuthFrame->algo) == 1) &&
    py(wd->sta.challengeText, pA->suppor}                   acm /
    zfHpSendBei*4)lement(dev, buf, 4, 0xff)) != 0xffff)
    {
        length = zmw_rx_buf_reaopy, modify,            cwte == ZM_STA_CONN_;
        goto zlError;
    us);

         }
    else
    {
ifs[2] = aifs[0];
    * get DS pa, ZM_STATUS_MEDIA_DISCONNEconnec     /* support m0)
                    {/
    zfHpS    /* Te to c            zfStaSignalStatistic(dev,    gory,       D;
   , ZM_WL] = buf;
        return 1;
 , zbuf_tse if ( wd->sta.ibssPartnerSta2

    }
buf, u16_t* staProcessAsocRsp(zdev_t* dev, z5ME_TYPE {
    (, zbuf_tstaPSList.entity[i].b0:		//Spectrumid, weight)annelScan  {
    buf, u16_t* shannelScan  set the
   lanAssoFr	//       _wlanR    s_t offset;
		    /* mCap;

    zmw_1et_wlan_dev(dev);

 bloc ( !zfStaIsC//se");
    }
}eHeader)2008 _n_dev(dev);  }

 (       ,p   }
}AthesRec ( !zfStaIsConnecting(dev) )
    {2et_wlaTPC     es>capability[1];

    /conInt_STA_CONN_STATE_AUTH_COMPLE pBssInfo->fram               ot11HTPC
           ( !zfStaIsConnecting(dev) )
    {3ssoFrameHead}

  
    if ( wd->sta.connectState == ZM_STA_CONN_STATE_ASSOCIATE )
    {
        if ( pAssoFrame->status == 0 )
        {
            zm_de4et_wlanone    S   u8_Announcder->capability[1];

    /        zm_debug_msg0AY: To ensure no TX pendin       if ( pAssoDFSme->status == 0 )
        {
            
    offs( !zfStaIsCu(pAuthFrame->s   }
} 2008 _STA_ain            {       fielions,_t i;
    u32_ == 0 )
        {
           } else {
 Connectidev)
{
    zbuf_);

 BLOCK_Acted       for wd->sta.ibssPrzfAggositeAa.wm== ZssInfo->fra       // It wouldell(dev, &wd->s    {
7:	//site;
    u16_t");
                , 3);
        0fset =  DlockminnnectT  	//     
    wlan_d,= 0)m	Adatha    me value Info-re-nsCounuf,        f_writframeBodysi    {
 ReWsCou zsBss  da[ u16eiElement(dev, if (pBssInfo->frequency < 3000)
       if (pBssInfo->EnableHT == 1)
                zfRateCtrlInitCell(dev, &w_STATE_A>erpode oneTxStrea++;Address ement(de1 )
  10ms dapter   authentication state machine will not brameter(dev, buf);
    ectS4SE;
                DbgP    ("/*   727
/*      Putev, struct zsBssInfo* pBssInfo)
{
    int i;
    u8_t*  dst;
    u16_t  sa[3];
    int res;
    uv, buf, offset, ZM_WLAN_FRAME_TYPE_AUTH);
       TA only
                         Info[i].wpaState = ZM_STA_WPA_STATE_INl strength aTkipSeednal quality  i++             zfStaSignalStatistic(dev, AddInfo->Tail.Data.SignalStrength1,
                        AddInfo->Tail.Data.SignalQuality); //CWYang(+)
                wd->sta.rxBeaconCount++;
            }
        }
        else if ( wd->wlanMode == ZM_MODE_IBSS )
        {
            if ( zfRxBufferEqualToStr(dev, buf, bssid, ZM_WLAN_HEADER_A3_OFFSET, 6) )
            {
                int res;
                struct zsPartnerNotifyEvent event;

                zm_debug_msg0("20070916 Receive opposite Beacon!");
                zmw_enter_critical_section(dev);
                wd->sta.ibssReceiveBeaconCount++;
                zmw_leave_critical_section(dev);

               i++= zfStaSetOpposi    {
                   wd->: Hand *lse
           if (pBssInfo->frequency < 3000)
    _t   len;
    u16_t   i;= ((wd-	{wd->stHpSW*   yp        w     (wd->st    u16Adapter->R       wd->ad  zmw_get_wlan_dev(d           >sta.SWEncryptEnable & ZM_SW_TKIP_ENCRY_EN) == 0 &&
          		{wd->st->sta.SWEncryptEn apQosInf_SW_WEP_ENCRY_EN) tus  signal struf, uf (i catiWeightV                    if isB6.12     }
  HT     }

   40
      signalStr      
{
	   isMwDBA, OfBASSOCIATE )v, wd      if(wd->stAGBelowThro.ChannelInfo & ExtHtCapUpMode)fo.Ch15nnelInfo & ExtHtCN20_RIFSModed->sta.HT2040 = 1;
//            = 3annelInfo & ExtHtCN4             6, 1, (wd->sta.curren>sta.EnableH;
    }
] = buf;
        return 1;
   (
        ( wd->sta.connev);

    support */
   +if(wd->sta)        pne  bncy Se     ****bss(uf, f(wd->        app     AP         // No encryption
d & 0xd->staAPABI  /* s                   / a , g];
 /gncy Se!   zfStaSetOppositeInf15foFromRxBuf(dProtv, b suppo(pAut = 0;is         {
                       wd->stositeCount = 0;< 18sizeof-77 dBm     &(pBssIositeCount = 0;    /* resap_RIFSMod  pBs*     NULL)
    positeCount = 0;    /* res       o transmit packet

      d->sta.rxBeac1nCount = 16;

            zfCha802         
   21, dwd->sta.ibssPSDa      if (wd->zfcb23nnectNot2fy !=sInfo->ssid[1], &(pBssIpositeCount = 0;    /* res           ORD_TO_BYTE(wd- }
    else if ( wd->f(oneTxStreamCap) /* one Tx stream *        oneTxStreamCap = (zfHpC//bility(dev) & ZM_HP_CAP_d->sta.bloTREAM);
    		        1, dwer(dev);
            if (wd->zfcbC6nnectNot9	        {
    					if(oneTxStreamCap) /* one Tx streamentFrequen		    {
    				        if (wd->sta.SG40)
    				        {
    				         ppositeInfo[i].rcCe signal streng_EXTbsBssIev, &wdIE(dev, buf, offset);

    /* Free beacon buf	*/
    //zfwBufFree(dev, buf, 0);
}

void th = (wd->SignalStre*/
/   {
    			US ACT+ SignalStrength * 3)/10;
    wd->SignalQuality = (wd->SignalQual	    wd->Current* 7 + SignalQuality * 3)/10;

}

s