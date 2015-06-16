/*
 *   ALSA driver for RME Hammerfall DSP audio interface(s)
 *
 *      Copyright (c) 2002  Paul Davis
 *                          Marcus Andersson
 *                          Thomas Charbonnel
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
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/firmware.h>
#include <linux/moduleparam.h>
#include <linux/math64.h>

#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/info.h>
#include <sound/asoundef.h>
#include <sound/rawmidi.h>
#include <sound/hwdep.h>
#include <sound/initval.h>
#include <sound/hdsp.h>

#include <asm/byteorder.h>
#include <asm/current.h>
#include <asm/io.h>

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	/* Index 0-MAX */
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	/* ID for this card */
static int enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;	/* Enable this card */

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for RME Hammerfall DSP interface.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for RME Hammerfall DSP interface.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable/disable specific Hammerfall DSP soundcards.");
MODULE_AUTHOR("Paul Davis <paul@linuxaudiosystems.com>, Marcus Andersson, Thomas Charbonnel <thomas@undata.org>");
MODULE_DESCRIPTION("RME Hammerfall DSP");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{RME Hammerfall-DSP},"
	        "{RME HDSP-9652},"
		"{RME HDSP-9632}}");
#ifdef HDSP_FW_LOADER
MODULE_FIRMWARE("multiface_firmware.bin");
MODULE_FIRMWARE("multiface_firmware_rev11.bin");
MODULE_FIRMWARE("digiface_firmware.bin");
MODULE_FIRMWARE("digiface_firmware_rev11.bin");
#endif

#define HDSP_MAX_CHANNELS        26
#define HDSP_MAX_DS_CHANNELS     14
#define HDSP_MAX_QS_CHANNELS     8
#define DIGIFACE_SS_CHANNELS     26
#define DIGIFACE_DS_CHANNELS     14
#define MULTIFACE_SS_CHANNELS    18
#define MULTIFACE_DS_CHANNELS    14
#define H9652_SS_CHANNELS        26
#define H9652_DS_CHANNELS        14
/* This does not include possible Analog Extension Boards
   AEBs are detected at card initialization
*/
#define H9632_SS_CHANNELS	 12
#define H9632_DS_CHANNELS	 8
#define H9632_QS_CHANNELS	 4

/* Write registers. These are defined as byte-offsets from the iobase value.
 */
#define HDSP_resetPointer               0
#define HDSP_freqReg			0
#define HDSP_outputBufferAddress	32
#define HDSP_inputBufferAddress		36
#define HDSP_controlRegister		64
#define HDSP_interruptConfirmation	96
#define HDSP_outputEnable	  	128
#define HDSP_control2Reg		256
#define HDSP_midiDataOut0  		352
#define HDSP_midiDataOut1  		356
#define HDSP_fifoData  			368
#define HDSP_inputEnable	 	384

/* Read registers. These are defined as byte-offsets from the iobase value
 */

#define HDSP_statusRegister    0
#define HDSP_timecode        128
#define HDSP_status2Register 192
#define HDSP_midiDataIn0     360
#define HDSP_midiDataIn1     364
#define HDSP_midiStatusOut0  384
#define HDSP_midiStatusOut1  388
#define HDSP_midiStatusIn0   392
#define HDSP_midiStatusIn1   396
#define HDSP_fifoStatus      400

/* the meters are regular i/o-mapped registers, but offset
   considerably from the rest. the peak registers are reset
   when read; the least-significant 4 bits are full-scale counters;
   the actual peak value is in the most-significant 24 bits.
*/

#define HDSP_playbackPeakLevel  4096  /* 26 * 32 bit values */
#define HDSP_inputPeakLevel     4224  /* 26 * 32 bit values */
#define HDSP_outputPeakLevel    4352  /* (26+2) * 32 bit values */
#define HDSP_playbackRmsLevel   4612  /* 26 * 64 bit values */
#define HDSP_inputRmsLevel      4868  /* 26 * 64 bit values */


/* This is for H9652 cards
   Peak values are read downward from the base
   Rms values are read upward
   There are rms values for the outputs too
   26*3 values are read in ss mode
   14*3 in ds mode, with no gap between values
*/
#define HDSP_9652_peakBase	7164
#define HDSP_9652_rmsBase	4096

/* c.f. the hdsp_9632_meters_t struct */
#define HDSP_9632_metersBase	4096

#define HDSP_IO_EXTENT     7168

/* control2 register bits */

#define HDSP_TMS                0x01
#define HDSP_TCK                0x02
#define HDSP_TDI                0x04
#define HDSP_JTAG               0x08
#define HDSP_PWDN               0x10
#define HDSP_PROGRAM	        0x020
#define HDSP_CONFIG_MODE_0	0x040
#define HDSP_CONFIG_MODE_1	0x080
#define HDSP_VERSION_BIT	0x100
#define HDSP_BIGENDIAN_MODE     0x200
#define HDSP_RD_MULTIPLE        0x400
#define HDSP_9652_ENABLE_MIXER  0x800
#define HDSP_TDO                0x10000000

#define HDSP_S_PROGRAM     	(HDSP_PROGRAM|HDSP_CONFIG_MODE_0)
#define HDSP_S_LOAD		(HDSP_PROGRAM|HDSP_CONFIG_MODE_1)

/* Control Register bits */

#define HDSP_Start                (1<<0)  /* start engine */
#define HDSP_Latency0             (1<<1)  /* buffer size = 2^n where n is defined by Latency{2,1,0} */
#define HDSP_Latency1             (1<<2)  /* [ see above ] */
#define HDSP_Latency2             (1<<3)  /* [ see above ] */
#define HDSP_ClockModeMaster      (1<<4)  /* 1=Master, 0=Slave/Autosync */
#define HDSP_AudioInterruptEnable (1<<5)  /* what do you think ? */
#define HDSP_Frequency0           (1<<6)  /* 0=44.1kHz/88.2kHz/176.4kHz 1=48kHz/96kHz/192kHz */
#define HDSP_Frequency1           (1<<7)  /* 0=32kHz/64kHz/128kHz */
#define HDSP_DoubleSpeed          (1<<8)  /* 0=normal speed, 1=double speed */
#define HDSP_SPDIFProfessional    (1<<9)  /* 0=consumer, 1=professional */
#define HDSP_SPDIFEmphasis        (1<<10) /* 0=none, 1=on */
#define HDSP_SPDIFNonAudio        (1<<11) /* 0=off, 1=on */
#define HDSP_SPDIFOpticalOut      (1<<12) /* 1=use 1st ADAT connector for SPDIF, 0=do not */
#define HDSP_SyncRef2             (1<<13)
#define HDSP_SPDIFInputSelect0    (1<<14)
#define HDSP_SPDIFInputSelect1    (1<<15)
#define HDSP_SyncRef0             (1<<16)
#define HDSP_SyncRef1             (1<<17)
#define HDSP_AnalogExtensionBoard (1<<18) /* For H9632 cards */
#define HDSP_XLRBreakoutCable     (1<<20) /* For H9632 cards */
#define HDSP_Midi0InterruptEnable (1<<22)
#define HDSP_Midi1InterruptEnable (1<<23)
#define HDSP_LineOut              (1<<24)
#define HDSP_ADGain0		  (1<<25) /* From here : H9632 specific */
#define HDSP_ADGain1		  (1<<26)
#define HDSP_DAGain0		  (1<<27)
#define HDSP_DAGain1		  (1<<28)
#define HDSP_PhoneGain0		  (1<<29)
#define HDSP_PhoneGain1		  (1<<30)
#define HDSP_QuadSpeed	  	  (1<<31)

#define HDSP_ADGainMask       (HDSP_ADGain0|HDSP_ADGain1)
#define HDSP_ADGainMinus10dBV  HDSP_ADGainMask
#define HDSP_ADGainPlus4dBu   (HDSP_ADGain0)
#define HDSP_ADGainLowGain     0

#define HDSP_DAGainMask         (HDSP_DAGain0|HDSP_DAGain1)
#define HDSP_DAGainHighGain      HDSP_DAGainMask
#define HDSP_DAGainPlus4dBu     (HDSP_DAGain0)
#define HDSP_DAGainMinus10dBV    0

#define HDSP_PhoneGainMask      (HDSP_PhoneGain0|HDSP_PhoneGain1)
#define HDSP_PhoneGain0dB        HDSP_PhoneGainMask
#define HDSP_PhoneGainMinus6dB  (HDSP_PhoneGain0)
#define HDSP_PhoneGainMinus12dB  0

#define HDSP_LatencyMask    (HDSP_Latency0|HDSP_Latency1|HDSP_Latency2)
#define HDSP_FrequencyMask  (HDSP_Frequency0|HDSP_Frequency1|HDSP_DoubleSpeed|HDSP_QuadSpeed)

#define HDSP_SPDIFInputMask    (HDSP_SPDIFInputSelect0|HDSP_SPDIFInputSelect1)
#define HDSP_SPDIFInputADAT1    0
#define HDSP_SPDIFInputCoaxial (HDSP_SPDIFInputSelect0)
#define HDSP_SPDIFInputCdrom   (HDSP_SPDIFInputSelect1)
#define HDSP_SPDIFInputAES     (HDSP_SPDIFInputSelect0|HDSP_SPDIFInputSelect1)

#define HDSP_SyncRefMask        (HDSP_SyncRef0|HDSP_SyncRef1|HDSP_SyncRef2)
#define HDSP_SyncRef_ADAT1       0
#define HDSP_SyncRef_ADAT2      (HDSP_SyncRef0)
#define HDSP_SyncRef_ADAT3      (HDSP_SyncRef1)
#define HDSP_SyncRef_SPDIF      (HDSP_SyncRef0|HDSP_SyncRef1)
#define HDSP_SyncRef_WORD       (HDSP_SyncRef2)
#define HDSP_SyncRef_ADAT_SYNC  (HDSP_SyncRef0|HDSP_SyncRef2)

/* Sample Clock Sources */

#define HDSP_CLOCK_SOURCE_AUTOSYNC           0
#define HDSP_CLOCK_SOURCE_INTERNAL_32KHZ     1
#define HDSP_CLOCK_SOURCE_INTERNAL_44_1KHZ   2
#define HDSP_CLOCK_SOURCE_INTERNAL_48KHZ     3
#define HDSP_CLOCK_SOURCE_INTERNAL_64KHZ     4
#define HDSP_CLOCK_SOURCE_INTERNAL_88_2KHZ   5
#define HDSP_CLOCK_SOURCE_INTERNAL_96KHZ     6
#define HDSP_CLOCK_SOURCE_INTERNAL_128KHZ    7
#define HDSP_CLOCK_SOURCE_INTERNAL_176_4KHZ  8
#define HDSP_CLOCK_SOURCE_INTERNAL_192KHZ    9

/* Preferred sync reference choices - used by "pref_sync_ref" control switch */

#define HDSP_SYNC_FROM_WORD      0
#define HDSP_SYNC_FROM_SPDIF     1
#define HDSP_SYNC_FROM_ADAT1     2
#define HDSP_SYNC_FROM_ADAT_SYNC 3
#define HDSP_SYNC_FROM_ADAT2     4
#define HDSP_SYNC_FROM_ADAT3     5

/* SyncCheck status */

#define HDSP_SYNC_CHECK_NO_LOCK 0
#define HDSP_SYNC_CHECK_LOCK    1
#define HDSP_SYNC_CHECK_SYNC	2

/* AutoSync references - used by "autosync_ref" control switch */

#define HDSP_AUTOSYNC_FROM_WORD      0
#define HDSP_AUTOSYNC_FROM_ADAT_SYNC 1
#define HDSP_AUTOSYNC_FROM_SPDIF     2
#define HDSP_AUTOSYNC_FROM_NONE	     3
#define HDSP_AUTOSYNC_FROM_ADAT1     4
#define HDSP_AUTOSYNC_FROM_ADAT2     5
#define HDSP_AUTOSYNC_FROM_ADAT3     6

/* Possible sources of S/PDIF input */

#define HDSP_SPDIFIN_OPTICAL  0	/* optical  (ADAT1) */
#define HDSP_SPDIFIN_COAXIAL  1	/* coaxial (RCA) */
#define HDSP_SPDIFIN_INTERNAL 2	/* internal (CDROM) */
#define HDSP_SPDIFIN_AES      3 /* xlr for H9632 (AES)*/

#define HDSP_Frequency32KHz    HDSP_Frequency0
#define HDSP_Frequency44_1KHz  HDSP_Frequency1
#define HDSP_Frequency48KHz    (HDSP_Frequency1|HDSP_Frequency0)
#define HDSP_Frequency64KHz    (HDSP_DoubleSpeed|HDSP_Frequency0)
#define HDSP_Frequency88_2KHz  (HDSP_DoubleSpeed|HDSP_Frequency1)
#define HDSP_Frequency96KHz    (HDSP_DoubleSpeed|HDSP_Frequency1|HDSP_Frequency0)
/* For H9632 cards */
#define HDSP_Frequency128KHz   (HDSP_QuadSpeed|HDSP_DoubleSpeed|HDSP_Frequency0)
#define HDSP_Frequency176_4KHz (HDSP_QuadSpeed|HDSP_DoubleSpeed|HDSP_Frequency1)
#define HDSP_Frequency192KHz   (HDSP_QuadSpeed|HDSP_DoubleSpeed|HDSP_Frequency1|HDSP_Frequency0)
/* RME says n = 104857600000000, but in the windows MADI driver, I see:
	return 104857600000000 / rate; // 100 MHz
	return 110100480000000 / rate; // 105 MHz
*/
#define DDS_NUMERATOR 104857600000000ULL;  /*  =  2^20 * 10^8 */

#define hdsp_encode_latency(x)       (((x)<<1) & HDSP_LatencyMask)
#define hdsp_decode_latency(x)       (((x) & HDSP_LatencyMask)>>1)

#define hdsp_encode_spdif_in(x) (((x)&0x3)<<14)
#define hdsp_decode_spdif_in(x) (((x)>>14)&0x3)

/* Status Register bits */

#define HDSP_audioIRQPending    (1<<0)
#define HDSP_Lock2              (1<<1)     /* this is for Digiface and H9652 */
#define HDSP_spdifFrequency3	HDSP_Lock2 /* this is for H9632 only */
#define HDSP_Lock1              (1<<2)
#define HDSP_Lock0              (1<<3)
#define HDSP_SPDIFSync          (1<<4)
#define HDSP_TimecodeLock       (1<<5)
#define HDSP_BufferPositionMask 0x000FFC0 /* Bit 6..15 : h/w buffer pointer */
#define HDSP_Sync2              (1<<16)
#define HDSP_Sync1              (1<<17)
#define HDSP_Sync0              (1<<18)
#define HDSP_DoubleSpeedStatus  (1<<19)
#define HDSP_ConfigError        (1<<20)
#define HDSP_DllError           (1<<21)
#define HDSP_spdifFrequency0    (1<<22)
#define HDSP_spdifFrequency1    (1<<23)
#define HDSP_spdifFrequency2    (1<<24)
#define HDSP_SPDIFErrorFlag     (1<<25)
#define HDSP_BufferID           (1<<26)
#define HDSP_TimecodeSync       (1<<27)
#define HDSP_AEBO          	(1<<28) /* H9632 specific Analog Extension Boards */
#define HDSP_AEBI		(1<<29) /* 0 = present, 1 = absent */
#define HDSP_midi0IRQPending    (1<<30)
#define HDSP_midi1IRQPending    (1<<31)

#define HDSP_spdifFrequencyMask    (HDSP_spdifFrequency0|HDSP_spdifFrequency1|HDSP_spdifFrequency2)
#define HDSP_spdifFrequencyMask_9632 (HDSP_spdifFrequency0|\
				      HDSP_spdifFrequency1|\
				      HDSP_spdifFrequency2|\
				      HDSP_spdifFrequency3)

#define HDSP_spdifFrequency32KHz   (HDSP_spdifFrequency0)
#define HDSP_spdifFrequency44_1KHz (HDSP_spdifFrequency1)
#define HDSP_spdifFrequency48KHz   (HDSP_spdifFrequency0|HDSP_spdifFrequency1)

#define HDSP_spdifFrequency64KHz   (HDSP_spdifFrequency2)
#define HDSP_spdifFrequency88_2KHz (HDSP_spdifFrequency0|HDSP_spdifFrequency2)
#define HDSP_spdifFrequency96KHz   (HDSP_spdifFrequency2|HDSP_spdifFrequency1)

/* This is for H9632 cards */
#define HDSP_spdifFrequency128KHz   (HDSP_spdifFrequency0|\
				     HDSP_spdifFrequency1|\
				     HDSP_spdifFrequency2)
#define HDSP_spdifFrequency176_4KHz HDSP_spdifFrequency3
#define HDSP_spdifFrequency192KHz   (HDSP_spdifFrequency3|HDSP_spdifFrequency0)

/* Status2 Register bits */

#define HDSP_version0     (1<<0)
#define HDSP_version1     (1<<1)
#define HDSP_version2     (1<<2)
#define HDSP_wc_lock      (1<<3)
#define HDSP_wc_sync      (1<<4)
#define HDSP_inp_freq0    (1<<5)
#define HDSP_inp_freq1    (1<<6)
#define HDSP_inp_freq2    (1<<7)
#define HDSP_SelSyncRef0  (1<<8)
#define HDSP_SelSyncRef1  (1<<9)
#define HDSP_SelSyncRef2  (1<<10)

#define HDSP_wc_valid (HDSP_wc_lock|HDSP_wc_sync)

#define HDSP_systemFrequencyMask (HDSP_inp_freq0|HDSP_inp_freq1|HDSP_inp_freq2)
#define HDSP_systemFrequency32   (HDSP_inp_freq0)
#define HDSP_systemFrequency44_1 (HDSP_inp_freq1)
#define HDSP_systemFrequency48   (HDSP_inp_freq0|HDSP_inp_freq1)
#define HDSP_systemFrequency64   (HDSP_inp_freq2)
#define HDSP_systemFrequency88_2 (HDSP_inp_freq0|HDSP_inp_freq2)
#define HDSP_systemFrequency96   (HDSP_inp_freq1|HDSP_inp_freq2)
/* FIXME : more values for 9632 cards ? */

#define HDSP_SelSyncRefMask        (HDSP_SelSyncRef0|HDSP_SelSyncRef1|HDSP_SelSyncRef2)
#define HDSP_SelSyncRef_ADAT1      0
#define HDSP_SelSyncRef_ADAT2      (HDSP_SelSyncRef0)
#define HDSP_SelSyncRef_ADAT3      (HDSP_SelSyncRef1)
#define HDSP_SelSyncRef_SPDIF      (HDSP_SelSyncRef0|HDSP_SelSyncRef1)
#define HDSP_SelSyncRef_WORD       (HDSP_SelSyncRef2)
#define HDSP_SelSyncRef_ADAT_SYNC  (HDSP_SelSyncRef0|HDSP_SelSyncRef2)

/* Card state flags */

#define HDSP_InitializationComplete  (1<<0)
#define HDSP_FirmwareLoaded	     (1<<1)
#define HDSP_FirmwareCached	     (1<<2)

/* FIFO wait times, defined in terms of 1/10ths of msecs */

#define HDSP_LONG_WAIT	 5000
#define HDSP_SHORT_WAIT  30

#define UNITY_GAIN                       32768
#define MINUS_INFINITY_GAIN              0

/* the size of a substream (1 mono data stream) */

#define HDSP_CHANNEL_BUFFER_SAMPLES  (16*1024)
#define HDSP_CHANNEL_BUFFER_BYTES    (4*HDSP_CHANNEL_BUFFER_SAMPLES)

/* the size of the area we need to allocate for DMA transfers. the
   size is the same regardless of the number of channels - the
   Multiface still uses the same memory area.

   Note that we allocate 1 more channel than is apparently needed
   because the h/w seems to write 1 byte beyond the end of the last
   page. Sigh.
*/

#define HDSP_DMA_AREA_BYTES ((HDSP_MAX_CHANNELS+1) * HDSP_CHANNEL_BUFFER_BYTES)
#define HDSP_DMA_AREA_KILOBYTES (HDSP_DMA_AREA_BYTES/1024)

/* use hotplug firmware loader? */
#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
#if !defined(HDSP_USE_HWDEP_LOADER)
#define HDSP_FW_LOADER
#endif
#endif

struct hdsp_9632_meters {
    u32 input_peak[16];
    u32 playback_peak[16];
    u32 output_peak[16];
    u32 xxx_peak[16];
    u32 padding[64];
    u32 input_rms_low[16];
    u32 playback_rms_low[16];
    u32 output_rms_low[16];
    u32 xxx_rms_low[16];
    u32 input_rms_high[16];
    u32 playback_rms_high[16];
    u32 output_rms_high[16];
    u32 xxx_rms_high[16];
};

struct hdsp_midi {
    struct hdsp             *hdsp;
    int                      id;
    struct snd_rawmidi           *rmidi;
    struct snd_rawmidi_substream *input;
    struct snd_rawmidi_substream *output;
    char                     istimer; /* timer in use */
    struct timer_list	     timer;
    spinlock_t               lock;
    int			     pending;
};

struct hdsp {
	spinlock_t            lock;
	struct snd_pcm_substream *capture_substream;
	struct snd_pcm_substream *playback_substream;
        struct hdsp_midi      midi[2];
	struct tasklet_struct midi_tasklet;
	int		      use_midi_tasklet;
	int                   precise_ptr;
	u32                   control_register;	     /* cached value */
	u32                   control2_register;     /* cached value */
	u32                   creg_spdif;
	u32                   creg_spdif_stream;
	int                   clock_source_locked;
	char                 *card_name;	     /* digiface/multiface */
	enum HDSP_IO_Type     io_type;               /* ditto, but for code use */
        unsigned short        firmware_rev;
	unsigned short	      state;		     /* stores state bits */
	u32		      firmware_cache[24413]; /* this helps recover from accidental iobox power failure */
	size_t                period_bytes; 	     /* guess what this is */
	unsigned char	      max_channels;
	unsigned char	      qs_in_channels;	     /* quad speed mode for H9632 */
	unsigned char         ds_in_channels;
	unsigned char         ss_in_channels;	    /* different for multiface/digiface */
	unsigned char	      qs_out_channels;
	unsigned char         ds_out_channels;
	unsigned char         ss_out_channels;

	struct snd_dma_buffer capture_dma_buf;
	struct snd_dma_buffer playback_dma_buf;
	unsigned char        *capture_buffer;	    /* suitably aligned address */
	unsigned char        *playback_buffer;	    /* suitably aligned address */

	pid_t                 capture_pid;
	pid_t                 playback_pid;
	int                   running;
	int                   system_sample_rate;
	char                 *channel_map;
	int                   dev;
	int                   irq;
	unsigned long         port;
        void __iomem         *iobase;
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct snd_hwdep          *hwdep;
	struct pci_dev       *pci;
	struct snd_kcontrol *spdif_ctl;
        unsigned short        mixer_matrix[HDSP_MATRIX_MIXER_SIZE];
	unsigned int          dds_value; /* last value written to freq register */
};

/* These tables map the ALSA channels 1..N to the channels that we
   need to use in order to find the relevant channel buffer. RME
   refer to this kind of mapping as between "the ADAT channel and
   the DMA channel." We index it using the logical audio channel,
   and the value is the DMA channel (i.e. channel buffer number)
   where the data for that channel can be read/written from/to.
*/

static char channel_map_df_ss[HDSP_MAX_CHANNELS] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
	18, 19, 20, 21, 22, 23, 24, 25
};

static char channel_map_mf_ss[HDSP_MAX_CHANNELS] = { /* Multiface */
	/* Analog */
	0, 1, 2, 3, 4, 5, 6, 7,
	/* ADAT 2 */
	16, 17, 18, 19, 20, 21, 22, 23,
	/* SPDIF */
	24, 25,
	-1, -1, -1, -1, -1, -1, -1, -1
};

static char channel_map_ds[HDSP_MAX_CHANNELS] = {
	/* ADAT channels are remapped */
	1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23,
	/* channels 12 and 13 are S/PDIF */
	24, 25,
	/* others don't exist */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static char channel_map_H9632_ss[HDSP_MAX_CHANNELS] = {
	/* ADAT channels */
	0, 1, 2, 3, 4, 5, 6, 7,
	/* SPDIF */
	8, 9,
	/* Analog */
	10, 11,
	/* AO4S-192 and AI4S-192 extension boards */
	12, 13, 14, 15,
	/* others don't exist */
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1
};

static char channel_map_H9632_ds[HDSP_MAX_CHANNELS] = {
	/* ADAT */
	1, 3, 5, 7,
	/* SPDIF */
	8, 9,
	/* Analog */
	10, 11,
	/* AO4S-192 and AI4S-192 extension boards */
	12, 13, 14, 15,
	/* others don't exist */
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1
};

static char channel_map_H9632_qs[HDSP_MAX_CHANNELS] = {
	/* ADAT is disabled in this mode */
	/* SPDIF */
	8, 9,
	/* Analog */
	10, 11,
	/* AO4S-192 and AI4S-192 extension boards */
	12, 13, 14, 15,
	/* others don't exist */
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1
};

static int snd_hammerfall_get_buffer(struct pci_dev *pci, struct snd_dma_buffer *dmab, size_t size)
{
	dmab->dev.type = SNDRV_DMA_TYPE_DEV;
	dmab->dev.dev = snd_dma_pci_data(pci);
	if (snd_dma_get_reserved_buf(dmab, snd_dma_pci_buf_id(pci))) {
		if (dmab->bytes >= size)
			return 0;
	}
	if (snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, snd_dma_pci_data(pci),
				size, dmab) < 0)
		return -ENOMEM;
	return 0;
}

static void snd_hammerfall_free_buffer(struct snd_dma_buffer *dmab, struct pci_dev *pci)
{
	if (dmab->area) {
		dmab->dev.dev = NULL; /* make it anonymous */
		snd_dma_reserve_buf(dmab, snd_dma_pci_buf_id(pci));
	}
}


static struct pci_device_id snd_hdsp_ids[] = {
	{
		.vendor = PCI_VENDOR_ID_XILINX,
		.device = PCI_DEVICE_ID_XILINX_HAMMERFALL_DSP,
		.subvendor = PCI_ANY_ID,
		.subdevice = PCI_ANY_ID,
	}, /* RME Hammerfall-DSP */
	{ 0, },
};

MODULE_DEVICE_TABLE(pci, snd_hdsp_ids);

/* prototypes */
static int snd_hdsp_create_alsa_devices(struct snd_card *card, struct hdsp *hdsp);
static int snd_hdsp_create_pcm(struct snd_card *card, struct hdsp *hdsp);
static int snd_hdsp_enable_io (struct hdsp *hdsp);
static void snd_hdsp_initialize_midi_flush (struct hdsp *hdsp);
static void snd_hdsp_initialize_channels (struct hdsp *hdsp);
static int hdsp_fifo_wait(struct hdsp *hdsp, int count, int timeout);
static int hdsp_autosync_ref(struct hdsp *hdsp);
static int snd_hdsp_set_defaults(struct hdsp *hdsp);
static void snd_hdsp_9652_enable_mixer (struct hdsp *hdsp);

static int hdsp_playback_to_output_key (struct hdsp *hdsp, int in, int out)
{
	switch (hdsp->io_type) {
	case Multiface:
	case Digiface:
	default:
		if (hdsp->firmware_rev == 0xa)
			return (64 * out) + (32 + (in));
		else
			return (52 * out) + (26 + (in));
	case H9632:
		return (32 * out) + (16 + (in));
	case H9652:
		return (52 * out) + (26 + (in));
	}
}

static int hdsp_input_to_output_key (struct hdsp *hdsp, int in, int out)
{
	switch (hdsp->io_type) {
	case Multiface:
	case Digiface:
	default:
		if (hdsp->firmware_rev == 0xa)
			return (64 * out) + in;
		else
			return (52 * out) + in;
	case H9632:
		return (32 * out) + in;
	case H9652:
		return (52 * out) + in;
	}
}

static void hdsp_write(struct hdsp *hdsp, int reg, int val)
{
	writel(val, hdsp->iobase + reg);
}

static unsigned int hdsp_read(struct hdsp *hdsp, int reg)
{
	return readl (hdsp->iobase + reg);
}

static int hdsp_check_for_iobox (struct hdsp *hdsp)
{
	if (hdsp->io_type == H9652 || hdsp->io_type == H9632) return 0;
	if (hdsp_read (hdsp, HDSP_statusRegister) & HDSP_ConfigError) {
		snd_printk ("Hammerfall-DSP: no Digiface or Multiface connected!\n");
		hdsp->state &= ~HDSP_FirmwareLoaded;
		return -EIO;
	}
	return 0;
}

static int hdsp_wait_for_iobox(struct hdsp *hdsp, unsigned int loops,
			       unsigned int delay)
{
	unsigned int i;

	if (hdsp->io_type == H9652 || hdsp->io_type == H9632)
		return 0;

	for (i = 0; i != loops; ++i) {
		if (hdsp_read(hdsp, HDSP_statusRegister) & HDSP_ConfigError)
			msleep(delay);
		else {
			snd_printd("Hammerfall-DSP: iobox found after %ums!\n",
				   i * delay);
			return 0;
		}
	}

	snd_printk("Hammerfall-DSP: no Digiface or Multiface connected!\n");
	hdsp->state &= ~HDSP_FirmwareLoaded;
	return -EIO;
}

static int snd_hdsp_load_firmware_from_cache(struct hdsp *hdsp) {

	int i;
	unsigned long flags;

	if ((hdsp_read (hdsp, HDSP_statusRegister) & HDSP_DllError) != 0) {

		snd_printk ("Hammerfall-DSP: loading firmware\n");

		hdsp_write (hdsp, HDSP_control2Reg, HDSP_S_PROGRAM);
		hdsp_write (hdsp, HDSP_fifoData, 0);

		if (hdsp_fifo_wait (hdsp, 0, HDSP_LONG_WAIT)) {
			snd_printk ("Hammerfall-DSP: timeout waiting for download preparation\n");
			return -EIO;
		}

		hdsp_write (hdsp, HDSP_control2Reg, HDSP_S_LOAD);

		for (i = 0; i < 24413; ++i) {
			hdsp_write(hdsp, HDSP_fifoData, hdsp->firmware_cache[i]);
			if (hdsp_fifo_wait (hdsp, 127, HDSP_LONG_WAIT)) {
				snd_printk ("Hammerfall-DSP: timeout during firmware loading\n");
				return -EIO;
			}
		}

		ssleep(3);

		if (hdsp_fifo_wait (hdsp, 0, HDSP_LONG_WAIT)) {
			snd_printk ("Hammerfall-DSP: timeout at end of firmware loading\n");
		    	return -EIO;
		}

#ifdef SNDRV_BIG_ENDIAN
		hdsp->control2_register = HDSP_BIGENDIAN_MODE;
#else
		hdsp->control2_register = 0;
#endif
		hdsp_write (hdsp, HDSP_control2Reg, hdsp->control2_register);
		snd_printk ("Hammerfall-DSP: finished firmware loading\n");

	}
	if (hdsp->state & HDSP_InitializationComplete) {
		snd_printk(KERN_INFO "Hammerfall-DSP: firmware loaded from cache, restoring defaults\n");
		spin_lock_irqsave(&hdsp->lock, flags);
		snd_hdsp_set_defaults(hdsp);
		spin_unlock_irqrestore(&hdsp->lock, flags);
	}

	hdsp->state |= HDSP_FirmwareLoaded;

	return 0;
}

static int hdsp_get_iobox_version (struct hdsp *hdsp)
{
	if ((hdsp_read (hdsp, HDSP_statusRegister) & HDSP_DllError) != 0) {

		hdsp_write (hdsp, HDSP_control2Reg, HDSP_PROGRAM);
		hdsp_write (hdsp, HDSP_fifoData, 0);
		if (hdsp_fifo_wait (hdsp, 0, HDSP_SHORT_WAIT) < 0)
			return -EIO;

		hdsp_write (hdsp, HDSP_control2Reg, HDSP_S_LOAD);
		hdsp_write (hdsp, HDSP_fifoData, 0);

		if (hdsp_fifo_wait (hdsp, 0, HDSP_SHORT_WAIT)) {
			hdsp->io_type = Multiface;
			hdsp_write (hdsp, HDSP_control2Reg, HDSP_VERSION_BIT);
			hdsp_write (hdsp, HDSP_control2Reg, HDSP_S_LOAD);
			hdsp_fifo_wait (hdsp, 0, HDSP_SHORT_WAIT);
		} else {
			hdsp->io_type = Digiface;
		}
	} else {
		/* firmware was already loaded, get iobox type */
		if (hdsp_read(hdsp, HDSP_status2Register) & HDSP_version1)
			hdsp->io_type = Multiface;
		else
			hdsp->io_type = Digiface;
	}
	return 0;
}


#ifdef HDSP_FW_LOADER
static int hdsp_request_fw_loader(struct hdsp *hdsp);
#endif

static int hdsp_check_for_firmware (struct hdsp *hdsp, int load_on_demand)
{
	if (hdsp->io_type == H9652 || hdsp->io_type == H9632)
		return 0;
	if ((hdsp_read (hdsp, HDSP_statusRegister) & HDSP_DllError) != 0) {
		hdsp->state &= ~HDSP_FirmwareLoaded;
		if (! load_on_demand)
			return -EIO;
		snd_printk(KERN_ERR "Hammerfall-DSP: firmware not present.\n");
		/* try to load firmware */
		if (! (hdsp->state & HDSP_FirmwareCached)) {
#ifdef HDSP_FW_LOADER
			if (! hdsp_request_fw_loader(hdsp))
				return 0;
#endif
			snd_printk(KERN_ERR
				   "Hammerfall-DSP: No firmware loaded nor "
				   "cached, please upload firmware.\n");
			return -EIO;
		}
		if (snd_hdsp_load_firmware_from_cache(hdsp) != 0) {
			snd_printk(KERN_ERR
				   "Hammerfall-DSP: Firmware loading from "
				   "cache failed, please upload manually.\n");
			return -EIO;
		}
	}
	return 0;
}


static int hdsp_fifo_wait(struct hdsp *hdsp, int count, int timeout)
{
	int i;

	/* the fifoStatus registers reports on how many words
	   are available in the command FIFO.
	*/

	for (i = 0; i < timeout; i++) {

		if ((int)(hdsp_read (hdsp, HDSP_fifoStatus) & 0xff) <= count)
			return 0;

		/* not very friendly, but we only do this during a firmware
		   load and changing the mixer, so we just put up with it.
		*/

		udelay (100);
	}

	snd_printk ("Hammerfall-DSP: wait for FIFO status <= %d failed after %d iterations\n",
		    count, timeout);
	return -1;
}

static int hdsp_read_gain (struct hdsp *hdsp, unsigned int addr)
{
	if (addr >= HDSP_MATRIX_MIXER_SIZE)
		return 0;

	return hdsp->mixer_matrix[addr];
}

static int hdsp_write_gain(struct hdsp *hdsp, unsigned int addr, unsigned short data)
{
	unsigned int ad;

	if (addr >= HDSP_MATRIX_MIXER_SIZE)
		return -1;

	if (hdsp->io_type == H9652 || hdsp->io_type == H9632) {

		/* from martin bjornsen:

		   "You can only write dwords to the
		   mixer memory which contain two
		   mixer values in the low and high
		   word. So if you want to change
		   value 0 you have to read value 1
		   from the cache and write both to
		   the first dword in the mixer
		   memory."
		*/

		if (hdsp->io_type == H9632 && addr >= 512)
			return 0;

		if (hdsp->io_type == H9652 && addr >= 1352)
			return 0;

		hdsp->mixer_matrix[addr] = data;


		/* `addr' addresses a 16-bit wide address, but
		   the address space accessed via hdsp_write
		   uses byte offsets. put another way, addr
		   varies from 0 to 1351, but to access the
		   corresponding memory location, we need
		   to access 0 to 2703 ...
		*/
		ad = addr/2;

		hdsp_write (hdsp, 4096 + (ad*4),
			    (hdsp->mixer_matrix[(addr&0x7fe)+1] << 16) +
			    hdsp->mixer_matrix[addr&0x7fe]);

		return 0;

	} else {

		ad = (addr << 16) + data;

		if (hdsp_fifo_wait(hdsp, 127, HDSP_LONG_WAIT))
			return -1;

		hdsp_write (hdsp, HDSP_fifoData, ad);
		hdsp->mixer_matrix[addr] = data;

	}

	return 0;
}

static int snd_hdsp_use_is_exclusive(struct hdsp *hdsp)
{
	unsigned long flags;
	int ret = 1;

	spin_lock_irqsave(&hdsp->lock, flags);
	if ((hdsp->playback_pid != hdsp->capture_pid) &&
	    (hdsp->playback_pid >= 0) && (hdsp->capture_pid >= 0))
		ret = 0;
	spin_unlock_irqrestore(&hdsp->lock, flags);
	return ret;
}

static int hdsp_spdif_sample_rate(struct hdsp *hdsp)
{
	unsigned int status = hdsp_read(hdsp, HDSP_statusRegister);
	unsigned int rate_bits = (status & HDSP_spdifFrequencyMask);

	/* For the 9632, the mask is different */
	if (hdsp->io_type == H9632)
		 rate_bits = (status & HDSP_spdifFrequencyMask_9632);

	if (status & HDSP_SPDIFErrorFlag)
		return 0;

	switch (rate_bits) {
	case HDSP_spdifFrequency32KHz: return 32000;
	case HDSP_spdifFrequency44_1KHz: return 44100;
	case HDSP_spdifFrequency48KHz: return 48000;
	case HDSP_spdifFrequency64KHz: return 64000;
	case HDSP_spdifFrequency88_2KHz: return 88200;
	case HDSP_spdifFrequency96KHz: return 96000;
	case HDSP_spdifFrequency128KHz:
		if (hdsp->io_type == H9632) return 128000;
		break;
	case HDSP_spdifFrequency176_4KHz:
		if (hdsp->io_type == H9632) return 176400;
		break;
	case HDSP_spdifFrequency192KHz:
		if (hdsp->io_type == H9632) return 192000;
		break;
	default:
		break;
	}
	snd_printk ("Hammerfall-DSP: unknown spdif frequency status; bits = 0x%x, status = 0x%x\n", rate_bits, status);
	return 0;
}

static int hdsp_external_sample_rate(struct hdsp *hdsp)
{
	unsigned int status2 = hdsp_read(hdsp, HDSP_status2Register);
	unsigned int rate_bits = status2 & HDSP_systemFrequencyMask;

	/* For the 9632 card, there seems to be no bit for indicating external
	 * sample rate greater than 96kHz. The card reports the corresponding
	 * single speed. So the best means seems to get spdif rate when
	 * autosync reference is spdif */
	if (hdsp->io_type == H9632 &&
	    hdsp_autosync_ref(hdsp) == HDSP_AUTOSYNC_FROM_SPDIF)
		 return hdsp_spdif_sample_rate(hdsp);

	switch (rate_bits) {
	case HDSP_systemFrequency32:   return 32000;
	case HDSP_systemFrequency44_1: return 44100;
	case HDSP_systemFrequency48:   return 48000;
	case HDSP_systemFrequency64:   return 64000;
	case HDSP_systemFrequency88_2: return 88200;
	case HDSP_systemFrequency96:   return 96000;
	default:
		return 0;
	}
}

static void hdsp_compute_period_size(struct hdsp *hdsp)
{
	hdsp->period_bytes = 1 << ((hdsp_decode_latency(hdsp->control_register) + 8));
}

static snd_pcm_uframes_t hdsp_hw_pointer(struct hdsp *hdsp)
{
	int position;

	position = hdsp_read(hdsp, HDSP_statusRegister);

	if (!hdsp->precise_ptr)
		return (position & HDSP_BufferID) ? (hdsp->period_bytes / 4) : 0;

	position &= HDSP_BufferPositionMask;
	position /= 4;
	position &= (hdsp->period_bytes/2) - 1;
	return position;
}

static void hdsp_reset_hw_pointer(struct hdsp *hdsp)
{
	hdsp_write (hdsp, HDSP_resetPointer, 0);
	if (hdsp->io_type == H9632 && hdsp->firmware_rev >= 152)
		/* HDSP_resetPointer = HDSP_freqReg, which is strange and
		 * requires (?) to write again DDS value after a reset pointer
		 * (at least, it works like this) */
		hdsp_write (hdsp, HDSP_freqReg, hdsp->dds_value);
}

static void hdsp_start_audio(struct hdsp *s)
{
	s->control_register |= (HDSP_AudioInterruptEnable | HDSP_Start);
	hdsp_write(s, HDSP_controlRegister, s->control_register);
}

static void hdsp_stop_audio(struct hdsp *s)
{
	s->control_register &= ~(HDSP_Start | HDSP_AudioInterruptEnable);
	hdsp_write(s, HDSP_controlRegister, s->control_register);
}

static void hdsp_silence_playback(struct hdsp *hdsp)
{
	memset(hdsp->playback_buffer, 0, HDSP_DMA_AREA_BYTES);
}

static int hdsp_set_interrupt_interval(struct hdsp *s, unsigned int frames)
{
	int n;

	spin_lock_irq(&s->lock);

	frames >>= 7;
	n = 0;
	while (frames) {
		n++;
		frames >>= 1;
	}

	s->control_register &= ~HDSP_LatencyMask;
	s->control_register |= hdsp_encode_latency(n);

	hdsp_write(s, HDSP_controlRegister, s->control_register);

	hdsp_compute_period_size(s);

	spin_unlock_irq(&s->lock);

	return 0;
}

static void hdsp_set_dds_value(struct hdsp *hdsp, int rate)
{
	u64 n;

	if (rate >= 112000)
		rate /= 4;
	else if (rate >= 56000)
		rate /= 2;

	n = DDS_NUMERATOR;
	n = div_u64(n, rate);
	/* n should be less than 2^32 for being written to FREQ register */
	snd_BUG_ON(n >> 32);
	/* HDSP_freqReg and HDSP_resetPointer are the same, so keep the DDS
	   value to write it after a reset */
	hdsp->dds_value = n;
	hdsp_write(hdsp, HDSP_freqReg, hdsp->dds_value);
}

static int hdsp_set_rate(struct hdsp *hdsp, int rate, int called_internally)
{
	int reject_if_open = 0;
	int current_rate;
	int rate_bits;

	/* ASSUMPTION: hdsp->lock is either held, or
	   there is no need for it (e.g. during module
	   initialization).
	*/

	if (!(hdsp->control_register & HDSP_ClockModeMaster)) {
		if (called_internally) {
			/* request from ctl or card initialization */
			snd_printk(KERN_ERR "Hammerfall-DSP: device is not running as a clock master: cannot set sample rate.\n");
			return -1;
		} else {
			/* hw_param request while in AutoSync mode */
			int external_freq = hdsp_external_sample_rate(hdsp);
			int spdif_freq = hdsp_spdif_sample_rate(hdsp);

			if ((spdif_freq == external_freq*2) && (hdsp_autosync_ref(hdsp) >= HDSP_AUTOSYNC_FROM_ADAT1))
				snd_printk(KERN_INFO "Hammerfall-DSP: Detected ADAT in double speed mode\n");
			else if (hdsp->io_type == H9632 && (spdif_freq == external_freq*4) && (hdsp_autosync_ref(hdsp) >= HDSP_AUTOSYNC_FROM_ADAT1))
				snd_printk(KERN_INFO "Hammerfall-DSP: Detected ADAT in quad speed mode\n");
			else if (rate != external_freq) {
				snd_printk(KERN_INFO "Hammerfall-DSP: No AutoSync source for requested rate\n");
				return -1;
			}
		}
	}

	current_rate = hdsp->system_sample_rate;

	/* Changing from a "single speed" to a "double speed" rate is
	   not allowed if any substreams are open. This is because
	   such a change causes a shift in the location of
	   the DMA buffers and a reduction in the number of available
	   buffers.

	   Note that a similar but essentially insoluble problem
	   exists for externally-driven rate changes. All we can do
	   is to flag rate changes in the read/write routines.  */

	if (rate > 96000 && hdsp->io_type != H9632)
		return -EINVAL;

	switch (rate) {
	case 32000:
		if (current_rate > 48000)
			reject_if_open = 1;
		rate_bits = HDSP_Frequency32KHz;
		break;
	case 44100:
		if (current_rate > 48000)
			reject_if_open = 1;
		rate_bits = HDSP_Frequency44_1KHz;
		break;
	case 48000:
		if (current_rate > 48000)
			reject_if_open = 1;
		rate_bits = HDSP_Frequency48KHz;
		break;
	case 64000:
		if (current_rate <= 48000 || current_rate > 96000)
			reject_if_open = 1;
		rate_bits = HDSP_Frequency64KHz;
		break;
	case 88200:
		if (current_rate <= 48000 || current_rate > 96000)
			reject_if_open = 1;
		rate_bits = HDSP_Frequency88_2KHz;
		break;
	case 96000:
		if (current_rate <= 48000 || current_rate > 96000)
			reject_if_open = 1;
		rate_bits = HDSP_Frequency96KHz;
		break;
	case 128000:
		if (current_rate < 128000)
			reject_if_open = 1;
		rate_bits = HDSP_Frequency128KHz;
		break;
	case 176400:
		if (current_rate < 128000)
			reject_if_open = 1;
		rate_bits = HDSP_Frequency176_4KHz;
		break;
	case 192000:
		if (current_rate < 128000)
			reject_if_open = 1;
		rate_bits = HDSP_Frequency192KHz;
		break;
	default:
		return -EINVAL;
	}

	if (reject_if_open && (hdsp->capture_pid >= 0 || hdsp->playback_pid >= 0)) {
		snd_printk ("Hammerfall-DSP: cannot change speed mode (capture PID = %d, playback PID = %d)\n",
			    hdsp->capture_pid,
			    hdsp->playback_pid);
		return -EBUSY;
	}

	hdsp->control_register &= ~HDSP_FrequencyMask;
	hdsp->control_register |= rate_bits;
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);

	/* For HDSP9632 rev 152, need to set DDS value in FREQ register */
	if (hdsp->io_type == H9632 && hdsp->firmware_rev >= 152)
		hdsp_set_dds_value(hdsp, rate);

	if (rate >= 128000) {
		hdsp->channel_map = channel_map_H9632_qs;
	} else if (rate > 48000) {
		if (hdsp->io_type == H9632)
			hdsp->channel_map = channel_map_H9632_ds;
		else
			hdsp->channel_map = channel_map_ds;
	} else {
		switch (hdsp->io_type) {
		case Multiface:
			hdsp->channel_map = channel_map_mf_ss;
			break;
		case Digiface:
		case H9652:
			hdsp->channel_map = channel_map_df_ss;
			break;
		case H9632:
			hdsp->channel_map = channel_map_H9632_ss;
			break;
		default:
			/* should never happen */
			break;
		}
	}

	hdsp->system_sample_rate = rate;

	return 0;
}

/*----------------------------------------------------------------------------
   MIDI
  ----------------------------------------------------------------------------*/

static unsigned char snd_hdsp_midi_read_byte (struct hdsp *hdsp, int id)
{
	/* the hardware already does the relevant bit-mask with 0xff */
	if (id)
		return hdsp_read(hdsp, HDSP_midiDataIn1);
	else
		return hdsp_read(hdsp, HDSP_midiDataIn0);
}

static void snd_hdsp_midi_write_byte (struct hdsp *hdsp, int id, int val)
{
	/* the hardware already does the relevant bit-mask with 0xff */
	if (id)
		hdsp_write(hdsp, HDSP_midiDataOut1, val);
	else
		hdsp_write(hdsp, HDSP_midiDataOut0, val);
}

static int snd_hdsp_midi_input_available (struct hdsp *hdsp, int id)
{
	if (id)
		return (hdsp_read(hdsp, HDSP_midiStatusIn1) & 0xff);
	else
		return (hdsp_read(hdsp, HDSP_midiStatusIn0) & 0xff);
}

static int snd_hdsp_midi_output_possible (struct hdsp *hdsp, int id)
{
	int fifo_bytes_used;

	if (id)
		fifo_bytes_used = hdsp_read(hdsp, HDSP_midiStatusOut1) & 0xff;
	else
		fifo_bytes_used = hdsp_read(hdsp, HDSP_midiStatusOut0) & 0xff;

	if (fifo_bytes_used < 128)
		return  128 - fifo_bytes_used;
	else
		return 0;
}

static void snd_hdsp_flush_midi_input (struct hdsp *hdsp, int id)
{
	while (snd_hdsp_midi_input_available (hdsp, id))
		snd_hdsp_midi_read_byte (hdsp, id);
}

static int snd_hdsp_midi_output_write (struct hdsp_midi *hmidi)
{
	unsigned long flags;
	int n_pending;
	int to_write;
	int i;
	unsigned char buf[128];

	/* Output is not interrupt driven */

	spin_lock_irqsave (&hmidi->lock, flags);
	if (hmidi->output) {
		if (!snd_rawmidi_transmit_empty (hmidi->output)) {
			if ((n_pending = snd_hdsp_midi_output_possible (hmidi->hdsp, hmidi->id)) > 0) {
				if (n_pending > (int)sizeof (buf))
					n_pending = sizeof (buf);

				if ((to_write = snd_rawmidi_transmit (hmidi->output, buf, n_pending)) > 0) {
					for (i = 0; i < to_write; ++i)
						snd_hdsp_midi_write_byte (hmidi->hdsp, hmidi->id, buf[i]);
				}
			}
		}
	}
	spin_unlock_irqrestore (&hmidi->lock, flags);
	return 0;
}

static int snd_hdsp_midi_input_read (struct hdsp_midi *hmidi)
{
	unsigned char buf[128]; /* this buffer is designed to match the MIDI input FIFO size */
	unsigned long flags;
	int n_pending;
	int i;

	spin_lock_irqsave (&hmidi->lock, flags);
	if ((n_pending = snd_hdsp_midi_input_available (hmidi->hdsp, hmidi->id)) > 0) {
		if (hmidi->input) {
			if (n_pending > (int)sizeof (buf))
				n_pending = sizeof (buf);
			for (i = 0; i < n_pending; ++i)
				buf[i] = snd_hdsp_midi_read_byte (hmidi->hdsp, hmidi->id);
			if (n_pending)
				snd_rawmidi_receive (hmidi->input, buf, n_pending);
		} else {
			/* flush the MIDI input FIFO */
			while (--n_pending)
				snd_hdsp_midi_read_byte (hmidi->hdsp, hmidi->id);
		}
	}
	hmidi->pending = 0;
	if (hmidi->id)
		hmidi->hdsp->control_register |= HDSP_Midi1InterruptEnable;
	else
		hmidi->hdsp->control_register |= HDSP_Midi0InterruptEnable;
	hdsp_write(hmidi->hdsp, HDSP_controlRegister, hmidi->hdsp->control_register);
	spin_unlock_irqrestore (&hmidi->lock, flags);
	return snd_hdsp_midi_output_write (hmidi);
}

static void snd_hdsp_midi_input_trigger(struct snd_rawmidi_substream *substream, int up)
{
	struct hdsp *hdsp;
	struct hdsp_midi *hmidi;
	unsigned long flags;
	u32 ie;

	hmidi = (struct hdsp_midi *) substream->rmidi->private_data;
	hdsp = hmidi->hdsp;
	ie = hmidi->id ? HDSP_Midi1InterruptEnable : HDSP_Midi0InterruptEnable;
	spin_lock_irqsave (&hdsp->lock, flags);
	if (up) {
		if (!(hdsp->control_register & ie)) {
			snd_hdsp_flush_midi_input (hdsp, hmidi->id);
			hdsp->control_register |= ie;
		}
	} else {
		hdsp->control_register &= ~ie;
		tasklet_kill(&hdsp->midi_tasklet);
	}

	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	spin_unlock_irqrestore (&hdsp->lock, flags);
}

static void snd_hdsp_midi_output_timer(unsigned long data)
{
	struct hdsp_midi *hmidi = (struct hdsp_midi *) data;
	unsigned long flags;

	snd_hdsp_midi_output_write(hmidi);
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

static void snd_hdsp_midi_output_trigger(struct snd_rawmidi_substream *substream, int up)
{
	struct hdsp_midi *hmidi;
	unsigned long flags;

	hmidi = (struct hdsp_midi *) substream->rmidi->private_data;
	spin_lock_irqsave (&hmidi->lock, flags);
	if (up) {
		if (!hmidi->istimer) {
			init_timer(&hmidi->timer);
			hmidi->timer.function = snd_hdsp_midi_output_timer;
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
		snd_hdsp_midi_output_write(hmidi);
}

static int snd_hdsp_midi_input_open(struct snd_rawmidi_substream *substream)
{
	struct hdsp_midi *hmidi;

	hmidi = (struct hdsp_midi *) substream->rmidi->private_data;
	spin_lock_irq (&hmidi->lock);
	snd_hdsp_flush_midi_input (hmidi->hdsp, hmidi->id);
	hmidi->input = substream;
	spin_unlock_irq (&hmidi->lock);

	return 0;
}

static int snd_hdsp_midi_output_open(struct snd_rawmidi_substream *substream)
{
	struct hdsp_midi *hmidi;

	hmidi = (struct hdsp_midi *) substream->rmidi->private_data;
	spin_lock_irq (&hmidi->lock);
	hmidi->output = substream;
	spin_unlock_irq (&hmidi->lock);

	return 0;
}

static int snd_hdsp_midi_input_close(struct snd_rawmidi_substream *substream)
{
	struct hdsp_midi *hmidi;

	snd_hdsp_midi_input_trigger (substream, 0);

	hmidi = (struct hdsp_midi *) substream->rmidi->private_data;
	spin_lock_irq (&hmidi->lock);
	hmidi->input = NULL;
	spin_unlock_irq (&hmidi->lock);

	return 0;
}

static int snd_hdsp_midi_output_close(struct snd_rawmidi_substream *substream)
{
	struct hdsp_midi *hmidi;

	snd_hdsp_midi_output_trigger (substream, 0);

	hmidi = (struct hdsp_midi *) substream->rmidi->private_data;
	spin_lock_irq (&hmidi->lock);
	hmidi->output = NULL;
	spin_unlock_irq (&hmidi->lock);

	return 0;
}

static struct snd_rawmidi_ops snd_hdsp_midi_output =
{
	.open =		snd_hdsp_midi_output_open,
	.close =	snd_hdsp_midi_output_close,
	.trigger =	snd_hdsp_midi_output_trigger,
};

static struct snd_rawmidi_ops snd_hdsp_midi_input =
{
	.open =		snd_hdsp_midi_input_open,
	.close =	snd_hdsp_midi_input_close,
	.trigger =	snd_hdsp_midi_input_trigger,
};

static int snd_hdsp_create_midi (struct snd_card *card, struct hdsp *hdsp, int id)
{
	char buf[32];

	hdsp->midi[id].id = id;
	hdsp->midi[id].rmidi = NULL;
	hdsp->midi[id].input = NULL;
	hdsp->midi[id].output = NULL;
	hdsp->midi[id].hdsp = hdsp;
	hdsp->midi[id].istimer = 0;
	hdsp->midi[id].pending = 0;
	spin_lock_init (&hdsp->midi[id].lock);

	sprintf (buf, "%s MIDI %d", card->shortname, id+1);
	if (snd_rawmidi_new (card, buf, id, 1, 1, &hdsp->midi[id].rmidi) < 0)
		return -1;

	sprintf(hdsp->midi[id].rmidi->name, "HDSP MIDI %d", id+1);
	hdsp->midi[id].rmidi->private_data = &hdsp->midi[id];

	snd_rawmidi_set_ops (hdsp->midi[id].rmidi, SNDRV_RAWMIDI_STREAM_OUTPUT, &snd_hdsp_midi_output);
	snd_rawmidi_set_ops (hdsp->midi[id].rmidi, SNDRV_RAWMIDI_STREAM_INPUT, &snd_hdsp_midi_input);

	hdsp->midi[id].rmidi->info_flags |= SNDRV_RAWMIDI_INFO_OUTPUT |
		SNDRV_RAWMIDI_INFO_INPUT |
		SNDRV_RAWMIDI_INFO_DUPLEX;

	return 0;
}

/*-----------------------------------------------------------------------------
  Control Interface
  ----------------------------------------------------------------------------*/

static u32 snd_hdsp_convert_from_aes(struct snd_aes_iec958 *aes)
{
	u32 val = 0;
	val |= (aes->status[0] & IEC958_AES0_PROFESSIONAL) ? HDSP_SPDIFProfessional : 0;
	val |= (aes->status[0] & IEC958_AES0_NONAUDIO) ? HDSP_SPDIFNonAudio : 0;
	if (val & HDSP_SPDIFProfessional)
		val |= (aes->status[0] & IEC958_AES0_PRO_EMPHASIS_5015) ? HDSP_SPDIFEmphasis : 0;
	else
		val |= (aes->status[0] & IEC958_AES0_CON_EMPHASIS_5015) ? HDSP_SPDIFEmphasis : 0;
	return val;
}

static void snd_hdsp_convert_to_aes(struct snd_aes_iec958 *aes, u32 val)
{
	aes->status[0] = ((val & HDSP_SPDIFProfessional) ? IEC958_AES0_PROFESSIONAL : 0) |
			 ((val & HDSP_SPDIFNonAudio) ? IEC958_AES0_NONAUDIO : 0);
	if (val & HDSP_SPDIFProfessional)
		aes->status[0] |= (val & HDSP_SPDIFEmphasis) ? IEC958_AES0_PRO_EMPHASIS_5015 : 0;
	else
		aes->status[0] |= (val & HDSP_SPDIFEmphasis) ? IEC958_AES0_CON_EMPHASIS_5015 : 0;
}

static int snd_hdsp_control_spdif_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int snd_hdsp_control_spdif_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	snd_hdsp_convert_to_aes(&ucontrol->value.iec958, hdsp->creg_spdif);
	return 0;
}

static int snd_hdsp_control_spdif_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	u32 val;

	val = snd_hdsp_convert_from_aes(&ucontrol->value.iec958);
	spin_lock_irq(&hdsp->lock);
	change = val != hdsp->creg_spdif;
	hdsp->creg_spdif = val;
	spin_unlock_irq(&hdsp->lock);
	return change;
}

static int snd_hdsp_control_spdif_stream_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int snd_hdsp_control_spdif_stream_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	snd_hdsp_convert_to_aes(&ucontrol->value.iec958, hdsp->creg_spdif_stream);
	return 0;
}

static int snd_hdsp_control_spdif_stream_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	u32 val;

	val = snd_hdsp_convert_from_aes(&ucontrol->value.iec958);
	spin_lock_irq(&hdsp->lock);
	change = val != hdsp->creg_spdif_stream;
	hdsp->creg_spdif_stream = val;
	hdsp->control_register &= ~(HDSP_SPDIFProfessional | HDSP_SPDIFNonAudio | HDSP_SPDIFEmphasis);
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register |= val);
	spin_unlock_irq(&hdsp->lock);
	return change;
}

static int snd_hdsp_control_spdif_mask_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int snd_hdsp_control_spdif_mask_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.iec958.status[0] = kcontrol->private_value;
	return 0;
}

#define HDSP_SPDIF_IN(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdsp_info_spdif_in, \
  .get = snd_hdsp_get_spdif_in, \
  .put = snd_hdsp_put_spdif_in }

static unsigned int hdsp_spdif_in(struct hdsp *hdsp)
{
	return hdsp_decode_spdif_in(hdsp->control_register & HDSP_SPDIFInputMask);
}

static int hdsp_set_spdif_input(struct hdsp *hdsp, int in)
{
	hdsp->control_register &= ~HDSP_SPDIFInputMask;
	hdsp->control_register |= hdsp_encode_spdif_in(in);
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	return 0;
}

static int snd_hdsp_info_spdif_in(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[4] = {"Optical", "Coaxial", "Internal", "AES"};
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = ((hdsp->io_type == H9632) ? 4 : 3);
	if (uinfo->value.enumerated.item > ((hdsp->io_type == H9632) ? 3 : 2))
		uinfo->value.enumerated.item = ((hdsp->io_type == H9632) ? 3 : 2);
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_hdsp_get_spdif_in(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp_spdif_in(hdsp);
	return 0;
}

static int snd_hdsp_put_spdif_in(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.enumerated.item[0] % ((hdsp->io_type == H9632) ? 4 : 3);
	spin_lock_irq(&hdsp->lock);
	change = val != hdsp_spdif_in(hdsp);
	if (change)
		hdsp_set_spdif_input(hdsp, val);
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define HDSP_SPDIF_OUT(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
  .info = snd_hdsp_info_spdif_bits, \
  .get = snd_hdsp_get_spdif_out, .put = snd_hdsp_put_spdif_out }

static int hdsp_spdif_out(struct hdsp *hdsp)
{
	return (hdsp->control_register & HDSP_SPDIFOpticalOut) ? 1 : 0;
}

static int hdsp_set_spdif_output(struct hdsp *hdsp, int out)
{
	if (out)
		hdsp->control_register |= HDSP_SPDIFOpticalOut;
	else
		hdsp->control_register &= ~HDSP_SPDIFOpticalOut;
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	return 0;
}

#define snd_hdsp_info_spdif_bits	snd_ctl_boolean_mono_info

static int snd_hdsp_get_spdif_out(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = hdsp_spdif_out(hdsp);
	return 0;
}

static int snd_hdsp_put_spdif_out(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdsp->lock);
	change = (int)val != hdsp_spdif_out(hdsp);
	hdsp_set_spdif_output(hdsp, val);
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define HDSP_SPDIF_PROFESSIONAL(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
  .info = snd_hdsp_info_spdif_bits, \
  .get = snd_hdsp_get_spdif_professional, .put = snd_hdsp_put_spdif_professional }

static int hdsp_spdif_professional(struct hdsp *hdsp)
{
	return (hdsp->control_register & HDSP_SPDIFProfessional) ? 1 : 0;
}

static int hdsp_set_spdif_professional(struct hdsp *hdsp, int val)
{
	if (val)
		hdsp->control_register |= HDSP_SPDIFProfessional;
	else
		hdsp->control_register &= ~HDSP_SPDIFProfessional;
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	return 0;
}

static int snd_hdsp_get_spdif_professional(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = hdsp_spdif_professional(hdsp);
	return 0;
}

static int snd_hdsp_put_spdif_professional(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdsp->lock);
	change = (int)val != hdsp_spdif_professional(hdsp);
	hdsp_set_spdif_professional(hdsp, val);
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define HDSP_SPDIF_EMPHASIS(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
  .info = snd_hdsp_info_spdif_bits, \
  .get = snd_hdsp_get_spdif_emphasis, .put = snd_hdsp_put_spdif_emphasis }

static int hdsp_spdif_emphasis(struct hdsp *hdsp)
{
	return (hdsp->control_register & HDSP_SPDIFEmphasis) ? 1 : 0;
}

static int hdsp_set_spdif_emphasis(struct hdsp *hdsp, int val)
{
	if (val)
		hdsp->control_register |= HDSP_SPDIFEmphasis;
	else
		hdsp->control_register &= ~HDSP_SPDIFEmphasis;
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	return 0;
}

static int snd_hdsp_get_spdif_emphasis(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = hdsp_spdif_emphasis(hdsp);
	return 0;
}

static int snd_hdsp_put_spdif_emphasis(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdsp->lock);
	change = (int)val != hdsp_spdif_emphasis(hdsp);
	hdsp_set_spdif_emphasis(hdsp, val);
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define HDSP_SPDIF_NON_AUDIO(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
  .info = snd_hdsp_info_spdif_bits, \
  .get = snd_hdsp_get_spdif_nonaudio, .put = snd_hdsp_put_spdif_nonaudio }

static int hdsp_spdif_nonaudio(struct hdsp *hdsp)
{
	return (hdsp->control_register & HDSP_SPDIFNonAudio) ? 1 : 0;
}

static int hdsp_set_spdif_nonaudio(struct hdsp *hdsp, int val)
{
	if (val)
		hdsp->control_register |= HDSP_SPDIFNonAudio;
	else
		hdsp->control_register &= ~HDSP_SPDIFNonAudio;
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	return 0;
}

static int snd_hdsp_get_spdif_nonaudio(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = hdsp_spdif_nonaudio(hdsp);
	return 0;
}

static int snd_hdsp_put_spdif_nonaudio(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdsp->lock);
	change = (int)val != hdsp_spdif_nonaudio(hdsp);
	hdsp_set_spdif_nonaudio(hdsp, val);
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define HDSP_SPDIF_SAMPLE_RATE(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ, \
  .info = snd_hdsp_info_spdif_sample_rate, \
  .get = snd_hdsp_get_spdif_sample_rate \
}

static int snd_hdsp_info_spdif_sample_rate(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = {"32000", "44100", "48000", "64000", "88200", "96000", "None", "128000", "176400", "192000"};
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = (hdsp->io_type == H9632) ? 10 : 7;
	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item = uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_hdsp_get_spdif_sample_rate(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	switch (hdsp_spdif_sample_rate(hdsp)) {
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
		ucontrol->value.enumerated.item[0] = 7;
		break;
	case 176400:
		ucontrol->value.enumerated.item[0] = 8;
		break;
	case 192000:
		ucontrol->value.enumerated.item[0] = 9;
		break;
	default:
		ucontrol->value.enumerated.item[0] = 6;
	}
	return 0;
}

#define HDSP_SYSTEM_SAMPLE_RATE(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ, \
  .info = snd_hdsp_info_system_sample_rate, \
  .get = snd_hdsp_get_system_sample_rate \
}

static int snd_hdsp_info_system_sample_rate(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	return 0;
}

static int snd_hdsp_get_system_sample_rate(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp->system_sample_rate;
	return 0;
}

#define HDSP_AUTOSYNC_SAMPLE_RATE(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ, \
  .info = snd_hdsp_info_autosync_sample_rate, \
  .get = snd_hdsp_get_autosync_sample_rate \
}

static int snd_hdsp_info_autosync_sample_rate(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	static char *texts[] = {"32000", "44100", "48000", "64000", "88200", "96000", "None", "128000", "176400", "192000"};
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = (hdsp->io_type == H9632) ? 10 : 7 ;
	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item = uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_hdsp_get_autosync_sample_rate(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	switch (hdsp_external_sample_rate(hdsp)) {
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
		ucontrol->value.enumerated.item[0] = 7;
		break;
	case 176400:
		ucontrol->value.enumerated.item[0] = 8;
		break;
	case 192000:
		ucontrol->value.enumerated.item[0] = 9;
		break;
	default:
		ucontrol->value.enumerated.item[0] = 6;
	}
	return 0;
}

#define HDSP_SYSTEM_CLOCK_MODE(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ, \
  .info = snd_hdsp_info_system_clock_mode, \
  .get = snd_hdsp_get_system_clock_mode \
}

static int hdsp_system_clock_mode(struct hdsp *hdsp)
{
	if (hdsp->control_register & HDSP_ClockModeMaster)
		return 0;
	else if (hdsp_external_sample_rate(hdsp) != hdsp->system_sample_rate)
			return 0;
	return 1;
}

static int snd_hdsp_info_system_clock_mode(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = {"Master", "Slave" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 2;
	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item = uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_hdsp_get_system_clock_mode(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp_system_clock_mode(hdsp);
	return 0;
}

#define HDSP_CLOCK_SOURCE(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdsp_info_clock_source, \
  .get = snd_hdsp_get_clock_source, \
  .put = snd_hdsp_put_clock_source \
}

static int hdsp_clock_source(struct hdsp *hdsp)
{
	if (hdsp->control_register & HDSP_ClockModeMaster) {
		switch (hdsp->system_sample_rate) {
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

static int hdsp_set_clock_source(struct hdsp *hdsp, int mode)
{
	int rate;
	switch (mode) {
	case HDSP_CLOCK_SOURCE_AUTOSYNC:
		if (hdsp_external_sample_rate(hdsp) != 0) {
		    if (!hdsp_set_rate(hdsp, hdsp_external_sample_rate(hdsp), 1)) {
			hdsp->control_register &= ~HDSP_ClockModeMaster;
			hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
			return 0;
		    }
		}
		return -1;
	case HDSP_CLOCK_SOURCE_INTERNAL_32KHZ:
		rate = 32000;
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_44_1KHZ:
		rate = 44100;
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_48KHZ:
		rate = 48000;
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_64KHZ:
		rate = 64000;
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_88_2KHZ:
		rate = 88200;
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_96KHZ:
		rate = 96000;
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_128KHZ:
		rate = 128000;
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_176_4KHZ:
		rate = 176400;
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_192KHZ:
		rate = 192000;
		break;
	default:
		rate = 48000;
	}
	hdsp->control_register |= HDSP_ClockModeMaster;
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	hdsp_set_rate(hdsp, rate, 1);
	return 0;
}

static int snd_hdsp_info_clock_source(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = {"AutoSync", "Internal 32.0 kHz", "Internal 44.1 kHz", "Internal 48.0 kHz", "Internal 64.0 kHz", "Internal 88.2 kHz", "Internal 96.0 kHz", "Internal 128 kHz", "Internal 176.4 kHz", "Internal 192.0 KHz" };
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	if (hdsp->io_type == H9632)
	    uinfo->value.enumerated.items = 10;
	else
	    uinfo->value.enumerated.items = 7;
	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item = uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_hdsp_get_clock_source(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp_clock_source(hdsp);
	return 0;
}

static int snd_hdsp_put_clock_source(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.enumerated.item[0];
	if (val < 0) val = 0;
	if (hdsp->io_type == H9632) {
		if (val > 9)
			val = 9;
	} else {
		if (val > 6)
			val = 6;
	}
	spin_lock_irq(&hdsp->lock);
	if (val != hdsp_clock_source(hdsp))
		change = (hdsp_set_clock_source(hdsp, val) == 0) ? 1 : 0;
	else
		change = 0;
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define snd_hdsp_info_clock_source_lock		snd_ctl_boolean_mono_info

static int snd_hdsp_get_clock_source_lock(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = hdsp->clock_source_locked;
	return 0;
}

static int snd_hdsp_put_clock_source_lock(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;

	change = (int)ucontrol->value.integer.value[0] != hdsp->clock_source_locked;
	if (change)
		hdsp->clock_source_locked = !!ucontrol->value.integer.value[0];
	return change;
}

#define HDSP_DA_GAIN(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdsp_info_da_gain, \
  .get = snd_hdsp_get_da_gain, \
  .put = snd_hdsp_put_da_gain \
}

static int hdsp_da_gain(struct hdsp *hdsp)
{
	switch (hdsp->control_register & HDSP_DAGainMask) {
	case HDSP_DAGainHighGain:
		return 0;
	case HDSP_DAGainPlus4dBu:
		return 1;
	case HDSP_DAGainMinus10dBV:
		return 2;
	default:
		return 1;
	}
}

static int hdsp_set_da_gain(struct hdsp *hdsp, int mode)
{
	hdsp->control_register &= ~HDSP_DAGainMask;
	switch (mode) {
	case 0:
		hdsp->control_register |= HDSP_DAGainHighGain;
		break;
	case 1:
		hdsp->control_register |= HDSP_DAGainPlus4dBu;
		break;
	case 2:
		hdsp->control_register |= HDSP_DAGainMinus10dBV;
		break;
	default:
		return -1;

	}
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	return 0;
}

static int snd_hdsp_info_da_gain(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = {"Hi Gain", "+4 dBu", "-10 dbV"};

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 3;
	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item = uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_hdsp_get_da_gain(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp_da_gain(hdsp);
	return 0;
}

static int snd_hdsp_put_da_gain(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.enumerated.item[0];
	if (val < 0) val = 0;
	if (val > 2) val = 2;
	spin_lock_irq(&hdsp->lock);
	if (val != hdsp_da_gain(hdsp))
		change = (hdsp_set_da_gain(hdsp, val) == 0) ? 1 : 0;
	else
		change = 0;
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define HDSP_AD_GAIN(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdsp_info_ad_gain, \
  .get = snd_hdsp_get_ad_gain, \
  .put = snd_hdsp_put_ad_gain \
}

static int hdsp_ad_gain(struct hdsp *hdsp)
{
	switch (hdsp->control_register & HDSP_ADGainMask) {
	case HDSP_ADGainMinus10dBV:
		return 0;
	case HDSP_ADGainPlus4dBu:
		return 1;
	case HDSP_ADGainLowGain:
		return 2;
	default:
		return 1;
	}
}

static int hdsp_set_ad_gain(struct hdsp *hdsp, int mode)
{
	hdsp->control_register &= ~HDSP_ADGainMask;
	switch (mode) {
	case 0:
		hdsp->control_register |= HDSP_ADGainMinus10dBV;
		break;
	case 1:
		hdsp->control_register |= HDSP_ADGainPlus4dBu;
		break;
	case 2:
		hdsp->control_register |= HDSP_ADGainLowGain;
		break;
	default:
		return -1;

	}
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	return 0;
}

static int snd_hdsp_info_ad_gain(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = {"-10 dBV", "+4 dBu", "Lo Gain"};

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 3;
	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item = uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_hdsp_get_ad_gain(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp_ad_gain(hdsp);
	return 0;
}

static int snd_hdsp_put_ad_gain(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.enumerated.item[0];
	if (val < 0) val = 0;
	if (val > 2) val = 2;
	spin_lock_irq(&hdsp->lock);
	if (val != hdsp_ad_gain(hdsp))
		change = (hdsp_set_ad_gain(hdsp, val) == 0) ? 1 : 0;
	else
		change = 0;
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define HDSP_PHONE_GAIN(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdsp_info_phone_gain, \
  .get = snd_hdsp_get_phone_gain, \
  .put = snd_hdsp_put_phone_gain \
}

static int hdsp_phone_gain(struct hdsp *hdsp)
{
	switch (hdsp->control_register & HDSP_PhoneGainMask) {
	case HDSP_PhoneGain0dB:
		return 0;
	case HDSP_PhoneGainMinus6dB:
		return 1;
	case HDSP_PhoneGainMinus12dB:
		return 2;
	default:
		return 0;
	}
}

static int hdsp_set_phone_gain(struct hdsp *hdsp, int mode)
{
	hdsp->control_register &= ~HDSP_PhoneGainMask;
	switch (mode) {
	case 0:
		hdsp->control_register |= HDSP_PhoneGain0dB;
		break;
	case 1:
		hdsp->control_register |= HDSP_PhoneGainMinus6dB;
		break;
	case 2:
		hdsp->control_register |= HDSP_PhoneGainMinus12dB;
		break;
	default:
		return -1;

	}
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	return 0;
}

static int snd_hdsp_info_phone_gain(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = {"0 dB", "-6 dB", "-12 dB"};

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 3;
	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item = uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_hdsp_get_phone_gain(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp_phone_gain(hdsp);
	return 0;
}

static int snd_hdsp_put_phone_gain(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.enumerated.item[0];
	if (val < 0) val = 0;
	if (val > 2) val = 2;
	spin_lock_irq(&hdsp->lock);
	if (val != hdsp_phone_gain(hdsp))
		change = (hdsp_set_phone_gain(hdsp, val) == 0) ? 1 : 0;
	else
		change = 0;
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define HDSP_XLR_BREAKOUT_CABLE(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdsp_info_xlr_breakout_cable, \
  .get = snd_hdsp_get_xlr_breakout_cable, \
  .put = snd_hdsp_put_xlr_breakout_cable \
}

static int hdsp_xlr_breakout_cable(struct hdsp *hdsp)
{
	if (hdsp->control_register & HDSP_XLRBreakoutCable)
		return 1;
	return 0;
}

static int hdsp_set_xlr_breakout_cable(struct hdsp *hdsp, int mode)
{
	if (mode)
		hdsp->control_register |= HDSP_XLRBreakoutCable;
	else
		hdsp->control_register &= ~HDSP_XLRBreakoutCable;
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	return 0;
}

#define snd_hdsp_info_xlr_breakout_cable	snd_ctl_boolean_mono_info

static int snd_hdsp_get_xlr_breakout_cable(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp_xlr_breakout_cable(hdsp);
	return 0;
}

static int snd_hdsp_put_xlr_breakout_cable(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdsp->lock);
	change = (int)val != hdsp_xlr_breakout_cable(hdsp);
	hdsp_set_xlr_breakout_cable(hdsp, val);
	spin_unlock_irq(&hdsp->lock);
	return change;
}

/* (De)activates old RME Analog Extension Board
   These are connected to the internal ADAT connector
   Switching this on desactivates external ADAT
*/
#define HDSP_AEB(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdsp_info_aeb, \
  .get = snd_hdsp_get_aeb, \
  .put = snd_hdsp_put_aeb \
}

static int hdsp_aeb(struct hdsp *hdsp)
{
	if (hdsp->control_register & HDSP_AnalogExtensionBoard)
		return 1;
	return 0;
}

static int hdsp_set_aeb(struct hdsp *hdsp, int mode)
{
	if (mode)
		hdsp->control_register |= HDSP_AnalogExtensionBoard;
	else
		hdsp->control_register &= ~HDSP_AnalogExtensionBoard;
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	return 0;
}

#define snd_hdsp_info_aeb		snd_ctl_boolean_mono_info

static int snd_hdsp_get_aeb(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp_aeb(hdsp);
	return 0;
}

static int snd_hdsp_put_aeb(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdsp->lock);
	change = (int)val != hdsp_aeb(hdsp);
	hdsp_set_aeb(hdsp, val);
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define HDSP_PREF_SYNC_REF(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdsp_info_pref_sync_ref, \
  .get = snd_hdsp_get_pref_sync_ref, \
  .put = snd_hdsp_put_pref_sync_ref \
}

static int hdsp_pref_sync_ref(struct hdsp *hdsp)
{
	/* Notice that this looks at the requested sync source,
	   not the one actually in use.
	*/

	switch (hdsp->control_register & HDSP_SyncRefMask) {
	case HDSP_SyncRef_ADAT1:
		return HDSP_SYNC_FROM_ADAT1;
	case HDSP_SyncRef_ADAT2:
		return HDSP_SYNC_FROM_ADAT2;
	case HDSP_SyncRef_ADAT3:
		return HDSP_SYNC_FROM_ADAT3;
	case HDSP_SyncRef_SPDIF:
		return HDSP_SYNC_FROM_SPDIF;
	case HDSP_SyncRef_WORD:
		return HDSP_SYNC_FROM_WORD;
	case HDSP_SyncRef_ADAT_SYNC:
		return HDSP_SYNC_FROM_ADAT_SYNC;
	default:
		return HDSP_SYNC_FROM_WORD;
	}
	return 0;
}

static int hdsp_set_pref_sync_ref(struct hdsp *hdsp, int pref)
{
	hdsp->control_register &= ~HDSP_SyncRefMask;
	switch (pref) {
	case HDSP_SYNC_FROM_ADAT1:
		hdsp->control_register &= ~HDSP_SyncRefMask; /* clear SyncRef bits */
		break;
	case HDSP_SYNC_FROM_ADAT2:
		hdsp->control_register |= HDSP_SyncRef_ADAT2;
		break;
	case HDSP_SYNC_FROM_ADAT3:
		hdsp->control_register |= HDSP_SyncRef_ADAT3;
		break;
	case HDSP_SYNC_FROM_SPDIF:
		hdsp->control_register |= HDSP_SyncRef_SPDIF;
		break;
	case HDSP_SYNC_FROM_WORD:
		hdsp->control_register |= HDSP_SyncRef_WORD;
		break;
	case HDSP_SYNC_FROM_ADAT_SYNC:
		hdsp->control_register |= HDSP_SyncRef_ADAT_SYNC;
		break;
	default:
		return -1;
	}
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	return 0;
}

static int snd_hdsp_info_pref_sync_ref(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = {"Word", "IEC958", "ADAT1", "ADAT Sync", "ADAT2", "ADAT3" };
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;

	switch (hdsp->io_type) {
	case Digiface:
	case H9652:
		uinfo->value.enumerated.items = 6;
		break;
	case Multiface:
		uinfo->value.enumerated.items = 4;
		break;
	case H9632:
		uinfo->value.enumerated.items = 3;
		break;
	default:
		uinfo->value.enumerated.items = 0;
		break;
	}

	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item = uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_hdsp_get_pref_sync_ref(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp_pref_sync_ref(hdsp);
	return 0;
}

static int snd_hdsp_put_pref_sync_ref(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change, max;
	unsigned int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;

	switch (hdsp->io_type) {
	case Digiface:
	case H9652:
		max = 6;
		break;
	case Multiface:
		max = 4;
		break;
	case H9632:
		max = 3;
		break;
	default:
		return -EIO;
	}

	val = ucontrol->value.enumerated.item[0] % max;
	spin_lock_irq(&hdsp->lock);
	change = (int)val != hdsp_pref_sync_ref(hdsp);
	hdsp_set_pref_sync_ref(hdsp, val);
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define HDSP_AUTOSYNC_REF(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ, \
  .info = snd_hdsp_info_autosync_ref, \
  .get = snd_hdsp_get_autosync_ref, \
}

static int hdsp_autosync_ref(struct hdsp *hdsp)
{
	/* This looks at the autosync selected sync reference */
	unsigned int status2 = hdsp_read(hdsp, HDSP_status2Register);

	switch (status2 & HDSP_SelSyncRefMask) {
	case HDSP_SelSyncRef_WORD:
		return HDSP_AUTOSYNC_FROM_WORD;
	case HDSP_SelSyncRef_ADAT_SYNC:
		return HDSP_AUTOSYNC_FROM_ADAT_SYNC;
	case HDSP_SelSyncRef_SPDIF:
		return HDSP_AUTOSYNC_FROM_SPDIF;
	case HDSP_SelSyncRefMask:
		return HDSP_AUTOSYNC_FROM_NONE;
	case HDSP_SelSyncRef_ADAT1:
		return HDSP_AUTOSYNC_FROM_ADAT1;
	case HDSP_SelSyncRef_ADAT2:
		return HDSP_AUTOSYNC_FROM_ADAT2;
	case HDSP_SelSyncRef_ADAT3:
		return HDSP_AUTOSYNC_FROM_ADAT3;
	default:
		return HDSP_AUTOSYNC_FROM_WORD;
	}
	return 0;
}

static int snd_hdsp_info_autosync_ref(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = {"Word", "ADAT Sync", "IEC958", "None", "ADAT1", "ADAT2", "ADAT3" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 7;
	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item = uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_hdsp_get_autosync_ref(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp_autosync_ref(hdsp);
	return 0;
}

#define HDSP_LINE_OUT(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdsp_info_line_out, \
  .get = snd_hdsp_get_line_out, \
  .put = snd_hdsp_put_line_out \
}

static int hdsp_line_out(struct hdsp *hdsp)
{
	return (hdsp->control_register & HDSP_LineOut) ? 1 : 0;
}

static int hdsp_set_line_output(struct hdsp *hdsp, int out)
{
	if (out)
		hdsp->control_register |= HDSP_LineOut;
	else
		hdsp->control_register &= ~HDSP_LineOut;
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	return 0;
}

#define snd_hdsp_info_line_out		snd_ctl_boolean_mono_info

static int snd_hdsp_get_line_out(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&hdsp->lock);
	ucontrol->value.integer.value[0] = hdsp_line_out(hdsp);
	spin_unlock_irq(&hdsp->lock);
	return 0;
}

static int snd_hdsp_put_line_out(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdsp->lock);
	change = (int)val != hdsp_line_out(hdsp);
	hdsp_set_line_output(hdsp, val);
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define HDSP_PRECISE_POINTER(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_CARD, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdsp_info_precise_pointer, \
  .get = snd_hdsp_get_precise_pointer, \
  .put = snd_hdsp_put_precise_pointer \
}

static int hdsp_set_precise_pointer(struct hdsp *hdsp, int precise)
{
	if (precise)
		hdsp->precise_ptr = 1;
	else
		hdsp->precise_ptr = 0;
	return 0;
}

#define snd_hdsp_info_precise_pointer		snd_ctl_boolean_mono_info

static int snd_hdsp_get_precise_pointer(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&hdsp->lock);
	ucontrol->value.integer.value[0] = hdsp->precise_ptr;
	spin_unlock_irq(&hdsp->lock);
	return 0;
}

static int snd_hdsp_put_precise_pointer(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdsp->lock);
	change = (int)val != hdsp->precise_ptr;
	hdsp_set_precise_pointer(hdsp, val);
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define HDSP_USE_MIDI_TASKLET(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_CARD, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdsp_info_use_midi_tasklet, \
  .get = snd_hdsp_get_use_midi_tasklet, \
  .put = snd_hdsp_put_use_midi_tasklet \
}

static int hdsp_set_use_midi_tasklet(struct hdsp *hdsp, int use_tasklet)
{
	if (use_tasklet)
		hdsp->use_midi_tasklet = 1;
	else
		hdsp->use_midi_tasklet = 0;
	return 0;
}

#define snd_hdsp_info_use_midi_tasklet		snd_ctl_boolean_mono_info

static int snd_hdsp_get_use_midi_tasklet(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	spin_lock_irq(&hdsp->lock);
	ucontrol->value.integer.value[0] = hdsp->use_midi_tasklet;
	spin_unlock_irq(&hdsp->lock);
	return 0;
}

static int snd_hdsp_put_use_midi_tasklet(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	unsigned int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.integer.value[0] & 1;
	spin_lock_irq(&hdsp->lock);
	change = (int)val != hdsp->use_midi_tasklet;
	hdsp_set_use_midi_tasklet(hdsp, val);
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define HDSP_MIXER(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_HWDEP, \
  .name = xname, \
  .index = xindex, \
  .device = 0, \
  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE | \
		 SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
  .info = snd_hdsp_info_mixer, \
  .get = snd_hdsp_get_mixer, \
  .put = snd_hdsp_put_mixer \
}

static int snd_hdsp_info_mixer(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 3;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 65536;
	uinfo->value.integer.step = 1;
	return 0;
}

static int snd_hdsp_get_mixer(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int source;
	int destination;
	int addr;

	source = ucontrol->value.integer.value[0];
	destination = ucontrol->value.integer.value[1];

	if (source >= hdsp->max_channels)
		addr = hdsp_playback_to_output_key(hdsp,source-hdsp->max_channels,destination);
	else
		addr = hdsp_input_to_output_key(hdsp,source, destination);

	spin_lock_irq(&hdsp->lock);
	ucontrol->value.integer.value[2] = hdsp_read_gain (hdsp, addr);
	spin_unlock_irq(&hdsp->lock);
	return 0;
}

static int snd_hdsp_put_mixer(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	int source;
	int destination;
	int gain;
	int addr;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;

	source = ucontrol->value.integer.value[0];
	destination = ucontrol->value.integer.value[1];

	if (source >= hdsp->max_channels)
		addr = hdsp_playback_to_output_key(hdsp,source-hdsp->max_channels, destination);
	else
		addr = hdsp_input_to_output_key(hdsp,source, destination);

	gain = ucontrol->value.integer.value[2];

	spin_lock_irq(&hdsp->lock);
	change = gain != hdsp_read_gain(hdsp, addr);
	if (change)
		hdsp_write_gain(hdsp, addr, gain);
	spin_unlock_irq(&hdsp->lock);
	return change;
}

#define HDSP_WC_SYNC_CHECK(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ | SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
  .info = snd_hdsp_info_sync_check, \
  .get = snd_hdsp_get_wc_sync_check \
}

static int snd_hdsp_info_sync_check(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = {"No Lock", "Lock", "Sync" };
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 3;
	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item = uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int hdsp_wc_sync_check(struct hdsp *hdsp)
{
	int status2 = hdsp_read(hdsp, HDSP_status2Register);
	if (status2 & HDSP_wc_lock) {
		if (status2 & HDSP_wc_sync)
			return 2;
		else
			 return 1;
	} else
		return 0;
	return 0;
}

static int snd_hdsp_get_wc_sync_check(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp_wc_sync_check(hdsp);
	return 0;
}

#define HDSP_SPDIF_SYNC_CHECK(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ | SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
  .info = snd_hdsp_info_sync_check, \
  .get = snd_hdsp_get_spdif_sync_check \
}

static int hdsp_spdif_sync_check(struct hdsp *hdsp)
{
	int status = hdsp_read(hdsp, HDSP_statusRegister);
	if (status & HDSP_SPDIFErrorFlag)
		return 0;
	else {
		if (status & HDSP_SPDIFSync)
			return 2;
		else
			return 1;
	}
	return 0;
}

static int snd_hdsp_get_spdif_sync_check(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp_spdif_sync_check(hdsp);
	return 0;
}

#define HDSP_ADATSYNC_SYNC_CHECK(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ | SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
  .info = snd_hdsp_info_sync_check, \
  .get = snd_hdsp_get_adatsync_sync_check \
}

static int hdsp_adatsync_sync_check(struct hdsp *hdsp)
{
	int status = hdsp_read(hdsp, HDSP_statusRegister);
	if (status & HDSP_TimecodeLock) {
		if (status & HDSP_TimecodeSync)
			return 2;
		else
			return 1;
	} else
		return 0;
}

static int snd_hdsp_get_adatsync_sync_check(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp_adatsync_sync_check(hdsp);
	return 0;
}

#define HDSP_ADAT_SYNC_CHECK \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .access = SNDRV_CTL_ELEM_ACCESS_READ | SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
  .info = snd_hdsp_info_sync_check, \
  .get = snd_hdsp_get_adat_sync_check \
}

static int hdsp_adat_sync_check(struct hdsp *hdsp, int idx)
{
	int status = hdsp_read(hdsp, HDSP_statusRegister);

	if (status & (HDSP_Lock0>>idx)) {
		if (status & (HDSP_Sync0>>idx))
			return 2;
		else
			return 1;
	} else
		return 0;
}

static int snd_hdsp_get_adat_sync_check(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int offset;
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	offset = ucontrol->id.index - 1;
	snd_BUG_ON(offset < 0);

	switch (hdsp->io_type) {
	case Digiface:
	case H9652:
		if (offset >= 3)
			return -EINVAL;
		break;
	case Multiface:
	case H9632:
		if (offset >= 1)
			return -EINVAL;
		break;
	default:
		return -EIO;
	}

	ucontrol->value.enumerated.item[0] = hdsp_adat_sync_check(hdsp, offset);
	return 0;
}

#define HDSP_DDS_OFFSET(xname, xindex) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_hdsp_info_dds_offset, \
  .get = snd_hdsp_get_dds_offset, \
  .put = snd_hdsp_put_dds_offset \
}

static int hdsp_dds_offset(struct hdsp *hdsp)
{
	u64 n;
	unsigned int dds_value = hdsp->dds_value;
	int system_sample_rate = hdsp->system_sample_rate;

	if (!dds_value)
		return 0;

	n = DDS_NUMERATOR;
	/*
	 * dds_value = n / rate
	 * rate = n / dds_value
	 */
	n = div_u64(n, dds_value);
	if (system_sample_rate >= 112000)
		n *= 4;
	else if (system_sample_rate >= 56000)
		n *= 2;
	return ((int)n) - system_sample_rate;
}

static int hdsp_set_dds_offset(struct hdsp *hdsp, int offset_hz)
{
	int rate = hdsp->system_sample_rate + offset_hz;
	hdsp_set_dds_value(hdsp, rate);
	return 0;
}

static int snd_hdsp_info_dds_offset(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = -5000;
	uinfo->value.integer.max = 5000;
	return 0;
}

static int snd_hdsp_get_dds_offset(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = hdsp_dds_offset(hdsp);
	return 0;
}

static int snd_hdsp_put_dds_offset(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int change;
	int val;

	if (!snd_hdsp_use_is_exclusive(hdsp))
		return -EBUSY;
	val = ucontrol->value.enumerated.item[0];
	spin_lock_irq(&hdsp->lock);
	if (val != hdsp_dds_offset(hdsp))
		change = (hdsp_set_dds_offset(hdsp, val) == 0) ? 1 : 0;
	else
		change = 0;
	spin_unlock_irq(&hdsp->lock);
	return change;
}

static struct snd_kcontrol_new snd_hdsp_9632_controls[] = {
HDSP_DA_GAIN("DA Gain", 0),
HDSP_AD_GAIN("AD Gain", 0),
HDSP_PHONE_GAIN("Phones Gain", 0),
HDSP_XLR_BREAKOUT_CABLE("XLR Breakout Cable", 0),
HDSP_DDS_OFFSET("DDS Sample Rate Offset", 0)
};

static struct snd_kcontrol_new snd_hdsp_controls[] = {
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		SNDRV_CTL_NAME_IEC958("",PLAYBACK,DEFAULT),
	.info =		snd_hdsp_control_spdif_info,
	.get =		snd_hdsp_control_spdif_get,
	.put =		snd_hdsp_control_spdif_put,
},
{
	.access =	SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_INACTIVE,
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		SNDRV_CTL_NAME_IEC958("",PLAYBACK,PCM_STREAM),
	.info =		snd_hdsp_control_spdif_stream_info,
	.get =		snd_hdsp_control_spdif_stream_get,
	.put =		snd_hdsp_control_spdif_stream_put,
},
{
	.access =	SNDRV_CTL_ELEM_ACCESS_READ,
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		SNDRV_CTL_NAME_IEC958("",PLAYBACK,CON_MASK),
	.info =		snd_hdsp_control_spdif_mask_info,
	.get =		snd_hdsp_control_spdif_mask_get,
	.private_value = IEC958_AES0_NONAUDIO |
  			 IEC958_AES0_PROFESSIONAL |
			 IEC958_AES0_CON_EMPHASIS,
},
{
	.access =	SNDRV_CTL_ELEM_ACCESS_READ,
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		SNDRV_CTL_NAME_IEC958("",PLAYBACK,PRO_MASK),
	.info =		snd_hdsp_control_spdif_mask_info,
	.get =		snd_hdsp_control_spdif_mask_get,
	.private_value = IEC958_AES0_NONAUDIO |
			 IEC958_AES0_PROFESSIONAL |
			 IEC958_AES0_PRO_EMPHASIS,
},
HDSP_MIXER("Mixer", 0),
HDSP_SPDIF_IN("IEC958 Input Connector", 0),
HDSP_SPDIF_OUT("IEC958 Output also on ADAT1", 0),
HDSP_SPDIF_PROFESSIONAL("IEC958 Professional Bit", 0),
HDSP_SPDIF_EMPHASIS("IEC958 Emphasis Bit", 0),
HDSP_SPDIF_NON_AUDIO("IEC958 Non-audio Bit", 0),
/* 'Sample Clock Source' complies with the alsa control naming scheme */
HDSP_CLOCK_SOURCE("Sample Clock Source", 0),
{
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Sample Clock Source Locking",
	.info = snd_hdsp_info_clock_source_lock,
	.get = snd_hdsp_get_clock_source_lock,
	.put = snd_hdsp_put_clock_source_lock,
},
HDSP_SYSTEM_CLOCK_MODE("System Clock Mode", 0),
HDSP_PREF_SYNC_REF("Preferred Sync Reference", 0),
HDSP_AUTOSYNC_REF("AutoSync Reference", 0),
HDSP_SPDIF_SAMPLE_RATE("SPDIF Sample Rate", 0),
HDSP_SYSTEM_SAMPLE_RATE("System Sample Rate", 0),
/* 'External Rate' complies with the alsa control naming scheme */
HDSP_AUTOSYNC_SAMPLE_RATE("External Rate", 0),
HDSP_WC_SYNC_CHECK("Word Clock Lock Status", 0),
HDSP_SPDIF_SYNC_CHECK("SPDIF Lock Status", 0),
HDSP_ADATSYNC_SYNC_CHECK("ADAT Sync Lock Status", 0),
HDSP_LINE_OUT("Line Out", 0),
HDSP_PRECISE_POINTER("Precise Pointer", 0),
HDSP_USE_MIDI_TASKLET("Use Midi Tasklet", 0),
};

static struct snd_kcontrol_new snd_hdsp_96xx_aeb = HDSP_AEB("Analog Extension Board", 0);
static struct snd_kcontrol_new snd_hdsp_adat_sync_check = HDSP_ADAT_SYNC_CHECK;

static int snd_hdsp_create_controls(struct snd_card *card, struct hdsp *hdsp)
{
	unsigned int idx;
	int err;
	struct snd_kcontrol *kctl;

	for (idx = 0; idx < ARRAY_SIZE(snd_hdsp_controls); idx++) {
		if ((err = snd_ctl_add(card, kctl = snd_ctl_new1(&snd_hdsp_controls[idx], hdsp))) < 0)
			return err;
		if (idx == 1)	/* IEC958 (S/PDIF) Stream */
			hdsp->spdif_ctl = kctl;
	}

	/* ADAT SyncCheck status */
	snd_hdsp_adat_sync_check.name = "ADAT Lock Status";
	snd_hdsp_adat_sync_check.index = 1;
	if ((err = snd_ctl_add (card, kctl = snd_ctl_new1(&snd_hdsp_adat_sync_check, hdsp))))
		return err;
	if (hdsp->io_type == Digiface || hdsp->io_type == H9652) {
		for (idx = 1; idx < 3; ++idx) {
			snd_hdsp_adat_sync_check.index = idx+1;
			if ((err = snd_ctl_add (card, kctl = snd_ctl_new1(&snd_hdsp_adat_sync_check, hdsp))))
				return err;
		}
	}

	/* DA, AD and Phone gain and XLR breakout cable controls for H9632 cards */
	if (hdsp->io_type == H9632) {
		for (idx = 0; idx < ARRAY_SIZE(snd_hdsp_9632_controls); idx++) {
			if ((err = snd_ctl_add(card, kctl = snd_ctl_new1(&snd_hdsp_9632_controls[idx], hdsp))) < 0)
				return err;
		}
	}

	/* AEB control for H96xx card */
	if (hdsp->io_type == H9632 || hdsp->io_type == H9652) {
		if ((err = snd_ctl_add(card, kctl = snd_ctl_new1(&snd_hdsp_96xx_aeb, hdsp))) < 0)
				return err;
	}

	return 0;
}

/*------------------------------------------------------------
   /proc interface
 ------------------------------------------------------------*/

static void
snd_hdsp_proc_read(struct snd_info_entry *entry, struct snd_info_buffer *buffer)
{
	struct hdsp *hdsp = (struct hdsp *) entry->private_data;
	unsigned int status;
	unsigned int status2;
	char *pref_sync_ref;
	char *autosync_ref;
	char *system_clock_mode;
	char *clock_source;
	int x;

	status = hdsp_read(hdsp, HDSP_statusRegister);
	status2 = hdsp_read(hdsp, HDSP_status2Register);

	snd_iprintf(buffer, "%s (Card #%d)\n", hdsp->card_name,
		    hdsp->card->number + 1);
	snd_iprintf(buffer, "Buffers: capture %p playback %p\n",
		    hdsp->capture_buffer, hdsp->playback_buffer);
	snd_iprintf(buffer, "IRQ: %d Registers bus: 0x%lx VM: 0x%lx\n",
		    hdsp->irq, hdsp->port, (unsigned long)hdsp->iobase);
	snd_iprintf(buffer, "Control register: 0x%x\n", hdsp->control_register);
	snd_iprintf(buffer, "Control2 register: 0x%x\n",
		    hdsp->control2_register);
	snd_iprintf(buffer, "Status register: 0x%x\n", status);
	snd_iprintf(buffer, "Status2 register: 0x%x\n", status2);

	if (hdsp_check_for_iobox(hdsp)) {
		snd_iprintf(buffer, "No I/O box connected.\n"
			    "Please connect one and upload firmware.\n");
		return;
	}

	if (hdsp_check_for_firmware(hdsp, 0)) {
		if (hdsp->state & HDSP_FirmwareCached) {
			if (snd_hdsp_load_firmware_from_cache(hdsp) != 0) {
				snd_iprintf(buffer, "Firmware loading from "
					    "cache failed, "
					    "please upload manually.\n");
				return;
			}
		} else {
			int err = -EINVAL;
#ifdef HDSP_FW_LOADER
			err = hdsp_request_fw_loader(hdsp);
#endif
			if (err < 0) {
				snd_iprintf(buffer,
					    "No firmware loaded nor cached, "
					    "please upload firmware.\n");
				return;
			}
		}
	}

	snd_iprintf(buffer, "FIFO status: %d\n", hdsp_read(hdsp, HDSP_fifoStatus) & 0xff);
	snd_iprintf(buffer, "MIDI1 Output status: 0x%x\n", hdsp_read(hdsp, HDSP_midiStatusOut0));
	snd_iprintf(buffer, "MIDI1 Input status: 0x%x\n", hdsp_read(hdsp, HDSP_midiStatusIn0));
	snd_iprintf(buffer, "MIDI2 Output status: 0x%x\n", hdsp_read(hdsp, HDSP_midiStatusOut1));
	snd_iprintf(buffer, "MIDI2 Input status: 0x%x\n", hdsp_read(hdsp, HDSP_midiStatusIn1));
	snd_iprintf(buffer, "Use Midi Tasklet: %s\n", hdsp->use_midi_tasklet ? "on" : "off");

	snd_iprintf(buffer, "\n");

	x = 1 << (6 + hdsp_decode_latency(hdsp->control_register & HDSP_LatencyMask));

	snd_iprintf(buffer, "Buffer Size (Latency): %d samples (2 periods of %lu bytes)\n", x, (unsigned long) hdsp->period_bytes);
	snd_iprintf(buffer, "Hardware pointer (frames): %ld\n", hdsp_hw_pointer(hdsp));
	snd_iprintf(buffer, "Precise pointer: %s\n", hdsp->precise_ptr ? "on" : "off");
	snd_iprintf(buffer, "Line out: %s\n", (hdsp->control_register & HDSP_LineOut) ? "on" : "off");

	snd_iprintf(buffer, "Firmware version: %d\n", (status2&HDSP_version0)|(status2&HDSP_version1)<<1|(status2&HDSP_version2)<<2);

	snd_iprintf(buffer, "\n");

	switch (hdsp_clock_source(hdsp)) {
	case HDSP_CLOCK_SOURCE_AUTOSYNC:
		clock_source = "AutoSync";
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_32KHZ:
		clock_source = "Internal 32 kHz";
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_44_1KHZ:
		clock_source = "Internal 44.1 kHz";
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_48KHZ:
		clock_source = "Internal 48 kHz";
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_64KHZ:
		clock_source = "Internal 64 kHz";
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_88_2KHZ:
		clock_source = "Internal 88.2 kHz";
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_96KHZ:
		clock_source = "Internal 96 kHz";
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_128KHZ:
		clock_source = "Internal 128 kHz";
		break;
	case HDSP_CLOCK_SOURCE_INTERNAL_176_4KHZ:
		clock_source = "Internal 176.4 kHz";
		break;
		case HDSP_CLOCK_SOURCE_INTERNAL_192KHZ:
		clock_source = "Internal 192 kHz";
		break;
	default:
		clock_source = "Error";
	}
	snd_iprintf (buffer, "Sample Clock Source: %s\n", clock_source);

	if (hdsp_system_clock_mode(hdsp))
		system_clock_mode = "Slave";
	else
		system_clock_mode = "Master";

	switch (hdsp_pref_sync_ref (hdsp)) {
	case HDSP_SYNC_FROM_WORD:
		pref_sync_ref = "Word Clock";
		break;
	case HDSP_SYNC_FROM_ADAT_SYNC:
		pref_sync_ref = "ADAT Sync";
		break;
	case HDSP_SYNC_FROM_SPDIF:
		pref_sync_ref = "SPDIF";
		break;
	case HDSP_SYNC_FROM_ADAT1:
		pref_sync_ref = "ADAT1";
		break;
	case HDSP_SYNC_FROM_ADAT2:
		pref_sync_ref = "ADAT2";
		break;
	case HDSP_SYNC_FROM_ADAT3:
		pref_sync_ref = "ADAT3";
		break;
	default:
		pref_sync_ref = "Word Clock";
		break;
	}
	snd_iprintf (buffer, "Preferred Sync Reference: %s\n", pref_sync_ref);

	switch (hdsp_autosync_ref (hdsp)) {
	case HDSP_AUTOSYNC_FROM_WORD:
		autosync_ref = "Word Clock";
		break;
	case HDSP_AUTOSYNC_FROM_ADAT_SYNC:
		autosync_ref = "ADAT Sync";
		break;
	case HDSP_AUTOSYNC_FROM_SPDIF:
		autosync_ref = "SPDIF";
		break;
	case HDSP_AUTOSYNC_FROM_NONE:
		autosync_ref = "None";
		break;
	case HDSP_AUTOSYNC_FROM_ADAT1:
		autosync_ref = "ADAT1";
		break;
	case HDSP_AUTOSYNC_FROM_ADAT2:
		autosync_ref = "ADAT2";
		break;
	case HDSP_AUTOSYNC_FROM_ADAT3:
		autosync_ref = "ADAT3";
		break;
	default:
		autosync_ref = "---";
		break;
	}
	snd_iprintf (buffer, "AutoSync Reference: %s\n", autosync_ref);

	snd_iprintf (buffer, "AutoSync Frequency: %d\n", hdsp_external_sample_rate(hdsp));

	snd_iprintf (buffer, "System Clock Mode: %s\n", system_clock_mode);

	snd_iprintf (buffer, "System Clock Frequency: %d\n", hdsp->system_sample_rate);
	snd_iprintf (buffer, "System Clock Locked: %s\n", hdsp->clock_source_locked ? "Yes" : "No");

	snd_iprintf(buffer, "\n");

	switch (hdsp_spdif_in(hdsp)) {
	case HDSP_SPDIFIN_OPTICAL:
		snd_iprintf(buffer, "IEC958 input: Optical\n");
		break;
	case HDSP_SPDIFIN_COAXIAL:
		snd_iprintf(buffer, "IEC958 input: Coaxial\n");
		break;
	case HDSP_SPDIFIN_INTERNAL:
		snd_iprintf(buffer, "IEC958 input: Internal\n");
		break;
	case HDSP_SPDIFIN_AES:
		snd_iprintf(buffer, "IEC958 input: AES\n");
		break;
	default:
		snd_iprintf(buffer, "IEC958 input: ???\n");
		break;
	}

	if (hdsp->control_register & HDSP_SPDIFOpticalOut)
		snd_iprintf(buffer, "IEC958 output: Coaxial & ADAT1\n");
	else
		snd_iprintf(buffer, "IEC958 output: Coaxial only\n");

	if (hdsp->control_register & HDSP_SPDIFProfessional)
		snd_iprintf(buffer, "IEC958 quality: Professional\n");
	else
		snd_iprintf(buffer, "IEC958 quality: Consumer\n");

	if (hdsp->control_register & HDSP_SPDIFEmphasis)
		snd_iprintf(buffer, "IEC958 emphasis: on\n");
	else
		snd_iprintf(buffer, "IEC958 emphasis: off\n");

	if (hdsp->control_register & HDSP_SPDIFNonAudio)
		snd_iprintf(buffer, "IEC958 NonAudio: on\n");
	else
		snd_iprintf(buffer, "IEC958 NonAudio: off\n");
	if ((x = hdsp_spdif_sample_rate (hdsp)) != 0)
		snd_iprintf (buffer, "IEC958 sample rate: %d\n", x);
	else
		snd_iprintf (buffer, "IEC958 sample rate: Error flag set\n");

	snd_iprintf(buffer, "\n");

	/* Sync Check */
	x = status & HDSP_Sync0;
	if (status & HDSP_Lock0)
		snd_iprintf(buffer, "ADAT1: %s\n", x ? "Sync" : "Lock");
	else
		snd_iprintf(buffer, "ADAT1: No Lock\n");

	switch (hdsp->io_type) {
	case Digiface:
	case H9652:
		x = status & HDSP_Sync1;
		if (status & HDSP_Lock1)
			snd_iprintf(buffer, "ADAT2: %s\n", x ? "Sync" : "A dr");
		elseer for RME Hammerfall DSP audio No A dr\nopyrigx = sta/*
 *   ALS
 *
2yrigif (cus Andersson
A dr2ver for RME Hammerfall DSP aud3o interface(s)
 *
 *      Copyright (c) 2002  Paul Davis
 *       3                 break;
	default:
		/* relax */ms of the G}

  Marcus Andersson
 PDIF
 *
;
                    SoftErrorFlagver or RME Hamm erfall DSP Soft                ht (c) cense, or
 *   (at your optiointerface(s)
 *
 *      Copyrhed by
 *   2 *   ALSwc_sare Foundation; l,
 *   but locke License, or
 *   (at youWord C eveo interface(s)
 *
 *      Copyriersion.
 *
 *   This program  *   MERCHAN             hed by
 *   the FreeTimecodeware Foundation; either v
 *
 *  A dre License, or
 erfall DSP aud 
 *
ANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  SPublic License
 *   a Public Licenseor RME Hammerfall DSP License/* Informations about H9632 specific controlsicens    hdsp->io_type ==SA
 *
) {
		char *tmpense	switch.h>
#i_ad_gainh>
#i)y.h>
#iase 0ral 	tmp = "-10 dBV"yrig of the Glab.h>1#include <li+4 dBu.h>
#include <lNU General lude <liLo Gain.h>
#include <l}ite to the Free Software
 nux/m stributedtmpcensex/interrupt.h>dainclude <linux/slab.h>
#include <liHinux/math64.h>

#inclinux/firmware.h>
#include <linux/moduleparam.h>
#include <nux/pci.h>
#include <lude <sound/core.h>
#incluDA <sound/control.h>
#include <sound/pcphoneinclude <linux/slab.h>
#include <lix/pc.h>
#include <linux/firmware.h>
#-6 index[SNDRV_CARDSd/hwdep.h>
#include <2 index[SNDRV_CARDSude <sound/core.h>
#incluPrrens <sound/control.h>
#inclur RME Hammerfall DSPXLR Bf th7 USCablend/controlpt.h>xlr_of thout_cPNP;de <lie(s)yes *   noicensenit.h>
#incude <li_register *   ALSAnalogExtensionBoardver for RME Hammerfall DSP EB : on ( aud1 internal)        ht (c) 2002  Paul Davis
 *    ;
moduffe_param_exray(id, charp,  Temple Place, Suite 330, Blish}

cus ic void NDRVasm/curoc_init(structe thi *e <li
{
	MODULE_NDRV_nfo_entry *le spense    !able/cardLL, 04new444);
MOard, ">
#i", &le sp)e Licenseisabset_text_ops(le sple thi, bool, NULL, 04read);m_array(enable, bool, NULfree_rfall s
MODULE_PARM_DESC(enableool,ammerfallrg>");
MODUL(&;
MODULEpture_dma;
MOle thi->pcie_parfall DSP");
MODULE_LICENSE("GPL")playbackE_SUPPORTED_DEVICE("{{_array(enaint __dev44); bool, NUL44);ialize_memory
MODULE_PARM_DESC(enablunsigned long pb_bus, c("mulHammerfarfall DSP");
MOget_LICENSED_DEVICE(, ("GPL");
MODULE_SUPPORTE  ALSDMA_AREA_BYTES) < 0 ||
	    bool,.bin");
MODULE_FIRMWARE("digiface_firmwE HDSP-9652},"
		"_FIRMWARE("digiface_firmy.h>
#, 0444);
MO
MODULE_SUPPOR.areaver for R_SUPg>");pagesE("GPL");
MODULE_SUPPORpyrigE Hamk(KERN_ERR "%s: no 
MODULE availPNP;nable thiDULE_A_namepyrigreturn -ENOMEMublishe/* Align t
#des-space 64K boundaspec/

	face_f = ALIGNe HDSP_MAX_QS_CHANNELS  ddr, 0x10000ule_paE("mulefine H9652_DS_E HDSP-9652},"
	 14
/* This does noELS  Tell the LE_A where it inux/i
	pt.h>writparam_ HDSP_MinputBfall Addresltiface_fe_pafine H9632_SS_CHANNELSout12
#define H9632_DE("mul dete HDSP_MAX_QS_Crfall  =HANNELS  X_QS_CHANNELS      + ( 26
#de-yte-offsets from the iobddrNELS	 8
nalog Extenned as byte-offlog Extension Boarase val includ
#definelog Extension Boards
 dete MULTIF09632}}");
#ifdefbool, NULudioNU GeneE_DESCRIPTION("RME HammeMODULE_FIfdefidetecteASSUMPTION:HANNELS eveon
*either held, ore_revdefieon
*no need#defholdatio(e.g. during modulee_revULE_FIRMW02111).
	      /* set ter		64
:
e_rev Soft I 12
 via Coaxe_revMaESC(ice	  	modefine maximum latency (7 => 2^7 = 8192 samples,_SS_byte#define,e_rev1 value
 */

#defwhich implies 2 4096s byte-, 32ts froperiods).
 value
 */
EnPNP;	line outt1  		35644);
MODULE_PARM_DESC(i=HDSP_MMERCHModeutEnabl|
	base value
 */

#dher versio68
#d HDSial#define HDSP_midiDataIpt.h>en *  _ers. Th(7)#define HDSP_midiDataIn1   LineOut;
#define H9632_SS_CHANNELSude <liRM_DESC(CHANNELS DULE_PARM_DESC( det#ifdef SNDRV_BIG_ENDIAN2Register 192
#2define HDSP_midiDBIGers ar_MODE;
#ht (c)regular i/o-mapped registe0onsindifinit.h>
#include <linux/de5mas  value
 bool, NUL; th_e8
#de_mixer.h>
#iPARTICULAR fine H9632.h>
#iSP_midiStatusI2Reg
#define HDSP_fapped regiers. Thes_reudiohw_poarrayale countdefincompute_DSP_ti_sizparam_adetectesilence everyth 		3/
#defor (ieak r i <In1   MATRIX_MIXER_SIZE; ++iver >
#incull-s_matrix[i] = MINUS_INFINITY_GAINdete   4224  /* 26 *( reset
   when read; th ||HANNELSlude <linux/delay.? 1352 :* 32 bit values */
#def)ine HD4
#define HDS H9632inclul peak vi,  /* (26+2) * 32 bits <pae MULTIFACIOCHANNELS  A
 *
 */

#inclter		64
ux/init.h>
#include <linux/delay.h>
#44);
MODULE_PARM_DESC(i|= (_FIRMWAux/mPlus4dBu |dex, "IDo
   26*3 values areis caux/m0dBpyrig 392
#define HDSP_midiStatusIn1   396
#define HDSP_fifoStatus   ANNELS  efinahe base
 rate so that card hannel mapon
*efinup_status2RegisRegis4096 peak v48000, 1ufferAddress		36
#define ble, #defimidi_tasklet(MODULE_FIRMWARargenable, "EnaPARM_DESC( =    DULE_PARM_D)argHammerfaoutputPedi[0].pistenhe Licens    7168

	 12
mas C E("GPL")K      define HDSP_TCK    1           0x02
#define HDSP_TDI                0x01
#de32}}");
#ifrq MULTI__LOADER
MODULterrupt(on	96rq,XTENT *dev_id */

#define HDSP_TMS                0x0 E_0	0x;tConfirmation	9cus Andefint audiodefine K   0BIGENDIAN_M1SP_VERSION_BIT	0AN_MOx100
#defefine HDSP_RD_MUL1x100
#define sche
#deeak reble,s Andbyte-omas C peak value #definn1   396*/
#dHDSP_Marcus Andersson
HDSP_IRQP      ;
	AN_MOMarcus Andersson
AN_MOP_PROGRAM|HDSP_CO1FIG_MODE_0)
#define H1P_PROGRAM|HDmmerfalne HDS&& !P_CONF bits */iver  MULTIFIRQ_NONE/

#definH9632_SS_CHANNELS	 #defineConfi  02111, 0*/
#d_MULTIPLE  e HDSP_TDO  l peak value K   SdefinIn0) & 0xff	(HDSP_P       (1<<1)  /* buffer size = 2^n where n1is definedmmerfalHDSP_TC#defe *   ALSIDSP_midiDataOCoyte-tes <paStart      HANDLED<<2)  /*HDSP_8  /* 26 * 64 P_MAX_QS_Csubstreamver for Rpcm * 32 bielapse      VICEm abo    s[/* thePCM_STREAM_CAPTURE].aster      NULL, 0444);
ME HDSP-96aster      (1<<4)  /* 1=Master, 0=Slave/Autosync */
#define HDSP_AudioIntPLAYBACKnable (1<<5)  lishe    s */

#de_MULTIPLE  
#define HDSP_Clus ful8

/* cont8  /* 6
#dwe disPNP;	ine */
#ds    4this 	 12
 until L, 0ess 		3is doneicensee are rms values for the&= ~ 32 bie HDSne */
#d28
#deh>
#ith no gap between values
*/
#define HDSP_9652_peakBase	7164
#defiP_outputPe              = 0
#d		LE_MIXER  0/
#de} ht (z/128k02
#define HDSP_TDI                0x04
#def_DEF}init.h>
#include <li!= Multif652_&&  /* 26 * 64 bi!ux/delaDSP_Freq1, 0=do noency1           (1<<7)  /* 0=32kHz/64kHz/128kHz */
#define HDSP_DoubleSpeed          (1<<8)  /* 0=normal speed, 1=double speed */
#define HDSP_SPDIFProf1ssional    (1<<9)  /* 0=consumer, 1=professional */
#define HDSP_SPDIFEmphasis        (1<<10) /* 0          n */
#define HDSP_SPDIFNonAudio        (1<<11) /* 0=off, 1=on */
#define HD10
#dDIFOpticalOut       /* 0=32kHz/64kt ADLE_MIXERver /* cont_LE_MIXER          0x2kHz/64kH;erAddress<3)  /* [ see_array(ena<4)  /* uframes        0x02_playbackPee, "Enable/ /* aster     *aster      /

#define HDSP_TMS     H9632 specific *_chip(/96kHz/192kHz MULTIF (1<<25) /* Fromle coun_array(enainclud 4096 hdsp_9fine HDt ev02111
MODULE_PARM_DESC(,#deffine HDIT	0x1    30)
#define HDSP  (1<<2)
SP_AP_RD_apped	  (1<<2;
ode      are_rev1BUG_ON( hdsp_96irmware hdsp_96>byte-offmax	  (1<<2s         (1<NULL<<2)  /*(DGainMask      byte-offs (1<<29map[n0)
#de]     1Mask
#define HDSP_ADGaific */==
/* theSP_AudioInterruptEwGain     0These are defined as + inPlus4dBu   (HD*_midiDaHANNEL_BUFFERgiface_ARTICULAR SP_DAGain1)
#   0
#define HDSainHighGain      HDSP_DAGainMask
#define HDSP_DA6
#define HDSP_controlRE HDSP-96copltiface_f H9632 specific */
#define H,1<<31)

#def30)
#dignifine HDSP_ADGainposNFIG_MO__user *src Anderne HDSP_ADGaincountDSP_ADGain1		  (1<<26)
#define HDSP_DAGain0		  (1<<27)
#defininclud  (1<<29)
#irmware_rev1P_ADGaipos +#defin >SP_DAGainMask
#define HDSP / 4         (1<-EINVAHDSP_  (1<<29)
#e HDSP_T  (1<<29)
#define HDSP_l peak vaster    ->pstrnc */
#d,1)

#defi FoundatHDSP_Laten!  (1<<29)
#uency0|HDSP_Fre are    HDSP_fromnMinuin1)
#deubleS+DSP_ * 4, dB  (define*equency0|HDSP_FrFAULTfine HDSP_definoneGainMask      (HDSP_Phsets froHDSP_PhoneGain1)
#define HDSP_PhoneGain0dB        HDSP_PhoneainMask
#define HDSP_PhoneGainMinus6dst (HDSP_PhoneGain0)
#define HDSP_PhoneGainMinus12dB  0

#define HDSP_LatencyMask    (HDSP_Latency0|HDSP_Latency1|HDSP_Latency2)
#define HDSP_FrequencyMask  (HDSP_Frequency0|HDSP_Frequency1|HDSP_DoubleSpeed|HDSP_QuadSpeed)

#define HDSP_SPDIFInputMask    (HDSP_SPDIFInputSelect0|HDSP_SPDIFInputSelect1)
#define HDSP_SPDIFInputADAT1to0
#def_Syncine HDSP_SPDIFInputCoaxHDSP_SPDIFInputSelect0)
#define HDSP_SPDIFInputCdrom   (HDSP_SPDIFInhw_ne HDSP_PhoneGain1)
#define HDSP_PhoneGain0dB        HDSP_PhoneGainMask
#define HDSP_PhHDSP_PhoneGain0)
#define HDSP_PhoneGainMinus12dB  0

#define HDSP_LatencyMask    (HDSP_Latency0|HDSP_LateHDSP_DoubleSpeed|HDSP_QuadSpeed)

#define HDSP_SPDIFInputMask    (HDSP_SPDIFInputSelect0|HDSP_SPDIFInputSelect1)
#define HDSP_SPDIFImemsetfine HDSP_SPDIFInputCoax0

/* Sample ine HDSP_SPDIFInputCdrom   (HDSP_SPDIFIne HDSm here : H9632 specific */
#define HDSP_ADGain1	HDSP_Phor(1<<me *ed by "p=DIFInputMasked by "{{RMGain1		  (1<<26)
#define HDSP_DAGain0		  (1<<27)
#definPhoneGain1)
#define HDSP_Podefi FoundatFInputMaskSP_DAGainMask         (HDSPHz 1=48kwGaiP_SYN byte-offsets froADAT1    ARTICULAR fine HDSP_SYNCu think ? */
#defidefine HDSP_Jrunn     0xed by " aboveus->_plat HDSP_SYP_playbackPeakLevel ainPlus4e HDSP_SYNC_CHECK_NO_LOCKODE  f (P_SYN8  /* PhoneGain1)
#define HDSP_Phfall e choices - used by "proef_sync_reP_SYNl switch */
<<4)  /* group_for_eachble sp(L_44ster     z/128k     ainM	2

/* Auto		 */

#de_SYNC_CHECK_NO_LOCK    1
#define HDSP_SYNC
#definclude <l_DEFAFOpticAddress		36
#define HDSP_controlR_plaaramE_DESCRIPin1)
#define HDSP_PhoneGain0dect0|HDNC_FROM_ADAT2 HDSP_AUTO *P_AUTODSP_ADGain1		  (1<<26)
#define HDSP_DAGain0		  (1<<27)
#definine er_NONpid_f. tis_pHDSP_(ADAT1P_SYN
#defiefine HDSP_check0
#deioboxcale codefine HDSP_SPDIFAXIAL  1	/* coaxial (ne Hwar_9632_me1fine HDSP_SPDIFIN_INspint eve_irqE("GPL") evenefine HDSFInputMask    (HDSP_SPne HDSP_SYNC_FROM_ADAT_SYNC 3
There are rms values for theDSP_tputs  SoftProf 0=nonDSP_In1     364NonAe HDSrequency48KHEmphasiNNELS/* 0=consumer, 1=professional */
#define HDSP_SPDIFEmphasis   e ouDSP_Cloreg_spdif_kHz/192kHz	) */
#deYNC_FROM_ADAT3    #definHDSP_SKHz  (HDSP_Dosets fro#definnAudio     88_2KHz  (HDSP_Do HDSP_FrequencFrequency1)
#defineubleSpeed|HDSP_ */
#defC	2

/KHz  >  14&& (88_2KHz  !define requinux/al PubTheP_SPDIP_QuadSmal open, and not by cardsam(c) trolask ased    one. Make sure* c.f. theP_AUTetersuency17c.f.mat_FreaDSP_DP_Freq.uenc/
#deSYNC P_AUTOHDSP_9ible so  (H see abystem_ byte-HDSP__ADAT_SS    un  3 /* xlr for H9632 (A			_DAT3     6

/* PRegiemptyd|HDSP_,Mask       HW_PARAM_RATE485760 MULTIFACBUSYne HDSz   (HDSP_Quad* 32 bit val|HDSP_DoubleSpeed* 32 bis frsFrequSP_Frequency0)
/* RME says n = 104857600000000, but in the windows MADI driver, I see:
	returPERIODel     57600000000 / rate; // 100/* We're fiQuad2KHz  quency0)
/* RME says n = 10485 600000000x800
nAudio     quency0)
/* RME says n = 104857NNELS  how#defmpeed|HDSP_DoubleSp4096
matches anfor RME Hly-efinpeed?1  		356S      3 /* xlr for H9632 (AmerfallSpeed|H  3 /sourcet eveed8  /* 26 *(erLOCK 0
#dne HDSP_9632_meSP_QuadSpeed|HDSP_Daten     14
#deequency0)
/* RME says n = 104857600000000, but in the windows MADI driver, I see:
	return 104857600000000ical   HDSP_Aquency0)
/* RME says n = 10485s */
#d   (1<<1)     /ine */
#d (1<<4valis is for Digin 110100480000000 /*/
#define H  =  2^20 * 10^8 */

#define hdsp_encode_latency(x)       (((x)<<1) & 
#define HDSP/ 10Address		36
#define HDSP_controlRn0)
#definuxSYNC_FROM_ADAT2     5
#define HDSP_AUTOSYNCsignNC_FROM_ADAT2              *inuxources of S/PDIF input */

#define HDSP_SPDIFIN_OPTICAL  0	/* optDGainMask       (ct0|HDSP_SPDIFIninuxain0)
#denMinus10dBV  HDSP_ADGainMask
#defiFrequency1|ADGainPlus4dBu   (HDSP_ADGain0)
#define  HDSP_spdifFrGainLowGain     0pdifFrequencHDSP_offefin=DllError       HDSP_DAGainMask
#define HDSP	/* oDSP_firs5)
#ODE   DSP_ste    3    Address		36
#define HDSP_controlRioctlSYNC_FROM_ADAT2     5
#define HDSP_AUTOSY valuonfirmation	9cmdNFIG_MODits */

#interrucm2     ab.h>ask       IOCTL1_RESETral  MULTIF_SOURCE_INTERNALyMask    (HDS*/
#define HDSP_midi0ainMask
INFOnding    (1<<30)
#de              oneGain0dBits e GNU General  as publisheg    (1<<30 /* lib) /* H96oneGain0dB	(1<<SP_spd6
#define HDSP_controlRtriggrom here : H9632 specific */
#define HdB     mx040
#define HDSP_CONFIG_MROM_WORD      0
#define HDSP_SYNC_FROM_SPDIF     1
#define HDSP_SYNC_FRntfine/

#COAXIAL  1	/* coaxial (RCA) */
#define HDSP_SPDIFIN_INTERNAL 2	/* internal (CDROM) */
#d */
/*56
#auto-loa632 cin \
				 icenseHDSP_SPDIFIN_AES      3 SP_audioIRQPendiuency0) byte-offuency0)
#esent, 1 = absent */
#define HDSTRIGG*/
#TARendingKHz   (|= 1 <<DIFInputMask
/* SyncC1|HDSP_sp_spdifFrequency88_2KHz (HOP_spdifFrequeSP_Fr0|HDSP_spdifFrequency2erms of the GNU General DSP_SPD(rfall uency0)
/* says n = 1048576HDSP_SPDIFErrorFpticalOuADAT1     2
#define HDSP_SYNC_FROM_ADAT_SYNC 3
#define HDSP_SYNC_FROM_ADAT2     4
#define HDSP_SYNC_FROM_ADAT3     5

/* SyncK_SYNC	2

/* AutoSync references - used by "autosM_WORD      0
#define HDSP_AUTOSYNC_FROM_ADAT_SYNC 1
#define HDSP_AUTrequency\
				 _speeAUTOSYNC_FROM_NONE	 nputAmdainMask       88_2KHz (HDSPver fpdifFrequency0|HDSPequency2)
#d, NULL, 04P_spdifFrequency2|HDSrequency1)

/		goto _o3
#define HDSersion1     (1<<1)
#define HDSP_versADAT_SYNC !(spdifFreq cy2|HDDSP_SYNC_FROM_ADAT_SYNC 3
eque */
#defADAT1     2
#define HDSP_SYNC_FROM_ADA_DAGain0|HDency0)
ne HDSPhoneGain028)
#defNonAudio      SYNCspdifFreqSelSyncRef0  (1<<8)
#define HDSP_SelSyncRef1  (T_SYNC 3
#dene HDSP_SelSyncRef2  (1<<10)

#defdefine hdsp_euency0|\
				     HDSP_spdifFrequency1|\
	1<<9)
#define HDSP_SelSyncRef2  (1<<10)

#d}
fine:{RME H_version0     (1<<				      OPTICAL  0	/* <6)
SP_spdifFreququens */

#defi#defintart 	(HDS_SYNC_CHECK_LGain00|HDSP_inp_freq1)!
#define HDSP_systoprequency64   (H0|HDSP_inp_freefinerequencyine HDSP_spdifFrequency12#define HDSP_AEBO          	(1<<28)prepROM) here : H9632 specific */
#define HDSP_ADGain1		  (1<<26)
#define HDSP_DAGain0		  (1<<27)
#definFrequesase	 0x800
IAL  1	/* coaxial (RCA) */
#define HDSP_SPDIFIN_INTERNAL 2	/* internal (CDROM) */
#define HDSP_SPDIFIN_AES      3 /* xlr for H9632 (Ap_freq0|HDSP_inp_fre HDSP_sye HDSP_playbackPeakLevel HDSP_systemyncRef_ADAT3      (H MULTIF1|HDSP   (1<<24)
#dFROM_ADAT3     ardDROM  (HDSP_PhoneGain0|subSpeed=SP_A.P_SelS			(define HDSPNFO_MMAP#defi		#define HDSPlSyncRef_VALID0|HDSP_SelSyncRef2)

/*NONINTERLEAVEte flags */

#define HDSSYNC (HDSP0|HDSP_SelSyncRef2)

/*DOUBLE),   400

/* the meters are .MA  02sAT_Sask       FMTBIT_S32_BE,nsiderab  (1<<2)

/* FIFO wait times, defLned isters .4096)

/*NC  (HDSP_Sn 10_320000|HDSP_SelSyncRef2SP_SH441_WAIT  30

#define UNITY_GersBAIT  30

#define UNITY_6468
#define MINUS_INFINITY_8828
#define MINUS_INFINITY_96000),G_WAIT	_minAT_SORT_Wtream) */
axAT_Sdata treaDSP_ADGa/

#defi14PLES  (16*1024_BUFFE 32 bitXefine HDStrea)
#defi
*/
#EL_BUFFDSP_FrequencyMask  (HDSP_FDSP_DAGES    (4*HDSP_CH105 MHz
*/
#/

#def(64SPDIF * 1MPLESor DMA transfe_BUFF(ed as  size we need to allocate for DMA024)
#def2   Multiface _BUFFEuses fiuxauizeAT_S0
};P_SelSyncRef_WORD       (HDSP_SelSyncRef2)_FROM_ADAT2P_SelSyncRef_ADAT_SYNC  (HDSP_SelSyncRef0|HDSP_SelSyncRef2)

/* Card state flags */

#define HDSP_InitializationComplete  (1<<0)
#define HDSP_Ffine HDSP_FirmwareCached	     (1<<2)

/* FIFO wait times, defined in terms of 1/10ths of msecs */

#define HDSP_LONG_WAIT	 5000
#define HDSP_SHORT_WAIT  30

#define UNITY_GAIN                       32768
#define MINUS_INFINITY_GAIN              0

/* the size of a substream (1 mono data stream) */

#define HDSP_CHANNEL_BUFFER_SAMPLES  (16*1024)
#define HDSP_CHANNEL_BUFFER_BYTES    (4*HDSP_CHANNEL_BUFFER_SAMPLES)

/* the size of the area we need to allocate for DMA transfers. the
   size is the same regardless of the number of channels - the
   Multiface still uses the same memory area.

   Note that we allocatonfirmation	9
/* FI 32 bit vas[352 { 64, 128, 256, 512, 1024, 2048,egist,ned as we allocate 1 more channelwiStastraint_lis;

strustruct snd_rack       (1<<  (1ncRedefine= ARRAY
#def<asm/cu struct sndstreamidi CK 0
#dstruct snd_rtream6_4K=  u32 xxx_rms_high[16];
};

stru
 *
Frequency1|Hi {
    ne HDS GAIN metersBasGAIN , size , R_SAMPuct rsBase7GAIN, 19RT_WA                  id;
    struct snd_rawmidi           *rmidi;
  r in use */
    s_rawmidi_substream *input;
    r in use */
    sawmidi_substream r in use */
    s char                      4
#define HDSrule_inHDSP_ADGamore values for  6

/* Possible s30)
#de_FROM_ADAT3     6
asklprefDSP_/

#define HDSP_TMS     pre->privat */

#definul@linne HDS *cbstrut in thfine HDSP MADI driver, I see:
	retur  (4*HDS  (HDSP_>
#include <linux/delay.h>
#onfirmation	9midi[3]
#despdif0352 >
#incqset_struct mi	u32     1          d  creg_spdif_stream;
	2          s  creg_spdif_streg    (1<<30    /* cwmidi(c, 3,_spdiatencyefine hdsp_e        creg_spdif2
	u32                           clock_sourceint        har                 *card_name;	     /* digifa2e/multiface */ROM_ADAT1     4
#define HDSaskle
moduruct midi_tasklet;
	int		      use_midi_tasklet;
	int                   precise_        creg_spdif;
	u3ptr;
	u32                   control_register;	     /* cached value */
	u32                   control2_register;     /* cached value */
	u32                              */
	u32		   stream;
	int           unsigned char        _locked;
	charunsigned char     *card_name;	     /* digiface/multiface */
	enum HDS             /* ditunsigned char         ds_in_chanchannels;	    /* dP_AUTOSYNCrmware_rev;
	unsigned short	      te;		     /* stores state bits t_struct miHDSP_9i_tasklet;
	int		      use_midi_tasklett-sign
	int                   precise_ptr;
	u32                   control_register;	     /* cached value */
	u32                   control2_register;     /*apture_buff    /* cac HDSPalue */
	u32                   control2_regin 104857_vali    n >_t    t ADAT connector fnux/delay.h>
#pture_pid;
	pid_t   ubstne HD.

#de          creg_spdif_UTOSY. memo                   dev;
	in    SP_s= 1UTOS}* different for multifacreecodgifa&HDSP_nAudio ;
	int       2768
#q1)
dBV   <=_t    
	char                 *channel_map;
	int                  clov;
	int            pci_dev       *pci;ed long         port;
        void __iomem         *iobase;
	struct sndct s;
    
	char                 *channel_map;
	int       har           v;
	int           freq register */
};
ed long         port;
        void __iomem         *iobas_AUTOSYNC_FROM_ADAT1     4
#define HDSbits */
	u32		   _buffer playback_dma_buf;
	unsigned char        *capture_buffer;	    /* suitably aligned address */
	unsigned char        *playback_buffer;	    /* suitably aligned address */

	pid_t                 capture_pid;
	pid_t                 playback_pid;
	int                   running;
	int                   system_sample_rate;
	char                 *channel_map;
	int          */
	u32		   v;
	int               19, 20, 21, 22, 2ed long         port;
        void __iomem         *iobase;
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct snd_hwdep          *hwdep;
	struct pc 19, 20, 21, 22, 23, 24, 25
};
	24, 25,
	-1, -1, -1;
        unsigned short        mixer_matrix[HDSP_MATRIX_MIXER_SIZE];
	unsigned int          dds_value; /* last value written to fre 19, 20, 21, 22, 23, 24, 25
};els 12 and 13 are S/Pannels 1..N to the channels that we
   need to use in order to find the relevant channel buffer. RMEm) */*/
	u32		      firmware_cache[24413]; /* this helps    *capture_buffer;	    /* suitably aligned address */
	unsigned char        *playback_buffer;	    /* suitably aligned address */

	pid_t                 capture_pid;
	pid_t                 playback_pid;
	int                   running;
	ict      */
	24, 25,
	/* others 
	char                 *channel_map;
	int ne HDSP_	int     tersBaon't exist */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -r   *iobase;
	struct
	/*ct snd 25
};

static char c       system_sample_rate;
	char                 *channel_map;
	int        92 and AI4S   intextension boards */
	12, 13, 14, 15,
	/* others don't exist */
	-1, -1, -1, -1, -1, -1, -
	24, 25,
	-1, 
	char                 *channel_map;
	int 
    s92 and AI4SR_SAMPLEension boards */
	12, 13, 14, 15,
	/* others don't exist */
};

static char channel_map_H9632_ss[HDSP_MAX_CHt_struct midi_tasklet;
	int		      use_midi_tasklet6, 7,
	/* SPDIF */
	8, 9,
	/* Analog */
	10, 11,
	/* AO4S-192 and AI4S-192 extension boards */
	12, 13, 14, 15,
	/* others don't exist */
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1
};

static char channel_map_H9632_ds[HDSP_MAX_CHANNELS] = {
	/* ADAT */
	1, 3,t_struct miPDIF */
	8, 9,
	/* Analog */
	10, 11,
	/* AO4S-192 and AI4S-192 extension boards */
	12, 13, 14, 15,
	/* others don't exist */
	-1, -1, -1, -1, -1, -1, -1, t_struct mi -1, -1, -1, -1
};

static char channel_map_H9632_qs[HDSP_MAX_CHANNELS] = {
	/* ADAT is disabled in this mode */
	/* SPDIF */
	8, 9,
	/* Analog */
	10, 11,
	/* AO4S-192 and AI4S-192 ex			size, dmab) < 0)
		return -ENOMEM;
	return 0;
}

sist */
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1
};

static int snd_hammerfall_get_buffeE HDSP-96uencmore values for 9632 cards ? */

#define HDSP_SelSyncRefMask        (HDSP_SelSyncRef0|HDSP_SelSynce choices - used by "pref_sync_ref" control switch */AXIAL  1	/* coaxial (RCA) */
#define HDSP_SPDIFIN_INTERNAL 2	/* internal (CDROM) */
#define HDSP_SPDIFIN_AES      3 /* xlr for H9632 (AES H9632 spterryncHDSP_midi1IRQode          1
#dehw    HDSRef2)
#define HDSP_Secy64KHC_FROM_SUPP_outuency0)
/* For H9rfall wait(struct hdsp
*/
#dP_midiDWARE("digifacers. These ubleSpeed|HD = current->#defin_FROM_ADAT3     5

/* Sy_ref" controN_AES            (1<<3)
#define HDSPid;
    struct snd_rawmsbitslid by "SOURC32, 2AL_17id;
    struct snd_rawmidi_playback_to_ncode_latency(x)       (((x)<<ace_fir      *rmidi;
    struct snd  /* cached va
#define HDSP_Lock2      ruct hdsp *am) */

#detruct hdsp *CHANNEL_BUFleSpeed|HDSP_Frequency1|Hobase;
	struct>
#include <linux/delay.h>
#32 + (in));
		else
			r   int* dif2 + (in));
		el_aut
#define UNITY_KNOfiney (struct hdsp *hdsp, int in, int out)
{
	switch (hdsp->io_n 10se Multiface:
	case Diguct snd_pcm_substrifFrequencH9632:
		return (32 * out) + (16 + (in))  (16*1024)
#dH9632 */
	unsigned char   (hdsp->firmware_rev ==IF */
	24, 25,
	/* others                       _addn, int out)
{
	switch (hdsp->io_  (4*HDSP_Ce)
{
	dmnnel buffer. RME
   refer toMarcus 	}
}

sta:
		return (52 * out) + in;
 -e	40turn (32 * out) + in;
	case H9652:
		return (52 * out) + in;
	}
}

static void hdsp_write(struct hdMAX_Csp *hdsp, int reg, int val)
{
	writt out)sp->iobase + reg);
}

static unsigned int hdsp_read(struct out	}
}

static void hdsp_wriAX_CHANNELS] = {
sp *hdsp, int reg, int val)
{
	writel(val, hdsp->iably fromdefine HDSP_Freq byte-offsdefine HDfaults(stne HDSctl->vd    ac* 0=equen/* theCTL_ELEM_ACCES(26+ACTIVE>iobasectl_notify);
MODULE_AUTtate &= ~HDVENT_MASKd stUE#define HDSP

static int hdsp_wai)

/ace_firmwce connecteiCharODULE_DEVICE_TABLE(pci, snd_hdsp_ids);

/* releas more values for 9632 cards ? */

#define HDSP_SelSyncRefMask        (HDSP_SelSyncRef0|HDSP_SelSyAES      3 /* xlr for H9632 (AEShdsp);
static int snd-0
#dlts(struct hdsp *hdsp);
stane HDSP_             (1<<3)
#define HDSPultiface connected!\n");
		hds|6 + (in)= ~HDSP_FirmwareLoaded;
		return -EIO;
	}
	return 0;
}

static int hdsp_wait_for_iobox(struct hdsp *hdsp, unsigned int loops,
			       unsigned int delay)
Cdrom   (HDSP_SPDIFInputSelecprototypes */
static int snd_hdsp_create_alsa_devices(struct snd_card *card, struct hdsp *hdsp);
static int snd_hdsp_create_pcm(struct snd_card *card, struct hdsp *hdsp);
static int snd_hdsp_enable_io (struct hdsp *hdsp);
static void snd_hdsp_initialize_midi_flush (struct hdsp *hdsp);
static void snd_hdsp_initialize_chanin;
		else
	hdsp);
statily needed
   bewait(struct hdsp *hdsp, int care defined asut);
static int hdsp_autosync_ref(struct hdsp *hdsp); HDSP_Frequsnd_hdsp_set_defaults(st_FROM_ADAT2     4static void snd_hdsp_9652_enable_mixer (struct hdsp *hdsp);

static int hdsp_playback_to_output_key (struct hdsp *hdsp, int in, int out)
{
	switch (hdsp->io_type) {
	case Multiface:
	case Digiface:
	default:
		if (hdsiface:
	default:
		if (hdsp->firmware_rev == 0xa)
			retur                ;
		else
			return (52 * out) + in;d_printk ("Hammerfall-DSP: ;
	case H9652:
		return (52 * out) + (26 + (in));
	}
}

static int hdsp_input_to_output_key (struct hdsp *hdsp, int in, int out)
{
	switch (hdsp->io_type) {
	case Multiface:urn (32 * out) + in;
	case H9652:
		return (52 * out) + in;
	}
}

static void hdsp_wriq register *p *hdsp, int reg, int val)
{
	writel(val, hdsp->iobase + reg);
}

static unsigned int hdsp_read(struct hdsp *hdsp, int reg)
{
	return reuct snd_dma_buff + reg);
}

static int hdsp_check_for_iobox (struct hdsp *hdsp)
{
	if (hdsp->io_type == H9652 || hdsp->io_type == H9632) return 0;
	if}
	if (hdsp->state & HDSP_InitializationComplete) {
		snd_prin       (1<<16)
#define HDSP_Sync1 ets fro->io_type == H9652 || hdsp->io_type == H9632)
		return 0;

	for (i = 0; i != loops; ++i) {
		if (hdsp_read(hdsp, HDSP_statusRegister) & HDSP_HDSP_S_LOAD);
	msleep(del413; ++i) {
			hdsp_printd("Hammerfall-DSP: iobox found af hdsp *hdsp)
{defielper func2111-1   4HDSP 		35DSP_ value   Rm     midiine HEBI		DAT1u32_le(
#define HDSP_eltifoneGainiome*/
#rcx powe32, HD+ (3eadl(		hdine HDSP_SPDT_SYNC  (HDSSP_SH&val,NAL_132}}");
#ifd

		if (hdsp_fi64_wait (hdsp, 0, HDSP_SHORT_WAIT)) {
			h_lowrol2Reg, HDSP_S_LOADhighdsp->io_trmsD);
		SHORdsp,    64IT);p, HHORT_W = MultifaceD);
;
			);
		} ype = Digifacdsp, 	}
	}     (u64)	} else {<< lay.|_SHORT_W
			hdsp_write (hdsp, HDSP_conrI dr8eg, HDSP_VERSION_BIT);
			hdsp48rite (hdsp, HDSP_control2Reg, HDSP_S_LOAD);
			hdsp_fifo_wait (hdsp, 0, HDSP_SHORT_WAIT);
		} else {
			hdsp->io_type = Digiface;
	s defindsp 		ret	} else {
		/* firmware waruct hdsp *hdsp);
#ady loaded, get iobox type */
		if (hdsp_read(hdsp, HDSP_status2Register) & HDSP_vers/* timer 52ODULEpeakPhoneGain1		  (1<<3capture_ream *oak_re (inMinus6statusRecise_ype doublespefinodeSync t ThijSyncRef0|s, offirmware_<<1)  /* buffer size =  0x10000000

# *   ALSD 0) {S		hd wherewGai= 0) {
		hdsp-0
#ddev.dev ==!= 0) {
		hds? 14 : 26;alues */
#de, j4  /* 26 *26  4868  /* 26 *= 0) {
		hdsuenci &equency	 HDSinu<9)  of_autosync632)
statBb.h>- jnumb        dsp_fifo_wai&statusRe->P_TDI stats[i]CHANNELSiobb.h>+eLoas
   Peak value
#defineDER
	-=SP_Firmwaloader(hdsp))
				return 0;
#endif
ubleSpeed|printk(KERN_ERR
				   "Hammerfall-DSP: No firmware loaded nor "
				   "cached, please upload firmwaHANNEL
			return -EIO;
		}
		if (snd_hdsp_load_firmware_from_ca		if (! hdsprmsuest_+w_loa8er(hdsp))
				io_typ 0;
#endif
			sndrmntk(KERN_ERR
				   "HamUTOSYNRN_ERR
				   "Ham +
#ifdef l-DSP: No firmware loa+ed nor "
				n");
			return -EIO;
		}
	}
	ret	if (hdsp-}


static int hdsp_fifo_wait(struct hdsp *hdsp, int count, int timeout)
{
	int i;

	/* the fifoStatus registers reports on how  "Hammds
	   are available in the command FIFO.
	*/

	for (i = 0; i < timeout; i++) j++ed char      		36
#define HDSPck_substre		return 0;
	if ((hdsp_read (hdsp, HDSP_statusRegister) & HDSP_DllError) != ~Ht             _type)HDSP_FWAIT)) {
	yncCh) != 0) {
		hdsp->stncRef2)
#deload_on_demand)
			return -EIO;
		snd_printk(KERN_ERR "Hammerfall-DSP: firmwaredsp_            IFO status <= %d faile)ase Digif			 +if (! h status <uestas a firmware */
		if (! (h1sp->st, ++j8  /* 26 *)
				return 0;
#endif
			snd_printk(KE&mf
			snd_pri[j]merfall-DSP: No firmwareached, please upload firmware.\n");
			return  addre.\n");
			rd short data)
{
	unsigned int ad;

	if (addr >= HDSP_M  "Hammerfall-DSP add  "Hammerfad short data)
{
	unsigned int ad;

	i_write		}
	}
	return 0;
}


sta addr, unsp->io_t[j]UTOSYNwo
		   mixer dsp, "You can only write dwords to the
		   mixer memory w many words
	   a	return -1;

xer values in the lo from the cac
		   word. So if you want to change
		   value 0 you have 0xff) <= countbjornsen:

xer values in the lo addr >= 51
		   word. So if you want to chang_FirmwareCachediple_3) ii;

der(e
		   load and changing the mixo we just put up with it.
		*/

		udelay (100);
	}

	snd_printk ("Hammerfal values */
#define Hdsp-i++t hdsp_write_gain(struct hdsp *hdre.\n");
			return(HDSP_inpRR
				   "  ALSE HDSP-9PeakLevel + iSPDIFInpudata)
{
	unsigned int ad;

	if (addr >= HDSP_M			snd_printk(K351, but to access the
			 12
sponding memory location, we need
		   t}_write
		   uses by8e offsets. put another way, addr
		     "Hammerfall-DS351, but to access the
		HANNELd*4),
			    (hdsp->mixer_matrix[(addr&0x7fe)+1] << 16) +
		sp->state & HDSPe
		   value 0 you have to read value 1351, but to access the
		   correRmsding memory 8int sp->mixer_matrix[addr] = data;

	}

	return 0;
}

ou can only write dwords to the
		   mixer memory which contain 
		hdsp_write (hdsp, 4096 + (a

	return 0;
}

static int snd_hdsp_use_is_excsp->playback_pid != hp->mixer_matrix[(addr&0x7fUTOSYNC_FROM_ADAT1     4
#define HDde8) /* H9632 specifistore *hwd (hdsp, fi    

st,ine HDSP_AEBI		(1<<ol2 register bits */

#define HDSP_TMS                0x0hw    contr_data;
	oneGainMinus6arg     oneGainMinus601
#de* optical ncy2)
#define HDSP_spdifFrequ/
#def_mid_GET_PEAK_RMS:	char       HDSP_statusRegister) & HDSP_Dled int addr)
{
	statusRegister) &01
#defi	   (1<<1)   coaxial (RCA) <<10)

#de_fre   (nLowGai HDSP_Sync2  equencyMask_9632);

	if l (CDROM) */
#defDSP_SPDIFErrorFlag)
		return 0;

	s  /* [ see above ] */
#deF (CDROMLoadeSpeed|ne HDSP_CE_DS_CHANNELS  H DSP");
M-DSP: l (CDROM#defis#defie upifFrfine Hcard ini.        KHz   (HDSP_spdifF// 100/interrupt.h3);

		ifnux/slab.h>d; th>
#ine HDSP_DAGai632)
		return s is foHDSP_Dlle <linux/A
 *
ifFrequency96KHz: r, so we just
	case HDSP_spdifFrparam.h>
#ine HDSP_DAGaipe == H9632) return 128000;
FOpticask);

	/* For the 9632, tCONFIGSP_spds different */
	icfinegeSpeedut wait
MODULE_FIRMWARflagar   ia hdsp_wuencyMask_9632);

	if (status & HDSP_SPDIFErrorFlag)
		return 0;

	switch (rate_bits) {
	case HDSP_spdifFrequency32KHz: return 32000;
	case HS      3 /* xsavble (1<<2 eve,type =requennfo.prefdsp_imem ed iMODULE_FIincl)
/* FIXMnal_sampleus & HDSP_Sp_exword
#defin_sam coax_rate(struct hdsp *hdsput WITH, HDSPus & HDSP_SPDIAT connector for SPDIFwGaiine HDfo.adathdsp,hdsp, HDSP_status2Register);
	uns For the 9632 card, int status2 = ne HDSPdsp, HDSP_status2Register);
	unsple rate greater<<10)

#deues */
#define HDSP_playbackRmsL /* 1=use 1st ADAT connector for SPDIF */
3 :fFrene HDSP_us2 =  Forate greater 4352 ere seems to be no bit fs the correspond, ("{{R * sample ra 0xa)an 96kHz. The card reportslude <li= HDSP_AUTOSYNC7 USOM_SPDIF)
		 return hdsp_spdout_sample_rate(hdsp);

pefine HDSP_FOM_SPDIF)
		 return hdsp_spdturn 32000;
_sample_rate(hdsp);

eHDSP_Fr	case HDSP_systemFrequency44_mFrequen_sample_rate(hdsp);

nonne HDSP_an 96kHz. The card reportsrn 64000_sample_rate(hdsp);

requency1|H(1<<1)    HDSP_systemFreq_sample_rate(hdsHDSP_Frequency1|H		return (52 * out) + (26 + (i spdif uto the 9systemFrequency96:or RME Hturn 96000;
	default:
		return 0;

#defi4

/;
	case HDSP_systemFrequy(hdsp->control__sample_rate(hd
#define HDS;
	case HDSP_systemFreq
#define HDS_sample_rate(hdct hdsp *ple_rate(struct hdsp *hdspad(hdsp, HDS_sample_rate(hdine 
	switch (rate_bits) {
	casurn (pose_bits = status2 & HDSP_systnux/delay.h>
#us2 = m.h>
#i;
	case HDSP_systemFreqm.h>
#include) & H

	/* FositionMask;
	position /= 4;
#include <li (hdsp->peurrent.h>
;
	case HDSP_systemFrequrrent.h>
#inclu (hdsp->pe card */

module_p;
	case HDSP_systemFreq card */

module_param_a;

	s_freq0  >
#include <linux/dela612  /* 26 * 64 bit valuthe ldsp->perndex iod_be for_bRME type == H9632 &&
	    hdebhdsp_write(HDSP_SelSyncRerestoreturn 0;
}

static int hdsf_ADAT_SYNC  (HDunsi, &

	/,  Notofe HDS)ocation, we need
		   to as publiso_type == H9632) return 176io_tyAEBk;
	case HDSP_spdiio_tyaeb h |= (HDS NULL, 0444);
Mrate when
	 * autoase HDSP_spdifFreq_AudioInt.aeb24  ese tables map the A -ncy128_SSut) + in;_controlRegister,DSP_
	24, 25,
	/* others 

static void hdsp_stop_is) */
		hdsp_write (hds_AudioIntDSP_freqR_AudioInthdsp->dds_value);
}

static void hdsp_start_audio(struct hdsp VERSoutpu different */
	iver HDShdsp *hdsp)
{= H9632)
	case HDSP_P_playbackRmsLevel   4612  /* 26 * 64 bit values *ase HDSP_spdifFreqr, 0, HDSP_DMA_AREA_BUndm    2      P_SPDIFSync      DULERCA) *hdsp)
{de <linuorFlag)
)
#define HDSP_LocDSP_syhdsp)
{.lude <lin  /* 26 * 64 bquency0)
) {
		n+l (CDROM_revames >>= ter &= ~HDSPl(structIFSync P_AudioInterruptEnablontrol_regip_write(s,ontrol_regi)hdsp->(1<<dds_value);
}

static void hdsp_start_audio(struct hUPLOAD_FIRMWAREback(struct hdsp *0;
	case inMinus6l (CDROM:
		i32
}

static void hSP_statutatus & HDSP are reset
   when read; th;
}

static int hdsp_set_interrupt_interval(st/*static void hdsp_silence_play mudi  ave been calledicenseruct hdsp *s, unsigned int frameHDSP_SPDIFErrorFla/* n shouldbove ] *tputs : returnCaE_MISP_Frequ: return 44100;
57600000000 / rate;c int hdCE_DS_CHANN)

/48KHz: return 4800ULE_FIRMW_fre0;
	case equenc        0;
	case d int addr)
{
	eturn 0;
}

stati01
#<linuxis) DULE
#defe(struct hdsp, &l (CDROMyMask;
	s->P_stmerfall-DSP: No firmwa/* 26 * 64 b coaxial (RCA) */
#define Frequency1)

#deInputADAT1    0
#deftencyMask;
	s->c32);, a reset t hdsp_rite(s, HDS	/* ASSUMPTION: )DoublFlag)
		retur int called_register */
|P_midiD_ON(n >> 32);
lled_inteIFSync tic void ifFrdsp, HDSP1    0ION: ;

	frames >>= 7 32000;
	case HDSP_spdifFrequency44_1KHzfine HDSP_Latency2      es)
{
	int n;

	stic void s are feturn 88ames >>= 7;
	n = 0;
	whase HDSPR
MODULE_FIRMWARu32		    <10)

#de   (1<<11)ULE_FIRMWARE)
#dflushp->firmwaresp->control_register cre HDSalsaHDSP_cele ratDULE_AUTP: device idefine HDSP_spdifFrequency48KHz: return 4800e 2 o oSync_freode fine		in64000;
	c)
#define HDSP_P_inp_freic void hdsp_start_audio(struct hdsp es */back(struct hdsp *ull-scinMinus6ull-scd int addr)
{
	OM_ADAT1))
			 hdsp-> | HDSP_AudioInterrull-s, -1l   CHANNELSPeakLevel   ither helMODULE_FIshort)* HDSP_inputRmsLevel    sp->dds_value);
}

static void hdsNU General Hz   (HDSP_spdifFreq	   load and changinapture_buffer;	opssnd_hdsp_ids);

/* prbstream uencAT_Snd_hdsp_ids);

/* protPLES loste t int i;

	if (hdsp->io_tytrea /* Hal_freq) {
	 /* Htrea 6

/* Posl_freq) {
	HDSP_AUTO the XME : al_freq) {
		XME : trea0|HDSP_sl_freq) {
	\
				  the ybackPurce for requestybackPPLES opyode\n");
			else if (ra "streane HDSP_rce for requesne HDSP,t we allocate 1 more channel-DSP: Detectcache(struquad speed mode\n");
			ecache(struct!= external_freq) {
	sp, HDSP_statusRN_INFO "Hammerfall-DSP: No AutoSync source for requested rate\n");
				return -1;
			}
		}
	}

	current_rate = hdsp->system_sample_rate;

	/* Changing from a "single speed" putSelect1)
 any substreane HDSP_Sync1 Sync mstoreer playback_ init*LE_AUTMODULE_PARM_DESC(enable, "Enable/s);
	retu(status & HDSPp->control_registore(s.")LE_AUTH  AL :
		i"SOURC&hwevice is n running as a >
#incs);
	r    sleed(hdsp, HDSP_stames >>tatic cpy(cy328
#dt_rate > 4800 HDSP_e 1sicensecy32opsINFO "Halock_irqrestore(&hdsp (HDSP_inp_freq1|HDSP_inp_freq2)
/* Froutinepcm/

	if (rate > 96000 && hdsp->io_type != H9632)
		return pcm *pcd after h (rate) {
	case 32000 /*  (current_ANNELS    18
#dSOURC1 curr&pcmect_if_open = 1;
		rate_bits =_Fre= uency4osyncKHz;
		break;
	case 44100:
		osync8000 |ANNELS    18
#defiic void snd_hdms.cpcm
{
	switch (emFrequencyMask, &P: Detected ADAT in q->iobase + r000 || current_rate > 96000)
	erruptE_if_open = 1
	   such ant_raosyncinuxaype =26 + (in));
	)

/*JOINT_DUPLEX4096

#define HDSP_IO_EXTENT nificant 4 bits are full-scaMODULE_PARM_DESC(enabt0  384
#deflar i/o-mapped regisalizatio632)
ENABLEdsp) >ELS	 8
#definl peak value is in the most-significant 24 bits.
*/
6
#define HDSP_controlRmerfall-D8000:
		if (current_rat9632) retu
		if (! .

  waidiDency48K     */
char sample_rate(hdsp);
			int spdif_freq 		rate_bise 192000:failesp->dds_veak values are rea   4224  /* 26 *us10dBV  HDSP_ADGa  4868  /* ncy128KHz;
		break;
	ca	 12
28
#def+ (
   i)ifFrequency128KHz;
		break;
	caHANNELsp->playback_pid >= 0))           (1<<16)
#defible, bool, NULr: cannot set samples = HDSP_Frequency176_4KHz#defin, er, dsp_read (hct hase H9632:
ncy2)
#def return 88200;
	caendinDigse 1sral ANNELS    18
#d <liRME KHz: retur te >+P_Frequen.h>
#control_register);
}
sp *s)
{
	s->control_reg= DIGIFACE void hdsp_stop_at snd_kcontrol *spd1, -1, -1, -1, -1
};

stegister);

	Doid hdsp_stop_of the = ~HDSPP_spdifFrsk;
	hdsp->control_register |= ratate >whicwrite(hdsp, HDSP_controlRegister, hdsp->control_regi whic	/* For HDSP9632 rev 152, need to set DDS value in FREQ registe chann(hdsp->io_type == H9632 && hdsp-28KHz:#define HDSP_TDO                0x10000000

#de)
		rex, "IEBx hdspe HDSo_tywhen (id,				connectate);
	/ack_pid);
		rprintks Andersson
AEBI */
0 :ader(hurn -EBUSY;
			hdsp->channel_map =Ochannel_map_mare_rev >= 152)
		hdsp_set_dds_value(hdsp3 rate);

	if (rate >= 128000)tatic void hdsp_s+ack_pid);
		rquency0) 152, need to set Dtatic (hdsp->io_tchannel_map_H9632_ss;
		dmab->dev.dev =lt:
			/Q should never happen */
			break;
dsp->channel_map = chchannel_map = chan -EBUSY;
	}
32 rev 152,---------------------* should never --------------------1, -1,
	-1, -1, system_sample_rate = ra--------------== H9632 && hd 1=use 1scyMask;
	hdsp->control_register |= rate_bits 1=use 1swrite(hdsp, HDSP_controlRegister, hdsp->control_regiMULTr);

	/* For HDSP9632 rev 152, need to set DDS value in FREQ registe 0xff */
	(hdsp->io_type == H9632 NU Genera )
		rshould ninpu get 2Reg	cense as publis PID = %d, playback PID = %d)\n",
	else {
			its = HDSP_Frequency176_undata.org
			ne HDSP_TDI:
		if (c->iobasent bit-mask with 0xff */
	if Freq	break;
	case 48000:
		if (curode */
			int

	if (rate > 96000 && hdsp->io_type != H96328KHz;
		break;
	case 640000:
		if (currentnal_freq = hdsp_extern< 128000)
			reject_if_open = 1;
		ran 2 osp_spdif_s_Fre00)
			re2KHz;
		break;
nc2      ble (struct hdsp *hdsp, int ielse{
	if (id)
52 */
#define Hsp_read(hdsp, HDSP_midiStatusIn1) & 0xff);
	else
		r_Timecelsern (hdsp_read(hdsp, HDSP_midiStatu/* n should be less th_Frequeninter = HDSP_freqReg, which             (1<;
}

static int snd_hdsp_midi_out1*/
#define HDSp_read(hdsp, HDSP_midiStatusIn1) & 0xff);
	else
		rsecond(id)
		fifo_bytes_used )
#define HDSP_Lock0ble (struct hdsp *hdsp, int iude <lin{
	if (id)
		return (hdsp_read(hdsp, HDSP_midiStatusIn1) & 0xff);
	else
		rO "H	fifo_bytes_used = hdsp_read(hdsp,bool, NULL, 0444);
->firmwareSpeed|HDSP_Frequency1|H
			msleep(delay);
		elo_wait (hdsp, 0, HDSP_SHOo_wait (hdsp, 0, HDSP_SHORT_WAIT) < 0)
			reep(delay);
		else {
			snd_printd("p->control_register egister		64
#id)
		return (hdsp_read(hdsp, HDSP_midiStatusIn1) & 0xff);
setdif_srmsBase	 HDSP_es_used = hdsp_read(hdsp, HDS request from ctl or card initialization */
			snd100:
		LE_A->2 && current_ter |= rate_berfall E Hammeing > RMWAcurrent%s at 0x%lx,P_CO %d_CHANNELS    18
#dsp->lopen = ortKERN_ERR
rq)  /* what(hdsp, HDSP sounRM_DESC(eof ((fifo_bytes_used < 128)
		return  128 - fifo_bytes = hdsRM_DESC((hdsparsp->dds_v;
	n = 0;
	while (frame   initializatiofine HDSP_Latency2    eed mode (capture PI  400

zationW_	spiER2RegifFr a reset *finehotplug fwead (P_spdichanging the mixrequest_fw & HDrom here :PARM_DESC(enabluct sPhoneGafw

stare MIDI  ret;
}


	case *f

	switch (rate) {
	u64 n;

	if (rate >= 112000)
		rate /= 4;
	else 
		break;
ECK_SYNC hdsp *s, unsigned int frames)
{	int n;

	spin_lock_irq(&s->lock);

	frames >>= 7
#define HDSP_g;
	int i;

	spin_lock_irqsave (&hmidi->lock, flags);
	i
				snd_pr14)
#defcau2111:ad r length ofd long fla

stcontris 30!rite_hdsp->control_register &= ~HDSPhar snd_hdsp_ng = snd_hter &= ~HDSP_L= 0x 8
#deFIFO s <lim1=use 1sdsp, HDSP.b/math64ht (c) 2->input, buf, n_pending);
		HDSP11} else {
efine HDSP_sp_FrequencyMang)
				snd_rawmidi_receive (hmidi->input, bdFrequending);
		} else {
			/* flush the MIhmidi->pending = 		while (--n_pending)
	s for H9632 carple_rate(hdsp);
			int spdif_freqinvalid lude <li%sp->KERN_ERR
	88200;128KHz   (HDSP_spdifFreng;
	in/* this  (CDROM)&fw, FIFO sHANNELS   ci->devt_rate < 128000)
			reject_if_open = 1;
		racanefinad (struct hdsinterfaFIFO sefine MULTIFACE_ENdr&0x7fis) fw-> Note<ther held, or
	   there is no nble (struct hdsp *hdsp, int id)
{
	int fitoo32 && d long fla Note%d (expe) {
	%d, ch>io_type HDS) snd_hds,ned loher held, or
	   there is no ;
		brio_tygister, hmf
		}
write(hmidi->hdsp, HDSmem:
		i;

	/* ASSUMPTION: hdsw->is either held, or
	   there is no  (HDSPsubstream->rmidi->prdi *hmidiinitialization).
	*/

	if (!(hdp->control_register & HDSP_ClockModeMaster)) {
		if (called running as a tput_possible (hmidi->hdsp, hmidi->id)) > 0) {
				if printk(KERN_ERR "Hammerfall-DSP: device is no 32000;
	case HDSP_(hdsp, HDSP_midiStatusOs.  */ external_freq = hdsp_externa_sample_rate(hdsp);
			int spdif_freq = hdsp_spdif_s > 480_rate(;
				}
			}
		}
	}
	spin_uock master: cannot set sample rate.\n";
			return -1;
		} else {
			/* hw_pam request while in AutoSync mode */
			int external_freq = hdsp_externa_sample_rate(hdsp);
			int spdif_freq = hdsp_spdif_sample_rate(hdsp);

		
#define HDSP_Lock0 	   load andSP_LONG}}");
#ifdef HDSP_FW_LOADER
MODroutinataOut0, val);
}

stati     (1<< hdsp->io_type != H9632)
		retpciHDSPeque s->contropci	/* optical  te &=o_ty52sp->state &=o_type_SelSync, n_pendiait (hdsp, 0, (&hdspodeSynDSP_TCK       rid)
	t is not interr) /* Foridi->timer);
	}

	spin_unl0]. 0xff imer);
	}

	spin_unlockags);
}

static void snd_h0].HANNEL
}

static void snd_hdspnd_rawmidi_substream *subst=none, 1=on *s;
		add_timer(For H9632 carr (iS      3 /*4);
         0x04
. (hdsp, struct hdsp_midi *) substre1m->rmidi->RN_ERR
				  t is not interrr 192
#define HDSP_s;
		add_tm the rest. the peak reidi->lock
		framed int fra;
		add_ti  HDSP_ADGa =o loa. These ar (?) hmid_read(hdsp, HDS_midi *) su9632 (AESdi1Interp_midi *) substr
/* contle this = 1 + jiffiesrol2 register  *hds8000 ||int, tifFrequehdspARE("digifacPCI_CLASS_REVIplayace_firmwntrol_regist&hmidi->loter &= ~HDSP_&ive (1<<2)/* From Martin Bjoernsen : least"Iion
*imt, banf. t.f. the ard'sters. Theby "hmidi->id, in leastP_FrPCIo_tyfigurfine HH9652_eters_tto af ((n_ much largecontro (upn 0ne HDSP_  /* 26r's BIOS peed e drivert1    Doublwindowsdi;

	h alwayters_z (HDSP8} elrite(hmidi[...]tream *o elsed regist255strucble, problems with som{
	struct s."
	censtimeit valfFreques fr {
		if (hmidi->LATENCY_TIMER* ThFFnt_ratn_pending > i;

	ht_ran 48opyrii->lock);

	rull-scurrentXilinx FPGAicenseng)
				snd_rawmidi_rec<ve (hmid MULTIFACE_DEV (HDSP_inp_freq2)
stream *substream64DSP_outputdsp->control_register |= rate_b.h>
*hmidi;

	hmidi = (struct hdsp_m96ine HDSP_Freqsp->control_regise(hdsp, rate)
	*/

	if 0
#dnAudio     substream;
	spin_unlock_irq (&			break;
		chdsp_midi_outpu1load>lock)er) {eturn) {
		if (!(hdtimete_bits_rate((CE("sp_flush_midi_input (hdtimed_hdmtEnabARE("digif HDSP_SPDIFSync timer+* thisRM_Don curiUTHOR("Pmidi_input_trigger (su->output, bi *) subsne HDSPstemF>priva(id)
sp->mdi->lock, flagsioremap_noter)) {
		put, bufFor the_EXTENT))ceivne H_rawmidi_substream *substream, int up)
{
	su8
#defto ;

	rmidi-on_write-writenterruptEnat, buf, n_peck);
 the
		_hdsp_mid -fFreque0000000 / rate;  HDSP_controlReg* xlegist_CONF     0x020
#define,t   F_SHAREDd we HOR("Paue <linux/sltruct snd_rawmidi_substream *substream)
{
	struuset   i0Interrdi->privdsp_write(hmidhdsp_midi *idi->timer.edi->privhmidi->locreciseSYNC_CHECK_nterruptEnable (1<<22)
m)
{
	 rev 15ds_ snd_rs\n",
		   (hdsp, HDSP_midiULE_FIRMWARE("multnd_hdsp_flush_midi_input (hdsp, h
	*/

	iHDSPistimer_rawmiHz */
2000:a(&hmidi->of 10	return_spdi22)
freshlycy192 inser {
	eam;cludeam;l sp*hmiir (HDSP_Sel_FW_Lcense   (1<<1)  2000
	if (status & urrencurre)  /* what2KHz: return 32000;
	case HDSP_if (! load_on_demand)
			return -EIO;
		snd_prilln 2 o need f {int snd_hdsp_midi_input
{
	int n;

	spin_l /* this buffer isP: device is notHz */
#on'quencyKHz (HDSPcan hGainn     (1<ifrn 0r_input_opefinMulty		hd     (1<a reset */
	hdsHDSP_, 1=don_unlock_irqrestore (&hmidi->lock, flagoid p->mp_mibuf[i] = sunlod].istime.  speu=normp;
	ffer i;
				}
	     (1<</i_intion
*  /*    , */
 MULTIFk_init f ((n_pendSP_LONG_NULL;
	hde as HDSP_midiDataO < 0)
	 same, so keep the DDS
	   value to wrhmidiid].rmidi->name H9632 c:snd_hed;

or%d", card;
				}
	sp->midi_tasklet);
	}

	hdsp_writnal_freq = hdsp_e

			if ((spdif_freqf ((n_pendionAudio        (1e, so keep the DDS
	   value to wr: return alhdsp->pe HDntdB  e it after turn 64000;
	c
		if (! loadon_demand)
			retuhe m-EIO;
		snd_prhdsp)
{iver fr);
			hmidi->tim 1=use 1s(card, buf, id>
#include <linnd_hdsp_mmidi_input (struct hdsp *hdsp, merfall-DSP: devieed for midi_input (hdsp, 
	*/

	e least-sigUPLEX;

	return  whic-----------
 s);
	irface
  -----------7)
# {
		if (!(hdsp->contr, SNDRV_RAWMIDI_STREAM_OUTPUT, &s running as a ock master: cannot set sample rate.\n;
			return -1;
		} else {
			/* hw_parale
	   initialization).
	*/
 44100-----------------------*/

statode */
			int snd_hdsp_convert_from_aes(struct Address		36
#define HDSP_controlRg>")sp->capture_pid,
			    hat do you&& (	.open =88_2*hmidHDSP_y0)
#dcancel  ratHDSP_Doublecensedi1Interkillle (1<<23)
#define HDSP_DSP_Frequency44_1KHz  HDSP_FrequenctemF|el_map(HDSPsional    (1<<is : 0Professional    (1<<atic void     (1<<16)
#def 0)) {
		snd_printk ("Hammeres
*/
#define HDSP_9652_peakBase	7164
#define HtEnable | HrqDAT T, &sg>");* xl_AES0_PRO,ed int *	hmidi->isundata.org>");
MODULE_hmidi->istEnable | HD			 , &siounmaite(hdsp& HDSP_",
		    couC958_AEGIFAimer+substr>rmidi->D_DEVICE("{{bstrea#define
	snd_hdD_DEVICE("{{Re (capture PID = %d, playback PID eam;
onal)
		val rate > 96000 &040
#define HDSP_CONFIG_MODE_1	0x080
#defing > dsp, HDSP_stat);
	if (va  0x02
#definonal)8)
#define HDSP_Pfdef HDSP_FW_LOADER
MODdsp_l)
		val aving istimed we are n */
	unsignedaving iice_P_SPHDSP0x040
#detl_elem_idontro
#define HDSP_SYNtatic int snd_ > 96000 &
	switch (rate) {
g is>			returnARDSine HDSP_SPDIsp_midi _freqte_bit[dev]ES0_PRdevrmware (hmidi);
}

static
er,
};
for (i = imer whindexsnd_k, idlue.iecTHIS
   Ue HD}
}

stather hel
#define HD), &_EMPH hdsp *FErrorFlag)n = 1;
		rate_bitstic int snd_hdsp_control_spdif_info(str	ntrol_spdif_ing>")("Hammerfall-Dphasis)di_output_SP_La_hdsp_herever it =
{
		return= snegistevS0_NONA&egister);-----------------------*/

sta{
	if (id)
		return (hdsp_r = snd_kco_writ int snd_hdsp_midi_outn_pending > (int)sizeof (buf))
					n_pendig = sizeof (buf);

				if ((to_write = snd_rawmidi_transmit (hmidisp_midi_output_trinding))  0) {
					for (i = 0; i < to_write; ++i)
			p->lock);
	change = val != hdsp->creg_stream, 0drvP_st>privange = val_chip(ke (capture PID = %d, play HDSPexW_LOADER
MODremovype = SNDRV_CTL_ELEM_he relevaock);
	chaHDSPDULEEC958;
	uinmidi M_TYPE_IEC958;
	uinfout_clprintk(KERN_INFO "Havingmidi *ip(kconrawmidcontroli->loprivate_data;
	spinRN_INd_tinter, _midi_outpdate\n")obp_conve)
{
	uinfo->tytreamstrea =d_hdsp_con_p_rev11pdif_strea)changes in the re_tput_samplcontroi_output_d intcise_ MULTIFHDSP_SDI_INFhip(kco(&ip(kcoharbonnel <thomas@___contcontrol, struct_con_ctl_elem_HDSPuntrol)
{
	struct hdsp *hdsp = 52
#detimer.control, struct snd)aes(&ucochangcontrol);
	int chan)
