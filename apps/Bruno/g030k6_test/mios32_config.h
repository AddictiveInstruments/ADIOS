// $Id: mios32_config.h 502 2009-05-09 14:20:30Z tk $
/*
 * Local MIOS32 configuration file
 *
 * this file allows to disable (or re-configure) default functions of MIOS32
 * available switches are listed in $MIOS32_PATH/modules/mios32/MIOS32_CONFIG.txt
 *
 */

#ifndef _MIOS32_CONFIG_H
#define _MIOS32_CONFIG_H

// The boot message which is print during startup and returned on a SysEx query
#define MIOS32_LCD_BOOT_MSG_LINE1 "G030K6 bare test"
#define MIOS32_LCD_BOOT_MSG_LINE2 "MIOS32_CORE_USE_FREERTOS"

// Nucleo-G030K6 onboard user LED is on PC6 (not the STM32G0xx family
// default of GPIOA/LL_GPIO_PIN_12 - see mios32_utils.c) - overridden here
// rather than in the template, this is board-specific wiring.
#define MIOS32_SOL_PORT GPIOC
#define MIOS32_SOL_PIN  LL_GPIO_PIN_6

// ---------------------------------------------------------------------------
// mios32_sys.c - core system init (clock, vector table, timebase). Always
// compiled, no on/off toggle - the CPU can't run without it.
//
// GPIO clocks: mios32_sys.c enables GPIOA/B/C/D(/F on G0xx) clocks for every
// project, so your own code or other drivers (mios32_spi.c, board-level
// code, etc.) never need to enable a GPIO port clock themselves.
//
// Clock config - override-able here without touching mios32_sys.c. Left
// commented = defaults (simplest/fastest, no crystal needed):
//   - STM32G0xx: HSI (16MHz internal RC) -> PLL -> 64MHz. No override point
//     exists yet on this family (no HSE code path in mios32_sys.c).
//   - STM32F4xx: HSI (16MHz internal RC) -> PLL -> 168MHz by default.
//#define MIOS32_SYS_CLOCK_SOURCE_HSE      // STM32F4xx only: use the 8MHz
                                            // crystal instead of HSI (e.g.
                                            // for tighter USB MIDI timing -
                                            // HSI is ~1% accurate, HSE isn't)
//#define PLL_N 336                        // only takes effect together with
                                            // MIOS32_SYS_CLOCK_SOURCE_HSE above
//
// CAUTION: MIOS32_SYS_CPU_FREQUENCY (the label mios32_sys.h exposes for the
// resulting SYSCLK) is a SEPARATE override point, declared in the shared
// header, not here - it does NOT recompute the PLL. If you override PLL_N/
// the clock source above and the resulting frequency changes, update
// MIOS32_SYS_CPU_FREQUENCY to match by hand, or every timer/baudrate
// calculation derived from it will silently be wrong.
//#define MIOS32_SYS_CPU_FREQUENCY 168000000
//
// RTC clock source - independent of the SYSCLK choice above (the backup
// domain keeps its own clocking regardless of a SYSCLK reconfiguration).
// Defaults to internal LSI (~32kHz RC, no crystal) on both families.
//#define MIOS32_SYS_RTC_CLOCK_SOURCE_HSE  // STM32F4xx only: RTC from HSE/16
                                            // instead of LSI - needs HSE
                                            // actually running (see above)
//#define MIOS32_SYS_RTC_ASYNCH_PRESCALER 127
//#define MIOS32_SYS_RTC_SYNCH_PRESCALER 255
// (RTC init itself is normally skipped via MIOS32_SYS_DONT_INIT_RTC below -
// the above only matters if you actually need the RTC/System Time feature.)

// ---------------------------------------------------------------------------
// mios32_irq.c - interrupt priority/enable helpers. Always compiled, no
// on/off toggle.

// ---------------------------------------------------------------------------
// mios32_utils.c - delay/timer/stopwatch/sign-of-life LED utilities. DELAY
// is always compiled (no toggle); TIMER/STOPWATCH/SOF are opt-in below.
// STOPWATCH + SOF are enabled by default here so the template has a working
// sign-of-life blink out of the box (see APP_Tick() in app.c) on any
// supported family without modification.
#define MIOS32_USE_STOPWATCH
// STOPWATCH now defaults to TIM17 (mios32_utils.c) - confirmed present on
// every STM32G0 tier including this board's G030K6 - no override needed
// here anymore.
#define MIOS32_USE_SOL
// MIOS32_SOL_PORT/MIOS32_SOL_PIN default to PA12 (same on every
// family/processor) - override both together here for custom hardware:
//#define MIOS32_SOL_PORT GPIOA				// (default)
//#define MIOS32_SOL_PIN  LL_GPIO_PIN_12		// (default)

// ---------------------------------------------------------------------------
// mios32_spi.c - left commented = SPI entirely disabled (default). Uncomment
// just the specific port(s) you actually need - MIOS32_USE_SPI itself is
// derived automatically from whichever MIOS32_USE_SPIx below are set (see
// mios32_spi.h), no need to define it separately:
//#define MIOS32_USE_SPI0
//#define MIOS32_USE_SPI1
//#define MIOS32_USE_SPI2                  // 3rd port - STM32F4xx (any board),
                                            // or STM32G0B0/G0B1/G0C1 only
//
// STM32G0xx: SPI1 (the 2nd port) is always the SPI2 peripheral, on every G0
// variant. SPI2 (the 3rd port) only exists on G0B0/G0B1/G0C1 - it's force-
// disabled at compile time on every other G0 chip.
//
// Chip select: each port has a single CS line, always plain GPIO (never an
// alternate function, even in slave mode). Control it with
// MIOS32_SPI_CS_PinSet(spi, value). A project needing a 2nd CS line per
// port drives that GPIO directly itself.
//
// CS pin - override both together per port for custom hardware:
// STM32G0xx defaults:
//#define MIOS32_SPI0_CS_PORT GPIOA			// (default)
//#define MIOS32_SPI0_CS_PIN  LL_GPIO_PIN_4	// (default)
//#define MIOS32_SPI1_CS_PORT GPIOB			// (default)
//#define MIOS32_SPI1_CS_PIN  LL_GPIO_PIN_12	// (default)
//#define MIOS32_SPI2_CS_PORT GPIOB			// (default, STM32G0B0/G0B1/G0C1 only)
//#define MIOS32_SPI2_CS_PIN  LL_GPIO_PIN_6	// (default, STM32G0B0/G0B1/G0C1 only)
// STM32F4xx defaults (MBHP_DIPCOREF4):
//#define MIOS32_SPI0_CS_PORT GPIOA			// (default)
//#define MIOS32_SPI0_CS_PIN  LL_GPIO_PIN_4	// (default)
//#define MIOS32_SPI1_CS_PORT GPIOB			// (default)
//#define MIOS32_SPI1_CS_PIN  LL_GPIO_PIN_1	// (default)
//#define MIOS32_SPI2_CS_PORT GPIOA			// (default)
//#define MIOS32_SPI2_CS_PIN  LL_GPIO_PIN_15	// (default)
//
// SCLK/MISO/MOSI can be moved the same way (port, pin AND alternate
// function - moving to a different pin usually means a different AF
// number too, check your chip's datasheet). Same override pattern for
// every port; SPI0 on STM32G0xx shown here as an example:
//#define MIOS32_SPI0_SCLK_PORT GPIOA			// (default)
//#define MIOS32_SPI0_SCLK_PIN  LL_GPIO_PIN_5	// (default)
//#define MIOS32_SPI0_SCLK_AF   LL_GPIO_AF_0	// (default)
//#define MIOS32_SPI0_MISO_PORT GPIOA			// (default)
//#define MIOS32_SPI0_MISO_PIN  LL_GPIO_PIN_6	// (default)
//#define MIOS32_SPI0_MISO_AF   LL_GPIO_AF_0	// (default)
//#define MIOS32_SPI0_MOSI_PORT GPIOA			// (default)
//#define MIOS32_SPI0_MOSI_PIN  LL_GPIO_PIN_7	// (default)
//#define MIOS32_SPI0_MOSI_AF   LL_GPIO_AF_0	// (default)
// SPI1/SPI2 follow the identical pattern (same _AF naming on both
// families) - see the #ifndef guards in mios32_spi.c for every default
// value.

// ---------------------------------------------------------------------------
// mios32_uart.c - UART0/UART1 enabled below by default (MIOS32_USE_UART0/1),
// since UART is the only reliable MIDI transport on STM32G0xx today (USB
// isn't implemented for that family yet). Default pin/peripheral assignment
// (STM32G070CB / F4xx MBHP_DIPCOREF4) - override individually for custom
// hardware, port/pin/AF/peripheral can all move independently, same pattern
// as mios32_spi.c:
//#define MIOS32_UART0_TX_PORT     GPIOB           // (default, STM32G0xx)
//#define MIOS32_UART0_TX_PIN      LL_GPIO_PIN_8    // (default, STM32G0xx)
//#define MIOS32_UART0_TX_AF       LL_GPIO_AF_4     // (default, STM32G0xx)
//#define MIOS32_UART0             USART3           // (default, STM32G0xx)
// see the #ifndef guards in mios32_uart.c for every default value on both
// families, and for UART1's equivalent set of overrides.
//
// More ports exist and can be turned on the same way (MIOS32_USE_UART2,
// MIOS32_USE_UART3, ...): STM32G070CB has 4 total (UART0..UART3), the
// STM32G0B0 6-USART tier has 6 (UART0..UART5), F4xx has up to 10
// (UART0..UART9) on the highest-tier chips - see the module-level comment
// in each family's mios32_uart.c for the full port list, force-undef
// conditions (some ports don't exist on every chip), and the shared-IRQ-
// vector caveats (differ by chip tier - a simple UART0/UART3 pair on
// STM32G070, up to 4 ports sharing one vector on STM32G0B0).
//#define MIOS32_USE_UART2
//#define MIOS32_USE_UART3
//
// If your board runs a TX pin through an inverting level-shifter (STM32G0xx
// only - no such hardware feature on STM32F4xx's USART peripheral):
//#define MIOS32_UART0_TX_INVERTED

// disable code modules
//#define MIOS32_USE_I2S
#define MIOS32_DONT_USE_AIN
#define MIOS32_DONT_USE_LCD
// (the MIDI core itself is always compiled - not optional, see mios32_midi.c;
// only the transports are opt-in: MIOS32_USE_DIN_MIDI etc, further below)
#define MIOS32_DONT_USE_OSC
#define MIOS32_DONT_USE_COM
//#define MIOS32_DONT_USE_USB
#define MIOS32_DONT_USE_USB_HOST
#define MIOS32_DONT_USE_USB_HS_HOST
//#define MIOS32_DONT_USE_USB_MIDI
//#define MIOS32_USE_USB_COM


// =============================================================================
// FreeRTOS: two independent, numeric (0/1) opt-in switches, both with a
// RAM/FLASH-tiered default (see include/mios32/mios32_sys.h for the exact
// logic) - nothing needs to be defined here unless you want to override
// that default. Numeric rather than plain #define presence specifically so
// EITHER direction of override is possible ("#define ... 0" to force off on
// a chip that would default to 1, or "... 1" to force on where it would
// default to 0) - a bare #undef can't express "explicitly off" as opposed
// to "undecided, apply the tier default".
//
//   MIOS32_APP_USE_FREERTOS  - is the FreeRTOS kernel itself compiled/
//                              linked in at all? (renamed from the old
//                              opt-out MIOS32_DONT_USE_FREERTOS)
//   MIOS32_CORE_USE_FREERTOS - does programming_models/traditional/main.c
//                              schedule the application Hooks (APP_Tick,
//                              APP_MIDI_Tick, DIN/ENC/AIN/COM callbacks...)
//                              via FreeRTOS tasks, or via a bare-metal
//                              super-loop timed by a dedicated SysTick
//                              handler instead?
//
// Both default to 0 (bare-metal, no FreeRTOS) on chips in the "small" tier
// - RAM <= 8K or FLASH <= 32K (physical chip specs: STM32G030K6, STM32G031K8
// today) - and to 1 (FreeRTOS) everywhere else. Why: measured empirically,
// FreeRTOS's own kernel + heap already consumes ~83% of a full G030K6 build
// and roughly half the *total* RAM on G031K8 (the heap alone, before any
// application code) - there just isn't enough room left for a real
// application otherwise.
//
// *** THE TRADE-OFF, READ THIS BEFORE RELYING ON BARE-METAL MODE ***
// With FreeRTOS (MIOS32_CORE_USE_FREERTOS=1, the traditional model): the
// Hooks run as two SEPARATE, PREEMPTIVELY SCHEDULED tasks (TASK_Hooks for
// DIN/ENC/AIN/COM/APP_Tick, TASK_MIDI_Hooks for MIDI). If an application
// hook blocks or runs long (a slow screen redraw, a busy-wait, anything),
// MIDI keeps being processed on schedule regardless, since TASK_MIDI_Hooks
// can still preempt it.
// Without FreeRTOS (=0, bare-metal super-loop): the two Hooks collapse into
// ONE sequential 1mS block, with nothing left to preempt anything. A slow
// or blocking application hook NOW ALSO DELAYS MIDI PROCESSING. This is the
// real price of the RAM/FLASH savings above - not just a smaller binary,
// a different concurrency model. If your application on a small chip does
// anything that can block or run long inside APP_Tick()/APP_Background()/
// a DIN or ENC callback, keep that in mind (or force
// MIOS32_CORE_USE_FREERTOS back to 1 and accept the RAM/FLASH cost instead).
//
// A related switch, MIOS32_CORE_USE_CANARI (also numeric, also tiered -
// defaults to 1 exactly when MIOS32_CORE_USE_FREERTOS is 0): adds a
// stack-overflow canary to the bare-metal loop, since FreeRTOS's own
// configCHECK_FOR_STACK_OVERFLOW protection is gone along with the kernel.
// Costs ~630 bytes FLASH, 0 extra RAM (reuses the existing stack region -
// only one stack left once tasks are gone, unlike FreeRTOS's per-task
// watermarking) - measured on a G030K6 build.
//
// Examples, only needed if you want to override the tiered default:
//#define MIOS32_APP_USE_FREERTOS 0
//#define MIOS32_CORE_USE_FREERTOS 0
//#define MIOS32_CORE_USE_CANARI 0
// =============================================================================

#if 0
// Following settings allow to customize the USB device descriptor
#define MIOS32_USB_VENDOR_ID    0x16c0        // sponsored by voti.nl! see http://www.voti.nl/pids
#define MIOS32_USB_VENDOR_STR   "midibox.org" // you will see this in the USB device description
#define MIOS32_USB_PRODUCT_STR  "app skeleton"  // you will see this in the MIDI device list
#define MIOS32_USB_PRODUCT_ID   0x03fe        // ==1022; 1020-1029 reserved for T.Klose, 1000 - 1009 free for lab use... 0x3fe is required if the GM5 driver should be used
#define MIOS32_USB_VERSION_ID   0x1010        // v1.010


// 1 to stay compatible to USB MIDI spec, 0 as workaround for some windows versions...
#define MIOS32_USB_MIDI_USE_AC_INTERFACE 1

// allowed number of USB MIDI ports: 1..8
#define MIOS32_USB_MIDI_NUM_PORTS 1

// buffer size (should be at least >= MIOS32_USB_MIDI_DATA_*_SIZE/4)
#define MIOS32_USB_MIDI_RX_BUFFER_SIZE  512 // packages
#define MIOS32_USB_MIDI_TX_BUFFER_SIZE  512 // packages

// size of IN/OUT pipe
#define MIOS32_USB_MIDI_DATA_IN_SIZE           64
#define MIOS32_USB_MIDI_DATA_OUT_SIZE          64
#endif

// enable BSL enhancements in MIOS32 SysEx parser
//#define MIOS32_MIDI_BSL_ENHANCEMENTS 0

// to save memory on STM32 build:
#if defined(MIOS32_FAMILY_STM32F4xx)
# define MIOS32_SYS_DONT_INIT_RTC
//# define MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
#define MIOS32_BOARD_J15_LED_NUM 1

#define MIOS32_USE_UART0
#define MIOS32_USE_UART1
#define MIOS32_USE_DIN_MIDI
#elif defined(MIOS32_FAMILY_STM32G0xx)
#define MIOS32_DONT_USE_USB
#define MIOS32_DONT_USE_USB_MIDI
# define MIOS32_SYS_DONT_INIT_RTC
//# define MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
#define MIOS32_BOARD_J15_LED_NUM 1

// BSL_RELAY_BEGIN - copied verbatim into the bootloader and updater builds
// by etc/gen_bsl_boundary.sh, so all three talk on the same connector.
// UART0 = USART1 on PB6 (TX) / PB7 (RX): the only possible choice on a
// G030K6, since MIOS32_UARTn is USART(n+1) and this chip has USART1/USART2
// only - no USART3, which is exactly what the old numbering silently asked
// for here. Plain MIDI output stage: no TX inversion, push-pull drive.
#define MIOS32_USE_DIN_MIDI
#define MIOS32_USE_UART0
#define MIOS32_MIDI_DEFAULT_PORT DIN0
#define MIOS32_MIDI_DEBUG_PORT DIN0
#define MIOS32_UART0_TX_OD 0
// BSL_RELAY_END

#endif

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
//     = 1032B (MIOS32_MINIMAL_STACK_SIZE=1024, already 8-aligned) -> 1136B
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
#ifndef MIOS32_HEAP_SIZE
#define MIOS32_HEAP_SIZE 4*1024
#endif

#endif /* _MIOS32_CONFIG_H */
