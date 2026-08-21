/*
 * Header file for ADIOS System Initialisation
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _ADIOS_SYS_H
#define _ADIOS_SYS_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////


// constants which define the CPU frequency
// please note: changing this constant won't lead to any frequency change, instead
// timers used by ADIOS based components (and also FreeRTOS) will be configured
// wrongly.
// In order to change the frequency please add a clock option to ADIOS_SYS_Init()
#ifndef ADIOS_SYS_CPU_FREQUENCY
#if defined(ADIOS_FAMILY_STM32F10x)
# define ADIOS_SYS_CPU_FREQUENCY 72000000
#elif defined(ADIOS_FAMILY_STM32F4xx)
# define ADIOS_SYS_CPU_FREQUENCY 168000000
#elif defined(ADIOS_FAMILY_STM32G0xx)
# define ADIOS_SYS_CPU_FREQUENCY 64000000
#else
  // dummy
# define ADIOS_SYS_CPU_FREQUENCY 100000000
#endif
#endif


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FreeRTOS switches - presence/absence, like every other ADIOS_USE_* opt-in
/////////////////////////////////////////////////////////////////////////////
//! ADIOS_CORE_DONT_USE_FREERTOS - opt-OUT: core/main.c
//! drives the application hooks from a bare-metal super-loop clocked by
//! SysTick instead of FreeRTOS tasks, and the kernel is not compiled at all.
//! Defined automatically by core/core.mk on small chips
//! (FLASH <= 32K or RAM <= 8K, real figures taken from etc/ld/<family>.ld.S:
//! on those, kernel + heap would consume most of the chip - measured ~83% of
//! a G030K6 build, ~half the total RAM of a G031K8). On any bigger chip,
//! define it in adios_config.h to get the same bare-metal build.
//! Bare-mode implication: MIDI processing is no longer isolated from a
//! slow/blocking application hook (no task preemption) - see core/main.c.
//!
//! ADIOS_APP_USE_FREERTOS - opt-in, adios_config.h only: the application
//! itself calls FreeRTOS (tasks, queues, semaphores...). It requires the
//! scheduler, so it cannot be combined with the opt-out above - core/main.c
//! is who starts the scheduler; without it those calls could never run.
#if defined(ADIOS_APP_USE_FREERTOS) && defined(ADIOS_CORE_DONT_USE_FREERTOS)
# error "ADIOS_APP_USE_FREERTOS needs the scheduler: remove ADIOS_CORE_DONT_USE_FREERTOS (or the app opt-in)"
#endif

//! ADIOS_CORE_USE_CANARI - optional stack-overflow canary for the
//! bare-metal super-loop (core/main.c). Also
//! numeric, also overridable regardless of its default. Defaults to
//! active (1) exactly when the core runs bare-metal,
//! since FreeRTOS's own configCHECK_FOR_STACK_OVERFLOW protection is gone
//! along with the kernel; inactive (0) when FreeRTOS tasks are in use,
//! since that protection already covers it - a bare-metal canary there
//! would just be redundant flash/RAM cost. Unlike FreeRTOS's per-task
//! watermarking, only ONE canary is needed here: there's only one stack
//! left once tasks are gone.
#ifndef ADIOS_CORE_USE_CANARI
#ifdef ADIOS_CORE_DONT_USE_FREERTOS
#define ADIOS_CORE_USE_CANARI 1
#else
#define ADIOS_CORE_USE_CANARI 0
#endif
#endif


#if defined(ADIOS_FAMILY_STM32F10x)
//! STM32F1 specific help macros for pin access
# define ADIOS_SYS_STM_PINSET(port, pin_mask, v) { if( v ) port->BSRR = pin_mask; else port->BRR = pin_mask; }
# define ADIOS_SYS_STM_PINSET_1(port, pin_mask)  { port->BSRR = pin_mask; }
# define ADIOS_SYS_STM_PINSET_0(port, pin_mask)  { port->BRR = pin_mask; }
# define ADIOS_SYS_STM_PINGET(port, pin_mask)    ((port->IDR & (pin_mask)) ? 1 : 0)
#endif

#if defined(ADIOS_FAMILY_STM32F4xx) || defined(ADIOS_FAMILY_STM32G0xx)
//! STM32F4 specific help macros for pin access
# define ADIOS_SYS_STM_PINSET(port, pin_mask, v) { if( v ) port->BSRR = pin_mask; else port->BSRR = (pin_mask<<16); }
# define ADIOS_SYS_STM_PINSET_1(port, pin_mask)  { port->BSRR = pin_mask; }
# define ADIOS_SYS_STM_PINSET_0(port, pin_mask)  { port->BSRR = (pin_mask<<16); }
# define ADIOS_SYS_STM_PINGET(port, pin_mask)    ((port->IDR & (pin_mask)) ? 1 : 0)
#endif

// How a pin is driven. Used wherever a caller has to say what kind of output
// it wants - ADIOS_UART_InitPort(), for one, needs to know whether a TX line
// is push-pull or open drain.
typedef enum {
  ADIOS_PIN_MODE_IGNORE = 0,
  ADIOS_PIN_MODE_ANALOG,
  ADIOS_PIN_MODE_INPUT,
  ADIOS_PIN_MODE_INPUT_PD,
  ADIOS_PIN_MODE_INPUT_PU,
  ADIOS_PIN_MODE_OUTPUT_PP,
  ADIOS_PIN_MODE_OUTPUT_OD
} adios_pin_mode_t;

// STM32 only:
// The DBGMCU_CR register allows to suspend peripherals when CPU is in halt
// state to simplify debugging (e.g. no timer interrupt is triggered each
// time the program is stepped)
// See STM32 reference manual for the meaning of these flags.
// By default, we suspend all peripherals which are provided by DBGMCU_CR
#ifndef ADIOS_SYS_STM32_DBGMCU_CR
#define ADIOS_SYS_STM32_DBGMCU_CR 0x001fff00
#endif


// Is there a bootloader at all? Set ADIOS_USE_BOOTLOADER = 0 in a project's
// Makefile (core.mk relays it as a -D) to build an application
// linked at the base of flash, with no reserved bootloader region and no
// embedded bootloader image. Everything that only makes sense WITH one is
// then compiled out: the persistent info block below, the TAMP request /
// entry-override helpers in adios_sys.c, and the "reboot into the
// bootloader" SysEx query in adios_midi.c.
#ifndef ADIOS_USE_BOOTLOADER
# define ADIOS_USE_BOOTLOADER 1
#endif

// dynamic bootloader/app flash boundary (opt-in, ADIOS_USE_DYNAMIC_BSL_BOUNDARY
// in the project's own Makefile - see core.mk and
// etc/gen_bsl_boundary.sh): if that mechanism generated a project-local
// adios_bsl_boundary.h, pull it in here so ADIOS_APP_FLASH_START_ADDR is
// defined before it's used below. __has_include makes this a silent no-op
// for every project that doesn't use the mechanism instead of requiring a
// per-project #include.
// The ADIOS_USE_BOOTLOADER guard matters: a project that HAS built with the
// dynamic mechanism keeps that generated header in its directory forever, and
// without this guard it would silently redefine ADIOS_APP_FLASH_START_ADDR
// over the 0 that core.mk passes for a bootloader-less build,
// reporting a boundary that is not on the chip.
#if ADIOS_USE_BOOTLOADER
# if __has_include("adios_bsl_boundary.h")
#  include "adios_bsl_boundary.h"
# endif
#endif


/////////////////////////////////////////////////////////////////////////////
// Persistent SysEx device ID (ADIOS_DEVICE_ID_PERSIST, opt-in from the
// project's Makefile - see core/core.mk)
//
// The record occupies the LAST TWO BYTES of flash: a 0x42 confirm marker
// followed by the value. Deliberately the very top of memory, because that is
// the one address an application and a bootloader both compute for themselves
// - so neither has to be told where the other put it, and the two stay
// independent. It sits inside the ADIOS_USERDATA_PAGES region, which the
// linker script carves out of the application's own FLASH region: reserving
// the page is what makes an application that grows into it fail to link.
//
// A project that already keeps data at the top of flash moves its own fields
// down rather than relocating this record. Anchoring system fields at the END
// of the last page keeps them independent of whatever layout the project uses
// below, so both can grow towards each other without either being renumbered.
/////////////////////////////////////////////////////////////////////////////
#ifndef ADIOS_DEVICE_ID_PERSIST
# define ADIOS_DEVICE_ID_PERSIST 0
#endif

#if ADIOS_DEVICE_ID_PERSIST
// No check on ADIOS_USERDATA_PAGES here, deliberately: the reservation is the
// APPLICATION's business and its Makefile already enforces it (the switch
// defaults the page count, and an explicit 0 is a build error). The BOOTLOADER
// compiles this same file with the switch relayed through its generated header
// and knows nothing of page counts - it only ever READS the two bytes. A check
// here therefore broke the bootloader build instead of catching anything.
// runtime expressions: the flash size comes from the chip itself, so one
// binary stays correct across the derivatives of a family
# define ADIOS_SYS_ADDR_PERSIST_DEVICE_ID_CONFIRM (0x08000000 + ADIOS_SYS_FlashSizeGet() - 2)
# define ADIOS_SYS_ADDR_PERSIST_DEVICE_ID         (0x08000000 + ADIOS_SYS_FlashSizeGet() - 1)
#endif

// location of the Device ID and USB device name
// The bootloader update tool allows to change these values from ADIOS terminal
// NOTE: deliberately left UNDEFINED when there is no bootloader. The block
// lives at (boundary - 0x100), which without a bootloader would resolve to
// 0x07FFFF00 - outside flash entirely - and every reader of it here and in
// adios_midi.c / adios_lcd.c is guarded by #ifdef on this very macro, so
// not defining it makes them all disappear cleanly rather than dereference
// a wild address.
#if !ADIOS_USE_BOOTLOADER
// no info block: nothing persistent to read from, see above
#elif defined(ADIOS_FAMILY_STM32F10x)
# define ADIOS_SYS_ADDR_BSL_INFO_BEGIN    0x08003f00
#elif defined(ADIOS_FAMILY_STM32F4xx) || defined(ADIOS_FAMILY_STM32G0xx)
// last 256 bytes before the (dynamic, per-project) bootloader/app boundary -
// must move together with ADIOS_APP_FLASH_START_ADDR, never a fixed address.
// F4xx: boundary is sector-granular (16K), rounds to a whole number of
// flash sectors - see etc/gen_bsl_boundary.sh. G0xx: page-granular (2K).
# define ADIOS_SYS_ADDR_BSL_INFO_BEGIN    (0x08000000 + ADIOS_APP_FLASH_START_ADDR - 0x100)
#elif defined(ADIOS_FAMILY_LPC17xx)
# define ADIOS_SYS_ADDR_BSL_INFO_BEGIN    0x00003f00
#else
// no warning or error for other families... just don't support these features
#endif

#ifdef ADIOS_SYS_ADDR_BSL_INFO_BEGIN
// NOTE: a change here will mean that:
//   - the bootloader update application (which programs the parameters) has to be re-released
//   - all applications have to be re-released
//   -> better never change the addresses!!!
// New parameters can be added by:
//   - asking TK
//   - searching for an unused offset
//   - adding a confirmation code (parameters will only be taken if confirmation code is 0x42)
//   - adding parameter addresses
# define ADIOS_SYS_ADDR_LCD_PAR_CONFIRM    (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xc0) // 0x42 to confirm value
# define ADIOS_SYS_ADDR_LCD_PAR_TYPE       (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xc1)
# define ADIOS_SYS_ADDR_LCD_PAR_NUM_X      (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xc2)
# define ADIOS_SYS_ADDR_LCD_PAR_NUM_Y      (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xc3)
# define ADIOS_SYS_ADDR_LCD_PAR_WIDTH      (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xc4)
# define ADIOS_SYS_ADDR_LCD_PAR_HEIGHT     (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xc5)

# define ADIOS_SYS_ADDR_DEVICE_ID_CONFIRM  (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xd0) // 0x42 to confirm value
# define ADIOS_SYS_ADDR_DEVICE_ID          (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xd1)
# define ADIOS_SYS_ADDR_FASTBOOT_CONFIRM   (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xd2) // 0x42 to confirm value
# define ADIOS_SYS_ADDR_FASTBOOT           (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xd3)
# define ADIOS_SYS_ADDR_SINGLE_USB_CONFIRM (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xd4) // 0x42 to confirm value
# define ADIOS_SYS_ADDR_SINGLE_USB         (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xd5)
# define ADIOS_SYS_ADDR_ENFORCE_USB_DEVICE_CONFIRM (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xd6) // 0x42 to confirm value
# define ADIOS_SYS_ADDR_ENFORCE_USB_DEVICE (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xd7)
# define ADIOS_SYS_ADDR_SPI_MIDI_CONFIRM   (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xd8) // 0x42 to confirm value
# define ADIOS_SYS_ADDR_SPI_MIDI           (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xd9)


# define ADIOS_SYS_ADDR_USB_DEV_NAME      (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xe0) // all chars != 0x00 and 0x20...0x7f to confirm string
# define ADIOS_SYS_USB_DEV_NAME_LEN       0x20

# define ADIOS_SYS_ADDR_BSL_INFO_END      (ADIOS_SYS_ADDR_BSL_INFO_BEGIN+0xff)
#endif


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////

// *not* compatible to NTP timestamp format, the fraction has mS accuracy
typedef struct {
  u32 seconds;
  u32 fraction_ms;
} adios_sys_time_t;


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 ADIOS_SYS_Init(u32 mode);

extern s32 ADIOS_SYS_Reset(void);

#if defined(ADIOS_FAMILY_STM32G0xx) || defined(ADIOS_FAMILY_STM32F4xx)
// magic value written to a backup register (STM32G0xx: TAMP/RTC: STM32F4xx:
// RTC->BKP0R directly) to request that the bootloader stays resident after
// the next reset (survives NVIC_SystemReset())
#define ADIOS_SYS_BOOTLOADER_MODE_MAGIC 0x424c0001

extern s32 ADIOS_SYS_BootloaderModeRequest(void);
extern s32 ADIOS_SYS_BootloaderModeRequested(void);
extern s32 ADIOS_SYS_AppEntryOverrideSet(u32 addr);
extern u32 ADIOS_SYS_AppEntryOverrideGet(void);
#endif

extern u32 ADIOS_SYS_ChipIDGet(void);
extern u32 ADIOS_SYS_FlashSizeGet(void);
extern u32 ADIOS_SYS_RAMSizeGet(void);
extern s32 ADIOS_SYS_SerialNumberGet(char *str);

extern s32 ADIOS_SYS_TimeSet(adios_sys_time_t t);
extern adios_sys_time_t ADIOS_SYS_TimeGet(void);

extern s32 ADIOS_SYS_DMA_CallbackSet(u8 dma, u8 chn, void *callback);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#endif /* _ADIOS_SYS_H */
