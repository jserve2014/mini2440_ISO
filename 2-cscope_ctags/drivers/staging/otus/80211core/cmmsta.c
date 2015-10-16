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

*
 * Copyrr if (ight (c) 2007-2 Inc.
 *r * P> ZM_CACHED_FRAMEBODY_SIZE)ion to use, c{pose with or se,  modify, and/or distribute toms software for any * Cpu;pose with or }spose with or opyr(k=0; k<8; k++rpose with or *
 *out fee is hereby granted, provided t[k]CommunicationtimeStamp[k]ce and this permWARE IS PROVDED "AS IS" AND THE AUTH8R DISCLAIMS AL and/oInterval[0S * CWITH REGARWARE INCLUDING ALL IMPLI9D WARRANTIES OF * CMERCHANTAB1LITYING AFITNESS. IN NO EVENT SHALLALL 10D WARRANTIES OcapabilityBIECIAL, DIRECT, INDIRECT, OR CONSEQUENTIA1 DAMAGLIABR ANYATA OR P
 PECIAL, DIRECT, // appear12in a modify, and/or distribute lis hies.
 
 * CLL IONTRAC  NEGSCLAIMS ALs IncRISING OCTION, ARISING OUT OSOFTWARE IS PROVIWARE INCLUDING ALL IMPLIO+12D WARRANTIES O WITH THEIER IY AND FITNESS  TO THISTHIS zmw_e * A_critical_secS AL(dev)
 * WITH REGARDes = zfStaSetOppositeIMS FromBSSIMS cons,ISCLAIMS tant */
u8_t   opy, zgWpa= 0 ies.
 *
 * THE SOFTWARE IS PROVIzfMemoryopyr(event.bssid, (AesOu*)(OR BE LIABL 0x0), 6   zgWpaAesOui[pa2RadiusOui[] = {  zg0peerMacAddrf, 0xact zc01 };aAesOmacaddrWpa2Ae, 0xac, 0x04 D"

/* TODO : chanleaveobal variablee wi, 0xtanN "..ACTION_t  zfwIbssPartnerNotifyTlb[161, &x04 }   zgWpaAesOui[go      nect_done
 * WITH R   31,.h"sion/* IBSS not found */"

/*c, 0xbssNotF*****ies.
 {
#ifdefe abENABLE_****_WPA2PSK"

/* TODu16_t offset ;
#endif"

/* TODc, 0x modify,ibssJoinOnlyx50t zcf2,E SOFTWARE IS Pzm_debug_msg0("*****join only...retrking"oidRadiusOtartCCb(zdList _*/
/onstssion***           , mod5, 40*****     Count<211, 1DESCRIP11, 1         ange gl                        zgWpaAesOui[/
/*    INzf1taPutAp       **, do 0x04survey!!          **ng A* =", INDIRECT,/*      dev : d             */
/*             dev : de++/*      dev ng A7,
/*      dev : deAP's BSSID       Put AP into bloc  */    list.AP's *
 * Copelse: AP's BSSID        of AP                                         *//* Fail     *of AP  TODO create            *iRCHAof AP                       , 0x               *         , 0x0 : AP's BSSID   OUTPUTS }/*      bssid (zfHpIsDfsChannelTlb[16 modfrequency)                                     paRadHpFindFirstNon   non    weight : weight of A> 300    OUTPUTS           */
/*    ws.autoSetF         *t zcTION DESCR{    Autoof AP                                INPUTS  C      Ad Hoc Network B WITons, INh"
#dhocModevice po                              Clea                         2006.12                INC.   R                 0xffP list.                    en Chen ****/
void zfStaPutApId     in         0x0 IN       8_t* b255,  511            CLAIs8_t* or = 1bssid, u8_t// modify,flag8_t*    gingj;/*    0x00, 0xCor    weight : weight of AP        _forobal v] = {        DesiredBlb[1     RUuies.
 *
 * SOFTWARE IS PRF
 * OR IN 6 OR PERFORMANCE OF THIS SOFTWARE.
 */_for_****DI modify,dl variable ude "../hal/hpreg.h

/* TOD                 we modi:   {
   of A#if
   >sta.blockimst    [LATA mw_engApbloc[i].&                  {
!= bssidDATA dr[j]!=      [j]>> 8>sta.blockin{/*      bsclude      break;
1j])/*      bssid : A           3
/*      bssidif(j==->sta.blockin              4   break;
       2==6)
            {
            5       doesn't hav    weight : we      zfGenerateRand      DTlb[16]zcCwTlb     break;      {
f    zgW****/
void zfS#         {
  e       entry f    *//*      b appeir ini<ZM_MAX_BLOCKING_AP_LIST     )0)
  t/*    NumberTlb[16]      break;1d, u 31    63,**** == [j])= ~ZM_BIT_    OUT********/*      b|h*/
/    e  {
      E; i                          zfUpdateable Tlb[16ght == 0)
   * pwlan_dev(datimWindow*****0aKINGmw_get_w*  pur informae wi6; j++)
          E-1);
     <ist ioH_G_14))
   2.4 GHz  b+g/*      b/*     sam  {
/
/*    AUfc.b047,G    , 0xac, 0x04 };,  &&zmw_entupport     & (ZM_WIRELESS_MODE_24_54|sid[j]   1[jc, 0, 0xN))           0x04] = {   _for_criticaResetSa.    RateTlb[16ZM_DEFAULT_SUPPORT_RATE-1);
 AG    }  7                    }+6)
           {
= bssid[j]  {
   =   {
  are_fo_AP_Lmw_
    obal variable to constare_fB

    return;
}


/******}*****                **************************/
/*                       }   breretu               */
/*    AUepStatu          CRY     _WEP_Dj<6; jD511, 1                    modify,, WHETHER I              4                             {
                IST_SIZE-1);
             S PRfuZE-1);
      0x0f,AesOuweipreambleTypeInUsedG_AP_LIList     */
/*          chang                         lPREAM     YPE_LONG                      *LIST_SIZE-1);
                    5                                       ************                                      SHORT    */
/*                                                 ta.bobal var****SetuSCLADesc8_t* bss                              ht == 0)
   and/0411 Adions, INCList[t[i].weito its      */
ISCLA!!!/
/*              0x */
/
      d[L WAsR BECKING_AP_LIST_SIZE-+= 8                and/o i * ANY 6; j++)
    WARE INCLUDING ALL IMPLI       ***********mw_enLE FOR
 * ANY )   {
      t of AP                                                             *               , WHETHER It in blockinght == 0)
 hen Chen        Atheros Communications, INC.    2006.12      OUTPtephen C****v);
    d that theSCLAIMS A                   1             f(w0            ble t2006.12    /if (wduelement iSE : AP no                      _critical_sectsid[LAN_EID_er  _critical_s**********ngthev(dev);
    //zmw_declare_for_critical_sectiter_criti0x0Len_eRCHAobal variablet in blocki/
/*     for(iOR IiGLIGENCE********; iON, ARISING O             en Chen ngApList[i].weight == 0)
   * pu entr)
 [i_critical_se           
{
 
     [5] &             ZE)    */ZM constSET_CCK                (                     brOFDM)&&C     ;
  z=6)
         ight (c) loc SOFTWARE IS PR*/
/*      Stjr inj<6; j             {
            UE;
           /*       bssiT_SIZE)
        zfStaI//****declarll, pick onariable to cist.ve_crible to cons                                         zfStaI_SIZlen
/*    *
/*      Issid, u8_t, 1023***************        */= bssid[j  {
     modi!=             appeCKING_AP_LIST_SIZE; i+           6 }
  (0x1<<i)UE;
 hB    Lis       {
         SOFTWARE IS PROVIv);
    //zmw_declare_for_critical_sect       {
        	;
}

g11taRefTbl[i]    adiusRefBasicres    */
/*<<(7-bloc3,    7,   15,    er_critical      break;
                }*******      :              : device pointer      WARE INCLUDING ALL IMPLI : deviceLISTler          i    }

 break;DS pa Inc             i, lare_fowhole          l bssi  }
    //zmwve_critical_section(dev);
    return FALSE;
}


DS                               *         zfStaIa.blockingApList[i].weight == 0)
  _              zf

        /                                                                  *    	zfCdev);ToNum* pu-1        , NULLOUTPUTS          */
/             *                       .h"
#*****  }
    }
    //zmwve_critical_section(dev);
    return FALSE;
}


                 zf                                                                    */2TION _t* d                   */
/********                                          *      IsApI********    ble to con                          ERP IMS t[i].weiMS AExtenE AU       eions,zs            */ appection(dev);
                     
                              }

      
   **********sApInBl   zmw_enter_criurn(devE       lushFlag)ssidzmw_       break;
t[i].werpE }
    one              //    i********      riticaSCRIPTION                  zfStaIockinn FALSE;
}


  if (ight = 0;
         i     */
/*                  ************************************             if                              }
    }
    zmw_lea/
      ******* modi!= 0 }
               * iare_fori = bssi                  zm}
        }
        /*Thi              opy,j ==aRefres          return TRUE;n AUTHOR              ***/
/*               npList[LOCKING_AP_LIST_SIZE; i+   }
    }
    zmw_leave_/               /*      bssi************************ (flushFlag != 0)
                 return TRUE;
    mw_enter_critical_sectioXTENDED     zfStaConnectFailndle  into t failure       }
        }
    }
    zmw_leav              zfSta                         ***********/
void zfStaPutApIn                                8(j=0;FUN                                 
/*    INPUTS            */
/*               zfStIs       bl                                       */
/*                                                                                     */
/*    OUTPUgList     */ice pointer                                                       }                   t[i].wei********                                	                                        *         flu      ction(dev);
           orFlag : flushRSN : im    aT_SI (wd->sta.binfweighe theNG_Auweigh     Il_sec*********[i].wei**********cal_secti/
voiuth     );
    UTH}

         noif (wd->sta.blockion();
zcCwT WITH    urn FALSE;
}opyr6_t i, j;
 ice pointer   zcCwTl{
 sn[64]                                  and weig*********ID                ason,      *0x30,zmw_leave_v/*      P*    INFLto co*     u16_t* reson, u16_t*  bs14           ght(wd->sta*******Vers                  /* Change inter01,T);
                zfChangeAdapteGroup Cip    Sui zmwdefault=TKIPv(devZM_STAook TE    CONNEC   /tion(/ight-acking/al state                AdaptePairwiseHT AP inftaiweigh*/
ease look bug#32495 */
    //z Commun ImND Te WEP/on pGARDM_MAXce er    tCb(zdepose 0x8 (ZM_MAXify(dplease look bug#32495        //
/* SetTTSIFSTime(d2 Improve WEP/TKIP performace Aut****va****      Key Manag******dev, us 127,ges        opy,ightzfcb*    IN4095, dev)              KING_AP_LIzfg      //f, 0xacypSTATE_DISbs    fStaIsApInBl/*                  nal AUTHOR        

    /* S       Into   */is Communication 
    }

    /* Put AP into internal  intersid)/*  ave_ta.bloTION DESCOverwr    *
 * nnection statuby      ********reason : reason x0f, 0xac, 0rsn+4 { 0x00  3,   ,                  : reason of failure                     AESd->sta.blockingApList[i].weight =rin bION /
/*cbConnectNotify(dev, ungeAE                    ta.blocight (c)opp10 { 0/
/* pListAesOuz             */
/*                  flushFlag != 0)
            {
                wd->sta.blockingApList[RSN_I     zfStaConnectFai**************/
/* *   ***************                         t(zdex = 0= { 0 zmw_ge**************************************ToIter                          numToItSIZE) onnect )
    {
  numToIti+2
/*  =( oppositeC   u8_t index modify,rsnIe,= {  numT[1]+2                ockingApList[i].weie callbacCb caIight--us     A , run tradie wi               */
 Issue                                     */
/*    I    /
/*Pskection(dev);
    }12/
/*      bssid : AP's BSSID           replact_wlan             }
}

u8
{
HT CATA OR Piesif (w
    //zmw
    }

    for(i=0; OUKING  b{   /nt i;9geAdap4C }   break;
      ve_critical_section(dev);
    return FALSE;
}


WPAt index = 0mToItera     }
    }
    zmw_leave_critical_sect);
 HTCap.Data.*******+ 4(dev);

    opposite    Handi < 3sid : BSSID                           > numToIterate )
    {
        oppo  incal_section(d                   OUNT               */
/*       {
     /
/*     0nt == 0ailount          break; i<ZM_MAX_BLOCKING26sEqual< ZM_MAX_OPPOSITcatiion(validwd->sta.oppoterate )
    {
   cALL nue*) sa, wd->staBytList(zdev_t* dev,  AUTHOR        dev);

  t zf********ac, sa,Scan *pF****Idx(wd->staady  numToIterate;
    ady i  int o***********************fMemo((u8_t*) sa, wd//wd->sta.opposit zmw_ge        ockingA <e lot[i]OPPOSITE_C;
        if ( zfMemoryIsEqual((u8_t*) sa, wExtCounter++;
            wd->stdr, 6) )
        {
     IsEqual/wd->sta.oppositeInfo[i].aliveCounter++;
            wd->sta.opposi; i<ZM_MAX_BLOCKING_A numToIterate--;
    }

 EqualadiusOuiIsEqual( 0xac*) alrewd an unused slositeInfo[i]mst u16, 6)ter++;
                  ///wd->sta.oppositeInfo[i]alivsta.oper++;
    }

    f/wd->sta.oppositeInfo[eturn 0;
}2_t opp =e lo****_PEER_ALIVNT )
  teCount;
    u8_t i;
 */
/*      ted, provided that the                               and/or distributStepi above
 * copyright notiies.
 *
 *ction(dev);
                return TRhattepheabove    s hereby gnot     WITH RTER  int opp      */
6 Leid == 0 )
        {
  could ], ct*****, 26; j 4095(u8_t*)  fun             if (     zfStaCo=cking     zfStaConprintk("ThList(zdev_t* dev, 1 = %02x\                         evice pointe       tored */
 =, 0xac)i;
      8_t i;
    u8_0; i<ZM_MAX_     sApInBl*pt[i] OR IN 0)
     OR OTHER TORTIOUS  511, , ARISINGate )
  x = 0;
   t(zdev_t* dnclude "cprecomp.h"
#Revice pointeX_OPPOSITE_zdev_t* dev,                                          0x0f, 0xac, 0x04 };ockin_t zcCwTlbCOUNTER  3,    7,  UNTER;
    wdingApLi
     u8_t *)zcCwTlbblockingApLisnumToIterate+                                        s8_t zfSeInfo[i].camIdx 
/*      Is                     */
/*    AUT_SIZE)
  */
/8_t*) (zdev_t*D         ARTNER_LO            nt++ff                                 X_OPPO          SOFTWARE  */
    zfChang                OUT}

tCbountvv_t*:_OPPO****En    Band/o
        

             LE FOR
 * ANY        ti; i++)
 /* S    */      PRe}
    /0x0f, 0nd zsunicati);mw_geef    dev(BssctCbtht);
appZD1211B HalPlusv_t*bug_mSet*  dst;te )
    {
  0; i<ZMady re    w_leave ut AP*****         r   *moni    

  new stf, 0xIdx = i                                    zfTalreSchedul********** OR Called MONITOR*****TICKddrare_forfMe  dscomeon(dev);

    return index;
}


st      **EP/T    iBSSInf    re-!   }
   No encryplbac     M_HP_CAP_11N_ONE_
        ug#3US_MEDIA_*/
    6; j++)
      == 0)
  X_OPPO         wrapInfo[i**********STAv);
 teCount/*ED}
    /d zfStaInonnPowerInHalfDbm       GetTransmit     OR                     zfRateCDELAYED_JOIN_INDIC*  ds          !     zfStaConnectFai if (pBssInfifListinDelayedIByMACion(dev);
   0f, 0xac, 0      fo&ap!=0)?3:2, 1,mmunicatEsection(   */
5ABLEv,     of(struct z(u8_t*) sa, wdell    ritical_          positeInfo[i]rcCell, (oneTxSt    C //z &e loHPndIdx = i;
    re-X_ST  */still =0; i < ZM_MAX_OPPOSIif (wd->st ZM_MAX_pBs8_t*) sa, a.opp/*      PutteCount;
    8_t i;
    u8;

          o(zde12  tCell(ications            00;
   ;
        INsApInBlo0x0f,xtSuppo    tenfo[dev);
   nfo->ma        /*_NOT_FOUG AL   */
Ha */
/ peer stat    /ap!}a.oppoInitCeProcessG40)SInfo(zdelb[16zbuf */
buf)
      osit************8_t* bssid, u             resceivl_seG40) wnfo->mn_t*)ug_msg0          /*dateGrecvHz */->SG4sApIsta.c e
       CLAIMS *n(denfpositeAPToxtSuppoG40);
    11n        /   zCtrl{
    nfo-iteCicopy, nitCell(streamCap!=0)?3:2, SCLAIMS  oppos ( oppositeCount > NowCLAIMS    }
       zfRati.oppositeInfret, apWns(zdev__OPPOSI32_t kG40   zmw_g  zm_dez if s                     else
    G40);
  l_section(                                                            ockingCLAIMS AositeInfo[bss    .head&ight (********ppositeInf      {
 bss               ight--;
  /* 5G OR PROFITS, WHETHER I*             {
a.oppositeInfo[i].rcCeG40);
    SkI                         D            zfStaI    u8_t i;
    u8_)
    {
  lateCt0CtrlInitC, pB }
        ei].rcCellIn     zf zm_dj=0; j<6; j(  }
     *pFoundI&[16] = {   v_t* 2]   wd->s    UNTE(dev);

    /* Change int  2006.12     ********))&&te;
    AesOui;_MAX_OPPO*******{=DISCLAIMS ALv_t* 1]))||
    u8_t   bS******Ext   z   u8_t30e    u8_t   b  bS      BSSInf   *any+)
 :******ans STA's
   ->staus must matcthere's still sp****MAX_OPPOons(zdev_t* dlReturn:
   */
    u32_t oneTxp    rtsctsRecurityres =!sInfoSECURCIAL_readWPA   wd->sta.oppositeInfo[i].: reason of fRTS:     6M d->sta.blockingApList[i].weight =    {
         ANY = b/*      Steph                     */
 Handle erStations(zdev_t* dev, u8_t numTo&i);
    if (&&zlRa.blo:teInf return 0;
}

int zfSt     ) f

    d;n();CTSFALSE;
}

sa[0c, 0  20rx_b       goto zlReturn;
       zmw_             (?3:2,nt == 0 )
       onnecs != 0 ZM_MAX_Oreswd->sta.bropUn    if edPktnfo->1)))ebug_mbuf,a, 6se loWLA         */
/****>positeCount;
   PENa.currentF    r_critical_
    }

    AUTOmw_enter_critical_section(*********   {
         Privacy policyTS:OF     is //z_enter_critical_d->sta.oppositeSCLAIMS ALnexRxBuf(&&AN */
 (c)cE)
 inu                                   reWPA negatived->sssue another SCANteInfo[i]-eck zm_debug_msg0Tlb[16] = {   w_rx_buf_readh(dev, buf, ZM_WLAN_H   {
         else
                 && (w  wd6.12    X_OP = b****(dev) & ZM_HP_CAP_t6M, [i].went == 0 )
       bSu ( opposiON                  zfStaI->sta.oppositet weight)
{
                if (wd->sta.blocion();

    if (weight > 0)
                  eInfo[i1] =  wd->sta.cING_  u3(dev);

    /* ChangezlReturn;
    }

    dst = m*****_mmssidLV_1, "       et_wlaDESCR/
/* edwd->sta.b   && (wd->wle
       r         < ZM_MAX_OPPOSIT                                     cal_k00)
6es = zfStaFindFreeOpposite(dev, sa, &i);T    1****************WLAN_HEADER_A2_OF/11ness }

static voi zftreamCap!mCap = (zfHpCapability(dev) & ZM_HP_CAP_11N_11N_ONE_TX_STREAM);

                                      zf    Eweight}
 m    dev, buf, ZM_WLAN_ECONNECTION W    else
                             HEADunt 2_OFFS[i].rcHT&& (wd(dev);

    /* Change||lReturn;
   apCapebug_mAllportAP) )
                */
[i].rcCellGR_SCAN_her SCAN *unt > numToIt= * ANNEL_A_        zfRateCtr= zmw if (wd->st     eSCAN */40&& (wdMemoryIsEquaight (c)structH   else
     |   {
     tions,, xStreamta.oppositeInfo[i]      {
    o[i].aliveCounter = ZRateCtrlInitCell(dev, &wd->stTx;
}

int zfStaSet              rzfRateCtrlInitCell(dev, &AP a if (wd->st    else
            {
e
            {
                //11g
     a           //11g
trlInitCell=0)?3:2,           zfRateCtrlInitCell(dev, &wd->stTG      /       zfRat blo (wda. 0;
}

int zfStaSetif ( oppositeCount > numToIterate )
    {
           
        if (wd->sta.currentFrequency < 3000)
        {
            /* 2.4G1,  zfR 0;
}

intxe;
    }edIZE)
+;
            wdount > numToIterate )
    {
       {
     B          zfRateCtrlInioneTxStreamCap!=0)?3:2, 1, w             //ableHT M, RTS< 3000)
        {
            /* 2.4GHz */      if (wd->s           wd->stsa[2lInitCell(dev, RTS:OFDM 6.blod->wlu     ;
               reason of failure  ount;
    u8_t i;
 _OFFSECounGt;
 t = wd->:CCK 1M,  sa[      1RTS:OFDM 6M */
    u32_t i;ectNotify(_11bj;
 ;
 A          }
        }
        e2se
        {
            /* 5GHz */
 &teCtrlInitC20          }
        }
        e3zmw_enter_critical_section(********mCap = (zfHpCapability(dev) & ZM_HP_CAP_11N_ONE_TX_STREAM);

    if ( ((offset = zfSkipa.EnAWEP/dev_t*l_seli0MACAddr(uf, ZM_WLAN_E(teIn*/0x0f,Count   zfRate  {
Tlb[16] = {   [i].weig &wd->ff(dev) & ZM_HP_CHEADbloc6M, RTS ==e
            {
 0, "CCtrlIniteCtrlInitCell(deo[i].r       /ribue= bstilla choice!             zfRateCuy < 3000)
zfHpCapabilOppositeInfo(dev, iCap = (zfHpCapability(dev) & ZM_HP_CAP_11N_ONE_TX_STREAM);

    if ( ((offset = if (wd->sta.currentFrequency < 3000)
0 */
Updcbb; /eigh}
ra-BS      {
                 ght)
{
    ita.Ss != 0 )
    {**********/
/*                   apWme already s***SCLAIMS ALwmid == 0 AMAGES
(11N_ONE_TX_STREACb(zdpBssInfist.   ount = wd->sta.o;
       ction(dev);

    if (rtsctsRate}

SInfo(zded  offs6_t   offsInfo->EnableHTInitCell(zfw 0x00001bb; /* C*)eCtrlInitCo-i].rde & x0f,
}

u8;CtrlInitCell(dev, d->sta.c}
    //zmwu16_t   offsetbssid, ZM_WLORD_TO_BYsection(dev);

    return index;
}


sid,       set=001bb; /* CTSteInfo[i     xtSuppor*/
u16rlInitCell(dn 0;
}

int zfStaSetount > numToIterad->sta.currentFrequency < 3000)
        {
 f ( zfMemoryIs     portMode & IRELE//iteInfo, j=1
/* * bu    if*    OUTP//zcCwTret= (zfR:
    return 0;
}

in= )
   E    .oppositeInf     {
 cCwTdens******0);
PDU_DENSbuf_NONE  //11b
         5    zfRzfRateCtrlInitCell(ication  }
    else
    {
      zfS modi                  1) Ad-Hoc://11b
         Need review : 6; jxtSu ->if (rtModezfSta       zetc,ns, I/c   berNG_Aet? }
   O       ;

    if (Cap!StaIsApInBl, 6);
       */
/*                  OUTERP)) != 0xffff )
            {
      tion t TxQs CWMIN     AX, AIFS     TXO   *WMEy < eason, b.se
      (zfRxBuDson, bQoteIni].pkInstalled  mCap!=0)?3 // CheteInfCell(dev, Time(dev, 0);tCellHEADE

                  stop*****lse
   dscann  }
    //zeturn 0;
}

int zfStaSetZM_BIT_1 )
                {
     tTimeSadateMgrWakeup < 3000)
   ppositeInmepositeIn         *
/*      psMgr.tempse iUp
       AN_PREN2_Eqo != 0 )
-N AP
		/QueueF    0,    1eByM;

uapsdQ              }
TS:CCK      E_TX_      offs           //11g
  Reorder B    {
OPPORSSI--CWYang(N, ARISzf};
u8_t &wd-mpCell(dev       zfd, bssid);
n:          zfRateCtrlInilb[16&bssid, ZM_WL>;

	  zmw_enter_SWEse
   [i].rc.SG40)
	     th  zmw_enter_cSafta.opposit	.blo/*      weix0f,Dis].rc   u8_t16         //Quickly rebootameter}
	nfo[i]       2_OFFSET,ExtRate   {
            /nt++_assert0x0f,t is changed */f ( zfMemoryIt   offset;
 erpo[i].macgWpaRInitCom          *************rtscts ement(dev, buf, ZM_WLAN
     dsget_wl             ********uER_A2_OFFSE if ((0* ht */WME pv, (u                        *   oIMS AL       (wd->ws, INC.    2006.12  _fInnectNoti t  return FALSE;
}

else
 {
   >ositeInfo[i].MemoryIsEquaiX_STREAMAlReturn:
       on(dev);
   *************************/
/*                 ZERO      +8)
         ;
    }

         se
        {
      ate  (wd->sta.EnableHT   zfS_mmist[LV_/* CwmePara************ount ch, 0x0f,        t i;
    u8_t* THOR BE LIABLE FOR
 * ANY SIL +rameter esa[3);

    res =   re* dev, )    BE LI     FOR55,  NY SPE) <<)
             zmw_enerate;
    }

  meParfor(i=0; i < ZM_MAX_OPPOSIerate;
    }

   1t, 1, wM       if  :8+(i*4ESS b[5];
t : weight of AP                                           *       */ight--;
             //11g
1)
            {
   rxWwmeParae       goto zlReturn;
    }

    dselse
    sta.wmeParamelse
  if ((len = zmw> 5)SGev, sta.wmeParamete;               zfRCENC_BIT_1 )
               E    zmw_sa[1 u16;
macAdduf_readh acameterSetCount =                   me[i].rc;
    OF C // Ch WMMrlIni    &= (~(1<10+i*encIniter e cualToStmp=].macAdSet        acm;
 Ndbssicm_PSK          break;
 TRUE)wpaerTE(wd->sta    i <Time(dINint i;***********0      dev, buf, ZM_WLAN_EpOR PROFITS,cAddedev) TRUIZE), zfp****siteCouP  continue;
 T_SIZE)
 ons(zdev_t*st, (u8_t opyrF   /         if (wd->st1);
        3    OUT== 0  }
           portModzfwCenc);
 rcCelan_ProbresprtExtRat        z         {
        eter and update TxQ par         {
         UNTER;
    wdwmin[ac] = zbuf,
1,*    INPUTS                     {
      tExtRate = 0;
    }

                    if ((acm & 0x4) != 0)
     [i].aiWlanIBSSGetPeerSt           txop[2] = txop[0];
             goto zlReturn;
    }

    dst = wRaTHOR                   ounter = Z        tmp<ac)StaInitCommonOpposi1)
            {
        .bIis fu                        to 2^ev, &wd->sta.urn;
    }

    dst = 0x00 3,    7,   1erStations(zdev_t* dev, u8_t numTosid);ies.
 *
 * THE SOFTWARE IS PROVIDED "AS           aifs[sid)IBSS)
         && (wd-     y(deove
 * clse
   ion/kListaifs[     sid);
    }

    /* Put   zmw_enter_c        }
    }
    else
    {
    if ( oppositeCountx0f,[i].rc****************,bssidSa.SG40urn 0;_EN=0; io
   HiDEerTh                     (zdev_t* dev, u16_t fluDStaIs      }
      in& 0x3     }
   2] = zmw*/tMode &Time(dev, o->S(tm           ctrlInitC//leHT == 1)
            {
      rThan   {
 0x8) !IT ;nModlb[(tmp >> 4)];
O : Handle    /*ta.c oppositeCouac] = zcCwTlb[(tmp >> 4)];
   db(dmin[4c, 03;
    }
AE    }
     cwmaxta.wme7IteCtrWME Set      eCount;
    u32_t i;ac0[2]+8;

     > ((cwmes = zfStaFindFreeOpposite(dev, sa, &i);
    if)
  len >= (nnec8 us
      ;

    /* Changenel, is5v_t* )
    {
     8US       }
            rtsctsRate =            (dev, &wd-
       pEADER_A2_rocessacfo[i]
    /[nAc2 >> 4)0; i<Zwd->st00; i<ZM_MAX  ---------+------+------                 if ((acmametemat|El+mat|Elode|N8;

 +-------------rn index;
}


sxop[2c, 0+----r|Channel Switch Count|
}
Channel ---+
  
/*    if QosmpsParaf------- {
        if  zfRe|New ChaNew Channel  1       1   |	     1       1   |	     1   zfRatthe leCous     et_wlanID|Lion(d|none    Switch wlan|New +----1 28;

 r|Channel Switch Count|
    +------+----------+------+-----------------------t+8);
         teQosParamete           {
P
		/b; /* CTS }
  buight--;
            }
                          */
/*                 |       1          |                     ount--;
        ocess3c, 0cwm>sta.SG40);
   rentFrequency < 3000)
5ies.
 *
 * THE SOFTWARE IS PROVIDED      {
                wd->sta.bloc                 el Switch Count|
    +------+----------+------+------------- )
    {
        //zm_debug_msg0("EI i<ZMoppositeCount = wd->sta.o;
        **********************************************                             **********************************************yFromRxBuffer(dev, buf, pBssInfo->supportedRates, offset, length+2);

        dr[j] != bs     ---802r set40MP ad           >sta.SG?3:2,leHT == 1)
            {
  /
    u32_t one->Enab immedixtChmw_leave           return TRUE;
  o->S  203   wd->sta.oppositeInfo[i].aliIMS Wid(dev);
+i*4);
                    /* Convert Exo[i].rcC/
    bportMode &      x_buf_readb(dev, buf, offset+1);
        //zfCoen Cuse1) =OID_INTRESET.oppo      firm
 * co(i*4 .DFSDisable  zm_debug_msg-------6_t  treamCap!{       Notiel------------Owl *    AUTfo[i].aliveCounter = ZthOwl*  dst// = zf}
                       
   n     index = 0, FWi].pkInwill is fu].rc*ll, 1AC_REG_RETRYOPPOction */
void zfMA "wmeParamto 0r set c          d->sta.SG40);etHp       HwRomRxBuf(        /* Convert to 2ounter = Z***/
/*         tion(d,nfo[i     is5GE parameter e       ; Check if there's still sp*
 *    E_REGfStaInitCommonOppositeInfo(dev, i);
z           zfRate* Trgger Rx D  }
    d                H in rtRec                              	//3wrappr->ZD80211HS  zfng.Dis    TxB > ((cw_buf_readb(dev	//AcquirmentOfPhyReg( wrappr)	            Find WME parm & tmp;***************205_WRITE_REGISTER(            ;
    )
  Dn >= (      en >= (wme*    INedo->SG40zfshf_t*slot,
   ld causeTS    y < 30 Element F zfStaIsA   j==6)f_t* buf)
{
    u1
/*                                  |unsigned 2positeInfoFromRxBuf(zdev_tERNAL_WRITEm & 0x4) wd-1 zfRDFSe channel_dev(dev);
BLOCKING_A     prot        elSe             break;
       Py    	//if Chann disi].weight == 0)

    Slot            r station
      }

        if (zmw_rx_buf_readb(dsection(dev);

      	//if C	wd-ff_enter_critical_Noppo (zfy < 30].macAdduf    }, ZM_WLANop dma,
 t+3), 
}

int zfSt1.oppoteInfoFromRxBuf(zdev_, STA should vent > 
    }
    elj=0; j<6; j++)
       RateCtrrvBA            z      & = 0 , STA shou 8701 :t thG* OR3500 (MARVE        }
  unt, anDownlink issue     {
      	//];
 ount > , STA should change channel frequency);Reg          {
    tation
then restart rx dma but not tx dma
      vent  2007- lo_MGR = (tmp >> 5, offset******ount, annnounency S/good     T == 1)
     << 6; // 2t = 0 , STA shou int iconCtue    ( r Detew****             iap"            	/ountadiusOdisTS:CCK 
owtephetcConnehex DMif ( pass  	//if Channel Switch Cous Swi tx dm     to f |   lowChannel(//St_secprob*****pon
        body,     CKING .pkIn                ted, provided that theCONNECTION WITH THE USEPermiss/*      fluqual((u8_t*) sa, wd->sta.oppoot for new peer station
    for(i=0; i < ZM_MAX_OPPOSITE_COUNT; i++)
    {
        if ( wd->sta.oppositeInfo[i].valin         
 * OR IN lCTION, ARISING ction(dev);
                return TRU_LISTRRANTIES O    OR BE LI "../hal/h/Chao[i], PSCht* dLUD  /*vingIMPLIEAUTHOR BE LIABLE FOR
 * ANY S* WHATSOEVERINDIRECT, OR CONSEQUENTI                     acm = 0xf;
AN
 * ACTIv, &TION OOR CONSEQUENTIAi].ad     zfRateCtrlInitCellWHATSOEVERULT  /*FROM LOS8_t  USE,      ORE.
 FITS, WHEzmw_gIN A255,  511ol */
void zmw_enter_cteCount == 0 )
    - 12)Sta   if Dot11HTPC, txop);
     */
 v_t*   if{Listy !=in}

   "IZE)ctrl****#FSET, 6) ../hal/h 0x5RxBuf : Handle d->sta.currentFrequency < 3000)
   /
    u32_t oneTdev, rtscts                         N)) )x

    dst = wd->HEADER_A2_OFFSET+2);
X_PS_STA; i++)
SHARED_KEYIT_4[(tmp & 0xf)];
 ount =1ngApList[i]PS3000rentFrequenc}  if (wd->sta WITH puenc(b     foode oateDot;
    }

    for(i=0; i < +---- ( (o+------;
    }

    for(i=0; i < Z  offset+;
       AN_*/
           ET+icationt(Ad     ,x2 )
  INBSSInf    1EP                zfRateC < 300e    (zfHpCapy < 30Element Format
                 {SLency Selection 
            /ratePeerStations(zdev_t* dev, u8_t numTota.bPBuffer(           }
        }
    } //ifca;
    }

    for(i=0; 8;

 a.wme28hanged */SET, 6) )
   ntegewd->staUpdateDot11HDFS(zd---+- Number|)+1h when continue recevin+i*4);
                   */
void zfSPriosInfo
       Ac2j;
   ***************************canMgrScanStartunt > nu.ent;
   gh.bDataQueuedInitCommonOSTA )
       	else
       lowChannel(demonOppos}

U                        //          break;
                                           }

            /aPSET,t.entityion(b    [(tmp sta.staPSList.en02.1 packew    Zpe  offsshared-keyit */ehbug_msg0("lnStowlan_larode on" looCAN_MGRIZE)_t*  if (tmp >> 5)_Modoutunt > %       {sta.staPSList.e     f[index+pdatIsSX_OPP
}

i == ZM_MAX_PS_STA )
***************** i<ZM_MAX_BLOCKING_AP_LIST_Ssta**/
/*         o, 40f WME y < 30     STA; ax,           fo[i].i].bUsed )
   = TRUE(dev, buf,
      WITH RxBuffapt                    checkerS0f, eOpp    */
      	//zfHpAddAlwd->sta.staPSList.count )
    {
      cnew o+;taPSList.entity[i].bUsed )
   = TRUE;
         MAX_OPPOSIist.BI                    AX_OPPs |                   if          fFoundToStX_PS_1evice pointer                wd->st[(tmp & 0xf)];
  >sta.staPSList.e                            */
/*    trl = zmw_rx--+------inf ( (offs     ;
    }

    fo           if ( !w*/
           ment(dev, buf, ZM_WLAN_> 5GHz */
        InitCom          c]=z    fram  foortMode & unt )
         //     */
leninitial * P    rel;
            ];
                    }
            |Value |   37     |  3   |       0 or AtimWindow(dev);dev, rtsctthere's still sptical_section(d*      ebug_msg0(*
 * st.entystemTeByMACAddr(zdev    {
                   ac0PriorityHighe                             wd->s  zm_debug_msg= NULL)    /sid);per(wd->ceta047,nnectulat            }
*/
);
    }

    /* Put               
         < ZM_MAX_OPPOSIbroadcomHnnouy !=n    se rx
/* IBSS poenD120dapter,CR24, 0******	                      */
       blockietectEvent(xaoack( CCA hpackei;
  tEvent(Adffset+2)
         f ZM_ENABd zfStaInitComleAtimWindow}

   /*  tr-------INr(dev, c);

    if (pBssection(dev);

al vari itica         _enter_critiCb(zdzlncy < 3000)ed ZM_WLAN_paRadFinreturn FALSE;
}V       zfRateC-+-----uf, dst           Cbal_section(); of da---+--  20txop);
   eInfo[i].rcCell, (oneTxStreamCap!=0)?3:2, 1, wrn:
    return 0;
}

int zfStx_buf_readh(dese
bProt      rWPA zm_rlInitCell(devdev, buf, ZM_WLAN_EID_HT_Co->SG40)       kingquene and 
       mk   zmweued_dev(devell(dev, &wd  {
  AlgStaIsApI0             zfHpSetSlotTime(d WME 6_t         /* check source address */
                     / |   rcCePSDa}

    //           zmw_wmin, cwmax, aifs, txop);
                }
       entity[i].bUsed )
          ev);_rx_buf_reas-----                w  if (zfRxBufferc11N_AtimWindow(dev);op);
          
   a.Non>sta.ibPSKf_readb(demazfFind     |  1   |	    +-] = c->sta.blockingApList[i].weight =Buffer(dev, buf, wd->sta.requency < 3000)
      1   |	     1   f ZM_ENA**************  1   2e "../hal/hpregPPOSITE_CO <;
    }

    f wd->sta.oppositeInfo[i].valid =[i].v---------+none   7  if ifert  i++)
    {
es = zfStaFindFreeOpposite(dev, sa, &i); < 300quene and stop dmaX_STREAM);

 NFRASTRUCTURE)&&(ev(dev);

    zmw_declare                break;
            }cy <               wd->sta.ibssPSDa2f_readb(dev, buF_Mode, 
    {
  zmw_gue[wlanMode != ZM_M   zew onge chan|   cwtatic void zfSta    , 4);
#endif

 **** 0 (zfRxBuf>wlanModep
   Savewlan_his sok bPS     [(tmp & 0xf)];
  unt )
    ------8IBSS )
    {
       return ;+

   usavini=0; i<ZM_MAX_PS_STA;}Cap!=0)?wd->sta.bloc0pList-------3PSLis-sav    mzfHpt* buf)
{
   047, SSend, txop);
   (wd->sta.t[i].wff)
    eter ebcast u16urce a{eInff_t zc->sta.staPSLABLE    // Check if there's still spel;
 !        	//{
  dd disM_WLAN_TE       }
   zmw_fresh1) ==S_M{
  CA       = TRUE)
OADCAST(da   zfRateCtrlInitC                 w       0      return ;
BSS )
nMode != ZM6_t* )M,
   ULAIMstnectioNum           zfSenMmFrame(dev, ZM_WLAzcCwBSSInfl_seto +4);has beefoFrecklid ==          2          f, 0);
   	zfCo         /goa.cusleepf ZM_ENABLTRUE;
                   	//TRUE;
                 ction();   if  if,
                 wd->sta.ibssPSDaf_t* 

    if ( wd->wlanMode != ZM_MtaQo to sZM_EXTERN             (u8_t.DFSDisableTx ! }
    else
  1      |uns|ies.
 *
 * THE SOFTWARE IS PROVIpposite       0,d->sta.staPSLi    HEADEi<ZM_MAX     u->sta.TxQ                           -----9         )
}A3_Op   /*  e se11ssPStExtR	/v_t*     upositeBcection= TRUE;
         // ena    Cb(zdeSInfo(zdwd->sta.staPSList.entity[i].bUsed = TRUE;
  i++)
    {
        if A; i++)
   "../hal/hpreg        ZM0xff
        	//{
 ingd dis) ) pow          ta.bloc*************************    ( wd->sta                      iteInfou16_t   another SCAN_CAP_11N_       nectioni == ZM_MAX_PS_STA )
RASTRUCTURIEO offset+1posite1N_ONE_TX!STRE       0n      ) zfS*****amic FrCcan...         Reconnect: NOTAM);

                   ew oaReconnect(zdev_flusi++)
    {
        if+8);
         
            {
dev,kingApLtion();

        +ii*4] >RE)_HP_CAP_11s  }
    else
    {
        6M */
        }
    }
          

    // R.ss            {
                  if (wd->sta.currentFrequyFromRxBuffer(dev, buf, pBssInfo- }
     hannel
    if ( wd(i*4Vtxy);
    wd->sleAtimWindow(dev);
{
    zmw_gcal_snMode != ZM_MODE_ .opposi recero      tion(d	t sa, witch Announcement) not foundac]=ev(dev);

  = TRUE) */
/************************************nnoundFreeO)ddr,etSlotTime(dev, 0);
   
            {
}
       ction( zm_count, and           wd->sta.blockingApList[i].w   wd->beaconInt    *pFou      BUFquency);
 tExtRatson,astAUTS    x_buf_readb(dev, buf, offset+1);
       ting.Channel;
nd q 2007-ERCHANTA ==PSKcurrentFrequency < 3000)
        {
            /* 2.4G    *pFoundIwdf ( (wd;
           zfRateCtrlFindFreeOpposite(dev, sa, &i);
    if ( zfStaCheckRxBeacon(zdev_tev);
 >wlanMode != ZMsed )
            {
            d->sta1N_ONE_TX_STREAM);

 ffff, 0xffff,  }

    // Send ATIM to preh when )) == 0 )
        {
            /* 2, 0, pBs= TRUE;
             {
            /* 10InitCommonOpfMemoryIsEqual     tick % (     {
            /* 10) /     teCoRCopy())apter->UtilityChannel = Adapte= wd(AdaprxB2007- sa, wRUCTURE ) && (zfStaIsConnec     wd->sta.baCon                            0, 0, 0);
            if ( wd->sta.staPStCell(dev, &wd0, i<ZM_MAX_PS_STA; i  0, 0, 0);1          |       return       ist[
    int res   {
        re   0, 0, 0);{
     Qu*********************cal_section(dev);
    return {
     w_le( wd->sta BSS */
        zmw_e, wd->sta.bssi&&eAtimWindo    }
            wd->sta     if ( zfMemoryIsreamCap!da[3];

         0, 0);
               
    if ( wd->sta.bCha********canMgrSctaQukd disabtExtR
{
    zmw_gight (c) Cha{
     N AN
 * ACTION Out(zdev_t* dev)
{
    /*     v    ingMgrMain(dev) ZM_MODE_INFRASTRUCTUR_FRAME_TYPE_DEAUTH, wd->sta.bss }

    // R.}

uLentaConn if ( zfMemoryIs          zfSt7         }
   : NOT        !!sa, MgrMaitorigh+)
 "    	//if C/m = 0x    otTime(dev,if ( opposiED_RATE)) != 0xffff)
                  r (info[iInitCommonO_STA_CONN_STf ( (UTH_SHARE_2)**********/
{
                 zfStaIsApInBl// RAY: T

  s((wdno TX pp!=0ng be 0,  re-ectNotif     buf_NAPco if ( (wd, (u8_t WlanET      bug#324ASSOCI();

    if ( wd->wlanoppositeInng lr100ms, txop);
    zfChangeAdapte           {
                  wd->sta    d->sta.opponnectState fP_CONNECT_TIMEO          zf/
   S    bMgrMai      zfStaIsA;
                 R    wd-         }
         /
            	zfSendMmFrame(dev, ZM_WLAN_FRAME_TYPE_DEAUTH, wd->sta.bssid, 3, 0, 0);
                /* Beacon Lost */
                zfStaonnectFail(dev, ZM_STATUS_MEDIA_DISCONNECT_BEACON_MISS,
                        wd->sta.bssid, 0);
                }
                else
                {
                   wd->sta.beaconMissState = 1;
                    /* Re        {
                //11g
fStaIsConnec 2007-Missin bTX_STe no TX pending bef            //	/*leTxf zmw tvaliwe lef  if (wd->st           Mm2008   {
     v, bu for _     DE            }
, 0x0f,3 wd->sStaInitCommonOpposi/*    wd- Let+3)e
        {
            ZM_MODE_Izfi    e chann        	//if ChannonnecUTS edS_STA; i++)
    {
 CTION DESCRIPTION                  zfStaIreamCap!=(wd->wt;
    u8_t    erp;
    u8_          zfStaI, u8/
void zuthNE_TX_STREA    AM);

    )
	{{
  Fix somHpReseTE_REGI "wmeParameterSetCount op[acr             ExV2   }
 v);
fre    }
        catiZM_OI40
/*      bssid ZM_MAX_PS_STA; i++)
    {
        if ( wd->sta.s1   |	     1    to preckConnectTimeout(zdev_t    zmw ZM_EXTERNAL_ALLOC_BUF, 0);
    }
IBSS )
   i]->Ba
    }

    for(i=0;e != ZM_MODE_IN BSS */
        zoffset, seq;

    zmw_get_wlan_dfStaCheckConnectTWLAN_FRAME_TYPE_ATIM,
                        fo[i].macAdd        off                              0, 0, 0);
         INng louode o    	//if C   }
         
              ST_OR_BR
    wd->sta.caifs[2];
     [3] = cwmin[2];fAggScanAndClear(dev, wd->tick);
 ZM_

    if ( wd->wlanMode != ZilityChanntrol */
    zmw_tx_buf_writeh(dewlanMode != Zt, (u8_RAME_TYPE_ATIM,
           /ncy Sele             ZM_WLAN_HEADE     }

       /->sta.reCtrls a JuABIL[] =tickit's ok!! Ra {
     [icurrentFrequency < 3000)
        {
_AUTO)
	{ // Fix some AP not send au>sta.staPSList.entity[i].bDataQueued )t* dev)
{
   r100ms(dev);
        zfStaCheckConnectTimeout(dev);
             ;
    MgrMain(dev);
    /*   wd oneTason, b;
    /* Duratency < 3000     zf}

    for(i=0; i<wd sta.cuode on");.bDaeAtimWindow(dev)               0, 0, 0);
   E) )
    {
  a);
        n[ac] = zcal_section();

 f_readb(dev ZM_MODE_AP &quene and riteerEqualToSList.ent 1, v);
Opposit[1]ffseng 0x0);
 +=2**********t of bewuf, offset, wdev, wd->d[0]);
    off2et+=      zmw_tx_buf_wrCONNddre "pB3GR_SCAN_Iteh(dev, buf, offset, wd->sta.bssid[1])wd->sta.bstaPt+=2;
        )
  
    zmw_tx_buf_et, wd->sta.bssid[2]);
    offset+=2;

 ;
    offset+=2;
    zmw_tx_buf_wa[2] = zmw_    offset+=2;

    /* Sequfset+=2;
    zmw_tx__buf_wriSf_writ zero G40);
         lInitCell(devM_IS_new     
                       }24-31 Time Stamp   else
           
            //zfDisableAtimWindow(dev);/cal_sA */     +=2;
  ;
    offset+=2;
M_MODE_I      //offset+=2;

    /*_AP && */
          rval);
  fur bssid*/i], 0,
                     f_readb(devv);
    .wmeParay BSS");
unt6)
            {
  ?3:2, 1, weq;

    zmw(dev);
    ev_t* dzf    Mgrt);
Sto ((ofelse
       / els= I   zNAd->sa.currentFrequency <il(dev: reason of fb         ifsta.staPSList.entity[i].bUsed )
        strucbDataQueuedHT"
		{
   (       aRad }
      d updatf ZM_ENABL       oReconnect )
    {
       < 300      }
            elsList.entity[i].bDataQ    if ( !zfStaIsConnected(dev)    
          (dev, buf, offsetf_readb(deSsito pr    zmw_tx_bu          ev);
        elsSTREAC      / {
  ate AP address an( wd->s	et+=            ck(zdevZM_RATE_aRadMmAddIe4_N)) )witch Counwd->sta.bssi
    }

    for(i=0; i < ZM_MAX_OPP		ATUS_MED   /_section(deE     ;
  _  break offset+DE_2 wholan_dev(d     Only accompany with enablDsc.bIbssGMode
            ccompany witSET+nabl047,c.bIbssGMode
         EDf_readb(dezfStaAddIeI+=2;
    zmw_tx_bteh(dev, buf, offs       .bIbAMAGES
set);
        a, u8_t *pFoundIdx)
{
    u32_t oppomer
  ;
    offset+=2;
 HANTAce nsEER_As !=ppositeIn

codev, pose!    tTime(0);
    of beacon */
1("	//Foll  offse	;
  RF=    rval);
  fset, wd->sta.b}

    for(i=0;/* .t */
            ifbuf_wristruct G: zfHpck(zdev_t* dev)* En;
    zmSudev,       {
                wd->sta.beth enabling a mode .
        {
         ed R BSSateCtrlpositity[i].bUsed = TRUE;
      h enabli mod    {
      	    /* EkingAn_dev(dev);
ailCntOfiticaBLOCKIfer. a mode .
        {
      	    /* a   +ck(zdev_t*     company with enabling a mode .
        {
      	    /* en Che   	offset = zfMmAddIeSupportRate(dev, buf, offset,
        	                DS     */ if (wd->erpElement = 0;
       	    offs     PORT_RATE, /LAN_H :SetSlaORT_RA;
     endif

    if ( dIeDs(dev, buf, offset);

    	offsion */
:/
        if ( wd->sta.aufrT; i++)
    16_t  sa[_N)) )    //aMode */
     ccompany with enabling a mode .
        {
      	    /* E          Announcement) */
  wd->   	offset = zfMmAddIeDs(dev, buf, offset); )
        {
 RSNe a Modefode addl_se.bIbssGMode
            ffset+4)   */,
       e */
    	offset = zfMmAddIeSupportRate(dev, buf, offset,
        	            ****************H);
        }
    }

    if ( wd->wlanMode != .authMode =Ji-Hua>staentity[_STRyDAS Technology Corporf, 0xa    St5._t weight/*ev_tC DAMAGESie WITfo*/
         company with ena== 0  ( (wd->.bIbssGMode
  
        {
            /* Enable G Mode */
            /* Extended Supppof, off  }

>sta.    rlInitCell(dev, &wd->sta.              
        teInfo[i & 0x4) != 0)
       d, 2);
    retusiteInfop);
      buf,tx_tTimck, vbAX_OPPO            *****ectNotifyiticeasso )
     Signal************** txop);
          
    z    05_WRI
    parameter elems

void zfSt (wd->sta.b:wd->b/       n_dev(dev);
2;
    /* Address-N AP
		//ZM_WIRELESScal_section();
   You_HEAd     Do Works Like Mov[id )
   lny *
 * PSDattect byacastAd  /* 1212 : write to beacon fifo */
    /* 1221 : write to share memory ** HT Capabilities Info */
        offset = zfMmAddHTCapability(dev, buf, offset); ERP Information */
       	  _AUTH);
        sta.Wpa ok*/
         ionalIESi)ss(dev, buf, offset);

   {
  zfStaAdil(dev, ZM_STAe == ZM_AUTH_MODE_WPA2PSK )
        {
            offset = zfwStaAddIeWpaRsn(dev, buf, offset, Zr( i=0; i<wd->sta.bsd HT Capabilities Info */
        offset = zfMmAddExtenderp(dev, buf, offset);
       	}

       	/* TODO : country information */
        /* RSN */
        if ( wd->sta.aut is ok */
        if     eason, bssidet, frequency;    ck(zdev_t* M_MODE_INFRAS    }

        zfStaCheck        al_section();

    zzfStaAddIbssAddw/* RSN *WpaRs cons    zmw_tx_buf_ATUS_MEDIA_DISCONNE     buf, 4);
#endif

 v(dev);

    zmw_declare_for_critical_section();o->bssid[j] != pP         (Adapif A hi bufd[j] )
        alIESize )
        offset = zfStaAddIbssAdditionalIE(dev, buf, offset);

  k if it is ok */
xtended SalIESize )
        offset = zfStaAddIbssAdditionxtended        for( k=1; k<pProbeRspHeaddev)
{
    zmw_g, offs6.0bssAd++)
onalIEros Cf_t* buf)
*/
        /* RSN {
             k=1; k<pProbeRspHeader->s/* 1212    uf, 
			uf, offfi offset =     }2Info         f ( w musOuiGR_SCAN_INrnalndB      , ZM_STATUS_MED              Free    }
  MA */ight--;
  
        st.ent  else
    */
/*v_t* dev, zbuf_t* buf)
{
    zmw_gettaSignalStatistic(;

    /* Change inter   {
eter and update TxQ par    	//ReleaseDoNotSleep(A
/* IgockinSto8 Improve WEP/TKIP performace OUI("l Swi  {
            //     4)  // L);
5   {
f     /1v, 0x8);

    /* Notify wrapp call, ZM_STATU    if (wd->zfcbConnectNotify != NULL)
    {
        wd->zft(zdev_t* dev)
{
  eason, bssid);
    }

    /* Put AP into internaligned integelMGR_SCAN_INTERNAL);
        zfid *ctx)
{
    u8_t op/* Issue another SCAN */
    if ( wd->sta.bAutoReconnect )
    {
         if ( wd->sta.h Countt internais chan...");
        zfScanMgrScanStop(    frequency);
   TE_REGIST0   else)       zm_debug_msg0(" *), 0x0f, WEP/TKctNotify !=ount anoan_de    ZM_MODE_INFRAight (c)         }
   ckConnectTimeout(          zfStal SwiScanStop(dif (...e == ZM_STA_set);

      Stop(       } else {
                    frequenode  ( ((freq_WLAN_EID_DS)) != 0xffff) {
         ctByReasslan_de->frequedev(de   zfisMao[i].rcCe;
        }

        /* Check channel */
        /* Add check channeleParameterSet    zfChangeAdapte     if (isMatched) {
            if ev, 0x8);

    /* Notify wrappbuf, offset+1)) == 1) {
                    channel = zmw_rx_buf_readb(dev, buf, offset+2);
                    if (zfHpIsAllowedChannel(dev,  blocking lis {
                    frequenstruct zsBssInfo* pBs        el= ZM_MAX_PS_STA )
       }over z    struct zsBssInfo* pBssInfo, struct       } else {
              er TBTTSE;
                break;
 ev_t* dev, u8GR_SCAN_INTERNAL);
        zfcy = 0;
                }
            } else {
                frequency = wd->sta.currentFrequency;
            }

            if (frequency != 0) {
                if ( ((frequen     || ((frequency < 3000) && (pBssI******* if ( ((freq         || ((frequency < 3000) && (pBssInfo->frequency < 3000)) ) {_tx_buf_writeh(dev, buf, offset, wd->macA)) == 0 )
       ON                  zfStaIy !=nnectNoticallback, vo          }
         // Check if there's still sp{
        ssn+8SignalQual{
   )
  < 3000)) ) {Smin, cwmax, aifs, txop);
                }
            }
          e anllback, oppos*ct
        
     numToIterate;
    
    )
		{        tion1wd->sta.*********************       */
/Tac, ToIntTxBeckCond Your CodsteIn        .opp    1           |      1   |	     1       -}

		op[ac]=z           0))
    {
 lenength+2);

 

    *pFoundI          }
		}
		e if (pProbeRsp+    
TH )
u Show_)+4))>sta.		zfwGetxfffZero---+--    y < 300xffff )
 D_DS)	if(dev, buf, NGTH )
    {
   th[i].bDt[i]    _LENGTH ntinue;
   lse
		{
       }
            } byf_t*- 0x0f*************  if ( length != 2E_AP && 0);;
    y < 3000)) ) {S		          zfStaEID(    )RspHin.alive ==				;

    ErrorzfHpI}
dev, zf ( ZGTH  ZM_MAX_Otion(de_get_||nnel, 0)
        {
          cy;
            }

            fset+2);

        if (zfFSET, 6);

    Channelfset, wdev, NG_AP_LIength+2);

    /ev, buf, ZM                ta.bssidtion(d].bDa Show_Flgetk pBssInfo = "M_MODE_INFRAScompany wit1bb; /      EqualToStr(dev, bu   /DS)ned integer    |)
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
        if ( length_debug_msg00i, j = Fbug_mVE_WIFI
    dane and stop dma,
 t{
        f, offset/*   Allowednone   );
          } else {
                    fck SSID */
        if ((dev, channe == ZM_i].bUsed )
            {
   FromRxBuf(zdev_t//zmw_declare_for_critical_sectiREASOCREQteAllowChannel(dev, wd->sCap!=0)?3:2, 0, pBspmkidIMS ] = cwnt(dev, buf, ZM_eck ability[0]uf_writeh(dev, bu       ZM_WLAN_H******)ff)
			{}I* DS pL WAstamIMS [i]] = cwm        {
                wd->sta.beQosPara p    eRspHindex = 0.bssidZM_WLAet+=2;
    zmw_tx_buf_writeh(d*         ease look bug#32495 */
w_tx_buf_writeh(dev, buf,ntOfReasso > 2 )
     rn ;
  if ( !zfer->timeStamp )
 pProbeRspHeader->beaconInterval[0];
    pBssF }
 PMKIDfrequenit* dev)
{
    foleep
              0, 0, 0);
  sn[2}

  ((leif ( (wd->sta.connectS Copy tranl, 0_STATE_AUTH_OPEN)||
    ell, 1, 0, pc    ximumeaconIntereadchecY DAMAGES
[1PPORT_RA/   u8_t index = 02ev, 0x8);

    /* Notify wrM,
              BodysizL WARRANTIii].bDBodys, 1  3,l, 0); Matched = 036canM1/zfwB18 2 */
    zmw_tx_buf_writeh(dev, buf, offsequency = zfChNumToFreq(dev, channel, 0);PPOSI        teInfo[i].a
    for(i=0; i < ZM_MAX_OPPOSITE_COUNT; i++)
    {
       /* DS paramealitLen+(EID+DatTime(dev,G40);
            elmer
    ifection(dev);

  hNumToFariable IE
taTimer, is5G         ritical_section(dev);

  ter s5G 0);;
           f ( wd-os CsT);
    typ */
                     wd->staAbn_MAXlk pBeParamwd->IE".bDataQur[i] = pProbeRspHeadl, 0);>sta6d->sta.o   zodysize)   pBssInfo->securityType = ZM_SECURITY_TYPE_NONE;

    /* get macaddr */
    for( i=0; i<6; i++ )
    {
        pBssInfo->macaddr[i] = pProbeRspHeaderEnab-1))
      if ( !zfDS p      )
    {ount i=0ingAp6 entry                          * Sequ{
     n;

    /* get supported rates/
    ifandle 11n */
 ithout fee is hemw_enter_csInfo->frameE(dev, bwlan_<------+------------      zm_debug_msg0("  pBssInfo->frameBodysize = zfwBufGetSize Copyr frannelysize =         i].valid == 0 )
   tExtRate = 0TimeoutCount++;
	*  ored *lheck thCardnsid, oto gdev,us*******ly < 30eturnt = 0 , STA shoTES_IE_We'llxffff,accept 1ble IE
ta.cuow    Ennel, 0)aength)ase look bug#32495LAN_Ee);
    pBssInfo->franfo->capabilie = accumulaoffs8 packet

        	//if (zmw_r.enticationBODY_SIZE-1)ssInfo->frameBodysizBODY_SIZE-1))
   || lees */
 
            ATE_AUnfo->frameBodysizbwd->beaconIn        }
    }
    e   }-x0);
  (dev, .entiROBEDIA_DISany
 * pu-List.ent          nfo->bssid[j] != pProbeRency < 3000)
        {
            /* 2.4GHz */;
      08 Athesos Comist.entiNFO_SIZE)
  engtetSize(late(wd->sta.conndcan...te = 0x0000achIEnel, 0))     {
            zm_msg0_mm(ZM_L + accumn 0; che+1) + 2ev, ceachIElengtta)0; i < ZM_MAX_O(        if (wQuaIeWpaR 3)/1sa[i}

[i].rcCetreamCap*// Cop                           ZM_WLAN_H, ly set re    aconInte2008 eBodys *nfo->frameBodys"Allocate beaco       }
 (offselare_foindEleme       ( (offset    .opplse
             AX_Cu16_tzfHpRoffset, frequency;
    struct zsBssInfo* pBssInf                  zmw_get_wlan_dev(dev);(pBssInfo = wd->sta.bssList.head) == NULL)
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
                 a.i2
                        isMatched = 0;
                        break;
                    }
                }
            }
            }
            else
            {
    ty =  break;
                        }
     b    /*f    y = wd->staader//CWYang(+)
{
    zcallbstic, txop);
          d Your Code to Do Wongth > 3);
 ortEx/[5];
  +dev, &Show_Flag);

		if(Show_Flag)
	->SignalQuality = (wd->Sii].albuf_writTIMtTime(deAX_WPS_IE_SIZE;
       zfChNumr Code to DNrtReINFO off res = zfSN_EIT"
			alStaeAtimWindow(de    /* onlyly offsregn 0;se
 >staL WA*/
            /ight (c) 802_11Duct zsAddition
            fo->frashFleader->bssid[j] )
 /
        pBssInfo->frequency = wd->sta.currentFreqERPncy;
 staPSL /* get supported rates *Wm          {
            zm_msg0    re */
    wdtaSignal   zmw_get_wlan_dev(dev);

    if ( pBssInfo->apCap |= ZM_SuperG_AP;
    }

    /* get XR IdBeacon 	   */
    if ((offset = zfFindXR  if (wd->sta.b802_11D)
        {
        EID_Dng before re        {
       UPP 	   rmal"* purck SSID */
        if (              wd->staxtended SIZE) zfCopyFromRxBufferle == ZM_STA_u16_t)eachIElength;
  isableAtimWind
        pBssInfo->channel = channel;

extanProbeRsCapab }
    else
    {
   dif

    if ( Z get supported rates */
rityType = ZM_SECATE_AUTH_SHARE_2)nfo->rsnIe[1] = 0;
    }
#ifximumdMmFrame(VATE)) != 0x.bIbIE found */
        pBssInfo->frequency = wd->sta.currentFreqWPA_Ibuf, ZM_WLAN_EID_RSN_IE)) != 0xffff )
    {
        length = zmw_rx_buf_readb(dev, buf, offset+1);
      h > ZM_MAX_IE_SIZE)
        {
   if (wd- length = ZM_MS_STA; i++)
 ****aptereturn->frameBodysizN_STAk]uf_t* buf)
{
    u1    R_AP);
    pBssInfo->fra    dev, buf, ZM_WLAN_EID_CENC_IE)) != 0xffff )
    {
                  break;
       ***********************/
/* if ( oppositeCount > numToIterate )
  , buf,YPE_NON!= 0xfffuct zsAdditionInfo          }
 fHpIelso->SG40)db(dev, buf, offset+1);
        if (length > ZM_MAX_WPS_IE_SIZE )
     WIFI_IE
            length = ZM_MAX_WPS_IE_SIZE;
        }
        zfCopyFromR7 Average H    ,AX_WPS_IE_SIZE;
        }
        zfCopyFromR;
    }

    fandle 11n */          wmling a mj;
  5 apQoicatiS_STA; i++)
    Recover zero      pBssFhIElengtZM_MAX_PS_STA )       else if ((offset =    {
            //fFindWifiwd->sta.b802_11D)
   zfFindWifiElement(dev, buf, 2, 0)) != 0xffff)
    {
	fo->chanQoSred */
            /   offseIZE )
     tcumu= zfFSiFreeOppo( (offset = zfFindElemSupG IE */
    if ((offset = zfFindSuperGElement(dev, buf, ZM_WLAN_EID_VENDOR_PRIVATE)) != 0xffff)
    {
        pBssInfo->apCap |= ZM_SuperG_AP;
    }

    /* get XR IE */
    if ((offset = zfFindXRElement(tTime(dev  pBssInfo->securityType = ZM_SECURIT 0xffffe
        {
        zmw_get_wlan_dev(devpe = ZM_SECURITY_TYPE_CENC;
        pBssInfo->capability[0] &= 0xffef;
    }
   ID_ = wd zmw_rx_buf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_IE_SIZE )
        {
            length = ZM_MAX_IE_SIZE;
        }
        zfCopyFromRxBuffer(dev, buf, pnIe, offset, length+2);
        pBssInfo->secu>sta.oRITY_TYPE_WPA;
    }
   t found");
   +accumul*/
/zfCopyw_rx_bufSCONNEWPA }
    else
    {
        pBssInfo->rsnIe>sta.  if ( (offset ******wd->s       acmVATE)) != 0x acm(dev, buf, ZM_WLAN_EID_CENC_IE)) != 0xffff )
    {
        len acm       pBssInfo->SG40 = 1;
        }
        else
        {
            pBssInfo->SG40 = 0;
        }
    }
    elheck SSID */
        if (lement(dev, buf, ZM_WLAN_PREN2_EID_HTCAPABILITY)) != 0xffff)
    {
       hary = wdeHT = 1;
      ncrease rxB_readh(dev, b*/
/bssid[lenfo->apCap |= ZM_All11N_A acm_ENABLE_CENC
    /*      goto z0]    staPe        else
    {
        pBssInfo->rsnIecrx_b+2) & 0x02)
           //           pBssInfo->enablWME        lement(dev, buf, 4, 0xff)) != 0xffff)
    {
   *cCwTMaxTx



  wlanMode !Mid->snfo->fzfwBu     length = zmw_rx_buf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_WPS_IE_SIZE )
     POWERxtSueCtrl;
    }
        else
        {
WPgth > ZM_ pBssInfo->rsnIe, offset, length+2);
        pBss device p;

    /*ChOffset =     /hIElengt pBssInfo-( (offset = zfFif    buf, ZsnIe[tructfo->chanev, o->Enablil(dev, ZM_ elsesc*/
        pBssInfo->Enable* RecovindEChOffset =        &pBsaxrsnIe[1]ChO      != 3elSc{
        pBssInfo->rsnIe[ }
    40= 0;
 o->extChO          rame(dev,        IE */
    if ((offset = zfFindSuperGElement(dev, buf, ZM_WLAN_EID_VENDOR_PRIVATE)) != 0xffff)
    {
        pBssInfo->apCap |= ZM_SuperG_AP;
    }

    /* get XR IE */
    if ((offset = zfFindXRElement(db(dev,Ch     /* FleHT40 = 1;
        }
        else
     tState == ZM_STA_s    zmsnProbeRspSIZE;
    pBssInfo->apCap |= ZM_SuperG_AP;
    }

    /* get XR Iement(dev, buf, ZM_WLAN_EID_RSN_IE)) != 0xffff )
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
        zfCopyFromRxBuffer(dev, buf, p get supported rat )
 eason, bs                   isMatched = 0;
                        break;
                    }
                }
            }
            }
            else
            {
    mw_get_wffset =(dev, buf, 4, 0xff)) != 0xffff)
    {taSendBeacon(zdev_t* dev)
    /*24G     (wd->sting.Channel5, off      wlan_& Z>sta.opposit      stru to co       /* fMmStaT          
    {
        pBsf, offset, wd->sta.bssid[1  if (wd->sta.currentFrequency < 3000)
   <ZM_MAX_BLOCKING else
gu {
 u8T].rc.a get       d zfStaTimer1 }
      alid == 0 )
       ATE)) !_MODE_5castA;
    }

               bleHT =  if HIe[1it for suppffset+1)*****************************db(dev,T) {5id : AP's BSSID    got5RspHeade;
    }

  , 3else to cod->sta.bssList].add* 2 + {
  //5G f }
  y pair, 2,4G (    }

ousxtended S   wd->2 b    uf_readb(dev, buf, offset+1);
        if (length > ZM_MAX_WPS_IE_SIZE )
     timerChED_ {
    S 
            length = ZM_MAX_WPS_IE_SIZE;
        }
        zfCopyFromR to coone time Fr2.4AP a      : , "wmeParametfo->chann( wd->wlanMode         {
        if (zfHpIsAllowedChannel(dev, pB1q(deve_writ wd->Frequl  zfPSET, 6)o->cou 0;
    }

      zm_de   if (zfHpIsAllowedChannel(dev, pB= pProbeR*/
            /o, u8_t type)
{
    u8_t fo */
         :meBock(zdev_t* dev)
{
 /* rejquennon-nameB

                       el;
!> 4000n + eachBssInfo->frameBodysize, ZM_WLAN_PREN2_EIpB5Info->EnableHT freq#en_HTCAP->extMdev);
    for (i     if  = ZM_MAX_ssInfo->frameBodysize, ZM_WLAN_PREN2_E-pBss)mw_r               G40);
      Info->countryIzf!= 1 )        /wmeSupporECIA}

int zfStaSetOp           ; i<6; i++ )
    {               pBssHeader-          pBssInfo-Removerequency = wd        r->RF_Mode, 1)M_MAX_PS_STA; i++)TY);
                pBss != 0xffff)
			{}ID=M_WLAN_PREN2_EID_HTINFORM>              	    /* ERP Information */
     /* support mode: none */
     EIInfo-ent(deHTt = zmw_rx_ment(dev, pBssInfo-);
                pBssInfo-_PREN2_EID_HTINFORMATION);
}
        }
    } else {
        if (wd->supportMode & ZM_WIRELESS_MODE_24         _HT   {RMA11, == 1)
               thingIE */
    if ((offset   {
                //1    /* onlyil(dev, ZM_SdeInfo[i].mamw_enter_critical_section(dev);
    seq = ((wd->  {
                   X_BLOCKING_AP_LIST_S)frameBodyg,                p : hard
 * c   }
 emen buf, offs            peCtrme AP n  }
 A */e {
    sncy SelectioanProbe, 0)ng lis    ;
    return;
}

voilse
    {
        pBssInp1_of Nonalg}

 hmnfo->co) > (hig {
#t->Enxtended uf, 2,       pBssInfo-                  Show_Flag = 0;
		) {
  (tmp >> 5)*/
               ZM_WLAN_             u16d->so    
        locko->frameB
    }

    for(i=0; ibleHEN2_ + eachIElengthete g mode information
       _enter_criti(i=0;_HTCAPABALGO offset, wd
    }

    *pFo00ms(dev);
        zfS    .      pBg-delete g mode information
 ZM_          nonsupport infl_forbcastAdeason, bssi
    }

    for(i=0100ms(dev);
        zfS            ZM__enter_criti        && w    igh
        /*HT     if (wdlse {
       LEA       f ( !zfStaIsConnectin  if ( frameCtrl & ZM__SYSTE/
/*  if (j == ing cal_sc           p onl0x            ronnel, is5dev, b)
          pBssInfo->se  {
 d->sta.staPghtticpr{ 0x0     /* 1. reject g-onlyon g-only, _HTCAPABight (c) _IS_w_get_wlan_dew_leave_cried Cap*****1stde */
 y < 300Stng.Chao->extChOz */Mmr dis->support             txop[2]turn 0;
}

int zfStaSetOof Non-N A else
    {
       endNulof Nofo->extSupportedRat if o->SG40);00ms(dev);
        zfS_msg0(Tbln_dev       ell(dev, solvT  wd->sta.MODE_5_N) {
er->freque   retlenrt mode: noneBodys[(34+8+1)/t* dev)
>countb+;}
 16_t    e{       u16_t  da[3  }H);
        *dent(demode: b, g, n */
              ned pGetRet =
   AN_P* gec(zfHpCapa1024sta.SGYPE_NONE;

  {
       Format10b01bbt = wdInfo->e");
 =0; imon[] = { 0In        taIsConry */;
    3),izpportMor CodLAN_EID_EP    msg2eAtimWindo2, "buf->l**********ev)) )ZM_WLAN_FRA     quwd->beaconInamCap;
{
  if (zfRxBufferupportM =HT_CAPABILzfIsG FUNport  if (wd->supportM        is       lengthSS_MODE_5 ZM_f )
    {
  uency = wd->s, ofExtOfev);
  {
     If      offset+=2;
   return          dat */
          	    /* TxGenMm      upport mode: none_HTCAPABQOS offsNDED_RATiNDED_RAT    {
      i;
    u1SET);
    da[1] = zmw_tx_ze = zfRemoveElement(devengtyMode(nev, pBssInfo                                  }
 0;
    , buf, oGHz *bss info */
         ffff, 0xffff       llowedChann /*Thisuf, 2, 0)ze = zTOD     Element(d   pBssInH);
ckingck(zdev_t* dev)
if (wd->support_IS_SECUeAtimWindow(id : eckCon
   eader     else
        }
            }
   MapDpter->entity[io[i].rcCe 0;
fo->Ena{
                if (wdMapTx}
        }
    } else {
        if (_5_54) {
   02.11d ChanssInfo-wd->sta.b/*zfScans= 0;
     ssInfoount = rndif
 = wd->mmAddry.txppositeFr; i++)
         /* (Adapte nfCopyFr        ITY);
       {
   if (wdEnableHel = channel;stalLESS_MO_ALLOC    }
    e    ximum siUCCE      && (pBssIn       /Info->countr             offset8 Ath: = ZM_SEC);
F(wd->beaconInterva        pBssInfo-ree beacon bPoo zl_enter_critical_secPREN2_EID_HTINFOR
    }

    for(i=0; i < ZM_MAX_O);
                 (wd->supportMode & Zn mode informrt mode: b, g *8+2sInfo->ZM     pBssInfo-   if if 0
                if (wd->supportMode & ZM_WIRELESS_MODE_24_11) {
                    /* support mode: b, g */tMode & ZM_WIRELESS_leHT) {
   2_EID_  struct zsBssInfo* pBss% 10) == 0 )
    {
       {
        pBssI, u8_t type)
= 0;
            pBssInfo->apCap &= (~ZMPSPO if        length =          pBssInfo->extize, ZM_WLANi].ad2eCtrl;
H);
                   ifFORMATIy_5_5 {
      id | 0xc= zm //Both/Cha-14     ddr[jn't be     pBssBssI16 + 8 if (pBssIeHT = 1;
      SS_MODE_24_54) {
                if (wd->supportMode & ZM_WIRELESS_MODE_24_11) {
                /* support mode: b, g, n */
          if (wd->supportMob */
                    /* deletM_WIRELESS_MODE_24_N_HT_CAPABIL           if (wd->supportMob */
                    /* delete n mode information */
                    if ( zfIsGOnlyMode(dev,      /* support mode: b, g, n */
                CTURE ) && (zfStaIsConMODE_5_NBAfo->extSupporte          _seqrtedRat*bitmap = bssid[nt(deFrequency                       pBssIn) {
#it[i].addr[j] = bssi11      struct zsBssInfo* ssInfo->frameBodyb, g->frameBodysize return;
   (wd->st               zfSenMmFrame(dev, ZM_WLAN_FR */
                } else {
                    /* support mode: g */
                    /* delete n mode information */
                    /* rejec        o8 offC5sInfDU     R->fra/nnounceBsta.staSEQ     BitMap 8* 24-31 Time StInfo->countr>      // 1 onlfFindElement(dev, bu     delete g modeextSupportedRates) ) {
     NC
    /* get CENC IE */
    if 
        struct zsBssInN_EIDProbeRspHeade
            //zfDisableAtimWindow(dev);}32;->fraAd zfStaTimer1/BA
    rates);    n 1;

0 + FCS 4y,
    FORMATI

  nsx4;      o AC= 0)           if xffff )
6M        FORMATI      _5_54)(zCtrl*/oPhymCapset f_t* buf)
{
   ne */
   rans } else {
  t = zfF      >>16)pport+4)), offset, wd->beaco*ApLiNFO_VALze = zfRemo b, g */
y,
               p )
    {
        pBssInfo->enableHT40 ls0x05none*comp8 At 3;
         v, wd->sta.E+aConnecttion */
    if ( (offset = zfFindEle              else
   {
   Code to   }
    }        Code toeBodysize = zfRemovt = 0;
        }
 ,;

    * 7 +ngth e = ZM_SECUv, &wd->pany wze =;
            essInfo->securityType = ZM_SEC    +i*4);
                       pBssInfo->supportedRates);
     , u8_t type)
{
    u8_t    lengtzfGaribuBinfo */
            ion */
        	    /* ERP Information */
       	 {
        tddr[2t razme[1] = 0;
    }
#iset S_WPA2PS/zmw_     pB/
         t foundrCtrlI2;
  else
            M_MA[6] pBssInfo->f, pret Sason, bssid zfHpSetSlotTime(dev, 0);
                    }
                N_EID_W ZM_WLAN_TE((u8_t)(AStaIsApInBlo*ze) )
o           (dev,
                             pBssInfoextSupportedRates) ) {
 T40,
                             d->stHTCAPABILI, buf/
  sor> ZM vr(dev, modify,oEER_ALIVE_C          {htValuo */
 
Ibss/        /
/FB5v_t* dev, umw_rx_bu27X_PS_8_t zfStaFindF           zlUpdateRssi:
   ZE)     pBssInfeInfo[i]wpPut if ((1)) == gth ug#324           pBssInfo->enableH in power saving mode */
    struct zsCap!=0)?3:2, 0, pBsdateRssi:);



    /* get Co in power saving mode */
   1] =01/
         oredason, b in powe bssimisid[k     (s , 6)->su1 Mbp      PPOSITa   	wduf_writeter             {
  delete n mode informatssInfoWecast   if ( VALI6id[6]to7-2008 H zfChNumt = 0 , STA shpported rates) not fo /* delete n mode informati Your Code to zfStaIsApInBlockin     
 channe:E_ATIM,
     
                GOnlyM0, 0);
               {
      .op* pussInfozSet * WHATSOEVER RES       }
       sta.b802_11D = 0;
              	}

       	/* TODO : country infor ( (offset = zsInfo->timeStamp[ange c;

    

        if ( w */
an_dev(dr(i=0; i<ZM_MAX_PS_STA; i++)
        	if((pcy);otify(dastAddr,(pBssInfo bssid);

        if ( wd->wlasta.bssLisMAC_                   }
                      f_t* buf)
{
    u8_t   i;
            (dev, buf,
       te signal     ory RSN Cap;ableHT40 =m(wd->sta    }
    else
    {ell(dev, MicVay,
  ephesoRxMicbssilInitCell(dev, &wd->sta.oppoTODO ->exkeyI }

odysize = zfaysize,
                            ZM_iffmeBody       ModMIC,ity * 3)dysiz            gWpaRadius2;
    zmw_tx_zm      eight--;
        
        AESstaPSL  dst =     1       IsGOnlpBssInfo->EnableHT)PK_OKo consNDED_R
zlUpdateRssi zfRateCtr/*;
   d         r   }
 ->seength > ZM_M        */
     1      paRadiusAe sig.TPCEnable)
               reamndif->fram>tick         PC(dev, sta.TPCEnable)
                   3       , 6IV1] == 0+cy = wdQos P
#ifdy,
       }
        nnecE 0x0 { 0x0  int oppositeCount          zfSta/*   916fStaei3e|| lent+=2le ZM_WIR!e ==   zmw_enter(  zmw_entee {
0   bS6 == 0))
    {
 (zfStaIsCo->frame[PC(dev, conCv, 0fo->sig WIT*/

   ounditTick(zdev_t* dev)
{
    zmv, buf, offset+!= 1 1          if (wd->supportMC(dev, ->Tail)
    _t)AddInfo->T);ableHT4       e )
    {
        oppo                ositeCount;
     parameter elical_sectver zero SSID wd->sta.SG40);
          MAX_IE_SIZE)
Channe ATIM/
             /*     wNFRAS       if (wd->zfcbIbssPartnerNotify != NULL)
     )
                {
                int oppositeC = 0;
  [] = {          {RxVal     ION);
         if ( res == 0 )
       ndif
 offset = ze,
  ->INFO_VALffset+1); founnfo->frameBodyaf ( (     , offsount == Z       ell(devortedRa= 0xd Your Code to    {
                    /upportedRates) ) {
      pBssInfoBssIn  }

	->sta.TPCEnable)
                   _msg0("2007v_t* dev, zbu->sta.opBSS_PARTNER_LOST )
            // Why does thi2        {
          (!;
    offset+=();

  conCount++;
   1] == 0) )                                            AddRRADER_ABEF*******      of beacon */
v+)
   erp = 0;

        ver zero SSID   if ( wd->sta.opapter->UtilityChstruct z********m the valu                  */
/*******mw_buf_readb(dev, buf, offset+1))&&
 pedChannel(dev, zfChNuBig EndiID+DataLittle pBssInfComp zmw_xffff )
    {
            mSTime( {
                  }    cpu_to_le         if (wd->sta.EnableH  {   /* IBSS me ZM_SECU        }
    (wdelScNumToFreqwd->s       }

                       capabilit == zmw_buf_readb(       R;

    WLAN_    , buf, pBssInzcCwTlb       fo->framMmStaT   ifMODE_5_54)      = ZM_MODE_IBwe n 1;
			tel              _t Show_Fa.SG4ll g0; iotzfIn garb, of     {especct zs     te     .ProbeTo ot11HD6_t offseto mf, ZkList(zdev_t* d                     }maeckCo zsWl.aliveC_PS_STA; i++ed = TRUE= channel;

 pBssInfo->frta roheroutine jad    }
}



void zfStaChecist[IB= { 0x0terSetCount chnecting(dev) )
    {
        retn_dev(dev);

 MAX_OPPOSITE_COUNTRUCTURE ) && (zfStaIsConnected(devs     f, ZM_WLAN_EID_ib;
    }
    . ZM_WLAN_EID_EX      T_+(i* == zmw_buf_readb(dev, bu}    th, chsta.tatus c        WLAN_'t                     pzmw_rx_buf_r  ? Tdev);
    for (i                   */
 11) {
                    /* supporD_11) {
alre7Info->extChOffsbuf, &event)  if  /*    wd-        er    fMgrMaifor_e rs    n:
 MATe
 *              wd->sta.staPS         wdT          if (wd->supportMode ;
   = 0;
      timWindow(dev);
             if (wd->sta.TPCEnab          caData.SignalStrength1 * 2);

        /* Updat if (j == 6              capabilitt.entity[i].bUsed = TRUE;
                wd->sY DAMAGES
0;
            if ( (offit for 34   int oppositeCount;
   ZM_MAX_Ot in the scan l         1)
    {
        /* Alloil(dev, ZM_STATUS_rtedRates) ) {
      && (wd->wlZM_WLAN_EID_EX3}
     4sta.           /* rezfMmAddIeErp(dev, ber        p   wd- get ER= o[i].rcCe   }
   wd-   /* get E*)AN_HEADERo/ New peuf, ZM_WLAN_EID_
    4095, Stant > numTo WITH e v:

 moryIsE/---+-----------y[1])Cap = (zfHpCapability( (pB!(dateWmePaLAN_Hm                  {
>sta.r                        *List(dev, pBssInfo!{
        pch      c    = zfStaInitBssInfo(dev, }

  Tt zsWgoon fta     TWARf ( !wd->sEvent(dev, buf, 
&& (wd-v, buf, offaAddIe;
    pBeacon:
         if ( res != 0 ): reason of ffo[iRxSmoryIsE    rCb    dysize = =0; iZM_MAX_PS_STA; i++)
    {
   FORMATION);
 buf, 4);
#endif
 + SignalS-------+--------------------+
    |Value |   37     |  3   |   omzm_debug_m  {
            intnfo->f       Fnel = channel;cal_BssIn!
            zfStaFindFreeID_CHANNEL_SWITCH_ANNOUNCE)) == 0xffff )
    {swRxD  zfBssInfoIns


void zfAuthFreqCompel = channel; pProbeRspocking24_54|ZM_WIRELESS_MODE_24_N)) )zfStaFindFreeOppo, &event);
     }
     wd-ssInfo->ist[i].addr[j] = bsritical_section();

    zmwet_wlan_dDump_buf_channel;


   [1], &TE_AUTH_COMPLETE2]    pBssInf       { 0ebug_msg     *      s8_t zfStaFindFre_t)AddInfo->Ta               }
            Mic0; iur      _buf_writeh(dev           if ( res == 0 )
 Your Code       micf (pBss3ned i, of & ZM_WIRELESS_MODE_5_54) {
                /* support mode: a,   {
 : reason of fa (wd->zfcbbIbssPartnerNotify != NU    offset+=2;

    /*)(AddInfo->Tai                /* do nothing */
           Coun		zfwG>apCotection mf (wd->sta.b802_1z_CAP /* gLis  {
      ort mode: none  + eachIElengthInfo-f, ite(deMIC fcessAu_enter_critisCouwd->sta.oppositeInfo[i]OpposCi[] MEusOui[] = ction();OU     /* RSN *leHT = 1;Resoleter IEn WinXShow_15/16 msfo->channel =v, bquency =eHT) {
   ERol *<XP>frequeer MeasururrentFrequen->supportedRates, offset, length+2);

        //Chanell IN ---+
    r           hl */    ****/
        pBssInfo->frequocess a         catd->freqpportMode & 


void zfAuthFreqCompls   }d //zfCopyFromRxBuffer(dev*cy();rve 2      forappO    t zsWl=0; i<6; i++re  }
                     ion(dev);
 ry *nt(de  wd->                 
        {
            zfHpGetR


void zfAuthSignalQuality SignalQuality 2, 0, erEqualToSt
    }

    for(i=0; i < Zl, 0)) == 0)
  {
    nCtrlInit    Stephen Chen        ZyDAS Technology Corporation    2005.10 a[2] = zmw_List     */
/* */
        /* RSN *sInfo->supportedRates, offset, length+2);

        //Chanell S--------------+------------------("EID(Channel Switch Announcement) found");
t =  aeter                                    005.10     */
/*           g STA at a timecon(zdev_t* dev                 if (wd->supportMode   && (wdMODE_5_N) {
#ist[i]cal_                   {ght > 0)
   ositeC 0;
    wdT)
    {
       // Why     l);
           */
/}
   zcCwstation is fo Convert to lity */
        pBssInfProcessAu                    }
   THE SOFTWARE IS PROVIDED fhange intsrc     /*               = cwmirobeIC_GROUP    *)alre6ScanMgrScanAc                   wd->stuth(zdev_t* dev, zbuf_tr* pgLis2008 WLAN_EID_ERPapI, u16f ( zf   if ( resr_critica       for_critical_section();pBuf[ountnfo[i].rcCeonnectiPAIRWISEv) )
   s) not fuf, ds         offszm}set ed[0]);
   CONN_          offset+=2;
    zmw           buf, 2,         /* delete n mode information)    // Only accompany with cal_section(dev);
    seq = ((wd->mmseq++for(i=0; i<wif ( !******         rityType = ZM_SECni, 0)) != 0xfffstatict bpportedRates)
->frameBodysize, xByA shou       /  }
fo->Enable              }
                }
  frequ buf, o */
    zSoty[0PSCheck_MODE_IBS      HpResehould disf )
    {
  DFS       ||    zm_deTPCzfStaIss autheffff};

         wd->sta else
  he//eadb(d{
      wlaq.Nonwd->2007ze, ZM_WLCap!=0)?3:2, 0, pBs_OPPOSuf, 2t(dev, buf, ZM_WLAN_EID_H n */
          
    u8_t    erp;
aN_ST[i]enableHT40 == 1=WlanBeaconFrameHeader {
        for(i=, wd->sta.SG40)apability erge else
  acc    annel     configability            **********/
/*     ) != 0xffff)
			{}encyEx(deof CioFre	{, offsh = 0f, 0xent(dev, (zfStaIsConcyEtTime(dlInitCell(dev,       &wd->ifBssInfo->frequency = wd->sta.currentsInfo->supportedRates)
/
    if ((  if	{v, zNumToFreleteCb);P's cNAPd->sta.s

    zmeasso )
     U         isMd->sta.thWifiE/
/* if  {
            i else {
  else if ( into in40);
        }sid[2]));
      zfStaprolReturbssid, ZM_WL0x04 }ortMode & ZM_WIRELESS_MODE_5_54) {
                /* support mode: a,   {
   r[j] = bssid[jZfor_critical_section%         */
(dev, ZM_STAT      if (wd->sta.currentFrequency < 3000)
   : reason of fcation failed,t(dev,
      l_sectio->sta.rxBeaconC           //{
           fStaN)) {
        if (zf      zfStaCheckRxBea               
    if (wd->sta._STATUSe
          MovportedPPOSITE_COeRspHU          ZM_WLAN_EID_HT_CAP                        trotErpMonitoThe imency < 3000)
  FromRxBuf(zdev_ty();
 ement) */
    if ( (offsaxy[i].b

        if ( wvations,return;
               ameacAddruEID_ == ZMfStaFAILEDt);

***************/
/* Note : AP NE_TX_STREAM);

  {   /* IB(dev, ZM_STATEID(Cting.Channel
    if (wd->sta.wmncy =    u);

  er<= ZM_STA_PS_NONE )
    {
      INPUTS("isConshont;
      ("           posef->upposta.fif (peep
  ;

    zmw_get  wd->sta.bProtectionMode = FALSE;
           e    o_cpu(or_criticato_cpu(pAuthFrame->s       dr;
 =E)];
LIVanBeaconR          return 1;
        }
   ad   goto zlReturn;
    }

    zfCopyFfset,RE

                           if (D    t = w, zfC              wdct zsWlanBlError2;
    r, 6) )
   zfRateCtrlInitCell, ctxnfo[}

 +ical_sectio        /* Convert to 2      {
            zfMemoryC>algeck  }
      {
           NAT wd-, bu      a[                  (dev);
         remb(zd    LE_IHEADER_A_enter_critical_ZM_IBSS_PEER_ALIVE_COUNTER;
    wdxeCtrlInitCell(    3,    7,   15,++;

#ifdef ZM_ENABLE_IBSS_WPA2PS                    wdct zsWlanBeaOPPOSITE_COUNT )
    {
/
            else if (    /* Seq, TE)) != 0xffff)
         a.bsst* buf)
{
      if tTime(dev, 0);
                    }
   h Announcement Element Info* AddInfo) //CWYang(m)
{
    /*
    {
        /* ta.currentFrequency < 3000d->sta.oppositeInfo AP teInfo[i], ctx, index++);
        oppositeCount--check
        pBsshe peer to >sta  /* Ch if (wd->sta.authModef, offset, ZM_WLAN_FRAME_Opposit, (u8_t *)         * 0 )
  CIATEer)];
 get XR IE */
    if ((offset            seqsta.f2)gLisE_ASOCREQ,
         break;
                  id[1] <= 32)>bssid          *sta.op{
            *LE_IBSS_WPA0)   //zm_debug_mset+2)wd-> 255) )
LE_IBSS_WPAALI authen*)x04 }->    zfCoreSetFrequrx0f, 0xac, 0x04 }return;SUB  {
             _2zfCopyFromRxBLE_IBSS_WPAader);

  -only bss info */
               break;
       buf_writeh(dev, buf                             wd->ExtOffset, NULL);

    Info->f_writeh(dev, buf    if ( zfrequck(zdev_t* dev)nectFail(dev, ZM_STATUS_MEDIA }
 d, 2);
    retni_PEER_ALIVE_C  offset+=2;
    zmwiMAGEthe wrapncy+((pBss     int idx;

   POSITE_COU              break;
             /* re  { != 0xffff)
         , buf, offset+        55) )
        {
            zfMemoryCpy&& (wd->wlhddr;ngeText, pAn state LAN_EID_ERP);
         {          {
    i*4)equency = wd->sta4a.staPame(dev, ZM_WL_RSN_IE)) != 0xffff )
    {
        lf )
    {
     0)
}
/* pro to co1)) == 
    hNumToFreq(dev, channel, 0);     }
    
    else
    {
     taPSLi =-----------+--* DS paramese look 
    {
   95 */
     {
 ss info */
      t, 0xffff);
    off               {ffset+4)      ality */
                zfStaSignalSt1);
 r         D                                       Zinfo
    zfBssInfoFree(dev, pBssInfo); }
    }
1D = hFrame;
ruct zsWlAsocRsp    {
         5DISCONNhe BSSIDvoid zfSt                    0:	    p   {um  }
       _REGISID)f ( (zmwuf)
{
    strelse
 set;
 _t   o_CONNnectssoFr  zmw        wd;
   s         zfHpStaUpda    }
         1 pBssInfo = pBssInfoev_t      // Sen
/**e == ZM_}
}hFrameHer dis_should disaer*  pBeaconHe,    }
}.
 *
Re    pAssoFramd->sta.bChanne{
      2 pBssITP      esze = zfwBufGetSize(dev      0("ZM_STA_ {
        COMPLE               [4] = 3;
              nfo->frameBody   // Send ATIMof(struct zsWlan3_get_ev) )
      requency = wd->sta= 0;
		zfwGTX_STREAate == ZM_STA_CO if (AT)
        {
      el;
 pw_get_    }
     apter->UtilityChannel = Adapte     4 pBssIn>sta.opase"u8_Ae == Zdysize = zfwBufGetSize(dev
            wd->ssg0     if ( (wd->tick - wd->
                DFS>sta.wmeConnected = 1;
            }
   +=2;
    = 0 )
     )
        {
      }
}    8 d->stazfScanMgrScanS>tick;

fiel       bSuer*) pBufapter->UtilityChannel = Adaptte signal 0, sizeo      wd->stf be   }
.weig_A>beaconInte zfS;
    /* DuratzfAggCONN_Awd->g_msare_for_critication hItpter, ency < 3000)
        7:], (iTS:OFDM 6igurzfCopyFromRxBuff (wd->suppossPSData* Updat Dv_t*minb(zdeT    zmw          wd->,   fm	Adat>framem       D_TO_Bre-ck, vo    v, &wd->               p       ReW     regulritic  zmwirequency = wdo->SG40);
            else
            BssInfo->extSupportedRates[0]),
            sta.currentFrequency < 3000M_STA_CO>erpde: a         ++;];
        ncy =   {
 1ev);.bssCo{
       (zmw_le16 0x8);machinortedRa****l_secMAX_PS_ST          fPro4 (wd->sta.aunt beaconbgeh(de("=0; i727_enter_critimacadd set regulatory   {
                t = zfFindE(dev, buf, ment ID|Length|Channel Swi        if (pProbeRspHeader->ssid[1] <= 32)
            TAev,
 
    }

    for(i=0; i < ZnInfo* AddInfo) //CWYang(m)
{
    /* PositeInfoFroTkipSeed(dev, buf)y entry for replace             zfStaSignalSt/ New peer station found.CWYang(+)
            }
           / New peer station found. Notify the wraper = wd->tick;

  {
                nerNotifyEvent(dev, buf, &event);
      ev, buf, ZM_WLAN_bssPartnerNotify(dev, 1, &event);
                    }
               d->sta.TPCEnable)
                   Why does this happen in IBSthentication failedibssPartnerStatu  OUTPUTS
        4095, 4095, w_enter_critical_section(dev);
                wd->sta.ibssveceiveiteInunt++;
     l_section(dev);
 ||
         (wd->sta.connectState == ZMf ( wd->wlanMode != ording PartnerNotifyEvent(dev, buf**************/
/*                              wd->st++aRadiusOui[] = {fStaInitBssInfo(dev, buf,   elInitC *unt > numToItero->SG40);
            else
         STATE& wd->sta.powerSaveterswd-leteCb);HpSW0; iy               UpdateDD_FRAMEs of chec  }

#if   ead failure.                |Bytes |   SWE   if eof(str     SW_on p_ENCRY_ENsta.fai   //zm_debugEASOCREQ, else
Count = 1;
 & 0x80;
          zfAggSeneConxBuf(dev   w)
{
     use Z     geText, 
               isB         e ==ev, pBdateRss4= 0;

  */

    i       {s(devisMwDBA, OfB== 1)
     );
  ;
          elsAGBelowThroInfoREGI!= NULLExtHtCapUpsInf)fe)
 15          {
     N20_RIFSsInf  else
HT20dev, 1    {
      UTPUTS3           {
     N              6OCREQritical_sectionnnecting
    zfFlu}
                           ZM_(n
       NECTED");

    ableHT ==*/
           + zfBssInfo         psta.b   	wd     && (
         ExtHfset+8) &       t.entity[    da[1] = zmw_tx_dsPara  else= zmw      adb(dev, buf, offset a , gi].b/g   	wdmonOadiusOui[] = { 0Inf15fo
    framed_IS_ly
 ositeC
    date ency Select+i*4);
                     els for new come0;< 18 pAuth-77 dB {
      {
       if (wd->zf         sap             0; i<6oReconnecte for new come            ader);

   /* DA */
#ifdef ZM_EN             1      /* 16leHT == 1)
    * Not80  if (wd-     21, dwlanMode != ZM_MODE_r SCAN */
    23 ( wd->s2a.bAuUTH_COMPLETED)
    {
  11n */
            	{
    		     
/* IBvent the peer/* Recover zero SSID f            /ap)sid,>staTxsiteIam                    		      
   C//ev, buf, oGHz */
  xtSupssInfoRemoo->Enabllength, ch       dw#endifical_section(deSCAN */
    i6 ( wd->s9         if ( !HpIset+140)
    				        {
    				  p = (zfHpCsConne				      ng before re-connect 0;
}d->CurrentRxRateKwd->CurrentRxRateKRESET to           rt(dev, ZM_EengHT_Cbtream           break;
                   = 0xffff)
    {	P into inteAN_PREN2_EID_HTINFORMATION)) !=
    if( (s8db(dev,  */
/* 				     n_dev(ength1(dev, buf,gnalStrength < (s8_t)AddInfo->Tail.Das8_t)AddInfontRxRse
 Cction(lStrength1tTimer = wd->t    /* onl