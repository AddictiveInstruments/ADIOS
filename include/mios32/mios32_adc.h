/*
 * Header file for the ADC driver
 *
 * Scans a set of analog inputs by DMA, applies a deadband so that only real
 * movement is reported, and calls the application back with what moved.
 *
 *
 * HOW TO USE IT
 * =============
 *
 * 1. In your mios32_config.h, name the ADC you want and the channels it must
 *    convert. MIOS32_ADCn is ADC(n+1), and the bits of the mask are HARDWARE
 *    channel numbers:
 *
 *      #define MIOS32_USE_ADC0          1        // ADC1
 *      #define MIOS32_ADC0_CHANNEL_MASK 0x000f   // its channels 0,1,2,3
 *
 *    That is the whole declaration - MIOS32_USE_ADC derives itself, and the
 *    scan starts on its own. Nothing else has to be called to get going: the
 *    driver is initialised for you and rescanned every millisecond.
 *
 *    WHICH channel numbers are legal depends on the chip, and it is not the
 *    same on both families - the STM32G0 in particular does not number its
 *    inputs 0..15. Read the channel map at the head of the family driver,
 *    mios32/<FAMILY>/mios32_adc.c, before choosing a mask; an impossible
 *    choice is refused there at compile time with a message that says why.
 *
 * 2. Receive movement in your application:
 *
 *      void APP_ADC_NotifyChange(u32 port, u32 chn, u32 value)
 *      {
 *        // chn is the hardware channel number, exactly as in the mask above
 *        // value is 0..4095, or 0..(4095 * oversampling rate)
 *      }
 *
 *    This is called once per channel that moved by more than the deadband,
 *    and never for a channel that merely trembles.
 *
 * 3. Or read a channel whenever you like, instead of waiting to be told:
 *
 *      s32 v = MIOS32_ADC_ChannelGet(MIOS32_ADC_PORT_ADC0, 2);
 *
 *    It returns the last published value, or < 0 if that channel was not in
 *    the mask.
 *
 * The deadband can be widened or narrowed while running with
 * MIOS32_ADC_DeadbandSet(); everything else is a compile-time setting, and
 * every one of them can be overridden from mios32_config.h - see the list
 * below and the per-family list in the driver.
 *
 *
 * TWO THINGS WORTH KNOWING BEFORE YOU WIRE ANYTHING
 * =================================================
 *
 * Channel numbers are the silicon's. Nothing is remapped and nothing is
 * compacted: the mask, MIOS32_ADC_ChannelGet() and the callback all speak
 * the numbering of the reference manual. If you enable channels 3 and 9 you
 * are called back with 3 and 9, not with 0 and 1.
 *
 * Driving an external analog multiplexer is not this driver's job. If you
 * need one, switch it from the callback installed by
 * MIOS32_ADC_ServicePrepareCallback_Init(): it runs before each scan and can
 * postpone that scan while the multiplexer settles.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _MIOS32_ADC_H
#define _MIOS32_ADC_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// Auto-derive the master switch from any individual port actually wanted -
// no need for the project to set MIOS32_USE_ADC on top of MIOS32_USE_ADC0/1/2.
// Same reasoning (and same placement) as MIOS32_USE_SPI in mios32_spi.h and
// MIOS32_USE_I2C in mios32_i2c.h: programming_models/traditional/main.c tests
// the bare master switch to decide whether to call MIOS32_ADC_Init(), and a
// #define made inside mios32_adc.c would never reach that translation unit.
#if !defined(MIOS32_USE_ADC) && (defined(MIOS32_USE_ADC0) || defined(MIOS32_USE_ADC1) || defined(MIOS32_USE_ADC2))
#define MIOS32_USE_ADC
#endif


// Port indices accepted by every function below, following the same rule as
// UART and I2C: MIOS32_ADCn is ADC(n+1). Port 0 = ADC1, port 1 = ADC2,
// port 2 = ADC3.
//
// Which of them exist is decided by the family driver from the processor:
// ADC2 and ADC3 are present only on the "big" STM32F4 lines (F405, F407,
// F415, F417, F427, F429, F437, F439, F446, F469, F479) and on NO G0 at all.
// Requesting a port the chip doesn't have is a compile-time #error naming
// the port, not a runtime surprise. See the tier tables at the head of
// mios32/<FAMILY>/mios32_adc.c.
#define MIOS32_ADC_PORT_ADC0    0
#define MIOS32_ADC_PORT_ADC1    1
#define MIOS32_ADC_PORT_ADC2    2


// Which hardware channels to convert, one mask per ADC. Bit c enables
// channel c. Default 0 = that ADC is configured but scans nothing, which is
// almost certainly a mistake - the family driver says so with a #warning.
//
// The channel numbers are the silicon's, NOT a board's. Consult the channel
// map in the family driver before setting these: on G0 the usable bits are
// 0..11 and 15..18, on F4 ADC1/ADC2 they are 0..15, and ADC3 has a table of
// its own because half of its inputs sit on PORTF.
#ifndef MIOS32_ADC0_CHANNEL_MASK
#define MIOS32_ADC0_CHANNEL_MASK 0
#endif
#ifndef MIOS32_ADC1_CHANNEL_MASK
#define MIOS32_ADC1_CHANNEL_MASK 0
#endif
#ifndef MIOS32_ADC2_CHANNEL_MASK
#define MIOS32_ADC2_CHANNEL_MASK 0
#endif


// Oversampling: accumulate this many conversions before publishing a value.
// 1 disables it. The result keeps the sum, so the reported value spans
// 12 bits * rate - with rate 4 the range is 0..16383, not 0..4095.
//
// HOW it is done differs, and deliberately so: the STM32G0 has a hardware
// oversampler (ratio 2..256, with its own right-shift) and uses it, costing
// nothing; the STM32F4 has none at all and accumulates in the DMA interrupt.
// Same knob, same result, one of the two families simply gets it for free.
#ifndef MIOS32_ADC_OVERSAMPLING_RATE
#define MIOS32_ADC_OVERSAMPLING_RATE 1
#endif

// The accumulator is 16 bits wide on both families - it is the ADC data
// register itself on G0, and a u16 array on F4 - so a sum of 12-bit samples
// stops fitting above 16 conversions (16 * 4095 = 65520, 32 would wrap).
// Checked here rather than left to overflow into something that looks like
// noise on an oscilloscope.
#if MIOS32_ADC_OVERSAMPLING_RATE > 16
# error "MIOS32_ADC_OVERSAMPLING_RATE > 16 overflows the 16-bit accumulator (rate * 4095 must stay below 65536). Use 16 at most, or divide in the application."
#endif
#if MIOS32_ADC_OVERSAMPLING_RATE < 1
# error "MIOS32_ADC_OVERSAMPLING_RATE must be at least 1 (1 = no oversampling)."
#endif


// A new conversion result is published, and the application notified, only
// once it differs from the last published one by more than this. Without it
// the last bits of a 12-bit reading dance permanently on any real pot.
//
// Runtime-adjustable through MIOS32_ADC_DeadbandSet(); this is the value
// installed at Init.
#ifndef MIOS32_ADC_DEADBAND
#define MIOS32_ADC_DEADBAND 31
#endif


// A wider deadband, applied to a channel that has been still for a while.
// A channel leaves the idle state as soon as this wider band is exceeded,
// and returns to it after MIOS32_ADC_IDLE_CTR conversions without movement.
// This is what keeps EMI from waking up a knob nobody is touching.
// Set to 0 to disable, in which case the normal deadband always applies.
#ifndef MIOS32_ADC_DEADBAND_IDLE
#define MIOS32_ADC_DEADBAND_IDLE 127
#endif

// How many quiet conversions send a channel into the idle state.
// Allowed range 1..65535.
#ifndef MIOS32_ADC_IDLE_CTR
#define MIOS32_ADC_IDLE_CTR 3000
#endif


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_ADC_Init(u32 mode);

//! Last published value of a channel, in hardware channel numbering.
//! Resolution depends on MIOS32_ADC_OVERSAMPLING_RATE.
//! < 0 if the port or the channel is not enabled.
extern s32 MIOS32_ADC_ChannelGet(u8 port, u8 chn);

extern s32 MIOS32_ADC_DeadbandGet(void);
extern s32 MIOS32_ADC_DeadbandSet(u16 deadband);

//! Calls the given callback for every channel that moved since last time:
//! \code
//!   void APP_ADC_NotifyChange(u32 port, u32 chn, u32 value)
//! \endcode
//! then starts the next scan.
extern s32 MIOS32_ADC_Handler(void *callback);

extern s32 MIOS32_ADC_StartConversions(void);

//! Optional callback run before each scan is started, to settle external
//! analog switching. The scan starts if it returns 0, and is skipped if it
//! returns >= 1, which is how a caller inserts a settling delay.
extern s32 MIOS32_ADC_ServicePrepareCallback_Init(void *callback);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#endif /* _MIOS32_ADC_H */
