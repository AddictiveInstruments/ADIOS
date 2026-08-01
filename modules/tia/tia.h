// $Id: tia.h 1686 2013-02-07 22:11:42Z tk $
/*
 * Header file for TIA functions
 *
 * ==========================================================================
 *
 *  Copyright (C) 2013 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _TIA_H
#define _TIA_H

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// Maximum number of TIA chains (1 or 2)
// Each chain has a dedicated "load" line (e.g. J19:RC1 or J19:RC2)
// (Number of chains can be changed via soft-configuration during runtime.)
#ifndef TIA_NUM_CHAINS
#define TIA_NUM_CHAINS 1
#endif


// Maximum number of TIA chip
#ifndef TIA_NUM
#define TIA_NUM 1
#endif


// Which SPI peripheral should be used
// allowed values: 0 (J16), 1 (J8/9) and 2 (J19)
#ifndef TIA_SPI
#define TIA_SPI 2
#endif

// Which RC pin of the SPI port should be used for shift registers CS line
// allowed values: 0 or 1 for SPI0 (J16:RC1, J16:RC2), 0 for SPI1 (J8/9:RC), 0 or 1 for SPI2 (J19:RC1, J19:RC2)
#ifndef TIA_SPI_RC_PIN
#define TIA_SPI_RC_PIN 0
#endif

// Which RC pin of the SPI port should be used for TIA RW line
// allowed values: 0 or 1 for SPI0 (J16:RC1, J16:RC2), 0 for SPI1 (J8/9:RC), 0 or 1 for SPI2 (J19:RC1, J19:RC2)
#ifndef TIA_SPI_RW_PIN
#define TIA_SPI_RW_PIN 1
#endif
// more CS lines are possible, but not prepared yet (TIA_SetCs() has to be enhanced)

// should output pins be used in Open Drain mode? (perfect for 3.3V->5V levelshifting)
#ifndef TIA_SPI_OUTPUTS_OD
#if MIOS32_BOARD_MBHP_CORE_STM32
# define TIA_SPI_OUTPUTS_OD 1
#else
  // e.g. MBHP_CORE_LPC17 module
# define TIA_SPI_OUTPUTS_OD 0
#endif
#endif


// TIA registers
#define TIA_AUDC0       0x05
#define TIA_AUDC1       0x06
#define TIA_AUDF0       0x07
#define TIA_AUDF1       0x08
#define TIA_AUDV0       0x09
#define TIA_AUDV1       0x0A


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 TIA_Init(u32 mode);

extern s32 TIA_WriteReg(u8 device, u8 reg, u8 value);
extern s32 TIA_WriteAllRegs(u8 device, u8 reg, u8 value);



/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#ifdef __cplusplus
}
#endif

#endif /* _TIA_H */
