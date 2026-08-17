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
 * That pumping is why the sector calls must run in application context. They
 * call MIOS32_USB_Handler() themselves; from interrupt context that would
 * re-enter the stack, and the re-entrancy guard would turn the wait into a
 * timeout.
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

static u8  msc_daddr;        // device address, 0 = nothing attached
static u32 msc_num_sectors;
static u16 msc_sector_size;

// Completion handshake between the TinyUSB callback and the blocking call.
static volatile u8 xfer_done;
static volatile u8 xfer_ok;


/////////////////////////////////////////////////////////////////////////////
//! Initializes the USB MSC host class.
//! \param[in] mode currently only mode 0 is supported
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MSC_Init(u32 mode)
{
  if( mode != 0 )
    return -1;

  msc_daddr = 0;
  msc_num_sectors = 0;
  msc_sector_size = 0;

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
//! Capacity of the attached medium.
//! \return < 0 while nothing is attached
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
    if( waited_ms >= MIOS32_USB_MSC_TIMEOUT_MS )
      return -3;

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
  if( !MIOS32_USB_MSC_CheckAvailable() )
    return -1;

  xfer_done = 0;

  if( !tuh_msc_read10(msc_daddr, 0, buffer, sector, 1, xfer_complete_cb, 0) )
    return -2; // an operation is already pending

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
  if( !MIOS32_USB_MSC_CheckAvailable() )
    return -1;

  xfer_done = 0;

  if( !tuh_msc_write10(msc_daddr, 0, buffer, sector, 1, xfer_complete_cb, 0) )
    return -2;

  return xfer_wait();
}


/////////////////////////////////////////////////////////////////////////////
// TinyUSB host class callbacks
/////////////////////////////////////////////////////////////////////////////

void tuh_msc_mount_cb(uint8_t dev_addr)
{
  msc_daddr = dev_addr;
  msc_num_sectors = tuh_msc_get_block_count(dev_addr, 0);
  msc_sector_size = (u16)tuh_msc_get_block_size(dev_addr, 0);

  // The file layer above assumes 512-byte sectors, the near-universal value
  // for sticks and cards. A 4K-native medium would need real support, not a
  // silent misread - so it is simply not offered.
  if( msc_sector_size != 512 )
    msc_daddr = 0;
}


void tuh_msc_umount_cb(uint8_t dev_addr)
{
  if( msc_daddr == dev_addr ) {
    msc_daddr = 0;
    msc_num_sectors = 0;
    msc_sector_size = 0;
  }
}

#endif /* MIOS32_USE_USB_HOST_MSC */
