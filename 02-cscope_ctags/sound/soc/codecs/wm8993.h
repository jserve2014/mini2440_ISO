#ifndef WM8993_H
#define WM8993_H

extern struct snd_soc_dai wm8993_dai;
extern struct snd_soc_codec_device soc_codec_dev_wm8993;

#define WM8993_SYSCLK_MCLK     1
#define WM8993_SYSCLK_FLL      2

#define WM8993_FLL_MCLK  1
#define WM8993_FLL_BCLK  2
#define WM8993_FLL_LRCLK 3

/*
 * Register values.
 */
#define WM8993_SOFTWARE_RESET                   0x00
#define WM8993_POWER_MANAGEMENT_1               0x01
#define WM8993_POWER_MANAGEMENT_2               0x02
#define WM8993_POWER_MANAGEMENT_3               0x03
#define WM8993_AUDIO_INTERFACE_1                0x04
#define WM8993_AUDIO_INTERFACE_2                0x05
#define WM8993_CLOCKING_1                       0x06
#define WM8993_CLOCKING_2                       0x07
#define WM8993_AUDIO_INTERFACE_3                0x08
#define WM8993_AUDIO_INTERFACE_4                0x09
#define WM8993_DAC_CTRL                         0x0A
#define WM8993_LEFT_DAC_DIGITAL_VOLUME          0x0B
#define WM8993_RIGHT_DAC_DIGITAL_VOLUME         0x0C
#define WM8993_DIGITAL_SIDE_TONE                0x0D
#define WM8993_ADC_CTRL                         0x0E
#define WM8993_LEFT_ADC_DIGITAL_VOLUME          0x0F
#define WM8993_RIGHT_ADC_DIGITAL_VOLUME         0x10
#define WM8993_GPIO_CTRL_1                      0x12
#define WM8993_GPIO1                            0x13
#define WM8993_IRQ_DEBOUNCE                     0x14
#define WM8993_GPIOCTRL_2                       0x16
#define WM8993_GPIO_POL                         0x17
#define WM8993_LEFT_LINE_INPUT_1_2_VOLUME       0x18
#define WM8993_LEFT_LINE_INPUT_3_4_VOLUME       0x19
#define WM8993_RIGHT_LINE_INPUT_1_2_VOLUME      0x1A
#define WM8993_RIGHT_LINE_INPUT_3_4_VOLUME      0x1B
#define WM8993_LEFT_OUTPUT_VOLUME               0x1C
#define WM8993_RIGHT_OUTPUT_VOLUME              0x1D
#define WM8993_LINE_OUTPUTS_VOLUME              0x1E
#define WM8993_HPOUT2_VOLUME                    0x1F
#define WM8993_LEFT_OPGA_VOLUME                 0x20
#define WM8993_RIGHT_OPGA_VOLUME                0x21
#define WM8993_SPKMIXL_ATTENUATION              0x22
#define WM8993_SPKMIXR_ATTENUATION              0x23
#define WM8993_SPKOUT_MIXERS                    0x24
#define WM8993_SPKOUT_BOOST                     0x25
#define WM8993_SPEAKER_VOLUME_LEFT              0x26
#define WM8993_SPEAKER_VOLUME_RIGHT             0x27
#define WM8993_INPUT_MIXER2                     0x28
#define WM8993_INPUT_MIXER3                     0x29
#define WM8993_INPUT_MIXER4                     0x2A
#define WM8993_INPUT_MIXER5                     0x2B
#define WM8993_INPUT_MIXER6                     0x2C
#define WM8993_OUTPUT_MIXER1                    0x2D
#define WM8993_OUTPUT_MIXER2                    0x2E
#define WM8993_OUTPUT_MIXER3                    0x2F
#define WM8993_OUTPUT_MIXER4                    0x30
#define WM8993_OUTPUT_MIXER5                    0x31
#define WM8993_OUTPUT_MIXER6                    0x32
#define WM8993_HPOUT2_MIXER                     0x33
#define WM8993_LINE_MIXER1                      0x34
#define WM8993_LINE_MIXER2                      0x35
#define WM8993_SPEAKER_MIXER                    0x36
#define WM8993_ADDITIONAL_CONTROL               0x37
#define WM8993_ANTIPOP1                         0x38
#define WM8993_ANTIPOP2                         0x39
#define WM8993_MICBIAS                          0x3A
#define WM8993_FLL_CONTROL_1                    0x3C
#define WM8993_FLL_CONTROL_2                    0x3D
#define WM8993_FLL_CONTROL_3                    0x3E
#define WM8993_FLL_CONTROL_4                    0x3F
#define WM8993_FLL_CONTROL_5                    0x40
#define WM8993_CLOCKING_3                       0x41
#define WM8993_CLOCKING_4                       0x42
#define WM8993_MW_SLAVE_CONTROL                 0x43
#define WM8993_BUS_CONTROL_1                    0x45
#define WM8993_WRITE_SEQUENCER_0                0x46
#define WM8993_WRITE_SEQUENCER_1                0x47
#define WM8993_WRITE_SEQUENCER_2                0x48
#define WM8993_WRITE_SEQUENCER_3                0x49
#define WM8993_WRITE_SEQUENCER_4                0x4A
#define WM8993_WRITE_SEQUENCER_5                0x4B
#define WM8993_CHARGE_PUMP_1                    0x4C
#define WM8993_CLASS_W_0                        0x51
#define WM8993_DC_SERVO_0                       0x54
#define WM8993_DC_SERVO_1                       0x55
#define WM8993_DC_SERVO_3                       0x57
#define WM8993_DC_SERVO_READBACK_0              0x58
#define WM8993_DC_SERVO_READBACK_1              0x59
#define WM8993_DC_SERVO_READBACK_2              0x5A
#define WM8993_ANALOGUE_HP_0                    0x60
#define WM8993_EQ1                              0x62
#define WM8993_EQ2                              0x63
#define WM8993_EQ3                              0x64
#define WM8993_EQ4                              0x65
#define WM8993_EQ5                              0x66
#define WM8993_EQ6                              0x67
#define WM8993_EQ7                              0x68
#define WM8993_EQ8                              0x69
#define WM8993_EQ9                              0x6A
#define WM8993_EQ10                             0x6B
#define WM8993_EQ11                             0x6C
#define WM8993_EQ12                             0x6D
#define WM8993_EQ13                             0x6E
#define WM8993_EQ14                             0x6F
#define WM8993_EQ15                             0x70
#define WM8993_EQ16                             0x71
#define WM8993_EQ17                             0x72
#define WM8993_EQ18                             0x73
#define WM8993_EQ19                             0x74
#define WM8993_EQ20                             0x75
#define WM8993_EQ21                             0x76
#define WM8993_EQ22                             0x77
#define WM8993_EQ23                             0x78
#define WM8993_EQ24                             0x79
#define WM8993_DIGITAL_PULLS                    0x7A
#define WM8993_DRC_CONTROL_1                    0x7B
#define WM8993_DRC_CONTROL_2                    0x7C
#define WM8993_DRC_CONTROL_3                    0x7D
#define WM8993_DRC_CONTROL_4                    0x7E

#define WM8993_REGISTER_COUNT                   0x7F
#define WM8993_MAX_REGISTER                     0x7E

/*
 * Field Definitions.
 */

/*
 * R0 (0x00) - Software Reset
 */
#define WM8993_SW_RESET_MASK                    0xFFFF  /* SW_RESET - [15:0] */
#define WM8993_SW_RESET_SHIFT                        0  /* SW_RESET - [15:0] */
#define WM8993_SW_RESET_WIDTH                       16  /* SW_RESET - [15:0] */

/*
 * R1 (0x01) - Power Management (1)
 */
#define WM8993_SPKOUTR_ENA                      0x2000  /* SPKOUTR_ENA */
#define WM8993_SPKOUTR_ENA_MASK                 0x2000  /* SPKOUTR_ENA */
#define WM8993_SPKOUTR_ENA_SHIFT                    13  /* SPKOUTR_ENA */
#define WM8993_SPKOUTR_ENA_WIDTH                     1  /* SPKOUTR_ENA */
#define WM8993_SPKOUTL_ENA                      0x1000  /* SPKOUTL_ENA */
#define WM8993_SPKOUTL_ENA_MASK                 0x1000  /* SPKOUTL_ENA */
#define WM8993_SPKOUTL_ENA_SHIFT                    12  /* SPKOUTL_ENA */
#define WM8993_SPKOUTL_ENA_WIDTH                     1  /* SPKOUTL_ENA */
#define WM8993_HPOUT2_ENA                       0x0800  /* HPOUT2_ENA */
#define WM8993_HPOUT2_ENA_MASK                  0x0800  /* HPOUT2_ENA */
#define WM8993_HPOUT2_ENA_SHIFT                     11  /* HPOUT2_ENA */
#define WM8993_HPOUT2_ENA_WIDTH                      1  /* HPOUT2_ENA */
#define WM8993_HPOUT1L_ENA                      0x0200  /* HPOUT1L_ENA */
#define WM8993_HPOUT1L_ENA_MASK                 0x0200  /* HPOUT1L_ENA */
#define WM8993_HPOUT1L_ENA_SHIFT                     9  /* HPOUT1L_ENA */
#define WM8993_HPOUT1L_ENA_WIDTH                     1  /* HPOUT1L_ENA */
#define WM8993_HPOUT1R_ENA                      0x0100  /* HPOUT1R_ENA */
#define WM8993_HPOUT1R_ENA_MASK                 0x0100  /* HPOUT1R_ENA */
#define WM8993_HPOUT1R_ENA_SHIFT                     8  /* HPOUT1R_ENA */
#define WM8993_HPOUT1R_ENA_WIDTH                     1  /* HPOUT1R_ENA */
#define WM8993_MICB2_ENA                        0x0020  /* MICB2_ENA */
#define WM8993_MICB2_ENA_MASK                   0x0020  /* MICB2_ENA */
#define WM8993_MICB2_ENA_SHIFT                       5  /* MICB2_ENA */
#define WM8993_MICB2_ENA_WIDTH                       1  /* MICB2_ENA */
#define WM8993_MICB1_ENA                        0x0010  /* MICB1_ENA */
#define WM8993_MICB1_ENA_MASK                   0x0010  /* MICB1_ENA */
#define WM8993_MICB1_ENA_SHIFT                       4  /* MICB1_ENA */
#define WM8993_MICB1_ENA_WIDTH                       1  /* MICB1_ENA */
#define WM8993_VMID_SEL_MASK                    0x0006  /* VMID_SEL - [2:1] */
#define WM8993_VMID_SEL_SHIFT                        1  /* VMID_SEL - [2:1] */
#define WM8993_VMID_SEL_WIDTH                        2  /* VMID_SEL - [2:1] */
#define WM8993_BIAS_ENA                         0x0001  /* BIAS_ENA */
#define WM8993_BIAS_ENA_MASK                    0x0001  /* BIAS_ENA */
#define WM8993_BIAS_ENA_SHIFT                        0  /* BIAS_ENA */
#define WM8993_BIAS_ENA_WIDTH                        1  /* BIAS_ENA */

/*
 * R2 (0x02) - Power Management (2)
 */
#define WM8993_TSHUT_ENA                        0x4000  /* TSHUT_ENA */
#define WM8993_TSHUT_ENA_MASK                   0x4000  /* TSHUT_ENA */
#define WM8993_TSHUT_ENA_SHIFT                      14  /* TSHUT_ENA */
#define WM8993_TSHUT_ENA_WIDTH                       1  /* TSHUT_ENA */
#define WM8993_TSHUT_OPDIS                      0x2000  /* TSHUT_OPDIS */
#define WM8993_TSHUT_OPDIS_MASK                 0x2000  /* TSHUT_OPDIS */
#define WM8993_TSHUT_OPDIS_SHIFT                    13  /* TSHUT_OPDIS */
#define WM8993_TSHUT_OPDIS_WIDTH                     1  /* TSHUT_OPDIS */
#define WM8993_OPCLK_ENA                        0x0800  /* OPCLK_ENA */
#define WM8993_OPCLK_ENA_MASK                   0x0800  /* OPCLK_ENA */
#define WM8993_OPCLK_ENA_SHIFT                      11  /* OPCLK_ENA */
#define WM8993_OPCLK_ENA_WIDTH                       1  /* OPCLK_ENA */
#define WM8993_MIXINL_ENA                       0x0200  /* MIXINL_ENA */
#define WM8993_MIXINL_ENA_MASK                  0x0200  /* MIXINL_ENA */
#define WM8993_MIXINL_ENA_SHIFT                      9  /* MIXINL_ENA */
#define WM8993_MIXINL_ENA_WIDTH                      1  /* MIXINL_ENA */
#define WM8993_MIXINR_ENA                       0x0100  /* MIXINR_ENA */
#define WM8993_MIXINR_ENA_MASK                  0x0100  /* MIXINR_ENA */
#define WM8993_MIXINR_ENA_SHIFT                      8  /* MIXINR_ENA */
#define WM8993_MIXINR_ENA_WIDTH                      1  /* MIXINR_ENA */
#define WM8993_IN2L_ENA                         0x0080  /* IN2L_ENA */
#define WM8993_IN2L_ENA_MASK                    0x0080  /* IN2L_ENA */
#define WM8993_IN2L_ENA_SHIFT                        7  /* IN2L_ENA */
#define WM8993_IN2L_ENA_WIDTH                        1  /* IN2L_ENA */
#define WM8993_IN1L_ENA                         0x0040  /* IN1L_ENA */
#define WM8993_IN1L_ENA_MASK                    0x0040  /* IN1L_ENA */
#define WM8993_IN1L_ENA_SHIFT                        6  /* IN1L_ENA */
#define WM8993_IN1L_ENA_WIDTH                        1  /* IN1L_ENA */
#define WM8993_IN2R_ENA                         0x0020  /* IN2R_ENA */
#define WM8993_IN2R_ENA_MASK                    0x0020  /* IN2R_ENA */
#define WM8993_IN2R_ENA_SHIFT                        5  /* IN2R_ENA */
#define WM8993_IN2R_ENA_WIDTH                        1  /* IN2R_ENA */
#define WM8993_IN1R_ENA                         0x0010  /* IN1R_ENA */
#define WM8993_IN1R_ENA_MASK                    0x0010  /* IN1R_ENA */
#define WM8993_IN1R_ENA_SHIFT                        4  /* IN1R_ENA */
#define WM8993_IN1R_ENA_WIDTH                        1  /* IN1R_ENA */
#define WM8993_ADCL_ENA                         0x0002  /* ADCL_ENA */
#define WM8993_ADCL_ENA_MASK                    0x0002  /* ADCL_ENA */
#define WM8993_ADCL_ENA_SHIFT                        1  /* ADCL_ENA */
#define WM8993_ADCL_ENA_WIDTH                        1  /* ADCL_ENA */
#define WM8993_ADCR_ENA                         0x0001  /* ADCR_ENA */
#define WM8993_ADCR_ENA_MASK                    0x0001  /* ADCR_ENA */
#define WM8993_ADCR_ENA_SHIFT                        0  /* ADCR_ENA */
#define WM8993_ADCR_ENA_WIDTH                        1  /* ADCR_ENA */

/*
 * R3 (0x03) - Power Management (3)
 */
#define WM8993_LINEOUT1N_ENA                    0x2000  /* LINEOUT1N_ENA */
#define WM8993_LINEOUT1N_ENA_MASK               0x2000  /* LINEOUT1N_ENA */
#define WM8993_LINEOUT1N_ENA_SHIFT                  13  /* LINEOUT1N_ENA */
#define WM8993_LINEOUT1N_ENA_WIDTH                   1  /* LINEOUT1N_ENA */
#define WM8993_LINEOUT1P_ENA                    0x1000  /* LINEOUT1P_ENA */
#define WM8993_LINEOUT1P_ENA_MASK               0x1000  /* LINEOUT1P_ENA */
#define WM8993_LINEOUT1P_ENA_SHIFT                  12  /* LINEOUT1P_ENA */
#define WM8993_LINEOUT1P_ENA_WIDTH                   1  /* LINEOUT1P_ENA */
#define WM8993_LINEOUT2N_ENA                    0x0800  /* LINEOUT2N_ENA */
#define WM8993_LINEOUT2N_ENA_MASK               0x0800  /* LINEOUT2N_ENA */
#define WM8993_LINEOUT2N_ENA_SHIFT                  11  /* LINEOUT2N_ENA */
#define WM8993_LINEOUT2N_ENA_WIDTH                   1  /* LINEOUT2N_ENA */
#define WM8993_LINEOUT2P_ENA                    0x0400  /* LINEOUT2P_ENA */
#define WM8993_LINEOUT2P_ENA_MASK               0x0400  /* LINEOUT2P_ENA */
#define WM8993_LINEOUT2P_ENA_SHIFT                  10  /* LINEOUT2P_ENA */
#define WM8993_LINEOUT2P_ENA_WIDTH                   1  /* LINEOUT2P_ENA */
#define WM8993_SPKRVOL_ENA                      0x0200  /* SPKRVOL_ENA */
#define WM8993_SPKRVOL_ENA_MASK                 0x0200  /* SPKRVOL_ENA */
#define WM8993_SPKRVOL_ENA_SHIFT                     9  /* SPKRVOL_ENA */
#define WM8993_SPKRVOL_ENA_WIDTH                     1  /* SPKRVOL_ENA */
#define WM8993_SPKLVOL_ENA                      0x0100  /* SPKLVOL_ENA */
#define WM8993_SPKLVOL_ENA_MASK                 0x0100  /* SPKLVOL_ENA */
#define WM8993_SPKLVOL_ENA_SHIFT                     8  /* SPKLVOL_ENA */
#define WM8993_SPKLVOL_ENA_WIDTH                     1  /* SPKLVOL_ENA */
#define WM8993_MIXOUTLVOL_ENA                   0x0080  /* MIXOUTLVOL_ENA */
#define WM8993_MIXOUTLVOL_ENA_MASK              0x0080  /* MIXOUTLVOL_ENA */
#define WM8993_MIXOUTLVOL_ENA_SHIFT                  7  /* MIXOUTLVOL_ENA */
#define WM8993_MIXOUTLVOL_ENA_WIDTH                  1  /* MIXOUTLVOL_ENA */
#define WM8993_MIXOUTRVOL_ENA                   0x0040  /* MIXOUTRVOL_ENA */
#define WM8993_MIXOUTRVOL_ENA_MASK              0x0040  /* MIXOUTRVOL_ENA */
#define WM8993_MIXOUTRVOL_ENA_SHIFT                  6  /* MIXOUTRVOL_ENA */
#define WM8993_MIXOUTRVOL_ENA_WIDTH                  1  /* MIXOUTRVOL_ENA */
#define WM8993_MIXOUTL_ENA                      0x0020  /* MIXOUTL_ENA */
#define WM8993_MIXOUTL_ENA_MASK                 0x0020  /* MIXOUTL_ENA */
#define WM8993_MIXOUTL_ENA_SHIFT                     5  /* MIXOUTL_ENA */
#define WM8993_MIXOUTL_ENA_WIDTH                     1  /* MIXOUTL_ENA */
#define WM8993_MIXOUTR_ENA                      0x0010  /* MIXOUTR_ENA */
#define WM8993_MIXOUTR_ENA_MASK                 0x0010  /* MIXOUTR_ENA */
#define WM8993_MIXOUTR_ENA_SHIFT                     4  /* MIXOUTR_ENA */
#define WM8993_MIXOUTR_ENA_WIDTH                     1  /* MIXOUTR_ENA */
#define WM8993_DACL_ENA                         0x0002  /* DACL_ENA */
#define WM8993_DACL_ENA_MASK                    0x0002  /* DACL_ENA */
#define WM8993_DACL_ENA_SHIFT                        1  /* DACL_ENA */
#define WM8993_DACL_ENA_WIDTH                        1  /* DACL_ENA */
#define WM8993_DACR_ENA                         0x0001  /* DACR_ENA */
#define WM8993_DACR_ENA_MASK                    0x0001  /* DACR_ENA */
#define WM8993_DACR_ENA_SHIFT                        0  /* DACR_ENA */
#define WM8993_DACR_ENA_WIDTH                        1  /* DACR_ENA */

/*
 * R4 (0x04) - Audio Interface (1)
 */
#define WM8993_AIFADCL_SRC                      0x8000  /* AIFADCL_SRC */
#define WM8993_AIFADCL_SRC_MASK                 0x8000  /* AIFADCL_SRC */
#define WM8993_AIFADCL_SRC_SHIFT                    15  /* AIFADCL_SRC */
#define WM8993_AIFADCL_SRC_WIDTH                     1  /* AIFADCL_SRC */
#define WM8993_AIFADCR_SRC                      0x4000  /* AIFADCR_SRC */
#define WM8993_AIFADCR_SRC_MASK                 0x4000  /* AIFADCR_SRC */
#define WM8993_AIFADCR_SRC_SHIFT                    14  /* AIFADCR_SRC */
#define WM8993_AIFADCR_SRC_WIDTH                     1  /* AIFADCR_SRC */
#define WM8993_AIFADC_TDM                       0x2000  /* AIFADC_TDM */
#define WM8993_AIFADC_TDM_MASK                  0x2000  /* AIFADC_TDM */
#define WM8993_AIFADC_TDM_SHIFT                     13  /* AIFADC_TDM */
#define WM8993_AIFADC_TDM_WIDTH                      1  /* AIFADC_TDM */
#define WM8993_AIFADC_TDM_CHAN                  0x1000  /* AIFADC_TDM_CHAN */
#define WM8993_AIFADC_TDM_CHAN_MASK             0x1000  /* AIFADC_TDM_CHAN */
#define WM8993_AIFADC_TDM_CHAN_SHIFT                12  /* AIFADC_TDM_CHAN */
#define WM8993_AIFADC_TDM_CHAN_WIDTH                 1  /* AIFADC_TDM_CHAN */
#define WM8993_BCLK_DIR                         0x0200  /* BCLK_DIR */
#define WM8993_BCLK_DIR_MASK                    0x0200  /* BCLK_DIR */
#define WM8993_BCLK_DIR_SHIFT                        9  /* BCLK_DIR */
#define WM8993_BCLK_DIR_WIDTH                        1  /* BCLK_DIR */
#define WM8993_AIF_BCLK_INV                     0x0100  /* AIF_BCLK_INV */
#define WM8993_AIF_BCLK_INV_MASK                0x0100  /* AIF_BCLK_INV */
#define WM8993_AIF_BCLK_INV_SHIFT                    8  /* AIF_BCLK_INV */
#define WM8993_AIF_BCLK_INV_WIDTH                    1  /* AIF_BCLK_INV */
#define WM8993_AIF_LRCLK_INV                    0x0080  /* AIF_LRCLK_INV */
#define WM8993_AIF_LRCLK_INV_MASK               0x0080  /* AIF_LRCLK_INV */
#define WM8993_AIF_LRCLK_INV_SHIFT                   7  /* AIF_LRCLK_INV */
#define WM8993_AIF_LRCLK_INV_WIDTH                   1  /* AIF_LRCLK_INV */
#define WM8993_AIF_WL_MASK                      0x0060  /* AIF_WL - [6:5] */
#define WM8993_AIF_WL_SHIFT                          5  /* AIF_WL - [6:5] */
#define WM8993_AIF_WL_WIDTH                          2  /* AIF_WL - [6:5] */
#define WM8993_AIF_FMT_MASK                     0x0018  /* AIF_FMT - [4:3] */
#define WM8993_AIF_FMT_SHIFT                         3  /* AIF_FMT - [4:3] */
#define WM8993_AIF_FMT_WIDTH                         2  /* AIF_FMT - [4:3] */

/*
 * R5 (0x05) - Audio Interface (2)
 */
#define WM8993_AIFDACL_SRC                      0x8000  /* AIFDACL_SRC */
#define WM8993_AIFDACL_SRC_MASK                 0x8000  /* AIFDACL_SRC */
#define WM8993_AIFDACL_SRC_SHIFT                    15  /* AIFDACL_SRC */
#define WM8993_AIFDACL_SRC_WIDTH                     1  /* AIFDACL_SRC */
#define WM8993_AIFDACR_SRC                      0x4000  /* AIFDACR_SRC */
#define WM8993_AIFDACR_SRC_MASK                 0x4000  /* AIFDACR_SRC */
#define WM8993_AIFDACR_SRC_SHIFT                    14  /* AIFDACR_SRC */
#define WM8993_AIFDACR_SRC_WIDTH                     1  /* AIFDACR_SRC */
#define WM8993_AIFDAC_TDM                       0x2000  /* AIFDAC_TDM */
#define WM8993_AIFDAC_TDM_MASK                  0x2000  /* AIFDAC_TDM */
#define WM8993_AIFDAC_TDM_SHIFT                     13  /* AIFDAC_TDM */
#define WM8993_AIFDAC_TDM_WIDTH                      1  /* AIFDAC_TDM */
#define WM8993_AIFDAC_TDM_CHAN                  0x1000  /* AIFDAC_TDM_CHAN */
#define WM8993_AIFDAC_TDM_CHAN_MASK             0x1000  /* AIFDAC_TDM_CHAN */
#define WM8993_AIFDAC_TDM_CHAN_SHIFT                12  /* AIFDAC_TDM_CHAN */
#define WM8993_AIFDAC_TDM_CHAN_WIDTH                 1  /* AIFDAC_TDM_CHAN */
#define WM8993_DAC_BOOST_MASK                   0x0C00  /* DAC_BOOST - [11:10] */
#define WM8993_DAC_BOOST_SHIFT                      10  /* DAC_BOOST - [11:10] */
#define WM8993_DAC_BOOST_WIDTH                       2  /* DAC_BOOST - [11:10] */
#define WM8993_DAC_COMP                         0x0010  /* DAC_COMP */
#define WM8993_DAC_COMP_MASK                    0x0010  /* DAC_COMP */
#define WM8993_DAC_COMP_SHIFT                        4  /* DAC_COMP */
#define WM8993_DAC_COMP_WIDTH                        1  /* DAC_COMP */
#define WM8993_DAC_COMPMODE                     0x0008  /* DAC_COMPMODE */
#define WM8993_DAC_COMPMODE_MASK                0x0008  /* DAC_COMPMODE */
#define WM8993_DAC_COMPMODE_SHIFT                    3  /* DAC_COMPMODE */
#define WM8993_DAC_COMPMODE_WIDTH                    1  /* DAC_COMPMODE */
#define WM8993_ADC_COMP                         0x0004  /* ADC_COMP */
#define WM8993_ADC_COMP_MASK                    0x0004  /* ADC_COMP */
#define WM8993_ADC_COMP_SHIFT                        2  /* ADC_COMP */
#define WM8993_ADC_COMP_WIDTH                        1  /* ADC_COMP */
#define WM8993_ADC_COMPMODE                     0x0002  /* ADC_COMPMODE */
#define WM8993_ADC_COMPMODE_MASK                0x0002  /* ADC_COMPMODE */
#define WM8993_ADC_COMPMODE_SHIFT                    1  /* ADC_COMPMODE */
#define WM8993_ADC_COMPMODE_WIDTH                    1  /* ADC_COMPMODE */
#define WM8993_LOOPBACK                         0x0001  /* LOOPBACK */
#define WM8993_LOOPBACK_MASK                    0x0001  /* LOOPBACK */
#define WM8993_LOOPBACK_SHIFT                        0  /* LOOPBACK */
#define WM8993_LOOPBACK_WIDTH                        1  /* LOOPBACK */

/*
 * R6 (0x06) - Clocking 1
 */
#define WM8993_TOCLK_RATE                       0x8000  /* TOCLK_RATE */
#define WM8993_TOCLK_RATE_MASK                  0x8000  /* TOCLK_RATE */
#define WM8993_TOCLK_RATE_SHIFT                     15  /* TOCLK_RATE */
#define WM8993_TOCLK_RATE_WIDTH                      1  /* TOCLK_RATE */
#define WM8993_TOCLK_ENA                        0x4000  /* TOCLK_ENA */
#define WM8993_TOCLK_ENA_MASK                   0x4000  /* TOCLK_ENA */
#define WM8993_TOCLK_ENA_SHIFT                      14  /* TOCLK_ENA */
#define WM8993_TOCLK_ENA_WIDTH                       1  /* TOCLK_ENA */
#define WM8993_OPCLK_DIV_MASK                   0x1E00  /* OPCLK_DIV - [12:9] */
#define WM8993_OPCLK_DIV_SHIFT                       9  /* OPCLK_DIV - [12:9] */
#define WM8993_OPCLK_DIV_WIDTH                       4  /* OPCLK_DIV - [12:9] */
#define WM8993_DCLK_DIV_MASK                    0x01C0  /* DCLK_DIV - [8:6] */
#define WM8993_DCLK_DIV_SHIFT                        6  /* DCLK_DIV - [8:6] */
#define WM8993_DCLK_DIV_WIDTH                        3  /* DCLK_DIV - [8:6] */
#define WM8993_BCLK_DIV_MASK                    0x001E  /* BCLK_DIV - [4:1] */
#define WM8993_BCLK_DIV_SHIFT                        1  /* BCLK_DIV - [4:1] */
#define WM8993_BCLK_DIV_WIDTH                        4  /* BCLK_DIV - [4:1] */

/*
 * R7 (0x07) - Clocking 2
 */
#define WM8993_MCLK_SRC                         0x8000  /* MCLK_SRC */
#define WM8993_MCLK_SRC_MASK                    0x8000  /* MCLK_SRC */
#define WM8993_MCLK_SRC_SHIFT                       15  /* MCLK_SRC */
#define WM8993_MCLK_SRC_WIDTH                        1  /* MCLK_SRC */
#define WM8993_SYSCLK_SRC                       0x4000  /* SYSCLK_SRC */
#define WM8993_SYSCLK_SRC_MASK                  0x4000  /* SYSCLK_SRC */
#define WM8993_SYSCLK_SRC_SHIFT                     14  /* SYSCLK_SRC */
#define WM8993_SYSCLK_SRC_WIDTH                      1  /* SYSCLK_SRC */
#define WM8993_MCLK_DIV                         0x1000  /* MCLK_DIV */
#define WM8993_MCLK_DIV_MASK                    0x1000  /* MCLK_DIV */
#define WM8993_MCLK_DIV_SHIFT                       12  /* MCLK_DIV */
#define WM8993_MCLK_DIV_WIDTH                        1  /* MCLK_DIV */
#define WM8993_MCLK_INV                         0x0400  /* MCLK_INV */
#define WM8993_MCLK_INV_MASK                    0x0400  /* MCLK_INV */
#define WM8993_MCLK_INV_SHIFT                       10  /* MCLK_INV */
#define WM8993_MCLK_INV_WIDTH                        1  /* MCLK_INV */
#define WM8993_ADC_DIV_MASK                     0x00E0  /* ADC_DIV - [7:5] */
#define WM8993_ADC_DIV_SHIFT                         5  /* ADC_DIV - [7:5] */
#define WM8993_ADC_DIV_WIDTH                         3  /* ADC_DIV - [7:5] */
#define WM8993_DAC_DIV_MASK                     0x001C  /* DAC_DIV - [4:2] */
#define WM8993_DAC_DIV_SHIFT                         2  /* DAC_DIV - [4:2] */
#define WM8993_DAC_DIV_WIDTH                         3  /* DAC_DIV - [4:2] */

/*
 * R8 (0x08) - Audio Interface (3)
 */
#define WM8993_AIF_MSTR1                        0x8000  /* AIF_MSTR1 */
#define WM8993_AIF_MSTR1_MASK                   0x8000  /* AIF_MSTR1 */
#define WM8993_AIF_MSTR1_SHIFT                      15  /* AIF_MSTR1 */
#define WM8993_AIF_MSTR1_WIDTH                       1  /* AIF_MSTR1 */

/*
 * R9 (0x09) - Audio Interface (4)
 */
#define WM8993_AIF_TRIS                         0x2000  /* AIF_TRIS */
#define WM8993_AIF_TRIS_MASK                    0x2000  /* AIF_TRIS */
#define WM8993_AIF_TRIS_SHIFT                       13  /* AIF_TRIS */
#define WM8993_AIF_TRIS_WIDTH                        1  /* AIF_TRIS */
#define WM8993_LRCLK_DIR                        0x0800  /* LRCLK_DIR */
#define WM8993_LRCLK_DIR_MASK                   0x0800  /* LRCLK_DIR */
#define WM8993_LRCLK_DIR_SHIFT                      11  /* LRCLK_DIR */
#define WM8993_LRCLK_DIR_WIDTH                       1  /* LRCLK_DIR */
#define WM8993_LRCLK_RATE_MASK                  0x07FF  /* LRCLK_RATE - [10:0] */
#define WM8993_LRCLK_RATE_SHIFT                      0  /* LRCLK_RATE - [10:0] */
#define WM8993_LRCLK_RATE_WIDTH                     11  /* LRCLK_RATE - [10:0] */

/*
 * R10 (0x0A) - DAC CTRL
 */
#define WM8993_DAC_OSR128                       0x2000  /* DAC_OSR128 */
#define WM8993_DAC_OSR128_MASK                  0x2000  /* DAC_OSR128 */
#define WM8993_DAC_OSR128_SHIFT                     13  /* DAC_OSR128 */
#define WM8993_DAC_OSR128_WIDTH                      1  /* DAC_OSR128 */
#define WM8993_DAC_MONO                         0x0200  /* DAC_MONO */
#define WM8993_DAC_MONO_MASK                    0x0200  /* DAC_MONO */
#define WM8993_DAC_MONO_SHIFT                        9  /* DAC_MONO */
#define WM8993_DAC_MONO_WIDTH                        1  /* DAC_MONO */
#define WM8993_DAC_SB_FILT                      0x0100  /* DAC_SB_FILT */
#define WM8993_DAC_SB_FILT_MASK                 0x0100  /* DAC_SB_FILT */
#define WM8993_DAC_SB_FILT_SHIFT                     8  /* DAC_SB_FILT */
#define WM8993_DAC_SB_FILT_WIDTH                     1  /* DAC_SB_FILT */
#define WM8993_DAC_MUTERATE                     0x0080  /* DAC_MUTERATE */
#define WM8993_DAC_MUTERATE_MASK                0x0080  /* DAC_MUTERATE */
#define WM8993_DAC_MUTERATE_SHIFT                    7  /* DAC_MUTERATE */
#define WM8993_DAC_MUTERATE_WIDTH                    1  /* DAC_MUTERATE */
#define WM8993_DAC_UNMUTE_RAMP                  0x0040  /* DAC_UNMUTE_RAMP */
#define WM8993_DAC_UNMUTE_RAMP_MASK             0x0040  /* DAC_UNMUTE_RAMP */
#define WM8993_DAC_UNMUTE_RAMP_SHIFT                 6  /* DAC_UNMUTE_RAMP */
#define WM8993_DAC_UNMUTE_RAMP_WIDTH                 1  /* DAC_UNMUTE_RAMP */
#define WM8993_DEEMPH_MASK                      0x0030  /* DEEMPH - [5:4] */
#define WM8993_DEEMPH_SHIFT                          4  /* DEEMPH - [5:4] */
#define WM8993_DEEMPH_WIDTH                          2  /* DEEMPH - [5:4] */
#define WM8993_DAC_MUTE                         0x0004  /* DAC_MUTE */
#define WM8993_DAC_MUTE_MASK                    0x0004  /* DAC_MUTE */
#define WM8993_DAC_MUTE_SHIFT                        2  /* DAC_MUTE */
#define WM8993_DAC_MUTE_WIDTH                        1  /* DAC_MUTE */
#define WM8993_DACL_DATINV                      0x0002  /* DACL_DATINV */
#define WM8993_DACL_DATINV_MASK                 0x0002  /* DACL_DATINV */
#define WM8993_DACL_DATINV_SHIFT                     1  /* DACL_DATINV */
#define WM8993_DACL_DATINV_WIDTH                     1  /* DACL_DATINV */
#define WM8993_DACR_DATINV                      0x0001  /* DACR_DATINV */
#define WM8993_DACR_DATINV_MASK                 0x0001  /* DACR_DATINV */
#define WM8993_DACR_DATINV_SHIFT                     0  /* DACR_DATINV */
#define WM8993_DACR_DATINV_WIDTH                     1  /* DACR_DATINV */

/*
 * R11 (0x0B) - Left DAC Digital Volume
 */
#define WM8993_DAC_VU                           0x0100  /* DAC_VU */
#define WM8993_DAC_VU_MASK                      0x0100  /* DAC_VU */
#define WM8993_DAC_VU_SHIFT                          8  /* DAC_VU */
#define WM8993_DAC_VU_WIDTH                          1  /* DAC_VU */
#define WM8993_DACL_VOL_MASK                    0x00FF  /* DACL_VOL - [7:0] */
#define WM8993_DACL_VOL_SHIFT                        0  /* DACL_VOL - [7:0] */
#define WM8993_DACL_VOL_WIDTH                        8  /* DACL_VOL - [7:0] */

/*
 * R12 (0x0C) - Right DAC Digital Volume
 */
#define WM8993_DAC_VU                           0x0100  /* DAC_VU */
#define WM8993_DAC_VU_MASK                      0x0100  /* DAC_VU */
#define WM8993_DAC_VU_SHIFT                          8  /* DAC_VU */
#define WM8993_DAC_VU_WIDTH                          1  /* DAC_VU */
#define WM8993_DACR_VOL_MASK                    0x00FF  /* DACR_VOL - [7:0] */
#define WM8993_DACR_VOL_SHIFT                        0  /* DACR_VOL - [7:0] */
#define WM8993_DACR_VOL_WIDTH                        8  /* DACR_VOL - [7:0] */

/*
 * R13 (0x0D) - Digital Side Tone
 */
#define WM8993_ADCL_DAC_SVOL_MASK               0x1E00  /* ADCL_DAC_SVOL - [12:9] */
#define WM8993_ADCL_DAC_SVOL_SHIFT                   9  /* ADCL_DAC_SVOL - [12:9] */
#define WM8993_ADCL_DAC_SVOL_WIDTH                   4  /* ADCL_DAC_SVOL - [12:9] */
#define WM8993_ADCR_DAC_SVOL_MASK               0x01E0  /* ADCR_DAC_SVOL - [8:5] */
#define WM8993_ADCR_DAC_SVOL_SHIFT                   5  /* ADCR_DAC_SVOL - [8:5] */
#define WM8993_ADCR_DAC_SVOL_WIDTH                   4  /* ADCR_DAC_SVOL - [8:5] */
#define WM8993_ADC_TO_DACL_MASK                 0x000C  /* ADC_TO_DACL - [3:2] */
#define WM8993_ADC_TO_DACL_SHIFT                     2  /* ADC_TO_DACL - [3:2] */
#define WM8993_ADC_TO_DACL_WIDTH                     2  /* ADC_TO_DACL - [3:2] */
#define WM8993_ADC_TO_DACR_MASK                 0x0003  /* ADC_TO_DACR - [1:0] */
#define WM8993_ADC_TO_DACR_SHIFT                     0  /* ADC_TO_DACR - [1:0] */
#define WM8993_ADC_TO_DACR_WIDTH                     2  /* ADC_TO_DACR - [1:0] */

/*
 * R14 (0x0E) - ADC CTRL
 */
#define WM8993_ADC_OSR128                       0x0200  /* ADC_OSR128 */
#define WM8993_ADC_OSR128_MASK                  0x0200  /* ADC_OSR128 */
#define WM8993_ADC_OSR128_SHIFT                      9  /* ADC_OSR128 */
#define WM8993_ADC_OSR128_WIDTH                      1  /* ADC_OSR128 */
#define WM8993_ADC_HPF                          0x0100  /* ADC_HPF */
#define WM8993_ADC_HPF_MASK                     0x0100  /* ADC_HPF */
#define WM8993_ADC_HPF_SHIFT                         8  /* ADC_HPF */
#define WM8993_ADC_HPF_WIDTH                         1  /* ADC_HPF */
#define WM8993_ADC_HPF_CUT_MASK                 0x0060  /* ADC_HPF_CUT - [6:5] */
#define WM8993_ADC_HPF_CUT_SHIFT                     5  /* ADC_HPF_CUT - [6:5] */
#define WM8993_ADC_HPF_CUT_WIDTH                     2  /* ADC_HPF_CUT - [6:5] */
#define WM8993_ADCL_DATINV                      0x0002  /* ADCL_DATINV */
#define WM8993_ADCL_DATINV_MASK                 0x0002  /* ADCL_DATINV */
#define WM8993_ADCL_DATINV_SHIFT                     1  /* ADCL_DATINV */
#define WM8993_ADCL_DATINV_WIDTH                     1  /* ADCL_DATINV */
#define WM8993_ADCR_DATINV                      0x0001  /* ADCR_DATINV */
#define WM8993_ADCR_DATINV_MASK                 0x0001  /* ADCR_DATINV */
#define WM8993_ADCR_DATINV_SHIFT                     0  /* ADCR_DATINV */
#define WM8993_ADCR_DATINV_WIDTH                     1  /* ADCR_DATINV */

/*
 * R15 (0x0F) - Left ADC Digital Volume
 */
#define WM8993_ADC_VU                           0x0100  /* ADC_VU */
#define WM8993_ADC_VU_MASK                      0x0100  /* ADC_VU */
#define WM8993_ADC_VU_SHIFT                          8  /* ADC_VU */
#define WM8993_ADC_VU_WIDTH                          1  /* ADC_VU */
#define WM8993_ADCL_VOL_MASK                    0x00FF  /* ADCL_VOL - [7:0] */
#define WM8993_ADCL_VOL_SHIFT                        0  /* ADCL_VOL - [7:0] */
#define WM8993_ADCL_VOL_WIDTH                        8  /* ADCL_VOL - [7:0] */

/*
 * R16 (0x10) - Right ADC Digital Volume
 */
#define WM8993_ADC_VU                           0x0100  /* ADC_VU */
#define WM8993_ADC_VU_MASK                      0x0100  /* ADC_VU */
#define WM8993_ADC_VU_SHIFT                          8  /* ADC_VU */
#define WM8993_ADC_VU_WIDTH                          1  /* ADC_VU */
#define WM8993_ADCR_VOL_MASK                    0x00FF  /* ADCR_VOL - [7:0] */
#define WM8993_ADCR_VOL_SHIFT                        0  /* ADCR_VOL - [7:0] */
#define WM8993_ADCR_VOL_WIDTH                        8  /* ADCR_VOL - [7:0] */

/*
 * R18 (0x12) - GPIO CTRL 1
 */
#define WM8993_JD2_SC_EINT                      0x8000  /* JD2_SC_EINT */
#define WM8993_JD2_SC_EINT_MASK                 0x8000  /* JD2_SC_EINT */
#define WM8993_JD2_SC_EINT_SHIFT                    15  /* JD2_SC_EINT */
#define WM8993_JD2_SC_EINT_WIDTH                     1  /* JD2_SC_EINT */
#define WM8993_JD2_EINT                         0x4000  /* JD2_EINT */
#define WM8993_JD2_EINT_MASK                    0x4000  /* JD2_EINT */
#define WM8993_JD2_EINT_SHIFT                       14  /* JD2_EINT */
#define WM8993_JD2_EINT_WIDTH                        1  /* JD2_EINT */
#define WM8993_WSEQ_EINT                        0x2000  /* WSEQ_EINT */
#define WM8993_WSEQ_EINT_MASK                   0x2000  /* WSEQ_EINT */
#define WM8993_WSEQ_EINT_SHIFT                      13  /* WSEQ_EINT */
#define WM8993_WSEQ_EINT_WIDTH                       1  /* WSEQ_EINT */
#define WM8993_IRQ                              0x1000  /* IRQ */
#define WM8993_IRQ_MASK                         0x1000  /* IRQ */
#define WM8993_IRQ_SHIFT                            12  /* IRQ */
#define WM8993_IRQ_WIDTH                             1  /* IRQ */
#define WM8993_TEMPOK_EINT                      0x0800  /* TEMPOK_EINT */
#define WM8993_TEMPOK_EINT_MASK                 0x0800  /* TEMPOK_EINT */
#define WM8993_TEMPOK_EINT_SHIFT                    11  /* TEMPOK_EINT */
#define WM8993_TEMPOK_EINT_WIDTH                     1  /* TEMPOK_EINT */
#define WM8993_JD1_SC_EINT                      0x0400  /* JD1_SC_EINT */
#define WM8993_JD1_SC_EINT_MASK                 0x0400  /* JD1_SC_EINT */
#define WM8993_JD1_SC_EINT_SHIFT                    10  /* JD1_SC_EINT */
#define WM8993_JD1_SC_EINT_WIDTH                     1  /* JD1_SC_EINT */
#define WM8993_JD1_EINT                         0x0200  /* JD1_EINT */
#define WM8993_JD1_EINT_MASK                    0x0200  /* JD1_EINT */
#define WM8993_JD1_EINT_SHIFT                        9  /* JD1_EINT */
#define WM8993_JD1_EINT_WIDTH                        1  /* JD1_EINT */
#define WM8993_FLL_LOCK_EINT                    0x0100  /* FLL_LOCK_EINT */
#define WM8993_FLL_LOCK_EINT_MASK               0x0100  /* FLL_LOCK_EINT */
#define WM8993_FLL_LOCK_EINT_SHIFT                   8  /* FLL_LOCK_EINT */
#define WM8993_FLL_LOCK_EINT_WIDTH                   1  /* FLL_LOCK_EINT */
#define WM8993_GPI8_EINT                        0x0080  /* GPI8_EINT */
#define WM8993_GPI8_EINT_MASK                   0x0080  /* GPI8_EINT */
#define WM8993_GPI8_EINT_SHIFT                       7  /* GPI8_EINT */
#define WM8993_GPI8_EINT_WIDTH                       1  /* GPI8_EINT */
#define WM8993_GPI7_EINT                        0x0040  /* GPI7_EINT */
#define WM8993_GPI7_EINT_MASK                   0x0040  /* GPI7_EINT */
#define WM8993_GPI7_EINT_SHIFT                       6  /* GPI7_EINT */
#define WM8993_GPI7_EINT_WIDTH                       1  /* GPI7_EINT */
#define WM8993_GPIO1_EINT                       0x0001  /* GPIO1_EINT */
#define WM8993_GPIO1_EINT_MASK                  0x0001  /* GPIO1_EINT */
#define WM8993_GPIO1_EINT_SHIFT                      0  /* GPIO1_EINT */
#define WM8993_GPIO1_EINT_WIDTH                      1  /* GPIO1_EINT */

/*
 * R19 (0x13) - GPIO1
 */
#define WM8993_GPIO1_PU                         0x0020  /* GPIO1_PU */
#define WM8993_GPIO1_PU_MASK                    0x0020  /* GPIO1_PU */
#define WM8993_GPIO1_PU_SHIFT                        5  /* GPIO1_PU */
#define WM8993_GPIO1_PU_WIDTH                        1  /* GPIO1_PU */
#define WM8993_GPIO1_PD                         0x0010  /* GPIO1_PD */
#define WM8993_GPIO1_PD_MASK                    0x0010  /* GPIO1_PD */
#define WM8993_GPIO1_PD_SHIFT                        4  /* GPIO1_PD */
#define WM8993_GPIO1_PD_WIDTH                        1  /* GPIO1_PD */
#define WM8993_GPIO1_SEL_MASK                   0x000F  /* GPIO1_SEL - [3:0] */
#define WM8993_GPIO1_SEL_SHIFT                       0  /* GPIO1_SEL - [3:0] */
#define WM8993_GPIO1_SEL_WIDTH                       4  /* GPIO1_SEL - [3:0] */

/*
 * R20 (0x14) - IRQ_DEBOUNCE
 */
#define WM8993_JD2_SC_DB                        0x8000  /* JD2_SC_DB */
#define WM8993_JD2_SC_DB_MASK                   0x8000  /* JD2_SC_DB */
#define WM8993_JD2_SC_DB_SHIFT                      15  /* JD2_SC_DB */
#define WM8993_JD2_SC_DB_WIDTH                       1  /* JD2_SC_DB */
#define WM8993_JD2_DB                           0x4000  /* JD2_DB */
#define WM8993_JD2_DB_MASK                      0x4000  /* JD2_DB */
#define WM8993_JD2_DB_SHIFT                         14  /* JD2_DB */
#define WM8993_JD2_DB_WIDTH                          1  /* JD2_DB */
#define WM8993_WSEQ_DB                          0x2000  /* WSEQ_DB */
#define WM8993_WSEQ_DB_MASK                     0x2000  /* WSEQ_DB */
#define WM8993_WSEQ_DB_SHIFT                        13  /* WSEQ_DB */
#define WM8993_WSEQ_DB_WIDTH                         1  /* WSEQ_DB */
#define WM8993_TEMPOK_DB                        0x0800  /* TEMPOK_DB */
#define WM8993_TEMPOK_DB_MASK                   0x0800  /* TEMPOK_DB */
#define WM8993_TEMPOK_DB_SHIFT                      11  /* TEMPOK_DB */
#define WM8993_TEMPOK_DB_WIDTH                       1  /* TEMPOK_DB */
#define WM8993_JD1_SC_DB                        0x0400  /* JD1_SC_DB */
#define WM8993_JD1_SC_DB_MASK                   0x0400  /* JD1_SC_DB */
#define WM8993_JD1_SC_DB_SHIFT                      10  /* JD1_SC_DB */
#define WM8993_JD1_SC_DB_WIDTH                       1  /* JD1_SC_DB */
#define WM8993_JD1_DB                           0x0200  /* JD1_DB */
#define WM8993_JD1_DB_MASK                      0x0200  /* JD1_DB */
#define WM8993_JD1_DB_SHIFT                          9  /* JD1_DB */
#define WM8993_JD1_DB_WIDTH                          1  /* JD1_DB */
#define WM8993_FLL_LOCK_DB                      0x0100  /* FLL_LOCK_DB */
#define WM8993_FLL_LOCK_DB_MASK                 0x0100  /* FLL_LOCK_DB */
#define WM8993_FLL_LOCK_DB_SHIFT                     8  /* FLL_LOCK_DB */
#define WM8993_FLL_LOCK_DB_WIDTH                     1  /* FLL_LOCK_DB */
#define WM8993_GPI8_DB                          0x0080  /* GPI8_DB */
#define WM8993_GPI8_DB_MASK                     0x0080  /* GPI8_DB */
#define WM8993_GPI8_DB_SHIFT                         7  /* GPI8_DB */
#define WM8993_GPI8_DB_WIDTH                         1  /* GPI8_DB */
#define WM8993_GPI7_DB                          0x0008  /* GPI7_DB */
#define WM8993_GPI7_DB_MASK                     0x0008  /* GPI7_DB */
#define WM8993_GPI7_DB_SHIFT                         3  /* GPI7_DB */
#define WM8993_GPI7_DB_WIDTH                         1  /* GPI7_DB */
#define WM8993_GPIO1_DB                         0x0001  /* GPIO1_DB */
#define WM8993_GPIO1_DB_MASK                    0x0001  /* GPIO1_DB */
#define WM8993_GPIO1_DB_SHIFT                        0  /* GPIO1_DB */
#define WM8993_GPIO1_DB_WIDTH                        1  /* GPIO1_DB */

/*
 * R22 (0x16) - GPIOCTRL 2
 */
#define WM8993_IM_JD2_EINT                      0x2000  /* IM_JD2_EINT */
#define WM8993_IM_JD2_EINT_MASK                 0x2000  /* IM_JD2_EINT */
#define WM8993_IM_JD2_EINT_SHIFT                    13  /* IM_JD2_EINT */
#define WM8993_IM_JD2_EINT_WIDTH                     1  /* IM_JD2_EINT */
#define WM8993_IM_JD2_SC_EINT                   0x1000  /* IM_JD2_SC_EINT */
#define WM8993_IM_JD2_SC_EINT_MASK              0x1000  /* IM_JD2_SC_EINT */
#define WM8993_IM_JD2_SC_EINT_SHIFT                 12  /* IM_JD2_SC_EINT */
#define WM8993_IM_JD2_SC_EINT_WIDTH                  1  /* IM_JD2_SC_EINT */
#define WM8993_IM_TEMPOK_EINT                   0x0800  /* IM_TEMPOK_EINT */
#define WM8993_IM_TEMPOK_EINT_MASK              0x0800  /* IM_TEMPOK_EINT */
#define WM8993_IM_TEMPOK_EINT_SHIFT                 11  /* IM_TEMPOK_EINT */
#define WM8993_IM_TEMPOK_EINT_WIDTH                  1  /* IM_TEMPOK_EINT */
#define WM8993_IM_JD1_SC_EINT                   0x0400  /* IM_JD1_SC_EINT */
#define WM8993_IM_JD1_SC_EINT_MASK              0x0400  /* IM_JD1_SC_EINT */
#define WM8993_IM_JD1_SC_EINT_SHIFT                 10  /* IM_JD1_SC_EINT */
#define WM8993_IM_JD1_SC_EINT_WIDTH                  1  /* IM_JD1_SC_EINT */
#define WM8993_IM_JD1_EINT                      0x0200  /* IM_JD1_EINT */
#define WM8993_IM_JD1_EINT_MASK                 0x0200  /* IM_JD1_EINT */
#define WM8993_IM_JD1_EINT_SHIFT                     9  /* IM_JD1_EINT */
#define WM8993_IM_JD1_EINT_WIDTH                     1  /* IM_JD1_EINT */
#define WM8993_IM_FLL_LOCK_EINT                 0x0100  /* IM_FLL_LOCK_EINT */
#define WM8993_IM_FLL_LOCK_EINT_MASK            0x0100  /* IM_FLL_LOCK_EINT */
#define WM8993_IM_FLL_LOCK_EINT_SHIFT                8  /* IM_FLL_LOCK_EINT */
#define WM8993_IM_FLL_LOCK_EINT_WIDTH                1  /* IM_FLL_LOCK_EINT */
#define WM8993_IM_GPI8_EINT                     0x0040  /* IM_GPI8_EINT */
#define WM8993_IM_GPI8_EINT_MASK                0x0040  /* IM_GPI8_EINT */
#define WM8993_IM_GPI8_EINT_SHIFT                    6  /* IM_GPI8_EINT */
#define WM8993_IM_GPI8_EINT_WIDTH                    1  /* IM_GPI8_EINT */
#define WM8993_IM_GPIO1_EINT                    0x0020  /* IM_GPIO1_EINT */
#define WM8993_IM_GPIO1_EINT_MASK               0x0020  /* IM_GPIO1_EINT */
#define WM8993_IM_GPIO1_EINT_SHIFT                   5  /* IM_GPIO1_EINT */
#define WM8993_IM_GPIO1_EINT_WIDTH                   1  /* IM_GPIO1_EINT */
#define WM8993_GPI8_ENA                         0x0010  /* GPI8_ENA */
#define WM8993_GPI8_ENA_MASK                    0x0010  /* GPI8_ENA */
#define WM8993_GPI8_ENA_SHIFT                        4  /* GPI8_ENA */
#define WM8993_GPI8_ENA_WIDTH                        1  /* GPI8_ENA */
#define WM8993_IM_GPI7_EINT                     0x0004  /* IM_GPI7_EINT */
#define WM8993_IM_GPI7_EINT_MASK                0x0004  /* IM_GPI7_EINT */
#define WM8993_IM_GPI7_EINT_SHIFT                    2  /* IM_GPI7_EINT */
#define WM8993_IM_GPI7_EINT_WIDTH                    1  /* IM_GPI7_EINT */
#define WM8993_IM_WSEQ_EINT                     0x0002  /* IM_WSEQ_EINT */
#define WM8993_IM_WSEQ_EINT_MASK                0x0002  /* IM_WSEQ_EINT */
#define WM8993_IM_WSEQ_EINT_SHIFT                    1  /* IM_WSEQ_EINT */
#define WM8993_IM_WSEQ_EINT_WIDTH                    1  /* IM_WSEQ_EINT */
#define WM8993_GPI7_ENA                         0x0001  /* GPI7_ENA */
#define WM8993_GPI7_ENA_MASK                    0x0001  /* GPI7_ENA */
#define WM8993_GPI7_ENA_SHIFT                        0  /* GPI7_ENA */
#define WM8993_GPI7_ENA_WIDTH                        1  /* GPI7_ENA */

/*
 * R23 (0x17) - GPIO_POL
 */
#define WM8993_JD2_SC_POL                       0x8000  /* JD2_SC_POL */
#define WM8993_JD2_SC_POL_MASK                  0x8000  /* JD2_SC_POL */
#define WM8993_JD2_SC_POL_SHIFT                     15  /* JD2_SC_POL */
#define WM8993_JD2_SC_POL_WIDTH                      1  /* JD2_SC_POL */
#define WM8993_JD2_POL                          0x4000  /* JD2_POL */
#define WM8993_JD2_POL_MASK                     0x4000  /* JD2_POL */
#define WM8993_JD2_POL_SHIFT                        14  /* JD2_POL */
#define WM8993_JD2_POL_WIDTH                         1  /* JD2_POL */
#define WM8993_WSEQ_POL                         0x2000  /* WSEQ_POL */
#define WM8993_WSEQ_POL_MASK                    0x2000  /* WSEQ_POL */
#define WM8993_WSEQ_POL_SHIFT                       13  /* WSEQ_POL */
#define WM8993_WSEQ_POL_WIDTH                        1  /* WSEQ_POL */
#define WM8993_IRQ_POL                          0x1000  /* IRQ_POL */
#define WM8993_IRQ_POL_MASK                     0x1000  /* IRQ_POL */
#define WM8993_IRQ_POL_SHIFT                        12  /* IRQ_POL */
#define WM8993_IRQ_POL_WIDTH                         1  /* IRQ_POL */
#define WM8993_TEMPOK_POL                       0x0800  /* TEMPOK_POL */
#define WM8993_TEMPOK_POL_MASK                  0x0800  /* TEMPOK_POL */
#define WM8993_TEMPOK_POL_SHIFT                     11  /* TEMPOK_POL */
#define WM8993_TEMPOK_POL_WIDTH                      1  /* TEMPOK_POL */
#define WM8993_JD1_SC_POL                       0x0400  /* JD1_SC_POL */
#define WM8993_JD1_SC_POL_MASK                  0x0400  /* JD1_SC_POL */
#define WM8993_JD1_SC_POL_SHIFT                     10  /* JD1_SC_POL */
#define WM8993_JD1_SC_POL_WIDTH                      1  /* JD1_SC_POL */
#define WM8993_JD1_POL                          0x0200  /* JD1_POL */
#define WM8993_JD1_POL_MASK                     0x0200  /* JD1_POL */
#define WM8993_JD1_POL_SHIFT                         9  /* JD1_POL */
#define WM8993_JD1_POL_WIDTH                         1  /* JD1_POL */
#define WM8993_FLL_LOCK_POL                     0x0100  /* FLL_LOCK_POL */
#define WM8993_FLL_LOCK_POL_MASK                0x0100  /* FLL_LOCK_POL */
#define WM8993_FLL_LOCK_POL_SHIFT                    8  /* FLL_LOCK_POL */
#define WM8993_FLL_LOCK_POL_WIDTH                    1  /* FLL_LOCK_POL */
#define WM8993_GPI8_POL                         0x0080  /* GPI8_POL */
#define WM8993_GPI8_POL_MASK                    0x0080  /* GPI8_POL */
#define WM8993_GPI8_POL_SHIFT                        7  /* GPI8_POL */
#define WM8993_GPI8_POL_WIDTH                        1  /* GPI8_POL */
#define WM8993_GPI7_POL                         0x0040  /* GPI7_POL */
#define WM8993_GPI7_POL_MASK                    0x0040  /* GPI7_POL */
#define WM8993_GPI7_POL_SHIFT                        6  /* GPI7_POL */
#define WM8993_GPI7_POL_WIDTH                        1  /* GPI7_POL */
#define WM8993_GPIO1_POL                        0x0001  /* GPIO1_POL */
#define WM8993_GPIO1_POL_MASK                   0x0001  /* GPIO1_POL */
#define WM8993_GPIO1_POL_SHIFT                       0  /* GPIO1_POL */
#define WM8993_GPIO1_POL_WIDTH                       1  /* GPIO1_POL */

/*
 * R24 (0x18) - Left Line Input 1&2 Volume
 */
#define WM8993_IN1_VU                           0x0100  /* IN1_VU */
#define WM8993_IN1_VU_MASK                      0x0100  /* IN1_VU */
#define WM8993_IN1_VU_SHIFT                          8  /* IN1_VU */
#define WM8993_IN1_VU_WIDTH                          1  /* IN1_VU */
#define WM8993_IN1L_MUTE                        0x0080  /* IN1L_MUTE */
#define WM8993_IN1L_MUTE_MASK                   0x0080  /* IN1L_MUTE */
#define WM8993_IN1L_MUTE_SHIFT                       7  /* IN1L_MUTE */
#define WM8993_IN1L_MUTE_WIDTH                       1  /* IN1L_MUTE */
#define WM8993_IN1L_ZC                          0x0040  /* IN1L_ZC */
#define WM8993_IN1L_ZC_MASK                     0x0040  /* IN1L_ZC */
#define WM8993_IN1L_ZC_SHIFT                         6  /* IN1L_ZC */
#define WM8993_IN1L_ZC_WIDTH                         1  /* IN1L_ZC */
#define WM8993_IN1L_VOL_MASK                    0x001F  /* IN1L_VOL - [4:0] */
#define WM8993_IN1L_VOL_SHIFT                        0  /* IN1L_VOL - [4:0] */
#define WM8993_IN1L_VOL_WIDTH                        5  /* IN1L_VOL - [4:0] */

/*
 * R25 (0x19) - Left Line Input 3&4 Volume
 */
#define WM8993_IN2_VU                           0x0100  /* IN2_VU */
#define WM8993_IN2_VU_MASK                      0x0100  /* IN2_VU */
#define WM8993_IN2_VU_SHIFT                          8  /* IN2_VU */
#define WM8993_IN2_VU_WIDTH                          1  /* IN2_VU */
#define WM8993_IN2L_MUTE                        0x0080  /* IN2L_MUTE */
#define WM8993_IN2L_MUTE_MASK                   0x0080  /* IN2L_MUTE */
#define WM8993_IN2L_MUTE_SHIFT                       7  /* IN2L_MUTE */
#define WM8993_IN2L_MUTE_WIDTH                       1  /* IN2L_MUTE */
#define WM8993_IN2L_ZC                          0x0040  /* IN2L_ZC */
#define WM8993_IN2L_ZC_MASK                     0x0040  /* IN2L_ZC */
#define WM8993_IN2L_ZC_SHIFT                         6  /* IN2L_ZC */
#define WM8993_IN2L_ZC_WIDTH                         1  /* IN2L_ZC */
#define WM8993_IN2L_VOL_MASK                    0x001F  /* IN2L_VOL - [4:0] */
#define WM8993_IN2L_VOL_SHIFT                        0  /* IN2L_VOL - [4:0] */
#define WM8993_IN2L_VOL_WIDTH                        5  /* IN2L_VOL - [4:0] */

/*
 * R26 (0x1A) - Right Line Input 1&2 Volume
 */
#define WM8993_IN1_VU                           0x0100  /* IN1_VU */
#define WM8993_IN1_VU_MASK                      0x0100  /* IN1_VU */
#define WM8993_IN1_VU_SHIFT                          8  /* IN1_VU */
#define WM8993_IN1_VU_WIDTH                          1  /* IN1_VU */
#define WM8993_IN1R_MUTE                        0x0080  /* IN1R_MUTE */
#define WM8993_IN1R_MUTE_MASK                   0x0080  /* IN1R_MUTE */
#define WM8993_IN1R_MUTE_SHIFT                       7  /* IN1R_MUTE */
#define WM8993_IN1R_MUTE_WIDTH                       1  /* IN1R_MUTE */
#define WM8993_IN1R_ZC                          0x0040  /* IN1R_ZC */
#define WM8993_IN1R_ZC_MASK                     0x0040  /* IN1R_ZC */
#define WM8993_IN1R_ZC_SHIFT                         6  /* IN1R_ZC */
#define WM8993_IN1R_ZC_WIDTH                         1  /* IN1R_ZC */
#define WM8993_IN1R_VOL_MASK                    0x001F  /* IN1R_VOL - [4:0] */
#define WM8993_IN1R_VOL_SHIFT                        0  /* IN1R_VOL - [4:0] */
#define WM8993_IN1R_VOL_WIDTH                        5  /* IN1R_VOL - [4:0] */

/*
 * R27 (0x1B) - Right Line Input 3&4 Volume
 */
#define WM8993_IN2_VU                           0x0100  /* IN2_VU */
#define WM8993_IN2_VU_MASK                      0x0100  /* IN2_VU */
#define WM8993_IN2_VU_SHIFT                          8  /* IN2_VU */
#define WM8993_IN2_VU_WIDTH                          1  /* IN2_VU */
#define WM8993_IN2R_MUTE                        0x0080  /* IN2R_MUTE */
#define WM8993_IN2R_MUTE_MASK                   0x0080  /* IN2R_MUTE */
#define WM8993_IN2R_MUTE_SHIFT                       7  /* IN2R_MUTE */
#define WM8993_IN2R_MUTE_WIDTH                       1  /* IN2R_MUTE */
#define WM8993_IN2R_ZC                          0x0040  /* IN2R_ZC */
#define WM8993_IN2R_ZC_MASK                     0x0040  /* IN2R_ZC */
#define WM8993_IN2R_ZC_SHIFT                         6  /* IN2R_ZC */
#define WM8993_IN2R_ZC_WIDTH                         1  /* IN2R_ZC */
#define WM8993_IN2R_VOL_MASK                    0x001F  /* IN2R_VOL - [4:0] */
#define WM8993_IN2R_VOL_SHIFT                        0  /* IN2R_VOL - [4:0] */
#define WM8993_IN2R_VOL_WIDTH                        5  /* IN2R_VOL - [4:0] */

/*
 * R28 (0x1C) - Left Output Volume
 */
#define WM8993_HPOUT1_VU                        0x0100  /* HPOUT1_VU */
#define WM8993_HPOUT1_VU_MASK                   0x0100  /* HPOUT1_VU */
#define WM8993_HPOUT1_VU_SHIFT                       8  /* HPOUT1_VU */
#define WM8993_HPOUT1_VU_WIDTH                       1  /* HPOUT1_VU */
#define WM8993_HPOUT1L_ZC                       0x0080  /* HPOUT1L_ZC */
#define WM8993_HPOUT1L_ZC_MASK                  0x0080  /* HPOUT1L_ZC */
#define WM8993_HPOUT1L_ZC_SHIFT                      7  /* HPOUT1L_ZC */
#define WM8993_HPOUT1L_ZC_WIDTH                      1  /* HPOUT1L_ZC */
#define WM8993_HPOUT1L_MUTE_N                   0x0040  /* HPOUT1L_MUTE_N */
#define WM8993_HPOUT1L_MUTE_N_MASK              0x0040  /* HPOUT1L_MUTE_N */
#define WM8993_HPOUT1L_MUTE_N_SHIFT                  6  /* HPOUT1L_MUTE_N */
#define WM8993_HPOUT1L_MUTE_N_WIDTH                  1  /* HPOUT1L_MUTE_N */
#define WM8993_HPOUT1L_VOL_MASK                 0x003F  /* HPOUT1L_VOL - [5:0] */
#define WM8993_HPOUT1L_VOL_SHIFT                     0  /* HPOUT1L_VOL - [5:0] */
#define WM8993_HPOUT1L_VOL_WIDTH                     6  /* HPOUT1L_VOL - [5:0] */

/*
 * R29 (0x1D) - Right Output Volume
 */
#define WM8993_HPOUT1_VU                        0x0100  /* HPOUT1_VU */
#define WM8993_HPOUT1_VU_MASK                   0x0100  /* HPOUT1_VU */
#define WM8993_HPOUT1_VU_SHIFT                       8  /* HPOUT1_VU */
#define WM8993_HPOUT1_VU_WIDTH                       1  /* HPOUT1_VU */
#define WM8993_HPOUT1R_ZC                       0x0080  /* HPOUT1R_ZC */
#define WM8993_HPOUT1R_ZC_MASK                  0x0080  /* HPOUT1R_ZC */
#define WM8993_HPOUT1R_ZC_SHIFT                      7  /* HPOUT1R_ZC */
#define WM8993_HPOUT1R_ZC_WIDTH                      1  /* HPOUT1R_ZC */
#define WM8993_HPOUT1R_MUTE_N                   0x0040  /* HPOUT1R_MUTE_N */
#define WM8993_HPOUT1R_MUTE_N_MASK              0x0040  /* HPOUT1R_MUTE_N */
#define WM8993_HPOUT1R_MUTE_N_SHIFT                  6  /* HPOUT1R_MUTE_N */
#define WM8993_HPOUT1R_MUTE_N_WIDTH                  1  /* HPOUT1R_MUTE_N */
#define WM8993_HPOUT1R_VOL_MASK                 0x003F  /* HPOUT1R_VOL - [5:0] */
#define WM8993_HPOUT1R_VOL_SHIFT                     0  /* HPOUT1R_VOL - [5:0] */
#define WM8993_HPOUT1R_VOL_WIDTH                     6  /* HPOUT1R_VOL - [5:0] */

/*
 * R30 (0x1E) - Line Outputs Volume
 */
#define WM8993_LINEOUT1N_MUTE                   0x0040  /* LINEOUT1N_MUTE */
#define WM8993_LINEOUT1N_MUTE_MASK              0x0040  /* LINEOUT1N_MUTE */
#define WM8993_LINEOUT1N_MUTE_SHIFT                  6  /* LINEOUT1N_MUTE */
#define WM8993_LINEOUT1N_MUTE_WIDTH                  1  /* LINEOUT1N_MUTE */
#define WM8993_LINEOUT1P_MUTE                   0x0020  /* LINEOUT1P_MUTE */
#define WM8993_LINEOUT1P_MUTE_MASK              0x0020  /* LINEOUT1P_MUTE */
#define WM8993_LINEOUT1P_MUTE_SHIFT                  5  /* LINEOUT1P_MUTE */
#define WM8993_LINEOUT1P_MUTE_WIDTH                  1  /* LINEOUT1P_MUTE */
#define WM8993_LINEOUT1_VOL                     0x0010  /* LINEOUT1_VOL */
#define WM8993_LINEOUT1_VOL_MASK                0x0010  /* LINEOUT1_VOL */
#define WM8993_LINEOUT1_VOL_SHIFT                    4  /* LINEOUT1_VOL */
#define WM8993_LINEOUT1_VOL_WIDTH                    1  /* LINEOUT1_VOL */
#define WM8993_LINEOUT2N_MUTE                   0x0004  /* LINEOUT2N_MUTE */
#define WM8993_LINEOUT2N_MUTE_MASK              0x0004  /* LINEOUT2N_MUTE */
#define WM8993_LINEOUT2N_MUTE_SHIFT                  2  /* LINEOUT2N_MUTE */
#define WM8993_LINEOUT2N_MUTE_WIDTH                  1  /* LINEOUT2N_MUTE */
#define WM8993_LINEOUT2P_MUTE                   0x0002  /* LINEOUT2P_MUTE */
#define WM8993_LINEOUT2P_MUTE_MASK              0x0002  /* LINEOUT2P_MUTE */
#define WM8993_LINEOUT2P_MUTE_SHIFT                  1  /* LINEOUT2P_MUTE */
#define WM8993_LINEOUT2P_MUTE_WIDTH                  1  /* LINEOUT2P_MUTE */
#define WM8993_LINEOUT2_VOL                     0x0001  /* LINEOUT2_VOL */
#define WM8993_LINEOUT2_VOL_MASK                0x0001  /* LINEOUT2_VOL */
#define WM8993_LINEOUT2_VOL_SHIFT                    0  /* LINEOUT2_VOL */
#define WM8993_LINEOUT2_VOL_WIDTH                    1  /* LINEOUT2_VOL */

/*
 * R31 (0x1F) - HPOUT2 Volume
 */
#define WM8993_HPOUT2_MUTE                      0x0020  /* HPOUT2_MUTE */
#define WM8993_HPOUT2_MUTE_MASK                 0x0020  /* HPOUT2_MUTE */
#define WM8993_HPOUT2_MUTE_SHIFT                     5  /* HPOUT2_MUTE */
#define WM8993_HPOUT2_MUTE_WIDTH                     1  /* HPOUT2_MUTE */
#define WM8993_HPOUT2_VOL                       0x0010  /* HPOUT2_VOL */
#define WM8993_HPOUT2_VOL_MASK                  0x0010  /* HPOUT2_VOL */
#define WM8993_HPOUT2_VOL_SHIFT                      4  /* HPOUT2_VOL */
#define WM8993_HPOUT2_VOL_WIDTH                      1  /* HPOUT2_VOL */

/*
 * R32 (0x20) - Left OPGA Volume
 */
#define WM8993_MIXOUT_VU                        0x0100  /* MIXOUT_VU */
#define WM8993_MIXOUT_VU_MASK                   0x0100  /* MIXOUT_VU */
#define WM8993_MIXOUT_VU_SHIFT                       8  /* MIXOUT_VU */
#define WM8993_MIXOUT_VU_WIDTH                       1  /* MIXOUT_VU */
#define WM8993_MIXOUTL_ZC                       0x0080  /* MIXOUTL_ZC */
#define WM8993_MIXOUTL_ZC_MASK                  0x0080  /* MIXOUTL_ZC */
#define WM8993_MIXOUTL_ZC_SHIFT                      7  /* MIXOUTL_ZC */
#define WM8993_MIXOUTL_ZC_WIDTH                      1  /* MIXOUTL_ZC */
#define WM8993_MIXOUTL_MUTE_N                   0x0040  /* MIXOUTL_MUTE_N */
#define WM8993_MIXOUTL_MUTE_N_MASK              0x0040  /* MIXOUTL_MUTE_N */
#define WM8993_MIXOUTL_MUTE_N_SHIFT                  6  /* MIXOUTL_MUTE_N */
#define WM8993_MIXOUTL_MUTE_N_WIDTH                  1  /* MIXOUTL_MUTE_N */
#define WM8993_MIXOUTL_VOL_MASK                 0x003F  /* MIXOUTL_VOL - [5:0] */
#define WM8993_MIXOUTL_VOL_SHIFT                     0  /* MIXOUTL_VOL - [5:0] */
#define WM8993_MIXOUTL_VOL_WIDTH                     6  /* MIXOUTL_VOL - [5:0] */

/*
 * R33 (0x21) - Right OPGA Volume
 */
#define WM8993_MIXOUT_VU                        0x0100  /* MIXOUT_VU */
#define WM8993_MIXOUT_VU_MASK                   0x0100  /* MIXOUT_VU */
#define WM8993_MIXOUT_VU_SHIFT                       8  /* MIXOUT_VU */
#define WM8993_MIXOUT_VU_WIDTH                       1  /* MIXOUT_VU */
#define WM8993_MIXOUTR_ZC                       0x0080  /* MIXOUTR_ZC */
#define WM8993_MIXOUTR_ZC_MASK                  0x0080  /* MIXOUTR_ZC */
#define WM8993_MIXOUTR_ZC_SHIFT                      7  /* MIXOUTR_ZC */
#define WM8993_MIXOUTR_ZC_WIDTH                      1  /* MIXOUTR_ZC */
#define WM8993_MIXOUTR_MUTE_N                   0x0040  /* MIXOUTR_MUTE_N */
#define WM8993_MIXOUTR_MUTE_N_MASK              0x0040  /* MIXOUTR_MUTE_N */
#define WM8993_MIXOUTR_MUTE_N_SHIFT                  6  /* MIXOUTR_MUTE_N */
#define WM8993_MIXOUTR_MUTE_N_WIDTH                  1  /* MIXOUTR_MUTE_N */
#define WM8993_MIXOUTR_VOL_MASK                 0x003F  /* MIXOUTR_VOL - [5:0] */
#define WM8993_MIXOUTR_VOL_SHIFT                     0  /* MIXOUTR_VOL - [5:0] */
#define WM8993_MIXOUTR_VOL_WIDTH                     6  /* MIXOUTR_VOL - [5:0] */

/*
 * R34 (0x22) - SPKMIXL Attenuation
 */
#define WM8993_MIXINL_SPKMIXL_VOL               0x0020  /* MIXINL_SPKMIXL_VOL */
#define WM8993_MIXINL_SPKMIXL_VOL_MASK          0x0020  /* MIXINL_SPKMIXL_VOL */
#define WM8993_MIXINL_SPKMIXL_VOL_SHIFT              5  /* MIXINL_SPKMIXL_VOL */
#define WM8993_MIXINL_SPKMIXL_VOL_WIDTH              1  /* MIXINL_SPKMIXL_VOL */
#define WM8993_IN1LP_SPKMIXL_VOL                0x0010  /* IN1LP_SPKMIXL_VOL */
#define WM8993_IN1LP_SPKMIXL_VOL_MASK           0x0010  /* IN1LP_SPKMIXL_VOL */
#define WM8993_IN1LP_SPKMIXL_VOL_SHIFT               4  /* IN1LP_SPKMIXL_VOL */
#define WM8993_IN1LP_SPKMIXL_VOL_WIDTH               1  /* IN1LP_SPKMIXL_VOL */
#define WM8993_MIXOUTL_SPKMIXL_VOL              0x0008  /* MIXOUTL_SPKMIXL_VOL */
#define WM8993_MIXOUTL_SPKMIXL_VOL_MASK         0x0008  /* MIXOUTL_SPKMIXL_VOL */
#define WM8993_MIXOUTL_SPKMIXL_VOL_SHIFT             3  /* MIXOUTL_SPKMIXL_VOL */
#define WM8993_MIXOUTL_SPKMIXL_VOL_WIDTH             1  /* MIXOUTL_SPKMIXL_VOL */
#define WM8993_DACL_SPKMIXL_VOL                 0x0004  /* DACL_SPKMIXL_VOL */
#define WM8993_DACL_SPKMIXL_VOL_MASK            0x0004  /* DACL_SPKMIXL_VOL */
#define WM8993_DACL_SPKMIXL_VOL_SHIFT                2  /* DACL_SPKMIXL_VOL */
#define WM8993_DACL_SPKMIXL_VOL_WIDTH                1  /* DACL_SPKMIXL_VOL */
#define WM8993_SPKMIXL_VOL_MASK                 0x0003  /* SPKMIXL_VOL - [1:0] */
#define WM8993_SPKMIXL_VOL_SHIFT                     0  /* SPKMIXL_VOL - [1:0] */
#define WM8993_SPKMIXL_VOL_WIDTH                     2  /* SPKMIXL_VOL - [1:0] */

/*
 * R35 (0x23) - SPKMIXR Attenuation
 */
#define WM8993_SPKOUT_CLASSAB_MODE              0x0100  /* SPKOUT_CLASSAB_MODE */
#define WM8993_SPKOUT_CLASSAB_MODE_MASK         0x0100  /* SPKOUT_CLASSAB_MODE */
#define WM8993_SPKOUT_CLASSAB_MODE_SHIFT             8  /* SPKOUT_CLASSAB_MODE */
#define WM8993_SPKOUT_CLASSAB_MODE_WIDTH             1  /* SPKOUT_CLASSAB_MODE */
#define WM8993_MIXINR_SPKMIXR_VOL               0x0020  /* MIXINR_SPKMIXR_VOL */
#define WM8993_MIXINR_SPKMIXR_VOL_MASK          0x0020  /* MIXINR_SPKMIXR_VOL */
#define WM8993_MIXINR_SPKMIXR_VOL_SHIFT              5  /* MIXINR_SPKMIXR_VOL */
#define WM8993_MIXINR_SPKMIXR_VOL_WIDTH              1  /* MIXINR_SPKMIXR_VOL */
#define WM8993_IN1RP_SPKMIXR_VOL                0x0010  /* IN1RP_SPKMIXR_VOL */
#define WM8993_IN1RP_SPKMIXR_VOL_MASK           0x0010  /* IN1RP_SPKMIXR_VOL */
#define WM8993_IN1RP_SPKMIXR_VOL_SHIFT               4  /* IN1RP_SPKMIXR_VOL */
#define WM8993_IN1RP_SPKMIXR_VOL_WIDTH               1  /* IN1RP_SPKMIXR_VOL */
#define WM8993_MIXOUTR_SPKMIXR_VOL              0x0008  /* MIXOUTR_SPKMIXR_VOL */
#define WM8993_MIXOUTR_SPKMIXR_VOL_MASK         0x0008  /* MIXOUTR_SPKMIXR_VOL */
#define WM8993_MIXOUTR_SPKMIXR_VOL_SHIFT             3  /* MIXOUTR_SPKMIXR_VOL */
#define WM8993_MIXOUTR_SPKMIXR_VOL_WIDTH             1  /* MIXOUTR_SPKMIXR_VOL */
#define WM8993_DACR_SPKMIXR_VOL                 0x0004  /* DACR_SPKMIXR_VOL */
#define WM8993_DACR_SPKMIXR_VOL_MASK            0x0004  /* DACR_SPKMIXR_VOL */
#define WM8993_DACR_SPKMIXR_VOL_SHIFT                2  /* DACR_SPKMIXR_VOL */
#define WM8993_DACR_SPKMIXR_VOL_WIDTH                1  /* DACR_SPKMIXR_VOL */
#define WM8993_SPKMIXR_VOL_MASK                 0x0003  /* SPKMIXR_VOL - [1:0] */
#define WM8993_SPKMIXR_VOL_SHIFT                     0  /* SPKMIXR_VOL - [1:0] */
#define WM8993_SPKMIXR_VOL_WIDTH                     2  /* SPKMIXR_VOL - [1:0] */

/*
 * R36 (0x24) - SPKOUT Mixers
 */
#define WM8993_VRX_TO_SPKOUTL                   0x0020  /* VRX_TO_SPKOUTL */
#define WM8993_VRX_TO_SPKOUTL_MASK              0x0020  /* VRX_TO_SPKOUTL */
#define WM8993_VRX_TO_SPKOUTL_SHIFT                  5  /* VRX_TO_SPKOUTL */
#define WM8993_VRX_TO_SPKOUTL_WIDTH                  1  /* VRX_TO_SPKOUTL */
#define WM8993_SPKMIXL_TO_SPKOUTL               0x0010  /* SPKMIXL_TO_SPKOUTL */
#define WM8993_SPKMIXL_TO_SPKOUTL_MASK          0x0010  /* SPKMIXL_TO_SPKOUTL */
#define WM8993_SPKMIXL_TO_SPKOUTL_SHIFT              4  /* SPKMIXL_TO_SPKOUTL */
#define WM8993_SPKMIXL_TO_SPKOUTL_WIDTH              1  /* SPKMIXL_TO_SPKOUTL */
#define WM8993_SPKMIXR_TO_SPKOUTL               0x0008  /* SPKMIXR_TO_SPKOUTL */
#define WM8993_SPKMIXR_TO_SPKOUTL_MASK          0x0008  /* SPKMIXR_TO_SPKOUTL */
#define WM8993_SPKMIXR_TO_SPKOUTL_SHIFT              3  /* SPKMIXR_TO_SPKOUTL */
#define WM8993_SPKMIXR_TO_SPKOUTL_WIDTH              1  /* SPKMIXR_TO_SPKOUTL */
#define WM8993_VRX_TO_SPKOUTR                   0x0004  /* VRX_TO_SPKOUTR */
#define WM8993_VRX_TO_SPKOUTR_MASK              0x0004  /* VRX_TO_SPKOUTR */
#define WM8993_VRX_TO_SPKOUTR_SHIFT                  2  /* VRX_TO_SPKOUTR */
#define WM8993_VRX_TO_SPKOUTR_WIDTH                  1  /* VRX_TO_SPKOUTR */
#define WM8993_SPKMIXL_TO_SPKOUTR               0x0002  /* SPKMIXL_TO_SPKOUTR */
#define WM8993_SPKMIXL_TO_SPKOUTR_MASK          0x0002  /* SPKMIXL_TO_SPKOUTR */
#define WM8993_SPKMIXL_TO_SPKOUTR_SHIFT              1  /* SPKMIXL_TO_SPKOUTR */
#define WM8993_SPKMIXL_TO_SPKOUTR_WIDTH              1  /* SPKMIXL_TO_SPKOUTR */
#define WM8993_SPKMIXR_TO_SPKOUTR               0x0001  /* SPKMIXR_TO_SPKOUTR */
#define WM8993_SPKMIXR_TO_SPKOUTR_MASK          0x0001  /* SPKMIXR_TO_SPKOUTR */
#define WM8993_SPKMIXR_TO_SPKOUTR_SHIFT              0  /* SPKMIXR_TO_SPKOUTR */
#define WM8993_SPKMIXR_TO_SPKOUTR_WIDTH              1  /* SPKMIXR_TO_SPKOUTR */

/*
 * R37 (0x25) - SPKOUT Boost
 */
#define WM8993_SPKOUTL_BOOST_MASK               0x0038  /* SPKOUTL_BOOST - [5:3] */
#define WM8993_SPKOUTL_BOOST_SHIFT                   3  /* SPKOUTL_BOOST - [5:3] */
#define WM8993_SPKOUTL_BOOST_WIDTH                   3  /* SPKOUTL_BOOST - [5:3] */
#define WM8993_SPKOUTR_BOOST_MASK               0x0007  /* SPKOUTR_BOOST - [2:0] */
#define WM8993_SPKOUTR_BOOST_SHIFT                   0  /* SPKOUTR_BOOST - [2:0] */
#define WM8993_SPKOUTR_BOOST_WIDTH                   3  /* SPKOUTR_BOOST - [2:0] */

/*
 * R38 (0x26) - Speaker Volume Left
 */
#define WM8993_SPKOUT_VU                        0x0100  /* SPKOUT_VU */
#define WM8993_SPKOUT_VU_MASK                   0x0100  /* SPKOUT_VU */
#define WM8993_SPKOUT_VU_SHIFT                       8  /* SPKOUT_VU */
#define WM8993_SPKOUT_VU_WIDTH                       1  /* SPKOUT_VU */
#define WM8993_SPKOUTL_ZC                       0x0080  /* SPKOUTL_ZC */
#define WM8993_SPKOUTL_ZC_MASK                  0x0080  /* SPKOUTL_ZC */
#define WM8993_SPKOUTL_ZC_SHIFT                      7  /* SPKOUTL_ZC */
#define WM8993_SPKOUTL_ZC_WIDTH                      1  /* SPKOUTL_ZC */
#define WM8993_SPKOUTL_MUTE_N                   0x0040  /* SPKOUTL_MUTE_N */
#define WM8993_SPKOUTL_MUTE_N_MASK              0x0040  /* SPKOUTL_MUTE_N */
#define WM8993_SPKOUTL_MUTE_N_SHIFT                  6  /* SPKOUTL_MUTE_N */
#define WM8993_SPKOUTL_MUTE_N_WIDTH                  1  /* SPKOUTL_MUTE_N */
#define WM8993_SPKOUTL_VOL_MASK                 0x003F  /* SPKOUTL_VOL - [5:0] */
#define WM8993_SPKOUTL_VOL_SHIFT                     0  /* SPKOUTL_VOL - [5:0] */
#define WM8993_SPKOUTL_VOL_WIDTH                     6  /* SPKOUTL_VOL - [5:0] */

/*
 * R39 (0x27) - Speaker Volume Right
 */
#define WM8993_SPKOUT_VU                        0x0100  /* SPKOUT_VU */
#define WM8993_SPKOUT_VU_MASK                   0x0100  /* SPKOUT_VU */
#define WM8993_SPKOUT_VU_SHIFT                       8  /* SPKOUT_VU */
#define WM8993_SPKOUT_VU_WIDTH                       1  /* SPKOUT_VU */
#define WM8993_SPKOUTR_ZC                       0x0080  /* SPKOUTR_ZC */
#define WM8993_SPKOUTR_ZC_MASK                  0x0080  /* SPKOUTR_ZC */
#define WM8993_SPKOUTR_ZC_SHIFT                      7  /* SPKOUTR_ZC */
#define WM8993_SPKOUTR_ZC_WIDTH                      1  /* SPKOUTR_ZC */
#define WM8993_SPKOUTR_MUTE_N                   0x0040  /* SPKOUTR_MUTE_N */
#define WM8993_SPKOUTR_MUTE_N_MASK              0x0040  /* SPKOUTR_MUTE_N */
#define WM8993_SPKOUTR_MUTE_N_SHIFT                  6  /* SPKOUTR_MUTE_N */
#define WM8993_SPKOUTR_MUTE_N_WIDTH                  1  /* SPKOUTR_MUTE_N */
#define WM8993_SPKOUTR_VOL_MASK                 0x003F  /* SPKOUTR_VOL - [5:0] */
#define WM8993_SPKOUTR_VOL_SHIFT                     0  /* SPKOUTR_VOL - [5:0] */
#define WM8993_SPKOUTR_VOL_WIDTH                     6  /* SPKOUTR_VOL - [5:0] */

/*
 * R40 (0x28) - Input Mixer2
 */
#define WM8993_IN2LP_TO_IN2L                    0x0080  /* IN2LP_TO_IN2L */
#define WM8993_IN2LP_TO_IN2L_MASK               0x0080  /* IN2LP_TO_IN2L */
#define WM8993_IN2LP_TO_IN2L_SHIFT                   7  /* IN2LP_TO_IN2L */
#define WM8993_IN2LP_TO_IN2L_WIDTH                   1  /* IN2LP_TO_IN2L */
#define WM8993_IN2LN_TO_IN2L                    0x0040  /* IN2LN_TO_IN2L */
#define WM8993_IN2LN_TO_IN2L_MASK               0x0040  /* IN2LN_TO_IN2L */
#define WM8993_IN2LN_TO_IN2L_SHIFT                   6  /* IN2LN_TO_IN2L */
#define WM8993_IN2LN_TO_IN2L_WIDTH                   1  /* IN2LN_TO_IN2L */
#define WM8993_IN1LP_TO_IN1L                    0x0020  /* IN1LP_TO_IN1L */
#define WM8993_IN1LP_TO_IN1L_MASK               0x0020  /* IN1LP_TO_IN1L */
#define WM8993_IN1LP_TO_IN1L_SHIFT                   5  /* IN1LP_TO_IN1L */
#define WM8993_IN1LP_TO_IN1L_WIDTH                   1  /* IN1LP_TO_IN1L */
#define WM8993_IN1LN_TO_IN1L                    0x0010  /* IN1LN_TO_IN1L */
#define WM8993_IN1LN_TO_IN1L_MASK               0x0010  /* IN1LN_TO_IN1L */
#define WM8993_IN1LN_TO_IN1L_SHIFT                   4  /* IN1LN_TO_IN1L */
#define WM8993_IN1LN_TO_IN1L_WIDTH                   1  /* IN1LN_TO_IN1L */
#define WM8993_IN2RP_TO_IN2R                    0x0008  /* IN2RP_TO_IN2R */
#define WM8993_IN2RP_TO_IN2R_MASK               0x0008  /* IN2RP_TO_IN2R */
#define WM8993_IN2RP_TO_IN2R_SHIFT                   3  /* IN2RP_TO_IN2R */
#define WM8993_IN2RP_TO_IN2R_WIDTH                   1  /* IN2RP_TO_IN2R */
#define WM8993_IN2RN_TO_IN2R                    0x0004  /* IN2RN_TO_IN2R */
#define WM8993_IN2RN_TO_IN2R_MASK               0x0004  /* IN2RN_TO_IN2R */
#define WM8993_IN2RN_TO_IN2R_SHIFT                   2  /* IN2RN_TO_IN2R */
#define WM8993_IN2RN_TO_IN2R_WIDTH                   1  /* IN2RN_TO_IN2R */
#define WM8993_IN1RP_TO_IN1R                    0x0002  /* IN1RP_TO_IN1R */
#define WM8993_IN1RP_TO_IN1R_MASK               0x0002  /* IN1RP_TO_IN1R */
#define WM8993_IN1RP_TO_IN1R_SHIFT                   1  /* IN1RP_TO_IN1R */
#define WM8993_IN1RP_TO_IN1R_WIDTH                   1  /* IN1RP_TO_IN1R */
#define WM8993_IN1RN_TO_IN1R                    0x0001  /* IN1RN_TO_IN1R */
#define WM8993_IN1RN_TO_IN1R_MASK               0x0001  /* IN1RN_TO_IN1R */
#define WM8993_IN1RN_TO_IN1R_SHIFT                   0  /* IN1RN_TO_IN1R */
#define WM8993_IN1RN_TO_IN1R_WIDTH                   1  /* IN1RN_TO_IN1R */

/*
 * R41 (0x29) - Input Mixer3
 */
#define WM8993_IN2L_TO_MIXINL                   0x0100  /* IN2L_TO_MIXINL */
#define WM8993_IN2L_TO_MIXINL_MASK              0x0100  /* IN2L_TO_MIXINL */
#define WM8993_IN2L_TO_MIXINL_SHIFT                  8  /* IN2L_TO_MIXINL */
#define WM8993_IN2L_TO_MIXINL_WIDTH                  1  /* IN2L_TO_MIXINL */
#define WM8993_IN2L_MIXINL_VOL                  0x0080  /* IN2L_MIXINL_VOL */
#define WM8993_IN2L_MIXINL_VOL_MASK             0x0080  /* IN2L_MIXINL_VOL */
#define WM8993_IN2L_MIXINL_VOL_SHIFT                 7  /* IN2L_MIXINL_VOL */
#define WM8993_IN2L_MIXINL_VOL_WIDTH                 1  /* IN2L_MIXINL_VOL */
#define WM8993_IN1L_TO_MIXINL                   0x0020  /* IN1L_TO_MIXINL */
#define WM8993_IN1L_TO_MIXINL_MASK              0x0020  /* IN1L_TO_MIXINL */
#define WM8993_IN1L_TO_MIXINL_SHIFT                  5  /* IN1L_TO_MIXINL */
#define WM8993_IN1L_TO_MIXINL_WIDTH                  1  /* IN1L_TO_MIXINL */
#define WM8993_IN1L_MIXINL_VOL                  0x0010  /* IN1L_MIXINL_VOL */
#define WM8993_IN1L_MIXINL_VOL_MASK             0x0010  /* IN1L_MIXINL_VOL */
#define WM8993_IN1L_MIXINL_VOL_SHIFT                 4  /* IN1L_MIXINL_VOL */
#define WM8993_IN1L_MIXINL_VOL_WIDTH                 1  /* IN1L_MIXINL_VOL */
#define WM8993_MIXOUTL_MIXINL_VOL_MASK          0x0007  /* MIXOUTL_MIXINL_VOL - [2:0] */
#define WM8993_MIXOUTL_MIXINL_VOL_SHIFT              0  /* MIXOUTL_MIXINL_VOL - [2:0] */
#define WM8993_MIXOUTL_MIXINL_VOL_WIDTH              3  /* MIXOUTL_MIXINL_VOL - [2:0] */

/*
 * R42 (0x2A) - Input Mixer4
 */
#define WM8993_IN2R_TO_MIXINR                   0x0100  /* IN2R_TO_MIXINR */
#define WM8993_IN2R_TO_MIXINR_MASK              0x0100  /* IN2R_TO_MIXINR */
#define WM8993_IN2R_TO_MIXINR_SHIFT                  8  /* IN2R_TO_MIXINR */
#define WM8993_IN2R_TO_MIXINR_WIDTH                  1  /* IN2R_TO_MIXINR */
#define WM8993_IN2R_MIXINR_VOL                  0x0080  /* IN2R_MIXINR_VOL */
#define WM8993_IN2R_MIXINR_VOL_MASK             0x0080  /* IN2R_MIXINR_VOL */
#define WM8993_IN2R_MIXINR_VOL_SHIFT                 7  /* IN2R_MIXINR_VOL */
#define WM8993_IN2R_MIXINR_VOL_WIDTH                 1  /* IN2R_MIXINR_VOL */
#define WM8993_IN1R_TO_MIXINR                   0x0020  /* IN1R_TO_MIXINR */
#define WM8993_IN1R_TO_MIXINR_MASK              0x0020  /* IN1R_TO_MIXINR */
#define WM8993_IN1R_TO_MIXINR_SHIFT                  5  /* IN1R_TO_MIXINR */
#define WM8993_IN1R_TO_MIXINR_WIDTH                  1  /* IN1R_TO_MIXINR */
#define WM8993_IN1R_MIXINR_VOL                  0x0010  /* IN1R_MIXINR_VOL */
#define WM8993_IN1R_MIXINR_VOL_MASK             0x0010  /* IN1R_MIXINR_VOL */
#define WM8993_IN1R_MIXINR_VOL_SHIFT                 4  /* IN1R_MIXINR_VOL */
#define WM8993_IN1R_MIXINR_VOL_WIDTH                 1  /* IN1R_MIXINR_VOL */
#define WM8993_MIXOUTR_MIXINR_VOL_MASK          0x0007  /* MIXOUTR_MIXINR_VOL - [2:0] */
#define WM8993_MIXOUTR_MIXINR_VOL_SHIFT              0  /* MIXOUTR_MIXINR_VOL - [2:0] */
#define WM8993_MIXOUTR_MIXINR_VOL_WIDTH              3  /* MIXOUTR_MIXINR_VOL - [2:0] */

/*
 * R43 (0x2B) - Input Mixer5
 */
#define WM8993_IN1LP_MIXINL_VOL_MASK            0x01C0  /* IN1LP_MIXINL_VOL - [8:6] */
#define WM8993_IN1LP_MIXINL_VOL_SHIFT                6  /* IN1LP_MIXINL_VOL - [8:6] */
#define WM8993_IN1LP_MIXINL_VOL_WIDTH                3  /* IN1LP_MIXINL_VOL - [8:6] */
#define WM8993_VRX_MIXINL_VOL_MASK              0x0007  /* VRX_MIXINL_VOL - [2:0] */
#define WM8993_VRX_MIXINL_VOL_SHIFT                  0  /* VRX_MIXINL_VOL - [2:0] */
#define WM8993_VRX_MIXINL_VOL_WIDTH                  3  /* VRX_MIXINL_VOL - [2:0] */

/*
 * R44 (0x2C) - Input Mixer6
 */
#define WM8993_IN1RP_MIXINR_VOL_MASK            0x01C0  /* IN1RP_MIXINR_VOL - [8:6] */
#define WM8993_IN1RP_MIXINR_VOL_SHIFT                6  /* IN1RP_MIXINR_VOL - [8:6] */
#define WM8993_IN1RP_MIXINR_VOL_WIDTH                3  /* IN1RP_MIXINR_VOL - [8:6] */
#define WM8993_VRX_MIXINR_VOL_MASK              0x0007  /* VRX_MIXINR_VOL - [2:0] */
#define WM8993_VRX_MIXINR_VOL_SHIFT                  0  /* VRX_MIXINR_VOL - [2:0] */
#define WM8993_VRX_MIXINR_VOL_WIDTH                  3  /* VRX_MIXINR_VOL - [2:0] */

/*
 * R45 (0x2D) - Output Mixer1
 */
#define WM8993_DACL_TO_HPOUT1L                  0x0100  /* DACL_TO_HPOUT1L */
#define WM8993_DACL_TO_HPOUT1L_MASK             0x0100  /* DACL_TO_HPOUT1L */
#define WM8993_DACL_TO_HPOUT1L_SHIFT                 8  /* DACL_TO_HPOUT1L */
#define WM8993_DACL_TO_HPOUT1L_WIDTH                 1  /* DACL_TO_HPOUT1L */
#define WM8993_MIXINR_TO_MIXOUTL                0x0080  /* MIXINR_TO_MIXOUTL */
#define WM8993_MIXINR_TO_MIXOUTL_MASK           0x0080  /* MIXINR_TO_MIXOUTL */
#define WM8993_MIXINR_TO_MIXOUTL_SHIFT               7  /* MIXINR_TO_MIXOUTL */
#define WM8993_MIXINR_TO_MIXOUTL_WIDTH               1  /* MIXINR_TO_MIXOUTL */
#define WM8993_MIXINL_TO_MIXOUTL                0x0040  /* MIXINL_TO_MIXOUTL */
#define WM8993_MIXINL_TO_MIXOUTL_MASK           0x0040  /* MIXINL_TO_MIXOUTL */
#define WM8993_MIXINL_TO_MIXOUTL_SHIFT               6  /* MIXINL_TO_MIXOUTL */
#define WM8993_MIXINL_TO_MIXOUTL_WIDTH               1  /* MIXINL_TO_MIXOUTL */
#define WM8993_IN2RN_TO_MIXOUTL                 0x0020  /* IN2RN_TO_MIXOUTL */
#define WM8993_IN2RN_TO_MIXOUTL_MASK            0x0020  /* IN2RN_TO_MIXOUTL */
#define WM8993_IN2RN_TO_MIXOUTL_SHIFT                5  /* IN2RN_TO_MIXOUTL */
#define WM8993_IN2RN_TO_MIXOUTL_WIDTH                1  /* IN2RN_TO_MIXOUTL */
#define WM8993_IN2LN_TO_MIXOUTL                 0x0010  /* IN2LN_TO_MIXOUTL */
#define WM8993_IN2LN_TO_MIXOUTL_MASK            0x0010  /* IN2LN_TO_MIXOUTL */
#define WM8993_IN2LN_TO_MIXOUTL_SHIFT                4  /* IN2LN_TO_MIXOUTL */
#define WM8993_IN2LN_TO_MIXOUTL_WIDTH                1  /* IN2LN_TO_MIXOUTL */
#define WM8993_IN1R_TO_MIXOUTL                  0x0008  /* IN1R_TO_MIXOUTL */
#define WM8993_IN1R_TO_MIXOUTL_MASK             0x0008  /* IN1R_TO_MIXOUTL */
#define WM8993_IN1R_TO_MIXOUTL_SHIFT                 3  /* IN1R_TO_MIXOUTL */
#define WM8993_IN1R_TO_MIXOUTL_WIDTH                 1  /* IN1R_TO_MIXOUTL */
#define WM8993_IN1L_TO_MIXOUTL                  0x0004  /* IN1L_TO_MIXOUTL */
#define WM8993_IN1L_TO_MIXOUTL_MASK             0x0004  /* IN1L_TO_MIXOUTL */
#define WM8993_IN1L_TO_MIXOUTL_SHIFT                 2  /* IN1L_TO_MIXOUTL */
#define WM8993_IN1L_TO_MIXOUTL_WIDTH                 1  /* IN1L_TO_MIXOUTL */
#define WM8993_IN2LP_TO_MIXOUTL                 0x0002  /* IN2LP_TO_MIXOUTL */
#define WM8993_IN2LP_TO_MIXOUTL_MASK            0x0002  /* IN2LP_TO_MIXOUTL */
#define WM8993_IN2LP_TO_MIXOUTL_SHIFT                1  /* IN2LP_TO_MIXOUTL */
#define WM8993_IN2LP_TO_MIXOUTL_WIDTH                1  /* IN2LP_TO_MIXOUTL */
#define WM8993_DACL_TO_MIXOUTL                  0x0001  /* DACL_TO_MIXOUTL */
#define WM8993_DACL_TO_MIXOUTL_MASK             0x0001  /* DACL_TO_MIXOUTL */
#define WM8993_DACL_TO_MIXOUTL_SHIFT                 0  /* DACL_TO_MIXOUTL */
#define WM8993_DACL_TO_MIXOUTL_WIDTH                 1  /* DACL_TO_MIXOUTL */

/*
 * R46 (0x2E) - Output Mixer2
 */
#define WM8993_DACR_TO_HPOUT1R                  0x0100  /* DACR_TO_HPOUT1R */
#define WM8993_DACR_TO_HPOUT1R_MASK             0x0100  /* DACR_TO_HPOUT1R */
#define WM8993_DACR_TO_HPOUT1R_SHIFT                 8  /* DACR_TO_HPOUT1R */
#define WM8993_DACR_TO_HPOUT1R_WIDTH                 1  /* DACR_TO_HPOUT1R */
#define WM8993_MIXINL_TO_MIXOUTR                0x0080  /* MIXINL_TO_MIXOUTR */
#define WM8993_MIXINL_TO_MIXOUTR_MASK           0x0080  /* MIXINL_TO_MIXOUTR */
#define WM8993_MIXINL_TO_MIXOUTR_SHIFT               7  /* MIXINL_TO_MIXOUTR */
#define WM8993_MIXINL_TO_MIXOUTR_WIDTH               1  /* MIXINL_TO_MIXOUTR */
#define WM8993_MIXINR_TO_MIXOUTR                0x0040  /* MIXINR_TO_MIXOUTR */
#define WM8993_MIXINR_TO_MIXOUTR_MASK           0x0040  /* MIXINR_TO_MIXOUTR */
#define WM8993_MIXINR_TO_MIXOUTR_SHIFT               6  /* MIXINR_TO_MIXOUTR */
#define WM8993_MIXINR_TO_MIXOUTR_WIDTH               1  /* MIXINR_TO_MIXOUTR */
#define WM8993_IN2LN_TO_MIXOUTR                 0x0020  /* IN2LN_TO_MIXOUTR */
#define WM8993_IN2LN_TO_MIXOUTR_MASK            0x0020  /* IN2LN_TO_MIXOUTR */
#define WM8993_IN2LN_TO_MIXOUTR_SHIFT                5  /* IN2LN_TO_MIXOUTR */
#define WM8993_IN2LN_TO_MIXOUTR_WIDTH                1  /* IN2LN_TO_MIXOUTR */
#define WM8993_IN2RN_TO_MIXOUTR                 0x0010  /* IN2RN_TO_MIXOUTR */
#define WM8993_IN2RN_TO_MIXOUTR_MASK            0x0010  /* IN2RN_TO_MIXOUTR */
#define WM8993_IN2RN_TO_MIXOUTR_SHIFT                4  /* IN2RN_TO_MIXOUTR */
#define WM8993_IN2RN_TO_MIXOUTR_WIDTH                1  /* IN2RN_TO_MIXOUTR */
#define WM8993_IN1L_TO_MIXOUTR                  0x0008  /* IN1L_TO_MIXOUTR */
#define WM8993_IN1L_TO_MIXOUTR_MASK             0x0008  /* IN1L_TO_MIXOUTR */
#define WM8993_IN1L_TO_MIXOUTR_SHIFT                 3  /* IN1L_TO_MIXOUTR */
#define WM8993_IN1L_TO_MIXOUTR_WIDTH                 1  /* IN1L_TO_MIXOUTR */
#define WM8993_IN1R_TO_MIXOUTR                  0x0004  /* IN1R_TO_MIXOUTR */
#define WM8993_IN1R_TO_MIXOUTR_MASK             0x0004  /* IN1R_TO_MIXOUTR */
#define WM8993_IN1R_TO_MIXOUTR_SHIFT                 2  /* IN1R_TO_MIXOUTR */
#define WM8993_IN1R_TO_MIXOUTR_WIDTH                 1  /* IN1R_TO_MIXOUTR */
#define WM8993_IN2RP_TO_MIXOUTR                 0x0002  /* IN2RP_TO_MIXOUTR */
#define WM8993_IN2RP_TO_MIXOUTR_MASK            0x0002  /* IN2RP_TO_MIXOUTR */
#define WM8993_IN2RP_TO_MIXOUTR_SHIFT                1  /* IN2RP_TO_MIXOUTR */
#define WM8993_IN2RP_TO_MIXOUTR_WIDTH                1  /* IN2RP_TO_MIXOUTR */
#define WM8993_DACR_TO_MIXOUTR                  0x0001  /* DACR_TO_MIXOUTR */
#define WM8993_DACR_TO_MIXOUTR_MASK             0x0001  /* DACR_TO_MIXOUTR */
#define WM8993_DACR_TO_MIXOUTR_SHIFT                 0  /* DACR_TO_MIXOUTR */
#define WM8993_DACR_TO_MIXOUTR_WIDTH                 1  /* DACR_TO_MIXOUTR */

/*
 * R47 (0x2F) - Output Mixer3
 */
#define WM8993_IN2LP_MIXOUTL_VOL_MASK           0x0E00  /* IN2LP_MIXOUTL_VOL - [11:9] */
#define WM8993_IN2LP_MIXOUTL_VOL_SHIFT               9  /* IN2LP_MIXOUTL_VOL - [11:9] */
#define WM8993_IN2LP_MIXOUTL_VOL_WIDTH               3  /* IN2LP_MIXOUTL_VOL - [11:9] */
#define WM8993_IN2LN_MIXOUTL_VOL_MASK           0x01C0  /* IN2LN_MIXOUTL_VOL - [8:6] */
#define WM8993_IN2LN_MIXOUTL_VOL_SHIFT               6  /* IN2LN_MIXOUTL_VOL - [8:6] */
#define WM8993_IN2LN_MIXOUTL_VOL_WIDTH               3  /* IN2LN_MIXOUTL_VOL - [8:6] */
#define WM8993_IN1R_MIXOUTL_VOL_MASK            0x0038  /* IN1R_MIXOUTL_VOL - [5:3] */
#define WM8993_IN1R_MIXOUTL_VOL_SHIFT                3  /* IN1R_MIXOUTL_VOL - [5:3] */
#define WM8993_IN1R_MIXOUTL_VOL_WIDTH                3  /* IN1R_MIXOUTL_VOL - [5:3] */
#define WM8993_IN1L_MIXOUTL_VOL_MASK            0x0007  /* IN1L_MIXOUTL_VOL - [2:0] */
#define WM8993_IN1L_MIXOUTL_VOL_SHIFT                0  /* IN1L_MIXOUTL_VOL - [2:0] */
#define WM8993_IN1L_MIXOUTL_VOL_WIDTH                3  /* IN1L_MIXOUTL_VOL - [2:0] */

/*
 * R48 (0x30) - Output Mixer4
 */
#define WM8993_IN2RP_MIXOUTR_VOL_MASK           0x0E00  /* IN2RP_MIXOUTR_VOL - [11:9] */
#define WM8993_IN2RP_MIXOUTR_VOL_SHIFT               9  /* IN2RP_MIXOUTR_VOL - [11:9] */
#define WM8993_IN2RP_MIXOUTR_VOL_WIDTH               3  /* IN2RP_MIXOUTR_VOL - [11:9] */
#define WM8993_IN2RN_MIXOUTR_VOL_MASK           0x01C0  /* IN2RN_MIXOUTR_VOL - [8:6] */
#define WM8993_IN2RN_MIXOUTR_VOL_SHIFT               6  /* IN2RN_MIXOUTR_VOL - [8:6] */
#define WM8993_IN2RN_MIXOUTR_VOL_WIDTH               3  /* IN2RN_MIXOUTR_VOL - [8:6] */
#define WM8993_IN1L_MIXOUTR_VOL_MASK            0x0038  /* IN1L_MIXOUTR_VOL - [5:3] */
#define WM8993_IN1L_MIXOUTR_VOL_SHIFT                3  /* IN1L_MIXOUTR_VOL - [5:3] */
#define WM8993_IN1L_MIXOUTR_VOL_WIDTH                3  /* IN1L_MIXOUTR_VOL - [5:3] */
#define WM8993_IN1R_MIXOUTR_VOL_MASK            0x0007  /* IN1R_MIXOUTR_VOL - [2:0] */
#define WM8993_IN1R_MIXOUTR_VOL_SHIFT                0  /* IN1R_MIXOUTR_VOL - [2:0] */
#define WM8993_IN1R_MIXOUTR_VOL_WIDTH                3  /* IN1R_MIXOUTR_VOL - [2:0] */

/*
 * R49 (0x31) - Output Mixer5
 */
#define WM8993_DACL_MIXOUTL_VOL_MASK            0x0E00  /* DACL_MIXOUTL_VOL - [11:9] */
#define WM8993_DACL_MIXOUTL_VOL_SHIFT                9  /* DACL_MIXOUTL_VOL - [11:9] */
#define WM8993_DACL_MIXOUTL_VOL_WIDTH                3  /* DACL_MIXOUTL_VOL - [11:9] */
#define WM8993_IN2RN_MIXOUTL_VOL_MASK           0x01C0  /* IN2RN_MIXOUTL_VOL - [8:6] */
#define WM8993_IN2RN_MIXOUTL_VOL_SHIFT               6  /* IN2RN_MIXOUTL_VOL - [8:6] */
#define WM8993_IN2RN_MIXOUTL_VOL_WIDTH               3  /* IN2RN_MIXOUTL_VOL - [8:6] */
#define WM8993_MIXINR_MIXOUTL_VOL_MASK          0x0038  /* MIXINR_MIXOUTL_VOL - [5:3] */
#define WM8993_MIXINR_MIXOUTL_VOL_SHIFT              3  /* MIXINR_MIXOUTL_VOL - [5:3] */
#define WM8993_MIXINR_MIXOUTL_VOL_WIDTH              3  /* MIXINR_MIXOUTL_VOL - [5:3] */
#define WM8993_MIXINL_MIXOUTL_VOL_MASK          0x0007  /* MIXINL_MIXOUTL_VOL - [2:0] */
#define WM8993_MIXINL_MIXOUTL_VOL_SHIFT              0  /* MIXINL_MIXOUTL_VOL - [2:0] */
#define WM8993_MIXINL_MIXOUTL_VOL_WIDTH              3  /* MIXINL_MIXOUTL_VOL - [2:0] */

/*
 * R50 (0x32) - Output Mixer6
 */
#define WM8993_DACR_MIXOUTR_VOL_MASK            0x0E00  /* DACR_MIXOUTR_VOL - [11:9] */
#define WM8993_DACR_MIXOUTR_VOL_SHIFT                9  /* DACR_MIXOUTR_VOL - [11:9] */
#define WM8993_DACR_MIXOUTR_VOL_WIDTH                3  /* DACR_MIXOUTR_VOL - [11:9] */
#define WM8993_IN2LN_MIXOUTR_VOL_MASK           0x01C0  /* IN2LN_MIXOUTR_VOL - [8:6] */
#define WM8993_IN2LN_MIXOUTR_VOL_SHIFT               6  /* IN2LN_MIXOUTR_VOL - [8:6] */
#define WM8993_IN2LN_MIXOUTR_VOL_WIDTH               3  /* IN2LN_MIXOUTR_VOL - [8:6] */
#define WM8993_MIXINL_MIXOUTR_VOL_MASK          0x0038  /* MIXINL_MIXOUTR_VOL - [5:3] */
#define WM8993_MIXINL_MIXOUTR_VOL_SHIFT              3  /* MIXINL_MIXOUTR_VOL - [5:3] */
#define WM8993_MIXINL_MIXOUTR_VOL_WIDTH              3  /* MIXINL_MIXOUTR_VOL - [5:3] */
#define WM8993_MIXINR_MIXOUTR_VOL_MASK          0x0007  /* MIXINR_MIXOUTR_VOL - [2:0] */
#define WM8993_MIXINR_MIXOUTR_VOL_SHIFT              0  /* MIXINR_MIXOUTR_VOL - [2:0] */
#define WM8993_MIXINR_MIXOUTR_VOL_WIDTH              3  /* MIXINR_MIXOUTR_VOL - [2:0] */

/*
 * R51 (0x33) - HPOUT2 Mixer
 */
#define WM8993_VRX_TO_HPOUT2                    0x0020  /* VRX_TO_HPOUT2 */
#define WM8993_VRX_TO_HPOUT2_MASK               0x0020  /* VRX_TO_HPOUT2 */
#define WM8993_VRX_TO_HPOUT2_SHIFT                   5  /* VRX_TO_HPOUT2 */
#define WM8993_VRX_TO_HPOUT2_WIDTH                   1  /* VRX_TO_HPOUT2 */
#define WM8993_MIXOUTLVOL_TO_HPOUT2             0x0010  /* MIXOUTLVOL_TO_HPOUT2 */
#define WM8993_MIXOUTLVOL_TO_HPOUT2_MASK        0x0010  /* MIXOUTLVOL_TO_HPOUT2 */
#define WM8993_MIXOUTLVOL_TO_HPOUT2_SHIFT            4  /* MIXOUTLVOL_TO_HPOUT2 */
#define WM8993_MIXOUTLVOL_TO_HPOUT2_WIDTH            1  /* MIXOUTLVOL_TO_HPOUT2 */
#define WM8993_MIXOUTRVOL_TO_HPOUT2             0x0008  /* MIXOUTRVOL_TO_HPOUT2 */
#define WM8993_MIXOUTRVOL_TO_HPOUT2_MASK        0x0008  /* MIXOUTRVOL_TO_HPOUT2 */
#define WM8993_MIXOUTRVOL_TO_HPOUT2_SHIFT            3  /* MIXOUTRVOL_TO_HPOUT2 */
#define WM8993_MIXOUTRVOL_TO_HPOUT2_WIDTH            1  /* MIXOUTRVOL_TO_HPOUT2 */

/*
 * R52 (0x34) - Line Mixer1
 */
#define WM8993_MIXOUTL_TO_LINEOUT1N             0x0040  /* MIXOUTL_TO_LINEOUT1N */
#define WM8993_MIXOUTL_TO_LINEOUT1N_MASK        0x0040  /* MIXOUTL_TO_LINEOUT1N */
#define WM8993_MIXOUTL_TO_LINEOUT1N_SHIFT            6  /* MIXOUTL_TO_LINEOUT1N */
#define WM8993_MIXOUTL_TO_LINEOUT1N_WIDTH            1  /* MIXOUTL_TO_LINEOUT1N */
#define WM8993_MIXOUTR_TO_LINEOUT1N             0x0020  /* MIXOUTR_TO_LINEOUT1N */
#define WM8993_MIXOUTR_TO_LINEOUT1N_MASK        0x0020  /* MIXOUTR_TO_LINEOUT1N */
#define WM8993_MIXOUTR_TO_LINEOUT1N_SHIFT            5  /* MIXOUTR_TO_LINEOUT1N */
#define WM8993_MIXOUTR_TO_LINEOUT1N_WIDTH            1  /* MIXOUTR_TO_LINEOUT1N */
#define WM8993_LINEOUT1_MODE                    0x0010  /* LINEOUT1_MODE */
#define WM8993_LINEOUT1_MODE_MASK               0x0010  /* LINEOUT1_MODE */
#define WM8993_LINEOUT1_MODE_SHIFT                   4  /* LINEOUT1_MODE */
#define WM8993_LINEOUT1_MODE_WIDTH                   1  /* LINEOUT1_MODE */
#define WM8993_IN1R_TO_LINEOUT1P                0x0004  /* IN1R_TO_LINEOUT1P */
#define WM8993_IN1R_TO_LINEOUT1P_MASK           0x0004  /* IN1R_TO_LINEOUT1P */
#define WM8993_IN1R_TO_LINEOUT1P_SHIFT               2  /* IN1R_TO_LINEOUT1P */
#define WM8993_IN1R_TO_LINEOUT1P_WIDTH               1  /* IN1R_TO_LINEOUT1P */
#define WM8993_IN1L_TO_LINEOUT1P                0x0002  /* IN1L_TO_LINEOUT1P */
#define WM8993_IN1L_TO_LINEOUT1P_MASK           0x0002  /* IN1L_TO_LINEOUT1P */
#define WM8993_IN1L_TO_LINEOUT1P_SHIFT               1  /* IN1L_TO_LINEOUT1P */
#define WM8993_IN1L_TO_LINEOUT1P_WIDTH               1  /* IN1L_TO_LINEOUT1P */
#define WM8993_MIXOUTL_TO_LINEOUT1P             0x0001  /* MIXOUTL_TO_LINEOUT1P */
#define WM8993_MIXOUTL_TO_LINEOUT1P_MASK        0x0001  /* MIXOUTL_TO_LINEOUT1P */
#define WM8993_MIXOUTL_TO_LINEOUT1P_SHIFT            0  /* MIXOUTL_TO_LINEOUT1P */
#define WM8993_MIXOUTL_TO_LINEOUT1P_WIDTH            1  /* MIXOUTL_TO_LINEOUT1P */

/*
 * R53 (0x35) - Line Mixer2
 */
#define WM8993_MIXOUTR_TO_LINEOUT2N             0x0040  /* MIXOUTR_TO_LINEOUT2N */
#define WM8993_MIXOUTR_TO_LINEOUT2N_MASK        0x0040  /* MIXOUTR_TO_LINEOUT2N */
#define WM8993_MIXOUTR_TO_LINEOUT2N_SHIFT            6  /* MIXOUTR_TO_LINEOUT2N */
#define WM8993_MIXOUTR_TO_LINEOUT2N_WIDTH            1  /* MIXOUTR_TO_LINEOUT2N */
#define WM8993_MIXOUTL_TO_LINEOUT2N             0x0020  /* MIXOUTL_TO_LINEOUT2N */
#define WM8993_MIXOUTL_TO_LINEOUT2N_MASK        0x0020  /* MIXOUTL_TO_LINEOUT2N */
#define WM8993_MIXOUTL_TO_LINEOUT2N_SHIFT            5  /* MIXOUTL_TO_LINEOUT2N */
#define WM8993_MIXOUTL_TO_LINEOUT2N_WIDTH            1  /* MIXOUTL_TO_LINEOUT2N */
#define WM8993_LINEOUT2_MODE                    0x0010  /* LINEOUT2_MODE */
#define WM8993_LINEOUT2_MODE_MASK               0x0010  /* LINEOUT2_MODE */
#define WM8993_LINEOUT2_MODE_SHIFT                   4  /* LINEOUT2_MODE */
#define WM8993_LINEOUT2_MODE_WIDTH                   1  /* LINEOUT2_MODE */
#define WM8993_IN1L_TO_LINEOUT2P                0x0004  /* IN1L_TO_LINEOUT2P */
#define WM8993_IN1L_TO_LINEOUT2P_MASK           0x0004  /* IN1L_TO_LINEOUT2P */
#define WM8993_IN1L_TO_LINEOUT2P_SHIFT               2  /* IN1L_TO_LINEOUT2P */
#define WM8993_IN1L_TO_LINEOUT2P_WIDTH               1  /* IN1L_TO_LINEOUT2P */
#define WM8993_IN1R_TO_LINEOUT2P                0x0002  /* IN1R_TO_LINEOUT2P */
#define WM8993_IN1R_TO_LINEOUT2P_MASK           0x0002  /* IN1R_TO_LINEOUT2P */
#define WM8993_IN1R_TO_LINEOUT2P_SHIFT               1  /* IN1R_TO_LINEOUT2P */
#define WM8993_IN1R_TO_LINEOUT2P_WIDTH               1  /* IN1R_TO_LINEOUT2P */
#define WM8993_MIXOUTR_TO_LINEOUT2P             0x0001  /* MIXOUTR_TO_LINEOUT2P */
#define WM8993_MIXOUTR_TO_LINEOUT2P_MASK        0x0001  /* MIXOUTR_TO_LINEOUT2P */
#define WM8993_MIXOUTR_TO_LINEOUT2P_SHIFT            0  /* MIXOUTR_TO_LINEOUT2P */
#define WM8993_MIXOUTR_TO_LINEOUT2P_WIDTH            1  /* MIXOUTR_TO_LINEOUT2P */

/*
 * R54 (0x36) - Speaker Mixer
 */
#define WM8993_SPKAB_REF_SEL                    0x0100  /* SPKAB_REF_SEL */
#define WM8993_SPKAB_REF_SEL_MASK               0x0100  /* SPKAB_REF_SEL */
#define WM8993_SPKAB_REF_SEL_SHIFT                   8  /* SPKAB_REF_SEL */
#define WM8993_SPKAB_REF_SEL_WIDTH                   1  /* SPKAB_REF_SEL */
#define WM8993_MIXINL_TO_SPKMIXL                0x0080  /* MIXINL_TO_SPKMIXL */
#define WM8993_MIXINL_TO_SPKMIXL_MASK           0x0080  /* MIXINL_TO_SPKMIXL */
#define WM8993_MIXINL_TO_SPKMIXL_SHIFT               7  /* MIXINL_TO_SPKMIXL */
#define WM8993_MIXINL_TO_SPKMIXL_WIDTH               1  /* MIXINL_TO_SPKMIXL */
#define WM8993_MIXINR_TO_SPKMIXR                0x0040  /* MIXINR_TO_SPKMIXR */
#define WM8993_MIXINR_TO_SPKMIXR_MASK           0x0040  /* MIXINR_TO_SPKMIXR */
#define WM8993_MIXINR_TO_SPKMIXR_SHIFT               6  /* MIXINR_TO_SPKMIXR */
#define WM8993_MIXINR_TO_SPKMIXR_WIDTH               1  /* MIXINR_TO_SPKMIXR */
#define WM8993_IN1LP_TO_SPKMIXL                 0x0020  /* IN1LP_TO_SPKMIXL */
#define WM8993_IN1LP_TO_SPKMIXL_MASK            0x0020  /* IN1LP_TO_SPKMIXL */
#define WM8993_IN1LP_TO_SPKMIXL_SHIFT                5  /* IN1LP_TO_SPKMIXL */
#define WM8993_IN1LP_TO_SPKMIXL_WIDTH                1  /* IN1LP_TO_SPKMIXL */
#define WM8993_IN1RP_TO_SPKMIXR                 0x0010  /* IN1RP_TO_SPKMIXR */
#define WM8993_IN1RP_TO_SPKMIXR_MASK            0x0010  /* IN1RP_TO_SPKMIXR */
#define WM8993_IN1RP_TO_SPKMIXR_SHIFT                4  /* IN1RP_TO_SPKMIXR */
#define WM8993_IN1RP_TO_SPKMIXR_WIDTH                1  /* IN1RP_TO_SPKMIXR */
#define WM8993_MIXOUTL_TO_SPKMIXL               0x0008  /* MIXOUTL_TO_SPKMIXL */
#define WM8993_MIXOUTL_TO_SPKMIXL_MASK          0x0008  /* MIXOUTL_TO_SPKMIXL */
#define WM8993_MIXOUTL_TO_SPKMIXL_SHIFT              3  /* MIXOUTL_TO_SPKMIXL */
#define WM8993_MIXOUTL_TO_SPKMIXL_WIDTH              1  /* MIXOUTL_TO_SPKMIXL */
#define WM8993_MIXOUTR_TO_SPKMIXR               0x0004  /* MIXOUTR_TO_SPKMIXR */
#define WM8993_MIXOUTR_TO_SPKMIXR_MASK          0x0004  /* MIXOUTR_TO_SPKMIXR */
#define WM8993_MIXOUTR_TO_SPKMIXR_SHIFT              2  /* MIXOUTR_TO_SPKMIXR */
#define WM8993_MIXOUTR_TO_SPKMIXR_WIDTH              1  /* MIXOUTR_TO_SPKMIXR */
#define WM8993_DACL_TO_SPKMIXL                  0x0002  /* DACL_TO_SPKMIXL */
#define WM8993_DACL_TO_SPKMIXL_MASK             0x0002  /* DACL_TO_SPKMIXL */
#define WM8993_DACL_TO_SPKMIXL_SHIFT                 1  /* DACL_TO_SPKMIXL */
#define WM8993_DACL_TO_SPKMIXL_WIDTH                 1  /* DACL_TO_SPKMIXL */
#define WM8993_DACR_TO_SPKMIXR                  0x0001  /* DACR_TO_SPKMIXR */
#define WM8993_DACR_TO_SPKMIXR_MASK             0x0001  /* DACR_TO_SPKMIXR */
#define WM8993_DACR_TO_SPKMIXR_SHIFT                 0  /* DACR_TO_SPKMIXR */
#define WM8993_DACR_TO_SPKMIXR_WIDTH                 1  /* DACR_TO_SPKMIXR */

/*
 * R55 (0x37) - Additional Control
 */
#define WM8993_LINEOUT1_FB                      0x0080  /* LINEOUT1_FB */
#define WM8993_LINEOUT1_FB_MASK                 0x0080  /* LINEOUT1_FB */
#define WM8993_LINEOUT1_FB_SHIFT                     7  /* LINEOUT1_FB */
#define WM8993_LINEOUT1_FB_WIDTH                     1  /* LINEOUT1_FB */
#define WM8993_LINEOUT2_FB                      0x0040  /* LINEOUT2_FB */
#define WM8993_LINEOUT2_FB_MASK                 0x0040  /* LINEOUT2_FB */
#define WM8993_LINEOUT2_FB_SHIFT                     6  /* LINEOUT2_FB */
#define WM8993_LINEOUT2_FB_WIDTH                     1  /* LINEOUT2_FB */
#define WM8993_VROI                             0x0001  /* VROI */
#define WM8993_VROI_MASK                        0x0001  /* VROI */
#define WM8993_VROI_SHIFT                            0  /* VROI */
#define WM8993_VROI_WIDTH                            1  /* VROI */

/*
 * R56 (0x38) - AntiPOP1
 */
#define WM8993_LINEOUT_VMID_BUF_ENA             0x0080  /* LINEOUT_VMID_BUF_ENA */
#define WM8993_LINEOUT_VMID_BUF_ENA_MASK        0x0080  /* LINEOUT_VMID_BUF_ENA */
#define WM8993_LINEOUT_VMID_BUF_ENA_SHIFT            7  /* LINEOUT_VMID_BUF_ENA */
#define WM8993_LINEOUT_VMID_BUF_ENA_WIDTH            1  /* LINEOUT_VMID_BUF_ENA */
#define WM8993_HPOUT2_IN_ENA                    0x0040  /* HPOUT2_IN_ENA */
#define WM8993_HPOUT2_IN_ENA_MASK               0x0040  /* HPOUT2_IN_ENA */
#define WM8993_HPOUT2_IN_ENA_SHIFT                   6  /* HPOUT2_IN_ENA */
#define WM8993_HPOUT2_IN_ENA_WIDTH                   1  /* HPOUT2_IN_ENA */
#define WM8993_LINEOUT1_DISCH                   0x0020  /* LINEOUT1_DISCH */
#define WM8993_LINEOUT1_DISCH_MASK              0x0020  /* LINEOUT1_DISCH */
#define WM8993_LINEOUT1_DISCH_SHIFT                  5  /* LINEOUT1_DISCH */
#define WM8993_LINEOUT1_DISCH_WIDTH                  1  /* LINEOUT1_DISCH */
#define WM8993_LINEOUT2_DISCH                   0x0010  /* LINEOUT2_DISCH */
#define WM8993_LINEOUT2_DISCH_MASK              0x0010  /* LINEOUT2_DISCH */
#define WM8993_LINEOUT2_DISCH_SHIFT                  4  /* LINEOUT2_DISCH */
#define WM8993_LINEOUT2_DISCH_WIDTH                  1  /* LINEOUT2_DISCH */

/*
 * R57 (0x39) - AntiPOP2
 */
#define WM8993_VMID_RAMP_MASK                   0x0060  /* VMID_RAMP - [6:5] */
#define WM8993_VMID_RAMP_SHIFT                       5  /* VMID_RAMP - [6:5] */
#define WM8993_VMID_RAMP_WIDTH                       2  /* VMID_RAMP - [6:5] */
#define WM8993_VMID_BUF_ENA                     0x0008  /* VMID_BUF_ENA */
#define WM8993_VMID_BUF_ENA_MASK                0x0008  /* VMID_BUF_ENA */
#define WM8993_VMID_BUF_ENA_SHIFT                    3  /* VMID_BUF_ENA */
#define WM8993_VMID_BUF_ENA_WIDTH                    1  /* VMID_BUF_ENA */
#define WM8993_STARTUP_BIAS_ENA                 0x0004  /* STARTUP_BIAS_ENA */
#define WM8993_STARTUP_BIAS_ENA_MASK            0x0004  /* STARTUP_BIAS_ENA */
#define WM8993_STARTUP_BIAS_ENA_SHIFT                2  /* STARTUP_BIAS_ENA */
#define WM8993_STARTUP_BIAS_ENA_WIDTH                1  /* STARTUP_BIAS_ENA */
#define WM8993_BIAS_SRC                         0x0002  /* BIAS_SRC */
#define WM8993_BIAS_SRC_MASK                    0x0002  /* BIAS_SRC */
#define WM8993_BIAS_SRC_SHIFT                        1  /* BIAS_SRC */
#define WM8993_BIAS_SRC_WIDTH                        1  /* BIAS_SRC */
#define WM8993_VMID_DISCH                       0x0001  /* VMID_DISCH */
#define WM8993_VMID_DISCH_MASK                  0x0001  /* VMID_DISCH */
#define WM8993_VMID_DISCH_SHIFT                      0  /* VMID_DISCH */
#define WM8993_VMID_DISCH_WIDTH                      1  /* VMID_DISCH */

/*
 * R58 (0x3A) - MICBIAS
 */
#define WM8993_JD_SCTHR_MASK                    0x00C0  /* JD_SCTHR - [7:6] */
#define WM8993_JD_SCTHR_SHIFT                        6  /* JD_SCTHR - [7:6] */
#define WM8993_JD_SCTHR_WIDTH                        2  /* JD_SCTHR - [7:6] */
#define WM8993_JD_THR_MASK                      0x0030  /* JD_THR - [5:4] */
#define WM8993_JD_THR_SHIFT                          4  /* JD_THR - [5:4] */
#define WM8993_JD_THR_WIDTH                          2  /* JD_THR - [5:4] */
#define WM8993_JD_ENA                           0x0004  /* JD_ENA */
#define WM8993_JD_ENA_MASK                      0x0004  /* JD_ENA */
#define WM8993_JD_ENA_SHIFT                          2  /* JD_ENA */
#define WM8993_JD_ENA_WIDTH                          1  /* JD_ENA */
#define WM8993_MICB2_LVL                        0x0002  /* MICB2_LVL */
#define WM8993_MICB2_LVL_MASK                   0x0002  /* MICB2_LVL */
#define WM8993_MICB2_LVL_SHIFT                       1  /* MICB2_LVL */
#define WM8993_MICB2_LVL_WIDTH                       1  /* MICB2_LVL */
#define WM8993_MICB1_LVL                        0x0001  /* MICB1_LVL */
#define WM8993_MICB1_LVL_MASK                   0x0001  /* MICB1_LVL */
#define WM8993_MICB1_LVL_SHIFT                       0  /* MICB1_LVL */
#define WM8993_MICB1_LVL_WIDTH                       1  /* MICB1_LVL */

/*
 * R60 (0x3C) - FLL Control 1
 */
#define WM8993_FLL_FRAC                         0x0004  /* FLL_FRAC */
#define WM8993_FLL_FRAC_MASK                    0x0004  /* FLL_FRAC */
#define WM8993_FLL_FRAC_SHIFT                        2  /* FLL_FRAC */
#define WM8993_FLL_FRAC_WIDTH                        1  /* FLL_FRAC */
#define WM8993_FLL_OSC_ENA                      0x0002  /* FLL_OSC_ENA */
#define WM8993_FLL_OSC_ENA_MASK                 0x0002  /* FLL_OSC_ENA */
#define WM8993_FLL_OSC_ENA_SHIFT                     1  /* FLL_OSC_ENA */
#define WM8993_FLL_OSC_ENA_WIDTH                     1  /* FLL_OSC_ENA */
#define WM8993_FLL_ENA                          0x0001  /* FLL_ENA */
#define WM8993_FLL_ENA_MASK                     0x0001  /* FLL_ENA */
#define WM8993_FLL_ENA_SHIFT                         0  /* FLL_ENA */
#define WM8993_FLL_ENA_WIDTH                         1  /* FLL_ENA */

/*
 * R61 (0x3D) - FLL Control 2
 */
#define WM8993_FLL_OUTDIV_MASK                  0x0700  /* FLL_OUTDIV - [10:8] */
#define WM8993_FLL_OUTDIV_SHIFT                      8  /* FLL_OUTDIV - [10:8] */
#define WM8993_FLL_OUTDIV_WIDTH                      3  /* FLL_OUTDIV - [10:8] */
#define WM8993_FLL_CTRL_RATE_MASK               0x0070  /* FLL_CTRL_RATE - [6:4] */
#define WM8993_FLL_CTRL_RATE_SHIFT                   4  /* FLL_CTRL_RATE - [6:4] */
#define WM8993_FLL_CTRL_RATE_WIDTH                   3  /* FLL_CTRL_RATE - [6:4] */
#define WM8993_FLL_FRATIO_MASK                  0x0007  /* FLL_FRATIO - [2:0] */
#define WM8993_FLL_FRATIO_SHIFT                      0  /* FLL_FRATIO - [2:0] */
#define WM8993_FLL_FRATIO_WIDTH                      3  /* FLL_FRATIO - [2:0] */

/*
 * R62 (0x3E) - FLL Control 3
 */
#define WM8993_FLL_K_MASK                       0xFFFF  /* FLL_K - [15:0] */
#define WM8993_FLL_K_SHIFT                           0  /* FLL_K - [15:0] */
#define WM8993_FLL_K_WIDTH                          16  /* FLL_K - [15:0] */

/*
 * R63 (0x3F) - FLL Control 4
 */
#define WM8993_FLL_N_MASK                       0x7FE0  /* FLL_N - [14:5] */
#define WM8993_FLL_N_SHIFT                           5  /* FLL_N - [14:5] */
#define WM8993_FLL_N_WIDTH                          10  /* FLL_N - [14:5] */
#define WM8993_FLL_GAIN_MASK                    0x000F  /* FLL_GAIN - [3:0] */
#define WM8993_FLL_GAIN_SHIFT                        0  /* FLL_GAIN - [3:0] */
#define WM8993_FLL_GAIN_WIDTH                        4  /* FLL_GAIN - [3:0] */

/*
 * R64 (0x40) - FLL Control 5
 */
#define WM8993_FLL_FRC_NCO_VAL_MASK             0x1F80  /* FLL_FRC_NCO_VAL - [12:7] */
#define WM8993_FLL_FRC_NCO_VAL_SHIFT                 7  /* FLL_FRC_NCO_VAL - [12:7] */
#define WM8993_FLL_FRC_NCO_VAL_WIDTH                 6  /* FLL_FRC_NCO_VAL - [12:7] */
#define WM8993_FLL_FRC_NCO                      0x0040  /* FLL_FRC_NCO */
#define WM8993_FLL_FRC_NCO_MASK                 0x0040  /* FLL_FRC_NCO */
#define WM8993_FLL_FRC_NCO_SHIFT                     6  /* FLL_FRC_NCO */
#define WM8993_FLL_FRC_NCO_WIDTH                     1  /* FLL_FRC_NCO */
#define WM8993_FLL_CLK_REF_DIV_MASK             0x0018  /* FLL_CLK_REF_DIV - [4:3] */
#define WM8993_FLL_CLK_REF_DIV_SHIFT                 3  /* FLL_CLK_REF_DIV - [4:3] */
#define WM8993_FLL_CLK_REF_DIV_WIDTH                 2  /* FLL_CLK_REF_DIV - [4:3] */
#define WM8993_FLL_CLK_SRC_MASK                 0x0003  /* FLL_CLK_SRC - [1:0] */
#define WM8993_FLL_CLK_SRC_SHIFT                     0  /* FLL_CLK_SRC - [1:0] */
#define WM8993_FLL_CLK_SRC_WIDTH                     2  /* FLL_CLK_SRC - [1:0] */

/*
 * R65 (0x41) - Clocking 3
 */
#define WM8993_CLK_DCS_DIV_MASK                 0x3C00  /* CLK_DCS_DIV - [13:10] */
#define WM8993_CLK_DCS_DIV_SHIFT                    10  /* CLK_DCS_DIV - [13:10] */
#define WM8993_CLK_DCS_DIV_WIDTH                     4  /* CLK_DCS_DIV - [13:10] */
#define WM8993_SAMPLE_RATE_MASK                 0x0380  /* SAMPLE_RATE - [9:7] */
#define WM8993_SAMPLE_RATE_SHIFT                     7  /* SAMPLE_RATE - [9:7] */
#define WM8993_SAMPLE_RATE_WIDTH                     3  /* SAMPLE_RATE - [9:7] */
#define WM8993_CLK_SYS_RATE_MASK                0x001E  /* CLK_SYS_RATE - [4:1] */
#define WM8993_CLK_SYS_RATE_SHIFT                    1  /* CLK_SYS_RATE - [4:1] */
#define WM8993_CLK_SYS_RATE_WIDTH                    4  /* CLK_SYS_RATE - [4:1] */
#define WM8993_CLK_DSP_ENA                      0x0001  /* CLK_DSP_ENA */
#define WM8993_CLK_DSP_ENA_MASK                 0x0001  /* CLK_DSP_ENA */
#define WM8993_CLK_DSP_ENA_SHIFT                     0  /* CLK_DSP_ENA */
#define WM8993_CLK_DSP_ENA_WIDTH                     1  /* CLK_DSP_ENA */

/*
 * R66 (0x42) - Clocking 4
 */
#define WM8993_DAC_DIV4                         0x0200  /* DAC_DIV4 */
#define WM8993_DAC_DIV4_MASK                    0x0200  /* DAC_DIV4 */
#define WM8993_DAC_DIV4_SHIFT                        9  /* DAC_DIV4 */
#define WM8993_DAC_DIV4_WIDTH                        1  /* DAC_DIV4 */
#define WM8993_CLK_256K_DIV_MASK                0x007E  /* CLK_256K_DIV - [6:1] */
#define WM8993_CLK_256K_DIV_SHIFT                    1  /* CLK_256K_DIV - [6:1] */
#define WM8993_CLK_256K_DIV_WIDTH                    6  /* CLK_256K_DIV - [6:1] */
#define WM8993_SR_MODE                          0x0001  /* SR_MODE */
#define WM8993_SR_MODE_MASK                     0x0001  /* SR_MODE */
#define WM8993_SR_MODE_SHIFT                         0  /* SR_MODE */
#define WM8993_SR_MODE_WIDTH                         1  /* SR_MODE */

/*
 * R67 (0x43) - MW Slave Control
 */
#define WM8993_MASK_WRITE_ENA                   0x0001  /* MASK_WRITE_ENA */
#define WM8993_MASK_WRITE_ENA_MASK              0x0001  /* MASK_WRITE_ENA */
#define WM8993_MASK_WRITE_ENA_SHIFT                  0  /* MASK_WRITE_ENA */
#define WM8993_MASK_WRITE_ENA_WIDTH                  1  /* MASK_WRITE_ENA */

/*
 * R69 (0x45) - Bus Control 1
 */
#define WM8993_CLK_SYS_ENA M8993_H
#define WM8990x0002  /* CL#ifndef WM*/
#define WM8993_struct snd__MASK_H
#define WM8993_H

extern struct snd_soc_dai wm8993_dai;
extern struSHIFTM8993_H
#define WM8991tern struct snd_soc_dai wm8993_dai;
extern struWIDTH
#define WM8993_SYSCLK_FLL      2

#define
/*
 * R70 (0x46) - Write Sequencer 0
soc_dai wm8993_dai;WSEQef WM8993_H
#define WM8993993_H
100tern          993_SOFTWARE_RESET        uct snd_soc_codec_devie WM8993_POWER_MANAGEMENT_1               0x01
#de    1
#define WM8993_SYSCL   8POWER_MANAGEMENT_1               0x01
#de WM8993_FLL_BCLK  2
#definSCLK_FLL _MANAGEMENT_1               0x01WRITE_INDEXuct snd_soc_codec_H

1FUDIO_INTER#define WM8 - [4:0]E_2                0x05
#define WM89    1
#define WM8993__POWER_MANA    0x06
#define WM8993_CLOCKING_2                      WM8993_FLL_BCLK  2
#5AUDIO_INTERFACE_3                
/*
 * Re1ister7values.
 */
#define 1M8993_SOFTWARE_RESET     DAT       uct snd_soc_codec_0x703_POWER_MANA          efin14:12M8993_CLOCKING_2                   0    1
#define WM8993_1xtern _DIGITAL_VOLUME         0x0C
#define WM8993_DIGITAL_SIDE_TON WM8993_FLL_BCLK  2
#d3define WM8993_ADC_CTRL                         0x0E
#defineSTART0x0B
#define WM8993_0FGHT_DAC_DIGITAL_V          1:893_RIGHT_ADC_DIGITAL_VOLUME             1
#define WM8993_S03
#define                     0x12
#define WM8993_GPIO1          WM8993_FLL_BCLK  2
#d4O_CTRL_1                      0x12
#define WM8993_GPIADDRefine WM8993_POWER_MANAG_H

F               efin7 WM8993_CLOCKING_2              MANAGEMENT_3               0_POWER_MANA_INPUT_1_2_VOLUME       0x18
#define WM89         0x04
#define WM899303
#define _INPUT_1_2_VOLUM
/*
 * Re2ister8values.
 */
#define 2M8993_SOFTWARE_RESET      OS           0x00
#define WM84IGHT_DAC_DIGI1C
#VOLUME               0x1C
efine WM8993_POWER_MANAGEME_VOLUME              0x1D
#define WM8993_LINEMANAGEMENT_3               01                  0x1D
#define WM8993_LINE         0x04
#define WM8993_AUDIO_INTERF      0x1D
#define WM8993_DELAYefine WM8993_POWER_MANA93_GPIO_CTRL_1   KMIX          0x12
#define WM8993_GPIOKMIXLMANAGEMENT_3               3
#define WMWM8993_SPKMIXR_ATTENUATION              0x2         0x04
#define WM899            WM8993_SPKMIXR_ATTENUATION                          0x17
#define WM8993_LEFT_LINE26
#UT_1_2_VOLUME       0x18
#define      3_LEFT_LINE_INPUT_3_4_VOLUME       0x1       0x27
#define WM8993_INPUT_MIXER2  UME      0x1A
#define WM8993_RIGHT_LINE       0x27
#def
/*
 * Re3ister9values.
 */
#define 3M8993_SOFTWARE_RESET     ABOREFT_LINE_INPUT_3_4_VOLUMEx023_POWER_MANA_INPUT    0x2B
#define WM8993_INPUL_ATTENUATION                0x2C
#define WM8993_OUTPUT_MIXER1                               0x28
#de90x2C
#define WM8993_OUTPUT_MIXER1                     0x04
#define WM899AUDIO_INTERne WM8993_OUTPUT_MIXER1                    0x00
#define WM8993_POWER_MANA      PUT_MIXER5                  L_ATTENUATION             UT_MIXER6                    0x32
#define WM89933
#define WM8993_SPKOUT_MIXERS                        0x32
#define WM89934                    0x30
#define WM89                0x32
#define WM8993e WM8993_CLOCKING_1       3           ADDITIONAL_efin5 WM8993_CLOCKING_2         ADDITIONAL_C  0x07
#define WM8993_AUDIO_INTEe WM8993_ANTIPOP1                         0x38
#define  WM8993_FLL_BCLK  2
#6                  0x39
#define WM
/*
 * Re4isterAvalues.
 */
#define 4M8993_SOFTWARE_RESET     BUSY          0x00
#define WM8900AUDIO_INTER#defi                  0x3D
#defdefine WM8993_SPEAKER_VOLUME                0x3E
#define WM8993_FLL_CONTR                   0x28
#define WM8993_    0x3E
#define WM8993_FLL_CONTR         0x04
#define WM8993               0x3E
/*
 * Re5isterBvalues.
 */
#define 5M8993_SOFTWARE_RESET     CURRENTIONAL_CONTROL             0x37
#defin93_BUS_CONTROTIPOP1                         093_BUS_CONTROL                   _POWER_MANAfine WM8993_WRITE_SEQUENCER_0                0x46
#define W WM8993_FLL_BCLK  28993_FLL_COfine WM8993_WRITE_SEQUENC
/*
 * Re6isterCvaluCharge Pump993_LEFT_DAC_DIGITAL_VCP               0x00
#define W    8IGHT_DAC 0x4A
#R_4                0x4A
efine WM8993_POWER_MANAGEENCER_5                0x4B
#define WM8993_CHMANAGEMENT_3               0x 10x09
#          0x4B
#define WM8993_CH         0x04
#define WM8993_CLK_FLL           
/*
 * R8     518993_lass W WM8993_SOFTWARE_RESETCP_DYN_FREQM8993_H
#define WM8993_H

extern s        0x5VO_3                       0xuct snd_soc_codec_device soc_codecK_0              0x58
#define WM8993_DC_S    1
#define WM8993_SYSCLK_FLL  K_0              0x58
#define WM8993_DC_S WM8993_FLL_BCLK  2
#define WM899K_0              0x58
#define WM8993V           0x00
#define WM893F
#defi2        
#define WM8993_EQ2       efine WM8993_POWER_MANAGEMEN#define WM8993_EQ3                          MANAGEMENT_3               0x         993_EQ3                                   0x04
#define WM8993_AUDIO_    0x66
#de     0x55#defi54valuDC Servo WM8993_SOFTWARE_RESETDCS_TRIG_SINGLE_11                 2IGHT_DAC                  #define WM8993_EQ8                 define WM8993_SP9
#define WM8993_EQ9                              0x6A
#define WM                   1UME   M8993_EQ9                              0x6A
#define WM WM8993_FLL_BCLK  2
K_FLL M8993_EQ9                              0x6A
#define 01                 1define WM8993_EQ9       0define WM8993_EQ14                  M8993_EQ10        efine WM8993_EQ15                             0x70
#define WM89           0x6C
#defxtern 993_EQ15                             0x70
#define WM89993_EQ13                             0x6E
                     0x70
#defERIES               0x69
993_OUTPU                  ine WM8993_EQ20                    M8993_EQ10        ine WM8993_EQ21                             0x76
#define WM8993                   0      3_EQ21                             0x76
#define WM8993993_EQ13                                                     0x76
#define WM89            0x6F
#d         OL_1              e WM8993_DRC_CONTROL_1             3_EQ22              WM8993_DRC_CONTROL_2                    0x7C
#define WM8993_D                   003
#deDRC_CONTROL_2                    0x7C
#define WM8993_DDIGITAL_PULLS                    0x7A
#def                 0x7C
#define     UP               0xEQ4 2M8993_DRC_CONTRO* R0 (0x0Field Definitions.
 */

/*
 * R0 (0xL_1                t
 */
#define WM8993_SW_RESET_MASK                    0xFFFF  /                   0x09
#define WM8993_SW_RESET_MASK                    0xFFFF  / WM8993_FLL_BCLK  2                993_SW_RESET_MASK                    0xFFFF             0x6F     
 */
#define WM8993_SW_* Field Definitions.
 */

/*
 * R0 (0_DRC_CONTROL_3                    0x2000  /* SPKOUTR_ENA */
#define WM8993_SPKOUTR_                                0x2000  /* SPKOUTR_ENA */
#define WM8993_SPKOUTR_SW_RESET - [15:0] */

/*
 * R1 (0x01) - Po                 0x7C
#defineDAC_WR           0x75
#defi00M8993_REGISTER_          WM8993_SPKOUTL_ENA                3_EQ22             KOUTL_ENA */
#define WM8993_SPKOUTL_ENA_MASK                 0                   0ine WM8993_EQ12ine WM8993_SPKOUTL_ENA_MASK                 0993_EQ13                           ine WM8993_SPKOUTL_ENA_MASK                        0x7B
#define00   13  /* SPKOU HPOUT2_E                 0x0800  /* HPOUT2_0x1000  /* SPKOUTL_EN_HPOUT2_ENA_MASK                  0x0800  /* HPOUT2_ENA */
#                   0e WM8993_EQ18  K                  0x0800  /* HPOUT2_ENA */
#KOUTL_ENA */
#define WM8993_HPOUT2_ENA                0x0800  /* H3_CHCHAN               0x6993_H

extern fine WM8993_HPOHPOUT1L_ENA */
#define WM8993_HPdefine WM8993_SPEAK            0x0200  /* HPOUT1L_ENA */
#define WM8993_HPOUT1                   0x28          200  /* HPOUT1L_ENA */
#define WM8993_HPOUT1 WM8993_FLL_BCLK  2
#de      1  /* HPOUT1L_ENA */
#define WM8993_HPOUT1R_EN            0x6F
93_EQ4        HPOUT1R_ENA_MAS*/
#define WM8993_HPOUT1R_ENA_MA1L_ENA_SHIFT            0  /* HPOUT1R_ENA */
#define WM8993_HPOUT1R_ENA_SHIFT                    0x28M8993_DRC_T1R_ENA */
#define WM8993_HPOUT1R_ENA_SHIFT                      0x0100  /* HPOUT1R_EN/
#de     0x5593_MW55           0x93_LEFT_DAC_DIGITAL_V    7A
#defNO_0T1L_ENA_SHIFT     93_GEM8993_DRC_ */
#define        5M8993_CLOCKING_2    _ENA */
#define W                   0x*/
#define             5  /* MICB2_ENA */
#define WM8993_MICB2_ENA_ WM8993_FLL_BCLK  2
#7      1  /* MICB2_ENA */
#define WM8993_MICB1_ENA    TIMER_PERIODne WM8993_MICB2_EM8993              0x0010  /* efin3 WM8993_CLOCKING_2            0x0010  /* M                  M8993_DRC_CCB1_ENA_SHIFT                       4  /* MICB1_ENA */
#defin WM8993_FLL_BCLK     13  /* SCB1_ENA_SHIFT                  0x557_MASK    0       0x       0x2B
#define W           VALUT1L_ENA_SHIFT     0xFGPIO_CTR   1  /* VMID_SE     5    0x12
#define WM899   1  /* VMID_SEL                   0xM8993_REGI_SEL_WIDTH                        2  /* VMID_SEL - [2:1]  WM8993_FLL_BCLK  2
#S_ENA                         0x0001  /* BIAS_ENA */
#define WM89T                 WM8993_LEFT_BIAS_ENA_SHIFT UT_1_2_VOLUME       0x18
#d_BIAS_ENA_SHIFT    0x07
#define WM8993_AUDIO BIAS_ENA */
#define WM8993_BIAS_ENA_WIDTH                  _BIAS_ENA_MASK                    0x0001  efine WM8993_     0x558_MASKfine        0xReadbackx68
#define WM8993_EQ8    26
#PATH
#define WM8993_FLL_C     0x1E
#HUT_ENA */
#define     0x4000  /* TSHUT_ENA */
#definM8993_EQ10        _SHIFT                      14  /* TSHUT_ENA */
#define WM8993_           0x6C
#def   13  /*                14  /* TSHUT_ENA */
#define WM8993_993_EQ13                                     14  /* TSHUT_ENA */
#993_NELuct snd_soc_codec_device3SHIFT       OPDIS_S     3   0x0C
#define WM8993_HUT_OPDIS_SH                   0x28
#ne WM8993_EOPDIS */
#define WM8993_TSHUT_OPDIS_WIDTH        WM8993_FLL_BCLK  2
#definOPDIS */
#define WM8993_OPCLK_ENA                  AL_COMPLETE                    3  /* TSHUT_O  /* OPCLK_efin9                2  /* VMID0  /* OPCLK_E*/
#define WM8993_BIAS_ENA     SHIFT                      11  /* OPCLK_ENA */
#define W WM8993_FLL_BCLK  2
#OPDIS */
#dHIFT                      11  /* OPCLK_ENA       * OPCLK_ENA */
#define     */

/*
 * R2 (0x0T           5:4define WM8993_MIXINL_ENA_MASK                                  0x2000 /
#define WM8993_MIXINL_ENA_SHIFT                      9  /* WM8993_FLL_BCLK         0x03_MIXINL_ENA_WIDTH                      1  /* 01) - Po* OPCLK_ENA */
#definM8993ine WM8993fine WM8993_MIXI     WM8993_BIAS_ENA_WIDTH     fine WM8993_MIXIN                            0x0100  /* MIXINR_ENA */
#define WM8993_MIXINR_ENA_SHIFT        WM8993_FLL_BCLK        0x0_ENA */
#define WM8993_MIXIN     0x559_MASK_MIXENA_MASK                0x0020  /* MICB2_ENAINTEG3_HPOUT1L_ENA_SHIFT           0  /* BIA            fine WM8993_BIAS_ENA_WIDTH                        1  /* BIAS_ENA */

/*
 * fine WM8993_IN2L_ENA_SHIFT                        7  /* _BIAS_ENA_MASK                 fine WM8993_IN2L_ENA_SHI
/*
 * R9giste5e WM8NA_MASK           UT_VOLUME                                                   0  /* BIA040  /* IN1LIN2L_ENA_SHIFT                        7  /        1  /* BIAS_ENA */

/*
 * SHIFT                        6  /* IN1L_ENA */
#define Wine WM8993_IN1L_ENA                       HUT_ENA */
#define WM9fine 60valuAnalogue HP WM8993_SOFTWARE_RESETHPOUT1_AUTO_PU     0x00
#define WM8993_POWERR_ENA */
#defin    0x0020  /* IN2R_ENA */
#defi1L_ENA_SHIFT          FT                        5  /* IN2R_ENA */
#define WM89                   0x2803
#de                  5  /* IN2R_ENA */
#define WM89                     0x0100                    5  /* IN2R_ENA */
#deL_RM WM8NPUT_MIXER6        M8998            NA_SHIFT    */
#define WM8993_IN1R_ENA_SHIFT   0x1000  /* SPKOUTL_E 4  /* IN1R_ENA */
#define WM8993_IN1R_ENA_WIDTH                                0B1_ENAIN1R_ENA */
#define WM8993_IN1R_ENA_WIDTH             993_EQ13                  IN1R_ENA */
#define WM8993_IN1R_ENA_WIDTH   OUTP8993_H
#define WM8993_H

44  /* IN1R_ENAADCL_                    1  /* ADCLdefine WM8993_SPEAKER3_ADCL_ENA_WIDTH                        1  /* ADCL_ENA                    0x28
#8993_FWIDTH                        1  /* ADCL_ENA  WM8993_FLL_BCLK  2
#defiM8993_ADCL_ENA                     1  /* ADCLDLfine WM8993_FLL_CONTROL15:0] */
#d */
#define     0  /* ADCR_ENA */
#defineuct snd_soc_codec_device s                    1  /* ADCR_ENA */

/*
 * R3 (                   0x28
#d0x09
#             1  /* ADCR_ENA */

/*
 * R3 ( WM8993_FLL_BCLK  2
#define WM89             1  /* ADCR_ENA */

/*
 RA_SHIFT                       OUTL_ENENA_SHIFT          WM8993_LINEOUT1N_ENA_SHIFT        0x1000  /* SPKOUTL_ENA */
#ENA */
#define WM8993_LINEOUT1N_ENA_WIDTH              /* SPKOUTL_ENA */
#defineENA */
#define WM8993_LINEOUT1N_ENA_WIDTH             DCL_ENA */
#define WM8993_ADCL_E
#define WM8993_LINEOUT1N_ENA_WIDTH   ADCL_ENA */
#define WM8993_AD2_ENA_S           12P_ENA_SHIFT                  1 */
#define WM8993_ADCR_Eefine WM8993_LINEOUT1P_ENA_WIDTH                                      0x28
#xtern M8993_LINEOUT1P_ENA_WIDTH                   ne WM8993_ADCR_ENA_SHIFT             LINEOUT1P_ENA_WIDTH              ine WM8993_ADCR_ENA_WIDTH    extern             OUT2N_ENA_SHIFT              uct snd_soc_codec_device soc_codene WM8993_LINEOUT2N_ENA_WIDTH                 1
#define WM8993_SYSCLK_FLL ne WM8993_LINEOUT2N_ENA_WIDTH                      0x2000  /* LINEOUT1N_ENA */
#993_LINEOM8993_IN2993_T62valuEQ93_LEFT_DAC_DIGITAL_V                 0x00
#define W    8  /* HPOANAGEMENT_1              x01
#define WM8993_POWER_MANAGE93_LINEOUT2P_ENA_WIDTH                   1  /               0x51
#define WM  /* MIENA_WIDTH                   1  /x54
#define WM8993_DC_SERVO_1        ANAGEMENT_M8993_IN2 0x0063HIFT  UT_VOLUME                1_GAINL_ATTENUATION                     KRVOL_ENA efine WM8993_CLOCKING_2    KRVOL_ENA *                   0x28
#de WM8993_S                   1  /* SPKRVOL_ENA */
#defi         0x04
#define WM8990x09
#          0x0100  /* S
/*
 * R93_PIFT      EQ       0x2B
#define W    2_ENA */
#define WM8993_SPKRVOL_ENA_WIDTH                     1  /* SPKRVOL_ENA *       ne WM8993_SPKLVOL_ENA                993_SPKLVOL_ENA_WIDTH                     1 93_SPKLVOL_ENA_MASK                 0993_SPKLVOL_ENA_WIENA */
#de     6     EQ2                        3_ENA */
#define WM8993_SPKRVOL_ENA_WIDTH   993_MI            1  /* SPKRVOL_ENA *993_MIXne WM8993_SPKLVOL_ENA                 /* MIXOUTLVOL_ENA */
#define WM8993_MIXOUTL93_SPKLVOL_ENA_MASK                 0 /* MIXOUTLVOL_ENAENA */
#de1B
#d6 valuEQ         0x43
#define    4_ENA */
#define WM8993_SPKRVOL_ENA_WIDTH   40  /*            1  /* SPKRVOL_ENA *40  /* ne WM8993_SPKLVOL_ENA                UTRVOL_ENA_SHIFT                  6  /* MIXO93_SPKLVOL_ENA_MASK                 0UTRVOL_ENA_SHIFT  ENA */
#de_INPU6    0EQ6A_MASK              0x005_ENA */
#define WM8993_SPKRVOL_ENA_WIDTH   UTL_EN            1  /* SPKRVOL_ENA *UTL_ENAne WM8993_SPKLVOL_ENA                UTL_ENA */
#define WM8993_MIXOUTL_ENA_SHIFT 93_SPKLVOL_ENA_MASK                 0UTL_ENA */
#defineENA */
#de#defi6fine EQ7            9  /* SPKRVOL_CHARGE_PUMP_1              ine 93_LEFTR_ENA *             1  /* SPKRVOL_ENA */
0x0200  /* SPKRVOL_ENA */
#defin                   0x0010  /* MIXOUTR_ENA */
#def         0x04
#define WM8993_A8993_F             4  /* MENA */
#de93_MW6_MIXEEQ8            9  /* SPKRVOL_B/
#define WM8993_MIXOUTR_ENA_MASK          B      0x0010  /* MIXOUTR_ENA */
#dBfine WM8993_MIXOUTR_ENA_SHIFT              _DACL_ENA_MASK                    0x0M8993_MIXOUTR_ENA_WIDTH                   _DACL_ENA_MASKENA */
#deR_ENA_e WM8EQ9            9  /* SPKRVOL_PGefine WM8993_POWER_MANAGEME_MASK          PG      0x0010  /* MIXOUTR_ENA */
#dPGWM8993_EQ5                          CR_ENA */
#define WM8993_DACR_ENA_MASK       M8993_CLOCKING_4                          */
#define WM8ENA */
#deM89936SLAVEEQ1WM8993_SOFTWARE_RESETOUTLVO*/
#define WM8993_MIXOUTR_ENA_MASK        2        4  /* MIXOUTR_ENA */
#defiR4 (ine WM8993_MIXOUTR_ENA_SHIFT            WM8993_AIFADCL_SRC                     M8993_MIXOUTR_ENA_WIDTH                 WM8993_AIFADCL_SENA */
#deP_ENA_M8993EQ1                10  /* LB2        0x0002  /* DACL_ENA */
#define WM892NA_SHIFT                        1 M899 0x8000  /* AIFADCL_SRC */
#define WM8993_ /* AIFADCL_SRC */
#define WM8993_AIF00  /* AIFADCL_SRC */
#define WM8993_AIFAD                       1SHIFT D 15  /*             9  /* SPKRVO2_C93_AIFADCL_SRC_WIDTH                     1 C/* AIFADCL_SRC */
#define WM8993_ACFADCR_SRC                      0x4000  /* A_WIDTH                     1  /* AIFA00  /* AIFADCL_SRC */
#define WM8993_AIFAD_WIDTH        ENA */
#d1fine WE 15  /*PKLVOL_ENA_SHIFT                                    0x0001  /* DACR_EHIFT/* AIFADCL_SRC */
#define WM8993_A                0x0001  /* DACR_ENA */
#de/
#define WM8993_AIFADC_TDM_WIDTH                 0  /* DACR_ENA */
#define WM89/
#define WM8993_000  /* AI0080  F 15  /*UTLVOL_ENA */
#define WM899*/
#define WM8993_MIXOUTR_ENA_MASK        3        4  /* MIXOUTR_ENA */
#defiN */ine WM8993_MIXOUTR_ENA_SHIFT                         12  /* AIFADC_TDM_CHAN */
M8993_MIXOUTR_ENA_WIDTH                              12 000  /* AI1B
#d7MASK EQ1NA_MASK              0x003        0x0002  /* DACL_ENA */
#define WM893AIFADCR_SRC */
#define WM8993_AIFA    #define WM8993_AIFADC_TDM_CHAN_WIDTH      8993_BCLK_DIR_SHIFT                  define WM8993_BCLK_DIR                                           1_INPU7ne WMEQ1NA */
#define WM8993_MIXO3 /* AIFADCR_SRC */
#define WM8993_AIFADCR_S3                  0x2000  /* AIFAD_AIF#define WM8993_AIFADC_TDM_CHAN_WIDTH      IF_BCLK_INV */
#define WM8993_AIF_BCLdefine WM8993_BCLK_DIR                               0x2000  /* AI#defi7SHIFT       0x0010  /* MIXOUTR_EN3IFT                     13  /* AIFADC_TDM */* AF_BCLK_INV */
#define WM8993_AIF_B                0x0001  /* DACR_ENA */
#de_INV_MASK               0x0080  /* AIF_LR         0  /* DACR_ENA */
#define WM89_INV_MASK                0x93_MW7       1_DACL_ENA                4 */
#define WM8993_AIFADC_TDM_CHAN_SHIFT   4        4  /* MIXOUTR_ENA */
#defiMASKine WM8993_MIXOUTR_ENA_SHIFT            - [6:5] */
#define WM8993_AIF_WL_SHIFT M8993_MIXOUTR_ENA_WIDTH                 - [6:5] */
#defi        0xfine 7M8993_S1 */
#define WM8993_DACR_E4        0x0002  /* DACL_ENA */
#define WM894LK_DIR_WIDTH                       AIF                         5  /* AIF_WL - [6FT                         3  /* AIF_                       2  /* AIF_WL - [6:5V                     0xM89937/* MIXO21  /* DACR_ENA */

/*
 * 4 /* AIFADCR_SRC */
#define WM8993_AIFADCR_S4BCLK_INV */
#define WM8993_AIF_BCL0x80                         5  /* AIF_WL - [6CL_SRC_MASK                 0x8000  /                       2  /* AIF_WL - [6:5           0x2000  /* AI993_T7TRVOL_E2 AIFADCL_SRC */
#define W4IFT                     13  /* AIFADC_TDM *ACR_L_SRC_MASK                 0x8000                 0x0001  /* DACR_ENA */
#deACR_SRC */
#define WM8993_AIFDACR_SRC_MAS         0  /* DACR_ENA */
#define WM89ACR_SRC */
#defin        0x 0x007XOUTL_E2             9  /* SPKRVO5 */
#define WM8993_AIFADC_TDM_CHAN_SHIFT   5        4  /* MIXOUTR_ENA */
#defiSRC ine WM8993_MIXOUTR_ENA_SHIFT                       0x2000  /* AIFDAC_TDM */
#deM8993_MIXOUTR_ENA_WIDTH                            0x200ENA */
#d2fine 7       2PKLVOL_ENA_SHIFT         5        0x0002  /* DACL_ENA */
#define WM895TH                         2  /* A    fine WM8993_AIFDAC_TDM_MASK               _AIFDAC_TDM_CHAN                  0x1M8993_AIFDAC_TDM_SHIFT                                           2     7 WM89932UTLVOL_ENA */
#define WM85IFT                     13  /* AIFADC_TDM *defiAIFDAC_TDM_CHAN                  0                0x0001  /* DACR_ENA */
#de        1  /* AIFDAC_TDM_CHAN */
#define          0  /* DACR_ENA */
#define WM89        1  /* AIF_SHIFT     */
#dfine Wigital PullsM8993_SOFTWARE_RESETMstrufine WM8993_IN2R_ENA_            4  /*         AC_BOOST_WIDTH           /
#define WM8993_MIXOUTR_ENA- [11:10] */
#define WM8993_DAC_COMP                      0x51
#define WMB1_ENA */
#define WM8993_DAC_COMP       x54
#define WM8993_DC_SERVO_1 K_FLL  */
#define WM8993_DAC_COMP     DUT2P_ENA */
#define WM8993_LINCL_ENA_COMP_WIDdefine WM8993_DAC_COMP_WI                   0x0010  /* /* DAC_COMP */
#define WM8993_DAC_COMPMOD               0x51
#define WM8993_FCOMP */
#define WM8993_DAC_COMPMODT                        4  /* DAC_COMP *//
#define WM8993_DACDACDAT               2  /* DAC_BO15:0] */
#deine WM899AC_COMPMODE */
#define WM89define WM8993_SPEAKER_VOLUME             1  /* DAC_COMPMODE */
#define WM                   0x28
#def*/
#def     1  /* DAC_COMPMODE */
#define WMM8993_CLOCKING_4                     0x0004  /* ADC_COMP */
#define WMIDTH                                 ine WM899/* ADC_COMP */
#define WM89define WM8993_SPEAKER_VOLUME             1  /* ADC_COMP */
#define WM8993                   0x28
#def   13       1  /* ADC_COMP */
#define WM899393_ADC_COMP_SHIFT                        2 /* ADC_COMP */
#defLR                  2  /* DAC_BOOTL_ENA */
#E */
#def 1  /* ADC_COMPMODE */
#de     0x64
#define WM8993_EQ4                      1  /* ADC_COMPMODE */
MANAGEMENT_3               0xUME                 1  /* ADC_COMPMODE */
         0x04
#define WM8993_AUDIO_              1  /* ADC_COMPMODE *IDTH                      M8993_HPOUTPBACK */
         0  /* LOOPBACK */     0x64
#define WM8993_EQ4                          1  /* LOOPBACK */
MANAGEMENT_3               0xxtern                   1  /* LOOPBACK */
#define WM8993_LOOPBACK_SHIFT                        1  /* LOOB                  2  /* DAC_BOOST - extern E_SHIFT e WM8993_TOCLK_RATE_SHIFT                   0x0010  /* CLK_RATE */
#define WM8993_TOCLK_RATE_WID               0x51
#define WM8K_RATE */
#define WM8993_TOCLK_RATE_WIDT                        4  /* DAC_CE */
#define WM8993_TOCLK_RATE_WIDTH                        1   /* HPO_TOCLK_E */
#define WM8993_TOCLK_DTH                      1  /* /* TOCLK_ENA_ENA */
#define WM8993_TOCLKine WM8993_MIXOUTR_ENA_SHIFT        /* TOCLK_ENA */
#define WM8993_OPC_MASK                   0x4000  /* TOCLK_E_ENA _SHIFT    0100  SLAVEDRC Controline WM8993_IN2L_ENA_MASRCLINEOUT2P_ENA */
#define WM899CER_5      DTH     M8993_OPCLK_DIV_WIDTH    /
#define WM8993_MIXOUTR_ENALK_DIV - [12:9] */
#define WM8993_DCLK_DIV_MANAGEMENT_3               0x8993_DC[12:9] */
#define WM8993_DCLK_DIV_T                        4  /* DAC_C[12:9] */
#define WM8993_DCLK_3_MI*/
#ine WM8993_RIGHT_OUTPUT_VOLUME  - [8:6] */
#d   3  /* DCLK_DIV - [8:6] */
# */
#define WM8993_ADCRMASK                    0x001E  /* BCLK_DIV - [4:1] *                   0x28
      0x              0x001E  /* BCLK_DIV - [4:1] *ne WM8993_ADCR_ENA_SHIFT       IDTH                        4  /* BSMOOTHef WM8993_H
#define WM80x08SK         C          #define WM8993_MCLK_SRC         1L_ENA_SHIFT            /* MCLK_SRC */
#define WM8993_MCLK_SRC_MASK                              0x21) - ClockiC */
#define WM8993_MCLK_SRC_MASK                                0x0100  /ine WM8993_MCLK_SRC_WIDTH           QR               0x00
#define0x04SK           0x400C                       0x40L_ATTENUATION             ne WM8993_SYSCLK_SRC_MASK                  0x400    1
#define WM8993_SYSCLK WM8993_SYSCLK_SRC_MASK                  0x4004                    0x30
#define3_SYSCLK_SRC_MASK                ANTICLI0x4A
#define WM8993_WR                 0x1000  /*                         0x1000  /*      0x0080  /* IN2 WM8993_MCLK_DIV_MASK                    0x1000  /* MCLK_DIV                   0xWM8993_CLK_DIV_MASK                    0x1000  /* MCLK_DIV WM8993_FLL_BCLK  2
# WM8993_MC_DIV_MASK                    0x1000HYSTef WM8993_H
#define WM899            ine WM8993_M/* MCLK_INV */
#define WM8993_ */
#define WM8993_ADCR_         0x0400  /* MCLK_INV */
#define WM8993_MCLK_                   0x28
#M8993_R0x0400  /* MCLK_INV */
#define WM8993_MCLK_ [4:1] */

/*
 * R7 (0x07) - Clocki400  /* MCLK_INV */
#define WM8THRESH_DIV -0x10
#define WM8993_* MIXINL_3_ADC_DIV_SHIF993_MIXINL_ENA_SHIFT        3_ADC_DIV_SHIFT                   0x2_DIV_WIDTHDC_DIV - [7:5] */
#define WM8993_ADC_DIV_WIDTH         WM8993_FLL_BCLK  2
#d       V - [7:5] */
#define WM8993_DAC_DIV_MASK     MINENA */
#define WM8993_SPKRM8993CWM8993_MC             e WM8993_TSHUT_OPDIS_W                              0x28
#d - [4:2] *e WM8993_DAC_DIV_WIDTH                        x0800  /* OPCLK_ENA */
#define WMR8 (0x08) - Audio Interface (3)
 */
#definAX          2  /* DAC_DIV - [4:2]ine WM893_AIF_MSTNR_ENA */
#define WM8993_MIX93_AIF_MSTR                   0x28
#dK         AIF_MSTR1 */
#define WM8993_AIF_MSTR1_SHIFT   3_AIF_MSTR1                        0xIF_MSTR1 */
#defin_SHIFT    F_LRCLM89939] */
#definSK                        TTACK_RAK_ENA */
#define WMx000ASK                0x20       ne WM8993_TSHUT_OPDIS_W          0x200E                0x0D
#defin8993_AIF_TRIS_MASK                    0x2000  /* AIF_TRIS */8993_GPIOCTRL_2             8993_AIF_TRIS_MASK                    0x2000  /*DECAY 0x2000  /* AIF_TRIS */     0x22
#dCLK_DIR                 0x12
#define WM899CLK_DIR        R_ENA                        CLK_DIR */
#define WM8993_LRCLK_DIR_MASK                  WM8993_FLL_BCLK  2
#de          DIR */
#define WM8993_LRCLK_DIR_MASK      DC_DIV_* OPT                     F */
#definK_DIR */
#dUT_1_2               0x2000  /*K_DIR */
#de                   0x2IS_SHIFT               0x07FF  /* LRCLK_RATE - [10:0] */
#define WM8993_FLL_BCLK  2
#d8993_F                 0x07FF  /*_SHIFT    AIF_LRR_SRC9] */
#defin                      993_MM8993_ */
#define WM8993_ADCRF  /* MCLK_SRM8993_DAASK     1993_LRCLK_DIR_MASK      M8993_DAC1] */
#define WM8993_BCLK             /* DAC_OSR128 */
#define WM8993_DAC_OSR128_MASK WM8993_FLL_BCLK  2
#defi        6 R128 */
#define WM8993_DAC_OSR128_SHIFT  R0_SLOPE*/
#define WM8993_LRC0x07  /* LRCLK_        1  /*     0M8993_LRCLK_DIR_MASK              1  /*      0x7E

#define WM8993_R3_DAC_MONO                         0x0200  /* DAC_MONO */
#def WM8993_FLL_BCLK  2
x8000  /* AC_MONO                         0x0200  /* DAFF993_SPE             0x0010  /* DAC_COMO_WIDTH      ine WM8993_DAC_MONO_WIDTH      */
#define WM8993_ADCR_EDAC_MONO */
#define WM8993_DAC_SB_FILT                                0x28
#B1_ENA  */
#define WM8993_DAC_SB_FILT              [4:1] */

/*
 * R7 (0x07) - Clocki
#define WM8993_DAC_SB_FILT    DC_DIV_Q            0x17
#def[4:2] */
#defin         3_DAC_DIV_WIDTH                                             0x28
FT               8993_DAC_MUTERATE                     0x008 WM8993_FLL_BCLK  2
#defefine WM8993_DAC_MUTERATE_MASK                0x000x200             1  /* DAC      0x8000  /* C_MUTERR1 */
#define WM8993_AIF_MSTR1_C_MUTERA                   15  /* AIF_MSTR1 DTH                    1  /* DAC_MUTERATE */
#3_AIF_MSTR1                        0DTH                _SHIFT    FMT_MADM */9] */
#defin2                         1      1  /* DAC_OSR128 */
#deEASK                      ASK     3         1  /* DAC_MUTERA            6           0x6C
#define WM8fine WM8993_DAC_UNMUTE_RAMP_WIDTH                 1  /* DAC_UNMO_SHIFT                         WM8993_DAC_UNMUTE_RAMP_WIDTH                _ENA */
ENA */
#define WM89930x10  /* LRCLK_#define WM89     2M8993_LRCLK_DIR_MASK      #define WM899*/
#define WM8993_BIAS_ENA              2  /* DEEMPH - [5:4] */
#define WM8993_DAC_MUTE ACE_4                0x09
#004  /* DAC_MUTE */
#define W
#endif
