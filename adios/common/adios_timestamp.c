//! \defgroup ADIOS_TIMESTAMP
//!
//! mS accurate Timestamp for ADIOS
//! \{
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

// this module can be optionally disabled in a local adios_config.h file (included from adios.h)
// To use it, declare ADIOS_USE_TIMESTAMP in your project's
// adios_config.h.
#if defined(ADIOS_USE_TIMESTAMP)


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static u32 timestamp;


/////////////////////////////////////////////////////////////////////////////
//! Resets the timestamp
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_TIMESTAMP_Init(u32 mode)
{
  // currently only mode 0 supported
  if( mode != 0 )
    return -1; // unsupported mode

  timestamp = 0;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Increments the timestamp, typically based on the FreeRTOS clock (1 mS)
//!
//! \note this function is called from vApplicationTickHook in main.c
//! Don't call it from your application.
//!
//! \return number of SRs
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_TIMESTAMP_Inc(void)
{
  ++timestamp;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Use this function to get the current timestamp
//!
//! \return the current timestamp
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_TIMESTAMP_Get(void)
{
  return timestamp;
}


/////////////////////////////////////////////////////////////////////////////
//! Use this function to get the delay which has passed between a given and
//! and current timestamp.
//!
//! Usage Example:
//! \code
//!   u32 captured_timestamp = ADIOS_TIMESTAMP_GetDelay();
//!   // ...
//!   // ... do something ...
//!   // ...
//!   u32 delay_in_ms = ADIOS_TIMESTAMP_GetDelay(captured_timestamp);
//!   ADIOS_MIDI_SendDebugMessage("Delay: %d mS\n", delay_in_ms);
//! \endcode
//! \return the delay between the given and the current timestamp
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_TIMESTAMP_GetDelay(u32 captured_timestamp)
{
  // will automatically roll over:
  // e.g. 0x00000010 - 0xfffffff0 = 0x00000020
  return timestamp - captured_timestamp;
}

//! \}

#endif /* ADIOS_USE_TIMESTAMP */

