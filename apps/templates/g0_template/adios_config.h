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
#define ADIOS_APP_NAME1 "G0 template"
#define ADIOS_APP_NAME2 "starting point, edit me"
#define ADIOS_APP_VERSION "v1.000"

// Nucleo-G030K6 onboard user LED is on PC6 (not the STM32G0xx family
// default of GPIOA/LL_GPIO_PIN_12 - see adios_utils.c) - overridden here
// rather than in the template, this is board-specific wiring.
#define ADIOS_SOL_PORT GPIOC
#define ADIOS_SOL_PIN  LL_GPIO_PIN_6

// ---------------------------------------------------------------------------
// adios_sys.c - core system init (clock, vector table, timebase). Always
// compiled, no on/off toggle - the CPU can't run without it.
//
// GPIO clocks: adios_sys.c enables GPIOA/B/C/D(/F on G0xx) clocks for every
// project, so your own code or other drivers (adios_spi.c, board-level
// code, etc.) never need to enable a GPIO port clock themselves.
//
// Clock config - override-able here without touching adios_sys.c. Left
// commented = defaults (simplest/fastest, no crystal needed):
//   - STM32G0xx: HSI (16MHz internal RC) -> PLL -> 64MHz. No override point
//     exists yet on this family (no HSE code path in adios_sys.c).
//   - STM32F4xx: HSI (16MHz internal RC) -> PLL -> 168MHz by default.
//#define ADIOS_SYS_CLOCK_SOURCE_HSE      // STM32F4xx only: use the 8MHz
                                            // crystal instead of HSI (e.g.
                                            // for tighter USB MIDI timing -
                                            // HSI is ~1% accurate, HSE isn't)
//#define PLL_N 336                        // only takes effect together with
                                            // ADIOS_SYS_CLOCK_SOURCE_HSE above
//
// CAUTION: ADIOS_SYS_CPU_FREQUENCY (the label adios_sys.h exposes for the
// resulting SYSCLK) is a SEPARATE override point, declared in the shared
// header, not here - it does NOT recompute the PLL. If you override PLL_N/
// the clock source above and the resulting frequency changes, update
// ADIOS_SYS_CPU_FREQUENCY to match by hand, or every timer/baudrate
// calculation derived from it will silently be wrong.
//#define ADIOS_SYS_CPU_FREQUENCY 168000000
//
// RTC clock source - independent of the SYSCLK choice above (the backup
// domain keeps its own clocking regardless of a SYSCLK reconfiguration).
// Defaults to internal LSI (~32kHz RC, no crystal) on both families.
//#define ADIOS_SYS_RTC_CLOCK_SOURCE_HSE  // STM32F4xx only: RTC from HSE/16
                                            // instead of LSI - needs HSE
                                            // actually running (see above)
//#define ADIOS_SYS_RTC_ASYNCH_PRESCALER 127
//#define ADIOS_SYS_RTC_SYNCH_PRESCALER 255
// (RTC init itself is normally skipped via ADIOS_SYS_DONT_INIT_RTC below -
// the above only matters if you actually need the RTC/System Time feature.)

// ---------------------------------------------------------------------------
// adios_irq.c - interrupt priority/enable helpers. Always compiled, no
// on/off toggle.

// ---------------------------------------------------------------------------
// adios_utils.c - delay/timer/stopwatch/sign-of-life LED utilities. DELAY
// is always compiled (no toggle); TIMER/STOPWATCH/SOF are opt-in below.
// STOPWATCH + SOF are enabled by default here so the template has a working
// sign-of-life blink out of the box (see APP_Tick() in app.c) on any
// supported family without modification.
#define ADIOS_USE_STOPWATCH
// STOPWATCH now defaults to TIM17 (adios_utils.c) - confirmed present on
// every STM32G0 tier including this board's G030K6 - no override needed
// here anymore.
#define ADIOS_USE_SOL
// ADIOS_SOL_PORT/ADIOS_SOL_PIN default to PA12 (same on every
// family/processor) - override both together here for custom hardware:
//#define ADIOS_SOL_PORT GPIOA				// (default)
//#define ADIOS_SOL_PIN  LL_GPIO_PIN_12		// (default)

// ---------------------------------------------------------------------------
// adios_spi.c - left commented = SPI entirely disabled (default). Uncomment
// just the specific port(s) you actually need - ADIOS_USE_SPI itself is
// derived automatically from whichever ADIOS_USE_SPIx below are set (see
// adios_spi.h), no need to define it separately:
//#define ADIOS_USE_SPI0
//#define ADIOS_USE_SPI1
//#define ADIOS_USE_SPI2                  // 3rd port - STM32F4xx (any board),
                                            // or STM32G0B0/G0B1/G0C1 only
//
// STM32G0xx: SPI1 (the 2nd port) is always the SPI2 peripheral, on every G0
// variant. SPI2 (the 3rd port) only exists on G0B0/G0B1/G0C1 - it's force-
// disabled at compile time on every other G0 chip.
//
// Chip select: each port has a single CS line, always plain GPIO (never an
// alternate function, even in slave mode). Control it with
// ADIOS_SPI_CS_PinSet(spi, value). A project needing a 2nd CS line per
// port drives that GPIO directly itself.
//
// CS pin - override both together per port for custom hardware:
// STM32G0xx defaults:
//#define ADIOS_SPI0_CS_PORT GPIOA			// (default)
//#define ADIOS_SPI0_CS_PIN  LL_GPIO_PIN_4	// (default)
//#define ADIOS_SPI1_CS_PORT GPIOB			// (default)
//#define ADIOS_SPI1_CS_PIN  LL_GPIO_PIN_12	// (default)
//#define ADIOS_SPI2_CS_PORT GPIOB			// (default, STM32G0B0/G0B1/G0C1 only)
//#define ADIOS_SPI2_CS_PIN  LL_GPIO_PIN_6	// (default, STM32G0B0/G0B1/G0C1 only)
// STM32F4xx defaults (MBHP_DIPCOREF4):
//#define ADIOS_SPI0_CS_PORT GPIOA			// (default)
//#define ADIOS_SPI0_CS_PIN  LL_GPIO_PIN_4	// (default)
//#define ADIOS_SPI1_CS_PORT GPIOB			// (default)
//#define ADIOS_SPI1_CS_PIN  LL_GPIO_PIN_1	// (default)
//#define ADIOS_SPI2_CS_PORT GPIOA			// (default)
//#define ADIOS_SPI2_CS_PIN  LL_GPIO_PIN_15	// (default)
//
// SCLK/MISO/MOSI can be moved the same way (port, pin AND alternate
// function - moving to a different pin usually means a different AF
// number too, check your chip's datasheet). Same override pattern for
// every port; SPI0 on STM32G0xx shown here as an example:
//#define ADIOS_SPI0_SCLK_PORT GPIOA			// (default)
//#define ADIOS_SPI0_SCLK_PIN  LL_GPIO_PIN_5	// (default)
//#define ADIOS_SPI0_SCLK_AF   LL_GPIO_AF_0	// (default)
//#define ADIOS_SPI0_MISO_PORT GPIOA			// (default)
//#define ADIOS_SPI0_MISO_PIN  LL_GPIO_PIN_6	// (default)
//#define ADIOS_SPI0_MISO_AF   LL_GPIO_AF_0	// (default)
//#define ADIOS_SPI0_MOSI_PORT GPIOA			// (default)
//#define ADIOS_SPI0_MOSI_PIN  LL_GPIO_PIN_7	// (default)
//#define ADIOS_SPI0_MOSI_AF   LL_GPIO_AF_0	// (default)
// SPI1/SPI2 follow the identical pattern (same _AF naming on both
// families) - see the #ifndef guards in adios_spi.c for every default
// value.

// ---------------------------------------------------------------------------
// adios_uart.c - UART0/UART1 enabled below by default (ADIOS_USE_UART0/1),
// since UART is the only reliable MIDI transport on STM32G0xx today (USB
// isn't implemented for that family yet). Default pin/peripheral assignment
// (STM32G070CB / F4xx MBHP_DIPCOREF4) - override individually for custom
// hardware, port/pin/AF/peripheral can all move independently, same pattern
// as adios_spi.c:
//#define ADIOS_UART0_TX_PORT     GPIOB           // (default, STM32G0xx)
//#define ADIOS_UART0_TX_PIN      LL_GPIO_PIN_8    // (default, STM32G0xx)
//#define ADIOS_UART0_TX_AF       LL_GPIO_AF_4     // (default, STM32G0xx)
//#define ADIOS_UART0             USART3           // (default, STM32G0xx)
// see the #ifndef guards in adios_uart.c for every default value on both
// families, and for UART1's equivalent set of overrides.
//
// More ports exist and can be turned on the same way (ADIOS_USE_UART2,
// ADIOS_USE_UART3, ...): STM32G070CB has 4 total (UART0..UART3), the
// STM32G0B0 6-USART tier has 6 (UART0..UART5), F4xx has up to 10
// (UART0..UART9) on the highest-tier chips - see the module-level comment
// in each family's adios_uart.c for the full port list, force-undef
// conditions (some ports don't exist on every chip), and the shared-IRQ-
// vector caveats (differ by chip tier - a simple UART0/UART3 pair on
// STM32G070, up to 4 ports sharing one vector on STM32G0B0).
//#define ADIOS_USE_UART2
//#define ADIOS_USE_UART3
//
// If your board runs a TX pin through an inverting level-shifter (STM32G0xx
// only - no such hardware feature on STM32F4xx's USART peripheral):
//#define ADIOS_UART0_TX_INVERTED

// disable code modules
//#define ADIOS_USE_I2S
#define ADIOS_DONT_USE_AIN
#define ADIOS_DONT_USE_LCD
// (the MIDI core itself is always compiled - not optional, see adios_midi.c;
// only the transports are opt-in: ADIOS_USE_DIN_MIDI etc, further below)
//#define ADIOS_DONT_USE_USB
#define ADIOS_DONT_USE_USB_HOST
#define ADIOS_DONT_USE_USB_HS_HOST
//#define ADIOS_DONT_USE_USB_MIDI
//#define ADIOS_USE_USB_COM


// =============================================================================
// FreeRTOS: presence/absence switches, like every other ADIOS_USE_* opt-in
// (see include/adios/adios_sys.h). Nothing needs to be defined here on most
// projects - the chip decides the default:
//
//   FLASH <= 32K or RAM <= 8K (real figures from etc/ld/<family>.ld.S)
//     -> bare-metal: core/core.mk defines ADIOS_CORE_DONT_USE_FREERTOS
//        itself and the kernel is not even compiled.
//   bigger chips
//     -> the traditional FreeRTOS model, scheduler on.
//
//   ADIOS_CORE_DONT_USE_FREERTOS - opt-OUT: define it here to force the
//                              bare-metal build on a big chip too.
//   ADIOS_APP_USE_FREERTOS   - opt-in: declare it when the APPLICATION
//                              ITSELF calls FreeRTOS (tasks, queues,
//                              semaphores...). Incompatible with the
//                              opt-out above - the build refuses the
//                              combination, since core/main.c is who
//                              starts the scheduler.
//
// *** THE TRADE-OFF, READ THIS BEFORE RELYING ON BARE-METAL MODE ***
// With FreeRTOS (the traditional model): the Hooks run as two SEPARATE,
// PREEMPTIVELY SCHEDULED tasks (TASK_Hooks for DIN/ENC/AIN/APP_Tick,
// TASK_MIDI_Hooks for MIDI). If an application hook blocks or runs long
// (a slow screen redraw, a busy-wait, anything), MIDI keeps being
// processed on schedule regardless, since TASK_MIDI_Hooks can still
// preempt it.
// Bare-metal (ADIOS_CORE_DONT_USE_FREERTOS): the two Hooks collapse into
// ONE sequential 1mS block, with nothing left to preempt anything. A slow
// or blocking application hook NOW ALSO DELAYS MIDI PROCESSING. This is
// the real price of the RAM/FLASH savings (measured: FreeRTOS costs ~83%
// of a full G030K6 build, about half the total RAM of a G031K8) - not
// just a smaller binary, a different concurrency model.
//
// A related switch, ADIOS_CORE_USE_CANARI (numeric 0/1 - defaults to 1
// exactly when the core runs bare-metal): adds a stack-overflow canary to
// the super-loop, since FreeRTOS's own configCHECK_FOR_STACK_OVERFLOW
// protection is gone along with the kernel. Costs ~630 bytes FLASH, 0
// extra RAM (reuses the existing stack region - only one stack left once
// tasks are gone) - measured on a G030K6 build.
//
// Examples:
//#define ADIOS_CORE_DONT_USE_FREERTOS
//#define ADIOS_APP_USE_FREERTOS
//#define ADIOS_CORE_USE_CANARI 0
// =============================================================================


// enable BSL enhancements in ADIOS SysEx parser
//#define ADIOS_MIDI_BSL_ENHANCEMENTS 0

// to save memory on STM32 build:
#define ADIOS_DONT_USE_USB
#define ADIOS_DONT_USE_USB_MIDI
# define ADIOS_SYS_DONT_INIT_RTC
//# define ADIOS_MIDI_DISABLE_DEBUG_MESSAGE

// BSL_RELAY_BEGIN - copied verbatim into the bootloader and updater builds
// by etc/gen_bsl_boundary.sh, so all three talk on the same connector.
// UART0 = USART1 on PB6 (TX) / PB7 (RX): the only possible choice on a
// G030K6, since ADIOS_UARTn is USART(n+1) and this chip has USART1/USART2
// only - no USART3, which is exactly what the old numbering silently asked
// for here. Plain MIDI output stage: no TX inversion, push-pull drive.
#define ADIOS_USE_DIN_MIDI
#define ADIOS_USE_UART0
#define ADIOS_MIDI_DEFAULT_PORT DIN0
#define ADIOS_MIDI_DEBUG_PORT DIN0
#define ADIOS_UART0_TX_OD 0
// BSL_RELAY_END

// FreeRTOS heap, sized for the minimal task set this template actually
// creates by default (Idle + TASK_Hooks + TASK_MIDI_Hooks, no software
// timer task - configUSE_TIMERS is 0), instead of the generic 6*1024
// default in FreeRTOSConfig.h - matters most on the smallest G0 RAM
// budgets (8K total on G030K6/G031K8). Calculated, not just guessed:
//   - sizeof(TCB_t) measured at 92 bytes for this exact FreeRTOSConfig.h
//     (configUSE_MUTEXES/RECURSIVE_MUTEXES/TRACE_FACILITY=1, no MPU/
//     TrustZone extra fields, GCC ARM_CM0 32-bit pointers) via a one-off
//     compile-time probe against FreeRTOS/Source/tasks.c - re-measure if
//     you change those FreeRTOSConfig.h settings, this number isn't a
//     generic FreeRTOS constant.
//   - each task = 2 separate heap_4 pvPortMalloc() calls (TCB, then
//     stack), each with an 8-byte BlockLink_t header rounded up to the
//     8-byte alignment (portBYTE_ALIGNMENT): TCB block = 104B, stack block
//     = 1032B (ADIOS_MINIMAL_STACK_SIZE=1024, already 8-aligned) -> 1136B
//     per task.
//   - 3 tasks (Idle, Hooks, MIDI_Hooks) x 1136B = 3408B, +8B heap_4 end
//     sentinel = 3416B hard minimum for this exact task set.
// 4*1024 leaves ~680 bytes of headroom above that minimum (e.g. for a
// mutex/queue the application code might add) without carrying the
// original default's ~2.7KB of unused slack. If your app creates
// additional tasks/queues/mutexes, raise this accordingly - a heap
// allocation failure past this point is easy to miss, since it triggers
// vApplicationMallocFailedHook silently at runtime rather than a build
// error (configUSE_MALLOC_FAILED_HOOK=1 in FreeRTOSConfig.h).
#ifndef ADIOS_HEAP_SIZE
#define ADIOS_HEAP_SIZE 4*1024
#endif

#endif /* _ADIOS_CONFIG_H */
