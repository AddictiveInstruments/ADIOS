/*
 * Header file for the USB MSC (mass storage) host class.
 *
 * ==========================================================================
 *
 * HOW TO USE IT
 *
 * 1) Ask for the class in your mios32_config.h:
 *
 *      #define MIOS32_USE_USB_HOST_MSC
 *
 * 2) Poll for a medium and read it by sectors:
 *
 *      if( MIOS32_USB_MSC_CheckAvailable() ) {
 *        u8 buffer[512];
 *        MIOS32_USB_MSC_SectorRead(0, buffer);
 *      }
 *
 * The API mirrors MIOS32_SDCARD on purpose: both are 512-byte block devices,
 * and a file layer written against one mounts the other unchanged.
 *
 * The sector calls BLOCK - they drive the USB stack while waiting, with a
 * timeout - so call them from application context (APP_Tick or a task),
 * never from an interrupt.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _MIOS32_USB_MSC_H
#define _MIOS32_USB_MSC_H

#if defined(MIOS32_USE_USB_HOST_MSC)

extern s32 MIOS32_USB_MSC_Init(u32 mode);

// Called by the USB layer, not by an application.
extern s32 MIOS32_USB_MSC_Periodic_mS(void);


/////////////////////////////////////////////////////////////////////////////
// READY - the one medium being served
//
// The sector calls below read and write THIS medium. One is served at a time,
// and which one is decided by arrival: the device plugged in first, and within
// it the lowest slot holding a card. When that device leaves, the next in line
// takes over on its own.
/////////////////////////////////////////////////////////////////////////////

// 1 while a medium is attached and ready.
extern s32 MIOS32_USB_MSC_CheckAvailable(void);

// Capacity of the medium being served. Returns < 0 while there is none.
extern s32 MIOS32_USB_MSC_SizeGet(u32 *num_sectors, u16 *sector_size);

// Told when the served medium appears or goes away - including when it changes
// because one device took over from another. Capacity is what says WHICH
// medium it now is; ask for it when this fires.
extern s32 MIOS32_USB_MSC_ReadyCallback_Init(void (*callback)(u8 ready));


/////////////////////////////////////////////////////////////////////////////
// CONNECTED - everything attached
//
// A device can be present with no card in it, and a card reader can hold
// several. This says what is plugged in, whether or not it is the one being
// served.
/////////////////////////////////////////////////////////////////////////////

typedef struct {
  u8  dev;          // its USB address
  u8  num_units;    // slots it offers - a stick has one, a reader several
  u8  served;       // 1 if this is the device the sector calls are using
  u32 num_sectors;  // of the served unit, 0 if this device has no medium ready
  u16 sector_size;
} mios32_usb_msc_dev_info_t;

// How many devices are attached, in arrival order.
extern s32 MIOS32_USB_MSC_ConnectedNumGet(void);

// One of them, 0 being the one that arrived first. < 0 if n is out of range.
extern s32 MIOS32_USB_MSC_ConnectedGet(u8 n, mios32_usb_msc_dev_info_t *info);

// Told when a device is plugged in or unplugged. Reported from the periodic
// call, not from the moment it happens: an arrival lands while the stack is
// still bringing the device up, which is no place to run application code.
extern s32 MIOS32_USB_MSC_ChangeCallback_Init(void (*callback)(u8 dev, u8 connected));

// 512-byte sector transfer. Blocking, with timeout.
// Return: 0 on success, -1 no medium, -2 busy, -3 timeout, -4 refused
extern s32 MIOS32_USB_MSC_SectorRead(u32 sector, u8 *buffer);
extern s32 MIOS32_USB_MSC_SectorWrite(u32 sector, u8 *buffer);

#endif /* MIOS32_USE_USB_HOST_MSC */

#endif /* _MIOS32_USB_MSC_H */
