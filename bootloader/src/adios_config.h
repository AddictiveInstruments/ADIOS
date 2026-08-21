/*
 * Local ADIOS configuration file
 *
 * this file allows to disable (or re-configure) default functions of ADIOS
 * available switches are listed in $ADIOS_PATH/modules/adios/ADIOS_CONFIG.txt
 *
 */

#ifndef _ADIOS_CONFIG_H
#define _ADIOS_CONFIG_H

// How this program identifies itself to a host (see adios_midi.h)
#define ADIOS_APP_NAME1 "ADIOS Bootloader"
#define ADIOS_APP_NAME2 "(c) 2026 B.Dupeyron"
#define ADIOS_APP_VERSION "v1.000"

// disable code modules
// adios_sys.c/adios_irq.c: indispensable, always compiled, no toggle.
// adios_utils.c (delay/timer/stopwatch/sof): DELAY is indispensable (no
// toggle); TIMER/STOPWATCH/SOF are opt-in. STOPWATCH is used by main.c/
// bsl_sysex.c for the flash-erase/write timing measurement printed over SysEx.
#define ADIOS_USE_STOPWATCH

#define ADIOS_DONT_USE_AIN
#define ADIOS_DONT_USE_LCD

// (MIDI core is always compiled - not optional, see adios_midi.c; only the
// transports below are opt-in)
#define ADIOS_DONT_USE_USB_HOST
#define ADIOS_DONT_USE_USB_HS_HOST
//#define ADIOS_USE_USB_COM

//#define ADIOS_USE_I2S
// The board module is gone (2026-08-11): it carried the MBHP boards frozen
// connectors (J5/J10/J15/J28/LED/DAC). All that survived is its status LED,
// now the sign-of-life pin - same default as before, GPIOA pin 12.
#define ADIOS_USE_SOL

// calls to FreeRTOS required? (e.g. to disable tasks on critical sections)
// ADIOS_APP_USE_FREERTOS is opt-in (renamed from ADIOS_DONT_USE_FREERTOS),
// numeric (0/1) with a RAM/FLASH-tiered default in adios_sys.h - MUST be
// forced to 0 explicitly here rather than left to that default: on a
// non-small-tier chip (e.g. this bootloader built for G070CB) the tier
// default would be 1, and adios_sys.c would then try to #include
// <FreeRTOS.h> - a header this bootloader's own Makefile never puts on the
// include path (its main.c is a bare super-loop, never went through the
// FreeRTOS-based programming model to begin with, on ANY chip it targets).
#define ADIOS_APP_USE_FREERTOS 0

#if defined(ADIOS_FAMILY_STM32F4xx)
#if 0
// Following settings allow to customize the USB device descriptor
#define ADIOS_USB_VENDOR_ID    0x16c0        // sponsored by voti.nl! see http://www.voti.nl/pids
#define ADIOS_USB_VENDOR_STR   "midibox.org" // you will see this in the USB device description
#define ADIOS_USB_PRODUCT_STR  "ADIOS Bootloader"  // you will see this in the MIDI device list
#define ADIOS_USB_PRODUCT_ID   0x03fe        // ==1022; 1020-1029 reserved for T.Klose, 1000 - 1009 free for lab use... 0x3fe is required if the GM5 driver should be used
#define ADIOS_USB_VERSION_ID   0x1010        // v1.010


// 1 to stay compatible to USB MIDI spec, 0 as workaround for some windows versions...
#define ADIOS_USB_MIDI_USE_AC_INTERFACE 1

// allowed number of USB MIDI ports: 1..8
#define ADIOS_USB_MIDI_NUM_PORTS 1

// buffer size (should be at least >= ADIOS_USB_MIDI_DATA_*_SIZE/4)
#define ADIOS_USB_MIDI_RX_BUFFER_SIZE  512 // packages
#define ADIOS_USB_MIDI_TX_BUFFER_SIZE  512 // packages

// size of IN/OUT pipe
#define ADIOS_USB_MIDI_DATA_IN_SIZE           64
#define ADIOS_USB_MIDI_DATA_OUT_SIZE          64

// unfortunately!!! Only 584 bytes are missing, maybe the USB driver could be optimized by removing irrelevant code
// (opt-in world: simply not defining ADIOS_USE_UART0/ADIOS_USE_DIN_MIDI
// keeps the UART transport out - the old DONT_USE_UART* opt-outs are gone)

#else
// No transport is chosen here, deliberately - see the check further down,
// where the generated header lands. Which connector this build must talk on
// is a fact of the BOARD, so it arrives from the project's BSL_RELAY block
// like every other board fact, and its absence stops the build instead of
// falling back to a family default. That fallback is exactly what let this
// bootloader and its update tool end up on DIFFERENT connectors, each
// silently right in its own file: the tool then answered nobody.
#endif
#endif
// enable BSL enhancements in ADIOS SysEx parser
#define ADIOS_MIDI_BSL_ENHANCEMENTS 1

// announce ourselves as the bootloader on the core-type query (0x0b) - a
// new-generation marker too: MIOS Studio uses a positive answer here to know
// the entry-override SysEx command is understood (legacy BSLs DISACK 0x0b)
#define ADIOS_MIDI_CORE_TYPE_STR "BSL"

// exclude default BSL image from ADIOS
#define ADIOS_DONT_INCLUDE_BSL

// The only strings this build formats are its answers to the host's identity
// queries, and between them they use "%d" and "%08x". printf-stdarg.c serves
// those from a parser a quarter of the size when this is defined - worth it
// here and nowhere else, because it is THIS binary's size that decides where
// the application starts (the boundary rounds up to an erase unit, a whole
// 16K sector on some families). See that file for what the reduced parser
// does and does not understand.
#define BSL_USE_REDUCED_SPRINTF

// to save memory on STM32 build:
#if defined(ADIOS_FAMILY_STM32F10x) || defined(ADIOS_FAMILY_STM32F4xx)
# define ADIOS_SYS_DONT_INIT_RTC
# define ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
#endif

#if defined(ADIOS_FAMILY_STM32F4xx)

// reserved flash size for the bootloader itself.
// auto-computed by etc/gen_bsl_boundary.sh from the bootloader's own compiled
// size (see adios_bsl_boundary.h, generated in this same directory), rounded
// up to a whole number of 16K flash sectors (F4 erases at sector granularity,
// unlike G0xx's uniform 2K pages). There is deliberately no fallback -
// see below.
#if __has_include("adios_bsl_boundary.h")
#include "adios_bsl_boundary.h"
#endif
#ifndef ADIOS_MIDI_DEFAULT_PORT
# error "No board MIDI wiring reached this bootloader build: add a BSL_RELAY_BEGIN/END block to your project's adios_config.h declaring the port it must talk on (see apps/Bruno/f407_test/adios_config.h)."
#endif
#ifndef ADIOS_APP_FLASH_START_ADDR
// NO fallback, deliberately. There used to be one - 0x4000, "only applies
// before the first run of that script" - and it cost an afternoon: a
// bootloader that has outgrown one sector, built believing the application
// starts at 0x4000, ERASES ITS OWN TAIL the moment an upload begins - that
// is where it thinks the application head lives, and erasing it is the
// first thing it does. It then locks up on the next boot executing erased
// flash, with nothing to explain itself. Failing to build is far better.
# error "adios_bsl_boundary.h was not included: this bootloader does not know where the application starts. That header is generated by etc/gen_bsl_boundary.sh, which a project build runs - build through the project, not by hand."
#endif

#endif


#if defined(ADIOS_FAMILY_STM32G0xx)
// NO MIDI wiring here on purpose. Which port this bootloader talks on, its
// TX polarity and its pin drive mode belong to the BOARD, not to the
// bootloader: it shares one physical connector with its application. The
// project declares them once in its own adios_config.h, inside a
// BSL_RELAY_BEGIN/END block, and etc/gen_bsl_boundary.sh copies them into
// the generated header included below (see apps/Bruno/5x6_505 for an
// example). This file used to hardcode the 5x6_505's arrangement, which
// made it describe exactly one instrument - and silently produced a mute
// bootloader on any board wired differently, or one asking for a peripheral
// its chip doesn't have.
#define ADIOS_DONT_USE_USB
#define ADIOS_DONT_USE_USB_MIDI
# define ADIOS_SYS_DONT_INIT_RTC
// debug-message support stripped (production default): the vsprintf
// machinery alone pushes the new-generation BSL over its 10240-byte page
// (10456 bytes with it), bouncing the boundary from 0x2800 to 0x3000 -
// errors still reach MIOS Studio as DISACK codes.
// Commenting this line out is therefore also how the boundary-MIGRATION
// path gets exercised on real hardware (bigger BSL -> 0x3000): it tests the
// updater's old-info-block scan, its relocation to the new boundary, and
// the app re-link. Always come back to the stripped state afterwards.
# define ADIOS_MIDI_DISABLE_DEBUG_MESSAGE

// reserved flash size for the bootloader itself.
// auto-computed by etc/gen_bsl_boundary.sh from the bootloader's own compiled
// size (see adios_bsl_boundary.h, generated in this same directory) - the
// 0x2800 fallback below only applies before the first run of that script.
#if __has_include("adios_bsl_boundary.h")
#include "adios_bsl_boundary.h"
#endif
// the relayed block above is what gives this bootloader a MIDI transport at
// all - without it the build would succeed and produce a core that never
// answers, the exact failure this whole mechanism exists to prevent
#ifndef ADIOS_USE_DIN_MIDI
# error "No board MIDI wiring reached this bootloader build: add a BSL_RELAY_BEGIN/END block to your project's adios_config.h declaring the port it must talk on (see apps/Bruno/5x6_505/adios_config.h)."
#endif

#ifndef ADIOS_APP_FLASH_START_ADDR
#define ADIOS_APP_FLASH_START_ADDR 0x2800
#endif

#endif



// AND IT DEBUGS WHERE IT TALKS, unless the project said otherwise itself.
// The transport comes from the relay block; a debug port pointing anywhere
// else would drag a second driver into a binary that has room for none, and
// would speak into a socket this board may not even have wired. Placed after
// both family branches, so it sees whatever the relay declared - a project
// that relays its own debug port keeps it. The application is free to debug
// somewhere else entirely: it declares that OUTSIDE the relay block, which is
// precisely why it cannot reach this file.
#if !defined(ADIOS_MIDI_DEBUG_PORT) && defined(ADIOS_MIDI_DEFAULT_PORT)
# define ADIOS_MIDI_DEBUG_PORT ADIOS_MIDI_DEFAULT_PORT
#endif

#endif /* _ADIOS_CONFIG_H */
