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
#define ADIOS_APP_NAME1 "ADIOS BSL Updater"
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
// Same two opt-outs as the bootloader, and no more: this tool REPLACES that
// bootloader, so it has to come up on the same connector - which means it
// must be able to compile the same transports. Disabling USB outright here
// (as this file used to) made that impossible on a USB-wired board, whatever
// the project relayed.
#define ADIOS_DONT_USE_USB_HOST
#define ADIOS_DONT_USE_USB_HS_HOST
//#define ADIOS_USE_USB_COM

//#define ADIOS_USE_I2S
// The board module is gone (2026-08-11): it carried the MBHP boards frozen
// connectors (J5/J10/J15/J28/LED/DAC). All that survived is its status LED,
// now the sign-of-life pin - same default as before, GPIOA pin 12.
#define ADIOS_USE_SOL

// This bootloader is bare-metal on EVERY chip it targets: its main.c is a
// plain super-loop, its Makefile never puts FreeRTOS on the include path.
// Declaring the opt-out here keeps adios_sys.c from reaching for
// <FreeRTOS.h> on chips where the scheduler would otherwise be on (this
// build does not go through core.mk, so it must say so itself).
#define ADIOS_CORE_DONT_USE_FREERTOS

#if defined(ADIOS_FAMILY_STM32F4xx)
// No transport is chosen here, deliberately - see the check further down,
// where the generated header lands. This tool has to answer on the same
// connector as the bootloader it installs, so it takes the port from the
// same place that bootloader does: the project's BSL_RELAY block. A family
// default here is precisely how the two came to disagree - this file said
// DIN, the bootloader said USB, each perfectly consistent on its own.
#endif
// enable BSL enhancements in ADIOS SysEx parser
#define ADIOS_MIDI_BSL_ENHANCEMENTS 1

// announce ourselves as the BSL-update tool on the core-type query (0x0b):
// ADIOS Studio only ever allows a hex targeting the protected bootloader
// range to be sent to a core answering "UPDATER" here
#define ADIOS_MIDI_CORE_TYPE_STR "UPDATER"

// exclude default BSL image from ADIOS
#define ADIOS_DONT_INCLUDE_BSL

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
// unlike G0xx's uniform 2K pages) - the 0x4000 fallback below (one sector)
// only applies before the first run of that script.
#if __has_include("adios_bsl_boundary.h")
#include "adios_bsl_boundary.h"
#endif
#ifndef ADIOS_MIDI_DEFAULT_PORT
# error "No board MIDI wiring reached this updater build: add a BSL_RELAY_BEGIN/END block to your project's adios_config.h declaring the port it must talk on (see apps/Bruno/f407_test/adios_config.h)."
#endif
#ifndef ADIOS_APP_FLASH_START_ADDR
#define ADIOS_APP_FLASH_START_ADDR 0x4000
#endif

#endif


#if defined(ADIOS_FAMILY_STM32G0xx)
// NO MIDI wiring here, same rule as the bootloader this tool installs (see
// ../src/adios_config.h): the board's port, TX polarity and pin drive mode
// arrive from the project's BSL_RELAY block through the generated header
// included below. The updater must obviously talk on the same connector as
// the bootloader it replaces.
#define ADIOS_DONT_USE_USB
#define ADIOS_DONT_USE_USB_MIDI
# define ADIOS_SYS_DONT_INIT_RTC
// debug-message support stripped: the updater is entirely driven by MIOS
// Studio (errors travel as DISACK codes), and its 10K window on 32K parts
// has no room for the vsprintf machinery
# define ADIOS_MIDI_DISABLE_DEBUG_MESSAGE

// reserved flash size for the bootloader itself.
// auto-computed by etc/gen_bsl_boundary.sh from the bootloader's own compiled
// size (see adios_bsl_boundary.h, generated in this same directory) - the
// 0x2800 fallback below only applies before the first run of that script.
#if __has_include("adios_bsl_boundary.h")
#include "adios_bsl_boundary.h"
#endif
// same guard as the bootloader: without the project's relayed wiring this
// tool would build fine and never answer ADIOS Studio
#ifndef ADIOS_USE_DIN_MIDI
# error "No board MIDI wiring reached this updater build: add a BSL_RELAY_BEGIN/END block to your project's adios_config.h declaring the port it must talk on (see apps/Bruno/5x6_505/adios_config.h)."
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
