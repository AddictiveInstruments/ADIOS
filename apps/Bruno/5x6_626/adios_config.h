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

// calls to FreeRTOS required? (e.g. to disable tasks on critical sections)
// opt-in, renamed from ADIOS_DONT_USE_FREERTOS
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
