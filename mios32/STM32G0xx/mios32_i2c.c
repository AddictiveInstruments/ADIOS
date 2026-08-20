//! \defgroup MIOS32_I2C
//!
//! I2C driver for STM32G0xx - peripheral generation v2.
//!
//! Every G0 carries two I2C, and G0B0/G0B1/G0C1 carry a third. Port
//! numbering follows the peripheral: MIOS32_I2C0 is I2C1, MIOS32_I2C1 is
//! I2C2, MIOS32_I2C2 is I2C3 - see the port table below.
//!
//! The transfer engine itself is not here: it lives in
//! mios32/common/mios32_i2c_v2.inc, shared with the FMPI2C1 port of the
//! late STM32F4, which is the same v2 silicon. This file is the part that
//! IS chip-specific: which ports exist, on which pins, on which clock, and
//! behind which interrupt vector.
//!
//! ---------------------------------------------------------------------------
//! EVERY PIN AND AF BELOW, taken from ST's own MCU database
//! (STM32CubeMX/db/mcu/IP/GPIO-STM32*_gpio_v1_0_Modes.xml), so that
//! overriding one never means opening a datasheet. Package availability
//! still applies: check your part before picking one.
//!
//!   G030 / G031 / G041 / G050 / G051 / G061 / G070 / G071 / G081
//!     I2C1_SCL  PA9(AF6)  PB6(AF6)  PB8(AF6)
//!     I2C1_SDA  PA10(AF6) PB7(AF6)  PB9(AF6)
//!     I2C2_SCL  PB10(AF6) PB13(AF6)
//!     I2C2_SDA  PB11(AF6) PB14(AF6)
//!
//!   G0B0 / G0B1 / G0C1  (adds a third port, and more choices on the second)
//!     I2C1_SCL  PA9(AF6)  PB6(AF6)  PB8(AF6)
//!     I2C1_SDA  PA10(AF6) PB7(AF6)  PB9(AF6)
//!     I2C2_SCL  PA7(AF8)  PA9(AF8)  PB3(AF8)  PB10(AF6) PB13(AF6)
//!     I2C2_SDA  PA6(AF8)  PA10(AF8) PB4(AF8)  PB11(AF6) PB14(AF6)
//!     I2C3_SCL  PA7(AF9)  PB3(AF6)  PC0(AF6)
//!     I2C3_SDA  PA6(AF9)  PB4(AF6)  PC1(AF6)
//!
//! Small packages also expose PA9/PA10 remapped onto the PA11/PA12 pads
//! (SYSCFG PA11_RMP/PA12_RMP) - that is a pad question, not an AF question,
//! and the AF stays 6.
//! ---------------------------------------------------------------------------
//!
//! \{
/* ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron
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
#if defined(MIOS32_USE_I2C)


/////////////////////////////////////////////////////////////////////////////
// Which ports this chip actually has
//
// Same treatment as MIOS32_USE_SPI2 in mios32_spi.c: a port the silicon
// does not carry is forced off here, whatever the project asked for, rather
// than failing later at the linker with an undefined I2C3 symbol.
/////////////////////////////////////////////////////////////////////////////

// I2C3 (3rd port) exists on G0B0/G0B1/G0C1 only
#if defined(MIOS32_USE_I2C2) && !defined(MIOS32_PROCESSOR_STM32G0B0) && !defined(MIOS32_PROCESSOR_STM32G0B1) && !defined(MIOS32_PROCESSOR_STM32G0C1)
#undef MIOS32_USE_I2C2
#endif

// On G0B0/G0B1/G0C1 the second and third ports share ONE NVIC vector,
// I2C2_3_IRQn, where the rest of the family gives I2C2 its own I2C2_IRQn.
// Exactly the situation already handled for SPI's DMA channels on these
// chips - one handler has to service both records.
#if defined(MIOS32_PROCESSOR_STM32G0B0) || defined(MIOS32_PROCESSOR_STM32G0B1) || defined(MIOS32_PROCESSOR_STM32G0C1)
#define MIOS32_I2C_SHARED_IRQ_VECTOR 1
#endif


/////////////////////////////////////////////////////////////////////////////
// Port definitions
// (not part of mios32_i2c.h, since overruling would lead to a hardware
// dependency in MIOS32 applications - same reasoning as mios32_spi.c)
//
// Everything below can be redefined from a project's mios32_config.h. That
// is the whole point: these files are meant to be written once and never
// reopened, so the entire configuration surface of the peripheral is
// exposed - including the parts that live OUTSIDE it, in RCC (the kernel
// clock source) and SYSCFG (Fast-mode Plus drive).
/////////////////////////////////////////////////////////////////////////////

//! Bus frequency in Hz. 100000 standard, 400000 fast, up to 1000000 with
//! Fast-mode Plus enabled AND pull-ups sized for it. 400000 is what the
//! pre-2026 driver defaulted to; keep 100000 in mind for long or noisy runs.
#ifndef MIOS32_I2C0_BUS_FREQUENCY
#define MIOS32_I2C0_BUS_FREQUENCY 400000
#endif
#ifndef MIOS32_I2C1_BUS_FREQUENCY
#define MIOS32_I2C1_BUS_FREQUENCY 400000
#endif
#ifndef MIOS32_I2C2_BUS_FREQUENCY
#define MIOS32_I2C2_BUS_FREQUENCY 400000
#endif

//! Analog filter: suppresses spikes up to 50 nS, required by the I2C spec
//! in fast mode. Costs propagation delay, which the timing computation
//! accounts for. Disable it only with a reason.
#ifndef MIOS32_I2C_ANALOG_FILTER
#define MIOS32_I2C_ANALOG_FILTER LL_I2C_ANALOGFILTER_ENABLE
#endif

//! Digital filter, 0..15, in units of the kernel clock. 0 = off. Also enters
//! the timing computation.
#ifndef MIOS32_I2C_DIGITAL_FILTER
#define MIOS32_I2C_DIGITAL_FILTER 0
#endif

//! Kernel clock source. HSI16 makes the bus timing independent of any later
//! change to the clock tree - worth considering on a project that retunes
//! PCLK.
//!
//! Which ports get a say varies by chip, and the LL header is the honest
//! test for it: I2C1 always has a selector, I2C2 only on the parts whose
//! RCC carries an I2C2SEL field, and I2C3 never. Where there is none, the
//! kernel clock IS PCLK1 and is read back from RCC for the timing.
#ifndef MIOS32_I2C0_CLKSOURCE
#define MIOS32_I2C0_CLKSOURCE LL_RCC_I2C1_CLKSOURCE_PCLK1
#endif
#if defined(LL_RCC_I2C2_CLKSOURCE_PCLK1) && !defined(MIOS32_I2C1_CLKSOURCE)
#define MIOS32_I2C1_CLKSOURCE LL_RCC_I2C2_CLKSOURCE_PCLK1
#endif

//! 1 = this port runs without interrupts, MIOS32_I2C_TransferWait() drives
//! the state machine itself. The API is identical either way.
#ifndef MIOS32_I2C0_POLLED
#define MIOS32_I2C0_POLLED 0
#endif
#ifndef MIOS32_I2C1_POLLED
#define MIOS32_I2C1_POLLED 0
#endif
#ifndef MIOS32_I2C2_POLLED
#define MIOS32_I2C2_POLLED 0
#endif

//! GPIO side. I2C lines are open-drain by definition. The internal pull-ups
//! are ca. 40 kOhm - fine to keep a bus alive, NOT a substitute for real
//! external resistors above 100 kHz.
#ifndef MIOS32_I2C_PIN_PULL
#define MIOS32_I2C_PIN_PULL LL_GPIO_PULL_UP
#endif
#ifndef MIOS32_I2C_PIN_SPEED
#define MIOS32_I2C_PIN_SPEED LL_GPIO_SPEED_FREQ_HIGH
#endif

// ---- port 0 = I2C1 --------------------------------------------------------
#define MIOS32_I2C0_PTR        I2C1
#ifndef MIOS32_I2C0_CLOCK
#define MIOS32_I2C0_CLOCK      LL_APB1_GRP1_PERIPH_I2C1
#endif
#ifndef MIOS32_I2C0_SCL_PORT
#define MIOS32_I2C0_SCL_PORT   GPIOB
#endif
#ifndef MIOS32_I2C0_SCL_PIN
#define MIOS32_I2C0_SCL_PIN    LL_GPIO_PIN_6
#endif
#ifndef MIOS32_I2C0_SCL_AF
#define MIOS32_I2C0_SCL_AF     LL_GPIO_AF_6
#endif
#ifndef MIOS32_I2C0_SDA_PORT
#define MIOS32_I2C0_SDA_PORT   GPIOB
#endif
#ifndef MIOS32_I2C0_SDA_PIN
#define MIOS32_I2C0_SDA_PIN    LL_GPIO_PIN_7
#endif
#ifndef MIOS32_I2C0_SDA_AF
#define MIOS32_I2C0_SDA_AF     LL_GPIO_AF_6
#endif
#ifndef MIOS32_I2C0_IRQ_CHANNEL
#define MIOS32_I2C0_IRQ_CHANNEL I2C1_IRQn
#endif
#ifndef MIOS32_I2C0_IRQHANDLER_FUNC
#define MIOS32_I2C0_IRQHANDLER_FUNC void I2C1_IRQHandler(void)
#endif

// ---- port 1 = I2C2 --------------------------------------------------------
#define MIOS32_I2C1_PTR        I2C2
#ifndef MIOS32_I2C1_CLOCK
#define MIOS32_I2C1_CLOCK      LL_APB1_GRP1_PERIPH_I2C2
#endif
#ifndef MIOS32_I2C1_SCL_PORT
#define MIOS32_I2C1_SCL_PORT   GPIOB
#endif
#ifndef MIOS32_I2C1_SCL_PIN
#define MIOS32_I2C1_SCL_PIN    LL_GPIO_PIN_10
#endif
#ifndef MIOS32_I2C1_SCL_AF
#define MIOS32_I2C1_SCL_AF     LL_GPIO_AF_6
#endif
#ifndef MIOS32_I2C1_SDA_PORT
#define MIOS32_I2C1_SDA_PORT   GPIOB
#endif
#ifndef MIOS32_I2C1_SDA_PIN
#define MIOS32_I2C1_SDA_PIN    LL_GPIO_PIN_11
#endif
#ifndef MIOS32_I2C1_SDA_AF
#define MIOS32_I2C1_SDA_AF     LL_GPIO_AF_6
#endif
#ifndef MIOS32_I2C1_IRQ_CHANNEL
# if MIOS32_I2C_SHARED_IRQ_VECTOR
#  define MIOS32_I2C1_IRQ_CHANNEL I2C2_3_IRQn
# else
#  define MIOS32_I2C1_IRQ_CHANNEL I2C2_IRQn
# endif
#endif

// ---- port 2 = I2C3, G0B0/G0B1/G0C1 only -----------------------------------
#ifdef MIOS32_USE_I2C2
#define MIOS32_I2C2_PTR        I2C3
#ifndef MIOS32_I2C2_CLOCK
#define MIOS32_I2C2_CLOCK      LL_APB1_GRP1_PERIPH_I2C3
#endif
#ifndef MIOS32_I2C2_SCL_PORT
#define MIOS32_I2C2_SCL_PORT   GPIOB
#endif
#ifndef MIOS32_I2C2_SCL_PIN
#define MIOS32_I2C2_SCL_PIN    LL_GPIO_PIN_3
#endif
#ifndef MIOS32_I2C2_SCL_AF
#define MIOS32_I2C2_SCL_AF     LL_GPIO_AF_6
#endif
#ifndef MIOS32_I2C2_SDA_PORT
#define MIOS32_I2C2_SDA_PORT   GPIOB
#endif
#ifndef MIOS32_I2C2_SDA_PIN
#define MIOS32_I2C2_SDA_PIN    LL_GPIO_PIN_4
#endif
#ifndef MIOS32_I2C2_SDA_AF
#define MIOS32_I2C2_SDA_AF     LL_GPIO_AF_6
#endif
#ifndef MIOS32_I2C2_IRQ_CHANNEL
#define MIOS32_I2C2_IRQ_CHANNEL I2C2_3_IRQn
#endif
#endif

// The vector shared by ports 1 and 2 on G0B*, or port 1's own vector
// elsewhere. Named once so the handler below has a single definition.
#ifndef MIOS32_I2C1_IRQHANDLER_FUNC
# if MIOS32_I2C_SHARED_IRQ_VECTOR
#  define MIOS32_I2C1_IRQHANDLER_FUNC void I2C2_3_IRQHandler(void)
# else
#  define MIOS32_I2C1_IRQHANDLER_FUNC void I2C2_IRQHandler(void)
# endif
#endif


/////////////////////////////////////////////////////////////////////////////
// The shared v2 transfer engine
/////////////////////////////////////////////////////////////////////////////

#define I2CV2(name)   LL_I2C_##name
#define I2CV2_TypeDef I2C_TypeDef

#include "../common/mios32_i2c_v2.inc"


/////////////////////////////////////////////////////////////////////////////
// Local variables
//
// One record per ENABLED port, plus a table of pointers - a disabled port
// costs a NULL, not a record. Same shape as the UART buffers.
/////////////////////////////////////////////////////////////////////////////

#ifdef MIOS32_USE_I2C0
static mios32_i2c_v2_rec_t i2c0_rec;
#endif
#ifdef MIOS32_USE_I2C1
static mios32_i2c_v2_rec_t i2c1_rec;
#endif
#ifdef MIOS32_USE_I2C2
static mios32_i2c_v2_rec_t i2c2_rec;
#endif

#define MIOS32_I2C_PORT_NUM 3

static mios32_i2c_v2_rec_t * const i2c_rec[MIOS32_I2C_PORT_NUM] = {
#ifdef MIOS32_USE_I2C0
  &i2c0_rec,
#else
  NULL,
#endif
#ifdef MIOS32_USE_I2C1
  &i2c1_rec,
#else
  NULL,
#endif
#ifdef MIOS32_USE_I2C2
  &i2c2_rec,
#else
  NULL,
#endif
};


/////////////////////////////////////////////////////////////////////////////
// Local helper: brings up one port's pins
/////////////////////////////////////////////////////////////////////////////
static void MIOS32_I2C_PinInit(GPIO_TypeDef *port, u32 pin, u32 af)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct;

  GPIO_InitStruct.Pin = pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = MIOS32_I2C_PIN_SPEED;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
  GPIO_InitStruct.Pull = MIOS32_I2C_PIN_PULL;
  GPIO_InitStruct.Alternate = af;
  LL_GPIO_Init(port, &GPIO_InitStruct);
}


/////////////////////////////////////////////////////////////////////////////
//! Initializes every I2C port the project asked for.
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_I2C_Init(u32 mode)
{
  if( mode != 0 )
    return -1;

#ifdef MIOS32_USE_I2C0
  {
    u32 timing;

    LL_APB1_GRP1_EnableClock(MIOS32_I2C0_CLOCK);
    LL_RCC_SetI2CClockSource(MIOS32_I2C0_CLKSOURCE);

    MIOS32_I2C_PinInit(MIOS32_I2C0_SCL_PORT, MIOS32_I2C0_SCL_PIN, MIOS32_I2C0_SCL_AF);
    MIOS32_I2C_PinInit(MIOS32_I2C0_SDA_PORT, MIOS32_I2C0_SDA_PIN, MIOS32_I2C0_SDA_AF);

#ifdef MIOS32_I2C0_FASTMODEPLUS
    // 20 mA drive, a SYSCFG matter rather than an I2C one. On this family it
    // can be asked for per peripheral (LL_SYSCFG_I2C_FASTMODEPLUS_I2C1) or
    // per pin (..._PB6, ..._PB7, ..._PB8, ..._PB9, ..._PA9, ..._PA10).
    LL_SYSCFG_EnableFastModePlus(MIOS32_I2C0_FASTMODEPLUS);
#endif

#ifdef MIOS32_I2C0_TIMINGR
    timing = MIOS32_I2C0_TIMINGR;
#else
    timing = MIOS32_I2C_V2_TimingCalc(LL_RCC_GetI2CClockFreq(LL_RCC_I2C1_CLKSOURCE),
                                      MIOS32_I2C0_BUS_FREQUENCY,
                                      MIOS32_I2C_ANALOG_FILTER == LL_I2C_ANALOGFILTER_ENABLE,
                                      MIOS32_I2C_DIGITAL_FILTER);
#endif

    MIOS32_I2C_V2_PortInit(&i2c0_rec, MIOS32_I2C0_PTR, timing,
                           MIOS32_I2C_ANALOG_FILTER, MIOS32_I2C_DIGITAL_FILTER,
                           MIOS32_I2C0_POLLED);

#if !MIOS32_I2C0_POLLED
    MIOS32_IRQ_Install(MIOS32_I2C0_IRQ_CHANNEL, MIOS32_IRQ_I2C_EV_PRIORITY);
#endif
  }
#endif

#ifdef MIOS32_USE_I2C1
  {
    u32 timing;

    LL_APB1_GRP1_EnableClock(MIOS32_I2C1_CLOCK);
#ifdef MIOS32_I2C1_CLKSOURCE
    LL_RCC_SetI2CClockSource(MIOS32_I2C1_CLKSOURCE);
#endif

    MIOS32_I2C_PinInit(MIOS32_I2C1_SCL_PORT, MIOS32_I2C1_SCL_PIN, MIOS32_I2C1_SCL_AF);
    MIOS32_I2C_PinInit(MIOS32_I2C1_SDA_PORT, MIOS32_I2C1_SDA_PIN, MIOS32_I2C1_SDA_AF);

#ifdef MIOS32_I2C1_FASTMODEPLUS
    LL_SYSCFG_EnableFastModePlus(MIOS32_I2C1_FASTMODEPLUS);
#endif

#ifdef MIOS32_I2C1_TIMINGR
    timing = MIOS32_I2C1_TIMINGR;
#elif defined(LL_RCC_I2C2_CLKSOURCE)
    timing = MIOS32_I2C_V2_TimingCalc(LL_RCC_GetI2CClockFreq(LL_RCC_I2C2_CLKSOURCE),
                                      MIOS32_I2C1_BUS_FREQUENCY,
                                      MIOS32_I2C_ANALOG_FILTER == LL_I2C_ANALOGFILTER_ENABLE,
                                      MIOS32_I2C_DIGITAL_FILTER);
#else
    {
      // no selector for this port on this chip: PCLK1 it is
      LL_RCC_ClocksTypeDef clocks;
      LL_RCC_GetSystemClocksFreq(&clocks);
      timing = MIOS32_I2C_V2_TimingCalc(clocks.PCLK1_Frequency,
                                        MIOS32_I2C1_BUS_FREQUENCY,
                                        MIOS32_I2C_ANALOG_FILTER == LL_I2C_ANALOGFILTER_ENABLE,
                                        MIOS32_I2C_DIGITAL_FILTER);
    }
#endif

    MIOS32_I2C_V2_PortInit(&i2c1_rec, MIOS32_I2C1_PTR, timing,
                           MIOS32_I2C_ANALOG_FILTER, MIOS32_I2C_DIGITAL_FILTER,
                           MIOS32_I2C1_POLLED);

#if !MIOS32_I2C1_POLLED
    MIOS32_IRQ_Install(MIOS32_I2C1_IRQ_CHANNEL, MIOS32_IRQ_I2C_EV_PRIORITY);
#endif
  }
#endif

#ifdef MIOS32_USE_I2C2
  {
    u32 timing;

    LL_APB1_GRP1_EnableClock(MIOS32_I2C2_CLOCK);
    // no clock source selection for I2C3 on this family: PCLK1, always

    MIOS32_I2C_PinInit(MIOS32_I2C2_SCL_PORT, MIOS32_I2C2_SCL_PIN, MIOS32_I2C2_SCL_AF);
    MIOS32_I2C_PinInit(MIOS32_I2C2_SDA_PORT, MIOS32_I2C2_SDA_PIN, MIOS32_I2C2_SDA_AF);

#ifdef MIOS32_I2C2_FASTMODEPLUS
    LL_SYSCFG_EnableFastModePlus(MIOS32_I2C2_FASTMODEPLUS);
#endif

#ifdef MIOS32_I2C2_TIMINGR
    timing = MIOS32_I2C2_TIMINGR;
#else
    {
      // I2C3 has no clock source selector on this family, so its kernel
      // clock IS PCLK1 - read it rather than assume it
      LL_RCC_ClocksTypeDef clocks;
      LL_RCC_GetSystemClocksFreq(&clocks);
      timing = MIOS32_I2C_V2_TimingCalc(clocks.PCLK1_Frequency,
                                        MIOS32_I2C2_BUS_FREQUENCY,
                                        MIOS32_I2C_ANALOG_FILTER == LL_I2C_ANALOGFILTER_ENABLE,
                                        MIOS32_I2C_DIGITAL_FILTER);
    }
#endif

    MIOS32_I2C_V2_PortInit(&i2c2_rec, MIOS32_I2C2_PTR, timing,
                           MIOS32_I2C_ANALOG_FILTER, MIOS32_I2C_DIGITAL_FILTER,
                           MIOS32_I2C2_POLLED);

#if !MIOS32_I2C2_POLLED
    MIOS32_IRQ_Install(MIOS32_I2C2_IRQ_CHANNEL, MIOS32_IRQ_I2C_EV_PRIORITY);
#endif
  }
#endif

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Takes the port, so that two tasks cannot interleave transfers on the
//! same bus.
//! \param[in] i2c_port the port (0..2)
//! \param[in] semaphore_type MIOS32_I2C_BLOCKING or MIOS32_I2C_NON_BLOCKING
//! \return 0 on success, -1 if the port is taken (non-blocking request)
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_I2C_TransferBegin(u8 i2c_port, mios32_i2c_semaphore_t semaphore_type)
{
  mios32_i2c_v2_rec_t *rec;
  s32 status = -1;

  if( i2c_port >= MIOS32_I2C_PORT_NUM || (rec = i2c_rec[i2c_port]) == NULL )
    return MIOS32_I2C_ERROR_INVALID_PORT;

  do {
    MIOS32_IRQ_Disable();
    if( !rec->semaphore ) {
      rec->semaphore = 1;
      status = 0;
    }
    MIOS32_IRQ_Enable();
  } while( status < 0 && semaphore_type == MIOS32_I2C_BLOCKING );

  return status;
}


/////////////////////////////////////////////////////////////////////////////
//! Releases the port taken with MIOS32_I2C_TransferBegin().
//! \param[in] i2c_port the port (0..2)
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_I2C_TransferFinished(u8 i2c_port)
{
  mios32_i2c_v2_rec_t *rec;

  if( i2c_port >= MIOS32_I2C_PORT_NUM || (rec = i2c_rec[i2c_port]) == NULL )
    return MIOS32_I2C_ERROR_INVALID_PORT;

  rec->semaphore = 0;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Starts a transfer. Returns as soon as it is armed - use
//! MIOS32_I2C_TransferWait() to know how it ended.
//! \param[in] i2c_port the port (0..2)
//! \param[in] transfer MIOS32_I2C_READ, _WRITE or _WRITE_WITHOUT_STOP
//! \param[in] address the slave address in 8-bit form (bit 0 ignored: the
//!            direction comes from the transfer type)
//! \param[in] buffer where the bytes come from or go to
//! \param[in] len how many - any length, windows above 255 are chained
//!            through the peripheral's RELOAD mechanism
//! \return 0 on success, < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_I2C_Transfer(u8 i2c_port, mios32_i2c_transfer_t transfer, u8 address, u8 *buffer, u16 len)
{
  mios32_i2c_v2_rec_t *rec;

  if( i2c_port >= MIOS32_I2C_PORT_NUM || (rec = i2c_rec[i2c_port]) == NULL )
    return MIOS32_I2C_ERROR_INVALID_PORT;

  return MIOS32_I2C_V2_TransferStart(rec, transfer, address, buffer, len);
}


/////////////////////////////////////////////////////////////////////////////
//! Checks on a running transfer without waiting.
//! \param[in] i2c_port the port (0..2)
//! \return 0 finished, 1 still running, < 0 the error it failed with
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_I2C_TransferCheck(u8 i2c_port)
{
  mios32_i2c_v2_rec_t *rec;

  if( i2c_port >= MIOS32_I2C_PORT_NUM || (rec = i2c_rec[i2c_port]) == NULL )
    return MIOS32_I2C_ERROR_INVALID_PORT;

  if( rec->polled && rec->busy )
    MIOS32_I2C_V2_Step(rec);

  if( rec->busy )
    return 1;

  return rec->error;
}


/////////////////////////////////////////////////////////////////////////////
//! Waits for the running transfer to finish.
//! \param[in] i2c_port the port (0..2)
//! \return 0 on success, < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_I2C_TransferWait(u8 i2c_port)
{
  mios32_i2c_v2_rec_t *rec;

  if( i2c_port >= MIOS32_I2C_PORT_NUM || (rec = i2c_rec[i2c_port]) == NULL )
    return MIOS32_I2C_ERROR_INVALID_PORT;

  return MIOS32_I2C_V2_TransferWait(rec);
}


/////////////////////////////////////////////////////////////////////////////
//! \param[in] i2c_port the port (0..2)
//! \return the error the last transfer ended with, 0 if it went well
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_I2C_LastErrorGet(u8 i2c_port)
{
  mios32_i2c_v2_rec_t *rec;

  if( i2c_port >= MIOS32_I2C_PORT_NUM || (rec = i2c_rec[i2c_port]) == NULL )
    return MIOS32_I2C_ERROR_INVALID_PORT;

  return rec->error;
}


/////////////////////////////////////////////////////////////////////////////
// Interrupt handlers
/////////////////////////////////////////////////////////////////////////////

#if defined(MIOS32_USE_I2C0) && !MIOS32_I2C0_POLLED
MIOS32_I2C0_IRQHANDLER_FUNC
{
  MIOS32_I2C_V2_Step(&i2c0_rec);
}
#endif

// On G0B0/G0B1/G0C1 this one vector serves I2C2 AND I2C3: both records get
// stepped, and each ignores the call when it has nothing running.
#if (defined(MIOS32_USE_I2C1) && !MIOS32_I2C1_POLLED) || (defined(MIOS32_USE_I2C2) && !MIOS32_I2C2_POLLED)
MIOS32_I2C1_IRQHANDLER_FUNC
{
#if defined(MIOS32_USE_I2C1) && !MIOS32_I2C1_POLLED
  MIOS32_I2C_V2_Step(&i2c1_rec);
#endif
#if MIOS32_I2C_SHARED_IRQ_VECTOR && defined(MIOS32_USE_I2C2) && !MIOS32_I2C2_POLLED
  MIOS32_I2C_V2_Step(&i2c2_rec);
#endif
}
#endif

//! \}

#endif /* MIOS32_USE_I2C */
