//! \defgroup ADIOS_I2C
//!
//! I2C driver for STM32F4xx.
//!
//! This family is where the two generations of ST's I2C silicon meet:
//!
//!   - I2C1/I2C2/I2C3 are generation v1 - SR1/SR2, CCR, TRISE, and ACK and
//!     STOP posted by hand. That state machine is written out below, in
//!     this file, because nothing else in the tree speaks v1.
//!
//!   - FMPI2C1, present on F410/F412/F413/F423/F446 only, is generation v2 -
//!     the SAME block as the G0's I2C, Fast-mode Plus capable, which is
//!     where its name comes from. It is NOT driven by the code below: it
//!     uses adios/common/adios_i2c_v2.inc, shared with the G0 driver.
//!     Measured on the LL headers: stm32f4xx_ll_i2c.h has 62 SR1 and zero
//!     TIMINGR, stm32f4xx_ll_fmpi2c.h has 17 TIMINGR and 15 AUTOEND -
//!     figure for figure the same as stm32g0xx_ll_i2c.h.
//!
//! Port numbering follows the peripheral: ADIOS_I2C0 is I2C1, ADIOS_I2C1
//! is I2C2, ADIOS_I2C2 is I2C3. FMPI2C1 keeps ST's own name and sits at
//! port index ADIOS_I2C_PORT_FMPI2C0 (3) - it is not "I2C4", and on the
//! F410 there is no I2C3 at all, so numbering it in sequence would lie.
//!
//! ---------------------------------------------------------------------------
//! EVERY PIN AND AF BELOW, from ST's MCU database
//! (STM32CubeMX/db/mcu/IP/GPIO-STM32F4*_gpio_v1_0_Modes.xml). Package
//! availability still applies - check your part.
//!
//!   all F4 in this tree
//!     I2C1_SCL  PB6(AF4)  PB8(AF4)
//!     I2C1_SDA  PB7(AF4)  PB9(AF4)
//!     I2C2_SCL  PB10(AF4) PF1(AF4) [PH4(AF4) on F405/407/415/417/427/429/437/439/469/479]
//!     I2C2_SDA  PB11(AF4) PF0(AF4) [PH5(AF4) idem] [PB3(AF9)/PB9(AF9) on F401/410/411/412/413]
//!                                  [PB3(AF4)/PC12(AF4) on F446]
//!     I2C3_SCL  PA8(AF4)  [PH7(AF4) on the big parts]        - absent on F410
//!     I2C3_SDA  PC9(AF4)  [PH8(AF4)] [PB4(AF9)/PB8(AF9) on F401/411/412/413] [PB4(AF4) on F446]
//!
//!   FMPI2C1, F410 / F412 / F413 / F423 / F446 only
//!     FMPI2C1_SCL  PC6(AF4)  PB15(AF4) PB10(AF9) PD12(AF4) PD14(AF4) PF14(AF4)
//!     FMPI2C1_SDA  PC7(AF4)  PB14(AF4) PB3(AF4)  PD13(AF4) PD15(AF4) PF15(AF4)
//!     (F410 offers PA8/PB10/PB15/PC6 for SCL and PB3/PB14/PC7/PC9 for SDA)
//! ---------------------------------------------------------------------------
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

// this module can be optionally disabled in a local adios_config.h file (included from adios.h)
#if defined(ADIOS_USE_I2C)


/////////////////////////////////////////////////////////////////////////////
// Which ports this chip actually has
//
// Feature-tested on the CMSIS device header rather than on a list of
// processor names: I2C3_BASE and FMPI2C1 are exactly what tells the truth
// about the silicon, and the list of F4 parts is long enough that a name
// list would rot.
/////////////////////////////////////////////////////////////////////////////

// I2C3 is missing on the F410
#if defined(ADIOS_USE_I2C2) && !defined(I2C3)
#undef ADIOS_USE_I2C2
#endif

// FMPI2C1 exists on F410/F412/F413/F423/F446 only
#if defined(ADIOS_USE_FMPI2C0) && !defined(FMPI2C1)
#undef ADIOS_USE_FMPI2C0
#endif


/////////////////////////////////////////////////////////////////////////////
// Port definitions
// (not part of adios_i2c.h, since overruling would lead to a hardware
// dependency in ADIOS applications - same reasoning as adios_spi.c)
//
// All of it is overridable from a project's adios_config.h, including what
// lives outside the peripheral: the FMPI2C kernel clock in RCC.
/////////////////////////////////////////////////////////////////////////////

//! Bus frequency in Hz. v1 tops out at 400000; only FMPI2C1 can go beyond.
#ifndef ADIOS_I2C0_BUS_FREQUENCY
#define ADIOS_I2C0_BUS_FREQUENCY 400000
#endif
#ifndef ADIOS_I2C1_BUS_FREQUENCY
#define ADIOS_I2C1_BUS_FREQUENCY 400000
#endif
#ifndef ADIOS_I2C2_BUS_FREQUENCY
#define ADIOS_I2C2_BUS_FREQUENCY 400000
#endif
#ifndef ADIOS_FMPI2C0_BUS_FREQUENCY
#define ADIOS_FMPI2C0_BUS_FREQUENCY 400000
#endif

//! Fast mode duty cycle, v1 only: 2 gives t_LOW/t_HIGH = 2, 16/9 buys a bit
//! more margin on the rise time at 400 kHz.
#ifndef ADIOS_I2C_DUTYCYCLE
#define ADIOS_I2C_DUTYCYCLE LL_I2C_DUTYCYCLE_2
#endif

//! Analog filter (spikes up to 50 nS, required by the spec in fast mode)
//! and digital filter (0..15 peripheral clocks) - for the v1 ports.
//!
//! NOT EVERY F4 HAS THEM: the noise filter register I2C_FLTR arrived with
//! the F42x. On an F405/F407/F415/F417 there is no such register at all, so
//! there is nothing to configure and the LL init struct does not even carry
//! the fields. Feature-tested on CMSIS rather than on a list of part
//! numbers, which is the thing that cannot rot.
#if defined(I2C_FLTR_ANOFF) && defined(I2C_FLTR_DNF)
#ifndef ADIOS_I2C_ANALOG_FILTER
#define ADIOS_I2C_ANALOG_FILTER LL_I2C_ANALOGFILTER_ENABLE
#endif
#ifndef ADIOS_I2C_DIGITAL_FILTER
#define ADIOS_I2C_DIGITAL_FILTER 0
#endif
#endif

//! Same two, for FMPI2C1 - separate names because they are a separate
//! peripheral generation with its own LL constants, and because a chip can
//! carry a v2 port whose v1 ports have no filter register.
#ifndef ADIOS_FMPI2C_ANALOG_FILTER
#define ADIOS_FMPI2C_ANALOG_FILTER LL_FMPI2C_ANALOGFILTER_ENABLE
#endif
#ifndef ADIOS_FMPI2C_DIGITAL_FILTER
#define ADIOS_FMPI2C_DIGITAL_FILTER 0
#endif

//! 1 = no interrupts on this port, ADIOS_I2C_TransferWait() drives it.
#ifndef ADIOS_I2C0_POLLED
#define ADIOS_I2C0_POLLED 0
#endif
#ifndef ADIOS_I2C1_POLLED
#define ADIOS_I2C1_POLLED 0
#endif
#ifndef ADIOS_I2C2_POLLED
#define ADIOS_I2C2_POLLED 0
#endif
#ifndef ADIOS_FMPI2C0_POLLED
#define ADIOS_FMPI2C0_POLLED 0
#endif

//! GPIO side - open-drain by definition; the internal pull-ups are weak and
//! are no substitute for real resistors above 100 kHz.
#ifndef ADIOS_I2C_PIN_PULL
#define ADIOS_I2C_PIN_PULL LL_GPIO_PULL_UP
#endif
#ifndef ADIOS_I2C_PIN_SPEED
#define ADIOS_I2C_PIN_SPEED LL_GPIO_SPEED_FREQ_HIGH
#endif

// ---- port 0 = I2C1 (v1) ---------------------------------------------------
#define ADIOS_I2C0_PTR        I2C1
#ifndef ADIOS_I2C0_CLOCK
#define ADIOS_I2C0_CLOCK      LL_APB1_GRP1_PERIPH_I2C1
#endif
#ifndef ADIOS_I2C0_SCL_PORT
#define ADIOS_I2C0_SCL_PORT   GPIOB
#endif
#ifndef ADIOS_I2C0_SCL_PIN
#define ADIOS_I2C0_SCL_PIN    LL_GPIO_PIN_6
#endif
#ifndef ADIOS_I2C0_SCL_AF
#define ADIOS_I2C0_SCL_AF     LL_GPIO_AF_4
#endif
#ifndef ADIOS_I2C0_SDA_PORT
#define ADIOS_I2C0_SDA_PORT   GPIOB
#endif
#ifndef ADIOS_I2C0_SDA_PIN
#define ADIOS_I2C0_SDA_PIN    LL_GPIO_PIN_7
#endif
#ifndef ADIOS_I2C0_SDA_AF
#define ADIOS_I2C0_SDA_AF     LL_GPIO_AF_4
#endif
#ifndef ADIOS_I2C0_IRQ_EV_CHANNEL
#define ADIOS_I2C0_IRQ_EV_CHANNEL I2C1_EV_IRQn
#endif
#ifndef ADIOS_I2C0_IRQ_ER_CHANNEL
#define ADIOS_I2C0_IRQ_ER_CHANNEL I2C1_ER_IRQn
#endif
#ifndef ADIOS_I2C0_IRQHANDLER_EV_FUNC
#define ADIOS_I2C0_IRQHANDLER_EV_FUNC void I2C1_EV_IRQHandler(void)
#endif
#ifndef ADIOS_I2C0_IRQHANDLER_ER_FUNC
#define ADIOS_I2C0_IRQHANDLER_ER_FUNC void I2C1_ER_IRQHandler(void)
#endif

// ---- port 1 = I2C2 (v1) ---------------------------------------------------
#define ADIOS_I2C1_PTR        I2C2
#ifndef ADIOS_I2C1_CLOCK
#define ADIOS_I2C1_CLOCK      LL_APB1_GRP1_PERIPH_I2C2
#endif
#ifndef ADIOS_I2C1_SCL_PORT
#define ADIOS_I2C1_SCL_PORT   GPIOB
#endif
#ifndef ADIOS_I2C1_SCL_PIN
#define ADIOS_I2C1_SCL_PIN    LL_GPIO_PIN_10
#endif
#ifndef ADIOS_I2C1_SCL_AF
#define ADIOS_I2C1_SCL_AF     LL_GPIO_AF_4
#endif
#ifndef ADIOS_I2C1_SDA_PORT
#define ADIOS_I2C1_SDA_PORT   GPIOB
#endif
#ifndef ADIOS_I2C1_SDA_PIN
#define ADIOS_I2C1_SDA_PIN    LL_GPIO_PIN_11
#endif
#ifndef ADIOS_I2C1_SDA_AF
#define ADIOS_I2C1_SDA_AF     LL_GPIO_AF_4
#endif
#ifndef ADIOS_I2C1_IRQ_EV_CHANNEL
#define ADIOS_I2C1_IRQ_EV_CHANNEL I2C2_EV_IRQn
#endif
#ifndef ADIOS_I2C1_IRQ_ER_CHANNEL
#define ADIOS_I2C1_IRQ_ER_CHANNEL I2C2_ER_IRQn
#endif
#ifndef ADIOS_I2C1_IRQHANDLER_EV_FUNC
#define ADIOS_I2C1_IRQHANDLER_EV_FUNC void I2C2_EV_IRQHandler(void)
#endif
#ifndef ADIOS_I2C1_IRQHANDLER_ER_FUNC
#define ADIOS_I2C1_IRQHANDLER_ER_FUNC void I2C2_ER_IRQHandler(void)
#endif

// ---- port 2 = I2C3 (v1), absent on F410 -----------------------------------
#ifdef ADIOS_USE_I2C2
#define ADIOS_I2C2_PTR        I2C3
#ifndef ADIOS_I2C2_CLOCK
#define ADIOS_I2C2_CLOCK      LL_APB1_GRP1_PERIPH_I2C3
#endif
#ifndef ADIOS_I2C2_SCL_PORT
#define ADIOS_I2C2_SCL_PORT   GPIOA
#endif
#ifndef ADIOS_I2C2_SCL_PIN
#define ADIOS_I2C2_SCL_PIN    LL_GPIO_PIN_8
#endif
#ifndef ADIOS_I2C2_SCL_AF
#define ADIOS_I2C2_SCL_AF     LL_GPIO_AF_4
#endif
#ifndef ADIOS_I2C2_SDA_PORT
#define ADIOS_I2C2_SDA_PORT   GPIOC
#endif
#ifndef ADIOS_I2C2_SDA_PIN
#define ADIOS_I2C2_SDA_PIN    LL_GPIO_PIN_9
#endif
#ifndef ADIOS_I2C2_SDA_AF
#define ADIOS_I2C2_SDA_AF     LL_GPIO_AF_4
#endif
#ifndef ADIOS_I2C2_IRQ_EV_CHANNEL
#define ADIOS_I2C2_IRQ_EV_CHANNEL I2C3_EV_IRQn
#endif
#ifndef ADIOS_I2C2_IRQ_ER_CHANNEL
#define ADIOS_I2C2_IRQ_ER_CHANNEL I2C3_ER_IRQn
#endif
#ifndef ADIOS_I2C2_IRQHANDLER_EV_FUNC
#define ADIOS_I2C2_IRQHANDLER_EV_FUNC void I2C3_EV_IRQHandler(void)
#endif
#ifndef ADIOS_I2C2_IRQHANDLER_ER_FUNC
#define ADIOS_I2C2_IRQHANDLER_ER_FUNC void I2C3_ER_IRQHandler(void)
#endif
#endif

// ---- port 3 = FMPI2C1 (v2), F410/F412/F413/F423/F446 only ------------------
#ifdef ADIOS_USE_FMPI2C0
#define ADIOS_FMPI2C0_PTR     FMPI2C1
#ifndef ADIOS_FMPI2C0_CLOCK
#define ADIOS_FMPI2C0_CLOCK   LL_APB1_GRP1_PERIPH_FMPI2C1
#endif
//! Kernel clock. Unlike v1, a v2 block does not run off PCLK unless told to;
//! HSI makes its timing independent of the clock tree.
#ifndef ADIOS_FMPI2C0_CLKSOURCE
#define ADIOS_FMPI2C0_CLKSOURCE LL_RCC_FMPI2C1_CLKSOURCE_PCLK1
#endif
#ifndef ADIOS_FMPI2C0_SCL_PORT
#define ADIOS_FMPI2C0_SCL_PORT GPIOC
#endif
#ifndef ADIOS_FMPI2C0_SCL_PIN
#define ADIOS_FMPI2C0_SCL_PIN  LL_GPIO_PIN_6
#endif
#ifndef ADIOS_FMPI2C0_SCL_AF
#define ADIOS_FMPI2C0_SCL_AF   LL_GPIO_AF_4
#endif
#ifndef ADIOS_FMPI2C0_SDA_PORT
#define ADIOS_FMPI2C0_SDA_PORT GPIOC
#endif
#ifndef ADIOS_FMPI2C0_SDA_PIN
#define ADIOS_FMPI2C0_SDA_PIN  LL_GPIO_PIN_7
#endif
#ifndef ADIOS_FMPI2C0_SDA_AF
#define ADIOS_FMPI2C0_SDA_AF   LL_GPIO_AF_4
#endif
#ifndef ADIOS_FMPI2C0_IRQ_EV_CHANNEL
#define ADIOS_FMPI2C0_IRQ_EV_CHANNEL FMPI2C1_EV_IRQn
#endif
#ifndef ADIOS_FMPI2C0_IRQ_ER_CHANNEL
#define ADIOS_FMPI2C0_IRQ_ER_CHANNEL FMPI2C1_ER_IRQn
#endif
#ifndef ADIOS_FMPI2C0_IRQHANDLER_EV_FUNC
#define ADIOS_FMPI2C0_IRQHANDLER_EV_FUNC void FMPI2C1_EV_IRQHandler(void)
#endif
#ifndef ADIOS_FMPI2C0_IRQHANDLER_ER_FUNC
#define ADIOS_FMPI2C0_IRQHANDLER_ER_FUNC void FMPI2C1_ER_IRQHandler(void)
#endif
#endif


/////////////////////////////////////////////////////////////////////////////
// The shared v2 transfer engine - for FMPI2C1 only
/////////////////////////////////////////////////////////////////////////////

#ifdef ADIOS_USE_FMPI2C0
#define I2CV2(name)   LL_FMPI2C_##name
#define I2CV2_TypeDef FMPI2C_TypeDef

#include "../common/adios_i2c_v2.inc"
#endif


/////////////////////////////////////////////////////////////////////////////
// The v1 transfer engine - for I2C1/I2C2/I2C3
//
// v1 has no NBYTES and no AUTOEND: the master posts every ACK and the STOP
// itself, and the tail of a read is a genuinely awkward dance because the
// peripheral has a one-byte shift register behind the data register. The
// three cases below (1 byte, 2 bytes, more) are the sequences from the
// reference manual, and they are not interchangeable.
/////////////////////////////////////////////////////////////////////////////

typedef enum {
  V1_IDLE = 0,
  V1_START,       // START posted, waiting for SB
  V1_ADDR,        // address posted, waiting for ADDR
  V1_TX,          // moving bytes out
  V1_RX           // moving bytes in
} adios_i2c_v1_state_t;

typedef struct {
  I2C_TypeDef *base;

  u8  *buffer;
  u16  len;
  u16  ix;

  u8   address;
  u8   transfer_type;
  u8   state;

  u8   polled;
  volatile u8  busy;
  volatile s32 error;
  volatile u8  semaphore;
} adios_i2c_v1_rec_t;


static void ADIOS_I2C_V1_Finish(adios_i2c_v1_rec_t *rec, s32 error)
{
  LL_I2C_DisableIT_EVT(rec->base);
  LL_I2C_DisableIT_BUF(rec->base);
  LL_I2C_DisableIT_ERR(rec->base);

  rec->state = V1_IDLE;
  rec->error = error;
  rec->busy = 0;
}


static void ADIOS_I2C_V1_Step(adios_i2c_v1_rec_t *rec)
{
  I2C_TypeDef *base = rec->base;

  if( !rec->busy )
    return;

  // errors first

  if( LL_I2C_IsActiveFlag_AF(base) ) {
    LL_I2C_ClearFlag_AF(base);
    LL_I2C_GenerateStopCondition(base);
    ADIOS_I2C_V1_Finish(rec, ADIOS_I2C_ERROR_SLAVE_NOT_CONNECTED);
    return;
  }

  if( LL_I2C_IsActiveFlag_ARLO(base) ) {
    LL_I2C_ClearFlag_ARLO(base);
    ADIOS_I2C_V1_Finish(rec, ADIOS_I2C_ERROR_ARBITRATION_LOST);
    return;
  }

  if( LL_I2C_IsActiveFlag_BERR(base) ) {
    LL_I2C_ClearFlag_BERR(base);
    LL_I2C_GenerateStopCondition(base);
    ADIOS_I2C_V1_Finish(rec, ADIOS_I2C_ERROR_BUS);
    return;
  }

  if( LL_I2C_IsActiveFlag_OVR(base) ) {
    LL_I2C_ClearFlag_OVR(base);
    ADIOS_I2C_V1_Finish(rec, ADIOS_I2C_ERROR_RX_BUFFER_OVERRUN);
    return;
  }

  switch( rec->state ) {

  case V1_START:
    if( LL_I2C_IsActiveFlag_SB(base) ) {
      // SB is cleared by reading SR1 (done above) then writing DR
      if( rec->transfer_type == ADIOS_I2C_READ )
        LL_I2C_TransmitData8(base, rec->address | 1);
      else
        LL_I2C_TransmitData8(base, rec->address & ~1);
      rec->state = V1_ADDR;
    }
    break;

  case V1_ADDR:
    if( LL_I2C_IsActiveFlag_ADDR(base) ) {
      if( rec->transfer_type != ADIOS_I2C_READ ) {
        LL_I2C_ClearFlag_ADDR(base);
        rec->state = V1_TX;
        break;
      }

      // reads: what has to happen around the ADDR clear depends on how many
      // bytes are still coming, because the shift register is already
      // fetching the next one
      if( rec->len == 1 ) {
        LL_I2C_AcknowledgeNextData(base, LL_I2C_NACK);
        LL_I2C_ClearFlag_ADDR(base);
        LL_I2C_GenerateStopCondition(base);
      } else if( rec->len == 2 ) {
        // POS was set at transfer start: the NACK applies to the byte AFTER
        // the one currently shifting in
        LL_I2C_AcknowledgeNextData(base, LL_I2C_NACK);
        LL_I2C_ClearFlag_ADDR(base);
      } else {
        LL_I2C_AcknowledgeNextData(base, LL_I2C_ACK);
        LL_I2C_ClearFlag_ADDR(base);
      }
      rec->state = V1_RX;
    }
    break;

  case V1_TX:
    if( rec->ix < rec->len ) {
      if( LL_I2C_IsActiveFlag_TXE(base) )
        LL_I2C_TransmitData8(base, rec->buffer[rec->ix++]);
    } else if( LL_I2C_IsActiveFlag_BTF(base) || LL_I2C_IsActiveFlag_TXE(base) ) {
      // everything handed over AND shifted out
      if( rec->transfer_type == ADIOS_I2C_WRITE_WITHOUT_STOP ) {
        // bus deliberately held for the repeated START the caller will issue
        ADIOS_I2C_V1_Finish(rec, 0);
      } else {
        LL_I2C_GenerateStopCondition(base);
        ADIOS_I2C_V1_Finish(rec, 0);
      }
    }
    break;

  case V1_RX: {
    u16 remaining = rec->len - rec->ix;

    if( rec->len == 1 ) {
      if( LL_I2C_IsActiveFlag_RXNE(base) ) {
        rec->buffer[rec->ix++] = LL_I2C_ReceiveData8(base);
        ADIOS_I2C_V1_Finish(rec, 0);
      }
    } else if( rec->len == 2 ) {
      // both bytes are taken at once, after BTF, with the STOP posted
      // between them - reading earlier would let the peripheral ACK a third
      if( LL_I2C_IsActiveFlag_BTF(base) ) {
        LL_I2C_GenerateStopCondition(base);
        rec->buffer[rec->ix++] = LL_I2C_ReceiveData8(base);
        rec->buffer[rec->ix++] = LL_I2C_ReceiveData8(base);
        LL_I2C_DisableBitPOS(base);
        ADIOS_I2C_V1_Finish(rec, 0);
      }
    } else {
      if( remaining > 3 ) {
        if( LL_I2C_IsActiveFlag_RXNE(base) )
          rec->buffer[rec->ix++] = LL_I2C_ReceiveData8(base);
      } else if( remaining == 3 ) {
        // the last three are taken on BTF so that the NACK lands on the
        // right byte: N-2 is read, then STOP, then N-1
        if( LL_I2C_IsActiveFlag_BTF(base) ) {
          LL_I2C_AcknowledgeNextData(base, LL_I2C_NACK);
          rec->buffer[rec->ix++] = LL_I2C_ReceiveData8(base);
          LL_I2C_GenerateStopCondition(base);
          rec->buffer[rec->ix++] = LL_I2C_ReceiveData8(base);
        }
      } else if( remaining == 1 ) {
        if( LL_I2C_IsActiveFlag_RXNE(base) ) {
          rec->buffer[rec->ix++] = LL_I2C_ReceiveData8(base);
          ADIOS_I2C_V1_Finish(rec, 0);
        }
      }
    }
  } break;

  default:
    break;
  }
}


static s32 ADIOS_I2C_V1_TransferStart(adios_i2c_v1_rec_t *rec,
                                       adios_i2c_transfer_t transfer,
                                       u8 address, u8 *buffer, u16 len)
{
  if( rec->base == NULL )
    return ADIOS_I2C_ERROR_INVALID_PORT;

  if( transfer != ADIOS_I2C_READ &&
      transfer != ADIOS_I2C_WRITE &&
      transfer != ADIOS_I2C_WRITE_WITHOUT_STOP )
    return ADIOS_I2C_ERROR_UNSUPPORTED_TRANSFER_TYPE;

  if( rec->error < 0 ) {
    s32 prev = rec->error;
    rec->error = 0;
    return prev + ADIOS_I2C_ERROR_PREV_OFFSET;
  }

  rec->buffer = buffer;
  rec->len = len;
  rec->ix = 0;
  rec->address = address;
  rec->transfer_type = (u8)transfer;
  rec->error = 0;
  rec->busy = 1;
  rec->state = V1_START;

  // POS only means anything for a two-byte read, and it must be set before
  // the address is acknowledged
  if( transfer == ADIOS_I2C_READ && len == 2 )
    LL_I2C_EnableBitPOS(rec->base);
  else
    LL_I2C_DisableBitPOS(rec->base);

  LL_I2C_AcknowledgeNextData(rec->base, LL_I2C_ACK);
  LL_I2C_GenerateStartCondition(rec->base);

  if( !rec->polled ) {
    LL_I2C_EnableIT_EVT(rec->base);
    LL_I2C_EnableIT_BUF(rec->base);
    LL_I2C_EnableIT_ERR(rec->base);
  }

  return 0;
}


static s32 ADIOS_I2C_V1_TransferWait(adios_i2c_v1_rec_t *rec)
{
  u32 timeout = ADIOS_I2C_TIMEOUT_VALUE;

  while( rec->busy && timeout-- ) {
    if( rec->polled )
      ADIOS_I2C_V1_Step(rec);
#ifndef ADIOS_DONT_USE_DELAY
    else
      ADIOS_DELAY_Wait_uS(1);
#endif
  }

  if( rec->busy ) {
    LL_I2C_GenerateStopCondition(rec->base);
    ADIOS_I2C_V1_Finish(rec, ADIOS_I2C_ERROR_TIMEOUT);
    // The STOP request itself can sit for ever on a wedged interface. PE low
    // resets the state machine and releases the lines, the configuration
    // registers are kept - same treatment as the v2 engine's timeout.
    LL_I2C_Disable(rec->base);
    (void)LL_I2C_IsEnabled(rec->base);   // PE must stay low a few APB cycles
    LL_I2C_Enable(rec->base);
    return ADIOS_I2C_ERROR_TIMEOUT;
  }

  return rec->error;
}


static void ADIOS_I2C_V1_PortInit(adios_i2c_v1_rec_t *rec, I2C_TypeDef *base,
                                   u32 bus_frequency, u8 polled)
{
  LL_I2C_InitTypeDef I2C_InitStruct;

  rec->base = base;
  rec->buffer = NULL;
  rec->len = 0;
  rec->ix = 0;
  rec->state = V1_IDLE;
  rec->busy = 0;
  rec->error = 0;
  rec->semaphore = 0;
  rec->polled = polled;

  LL_I2C_Disable(base);

  // LL_I2C_Init works out FREQR, CCR and TRISE from the APB clock it reads
  // back from RCC - which is the whole timing story on v1, there is no
  // TIMINGR word here.
  I2C_InitStruct.PeripheralMode = LL_I2C_MODE_I2C;
  I2C_InitStruct.ClockSpeed = bus_frequency;
  I2C_InitStruct.DutyCycle = ADIOS_I2C_DUTYCYCLE;
#if defined(I2C_FLTR_ANOFF) && defined(I2C_FLTR_DNF)
  I2C_InitStruct.AnalogFilter = ADIOS_I2C_ANALOG_FILTER;
  I2C_InitStruct.DigitalFilter = ADIOS_I2C_DIGITAL_FILTER;
#endif
  I2C_InitStruct.OwnAddress1 = 0;
  I2C_InitStruct.TypeAcknowledge = LL_I2C_ACK;
  I2C_InitStruct.OwnAddrSize = LL_I2C_OWNADDRESS1_7BIT;
  LL_I2C_Init(base, &I2C_InitStruct);

  LL_I2C_Enable(base);
}


/////////////////////////////////////////////////////////////////////////////
// Local variables
//
// One record per ENABLED port; a disabled one costs a NULL in the table.
/////////////////////////////////////////////////////////////////////////////

#ifdef ADIOS_USE_I2C0
static adios_i2c_v1_rec_t i2c0_rec;
#endif
#ifdef ADIOS_USE_I2C1
static adios_i2c_v1_rec_t i2c1_rec;
#endif
#ifdef ADIOS_USE_I2C2
static adios_i2c_v1_rec_t i2c2_rec;
#endif
#ifdef ADIOS_USE_FMPI2C0
static adios_i2c_v2_rec_t fmpi2c0_rec;
#endif

#define ADIOS_I2C_PORT_NUM 4

static adios_i2c_v1_rec_t * const i2c_v1_rec[ADIOS_I2C_PORT_NUM] = {
#ifdef ADIOS_USE_I2C0
  &i2c0_rec,
#else
  NULL,
#endif
#ifdef ADIOS_USE_I2C1
  &i2c1_rec,
#else
  NULL,
#endif
#ifdef ADIOS_USE_I2C2
  &i2c2_rec,
#else
  NULL,
#endif
  NULL   // port 3 is FMPI2C1, a v2 block - not driven by the v1 engine
};


/////////////////////////////////////////////////////////////////////////////
// Local helper: brings up one port's pins
/////////////////////////////////////////////////////////////////////////////
static void ADIOS_I2C_PinInit(GPIO_TypeDef *port, u32 pin, u32 af)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct;

  GPIO_InitStruct.Pin = pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = ADIOS_I2C_PIN_SPEED;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
  GPIO_InitStruct.Pull = ADIOS_I2C_PIN_PULL;
  GPIO_InitStruct.Alternate = af;
  LL_GPIO_Init(port, &GPIO_InitStruct);
}


/////////////////////////////////////////////////////////////////////////////
//! Initializes every I2C port the project asked for.
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_I2C_Init(u32 mode)
{
  if( mode != 0 )
    return -1;

#ifdef ADIOS_USE_I2C0
  LL_APB1_GRP1_EnableClock(ADIOS_I2C0_CLOCK);
  ADIOS_I2C_PinInit(ADIOS_I2C0_SCL_PORT, ADIOS_I2C0_SCL_PIN, ADIOS_I2C0_SCL_AF);
  ADIOS_I2C_PinInit(ADIOS_I2C0_SDA_PORT, ADIOS_I2C0_SDA_PIN, ADIOS_I2C0_SDA_AF);
  ADIOS_I2C_V1_PortInit(&i2c0_rec, ADIOS_I2C0_PTR, ADIOS_I2C0_BUS_FREQUENCY, ADIOS_I2C0_POLLED);
#if !ADIOS_I2C0_POLLED
  ADIOS_IRQ_Install(ADIOS_I2C0_IRQ_EV_CHANNEL, ADIOS_IRQ_I2C_EV_PRIORITY);
  ADIOS_IRQ_Install(ADIOS_I2C0_IRQ_ER_CHANNEL, ADIOS_IRQ_I2C_ER_PRIORITY);
#endif
#endif

#ifdef ADIOS_USE_I2C1
  LL_APB1_GRP1_EnableClock(ADIOS_I2C1_CLOCK);
  ADIOS_I2C_PinInit(ADIOS_I2C1_SCL_PORT, ADIOS_I2C1_SCL_PIN, ADIOS_I2C1_SCL_AF);
  ADIOS_I2C_PinInit(ADIOS_I2C1_SDA_PORT, ADIOS_I2C1_SDA_PIN, ADIOS_I2C1_SDA_AF);
  ADIOS_I2C_V1_PortInit(&i2c1_rec, ADIOS_I2C1_PTR, ADIOS_I2C1_BUS_FREQUENCY, ADIOS_I2C1_POLLED);
#if !ADIOS_I2C1_POLLED
  ADIOS_IRQ_Install(ADIOS_I2C1_IRQ_EV_CHANNEL, ADIOS_IRQ_I2C_EV_PRIORITY);
  ADIOS_IRQ_Install(ADIOS_I2C1_IRQ_ER_CHANNEL, ADIOS_IRQ_I2C_ER_PRIORITY);
#endif
#endif

#ifdef ADIOS_USE_I2C2
  LL_APB1_GRP1_EnableClock(ADIOS_I2C2_CLOCK);
  ADIOS_I2C_PinInit(ADIOS_I2C2_SCL_PORT, ADIOS_I2C2_SCL_PIN, ADIOS_I2C2_SCL_AF);
  ADIOS_I2C_PinInit(ADIOS_I2C2_SDA_PORT, ADIOS_I2C2_SDA_PIN, ADIOS_I2C2_SDA_AF);
  ADIOS_I2C_V1_PortInit(&i2c2_rec, ADIOS_I2C2_PTR, ADIOS_I2C2_BUS_FREQUENCY, ADIOS_I2C2_POLLED);
#if !ADIOS_I2C2_POLLED
  ADIOS_IRQ_Install(ADIOS_I2C2_IRQ_EV_CHANNEL, ADIOS_IRQ_I2C_EV_PRIORITY);
  ADIOS_IRQ_Install(ADIOS_I2C2_IRQ_ER_CHANNEL, ADIOS_IRQ_I2C_ER_PRIORITY);
#endif
#endif

#ifdef ADIOS_USE_FMPI2C0
  {
    u32 timing;

    LL_APB1_GRP1_EnableClock(ADIOS_FMPI2C0_CLOCK);
    LL_RCC_SetFMPI2CClockSource(ADIOS_FMPI2C0_CLKSOURCE);

    ADIOS_I2C_PinInit(ADIOS_FMPI2C0_SCL_PORT, ADIOS_FMPI2C0_SCL_PIN, ADIOS_FMPI2C0_SCL_AF);
    ADIOS_I2C_PinInit(ADIOS_FMPI2C0_SDA_PORT, ADIOS_FMPI2C0_SDA_PIN, ADIOS_FMPI2C0_SDA_AF);

#ifdef ADIOS_FMPI2C0_TIMINGR
    timing = ADIOS_FMPI2C0_TIMINGR;
#else
    timing = ADIOS_I2C_V2_TimingCalc(LL_RCC_GetFMPI2CClockFreq(LL_RCC_FMPI2C1_CLKSOURCE),
                                      ADIOS_FMPI2C0_BUS_FREQUENCY,
                                      ADIOS_FMPI2C_ANALOG_FILTER == LL_FMPI2C_ANALOGFILTER_ENABLE,
                                      ADIOS_FMPI2C_DIGITAL_FILTER);
#endif

    ADIOS_I2C_V2_PortInit(&fmpi2c0_rec, ADIOS_FMPI2C0_PTR, timing,
                           ADIOS_FMPI2C_ANALOG_FILTER, ADIOS_FMPI2C_DIGITAL_FILTER,
                           ADIOS_FMPI2C0_POLLED);

#if !ADIOS_FMPI2C0_POLLED
    ADIOS_IRQ_Install(ADIOS_FMPI2C0_IRQ_EV_CHANNEL, ADIOS_IRQ_I2C_EV_PRIORITY);
    ADIOS_IRQ_Install(ADIOS_FMPI2C0_IRQ_ER_CHANNEL, ADIOS_IRQ_I2C_ER_PRIORITY);
#endif
  }
#endif

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Clocks a wedged bus clear again.
//!
//! A slave abandoned mid-transfer keeps driving SDA (it is still shifting
//! out the byte it was asked for) and the bus is dead until it sees enough
//! clocks to finish - power cycling was the only cure. The standard remedy:
//! up to nine SCL pulses by hand, then a STOP, then the peripheral's state
//! machine reset through PE.
//!
//! HOW TO USE IT
//!   1. call it when a transfer chain fails persistently (NACK or timeout
//!      on every retry), with the port taken via ADIOS_I2C_TransferBegin()
//!   2. retry the transfer ONCE afterwards - a second failure means the
//!      problem is not a wedged slave
//!
//! \param[in] i2c_port the port (0..2, or ADIOS_I2C_PORT_FMPI2C0)
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_I2C_BusClear(u8 i2c_port)
{
  GPIO_TypeDef *scl_port, *sda_port;
  u32 scl_pin, sda_pin;

#ifdef ADIOS_USE_FMPI2C0
  if( i2c_port == ADIOS_I2C_PORT_FMPI2C0 ) {
    scl_port = ADIOS_FMPI2C0_SCL_PORT; scl_pin = ADIOS_FMPI2C0_SCL_PIN;
    sda_port = ADIOS_FMPI2C0_SDA_PORT; sda_pin = ADIOS_FMPI2C0_SDA_PIN;
  } else
#endif
  switch( i2c_port ) {
#ifdef ADIOS_USE_I2C0
    case 0:
      scl_port = ADIOS_I2C0_SCL_PORT; scl_pin = ADIOS_I2C0_SCL_PIN;
      sda_port = ADIOS_I2C0_SDA_PORT; sda_pin = ADIOS_I2C0_SDA_PIN;
      break;
#endif
#ifdef ADIOS_USE_I2C1
    case 1:
      scl_port = ADIOS_I2C1_SCL_PORT; scl_pin = ADIOS_I2C1_SCL_PIN;
      sda_port = ADIOS_I2C1_SDA_PORT; sda_pin = ADIOS_I2C1_SDA_PIN;
      break;
#endif
#ifdef ADIOS_USE_I2C2
    case 2:
      scl_port = ADIOS_I2C2_SCL_PORT; scl_pin = ADIOS_I2C2_SCL_PIN;
      sda_port = ADIOS_I2C2_SDA_PORT; sda_pin = ADIOS_I2C2_SDA_PIN;
      break;
#endif
    default:
      return ADIOS_I2C_ERROR_INVALID_PORT;
  }

  // take SCL over as a plain output - it keeps its open-drain and pull-up
  // from the init, so this only changes WHO drives it. ODR high first, or
  // the mode switch itself would glitch the line low.
  LL_GPIO_SetOutputPin(scl_port, scl_pin);
  LL_GPIO_SetPinMode(scl_port, scl_pin, LL_GPIO_MODE_OUTPUT);

  // nine clocks at ~100 kHz, or fewer if SDA lets go before that
  for(int i=0; i<9; ++i) {
    LL_GPIO_ResetOutputPin(scl_port, scl_pin);
    ADIOS_DELAY_Wait_uS(5);
    LL_GPIO_SetOutputPin(scl_port, scl_pin);
    ADIOS_DELAY_Wait_uS(5);
    if( LL_GPIO_IsInputPinSet(sda_port, sda_pin) )
      break;
  }

  // a STOP by hand: SDA low, then released while SCL is high
  LL_GPIO_SetOutputPin(sda_port, sda_pin);
  LL_GPIO_SetPinMode(sda_port, sda_pin, LL_GPIO_MODE_OUTPUT);
  LL_GPIO_ResetOutputPin(sda_port, sda_pin);
  ADIOS_DELAY_Wait_uS(5);
  LL_GPIO_SetOutputPin(sda_port, sda_pin);
  ADIOS_DELAY_Wait_uS(5);

  // both lines back to the peripheral
  LL_GPIO_SetPinMode(scl_port, scl_pin, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetPinMode(sda_port, sda_pin, LL_GPIO_MODE_ALTERNATE);

  // and ITS state machine reset too - PE low resets it and releases the
  // lines, the configuration registers are kept
#ifdef ADIOS_USE_FMPI2C0
  if( i2c_port == ADIOS_I2C_PORT_FMPI2C0 ) {
    LL_FMPI2C_Disable(ADIOS_FMPI2C0_PTR);
    (void)LL_FMPI2C_IsEnabled(ADIOS_FMPI2C0_PTR);  // PE low a few APB cycles
    LL_FMPI2C_Enable(ADIOS_FMPI2C0_PTR);
    return 0;
  }
#endif
  {
    I2C_TypeDef *base = i2c_v1_rec[i2c_port]->base;
    LL_I2C_Disable(base);
    (void)LL_I2C_IsEnabled(base);                  // PE low a few APB cycles
    LL_I2C_Enable(base);
  }

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//! Takes the port, so that two tasks cannot interleave transfers on the
//! same bus.
//! \param[in] i2c_port the port (0..2, or ADIOS_I2C_PORT_FMPI2C0)
//! \param[in] semaphore_type ADIOS_I2C_BLOCKING or ADIOS_I2C_NON_BLOCKING
//! \return 0 on success, -1 if the port is taken (non-blocking request)
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_I2C_TransferBegin(u8 i2c_port, adios_i2c_semaphore_t semaphore_type)
{
  volatile u8 *semaphore;
  s32 status = -1;

#ifdef ADIOS_USE_FMPI2C0
  if( i2c_port == ADIOS_I2C_PORT_FMPI2C0 )
    semaphore = &fmpi2c0_rec.semaphore;
  else
#endif
  {
    if( i2c_port >= ADIOS_I2C_PORT_NUM || i2c_v1_rec[i2c_port] == NULL )
      return ADIOS_I2C_ERROR_INVALID_PORT;
    semaphore = &i2c_v1_rec[i2c_port]->semaphore;
  }

  do {
    ADIOS_IRQ_Disable();
    if( !*semaphore ) {
      *semaphore = 1;
      status = 0;
    }
    ADIOS_IRQ_Enable();
  } while( status < 0 && semaphore_type == ADIOS_I2C_BLOCKING );

  return status;
}


/////////////////////////////////////////////////////////////////////////////
//! Releases the port taken with ADIOS_I2C_TransferBegin().
//! \param[in] i2c_port the port (0..2, or ADIOS_I2C_PORT_FMPI2C0)
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_I2C_TransferFinished(u8 i2c_port)
{
#ifdef ADIOS_USE_FMPI2C0
  if( i2c_port == ADIOS_I2C_PORT_FMPI2C0 ) {
    fmpi2c0_rec.semaphore = 0;
    return 0;
  }
#endif

  if( i2c_port >= ADIOS_I2C_PORT_NUM || i2c_v1_rec[i2c_port] == NULL )
    return ADIOS_I2C_ERROR_INVALID_PORT;

  i2c_v1_rec[i2c_port]->semaphore = 0;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Starts a transfer.
//! \param[in] i2c_port the port (0..2, or ADIOS_I2C_PORT_FMPI2C0)
//! \param[in] transfer ADIOS_I2C_READ, _WRITE or _WRITE_WITHOUT_STOP
//! \param[in] address the slave address in 8-bit form (bit 0 ignored)
//! \param[in] buffer where the bytes come from or go to
//! \param[in] len how many
//! \return 0 on success, < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_I2C_Transfer(u8 i2c_port, adios_i2c_transfer_t transfer, u8 address, u8 *buffer, u16 len)
{
#ifdef ADIOS_USE_FMPI2C0
  if( i2c_port == ADIOS_I2C_PORT_FMPI2C0 )
    return ADIOS_I2C_V2_TransferStart(&fmpi2c0_rec, transfer, address, buffer, len);
#endif

  if( i2c_port >= ADIOS_I2C_PORT_NUM || i2c_v1_rec[i2c_port] == NULL )
    return ADIOS_I2C_ERROR_INVALID_PORT;

  return ADIOS_I2C_V1_TransferStart(i2c_v1_rec[i2c_port], transfer, address, buffer, len);
}


/////////////////////////////////////////////////////////////////////////////
//! Checks on a running transfer without waiting.
//! \param[in] i2c_port the port (0..2, or ADIOS_I2C_PORT_FMPI2C0)
//! \return 0 finished, 1 still running, < 0 the error it failed with
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_I2C_TransferCheck(u8 i2c_port)
{
#ifdef ADIOS_USE_FMPI2C0
  if( i2c_port == ADIOS_I2C_PORT_FMPI2C0 ) {
    if( fmpi2c0_rec.polled && fmpi2c0_rec.busy )
      ADIOS_I2C_V2_Step(&fmpi2c0_rec);
    return fmpi2c0_rec.busy ? 1 : fmpi2c0_rec.error;
  }
#endif

  if( i2c_port >= ADIOS_I2C_PORT_NUM || i2c_v1_rec[i2c_port] == NULL )
    return ADIOS_I2C_ERROR_INVALID_PORT;

  {
    adios_i2c_v1_rec_t *rec = i2c_v1_rec[i2c_port];

    if( rec->polled && rec->busy )
      ADIOS_I2C_V1_Step(rec);

    return rec->busy ? 1 : rec->error;
  }
}


/////////////////////////////////////////////////////////////////////////////
//! Waits for the running transfer to finish.
//! \param[in] i2c_port the port (0..2, or ADIOS_I2C_PORT_FMPI2C0)
//! \return 0 on success, < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_I2C_TransferWait(u8 i2c_port)
{
#ifdef ADIOS_USE_FMPI2C0
  if( i2c_port == ADIOS_I2C_PORT_FMPI2C0 )
    return ADIOS_I2C_V2_TransferWait(&fmpi2c0_rec);
#endif

  if( i2c_port >= ADIOS_I2C_PORT_NUM || i2c_v1_rec[i2c_port] == NULL )
    return ADIOS_I2C_ERROR_INVALID_PORT;

  return ADIOS_I2C_V1_TransferWait(i2c_v1_rec[i2c_port]);
}


/////////////////////////////////////////////////////////////////////////////
//! \param[in] i2c_port the port (0..2, or ADIOS_I2C_PORT_FMPI2C0)
//! \return the error the last transfer ended with, 0 if it went well
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_I2C_LastErrorGet(u8 i2c_port)
{
#ifdef ADIOS_USE_FMPI2C0
  if( i2c_port == ADIOS_I2C_PORT_FMPI2C0 )
    return fmpi2c0_rec.error;
#endif

  if( i2c_port >= ADIOS_I2C_PORT_NUM || i2c_v1_rec[i2c_port] == NULL )
    return ADIOS_I2C_ERROR_INVALID_PORT;

  return i2c_v1_rec[i2c_port]->error;
}


/////////////////////////////////////////////////////////////////////////////
// Interrupt handlers
//
// v1 splits events and errors across two vectors; both land in the same
// step function, which looks at the error flags first anyway.
/////////////////////////////////////////////////////////////////////////////

#if defined(ADIOS_USE_I2C0) && !ADIOS_I2C0_POLLED
ADIOS_I2C0_IRQHANDLER_EV_FUNC { ADIOS_I2C_V1_Step(&i2c0_rec); }
ADIOS_I2C0_IRQHANDLER_ER_FUNC { ADIOS_I2C_V1_Step(&i2c0_rec); }
#endif

#if defined(ADIOS_USE_I2C1) && !ADIOS_I2C1_POLLED
ADIOS_I2C1_IRQHANDLER_EV_FUNC { ADIOS_I2C_V1_Step(&i2c1_rec); }
ADIOS_I2C1_IRQHANDLER_ER_FUNC { ADIOS_I2C_V1_Step(&i2c1_rec); }
#endif

#if defined(ADIOS_USE_I2C2) && !ADIOS_I2C2_POLLED
ADIOS_I2C2_IRQHANDLER_EV_FUNC { ADIOS_I2C_V1_Step(&i2c2_rec); }
ADIOS_I2C2_IRQHANDLER_ER_FUNC { ADIOS_I2C_V1_Step(&i2c2_rec); }
#endif

#if defined(ADIOS_USE_FMPI2C0) && !ADIOS_FMPI2C0_POLLED
ADIOS_FMPI2C0_IRQHANDLER_EV_FUNC { ADIOS_I2C_V2_Step(&fmpi2c0_rec); }
ADIOS_FMPI2C0_IRQHANDLER_ER_FUNC { ADIOS_I2C_V2_Step(&fmpi2c0_rec); }
#endif

//! \}

#endif /* ADIOS_USE_I2C */
