//
// Includes the bootloader code into the project
// If no bootloader is provided for the board, reserve the BSL range instead
//
/* ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <adios.h>

// This switch is only for expert usage - it frees 16k, but can lead to
// big trouble if a user downloads the binary with the ADIOS BSL, and
// does not have a way to reflash it over SWD
// to recover the BSL
#ifndef ADIOS_DONT_INCLUDE_BSL
// ADIOS_BSL_INC_FILE is defined by the generated adios_bsl_boundary.h
// (see etc/gen_bsl_boundary.sh) - the project already knows which single,
// fixed chip it's built for, so it directly names the matching .inc file
// instead of re-deriving it here from a separate ADIOS_BOARD_xxx define.
# ifdef ADIOS_BSL_INC_FILE
#  include ADIOS_BSL_INC_FILE
# else
#  error "ADIOS_BSL_INC_FILE not defined - run etc/gen_bsl_boundary.sh for this project first (see any project Makefile for how it is invoked)"
# endif
#endif /* ADIOS_DONT_INCLUDE_BSL */
