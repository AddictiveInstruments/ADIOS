//! \defgroup ADIOS_IRQ
//!
//! System Specific IRQ Enable/Disable routines
//! 
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

// this module is indispensable (interrupt enable/disable, NVIC install/
// deinstall - used throughout the whole stack) - always compiled, no
// ADIOS_DONT_USE_IRQ/ADIOS_USE_IRQ toggle.

// the nesting counter ensures, that interrupts won't be enabled as long as
// nested functions disable them
static u32 nested_ctr;

// stored priority level before IRQ has been disabled (important for co-existence with vPortEnterCritical)
static u32 prev_primask;


/////////////////////////////////////////////////////////////////////////////
//! This function disables all interrupts (nested)
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_IRQ_Disable(void)
{
  // get current priority if nested level == 0
  if( !nested_ctr ) {
    __asm volatile (			   \
		    "	mrs %0, primask\n" \
		    : "=r" (prev_primask)  \
		    );
  }

  // disable interrupts
  __asm volatile ( \
		  "	mov r0, #1     \n" \
		  "	msr primask, r0\n" \
		  :::"r0"	 \
		  );

  ++nested_ctr;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! This function enables all interrupts (nested)
//! \return < 0 on errors
//! \return -1 on nesting errors (ADIOS_IRQ_Disable() hasn't been called before)
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_IRQ_Enable(void)
{
  // check for nesting error
  if( nested_ctr == 0 )
    return -1; // nesting error

  // decrease nesting level
  --nested_ctr;

  // set back previous priority once nested level reached 0 again
  if( nested_ctr == 0 ) {
    __asm volatile ( \
		    "	msr primask, %0\n" \
		    :: "r" (prev_primask)  \
		    );
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! This function installs an interrupt service.
//! \param[in] IRQn the interrupt number as defined in the CMSIS (e.g. CAN_IRQn)
//! \param[in] priority the priority from 0..15 - than lower the value, than higher the priority.\n
//! Please prefer the usage of ADIOS_IRQ_PRIO_LOW .. MID .. HIGH .. HIGHEST
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_IRQ_Install(u8 IRQn, u8 priority)
{
  // no check for IRQn as it's device dependent

  if( priority >= 16 )
    return -1; // invalid priority

  NVIC_SetPriority(IRQn, priority);
  NVIC_EnableIRQ(IRQn);

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! This function deinstalls an interrupt service.
//! \param[in] IRQn the interrupt number as defined in the CMSIS (e.g. CAN_IRQn)
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_IRQ_DeInstall(u8 IRQn)
{
  NVIC_DisableIRQ(IRQn);

  return 0; // no error
}

//! \}

