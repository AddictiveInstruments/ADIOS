/*
 * Header file for SRIO Driver
 *
 * Clocks a chain of shift registers over SPI: shifts input registers in and
 * output registers out, in one transfer, repeated every millisecond.
 *
 * This is the engine only - it moves bytes to and from the chain and knows
 * nothing about what they mean. Turning those bytes into button presses or
 * LED states is the job of the modules above it: ADIOS_SRIN for inputs,
 * ADIOS_SROUT for outputs, ADIOS_ENC for rotary encoders. Each of those
 * needs this one, and says so at compile time if it is missing.
 *
 * HOW TO USE IT
 * -------------
 * 1. In your adios_config.h, ask for it, name the SPI port the chain is
 *    wired to, and say how many registers are in it. The SPI port has to be
 *    declared as well:
 *
 *      #define ADIOS_USE_SRIO    1
 *      #define ADIOS_USE_SPI1    1
 *      #define ADIOS_SRIO_SPI    1
 *      #define ADIOS_SRIO_NUM_SR 4    // 4 x 8 bits in, 4 x 8 bits out
 *
 * 2. Usually, do nothing else: add ADIOS_USE_SRIN or ADIOS_USE_SROUT and
 *    work in pins rather than in registers. If you do want the raw bytes,
 *    they are in adios_srio_din[] and adios_srio_dout[], one entry per
 *    register, refreshed by the scan.
 *
 * 3. To act on the chain at a precise moment in the scan - driving a matrix
 *    row before its columns are sampled, typically - install a callback
 *    with ADIOS_SRIO_ScanStart(). It runs just before each transfer.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _ADIOS_SRIO_H
#define _ADIOS_SRIO_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// 16 should be maximum, more registers would require buffers at the SCLK/RCLK lines
// and probably also a lower scan frequency
// The number of SRs can be optionally overruled from the local adios_config.h file
#ifndef ADIOS_SRIO_NUM_SR
#define ADIOS_SRIO_NUM_SR 16
#endif


// how many DOUT pages are supported (used for dimmed LED and optimized matrix handling support)
#ifndef ADIOS_SRIO_NUM_DOUT_PAGES
#define ADIOS_SRIO_NUM_DOUT_PAGES 1
#endif

// Which SPI port carries the shift register chain. The port itself must be
// declared too (ADIOS_USE_SPI0 / _SPI1 / _SPI2) - see the #error in
// adios_srio.c, which says so at compile time rather than letting the link
// fail on five missing ADIOS_SPI_* symbols.
#ifndef ADIOS_SRIO_SPI
#define ADIOS_SRIO_SPI 1
#endif

// The latch line is not named here: a port has one chip select, declared as
// ADIOS_SPIn_CS_PORT/_PIN in the family driver, and this module strobes it
// through ADIOS_SPI_CS_PinSet(port, level) to load and latch the chain.

// should output pins be used in Open Drain mode? (perfect for 3.3V->5V levelshifting)
#ifndef ADIOS_SRIO_OUTPUTS_OD
#define ADIOS_SRIO_OUTPUTS_OD 0
#endif



/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 ADIOS_SRIO_Init(u32 mode);

extern u8  ADIOS_SRIO_ScanNumGet(void);
extern s32 ADIOS_SRIO_ScanNumSet(u8 new_num_sr);

extern s32 ADIOS_SRIO_DoutPageGet(void);

extern u32 ADIOS_SRIO_DebounceGet(void);
extern s32 ADIOS_SRIO_DebounceSet(u16 debounce_time);
extern s32 ADIOS_SRIO_DebounceStart(void);

extern s32 ADIOS_SRIO_ScanStart(void *notify_hook);



/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////

extern volatile u8 adios_srio_dout[ADIOS_SRIO_NUM_DOUT_PAGES][ADIOS_SRIO_NUM_SR];
extern volatile u8 adios_srio_din[ADIOS_SRIO_NUM_SR];
extern volatile u8 adios_srio_din_buffer[ADIOS_SRIO_NUM_SR]; // only required for emulation
extern volatile u8 adios_srio_din_changed[ADIOS_SRIO_NUM_SR];

// the current DOUT page
#if ADIOS_SRIO_NUM_DOUT_PAGES > 1
extern u8 adios_srio_dout_page_ctr;
#endif

#endif /* _ADIOS_SRIO_H */
