/*
 *   ALSA driver for RME Hammerfall DSP MADI audio interface(s)
 *
 *      Copyright (c) 2003 Winfried Ritsch (IEM)
 *      code based on hdsp.c   Paul Davis
 *                             Marcus Andersson
 *                             Thomas Charbonnel
 *      Modified 2006-06-01 for AES32 support by Remy Bruno
 *                                               <remy.bruno@trinnov.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/math64.h>
#include <asm/io.h>

#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/info.h>
#include <sound/asoundef.h>
#include <sound/rawmidi.h>
#include <sound/hwdep.h>
#include <sound/initval.h>

#include <sound/hdspm.h>

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	  /* Index 0-MAX */
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	  /* ID for this card */
static int enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;/* Enable this card */

/* Disable precise pointer at start */
static int precise_ptr[SNDRV_CARDS];

/* Send all playback to line outs */
static int line_outs_monitor[SNDRV_CARDS];

/* Enable Analog Outs on Channel 63/64 by default */
static int enable_monitor[SNDRV_CARDS];

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for RME HDSPM interface.");

module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for RME HDSPM interface.");

module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable/disable specific HDSPM soundcards.");

module_param_array(precise_ptr, bool, NULL, 0444);
MODULE_PARM_DESC(precise_ptr, "Enable or disable precise pointer.");

module_param_array(line_outs_monitor, bool, NULL, 0444);
MODULE_PARM_DESC(line_outs_monitor,
		 "Send playback streams to analog outs by default.");

module_param_array(enable_monitor, bool, NULL, 0444);
MODULE_PARM_DESC(enable_monitor,
		 "Enable Analog Out on Channel 63/64 by default.");

MODULE_AUTHOR
      ("Winfried Ritsch <ritsch_AT_iem.at>, "
       "Paul Davis <paul@linuxaudiosystems.com>, "
       "Marcus Andersson, Thomas Charbonnel <thomas@undata.org>, "
       "Remy Bruno <remy.bruno@trinnov.com>");
MODULE_DESCRIPTION("RME HDSPM");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{RME HDSPM-MADI}}");

/* --- Write registers. --- 
  These are defined as byte-offsets from the iobase value.  */

#define HDSPM_controlRegister	     64
#define HDSPM_interruptConfirmation  96
#define HDSPM_control2Reg	     256  /* not in specs ???????? */
#define HDSPM_freqReg                256  /* for AES32 */
#define HDSPM_midiDataOut0  	     352  /* just believe in old code */
#define HDSPM_midiDataOut1  	     356
#define HDSPM_eeprom_wr		     384  /* for AES32 */

/* DMA enable for 64 channels, only Bit 0 is relevant */
#define HDSPM_outputEnableBase       512  /* 512-767  input  DMA */ 
#define HDSPM_inputEnableBase        768  /* 768-1023 output DMA */

/* 16 page addresses for each of the 64 channels DMA buffer in and out 
   (each 64k=16*4k) Buffer must be 4k aligned (which is default i386 ????) */
#define HDSPM_pageAddressBufferOut       8192
#define HDSPM_pageAddressBufferIn        (HDSPM_pageAddressBufferOut+64*16*4)

#define HDSPM_MADI_mixerBase    32768	/* 32768-65535 for 2x64x64 Fader */

#define HDSPM_MATRIX_MIXER_SIZE  8192	/* = 2*64*64 * 4 Byte => 32kB */

/* --- Read registers. ---
   These are defined as byte-offsets from the iobase value */
#define HDSPM_statusRegister    0
/*#define HDSPM_statusRegister2  96 */
/* after RME Windows driver sources, status2 is 4-byte word # 48 = word at
 * offset 192, for AES32 *and* MADI
 * => need to check that offset 192 is working on MADI */
#define HDSPM_statusRegister2  192
#define HDSPM_timecodeRegister 128

#define HDSPM_midiDataIn0     360
#define HDSPM_midiDataIn1     364

/* status is data bytes in MIDI-FIFO (0-128) */
#define HDSPM_midiStatusOut0  384	
#define HDSPM_midiStatusOut1  388	
#define HDSPM_midiStatusIn0   392	
#define HDSPM_midiStatusIn1   396	


/* the meters are regular i/o-mapped registers, but offset
   considerably from the rest. the peak registers are reset
   when read; the least-significant 4 bits are full-scale counters; 
   the actual peak value is in the most-significant 24 bits.
*/
#define HDSPM_MADI_peakrmsbase 	4096	/* 4096-8191 2x64x32Bit Meters */

/* --- Control Register bits --------- */
#define HDSPM_Start                (1<<0) /* start engine */

#define HDSPM_Latency0             (1<<1) /* buffer size = 2^n */
#define HDSPM_Latency1             (1<<2) /* where n is defined */
#define HDSPM_Latency2             (1<<3) /* by Latency{2,1,0} */

#define HDSPM_ClockModeMaster      (1<<4) /* 1=Master, 0=Slave/Autosync */

#define HDSPM_AudioInterruptEnable (1<<5) /* what do you think ? */

#define HDSPM_Frequency0  (1<<6)  /* 0=44.1kHz/88.2kHz 1=48kHz/96kHz */
#define HDSPM_Frequency1  (1<<7)  /* 0=32kHz/64kHz */
#define HDSPM_DoubleSpeed (1<<8)  /* 0=normal speed, 1=double speed */
#define HDSPM_QuadSpeed   (1<<31) /* quad speed bit */

#define HDSPM_Professional (1<<9) /* Professional */ /* AES32 ONLY */
#define HDSPM_TX_64ch     (1<<10) /* Output 64channel MODE=1,
				     56channelMODE=0 */ /* MADI ONLY*/
#define HDSPM_Emphasis    (1<<10) /* Emphasis */ /* AES32 ONLY */

#define HDSPM_AutoInp     (1<<11) /* Auto Input (takeover) == Safe Mode, 
                                     0=off, 1=on  */ /* MADI ONLY */
#define HDSPM_Dolby       (1<<11) /* Dolby = "NonAudio" ?? */ /* AES32 ONLY */

#define HDSPM_InputSelect0 (1<<14) /* Input select 0= optical, 1=coax
				    * -- MADI ONLY
				    */
#define HDSPM_InputSelect1 (1<<15) /* should be 0 */

#define HDSPM_SyncRef0     (1<<16) /* 0=WOrd, 1=MADI */
#define HDSPM_SyncRef1     (1<<17) /* for AES32: SyncRefN codes the AES # */
#define HDSPM_SyncRef2     (1<<13)
#define HDSPM_SyncRef3     (1<<25)

#define HDSPM_SMUX         (1<<18) /* Frame ??? */ /* MADI ONY */
#define HDSPM_clr_tms      (1<<19) /* clear track marker, do not use 
                                      AES additional bits in
				      lower 5 Audiodatabits ??? */
#define HDSPM_taxi_reset   (1<<20) /* ??? */ /* MADI ONLY ? */
#define HDSPM_WCK48        (1<<20) /* Frame ??? = HDSPM_SMUX */ /* AES32 ONLY */

#define HDSPM_Midi0InterruptEnable (1<<22)
#define HDSPM_Midi1InterruptEnable (1<<23)

#define HDSPM_LineOut (1<<24) /* Analog Out on channel 63/64 on=1, mute=0 */

#define HDSPM_DS_DoubleWire (1<<26) /* AES32 ONLY */
#define HDSPM_QS_DoubleWire (1<<27) /* AES32 ONLY */
#define HDSPM_QS_QuadWire   (1<<28) /* AES32 ONLY */

#define HDSPM_wclk_sel (1<<30)

/* --- bit helper defines */
#define HDSPM_LatencyMask    (HDSPM_Latency0|HDSPM_Latency1|HDSPM_Latency2)
#define HDSPM_FrequencyMask  (HDSPM_Frequency0|HDSPM_Frequency1|\
			      HDSPM_DoubleSpeed|HDSPM_QuadSpeed)
#define HDSPM_InputMask      (HDSPM_InputSelect0|HDSPM_InputSelect1)
#define HDSPM_InputOptical   0
#define HDSPM_InputCoaxial   (HDSPM_InputSelect0)
#define HDSPM_SyncRefMask    (HDSPM_SyncRef0|HDSPM_SyncRef1|\
			      HDSPM_SyncRef2|HDSPM_SyncRef3)
#define HDSPM_SyncRef_Word   0
#define HDSPM_SyncRef_MADI   (HDSPM_SyncRef0)

#define HDSPM_SYNC_FROM_WORD 0	/* Preferred sync reference */
#define HDSPM_SYNC_FROM_MADI 1	/* choices - used by "pref_sync_ref" */

#define HDSPM_Frequency32KHz    HDSPM_Frequency0
#define HDSPM_Frequency44_1KHz  HDSPM_Frequency1
#define HDSPM_Frequency48KHz   (HDSPM_Frequency1|HDSPM_Frequency0)
#define HDSPM_Frequency64KHz   (HDSPM_DoubleSpeed|HDSPM_Frequency0)
#define HDSPM_Frequency88_2KHz (HDSPM_DoubleSpeed|HDSPM_Frequency1)
#define HDSPM_Frequency96KHz   (HDSPM_DoubleSpeed|HDSPM_Frequency1|\
				HDSPM_Frequency0)
#define HDSPM_Frequency128KHz   (HDSPM_QuadSpeed|HDSPM_Frequency0)
#define HDSPM_Frequency176_4KHz   (HDSPM_QuadSpeed|HDSPM_Frequency1)
#define HDSPM_Frequency192KHz   (HDSPM_QuadSpeed|HDSPM_Frequency1|\
				 HDSPM_Frequency0)

/* --- for internal discrimination */
#define HDSPM_CLOCK_SOURCE_AUTOSYNC          0	/* Sample Clock Sources */
#define HDSPM_CLOCK_SOURCE_INTERNAL_32KHZ    1
#define HDSPM_CLOCK_SOURCE_INTERNAL_44_1KHZ  2
#define HDSPM_CLOCK_SOURCE_INTERNAL_48KHZ    3
#define HDSPM_CLOCK_SOURCE_INTERNAL_64KHZ    4
#define HDSPM_CLOCK_SOURCE_INTERNAL_88_2KHZ  5
#define HDSPM_CLOCK_SOURCE_INTERNAL_96KHZ    6
#define HDSPM_CLOCK_SOURCE_INTERNAL_128KHZ   7
#define HDSPM_CLOCK_SOURCE_INTERNAL_176_4KHZ 8
#define HDSPM_CLOCK_SOURCE_INTERNAL_192KHZ   9

/* Synccheck Status */
#define HDSPM_SYNC_CHECK_NO_LOCK 0
#define HDSPM_SYNC_CHECK_LOCK    1
#define HDSPM_SYNC_CHECK_SYNC	 2

/* AutoSync References - used by "autosync_ref" control switch */
#define HDSPM_AUTOSYNC_FROM_WORD      0
#define HDSPM_AUTOSYNC_FROM_MADI      1
#define HDSPM_AUTOSYNC_FROM_NONE      2

/* Possible sources of MADI input */
#define HDSPM_OPTICAL 0		/* optical   */
#define HDSPM_COAXIAL 1		/* BNC */

#define hdspm_encode_latency(x)       (((x)<<1) & HDSPM_LatencyMask)
#define hdspm_decode_latency(x)       (((x) & HDSPM_LatencyMask)>>1)

#define hdspm_encode_in(x) (((x)&0x3)<<14)
#define hdspm_decode_in(x) (((x)>>14)&0x3)

/* --- control2 register bits --- */
#define HDSPM_TMS             (1<<0)
#define HDSPM_TCK             (1<<1)
#define HDSPM_TDI             (1<<2)
#define HDSPM_JTAG            (1<<3)
#define HDSPM_PWDN            (1<<4)
#define HDSPM_PROGRAM	      (1<<5)
#define HDSPM_CONFIG_MODE_0   (1<<6)
#define HDSPM_CONFIG_MODE_1   (1<<7)
/*#define HDSPM_VERSION_BIT     (1<<8) not defined any more*/
#define HDSPM_BIGENDIAN_MODE  (1<<9)
#define HDSPM_RD_MULTIPLE     (1<<10)

/* --- Status Register bits --- */ /* MADI ONLY */ /* Bits defined here and
     that do not conflict with specific bits for AES32 seem to be valid also
     for the AES32
 */
#define HDSPM_audioIRQPending    (1<<0)	/* IRQ is high and pending */
#define HDSPM_RX_64ch            (1<<1)	/* Input 64chan. MODE=1, 56chn MODE=0 */
#define HDSPM_AB_int             (1<<2)	/* InputChannel Opt=0, Coax=1
					 * (like inp0)
					 */
#define HDSPM_madiLock           (1<<3)	/* MADI Locked =1, no=0 */

#define HDSPM_BufferPositionMask 0x000FFC0 /* Bit 6..15 : h/w buffer pointer */
                                           /* since 64byte accurate last 6 bits 
                                              are not used */

#define HDSPM_madiSync          (1<<18) /* MADI is in sync */
#define HDSPM_DoubleSpeedStatus (1<<19) /* (input) card in double speed */

#define HDSPM_madiFreq0         (1<<22)	/* system freq 0=error */
#define HDSPM_madiFreq1         (1<<23)	/* 1=32, 2=44.1 3=48 */
#define HDSPM_madiFreq2         (1<<24)	/* 4=64, 5=88.2 6=96 */
#define HDSPM_madiFreq3         (1<<25)	/* 7=128, 8=176.4 9=192 */

#define HDSPM_BufferID          (1<<26)	/* (Double)Buffer ID toggles with
					 * Interrupt
					 */
#define HDSPM_midi0IRQPending   (1<<30)	/* MIDI IRQ is pending  */
#define HDSPM_midi1IRQPending   (1<<31)	/* and aktiv */

/* --- status bit helpers */
#define HDSPM_madiFreqMask  (HDSPM_madiFreq0|HDSPM_madiFreq1|\
			     HDSPM_madiFreq2|HDSPM_madiFreq3)
#define HDSPM_madiFreq32    (HDSPM_madiFreq0)
#define HDSPM_madiFreq44_1  (HDSPM_madiFreq1)
#define HDSPM_madiFreq48    (HDSPM_madiFreq0|HDSPM_madiFreq1)
#define HDSPM_madiFreq64    (HDSPM_madiFreq2)
#define HDSPM_madiFreq88_2  (HDSPM_madiFreq0|HDSPM_madiFreq2)
#define HDSPM_madiFreq96    (HDSPM_madiFreq1|HDSPM_madiFreq2)
#define HDSPM_madiFreq128   (HDSPM_madiFreq0|HDSPM_madiFreq1|HDSPM_madiFreq2)
#define HDSPM_madiFreq176_4 (HDSPM_madiFreq3)
#define HDSPM_madiFreq192   (HDSPM_madiFreq3|HDSPM_madiFreq0)

/* Status2 Register bits */ /* MADI ONLY */

#define HDSPM_version0 (1<<0)	/* not realy defined but I guess */
#define HDSPM_version1 (1<<1)	/* in former cards it was ??? */
#define HDSPM_version2 (1<<2)

#define HDSPM_wcLock (1<<3)	/* Wordclock is detected and locked */
#define HDSPM_wcSync (1<<4)	/* Wordclock is in sync with systemclock */

#define HDSPM_wc_freq0 (1<<5)	/* input freq detected via autosync  */
#define HDSPM_wc_freq1 (1<<6)	/* 001=32, 010==44.1, 011=48, */
#define HDSPM_wc_freq2 (1<<7)	/* 100=64, 101=88.2, 110=96, */
/* missing Bit   for               111=128, 1000=176.4, 1001=192 */

#define HDSPM_SelSyncRef0 (1<<8)	/* Sync Source in slave mode */
#define HDSPM_SelSyncRef1 (1<<9)	/* 000=word, 001=MADI, */
#define HDSPM_SelSyncRef2 (1<<10)	/* 111=no valid signal */

#define HDSPM_wc_valid (HDSPM_wcLock|HDSPM_wcSync)

#define HDSPM_wcFreqMask  (HDSPM_wc_freq0|HDSPM_wc_freq1|HDSPM_wc_freq2)
#define HDSPM_wcFreq32    (HDSPM_wc_freq0)
#define HDSPM_wcFreq44_1  (HDSPM_wc_freq1)
#define HDSPM_wcFreq48    (HDSPM_wc_freq0|HDSPM_wc_freq1)
#define HDSPM_wcFreq64    (HDSPM_wc_freq2)
#define HDSPM_wcFreq88_2  (HDSPM_wc_freq0|HDSPM_wc_freq2)
#define HDSPM_wcFreq96    (HDSPM_wc_freq1|HDSPM_wc_freq2)


#define HDSPM_SelSyncRefMask       (HDSPM_SelSyncRef0|HDSPM_SelSyncRef1|\
				    HDSPM_SelSyncRef2)
#define HDSPM_SelSyncRef_WORD      0
#define HDSPM_SelSyncRef_MADI      (HDSPM_SelSyncRef0)
#define HDSPM_SelSyncRef_NVALID    (HDSPM_SelSyncRef0|HDSPM_SelSyncRef1|\
				    HDSPM_SelSyncRef2)

/*
   For AES32, bits for status, status2 and timecode are different
*/
/* status */
#define HDSPM_AES32_wcLock	0x0200000
#define HDSPM_AES32_wcFreq_bit  22
/* (status >> HDSPM_AES32_wcFreq_bit) & 0xF gives WC frequency (cf function 
  HDSPM_bit2freq */
#define HDSPM_AES32_syncref_bit  16
/* (status >> HDSPM_AES32_syncref_bit) & 0xF gives sync source */

#define HDSPM_AES32_AUTOSYNC_FROM_WORD 0
#define HDSPM_AES32_AUTOSYNC_FROM_AES1 1
#define HDSPM_AES32_AUTOSYNC_FROM_AES2 2
#define HDSPM_AES32_AUTOSYNC_FROM_AES3 3
#define HDSPM_AES32_AUTOSYNC_FROM_AES4 4
#define HDSPM_AES32_AUTOSYNC_FROM_AES5 5
#define HDSPM_AES32_AUTOSYNC_FROM_AES6 6
#define HDSPM_AES32_AUTOSYNC_FROM_AES7 7
#define HDSPM_AES32_AUTOSYNC_FROM_AES8 8
#define HDSPM_AES32_AUTOSYNC_FROM_NONE 9

/*  status2 */
/* HDSPM_LockAES_bit is given by HDSPM_LockAES >> (AES# - 1) */
#define HDSPM_LockAES   0x80
#define HDSPM_LockAES1  0x80
#define HDSPM_LockAES2  0x40
#define HDSPM_LockAES3  0x20
#define HDSPM_LockAES4  0x10
#define HDSPM_LockAES5  0x8
#define HDSPM_LockAES6  0x4
#define HDSPM_LockAES7  0x2
#define HDSPM_LockAES8  0x1
/*
   Timecode
   After windows driver sources, bits 4*i to 4*i+3 give the input frequency on
   AES i+1
 bits 3210
      0001  32kHz
      0010  44.1kHz
      0011  48kHz
      0100  64kHz
      0101  88.2kHz
      0110  96kHz
      0111  128kHz
      1000  176.4kHz
      1001  192kHz
  NB: Timecode register doesn't seem to work on AES32 card revision 230
*/

/* Mixer Values */
#define UNITY_GAIN          32768	/* = 65536/2 */
#define MINUS_INFINITY_GAIN 0

/* Number of channels for different Speed Modes */
#define MADI_SS_CHANNELS       64
#define MADI_DS_CHANNELS       32
#define MADI_QS_CHANNELS       16

/* the size of a substream (1 mono data stream) */
#define HDSPM_CHANNEL_BUFFER_SAMPLES  (16*1024)
#define HDSPM_CHANNEL_BUFFER_BYTES    (4*HDSPM_CHANNEL_BUFFER_SAMPLES)

/* the size of the area we need to allocate for DMA transfers. the
   size is the same regardless of the number of channels, and
   also the latency to use. 
   for one direction !!!
*/
#define HDSPM_DMA_AREA_BYTES (HDSPM_MAX_CHANNELS * HDSPM_CHANNEL_BUFFER_BYTES)
#define HDSPM_DMA_AREA_KILOBYTES (HDSPM_DMA_AREA_BYTES/1024)

/* revisions >= 230 indicate AES32 card */
#define HDSPM_AESREVISION 230

/* speed factor modes */
#define HDSPM_SPEED_SINGLE 0
#define HDSPM_SPEED_DOUBLE 1
#define HDSPM_SPEED_QUAD   2
/* names for speed modes */
static char *hdspm_speed_names[] = { "single", "double", "quad" };

struct hdspm_midi {
	struct hdspm *hdspm;
	int id;
	struct snd_rawmidi *rmidi;
	struct snd_rawmidi_substream *input;
	struct snd_rawmidi_substream *output;
	char istimer;		/* timer in use */
	struct timer_list timer;
	spinlock_t lock;
	int pending;
};

struct hdspm {
        spinlock_t lock;
	/* only one playback and/or capture stream */
        struct snd_pcm_substream *capture_substream;
        struct snd_pcm_substream *playback_substream;

	char *card_name;	     /* for procinfo */
	unsigned short firmware_rev; /* dont know if relevant (yes if AES32)*/

	unsigned char is_aes32;    /* indicates if card is AES32 */

	int precise_ptr;	/* use precise pointers, to be tested */
	int monitor_outs;	/* set up monitoring outs init flag */

	u32 control_register;	/* cached value */
	u32 control2_register;	/* cached value */

	struct hdspm_midi midi[2];
	struct tasklet_struct midi_tasklet;

	size_t period_bytes;
	unsigned char ss_channels;	/* channels of card in single speed */
	unsigned char ds_channels;	/* Double Speed */
	unsigned char qs_channels;	/* Quad Speed */

	unsigned char *playback_buffer;	/* suitably aligned address */
	unsigned char *capture_buffer;	/* suitably aligned address */

	pid_t capture_pid;	/* process id which uses capture */
	pid_t playback_pid;	/* process id which uses capture */
	int running;		/* running status */

	int last_external_sample_rate;	/* samplerate mystic ... */
	int last_internal_sample_rate;
	int system_sample_rate;

	char *channel_map;	/* channel map for DS and Quadspeed */

	int dev;		/* Hardware vars... */
	int irq;
	unsigned long port;
	void __iomem *iobase;

	int irq_count;		/* for debug */

	struct snd_card *card;	/* one card */
	struct snd_pcm *pcm;		/* has one pcm */
	struct snd_hwdep *hwdep;	/* and a hwdep for additional ioctl */
	struct pci_dev *pci;	/* and an pci info */

	/* Mixer vars */
	/* fast alsa mixer */
	struct snd_kcontrol *playback_mixer_ctls[HDSPM_MAX_CHANNELS];
	/* but input to much, so not used */
	struct snd_kcontrol *input_mixer_ctls[HDSPM_MAX_CHANNELS];
	/* full mixer accessable over mixer ioctl or hwdep-device */
	struct hdspm_mixer *mixer;

};

/* These tables map the ALSA channels 1..N to the channels that we
   need to use in order to find the relevant channel buffer. RME
   refer to this kind of mapping as between "the ADAT channel and
   the DMA channel." We index it using the logical audio channel,
   and the value is the DMA channel (i.e. channel buffer number)
   where the data for that channel can be read/written from/to.
*/

static char channel_map_madi_ss[HDSPM_MAX_CHANNELS] = {
   0, 1, 2, 3, 4, 5, 6, 7,
   8, 9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23,
   24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39,
   40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55,
   56, 57, 58, 59, 60, 61, 62, 63
};


static struct pci_device_id snd_hdspm_ids[] __devinitdata = {
	{
	 .vendor = PCI_VENDOR_ID_XILINX,
	 .device = PCI_DEVICE_ID_XILINX_HAMMERFALL_DSP_MADI,
	 .subvendor = PCI_ANY_ID,
	 .subdevice = PCI_ANY_ID,
	 .class = 0,
	 .class_mask = 0,
	 .driver_data = 0},
	{0,}
};

MODULE_DEVICE_TABLE(pci, snd_hdspm_ids);

/* prototypes */
static int __devinit snd_hdspm_create_alsa_devices(struct snd_card *card,
						   struct hdspm * hdspm);
static int __devinit snd_hdspm_create_pcm(struct snd_card *card,
					  struct hdspm * hdspm);

static inline void snd_hdspm_initialize_midi_flush(struct hdspm * hdspm);
static int hdspm_update_simple_mixer_controls(struct hdspm * hdspm);
static int hdspm_autosync_ref(struct hdspm * hdspm);
static int snd_hdspm_set_defaults(struct hdspm * hdspm);
static void hdspm_set_sgbuf(struct hdspm * hdspm,
			    struct snd_pcm_substream *substream,
			     unsigned int reg, int channels);

static inline int HDSPM_bit2freq(int n)
{
	static const int bit2freq_tab[] = {
		0, 32000, 44100, 48000, 64000, 88200,
		96000, 128000, 176400, 192000 };
	if (n < 1 || n > 9)
		return 0;
	return bit2freq_tab[n];
}

/* Write/read to/from HDSPM with Adresses in Bytes
   not words but only 32Bit writes are allowed */

static inline void hdspm_write(struct hdspm * hdspm, unsigned int reg,
			       unsigned int val)
{
	writel(val, hdspm->iobase + reg);
}

static inline unsigned int hdspm_read(struct hdspm * hdspm, unsigned int reg)
{
	return readl(hdspm->iobase + reg);
}

/* for each output channel (chan) I have an Input (in) and Playback (pb) Fader 
   mixer is write only on hardware so we have to cache him for read 
   each fader is a u32, but uses only the first 16 bit */

static inline int hdspm_read_in_gain(struct hdspm * hdspm, unsigned int chan,
				     unsigned int in)
{
	if (chan >= HDSPM_MIXER_CHANNELS || in >= HDSPM_MIXER_CHANNELS)
		return 0;

	return hdspm->mixer->ch[chan].in[in];
}

static inline int hdspm_read_pb_gain(struct hdspm * hdspm, unsigned int chan,
				     unsigned int pb)
{
	if (chan >= HDSPM_MIXER_CHANNELS || pb >= HDSPM_MIXER_CHANNELS)
		return 0;
	return hdspm->mixer->ch[chan].pb[pb];
}

static int hdspm_write_in_gain(struct hdspm *hdspm, unsigned int chan,
				      unsigned int in, unsigned short data)
{
	if (chan >= HDSPM_MIXER_CHANNELS || in >= HDSPM_MIXER_CHANNELS)
		return -1;

	hdspm_write(hdspm,
		    HDSPM_MADI_mixerBase +
		    ((in + 128 * chan) * sizeof(u32)),
		    (hdspm->mixer->ch[chan].in[in] = data & 0xFFFF));
	return 0;
}

static int hdspm_write_pb_gain(struct hdspm *hdspm, unsigned int chan,
				      unsigned int pb, unsigned short data)
{
	if (chan >= HDSPM_MIXER_CHANNELS || pb >= HDSPM_MIXER_CHANNELS)
		return -1;

	hdspm_write(hdspm,
		    HDSPM_MADI_mixerBase +
		    ((64 + pb + 128 * chan) * sizeof(u32)),
		    (hdspm->mixer->ch[chan].pb[pb] = data & 0xFFFF));
	return 0;
}


/* enable DMA for specific channels, now available for DSP-MADI */
static inline void snd_hdspm_enable_in(struct hdspm * hdspm, int i, int v)
{
	hdspm_write(hdspm, HDSPM_inputEnableBase + (4 * i), v);
}

static inline void snd_hdspm_enable_out(struct hdspm * hdspm, int i, int v)
{
	hdspm_write(hdspm, HDSPM_outputEnableBase + (4 * i), v);
}

/* check if same process is writing and reading */
static int snd_hdspm_use_is_exclusive(struct hdspm *hdspm)
{
	unsigned long flags;
	int ret = 1;

	spin_lock_irqsave(&hdspm->lock, flags);
	if ((hdspm->playback_pid != hdspm->capture_pid) &&
	    (hdspm->playback_pid >= 0) && (hdspm->capture_pid >= 0)) {
		ret = 0;
	}
	spin_unlock_irqrestore(&hdspm->lock, flags);
	return ret;
}

/* check for external sample rate */
static int hdspm_external_sample_rate(struct hdspm *hdspm)
{
	if (hdspm->is_aes32) {
		unsigned int status2 = hdspm_read(hdspm, HDSPM_statusRegister2);
		unsigned int status = hdspm_read(hdspm, HDSPM_statusRegister);
		unsigned int timecode =
			hdspm_read(hdspm, HDSPM_timecodeRegister);

		int syncref = hdspm_autosync_ref(hdspm);

		if (syncref == HDSPM_AES32_AUTOSYNC_FROM_WORD &&
				status & HDSPM_AES32_wcLock)
			return HDSPM_bit2freq((status >> HDSPM_AES32_wcFreq_bit)
					      & 0xF);
		if (syncref >= HDSPM_AES32_AUTOSYNC_FROM_AES1 &&
			syncref <= HDSPM_AES32_AUTOSYNC_FROM_AES8 &&
			status2 & (HDSPM_LockAES >>
			          (syncref - HDSPM_AES32_AUTOSYNC_FROM_AES1)))
			return HDSPM_bit2freq((timecode >>
			  (4*(syncref-HDSPM_AES32_AUTOSYNC_FROM_AES1))) & 0xF);
		return 0;
	} else {
		unsigned int status2 = hdspm_read(hdspm, HDSPM_statusRegister2);
		unsigned int status = hdspm_read(hdspm, HDSPM_statusRegister);
		unsigned int rate_bits;
		int rate = 0;

		/* if wordclock has synced freq and wordclock is valid */
		if ((status2 & HDSPM_wcLock) != 0 &&
				(status & HDSPM_SelSyncRef0) == 0) {

			rate_bits = status2 & HDSPM_wcFreqMask;

			switch (rate_bits) {
			case HDSPM_wcFreq32:
				rate = 32000;
				break;
			case HDSPM_wcFreq44_1:
				rate = 44100;
				break;
			case HDSPM_wcFreq48:
				rate = 48000;
				break;
			case HDSPM_wcFreq64:
				rate = 64000;
				break;
			case HDSPM_wcFreq88_2:
				rate = 88200;
				break;
			case HDSPM_wcFreq96:
				rate = 96000;
				break;
				/* Quadspeed Bit missing ???? */
			default:
				rate = 0;
				break;
			}
		}

		/* if rate detected and Syncref is Word than have it,
		 * word has priority to MADI
		 */
		if (rate != 0 &&
	            (status2 & HDSPM_SelSyncRefMask) == HDSPM_SelSyncRef_WORD)
			return rate;

		/* maby a madi input (which is taken if sel sync is madi) */
		if (status & HDSPM_madiLock) {
			rate_bits = status & HDSPM_madiFreqMask;

			switch (rate_bits) {
			case HDSPM_madiFreq32:
				rate = 32000;
				break;
			case HDSPM_madiFreq44_1:
				rate = 44100;
				break;
			case HDSPM_madiFreq48:
				rate = 48000;
				break;
			case HDSPM_madiFreq64:
				rate = 64000;
				break;
			case HDSPM_madiFreq88_2:
				rate = 88200;
				break;
			case HDSPM_madiFreq96:
				rate = 96000;
				break;
			case HDSPM_madiFreq128:
				rate = 128000;
				break;
			case HDSPM_madiFreq176_4:
				rate = 176400;
				break;
			case HDSPM_madiFreq192:
				rate = 192000;
				break;
			default:
				rate = 0;
				break;
			}
		}
		return rate;
	}
}

/* Latency function */
static inline void hdspm_compute_period_size(struct hdspm * hdspm)
{
	hdspm->period_bytes =
	    1 << ((hdspm_decode_latency(hdspm->control_register) + 8));
}

static snd_pcm_uframes_t hdspm_hw_pointer(struct hdspm * hdspm)
{
	int position;

	position = hdspm_read(hdspm, HDSPM_statusRegister);

	if (!hdspm->precise_ptr)
		return (position & HDSPM_BufferID) ?
			(hdspm->period_bytes / 4) : 0;

	/* hwpointer comes in bytes and is 64Bytes accurate (by docu since
	   PCI Burst)
	   i have experimented that it is at most 64 Byte to much for playing 
	   so substraction of 64 byte should be ok for ALSA, but use it only
	   for application where you know what you do since if you come to
	   near with record pointer it can be a disaster */

	position &= HDSPM_BufferPositionMask;
	position = ((position - 64) % (2 * hdspm->period_bytes)) / 4;

	return position;
}


static inline void hdspm_start_audio(struct hdspm * s)
{
	s->control_register |= (HDSPM_AudioInterruptEnable | HDSPM_Start);
	hdspm_write(s, HDSPM_controlRegister, s->control_register);
}

static inline void hdspm_stop_audio(struct hdspm * s)
{
	s->control_register &= ~(HDSPM_Start | HDSPM_AudioInterruptEnable);
	hdspm_write(s, HDSPM_controlRegister, s->control_register);
}

/* should I silence all or only opened ones ? doit all for first even is 4MB*/
static void hdspm_silence_playback(struct hdspm *hdspm)
{
	int i;
	int n = hdspm->period_bytes;
	void *buf = hdspm->playback_buffer;

	if (buf == NULL)
		return;

	for (i = 0; i < HDSPM_MAX_CHANNELS; i++) {
		memset(buf, 0, n);
		buf += HDSPM_CHANNEL_BUFFER_BYTES;
	}
}

static int hdspm_set_interrupt_interval(struct hdspm * s, unsigned int frames)
{
	int n;

	spin_lock_irq(&s->lock);

	frames >>= 7;
	n = 0;
	while (frames) {
		n++;
		frames >>= 1;
	}
	s->control_register &= ~HDSPM_LatencyMask;
	s->control_register |= hdspm_encode_latency(n);

	hdspm_write(s, HDSPM_controlRegister, s->control_register);

	hdspm_compute_period_size(s);

	spin_unlock_irq(&s->lock);

	return 0;
}

static void hdspm_set_dds_value(struct hdspm *hdspm, int rate)
{
	u64 n;
	
	if (rate >= 112000)
		rate /= 4;
	else if (rate >= 56000)
		rate /= 2;

	/* RME says n = 104857600000000, but in the windows MADI driver, I see:
//	return 104857600000000 / rate; // 100 MHz
	return 110100480000000 / rate; // 105 MHz
        */	   
	/* n = 104857600000000ULL; */ /*  =  2^20 * 10^8 */
	n = 110100480000000ULL;    /* Value checked for AES32 and MADI */
	n = div_u64(n, rate);
	/* n should be less than 2^32 for being written to FREQ register */
	snd_BUG_ON(n >> 32);
	hdspm_write(hdspm, HDSPM_freqReg, (u32)n);
}

/* dummy set rate lets see what happens */
static int hdspm_set_rate(struct hdspm * hdspm, int rate, int called_internally)
{
	int current_rate;
	int rate_bits;
	int not_set = 0;
	int current_speed, target_speed;

	/* ASSUMPTION: hdspm->lock is either set, or there is no need for
	   it (e.g. during module initialization).
	 */

	if (!(hdspm->control_register & HDSPM_ClockModeMaster)) {

	        /* SLAVE --- */ 
		if (called_internally) {

        	  /* request from ctl or card initialization 
	             just make a warning an remember setting 
		     for future master mode switching */
    
			snd_printk(KERN_WARNING "HDSPM: "
				   "Warning: device is not running "
				   "as a clock master.\n");
			not_set = 1;
		} else {

			/* hw_param request while in AutoSync mode */
			int external_freq =
			    hdspm_external_sample_rate(hdspm);

			if (hdspm_autosync_ref(hdspm) ==
			    HDSPM_AUTOSYNC_FROM_NONE) {

				snd_printk(KERN_WARNING "HDSPM: "
					   "Detected no Externel Sync \n");
				not_set = 1;

			} else if (rate != external_freq) {

				snd_printk(KERN_WARNING "HDSPM: "
					   "Warning: No AutoSync source for "
					   "requested rate\n");
				not_set = 1;
			}
		}
	}

	current_rate = hdspm->system_sample_rate;

	/* Changing between Singe, Double and Quad speed is not
	   allowed if any substreams are open. This is because such a change
	   causes a shift in the location of the DMA buffers and a reduction
	   in the number of available buffers.

	   Note that a similar but essentially insoluble problem exists for
	   externally-driven rate changes. All we can do is to flag rate
	   changes in the read/write routines.  
	 */

	if (current_rate <= 48000)
		current_speed = HDSPM_SPEED_SINGLE;
	else if (current_rate <= 96000)
		current_speed = HDSPM_SPEED_DOUBLE;
	else
		current_speed = HDSPM_SPEED_QUAD;

	if (rate <= 48000)
		target_speed = HDSPM_SPEED_SINGLE;
	else if (rate <= 96000)
		target_speed = HDSPM_SPEED_DOUBLE;
	else
		target_speed = HDSPM_SPEED_QUAD;

	switch (rate) {
	case 32000:
		rate_bits = HDSPM_Frequency32KHz;
		break;
	case 44100:
		rate_bits = HDSPM_Frequency44_1KHz;
		break;
	case 48000:
		rate_bits = HDSPM_Frequency48KHz;
		break;
	case 64000:
		rate_bits = HDSPM_Frequency64KHz;
		break;
	case 88200:
		rate_bits = HDSPM_Frequency88_2KHz;
		break;
	case 96000:
		rate_bits = HDSPM_Frequency96KHz;
		break;
	case 128000:
		rate_bits = HDSPM_Frequency128KHz;
		break;
	case 176400:
		rate_bits = HDSPM_Frequency176_4KHz;
		break;
	case 192000:
		rate_bits = HDSPM_Frequency192KHz;
		break;
	default:
		return -EINVAL;
	}

	if (current_speed != target_speed
	    && (hdspm->capture_pid >= 0 || hdspm->playback_pid >= 0)) {
		snd_printk
		    (KERN_ERR "HDSPM: "
		     "cannot change from %s speed to %s speed mode "
		     "(capture PID = %d, playback PID = %d)\n",
		     hdspm_speed_names[current_speed],
		     hdspm_speed_names[target_speed],
		     hdspm->capture_pid, hdspm->playback_pid);
		return -EBUSY;
	}

	hdspm->control_register &= ~HDSPM_FrequencyMask;
	hdspm->control_register |= rate_bits;
	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);

	/* For AES32, need to set DDS value in FREQ register
	   For MADI, also apparently */
	hdspm_set_dds_value(hdspm, rate);
	
	if (hdspm->is_aes32 && rate != current_rate)
		hdspm_write(hdspm, HDSPM_eeprom_wr, 0);
	
	/* For AES32 and for MADI (at least rev 204), channel_map needs to
	 * always be channel_map_madi_ss, whatever the sample rate */
	hdspm->channel_map = channel_map_madi_ss;

	hdspm->system_sample_rate = rate;

	if (not_set != 0)
		return -1;

	return 0;
}

/* mainly for init to 0 on load */
static void all_in_all_mixer(struct hdspm * hdspm, int sgain)
{
	int i, j;
	unsigned int gain;

	if (sgain > UNITY_GAIN)
		gain = UNITY_GAIN;
	else if (sgain < 0)
		gain = 0;
	else
		gain = sgain;

	for (i = 0; i < HDSPM_MIXER_CHANNELS; i++)
		for (j = 0; j < HDSPM_MIXER_CHANNELS; j++) {
			hdspm_write_in_gain(hdspm, i, j, gain);
			hdspm_write_pb_gain(hdspm, i, j, gain);
		}
}

/*----------------------------------------------------------------------------
   MIDI
  ----------------------------------------------------------------------------*/

static inline unsigned char snd_hdspm_midi_read_byte (struct hdspm *hdspm,
						      int id)
{
	/* the hardware already does the relevant bit-mask with 0xff */
	if (id)
		return hdspm_read(hdspm, HDSPM_midiDataIn1);
	else
		return hdspm_read(hdspm, HDSPM_midiDataIn0);
}

static inline void snd_hdspm_midi_write_byte (struct hdspm *hdspm, int id,
					      int val)
{
	/* the hardware already does the relevant bit-mask with 0xff */
	if (id)
		hdspm_write(hdspm, HDSPM_midiDataOut1, val);
	else
		hdspm_write(hdspm, HDSPM_midiDataOut0, val);
}

static inline int snd_hdspm_midi_input_available (struct hdspm *hdspm, int id)
{
	if (id)
		return (hdspm_read(hdspm, HDSPM_midiStatusIn1) & 0xff);
	else
		return (hdspm_read(hdspm, HDSPM_midiStatusIn0) & 0xff);
}

static inline int snd_hdspm_midi_output_possible (struct hdspm *hdspm, int id)
{
	int fifo_bytes_used;

	if (id)
		fifo_bytes_used = hdspm_read(hdspm, HDSPM_midiStatusOut1);
	else
		fifo_bytes_used = hdspm_read(hdspm, HDSPM_midiStatusOut0);
	fifo_bytes_used &= 0xff;

	if (fifo_bytes_used < 128)
		return  128 - fifo_bytes_used;
	else
		return 0;
}

static void snd_hdspm_flush_midi_input(struct hdspm *hdspm, int id)
{
	while (snd_hdspm_midi_input_available (hdspm, id))
		snd_hdspm_midi_read_byte (hdspm, id);
}

static int snd_hdspm_midi_output_write (struct hdspm_midi *hmidi)
{
	unsigned long flags;
	int n_pending;
	int to_write;
	int i;
	unsigned char buf[128];

	/* Output is not interrupt driven */
		
	spin_lock_irqsave (&hmidi->lock, flags);
	if (hmidi->output &&
	    !snd_rawmidi_transmit_empty (hmidi->output)) {
		n_pending = snd_hdspm_midi_output_possible (hmidi->hdspm,
							    hmidi->id);
		if (n_pending > 0) {
			if (n_pending > (int)sizeof (buf))
				n_pending = sizeof (buf);
		
			to_write = snd_rawmidi_transmit (hmidi->output, buf,
							 n_pending);
			if (to_write > 0) {
				for (i = 0; i < to_write; ++i) 
					snd_hdspm_midi_write_byte (hmidi->hdspm,
								   hmidi->id,
								   buf[i]);
			}
		}
	}
	spin_unlock_irqrestore (&hmidi->lock, flags);
	return 0;
}

static int snd_hdspm_midi_input_read (struct hdspm_midi *hmidi)
{
	unsigned char buf[128]; /* this buffer is designed to match the MIDI
				 * input FIFO size
				 */
	unsigned long flags;
	int n_pending;
	int i;

	spin_lock_irqsave (&hmidi->lock, flags);
	n_pending = snd_hdspm_midi_input_available (hmidi->hdspm, hmidi->id);
	if (n_pending > 0) {
		if (hmidi->input) {
			if (n_pending > (int)sizeof (buf))
				n_pending = sizeof (buf);
			for (i = 0; i < n_pending; ++i)
				buf[i] = snd_hdspm_midi_read_byte (hmidi->hdspm,
								   hmidi->id);
			if (n_pending)
				snd_rawmidi_receive (hmidi->input, buf,
						     n_pending);
		} else {
			/* flush the MIDI input FIFO */
			while (n_pending--)
				snd_hdspm_midi_read_byte (hmidi->hdspm,
							  hmidi->id);
		}
	}
	hmidi->pending = 0;
	if (hmidi->id)
		hmidi->hdspm->control_register |= HDSPM_Midi1InterruptEnable;
	else
		hmidi->hdspm->control_register |= HDSPM_Midi0InterruptEnable;
	hdspm_write(hmidi->hdspm, HDSPM_controlRegister,
		    hmidi->hdspm->control_register);
	spin_unlock_irqrestore (&hmidi->lock, flags);
	return snd_hdspm_midi_output_write (hmidi);
}

static void
snd_hdspm_midi_input_trigger(struct snd_rawmidi_substream *substream, int up)
{
	struct hdspm *hdspm;
	struct hdspm_midi *hmidi;
	unsigned long flags;
	u32 ie;

	hmidi = substream->rmidi->private_data;
	hdspm = hmidi->hdspm;
	ie = hmidi->id ?
		HDSPM_Midi1InterruptEnable : HDSPM_Midi0InterruptEnable;
	spin_lock_irqsave (&hdspm->lock, flags);
	if (up) {
		if (!(hdspm->control_register & ie)) {
			snd_hdspm_flush_midi_input (hdspm, hmidi->id);
			hdspm->control_register |= ie;
		}
	} else {
		hdspm->control_register &= ~ie;
	}

	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);
	spin_unlock_irqrestore (&hdspm->lock, flags);
}

static void snd_hdspm_midi_output_timer(unsigned long data)
{
	struct hdspm_midi *hmidi = (struct hdspm_midi *) data;
	unsigned long flags;
	
	snd_hdspm_midi_output_write(hmidi);
	spin_lock_irqsave (&hmidi->lock, flags);

	/* this does not bump hmidi->istimer, because the
	   kernel automatically removed the timer when it
	   expired, and we are now adding it back, thus
	   leaving istimer wherever it was set before.  
	*/

	if (hmidi->istimer) {
		hmidi->timer.expires = 1 + jiffies;
		add_timer(&hmidi->timer);
	}

	spin_unlock_irqrestore (&hmidi->lock, flags);
}

static void
snd_hdspm_midi_output_trigger(struct snd_rawmidi_substream *substream, int up)
{
	struct hdspm_midi *hmidi;
	unsigned long flags;

	hmidi = substream->rmidi->private_data;
	spin_lock_irqsave (&hmidi->lock, flags);
	if (up) {
		if (!hmidi->istimer) {
			init_timer(&hmidi->timer);
			hmidi->timer.function = snd_hdspm_midi_output_timer;
			hmidi->timer.data = (unsigned long) hmidi;
			hmidi->timer.expires = 1 + jiffies;
			add_timer(&hmidi->timer);
			hmidi->istimer++;
		}
	} else {
		if (hmidi->istimer && --hmidi->istimer <= 0)
			del_timer (&hmidi->timer);
	}
	spin_unlock_irqrestore (&hmidi->lock, flags);
	if (up)
		snd_hdspm_midi_output_write(hmidi);
}

static int snd_hdspm_midi_input_open(struct snd_rawmidi_substream *substream)
{
	struct hdspm_midi *hmidi;

	hmidi = substream->rmidi->private_data;
	spin_lock_irq (&hmidi->lock);
	snd_hdspm_flush_midi_input (hmidi->hdspm, hmidi->id);
	hmidi->input = substream;
	spin_unlock_irq (&hmidi->lock);

	return 0;
}

static int snd_hdspm_midi_output_open(struct snd_rawmidi_substream *substream)
{
	struct hdspm_midi *hmidi;

	hmidi = substream->rmidi->private_data;
	spin_lock_irq (&hmidi->lock);
	hmidi->output = substream;
	spin_unlock_irq (&hmidi->lock);

	return 0;
}

static int snd_hdspm_midi_input_close(struct snd_rawmidi_substream *substream)
{
	struct hdspm_midi *hmidi;

	snd_hdspm_midi_input_trigger (substream, 0);

	hmidi = substream->rmidi->private_data;
	spin_lock_irq (&hmidi->lock);
	hmidi->input = NULL;
	spin_unlock_irq (&hmidi->lock);

	return 0;
}

static int snd_hdspm_midi_output_close(struct snd_rawmidi_substream *substream)
{
	struct hdspm_midi *hmidi;

	snd_hdspm_midi_output_trigger (substream, 0);

	hmidi = substream->rmidi->private_data;
	spin_lock_irq (&hmidi->lock);
	hmidi->output = NULL;
	spin_unlock_irq (&hmidi->lock);

	return 0;
}

static struct snd_rawmidi_ops snd_hdspm_midi_output =
{
	.open =		snd_hdspm_midi_output_open,
	.close =	snd_hdspm_midi_output_close,
	.trigger =	snd_hdspm_midi_output_trigger,
};

static struct snd_rawmidi_ops snd_hdspm_midi_input =
{
	.open =		snd_hdspm_midi_input_open,
	.close =	snd_hdspm_midi_input_close,
	.trigger =	snd_hdspm_midi_input_trigger,
};

static int __devinit snd_hdspm_create_midi (struct snd_card *card,
					    struct hdspm *hdspm, int id)
{
	int err;
	char buf[32];

	hdspm->midi[id].id = id;
	hdspm->midi[id].hdspm = hdspm;
	spin_lock_init (&hdspm->midi[id].lock);

	sprintf (buf, "%s MIDI %d", card->shortname, id+1);
	err = snd_rawmidi_new (card, buf, id, 1, 1, &hdspm->midi[id].rmidi);
	if (err < 0)
		return err;

	sprintf(hdspm->midi[id].rmidi->name, "HDSPM MIDI %d", id+1);
	hdspm->midi[id].rmidi->private_data = &hdspm->midi[id];

	snd_rawmidi_set_ops(hdspm->midi[id].rmidi, SNDRV_RAWMIDI_STREAM_OUTPUT,
			    &snd_hdspm_midi_output);
	snd_rawmidi_set_ops(hdspm->midi[id].rmidi, SNDRV_RAWMIDI_STREAM_INPUT,
			    &snd_hdspm_midi_input);

	hdspm->midi[id].rmidi->info_flags |= SNDRV_RAWMIDI_INFO_OUTPUT |
		SNDRV_RAWMIDI_INFO_INPUT |
		SNDRV_RAWMIDI_INFO_DUPLEX;

	return 0;
}


static void hdspm_midi_tasklet(unsigned long arg)
{
	struct hdspm *hdspm = (struct hdspm *)arg;
	
	if (hdspm->midi[0].pending)
		snd_hdspm_midi_input_read (&hdspm->midi[0]);
	if (hdspm->midi[1].pending)
		snd_hdspm_midi_input_read (&hdspm->midi[1]);
} 


/*-----------------------------------------------------------------------------
  Status Interface
  ----------------------------------------------------------------------------*/

/* get the system sample rate which is set */

#define HDSPM_SYSTEM_SAMPLE_RATE(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ, \
  .info = snd_hdspm_info_system_sample_rate, \
  .get = snd_hdspm_get_system_sample_rate \
}

static int snd_hdspm_info_system_sample_rate(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	return 0;
}

static int snd_hdspm_get_system_sample_rate(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *
					    ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdspm->system_sample_rate;
	return 0;
}

#define HDSPM_AUTOSYNC_SAMPLE_RATE(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ, \
  .info = snd_hdspm_info_autosync_sample_rate, \
  .get = snd_hdspm_get_autosync_sample_rate \
}

static int snd_hdspm_info_autosync_sample_rate(struct snd_kcontrol *kcontrol,
					       struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = { "32000", "44100", "48000",
		"64000", "88200", "96000",
		"128000", "176400", "192000",
		"None"
	};
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 10;
	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item =
		    uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name,
	       texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_hdspm_get_autosync_sample_rate(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_value *
					      ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	switch (hdspm_external_sample_rate(hdspm)) {
	case 32000:
		ucontrol->value.enumerated.item[0] = 0;
		break;
	case 44100:
		ucontrol->value.enumerated.item[0] = 1;
		break;
	case 48000:
		ucontrol->value.enumerated.item[0] = 2;
		break;
	case 64000:
		ucontrol->value.enumerated.item[0] = 3;
		break;
	case 88200:
		ucontrol->value.enumerated.item[0] = 4;
		break;
	case 96000:
		ucontrol->value.enumerated.item[0] = 5;
		break;
	case 128000:
		ucontrol->value.enumerated.item[0] = 6;
		break;
	case 176400:
		ucontrol->value.enumerated.item[0] = 7;
		break;
	case 192000:
		ucontrol->value.enumerated.item[0] = 8;
		break;

	default:
		ucontrol->value.enumerated.item[0] = 9;
	}
	return 0;
}

#define HDSPM_SYSTEM_CLOCK_MODE(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ, \
  .info = snd_hdspm_info_system_clock_mode, \
  .get = snd_hdspm_get_system_clock_mode, \
}



static int hdspm_system_clock_mode(struct hdspm * hdspm)
{
        /* Always reflect the hardware info, rme is never wrong !!!! */

	if (hdspm->control_register & HDSPM_ClockModeMaster)
		return 0;
	return 1;
}

static int snd_hdspm_info_system_clock_mode(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = { "Master", "Slave" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 2;
	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item =
		    uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name,
	       texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_hdspm_get_system_clock_mode(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] =
	    hdspm_system_clock_mode(hdspm);
	return 0;
}

#define HDSPM_CLOCK_SOURCE(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdspm_info_clock_source, \
  .get = snd_hdspm_get_clock_source, \
  .put = snd_hdspm_put_clock_source \
}

static int hdspm_clock_source(struct hdspm * hdspm)
{
	if (hdspm->control_register & HDSPM_ClockModeMaster) {
		switch (hdspm->system_sample_rate) {
		case 32000:
			return 1;
		case 44100:
			return 2;
		case 48000:
			return 3;
		case 64000:
			return 4;
		case 88200:
			return 5;
		case 96000:
			return 6;
		case 128000:
			return 7;
		case 176400:
			return 8;
		case 192000:
			return 9;
		default:
			return 3;
		}
	} else {
		return 0;
	}
}

static int hdspm_set_clock_source(struct hdspm * hdspm, int mode)
{
	int rate;
	switch (mode) {

	case HDSPM_CLOCK_SOURCE_AUTOSYNC:
		if (hdspm_external_sample_rate(hdspm) != 0) {
			hdspm->control_register &= ~HDSPM_ClockModeMaster;
			hdspm_write(hdspm, HDSPM_controlRegister,
				    hdspm->control_register);
			return 0;
		}
		return -1;
	case HDSPM_CLOCK_SOURCE_INTERNAL_32KHZ:
		rate = 32000;
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_44_1KHZ:
		rate = 44100;
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_48KHZ:
		rate = 48000;
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_64KHZ:
		rate = 64000;
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_88_2KHZ:
		rate = 88200;
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_96KHZ:
		rate = 96000;
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_128KHZ:
		rate = 128000;
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_176_4KHZ:
		rate = 176400;
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_192KHZ:
		rate = 192000;
		break;

	default:
		rate = 44100;
	}
	hdspm->control_register |= HDSPM_ClockModeMaster;
	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);
	hdspm_set_rate(hdspm, rate, 1);
	return 0;
}

static int snd_hdspm_info_clock_source(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = { "AutoSync",
		"Internal 32.0 kHz", "Internal 44.1 kHz",
		    "Internal 48.0 kHz",
		"Internal 64.0 kHz", "Internal 88.2 kHz",
		    "Internal 96.0 kHz",
		"Internal 128.0 kHz", "Internal 176.4 kHz",
		    "Internal 192.0 kHz"
	};

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 10;

	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item =
		    uinfo->value.enumerated.items - 1;

	strcpy(uinfo->value.enumerated.name,
	       texts[uinfo->value.enumerated.item]);

	return 0;
}

static int snd_hdspm_get_clock_source(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdspm_clock_source(hdspm);
	return 0;
}

static int snd_hdspm_put_clock_source(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int change;
	int val;

	if (!snd_hdspm_use_is_exclusive(hdspm))
		return -EBUSY;
	val = ucontrol->value.enumerated.item[0];
	if (val < 0)
		val = 0;
	if (val > 9)
		val = 9;
	spin_lock_irq(&hdspm->lock);
	if (val != hdspm_clock_source(hdspm))
		change = (hdspm_set_clock_source(hdspm, val) == 0) ? 1 : 0;
	else
		change = 0;
	spin_unlock_irq(&hdspm->lock);
	return change;
}

#define HDSPM_PREF_SYNC_REF(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdspm_info_pref_sync_ref, \
  .get = snd_hdspm_get_pref_sync_ref, \
  .put = snd_hdspm_put_pref_sync_ref \
}

static int hdspm_pref_sync_ref(struct hdspm * hdspm)
{
	/* Notice that this looks at the requested sync source,
	   not the one actually in use.
	 */
	if (hdspm->is_aes32) {
		switch (hdspm->control_register & HDSPM_SyncRefMask) {
		/* number gives AES index, except for 0 which
		   corresponds to WordClock */
		case 0: return 0;
		case HDSPM_SyncRef0: return 1;
		case HDSPM_SyncRef1: return 2;
		case HDSPM_SyncRef1+HDSPM_SyncRef0: return 3;
		case HDSPM_SyncRef2: return 4;
		case HDSPM_SyncRef2+HDSPM_SyncRef0: return 5;
		case HDSPM_SyncRef2+HDSPM_SyncRef1: return 6;
		case HDSPM_SyncRef2+HDSPM_SyncRef1+HDSPM_SyncRef0: return 7;
		case HDSPM_SyncRef3: return 8;
		}
	} else {
		switch (hdspm->control_register & HDSPM_SyncRefMask) {
		case HDSPM_SyncRef_Word:
			return HDSPM_SYNC_FROM_WORD;
		case HDSPM_SyncRef_MADI:
			return HDSPM_SYNC_FROM_MADI;
		}
	}

	return HDSPM_SYNC_FROM_WORD;
}

static int hdspm_set_pref_sync_ref(struct hdspm * hdspm, int pref)
{
	hdspm->control_register &= ~HDSPM_SyncRefMask;

	if (hdspm->is_aes32) {
		switch (pref) {
		case 0:
		       hdspm->control_register |= 0;
		       break;
		case 1:
		       hdspm->control_register |= HDSPM_SyncRef0;
		       break;
		case 2:
		       hdspm->control_register |= HDSPM_SyncRef1;
		       break;
		case 3:
		       hdspm->control_register |= HDSPM_SyncRef1+HDSPM_SyncRef0;
		       break;
		case 4:
		       hdspm->control_register |= HDSPM_SyncRef2;
		       break;
		case 5:
		       hdspm->control_register |= HDSPM_SyncRef2+HDSPM_SyncRef0;
		       break;
		case 6:
		       hdspm->control_register |= HDSPM_SyncRef2+HDSPM_SyncRef1;
		       break;
		case 7:
		       hdspm->control_register |=
			       HDSPM_SyncRef2+HDSPM_SyncRef1+HDSPM_SyncRef0;
		       break;
		case 8:
		       hdspm->control_register |= HDSPM_SyncRef3;
		       break;
		default:
		       return -1;
		}
	} else {
		switch (pref) {
		case HDSPM_SYNC_FROM_MADI:
			hdspm->control_register |= HDSPM_SyncRef_MADI;
			break;
		case HDSPM_SYNC_FROM_WORD:
			hdspm->control_register |= HDSPM_SyncRef_Word;
			break;
		default:
			return -1;
		}
	}
	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);
	return 0;
}

static int snd_hdspm_info_pref_sync_ref(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_info *uinfo)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	if (hdspm->is_aes32) {
		static char *texts[] = { "Word", "AES1", "AES2", "AES3",
			"AES4", "AES5",	"AES6", "AES7", "AES8" };

		uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
		uinfo->count = 1;

		uinfo->value.enumerated.items = 9;

		if (uinfo->value.enumerated.item >=
		    uinfo->value.enumerated.items)
			uinfo->value.enumerated.item =
				uinfo->value.enumerated.items - 1;
		strcpy(uinfo->value.enumerated.name,
				texts[uinfo->value.enumerated.item]);
	} else {
		static char *texts[] = { "Word", "MADI" };

		uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
		uinfo->count = 1;

		uinfo->value.enumerated.items = 2;

		if (uinfo->value.enumerated.item >=
		    uinfo->value.enumerated.items)
			uinfo->value.enumerated.item =
				uinfo->value.enumerated.items - 1;
		strcpy(uinfo->value.enumerated.name,
				texts[uinfo->value.enumerated.item]);
	}
	return 0;
}

static int snd_hdspm_get_pref_sync_ref(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdspm_pref_sync_ref(hdspm);
	return 0;
}

static int snd_hdspm_put_pref_sync_ref(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int change, max;
	unsigned int val;

	max = hdspm->is_aes32 ? 9 : 2;

	if (!snd_hdspm_use_is_exclusive(hdspm))
		return -EBUSY;

	val = ucontrol->value.enumerated.item[0] % max;

	spin_lock_irq(&hdspm->lock);
	change = (int) val != hdspm_pref_sync_ref(hdspm);
	hdspm_set_pref_sync_ref(hdspm, val);
	spin_unlock_irq(&hdspm->lock);
	return change;
}

#define HDSPM_AUTOSYNC_REF(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ, \
  .info = snd_hdspm_info_autosync_ref, \
  .get = snd_hdspm_get_autosync_ref, \
}

static int hdspm_autosync_ref(struct hdspm * hdspm)
{
	if (hdspm->is_aes32) {
		unsigned int status = hdspm_read(hdspm, HDSPM_statusRegister);
		unsigned int syncref = (status >> HDSPM_AES32_syncref_bit) &
			0xF;
		if (syncref == 0)
			return HDSPM_AES32_AUTOSYNC_FROM_WORD;
		if (syncref <= 8)
			return syncref;
		return HDSPM_AES32_AUTOSYNC_FROM_NONE;
	} else {
		/* This looks at the autosync selected sync reference */
		unsigned int status2 = hdspm_read(hdspm, HDSPM_statusRegister2);

		switch (status2 & HDSPM_SelSyncRefMask) {
		case HDSPM_SelSyncRef_WORD:
			return HDSPM_AUTOSYNC_FROM_WORD;
		case HDSPM_SelSyncRef_MADI:
			return HDSPM_AUTOSYNC_FROM_MADI;
		case HDSPM_SelSyncRef_NVALID:
			return HDSPM_AUTOSYNC_FROM_NONE;
		default:
			return 0;
		}

		return 0;
	}
}

static int snd_hdspm_info_autosync_ref(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	if (hdspm->is_aes32) {
		static char *texts[] = { "WordClock", "AES1", "AES2", "AES3",
			"AES4",	"AES5", "AES6", "AES7", "AES8", "None"};

		uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
		uinfo->count = 1;
		uinfo->value.enumerated.items = 10;
		if (uinfo->value.enumerated.item >=
		    uinfo->value.enumerated.items)
			uinfo->value.enumerated.item =
				uinfo->value.enumerated.items - 1;
		strcpy(uinfo->value.enumerated.name,
				texts[uinfo->value.enumerated.item]);
	} else {
		static char *texts[] = { "WordClock", "MADI", "None" };

		uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
		uinfo->count = 1;
		uinfo->value.enumerated.items = 3;
		if (uinfo->value.enumerated.item >=
		    uinfo->value.enumerated.items)
			uinfo->value.enumerated.item =
				uinfo->value.enumerated.items - 1;
		strcpy(uinfo->value.enumerated.name,
				texts[uinfo->value.enumerated.item]);
	}
	return 0;
}

static int snd_hdspm_get_autosync_ref(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdspm_autosync_ref(hdspm);
	return 0;
}

#define HDSPM_LINE_OUT(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdspm_info_line_out, \
  .get = snd_hdspm_get_line_out, \
  .put = snd_hdspm_put_line_out \
}

static int hdspm_line_out(struct hdspm * hdspm)
{
	return (hdspm->control_register & HDSPM_LineOut) ? 1 : 0;
}


static int hdspm_set_line_output(struct hdspm * hdspm, int out)
{
	if (out)
		hdspm->control_register |= HDSPM_LineOut;
	else
		hdspm->control_register &= ~HDSPM_LineOut;
	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);

	return 0;
}

#define snd_hdspm_info_line_out		snd_ctl_boolean_mono_info

static int snd_hdspm_get_line_out(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&hdspm->lock);
	ucontrol->value.integer.value[0] = hdspm_line_out(hdspm);
	spin_unlock_irq(&hdspm->lock);
	return 0;
}

static int snd_hdspm_put_line_out(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdspm_use_is_exclusive(hdspm))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdspm->lock);
	change = (int) val != hdspm_line_out(hdspm);
	hdspm_set_line_output(hdspm, val);
	spin_unlock_irq(&hdspm->lock);
	return change;
}

#define HDSPM_TX_64(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdspm_info_tx_64, \
  .get = snd_hdspm_get_tx_64, \
  .put = snd_hdspm_put_tx_64 \
}

static int hdspm_tx_64(struct hdspm * hdspm)
{
	return (hdspm->control_register & HDSPM_TX_64ch) ? 1 : 0;
}

static int hdspm_set_tx_64(struct hdspm * hdspm, int out)
{
	if (out)
		hdspm->control_register |= HDSPM_TX_64ch;
	else
		hdspm->control_register &= ~HDSPM_TX_64ch;
	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);

	return 0;
}

#define snd_hdspm_info_tx_64		snd_ctl_boolean_mono_info

static int snd_hdspm_get_tx_64(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&hdspm->lock);
	ucontrol->value.integer.value[0] = hdspm_tx_64(hdspm);
	spin_unlock_irq(&hdspm->lock);
	return 0;
}

static int snd_hdspm_put_tx_64(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdspm_use_is_exclusive(hdspm))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdspm->lock);
	change = (int) val != hdspm_tx_64(hdspm);
	hdspm_set_tx_64(hdspm, val);
	spin_unlock_irq(&hdspm->lock);
	return change;
}

#define HDSPM_C_TMS(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdspm_info_c_tms, \
  .get = snd_hdspm_get_c_tms, \
  .put = snd_hdspm_put_c_tms \
}

static int hdspm_c_tms(struct hdspm * hdspm)
{
	return (hdspm->control_register & HDSPM_clr_tms) ? 1 : 0;
}

static int hdspm_set_c_tms(struct hdspm * hdspm, int out)
{
	if (out)
		hdspm->control_register |= HDSPM_clr_tms;
	else
		hdspm->control_register &= ~HDSPM_clr_tms;
	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);

	return 0;
}

#define snd_hdspm_info_c_tms		snd_ctl_boolean_mono_info

static int snd_hdspm_get_c_tms(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&hdspm->lock);
	ucontrol->value.integer.value[0] = hdspm_c_tms(hdspm);
	spin_unlock_irq(&hdspm->lock);
	return 0;
}

static int snd_hdspm_put_c_tms(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdspm_use_is_exclusive(hdspm))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdspm->lock);
	change = (int) val != hdspm_c_tms(hdspm);
	hdspm_set_c_tms(hdspm, val);
	spin_unlock_irq(&hdspm->lock);
	return change;
}

#define HDSPM_SAFE_MODE(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdspm_info_safe_mode, \
  .get = snd_hdspm_get_safe_mode, \
  .put = snd_hdspm_put_safe_mode \
}

static int hdspm_safe_mode(struct hdspm * hdspm)
{
	return (hdspm->control_register & HDSPM_AutoInp) ? 1 : 0;
}

static int hdspm_set_safe_mode(struct hdspm * hdspm, int out)
{
	if (out)
		hdspm->control_register |= HDSPM_AutoInp;
	else
		hdspm->control_register &= ~HDSPM_AutoInp;
	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);

	return 0;
}

#define snd_hdspm_info_safe_mode	snd_ctl_boolean_mono_info

static int snd_hdspm_get_safe_mode(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&hdspm->lock);
	ucontrol->value.integer.value[0] = hdspm_safe_mode(hdspm);
	spin_unlock_irq(&hdspm->lock);
	return 0;
}

static int snd_hdspm_put_safe_mode(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdspm_use_is_exclusive(hdspm))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdspm->lock);
	change = (int) val != hdspm_safe_mode(hdspm);
	hdspm_set_safe_mode(hdspm, val);
	spin_unlock_irq(&hdspm->lock);
	return change;
}

#define HDSPM_EMPHASIS(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdspm_info_emphasis, \
  .get = snd_hdspm_get_emphasis, \
  .put = snd_hdspm_put_emphasis \
}

static int hdspm_emphasis(struct hdspm * hdspm)
{
	return (hdspm->control_register & HDSPM_Emphasis) ? 1 : 0;
}

static int hdspm_set_emphasis(struct hdspm * hdspm, int emp)
{
	if (emp)
		hdspm->control_register |= HDSPM_Emphasis;
	else
		hdspm->control_register &= ~HDSPM_Emphasis;
	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);

	return 0;
}

#define snd_hdspm_info_emphasis		snd_ctl_boolean_mono_info

static int snd_hdspm_get_emphasis(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&hdspm->lock);
	ucontrol->value.enumerated.item[0] = hdspm_emphasis(hdspm);
	spin_unlock_irq(&hdspm->lock);
	return 0;
}

static int snd_hdspm_put_emphasis(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdspm_use_is_exclusive(hdspm))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdspm->lock);
	change = (int) val != hdspm_emphasis(hdspm);
	hdspm_set_emphasis(hdspm, val);
	spin_unlock_irq(&hdspm->lock);
	return change;
}

#define HDSPM_DOLBY(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdspm_info_dolby, \
  .get = snd_hdspm_get_dolby, \
  .put = snd_hdspm_put_dolby \
}

static int hdspm_dolby(struct hdspm * hdspm)
{
	return (hdspm->control_register & HDSPM_Dolby) ? 1 : 0;
}

static int hdspm_set_dolby(struct hdspm * hdspm, int dol)
{
	if (dol)
		hdspm->control_register |= HDSPM_Dolby;
	else
		hdspm->control_register &= ~HDSPM_Dolby;
	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);

	return 0;
}

#define snd_hdspm_info_dolby		snd_ctl_boolean_mono_info

static int snd_hdspm_get_dolby(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&hdspm->lock);
	ucontrol->value.enumerated.item[0] = hdspm_dolby(hdspm);
	spin_unlock_irq(&hdspm->lock);
	return 0;
}

static int snd_hdspm_put_dolby(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdspm_use_is_exclusive(hdspm))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdspm->lock);
	change = (int) val != hdspm_dolby(hdspm);
	hdspm_set_dolby(hdspm, val);
	spin_unlock_irq(&hdspm->lock);
	return change;
}

#define HDSPM_PROFESSIONAL(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdspm_info_professional, \
  .get = snd_hdspm_get_professional, \
  .put = snd_hdspm_put_professional \
}

static int hdspm_professional(struct hdspm * hdspm)
{
	return (hdspm->control_register & HDSPM_Professional) ? 1 : 0;
}

static int hdspm_set_professional(struct hdspm * hdspm, int dol)
{
	if (dol)
		hdspm->control_register |= HDSPM_Professional;
	else
		hdspm->control_register &= ~HDSPM_Professional;
	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);

	return 0;
}

#define snd_hdspm_info_professional	snd_ctl_boolean_mono_info

static int snd_hdspm_get_professional(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&hdspm->lock);
	ucontrol->value.enumerated.item[0] = hdspm_professional(hdspm);
	spin_unlock_irq(&hdspm->lock);
	return 0;
}

static int snd_hdspm_put_professional(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdspm_use_is_exclusive(hdspm))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdspm->lock);
	change = (int) val != hdspm_professional(hdspm);
	hdspm_set_professional(hdspm, val);
	spin_unlock_irq(&hdspm->lock);
	return change;
}

#define HDSPM_INPUT_SELECT(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdspm_info_input_select, \
  .get = snd_hdspm_get_input_select, \
  .put = snd_hdspm_put_input_select \
}

static int hdspm_input_select(struct hdspm * hdspm)
{
	return (hdspm->control_register & HDSPM_InputSelect0) ? 1 : 0;
}

static int hdspm_set_input_select(struct hdspm * hdspm, int out)
{
	if (out)
		hdspm->control_register |= HDSPM_InputSelect0;
	else
		hdspm->control_register &= ~HDSPM_InputSelect0;
	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);

	return 0;
}

static int snd_hdspm_info_input_select(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = { "optical", "coaxial" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 2;

	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item =
		    uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name,
	       texts[uinfo->value.enumerated.item]);

	return 0;
}

static int snd_hdspm_get_input_select(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&hdspm->lock);
	ucontrol->value.enumerated.item[0] = hdspm_input_select(hdspm);
	spin_unlock_irq(&hdspm->lock);
	return 0;
}

static int snd_hdspm_put_input_select(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdspm_use_is_exclusive(hdspm))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdspm->lock);
	change = (int) val != hdspm_input_select(hdspm);
	hdspm_set_input_select(hdspm, val);
	spin_unlock_irq(&hdspm->lock);
	return change;
}

#define HDSPM_DS_WIRE(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdspm_info_ds_wire, \
  .get = snd_hdspm_get_ds_wire, \
  .put = snd_hdspm_put_ds_wire \
}

static int hdspm_ds_wire(struct hdspm * hdspm)
{
	return (hdspm->control_register & HDSPM_DS_DoubleWire) ? 1 : 0;
}

static int hdspm_set_ds_wire(struct hdspm * hdspm, int ds)
{
	if (ds)
		hdspm->control_register |= HDSPM_DS_DoubleWire;
	else
		hdspm->control_register &= ~HDSPM_DS_DoubleWire;
	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);

	return 0;
}

static int snd_hdspm_info_ds_wire(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = { "Single", "Double" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 2;

	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item =
		    uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name,
	       texts[uinfo->value.enumerated.item]);

	return 0;
}

static int snd_hdspm_get_ds_wire(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&hdspm->lock);
	ucontrol->value.enumerated.item[0] = hdspm_ds_wire(hdspm);
	spin_unlock_irq(&hdspm->lock);
	return 0;
}

static int snd_hdspm_put_ds_wire(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdspm_use_is_exclusive(hdspm))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdspm->lock);
	change = (int) val != hdspm_ds_wire(hdspm);
	hdspm_set_ds_wire(hdspm, val);
	spin_unlock_irq(&hdspm->lock);
	return change;
}

#define HDSPM_QS_WIRE(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdspm_info_qs_wire, \
  .get = snd_hdspm_get_qs_wire, \
  .put = snd_hdspm_put_qs_wire \
}

static int hdspm_qs_wire(struct hdspm * hdspm)
{
	if (hdspm->control_register & HDSPM_QS_DoubleWire)
		return 1;
	if (hdspm->control_register & HDSPM_QS_QuadWire)
		return 2;
	return 0;
}

static int hdspm_set_qs_wire(struct hdspm * hdspm, int mode)
{
	hdspm->control_register &= ~(HDSPM_QS_DoubleWire | HDSPM_QS_QuadWire);
	switch (mode) {
	case 0:
		break;
	case 1:
		hdspm->control_register |= HDSPM_QS_DoubleWire;
		break;
	case 2:
		hdspm->control_register |= HDSPM_QS_QuadWire;
		break;
	}
	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);

	return 0;
}

static int snd_hdspm_info_qs_wire(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = { "Single", "Double", "Quad" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 3;

	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item =
		    uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name,
	       texts[uinfo->value.enumerated.item]);

	return 0;
}

static int snd_hdspm_get_qs_wire(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&hdspm->lock);
	ucontrol->value.enumerated.item[0] = hdspm_qs_wire(hdspm);
	spin_unlock_irq(&hdspm->lock);
	return 0;
}

static int snd_hdspm_put_qs_wire(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int change;
	int val;

	if (!snd_hdspm_use_is_exclusive(hdspm))
		return -EBUSY;
	val = ucontrol->value.integer.value[0];
	if (val < 0)
		val = 0;
	if (val > 2)
		val = 2;
	spin_lock_irq(&hdspm->lock);
	change = val != hdspm_qs_wire(hdspm);
	hdspm_set_qs_wire(hdspm, val);
	spin_unlock_irq(&hdspm->lock);
	return change;
}

/*           Simple Mixer
  deprecated since to much faders ???
  MIXER interface says output (source, destination, value)
   where source > MAX_channels are playback channels 
   on MADICARD 
  - playback mixer matrix: [channelout+64] [output] [value]
  - input(thru) mixer matrix: [channelin] [output] [value]
  (better do 2 kontrols for seperation ?)
*/

#define HDSPM_MIXER(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_HWDEP, \
  .name = xname, \
  .index = xindex, \
  .device = 0, \
  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE | \
		 SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
  .info = snd_hdspm_info_mixer, \
  .get = snd_hdspm_get_mixer, \
  .put = snd_hdspm_put_mixer \
}

static int snd_hdspm_info_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 3;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 65535;
	uinfo->value.integer.step = 1;
	return 0;
}

static int snd_hdspm_get_mixer(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int source;
	int destination;

	source = ucontrol->value.integer.value[0];
	if (source < 0)
		source = 0;
	else if (source >= 2 * HDSPM_MAX_CHANNELS)
		source = 2 * HDSPM_MAX_CHANNELS - 1;

	destination = ucontrol->value.integer.value[1];
	if (destination < 0)
		destination = 0;
	else if (destination >= HDSPM_MAX_CHANNELS)
		destination = HDSPM_MAX_CHANNELS - 1;

	spin_lock_irq(&hdspm->lock);
	if (source >= HDSPM_MAX_CHANNELS)
		ucontrol->value.integer.value[2] =
		    hdspm_read_pb_gain(hdspm, destination,
				       source - HDSPM_MAX_CHANNELS);
	else
		ucontrol->value.integer.value[2] =
		    hdspm_read_in_gain(hdspm, destination, source);

	spin_unlock_irq(&hdspm->lock);

	return 0;
}

static int snd_hdspm_put_mixer(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int change;
	int source;
	int destination;
	int gain;

	if (!snd_hdspm_use_is_exclusive(hdspm))
		return -EBUSY;

	source = ucontrol->value.integer.value[0];
	destination = ucontrol->value.integer.value[1];

	if (source < 0 || source >= 2 * HDSPM_MAX_CHANNELS)
		return -1;
	if (destination < 0 || destination >= HDSPM_MAX_CHANNELS)
		return -1;

	gain = ucontrol->value.integer.value[2];

	spin_lock_irq(&hdspm->lock);

	if (source >= HDSPM_MAX_CHANNELS)
		change = gain != hdspm_read_pb_gain(hdspm, destination,
						    source -
						    HDSPM_MAX_CHANNELS);
	else
		change = gain != hdspm_read_in_gain(hdspm, destination,
						    source);

	if (change) {
		if (source >= HDSPM_MAX_CHANNELS)
			hdspm_write_pb_gain(hdspm, destination,
					    source - HDSPM_MAX_CHANNELS,
					    gain);
		else
			hdspm_write_in_gain(hdspm, destination, source,
					    gain);
	}
	spin_unlock_irq(&hdspm->lock);

	return change;
}

/* The simple mixer control(s) provide gain control for the
   basic 1:1 mappings of playback streams to output
   streams. 
*/

#define HDSPM_PLAYBACK_MIXER \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ | SNDRV_CTL_ELEM_ACCESS_WRITE | \
		 SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
  .info = snd_hdspm_info_playback_mixer, \
  .get = snd_hdspm_get_playback_mixer, \
  .put = snd_hdspm_put_playback_mixer \
}

static int snd_hdspm_info_playback_mixer(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 65536;
	uinfo->value.integer.step = 1;
	return 0;
}

static int snd_hdspm_get_playback_mixer(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int channel;
	int mapped_channel;

	channel = ucontrol->id.index - 1;

	if (snd_BUG_ON(channel < 0 || channel >= HDSPM_MAX_CHANNELS))
		return -EINVAL;

	mapped_channel = hdspm->channel_map[channel];
	if (mapped_channel < 0)
		return -EINVAL;

	spin_lock_irq(&hdspm->lock);
	ucontrol->value.integer.value[0] =
	    hdspm_read_pb_gain(hdspm, mapped_channel, mapped_channel);
	spin_unlock_irq(&hdspm->lock);

	/*
	snd_printdd("get pb mixer index %d, channel %d, mapped_channel %d, "
		    "value %d\n",
		    ucontrol->id.index, channel, mapped_channel,
		    ucontrol->value.integer.value[0]); 
	*/
	return 0;
}

static int snd_hdspm_put_playback_mixer(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);
	int change;
	int channel;
	int mapped_channel;
	int gain;

	if (!snd_hdspm_use_is_exclusive(hdspm))
		return -EBUSY;

	channel = ucontrol->id.index - 1;

	if (snd_BUG_ON(channel < 0 || channel >= HDSPM_MAX_CHANNELS))
		return -EINVAL;

	mapped_channel = hdspm->channel_map[channel];
	if (mapped_channel < 0)
		return -EINVAL;

	gain = ucontrol->value.integer.value[0];

	spin_lock_irq(&hdspm->lock);
	change =
	    gain != hdspm_read_pb_gain(hdspm, mapped_channel,
				       mapped_channel);
	if (change)
		hdspm_write_pb_gain(hdspm, mapped_channel, mapped_channel,
				    gain);
	spin_unlock_irq(&hdspm->lock);
	return change;
}

#define HDSPM_WC_SYNC_CHECK(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ | SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
  .info = snd_hdspm_info_sync_check, \
  .get = snd_hdspm_get_wc_sync_check \
}

static int snd_hdspm_info_sync_check(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = { "No Lock", "Lock", "Sync" };
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 3;
	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item =
		    uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name,
	       texts[uinfo->value.enumerated.item]);
	return 0;
}

static int hdspm_wc_sync_check(struct hdspm * hdspm)
{
	if (hdspm->is_aes32) {
		int status = hdspm_read(hdspm, HDSPM_statusRegister);
		if (status & HDSPM_AES32_wcLock) {
			/* I don't know how to differenciate sync from lock.
			   Doing as if sync for now */
			return 2;
		}
		return 0;
	} else {
		int status2 = hdspm_read(hdspm, HDSPM_statusRegister2);
		if (status2 & HDSPM_wcLock) {
			if (status2 & HDSPM_wcSync)
				return 2;
			else
				return 1;
		}
		return 0;
	}
}

static int snd_hdspm_get_wc_sync_check(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdspm_wc_sync_check(hdspm);
	return 0;
}


#define HDSPM_MADI_SYNC_CHECK(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ | SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
  .info = snd_hdspm_info_sync_check, \
  .get = snd_hdspm_get_madisync_sync_check \
}

static int hdspm_madisync_sync_check(struct hdspm * hdspm)
{
	int status = hdspm_read(hdspm, HDSPM_statusRegister);
	if (status & HDSPM_madiLock) {
		if (status & HDSPM_madiSync)
			return 2;
		else
			return 1;
	}
	return 0;
}

static int snd_hdspm_get_madisync_sync_check(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *
					     ucontrol)
{
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] =
	    hdspm_madisync_sync_check(hdspm);
	return 0;
}


#define HDSPM_AES_SYNC_CHECK(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ | SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
  .info = snd_hdspm_info_sync_check, \
  .get = snd_hdspm_get_aes_sync_check \
}

static int hdspm_aes_sync_check(struct hdspm * hdspm, int idx)
{
	int status2 = hdspm_read(hdspm, HDSPM_statusRegister2);
	if (status2 & (HDSPM_LockAES >> idx)) {
		/* I don't know how to differenciate sync from lock.
		   Doing as if sync for now */
		return 2;
	}
	return 0;
}

static int snd_hdspm_get_aes_sync_check(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int offset;
	struct hdspm *hdspm = snd_kcontrol_chip(kcontrol);

	offset = ucontrol->id.index - 1;
	if (offset < 0 || offset >= 8)
		return -EINVAL;

	ucontrol->value.enumerated.item[0] =
		hdspm_aes_sync_check(hdspm, offset);
	return 0;
}


static struct snd_kcontrol_new snd_hdspm_controls_madi[] = {

	HDSPM_MIXER("Mixer", 0),
/* 'Sample Clock Source' complies with the alsa control naming scheme */
	HDSPM_CLOCK_SOURCE("Sample Clock Source", 0),

	HDSPM_SYSTEM_CLOCK_MODE("System Clock Mode", 0),
	HDSPM_PREF_SYNC_REF("Preferred Sync Reference", 0),
	HDSPM_AUTOSYNC_REF("AutoSync Reference", 0),
	HDSPM_SYSTEM_SAMPLE_RATE("System Sample Rate", 0),
/* 'External Rate' complies with the alsa control naming scheme */
	HDSPM_AUTOSYNC_SAMPLE_RATE("External Rate", 0),
	HDSPM_WC_SYNC_CHECK("Word Clock Lock Status", 0),
	HDSPM_MADI_SYNC_CHECK("MADI Sync Lock Status", 0),
	HDSPM_LINE_OUT("Line Out", 0),
	HDSPM_TX_64("TX 64 channels mode", 0),
	HDSPM_C_TMS("Clear Track Marker", 0),
	HDSPM_SAFE_MODE("Safe Mode", 0),
	HDSPM_INPUT_SELECT("Input Select", 0),
};

static struct snd_kcontrol_new snd_hdspm_controls_aes32[] = {

	HDSPM_MIXER("Mixer", 0),
/* 'Sample Clock Source' complies with the alsa control naming scheme */
	HDSPM_CLOCK_SOURCE("Sample Clock Source", 0),

	HDSPM_SYSTEM_CLOCK_MODE("System Clock Mode", 0),
	HDSPM_PREF_SYNC_REF("Preferred Sync Reference", 0),
	HDSPM_AUTOSYNC_REF("AutoSync Reference", 0),
	HDSPM_SYSTEM_SAMPLE_RATE("System Sample Rate", 0),
/* 'External Rate' complies with the alsa control naming scheme */
	HDSPM_AUTOSYNC_SAMPLE_RATE("External Rate", 0),
	HDSPM_WC_SYNC_CHECK("Word Clock Lock Status", 0),
/*	HDSPM_AES_SYNC_CHECK("AES Lock Status", 0),*/ /* created in snd_hdspm_create_controls() */
	HDSPM_LINE_OUT("Line Out", 0),
	HDSPM_EMPHASIS("Emphasis", 0),
	HDSPM_DOLBY("Non Audio", 0),
	HDSPM_PROFESSIONAL("Professional", 0),
	HDSPM_C_TMS("Clear Track Marker", 0),
	HDSPM_DS_WIRE("Double Speed Wire Mode", 0),
	HDSPM_QS_WIRE("Quad Speed Wire Mode", 0),
};

static struct snd_kcontrol_new snd_hdspm_playback_mixer = HDSPM_PLAYBACK_MIXER;


static int hdspm_update_simple_mixer_controls(struct hdspm * hdspm)
{
	int i;

	for (i = hdspm->ds_channels; i < hdspm->ss_channels; ++i) {
		if (hdspm->system_sample_rate > 48000) {
			hdspm->playback_mixer_ctls[i]->vd[0].access =
			    SNDRV_CTL_ELEM_ACCESS_INACTIVE |
			    SNDRV_CTL_ELEM_ACCESS_READ |
			    SNDRV_CTL_ELEM_ACCESS_VOLATILE;
		} else {
			hdspm->playback_mixer_ctls[i]->vd[0].access =
			    SNDRV_CTL_ELEM_ACCESS_READWRITE |
			    SNDRV_CTL_ELEM_ACCESS_VOLATILE;
		}
		snd_ctl_notify(hdspm->card, SNDRV_CTL_EVENT_MASK_VALUE |
			       SNDRV_CTL_EVENT_MASK_INFO,
			       &hdspm->playback_mixer_ctls[i]->id);
	}

	return 0;
}


static int snd_hdspm_create_controls(struct snd_card *card, struct hdspm * hdspm)
{
	unsigned int idx, limit;
	int err;
	struct snd_kcontrol *kctl;

	/* add control list first */
	if (hdspm->is_aes32) {
		struct snd_kcontrol_new aes_sync_ctl =
			HDSPM_AES_SYNC_CHECK("AES Lock Status", 0);

		for (idx = 0; idx < ARRAY_SIZE(snd_hdspm_controls_aes32);
		     idx++) {
			err = snd_ctl_add(card,
					  snd_ctl_new1(&snd_hdspm_controls_aes32[idx],
						       hdspm));
			if (err < 0)
				return err;
		}
		for (idx = 1; idx <= 8; idx++) {
			aes_sync_ctl.index = idx;
			err = snd_ctl_add(card,
					  snd_ctl_new1(&aes_sync_ctl, hdspm));
			if (err < 0)
				return err;
		}
	} else {
		for (idx = 0; idx < ARRAY_SIZE(snd_hdspm_controls_madi);
		     idx++) {
			err = snd_ctl_add(card,
					  snd_ctl_new1(&snd_hdspm_controls_madi[idx],
						       hdspm));
			if (err < 0)
				return err;
		}
	}

	/* Channel playback mixer as default control 
	   Note: the whole matrix would be 128*HDSPM_MIXER_CHANNELS Faders,
	   thats too * big for any alsamixer they are accesible via special
	   IOCTL on hwdep and the mixer 2dimensional mixer control
	*/

	snd_hdspm_playback_mixer.name = "Chn";
	limit = HDSPM_MAX_CHANNELS;

	/* The index values are one greater than the channel ID so that
	 * alsamixer will display them correctly. We want to use the index
	 * for fast lookup of the relevant channel, but if we use it at all,
	 * most ALSA software does the wrong thing with it ...
	 */

	for (idx = 0; idx < limit; ++idx) {
		snd_hdspm_playback_mixer.index = idx + 1;
		kctl = snd_ctl_new1(&snd_hdspm_playback_mixer, hdspm);
		err = snd_ctl_add(card, kctl);
		if (err < 0)
			return err;
		hdspm->playback_mixer_ctls[idx] = kctl;
	}

	return 0;
}

/*------------------------------------------------------------
   /proc interface 
 ------------------------------------------------------------*/

static void
snd_hdspm_proc_read_madi(struct snd_info_entry * entry,
			 struct snd_info_buffer *buffer)
{
	struct hdspm *hdspm = entry->private_data;
	unsigned int status;
	unsigned int status2;
	char *pref_sync_ref;
	char *autosync_ref;
	char *system_clock_mode;
	char *clock_source;
	char *insel;
	char *syncref;
	int x, x2;

	status = hdspm_read(hdspm, HDSPM_statusRegister);
	status2 = hdspm_read(hdspm, HDSPM_statusRegister2);

	snd_iprintf(buffer, "%s (Card #%d) Rev.%x Status2first3bits: %x\n",
		    hdspm->card_name, hdspm->card->number + 1,
		    hdspm->firmware_rev,
		    (status2 & HDSPM_version0) |
		    (status2 & HDSPM_version1) | (status2 &
						  HDSPM_version2));

	snd_iprintf(buffer, "IRQ: %d Registers bus: 0x%lx VM: 0x%lx\n",
		    hdspm->irq, hdspm->port, (unsigned long)hdspm->iobase);

	snd_iprintf(buffer, "--- System ---\n");

	snd_iprintf(buffer,
		    "IRQ Pending: Audio=%d, MIDI0=%d, MIDI1=%d, IRQcount=%d\n",
		    status & HDSPM_audioIRQPending,
		    (status & HDSPM_midi0IRQPending) ? 1 : 0,
		    (status & HDSPM_midi1IRQPending) ? 1 : 0,
		    hdspm->irq_count);
	snd_iprintf(buffer,
		    "HW pointer: id = %d, rawptr = %d (%d->%d) "
		    "estimated= %ld (bytes)\n",
		    ((status & HDSPM_BufferID) ? 1 : 0),
		    (status & HDSPM_BufferPositionMask),
		    (status & HDSPM_BufferPositionMask) %
		    (2 * (int)hdspm->period_bytes),
		    ((status & HDSPM_BufferPositionMask) - 64) %
		    (2 * (int)hdspm->period_bytes),
		    (long) hdspm_hw_pointer(hdspm) * 4);

	snd_iprintf(buffer,
		    "MIDI FIFO: Out1=0x%x, Out2=0x%x, In1=0x%x, In2=0x%x \n",
		    hdspm_read(hdspm, HDSPM_midiStatusOut0) & 0xFF,
		    hdspm_read(hdspm, HDSPM_midiStatusOut1) & 0xFF,
		    hdspm_read(hdspm, HDSPM_midiStatusIn0) & 0xFF,
		    hdspm_read(hdspm, HDSPM_midiStatusIn1) & 0xFF);
	snd_iprintf(buffer,
		    "Register: ctrl1=0x%x, ctrl2=0x%x, status1=0x%x, "
		    "status2=0x%x\n",
		    hdspm->control_register, hdspm->control2_register,
		    status, status2);

	snd_iprintf(buffer, "--- Settings ---\n");

	x = 1 << (6 + hdspm_decode_latency(hdspm->control_register &
					   HDSPM_LatencyMask));

	snd_iprintf(buffer,
		    "Size (Latency): %d samples (2 periods of %lu bytes)\n",
		    x, (unsigned long) hdspm->period_bytes);

	snd_iprintf(buffer, "Line out: %s,   Precise Pointer: %s\n",
		    (hdspm->control_register & HDSPM_LineOut) ? "on " : "off",
		    (hdspm->precise_ptr) ? "on" : "off");

	switch (hdspm->control_register & HDSPM_InputMask) {
	case HDSPM_InputOptical:
		insel = "Optical";
		break;
	case HDSPM_InputCoaxial:
		insel = "Coaxial";
		break;
	default:
		insel = "Unkown";
	}

	switch (hdspm->control_register & HDSPM_SyncRefMask) {
	case HDSPM_SyncRef_Word:
		syncref = "WordClock";
		break;
	case HDSPM_SyncRef_MADI:
		syncref = "MADI";
		break;
	default:
		syncref = "Unkown";
	}
	snd_iprintf(buffer, "Inputsel = %s, SyncRef = %s\n", insel,
		    syncref);

	snd_iprintf(buffer,
		    "ClearTrackMarker = %s, Transmit in %s Channel Mode, "
		    "Auto Input %s\n",
		    (hdspm->
		     control_register & HDSPM_clr_tms) ? "on" : "off",
		    (hdspm->
		     control_register & HDSPM_TX_64ch) ? "64" : "56",
		    (hdspm->
		     control_register & HDSPM_AutoInp) ? "on" : "off");

	switch (hdspm_clock_source(hdspm)) {
	case HDSPM_CLOCK_SOURCE_AUTOSYNC:
		clock_source = "AutoSync";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_32KHZ:
		clock_source = "Internal 32 kHz";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_44_1KHZ:
		clock_source = "Internal 44.1 kHz";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_48KHZ:
		clock_source = "Internal 48 kHz";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_64KHZ:
		clock_source = "Internal 64 kHz";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_88_2KHZ:
		clock_source = "Internal 88.2 kHz";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_96KHZ:
		clock_source = "Internal 96 kHz";
		break;
	default:
		clock_source = "Error";
	}
	snd_iprintf(buffer, "Sample Clock Source: %s\n", clock_source);
	if (!(hdspm->control_register & HDSPM_ClockModeMaster))
		system_clock_mode = "Slave";
	else
		system_clock_mode = "Master";
	snd_iprintf(buffer, "System Clock Mode: %s\n", system_clock_mode);

	switch (hdspm_pref_sync_ref(hdspm)) {
	case HDSPM_SYNC_FROM_WORD:
		pref_sync_ref = "Word Clock";
		break;
	case HDSPM_SYNC_FROM_MADI:
		pref_sync_ref = "MADI Sync";
		break;
	default:
		pref_sync_ref = "XXXX Clock";
		break;
	}
	snd_iprintf(buffer, "Preferred Sync Reference: %s\n",
		    pref_sync_ref);

	snd_iprintf(buffer, "System Clock Frequency: %d\n",
		    hdspm->system_sample_rate);


	snd_iprintf(buffer, "--- Status:\n");

	x = status & HDSPM_madiSync;
	x2 = status2 & HDSPM_wcSync;

	snd_iprintf(buffer, "Inputs MADI=%s, WordClock=%s\n",
		    (status & HDSPM_madiLock) ? (x ? "Sync" : "Lock") :
		    "NoLock",
		    (status2 & HDSPM_wcLock) ? (x2 ? "Sync" : "Lock") :
		    "NoLock");

	switch (hdspm_autosync_ref(hdspm)) {
	case HDSPM_AUTOSYNC_FROM_WORD:
		autosync_ref = "Word Clock";
		break;
	case HDSPM_AUTOSYNC_FROM_MADI:
		autosync_ref = "MADI Sync";
		break;
	case HDSPM_AUTOSYNC_FROM_NONE:
		autosync_ref = "Input not valid";
		break;
	default:
		autosync_ref = "---";
		break;
	}
	snd_iprintf(buffer,
		    "AutoSync: Reference= %s, Freq=%d (MADI = %d, Word = %d)\n",
		    autosync_ref, hdspm_external_sample_rate(hdspm),
		    (status & HDSPM_madiFreqMask) >> 22,
		    (status2 & HDSPM_wcFreqMask) >> 5);

	snd_iprintf(buffer, "Input: %s, Mode=%s\n",
		    (status & HDSPM_AB_int) ? "Coax" : "Optical",
		    (status & HDSPM_RX_64ch) ? "64 channels" :
		    "56 channels");

	snd_iprintf(buffer, "\n");
}

static void
snd_hdspm_proc_read_aes32(struct snd_info_entry * entry,
			  struct snd_info_buffer *buffer)
{
	struct hdspm *hdspm = entry->private_data;
	unsigned int status;
	unsigned int status2;
	unsigned int timecode;
	int pref_syncref;
	char *autosync_ref;
	char *system_clock_mode;
	char *clock_source;
	int x;

	status = hdspm_read(hdspm, HDSPM_statusRegister);
	status2 = hdspm_read(hdspm, HDSPM_statusRegister2);
	timecode = hdspm_read(hdspm, HDSPM_timecodeRegister);

	snd_iprintf(buffer, "%s (Card #%d) Rev.%x\n",
		    hdspm->card_name, hdspm->card->number + 1,
		    hdspm->firmware_rev);

	snd_iprintf(buffer, "IRQ: %d Registers bus: 0x%lx VM: 0x%lx\n",
		    hdspm->irq, hdspm->port, (unsigned long)hdspm->iobase);

	snd_iprintf(buffer, "--- System ---\n");

	snd_iprintf(buffer,
		    "IRQ Pending: Audio=%d, MIDI0=%d, MIDI1=%d, IRQcount=%d\n",
		    status & HDSPM_audioIRQPending,
		    (status & HDSPM_midi0IRQPending) ? 1 : 0,
		    (status & HDSPM_midi1IRQPending) ? 1 : 0,
		    hdspm->irq_count);
	snd_iprintf(buffer,
		    "HW pointer: id = %d, rawptr = %d (%d->%d) "
		    "estimated= %ld (bytes)\n",
		    ((status & HDSPM_BufferID) ? 1 : 0),
		    (status & HDSPM_BufferPositionMask),
		    (status & HDSPM_BufferPositionMask) %
		    (2 * (int)hdspm->period_bytes),
		    ((status & HDSPM_BufferPositionMask) - 64) %
		    (2 * (int)hdspm->period_bytes),
		    (long) hdspm_hw_pointer(hdspm) * 4);

	snd_iprintf(buffer,
		    "MIDI FIFO: Out1=0x%x, Out2=0x%x, In1=0x%x, In2=0x%x \n",
		    hdspm_read(hdspm, HDSPM_midiStatusOut0) & 0xFF,
		    hdspm_read(hdspm, HDSPM_midiStatusOut1) & 0xFF,
		    hdspm_read(hdspm, HDSPM_midiStatusIn0) & 0xFF,
		    hdspm_read(hdspm, HDSPM_midiStatusIn1) & 0xFF);
	snd_iprintf(buffer,
		    "Register: ctrl1=0x%x, status1=0x%x, status2=0x%x, "
		    "timecode=0x%x\n",
		    hdspm->control_register,
		    status, status2, timecode);

	snd_iprintf(buffer, "--- Settings ---\n");

	x = 1 << (6 + hdspm_decode_latency(hdspm->control_register &
					   HDSPM_LatencyMask));

	snd_iprintf(buffer,
		    "Size (Latency): %d samples (2 periods of %lu bytes)\n",
		    x, (unsigned long) hdspm->period_bytes);

	snd_iprintf(buffer, "Line out: %s,   Precise Pointer: %s\n",
		    (hdspm->
		     control_register & HDSPM_LineOut) ? "on " : "off",
		    (hdspm->precise_ptr) ? "on" : "off");

	snd_iprintf(buffer,
		    "ClearTrackMarker %s, Emphasis %s, Dolby %s\n",
		    (hdspm->
		     control_register & HDSPM_clr_tms) ? "on" : "off",
		    (hdspm->
		     control_register & HDSPM_Emphasis) ? "on" : "off",
		    (hdspm->
		     control_register & HDSPM_Dolby) ? "on" : "off");

	switch (hdspm_clock_source(hdspm)) {
	case HDSPM_CLOCK_SOURCE_AUTOSYNC:
		clock_source = "AutoSync";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_32KHZ:
		clock_source = "Internal 32 kHz";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_44_1KHZ:
		clock_source = "Internal 44.1 kHz";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_48KHZ:
		clock_source = "Internal 48 kHz";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_64KHZ:
		clock_source = "Internal 64 kHz";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_88_2KHZ:
		clock_source = "Internal 88.2 kHz";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_96KHZ:
		clock_source = "Internal 96 kHz";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_128KHZ:
		clock_source = "Internal 128 kHz";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_176_4KHZ:
		clock_source = "Internal 176.4 kHz";
		break;
	case HDSPM_CLOCK_SOURCE_INTERNAL_192KHZ:
		clock_source = "Internal 192 kHz";
		break;
	default:
		clock_source = "Error";
	}
	snd_iprintf(buffer, "Sample Clock Source: %s\n", clock_source);
	if (!(hdspm->control_register & HDSPM_ClockModeMaster))
		system_clock_mode = "Slave";
	else
		system_clock_mode = "Master";
	snd_iprintf(buffer, "System Clock Mode: %s\n", system_clock_mode);

	pref_syncref = hdspm_pref_sync_ref(hdspm);
	if (pref_syncref == 0)
		snd_iprintf(buffer, "Preferred Sync Reference: Word Clock\n");
	else
		snd_iprintf(buffer, "Preferred Sync Reference: AES%d\n",
				pref_syncref);

	snd_iprintf(buffer, "System Clock Frequency: %d\n",
		    hdspm->system_sample_rate);

	snd_iprintf(buffer, "Double speed: %s\n",
			hdspm->control_register & HDSPM_DS_DoubleWire?
			"Double wire" : "Single wire");
	snd_iprintf(buffer, "Quad speed: %s\n",
			hdspm->control_register & HDSPM_QS_DoubleWire?
			"Double wire" :
			hdspm->control_register & HDSPM_QS_QuadWire?
			"Quad wire" : "Single wire");

	snd_iprintf(buffer, "--- Status:\n");

	snd_iprintf(buffer, "Word: %s  Frequency: %d\n",
		    (status & HDSPM_AES32_wcLock)? "Sync   " : "No Lock",
		    HDSPM_bit2freq((status >> HDSPM_AES32_wcFreq_bit) & 0xF));

	for (x = 0; x < 8; x++) {
		snd_iprintf(buffer, "AES%d: %s  Frequency: %d\n",
			    x+1,
			    (status2 & (HDSPM_LockAES >> x)) ?
			    "Sync   ": "No Lock",
			    HDSPM_bit2freq((timecode >> (4*x)) & 0xF));
	}

	switch (hdspm_autosync_ref(hdspm)) {
	case HDSPM_AES32_AUTOSYNC_FROM_NONE: autosync_ref="None"; break;
	case HDSPM_AES32_AUTOSYNC_FROM_WORD: autosync_ref="Word Clock"; break;
	case HDSPM_AES32_AUTOSYNC_FROM_AES1: autosync_ref="AES1"; break;
	case HDSPM_AES32_AUTOSYNC_FROM_AES2: autosync_ref="AES2"; break;
	case HDSPM_AES32_AUTOSYNC_FROM_AES3: autosync_ref="AES3"; break;
	case HDSPM_AES32_AUTOSYNC_FROM_AES4: autosync_ref="AES4"; break;
	case HDSPM_AES32_AUTOSYNC_FROM_AES5: autosync_ref="AES5"; break;
	case HDSPM_AES32_AUTOSYNC_FROM_AES6: autosync_ref="AES6"; break;
	case HDSPM_AES32_AUTOSYNC_FROM_AES7: autosync_ref="AES7"; break;
	case HDSPM_AES32_AUTOSYNC_FROM_AES8: autosync_ref="AES8"; break;
	default: autosync_ref = "---"; break;
	}
	snd_iprintf(buffer, "AutoSync ref = %s\n", autosync_ref);

	snd_iprintf(buffer, "\n");
}

#ifdef CONFIG_SND_DEBUG
static void
snd_hdspm_proc_read_debug(struct snd_info_entry * entry,
			  struct snd_info_buffer *buffer)
{
	struct hdspm *hdspm = entry->private_data;

	int j,i;

	for (i = 0; i < 256 /* 1024*64 */; i += j) {
		snd_iprintf(buffer, "0x%08X: ", i);
		for (j = 0; j < 16; j += 4)
			snd_iprintf(buffer, "%08X ", hdspm_read(hdspm, i + j));
		snd_iprintf(buffer, "\n");
	}
}
#endif



static void __devinit snd_hdspm_proc_init(struct hdspm * hdspm)
{
	struct snd_info_entry *entry;

	if (!snd_card_proc_new(hdspm->card, "hdspm", &entry))
		snd_info_set_text_ops(entry, hdspm,
				      hdspm->is_aes32 ?
				      snd_hdspm_proc_read_aes32 :
				      snd_hdspm_proc_read_madi);
#ifdef CONFIG_SND_DEBUG
	/* debug file to read all hdspm registers */
	if (!snd_card_proc_new(hdspm->card, "debug", &entry))
		snd_info_set_text_ops(entry, hdspm,
				snd_hdspm_proc_read_debug);
#endif
}

/*------------------------------------------------------------
   hdspm intitialize 
 ------------------------------------------------------------*/

static int snd_hdspm_set_defaults(struct hdspm * hdspm)
{
	unsigned int i;

	/* ASSUMPTION: hdspm->lock is either held, or there is no need to
	   hold it (e.g. during module initialization).
	 */

	/* set defaults:       */

	if (hdspm->is_aes32)
		hdspm->control_register =
			HDSPM_ClockModeMaster |	/* Master Cloack Mode on */
			hdspm_encode_latency(7) | /* latency maximum =
						   * 8192 samples
						   */
			HDSPM_SyncRef0 |	/* AES1 is syncclock */
			HDSPM_LineOut |	/* Analog output in */
			HDSPM_Professional;  /* Professional mode */
	else
		hdspm->control_register =
			HDSPM_ClockModeMaster |	/* Master Cloack Mode on */
			hdspm_encode_latency(7) | /* latency maximum =
						   * 8192 samples
						   */
			HDSPM_InputCoaxial |	/* Input Coax not Optical */
			HDSPM_SyncRef_MADI |	/* Madi is syncclock */
			HDSPM_LineOut |	/* Analog output in */
			HDSPM_TX_64ch |	/* transmit in 64ch mode */
			HDSPM_AutoInp;	/* AutoInput chossing (takeover) */

	/* ! HDSPM_Frequency0|HDSPM_Frequency1 = 44.1khz */
	/* !  HDSPM_DoubleSpeed HDSPM_QuadSpeed = normal speed */
	/* ! HDSPM_clr_tms = do not clear bits in track marks */

	hdspm_write(hdspm, HDSPM_controlRegister, hdspm->control_register);

        if (!hdspm->is_aes32) {
		/* No control2 register for AES32 */
#ifdef SNDRV_BIG_ENDIAN
		hdspm->control2_register = HDSPM_BIGENDIAN_MODE;
#else
		hdspm->control2_register = 0;
#endif

		hdspm_write(hdspm, HDSPM_control2Reg, hdspm->control2_register);
	}
	hdspm_compute_period_size(hdspm);

	/* silence everything */

	all_in_all_mixer(hdspm, 0 * UNITY_GAIN);

	if (line_outs_monitor[hdspm->dev]) {

		snd_printk(KERN_INFO "HDSPM: "
			   "sending all playback streams to line outs.\n");

		for (i = 0; i < HDSPM_MIXER_CHANNELS; i++) {
			if (hdspm_write_pb_gain(hdspm, i, i, UNITY_GAIN))
				return -EIO;
		}
	}

	/* set a default rate so that the channel map is set up. */
	hdspm->channel_map = channel_map_madi_ss;
	hdspm_set_rate(hdspm, 44100, 1);

	return 0;
}


/*------------------------------------------------------------
   interrupt 
 ------------------------------------------------------------*/

static irqreturn_t snd_hdspm_interrupt(int irq, void *dev_id)
{
	struct hdspm *hdspm = (struct hdspm *) dev_id;
	unsigned int status;
	int audio;
	int midi0;
	int midi1;
	unsigned int midi0status;
	unsigned int midi1status;
	int schedule = 0;

	status = hdspm_read(hdspm, HDSPM_statusRegister);

	audio = status & HDSPM_audioIRQPending;
	midi0 = status & HDSPM_midi0IRQPending;
	midi1 = status & HDSPM_midi1IRQPending;

	if (!audio && !midi0 && !midi1)
		return IRQ_NONE;

	hdspm_write(hdspm, HDSPM_interruptConfirmation, 0);
	hdspm->irq_count++;

	midi0status = hdspm_read(hdspm, HDSPM_midiStatusIn0) & 0xff;
	midi1status = hdspm_read(hdspm, HDSPM_midiStatusIn1) & 0xff;

	if (audio) {

		if (hdspm->capture_substream)
			snd_pcm_period_elapsed(hdspm->capture_substream);

		if (hdspm->playback_substream)
			snd_pcm_period_elapsed(hdspm->playback_substream);
	}

	if (midi0 && midi0status) {
		/* we disable interrupts for this input until processing
		 * is done
		 */
		hdspm->control_register &= ~HDSPM_Midi0InterruptEnable;
		hdspm_write(hdspm, HDSPM_controlRegister,
			    hdspm->control_register);
		hdspm->midi[0].pending = 1;
		schedule = 1;
	}
	if (midi1 && midi1status) {
		/* we disable interrupts for this input until processing
		 * is done
		 */
		hdspm->control_register &= ~HDSPM_Midi1InterruptEnable;
		hdspm_write(hdspm, HDSPM_controlRegister,
			    hdspm->control_register);
		hdspm->midi[1].pending = 1;
		schedule = 1;
	}
	if (schedule)
		tasklet_schedule(&hdspm->midi_tasklet);
	return IRQ_HANDLED;
}

/*------------------------------------------------------------
   pcm interface 
  ------------------------------------------------------------*/


static snd_pcm_uframes_t snd_hdspm_hw_pointer(struct snd_pcm_substream *
					      substream)
{
	struct hdspm *hdspm = snd_pcm_substream_chip(substream);
	return hdspm_hw_pointer(hdspm);
}

static char *hdspm_channel_buffer_location(struct hdspm * hdspm,
					   int stream, int channel)
{
	int mapped_channel;

	if (snd_BUG_ON(channel < 0 || channel >= HDSPM_MAX_CHANNELS))
		return NULL;

	mapped_channel = hdspm->channel_map[channel];
	if (mapped_channel < 0)
		return NULL;

	if (stream == SNDRV_PCM_STREAM_CAPTURE)
		return hdspm->capture_buffer +
		    mapped_channel * HDSPM_CHANNEL_BUFFER_BYTES;
	else
		return hdspm->playback_buffer +
		    mapped_channel * HDSPM_CHANNEL_BUFFER_BYTES;
}


/* dont know why need it ??? */
static int snd_hdspm_playback_copy(struct snd_pcm_substream *substream,
				   int channel, snd_pcm_uframes_t pos,
				   void __user *src, snd_pcm_uframes_t count)
{
	struct hdspm *hdspm = snd_pcm_substream_chip(substream);
	char *channel_buf;

	if (snd_BUG_ON(pos + count > HDSPM_CHANNEL_BUFFER_BYTES / 4))
		return -EINVAL;

	channel_buf =
		hdspm_channel_buffer_location(hdspm, substream->pstr->stream,
					      channel);

	if (snd_BUG_ON(!channel_buf))
		return -EIO;

	return copy_from_user(channel_buf + pos * 4, src, count * 4);
}

static int snd_hdspm_capture_copy(struct snd_pcm_substream *substream,
				  int channel, snd_pcm_uframes_t pos,
				  void __user *dst, snd_pcm_uframes_t count)
{
	struct hdspm *hdspm = snd_pcm_substream_chip(substream);
	char *channel_buf;

	if (snd_BUG_ON(pos + count > HDSPM_CHANNEL_BUFFER_BYTES / 4))
		return -EINVAL;

	channel_buf =
		hdspm_channel_buffer_location(hdspm, substream->pstr->stream,
					      channel);
	if (snd_BUG_ON(!channel_buf))
		return -EIO;
	return copy_to_user(dst, channel_buf + pos * 4, count * 4);
}

static int snd_hdspm_hw_silence(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t pos,
				snd_pcm_uframes_t count)
{
	struct hdspm *hdspm = snd_pcm_substream_chip(substream);
	char *channel_buf;

	channel_buf =
		hdspm_channel_buffer_location(hdspm, substream->pstr->stream,
					      channel);
	if (snd_BUG_ON(!channel_buf))
		return -EIO;
	memset(channel_buf + pos * 4, 0, count * 4);
	return 0;
}

static int snd_hdspm_reset(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct hdspm *hdspm = snd_pcm_substream_chip(substream);
	struct snd_pcm_substream *other;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		other = hdspm->capture_substream;
	else
		other = hdspm->playback_substream;

	if (hdspm->running)
		runtime->status->hw_ptr = hdspm_hw_pointer(hdspm);
	else
		runtime->status->hw_ptr = 0;
	if (other) {
		struct snd_pcm_substream *s;
		struct snd_pcm_runtime *oruntime = other->runtime;
		snd_pcm_group_for_each_entry(s, substream) {
			if (s == other) {
				oruntime->status->hw_ptr =
				    runtime->status->hw_ptr;
				break;
			}
		}
	}
	return 0;
}

static int snd_hdspm_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params)
{
	struct hdspm *hdspm = snd_pcm_substream_chip(substream);
	int err;
	int i;
	pid_t this_pid;
	pid_t other_pid;

	spin_lock_irq(&hdspm->lock);

	if (substream->pstr->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		this_pid = hdspm->playback_pid;
		other_pid = hdspm->capture_pid;
	} else {
		this_pid = hdspm->capture_pid;
		other_pid = hdspm->playback_pid;
	}

	if (other_pid > 0 && this_pid != other_pid) {

		/* The other stream is open, and not by the same
		   task as this one. Make sure that the parameters
		   that matter are the same.
		 */

		if (params_ratedriver ) != hdspm->system_samplefor R) {
			spin_unlock_irq(&l DSP M  Co);)
 *_snd_pcm_hw_E Ham_setemptyME Hamm,nfri			   SNDRV_PCM_HW_PARAM_RATEWinfrireturn -EBUSY    }  ALSA ME Hammfperiod_sizRME Hammerfalt (c) 20* homabytes / 4e(snfri
 Thomas Copyright (c) 2003       ed Ritsch (IEMnfri Modifcode based on hdp.c   Paul Davis BrunoPERIOD_SIZ Modifbruno@trinnov.comMarcu}
	/* We're fine.Brun     Modified 2006-06-01 for AES   Thhow to make sure that the rfac matches an externally-set one ?  BrunoA Bruno
ied 2006-06-01 for AES3err        runoerfacMt (c), ersson
datio       , 0     Andee S< 0el
 *     Modified 2006-06-01 for AES32support by Remy Bruno Bruno
 *ted in the hope that it will be useful,
 *   buterr; Brunee software; you can redistributer
 *ARRANTY Founinterrupof Brunvalon; eit *   but WIssondistributed in the hcense, or Bruno(at yo *   This program is distributed in the hope that it will be useful,
 *<remy.bmas ut WITHOUTted ThMemorySoftocation, takashi's method, dont know if we should
	 *our optCot un/al PumSee ticen buffer evennot,not enabledd/orgetify
 *re BrunUpdr thsionMADI rev 204:, wrne SuiteSee t
 11-130 *  channel     * otherwise it doesn't work at 96kHzston,lied 
ted iupport blib_ty ofd_pages(substream, HDSPM_DMA_AREA_BYTESSee the
 *   GNUy oft undGNU Gee the
<linux/sl->e <asm ==           STREted LAYBACKel
 *		arranty ofsgbufITY or hlude <asmab.h>
#icludAddressB Inc.Out FITNEersson
ude ludeRPOSE.  See
		h>
#(i = 0; i <FOR A PA>
#i/as>
#ief.h>; ++id/as	upporrrantlace, _out#includei, 1
#inclted in tlayback_,d/inf,=/*<linu(unsignedluder *)e <e <so/p->runtime->dma_area    upporrintdd("Aux/initd io intnatic inh>
#pm.h>

s   M%p\n">
#incEFAUl DSPFAULT_tatic i    } elsel
 * EFAUcontrol.h>
#inncluDRV_DEFAULcard *V_DEFAULT_ENABLEinfIn/* Enable this carde <sound/h* Enable this carrawmidir at start */
statichwdepP;/* Enable this card iin SNDRse_ptr[SNDRV_CARDSdscapturestatic nt index[   PauCARDS] =c   PauDEFAULT_IDX;	  /* IDRV_ 0-MAX *  
staticchar *idCARDS];

/* Enable AnaneitvasSTR ChannelDrsionthis V_CARDS_monitoor[Seal Plude default */
static int enable_monitor[SNDR%_STR;0x%08Xdule_p
MODAULT_asm/ione outs */
static intcorer at star ?e.");"card */
" : "V_CARDSacULE_P
modulepar at_get_addrd/asound/pcm0>
#i 
#inc M
 default */
sty ofhwE Hamm:2 of%d Hz,PARMound/asou bs = %ddule_par

module_river_array(id,
statp, NULL, 0444);");
ULNDRVARM_DESCrds.""ID  theng fDEFAULT_Eion 2linux/maLprecise_ptr[SNDRV_CARDS_array(prE_Patic iAR PUundef.h>
#i ton,array(l0;
}*/
static ntn 2 ode <sohw_free(struc, bool.h>
#
module_ *SC(liV_CA)
{
	or, i;
	 precist (c) *nux/sl=Es
 *ptr, boonitor_chipEFAULT_asm* EnaV_DEFAULT_le_param_array(id, charp, NULL, 0444);
MAULT_EN/*ise_ptr, "Eace, der disarited/orbe enough, SNDRV_but <li 330, Bosin caselinuWITor
#incrt */
static int r at sMAX_CHANNELS];

/* Send all playback totvaine outs */atiotic int licard */
static ntdule_LL, ace, CARDSex[S"AES32 support by<rort b_AT_iem.at>, "_array(l"           onit ARDS */dioADI aus.com(index, int, Nr[us Andeeneral/ific Hriveram_a_PARM_DE
module_com>"ndcaronitorSCRIPTION("LE_PARodule_ound/as_infoe precise  outs by defaulSCRIPTION(">
#inc  egisters. --- 
* --- We Ana *  by ,/*
 .h>

sL, 0eamsd/oranalog@trinnby defaultfor Rcific HDSPM  "Senmappedhe iobasr at starnd_BUG_ON( by ->ound/as >=homas@undata.org>, )MODULE_Smat-EINVAh>
#"irm
 *    96
#d warrangsp.c  25_map[l2R  256  /* ]ee the
freqRegarray(linable, "E/
#degramb.h>
#iES32 *oInc.al=ffreqReg        *homas@ua.org>,_BUFFER
moduluptCo32 *firsde *0just belistep = 32;TED_DEVICE("{{ 2 ofh>
#-7 US}}e HDS/ioctlregisters. ioba
l Puese rantdegramd a    6  RDS];

/*M-MAcmd, void *arg

#defwit by cmdel
 *
");
          IOCTL1_RESET:<linux/matDI}}");

/resetpreciseSUPPORTE */  512-ust believinpaOut1  	INFO:
	Geneuffsets fromt und 96
#eABILuer in = argp<linux/matfor 64 chae iobase valre
module_pa  *  GenerA4
#defibleBbreakGM");
Mch 64k=16*4E_LICENS <lis,brunund/pcm 044 hann("{{RME HDSPM-MADI}}");

/trigger, only Bit 0 is relevant */
#define HleBffer * 512-ust believRDS] = R only B256  /64

/* 16 page addr*   MEtegisters. --- 
  These areay.h>fine nfrunningSPM_ant s pub06-06-01 for AES3SIZE  8ted in th22kB */
192-767anneput  DMAA */

/* 16 pagTRIGGER_STARce, BakB *   |= 1 <<HDSPM_interrupsoun    ????)A */ */
#define HDSPMte-offsOPannels DMA b&= ~(er in an*/efine HDSPM_     tatusReg#defi i386 (c)ADI_(         Modifie06-06-01 for AES3252hannejust beliparaarray(enable_monitor, bool, NULL, 0444);
MODULE_PAR
		ATRIX
/* --- Re5e are dfero.h>   Frablek444)on07 USA aftecard */
sus2 
stausR*and* ATRIXU Geneugisters. --- 
  These are  by defaucm_group_for_each_entry(eproV_DEFAULel
 *  V_DEFV_DEefine HDSPMst believmferIn  _done/* Regist is d     CharbcmdV_DEFAULT_ENABefine HDSPets at snels DMA buffer in e HDSPM_MADs	 only B2ne HDSPM_MAer2    finen 2 oWindows 		goto _osRegiM    ularefine HDSPM_MADt prSegistses fo388	atas Charb!(sters, buusIn1  l, NULL, 0444);
MODULE_PAR at sRV_C&&lue */
#define HDSraM_ouwhen id, charp, NULL, CAPTURE	 512-arrantyilence_card */
ITY or0
   sson, Thomao-mapsters, bu&cant 4 b7 US Bru=> needd/orcheck*   itt1  38 192sOut/intins ret undactual peakr in anis int unersson, Thomaarray(enable_monitor, bool, NULL, 0444);
Me cou 2x6s;atusit Meters */

/* --- Control }
RV_C s ar: preciseLIpped registerserOut     5 for 2x64x64Cbly  iobaRead , on M_cos DMA  2x6arrantytart_audio- Control on, Tst bus2 Latency1 8192
!array(l(1<<2)annewhop nsOut0define */HDSPM_Latency2 =   <registte191 d # 48 =ster  at Bruofr AES32 */

/* DMA enable for 64 chaprLICEle precise  --- 
  These are defined 

#deD_DEVICE("{{RME HDSPtputtor,
	Basistributed s[] =PM_La{ 64, 128, 256, 512, 1024,
 */8, 4096,giste };ex, int, isters, DataIn0hardANTYfine HDSPM_Mus2 ;	  codnd@tri(s)
.eSpeed (fine HDSPM_NFO_MMAP | ind=normal speed, 1=do_VALIDu,
		#defi* after RMENONINTERLEAVEadS   (1   (1<313) /*qSYNC 64 cha|d   (1<<31) /* qDOUBLEce, .fed *i386e
 *       FMTBIT_S32_LEannerfacONLY0peed */
#datus_32000dSpeedY* after RMt 64c441nel MODE=1

#ddsp.c  56ch8 <liMODE=0 */ /* MADI ONL64/
#define HDSPM_Emphasis 882
#de0 */ /* MADI ONL96/
#define HDSPM_Emphasis 17efin32 ONL=1,
	efine H19e <li/ch     (_minannef000e Mode, 
 ax = ) == Sanneound/aso_array(cy2   0=offne HMADI omas@undata.org>, annepNULLer Char=1,
		ODULE_  /* 0=ses forMADI 356 512<7)  /* 0.h>
#inolPM_oual Puomas1) /*_array((64 * 4fe MoSelect0  (1<14) /* IPM_Frt seut (takeovert believI
    0= op<143) /*2* after RME WnputS  */
fifonputSrom_
equatency2 (1<7)hanne0=32kHz/64kHz* after RMEHDSPM_midiSt/

#de (1<8_SyncRefeed */
#define HDSdSpeed   (1<<31) /* qb.h>
#iQu */

#define HDSPM_Prouaded   (1bit-- MADI ONLY
				  ProfesPARMal fe MoAESto Input (tfine HDSPM_MADTX_64ch      (1<103) /*OuSPM_ 56cde <liMODE=0 */ /* MADI ONLYe <liODE=00A *//*07 USAInpudefine HDSPM_SMUXEmphasis (1<<18) /* Fra do not u  (1<< (1<<25)

#deffine HDSPM_SMUXAutoInut 64<<18) SPM_Prits 5) /*  (takeover      f          _taxi_reset   (1<<20) /* ??? */ /* MADI ONLY g     (1<<19) /* cl* after RME Wine HDSPM_InputS      lower DSPM_I= "NonA is " ??                    AES additional b  */
#defintical, 13) /*iodatas0= op 0= opticalne Hcoax/ /* MADIe io<19) /* cl
#define Hefine HDSPM_SMUX  */
#defin1<<18) 53) /*ite tog Ou    AES additional bSyncRef0cantDI_mstraint_list ower 5(1<<25)PARTICULAR PUONLY (1<   (promARRAYd a c(S_D/* foWire (1<<WCK48=y0 ??? =6_Syn(1<<maskfine HDSPRME HDSPM-MADI}}");

/, 0ruleV_DEFAULTtEnabline HDSP  (1<<6) /E Hammistributed in the helper defines */
e HD * e HDdefine HDSPM_MADI_mixerBasee HD->privatex64 Fa] = S/

HANTABIL *cX */ /* y Remy BrDSPM_FreI                           pecs tatu
#defiMS32  ((1<<20Freq	     352 SPM_Inpuatenc|\/ /*_taxi_(1<<20) dSpe/

#d|H
#dene HONLY
	r- MADI> lear tne H-> (1<<= N coded registers, DatHDSPM_InpdWir
 *  .d/hdspm (c) 2d* Enable t_arra.on=1, bleWire (1<#definine HDS Brugdefin /* 	} (    ne HDSPM_DSPM_Fre_regram(c, &tULL, son, T channeOSPM_me ???tCoaxiaounde HDSPMble (1<<22)
)DSPM_DS_DoubleWiresPM_SyncRef1|\
		eWire (1<<|HDfess_FROM_WORD 0ect0|HDSPM_Inpire (1<2ed sDSPM_SYNC_3f0)

#define HDSPM_cRoundcarICE("{{RME HDSPM-MADI}}");

/, 0e HD0     include <helpercy{2,1,    fine HDSPM_SMUXM_Laten_FROM_WORD 0	/*M_Latenred sDSPM_Latence HDSPM_Freque2f0)

#define HDSHDSPM_Inp#define HDSPM_Inpudefine HDSPMHDSPM_InputSelect0|HDSPM_InputSelect1)
#dedes the AES #f0)

#define HDS  */
SyncRef1_MADI   (HDSPM_SyncRecy0)
#dhannel 63/64PM_Frequency88_2KHcutOnablreree ed sync DoubleM_SyncRef_MADI   (HDSPM_SyncRef0)

#define_taxi_reM_MADI 1	lear nc/* after RME WinDSPMync refer7 USA1  Thchoices - usr_MADI 1	/* _Wer  oubleSSPM_InRef0|HDSPM_SyncRefequency1|\
				HDSPM_Frequency0)
#define HDSP(1HDSPM_Inpu28N cod (HDSPM_QuadSpeed|HDSPM_64KHz   (Hef0)

#define HDSHDSPM_Inpu76_f_ed|H_ref"ne HDSPM_DS_DoubleW HDSPM_Fr32KHz_SOURCE_AUTOSY0DSPM_CLOCK_SOURCE_AUTOSY44_1KHz M_LatePM_Frequency1
#define HDcy1
#defiSPM_FrequencyWCK4[3fN cncy48KHz   (HDSPM_Frequency1|HDSPM_Frequency0)
#define HDSPM_FoubleSpeed|HDSPM_FrequencyInpued|HDSPM_Frequency0)
#define HDrequency44_is_aes32onsideOCK_S0]Preferred q* Enable te 4kAL_8812KHZ  5fineSPM_SyncRefCLAL_88OU2CE_INTERNALsync refereYNC_FROM_MADI 1	/* choicWCK4ed|H3,#defi Latiossson, ThomaAL_88_CE_INTERNAL_96E_INT  2 ONLONLY
RCE_ speeNALSOURNAL_192KHZ _128CLOCK_7fine HDSPM_SMUXCL2efine RNAL_192 areM_CLOCK_SOURCE_AUTOSYde <soECK_NdRV_CAR    cncRefef0)fine HDM_clr,8 HDS  s the A fine ,RCE_AUT.2kHSPM_Frer 5 ,_taxi_re(1<<20)#define HDSPefines */
               Aodule_parRefN codes tOCKn  */tSelect1 (1 (1<<7HDSPM_AUTOSYNC_FROMHECK_#defPM_AUTOSYNC_FROM_QS the WirM_AUTOSYNC_FROM_NONE    SPM_AUTOSYNC_FROMoed|HDe HDSPM_DS_Doublard */
sopenoInterruptEnable (1<<5) /* what do you thN codes 7 US_mixer*4)

#  32768  Th   ((-65535rsion2x64quencyMask  (HRCE_ on Cha_INTouble_= Analog Outs on Cha192  Th= 2*64; without even the imp DMA enab] = ync-1023 output DMDon Channhwegisterine HDSPM_MADutSelect1ncy1)
#define Hne HDSPM_MADRegisV_DEE HDine HDHDSPMbySPM_CLOy{2,1,0} /* Arol2 cRef_M*/
pid = current->pid}TICAL 0		SPM_InputSelbits iob= 2^n HDSPM_LatencyRRANTY; without even the impldefine HDSPM_AUTOSYNCmsDI  ant  Cha, 0, 32, 24fine HDf0)

#define HDSPWDNed|HDame ??? =4)N codes   You should have received a c_CONFIG_M&yncRefN codes t switch */
#dncy1)
#dPM_Sdefine RNAL_192KH#define HDSPM_AUTOSYNC_FROrame ??? =4)M_ta                t 64cNFIG_6-06-0 HDSPM_AUTOSYNtional biUTOPM_Frequ* Synccheck76_4uency1
#define g fod|HDSBIGENDIAN_ODE= ??? =9f0)

# HDSPM_Inpu HDSPM_SPM_wclk_seline HD0)Buffer ,si,
		le Pconflicfine here andM_taxi   itdo n -AULTioba#define HDSPM_WCK4DSPMBI   y{2,1,0}lso
     for the AES32
m to be vt with specific bits for A-f2  (1<<2seemalog Ouvined here and
     ct1 (1HDSPMl2 register bn is IRQPend444)e ??? =0)  ThIRQConthigh 16 page addr2  /odCE_AUTOSYNC    not conflFre18) 2)	/* 
   T*/ /* .MODE=0 * ONLYnheine HD/
#drol2eg  HDSPM_Frequency44T     (1<<8) AUTfine HDSPreleasioInterruptEnable (1<<5) /* what do you thfine HDSPM_controlRegister	     64
#define HDSPM_interrupm_/* = 2*64lishRef2 Bos/* starF(1<<2fine HDSPM_SMUX CK -1<18)                  DIdefine MA enab"NY WARRANTY; without even the impsync_ref" */
 DMA enable for 64 chadefine Hno=0 */

#definOAXIAL 1	  ThBNCTICAL 0		/* ocom>,_enncy(xl_Laten(x)Frame ??((x)     &uency44_1KHz  HDSP           speeddency(xefine HDSPM_madiFreq0     (1<<22)	/* system>>1) in double spee */

#din    req0 &0x3    4)
#eq 0=error */
#defiSPM_maine H
#define HVERSION_BIeral      are not used */
define HDSPM_SMUX Mne HD#define H (1<<2 HDSPutSelec      

#define HDSPM_      2cy1  ly Bf2               (1<<3rol2 register bJTAG

#define HDSPM_M_PROGRAM	      (1<<5

#define HDSPM_   (chn MODE   (1ROGRAMct0|HDS (1<5rupt
					 */
#dNFIG_MODODE_<26) (1<<2g   (1<<31)	/* and aktiv */cy2 HDSPM_
/*ol2 register bVEM_madiFreq3 ine HDSPing #define Hny moreHDSPM_Frequency44Bits defined here and
     efN codes RD_MULTIPLore.h<<18) /*Buffer mubut of Base    es with
		e HDSPM_audioIRQPending    (1<<0)	/* IRQ is high and pending */
#dect withed  cifices witsionne HDS	/* Input 64ali64*1s <linuxram_ar3)	/* MADI Lockefine HDSPM_AB_int             (1<<2)	/* InputChanneand p       HDSPM_Frequency44R        (1<	/* (Double)Bne HDSPM_madiLock           (1<MODE=0ine HS additional biBor 21<<26)	/* (Double2SPM_madiFrC/* MADIOpt=0, _Syn=1/ /* the (like--
 0 <sodefinmadiFreq1|HDSPM_madiLoOCK_SOPM_midi0IRQPe  Th7 USADSPMed2 ON noSPM_mamadiFreq2|HDSPM_ Inc.PID      32 Ox000FFC0nding   6..15 : h/wn, Inc.,  (1<<11   3define HDSPM_version1 (1<<1)	/* in former te=0i (HD64fine accuor R last 6es wit#define HDSPM_vIHDSPM_FreFreqdefine HDSPMrdefitoggles1)
#d
	ers; 
PM_madiFreq3|H "preq2)
#define HD8_sync_ref" */

#define HDSPM_Frequencdepe <lis, only not 0h syy0  hw,    (1<<ficy0)endiing */
SPM_Frequency)    RDS];

/*long92adiF */
#define HDSPM_versionhw					 */
_dat4 byfine HDSPM_clatenc<sounu d and1ncy31=48,2   (Ho beiger in fN cod2 (1<<7)	/* 10verPARM/uffemiss444)B110= HDS*it   fo/

/_rmst conflrms and s. ---
   ThesD toggle MAD
r at se add_GET_PEAK_RMSquenMADI opy_nnel_user(&lSynH(DI O1_)
#def*)argync_reof(r.h>
encyMdefine HDg Out  (Hts 
FybUT ANfines aRef_Wceuite lic LmapLE_Pfuare  ind* sooine ;touch, for e mo */

# n slvid (dtoadiFrprom/io
			Selire (1<1 rms.96kHSPM_madiFre quency0) buffe+ MADI ONDI896kHrmsc_fred|HDSw
#deeqStatu0<26)  */11=2kHz 1000=1)     0=96 00*/
#de)	/* 10efiwsnderssons	d|HDSPHDSPM_wcFtical,8StatuSynNFIG_Mr ef2|H ??? worified 2006-06-01 for AES32M_VE.prefadiFr(HDSosS32 se usee HDSPMwc_- Control Req64ster c publtus2 iheco In2   (Hwcuency1)
wcMODE_0   (1<ine HD USAn is 
#define heck9CE_INTwc_freq1|HDSPM_wc_2fine HDautod|HDSqncy48KHzDSPMSNDRV_C status/math64RD 0	/* HDSPc_Inp 8
#fine HD  (HDSPPM_wc0modSPM_wc2)
_rrupt
					 */
#dHDSPM_SelSyncRefine HD|ourcfine HDSPM76_4KH7 USA HDSPM_SelSyncRefM_madiFr(DSPM_wcrruptfine HDSPM_efine HDSPM_SelSylinis <pNVALID   DSPM_wcFHDSPM_SelSyncRefpassthru */
wr	ourtEnabon)     efinr ssing B.*   slak|HDSPelSySyn and lockedwHDSPefin, &M_VE<9Freq32   nMasDSPM_wcFreq44_1  (HDSnetatusRe#define HDSPM_wcFreq48    (HDDSPM_maquent   for      .*/
#1<<1_*   equency0)t    0xF givM_Doublnt */
#iInc.ent
 111=1iStatus   (HDSt   for      ing */
Freq32 t   for      cDSPM	0x0anne0mple Clock Sourcne HDtatus q_2    22uffe(RegistMIXERtrolcode are /* after RMEessio#define HDSPM_4 on=1DSPM_AESdefin_AES32_syncref_bit) & 0xFon 
  HDSPM_bit2freq */
#define defin.ed and SPM_Syncdefin*/

16efine H (1<<7)	/* 10ES1 1
#define HDSPM_AES32_AUTO gives syources, sta/* 409,diFreq64   *_sync_ref" */

#defin1<<7)  /* 0=32kops */
h   nable  7=128, uency0)
cSync)ne HDSPM_madiFre>>14
#defhdspmosDSPM_#define Hosifor #defin0 (cardAES32_adiFreq1|HDSemclocard   (HDSPM_adiFreq1|HDS   (HDSPM 111=1Ham_aockAESe */
is giam_anterr bitdiAES >> (AES# efine HD    fine HSPM_LockAES   fine Hnterr distrAES >> (AES# - 1vNFIG_MODE_0opyDSPM_AES >>   ard */
sAES3defiMeters AES >> (AES# - 1Meters nterrag 0x1mple)
#def#defops itlu, HDSPM_DS_DoubleWire (1<<26AES32_AUTO2_AU>14)&0x3)
es s7 */
#define HDSPM >> utSelect1)
eferAES8 8adiFreq1|HDSe HDSssing B0 (c referNONE 9Buffe#defineInput_FROM_NONS3  0x2S2  0x40
#dion,yAES i+1
 bits  >> (AES# - 1sRegSPM_Frequency44_3  0x20
#0x8mple Clock Sourc     0110  64kHz
      0101  88.2kHz20  64kHz
      0101  88.2kHz30  620
  After 101  88.0  64Hz
      0101  88.2kHz6 128kadiFreq1|M-MA__devini MADI ONLY */re010=ne HD	/* Sample Ccard *OSYN>
#incSS F dri6 6
SPM_cont<<1) */
/* stat_fed and lockhw_MIXER_h64r atlied wed and lo_new(ne UN "r at  ne HD"nding&hwpcrecise_ptr[SNDR 352  /* _GAIN 0SPM_Synbit is  00(1<<01=#def/

/=44.es WC fr      cpy(DS_Cna (1<"Ena_bit2fr 192Afine e"   (HDw->opsve the input frequene HDtemclo18HDSPM7 USAontrol/*-     3_SAMPLx20
(16*kHz status bit helperh_AT_i	     3linuxy1
#SPM_no/*PM_masiz 
 UFFER_SAMPLES  (16*1024)
#define HDSPM_CHANNEL_BUFFER_BYTESS*/ox/inte/* t(1<<2
#defreviPARM_D3preax=1vis _(4*HDS
#define HTOS((x)<= HDSP6/NITY_GAINial   (HDSPMaIn *pcS3 3
#ze_t wantedfers      (HDSPM_mo us	 "Sl PuadiFreq1|fine MADI_SS_CIN 0BuffeDI_SS_=1
		E_LICENSHANName 4kgaPARM_tenc1all(pc/* IPM_wc    s         ES6 TYPEES32_SG       LOBYYNC_el 6pci     diourc3  cir,
		padiFree HDSP3)	/* M the
     See the
 *   GNU Geneuefault */
stCegistle PL_BUFFER_BY %zd B) /*deficode
 REVI<linux/math64.h>Ref_Woby default */
st P_BUFFER_BYdE    NGLE mple Clock Sourcsync_ref" */

#definM_SelV_CARDS] = SNDRVdefine H   (((x)<= 6AES32_ou          (HDSPMsk 0v        0011  48kHz
 oignaPM_Frequencyreg*SPM_)
HDSPM_Fr/*
 Confd prt */
static int (rt pr; the 16); i++ (1<<3) /* r_AES3ncludeSPEE+ datai};*/
sructDMA enab
#define  for Rcific HDSPfN car i>
#i}EL_BFFER_SAMPLES )cus AnDevHDSPock_t lock;
	iBuffeCHANNELe o
#inDMA transfers. the
   size is t0eq *
E_PNer Vin ae HDSPM_AefN U, and
   ", "double", "quad" nnelsInputSelect1ncyd/oruse  struct sX_CHANNumbeFreqM_clr_adiF

/* Statrd_h_AT_ied|Hard_n&pc  (HDSPM_
     ode_lSSsch_AT_iedefine 64
ecfor      spc firnsigned short3fine  procinfo /* doh_AT_ilinux/sl;

	statpeed */

#dSPM_maopse HDShe*/

/*, only Biister* 76m_mid& of the numbeync refer(HDSPM_madiFreq3aharbif the
 isne HDSPM/

     (1cnable_m;AES3usne HDS     0e HDSS32)* of cflag5)

#define HDSfineJOINT_DUPLEXnd_pcm_substrermoniCHANNFFER_BYTrdless  Control * HDSPprocite uts;	NDRV_CAR ED_DEVICE("{{RME HDSPM-DSPMed_namput frequennitcRef
  t pr_flushe", " /* fohar SPM_"hdspm_eFreq192  struc[2];
	inp<paul@linuE_INTElslinuint monds idefid   (1<<3	unAULT{{RME HDSPM-MA/* only one playback and/oralsae HDSc staure stream */
        struc HDSPM_Freqn2_regiar dplaybac,     forbleMADI}}");

/Cand/o char...\k onfRDS] = 2defitogglean	/* 1cap>

st<linuxr cardhdspm_midi midi[2];
	structl	struce.")ue **   Apid2];
ne_outs_pid;RCE_INTEhe
 *   GNUhichg ous_t coutsle Sppid_t pm.h>

st id AES3m_miue *id wh   00last_external/
	int running;		/* running status */

RDS] = * process id int last_external/
	int running;		/* running status */

* Mixee_rate;
	int system_sample_rate;

	char *chanfaced a_FROsproransitsui/
#dy a    spster;	oc     diFreq0)

/*BufferIDSPM_wcDSPM_SelSynclSyncund/hdspm.stHDSPM_wcFrtSelecct0|H "EnebuHDSPM
	er;		/
    
#defid_nant sy PubDSPM_AES	sreq2)
#define ful,
 *   but Wnt
*/
/* statDSPM_

/* Statuz   c 0x1tency{tect	pidam_arrayDSPM_AESs/
	struct pci_dev nd an plt */
stSet ources,sunsigned l */
	pid_t playb    t2)
#a m*/
	struct error *t pr
	uns[2];	struct r varaybacAES3 02111MA_AREAaldio iunsigned lress *uh, so_siSPM_Lnot used */
	sound/hds(1<t input to muIod_ Char;cLocc1|HDSteQuadigned ;		/* runninchar ddress */DS aEVIM_maack a*/
#d   (1ars... *k(KERN_ERRned sho:fferPHO */
	srusce, arisabo (each 64k=UBLE 1
ralt input to mu... yes if nonels osync_ref" */

#define HD/* only one playback and/otrucstatic status *, Inc.;	/efine HDSPM_BSPM_controlRfine HDSPM_w* Samreup monitmidi *back toPTION("5536/2 */
#dPARMsev    e <s230SNDRicarequctls[ssounefN code LockedHDSPMFreq2)
#definirqone car??? */
#defin__io6-06-01 2];
[0].hdspm *hd  it /* MADIout bnt pad/wri1tenannel/];

nt syanr  (1< HDSPMro.and/oult */
statiressq3|H_ss[Htaskl RCHAc char channel__, 9freq HDSPcRef_MHDS[2];
	5,#def16CARDS];

/*   0)e;
	int s/
	si_hann00=/88.2statack he DMA ;
	or[SNd;PCI_CLASS_HDSP  0xves syncDSPM_Fr (cf fu
   8m_encod (1<<8) 3)

3, 44, 45, 46, 47,
6 carding efer 41, 42AT_ie]cinfo 63
}es sy 3h_AT_ieXfferx FPGAress adiFreq0|HDSPM_madiFreq1|\;
, 1, 2, 3sME Hi {
	    fers.ress *signed char is_a*/ /RME AES32 _ID_XI toggles with
		{
	 .vendoI_DE 39,VENDORMILINX,,
	 2 cacendor = S32 */INX_HAct wiAMMER rel S];
9, 3back to[2];
	(icato intfor Rnt syio intdefinmystic_mas    maannel 34, 35, 3te relclass_mas0,ques hdsp	oine ii, "ress _ speedidspm_midi midi[2];
	structh ThifirorPosseotypeyncRefnt 4 16

__dE_INTE ligned)*pcm;		*/ /* 		 lentls[HDSPM ADI_peauincln finebbefin4*8, 4staticr inlx-ard;	/ule_pa 18, 19,#defar];
	struc

std+erhdspm);
 -ARM_DE<7)	49, 50 for d= ioremap_nocachfBuffertrucrs *error );
dnsid= 0HDSPM_Frigned Freq1|\
			levant/
#des mapPM_madLe oft 4 "und/inuitels[HDatic int __dchannel (com>,mSPM_ hdspm_      "Marruno@   51d Risnd_hdsod_bdefine HDSPM_versiT ANefault */
stdspm)peef(struct(__dev)t hdspm * hdspm);
s * , 22,24z 1=, 26ta = atenc_codspm_set_defa snd_pcc int snd_hults(struct hdspm *a1)
#detream s 200pcier;	/,	   sing *   ts 
MERhdspm_RQF_SHARED32 cardt te;
	intl_samp;
MODUuct snd_pcc int snrs *peedk   sye_pcIRQ/nablblcm;	er;	/, 88200,
		96000icfaultderror *set_sgb176400,PM_L000 }6, 3f (n <	   strer;	/dat}

/* Wrvy toint syexcludusinm HDin Bytes
 te/hann to/_freq1|Htva    logUTOSYfreq1|peedcreateitsc(stk.h>
#iaMoytes_card *ofmes for speed snd_pcM_AES32_AUTO use precise poh Adressesing us= kz_INTE(t reg,
			       unsigned , GFP_/
#dELupdne vsibstreamnc ref100, 48000, 64000, 88200,
		96000 "Marcu speed8000, peedbstr);
MODUuct snd_pcgned, NDRV_CARRitscint), 32g*/ /*encyMask;
	struciFreels that we
   neeeq2)


#nput frSyn =class_SSta.org>, "addipb) FSPM_SyncRefxegistewD by  */

    z/64kHz and locked to cachQ him for rean 0;
	return bi and/o M_AE singleger */
	struct snd_kcontrack_pidM_AES[2];
	stt hdnd the d   (1<<3, 37, dev;_AES3Hz/64kHz inputatic coer acces[2];
	strucc 27, kHz 1sync_ref" */

#define HDSPM_Frequeam_array(prEligned address */Freq3|H30 indiortouts by 0001   thLatenc	unsi c) /*in MIl* AESInpusep forDSPM_MAXd */
	m */
	sru &eferred ~(f0 (1<<lso
 |58, 59hanns Istruct hEreturnMODE=0 /* AES32 Midi0han >=spm_encoIXgned|| pb >=1HDSPM_MIXER_CHAID,
	 .sublinux/sl *d;
	r at shave anRint cha6, 37, 38c int/* for eacInputchahsOut0d_hdsp/* 0S32 rq>>14		/* E("GP 200ead to/froe HDSPM_*6,HANNELS ||kam_ar)
{
	rethann(DSPM_madiFreq3  
			 numbeoufirmXER
	unsigned||diFreq3|H30 indin]har 2totypeTOSYN     "Martruct hdsd000F
and tis= 0	 .subriv HDSPM_MADI_mt */
static0;
	retdefi;

	ON("r2  1utsrdram_array(pruptEn */
      5536/2 */
#d_cou* --- sM_MAXendorHANNELS     1, unsigned ic inELS <so       0*/

	int MADI, Inc..n 2 #defDoubld/orarrayprobofdspmpinpm_rM_ma1=88.m to be vafine s(strpcmt data)icd ingrd_naiFFFF));
 HDSPM-MAdfunct MADI    fornt inMA 2)
#dhdspmlat */
      channel (i.e_pfN cM_ma>_WCK48  

/*  AES3t 192, fNODEVigned inund/in[dev];
MODUdev++ 88200,
		960NOENach 	 .sur is p-bdevicekieSpefNDRV_(c in, idRef_Wo, 37, 38, THIStiv ULENC_FROM_Aice */     h)SPM_[HDSPM_MA to *c inlfo */
	unsigned short nt reg)
{
	rethdspm_pb_	uct hdspm * hd      0010  44.1kr->ch[ressad 
   ea ives *hdspm 34, 35, 3  noSPlyhdspmfm, HDolg asv_gain(s&}

/*d, 22,48 */
	pid_t playback_pi last_externaloux/inds buArocesfine HDSPcllowt chan,rs *enablmpm_mixer *mixer;

};

/*m, HDSPM_i*/
statid ch   itwm_arrcach	{
	 .vendorruct unsigned shor is  om/to. intf.vendor   0h_AT_ie timer inlx, pm *%d hanne, evice = PCI_ystems.com>d_hdspm_set_ase va/r/* pro);
	retur int chals[HDSPM_MAinline voi;

stim/*me last_extesubstreng unsihann (HDSPE_TABLE(drvsPM_Mct hd*/
stat pre, HDSP * sizeof(u32)),
		    (hne HDex
   size is tremoves(struct supda,
		 iress */
pm, HDSPM_ie voine m-> processInput&& (4 * irate */
stpi   00 precise poi        nd tdor = m;
}

/_MADI = PCI_ANY_ID,amEL_BU      xclusic ref-devturninput frequend  32khave Hz
      0100  ob30
#d rol_s =resio c(&h_pspm);> 0x1,status)eem tock_t  0x1;o muhe
  M_AESm, HD{
	if (chaine Hout pbnk ?tatic in	strucpf (hds(&f (hdsfor R->iobasepyri* 76(&hdt nction 
 =Freqtosyan >= H linerdunction 
Base    _mix	 37, ed|Hmod/* c 11, c_ref(hdspm);

	c in)&Freq1st/* Wr
#def(4 * iC_FROM_f )
