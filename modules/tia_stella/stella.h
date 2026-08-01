// $Id: tia.h 1686 2013-02-07 22:11:42Z tk $
/*
 * Header file for TIA functions
 *
 * ==========================================================================
 *
 *  Copyright (C) 2013 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _STELLA_H
#define _STELLA_H

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////////////////////////////////
// Pin definitions
/////////////////////////////////////////////////////////////////////////////
#define STELLA_SPI_SO_PORT    GPIOB
#define STELLA_SPI_SO_PIN     LL_GPIO_PIN_10
#define STELLA_SPI_SL_PORT    GPIOB
#define STELLA_SPI_SL_PIN     LL_GPIO_PIN_11
#define STELLA_SPI_RC_PORT    GPIOC
#define STELLA_SPI_RC_PIN     LL_GPIO_PIN_5
#define STELLA_TIA_O2_PORT    GPIOB
#define STELLA_TIA_O2_PIN     LL_GPIO_PIN_6
#define STELLA_TIA_O2_PINSRC  GPIO_PinSource6
# if defined(MIOS32_BOARD_STM32F4DISCOVERY) || defined(MIOS32_BOARD_MBHP_CORE_STM32F4)
#define STELLA_SPI_SC_PORT   GPIOB
#define STELLA_SPI_SC_PIN    LL_GPIO_PIN_9
# elif defined(MIOS32_BOARD_MBHP_DIPCOREF4)
#define STELLA_SPI_SC_PORT   GPIOB
#define STELLA_SPI_SC_PIN    LL_GPIO_PIN_7
#endif

#define STELLA_SPI_SO_SET(v)      MIOS32_SYS_STM_PINSET(STELLA_SPI_SO_PORT, STELLA_SPI_SO_PIN, v?1:0)
#define STELLA_SPI_SO_SET_0       MIOS32_SYS_STM_PINSET(STELLA_SPI_SO_PORT, STELLA_SPI_SO_PIN, 0)
#define STELLA_SPI_SO_SET_1       MIOS32_SYS_STM_PINSET(STELLA_SPI_SO_PORT, STELLA_SPI_SO_PIN, 1)
#define STELLA_SPI_SC_SET_0       MIOS32_SYS_STM_PINSET(STELLA_SPI_SC_PORT, STELLA_SPI_SC_PIN, 0)
#define STELLA_SPI_SC_SET_1       MIOS32_SYS_STM_PINSET(STELLA_SPI_SC_PORT, STELLA_SPI_SC_PIN, 1)
#define STELLA_SPI_RC_SET_0       MIOS32_SYS_STM_PINSET(STELLA_SPI_RC_PORT, STELLA_SPI_RC_PIN, 0)
#define STELLA_SPI_RC_SET_1       MIOS32_SYS_STM_PINSET(STELLA_SPI_RC_PORT, STELLA_SPI_RC_PIN, 1)
#define STELLA_SPI_SL_SET_0       MIOS32_SYS_STM_PINSET(STELLA_SPI_SL_PORT, STELLA_SPI_SL_PIN, 0)
#define STELLA_SPI_SL_SET_1       MIOS32_SYS_STM_PINSET(STELLA_SPI_SL_PORT, STELLA_SPI_SL_PIN, 1)

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// Number of TIA chip
#ifndef STELLA_NUM
#define STELLA_NUM 4
#endif

// TIA registers
#define STELLA_AUDX_NUM 2
#define STELLA_TIA_AUDC0          0x05
#define STELLA_TIA_AUDC1          0x06
#define STELLA_TIA_AUDF0          0x07
#define STELLA_TIA_AUDF1          0x08
#define STELLA_TIA_AUDV0          0x09
#define STELLA_TIA_AUDV1          0x0A

// DDS waveform output modes
#define STELLA_DDS_OFF            0
//#define STELLA_DDS_TRIANGLE       1
#define STELLA_DDS_SQUARE         2
//#define STELLA_DDS_SINE           3

// DDS Control register bits
#define STELLA_DDS_B28            13
#define STELLA_DDS_HLB            12
#define STELLA_DDS_FSELECT        11
#define STELLA_DDS_PSELECT        10
#define STELLA_DDS_RESET          8
#define STELLA_DDS_SLEEP1         7
#define STELLA_DDS_SLEEP12        6
#define STELLA_DDS_OPBITEN        5
#define STELLA_DDS_DIV2           3
#define STELLA_DDS_MODE           1

// DDS register addresses
#define STELLA_DDS_FREQ0          (1<<14)
//#define STELLA_DDS_FREQ1          (1<<15)
#define STELLA_DDS_PHASE0         (3<<14)
//#define STELLA_DDS_PHASE1         ((3<<14)|(1<<13))

#define STELLA_DDS_MCLK           25000000
#define STELLA_DDS_2POW28         268435456
// Macro that calculates the value for a DDS frequency register from a frequency
//#define STELLA_DDS_FREQ_CALC(freq) (uint32_t)(((double)AD_2POW28/(double)STELLA_DDS_MCLK*freq)*227)
#define STELLA_DDS_FREQ_CALC(freq) (uint32_t)((float)2437.393949048*(float)freq)
// Macro that calculates the value for a DDS phase register from a phase in degrees
//#define STELLA_DDS_PHASE_CALC(phase_deg) (uint16_t)((512*phase_deg)/45)

/////////////////////////////////////////////////////////////////////////////
// Global structures
/////////////////////////////////////////////////////////////////////////////

// AUDx POlynom Enum
typedef enum {
  _STELLA_SILENT = 0,
  _STELLA_SAW = 1,
  _STELLA_DISTO = 2,
  _STELLA_ENGINE = 3,
  _STELLA_SQUARE = 4,
  _STELLA_BASS = 6,
  _STELLA_PITFALL = 9,
  _STELLA_NOISE= 8,
  _STELLA_SILENT2 = 11,
  _STELLA_LEAD = 12,
  _STELLA_BUZZ_L = 14,
  _STELLA_BUZZ_H = 15,
} stella_poly_t;

// Portamento modes
typedef enum {
  _LCR = 0,       // linear constant rate
  _LCT = 1,       // linear constant time
  _GLISSANDO_ON = 2,
  _GLISSANDO_OFF = 3,
  _LEGATO_ON = 4,
  _LEGATO_OFF = 5
} stella_porta_mode_t;

// Portamento modes
typedef enum {
  _POLY = 0,
  _UNISON = 1,
  _MULTI = 2,
  _DRUM =3
} stella_engine_mode_t;

// envelope ******************************************************************
typedef struct {
  u8 level:7;
  u8 _free_bit1:1;
  u8 env_a:7;
  u8 _free_bit2:1;
  u8 env_d:7;
  u8 _free_bit3:1;
  u8 env_s:7;
  u8 _free_bit4:1;
  u8 env_r:7;
  u8 _free_bit5:1;
  u16 env_aAccum;
  u16 env_dAccum;
  u16 env_sAccum;
  u16 env_rAccum;
  u16 env_accumulator;
  u8  env_gate:1;
  u8  env_stat:2;
  u8  env_adsr:1;            // AD / ADSR
  u8 _free_bit6:4;
  u8  divider:5;
  u8 _free_bit7:3;
  u8  volume:4;
  u8  polynom:4;
  
} stella_audx_t;

// Struct that holds all the Stella's configuration
typedef struct {
  s32   curr_note;
  s32   target_note;
  s32   pb_mod;
  s32   porta_begin_note;
  s32   porta_end_note;
  s32   trans_mod;
  s32   ft_mod;
  s32   dds_note;
  u16   dds_ctrl;
  u16   porta_ctr;
  s8    transpose;
  s8    finetune;
  u8    pb_range:7;
  u8    note_gate:1;
  u8    vel:7;
  u8 _free_bit2:1;
  u8    level:7;
  u8 _free_bit3:1;
  u16   porta_glide:7;
  u16   porta_active:1;
  u16   porta_constant:1;
  u16   porta_gliss:1;
  u16   porta_legato:1;
  u16   porta_repeat:1;
  u16 _free_bit4:4;
  stella_audx_t aud[STELLA_AUDX_NUM];
} stella_tia_t;

// envelope ******************************************************************
typedef struct {
  u8  divider:5;
  u8 _free_bit6:3;
  u8  volume:4;
  u8  polynom:4;
  
} stella_shadow_audx_t;

// Struct that holds all the Stella's configuration
typedef struct {
  s32 dds_note;
  u16 dds_ctrl;
  stella_shadow_audx_t aud[STELLA_AUDX_NUM];
} stella_shadow_tia_t;

/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////
 
extern s32 STELLA_Init(u32 mode);

// Voice(TIA+DDS) functions
extern s32 STELLA_NoteOn(s8 voice, mios32_midi_package_t midi_package);
extern s32 STELLA_NoteOff(s8 voice, mios32_midi_package_t midi_package);
extern s32 STELLA_PitchBendSet(s8 voice, mios32_midi_package_t midi_package);
extern s32 STELLA_PitchBendRangeSet(s8 voice, u8 value);
extern s32 STELLA_PitchBendRangeGet(u8 voice);
extern s32 STELLA_PortaGlideSet(s8 voice, u8 value);
extern s32 STELLA_PortaGlideGet(u8 voice);
extern s32 STELLA_PortaModeSet(s8 voice, u8 value);
extern s32 STELLA_PortaModeGet(u8 voice);
extern s32 STELLA_PortaGlissandoSet(s8 voice, u8 value);
extern s32 STELLA_PortaGlissandoGet(u8 voice);
extern s32 STELLA_PortaLegatoSet(s8 voice, u8 value);
extern s32 STELLA_PortaLegatoGet(u8 voice);
extern s32 STELLA_PortaRepeatSet(s8 voice, u8 value);
extern s32 STELLA_PortaRepeatGet(u8 voice);
extern s32 STELLA_TransposeSet(s8 voice, s8 value);
extern s32 STELLA_TransposeGet(u8 voice);
extern s32 STELLA_FinetuneSet(s8 voice, s8 value);
extern s32 STELLA_FinetuneGet(u8 voice);
extern s32 STELLA_LevelSet(s8 voice, u8 value);
extern s32 STELLA_LevelGet(u8 voice);

// TIA AUDx functions
extern s32 STELLA_AUDx_PolynomSet(s8 voice, u8 audx, u8 value);
extern s32 STELLA_AUDx_PolynomGet(u8 voice, u8 audx);
extern s32 STELLA_AUDx_DividerSet(s8 voice, u8 audx, u8 value);
extern s32 STELLA_AUDx_DividerGet(u8 voice, u8 audx);
extern s32 STELLA_AUDx_LevelSet(s8 voice, u8 audx, u8 value);
extern s32 STELLA_AUDx_LevelGet(u8 voice, u8 audx);
extern s32 STELLA_AUDx_EnvModeSet(s8 voice, u8 audx, u8 value);
extern s32 STELLA_AUDx_EnvModeGet(u8 voice, u8 audx);
extern s32 STELLA_AUDx_AttackSet(s8 voice, u8 audx, u8 value);
extern s32 STELLA_AUDx_AttackGet(u8 voice, u8 audx);
extern s32 STELLA_AUDx_DecaySet(s8 voice, u8 audx, u8 value);
extern s32 STELLA_AUDx_DecayGet(u8 voice, u8 audx);
extern s32 STELLA_AUDx_SustainSet(s8 voice, u8 audx, u8 value);
extern s32 STELLA_AUDx_SustainGet(u8 voice, u8 audx);
extern s32 STELLA_AUDx_ReleaseSet(s8 voice, u8 audx, u8 value);
extern s32 STELLA_AUDx_ReleaseGet(u8 voice, u8 audx);

/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////
extern stella_tia_t stella[STELLA_NUM];
extern stella_shadow_tia_t stella_shadow[STELLA_NUM];

#ifdef __cplusplus
}
#endif

#endif /* _STELLA_TIA_H */
