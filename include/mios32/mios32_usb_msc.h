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
 *  Copyright (C) 2026 Bruno Dupeyron
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

#ifndef _MIOS32_USB_MSC_H
#define _MIOS32_USB_MSC_H

#if defined(MIOS32_USE_USB_HOST_MSC)

extern s32 MIOS32_USB_MSC_Init(u32 mode);

// 1 while a medium is attached and ready.
extern s32 MIOS32_USB_MSC_CheckAvailable(void);

// Capacity of the attached medium. Returns < 0 while nothing is attached.
extern s32 MIOS32_USB_MSC_SizeGet(u32 *num_sectors, u16 *sector_size);

// 512-byte sector transfer. Blocking, with timeout.
// Return: 0 on success, -1 no medium, -2 busy, -3 timeout, -4 refused
extern s32 MIOS32_USB_MSC_SectorRead(u32 sector, u8 *buffer);
extern s32 MIOS32_USB_MSC_SectorWrite(u32 sector, u8 *buffer);

#endif /* MIOS32_USE_USB_HOST_MSC */

#endif /* _MIOS32_USB_MSC_H */
