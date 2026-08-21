/*
 * Header file for ADIOS
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _ADIOS_H
#define _ADIOS_H

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////////////////////////////////
// processor specific variable types
/////////////////////////////////////////////////////////////////////////////

#if defined(ADIOS_FAMILY_STM32F4xx)
//# ifndef __cplusplus
#include <stm32f4xx_conf.h>
#include <stm32f4xx.h>

//# else
  // STM32 drivers currently not enabled for C++ due to typedef conflicts (e.g. "bool")
#  include <adios_datatypes.h>
//# endif
#elif defined(ADIOS_FAMILY_STM32G0xx)
#include <stm32g0xx_conf.h>
#include <stm32g0xx.h>
#include <adios_datatypes.h>
#else
# include <adios_datatypes.h>
# warning "Unsupported ADIOS_FAMILY selected!"
#endif


/////////////////////////////////////////////////////////////////////////////
// include C headers
/////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>


/////////////////////////////////////////////////////////////////////////////
// include local config file
// (see ADIOS_CONFIG.txt for available switches)
/////////////////////////////////////////////////////////////////////////////

#include "adios_config.h"


/////////////////////////////////////////////////////////////////////////////
// include adios_*.h files of all MIOS modules
/////////////////////////////////////////////////////////////////////////////

#include <adios_irq.h>
#include <adios_sys.h>
#include <adios_spi.h>
#include <adios_srio.h>
#include <adios_srin.h>
#include <adios_srout.h>
#include <adios_enc.h>
#include <adios_adc.h>
#include <adios_dac.h>
#include <adios_midi.h>
#include <adios_usb.h>
#include <adios_usb_midi.h>
#include <adios_usb_hid.h>
#include <adios_usb_msc.h>
#include <adios_uart.h>
#include <adios_din_midi.h>
#include <adios_spi_midi.h>
#include <adios_i2c.h>
#include <adios_i2s.h>
#include <adios_timestamp.h>
#include <adios_utils.h>
#include <adios_sdcard.h>


/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////



#ifdef __cplusplus
}
#endif

#endif /* _ADIOS_H */
