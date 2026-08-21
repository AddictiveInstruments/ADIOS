/*
 * Header file for SROUT Driver
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _ADIOS_SROUT_H
#define _ADIOS_SROUT_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 ADIOS_SROUT_Init(u32 mode);

extern s32 ADIOS_SROUT_PinGet(u32 pin);
extern s32 ADIOS_SROUT_PinSet(u32 pin, u32 value);

extern s32 ADIOS_SROUT_PagePinGet(u8 page, u32 pin);
extern s32 ADIOS_SROUT_PagePinSet(u8 page, u32 pin, u32 value);

extern s32 ADIOS_SROUT_SRGet(u32 sr);
extern s32 ADIOS_SROUT_SRSet(u32 sr, u8 value);

extern s32 ADIOS_SROUT_PageSRGet(u8 page, u32 sr);
extern s32 ADIOS_SROUT_PageSRSet(u8 page, u32 sr, u8 value);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////

extern const u8 adios_srout_reverse_tab[256];

#endif /* _ADIOS_SROUT_H */
