// $Id$
/*
 * Header file for the DAC driver
 *
 * New on 2026-08-14. There was no DAC driver before: mios32_board carried a
 * MIOS32_BOARD_DAC_PinInit()/PinSet() pair, which went with the rest of the
 * frozen-connector API on 2026-08-11 and had no living caller.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

#ifndef _MIOS32_DAC_H
#define _MIOS32_DAC_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// There is exactly ONE DAC instance on both families - no line of either has
// a DAC2 - so this switch carries no index. An index that can only ever be 0
// is noise. Channels DO have indices, see below.
//
// Unlike SPI/I2C/ADC there is nothing to auto-derive: a project asking for
// the DAC writes MIOS32_USE_DAC and that is the whole declaration.
// programming_models/traditional/main.c tests it to decide whether to call
// MIOS32_DAC_Init().


// Channel indices, following the same rule as every other peripheral here:
// MIOS32_DAC_CHANNELn is hardware channel (n+1).
//
// Channel 2 does not exist everywhere. It is missing on the STM32F410 - the
// only F4 with a single-channel DAC - and the family driver refuses it with
// an #error naming the part. Every G0 that has a DAC at all has both.
#define MIOS32_DAC_CHANNEL0     0
#define MIOS32_DAC_CHANNEL1     1


// Which channels to enable, bit n for MIOS32_DAC_CHANNELn.
// Default 0 = the DAC is initialised and drives nothing, which is almost
// certainly a mistake; the family driver says so with a #warning.
#ifndef MIOS32_DAC_CHANNEL_MASK
#define MIOS32_DAC_CHANNEL_MASK 0
#endif


// Output buffer. Enabled, the DAC can drive a real load (a few hundred ohms)
// but it cannot reach either rail - roughly 0.2 V to VDD-0.2 V. Disabled,
// the output swings the full range but only into a high impedance, so it
// needs an external op-amp behind it.
//
// Enabled by default because the common case is driving something directly.
// Set to LL_DAC_OUTPUT_BUFFER_DISABLE when an op-amp follows, or when the
// bottom and top of the range actually matter - a CV output, typically.
#ifndef MIOS32_DAC_OUTPUT_BUFFER
#define MIOS32_DAC_OUTPUT_BUFFER LL_DAC_OUTPUT_BUFFER_ENABLE
#endif


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_DAC_Init(u32 mode);

//! Writes a channel and converts it immediately (software trigger).
//! \param[in] chn MIOS32_DAC_CHANNEL0 or MIOS32_DAC_CHANNEL1
//! \param[in] value 12-bit, right aligned - 0..4095, higher bits ignored
//! \return < 0 if the channel does not exist or is not enabled
extern s32 MIOS32_DAC_ChannelSet(u8 chn, u16 value);

//! Reads back what the channel is currently driving, from its output
//! register - not from a copy kept here.
//! \return < 0 if the channel does not exist or is not enabled
extern s32 MIOS32_DAC_ChannelGet(u8 chn);

//! Turns one channel on or off without touching its value.
//! \return < 0 if the channel does not exist or is not enabled
extern s32 MIOS32_DAC_ChannelEnable(u8 chn, u8 enable);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#endif /* _MIOS32_DAC_H */
