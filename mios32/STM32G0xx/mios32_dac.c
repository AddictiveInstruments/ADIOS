//! \defgroup MIOS32_DAC
//!
//! DAC driver for MIOS32 - STM32G0xx
//!
//!
//! THE DAC IS THE MARKER OF THE "1" LINES
//! ======================================
//!
//! Half the G0 family has no DAC at all. Verified in the CMSIS headers of
//! all twelve devices in the tree, not assumed:
//!
//!     DAC1, 2 channels ... G051, G061, G071, G081, G0B1, G0C1
//!     NO DAC ............. G030, G031, G041, G050, G070, G0B0
//!
//! On a chip from the second list there is no analog output at all, and a
//! voltage has to come from filtered PWM or from an external part instead.
//! The guard below is #if defined(DAC1), so asking for the DAC on such a
//! chip is answered at compile time rather than by a link failure.
//!
//! Every G0 that has a DAC has BOTH channels - the single-channel case is an
//! F4 peculiarity (the F410).
//!
//!     channel 1 -> PA4       channel 2 -> PA5
//!
//!
//! WHY THIS IS NOT SHARED WITH THE F4 DRIVER
//! =========================================
//!
//! The two LL APIs are nearly identical, and a shared body was considered.
//! The G0's DAC block is a later generation and has two controls the F4 has
//! no notion of: LL_DAC_SetOutputConnection() (route the output to a pin or
//! keep it inside the chip, feeding a comparator or an op-amp) and
//! LL_DAC_SetOutputMode() (sample-and-hold, which lets the output hold its
//! value while the DAC sleeps). Sharing the body would have meant guarding
//! those two inside common code, which is the thing we do not do here.
//! They are set to their plain values below, but they are set - and they
//! belong in the family file.
//!
//! Wave generation (noise and triangle) and DMA-fed output are NOT
//! implemented. The peripheral supports both; nothing here needs them yet.
//!
//! \{
/* ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
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
# error "MIOS32_USE_DAC requested, but this STM32G0 has no DAC. It exists on G051/G061/G071/G081/G0B1/G0C1 only - not on G030, G031, G041, G050, G070 or G0B0."
#else

#if !MIOS32_DAC_CHANNEL_MASK
# warning "MIOS32_USE_DAC is set but MIOS32_DAC_CHANNEL_MASK is 0: the DAC will be initialised and drive nothing."
#endif

#if MIOS32_DAC_CHANNEL_MASK & ~0x3
# error "MIOS32_DAC_CHANNEL_MASK has a bit above 1. This DAC has two channels: bit 0 is MIOS32_DAC_CHANNEL0 (PA4), bit 1 is MIOS32_DAC_CHANNEL1 (PA5)."
#endif


/////////////////////////////////////////////////////////////////////////////
// Overridable settings
/////////////////////////////////////////////////////////////////////////////

// Output pins. Same on every G0 package that carries a DAC.
#ifndef MIOS32_DAC_CH0_PORT
#define MIOS32_DAC_CH0_PORT GPIOA
#define MIOS32_DAC_CH0_PIN  LL_GPIO_PIN_4
#endif
#ifndef MIOS32_DAC_CH1_PORT
#define MIOS32_DAC_CH1_PORT GPIOA
#define MIOS32_DAC_CH1_PIN  LL_GPIO_PIN_5
#endif

// Where the output goes. GPIO is the pin; INTERNAL keeps it inside the chip
// for a comparator or an on-chip op-amp, without occupying the pad.
#ifndef MIOS32_DAC_OUTPUT_CONNECTION
#define MIOS32_DAC_OUTPUT_CONNECTION LL_DAC_OUTPUT_CONNECT_GPIO
#endif

// NORMAL drives continuously. SAMPLE_AND_HOLD refreshes an external
// capacitor periodically and lets the peripheral idle in between, which is
// what makes the DAC usable in the low-power modes. It needs its sample,
// hold and refresh times programmed as well, so it is not a drop-in switch.
#ifndef MIOS32_DAC_OUTPUT_MODE
#define MIOS32_DAC_OUTPUT_MODE LL_DAC_OUTPUT_MODE_NORMAL
#endif


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

#if MIOS32_DAC_CHANNEL_MASK

static const u32 chn_ll[2] = { LL_DAC_CHANNEL_1, LL_DAC_CHANNEL_2 };

#endif


/////////////////////////////////////////////////////////////////////////////
// Local helpers
/////////////////////////////////////////////////////////////////////////////

#if MIOS32_DAC_CHANNEL_MASK
//! \return the LL channel constant, or 0 if the index is out of range or
//! that channel was not enabled by MIOS32_DAC_CHANNEL_MASK
static u32 MIOS32_DAC_ChannelCheck(u8 chn)
{
  if( chn > 1 )
    return 0;
  if( !(MIOS32_DAC_CHANNEL_MASK & (1 << chn)) )
    return 0;
  return chn_ll[chn];
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

  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1); // DAC sits behind the same bus gate on G0
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_DAC1);

  for(c=0; c<2; ++c) {
    if( !(MIOS32_DAC_CHANNEL_MASK & (1 << c)) )
      continue;

    LL_DAC_Disable(DAC1, chn_ll[c]);

    LL_DAC_SetOutputBuffer(DAC1, chn_ll[c], MIOS32_DAC_OUTPUT_BUFFER);
    LL_DAC_SetOutputConnection(DAC1, chn_ll[c], MIOS32_DAC_OUTPUT_CONNECTION);
    LL_DAC_SetOutputMode(DAC1, chn_ll[c], MIOS32_DAC_OUTPUT_MODE);

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
