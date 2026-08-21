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
#define ADIOS_APP_NAME1 "5x6 Display/ROM"
#define ADIOS_APP_NAME2 "(C) 2024 B.Dupeyron"
#define ADIOS_APP_VERSION "v1.000"

// How long the startup screen stays up before the running display takes
// over, in milliseconds. 0 = no hold.
#define APP_SPLASH_MS 2000

// temporary 5x6 define
// 505 config
#define TR5X6_UNIT_SELECT 505

#if TR5X6_UNIT_SELECT==505
#define TR5X6_SYSEX_ACK_UNIT_TYPE      	0x50
#define TR5X6_BANK_NUM      	16
#define TR5X6_SLOT_NUM      	16
#define TR5X6_VERSION      		"b0 . 004 (beta)"
// 626 config
#else // TR5X6_UNIT_SELECT==626
#define TR5X6_SYSEX_ACK_UNIT_TYPE      	0x62
#define TR5X6_BANK_NUM      	8
#define TR5X6_SLOT_NUM      	30
#define TR5X6_VERSION      		"b0 . 002 (beta)"
#endif
//#define REDUCED_APP_LCD
// (ADIOS_MIDI_DEFAULT_PORT / _DEBUG_PORT are set in the BSL_RELAY block of
// the UART section below, with the rest of this board's MIDI wiring)
// disable code modules
// adios_spi.c - SPI0 drives the ROM (tr5x6_rom.c), SPI1 drives the TFT
// (5x6_tft.c). Both use ADIOS_SPI_CS_PinSet(spi, value) for chip select.
#define ADIOS_USE_SPI0
#define ADIOS_USE_SPI1
#define TR5X6_DECOD_SOFT_SPI
#ifdef TR5X6_DECOD_SOFT_SPI
#endif
#define ADIOS_DONT_USE_AIN
//#define ADIOS_DONT_USE_LCD
// (the MIDI core itself is always compiled - not optional, see adios_midi.c;
// only the transports are opt-in: ADIOS_USE_DIN_MIDI etc, further below)
//#define ADIOS_DONT_USE_USB
#define ADIOS_DONT_USE_USB_HOST
#define ADIOS_DONT_USE_USB_HS_HOST
//#define ADIOS_DONT_USE_USB_MIDI
//#define ADIOS_USE_USB_COM

//#define ADIOS_USE_I2S
// The board module is gone (2026-08-11): it carried the MBHP boards frozen
// connectors (J5/J10/J15/J28/LED/DAC). All that survived is its status LED,
// now the sign-of-life pin - same default as before, GPIOA pin 12.
#define ADIOS_USE_SOL
// adios_utils.c - delay is indispensable (no toggle); timer/stopwatch/sof
// are opt-in:
#define ADIOS_USE_STOPWATCH
// ADIOS_USE_TIMER / ADIOS_USE_SOF left undefined - unused by this project

// calls to FreeRTOS required? (e.g. to disable tasks on critical sections)
// opt-in, renamed from ADIOS_DONT_USE_FREERTOS - left undefined here since
// this project uses FreeRTOS (G070CB has plenty of RAM/FLASH margin for it).
//#define ADIOS_APP_USE_FREERTOS

#if 0
// Following settings allow to customize the USB device descriptor
#define ADIOS_USB_VENDOR_ID    0x16c0        // sponsored by voti.nl! see http://www.voti.nl/pids
#define ADIOS_USB_VENDOR_STR   "midibox.org" // you will see this in the USB device description
#define ADIOS_USB_PRODUCT_STR  "app skeleton"  // you will see this in the MIDI device list
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
#endif

// enable BSL enhancements in ADIOS SysEx parser
//#define ADIOS_MIDI_BSL_ENHANCEMENTS 0

// exclude default BSL image from ADIOS
//#define ADIOS_DONT_INCLUDE_BSL

// to save memory on STM32 build:
#if defined(ADIOS_FAMILY_STM32F4xx)
# define ADIOS_SYS_DONT_INIT_RTC
//# define ADIOS_MIDI_DISABLE_DEBUG_MESSAGE

#define ADIOS_USE_UART0
#define ADIOS_USE_UART1
#define ADIOS_USE_DIN_MIDI
#elif defined(ADIOS_FAMILY_STM32G0xx)
#define ADIOS_DONT_USE_USB
#define ADIOS_DONT_USE_USB_MIDI
# define ADIOS_SYS_DONT_INIT_RTC
//# define ADIOS_MIDI_DISABLE_DEBUG_MESSAGE

// This board's two MIDI links (ADIOS_UARTn is USART(n+1) on G0xx):
//   UART0 = USART1, PB7 in  - the TR-505 host's MIDI output (RX only; its
//           TX pin PB6 is not wired to anything)          -> port DIN0
//   UART2 = USART3, PB8 out - the instrument's physical MIDI OUT, where
//           everything is merged, and PB9 in, wired in parallel with the
//           host's own MIDI input                          -> port DIN2
// Only the second one is the bootloader's business - it is the connector
// the outside world (and MIOS Studio) talks to - so it lives in the relayed
// block below; the host link is this application's alone.
#define ADIOS_USE_UART0

// BSL_RELAY_BEGIN - copied verbatim into the bootloader and updater builds
// by etc/gen_bsl_boundary.sh. They must talk on the same physical connector
// as this application, so its wiring is declared here once and shared,
// rather than duplicated (and drifting) in bootloader/src/adios_config.h.
#define ADIOS_USE_DIN_MIDI
#define ADIOS_USE_UART2
#define ADIOS_MIDI_DEFAULT_PORT DIN2
#define ADIOS_MIDI_DEBUG_PORT DIN2
// UART2 (USART3) TX runs through an external 3V3->5V transistor stage that
// inverts the signal; every port defaults to normal polarity in the driver.
#define ADIOS_UART2_TX_INVERTED
// ...and that stage must be DRIVEN, so push-pull. adios_uart.h defaults
// TX_OD to 0 for UART0 but to 1 (open drain) for every other port - an
// MBHP-core board convention, not a chip fact. It bit us the day UART
// numbering was realigned and this board's output moved from UART0 to
// UART2: the pin silently became open drain, nothing pulled it high, and
// the instrument went mute while the firmware kept queueing bytes happily.
#define ADIOS_UART2_TX_OD 0
// BSL_RELAY_END

// The MIDI activity indicator on this machine's screen. OUTSIDE the relay
// block on purpose: the bootloader has no screen and no use for it, and
// anything inside that block is copied verbatim into its build.
#define ADIOS_USE_MIDI_ACT

#endif

#define ADIOS_TASK_HOOKS_STACK_SIZE	1000
#define ADIOS_TASK_MIDI_HOOKS_STACK_SIZE	1900
#define TFT_TASK_STACK_SIZE	1900
#define ROM_TASK_STACK_SIZE	2100

#define ADIOS_MINIMAL_STACK_SIZE	384
#define ADIOS_HEAP_SIZE	14*1024

#endif /* _ADIOS_CONFIG_H */
