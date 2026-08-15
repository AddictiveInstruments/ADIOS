// $Id$
/*
 * Header file for the ADC driver
 *
 * Replaces MIOS32_AIN on 2026-08-14. "ADC" is what every ST reference manual
 * calls this peripheral; "AIN" meant "the analog inputs of a MIDIbox core
 * module", and that is precisely why the old driver carried a board pinout
 * hard-coded in its channel table (J5A.A0 -> PC1, J5A.A1 -> PC2, ...) - the
 * last surviving piece of the frozen-connector API deleted with mios32_board
 * on 2026-08-11. Renaming it to what the silicon calls it removes the place
 * where a board could hide.
 *
 * What a port of old code has to know:
 *
 *   - THE CHANNEL MASK MEANS SOMETHING ELSE NOW. MIOS32_AIN_CHANNEL_MASK
 *     selected among 8 fixed J5 pins. MIOS32_ADCn_CHANNEL_MASK selects
 *     HARDWARE CHANNELS: bit c enables channel c of that ADC. Nothing is
 *     remapped, and nothing is compacted - MIOS32_ADC_ChannelGet() and the
 *     notification callback both speak in hardware channel numbers. On G0
 *     that means the valid bits are 0..11 and 15..18, because channels 12,
 *     13 and 14 are the temperature sensor, VREFINT and VBAT (see the
 *     channel map at the head of mios32/STM32G0xx/mios32_adc.c).
 *
 *   - THE AINX4 MULTIPLEXER SUPPORT IS GONE. MIOS32_AIN_MUX_PINS, the three
 *     select lines on J5C and the mux_selection_order[] table were the
 *     MBHP_AINX4 module: an external board, driven from inside a peripheral
 *     driver. External analog expanders belong in modules/, next to ainser.
 *
 *   - THE APPLICATION HOOK GAINED THE PORT. APP_AIN_NotifyChange(pin, value)
 *     became APP_ADC_NotifyChange(port, chn, value), because an F4 can scan
 *     three ADCs at once and "pin 3" would otherwise be ambiguous.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
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
// HOW it is done differs, and deliberately so: the G0 has a hardware
// oversampler (ratio 2..256, with its own right-shift) and uses it, costing
// nothing; the F4 has none at all and accumulates in the DMA interrupt, the
// way the old AIN driver did on every family. Same knob, same result, one
// of the two families simply gets it for free.
#ifndef MIOS32_ADC_OVERSAMPLING_RATE
#define MIOS32_ADC_OVERSAMPLING_RATE 1
#endif

// The accumulator is 16 bits wide on both families - it is the ADC data
// register itself on G0, and a u16 array on F4 - so a sum of 12-bit samples
// stops fitting above 16 conversions (16 * 4095 = 65520, 32 would wrap).
// The old AIN driver had exactly the same u16 limit and never checked it:
// MIOS32_AIN_OVERSAMPLING_RATE above 16 silently overflowed and produced
// garbage that looked like noise. It is checked here instead.
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
