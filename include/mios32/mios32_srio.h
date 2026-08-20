/*
 * Header file for SRIO Driver
 *
 * Clocks a chain of shift registers over SPI: shifts input registers in and
 * output registers out, in one transfer, repeated every millisecond.
 *
 * This is the engine only - it moves bytes to and from the chain and knows
 * nothing about what they mean. Turning those bytes into button presses or
 * LED states is the job of the modules above it: MIOS32_SRIN for inputs,
 * MIOS32_SROUT for outputs, MIOS32_ENC for rotary encoders. Each of those
 * needs this one, and says so at compile time if it is missing.
 *
 * HOW TO USE IT
 * -------------
 * 1. In your mios32_config.h, ask for it, name the SPI port the chain is
 *    wired to, and say how many registers are in it. The SPI port has to be
 *    declared as well:
 *
 *      #define MIOS32_USE_SRIO    1
 *      #define MIOS32_USE_SPI1    1
 *      #define MIOS32_SRIO_SPI    1
 *      #define MIOS32_SRIO_NUM_SR 4    // 4 x 8 bits in, 4 x 8 bits out
 *
 * 2. Usually, do nothing else: add MIOS32_USE_SRIN or MIOS32_USE_SROUT and
 *    work in pins rather than in registers. If you do want the raw bytes,
 *    they are in mios32_srio_din[] and mios32_srio_dout[], one entry per
 *    register, refreshed by the scan.
 *
 * 3. To act on the chain at a precise moment in the scan - driving a matrix
 *    row before its columns are sampled, typically - install a callback
 *    with MIOS32_SRIO_ScanStart(). It runs just before each transfer.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _MIOS32_SRIO_H
#define _MIOS32_SRIO_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// 16 should be maximum, more registers would require buffers at the SCLK/RCLK lines
// and probably also a lower scan frequency
// The number of SRs can be optionally overruled from the local mios32_config.h file
#ifndef MIOS32_SRIO_NUM_SR
#define MIOS32_SRIO_NUM_SR 16
#endif


// how many DOUT pages are supported (used for dimmed LED and optimized matrix handling support)
#ifndef MIOS32_SRIO_NUM_DOUT_PAGES
#define MIOS32_SRIO_NUM_DOUT_PAGES 1
#endif

// Which SPI port carries the shift register chain. The port itself must be
// declared too (MIOS32_USE_SPI0 / _SPI1 / _SPI2) - see the #error in
// mios32_srio.c, which says so at compile time rather than letting the link
// fail on five missing MIOS32_SPI_* symbols.
#ifndef MIOS32_SRIO_SPI
#define MIOS32_SRIO_SPI 1
#endif

// The latch line is not named here: a port has one chip select, declared as
// MIOS32_SPIn_CS_PORT/_PIN in the family driver, and this module strobes it
// through MIOS32_SPI_CS_PinSet(port, level) to load and latch the chain.

// should output pins be used in Open Drain mode? (perfect for 3.3V->5V levelshifting)
#ifndef MIOS32_SRIO_OUTPUTS_OD
#define MIOS32_SRIO_OUTPUTS_OD 0
#endif



/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_SRIO_Init(u32 mode);

extern u8  MIOS32_SRIO_ScanNumGet(void);
extern s32 MIOS32_SRIO_ScanNumSet(u8 new_num_sr);

extern s32 MIOS32_SRIO_DoutPageGet(void);

extern u32 MIOS32_SRIO_DebounceGet(void);
extern s32 MIOS32_SRIO_DebounceSet(u16 debounce_time);
extern s32 MIOS32_SRIO_DebounceStart(void);

extern s32 MIOS32_SRIO_ScanStart(void *notify_hook);



/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////

extern volatile u8 mios32_srio_dout[MIOS32_SRIO_NUM_DOUT_PAGES][MIOS32_SRIO_NUM_SR];
extern volatile u8 mios32_srio_din[MIOS32_SRIO_NUM_SR];
extern volatile u8 mios32_srio_din_buffer[MIOS32_SRIO_NUM_SR]; // only required for emulation
extern volatile u8 mios32_srio_din_changed[MIOS32_SRIO_NUM_SR];

// the current DOUT page
#if MIOS32_SRIO_NUM_DOUT_PAGES > 1
extern u8 mios32_srio_dout_page_ctr;
#endif

#endif /* _MIOS32_SRIO_H */
