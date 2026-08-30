/*
 * Local ADIOS configuration for terminal_test.
 *
 * Available switches are listed in $ADIOS_PATH/modules/adios/ADIOS_CONFIG.txt
 */

#ifndef _ADIOS_CONFIG_H
#define _ADIOS_CONFIG_H

// How this program identifies itself to a host (Ping in ADIOS Studio).
#define ADIOS_APP_NAME1   "Terminal Test"
#define ADIOS_APP_NAME2   "debug console demo"
#define ADIOS_APP_VERSION "v1.000"

// Target chip - read by include/makefile/app_config.mk for the whole build.
#define ADIOS_PROCESSOR STM32G070CB
#define ADIOS_USE_DYNAMIC_BSL_BOUNDARY 1

// sign-of-life blink (default LED pin PA12) + the millisecond timestamp used
// by the blink and the 'uptime' command
#define ADIOS_USE_STOPWATCH
#define ADIOS_USE_TIMESTAMP
#define ADIOS_USE_SOL

// nothing else needed: no analog in, no LCD, no USB on this chip
#define ADIOS_DONT_USE_AIN
#define ADIOS_DONT_USE_LCD
#define ADIOS_DONT_USE_USB
#define ADIOS_DONT_USE_USB_MIDI
#define ADIOS_DONT_USE_USB_HOST
#define ADIOS_DONT_USE_USB_HS_HOST
#define ADIOS_SYS_DONT_INIT_RTC

// BSL_RELAY_BEGIN - the MIDI connector this board is reached on (copied into
// the bootloader/updater builds). Matches the 5x6 wiring: UART2 = DIN2, which
// is where the FastLane is connected. Debug/console text goes out the same port.
#define ADIOS_USE_DIN_MIDI
#define ADIOS_USE_UART0
#define ADIOS_USE_UART2
#define ADIOS_MIDI_DEFAULT_PORT DIN2
#define ADIOS_MIDI_DEBUG_PORT DIN2
// This board's UART2 (USART3) TX drives an external 3V3->5V transistor stage
// that INVERTS the signal and must be actively driven. Two settings the board
// needs and the driver does not assume: normal polarity by default, and TX_OD
// defaulting to 1 (open drain) for every port except UART0. Omit either and
// DIN2 puts garbage on the wire (a floating open-drain pin reads as a stream
// of 0x00 -> the host shows a flood of pitch bend via running status).
#define ADIOS_UART2_TX_INVERTED
#define ADIOS_UART2_TX_OD 0
// BSL_RELAY_END

#ifndef ADIOS_HEAP_SIZE
#define ADIOS_HEAP_SIZE 4*1024
#endif

#endif /* _ADIOS_CONFIG_H */
