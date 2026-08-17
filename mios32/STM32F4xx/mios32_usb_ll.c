/*
 * USB low level for this family: clocks, pins, interrupt, role source.
 *
 * Everything above this file is family-independent - TinyUSB absorbs the
 * controller, and mios32_usb.c only decides roles. What is left here is what
 * genuinely differs from one silicon to the next.
 *
 * This family has two OTG cores, and TinyUSB numbers them the way the silicon
 * does: port 0 is OTG_FS, port 1 is OTG_HS. Both are served here. Note that
 * the high-speed core runs at FULL speed unless an external PHY is fitted -
 * which is why its pins carry the AF that the reference manual calls OTG_HS_FS.
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

#if defined(MIOS32_USE_USB)

#include <tusb.h>
#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_gpio.h>


/////////////////////////////////////////////////////////////////////////////
// Pins
//
// Each core brings its data pair out on one place only, so these are facts
// rather than choices - but they stay overridable because a package variant
// can move them.
/////////////////////////////////////////////////////////////////////////////

// Port 0 - OTG_FS, AF10
#ifndef MIOS32_USB_P0_DM_PORT
# define MIOS32_USB_P0_DM_PORT   GPIOA
#endif
#ifndef MIOS32_USB_P0_DM_PIN
# define MIOS32_USB_P0_DM_PIN    LL_GPIO_PIN_11
#endif
#ifndef MIOS32_USB_P0_DP_PORT
# define MIOS32_USB_P0_DP_PORT   GPIOA
#endif
#ifndef MIOS32_USB_P0_DP_PIN
# define MIOS32_USB_P0_DP_PIN    LL_GPIO_PIN_12
#endif
#ifndef MIOS32_USB_P0_AF
# define MIOS32_USB_P0_AF        LL_GPIO_AF_10
#endif

// Port 1 - OTG_HS in full-speed mode, AF12
#ifndef MIOS32_USB_P1_DM_PORT
# define MIOS32_USB_P1_DM_PORT   GPIOB
#endif
#ifndef MIOS32_USB_P1_DM_PIN
# define MIOS32_USB_P1_DM_PIN    LL_GPIO_PIN_14
#endif
#ifndef MIOS32_USB_P1_DP_PORT
# define MIOS32_USB_P1_DP_PORT   GPIOB
#endif
#ifndef MIOS32_USB_P1_DP_PIN
# define MIOS32_USB_P1_DP_PIN    LL_GPIO_PIN_15
#endif
#ifndef MIOS32_USB_P1_AF
# define MIOS32_USB_P1_AF        LL_GPIO_AF_12
#endif


/////////////////////////////////////////////////////////////////////////////
// How each port learns its role
//
// The OTG core CAN detect it: it has a real ID pin, reports it in
// GOTGCTL.CIDSTS and interrupts on GINTSTS.CIDSCHG. But that only means
// anything if the board fits a connector carrying ID - a micro-AB, or a
// Type-C controller whose ID output is wired to it.
//
// So the default is FIXED, because a board that gives each port its own
// dedicated connector has already decided the role in its mechanics: a type-A
// receptacle is a host socket, a type-B one is a device socket. A board that
// really wires ID says so:
//
//   #define MIOS32_USB_P0_ROLE_SOURCE  MIOS32_USB_ROLE_SRC_ID
/////////////////////////////////////////////////////////////////////////////

#ifndef MIOS32_USB_P0_ROLE_SOURCE
# define MIOS32_USB_P0_ROLE_SOURCE  MIOS32_USB_ROLE_SRC_FIXED
#endif
#ifndef MIOS32_USB_P1_ROLE_SOURCE
# define MIOS32_USB_P1_ROLE_SOURCE  MIOS32_USB_ROLE_SRC_FIXED
#endif


/////////////////////////////////////////////////////////////////////////////
//! Brings the controller up for one port in the requested role.
//! \param[in] port 0 for OTG_FS, 1 for OTG_HS
//! \param[in] role the role it is to play
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_LL_Init(u8 port, mios32_usb_role_t role)
{
  LL_GPIO_InitTypeDef gpio;

  if( port >= MIOS32_USB_NUM_PORTS )
    return -1;

  if( role != MIOS32_USB_ROLE_DEVICE && role != MIOS32_USB_ROLE_HOST )
    return -2;

  // The 48 MHz the controllers run on is set up with the rest of the clock
  // tree in mios32_sys.c - it is a system-wide decision, not a USB one.

  LL_GPIO_StructInit(&gpio);
  gpio.Mode       = LL_GPIO_MODE_ALTERNATE;
  gpio.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  gpio.Pull       = LL_GPIO_PULL_NO;

  if( port == 0 ) {
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);

    gpio.Alternate = MIOS32_USB_P0_AF;
    gpio.Pin = MIOS32_USB_P0_DM_PIN;
    LL_GPIO_Init(MIOS32_USB_P0_DM_PORT, &gpio);
    gpio.Pin = MIOS32_USB_P0_DP_PIN;
    LL_GPIO_Init(MIOS32_USB_P0_DP_PORT, &gpio);

    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_OTGFS);

    MIOS32_IRQ_Install(OTG_FS_IRQn, MIOS32_IRQ_USB_PRIORITY);

  } else {
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);

    gpio.Alternate = MIOS32_USB_P1_AF;
    gpio.Pin = MIOS32_USB_P1_DM_PIN;
    LL_GPIO_Init(MIOS32_USB_P1_DM_PORT, &gpio);
    gpio.Pin = MIOS32_USB_P1_DP_PIN;
    LL_GPIO_Init(MIOS32_USB_P1_DP_PORT, &gpio);

    // OTGHS only. NOT OTGHSULPI: that clock feeds the external high-speed PHY
    // interface, and leaving it on while using the internal full-speed PHY
    // holds the core in a state where nothing enumerates.
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_OTGHS);

    MIOS32_IRQ_Install(OTG_HS_IRQn, MIOS32_IRQ_USB_PRIORITY);
  }

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! \return how this port learns its role
/////////////////////////////////////////////////////////////////////////////
mios32_usb_role_source_t MIOS32_USB_LL_RoleSourceGet(u8 port)
{
  switch( port ) {
  case 0:  return MIOS32_USB_P0_ROLE_SOURCE;
  case 1:  return MIOS32_USB_P1_ROLE_SOURCE;
  default: return MIOS32_USB_ROLE_SRC_FIXED;
  }
}


/////////////////////////////////////////////////////////////////////////////
// Interrupts
//
// Declared weak in the startup file, so defining them here is what claims
// them. tusb_int_handler() dispatches to whichever stack owns the port, so
// there is nothing to decide at this level.
/////////////////////////////////////////////////////////////////////////////

void OTG_FS_IRQHandler(void)
{
  tusb_int_handler(0, true);
}

#if MIOS32_USB_NUM_PORTS > 1
void OTG_HS_IRQHandler(void)
{
  tusb_int_handler(1, true);
}
#endif

#endif /* MIOS32_USE_USB */
