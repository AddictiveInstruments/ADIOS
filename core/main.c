/*
 * Traditional Programming Model
 * Provides similar hooks like PIC based MIOS8 to the application
 *
 * ==========================================================================
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
#include <app.h>

// ===========================================================================
// ADIOS_CORE_USE_FREERTOS (see adios_sys.h for the auto-derived default,
// tiered by RAM/FLASH budget - RAM<=8K or FLASH<=32K defaults this OFF):
// decides how the application Hooks below are scheduled.
//
//   - 1: the traditional FreeRTOS model - TASK_Hooks and TASK_MIDI_Hooks
//     run as two SEPARATE, PREEMPTIVELY SCHEDULED tasks. If an application
//     hook (APP_Tick, a DIN/ENC/AIN/COM callback...) blocks or runs long,
//     MIDI keeps being processed on schedule regardless, since
//     TASK_MIDI_Hooks can still preempt it.
//
//   - 0: a bare-metal super-loop, no FreeRTOS kernel involved at all here
//     (see ADIOS_APP_USE_FREERTOS for the kernel-presence switch,
//     independent of this one) - timed by a dedicated SysTick handler
//     instead of the FreeRTOS tick (ADIOS_STOPWATCH is deliberately NOT
//     used for this: it must stay free for the application's own timing
//     needs). The two Hooks collapse into ONE sequential 1mS block - THIS
//     REMOVES THE ISOLATION DESCRIBED ABOVE: a slow/blocking application
//     hook now delays MIDI processing too, since everything runs on the
//     same single thread of execution with nothing left to preempt it.
//     Worth it on the smallest chips only because of what FreeRTOS itself
//     costs there - measured empirically at ~83% of a full G030K6 build,
//     and roughly half the total RAM on G031K8 (heap alone, before any
//     application code) - see apps/templates/app_skeleton/adios_config.h
//     for the full writeup.
//
// Both switches are numeric (0/1), not plain #define presence - checked
// with "#if", not "#ifdef" - specifically so a project CAN override either
// one to 0 even where the RAM/FLASH tier would otherwise default it to 1
// (a bare #undef can't express "explicitly off" vs. "undecided, apply the
// tier default"). Override in your own adios_config.h, e.g.
// "#define ADIOS_CORE_USE_FREERTOS 0".
// ===========================================================================
#if ADIOS_CORE_USE_FREERTOS
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#endif


/////////////////////////////////////////////////////////////////////////////
// External Prototypes
/////////////////////////////////////////////////////////////////////////////

extern void __libc_init_array(void);  /* calls CTORS of static objects */


/////////////////////////////////////////////////////////////////////////////
// Task Priorities and stack sizes (ADIOS_CORE_USE_FREERTOS only - a bare
// super-loop has no separate task stacks, everything runs on the single
// main() stack sized by the linker script)
/////////////////////////////////////////////////////////////////////////////

#if ADIOS_CORE_USE_FREERTOS
#define PRIORITY_TASK_HOOKS		( tskIDLE_PRIORITY + 3 )

#ifndef ADIOS_TASK_HOOKS_STACK_SIZE
# if defined(ADIOS_USE_USB_HOST_MIDI) || defined(ADIOS_USE_USB_HOST_HID) || defined(ADIOS_USE_USB_HOST_MSC)
// The USB HOST stack runs from this task, and its enumeration path is deep:
// control transfers, descriptor parsing, hub, class drivers. The device stack
// is nothing like as demanding - it only ever reacts to what a host asks - so
// this extra room is charged to host mode alone.
//
// Getting it wrong is brutal rather than gradual, which is why the default
// moves instead of being left to each project: FreeRTOS's overflow check
// fires, vApplicationStackOverflowHook() calls _abort(), and EVERY task stops
// while interrupts keep running. The machine then still enumerates over USB
// and answers nothing - a symptom that points nowhere near a stack.
// Expressed with ADIOS_MINIMAL_STACK_SIZE, which is a plain number of BYTES,
// and not with configMINIMAL_STACK_SIZE, which is words AND carries a cast -
// a cast the preprocessor cannot evaluate, so the check further down could
// not be written against it.
#  define ADIOS_TASK_HOOKS_STACK_SIZE (3*(ADIOS_MINIMAL_STACK_SIZE))
# else
#  define ADIOS_TASK_HOOKS_STACK_SIZE (ADIOS_MINIMAL_STACK_SIZE)
# endif
#endif
#ifndef ADIOS_TASK_MIDI_HOOKS_STACK_SIZE
# if defined(ADIOS_USE_USB_HOST_MIDI) || defined(ADIOS_USE_USB_HOST_HID) || defined(ADIOS_USE_USB_HOST_MSC)
// This is the task that does the deep work on a request: SysEx parser,
// building the reply, sprintf, and the whole USB write path underneath. A
// host-enabled build pushes that path just past one minimal stack - and a
// TRANSIENT overshoot is the worst failure there is: overflow check 1 only
// looks at the stack pointer on a context switch, so a spike that recedes in
// time is never seen, it just corrupts whatever the heap placed next door.
// (Check mode 2, set in FreeRTOSConfig.h, does catch these - keep it there.)
#  define ADIOS_TASK_MIDI_HOOKS_STACK_SIZE (2*(ADIOS_MINIMAL_STACK_SIZE))
# else
#  define ADIOS_TASK_MIDI_HOOKS_STACK_SIZE (ADIOS_MINIMAL_STACK_SIZE)
# endif
#endif

// Every task stack is carved out of the FreeRTOS heap, and an allocation that
// does not fit does NOT degrade gracefully: xTaskCreate() simply fails, the
// scheduler never starts, and the machine looks stone dead - no USB, no LED,
// nothing to read but a task count of zero. Catch it here, where the numbers
// are still visible, rather than on the bench.
//
// The idle task takes one minimal stack; the rest is control blocks, queues
// and the allocator's own overhead.
#define ADIOS_TASK_STACK_TOTAL ((ADIOS_TASK_HOOKS_STACK_SIZE) + (ADIOS_TASK_MIDI_HOOKS_STACK_SIZE) + (ADIOS_MINIMAL_STACK_SIZE))
#if (ADIOS_HEAP_SIZE) < ((ADIOS_TASK_STACK_TOTAL) + 1024)
# error "ADIOS_HEAP_SIZE is too small for the task stacks this build asks for: raise it in your adios_config.h. Asking for a USB HOST class enlarges the Hooks stack, which is the usual reason to land here."
#endif
#endif


/////////////////////////////////////////////////////////////////////////////
// Local prototypes
/////////////////////////////////////////////////////////////////////////////
#if ADIOS_CORE_USE_FREERTOS
static void TASK_Hooks(void *pvParameters);
static void TASK_MIDI_Hooks(void *pvParameters);
#endif
static void ADIOS_CORE_NonMIDI_Tick(void);
static void ADIOS_CORE_MIDI_Tick(void);
#if !ADIOS_CORE_USE_FREERTOS
static void ADIOS_CORE_BareLoop_Run(void);
#endif


/////////////////////////////////////////////////////////////////////////////
// FreeRTOS Heap
/////////////////////////////////////////////////////////////////////////////

#if configAPPLICATION_ALLOCATED_HEAP
# if defined(ADIOS_FAMILY_LPC17xx) && !defined(ADIOS_FREERTOS_HEAP_SECTION)
#  define ADIOS_FREERTOS_HEAP_SECTION __attribute__ ((section (".bss_ahb")))
# else
#  define ADIOS_FREERTOS_HEAP_SECTION
# endif

uint8_t ADIOS_FREERTOS_HEAP_SECTION ucHeap[configTOTAL_HEAP_SIZE];
#endif


/////////////////////////////////////////////////////////////////////////////
// Dummies for APP_Tick and APP_MIDI_Tick (if not used in app.c)
/////////////////////////////////////////////////////////////////////////////

__attribute__ ((weak)) void APP_Tick(void)
{
}

__attribute__ ((weak)) void APP_MIDI_Tick(void)
{
}


/////////////////////////////////////////////////////////////////////////////
// Main function
/////////////////////////////////////////////////////////////////////////////
int main(void)
{
  // initialize hardware and ADIOS modules
#ifndef ADIOS_DONT_USE_SYS
  ADIOS_SYS_Init(0);
#endif
#ifndef ADIOS_DONT_USE_DELAY
  ADIOS_DELAY_Init(0);
#endif
#ifdef ADIOS_USE_TIMESTAMP
  ADIOS_TIMESTAMP_Init(0);
#endif
#ifdef ADIOS_USE_SOL
  // sign-of-life LED: opt in with ADIOS_USE_SOL and name the pin with
  // ADIOS_SOL_PORT / ADIOS_SOL_PIN, see adios_utils.h
  ADIOS_SOL_Init();
#endif
#ifdef ADIOS_USE_SPI
  ADIOS_SPI_Init(0);
#endif
#ifdef ADIOS_USE_I2C
  // auto-derived in adios_i2c.h from any ADIOS_USE_I2C0/1/2 or
  // ADIOS_USE_FMPI2C0 the project declared
  ADIOS_I2C_Init(0);
#endif
#ifdef ADIOS_USE_SRIO
  ADIOS_SRIO_Init(0);
#endif
#ifdef ADIOS_USE_SRIN
  ADIOS_SRIN_Init(0);
#endif
#ifdef ADIOS_USE_SROUT
  ADIOS_SROUT_Init(0);
#endif
#ifdef ADIOS_USE_ENC
  ADIOS_ENC_Init(0);
#endif
#ifdef ADIOS_USE_ADC
  ADIOS_ADC_Init(0);
#endif
#ifdef ADIOS_USE_DAC
  ADIOS_DAC_Init(0);
#endif
  // the MIDI core is always initialized - it is not optional (2026-08-09,
  // see adios_midi.c): only the transports underneath are opt-in
  ADIOS_MIDI_Init(0);
#ifndef ADIOS_DONT_USE_USB
  ADIOS_USB_Init(0);
#endif
  // NO DISPLAY IS INITIALISED HERE. A screen is the application's business:
  // call APP_LCD_Init(0) from APP_Init(), at the point in your own sequence
  // where it belongs - see the commented line in the template's app.c.
#ifdef ADIOS_USE_I2S
  ADIOS_I2S_Init(0);
#endif

  // call C++ constructors
  __libc_init_array();

  // initialize application
  APP_Init();

  // (no boot screen and no startup delay: an application that wants to
  // greet the user does it from APP_Init(), where it also decides how long
  // to wait. Its name and version are reported over MIDI regardless - see
  // ADIOS_APP_NAME1/2 in adios_midi.h.)

#if ADIOS_CORE_USE_FREERTOS
  // start the task which calls the application hooks
  xTaskCreate(TASK_Hooks, "Hooks", (ADIOS_TASK_HOOKS_STACK_SIZE)/4, NULL, PRIORITY_TASK_HOOKS, NULL);
  xTaskCreate(TASK_MIDI_Hooks, "MIDI_Hooks", (ADIOS_TASK_MIDI_HOOKS_STACK_SIZE)/4, NULL, PRIORITY_TASK_HOOKS, NULL);
  //ADIOS_BOARD_LED_Set(1, 1);
  // start the scheduler
  vTaskStartScheduler();

  // Will only get here if there was not enough heap space to create the idle task
  return 0;
#else
  // bare-metal super-loop - see the ADIOS_CORE_USE_FREERTOS module-level
  // comment above for what this trades away.
  ADIOS_CORE_BareLoop_Run(); // never returns
  return 0; // never reached
#endif
}


/////////////////////////////////////////////////////////////////////////////
// Application Tick Hook (called by FreeRTOS each mS)
/////////////////////////////////////////////////////////////////////////////
void SRIO_ServiceFinish(void)
{
#ifdef ADIOS_USE_SRIO

# ifdef ADIOS_USE_ENC
  // update encoder states
  ADIOS_ENC_UpdateStates();
# endif

  // notify application about finished SRIO scan
  APP_SRIO_ServiceFinish();
#endif
}

#if ADIOS_CORE_USE_FREERTOS
void vApplicationTickHook(void)
{
#ifdef ADIOS_USE_TIMESTAMP
  ADIOS_TIMESTAMP_Inc();
#endif

#if defined(ADIOS_USE_SRIO) && !defined(ADIOS_DONT_SERVICE_SRIO_SCAN)
  // notify application about SRIO scan start
  APP_SRIO_ServicePrepare();

  // start next SRIO scan - IRQ notification to SRIO_ServiceFinish()
  ADIOS_SRIO_ScanStart(SRIO_ServiceFinish);
#endif
}


/////////////////////////////////////////////////////////////////////////////
// Idle Hook (called by FreeRTOS when nothing else to do)
/////////////////////////////////////////////////////////////////////////////
void vApplicationIdleHook(void)
{
  APP_Background();

}
#endif


/////////////////////////////////////////////////////////////////////////////
// Shared 1mS hook bodies - the ONE place that defines what runs each
// millisecond, called from either the FreeRTOS tasks below or the
// bare-metal super-loop further down (ADIOS_CORE_USE_FREERTOS=0) -
// deliberately factored out so the two scheduling modes can never drift
// apart from each other.
/////////////////////////////////////////////////////////////////////////////
static void ADIOS_CORE_MIDI_Tick(void)
{
  // handle timeout/expire counters and USB packages
  ADIOS_MIDI_Periodic_mS();

  // check for incoming MIDI packages and call hook
  ADIOS_MIDI_Receive_Handler(APP_MIDI_NotifyPackage);

  // optional application specific hook
  // helps to save memory (re-use the TASK_Hooks for other purposes...)
  APP_MIDI_Tick();
}

static void ADIOS_CORE_NonMIDI_Tick(void)
{
#if defined(ADIOS_USE_USB_MIDI)
  // Drive the USB stack. One call whatever a port is doing - device or host,
  // one port or several: deciding what to run is the USB layer's job, not
  // this loop's.
  ADIOS_USB_Handler();
#endif

#ifdef ADIOS_USE_SRIN
  // check for input shift register pin changes, call APP_SRIN_NotifyToggle on
  // each toggled pin. The application-facing hook keeps its DIN name on
  // purpose: what an app sees is still a digital input pin, whatever the
  // driver that scans the chain is called.
  ADIOS_SRIN_Handler(APP_SRIN_NotifyToggle);

  // check for encoder changes, call APP_ENC_NotifyChanged on each change
# ifdef ADIOS_USE_ENC
  ADIOS_ENC_Handler(APP_ENC_NotifyChange);
# endif
#endif

#if defined(ADIOS_USE_ADC) && !defined(ADIOS_DONT_SERVICE_ADC)
  // check for ADC channel changes, call APP_ADC_NotifyChange on each change
  ADIOS_ADC_Handler(APP_ADC_NotifyChange);
#endif

  // optional APP_Tick() hook
  // helps to save memory (re-use the TASK_Hooks for other purposes...)
  APP_Tick();
}


#if ADIOS_CORE_USE_FREERTOS
/////////////////////////////////////////////////////////////////////////////
// MIDI task (separated from TASK_Hooks() to ensure parallel handling of
// MIDI events if a hook in TASK_Hooks() blocks)
/////////////////////////////////////////////////////////////////////////////
static void TASK_MIDI_Hooks(void *pvParameters)
{
  portTickType xLastExecutionTime;

  // Initialise the xLastExecutionTime variable on task entry
  xLastExecutionTime = xTaskGetTickCount();

  while( 1 ) {
    vTaskDelayUntil(&xLastExecutionTime, 1 / portTICK_RATE_MS);

    // skip delay gap if we had to wait for more than 5 ticks to avoid
    // unnecessary repeats until xLastExecutionTime reached xTaskGetTickCount() again
    portTickType xCurrentTickCount = xTaskGetTickCount();
    if( xLastExecutionTime < (xCurrentTickCount-5) )
      xLastExecutionTime = xCurrentTickCount;

    ADIOS_CORE_MIDI_Tick();
  }
}


/////////////////////////////////////////////////////////////////////////////
// Remaining application hooks
/////////////////////////////////////////////////////////////////////////////
static void TASK_Hooks(void *pvParameters)
{

  portTickType xLastExecutionTime;

  // Initialise the xLastExecutionTime variable on task entry
  xLastExecutionTime = xTaskGetTickCount();

  while( 1 ) {
    vTaskDelayUntil(&xLastExecutionTime, 1 / portTICK_RATE_MS);

    // skip delay gap if we had to wait for more than 5 ticks to avoid
    // unnecessary repeats until xLastExecutionTime reached xTaskGetTickCount() again
    portTickType xCurrentTickCount = xTaskGetTickCount();
    if( xLastExecutionTime < (xCurrentTickCount-5) )
      xLastExecutionTime = xCurrentTickCount;

    ADIOS_CORE_NonMIDI_Tick();
  }
}
#endif


#if !ADIOS_CORE_USE_FREERTOS
/////////////////////////////////////////////////////////////////////////////
// Bare-metal super-loop (ADIOS_CORE_USE_FREERTOS=0) - replaces
// vTaskStartScheduler()/TASK_Hooks/TASK_MIDI_Hooks/vApplicationTickHook/
// vApplicationIdleHook above. Timed by a dedicated SysTick handler rather
// than FreeRTOS's tick or ADIOS_STOPWATCH (kept free for the application -
// see the module-level comment near the top of this file).
/////////////////////////////////////////////////////////////////////////////

static volatile u32 adios_core_tick_ms;

// dedicated to this super-loop's own timebase - nothing else in adios/
// common or the family drivers touches SysTick (ADIOS_DELAY uses a TIM
// peripheral, ADIOS_STOPWATCH another, ADIOS_TIMER its own table), so
// this can't collide with anything the application or ADIOS itself does.
void SysTick_Handler(void)
{
  ++adios_core_tick_ms;
}

extern void _abort(void); // defined further below in this file

#if ADIOS_CORE_USE_CANARI
// Stack-overflow canary - the bare-metal replacement for FreeRTOS's
// configCHECK_FOR_STACK_OVERFLOW (see ADIOS_CORE_USE_CANARI in
// adios_sys.h). Only ONE canary needed: only one stack left once tasks
// are gone, unlike FreeRTOS's per-task watermarking.
#define ADIOS_CORE_CANARY_PATTERN 0xa5a5a5a5

extern u32 _estack;      // top of stack (linker symbol, see the .ld file)
extern u32 __Stack_Init; // bottom of the reserved stack region (ditto)

static void ADIOS_CORE_Canary_Init(void)
{
  // fill everything below the CURRENT stack pointer with the pattern -
  // deliberately stops there rather than filling the whole region blindly,
  // so this can't clobber a stack frame already in active use at the point
  // this runs (main() itself, and whatever it already called into).
  u32 *p = &__Stack_Init;
  u32 *sp = (u32 *)__get_MSP();
  while( p < sp )
    *p++ = ADIOS_CORE_CANARY_PATTERN;
}

static void ADIOS_CORE_Canary_Check(void)
{
  if( __Stack_Init != ADIOS_CORE_CANARY_PATTERN ) {
    ADIOS_MIDI_SendDebugMessage("======================\n");
    ADIOS_MIDI_SendDebugMessage("!!! STACK OVERFLOW !!!\n");
    ADIOS_MIDI_SendDebugMessage("(bare-metal canary, ADIOS_CORE_USE_CANARI)\n");
    ADIOS_MIDI_SendDebugMessage("======================\n");
    _abort();
  }
}
#endif

static void ADIOS_CORE_BareLoop_Run(void)
{
  // 1mS tick, matching FreeRTOS's own configTICK_RATE_HZ=1000 in the other
  // mode - ADIOS_SYS_CPU_FREQUENCY (not the runtime SystemCoreClock
  // variable) to stay consistent with how FreeRTOSConfig.h's own
  // configCPU_CLOCK_HZ is derived.
  SysTick_Config(ADIOS_SYS_CPU_FREQUENCY / 1000);

#if ADIOS_CORE_USE_CANARI
  ADIOS_CORE_Canary_Init();
#endif

  u32 last_tick = adios_core_tick_ms;
  while( 1 ) {
    u32 now = adios_core_tick_ms;
    if( now != last_tick ) {
      // catches up to "now" in one shot rather than replaying every missed
      // mS if a previous iteration ran long - same intent as the
      // "skip delay gap" logic in TASK_Hooks/TASK_MIDI_Hooks above.
      last_tick = now;

#ifdef ADIOS_USE_TIMESTAMP
      ADIOS_TIMESTAMP_Inc();
#endif
#if defined(ADIOS_USE_SRIO) && !defined(ADIOS_DONT_SERVICE_SRIO_SCAN)
      APP_SRIO_ServicePrepare();
      ADIOS_SRIO_ScanStart(SRIO_ServiceFinish);
#endif

      ADIOS_CORE_NonMIDI_Tick();
      ADIOS_CORE_MIDI_Tick();

#if ADIOS_CORE_USE_CANARI
      ADIOS_CORE_Canary_Check();
#endif
    }

    // replaces vApplicationIdleHook() - called as often as possible between
    // 1mS blocks, same as FreeRTOS calls it whenever nothing else is ready
    // to run.
    APP_Background();
  }
}
#endif


/////////////////////////////////////////////////////////////////////////////
// This function aborts any operations, but keeps MIDI alive (for uploading
// a new firmware)
// If MIDI isn't enabled, the status LED will be flashed
/////////////////////////////////////////////////////////////////////////////
void _abort(void)
{
  // keep MIDI alive, so that program code can be updated
  u32 delay_ctr = 0;
  while( 1 ) {
    ++delay_ctr;

    if( (delay_ctr % 100) == 0 ) {
      // handle timeout/expire counters and USB packages
      ADIOS_MIDI_Periodic_mS();
    }

    // check for incoming MIDI packages and call hook
    ADIOS_MIDI_Receive_Handler(APP_MIDI_NotifyPackage);

    if( (delay_ctr % 10000) == 0 ) {
#ifdef ADIOS_USE_SOL
      // heartbeat. The board module offered a read-back to invert the LED;
      // the sign-of-life pin toggles itself, so nothing has to be read.
      ADIOS_SOL_Tog();
#endif
    }
  }
}


/////////////////////////////////////////////////////////////////////////////
// enabled in FreeRTOSConfig.h
/////////////////////////////////////////////////////////////////////////////
void vApplicationMallocFailedHook(void)
{

  // Note: message won't be sent if MIDI task cannot be created!
  ADIOS_MIDI_SendDebugMessage("FATAL: FreeRTOS Malloc Error!!!\n");

  _abort();
}


/////////////////////////////////////////////////////////////////////////////
// Required if static allocation is enabled in FreeRTOSConfig.h
/////////////////////////////////////////////////////////////////////////////
#if configSUPPORT_STATIC_ALLOCATION && configUSE_IDLE_HOOK

// default:
#ifndef ADIOS_APP_BACKGROUND_STACK_SIZE
#define ADIOS_APP_BACKGROUND_SIZE ((ADIOS_MINIMAL_STACK_SIZE)/4)
#warning "ADIOS_APP_BACKGROUND_SIZE hasn't been defined in adios_config.h --- using default ADIOS_MINIMAL_STACK_SIZE"
#endif

static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[ADIOS_APP_BACKGROUND_SIZE];
 
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = ADIOS_APP_BACKGROUND_SIZE;
}
#endif

#if configSUPPORT_STATIC_ALLOCATION && configUSE_TIMERS

// default:
#ifndef ADIOS_FREERTOS_TIMER_TASK_STACK_SIZE
#define ADIOS_FREERTOS_TIMER_TASK_STACK_SIZE ((ADIOS_MINIMAL_STACK_SIZE)/4)
#warning "ADIOS_FREERTOS_TIMER_TASK_STACK_SIZE hasn't been defined in adios_config.h --- using default ADIOS_MINIMAL_STACK_SIZE"
#endif

static StaticTask_t xTimerTaskTCBBuffer;
static StackType_t xTimerStack[ADIOS_FREERTOS_TIMER_TASK_STACK_SIZE];
 
/* If static allocation is supported then the application must provide the
   following callback function - which enables the application to optionally
   provide the memory that will be used by the timer task as the task's stack
   and TCB. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCBBuffer;
  *ppxTimerTaskStackBuffer = &xTimerStack[0];
  *pulTimerTaskStackSize = ADIOS_FREERTOS_TIMER_TASK_STACK_SIZE;
}
#endif


/////////////////////////////////////////////////////////////////////////////
// _exit() for newer newlib versions
/////////////////////////////////////////////////////////////////////////////
void exit(int par)
{

  // Note: message won't be sent if MIDI task cannot be created!
  ADIOS_MIDI_SendDebugMessage("Goodbye!\n");

  // pro forma: since this is a noreturn function, loop endless and call _abort (which will never exit)
  while( 1 )
    _abort();
}

// see http://www.linuxquestions.org/questions/programming-9/fyi-shared-libs-and-iostream-c-331113/
void *__dso_handle = NULL;


/////////////////////////////////////////////////////////////////////////////
// Customized HardFault Handler which prints out debugging informations
/////////////////////////////////////////////////////////////////////////////
void HardFault_Handler_c(unsigned int * hardfault_args)
{

  // from the book: "The definiteve guide to the ARM Cortex-M3"
  volatile unsigned int stacked_r0;
  volatile unsigned int stacked_r1;
  volatile unsigned int stacked_r2;
  volatile unsigned int stacked_r3;
  volatile unsigned int stacked_r12;
  volatile unsigned int stacked_lr;
  volatile unsigned int stacked_pc;
  volatile unsigned int stacked_psr;

  stacked_r0 = ((unsigned long) hardfault_args[0]);
  stacked_r1 = ((unsigned long) hardfault_args[1]);
  stacked_r2 = ((unsigned long) hardfault_args[2]);
  stacked_r3 = ((unsigned long) hardfault_args[3]);

  stacked_r12 = ((unsigned long) hardfault_args[4]);
  stacked_lr = ((unsigned long) hardfault_args[5]);
  stacked_pc = ((unsigned long) hardfault_args[6]);
  stacked_psr = ((unsigned long) hardfault_args[7]);
  ADIOS_MIDI_SendDebugMessage("Hard Fault PC = %08x\n", stacked_pc); // ensure that at least the PC will be sent
  ADIOS_MIDI_SendDebugMessage("==================\n");
  ADIOS_MIDI_SendDebugMessage("!!! HARD FAULT !!!\n");
  ADIOS_MIDI_SendDebugMessage("==================\n");
  ADIOS_MIDI_SendDebugMessage("R0 = %08x\n", stacked_r0);
  ADIOS_MIDI_SendDebugMessage("R1 = %08x\n", stacked_r1);
  ADIOS_MIDI_SendDebugMessage("R2 = %08x\n", stacked_r2);
  ADIOS_MIDI_SendDebugMessage("R3 = %08x\n", stacked_r3);
  ADIOS_MIDI_SendDebugMessage("R12 = %08x\n", stacked_r12);
  ADIOS_MIDI_SendDebugMessage("LR = %08x\n", stacked_lr);
  ADIOS_MIDI_SendDebugMessage("PC = %08x\n", stacked_pc);
  ADIOS_MIDI_SendDebugMessage("PSR = %08x\n", stacked_psr);
  ADIOS_MIDI_SendDebugMessage("BFAR = %08x\n", (*((volatile unsigned long *)(0xE000ED38))));
  ADIOS_MIDI_SendDebugMessage("CFSR = %08x\n", (*((volatile unsigned long *)(0xE000ED28))));
  ADIOS_MIDI_SendDebugMessage("HFSR = %08x\n", (*((volatile unsigned long *)(0xE000ED2C))));
  ADIOS_MIDI_SendDebugMessage("DFSR = %08x\n", (*((volatile unsigned long *)(0xE000ED30))));
  ADIOS_MIDI_SendDebugMessage("AFSR = %08x\n", (*((volatile unsigned long *)(0xE000ED3C))));

  _abort();
}


void HardFault_Handler(void)
{
//  __asm("TST LR, #4");
//  __asm("ITE EQ");
//  __asm("MRSEQ R0, MSP");
//  __asm("MRSNE R0, PSP");
//  __asm("B HardFault_Handler_c");
}

// used if configCHECK_FOR_STACK_OVERFLOW enabled (set to 1 or 2) in FreeRTOSConfig.h
#if configCHECK_FOR_STACK_OVERFLOW
// Which task overflowed, readable over SWD after the crash. The MIDI debug
// message below rarely gets out: the overflow stops the very machinery that
// would send it. This buffer survives where the message does not.
char adios_stack_overflow_task[16];

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  u8 i;
  for(i=0; i<15 && pcTaskName[i]; ++i)
    adios_stack_overflow_task[i] = pcTaskName[i];
  adios_stack_overflow_task[i] = 0;

  ADIOS_MIDI_SendDebugMessage("======================\n");
  ADIOS_MIDI_SendDebugMessage("!!! STACK OVERFLOW !!!\n");
  ADIOS_MIDI_SendDebugMessage("======================\n");
  ADIOS_MIDI_SendDebugMessage("Function: %s\n", pcTaskName);

  _abort();
}
#endif

