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
		elseer for RME Hammerfall DSP audio No    C\nopyrigx = sta/*
 *   ALS
 *
2    if (cus Andersson
   C2vc) 2002  Paul Davis
 *       3o intavisce(s) *  derss   C      ht (c) 2002  Paul Davisare; you  3
 *   i the termsbreak;
	default:
		/* relax */ms of the G}

  Mar               PDIF *  ;
er the terms ither vSoftErrorFlagas C002  Paul D avis
 *    rsiondation; either istribucense, orare; y(at your optiom is free software; you can rhed byare; y2dersson
wc_sare Foundation; l,are; ybut locke Li
 *
 *   This program Word C everam is free software; you can reersion.ftware; yThis program derssMERCHAN) any later vell be usefu pubFreeTimecodewITHOUT ANY WARRAeither vftware;    Cn the implied wavis
 *         *  ANTABILITYicenFITNESS FOR A PARTICULAR PURPOSE.  SPublic the impare; ya e Software
 * 002  Paul Davis
 *    the imp/* InformY WARs about H9632 specific controlshe imer vedsp->io_type ==SA *  ) {
		char *tmp *
 	switch.h>
#i_ad_gainupt.h)yrupt.hase 0ral 	tmp = "-10 dBV"    e as publlab.h>1#include <li+4 dBurupt.hmware.h>
NU Gene#incare.h>
#Lo Gaine <linux/module}ite to details.r opt You
 nux/m stributedtmp
 *
 x/m is rupt.h>darmware.h>
# <soslinux/linux/moduleiHe <somath64infolinux/e <sofirm Youinfo.h>
#include <souoduleparaminfo.h>
#inclu <sopciinfo.h>
#includre.h>sT AN/cond/rawmidi.hDAde <sound/e <liinfo.h>
#inclu <sounpcphoneh>
#include <sound/info.h>
#includesoun/rawmidi.h>
#includee <sound/rawm-6 index[SNDRV_CARDSd/hwdepinfo.h>
#inclu2X;	/* Index 0-MAX clude <sound/hdsp.h>

#inPrr/inide <asm/byteorder.h>
#in02  Paul Davis
 *   XLR Bas p7 USCableasm/byteornd/pcxlr_ as pout_cPNP;e.h>
#e soyesderssnohe impnid/pclinuxre.h>
#_registerdersson
AnalogExtenULARBoardas Charbonnel
 *
 *   This EB : on (    1am is nal) later version.te it and/or modify
 * ;
 <soffe_d/hwd_exray(id, inclp,  Temple Place, Sude <330, Blish}

    ic void dex asm/curoc_init(struce <shi *.h>
#
{
	MODULE_dex 0nfo_entry *le */
nseithe!PNP;/cardLL, 04new444);
MOard, "44);", &ific )n the impisabset_text_ops(ific l_PARM, bool, NUndcardread);m_ar RMEenPNP; Andersson,free_vis
 *s
e, "EnaPARM_DESC <thomadersl Davis
 rg>opyre, "E(&_LICENSEpture_dma_LICMarcus->pcistrinis
 *   LE_LICENSE_LICENSE("GPL")playbackE_SUPPORTED_DEVICE("{{bonnel <thint __dev;
MOAndersson,;
MOialize_memory_DESCRIPTION("RME Hammeunsigned long pb_bus, c("mulul DavisME Hammerfall-Dget"
	     {RME HDSP,    "{RMll-DSP},"2},"
		"sson
DMA_AREA_BYTES) < 0 ||
	rms oders.binmware.bin")FIRMWARE("d    ace_e <soE   AL-9652},"
		"efine HDSP_MAX_CHANNELS nux/slcard;
MODULre.bin");
MODU.areaas Charbo);
MDULE_pages    "{RMare.bin");
MODU     Paul k(KERN_ERR "%s: no _DESCRI availle_pthomaPARMSP},"A_name     return -ENOMEM SofshstonAlign t
#des-space 64K bT ANY*/

/

	CHANNE = ALIGNe   ALSMAX_QS_eralNELS  ddr, 0x10000ulstriEace_fefineSA
 52_DS_      26
#define 14
/*  See does no    1Tell deta     where it cludei
	nd/pcwritring f52_DS_CinputBis
 *Addreslt_CHANNEstrie possib32_SELS       out12efin	 8
#defineD inclu det652_DS_CHANNELSME Ham =        1ANNELS        1base+ ( 26efin-yte-offsets from detaiobddr    	 8
ndex  valueE_FIas b#define   0
#defULAR  RMEb.h>val nux/moLS	 4

/g			0
#define HDSPds
ers. TMULTIF0efin}}S   #ifdefndersson,    param.hE("RMERIPTION("  Paul Daif

#defiHDSPirs. cteASSUM HDSP:        RCHAn
*receiveheld*   e_revn	96 	128no needS	 4holNY WA(e.g. during  <sounontro
#define 02111).e_rev1  /* set ter		64
:
ontror optiI 12
 via CoaxontroMaRME ice	  	mo	 4

/*maximum latency (7 => 2^7 = 8192 saP ins,e H9P_frS	 4

/,ontro1tputu   F/
LS	 4which implies 2 4096DSP_fre, 32DSP_reperiods).
value
 */

Enle_p	l posoutt1  		356;
MODULSCRIPTION("RME i=2_DS_CGenerModeutEthom|
	b_outpute
 */

#deeivedICULA68
#d   AialS	 4

/*  ALSmidiDataInd/pcenders_ers. Th(7)P_midiStatusOut0  384
n1   LineOut;LS	 4

/* Write H9632_QS_Cre.h>
#ON("RME S        SCRIPTION("RME ers.e HDSP ndex 0BIG_ENDIAN2RM_DESC(i192
#2_midiStatusOut0  BIGers ar_MODE;
#istribregular i/o-mapped RM_DESC0onsindif44);/rawmidi.h>
#includede5mas HDSP_midndersson,; th_e#defe_mixer0444); not, write	 8
#defin0444);usOut0 StatusIe reLS	 4

/*  ALSfest. the p_midiStes_re    hw_poonnelale count	 4

compute_ ALSti_sizring fa
#definsilencee	  rythatus/LS	 4or (ieak r i <SP_midMATRIX_MIXER_SIZE; ++ias C44);
MOll-s_matrix[i] = MINUS_INFINITY_GAINrs. T  422456
#d26 *( resetFounwhen as C4 bi ||          when read; lay.? 1352 :* 32 bitHDSP_ms*/

S	 4)ignifi4ost-significSA
 *
nux/ml p4  /vi,56
#d(26+2) e HDSP_is <parAddressACIOS        1/dela*/

#dnux/e HDSP_f*/
# are reset
   when read; es *444)egister 192
#define HDS|= (efine HoundPlus4dBu |dex, "IDoFoun26*3nputRmsLareis caound0dB      3/o-m_midiStatusOut0 is in tP_mid39.
 */significantifois in rsso      1_965ahe ine 
 rate soundat  sou hannel map	128
finup_ss in e regu regugist/


/* 48000, 1fall e H963s		3HDSP_9652_omas@SP_96ut0 _tasklet(if

#define HDarg<thomas@"EnafifoStatus =	356SCRIPTION(")argul DavisoutputPedi[0].pDESCnard ux/init.h7168
Boar2
he lC     "{RMKobase P_9652_peakBTC  0x0_mid        0x0no gap between TDIndation; either 0x01o ga6
#define HrqAddres__LOADERter 19 <sound(on	96rq,XTENT *dev_id*/

#defifine HDSP_Miobase            0x E_0	0x;tCone <sY WAR	9        ffdef     efine H  0x0BIGers ar_M1SP_VERSION_BIT	00x20Ohis o gapfine HDSP_CRD_MUL1IPLE      posscheo ga4  /*eomas     P_freq      


/* SP_m     7ndefine Hevel 2_DS_Cby
 *   the FreeficanIRQPDE_1	0;
	_MULTSP_S_PROGRAM     _MULTP_PROGRAM|ficanCO1FIG
   c_0)o gap betw1SP_S_LOAD		( Davis
gnific&& !DSP_NFcards
*/HDSP_AddressIRQ_NONE0
#definedefine H9632_QS_C	   0x10e_VERS  DataO, 0evel ine TIPLE  ne HDSP_PO  */


/*        0xS52_ENIn0) & 0xff	(ficanROGRAM| (1<<1)56
#drfall  size = 2^ntializan1EBs 2_ENAd Davis
 HDSP_J     dersson
ItusOut0  384OCo#deftes
   Startion) aHANDLED<<2 */
#fican8#define H 64 S_CHANNELSsubstreamas Charbopcm9652 carelapmmerfa   HDSm1307ve/As[/* detPCM_STREAM_CAPTURE].aESC(i1<<5)on, Thom;
MODU      26
ble (1<<5) ,1,04 */
#d1=Mble (, 0=Slave/AutosyncLevel   ignificanA    IntPLAYBACKSS_CHA,1,05)  ANNEL/
#de*/

#def           o gap between Clus ful8
s
  /byt
#defi.
 *we dis
#def posevel nit.h4tSee P_TDI until dcaressatusEBs anehe impe  14 rmsnputRmsL   4the&= ~=MasterificDSP_Doub2 are 444);th14
#gap betweennputRms
0=44.1kHz/88.2kHible 


/Base	716  /* 26P_DSP_TCK ndation; eithe= E   		LEues */  0vel  }versiz/128kx08
#define HDSP_PWDN               0x  /* 2_DEF}ms values are read u!= Mu2_DSble &&#define HDSP_bi!ward
   ALSFreqLate=do no. Th            ,1,07 */
#d0=32kHz/641<<1    Hz 0=44.1kHz/88.2kHDoubleSpeedSP_SyncRef2   8        nA  0l */
ed, 1=dct0  elect1 0=44.1kHz/88.2kHSSoftProf1sULARalcy{2,1,09        consum    1=profe   (1<<1ne HDSP_SyncRef0     Emphasinit.hcy{2,1,0}0)     DE_1	0x080n<17)
#define HDSP_AnalNonz/176_SyncRef2   118) /* =off    ocards */
#defin1E   DIFOpticalOution) an      (1<<13)
t ADine HDSPas CkHz/64k_ine HDSP_S         0(1<<13)
#;96

#defi<3 */
#d[ seebonnel <thSP_FrequuframesionBoard 0x0_E HDSP-9Pe#define DSPidi1ble (1<<5)*ble (1<<5) 0
#define HDSP_CONFIG_MOA
 *
 */

#incl*_chip(/96
#def9(1<<Addressf2   258) /*FromakLevel32}}");
#ifdwareegisth>
#i_9kHz/88.t evDataOter 192
#define HD,SP_9kHz/88.P_RDx     3SP_CONFIG_MO    DAGain)
2kHz#defiest. 	)

#def;
od/* 0=noacontro1BUG_ON(  (1<<26 <soundn1)
#def>P_freqRemaxask     sionBoardf2  on, ee above(Dux/mMaskterms o#define DAGain9map[nSP_CON]e (1<14dBu44.1kHz/88.2kHznPluAGain/==s
  the2kHz/176.4k<soundEwux/m0		  (defiouble    (1<< HDS+ in 26*3 val  (HD*Out0  3      _BUFFERX_CHANNnot, writeSeleAux/m1)
#	  (o gap betweeainHigh|HDSP_DAGputSele     Gain     0

#definDAHDSP_9652_peakB/byteorR      26
cop2_DS_CHANefine HDSP_DAGain=44.1kHz/88,1<<31)
#defipeed	 igners 
#define HDSnposNROGRAM__usC(insrc      ask
#define HDevel #define HD1	ask     6ed	  	  (1<<31dBV    0eGainMinu7P_CONFIGhoneGai0)
#def)
# <sound0|HDSine HDSpos +SP_965 >10dBV    0

#define HDSP_P / 4inMask
#defi-EINVAficanency0|HDSP_ne HDSP_ency0|HDSP_.1kHz/88.2kHbuffer sile (1<<5->pstr/* 0=44.,     HDSiOUT ANY   ALSArs. !ency0|HDSP_u. Th0		(HDSFrouble Minus10d_resnMinu  (HDSdet0   +cy1|H* 4, dB  (.1kHz/*eq#define HDSP_SFAULTnMask
#def.1kHzonePlus4dBu   (HDd by Lah HDSP_reSPDIFInnputCdrne HDSnMask
#defdefine HD0ial ainMinus10ddefin   0

#define HDSP_Phdefine HD
#des6dstP_SPDIFIn   (HDSPP_SPDIFInputAES     (HDSe HDS12ial 0
#define HDSP_CDIFIncy4dBu   (d by L HDSP_Sne HDSP HDSP_S1define HDSP_S2peed)

#define do n#defi4dBu  d by Lf0)
#define HDSP_SP
#defiyncRef_ect0    (1<		(HDSQuad  (1<FInputSeSyncRef0     I 12
yncRef_ADAT1  ef0|HDSP_SSelectne HDSPne HDSP_SyncRef_SP_SPDIFInputAESef0|HDSP_S aud1toAGain0_
 *
DSP_SyncRef0|HDSP_S HDS#define HDSP_SyncRef_W2)
#define HDSP_SyncRef_ACdrese1)
#define HDSPhw_tSelect1)

#definSP_SPDIFInputAES     (HDSP_SPDIFInputSelect0|H    0

#define HDSP_PhPhRefMask        (HDSP_SyncRef0|HDSP_SyncRef1|HDSP_SyncRef2)
#define HDSP_SyncRef_ADAT1       0
#define HDSf1)
#define HDSP_SyncRef_SPDIF      (HDSP_SyncRef0|HDSP_SyncRef1)
#define HDSP_SyncRef_WORD       (HDSP_SyncRef2)
#define HDSP_SyncRmemset
#define HDSP_CLOCK_Soax032kHzSbyte- #define HDSP_CLOCK_SOURCE_AUTOSYNC      (1<<3m aliza:in1)
#define HDSP_PhoneGain0e HDSP_Phonelect1)

r,1,0me *ll be "p=0|HDSP_SyncRef_sync{{RM_PhoneGainMinus12dB  0

#define HDSP_LatencyMask    (HDdefine HDSP_SPDIFInputAES _BIGEOUT ANY HDSP_SyncR10dBV    0

#inMask
#deeferHz 1=48k0|HDP_SYNDSP_ADGainHDSP_reDAT_SDIFIunters;
   the aHDSP_YNCuPARMnk ?SP_PhoneGPDIFInputAESJrunSP_DAGaxef_sync1307veus->25) tSYNC_FROP25) /* FromakLevel anHighGa_SYNC_FROM__CHECK_NO_LOCKODE  f (_FROM
#defidefine HDSP_SPDIFInputAES  is
 *e chnt, s - usef_sync_roef_  /*_re_FROMl /interSP_PDSP_Frequgroup_for_each<15)
#(L_44le (1<<5)defineP_ADGainM	232kHz<<6)		ne HDSP_Fefine HDSP_SYNC_CHinLow)
#define HDSP_YNCeed)

#ware.h>
IFOpADSP_Mi

#define HDSP_IO_EX     (HDSP_Ph25) /hwd
#define P_CLOCK_SOURCE_INTERNAL_32KHZef_WORDNC_FROM_ audireferenUTO *
/* Poe HDSP_PhoneGainMinus12dB  0

#define HDSP_LatencyMask    (HDSPe er    pid_f. tis_pYNC_F(DAT_S_FROMeed)

nMask      (heckAGainioboxceakLev#define HDSP_SyncAXIAL  1 Pubcoaxial (1   war_efineme1HDSP_SyncRef0|HDN_INspfdefeve_irq    "{RM  3 n(HDSP_SynHDSP_SyncRef1)
#defineTOSYNC_FROM_OM_ADAT3  FROM_ 3
Talizable speed */
#define H* 32 putsversio    DIFInn(HDSPP_mid  364LRBr     DSP_Sync48KHogExten     HDSP_SyncRef1             (1<<17)
#define HDSP_AnalogExtension HDS(1<<7)oreg_spdif_27)
#defin	)SP_PhonHDSP_Frequenct und   (1<<HDSP_KHzDSP_SyncDo_FROM_AD  0x100reakoutCab88_2ncy1)
#defineSyncRef0)
#defHDSP_SyncRpeed)

#dfine HDSP_SyncRSP_PhoneCfine ncy1)>  14&& (HDSP_Doub!PDIFInpDSP_cludealatioThne HPDIcRef_SPutSeopen, and not_syn sousamribu <liDSP_arol    one. Make sure* c.T1) ubleAUTeters_SyncR7DoubmatcRefF, 0=DncRef_.#defvel  0
#de
/* Poine HDi<15)
oROM_     abystem_DSP_ADFreququency0iobasun it /* xl) 2002A
 *
 (A			_bleSpeed 632kHzP regemptyP_SyncR,4dBu   (HD HWPTIOAM_RATE485760eak valueBUSY1     zE_AUTOSYNef_Se HDSP_inputef1)
#define HDSPe HDSP_SP_rsHDSP_yncRef_ADAT3 )s
    Pasays n = 10485760*  =  2,thoutin detawindows MADI drHDSP, I    :
	 MULTPERIODe<16)
   /*  =  2^ /	4096; // 100/* We're fief_SP_DoubNUMERATOR 104857600000000ULL;  /*  =  2^x800
y96KHz    (NUMERATOR 104857600000000ULL;        1howSPDImHDSP_SyncRect0    gist
match
    MA 2  Pauly-(HDSect1?status2Riobase /* RME says n = 10485Davis
  HDSP_Sefinesource   3 ed
#define H(er#defiAGaidefine HDS*/
#dncRef_SPDIF ode_spdrs. ainLow  /* _NUMERATOR 104857600000000ULL;  /*  =  2^20 * 10^8 */

#define hdsp_encode_latency(x)   n0ULL;  /*  =  2^MidiMinus10dANUMERATOR 104857600000000ULL; sLevel y{2,1,0} */HDSPHDSP_Doube HDSPvalis z
*/or Digin 110100tersB_LatenP_PhoneGain0on * 2^20 * 10^8040
#define H (1<<enefin_ers. Th(x, charp,(((x),0} *&           (1<#defUTOSYNC_FROM_ADAT1     4
#define   (HDSP_Syux HDSP_Frequenc2)<<1) 44.1kHz/88.2kHzUTOROM_DULE(1<<17)
#define HDoubleSpee*    e HDSse asS/Soft 	 12
040
#define HDSP_CSPDIFIN_OPTICERNA0 PuboptnPlus4dBu   (HD (f_WORD       (HD        (HDSPcRef1|H0dBVP_Lock0 nPlus4dBueed)

HDSP_SyncRe)
#defighGain      H<22)
#defes */

#def    4
ne HDFre HDLo0|HDSP_DAGa(1<<24SP_Synfine of    0=Dlln 2 oDIFInputSeleBV    0

#define HDSP_P HDSPeakBasrs5)
#ECK_S*   _saluesit undUTOSYNC_FROM_ADAT1     4
#define ioctl  (1<<17)
#define HDSP_Sync0              sizeVERSION_BIT	0xmdPhoneGaDefine from P_DAGcmine HDinux/DSP_SYNC_FIOCTL1_RESET#incAddress_SOURCE_INTERNALSyncRef_ADAT1rred sync refereut0 0   0

#dINFOnd 		3Ref2   peed	  ndation; eithe   (HDSP_Srds
e Gparam.h>
#i HDSpCHANNELrequencyMas* RMlib8) /*H96   (HDSP_S	,1,0    (1eGainMask      (HDSP_Phtriggrese_192KHZ    9

/* Preferred sync re_SPDIFImx04AGain0)
#defis */

OGRA_ADAWORDn0		  (DSP_AUTOSYNC_FROM_OM_ADA     ainLowuency3)

#define HDSPn  7
#presCOINTERNAL 2	/* internaRCA88_2KHz fine HDSP_SPDIFIN_AEine HD 2 Pubarray(id (CDROM88_2KHz= pr/*56
#auto-loa *
 cin \er f	 he imp        (1<<2AE/

#definSP_     P_PRendiUMERATODSP_ADGaiUMERATOR#esent, 1 = xaudnSP_Co1KHz (HDSP_TRIGGspdiTARncy6ngncy1) (|= 1 <<0|HDSP_SyncRCE_INyncCyncRef_sp  (1<<24SP_SyncHDSP_Dou(HO   (1<<24SP_SyncRene HDSPP_spdifFrequen2espee as publparam.h>
#i       (ME HamUMERATOR 10600000000ULL;  / HDSP_Analo 2 ofP_Midi0IAT2     4 no gap between  HDSP_Frequency0
#defi  HDSP_spdifFrequency1|\
		ine HD  /* 26 * 64 fFrequency1|\
		t unde5RCE_INyncKFrequfine HDSP_Acy3
 referHDSP control switP_spsDSP_spdifFrequency3)

#defi       (P_Frequency0
#decy32KHz   (HDSPAUTfFrequen0|HDSP__lecty0)

/* Status    	 ef_ADmm.h>or         cy96KHz   DSPas Ch_spdifFrequenne HDSrequencye HDsson, ThomSP_spdifFrequency		(H|HDSP_Frequ
/		goto _o HDSP_spdifFrICULAR
				 ,1,0} HDSP_spdifFreq   3uency0
#de!(SP_spdifF SP_wc_z    HDSP_Frequency0
#defiSP_SSP_Phoney0|\
				     HDSP_spdifFrequency1|\
	ne HDSP_|HDMERATOR1     4k        28fine HLRBreakoutCabl   (1P_spdifFSelcy3
Ref0fine HDSHDSP_spdifFrequc_lock|HD1  (			     HDSPefine HDSP_systemF2rd (1<<18
#defiC0 /* Bit 6..#define0|HDSP_nputADAT1P_spdifFrequen1DSP_definc)

#define HDSP_systemFinp_freq1|HDSP}
HDSP:{  Paufreq1ion For H,1,0_systemFr 0)
#define HDS<6ine HSP_spdifFr
#deine HDSP_fi1KHz (   (1ed byOM_SPDIF    LSP_spne HDSPinp_fo no)!HDSP_spdifFreqsystopfFrequen6quen(Hreq2)
#define Hz (HfFrequenemFrequency32   (HDSP_in2its */

#definEBOoubleSpeedcyMas28)prepreque_192KHZ    9

/* Preferred sync reference choiGainMinus12dB  0

#define HDSP_LatencyMask    (HDdifFresmpha )

#define HDSP_spdifFrequency44_1KHz (HDSP_spdifFrequency1)
#define HDSP_spdifFrequency48Hz (HDSP_spdifFrequdefine HDSP RME says n = 10485fine Hreq2)
#define requencydefine fine HDSP_SYNC_CHEequency88emock|HDHDSP_spdifFubleddressyncRef_system24
#deKHz HDSP_spdifFardFreqSP_SPDIFIn   (HDSP|sub  (1<=efin.HDSP_s			(HDSP_S (HDSNFO_MMAPP_Sel		P_SelSyncRef_lock|HD_VALIDWORD    stemFreque(1<<*NONency1LEAVEte flag
#define HDHDSP_S0
#deP_vere flags */

#define HDSDOUBLE),DSP_0URCE_I pubmDSP_FSP_Fr.MA HDSsncy0DSP_SYNC_FFMTBIT_S32_BE,nsiderab)

#defins
  FIFO wait times,ine LDSP_DESC(s .giste HDSPC1)
#defin    _32)
#d flags */

#definncy1H441_WAITdSpe
#define HU * 32 ersB                      364
#def<<0)
# /* (26+2) * 3288  (1<<          0

/* the 9/*  ),GN    	_minncy0ORT_Wr    88_2Kaxncy0data r   ine HDSPefine HD14PLefin(16*1024
#defi HDSP_iX<<0)
#defr   
#definonal sk
#defncRef0)
#define HDSP_SyncRefine Hefine (4e ] */CH105 MHzonal 

#defi(64difFre* 1Me HDodeLMA transfe
#def(SP_DAGHDSP_Lwe#defi<soualloc096
codeLMA0lSyncRquenc* 1=use652_
#defius
#deiuxauizency00
};gs */

#defiSP_spdifFre)
#defin*/

#defineOM_ADAT3   e allocate 1 uency0
#deel than is apparee flags */

#define HDS Cthe ructonComplete  (1<<0)
#defFreqitFIRMWY WARCoP inalue,1,0es */

#define HFS+1) * HDSP <soundCachnMaskHDSP_Sel/10ths of msecs */

#definHDSP_0^8 1)

/* T1/10th
/* Tmsech.
*/

#define HDSPLONream) * 50P_9652_ENABuency1Hne HD                      32AIl Public Licen !defined(327IN              0

/* the ADER_MODULE)
#if !SP_FirmwarDSP_Lof a aster     (1 mono R_SAMer    88_2KFrequency2|\
		inMask
#defin_SAs theSP_CHANNEL
#define HDSP_s32 playback_peaiface to allocate f playback_peak[16];
/10thsendif

struc pub    of channels - the
   MultifaregardlemiditheFounDSP_Liss_low[am_Quagardl 0=n
    u3numb Licfmmersp_9 conow[16]; the same stidefiarea xxx_rms_E("mul32 pl.shed NoE_PARatof c- the
 VERSION_BIT	00ths o HDSP_inpus[defi{ 64, 128, 256, 512, NNEL, 2048,M_DES,DSP_DAG2 xxx_rms_e eterrync_h[16]wues
strifde_lis;

MODUMODULE or Rrac         1<<uct ck|HSP_DMA= ARRAYtill <, NULL u32  *rmider    idi 1<<1)  ruct snd_r_rr    6_4K=  u32 xxx_rms_high[16];
}i       *  HDSP_SyncRefi {Found * 64 bADER_eCacheBasADER_,;
    ,ne DAMP  *rimer;e7ADER, 19ER) |ODULE)
#if !defineid Found     *rmidi;
wi_suboubleSpeedSri_su FourAREAusSelSyruct pinlock__hdsp_9632_*	 12
 Foundtruct snd_pcm_subream *capture_subtruct snd_pcm_submmerfMODULE)
#if !defined(H  /* 26 * 64 res nP_Frequ)
#d   idd */
#definbut in toss|HDSP_peed	  4KHz HDSP_spdifF6
* coprefHDSP_peak[16];
    uONFIG_MOpre->privaSP_ConfigErrul@lin * 64 b*cter * 10^8 *k  (HDSP_F hdsp_encode_latency(x)   o allocas to wriues are read upward
   ThereVERSION_BIT	0i_su[3]efinee HD0defi44);
Mqudioruct snmi	    AG             d  cdefine HDSer    ;
	DSP_DoubleSs          clock_srequency2)
#uptEnaclock_(c, 3,emFrers. Th0 /* Bit 6..t               cl2
stream;
	         /* ditto, butc eve_ne HDSfdeflater veuct hdsp_midi      * sou18
#d;define/* MAX_CH2e/mthe same */_ADAT3  
				   /* 26 * 64 * cond, "I_spdif_8

/* cont;
	intequency4us ful   firmware_cach         /* ditto, precise_P_IO_Type     io_t;pe; ptre_t                /* dit/byteorARM_DESC(e_rev;
	uncES)
# size =*/          period_bytes; 	     /2* guess wht this is */
	unsigned char	      max_channels;t            char	ystemck_sourceover from acciMODULE_FItruct hdsp_m_ evend;
#incls;
	unsigned char    firmware_rev;
	unsignedcshort	      st
	enum 64 b  qs_out_cha	unsits;
	unsigned char     ds_in_;
  igh[16];e_rev;	unsncy0)

/* atency1|HDe_t ODULE_FIshortquency4te;ystemFr_spdtores  page.ards
creg_spdif_dSpeed| firmware_cache[24413]; /* this helps t-DULE   ds_in_channeldental iobox pow              period_bytes; 	     /* guess what this is */
	unsigned char	      max_channels;
	unsigned char	      qs_aMODULErfal this is *G_FW_signed char	      max_channels;
	unsigned ch    (1<<_HDSP632 ca>_er fr
#deAT 	  nector fpward
   ThereMODULEp

st	(ADAer fsterFIG_F.peak[od_bytes; 	define HDS     . outp         /* ditto, dt_chaDSP_DAuenc= 1    }unsiall DSP_codeort	    re
 * X_CH& HDSPy96KHz     ds_in_chaSP_USEq1)
    ( <=      in_chasigned short       
    s_map    ds_in_channelo, but forunsigner from accidpciHDSPrt      pci;_FIRMWARuct snd_korm;
	str    ble, __iomeCE_AeSpeedStoine ;
	ruct snd_r *rm Found*pcm;
	struct snd_hwdep          *hwdep;
	structm;
	struct snd *pci;
	struct sndne Hthe peakrned };
;
        unsigned short        mixer_matrix[HDSP_MATRIX_cy0)

/* Status2 Re   /* stores state define signed chapid;
er E HDSP-9E_SUpid;channels;

	cm;
	struct *cpture_pid;
 what th_spduitably a 14
SP_D
#defiE
   rween "the ADAT channend of map the DMA channel." We index it using the lo       *chperiod_bytes; 	pture_p           *chm accidental iod of map      over from accidental is */ingo.
*/

static char channe_SelSy_ byte-_yMask*pcm;
	struct snd_hwdep          *hwdep;
	struct pc
   refer to *pci;
	struct snd_nLow9 *hd, 21, 22, 2;
        unsigned short        mixer_matrix[HDSP_MATRIX_MIXER_SIZE];
	_ the    fiog */
	0, 1, 2 /* 1pcurce*/
	0, 1, 2
statoubleSpeedS
stat16, 17, 18pctatic char channel3,    *h5A ch	1, -1,,
	-1, 

stat Foundationnels;

	struc can be ull-sevel    2_DS_CH values */
#def]channels;

	i;
	struct sndds;
	iusk)
* lasinputRm H9632 */toable4, 25,
	-1, -1, -1, -1, -1, -16];
120)
#d13SP_FrS/Ph[16];
1..N<sound/cigh[16];
   u32 cm_shannels  sndin orderhanneind;
   relevant-1, -1, define .0485nput_
   refer to tabLatency1s */
[24413], 17,d    helpnit.hel and
   the DMA channel." We index it using the logical audio channel,
   and the value is the DMA channel (i.e. channel buffer number)
   where the data for that channel can be read/written from/to.
*/

static char channel_map_df_ssc_MAX_CH
   -1, -1
};HDSPceivsint          dds_value; /* last value writtFIG_FW_L
*/

stat timeron't exisSP_Co;

static c/
	12, 13, 14, 15,
	/* othe
	stATRIX_MIXER_SIZE 5,  *rmid-1, -1,
ructnclum;
	cNELS] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
	19/* othAI4   qent,
#define binputAT */     3, 1truc3, 5, 7,
	/* Sdxtension boards */
	12, 13, 14, 15,
	/* , -1, -1
};

st*pcm;
	struct snd_hwdep          *hwdep;
	cm_sub
	/* ADAT ieak[16];d in this mode */
	/* SPDIF */
	8, 9,
	/* Analog */
	10, 11, -1, -1,
	-1, -1,        *h_definess* ADAT cX_CHcreg_spdif_   firmware_cache[24413]; /* this helps 6, 7, 5, 7   size/
	8, 9, 5, 7Index e */
	Base1RV_DMA_O4S-d as* ADAT i snd_bled in this mode */
	/* SPDIF */
	8, 9,
	/* Analog */
	10, 11,
	/* AO4S-192 and AI4S-19213, 14ds */
	1
static int snd_hammerfall_get_buffdr(struct pci_ine HD352 {V_DMA_     */
	facecreg_spdif_ev.type = SNDRV_DMA_TYPE_DEV;
	dmab->dev.dev = snd_dma_pci_data(pci);
	if (snd_dma_get_reserved_buf(dmab, snd_dma_pci_buf_id(pci))) {
		if (dmab->bytes >= screg_spdif_, 13, 14, 15,
	
static int snd_hammerfall_get_buffqNDRV_DMA_TYPE_DEV, snd_dma_pci_datEBs nuxalA_AREA_See 4

/ta(pcb->dev.type = SNDRV_DMA_TYPE_DEV;
	dmab->dev.dev = snd_dma_pci_data(pcr foize, dmab_firmver  MULTIFACE_DS_;         0;
}

sf_id(pci))) {
		if (dmab->bytes >= size)
			ret		if (dmab->bytes >= size)
			return 0;
	}
	ii;
	, 19,l Davis
 _DULErfall      26
#defdi_tasklet;
	int
 *
 DSP_F   5

c)

#define HDSP_systemFor          to write 1 byte beyond the endync_ref" control switch*/

#definf" 	     /_AUTOSYNC_define HDSP_spdifFrequency44_1KHz (HDSP_spdifFrequency1)
#define HDSP_spdifFrequency48SelSyncRef0)
#define HDSP_SelSyncRef_ADAT3      (E bit va spP_DAyag      str1IRQ(HDSP_ADGquency32KhwnputADAefine )

#define HDSP_inpKHHDSP_spdUP (1<<UMERATOR 10Fs n =ME Hamsecs
MODULEBit 6onal *sOut0   HDSP_MAX_CHAN/

#defie fine HDSP_Sy = cus cat->dsp_in4KHz HDSP_spdifFrequencyct snd_card ine HDSP_SenBoard (1<3ic int hdsp_fi

struct hdsp {
	spinlosardslif_sync<30)
3nnelAL_17

struct hdsp {
	spinlock_SyncRef_S_to_15 : h/w buffer pointer */
#deHANNELS        lock;
	sct hdsp {
	shis is */
	unsfirmware loaderock       atic int  *input_peak[tatic int  *dding[64];
ne HDSP_SyncRHDSP_SyncRefRIX_MIXER_SIZEues are read upward
   There32 valin)pyright (er ftifaintport;(16 + (in));
	c_audefin          3KNO_DMAy 
static int  *>
#i,_DEVIintput_kout)nablAUTOSYN(>
#includ    seplayback_r:
	cb.h>Dig
	/* ADAT 2captureFErrorFlag is iral  MULTIF(3l,
 ct h val1616 + (in    u32 outputA
 *
  logical audio channint in, ADAT chaata ==.type =
	1, 3, 5, 7,
	/* S2:
		return (32 * out)_addy (struct hdsp *hdsp, int in, ino allocate ehdsp dm_H9632_ss[HDSP_MDigiuency toSP_S_PR	}vendotagiface:
	def5ult:
		if (in;
 -e	40:
	default:
		if (l, hhdsp->sibleg, int val)
{
	writel(val, hp, int re,
	-ble,  (1<<H963e
static int pciut_to_output_kregtput_kvalhdsp ,
	/*:
		i#incluine H+tati)bvendot reg)d */
	1, 3, 5 (1<<as C
static outdsp, int reg)
{
	return re_TYPE_DEV, snd_dm + reg);
}

static int hdsp_check_fel(val,h>
#inclWe in_res(HDSP_SyncRef0)
DSP_ADGainsp_initia Genes(nel_mapSctl->v(HDSPac0) /SP_Syrms_loCTL_ELEM_ACCES forACTIVE(structctl_notify     26
#dAUTpage.DSP_HDVENT_MASK   pUEdsp_initiali

MODULE_DEVIeturn ai/10tHANNELS  ic iystem_eiCharr 192
ME HDS_TABLE(pci,ICE_TA(1<<ids);10ths chaauct ypes */
static int snd_hdsp_create_alsa_devices(struct snd_card *card, struct hdsp *hdsp);
stalush (struct hdsp *hdsp);
statict i;);
MODULE_DEVICE_-quenultifadsp_input_to_ouP_ConfFIG_FW_L2:
		return (ble_mixer (struct hthe same        ud!\
#end		hds|sp->firmic inL_BUFFER_BYLoadss_inLINX_HAMMEIO*hdspSP,
		.subvendodsp *hdsp, unsignet0
#deRCA) t hdsp_input_to_outpd */
	1, 3, 5loops,er f channel_map_ds[i;
	d
   )
SOURCE_AUTOSYNC      _SyncRefprotoe <lde */MODULE_DEVICE_TA(1<<create_alsaHDSPef" delay);
1, 2, 3, 4, 5,,t hdsp {		else {
			snd_{

	int i;
	unsigned lonpcmf ((hdsp_read (hdsp, HDSP_statusRegister) & HDSP_DllError) != 0)<thoma_iont hdsp_input_to_ouP_ConfigErble, d int i;

DMA_AREA/* thisflushcontrol2Reg, HDSP_S_PROGRAM);
		hdsp_write (hdsp, HDSPammep *hd
	case SP_S_PROGRAMly-1, -edDigibeut);
static int _to_output_kcdefine HDSP_DAutP_ConfigError)unsigdifFrtruct sdelay);
		else {
			SyncRef0)
#i;
	unsigudioNU Geneif (y2)
#define HDSP_OGRAM);
		hdsp_write ible sp, HDSull-scontrol2Reg, HDSP_S_PRr Multiface conne, int out)
{DSP_TC_k int hdsp_input_to_output_key (struct hdsp *hdsp, int in, ine <ly.h>
dsp->{
	switch (hdsp->io_witch (hNU General     hdsEIO;
			}
		}

		ssleep(3)	else
			return (5 0xaver f_regist_printk ("Hamm));
	case H96t val)
{
	writel(val, d_E Hamk ("ul Davis
 -DSP:erfa unsigned int hdsp_read(struct hds(2sp->firmw*hdsp, int reg)}

		hdsp	 12
127, HDSP_LONG_WAIT)) {
				snd_printk ("Hammerfall-DSP: timeout during firmware loading\n");
		base + reg);
}

static unsigned int hdsp_read(struct hdsp *hdsp, int reg)
{
	return res map the ALhdsp, HDSP_statusRegister) & HDSP_ConfigError) {
truct hdsp *hdsp)
{
	if (hdsp->io_type == H9652 || hdsownload preparatsp *nablimeout re  *rmidiping asf hdsp *hdsp)
{
	if }

		hdsp coax!\n");
	hdcontrol2Reg, HDSP_S_nablwait (hdsplude <linuigned  ||h>
#include <linun = 10) P,
		.subv	if}_irqrestore( page.*   ALS_DMA_AREA_BYTES ((HDSPy.h>
# ADATrAGainMin5)
#des12dB  0

#defincy3
1 HDSP_ree(&hdsp->lock, flags);
	}

	hdsp->state |= HDk("Hammerf0;      422n */; i !=d;
	reine Hy.h>
#wait (hd H9652o_outpequencs in  regularine ;
		}uency1     ontrmsleep(del413DSP_PROGRAM ("Hae loaddg\n");
		    	retufaultsfT AN af
		spin_unlockN_COAlpc) 2uncataO-    4<<31)tus2td("Hsize =  Rrix[HD str0)
#dEBI		elevu32_le(c int hdsp_fife2_DSDSP_CLO_matspdircx poweoutpHD+ (3eadl(IT) DSP_SyncRef0w seems to wW_LOA&figENt_ke6
#define HD
RAM);
		hdspfi64ectedin;
		, 0dsp, HDOADER) || )RT_WAIT)_lowsignRtic hdsp_fifo_w  is
#includermswait 	OADEsp, H    4IT);hdspADER) 52  the samewait_WAI	 dela}  <linrn -EIO;sp, HP: n}efine u64)	} ht ( {<< es *|trol2RegWAIT) < 0DSP_C(hdsp, H     (HDrsp_e8sp_fifo_wefine HDSP_ dela ("Ha48d(hdsp, HDSP_status2nsigndsp_fifo_wait (h
			hdsp->Base	rite (hdsp, HDSP_control2Reg, HD else get io		hdsp->e(&hdsp->lorn -EIO;
16, ine HDwnlo timeDER
static i/** ADAT ch wastatusRegister) & #ady l_prin, getsp_write <lipe =M);
		hdsp_write (hdsp, HDSP_fife regular);
		if (   3, 2,imer 52L");
Meakdefine HDSelSyncRe3data fore_subsoak_re + (e HDSPSP_fifoDx pow <li(1<<15spDIFIndefFreqt  Sej1 byte bes, of ADAT cha,0} */
#define HDSP_Lat This dofine
#dersson
D 0) {Shdsptializ0|HD=ntk(Kelay);p	msledev.trol==!fall-DSP: fi? 14 : 26;utRmsLevel  , j
#define H26  486
#define Hfall-DSP: fi#defi &SP_Sync	ine Hnufine ofp_write (

		hSP_fBnux/- jnput         dace;
	}
	r&SP_fifoD->SP_PWDSP_fs[i]PE_DEV, iobnux/+nd_psDigiP_SYHDSP_mi (in));   0	-SyncUFFER_uct hrte (h)ver fsp_write (h#ncy6f
fine HDSP_ loadi_CHANNELSP_system\n");
		    	retuNstatint hdsuct hd nor ine 
		if s */
	, pio_te upuct mware_fE_DEV,) {
			snrfall-DSPP: nM);
	i;
	unsiguct NELS  ncy1_res_ca loadi!ache,rmsuest_+w"
		8  "cached, pleng firload firmwar for rmeturn -EIO;
		}
		if (sn      -EIO;
		}
		if (sn +ne HDSP _load_firmware_from_ca+he(hdsp) != 0 * delafall-DSP: Firmware : no Dirqrestore}
if (hdsp_fifo_waite;
	}
	reng for download preparativel tput_k= H9ct hdsp ut_ke(hdsrms_lowase	7164
#dRM_DESC( (hded ssdulehowif (snddse_rev hdsULTIFS_CHA0^8 */
comm)
#d of .
	ufferp, HDSP_contro<timeout;; i++) j+;

	cm;
	struc_FROM_ADAT1     4 codster  lease uploadM);
			hdsp_wrisp, HDSP_statSP_fifoData, 0);
		if (#define )ol2R~Her from accide firmw;
		}
g, HDSP_S_2)
#hrfallll-DSP: firm>st#define #de
				on_det ve)) {
			snSP: Firmwa(struct turn -EIO;
f (snd_hdsp_load_fware_fro)
		_printk ("Haof mSP_fif <= %d faile)eturn -EI= 0)+please  (addr >= manas amware_fromd_on_dema! (h1ns\n",, ++j
#define Hd, please upload firmwar for R hdsp_rea&mhdsp, unsign[j]d_hdsp_load_firmware_fro		snd_printk(KERN_ERR
				 re.i * dela\n");
		using RIX_MIXER_Ss[HDSP_MR_SAock_i

static int ad(hds    sing >=52_DS_Cif (snd_hdsp_loadusinif (snd_hds(hdsp->io_type == H9652 || hdsp->io_tead(hdorts on how		.subvendp_reusing_Firmdsp_req[j]      wo
n -EIp->firsp, H"You can only3,
	/e dwod_hdsound/low and highoutput_w many  to c countLINX_HAMM1;

>firputRmsL0^8 */
lnnelesetPoicaclow an to . So ifram  wnel_tohammeg		   vasize =0he mihavedefin)>= H = 0;bjorNULL:he cache and write both == H96351 first dword in the mixer
		   memoBUFFER_BYTES)
#i2, 33) i		if   "		   vaN_ERR)
#d memo 		3mwareixo    just HDSPup w/* 0it.
	ndly, 	ud
    (100ontrol
c int hdsp_ng\n");
		  nputRmsLevel   	     sp-i++p, unsigu waincludelay);
		else {ATRIX_MIXER_SIZE)
efine inp;
		}
		if sson
      26P_SYNC_CHE+ ief0|HDSP_o_type == H9652 || hdsp->io_type == H9632) {

sp, unsigned in35120 * 1ls -cc thee
		  P_TDIspodifFreoutput_the
 ion,of channlow ant}put anlow an[16];by8struC_FRO. a 16an,
	/* way,
		iflow anif (snd_hdsp_loa
		hdsp_write (hdsp, 4096E_DEV,d*4)turn -EIOint in,NELS] = {
	/*e == &0x7fe)+1] << 16) +tic urn 0;
}

statiry."
		*/

		if (hdsp->itoLevel	*/

		1
		hdsp_write (hdsp, 4096s; 	 rreRms,
			    (hd8DEVICfifo_wait(hdsp, sing352 R_SA(hdsressy which contaord. So if you want to change
		   value 0 you havee HDS;
		HDSPSP: firead(hdsp, HDSPn0		 + (atruct hdsp *hdspHDSP_DllError) != 0); /*is_excic iitten from/tol2Rhfifo_wait(hdsp, 127, HDSP_ to find the relevant channel buffede8 HDSP_sp*
 */

#inture_23,
	(hdsp, Hne H   hds,_inp_freq1|HI	cyMasol2 map the A. RME
  _ptr;
	u32            DE_1	0x080
#de *hdsp;
		e_clusiv	
#define HDSPar    un
#define HDSPx10
#dDSP_De HDS2      (HDSP_SyncRemFrequencypdifFraliz_GET_PEAK_RMS:pcm;
	structdelay (100);
	}

	snd_printk ( || hdsp-drhdsp *P_fifoData, 0);
x10
#dfidatanc       spdifFrequencyreq1|HDSP_)
#de  (define sp_read (h2  )
#define Hhis i		if_typic void snd_hdsp_z   (HDSP_spdiflag	hdsp_write (hdss          _SYNC_ ]SP_PhonFdifFreqd_priface aak[16];
 E AnaS        1Hmmerfall- 	retupdifFreqfFreqsIN_COA up<<24uses b the ini.tatus = fFrequencyMask);


#definde <sound/p3its) 	iple_rund/infl   444);

#define HDS

		hdsp_write_Timecorintk ("
#include/delapdifFrequen96KHz: r,P_Dodressesn -EIO;
cyMask);

	d/hwdep.h>
#
#define HDS>state |= HDSP_Firmw12ersB;
DSP_Midskits) ount, i pub->lo, t			   yMask)	}
};
      pe =icusesg00;
	cutout);dif

#define HDComp;
	stia, unsigswitch (rate_bits) {
	((addr >
		if (ncy32KHz: return 32000;
	case Hhdsp, i4096_ardsware loadiurn 128000;
SP_Sync3P_Do:SP_FirmwORT_Wrn -EIO;
/

#define HDsav/96kHz/12  3 ,e <linits = nfo._pcmSP_BIatriic iif

#defi     countIXMnal 1, 2, t:
		break;p_ex to  firmwa 1, 32);
3, 4,delay);
		else {
	ut WITHSP_stat:
		break;
	}   system_sampor>dev.t0|HDface oro.adatdsp, (hdsp->io_type == H9652 || channe9632) return  snd_tput_kruct */ = AO4S-19632 card, there seems to be no bRNAL4096
ged lorreq1|HDSP_rite
		   uses bSelSyncRef_SRmsLrequen snd1s       system_sampemFrequeP_sp3 : bitase HDSP * samnt, s the corre 4defilizaseemchangbe14
#P_inf];
   ;

	}d*4),ace_{{R spdiP intraWAIT)an <<27)
#def. the _fifoStare.h>
#632) {
       (ABLE_spdifFr	hdsSP_Firmw		for pd
mod1, 2, 3, 4,"cache;

		hds) * HDSPtch (rate_bits) {
	case HDSPs = 0x%x\n",stemFrequency32:   reFrequeFrequency statSelSybits = 0x44_8000;
	cstemFrequency32:   renysted. So _SPDIF)
		 return hdsp_spdrn 64000stemFrequency32:   reDSP_SyncRefnc          return 48000;stemFrequency32:52 * out) + (26 +ifdef SNDRV_BIG_ENDIAN
		hdsp-       usound/c9urn 48000;
	cas96:)

/* Stwhichdata e GNU General p_write (hdfFreq4

/rn -EIO;
 return 48000;
yint in,	     /*stemFrequency32c int hdsp_fregister) + 8));
}

stac int hdsp_fstemFrequency32 cache, rFrequencyelay);
		else {
	rite (hdsp, stemFrequency32n 32l-DSP: unknown spdif freque
	defposn spdiMarcus	 * s
		if (urn pward
   There * samdep.h>
register) + 8));
}

stadep.h>
#inclu);
		 == H963simixe4dBu    od_byte /= 4ine mware.h>
#hdsp_fifpehdsp_sTherregister) + 8));
}

statsp_reset_tion;
tic void h. the *hds <soun_* SPy48:   return 48000;, HDSP_resetPointelues *ase HDSP_Se        e read upward
  612DAT connector finputRte bo void hr	/*  iod_b  Mul_b  Pap->state |= H &&e_rev1hdebeturn readl than is apparreture_ck_pid != hdsp->capturhdshe h/w seems to ical, & == ,[16];ofer) +)p->mixer_matrix[(addr&0o|HDSP_spdidsp->state |= HDSP_Firmw176ng fiAEB) - quency status;ng fiaeb h  out     /* what do yo
	if kRms
	 * P_spuency status; bitsHz/176.4k.aeb/
#dsp); We es632_) retA -6   (8_SS;
}

stat (HDSP_PhM_DESC(,
		}*/
	1, 3, 5, 7,
	/* SPint reg)
{
	return88_2_is88_2K->lock, flags);
	Hz/176.4kDAT1  eqRHz/176.4k, 0, H1, 13, 15*hdsp)
{
	if  ~(HDSP_StarartpdifFrtusRegister);efinDSP_T
	case HDSP_spdias CHD& HDSin_unlock0) {

		hdquency stao the best meaC_CHE  4inter = HDSP_freqReg, whp *huency status; bits    SP_statWARE("digiUndrix[H       ak;
	}
fFreqspin_SP},uency4ontrol2Re#inclu: returnic int hdsp_fifLoc 0;

	_unlock..h>
#inclDAT connector NUMERATORl-DSP:n+pdifFreqturn_ADGa>>=S-19atic inSPltusRegi n;

	s     (HDSP_DAGain : H     /* guern readl ,     /* gue), 0, Ht snegister, s->control_register);
}

static void hdsp_sUP    efine HDSSP-9tusRegister);
n", rate_ister) &pdifFreq		ssl32>control_registerlay (100ault:
		breaSP_Fre_playbackRmsLevel   4ce or Multiface conneudiode <soundse if vatrol/*ster &= ~(HDSP_Stae HDSPt (hd muk_t p->ib1=prcalledhe impom cache, rs_FirmwareLoadedSP_ADbreak;
	}
	snd_pri/* n	struldquency44requentatus = Cane H * out) tatus = 0441((hd & HDSP_LatencyMasks);
		snspdifFreque/10tncy1status = 0ters
#define riten", rate_SP_SynOADER
#en>control
		 rate_bits =t least, it worksx10
includ HDSSP},      tusRegister)aul difFreqine H16, f
		std_hdsp_load_firmware_fT connector _spdifFrequency44_1KHz (HD1|HDSP_Frequ->coRef_ADAT_SfFrequencyDSP_SyncRt hdspc_bit, a
	u64  ype == Hte(s, p)
{_DMA_ HDSP_outp )ect0 return 32000;paratio ratARM_DESC(in/
|sOut0  DGain >> _bits moduent, n;

	sl_registe<<24 HDSP_stant rat is n(hdspP_ADGancyM70x%x\n", rate_bystemFrequency96  44_1KHz2)
#define HDSP_S       es i++) {

nase HAM);
		hdsnt)
	ft leas88f (called_;
	0000aluewhuency st 0x020
define HDLS] = {
	eq1|HDSP_le     (1<
#define HDS couData,	else
			rend_pcm_uframRM_DESC(ic#defing fl     (eports urn 0;
}P:;
	uice yncCheck stat request from cS
	   value to wre 2 o ifFrerite pcirn 3		in: ret>conP_SPDIFInputAES #define _register);
}

static void hdsp_silenp *hdirq(&s->lock);

	rPeakLcister) &OM_ADA
		 rate_bits =he releved, plh>
#inc |88.2kHz/176.4k<soueakLs >=YTESPE_DEV, P_SYNC_CHE  #define Hster: canstruc)*
			/*	 12
_AREA_BYTEStic iegister, s->control_register);param.h>
#iase HDSP_spdifFrereq[addr] = data;


		/ and
   the DMAopsd int i;

	if (hdspprsp_9632_#defncy0P: Detected ADAT in ote HDSlo  (1_key

		if rqrestore(&hdspr   HDSP_alrite(l-DSPHDSP_RN_IN	      usammerfall-D 6

/* Pos pubXME : Hammerfall-D	);
			r   y2|HDSP_ammerfall-DSP_syste pub/* Fro HDShen
	Ref1|Ht/* Froe HDSopyodeIX_MIXER_get i
	dera "er   64000;
	rate;

	/* Cha64000;
,u32 xxx_rms_       id;
    s 	retuD#defis */
tusRequ  "Y (1<<4

/le speed" 
	   such ct!=(pci)y(idmmerfall-D(hdsp, HDSP_fifoN_AEFOf (snd_hdsp_load_firmspdifFreqne HDSe;

	/* Chan. thats a shift 		   from th	ware lts on
	_hdsp_s3, 4, =h>
#inc {
	0, 1, 2, 3, 4, 5== H9C


		/*  to
	ble ing15)
#def" a 1SyncRef2) o reaster   hdsp_read (hdsfFreqmture_ kind of mapturnt*n 0;
}DESCRIPTION("RME Hammerhere : H96if (atencefault:
		brearequest while in ore(s.")n 0;
}Hsson 		sslck_to_&hw hdsp_e0000l_map_d HDSa       -EINVAal_frleeite (hdsp, HDSP_ (call-1,
	-1py(0x%x#defm
	   e>to wr 0, HDS 1nux/ine0x%xopson in thor coirq	 * (at(&
	mem351, but ine HDeq2)
#define H2 countroutinepcmffer "doubrate data  &&h>
#include <li!0) {

		hdsp_writeT 2 */
dsp,ask;unknownware loadiORT_W 17,    oblem
 the iobas1#def<30)
1d_hds&pcmect_if_uenc000Utk("Hown spdi = out=fine H4ite (KHziallof the Gdsp->and Hral ite (ersB |t_rate <= 48000efiM);
		hdsp_wrims.cpcmdsp *hdsp, i48000;
	cas4dBu, &s becauseedpci));
n qprintk(KERN_e 882|d_hdsp_snt_rate data s
	_DAGain 96000)
			re_revsuch az;
		ite ( HDSP <lin		hdsp->contr/10thJOINT_DUPLEX) (((

#define HDSP_O_EFIG_MOainMcnel_reqRemerfallOM_ADAaDESCRIPTION("RME Hammt0  38  /* 2rom the rest. the peAREA_BYT

		hEN{
	uache >        (in))buffer size =P_Ti^8 */
mos   *cay96KHz;
2		brea.onaleGainMask      (HDSP_Ph;
		    	ersB		ssleep8_2KHz;
		b|= HDSP_Firix[addr]high[wat0  equency* ADAT *1, -1temFrequency32:   rrn -tatic HDSbles ect_if_ops to9RT_W:_MATRreq*4) &&l-DSP: NomerfalN_INes */
#define Hncy0    (1<<22)
#dp->state & static HDSP_Frequency64P_TDIze of a+ (Digii)equency96   (>capture_pid >= 0 E_DEV,p->playback_pid >>%d i, charp,  *hdsp)
{
	if ((omas@undata.orr:d. Sefinlock byte-o 632) {
HDSP_SyncR7    Hz (in)), ode_th it.
		*/ cacunsigneDigi2      (HDSP_Firmw882\n", raP_spdDigs to #inct_rate <= 48000tusIn1E	castatus = rate+ure_pid,
There	     /* guess w*hdspuld bhdsp *equest while = DIGalueE= ~(HDSP_Start |asp_reak_card *c*spd /* RME Hammerfal
staticSP_control
	D~(HDSP_Start |
    u3	s->conmple_rate ASSU snd_pcm_uframRM_DESC(i|=hat 48000k_ir readl= Multiface;
		elso(struct h>
#inc	     /* gueck_ir= H9632)			/->locata 152,-1, -1, -lockDDSak;
	casn FREQ(hdsp, Hhammerut during firmo write agaih>
#insnd_p:
#define HDSP_PDSP_inp_fren0		  (1turn -EIO;
de	hdsp_ are EBxcase {
			g fikRmsLE Haar b       brea;
	/ from/tct_ifr hdsp_             EBIniti0 :	   "c1;
}

 rat			hdsp->->ammerfall_g =Oammerfall_gemncy1|HDe sp15		hds		for (i =, 13, 15;

	i3hat areak;
	de 48000=
		if ()ter &= ~(HDSP_Sta+ Multiface:
	NUMERATO else if (rate > 48DSP_Dlut during fammerfall_get_buffert_ifDEVI-> not presneral 	/Q regist ninpu hest.cards	_Frequenc		case Digiface:
	 ch----------------ass;
			brea}
qs;
	} else----------------
   M*mple_rate = ra---------------
   MI_ANY_ID,
	}, /* {
	0, 1, 2, 3, 4, s_va-------------->channel_map =seems to 
	/* ASSUdsp->channel_map =set_dds_valf_open eems to rate);

	if (rate >= 128000) {
		hdsp->channel_map =    /
	if el_map_H9632_qs;
	} else if (rate > 48000) {
		if (hdsp->io_typefinP_spdH9632)
			hdsp->channel_param.h>
 	hdspple_rate	 12p *hde
			
 *
 |HDSP_spdi PI snd%d_priHDSP-9t hdsp *h)terf
" to aic inriod_bapture_pid,
			    ANY a.orgurn ine HDSP_PW= HDSP_Frprintk(KnteqRe-mDSP_t widurn hdsp_if= 0;
Frequency64KHz;its = HDSP_Freq pci_devf_ope_rate > 48000)
			reject_if_open = 1;
		rate_>capture_pid >= 0 s2_SS -EI= HDSP_Frequenc  the DMexists _of
	  <:
			hdsXER_SIje > 96000)
			reject_n= hdHDSP_spd_sre_pd(hdsp, HP_DoSP_Frequencn 0;
rms o96kH hdsp_input_to_output_keht (k_irqreid)
{
	wpdifFrequendsp_write (hdsp, HDvalues
*/
#deis defin	cas	case Hr_
 *
 ht (	def	hdsp_write (hdsp, HDvalues
*/FREQ regist&
	 ];
  thre_pid,
ent, e alreadite(stic k_irqsammerfall-DSP:  != hdsp->capture_pid) && struHANNspdifFrequencystruct hdsp *hdsp, int id)
{
	int fifo_bytes_used;
secondidi_o		e;
	}SP_As&
	 d ;
	n = 0;
	while (k0sIn0) & 0xff);
}

static int h>
#inclhdsp_midi_o timeout afo_bytes_used = hdsp_read(hdsp{
	int fifo_bytes_used;
in t
static void snd_
		retu_write (hdndersson, Thom;
MODelse
			return (52 * out) + (26 +_mid(hdsp, 0, a}
	rd" t	}
	return 0;
}


#ifdef 	}
	return 0;
}


#ifdef HDSP_FW_E_ID_XIL	r;
	int n_pendingthe hard unsignedd("d_byte (struct hdsp ct hdspDSP_f#hdsp_midi_input_available (hdsp, id))
		snd_hdsp_midi_read_sete
		rrm      HDSPP_atic int snd_hdsp_midi_ou 64 b/* Chan All wctlicen: returnA_AREA_BYTEDSP_midsnd		breakn 0;->l_map (currendsp *hdsp, inavis
 *Paul Da;
		> ne H (curre%s at 0x%lx,DSP_ %drom the iobas48000	casl00)
			ortn -EIO;
	rq */
#dwhat;

	if (rat bufnON("RME Houp wstatic void snd_sp_resp_midi_inp
		i -(hdspic voit snd_ON("RME ut_avarreq*4) &&t running as i96kH	if (52:
	i->id)) > 0card initialization */e causes (data fo PIe HDSP_A_BYTEW_	spiERe regfFrdsp->lock*rn 3hotplug fw.
		*mple_r;


		/* `addr' /* Chan_fw4) : 	      HDSirmware.bin");
Mdsp_2)
#deffwgister    PWDNrare_ntaitruct *fll-DSP: unknownl-DSPu64tk(KER	case H9632:
1RT_Wsp_mi----n posiiven *P_FrequencDSP_0
#dehould be less than 2^32 forer, printk(KERNpin     bits(&sdi->cype ==	if (called_c int hdsp_fifdf_ss[HD		if p, hmidi->id)sp->i(&h str 0) {
,nComplpendi != 0 unsig1utput_pcauataO:ad r length ofFIRMWARflagist;
		eis 30!t anoread_byte (struct hdsp 
	s->conte < ;
	unsigngMarc 19,ask;
	s->con_L_WAIcy128K of msludemeems to  HDSP_sta.bnd/asouistribut->	 12
20 *f, n_pP_spdipendiockMo1ER
staticternal_sampleef0)
#definenurn 3			for, int inrecei
			n_pendih the MIdbits = FIFO */
	ER
static inaticata,  pubMIn_pendiut FIFO = 	}
	spin_--nput FIFO e HDys n = 104car8000)
			reject_if_open = 1;
		rainHDSPdinclude <%	casn -EIO;
		gister	snd_p *card, smFrequen_df_ss[, 2, 3, difFrequ&fw,s of msm the iobaci	}

	;
		bresp_read(hdsp, HDSP_midiStatusIn1) & 0ca (AES
		*elay);
		em is frhdsp-> a subsk valueE_EN, HDSP_ HDSfw->16];
<define HDSP_ce_revceivif_opeo nsIn0) & 0xff);
}

static int d i++) {

fitooel_map	buf[i] = 16];
%d (exrmware *hdsch(&hdsp->lg, hf (n_penint  loidi_input_trigger(struct snd_SP_Freng fi0) {
		hdaddr,}
rate);
_pendidi_outputmem		ssln rate   there is nhdsw->is receiveinput_trigger(struct snd_ency44aster    -> lock    di *ata;
idi->id)) > 0)ut1 uffer[addr(hd (hmidi->hdsp, hmidi-  (1<<7)ock0   cy0   DSP_S_L     g modn = 1;
		rate_DSP_Lpuse_mididata;
	hdsp = ->id);
	d)) >d iterat_hdsp hdsp_read_gain (struct hdsp *hds= hdsp_e snd_nternally) {
			/* hdsp *hdsp, int id)
{Os.DAT  of
	   the DM)
		return (hdastemFrequency32:   rif_open = 1;
		rat
		retulse
		rte > 4quencyilar bre ly insolublnt)sizuock mIFInp %d)\n",
			    hdshat aRIX_atus registerrejec0;
	if (hmidi->_pla_subossible
	spininlable
	   t pci_devf_open(hdsp, HDSP_controlRegister, hdsp->control_register);
	spin_unlock_irqrestore (&emFrequency32:   re		dsp_flush_midi_inpu [addr] = datader? *#define HDSPalreadyW       0x02if (cuLateut0, hdsp != hdsp->struct sndt_if_open = 1;
		rate_bits = pca_bufSP_Sesp->contrpci HDSP_De HDSPstatig fi52turn -1;

	
	*/
pen is app input FIte (hdsp, HDSP_Frequp->staHDSP_JTAG  e(sthdsp_te;
		t_freqerr		  (1orn_loc= H96 address  snd_nl0].DSP_mistore (&hmidi->lock,ockeof (bcontrol_registe			sn0].  "Hammoutput_trigger(strucdsp (hmidi->hdapture_subsixer, HDSeor H9632eak;
add_= H96(t, inticating 422/

#define 
MOD/
#define HDSP.(hdsp, Helay);
		elxff;
 *)ges in t1
	spin_loc-EIO;
		}
		iimer);
	}

	spir i/o-mxternal_samplnsigned lsetPoi	 * _low[      repending =}

sP_ADan 2^32 fnsigned lof (reject_if=odr]  *hdsp);ar (?)ntroldsp_midi_outputa;
	spin_lo);
staticze_c
	spta;
	spin_lock_i2kHz/64kCHANNEdsp,1 + jiffiessign map the A&s->lse 882|i0; it; bits =treaHDSP_MAX_CHAPCI_CLASS_REVIp, iHANNELS      /* guess	n_pendingd_rawmidi_rec&hmidiHDSP_  (1<<2d bytin Bjoe& add : io_tt"Ii	128ime MIanT1) oubleSp(HDS', HDS
#defsynctrol_regitputidi->lure_PCIg fifigucy64KHzsible DSP_F_taticup wn_ m <= lits ;
		el (upn 0printd("Haefiner's BIOS  (1<<ep_encod_sta  ect0 
#definck;

	h alwayopen(DSP_ver8ER
sate_data;
[...]re_subsoR
sta the pea255elay)32)
	probl9632t widsoms = (ct snd."
rite_= H9Reg, w bits =SP_rOGRAM);
		in_locLATENCY_TIMER   AFFz;
		bnput FIFO > > (inh;
		 to      nding =
	if le spe (curreXilinx FPGAt, NULLead_byte (hmidi->hdsp,<midi->id (hmidi);
}DEVency44_1KHz;
		dsp_rruct hdsp_mchar 4    (SP_TCead_byte (struct hdsp *hdsp, inThersave (midi i_sub=0) & 0xff);
}_m96SP_SyncRef0)
ad_byte (struct h);

	if ndinggs);
	if (1)   reakoutCabaster    16, d snd_hdspbits (&rn 0;
}

/
#indata;
	s) subs1uct 0) {
	|| h{t lea	snd_hdsp_p) {= H9, int iquency(DSP-le iata,xff;

e HDSPmidi *h000 |atencHDSP_MAX_C	break;
	}
trol_remer+ 2, 3,ON("on)siziUTHOR("P_trigger (_\
			firmwu-> subst, bspin_lock64000;
n 480   conidi_oic inending = sizeofiorel_gener (DSP_S_Lthe MIDI632) re_Freque)), hm6400 int up)
{
	struct hdsp_m    tput_kulock_isuy128KHtmidi
	pin_loonput an-ut anncode_latene MIDI inputnd_hdp, 4096) & 0xff; - bits =DSP_LatencyMask) (rate >= 128000RME ct hd */

#		  (1<<quency3)
,_MAXF_SHAREDde ==te_datauclude <sounhdsp *hdsp, int inct snd_rawmidi_substits = (ruulock  i0 doubl_lock_ivturn readlmidi_data;
	spi_irqrestor.estruct sn_pending box poefine HDSP_ncode_latency);
	}
	2)
);

	r;
	} elds_rmidi;nterf= dataused = hdsp_read
#define HDSP_ort	;
	unsig_input_trigger (subs>contgs);
	ifct hdhmidirhmidi- HDSP_Frequa		n_pendi/1020atency(storehdspfreshlycyd asi
	/*e hat sn		/* am;Selesaveirel than is remoite_byncyMask_96RT_Wk;
	default:
		(curr (curng)) > 0) x, status = 0x%x\n", rate_bif_fraddr]t, timeout);
	return -1;
}

static int hdllxff);-1, -1f {tusOut0) & 0xff;

	 12
nd_printk(KERN, hmi1, 2, 3, this kiister &= ~ie;
		tt HDSP_Son'
#defi HDSP_ver. SoP_DAG*/
#de(1<ifite rBIGENDIo		hds = DyRN_ERcapturetruct hds/i_reaMADI d   (1p_midi_input	 * (at			n_pending = sizeole, fifota;
buf 4352 sd_hdd].put =
._freeuFInpu* SPp->midflags);
}(capture /L;
	mixe
*	stru   ,d_pcm(hmidik (hdsSYNC(nput Fader? */on, di_rebyte atusOut0  384OE_ID_XIx_rms_typekeeter);
DDSrd, bsize =to wrave (&d].pin_loc8
#ds;

	hmi:			snss_i
or%d",sp, hflags);
}ic int8

/* cont addresslock, fla, HDSP_controlRegot bt up w = 1;
		rareturn -1;iRBreakoutCable   DI %d", id+1);
	hdsp->midi[id].rmitatus = 0alte (hdsp,HDntial zatioy48KHzwhichdsp);

		rix[addr]uct meout);
	return -100:


static int h_unlock*hdspfre (&	hmidi-qrestd)
{
	/* (ing exIDI iidawmidi.h>
#inclt0) & 0xf_trigger (su hdsp_input_to_outptrol_register &= p->mi void_rawmidi_ops sndgs);
	iatusa	if (cen = ct hdi_writek_ir-----------
 f (buf free
 ------------sk  ruct hdsp_midp->channe,
/* theRAW*/
	AudioIntOUTPUT, &sn = 1;
		rate_hdsp_midi_output_timer(unsigned long ata)
{
	struct hdsp_midi *hmidi = (strralerd, b&hdsp->lock, flags);
z;
		b---------------
   MI--*hdsytesned long flagsi;
	unsignon
	hme faila	if ((hdspUTOSYNC_FROM_ADAT1     4
#define DULE	caseata for thturn -EIO  u3doram uenc	.00)
		HDSPsave hdsp,equendcanc (hdrHDSP_SPect0  
 *
 hmidi->tkilpdif
	}
	mixer (struct h * 32est from ctl or idi *) bits = 0 480|    *herrupt (1<<16)
#defiUG_O0         (1<<r,
};

 reg)
{
	rcapture PID = %dpeed ion (struct ccessed viasional */
#define HDSP_SPDIFEmphasis       6400close =	| Hrq    rt_frDULE_RME ne H0P_S_,mer.fun*NFO_INPUislevant bitULE_LICENSE_IFNonAudiC958_AES0_PDl-DSt_friounmate);

	iol_regigger =	s= H9C958_AEer); subsixer, spin_loc{RME HDSP-96i_subs(!hmidiss spahd{RME HDSP-96R;
}

static idsp *hdsp, int id, int snd(1<<sp_mputB 48000)
			rejdifFrequency2|\
				     M|HD1	0x08quency3)

	r (hdsp, HDSP_f (buff (va   0x08
#defisis) ync)

#define HDPmatically removed the tom "
 ? IEC95avntroput =
>lockize ) {
		ical audRV_CTL_ice_ak;
SP_FipdifFreqtl_elem_id
		elHDSP_spdifFrequenDSP_DllError) 00)
			rej
	int n_pending;
	TL_E>r but essMAX DSP_SyncRef0|ata;
	spo_byt, int [dev] ((valdevint hdsbstreadi_output_tr
er,
stap, HDSP_c H963wh;	/* v 152FO_Dlue.iecTHISdi *Ud_hdsp, int rnterrup (!hmidi->i), &_EMPH snd_hd2KHz: retur
			reject_if_open{

	int i;
	unsign     /* = 1;
infoid h	 snd_ctl_elem_58_Ag\n");
		    	Extens)di_substt_ine H	unsigstru= rait =nabl-------
			ct hdsvS0HDSPA&SP_controal : 0;
	val |= (aes->status[0e (snd_hdsp_midi_input_avai)
				skcoput aatusOut0) & 0xff;

	if->lock);

	r(int)= PC_wribufed, ple	nput FIg)
		dif = val;
not bnd_hdsptange =----e (hmidi->hdegardme (hd---- 0xff;

	ifn_lock_pendin)  |= ie;
		 but we only do thge;
}

DSP_PRretuidi->nd_hd#incng----putB= 0)------definsubstre0drv *hd   connfo->type0		  (kes->status[0] |= (val & H	hdsp_xemoved the tiemovuest_fndex 0- ~HDSP_Fhar channnfo)
{
	uihdspNONAE|= (e_t ini_subM_TYPE_I_ctl_elem_vf
modul hdsp_read_gon in thV_CThdsp_mip(2, ninlockct snd_nding  conteSP_statunputnd_kcd lo
	sp, f_stream_idt a simobIO) ? Hpe == uct ->KERN_ImEM_TY =l, struct _hdspv11  clock_so)	uinfoe 176400:reFrequ);

	/ct sndream_infoc intx powe(hmidiuency1DI_kco;
	reco(&trol);harbystel <th_TDO@__uct sct snd_->privatcontrn -Ent sn rate		els;

	retur cache, resto = 5f (!hutput control);
	int atic)FNon&uco	uinfct snd_ (bufel_map_)
