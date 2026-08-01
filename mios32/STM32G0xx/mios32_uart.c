// $Id: mios32_uart.c 2312 2016-02-27 23:04:51Z tk $
//! \defgroup MIOS32_UART
//!
//! U(S)ART functions for MIOS32
//!
//! Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
//! 
//! \{
/* ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>

// this module can be optionally disabled in a local mios32_config.h file (included from mios32.h)
#if !defined(MIOS32_DONT_USE_UART)


/////////////////////////////////////////////////////////////////////////////
// Pin definitions and USART mappings
/////////////////////////////////////////////////////////////////////////////

// how many UARTs are supported?
# if defined(MIOS32_PROCESSOR_STM32G050K8)
#if MIOS32_UART_NUM > 1
# define NUM_SUPPORTED_UARTS 2
#else
# define NUM_SUPPORTED_UARTS MIOS32_UART_NUM
#endif

#define MIOS32_UART0_TX_PORT     GPIOB
#define MIOS32_UART0_TX_PIN      LL_GPIO_PIN_6
#define MIOS32_UART0_TX_ALT      LL_GPIO_AF_0
#define MIOS32_UART0_RX_PORT     GPIOB
#define MIOS32_UART0_RX_PIN      LL_GPIO_PIN_7
#define MIOS32_UART0_RX_ALT      LL_GPIO_AF_0
#define MIOS32_UART0             USART1
#define MIOS32_UART0_IRQ_CHANNEL USART1_IRQn
#define MIOS32_UART0_IRQHANDLER_FUNC void USART1_IRQHandler(void)
#define MIOS32_UART0_RESET_FUNC  { LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_USART1); LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_USART1); }
//#define MIOS32_UART0_REMAP_FUNC  { LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_2, LL_GPIO_AF_7); LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_3, LL_GPIO_AF_7); }
#define MIOS32_UART0_CLOCK_FUNC  { LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1); LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB); }
//#define MIOS32_UART0_CLOCK_SOURCE	 LL_RCC_USART1_CLKSOURCE_SYSCLK

#define MIOS32_UART1_TX_PORT     GPIOA
#define MIOS32_UART1_TX_PIN      LL_GPIO_PIN_2
#define MIOS32_UART1_TX_ALT      LL_GPIO_AF_1
#define MIOS32_UART1_RX_PORT     GPIOA
#define MIOS32_UART1_RX_PIN      LL_GPIO_PIN_3
#define MIOS32_UART1_RX_ALT      LL_GPIO_AF_1
#define MIOS32_UART1             USART2
#define MIOS32_UART1_IRQ_CHANNEL USART2_IRQn
#define MIOS32_UART1_IRQHANDLER_FUNC void USART2_IRQHandler(void)
#define MIOS32_UART1_RESET_FUNC  { LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_USART2); LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_USART2); }
//#define MIOS32_UART1_REMAP_FUNC  { LL_GPIO_SetAFPin_8_15(GPIOC, LL_GPIO_PIN_10, LL_GPIO_AF_8); LL_GPIO_SetAFPin_8_15(GPIOC, LL_GPIO_PIN_11, LL_GPIO_AF_8); }
#define MIOS32_UART1_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2); LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA); }

#ifdef MIOS32_UART_MIDI_TX_BYPASS_OPTION
#define MIOS32_UARTx_BYPASS_PORT     	0	// MIOS32_UART0
#define MIOS32_UARTx_BYPASS_TX_PORT     GPIOB
#define MIOS32_UARTx_BYPASS_TX_PIN      LL_GPIO_PIN_8
#define MIOS32_UARTx_EXTI_PORT    		LL_EXTI_CONFIG_PORTB
#define MIOS32_UARTx_EXTI_PIN     		LL_EXTI_CONFIG_LINE8
#define MIOS32_UARTx_EXTI_LINE    		LL_EXTI_LINE_8
#define MIOS32_UARTx_EXTI_IRQ    		EXTI4_15_IRQn
#endif



# elif defined(MIOS32_PROCESSOR_STM32G070CB)
#if MIOS32_UART_NUM > 1
# define NUM_SUPPORTED_UARTS 2
#else
# define NUM_SUPPORTED_UARTS MIOS32_UART_NUM
#endif

#define MIOS32_UART0_TX_PORT     GPIOB
#define MIOS32_UART0_TX_PIN      LL_GPIO_PIN_8
#define MIOS32_UART0_TX_ALT      LL_GPIO_AF_4
#define MIOS32_UART0_RX_PORT     GPIOB
#define MIOS32_UART0_RX_PIN      LL_GPIO_PIN_9
#define MIOS32_UART0_RX_ALT      LL_GPIO_AF_4
#define MIOS32_UART0             USART3
#define MIOS32_UART0_IRQ_CHANNEL USART3_4_IRQn
#define MIOS32_UART0_IRQHANDLER_FUNC void USART3_4_IRQHandler(void)
#define MIOS32_UART0_RESET_FUNC  { LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_USART3); LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_USART3); }
#define MIOS32_UART0_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3); LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB); }

#define MIOS32_UART1_TX_PORT     GPIOB
#define MIOS32_UART1_TX_PIN      LL_GPIO_PIN_6
#define MIOS32_UART1_TX_ALT      LL_GPIO_AF_0
#define MIOS32_UART1_RX_PORT     GPIOB
#define MIOS32_UART1_RX_PIN      LL_GPIO_PIN_7
#define MIOS32_UART1_RX_ALT      LL_GPIO_AF_0
#define MIOS32_UART1             USART1
#define MIOS32_UART1_IRQ_CHANNEL USART1_IRQn
#define MIOS32_UART1_IRQHANDLER_FUNC void USART1_IRQHandler(void)
#define MIOS32_UART1_RESET_FUNC  { LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_USART1); LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_USART1); }
#define MIOS32_UART1_CLOCK_FUNC  { LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1); LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB); }
#define MIOS32_UART1_CLOCK_SOURCE	 LL_RCC_USART1_CLKSOURCE_PCLK1

#else
#define MIOS32_DONT_USE_UART
# define NUM_SUPPORTED_UARTS 0
#warning "No UART defined for this MIOS32_BOARD"
# endif






/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

#if NUM_SUPPORTED_UARTS >= 1
#ifdef MIOS32_UART_MIDI_TX_BYPASS_OPTION
static u8 uart_tx_bypass=0;	// not sure needed
#endif
static u8  uart_midi_act=0;
static u8  uart_assigned_to_midi;
static u32 uart_baudrate[NUM_SUPPORTED_UARTS];
static mios32_board_pin_mode_t  uart_tx_pin_mode[NUM_SUPPORTED_UARTS];

static u8 rx_buffer[NUM_SUPPORTED_UARTS][MIOS32_UART_RX_BUFFER_SIZE];
static volatile u8 rx_buffer_tail[NUM_SUPPORTED_UARTS];
static volatile u8 rx_buffer_head[NUM_SUPPORTED_UARTS];
static volatile u8 rx_buffer_size[NUM_SUPPORTED_UARTS];

static u8 tx_buffer[NUM_SUPPORTED_UARTS][MIOS32_UART_TX_BUFFER_SIZE];
static volatile u8 tx_buffer_tail[NUM_SUPPORTED_UARTS];
static volatile u8 tx_buffer_head[NUM_SUPPORTED_UARTS];
static volatile u8 tx_buffer_size[NUM_SUPPORTED_UARTS];
#endif


/////////////////////////////////////////////////////////////////////////////
//! Initializes UART interfaces
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
//! \note Applications shouldn't call this function directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_Init(u32 mode)
{
  // currently only mode 0 supported
  if( mode != 0 )
    return -1; // unsupported mode

#if NUM_SUPPORTED_UARTS == 0
  return -1; // no UARTs
#else

  // map UART pins and enable clocks
#if MIOS32_UART0_ASSIGNMENT != 0
  //MIOS32_UART0_REMAP_FUNC;
  MIOS32_UART0_RESET_FUNC;
  MIOS32_UART0_CLOCK_FUNC;
#endif
#if NUM_SUPPORTED_UARTS >= 2 && MIOS32_UART1_ASSIGNMENT != 0
  //MIOS32_UART1_REMAP_FUNC;
  MIOS32_UART1_RESET_FUNC;
  MIOS32_UART1_CLOCK_FUNC
#endif

  // initialize UARTs and clear buffers
  {
    u8 uart;
    for(uart=0; uart<NUM_SUPPORTED_UARTS; ++uart) {
      rx_buffer_tail[uart] = rx_buffer_head[uart] = rx_buffer_size[uart] = 0;
      tx_buffer_tail[uart] = tx_buffer_head[uart] = tx_buffer_size[uart] = 0;

      MIOS32_UART_InitPortDefault(uart);
    }
  }

  // configure and enable UART interrupts
#if MIOS32_UART0_ASSIGNMENT != 0
  MIOS32_IRQ_Install(MIOS32_UART0_IRQ_CHANNEL, MIOS32_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(MIOS32_UART0);

#endif

#if NUM_SUPPORTED_UARTS >= 2 && MIOS32_UART1_ASSIGNMENT != 0
  MIOS32_IRQ_Install(MIOS32_UART1_IRQ_CHANNEL, MIOS32_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(MIOS32_UART1);

#endif

  // enable UARTs
#if MIOS32_UART0_ASSIGNMENT != 0
  LL_USART_Enable(MIOS32_UART0);
  /* Polling USART initialisation */
  while((!(LL_USART_IsActiveFlag_TEACK(MIOS32_UART0))) || (!(LL_USART_IsActiveFlag_REACK(MIOS32_UART0)))){}
#endif
#if NUM_SUPPORTED_UARTS >= 2 && MIOS32_UART1_ASSIGNMENT != 0
  LL_USART_Enable(MIOS32_UART1);
  /* Polling USART initialisation */
  while((!(LL_USART_IsActiveFlag_TEACK(MIOS32_UART1))) || (!(LL_USART_IsActiveFlag_REACK(MIOS32_UART1)))){}
#endif

  return 0; // no error
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! \return 0 if UART is not assigned to a MIDI function
//! \return 1 if UART is assigned to a MIDI function
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_IsAssignedToMIDI(u8 uart)
{
#if NUM_SUPPORTED_UARTS == 0
  return 0; // no UART available
#else
  return (uart_assigned_to_midi & (1 << uart)) ? 1 : 0;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Initializes a given UART interface based on given baudrate and TX output mode
//! \param[in] uart UART number (0..2)
//! \param[in] baudrate the baudrate
//! \param[in] tx_pin_mode the TX pin mode
//!   <UL>
//!     <LI>MIOS32_BOARD_PIN_MODE_OUTPUT_PP: TX pin configured for push-pull mode
//!     <LI>MIOS32_BOARD_PIN_MODE_OUTPUT_OD: TX pin configured for open drain mode
//!   </UL>
//! \param[in] is_midi MIDI or common UART interface?
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_InitPort(u8 uart, u32 baudrate, mios32_board_pin_mode_t tx_pin_mode, u8 is_midi)
{
#if NUM_SUPPORTED_UARTS == 0
  return -1; // no UART available
#else
  LL_GPIO_InitTypeDef GPIO_InitStructure;
  LL_GPIO_StructInit(&GPIO_InitStructure);


  if( uart >= NUM_SUPPORTED_UARTS )
    return -1; // unsupported UART

  // MIDI assignment
  if( is_midi ) {
    uart_assigned_to_midi |= (1 << uart);
  } else {
    uart_assigned_to_midi &= ~(1 << uart);
  }
  // store pin mode
  uart_tx_pin_mode[uart]=tx_pin_mode;

#ifdef MIOS32_UART_MIDI_TX_BYPASS_OPTION

		  GPIO_InitStructure.Pin = MIOS32_UARTx_BYPASS_TX_PIN;
		  GPIO_InitStructure.Mode = LL_GPIO_MODE_INPUT;
		  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
		  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
		  GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
		LL_GPIO_Init(MIOS32_UARTx_BYPASS_TX_PORT, &GPIO_InitStructure);

		LL_EXTI_InitTypeDef EXTI_InitStruct = {0};
		LL_EXTI_SetEXTISource(MIOS32_UARTx_EXTI_PORT, MIOS32_UARTx_EXTI_PIN);
		EXTI_InitStruct.Line_0_31 = MIOS32_UARTx_EXTI_LINE;
		EXTI_InitStruct.LineCommand = ENABLE;
		EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
		EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING_FALLING;
		LL_EXTI_Init(&EXTI_InitStruct);

		MIOS32_IRQ_Install(MIOS32_UARTx_EXTI_IRQ, 0);
		uart_tx_bypass=0;		// disabled(default at startup
		MIOS32_STOPWATCH_Init(100);
#endif

	GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_HIGH;

  switch( uart ) {
#if NUM_SUPPORTED_UARTS >= 1 && MIOS32_UART0_ASSIGNMENT != 0
  case 0: {
    // output
    GPIO_InitStructure.Pin = MIOS32_UART0_TX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = (tx_pin_mode == MIOS32_BOARD_PIN_MODE_OUTPUT_PP) ? LL_GPIO_OUTPUT_PUSHPULL : LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStructure.Alternate = MIOS32_UART0_TX_ALT;
    LL_GPIO_Init(MIOS32_UART0_TX_PORT, &GPIO_InitStructure);

    // inputs with internal pull-up
    GPIO_InitStructure.Pin = MIOS32_UART0_RX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStructure.Pull  = LL_GPIO_PULL_UP;
    GPIO_InitStructure.Alternate = MIOS32_UART0_RX_ALT;
    LL_GPIO_Init(MIOS32_UART0_RX_PORT, &GPIO_InitStructure);

    // UART configuration
    MIOS32_UART_BaudrateSet(uart, baudrate);
  } break;
#endif

#if NUM_SUPPORTED_UARTS >= 2 && MIOS32_UART1_ASSIGNMENT != 0
  case 1: {
    // output
    GPIO_InitStructure.Pin = MIOS32_UART1_TX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = (tx_pin_mode == MIOS32_BOARD_PIN_MODE_OUTPUT_PP) ? LL_GPIO_OUTPUT_PUSHPULL : LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStructure.Alternate = MIOS32_UART1_TX_ALT;
    LL_GPIO_Init(MIOS32_UART1_TX_PORT, &GPIO_InitStructure);

    // inputs with internal pull-up
    GPIO_InitStructure.Pin = MIOS32_UART1_RX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStructure.Pull  = LL_GPIO_PULL_UP;
    GPIO_InitStructure.Alternate = MIOS32_UART1_RX_ALT;
    LL_GPIO_Init(MIOS32_UART1_RX_PORT, &GPIO_InitStructure);

    // UART configuration
    MIOS32_UART_BaudrateSet(uart, baudrate);
  } break;
#endif

  default:
    return -1; // unsupported UART
  }

  return 0; // no error
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Initializes a given UART interface based on default settings
//! \param[in] uart UART number (0..2)
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_InitPortDefault(u8 uart)
{
#if NUM_SUPPORTED_UARTS == 0
  return -1; // no UART available
#else
  switch( uart ) {
#if NUM_SUPPORTED_UARTS >= 1 && MIOS32_UART0_ASSIGNMENT != 0
  case 0: {
# if MIOS32_UART0_TX_OD
    MIOS32_UART_InitPort(0, MIOS32_UART0_BAUDRATE, MIOS32_BOARD_PIN_MODE_OUTPUT_OD, MIOS32_UART0_ASSIGNMENT == 1);
# else
    MIOS32_UART_InitPort(0, MIOS32_UART0_BAUDRATE, MIOS32_BOARD_PIN_MODE_OUTPUT_PP, MIOS32_UART0_ASSIGNMENT == 1);
# endif
  } break;
#endif

#if NUM_SUPPORTED_UARTS >= 2 && MIOS32_UART1_ASSIGNMENT != 0
  case 1: {
# if MIOS32_UART1_TX_OD
    MIOS32_UART_InitPort(1, MIOS32_UART1_BAUDRATE, MIOS32_BOARD_PIN_MODE_OUTPUT_OD, MIOS32_UART1_ASSIGNMENT == 1);
# else
    MIOS32_UART_InitPort(1, MIOS32_UART1_BAUDRATE, MIOS32_BOARD_PIN_MODE_OUTPUT_PP, MIOS32_UART1_ASSIGNMENT == 1);
# endif
  } break;
#endif

  default:
    return -1; // unsupported UART
  }

  return 0; // no error
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! sets the baudrate of a UART port
//! \param[in] uart UART number (0..2)
//! \param[in] baudrate the baudrate
//! \return 0: baudrate has been changed
//! \return -1: uart not available
//! \return -2: function not prepared for this UART
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_BaudrateSet(u8 uart, u32 baudrate)
{
#if NUM_SUPPORTED_UARTS == 0
  return -1; // no UART available
#else
  if( uart >= NUM_SUPPORTED_UARTS )
    return -1;
  // USART configuration
  LL_USART_InitTypeDef USART_InitStructure;
  USART_InitStructure.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStructure.StopBits = LL_USART_STOPBITS_1;
  USART_InitStructure.Parity = LL_USART_PARITY_NONE;
  USART_InitStructure.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStructure.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStructure.OverSampling = LL_USART_OVERSAMPLING_16;
  USART_InitStructure.BaudRate = baudrate;
  USART_InitStructure.PrescalerValue = LL_USART_PRESCALER_DIV1;

  switch( uart ) {
  case 0:
	  LL_USART_Init(MIOS32_UART0, &USART_InitStructure);
	  LL_USART_SetTXFIFOThreshold(MIOS32_UART0, LL_USART_FIFOTHRESHOLD_1_4);
	  LL_USART_SetRXFIFOThreshold(MIOS32_UART0, LL_USART_FIFOTHRESHOLD_1_8);
	  LL_USART_DisableFIFO(MIOS32_UART0);
	  LL_USART_DisableOverrunDetect(MIOS32_UART0);
	  LL_USART_DisableDMADeactOnRxErr(MIOS32_UART0);
	  LL_USART_ConfigAsyncMode(MIOS32_UART0);
	  LL_USART_SetTXPinLevel(MIOS32_UART0, LL_USART_TXPIN_LEVEL_INVERTED);
#ifdef MIOS32_UART0_CLOCK_SOURCE
	  LL_RCC_SetUSARTClockSource(MIOS32_UART0_CLOCK_SOURCE);
#endif
	  //LL_USART_SetBaudRate(MIOS32_UART0, 64000000, LL_USART_PRESCALER_DIV1, LL_USART_OVERSAMPLING_16, baudrate);

	  break;
#if NUM_SUPPORTED_UARTS >= 2
  case 1:
	  LL_USART_Init(MIOS32_UART1, &USART_InitStructure);
	  LL_USART_SetTXFIFOThreshold(MIOS32_UART1, LL_USART_FIFOTHRESHOLD_1_4);
	  LL_USART_SetRXFIFOThreshold(MIOS32_UART1, LL_USART_FIFOTHRESHOLD_1_8);
	  LL_USART_DisableFIFO(MIOS32_UART1);
	  LL_USART_DisableOverrunDetect(MIOS32_UART1);
	  LL_USART_DisableDMADeactOnRxErr(MIOS32_UART1);
	  LL_USART_ConfigAsyncMode(MIOS32_UART1);
#ifdef MIOS32_UART1_CLOCK_SOURCE
	  LL_RCC_SetUSARTClockSource(MIOS32_UART1_CLOCK_SOURCE);
#endif
	  //LL_USART_SetBaudRate(MIOS32_UART1, 16000000, LL_USART_PRESCALER_DIV1, LL_USART_OVERSAMPLING_16, baudrate);
	  break;
#endif
  default:
    return -2; // not prepared
  }

  // store baudrate in array
  uart_baudrate[uart] = baudrate;

  return 0;
#endif
}

/////////////////////////////////////////////////////////////////////////////
//! returns the current baudrate of a UART port
//! \param[in] uart UART number (0..2)
//! \return 0: uart not available
//! \return all other values: the current baudrate
/////////////////////////////////////////////////////////////////////////////
u32 MIOS32_UART_BaudrateGet(u8 uart)
{
#if NUM_SUPPORTED_UARTS == 0
  return 0; // no UART available
#else
  if( uart >= NUM_SUPPORTED_UARTS )
    return 0;
  else
    return uart_baudrate[uart];
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! returns number of free bytes in receive buffer
//! \param[in] uart UART number (0..2)
//! \return uart number of free bytes
//! \return 1: uart available
//! \return 0: uart not available
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferFree(u8 uart)
{
#if NUM_SUPPORTED_UARTS == 0
  return 0; // no UART available
#else
  if( uart >= NUM_SUPPORTED_UARTS )
    return 0;
  else
    return MIOS32_UART_RX_BUFFER_SIZE - rx_buffer_size[uart];
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! returns number of used bytes in receive buffer
//! \param[in] uart UART number (0..2)
//! \return > 0: number of used bytes
//! \return 0 if uart not available
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferUsed(u8 uart)
{
#if NUM_SUPPORTED_UARTS == 0
  return 0; // no UART available
#else
  if( uart >= NUM_SUPPORTED_UARTS )
    return 0;
  else
    return rx_buffer_size[uart];
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! gets a byte from the receive buffer
//! \param[in] uart UART number (0..2)
//! \return -1 if UART not available
//! \return -2 if no new byte available
//! \return >= 0: number of received bytes
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferGet(u8 uart)
{
#if NUM_SUPPORTED_UARTS == 0
  return -1; // no UART available
#else
  if( uart >= NUM_SUPPORTED_UARTS )
    return -1; // UART not available

  if( !rx_buffer_size[uart] )
    return -2; // nothing new in buffer

  // get byte - this operation should be atomic!
  MIOS32_IRQ_Disable();
  u8 b = rx_buffer[uart][rx_buffer_tail[uart]];
  if( ++rx_buffer_tail[uart] >= MIOS32_UART_RX_BUFFER_SIZE )
    rx_buffer_tail[uart] = 0;
  --rx_buffer_size[uart];
  MIOS32_IRQ_Enable();

  return b; // return received byte
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! returns the next byte of the receive buffer without taking it
//! \param[in] uart UART number (0..2)
//! \return -1 if UART not available
//! \return -2 if no new byte available
//! \return >= 0: number of received bytes
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferPeek(u8 uart)
{
#if NUM_SUPPORTED_UARTS == 0
  return -1; // no UART available
#else
  if( uart >= NUM_SUPPORTED_UARTS )
    return -1; // UART not available

  if( !rx_buffer_size[uart] )
    return -2; // nothing new in buffer

  // get byte - this operation should be atomic!
  MIOS32_IRQ_Disable();
  u8 b = rx_buffer[uart][rx_buffer_tail[uart]];
  MIOS32_IRQ_Enable();

  return b; // return received byte
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! puts a byte onto the receive buffer
//! \param[in] uart UART number (0..2)
//! \param[in] b byte which should be put into Rx buffer
//! \return 0 if no error
//! \return -1 if UART not available
//! \return -2 if buffer full (retry)
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferPut(u8 uart, u8 b)
{
#if NUM_SUPPORTED_UARTS == 0
  return -1; // no UART available
#else
  if( uart >= NUM_SUPPORTED_UARTS )
    return -1; // UART not available

  if( rx_buffer_size[uart] >= MIOS32_UART_RX_BUFFER_SIZE )
    return -2; // buffer full (retry)

  // copy received byte into receive buffer
  // this operation should be atomic!
  MIOS32_IRQ_Disable();
  rx_buffer[uart][rx_buffer_head[uart]] = b;
  if( ++rx_buffer_head[uart] >= MIOS32_UART_RX_BUFFER_SIZE )
    rx_buffer_head[uart] = 0;
  ++rx_buffer_size[uart];
  MIOS32_IRQ_Enable();

  return 0; // no error
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! returns number of free bytes in transmit buffer
//! \param[in] uart UART number (0..2)
//! \return number of free bytes
//! \return 0 if uart not available
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferFree(u8 uart)
{
#if NUM_SUPPORTED_UARTS == 0
  return 0; // no UART available
#else
  if( uart >= NUM_SUPPORTED_UARTS )
    return 0;
  else
    return MIOS32_UART_TX_BUFFER_SIZE - tx_buffer_size[uart];
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! returns number of used bytes in transmit buffer
//! \param[in] uart UART number (0..2)
//! \return number of used bytes
//! \return 0 if uart not available
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferUsed(u8 uart)
{
#if NUM_SUPPORTED_UARTS == 0
  return 0; // no UART available
#else
  if( uart >= NUM_SUPPORTED_UARTS )
    return 0;
  else
    return tx_buffer_size[uart];
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! gets a byte from the transmit buffer
//! \param[in] uart UART number (0..2)
//! \return -1 if UART not available
//! \return -2 if no new byte available
//! \return >= 0: transmitted byte
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferGet(u8 uart)
{
#if NUM_SUPPORTED_UARTS == 0
  return -1; // no UART available
#else
  if( uart >= NUM_SUPPORTED_UARTS )
    return -1; // UART not available

  if( !tx_buffer_size[uart] )
    return -2; // nothing new in buffer

  // get byte - this operation should be atomic!
  MIOS32_IRQ_Disable();
  u8 b = tx_buffer[uart][tx_buffer_tail[uart]];
  if( ++tx_buffer_tail[uart] >= MIOS32_UART_TX_BUFFER_SIZE )
    tx_buffer_tail[uart] = 0;
  --tx_buffer_size[uart];
  MIOS32_IRQ_Enable();

  return b; // return transmitted byte
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! puts more than one byte onto the transmit buffer (used for atomic sends)
//! \param[in] uart UART number (0..2)
//! \param[in] *buffer pointer to buffer to be sent
//! \param[in] len number of bytes to be sent
//! \return 0 if no error
//! \return -1 if UART not available
//! \return -2 if buffer full or cannot get all requested bytes (retry)
//! \return -3 if UART not supported by MIOS32_UART_TxBufferPut Routine
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferPutMore_NonBlocking(u8 uart, u8 *buffer, u16 len)
{
#if NUM_SUPPORTED_UARTS == 0
  return -1; // no UART available
#else
  if( uart >= NUM_SUPPORTED_UARTS )
    return -1; // UART not available

  if( (tx_buffer_size[uart]+len) >= MIOS32_UART_TX_BUFFER_SIZE )
    return -2; // buffer full or cannot get all requested bytes (retry)

  // copy bytes to be transmitted into transmit buffer
  // this operation should be atomic!
#ifdef MIOS32_UART_MIDI_TX_BYPASS_OPTION
	MIOS32_UART_TX_Bypass(uart, 0);
#endif
  MIOS32_IRQ_Disable();

  u16 i;
  for(i=0; i<len; ++i) {
    tx_buffer[uart][tx_buffer_head[uart]] = *buffer++;

    if( ++tx_buffer_head[uart] >= MIOS32_UART_TX_BUFFER_SIZE )
      tx_buffer_head[uart] = 0;

    // enable Tx interrupt if buffer was empty
    if( ++tx_buffer_size[uart] == 1 ) {
      switch( uart ) {
        case 0: MIOS32_UART0->CR1 |= (1 << 7); break; // enable TXE interrupt (TXEIE=1)
#if NUM_SUPPORTED_UARTS >=2
        case 1: MIOS32_UART1->CR1 |= (1 << 7); break; // enable TXE interrupt (TXEIE=1)
#endif
        default: MIOS32_IRQ_Enable(); return -3; // uart not supported by routine (yet)
      }
    }
  }

  MIOS32_IRQ_Enable();

  return 0; // no error
#endif
}

/////////////////////////////////////////////////////////////////////////////
//! puts more than one byte onto the transmit buffer (used for atomic sends)<BR>
//! (blocking function)
//! \param[in] uart UART number (0..2)
//! \param[in] *buffer pointer to buffer to be sent
//! \param[in] len number of bytes to be sent
//! \return 0 if no error
//! \return -1 if UART not available
//! \return -3 if UART not supported by MIOS32_UART_TxBufferPut Routine
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferPutMore(u8 uart, u8 *buffer, u16 len)
{
  s32 error;

  while( (error=MIOS32_UART_TxBufferPutMore_NonBlocking(uart, buffer, len)) == -2 );

  return error;
}


/////////////////////////////////////////////////////////////////////////////
//! puts a byte onto the transmit buffer
//! \param[in] uart UART number (0..2)
//! \param[in] b byte which should be put into Tx buffer
//! \return 0 if no error
//! \return -1 if UART not available
//! \return -2 if buffer full (retry)
//! \return -3 if UART not supported by MIOS32_UART_TxBufferPut Routine
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferPut_NonBlocking(u8 uart, u8 b)
{
  // for more comfortable usage...
  // -> just forward to MIOS32_UART_TxBufferPutMore
  return MIOS32_UART_TxBufferPutMore(uart, &b, 1);
}


/////////////////////////////////////////////////////////////////////////////
//! puts a byte onto the transmit buffer<BR>
//! (blocking function)
//! \param[in] uart UART number (0..2)
//! \param[in] b byte which should be put into Tx buffer
//! \return 0 if no error
//! \return -1 if UART not available
//! \return -3 if UART not supported by MIOS32_UART_TxBufferPut Routine
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferPut(u8 uart, u8 b)
{
  s32 error;

  while( (error=MIOS32_UART_TxBufferPutMore(uart, &b, 1)) == -2 );

  return error;
}


/////////////////////////////////////////////////////////////////////////////
// Interrupt handler for first UART
/////////////////////////////////////////////////////////////////////////////
#if NUM_SUPPORTED_UARTS >= 1
MIOS32_UART0_IRQHANDLER_FUNC
{
  if( MIOS32_UART0->ISR & (1 << 5) ) { // check if RXNE flag is set
    u8 b = MIOS32_UART0->RDR;
    if(b!=0xf8)uart_midi_act |=1;
    s32 status = MIOS32_UART_IsAssignedToMIDI(0) ? MIOS32_MIDI_SendByteToRxCallback(UART0, b) : 0;

    if( status == 0 && MIOS32_UART_RxBufferPut(0, b) < 0 ) {
      // here we could add some error handling
    }
  }

  if( MIOS32_UART0->ISR & (1 << 7) ) { // check if TXE flag is set
    if( MIOS32_UART_TxBufferUsed(0) > 0 ) {
      s32 b = MIOS32_UART_TxBufferGet(0);
      if( b < 0 ) {
	// here we could add some error handling
	MIOS32_UART0->TDR = 0xff;
      } else {
	MIOS32_UART0->TDR = b;
	if(b!=0xf8)uart_midi_act |=2;
      }
    } else {
      MIOS32_UART0->CR1 &= ~(1 << 7); // disable TXE interrupt (TXEIE=0)
#ifdef MIOS32_UART_MIDI_TX_BYPASS_OPTION
      while((MIOS32_UART0->ISR & (1 << 6))==0){};
      MIOS32_UART_TX_Bypass(0, 1);
#endif
    }
  }
}
#endif


/////////////////////////////////////////////////////////////////////////////
// Interrupt handler for second UART
/////////////////////////////////////////////////////////////////////////////
#if NUM_SUPPORTED_UARTS >= 2
MIOS32_UART1_IRQHANDLER_FUNC
{
  if( MIOS32_UART1->ISR & (1 << 5) ) { // check if RXNE flag is set
    u8 b = MIOS32_UART1->RDR;
    if(b!=0xf8)uart_midi_act |=4;
    s32 status = MIOS32_UART_IsAssignedToMIDI(1) ? MIOS32_MIDI_SendByteToRxCallback(UART1, b) : 0;

    if( status == 0 && MIOS32_UART_RxBufferPut(1, b) < 0 ) {
      // here we could add some error handling
    }
  }
  
  if( MIOS32_UART1->ISR & (1 << 7) ) { // check if TXE flag is set
    if( MIOS32_UART_TxBufferUsed(1) > 0 ) {
      s32 b = MIOS32_UART_TxBufferGet(1);
      if( b < 0 ) {
	// here we could add some error handling
	MIOS32_UART1->TDR = 0xff;
      } else {
	MIOS32_UART1->TDR = b;
	if(b!=0xf8)uart_midi_act |=8;
      }
    } else {
      MIOS32_UART1->CR1 &= ~(1 << 7); // disable TXE interrupt (TXEIE=0)
#ifdef MIOS32_UART_MIDI_TX_BYPASS_OPTION
      MIOS32_UART_TX_Bypass(1, 1);
#endif
    }
  }
}
#endif


/////////////////////////////////////////////////////////////////////////////
//! This function set the MIDI TX in bypass state
//! This option is usefull to insert an upgrade in an existing machine.
//! \param[in] uart_port UART number (0..2)
//! \param[in] enable/disable
//! \return -1 if port not available
//! \return < 0 on errors
//! \note Applications shouldn't call this function directly, instead please use \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TX_Bypass(u8 uart, u8 enable)
{
#ifdef MIOS32_UART_MIDI_TX_BYPASS_OPTION
#if MIOS32_UART_NUM == 0
	return -1; // all UARTs explicitely disabled
#else
	if(uart!=MIOS32_UARTx_BYPASS_PORT)
		return -1;
	if(	enable == uart_tx_bypass)
		return 0;

	LL_GPIO_InitTypeDef GPIO_InitStructure;
	LL_GPIO_StructInit(&GPIO_InitStructure);

	GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
	while(MIOS32_STOPWATCH_ValueGet()<82){}
	if(enable){
		uart_tx_bypass=enable;
		//MIOS32_STOPWATCH_Init(100);
		GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
		GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	}else{
			//
		uart_tx_bypass=enable;
		//MIOS32_STOPWATCH_Stop();
		GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
		GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_HIGH;
	}
#if MIOS32_UARTx_BYPASS_PORT==0	// MIOS32_UART0
	GPIO_InitStructure.Pin = MIOS32_UART0_TX_PIN;
	GPIO_InitStructure.Alternate = MIOS32_UART0_TX_ALT;
	GPIO_InitStructure.OutputType = (uart_tx_pin_mode[0] == MIOS32_BOARD_PIN_MODE_OUTPUT_PP) ? LL_GPIO_OUTPUT_PUSHPULL : LL_GPIO_OUTPUT_OPENDRAIN;
	LL_GPIO_Init(MIOS32_UART0_TX_PORT, &GPIO_InitStructure);
	while((!(LL_USART_IsActiveFlag_TEACK(MIOS32_UART0))) || (!(LL_USART_IsActiveFlag_REACK(MIOS32_UART0)))){}
#elif MIOS32_UARTx_BYPASS_PORT==1	// MIOS32_UART0
	GPIO_InitStructure.Pin = MIOS32_UART1_TX_PIN;
	GPIO_InitStructure.Alternate = MIOS32_UART1_TX_ALT;
	GPIO_InitStructure.OutputType = (uart_tx_pin_mode[1] == MIOS32_BOARD_PIN_MODE_OUTPUT_PP) ? LL_GPIO_OUTPUT_PUSHPULL : LL_GPIO_OUTPUT_OPENDRAIN;
	LL_GPIO_Init(MIOS32_UART1_TX_PORT, &GPIO_InitStructure);
	while((!(LL_USART_IsActiveFlag_TEACK(MIOS32_UART1))) || (!(LL_USART_IsActiveFlag_REACK(MIOS32_UART1)))){	}
#else
#warning "No TX bypass UART set!"
#endif

	return 0;
#endif
#else
	return 0; // no error
#endif
}

/////////////////////////////////////////////////////////////////////////////
//! This function set the MIDI TX in bypass state
//! This option is usefull to insert an upgrade in an existing machine.
//! \param[in] uart_port UART number (0..2)
//! \param[in] enable/disable
//! \return -1 if port not available
//! \return < 0 on errors
//! \note Applications shouldn't call this function directly, instead please use \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
u8 MIOS32_UART_RXTX_Act(void){
	u8 status = 0;
#if NUM_SUPPORTED_UARTS == 0
	return -1; // all UARTs explicitely disabled
#else
	if(uart_midi_act){
		status=uart_midi_act;
	}
	uart_midi_act=0;

	return status; // no error
#endif
}



/////////////////////////////////////////////////////////////////////////////
//! MIDI TX in bypass callback
//! This option is usefull to insert an upgrade in an existing machine.
//! \EXTIx_x_IRQHandler must be placed in app
//! \with callback inside
/////////////////////////////////////////////////////////////////////////////
void MIOS32_UART_TX_BypassCallback(void){
#ifdef MIOS32_UART_MIDI_TX_BYPASS_OPTION
	if (LL_EXTI_IsActiveFallingFlag_0_31(MIOS32_UARTx_EXTI_LINE) != RESET)
	{
		LL_EXTI_ClearFallingFlag_0_31(MIOS32_UARTx_EXTI_LINE);

		if(uart_tx_bypass){
#if MIOS32_UARTx_BYPASS_PORT==0	// MIOS32_UART0
			MIOS32_SYS_STM_PINSET_0(MIOS32_UART0_TX_PORT, MIOS32_UART0_TX_PIN);
#elif MIOS32_UARTx_BYPASS_PORT==1	// MIOS32_UART0
			MIOS32_SYS_STM_PINSET_0(MIOS32_UART0_TX_PORT, MIOS32_UART1_TX_PIN);
#else
#warning "No TX bypass UART set!"
#endif
		}
		MIOS32_STOPWATCH_Reset();

	}
	if (LL_EXTI_IsActiveRisingFlag_0_31(MIOS32_UARTx_EXTI_LINE) != RESET)
	{
		LL_EXTI_ClearRisingFlag_0_31(MIOS32_UARTx_EXTI_LINE);

		if(uart_tx_bypass){
#if MIOS32_UARTx_BYPASS_PORT==0	// MIOS32_UART0
			MIOS32_SYS_STM_PINSET_1(MIOS32_UART0_TX_PORT, MIOS32_UART0_TX_PIN);
#elif MIOS32_UARTx_BYPASS_PORT==1	// MIOS32_UART0
			MIOS32_SYS_STM_PINSET_1(MIOS32_UART0_TX_PORT, MIOS32_UART1_TX_PIN);
#else
#warning "No TX bypass UART set!"
#endif
			if(MIOS32_STOPWATCH_ValueGet()<2)uart_midi_act |=0x10;
		}

	}
#endif
}


#endif /* MIOS32_DONT_USE_UART */
