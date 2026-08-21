//! \defgroup ADIOS_UART
//!
//! U(S)ART functions for ADIOS - STM32F4xx
//!
//! Applications shouldn't call these functions directly, instead please use \ref ADIOS_COM or \ref ADIOS_MIDI layer functions
//!
//! Up to 10 ports are supported, matching the 10 USART/UART peripherals
//! present on the highest-tier F4 chips (STM32F413/F423). Every F4 chip has
//! at least 6 (USART1/2/3/6 + UART4/5); F427/F429/F437/F439/F469/F479 add
//! UART7/UART8 (8 total); F413/F423 add UART9/UART10 on top of that (10
//! total). ADIOS_UART6..ADIOS_UART9 are force-disabled at compile time via
//! the CMSIS device-tier macro (STM32F427xx, STM32F429xx, STM32F413xx, etc)
//! on every processor that doesn't have that many UARTs in silicon - this is
//! automatic per ADIOS_PROCESSOR_xxx, no edit needed here when a new F4
//! project is added to the build system.
//!
//! Unlike STM32G0xx, every F4 UART/USART has its OWN independent NVIC vector
//! - no shared-handler complexity here.
//!
//! Default pin/peripheral assignment - override individually
//! in a local adios_config.h if your board uses different pins:
//! \code
//!   #define ADIOS_UART0_TX_PORT     GPIOB
//!   #define ADIOS_UART0_TX_PIN      LL_GPIO_PIN_6
//!   #define ADIOS_UART0_TX_AF       LL_GPIO_AF_7
//!   #define ADIOS_UART0_RX_PORT     GPIOB
//!   #define ADIOS_UART0_RX_PIN      LL_GPIO_PIN_7
//!   #define ADIOS_UART0_RX_AF       LL_GPIO_AF_7
//!   #define ADIOS_UART0             USART1
//!   #define ADIOS_UART0_IRQ_CHANNEL USART1_IRQn
//!   #define ADIOS_UART0_IRQHANDLER_FUNC void USART1_IRQHandler(void)
//!   #define ADIOS_UART0_CLOCK_FUNC  { ... }
//!   #define ADIOS_UART0_APB_FREQ    84000000
//! \endcode
//! (same set of defines exists for ADIOS_UART1..ADIOS_UART9)
//!
//! ADIOS_UARTx_APB_FREQ matters because USART1/USART6/UART9/UART10 sit on
//! APB2 (84 MHz by default) while everything else sits on APB1 (42 MHz by
//! default) - get this wrong and the baudrate comes out at half (or double)
//! the intended value. Override it if your project uses a different clock
//! tree (see adios_sys.c's PLL_N/clock source overrides).
//!
//! Note: unlike STM32G0xx, this peripheral generation has no hardware TX
//! polarity inversion (no ADIOS_UARTx_TX_INVERTED support) - if a board
//! needs an inverted MIDI output, it needs an external inverting stage
//! that's transparent to this driver (a plain buffer, not a level-shifting
//! transistor that also inverts).
//!
//! \{
/* ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <adios.h>

// auto-derive the master switch from any individual port actually wanted -
// no need for the project to separately set ADIOS_USE_UART on top of
// ADIOS_USE_UARTx. Safe to do locally in this .c file only because nothing
// outside adios_uart.c ever checks the bare ADIOS_USE_UART macro
// (verified) - unlike ADIOS_USE_SPI, which core/
// main.c also checks directly, so that one is derived from the shared
// header (adios_spi.h) instead, not locally in adios_spi.c.
#if !defined(ADIOS_USE_UART) && (defined(ADIOS_USE_UART0) || defined(ADIOS_USE_UART1) || defined(ADIOS_USE_UART2) || defined(ADIOS_USE_UART3) || defined(ADIOS_USE_UART4) || defined(ADIOS_USE_UART5) || defined(ADIOS_USE_UART6) || defined(ADIOS_USE_UART7) || defined(ADIOS_USE_UART8) || defined(ADIOS_USE_UART9))
#define ADIOS_USE_UART
#endif

// this module can be optionally enabled in a local adios_config.h file (included from adios.h)
#if defined(ADIOS_USE_UART)

// UART6/UART7 (UART7/UART8 peripherals) only exist on the 8-UART and
// 10-UART tiers; UART8/UART9 (UART9/UART10 peripherals) only exist on the
// 10-UART tier. Checked via the CMSIS device-tier macro (STM32F427xx,
// STM32F429xx, etc - defined by stm32f4xx.h's own dispatcher once
// ADIOS_PROCESSOR_xxx has selected a device header), not a fixed
// "no such processor in this build system yet" assumption - so a project
// that adds e.g. PROCESSOR=STM32F429ZI to its own Makefile gets UART6/7
// for free, no edit to this common driver file required.
#if defined(ADIOS_USE_UART6) && !(defined(STM32F427xx) || defined(STM32F437xx) || defined(STM32F429xx) || defined(STM32F439xx) || defined(STM32F469xx) || defined(STM32F479xx) || defined(STM32F413xx) || defined(STM32F423xx))
#undef ADIOS_USE_UART6
#endif
#if defined(ADIOS_USE_UART7) && !(defined(STM32F427xx) || defined(STM32F437xx) || defined(STM32F429xx) || defined(STM32F439xx) || defined(STM32F469xx) || defined(STM32F479xx) || defined(STM32F413xx) || defined(STM32F423xx))
#undef ADIOS_USE_UART7
#endif
#if defined(ADIOS_USE_UART8) && !(defined(STM32F413xx) || defined(STM32F423xx))
#undef ADIOS_USE_UART8
#endif
#if defined(ADIOS_USE_UART9) && !(defined(STM32F413xx) || defined(STM32F423xx))
#undef ADIOS_USE_UART9
#endif


/////////////////////////////////////////////////////////////////////////////
// Pin definitions and USART mappings
/////////////////////////////////////////////////////////////////////////////

// fixed number of port "slots" (buffers, arrays) - independent of how many
// are actually enabled via ADIOS_USE_UARTx, same pattern as adios_spi.c
#define ADIOS_UART_MAX_PORTS 10

// UART0 (USART1 peripheral) - APB2 (84 MHz). PB6/PB7, and deliberately NOT
// the PA9/PA10 this peripheral also offers: on this family those two pins
// are OTG_FS_VBUS and OTG_FS_ID, so a default sitting there would fight the
// USB connector on every board that has one. PB6/PB7 is also what the
// STM32G0xx driver uses for USART1, so the first MIDI port lands on the
// same pins whichever family a design is built on.
#ifndef ADIOS_UART0_TX_PORT
#define ADIOS_UART0_TX_PORT     GPIOB
#endif
#ifndef ADIOS_UART0_TX_PIN
#define ADIOS_UART0_TX_PIN      LL_GPIO_PIN_6
#endif
#ifndef ADIOS_UART0_TX_AF
#define ADIOS_UART0_TX_AF       LL_GPIO_AF_7
#endif
#ifndef ADIOS_UART0_RX_PORT
#define ADIOS_UART0_RX_PORT     GPIOB
#endif
#ifndef ADIOS_UART0_RX_PIN
#define ADIOS_UART0_RX_PIN      LL_GPIO_PIN_7
#endif
#ifndef ADIOS_UART0_RX_AF
#define ADIOS_UART0_RX_AF       LL_GPIO_AF_7
#endif
#ifndef ADIOS_UART0
#define ADIOS_UART0             USART1
#endif
#ifndef ADIOS_UART0_IRQ_CHANNEL
#define ADIOS_UART0_IRQ_CHANNEL USART1_IRQn
#endif
#ifndef ADIOS_UART0_IRQHANDLER_FUNC
#define ADIOS_UART0_IRQHANDLER_FUNC void USART1_IRQHandler(void)
#endif
#ifndef ADIOS_UART0_CLOCK_FUNC
#define ADIOS_UART0_CLOCK_FUNC  { LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1); LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB); }
#endif
#ifndef ADIOS_UART0_APB_FREQ
#define ADIOS_UART0_APB_FREQ    84000000 // APB2
#endif

#ifndef ADIOS_UART1_TX_PORT
#define ADIOS_UART1_TX_PORT     GPIOA
#endif
#ifndef ADIOS_UART1_TX_PIN
#define ADIOS_UART1_TX_PIN      LL_GPIO_PIN_2
#endif
#ifndef ADIOS_UART1_TX_AF
#define ADIOS_UART1_TX_AF       LL_GPIO_AF_7
#endif
#ifndef ADIOS_UART1_RX_PORT
#define ADIOS_UART1_RX_PORT     GPIOA
#endif
#ifndef ADIOS_UART1_RX_PIN
#define ADIOS_UART1_RX_PIN      LL_GPIO_PIN_3
#endif
#ifndef ADIOS_UART1_RX_AF
#define ADIOS_UART1_RX_AF       LL_GPIO_AF_7
#endif
#ifndef ADIOS_UART1
#define ADIOS_UART1             USART2
#endif
#ifndef ADIOS_UART1_IRQ_CHANNEL
#define ADIOS_UART1_IRQ_CHANNEL USART2_IRQn
#endif
#ifndef ADIOS_UART1_IRQHANDLER_FUNC
#define ADIOS_UART1_IRQHANDLER_FUNC void USART2_IRQHandler(void)
#endif
#ifndef ADIOS_UART1_CLOCK_FUNC
#define ADIOS_UART1_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2); LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA); }
#endif
#ifndef ADIOS_UART1_APB_FREQ
#define ADIOS_UART1_APB_FREQ    42000000 // APB1
#endif

// UART2 (USART3 peripheral) - APB1.
#if defined(ADIOS_USE_UART2)
#ifndef ADIOS_UART2_TX_PORT
#define ADIOS_UART2_TX_PORT     GPIOB
#endif
#ifndef ADIOS_UART2_TX_PIN
#define ADIOS_UART2_TX_PIN      LL_GPIO_PIN_10
#endif
#ifndef ADIOS_UART2_TX_AF
#define ADIOS_UART2_TX_AF       LL_GPIO_AF_7
#endif
#ifndef ADIOS_UART2_RX_PORT
#define ADIOS_UART2_RX_PORT     GPIOB
#endif
#ifndef ADIOS_UART2_RX_PIN
#define ADIOS_UART2_RX_PIN      LL_GPIO_PIN_11
#endif
#ifndef ADIOS_UART2_RX_AF
#define ADIOS_UART2_RX_AF       LL_GPIO_AF_7
#endif
#ifndef ADIOS_UART2
#define ADIOS_UART2             USART3
#endif
#ifndef ADIOS_UART2_IRQ_CHANNEL
#define ADIOS_UART2_IRQ_CHANNEL USART3_IRQn
#endif
#ifndef ADIOS_UART2_IRQHANDLER_FUNC
#define ADIOS_UART2_IRQHANDLER_FUNC void USART3_IRQHandler(void)
#endif
#ifndef ADIOS_UART2_CLOCK_FUNC
#define ADIOS_UART2_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3); LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB); }
#endif
#ifndef ADIOS_UART2_APB_FREQ
#define ADIOS_UART2_APB_FREQ    42000000 // APB1
#endif
#endif

// UART3 (UART4 peripheral) - APB1.
#if defined(ADIOS_USE_UART3)
#ifndef ADIOS_UART3_TX_PORT
#define ADIOS_UART3_TX_PORT     GPIOC
#endif
#ifndef ADIOS_UART3_TX_PIN
#define ADIOS_UART3_TX_PIN      LL_GPIO_PIN_10
#endif
#ifndef ADIOS_UART3_TX_AF
#define ADIOS_UART3_TX_AF       LL_GPIO_AF_8
#endif
#ifndef ADIOS_UART3_RX_PORT
#define ADIOS_UART3_RX_PORT     GPIOC
#endif
#ifndef ADIOS_UART3_RX_PIN
#define ADIOS_UART3_RX_PIN      LL_GPIO_PIN_11
#endif
#ifndef ADIOS_UART3_RX_AF
#define ADIOS_UART3_RX_AF       LL_GPIO_AF_8
#endif
#ifndef ADIOS_UART3
#define ADIOS_UART3             UART4
#endif
#ifndef ADIOS_UART3_IRQ_CHANNEL
#define ADIOS_UART3_IRQ_CHANNEL UART4_IRQn
#endif
#ifndef ADIOS_UART3_IRQHANDLER_FUNC
#define ADIOS_UART3_IRQHANDLER_FUNC void UART4_IRQHandler(void)
#endif
#ifndef ADIOS_UART3_CLOCK_FUNC
#define ADIOS_UART3_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART4); LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC); }
#endif
#ifndef ADIOS_UART3_APB_FREQ
#define ADIOS_UART3_APB_FREQ    42000000 // APB1
#endif
#endif

// UART4 (UART5 peripheral) - APB1. TX/RX on different ports (PC12/PD2).
#if defined(ADIOS_USE_UART4)
#ifndef ADIOS_UART4_TX_PORT
#define ADIOS_UART4_TX_PORT     GPIOC
#endif
#ifndef ADIOS_UART4_TX_PIN
#define ADIOS_UART4_TX_PIN      LL_GPIO_PIN_12
#endif
#ifndef ADIOS_UART4_TX_AF
#define ADIOS_UART4_TX_AF       LL_GPIO_AF_8
#endif
#ifndef ADIOS_UART4_RX_PORT
#define ADIOS_UART4_RX_PORT     GPIOD
#endif
#ifndef ADIOS_UART4_RX_PIN
#define ADIOS_UART4_RX_PIN      LL_GPIO_PIN_2
#endif
#ifndef ADIOS_UART4_RX_AF
#define ADIOS_UART4_RX_AF       LL_GPIO_AF_8
#endif
#ifndef ADIOS_UART4
#define ADIOS_UART4             UART5
#endif
#ifndef ADIOS_UART4_IRQ_CHANNEL
#define ADIOS_UART4_IRQ_CHANNEL UART5_IRQn
#endif
#ifndef ADIOS_UART4_IRQHANDLER_FUNC
#define ADIOS_UART4_IRQHANDLER_FUNC void UART5_IRQHandler(void)
#endif
#ifndef ADIOS_UART4_CLOCK_FUNC
#define ADIOS_UART4_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART5); LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC); LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD); }
#endif
#ifndef ADIOS_UART4_APB_FREQ
#define ADIOS_UART4_APB_FREQ    42000000 // APB1
#endif
#endif

// UART5 (USART6 peripheral) - APB2 (84 MHz default clock, unlike UART1..4).
#if defined(ADIOS_USE_UART5)
#ifndef ADIOS_UART5_TX_PORT
#define ADIOS_UART5_TX_PORT     GPIOC
#endif
#ifndef ADIOS_UART5_TX_PIN
#define ADIOS_UART5_TX_PIN      LL_GPIO_PIN_6
#endif
#ifndef ADIOS_UART5_TX_AF
#define ADIOS_UART5_TX_AF       LL_GPIO_AF_8
#endif
#ifndef ADIOS_UART5_RX_PORT
#define ADIOS_UART5_RX_PORT     GPIOC
#endif
#ifndef ADIOS_UART5_RX_PIN
#define ADIOS_UART5_RX_PIN      LL_GPIO_PIN_7
#endif
#ifndef ADIOS_UART5_RX_AF
#define ADIOS_UART5_RX_AF       LL_GPIO_AF_8
#endif
#ifndef ADIOS_UART5
#define ADIOS_UART5             USART6
#endif
#ifndef ADIOS_UART5_IRQ_CHANNEL
#define ADIOS_UART5_IRQ_CHANNEL USART6_IRQn
#endif
#ifndef ADIOS_UART5_IRQHANDLER_FUNC
#define ADIOS_UART5_IRQHANDLER_FUNC void USART6_IRQHandler(void)
#endif
#ifndef ADIOS_UART5_CLOCK_FUNC
#define ADIOS_UART5_CLOCK_FUNC  { LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART6); LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC); }
#endif
#ifndef ADIOS_UART5_APB_FREQ
#define ADIOS_UART5_APB_FREQ    84000000 // APB2
#endif
#endif

// UART6 (UART7 peripheral, 8-UART tier) - force-undef above on every
// currently supported processor. Definitions kept ready for when a
// higher-tier F4 processor is added.
#if defined(ADIOS_USE_UART6)
#ifndef ADIOS_UART6_TX_PORT
#define ADIOS_UART6_TX_PORT     GPIOE
#endif
#ifndef ADIOS_UART6_TX_PIN
#define ADIOS_UART6_TX_PIN      LL_GPIO_PIN_8
#endif
#ifndef ADIOS_UART6_TX_AF
#define ADIOS_UART6_TX_AF       LL_GPIO_AF_8
#endif
#ifndef ADIOS_UART6_RX_PORT
#define ADIOS_UART6_RX_PORT     GPIOE
#endif
#ifndef ADIOS_UART6_RX_PIN
#define ADIOS_UART6_RX_PIN      LL_GPIO_PIN_7
#endif
#ifndef ADIOS_UART6_RX_AF
#define ADIOS_UART6_RX_AF       LL_GPIO_AF_8
#endif
#ifndef ADIOS_UART6
#define ADIOS_UART6             UART7
#endif
#ifndef ADIOS_UART6_IRQ_CHANNEL
#define ADIOS_UART6_IRQ_CHANNEL UART7_IRQn
#endif
#ifndef ADIOS_UART6_IRQHANDLER_FUNC
#define ADIOS_UART6_IRQHANDLER_FUNC void UART7_IRQHandler(void)
#endif
#ifndef ADIOS_UART6_CLOCK_FUNC
#define ADIOS_UART6_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART7); LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOE); }
#endif
#ifndef ADIOS_UART6_APB_FREQ
#define ADIOS_UART6_APB_FREQ    42000000 // APB1
#endif
#endif

// UART7 (UART8 peripheral, 8-UART tier) - force-undef above, see UART6.
#if defined(ADIOS_USE_UART7)
#ifndef ADIOS_UART7_TX_PORT
#define ADIOS_UART7_TX_PORT     GPIOE
#endif
#ifndef ADIOS_UART7_TX_PIN
#define ADIOS_UART7_TX_PIN      LL_GPIO_PIN_1
#endif
#ifndef ADIOS_UART7_TX_AF
#define ADIOS_UART7_TX_AF       LL_GPIO_AF_8
#endif
#ifndef ADIOS_UART7_RX_PORT
#define ADIOS_UART7_RX_PORT     GPIOE
#endif
#ifndef ADIOS_UART7_RX_PIN
#define ADIOS_UART7_RX_PIN      LL_GPIO_PIN_0
#endif
#ifndef ADIOS_UART7_RX_AF
#define ADIOS_UART7_RX_AF       LL_GPIO_AF_8
#endif
#ifndef ADIOS_UART7
#define ADIOS_UART7             UART8
#endif
#ifndef ADIOS_UART7_IRQ_CHANNEL
#define ADIOS_UART7_IRQ_CHANNEL UART8_IRQn
#endif
#ifndef ADIOS_UART7_IRQHANDLER_FUNC
#define ADIOS_UART7_IRQHANDLER_FUNC void UART8_IRQHandler(void)
#endif
#ifndef ADIOS_UART7_CLOCK_FUNC
#define ADIOS_UART7_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART8); LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOE); }
#endif
#ifndef ADIOS_UART7_APB_FREQ
#define ADIOS_UART7_APB_FREQ    42000000 // APB1
#endif
#endif

// UART8 (UART9 peripheral, 10-UART tier F413/F423 only) - force-undef
// above. APB2 (unlike UART6/UART7).
#if defined(ADIOS_USE_UART8)
#ifndef ADIOS_UART8_TX_PORT
#define ADIOS_UART8_TX_PORT     GPIOD
#endif
#ifndef ADIOS_UART8_TX_PIN
#define ADIOS_UART8_TX_PIN      LL_GPIO_PIN_15
#endif
#ifndef ADIOS_UART8_TX_AF
#define ADIOS_UART8_TX_AF       LL_GPIO_AF_11
#endif
#ifndef ADIOS_UART8_RX_PORT
#define ADIOS_UART8_RX_PORT     GPIOD
#endif
#ifndef ADIOS_UART8_RX_PIN
#define ADIOS_UART8_RX_PIN      LL_GPIO_PIN_14
#endif
#ifndef ADIOS_UART8_RX_AF
#define ADIOS_UART8_RX_AF       LL_GPIO_AF_11
#endif
#ifndef ADIOS_UART8
#define ADIOS_UART8             UART9
#endif
#ifndef ADIOS_UART8_IRQ_CHANNEL
#define ADIOS_UART8_IRQ_CHANNEL UART9_IRQn
#endif
#ifndef ADIOS_UART8_IRQHANDLER_FUNC
#define ADIOS_UART8_IRQHANDLER_FUNC void UART9_IRQHandler(void)
#endif
#ifndef ADIOS_UART8_CLOCK_FUNC
#define ADIOS_UART8_CLOCK_FUNC  { LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_UART9); LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD); }
#endif
#ifndef ADIOS_UART8_APB_FREQ
#define ADIOS_UART8_APB_FREQ    84000000 // APB2
#endif
#endif

// UART9 (UART10 peripheral, 10-UART tier F413/F423 only) - force-undef
// above. APB2.
#if defined(ADIOS_USE_UART9)
#ifndef ADIOS_UART9_TX_PORT
#define ADIOS_UART9_TX_PORT     GPIOE
#endif
#ifndef ADIOS_UART9_TX_PIN
#define ADIOS_UART9_TX_PIN      LL_GPIO_PIN_3
#endif
#ifndef ADIOS_UART9_TX_AF
#define ADIOS_UART9_TX_AF       LL_GPIO_AF_11
#endif
#ifndef ADIOS_UART9_RX_PORT
#define ADIOS_UART9_RX_PORT     GPIOE
#endif
#ifndef ADIOS_UART9_RX_PIN
#define ADIOS_UART9_RX_PIN      LL_GPIO_PIN_2
#endif
#ifndef ADIOS_UART9_RX_AF
#define ADIOS_UART9_RX_AF       LL_GPIO_AF_11
#endif
#ifndef ADIOS_UART9
#define ADIOS_UART9             UART10
#endif
#ifndef ADIOS_UART9_IRQ_CHANNEL
#define ADIOS_UART9_IRQ_CHANNEL UART10_IRQn
#endif
#ifndef ADIOS_UART9_IRQHANDLER_FUNC
#define ADIOS_UART9_IRQHANDLER_FUNC void UART10_IRQHandler(void)
#endif
#ifndef ADIOS_UART9_CLOCK_FUNC
#define ADIOS_UART9_CLOCK_FUNC  { LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_UART10); LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOE); }
#endif
#ifndef ADIOS_UART9_APB_FREQ
#define ADIOS_UART9_APB_FREQ    84000000 // APB2
#endif
#endif


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static u16 uart_assigned_to_midi;
static u32 uart_baudrate[ADIOS_UART_MAX_PORTS];

// named per-port RX/TX buffers - only allocated for ports actually enabled
// via ADIOS_USE_UARTx, unlike the small per-port state above (baudrate)
// and below (tail/head/size counters), which stay as cheap
// ADIOS_UART_MAX_PORTS-sized arrays regardless of enablement (a few bytes
// per port, not worth the indirection). The pointer tables below resolve
// each slot at compile time - NULL for any port not compiled in - so the
// existing generic array-indexed function bodies only need a NULL check
// added, no other logic changes. Matters a lot more here than on G0xx:
// ADIOS_UART_MAX_PORTS is 10, so a project using only 2 ports would
// otherwise waste 8*(64+64) = 1024 bytes of RAM on buffers it never uses.
#if defined(ADIOS_USE_UART0)
static u8 uart0_rx_buffer[ADIOS_UART_RX_BUFFER_SIZE];
static u8 uart0_tx_buffer[ADIOS_UART_TX_BUFFER_SIZE];
#endif
#if defined(ADIOS_USE_UART1)
static u8 uart1_rx_buffer[ADIOS_UART_RX_BUFFER_SIZE];
static u8 uart1_tx_buffer[ADIOS_UART_TX_BUFFER_SIZE];
#endif
#if defined(ADIOS_USE_UART2)
static u8 uart2_rx_buffer[ADIOS_UART_RX_BUFFER_SIZE];
static u8 uart2_tx_buffer[ADIOS_UART_TX_BUFFER_SIZE];
#endif
#if defined(ADIOS_USE_UART3)
static u8 uart3_rx_buffer[ADIOS_UART_RX_BUFFER_SIZE];
static u8 uart3_tx_buffer[ADIOS_UART_TX_BUFFER_SIZE];
#endif
#if defined(ADIOS_USE_UART4)
static u8 uart4_rx_buffer[ADIOS_UART_RX_BUFFER_SIZE];
static u8 uart4_tx_buffer[ADIOS_UART_TX_BUFFER_SIZE];
#endif
#if defined(ADIOS_USE_UART5)
static u8 uart5_rx_buffer[ADIOS_UART_RX_BUFFER_SIZE];
static u8 uart5_tx_buffer[ADIOS_UART_TX_BUFFER_SIZE];
#endif
#if defined(ADIOS_USE_UART6)
static u8 uart6_rx_buffer[ADIOS_UART_RX_BUFFER_SIZE];
static u8 uart6_tx_buffer[ADIOS_UART_TX_BUFFER_SIZE];
#endif
#if defined(ADIOS_USE_UART7)
static u8 uart7_rx_buffer[ADIOS_UART_RX_BUFFER_SIZE];
static u8 uart7_tx_buffer[ADIOS_UART_TX_BUFFER_SIZE];
#endif
#if defined(ADIOS_USE_UART8)
static u8 uart8_rx_buffer[ADIOS_UART_RX_BUFFER_SIZE];
static u8 uart8_tx_buffer[ADIOS_UART_TX_BUFFER_SIZE];
#endif
#if defined(ADIOS_USE_UART9)
static u8 uart9_rx_buffer[ADIOS_UART_RX_BUFFER_SIZE];
static u8 uart9_tx_buffer[ADIOS_UART_TX_BUFFER_SIZE];
#endif

static u8 * const rx_buffer_ptr[ADIOS_UART_MAX_PORTS] = {
#if defined(ADIOS_USE_UART0)
  uart0_rx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART1)
  uart1_rx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART2)
  uart2_rx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART3)
  uart3_rx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART4)
  uart4_rx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART5)
  uart5_rx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART6)
  uart6_rx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART7)
  uart7_rx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART8)
  uart8_rx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART9)
  uart9_rx_buffer,
#else
  NULL,
#endif
};
static u8 * const tx_buffer_ptr[ADIOS_UART_MAX_PORTS] = {
#if defined(ADIOS_USE_UART0)
  uart0_tx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART1)
  uart1_tx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART2)
  uart2_tx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART3)
  uart3_tx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART4)
  uart4_tx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART5)
  uart5_tx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART6)
  uart6_tx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART7)
  uart7_tx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART8)
  uart8_tx_buffer,
#else
  NULL,
#endif
#if defined(ADIOS_USE_UART9)
  uart9_tx_buffer,
#else
  NULL,
#endif
};

static volatile u8 rx_buffer_tail[ADIOS_UART_MAX_PORTS];
static volatile u8 rx_buffer_head[ADIOS_UART_MAX_PORTS];
static volatile u8 rx_buffer_size[ADIOS_UART_MAX_PORTS];

static volatile u8 tx_buffer_tail[ADIOS_UART_MAX_PORTS];
static volatile u8 tx_buffer_head[ADIOS_UART_MAX_PORTS];
static volatile u8 tx_buffer_size[ADIOS_UART_MAX_PORTS];


/////////////////////////////////////////////////////////////////////////////
//! Initializes UART interfaces
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
//! \note Applications shouldn't call this function directly, instead please use \ref ADIOS_COM or \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_Init(u32 mode)
{
  // currently only mode 0 supported
  if( mode != 0 )
    return -1; // unsupported mode

  // map UART pins and enable clocks
#if defined(ADIOS_USE_UART0) && ADIOS_UART0_ASSIGNMENT != 0
  ADIOS_UART0_CLOCK_FUNC;
#endif
#if defined(ADIOS_USE_UART1) && ADIOS_UART1_ASSIGNMENT != 0
  ADIOS_UART1_CLOCK_FUNC
#endif
#if defined(ADIOS_USE_UART2) && ADIOS_UART2_ASSIGNMENT != 0
  ADIOS_UART2_CLOCK_FUNC
#endif
#if defined(ADIOS_USE_UART3) && ADIOS_UART3_ASSIGNMENT != 0
  ADIOS_UART3_CLOCK_FUNC
#endif
#if defined(ADIOS_USE_UART4) && ADIOS_UART4_ASSIGNMENT != 0
  ADIOS_UART4_CLOCK_FUNC
#endif
#if defined(ADIOS_USE_UART5) && ADIOS_UART5_ASSIGNMENT != 0
  ADIOS_UART5_CLOCK_FUNC
#endif
#if defined(ADIOS_USE_UART6) && ADIOS_UART6_ASSIGNMENT != 0
  ADIOS_UART6_CLOCK_FUNC
#endif
#if defined(ADIOS_USE_UART7) && ADIOS_UART7_ASSIGNMENT != 0
  ADIOS_UART7_CLOCK_FUNC
#endif
#if defined(ADIOS_USE_UART8) && ADIOS_UART8_ASSIGNMENT != 0
  ADIOS_UART8_CLOCK_FUNC
#endif
#if defined(ADIOS_USE_UART9) && ADIOS_UART9_ASSIGNMENT != 0
  ADIOS_UART9_CLOCK_FUNC
#endif

  // initialize UARTs and clear buffers
  {
    u8 uart;
    for(uart=0; uart<ADIOS_UART_MAX_PORTS; ++uart) {
      rx_buffer_tail[uart] = rx_buffer_head[uart] = rx_buffer_size[uart] = 0;
      tx_buffer_tail[uart] = tx_buffer_head[uart] = tx_buffer_size[uart] = 0;

      ADIOS_UART_InitPortDefault(uart);
    }
  }

  // configure and enable UART interrupts
#if defined(ADIOS_USE_UART0) && ADIOS_UART0_ASSIGNMENT != 0
  ADIOS_IRQ_Install(ADIOS_UART0_IRQ_CHANNEL, ADIOS_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(ADIOS_UART0);
#endif
#if defined(ADIOS_USE_UART1) && ADIOS_UART1_ASSIGNMENT != 0
  ADIOS_IRQ_Install(ADIOS_UART1_IRQ_CHANNEL, ADIOS_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(ADIOS_UART1);
#endif
#if defined(ADIOS_USE_UART2) && ADIOS_UART2_ASSIGNMENT != 0
  ADIOS_IRQ_Install(ADIOS_UART2_IRQ_CHANNEL, ADIOS_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(ADIOS_UART2);
#endif
#if defined(ADIOS_USE_UART3) && ADIOS_UART3_ASSIGNMENT != 0
  ADIOS_IRQ_Install(ADIOS_UART3_IRQ_CHANNEL, ADIOS_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(ADIOS_UART3);
#endif
#if defined(ADIOS_USE_UART4) && ADIOS_UART4_ASSIGNMENT != 0
  ADIOS_IRQ_Install(ADIOS_UART4_IRQ_CHANNEL, ADIOS_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(ADIOS_UART4);
#endif
#if defined(ADIOS_USE_UART5) && ADIOS_UART5_ASSIGNMENT != 0
  ADIOS_IRQ_Install(ADIOS_UART5_IRQ_CHANNEL, ADIOS_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(ADIOS_UART5);
#endif
#if defined(ADIOS_USE_UART6) && ADIOS_UART6_ASSIGNMENT != 0
  ADIOS_IRQ_Install(ADIOS_UART6_IRQ_CHANNEL, ADIOS_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(ADIOS_UART6);
#endif
#if defined(ADIOS_USE_UART7) && ADIOS_UART7_ASSIGNMENT != 0
  ADIOS_IRQ_Install(ADIOS_UART7_IRQ_CHANNEL, ADIOS_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(ADIOS_UART7);
#endif
#if defined(ADIOS_USE_UART8) && ADIOS_UART8_ASSIGNMENT != 0
  ADIOS_IRQ_Install(ADIOS_UART8_IRQ_CHANNEL, ADIOS_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(ADIOS_UART8);
#endif
#if defined(ADIOS_USE_UART9) && ADIOS_UART9_ASSIGNMENT != 0
  ADIOS_IRQ_Install(ADIOS_UART9_IRQ_CHANNEL, ADIOS_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(ADIOS_UART9);
#endif

  // enable UARTs
#if defined(ADIOS_USE_UART0) && ADIOS_UART0_ASSIGNMENT != 0
  LL_USART_Enable(ADIOS_UART0);
#endif
#if defined(ADIOS_USE_UART1) && ADIOS_UART1_ASSIGNMENT != 0
  LL_USART_Enable(ADIOS_UART1);
#endif
#if defined(ADIOS_USE_UART2) && ADIOS_UART2_ASSIGNMENT != 0
  LL_USART_Enable(ADIOS_UART2);
#endif
#if defined(ADIOS_USE_UART3) && ADIOS_UART3_ASSIGNMENT != 0
  LL_USART_Enable(ADIOS_UART3);
#endif
#if defined(ADIOS_USE_UART4) && ADIOS_UART4_ASSIGNMENT != 0
  LL_USART_Enable(ADIOS_UART4);
#endif
#if defined(ADIOS_USE_UART5) && ADIOS_UART5_ASSIGNMENT != 0
  LL_USART_Enable(ADIOS_UART5);
#endif
#if defined(ADIOS_USE_UART6) && ADIOS_UART6_ASSIGNMENT != 0
  LL_USART_Enable(ADIOS_UART6);
#endif
#if defined(ADIOS_USE_UART7) && ADIOS_UART7_ASSIGNMENT != 0
  LL_USART_Enable(ADIOS_UART7);
#endif
#if defined(ADIOS_USE_UART8) && ADIOS_UART8_ASSIGNMENT != 0
  LL_USART_Enable(ADIOS_UART8);
#endif
#if defined(ADIOS_USE_UART9) && ADIOS_UART9_ASSIGNMENT != 0
  LL_USART_Enable(ADIOS_UART9);
#endif

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! \return 0 if UART is not assigned to a MIDI function
//! \return 1 if UART is assigned to a MIDI function
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_IsAssignedToMIDI(u8 uart)
{
  if( uart >= ADIOS_UART_MAX_PORTS )
    return 0; // no UART available
  return (uart_assigned_to_midi & (1 << uart)) ? 1 : 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Initializes a given UART interface based on given baudrate and TX output mode
//! \param[in] uart UART number (0..9)
//! \param[in] baudrate the baudrate
//! \param[in] tx_pin_mode the TX pin mode
//!   <UL>
//!     <LI>ADIOS_PIN_MODE_OUTPUT_PP: TX pin configured for push-pull mode
//!     <LI>ADIOS_PIN_MODE_OUTPUT_OD: TX pin configured for open drain mode
//!   </UL>
//! \param[in] is_midi MIDI or common UART interface?
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_InitPort(u8 uart, u32 baudrate, adios_pin_mode_t tx_pin_mode, u8 is_midi)
{
  LL_GPIO_InitTypeDef GPIO_InitStructure;
  LL_GPIO_StructInit(&GPIO_InitStructure);
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_HIGH;

  if( uart >= ADIOS_UART_MAX_PORTS )
    return -1; // unsupported UART

  // MIDI assignment
  if( is_midi ) {
    uart_assigned_to_midi |= (1 << uart);
  } else {
    uart_assigned_to_midi &= ~(1 << uart);
  }

#define ADIOS_UART_INITPORT_CASE(n) \
  case n: { \
    GPIO_InitStructure.Pin = ADIOS_UART##n##_TX_PIN; \
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE; \
    GPIO_InitStructure.OutputType = (tx_pin_mode == ADIOS_PIN_MODE_OUTPUT_PP) ? LL_GPIO_OUTPUT_PUSHPULL : LL_GPIO_OUTPUT_OPENDRAIN; \
    GPIO_InitStructure.Pull = LL_GPIO_PULL_NO; \
    GPIO_InitStructure.Alternate = ADIOS_UART##n##_TX_AF; \
    LL_GPIO_Init(ADIOS_UART##n##_TX_PORT, &GPIO_InitStructure); \
    GPIO_InitStructure.Pin = ADIOS_UART##n##_RX_PIN; \
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE; \
    GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL; \
    GPIO_InitStructure.Pull  = LL_GPIO_PULL_UP; \
    GPIO_InitStructure.Alternate = ADIOS_UART##n##_RX_AF; \
    LL_GPIO_Init(ADIOS_UART##n##_RX_PORT, &GPIO_InitStructure); \
    ADIOS_UART_BaudrateSet(uart, baudrate); \
  } break;

  switch( uart ) {
#if defined(ADIOS_USE_UART0) && ADIOS_UART0_ASSIGNMENT != 0
  ADIOS_UART_INITPORT_CASE(0)
#endif
#if defined(ADIOS_USE_UART1) && ADIOS_UART1_ASSIGNMENT != 0
  ADIOS_UART_INITPORT_CASE(1)
#endif
#if defined(ADIOS_USE_UART2) && ADIOS_UART2_ASSIGNMENT != 0
  ADIOS_UART_INITPORT_CASE(2)
#endif
#if defined(ADIOS_USE_UART3) && ADIOS_UART3_ASSIGNMENT != 0
  ADIOS_UART_INITPORT_CASE(3)
#endif
#if defined(ADIOS_USE_UART4) && ADIOS_UART4_ASSIGNMENT != 0
  ADIOS_UART_INITPORT_CASE(4)
#endif
#if defined(ADIOS_USE_UART5) && ADIOS_UART5_ASSIGNMENT != 0
  ADIOS_UART_INITPORT_CASE(5)
#endif
#if defined(ADIOS_USE_UART6) && ADIOS_UART6_ASSIGNMENT != 0
  ADIOS_UART_INITPORT_CASE(6)
#endif
#if defined(ADIOS_USE_UART7) && ADIOS_UART7_ASSIGNMENT != 0
  ADIOS_UART_INITPORT_CASE(7)
#endif
#if defined(ADIOS_USE_UART8) && ADIOS_UART8_ASSIGNMENT != 0
  ADIOS_UART_INITPORT_CASE(8)
#endif
#if defined(ADIOS_USE_UART9) && ADIOS_UART9_ASSIGNMENT != 0
  ADIOS_UART_INITPORT_CASE(9)
#endif

  default:
    return -1; // unsupported UART
  }

#undef ADIOS_UART_INITPORT_CASE

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Initializes a given UART interface based on default settings
//! \param[in] uart UART number (0..9)
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_InitPortDefault(u8 uart)
{
#define ADIOS_UART_INITPORTDEFAULT_CASE(n) \
  case n: { \
    if( ADIOS_UART##n##_TX_OD ) \
      ADIOS_UART_InitPort(n, ADIOS_UART##n##_BAUDRATE, ADIOS_PIN_MODE_OUTPUT_OD, ADIOS_UART##n##_ASSIGNMENT == 1); \
    else \
      ADIOS_UART_InitPort(n, ADIOS_UART##n##_BAUDRATE, ADIOS_PIN_MODE_OUTPUT_PP, ADIOS_UART##n##_ASSIGNMENT == 1); \
  } break;

  switch( uart ) {
#if defined(ADIOS_USE_UART0) && ADIOS_UART0_ASSIGNMENT != 0
  ADIOS_UART_INITPORTDEFAULT_CASE(0)
#endif
#if defined(ADIOS_USE_UART1) && ADIOS_UART1_ASSIGNMENT != 0
  ADIOS_UART_INITPORTDEFAULT_CASE(1)
#endif
#if defined(ADIOS_USE_UART2) && ADIOS_UART2_ASSIGNMENT != 0
  ADIOS_UART_INITPORTDEFAULT_CASE(2)
#endif
#if defined(ADIOS_USE_UART3) && ADIOS_UART3_ASSIGNMENT != 0
  ADIOS_UART_INITPORTDEFAULT_CASE(3)
#endif
#if defined(ADIOS_USE_UART4) && ADIOS_UART4_ASSIGNMENT != 0
  ADIOS_UART_INITPORTDEFAULT_CASE(4)
#endif
#if defined(ADIOS_USE_UART5) && ADIOS_UART5_ASSIGNMENT != 0
  ADIOS_UART_INITPORTDEFAULT_CASE(5)
#endif
#if defined(ADIOS_USE_UART6) && ADIOS_UART6_ASSIGNMENT != 0
  ADIOS_UART_INITPORTDEFAULT_CASE(6)
#endif
#if defined(ADIOS_USE_UART7) && ADIOS_UART7_ASSIGNMENT != 0
  ADIOS_UART_INITPORTDEFAULT_CASE(7)
#endif
#if defined(ADIOS_USE_UART8) && ADIOS_UART8_ASSIGNMENT != 0
  ADIOS_UART_INITPORTDEFAULT_CASE(8)
#endif
#if defined(ADIOS_USE_UART9) && ADIOS_UART9_ASSIGNMENT != 0
  ADIOS_UART_INITPORTDEFAULT_CASE(9)
#endif

  default:
    return -1; // unsupported UART
  }

#undef ADIOS_UART_INITPORTDEFAULT_CASE

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! sets the baudrate of a UART port
//! \param[in] uart UART number (0..9)
//! \param[in] baudrate the baudrate
//! \return 0: baudrate has been changed
//! \return -1: uart not available
//! \return -2: function not prepared for this UART
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_BaudrateSet(u8 uart, u32 baudrate)
{
  if( uart >= ADIOS_UART_MAX_PORTS )
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

#define ADIOS_UART_BAUDRATESET_CASE(n) \
  case n: \
    LL_USART_Init(ADIOS_UART##n, &USART_InitStructure); \
    LL_USART_ConfigAsyncMode(ADIOS_UART##n); \
    LL_USART_SetBaudRate(ADIOS_UART##n, ADIOS_UART##n##_APB_FREQ, LL_USART_OVERSAMPLING_16, baudrate); \
    break;

  switch( uart ) {
#if defined(ADIOS_USE_UART0)
  ADIOS_UART_BAUDRATESET_CASE(0)
#endif
#if defined(ADIOS_USE_UART1)
  ADIOS_UART_BAUDRATESET_CASE(1)
#endif
#if defined(ADIOS_USE_UART2)
  ADIOS_UART_BAUDRATESET_CASE(2)
#endif
#if defined(ADIOS_USE_UART3)
  ADIOS_UART_BAUDRATESET_CASE(3)
#endif
#if defined(ADIOS_USE_UART4)
  ADIOS_UART_BAUDRATESET_CASE(4)
#endif
#if defined(ADIOS_USE_UART5)
  ADIOS_UART_BAUDRATESET_CASE(5)
#endif
#if defined(ADIOS_USE_UART6)
  ADIOS_UART_BAUDRATESET_CASE(6)
#endif
#if defined(ADIOS_USE_UART7)
  ADIOS_UART_BAUDRATESET_CASE(7)
#endif
#if defined(ADIOS_USE_UART8)
  ADIOS_UART_BAUDRATESET_CASE(8)
#endif
#if defined(ADIOS_USE_UART9)
  ADIOS_UART_BAUDRATESET_CASE(9)
#endif
  default:
    return -2; // not prepared
  }

#undef ADIOS_UART_BAUDRATESET_CASE

  // store baudrate in array
  uart_baudrate[uart] = baudrate;

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//! returns the current baudrate of a UART port
//! \param[in] uart UART number (0..9)
//! \return 0: uart not available
//! \return all other values: the current baudrate
/////////////////////////////////////////////////////////////////////////////
u32 ADIOS_UART_BaudrateGet(u8 uart)
{
  if( uart >= ADIOS_UART_MAX_PORTS )
    return 0;
  return uart_baudrate[uart];
}


/////////////////////////////////////////////////////////////////////////////
//! returns number of free bytes in receive buffer
//! \param[in] uart UART number (0..9)
//! \return uart number of free bytes
//! \return 1: uart available
//! \return 0: uart not available
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_RxBufferFree(u8 uart)
{
  if( uart >= ADIOS_UART_MAX_PORTS )
    return 0;
  return ADIOS_UART_RX_BUFFER_SIZE - rx_buffer_size[uart];
}


/////////////////////////////////////////////////////////////////////////////
//! returns number of used bytes in receive buffer
//! \param[in] uart UART number (0..9)
//! \return > 0: number of used bytes
//! \return 0 if uart not available
//! \note Applications shouldn't call these functions directly, instead please use \ref ADIOS_COM or \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_RxBufferUsed(u8 uart)
{
  if( uart >= ADIOS_UART_MAX_PORTS )
    return 0;
  return rx_buffer_size[uart];
}


/////////////////////////////////////////////////////////////////////////////
//! gets a byte from the receive buffer
//! \param[in] uart UART number (0..9)
//! \return -1 if UART not available
//! \return -2 if no new byte available
//! \return >= 0: number of received bytes
//! \note Applications shouldn't call these functions directly, instead please use \ref ADIOS_COM or \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_RxBufferGet(u8 uart)
{
  if( uart >= ADIOS_UART_MAX_PORTS || rx_buffer_ptr[uart] == NULL )
    return -1; // UART not available

  if( !rx_buffer_size[uart] )
    return -2; // nothing new in buffer

  // get byte - this operation should be atomic!
  ADIOS_IRQ_Disable();
  u8 b = rx_buffer_ptr[uart][rx_buffer_tail[uart]];
  if( ++rx_buffer_tail[uart] >= ADIOS_UART_RX_BUFFER_SIZE )
    rx_buffer_tail[uart] = 0;
  --rx_buffer_size[uart];
  ADIOS_IRQ_Enable();

  return b; // return received byte
}


/////////////////////////////////////////////////////////////////////////////
//! returns the next byte of the receive buffer without taking it
//! \param[in] uart UART number (0..9)
//! \return -1 if UART not available
//! \return -2 if no new byte available
//! \return >= 0: number of received bytes
//! \note Applications shouldn't call these functions directly, instead please use \ref ADIOS_COM or \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_RxBufferPeek(u8 uart)
{
  if( uart >= ADIOS_UART_MAX_PORTS || rx_buffer_ptr[uart] == NULL )
    return -1; // UART not available

  if( !rx_buffer_size[uart] )
    return -2; // nothing new in buffer

  // get byte - this operation should be atomic!
  ADIOS_IRQ_Disable();
  u8 b = rx_buffer_ptr[uart][rx_buffer_tail[uart]];
  ADIOS_IRQ_Enable();

  return b; // return received byte
}


/////////////////////////////////////////////////////////////////////////////
//! puts a byte onto the receive buffer
//! \param[in] uart UART number (0..9)
//! \param[in] b byte which should be put into Rx buffer
//! \return 0 if no error
//! \return -1 if UART not available
//! \return -2 if buffer full (retry)
//! \note Applications shouldn't call these functions directly, instead please use \ref ADIOS_COM or \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_RxBufferPut(u8 uart, u8 b)
{
  if( uart >= ADIOS_UART_MAX_PORTS || rx_buffer_ptr[uart] == NULL )
    return -1; // UART not available

  if( rx_buffer_size[uart] >= ADIOS_UART_RX_BUFFER_SIZE )
    return -2; // buffer full (retry)

  // copy received byte into receive buffer
  // this operation should be atomic!
  ADIOS_IRQ_Disable();
  rx_buffer_ptr[uart][rx_buffer_head[uart]] = b;
  if( ++rx_buffer_head[uart] >= ADIOS_UART_RX_BUFFER_SIZE )
    rx_buffer_head[uart] = 0;
  ++rx_buffer_size[uart];
  ADIOS_IRQ_Enable();

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! returns number of free bytes in transmit buffer
//! \param[in] uart UART number (0..9)
//! \return number of free bytes
//! \return 0 if uart not available
//! \note Applications shouldn't call these functions directly, instead please use \ref ADIOS_COM or \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_TxBufferFree(u8 uart)
{
  if( uart >= ADIOS_UART_MAX_PORTS )
    return 0;
  return ADIOS_UART_TX_BUFFER_SIZE - tx_buffer_size[uart];
}


/////////////////////////////////////////////////////////////////////////////
//! returns number of used bytes in transmit buffer
//! \param[in] uart UART number (0..9)
//! \return number of used bytes
//! \return 0 if uart not available
//! \note Applications shouldn't call these functions directly, instead please use \ref ADIOS_COM or \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_TxBufferUsed(u8 uart)
{
  if( uart >= ADIOS_UART_MAX_PORTS )
    return 0;
  return tx_buffer_size[uart];
}


/////////////////////////////////////////////////////////////////////////////
//! gets a byte from the transmit buffer
//! \param[in] uart UART number (0..9)
//! \return -1 if UART not available
//! \return -2 if no new byte available
//! \return >= 0: transmitted byte
//! \note Applications shouldn't call these functions directly, instead please use \ref ADIOS_COM or \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_TxBufferGet(u8 uart)
{
  if( uart >= ADIOS_UART_MAX_PORTS || tx_buffer_ptr[uart] == NULL )
    return -1; // UART not available

  if( !tx_buffer_size[uart] )
    return -2; // nothing new in buffer

  // get byte - this operation should be atomic!
  ADIOS_IRQ_Disable();
  u8 b = tx_buffer_ptr[uart][tx_buffer_tail[uart]];
  if( ++tx_buffer_tail[uart] >= ADIOS_UART_TX_BUFFER_SIZE )
    tx_buffer_tail[uart] = 0;
  --tx_buffer_size[uart];
  ADIOS_IRQ_Enable();

  return b; // return transmitted byte
}


/////////////////////////////////////////////////////////////////////////////
//! puts more than one byte onto the transmit buffer (used for atomic sends)
//! \param[in] uart UART number (0..9)
//! \param[in] *buffer pointer to buffer to be sent
//! \param[in] len number of bytes to be sent
//! \return 0 if no error
//! \return -1 if UART not available
//! \return -2 if buffer full or cannot get all requested bytes (retry)
//! \return -3 if UART not supported by ADIOS_UART_TxBufferPut Routine
//! \note Applications shouldn't call these functions directly, instead please use \ref ADIOS_COM or \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_TxBufferPutMore_NonBlocking(u8 uart, u8 *buffer, u16 len)
{
  if( uart >= ADIOS_UART_MAX_PORTS || tx_buffer_ptr[uart] == NULL )
    return -1; // UART not available

  if( (tx_buffer_size[uart]+len) >= ADIOS_UART_TX_BUFFER_SIZE )
    return -2; // buffer full or cannot get all requested bytes (retry)

  // copy bytes to be transmitted into transmit buffer
  // this operation should be atomic!
  ADIOS_IRQ_Disable();

  u16 i;
  for(i=0; i<len; ++i) {
    tx_buffer_ptr[uart][tx_buffer_head[uart]] = *buffer++;

    if( ++tx_buffer_head[uart] >= ADIOS_UART_TX_BUFFER_SIZE )
      tx_buffer_head[uart] = 0;

    // enable Tx interrupt if buffer was empty
    if( ++tx_buffer_size[uart] == 1 ) {
      switch( uart ) {
#if defined(ADIOS_USE_UART0)
        case 0: ADIOS_UART0->CR1 |= (1 << 7); break; // enable TXE interrupt (TXEIE=1)
#endif
#if defined(ADIOS_USE_UART1)
        case 1: ADIOS_UART1->CR1 |= (1 << 7); break;
#endif
#if defined(ADIOS_USE_UART2)
        case 2: ADIOS_UART2->CR1 |= (1 << 7); break;
#endif
#if defined(ADIOS_USE_UART3)
        case 3: ADIOS_UART3->CR1 |= (1 << 7); break;
#endif
#if defined(ADIOS_USE_UART4)
        case 4: ADIOS_UART4->CR1 |= (1 << 7); break;
#endif
#if defined(ADIOS_USE_UART5)
        case 5: ADIOS_UART5->CR1 |= (1 << 7); break;
#endif
#if defined(ADIOS_USE_UART6)
        case 6: ADIOS_UART6->CR1 |= (1 << 7); break;
#endif
#if defined(ADIOS_USE_UART7)
        case 7: ADIOS_UART7->CR1 |= (1 << 7); break;
#endif
#if defined(ADIOS_USE_UART8)
        case 8: ADIOS_UART8->CR1 |= (1 << 7); break;
#endif
#if defined(ADIOS_USE_UART9)
        case 9: ADIOS_UART9->CR1 |= (1 << 7); break;
#endif
        default: ADIOS_IRQ_Enable(); return -3; // uart not supported by routine (yet)
      }
    }
  }

  ADIOS_IRQ_Enable();

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! puts more than one byte onto the transmit buffer (used for atomic sends)<BR>
//! (blocking function)
//! \param[in] uart UART number (0..9)
//! \param[in] *buffer pointer to buffer to be sent
//! \param[in] len number of bytes to be sent
//! \return 0 if no error
//! \return -1 if UART not available
//! \return -3 if UART not supported by ADIOS_UART_TxBufferPut Routine
//! \note Applications shouldn't call these functions directly, instead please use \ref ADIOS_COM or \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_TxBufferPutMore(u8 uart, u8 *buffer, u16 len)
{
  s32 error;

  while( (error=ADIOS_UART_TxBufferPutMore_NonBlocking(uart, buffer, len)) == -2 );

  return error;
}


/////////////////////////////////////////////////////////////////////////////
//! puts a byte onto the transmit buffer
//! \param[in] uart UART number (0..9)
//! \param[in] b byte which should be put into Tx buffer
//! \return 0 if no error
//! \return -1 if UART not available
//! \return -2 if buffer full (retry)
//! \return -3 if UART not supported by ADIOS_UART_TxBufferPut Routine
//! \note Applications shouldn't call these functions directly, instead please use \ref ADIOS_COM or \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_TxBufferPut_NonBlocking(u8 uart, u8 b)
{
  // for more comfortable usage...
  // -> just forward to ADIOS_UART_TxBufferPutMore
  return ADIOS_UART_TxBufferPutMore(uart, &b, 1);
}


/////////////////////////////////////////////////////////////////////////////
//! puts a byte onto the transmit buffer<BR>
//! (blocking function)
//! \param[in] uart UART number (0..9)
//! \param[in] b byte which should be put into Tx buffer
//! \return 0 if no error
//! \return -1 if UART not available
//! \return -3 if UART not supported by ADIOS_UART_TxBufferPut Routine
//! \note Applications shouldn't call these functions directly, instead please use \ref ADIOS_COM or \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_UART_TxBufferPut(u8 uart, u8 b)
{
  s32 error;

  while( (error=ADIOS_UART_TxBufferPutMore(uart, &b, 1)) == -2 );

  return error;
}


/////////////////////////////////////////////////////////////////////////////
// Interrupt handlers - one per port, every F4 UART/USART has its own
// independent NVIC vector (unlike STM32G0xx, no shared-handler complexity)
/////////////////////////////////////////////////////////////////////////////
#define ADIOS_UART_IRQHANDLER_BODY(n) \
  if( ADIOS_UART##n->SR & (1 << 5) ) { \
    u8 b = ADIOS_UART##n->DR; \
    s32 status = ADIOS_UART_IsAssignedToMIDI(n) ? ADIOS_MIDI_SendByteToRxCallback(DIN##n, b) : 0; \
    if( status == 0 && ADIOS_UART_RxBufferPut(n, b) < 0 ) { \
    } \
  } \
  if( ADIOS_UART##n->SR & (1 << 7) ) { \
    if( ADIOS_UART_TxBufferUsed(n) > 0 ) { \
      s32 b = ADIOS_UART_TxBufferGet(n); \
      if( b < 0 ) { \
        ADIOS_UART##n->DR = 0xff; \
      } else { \
        ADIOS_UART##n->DR = b; \
      } \
    } else { \
      ADIOS_UART##n->CR1 &= ~(1 << 7); \
    } \
  }

#if defined(ADIOS_USE_UART0)
ADIOS_UART0_IRQHANDLER_FUNC { ADIOS_UART_IRQHANDLER_BODY(0) }
#endif
#if defined(ADIOS_USE_UART1)
ADIOS_UART1_IRQHANDLER_FUNC { ADIOS_UART_IRQHANDLER_BODY(1) }
#endif
#if defined(ADIOS_USE_UART2)
ADIOS_UART2_IRQHANDLER_FUNC { ADIOS_UART_IRQHANDLER_BODY(2) }
#endif
#if defined(ADIOS_USE_UART3)
ADIOS_UART3_IRQHANDLER_FUNC { ADIOS_UART_IRQHANDLER_BODY(3) }
#endif
#if defined(ADIOS_USE_UART4)
ADIOS_UART4_IRQHANDLER_FUNC { ADIOS_UART_IRQHANDLER_BODY(4) }
#endif
#if defined(ADIOS_USE_UART5)
ADIOS_UART5_IRQHANDLER_FUNC { ADIOS_UART_IRQHANDLER_BODY(5) }
#endif
#if defined(ADIOS_USE_UART6)
ADIOS_UART6_IRQHANDLER_FUNC { ADIOS_UART_IRQHANDLER_BODY(6) }
#endif
#if defined(ADIOS_USE_UART7)
ADIOS_UART7_IRQHANDLER_FUNC { ADIOS_UART_IRQHANDLER_BODY(7) }
#endif
#if defined(ADIOS_USE_UART8)
ADIOS_UART8_IRQHANDLER_FUNC { ADIOS_UART_IRQHANDLER_BODY(8) }
#endif
#if defined(ADIOS_USE_UART9)
ADIOS_UART9_IRQHANDLER_FUNC { ADIOS_UART_IRQHANDLER_BODY(9) }
#endif

#undef ADIOS_UART_IRQHANDLER_BODY

//! \}

#endif /* ADIOS_USE_UART */
