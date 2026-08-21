/*
 * USB low level for this family: clocks, pins, interrupt, role source.
 *
 * Everything above this file is family-independent - TinyUSB absorbs the
 * controller, and adios_usb.c only decides roles. What is left here is what
 * genuinely differs from one silicon to the next.
 *
 * This family has two OTG cores, and TinyUSB numbers them the way the silicon
 * does: port 0 is OTG_FS, port 1 is OTG_HS. Both are served here. Note that
 * the high-speed core runs at FULL speed unless an external PHY is fitted -
 * which is why its pins carry the AF that the reference manual calls OTG_HS_FS.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#include <adios.h>

#if defined(ADIOS_USE_USB)

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
#ifndef ADIOS_USB_P0_DM_PORT
# define ADIOS_USB_P0_DM_PORT   GPIOA
#endif
#ifndef ADIOS_USB_P0_DM_PIN
# define ADIOS_USB_P0_DM_PIN    LL_GPIO_PIN_11
#endif
#ifndef ADIOS_USB_P0_DP_PORT
# define ADIOS_USB_P0_DP_PORT   GPIOA
#endif
#ifndef ADIOS_USB_P0_DP_PIN
# define ADIOS_USB_P0_DP_PIN    LL_GPIO_PIN_12
#endif
#ifndef ADIOS_USB_P0_AF
# define ADIOS_USB_P0_AF        LL_GPIO_AF_10
#endif

// Port 1 - OTG_HS in full-speed mode, AF12
#ifndef ADIOS_USB_P1_DM_PORT
# define ADIOS_USB_P1_DM_PORT   GPIOB
#endif
#ifndef ADIOS_USB_P1_DM_PIN
# define ADIOS_USB_P1_DM_PIN    LL_GPIO_PIN_14
#endif
#ifndef ADIOS_USB_P1_DP_PORT
# define ADIOS_USB_P1_DP_PORT   GPIOB
#endif
#ifndef ADIOS_USB_P1_DP_PIN
# define ADIOS_USB_P1_DP_PIN    LL_GPIO_PIN_15
#endif
#ifndef ADIOS_USB_P1_AF
# define ADIOS_USB_P1_AF        LL_GPIO_AF_12
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
//   #define ADIOS_USB_P0_ROLE_SOURCE  ADIOS_USB_ROLE_SRC_ID
/////////////////////////////////////////////////////////////////////////////

// The defaults themselves live in adios_usb.h, where every layer can read
// them: whether a board detects its role decides how much of the COMMON
// machinery is compiled at all, not merely what happens in this file.

// The ID contact itself is read as an ORDINARY INPUT, not through the
// controller's own ID logic, and that is deliberate:
//
//  - the stack FORCES the mode it wants (GUSBCFG.FDMOD / FHMOD) instead of
//    letting the pin decide, so wiring the contact to the controller would
//    change nothing it does;
//  - the controller's ID register can only be read once the controller is
//    clocked, and its clock is only switched on once a role is known - which
//    is the very thing being asked. A plain input has no such circle;
//  - and the controller's interrupt vector belongs to the stack. Reading a
//    pin of our own keeps this out of it entirely.
//
// A micro-AB receptacle grounds the contact in an A plug and leaves it open
// in a B one, so the input is pulled up and LOW means HOST.
#ifndef ADIOS_USB_P0_ID_PORT
# define ADIOS_USB_P0_ID_PORT      GPIOA
#endif
#ifndef ADIOS_USB_P0_ID_PIN
# define ADIOS_USB_P0_ID_PIN       LL_GPIO_PIN_10
#endif
#ifndef ADIOS_USB_P1_ID_PORT
# define ADIOS_USB_P1_ID_PORT      GPIOB
#endif
#ifndef ADIOS_USB_P1_ID_PIN
# define ADIOS_USB_P1_ID_PIN       LL_GPIO_PIN_12
#endif


/////////////////////////////////////////////////////////////////////////////
// VBUS
//
// A host must SUPPLY the 5 V; a device only consumes it. The controller
// cannot do that itself - it drives a power switch through an ordinary GPIO,
// and a board without such a switch can never act as a host whatever the
// software does.
//
// Without this, a plugged device is never powered, never pulls D+ up, and the
// port reports nothing at all: HPRT.PCSTS stays at 0. A device with its own
// supply changes nothing - most still wait to see the host's VBUS before
// presenting themselves.
//
// CAUTION on this family: PC14/PC15 are OSC32_IN/OSC32_OUT, the 32 kHz
// crystal pins, and live in the backup domain. They are usable as plain GPIO
// only while the LSE is off, and they drive less current than an ordinary
// pin - enough for a switch's enable input, not for anything else.
//
// Polarity is a property of the SWITCH, not of the chip, so it is declared.
// The default is active LOW, which is what ST's STMPS216x family wants on its
// enable input - measured, not assumed. A board using a switch that enables on
// a high says so:
//   #define ADIOS_USB_P1_VBUS_ACTIVE_HIGH 1
//
// Those switches also bring out a FLAG output for over-current, open drain and
// asserted LOW, so the pin reading it needs a pull-up.
/////////////////////////////////////////////////////////////////////////////

// DECLARING THE PIN IS HOW A BOARD SAYS IT HAS A SWITCH AT ALL. Port 1 keeps
// a default because that is the socket a two-socket board hosts on; port 0
// has none, so the many boards whose only socket takes its 5 V from the cable
// never touch a pin they do not have.
#ifndef ADIOS_USB_P1_VBUS_PORT
# define ADIOS_USB_P1_VBUS_PORT   GPIOC
#endif
#ifndef ADIOS_USB_P1_VBUS_PIN
# define ADIOS_USB_P1_VBUS_PIN    LL_GPIO_PIN_14
#endif
#ifndef ADIOS_USB_P1_VBUS_ACTIVE_HIGH
# define ADIOS_USB_P1_VBUS_ACTIVE_HIGH 0
#endif
#ifndef ADIOS_USB_P0_VBUS_ACTIVE_HIGH
# define ADIOS_USB_P0_VBUS_ACTIVE_HIGH 0
#endif

// The FLAG output is open drain and asserted LOW, so the pin reading it is
// pulled up. Informational only: the switch limits current and shuts itself
// down whatever software does - this just lets software SAY that it did.
#ifndef ADIOS_USB_P0_OC_ACTIVE_HIGH
# define ADIOS_USB_P0_OC_ACTIVE_HIGH 0
#endif
#ifndef ADIOS_USB_P1_OC_ACTIVE_HIGH
# define ADIOS_USB_P1_OC_ACTIVE_HIGH 0
#endif

// Each helper below is compiled only for a build that DECLARED the pins it
// serves: a bootloader, or a board whose single socket takes its 5 V from the
// cable, carries none of them.
#if defined(ADIOS_USB_P0_VBUS_PIN) \
 || (ADIOS_USB_NUM_PORTS > 1 && defined(ADIOS_USB_P1_VBUS_PIN))
# define USB_VBUS_SWITCHED 1
#else
# define USB_VBUS_SWITCHED 0
#endif

#if defined(ADIOS_USB_P0_OC_PIN) \
 || ADIOS_USB_P0_ROLE_SOURCE != ADIOS_USB_ROLE_SRC_FIXED \
 || (ADIOS_USB_NUM_PORTS > 1 && (defined(ADIOS_USB_P1_OC_PIN) \
     || ADIOS_USB_P1_ROLE_SOURCE != ADIOS_USB_ROLE_SRC_FIXED))
# define USB_PIN_SENSED 1
#else
# define USB_PIN_SENSED 0
#endif

#if USB_VBUS_SWITCHED || USB_PIN_SENSED
// Which clock a pin needs follows from which GPIO port it sits on, and these
// pins are declared rather than fixed - so it is derived instead of listed,
// and a board that moves one is not also obliged to remember its clock.
static void gpio_clock_enable(GPIO_TypeDef *gpio_port)
{
  // DERIVED, not listed. The GPIO ports sit at a regular 0x400 stride from the
  // start of their bus, and their enable bits run in the same order from bit 0
  // - so the port's own address gives its bit, and a board that moves a pin to
  // a port nobody thought of still gets its clock.
  LL_AHB1_GRP1_EnableClock(1u << (((u32)gpio_port - AHB1PERIPH_BASE) / 0x400u));
}

#endif /* USB_VBUS_SWITCHED || USB_PIN_SENSED */

#if USB_VBUS_SWITCHED
// "on" and "active_high" are both 0 or 1, so polarity is arithmetic rather
// than a second code path: the pin goes high exactly when the two agree.
static void vbus_drive(GPIO_TypeDef *gpio_port, u32 pin, u8 active_high, u8 on)
{
  LL_GPIO_InitTypeDef gpio;

  gpio_clock_enable(gpio_port);

  LL_GPIO_StructInit(&gpio);
  gpio.Pin        = pin;
  gpio.Mode       = LL_GPIO_MODE_OUTPUT;
  gpio.Speed      = LL_GPIO_SPEED_FREQ_LOW;
  gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  gpio.Pull       = LL_GPIO_PULL_NO;
  LL_GPIO_Init(gpio_port, &gpio);

  if( on == active_high )
    LL_GPIO_SetOutputPin(gpio_port, pin);
  else
    LL_GPIO_ResetOutputPin(gpio_port, pin);
}
#endif /* USB_VBUS_SWITCHED */

#if USB_PIN_SENSED
// Claims a pin that REPORTS rather than commands - the over-current flag, the
// ID contact. Pulled to the level that means "nothing to report", so an
// unconnected pin reads as the quiet case instead of floating into either.
static void sense_claim(GPIO_TypeDef *gpio_port, u32 pin, u8 active_high)
{
  LL_GPIO_InitTypeDef gpio;

  gpio_clock_enable(gpio_port);

  LL_GPIO_StructInit(&gpio);
  gpio.Pin  = pin;
  gpio.Mode = LL_GPIO_MODE_INPUT;
  gpio.Pull = active_high ? LL_GPIO_PULL_DOWN : LL_GPIO_PULL_UP;
  LL_GPIO_Init(gpio_port, &gpio);
}
#endif /* USB_PIN_SENSED */


/////////////////////////////////////////////////////////////////////////////
//! Brings the controller up for one port in the requested role.
//! \param[in] port 0 for OTG_FS, 1 for OTG_HS
//! \param[in] role the role it is to play
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_USB_LL_Init(u8 port, adios_usb_role_t role)
{
  LL_GPIO_InitTypeDef gpio;

  if( port >= ADIOS_USB_NUM_PORTS )
    return -1;

  if( role != ADIOS_USB_ROLE_DEVICE && role != ADIOS_USB_ROLE_HOST )
    return -2;

  // The 48 MHz the controllers run on is set up with the rest of the clock
  // tree in adios_sys.c - it is a system-wide decision, not a USB one.

  LL_GPIO_StructInit(&gpio);
  gpio.Mode       = LL_GPIO_MODE_ALTERNATE;
  gpio.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  gpio.Pull       = LL_GPIO_PULL_NO;

  if( port == 0 ) {
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);

    gpio.Alternate = ADIOS_USB_P0_AF;
    gpio.Pin = ADIOS_USB_P0_DM_PIN;
    LL_GPIO_Init(ADIOS_USB_P0_DM_PORT, &gpio);
    gpio.Pin = ADIOS_USB_P0_DP_PIN;
    LL_GPIO_Init(ADIOS_USB_P0_DP_PORT, &gpio);

    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_OTGFS);

#ifdef ADIOS_USB_P0_VBUS_PIN
    // Power the socket before the stack looks at it - and just as important,
    // TAKE THE POWER AWAY when this port comes up as a device, which is how a
    // single socket that has just stopped hosting stops feeding a bus it is
    // about to be a guest on.
    vbus_drive(ADIOS_USB_P0_VBUS_PORT, ADIOS_USB_P0_VBUS_PIN,
               ADIOS_USB_P0_VBUS_ACTIVE_HIGH, role == ADIOS_USB_ROLE_HOST);
#endif
#ifdef ADIOS_USB_P0_OC_PIN
    sense_claim(ADIOS_USB_P0_OC_PORT, ADIOS_USB_P0_OC_PIN,
                ADIOS_USB_P0_OC_ACTIVE_HIGH);
#endif

    ADIOS_IRQ_Install(OTG_FS_IRQn, ADIOS_IRQ_USB_PRIORITY);

  } else {
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);

    gpio.Alternate = ADIOS_USB_P1_AF;
    gpio.Pin = ADIOS_USB_P1_DM_PIN;
    LL_GPIO_Init(ADIOS_USB_P1_DM_PORT, &gpio);
    gpio.Pin = ADIOS_USB_P1_DP_PIN;
    LL_GPIO_Init(ADIOS_USB_P1_DP_PORT, &gpio);

    // OTGHS only. NOT OTGHSULPI: that clock feeds the external high-speed PHY
    // interface, and leaving it on while using the internal full-speed PHY
    // holds the core in a state where nothing enumerates.
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_OTGHS);

    // Power the socket before the stack looks at it. A host with no VBUS sees
    // an empty port forever; a device must not be handed 5 V at all.
#if ADIOS_USB_NUM_PORTS > 1 && defined(ADIOS_USB_P1_VBUS_PIN)
    vbus_drive(ADIOS_USB_P1_VBUS_PORT, ADIOS_USB_P1_VBUS_PIN,
               ADIOS_USB_P1_VBUS_ACTIVE_HIGH, role == ADIOS_USB_ROLE_HOST);
#endif
#if ADIOS_USB_NUM_PORTS > 1 && defined(ADIOS_USB_P1_OC_PIN)
    sense_claim(ADIOS_USB_P1_OC_PORT, ADIOS_USB_P1_OC_PIN,
                ADIOS_USB_P1_OC_ACTIVE_HIGH);
#endif

    ADIOS_IRQ_Install(OTG_HS_IRQn, ADIOS_IRQ_USB_PRIORITY);
  }

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Does this port hold a LIVE device session from a previous life?
//!
//! A core-only reset restarts the processor and leaves this controller
//! running: still attached, still carrying the address the host assigned.
//! That is the state the adoption path takes over (see adios_usb.c), and
//! this is how it is recognised - from the silicon, never from RAM, which a
//! reset wipes.
//! \return 1 if the device controller is clocked, attached and addressed
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_USB_LL_DeviceIsWarm(u8 port)
{
  if( port != 0 )
    return 0; // this reads the FS controller's registers and no others. Note
              // it does NOT say "port 0 is the device port": port 0 can host
              // too. The caller only asks when the role it wants IS device.

  if( !(RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN) )
    return 0; // controller not even clocked: cold boot

  USB_OTG_DeviceTypeDef *dev = (USB_OTG_DeviceTypeDef *)(USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE);

  if( dev->DCTL & USB_OTG_DCTL_SDIS )
    return 0; // soft-disconnected: no session to adopt

  if( ((dev->DCFG & USB_OTG_DCFG_DAD) >> USB_OTG_DCFG_DAD_Pos) == 0 )
    return 0; // never addressed: enumeration had not happened

  return 1;
}


/////////////////////////////////////////////////////////////////////////////
//! Silences the port's interrupt at BOTH levels - the controller's global
//! enable and the NVIC line - so a live session can be adopted without a
//! single interrupt landing on software that does not exist yet. The adoption
//! re-enables both when its state is ready.
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_USB_LL_IrqSilence(u8 port)
{
  if( port != 0 )
    return -1;

  USB_OTG_GlobalTypeDef *otg = (USB_OTG_GlobalTypeDef *)USB_OTG_FS_PERIPH_BASE;
  otg->GAHBCFG &= ~USB_OTG_GAHBCFG_GINT;
  NVIC_DisableIRQ(OTG_FS_IRQn);

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! \return how this port learns its role
/////////////////////////////////////////////////////////////////////////////
adios_usb_role_source_t ADIOS_USB_LL_RoleSourceGet(u8 port)
{
  switch( port ) {
  case 0:  return ADIOS_USB_P0_ROLE_SOURCE;
  case 1:  return ADIOS_USB_P1_ROLE_SOURCE;
  default: return ADIOS_USB_ROLE_SRC_FIXED;
  }
}


#if ADIOS_USB_P0_ROLE_SOURCE == ADIOS_USB_ROLE_SRC_ID || ADIOS_USB_P1_ROLE_SOURCE == ADIOS_USB_ROLE_SRC_ID
static u8 id_claimed;

static adios_usb_role_t id_read(u8 port, GPIO_TypeDef *gpio_port, u32 pin)
{
  if( !(id_claimed & (1 << port)) ) {
    sense_claim(gpio_port, pin, 0); // open = B plug, so the quiet level is high
    id_claimed |= (1 << port);

    // Let the pull-up charge the line before the FIRST reading is taken. That
    // reading is not just any reading: at start-up it decides the role, and
    // therefore whether a live session left by the previous stage is adopted
    // or thrown away.
    { volatile u32 d; for(d=0; d<1000; ++d); }
  }

  return LL_GPIO_IsInputPinSet(gpio_port, pin) ? ADIOS_USB_ROLE_DEVICE
                                               : ADIOS_USB_ROLE_HOST;
}
#endif


/////////////////////////////////////////////////////////////////////////////
//! Reads the role a port's source is asking for, as it stands right now.
//!
//! Raw: no debouncing, no memory of what it said last time. What to do about
//! a reading that differs from the role in force is the common layer's
//! business, not this one's.
//! \param[in] port 0 for OTG_FS, 1 for OTG_HS
//! \return the role read, or ADIOS_USB_ROLE_NONE when this port detects
//!         none - which is not a failure, it is a fixed port saying "the
//!         project decides, not me"
/////////////////////////////////////////////////////////////////////////////
adios_usb_role_t ADIOS_USB_LL_RoleDetect(u8 port)
{
  switch( port ) {
#if ADIOS_USB_P0_ROLE_SOURCE == ADIOS_USB_ROLE_SRC_ID
  case 0: return id_read(0, ADIOS_USB_P0_ID_PORT, ADIOS_USB_P0_ID_PIN);
#endif
#if ADIOS_USB_P1_ROLE_SOURCE == ADIOS_USB_ROLE_SRC_ID
  case 1: return id_read(1, ADIOS_USB_P1_ID_PORT, ADIOS_USB_P1_ID_PIN);
#endif
  default: break;
  }

  return ADIOS_USB_ROLE_NONE;
}


/////////////////////////////////////////////////////////////////////////////
//! Is the port's power switch reporting over-current?
//!
//! The switch protects the board on its own; this only makes the fact
//! readable, so an application can say why a device went quiet instead of
//! leaving the user to guess.
//! \return 1 asserted, 0 clear, -1 if this port has no flag wired
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_USB_LL_OverCurrent(u8 port)
{
  switch( port ) {
#ifdef ADIOS_USB_P0_OC_PIN
  case 0: return LL_GPIO_IsInputPinSet(ADIOS_USB_P0_OC_PORT, ADIOS_USB_P0_OC_PIN)
                 == ADIOS_USB_P0_OC_ACTIVE_HIGH;
#endif
#ifdef ADIOS_USB_P1_OC_PIN
  case 1: return LL_GPIO_IsInputPinSet(ADIOS_USB_P1_OC_PORT, ADIOS_USB_P1_OC_PIN)
                 == ADIOS_USB_P1_OC_ACTIVE_HIGH;
#endif
  default: break;
  }

  return -1;
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

#if ADIOS_USB_NUM_PORTS > 1
void OTG_HS_IRQHandler(void)
{
  tusb_int_handler(1, true);
}
#endif

#endif /* ADIOS_USE_USB */
