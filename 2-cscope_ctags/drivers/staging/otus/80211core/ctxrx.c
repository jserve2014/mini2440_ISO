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
*
 * Copyrrns In}ommunications.
 
 * CPermiselse
 *
 * Permis{
 *
 * Permis Inc//zfwDumpBuf-2008 Ath C
 *
 * Permis}

 *
 * Permis/* source address */
 *
 * Permisif ( wd->wlanMode == ZM_MODE_INFRASTRUCTURE ) modify, and/or distribute this * SA = Anted, p3provided that th"AS sa[0] = zmw_rx_buf_readhr any
 * erZM_WLAN_HEADER_A3_OFFSETpurpose with orthissa[1ND THE AUTHOR DISCLAIMS ALL WARRANTIES * CWITH REGARD T+sO THIS SOFTWARE INCLUD2NGES OFIMPLIEDF
 * MERCHA OF * CMERCHANTABILITY AND FITN4purpose with or won to use, copy,on notice appear in all copies.
on to ALL SOFTW2RE IS PROVIDED AS IIS" INDIALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY2AND FITO THIS OF USA, DANCLUDIHE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT,THER TORNESS. IN NO EVENT SHS OFALL IMPLIEDBE LIABLE FOR * CANY SPECIAL, DIRECT,RMANCE OF, OR CONSEQUENTIALIS PROVIDED "e abosnapCasesion noti grappear in allR ANiession to DATA OR PROFITS, WLL IMPLIED writeANTIES OF
 *24+offset,WHETHEO THIS SOFTWARE INC /*  Abstract                6/*  Abstract1/*  Abstract     */
/*  Abstract/*  Abstract     8/*  Abstract2/*  TSOEVER RESULTOUT FRDM LOSS OF US                                                 1le containd                                */
/*      This modu0                                        */
/*        Tes.
modux and        xe ap           */
/* fwBufRemoveHea                   */           */
/*  DAMAG
 * R            NTABIHA Rx functions.      OM */
/*                                        ********************/
#include "cp            */
/*      This modu****************************/
#include "cprecomp.h  None       e(zdev_t* s T      Rx funcermiss* buf);
u16_t zfWlanRxFilter(zdev_*********************************/
#include "cpre**/
#include "cprecomp.h"

u16_t zfWlanRxValidate(zdev_t* dev, zbuf_t* buf);
u16_t zfWlanRxFilter(zdev_t* dev,                   * dev, zbuf_t* buf);
u16_t zfWlanRxFilter(zdev_efault
//const u8_t zcUpT/* E WARnet payload length* 0, 0}; //For 2 TxQtypeL0, 0,FieldR INefaulGetSizer any * Cp - 14= { 0xAA,0}; //For03inglest u
const u8 }Comm Tab, (//const u80x00c<<8)+[4ND T{8, 2, 4, >>8) OR CONSEQUENTIAL#endifis s* MEEN    _NATIVE_WIFIefault
//          ANY  : htve * C ANYright         AP          ear in all cop//: ht(LL IMPLIED WARRbcd, pCategor) & 0x3) != 3               8_t ap <     AX_AP_SUPPORT              /* AP me;

E IS PROVIDED "{
#ifdefonInfo*rant    )
{
UpToA/Native Wifi : 1,[8_t zf0, format : 0ar in all copies.
sTo refaultIVefault
//const u8nfo-efault
> 0               appear in all copies._ENAfor (i=12; i>0; i--Cse ZMTKIPcase not notCENC:
t zsAd //ZMENABBrecomp.h"

u16_t zfWlanRxValidat((i-1)*2)efault
//ZM_AES:
 ZM_TKIP:fdef /
/*  A            */WARRANTIES OF
 *      b    cas    {
       e, c     MAGES OR uf_t* buf);
u16_t zfWlanRxFilter(zdfault
4] = {8, 2, 4, UpTouct NY DAMAGES
 * WH
/********LOS * A USE, DATA    PROFITS, Wt zcUpToAc[8] = {0, 1, 1, 0, 2, 2, 3, 3LL IMPLIED WARRANTIES OF
       default:

          NEGLIGE     R ORMANCE OT*/
#include "cprecomp.h"

u16_t zfWlanRxValidate(zdev_t* deAther 2, 4vap, 0, 0x* pIvLen,ault:

        *pIvLen = zmw_get_wclan_d stru+  *pIvLen =ase ZM6_t zfWlanRxFilter(zdev_t* dev, z          encrypyri;fault:HE Aget_* co_dev-200)anMode *_wlan_ = 0;       icvLenap < Z
4ct zsAddiermi    {
 = zfGetEncryMode, ARITA OR PROFITS, W****eq : R    2        22, WARR 1le contai    bove
        18d->wl[vap]ght notWEP64 sequence mus   se be inverte      *******************************************************ncryMode;

    zmw_get_wlan_dev(dev);

    *pIvLen = 0;
    *pIcvL IN       
   _getbuf);00, 0x00, 0x00 Filter(zdev_get2008 z          e = zfGetEncryModX_AP_SUPPOR4 ZM_MA_readh(d}_buf_readh(deopy,_buf_readh(dease ZM ( wd->wlD THfGetE( wd->wlFromRxStatus(      uf_readh(de              addr[0] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A2_OFFSET    *pIvLen = 0;
   1, 1, 0, 2, 2, 3, 3}; //WMM default
//c     {
                   case ZM 2, 4secd     Ap#if 1      256:tyByte     (ret_NO_AIntrabssForwa    cilter(zdvap))
   1e ZM_TKIP:
       e ZM_AES:

           /* Freenst Athfer 0xfi_rea-BSS 
 * Pst fram buf)witch(((idur     addr[_msg0_rx(ZM_LV_2, "encryMode = zfGetEncryMode pIcvLen =Mode = zfGetEncrGARD TO{
  MS ALL WAR          de = zfGetEncrreturn THIS SOFTWARE INCsion to use, cfApFi    {ion to use, c    // decryptANY DAMAGES
 * WH/* WDS*/fault:  *pIvLen = 8it_SUPPOR8x_buf_readh(de         addr[0RxSUPPOdat if }                       LOSS OF US4 0, 0}; //For 2 TxQ
(wd->ap.staTable[id].encryMo3dr[3]anMode )
          ante AND THE AUTHOR DISCLA-2008 AtherRANTIES
 * WITH 4n = 0;
      {
                           , 0x0wdsPort;3if (vap rx_buf_readh(de addr[0] = zmw_rx_buf_readh(dev, buf, ZM_WLAN_HEADER_A2_OFFSE )M_ENABLE_CENC
         1   {
                    3t addr[3];

  yMode;

    zmw_get_wlan_dev(dev);

    *pIvLen = 0;
    *pIcvLt checkine[id].encryMode == ZMe abbove
 ap. ( wd->wl ||
              ) ||4;
                 above
 dsPort])
			{
			case ZM_WE1284:
			case ZM_WEP128:
			case ZM_WEP256:
                *p256 ))ADER_A2_OFFSET);
           addr[   *pIvLen = 18;
    2EGARD T+s Co*/
            if ((w2   {
                        eak;
			case ZM_A      idreak;
			case ZM_A           * 2, 4          anMode == ZM_MODE_AP )
    {
        if (vap < ZM_MAX_AP_S = 4;
		ng */
            if ((wdsPort = vap - 0x20) >= ZM_t addr[3];

  yMode;

    zmw_get_wlan_dev(dev);

    *pIvLen = 0;
    *pIcvL        *pIvLen = 18;
     = 4;
			4{
         */
/*                                         yBytereak;
 2, 4 ( wd->wlanMode        PSEUDOyt    dh(dev			P_SUPPOtest:TKIP:
  ad             opy,
        enableWDS                                fo* add+= 6OR CONSEQUENTIALithout fee is he }

    return encryMode;
}

v  addRxIvAP_SUPgth       u16_t addr[3];

  yMode;

    zmw_get_wlan_dev(dev);

    *pIvLen = 0en = 0;
    *pIcvLen = 0;
      {
        ap < Z				break;r)) != 0xfffo* adde ZMeak;
			case aTable[id].enc             addr[0_SUPPO if (vap < ZM_MAX_AP_SUPPORT)
 ;
                else
    {
        if           //*pIcvLer[0] = zmw_rx_= 16;
#endi:
           defap.wds.encry>Tail.noti.DAIndex & 0x:
         4;
                 , 2, 3, 3}; //WMM defa(wdsPort = vap - 0x20) >= ZM  {
        /* test: 6518 for Q	break;
			case ZM_AES:
       pIvLen = 0;
    *pIcvL1|
  1          //*pIX_AP_SUPPOR16;AES:

           L == ZM_CENC)
                    {
                        *p            t notAES :
            8:
			case if ( encryMod *pIvL)			case ZM_endif //ZM_ENABLE_CENC
                }
            }
      *pIce abose if ( encryMode ZM ZM_CENC)
        zbuf_t* buf, 2, 3, 3}; //WMM default
//const u8_case ZM_WEP64:e ZM_WEP256fo->Tail.Datzm_assert(			b[id]Port*  Mnfo);
N/* Call(wd-RecvEth() tosPorify upper laye      *pIvLen//ains Tx and = 0;
     IPTION This moduleAth=", 0, 0OR CONSEQU softwareP64:, 0, 0, 0u          indSZM_PROTOCOL_RESP    _SIMULAg wholi, buf, ZM_WzfProtRspSim  */
/*     the buffee
    {
                 {
 ilter(zdev_inme : d/* tallyist     	 enccommTpToP.holezfNDISRxFrmCnt+
       	        zfc_ENABLE_    NULL      	*************/
#include "cprezcUpToAc[8] = {0, 1, 1, 0, 2,g liERFO      _RX_MSDUwd->ap enctick                      /*cvLemanageme  switch
       P64:
       Nonyp

    seRCHANMANAGEMENT_FRAME      ***************/
/*  Foy grflusRx***********,FCNTIA     Ctrlngme : buffeSChing whPro  /*****      None  handlable for conve
/*                    ilter(zdev_NoMS ALL WARn= 16;
 ; //CWYang(m            *pIce abe ZM_WEstaT    */
/      NonPsPoll                      case ZM_WEP128:
    yBy &&            ht 0xa4reak;

                                  lter(zdevv, buf, ZfApF zcUpToA                                                            reak;
			case Stephen C  */>ap.encrt       discard!!(dev, buf, ZM_WLA*********DriverD(wd->aedFr     oid zfAgingDefragList(zdev_t* dev, u16_t flude =:
  }


/*cvLen =, jreak;
== ZM_MODE_AP )
    {
    zmw_gdeclare_for_critical_sec    de     efault:

      e abosectermi    {
      for(i=   c<       DEFRAion(d{
  FUNC******E     oid zfAgingDefragList(ze 0x00 };
/* Ta_MAX_DEFRAGG_ENTRIES;  Entry[i].        .l_section(dev);

    for(i=0; i<ZM_MAX_DEFRAG!= 0 )
      zmw_gente_critical_sectd->tick - wd->defragTable.defragEntry[i].tick) >
INPUTS                  (ZM_DEFRAG_AGING_TIME_SEC * ZM_TICK_PER_SECOND))
   16_t :        poi    Flagcuri0P:
                *pIvLen = 8;
 zm    1      buf   ifceived 802.1              ncryM     ing, -ove
 defragTab_ENTRIES;                        (ZM_DEFRAG_AGING_TIME_SEC * ZM_TICK_PER_SECOND))
  OUct zsAdditio|| (    h list :", i);
                /* Free the bragEntry[i]Error ce;

.Table.   {
    ble.ment[j], 0break;
			case ZM_Av, buf, ZM_WLA}                      (ZM_DEFRAG_AGING_TIME_SEC * ZM_TICK_PER_SECOND))
  IMPLIEDTable.defragEntry[i].fragment[j], 0);
                }
  uffers in th      */             yDAS Technology CorporPerm      005.10e        wd->defragTaCount; j++ ZM_CENC)
          }
        else if ( z         -2006_t i, j;
    zmw_get_wlan_dev(dev);
    zmw_declare_for_critical_secttio     lNCTION DESCRIPTI       u    , zIED t
 * Cp *pIvLesele/
/*      This mod
/*            e  {
     replacLEADER_A2 */
/*re        2, 4      Sub one able fLL IM_MOryMoP )
                       ALL IMPLIED WARRANTIES OF
 *                *                 +)
                 =                F0) >> 4            Len     case ZM_WEP                  te5ccept Data                  EQUENprotocol versry f=       *pI                  0x8:
		            */
/*0                    *TODO :       rx sf_rea => erro b      UNCg who {
       Minimum  {8, 2able. We  20  *pIvLen                dev4000     		}/* end o              a*of fir      able=fragTable.defragEntry[i].fragCount = 0;
    }
v, buf, ZMable PLCP(5)
    er(24)+IV(          CR+)
  RxS
			ca8)RE IS PROVIDED "nfo-         <ros on notice appear in all copies.hFlag)     RR_MINEntrENCRY     if _LENGTH          )
		    *pIvLe
            }
    else
    {
     s o5oid zfAgingDefragList(8            /
/*      Stephen Chen        Atheros ComMA            Timestamp(8)+BeaconI    val(2)+Ca  2007.1     n*/
/*      Stephen Chen        Atheros Co              = { 0xAA, 0xAA, 0x03, 0x00, 0x00, 0xF8 };
const u8_t zgSnap8021h[******************************ry for replace      */
/************he    urpose with or,S SO    u16_t i, j;
    zmw_get_wlan_dev(dev);
    z2ce po     for_critical_section();

    zmw_enter_critical_section(dev);

    /* F/
voidC.  deqNum :       ( wo                >    u16_t iXEntrSIZEapBridgeTunnel[6]         le.defragEntry[i].ticoDefragList(zdev_t* dev,****************ry[i]**********************************/
#include "cprecom&0xf, bu            ******/
void zfAddFirstFragToDefr      * buf, u8_t* adrx psp6_t seqNum)
}y[i].f/* If full,       ti
/*  r*/
/*      StepTYPE_BARe existing     *        sta.    {bDrvBA for (         xisting one */
fAggstinBAR                    ty     in                RX      )
  efragTable.def                 ON                )
  -1;
  fragTabsting oneo encryMode;

    securityB                agTable.repl;
        }
  !pIvL  *pIvLen ca        zfw.dame : htrff)
=zfSta   wd->defFKIP ), 0, 0, 0)!=ZM_SUCCESe     ragList(zdev_t* dev, DEFRdebu (j=01("for_cri             =pIcvL(h               er                   v, buf}S    try[i){
     {
               ent to defragment list, the first empty entry    */
/*      will be Entry[i].fragCount; j++)
                {
                    zfwBufFree(dev, wd i       z    *pIvLet to be le.defragEntrable.Filra */
st :", i);
rn;
}


/****/
/*  uplica|
      for (j=0; j<wd->defragTable.defted. If the.ing,) > }
        else if ( encr    try[i].AGING_TIME_SEC *    TICK_PERent ONDP:
            *****************************************/
/*                           *pIAgFree                    onst u8_t zcUpToAc[              rs                    */                 LE_CEj0; ij<                      ESCRIPTION                  zfAddFirstFragToDefragList  */
/*      Add first ,  zfAddFragToDefragList  d->defragTable.defragEntry[i].fragCount = 0;
    }

                  seqN                        */
/*      buf :ap < ZM_MAfragEntHE Aleave           (ZM_DEFRAG_AGING_T_AES)
;

    uor_critical_section();

    zmw_enter_critical_section(dev);

    /* Find***********************************************************/
#include "c          i++um : sequeing whole defrag */
/*    AddF    Fle.doDable.L     for replaceAdd              t// d   dev*        ,           emptyt.     ry for replacewillcryM */
/*ed..repaced */isplaceNum++ & (ZM_MAX */
/*           src[3             dst0t.                               seq    u16_t i, j;
    zmw_get_wlinde                 apBridgeTun  {
        u == 0;ice User prior        u8_t secgth(zdev_t* dev, zbuf_t* bufeclare_for_critical_sect			cask;

      BUFFER_TRACE0, 0, 0, 0,ount; j++)
PREFI)
    *pIfo* addh(zdev_t* dev, zbuf_t* buf, u8_t vap, u16_t* pIvR  *pIv (*/
/*  / Don't diIS P 2^4 becauONSEe d     wa   Oheing 
            kaced be tre{
   as                */
/*   _t add          are_for_critical_section();

+2dr, u16_    ifdef ZM_s one yMode;
    sAdditionIm             ER IN AN
 * ACTION OF CONTRACT,Info)
{1            OUT OF
 * OR IN CONNECTION WITH         dr, u16_              *                     ;
    u1     w_get_wQ           
/*                           8MAX_Dpl88    /* Free the       x
    c: devic;able.byte4:
     mo            up &ion(7fragTable.defra        Addr,+upce pointFILT6_t Len= ROW-1zmw_get_wlaTBD : ent[j],  */
/*    reby graE SOFTW== ownmunica SOFTWE IS PRO  /* CommacSS O, j;,=r[j] =        omp    anteOUT ice po1                               ALL ice po2]ist(zdev_t* dev, ntry[i].tick) >                 =>src    :
     0}; //For 2 TxQ
/raent[j],rxSrcIsOwnMas, INefra*/
/*    AON               SRC_ADDR_IS_O zfGeCTA' */
ent[j                  OUT    _t* buf)eqNTIAt nu== ZM_MODE*/
/*      Stephen Chon_crit                     ction fragment buffer  w_able. u16_t seqNum)
{
            fo* add(i   *c</
/*            COL/*   oist(      edvice pointer        r       Tbl[i][ i<ZM].Couned, pro              de = zfGetEncryMode       /* A&& (nfo* ad < 8P:
 <6          zfAddFirs  }
        else if ( enyte5            e OUTPUTdh(dr[j]curiw        }
        else if ( encrgTable.defragEntry[      up       ES
 * WHATSOEVER RESULTING F         defr&0x800)==D    ryMode =efault:

                   gTable.defragEntry[t nu=pare                    ZM_AES:

            

/*ragN                    cryMPort])
			ncryModTKIP)
h  {
         */
/*                    */
ragList(zdntry[i].fragment[0ToDefrahit=>um : sequence**********************;
#endi*********m : sequall */
/*   if ( encryMode =)
           DUPLICATefragTa zfAddFirstFragToDefragList  *              ;
#endi                         ev, wd-enc        */
/*   , up
    :
			casenumb       i<ZM_MAX_DEF         Gegncry0 ZM_CENC)
        {
   */
/*      dev== 0merge     */
/*    if more          is clear                //X_AP_SUPPORT)
                    ********************* addr : address of                   {
              // dec}    =ence fragment buffer               ment nne in defr     *pIvLee.de def                 wd->defragTable.d0]                  for(k    nt; j++andom        aPLIEumrtAd  *pIvLenragT= ( */
/)     ing,****/
/*            COL-             pIvLen = 8;
    co    rame to def     [j] = Len-defr    ) < 156Length(dev, returnBufefragDon      {
                                    zfRxBALL I[fragN{
                                    nB			b0, &ivLen =&i                            zfRxB             fo);
                            frag      ted. If the list .             addr : address of frag   */
/* TxGen   d                                    *r.               lenBLE_CENC
        case ZM_CENC:
mic
/*      18;
    MicVar*  pMicKe               ,       , 1     0O      }
          bValue, q    &icvLen,le.de Add f    ZM_W[1nt[k    u16_t i, j;
    zmw_get_wlan_ragEntry[i].fr    wd-ragEnt                 e.dedefragT     ApGetT                  &/* Compa
          : htrted. If tst fragment buffe[i].fragCount == 0 )IcvL_readh(dev, buffragEntry[i].fragCount = 1;
    securitnROVIis p          no                     *Sta                       gList  */
/*      AdS= {0, -2008 _AES)
     starX_DEF                               for(           */
/*             /* Compa        if (defragDone == 0)
    {
   zfMicCragHC.    200== ZM_MODE_ppe      an******** ZM_W
             */
/*    : sequence 16ablex andN       {ULL;VISTA DAd->defragn(dev)tical_stection(dev);

    /* F      ros CommuicAt* defrag(     */
       */ameLen-frag    ret   deES)
 =cvLen    sagmeor fS         EntryseqNum = seddr[3];
zf      *            *pIcvLen = 0;
			break* pbIs      dev(dev);
P              {
         sAddition          if (nfo* adEP64:
 struct zsAdEP64:
             BitEP64:
     gment6       ddr[j] 
    zmgHead)              CENCalig      dFragToDefrtAddr = zfwBufGetSize(dev, r].fragCount = 0;
    }

  ;
       */
/*wmeConnecSCRIPTION   */
/*    cvLen = 0;
			breNum;
    u16_t seqNum;
    ZM_80211      *IPAND FIT +nBuf>> 5;
    u16_t i;
   one == 0)
    {
     en = 0;
			bre0   {
        wd->commTally.swRxFragmenreadb(dev, buf, 1) & ZM_             nfo-                          efragTablNo, 1,rt of a;
       PerminBufze(dev, r_AES)
          */_WLAN_HEADER_    *pIvLeve
      
/* .sw    m;
        ++ize(dev, rframeLe=st part (dev)== 0  /* Free the /
     casQos S        MIC    declade;

HOR                    /* First part of a fragment                                 2008 AthergmenNum++Num               e desAddit}

                              */
/*      seqNv_t* dev, u16_t flush{
          if (fragNum == 0)seq=", seqNum);
                                   */
/*  ry[i] *pI 1 *d->defrag     {
    (sg1_rx(>>1)TION        */
/*            ->defragTi*ALL Irag freak;bu[i]ss offing the buffENTINULL;
      +OUT Oo* addImseqNum = >> 8msg1_r              agEntry[i]lan_dev(dev);

   ddFragToDefragList(dev, buf,                 NULL;
    RIPT{
            /*    {
              zm_msg1_rx(ZMable  }
      }
7.1      */
/* -2008 Ath)        ];
    u1 = FALSE      t part ofHE A                  *p2	break;
nfo*  (i=0; i<6GetMic(ULL)
 *)lenEvLen, &icvLen,MiagTabameLen-IZBit=_MICry[i].addr[j] = addr[j];
    }

    zmw_leave_critical_section(dev);

    return;
}


/************************************************************************/
/*                                       e(devIpTosAnd 1 *         ragList(zdeGet IP TOSAES)
6_t          rom able.defra                      ragList(zdev_t* ble.mid    oDefr   */
/*    OUTPUTS           apBridgeTunnel[6]                      */
/*      addInfo : addition info of fragment buffer                *****************************************************/
voidmsg2_rx(ZM_Ld38            dev : dev                                   rn;
}


/**up : (dipwdsPCENC      Free******************RP");
  {
                    6_t Oyte rARP"               /* ip                                   */
/*      buf : first fragment buffer                                     */
/*      addr : address of fragment buffer                               */
/* Nonum : sequence fragment buffer                                     */
/*      fragNum : fragment number of fragment buffer                    */
/*      moreFrag : more frag bit of fragment buffer                     */
/*      add                    arpOp == 0x0100) && (dip[0]6.6rx_buf_readh(dev, buf, 40);
        zm_msg2_rx(ZM_LV_2, "arpOp=", arpOp);
        zm_mdr[j] = addr[j];
    }

    zmw_leave_critical_section(dev);

    return; an                             /* WDS port checki*****************pIvLen =
/*          ip    * Middle or l
	  */
/*                   toseqNum =        onst u       eluf_t* buf), 1,  if (defragDone == 0)
    {
  nfo-MP *>= 34) //        IPv4 packet size4 *y(se ZM hheckin+20(     ro    z               UTHOR DwrcvLeC.    20ADER_A2_OFFSET+i);
        }

        if (m = zm=", m))6})
    {
        zfwBufF
    ;
    u16_t seqNum;
    sg1_rx(ZM          zmw_rx_         t fragNum;agNum      concryz*/
void zfAgingDefragLisdip                   *pI/* PING req 0xa8c0);
    u16_t seqNum;
            */
/*    INPUTFr      */
/*      Stephenuesy[i]p     z    on notice appear in all copies.tos  OUT192.168.1.15            t is buffvIcvLx1)dd frdip2      /*               r last    HEADi=0; i<Z3/*                  gNum;
    u16_t ANTIES OF
 * ME        if (fragNum == 06                                       /* PINGVLAN tagAES)
IPv6     *pIcvLen =     zmw_rx_buf_agTablcase       }
         if

    if 
    _rx_buf_writeSn          ip address */
           mw_rx_buf_writeh(dlenr    1;
 qNum, j, k;
 pIvLen = 6+(i*2)1)
    {
        zfwABILITLEN      ddress of0OUT OF
 *

          exchange      p     dst    _msg1_rx(tructnErrqNumALL IMPLI++)
            {
                temp = zmw_rx_        *  zm_mspa    ;
    fragNuu16           temp = zmw_rx_ exch, tcase zmw_gHE AUTHOR D               *p8nst u80gEntry[i].fragCo0xa8c0);
            zmw_rx_1    if (defragd;
	{
   icmpc0);
                 ,
   echoMP = zmw_r         ethTyp/
   rite8

    re<            0_rse ZM_WEP1,  *pII0x0200);

    dst * zzm_msg1_rx(ZM_LV_2, "Frag_t* bu?iddle or lasteh(dev,emeqNum)
                if    {
  ip[0]nfragTabFC1042 DISCLb                RP")    {
                        *p30);12       zm_msg0_rx(ZM2) zmw_r      for (i=     *pIpro13ntry[i].frddFragToDefr2("t */
_WEP*pbIs     uf_reai",st */
  6)  u16_t     buf, 36);
> 1                     zERMANNETOR                  //co  zm 12          f_writeh(d0xaaaa, "Dst    }_rx_OUT O_writ3, "Dst Port & 0x   *pIip1="0x8137      cPort == 0x1, d80f3    }
        else if ( encrBridge Tunne (ight notiAX              0xF8                                         */          F temp);
            }
ge   te_msg10             LE_CENC0;         zmw1)
    {
        zfwBuf    *pIvLe88  }
         0xa000);
   (frht (T)/*  7wd->ap.staTPort                     */

    if (l(seqNu/Find  3sg1_rx(ZM_LV_2, "Frw_rx_buf_re/
  a8tion         HE AUTHOR            zfwB_rx(ZMo ;
      

x_bufzfIsVtxqE                _t ethType;
8,s      = TRUefragTa0);

 mentfragEntry[i].fragCount  (( wd->ap.encrt* addr, u16_t seqNum)
{
C_buf_2readh(de  }
        else if ( t is nfo* a       vmm     Bit                                 */
/* ev_t 24(    ) ogotTable.d_d         ap(bIsDefnumreakc < 48) + ((num & 0xff) <<         tx
    m = 2/* exchtxf_rea[iNum     (fr              lea8c0);
            zmw_g0_rx(ZM_* exch          0, 0x00,Swap(          *pIvLase Z                      startAddr += (fr /       case Zddr[j] = addr[j];
    }

    zmw_leave_critical_section(dev);

    return;
}


/************************************************************************/
/*                                       Put
       *pIvLen =  checksum       Put     zmw_&    virtual TxQ                   p[1] = zmw_rx_buf_readh(dev, buf, 40);
        zm_msg2_rx(ZM_LV_2, "arpOp=", arpOp);
        zm_msg2_rx(ZM_LV_2, "ip0=", dip[0]);
        zm_msg2_rx(ZM_LV_2, "ip1=", dip[1]);

        //ARP request to 192.168.1.15
        if ((arpOp == 0x0100) && (dip[0] == 0buf, 38, tARP");
 */
  0f01hange src ip           zmw_rx_buf_writeh(d     buf : first fragment buffer                                     */
/*      addr : address of fragment buffer                               */
/*      */
/* opare s seqNum : sequence fragment buffer            ted, prosg1_rx(ZM_LV_, 6+(i*2))i<5   temp = zm                      wd- zmw_ {
                        *p22* exchange src ip and dst0xa8c0);
            zmw_rx_3     zm                 zmw_       
              hard     anted, prodev, buf, 28, 0x0f01);

            /* ch    0xaiteh(dev, buf, 28, 0x0f01);

            /* ch24uf_writeh(dev, buf, 28, 0x0f01);

            /* ch2  */
/*                                              
     eplacement.                 }
     }

  AGG_TALLY, 10,         ", seqN*agg_ta        if
    }

    returAGGRE       {
  #ifn  }

  BYPASSc ip _SCHEDULOUT dev, buseqNum = s*/
/*tid  {
          t of(_rea)(        temp = zmw_rx_     }
            /*            zfAddFirstFragTiteh(dsg1_rx(ZM_LV_
    }

    zmw&                            ClassifyTx     ifmmunications, INC ip and c /* exc  zm_msg2_rx(ZMbuf_w                 dev, buf, 34, srcPort+1)  {
 ZM_NO_Ac[up&0x7= zm,ap <x15036        {
   * {0, by honda6,   tPomain A-MPDU    re      *fuum)
{
      / {
               {
      mw_get_wla& encsg1_rx(ZM(seqNum & 0->gotv, buf,s_suind andev, buf
            }
            zm      zmw_rx_buf_writeh(dev, buf, 6, 0     iuf_reh(devdefragTaM_WEP256) bA(ZM_LV_2, ==      sg1_rx(ZM_Lif(      case ZM_WEP128:
        ||          tem     case ZM_WEP128:
                   &&         Ev, buHTize(dev, r 0x0f01);

            /* change icmcas )               a. (in           e_     && cseqNum_to_11n_       a                     ag = FALSE;ve
 f     zmAggSrc           }
     (lenErr == 1)
      */
Tx, 26_buf_8ctid            temnfo-     */
/* ==icSizen notice appear in all copies.
r                             wd= zmw_f //ZM_ENABLE_                  */
/*      bu              EP:
80x andEXCEED_PRIORITY_THRESHOLD            e           */
/* uf_readh(  zmw_rx_buf_wrxQosDrop     [ac                                                                               _tntry[i].frag    zm0ntry[i]SOFTVTXQplaceNu    r(ZM_LV_2, "Dst Po[0]);
        zm_msg2_rx(ZM_LV_2, "ip1=", dip[1]);

        //ARP request to 192.168TX       *UNAVAILLen=              */
/*      dev : device p
v, bdstf, i*2   zmw_   /* hFreef, 34, srcPort+1);
ontinu    llow  zmeqNum)
ion, p  /* to: WL_t* bu                                */
/*      buf : f  zmw_rx_buf_writeh(dev,
           writeh(dev, buAthero      /* == wfortwriteh(dev, buf,pointer      g0_rx  zm  tezmw_declare_fo    }
    /3_MAX_Dpl002;
        zmw_rx_         le     DISCL   switTIESuv, bu(dev,ol
			case ZM_T/* whole         burst(assum    x_buf */
/*      Stephe*/
            zmw_rx_buf_wds.encryMo      temp = z       zmw_rac] -      zmrc ip ac])      WLA    diMASKP");
            s>     = 0;
			b-2        eadh(devc0);
      enc       IpvLennd802=                     /*     temp = zm************8;
       rTbl_msg0Tbl             , 6+(i*2))x_buf_writeh(dev, buf, 6, 0xa000);

    /* Fin   int i;

    for(i=0;  OUTCount)
                             dstvtAN port,drop     2));
 ging the buffENTIdefrag168.1.15
        if ((arpOp == 0x0100) && (RP request to 192.168.1.15
        if ((arpOp == 0x0100) && uf_readh(dev, buf,HpSe :ent[j], 0, 0)t, 0=>standartPort = zmw_r)_t* [] can         if ((epOp == 0x0100) && (dip[mw_get    u8_           def
   kieqNum)
                     */
/*            }
           aceNu + ((              zm_msg0_rx(ZM);
  i    

    for(i=0; i<12;i++ble.defragEn(dev);

.1  ->de1eplace existing onehlUPPOR24ry[i].frzfGevtxq        +      Z0, buf, 0,
            ZM_EXTERNAL_ALLOC      )ecuriragEntry[i

    return retf, 30+(i*2    }

    return 0;

zlError:

            */
			bzdev_t* re/zmw_get_L_ALLOC_BUF, 0, 0)
     ave_critical_section(dev);

    return;
}


/****ESCRfor (nter_critical_section(dev);

    /* Find0, 0x00,iTxSedrTbl buf,ntCount++;
    , 0x0   */
/* zdev_t* h(devurn 0;
    }
    ether  u8[Eth             =0;ie de* ICMPErr ==                  Etrot =      Stephen Chen     RIPTPROVo tr }fer zmw_ge);
                            fragHead                        if ((arpOp == 0x0100) && (header[i     zmw                  *p       
        {
            fragNum : fragment number of riteh(dev, buf,src ip            N port, 0=>standard, 0x1-0x7=>VAP, 0x     fHpSenev, buf, 0)w_leave_critical_section(dev);

    return;
}


n < Z}

u8Ful        [i].addr[j] = addr[j];
    }

    zmw_leave_critical_section(dev);

    return;
}


/************************************************************************/
/*                                       G(dev, buf, 34, sr0xa8c0);
          AND xa8c0) && 
			bwriteh(dev, buf,icvLen,Pa****     sum(ZM_LV_2     */
/*      Add middle or last fragment to defragment list.                 */
/*                                                                      */
/*    INPUTS                                                            */
/*      buf : first fragment buffer                                     */
/*      addr : address of fragment buffer                               */
/* h(dev, buf, 50, temp);
#endif
            }

        }
          writeh(dev, buf, dev);
    zmw_d  {
       ZM_LV_2, "UDP");
            srcPort = zmw_  */
/iteh(dev, buf, 28, 0x0f01);

            /* chbuf_writeh(dev, buf, 28, 0x0f01);

            /* change_writeh(dsg1_rx(ZM_LV_2, "Fr            *pIUD0x0200);

       src   }    {
            zm_msg0_rx(ZMA au;

    if ((errframe for power saving STA */
     6h(dev, buf, 28, 0000);
      (ZM_MAX            laceNum++ &for (i=
/*                               temp = zmw_rx_(i*2), temp);
            }
            zmw_rx_ZM_LV_2,iteh(de        */
/*                            um);
            zsmit[8] = {0,   u8_tbuf, gingDefra,       zmw_buf_readb(Buf, fPowerSa src if ((ret =   zfPowerSarx_buf_writeh(S power- dip[1]);

        //ARP request to 192.168.1.15
        if ((arpOp == 0x0100) && (   */
/*    ging defragid zfAgingDefragList(zdev_t* dev, u16_t flush;
            zm          e     f_readh(d_buf_writeh(de src p{
      erro  }
 _rx_buf_writeh(dev, buf, 8, 0x0000);
          4                 zmw_rx_briteh(dev, buf, 6, 0xa000);
      xa8c0) && (Q[ac] */
        z
        /* Put to VTXQ[ac]mr-saving mode, buf, 8, 0x0000);
          8rn error    ifMr-saving mo                        *{
        ret = zfTx                        *p5gEntry[i].fragCount t = zfTxSendEt  /* Push VTXQ[ac] */
        ~ zmwsg2_tx(ZM_LV_1, "Tx Com+= 2

    zfwBufFree(dev, buf, err);
    return err;
}


p err=", err);

    zfwBufFree(d0xa8c0);
            zmw_rx_50rn error t zsAditeh(dev, buf, 10, 0x000                    dst */
   1 4;
        }
        el if ((err w_rx_buf_   u16_t hlen;
    u16_t header[(24+25+1)/      zmw_rx_buf_writeh(dev, buf, 6, 0xa000);
      /* exchange src ip and dst0xa8c0);
            zmw_rx_         or if port is disabled */
    if ((err = zfTxPortControl(dev, buf, port)) == ZM_PORT_DISABLED)
    {
        err = ZM_ERR_TX_PORT_DISABLED;
        goto zlError;
    }

#if 1
    if ((wd->wlanMode == ZM_MODE_AP) && (p12t < 0x20))
    {
        /* AP : Buffer frame for power saving STA */
        if ((ret = zfApBufferPsFrame(dev, buf, port)) == 1)
        {
            return     *pIPTIOmm  if (er fram        {
           =>VAP, 0x20-0x25=>WDS     */
/*p);
            }
            zmw_rx_b=>VAP, 0x20-0x25=>WDriteh(dev, b                }
    }
 Buf, starMM   INPUTS   a         and dst ip */
        (i*2));
    */ }
    }
    /* IC                                                    ZyDAS Technology Corporation    2005.5      */
/*                 (dip[0] == 0LV_2        {
            return ZM_SUCCESS;
        }
    }
#eBuf, star                      */
/*      port : WLAN por0_EXTERNAr (i=0rn 0;

zlError:

  AES)ror:       for(k=1; k<w_declare_forFditibuf);
u16_t zfWlanRxFilter(zdece and fragmeny Corporation    2005.5      */
/*                             */
/r c */
y */
const u8_t zcUpToAc[8] = {0, 1, 1, 0, 2, 2, 3, 3}; //WMM default
//const u8_t zcUpTo2];
    u16_t micLEXTERNAL_ALLOC1   **********     ragToDefragLiType == 0x0008)   ifzf         -2008 Ather=>WDS ZM_t(zdev_t* dev, zbuFrag frag;
    if ((((wd->             sFrag*                                */
/*      addInfo : addition info of fragment buffer      or_critical_section();

    zmw_enter_critical_section(dev);

    /* FinddEth           ameL        *pIcvLen = 0;
			br, 0x0HpSe              err     anMode == ZM_MODE_AP )
    {
      g li/*      fluThFlag     */
/*     *
    zmwffers t         *pIset = 0;

   )u16_t     _decl if ((/* R;
     headeifror:

is****             t is ebuf, u16_   }ControlLen;
    u16_t ))ncryMod& 0x_DIS    D

    return retTosAndENABRRlag OSP bit
    i if ((ret f, 30zl
    ;
    u16indSt zfTxGetIpve
 * copyright notice aAP      pev, zbuf_-0x25=>WDS     */
/* dMode10-0x17=>VAPMode2   d25=>WDARP                      */
/*      addInfo : addition info of fragment buffer          ce and fragragEntry[i                             t to be          if (s;
    u16_t header[(8+30+2+18)/2];    /* ctr+(4+a1+a2+a3+2+a4)+qos+iv */
    u16_t headerLen;
    u16a000);
       /* exchange src ip and dst      and dst ip */
           E_IBSS_Pmp = z                           zsFrag frag;
    u16_t i, j, id;
    u16_t offset;
    u16_t da[3];
    u16_t sa[3];}


= ZM_CENC)
ncryMode == ZM_AES)
dev, buf, 20);
             }       E_WIFI
    //e above
 bQoSE
			b
      t is      z              u  OUT WLA    fTxPortContr    ndFrPutuf_w 8));
}


M_MODE_AP) ev, bsh12);
        sa[2] = _bufshreadh(de];
    u1= 0)
        {
        zmw_tx_b6_t frameLen;
    u16_t fras                 0);
#e i++)
    {
  S    VM_MODE_AP) (weigh|
  roun    binfragOff;
 tructnap[8/2buf, 22);
    bufLen      }     /* Adize(dev    /
       d  *pITxD queu{
   um;
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

    zm_msg1_tx(ZM_LV 0;
    //st   if ((wd->wlanMode == ZM_MODE_AP) && (/
/*       sg0_rx(ZM_LV_2, "UDP")         /* change src */
            zmw_rx_buf_writeh(dev, buf, 6, 0xa000);
            zmw_rx_buf_writeh(dev, buf, 8, 0x0000);
            zmw_rx_buf_writeh(dev, buf, 10, 0x0000);

            zm_msg0_rx(ZM_LV_2, "UDP");
            srcPort = zmw_rx_buf_readh(dev, buf, 34);
            dstPort = zmw_rx_buf_readh(dev, buf, 36);
            zm_msg2_rx(ZM  */
008_t)   if ((re/ *
 *ZM_LV_2, "US" AND THE At */
/*    AUT       )xUTS yIdx;32     eeTxSize(devology r        returnkip lis != wd->vtemoveLen;
    u16_t header[(8+30+2+18)/2];    /* ctr+(4+a1+a2+a3       if ((ret = zf1("zfH
        xll(zde     *pIvLev, addr  temp = zmw             encha      break;
HAL_STATModeIction(de0xa8c0);
    !FUNCTodeMDK fragO   INPUTS                dFragToDefr0("HAL_bufdh(dWARRCCESS;.fragisabled *          hFlag)
{   legmentation SA */
    
DFSDis, bu.fragCount    return      = 5 if (da       /* Findl_sec DFSLAN_HEADER_A2_OFFS keydeclare_for;
#endif //ZM_ENABLE_CENCflagFreqC {
 */
    
       i;
    u16_H    until Rd->d}     y c Tabedzmw_tx_buf_readh a fragmentation= (u8_    fragEntrKeye TabSNAPcritic           src ip            vingMgrWakeup(de//ZM_ENABLE_CE
    }

    returPOW    AVE1_tx(ZM_LV_0, e ==   eSavingMgrIsSleepinCENC
)     for(k=1; k<w if ((ret = zf0(   zmw_s     bd sie[va              p1025-sMxze
   
 \n#endif           *pIcvENC
                xSendEth                ENABLE_CEN***********8_t)

/*wd->ap.sr     SUPPOR>ap.staTabum] =u1     */
/***********(ethTyp-------------- */
/*  Ethernet          */
/************************nfo->ap.staTabt to be rfragOff;
    u16_t newFl******hileNC.    200writeh(dev,ak{
    le.r        his per.d->w    else
  :     2];
    u16_t micL---+set;      ve
 in tdif "Src****    INPUTS               _0, "v);
  , 2zdev_t*"Src  26+IcvLew_tx_buf_readh(port < 0x20))
---20 : fr   zmwh  return 0e
    rGetIpTosble -v);
    ,ectio,, 36,     NU0     (srcrr) | AHpSzsFragncKeyId    + 2 + ( Network Lay/ARP rlan_dev(dev);

    *pIvLen = IN
    u16_t s      Lay       1, *********************************n, &icvLen, addInf                            cleared */
  k++)
                   OR ANY DAMAGES
 * WHATSOEVER RESULTING;
#en                     */
/*    AUTH            tbSchefo);Scareadh(a--2--+----ff);nel----) UDP(12)riticzfRxBus(seqNum =_t oP      port < 0x20))
    {
 f      0x0Sn-----Stop----------+------ Ethernet :ZM_LV_2, "UDP"                            ISABLED)
    {
  if ((err = zf    Evl tool loop ba    e listPayloa= ZM_ERR_TX_PORT_DISABLED;
        goto zlErr    }

#if 1
    if ((wd->wlanMode == ZM_    u16 = zmw_tx_ fragOff;
    u16_t newFlag;
    struct zsMicVar*  pMicKey;
                    Tx+-------            buf, 38, tARP");-- *--agNum                      ON DE6_t oP, 1, 0,L   addr :                   */keyIdx = 4;
          m---------4---->defra   {
                      */
/*                       M_WEP64:
eLen;
 TxQ[3]----+-------CENC
		   de-6--+--2--+-- | TCP(20) UDP(12) ICM        FUNSDU  |  DA |  SA | Type------IPTION             /* Firs---+- */
/*P(     ApplipOp;
    3uration| DA|SA |BSS
    for(     (devork ( 0);
ation|   on|    -----------------------( wd->sta.enc */

    if ( wd->sta.enon|    0;
    / =      */
/******      E)
                     fragHead += ivLen; /*Buf, startAfluTX_rt o: 1=>defra                       */---- - DA -*
 *:werSavSUPP-=----------                 -= removeLen;                 // MSDU Lengths - DA - SA
|****a2sulame |  addr : ad  addr3|ragmentaryMode ==------------- */
--------------------------   micLen = (      |Max |  SA | Type*1/ally */
 ncryMode == ZMon|     |  FCpGetSta      if (qosType2-------------- */
/*  Ethernet = 0;
        }
    }
    else if (wd->wlanagmeeO     
      u16_t arSUPPORFUNCTmovehreshold + t        = ZM_M    //  1 */
  ame |dt    if (w    um =t[k]gth      uf);    /   zfGetRxction(d_rx_buf_mad L | ac0     if HigndifhanAc2 AUTHOR DISCLb*2));
  -----------------------ype);
        if (qosType 2----+  else if (wd->wlanMo
            ault:

           }
    else if (wd->wlanMode yright notice and this permis                        in t&& (moreFap] IcvLen 0)
        {
            up = 0;
        }
    }
    else
        {
        ToPk    {
        dev, buf,adh(dewBufGe       }
    }
    el/ MSDU Lengths
    frameLen -=ragNum = 1;

  etSize(devragEntry0#1st create MIC Length ma 0xff00;
 BU lenEEP64)||
      yte5c  /* byte4 *y    zfTxGetIwd->wlanMode == ZM_MODE_INF_tx_buf_rea   *fApF
   taQos

  8_t)flda, &qLen, NQ[ac] */
  t is//zm_de                             u****          mw_rx_buf_rea             * copyright notice and this permis    up, &mic Pass the total payload to generate/
            zmw_rx_bh Check the HeaderLen and determ_tx_buf_readh(dev, buf);    // MSDU Lengths
    frameLen -= removeLen;                 /f[0], header, frag.seq[0],
      1#1st create MI removeLen, port, da, sa,
                                  up, &mic3en, snap, sna---+--------+---+-----------------------1---------------------Type);
  -----------------------------------== ZM_MODE_INFRASTRUCTURE)
    {
        if (wd->sta.wmeConnected ==        {
            up = 0;
        }
    }
    el snapLen, da, sa, up,
 All    /sit=",e-----p0);
 t ;  xce    /*ir       -------+---------3 da, &qo           (24+25+1)8);
     i| frame |duration| DA|SA |BSS                  dr
/**(/ed to b6--[i].addr[j] = addr[j];
    }

    zmw_leave_critical_section(dev);

    return;
}


/************************************************************************/
/*                                       Fl         keyIdx =       ragList(zdevhs l/* ---+-et ; uf_r;
    u16_t frameLen;
    u16_t fragNwd->LUDIid].keyIdx                */
/*1lse
    {
  LL T fragLen+32)) != NULL)
        pplicati {
        /* DA */
      /cKeyId = z= 0 */
 /*SizencKeyIddle[id].keyIdx                */
/*hernet frev, fragLen+32)) != NULL)
        	break;
 = wd->
                    offset    if ((dx = wd->ap.sble[id].keyIdx                      et ;   dev, fragLen+32)) != NULL)
        a8c0) &&                 frag.bufType[i] = 1zdev           //Dec****    I    *heckTOM, No meanFreein OTUS--         sa[1] = z                              up, &mic
#endif    |  DA ap.bcHalK0x4; [HpSe           i = zfApFFefrataULL);

 bug_msg1("#1 heidt :",xffmentAer tddr, u1AP : Buffe     t is     .1taTable[id].encryMode)
            {
            case ZM_AES:
            case ZM_TKIP:
#ifdef ZM_ENABLE_CENC
            case ZM_CENC:
#endif //ZM_ENABLE_CENC
       if ((ret = 0x4; /* More da              0x4; ;  {
   , 8, 0x0000);
                    fragLen;
  +--------- *--Idx);
    }
 ragLen - mpduLengthOffset;

                                   break;
#endif d           : [----]= wd->defragTable.def      d  |cKey1)) == 1)
PUTS    MSDU Lengths - DA - SA

  C Lengthly */
    micLeame Num opyr8_t)fl
         thresuration| DA|SA |Bp);
                                              *wd->defragTab    NULL);
              ]-
         u16_t en+RUCTUREf, 14);
   |= (1<<      u16_t ahe HeaderC
      -----     set = 8;

  rag.seq[i_rx_buf_writeh(devreadbbuf_readh(delag)
{
    u16_t err;
 x_buf_writeh(devd  |  */
/* UCTU24+25+1)/2];
  readbopyr
                       buf[i]********,         tai        f, ZM_W soft Add          ] =                    buf[i],  Intermedior (i=0; i<3; 0x4;               
        /* DA (dev -= removeLen;                 // MSDU Le_sec buf);

      0xf
    //Decide Key I(1h[6]&ead);
    poration    2005.5
        {
  U8021h[6] = voA */
  and dst ip */
          O/
/*  wd->*/
_sec+/
     of a fragmentationse //Len p);
#eff    pseud(dev                  //fBr, 0,         for(k=1; k<wd->de       , buf, ZM_W-=set, fraand dst ip */
        /* Last fragment  */
                    Mul * P2, "Frag seq=", seqNum);
                        zfTxBufferCopy(dev, frag.buf[i], encle      */
xTraff device pointer  = zfwBufGeet =+ToRxBr, snapL/
       adh(dev/
                 buf[i], fragLe                    Bufferbuf[i], buf, 0, offset, frmeLen, m      ing */
            i					  */
/*  MPDU :        break;
   {
        if (w       of A */
                              TosA ((err = zfTxPor** */
/* Add 2    */
/*    Num);
            z                 frag.flag[i] = (u8_t)flag; eaderLen = z/*      addInfo : addition info of fragment buffer           napLen);
         ( wIsRIFSfragment[j], , 0x0000);
            zmw_rx_buf);
  u16_t removeLen;
    u16_t hea  /* SA *2 Recor    , devi
        {
 OUTPUtse, ne whoad L            temp = zs se*/
   -----
   ----dh(dragBit = (zmw_bu//.repwical_s>eturn 01--+--_dev(dev);
   *      deEP128:
			casNagme*      de*/
/*qoame uf, , 4, nted
------***********en;
);
    }
    else if ( wd22       )
    {
   of fra         */
/**** ZM_WLAN_HEADERLE_CEN14);
    }
    else if ( wden)
fragOff;
  = {i*pIc    (WirelOFTWDin all  && (SysTable for con//DbgPrint("TheE_AP -------  = ZM_en+, snap    -------u16_t frd: %d"    {/
          );

    /* Fin     }
                 ,        ------ZM_MODE_AP) && (pf802, sa, u--------rifn, mic   */
/*      (V)fTxPortControl(debufBIT_5    INPUTS  // ACK polic/zfwB"N cras            z     IFS-Lik       if       {
     end(dev, fran, NSetSize(dev, _BUF, 0, 0)) != ZM_SUCCnt]e ZM_0,16_t afset =--1---+-------"
   Off="set,M_TKIP:
             *pDETEC.                                     }
 rag.seq[, 10, 0x0000);
 ZM_NO_     FIP     e----           le----ln, fPatte_readh     &       );    moreFrag : more frag bit of fragment buffer    11Mgm             ol            rag li2*    l 1, 1, 0,to g     TabMP*******                                 Frag(dev, buf,     u16_t fragLen;     P");
            s   bu      default:

          emoveL         mat-2--2, "ip1=", dip[1]);

        //A#3_wri              zmw     -= removeLen;           ufType[i],
     
    u16, 0x0mHp     *Rfrag.bufpplication currenM_TKor (i=<     ?1:0)e    (dev, fragOfftual AP, 2HT204-1=0; i<fragN    ent buffer     Setbuf,intim         return ZM_SUCCESS;
 Frag(dev,                                       //u16_t addrTblSize;
 nd an emptgy[i]            */
/       da[2] = e D       */   u16_t err;
    //u16_t addrTblSize;
         /* SA */er     t temp;
    WlanTe      buf

    u16_t ar.   /      w_rx_b   /  u16_t arprt of1  frag.he//
    uf, 0, &ivLen, &icvLable )
    if8+30+2+18)/2];    /* ctr+(4+a1+a;
       /
/*    OUTPUTS    heckin      gEntrTIMen;
IME->defragTable.   //frameLen  addrTblSize;
    //struct z           *pIcvLen = 4;
			_buf_writeh(N1 = %d----2.encryMode    *\nmtual AP, 2}

    retuLV_2, "Dst Por(dev, frag.bu//t = 0;

    zmw_get_wlan_dev(dev);

    zmw */
/*                        */   zmw_declare_for== ZM_MODE_AP )
    {
      e above
 * copyright notice and this permis


const u8_t zgSn// U
/* 0 */
/*         zfwCopyBufC /*  buf)

   }
 ************f fragment buffer                     */
/*                                  );
            zpyright notice and this permissi    u16_t header[(24de == ZM_MODE_INFRASTRUCTURE )
/ISCONNECT_tx_buf_readh(dev, buf,  of fragment buffer                     */
/*   rx_buf 0=>standard, 0x1-0x7=>VAP, 0x20O                 adjac         d to     ex0200);

       );
       POCnt ( wg.buf[i-1]);
        zmw_rx_],en );
 ------               */ /* Compa  t(dev, buf,         --- Va", dip[0]oad L |       fragNu *****************);
        /* SA */        D                 zflag      3, 3}; //WMM default
//ce  addr : addresse(zdev_t* dev, zbuf_t* buf);
u16_t zfWlanRxFil
        //A  u16_t snap[8/Hect     lError:

port < 0"zfTxSend       if (d(deped dury[i].iscon FIFOype[i],0000);
            zmw_rx_buf_ZM_NO_ApBuff-----------+------LED;
}



/*******************                                                      zfIdl    me vss stazfw       re().  *2----+--------//EOSP bit
      u16_t removeLen;
    u16_t hceived 802.11 frame buffer.             f_readh   Stephen      