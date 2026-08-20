/*
 * Header file for UART MIDI functions
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _MIOS32_DIN_MIDI_H
#define _MIOS32_DIN_MIDI_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_DIN_MIDI_Init(u32 mode);

extern s32 MIOS32_DIN_MIDI_CheckAvailable(u8 din_port);

extern s32 MIOS32_DIN_MIDI_RS_OptimisationSet(u8 din_port, u8 enable);
extern s32 MIOS32_DIN_MIDI_RS_OptimisationGet(u8 din_port);
extern s32 MIOS32_DIN_MIDI_RS_Reset(u8 din_port);

extern s32 MIOS32_DIN_MIDI_Periodic_mS(void);

extern s32 MIOS32_DIN_MIDI_PackageSend_NonBlocking(u8 din_port, mios32_midi_package_t package);
extern s32 MIOS32_DIN_MIDI_PackageSend(u8 din_port, mios32_midi_package_t package);
extern s32 MIOS32_DIN_MIDI_PackageReceive(u8 din_port, mios32_midi_package_t *package);




/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////



#endif /* _MIOS32_DIN_MIDI_H */
