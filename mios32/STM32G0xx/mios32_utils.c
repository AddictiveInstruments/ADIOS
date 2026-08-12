// mios32_utils.c
//! \defgroup MIOS32_UTILS
//!
//! Delay / Timer / Stopwatch / Sign-of-life (SOF) LED utility functions for
//! MIOS32 - each independently opt-in via its own MIOS32_USE_x define, see
//! below.
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


/////////////////////////////////////////////////////////////////////////////
// MIOS32_DELAY - busy-wait microsecond delays - indispensable, no on/off
// toggle: called unconditionally by the programming model's main.c and by
// MIOS32_SYS_Reset() itself.
/////////////////////////////////////////////////////////////////////////////

// timer used for MIOS32_DELAY functions (TIM1..TIM7)
#ifndef MIOS32_DELAY_TIMER
#define MIOS32_DELAY_TIMER  TIM14
#endif

#ifndef MIOS32_DELAY_TIMER_RCC
#define MIOS32_DELAY_TIMER_RCC LL_APB2_GRP1_PERIPH_TIM14
#endif

// timers clocked at CPU clock
#define DELAY_TIM_PERIPHERAL_FRQ MIOS32_SYS_CPU_FREQUENCY

/////////////////////////////////////////////////////////////////////////////
//! Initializes the Timer used by MIOS32_DELAY functions<BR>
//! This function has to be executed before wait functions are used
//! (already done in main.c of the programming model)
//!
//! Currently TIM14 is allocated by MIOS32_DELAY functions - if this clashes
//! with your application, just switch to another timer by overriding
//! MIOS32_DELAY_TIMER and MIOS32_DELAY_TIMER_RCC in your mios32_config.h file
//!
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DELAY_Init(u32 mode)
{
  // currently only mode 0 supported
  if( mode != 0 )
    return -1; // unsupported mode

  // enable timer clock
  LL_APB2_GRP1_EnableClock(MIOS32_DELAY_TIMER_RCC);

  // time base configuration
  LL_TIM_InitTypeDef  TIM_TimeBaseStructure;
  TIM_TimeBaseStructure.Autoreload = 65535; // maximum value
  TIM_TimeBaseStructure.Prescaler = (DELAY_TIM_PERIPHERAL_FRQ/1000000)-1; // for 1 uS accuracy
  TIM_TimeBaseStructure.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  TIM_TimeBaseStructure.CounterMode = LL_TIM_COUNTERMODE_UP;
  LL_TIM_Init(MIOS32_DELAY_TIMER, &TIM_TimeBaseStructure);

  // enable counter
  LL_TIM_EnableCounter(MIOS32_DELAY_TIMER);

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! Waits for a specific number of uS<BR>
//! Example:<BR>
//! \code
//!   // wait for 500 uS
//!   MIOS32_DELAY_Wait_uS(500);
//! \endcode
//! \param[in] uS delay (1..65535 microseconds)
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DELAY_Wait_uS(u16 uS)
{
  u16 start = MIOS32_DELAY_TIMER->CNT;

  // note that this even works on 16bit counter wrap-arounds
  while( (u16)(MIOS32_DELAY_TIMER->CNT - start) <= uS );

  return 0; // no error
}



/////////////////////////////////////////////////////////////////////////////
// MIOS32_TIMER - periodic interrupt-driven timers
/////////////////////////////////////////////////////////////////////////////
#if defined(MIOS32_USE_TIMER)

#define NUM_TIMERS 3

// TIM1/TIM3/TIM16 - confirmed present on every STM32G0 tier (verified via
// each tier's CMSIS device header, from the smallest 2-USART G030/G031 up
// to the 6-USART+2xLPUART G0B1/G0C1) - unlike the former defaults TIM2/
// TIM5, missing below the 4-USART tier, and doesn't collide with
// MIOS32_DELAY (TIM14) or MIOS32_STOPWATCH (TIM17). This whole module was
// previously left-over STM32 Standard Peripheral Library (SPL) code -
// RCC_APB1Periph_TIM2, TIM_ITConfig(), TIM_TimeBaseInit() etc - none of
// which exist in the STM32G0xx LL driver at all (nor does TIM8, referenced
// in the old RCC-bus check); it would fail to compile on every G0 chip the
// moment a project defined MIOS32_USE_TIMER, regardless of tier - nobody
// ever had, so it was never caught. Rewritten against LL below.
// Override all five (_BASE/_RCC/_RCC_ENABLE/_IRQ/_IRQ_HANDLER) together per
// slot if one of these conflicts with something else on your hardware.
#ifndef TIMER0_BASE
#define TIMER0_BASE                 TIM1
#endif
#ifndef TIMER0_RCC
#define TIMER0_RCC                  LL_APB2_GRP1_PERIPH_TIM1
#endif
#ifndef TIMER0_RCC_ENABLE
#define TIMER0_RCC_ENABLE()         LL_APB2_GRP1_EnableClock(TIMER0_RCC)
#endif
#ifndef TIMER0_IRQ
#define TIMER0_IRQ                  TIM1_BRK_UP_TRG_COM_IRQn
#endif
#ifndef TIMER0_IRQ_HANDLER
#define TIMER0_IRQ_HANDLER          void TIM1_BRK_UP_TRG_COM_IRQHandler(void)
#endif

#ifndef TIMER1_BASE
#define TIMER1_BASE                 TIM3
#endif
#ifndef TIMER1_RCC
#define TIMER1_RCC                  LL_APB1_GRP1_PERIPH_TIM3
#endif
#ifndef TIMER1_RCC_ENABLE
#define TIMER1_RCC_ENABLE()         LL_APB1_GRP1_EnableClock(TIMER1_RCC)
#endif
#ifndef TIMER1_IRQ
#define TIMER1_IRQ                  TIM3_IRQn
#endif
#ifndef TIMER1_IRQ_HANDLER
#define TIMER1_IRQ_HANDLER          void TIM3_IRQHandler(void)
#endif

#ifndef TIMER2_BASE
#define TIMER2_BASE                 TIM16
#endif
#ifndef TIMER2_RCC
#define TIMER2_RCC                  LL_APB2_GRP1_PERIPH_TIM16
#endif
#ifndef TIMER2_RCC_ENABLE
#define TIMER2_RCC_ENABLE()         LL_APB2_GRP1_EnableClock(TIMER2_RCC)
#endif
#ifndef TIMER2_IRQ
#define TIMER2_IRQ                  TIM16_IRQn
#endif
#ifndef TIMER2_IRQ_HANDLER
#define TIMER2_IRQ_HANDLER          void TIM16_IRQHandler(void)
#endif

// timers clocked at CPU clock
#define TIMER_TIM_PERIPHERAL_FRQ (MIOS32_SYS_CPU_FREQUENCY)

static TIM_TypeDef * const timer_base[NUM_TIMERS] = { TIMER0_BASE, TIMER1_BASE, TIMER2_BASE };
static const u32 timer_irq_chn[NUM_TIMERS] = { TIMER0_IRQ, TIMER1_IRQ, TIMER2_IRQ };
static void (*timer_callback[NUM_TIMERS])(void);

/////////////////////////////////////////////////////////////////////////////
//! Initialize a timer
//! \param[in] timer (0..2)<BR>
//!     Timer allocation: 0=TIM1, 1=TIM3, 2=TIM16 (see TIMERn_BASE overrides above)
//! \param[in] period in uS accuracy (1..65536)
//! \param[in] _irq_handler (function name)
//! \param[in] irq_priority: one of these values:
//! <UL>
//!   <LI>MIOS32_IRQ_PRIO_LOW      // lower than RTOS
//!   <LI>MIOS32_IRQ_PRIO_MID      // higher than RTOS
//!   <LI>MIOS32_IRQ_PRIO_HIGH     // same like SRIO, AIN, etc...
//!   <LI>MIOS32_IRQ_PRIO_HIGHEST  // higher than SRIO, AIN, etc...
//! </UL>
//!
//! Example:<BR>
//! \code
//!   // initialize timer for 1000 uS (= 1 mS) period
//!   MIOS32_TIMER_Init(0, 1000, MyTimer, MIOS32_IRQ_PRIO_MID);
//! \endcode
//! this will call following function periodically:
//! \code
//! void MyTimer(void)
//! {
//!    // your code
//! }
//! \endcode
//! \return 0 if initialisation passed
//! \return -1 if invalid timer number
//! \return -2 if invalid period
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_TIMER_Init(u8 timer, u32 period, void (*_irq_handler)(void), u8 irq_priority)
{
  // check if valid timer
  if( timer >= NUM_TIMERS )
    return -1; // invalid timer selected

  // check if valid period
  if( period < 1 || period >= 65537 )
    return -2;

  // enable timer clock (per-slot bus differs: TIM1/TIM16 on APB2, TIM3 on APB1)
  switch( timer ) {
  case 0: TIMER0_RCC_ENABLE(); break;
  case 1: TIMER1_RCC_ENABLE(); break;
  case 2: TIMER2_RCC_ENABLE(); break;
  }

  // disable interrupt (if active from previous configuration)
  LL_TIM_DisableIT_UPDATE(timer_base[timer]);

  // copy callback function
  timer_callback[timer] = _irq_handler;

  // time base configuration
  LL_TIM_InitTypeDef TIM_TimeBaseStructure;
  TIM_TimeBaseStructure.Prescaler = (TIMER_TIM_PERIPHERAL_FRQ/1000000)-1; // for 1 uS accuracy
  TIM_TimeBaseStructure.CounterMode = LL_TIM_COUNTERMODE_UP;
  TIM_TimeBaseStructure.Autoreload = period-1;
  TIM_TimeBaseStructure.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  // only meaningful for TIM1 (the sole advanced/repetition-counter-capable
  // instance among the three slots) - explicitly zeroed rather than left
  // uninitialised, since LL_TIM_Init() does write it there.
  TIM_TimeBaseStructure.RepetitionCounter = 0;
  LL_TIM_Init(timer_base[timer], &TIM_TimeBaseStructure);

  // enable interrupt
  LL_TIM_EnableIT_UPDATE(timer_base[timer]);

  // enable counter
  LL_TIM_EnableCounter(timer_base[timer]);

  // enable global interrupt
  MIOS32_IRQ_Install(timer_irq_chn[timer], irq_priority);

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! Re-Initialize a timer with given period
//!
//! Example:<BR>
//! \code
//!   // change timer period to 2 mS
//!   MIOS32_TIMER_ReInit(0, 2000);
//! \endcode
//! \param[in] timer (0..2)<BR>
//!     Timer allocation: 0=TIM1, 1=TIM3, 2=TIM16 (see TIMERn_BASE overrides above)
//! \param[in] period in uS accuracy (1..65536)
//! \return 0 if initialisation passed
//! \return if invalid timer number
//! \return -2 if invalid period
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_TIMER_ReInit(u8 timer, u32 period)
{
  // check if valid timer
  if( timer >= NUM_TIMERS )
    return -1; // invalid timer selected

  // check if valid period
  if( period < 1 || period >= 65537 )
    return -2;

  // time base configuration
  LL_TIM_InitTypeDef TIM_TimeBaseStructure;
  TIM_TimeBaseStructure.Prescaler = (TIMER_TIM_PERIPHERAL_FRQ/1000000)-1; // for 1 uS accuracy
  TIM_TimeBaseStructure.CounterMode = LL_TIM_COUNTERMODE_UP;
  TIM_TimeBaseStructure.Autoreload = period-1;
  TIM_TimeBaseStructure.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  TIM_TimeBaseStructure.RepetitionCounter = 0;
  LL_TIM_Init(timer_base[timer], &TIM_TimeBaseStructure);

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! De-Initialize a timer
//!
//! Example:<BR>
//! \code
//!   // disable timer
//!   MIOS32_TIMER_DeInit(0);
//! \endcode
//! \param[in] timer (0..2)
//! \return 0 if timer has been disabled
//! \return -1 if invalid timer number
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_TIMER_DeInit(u8 timer)
{
  // check if valid timer
  if( timer >= NUM_TIMERS )
    return -1; // invalid timer selected

  // deinitialize timer
  LL_TIM_DeInit(timer_base[timer]);

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! Interrupt handlers
//! \note don't call them directly from application
/////////////////////////////////////////////////////////////////////////////
#ifndef MIOS32_DONT_ALLOCATE_TIM1_IRQn
TIMER0_IRQ_HANDLER
{
  if( LL_TIM_IsActiveFlag_UPDATE(TIMER0_BASE) ) {
    LL_TIM_ClearFlag_UPDATE(TIMER0_BASE);
    timer_callback[0]();
  }
}
#endif

#ifndef MIOS32_DONT_ALLOCATE_TIM3_IRQn
TIMER1_IRQ_HANDLER
{
  if( LL_TIM_IsActiveFlag_UPDATE(TIMER1_BASE) ) {
    LL_TIM_ClearFlag_UPDATE(TIMER1_BASE);
    timer_callback[1]();
  }
}
#endif

#ifndef MIOS32_DONT_ALLOCATE_TIM16_IRQn
TIMER2_IRQ_HANDLER
{
  if( LL_TIM_IsActiveFlag_UPDATE(TIMER2_BASE) ) {
    LL_TIM_ClearFlag_UPDATE(TIMER2_BASE);
    timer_callback[2]();
  }
}
#endif

#endif /* MIOS32_USE_TIMER */


/////////////////////////////////////////////////////////////////////////////
// MIOS32_STOPWATCH - one-shot elapsed-time measurement
/////////////////////////////////////////////////////////////////////////////
#if defined(MIOS32_USE_STOPWATCH)

// single default timer - TIM17, confirmed present on EVERY STM32G0 chip
// tier (verified via each tier's CMSIS device header, from the smallest
// 2-USART G030/G031 up to the 6-USART+2xLPUART G0B1/G0C1) - unlike the
// former default TIM6, which doesn't exist at all below the 6-USART tier
// (found via a real G030K6 build failing on it). Doesn't collide with
// MIOS32_DELAY (TIM14) or MIOS32_TIMER's default table (TIM2/TIM3/TIM5,
// itself not universal either - TIM1/TIM16 left free for the application).
// Override all three together in your project's mios32_config.h if TIM17
// conflicts with something else on your hardware (STOPWATCH_TIMER_RCC_
// ENABLE must call the LL_APBx_GRP1_EnableClock() matching whichever bus
// your chosen timer sits on - TIM17 is on APB2, unlike TIM6's APB1).
#ifndef STOPWATCH_TIMER_BASE
#define STOPWATCH_TIMER_BASE                 TIM17
#endif
#ifndef STOPWATCH_TIMER_RCC
#define STOPWATCH_TIMER_RCC   LL_APB2_GRP1_PERIPH_TIM17
#endif
#ifndef STOPWATCH_TIMER_RCC_ENABLE
#define STOPWATCH_TIMER_RCC_ENABLE() LL_APB2_GRP1_EnableClock(STOPWATCH_TIMER_RCC)
#endif

// timers clocked at CPU/2 clock
#define STOPWATCH_TIM_PERIPHERAL_FRQ (MIOS32_SYS_CPU_FREQUENCY)

/////////////////////////////////////////////////////////////////////////////
//! Initializes the 16bit stopwatch timer with the desired resolution:
//! <UL>
//!  <LI>1: 1 uS resolution, time measurement possible in the range of 0.001mS .. 65.535 mS
//!  <LI>10: 10 uS resolution: 0.01 mS .. 655.35 mS
//!  <LI>100: 100 uS resolution: 0.1 mS .. 6.5535 seconds
//!  <LI>1000: NOT SUPPORTED FOR STM32F4!!!
//!  <LI>other values should not be used!
//! <UL>
//!
//! Example:<BR>
//! \code
//!   // initialize the stopwatch for 100 uS resolution
//!   // (only has to be done once, e.g. in APP_Init())
//!   MIOS32_STOPWATCH_Init(100);
//!
//!   // reset stopwatch
//!   MIOS32_STOPWATCH_Reset();
//!
//!   // execute function
//!   MyFunction();
//!
//!   // send execution time via DEFAULT COM interface
//!   u32 delay = MIOS32_STOPWATCH_ValueGet();
//!   printf("Execution time of MyFunction: ");
//!   if( delay == 0xffffffff )
//!     printf("Overrun!\n\r");
//!   else
//!     printf("%d.%d mS\n\r", delay/10, delay%10);
//! \endcode
//! \param[in] resolution 1, 10, 100 or 1000
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_STOPWATCH_Init(u32 resolution)
{
  // enable timer clock
  STOPWATCH_TIMER_RCC_ENABLE();

  // time base configuration
  LL_TIM_InitTypeDef  TIM_TimeBaseStructure;
  TIM_TimeBaseStructure.Autoreload = 65535; // maximum value
  TIM_TimeBaseStructure.Prescaler = ((STOPWATCH_TIM_PERIPHERAL_FRQ/1000000) * resolution)-1; // <resolution> uS accuracy
  TIM_TimeBaseStructure.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  TIM_TimeBaseStructure.CounterMode = LL_TIM_COUNTERMODE_UP;
  LL_TIM_Init(STOPWATCH_TIMER_BASE, &TIM_TimeBaseStructure);

  // enable interrupt request
  LL_TIM_EnableIT_UPDATE(STOPWATCH_TIMER_BASE);

  // start counter
  LL_TIM_EnableCounter(STOPWATCH_TIMER_BASE);

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! Stops the stopwatch
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_STOPWATCH_Stop(void)
{
  LL_TIM_DisableCounter(STOPWATCH_TIMER_BASE);

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! Resets the stopwatch
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_STOPWATCH_Reset(void)
{
  // reset counter
  STOPWATCH_TIMER_BASE->CNT = 1; // set to 1 instead of 0 to avoid new IRQ request
  LL_TIM_ClearFlag_UPDATE(STOPWATCH_TIMER_BASE);

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! Returns current value of stopwatch
//! \return 1..65535: valid stopwatch value
//! \return 0xffffffff: counter overrun
/////////////////////////////////////////////////////////////////////////////
u32 MIOS32_STOPWATCH_ValueGet(void)
{
  u32 value = STOPWATCH_TIMER_BASE->CNT;

  if( LL_TIM_IsActiveFlag_UPDATE(STOPWATCH_TIMER_BASE) )
    value = 0xffffffff;

  return value;
}

#endif /* MIOS32_USE_STOPWATCH */


/////////////////////////////////////////////////////////////////////////////
// MIOS32_SOL - Sign Of Life LED ("SOF" was the USB term Start Of Frame,
// misleading for a heartbeat indicator - renamed 2026-08-11)
// A single GPIO toggled as a heartbeat - project-configurable pin, meant as
// a lightweight replacement for mios32_board.c's fixed on-board LED.
/////////////////////////////////////////////////////////////////////////////
#if defined(MIOS32_USE_SOL)

// single default (same for every family/processor) - override both
// together in your project's mios32_config.h for custom hardware
#ifndef MIOS32_SOL_PORT
#define MIOS32_SOL_PORT GPIOA
#endif
#ifndef MIOS32_SOL_PIN
#define MIOS32_SOL_PIN LL_GPIO_PIN_12
#endif

/////////////////////////////////////////////////////////////////////////////
//! Initializes the sign-of-life LED GPIO as a push-pull output (cleared)
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SOL_Init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStructure;
  LL_GPIO_StructInit(&GPIO_InitStructure);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Pin = MIOS32_SOL_PIN;
  LL_GPIO_Init(MIOS32_SOL_PORT, &GPIO_InitStructure);

  return MIOS32_SOL_Clr();
}

/////////////////////////////////////////////////////////////////////////////
//! Turns the sign-of-life LED on
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SOL_Set(void)
{
  MIOS32_SYS_STM_PINSET_1(MIOS32_SOL_PORT, MIOS32_SOL_PIN);
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//! Turns the sign-of-life LED off
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SOL_Clr(void)
{
  MIOS32_SYS_STM_PINSET_0(MIOS32_SOL_PORT, MIOS32_SOL_PIN);
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//! Toggles the sign-of-life LED
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SOL_Tog(void)
{
  MIOS32_SOL_PORT->ODR ^= MIOS32_SOL_PIN;
  return 0;
}

#endif /* MIOS32_USE_SOL */

//! \}
