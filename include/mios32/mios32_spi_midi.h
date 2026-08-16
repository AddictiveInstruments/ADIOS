// $Id$
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

// Carries MIDI packages over an SPI link, to a board that offers one or
// more MIDI ports at the far end.
//
// THIS TRANSPORT IS TRANSPARENT: it moves packages and knows nothing about
// what sits at the other end. Whatever is specific to one board - its
// command set, its GPIOs, its status reporting - belongs in that board's
// own driver under modules/, not here.
//
// HOW TO USE IT
// -------------
// 1. In your mios32_config.h, ask for it, name the SPI port, and say how
//    many MIDI ports the board at the far end offers. The SPI port has to
//    be declared as well:
//
//      #define MIOS32_USE_SPI_MIDI        1
//      #define MIOS32_USE_SPI1            1
//      #define MIOS32_SPI_MIDI_SPI        1
//      #define MIOS32_SPI_MIDI_NUM_PORTS  4
//
// 2. Then use those ports like any other MIDI ports - send to SPIM0..SPIM15,
//    and received packages arrive through the usual MIDI receive hook.
//    Nothing specific to this transport has to be called.
//
// 3. Only if you are writing the driver for the board at the far end, and it
//    speaks its own protocol alongside MIDI: register with
//    MIOS32_SPI_MIDI_RawWordCallback_Init() to see each received word before
//    it is parsed. See that prototype below.
//
// The settings that follow describe the link itself. A board driver normally
// declares them on your behalf, so read its header before setting them here.

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

// The chip select line is not named here: a port has one, declared as
// MIOS32_SPIn_CS_PORT/_PIN in the family driver, and this transport drives
// it through MIOS32_SPI_CS_PinSet(port, level).

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

// Lets a board driver see each received word BEFORE it is parsed as MIDI,
// so a board can carry its own protocol on the same link. Install it from
// the board driver's own init:
//
//   static s32 MyBoard_RawWord(u32 word)
//   {
//     if( (word & 0x0f000000) == 0x01000000 ) { ...; return 1; } // mine
//     return 0;                                                  // MIDI
//   }
//   MIOS32_SPI_MIDI_RawWordCallback_Init(MyBoard_RawWord);
//
// Return 1 to consume the word, 0 to let it through to the MIDI parser.
extern s32 MIOS32_SPI_MIDI_RawWordCallback_Init(s32 (*callback_raw_word)(u32 word));

// Note that running status optimisation is not implemented by this
// transport: MIOS32_MIDI_RS_OptimisationSet() answers -1 for the SPIM range.
// If the board at the far end offers it, it does so through its own driver.

/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////



#endif /* _MIOS32_SPI_MIDI_H */
