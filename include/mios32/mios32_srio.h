// $Id: mios32_srio.h 2400 2016-08-12 22:53:59Z tk $
/*
 * Header file for SRIO Driver
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
//
// (The default used to be chosen by board: 0 on an STM32_PRIMER, "since RCLK
// conflicts with USB detach pin @B12", 1 everywhere else. That board is not
// defined anywhere in this tree, so the branch always took its #else -
// removed 2026-08-13 along with the two below. Nothing changes for anyone;
// a project that needs another port says so here.)
#ifndef MIOS32_SRIO_SPI
#define MIOS32_SRIO_SPI 1
#endif

// (MIOS32_SRIO_SPI_RC_PIN and MIOS32_SRIO_SPI_RC_PIN2 were defined here and
// used NOWHERE - leftovers from the MBHP boards, where one SPI port exposed
// two chip select lines, J16:RC1 and J16:RC2. A port now has a single CS
// under manual GPIO control, named by MIOS32_SPIn_CS_PORT/_PIN in the family
// driver, and this module drives it through MIOS32_SPI_CS_PinSet(port,
// level). Removed 2026-08-13, same as MIOS32_SDCARD_SPI_RC_PIN.)

// should output pins be used in Open Drain mode? (perfect for 3.3V->5V levelshifting)
// (default was 1 on MBHP_CORE_STM32, 0 elsewhere - that board is not defined
// in this tree either, so 0 is what every build already got)
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
