/*
 * USB MIDI transport.
 *
 * Serves the functions mios32_midi.c expects from a transport, on top of
 * TinyUSB - the device class on one controller, the host class on the other.
 *
 * There is no format conversion here, and that is not an accident: a
 * mios32_midi_package_t IS a USB MIDI 1.0 event packet. Its first byte holds
 * the cable in the high nibble and the code index number in the low nibble,
 * which is exactly what the class puts on the wire, and the three that follow
 * are the MIDI bytes. So the two talk to each other by plain copy.
 *
 * PORT NUMBERING. The OS addresses USB MIDI by a single index 0..31, which is
 * simply (port - USB0). The MIDI port ranges were laid out so that 0x10..0x2f
 * is contiguous, which is what lets this index split cleanly:
 *
 *   controller = index / 16      cable = index % 16
 *
 * Controller 0 is what this machine presents to a host; controller 1 is what
 * it drives. Same numbering either way - the range says which socket, not
 * which role.
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
// How long a blocking send waits before giving up, in milliseconds. A host
// that has stopped collecting must not be able to stall the application
// forever - losing a package is recoverable, a frozen instrument is not.
/////////////////////////////////////////////////////////////////////////////

#ifndef MIOS32_USB_MIDI_SEND_TIMEOUT_MS
# define MIOS32_USB_MIDI_SEND_TIMEOUT_MS  100
#endif

#define CABLES_PER_CONTROLLER  16


// TEMPORARY diagnostics, read over SWD. Declared here rather than beside
// their uses because the send path comes first in this file.
u32 mios32_usb_dbg_rxpoll;   // PackageReceive called
u32 mios32_usb_dbg_mounted;  // tud_midi_mounted() was true
u32 mios32_usb_dbg_rxpkt;    // a device packet was actually returned
u32 mios32_usb_dbg_txok;     // device packet written
u32 mios32_usb_dbg_txfull;   // device write refused (buffer full)
u32 mios32_usb_dbg_txna;     // send refused: cable not available
u32 mios32_usb_dbg_rxhost;   // packet received from an attached device (host side)


/////////////////////////////////////////////////////////////////////////////
// Host-side cable allocation
//
// TinyUSB gives each ATTACHED MIDI interface an index of its own, and each
// carries its own cables. The OS, on the other hand, offers one flat range of
// sixteen. So attached interfaces are given consecutive blocks of that range
// as they arrive, and their blocks are released when they leave.
//
// Blocks are not compacted on unplug: a keyboard that disappears and comes
// back keeps its numbers as long as nothing else claimed them, which is what
// an application storing a route expects. The range fills up rather than
// shuffles.
/////////////////////////////////////////////////////////////////////////////

#if CFG_TUH_MIDI

typedef struct {
  u8 in_use;
  u8 tuh_idx;     // TinyUSB's index for the attached interface
  u8 first_cable; // where its block starts in the OS range
  u8 num_cables;
} host_itf_t;

static host_itf_t host_itf[CFG_TUH_DEVICE_MAX];

// OS cable -> attached interface, or 0xff when nothing owns it
static u8 host_cable_owner[CABLES_PER_CONTROLLER];

static void (*host_change_callback)(u8 itf, u8 connected);

// Arrivals and departures, held until the periodic call can report them. Not
// passed on the moment they happen: that moment is inside the stack's own
// bringing-up of the device, where application code disturbs the enumeration
// still in progress - which shows up as other devices on the same bus never
// appearing at all.
#define MIDI_EVENT_MAX 8

static struct { u8 connected; u8 itf; } event_q[MIDI_EVENT_MAX];
static u8 event_n;

static void event_add(u8 connected, u8 itf)
{
  if( event_n < MIDI_EVENT_MAX ) {
    event_q[event_n].connected = connected;
    event_q[event_n].itf = itf;
    ++event_n;
  }
}

#endif


/////////////////////////////////////////////////////////////////////////////
//! Initializes the USB MIDI transport.
//! The stack itself is brought up by MIOS32_USB_Init(), which the core calls
//! first.
//! \param[in] mode currently only mode 0 is supported
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_Init(u32 mode)
{
  if( mode != 0 )
    return -1;

#if CFG_TUH_MIDI
  {
    u8 i;

    host_change_callback = NULL;
    event_n = 0;

    for(i=0; i<CFG_TUH_DEVICE_MAX; ++i)
      host_itf[i].in_use = 0;
    for(i=0; i<CABLES_PER_CONTROLLER; ++i)
      host_cable_owner[i] = 0xff;
  }
#endif

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! \param[in] idx 0..31 - see the note on port numbering above
//! \return 1 if that cable can carry MIDI right now
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_CheckAvailable(u8 idx)
{
  u8 controller = idx / CABLES_PER_CONTROLLER;
  u8 cable      = idx % CABLES_PER_CONTROLLER;

  if( idx >= (MIOS32_USB_NUM_PORTS * CABLES_PER_CONTROLLER) )
    return 0;

  if( controller == 0 ) {
#if CFG_TUD_MIDI
    if( cable >= MIOS32_USB_MIDI_NUM_PORTS )
      return 0;
    // Mounted means the host has configured us. Before that the cables exist
    // in the descriptor but lead nowhere.
    return tud_midi_mounted() ? 1 : 0;
#else
    return 0;
#endif
  }

#if CFG_TUH_MIDI
  {
    u8 owner = host_cable_owner[cable];
    if( owner == 0xff || !host_itf[owner].in_use )
      return 0;
    return tuh_midi_mounted(host_itf[owner].tuh_idx) ? 1 : 0;
  }
#else
  return 0;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Sends a package, or reports that it could not be sent right now.
//! \param[in] idx 0..31
//! \param[in] package the package to send
//! \return 0 on success, -1 if the cable is not available, -2 if the buffer
//!         is full - which is a "try again", not a failure
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_PackageSend_NonBlocking(u8 idx, mios32_midi_package_t package)
{
  u8 controller = idx / CABLES_PER_CONTROLLER;
  u8 cable      = idx % CABLES_PER_CONTROLLER;

  if( !MIOS32_USB_MIDI_CheckAvailable(idx) ) {
    ++mios32_usb_dbg_txna;
    return -1;
  }

  if( controller == 0 ) {
#if CFG_TUD_MIDI
    package.cable = cable;
    if( tud_midi_packet_write(package.bytes) ) { ++mios32_usb_dbg_txok; return 0; }
    ++mios32_usb_dbg_txfull;
    return -2;
#else
    return -1;
#endif
  }

#if CFG_TUH_MIDI
  {
    u8 owner = host_cable_owner[cable];
    // The attached device numbers its own cables from zero, so the OS cable
    // has to be brought back into its local space before sending.
    package.cable = cable - host_itf[owner].first_cable;
    return tuh_midi_packet_write_n(host_itf[owner].tuh_idx, package.bytes, 4) ? 0 : -2;
  }
#else
  return -1;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Sends a package, waiting for room if need be.
//! \param[in] idx 0..31
//! \param[in] package the package to send
//! \return 0 on success, -1 if the cable is not available, -3 on timeout
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_PackageSend(u8 idx, mios32_midi_package_t package)
{
  u32 waited_ms = 0;

  for(;;) {
    s32 status = MIOS32_USB_MIDI_PackageSend_NonBlocking(idx, package);

    if( status != -2 )
      return status; // sent, or the cable is gone

    if( waited_ms >= MIOS32_USB_MIDI_SEND_TIMEOUT_MS )
      return -3;

    // Deliberately NOT pumping the stack here. Doing so re-enters TinyUSB
    // from inside whatever callback led to this send, which is not safe. The
    // buffers are sized so a whole answer fits without needing a mid-flight
    // drain; if we ever land here, waiting for the tick is the correct thing,
    // and the timeout above is what keeps it bounded.
    MIOS32_DELAY_Wait_uS(1000);
    ++waited_ms;
  }
}


/////////////////////////////////////////////////////////////////////////////
//! Takes the next received package, from either controller.
//! \param[out] package the package
//! \param[out] idx which 0..31 cable it arrived on
//! \return 0 if a package was returned, -1 if nothing is waiting
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_PackageReceive(mios32_midi_package_t *package, u8 *idx)
{
  ++mios32_usb_dbg_rxpoll;

#if CFG_TUD_MIDI
  if( tud_midi_mounted() ) {
    ++mios32_usb_dbg_mounted;
    if( tud_midi_packet_read(package->bytes) ) {
      ++mios32_usb_dbg_rxpkt;
      *idx = package->cable; // controller 0, so the cable is the index
      return 0;
    }
  }
#endif

#if CFG_TUH_MIDI
  {
    u8 i;
    for(i=0; i<CFG_TUH_DEVICE_MAX; ++i) {
      if( !host_itf[i].in_use )
        continue;

      if( tuh_midi_packet_read_n(host_itf[i].tuh_idx, package->bytes, 4) == 4 ) {
        // Local cable of the attached device -> OS cable, then into the
        // second controller's range.
        ++mios32_usb_dbg_rxhost;
        u8 cable = host_itf[i].first_cable + package->cable;
        *idx = CABLES_PER_CONTROLLER + cable;
        return 0;
      }
    }
  }
#endif

  return -1;
}


/////////////////////////////////////////////////////////////////////////////
//! Periodic service.
//! The stack is driven by MIOS32_USB_Handler() from the same tick, so this
//! exists only to keep the transport interface uniform.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_Periodic_mS(void)
{
#if CFG_TUH_MIDI
  // Arrivals and departures, now that we are out of the stack's own work.
  while( event_n ) {
    u8 i;

    if( host_change_callback != NULL )
      host_change_callback(event_q[0].itf, event_q[0].connected);

    for(i=1; i<event_n; ++i)
      event_q[i-1] = event_q[i];
    --event_n;
  }
#endif

  return 0;
}


#if CFG_TUH_MIDI

/////////////////////////////////////////////////////////////////////////////
//! Installs the hook called when an attached device arrives or leaves.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_HostChangeCallback_Init(void (*callback)(u8 itf, u8 connected))
{
  host_change_callback = callback;
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Where an attached device sits in the port range, and how big it is.
//! \return < 0 if the index is out of range
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_HostInfoGet(u8 itf, mios32_usb_midi_host_info_t *info)
{
  u8 i;

  if( info == NULL )
    return -1;

  info->connected = 0;
  info->first_port = 0;
  info->num_ports = 0;
  info->num_in = 0;
  info->num_out = 0;

  if( itf >= CFG_TUH_DEVICE_MAX )
    return -1;

  // The table is indexed by our own slot, not by TinyUSB's index, so the one
  // being asked about has to be found.
  for(i=0; i<CFG_TUH_DEVICE_MAX; ++i) {
    if( !host_itf[i].in_use || host_itf[i].tuh_idx != itf )
      continue;

    info->connected  = 1;
    info->first_port = USB16 + host_itf[i].first_cable;
    info->num_ports  = host_itf[i].num_cables;
    info->num_in     = tuh_midi_get_rx_cable_count(itf);
    info->num_out    = tuh_midi_get_tx_cable_count(itf);
    break;
  }

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! \return how many interfaces may be attached at once
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_HostNumGet(void)
{
  return CFG_TUH_DEVICE_MAX;
}

#endif /* CFG_TUH_MIDI */


/////////////////////////////////////////////////////////////////////////////
// Host class callbacks: something was plugged in, or pulled out.
/////////////////////////////////////////////////////////////////////////////

#if CFG_TUH_MIDI

void tuh_midi_mount_cb(uint8_t tuh_idx, const tuh_midi_mount_cb_t *mount_cb_data)
{
  u8 i, c;
  u8 rx = tuh_midi_get_rx_cable_count(tuh_idx);
  u8 tx = tuh_midi_get_tx_cable_count(tuh_idx);
  u8 num = (rx > tx) ? rx : tx;

  (void)mount_cb_data;

  if( num < 1 )
    num = 1;

  for(i=0; i<CFG_TUH_DEVICE_MAX; ++i) {
    if( host_itf[i].in_use )
      continue;

    // First run of free cables wide enough for this device.
    for(c=0; (c + num) <= CABLES_PER_CONTROLLER; ++c) {
      u8 k, free = 1;
      for(k=0; k<num; ++k)
        if( host_cable_owner[c + k] != 0xff ) { free = 0; break; }

      if( free ) {
        for(k=0; k<num; ++k)
          host_cable_owner[c + k] = i;

        host_itf[i].in_use      = 1;
        host_itf[i].tuh_idx     = tuh_idx;
        host_itf[i].first_cable = c;
        host_itf[i].num_cables  = num;

        event_add(1, tuh_idx);
        return;
      }
    }

    // No room. Better to ignore the device than to hand it cables that
    // already belong to another one.
    return;
  }
}


void tuh_midi_umount_cb(uint8_t tuh_idx)
{
  u8 i, k;

  event_add(0, tuh_idx);

  for(i=0; i<CFG_TUH_DEVICE_MAX; ++i) {
    if( !host_itf[i].in_use || host_itf[i].tuh_idx != tuh_idx )
      continue;

    for(k=0; k<host_itf[i].num_cables; ++k)
      host_cable_owner[host_itf[i].first_cable + k] = 0xff;

    host_itf[i].in_use = 0;
    return;
  }
}


void tuh_midi_rx_cb(uint8_t tuh_idx, uint32_t xferred_bytes)
{
  // Nothing to do: MIOS32_USB_MIDI_PackageReceive() drains the interfaces
  // from the MIDI handler, in the same place every other transport is polled.
  // Pulling packages out here would deliver them from interrupt context.
  (void)tuh_idx;
  (void)xferred_bytes;
}

#endif /* CFG_TUH_MIDI */

#endif /* MIOS32_USE_USB */
