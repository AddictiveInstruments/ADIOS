/*
 * Header file for the DAC driver
 *
 * Drives the on-chip digital-to-analog converter: you write a 12-bit number,
 * the pin holds the matching voltage between VSSA and VREF+.
 *
 *
 * HOW TO USE IT
 * =============
 *
 * 1. In your adios_config.h, ask for the DAC and say which of its channels
 *    you want. Bit n of the mask is ADIOS_DAC_CHANNELn:
 *
 *      #define ADIOS_USE_DAC          1
 *      #define ADIOS_DAC_CHANNEL_MASK 0x3   // both channels
 *
 *    The pins are configured, the channels enabled and set to zero for you;
 *    nothing has to be called to start.
 *
 *    Not every chip has a DAC, and not every one that does has two channels.
 *    Asking for one that does not exist is refused at compile time with a
 *    message naming what the chip really has - see the family driver,
 *    adios/<FAMILY>/adios_dac.c, which also lists the output pins.
 *
 * 2. Write a voltage:
 *
 *      ADIOS_DAC_ChannelSet(ADIOS_DAC_CHANNEL0, 2048);   // half scale
 *
 *    The value is 12-bit right aligned, 0..4095, and it is converted
 *    immediately - there is no trigger to arm and no buffer to flush. Full
 *    scale is VREF+, so 2048 is about VREF+/2.
 *
 * 3. Read back what a channel is driving, or switch one off without losing
 *    its value:
 *
 *      s32 v = ADIOS_DAC_ChannelGet(ADIOS_DAC_CHANNEL0);
 *      ADIOS_DAC_ChannelEnable(ADIOS_DAC_CHANNEL1, 0);
 *
 * BEFORE YOU WIRE IT: decide about the output buffer, below. It is enabled
 * by default, which lets the pin drive a real load but costs you both ends
 * of the range - the output can reach neither 0 V nor VREF+. If the extremes
 * matter, disable it and put an op-amp behind the pin.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _ADIOS_DAC_H
#define _ADIOS_DAC_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// There is exactly ONE DAC instance on both families - no line of either has
// a DAC2 - so this switch carries no index. An index that can only ever be 0
// is noise. Channels DO have indices, see below.
//
// Unlike SPI/I2C/ADC there is nothing to auto-derive: a project asking for
// the DAC writes ADIOS_USE_DAC and that is the whole declaration.
// core/main.c tests it to decide whether to call
// ADIOS_DAC_Init().


// Channel indices, following the same rule as every other peripheral here:
// ADIOS_DAC_CHANNELn is hardware channel (n+1).
//
// Channel 2 does not exist everywhere. It is missing on the STM32F410 - the
// only F4 with a single-channel DAC - and the family driver refuses it with
// an #error naming the part. Every G0 that has a DAC at all has both.
#define ADIOS_DAC_CHANNEL0     0
#define ADIOS_DAC_CHANNEL1     1


// Which channels to enable, bit n for ADIOS_DAC_CHANNELn.
// Default 0 = the DAC is initialised and drives nothing, which is almost
// certainly a mistake; the family driver says so with a #warning.
#ifndef ADIOS_DAC_CHANNEL_MASK
#define ADIOS_DAC_CHANNEL_MASK 0
#endif


// Output buffer. Enabled, the DAC can drive a real load (a few hundred ohms)
// but it cannot reach either rail - roughly 0.2 V to VDD-0.2 V. Disabled,
// the output swings the full range but only into a high impedance, so it
// needs an external op-amp behind it.
//
// Enabled by default because the common case is driving something directly.
// Set to LL_DAC_OUTPUT_BUFFER_DISABLE when an op-amp follows, or when the
// bottom and top of the range actually matter - a CV output, typically.
#ifndef ADIOS_DAC_OUTPUT_BUFFER
#define ADIOS_DAC_OUTPUT_BUFFER LL_DAC_OUTPUT_BUFFER_ENABLE
#endif


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 ADIOS_DAC_Init(u32 mode);

//! Writes a channel and converts it immediately (software trigger).
//! \param[in] chn ADIOS_DAC_CHANNEL0 or ADIOS_DAC_CHANNEL1
//! \param[in] value 12-bit, right aligned - 0..4095, higher bits ignored
//! \return < 0 if the channel does not exist or is not enabled
extern s32 ADIOS_DAC_ChannelSet(u8 chn, u16 value);

//! Reads back what the channel is currently driving, from its output
//! register - not from a copy kept here.
//! \return < 0 if the channel does not exist or is not enabled
extern s32 ADIOS_DAC_ChannelGet(u8 chn);

//! Turns one channel on or off without touching its value.
//! \return < 0 if the channel does not exist or is not enabled
extern s32 ADIOS_DAC_ChannelEnable(u8 chn, u8 enable);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#endif /* _ADIOS_DAC_H */
