// $Id: mios32_config.h 194 2008-12-18 01:47:21Z tk $
/*
 * Local MIOS32 configuration file
 *
 * this file allows to disable (or re-configure) default functions of MIOS32
 * available switches are listed in $MIOS32_PATH/modules/mios32/MIOS32_CONFIG.txt
 *
 */

#ifndef _MIOS32_CONFIG_H
#define _MIOS32_CONFIG_H

// How this program identifies itself to a host (see mios32_midi.h)
#define MIOS32_APP_NAME1 "ADIOS Bootloader"
#define MIOS32_APP_NAME2 "(c) 2026 B.Dupeyron"
#define MIOS32_APP_VERSION "v1.000"

// disable code modules
// mios32_sys.c/mios32_irq.c: indispensable, always compiled, no toggle.
// mios32_utils.c (delay/timer/stopwatch/sof): DELAY is indispensable (no
// toggle); TIMER/STOPWATCH/SOF are opt-in. STOPWATCH is used by main.c/
// bsl_sysex.c for the flash-erase/write timing measurement printed over SysEx.
#define MIOS32_USE_STOPWATCH

#define MIOS32_DONT_USE_AIN
#define MIOS32_DONT_USE_LCD

// (MIDI core is always compiled - not optional, see mios32_midi.c; only the
// transports below are opt-in)
#define MIOS32_DONT_USE_USB
#define MIOS32_DONT_USE_USB_HOST
#define MIOS32_DONT_USE_USB_HS_HOST
#define MIOS32_DONT_USE_USB_MIDI
//#define MIOS32_USE_USB_COM

//#define MIOS32_USE_I2S
// The board module is gone (2026-08-11): it carried the MBHP boards frozen
// connectors (J5/J10/J15/J28/LED/DAC). All that survived is its status LED,
// now the sign-of-life pin - same default as before, GPIOA pin 12.
#define MIOS32_USE_SOL

// calls to FreeRTOS required? (e.g. to disable tasks on critical sections)
// MIOS32_APP_USE_FREERTOS is opt-in (renamed from MIOS32_DONT_USE_FREERTOS),
// numeric (0/1) with a RAM/FLASH-tiered default in mios32_sys.h - MUST be
// forced to 0 explicitly here rather than left to that default: on a
// non-small-tier chip (e.g. this bootloader built for G070CB) the tier
// default would be 1, and mios32_sys.c would then try to #include
// <FreeRTOS.h> - a header this bootloader's own Makefile never puts on the
// include path (its main.c is a bare super-loop, never went through the
// FreeRTOS-based programming model to begin with, on ANY chip it targets).
#define MIOS32_APP_USE_FREERTOS 0

#if defined(MIOS32_FAMILY_STM32F4xx)
#if 0
// Following settings allow to customize the USB device descriptor
#define MIOS32_USB_VENDOR_ID    0x16c0        // sponsored by voti.nl! see http://www.voti.nl/pids
#define MIOS32_USB_VENDOR_STR   "midibox.org" // you will see this in the USB device description
#define MIOS32_USB_PRODUCT_STR  "MIOS32 Bootloader"  // you will see this in the MIDI device list
#define MIOS32_USB_PRODUCT_ID   0x03fe        // ==1022; 1020-1029 reserved for T.Klose, 1000 - 1009 free for lab use... 0x3fe is required if the GM5 driver should be used
#define MIOS32_USB_VERSION_ID   0x1010        // v1.010


// 1 to stay compatible to USB MIDI spec, 0 as workaround for some windows versions...
#define MIOS32_USB_MIDI_USE_AC_INTERFACE 1

// allowed number of USB MIDI ports: 1..8
#define MIOS32_USB_MIDI_NUM_PORTS 1

// buffer size (should be at least >= MIOS32_USB_MIDI_DATA_*_SIZE/4)
#define MIOS32_USB_MIDI_RX_BUFFER_SIZE  512 // packages
#define MIOS32_USB_MIDI_TX_BUFFER_SIZE  512 // packages

// size of IN/OUT pipe
#define MIOS32_USB_MIDI_DATA_IN_SIZE           64
#define MIOS32_USB_MIDI_DATA_OUT_SIZE          64

// unfortunately!!! Only 584 bytes are missing, maybe the USB driver could be optimized by removing irrelevant code
// (opt-in world: simply not defining MIOS32_USE_UART0/MIOS32_USE_DIN_MIDI
// keeps the UART transport out - the old DONT_USE_UART* opt-outs are gone)

#else
// the default MIDI port for MIDI output
#define MIOS32_MIDI_DEFAULT_PORT DIN0
// the default MIDI port for debugging output via MIOS32_MIDI_SendDebugMessage
#define MIOS32_MIDI_DEBUG_PORT DIN0

#define MIOS32_USE_UART0
#define MIOS32_USE_DIN_MIDI
#define MIOS32_DONT_USE_USB
#define MIOS32_DONT_USE_USB_MIDI
#endif
#endif
// enable BSL enhancements in MIOS32 SysEx parser
#define MIOS32_MIDI_BSL_ENHANCEMENTS 1

// announce ourselves as the bootloader on the core-type query (0x0b) - a
// new-generation marker too: MIOS Studio uses a positive answer here to know
// the entry-override SysEx command is understood (legacy BSLs DISACK 0x0b)
#define MIOS32_MIDI_CORE_TYPE_STR "BSL"

// exclude default BSL image from MIOS32
#define MIOS32_DONT_INCLUDE_BSL

// to save memory on STM32 build:
#if defined(MIOS32_FAMILY_STM32F10x) || defined(MIOS32_FAMILY_STM32F4xx)
# define MIOS32_SYS_DONT_INIT_RTC
# define MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
#endif

#if defined(MIOS32_FAMILY_STM32F4xx)

// reserved flash size for the bootloader itself.
// auto-computed by etc/gen_bsl_boundary.sh from the bootloader's own compiled
// size (see mios32_bsl_boundary.h, generated in this same directory), rounded
// up to a whole number of 16K flash sectors (F4 erases at sector granularity,
// unlike G0xx's uniform 2K pages) - the 0x4000 fallback below (one sector)
// only applies before the first run of that script.
#if __has_include("mios32_bsl_boundary.h")
#include "mios32_bsl_boundary.h"
#endif
#ifndef MIOS32_APP_FLASH_START_ADDR
#define MIOS32_APP_FLASH_START_ADDR 0x4000
#endif

#endif


#if defined(MIOS32_FAMILY_STM32G0xx)
// NO MIDI wiring here on purpose. Which port this bootloader talks on, its
// TX polarity and its pin drive mode belong to the BOARD, not to the
// bootloader: it shares one physical connector with its application. The
// project declares them once in its own mios32_config.h, inside a
// BSL_RELAY_BEGIN/END block, and etc/gen_bsl_boundary.sh copies them into
// the generated header included below (see apps/Bruno/5x6_505 for an
// example). This file used to hardcode the 5x6_505's arrangement, which
// made it describe exactly one instrument - and silently produced a mute
// bootloader on any board wired differently, or one asking for a peripheral
// its chip doesn't have.
#define MIOS32_DONT_USE_USB
#define MIOS32_DONT_USE_USB_MIDI
# define MIOS32_SYS_DONT_INIT_RTC
// debug-message support stripped (production default): the vsprintf
// machinery alone pushes the new-generation BSL over its 10240-byte page
// (10456 bytes with it), bouncing the boundary from 0x2800 to 0x3000 -
// errors still reach MIOS Studio as DISACK codes.
// Commenting this line out is therefore also how the boundary-MIGRATION
// path gets exercised on real hardware (bigger BSL -> 0x3000): it tests the
// updater's old-info-block scan, its relocation to the new boundary, and
// the app re-link. Always come back to the stripped state afterwards.
# define MIOS32_MIDI_DISABLE_DEBUG_MESSAGE

// reserved flash size for the bootloader itself.
// auto-computed by etc/gen_bsl_boundary.sh from the bootloader's own compiled
// size (see mios32_bsl_boundary.h, generated in this same directory) - the
// 0x2800 fallback below only applies before the first run of that script.
#if __has_include("mios32_bsl_boundary.h")
#include "mios32_bsl_boundary.h"
#endif
// the relayed block above is what gives this bootloader a MIDI transport at
// all - without it the build would succeed and produce a core that never
// answers, the exact failure this whole mechanism exists to prevent
#ifndef MIOS32_USE_DIN_MIDI
# error "No board MIDI wiring reached this bootloader build: add a BSL_RELAY_BEGIN/END block to your project's mios32_config.h declaring the port it must talk on (see apps/Bruno/5x6_505/mios32_config.h)."
#endif

#ifndef MIOS32_APP_FLASH_START_ADDR
#define MIOS32_APP_FLASH_START_ADDR 0x2800
#endif

#endif


#endif /* _MIOS32_CONFIG_H */
