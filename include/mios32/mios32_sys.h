// $Id: mios32_sys.h 2097 2014-12-05 22:05:12Z tk $
/*
 * Header file for MIOS32 System Initialisation
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _MIOS32_SYS_H
#define _MIOS32_SYS_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////


// constants which define the CPU frequency
// please note: changing this constant won't lead to any frequency change, instead
// timers used by MIOS32 based components (and also FreeRTOS) will be configured
// wrongly.
// In order to change the frequency please add a clock option to MIOS32_SYS_Init()
#ifndef MIOS32_SYS_CPU_FREQUENCY
#if defined(MIOS32_FAMILY_STM32F10x)
# define MIOS32_SYS_CPU_FREQUENCY 72000000
#elif defined(MIOS32_FAMILY_STM32F4xx)
# define MIOS32_SYS_CPU_FREQUENCY 168000000
#elif defined(MIOS32_FAMILY_STM32G0xx)
# define MIOS32_SYS_CPU_FREQUENCY 64000000
#else
  // dummy
# define MIOS32_SYS_CPU_FREQUENCY 100000000
#endif
#endif


#if defined(MIOS32_FAMILY_STM32F10x)
//! STM32F1 specific help macros for pin access
# define MIOS32_SYS_STM_PINSET(port, pin_mask, v) { if( v ) port->BSRR = pin_mask; else port->BRR = pin_mask; }
# define MIOS32_SYS_STM_PINSET_1(port, pin_mask)  { port->BSRR = pin_mask; }
# define MIOS32_SYS_STM_PINSET_0(port, pin_mask)  { port->BRR = pin_mask; }
# define MIOS32_SYS_STM_PINGET(port, pin_mask)    ((port->IDR & (pin_mask)) ? 1 : 0)
#endif

#if defined(MIOS32_FAMILY_STM32F4xx) || defined(MIOS32_FAMILY_STM32G0xx)
//! STM32F4 specific help macros for pin access
# define MIOS32_SYS_STM_PINSET(port, pin_mask, v) { if( v ) port->BSRR = pin_mask; else port->BSRR = (pin_mask<<16); }
# define MIOS32_SYS_STM_PINSET_1(port, pin_mask)  { port->BSRR = pin_mask; }
# define MIOS32_SYS_STM_PINSET_0(port, pin_mask)  { port->BSRR = (pin_mask<<16); }
# define MIOS32_SYS_STM_PINGET(port, pin_mask)    ((port->IDR & (pin_mask)) ? 1 : 0)
#endif

// STM32 only:
// The DBGMCU_CR register allows to suspend peripherals when CPU is in halt
// state to simplify debugging (e.g. no timer interrupt is triggered each
// time the program is stepped)
// See STM32 reference manual for the meaning of these flags.
// By default, we suspend all peripherals which are provided by DBGMCU_CR
#ifndef MIOS32_SYS_STM32_DBGMCU_CR
#define MIOS32_SYS_STM32_DBGMCU_CR 0x001fff00
#endif


// location of the Device ID and USB device name
// The bootloader update tool allows to change these values from MIOS terminal
#if defined(MIOS32_FAMILY_STM32F10x) || defined(MIOS32_FAMILY_STM32F4xx)
# define MIOS32_SYS_ADDR_BSL_INFO_BEGIN    0x08003f00
#elif defined(MIOS32_FAMILY_STM32G0xx)
// last 256 bytes before the (dynamic, per-project) bootloader/app boundary -
// must move together with MIOS32_APP_FLASH_START_ADDR, never a fixed address
# define MIOS32_SYS_ADDR_BSL_INFO_BEGIN    (0x08000000 + MIOS32_APP_FLASH_START_ADDR - 0x100)
#elif defined(MIOS32_FAMILY_LPC17xx)
# define MIOS32_SYS_ADDR_BSL_INFO_BEGIN    0x00003f00
#else
// no warning or error for other families... just don't support these features
#endif

#ifdef MIOS32_SYS_ADDR_BSL_INFO_BEGIN
// NOTE: a change here will mean that:
//   - the bootloader update application (which programs the parameters) has to be re-released
//   - all applications have to be re-released
//   -> better never change the addresses!!!
// New parameters can be added by:
//   - asking TK
//   - searching for an unused offset
//   - adding a confirmation code (parameters will only be taken if confirmation code is 0x42)
//   - adding parameter addresses
# define MIOS32_SYS_ADDR_LCD_PAR_CONFIRM    (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xc0) // 0x42 to confirm value
# define MIOS32_SYS_ADDR_LCD_PAR_TYPE       (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xc1)
# define MIOS32_SYS_ADDR_LCD_PAR_NUM_X      (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xc2)
# define MIOS32_SYS_ADDR_LCD_PAR_NUM_Y      (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xc3)
# define MIOS32_SYS_ADDR_LCD_PAR_WIDTH      (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xc4)
# define MIOS32_SYS_ADDR_LCD_PAR_HEIGHT     (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xc5)

# define MIOS32_SYS_ADDR_DEVICE_ID_CONFIRM  (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xd0) // 0x42 to confirm value
# define MIOS32_SYS_ADDR_DEVICE_ID          (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xd1)
# define MIOS32_SYS_ADDR_FASTBOOT_CONFIRM   (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xd2) // 0x42 to confirm value
# define MIOS32_SYS_ADDR_FASTBOOT           (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xd3)
# define MIOS32_SYS_ADDR_SINGLE_USB_CONFIRM (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xd4) // 0x42 to confirm value
# define MIOS32_SYS_ADDR_SINGLE_USB         (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xd5)
# define MIOS32_SYS_ADDR_ENFORCE_USB_DEVICE_CONFIRM (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xd6) // 0x42 to confirm value
# define MIOS32_SYS_ADDR_ENFORCE_USB_DEVICE (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xd7)
# define MIOS32_SYS_ADDR_SPI_MIDI_CONFIRM   (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xd8) // 0x42 to confirm value
# define MIOS32_SYS_ADDR_SPI_MIDI           (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xd9)


# define MIOS32_SYS_ADDR_USB_DEV_NAME      (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xe0) // all chars != 0x00 and 0x20...0x7f to confirm string
# define MIOS32_SYS_USB_DEV_NAME_LEN       0x20

# define MIOS32_SYS_ADDR_BSL_INFO_END      (MIOS32_SYS_ADDR_BSL_INFO_BEGIN+0xff)
#endif


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////

// *not* compatible to NTP timestamp format, the fraction has mS accuracy
typedef struct {
  u32 seconds;
  u32 fraction_ms;
} mios32_sys_time_t;


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_SYS_Init(u32 mode);

extern s32 MIOS32_SYS_Reset(void);

#if defined(MIOS32_FAMILY_STM32G0xx)
// magic value written to the TAMP/RTC backup register to request that the
// bootloader stays resident after the next reset (survives NVIC_SystemReset())
#define MIOS32_SYS_BOOTLOADER_MODE_MAGIC 0x424c0001

extern s32 MIOS32_SYS_BootloaderModeRequest(void);
extern s32 MIOS32_SYS_BootloaderModeRequested(void);
#endif

extern u32 MIOS32_SYS_ChipIDGet(void);
extern u32 MIOS32_SYS_FlashSizeGet(void);
extern u32 MIOS32_SYS_RAMSizeGet(void);
extern s32 MIOS32_SYS_SerialNumberGet(char *str);

extern s32 MIOS32_SYS_TimeSet(mios32_sys_time_t t);
extern mios32_sys_time_t MIOS32_SYS_TimeGet(void);

extern s32 MIOS32_SYS_DMA_CallbackSet(u8 dma, u8 chn, void *callback);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#endif /* _MIOS32_SYS_H */
