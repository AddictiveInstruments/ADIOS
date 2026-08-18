/*
 * USB MSC (mass storage) host class.
 *
 * A 512-byte block device with the same face as MIOS32_SDCARD, so the file
 * layer mounts a USB stick the way it mounts a card. Underneath, TinyUSB's
 * MSC host is asynchronous - a transfer is submitted and completes later, in
 * a callback - and this file turns that into the blocking sector calls the
 * file layer expects: submit, then drive the stack until the completion
 * arrives or a timeout says the medium is gone.
 *
 * That pumping is why the sector calls must run in a task, never from an
 * interrupt: they drive MIOS32_USB_Handler() themselves while they wait. They
 * ARE allowed from the callbacks below - the USB layer lifts its guard before
 * running any of this file, precisely so that being told "a medium is ready"
 * and reading a sector of it can happen in the same breath.
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

#if defined(MIOS32_USE_USB_HOST_MSC)

#include <tusb.h>


/////////////////////////////////////////////////////////////////////////////
// How long a sector transfer may take before the medium is declared gone.
// USB sticks routinely stall tens of milliseconds on a write (wear leveling);
// a whole second means the device left or died.
/////////////////////////////////////////////////////////////////////////////

#ifndef MIOS32_USB_MSC_TIMEOUT_MS
# define MIOS32_USB_MSC_TIMEOUT_MS  1000
#endif


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

// Attached devices, in the order they arrived. One medium is served at a
// time - see MIOS32_USB_MSC_SectorRead() - and this decides which: the one
// that got here first. When it leaves, the next in line takes over, and so on
// however many are plugged in. Arrival order is kept precisely so that adding
// a second device never disturbs the one already in use.
static u8  dev_list[CFG_TUH_DEVICE_MAX];
static u8  dev_count;

static u8  msc_dev;          // the device being served, 0 = none
static u8  msc_daddr;        // same, but only once a medium is usable
static u8  msc_lun;          // which unit of it carries that medium
static u32 msc_num_sectors;
static u16 msc_sector_size;

// Completion handshake between the TinyUSB callback and the blocking call.
static volatile u8 xfer_done;
static volatile u8 xfer_ok;

// Watching for a card appearing or leaving - see MIOS32_USB_MSC_Periodic_mS().
static u16 probe_countdown;
static u8  probe_lun;
static volatile u8 probe_busy;
static u16 probe_waited;
static u8  probe_retried;    // the complaint has been collected once
static u8  probe_seen;       // good answers in a row from a candidate slot
static u8  probe_missed;     // refusals in a row from the slot in use

static void (*ready_callback)(u8 ready);
static void (*change_callback)(u8 dev, u8 connected);

// What was last reported as ready, so a change can be spotted. The capacity
// identifies the medium: a different card is a different size, and one that
// comes back has passed through "nothing there" on the way.
static u8  ready_reported;
static u32 ready_reported_sectors;

// Arrivals and departures, held until the periodic call can report them. Not
// passed on the moment they happen: that moment is inside the stack's own
// bringing-up of the device, where application code disturbs the enumeration
// still in progress.
#define MSC_EVENT_MAX 8

static struct { u8 connected; u8 dev; } event_q[MSC_EVENT_MAX];
static u8 event_n;

static void event_add(u8 connected, u8 dev)
{
  if( event_n < MSC_EVENT_MAX ) {
    event_q[event_n].connected = connected;
    event_q[event_n].dev = dev;
    ++event_n;
  }
}


/////////////////////////////////////////////////////////////////////////////
//! Initializes the USB MSC host class.
//! \param[in] mode currently only mode 0 is supported
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MSC_Init(u32 mode)
{
  if( mode != 0 )
    return -1;

  dev_count = 0;
  msc_dev = 0;
  msc_daddr = 0;
  msc_num_sectors = 0;
  msc_sector_size = 0;

  ready_callback = NULL;
  change_callback = NULL;
  ready_reported = 0;
  ready_reported_sectors = 0;
  event_n = 0;

  // Nothing in flight: the watcher below may ask its first question.
  xfer_done = 1;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! \return 1 while a medium is attached and ready
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MSC_CheckAvailable(void)
{
  return (msc_daddr != 0 && tuh_msc_mounted(msc_daddr)) ? 1 : 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Installs the hook called when the served medium appears or goes away.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MSC_ReadyCallback_Init(void (*callback)(u8 ready))
{
  ready_callback = callback;
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Installs the hook called when a device is plugged in or unplugged.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MSC_ChangeCallback_Init(void (*callback)(u8 dev, u8 connected))
{
  change_callback = callback;
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! \return how many devices are attached, in arrival order
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MSC_ConnectedNumGet(void)
{
  return dev_count;
}


/////////////////////////////////////////////////////////////////////////////
//! One of the attached devices, 0 being the one that arrived first.
//! \return < 0 if n is out of range
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MSC_ConnectedGet(u8 n, mios32_usb_msc_dev_info_t *info)
{
  if( info == NULL || n >= dev_count )
    return -1;

  info->dev       = dev_list[n];
  info->num_units = tuh_msc_get_maxlun(dev_list[n]);
  info->served    = (dev_list[n] == msc_dev) ? 1 : 0;

  // Only the served device has a medium known to be ready: the others are not
  // being asked, so saying anything about their slots would be a guess.
  if( info->served && msc_daddr ) {
    info->num_sectors = msc_num_sectors;
    info->sector_size = msc_sector_size;
  } else {
    info->num_sectors = 0;
    info->sector_size = 0;
  }

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Capacity of the medium being served.
//! \return < 0 while there is none
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MSC_SizeGet(u32 *num_sectors, u16 *sector_size)
{
  if( !MIOS32_USB_MSC_CheckAvailable() )
    return -1;

  *num_sectors = msc_num_sectors;
  *sector_size = msc_sector_size;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// The completion callback, and the wait that pairs with it.
/////////////////////////////////////////////////////////////////////////////

static bool xfer_complete_cb(uint8_t daddr, const tuh_msc_complete_data_t *cb_data)
{
  (void)daddr;

  xfer_ok = (cb_data->csw->status == 0);
  xfer_done = 1;

  return true;
}

static s32 xfer_wait(void)
{
  u32 waited_ms = 0;

  while( !xfer_done ) {
    if( waited_ms >= MIOS32_USB_MSC_TIMEOUT_MS ) {
      // Give up on the answer, but do not leave the handshake half-open: this
      // flag is what tells everything else that nothing is in flight, and
      // stuck at zero it silences the watching of the slots for good.
      xfer_done = 1;
      return -3;
    }

    // The completion arrives when the host task processes the transfer, so
    // the stack has to be driven while waiting - this is what makes the call
    // application-context only.
    MIOS32_USB_Handler();
    MIOS32_DELAY_Wait_uS(1000);
    ++waited_ms;
  }

  return xfer_ok ? 0 : -4;
}


/////////////////////////////////////////////////////////////////////////////
//! Reads one 512-byte sector.
//! \param[in] sector the sector number
//! \param[out] buffer 512 bytes
//! \return 0 on success, -1 no medium, -2 busy, -3 timeout, -4 refused
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MSC_SectorRead(u32 sector, u8 *buffer)
{
  u32 waited_ms = 0;

  if( !MIOS32_USB_MSC_CheckAvailable() )
    return -1;

  // "Not now" is not "no". The device may still be finishing something of its
  // own - the watching of the slots asks it questions too, and a medium that
  // has just appeared is often mid-answer. This call is a blocking one with a
  // timeout of its own, so it waits its turn instead of handing back a failure
  // the caller can do nothing with.
  for(;;) {
    xfer_done = 0;

    if( tuh_msc_read10(msc_daddr, msc_lun, buffer, sector, 1, xfer_complete_cb, 0) )
      break;

    xfer_done = 1;

    if( waited_ms >= MIOS32_USB_MSC_TIMEOUT_MS )
      return -2;

    MIOS32_USB_Handler();
    MIOS32_DELAY_Wait_uS(1000);
    ++waited_ms;
  }

  return xfer_wait();
}


/////////////////////////////////////////////////////////////////////////////
//! Writes one 512-byte sector.
//! \param[in] sector the sector number
//! \param[in] buffer 512 bytes
//! \return 0 on success, -1 no medium, -2 busy, -3 timeout, -4 refused
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MSC_SectorWrite(u32 sector, u8 *buffer)
{
  u32 waited_ms = 0;

  if( !MIOS32_USB_MSC_CheckAvailable() )
    return -1;

  // Waits its turn, for the same reason as the read above.
  for(;;) {
    xfer_done = 0;

    if( tuh_msc_write10(msc_daddr, msc_lun, buffer, sector, 1, xfer_complete_cb, 0) )
      break;

    xfer_done = 1;

    if( waited_ms >= MIOS32_USB_MSC_TIMEOUT_MS )
      return -2;

    MIOS32_USB_Handler();
    MIOS32_DELAY_Wait_uS(1000);
    ++waited_ms;
  }

  return xfer_wait();
}


/////////////////////////////////////////////////////////////////////////////
// TinyUSB host class callbacks
/////////////////////////////////////////////////////////////////////////////

// Takes the device at the head of the queue and looks through its units for
// something usable. Called whenever the queue changes.
static void select_served(void)
{
  u8 lun, count;

  msc_dev = dev_count ? dev_list[0] : 0;
  msc_daddr = 0;
  msc_num_sectors = 0;
  msc_sector_size = 0;

  // A question left over from whatever was served before will never be
  // answered now. Left standing, that flag blocks every question that follows
  // and the slots are never looked at again.
  probe_busy = 0;
  probe_lun = 0;
  probe_countdown = 0;
  probe_seen = 0;
  probe_missed = 0;

  if( msc_dev == 0 )
    return;

  // A memory card reader is one device with one slot per unit, and the slots
  // that are empty answer nothing. So the units are looked through until one
  // holds a medium, rather than assuming the first one does - a reader whose
  // first slot happens to be empty is the normal case, not the exception.
  count = tuh_msc_get_maxlun(msc_dev);
  for(lun=0; lun<count; ++lun) {
    u32 sectors = tuh_msc_get_block_count(msc_dev, lun);
    u16 size    = (u16)tuh_msc_get_block_size(msc_dev, lun);

    // The file layer above assumes 512-byte sectors, the near-universal value
    // for sticks and cards. A 4K-native medium would need real support, not a
    // silent misread - so it is simply not offered.
    if( sectors && size == 512 ) {
      msc_daddr = msc_dev;
      msc_lun = lun;
      msc_num_sectors = sectors;
      msc_sector_size = size;
      break;
    }
  }
}


void tuh_msc_mount_cb(uint8_t dev_addr)
{
  u8 i;

  for(i=0; i<dev_count; ++i)
    if( dev_list[i] == dev_addr )
      return; // already known

  if( dev_count >= CFG_TUH_DEVICE_MAX )
    return;

  dev_list[dev_count++] = dev_addr;

  event_add(1, dev_addr);

  // Only the first one in gets served. A device arriving later waits its turn
  // rather than taking over from something that may be in the middle of being
  // read.
  if( dev_count == 1 )
    select_served();
}


void tuh_msc_umount_cb(uint8_t dev_addr)
{
  u8 i, j;

  for(i=0; i<dev_count; ++i) {
    if( dev_list[i] != dev_addr )
      continue;

    for(j=i; j+1<dev_count; ++j)
      dev_list[j] = dev_list[j+1]; // close the gap, keeping the order
    --dev_count;

    event_add(0, dev_addr);

    // Only re-choose if the one that left was the one in use. Otherwise
    // nothing changes for whoever is reading.
    if( msc_dev == dev_addr )
      select_served();

    return;
  }
}


/////////////////////////////////////////////////////////////////////////////
// Watching the slots
//
// The capacities are read once, while the device is being enumerated. That is
// enough for a stick, which is its own medium, but not for a card reader: it
// is normally plugged in empty and filled afterwards, and a card put in later
// would never be noticed. So the units are asked again, slowly, one per pass.
//
// Slowly on purpose. Each question is a SCSI command on the bus, and an empty
// slot answers "not ready" every time - there is no point paying for that more
// than about once a second.
/////////////////////////////////////////////////////////////////////////////

#ifndef MIOS32_USB_MSC_PROBE_INTERVAL_MS
# define MIOS32_USB_MSC_PROBE_INTERVAL_MS 1000
#endif

// How long to wait for an answer before writing the question off.
#ifndef MIOS32_USB_MSC_PROBE_TIMEOUT_MS
# define MIOS32_USB_MSC_PROBE_TIMEOUT_MS 2000
#endif

// The class hands the answer back raw - it fills its own table only while
// enumerating - so the reply is read here. Both fields are big-endian, and
// the count is the LAST block number, not how many there are.
static scsi_read_capacity10_resp_t probe_resp;

static bool probe_capacity_cb(uint8_t daddr, const tuh_msc_complete_data_t *cb_data)
{
  if( cb_data->csw->status == 0 ) {
    u32 sectors = tu_ntohl(probe_resp.last_lba) + 1;
    u16 size    = (u16)tu_ntohl(probe_resp.block_size);

    if( sectors && size == 512 ) {
      // Not offered on the strength of one good answer. A card just pushed in
      // says it is ready before it truly is, and the first read of it comes
      // back refused - which the layer above can only read as a broken medium.
      // A second answer, a moment later, is what settles it.
      if( msc_daddr != 0 || ++probe_seen >= 2 ) {
        msc_daddr = daddr;
        msc_lun = cb_data->cbw->lun;
        msc_num_sectors = sectors;
        msc_sector_size = size;
        probe_missed = 0;
      }
    } else {
      probe_seen = 0;
    }
  }

  probe_busy = 0;
  return true;
}

static bool probe_ready_cb(uint8_t daddr, const tuh_msc_complete_data_t *cb_data);

// Somewhere to put the answer to Request Sense. Its contents are of no
// interest here - asking is what clears the condition.
static u8 probe_sense[18];

static bool probe_sense_cb(uint8_t daddr, const tuh_msc_complete_data_t *cb_data)
{
  // The complaint has been collected; ask again now that it is out of the way.
  if( cb_data->csw->status != 0 ||
      !tuh_msc_test_unit_ready(daddr, cb_data->cbw->lun, probe_ready_cb, 0) )
    probe_busy = 0;

  return true;
}

static bool probe_ready_cb(uint8_t daddr, const tuh_msc_complete_data_t *cb_data)
{
  u8 lun = cb_data->cbw->lun;

  if( cb_data->csw->status != 0 ) {
    // A refusal is not the same as an empty slot. A unit whose card has just
    // been put in answers the first question with a complaint - "the medium
    // changed" - and keeps refusing until someone collects it. That is why a
    // card inserted while the reader stayed plugged in was never noticed,
    // while unplugging and plugging the reader back in worked: enumeration
    // collects the complaint, this did not.
    //
    // So it is collected here too, once, and the question asked again.
    if( !probe_retried ) {
      probe_retried = 1;
      if( tuh_msc_request_sense(daddr, lun, probe_sense, probe_sense_cb, 0) )
        return true; // answer comes back in probe_sense_cb
    }

    // The slot is not ready. If it is the one in use, that may mean the card
    // was taken out - or merely that this one question went badly. Dropping a
    // working medium on a single refusal makes it flicker in and out; it takes
    // two in a row to call it gone.
    if( msc_daddr == daddr && msc_lun == lun ) {
      if( ++probe_missed >= 2 ) {
        msc_daddr = 0;
        msc_num_sectors = 0;
        msc_sector_size = 0;
        probe_seen = 0;
      }
    } else {
      probe_seen = 0;
    }

    probe_busy = 0;
    return true;
  }

  probe_missed = 0;

  // Ready. Ask what is in it - a card that was swapped for another one is a
  // different size, so the answer is taken every time rather than assumed.
  if( !tuh_msc_read_capacity(daddr, lun, &probe_resp, probe_capacity_cb, 0) )
    probe_busy = 0;

  return true;
}


/////////////////////////////////////////////////////////////////////////////
//! Looks for a card appearing in, or leaving, one slot per call.
//! Called by the USB layer; an application has nothing to do.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MSC_Periodic_mS(void)
{
  u8 count;

  // Arrivals and departures, now that we are out of the stack's own work.
  while( event_n ) {
    u8 i;

    if( change_callback != NULL )
      change_callback(event_q[0].dev, event_q[0].connected);

    for(i=1; i<event_n; ++i)
      event_q[i-1] = event_q[i];
    --event_n;
  }

  // And whether the served medium changed. A different capacity means a
  // different medium, so swapping one card for another is reported as the
  // first one leaving and the new one arriving - which is what a file layer
  // has to hear to let go of anything it was holding.
  {
    u8 ready = MIOS32_USB_MSC_CheckAvailable() ? 1 : 0;

    if( ready != ready_reported ||
        (ready && msc_num_sectors != ready_reported_sectors) ) {

      if( ready_reported && ready_callback != NULL )
        ready_callback(0); // the old one is gone, whatever comes next

      ready_reported = ready;
      ready_reported_sectors = ready ? msc_num_sectors : 0;

      if( ready && ready_callback != NULL )
        ready_callback(1);
    }
  }

  if( msc_dev == 0 )
    return 0;

  // A question still outstanding blocks the next one - but only for so long.
  // An answer can go missing (a device pulled out mid-command, a reader that
  // simply never replies), and one lost answer must not silence the watching
  // for good. Well past any honest reply, the question is written off.
  if( probe_busy ) {
    if( ++probe_waited < MIOS32_USB_MSC_PROBE_TIMEOUT_MS )
      return 0;
    probe_busy = 0;
  }
  probe_waited = 0;

  // A sector transfer drives the stack from the application while it waits,
  // which lands back here. Asking a second question now would be refused, and
  // would cost the answer to the first.
  if( !xfer_done )
    return 0;

  if( probe_countdown ) {
    --probe_countdown;
    return 0;
  }
  probe_countdown = MIOS32_USB_MSC_PROBE_INTERVAL_MS;

  count = tuh_msc_get_maxlun(msc_dev);
  if( count < 1 )
    return 0;

  // While a medium is in use, that is the only slot worth asking about - so
  // its removal is noticed within a second or so. With nothing in use, the
  // slots are toured one per pass instead, looking for a card to appear.
  //
  // A slot that has just answered well is asked again rather than left for the
  // end of the tour: the confirmation is what makes it usable, and waiting a
  // whole tour for it would make every insertion feel broken.
  if( msc_daddr != 0 )
    probe_lun = msc_lun;
  else if( probe_seen == 0 && ++probe_lun >= count )
    probe_lun = 0;

  probe_busy = 1;
  probe_retried = 0;
  if( !tuh_msc_test_unit_ready(msc_dev, probe_lun, probe_ready_cb, 0) )
    probe_busy = 0;

  return 0;
}

#endif /* MIOS32_USE_USB_HOST_MSC */
