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
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
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

// timer used for MIOS32_DELAY functions (TIM1..TIM8)
#ifndef MIOS32_DELAY_TIMER
#define MIOS32_DELAY_TIMER  TIM1
#endif

#ifndef MIOS32_DELAY_TIMER_RCC
#define MIOS32_DELAY_TIMER_RCC LL_APB2_GRP1_PERIPH_TIM1
#endif

// override all three together if you override the timer above
// (MIOS32_DELAY_TIMER_RCC_ENABLE must call the LL_APBx_GRP1_EnableClock()
// matching whichever bus your chosen timer sits on)
#ifndef MIOS32_DELAY_TIMER_RCC_ENABLE
#define MIOS32_DELAY_TIMER_RCC_ENABLE() LL_APB2_GRP1_EnableClock(MIOS32_DELAY_TIMER_RCC)
#endif

// timers clocked at CPU clock
#define DELAY_TIM_PERIPHERAL_FRQ MIOS32_SYS_CPU_FREQUENCY

/////////////////////////////////////////////////////////////////////////////
//! Initializes the Timer used by MIOS32_DELAY functions<BR>
//! This function has to be executed before wait functions are used
//! (already done in main.c of the programming model)
//!
//! Currently TIM1 is allocated by MIOS32_DELAY functions - if this clashes with
//! your application, just switch to another timer by overriding
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
  MIOS32_DELAY_TIMER_RCC_ENABLE();

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

#define TIMER0_BASE                 TIM2
#define TIMER0_RCC   RCC_APB1Periph_TIM2
#define TIMER0_IRQ                  TIM2_IRQn
#define TIMER0_IRQ_HANDLER     void TIM2_IRQHandler(void)

#define TIMER1_BASE                 TIM3
#define TIMER1_RCC   RCC_APB1Periph_TIM3
#define TIMER1_IRQ                  TIM3_IRQn
#define TIMER1_IRQ_HANDLER     void TIM3_IRQHandler(void)

#define TIMER2_BASE                 TIM5
#define TIMER2_RCC   RCC_APB1Periph_TIM5
#define TIMER2_IRQ                  TIM5_IRQn
#define TIMER2_IRQ_HANDLER     void TIM5_IRQHandler(void)

// timers clocked at CPU/2 clock
#define TIMER_TIM_PERIPHERAL_FRQ (MIOS32_SYS_CPU_FREQUENCY/2)

static TIM_TypeDef *timer_base[NUM_TIMERS] = { TIMER0_BASE, TIMER1_BASE, TIMER2_BASE };
static u32 timer_rcc[NUM_TIMERS] = { TIMER0_RCC, TIMER1_RCC, TIMER2_RCC };
static const u32 timer_irq_chn[NUM_TIMERS] = { TIMER0_IRQ, TIMER1_IRQ, TIMER2_IRQ };
static void (*timer_callback[NUM_TIMERS])(void);

/////////////////////////////////////////////////////////////////////////////
//! Initialize a timer
//! \param[in] timer (0..2)<BR>
//!     Timer allocation on STM32: 0=TIM2, 1=TIM3, 2=TIM5
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

  // enable timer clock
  if( timer_base[timer] == TIM1 || timer_base[timer] == TIM8 )
    RCC_APB2PeriphClockCmd(timer_rcc[timer], ENABLE);
  else
    RCC_APB1PeriphClockCmd(timer_rcc[timer], ENABLE);

  // disable interrupt (if active from previous configuration)
  TIM_ITConfig(timer_base[timer], TIM_IT_Update, DISABLE);

  // copy callback function
  timer_callback[timer] = _irq_handler;

  // time base configuration
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
  TIM_TimeBaseStructure.TIM_Period = period-1;
  TIM_TimeBaseStructure.TIM_Prescaler = (TIMER_TIM_PERIPHERAL_FRQ/1000000)-1; // for 1 uS accuracy
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit(timer_base[timer], &TIM_TimeBaseStructure);

  // enable interrupt
  TIM_ITConfig(timer_base[timer], TIM_IT_Update, ENABLE);

  // enable counter
  TIM_Cmd(timer_base[timer], ENABLE);

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
//!     Timer allocation on STM32: 0=TIM2, 1=TIM3, 2=TIM5
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
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
  TIM_TimeBaseStructure.TIM_Period = period - 1;
  TIM_TimeBaseStructure.TIM_Prescaler = (TIMER_TIM_PERIPHERAL_FRQ/1000000)-1; // for 1 uS accuracy
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit(timer_base[timer], &TIM_TimeBaseStructure);

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
  TIM_DeInit(timer_base[timer]);

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! Interrupt handlers
//! \note don't call them directly from application
/////////////////////////////////////////////////////////////////////////////
#ifndef MIOS32_DONT_ALLOCATE_TIM2_IRQn
TIMER0_IRQ_HANDLER
{
  if( TIM_GetITStatus(TIMER0_BASE, TIM_IT_Update) != RESET ) {
    TIM_ClearITPendingBit(TIMER0_BASE, TIM_IT_Update);
    timer_callback[0]();
  }
}
#endif

#ifndef MIOS32_DONT_ALLOCATE_TIM3_IRQn
TIMER1_IRQ_HANDLER
{
  if( TIM_GetITStatus(TIMER1_BASE, TIM_IT_Update) != RESET ) {
    TIM_ClearITPendingBit(TIMER1_BASE, TIM_IT_Update);
    timer_callback[1]();
  }
}
#endif

#ifndef MIOS32_DONT_ALLOCATE_TIM5_IRQn
TIMER2_IRQ_HANDLER
{
  if( TIM_GetITStatus(TIMER2_BASE, TIM_IT_Update) != RESET ) {
    TIM_ClearITPendingBit(TIMER2_BASE, TIM_IT_Update);
    timer_callback[2]();
  }
}
#endif

#endif /* MIOS32_USE_TIMER */


/////////////////////////////////////////////////////////////////////////////
// MIOS32_STOPWATCH - one-shot elapsed-time measurement
/////////////////////////////////////////////////////////////////////////////
#if defined(MIOS32_USE_STOPWATCH)

// Single default timer - TIM11, confirmed present on EVERY STM32F4 whose
// device header ships here (17 of them, checked one by one). The former
// default TIM6 is NOT universal: the F401 and F411 have neither TIM6 nor
// TIM7, and the F410 carries only TIM1/TIM5/TIM6/TIM9/TIM11. Found the day
// the CMSIS device macro stopped being hand-listed (2026-08-13) and an F401
// could be built for the first time - it stopped right here, on a timer
// that does not exist in its silicon. Before that, such a chip silently
// compiled against the F405 header, where TIM6 resolves to a real address.
//
// Of the four universal ones, TIM11 is the one an application is least
// likely to want: single channel, 16 bits. TIM1 is already taken by
// MIOS32_DELAY on this family, TIM5 is the 32-bit general purpose timer,
// TIM9 has two channels. Same reasoning that picked TIM17 on the G0.
//
// Override all four together in your project's mios32_config.h if TIM11
// conflicts with something on your hardware. FOUR, not three:
// STOPWATCH_TIMER_RCC_ENABLE must call the LL_APBx_GRP1_EnableClock()
// matching the bus your timer sits on, AND STOPWATCH_TIM_PERIPHERAL_FRQ
// must match that bus too - see just below, it is not the same number on
// APB1 and APB2.
#ifndef STOPWATCH_TIMER_BASE
#define STOPWATCH_TIMER_BASE                 TIM11
#endif
#ifndef STOPWATCH_TIMER_RCC
#define STOPWATCH_TIMER_RCC   LL_APB2_GRP1_PERIPH_TIM11
#endif
#ifndef STOPWATCH_TIMER_RCC_ENABLE
#define STOPWATCH_TIMER_RCC_ENABLE() LL_APB2_GRP1_EnableClock(STOPWATCH_TIMER_RCC)
#endif

// Timer clock, which is NOT the bus clock: on STM32F4 a timer runs at twice
// its APB clock whenever that APB prescaler is not 1. mios32_sys.c sets
// APB1 to /4 and APB2 to /2, so an APB1 timer (the former TIM6) is clocked
// at HCLK/2 while an APB2 timer (TIM11) is clocked at HCLK. Getting this
// wrong does not break the build - it silently doubles or halves every
// duration the stopwatch reports.
#ifndef STOPWATCH_TIM_PERIPHERAL_FRQ
#define STOPWATCH_TIM_PERIPHERAL_FRQ (MIOS32_SYS_CPU_FREQUENCY)
#endif

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
//! \note: this function uses STOPWATCH_TIMER_BASE, TIM11 by default
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
