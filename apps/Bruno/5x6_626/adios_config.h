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

// temporary 5x6 define
// 505 config
#define TR5X6_UNIT_SELECT 626
#if TR5X6_UNIT_SELECT==505
#define TR5X6_SYSEX_ACK_UNIT_TYPE      	0x50
#define TR5X6_BANK_NUM      	16
#define TR5X6_SLOT_NUM      	16
// 626 config
#else // TR5X6_UNIT_SELECT==626
#define TR5X6_SYSEX_ACK_UNIT_TYPE      	0x62
#define TR5X6_BANK_NUM      	8
#define TR5X6_SLOT_NUM      	30
#endif    
//#define REDUCED_APP_LCD
#define ADIOS_MIDI_DEFAULT_PORT UART0

// The MIDI activity indicator on this machine's screen. Opt-in: without it
// the engine carries no activity table and no marking at all.
#define ADIOS_USE_MIDI_ACT
#define ADIOS_MIDI_DEBUG_PORT UART0
// disable code modules
//#define ADIOS_DONT_USE_SYS
//#define ADIOS_DONT_USE_IRQ
//#define ADIOS_DONT_USE_SPI
//#define ADIOS_DONT_USE_SPI0
#define TR5X6_DECOD_SOFT_SPI
#ifdef TR5X6_DECOD_SOFT_SPI
#endif
#define ADIOS_DONT_USE_AIN
//#define ADIOS_DONT_USE_LCD
//#define ADIOS_DONT_USE_MIDI
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
#define ADIOS_DONT_USE_TIMER
//#define ADIOS_DONT_USE_STOPWATCH
//#define ADIOS_DONT_USE_DELAY

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
#if defined(ADIOS_FAMILY_STM32F4xx)
# define ADIOS_SYS_DONT_INIT_RTC
//# define ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
// to save some additional memory for STM32F4:
#define ADIOS_UART_NUM 2

// unfortunately!!! Only 584 bytes are missing, maybe the USB driver could be optimized by removing irrelevant code
//# define ADIOS_DONT_USE_UART
//# define ADIOS_DONT_USE_UART_MIDI
#elif defined(ADIOS_FAMILY_STM32G0xx)
#define ADIOS_DONT_USE_USB
#define ADIOS_DONT_USE_USB_MIDI
# define ADIOS_SYS_DONT_INIT_RTC
//# define ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
// to save some additional memory for STM32F4:
#define ADIOS_UART_NUM 2
#if defined(ADIOS_PROCESSOR_STM32G050K8)
#define ADIOS_UART_MIDI_TX_BYPASS_OPTION
#endif

// unfortunately!!! Only 584 bytes are missing, maybe the USB driver could be optimized by removing irrelevant code
//# define ADIOS_DONT_USE_UART
//# define ADIOS_DONT_USE_UART_MIDI

#endif

#define ADIOS_TASK_HOOKS_STACK_SIZE	1000
#define ADIOS_TASK_MIDI_HOOKS_STACK_SIZE	1900
#define TFT_TASK_STACK_SIZE	1900
#define ROM_TASK_STACK_SIZE	1900

#define ADIOS_MINIMAL_STACK_SIZE	384
#define ADIOS_HEAP_SIZE	14*1024

#endif /* _ADIOS_CONFIG_H */
