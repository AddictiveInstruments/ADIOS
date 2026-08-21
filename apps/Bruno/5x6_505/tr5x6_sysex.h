/*
 * BSL SysEx Header
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _TR5X6_SYSEX_H
#define _TR5X6_SYSEX_H

/////////////////////////////////////////////////////////////////////////////
// Exported variables
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// global definitions
/////////////////////////////////////////////////////////////////////////////

// max. received bytes
#define TR5X6_SYSEX_MAX_BYTES 1024

// buffer size (due to scrambling, must be larger than received bytes)
// + some bytes to send the header
#define TR5X6_SYSEX_BUFFER_SIZE (((TR5X6_SYSEX_MAX_BYTES*8)/7) + 20)


/////////////////////////////////////////////////////////////////////////////
// Type definitions
/////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 TR5X6_SYSEX_Init(u32 mode);
extern s32 TR5X6_SYSEX_HaltStateGet(void);
extern s32 TR5X6_SYSEX_ReleaseHaltState(void);
extern s32 TR5X6_SYSEX_Parser(adios_midi_port_t port, u8 midi_in);
extern s32 TR5X6_SYSEX_SendUploadReq(adios_midi_port_t port);

#endif /* _TR5X6_SYSEX_H */
