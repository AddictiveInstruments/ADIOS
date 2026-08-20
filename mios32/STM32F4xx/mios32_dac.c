//! \defgroup MIOS32_DAC
//!
//! DAC driver for MIOS32 - STM32F4xx
//!
//!
//! THREE CASES ON THIS FAMILY, NOT TWO
//! ===================================
//!
//! Verified in the CMSIS headers of all 23 F4 devices in the tree:
//!
//!     DAC1, 2 channels ... F405, F407, F413, F415, F417, F423, F427,
//!                          F429, F437, F439, F446, F469, F479
//!     DAC1, 1 CHANNEL .... F410
//!     NO DAC ............. F401, F411, F412
//!
//! and the F410 is odd twice over, which is the trap of this file:
//!
//!     every other F4 ...... channel 1 -> PA4    channel 2 -> PA5
//!     F410 ................ channel 1 -> PA5    (no channel 2)
//!
//! So a schematic carried over from an F407 would put the F410's only
//! output on a pin that converts nothing. Checked pin by pin across all
//! five F410 packages in ST's MCU database - it is systematic, not a
//! bonding accident on one package.
//!
//! Both facts are keyed off DAC_CHANNEL2_SUPPORT rather than off a part
//! number: on this family the F410 IS the device without channel 2, so the
//! feature macro that CMSIS already defines tells us everything, and no
//! hand-written list can go stale.
//!
//!
//! WHY THIS IS NOT SHARED WITH THE G0 DRIVER
//! =========================================
//!
//! The two LL APIs are nearly identical and a shared body was considered.
//! The G0's block is a later generation with two controls this one does not
//! have - output connection (pin or internal) and sample-and-hold - so
//! sharing would have meant guarding those inside common code. See the same
//! note at the head of mios32/STM32G0xx/mios32_dac.c.
//!
//! Wave generation (noise and triangle) and DMA-fed output are NOT
//! implemented. The peripheral supports both; nothing here needs them yet.
//!
//! \{
/* ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>

// this module can be optionally disabled in a local mios32_config.h file (included from mios32.h)
#if defined(MIOS32_USE_DAC)


/////////////////////////////////////////////////////////////////////////////
// Tier check
/////////////////////////////////////////////////////////////////////////////

// Note the #else that follows, and that closes only at the end of the file:
// #error does not stop GCC, it just records a message and carries on. Without
// the #else, the real diagnostic would be buried under a page of "undeclared
// LL_DAC_CHANNEL_1" noise from a body that cannot compile anyway.
#if !defined(DAC1)
# error "MIOS32_USE_DAC requested, but this STM32F4 has no DAC. F401, F411 and F412 have none; every other F4 line does."
#else

#if !MIOS32_DAC_CHANNEL_MASK
# warning "MIOS32_USE_DAC is set but MIOS32_DAC_CHANNEL_MASK is 0: the DAC will be initialised and drive nothing."
#endif

#if MIOS32_DAC_CHANNEL_MASK & ~0x3
# error "MIOS32_DAC_CHANNEL_MASK has a bit above 1. This DAC has at most two channels: bit 0 is MIOS32_DAC_CHANNEL0, bit 1 is MIOS32_DAC_CHANNEL1."
#endif

#if !defined(DAC_CHANNEL2_SUPPORT) && (MIOS32_DAC_CHANNEL_MASK & (1 << 1))
# error "MIOS32_DAC_CHANNEL_MASK selects channel 2, but this chip's DAC has a single channel - it is an STM32F410. Note also that its one output is on PA5, not on PA4 as everywhere else."
#endif


/////////////////////////////////////////////////////////////////////////////
// Overridable settings
/////////////////////////////////////////////////////////////////////////////

// Output pins. Channel 1 is on PA4 across the family - EXCEPT on the F410,
// where it is on PA5. Keyed off DAC_CHANNEL2_SUPPORT because on the F4 the
// single-channel part and the PA5 part are the same part.
#ifndef MIOS32_DAC_CH0_PORT
#define MIOS32_DAC_CH0_PORT GPIOA
#if defined(DAC_CHANNEL2_SUPPORT)
#define MIOS32_DAC_CH0_PIN  LL_GPIO_PIN_4
#else
#define MIOS32_DAC_CH0_PIN  LL_GPIO_PIN_5   // STM32F410: its only output
#endif
#endif

#ifndef MIOS32_DAC_CH1_PORT
#define MIOS32_DAC_CH1_PORT GPIOA
#define MIOS32_DAC_CH1_PIN  LL_GPIO_PIN_5
#endif


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

#if MIOS32_DAC_CHANNEL_MASK

static const u32 chn_ll[2] = {
  LL_DAC_CHANNEL_1,
#if defined(DAC_CHANNEL2_SUPPORT)
  LL_DAC_CHANNEL_2,
#else
  0,  // no channel 2 on this chip; the #error above already refused it
#endif
};

#endif


/////////////////////////////////////////////////////////////////////////////
// Local helpers
/////////////////////////////////////////////////////////////////////////////

#if MIOS32_DAC_CHANNEL_MASK
//! \return the LL channel constant, or 0 if the index is out of range, that
//! channel was not enabled by MIOS32_DAC_CHANNEL_MASK, or the chip has no
//! such channel
static u32 MIOS32_DAC_ChannelCheck(u8 chn)
{
  if( chn > 1 )
    return 0;
  if( !(MIOS32_DAC_CHANNEL_MASK & (1 << chn)) )
    return 0;
  return chn_ll[chn]; // 0 for channel 2 on a single-channel part
}
#endif


/////////////////////////////////////////////////////////////////////////////
//! Initializes the DAC driver
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DAC_Init(u32 mode)
{
  if( mode != 0 )
    return -1; // unsupported mode

#if !MIOS32_DAC_CHANNEL_MASK
  return -1; // no channel selected
#else
  int c;

  // The output pin must be analog even when the buffer is enabled: a pin
  // left in its reset state loads the DAC through its input stage.
  LL_GPIO_InitTypeDef GPIO_InitStructure;
  LL_GPIO_StructInit(&GPIO_InitStructure);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;

#if MIOS32_DAC_CHANNEL_MASK & (1 << 0)
  GPIO_InitStructure.Pin = MIOS32_DAC_CH0_PIN;
  LL_GPIO_Init(MIOS32_DAC_CH0_PORT, &GPIO_InitStructure);
#endif
#if MIOS32_DAC_CHANNEL_MASK & (1 << 1)
  GPIO_InitStructure.Pin = MIOS32_DAC_CH1_PIN;
  LL_GPIO_Init(MIOS32_DAC_CH1_PORT, &GPIO_InitStructure);
#endif

  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_DAC1);

  for(c=0; c<2; ++c) {
    if( !(MIOS32_DAC_CHANNEL_MASK & (1 << c)) )
      continue;
    if( !chn_ll[c] )
      continue; // channel does not exist on this chip

    LL_DAC_Disable(DAC1, chn_ll[c]);

    LL_DAC_SetOutputBuffer(DAC1, chn_ll[c], MIOS32_DAC_OUTPUT_BUFFER);

    // software trigger: a write to the holding register is converted when
    // MIOS32_DAC_ChannelSet() asks for it, not on a timer
    LL_DAC_SetTriggerSource(DAC1, chn_ll[c], LL_DAC_TRIG_SOFTWARE);
    LL_DAC_SetWaveAutoGeneration(DAC1, chn_ll[c], LL_DAC_WAVE_AUTO_GENERATION_NONE);
    LL_DAC_EnableTrigger(DAC1, chn_ll[c]);

    // start at zero rather than at whatever the register held
    LL_DAC_ConvertData12RightAligned(DAC1, chn_ll[c], 0);

    LL_DAC_Enable(DAC1, chn_ll[c]);
    LL_DAC_TrigSWConversion(DAC1, chn_ll[c]);
  }

  // the output buffer needs its startup time before the pin is trustworthy
  MIOS32_DELAY_Wait_uS(LL_DAC_DELAY_STARTUP_VOLTAGE_SETTLING_US + 1);

  return 0;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Writes a channel and converts it immediately.
//! \param[in] chn MIOS32_DAC_CHANNEL0 or MIOS32_DAC_CHANNEL1
//! \param[in] value 12-bit, right aligned
//! \return < 0 if the channel does not exist or is not enabled
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DAC_ChannelSet(u8 chn, u16 value)
{
#if !MIOS32_DAC_CHANNEL_MASK
  return -1;
#else
  u32 ll_chn = MIOS32_DAC_ChannelCheck(chn);
  if( !ll_chn )
    return -1;

  LL_DAC_ConvertData12RightAligned(DAC1, ll_chn, value & 0xfff);
  LL_DAC_TrigSWConversion(DAC1, ll_chn);

  return 0;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Reads back what the channel is currently driving.
//! \return < 0 if the channel does not exist or is not enabled
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DAC_ChannelGet(u8 chn)
{
#if !MIOS32_DAC_CHANNEL_MASK
  return -1;
#else
  u32 ll_chn = MIOS32_DAC_ChannelCheck(chn);
  if( !ll_chn )
    return -1;

  return LL_DAC_RetrieveOutputData(DAC1, ll_chn);
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Turns one channel on or off without touching its value.
//! \return < 0 if the channel does not exist or is not enabled
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DAC_ChannelEnable(u8 chn, u8 enable)
{
#if !MIOS32_DAC_CHANNEL_MASK
  return -1;
#else
  u32 ll_chn = MIOS32_DAC_ChannelCheck(chn);
  if( !ll_chn )
    return -1;

  if( enable ) {
    LL_DAC_Enable(DAC1, ll_chn);
    MIOS32_DELAY_Wait_uS(LL_DAC_DELAY_STARTUP_VOLTAGE_SETTLING_US + 1);
  } else {
    LL_DAC_Disable(DAC1, ll_chn);
  }

  return 0;
#endif
}

//! \}

#endif /* defined(DAC1) */

#endif /* MIOS32_USE_DAC */
