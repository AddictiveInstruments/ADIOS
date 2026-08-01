// $Id: tia.c 1769 2013-05-02 19:45:09Z tk $
//! \defgroup TIA
//!
//! MAX72xx module driver
//!
//! \{
/* ==========================================================================
 *
 *  Copyright (C) 2013 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>

#include "stella.h"
#include "stella_tables.h"
#include <math.h>

/////////////////////////////////////////////////////////////////////////////
// Local structures
/////////////////////////////////////////////////////////////////////////////

// ENVx State
typedef enum {
  _ATTACK = 0,
  _DECAY = 1,
  _SUSTAIN = 2,
  _RELEASE = 3
} env_state_t;


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

// timers clocked at CPU/2 clock
#define TIM_PERIPHERAL_FRQ (MIOS32_SYS_CPU_FREQUENCY/2)

#define REG_NUM (STELLA_NUM/4+((STELLA_NUM%4)?1:0))
#define REG_ALL_TIA 0xaa
#define REG_ALL_DDS 0x55
#define REG_ALL_IDLE 0xff
// Stella engine timer resolution
#define TICK_RESO 100


//#define FREQ_VERBOSE
//#define ENV_VERBOSE
/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Global variables
/////////////////////////////////////////////////////////////////////////////
stella_tia_t stella[STELLA_NUM];
stella_shadow_tia_t stella_shadow[STELLA_NUM];

/////////////////////////////////////////////////////////////////////////////
// Local Prototypes
/////////////////////////////////////////////////////////////////////////////
static s32 STELLA_StopRestart(s8 voice, u8 state);
static void STELLA_AUDx_SustainCalc(s8 voice, u8 audx);
static u16 STELLA_AUDx_EnvProcess(u8 voice, u8 audx);
static void STELLA_Tick(void);
static float noteToFreq(int note);


/////////////////////////////////////////////////////////////////////////////
// Serial data shift
/////////////////////////////////////////////////////////////////////////////

inline static s32 STELLA_REG_SerDataShift(u8 data) {
  int i;
  for(i=0; i<8; ++i, data <<= 1) {
    STELLA_SPI_SO_SET(data & 0x80); // SO
    STELLA_SPI_SC_SET_0; // SC = 0 (Clk)
    STELLA_SPI_SC_SET_0; // stretch
    STELLA_SPI_SC_SET_0; // stretch
    STELLA_SPI_SC_SET_0; // stretch
    STELLA_SPI_SC_SET_0; // stretch
    STELLA_SPI_SC_SET_1; // SC = 1 (Clk)
    STELLA_SPI_SC_SET_1; // stretch
    STELLA_SPI_SC_SET_1; // stretch
  }
  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
inline static s32 STELLA_DDS_SerDataShift(u16 data) {
  int i;
  for(i=0; i<16; ++i, data <<= 1) {
    STELLA_SPI_SO_SET(data & 0x8000); // SO
    STELLA_SPI_SC_SET_1; // SC = 0 (Clk)
    STELLA_SPI_SC_SET_1; // stretch
    STELLA_SPI_SC_SET_0; // stretch
    STELLA_SPI_SC_SET_0; // stretch
    STELLA_SPI_SC_SET_0; // stretch
    STELLA_SPI_SC_SET_0; // SC = 1 (Clk)
    STELLA_SPI_SC_SET_1; // stretch
    STELLA_SPI_SC_SET_1; // stretch
  }
  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
inline static s32 STELLA_UpdateSelection(void)
{
  STELLA_SPI_SL_SET_0;  // SL = 0
  STELLA_SPI_SL_SET_1;  // SL = 1
  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
inline static s32 STELLA_UpdateTIA(void)
{
  STELLA_SPI_RC_SET_0;  // RC = 0
  STELLA_SPI_RC_SET_1;  // RC = 1
  
  return 0; // no error
}
/////////////////////////////////////////////////////////////////////////////
inline static s32 STELLA_WaitClockTIA(void)
{
  // synchronize with rising edge of TIA.O2 clock
  TIM4->SR &= ~TIM_SR_CC1IF;
  while( !(TIM4->SR & TIM_SR_CC1IF) );
  return 0; // no error
}
/////////////////////////////////////////////////////////////////////////////
inline static s32 STELLA_DDS_FSYNC_Clr(void)
{
  STELLA_SPI_RC_SET_0;  // RC = 0
  STELLA_SPI_RC_SET_0;  // stretch
  return 0; // no error
}
/////////////////////////////////////////////////////////////////////////////
inline static s32 STELLA_DDS_FSYNC_Set(void)
{
  STELLA_SPI_RC_SET_0;  // RC = 0
  STELLA_SPI_RC_SET_1;  // RC = 1
  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! Initializes MAX72xx driver
//! Should be called from Init() during startup
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_Init(u32 mode)
{
  s32 status = 0;
  int i;
  
  // currently only mode 0 supported
  if( mode != 0 )
    return -1; // unsupported mode
  
  // init GPIO
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_StructInit(&GPIO_InitStructure);
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
  // J4A.SC as SO pin
  GPIO_InitStructure.GPIO_Pin = STELLA_SPI_SO_PIN;
  GPIO_Init(STELLA_SPI_SO_PORT, &GPIO_InitStructure);
  // J4B.SD as SC pin
  GPIO_InitStructure.GPIO_Pin = STELLA_SPI_SC_PIN;
  GPIO_Init(STELLA_SPI_SC_PORT, &GPIO_InitStructure);
  // J4A.SD as SL(selection) pin
  GPIO_InitStructure.GPIO_Pin = STELLA_SPI_SL_PIN;
  GPIO_Init(STELLA_SPI_SL_PORT, &GPIO_InitStructure);
  // J4B.SC as TIA.O2 pin
  GPIO_PinAFConfig(STELLA_TIA_O2_PORT, STELLA_TIA_O2_PINSRC, GPIO_AF_TIM4);
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin = STELLA_TIA_O2_PIN;
  GPIO_Init(STELLA_TIA_O2_PORT, &GPIO_InitStructure);
  // J5.A0 as RC pin
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_Pin = STELLA_SPI_RC_PIN;
  GPIO_Init(STELLA_SPI_RC_PORT, &GPIO_InitStructure);
  
  // RC and SL at idle state
  STELLA_SPI_RC_SET_1;
  STELLA_SPI_SL_SET_1;
  
  // TIA.O2 @3MHz using TIM4
  TIM_TimeBaseInitTypeDef TIM_BaseStruct;
  //Enable clock for TIM4
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
  TIM_BaseStruct.TIM_Prescaler =  0;
  TIM_BaseStruct.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_BaseStruct.TIM_Period = 42-1; /* 2MHz PWM */
  TIM_BaseStruct.TIM_ClockDivision = 0;
  TIM_BaseStruct.TIM_RepetitionCounter = 0;
  TIM_TimeBaseInit(TIM4, &TIM_BaseStruct);
  // Start count on TIM4
  TIM_Cmd(TIM4, ENABLE);
  // init PWM
  TIM_OCInitTypeDef TIM_OCStruct;
  TIM_OCStruct.TIM_OCMode = TIM_OCMode_PWM2;
  TIM_OCStruct.TIM_OutputState = TIM_OutputState_Enable;
  TIM_OCStruct.TIM_OCPolarity = TIM_OCPolarity_Low;
  TIM_OCStruct.TIM_Pulse = 21-1; /* (50)% duty cycle */
  TIM_OC1Init(TIM4, &TIM_OCStruct);
  TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
  
  // put dummy value in the stella structures for correct startup
  STELLA_AUDx_PolynomSet(-1, 2, _STELLA_SILENT);
  STELLA_AUDx_LevelSet(-1, 2, 0);
  STELLA_AUDx_DividerSet(-1, 2, 0);
  STELLA_AUDx_EnvModeSet(-1, 2, 1);
  STELLA_AUDx_AttackSet(-1, 2, 0);
  STELLA_AUDx_DecaySet(-1, 2, 127);
  STELLA_AUDx_SustainSet(-1, 2, 127);
  STELLA_AUDx_ReleaseSet(-1, 2, 10);
  
  for (i=0; i<STELLA_NUM; i++) {
    stella[i].curr_note = 69<<9;
    stella[i].dds_note = 69<<9;
    stella[i].target_note = 69<<9;
    stella[i].dds_ctrl = 0;
    stella[i].vel = 0;
    stella[i].pb_mod = 0;
    stella[i].pb_range = 12;
    stella[i].trans_mod = 0;
    stella[i].transpose = 0;
    stella[i].ft_mod = 0;
    stella[i].finetune = 0;
    stella[i].level = 127;
    stella[i].porta_active = 0;
    stella[i].porta_glide = 0;
    stella[i].porta_gliss = 0;
    stella[i].porta_constant = 0;
    stella[i].porta_legato = 0;
    stella[i].porta_repeat = 0;
    stella_shadow[i].dds_note = 69<<9;
    stella_shadow[i].dds_ctrl = stella[i].dds_ctrl;
    stella_shadow[i].aud[0].polynom = stella[i].aud[0].polynom;
    stella_shadow[i].aud[1].polynom = stella[i].aud[1].polynom;
    stella_shadow[i].aud[0].volume = stella[i].aud[0].volume;
    stella_shadow[i].aud[1].volume = stella[i].aud[1].volume;
    stella_shadow[i].aud[0].divider = stella[i].aud[0].divider;
    stella_shadow[i].aud[1].divider = stella[i].aud[1].divider;
  }
  
  /* Init/reset the TIA */
  // All TIA registers Selected
  STELLA_REG_SerDataShift(REG_ALL_TIA);
  STELLA_UpdateSelection();
  // Shift idle state
  STELLA_REG_SerDataShift(_STELLA_SILENT);
  STELLA_REG_SerDataShift(STELLA_TIA_AUDC0);
  STELLA_UpdateTIA();
  STELLA_WaitClockTIA();
  // Shift idle state
  STELLA_REG_SerDataShift(_STELLA_SILENT);
  STELLA_REG_SerDataShift(STELLA_TIA_AUDC1);
  STELLA_UpdateTIA();
  STELLA_WaitClockTIA();
  // Shift idle state
  STELLA_REG_SerDataShift(0);
  STELLA_REG_SerDataShift(STELLA_TIA_AUDV0);
  STELLA_UpdateTIA();
  STELLA_WaitClockTIA();
  // Shift idle state
  STELLA_REG_SerDataShift(0);
  STELLA_REG_SerDataShift(STELLA_TIA_AUDV1);
  STELLA_UpdateTIA();
  STELLA_WaitClockTIA();
  // Shift idle state
  STELLA_REG_SerDataShift(0);
  STELLA_REG_SerDataShift(STELLA_TIA_AUDF0);
  STELLA_UpdateTIA();
  STELLA_WaitClockTIA();
  // Shift idle state
  STELLA_REG_SerDataShift(0);
  STELLA_REG_SerDataShift(STELLA_TIA_AUDF1);
  STELLA_UpdateTIA();
  
  // Reset selection registers
  u8 sl_reg[REG_NUM];
  for (i=0; i<REG_NUM; i++){
    sl_reg[i]=0xff;
    STELLA_REG_SerDataShift(sl_reg[i]);
  }
  STELLA_UpdateSelection();
  
  // All DDS Selected
  for(i=0; i<STELLA_NUM; i++){
    stella[i].dds_ctrl = (1<<STELLA_DDS_B28) | (1<<STELLA_DDS_RESET) | (1<<STELLA_DDS_OPBITEN) | (1<<STELLA_DDS_DIV2) | (1<<STELLA_DDS_SLEEP12);
  }
  float freq = noteToFreq(stella[0].curr_note);
  u32 dds_reg = STELLA_DDS_FREQ_CALC(freq);
  STELLA_REG_SerDataShift(REG_ALL_DDS);
  STELLA_UpdateSelection();
  // start sending data to DDS
  STELLA_DDS_FSYNC_Clr();
  STELLA_DDS_SerDataShift(stella[0].dds_ctrl);  //Control register
  STELLA_DDS_SerDataShift(STELLA_DDS_FREQ0 | (0x3FFF&(uint16_t)(dds_reg)));  // FREQ 0 LSB
  STELLA_DDS_SerDataShift(STELLA_DDS_FREQ0 | (0x3FFF&(uint16_t)(dds_reg>>14)));  // FREQ 0 MSB
  STELLA_DDS_SerDataShift(0xC000);  // PHASE 0
  for(i=0; i<STELLA_NUM; i++){
    stella[i].dds_ctrl &= ~(1<<STELLA_DDS_RESET);
  }
  STELLA_DDS_SerDataShift(stella[0].dds_ctrl);  // exit reset and square wave
  STELLA_DDS_FSYNC_Set();
  // no Selected
  STELLA_REG_SerDataShift(0xff);
  STELLA_UpdateSelection();
  
  // Initialize timer for 2000uS (=2mS) Period
  MIOS32_TIMER_Init(0, 1000, STELLA_Tick, MIOS32_IRQ_PRIO_HIGH);
  
  return status;
}


/////////////////////////////////////////////////////////////////////////////
// Receives Note On
// IN: <voice>: if -1: All TIA
//            else: the TIA number
// IN: <midi_package>: the NoteOn midi_package
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_NoteOn(s8 voice, mios32_midi_package_t midi_package)
{
  s32 status = 0;
  int i;
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  if( midi_package.event != NoteOn )
    return -2; // invalid midi_package
  
  // note to freq
  if( midi_package.event == NoteOn && midi_package.velocity > 0 ){
    
    // store/start attack
    if(voice<0){
      for (i=0; i<STELLA_NUM; i++){
        stella[i].target_note = midi_package.note<<9;
        stella[i].vel = midi_package.velocity;
        STELLA_AUDx_SustainCalc(i, 0);
        STELLA_AUDx_SustainCalc(i, 1);
        stella[i].aud[0].env_accumulator = 0;
        stella[i].aud[1].env_accumulator = 0;
        stella[i].aud[0].env_stat = _ATTACK;
        stella[i].aud[1].env_stat = _ATTACK;
        // portamento legato
        if((stella[i].note_gate && stella[i].porta_legato) || !stella[i].porta_legato)
          stella[i].porta_active = 1;
        
        stella[i].note_gate = 1;
        stella[i].aud[0].env_gate = 1;
        stella[i].aud[1].env_gate = 1;
        
      }
    }else{
      stella[(u8)voice].target_note = midi_package.note<<9;
      stella[(u8)voice].vel = midi_package.velocity;
      STELLA_AUDx_SustainCalc((u8)voice, 0);
      STELLA_AUDx_SustainCalc((u8)voice, 1);
      stella[(u8)voice].aud[0].env_accumulator = 0;
      stella[(u8)voice].aud[1].env_accumulator = 0;
      stella[(u8)voice].aud[0].env_stat = _ATTACK;
      stella[(u8)voice].aud[1].env_stat = _ATTACK;
      // portamento legato
      if((stella[(u8)voice].note_gate && stella[(u8)voice].porta_legato) || !stella[(u8)voice].porta_legato)
        stella[(u8)voice].porta_active = 1;
        
      stella[(u8)voice].note_gate = 1;
      stella[(u8)voice].aud[0].env_gate = 1;
      stella[(u8)voice].aud[1].env_gate = 1;
      
    }
  }
  
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Receives Note Off
// IN: <voice>: if -1: All TIA
//               else: the TIA number
// IN: <midi_package>: the NoteOn/NoteOff midi_package
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_NoteOff(s8 voice, mios32_midi_package_t midi_package)
{
  s32 status = 0;
  int i;
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  if( (midi_package.event != NoteOn) && (midi_package.event != NoteOff) )
    return -2; // invalid midi_package
  
  // store/start release
  if(midi_package.event == NoteOff)midi_package.velocity = 0;
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      stella[i].vel = midi_package.velocity;
      stella[i].aud[0].env_stat = _RELEASE;
      stella[i].aud[1].env_stat = _RELEASE;
      stella[i].note_gate = 0;
    }
  }else{
    stella[(u8)voice].vel = midi_package.velocity;
    stella[(u8)voice].aud[0].env_stat = _RELEASE;
    stella[(u8)voice].aud[1].env_stat = _RELEASE;
    stella[(u8)voice].note_gate = 0;
  }
  return status;
}


/////////////////////////////////////////////////////////////////////////////
// Receives the Pitch-Bend
// IN: <voice>: if -1: All TIA
//               else: the TIA number
// IN: <midi_package>: the PB midi_package
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_PitchBendSet(s8 voice, mios32_midi_package_t midi_package)
{
  s32 status = 0;
  int i;
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  if( midi_package.event != PitchBend )
    return -2; // invalid midi_package
  
  s16 pb = (midi_package.evnt2 + (midi_package.evnt2<<7))-8192;

  // set new freq
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      if(pb==0) stella[i].pb_mod = 0;
      else stella[i].pb_mod = (s32)((stella[i].pb_range<<9)*pb/8191);
    }
  }else{
    if(pb==0) stella[(u8)voice].pb_mod = 0;
    else stella[(u8)voice].pb_mod = (s32)((stella[(u8)voice].pb_range<<9)*pb/8191);
  }

  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Sets the Pitch-Bend Range
// IN: <voice>: if -1: All TIA
//               else: the TIA number
// IN: <value>: the PB range, 1-127
// OUT: returns < 0 if update failed
// Note: this is not a realtime parameter
// updated on Pitch-Bend change only
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_PitchBendRangeSet(s8 voice, u8 value)
{
  s32 status = 0;
  int i;
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  if(value>127) value=127;
  if(value==0)value=1;
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      stella[i].pb_range = value;
    }
  }else {
    stella[(u8)voice].pb_range = value;
  }
  
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets a Pitch-Bend Range
// IN: <voice>: the TIA number
// OUT: returns < 0 if update failed
//      else returns Pitch-Bend Range
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_PitchBendRangeGet(u8 voice)
{
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  return (s32)stella[voice].pb_range;
}

/////////////////////////////////////////////////////////////////////////////
// Sets the Portamento glide
// IN: <voice>: if -1: All TIA
//               else: the TIA number
// IN: <value>: the glide, 1-127
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_PortaGlideSet(s8 voice, u8 value)
{
  s32 status = 0;
  int i;
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  if(value>127) value=127;
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      stella[i].porta_glide = value;
    }
  }else {
    stella[(u8)voice].porta_glide = value;
  }
  
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets a Portamento glide
// IN: <voice>: the TIA number
// OUT: returns < 0 if update failed
//      else returns Portamento glide
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_PortaGlideGet(u8 voice)
{
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  return (s32)stella[voice].porta_glide;
}

/////////////////////////////////////////////////////////////////////////////
// Sets the Portamento Mode
// IN: <voice>: if -1: All TIA
//               else: the TIA number
// IN: <value>: 0, _LCR, Linear Constant Rate
//              >1, _LCT, Linear Constant Time
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_PortaModeSet(s8 voice, u8 value)
{
  s32 status = 0;
  int i;
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      stella[i].porta_constant = value?1:0;
    }
  }else {
    stella[(u8)voice].porta_constant = value?1:0;
  }
  
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets a Portamento Mode
// IN: <voice>: the TIA number
// OUT: returns < 0 if update failed
//      else returns Portamento Mode
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_PortaModeGet(u8 voice)
{
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  return (s32)stella[voice].porta_constant;
}

/////////////////////////////////////////////////////////////////////////////
// Sets the Portamento as Glissando
// IN: <voice>: if -1: All TIA
//               else: the TIA number
// IN: <value>: 0, Glissando off
//              >1, Glissando on
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_PortaGlissandoSet(s8 voice, u8 value)
{
  s32 status = 0;
  int i;
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      stella[i].porta_gliss = value?1:0;
    }
  }else {
    stella[(u8)voice].porta_gliss = value?1:0;
  }
  
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets if Portamento is Glissando
// IN: <voice>: the TIA number
// OUT: returns < 0 if update failed
//      else returns Glissando state
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_PortaGlissandoGet(u8 voice)
{
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  return (s32)stella[voice].porta_gliss;
}

/////////////////////////////////////////////////////////////////////////////
// Sets the Portamento Legato
// IN: <voice>: if -1: All TIA
//               else: the TIA number
// IN: <value>: 0, Legato off
//              >1, Legato on
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_PortaLegatoSet(s8 voice, u8 value)
{
  s32 status = 0;
  int i;
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      stella[i].porta_legato = value?1:0;
    }
  }else {
    stella[(u8)voice].porta_legato = value?1:0;
  }
  
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets if Portamento Legato
// IN: <voice>: the TIA number
// OUT: returns < 0 if update failed
//      else returns Legato state
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_PortaRepeatGet(u8 voice)
{
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  return (s32)stella[voice].porta_legato;
}

/////////////////////////////////////////////////////////////////////////////
// Sets the Portamento Repeat
// IN: <voice>: if -1: All TIA
//               else: the TIA number
// IN: <value>: 0, Repeat off
//              >1, Repeat on
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_PortaRepeatSet(s8 voice, u8 value)
{
  s32 status = 0;
  int i;
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      stella[i].porta_repeat = value?1:0;
    }
  }else {
    stella[(u8)voice].porta_repeat = value?1:0;
  }
  
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets if Portamento Repeat
// IN: <voice>: the TIA number
// OUT: returns < 0 if update failed
//      else returns Repeat state
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_PortaLegatoGet(u8 voice)
{
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  return (s32)stella[voice].porta_repeat;
}

/////////////////////////////////////////////////////////////////////////////
// Receives the Transpose
// IN: <voice>: if -1: All TIA
//               else: the TIA number
// IN: <value>:  +/- 64 semitones
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_TransposeSet(s8 voice, s8 value)
{
  s32 status = 0;
  int i;
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  
  if(value>64)value = 64;
  if(value<-64)value = -64;
  // set new freq
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      stella[i].transpose = value;
      if(value==0) stella[i].trans_mod = 0;
      else stella[i].trans_mod = (s32)((value<<9));
    }
  }else{
    stella[(u8)voice].transpose = value;
    if(value==0) stella[(u8)voice].trans_mod = 0;
    else stella[(u8)voice].trans_mod = (s32)((value<<9));
  }

  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets a Transpose
// IN: <voice>: the TIA number
// OUT: returns < 0 if update failed
//      else returns Transpose
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_TransposeGet(u8 voice)
{
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  return (s32)stella[voice].transpose;
}

/////////////////////////////////////////////////////////////////////////////
// Receives the Detune/Finetune
// IN: <voice>: if -1: All TIA
//               else: the TIA number
// IN: <value>:  +/- 100 cents
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_FinetuneSet(s8 voice, s8 value)
{
  s32 status = 0;
  int i;
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  
  if(value>100)value = 100;
  if(value<-100)value = -100;
  // set new freq
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      stella[i].finetune = value;
      if(value==0) stella[i].ft_mod = 0;
      else stella[i].ft_mod = (s32)((1<<9)*value/100);
    }
  }else{
    stella[(u8)voice].finetune = value;
    if(value==0) stella[(u8)voice].ft_mod = 0;
    else stella[(u8)voice].ft_mod = (s32)((1<<9)*value/100);
  }

  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets a Detune/Finetune
// IN: <voice>: the TIA number
// OUT: returns < 0 if update failed
//      else returns Detune/Finetune
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_FinetuneGet(u8 voice)
{
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  return (s32)stella[voice].finetune;
}

/////////////////////////////////////////////////////////////////////////////
// Sets a TIA level
// IN: <voice>: if -1: All TIA
//            else: the TIA number
// IN: <value>: 0 to 127
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_LevelSet(s8 voice, u8 value)
{
  s32 status = 0;
  int i;
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  
  if(value>127) value=127;
  // store
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      stella[i].level = value;
    }
  }else{
    stella[(u8)voice].level = value;
  }
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets a TIA level
// IN: <voice>: the TIA number
// OUT: returns < 0 if update failed
//      else returns the level
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_LevelGet(u8 voice)
{
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
  return (s32)stella[voice].level;
}

/////////////////////////////////////////////////////////////////////////////
// Sets a TIA AUDx polynom (AUDC)
// IN: <voice>: if -1: All TIA
//            else: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
//             2 for both AUDx
// IN: <polynom>: the polynom, see stella_poly_t enum
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_PolynomSet(s8 voice, u8 audx, u8 value)
{
  s32 status = 0;
  int i;
  if( (voice >= STELLA_NUM) || (audx>STELLA_AUDX_NUM)  )
    return -1; // invalid voice/audx
  switch (value) {
    case 0: value = _STELLA_SAW; break;
    case 1: value = _STELLA_DISTO; break;
    case 2: value = _STELLA_ENGINE; break;
    case 3: value = _STELLA_SQUARE; break;
    case 4: value = _STELLA_BASS; break;
    case 5: value = _STELLA_PITFALL; break;
    case 6: value = _STELLA_NOISE; break;
    case 7: value = _STELLA_LEAD; break;
    case 8: value = _STELLA_BUZZ_L; break;
    case 9: value = _STELLA_BUZZ_H; break;
    default:return -2; // invalid value
}
  // store
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      if(audx<2)stella[i].aud[audx].polynom = value;
      else{
        stella[i].aud[0].polynom = value;
        stella[i].aud[1].polynom = value;
      }
    }
  }else{
    if(audx<2)stella[(u8)voice].aud[(u8)audx].polynom = value;
    else{
      stella[(u8)voice].aud[0].polynom = value;
      stella[(u8)voice].aud[1].polynom = value;
    }
  }
  
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets a TIA AUDx polynom (AUDC)
// IN: <voice>: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
// OUT: returns < 0 if update failed
//      else returns the polynom
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_PolynomGet(u8 voice, u8 audx)
{
  if( (voice >= STELLA_NUM) || (audx>=STELLA_AUDX_NUM) )
    return -1; // invalid voice/audx
  return (s32)stella[voice].aud[audx].polynom;
}


/////////////////////////////////////////////////////////////////////////////
// Sets a TIA AUDx freq divider (AUDF)
// IN: <voice>: if -1: All TIA
//            else: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
//             2 for both AUDx
// IN: <value>: 0 to 31, only 32(5bits)
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_DividerSet(s8 voice, u8 audx, u8 value)
{
  s32 status = 0;
  int i;
  if( (voice >= STELLA_NUM) || (audx>STELLA_AUDX_NUM)  )
    return -1; // invalid voice/audx
  
  if(value>31) value=31;
  // store
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      if(audx<2)stella[i].aud[audx].divider = value;
      else{
        stella[i].aud[0].divider = value;
        stella[i].aud[1].divider = value;
      }
    }
  }else{
    if(audx<2)stella[(u8)voice].aud[audx].divider = value;
    else{
      stella[(u8)voice].aud[0].divider = value;
      stella[(u8)voice].aud[1].divider = value;
    }
  }
  
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets a TIA AUDx volume (AUDV)
// IN: <voice>: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
// OUT: returns < 0 if update failed
//      else returns the divider
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_DividerGet(u8 voice, u8 audx)
{
  if( (voice >= STELLA_NUM) || (audx>=STELLA_AUDX_NUM) )
    return -1; // invalid voice/audx
  return (s32)stella[voice].aud[audx].divider;
}

/////////////////////////////////////////////////////////////////////////////
// Sets a TIA AUDx level
// IN: <voice>: if -1: All TIA
//            else: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
//             2 for both AUDx
// IN: <value>: 0 to 127
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_LevelSet(s8 voice, u8 audx, u8 value)
{
  s32 status = 0;
  int i;
  if( (voice >= STELLA_NUM) || (audx>STELLA_AUDX_NUM)  )
    return -1; // invalid voice/audx
  
  if(value>127) value=127;
  // store
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      if(audx<2)stella[i].aud[audx].level = value;
      else{
        stella[i].aud[0].level = value;
        stella[i].aud[1].level = value;
      }
    }
  }else{
    if(audx<2)stella[(u8)voice].aud[audx].level = value;
    else{
      stella[(u8)voice].aud[0].level = value;
      stella[(u8)voice].aud[1].level = value;
    }
  }
  //DEBUG_MSG("[STELLA_AUDx_LevelSet]voice#%d, volume=%d, %d", voice, volume, stella[(u8)voice].aud[0].volume);
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets a TIA AUDx level
// IN: <voice>: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
// OUT: returns < 0 if update failed
//      else returns the level
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_LevelGet(u8 voice, u8 audx)
{
  if( (voice >= STELLA_NUM) || (audx>=STELLA_AUDX_NUM) )
    return -1; // invalid voice/audx
  return (s32)stella[voice].aud[audx].level;
}

/////////////////////////////////////////////////////////////////////////////
// Sets a TIA AUDx enveloppe mode
// IN: <voice>: if -1: All TIA
//            else: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
//             2 for both AUDx
// IN: <value>: =0 for AD, >0 for ADSR
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_EnvModeSet(s8 voice, u8 audx, u8 value)
{
  s32 status = 0;
  int i;
  if( (voice >= STELLA_NUM) || (audx>STELLA_AUDX_NUM)  )
    return -1; // invalid voice/audx
  
  // store
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      if(audx<2)stella[i].aud[audx].env_adsr = value?1:0;
      else{
        stella[i].aud[0].env_adsr = value?1:0;
        stella[i].aud[1].env_adsr = value?1:0;
      }
    }
  }else{
    if(audx<2)stella[(u8)voice].aud[audx].env_adsr = value?1:0;
    else{
      stella[(u8)voice].aud[0].env_adsr = value?1:0;
      stella[(u8)voice].aud[1].env_adsr = value?1:0;
    }
  }
  
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets a TIA AUDx enveloppe mode
// IN: <voice>: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
// OUT: returns 0 for AD
//              1 for ADSR
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_EnvModeGet(u8 voice, u8 audx)
{
  if( (voice >= STELLA_NUM) || (audx>=STELLA_AUDX_NUM) )
    return -1; // invalid voice/audx
  return (s32)stella[voice].aud[audx].env_adsr;
}

/////////////////////////////////////////////////////////////////////////////
// Sets a TIA AUDx enveloppe attack
// IN: <voice>: if -1: All TIA
//            else: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
//             2 for both AUDx
// IN: <volume>: 0 to 127
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_AttackSet(s8 voice, u8 audx, u8 value)
{
  s32 status = 0;
  int i;
  if( (voice >= STELLA_NUM) || (audx>STELLA_AUDX_NUM)  )
    return -1; // invalid voice/audx
  
  // store
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      if(audx<2){
        stella[i].aud[audx].env_aAccum = stella_EnvTable[value<<1];
        stella[i].aud[audx].env_a = value;
      }else{
        stella[i].aud[0].env_aAccum = stella_EnvTable[value<<1];
        stella[i].aud[0].env_a = value;
        stella[i].aud[1].env_aAccum = stella_EnvTable[value<<1];
        stella[i].aud[1].env_a = value;
      }
    }
  }else{
    if(audx<2){
      stella[(u8)voice].aud[audx].env_aAccum = stella_EnvTable[value<<1];
      stella[(u8)voice].aud[audx].env_a = value;
    }else{
      stella[(u8)voice].aud[0].env_aAccum = stella_EnvTable[value<<1];
      stella[(u8)voice].aud[0].env_a = value;
      stella[(u8)voice].aud[1].env_aAccum = stella_EnvTable[value<<1];
      stella[(u8)voice].aud[1].env_a = value;
    }
  }
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets a TIA AUDx enveloppe attack
// IN: <voice>: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
// OUT: returns < 0 if update failed
//      else returns the attack
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_AttackGet(u8 voice, u8 audx)
{
  if( (voice >= STELLA_NUM) || (audx>=STELLA_AUDX_NUM) )
    return -1; // invalid voice/audx
  return (s32)stella[voice].aud[audx].env_a;
}

/////////////////////////////////////////////////////////////////////////////
// Sets a TIA AUDx enveloppe decay
// IN: <voice>: if -1: All TIA
//            else: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
//             2 for both AUDx
// IN: <volume>: 0 to 127
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_DecaySet(s8 voice, u8 audx, u8 value)
{
  s32 status = 0;
  int i;
  if( (voice >= STELLA_NUM) || (audx>STELLA_AUDX_NUM)  )
    return -1; // invalid voice/audx
  
  // store
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      if(audx<2){
        stella[i].aud[audx].env_dAccum = stella_EnvTable[value<<1];
        stella[i].aud[audx].env_d = value;
      }else{
        stella[i].aud[0].env_dAccum = stella_EnvTable[value<<1];
        stella[i].aud[0].env_d = value;
        stella[i].aud[1].env_dAccum = stella_EnvTable[value<<1];
        stella[i].aud[1].env_d = value;
      }
    }
  }else{
    if(audx<2){
      stella[(u8)voice].aud[audx].env_dAccum = stella_EnvTable[value<<1];
      stella[(u8)voice].aud[audx].env_d = value;
    }else{
      stella[(u8)voice].aud[0].env_dAccum = stella_EnvTable[value<<1];
      stella[(u8)voice].aud[0].env_d = value;
      stella[(u8)voice].aud[1].env_dAccum = stella_EnvTable[value<<1];
      stella[(u8)voice].aud[1].env_d = value;
    }
  }
  
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets a TIA AUDx enveloppe decay
// IN: <voice>: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
// OUT: returns < 0 if update failed
//      else returns the decay
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_DecayGet(u8 voice, u8 audx)
{
  if( (voice >= STELLA_NUM) || (audx>=STELLA_AUDX_NUM) )
    return -1; // invalid voice/audx
  return (s32)stella[voice].aud[audx].env_d;
}

/////////////////////////////////////////////////////////////////////////////
// Calc accumulator enveloppe sustain
// IN: <voice>: if -1: All TIA
//            else: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
//             2 for both AUDx
// IN: <volume>: 0 to 127
/////////////////////////////////////////////////////////////////////////////
void STELLA_AUDx_SustainCalc(s8 voice, u8 audx)
{
  // for commodity
  stella_tia_t *t=&stella[voice];
  stella_audx_t *a = &stella[voice].aud[audx];
  // store
  a->env_sAccum = (u16)((a->env_s*t->vel/0x7f)<<9);
}

/////////////////////////////////////////////////////////////////////////////
// Sets a TIA AUDx enveloppe sustain
// IN: <voice>: if -1: All TIA
//            else: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
//             2 for both AUDx
// IN: <volume>: 0 to 127
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_SustainSet(s8 voice, u8 audx, u8 value)
{
  s32 status = 0;
  int i;
  if( (voice >= STELLA_NUM) || (audx>STELLA_AUDX_NUM)  )
    return -1; // invalid voice/audx
  
  if(value>127) value=127;
  // store
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      if(audx<2){
        stella[i].aud[audx].env_s = value;
        STELLA_AUDx_SustainCalc(i, audx);
      }else{
        stella[i].aud[0].env_s = value;
        STELLA_AUDx_SustainCalc(i, 0);
        stella[i].aud[1].env_s = value;
        STELLA_AUDx_SustainCalc(i, 1);
      }
    }
  }else{
    if(audx<2){
      stella[(u8)voice].aud[audx].env_s = value;
      STELLA_AUDx_SustainCalc((u8)voice, audx);
    }else{
      stella[(u8)voice].aud[0].env_s = value;
      STELLA_AUDx_SustainCalc((u8)voice, 0);
      stella[(u8)voice].aud[1].env_s = value;
      STELLA_AUDx_SustainCalc((u8)voice, 1);
    }
  }
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets a TIA AUDx enveloppe sustain
// IN: <voice>: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
// OUT: returns < 0 if update failed
//      else returns the sustain
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_SustainGet(u8 voice, u8 audx)
{
  if( (voice >= STELLA_NUM) || (audx>=STELLA_AUDX_NUM) )
    return -1; // invalid voice/audx
  return (s32)stella[voice].aud[audx].env_s;
}

/////////////////////////////////////////////////////////////////////////////
// Sets a TIA AUDx enveloppe release
// IN: <voice>: if -1: All TIA
//            else: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
//             2 for both AUDx
// IN: <volume>: 0 to 127
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_ReleaseSet(s8 voice, u8 audx, u8 value)
{
  s32 status = 0;
  int i;
  if( (voice >= STELLA_NUM) || (audx>STELLA_AUDX_NUM)  )
    return -1; // invalid voice/audx
  
  // store
  if(voice<0){
    for (i=0; i<STELLA_NUM; i++){
      if(audx<2){
        stella[i].aud[audx].env_rAccum = stella_EnvTable[value<<1];
        stella[i].aud[audx].env_r = value;
      }else{
        stella[i].aud[0].env_rAccum = stella_EnvTable[value<<1];
        stella[i].aud[0].env_r = value;
        stella[i].aud[1].env_rAccum = stella_EnvTable[value<<1];
        stella[i].aud[1].env_r = value;
      }
    }
  }else{
    if(audx<2){
      stella[(u8)voice].aud[audx].env_rAccum = stella_EnvTable[value<<1];
      stella[(u8)voice].aud[audx].env_r = value;
    }else{
      stella[(u8)voice].aud[0].env_rAccum = stella_EnvTable[value<<1];
      stella[(u8)voice].aud[0].env_r = value;
      stella[(u8)voice].aud[1].env_rAccum = stella_EnvTable[value<<1];
      stella[(u8)voice].aud[1].env_r = value;
    }
  }
  return status;
}

/////////////////////////////////////////////////////////////////////////////
// Gets a TIA AUDx enveloppe release
// IN: <voice>: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
// OUT: returns < 0 if update failed
//      else returns the release
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_AUDx_ReleaseGet(u8 voice, u8 audx)
{
  if( (voice >= STELLA_NUM) || (audx>=STELLA_AUDX_NUM) )
    return -1; // invalid voice/audx
  return (s32)stella[voice].aud[audx].env_r;
}


/////////////////////////////////////////////////////////////////////////////
// Process an enveloppe
// Must be called every 100us
// IN: <voice>: the TIA number
// IN: <audx>: 0 for AUD0
//             1 for AUD1
// OUT: returns the enveloppe amplitude
/////////////////////////////////////////////////////////////////////////////
u16 STELLA_AUDx_EnvProcess(u8 voice, u8 audx)
{
  u32 out = 0;
  // for commodity
  stella_tia_t *t=&stella[voice];
  stella_audx_t *a = &stella[voice].aud[audx];
  // store accum
  u16 oacc = a->env_accumulator;
  
  // gate off => return 0
  if (!a->env_gate) {
    out = 0;
    
  } else if (a->env_stat == _ATTACK) {
    out = a->env_aAccum;
    a->env_accumulator += out;
    if ((a->env_accumulator  < oacc) ||
        (a->env_accumulator  >= (t->vel<<9))) {
      out = (t->vel<<9);
      a->env_accumulator = (t->vel<<9);
      a->env_stat = _DECAY;
#ifdef ENV_VERBOSE
      MIOS32_MIDI_SendDebugMessage("T%d/A%d env: attack end \n", voice, audx);
#endif
    } else {
      // all good we're still in attack
      out = a->env_accumulator;
    }
  } else if (a->env_stat == _DECAY) {
    out = a->env_dAccum;
    a->env_accumulator -= out;
    if(a->env_adsr){
      if ((a->env_accumulator < a->env_sAccum+1) ||
          (a->env_accumulator > oacc)) {
        out = a->env_sAccum;
        a->env_accumulator = a->env_sAccum;
        a->env_stat = _SUSTAIN;
#ifdef ENV_VERBOSE
        MIOS32_MIDI_SendDebugMessage("T%d/A%d env: decay end \n", voice, audx);
#endif
      } else {
        out = a->env_accumulator;
      }
    }else{
      if (a->env_accumulator > oacc) {
        out = 0;
        a->env_gate = 0;
#ifdef ENV_VERBOSE
        MIOS32_MIDI_SendDebugMessage("T%d/A%d env: decay end \n", voice, audx);
#endif
      } else {
        out = a->env_accumulator;
      }
    }
  } else if (a->env_stat == _SUSTAIN) {
    a->env_accumulator = a->env_sAccum;
    out = a->env_sAccum;
  } else if (a->env_stat == _RELEASE) {
    a->env_accumulator -= a->env_rAccum;
    
    if (a->env_accumulator > oacc) {
      out = 0;
      a->env_gate = 0;
#ifdef ENV_VERBOSE
      MIOS32_MIDI_SendDebugMessage("T%d/A%d env: release end \n", voice, audx);
#endif
    } else {
      out = a->env_accumulator;
    }
  }
  // apply TIA level
  out =(u16)(out*((t->level)<<9)/(0x7f<<9));
  // apply AUDx level
  out = (u16)(out*((a->level)<<9)/(0x7f<<9));
  return (u16)out;
}


////////////////////////////////////////////////////////////////////////////
// Stop Restart a TIA+DDS
// IN: <voice>: if -1: All TIA
//            else: the TIA number
// IN: <state>: if 0: stop,
//              else: Restart
// OUT: returns < 0 if update failed
/////////////////////////////////////////////////////////////////////////////
s32 STELLA_StopRestart(s8 voice, u8 state)
{
  s32 status = 0;
  
  if( voice >= STELLA_NUM )
    return -1; // invalid voice
    
  // TODO: restart/reinit
  //stella[(u8)voice].porta_freq = stella[(u8)voice].freq;
  return status;
}


/////////////////////////////////////////////////////////////////////////////
// Must be called every 100us
/////////////////////////////////////////////////////////////////////////////
void STELLA_Tick(void)
{
  int i, j;
  u8 r;
  u8 sl_reg[REG_NUM];
  
  // process frequencies
  for (i=0; i<(STELLA_NUM); i++) {
    stella_tia_t *t = &stella[i];
    if(t->porta_end_note != t->target_note){
      t->porta_end_note = t->target_note;
      t->porta_begin_note = t->curr_note;
      t->porta_ctr = 0;
      if(t->target_note == t->curr_note){
       t->curr_note = t->porta_begin_note;
        t->porta_active = 0;
      }
        
    }else if((t->target_note == t->curr_note)
            && t->porta_repeat && !t->porta_legato){
      if( t->porta_constant )t->porta_ctr = 0;
      //else t->curr_note = t->porta_begin_note;
    }
    
    s32 note = t->target_note;
    if( t->porta_active && t->porta_glide ) {
      note = t->curr_note;
      
      
      if( t->porta_constant ) {     // Glide is constant time
        // increment counter
        int porta_ctr = t->porta_ctr + stella_EnvTable[t->porta_glide<<1];
        // target reached on overrun
        if(porta_ctr>=0xffff){
          note = t->target_note;
          t->porta_active = 0;
        }else{
          t->porta_ctr = porta_ctr;
          // scale between new and old frequency
          int delta = t->porta_end_note - t->porta_begin_note;
          note = t->porta_begin_note + ((delta*porta_ctr)>>16);
          if( delta > 0 ) {
            if( note >= t->target_note ) {
              note = t->target_note;
              t->porta_active = 0;
            }
          } else {
            if( note <= t->target_note ) {
              note = t->target_note;
              t->porta_active = 0;
            }
          }
        }
        
      }else{    // Glide is constant rate
        // increment/decrement frequency
        int inc = stella_EnvTable[t->porta_glide<<1];
        if( !inc )inc = 1;
        if( t->target_note > note ) {
          note += inc;
          if( note >= t->target_note ) {
            note = t->target_note;
            t->porta_active = 0;
          }
        } else {
          note -= inc;
          if( note <= t->target_note ) {
            note = t->target_note;
            t->porta_active = 0;
          }
        }
      }
    }
    t->curr_note = note;
    // Glissando mode
    if(t->porta_gliss){
      note >>=9;
      note <<=9;
    }
    // apply pitch-bend
    note += t->pb_mod;
    // apply transpose
    note += t->trans_mod;
    // apply finetune
    note += t->ft_mod;
    // limiting
    if(note<0)note = 0;
    if(note>0x12fff)note = 0x12fff; // limit is 127 notes + 2 octaves
    t->dds_note = note;
  }
  
  // process the enveloppes
  for (i=0; i<(STELLA_NUM); i++) {
    for (j=0; j<(STELLA_AUDX_NUM); j++) {
      stella[i].aud[j].volume = STELLA_AUDx_EnvProcess(i, j)>>12;
    }
  }
  
  // Clean selectioon registers
  for (i=0; i<REG_NUM; i++){
    sl_reg[i]=REG_ALL_IDLE;
    STELLA_REG_SerDataShift(sl_reg[i]);
  }
  STELLA_UpdateSelection();
  
  // start transfer
  for (i=0; i<(STELLA_NUM); i++) {
    
    // frequency update
    if(stella[i].dds_note != stella_shadow[i].dds_note){
      float freq = noteToFreq(stella[i].dds_note);
      u32 dds_reg = STELLA_DDS_FREQ_CALC(freq);
      r = i/4;
      sl_reg[r] = ~(2<<((i%4)*2));
      for (j=(REG_NUM-1); j==0; j--)STELLA_REG_SerDataShift(sl_reg[j]);
      STELLA_UpdateSelection();
      // start sending data to DDS
      STELLA_DDS_FSYNC_Clr();
      STELLA_DDS_SerDataShift(stella[i].dds_ctrl);  // exit reset and square wave
      STELLA_DDS_SerDataShift(STELLA_DDS_FREQ0 | (0x3FFF&(uint16_t)(dds_reg)));  // FREQ 0 LSB
      STELLA_DDS_SerDataShift(STELLA_DDS_FREQ0 | (0x3FFF&(uint16_t)(dds_reg>>14)));  // FREQ 0 MSB
      STELLA_DDS_FSYNC_Set();
      sl_reg[r]=REG_ALL_IDLE;
      for (j=0; j<REG_NUM; j++)STELLA_REG_SerDataShift(sl_reg[j]);
      STELLA_UpdateSelection();
      stella_shadow[i].dds_note = stella[i].dds_note;
      //if(i==0)DEBUG_MSG("[STELLA_Tick]voice#%d, freq=%d", i, stella[i].dds_note);
    }
    if( (stella[i].aud[0].polynom != stella_shadow[i].aud[0].polynom) || (stella[i].aud[1].polynom != stella_shadow[i].aud[1].polynom)
       || (stella[i].aud[0].volume != stella_shadow[i].aud[0].volume) || (stella[i].aud[1].volume != stella_shadow[i].aud[1].volume)
       || (stella[i].aud[0].divider != stella_shadow[i].aud[0].divider) || (stella[i].aud[1].divider != stella_shadow[i].aud[1].divider)){
      //DEBUG_MSG("[STELLA_Tick]voice#%d, vel=%d", t, stella[i].vel);
      r = i/4;
      sl_reg[r] = ~(1<<((i%4)*2));
      for (j=(REG_NUM-1); j==0; j--)STELLA_REG_SerDataShift(sl_reg[j]);
      STELLA_UpdateSelection();
      // AUDC0 update
      if((stella[i].aud[0].polynom != stella_shadow[i].aud[0].polynom) &&
        (stella[i].aud[0].volume && stella_shadow[i].aud[0].volume)){
        STELLA_REG_SerDataShift((u8)stella[i].aud[0].polynom);
        STELLA_REG_SerDataShift(STELLA_TIA_AUDC0);
        STELLA_UpdateTIA();
        STELLA_WaitClockTIA();
        stella_shadow[i].aud[0].polynom = stella[i].aud[0].polynom;
      }
      // AUDC1 update
      if((stella[i].aud[1].polynom != stella_shadow[i].aud[1].polynom) &&
        (stella[i].aud[1].volume && stella_shadow[i].aud[1].volume)){
        STELLA_REG_SerDataShift((u8)stella[i].aud[1].polynom);
        STELLA_REG_SerDataShift(STELLA_TIA_AUDC1);
        STELLA_UpdateTIA();
        STELLA_WaitClockTIA();
        stella_shadow[i].aud[1].polynom = stella[i].aud[1].polynom;
      }
      // AUDV0 update
      if(stella[i].aud[0].volume != stella_shadow[i].aud[0].volume){
        if(!stella[i].aud[0].volume){
          STELLA_REG_SerDataShift(_STELLA_SILENT);
          STELLA_REG_SerDataShift(STELLA_TIA_AUDC0);
          STELLA_UpdateTIA();
          STELLA_WaitClockTIA();
        }else if(!stella_shadow[i].aud[0].volume){
          STELLA_REG_SerDataShift(stella[i].aud[0].polynom);
          STELLA_REG_SerDataShift(STELLA_TIA_AUDC0);
          STELLA_UpdateTIA();
          STELLA_WaitClockTIA();
        }
        STELLA_REG_SerDataShift(stella[i].aud[0].volume);
        STELLA_REG_SerDataShift(STELLA_TIA_AUDV0);
        STELLA_UpdateTIA();
        STELLA_WaitClockTIA();
        stella_shadow[i].aud[0].volume = stella[i].aud[0].volume;

        //DEBUG_MSG("[STELLA_Tick]voice#%d, volume=%d", i, stella[i].aud[0].volume);
      }
      // AUDV1 update
      if(stella[i].aud[1].volume != stella_shadow[i].aud[1].volume){
        if(!stella[i].aud[1].volume){
          STELLA_REG_SerDataShift(_STELLA_SILENT);
          STELLA_REG_SerDataShift(STELLA_TIA_AUDC1);
          STELLA_UpdateTIA();
          STELLA_WaitClockTIA();
        }else if(!stella_shadow[i].aud[1].volume){
          STELLA_REG_SerDataShift(stella[i].aud[1].polynom);
          STELLA_REG_SerDataShift(STELLA_TIA_AUDC1);
          STELLA_UpdateTIA();
          STELLA_WaitClockTIA();
        }
        STELLA_REG_SerDataShift(stella[i].aud[1].volume);
        STELLA_REG_SerDataShift(STELLA_TIA_AUDV1);
        STELLA_UpdateTIA();
        STELLA_WaitClockTIA();
        stella_shadow[i].aud[1].volume = stella[i].aud[1].volume;
        //DEBUG_MSG("[STELLA_Tick]voice#%d, volume=%d", t, volume);
      }
      // AUDF0 update
      if(stella[i].aud[0].divider != stella_shadow[i].aud[0].divider){
        STELLA_REG_SerDataShift(stella[i].aud[0].divider);
        STELLA_REG_SerDataShift(STELLA_TIA_AUDF0);
        STELLA_UpdateTIA();
        STELLA_WaitClockTIA();
        stella_shadow[i].aud[0].divider = stella[i].aud[0].divider;
      }
      // AUDF1 update
      if(stella[i].aud[1].divider != stella_shadow[i].aud[1].divider){
        STELLA_REG_SerDataShift(stella[i].aud[1].divider);
        STELLA_REG_SerDataShift(STELLA_TIA_AUDF1);
        STELLA_UpdateTIA();
        STELLA_WaitClockTIA();
        stella_shadow[i].aud[1].divider = stella[i].aud[1].divider;
        
      }
      sl_reg[r]=REG_ALL_IDLE;
      for (j=0; j<REG_NUM; j++)STELLA_REG_SerDataShift(sl_reg[j]);
      STELLA_UpdateSelection();
    }
  }
}


/////////////////////////////////////////////////////////////////////////////
// Help functions
/////////////////////////////////////////////////////////////////////////////
float noteToFreq(int note) {
  int a = 440; // frequency of A (coomon value is 440Hz)
  return a * pow(2, ((note - 35328) / 6144.0));
}
