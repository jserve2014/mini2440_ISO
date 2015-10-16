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
#define W89930x0002  /* CL#ifndef WM*/#define WM89933_struct snd__MASKH
#define WM8993_H
#
extern 
extern strsoc_dai wmdevicdai;soc_codec_deSHIFT8993_H
#define WM89931_codec_dev_wm8993;

#define WM8993_SYSCLK_MCLK WIDTd_soc_codec_devicSYSCLK_FLL 993_F2
#define 
/*
 * R70 (0x46) - Write Sequencer 0
3;

#define WM8993_WSEQsnd_s993_H
#define WM89933evice 100_codeR_MANAGEM 2
#dOFTWARE_RESETR_MANAGEev_wm8993;

codec_devidec_devicPOWER_MANAGEMENT_1R_MANAGEME WM890x01#def WM8OWER_LL_BCLK  2
#defin   8               0x02
#define WM8993_POWER_M       FLL_BCLKFLL_MENT_3fine WM89          0x02
#define WM8993_POWRITE_INDEXefine WM8993_POWERH

1FUDIO_INTERMENT_3      - [4:0]E_22
#define WM89993_P5#define WM899MANAGEMENT_3         2                 6_soc_codec_devicCLOCKING993_CLOCKING_2    INTERF        0x04
#define5A          FACE_393_AUDIO_INTERFA
/*
 * Re1ister7values.
 oc_dai wm891K  2
#d              0x01DA  0x01
#efine WM8993_POWER0x70_2          R_MANAGEMENT_314:12     0x08
#define WM8993_AUDIO_INTE0MANAGEMENT_3         1c_code_DIGITAL_VOLUMEING_2       C_soc_codec_devic WM8993_SIDE_TON         0x04
#define 3ENT_3         ADC_CTR8993_FL993_DIGITAL_SIDE_TONx0E#define START0x0B_soc_codec_devic0FGHT_DACe WM8993_A8993_RIGHT1:893_RIPIO_0F
# WM8993_ADC_CTRL       MANAGEMENT_3          03#define W8993_RIGHT_ADC_DIGITAL1ine WM89         GPIO02
#define         0x04
#define 4O#defix02
#define WM899OUNCE                     0x14ADDRNT_3                        FWM8993_IRQ_DEBONT_37       0x08
#define WM8993_AUDIO         0x93_DAC_CTRL     0x17
#define_INPUT_1_2_ADC_CTRL     0x18               0x2       4    0x00
#defin3
#define W9
#define WM8993         2     8   0x0A
#define WM89 0x0C
#               0x01
OSCKING_2       0#define WM89412
#dCTRL_1  1C
#_GPIO1             _LINEC
           0x17
#define EME3_GPIO1             LINED_soc_codec_devicLINE93_LEFT_LINE_INPUT_3_4_VOLUM        0x16
#defin3_HPOUT2_VOLUME         UME      0x1A
#define WM8993_x09
#define VOLUME                 0x2DELAY           0x17
#define  0x14
          KMIXDEBOUNCE                     0x14
WM89L93_LEFT_LINE_INPUT_3_4_VOLU
#define WWMLK  2
#dPWM89R_ATTENUATIONFT_OPGA_VOLUME  93_CLOCKIN0x1A
#define WM899UDIO_INTERFACE_4       0x24
#define WM8993_SPKOUT_BO_OPGA_VOLUME   7OUT2_VOLUME      EFT     26
#efine WM8993_RIGHT_LINE_INPUT_1_2T_MIXEIGHT      9
#defi3_4WM8993_RIGHT_LINEKOUT_BOOSTSPEAKER_VOLUME_RI
#defiMIXER2  _CTRL    0x1A_soc_codec_devicx12
#d0
#define WIXER3             3     9   0x0A
#define WM893T_VOLUME               0xABOR               0x28
#defix020x17
#define         0x2A10
#define WM8993
#deLME_LEFT              0x26
#d0x2               0xOUT    0x29
        0x16
#define WMe WM8993_OUT_INPU9OUTPUT_MIXER2                    0x2E
#define WM8993_O0x1A
#define WM899x09
#defineER2                    0x2E
#define WM8993_993_RIGHT_OUTPUT  0x17
#defineUTPUT_    0x29
5WM8993_IRQ_DEBOUNC         0x2D
#define WM89   0x29
68993_RIGHT_ADC_DIGITAL3                ERS         SPEAKER_O   0x29

#define WM8efine WM8993_LINE_MIXER1        40x33
#define WM8993_LINne WM8993_OUTP_MIXER2                      0x35
#        0x08
#defi_INPUT_M93_DAC_CTRL ADDIe WMAL_NT_35       0x08
#define WM8993_e WM8993_ANC#defiSPEAKER_VOLUME_RIx09
#definP2        NTIPOP 0x2E
#define WM8993_OUTPU0x3INPUT_MIXER        0x04
#define 0x33
#define WM8990x39#define WM8         4     A   0x0A
#define WM894T_VOLUME               0xBUSY   0x31
#define WM8993_OUTP00x09
#defineMENT_993_FLL_CONTROL_1    POUT2_ENT_3          PEAKER   0x1E
#define WM8991    VOLUME         0x3A
#CONTR8993_MICBIAS                          08993_FLL_CONTROL_5                    0x400x1A
#define WM8993
#define WM8993_FLL_        5     B   0x0A
#define WM895T_VOLUME               0xCURREN#define     Oine WM8993_RIG0x3SPEAKER_93_BUSOL_1   9
#define WM8993_MICBIAS        fine WM8993_Wine WM8993_RIGHT_ADCx17
#define           0#definSEQUENCER_NE   _OUTPUT_MIXER34                 0x3A
#define                     0x47
#define WM8fine WM896     C   0Charge PumpE_RIGHT  CTRL_1       CP        0x31
#define WM8993_OTE_S8VOLUME  _2  A
#R_define WM8993_SPE     E_OUTPUTS_VOLUME         WM8993 0x32
#define WM80x410
#define WM8993CH93_LEFT_LINE_INPUT_3_4_VOLUMx 10x09
#define WM8993_CLASS_W_0         efine WM8993_RIGHT_OPGA_VOLUMine WM8993_FL             8     51TPUT_lass     0x48
_DAC_DIGITAL_VCP_DYN_FREQ           0x00
#defince soc_codecME      0x5VO993_DAC_CTRL      E      0xefine WM8993_POWER_MANce 8993_POWEK3_WRITE_SEQUENCE0x5_INPUT_1_2_VOL3_SPC_
#defGEMENT_3              e WM899
#define WM8993_DC_SERVO_READBACK_2               0x04
#define WM89L_5    
#define WM8993_DC_SERVO_READBACK_2            define WM8993_OUTP3Fne WM893_CLOCKIRVO_READBACK_2  EQ93_CLOCKE_OUTPUTS_VOLUME           N                    0x58
#define WM8993_DM8993_LEFT_LINE_INPUT_3_4_VOLUMx_MANAGEMENT_1               0x65
#define Wefine WM8993_RIGHT_OPGA_VOLUME           6     993_DC_S5_CLOC54   0DC Servo_SERVO_3              DCS_TRIG_SINGLE_1        0x16
#defi2_5       M8993_EQ9                         5
#defx6A
#define ROL_4                                        0x36
#dITE_SEQUENCER_2 6INPUT_MIXER4WM8993_IRQ_DEBOUNCE1_CTRL             0x6B
#define WM8993_EQ11                           0x04
#define WM89            0x6B
#define WM8993_EQ11                993_LEFT_OPGA_VOLUM1                    0x6B0                1define WM8993_SPEAK         _WRITE_SE                 0x32
#define WM899             7XER                    0x0x6      c_code          0x71
#define WM8993_EQ17                           #define WM8993_EQ6            0x6E
define WM8993_EQ17            ERIE8993_LINE_MIXER20x69
         M8993_EQ9                       _WRITE_SEQUENCER_ WM8993_EQ16                         0x2E
#define WM8993_OUTPUT_MI0x7                993_DIGITAL_SIDE_TONE             0x77
#define WM8993_EQ23                      3_EQ19                             0x7
#define WM8993_EQ23                            0x75
#ddefiA
#defineO          0x16
#d WM8993_SPRCOL_1    x02
#define WM8_EQ2493_CLOCKING_2               0x7C
#ne WM8993_AUDIO_INTER0x7               0x0fine WM8993_DRC_CONT3
#def      0x7D
#define WM8993_DRC_CONTROL_4                WM8993_PULL8993_LINE_MIXER2     0x7INPUT_ne WM8993_DRC_CONTROL_4            0x4A
#define WM899EQ4PUT_VOLU      0x7C* Rgiste0Field DLOCKition0A
#deffine WM8giste          0x16
#deft
#define WM89    0x34W      uct s8993_RIGHT_ADC_DIGITALFFFF  /            0x31
#defi                 W_RESET_SHIFT                        0  /*         0x04
#defiA
#define WM89933_SW_RESET_WIDTH                       16  /*       0x7B
#de93_EQ3*/
#define WM8993_SW_R* SET_MASK                    0xFFFF        0x7C
#        0x78
#defineWM89000tern 4
#defRef WMoc_dai wm8993_dai;_ENA */
S                    0x7A
#defin /* SPKOUTR_ENA */
#define WM8993_SPKOUTR_ENA_SHIW_RESET_efin15:0]         0xF1SW_RE1valuPone WM8993_DRC_CONTROL_4      CTRLW      0x40
#0x7       004      EGISTEHIFT            0x34
#defLef WM8993_SPKOUT_MIXERC_CONTROL_3      ENA_MASK  ine WM8993_SPKOUTR_ENA_SMASK SHIFT                    0x76
#define WM890              28993_SPKOUTL_ENA_SHIFT                    12 3_EQ19                             8993_SPKOUTL_ENA_SHIFT                    12    0x100010
#definSPKO     UTR_ENA  HPOUT2_3F
#define WM8993OCKI8SPKOUTR       0x1 SPKOUTR_ENA *MASK_              0x0800  /* HPOUT2_E /* HPOUT2_ENA */

#define /* SPKOUTL_ENA */
#            8                  11  /* HPOUT2_ENA */
#define ENA */
#define WM8993_SPKOUTENA */
#def            11  /* HPOUT2_E    CHA          0x26
#
#de_SERVO_READBA                    1T1L_ENA                   ROL_4                             2HPOUT2_ENA *ENA */
#define WM8993_HPOUT1L_E     0x40
#define WM899     0x69
# /* HPOUT1L_ENA */
#define WM8993_HPOUT1L_EN        0x04
#define W       * HPOUT1L_ENA */
#define WM8993_HPOUT1L_E/
#d        0x7B
#def0x66
define WMUT1R_ENA_MT    A                      1R_ENA */ENA */_    1          12  /HPOUT1L_E/
#define WM8993_SPKOUTRUT1R_ENA_SHI              8  define WM899
 */
#defi_ENA */
#define WM8993_HPOUT1R_ENA_WIDTH                     1 993_PO /* HPOUT1L_E/
#dc_dai         93_MW5 0x32
#defin0xNCER_4               /* ME

/*
 NO_0_ENA */DTH          0xE
 */
#defiefine WM8993  5  /*       x08
#define WM
#define ITE_SEQUENCE WM8993_EQ11                 5  /*   /* M* HPOMICB/
#define CLOCKING_3    fine WM89_                     70100  /* HPOfine WM8993_MICB1_ENA           1x0200  /TIMER_PERIODENA                          0x78
#d993_    /*     3       0x08
#define WM8993_AUD_ENA_SHIFT             0x6C
#d
 */
#defin                       0x0020  /* M 4/
#define      oc_dai wm                  _HPOUT2_ENA                  1  /* MICB1_E    7SHIFT    _WRITE_S         OUTPUT_MIXER1 TH         VALL_ENA */DTH        0xF 0x22
#d0  /* HPOVMID_Sdefine 0x32                  VMID_SEL_WIDTH  ine WM8993_RIGHT_ADC0xUTL_ENA */:1] _ WM89A
#define WM8993_DRC_CONxtern - [2:1] *- [2:1]       0x3A
#define WMdef WMe WM8993_RIGHT_ADC_DIGITAL_0993_/* BIAdef WMoc_dai wm8993_d                  LUME_RIGHT  ne WM899DTH    efine WM8993_RIGHT_LINE_INP BIAS_ENA */
#de1_ENA 3_ANTIPOP2            ine WM8993_BIAS_ENA_SHIFTfineIAS_ENA                0x0001  ement (2)
HIFT                       */
#dLOCKING_3        ine W8           5  /* 0xReadbackx6ERVO_READBACK_2        0    PA8993_FLL_BCLK  2
#     WM8993_LE
#HUTWM8993_MICB1_ENA WM8993 SPKOUTRTS                 993_EQ16                          0x0020  /* M    _ENA */
#define WM8993ENA */
#define    0x72
#defin_HPOUT2_EN        1  /* TSHUT_ENA */
#define WM8993_TSHUT_OPDIGITAL_PULLS                    0x7A
#defineSHUT_ENA */
#define WnageNELSERVO_READBACK_1        3            OPDIS        0x3                  0x    OPDIS *           0x0001  /M8993_0x4000  /* OPDISefine WM8993_SPKOUTA */
#OPDIS                      0x04
#define WM89define WM8993_OPCLK_ENA OP993_           0x0001  /*TROL_MPLET           0x0800 #defin_ENA */
#O_OPCLSK    
#def  0x0001  /* BIAS_ENA */
#SHIFT SK     oc_dai wm8993_dai;ne WM8993                        1  /* TS
#defiSK        oc_dai wm899x0800  /* OPCLK_ENA *define WM89            1  /* OPCLK_ENA */
#define WM89_MASK #define WM8993_MIXINL_ESK          0xF2SW_RE          8 5:4CB1_ENA         XIN         0x0800  /* HPOUT2_ENA */
   0x2000  /* SPK_MICB1_ENA                                 0x0020  /* M9_OPCLASK                        H                           0x0001  /* B
#defi*/
#defi#define WM8993_MIXINLManag93_TSHUT_OL_ENA_WIDTH        0x0800  /ment (2)
 */
#definL_ENA_WIDTH                        13  /* SPKOUB2_ENA */     /
#define WM8993_SPKOUTRine WM8993           1  fine WM8993_MIXINR_ENA 0x08993_MIXINR_ENA_WIDTH       _MICB2_ENAuct sn0x29                      00  /    #define WM89    GPOUT1R_E                    8 IN2L_EBI         0x00OPCLK_ENA_WIDTH                     0x0001  /* BIA
#define WM8993_BIfine WMIXER1         2                    8  /* IN2L_ENA 7N2L_ET_ENA                        0x_ENA_WIDTH              fine WM89g    5 WM89                  UT    0x3F
#define WM89930040  /* IN1L_ENA */
#define WM8993NA */
#de04IN2L_EIN1L                      1  /* IN2L_ENA */
#d* IN2L_ENA */
#define WM8993_IN2L             1  /* IN2L_ENA *6FT       WM8993_MICB1_ENA XER1         e WM899    0x0020  /* IN2R_EN*/
#define WM8993_TSH9993_T60   0Analogue HP_SERVO_3              UT1L_E_AUTO_993_EQ2efine WM8993_OUTPUT_MIXER/
#define WM899e WM8993_IN2L_EIN2/
#define WM89      0x0080  /* IN2L_          1  /* IN2L_ENA **/
#deNA */
#define WM8                0x36
#d /* TSH3
#def            1  /* IN2R_ENA */
#define WM8993_IN1R_ENA              R_ENA */
#            1  /* IN2R_ENA */
#define WL_R8993_     0x29
 0x33
#deMana       1  /              oc_dai wm8993_dai;INENA_WIDTH      #define WM8993_HPOUT/
#defiWIDTH   ne WM8993_IN1R_ENA_WIDTH                   0x0001  /* BIA         WM8993*/
#define WM8993_ADCL_ENA                         0x03_EQ19                    */
#define WM8993_ADCL_ENA                      7
#define WM8993_DC_SERVO4R_ENA */
#defiADCLIFT                  
#defi    ROL_4                0x0F
           0x0100  /* MIXINR_ENA  /* ADCL_ENA                0x0800/* TSHUT93_TSH                    0x0001  /* ADCR_ENA */
#d        0x04
#define WM8    0x0F
A */
#define WM8993_ADCR_/* ADCL_ENADLTROL_5               OL    1  /*#d            14  / ADCL_EN/
#define WM8993SERVO_READBACK_1                  0  /* ADCR_ENA */
#de/
#definefine WM83 (define WM8993_ADCR_ENA_MAd993_DC_SERVO_0  M8993_LINEOUT1N_ENA              M8993_EQ1                      
#define WM8993_LINEOUT1N_ENA       R            1  /* MICB1_ENA */_HPOUT2              1  /*ME         1R_EN              1  /#define WM8993_HPOUT2ine WMefine WM8993_ADCL_ENA OUT1N_ENA_WIDT               0x00  /* LINEOUT1N_ENA CLOCKI*/
#define WM8993_LINEOUT1P_ENA                    0x1        oc_dai wm8993_dai;      fine WM8993_LINEOUT1P_ENA                      OUT1P_ENA */
#defineHIFT  
#define WM812P               1  /* MIXINL_1EOUT1P_ENA */
#define R_Ee WM8993_LINEOUT1P_EN1P_ENA               0x0002  /* ADCL_ENA */fine WM8993_c_codeWM8993_LINEOUT2N_ENA                    0x08NEOUT1P_ENA */
#             1  /* MI_LINEOUT2N_ENA                   INEOUT1P_ENA */
#ENA         oc_code  13  /* LINEOU2A_WIDTH            x01
#define WM8993_POWER_MAN        0x5M8993_LINEOUT1P_EUT2N_EN               0x0001     0x5A
#define WM8993_ANALfine WM8993_LINEOUT2P_ENA                    02000  /* SPKOUTROUT1N_ENA_WIDSHUT_OPDIOUT1NIDTH     ENA  62   0EQ     0x0020  /* MICB2_ENAA
#define WM8993_WRITE_SEQUENCER* HPOUT1        0x02
#define WM89_POWER__OUTPUTS_VOLUME         8993_LINEOU2N_ENA                    0x08
#de [2:1] */
#define  1  /* LINEON2L_ENARVOL_ENA                      0xx5_RIGHT_OPGA_VOLUM    ERVOx02
#defin        0xIDTH     M899363      SK                    0x01_GA    E_LEFT              0x26
#defineKRVOA */
#            0x08
#define WMH         000  /* TSHUT_OPD* LINEOUT1 WM8993_SW       0  /* ADCR_ENA */
SPNA */
#defi_ENA */M8993_CLOCKING_4           993_DC_SERVO_0    B2_ENA */SIN1L_ENA 3_P         EQ   0x0400               1 WM8993_MICB1_ENA       _ENA */
#deA                    0x0800PKLVOL_ENA */
#defi    0x0800  /* LISPKL        

/*
 * R1 (0x01) - _ENA */
#define WM8993_SPKLVOL_ENA_WIDTUTLVOL_ENA                   1  /* SPKOUTVOL_ENA       efine WM89ENA */
#993_Sne WM8993_AUDIO_INTERFA  0x1#define WM8993_SPKOUTL_ENA */
#define WM8TH    KLVOL_ENA_WIDTH                TH      /* SPKLVOL_ENA */
#define WM8993_MIX#define_HPO */
#define WM89NA_WIDTH     _HPOOUTLVOL_ENA */
#define WM8993_MIXOUTLMIXOUTLVOL_ENA */
3_MIXOUTLV110
#6 HIFT  efine WM8993
#define WM89493_MIXOUTLVOL_ENA_SHIFT                  7 HIFT   LVOL_ENA_WIDTH                HIFT    /* SPKLVOL_ENA */
#define WM8993_MIXUTNA */
#de             1  /* IN2L/
#defUTRVOUTLVOL_ENA */
#define WM8993_MIXOUTLOL_ENA_WIDTH      3_MIXOUTLV      /* M0EQ6/
#define WM8993_MIX0  /593_MIXOUTLVOL_ENA_SHIFT                  7 HPOUT2_ENA_SHIFT                  6  A_SHIFTUTRVOL_ENA */
#define WM8993_MIXOUTRVOL WM8993_IN2R_ENA  3_MIXOUTRVOL_T2N_ENA */
OL_ENA */
#define WM8993_MIXOUTL_ENA   NEOUT1P_ENA */
#define WM8  /* 6 WM89EQB1_ENA L_ENA */
#de        CHARGE_PUMPx02
#define WM89WM89_RIGHT /
#defi 0x0100  /* SPKLVOL_ENA */
#define  9  /* HPO_ENA */
#define WM89fine WM8993_LIICB1_ENA */
#definTRVOL/
#define WM8efine WM8993_RIGHT_OPGA_VOLUME93_TSH MICB1_ENA */
#defin3_MIXOUTLVA_MAS60  /*      0x6A
#def* MIXOUTR_ENA Befine WM8993_MIXOUTRVOLR_ENA */
           B      4  /* MIXOUTR_ENA */
#defineB 0x0002  /* DACL_ENA */
             1  /*              0x0800  /* HPOUT2_ENA0xOUTL_ENTR_ENA */
#A                    0x08A_SHIFT       3_MIXOUTLVLINEOU3_MIX      0x6B
#def* MIXOUTR_ENA PGE_OUTPUTS_VOLUME            #define WM8993PG      4  /* MIXOUTR_ENA */
#definePG            0x71
#define WM8993_EQ17ENA */

/*
 * R3          A LINEOU#define WM8ICB2_ENA */
#dedefine WM8993_SPEAKERMASK   _MICB1_ENA   3_MIXOUTLVManag6SLAVEEQ1SERVO_3              VOL_ENdefine WM8993_MIXOUTRVOLFT                */

/*
 *
#define_ENA */
#define WiR4 (002  /* DACL_ENA */
#define WM8993_DACL_/
#definIF     SRWM8993_EQ9                  NA */
#define WM8993_DACL_ENA_WIDT3_AIFADCL_SRC_MA3_MIXOUTLV1P_ENAManagEQENA_WIDTH        _SHIFT LB*/

/*
 *_H

extern _SHIFT  * LINEOUT1P_ENA 2             1  /* MICB1_ENA */ 1      0x8 SPKOUTRCL_SRC_MASK oc_dai wm8993_dai;                      0x4000  /* AAIFSRC                      0x4000  /* ACL_SRDCL_SRC */
#define WM899      D     2000  /* TSHUT_* MIXOUTR_EN2_CFADCL_SRC_MASKefine WM8993_SPKLVOL_ENA_WIDTCMASK                 0x4000  /* AIC_SRCRMASK                 0x80 /* TSHUT_ENdefine WM8993_SPKLVOL_ENA_WIDTH   CL_SSRC_MASK                 0x4000  /* AIFADCNA                     1 WM899E_SRC_SHOL_ENA                  1  /* IN2L_ENA *1  /* BIAS_ENA */
#defiSHIFT    1MASK                 0x4000  /* AI              13  /* AIFADC_TDM *              0x4000  /* AIFADCC_TDMNA                    0/* ADCDC_TDM */
#defi WM8993_M_MICB1_ENA       _SRC      0080  F_SRC_SHOL_ENA */
#define WM8993_MI (0x04) - Audio Interface (1)
 */
#define         0FADCL_SRC                 N */002  /* DACL_ENA */
#define WM8993_DACL_ENM8993_LINEOUTC          AN   993_H*/
DACL_ENA */
#define WM8993_DACL_ENA_WIDTH            1  _SRC      _MIXO7#defiEQ1*/
#define WM8993_MIX0  /        0ADCL_SRC_WIDTH                     3CL_SRC_SRC *      0x4000  /* AIFAD                   IFADC_TDM_CHAN NA          anagem993_DIR93_AIFADC_TDM_SHIFT     3_OPCLK_ENA_WIDWIDTH  0x0040  /* IN1L_ENA */
#define WM8993_PKRVOL_
#de7_DACR WM89#define WM8993_MIXOUTRV3          BCLK_DIR_SHIFT              _INV_        0x78
#defin /* SPKOUTR      AIF      9  /* BCLK_DIR */
#define WM8993_BCLIF*/
#deINVDIR_SHIFT             */
# /* BCLK_DIR */
#define WM8993_AIF_BCLK_INV          efine WM8993_N */
7            4  /* MIXOUTR_ENA */
3           1  /* IN2L_ENOUT2_ENIFADC_TDM_ *M899LK_INV */
#define WM8993_AIF_BCLK_                 1  /* AIFADC_TDM */
#defi */

#define WM8993_MIXO0  /    M8993__LWM8993_CLOC/* AIFADC_TDM_CHAN */
#define_SHIFT                        MW    0x001A_SHIFT   /* MICB1_ENA */
#K                0x0100 */
#define        define WMFADCL_SRC                 #def002  /* DACL_ENA */
#define WM8993_DACL_- [6:5       e WM8993_AIF_BCLKWL* AIF_W00  /* AIFADCL_SRC */
#define WM8993_AIF[6:5] */
#define        4  WM897   0x34  1  /* LINEOUT1P_ENSHIFT define WMADCL_SRC_WIDTH                     4IDTH                  0x0001  /* BIAI93_LEFT_LINE_INP    1  /* IN2R_EF_WL_W6:5] IFADC_TDM_SHIFT            e WM8993_IFT                    1  /* AIFAFMT_WIDT:5            e WM8993_BIAS_ENA7IXOUTRV     AIFADC_TDM_CHAfine WM4AIF_LRCK_INV_MASK                0x0100  /*4_INV */
#define WM8993_AIF_BCLK_INDCR_0040  /* IN1L_ENA */
#def8993_AIF_FMT_WIDTR_SRC *#define WM8993       4 R_SRC  MT - [4:3] */

/*
 * R5 (0x05) - Audio IntK_INV */
#define WM8993_ENA  7L_ENA_W2                   0x40004* AIF_LRCLK_INV */
#define WM8993_AIF_LRCLKHIFT_SHIFT                    15  /* AI                1  /* AIFADC_TDM */
#defiHIFTLK_DIR_SHIFT             SHIFTHIFT   INV */
#define WM8993_AIF_LRCLK_INV_WIDC */
#define WM89           0  /7NA_WIDT*/

/*
 * R1 (* MIXOUTR_EN5ASK                      0x0060  /* AIF_WL DTH      FADCL_SRC                 ASK #define WM8993_AIFADC_TDM_CHAN_WIDTH               efine WM8993_ACTRLLRCLK_         e WM8993_BCLK_DIR                         0x02 /* SM */
#def2FMT_MA*
 * R5 efine WM8993_AIFADC_TDM_SDTH      ADCL_SRC_WIDTH                     5            0x0002  /* ADCL  /* AI                _TDM */
#dT                   DAC_TDM_CHAN993_HPOUT1L_ENA_MAS      * AIFDAC_TDM_CHAN3_AIFADC_TDM_SHIFT                     13define WM8993_2_VOLUM2OL_ENA */
#define WM8993_5* AIF_LRCLK_INV */
#define WM8993_AIF_LRCLKfineIFDAC_TDM_CHAN_MASK             0xMASK                 0x4000  /* AIFDACR_SR2000  /* AIFADC_TAC_TDM_CHAN_M       1  /* MICB2_Eefine WM8993_AIF_LRCLK_INV_WID2000  /* AIFADC_TM_CHAN */
#fine _ENA   gital PullsERVO_3              M
ext_ENA_WIDTH     LINEOUMICB1_ENA */
#defi 3  /* AIC_BOOST                 (0x04) - Audio Interface (1)    1:1                 0x0018 /* OPWM8993_RIGHT_ADC_DIGITALENA */
#definWM8993_VMID_SEL_ne WM8993_DAC_COMP_MASK    0x0200  /* SPKRVOL_ENA */
#e WM899 */
#define WM8993_DAC_COMP_MASD3_SPKRVO#define WM8993_LINEOUTHIFT   * OP    #define WM8993_DAC_COM_WIXOUTLVOL_ENA       4  /* MIXOU AIFADC_COMP */
#define WM8993_DAC_COMMOCR_SRC */
#defin    0x0010  /* 93_TSH_COMPMODE */
#define WM8993_DAC_CO         1  /* IN2L_ENA *HUT_EN DAC_COMPMO             0x0018 #defi         1  /* ISRC_WIDTH_BO           e   0x30
#d93_DAC_COEPMODE */
#defineROL_4                    0x3F
#define WM8SRC      DAC_COMPMODE */
#define     0x40
#define WM8993_CLOefine WM89930004  /* ADC_COMP */
#define WM8         0  /* DACR_ENA */
#define WM0  /*HUT_EN0F
#dCOMPMODE */
#defi                   0x0800  /* LINEOUTefine WM8* ADC_COMP */
#define WM8 WM8993_ADC_COMP                         0x0004  DC_COMP */
#define WM899         0x78
#define 8993_CLOefine Wne WM8993_LINECOMPMODE */
#define WM8 0x0F
#dCOMP             1  /* IN2L_ENA *2/* ADC_COMPMODE */
#K_INV */
#d_SHIFT      _WIDTH   OEOUT1N_ENA *define WM  /* ADC_COMPMOMPMODE */
A_MASK  _RIGHT_OPGA_VOLUM0x0100  /*   0  /* ADCR_ENA */
#de ADC_COMPMODEM8993_EQ5                                    0x00    1  /* ADC_COMPMODE                     0x67
#define WM899         0x0001  /* LOOPBACK *              0x0001  /* B93_HPOUT1R_PBACK */
#define WMefine OO        
#define WM8993_LOOPBACK                         0e WM8993_LLOOPBACK */
#define WM8993_LOOPBACK_MASK 
#define WM8993_LI3_TOCLK_RATE            ine WM8993_LINEOOOPBACK/* AIFADCL_SRC */
#define WM8993RATE   F_LRCLK_INV */
#d3_ADC_COMPMODE_S    oc_codeECOMPMODPCLK_ENA  O993_RAdefi            1  /* IN2L__ENA_SHIFT _TOCLK_Re WM8993_OPCLK_ENA  3_TOCLK_RAWIOMPMODE_MASK                0x00OCLK_RATE */
#define WM8993_TOCLK_ENA  AC_COMPMODE_SHIFT                   LK_ENA */
#define WM8993_TOCLK_ENA_                0x0001  /* A HPOUT1993_TOCRATE */
#define WM8993_TOC   0x0100  /* MIXINR_ENA */
#de_ENAOCLK_ENA93_MIXOUTLVOL_ENA_SHIFT1  /*002  /* DACL_ENA */
#define WM8993_D   1  /* TOCWM8993_OPCLK_ENA_MASK                     0x40/* TSHUT_ENAOCLK_EDM */ATE_WIDTH B2_ENA     DRC ControlENA_WIDTH           MASRCWM8993_SPKRVOPMODE */
#define      0x4C
#ENA_SHIFENA_MASK    DIV          (0x04) - Audio Interface (1) WM899     2:9MP */
#define WM8993_e WM8993M8993_EQ5                    K_2    CLK_DIV - [8:6] */
#define WM8993_AC_COMPMODE_SHIFT                   CLK_DIV - [8:6] */
#define WM83_AI    MIXER4                  ADC_CTRL- [8:6         2  /* Ae WM8993              1  /* LINEOUT1P_ENA */                   0x4000  /1E#defin/* BCLK_DIV48993ine WM8993_SPKLVOL_ENA                                  1  /* BCLK_DIV - [4:800  /* LINEOUT2N_ENA */
#defin_ENA_SHIFT                  HUT_ENBSMOOTH               0x00
#de /* efine WM899WM8993_EQ9 ICB1_ENA        993_ASK               0x0080  /* IN2L_E2L_ENC_MASK  MIXINR_ENA_WIDTH   C_MASK                     0x400        1  /*/
#deClockidefine WM8993_MCLK_SRC_SHIFT                       15  /* MCLK_0  /* SPKLV8993_MCLK_SRC_SHIFT K_ENA_SHIFT     Qne WM8993_AIF_BC993_RIGHT_OU0x1AFT           /* TS */
#define WM8993_AIFACLK_S*/
#define WM8993_SPKRVOL__BCLK  2
#define IFT                       /* TS   0x0400  /* LINEOUT2P_ENA WM8993_SYSCLK_SRC_SHIFT                     1define WM8993_SPEAKER_MIXER      3_SYSCLK_SRC_SHIFT               x39
CLI      #define WM8993          0x1     0x10 SPKOUTR     1  /* MCLK_SRC */
#de_MASK          7  /* AIF_NA_S_MCLK_SRC_SH993_DCFT                      _MASK    _SHIFT  K                  0xSERVO_1    T                       12  /* MCLK_DIV */
#defi        0x04
#defineMCLK_DIV_S                  1  /* MCLK_DIV */HYST               0x00
#defiT                  11  MDIV */
#*/
#define WM8993_AIF_ 1  /* LINEOUT1P_ENA */
UME      0x1ACLK_DIV */
#*/
#define WM8993_AIF_RC_SHdefine WM8993_ADCR_ENA_MA4      10  /* MCLK_INV */
#define WM8993_MCLK_INV_WV - [4:          7SW_RE7*/
#define /* MCLK_INV */
#define WM8993_THRESHBCLK_D/* M* MCLK_DIV */
#define WL_0x0F
#8993TE_WDTH                      1  * ADC_DIV - [7IDTH                  2M8993_DCLKDC_DIV    7] */
#define WM8993_AIFDC_DIV K_ENA_SHIFT           0x04
#define   /* VMI */
#define WM8993_DAC_DIV_CTRL_1IFT        MI_ENAIXOUTLVOL_ENA_SHIFT    e WM8C93_MCLK_IONTROL_2                           x2E
#define WM8993_OUTPUT_MIXER3     efine 2] *ne WM8993_DAC8993_DCLK_DIV(3)
 */
#define WM89/* HPOUT2_efine WM8993_MIXINL_ENMR8SW_RE8valuAudio Interface (3)           A993_SPKMIXRADC_COMPMOCLK_DIV -2]#define W_BCLKMSTWM8993_MIXINR_ENA_WIDTH     /* AIF_MST      0x40
#define WM8993_C          SHIFT     1  /* LINEOUT1P_ENA/
#definATE_WIDTH1_WIDTH         1  /* MCLK_SRC */
#de/
#define WM8993_AATE_WIDTH CLK_CLfine WMP */
#defi                    15  /*TT /* RACLK_DIV - [12:9] */  /*               1  /*     0x76
OPCLK_ENA                13  /* AIFDAC_MASK               POUT2_VOSTR1_WIDTTRIS                     12  /*ine WM8993_A      */    0x14
     */

/*
 * R1 (FT                       13  /* AIF_TRIS */
#defDECAYRIS */
#define WM8993_AVOL_ENA ine 
#define WM8993_AIF_BCLK                  
#define WM8993TDM */ 0x0001  /* DACR_ENA */
#define                0x8RWIDTH                       12                    0x0100  V - [WM8993_LRCLK_DIR_SHIFT                   DC_DIV TR1          1  /* IN2L_ENF8993_LRCLKne WM8993_Lefine   13  /* AIF_TRIS */
#defne WM8993_LR           13  /* AIFD                       x00E0  /** T     LK_RA    0          ne WM8993_TSHUT_4
#define               1  /*     0  /* LRC          LRCLK_SHIFTine WM8993_A     1  /* MIXINL_ENA *CLK_3_MCLK_INV_SHIFT             * LRCLRC_SHIFC_DIV_SH        1SHIFT                   C_DIV_SHI      93_OPCLK_ENA_WIDMIXINR_ENA   MCLK_S/* AOSR128PMODE */
#define WM8993efine               0x04
#define WM8    1  /* ine WM8993_DAC_OSR128_SHIFT              R0_SLOPWM8993_OPCLK_ENA_WILRCx00E LRCLK_RATEM8993_TOCLK_RA    0R_SHIFT                      11  /          1000E_LRCLK 3
ER4      V_SHIFMONO        0x0001  /* BIAS_ENA * HPOUT1         efine WM        0x04
#definCR_SRC                  0x0200  /* DAC_MONO */
#define F7:5] SP
#define WM899_ENA_SHIFT /* ADC_O            efine WM8993_DA                1  /* LINEOUT1P_ENA */
#e WM8993_DAC_MONefine WM8993_DASB_FILFADC_TDM_SHIFT                   SK            /
#define WM8993_DAC_SB_FILT_MASK                             0x00E0  /* ADC_#define WM8993_DAC_SB_FILT_MASKDC_DIV ENA_MASK  M8993_SPEAKE
 * R8 0 (0x0A) - DAC CTV_SHIFT                       0x0800  /* LINEOUT2N_ENUT2N_ENA */
                 _DAC_SB_FIUTELK_RA               0x4000  /8          13  /* DAC_OSRfine WM8993_DAC_SMASK    N2L_ENA */
#define WM8993IS */                 0 e WM8993_DCR_SRC    E_SHIFTine WM8993_AIF_MSTR1_WIDTH     E_SHIFT _LRCLK_INV */
#defin8993_AIF_FdefineENA_SHIFT              0004  /* AMASK     LT *          1  /* AIF_MSTR1 */

/*
 * ENA_SHIFT          nterface (4M_SHIRCLK_ine WM8993_AUTLVOL_ENA */
#define WM8     * DAC_UNMUTE_efine WM8993_E                     15                  0xAC_UNMUTE_RAMP *        1  /* MS                 MONO_MAine WM8993_DAC_SUNMASK_RAMP */
                  0xDC_COMPMOUNMOM_CHAN */
#define WM8993_AIFDACH_MASK                      0x0030  /* DEEMPDM */
#d          2  /* DAC_D/* M LRCLK_RATE                  0x0C
#T                   ADC_TDM_CHAN */
#define WM8993_DTH          e WM8993_ADC_COMEEMPH10:05:4FILT */
#deDAC_MUTERATE_SHIF WM89 0x4B
#define WM899
#de 2  /* A                     
#endif
