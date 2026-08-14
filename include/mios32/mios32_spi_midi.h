// $Id: mios32_spi_midi.h 2097 2014-12-05 22:05:12Z tk $
/*
 * Header file for SPI MIDI functions
 *
 * ==========================================================================
 *
 *  Copyright (C) 2014 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _MIOS32_SPI_MIDI_H
#define _MIOS32_SPI_MIDI_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// THIS TRANSPORT IS TRANSPARENT. It moves MIDI packages over an SPI link and
// knows nothing about what sits at the other end. Everything specific to one
// particular board - the M16 FPGA interface: its command set, its GPIO
// groups, its status reporting, its running-status control - left for
// modules/m16 on 2026-08-14, where it belongs. What used to be a
// "#if defined MIOS32_SPI_MIDI_USE_M16" branch inside each #ifndef below is
// now simply a value that the board's project declares for itself.
//
// A board driver that needs to see the raw words before they are parsed as
// MIDI registers itself with MIOS32_SPI_MIDI_RawWordCallback_Init().

// how many SPI MIDI ports are available?
// if 0: interface disabled (default)
// other allowed values: 1..16
#ifndef MIOS32_SPI_MIDI_NUM_PORTS
#define MIOS32_SPI_MIDI_NUM_PORTS 0
#endif

// Which SPI port carries the link. The port itself must be declared too
// (MIOS32_USE_SPI0 / _SPI1 / _SPI2) - see the #error in mios32_spi_midi.c.
#ifndef MIOS32_SPI_MIDI_SPI
#define MIOS32_SPI_MIDI_SPI 0
#endif

// (MIOS32_SPI_MIDI_SPI_RC_PIN was defined here and used NOWHERE - the fourth
// dead RC_PIN macro found on 2026-08-13/14, after those of SDCARD and SRIO.
// This one was funnier than the others: both branches of its #if defined
// MIOS32_SPI_MIDI_USE_M16 returned 1. Leftover from the MBHP boards, where
// one SPI port exposed two chip select lines - J16:RC1/RC2, J19:RC1/RC2. A
// port now has a single CS, driven through MIOS32_SPI_CS_PinSet(port, level).)

// Which transfer rate should be used?
// MIOS32_SPI_PRESCALER_16 typically results into ca. 5 MBit/s
#ifndef MIOS32_SPI_MIDI_SPI_PRESCALER
#define MIOS32_SPI_MIDI_SPI_PRESCALER MIOS32_SPI_PRESCALER_16
#endif

// DMA buffer size
// Note: should match with transfer rate
// e.g. with MIOS32_SPI_PRESCALER_64 (typically ca. 2 MBit) the transfer of 1 word takes ca. 18 uS
// MIOS32_SPI_MIDI_Tick() is serviced each mS
// Accordingly, we shouldn't send more than 55 words, taking 50 seems to be a good choice.
#ifndef MIOS32_SPI_MIDI_SCAN_BUFFER_SIZE
#define MIOS32_SPI_MIDI_SCAN_BUFFER_SIZE 16
#endif

// note also, that the resulting RAM consumption will be 4*4*MIOS32_SPI_MIDI_BUFFER_SIZE bytes (Rx/Tx double buffers for 4byte words)

// we've a separate RX ringbuffer which stores received MIDI events until the
// application processes them
#ifndef MIOS32_SPI_MIDI_RX_RINGBUFFER_SIZE
#define MIOS32_SPI_MIDI_RX_RINGBUFFER_SIZE 64
#endif



/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////

// (the M16 command and GPIO-mode enums lived here; they are the FPGA's
// vocabulary, not the transport's, and moved to modules/m16/m16.h)

/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_SPI_MIDI_Init(u32 mode);

extern s32 MIOS32_SPI_MIDI_Enabled(void);

extern s32 MIOS32_SPI_MIDI_CheckAvailable(u8 spi_midi_port);
extern s32 MIOS32_SPI_MIDI_Periodic_mS(void);

extern s32 MIOS32_SPI_MIDI_PackageSend_NonBlocking(mios32_midi_package_t package);
extern s32 MIOS32_SPI_MIDI_PackageSend(mios32_midi_package_t package);
extern s32 MIOS32_SPI_MIDI_PackageReceive(mios32_midi_package_t *package);

// Lets a board driver see each received word BEFORE it is parsed as MIDI.
// The callback returns 1 if it consumed the word, 0 to let it through. This
// is what replaces the M16 interception that used to be hard-wired into
// MIOS32_SPI_MIDI_Periodic_mS() - the transport now offers a hook instead of
// knowing one particular board's status protocol.
extern s32 MIOS32_SPI_MIDI_RawWordCallback_Init(s32 (*callback_raw_word)(u32 word));

// (MIOS32_SPI_MIDI_RS_OptimisationSet/Get were declared here. Their entire
// body was an M16 command carrying a 16-bit port mask, so they went to
// modules/m16 as MIOS32_SPIM_M16_RS_OptimisationSet/Get. The generic
// MIOS32_MIDI_RS_OptimisationSet() now answers -1 for the SPIM range, just
// as it already did for CAN: this transport does not implement running
// status optimisation, the board at the far end does.)

/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////



#endif /* _MIOS32_SPI_MIDI_H */
