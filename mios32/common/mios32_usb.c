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
#include <stdarg.h>

#if defined(MIOS32_USE_USB)

#include <tusb.h>

#if CFG_TUD_ENABLED
// LOCAL PATCH to TinyUSB (usbd.c): bring the device stack up around a session
// that is already live - see port_start() below for when that happens.
extern bool tud_rhport_adopt(uint8_t rhport, uint8_t cfg_num);
#endif


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

#if MIOS32_USB_ROLE_DETECTED
// Debouncing state for ports that DETECT their role. A reading has to repeat
// itself before it is believed: a plug scrapes its way in, and a role change
// is not a cheap event - it tears one stack down and builds the other up.
static mios32_usb_role_t role_seen[MIOS32_USB_NUM_PORTS];
static u8 role_seen_count[MIOS32_USB_NUM_PORTS];

// Defined further down, where the family layer's entry point lives. Declared
// here because the pump below calls it, and the pump comes first.
void MIOS32_USB_RoleChangeNotify(u8 port, mios32_usb_role_t role);
#endif

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

#if CFG_TUD_ENABLED
  // A LIVE session first: a core-only reset - an application rebooting into
  // its bootloader, or the bootloader handing over to a fresh application -
  // leaves this controller attached, addressed and configured, with only the
  // software gone. Adopting it means the host never sees a disconnect: its
  // handles stay valid, its conversation resumes where it left off. This is
  // what the old stack did with its handful of variables, and it is why an
  // upload never used to cost the port. Detect from the silicon, silence the
  // interrupt while the software half is rebuilt, adopt.
  if( role == MIOS32_USB_ROLE_DEVICE && MIOS32_USB_LL_DeviceIsWarm(port) ) {
    MIOS32_USB_LL_IrqSilence(port);

    if( MIOS32_USB_LL_Init(port, role) >= 0 && tud_rhport_adopt(port, 1) ) {
      // Close whatever SysEx the host's parser may hold half-open across the
      // seam. If a single event was lost at the transition - one F0 that
      // never met its F7 - the host-side reassembly waits forever and
      // silently swallows EVERYTHING that follows, while the device sees its
      // transfers drain perfectly: deafness with all counters green. A lone
      // terminator is ignored when nothing is open, and unblocks the parser
      // when something is. Cable 0, CIN 5: single-byte SysEx end.
      {
        uint8_t const sysex_flush[4] = { 0x05, 0xF7, 0x00, 0x00 };
        tud_midi_packet_write(sysex_flush);
      }

      port_role[port] = role;
      return 0;
    }

    // Adoption failed: fall through to a cold start. The worst it costs is
    // the disconnect the adoption exists to avoid.
  }
#endif

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

#if CFG_TUD_ENABLED
  // Present ourselves as a NEW attachment, by letting go of the bus for long
  // enough that the host cannot miss it.
  //
  // Coming up is not always coming up from nothing: a core that resets into
  // its bootloader, or an application that restarts, changes what it IS while
  // the cable stays in. The reset drops the pull-up for barely a moment -
  // too briefly for a host to call it an unplug - so the host carries on
  // addressing what it enumerated before, while this side has forgotten
  // everything and answers to nobody. Measured in exactly that state: pull-up
  // asserted, device address still zero, and no interrupt pending on either
  // side. Nothing breaks the deadlock, because neither end thinks anything
  // happened.
  //
  // Detaching deliberately makes the change visible: the host sees a device
  // leave and another arrive, and enumerates it from scratch.
  if( role == MIOS32_USB_ROLE_DEVICE ) {
    u32 i;
    tud_disconnect();
    for(i=0; i<MIOS32_USB_DETACH_MS; ++i)
      MIOS32_DELAY_Wait_uS(1000);
    tud_connect();
  }
#endif

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
  // project says so. A port that CAN detect it is ASKED, now, rather than
  // left waiting for a change to be signalled: the commonest situation of all
  // is a cable already in place at start-up, which produces no change at all.
  //
  // And on this family it decides more than the role. Coming up is often a
  // HAND-OVER, with a live device session left running by the stage before
  // (see port_start below): read the source too late and the port starts from
  // nothing, which costs exactly the session the adoption exists to keep.
  for(port=0; port<MIOS32_USB_NUM_PORTS; ++port) {
    mios32_usb_role_t role;

#if MIOS32_USB_ROLE_DETECTED
    role_seen[port] = MIOS32_USB_ROLE_NONE;
    role_seen_count[port] = 0;

    if( MIOS32_USB_LL_RoleSourceGet(port) != MIOS32_USB_ROLE_SRC_FIXED ) {
      role = MIOS32_USB_LL_RoleDetect(port);

      // A source with nothing to say yet - no plug, no decision. Leaving the
      // port idle is right: the pump re-reads it every millisecond.
      if( role == MIOS32_USB_ROLE_NONE )
        continue;
    } else
#endif
    role = (port == 0) ? MIOS32_USB_P0_ROLE : MIOS32_USB_P1_ROLE;

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

// TEMPORARY: RAM ring buffer for TinyUSB's own debug narration (CFG_TUSB_DEBUG,
// see tusb_config.h). Read over SWD: the write head first, then the text - the
// newest bytes are just before the head, the oldest just after it.
#if CFG_TUSB_DEBUG
#define DBG_LOG_SIZE 4096
char mios32_usb_dbg_log[DBG_LOG_SIZE];
u32  mios32_usb_dbg_log_head;

int mios32_usb_dbg_printf(const char *format, ...)
{
  // Static, and roomy, for two reasons learned the hard way. This runs from
  // inside USB callbacks, where stack is the scarcest thing there is - a
  // buffer this size on the stack is a stack overflow waiting to happen. And
  // there is no bounded vsprintf here, so the only defence against a long
  // line is to leave more room than any line needs. Bench instrument only:
  // not re-entrant, which is why it stays off unless a question needs it.
  static char line[512];
  va_list args;
  int n, i;

  va_start(args, format);
  n = vsprintf(line, format, args);
  va_end(args);

  for(i=0; i<n; ++i) {
    mios32_usb_dbg_log[mios32_usb_dbg_log_head % DBG_LOG_SIZE] = line[i];
    ++mios32_usb_dbg_log_head;
  }

  return n;
}
#endif

s32 MIOS32_USB_Handler(void)
{
  static u8 busy;
  u8 port;


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

#if MIOS32_USB_ROLE_DETECTED
  // A port that detects its role is RE-READ here rather than waited on as an
  // interrupt. Plugging a cable is a human gesture, so a reading every time
  // round the pump is a thousand times quicker than it needs to be - and it
  // brings the debouncing an edge would not have brought at all. Contacts
  // scrape on the way in, and acting on the scrape would tear a stack down
  // and build it back up several times over.
  for(port=0; port<MIOS32_USB_NUM_PORTS; ++port) {
    mios32_usb_role_t seen;

    if( MIOS32_USB_LL_RoleSourceGet(port) == MIOS32_USB_ROLE_SRC_FIXED )
      continue;

    seen = MIOS32_USB_LL_RoleDetect(port);

    // Nothing to say, or nothing new to say: forget any part-formed reading.
    if( seen == MIOS32_USB_ROLE_NONE || seen == port_role[port] ) {
      role_seen_count[port] = 0;
      continue;
    }

    if( seen != role_seen[port] ) {
      role_seen[port] = seen;
      role_seen_count[port] = 0;
    } else if( ++role_seen_count[port] >= MIOS32_USB_ROLE_SETTLE ) {
      role_seen_count[port] = 0;
      MIOS32_USB_RoleChangeNotify(port, seen);
    }
  }
#endif /* MIOS32_USB_ROLE_DETECTED */

  // Both tasks are global rather than per-port in TinyUSB, so each is called
  // at most once however many ports play that role.
#if CFG_TUD_ENABLED
  for(port=0; port<MIOS32_USB_NUM_PORTS; ++port) {
    if( port_role[port] == MIOS32_USB_ROLE_DEVICE ) {
      tud_task();
      break;
    }
  }
#endif

#if CFG_TUH_ENABLED
  for(port=0; port<MIOS32_USB_NUM_PORTS; ++port) {
    if( port_role[port] == MIOS32_USB_ROLE_HOST ) {
      tuh_task();
      break;
    }
  }
#endif

  // The stack's own tasks are done, so the guard above can be lifted here
  // rather than at the end. What follows is the classes' own periodic work,
  // and it reaches application code: a class tells an application that a
  // medium is ready, and the application reads a sector of it there and then.
  // That read is blocking, and it waits by driving this very function - which
  // it must be allowed to do. Holding the guard until the end would refuse it,
  // and the wait would run to its timeout with nothing ever pumped.
  busy = 0;

#if CFG_TUH_ENABLED
  {
    // The periodic work itself is not re-entrant, so a nested call pumps the
    // stack - which is what it came for - and skips this part.
    static u8 periodic_busy;

    if( !periodic_busy ) {
      periodic_busy = 1;

# if defined(MIOS32_USE_USB_HOST_HID)
      // After the stack has run, so a request the class could not place while
      // the bus was busy is retried now that it may be free.
      MIOS32_USB_HID_Periodic_mS();
# endif

# if defined(MIOS32_USE_USB_HOST_MSC)
      // Looks for a card appearing in, or leaving, a reader's slots.
      MIOS32_USB_MSC_Periodic_mS();
# endif

      periodic_busy = 0;
    }
  }
#endif

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
//! Prepares the USB device port for a hand-over to other code - a bootloader
//! jumping into its application, or the reverse.
//!
//! The session is deliberately left ALIVE: attached, addressed, endpoints
//! configured. The code taking over adopts it (see port_start) and the host
//! never sees a disconnect - its handles survive, exactly as they did with
//! the old stack. Only the interrupt is silenced, because between the jump
//! and the adoption there is no software to serve it; the hardware NAKs the
//! host on its own in the meantime, which is lossless.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_HandoffPrepare(void)
{
  if( !initialized )
    return -1;

  return MIOS32_USB_LL_IrqSilence(0);
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
