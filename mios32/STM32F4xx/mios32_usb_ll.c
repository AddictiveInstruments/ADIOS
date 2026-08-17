/*
 * USB low level for this family: clocks, pins, interrupt, role source.
 *
 * Everything above this file is family-independent - TinyUSB absorbs the
 * controller, and mios32_usb.c only decides roles. What is left here is what
 * genuinely differs from one silicon to the next.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

#include <mios32.h>

#if defined(MIOS32_USE_USB_MIDI)

#include <tusb.h>
#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_gpio.h>


/////////////////////////////////////////////////////////////////////////////
// Pins
//
// The full-speed core brings its data pair out on one place only, so these
// are facts rather than choices - but they stay overridable because a package
// variant can move them.
/////////////////////////////////////////////////////////////////////////////

#ifndef MIOS32_USB_DM_PORT
# define MIOS32_USB_DM_PORT   GPIOA
#endif
#ifndef MIOS32_USB_DM_PIN
# define MIOS32_USB_DM_PIN    LL_GPIO_PIN_11
#endif
#ifndef MIOS32_USB_DP_PORT
# define MIOS32_USB_DP_PORT   GPIOA
#endif
#ifndef MIOS32_USB_DP_PIN
# define MIOS32_USB_DP_PIN    LL_GPIO_PIN_12
#endif

// AF10 is the OTG_FS alternate function on this family.
#define MIOS32_USB_PIN_AF     LL_GPIO_AF_10


/////////////////////////////////////////////////////////////////////////////
// How this port learns its role
//
// The OTG core CAN detect it: it has a real ID pin, reports it in
// GOTGCTL.CIDSTS and interrupts on GINTSTS.CIDSCHG. But that only means
// anything if the board fits a connector carrying ID - a micro-AB, or a
// Type-C controller whose ID output is wired to it.
//
// So the default is FIXED, because most boards give each port a dedicated
// connector and the role is then decided by the mechanics. A board that does
// wire ID says so:
//
//   #define MIOS32_USB_ROLE_SOURCE  MIOS32_USB_ROLE_SRC_ID
/////////////////////////////////////////////////////////////////////////////

#ifndef MIOS32_USB_ROLE_SOURCE
# define MIOS32_USB_ROLE_SOURCE  MIOS32_USB_ROLE_SRC_FIXED
#endif


/////////////////////////////////////////////////////////////////////////////
//! Brings the controller up for one port in the requested role.
//! \param[in] port the USB port
//! \param[in] role the role it is to play
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_LL_Init(u8 port, mios32_usb_role_t role)
{
  LL_GPIO_InitTypeDef gpio;

  if( port != 0 )
    return -1; // only the full-speed core is served today

  if( role != MIOS32_USB_ROLE_DEVICE && role != MIOS32_USB_ROLE_HOST )
    return -2;

  // The 48 MHz the controller runs on is set up with the rest of the clock
  // tree in mios32_sys.c - it is a system-wide decision, not a USB one.

  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);

  LL_GPIO_StructInit(&gpio);
  gpio.Mode       = LL_GPIO_MODE_ALTERNATE;
  gpio.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  gpio.Pull       = LL_GPIO_PULL_NO;
  gpio.Alternate  = MIOS32_USB_PIN_AF;

  gpio.Pin = MIOS32_USB_DM_PIN;
  LL_GPIO_Init(MIOS32_USB_DM_PORT, &gpio);

  gpio.Pin = MIOS32_USB_DP_PIN;
  LL_GPIO_Init(MIOS32_USB_DP_PORT, &gpio);

  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_OTGFS);

  MIOS32_IRQ_Install(OTG_FS_IRQn, MIOS32_IRQ_USB_PRIORITY);

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! \return how this port learns its role
/////////////////////////////////////////////////////////////////////////////
mios32_usb_role_source_t MIOS32_USB_LL_RoleSourceGet(u8 port)
{
  if( port != 0 )
    return MIOS32_USB_ROLE_SRC_FIXED;

  return MIOS32_USB_ROLE_SOURCE;
}


/////////////////////////////////////////////////////////////////////////////
// Interrupt
//
// Declared weak in the startup file, so defining it here is what claims it.
/////////////////////////////////////////////////////////////////////////////

void OTG_FS_IRQHandler(void)
{
  tud_int_handler(0);
}

#endif /* MIOS32_USE_USB_MIDI */
