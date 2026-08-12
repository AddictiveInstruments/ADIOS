// $Id: mios32_uart.h 2403 2016-08-15 17:47:50Z tk $
/*
 * Header file for UART functions
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _MIOS32_UART_H
#define _MIOS32_UART_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// Tx buffer size (1..256)
#ifndef MIOS32_UART_TX_BUFFER_SIZE
#define MIOS32_UART_TX_BUFFER_SIZE 64
#endif

// Rx buffer size (1..256)
#ifndef MIOS32_UART_RX_BUFFER_SIZE
#define MIOS32_UART_RX_BUFFER_SIZE 64
#endif

// Baudrate of UART first interface
#ifndef MIOS32_UART0_BAUDRATE
#define MIOS32_UART0_BAUDRATE 31250
#endif

// should UART0 Tx pin configured for open drain (default) or push-pull mode?
#ifndef MIOS32_UART0_TX_OD
#define MIOS32_UART0_TX_OD 0
#endif

// Baudrate of UART second interface
#ifndef MIOS32_UART1_BAUDRATE
#define MIOS32_UART1_BAUDRATE 31250
#endif

// should UART1 Tx pin configured for open drain (default) or push-pull mode?
#ifndef MIOS32_UART1_TX_OD
#define MIOS32_UART1_TX_OD 1
#endif

// Baudrate of UART third interface
#ifndef MIOS32_UART2_BAUDRATE
#define MIOS32_UART2_BAUDRATE 31250
#endif

// should UART1 Tx pin configured for open drain (default) or push-pull mode?
#ifndef MIOS32_UART2_TX_OD
#define MIOS32_UART2_TX_OD 1
#endif

// Baudrate of UART fourth interface
#ifndef MIOS32_UART3_BAUDRATE
#define MIOS32_UART3_BAUDRATE 31250
#endif

// should UART3 Tx pin configured for open drain (default) or push-pull mode?
#ifndef MIOS32_UART3_TX_OD
#define MIOS32_UART3_TX_OD 1
#endif

// Baudrate/TX_OD for UART4..UART9 - only meaningful on STM32F4xx chips with
// more than 4 USART/UART peripherals (see mios32_uart.c's MIOS32_USE_UARTx
// force-undef for which ports actually exist per processor).
#ifndef MIOS32_UART4_BAUDRATE
#define MIOS32_UART4_BAUDRATE 31250
#endif
#ifndef MIOS32_UART4_TX_OD
#define MIOS32_UART4_TX_OD 1
#endif
#ifndef MIOS32_UART5_BAUDRATE
#define MIOS32_UART5_BAUDRATE 31250
#endif
#ifndef MIOS32_UART5_TX_OD
#define MIOS32_UART5_TX_OD 1
#endif
#ifndef MIOS32_UART6_BAUDRATE
#define MIOS32_UART6_BAUDRATE 31250
#endif
#ifndef MIOS32_UART6_TX_OD
#define MIOS32_UART6_TX_OD 1
#endif
#ifndef MIOS32_UART7_BAUDRATE
#define MIOS32_UART7_BAUDRATE 31250
#endif
#ifndef MIOS32_UART7_TX_OD
#define MIOS32_UART7_TX_OD 1
#endif
#ifndef MIOS32_UART8_BAUDRATE
#define MIOS32_UART8_BAUDRATE 31250
#endif
#ifndef MIOS32_UART8_TX_OD
#define MIOS32_UART8_TX_OD 1
#endif
#ifndef MIOS32_UART9_BAUDRATE
#define MIOS32_UART9_BAUDRATE 31250
#endif
#ifndef MIOS32_UART9_TX_OD
#define MIOS32_UART9_TX_OD 1
#endif

// Interface assignment: 0 = disabled, 1 = MIDI, 2 = COM
#ifndef MIOS32_UART0_ASSIGNMENT
#define MIOS32_UART0_ASSIGNMENT 1
#endif

// Interface assignment: 0 = disabled, 1 = MIDI, 2 = COM
#ifndef MIOS32_UART1_ASSIGNMENT
#define MIOS32_UART1_ASSIGNMENT 1
#endif

// Interface assignment: 0 = disabled, 1 = MIDI, 2 = COM
#ifndef MIOS32_UART2_ASSIGNMENT
#define MIOS32_UART2_ASSIGNMENT 1
#endif

// Interface assignment: 0 = disabled, 1 = MIDI, 2 = COM
#ifndef MIOS32_UART3_ASSIGNMENT
#define MIOS32_UART3_ASSIGNMENT 1
#endif

// Interface assignment: 0 = disabled, 1 = MIDI, 2 = COM (UART4..UART9)
#ifndef MIOS32_UART4_ASSIGNMENT
#define MIOS32_UART4_ASSIGNMENT 1
#endif
#ifndef MIOS32_UART5_ASSIGNMENT
#define MIOS32_UART5_ASSIGNMENT 1
#endif
#ifndef MIOS32_UART6_ASSIGNMENT
#define MIOS32_UART6_ASSIGNMENT 1
#endif
#ifndef MIOS32_UART7_ASSIGNMENT
#define MIOS32_UART7_ASSIGNMENT 1
#endif
#ifndef MIOS32_UART8_ASSIGNMENT
#define MIOS32_UART8_ASSIGNMENT 1
#endif
#ifndef MIOS32_UART9_ASSIGNMENT
#define MIOS32_UART9_ASSIGNMENT 1
#endif



/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_UART_Init(u32 mode);

extern s32 MIOS32_UART_IsAssignedToMIDI(u8 uart);

extern s32 MIOS32_UART_InitPort(u8 uart, u32 baudrate, mios32_pin_mode_t tx_pin_mode, u8 is_midi);
extern s32 MIOS32_UART_InitPortDefault(u8 uart);

extern s32 MIOS32_UART_BaudrateSet(u8 uart, u32 baudrate);
extern u32 MIOS32_UART_BaudrateGet(u8 uart);

extern s32 MIOS32_UART_RxBufferFree(u8 uart);
extern s32 MIOS32_UART_RxBufferUsed(u8 uart);
extern s32 MIOS32_UART_RxBufferGet(u8 uart);
extern s32 MIOS32_UART_RxBufferPeek(u8 uart);
extern s32 MIOS32_UART_RxBufferPut(u8 uart, u8 b);
extern s32 MIOS32_UART_TxBufferFree(u8 uart);
extern s32 MIOS32_UART_TxBufferUsed(u8 uart);
extern s32 MIOS32_UART_TxBufferGet(u8 uart);
extern s32 MIOS32_UART_TxBufferPut_NonBlocking(u8 uart, u8 b);
extern s32 MIOS32_UART_TxBufferPut(u8 uart, u8 b);
extern s32 MIOS32_UART_TxBufferPutMore_NonBlocking(u8 uart, u8 *buffer, u16 len);
extern s32 MIOS32_UART_TxBufferPutMore(u8 uart, u8 *buffer, u16 len);

// Flags returned by MIOS32_UART_ActGet(), same values whatever the port
#define MIOS32_UART_ACT_RX 0x01
#define MIOS32_UART_ACT_TX 0x02

// Line activity of ONE port, read-and-clear, normalised to the two flags
// above - the accessor to use: it can't drift when ports are renumbered.
extern u32 MIOS32_UART_ActGet(u8 uart);

// Line activity of EVERY port at once, read-and-clear: two bits per port in
// port order, RX = 1<<(2*n), TX = 1<<(2*n+1). u32 so the widest family fits
// (F4xx goes up to UART9, needing 20 bits on its own). Prefer ActGet above
// unless you really want the whole word - masking this one by hand means
// hardcoding port positions, which silently broke on the G0 renumbering.
extern u32 MIOS32_UART_RXTX_Act(void);

// NOTE: both are implemented by STM32G0xx only today; an F4xx project
// calling either will fail to link - see the F4xx driver.

/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////



#endif /* _MIOS32_UART_H */
