// $Id: main.c 42 2008-09-30 21:49:43Z tk $
/*
 * Traditional Programming Model
 * Provides similar hooks like PIC based MIOS8 to the application
 *
 * ==========================================================================
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
#include <app.h>
#ifndef MIOS32_DONT_USE_LCD
#include <app_lcd.h>
#endif

// ===========================================================================
// MIOS32_CORE_USE_FREERTOS (see mios32_sys.h for the auto-derived default,
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
//     (see MIOS32_APP_USE_FREERTOS for the kernel-presence switch,
//     independent of this one) - timed by a dedicated SysTick handler
//     instead of the FreeRTOS tick (MIOS32_STOPWATCH is deliberately NOT
//     used for this: it must stay free for the application's own timing
//     needs). The two Hooks collapse into ONE sequential 1mS block - THIS
//     REMOVES THE ISOLATION DESCRIBED ABOVE: a slow/blocking application
//     hook now delays MIDI processing too, since everything runs on the
//     same single thread of execution with nothing left to preempt it.
//     Worth it on the smallest chips only because of what FreeRTOS itself
//     costs there - measured empirically at ~83% of a full G030K6 build,
//     and roughly half the total RAM on G031K8 (heap alone, before any
//     application code) - see apps/templates/app_skeleton/mios32_config.h
//     for the full writeup.
//
// Both switches are numeric (0/1), not plain #define presence - checked
// with "#if", not "#ifdef" - specifically so a project CAN override either
// one to 0 even where the RAM/FLASH tier would otherwise default it to 1
// (a bare #undef can't express "explicitly off" vs. "undecided, apply the
// tier default"). Override in your own mios32_config.h, e.g.
// "#define MIOS32_CORE_USE_FREERTOS 0".
// ===========================================================================
#if MIOS32_CORE_USE_FREERTOS
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#endif


/////////////////////////////////////////////////////////////////////////////
// External Prototypes
/////////////////////////////////////////////////////////////////////////////

extern void __libc_init_array(void);  /* calls CTORS of static objects */


/////////////////////////////////////////////////////////////////////////////
// Task Priorities and stack sizes (MIOS32_CORE_USE_FREERTOS only - a bare
// super-loop has no separate task stacks, everything runs on the single
// main() stack sized by the linker script)
/////////////////////////////////////////////////////////////////////////////

#if MIOS32_CORE_USE_FREERTOS
#define PRIORITY_TASK_HOOKS		( tskIDLE_PRIORITY + 3 )

#ifndef MIOS32_TASK_HOOKS_STACK_SIZE
#define MIOS32_TASK_HOOKS_STACK_SIZE (configMINIMAL_STACK_SIZE*4)
#endif
#ifndef MIOS32_TASK_MIDI_HOOKS_STACK_SIZE
#define MIOS32_TASK_MIDI_HOOKS_STACK_SIZE (configMINIMAL_STACK_SIZE*4)
#endif
#endif


/////////////////////////////////////////////////////////////////////////////
// Local prototypes
/////////////////////////////////////////////////////////////////////////////
#if MIOS32_CORE_USE_FREERTOS
static void TASK_Hooks(void *pvParameters);
static void TASK_MIDI_Hooks(void *pvParameters);
#endif
static void MIOS32_CORE_NonMIDI_Tick(void);
static void MIOS32_CORE_MIDI_Tick(void);
#if !MIOS32_CORE_USE_FREERTOS
static void MIOS32_CORE_BareLoop_Run(void);
#endif


/////////////////////////////////////////////////////////////////////////////
// FreeRTOS Heap
/////////////////////////////////////////////////////////////////////////////

#if configAPPLICATION_ALLOCATED_HEAP
# if defined(MIOS32_FAMILY_LPC17xx) && !defined(MIOS32_FREERTOS_HEAP_SECTION)
#  define MIOS32_FREERTOS_HEAP_SECTION __attribute__ ((section (".bss_ahb")))
# else
#  define MIOS32_FREERTOS_HEAP_SECTION
# endif

uint8_t MIOS32_FREERTOS_HEAP_SECTION ucHeap[configTOTAL_HEAP_SIZE];
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
  // initialize hardware and MIOS32 modules
#ifndef MIOS32_DONT_USE_SYS
  MIOS32_SYS_Init(0);
#endif
#ifndef MIOS32_DONT_USE_DELAY
  MIOS32_DELAY_Init(0);
#endif
#ifndef MIOS32_DONT_USE_TIMESTAMP
  MIOS32_TIMESTAMP_Init(0);
#endif
#ifdef MIOS32_USE_SOL
  // was MIOS32_BOARD_Init(): the board module carried the MBHP boards' frozen
  // connector definitions (J5/J10/J15/J28/LED/DAC) and is gone. All that
  // survived of it here is the status LED, which is the sign-of-life pin.
  MIOS32_SOL_Init();
#endif
#ifdef MIOS32_USE_SPI
  MIOS32_SPI_Init(0);
#endif
#ifdef MIOS32_USE_I2C
  // auto-derived in mios32_i2c.h from any MIOS32_USE_I2C0/1/2 or
  // MIOS32_USE_FMPI2C0 the project declared
  MIOS32_I2C_Init(0);
#endif
#ifndef MIOS32_DONT_USE_SRIO
  MIOS32_SRIO_Init(0);
#endif
#if !defined(MIOS32_DONT_USE_DIN) && !defined(MIOS32_DONT_USE_SRIO)
  MIOS32_DIN_Init(0);
#endif
#if !defined(MIOS32_DONT_USE_DOUT) && !defined(MIOS32_DONT_USE_SRIO)
  MIOS32_DOUT_Init(0);
#endif
#if !defined(MIOS32_DONT_USE_ENC) && !defined(MIOS32_DONT_USE_SRIO)
  MIOS32_ENC_Init(0);
#endif
#if !defined(MIOS32_DONT_USE_MF)
  MIOS32_MF_Init(0);
#endif
#if !defined(MIOS32_DONT_USE_AIN)
  MIOS32_AIN_Init(0);
#endif
  // the MIDI core is always initialized - it is not optional (2026-08-09,
  // see mios32_midi.c): only the transports underneath are opt-in
  MIOS32_MIDI_Init(0);
#ifndef MIOS32_DONT_USE_USB
  MIOS32_USB_Init(0);
#endif
#ifndef MIOS32_DONT_USE_OSC
  MIOS32_OSC_Init(0);
#endif
#ifndef MIOS32_DONT_USE_COM
  MIOS32_COM_Init(0);
#endif
#ifndef MIOS32_DONT_USE_LCD
  MIOS32_LCD_Init(0);

# if defined(MIOS32_BOARD_MBHP_CORE_STM32) || defined(MIOS32_BOARD_MBHP_CORE_LPC17) || defined(MIOS32_BOARD_STM32F4DISCOVERY) || defined(MIOS32_BOARD_MBHP_CORE_STM32F4)
  // init second LCD as well (if available)
  MIOS32_LCD_DeviceSet(1);
  APP_LCD_Init(0);
  MIOS32_LCD_DeviceSet(0);
# endif
#endif
#ifdef MIOS32_USE_I2S
  MIOS32_I2S_Init(0);
#endif

  // call C++ constructors
  __libc_init_array();

  // initialize application
  APP_Init();

#if MIOS32_LCD_BOOT_MSG_DELAY
  // print boot message
# ifndef MIOS32_DONT_USE_LCD
  MIOS32_LCD_PrintBootMessage();
# endif

  // wait for given delay (usually 2 seconds)
# ifndef MIOS32_DONT_USE_DELAY
  int delay = 0;
  for(delay=0; delay<MIOS32_LCD_BOOT_MSG_DELAY; ++delay)
    MIOS32_DELAY_Wait_uS(1000);
# endif
#endif

#if MIOS32_CORE_USE_FREERTOS
  // start the task which calls the application hooks
  xTaskCreate(TASK_Hooks, "Hooks", (MIOS32_TASK_HOOKS_STACK_SIZE)/4, NULL, PRIORITY_TASK_HOOKS, NULL);
  xTaskCreate(TASK_MIDI_Hooks, "MIDI_Hooks", (MIOS32_TASK_MIDI_HOOKS_STACK_SIZE)/4, NULL, PRIORITY_TASK_HOOKS, NULL);
  //MIOS32_BOARD_LED_Set(1, 1);
  // start the scheduler
  vTaskStartScheduler();

  // Will only get here if there was not enough heap space to create the idle task
  return 0;
#else
  // bare-metal super-loop - see the MIOS32_CORE_USE_FREERTOS module-level
  // comment above for what this trades away.
  MIOS32_CORE_BareLoop_Run(); // never returns
  return 0; // never reached
#endif
}


/////////////////////////////////////////////////////////////////////////////
// Application Tick Hook (called by FreeRTOS each mS)
/////////////////////////////////////////////////////////////////////////////
void SRIO_ServiceFinish(void)
{
#ifndef MIOS32_DONT_USE_SRIO

# ifndef MIOS32_DONT_USE_ENC
  // update encoder states
  MIOS32_ENC_UpdateStates();
# endif

  // notify application about finished SRIO scan
  APP_SRIO_ServiceFinish();
#endif
}

#if MIOS32_CORE_USE_FREERTOS
void vApplicationTickHook(void)
{
#if !defined(MIOS32_DONT_USE_TIMESTAMP)
  MIOS32_TIMESTAMP_Inc();
#endif

#if !defined(MIOS32_DONT_USE_SRIO) && !defined(MIOS32_DONT_SERVICE_SRIO_SCAN)
  // notify application about SRIO scan start
  APP_SRIO_ServicePrepare();

  // start next SRIO scan - IRQ notification to SRIO_ServiceFinish()
  MIOS32_SRIO_ScanStart(SRIO_ServiceFinish);
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
// bare-metal super-loop further down (MIOS32_CORE_USE_FREERTOS=0) -
// deliberately factored out so the two scheduling modes can never drift
// apart from each other.
/////////////////////////////////////////////////////////////////////////////
static void MIOS32_CORE_MIDI_Tick(void)
{
  // handle timeout/expire counters and USB packages
  MIOS32_MIDI_Periodic_mS();

  // check for incoming MIDI packages and call hook
  MIOS32_MIDI_Receive_Handler(APP_MIDI_NotifyPackage);

  // optional application specific hook
  // helps to save memory (re-use the TASK_Hooks for other purposes...)
  APP_MIDI_Tick();
}

static void MIOS32_CORE_NonMIDI_Tick(void)
{
#if !defined(MIOS32_DONT_USE_USB_HOST) || !defined(MIOS32_DONT_USE_USB_HS_HOST)
  // process USB Host, only others than USB MIDI
#ifndef MIOS32_DONT_PROCESS_USB_HOST
  MIOS32_USB_HOST_Process();
#endif
#endif

#if !defined(MIOS32_DONT_USE_DIN) && !defined(MIOS32_DONT_USE_SRIO)
  // check for DIN pin changes, call APP_DIN_NotifyToggle on each toggled pin
  MIOS32_DIN_Handler(APP_DIN_NotifyToggle);

  // check for encoder changes, call APP_ENC_NotifyChanged on each change
# ifndef MIOS32_DONT_USE_ENC
  MIOS32_ENC_Handler(APP_ENC_NotifyChange);
# endif
#endif

#if !defined(MIOS32_DONT_USE_AIN) && !defined(MIOS32_DONT_SERVICE_AIN)
  // check for AIN pin changes, call APP_AIN_NotifyChange on each pin change
  MIOS32_AIN_Handler(APP_AIN_NotifyChange);
#endif

#if !defined(MIOS32_DONT_USE_COM)
  // check for incoming COM messages
  MIOS32_COM_Receive_Handler();
#endif

  // optional APP_Tick() hook
  // helps to save memory (re-use the TASK_Hooks for other purposes...)
  APP_Tick();
}


#if MIOS32_CORE_USE_FREERTOS
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

    MIOS32_CORE_MIDI_Tick();
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

    MIOS32_CORE_NonMIDI_Tick();
  }
}
#endif


#if !MIOS32_CORE_USE_FREERTOS
/////////////////////////////////////////////////////////////////////////////
// Bare-metal super-loop (MIOS32_CORE_USE_FREERTOS=0) - replaces
// vTaskStartScheduler()/TASK_Hooks/TASK_MIDI_Hooks/vApplicationTickHook/
// vApplicationIdleHook above. Timed by a dedicated SysTick handler rather
// than FreeRTOS's tick or MIOS32_STOPWATCH (kept free for the application -
// see the module-level comment near the top of this file).
/////////////////////////////////////////////////////////////////////////////

static volatile u32 mios32_core_tick_ms;

// dedicated to this super-loop's own timebase - nothing else in mios32/
// common or the family drivers touches SysTick (MIOS32_DELAY uses a TIM
// peripheral, MIOS32_STOPWATCH another, MIOS32_TIMER its own table), so
// this can't collide with anything the application or MIOS32 itself does.
void SysTick_Handler(void)
{
  ++mios32_core_tick_ms;
}

extern void _abort(void); // defined further below in this file

#if MIOS32_CORE_USE_CANARI
// Stack-overflow canary - the bare-metal replacement for FreeRTOS's
// configCHECK_FOR_STACK_OVERFLOW (see MIOS32_CORE_USE_CANARI in
// mios32_sys.h). Only ONE canary needed: only one stack left once tasks
// are gone, unlike FreeRTOS's per-task watermarking.
#define MIOS32_CORE_CANARY_PATTERN 0xa5a5a5a5

extern u32 _estack;      // top of stack (linker symbol, see the .ld file)
extern u32 __Stack_Init; // bottom of the reserved stack region (ditto)

static void MIOS32_CORE_Canary_Init(void)
{
  // fill everything below the CURRENT stack pointer with the pattern -
  // deliberately stops there rather than filling the whole region blindly,
  // so this can't clobber a stack frame already in active use at the point
  // this runs (main() itself, and whatever it already called into).
  u32 *p = &__Stack_Init;
  u32 *sp = (u32 *)__get_MSP();
  while( p < sp )
    *p++ = MIOS32_CORE_CANARY_PATTERN;
}

static void MIOS32_CORE_Canary_Check(void)
{
  if( __Stack_Init != MIOS32_CORE_CANARY_PATTERN ) {
    MIOS32_MIDI_SendDebugMessage("======================\n");
    MIOS32_MIDI_SendDebugMessage("!!! STACK OVERFLOW !!!\n");
    MIOS32_MIDI_SendDebugMessage("(bare-metal canary, MIOS32_CORE_USE_CANARI)\n");
    MIOS32_MIDI_SendDebugMessage("======================\n");
    _abort();
  }
}
#endif

static void MIOS32_CORE_BareLoop_Run(void)
{
  // 1mS tick, matching FreeRTOS's own configTICK_RATE_HZ=1000 in the other
  // mode - MIOS32_SYS_CPU_FREQUENCY (not the runtime SystemCoreClock
  // variable) to stay consistent with how FreeRTOSConfig.h's own
  // configCPU_CLOCK_HZ is derived.
  SysTick_Config(MIOS32_SYS_CPU_FREQUENCY / 1000);

#if MIOS32_CORE_USE_CANARI
  MIOS32_CORE_Canary_Init();
#endif

  u32 last_tick = mios32_core_tick_ms;
  while( 1 ) {
    u32 now = mios32_core_tick_ms;
    if( now != last_tick ) {
      // catches up to "now" in one shot rather than replaying every missed
      // mS if a previous iteration ran long - same intent as the
      // "skip delay gap" logic in TASK_Hooks/TASK_MIDI_Hooks above.
      last_tick = now;

#if !defined(MIOS32_DONT_USE_TIMESTAMP)
      MIOS32_TIMESTAMP_Inc();
#endif
#if !defined(MIOS32_DONT_USE_SRIO) && !defined(MIOS32_DONT_SERVICE_SRIO_SCAN)
      APP_SRIO_ServicePrepare();
      MIOS32_SRIO_ScanStart(SRIO_ServiceFinish);
#endif

      MIOS32_CORE_NonMIDI_Tick();
      MIOS32_CORE_MIDI_Tick();

#if MIOS32_CORE_USE_CANARI
      MIOS32_CORE_Canary_Check();
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
      MIOS32_MIDI_Periodic_mS();
    }

    // check for incoming MIDI packages and call hook
    MIOS32_MIDI_Receive_Handler(APP_MIDI_NotifyPackage);

    if( (delay_ctr % 10000) == 0 ) {
#ifdef MIOS32_USE_SOL
      // heartbeat. The board module offered a read-back to invert the LED;
      // the sign-of-life pin toggles itself, so nothing has to be read.
      MIOS32_SOL_Tog();
#endif
    }
  }
}


/////////////////////////////////////////////////////////////////////////////
// enabled in FreeRTOSConfig.h
/////////////////////////////////////////////////////////////////////////////
void vApplicationMallocFailedHook(void)
{
#ifndef MIOS32_DONT_USE_LCD
  // TODO: here we should select the normal font - but only if available!
  // MIOS32_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  MIOS32_LCD_BColourSet(0xffffff);
  MIOS32_LCD_FColourSet(0x000000);

  MIOS32_LCD_DeviceSet(0);
  MIOS32_LCD_Clear();
  MIOS32_LCD_CursorSet(0, 0);
  MIOS32_LCD_PrintString("FATAL: FreeRTOS "); // 16 chars
  MIOS32_LCD_CursorSet(0, 1);
  MIOS32_LCD_PrintString("Malloc Error!!! "); // 16 chars
#endif

  // Note: message won't be sent if MIDI task cannot be created!
  MIOS32_MIDI_SendDebugMessage("FATAL: FreeRTOS Malloc Error!!!\n");

  _abort();
}


/////////////////////////////////////////////////////////////////////////////
// Required if static allocation is enabled in FreeRTOSConfig.h
/////////////////////////////////////////////////////////////////////////////
#if configSUPPORT_STATIC_ALLOCATION && configUSE_IDLE_HOOK

// default:
#ifndef MIOS32_APP_BACKGROUND_STACK_SIZE
#define MIOS32_APP_BACKGROUND_SIZE ((MIOS32_MINIMAL_STACK_SIZE)/4)
#warning "MIOS32_APP_BACKGROUND_SIZE hasn't been defined in mios32_config.h --- using default MIOS32_MINIMAL_STACK_SIZE"
#endif

static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[MIOS32_APP_BACKGROUND_SIZE];
 
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = MIOS32_APP_BACKGROUND_SIZE;
}
#endif

#if configSUPPORT_STATIC_ALLOCATION && configUSE_TIMERS

// default:
#ifndef MIOS32_FREERTOS_TIMER_TASK_STACK_SIZE
#define MIOS32_FREERTOS_TIMER_TASK_STACK_SIZE ((MIOS32_MINIMAL_STACK_SIZE)/4)
#warning "MIOS32_FREERTOS_TIMER_TASK_STACK_SIZE hasn't been defined in mios32_config.h --- using default MIOS32_MINIMAL_STACK_SIZE"
#endif

static StaticTask_t xTimerTaskTCBBuffer;
static StackType_t xTimerStack[MIOS32_FREERTOS_TIMER_TASK_STACK_SIZE];
 
/* If static allocation is supported then the application must provide the
   following callback function - which enables the application to optionally
   provide the memory that will be used by the timer task as the task's stack
   and TCB. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCBBuffer;
  *ppxTimerTaskStackBuffer = &xTimerStack[0];
  *pulTimerTaskStackSize = MIOS32_FREERTOS_TIMER_TASK_STACK_SIZE;
}
#endif


/////////////////////////////////////////////////////////////////////////////
// _exit() for newer newlib versions
/////////////////////////////////////////////////////////////////////////////
void exit(int par)
{
#ifndef MIOS32_DONT_USE_LCD
  // TODO: here we should select the normal font - but only if available!
  // MIOS32_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  MIOS32_LCD_BColourSet(0xffffff);
  MIOS32_LCD_FColourSet(0x000000);

  MIOS32_LCD_DeviceSet(0);
  MIOS32_LCD_Clear();
  MIOS32_LCD_CursorSet(0, 0);
  MIOS32_LCD_PrintString("Goodbye!");
#endif

  // Note: message won't be sent if MIDI task cannot be created!
  MIOS32_MIDI_SendDebugMessage("Goodbye!\n");

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
  MIOS32_MIDI_SendDebugMessage("Hard Fault PC = %08x\n", stacked_pc); // ensure that at least the PC will be sent
  MIOS32_MIDI_SendDebugMessage("==================\n");
  MIOS32_MIDI_SendDebugMessage("!!! HARD FAULT !!!\n");
  MIOS32_MIDI_SendDebugMessage("==================\n");
  MIOS32_MIDI_SendDebugMessage("R0 = %08x\n", stacked_r0);
  MIOS32_MIDI_SendDebugMessage("R1 = %08x\n", stacked_r1);
  MIOS32_MIDI_SendDebugMessage("R2 = %08x\n", stacked_r2);
  MIOS32_MIDI_SendDebugMessage("R3 = %08x\n", stacked_r3);
  MIOS32_MIDI_SendDebugMessage("R12 = %08x\n", stacked_r12);
  MIOS32_MIDI_SendDebugMessage("LR = %08x\n", stacked_lr);
  MIOS32_MIDI_SendDebugMessage("PC = %08x\n", stacked_pc);
  MIOS32_MIDI_SendDebugMessage("PSR = %08x\n", stacked_psr);
  MIOS32_MIDI_SendDebugMessage("BFAR = %08x\n", (*((volatile unsigned long *)(0xE000ED38))));
  MIOS32_MIDI_SendDebugMessage("CFSR = %08x\n", (*((volatile unsigned long *)(0xE000ED28))));
  MIOS32_MIDI_SendDebugMessage("HFSR = %08x\n", (*((volatile unsigned long *)(0xE000ED2C))));
  MIOS32_MIDI_SendDebugMessage("DFSR = %08x\n", (*((volatile unsigned long *)(0xE000ED30))));
  MIOS32_MIDI_SendDebugMessage("AFSR = %08x\n", (*((volatile unsigned long *)(0xE000ED3C))));
#ifndef MIOS32_DONT_USE_LCD
  // TODO: here we should select the normal font - but only if available!
  // MIOS32_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  MIOS32_LCD_BColourSet(0xffffff);
  MIOS32_LCD_FColourSet(0x000000);

  MIOS32_LCD_DeviceSet(0);
  MIOS32_LCD_Clear();
  MIOS32_LCD_CursorSet(0, 0);
  MIOS32_LCD_PrintString("!! HARD FAULT !!");
  MIOS32_LCD_CursorSet(0, 1);
  MIOS32_LCD_PrintFormattedString("at PC=0x%08x", stacked_pc);
#endif

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
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{

  MIOS32_MIDI_SendDebugMessage("======================\n");
  MIOS32_MIDI_SendDebugMessage("!!! STACK OVERFLOW !!!\n");
  MIOS32_MIDI_SendDebugMessage("======================\n");
  MIOS32_MIDI_SendDebugMessage("Function: %s\n", pcTaskName);
#ifndef MIOS32_DONT_USE_LCD
  // TODO: here we should select the normal font - but only if available!
  // MIOS32_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  MIOS32_LCD_BColourSet(0xffffff);
  MIOS32_LCD_FColourSet(0x000000);

  MIOS32_LCD_DeviceSet(0);
  MIOS32_LCD_Clear();
  MIOS32_LCD_CursorSet(0, 0);
  MIOS32_LCD_PrintString("!! STACK OVERFLOW !!");
  MIOS32_LCD_CursorSet(0, 1);
  MIOS32_LCD_PrintFormattedString("in Task %s", pcTaskName);
#endif

  _abort();
}
#endif

