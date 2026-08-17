/*
 * USB layer: the adapter between this OS and TinyUSB.
 *
 * It owns three things and nothing else - which port plays which role, the
 * periodic call that drives the stack, and the handful of callbacks TinyUSB
 * expects from its host application. The classes live in their own files, and
 * everything below the controller lives in mios32/<FAMILY>/mios32_usb_ll.c.
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


/////////////////////////////////////////////////////////////////////////////
// Time base for the host stack
//
// The host stack times things - enumeration steps, deferred work - which the
// device stack never has to: a device only ever reacts to what the host asks.
// So TinyUSB wants a millisecond clock, and the OS already keeps one.
/////////////////////////////////////////////////////////////////////////////

#if CFG_TUH_ENABLED

# if !defined(MIOS32_USE_TIMESTAMP)
#  error "USB host needs a millisecond time base: add #define MIOS32_USE_TIMESTAMP to your mios32_config.h. It is already incremented from the 1 mS tick, so there is nothing else to wire."
# endif

uint32_t tusb_time_millis_api(void)
{
  return (uint32_t)MIOS32_TIMESTAMP_Get();
}

void tusb_time_delay_ms_api(uint32_t ms)
{
  while( ms-- )
    MIOS32_DELAY_Wait_uS(1000);
}

#endif


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static mios32_usb_role_t port_role[MIOS32_USB_NUM_PORTS];
static u8 initialized;

static void (*role_change_callback)(u8 port, mios32_usb_role_t role);


/////////////////////////////////////////////////////////////////////////////
// Role mapping
//
// MIOS32_USB_ROLE_DEVICE/HOST are 1 and 2 on purpose: TinyUSB's own
// TUSB_ROLE_DEVICE/HOST are 1 and 2 as well, so this is a cast and not a
// lookup table that could drift.
/////////////////////////////////////////////////////////////////////////////

static tusb_role_t role_to_tusb(mios32_usb_role_t role)
{
  switch( role ) {
  case MIOS32_USB_ROLE_DEVICE: return TUSB_ROLE_DEVICE;
  case MIOS32_USB_ROLE_HOST:   return TUSB_ROLE_HOST;
  default:                     return TUSB_ROLE_INVALID;
  }
}


/////////////////////////////////////////////////////////////////////////////
// Brings one port up in the requested role.
/////////////////////////////////////////////////////////////////////////////

static s32 port_start(u8 port, mios32_usb_role_t role)
{
  if( port >= MIOS32_USB_NUM_PORTS )
    return -1;

  if( role == MIOS32_USB_ROLE_NONE ) {
#if CFG_TUD_ENABLED
    if( port_role[port] == MIOS32_USB_ROLE_DEVICE )
      tud_deinit(port);
#endif
#if CFG_TUH_ENABLED
    if( port_role[port] == MIOS32_USB_ROLE_HOST )
      tuh_deinit(port);
#endif
    port_role[port] = MIOS32_USB_ROLE_NONE;
    return 0;
  }

  // Clocks, pins and interrupt first: the stack talks to a controller that
  // must already be alive when it does.
  if( MIOS32_USB_LL_Init(port, role) < 0 )
    return -2;

  tusb_rhport_init_t rh_init = {
    .role  = role_to_tusb(role),
    .speed = TUSB_SPEED_AUTO
  };

  if( !tusb_rhport_init(port, &rh_init) )
    return -3;

  port_role[port] = role;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Initializes the USB layer.
//! \param[in] mode currently only mode 0 is supported
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_Init(u32 mode)
{
  u8 port;

  if( mode != 0 )
    return -1;

  role_change_callback = NULL;

  for(port=0; port<MIOS32_USB_NUM_PORTS; ++port)
    port_role[port] = MIOS32_USB_ROLE_NONE;

  // A port whose role nothing can detect has to be told what it is, and the
  // project says so. A port that CAN detect its role is left idle until it
  // does - guessing would be worse than waiting.
  for(port=0; port<MIOS32_USB_NUM_PORTS; ++port) {
    if( MIOS32_USB_LL_RoleSourceGet(port) != MIOS32_USB_ROLE_SRC_FIXED )
      continue;

    mios32_usb_role_t role = (port == 0) ? MIOS32_USB_P0_ROLE : MIOS32_USB_P1_ROLE;

    // Asking for a role whose stack was never compiled in is a configuration
    // mistake, not a runtime condition - so it is worth reporting rather than
    // silently leaving the port dead.
#if !CFG_TUD_ENABLED
    if( role == MIOS32_USB_ROLE_DEVICE )
      return -3;
#endif
#if !CFG_TUH_ENABLED
    if( role == MIOS32_USB_ROLE_HOST )
      return -4;
#endif

    if( port_start(port, role) < 0 )
      return -2;
  }

#if defined(MIOS32_USE_USB_HOST_HID)
  // The class keeps per-keyboard state for its report diffing; give it a
  // clean start alongside the ports.
  MIOS32_USB_HID_Init(0);
#endif

  initialized = 1;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Drives the stack. Call regularly - the core calls it from the 1 mS tick.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
// TEMPORARY diagnostic - tells whether this is reached at all, and how often.
// Read over SWD; remove once the host/device interaction is understood.
u32 mios32_usb_dbg_handler;
u32 mios32_usb_dbg_tud;      // before tud_task()
u32 mios32_usb_dbg_tud_post; // after
u32 mios32_usb_dbg_tuh;      // before tuh_task()
u32 mios32_usb_dbg_tuh_post; // after

s32 MIOS32_USB_Handler(void)
{
  static u8 busy;
  u8 port;

  ++mios32_usb_dbg_handler;

  if( !initialized )
    return -1;

  // Refuse to be re-entered. TinyUSB is not re-entrant, and there is a real
  // path that tries: a blocking send waits for buffer space, pumps the stack
  // to make room, and that pump delivers received packets - back into the
  // parser whose message is still half-read. What comes out the other side is
  // then not a valid stream. Waiting a turn instead costs nothing: the caller
  // has its own timeout, and the tick will pump soon enough.
  if( busy )
    return -2;
  busy = 1;

  // Both tasks are global rather than per-port in TinyUSB, so each is called
  // at most once however many ports play that role.
#if CFG_TUD_ENABLED
  for(port=0; port<MIOS32_USB_NUM_PORTS; ++port) {
    if( port_role[port] == MIOS32_USB_ROLE_DEVICE ) {
      ++mios32_usb_dbg_tud; tud_task(); ++mios32_usb_dbg_tud_post;
      break;
    }
  }
#endif

#if CFG_TUH_ENABLED
  for(port=0; port<MIOS32_USB_NUM_PORTS; ++port) {
    if( port_role[port] == MIOS32_USB_ROLE_HOST ) {
      ++mios32_usb_dbg_tuh; tuh_task(); ++mios32_usb_dbg_tuh_post;
      break;
    }
  }
#endif

  busy = 0;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! \return 1 if the USB layer is up
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_IsInitialized(void)
{
  return initialized ? 1 : 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Puts a port into a role, or takes it down with MIOS32_USB_ROLE_NONE.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_RoleSet(u8 port, mios32_usb_role_t role)
{
  if( port >= MIOS32_USB_NUM_PORTS )
    return -1;

  if( port_role[port] == role )
    return 0; // already there

  // Down before up, always: the two stacks cannot share a controller, and a
  // half-torn-down one enumerates in ways that are painful to diagnose.
  if( port_role[port] != MIOS32_USB_ROLE_NONE )
    port_start(port, MIOS32_USB_ROLE_NONE);

  return port_start(port, role);
}


/////////////////////////////////////////////////////////////////////////////
//! \return the role a port is currently playing
/////////////////////////////////////////////////////////////////////////////
mios32_usb_role_t MIOS32_USB_RoleGet(u8 port)
{
  if( port >= MIOS32_USB_NUM_PORTS )
    return MIOS32_USB_ROLE_NONE;

  return port_role[port];
}


/////////////////////////////////////////////////////////////////////////////
//! Installs the hook called when a port changes role on its own.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_RoleChangeCallback_Init(void (*callback)(u8 port, mios32_usb_role_t role))
{
  role_change_callback = callback;
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// Called by the family layer when a port's role source says the role changed.
// Not public: an application observes this through the callback above.
/////////////////////////////////////////////////////////////////////////////
void MIOS32_USB_RoleChangeNotify(u8 port, mios32_usb_role_t role)
{
  if( port >= MIOS32_USB_NUM_PORTS )
    return;

  MIOS32_USB_RoleSet(port, role);

  if( role_change_callback != NULL )
    role_change_callback(port, role);
}

#endif /* MIOS32_USE_USB */
