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


/////////////////////////////////////////////////////////////////////////////
// FreeRTOS opt-in defaults - tiered by RAM/FLASH budget
/////////////////////////////////////////////////////////////////////////////
//! Two independent opt-in switches control FreeRTOS usage. Both are
//! NUMERIC (0 or 1), not plain presence/absence #defines - checked with
//! "#if MIOS32_xxx_USE_FREERTOS", not "#ifdef" - specifically so a project
//! can override either one to 0 even on a chip that would otherwise default
//! to 1 (a bare "#undef" can't express "explicitly off" as opposed to
//! "undecided, let the tier default apply" - a numeric value can).
//!   - MIOS32_APP_USE_FREERTOS (renamed from the old opt-out
//!     MIOS32_DONT_USE_FREERTOS) - is the FreeRTOS kernel itself compiled
//!     in at all? Consulted by mios32_sys.c (the only FreeRTOS touchpoint
//!     in the whole mios32/common + family driver tree) and, at the Make
//!     level, by programming_models/traditional/programming_model.mk
//!     (whether tasks.c/queue.c/etc even get compiled - see that file for
//!     why the C-side #if alone isn't enough to save the FLASH cost).
//!   - MIOS32_CORE_USE_FREERTOS - does programming_models/traditional/
//!     main.c schedule the application Hooks via FreeRTOS tasks (=1) or
//!     via a bare-metal super-loop clocked by SysTick (=0)? See that file
//!     for the full implication - in bare mode, MIDI processing is no
//!     longer isolated from a slow/blocking application hook (no more task
//!     preemption).
//!
//! Both default here to whether this processor is in the "small" RAM/FLASH
//! tier (RAM <= 8K or FLASH <= 32K, physical chip specs, not the app-only
//! region after bootloader reservation) - on those chips FreeRTOS's own
//! kernel + heap already consumes the majority of what's available (verified
//! empirically: ~83% of a G030K6 build, ~half the total RAM on G031K8),
//! leaving too little room for a real application. Either switch can still
//! be overridden explicitly per-project in mios32_config.h regardless of
//! this default - a project on a small chip can force FreeRTOS back on (or
//! vice versa on a big chip) with e.g. "#define MIOS32_CORE_USE_FREERTOS 0".
#if defined(MIOS32_PROCESSOR_STM32G030K6) || defined(MIOS32_PROCESSOR_STM32G031K8)
// RAM=8K/FLASH=32K (G030K6) and RAM=8K/FLASH=64K (G031K8) - both qualify via
// the RAM<=8K leg. Re-verify actual RAM/FLASH before adding any further
// processor here - don't extrapolate from name/family similarity alone.
#define MIOS32_SYS_SMALL_CHIP_TIER 1
#endif

#ifndef MIOS32_APP_USE_FREERTOS
#if defined(MIOS32_SYS_SMALL_CHIP_TIER)
#define MIOS32_APP_USE_FREERTOS 0
#else
#define MIOS32_APP_USE_FREERTOS 1
#endif
#endif

#ifndef MIOS32_CORE_USE_FREERTOS
#if defined(MIOS32_SYS_SMALL_CHIP_TIER)
#define MIOS32_CORE_USE_FREERTOS 0
#else
#define MIOS32_CORE_USE_FREERTOS 1
#endif
#endif

//! MIOS32_CORE_USE_CANARI - optional stack-overflow canary for the
//! bare-metal super-loop (programming_models/traditional/main.c). Also
//! numeric, also overridable regardless of its default. Defaults to the
//! opposite of MIOS32_CORE_USE_FREERTOS: active (1) when running bare,
//! since FreeRTOS's own configCHECK_FOR_STACK_OVERFLOW protection is gone
//! along with the kernel; inactive (0) when FreeRTOS tasks are in use,
//! since that protection already covers it - a bare-metal canary there
//! would just be redundant flash/RAM cost. Unlike FreeRTOS's per-task
//! watermarking, only ONE canary is needed here: there's only one stack
//! left once tasks are gone.
#ifndef MIOS32_CORE_USE_CANARI
#if MIOS32_CORE_USE_FREERTOS
#define MIOS32_CORE_USE_CANARI 0
#else
#define MIOS32_CORE_USE_CANARI 1
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


// dynamic bootloader/app flash boundary (opt-in, MIOS32_USE_DYNAMIC_BSL_BOUNDARY
// in the project's own Makefile - see programming_model.mk and
// etc/gen_bsl_boundary.sh): if that mechanism generated a project-local
// mios32_bsl_boundary.h, pull it in here so MIOS32_APP_FLASH_START_ADDR is
// defined before it's used below. __has_include makes this a silent no-op
// for every project that doesn't use the mechanism (noboot .ld projects,
// the bootloader's own build) instead of requiring a per-project #include.
#if __has_include("mios32_bsl_boundary.h")
#include "mios32_bsl_boundary.h"
#endif

// Is there a bootloader at all? Set MIOS32_USE_BOOTLOADER = 0 in a project's
// Makefile (programming_model.mk relays it as a -D) to build an application
// linked at the base of flash, with no reserved bootloader region and no
// embedded bootloader image. Everything that only makes sense WITH one is
// then compiled out: the persistent info block below, the TAMP request /
// entry-override helpers in mios32_sys.c, and the "reboot into the
// bootloader" SysEx query in mios32_midi.c.
#ifndef MIOS32_USE_BOOTLOADER
# define MIOS32_USE_BOOTLOADER 1
#endif

// location of the Device ID and USB device name
// The bootloader update tool allows to change these values from MIOS terminal
// NOTE: deliberately left UNDEFINED when there is no bootloader. The block
// lives at (boundary - 0x100), which without a bootloader would resolve to
// 0x07FFFF00 - outside flash entirely - and every reader of it here and in
// mios32_midi.c / mios32_lcd.c is guarded by #ifdef on this very macro, so
// not defining it makes them all disappear cleanly rather than dereference
// a wild address.
#if !MIOS32_USE_BOOTLOADER
// no info block: nothing persistent to read from, see above
#elif defined(MIOS32_FAMILY_STM32F10x)
# define MIOS32_SYS_ADDR_BSL_INFO_BEGIN    0x08003f00
#elif defined(MIOS32_FAMILY_STM32F4xx) || defined(MIOS32_FAMILY_STM32G0xx)
// last 256 bytes before the (dynamic, per-project) bootloader/app boundary -
// must move together with MIOS32_APP_FLASH_START_ADDR, never a fixed address.
// F4xx: boundary is sector-granular (16K), rounds to a whole number of
// flash sectors - see etc/gen_bsl_boundary.sh. G0xx: page-granular (2K).
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

#if defined(MIOS32_FAMILY_STM32G0xx) || defined(MIOS32_FAMILY_STM32F4xx)
// magic value written to a backup register (STM32G0xx: TAMP/RTC: STM32F4xx:
// RTC->BKP0R directly) to request that the bootloader stays resident after
// the next reset (survives NVIC_SystemReset())
#define MIOS32_SYS_BOOTLOADER_MODE_MAGIC 0x424c0001

extern s32 MIOS32_SYS_BootloaderModeRequest(void);
extern s32 MIOS32_SYS_BootloaderModeRequested(void);
extern s32 MIOS32_SYS_AppEntryOverrideSet(u32 addr);
extern u32 MIOS32_SYS_AppEntryOverrideGet(void);
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
