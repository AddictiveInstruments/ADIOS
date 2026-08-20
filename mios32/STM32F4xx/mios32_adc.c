//! \defgroup MIOS32_ADC
//!
//! ADC driver for MIOS32 - STM32F4xx
//!
//! See include/mios32/mios32_adc.h for the API and its settings; this file
//! is about the silicon.
//!
//!
//! ONE OR THREE ADCs, DEPENDING ON THE LINE
//! ========================================
//!
//! Verified in the CMSIS headers of all 23 F4 devices in the tree, not
//! assumed:
//!
//!     ADC1 only ....... F401, F410, F411, F412, F413, F423
//!     ADC1+ADC2+ADC3 .. F405, F407, F415, F417, F427, F429, F437, F439,
//!                       F446, F469, F479
//!
//! The guards below are #if defined(ADC2) / defined(ADC3), never a list of
//! part numbers: the CMSIS header for the chip already knows, and a list
//! written by hand is a list that goes stale.
//!
//! Each instance runs INDEPENDENTLY here - its own channel mask, its own DMA
//! stream, its own scan. The dual and triple SIMULTANEOUS modes are NOT
//! implemented: they sample several ADCs on one trigger, which serves motor
//! control and costs the freedom to give each instance its own channel
//! count. Halving a scan time that way also ties the instances together in
//! the result buffer, where one DMA stream then carries interleaved pairs.
//!
//!
//! CHANNEL MAP
//! ===========
//!
//! ADC1 and ADC2 share one map, contiguous from 0 to 15 - unlike the G0,
//! whose internal measurements sit in the middle of the range:
//!
//!     0..7   PA0..PA7
//!     8, 9   PB0, PB1
//!     10..15 PC0..PC5
//!     16     temperature sensor (*)   17  VREFINT     18  VBAT (*)
//!
//! ADC3 has a map of its own, and half of it is on PORTF:
//!
//!     0..3   PA0..PA3
//!     4..8   PF6..PF10       9   PF3
//!     10..13 PC0..PC3
//!     14, 15 PF4, PF5
//!
//! PORTF only exists from LQFP144 up. On an LQFP100 or smaller, ADC3 is
//! real but only channels 0..3 and 10..13 can be reached. Nothing here
//! prevents selecting the others; the pins simply are not bonded.
//!
//! (*) The temperature sensor is channel 16 on F401, F405, F407, F410, F415
//! and F417, but channel 18 SHARED WITH VBAT on every other F4 - and the
//! reference manual is explicit that only one of the two measurement paths
//! may be enabled at a time. Both are internal, so neither is reachable
//! through the channel masks below, which are for pins.
//!
//!
//! NO HARDWARE OVERSAMPLING ON THIS FAMILY
//! =======================================
//!
//! The STM32G0 has an oversampler in the peripheral; this family has none -
//! searching the whole F4 LL for the pattern returns nothing.
//! MIOS32_ADC_OVERSAMPLING_RATE is therefore honoured by accumulating in the
//! DMA interrupt. Same knob, same number, more CPU.
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

// this module can be optionally disabled in a local mios32_config.h file (included from mios32.h)
#if defined(MIOS32_USE_ADC)


/////////////////////////////////////////////////////////////////////////////
// Tier check - which ADC instances this chip actually has
/////////////////////////////////////////////////////////////////////////////

// The #else after these closes only at the end of the file: #error does not
// stop GCC, it records a message and carries on, so without it the real
// diagnostic would be buried under the secondary errors of a body that
// cannot compile anyway.
#if !defined(MIOS32_USE_ADC0) && !defined(MIOS32_USE_ADC1) && !defined(MIOS32_USE_ADC2)
# error "MIOS32_USE_ADC is set, but no ADC port was selected. Define MIOS32_USE_ADC0 for ADC1, MIOS32_USE_ADC1 for ADC2, MIOS32_USE_ADC2 for ADC3."
#elif defined(MIOS32_USE_ADC1) && !defined(ADC2)
# error "MIOS32_USE_ADC1 (ADC2) requested, but this STM32F4 has only ADC1. A second ADC exists on F405/F407/F415/F417/F427/F429/F437/F439/F446/F469/F479 only."
#elif defined(MIOS32_USE_ADC2) && !defined(ADC3)
# error "MIOS32_USE_ADC2 (ADC3) requested, but this STM32F4 has only ADC1. A third ADC exists on F405/F407/F415/F417/F427/F429/F437/F439/F446/F469/F479 only."
#else

// Channels 16, 17 and 18 are internal on this family too, and are reached
// through their own LL constants rather than through a pin mask.
#if defined(MIOS32_USE_ADC0) && (MIOS32_ADC0_CHANNEL_MASK & ~0x0000ffff)
# error "MIOS32_ADC0_CHANNEL_MASK selects a channel above 15. On STM32F4 the external inputs are channels 0..15; 16, 17 and 18 are the temperature sensor, VREFINT and VBAT."
#endif
#if defined(MIOS32_USE_ADC1) && (MIOS32_ADC1_CHANNEL_MASK & ~0x0000ffff)
# error "MIOS32_ADC1_CHANNEL_MASK selects a channel above 15. On STM32F4 the external inputs are channels 0..15."
#endif
#if defined(MIOS32_USE_ADC2) && (MIOS32_ADC2_CHANNEL_MASK & ~0x0000ffff)
# error "MIOS32_ADC2_CHANNEL_MASK selects a channel above 15. On STM32F4 the external inputs are channels 0..15."
#endif

#if defined(MIOS32_USE_ADC0) && !MIOS32_ADC0_CHANNEL_MASK
# warning "MIOS32_USE_ADC0 is set but MIOS32_ADC0_CHANNEL_MASK is 0: ADC1 will be initialised and convert nothing."
#endif
#if defined(MIOS32_USE_ADC1) && !MIOS32_ADC1_CHANNEL_MASK
# warning "MIOS32_USE_ADC1 is set but MIOS32_ADC1_CHANNEL_MASK is 0: ADC2 will be initialised and convert nothing."
#endif
#if defined(MIOS32_USE_ADC2) && !MIOS32_ADC2_CHANNEL_MASK
# warning "MIOS32_USE_ADC2 is set but MIOS32_ADC2_CHANNEL_MASK is 0: ADC3 will be initialised and convert nothing."
#endif


/////////////////////////////////////////////////////////////////////////////
// Overridable settings
/////////////////////////////////////////////////////////////////////////////

// ADC kernel clock, common to all three instances (it lives in ADC_CCR).
// APB2 divided by 4 keeps the ADC below its 36 MHz maximum on a 168 MHz F407.
#ifndef MIOS32_ADC_CLOCK_SOURCE
#define MIOS32_ADC_CLOCK_SOURCE LL_ADC_CLOCK_SYNC_PCLK_DIV4
#endif

// Sampling time, applied to every selected channel. 144 cycles suits a pot
// through a few kilo-ohms; lower it for a low impedance source.
#ifndef MIOS32_ADC_SAMPLINGTIME
#define MIOS32_ADC_SAMPLINGTIME LL_ADC_SAMPLINGTIME_144CYCLES
#endif

// DMA stream per instance. Unlike the G0, where DMAMUX lets any channel carry
// any request, each F4 ADC is hard-wired to a small set of DMA2 streams:
//
//     ADC1 -> DMA2 Stream0 Ch0  or  Stream4 Ch0
//     ADC2 -> DMA2 Stream2 Ch1  or  Stream3 Ch1
//     ADC3 -> DMA2 Stream0 Ch2  or  Stream1 Ch2
//
// The defaults below take the first free choice for each, which is why ADC3
// is on Stream1 and not on Stream0: Stream0 is ADC1's.
#ifndef MIOS32_ADC0_DMA_STREAM
#define MIOS32_ADC0_DMA_STREAM   LL_DMA_STREAM_0
#define MIOS32_ADC0_DMA_CHANNEL  LL_DMA_CHANNEL_0
#define MIOS32_ADC0_DMA_IRQn     DMA2_Stream0_IRQn
#define MIOS32_ADC0_DMA_IRQ_FUNC void DMA2_Stream0_IRQHandler(void)
#define MIOS32_ADC0_DMA_CLEAR()  do { LL_DMA_ClearFlag_TC0(DMA2); LL_DMA_ClearFlag_TE0(DMA2); LL_DMA_ClearFlag_HT0(DMA2); LL_DMA_ClearFlag_DME0(DMA2); LL_DMA_ClearFlag_FE0(DMA2); } while(0)
#endif

#ifndef MIOS32_ADC1_DMA_STREAM
#define MIOS32_ADC1_DMA_STREAM   LL_DMA_STREAM_2
#define MIOS32_ADC1_DMA_CHANNEL  LL_DMA_CHANNEL_1
#define MIOS32_ADC1_DMA_IRQn     DMA2_Stream2_IRQn
#define MIOS32_ADC1_DMA_IRQ_FUNC void DMA2_Stream2_IRQHandler(void)
#define MIOS32_ADC1_DMA_CLEAR()  do { LL_DMA_ClearFlag_TC2(DMA2); LL_DMA_ClearFlag_TE2(DMA2); LL_DMA_ClearFlag_HT2(DMA2); LL_DMA_ClearFlag_DME2(DMA2); LL_DMA_ClearFlag_FE2(DMA2); } while(0)
#endif

#ifndef MIOS32_ADC2_DMA_STREAM
#define MIOS32_ADC2_DMA_STREAM   LL_DMA_STREAM_1
#define MIOS32_ADC2_DMA_CHANNEL  LL_DMA_CHANNEL_2
#define MIOS32_ADC2_DMA_IRQn     DMA2_Stream1_IRQn
#define MIOS32_ADC2_DMA_IRQ_FUNC void DMA2_Stream1_IRQHandler(void)
#define MIOS32_ADC2_DMA_CLEAR()  do { LL_DMA_ClearFlag_TC1(DMA2); LL_DMA_ClearFlag_TE1(DMA2); LL_DMA_ClearFlag_HT1(DMA2); LL_DMA_ClearFlag_DME1(DMA2); LL_DMA_ClearFlag_FE1(DMA2); } while(0)
#endif


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

#define NUM_CHANNELS_MAX 16

// how many of a mask's bits are set - computed at compile time so each
// instance's buffers are exactly the right size and nothing is wasted
#define POPCNT16(m) ( \
  (((m) >> 0) & 1) + (((m) >> 1) & 1) + (((m) >> 2) & 1) + (((m) >> 3) & 1) + \
  (((m) >> 4) & 1) + (((m) >> 5) & 1) + (((m) >> 6) & 1) + (((m) >> 7) & 1) + \
  (((m) >> 8) & 1) + (((m) >> 9) & 1) + (((m) >>10) & 1) + (((m) >>11) & 1) + \
  (((m) >>12) & 1) + (((m) >>13) & 1) + (((m) >>14) & 1) + (((m) >>15) & 1) )

#if defined(MIOS32_USE_ADC0)
# define NUM_CHN_0 POPCNT16(MIOS32_ADC0_CHANNEL_MASK)
#else
# define NUM_CHN_0 0
#endif
#if defined(MIOS32_USE_ADC1)
# define NUM_CHN_1 POPCNT16(MIOS32_ADC1_CHANNEL_MASK)
#else
# define NUM_CHN_1 0
#endif
#if defined(MIOS32_USE_ADC2)
# define NUM_CHN_2 POPCNT16(MIOS32_ADC2_CHANNEL_MASK)
#else
# define NUM_CHN_2 0
#endif

#define NUM_CHN_TOTAL (NUM_CHN_0 + NUM_CHN_1 + NUM_CHN_2)


/////////////////////////////////////////////////////////////////////////////
// Local types
/////////////////////////////////////////////////////////////////////////////

typedef struct {
  GPIO_TypeDef *port;
  u16           pin_mask;
} adc_pin_t;

typedef struct {
  ADC_TypeDef *adc;         // peripheral instance
  u32          dma_stream;
  u32          dma_channel;
  u16          chn_mask;    // which hardware channels are selected
  u8           num_chn;     // how many bits that is
  u8           slot_base;   // where this instance starts in the shared arrays
  u16         *raw;         // DMA landing zone for this instance
} adc_inst_t;


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

#if NUM_CHN_TOTAL

// The published state is one flat set of arrays covering every instance, in
// port order - slot_base says where each instance starts. The raw DMA
// landing zones stay separate, one per instance, because each DMA stream
// writes its own scan.
static u16 adc_channel_values[NUM_CHN_TOTAL];
static u32 adc_channel_changed[(NUM_CHN_TOTAL + 31) / 32];
#if MIOS32_ADC_OVERSAMPLING_RATE >= 2
static u16 adc_values_sum[NUM_CHN_TOTAL];
static u8  oversampling_ctr[3];
#endif
#if MIOS32_ADC_DEADBAND_IDLE
static u16 adc_channel_idle_ctr[NUM_CHN_TOTAL];
#endif

#if NUM_CHN_0
static u16 adc_raw_0[NUM_CHN_0] __attribute__((aligned(4)));
#endif
#if NUM_CHN_1
static u16 adc_raw_1[NUM_CHN_1] __attribute__((aligned(4)));
#endif
#if NUM_CHN_2
static u16 adc_raw_2[NUM_CHN_2] __attribute__((aligned(4)));
#endif

static u16 adc_deadband;
static s32 (*service_prepare_callback)(void);

static adc_inst_t adc_inst[3];

// ADC1 and ADC2 share this map; ADC3 does not - see the header comment.
static const adc_pin_t pin_map_12[NUM_CHANNELS_MAX] = {
  { GPIOA, LL_GPIO_PIN_0  }, { GPIOA, LL_GPIO_PIN_1  },
  { GPIOA, LL_GPIO_PIN_2  }, { GPIOA, LL_GPIO_PIN_3  },
  { GPIOA, LL_GPIO_PIN_4  }, { GPIOA, LL_GPIO_PIN_5  },
  { GPIOA, LL_GPIO_PIN_6  }, { GPIOA, LL_GPIO_PIN_7  },
  { GPIOB, LL_GPIO_PIN_0  }, { GPIOB, LL_GPIO_PIN_1  },
  { GPIOC, LL_GPIO_PIN_0  }, { GPIOC, LL_GPIO_PIN_1  },
  { GPIOC, LL_GPIO_PIN_2  }, { GPIOC, LL_GPIO_PIN_3  },
  { GPIOC, LL_GPIO_PIN_4  }, { GPIOC, LL_GPIO_PIN_5  },
};

#if defined(MIOS32_USE_ADC2) && NUM_CHN_2
#if !defined(GPIOF)
# error "MIOS32_USE_ADC2 (ADC3) needs PORTF for channels 4..9, 14 and 15. This package has no PORTF - only ADC3 channels 0..3 and 10..13 are bonded, and the map below cannot be built."
#endif
static const adc_pin_t pin_map_3[NUM_CHANNELS_MAX] = {
  { GPIOA, LL_GPIO_PIN_0  }, { GPIOA, LL_GPIO_PIN_1  },
  { GPIOA, LL_GPIO_PIN_2  }, { GPIOA, LL_GPIO_PIN_3  },
  { GPIOF, LL_GPIO_PIN_6  }, { GPIOF, LL_GPIO_PIN_7  },
  { GPIOF, LL_GPIO_PIN_8  }, { GPIOF, LL_GPIO_PIN_9  },
  { GPIOF, LL_GPIO_PIN_10 }, { GPIOF, LL_GPIO_PIN_3  },
  { GPIOC, LL_GPIO_PIN_0  }, { GPIOC, LL_GPIO_PIN_1  },
  { GPIOC, LL_GPIO_PIN_2  }, { GPIOC, LL_GPIO_PIN_3  },
  { GPIOF, LL_GPIO_PIN_4  }, { GPIOF, LL_GPIO_PIN_5  },
};
#endif

// LL rank constants are not consecutive values, so they need a table
static const u32 seq_rank[NUM_CHANNELS_MAX] = {
  LL_ADC_REG_RANK_1,  LL_ADC_REG_RANK_2,  LL_ADC_REG_RANK_3,  LL_ADC_REG_RANK_4,
  LL_ADC_REG_RANK_5,  LL_ADC_REG_RANK_6,  LL_ADC_REG_RANK_7,  LL_ADC_REG_RANK_8,
  LL_ADC_REG_RANK_9,  LL_ADC_REG_RANK_10, LL_ADC_REG_RANK_11, LL_ADC_REG_RANK_12,
  LL_ADC_REG_RANK_13, LL_ADC_REG_RANK_14, LL_ADC_REG_RANK_15, LL_ADC_REG_RANK_16,
};

static const u32 seq_length[NUM_CHANNELS_MAX] = {
  LL_ADC_REG_SEQ_SCAN_DISABLE,        LL_ADC_REG_SEQ_SCAN_ENABLE_2RANKS,
  LL_ADC_REG_SEQ_SCAN_ENABLE_3RANKS,  LL_ADC_REG_SEQ_SCAN_ENABLE_4RANKS,
  LL_ADC_REG_SEQ_SCAN_ENABLE_5RANKS,  LL_ADC_REG_SEQ_SCAN_ENABLE_6RANKS,
  LL_ADC_REG_SEQ_SCAN_ENABLE_7RANKS,  LL_ADC_REG_SEQ_SCAN_ENABLE_8RANKS,
  LL_ADC_REG_SEQ_SCAN_ENABLE_9RANKS,  LL_ADC_REG_SEQ_SCAN_ENABLE_10RANKS,
  LL_ADC_REG_SEQ_SCAN_ENABLE_11RANKS, LL_ADC_REG_SEQ_SCAN_ENABLE_12RANKS,
  LL_ADC_REG_SEQ_SCAN_ENABLE_13RANKS, LL_ADC_REG_SEQ_SCAN_ENABLE_14RANKS,
  LL_ADC_REG_SEQ_SCAN_ENABLE_15RANKS, LL_ADC_REG_SEQ_SCAN_ENABLE_16RANKS,
};


/////////////////////////////////////////////////////////////////////////////
// Local helpers
/////////////////////////////////////////////////////////////////////////////

//! Applies the deadband to one instance's freshly landed scan and flags what
//! moved. Shared by the (up to three) DMA interrupt handlers.
static void MIOS32_ADC_ScanComplete(u8 port)
{
  adc_inst_t *inst = &adc_inst[port];
  int i;
  u16 *src;

#if MIOS32_ADC_OVERSAMPLING_RATE >= 2
  // no oversampler in this silicon: accumulate here
  u16 *acc = &adc_values_sum[inst->slot_base];
  if( oversampling_ctr[port] == 0 ) {
    for(i=0; i<inst->num_chn; ++i)
      acc[i] = inst->raw[i];
  } else {
    for(i=0; i<inst->num_chn; ++i)
      acc[i] += inst->raw[i];
  }

  if( ++oversampling_ctr[port] < MIOS32_ADC_OVERSAMPLING_RATE )
    return; // not a complete set yet
  oversampling_ctr[port] = 0;

  src = acc;
#else
  src = inst->raw;
#endif

  for(i=0; i<inst->num_chn; ++i) {
    u32 slot = inst->slot_base + i;

#if MIOS32_ADC_DEADBAND_IDLE
    // a channel that has been still for a while gets the wider band, so EMI
    // cannot wake up a knob nobody is touching
    u16 deadband = adc_channel_idle_ctr[slot] ? adc_deadband : MIOS32_ADC_DEADBAND_IDLE;
#else
    // the runtime deadband, not the compile-time constant: this branch must
    // still honour MIOS32_ADC_DeadbandSet()
    u16 deadband = adc_deadband;
#endif

    if( abs((s32)src[i] - (s32)adc_channel_values[slot]) > (s32)deadband ) {
      adc_channel_values[slot] = src[i];
      adc_channel_changed[slot >> 5] |= (1 << (slot & 0x1f));
#if MIOS32_ADC_DEADBAND_IDLE
      adc_channel_idle_ctr[slot] = MIOS32_ADC_IDLE_CTR;
#endif
    } else {
#if MIOS32_ADC_DEADBAND_IDLE
      if( adc_channel_idle_ctr[slot] )
        adc_channel_idle_ctr[slot] -= 1;
#endif
    }
  }
}


//! Configures one instance: pins, sequence, DMA. Returns the next free slot.
static u8 MIOS32_ADC_InstInit(u8 port, ADC_TypeDef *adc, u16 chn_mask, u8 num_chn,
                              u8 slot_base, u16 *raw,
                              const adc_pin_t *pin_map,
                              u32 dma_stream, u32 dma_channel, IRQn_Type dma_irqn)
{
  adc_inst_t *inst = &adc_inst[port];
  int c, rank;

  inst->adc = adc;
  inst->dma_stream = dma_stream;
  inst->dma_channel = dma_channel;
  inst->chn_mask = chn_mask;
  inst->num_chn = num_chn;
  inst->slot_base = slot_base;
  inst->raw = raw;

  if( !num_chn )
    return slot_base;

  // analog pins
  LL_GPIO_InitTypeDef GPIO_InitStructure;
  LL_GPIO_StructInit(&GPIO_InitStructure);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
  for(c=0; c<NUM_CHANNELS_MAX; ++c) {
    if( chn_mask & (1 << c) ) {
      GPIO_InitStructure.Pin = pin_map[c].pin_mask;
      LL_GPIO_Init(pin_map[c].port, &GPIO_InitStructure);
    }
  }

  LL_ADC_Disable(adc);
  LL_ADC_SetResolution(adc, LL_ADC_RESOLUTION_12B);
  LL_ADC_SetDataAlignment(adc, LL_ADC_DATA_ALIGN_RIGHT);
  LL_ADC_SetSequencersScanMode(adc, LL_ADC_SEQ_SCAN_ENABLE);

  // the regular sequence, in ascending channel order - which is what makes
  // slot N of the arrays the Nth set bit of the mask
  LL_ADC_REG_SetSequencerLength(adc, seq_length[num_chn - 1]);
  rank = 0;
  for(c=0; c<NUM_CHANNELS_MAX; ++c) {
    if( chn_mask & (1 << c) ) {
      u32 ll_chn = __LL_ADC_DECIMAL_NB_TO_CHANNEL(c);
      LL_ADC_REG_SetSequencerRanks(adc, seq_rank[rank], ll_chn);
      LL_ADC_SetChannelSamplingTime(adc, ll_chn, MIOS32_ADC_SAMPLINGTIME);
      ++rank;
    }
  }

  LL_ADC_REG_SetTriggerSource(adc, LL_ADC_REG_TRIG_SOFTWARE);
  LL_ADC_REG_SetContinuousMode(adc, LL_ADC_REG_CONV_SINGLE);
  LL_ADC_REG_SetDMATransfer(adc, LL_ADC_REG_DMA_TRANSFER_LIMITED);

  // DMA: peripheral to memory, one scan per request
  LL_DMA_SetChannelSelection(DMA2, dma_stream, dma_channel);
  LL_DMA_SetDataTransferDirection(DMA2, dma_stream, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
  LL_DMA_SetStreamPriorityLevel(DMA2, dma_stream, LL_DMA_PRIORITY_HIGH);
  LL_DMA_SetMode(DMA2, dma_stream, LL_DMA_MODE_NORMAL);
  LL_DMA_SetPeriphIncMode(DMA2, dma_stream, LL_DMA_PERIPH_NOINCREMENT);
  LL_DMA_SetMemoryIncMode(DMA2, dma_stream, LL_DMA_MEMORY_INCREMENT);
  LL_DMA_SetPeriphSize(DMA2, dma_stream, LL_DMA_PDATAALIGN_HALFWORD);
  LL_DMA_SetMemorySize(DMA2, dma_stream, LL_DMA_MDATAALIGN_HALFWORD);
  LL_DMA_ConfigAddresses(DMA2, dma_stream,
                         LL_ADC_DMA_GetRegAddr(adc, LL_ADC_DMA_REG_REGULAR_DATA),
                         (u32)raw,
                         LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
  LL_DMA_SetDataLength(DMA2, dma_stream, num_chn);
  LL_DMA_EnableIT_TC(DMA2, dma_stream);
  LL_DMA_EnableStream(DMA2, dma_stream);

  MIOS32_IRQ_Install(dma_irqn, MIOS32_IRQ_ADC_DMA_PRIORITY);

  LL_ADC_Enable(adc);

  return slot_base + num_chn;
}


//! Rearms one instance's DMA and triggers its scan.
static void MIOS32_ADC_InstStart(u8 port)
{
  adc_inst_t *inst = &adc_inst[port];

  if( !inst->num_chn )
    return;

  // the stream is in normal mode and stops after one scan: rearm it first
  LL_DMA_DisableStream(DMA2, inst->dma_stream);
  while( LL_DMA_IsEnabledStream(DMA2, inst->dma_stream) );
  LL_DMA_SetDataLength(DMA2, inst->dma_stream, inst->num_chn);
  LL_DMA_EnableStream(DMA2, inst->dma_stream);

  LL_ADC_REG_StartConversionSWStart(inst->adc);
}

#endif /* NUM_CHN_TOTAL */


/////////////////////////////////////////////////////////////////////////////
//! Initializes the ADC driver
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_ADC_Init(u32 mode)
{
  if( mode != 0 )
    return -1; // unsupported mode

#if !NUM_CHN_TOTAL
  return -1; // no channel selected
#else
  int i;
  u8 slot = 0;

  adc_deadband = MIOS32_ADC_DEADBAND;
  service_prepare_callback = NULL;

  for(i=0; i<NUM_CHN_TOTAL; ++i) {
    adc_channel_values[i] = 0;
#if MIOS32_ADC_OVERSAMPLING_RATE >= 2
    adc_values_sum[i] = 0;
#endif
#if MIOS32_ADC_DEADBAND_IDLE
    adc_channel_idle_ctr[i] = 0;
#endif
  }
  for(i=0; i<(int)(sizeof(adc_channel_changed)/sizeof(u32)); ++i)
    adc_channel_changed[i] = 0;
  for(i=0; i<3; ++i) {
    adc_inst[i].num_chn = 0;
    adc_inst[i].adc = NULL;
#if MIOS32_ADC_OVERSAMPLING_RATE >= 2
    oversampling_ctr[i] = 0;
#endif
  }

  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);

  // the kernel clock lives in the common register block, shared by all three
  LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(ADC1), MIOS32_ADC_CLOCK_SOURCE);

#if NUM_CHN_0
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC1);
  slot = MIOS32_ADC_InstInit(0, ADC1, MIOS32_ADC0_CHANNEL_MASK, NUM_CHN_0,
                             slot, adc_raw_0, pin_map_12,
                             MIOS32_ADC0_DMA_STREAM, MIOS32_ADC0_DMA_CHANNEL,
                             MIOS32_ADC0_DMA_IRQn);
#endif
#if NUM_CHN_1
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC2);
  slot = MIOS32_ADC_InstInit(1, ADC2, MIOS32_ADC1_CHANNEL_MASK, NUM_CHN_1,
                             slot, adc_raw_1, pin_map_12,
                             MIOS32_ADC1_DMA_STREAM, MIOS32_ADC1_DMA_CHANNEL,
                             MIOS32_ADC1_DMA_IRQn);
#endif
#if NUM_CHN_2
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC3);
  slot = MIOS32_ADC_InstInit(2, ADC3, MIOS32_ADC2_CHANNEL_MASK, NUM_CHN_2,
                             slot, adc_raw_2, pin_map_3,
                             MIOS32_ADC2_DMA_STREAM, MIOS32_ADC2_DMA_CHANNEL,
                             MIOS32_ADC2_DMA_IRQn);
#endif

  MIOS32_ADC_StartConversions();

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
s32 MIOS32_ADC_ServicePrepareCallback_Init(void *_service_prepare_callback)
{
#if !NUM_CHN_TOTAL
  return -1;
#else
  service_prepare_callback = _service_prepare_callback;
  return 0;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Returns the last published value of a channel, in HARDWARE channel
//! numbering - the same numbers used in MIOS32_ADCn_CHANNEL_MASK.
//! \param[in] port MIOS32_ADC_PORT_ADC0/1/2
//! \param[in] chn hardware channel number
//! \return < 0 if the port or channel is not enabled
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_ADC_ChannelGet(u8 port, u8 chn)
{
#if !NUM_CHN_TOTAL
  return -1;
#else
  adc_inst_t *inst;
  int c, rank;

  if( port >= 3 )
    return -1;

  inst = &adc_inst[port];
  if( !inst->num_chn || chn >= NUM_CHANNELS_MAX )
    return -1;
  if( !(inst->chn_mask & (1 << chn)) )
    return -1; // channel not enabled

  // slot = how many enabled channels come before this one
  rank = 0;
  for(c=0; c<chn; ++c)
    if( inst->chn_mask & (1 << c) )
      ++rank;

  return adc_channel_values[inst->slot_base + rank];
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! \return the deadband currently used to notify changes
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_ADC_DeadbandGet(void)
{
#if !NUM_CHN_TOTAL
  return -1;
#else
  return adc_deadband;
#endif
}

/////////////////////////////////////////////////////////////////////////////
//! Sets the change a channel has to show before it is published.
//! \return < 0 on error
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_ADC_DeadbandSet(u16 deadband)
{
#if !NUM_CHN_TOTAL
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
s32 MIOS32_ADC_Handler(void *_callback)
{
  if( _callback == NULL )
    return -1;

#if !NUM_CHN_TOTAL
  return -1;
#else
  int port, c, rank;
  void (*callback)(u32 port, u32 chn, u32 value) = _callback;

  for(port=0; port<3; ++port) {
    adc_inst_t *inst = &adc_inst[port];
    if( !inst->num_chn )
      continue;

    rank = 0;
    for(c=0; c<NUM_CHANNELS_MAX; ++c) {
      if( !(inst->chn_mask & (1 << c)) )
        continue;

      u32 slot = inst->slot_base + rank;
      u32 mask = 1 << (slot & 0x1f);
      if( adc_channel_changed[slot >> 5] & mask ) {
        MIOS32_IRQ_Disable();
        u32 value = adc_channel_values[slot];
        adc_channel_changed[slot >> 5] &= ~mask;
        MIOS32_IRQ_Enable();

        callback(port, c, value);
      }
      ++rank;
    }
  }

  // optional settling hook; skip the scan if it asks for more time
  if( service_prepare_callback != NULL && service_prepare_callback() >= 1 )
    return 0;

  MIOS32_ADC_StartConversions();

  return 0;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Starts one scan on every enabled instance. Call this from a timer if the
//! periodic MIOS32_ADC_Handler() service has been disabled.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_ADC_StartConversions(void)
{
#if !NUM_CHN_TOTAL
  return -1;
#else
  int port;
  for(port=0; port<3; ++port)
    MIOS32_ADC_InstStart(port);
  return 0;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! DMA interrupts, one per enabled instance, raised once that instance's
//! whole scan has landed in memory.
//! \note not to be called from an application
/////////////////////////////////////////////////////////////////////////////
#if NUM_CHN_0
MIOS32_ADC0_DMA_IRQ_FUNC
{
  MIOS32_ADC0_DMA_CLEAR();
  MIOS32_ADC_ScanComplete(0);
#if MIOS32_ADC_OVERSAMPLING_RATE >= 2
  if( oversampling_ctr[0] )
    MIOS32_ADC_InstStart(0); // more samples needed for this set
#endif
}
#endif

#if NUM_CHN_1
MIOS32_ADC1_DMA_IRQ_FUNC
{
  MIOS32_ADC1_DMA_CLEAR();
  MIOS32_ADC_ScanComplete(1);
#if MIOS32_ADC_OVERSAMPLING_RATE >= 2
  if( oversampling_ctr[1] )
    MIOS32_ADC_InstStart(1);
#endif
}
#endif

#if NUM_CHN_2
MIOS32_ADC2_DMA_IRQ_FUNC
{
  MIOS32_ADC2_DMA_CLEAR();
  MIOS32_ADC_ScanComplete(2);
#if MIOS32_ADC_OVERSAMPLING_RATE >= 2
  if( oversampling_ctr[2] )
    MIOS32_ADC_InstStart(2);
#endif
}
#endif

//! \}

#endif /* ADC port selection valid */

#endif /* MIOS32_USE_ADC */
