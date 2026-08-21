//! \defgroup ADIOS_ADC
//!
//! ADC driver for ADIOS - STM32G0xx
//!
//! See include/adios/adios_adc.h for the API and its settings; this file
//! is about the silicon.
//!
//!
//! THE G0 HAS EXACTLY ONE ADC, ON EVERY LINE
//! =========================================
//!
//! ADC1, 12-bit, on all twelve G0 devices in the tree (G030, G031, G041,
//! G050, G051, G061, G070, G071, G081, G0B0, G0B1, G0C1) - verified in their
//! CMSIS headers, not assumed. There is no ADC2 and no ADC3 anywhere in the
//! family, so ADIOS_USE_ADC1 / ADIOS_USE_ADC2 are refused below with an
//! #error rather than silently ignored.
//!
//!
//! CHANNEL MAP - AND THE HOLE IN THE MIDDLE OF IT
//! ==============================================
//!
//! The G0 offers 16 external inputs, but it does NOT number them 0..15: it
//! puts its three internal measurements in the middle of the range.
//!
//!     0..7   PA0..PA7
//!     8      PB0        9   PB1        10  PB2
//!     11     PB10  (*)
//!     12     TEMPERATURE SENSOR   - internal, not a pin
//!     13     VREFINT              - internal, not a pin
//!     14     VBAT                 - internal, not a pin
//!     15     PB11  (*)  16  PB12  (*)  17  PC4  (*)  18  PC5  (*)
//!
//! So a channel mask copied from an F4 project, or a loop written as
//! "for(c=0; c<16; c++)", reads the inside of the chip on three of its
//! positions and never notices. This is the single most likely mistake with
//! this driver, which is why the mask is checked against the internal
//! channels at compile time further down.
//!
//! (*) PACKAGE-DEPENDENT. Channels 0..10 sit on the same pins on every G0
//! package. Channels 11 and 15..18 do not: ST rebonds them on the small
//! packages. The defaults below are the LQFP48-and-larger mapping, which
//! covers the G070CB and the G0B1. For the LQFP32 (a G030K6, for instance)
//! the values are:
//!
//!     #define ADIOS_ADC_CH11_PORT GPIOB       // PB7,  not PB10
//!     #define ADIOS_ADC_CH11_PIN  LL_GPIO_PIN_7
//!     #define ADIOS_ADC_CH15_PORT GPIOA       // PA11, not PB11
//!     #define ADIOS_ADC_CH15_PIN  LL_GPIO_PIN_11
//!     #define ADIOS_ADC_CH16_PORT GPIOA       // PA12, not PB12
//!     #define ADIOS_ADC_CH16_PIN  LL_GPIO_PIN_12
//!
//! and channels 17 and 18 land on PA13 and PA14 - that is SWDIO and SWCLK.
//! They are real channels, and using them costs the debug port. That is a
//! decision, not a default, so this driver does not make it for you: there
//! is no default for 17 and 18 on a 32-pin package.
//!
//!
//! OVERSAMPLING IS FREE HERE
//! =========================
//!
//! The G0 has a hardware oversampler (ratio 2..256 with its own right shift);
//! the F4 has none. Both families honour ADIOS_ADC_OVERSAMPLING_RATE and
//! produce the same number, but this one does it in the peripheral instead of
//! accumulating in the DMA interrupt. The shift is left at zero so that the
//! published value is the plain sum, identical to the F4's - which is why the
//! rate is capped at 16 in adios_adc.h, above that the sum leaves 16 bits.
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
#if defined(ADIOS_USE_ADC)


/////////////////////////////////////////////////////////////////////////////
// Tier check - which ADC instances this family actually has
/////////////////////////////////////////////////////////////////////////////

// The #else after these closes only at the end of the file: #error does not
// stop GCC, it records a message and carries on, so without it the real
// diagnostic would be buried under the secondary errors of a body that
// cannot compile anyway.
#if defined(ADIOS_USE_ADC1)
# error "ADIOS_USE_ADC1 (ADC2) requested, but no STM32G0 has a second ADC. Only ADIOS_USE_ADC0 (ADC1) exists on this family."
#elif defined(ADIOS_USE_ADC2)
# error "ADIOS_USE_ADC2 (ADC3) requested, but no STM32G0 has a third ADC. Only ADIOS_USE_ADC0 (ADC1) exists on this family."
#elif !defined(ADIOS_USE_ADC0)
# error "ADIOS_USE_ADC is set, but no ADC port was selected. Define ADIOS_USE_ADC0 for ADC1."
#else

#if !ADIOS_ADC0_CHANNEL_MASK
# warning "ADIOS_USE_ADC0 is set but ADIOS_ADC0_CHANNEL_MASK is 0: the ADC will be initialised and convert nothing."
#endif

// The three internal measurements sit at 12, 13 and 14. Selecting them
// through the channel mask would convert the temperature sensor instead of
// the pin the author had in mind.
#if ADIOS_ADC0_CHANNEL_MASK & (1 << 12)
# error "ADIOS_ADC0_CHANNEL_MASK selects channel 12, which is the internal temperature sensor on STM32G0, not a pin. External inputs are channels 0..11 and 15..18."
#endif
#if ADIOS_ADC0_CHANNEL_MASK & (1 << 13)
# error "ADIOS_ADC0_CHANNEL_MASK selects channel 13, which is the internal VREFINT on STM32G0, not a pin. External inputs are channels 0..11 and 15..18."
#endif
#if ADIOS_ADC0_CHANNEL_MASK & (1 << 14)
# error "ADIOS_ADC0_CHANNEL_MASK selects channel 14, which is the internal VBAT on STM32G0, not a pin. External inputs are channels 0..11 and 15..18."
#endif
#if ADIOS_ADC0_CHANNEL_MASK & ~0x0007ffff
# error "ADIOS_ADC0_CHANNEL_MASK selects a channel above 18. The STM32G0 ADC stops at channel 18."
#endif


/////////////////////////////////////////////////////////////////////////////
// Overridable settings
/////////////////////////////////////////////////////////////////////////////

// ADC kernel clock. ASYNC_DIV4 off the dedicated 48 MHz-capable ADC clock is
// a safe default at any core frequency; SYNC_PCLK_DIV2 or DIV4 also work and
// avoid the asynchronous domain entirely.
#ifndef ADIOS_ADC_CLOCK_SOURCE
#define ADIOS_ADC_CLOCK_SOURCE LL_ADC_CLOCK_ASYNC_DIV4
#endif

// Sampling time, shared by all channels on the G0's "common" sampling timer 1.
// 79.5 cycles suits a pot through a few kilo-ohms; lower it for a low
// impedance source, raise it for a high one.
#ifndef ADIOS_ADC_SAMPLINGTIME
#define ADIOS_ADC_SAMPLINGTIME LL_ADC_SAMPLINGTIME_79CYCLES_5
#endif

// DMA channel carrying the results. Any DMA1 channel works: the G0 routes the
// request through DMAMUX, so there is no fixed channel/peripheral pairing to
// respect - unlike the F4, where each ADC is wired to specific streams.
#ifndef ADIOS_ADC_DMA_CHANNEL
#define ADIOS_ADC_DMA_CHANNEL LL_DMA_CHANNEL_1
#endif
#ifndef ADIOS_ADC_DMA_IRQn
#define ADIOS_ADC_DMA_IRQn DMA1_Channel1_IRQn
#endif
#ifndef ADIOS_ADC_DMA_IRQHANDLER_FUNC
#define ADIOS_ADC_DMA_IRQHANDLER_FUNC void DMA1_Channel1_IRQHandler(void)
#endif
#ifndef ADIOS_ADC_DMA_CLEAR_FLAGS
#define ADIOS_ADC_DMA_CLEAR_FLAGS() do { LL_DMA_ClearFlag_TC1(DMA1); LL_DMA_ClearFlag_TE1(DMA1); LL_DMA_ClearFlag_HT1(DMA1); } while(0)
#endif

// Analog pins per channel. Channels 0..10 are the same on every package;
// 11 and 15..18 move - see the header comment for the LQFP32 values.
#ifndef ADIOS_ADC_CH0_PORT
#define ADIOS_ADC_CH0_PORT  GPIOA
#define ADIOS_ADC_CH0_PIN   LL_GPIO_PIN_0
#endif
#ifndef ADIOS_ADC_CH1_PORT
#define ADIOS_ADC_CH1_PORT  GPIOA
#define ADIOS_ADC_CH1_PIN   LL_GPIO_PIN_1
#endif
#ifndef ADIOS_ADC_CH2_PORT
#define ADIOS_ADC_CH2_PORT  GPIOA
#define ADIOS_ADC_CH2_PIN   LL_GPIO_PIN_2
#endif
#ifndef ADIOS_ADC_CH3_PORT
#define ADIOS_ADC_CH3_PORT  GPIOA
#define ADIOS_ADC_CH3_PIN   LL_GPIO_PIN_3
#endif
#ifndef ADIOS_ADC_CH4_PORT
#define ADIOS_ADC_CH4_PORT  GPIOA
#define ADIOS_ADC_CH4_PIN   LL_GPIO_PIN_4
#endif
#ifndef ADIOS_ADC_CH5_PORT
#define ADIOS_ADC_CH5_PORT  GPIOA
#define ADIOS_ADC_CH5_PIN   LL_GPIO_PIN_5
#endif
#ifndef ADIOS_ADC_CH6_PORT
#define ADIOS_ADC_CH6_PORT  GPIOA
#define ADIOS_ADC_CH6_PIN   LL_GPIO_PIN_6
#endif
#ifndef ADIOS_ADC_CH7_PORT
#define ADIOS_ADC_CH7_PORT  GPIOA
#define ADIOS_ADC_CH7_PIN   LL_GPIO_PIN_7
#endif
#ifndef ADIOS_ADC_CH8_PORT
#define ADIOS_ADC_CH8_PORT  GPIOB
#define ADIOS_ADC_CH8_PIN   LL_GPIO_PIN_0
#endif
#ifndef ADIOS_ADC_CH9_PORT
#define ADIOS_ADC_CH9_PORT  GPIOB
#define ADIOS_ADC_CH9_PIN   LL_GPIO_PIN_1
#endif
#ifndef ADIOS_ADC_CH10_PORT
#define ADIOS_ADC_CH10_PORT GPIOB
#define ADIOS_ADC_CH10_PIN  LL_GPIO_PIN_2
#endif
#ifndef ADIOS_ADC_CH11_PORT
#define ADIOS_ADC_CH11_PORT GPIOB
#define ADIOS_ADC_CH11_PIN  LL_GPIO_PIN_10
#endif
#ifndef ADIOS_ADC_CH15_PORT
#define ADIOS_ADC_CH15_PORT GPIOB
#define ADIOS_ADC_CH15_PIN  LL_GPIO_PIN_11
#endif
#ifndef ADIOS_ADC_CH16_PORT
#define ADIOS_ADC_CH16_PORT GPIOB
#define ADIOS_ADC_CH16_PIN  LL_GPIO_PIN_12
#endif
#ifndef ADIOS_ADC_CH17_PORT
#define ADIOS_ADC_CH17_PORT GPIOC
#define ADIOS_ADC_CH17_PIN  LL_GPIO_PIN_4
#endif
#ifndef ADIOS_ADC_CH18_PORT
#define ADIOS_ADC_CH18_PORT GPIOC
#define ADIOS_ADC_CH18_PIN  LL_GPIO_PIN_5
#endif


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

#define NUM_CHANNELS_MAX 19

// how many of the mask's bits are set - computed at compile time so the
// buffers are exactly the right size and nothing is wasted, the same way the
// UART buffers were reworked on 2026-08-06
#define CHN_USED(c) ((ADIOS_ADC0_CHANNEL_MASK >> (c)) & 1)
#define NUM_USED_CHANNELS ( \
  CHN_USED(0)  + CHN_USED(1)  + CHN_USED(2)  + CHN_USED(3)  + \
  CHN_USED(4)  + CHN_USED(5)  + CHN_USED(6)  + CHN_USED(7)  + \
  CHN_USED(8)  + CHN_USED(9)  + CHN_USED(10) + CHN_USED(11) + \
  CHN_USED(15) + CHN_USED(16) + CHN_USED(17) + CHN_USED(18) )


/////////////////////////////////////////////////////////////////////////////
// Local types
/////////////////////////////////////////////////////////////////////////////

typedef struct {
  GPIO_TypeDef *port;
  u16           pin_mask;
} adc_pin_t;


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

#if ADIOS_ADC0_CHANNEL_MASK

// raw DMA landing zone, one entry per enabled channel, in ascending channel
// order - which is exactly the order the G0's fixed sequencer converts them
static u16 adc_conversion_values[NUM_USED_CHANNELS] __attribute__((aligned(4)));

// last published value per enabled channel
static u16 adc_channel_values[NUM_USED_CHANNELS];

// one bit per enabled channel, set by the DMA interrupt, consumed by Handler()
static u32 adc_channel_changed[(NUM_USED_CHANNELS + 31) / 32];

#if ADIOS_ADC_DEADBAND_IDLE
static u16 adc_channel_idle_ctr[NUM_USED_CHANNELS];
#endif

static u16 adc_deadband;

static s32 (*service_prepare_callback)(void);

// maps a slot in the arrays above back to its hardware channel number
static const u8 slot_to_chn[NUM_USED_CHANNELS] = {
#if CHN_USED(0)
  0,
#endif
#if CHN_USED(1)
  1,
#endif
#if CHN_USED(2)
  2,
#endif
#if CHN_USED(3)
  3,
#endif
#if CHN_USED(4)
  4,
#endif
#if CHN_USED(5)
  5,
#endif
#if CHN_USED(6)
  6,
#endif
#if CHN_USED(7)
  7,
#endif
#if CHN_USED(8)
  8,
#endif
#if CHN_USED(9)
  9,
#endif
#if CHN_USED(10)
  10,
#endif
#if CHN_USED(11)
  11,
#endif
#if CHN_USED(15)
  15,
#endif
#if CHN_USED(16)
  16,
#endif
#if CHN_USED(17)
  17,
#endif
#if CHN_USED(18)
  18,
#endif
};

static const adc_pin_t chn_pin[NUM_USED_CHANNELS] = {
#if CHN_USED(0)
  { ADIOS_ADC_CH0_PORT,  ADIOS_ADC_CH0_PIN  },
#endif
#if CHN_USED(1)
  { ADIOS_ADC_CH1_PORT,  ADIOS_ADC_CH1_PIN  },
#endif
#if CHN_USED(2)
  { ADIOS_ADC_CH2_PORT,  ADIOS_ADC_CH2_PIN  },
#endif
#if CHN_USED(3)
  { ADIOS_ADC_CH3_PORT,  ADIOS_ADC_CH3_PIN  },
#endif
#if CHN_USED(4)
  { ADIOS_ADC_CH4_PORT,  ADIOS_ADC_CH4_PIN  },
#endif
#if CHN_USED(5)
  { ADIOS_ADC_CH5_PORT,  ADIOS_ADC_CH5_PIN  },
#endif
#if CHN_USED(6)
  { ADIOS_ADC_CH6_PORT,  ADIOS_ADC_CH6_PIN  },
#endif
#if CHN_USED(7)
  { ADIOS_ADC_CH7_PORT,  ADIOS_ADC_CH7_PIN  },
#endif
#if CHN_USED(8)
  { ADIOS_ADC_CH8_PORT,  ADIOS_ADC_CH8_PIN  },
#endif
#if CHN_USED(9)
  { ADIOS_ADC_CH9_PORT,  ADIOS_ADC_CH9_PIN  },
#endif
#if CHN_USED(10)
  { ADIOS_ADC_CH10_PORT, ADIOS_ADC_CH10_PIN },
#endif
#if CHN_USED(11)
  { ADIOS_ADC_CH11_PORT, ADIOS_ADC_CH11_PIN },
#endif
#if CHN_USED(15)
  { ADIOS_ADC_CH15_PORT, ADIOS_ADC_CH15_PIN },
#endif
#if CHN_USED(16)
  { ADIOS_ADC_CH16_PORT, ADIOS_ADC_CH16_PIN },
#endif
#if CHN_USED(17)
  { ADIOS_ADC_CH17_PORT, ADIOS_ADC_CH17_PIN },
#endif
#if CHN_USED(18)
  { ADIOS_ADC_CH18_PORT, ADIOS_ADC_CH18_PIN },
#endif
};

#endif /* ADIOS_ADC0_CHANNEL_MASK */


/////////////////////////////////////////////////////////////////////////////
//! Initializes the ADC driver
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_ADC_Init(u32 mode)
{
  if( mode != 0 )
    return -1; // unsupported mode

#if !ADIOS_ADC0_CHANNEL_MASK
  return -1; // no channel selected
#else
  int i;

  adc_deadband = ADIOS_ADC_DEADBAND;
  service_prepare_callback = NULL;

  for(i=0; i<NUM_USED_CHANNELS; ++i) {
    adc_conversion_values[i] = 0;
    adc_channel_values[i] = 0;
#if ADIOS_ADC_DEADBAND_IDLE
    adc_channel_idle_ctr[i] = 0;
#endif
  }
  for(i=0; i<(int)(sizeof(adc_channel_changed)/sizeof(u32)); ++i)
    adc_channel_changed[i] = 0;

  // analog pins
  LL_GPIO_InitTypeDef GPIO_InitStructure;
  LL_GPIO_StructInit(&GPIO_InitStructure);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
  for(i=0; i<NUM_USED_CHANNELS; ++i) {
    GPIO_InitStructure.Pin = chn_pin[i].pin_mask;
    LL_GPIO_Init(chn_pin[i].port, &GPIO_InitStructure);
  }

  // peripheral clocks
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

  // the ADC must be disabled to be configured, and its voltage regulator
  // needs its startup time before calibration can run
  if( LL_ADC_IsEnabled(ADC1) ) {
    LL_ADC_Disable(ADC1);
    while( LL_ADC_IsDisableOngoing(ADC1) );
  }

  LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(ADC1), ADIOS_ADC_CLOCK_SOURCE);
  LL_ADC_SetClock(ADC1, ADIOS_ADC_CLOCK_SOURCE);

  LL_ADC_EnableInternalRegulator(ADC1);
  ADIOS_DELAY_Wait_uS(LL_ADC_DELAY_INTERNAL_REGUL_STAB_US + 1);

  // calibration, mandatory on this peripheral and only possible while disabled
  LL_ADC_StartCalibration(ADC1);
  while( LL_ADC_IsCalibrationOnGoing(ADC1) );

  LL_ADC_SetResolution(ADC1, LL_ADC_RESOLUTION_12B);
  LL_ADC_SetDataAlignment(ADC1, LL_ADC_DATA_ALIGN_RIGHT);

  // Fixed sequencer: CHSELR is a plain bitmap and the ADC converts the
  // selected channels in ascending order. That is why the arrays above are
  // indexed by rank-in-ascending-order and slot_to_chn[] translates back.
  LL_ADC_REG_SetSequencerConfigurable(ADC1, LL_ADC_REG_SEQ_FIXED);
  while( LL_ADC_IsActiveFlag_CCRDY(ADC1) == 0 );
  LL_ADC_REG_SetSequencerChannels(ADC1, 0);
  while( LL_ADC_IsActiveFlag_CCRDY(ADC1) == 0 );
  for(i=0; i<NUM_USED_CHANNELS; ++i) {
    LL_ADC_REG_SetSequencerChAdd(ADC1, __LL_ADC_DECIMAL_NB_TO_CHANNEL(slot_to_chn[i]));
    while( LL_ADC_IsActiveFlag_CCRDY(ADC1) == 0 );
  }
  LL_ADC_REG_SetSequencerScanDirection(ADC1, LL_ADC_REG_SEQ_SCAN_DIR_FORWARD);

  // one common sampling time for every channel
  for(i=0; i<NUM_USED_CHANNELS; ++i)
    LL_ADC_SetChannelSamplingTime(ADC1,
                                  __LL_ADC_DECIMAL_NB_TO_CHANNEL(slot_to_chn[i]),
                                  LL_ADC_SAMPLINGTIME_COMMON_1);
  LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1, ADIOS_ADC_SAMPLINGTIME);

  // software trigger, single scan per request, results pushed to DMA
  LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_SOFTWARE);
  LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
  LL_ADC_REG_SetOverrun(ADC1, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
  LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_LIMITED);

#if ADIOS_ADC_OVERSAMPLING_RATE >= 2
  // hardware oversampling, shift left at zero so the published value is the
  // plain sum of ADIOS_ADC_OVERSAMPLING_RATE samples - the same number the
  // F4 arrives at by accumulating in its DMA interrupt
  LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_GRP_REGULAR_CONTINUED);
  LL_ADC_ConfigOverSamplingRatioShift(ADC1,
#if ADIOS_ADC_OVERSAMPLING_RATE == 2
                                      LL_ADC_OVS_RATIO_2,
#elif ADIOS_ADC_OVERSAMPLING_RATE == 4
                                      LL_ADC_OVS_RATIO_4,
#elif ADIOS_ADC_OVERSAMPLING_RATE == 8
                                      LL_ADC_OVS_RATIO_8,
#elif ADIOS_ADC_OVERSAMPLING_RATE == 16
                                      LL_ADC_OVS_RATIO_16,
#else
# error "ADIOS_ADC_OVERSAMPLING_RATE must be 1, 2, 4, 8 or 16 on STM32G0: the hardware oversampler only accepts powers of two."
#endif
                                      LL_ADC_OVS_SHIFT_NONE);
#else
  LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_DISABLE);
#endif

  // DMA: peripheral to memory, circular over one scan's worth of results
  LL_DMA_SetPeriphRequest(DMA1, ADIOS_ADC_DMA_CHANNEL, LL_DMAMUX_REQ_ADC1);
  LL_DMA_SetDataTransferDirection(DMA1, ADIOS_ADC_DMA_CHANNEL, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
  LL_DMA_SetChannelPriorityLevel(DMA1, ADIOS_ADC_DMA_CHANNEL, LL_DMA_PRIORITY_HIGH);
  LL_DMA_SetMode(DMA1, ADIOS_ADC_DMA_CHANNEL, LL_DMA_MODE_CIRCULAR);
  LL_DMA_SetPeriphIncMode(DMA1, ADIOS_ADC_DMA_CHANNEL, LL_DMA_PERIPH_NOINCREMENT);
  LL_DMA_SetMemoryIncMode(DMA1, ADIOS_ADC_DMA_CHANNEL, LL_DMA_MEMORY_INCREMENT);
  LL_DMA_SetPeriphSize(DMA1, ADIOS_ADC_DMA_CHANNEL, LL_DMA_PDATAALIGN_HALFWORD);
  LL_DMA_SetMemorySize(DMA1, ADIOS_ADC_DMA_CHANNEL, LL_DMA_MDATAALIGN_HALFWORD);
  LL_DMA_ConfigAddresses(DMA1, ADIOS_ADC_DMA_CHANNEL,
                         LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA),
                         (u32)adc_conversion_values,
                         LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
  LL_DMA_SetDataLength(DMA1, ADIOS_ADC_DMA_CHANNEL, NUM_USED_CHANNELS);
  LL_DMA_EnableIT_TC(DMA1, ADIOS_ADC_DMA_CHANNEL);
  LL_DMA_EnableChannel(DMA1, ADIOS_ADC_DMA_CHANNEL);

  ADIOS_IRQ_Install(ADIOS_ADC_DMA_IRQn, ADIOS_IRQ_ADC_DMA_PRIORITY);

  // enable the ADC and wait for it to report ready
  LL_ADC_Enable(ADC1);
  while( LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0 );

  ADIOS_ADC_StartConversions();

  return 0;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Installs an optional "service prepare" callback, run before each scan.
//! The scan starts if it returns 0, and is skipped if it returns >= 1 -
//! which is how a caller inserts a settling delay after switching an
//! external analog source.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_ADC_ServicePrepareCallback_Init(void *_service_prepare_callback)
{
#if !ADIOS_ADC0_CHANNEL_MASK
  return -1;
#else
  service_prepare_callback = _service_prepare_callback;
  return 0;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Returns the last published value of a channel, in HARDWARE channel
//! numbering - the same numbers used in ADIOS_ADC0_CHANNEL_MASK.
//! \param[in] port must be ADIOS_ADC_PORT_ADC0 on this family
//! \param[in] chn hardware channel number
//! \return < 0 if the port or channel is not enabled
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_ADC_ChannelGet(u8 port, u8 chn)
{
#if !ADIOS_ADC0_CHANNEL_MASK
  return -1;
#else
  int i;

  if( port != ADIOS_ADC_PORT_ADC0 )
    return -1; // this family has one ADC

  for(i=0; i<NUM_USED_CHANNELS; ++i)
    if( slot_to_chn[i] == chn )
      return adc_channel_values[i];

  return -1; // channel not enabled
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! \return the deadband currently used to notify changes
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_ADC_DeadbandGet(void)
{
#if !ADIOS_ADC0_CHANNEL_MASK
  return -1;
#else
  return adc_deadband;
#endif
}

/////////////////////////////////////////////////////////////////////////////
//! Sets the change a channel has to show before it is published.
//! \return < 0 on error
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_ADC_DeadbandSet(u16 deadband)
{
#if !ADIOS_ADC0_CHANNEL_MASK
  return -1;
#else
  adc_deadband = deadband;
  return 0;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Calls the given callback for each channel that moved, then starts the
//! next scan:
//! \code
//!   void APP_ADC_NotifyChange(u32 port, u32 chn, u32 value)
//! \endcode
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_ADC_Handler(void *_callback)
{
  if( _callback == NULL )
    return -1;

#if !ADIOS_ADC0_CHANNEL_MASK
  return -1;
#else
  int i;
  void (*callback)(u32 port, u32 chn, u32 value) = _callback;

  for(i=0; i<NUM_USED_CHANNELS; ++i) {
    u32 mask = 1 << (i & 0x1f);
    if( adc_channel_changed[i >> 5] & mask ) {
      ADIOS_IRQ_Disable();
      u32 value = adc_channel_values[i];
      adc_channel_changed[i >> 5] &= ~mask;
      ADIOS_IRQ_Enable();

      callback(ADIOS_ADC_PORT_ADC0, slot_to_chn[i], value);
    }
  }

  // optional settling hook; skip the scan if it asks for more time
  if( service_prepare_callback != NULL && service_prepare_callback() >= 1 )
    return 0;

  ADIOS_ADC_StartConversions();

  return 0;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Starts one scan of all enabled channels. Call this from a timer if the
//! periodic ADIOS_ADC_Handler() service has been disabled.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_ADC_StartConversions(void)
{
#if !ADIOS_ADC0_CHANNEL_MASK
  return -1;
#else
  // the DMA is in limited mode and stops after one scan: rearm it, then
  // trigger the sequence
  LL_DMA_DisableChannel(DMA1, ADIOS_ADC_DMA_CHANNEL);
  LL_DMA_SetDataLength(DMA1, ADIOS_ADC_DMA_CHANNEL, NUM_USED_CHANNELS);
  LL_DMA_EnableChannel(DMA1, ADIOS_ADC_DMA_CHANNEL);

  LL_ADC_REG_StartConversion(ADC1);
  return 0;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! DMA interrupt, raised once the whole scan has landed in memory.
//! Applies the deadband and flags what moved.
//! \note not to be called from an application
/////////////////////////////////////////////////////////////////////////////
#if ADIOS_ADC0_CHANNEL_MASK
ADIOS_ADC_DMA_IRQHANDLER_FUNC
{
  int i;

  ADIOS_ADC_DMA_CLEAR_FLAGS();

  for(i=0; i<NUM_USED_CHANNELS; ++i) {
    u16 value = adc_conversion_values[i];

#if ADIOS_ADC_DEADBAND_IDLE
    // a channel that has been still for a while gets the wider band, so EMI
    // cannot wake up a knob nobody is touching
    u16 deadband = adc_channel_idle_ctr[i] ? adc_deadband : ADIOS_ADC_DEADBAND_IDLE;
#else
    // the runtime deadband, not the compile-time constant: this branch must
    // still honour ADIOS_ADC_DeadbandSet()
    u16 deadband = adc_deadband;
#endif

    if( abs((s32)value - (s32)adc_channel_values[i]) > (s32)deadband ) {
      adc_channel_values[i] = value;
      adc_channel_changed[i >> 5] |= (1 << (i & 0x1f));
#if ADIOS_ADC_DEADBAND_IDLE
      adc_channel_idle_ctr[i] = ADIOS_ADC_IDLE_CTR;
#endif
    } else {
#if ADIOS_ADC_DEADBAND_IDLE
      if( adc_channel_idle_ctr[i] )
        adc_channel_idle_ctr[i] -= 1;
#endif
    }
  }
}
#endif

//! \}

#endif /* ADC port selection valid */

#endif /* ADIOS_USE_ADC */
