//! \defgroup MIOS32_SPI
//!
//! Hardware Abstraction Layer for the SPI ports of the STM32F4
//!
//! Three ports are provided: SPI0 (SPI1 peripheral), SPI1 (SPI2 peripheral)
//! and SPI2 (SPI3 peripheral). Each port has a single CS line under manual
//! GPIO control.
//!
//! If SPI low-level functions are used to talk to something else on the same
//! port, make sure no MIOS32_* driver is claiming it at the same time. Those
//! drivers are opt-in, so this is a matter of NOT declaring
//! them (MIOS32_USE_SDCARD and the like) rather than of refusing them.
//!
//! Note that additional chip select lines can be easily added by using
//! the remaining free GPIOs of the core module. Shared SPI ports should be
//! arbitrated with (FreeRTOS based) Mutexes to avoid collisions!
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

// this module can be optionally enabled in a local mios32_config.h file (included from mios32.h)
#if defined(MIOS32_USE_SPI)


/////////////////////////////////////////////////////////////////////////////
// SPI Pin definitions
// (not part of mios32_spi.h file, since overruling would lead to a hardware
// dependency in MIOS32 applications)
//
// Each port has a single CS line, always plain GPIO (never an alternate
// function, even in slave mode - this driver never uses the SPI
// peripheral's own hardware NSS). A project needing a 2nd CS line per
// port drives that GPIO directly itself.
/////////////////////////////////////////////////////////////////////////////

#define MIOS32_SPI0_PTR        SPI1
#define MIOS32_SPI0_DMA_RX_PTR DMA2_Stream2
#define MIOS32_SPI0_DMA_RX_CHN LL_DMA_CHANNEL_3
#define MIOS32_SPI0_DMA_TX_PTR DMA2_Stream3
#define MIOS32_SPI0_DMA_TX_CHN LL_DMA_CHANNEL_3
#define MIOS32_SPI0_DMA_IRQ_CHANNEL DMA2_Stream2_IRQn
#define MIOS32_SPI0_DMA_IRQHANDLER_FUNC void DMA2_Stream2_IRQHandler(void)
#define MIOS32_SPI0_DMA_CTRL   DMA2
#define MIOS32_SPI0_DMA_RX_STREAM LL_DMA_STREAM_2
#define MIOS32_SPI0_DMA_TX_STREAM LL_DMA_STREAM_3
#define MIOS32_SPI0_DMA_RX_CLEAR_FLAGS() { LL_DMA_ClearFlag_TC2(DMA2); LL_DMA_ClearFlag_TE2(DMA2); LL_DMA_ClearFlag_HT2(DMA2); LL_DMA_ClearFlag_FE2(DMA2); }
#define MIOS32_SPI0_DMA_TX_CLEAR_FLAGS() { LL_DMA_ClearFlag_TC3(DMA2); LL_DMA_ClearFlag_TE3(DMA2); LL_DMA_ClearFlag_HT3(DMA2); LL_DMA_ClearFlag_FE3(DMA2); }
#ifndef MIOS32_SPI0_CS_PORT
#define MIOS32_SPI0_CS_PORT    GPIOA
#endif
#ifndef MIOS32_SPI0_CS_PIN
#define MIOS32_SPI0_CS_PIN     LL_GPIO_PIN_4
#endif
#ifndef MIOS32_SPI0_SCLK_PORT
#define MIOS32_SPI0_SCLK_PORT  GPIOA
#endif
#ifndef MIOS32_SPI0_SCLK_PIN
#define MIOS32_SPI0_SCLK_PIN   LL_GPIO_PIN_5
#endif
#ifndef MIOS32_SPI0_SCLK_AF
#define MIOS32_SPI0_SCLK_AF    LL_GPIO_AF_5
#endif
#ifndef MIOS32_SPI0_MISO_PORT
#define MIOS32_SPI0_MISO_PORT  GPIOA
#endif
#ifndef MIOS32_SPI0_MISO_PIN
#define MIOS32_SPI0_MISO_PIN   LL_GPIO_PIN_6
#endif
#ifndef MIOS32_SPI0_MISO_AF
#define MIOS32_SPI0_MISO_AF    LL_GPIO_AF_5
#endif
#ifndef MIOS32_SPI0_MOSI_PORT
#define MIOS32_SPI0_MOSI_PORT  GPIOA
#endif
#ifndef MIOS32_SPI0_MOSI_PIN
#define MIOS32_SPI0_MOSI_PIN   LL_GPIO_PIN_7
#endif
#ifndef MIOS32_SPI0_MOSI_AF
#define MIOS32_SPI0_MOSI_AF    LL_GPIO_AF_5
#endif


#define MIOS32_SPI1_PTR        SPI2
#define MIOS32_SPI1_DMA_RX_PTR DMA1_Stream3
#define MIOS32_SPI1_DMA_RX_CHN LL_DMA_CHANNEL_0
#define MIOS32_SPI1_DMA_TX_PTR DMA1_Stream4
#define MIOS32_SPI1_DMA_TX_CHN LL_DMA_CHANNEL_0
#define MIOS32_SPI1_DMA_IRQ_CHANNEL DMA1_Stream3_IRQn
#define MIOS32_SPI1_DMA_IRQHANDLER_FUNC void DMA1_Stream3_IRQHandler(void)
#define MIOS32_SPI1_DMA_CTRL   DMA1
#define MIOS32_SPI1_DMA_RX_STREAM LL_DMA_STREAM_3
#define MIOS32_SPI1_DMA_TX_STREAM LL_DMA_STREAM_4
#define MIOS32_SPI1_DMA_RX_CLEAR_FLAGS() { LL_DMA_ClearFlag_TC3(DMA1); LL_DMA_ClearFlag_TE3(DMA1); LL_DMA_ClearFlag_HT3(DMA1); LL_DMA_ClearFlag_FE3(DMA1); }
#define MIOS32_SPI1_DMA_TX_CLEAR_FLAGS() { LL_DMA_ClearFlag_TC4(DMA1); LL_DMA_ClearFlag_TE4(DMA1); LL_DMA_ClearFlag_HT4(DMA1); LL_DMA_ClearFlag_FE4(DMA1); }
#ifndef MIOS32_SPI1_CS_PORT
#define MIOS32_SPI1_CS_PORT    GPIOB
#endif
#ifndef MIOS32_SPI1_CS_PIN
#define MIOS32_SPI1_CS_PIN     LL_GPIO_PIN_1
#endif
#ifndef MIOS32_SPI1_SCLK_PORT
#define MIOS32_SPI1_SCLK_PORT  GPIOB
#endif
#ifndef MIOS32_SPI1_SCLK_PIN
#define MIOS32_SPI1_SCLK_PIN   LL_GPIO_PIN_13
#endif
#ifndef MIOS32_SPI1_SCLK_AF
#define MIOS32_SPI1_SCLK_AF    LL_GPIO_AF_5
#endif
#ifndef MIOS32_SPI1_MISO_PORT
#define MIOS32_SPI1_MISO_PORT  GPIOC
#endif
#ifndef MIOS32_SPI1_MISO_PIN
#define MIOS32_SPI1_MISO_PIN   LL_GPIO_PIN_2
#endif
#ifndef MIOS32_SPI1_MISO_AF
#define MIOS32_SPI1_MISO_AF    LL_GPIO_AF_5
#endif
#ifndef MIOS32_SPI1_MOSI_PORT
#define MIOS32_SPI1_MOSI_PORT  GPIOC
#endif
#ifndef MIOS32_SPI1_MOSI_PIN
#define MIOS32_SPI1_MOSI_PIN   LL_GPIO_PIN_3
#endif
#ifndef MIOS32_SPI1_MOSI_AF
#define MIOS32_SPI1_MOSI_AF    LL_GPIO_AF_5
#endif

#define MIOS32_SPI2_PTR        SPI3
#define MIOS32_SPI2_DMA_RX_PTR DMA1_Stream2
#define MIOS32_SPI2_DMA_RX_CHN LL_DMA_CHANNEL_0
#define MIOS32_SPI2_DMA_TX_PTR DMA1_Stream5
#define MIOS32_SPI2_DMA_TX_CHN LL_DMA_CHANNEL_0
#define MIOS32_SPI2_DMA_IRQ_CHANNEL DMA1_Stream2_IRQn
#define MIOS32_SPI2_DMA_IRQHANDLER_FUNC void DMA1_Stream2_IRQHandler(void)
#define MIOS32_SPI2_DMA_CTRL   DMA1
#define MIOS32_SPI2_DMA_RX_STREAM LL_DMA_STREAM_2
#define MIOS32_SPI2_DMA_TX_STREAM LL_DMA_STREAM_5
#define MIOS32_SPI2_DMA_RX_CLEAR_FLAGS() { LL_DMA_ClearFlag_TC2(DMA1); LL_DMA_ClearFlag_TE2(DMA1); LL_DMA_ClearFlag_HT2(DMA1); LL_DMA_ClearFlag_FE2(DMA1); }
#define MIOS32_SPI2_DMA_TX_CLEAR_FLAGS() { LL_DMA_ClearFlag_TC5(DMA1); LL_DMA_ClearFlag_TE5(DMA1); LL_DMA_ClearFlag_HT5(DMA1); LL_DMA_ClearFlag_FE5(DMA1); }
#ifndef MIOS32_SPI2_CS_PORT
#define MIOS32_SPI2_CS_PORT    GPIOA
#endif
#ifndef MIOS32_SPI2_CS_PIN
#define MIOS32_SPI2_CS_PIN     LL_GPIO_PIN_15
#endif
#ifndef MIOS32_SPI2_SCLK_PORT
#define MIOS32_SPI2_SCLK_PORT  GPIOB
#endif
#ifndef MIOS32_SPI2_SCLK_PIN
#define MIOS32_SPI2_SCLK_PIN   LL_GPIO_PIN_3
#endif
#ifndef MIOS32_SPI2_SCLK_AF
#define MIOS32_SPI2_SCLK_AF    LL_GPIO_AF_5
#endif
#ifndef MIOS32_SPI2_MISO_PORT
#define MIOS32_SPI2_MISO_PORT  GPIOB
#endif
#ifndef MIOS32_SPI2_MISO_PIN
#define MIOS32_SPI2_MISO_PIN   LL_GPIO_PIN_4
#endif
#ifndef MIOS32_SPI2_MISO_AF
#define MIOS32_SPI2_MISO_AF    LL_GPIO_AF_5
#endif
#ifndef MIOS32_SPI2_MOSI_PORT
#define MIOS32_SPI2_MOSI_PORT  GPIOB
#endif
#ifndef MIOS32_SPI2_MOSI_PIN
#define MIOS32_SPI2_MOSI_PIN   LL_GPIO_PIN_5
#endif
#ifndef MIOS32_SPI2_MOSI_AF
#define MIOS32_SPI2_MOSI_AF    LL_GPIO_AF_5
#endif

/////////////////////////////////////////////////////////////////////////////
// Local Defines
/////////////////////////////////////////////////////////////////////////////
#define CCR_ENABLE              ((uint32_t)0x00000001)



/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static void (*spi_callback[3])(void);

static u8 tx_dummy_byte;
static u8 rx_dummy_byte;


/////////////////////////////////////////////////////////////////////////////
//! Initializes SPI pins
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SPI_Init(u32 mode)
{
  // currently only mode 0 supported
  if( mode != 0 )
    return -1; // unsupported mode

  LL_DMA_InitTypeDef DMA_InitStructure;
  LL_DMA_StructInit(&DMA_InitStructure);

  ///////////////////////////////////////////////////////////////////////////
  // SPI0
  ///////////////////////////////////////////////////////////////////////////
#ifdef MIOS32_USE_SPI0

  // disable callback function
  spi_callback[0] = NULL;

  // set CS pin to 1
  MIOS32_SPI_CS_PinSet(0, 1);

  // IO configuration
  MIOS32_SPI_IO_Init(0, MIOS32_SPI_PIN_DRIVER_WEAK);

  // enable SPI peripheral clock
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

  // enable DMA1 and DMA2 clock
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);

  // DMA Configuration for SPI Rx Event
  LL_DMA_DisableStream(MIOS32_SPI0_DMA_CTRL, MIOS32_SPI0_DMA_RX_STREAM);
  DMA_InitStructure.Channel = MIOS32_SPI0_DMA_RX_CHN;
  DMA_InitStructure.PeriphOrM2MSrcAddress = (u32)&MIOS32_SPI0_PTR->DR;
  DMA_InitStructure.MemoryOrM2MDstAddress = 0; // will be configured later
  DMA_InitStructure.Direction = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
  DMA_InitStructure.NbData = 0; // will be configured later
  DMA_InitStructure.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
  DMA_InitStructure.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
  DMA_InitStructure.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
  DMA_InitStructure.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
  DMA_InitStructure.Mode = LL_DMA_MODE_NORMAL;
  DMA_InitStructure.Priority = LL_DMA_PRIORITY_MEDIUM;
  LL_DMA_Init(MIOS32_SPI0_DMA_CTRL, MIOS32_SPI0_DMA_RX_STREAM, &DMA_InitStructure);

  // DMA Configuration for SPI Tx Event
  // (partly re-using previous DMA setup)
  LL_DMA_DisableStream(MIOS32_SPI0_DMA_CTRL, MIOS32_SPI0_DMA_TX_STREAM);
  DMA_InitStructure.Channel = MIOS32_SPI0_DMA_TX_CHN;
  DMA_InitStructure.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
  LL_DMA_Init(MIOS32_SPI0_DMA_CTRL, MIOS32_SPI0_DMA_TX_STREAM, &DMA_InitStructure);

  // enable SPI
  LL_SPI_Enable(MIOS32_SPI0_PTR);

  // enable SPI interrupts to DMA
  LL_SPI_EnableDMAReq_RX(MIOS32_SPI0_PTR);
  LL_SPI_EnableDMAReq_TX(MIOS32_SPI0_PTR);

  // Configure DMA interrupt
  MIOS32_IRQ_Install(MIOS32_SPI0_DMA_IRQ_CHANNEL, MIOS32_IRQ_SPI_DMA_PRIORITY);

  // initial SPI peripheral configuration
  MIOS32_SPI_TransferModeInit(0, MIOS32_SPI_MODE_CLK1_PHASE1, MIOS32_SPI_PRESCALER_128);
#endif /* MIOS32_USE_SPI0 */


  ///////////////////////////////////////////////////////////////////////////
  // SPI1
  ///////////////////////////////////////////////////////////////////////////
#ifdef MIOS32_USE_SPI1

  // disable callback function
  spi_callback[1] = NULL;

  // set CS pin to 1
  MIOS32_SPI_CS_PinSet(1, 1);

  // IO configuration
  MIOS32_SPI_IO_Init(1, MIOS32_SPI_PIN_DRIVER_WEAK);

  // enable SPI peripheral clock
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2);

  // enable DMA1 and DMA2 clock
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);

  // DMA Configuration for SPI Rx Event
  LL_DMA_DisableStream(MIOS32_SPI1_DMA_CTRL, MIOS32_SPI1_DMA_RX_STREAM);
  DMA_InitStructure.Channel = MIOS32_SPI1_DMA_RX_CHN;
  DMA_InitStructure.PeriphOrM2MSrcAddress = (u32)&MIOS32_SPI1_PTR->DR;
  DMA_InitStructure.MemoryOrM2MDstAddress = 0; // will be configured later
  DMA_InitStructure.Direction = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
  DMA_InitStructure.NbData = 0; // will be configured later
  DMA_InitStructure.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
  DMA_InitStructure.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
  DMA_InitStructure.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
  DMA_InitStructure.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
  DMA_InitStructure.Mode = LL_DMA_MODE_NORMAL;
  DMA_InitStructure.Priority = LL_DMA_PRIORITY_MEDIUM;
  LL_DMA_Init(MIOS32_SPI1_DMA_CTRL, MIOS32_SPI1_DMA_RX_STREAM, &DMA_InitStructure);

  // DMA Configuration for SPI Tx Event
  // (partly re-using previous DMA setup)
  LL_DMA_DisableStream(MIOS32_SPI1_DMA_CTRL, MIOS32_SPI1_DMA_TX_STREAM);
  DMA_InitStructure.Channel = MIOS32_SPI1_DMA_TX_CHN;
  DMA_InitStructure.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
  LL_DMA_Init(MIOS32_SPI1_DMA_CTRL, MIOS32_SPI1_DMA_TX_STREAM, &DMA_InitStructure);

  // enable SPI
  LL_SPI_Enable(MIOS32_SPI1_PTR);

  // enable SPI interrupts to DMA
  LL_SPI_EnableDMAReq_RX(MIOS32_SPI1_PTR);
  LL_SPI_EnableDMAReq_TX(MIOS32_SPI1_PTR);

  // Configure DMA interrupt
  MIOS32_IRQ_Install(MIOS32_SPI1_DMA_IRQ_CHANNEL, MIOS32_IRQ_SPI_DMA_PRIORITY);

  // initial SPI peripheral configuration
  MIOS32_SPI_TransferModeInit(1, MIOS32_SPI_MODE_CLK1_PHASE1, MIOS32_SPI_PRESCALER_128);
#endif /* MIOS32_USE_SPI1 */


  ///////////////////////////////////////////////////////////////////////////
  // SPI2
  ///////////////////////////////////////////////////////////////////////////
#ifdef MIOS32_USE_SPI2

  // disable callback function
  spi_callback[2] = NULL;

  // set CS pin to 1
  MIOS32_SPI_CS_PinSet(2, 1);

  // IO configuration
  MIOS32_SPI_IO_Init(2, MIOS32_SPI_PIN_DRIVER_WEAK);

  // enable SPI peripheral clock
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI3);

  // enable DMA1 and DMA2 clock
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);

  // DMA Configuration for SPI Rx Event
  LL_DMA_DisableStream(MIOS32_SPI2_DMA_CTRL, MIOS32_SPI2_DMA_RX_STREAM);
  DMA_InitStructure.Channel = MIOS32_SPI2_DMA_RX_CHN;
  DMA_InitStructure.PeriphOrM2MSrcAddress = (u32)&MIOS32_SPI2_PTR->DR;
  DMA_InitStructure.MemoryOrM2MDstAddress = 0; // will be configured later
  DMA_InitStructure.Direction = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
  DMA_InitStructure.NbData = 0; // will be configured later
  DMA_InitStructure.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
  DMA_InitStructure.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
  DMA_InitStructure.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
  DMA_InitStructure.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
  DMA_InitStructure.Mode = LL_DMA_MODE_NORMAL;
  DMA_InitStructure.Priority = LL_DMA_PRIORITY_MEDIUM;
  LL_DMA_Init(MIOS32_SPI2_DMA_CTRL, MIOS32_SPI2_DMA_RX_STREAM, &DMA_InitStructure);

  // DMA Configuration for SPI Tx Event
  // (partly re-using previous DMA setup)
  LL_DMA_DisableStream(MIOS32_SPI2_DMA_CTRL, MIOS32_SPI2_DMA_TX_STREAM);
  DMA_InitStructure.Channel = MIOS32_SPI2_DMA_TX_CHN;
  DMA_InitStructure.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
  LL_DMA_Init(MIOS32_SPI2_DMA_CTRL, MIOS32_SPI2_DMA_TX_STREAM, &DMA_InitStructure);

  // enable SPI
  LL_SPI_Enable(MIOS32_SPI2_PTR);

  // enable SPI interrupts to DMA
  LL_SPI_EnableDMAReq_RX(MIOS32_SPI2_PTR);
  LL_SPI_EnableDMAReq_TX(MIOS32_SPI2_PTR);

  // Configure DMA interrupt
  MIOS32_IRQ_Install(MIOS32_SPI2_DMA_IRQ_CHANNEL, MIOS32_IRQ_SPI_DMA_PRIORITY);

  // initial SPI peripheral configuration
  MIOS32_SPI_TransferModeInit(2, MIOS32_SPI_MODE_CLK1_PHASE1, MIOS32_SPI_PRESCALER_128);
#endif /* MIOS32_USE_SPI2 */


  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! (Re-)initializes SPI IO Pins
//! By default, all output pins are configured with weak open drain drivers for 2 MHz
//! \param[in] spi SPI number (0, 1 or 2)
//! \param[in] spi_pin_driver configures the driver strength:
//! <UL>
//!   <LI>MIOS32_SPI_PIN_DRIVER_STRONG: configures outputs for up to 50 MHz
//!   <LI>MIOS32_SPI_PIN_DRIVER_STRONG_OD: configures outputs as open drain
//!       for up to 50 MHz (allows voltage shifting via pull-resistors)
//!   <LI>MIOS32_SPI_PIN_DRIVER_WEAK: configures outputs for up to 2 MHz (better EMC)
//!   <LI>MIOS32_SPI_PIN_DRIVER_WEAK_OD: configures outputs as open drain for
//!       up to 2 MHz (allows voltage shifting via pull-resistors)
//! </UL>
//! \return 0 if no error
//! \return -1 if disabled SPI port selected
//! \return -2 if unsupported SPI port selected
//! \return -3 if unsupported pin driver mode
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SPI_IO_Init(u8 spi, mios32_spi_pin_driver_t spi_pin_driver)
{
  // init GPIO structure
  LL_GPIO_InitTypeDef GPIO_InitStructure;
  LL_GPIO_StructInit(&GPIO_InitStructure);

  // select pin driver and output mode
  u8 slave = 0;
  switch( spi_pin_driver ) {
    case MIOS32_SPI_PIN_SLAVE_DRIVER_STRONG:
      slave = 1;
    case MIOS32_SPI_PIN_DRIVER_STRONG:
      GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
      GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
      break;

    case MIOS32_SPI_PIN_SLAVE_DRIVER_STRONG_OD:
      slave = 1;
    case MIOS32_SPI_PIN_DRIVER_STRONG_OD:
      GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
      GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
      break;

    case MIOS32_SPI_PIN_SLAVE_DRIVER_WEAK:
      slave = 1;
    case MIOS32_SPI_PIN_DRIVER_WEAK:
      GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
      GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
      break;

    case MIOS32_SPI_PIN_SLAVE_DRIVER_WEAK_OD:
      slave = 1;
    case MIOS32_SPI_PIN_DRIVER_WEAK_OD:
      GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
      GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
      break;

    default:
      return -3; // unsupported pin driver mode
  }

  switch( spi ) {
    case 0:
#ifndef MIOS32_USE_SPI0
      return -1; // disabled SPI port
#else
      if( slave ) {
	// SCLK and DOUT are inputs assigned to alternate functions
	GPIO_InitStructure.Mode = LL_GPIO_MODE_ALTERNATE;
	GPIO_InitStructure.Pin  = MIOS32_SPI0_SCLK_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI0_SCLK_AF;
	LL_GPIO_Init(MIOS32_SPI0_SCLK_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.Pin  = MIOS32_SPI0_MOSI_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI0_MOSI_AF;
	LL_GPIO_Init(MIOS32_SPI0_MOSI_PORT, &GPIO_InitStructure);
	// DOUT is output assigned to alternate function
	GPIO_InitStructure.Pin  = MIOS32_SPI0_MISO_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI0_MISO_AF;
	LL_GPIO_Init(MIOS32_SPI0_MISO_PORT, &GPIO_InitStructure);
      } else {
	// SCLK and DOUT are outputs assigned to alternate functions
	GPIO_InitStructure.Mode = LL_GPIO_MODE_ALTERNATE;
	GPIO_InitStructure.Pin  = MIOS32_SPI0_SCLK_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI0_SCLK_AF;
	LL_GPIO_Init(MIOS32_SPI0_SCLK_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.Pin  = MIOS32_SPI0_MOSI_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI0_MOSI_AF;
	LL_GPIO_Init(MIOS32_SPI0_MOSI_PORT, &GPIO_InitStructure);

	// DIN is input with pull-up
	GPIO_InitStructure.Pull = LL_GPIO_PULL_UP;
	GPIO_InitStructure.Pin  = MIOS32_SPI0_MISO_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI0_MISO_AF;
	LL_GPIO_Init(MIOS32_SPI0_MISO_PORT, &GPIO_InitStructure);
      }

      // CS is always plain GPIO output, regardless of master/slave mode
      GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
      GPIO_InitStructure.Pin  = MIOS32_SPI0_CS_PIN;
      LL_GPIO_Init(MIOS32_SPI0_CS_PORT, &GPIO_InitStructure);
      break;
#endif

    case 1:
#ifndef MIOS32_USE_SPI1
      return -1; // disabled SPI port
#else
      if( slave ) {
	return -3; // slave mode not supported for this pin
      } else {
	// SCLK and DIN are inputs
	GPIO_InitStructure.Mode = LL_GPIO_MODE_ALTERNATE;
	GPIO_InitStructure.Pin  = MIOS32_SPI1_SCLK_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI1_SCLK_AF;
	LL_GPIO_Init(MIOS32_SPI1_SCLK_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.Pin  = MIOS32_SPI1_MOSI_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI1_MOSI_AF;
	LL_GPIO_Init(MIOS32_SPI1_MOSI_PORT, &GPIO_InitStructure);

	// DIN is input with pull-up
	GPIO_InitStructure.Pull = LL_GPIO_PULL_UP;
	GPIO_InitStructure.Pin  = MIOS32_SPI1_MISO_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI1_MISO_AF;
	LL_GPIO_Init(MIOS32_SPI1_MISO_PORT, &GPIO_InitStructure);
      }

      // CS is always plain GPIO output, regardless of master/slave mode
      GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
      GPIO_InitStructure.Pin  = MIOS32_SPI1_CS_PIN;
      LL_GPIO_Init(MIOS32_SPI1_CS_PORT, &GPIO_InitStructure);

      break;
#endif

    case 2:
#ifndef MIOS32_USE_SPI2
      return -1; // disabled SPI port
#else
      if( slave ) {
	// SCLK and DOUT are inputs assigned to alternate functions
	GPIO_InitStructure.Mode = LL_GPIO_MODE_ALTERNATE;
	GPIO_InitStructure.Pin  = MIOS32_SPI2_SCLK_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI2_SCLK_AF;
	LL_GPIO_Init(MIOS32_SPI2_SCLK_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.Pin  = MIOS32_SPI2_MOSI_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI2_MOSI_AF;
	LL_GPIO_Init(MIOS32_SPI2_MOSI_PORT, &GPIO_InitStructure);
	// DOUT is output assigned to alternate function
	GPIO_InitStructure.Pin  = MIOS32_SPI2_MISO_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI2_MISO_AF;
	LL_GPIO_Init(MIOS32_SPI2_MISO_PORT, &GPIO_InitStructure);
      } else {
	// SCLK and DIN are inputs
	GPIO_InitStructure.Mode = LL_GPIO_MODE_ALTERNATE;
	GPIO_InitStructure.Pin  = MIOS32_SPI2_SCLK_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI2_SCLK_AF;
	LL_GPIO_Init(MIOS32_SPI2_SCLK_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.Pin  = MIOS32_SPI2_MOSI_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI2_MOSI_AF;
	LL_GPIO_Init(MIOS32_SPI2_MOSI_PORT, &GPIO_InitStructure);

	// DIN is input with pull-up
	GPIO_InitStructure.Pull = LL_GPIO_PULL_UP;
	GPIO_InitStructure.Pin  = MIOS32_SPI2_MISO_PIN;
	GPIO_InitStructure.Alternate = MIOS32_SPI2_MISO_AF;
	LL_GPIO_Init(MIOS32_SPI2_MISO_PORT, &GPIO_InitStructure);
      }

      // CS is always plain GPIO output, regardless of master/slave mode
      GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
      GPIO_InitStructure.Pin  = MIOS32_SPI2_CS_PIN;
      LL_GPIO_Init(MIOS32_SPI2_CS_PORT, &GPIO_InitStructure);

      break;
#endif

    default:
      return -2; // unsupported SPI port
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! (Re-)initializes SPI peripheral transfer mode
//! By default, all SPI peripherals are configured with
//! MIOS32_SPI_MODE_CLK1_PHASE1 and MIOS32_SPI_PRESCALER_128
//!
//! \param[in] spi SPI number (0, 1 or 2)
//! \param[in] spi_mode configures clock and capture phase:
//! <UL>
//!   <LI>MIOS32_SPI_MODE_CLK0_PHASE0: Idle level of clock is 0, data captured at rising edge
//!   <LI>MIOS32_SPI_MODE_CLK0_PHASE1: Idle level of clock is 0, data captured at falling edge
//!   <LI>MIOS32_SPI_MODE_CLK1_PHASE0: Idle level of clock is 1, data captured at falling edge
//!   <LI>MIOS32_SPI_MODE_CLK1_PHASE1: Idle level of clock is 1, data captured at rising edge
//! </UL>
//! \param[in] spi_prescaler configures the SPI speed:
//! <UL>
//! (SPI0 is clocked from the 84 MHz default APB2 bus on this family; SPI1/
//! SPI2 sit on APB1 at 42 MHz - exactly half these rates):
//!   <LI>MIOS32_SPI_PRESCALER_2: sets clock rate 23.8 nS @ 84 MHz (42 MBit/s)
//!   <LI>MIOS32_SPI_PRESCALER_4: sets clock rate 47.6 nS @ 84 MHz (21 MBit/s)
//!   <LI>MIOS32_SPI_PRESCALER_8: sets clock rate 95.2 nS @ 84 MHz (10.5 MBit/s)
//!   <LI>MIOS32_SPI_PRESCALER_16: sets clock rate 190.5 nS @ 84 MHz (5.25 MBit/s)
//!   <LI>MIOS32_SPI_PRESCALER_32: sets clock rate 381 nS @ 84 MHz (2.625 MBit/s)
//!   <LI>MIOS32_SPI_PRESCALER_64: sets clock rate 762 nS @ 84 MHz (1.3125 MBit/s)
//!   <LI>MIOS32_SPI_PRESCALER_128: sets clock rate 1.52 uS @ 84 MHz (0.65625 MBit/s)
//!   <LI>MIOS32_SPI_PRESCALER_256: sets clock rate 3.05 uS @ 84 MHz (0.328125 MBit/s)
//! </UL>
//! \return 0 if no error
//! \return -1 if disabled SPI port selected
//! \return -2 if unsupported SPI port selected
//! \return -3 if invalid spi_prescaler selected
//! \return -4 if invalid spi_mode selected
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SPI_TransferModeInit(u8 spi, mios32_spi_mode_t spi_mode, mios32_spi_prescaler_t spi_prescaler)
{
  // SPI configuration
  LL_SPI_InitTypeDef SPI_InitStructure;
  SPI_InitStructure.TransferDirection = LL_SPI_FULL_DUPLEX;
  SPI_InitStructure.Mode              = LL_SPI_MODE_MASTER;
  SPI_InitStructure.DataWidth         = LL_SPI_DATAWIDTH_8BIT;
  SPI_InitStructure.NSS               = LL_SPI_NSS_SOFT;
  SPI_InitStructure.BitOrder          = LL_SPI_MSB_FIRST;
  SPI_InitStructure.CRCCalculation    = LL_SPI_CRCCALCULATION_DISABLE;
  SPI_InitStructure.CRCPoly           = 7;

  switch( spi_mode ) {
    case MIOS32_SPI_MODE_SLAVE_CLK0_PHASE0:
      SPI_InitStructure.Mode = LL_SPI_MODE_SLAVE;
      SPI_InitStructure.NSS  = LL_SPI_NSS_HARD_INPUT;
    case MIOS32_SPI_MODE_CLK0_PHASE0:
      SPI_InitStructure.ClockPolarity = LL_SPI_POLARITY_LOW;
      SPI_InitStructure.ClockPhase    = LL_SPI_PHASE_1EDGE;
      break;

    case MIOS32_SPI_MODE_SLAVE_CLK0_PHASE1:
      SPI_InitStructure.Mode = LL_SPI_MODE_SLAVE;
      SPI_InitStructure.NSS  = LL_SPI_NSS_HARD_INPUT;
    case MIOS32_SPI_MODE_CLK0_PHASE1:
      SPI_InitStructure.ClockPolarity = LL_SPI_POLARITY_LOW;
      SPI_InitStructure.ClockPhase    = LL_SPI_PHASE_2EDGE;
      break;

    case MIOS32_SPI_MODE_SLAVE_CLK1_PHASE0:
      SPI_InitStructure.Mode = LL_SPI_MODE_SLAVE;
      SPI_InitStructure.NSS  = LL_SPI_NSS_HARD_INPUT;
    case MIOS32_SPI_MODE_CLK1_PHASE0:
      SPI_InitStructure.ClockPolarity = LL_SPI_POLARITY_HIGH;
      SPI_InitStructure.ClockPhase    = LL_SPI_PHASE_1EDGE;
      break;

    case MIOS32_SPI_MODE_SLAVE_CLK1_PHASE1:
      SPI_InitStructure.Mode = LL_SPI_MODE_SLAVE;
      SPI_InitStructure.NSS  = LL_SPI_NSS_HARD_INPUT;
    case MIOS32_SPI_MODE_CLK1_PHASE1:
      SPI_InitStructure.ClockPolarity = LL_SPI_POLARITY_HIGH;
      SPI_InitStructure.ClockPhase    = LL_SPI_PHASE_2EDGE;
      break;
    default:
      return -4; // invalid SPI clock/phase mode
  }

  if( spi_prescaler >= 8 )
    return -3; // invalid prescaler selected

  switch( spi ) {
    case 0: {
#ifndef MIOS32_USE_SPI0
      return -1; // disabled SPI port
#else
      u16 prev_cr1 = MIOS32_SPI0_PTR->CR1;

      LL_SPI_Disable(MIOS32_SPI0_PTR);
      SPI_InitStructure.BaudRate = (((u16)spi_prescaler&7)-1) << 3;
      LL_SPI_Init(MIOS32_SPI0_PTR, &SPI_InitStructure);
      LL_SPI_Enable(MIOS32_SPI0_PTR);

      if( SPI_InitStructure.Mode == LL_SPI_MODE_MASTER ) {
	if( (prev_cr1 ^ MIOS32_SPI0_PTR->CR1) & 3 ) { // CPOL and CPHA located at bit #1 and #0
	  // clock configuration has been changed - we should send a dummy byte
	  // before the application activates chip select.
	  // this solves a dependency between two drivers sharing the port
	  MIOS32_SPI_TransferByte(spi, 0xff);
	}
      }
#endif
    } break;

    case 1: {
#ifndef MIOS32_USE_SPI1
      return -1; // disabled SPI port
#else
      if( SPI_InitStructure.Mode == LL_SPI_MODE_SLAVE ) {
        return -3; // slave mode not supported for this SPI
      }
      u16 prev_cr1 = MIOS32_SPI1_PTR->CR1;

      LL_SPI_Disable(MIOS32_SPI1_PTR);
      SPI_InitStructure.BaudRate = (((u16)spi_prescaler&7)-1) << 3;
      LL_SPI_Init(MIOS32_SPI1_PTR, &SPI_InitStructure);
      LL_SPI_Enable(MIOS32_SPI1_PTR);

      if( SPI_InitStructure.Mode == LL_SPI_MODE_MASTER ) {
        if( (prev_cr1 ^ MIOS32_SPI1_PTR->CR1) & 3 ) { // CPOL and CPHA located at bit #1 and #0
          // clock configuration has been changed - we should send a dummy byte
          // before the application activates chip select.
          // this solves a dependency between two drivers sharing the port
          MIOS32_SPI_TransferByte(spi, 0xff);
        }
      }
#endif
    } break;

    case 2: {
#ifndef MIOS32_USE_SPI2
      return -1; // disabled SPI port
#else
      u16 prev_cr1 = MIOS32_SPI2_PTR->CR1;

      LL_SPI_Disable(MIOS32_SPI2_PTR);
      SPI_InitStructure.BaudRate = (((u16)spi_prescaler&7)-1) << 3;
      LL_SPI_Init(MIOS32_SPI2_PTR, &SPI_InitStructure);
      LL_SPI_Enable(MIOS32_SPI2_PTR);

      if( SPI_InitStructure.Mode == LL_SPI_MODE_MASTER ) {
	if( (prev_cr1 ^ MIOS32_SPI2_PTR->CR1) & 3 ) { // CPOL and CPHA located at bit #1 and #0
	  // clock configuration has been changed - we should send a dummy byte
	  // before the application activates chip select.
	  // this solves a dependency between two drivers sharing the port
	  MIOS32_SPI_TransferByte(spi, 0xff);
	}
      }
#endif
    } break;

    default:
      return -2; // unsupported SPI port
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Controls the CS (Chip Select) pin of a SPI port
//! \param[in] spi SPI number (0, 1 or 2)
//! \param[in] pin_value 0 or 1
//! \return 0 if no error
//! \return -1 if disabled SPI port selected
//! \return -2 if unsupported SPI port selected
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SPI_CS_PinSet(u8 spi, u8 pin_value)
{
  switch( spi ) {
  case 0:
#ifndef MIOS32_USE_SPI0
    return -1; // disabled SPI port
#else
    MIOS32_SYS_STM_PINSET(MIOS32_SPI0_CS_PORT, MIOS32_SPI0_CS_PIN, pin_value);
    break;
#endif

  case 1:
#ifndef MIOS32_USE_SPI1
    return -1; // disabled SPI port
#else
    MIOS32_SYS_STM_PINSET(MIOS32_SPI1_CS_PORT, MIOS32_SPI1_CS_PIN, pin_value);
    break;
#endif

  case 2:
#ifndef MIOS32_USE_SPI2
    return -1; // disabled SPI port
#else
    MIOS32_SYS_STM_PINSET(MIOS32_SPI2_CS_PORT, MIOS32_SPI2_CS_PIN, pin_value);
    break;
#endif

  default:
    return -2; // unsupported SPI port
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Transfers a byte to SPI output and reads back the return value from SPI input
//! \param[in] spi SPI number (0, 1 or 2)
//! \param[in] b the byte which should be transfered
//! \return >= 0: the read byte
//! \return -1 if disabled SPI port selected
//! \return -2 if unsupported SPI port selected
//! \return -3 if unsupported SPI mode configured via MIOS32_SPI_TransferModeInit()
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SPI_TransferByte(u8 spi, u8 b)
{
  SPI_TypeDef *spi_ptr;

  switch( spi ) {
    case 0:
#ifndef MIOS32_USE_SPI0
      return -1; // disabled SPI port
#else
      spi_ptr = MIOS32_SPI0_PTR;
      break;
#endif

    case 1:
#ifndef MIOS32_USE_SPI1
      return -1; // disabled SPI port
#else
      spi_ptr = MIOS32_SPI1_PTR;
      break;
#endif

    case 2:
#ifndef MIOS32_USE_SPI2
      return -1; // disabled SPI port
#else
      spi_ptr = MIOS32_SPI2_PTR;
      break;
#endif

    default:
      return -2; // unsupported SPI port
  }

  // send byte
  spi_ptr->DR = b;

  // TK: without this read (which can be done to any bus location) we could sporadically
  // get the status byte at the moment where DR is written. Accordingly, the busy bit
  // will be 0.
  // you won't see this dummy read in STM drivers, as they never have a DR write
  // followed by SR read, or as they are using SPI1/SPI2 pointers, which results into
  // some additional CPU instructions between strh/ldrh accesses.
  // We use a bus access instead of NOPs to avoid any risk for back-to-back transactions
  // over AHB (if SPI1/SPI2 pointers are used, there is still a risk for such a scenario,
  // e.g. if DMA loads the bus!)

  // wait until SPI transfer finished
  if( spi_ptr->CR1 & SPI_CR1_MSTR ) {
    while( spi_ptr->SR & SPI_SR_BSY );
  } else {
    while( !(spi_ptr->SR & SPI_SR_RXNE) );
  }

  // return received byte
  return spi_ptr->DR;
}


/////////////////////////////////////////////////////////////////////////////
//! Transfers a block of bytes via DMA.
//! \param[in] spi SPI number (0, 1 or 2)
//! \param[in] send_buffer pointer to buffer which should be sent.<BR>
//! If NULL, 0xff (all-one) will be sent.
//! \param[in] receive_buffer pointer to buffer which should get the received values.<BR>
//! If NULL, received bytes will be discarded.
//! \param[in] len number of bytes which should be transfered
//! \param[in] callback pointer to callback function which will be executed
//! from DMA channel interrupt once the transfer is finished.
//! If NULL, no callback function will be used, and MIOS32_SPI_TransferBlock() will
//! block until the transfer is finished.
//! \return >= 0 if no error during transfer
//! \return -1 if disabled SPI port selected
//! \return -2 if unsupported SPI port selected
//! \return -3 if function has been called during an ongoing DMA transfer
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SPI_TransferBlock(u8 spi, u8 *send_buffer, u8 *receive_buffer, u16 len, void *callback)
{
  SPI_TypeDef *spi_ptr;
  DMA_Stream_TypeDef *dma_tx_ptr, *dma_rx_ptr;

  switch( spi ) {
    case 0:
#ifndef MIOS32_USE_SPI0
      return -1; // disabled SPI port
#else
      spi_ptr = MIOS32_SPI0_PTR;
      dma_tx_ptr = MIOS32_SPI0_DMA_TX_PTR;
      dma_rx_ptr = MIOS32_SPI0_DMA_RX_PTR;
      break;
#endif

    case 1:
#ifndef MIOS32_USE_SPI1
      return -1; // disabled SPI port
#else
      spi_ptr = MIOS32_SPI1_PTR;
      dma_tx_ptr = MIOS32_SPI1_DMA_TX_PTR;
      dma_rx_ptr = MIOS32_SPI1_DMA_RX_PTR;
      break;
#endif

    case 2:
#ifndef MIOS32_USE_SPI2
      return -1; // disabled SPI port
#else
      spi_ptr = MIOS32_SPI2_PTR;
      dma_tx_ptr = MIOS32_SPI2_DMA_TX_PTR;
      dma_rx_ptr = MIOS32_SPI2_DMA_RX_PTR;
      break;
#endif

    default:
      return -2; // unsupported SPI port
  }

  // exit if ongoing transfer
  if( dma_rx_ptr->NDTR )
    return -3;

  // set callback function
  spi_callback[spi] = callback;

  // ensure that previously received value doesn't cause DMA access
  if( spi_ptr->DR );

  // configure Rx channel
  // the stream must be disabled to configure new values
  u32 rx_CCR = dma_rx_ptr->CR & ~CCR_ENABLE;
  dma_rx_ptr->CR = rx_CCR;
  if( receive_buffer != NULL ) {
    // enable memory addr. increment - bytes written into receive buffer
    dma_rx_ptr->M0AR = (u32)receive_buffer;
    rx_CCR |= DMA_SxCR_MINC;
  } else {
    // disable memory addr. increment - bytes written into dummy buffer
    rx_dummy_byte = 0xff;
    dma_rx_ptr->M0AR = (u32)&rx_dummy_byte;
    rx_CCR &= ~DMA_SxCR_MINC;
  }
  dma_rx_ptr->NDTR = len;
  rx_CCR |= CCR_ENABLE;


  // configure Tx channel
  // the stream must be disabled to configure new values
  u32 tx_CCR = dma_tx_ptr->CR & ~CCR_ENABLE;
  dma_tx_ptr->CR = tx_CCR;
  if( send_buffer != NULL ) {
    // enable memory addr. increment - bytes read from send buffer
    dma_tx_ptr->M0AR = (u32)send_buffer;
    tx_CCR |= DMA_SxCR_MINC;
  } else {
    // disable memory addr. increment - bytes read from dummy buffer
    tx_dummy_byte = 0xff;
    dma_tx_ptr->M0AR = (u32)&tx_dummy_byte;
    tx_CCR &= ~DMA_SxCR_MINC;
  }
  dma_tx_ptr->NDTR = len;

  // new for STM32F4 DMA: it's required to clear interrupt flags before DMA stream is enabled again
  switch( spi ) {
  case 0: MIOS32_SPI0_DMA_RX_CLEAR_FLAGS(); MIOS32_SPI0_DMA_TX_CLEAR_FLAGS(); break;
  case 1: MIOS32_SPI1_DMA_RX_CLEAR_FLAGS(); MIOS32_SPI1_DMA_TX_CLEAR_FLAGS(); break;
  case 2: MIOS32_SPI2_DMA_RX_CLEAR_FLAGS(); MIOS32_SPI2_DMA_TX_CLEAR_FLAGS(); break;
  }

  // enable DMA interrupt if callback function active
  if( callback != NULL ) {
    rx_CCR |= DMA_SxCR_TCIE;
    dma_rx_ptr->CR = rx_CCR;

    // start DMA transfer
    dma_tx_ptr->CR = tx_CCR | CCR_ENABLE;
  } else {
    rx_CCR &= ~DMA_SxCR_TCIE;
    dma_rx_ptr->CR = rx_CCR;

    // start DMA transfer
    dma_tx_ptr->CR = tx_CCR | CCR_ENABLE;

    // if no callback: wait until all bytes have been transmitted/received.
    // BOUNDED wait - same rationale as the STM32G0xx implementation: if a
    // second context stole RX bytes mid-transfer (MIOS32_SPI has no internal
    // locking, serialization is the caller's job), the RX counter never
    // reaches zero. Fail the transfer and clean up instead of spinning
    // forever; the counter must be zeroed too, or the ongoing-transfer guard
    // at the top of this function would reject every future call.
    u32 timeout_ctr = 10000000;
    while( dma_rx_ptr->NDTR ) {
      if( !--timeout_ctr ) {
        dma_rx_ptr->CR &= ~CCR_ENABLE;
        dma_tx_ptr->CR &= ~CCR_ENABLE;
        // a F4 DMA stream aborts asynchronously - EN must read back 0
        // before NDTR may be written (bounded, aborts settle in a few cycles)
        u32 settle = 10000;
        while( ((dma_rx_ptr->CR | dma_tx_ptr->CR) & CCR_ENABLE) && --settle );
        dma_rx_ptr->NDTR = 0;
        dma_tx_ptr->NDTR = 0;
        return -4; // transfer lost (RX stream out of sync)
      }
    }
  }

  return 0; // no error;
}


/////////////////////////////////////////////////////////////////////////////
// Called when callback function has been defined and SPI transfer has finished
/////////////////////////////////////////////////////////////////////////////
MIOS32_SPI0_DMA_IRQHANDLER_FUNC
{
  MIOS32_SPI0_DMA_RX_CLEAR_FLAGS();

  if( spi_callback[0] != NULL )
    spi_callback[0]();
}

MIOS32_SPI1_DMA_IRQHANDLER_FUNC
{
  MIOS32_SPI1_DMA_RX_CLEAR_FLAGS();

  if( spi_callback[1] != NULL )
    spi_callback[1]();
}

MIOS32_SPI2_DMA_IRQHANDLER_FUNC
{
  MIOS32_SPI2_DMA_RX_CLEAR_FLAGS();

  if( spi_callback[2] != NULL )
    spi_callback[2]();
}

//! \}

#endif /* MIOS32_USE_SPI */
