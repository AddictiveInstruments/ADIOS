// $Id: mios32_uart.c 2312 2016-02-27 23:04:51Z tk $
//! \defgroup MIOS32_UART
//!
//! U(S)ART functions for MIOS32 - STM32G0xx
//!
//! Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
//!
//! Up to 8 ports are supported: the 6 USART peripherals present on the
//! 6-USART G0 chip WITHOUT LPUART (STM32G0B0), plus UART6/UART7 which alias
//! LPUART1/LPUART2 on the "+LPUART" sibling tiers (see below). Coverage by
//! chip tier (verified against the CMSIS device-tier macros - STM32G070xx
//! etc, one of which stm32g0xx.h's own dispatcher requires be defined - not
//! the exact MIOS32_PROCESSOR_STM32G070CB part number, so every flash/pin
//! variant of the same silicon is covered automatically).
//!
//! Port numbering follows ST's own peripheral numbering: MIOS32_UARTn is
//! USART(n+1) - UART0=USART1, UART1=USART2, ... UART5=USART6 - then
//! UART6/UART7 for LPUART1/LPUART2. Until 2026-08-10 the mapping was
//! inherited from the MBHP core's connector wiring instead (UART0 was
//! USART3), which made every project config read like a puzzle and silently
//! dropped MIDI on the 2-USART chips, where USART3 doesn't exist. Projects
//! written against the old numbering must renumber their MIOS32_USE_UARTx
//! and any pin/peripheral overrides.
//!
//! Coverage by chip tier:
//!   - 2-USART (G030/G050): UART0/UART1 only (USART1/USART2).
//!   - 2-USART+LPUART1 (G031/G041/G051/G061): UART0/UART1, plus UART6
//!     (LPUART1) on its own independent vector (LPUART1_IRQn) - not shared
//!     with anything.
//!   - 4-USART (G070): UART0..UART3 (USART1..USART4); USART3+4 share one
//!     NVIC vector (USART3_4_IRQn).
//!   - 4-USART+LPUART1 (G071/G081): UART0..UART3 like G070, plus UART6
//!     (LPUART1) - but here USART3+4+LPUART1 share ONE vector together
//!     (USART3_4_LPUART1_IRQn, a different name than the plain 4-USART
//!     tier's USART3_4_IRQn even though the port count is the same).
//!   - 6-USART (G0B0): UART0..UART5 (USART1..USART6); USART3+4+5+6 share one
//!     NVIC vector (USART3_4_5_6_IRQn) - a 4-way share, not the same vector
//!     name as the 4-USART tier above, hence the tier-conditional
//!     MIOS32_UART_SHARED_IRQ_CHANNEL/IRQHANDLER_FUNC macros below.
//!   - 6-USART+2xLPUART (G0B1/G0C1): UART0..UART5 like G0B0, plus UART6
//!     (LPUART1, sharing USART3_4_5_6_LPUART1_IRQn with UART2/3/4/5) and
//!     UART7 (LPUART2) - which, unusually, shares UART1's own vector
//!     instead (USART2_LPUART2_IRQn) - USART2 is NOT independent on this
//!     one tier, unlike every other G0 tier. See MIOS32_UART1_IRQ_CHANNEL/
//!     IRQHANDLER_FUNC below, which are tier-conditional for this reason.
//!
//! LPUART registers alias the same USART_TypeDef layout (ISR/RDR/TDR/CR1 -
//! confirmed in CMSIS: LPUARTx is literally "(USART_TypeDef *)LPUARTx_BASE"),
//! so the generic RXNE/TXE interrupt handling and raw buffer routines below
//! work unchanged for UART6/UART7. Only initialisation differs: LPUART uses
//! its own LL_LPUART_Init/fixed x256 oversampling/PRESC-based BRR formula
//! (see MIOS32_UART_BaudrateSet), not LL_USART_Init. Both LPUART1 and
//! LPUART2 default to PCLK1 as their RCC clock source (MIOS32_UART6/7_
//! CLOCK_SOURCE, overridable) - the same source the other ports already use
//! by default - not LSE: LSE's 32.768kHz is far too slow for a MIDI baud
//! rate (fclk/baudrate ratio would be ~1, LPUART requires >=~3), and the
//! Stop-mode wakeup LSE would otherwise enable isn't used by this driver.
//!
//! Default pin/peripheral assignment (STM32G070CB) - override individually in a
//! local mios32_config.h if your project uses different pins or a different UART
//! peripheral (e.g. STM32G050K8):
//! \code
//!   #define MIOS32_UART0_TX_PORT     GPIOB
//!   #define MIOS32_UART0_TX_PIN      LL_GPIO_PIN_6
//!   #define MIOS32_UART0_TX_AF       LL_GPIO_AF_0
//!   #define MIOS32_UART0_RX_PORT     GPIOB
//!   #define MIOS32_UART0_RX_PIN      LL_GPIO_PIN_7
//!   #define MIOS32_UART0_RX_AF       LL_GPIO_AF_0
//!   #define MIOS32_UART0             USART1
//!   #define MIOS32_UART0_IRQ_CHANNEL USART1_IRQn
//!   #define MIOS32_UART0_IRQHANDLER_FUNC void USART1_IRQHandler(void)
//!   #define MIOS32_UART0_RESET_FUNC  { ... }
//!   #define MIOS32_UART0_CLOCK_FUNC  { ... }
//! \endcode
//! (same set of defines exists for MIOS32_UART1..UART7)
//!
//! If your board runs the TX pin through an external level-shifting stage
//! that inverts the signal (e.g. a 3V3->5V transistor for a MIDI DIN output),
//! define MIOS32_UARTx_TX_INVERTED for that port - every port defaults to
//! normal (non-inverted) polarity, this is a per-project hardware fact, not
//! a peripheral default, so it's opt-in only:
//! \code
//!   #define MIOS32_UART2_TX_INVERTED
//! \endcode
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

// USART1 (UART0) and USART2 (UART1) exist on EVERY G0 chip - never gated.

// USART3 (UART2) and USART4 (UART3) exist on the 4-USART tier (G070/G071/
// G081) and the 6-USART tier (G0B0/G0B1/G0C1) - checked via the CMSIS
// device-tier macro (defined by stm32g0xx.h's own dispatcher once
// MIOS32_PROCESSOR_xxx has selected a device header) rather than the exact
// MIOS32_PROCESSOR_STM32G070CB part number, so every flash/pin variant of
// the same silicon is covered automatically. The "+LPUART" siblings
// (G071/G081, G0B1/G0C1) are included here too - their shared vector name
// differs (see MIOS32_UART_SHARED_IRQ_CHANNEL below, which is tier-
// conditional for exactly this reason) but is now fully resolved.
#if defined(MIOS32_USE_UART2) && !(defined(STM32G070xx) || defined(STM32G0B0xx) || defined(STM32G071xx) || defined(STM32G081xx) || defined(STM32G0B1xx) || defined(STM32G0C1xx))
#undef MIOS32_USE_UART2
#endif
#if defined(MIOS32_USE_UART3) && !(defined(STM32G070xx) || defined(STM32G0B0xx) || defined(STM32G071xx) || defined(STM32G081xx) || defined(STM32G0B1xx) || defined(STM32G0C1xx))
#undef MIOS32_USE_UART3
#endif

// USART5 (UART4) and USART6 (UART5) exist on the 6-USART tier, with or
// without LPUART (G0B0/G0B1/G0C1).
#if defined(MIOS32_USE_UART4) && !(defined(STM32G0B0xx) || defined(STM32G0B1xx) || defined(STM32G0C1xx))
#undef MIOS32_USE_UART4
#endif
#if defined(MIOS32_USE_UART5) && !(defined(STM32G0B0xx) || defined(STM32G0B1xx) || defined(STM32G0C1xx))
#undef MIOS32_USE_UART5
#endif

// UART6 (LPUART1) exists on every "+LPUART" G0 tier (2/4/6-USART siblings).
#if defined(MIOS32_USE_UART6) && !(defined(STM32G031xx) || defined(STM32G041xx) || defined(STM32G051xx) || defined(STM32G061xx) || defined(STM32G071xx) || defined(STM32G081xx) || defined(STM32G0B1xx) || defined(STM32G0C1xx))
#undef MIOS32_USE_UART6
#endif

// UART7 (LPUART2) exists ONLY on the 6-USART+2xLPUART tier (G0B1/G0C1) -
// the 2-USART and 4-USART "+LPUART" siblings only ever get one LPUART.
#if defined(MIOS32_USE_UART7) && !(defined(STM32G0B1xx) || defined(STM32G0C1xx))
#undef MIOS32_USE_UART7
#endif

// auto-derive the master switch from any individual port actually wanted -
// no need for the project to separately set MIOS32_USE_UART on top of
// MIOS32_USE_UARTx. Safe to do locally in this .c file only because nothing
// outside mios32_uart.c ever checks the bare MIOS32_USE_UART macro
// (verified) - unlike MIOS32_USE_SPI, which programming_models/traditional/
// main.c also checks directly, so that one is derived from the shared
// header (mios32_spi.h) instead, not locally in mios32_spi.c.
// Derived AFTER the tier guards above, deliberately: a port the project
// asked for may have just been dropped because this chip doesn't have it,
// and the master switch must reflect what actually survived - otherwise the
// whole module would compile its buffers and Init() for zero usable ports.
#if !defined(MIOS32_USE_UART) && (defined(MIOS32_USE_UART0) || defined(MIOS32_USE_UART1) || defined(MIOS32_USE_UART2) || defined(MIOS32_USE_UART3) || defined(MIOS32_USE_UART4) || defined(MIOS32_USE_UART5) || defined(MIOS32_USE_UART6) || defined(MIOS32_USE_UART7))
#define MIOS32_USE_UART
#endif

// ...and if nothing survived while a UART-based transport was requested,
// say so loudly. Silence is the wrong answer here: the build would succeed,
// MIOS32_UART_Init() would drive nothing, and the symptom on the bench is a
// board that simply never answers - diagnosed the hard way on a G030K6,
// whose config asked for UART0 back when that meant USART3, a peripheral
// that chip doesn't have.
#if defined(MIOS32_USE_DIN_MIDI) && !defined(MIOS32_USE_UART)
# error "MIOS32_USE_DIN_MIDI is enabled, but no MIOS32_USE_UARTx port survives on this chip - see the tier table at the top of this file and pick a port this device actually has (MIOS32_UARTn is USART(n+1))."
#endif

// this module can be optionally enabled in a local mios32_config.h file (included from mios32.h)
#if defined(MIOS32_USE_UART)


// Tier flag: does this chip's LPUART1 (UART6) share a vector with the
// USART3+ group below (MIOS32_UART_SHARED_IRQ_CHANNEL) instead of having
// its own independent LPUART1_IRQn? True on the 4-USART+LPUART1 and
// 6-USART+2xLPUART tiers; false (independent vector) on the 2-USART+
// LPUART1 tier, where there's no USART3+ group to share with in the first
// place.
#if defined(STM32G071xx) || defined(STM32G081xx) || defined(STM32G0B1xx) || defined(STM32G0C1xx)
#define MIOS32_G0_LPUART1_SHARED 1
#endif

// Tier flag: does this chip have a second LPUART (UART7), which always
// shares USART2's (UART1's) own vector rather than USART2 being
// independent? True only on the 6-USART+2xLPUART tier.
#if defined(STM32G0B1xx) || defined(STM32G0C1xx)
#define MIOS32_G0_LPUART2_TIER 1
#endif

// USART3/USART4 (and, on the 6-USART tier, USART5/USART6 too, and on the
// "+LPUART" siblings, LPUART1/UART6 too) share a single NVIC vector - but
// the vector's NAME differs by tier (verified via CMSIS): USART3+4 on the
// plain 4-USART tier, USART3+4+5+6 together on the plain 6-USART tier,
// USART3+4+LPUART1 on the 4-USART+LPUART1 tier, USART3+4+5+6+LPUART1 on the
// 6-USART+2xLPUART tier. Every UARTx macro block below that touches this
// shared vector resolves through these two macros instead of a hardcoded
// name, so the SAME handler function below is correct on every tier
// without a chip-specific #ifdef inside the handler itself.
#if defined(STM32G0B1xx) || defined(STM32G0C1xx)
#define MIOS32_UART_SHARED_IRQ_CHANNEL    USART3_4_5_6_LPUART1_IRQn
#define MIOS32_UART_SHARED_IRQHANDLER_FUNC void USART3_4_5_6_LPUART1_IRQHandler(void)
#elif defined(STM32G0B0xx)
#define MIOS32_UART_SHARED_IRQ_CHANNEL    USART3_4_5_6_IRQn
#define MIOS32_UART_SHARED_IRQHANDLER_FUNC void USART3_4_5_6_IRQHandler(void)
#elif defined(STM32G071xx) || defined(STM32G081xx)
#define MIOS32_UART_SHARED_IRQ_CHANNEL    USART3_4_LPUART1_IRQn
#define MIOS32_UART_SHARED_IRQHANDLER_FUNC void USART3_4_LPUART1_IRQHandler(void)
#else
#define MIOS32_UART_SHARED_IRQ_CHANNEL    USART3_4_IRQn
#define MIOS32_UART_SHARED_IRQHANDLER_FUNC void USART3_4_IRQHandler(void)
#endif


/////////////////////////////////////////////////////////////////////////////
// Pin definitions and USART mappings
/////////////////////////////////////////////////////////////////////////////

// fixed number of port "slots" (buffers, arrays) - independent of how many
// are actually enabled via MIOS32_USE_UARTx, same pattern as mios32_spi.c
#define MIOS32_UART_MAX_PORTS 8

// UART0 (USART1 peripheral) - exists on EVERY G0 tier, never force-undef'd,
// so this #if is purely for structural symmetry with every other port block
// (not strictly required, but keeps "block == #if defined(MIOS32_USE_UARTx)"
// a reliable invariant everywhere in this file).
#if defined(MIOS32_USE_UART0)
#ifndef MIOS32_UART0_TX_PORT
#define MIOS32_UART0_TX_PORT     GPIOB
#endif
#ifndef MIOS32_UART0_TX_PIN
#define MIOS32_UART0_TX_PIN      LL_GPIO_PIN_6
#endif
#ifndef MIOS32_UART0_TX_AF
#define MIOS32_UART0_TX_AF       LL_GPIO_AF_0
#endif
#ifndef MIOS32_UART0_RX_PORT
#define MIOS32_UART0_RX_PORT     GPIOB
#endif
#ifndef MIOS32_UART0_RX_PIN
#define MIOS32_UART0_RX_PIN      LL_GPIO_PIN_7
#endif
#ifndef MIOS32_UART0_RX_AF
#define MIOS32_UART0_RX_AF       LL_GPIO_AF_0
#endif
#ifndef MIOS32_UART0
#define MIOS32_UART0             USART1
#endif
#ifndef MIOS32_UART0_IRQ_CHANNEL
#define MIOS32_UART0_IRQ_CHANNEL USART1_IRQn
#endif
#ifndef MIOS32_UART0_IRQHANDLER_FUNC
#define MIOS32_UART0_IRQHANDLER_FUNC void USART1_IRQHandler(void)
#endif
#ifndef MIOS32_UART0_RESET_FUNC
#define MIOS32_UART0_RESET_FUNC  { LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_USART1); LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_USART1); }
#endif
#ifndef MIOS32_UART0_CLOCK_FUNC
#define MIOS32_UART0_CLOCK_FUNC  { LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1); LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB); }
#endif
#ifndef MIOS32_UART0_CLOCK_SOURCE
#define MIOS32_UART0_CLOCK_SOURCE LL_RCC_USART1_CLKSOURCE_PCLK1
#endif
#endif

// UART1 (USART2 peripheral) - exists on EVERY G0 tier (2/4/6-USART, with or
// without LPUART), no force-undef needed. PA2/PA3 chosen over the
// alternative PA14/PA15 pins: those are SWCLK/SWDIO (debug port) on every
// STM32, a real conflict avoided here rather than a hypothetical one.
// IRQ_CHANNEL/IRQHANDLER_FUNC are tier-conditional: on every tier except
// 6-USART+2xLPUART (G0B1/G0C1), USART2 has its own independent vector
// (USART2_IRQn) like everywhere else in this file. On G0B1/G0C1 ONLY,
// USART2 shares its vector with LPUART2/UART7 instead (USART2_LPUART2_IRQn)
// - there is no USART2_IRQn on that one tier. See the combined handler
// further below, which services UART1 and/or UART7 depending on which are
// actually enabled.
#if defined(MIOS32_USE_UART1)
#ifndef MIOS32_UART1_TX_PORT
#define MIOS32_UART1_TX_PORT     GPIOA
#endif
#ifndef MIOS32_UART1_TX_PIN
#define MIOS32_UART1_TX_PIN      LL_GPIO_PIN_2
#endif
#ifndef MIOS32_UART1_TX_AF
#define MIOS32_UART1_TX_AF       LL_GPIO_AF_1
#endif
#ifndef MIOS32_UART1_RX_PORT
#define MIOS32_UART1_RX_PORT     GPIOA
#endif
#ifndef MIOS32_UART1_RX_PIN
#define MIOS32_UART1_RX_PIN      LL_GPIO_PIN_3
#endif
#ifndef MIOS32_UART1_RX_AF
#define MIOS32_UART1_RX_AF       LL_GPIO_AF_1
#endif
#ifndef MIOS32_UART1
#define MIOS32_UART1             USART2
#endif
#ifndef MIOS32_UART1_IRQ_CHANNEL
# if defined(MIOS32_G0_LPUART2_TIER)
#  define MIOS32_UART1_IRQ_CHANNEL USART2_LPUART2_IRQn
# else
#  define MIOS32_UART1_IRQ_CHANNEL USART2_IRQn
# endif
#endif
#ifndef MIOS32_UART1_IRQHANDLER_FUNC
# if defined(MIOS32_G0_LPUART2_TIER)
#  define MIOS32_UART1_IRQHANDLER_FUNC void USART2_LPUART2_IRQHandler(void)
# else
#  define MIOS32_UART1_IRQHANDLER_FUNC void USART2_IRQHandler(void)
# endif
#endif
#ifndef MIOS32_UART1_RESET_FUNC
#define MIOS32_UART1_RESET_FUNC  { LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_USART2); LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_USART2); }
#endif
#ifndef MIOS32_UART1_CLOCK_FUNC
#define MIOS32_UART1_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2); LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA); }
#endif
#endif

// UART2 (USART3 peripheral) - see MIOS32_USE_UART2 force-undef above (not
// available on the 2-USART tiers). Wrapped in #if defined(MIOS32_USE_UART2)
// like every other port block here - unlike UART0/UART1 above, USART3 does
// NOT exist on every G0 chip, so leaving this unguarded would define
// MIOS32_UART2 (-> bare "USART3") even on chips where that identifier isn't
// declared by CMSIS at all - harmless today only because every actual USE of
// MIOS32_UART2 elsewhere already re-checks MIOS32_USE_UART2 itself, but a
// latent trap for any future code that doesn't.
#if defined(MIOS32_USE_UART2)
#ifndef MIOS32_UART2_TX_PORT
#define MIOS32_UART2_TX_PORT     GPIOB
#endif
#ifndef MIOS32_UART2_TX_PIN
#define MIOS32_UART2_TX_PIN      LL_GPIO_PIN_8
#endif
#ifndef MIOS32_UART2_TX_AF
#define MIOS32_UART2_TX_AF       LL_GPIO_AF_4
#endif
#ifndef MIOS32_UART2_RX_PORT
#define MIOS32_UART2_RX_PORT     GPIOB
#endif
#ifndef MIOS32_UART2_RX_PIN
#define MIOS32_UART2_RX_PIN      LL_GPIO_PIN_9
#endif
#ifndef MIOS32_UART2_RX_AF
#define MIOS32_UART2_RX_AF       LL_GPIO_AF_4
#endif
#ifndef MIOS32_UART2
#define MIOS32_UART2             USART3
#endif
#ifndef MIOS32_UART2_IRQ_CHANNEL
#define MIOS32_UART2_IRQ_CHANNEL MIOS32_UART_SHARED_IRQ_CHANNEL
#endif
#ifndef MIOS32_UART2_IRQHANDLER_FUNC
#define MIOS32_UART2_IRQHANDLER_FUNC MIOS32_UART_SHARED_IRQHANDLER_FUNC
#endif
#ifndef MIOS32_UART2_RESET_FUNC
#define MIOS32_UART2_RESET_FUNC  { LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_USART3); LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_USART3); }
#endif
#ifndef MIOS32_UART2_CLOCK_FUNC
#define MIOS32_UART2_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3); LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB); }
#endif
// #define MIOS32_UART2_CLOCK_SOURCE <LL_RCC_USARTx_CLKSOURCE_...>  // not defined by default
#endif

// UART3 (USART4 peripheral) - exists on the 4-USART tier (G070) and the
// 6-USART tier (G0B0), see MIOS32_USE_UART3 force-undef above. Shares the
// tier-appropriate MIOS32_UART_SHARED_IRQ_CHANNEL vector with UART2 (and,
// on the 6-USART tier, also with UART4/UART5) - the IRQ handler is written
// once below and services whichever of these ports is actually enabled.
#if defined(MIOS32_USE_UART3)
#ifndef MIOS32_UART3_TX_PORT
#define MIOS32_UART3_TX_PORT     GPIOA
#endif
#ifndef MIOS32_UART3_TX_PIN
#define MIOS32_UART3_TX_PIN      LL_GPIO_PIN_0
#endif
#ifndef MIOS32_UART3_TX_AF
#define MIOS32_UART3_TX_AF       LL_GPIO_AF_4
#endif
#ifndef MIOS32_UART3_RX_PORT
#define MIOS32_UART3_RX_PORT     GPIOA
#endif
#ifndef MIOS32_UART3_RX_PIN
#define MIOS32_UART3_RX_PIN      LL_GPIO_PIN_1
#endif
#ifndef MIOS32_UART3_RX_AF
#define MIOS32_UART3_RX_AF       LL_GPIO_AF_4
#endif
#ifndef MIOS32_UART3
#define MIOS32_UART3             USART4
#endif
#ifndef MIOS32_UART3_IRQ_CHANNEL
#define MIOS32_UART3_IRQ_CHANNEL MIOS32_UART_SHARED_IRQ_CHANNEL
#endif
#ifndef MIOS32_UART3_RESET_FUNC
#define MIOS32_UART3_RESET_FUNC  { LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_USART4); LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_USART4); }
#endif
#ifndef MIOS32_UART3_CLOCK_FUNC
#define MIOS32_UART3_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART4); LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA); }
#endif
#endif

// UART4 (USART5 peripheral) - only exists on the 6-USART tier (G0B0), see
// MIOS32_USE_UART4 force-undef above. Shares MIOS32_UART_SHARED_IRQ_CHANNEL
// with UART2/UART3/UART5 on this tier.
// PIN MAPPING NOT VERIFIED against a real reference manual or reference
// hardware (no G0B0 board in hand this session) - override
// MIOS32_UART4_{TX,RX}_{PORT,PIN,AF} in your project's mios32_config.h once
// you have real hardware. Chosen only to avoid an obvious collision with
// UART2..UART3's own default pins above.
#if defined(MIOS32_USE_UART4)
#ifndef MIOS32_UART4_TX_PORT
#define MIOS32_UART4_TX_PORT     GPIOC
#endif
#ifndef MIOS32_UART4_TX_PIN
#define MIOS32_UART4_TX_PIN      LL_GPIO_PIN_0
#endif
#ifndef MIOS32_UART4_TX_AF
#define MIOS32_UART4_TX_AF       LL_GPIO_AF_1
#endif
#ifndef MIOS32_UART4_RX_PORT
#define MIOS32_UART4_RX_PORT     GPIOC
#endif
#ifndef MIOS32_UART4_RX_PIN
#define MIOS32_UART4_RX_PIN      LL_GPIO_PIN_1
#endif
#ifndef MIOS32_UART4_RX_AF
#define MIOS32_UART4_RX_AF       LL_GPIO_AF_1
#endif
#ifndef MIOS32_UART4
#define MIOS32_UART4             USART5
#endif
#ifndef MIOS32_UART4_IRQ_CHANNEL
#define MIOS32_UART4_IRQ_CHANNEL MIOS32_UART_SHARED_IRQ_CHANNEL
#endif
#ifndef MIOS32_UART4_RESET_FUNC
#define MIOS32_UART4_RESET_FUNC  { LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_USART5); LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_USART5); }
#endif
#ifndef MIOS32_UART4_CLOCK_FUNC
#define MIOS32_UART4_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART5); LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOC); }
#endif
#endif

// UART5 (USART6 peripheral) - only exists on the 6-USART tier (G0B0/G0B1/
// G0C1), see MIOS32_USE_UART5 force-undef above. Shares
// MIOS32_UART_SHARED_IRQ_CHANNEL with UART2/UART3/UART4 (and, on the
// "+LPUART" sibling of this tier, UART6/LPUART1 too) on this tier.
// PIN MAPPING NOT VERIFIED - see UART4 above, same caveat applies.
#if defined(MIOS32_USE_UART5)
#ifndef MIOS32_UART5_TX_PORT
#define MIOS32_UART5_TX_PORT     GPIOC
#endif
#ifndef MIOS32_UART5_TX_PIN
#define MIOS32_UART5_TX_PIN      LL_GPIO_PIN_2
#endif
#ifndef MIOS32_UART5_TX_AF
#define MIOS32_UART5_TX_AF       LL_GPIO_AF_2
#endif
#ifndef MIOS32_UART5_RX_PORT
#define MIOS32_UART5_RX_PORT     GPIOC
#endif
#ifndef MIOS32_UART5_RX_PIN
#define MIOS32_UART5_RX_PIN      LL_GPIO_PIN_3
#endif
#ifndef MIOS32_UART5_RX_AF
#define MIOS32_UART5_RX_AF       LL_GPIO_AF_2
#endif
#ifndef MIOS32_UART5
#define MIOS32_UART5             USART6
#endif
#ifndef MIOS32_UART5_IRQ_CHANNEL
#define MIOS32_UART5_IRQ_CHANNEL MIOS32_UART_SHARED_IRQ_CHANNEL
#endif
#ifndef MIOS32_UART5_RESET_FUNC
#define MIOS32_UART5_RESET_FUNC  { LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_USART6); LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_USART6); }
#endif
#ifndef MIOS32_UART5_CLOCK_FUNC
#define MIOS32_UART5_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART6); LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOC); }
#endif
#endif

// UART6 (LPUART1 peripheral) - exists on every "+LPUART" G0 tier, see
// MIOS32_USE_UART6 force-undef above. Vector depends on tier
// (MIOS32_G0_LPUART1_SHARED, set above): independent LPUART1_IRQn on the
// 2-USART+LPUART1 tier, or the shared MIOS32_UART_SHARED_IRQ_CHANNEL
// group (with UART2/UART3[/UART4/UART5]) on the 4-USART+LPUART1 and
// 6-USART+2xLPUART tiers.
// PIN MAPPING NOT VERIFIED against a real reference manual or reference
// hardware (no board with any of these chips in hand this session) -
// override MIOS32_UART6_{TX,RX}_{PORT,PIN,AF} in your project's
// mios32_config.h once you have real hardware. Chosen only to avoid an
// obvious collision with UART2..UART5's own default pins above.
// Clock source defaults to PCLK1 like every other port (see module-level
// comment) - LPUART's fixed x256 oversampling and PRESC-based BRR are
// handled in MIOS32_UART_BaudrateSet, not here.
#if defined(MIOS32_USE_UART6)
#ifndef MIOS32_UART6_TX_PORT
#define MIOS32_UART6_TX_PORT     GPIOA
#endif
#ifndef MIOS32_UART6_TX_PIN
#define MIOS32_UART6_TX_PIN      LL_GPIO_PIN_6
#endif
#ifndef MIOS32_UART6_TX_AF
#define MIOS32_UART6_TX_AF       LL_GPIO_AF_1
#endif
#ifndef MIOS32_UART6_RX_PORT
#define MIOS32_UART6_RX_PORT     GPIOA
#endif
#ifndef MIOS32_UART6_RX_PIN
#define MIOS32_UART6_RX_PIN      LL_GPIO_PIN_7
#endif
#ifndef MIOS32_UART6_RX_AF
#define MIOS32_UART6_RX_AF       LL_GPIO_AF_1
#endif
#ifndef MIOS32_UART6
#define MIOS32_UART6             LPUART1
#endif
#ifndef MIOS32_UART6_IRQ_CHANNEL
# if defined(MIOS32_G0_LPUART1_SHARED)
#  define MIOS32_UART6_IRQ_CHANNEL MIOS32_UART_SHARED_IRQ_CHANNEL
# else
#  define MIOS32_UART6_IRQ_CHANNEL LPUART1_IRQn
# endif
#endif
#ifndef MIOS32_UART6_IRQHANDLER_FUNC
# if defined(MIOS32_G0_LPUART1_SHARED)
#  define MIOS32_UART6_IRQHANDLER_FUNC MIOS32_UART_SHARED_IRQHANDLER_FUNC
# else
#  define MIOS32_UART6_IRQHANDLER_FUNC void LPUART1_IRQHandler(void)
# endif
#endif
#ifndef MIOS32_UART6_RESET_FUNC
#define MIOS32_UART6_RESET_FUNC  { LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_LPUART1); LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_LPUART1); }
#endif
#ifndef MIOS32_UART6_CLOCK_FUNC
#define MIOS32_UART6_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_LPUART1); LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA); }
#endif
#ifndef MIOS32_UART6_CLOCK_SOURCE
#define MIOS32_UART6_CLOCK_SOURCE LL_RCC_LPUART1_CLKSOURCE_PCLK1
#endif
#endif

// UART7 (LPUART2 peripheral) - exists ONLY on the 6-USART+2xLPUART tier
// (G0B1/G0C1), see MIOS32_USE_UART7 force-undef above. Always shares
// UART1's vector on this tier (MIOS32_UART1_IRQ_CHANNEL/IRQHANDLER_FUNC,
// already tier-conditional - see UART1 above) - there's no separate
// independent vector for LPUART2 on any G0 chip.
// PIN MAPPING NOT VERIFIED - see UART6 above, same caveat applies.
#if defined(MIOS32_USE_UART7)
#ifndef MIOS32_UART7_TX_PORT
#define MIOS32_UART7_TX_PORT     GPIOB
#endif
#ifndef MIOS32_UART7_TX_PIN
#define MIOS32_UART7_TX_PIN      LL_GPIO_PIN_10
#endif
#ifndef MIOS32_UART7_TX_AF
#define MIOS32_UART7_TX_AF       LL_GPIO_AF_4
#endif
#ifndef MIOS32_UART7_RX_PORT
#define MIOS32_UART7_RX_PORT     GPIOB
#endif
#ifndef MIOS32_UART7_RX_PIN
#define MIOS32_UART7_RX_PIN      LL_GPIO_PIN_11
#endif
#ifndef MIOS32_UART7_RX_AF
#define MIOS32_UART7_RX_AF       LL_GPIO_AF_4
#endif
#ifndef MIOS32_UART7
#define MIOS32_UART7             LPUART2
#endif
#ifndef MIOS32_UART7_IRQ_CHANNEL
#define MIOS32_UART7_IRQ_CHANNEL MIOS32_UART1_IRQ_CHANNEL
#endif
#ifndef MIOS32_UART7_IRQHANDLER_FUNC
#define MIOS32_UART7_IRQHANDLER_FUNC MIOS32_UART1_IRQHANDLER_FUNC
#endif
#ifndef MIOS32_UART7_RESET_FUNC
#define MIOS32_UART7_RESET_FUNC  { LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_LPUART2); LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_LPUART2); }
#endif
#ifndef MIOS32_UART7_CLOCK_FUNC
#define MIOS32_UART7_CLOCK_FUNC  { LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_LPUART2); LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB); }
#endif
#ifndef MIOS32_UART7_CLOCK_SOURCE
#define MIOS32_UART7_CLOCK_SOURCE LL_RCC_LPUART2_CLKSOURCE_PCLK1
#endif
#endif


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

// Line-activity flags for an optional indicator, two bits per port in port
// order - see MIOS32_UART_ActGet(). u32, so all MIOS32_UART_MAX_PORTS slots
// fit (16 ports would, should a family ever need them); the previous u8
// covered only ports 0..3 and left the higher ones silently unreported.
#define UART_ACT_RX(n)  (1UL << (2*(n)))
#define UART_ACT_TX(n)  (1UL << (2*(n)+1))

// Marking a byte as activity, from the RX/TX interrupts. MIDI clock (0xf8)
// is deliberately excluded: a synced setup sends it 24 times per beat and
// would hold the indicator permanently lit, showing nothing. The rule lives
// here, once - it used to be an inline "if(b!=0xf8)" repeated at all 18
// capture sites, where it read as an unexplained magic value.
#define UART_ACT_MARK_RX(n, b)  { if( (b) != 0xf8 ) uart_midi_act |= UART_ACT_RX(n); }
#define UART_ACT_MARK_TX(n, b)  { if( (b) != 0xf8 ) uart_midi_act |= UART_ACT_TX(n); }
static u32 uart_midi_act=0;
static u8  uart_assigned_to_midi;
static u32 uart_baudrate[MIOS32_UART_MAX_PORTS];
static mios32_pin_mode_t  uart_tx_pin_mode[MIOS32_UART_MAX_PORTS];

// named per-port RX/TX buffers - only allocated for ports actually enabled
// via MIOS32_USE_UARTx, unlike the small per-port state above (baudrate,
// pin mode) and below (tail/head/size counters), which stay as cheap
// MIOS32_UART_MAX_PORTS-sized arrays regardless of enablement (a few bytes
// per port, not worth the indirection). The pointer tables below resolve
// each slot at compile time - NULL for any port not compiled in - so the
// existing generic array-indexed function bodies only need a NULL check
// added, no other logic changes.
#if defined(MIOS32_USE_UART0)
static u8 uart0_rx_buffer[MIOS32_UART_RX_BUFFER_SIZE];
static u8 uart0_tx_buffer[MIOS32_UART_TX_BUFFER_SIZE];
#endif
#if defined(MIOS32_USE_UART1)
static u8 uart1_rx_buffer[MIOS32_UART_RX_BUFFER_SIZE];
static u8 uart1_tx_buffer[MIOS32_UART_TX_BUFFER_SIZE];
#endif
#if defined(MIOS32_USE_UART2)
static u8 uart2_rx_buffer[MIOS32_UART_RX_BUFFER_SIZE];
static u8 uart2_tx_buffer[MIOS32_UART_TX_BUFFER_SIZE];
#endif
#if defined(MIOS32_USE_UART3)
static u8 uart3_rx_buffer[MIOS32_UART_RX_BUFFER_SIZE];
static u8 uart3_tx_buffer[MIOS32_UART_TX_BUFFER_SIZE];
#endif
#if defined(MIOS32_USE_UART4)
static u8 uart4_rx_buffer[MIOS32_UART_RX_BUFFER_SIZE];
static u8 uart4_tx_buffer[MIOS32_UART_TX_BUFFER_SIZE];
#endif
#if defined(MIOS32_USE_UART5)
static u8 uart5_rx_buffer[MIOS32_UART_RX_BUFFER_SIZE];
static u8 uart5_tx_buffer[MIOS32_UART_TX_BUFFER_SIZE];
#endif
#if defined(MIOS32_USE_UART6)
static u8 uart6_rx_buffer[MIOS32_UART_RX_BUFFER_SIZE];
static u8 uart6_tx_buffer[MIOS32_UART_TX_BUFFER_SIZE];
#endif
#if defined(MIOS32_USE_UART7)
static u8 uart7_rx_buffer[MIOS32_UART_RX_BUFFER_SIZE];
static u8 uart7_tx_buffer[MIOS32_UART_TX_BUFFER_SIZE];
#endif

static u8 * const rx_buffer_ptr[MIOS32_UART_MAX_PORTS] = {
#if defined(MIOS32_USE_UART0)
  uart0_rx_buffer,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART1)
  uart1_rx_buffer,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART2)
  uart2_rx_buffer,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART3)
  uart3_rx_buffer,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART4)
  uart4_rx_buffer,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART5)
  uart5_rx_buffer,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART6)
  uart6_rx_buffer,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART7)
  uart7_rx_buffer,
#else
  NULL,
#endif
};
static u8 * const tx_buffer_ptr[MIOS32_UART_MAX_PORTS] = {
#if defined(MIOS32_USE_UART0)
  uart0_tx_buffer,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART1)
  uart1_tx_buffer,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART2)
  uart2_tx_buffer,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART3)
  uart3_tx_buffer,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART4)
  uart4_tx_buffer,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART5)
  uart5_tx_buffer,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART6)
  uart6_tx_buffer,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART7)
  uart7_tx_buffer,
#else
  NULL,
#endif
};

static volatile u8 rx_buffer_tail[MIOS32_UART_MAX_PORTS];
static volatile u8 rx_buffer_head[MIOS32_UART_MAX_PORTS];
static volatile u8 rx_buffer_size[MIOS32_UART_MAX_PORTS];

static volatile u8 tx_buffer_tail[MIOS32_UART_MAX_PORTS];
static volatile u8 tx_buffer_head[MIOS32_UART_MAX_PORTS];
static volatile u8 tx_buffer_size[MIOS32_UART_MAX_PORTS];


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

  // map UART pins and enable clocks
#if defined(MIOS32_USE_UART0) && MIOS32_UART0_ASSIGNMENT != 0
  MIOS32_UART0_RESET_FUNC;
  MIOS32_UART0_CLOCK_FUNC;
#endif
#if defined(MIOS32_USE_UART1) && MIOS32_UART1_ASSIGNMENT != 0
  MIOS32_UART1_RESET_FUNC;
  MIOS32_UART1_CLOCK_FUNC
#endif
#if defined(MIOS32_USE_UART2) && MIOS32_UART2_ASSIGNMENT != 0
  MIOS32_UART2_RESET_FUNC;
  MIOS32_UART2_CLOCK_FUNC
#endif
#if defined(MIOS32_USE_UART3) && MIOS32_UART3_ASSIGNMENT != 0
  MIOS32_UART3_RESET_FUNC;
  MIOS32_UART3_CLOCK_FUNC
#endif
#if defined(MIOS32_USE_UART4) && MIOS32_UART4_ASSIGNMENT != 0
  MIOS32_UART4_RESET_FUNC;
  MIOS32_UART4_CLOCK_FUNC
#endif
#if defined(MIOS32_USE_UART5) && MIOS32_UART5_ASSIGNMENT != 0
  MIOS32_UART5_RESET_FUNC;
  MIOS32_UART5_CLOCK_FUNC
#endif
#if defined(MIOS32_USE_UART6) && MIOS32_UART6_ASSIGNMENT != 0
  MIOS32_UART6_RESET_FUNC;
  MIOS32_UART6_CLOCK_FUNC
#endif
#if defined(MIOS32_USE_UART7) && MIOS32_UART7_ASSIGNMENT != 0
  MIOS32_UART7_RESET_FUNC;
  MIOS32_UART7_CLOCK_FUNC
#endif

  // initialize UARTs and clear buffers
  {
    u8 uart;
    for(uart=0; uart<MIOS32_UART_MAX_PORTS; ++uart) {
      rx_buffer_tail[uart] = rx_buffer_head[uart] = rx_buffer_size[uart] = 0;
      tx_buffer_tail[uart] = tx_buffer_head[uart] = tx_buffer_size[uart] = 0;

      MIOS32_UART_InitPortDefault(uart);
    }
  }

  // configure and enable UART interrupts
#if defined(MIOS32_USE_UART0) && MIOS32_UART0_ASSIGNMENT != 0
  MIOS32_IRQ_Install(MIOS32_UART0_IRQ_CHANNEL, MIOS32_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(MIOS32_UART0);
#endif
#if defined(MIOS32_USE_UART1) && MIOS32_UART1_ASSIGNMENT != 0
  MIOS32_IRQ_Install(MIOS32_UART1_IRQ_CHANNEL, MIOS32_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(MIOS32_UART1);
#endif
#if defined(MIOS32_USE_UART2) && MIOS32_UART2_ASSIGNMENT != 0
  MIOS32_IRQ_Install(MIOS32_UART2_IRQ_CHANNEL, MIOS32_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(MIOS32_UART2);
#endif
#if defined(MIOS32_USE_UART3) && MIOS32_UART3_ASSIGNMENT != 0
  MIOS32_IRQ_Install(MIOS32_UART3_IRQ_CHANNEL, MIOS32_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(MIOS32_UART3);
#endif
#if defined(MIOS32_USE_UART4) && MIOS32_UART4_ASSIGNMENT != 0
  MIOS32_IRQ_Install(MIOS32_UART4_IRQ_CHANNEL, MIOS32_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(MIOS32_UART4);
#endif
#if defined(MIOS32_USE_UART5) && MIOS32_UART5_ASSIGNMENT != 0
  MIOS32_IRQ_Install(MIOS32_UART5_IRQ_CHANNEL, MIOS32_IRQ_UART_PRIORITY);
  LL_USART_EnableIT_RXNE(MIOS32_UART5);
#endif
#if defined(MIOS32_USE_UART6) && MIOS32_UART6_ASSIGNMENT != 0
  MIOS32_IRQ_Install(MIOS32_UART6_IRQ_CHANNEL, MIOS32_IRQ_UART_PRIORITY);
  LL_LPUART_EnableIT_RXNE(MIOS32_UART6);
#endif
#if defined(MIOS32_USE_UART7) && MIOS32_UART7_ASSIGNMENT != 0
  MIOS32_IRQ_Install(MIOS32_UART7_IRQ_CHANNEL, MIOS32_IRQ_UART_PRIORITY);
  LL_LPUART_EnableIT_RXNE(MIOS32_UART7);
#endif

  // enable UARTs
#if defined(MIOS32_USE_UART0) && MIOS32_UART0_ASSIGNMENT != 0
  LL_USART_Enable(MIOS32_UART0);
  while((!(LL_USART_IsActiveFlag_TEACK(MIOS32_UART0))) || (!(LL_USART_IsActiveFlag_REACK(MIOS32_UART0)))){}
#endif
#if defined(MIOS32_USE_UART1) && MIOS32_UART1_ASSIGNMENT != 0
  LL_USART_Enable(MIOS32_UART1);
  while((!(LL_USART_IsActiveFlag_TEACK(MIOS32_UART1))) || (!(LL_USART_IsActiveFlag_REACK(MIOS32_UART1)))){}
#endif
#if defined(MIOS32_USE_UART2) && MIOS32_UART2_ASSIGNMENT != 0
  LL_USART_Enable(MIOS32_UART2);
  while((!(LL_USART_IsActiveFlag_TEACK(MIOS32_UART2))) || (!(LL_USART_IsActiveFlag_REACK(MIOS32_UART2)))){}
#endif
#if defined(MIOS32_USE_UART3) && MIOS32_UART3_ASSIGNMENT != 0
  LL_USART_Enable(MIOS32_UART3);
  while((!(LL_USART_IsActiveFlag_TEACK(MIOS32_UART3))) || (!(LL_USART_IsActiveFlag_REACK(MIOS32_UART3)))){}
#endif
#if defined(MIOS32_USE_UART4) && MIOS32_UART4_ASSIGNMENT != 0
  LL_USART_Enable(MIOS32_UART4);
  while((!(LL_USART_IsActiveFlag_TEACK(MIOS32_UART4))) || (!(LL_USART_IsActiveFlag_REACK(MIOS32_UART4)))){}
#endif
#if defined(MIOS32_USE_UART5) && MIOS32_UART5_ASSIGNMENT != 0
  LL_USART_Enable(MIOS32_UART5);
  while((!(LL_USART_IsActiveFlag_TEACK(MIOS32_UART5))) || (!(LL_USART_IsActiveFlag_REACK(MIOS32_UART5)))){}
#endif
#if defined(MIOS32_USE_UART6) && MIOS32_UART6_ASSIGNMENT != 0
  LL_LPUART_Enable(MIOS32_UART6);
  while((!(LL_LPUART_IsActiveFlag_TEACK(MIOS32_UART6))) || (!(LL_LPUART_IsActiveFlag_REACK(MIOS32_UART6)))){}
#endif
#if defined(MIOS32_USE_UART7) && MIOS32_UART7_ASSIGNMENT != 0
  LL_LPUART_Enable(MIOS32_UART7);
  while((!(LL_LPUART_IsActiveFlag_TEACK(MIOS32_UART7))) || (!(LL_LPUART_IsActiveFlag_REACK(MIOS32_UART7)))){}
#endif

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! \return 0 if UART is not assigned to a MIDI function
//! \return 1 if UART is assigned to a MIDI function
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_IsAssignedToMIDI(u8 uart)
{
  if( uart >= MIOS32_UART_MAX_PORTS )
    return 0; // no UART available
  return (uart_assigned_to_midi & (1 << uart)) ? 1 : 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Initializes a given UART interface based on given baudrate and TX output mode
//! \param[in] uart UART number (0..3)
//! \param[in] baudrate the baudrate
//! \param[in] tx_pin_mode the TX pin mode
//!   <UL>
//!     <LI>MIOS32_PIN_MODE_OUTPUT_PP: TX pin configured for push-pull mode
//!     <LI>MIOS32_PIN_MODE_OUTPUT_OD: TX pin configured for open drain mode
//!   </UL>
//! \param[in] is_midi MIDI or common UART interface?
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_InitPort(u8 uart, u32 baudrate, mios32_pin_mode_t tx_pin_mode, u8 is_midi)
{
  LL_GPIO_InitTypeDef GPIO_InitStructure;
  LL_GPIO_StructInit(&GPIO_InitStructure);

  if( uart >= MIOS32_UART_MAX_PORTS )
    return -1; // unsupported UART

  // MIDI assignment
  if( is_midi ) {
    uart_assigned_to_midi |= (1 << uart);
  } else {
    uart_assigned_to_midi &= ~(1 << uart);
  }
  // store pin mode
  uart_tx_pin_mode[uart]=tx_pin_mode;

  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_HIGH;

  switch( uart ) {
#if defined(MIOS32_USE_UART0) && MIOS32_UART0_ASSIGNMENT != 0
  case 0: {
    GPIO_InitStructure.Pin = MIOS32_UART0_TX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = (tx_pin_mode == MIOS32_PIN_MODE_OUTPUT_PP) ? LL_GPIO_OUTPUT_PUSHPULL : LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStructure.Alternate = MIOS32_UART0_TX_AF;
    LL_GPIO_Init(MIOS32_UART0_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = MIOS32_UART0_RX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStructure.Pull  = LL_GPIO_PULL_UP;
    GPIO_InitStructure.Alternate = MIOS32_UART0_RX_AF;
    LL_GPIO_Init(MIOS32_UART0_RX_PORT, &GPIO_InitStructure);

    MIOS32_UART_BaudrateSet(uart, baudrate);
  } break;
#endif

#if defined(MIOS32_USE_UART1) && MIOS32_UART1_ASSIGNMENT != 0
  case 1: {
    GPIO_InitStructure.Pin = MIOS32_UART1_TX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = (tx_pin_mode == MIOS32_PIN_MODE_OUTPUT_PP) ? LL_GPIO_OUTPUT_PUSHPULL : LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStructure.Alternate = MIOS32_UART1_TX_AF;
    LL_GPIO_Init(MIOS32_UART1_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = MIOS32_UART1_RX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStructure.Pull  = LL_GPIO_PULL_UP;
    GPIO_InitStructure.Alternate = MIOS32_UART1_RX_AF;
    LL_GPIO_Init(MIOS32_UART1_RX_PORT, &GPIO_InitStructure);

    MIOS32_UART_BaudrateSet(uart, baudrate);
  } break;
#endif

#if defined(MIOS32_USE_UART2) && MIOS32_UART2_ASSIGNMENT != 0
  case 2: {
    GPIO_InitStructure.Pin = MIOS32_UART2_TX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = (tx_pin_mode == MIOS32_PIN_MODE_OUTPUT_PP) ? LL_GPIO_OUTPUT_PUSHPULL : LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStructure.Alternate = MIOS32_UART2_TX_AF;
    LL_GPIO_Init(MIOS32_UART2_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = MIOS32_UART2_RX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStructure.Pull  = LL_GPIO_PULL_UP;
    GPIO_InitStructure.Alternate = MIOS32_UART2_RX_AF;
    LL_GPIO_Init(MIOS32_UART2_RX_PORT, &GPIO_InitStructure);

    MIOS32_UART_BaudrateSet(uart, baudrate);
  } break;
#endif

#if defined(MIOS32_USE_UART3) && MIOS32_UART3_ASSIGNMENT != 0
  case 3: {
    GPIO_InitStructure.Pin = MIOS32_UART3_TX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = (tx_pin_mode == MIOS32_PIN_MODE_OUTPUT_PP) ? LL_GPIO_OUTPUT_PUSHPULL : LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStructure.Alternate = MIOS32_UART3_TX_AF;
    LL_GPIO_Init(MIOS32_UART3_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = MIOS32_UART3_RX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStructure.Pull  = LL_GPIO_PULL_UP;
    GPIO_InitStructure.Alternate = MIOS32_UART3_RX_AF;
    LL_GPIO_Init(MIOS32_UART3_RX_PORT, &GPIO_InitStructure);

    MIOS32_UART_BaudrateSet(uart, baudrate);
  } break;
#endif

#if defined(MIOS32_USE_UART4) && MIOS32_UART4_ASSIGNMENT != 0
  case 4: {
    GPIO_InitStructure.Pin = MIOS32_UART4_TX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = (tx_pin_mode == MIOS32_PIN_MODE_OUTPUT_PP) ? LL_GPIO_OUTPUT_PUSHPULL : LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStructure.Alternate = MIOS32_UART4_TX_AF;
    LL_GPIO_Init(MIOS32_UART4_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = MIOS32_UART4_RX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStructure.Pull  = LL_GPIO_PULL_UP;
    GPIO_InitStructure.Alternate = MIOS32_UART4_RX_AF;
    LL_GPIO_Init(MIOS32_UART4_RX_PORT, &GPIO_InitStructure);

    MIOS32_UART_BaudrateSet(uart, baudrate);
  } break;
#endif

#if defined(MIOS32_USE_UART5) && MIOS32_UART5_ASSIGNMENT != 0
  case 5: {
    GPIO_InitStructure.Pin = MIOS32_UART5_TX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = (tx_pin_mode == MIOS32_PIN_MODE_OUTPUT_PP) ? LL_GPIO_OUTPUT_PUSHPULL : LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStructure.Alternate = MIOS32_UART5_TX_AF;
    LL_GPIO_Init(MIOS32_UART5_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = MIOS32_UART5_RX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStructure.Pull  = LL_GPIO_PULL_UP;
    GPIO_InitStructure.Alternate = MIOS32_UART5_RX_AF;
    LL_GPIO_Init(MIOS32_UART5_RX_PORT, &GPIO_InitStructure);

    MIOS32_UART_BaudrateSet(uart, baudrate);
  } break;
#endif

#if defined(MIOS32_USE_UART6) && MIOS32_UART6_ASSIGNMENT != 0
  case 6: {
    GPIO_InitStructure.Pin = MIOS32_UART6_TX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = (tx_pin_mode == MIOS32_PIN_MODE_OUTPUT_PP) ? LL_GPIO_OUTPUT_PUSHPULL : LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStructure.Alternate = MIOS32_UART6_TX_AF;
    LL_GPIO_Init(MIOS32_UART6_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = MIOS32_UART6_RX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStructure.Pull  = LL_GPIO_PULL_UP;
    GPIO_InitStructure.Alternate = MIOS32_UART6_RX_AF;
    LL_GPIO_Init(MIOS32_UART6_RX_PORT, &GPIO_InitStructure);

    MIOS32_UART_BaudrateSet(uart, baudrate);
  } break;
#endif

#if defined(MIOS32_USE_UART7) && MIOS32_UART7_ASSIGNMENT != 0
  case 7: {
    GPIO_InitStructure.Pin = MIOS32_UART7_TX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = (tx_pin_mode == MIOS32_PIN_MODE_OUTPUT_PP) ? LL_GPIO_OUTPUT_PUSHPULL : LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStructure.Alternate = MIOS32_UART7_TX_AF;
    LL_GPIO_Init(MIOS32_UART7_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = MIOS32_UART7_RX_PIN;
    GPIO_InitStructure.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStructure.Pull  = LL_GPIO_PULL_UP;
    GPIO_InitStructure.Alternate = MIOS32_UART7_RX_AF;
    LL_GPIO_Init(MIOS32_UART7_RX_PORT, &GPIO_InitStructure);

    MIOS32_UART_BaudrateSet(uart, baudrate);
  } break;
#endif

  default:
    return -1; // unsupported UART
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Initializes a given UART interface based on default settings
//! \param[in] uart UART number (0..3)
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_InitPortDefault(u8 uart)
{
  switch( uart ) {
#if defined(MIOS32_USE_UART0) && MIOS32_UART0_ASSIGNMENT != 0
  case 0: {
# if MIOS32_UART0_TX_OD
    MIOS32_UART_InitPort(0, MIOS32_UART0_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_OD, MIOS32_UART0_ASSIGNMENT == 1);
# else
    MIOS32_UART_InitPort(0, MIOS32_UART0_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_PP, MIOS32_UART0_ASSIGNMENT == 1);
# endif
  } break;
#endif

#if defined(MIOS32_USE_UART1) && MIOS32_UART1_ASSIGNMENT != 0
  case 1: {
# if MIOS32_UART1_TX_OD
    MIOS32_UART_InitPort(1, MIOS32_UART1_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_OD, MIOS32_UART1_ASSIGNMENT == 1);
# else
    MIOS32_UART_InitPort(1, MIOS32_UART1_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_PP, MIOS32_UART1_ASSIGNMENT == 1);
# endif
  } break;
#endif

#if defined(MIOS32_USE_UART2) && MIOS32_UART2_ASSIGNMENT != 0
  case 2: {
# if MIOS32_UART2_TX_OD
    MIOS32_UART_InitPort(2, MIOS32_UART2_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_OD, MIOS32_UART2_ASSIGNMENT == 1);
# else
    MIOS32_UART_InitPort(2, MIOS32_UART2_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_PP, MIOS32_UART2_ASSIGNMENT == 1);
# endif
  } break;
#endif

#if defined(MIOS32_USE_UART3) && MIOS32_UART3_ASSIGNMENT != 0
  case 3: {
# if MIOS32_UART3_TX_OD
    MIOS32_UART_InitPort(3, MIOS32_UART3_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_OD, MIOS32_UART3_ASSIGNMENT == 1);
# else
    MIOS32_UART_InitPort(3, MIOS32_UART3_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_PP, MIOS32_UART3_ASSIGNMENT == 1);
# endif
  } break;
#endif

#if defined(MIOS32_USE_UART4) && MIOS32_UART4_ASSIGNMENT != 0
  case 4: {
# if MIOS32_UART4_TX_OD
    MIOS32_UART_InitPort(4, MIOS32_UART4_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_OD, MIOS32_UART4_ASSIGNMENT == 1);
# else
    MIOS32_UART_InitPort(4, MIOS32_UART4_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_PP, MIOS32_UART4_ASSIGNMENT == 1);
# endif
  } break;
#endif

#if defined(MIOS32_USE_UART5) && MIOS32_UART5_ASSIGNMENT != 0
  case 5: {
# if MIOS32_UART5_TX_OD
    MIOS32_UART_InitPort(5, MIOS32_UART5_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_OD, MIOS32_UART5_ASSIGNMENT == 1);
# else
    MIOS32_UART_InitPort(5, MIOS32_UART5_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_PP, MIOS32_UART5_ASSIGNMENT == 1);
# endif
  } break;
#endif

#if defined(MIOS32_USE_UART6) && MIOS32_UART6_ASSIGNMENT != 0
  case 6: {
# if MIOS32_UART6_TX_OD
    MIOS32_UART_InitPort(6, MIOS32_UART6_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_OD, MIOS32_UART6_ASSIGNMENT == 1);
# else
    MIOS32_UART_InitPort(6, MIOS32_UART6_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_PP, MIOS32_UART6_ASSIGNMENT == 1);
# endif
  } break;
#endif

#if defined(MIOS32_USE_UART7) && MIOS32_UART7_ASSIGNMENT != 0
  case 7: {
# if MIOS32_UART7_TX_OD
    MIOS32_UART_InitPort(7, MIOS32_UART7_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_OD, MIOS32_UART7_ASSIGNMENT == 1);
# else
    MIOS32_UART_InitPort(7, MIOS32_UART7_BAUDRATE, MIOS32_PIN_MODE_OUTPUT_PP, MIOS32_UART7_ASSIGNMENT == 1);
# endif
  } break;
#endif

  default:
    return -1; // unsupported UART
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! sets the baudrate of a UART port
//! \param[in] uart UART number (0..3)
//! \param[in] baudrate the baudrate
//! \return 0: baudrate has been changed
//! \return -1: uart not available
//! \return -2: function not prepared for this UART
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_BaudrateSet(u8 uart, u32 baudrate)
{
  if( uart >= MIOS32_UART_MAX_PORTS )
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
#if defined(MIOS32_USE_UART0)
  case 0:
	  LL_USART_Init(MIOS32_UART0, &USART_InitStructure);
	  LL_USART_SetTXFIFOThreshold(MIOS32_UART0, LL_USART_FIFOTHRESHOLD_1_4);
	  LL_USART_SetRXFIFOThreshold(MIOS32_UART0, LL_USART_FIFOTHRESHOLD_1_8);
	  LL_USART_DisableFIFO(MIOS32_UART0);
	  LL_USART_DisableOverrunDetect(MIOS32_UART0);
	  LL_USART_DisableDMADeactOnRxErr(MIOS32_UART0);
	  LL_USART_ConfigAsyncMode(MIOS32_UART0);
#ifdef MIOS32_UART0_TX_INVERTED
	  LL_USART_SetTXPinLevel(MIOS32_UART0, LL_USART_TXPIN_LEVEL_INVERTED);
#endif
#ifdef MIOS32_UART0_CLOCK_SOURCE
	  LL_RCC_SetUSARTClockSource(MIOS32_UART0_CLOCK_SOURCE);
#endif
	  break;
#endif
#if defined(MIOS32_USE_UART1)
  case 1:
	  LL_USART_Init(MIOS32_UART1, &USART_InitStructure);
	  LL_USART_SetTXFIFOThreshold(MIOS32_UART1, LL_USART_FIFOTHRESHOLD_1_4);
	  LL_USART_SetRXFIFOThreshold(MIOS32_UART1, LL_USART_FIFOTHRESHOLD_1_8);
	  LL_USART_DisableFIFO(MIOS32_UART1);
	  LL_USART_DisableOverrunDetect(MIOS32_UART1);
	  LL_USART_DisableDMADeactOnRxErr(MIOS32_UART1);
	  LL_USART_ConfigAsyncMode(MIOS32_UART1);
#ifdef MIOS32_UART1_TX_INVERTED
	  LL_USART_SetTXPinLevel(MIOS32_UART1, LL_USART_TXPIN_LEVEL_INVERTED);
#endif
#ifdef MIOS32_UART1_CLOCK_SOURCE
	  LL_RCC_SetUSARTClockSource(MIOS32_UART1_CLOCK_SOURCE);
#endif
	  break;
#endif
#if defined(MIOS32_USE_UART2)
  case 2:
	  LL_USART_Init(MIOS32_UART2, &USART_InitStructure);
	  LL_USART_DisableOverrunDetect(MIOS32_UART2);
	  LL_USART_DisableDMADeactOnRxErr(MIOS32_UART2);
	  LL_USART_ConfigAsyncMode(MIOS32_UART2);
#ifdef MIOS32_UART2_TX_INVERTED
	  LL_USART_SetTXPinLevel(MIOS32_UART2, LL_USART_TXPIN_LEVEL_INVERTED);
#endif
#ifdef MIOS32_UART2_CLOCK_SOURCE
	  LL_RCC_SetUSARTClockSource(MIOS32_UART2_CLOCK_SOURCE);
#endif
	  break;
#endif
#if defined(MIOS32_USE_UART3)
  case 3:
	  LL_USART_Init(MIOS32_UART3, &USART_InitStructure);
	  LL_USART_DisableOverrunDetect(MIOS32_UART3);
	  LL_USART_DisableDMADeactOnRxErr(MIOS32_UART3);
	  LL_USART_ConfigAsyncMode(MIOS32_UART3);
#ifdef MIOS32_UART3_TX_INVERTED
	  LL_USART_SetTXPinLevel(MIOS32_UART3, LL_USART_TXPIN_LEVEL_INVERTED);
#endif
#ifdef MIOS32_UART3_CLOCK_SOURCE
	  LL_RCC_SetUSARTClockSource(MIOS32_UART3_CLOCK_SOURCE);
#endif
	  break;
#endif
#if defined(MIOS32_USE_UART4)
  case 4:
	  LL_USART_Init(MIOS32_UART4, &USART_InitStructure);
	  LL_USART_DisableOverrunDetect(MIOS32_UART4);
	  LL_USART_DisableDMADeactOnRxErr(MIOS32_UART4);
	  LL_USART_ConfigAsyncMode(MIOS32_UART4);
#ifdef MIOS32_UART4_TX_INVERTED
	  LL_USART_SetTXPinLevel(MIOS32_UART4, LL_USART_TXPIN_LEVEL_INVERTED);
#endif
#ifdef MIOS32_UART4_CLOCK_SOURCE
	  LL_RCC_SetUSARTClockSource(MIOS32_UART4_CLOCK_SOURCE);
#endif
	  break;
#endif
#if defined(MIOS32_USE_UART5)
  case 5:
	  LL_USART_Init(MIOS32_UART5, &USART_InitStructure);
	  LL_USART_DisableOverrunDetect(MIOS32_UART5);
	  LL_USART_DisableDMADeactOnRxErr(MIOS32_UART5);
	  LL_USART_ConfigAsyncMode(MIOS32_UART5);
#ifdef MIOS32_UART5_TX_INVERTED
	  LL_USART_SetTXPinLevel(MIOS32_UART5, LL_USART_TXPIN_LEVEL_INVERTED);
#endif
#ifdef MIOS32_UART5_CLOCK_SOURCE
	  LL_RCC_SetUSARTClockSource(MIOS32_UART5_CLOCK_SOURCE);
#endif
	  break;
#endif
#if defined(MIOS32_USE_UART6)
  case 6: {
    // LPUART: separate LL API/init struct - fixed x256 oversampling (no
    // OverSampling field), PRESC-based BRR instead of LL_USART's - see
    // module-level comment above.
    LL_LPUART_InitTypeDef LPUART_InitStructure;
    LPUART_InitStructure.DataWidth = LL_LPUART_DATAWIDTH_8B;
    LPUART_InitStructure.StopBits = LL_LPUART_STOPBITS_1;
    LPUART_InitStructure.Parity = LL_LPUART_PARITY_NONE;
    LPUART_InitStructure.HardwareFlowControl = LL_LPUART_HWCONTROL_NONE;
    LPUART_InitStructure.TransferDirection = LL_LPUART_DIRECTION_TX_RX;
    LPUART_InitStructure.BaudRate = baudrate;
    LPUART_InitStructure.PrescalerValue = LL_LPUART_PRESCALER_DIV1;
    LL_LPUART_Init(MIOS32_UART6, &LPUART_InitStructure);
    LL_LPUART_SetTXFIFOThreshold(MIOS32_UART6, LL_LPUART_FIFOTHRESHOLD_1_4);
    LL_LPUART_SetRXFIFOThreshold(MIOS32_UART6, LL_LPUART_FIFOTHRESHOLD_1_8);
    LL_LPUART_DisableFIFO(MIOS32_UART6);
    LL_LPUART_DisableOverrunDetect(MIOS32_UART6);
    LL_LPUART_DisableDMADeactOnRxErr(MIOS32_UART6);
#ifdef MIOS32_UART6_TX_INVERTED
    LL_LPUART_SetTXPinLevel(MIOS32_UART6, LL_LPUART_TXPIN_LEVEL_INVERTED);
#endif
#ifdef MIOS32_UART6_CLOCK_SOURCE
    LL_RCC_SetLPUARTClockSource(MIOS32_UART6_CLOCK_SOURCE);
#endif
  } break;
#endif
#if defined(MIOS32_USE_UART7)
  case 7: {
    LL_LPUART_InitTypeDef LPUART_InitStructure;
    LPUART_InitStructure.DataWidth = LL_LPUART_DATAWIDTH_8B;
    LPUART_InitStructure.StopBits = LL_LPUART_STOPBITS_1;
    LPUART_InitStructure.Parity = LL_LPUART_PARITY_NONE;
    LPUART_InitStructure.HardwareFlowControl = LL_LPUART_HWCONTROL_NONE;
    LPUART_InitStructure.TransferDirection = LL_LPUART_DIRECTION_TX_RX;
    LPUART_InitStructure.BaudRate = baudrate;
    LPUART_InitStructure.PrescalerValue = LL_LPUART_PRESCALER_DIV1;
    LL_LPUART_Init(MIOS32_UART7, &LPUART_InitStructure);
    LL_LPUART_SetTXFIFOThreshold(MIOS32_UART7, LL_LPUART_FIFOTHRESHOLD_1_4);
    LL_LPUART_SetRXFIFOThreshold(MIOS32_UART7, LL_LPUART_FIFOTHRESHOLD_1_8);
    LL_LPUART_DisableFIFO(MIOS32_UART7);
    LL_LPUART_DisableOverrunDetect(MIOS32_UART7);
    LL_LPUART_DisableDMADeactOnRxErr(MIOS32_UART7);
#ifdef MIOS32_UART7_TX_INVERTED
    LL_LPUART_SetTXPinLevel(MIOS32_UART7, LL_LPUART_TXPIN_LEVEL_INVERTED);
#endif
#ifdef MIOS32_UART7_CLOCK_SOURCE
    LL_RCC_SetLPUARTClockSource(MIOS32_UART7_CLOCK_SOURCE);
#endif
  } break;
#endif
  default:
    return -2; // not prepared
  }

  // store baudrate in array
  uart_baudrate[uart] = baudrate;

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//! returns the current baudrate of a UART port
//! \param[in] uart UART number (0..3)
//! \return 0: uart not available
//! \return all other values: the current baudrate
/////////////////////////////////////////////////////////////////////////////
u32 MIOS32_UART_BaudrateGet(u8 uart)
{
  if( uart >= MIOS32_UART_MAX_PORTS )
    return 0;
  return uart_baudrate[uart];
}


/////////////////////////////////////////////////////////////////////////////
//! returns number of free bytes in receive buffer
//! \param[in] uart UART number (0..3)
//! \return uart number of free bytes
//! \return 1: uart available
//! \return 0: uart not available
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferFree(u8 uart)
{
  if( uart >= MIOS32_UART_MAX_PORTS )
    return 0;
  return MIOS32_UART_RX_BUFFER_SIZE - rx_buffer_size[uart];
}


/////////////////////////////////////////////////////////////////////////////
//! returns number of used bytes in receive buffer
//! \param[in] uart UART number (0..3)
//! \return > 0: number of used bytes
//! \return 0 if uart not available
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferUsed(u8 uart)
{
  if( uart >= MIOS32_UART_MAX_PORTS )
    return 0;
  return rx_buffer_size[uart];
}


/////////////////////////////////////////////////////////////////////////////
//! gets a byte from the receive buffer
//! \param[in] uart UART number (0..3)
//! \return -1 if UART not available
//! \return -2 if no new byte available
//! \return >= 0: number of received bytes
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferGet(u8 uart)
{
  if( uart >= MIOS32_UART_MAX_PORTS || rx_buffer_ptr[uart] == NULL )
    return -1; // UART not available

  if( !rx_buffer_size[uart] )
    return -2; // nothing new in buffer

  // get byte - this operation should be atomic!
  MIOS32_IRQ_Disable();
  u8 b = rx_buffer_ptr[uart][rx_buffer_tail[uart]];
  if( ++rx_buffer_tail[uart] >= MIOS32_UART_RX_BUFFER_SIZE )
    rx_buffer_tail[uart] = 0;
  --rx_buffer_size[uart];
  MIOS32_IRQ_Enable();

  return b; // return received byte
}


/////////////////////////////////////////////////////////////////////////////
//! returns the next byte of the receive buffer without taking it
//! \param[in] uart UART number (0..3)
//! \return -1 if UART not available
//! \return -2 if no new byte available
//! \return >= 0: number of received bytes
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferPeek(u8 uart)
{
  if( uart >= MIOS32_UART_MAX_PORTS || rx_buffer_ptr[uart] == NULL )
    return -1; // UART not available

  if( !rx_buffer_size[uart] )
    return -2; // nothing new in buffer

  // get byte - this operation should be atomic!
  MIOS32_IRQ_Disable();
  u8 b = rx_buffer_ptr[uart][rx_buffer_tail[uart]];
  MIOS32_IRQ_Enable();

  return b; // return received byte
}


/////////////////////////////////////////////////////////////////////////////
//! puts a byte onto the receive buffer
//! \param[in] uart UART number (0..3)
//! \param[in] b byte which should be put into Rx buffer
//! \return 0 if no error
//! \return -1 if UART not available
//! \return -2 if buffer full (retry)
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_RxBufferPut(u8 uart, u8 b)
{
  if( uart >= MIOS32_UART_MAX_PORTS || rx_buffer_ptr[uart] == NULL )
    return -1; // UART not available

  if( rx_buffer_size[uart] >= MIOS32_UART_RX_BUFFER_SIZE )
    return -2; // buffer full (retry)

  // copy received byte into receive buffer
  // this operation should be atomic!
  MIOS32_IRQ_Disable();
  rx_buffer_ptr[uart][rx_buffer_head[uart]] = b;
  if( ++rx_buffer_head[uart] >= MIOS32_UART_RX_BUFFER_SIZE )
    rx_buffer_head[uart] = 0;
  ++rx_buffer_size[uart];
  MIOS32_IRQ_Enable();

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! returns number of free bytes in transmit buffer
//! \param[in] uart UART number (0..3)
//! \return number of free bytes
//! \return 0 if uart not available
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferFree(u8 uart)
{
  if( uart >= MIOS32_UART_MAX_PORTS )
    return 0;
  return MIOS32_UART_TX_BUFFER_SIZE - tx_buffer_size[uart];
}


/////////////////////////////////////////////////////////////////////////////
//! returns number of used bytes in transmit buffer
//! \param[in] uart UART number (0..3)
//! \return number of used bytes
//! \return 0 if uart not available
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferUsed(u8 uart)
{
  if( uart >= MIOS32_UART_MAX_PORTS )
    return 0;
  return tx_buffer_size[uart];
}


/////////////////////////////////////////////////////////////////////////////
//! gets a byte from the transmit buffer
//! \param[in] uart UART number (0..3)
//! \return -1 if UART not available
//! \return -2 if no new byte available
//! \return >= 0: transmitted byte
//! \note Applications shouldn't call these functions directly, instead please use \ref MIOS32_COM or \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_UART_TxBufferGet(u8 uart)
{
  if( uart >= MIOS32_UART_MAX_PORTS || tx_buffer_ptr[uart] == NULL )
    return -1; // UART not available

  if( !tx_buffer_size[uart] )
    return -2; // nothing new in buffer

  // get byte - this operation should be atomic!
  MIOS32_IRQ_Disable();
  u8 b = tx_buffer_ptr[uart][tx_buffer_tail[uart]];
  if( ++tx_buffer_tail[uart] >= MIOS32_UART_TX_BUFFER_SIZE )
    tx_buffer_tail[uart] = 0;
  --tx_buffer_size[uart];
  MIOS32_IRQ_Enable();

  return b; // return transmitted byte
}


/////////////////////////////////////////////////////////////////////////////
//! puts more than one byte onto the transmit buffer (used for atomic sends)
//! \param[in] uart UART number (0..3)
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
  if( uart >= MIOS32_UART_MAX_PORTS || tx_buffer_ptr[uart] == NULL )
    return -1; // UART not available

  if( (tx_buffer_size[uart]+len) >= MIOS32_UART_TX_BUFFER_SIZE )
    return -2; // buffer full or cannot get all requested bytes (retry)

  // copy bytes to be transmitted into transmit buffer
  // this operation should be atomic!
  MIOS32_IRQ_Disable();

  u16 i;
  for(i=0; i<len; ++i) {
    tx_buffer_ptr[uart][tx_buffer_head[uart]] = *buffer++;

    if( ++tx_buffer_head[uart] >= MIOS32_UART_TX_BUFFER_SIZE )
      tx_buffer_head[uart] = 0;

    // enable Tx interrupt if buffer was empty
    if( ++tx_buffer_size[uart] == 1 ) {
      switch( uart ) {
#if defined(MIOS32_USE_UART0)
        case 0: MIOS32_UART0->CR1 |= (1 << 7); break; // enable TXE interrupt (TXEIE=1)
#endif
#if defined(MIOS32_USE_UART1)
        case 1: MIOS32_UART1->CR1 |= (1 << 7); break; // enable TXE interrupt (TXEIE=1)
#endif
#if defined(MIOS32_USE_UART2)
        case 2: MIOS32_UART2->CR1 |= (1 << 7); break; // enable TXE interrupt (TXEIE=1)
#endif
#if defined(MIOS32_USE_UART3)
        case 3: MIOS32_UART3->CR1 |= (1 << 7); break; // enable TXE interrupt (TXEIE=1)
#endif
#if defined(MIOS32_USE_UART4)
        case 4: MIOS32_UART4->CR1 |= (1 << 7); break; // enable TXE interrupt (TXEIE=1)
#endif
#if defined(MIOS32_USE_UART5)
        case 5: MIOS32_UART5->CR1 |= (1 << 7); break; // enable TXE interrupt (TXEIE=1)
#endif
#if defined(MIOS32_USE_UART6)
        case 6: MIOS32_UART6->CR1 |= (1 << 7); break; // enable TXE interrupt (TXEIE=1) - LPUART shares the same CR1 layout
#endif
#if defined(MIOS32_USE_UART7)
        case 7: MIOS32_UART7->CR1 |= (1 << 7); break; // enable TXE interrupt (TXEIE=1) - LPUART shares the same CR1 layout
#endif
        default: MIOS32_IRQ_Enable(); return -3; // uart not supported by routine (yet)
      }
    }
  }

  MIOS32_IRQ_Enable();

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! puts more than one byte onto the transmit buffer (used for atomic sends)<BR>
//! (blocking function)
//! \param[in] uart UART number (0..3)
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
//! \param[in] uart UART number (0..3)
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
//! \param[in] uart UART number (0..3)
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
// Interrupt handler for UART0 (USART1) - independent NVIC vector
/////////////////////////////////////////////////////////////////////////////
#if defined(MIOS32_USE_UART1)
MIOS32_UART1_IRQHANDLER_FUNC
{
  if( MIOS32_UART1->ISR & (1 << 5) ) { // check if RXNE flag is set
    u8 b = MIOS32_UART1->RDR;
    UART_ACT_MARK_RX(1, b);
    s32 status = MIOS32_UART_IsAssignedToMIDI(1) ? MIOS32_MIDI_SendByteToRxCallback(DIN1, b) : 0;

    if( status == 0 && MIOS32_UART_RxBufferPut(1, b) < 0 ) {
      // here we could add some error handling
    }
  }

  if( MIOS32_UART1->ISR & (1 << 7) ) { // check if TXE flag is set
    if( MIOS32_UART_TxBufferUsed(1) > 0 ) {
      s32 b = MIOS32_UART_TxBufferGet(1);
      if( b < 0 ) {
	MIOS32_UART1->TDR = 0xff;
      } else {
	MIOS32_UART1->TDR = b;
	UART_ACT_MARK_TX(1, b);
      }
    } else {
      MIOS32_UART1->CR1 &= ~(1 << 7); // disable TXE interrupt (TXEIE=0)
    }
  }
}
#endif


/////////////////////////////////////////////////////////////////////////////
// Interrupt handler for UART1 (USART2) - independent NVIC vector on every
// tier except 6-USART+2xLPUART (G0B1/G0C1), where it's combined with UART7
// (LPUART2) instead (USART2_LPUART2_IRQn - see MIOS32_UART2_IRQHANDLER_FUNC,
// already tier-resolved to the right function name/signature above). Written
// as a single function body (like the UART2 group below) so the same source
// is correct on every tier: the UART7 block below simply never compiles in
// on tiers where UART7 doesn't exist.
/////////////////////////////////////////////////////////////////////////////
#if defined(MIOS32_USE_UART2) || defined(MIOS32_USE_UART7)
MIOS32_UART2_IRQHANDLER_FUNC
{
#if defined(MIOS32_USE_UART2)
  if( MIOS32_UART2->ISR & (1 << 5) ) { // check if RXNE flag is set
    u8 b = MIOS32_UART2->RDR;
    UART_ACT_MARK_RX(2, b);
    s32 status = MIOS32_UART_IsAssignedToMIDI(2) ? MIOS32_MIDI_SendByteToRxCallback(DIN2, b) : 0;

    if( status == 0 && MIOS32_UART_RxBufferPut(2, b) < 0 ) {
      // here we could add some error handling
    }
  }

  if( MIOS32_UART2->ISR & (1 << 7) ) { // check if TXE flag is set
    if( MIOS32_UART_TxBufferUsed(2) > 0 ) {
      s32 b = MIOS32_UART_TxBufferGet(2);
      if( b < 0 ) {
	MIOS32_UART2->TDR = 0xff;
      } else {
	MIOS32_UART2->TDR = b;
	UART_ACT_MARK_TX(2, b);
      }
    } else {
      MIOS32_UART2->CR1 &= ~(1 << 7); // disable TXE interrupt (TXEIE=0)
    }
  }
#endif

#if defined(MIOS32_USE_UART7)
  if( MIOS32_UART7->ISR & (1 << 5) ) { // check if RXNE flag is set
    u8 b = MIOS32_UART7->RDR;
    UART_ACT_MARK_RX(7, b);
    s32 status = MIOS32_UART_IsAssignedToMIDI(7) ? MIOS32_MIDI_SendByteToRxCallback(DIN7, b) : 0;

    if( status == 0 && MIOS32_UART_RxBufferPut(7, b) < 0 ) {
      // here we could add some error handling
    }
  }

  if( MIOS32_UART7->ISR & (1 << 7) ) { // check if TXE flag is set
    if( MIOS32_UART_TxBufferUsed(7) > 0 ) {
      s32 b = MIOS32_UART_TxBufferGet(7);
      if( b < 0 ) {
	MIOS32_UART7->TDR = 0xff;
      } else {
	MIOS32_UART7->TDR = b;
	UART_ACT_MARK_TX(7, b);
      }
    } else {
      MIOS32_UART7->CR1 &= ~(1 << 7); // disable TXE interrupt (TXEIE=0)
    }
  }
#endif
}
#endif


/////////////////////////////////////////////////////////////////////////////
// Interrupt handler for UART2 (USART3), UART3 (USART4), on the 6-USART
// tier UART4 (USART5)/UART5 (USART6) too, and on the "+LPUART" siblings of
// the 4-USART and 6-USART tiers, UART6 (LPUART1) as well (see
// MIOS32_G0_LPUART1_SHARED above - NOT on the 2-USART+LPUART1 tier, where
// UART6 has its own independent vector instead, serviced by the standalone
// handler further below). These peripherals share a single NVIC vector
// (see MIOS32_UART_SHARED_IRQ_CHANNEL above), so this ONE handler function
// services whichever of them is enabled. Each branch is written
// unconditionally here since the vector/handler name is shared regardless
// of which subset is active (on the plain 4-USART tier, UART4/UART5 are
// always force-undef'd, so those blocks simply never compile in there).
// uart_midi_act (line-activity flags for an optional indicator) now covers
// every port uniformly via UART_ACT_RX/TX(n) - it was a u8 with bits for
// ports 0..3 only, which left the higher ports silently unreported.
/////////////////////////////////////////////////////////////////////////////
#if defined(MIOS32_USE_UART0) || defined(MIOS32_USE_UART3) || defined(MIOS32_USE_UART4) || defined(MIOS32_USE_UART5) || (defined(MIOS32_USE_UART6) && defined(MIOS32_G0_LPUART1_SHARED))
MIOS32_UART0_IRQHANDLER_FUNC
{
#if defined(MIOS32_USE_UART0)
  if( MIOS32_UART0->ISR & (1 << 5) ) { // check if RXNE flag is set
    u8 b = MIOS32_UART0->RDR;
    UART_ACT_MARK_RX(0, b);
    s32 status = MIOS32_UART_IsAssignedToMIDI(0) ? MIOS32_MIDI_SendByteToRxCallback(DIN0, b) : 0;

    if( status == 0 && MIOS32_UART_RxBufferPut(0, b) < 0 ) {
      // here we could add some error handling
    }
  }

  if( MIOS32_UART0->ISR & (1 << 7) ) { // check if TXE flag is set
    if( MIOS32_UART_TxBufferUsed(0) > 0 ) {
      s32 b = MIOS32_UART_TxBufferGet(0);
      if( b < 0 ) {
	MIOS32_UART0->TDR = 0xff;
      } else {
	MIOS32_UART0->TDR = b;
	UART_ACT_MARK_TX(0, b);
      }
    } else {
      MIOS32_UART0->CR1 &= ~(1 << 7); // disable TXE interrupt (TXEIE=0)
    }
  }
#endif

#if defined(MIOS32_USE_UART3)
  if( MIOS32_UART3->ISR & (1 << 5) ) { // check if RXNE flag is set
    u8 b = MIOS32_UART3->RDR;
    UART_ACT_MARK_RX(3, b);
    s32 status = MIOS32_UART_IsAssignedToMIDI(3) ? MIOS32_MIDI_SendByteToRxCallback(DIN3, b) : 0;

    if( status == 0 && MIOS32_UART_RxBufferPut(3, b) < 0 ) {
      // here we could add some error handling
    }
  }

  if( MIOS32_UART3->ISR & (1 << 7) ) { // check if TXE flag is set
    if( MIOS32_UART_TxBufferUsed(3) > 0 ) {
      s32 b = MIOS32_UART_TxBufferGet(3);
      if( b < 0 ) {
	MIOS32_UART3->TDR = 0xff;
      } else {
	MIOS32_UART3->TDR = b;
	UART_ACT_MARK_TX(3, b);
      }
    } else {
      MIOS32_UART3->CR1 &= ~(1 << 7); // disable TXE interrupt (TXEIE=0)
    }
  }
#endif

#if defined(MIOS32_USE_UART4)
  if( MIOS32_UART4->ISR & (1 << 5) ) { // check if RXNE flag is set
    u8 b = MIOS32_UART4->RDR;
    UART_ACT_MARK_RX(4, b);
    s32 status = MIOS32_UART_IsAssignedToMIDI(4) ? MIOS32_MIDI_SendByteToRxCallback(DIN4, b) : 0;

    if( status == 0 && MIOS32_UART_RxBufferPut(4, b) < 0 ) {
      // here we could add some error handling
    }
  }

  if( MIOS32_UART4->ISR & (1 << 7) ) { // check if TXE flag is set
    if( MIOS32_UART_TxBufferUsed(4) > 0 ) {
      s32 b = MIOS32_UART_TxBufferGet(4);
      if( b < 0 ) {
	MIOS32_UART4->TDR = 0xff;
      } else {
	MIOS32_UART4->TDR = b;
	UART_ACT_MARK_TX(4, b);
      }
    } else {
      MIOS32_UART4->CR1 &= ~(1 << 7); // disable TXE interrupt (TXEIE=0)
    }
  }
#endif

#if defined(MIOS32_USE_UART5)
  if( MIOS32_UART5->ISR & (1 << 5) ) { // check if RXNE flag is set
    u8 b = MIOS32_UART5->RDR;
    UART_ACT_MARK_RX(5, b);
    s32 status = MIOS32_UART_IsAssignedToMIDI(5) ? MIOS32_MIDI_SendByteToRxCallback(DIN5, b) : 0;

    if( status == 0 && MIOS32_UART_RxBufferPut(5, b) < 0 ) {
      // here we could add some error handling
    }
  }

  if( MIOS32_UART5->ISR & (1 << 7) ) { // check if TXE flag is set
    if( MIOS32_UART_TxBufferUsed(5) > 0 ) {
      s32 b = MIOS32_UART_TxBufferGet(5);
      if( b < 0 ) {
	MIOS32_UART5->TDR = 0xff;
      } else {
	MIOS32_UART5->TDR = b;
	UART_ACT_MARK_TX(5, b);
      }
    } else {
      MIOS32_UART5->CR1 &= ~(1 << 7); // disable TXE interrupt (TXEIE=0)
    }
  }
#endif

#if defined(MIOS32_USE_UART6) && defined(MIOS32_G0_LPUART1_SHARED)
  if( MIOS32_UART6->ISR & (1 << 5) ) { // check if RXNE flag is set
    u8 b = MIOS32_UART6->RDR;
    UART_ACT_MARK_RX(6, b);
    s32 status = MIOS32_UART_IsAssignedToMIDI(6) ? MIOS32_MIDI_SendByteToRxCallback(DIN6, b) : 0;

    if( status == 0 && MIOS32_UART_RxBufferPut(6, b) < 0 ) {
      // here we could add some error handling
    }
  }

  if( MIOS32_UART6->ISR & (1 << 7) ) { // check if TXE flag is set
    if( MIOS32_UART_TxBufferUsed(6) > 0 ) {
      s32 b = MIOS32_UART_TxBufferGet(6);
      if( b < 0 ) {
	MIOS32_UART6->TDR = 0xff;
      } else {
	MIOS32_UART6->TDR = b;
	UART_ACT_MARK_TX(6, b);
      }
    } else {
      MIOS32_UART6->CR1 &= ~(1 << 7); // disable TXE interrupt (TXEIE=0)
    }
  }
#endif
}
#endif


/////////////////////////////////////////////////////////////////////////////
// Interrupt handler for UART6 (LPUART1) on the 2-USART+LPUART1 tier only,
// where it has its own independent vector (LPUART1_IRQn) rather than
// sharing the USART3+ group above (see MIOS32_G0_LPUART1_SHARED).
/////////////////////////////////////////////////////////////////////////////
#if defined(MIOS32_USE_UART6) && !defined(MIOS32_G0_LPUART1_SHARED)
MIOS32_UART6_IRQHANDLER_FUNC
{
  if( MIOS32_UART6->ISR & (1 << 5) ) { // check if RXNE flag is set
    u8 b = MIOS32_UART6->RDR;
    UART_ACT_MARK_RX(6, b);
    s32 status = MIOS32_UART_IsAssignedToMIDI(6) ? MIOS32_MIDI_SendByteToRxCallback(DIN6, b) : 0;

    if( status == 0 && MIOS32_UART_RxBufferPut(6, b) < 0 ) {
      // here we could add some error handling
    }
  }

  if( MIOS32_UART6->ISR & (1 << 7) ) { // check if TXE flag is set
    if( MIOS32_UART_TxBufferUsed(6) > 0 ) {
      s32 b = MIOS32_UART_TxBufferGet(6);
      if( b < 0 ) {
	MIOS32_UART6->TDR = 0xff;
      } else {
	MIOS32_UART6->TDR = b;
	UART_ACT_MARK_TX(6, b);
      }
    } else {
      MIOS32_UART6->CR1 &= ~(1 << 7); // disable TXE interrupt (TXEIE=0)
    }
  }
}
#endif


/////////////////////////////////////////////////////////////////////////////
//! Returns and clears the line-activity flags of every port (used to drive an
//! activity indicator): two bits per port, in port order - UARTn RX =
//! 1<<(2*n), TX = 1<<(2*n+1). So UART0 RX=0x01 TX=0x02, UART1 RX=0x04
//! TX=0x08, UART2 RX=0x10 TX=0x20, UART3 RX=0x40 TX=0x80, and so on up to
//! UART7 (0x4000/0x8000).
//! Flags are set on the raw byte as it crosses the wire, inside the RX/TX
//! interrupt - so they also catch traffic that never reaches the MIDI layer,
//! such as bytes pushed straight into a TX buffer by an application merging
//! one port onto another. MIDI clock (0xf8) is excluded, otherwise a synced
//! setup would light the indicator permanently.
//! \note Applications shouldn't call this function directly, instead please use \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
u32 MIOS32_UART_RXTX_Act(void){
	MIOS32_IRQ_Disable(); // an RX/TX interrupt may set a bit mid-update
	u32 status = uart_midi_act;
	uart_midi_act = 0;
	MIOS32_IRQ_Enable();

	return status; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Returns and clears the line-activity flags of ONE port, normalised to
//! MIOS32_UART_ACT_RX / MIOS32_UART_ACT_TX whatever the port number - so an
//! application asks about the port it means and never has to know where that
//! port's bits sit in the packed word:
//! \code
//!   if( MIOS32_UART_ActGet(2) & MIOS32_UART_ACT_RX ) // something came in on UART2
//! \endcode
//! Preferred over MIOS32_UART_RXTX_Act(): hardcoded masks over the packed
//! word silently pointed at the wrong ports the day UART numbering was
//! realigned, which is exactly the kind of breakage this accessor prevents.
//! \param[in] uart UART number (0..MIOS32_UART_MAX_PORTS-1)
//! \return activity flags for that port, 0 if none (or if uart is invalid)
/////////////////////////////////////////////////////////////////////////////
u32 MIOS32_UART_ActGet(u8 uart){
	if( uart >= MIOS32_UART_MAX_PORTS )
		return 0;

	u32 mask = UART_ACT_RX(uart) | UART_ACT_TX(uart);

	MIOS32_IRQ_Disable(); // read-modify-write against the RX/TX interrupts
	u32 act = uart_midi_act & mask;
	uart_midi_act &= ~mask;
	MIOS32_IRQ_Enable();

	return ((act & UART_ACT_RX(uart)) ? MIOS32_UART_ACT_RX : 0) |
	       ((act & UART_ACT_TX(uart)) ? MIOS32_UART_ACT_TX : 0);
}


//! \}

#endif /* MIOS32_USE_UART */
