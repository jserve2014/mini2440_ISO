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
/*                                                                      */
/*  Module Name : htr.c                                                 */
/*                                                                      */
/*  Abstract                                                            */
/*      This module contains Tx and Rx functions.                       */
/*                                                                      */
/*  NOTES                                                               */
/*      None                                                            */
/*                                                                      */
/************************************************************************/
#include "cprecomp.h"

u16_t zfWlanRxValidate(zdev_t* dev, zbuf_t* buf);
u16_t zfWlanRxFilter(zdev_t* dev, zbuf_t* buf);



const u8_t zgSnapBridgeTunnel[6] = { 0xAA, 0xAA, 0x03, 0x00, 0x00, 0xF8 };
const u8_t zgSnap8021h[6] = { 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00 };
/* Table for converting IP DSCP P2-P0 bits to 802.11e Access Category */
const u8_t zcUpToAc[8] = {0, 1, 1, 0, 2, 2, 3, 3}; //WMM default
//const u8_t zcUpToAc[8] = {0, 1, 1, 0, 0, 0, 0, 0}; //For 2 TxQ
//const u8_t zcUpToAc[8] = {0, 0, 0, 0, 0, 0, 0, 0}; //For single TxQ
const u8_t zcMaxspToPktNum[4] = {8, 2, 4, 6};

u8_t zfGetEncryModeFromRxStatus(struct zsAdditionInfo* addInfo)
{
    u8_t securityByte;
    u8_t encryMode;

    securityByte = (addInfo->Tail.Data.SAIndex & 0xc0) >> 4;  /* byte4 */
    securityByte |= (addInfo->Tail.Data.DAIndex & 0xc0) >> 6; /* byte5 */

    switch( securityByte )
    {
        case ZM_NO_WEP:
        case ZM_WEP64:
        case ZM_WEP128:
        case ZM_WEP256:
#ifdef ZM_ENABLE_CENC
        case ZM_CENC:
#endif //ZM_ENABLE_CENC
        case ZM_TKIP:
        case ZM_AES:

            encryMode = securityByte;
            break;

        default:

            if ( (securityByte & 0xf8) == 0x08 )
            {
                // decrypted by software
            }

            encryMode = ZM_NO_WEP;
            break;
    }

    return encryMode;
}

void zfGetRxIvIcvLength(zdev_t* dev, zbuf_t* buf, u8_t vap, u16_t* pIvLen,
                        u16_t* pIcvLen, struct zsAdditionInfo* addInfo)
{
    u16_t wdsPort;
    u8_t  encryMode;

    zmw_get_wlan_dev(dev);

    *pIvLen = 0;
    *pIcvLen = 0;

    encryMode = zfGetEncryModeFromRxStatus(addInfo);

    if ( wd->wlanMode == ZM_MODE_AP )
    {
        if (vap < ZM_MAX_AP_SUPPORT)
        {
            if (( wd->ap.encryMode[vap] == ZM_WEP64 ) ||
                    ( wd->ap.encryMode[vap] == ZM_WEP128 ) ||
                    ( wd->ap.encryMode[vap] == ZM_WEP256 ))
            {
                *pIvLen = 4;
                *pIcvLen = 4;
            }
            else
            {
                u16_t id;
                u16_t addr[3];

                addr[0] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A2_OFFSET);
                addr[1] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A2_OFFSET+2);
                addr[2] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A2_OFFSET+4);

                /* Find STA's information */
                if ((id = zfApFindSta(dev, addr)) != 0xffff)
                {
                    if (wd->ap.staTable[id].encryMode == ZM_TKIP)
                    {
                        *pIvLen = 8;
                        *pIcvLen = 4;
                    }
                    else if (wd->ap.staTable[id].encryMode == ZM_AES)
                    {
                        *pIvLen = 8;
                        *pIcvLen = 8; // AES MIC
                        //*pIcvLen = 0;
                    }
#ifdef ZM_ENABLE_CENC
                    else if (wd->ap.staTable[id].encryMode == ZM_CENC)
                    {
                        *pIvLen = 18;
                        *pIcvLen= 16;
                    }
#endif //ZM_ENABLE_CENC
                }
            }
            /* WDS port checking */
            if ((wdsPort = vap - 0x20) >= ZM_MAX_WDS_SUPPORT)
            {
                wdsPort = 0;
            }

            switch (wd->ap.wds.encryMode[wdsPort])
			{
			case ZM_WEP64:
			case ZM_WEP128:
			case ZM_WEP256:
                *pIvLen = 4;
                *pIcvLen = 4;
				break;
			case ZM_TKIP:
                *pIvLen = 8;
                *pIcvLen = 4;
				break;
			case ZM_AES:
                *pIvLen = 8;
                *pIcvLen = 0;
				break;
#ifdef ZM_ENABLE_CENC
            case ZM_CENC:
                *pIvLen = 18;
                *pIcvLen = 16;
				break;
#endif //ZM_ENABLE_CENC
			}/* end of switch */
        }
    }
	else if ( wd->wlanMode == ZM_MODE_PSEUDO)
    {
        /* test: 6518 for QA auto test */
        switch (encryMode)
		{
        case ZM_WEP64:
        case ZM_WEP128:
        case ZM_WEP256:
            *pIvLen = 4;
            *pIcvLen = 4;
			break;
		case ZM_TKIP:
            *pIvLen = 8;
            *pIcvLen = 4;
			break;
		case ZM_AES:
            *pIvLen = 8;
            *pIcvLen = 0;
			break;
#ifdef ZM_ENABLE_CENC
        case ZM_CENC:
            *pIvLen = 18;
            *pIcvLen = 16;
#endif //ZM_ENABLE_CENC
		}/* end of switch */
    }
    else
    {
        if ( (encryMode == ZM_WEP64)||
             (encryMode == ZM_WEP128)||
             (encryMode == ZM_WEP256) )
        {
            *pIvLen = 4;
            *pIcvLen = 4;
        }
        else if ( encryMode == ZM_TKIP )
        {
            *pIvLen = 8;
            *pIcvLen = 4;
        }
        else if ( encryMode == ZM_AES )
        {
            *pIvLen = 8;
            *pIcvLen = 8; // AES MIC
        }
#ifdef ZM_ENABLE_CENC
        else if ( encryMode == ZM_CENC)
        {
            *pIvLen = 18;
            *pIcvLen= 16;
        }
#endif //ZM_ENABLE_CENC
    }
}


/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfAgingDefragList           */
/*      Force flushing whole defrag list or aging the buffer            */
/*      in the defrag list.                                             */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      flushFlag : 1=>flushing, 0=>Aging                               */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      None                                                            */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen Chen        Atheros Communications, INC.    2007.1      */
/*                                                                      */
/************************************************************************/
void zfAgingDefragList(zdev_t* dev, u16_t flushFlag)
{
    u16_t i, j;
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    zmw_enter_critical_section(dev);

    for(i=0; i<ZM_MAX_DEFRAG_ENTRIES; i++)
    {
        if (wd->defragTable.defragEntry[i].fragCount != 0 )
        {
            if (((wd->tick - wd->defragTable.defragEntry[i].tick) >
                        (ZM_DEFRAG_AGING_TIME_SEC * ZM_TICK_PER_SECOND))
               || (flushFlag != 0))
            {
                zm_msg1_rx(ZM_LV_2, "Aging defrag list :", i);
                /* Free the buffers in the defrag list */
                for (j=0; j<wd->defragTable.defragEntry[i].fragCount; j++)
                {
                    zfwBufFree(dev, wd->defragTable.defragEntry[i].fragment[j], 0);
                }
            }
        }
        wd->defragTable.defragEntry[i].fragCount = 0;
    }

    zmw_leave_critical_section(dev);

    return;
}


/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfAddFirstFragToDefragList  */
/*      Add first fragment to defragment list, the first empty entry    */
/*      will be selected. If the list is full, sequentially select      */
/*      one entry for replacement.                                      */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      buf : first fragment buffer                                     */
/*      addr : address of first fragment buffer                         */
/*      seqNum : sequence of first fragment buffer                      */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      None                                                            */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen Chen        Atheros Communications, INC.    2007.1      */
/*                                                                      */
/************************************************************************/
void zfAddFirstFragToDefragList(zdev_t* dev, zbuf_t* buf, u8_t* addr, u16_t seqNum)
{
    u16_t i, j;
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    zmw_enter_critical_section(dev);

    /* Find an empty one in defrag list */
    for(i=0; i<ZM_MAX_DEFRAG_ENTRIES; i++)
    {
        if ( wd->defragTable.defragEntry[i].fragCount == 0 )
        {
            break;
        }
    }

    /* If full, sequentially replace existing one */
    if (i == ZM_MAX_DEFRAG_ENTRIES)
    {
        i = wd->defragTable.replaceNum++ & (ZM_MAX_DEFRAG_ENTRIES-1);
        /* Free the buffers in the defrag list to be replaced */
        for (j=0; j<wd->defragTable.defragEntry[i].fragCount; j++)
        {
            zfwBufFree(dev, wd->defragTable.defragEntry[i].fragment[j], 0);
        }
    }

    wd->defragTable.defragEntry[i].fragCount = 1;
    wd->defragTable.defragEntry[i].fragment[0] = buf;
    wd->defragTable.defragEntry[i].seqNum = seqNum;
    wd->defragTable.defragEntry[i].tick = wd->tick;

    for (j=0; j<6; j++)
    {
        wd->defragTable.defragEntry[i].addr[j] = addr[j];
    }

    zmw_leave_critical_section(dev);

    return;
}


/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfAddFragToDefragList       */
/*      Add middle or last fragment to defragment list.                 */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      buf : first fragment buffer                                     */
/*      addr : address of fragment buffer                               */
/*      seqNum : sequence fragment buffer                               */
/*      fragNum : fragment number of fragment buffer                    */
/*      moreFrag : more frag bit of fragment buffer                     */
/*      addInfo : addition info of fragment buffer                      */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      None                                                            */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen Chen        Atheros Communications, INC.    2007.1      */
/*                                                                      */
/************************************************************************/
zbuf_t* zfAddFragToDefragList(zdev_t* dev, zbuf_t* buf, u8_t* addr,
        u16_t seqNum, u8_t fragNum, u8_t moreFrag,
        struct zsAdditionInfo* addInfo)
{
    u16_t i, j, k;
    zbuf_t* returnBuf = NULL;
    u16_t defragDone = 0;
    u16_t lenErr = 0;
    u16_t startAddr, fragHead, frameLen, ivLen, icvLen;
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    zmw_enter_critical_section(dev);

    /* Find frag in the defrag list */
    for(i=0; i<ZM_MAX_DEFRAG_ENTRIES; i++)
    {
        if ( wd->defragTable.defragEntry[i].fragCount != 0 )
        {
            /* Compare address */
            for (j=0; j<6; j++)
            {
                if (addr[j] != wd->defragTable.defragEntry[i].addr[j])
                {
                    break;
                }
            }
            if (j == 6)
            {
                /* Compare sequence and fragment number */
                if (seqNum == wd->defragTable.defragEntry[i].seqNum)
                {
                    if ((fragNum == wd->defragTable.defragEntry[i].fragCount)
                                && (fragNum < 8))
                    {
                        /* Add frag frame to defrag list */
                        wd->defragTable.defragEntry[i].fragment[fragNum] = buf;
                        wd->defragTable.defragEntry[i].fragCount++;
                        defragDone = 1;

                        if (moreFrag == 0)
                        {
                            /* merge all fragment if more data bit is cleared */
                            returnBuf = wd->defragTable.defragEntry[i].fragment[0];
                            startAddr = zfwBufGetSize(dev, returnBuf);
                            /* skip WLAN header 24(Data) or 26(QoS Data) */
                            fragHead = 24 + ((zmw_rx_buf_readh(dev, returnBuf, 0) & 0x80) >> 6);
                            zfGetRxIvIcvLength(dev, returnBuf, 0, &ivLen, &icvLen, addInfo);
                            fragHead += ivLen; /* skip IV */
                            for(k=1; k<wd->defragTable.defragEntry[i].fragCount; k++)
                            {
                                frameLen = zfwBufGetSize(dev,
                                                         wd->defragTable.defragEntry[i].fragment[k]);
                                if ((startAddr+frameLen-fragHead) < 1560)
                                {
                                    zfRxBufferCopy(dev, returnBuf, wd->defragTable.defragEntry[i].fragment[k],
                                               startAddr, fragHead, frameLen-fragHead);
                                    startAddr += (frameLen-fragHead);
                                }
                                else
                                {
                                    lenErr = 1;
                                }
                                zfwBufFree(dev, wd->defragTable.defragEntry[i].fragment[k], 0);
                            }

                            wd->defragTable.defragEntry[i].fragCount = 0;
                            zfwBufSetSize(dev, returnBuf, startAddr);
                        }
                        break;
                    }
                }
            }
        }
    }

    zmw_leave_critical_section(dev);

    if (lenErr == 1)
    {
        zfwBufFree(dev, returnBuf, 0);
        return NULL;
    }
    if (defragDone == 0)
    {
        zfwBufFree(dev, buf, 0);
        return NULL;
    }

    return returnBuf;
}


/* return value = NULL => save or free this frame         */
zbuf_t* zfDefragment(zdev_t* dev, zbuf_t* buf, u8_t* pbIsDefrag,
                     struct zsAdditionInfo* addInfo)
{
    u8_t fragNum;
    u16_t seqNum;
    u8_t moreFragBit;
    u8_t addr[6];
    u16_t i;
    zmw_get_wlan_dev(dev);

    ZM_BUFFER_TRACE(dev, buf)

    *pbIsDefrag = FALSE;
    seqNum = zmw_buf_readh(dev, buf, 22);
    fragNum = (u8_t)(seqNum & 0xf);
    moreFragBit = (zmw_buf_readb(dev, buf, 1) & ZM_BIT_2) >> 2;

    if ((fragNum == 0) && (moreFragBit == 0))
    {
        /* Not part of a fragmentation */

        return buf;
    }
    else
    {
        wd->commTally.swRxFragmentCount++;
        seqNum = seqNum >> 4;
        for (i=0; i<6; i++)
        {
            addr[i] = zmw_rx_buf_readb(dev, buf, ZM_WLAN_HEADER_A2_OFFSET+i);
        }

        if (fragNum == 0)
        {
            /* more frag = 1 */
            /* First part of a fragmentation */
            zm_msg1_rx(ZM_LV_2, "First Frag, seq=", seqNum);
            zfAddFirstFragToDefragList(dev, buf, addr, seqNum);
            buf = NULL;
        }
        else
        {
            /* Middle or last part of a fragmentation */
            zm_msg1_rx(ZM_LV_2, "Frag seq=", seqNum);
            zm_msg1_rx(ZM_LV_2, "Frag moreFragBit=", moreFragBit);
            buf = zfAddFragToDefragList(dev, buf, addr, seqNum, fragNum, moreFragBit, addInfo);
            if (buf != NULL)
            {
                *pbIsDefrag = TRUE;
            }
        }
    }

    return buf;
}


#if ZM_PROTOCOL_RESPONSE_SIMULATION
u16_t zfSwap(u16_t num)
{
    return ((num >> 8) + ((num & 0xff) << 8));
}


void zfProtRspSim(zdev_t* dev, zbuf_t* buf)
{
    u16_t ethType;
    u16_t arpOp;
    u16_t prot;
    u16_t temp;
    u16_t i;
    u16_t dip[2];
    u16_t dstPort;
    u16_t srcPort;

    ethType = zmw_rx_buf_readh(dev, buf, 12);
    zm_msg2_rx(ZM_LV_2, "ethType=", ethType);

    /* ARP */
    if (ethType == 0x0608)
    {
        arpOp = zmw_rx_buf_readh(dev, buf, 20);
        dip[0] = zmw_rx_buf_readh(dev, buf, 38);
        dip[1] = zmw_rx_buf_readh(dev, buf, 40);
        zm_msg2_rx(ZM_LV_2, "arpOp=", arpOp);
        zm_msg2_rx(ZM_LV_2, "ip0=", dip[0]);
        zm_msg2_rx(ZM_LV_2, "ip1=", dip[1]);

        //ARP request to 192.168.1.15
        if ((arpOp == 0x0100) && (dip[0] == 0xa8c0) && (dip[1] == 0x0f01));
        {
            zm_msg0_rx(ZM_LV_2, "ARP");
            /* ARP response */
            zmw_rx_buf_writeh(dev, buf, 20, 0x0200);

            /* dst hardware address */

            /* src hardware address */
            //zmw_rx_buf_writeh(dev, buf, 6, 0xa000);
            //zmw_rx_buf_writeh(dev, buf, 8, 0x0000);
            //zmw_rx_buf_writeh(dev, buf, 10, 0x0000);

            /* dst ip address */
            for (i=0; i<5; i++)
            {
                temp = zmw_rx_buf_readh(dev, buf, 22+(i*2));
                zmw_rx_buf_writeh(dev, buf, 32+(i*2), temp);
            }

            /* src hardware address */
            zmw_rx_buf_writeh(dev, buf, 22, 0xa000);
            zmw_rx_buf_writeh(dev, buf, 24, 0x0000);
            zmw_rx_buf_writeh(dev, buf, 26, 0x0000);

            /* src ip address */
            zmw_rx_buf_writeh(dev, buf, 28, 0xa8c0);
            zmw_rx_buf_writeh(dev, buf, 30, 0x0f01);
        }
    }
    /* ICMP */
    else if (ethType == 0x0008)
    {
        zm_msg0_rx(ZM_LV_2, "IP");
        prot = zmw_rx_buf_readb(dev, buf, 23);
        dip[0] = zmw_rx_buf_readh(dev, buf, 30);
        dip[1] = zmw_rx_buf_readh(dev, buf, 32);
        zm_msg2_rx(ZM_LV_2, "prot=", prot);
        zm_msg2_rx(ZM_LV_2, "ip0=", dip[0]);
        zm_msg2_rx(ZM_LV_2, "ip1=", dip[1]);

        /* PING request to 192.168.1.15 */
        if ((prot == 0x1) && (dip[0] == 0xa8c0) && (dip[1] == 0x0f01))
        {
            zm_msg0_rx(ZM_LV_2, "ICMP");
            /* change dst */
            for (i=0; i<3; i++)
            {
                temp = zmw_rx_buf_readh(dev, buf, 6+(i*2));
                zmw_rx_buf_writeh(dev, buf, i*2, temp);
            }
            /* change src */
            zmw_rx_buf_writeh(dev, buf, 6, 0xa000);
            zmw_rx_buf_writeh(dev, buf, 8, 0x0000);
            zmw_rx_buf_writeh(dev, buf, 10, 0x0000);

            /* exchange src ip and dst ip */
            for (i=0; i<2; i++)
            {
                temp = zmw_rx_buf_readh(dev, buf, 26+(i*2));
                zmw_rx_buf_writeh(dev, buf, 30+(i*2), temp);
            }
            zmw_rx_buf_writeh(dev, buf, 26, 0xa8c0);
            zmw_rx_buf_writeh(dev, buf, 28, 0x0f01);

            /* change icmp type to echo reply */
            zmw_rx_buf_writeb(dev, buf, 34, 0x0);

            /* update icmp checksum */
            temp = zmw_rx_buf_readh(dev, buf, 36);
            temp += 8;
            zmw_rx_buf_writeh(dev, buf, 36, temp);
        }
        else if (prot == 0x6)
        {
            zm_msg0_rx(ZM_LV_2, "TCP");
            srcPort = zmw_rx_buf_readh(dev, buf, 34);
            dstPort = zmw_rx_buf_readh(dev, buf, 36);
            zm_msg2_rx(ZM_LV_2, "Src Port=", srcPort);
            zm_msg2_rx(ZM_LV_2, "Dst Port=", dstPort);
            if ((dstPort == 0x1500) || (srcPort == 0x1500))
            {
                zm_msg0_rx(ZM_LV_2, "FTP");

                /* change dst */
                for (i=0; i<3; i++)
                {
                    temp = zmw_rx_buf_readh(dev, buf, 6+(i*2));
                    zmw_rx_buf_writeh(dev, buf, i*2, temp);
                }
                /* change src */
                zmw_rx_buf_writeh(dev, buf, 6, 0xa000);
                zmw_rx_buf_writeh(dev, buf, 8, 0x0000);
                zmw_rx_buf_writeh(dev, buf, 10, 0x0000);

                /* exchange src ip and dst ip */
                for (i=0; i<2; i++)
                {
                    temp = zmw_rx_buf_readh(dev, buf, 26+(i*2));
                    zmw_rx_buf_writeh(dev, buf, 30+(i*2), temp);
                }
                zmw_rx_buf_writeh(dev, buf, 26, 0xa8c0);
                zmw_rx_buf_writeh(dev, buf, 28, 0x0f01);
#if 0
                /* Patch src port */
                temp = zmw_rx_buf_readh(dev, buf, 34);
                temp = zfSwap(zfSwap(temp) + 1);
                zmw_rx_buf_writeh(dev, buf, 34, temp);
                temp = zmw_rx_buf_readh(dev, buf, 38);
                temp = zfSwap(zfSwap(temp) + 1);
                zmw_rx_buf_writeh(dev, buf, 38, temp);

                /* Patch checksum */
                temp = zmw_rx_buf_readh(dev, buf, 50);
                temp = zfSwap(temp);
                temp = ~temp;
                temp += 2;
                temp = ~temp;
                temp = zfSwap(temp);
                zmw_rx_buf_writeh(dev, buf, 50, temp);
#endif
            }

        }
        else if (prot == 0x11)
        {
            /* change dst */
            for (i=0; i<3; i++)
            {
                temp = zmw_rx_buf_readh(dev, buf, 6+(i*2));
                zmw_rx_buf_writeh(dev, buf, i*2, temp);
            }
            /* change src */
            zmw_rx_buf_writeh(dev, buf, 6, 0xa000);
            zmw_rx_buf_writeh(dev, buf, 8, 0x0000);
            zmw_rx_buf_writeh(dev, buf, 10, 0x0000);

            zm_msg0_rx(ZM_LV_2, "UDP");
            srcPort = zmw_rx_buf_readh(dev, buf, 34);
            dstPort = zmw_rx_buf_readh(dev, buf, 36);
            zm_msg2_rx(ZM_LV_2, "Src Port=", srcPort);
            zm_msg2_rx(ZM_LV_2, "Dst Port=", dstPort);

            /* exchange src ip and dst ip */
            for (i=0; i<2; i++)
            {
                temp = zmw_rx_buf_readh(dev, buf, 26+(i*2));
                zmw_rx_buf_writeh(dev, buf, 30+(i*2), temp);
            }
            zmw_rx_buf_writeh(dev, buf, 26, 0xa8c0);
            zmw_rx_buf_writeh(dev, buf, 28, 0x0f01);

            /* exchange port */
            zmw_rx_buf_writeh(dev, buf, 34, srcPort+1);
            zmw_rx_buf_writeh(dev, buf, 36, dstPort);

            /* checksum = 0 */
            zmw_rx_buf_writeh(dev, buf, 40, 0);
        }

    }
    else if (ethType == 0x0060) /* =>0x0060 is port */
    {
        /* change src for Evl tool loop back receive */
        zmw_rx_buf_writeh(dev, buf, 6, 0xa000);
        zmw_rx_buf_writeh(dev, buf, 8, 0x0000);
        zmw_rx_buf_writeh(dev, buf, 10, 0x0000);
    }

}
#endif

/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfiTxSendEth                */
/*      Called to native 802.11 management frames                       */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      buf : buffer pointer                                            */
/*      port : WLAN port, 0=>standard, 0x1-0x7=>VAP, 0x20-0x25=>WDS     */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      error code                                                      */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Ray             ZyDAS Technology Corporation    2005.5      */
/*                                                                      */
/************************************************************************/
u16_t zfiTxSend80211Mgmt(zdev_t* dev, zbuf_t* buf, u16_t port)
{
    u16_t err;
    //u16_t addrTblSize = 0;
    //struct zsAddrTbl addrTbl;
    u16_t hlen;
    u16_t header[(24+25+1)/2];
    int i;

    for(i=0;i<12;i++)
    {
        header[i] = zmw_buf_readh(dev, buf, i);
    }
    hlen = 24;

    zfwBufRemoveHead(dev, buf, 24);

    if ((err = zfHpSend(dev, header, hlen, NULL, 0, NULL, 0, buf, 0,
            ZM_EXTERNAL_ALLOC_BUF, 0, 0)) != ZM_SUCCESS)
    {
        goto zlError;
    }

    return 0;

zlError:

    zfwBufFree(dev, buf, 0);
    return 0;
}

u8_t zfiIsTxQueueFull(zdev_t* dev)
{
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    zmw_enter_critical_section(dev);
    if ((((wd->vtxqHead[0] + 1) & ZM_VTXQ_SIZE_MASK) != wd->vtxqTail[0]) )
    {
        zmw_leave_critical_section(dev);
        return 0;
    }
    else
    {
        zmw_leave_critical_section(dev);
        return 1;
    }
}

/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfiTxSendEth                */
/*      Called to transmit Ethernet frame from upper layer.             */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      buf : buffer pointer                                            */
/*      port : WLAN port, 0=>standard, 0x1-0x7=>VAP, 0x20-0x25=>WDS     */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      error code                                                      */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen             ZyDAS Technology Corporation    2005.5      */
/*                                                                      */
/************************************************************************/
u16_t zfiTxSendEth(zdev_t* dev, zbuf_t* buf, u16_t port)
{
    u16_t err, ret;

    zmw_get_wlan_dev(dev);

    ZM_PERFORMANCE_TX_MSDU(dev, wd->tick);
    zm_msg1_tx(ZM_LV_2, "zfiTxSendEth(), port=", port);
    /* Return error if port is disabled */
    if ((err = zfTxPortControl(dev, buf, port)) == ZM_PORT_DISABLED)
    {
        err = ZM_ERR_TX_PORT_DISABLED;
        goto zlError;
    }

#if 1
    if ((wd->wlanMode == ZM_MODE_AP) && (port < 0x20))
    {
        /* AP : Buffer frame for power saving STA */
        if ((ret = zfApBufferPsFrame(dev, buf, port)) == 1)
        {
            return ZM_SUCCESS;
        }
    }
    else
#endif
    if (wd->wlanMode == ZM_MODE_INFRASTRUCTURE)
    {
        if ( zfPowerSavingMgrIsSleeping(dev) )
        {
            /*check ZM_ENABLE_POWER_SAVE flag*/
            zfPowerSavingMgrWakeup(dev);
        }
    }
#ifdef ZM_ENABLE_IBSS_PS
    /* IBSS power-saving mode */
    else if ( wd->wlanMode == ZM_MODE_IBSS )
    {
        if ( zfStaIbssPSQueueData(dev, buf) )
        {
            return ZM_SUCCESS;
        }
    }
#endif

#if 1
    //if ( wd->bQoSEnable )
    if (1)
    {
        /* Put to VTXQ[ac] */
        ret = zfPutVtxq(dev, buf);

        /* Push VTXQ[ac] */
        zfPushVtxq(dev);
    }
    else
    {
        ret = zfTxSendEth(dev, buf, port, ZM_EXTERNAL_ALLOC_BUF, 0);
    }

    return ret;
#else
    return zfTxSendEth(dev, buf, port, ZM_EXTERNAL_ALLOC_BUF, 0);
#endif

zlError:
    zm_msg2_tx(ZM_LV_1, "Tx Comp err=", err);

    zfwBufFree(dev, buf, err);
    return err;
}


/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfTxSendEth                 */
/*      Called to transmit Ethernet frame from upper layer.             */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      buf : buffer pointer                                            */
/*      port : WLAN port, 0=>standard, 0x10-0x17=>VAP, 0x20-0x25=>WDS   */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      error code                                                      */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen             ZyDAS Technology Corporation    2005.5      */
/*                                                                      */
/************************************************************************/
u16_t zfTxSendEth(zdev_t* dev, zbuf_t* buf, u16_t port, u16_t bufType, u16_t flag)
{
    u16_t err;
    //u16_t addrTblSize;
    //struct zsAddrTbl addrTbl;
    u16_t removeLen;
    u16_t header[(8+30+2+18)/2];    /* ctr+(4+a1+a2+a3+2+a4)+qos+iv */
    u16_t headerLen;
    u16_t mic[8/2];
    u16_t micLen;
    u16_t snap[8/2];
    u16_t snapLen;
    u16_t fragLen;
    u16_t frameLen;
    u16_t fragNum;
    struct zsFrag frag;
    u16_t i, j, id;
    u16_t offset;
    u16_t da[3];
    u16_t sa[3];
    u8_t up;
    u8_t qosType, keyIdx = 0;
    u16_t fragOff;
    u16_t newFlag;
    struct zsMicVar*  pMicKey;
    u8_t tkipFrameOffset = 0;

    zmw_get_wlan_dev(dev);

    zmw_declare_for_critical_section();

    newFlag = flag & 0xff00;
    flag = flag & 0xff;

    zm_msg1_tx(ZM_LV_2, "zfTxSendEth(), port=", port);

    /* Get IP TOS for QoS AC and IP frag offset */
    zfTxGetIpTosAndFrag(dev, buf, &up, &fragOff);

    //EOSP bit
    if (newFlag & 0x100)
    {
        up |= 0x10;
    }

#ifdef ZM_ENABLE_NATIVE_WIFI
    if ( wd->wlanMode == ZM_MODE_INFRASTRUCTURE )
    {
        /* DA */
        da[0] = zmw_tx_buf_readh(dev, buf, 16);
        da[1] = zmw_tx_buf_readh(dev, buf, 18);
        da[2] = zmw_tx_buf_readh(dev, buf, 20);
        /* SA */
        sa[0] = zmw_tx_buf_readh(dev, buf, 10);
        sa[1] = zmw_tx_buf_readh(dev, buf, 12);
        sa[2] = zmw_tx_buf_readh(dev, buf, 14);
    }
    else if ( wd->wlanMode == ZM_MODE_IBSS )
    {
        /* DA */
        da[0] = zmw_tx_buf_readh(dev, buf, 4);
        da[1] = zmw_tx_buf_readh(dev, buf, 6);
        da[2] = zmw_tx_buf_readh(dev, buf, 8);
        /* SA */
        sa[0] = zmw_tx_buf_readh(dev, buf, 10);
        sa[1] = zmw_tx_buf_readh(dev, buf, 12);
        sa[2] = zmw_tx_buf_readh(dev, buf, 14);
    }
    else if ( wd->wlanMode == ZM_MODE_AP )
    {
        /* DA */
        da[0] = zmw_tx_buf_readh(dev, buf, 4);
        da[1] = zmw_tx_buf_readh(dev, buf, 6);
        da[2] = zmw_tx_buf_readh(dev, buf, 8);
        /* SA */
        sa[0] = zmw_tx_buf_readh(dev, buf, 16);
        sa[1] = zmw_tx_buf_readh(dev, buf, 18);
        sa[2] = zmw_tx_buf_readh(dev, buf, 20);
    }
    else
    {
        //
    }
#else
    /* DA */
    da[0] = zmw_tx_buf_readh(dev, buf, 0);
    da[1] = zmw_tx_buf_readh(dev, buf, 2);
    da[2] = zmw_tx_buf_readh(dev, buf, 4);
    /* SA */
    sa[0] = zmw_tx_buf_readh(dev, buf, 6);
    sa[1] = zmw_tx_buf_readh(dev, buf, 8);
    sa[2] = zmw_tx_buf_readh(dev, buf, 10);
#endif
    //Decide Key Index in ATOM, No meaning in OTUS--CWYang(m)
    if (wd->wlanMode == ZM_MODE_AP)
    {
        keyIdx = wd->ap.bcHalKeyIdx[port];
        id = zfApFindSta(dev, da);
        if (id != 0xffff)
        {
            switch (wd->ap.staTable[id].encryMode)
            {
            case ZM_AES:
            case ZM_TKIP:
#ifdef ZM_ENABLE_CENC
            case ZM_CENC:
#endif //ZM_ENABLE_CENC
                keyIdx = wd->ap.staTable[id].keyIdx;
                break;
            }
        }
    }
    else
    {
        switch (wd->sta.encryMode)
        {
        case ZM_WEP64:
        case ZM_WEP128:
        case ZM_WEP256:
            keyIdx = wd->sta.keyId;
            break;
        case ZM_AES:
        case ZM_TKIP:
            if ((da[0] & 0x1))
                keyIdx = 5;
            else
                keyIdx = 4;
            break;
#ifdef ZM_ENABLE_CENC
        case ZM_CENC:
            keyIdx = wd->sta.cencKeyId;
            break;
#endif //ZM_ENABLE_CENC
        }
    }

    /* Create SNAP */
    removeLen = zfTxGenWlanSnap(dev, buf, snap, &snapLen);
    //zm_msg1_tx(ZM_LV_0, "fragOff=", fragOff);


/* ********************************************************************************************** */
/* Add 20071025 Mxzeng                                                                            */
/* ********************************************************************************************** */
/* ---------------------------------------------------------------------------------------------- */
/*  Ethernet : frameLen = zfwBufGetSize(dev, buf);                                                */
/* ---+--6--+--6--+--2--+-----20-----+-------------------------+------ Variable -------+--------- */
/*    |  DA |  SA | Type|  IP Header | TCP(20) UDP(12) ICMP(8) | Application Payload L |          */
/* ---+-----+-----+-----+------------+-------------------------+-----------------------+--------- */
/*  MSDU = 6 + 6 + 2 + ( Network Layer header ) + ( Transport Layer header ) + L                  */
/*                                                                                                */
/*  MSDU - DA - SA : frameLen -= removeLen;                                                       */
/* ---+--2--+-----20-----+-------------------------+------ Variable -------+--------------------- */
/*    | Type| IP Header  | TCP(20) UDP(12) ICMP(8) | Application Payload L |                      */
/* ---+-----+------------+-------------------------+-----------------------+--------------------- */
/*												  */
/*  MPDU : frameLen + mpduLengthOffset ;                                                          */
/* -+---2---+----2---+-6-+-6-+--6--+---2----+--1--+--1-+---1---+-------3------+-frameLen-+---4--+- */
/*  | frame |duration| DA|SA |BSSID|sequence|SNAP |SNAP|Control|    RFC 1042  |          |  FCS |  */
/*  |Control|        |   |   |     | number |DSAP |SSAP|       | encapsulation|          |      |  */
/* -+-------+--------+---+---+-----+--------+-----+----+-------+--------------+----------+------+- */
/* ----------------------------------------------------------------------------------------------- */

    if ( wd->sta.encryMode == ZM_TKIP )
        tkipFrameOffset = 8;

    fragLen = wd->fragThreshold + tkipFrameOffset;   // Fragmentation threshold for MPDU Lengths
    frameLen = zfwBufGetSize(dev, buf);    // MSDU Lengths
    frameLen -= removeLen;                 // MSDU Lengths - DA - SA

    /* #1st create MIC Length manually */
    micLen = 0;

    /* Access Category */
    if (wd->wlanMode == ZM_MODE_AP)
    {
        zfApGetStaQosType(dev, da, &qosType);
        if (qosType == 0)
        {
            up = 0;
        }
    }
    else if (wd->wlanMode == ZM_MODE_INFRASTRUCTURE)
    {
        if (wd->sta.wmeConnected == 0)
        {
            up = 0;
        }
    }
    else
    {
        /* TODO : STA QoS control field */
        up = 0;
    }

    /* #2nd Assign sequence number */
    zmw_enter_critical_section(dev);
    frag.seq[0] = ((wd->seq[zcUpToAc[up&0x7]]++) << 4);
    zmw_leave_critical_section(dev);

    /* #3rd Pass the total payload to generate MPDU length ! */
    frag.buf[0] = buf;
    frag.bufType[0] = bufType;
    frag.flag[0] = (u8_t)flag;
    fragNum = 1;

    headerLen = zfTxGenWlanHeader(dev, frag.buf[0], header, frag.seq[0],
                                  frag.flag[0], snapLen+micLen, removeLen, port, da, sa,
                                  up, &micLen, snap, snapLen, NULL);

    //zm_debug_msg1("#1 headerLen = ", headerLen);

    /* #4th Check the HeaderLen and determine whether the MPDU Lengths bigger than Fragmentation threshold  */
    /* If MPDU Lengths large than fragmentation threshold --> headerLen = 0 */
    if( headerLen != 0 )
    {
        zf80211FrameSend(dev, frag.buf[0], header, snapLen, da, sa, up,
                         headerLen, snap, mic, micLen, removeLen, frag.bufType[0],
                         zcUpToAc[up&0x7], keyIdx);
    }
    else //if( headerLen == 0 ) // Need to be fragmented
    {
        u16_t mpduLengthOffset;
        u16_t pseudSnapLen = 0;

        mpduLengthOffset = header[0] - frameLen; // For fragmentation threshold !

        micLen = zfTxGenWlanTail(dev, buf, snap, snapLen, mic); // Get snap and mic information

        fragLen = fragLen - mpduLengthOffset;

        //zm_debug_msg1("#2 frameLen = ", frameLen);
        //zm_debug_msg1("#3 fragThreshold = ", fragLen);

        /* fragmentation */
        if (frameLen >= fragLen)
        {
            //copy fragLen to frag
            i = 0;
            while( frameLen > 0 )
            {
                if ((frag.buf[i] = zfwBufAllocate(dev, fragLen+32)) != NULL)
                {
                    frag.bufType[i] = ZM_INTERNAL_ALLOC_BUF;
                    frag.seq[i] = frag.seq[0] + i;
                    offset = removeLen + i*fragLen;

                    /* Consider the offset if we consider snap length to the other fragmented frame */
                    if ( i >= 1 )
                        offset = offset + pseudSnapLen*(i-1);

                    if (frameLen > fragLen + pseudSnapLen)
                    {
                        frag.flag[i] = flag | 0x4; /* More data */
                        /* First fragment */
                        if (i == 0)
                        {
                            /* Add SNAP */
                            for (j=0; j<snapLen; j+=2)
                            {
                                zmw_tx_buf_writeh(dev, frag.buf[i], j, snap[(j>>1)]);
                            }
                            zfTxBufferCopy(dev, frag.buf[i], buf, snapLen, offset, fragLen);
                            zfwBufSetSize(dev, frag.buf[i], snapLen+fragLen);

                            /* Add pseud snap length to the other fragmented frame */
                            pseudSnapLen = snapLen;

                            frameLen -= fragLen;
                        }
                        /* Intermediate Fragment */
                        else
                        {
                            //zfTxBufferCopy(dev, frag.buf[i], buf, 0, offset, fragLen);
                            //zfwBufSetSize(dev, frag.buf[i], fragLen);

                            zfTxBufferCopy(dev, frag.buf[i], buf, 0, offset, fragLen+pseudSnapLen );
                            zfwBufSetSize(dev, frag.buf[i], fragLen+pseudSnapLen);

                            frameLen -= (fragLen+pseudSnapLen);
                        }
                        //frameLen -= fragLen;
                    }
                    else
                    {
                        /* Last fragment  */
                        zfTxBufferCopy(dev, frag.buf[i], buf, 0, offset, frameLen);
                        /* Add MIC if need */
                        if ( micLen )
                        {
                            zfCopyToRxBuffer(dev, frag.buf[i], (u8_t*) mic, frameLen, micLen);
                        }
                        zfwBufSetSize(dev, frag.buf[i], frameLen+micLen);
                        frameLen = 0;
                        frag.flag[i] = (u8_t)flag; /* No more data */
                    }
                    i++;
                }
                else
                {
                    break;
                }

                // Please pay attention to the index of the buf !!!
                // If write to null buf , the OS will crash !!!
                zfwCopyBufContext(dev, buf, frag.buf[i-1]);
            }
            fragNum = i;
            snapLen = micLen = removeLen = 0;

            zfwBufFree(dev, buf, 0);
        }

        for (i=0; i<fragNum; i++)
        {
            /* Create WLAN header(Control Setting + 802.11 header + IV) */
            headerLen = zfTxGenWlanHeader(dev, frag.buf[i], header, frag.seq[i],
                                    frag.flag[i], snapLen+micLen, removeLen, port, da, sa, up, &micLen,
                                    snap, snapLen, NULL);

            zf80211FrameSend(dev, frag.buf[i], header, snapLen, da, sa, up,
                             headerLen, snap, mic, micLen, removeLen, frag.bufType[i],
                             zcUpToAc[up&0x7], keyIdx);

        } /* for (i=0; i<fragNum; i++) */
    }

    return ZM_SUCCESS;
}


/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfTxPortControl             */
/*      Check port status.                                              */
/*                                                                      */
/*    INPUTS                                                            */
/*      buf : buffer pointer                                            */
/*      port : port number, 0=>standard, 10-17=>Virtual AP, 20-25=>WDS  */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      ZM_PORT_ENABLED or ZM_PORT_DISABLE                              */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Signature           ZyDAS Technology Corporation    2005.4      */
/*                                                                      */
/************************************************************************/
u16_t zfTxPortControl(zdev_t* dev, zbuf_t* buf, u16_t port)
{
    zmw_get_wlan_dev(dev);

    if ( wd->wlanMode == ZM_MODE_INFRASTRUCTURE )
    {
        if ( wd->sta.adapterState == ZM_STA_STATE_DISCONNECT )
        {
            zm_msg0_tx(ZM_LV_3, "Packets dropped due to disconnect state");
            return ZM_PORT_DISABLED;
        }
    }

    return ZM_PORT_ENABLED;
}



/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfIdlRecv                   */
/*      Do frame validation and filtering then pass to zfwRecv80211().  */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      buf : received 802.11 frame buffer.                             */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      None                                                            */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen             ZyDAS Technology Corporation    2005.10     */
/*                                                                      */
/************************************************************************/
void zfCoreRecv(zdev_t* dev, zbuf_t* buf, struct zsAdditionInfo* addInfo)
{
    u16_t ret = 0;
    u16_t bssid[3];
    struct agg_tid_rx *tid_rx;
    zmw_get_wlan_dev(dev);

    ZM_BUFFER_TRACE(dev, buf)

    /* tally */
    wd->commTally.DriverRxFrmCnt++;

    bssid[0] = zmw_buf_readh(dev, buf, 16);
    bssid[1] = zmw_buf_readh(dev, buf, 18);
    bssid[2] = zmw_buf_readh(dev, buf, 20);

    /* Validate Rx frame */
    if ((ret = zfWlanRxValidate(dev, buf)) != ZM_SUCCESS)
    {
        zm_msg1_rx(ZM_LV_1, "Rx invalid:", ret);
        goto zlError;
    }

#ifdef ZM_ENABLE_AGGREGATION
    //#ifndef ZM_ENABLE_FW_BA_RETRANSMISSION
    /*
     * add by honda
     */
    tid_rx = zfAggRxEnabled(dev, buf);
    if (tid_rx && wd->reorder)
    {
        zfAggRx(dev, buf, addInfo, tid_rx);

        return;
    }
    /*
     * end of add by honda
     */
    //#endif
#endif

    /* Filter Rx frame */
    if ((ret = zfWlanRxFilter(dev, buf)) != ZM_SUCCESS)
    {
        zm_msg1_rx(ZM_LV_1, "Rx duplicated:", ret);
        goto zlError;
    }

    /* Discard error frame except mic failure */
    if ((addInfo->Tail.Data.ErrorIndication & 0x3f) != 0)
    {
        if ( wd->XLinkMode && ((addInfo->Tail.Data.ErrorIndication & 0x3f)==0x10) &&
             zfCompareWithBssid(dev, bssid) )
        {
            // Bypass frames !!!
        }
        else
        {
            goto zlError;
        }
    }


    /* OTUS command-8212 dump rx packet */
    if (wd->rxPacketDump)
    {
        zfwDumpBuf(dev, buf);
    }

    /* Call zfwRecv80211() wrapper function to deliver Rx packet */
    /* to driver framework.                                      */

    if (wd->zfcbRecv80211 != NULL)
    {
        wd->zfcbRecv80211(dev, buf, addInfo); //CWYang(m)
    }
    else
    {
        zfiRecv80211(dev, buf, addInfo);
    }
    return;

zlError:
    zm_msg1_rx(ZM_LV_1, "Free packet, error code:", ret);

    wd->commTally.DriverDiscardedFrm++;

    /* Free Rx buffer */
    zfwBufFree(dev, buf, 0);

    return;
}


void zfShowRxEAPOL(zdev_t* dev, zbuf_t* buf, u16_t offset)
{
    u8_t   packetType, keyType, code, identifier, type, flags;
    u16_t  packetLen, keyInfo, keyLen, keyDataLen, length, Op_Code;
    u32_t  replayCounterH, replayCounterL, vendorId, VendorType;

    /* EAPOL packet type */
    packetType = zmw_rx_buf_readb(dev, buf, offset+1); // 0: EAP-Packet
                                                       // 1: EAPOL-Start
                                                       // 2: EAPOL-Logoff
                                                       // 3: EAPOL-Key
                                                       // 4: EAPOL-Encapsulated-ASF-Alert

    /* EAPOL frame format */
    /*  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15   */
    /* -----------------------------------------------   */
    /*            PAE Ethernet Type (0x888e)             */
    /* ----------------------------------------------- 2 */
    /*     Protocol Version    |         Type            */
    /* ----------------------------------------------- 4 */
    /*                       Length                      */
    /* ----------------------------------------------- 6 */
    /*                    Packet Body                    */
    /* ----------------------------------------------- N */

    /* EAPOL body length */
    packetLen = (((u16_t) zmw_rx_buf_readb(dev, buf, offset+2)) << 8) +
                zmw_rx_buf_readb(dev, buf, offset+3);

    if( packetType == 0 )
    { // EAP-Packet

        /* EAP-Packet Code */
        code = zmw_rx_buf_readb(dev, buf, offset+4); // 1 : Request
                                                     // 2 : Response
                                                     // 3 : Success
                                                     // 4 : Failure
        // An EAP packet of the type of Success and Failure has no Data field, and has a length of 4.

        /* EAP Packet format */
        /*  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15   */
        /* -----------------------------------------------   */
        /*           Code          |        Identifier       */
        /* ----------------------------------------------- 2 */
        /*                       Length                      */
        /* ----------------------------------------------- 4 */
        /*                        Data                       */
        /* ----------------------------------------------- N */

        zm_debug_msg0("EAP-Packet");
        zm_debug_msg1("Packet Length = ", packetLen);
        zm_debug_msg1("EAP-Packet Code = ", code);

        if( code == 1 )
        {
            zm_debug_msg0("EAP-Packet Request");

            /* EAP-Packet Identifier */
            identifier = zmw_rx_buf_readb(dev, buf, offset+5);
            /* EAP-Packet Length */
            length = (((u16_t) zmw_rx_buf_readb(dev, buf, offset+6)) << 8) +
                      zmw_rx_buf_readb(dev, buf, offset+7);
            /* EAP-Packet Type */
            type = zmw_rx_buf_readb(dev, buf, offset+8); // 1   : Identity
                                                         // 2   : Notification
                                                         // 3   : Nak (Response Only)
                                                         // 4   : MD5-Challenge
                                                         // 5   : One Time Password (OTP)
                                                         // 6   : Generic Token Card (GTC)
                                                         // 254 : (Expanded Types)Wi-Fi Protected Setup
                                                         // 255 : Experimental Use

            /* The data field in an EAP packet of the type of Request or Response is in the format shown bellowing */
            /*  0  1  2  3  4  5  6  7             N             */
            /* -----------------------------------------------   */
            /*           Type          |        Type Data        */
            /* -----------------------------------------------   */

            zm_debug_msg1("EAP-Packet Identifier = ", identifier);
            zm_debug_msg1("EAP-Packet Length = ", length);
            zm_debug_msg1("EAP-Packet Type = ", type);

            if( type == 1 )
            {
                zm_debug_msg0("EAP-Packet Request Identity");
            }
            else if( type == 2 )
            {
                zm_debug_msg0("EAP-Packet Request Notification");
            }
            else if( type == 4 )
            {
                zm_debug_msg0("EAP-Packet Request MD5-Challenge");
            }
            else if( type == 5 )
            {
                zm_debug_msg0("EAP-Packet Request One Time Password");
            }
            else if( type == 6 )
            {
                zm_debug_msg0("EAP-Packet Request Generic Token Card");
            }
            else if( type == 254 )
            {
                zm_debug_msg0("EAP-Packet Request Wi-Fi Protected Setup");

                /* 0                   1                   2                   3   */
                /* 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 */
                /*+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/
                /*|     Type      |               Vendor-Id                       |*/
                /*+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/
                /*|                          Vendor-Type                          |*/
                /*+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/
                /*|              Vendor data...                                    */
                /*+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                        */

                /* EAP-Packet Vendor ID */
                vendorId = (((u32_t) zmw_rx_buf_readb(dev, buf, offset+9)) << 16) +
                           (((u32_t) zmw_rx_buf_readb(dev, buf, offset+10)) << 8) +
                           zmw_rx_buf_readb(dev, buf, offset+11);
                /* EAP-Packet Vendor Type */
                VendorType = (((u32_t) zmw_rx_buf_readb(dev, buf, offset+12)) << 24) +
                             (((u32_t) zmw_rx_buf_readb(dev, buf, offset+13)) << 16) +
                             (((u32_t) zmw_rx_buf_readb(dev, buf, offset+14)) << 8) +
                             zmw_rx_buf_readb(dev, buf, offset+15);
                /* EAP-Packet Op Code */
                Op_Code = (((u16_t) zmw_rx_buf_readb(dev, buf, offset+16)) << 8) +
                          zmw_rx_buf_readb(dev, buf, offset+17);
                /* EAP-Packet Flags */
                flags = zmw_rx_buf_readb(dev, buf, offset+18);

                zm_debug_msg1("EAP-Packet Vendor ID = ", vendorId);
                zm_debug_msg1("EAP-Packet Venodr Type = ", VendorType);
                zm_debug_msg1("EAP-Packet Op Code = ", Op_Code);
                zm_debug_msg1("EAP-Packet Flags = ", flags);
            }
        }
        else if( code == 2 )
        {
            zm_debug_msg0("EAP-Packet Response");

            /* EAP-Packet Identifier */
            identifier = zmw_rx_buf_readb(dev, buf, offset+5);
            /* EAP-Packet Length */
            length = (((u16_t) zmw_rx_buf_readb(dev, buf, offset+6)) << 8) +
                      zmw_rx_buf_readb(dev, buf, offset+7);
            /* EAP-Packet Type */
            type = zmw_rx_buf_readb(dev, buf, offset+8);

            zm_debug_msg1("EAP-Packet Identifier = ", identifier);
            zm_debug_msg1("EAP-Packet Length = ", length);
            zm_debug_msg1("EAP-Packet Type = ", type);

            if( type == 1 )
            {
                zm_debug_msg0("EAP-Packet Response Identity");
            }
            else if( type == 2 )
            {
                zm_debug_msg0("EAP-Packet Request Notification");
            }
            else if( type == 3 )
            {
                zm_debug_msg0("EAP-Packet Request Nak");
            }
            else if( type == 4 )
            {
                zm_debug_msg0("EAP-Packet Request MD5-Challenge");
            }
            else if( type == 5 )
            {
                zm_debug_msg0("EAP-Packet Request One Time Password");
            }
            else if( type == 6 )
            {
                zm_debug_msg0("EAP-Packet Request Generic Token Card");
            }
            else if( type == 254 )
            {
                zm_debug_msg0("EAP-Packet Response Wi-Fi Protected Setup");

                /* EAP-Packet Vendor ID */
                vendorId = (((u32_t) zmw_rx_buf_readb(dev, buf, offset+9)) << 16) +
                           (((u32_t) zmw_rx_buf_readb(dev, buf, offset+10)) << 8) +
                           zmw_rx_buf_readb(dev, buf, offset+11);
                /* EAP-Packet Vendor Type */
                VendorType = (((u32_t) zmw_rx_buf_readb(dev, buf, offset+12)) << 24) +
                             (((u32_t) zmw_rx_buf_readb(dev, buf, offset+13)) << 16) +
                             (((u32_t) zmw_rx_buf_readb(dev, buf, offset+14)) << 8) +
                             zmw_rx_buf_readb(dev, buf, offset+15);
                /* EAP-Packet Op Code */
                Op_Code = (((u16_t) zmw_rx_buf_readb(dev, buf, offset+16)) << 8) +
                          zmw_rx_buf_readb(dev, buf, offset+17);
                /* EAP-Packet Flags */
                flags = zmw_rx_buf_readb(dev, buf, offset+18);

                zm_debug_msg1("EAP-Packet Vendor ID = ", vendorId);
                zm_debug_msg1("EAP-Packet Venodr Type = ", VendorType);
                zm_debug_msg1("EAP-Packet Op Code = ", Op_Code);
                zm_debug_msg1("EAP-Packet Flags = ", flags);
            }
        }
        else if( code == 3 )
        {
            zm_debug_msg0("EAP-Packet Success");

            /* EAP-Packet Identifier */
            identifier = zmw_rx_buf_readb(dev, buf, offset+5);
            /* EAP-Packet Length */
            length = (((u16_t) zmw_rx_buf_readb(dev, buf, offset+6)) << 8) +
                      zmw_rx_buf_readb(dev, buf, offset+7);

            zm_debug_msg1("EAP-Packet Identifier = ", identifier);
            zm_debug_msg1("EAP-Packet Length = ", length);
        }
        else if( code == 4 )
        {
            zm_debug_msg0("EAP-Packet Failure");

            /* EAP-Packet Identifier */
            identifier = zmw_rx_buf_readb(dev, buf, offset+5);
            /* EAP-Packet Length */
            length = (((u16_t) zmw_rx_buf_readb(dev, buf, offset+6)) << 8) +
                      zmw_rx_buf_readb(dev, buf, offset+7);

            zm_debug_msg1("EAP-Packet Identifier = ", identifier);
            zm_debug_msg1("EAP-Packet Length = ", length);
        }
    }
    else if( packetType == 1 )
    { // EAPOL-Start
        zm_debug_msg0("EAPOL-Start");
    }
    else if( packetType == 2 )
    { // EAPOL-Logoff
        zm_debug_msg0("EAPOL-Logoff");
    }
    else if( packetType == 3 )
    { // EAPOL-Key
        /* EAPOL-Key type */
        keyType = zmw_rx_buf_readb(dev, buf, offset+4);
        /* EAPOL-Key information */
        keyInfo = (((u16_t) zmw_rx_buf_readb(dev, buf, offset+5)) << 8) +
                  zmw_rx_buf_readb(dev, buf, offset+6);
        /* EAPOL-Key length */
        keyLen = (((u16_t) zmw_rx_buf_readb(dev, buf, offset+7)) << 8) +
                 zmw_rx_buf_readb(dev, buf, offset+8);
        /* EAPOL-Key replay counter (high double word) */
        replayCounterH = (((u32_t) zmw_rx_buf_readb(dev, buf, offset+9)) << 24) +
                         (((u32_t) zmw_rx_buf_readb(dev, buf, offset+10)) << 16) +
                         (((u32_t) zmw_rx_buf_readb(dev, buf, offset+11)) << 8) +
                         zmw_rx_buf_readb(dev, buf, offset+12);
        /* EAPOL-Key replay counter (low double word) */
        replayCounterL = (((u32_t) zmw_rx_buf_readb(dev, buf, offset+13)) << 24) +
                         (((u32_t) zmw_rx_buf_readb(dev, buf, offset+14)) << 16) +
                         (((u32_t) zmw_rx_buf_readb(dev, buf, offset+15)) << 8) +
                         zmw_rx_buf_readb(dev, buf, offset+16);
        /* EAPOL-Key data length */
        keyDataLen = (((u16_t) zmw_rx_buf_readb(dev, buf, offset+97)) << 8) +
                     zmw_rx_buf_readb(dev, buf, offset+98);

        zm_debug_msg0("EAPOL-Key");
        zm_debug_msg1("packet length = ", packetLen);

        if ( keyType == 254 )
        {
            zm_debug_msg0("key type = 254 (SSN key descriptor)");
        }
        else
        {
            zm_debug_msg2("key type = 0x", keyType);
        }

        zm_debug_msg2("replay counter(L) = ", replayCounterL);

        zm_debug_msg2("key information = ", keyInfo);

        if ( keyInfo & ZM_BIT_3 )
        {
            zm_debug_msg0("    - pairwise key");
        }
        else
        {
            zm_debug_msg0("    - group key");
        }

        if ( keyInfo & ZM_BIT_6 )
        {
            zm_debug_msg0("    - Tx key installed");
        }
        else
        {
            zm_debug_msg0("    - Tx key not set");
        }

        if ( keyInfo & ZM_BIT_7 )
        {
            zm_debug_msg0("    - Ack needed");
        }
        else
        {
            zm_debug_msg0("    - Ack not needed");
        }

        if ( keyInfo & ZM_BIT_8 )
        {
            zm_debug_msg0("    - MIC set");
        }
        else
        {
            zm_debug_msg0("    - MIC not set");
        }

        if ( keyInfo & ZM_BIT_9 )
        {
            zm_debug_msg0("    - packet encrypted");
        }
        else
        {
            zm_debug_msg0("    - packet not encrypted");
        }

        zm_debug_msg1("keyLen = ", keyLen);
        zm_debug_msg1("keyDataLen = ", keyDataLen);
    }
    else if( packetType == 4 )
    {
        zm_debug_msg0("EAPOL-Encapsulated-ASF-Alert");
    }
}

void zfShowTxEAPOL(zdev_t* dev, zbuf_t* buf, u16_t offset)
{
    u8_t   packetType, keyType, code, identifier, type, flags;
    u16_t  packetLen, keyInfo, keyLen, keyDataLen, length, Op_Code;
    u32_t  replayCounterH, replayCounterL, vendorId, VendorType;

    zmw_get_wlan_dev(dev);

    zm_debug_msg1("EAPOL Packet size = ", zfwBufGetSize(dev, buf));

    /* EAPOL packet type */
    // 0: EAP-Packet
    // 1: EAPOL-Start
    // 2: EAPOL-Logoff
    // 3: EAPOL-Key
    // 4: EAPOL-Encapsulated-ASF-Alert

    /* EAPOL frame format */
    /*  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15   */
    /* -----------------------------------------------   */
    /*            PAE Ethernet Type (0x888e)             */
    /* ----------------------------------------------- 2 */
    /*     Protocol Version    |         Type            */
    /* ----------------------------------------------- 4 */
    /*                       Length                      */
    /* ----------------------------------------------- 6 */
    /*                    Packet Body                    */
    /* ----------------------------------------------- N */

    packetType = zmw_tx_buf_readb(dev, buf, offset+1);
    /* EAPOL body length */
    packetLen = (((u16_t) zmw_tx_buf_readb(dev, buf, offset+2)) << 8) +
                zmw_tx_buf_readb(dev, buf, offset+3);

    if( packetType == 0 )
    { // EAP-Packet
        /* EAP-Packet Code */
        code = zmw_tx_buf_readb(dev, buf, offset+4); // 1 : Request
                                                     // 2 : Response
                                                     // 3 : Success
                                                     // 4 : Failure

        // An EAP packet of the type of Success and Failure has no Data field, and has a length of 4.

        /* EAP Packet format */
        /*  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15   */
        /* -----------------------------------------------   */
        /*           Code          |        Identifier       */
        /* ----------------------------------------------- 2 */
        /*                       Length                      */
        /* ----------------------------------------------- 4 */
        /*                        Data                       */
        /* ----------------------------------------------- N */

        zm_debug_msg0("EAP-Packet");
        zm_debug_msg1("Packet Length = ", packetLen);
        zm_debug_msg1("EAP-Packet Code = ", code);

        if( code == 1 )
        {
            zm_debug_msg0("EAP-Packet Request");

            /* EAP-Packet Identifier */
            identifier = zmw_tx_buf_readb(dev, buf, offset+5);
            /* EAP-Packet Length */
            length = (((u16_t) zmw_tx_buf_readb(dev, buf, offset+6)) << 8) +
                      zmw_tx_buf_readb(dev, buf, offset+7);
            /* EAP-Packet Type */
            type = zmw_tx_buf_readb(dev, buf, offset+8); // 1   : Identity
                                                         // 2   : Notification
                                                         // 3   : Nak (Response Only)
                                                         // 4   : MD5-Challenge
                                                         // 5   : One Time Password (OTP)
                                                         // 6   : Generic Token Card (GTC)
                                                         // 254 : (Expanded Types)Wi-Fi Protected Setup
                                                         // 255 : Experimental Use

            /* The data field in an EAP packet of the type of Request or Response is in the format shown bellowing */
            /*  0  1  2  3  4  5  6  7             N             */
            /* -----------------------------------------------   */
            /*           Type          |        Type Data        */
            /* -----------------------------------------------   */

            zm_debug_msg1("EAP-Packet Identifier = ", identifier);
            zm_debug_msg1("EAP-Packet Length = ", length);
            zm_debug_msg1("EAP-Packet Type = ", type);

            if( type == 1 )
            {
                zm_debug_msg0("EAP-Packet Request Identity");
            }
            else if( type == 2 )
            {
                zm_debug_msg0("EAP-Packet Request Notification");
            }
            else if( type == 4 )
            {
                zm_debug_msg0("EAP-Packet Request MD5-Challenge");
            }
            else if( type == 5 )
            {
                zm_debug_msg0("EAP-Packet Request One Time Password");
            }
            else if( type == 6 )
            {
                zm_debug_msg0("EAP-Packet Request Generic Token Card");
            }
            else if( type == 254 )
            {
                zm_debug_msg0("EAP-Packet Request Wi-Fi Protected Setup");

                /* 0                   1                   2                   3   */
                /* 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 */
                /*+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/
                /*|     Type      |               Vendor-Id                       |*/
                /*+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/
                /*|                          Vendor-Type                          |*/
                /*+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/
                /*|              Vendor data...                                    */
                /*+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                        */

                /* EAP-Packet Vendor ID */
                vendorId = (((u32_t) zmw_tx_buf_readb(dev, buf, offset+9)) << 16) +
                           (((u32_t) zmw_tx_buf_readb(dev, buf, offset+10)) << 8) +
                           zmw_tx_buf_readb(dev, buf, offset+11);
                /* EAP-Packet Vendor Type */
                VendorType = (((u32_t) zmw_tx_buf_readb(dev, buf, offset+12)) << 24) +
                             (((u32_t) zmw_tx_buf_readb(dev, buf, offset+13)) << 16) +
                             (((u32_t) zmw_tx_buf_readb(dev, buf, offset+14)) << 8) +
                             zmw_tx_buf_readb(dev, buf, offset+15);
                /* EAP-Packet Op Code */
                Op_Code = (((u16_t) zmw_tx_buf_readb(dev, buf, offset+16)) << 8) +
                          zmw_tx_buf_readb(dev, buf, offset+17);
                /* EAP-Packet Flags */
                flags = zmw_tx_buf_readb(dev, buf, offset+18);

                zm_debug_msg1("EAP-Packet Vendor ID = ", vendorId);
                zm_debug_msg1("EAP-Packet Venodr Type = ", VendorType);
                zm_debug_msg1("EAP-Packet Op Code = ", Op_Code);
                zm_debug_msg1("EAP-Packet Flags = ", flags);
            }
        }
        else if( code == 2 )
        {
            zm_debug_msg0("EAP-Packet Response");

            /* EAP-Packet Identifier */
            identifier = zmw_tx_buf_readb(dev, buf, offset+5);
            /* EAP-Packet Length */
            length = (((u16_t) zmw_tx_buf_readb(dev, buf, offset+6)) << 8) +
                      zmw_tx_buf_readb(dev, buf, offset+7);
            /* EAP-Packet Type */
            type = zmw_tx_buf_readb(dev, buf, offset+8);

            zm_debug_msg1("EAP-Packet Identifier = ", identifier);
            zm_debug_msg1("EAP-Packet Length = ", length);
            zm_debug_msg1("EAP-Packet Type = ", type);

            if( type == 1 )
            {
                zm_debug_msg0("EAP-Packet Response Identity");
            }
            else if( type == 2 )
            {
                zm_debug_msg0("EAP-Packet Request Notification");
            }
            else if( type == 3 )
            {
                zm_debug_msg0("EAP-Packet Request Nak");
            }
            else if( type == 4 )
            {
                zm_debug_msg0("EAP-Packet Request MD5-Challenge");
            }
            else if( type == 5 )
            {
                zm_debug_msg0("EAP-Packet Request One Time Password");
            }
            else if( type == 6 )
            {
                zm_debug_msg0("EAP-Packet Request Generic Token Card");
            }
            else if( type == 254 )
            {
                zm_debug_msg0("EAP-Packet Response Wi-Fi Protected Setup");

                /* EAP-Packet Vendor ID */
                vendorId = (((u32_t) zmw_tx_buf_readb(dev, buf, offset+9)) << 16) +
                           (((u32_t) zmw_tx_buf_readb(dev, buf, offset+10)) << 8) +
                           zmw_tx_buf_readb(dev, buf, offset+11);
                /* EAP-Packet Vendor Type */
                VendorType = (((u32_t) zmw_tx_buf_readb(dev, buf, offset+12)) << 24) +
                             (((u32_t) zmw_tx_buf_readb(dev, buf, offset+13)) << 16) +
                             (((u32_t) zmw_tx_buf_readb(dev, buf, offset+14)) << 8) +
                             zmw_tx_buf_readb(dev, buf, offset+15);
                /* EAP-Packet Op Code */
                Op_Code = (((u16_t) zmw_tx_buf_readb(dev, buf, offset+16)) << 8) +
                          zmw_tx_buf_readb(dev, buf, offset+17);
                /* EAP-Packet Flags */
                flags = zmw_tx_buf_readb(dev, buf, offset+18);

                zm_debug_msg1("EAP-Packet Vendor ID = ", vendorId);
                zm_debug_msg1("EAP-Packet Venodr Type = ", VendorType);
                zm_debug_msg1("EAP-Packet Op Code = ", Op_Code);
                zm_debug_msg1("EAP-Packet Flags = ", flags);
            }
        }
        else if( code == 3 )
        {
            zm_debug_msg0("EAP-Packet Success");

            /* EAP-Packet Identifier */
            identifier = zmw_rx_buf_readb(dev, buf, offset+5);
            /* EAP-Packet Length */
            length = (((u16_t) zmw_rx_buf_readb(dev, buf, offset+6)) << 8) +
                      zmw_rx_buf_readb(dev, buf, offset+7);

            zm_debug_msg1("EAP-Packet Identifier = ", identifier);
            zm_debug_msg1("EAP-Packet Length = ", length);
        }
        else if( code == 4 )
        {
            zm_debug_msg0("EAP-Packet Failure");

            /* EAP-Packet Identifier */
            identifier = zmw_tx_buf_readb(dev, buf, offset+5);
            /* EAP-Packet Length */
            length = (((u16_t) zmw_tx_buf_readb(dev, buf, offset+6)) << 8) +
                      zmw_tx_buf_readb(dev, buf, offset+7);

            zm_debug_msg1("EAP-Packet Identifier = ", identifier);
            zm_debug_msg1("EAP-Packet Length = ", length);
        }
    }
    else if( packetType == 1 )
    { // EAPOL-Start
        zm_debug_msg0("EAPOL-Start");
    }
    else if( packetType == 2 )
    { // EAPOL-Logoff
        zm_debug_msg0("EAPOL-Logoff");
    }
    else if( packetType == 3 )
    { // EAPOL-Key
        /* EAPOL-Key type */
        keyType = zmw_tx_buf_readb(dev, buf, offset+4);
        /* EAPOL-Key information */
        keyInfo = (((u16_t) zmw_tx_buf_readb(dev, buf, offset+5)) << 8) +
                  zmw_tx_buf_readb(dev, buf, offset+6);
        /* EAPOL-Key length */
        keyLen = (((u16_t) zmw_tx_buf_readb(dev, buf, offset+7)) << 8) +
                 zmw_tx_buf_readb(dev, buf, offset+8);
        /* EAPOL-Key replay counter (high double word) */
        replayCounterH = (((u32_t) zmw_tx_buf_readb(dev, buf, offset+9)) << 24) +
                         (((u32_t) zmw_tx_buf_readb(dev, buf, offset+10)) << 16) +
                         (((u32_t) zmw_tx_buf_readb(dev, buf, offset+11)) << 8) +
                         zmw_tx_buf_readb(dev, buf, offset+12);
        /* EAPOL-Key replay counter (low double word) */
        replayCounterL = (((u32_t) zmw_tx_buf_readb(dev, buf, offset+13)) << 24) +
                         (((u32_t) zmw_tx_buf_readb(dev, buf, offset+14)) << 16) +
                         (((u32_t) zmw_tx_buf_readb(dev, buf, offset+15)) << 8) +
                         zmw_tx_buf_readb(dev, buf, offset+16);
        /* EAPOL-Key data length */
        keyDataLen = (((u16_t) zmw_tx_buf_readb(dev, buf, offset+97)) << 8) +
                     zmw_tx_buf_readb(dev, buf, offset+98);

        zm_debug_msg0("EAPOL-Key");
        zm_debug_msg1("packet length = ", packetLen);

        if ( keyType == 254 )
        {
            zm_debug_msg0("key type = 254 (SSN key descriptor)");
        }
        else
        {
            zm_debug_msg2("key type = 0x", keyType);
        }

        zm_debug_msg2("replay counter(L) = ", replayCounterL);

        zm_debug_msg2("key information = ", keyInfo);

        if ( keyInfo & ZM_BIT_3 )
        {
            zm_debug_msg0("    - pairwise key");
        }
        else
        {
            zm_debug_msg0("    - group key");
        }

        if ( keyInfo & ZM_BIT_6 )
        {
            zm_debug_msg0("    - Tx key installed");
        }
        else
        {
            zm_debug_msg0("    - Tx key not set");
        }

        if ( keyInfo & ZM_BIT_7 )
        {
            zm_debug_msg0("    - Ack needed");
        }
        else
        {
            zm_debug_msg0("    - Ack not needed");
        }

        if ( keyInfo & ZM_BIT_8 )
        {
            zm_debug_msg0("    - MIC set");
        }
        else
        {
            zm_debug_msg0("    - MIC not set");
        }

        if ( keyInfo & ZM_BIT_9 )
        {
            zm_debug_msg0("    - packet encrypted");
        }
        else
        {
            zm_debug_msg0("    - packet not encrypted");
        }

        zm_debug_msg1("keyLen = ", keyLen);
        zm_debug_msg1("keyDataLen = ", keyDataLen);
    }
    else if( packetType == 4 )
    {
        zm_debug_msg0("EAPOL-Encapsulated-ASF-Alert");
    }
}


/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfiRecv80211                */
/*      Called to receive 802.11 frame.                                 */
/*                                                                      */
/*    INPUTS                                                            */
/*      dev : device pointer                                            */
/*      buf : received 802.11 frame buffer.                             */
/*                                                                      */
/*    OUTPUTS                                                           */
/*      None                                                            */
/*                                                                      */
/*    AUTHOR                                                            */
/*      Stephen             ZyDAS Technology Corporation    2005.5      */
/*                                                                      */
/************************************************************************/
void zfiRecv80211(zdev_t* dev, zbuf_t* buf, struct zsAdditionInfo* addInfo)
{
    u8_t snapCase=0, encryMode;
    u16_t frameType, typeLengthField;
    u16_t frameCtrl;
    u16_t frameSubtype;
    u16_t ret;
    u16_t len;
    u8_t bIsDefrag = 0;
    u16_t offset, tailLen;
    u8_t vap = 0;
    u16_t da[3], sa[3];
    u16_t ii;
    u8_t uapsdTrig = 0;
    zbuf_t* psBuf;
#ifdef ZM_ENABLE_NATIVE_WIFI
    u8_t i;
#endif

    zmw_get_wlan_dev(dev);

    ZM_BUFFER_TRACE(dev, buf)

    //zm_msg2_rx(ZM_LV_2, "zfiRecv80211(), buf=", buf);

    //zm_msg2_rx(ZM_LV_0, "h[0]=", zmw_rx_buf_readh(dev, buf, 0));
    //zm_msg2_rx(ZM_LV_0, "h[2]=", zmw_rx_buf_readh(dev, buf, 2));
    //zm_msg2_rx(ZM_LV_0, "h[4]=", zmw_rx_buf_readh(dev, buf, 4));

    frameCtrl = zmw_rx_buf_readb(dev, buf, 0);
    frameType = frameCtrl & 0xf;
    frameSubtype = frameCtrl & 0xf0;

#if 0   // Move to ProcessBeacon to judge if there's a new peer station
    if ( (wd->wlanMode == ZM_MODE_IBSS)&&
         (wd->sta.ibssPartnerStatus != ZM_IBSS_PARTNER_ALIVE) )
    {
        zfStaIbssMonitoring(dev, buf);
    }
#endif

    /* If data frame */
    if (frameType == ZM_WLAN_DATA_FRAME)
    {
        wd->sta.TotalNumberOfReceivePackets++;
        wd->sta.TotalNumberOfReceiveBytes += zfwBufGetSize(dev, buf);
        //zm_debug_msg1("Receive packets     = ", wd->sta.TotalNumberOfReceivePackets);

        //zm_msg0_rx(ZM_LV_0, "Rx data");
        if (wd->wlanMode == ZM_MODE_AP)
        {
            if ((ret = zfApUpdatePsBit(dev, buf, &vap, &uapsdTrig)) != ZM_SUCCESS)
            {
                zfwBufFree(dev, buf, 0);
                return;
            }

            if (((uapsdTrig&0xf) != 0) && ((frameSubtype & 0x80) != 0))
            {
                u8_t ac = zcUpToAc[zmw_buf_readb(dev, buf, 24)&0x7];
                u8_t pktNum;
                u8_t mb;
                u16_t flag;
                u8_t src[6];

                //printk("QoS ctrl=%d\n", zmw_buf_readb(dev, buf, 24));
                //printk("UAPSD trigger, ac=%d\n", ac);

                if (((0x8>>ac) & uapsdTrig) != 0)
                {
                    pktNum = zcMaxspToPktNum[(uapsdTrig>>4) & 0x3];

                    for (ii=0; ii<6; ii++)
                    {
                        src[ii] = zmw_buf_readb(dev, buf, ZM_WLAN_HEADER_A2_OFFSET+ii);
                    }

                    for (ii=0; ii<pktNum; ii++)
                    {
                        //if ((psBuf = zfQueueGet(dev, wd->ap.uapsdQ)) != NULL)
                        if ((psBuf = zfQueueGetWithMac(dev, wd->ap.uapsdQ, src, &mb)) != NULL)
                        {
                            if ((ii+1) == pktNum)
                            {
                                //EOSP anyway
                                flag = 0x100 | (mb<<5);
                            }
                            else
                            {
                                if (mb != 0)
                                {
                                    //more data, not EOSP
                                    flag = 0x20;
                                }
                                else
                                {
                                    //no more data, EOSP
                                    flag = 0x100;
                                }
                            }
                            zfTxSendEth(dev, psBuf, 0, ZM_EXTERNAL_ALLOC_BUF, flag);
                        }

                        if ((psBuf == NULL) || (mb == 0))
                        {
                            if ((ii == 0) && (psBuf == NULL))
                            {
                                zfSendMmFrame(dev, ZM_WLAN_FRAME_TYPE_QOS_NULL, (u16_t*)src, 0, 0, 0);
                            }
                            break;
                        }
                    }
                }
            }

        }
        else if ( wd->wlanMode == ZM_MODE_INFRASTRUCTURE )
        {
            u16_t frameCtrlMSB;
    		u8_t   bssid[6];

            /* Check Is RIFS frame and decide to enable RIFS or not */
            if( wd->sta.EnableHT )
                zfCheckIsRIFSFrame(dev, buf, frameSubtype);

            if ( zfPowerSavingMgrIsSleeping(dev) || wd->sta.psMgr.tempWakeUp == 1)
            {
                frameCtrlMSB = zmw_rx_buf_readb(dev, buf, 1);

                /* check more data */
                if ( frameCtrlMSB & ZM_BIT_5 )
                {
                    //if rx frame's AC is not delivery-enabled
                    if ((wd->sta.qosInfo&0xf) != 0xf)
                    {
                        u8_t rxAc = 0;
                        if ((frameSubtype & 0x80) != 0)
                        {
                            rxAc = zcUpToAc[zmw_buf_readb(dev, buf, 24)&0x7];
                        }

                        if (((0x8>>rxAc) & wd->sta.qosInfo) == 0)
                        {
                            zfSendPSPoll(dev);
                            wd->sta.psMgr.tempWakeUp = 0;
                        }
                    }
                }
            }
			/*increase beacon count when receive vaild data frame from AP*/
        	ZM_MAC_WORD_TO_BYTE(wd->sta.bssid, bssid);

			if (zfStaIsConnected(dev)&&
				zfRxBufferEqualToStr(dev, buf, bssid, ZM_WLAN_HEADER_A2_OFFSET, 6))
			{
                wd->sta.rxBeaconCount++;
			}
        }

        zm_msg1_rx(ZM_LV_2, "Rx VAP=", vap);

        /* handle IV, EXT-IV, ICV, and EXT-ICV */
        zfGetRxIvIcvLength(dev, buf, vap, &offset, &tailLen, addInfo);

        zfStaIbssPSCheckState(dev, buf);
        //QoS data frame
        if ((frameSubtype & 0x80) == 0x80)
        {
            offset += 2;
        }

        len = zfwBufGetSize(dev, buf);
        /* remove ICV */
        if (tailLen > 0)
        {
            if (len > tailLen)
            {
                len -= tailLen;
                zfwBufSetSize(dev, buf, len);
            }
        }

        /* Filter NULL data */
        if (((frameSubtype&0x40) != 0) || ((len = zfwBufGetSize(dev, buf))<=24))
        {
            zm_msg1_rx(ZM_LV_1, "Free Rx NULL data, len=", len);
            zfwBufFree(dev, buf, 0);
            return;
        }

        /* check and handle defragmentation */
        if ( wd->sta.bSafeMode && (wd->sta.wepStatus == ZM_ENCRYPTION_AES) && wd->sta.SWEncryptEnable )
        {
            zm_msg0_rx(ZM_LV_1, "Bypass defragmentation packets in safe mode");
        }
        else
        {
            if ( (buf = zfDefragment(dev, buf, &bIsDefrag, addInfo)) == NULL )
            {
                /* In this case, the buffer has been freed in zfDefragment */
                return;
            }
        }

        ret = ZM_MIC_SUCCESS;

        /* If SW WEP/TKIP are not turned on */
        if ((wd->sta.SWEncryptEnable & ZM_SW_TKIP_DECRY_EN) == 0 &&
            (wd->sta.SWEncryptEnable & ZM_SW_WEP_DECRY_EN) == 0)
        {
            encryMode = zfGetEncryModeFromRxStatus(addInfo);

            /* check if TKIP */
            if ( encryMode == ZM_TKIP )
            {
                if ( bIsDefrag )
                {
                    ret = zfMicRxVerify(dev, buf);
                }
                else
                {
                    /* check MIC failure bit */
                    if ( ZM_RX_STATUS_IS_MIC_FAIL(addInfo) )
                    {
                        ret = ZM_MIC_FAILURE;
                    }
                }

                if ( ret == ZM_MIC_FAILURE )
                {
                    u8_t Unicast_Pkt = 0x0;

                    if ((zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET) & 0x1) == 0)
                    {
                        wd->commTally.swRxUnicastMicFailCount++;
                        Unicast_Pkt = 0x1;
                    }/*
                    else if (zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET) == 0xffff)
                    {
                        wd->commTally.swRxMulticastMicFailCount++;
                    }*/
                    else
                    {
                        wd->commTally.swRxMulticastMicFailCount++;
                    }
                    if ( wd->wlanMode == ZM_MODE_AP )
                    {
                        u16_t idx;
                        u8_t addr[6];

                        for (idx=0; idx<6; idx++)
                        {
                            addr[idx] = zmw_rx_buf_readb(dev, buf, ZM_WLAN_HEADER_A2_OFFSET+idx);
                        }

                        if (wd->zfcbApMicFailureNotify != NULL)
                        {
                            wd->zfcbApMicFailureNotify(dev, addr, buf);
                        }
                    }
                    else
                    {
                        if(Unicast_Pkt)
                        {
                            zm_debug_msg0("Countermeasure : Unicast_Pkt ");
                        }
                        else
                        {
                            zm_debug_msg0("Countermeasure : Non-Unicast_Pkt ");
                        }

                        if((wd->TKIP_Group_KeyChanging == 0x0) || (Unicast_Pkt == 0x1))
                        {
                            zm_debug_msg0("Countermeasure : Do MIC Check ");
                            zfStaMicFailureHandling(dev, buf);
                        }
                        else
                        {
                            zm_debug_msg0("Countermeasure : SKIP MIC Check due to Group Keychanging ");
                        }
                    }
                    /* Discard MIC failed frame */
                    zfwBufFree(dev, buf, 0);
                    return;
                }
            }
        }
        else
        {
            u8_t IsEncryFrame;

            /* TODO: Check whether WEP bit is turned on in MAC header */
            encryMode = ZM_NO_WEP;

            IsEncryFrame = (zmw_rx_buf_readb(dev, buf, 1) & 0x40);

            if (IsEncryFrame)
            {
                /* Software decryption for TKIP */
                if (wd->sta.SWEncryptEnable & ZM_SW_TKIP_DECRY_EN)
                {
                    u16_t iv16;
                    u16_t iv32;
                    u8_t RC4Key[16];
                    u16_t IvOffset;
                    struct zsTkipSeed *rxSeed;

                    IvOffset = offset + ZM_SIZE_OF_WLAN_DATA_HEADER;

                    rxSeed = zfStaGetRxSeed(dev, buf);

                    if (rxSeed == NULL)
                    {
                        zm_debug_msg0("rxSeed is NULL");

                        /* Discard this frame */
                        zfwBufFree(dev, buf, 0);
                        return;
                    }

                    iv16 = (zmw_rx_buf_readb(dev, buf, IvOffset) << 8) + zmw_rx_buf_readb(dev, buf, IvOffset+2);
                    iv32 = zmw_rx_buf_readb(dev, buf, IvOffset+4) +
                           (zmw_rx_buf_readb(dev, buf, IvOffset+5) << 8) +
                           (zmw_rx_buf_readb(dev, buf, IvOffset+6) << 16) +
                           (zmw_rx_buf_readb(dev, buf, IvOffset+7) << 24);

                    /* TKIP Key Mixing */
                    zfTkipPhase1KeyMix(iv32, rxSeed);
                    zfTkipPhase2KeyMix(iv16, rxSeed);
                    zfTkipGetseeds(iv16, RC4Key, rxSeed);

                    /* Decrypt Data */
                    ret = zfTKIPDecrypt(dev, buf, IvOffset+ZM_SIZE_OF_IV+ZM_SIZE_OF_EXT_IV, 16, RC4Key);

                    if (ret == ZM_ICV_FAILURE)
                    {
                        zm_debug_msg0("TKIP ICV fail");

                        /* Discard ICV failed frame */
                        zfwBufFree(dev, buf, 0);
                        return;
                    }

                    /* Remove ICV from buffer */
                    zfwBufSetSize(dev, buf, len-4);

                    /* Check MIC */
                    ret = zfMicRxVerify(dev, buf);

                    if (ret == ZM_MIC_FAILURE)
                    {
                        if ((zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET) & 0x1) == 0)
                        {
                            wd->commTally.swRxUnicastMicFailCount++;
                        }
                        else if (zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET) == 0xffff)
                        {
                            wd->commTally.swRxMulticastMicFailCount++;
                        }
                        else
                        {
                            wd->commTally.swRxMulticastMicFailCount++;
                        }
                        if ( wd->wlanMode == ZM_MODE_AP )
                        {
                            u16_t idx;
                            u8_t addr[6];

                            for (idx=0; idx<6; idx++)
                            {
                                addr[idx] = zmw_rx_buf_readb(dev, buf, ZM_WLAN_HEADER_A2_OFFSET+idx);
                            }

                            if (wd->zfcbApMicFailureNotify != NULL)
                            {
                                wd->zfcbApMicFailureNotify(dev, addr, buf);
                            }
                        }
                        else
                        {
                            zfStaMicFailureHandling(dev, buf);
                        }

                        zm_debug_msg0("MIC fail");
                        /* Discard MIC failed frame */
                        zfwBufFree(dev, buf, 0);
                        return;
                    }

                    encryMode = ZM_TKIP;
                    offset += ZM_SIZE_OF_IV + ZM_SIZE_OF_EXT_IV;
                }
                else if(wd->sta.SWEncryptEnable & ZM_SW_WEP_DECRY_EN)
                {
                    u16_t IvOffset;
                    u8_t keyLen = 5;
                    u8_t iv[3];
                    u8_t *wepKey;
                    u8_t keyIdx;

                    IvOffset = offset + ZM_SIZE_OF_WLAN_DATA_HEADER;

                    /* Retrieve IV */
                    iv[0] = zmw_rx_buf_readb(dev, buf, IvOffset);
                    iv[1] = zmw_rx_buf_readb(dev, buf, IvOffset+1);
                    iv[2] = zmw_rx_buf_readb(dev, buf, IvOffset+2);

                    keyIdx = ((zmw_rx_buf_readb(dev, buf, IvOffset+3) >> 6) & 0x03);

                    IvOffset += ZM_SIZE_OF_IV;

                    if (wd->sta.SWEncryMode[keyIdx] == ZM_WEP64)
                    {
                        keyLen = 5;
                    }
                    else if (wd->sta.SWEncryMode[keyIdx] == ZM_WEP128)
                    {
                        keyLen = 13;
                    }
                    else if (wd->sta.SWEncryMode[keyIdx] == ZM_WEP256)
                    {
                        keyLen = 29;
                    }

                    zfWEPDecrypt(dev, buf, IvOffset, keyLen, wd->sta.wepKey[keyIdx], iv);

                    if (ret == ZM_ICV_FAILURE)
                    {
                        zm_debug_msg0("WEP ICV fail");

                        /* Discard ICV failed frame */
                        zfwBufFree(dev, buf, 0);
                        return;
                    }

                    encryMode = wd->sta.SWEncryMode[keyIdx];

                    /* Remove ICV from buffer */
                    zfwBufSetSize(dev, buf, len-4);

                    offset += ZM_SIZE_OF_IV;
                }
            }
        }

#ifdef ZM_ENABLE_CENC
        //else if ( encryMode == ZM_CENC ) /* check if CENC */
        if ( encryMode == ZM_CENC )
        {
            u32_t rxIV[4];

            rxIV[0] = (zmw_rx_buf_readh(dev, buf, 28) << 16)
                     + zmw_rx_buf_readh(dev, buf, 26);
            rxIV[1] = (zmw_rx_buf_readh(dev, buf, 32) << 16)
                     + zmw_rx_buf_readh(dev, buf, 30);
            rxIV[2] = (zmw_rx_buf_readh(dev, buf, 36) << 16)
                     + zmw_rx_buf_readh(dev, buf, 34);
            rxIV[3] = (zmw_rx_buf_readh(dev, buf, 40) << 16)
                     + zmw_rx_buf_readh(dev, buf, 38);

            //zm_debug_msg2("rxIV[0] = 0x", rxIV[0]);
            //zm_debug_msg2("rxIV[1] = 0x", rxIV[1]);
            //zm_debug_msg2("rxIV[2] = 0x", rxIV[2]);
            //zm_debug_msg2("rxIV[3] = 0x", rxIV[3]);

            /* destination address*/
            da[0] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET);
            da[1] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET+2);
            da[2] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET+4);

            if ( wd->wlanMode == ZM_MODE_AP )
            {
            }
            else
            {
                if ((da[0] & 0x1))
                { //multicast frame
                    /* Accumlate the PN sequence */
                    wd->sta.rxivGK[0] ++;

                    if (wd->sta.rxivGK[0] == 0)
                    {
                        wd->sta.rxivGK[1]++;
                    }

                    if (wd->sta.rxivGK[1] == 0)
                    {
                        wd->sta.rxivGK[2]++;
                    }

                    if (wd->sta.rxivGK[2] == 0)
                    {
                        wd->sta.rxivGK[3]++;
                    }

                    if (wd->sta.rxivGK[3] == 0)
                    {
                        wd->sta.rxivGK[0] = 0;
                        wd->sta.rxivGK[1] = 0;
                        wd->sta.rxivGK[2] = 0;
                    }

                    //zm_debug_msg2("wd->sta.rxivGK[0] = 0x", wd->sta.rxivGK[0]);
                    //zm_debug_msg2("wd->sta.rxivGK[1] = 0x", wd->sta.rxivGK[1]);
                    //zm_debug_msg2("wd->sta.rxivGK[2] = 0x", wd->sta.rxivGK[2]);
                    //zm_debug_msg2("wd->sta.rxivGK[3] = 0x", wd->sta.rxivGK[3]);

                    if ( !((wd->sta.rxivGK[0] == rxIV[0])
                        && (wd->sta.rxivGK[1] == rxIV[1])
                        && (wd->sta.rxivGK[2] == rxIV[2])
                        && (wd->sta.rxivGK[3] == rxIV[3])))
                    {
                        u8_t PacketDiscard = 0;
                        /* Discard PN Code Error frame */
                        if (rxIV[0] < wd->sta.rxivGK[0])
                        {
                            PacketDiscard = 1;
                        }
                        if (wd->sta.rxivGK[0] > 0xfffffff0)
                        { //boundary case
                            if ((rxIV[0] < 0xfffffff0)
                                && (((0xffffffff - wd->sta.rxivGK[0]) + rxIV[0]) > 16))
                            {
                                PacketDiscard = 1;
                            }
                        }
                        else
                        { //normal case
                            if ((rxIV[0] - wd->sta.rxivGK[0]) > 16)
                            {
                                PacketDiscard = 1;
                            }
                        }
                        // sync sta pn code with ap because of losting some packets
                        wd->sta.rxivGK[0] = rxIV[0];
                        wd->sta.rxivGK[1] = rxIV[1];
                        wd->sta.rxivGK[2] = rxIV[2];
                        wd->sta.rxivGK[3] = rxIV[3];
                        if (PacketDiscard)
                        {
                            zm_debug_msg0("Discard PN Code lost too much multicast frame");
                            zfwBufFree(dev, buf, 0);
                            return;
                        }
                    }
                }
                else
                { //unicast frame
                    /* Accumlate the PN sequence */
                    wd->sta.rxiv[0] += 2;

                    if (wd->sta.rxiv[0] == 0 || wd->sta.rxiv[0] == 1)
                    {
                        wd->sta.rxiv[1]++;
                    }

                    if (wd->sta.rxiv[1] == 0)
                    {
                        wd->sta.rxiv[2]++;
                    }

                    if (wd->sta.rxiv[2] == 0)
                    {
                        wd->sta.rxiv[3]++;
                    }

                    if (wd->sta.rxiv[3] == 0)
                    {
                        wd->sta.rxiv[0] = 0;
                        wd->sta.rxiv[1] = 0;
                        wd->sta.rxiv[2] = 0;
                    }

                    //zm_debug_msg2("wd->sta.rxiv[0] = 0x", wd->sta.rxiv[0]);
                    //zm_debug_msg2("wd->sta.rxiv[1] = 0x", wd->sta.rxiv[1]);
                    //zm_debug_msg2("wd->sta.rxiv[2] = 0x", wd->sta.rxiv[2]);
                    //zm_debug_msg2("wd->sta.rxiv[3] = 0x", wd->sta.rxiv[3]);

                    if ( !((wd->sta.rxiv[0] == rxIV[0])
                        && (wd->sta.rxiv[1] == rxIV[1])
                        && (wd->sta.rxiv[2] == rxIV[2])
                        && (wd->sta.rxiv[3] == rxIV[3])))
                    {
                        zm_debug_msg0("PN Code mismatch, lost unicast frame, sync pn code to recv packet");
                        // sync sta pn code with ap because of losting some packets
                        wd->sta.rxiv[0] = rxIV[0];
                        wd->sta.rxiv[1] = rxIV[1];
                        wd->sta.rxiv[2] = rxIV[2];
                        wd->sta.rxiv[3] = rxIV[3];
                        /* Discard PN Code Error frame */
                        //zm_debug_msg0("Discard PN Code mismatch unicast frame");
                        //zfwBufFree(dev, buf, 0);
                        //return;
                    }
                }
            }
        }
#endif //ZM_ENABLE_CENC

        /* for tally */
        if ((zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET) & 0x1) == 0)
        {
            /* for ACU to display RxRate */
            zfWlanUpdateRxRate(dev, addInfo);

            wd->commTally.rxUnicastFrm++;
            wd->commTally.rxUnicastOctets += (len-24);
        }
        else if (zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET) == 0xffff)
        {
            wd->commTally.rxBroadcastFrm++;
            wd->commTally.rxBroadcastOctets += (len-24);
        }
        else
        {
            wd->commTally.rxMulticastFrm++;
            wd->commTally.rxMulticastOctets += (len-24);
        }
        wd->ledStruct.rxTraffic++;

        if ((frameSubtype & 0x80) == 0x80)
        {
            /* if QoS control bit-7 is 1 => A-MSDU frame */
            if ((zmw_rx_buf_readh(dev, buf, 24) & 0x80) != 0)
            {
                zfDeAmsdu(dev, buf, vap, encryMode);
                return;
            }
        }

        // Remove MIC of TKIP
        if ( encryMode == ZM_TKIP )
        {
            zfwBufSetSize(dev, buf, zfwBufGetSize(dev, buf) - 8);
        }

        /* Convert 802.11 and SNAP header to ethernet header */
        if ( (wd->wlanMode == ZM_MODE_INFRASTRUCTURE)||
             (wd->wlanMode == ZM_MODE_IBSS) )
        {
            /* destination address*/
            da[0] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET);
            da[1] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET+2);
            da[2] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A1_OFFSET+4);

            /* check broadcast frame */
            if ( (da[0] == 0xffff) && (da[1] == 0xffff) && (da[2] == 0xffff) )
            {
                // Ap send broadcast frame to the DUT !
            }
            /* check multicast frame */
            /* TODO : Remove these code, hardware should be able to block */
            /*        multicast frame on the multicast address list       */
            /*        or bypass all multicast packet by flag bAllMulticast */
            else if ((da[0] & 0x01) && (wd->sta.bAllMulticast == 0))
            {
                for(ii=0; ii<wd->sta.multicastList.size; ii++)
                {
                    if ( zfMemoryIsEqual(wd->sta.multicastList.macAddr[ii].addr,
                                         (u8_t*) da, 6))
                    {
                        break;
                    }
                }

                if ( ii == wd->sta.multicastList.size )
                {   /* not found */
                    zm_debug_msg0("discard unknown multicast frame");

                    zfwBufFree(dev, buf, 0);
                    return;
                }
            }

#ifdef ZM_ENABLE_NATIVE_WIFI //Native Wifi : 1, Ethernet format : 0
            //To remove IV
            if (offset > 0)
            {
                for (i=12; i>0; i--)
                {
                    zmw_rx_buf_writeh(dev, buf, ((i-1)*2)+offset,
                            zmw_rx_buf_readh(dev, buf, (i-1)*2));
                }
                zfwBufRemoveHead(dev, buf, offset);
            }
#else

            if (zfRxBufferEqualToStr(dev, buf, zgSnapBridgeTunnel,
                                     24+offset, 6))
            {
                snapCase = 1;
            }
            else if ( zfRxBufferEqualToStr(dev, buf, zgSnap8021h,
                                           24+offset, 6) )
            {
                typeLengthField =
                    (((u16_t) zmw_rx_buf_readb(dev, buf, 30+offset)) << 8) +
                    zmw_rx_buf_readb(dev, buf, 31+offset);

                //zm_debug_msg2("tpyeLengthField = ", typeLengthField);

                //8137 : IPX, 80F3 : Appletalk
                if ( (typeLengthField != 0x8137)&&
                     (typeLengthField != 0x80F3) )
                {
                    snapCase = 2;
                }

                if ( typeLengthField == 0x888E )
                {
          /*
 * CopyzfShowRxEAPOL(dev, buf, 32);
/*
 * Copyrns In}ommunications.
 *
 * Permiselseommunications{ommunications Inc//zfwDumpBuf-2008 Ath Communications}
ommunications/* source address */ommunicationsif ( wd->wlanMode == ZM_MODE_INFRASTRUCTURE ) modify, and/or distribute this * SA = Anted, p3provided that th"AS sa[0] = zmw_rx_buf_readh-2008 AtherZM_WLAN_HEADER_A3_OFFSET Communications Incsa[1ND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD T+s Communications Incsa[2NG ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITN4purpose with or won to use, copy, modify, and/or distribute this  *
 * THE SOFTW2RE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH 2EGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITYTHER TORNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT,RMANCE OF, OR CONSEQUENTIALvided that the abosnapCasesion notice appear in all copies.
 *
 *E IS PROVIDED "AS IHE AUTHOR DwriteAIMS ALL WAR24+offset,IS" AN Communications Inc                             6            1                 */
/*  Abstract                 8            2    TSOEVER RESULTING FRD * THE SOFTW1     */
/*                                      1le containd                    */
/*  Abstract                 0                                          */
/*      This modu2            x and             */
/fwBufRemoveHead                     OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM   */
/*                                                                        */
/*  Abstract                                                            */
/*      This modu          s Tx and Rx functions.                       */
/*                                                      ***/
#include "cprecomp.h"

u16_t zfWlanRxValidate                                           */
/*      This mod1                                                     */
/*                               /* Ethernet payload length********************typeL0, 0,FieldD TH     GetSizer any
 * p - 14= { 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00 };
/* Tab, (//const u8_t zc<<8)+[4] = {8, 2, 4, >>8)purpose with or w#endifis sRRANENABLE_NATIVE_WIFI         AL DAMAGESopy,he abve
 * copyright notice aAPion noticeor distribute //e ab(HE AUTHOR DISCLbcess Categor) & 0x3) != 3ion notice appe abvap < notiAX_AP_SUPPORTion notice app/* AP myrigrovided that th{
#ifdefonInfo* addInfo)
{
    //Native Wifi : 1,[8] = {0, format : 0r distribute this sTo r      IV                 e ab       > 0ion notice appand/or distribute this_ENAfor (i=12; i>0; i--C
        case ZM_ ZM_CENC:
#endif //ZM_ENAB */
/*  Abstract                ((i-1)*2)         ZM_AES:

            enc     */
/*  AbstracISCLAIMS ALL WAR      br                   rmission to use, c                           */
/*   fault
//const u8_t zcUpTouct py, modify, and/TING FROM LOSS OF USE, DATA OR PROFITS, W                                       HE AUTHOR DISCLAIMS ALL WA ZM_AES:

            enc NEGLIGENCE OR OTHER TORT                */
/*  Abstract                            buf, u8_t vap, u16_t* pIvLen,
                        u16_t* pIcvLen, stru+           {
                 */
/*      This module containencryMode;

    zmw_get_wlan_dev(dev);

    *pIvLen = 0;
    *pIcvLen = 0;

4ct zsAdditionInfo* a                ARE IS PROVIDED "AS I FROeq : Read 20       22, ISCL 18        0   ( wd-6       18yMode[vap] == ZM_WEP64 sequence must not be inverted     */
/*                                                  buf, u8_t vap, u16_t* pIvLen,
                        u16_t* pIcvL INDIRECT, buf_t* buf);
u16_t zfWlanRxFilter(zdev_t* dev, zbuf_t* buf
                *pIcvLen = 4;
            }
            else
            {
    encryMode = zfGetEncryModeFromRxStatus(addInf           
                *pIcvLen = 4;
            }
            else
            {               u16_t                   */
/*               t zsAdditionInfo* addInfo)
{
    u8_t secd = zfAp#if 1ZM_WEP256:
#ifdef ZM_(retUpToAIntrabssForwar    */
/*   vap))ght 1C
        case ZM_CENC:
#endif //ZM_ENAB/* Free Rx Athferf ZMi    -BSS unicast fram    switch( secur = zfGetEn_msg0_rx(ZM_LV_2, "       {
                   "         {
            OFFSET    -2008 Ather0         {
            returnCommunications Inc.
 *
 * PermisfApFi zsAd
 *
 * Permission to use, copy, modify, and//* WDS*/

    switch( securitvLen = 8;
                        *pIRxLen =data   }ar in all copies.
 *
 * THE SOFTW4************************************************3dr[3];

                addr[0] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A4n, struct zsAdditionInfo* addInfo)
{
    u16_t wdsPort;3IvLen = 4;
                *pIcvLen = 4;
            }
            else
             );
                addr[1] = zmw_rx_buf_readh(dev,3v, zbuf_t* buf, u8_t vap, u16_t* pIvLen,
                        u16_t* pIcvLt checkin       {
            if (( wd->ap.encryMode[vap] == ZM_WEP64 ) ||
                    ( wd->ap.encryMode[vap] == ZM_WEP128 ) ||
                    ( wd->ap.encryMode[vap] == ZM_WEP256 ))
            {
                *pbuf, ZM_WLAN_HEADER_A2_OFFSET+2);
                addr[2] = zmw_rx_buf_readh(dev, buf
                u16_t id;
                u16_t ad    u8_t  encryMode;

    zmw_get_wlan_dev(dev);

    *pIvLen = 0;
    *pIcvL2_OFFSET);
                addr[1] = zmw_rx_buf_readh(dev,ev, zbuf_t* buf, u8_t vap, u16_t* pIvLen,
                        u16_t* pIcvLdev, buf, ZM_WLAN_HEADER_A2_OFFSET+4);

                         OR CONSEQUENTIAL DAMAGESurityByte;
    u8_t encryMode;

    securitPSEUDOyte = (addIn			cvLen =test:     casadd            else u8_t encenableWDSion notice appear in all copies.ENABLE_+= 6urpose with or without fee is heOM LOSS OF USE, DATA OR PROFITzfGetRxIvIcvLength(zdev_t* dev, zbuf_t* buf, u8_t vap, u16_t* pIvLen,
                              u16_t* pIcvLen, struct zsAdditionInf = 0;
				break;
#ifdef ZM_ENABLE_CENC
            case ZM_CENC:
                *pIvLen =pIvLen = 0;
    *pIcvLen = 0;

    encryMode = z             *pIvLen = 8;
                *pIcvLen = 4;
				break;
			case ZM_AES:
        if (vap < ZM_MAX_AP_SUPPORT)
        {
                            */
/*        r[1] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A2_OFFSET+2);
                addr[2] = z          u16_t* pIcvL1n = 18;
            *pIcvLen = 16;
#endif //ZM_ENABLdr[3];

                addr[0] = zmw_rx_buf_readh(dev, buf,  encryMode == ZM_AES )
                   (encryMode == ZM_WEP128)||
        IvLen = 4;
                *pIcvLen = 4;
            }
      lse if ( encryMode == ZM_CENC)
                u                   */
/*                       urityByte;
   te = (addInfo->Tail.Datzm_assert(able[id].enc*  Module N/* Call****RecvEth() top.enify upper layer   switch( s//      2           *pIRIPTION          8 Ath="y
 * purpose wit software for any
 * pur         indSZM_PROTOCOL_RESPONSE_SIMULATION  if          zfProtRspSimr any
 * purpose wit          *pIvLer            */
/*      in the d/* tally   switc	ve
 commT
/* .N   zfNDISRxFrmCnt++   in t	u8_t enczfcb       curiNULLion no	                            *                             g liERFORMANCE_RX_MSDU-2008 ve
 tickion noticeAL DAMAL DAM/*cvLemanagemen            *pI
    u8_t     Typight notTIES
MANAGEMENT_FRAMEion no          */
/*      Force flusRx           ,FC or      Ctrlng the buffeSCRIPTIONProcessM               handle             */
/*                  */
/*      No2008 AtheraddInfo); //CWYang(mion notice else if (wd->ap.staTable[id]         PsPoll*                t encryMode;

    securityBy &&           ght 0xa4)                 */
/*           0      /
/*     }
       zfAp       /
/*  r any
 * purpose wit                                   ;
            Stephen Chen        At1      discard!!   }
                     DriverD******edFrm                                                _AES)
  }


/*u16_t i, j;
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_sec/
/*default:

            if ( _section(dev);

    for(i=0; i<ZM_MAX_DEFRAtion();

FUNC     DESCRIP                       eWlanRxValidateM_MAX_DEFRAG_ENTRIES;  Entry[i].Rx      .al_section(dev);

    for(i=0; i<ZM_MAX_DEFRAG_ENTRIES;    zmw_enter_critical_section(dev);

    for(i=0; i<ZM_MAX_DEFRAG_ENTRIES;INPUTSw_enter_critical_section(dev);

    for(i=0; i<ZM_MAX_DEFRAG_ENTRIES;  dev :M_LVice pointerFlag != 0))
            {
                zm_msg1_rx(ZMbuf *pIvceived 802.11             if (((wd->tick - wd->defragTabtion();

    zmw_enter_critical_section(dev);

    for(i=0; i<ZM_MAX_DEFRAG_ENTRIES;OUT           || (flushFlag != 0))
            {
                ztion();

  Error cyrig.defragEntry[i].fragment[j], 0);
                }
            }  zmw_enter_critical_section(dev);

    for(i=0; i<ZM_MAX_DEFRAG_ENTRIES;AUTHOR       || (flushFlag != 0))
            {
                zm_msg1_rx(ZMStephen              yDAS Technology Corporatio     2005.10e.defragEntry[i].fragCount; j++)
                {
                    zfwBufFree(dev16_t i, j;
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_sectiou16_tle.defragEntry[i](zdev_t*M_LV, zOR Dt*
 * p
      select                */
/*          entry for replacLe
      selectret       8_t *     Sub one e     HE Aget_* co_dev   *    in t           THE AUTHOR DISCLAIMS ALL WARable[id]                          C                   =                F0) >> 4            LenUpToAc[8] = {0, 0, 0, 0, 0       yte5ccept Data/
/*      N       with protocol vers   *= 0   switc                  0x8) ||               0x0 */
/*              *TODO : check rx status => erro bit    UNCTION DESCR     Minimum nst u8gment Wep   switch( s                dev4000ecuriNC
        Info->Tail.Data*of first fragmen=].fragment[j], 0);
                }
         }
        e    PLCP(5)+    er(24)+IV(    C     CRC    RxS     (8)provided that the aber       <ros  modify, and/or distribute this_AES)
onInfRR_MINushFENCRYP       _LENGTH(encryMode)
		{
        case ZM_WEP64:
                   ss o5                      8sion notice                                          MAC           Timestamp(8)+BeaconIfragval(2)+Cap          None                                        6                   */
/*                                                              */
/*    AUTHOR          */
/*      Stephen Chen        Atheros Communications, IN                                                 24    */
/************************************************************************/
void zfAdeqNum : sequence oif       fragmen>           XushFSIZE.                er       ; i<ZM_MAX_DEFRAG_ENT         */
/*      Step               DEFRAG                                                 */
/&0xff   if                      */
/*    AUTHOR      */
/* n        Atherorx pspunications, }

    /* If full, sequentially re              TYPE_BARe existing one *u8_t encsta.;
			bDrvBA  if (wd->ap.st               fAgg    BARr any
 * purpose witty one in d              RX    NTRIES wd->defragTable               .fragCount; j++)
TRIES-1);
 wd->def         ove
 * copyright notice aAPsion no       }

    /* If fove
 * copyrig!P128:
        cadefragTable.dt the aboff)
=zfStaagEntry[i]F    r any
 * p)!=ZM_SUCCESS          */
/*      Steph */
debug    1("*******      ,    }
= "   (h (encryMode)
		               *     }
    } flushFlag)DEFREntry[i
{
    u16_t i, j;
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    zmw_enter_critical_section(dev);

    for(i=0; i<ZM_MAX_DEFRAG_ENTRIES; i++)
    {
        if (wd->defragTable.defragFilrag list :", i);
tion();

         duplicaap]      if (((wd->tick - wd->defragTable.defragEn.tick) >
                        (ZM_DEFRAG_AGING_TIME_SEC * ZM_TICK_PER_SECOND))
               || (flushFlag != 0))
            {
                zm_msg1_rx(ZM_LV_2, "Aging defrag list :", i);
                /* Free the buffers in the defrag list */
                for (j=0; j<wd->defragTable.defragEntry[i].fragCount; j++)
                {
                    zfwBufFree(dev, wd->defragTable.defragEntry[i].fragment[j], 0);
                }
            }
        }
        wd->defragTable.defragEntry[i].fragCount = 0;
    }

    zmw_leave_critical_section(dev);

    return;
}


/************************************************************************/
/*                                                                      */
/*    FUNCTION DESCRIPTION                  zfAddFirstFragToDefragList  */
/*      Add first fragment to defragment list, the first empty entry    */
/*      will be selected. If t      is full, sequentially select      */
/*src[3]            dst0 entry for replac one entry for rseq                              index            cont.          i     */
/* up    ;*/
 User priorit    I                                 HE Adeclare_for_critical_sec    (        ZM_BUFFER_TRACEr any
 * p         RX PREFIX   switcENABLE_                      HE AUTHOR DISCLAIMS ALL WARswitch (e      / Don't divide 2^4 because we d*****want theIPTI           *pkt    be treESCRIas********TION DESCRIPTIONev, zbseq       */
/*************************+2s Commun*/
/   u16_t seqNum, u8_t fragNum, u8_t m, OR CON    AND THE AUTHOR DISCLAIMS ALL WAR u8_t m1able[id]    ING ALL IMPLIED WARRANTIES OF
 *NULL;
  s Commun    THE AUTHOR BE LIABLE FOR
 * ANY NULL;
  , OR        Qo                 *pI                   88ly repl88e existing one *     ex & 0xc0) >> 4;  /* byte4  u8_t mo, OR CONSEQUup &repl7].fragment[j],        tAddr,+up/
      FILT  */* addROW-1 zmw_get_wlTBD : f      t fragment reby granted, p== own MAC ated, provided         macHE Si, j,=6_t i,       Compare addrING */
   1]                   for (j=0; jTHE */
   2] */
/*      Steph_DEFRAG_ENTRIES)
    {
         =>src is )
     ****************traf      rxSrcIsOwnMac    indS   case ZM.fragCount; j++)
SRC_ADDR_IS_OW    CTA's inf    wd->defr */
/*    OUTPUTS       seq or seq zmw_get_wl                     on*    INPUT              1ly rep                 *w_efragommunications, INC           ENABLE_(i=   c<ES; i++)
    {
 COL; i++o be replaced */
                rNone   Tbl[i][; i<Z].anteress */
             {
                 for        && (fragNum < 8))
 <6; j++)
            {
                      /* Add frag frame to def(addr[j] != w      {
                        /* Add frag frame t    = up */
/* fy, and/or distribute this               &0x800)==Done = ZM_AES:

            enc&&        /* Add frag frame tt nu=r */                  CENC:
#endif //ZM_ENAB    leave                   if ((.encryMode == ZM_TKIP)
hi    TION DESCRIPTION********************     */
/*******************Table.dhit=>TION DESCR   }
                    break;
        DION DESC    [id].encryMode == ZM_AES)
ount; j++)
DUPLICATi].frag              {
              ;
            break;
d].encryMode == ZM_TKIP)
      .encTION DESCRIPTION, upy[i].      ( wnumbgList         tAddr = zfwBufGeg == 0)
                  =                 /* merge all fragment if more data bit is cleared */
          //*pIcvLen = 0;
       ;
           }
                   agTable.defragEntrymmunications Inc.
 *
 * Permission to us}*/
 = wd->defragTable.defragEntry[i].fra   seqNum : sequmiss :        t			b   switch( sfragEntry[i].fragment[0];
     wBuf   }
       ffer      Random select aTHORumn   switch( sr   = (selec)  foring,NTRIES; i++)
    {
 COL-1            etRxIvIcvLength(colragNum < 8))
     6_t i, Len-fragHead) < 1560)
                     ING A+)
   Len-fragHead) < 1560)
                     THE Aj] != Len-fragHead) < 1560)
                nBuf, 0, &ivLen, &iad) < 1560)
                     up   in the dall fragment if more data bit is clea     ble.defragEntry[i].seq         agTable.defragEntry[i]. selectedTxGendefrTailis full, sequentially sel,      *r.c         r.c  lenvLen,
                         micect     struct zsMicVar*  pMicKey             i,, 0, 0,, 1, 1, 0O            
/*   bValue, qos       /*      BufFre.c  Byte[1nt[k                                 , 0);
        }
    }

    wd->defragTable.dagTa       UpToAApGetTx         */
/*   &d->defr    in the de abole.defrag/*              */
/*      Step      //*          }
    }

    /* If fove
 * copyright notice and this permission no>defragTable.defragEntSta[i].fragCount = 0;
                  zfwBufSetSize(dev, returnBuf, startAddr);
                        }
      BufFree(dev, wd->defra/*     d->defr 0, 0,                                 zfMicClear(        zmw_get_wlappend    and*******yByte )
    {
        case Z       wd->16efra2    fragCoun{****VISTA DAtry[i].fre(dev,itical_t0xc0) >> 4;  /* byte4 i             MicA     ].fr(e(dev, wree(dev, ble.defragE
/* retdefraturn = NULL => save or fSee this frame         */
zbuf_t* zfDefragment(zdev_t* dev, zbuf_t* buf, u8_t* pbIsDefrag,
        P;
       = wd->defra
    = NULL =>o)
{
    u8_t fragNum;
    u16_t seqNum;
    u8_t moreFragBit;
    u8_t addr[6];
    u16_t i;
    z     uf, 0);
       LE_Calign     efragTable.             break;
                    }
              buffers in twmeConnecap]               dev, zbuf_t* buf, u8   */
zbuf_t* zfDefragment(ZM_80211fragEntIPEGARD T + */
>> 5IsDefrag,
                            zbuf_t* buf, u80IsDefrag,
                                  break;
           wd->defragTabe abd->defragif (wd->ap.staTab      /* Not part of a fragmentation */

        return buf;
    }
    else
    {
        wd->commTally.swRxFragmentCount++;
        seqNum = seqNum >> 4;
   existing one */
 r      Qos Software MIC in I    pyrig            tCount++;
        seqNum = seqNum >> 4            zfAddFirstFragToDefradev, buf, addr, seqNum);
            buf = NULL;
        }
            }
            }
                            _DEFR   return buf;
    }
                               n */
            zm_msg1_rx(ZM_1);
 2, "Fragtry[i].fr= wd->defra(
      >>1)i].fragCount)
               ry[i].frai*THE A(frag)    bu[i]gEntrf purpose with oraddr, seqNum+ING AagNum, mmoreFragB>> 8/
    addInfo);
   ty one in d= wd->defra
      i].fragCount)
                           zfAdaddr, seqN]   {
        wd->com     wd->defr
/* ren */
        efra 0, 0,

    ZM_BUFFER_TRACE(dev, buf)

    *pbIsDefrag = FALSE;
    seqNum = zmw_buf_readh(dev, buf, 22);
    fragN         bGetMic(ULL)
 *)lenE           /* MidagTable.defIZBit=_MIC
{
    u16_t i, j;
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    zmw_enter_critical_section(dev);

    for(i=0; i<ZM_MAX_DEFRAG_ENTRIES; i++)
    {
        if (wd->defragTable.    tIpTosAndFragragList       */
/* Get IP TOSeturnt* z        from T         fAddFragToDefragList       */
/*      Add middle or last fragment to defragment list.                 */
/*                                                                      */
/*    INPUTS                                                            */
/*      d38);
      defrag list :", i);
                /* Free thetion();

  up : (dip[1] LE_C_AES)
ing u                 dip[1] = zmw_rx_buf_readh(dev,t* zOfdefrARP");
            /* ipf_readh(dev, ->defragTable.defragEntry[i].fragCount; j++)
                {
                    zfwBufFree(dev, wd->defragTable.defragEntry[i].fragment[j], 0);
                }
            }Non}
        wd->defragTable.defragEntry[i].fragCount = 0;t = 0;
    }

    zmw_leave_critical_section(dev);

    return;
}


/************************************************************************/
/*                              C                                       */
/*  6.6List       */
/*      Add middle or last fragment to defragment list.                 16_t i, j;
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_sectiovoidrx_buf_readh(dev, bu        else
                    u        *ev, buf,ect      
/* ipv            }
  
	selecte] =  one entry 
/* tosoreFrag*       0, 0x0v, buf,           st pa                               e abst p>= 34) //f first IPv4 packet sizegory(8] =  h     )+20(x(ZM_rot = z
              rx_buf_wr NUL(       t of a fragmentation */

        return buragBit=", m))6};
                     +    */
zbuf_t* zfDefragment(
        dip[1] = zmw_rx_b    e             *buffer   //co == zm                       dipss of
                           pv      */
zbuf_t* zfDefragment( zm_msg1_rx(ZM_LV_2, "Fr device                 uest repl    zx(ZM modify, and/or distribute thistost to 192.168.1.15 */
        if ((prot == 0x1) && (dip2_rx(ZM           zm_msg     (_LV_se
 (i=0; i<3; i++)
     }
    }
    */
zbuf_t* zAIMS ALL WARRAN   return buf;
    }
   6 OR CONSEQUENTIAL DAMAGES OR   zm_msg1_       VLAN tageturnIPv6_LV_2, "                       hFlag)
{
  
    }

    return returnBuf;                 Snap        else
                                      lenrr = 1;
 reFrAND THE Aev, buf, 6+(i*2));
                 * WITHLEN +          0x00ING ALL I         /* exchange src ip and dst ip */
      6_t startreFrTHE AUTHO         /* exchange src ip and dst ip */
      , OR CON*r last pa8;
 16_t i;
    u16c ip and dst ip */
      (i*2), t
{
 P;
   zmw_rx_buf_writeh(dev, buf, 8, 0x0000);
            zmw_rx_buf_writeh(dev, buf, 10,            d;
	ange icmprx_buf_writeh(   zmw_ev, b echoMP */
    else if (ethType == 00008)
    {
< 1    zm_msg0_r8] = {0, 1,_2, "IP");
        prot = zn */
            zm_msg1_A     ?            (i*2), temp                 return NULL;
  /* Gener     FC1042f_readb*               dip[0] = zmw_rx_buf_readh(dev, buf, 30);12uf_readh(dev, buf, 32);     zm_msg2_rx(ZM_LV_2, "pro13**********defragTable.2("t == {0, );
  or   }
    i",ot == 0x6)  /* Middle t == 0x6)
> 15        ting one */
 ETHERNETP64:
               type t = 12i=0; i<3;  0x0000);
0xaaaa, "Dst Port=", ING A0x0003i=0; i<3;      LV_2, "ip1="0x8137     aLV_2, "ip1=", d80f3  {
                        Bridge Tunne (i == ZM_MAXreadh(dev, buf0xF80                                                   RFC                     ge dst */
  00            for (i=0; (i*2), temp);
                    {
        88ssion notice
    return buf;ht (T) 2007-2008 Ather   zm      zfwBufFree(dev, returnBuf, 0);
  /**/
  3
            zm_msg2_rx(ZM_LV_= 0xa8c0) &        zmw_rx_bufd->defragTable type to uct zsAd

ev, bzfIsVtxqEmptyis full, seq(dev, buf, 28,sriteh = TRUi].fragf, 28,nt[k], 0);
                              Atheros Communications, INC.    200      {
                    if ((fragNu8_t encvmmq    Bit dh(dev,      {
            ad         FALS 24(Data) ogoto      _done entryap(u16_t num)   c < 4

    ZM_BUFFER_TRACE_readh(detx buf,ragB26+(i*2)tx
    [iNum] = buf;               mw_rx_buf_writeh(dev, bu, buf, 30+(i*2), temp);
 u16_t zfSwap(+(i*2), te:    {
   fragment if more data bit is clea      /teh(dev
{
   u16_t i, j;
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    zmw_enter_critical_section(dev);

    for(i=0; i<ZM_MAX_DEFRAG_ENTRIES; i++)
    {
        if (wd->defragTable.Putuf_w{
            zm_msg0_rx(ZM_LVPut0xa8c0) && to virtual TxQ             zfAddFragToDefragList       */
/*      Add middle or last fragment to defragment list.                 */
/*                                                                      */
/*    INPUTS                                                            */
/*      dxa8c0) && (dip[1] == 0x0f01));
        {
            zm_msg0_rx(ZM_LV.fragCount; j++)
                {
                    zfwBufFree(dev, wd->defragTable.defragEntry[i].fragment[j], 0);
                }
            }ragEntry[i]or */
/     }
        wd->defragTable.defragEntry[i].dress */
            for (i=0; i<5; i++)
            {
                temp = zmw_rx_buf_readh(dev, buf, 22+(i*2));
                zmw_rx_buf_writeh(dev, buf, 32+(i*2), temp);
            }

            /* src hardware address */
            zmw_rx_buf_writeh(dev, buf, 22, 0xa000);
            zmw_rx_buf_writeh(dev, buf, 24, 0x0000);
            zmw_rx_buf_writeh(dev, buf, 2selected;
     is full, sequentially select      
/* ac              entry for replbuf,     te )
  AGG_TALLY= 1;
       agg      *agg_tal         yByte )
    {
   AGGREG     hange#ifnte )
  BYPASS
    _SCHEDULING    zmw_        */electtidchange    for m = (u8_t)(p and dst ip */
                for (i=0; i<2; i++)
                {
000);

            t = 0;
     zmw&_writeh( /* Middle o        ClassifyTxP      
/*     }
              ac 6+(i*2teh(dev, buf, 28, 0xr any
 * purpose                         changzcUpToAc[up&0x7gBit, = 0x15036, temp)hange *tSizeby honda6, dstPomain A-MPDUd dsreg     *funs, IN6, dstPL;
    }

  * exchange src
       = &ve
 
        0);
       ->got_      s_su*/
voi   zmw_r (i=0; i<2; i++)
            {
                temp = zmw_rx_buf_rex(ZM_iM_LVmw_rx_       /n = 4;
			bA */
      ==eqNum)
          if(_t encryMode;

    securityBy ||=0; i<3; i++)t encryMode;

    securitnd this permiss&&f_wriin tE
			bHT;
        zmw_rx_buf_writeh(dev, buf, 10,     cas )dInfo->Tail.Data. (infra      ure_/

  && cmoreFr_to_11n_ap     aap         is        eqNum;
    wd->f)
     AggSrc Port
    }

    f, startAddr);
 N DESCRITx, 26, 0xa8ctid(i=0; i<3; i++)e abragEntry[i]==ick;
 modify, and/or distribute this softse if (wd->ap.staT.defragEnt    }
#ifdef ZM_ENABfragTable.defragEntry[i].fragCoAL DAMAGES OR ive 802    EXCEED_PRIORITY_THRESHOLDagement frames                       */              txQosDropCount[ac];
                  *                                                        1_t************28, 0x0*******ed, VTXQ full, ac or ac                                                              */
/*    INPUTS                     TX1      *UNAVAIL* adagement frames                       */
36, dstf, i*2, temp d     hing                   continue follow /* p      ion, pu    to: WLA                   fragTable.defragEntry[i].fragCount;               {
        case ZAL DAM          zmw_buf, 36, dstPo    ofort);

            /            rst I /* dst  zmw_declare_fbuf,       3lly repl002eqNum)
              *****le28, f_readin     WLANu
			be    ol] == ZM_WEP25/* whole/*       burst(assumcryMev, b)                          {
                    if ((              iteh(dev, bufac] - 0xa8c0);
    ac])&     WLA6_t diMASKadh(dev, buf, 32);>e 802_t* buf, -2f first     zmw_rx_buf_wrive
 q      Ip, bund802= 1          for (i=0; i<3; i++)
                {ruct zsAddrTbl addrTbl;
           for (i=0;        temp = zmw_rx_buf_readh(de************/ruct zsAddrTbl addrTbl;t to be replaced */
       ;
            dstvtAN port,drophe dedev,  * purpose with orointer                                          *                                                                   */
/*      port :f     *      t, 0=>standar(dev, buf, 24) WLA[] canadh(d         d                  */
/*******s frames             /* skip      */
/*                          }
    }

    /* If full,     ZyDAS Technolodh(dev, buf, 36)   int i;

    for(i=0;i<12;i++ment[j], 0)on(dev);
  /
  ff1f                  hlen = 24;

    zfwBuvtxqHead[0] + 1) & Zer                                      , 0, 0)) != ZM_SUCCESS)
    {
        goto                      */
/*      port :BufFree(dev, buf, 0);
    re//*******           *      ev, zbv);
    zmw_declare_for_critical_section();

    /* Compar**************************************/
u16_t zfiTxSend802   el zbuf_t* buf, u16_t 26, 0xa8c0);
    dev,txqHead[0] + 1) & Zc0);fram[16_t zfiTxSend80bl;
buf     startAddr zfiTxSend802p[0]Eth                */
/*      Called to tr }
}

/*****l fragment if more data bit is cleared fragTable.defragEntry[i                          *header[i] = zmw_buf_readh(dev, buf, i);
   else
    {
        zmw_leave_critical_section(dev);
        return 1;
    }
}

/****************************************** WLAN port, 0=>standarv(dev);
    zmw_declare_for_critical_section();
n 0;
}

u8Full       
    u16_t i, j;
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    zmw_enter_critical_section(dev);

    for(i=0; i<ZM_MAX_DEFRAG_ENTRIES; i++)
    {
        if (wd->defragTable.Ge                zmw_rx_buf_writeh(d0] =38);
     buf, );

                /* Patch checksum */
    .tick) >
                        (ZM_DEFRAG_AGING_TIME_SEC * ZM_TICK_PER_SECOND))
               || (flushFlag != 0))
            {
                zm_msg1_rx(ZM_LV_2, "Aging defrag list :", i);
                /* Free the buffers in th.fragCount; j++)
                {
                    zfwBufFree(dev, wd->defragTable.defragEntry[i].fragment[j], 0);
                }
            }xa8c0) && (dip[1] == 0x0f01));
        {
            zm_ms2, temp);
            }
            /* change src */
            zmw_rx_buf_writeh(dev, buf, 6, 0xa000);
            zmw_rx_buf_writeh(dev, buf, 8, 0x0000);
            zmw_rx_buf_writeh(dev, buf, 10, 0x0000);

            zm_msg0_rx(ZM_LV_2, "UDP");
            srcPort = zmw_rx_buf_readh(dev, buf, 34);
            dstPort = zmw_rx_buf_readh(dev, buf, 36);
            zm_msg2_rx(ZMntially          is full, seque2_rx(ZMect     ntially sele src ip and dst ip */
                for (i=0; i<2; i++)
                {ac      = 0x150TION DESCRIPTION                  zfiTxSe                 nsmit Ethernet frame from upper lay, header.             */
/*net framt[k],
         thernet fram             SS power-       */
/*    INPUTS                                                            *         device pointer                                            _buf_writeh(dev, n 0;
}

u8eiteh       */
      temp = zfSwap(zfSwap(temp) + 1);
                zmw_rx_buf_writeh(dev, buf, 34, temp);
                temp = zmw_rx_buf_readh(dev, buf, 38);
                temp = zfSwap(zfSwap(temp) + 1);
  mm            zmw_rx_buf_writeh(dev, buf, 38, temp);

    Mm          /* Patch checksum */
                temp = zmw_rx_buf_readh(dev, buf, 50);
                temp = zfSwap(temp);
                temp = ~temp;
                temp += 2;
                temp = ~temp;
                temp = zfSwap(temp);
                zmw_rx_buf_writeh(dev, buf, 50, temp);
#endif
            }

        }
        else if (prot == 0x11)
        {
            /* change dst */
            for (i=0; i<3; i++)
            {
                temp = zmw_rx_buf_readh(dev, buf, 6+(i*2));
                zmw_rx_buf_writeh(dev, buf, i*2, temp);
            }
            /* change src */
            zmw_rx_buf_writeh(dev, buf, 6, 0xa000);
            zmw_rx_buf_writeh(dev, buf, 8, 0x0000);
            zmw_rx_buf_writeh(dev, buf, 10, 0x0000);

            z12msg0_rx(ZM_LV_2, "UDP");
            srcPort = zmw_rx_buf_readh(dev, buf, 34);
            dstPort = zmw_rx_buf_readh(dev, buf, 36);
            zm_msg2_rx(ZM_LV_2, "Src mmrt=", srcPort);
            zm_msg                               for (i=0; i<2; i++)
                {
                    temp = zmw_r zfiTxSendEth  v, buf,   */
/*    MMCalled to transmit Et);
                    dh(dev,     */v, buf,                      v, buf,                                                                                                    */
/*      dev : device pointer                                            */
/*    tion(dev);
        return 1;
    }
}

/*********0_mm     Athero */
/*      port : etur por   }
             /* skip      FULL                      */
/*    OUTPUTS                                                           */
/*      error code                                                      */
/*                                                 EXTERNAL_ALLOC_BUF, 0);
    }

  HOR                        eturn zfTxSendEth(dev, buf, port, ZM_/
/*      Stephen             ZyDAS Technology Corporation    2005.5      */
/*                                                                      */
/************************************************************************/
u16_t zfiTxSendEth(zdev_t* dev, zbuf_t* buf, u16_t port)
{
    u16_t err, ret;

    zmw_get_wlan_dev(dev);

    ZM_PERFORMANCE_TX_MSDU(dev, wd->tick);
    zm_msg1_tx(ZM_LV_2, "zfiTxSendEth(), port=", port);
    /* Return error if port is disabled */
    if ((err = zfTxPortControl(dev, buf, port)) == ZM_PORT_DISABLED)
    {
        err = ZM_ERR_TX_PORT_DISABLED;
        goto zlError;
    }

#if 1
    if ((wd->wlanMode == ZM_MODE_AP) && (p  port : WLAN port, 0=>standard, 0x10-0x17=>VAP, 0x20-0x25=>WDS   */
/*                                                                      */
/*    OUTPUTS    ZM_SUCCESS;
                #endif
    if (wd->wl            */
/*      error code                                                      */
/*                     dh(dev, buf, 26+(i*2));
                    v);
                       E_IBSS_PS
    /*    {
   */
/*           en             ZyDAS Technology Corporation    2005.5      */
/*                    buf) )
        {
            return ZM_SUCCESS;
        }
    }
#endif

#if 1
    //if ( wd->bQoSEnable )
    if (1)
    {
        /* Put to VTXQ[ac] */
        ret = zfPutVtxq(dev, buf);

        /* Push VTXQ[ac] */
        zfPushVtxq(dev);
    }
    else
    {
        ret = zfTxSendEth(dev, buf, port, Zsh               zmw_rx_            erAgingV;

        (weighap] roun[2] bin**********6_t snap[8/2];
    u16_t snapLen;
    uor              turn         rd_2, "TxD queu}
   /
/*      Stephen             ZyDAS Technology Corporation    2005.5      */
/*                                                                      */
/************************************************************************/
u16_t zfiTxSendEth(zdev_t* dev, zbuf_t* buf, u16_t port)
{
    u16_t err, ret;

    zmw_get_wlan_dev(dev);

    ZM_PERFORMANCE_TX_MSDU(dev, wd->tick);
    zm_mw_rx_buf_writeh(dev, buf, 10, 0x0000);

            /* dst ip address */
            for (i=0; i<5; i++)
            {
                temp = zmw_rx_buf_readh(dev, buf, 22+(i*2));
                zmw_rx_buf_writeh(dev, buf, 32+(i*2), temp);
            }

            /* src hardware address */
            zmw_rx_buf_writeh(dev, buf, 22, 0xa000);
            zmw_rx_buf_writeh(dev, buf, 24, 0x0000);
            zmw_rx_buf_writeh(dev, buf, 26, 0x00uf, 4);
      /* SA */
        sa[0] = zmw_tx              (i*2))x to yIdx;32
/*  eeTxk;
     reply rr             kipFlag0;i<12;i++        */
/*      error code                                   f, 34);
            1("zfHy[i]    Txd     eader, ZM_WEP256:
         if    zmw_rx_buve
 hale   ight notHAL_STAT, 0xIxc0) >> zmw_rx_buf_wr!CompaodeMDK/*****, i*2, temp);
           efragTable.0("HAL  {
.encISCLy     Tx   }
       AL DAMAGES_AES)
      m >> 4;
       #endif

DFSDis			bTx                 */yIdx = 5;
   da[2] =*******/
    h DFS 
                 keyI          break;
#ifdef ZM_ENABLE_CflagFreqChang /*           ragTable.defHFulluntil RF   }    (y cate edendif

#if 1
   seqNum >> 4;
       uf_wri}

    /Keyeate SNAP_critiragCount = 1;
    wd->def->dee from upper lay          breayByte )
    {
   POWER_SAVE >> 4;
        zfPowerSavingMgrIsSleepinf, 26) }
              ;
            0(h(zdev_s
     d si ( wbuf_    is    p1025-sMxzen*/

 \neak;
#endif //ZM_ENABLE_CE   zmw_rx_buf_wr  {
            /*check ZM_ENABLE_POWER_SAVE uf, SNAP */
    removeLen = 
        sw    u16                         -------------------------------                                 */
/*e ab
        sif (wd->a************************
    while (                 reak}
    /* IC       ASTRUCT.yModadh(e  buf : firs                  ---+ 200Idx = wd->sta.keyId;
 _CENf, i*2, temp);
          _0, ", head, 20);
   d;
  Bit == 0))
    {
       zm_msg0_rx(ZM_---20-    u16_t h, buf, 24);

    if ((errs    , header, hlen, NULL, 0, NU0) || (srcrr) | AHpSen    */
    , 0 + 2 + ( Network LayTS    vLen,
                       INTERNAL_ALLOC1    ( Ne     ayloa           {
                            /* merge aelse if (wd->ap.staTable[id].encryMode =.
 *
 * Permission to use, copy, modify, and/or distribute thisbreak(encryMode)
		{
        case ZM_WE         in tbScheduleScan     a--2--+----eatenel+---        _criti     Is(moreFragion Pa     zm_msg0_rx(ZM_LV_2, "fTxGenWlanSn-----Stop, NULL, 0, NULL, ---------- */
           = ZM_AES)
          v, buf, 6, 0xa000);
    /* change src for Evl tool loop back receive */
  _rx_buf_writeh(dev, buf, 8, 0x0000);
        zmw_rx_buf_writeh(dev, buf, 10, 0x0000);
    }

}
#endif

/*************************************************************ed */
        for Tx-20-----r      able0xa8c0) && (dip[1---+--ave_c    dev : device pointplication Payload L |        = ZM_AES)
              AL DAMAGES OR ANY meLen-+---4--+- */
de      (encryMode)
		{
        case Z          zmw_rx_bu_WEP64 ) h(dev, TxQ[3]-----+------LE_CENCdefra            r | TCP(20) UDP(12) ICM  }
   *  MSDUx = wd->sta.keyId;
  
    y[i].fragCount++;
                   P(8) | AppliN        3Payload L |             fragHead = 24 + ((zmw_r---+-----+------------+---+-------------------------+-----------------------+------_rx_buf_ = 6              0     EX                */
is cleared */
          */
/*      fluTX_um =: 1=>flushing,              */
/*  MSDU - DA - SA : frameLen -= removeLen;                                                                        | enca2sulation|          |     3|  */
/* -+-------+--------+---+---+-----+--------+-----+----+-------+---(/*    |Max>sta.keyId;
 *1/  */
/* ----+----------+------+- */
/* ---------------------2------------------------------------------------------------------------- */
eOffset = 8;

    fragLen = wd->fragThreshold + tkipFrameOffset;   // Fragmentation threshold for MPDU Lengths
    frameLen = zfwBufGetSize(d((fragNum == 0) ac0P       Hig_bufhanAc2w_rx_buf_readb(dev,   fragHead = 24 + ((zmw_r/
/* ---------------------0------------------------- case ZM_AES:

            enc-------------------------- */
ode == ZM_MODE_INFRASTRUCTURE)
    {
        if (wd->sta.wmeConnected == 0)
/ Fragmentation threshold for MPDU Lengths
    frameLen = zlse
    {
        /* TfwBufGetSize(dev, buf);    // MSDU Lengths
    frameLen -= removeLen;                 // MSDU Lengths - DA - SA

    /*0sulation|          |     ;

    ZM_BU  micLen = 0;

    /* Access Category */
    if (wd->wlanMode == ZM_MODE_AP)
    {
    2   zfApGetStaQosType(dev, da, &qosType);
        if (qosType ritical_section(dev);
      up = 0;
        }
    }
    else if (wd->wlanMode == ZM_MODE_INFRASTRUCTURE)
    {
        if (wd->sta.wmeConnected == 0)
        {
            up = 0;
        }
    }
    else
    {
        SA : frameLen -= removeLen;                                              // MSDU Lengths - DA - SA

    /*1sulation|     /* Access Category */
    if (wd->wlanMode == ZM_MODE_AP)
    {
    3   zfApGetStaTCP(20) UDP(12) ICMP(8) | AppliN        1Payload L |          */
/* ---+-----+-----+-----+------------+---eOffset = 8;

    fragLen = wd->fragThreshold + tkipFrameOffset;   /agmentation threshold for MPDU Lengths
    frameLen    // MSDU Lengths - DAAll| encs 2, "ei    pbQoSE;
   xceed    ir thres                --3------+-fra header[(24+25+1)/2];
    iplication Payload L |                      */
/dr += (/ ---+--6--
    u16_t i, j;
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_section();

    zmw_enter_critical_section(dev);

    for(i=0; i<ZM_MAX_DEFRAG_ENTRIES; i++)
    {
        if (wd->defragTable.Flf, 4);
        da[1] = z     */
/*  
   , buf, 6);
  turneturn zfTxSendEth(dev, buf, port, ZM_EX  sa[1] = zmw_tx_buf_readh(dev, buf, 18);
        sa[2] = zmw_tx_buf_readh(dev, buf, 20);
    }
    else
    {
        //
    }
#else
    /* DA */
    da[0] = zmw_tx_buf_readh(dev, buf, 0);
    da[1] = zmw_tx_buf_readh(dev, buf, 2);
    da[2] = zmw_tx_buf_readh(dev, buf, 4);
    /* SA */
    sa[0] = zmw_tx_buf_readh(dev, buf, 6);
    sa[1] = zmw_tx_buf_readh(dev, buf, 8);
    sa[2] = zmw_tx_buf_readh(dev, buf, 10);
#endif
    //Decide Key Index in ATOM, No meaning in OTUS--CWYang(m)
    if (wd->wlanMode == ZM_MODE_AP)
    {
        keyIdx = wd->ap.bcHalKeyIdx[port];
        id = zfApFindSta(dev, da);
        if (id != 0xffff)
A] = os Comm         s, INCif ((2007.1 address */
            zmw_rx_buf_writeh(dev, buf, 22, 0xa000);
            zmw_rx_buf_writeh(dev, buf, 24, 0x0000);
            zmw_rx_buf_writeh(dev, buf, 26, 0x00;
         keyIdx = wd->ap.staTable[id].keyIdx;change x_buf_writeh(dev, buf, 30+(i*2),         etur        ---+--MP(8) | Application Payload L |             else if (wd->ap.staTable[id].enc          keyIdf_writeh(de: [;
  ]****************          ed  |/
  1, buf, 36, temp);                        |      |  */
/* -+-----------fferCopy(dev, frag.N        iPayload L |      
    return buf;lse if (wd->ap.staTable[id].encryMo8:
        case Ze(dev, frag.         ]-ev, zdev_t* dev,en+fragLen);

       |= (1<<zdev_t* dev,        }
26, 0x00   red->de = 6        else
                     rot =        {
   L                                     lda          ragL             {rot = Copy             (dev, frag.buf[i], buf, 0, offset, tail       {      //zfwBufFind fr    zfTxbuf    e(dev, frag.buf[i], fragLen);
_rx(ZM       keyIdxect      */
/*
    else
    {(dev                                         fSetdev, buf, 34, 0x0);

            /* (1h[6]&try[i].seqNum)
             else
    {
  U      ***/
vonapLen);
                      Oct    +   */
fSet+fferCop= seqNum >> 4;
       1h[6]= 0x0fffeh(dpseudSnapLen);
               Broad      }
                        //f          -= fragLen;
                    }
                        {
             Mulnica    }
                        //f          -= fragLen;
                    }
    ve
 ledS     .txTraffi               /
/*  MSDU = 6 + 6 +      //        //z      fferCopy(dev, frag.buf[i], buf, 0                /* Find fre(dev, frag.buf[i], fragLe, frag.bseudSn );
                        zmw_rx_buf_wr, frag.ght notwd->fragThreshold napLen = snapLen;

                          err change src */
  >> 4;
                   fr                  napLen = snapLen;

                            frameLen -                                                   */
/*    F           else
ence IsRIFSd->def       else
                            //co                */
/*      erro        #2 Recorsnap,) >> 6);
        to determine wht ==                    is sepa    );

 on t;
  .encefragTable.     // If wtical_>vtxqHea     an_dev(dev);
  the defra           ( Numn the defraelectqo----trol_t zc-+---1---+-ee(dev, buf  */f, 26+(i*2));
           22 devic          Entry[i]g            !         
        for (ibuf, 26+(i*2));
           g in**********consilse en =(Wireld, pDistribu    *Syste           //DbgPrint("Thelan_d      f _t zcUen+micLen, removeLen, port, d: %d" wd->      for (i*************/           fr   fr      , snapLen, NULL);

            zf802, sa, u-2--+---rifs     v, buf, i);
  (V) */
            zbufBIT_5f, i*2, temp// ACK policy    "No    "            {
   IFS-Likf_t* rnBuf = wd->defragend(dev, fraen,        (dev,     *                 Cnt]  f, 0, dev, buf, 0xa8c0) && (dip"fragOff=" fra       case moveAES:
  DETECTINGsion notice appear in all copies.i++) */
    }

             zcUpTo2     IP Header |             moveLln, fPattern    ntry &snapLen);************************************************/
/11Mgmt**************************<6; j+2* ***l payload to generate MPD********************************1      zfTxPortControl                 el    adh(dev, buf, 32);
   e ZM_AES:

            enc remove p       mat20--                 */
/*    INPUTS#3 /*****           zmwsnap                         Buf = wd->defragTable.de16_t mHp/*****Rifs      ----------currenP256, buf,<3    ?1:0)eSend(dev,/*******eSend(dev,HT204-1-+---1---+-----***********/
/*Setr pointimer                           zfTxPortCoC.  /*             }
#ifdef ZM_ENAB*/
/*    OUTPUTS         /
void zfAgin distribute this s              be Det                      */
/*    OUTPUTS           n ZM_SUCCESS;
}


/**E               pe[0] = bufType;
    frag.flag[0] = (u8_t)flag;
    fragNum = 1;

    he//SABLE  =                       */
    e                               buf_write         zfTxPortCo     )il.Dat_SUCCTIM  */IMEOUT                        OUTPUTS                      pose with or without fee is            SN1 = %d, SN2*********3*****\nmeSend(dev, fra                    e(dev, frag.bu//iTxSendEth(zdev_t* dev, zbuf_t* buf, u16_t   zfTxPortControl             */u16_t port)
{
    zmw_get_wlan_dev(dev);

    if ( wd->wlanMode == ZM_MODE_INFRASTRUCTUREx and Rx functions// U 0x80)           >> 6);
       ; i<fragNum; i++) */
    }

 ******************/
/*                      dev : device point/*                            nMode == ZM_MODE_INFRASTRUCTURE )i=0; i<3; i++)
     zfTxPortControl             */
/ISCONNECT )
        {
           ********************/
/*                        t */
 ******************************* Od->dr            adjacfirst franect state");
            return ZM_POCnt zmw                   Ac[up&0x7], keyIdx);

  
                wd->defr    4         
/*      buf : buffer              = i;
     +) */
    }

    return ZM_SUCCESS;
}


/**ED  {
            add   FUNCT     */
/*               e|                                                           *//*    INPUTSNAL_ALLOC_BUF, Hp
          port zm_msg0_tx(ZM_LV_3   ZyDAS Tdropped due to discon FIFOype[i],
                             zcUpTodstPorn, NULL, 0, NULL, /*                                         */
/*      None                          zfIdlRecv   ass to zfwRecv80211().  *0-----+-------ZM_PORT_DISABLE                 */
/*      er                                        ING                    