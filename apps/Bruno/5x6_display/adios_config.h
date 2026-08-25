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
#define ADIOS_APP_VERSION "b0.005 (beta)"

// How long the startup screen stays up before the running display takes
// over, in milliseconds. 0 = no hold.
#define APP_SPLASH_MS 2000

// Which board this firmware is built for. ONE difference, and it is where
// the bank data lives:
//   1 - no EEPROM. Records in the last 4 pages of internal flash, and the
//       current bank number is NOT remembered at all: a page write freezes
//       the core for tens of ms (the G0 executes from the flash it erases)
//       and costs one of the part's 10 000 erase cycles.
//   2 - an AT24C64 on I2C2, PB13/PB14. Records and bank number both live
//       there: byte addressable, no erase, a million cycles.
// The SysEx device ID never moves - last two bytes of internal flash in
// BOTH revisions, because that is where the bootloader looks.
#define APP_HARD_REV 1	// the Makefile reads this line too: it sizes
				// ADIOS_USERDATA_PAGES (4 pages of records, or 1
				// system page) and with it the linker reservation

#if APP_HARD_REV == 2
// The AT24C64's bus - the one I2C pair this board has left: I2C1's three pin
// options all land on the ROM bus or a MIDI line, and I2C2's default
// PB10/PB11 are the TFT's SPI. So I2C2 on PB13/PB14 (AF6), the only free
// pair. ADIOS numbering starts at 0: ADIOS_I2C1 IS the I2C2 peripheral, and
// core/main.c initialises it by itself.
#define ADIOS_USE_I2C1
#define ADIOS_I2C1_SCL_PORT	GPIOB
#define ADIOS_I2C1_SCL_PIN	LL_GPIO_PIN_13
#define ADIOS_I2C1_SDA_PORT	GPIOB
#define ADIOS_I2C1_SDA_PIN	LL_GPIO_PIN_14
#endif

// ONE firmware, BOTH machines. Which one this board is bolted into comes
// from the flash magic at boot - see APP_Init and tr5x6_unit_t. There is
// no build-time unit switch any more.

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

// This application is a FreeRTOS user in its own right: it creates the
// SPI mutex and tasks of its own (settings menu, TFT refresh, ROM check).
// Declared, so a bare-metal build of this project is refused at compile
// time instead of failing on the bench.
#define ADIOS_APP_USE_FREERTOS


// enable BSL enhancements in ADIOS SysEx parser
//#define ADIOS_MIDI_BSL_ENHANCEMENTS 0

// exclude default BSL image from ADIOS
//#define ADIOS_DONT_INCLUDE_BSL

// to save memory on STM32 build:
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
// the outside world (and ADIOS Studio) talks to - so it lives in the relayed
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

#define ADIOS_TASK_HOOKS_STACK_SIZE	1000
#define ADIOS_TASK_MIDI_HOOKS_STACK_SIZE	1900
#define TFT_TASK_STACK_SIZE	1900
#define ROM_TASK_STACK_SIZE	2100

#define ADIOS_MINIMAL_STACK_SIZE	384
#define ADIOS_HEAP_SIZE	14*1024

#endif /* _ADIOS_CONFIG_H */
