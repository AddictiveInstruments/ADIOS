/*
 * Shared transfer engine for the I2C v2 peripheral.
 *
 * NOT a translation unit: this file is #included by a family driver, after
 * that driver has defined the two macros below. It has no entry in
 * mios32.mk on purpose (the .inc extension keeps it out of the build's
 * source globs, same convention as the bootloader images).
 *
 * WHY IT IS SHARED
 * ----------------
 * ST built two generations of I2C silicon. v1 (STM32F1/F2/F4) is driven
 * through SR1/SR2/CCR/TRISE, with ACK and STOP posted by hand. v2 is driven
 * through ISR/ICR, the bus timing is one TIMINGR word, the byte count is
 * announced up front in NBYTES and the peripheral emits the STOP itself
 * (AUTOEND).
 *
 * The surprise is that v2 is not a G0 thing: the late STM32F4 parts
 * (F410/F412/F413/F423/F446) carry an EXTRA I2C block which is a v2, named
 * FMPI2C1 because v2 can reach Fast-mode Plus (1 MHz). Measured on the LL
 * headers shipped in drivers/: stm32f4xx_ll_i2c.h has 62 SR1, 30 CCR, 6
 * TRISE and zero TIMINGR; stm32f4xx_ll_fmpi2c.h and stm32g0xx_ll_i2c.h have
 * 52 ISR, 17 TIMINGR, 13 NBYTES and 15 AUTOEND - identical, figure for
 * figure, because they describe the same block.
 *
 * So the split that matters is v1 against v2, not F4 against G0. This file
 * is the v2 half, written once. The whole difference between the two users
 * is a name prefix: all 27 LL entry points this code needs exist on both
 * sides as LL_I2C_x and LL_FMPI2C_x. Hence a single token-pasting macro
 * rather than a table of aliases that could drift.
 *
 * WHAT THE INCLUDING FILE MUST DEFINE
 * -----------------------------------
 *   I2CV2(name)     token paste onto the family's LL prefix, used for both
 *                   functions and constants:
 *                     G0 : #define I2CV2(name) LL_I2C_##name
 *                     F4 : #define I2CV2(name) LL_FMPI2C_##name
 *   I2CV2_TypeDef   I2C_TypeDef or FMPI2C_TypeDef
 *
 * There is deliberately no #if MIOS32_PROCESSOR_* / MIOS32_BOARD_* in here:
 * this file never learns which chip it is on.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// TIMINGR
//
// In v2 the bus speed is not a divider but one 32-bit word packing PRESC,
// SCLDEL, SDADEL, SCLH and SCLL. It is computed here rather than tabulated,
// and that is a deliberate reversal: BOTH FILTERS AND THE KERNEL CLOCK
// SOURCE ENTER THE TIMING EQUATIONS. A table would silently become wrong
// the day someone overrides the analog filter, raises DNF, or switches the
// port from PCLK to HSI16 - and a bus that is 20% too fast does not fail
// cleanly, it fails as occasional NAKs on a long or loaded cable.
//
// Inputs are therefore the four things that actually determine the answer,
// and the kernel clock is read from RCC at init rather than assumed.
//
// Rounding is biased ONE WAY ON PURPOSE: every division rounds the SCL
// period UP, i.e. the bus comes out at or slightly below the requested
// frequency. Slower than asked always works; faster violates the spec.
//
// NOT VERIFIED ON HARDWARE (no I2C device on the bench as of 2026-08-13).
// Check the first bus you build at the scope. If you would rather trust
// ST's own tool, drop its value into MIOS32_I2Cn_TIMINGR and this function
// is skipped entirely.
/////////////////////////////////////////////////////////////////////////////

// Bus timing requirements, from the I2C specification. ns.
//                        t_SU;DAT  t_r(max)  t_f(max)
// standard mode  100k        250      1000       300
// fast mode      400k        100       300       300
// fast mode plus 1M           50       120       120

static u32 MIOS32_I2C_V2_TimingCalc(u32 i2cclk_hz, u32 scl_hz, u8 analog_filter_on, u8 dnf)
{
  u32 mhz = i2cclk_hz / 1000000;
  u32 t_su, t_r, t_f;
  u32 low_weight, high_weight;
  u32 period_ticks, af_ticks, sync_ticks;
  u32 presc, tp, scll = 0, sclh = 0, scldel, sdadel;

  if( !mhz || !scl_hz )
    return 0;

  if( scl_hz <= 100000 ) {
    t_su = 250; t_r = 1000; t_f = 300;
    low_weight = 1; high_weight = 1;          // symmetric in standard mode
  } else if( scl_hz <= 400000 ) {
    t_su = 100; t_r = 300; t_f = 300;
    low_weight = 2; high_weight = 1;          // t_LOW 1.3 uS vs t_HIGH 0.6 uS
  } else {
    t_su = 50; t_r = 120; t_f = 120;
    low_weight = 2; high_weight = 1;          // t_LOW 0.5 uS vs t_HIGH 0.26 uS
  }

  period_ticks = i2cclk_hz / scl_hz;

  // Delays the peripheral inserts before it even sees the line: two kernel
  // clocks of resynchronisation, the digital filter, and the analog filter's
  // own propagation (50 ns min per the datasheets). They shorten the half
  // periods the counters have to produce.
  af_ticks = analog_filter_on ? ((i2cclk_hz + 19999999) / 20000000) : 0;   // ceil(50ns)
  sync_ticks = 2 + dnf + af_ticks;

  // Smallest prescaler that keeps both counters inside their 8 bits.
  for(presc = 0; presc < 16; ++presc) {
    u32 low_ticks, high_ticks;

    tp = presc + 1;
    low_ticks  = (period_ticks * low_weight) / (low_weight + high_weight);
    high_ticks = period_ticks - low_ticks;

    low_ticks  = (low_ticks  > sync_ticks) ? (low_ticks  - sync_ticks) : 1;
    high_ticks = (high_ticks > sync_ticks) ? (high_ticks - sync_ticks) : 1;

    // round the counters UP: longer half period, slower bus, safe side
    scll = ((low_ticks  + tp - 1) / tp);
    sclh = ((high_ticks + tp - 1) / tp);
    scll = scll ? (scll - 1) : 0;
    sclh = sclh ? (sclh - 1) : 0;

    if( scll <= 255 && sclh <= 255 )
      break;
  }

  if( presc > 15 )
    presc = 15;
  tp = presc + 1;

  // SCLDEL - data setup: SDA must be stable t_SU;DAT before SCL rises, and
  // the rise itself has to be paid for.
  scldel = (((mhz * (t_r + t_su)) / 1000) + tp - 1) / tp;
  scldel = scldel ? (scldel - 1) : 0;
  if( scldel > 15 )
    scldel = 15;

  // SDADEL - data hold: SDA must not move until SCL has really fallen. The
  // fall time sets the floor; both filters already delay what we observe,
  // so their contribution is credited back.
  {
    u32 fall_ticks = (mhz * t_f) / 1000;
    u32 filtered = af_ticks + dnf + 3;
    sdadel = (fall_ticks > filtered) ? ((fall_ticks - filtered) + tp - 1) / tp : 0;
    if( sdadel > 15 )
      sdadel = 15;
  }

  return (presc  << 28) |
         (scldel << 20) |
         (sdadel << 16) |
         (sclh   <<  8) |
         (scll   <<  0);
}


/////////////////////////////////////////////////////////////////////////////
// Per-port state
/////////////////////////////////////////////////////////////////////////////

typedef struct {
  I2CV2_TypeDef *base;

  u8  *buffer;
  u16  len;                 // bytes still to move in the WHOLE transfer
  u16  ix;                  // index into buffer

  u8   address;             // 8-bit form, bit 0 ignored (RD_WRN carries it)
  u8   transfer_type;       // mios32_i2c_transfer_t
  u8   chunk;               // bytes left in the current NBYTES window

  u8   polled;              // 1: no interrupts, TransferWait() drives Step()
  volatile u8  busy;        // 1: transfer in progress
  volatile s32 error;       // 0 or a MIOS32_I2C_ERROR_* code
  volatile u8  semaphore;   // TransferBegin()/TransferFinished()
} mios32_i2c_v2_rec_t;


// NBYTES is 8 bits wide. Anything longer than this has to be handed to the
// peripheral in windows, chained through the RELOAD mechanism - see
// MIOS32_I2C_V2_ArmWindow() and the TCR branch of Step().
#define MIOS32_I2C_V2_MAX_NBYTES 255


/////////////////////////////////////////////////////////////////////////////
// Arms one NBYTES window.
//
// first == 1 emits a START and the address; first == 0 chains the next
// window of a RELOAD sequence without touching the bus.
//
// The end mode of a window is what tells the peripheral how to finish:
//   RELOAD  - more windows follow, raise TCR when this one is done
//   AUTOEND - last window, emit the STOP yourself
//   SOFTEND - last window, raise TC and HOLD the bus, so the caller can
//             issue a repeated START (register addressing)
/////////////////////////////////////////////////////////////////////////////
static void MIOS32_I2C_V2_ArmWindow(mios32_i2c_v2_rec_t *rec, u8 first)
{
  u16 remaining = rec->len - rec->ix;
  u32 end_mode;
  u32 request;

  rec->chunk = (remaining > MIOS32_I2C_V2_MAX_NBYTES) ? MIOS32_I2C_V2_MAX_NBYTES : (u8)remaining;

  if( remaining > MIOS32_I2C_V2_MAX_NBYTES )
    end_mode = I2CV2(MODE_RELOAD);
  else if( rec->transfer_type == MIOS32_I2C_WRITE_WITHOUT_STOP )
    end_mode = I2CV2(MODE_SOFTEND);
  else
    end_mode = I2CV2(MODE_AUTOEND);

  if( !first )
    request = I2CV2(GENERATE_NOSTARTSTOP);
  else if( rec->transfer_type == MIOS32_I2C_READ )
    request = I2CV2(GENERATE_START_READ);
  else
    request = I2CV2(GENERATE_START_WRITE);

  I2CV2(HandleTransfer)(rec->base, rec->address, I2CV2(ADDRSLAVE_7BIT),
                        rec->chunk, end_mode, request);
}


/////////////////////////////////////////////////////////////////////////////
// Ends a transfer: records the outcome, silences the interrupts, releases
// the caller waiting in TransferWait().
/////////////////////////////////////////////////////////////////////////////
static void MIOS32_I2C_V2_Finish(mios32_i2c_v2_rec_t *rec, s32 error)
{
  I2CV2(DisableIT_TX)(rec->base);
  I2CV2(DisableIT_RX)(rec->base);
  I2CV2(DisableIT_NACK)(rec->base);
  I2CV2(DisableIT_ERR)(rec->base);
  I2CV2(DisableIT_STOP)(rec->base);
  I2CV2(DisableIT_TC)(rec->base);

  rec->error = error;
  rec->busy = 0;
}


/////////////////////////////////////////////////////////////////////////////
// Advances the state machine by whatever the peripheral is asking for.
//
// Called from the port's ISR (the default) or from the wait loop when the
// port is declared polled - it is the same code either way, which is the
// point: there is one transfer engine here, not two.
/////////////////////////////////////////////////////////////////////////////
static void MIOS32_I2C_V2_Step(mios32_i2c_v2_rec_t *rec)
{
  I2CV2_TypeDef *base = rec->base;

  if( !rec->busy )
    return;

  // errors first - any of them ends the transfer

  if( I2CV2(IsActiveFlag_NACK)(base) ) {
    I2CV2(ClearFlag_NACK)(base);
    // a NACK on the address byte means nobody answered; a NACK in mid
    // transfer means the slave gave up. Both leave the bus to be released:
    // the peripheral emits the STOP itself in AUTOEND, otherwise do it here.
    if( rec->transfer_type == MIOS32_I2C_WRITE_WITHOUT_STOP )
      I2CV2(GenerateStopCondition)(base);
    MIOS32_I2C_V2_Finish(rec, MIOS32_I2C_ERROR_SLAVE_NOT_CONNECTED);
    return;
  }

  if( I2CV2(IsActiveFlag_ARLO)(base) ) {
    I2CV2(ClearFlag_ARLO)(base);
    MIOS32_I2C_V2_Finish(rec, MIOS32_I2C_ERROR_ARBITRATION_LOST);
    return;
  }

  if( I2CV2(IsActiveFlag_BERR)(base) ) {
    I2CV2(ClearFlag_BERR)(base);
    MIOS32_I2C_V2_Finish(rec, MIOS32_I2C_ERROR_BUS);
    return;
  }

  if( I2CV2(IsActiveFlag_OVR)(base) ) {
    I2CV2(ClearFlag_OVR)(base);
    MIOS32_I2C_V2_Finish(rec, MIOS32_I2C_ERROR_RX_BUFFER_OVERRUN);
    return;
  }

  // data movement

  if( I2CV2(IsActiveFlag_RXNE)(base) ) {
    if( rec->ix < rec->len )
      rec->buffer[rec->ix++] = I2CV2(ReceiveData8)(base);
    else
      (void)I2CV2(ReceiveData8)(base);   // drain, never leave RXNE standing
  }

  if( I2CV2(IsActiveFlag_TXIS)(base) ) {
    if( rec->ix < rec->len )
      I2CV2(TransmitData8)(base, rec->buffer[rec->ix++]);
    else
      I2CV2(TransmitData8)(base, 0xff);
  }

  // window boundaries

  if( I2CV2(IsActiveFlag_TCR)(base) ) {
    // NBYTES exhausted but the transfer continues: chain the next window.
    MIOS32_I2C_V2_ArmWindow(rec, 0);
  }

  if( I2CV2(IsActiveFlag_TC)(base) ) {
    // SOFTEND only: everything moved, bus deliberately still held for the
    // repeated START the caller is about to issue.
    MIOS32_I2C_V2_Finish(rec, 0);
    return;
  }

  if( I2CV2(IsActiveFlag_STOP)(base) ) {
    I2CV2(ClearFlag_STOP)(base);
    MIOS32_I2C_V2_Finish(rec, 0);
    return;
  }
}


/////////////////////////////////////////////////////////////////////////////
// Starts a transfer. Returns 0, or a negative error code if the port is
// busy or the request makes no sense.
/////////////////////////////////////////////////////////////////////////////
static s32 MIOS32_I2C_V2_TransferStart(mios32_i2c_v2_rec_t *rec,
                                       mios32_i2c_transfer_t transfer,
                                       u8 address, u8 *buffer, u16 len)
{
  if( rec->base == NULL )
    return MIOS32_I2C_ERROR_INVALID_PORT;

  if( transfer != MIOS32_I2C_READ &&
      transfer != MIOS32_I2C_WRITE &&
      transfer != MIOS32_I2C_WRITE_WITHOUT_STOP )
    return MIOS32_I2C_ERROR_UNSUPPORTED_TRANSFER_TYPE;

  // a failed previous transfer is reported once, offset, and then forgotten
  if( rec->error < 0 ) {
    s32 prev = rec->error;
    rec->error = 0;
    return prev + MIOS32_I2C_ERROR_PREV_OFFSET;
  }

  rec->buffer = buffer;
  rec->len = len;
  rec->ix = 0;
  rec->address = address;
  rec->transfer_type = (u8)transfer;
  rec->error = 0;
  rec->busy = 1;

  MIOS32_I2C_V2_ArmWindow(rec, 1);

  if( !rec->polled ) {
    I2CV2(EnableIT_NACK)(rec->base);
    I2CV2(EnableIT_ERR)(rec->base);
    I2CV2(EnableIT_STOP)(rec->base);
    I2CV2(EnableIT_TC)(rec->base);      // covers TC and TCR
    if( transfer == MIOS32_I2C_READ )
      I2CV2(EnableIT_RX)(rec->base);
    else
      I2CV2(EnableIT_TX)(rec->base);
  }

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// Waits for the running transfer to end. Drives the state machine itself on
// a polled port; on an interrupt-driven one it only watches, and the
// timeout is the guard against a bus that never answers.
/////////////////////////////////////////////////////////////////////////////
static s32 MIOS32_I2C_V2_TransferWait(mios32_i2c_v2_rec_t *rec)
{
  u32 timeout = MIOS32_I2C_TIMEOUT_VALUE;

  while( rec->busy && timeout-- ) {
    if( rec->polled )
      MIOS32_I2C_V2_Step(rec);
#ifndef MIOS32_DONT_USE_DELAY
    else
      MIOS32_DELAY_Wait_uS(1);       // makes the timeout count real microseconds
#endif
  }

  if( rec->busy ) {
    // nothing came back: drop the peripheral out of the transfer rather
    // than leave it half-armed for the next caller
    MIOS32_I2C_V2_Finish(rec, MIOS32_I2C_ERROR_TIMEOUT);
    return MIOS32_I2C_ERROR_TIMEOUT;
  }

  return rec->error;
}


/////////////////////////////////////////////////////////////////////////////
// Configures one port. Called once per enabled port from the family
// driver's MIOS32_I2C_Init(), with values the family driver resolved from
// its (fully overridable) port table.
//
// Order is not free: the filters and the timing can only be written while
// the peripheral is disabled (PE = 0).
/////////////////////////////////////////////////////////////////////////////
static void MIOS32_I2C_V2_PortInit(mios32_i2c_v2_rec_t *rec, I2CV2_TypeDef *base,
                                   u32 timing, u32 analog_filter, u32 digital_filter,
                                   u8 polled)
{
  rec->base = base;
  rec->buffer = NULL;
  rec->len = 0;
  rec->ix = 0;
  rec->busy = 0;
  rec->error = 0;
  rec->semaphore = 0;
  rec->polled = polled;

  I2CV2(Disable)(base);

  I2CV2(ConfigFilters)(base, analog_filter, digital_filter);
  I2CV2(SetTiming)(base, timing);
  I2CV2(SetMasterAddressingMode)(base, I2CV2(ADDRESSING_MODE_7BIT));

  // Clock stretching stays enabled: a master must let a slow slave hold SCL.
  // (NOSTRETCH is a slave-mode setting anyway - disabling it there is what
  // breaks a bus.)
  I2CV2(EnableClockStretching)(base);

  // Wakeup from Stop (CR1.WUPEN) is NOT implemented, on purpose. For the
  // record, should it ever be wanted: it wakes the CPU out of Stop when a
  // master addresses us, so it is a SLAVE feature; it forces HSI16 as the
  // peripheral's kernel clock, forces the digital filter to 0 (the filter
  // needs that clock running) and requires clock stretching. And it does not
  // exist everywhere: I2C1 only on G030/G031/G041/G050/G070, I2C1 and I2C2
  // on G0B1/G0C1, and on no F4 port at all - not even FMPI2C1.

  I2CV2(Enable)(base);
}
