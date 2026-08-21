/*
 * Header file for UART functions
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _ADIOS_UART_H
#define _ADIOS_UART_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// Tx buffer size (1..256)
#ifndef ADIOS_UART_TX_BUFFER_SIZE
#define ADIOS_UART_TX_BUFFER_SIZE 64
#endif

// Rx buffer size (1..256)
#ifndef ADIOS_UART_RX_BUFFER_SIZE
#define ADIOS_UART_RX_BUFFER_SIZE 64
#endif

// Baudrate of UART first interface
#ifndef ADIOS_UART0_BAUDRATE
#define ADIOS_UART0_BAUDRATE 31250
#endif

// should UART0 Tx pin configured for open drain (default) or push-pull mode?
#ifndef ADIOS_UART0_TX_OD
#define ADIOS_UART0_TX_OD 0
#endif

// Baudrate of UART second interface
#ifndef ADIOS_UART1_BAUDRATE
#define ADIOS_UART1_BAUDRATE 31250
#endif

// should UART1 Tx pin configured for open drain (default) or push-pull mode?
#ifndef ADIOS_UART1_TX_OD
#define ADIOS_UART1_TX_OD 0
#endif

// Baudrate of UART third interface
#ifndef ADIOS_UART2_BAUDRATE
#define ADIOS_UART2_BAUDRATE 31250
#endif

// should UART1 Tx pin configured for open drain (default) or push-pull mode?
#ifndef ADIOS_UART2_TX_OD
#define ADIOS_UART2_TX_OD 0
#endif

// Baudrate of UART fourth interface
#ifndef ADIOS_UART3_BAUDRATE
#define ADIOS_UART3_BAUDRATE 31250
#endif

// should UART3 Tx pin configured for open drain (default) or push-pull mode?
#ifndef ADIOS_UART3_TX_OD
#define ADIOS_UART3_TX_OD 0
#endif

// Baudrate/TX_OD for UART4..UART9 - only meaningful on STM32F4xx chips with
// more than 4 USART/UART peripherals (see adios_uart.c's ADIOS_USE_UARTx
// force-undef for which ports actually exist per processor).
#ifndef ADIOS_UART4_BAUDRATE
#define ADIOS_UART4_BAUDRATE 31250
#endif
#ifndef ADIOS_UART4_TX_OD
#define ADIOS_UART4_TX_OD 0
#endif
#ifndef ADIOS_UART5_BAUDRATE
#define ADIOS_UART5_BAUDRATE 31250
#endif
#ifndef ADIOS_UART5_TX_OD
#define ADIOS_UART5_TX_OD 0
#endif
#ifndef ADIOS_UART6_BAUDRATE
#define ADIOS_UART6_BAUDRATE 31250
#endif
#ifndef ADIOS_UART6_TX_OD
#define ADIOS_UART6_TX_OD 0
#endif
#ifndef ADIOS_UART7_BAUDRATE
#define ADIOS_UART7_BAUDRATE 31250
#endif
#ifndef ADIOS_UART7_TX_OD
#define ADIOS_UART7_TX_OD 0
#endif
#ifndef ADIOS_UART8_BAUDRATE
#define ADIOS_UART8_BAUDRATE 31250
#endif
#ifndef ADIOS_UART8_TX_OD
#define ADIOS_UART8_TX_OD 0
#endif
#ifndef ADIOS_UART9_BAUDRATE
#define ADIOS_UART9_BAUDRATE 31250
#endif
#ifndef ADIOS_UART9_TX_OD
#define ADIOS_UART9_TX_OD 0
#endif

// Interface assignment: 0 = disabled, 1 = MIDI, 2 = COM
#ifndef ADIOS_UART0_ASSIGNMENT
#define ADIOS_UART0_ASSIGNMENT 1
#endif

// Interface assignment: 0 = disabled, 1 = MIDI, 2 = COM
#ifndef ADIOS_UART1_ASSIGNMENT
#define ADIOS_UART1_ASSIGNMENT 1
#endif

// Interface assignment: 0 = disabled, 1 = MIDI, 2 = COM
#ifndef ADIOS_UART2_ASSIGNMENT
#define ADIOS_UART2_ASSIGNMENT 1
#endif

// Interface assignment: 0 = disabled, 1 = MIDI, 2 = COM
#ifndef ADIOS_UART3_ASSIGNMENT
#define ADIOS_UART3_ASSIGNMENT 1
#endif

// Interface assignment: 0 = disabled, 1 = MIDI, 2 = COM (UART4..UART9)
#ifndef ADIOS_UART4_ASSIGNMENT
#define ADIOS_UART4_ASSIGNMENT 1
#endif
#ifndef ADIOS_UART5_ASSIGNMENT
#define ADIOS_UART5_ASSIGNMENT 1
#endif
#ifndef ADIOS_UART6_ASSIGNMENT
#define ADIOS_UART6_ASSIGNMENT 1
#endif
#ifndef ADIOS_UART7_ASSIGNMENT
#define ADIOS_UART7_ASSIGNMENT 1
#endif
#ifndef ADIOS_UART8_ASSIGNMENT
#define ADIOS_UART8_ASSIGNMENT 1
#endif
#ifndef ADIOS_UART9_ASSIGNMENT
#define ADIOS_UART9_ASSIGNMENT 1
#endif



/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 ADIOS_UART_Init(u32 mode);

extern s32 ADIOS_UART_IsAssignedToMIDI(u8 uart);

extern s32 ADIOS_UART_InitPort(u8 uart, u32 baudrate, adios_pin_mode_t tx_pin_mode, u8 is_midi);
extern s32 ADIOS_UART_InitPortDefault(u8 uart);

extern s32 ADIOS_UART_BaudrateSet(u8 uart, u32 baudrate);
extern u32 ADIOS_UART_BaudrateGet(u8 uart);

extern s32 ADIOS_UART_RxBufferFree(u8 uart);
extern s32 ADIOS_UART_RxBufferUsed(u8 uart);
extern s32 ADIOS_UART_RxBufferGet(u8 uart);
extern s32 ADIOS_UART_RxBufferPeek(u8 uart);
extern s32 ADIOS_UART_RxBufferPut(u8 uart, u8 b);
extern s32 ADIOS_UART_TxBufferFree(u8 uart);
extern s32 ADIOS_UART_TxBufferUsed(u8 uart);
extern s32 ADIOS_UART_TxBufferGet(u8 uart);
extern s32 ADIOS_UART_TxBufferPut_NonBlocking(u8 uart, u8 b);
extern s32 ADIOS_UART_TxBufferPut(u8 uart, u8 b);
extern s32 ADIOS_UART_TxBufferPutMore_NonBlocking(u8 uart, u8 *buffer, u16 len);
extern s32 ADIOS_UART_TxBufferPutMore(u8 uart, u8 *buffer, u16 len);

// Line activity moved to the MIDI layer: ADIOS_MIDI_ActGet(port), declared
// in adios_midi.h. It reports EVERY port - USB, DIN and SPI alike -
// because the marking now happens inside the MIDI engine, which all of
// them pass through, instead of in this driver, which only ever saw wires.

/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////



#endif /* _ADIOS_UART_H */
