// $Id: mios32_spi.c 1938 2014-01-19 17:13:43Z tk $
//! \defgroup MIOS32_SPI
//!
//! Hardware Abstraction Layer for SPI ports of STM32G0xx
//!
//! Two ports are provided on every G0 chip: SPI0 (SPI1 peripheral) and SPI1
//! (SPI2 peripheral). A 3rd port, SPI2 (SPI3 peripheral), is available on
//! G0B0/G0B1/G0C1 only (MIOS32_USE_SPI2 is force-disabled on every other G0
//! chip). Each port has a single CS (chip select) line under manual GPIO
//! control - there is no second CS line on this family (unlike some
//! STM32F4xx boards).
//!
//! If SPI low-level functions should be used to access other peripherals,
//! please ensure that the appr. MIOS32_* drivers are disabled (e.g.
//! add '#define MIOS32_DONT_USE_SDCARD'
//! to your mios_config.h file)
//!
//! Note that additional chip select lines can be easily added by using
//! the remaining free GPIOs. Shared SPI ports should be arbitrated with
//! (FreeRTOS based) Mutexes to avoid collisions!
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

// SPI2 (3rd port, SPI3 peripheral) only exists on G0B0/G0B1/G0C1 - force it
// off on every other G0 chip, whatever the project's mios32_config.h says.
#if defined(MIOS32_USE_SPI2) && !defined(MIOS32_PROCESSOR_STM32G0B0) && !defined(MIOS32_PROCESSOR_STM32G0B1) && !defined(MIOS32_PROCESSOR_STM32G0C1)
#undef MIOS32_USE_SPI2
#endif

// on G0B0/G0B1/G0C1, DMA1 channels 4-7 and DMA2 channels 1-5 share a single
// NVIC vector (unlike the rest of the G0 family, where DMA1 ch4-7 has its
// own vector) - SPI1 (DMA1) and SPI2 (DMA2) must be serviced by one combined
// interrupt handler on these chips.
#if defined(MIOS32_PROCESSOR_STM32G0B0) || defined(MIOS32_PROCESSOR_STM32G0B1) || defined(MIOS32_PROCESSOR_STM32G0C1)
#define MIOS32_SPI_SHARED_DMA_VECTOR 1
#endif


/////////////////////////////////////////////////////////////////////////////
// SPI Pin definitions
// (not part of mios32_spi.h file, since overruling would lead to a hardware
// dependency in MIOS32 applications)
//
// CS is always plain GPIO (never an alternate function), even in slave mode -
// this driver never uses the SPI peripheral's own hardware NSS.
/////////////////////////////////////////////////////////////////////////////

#define MIOS32_SPI0_PTR        SPI1
#define MIOS32_SPI0_CLOCK      LL_APB2_GRP1_PERIPH_SPI1
#define MIOS32_SPI0_DMA_RX_PTR DMA1
#define MIOS32_SPI0_DMA_CLOCK  LL_AHB1_GRP1_PERIPH_DMA1
#define MIOS32_SPI0_DMA_RX_CHN LL_DMA_CHANNEL_2
#define MIOS32_SPI0_DMA_RX_REQ LL_DMAMUX_REQ_SPI1_RX
#define MIOS32_SPI0_DMA_RX_IRQ_FLAGS (LL_DMA_IFCR_CTCIF2 | LL_DMA_IFCR_CTEIF2 | LL_DMA_IFCR_CHTIF2 | LL_DMA_IFCR_CGIF2)
#define MIOS32_SPI0_DMA_TX_PTR DMA1
#define MIOS32_SPI0_DMA_TX_CHN LL_DMA_CHANNEL_3
#define MIOS32_SPI0_DMA_TX_REQ LL_DMAMUX_REQ_SPI1_TX
#define MIOS32_SPI0_DMA_TX_IRQ_FLAGS (LL_DMA_IFCR_CTCIF3 | LL_DMA_IFCR_CTEIF3 | LL_DMA_IFCR_CHTIF3 | LL_DMA_IFCR_CGIF3)
#define MIOS32_SPI0_DMA_IRQ_CHANNEL DMA1_Channel2_3_IRQn
#define MIOS32_SPI0_DMA_IRQHANDLER_FUNC void DMA1_Channel2_3_IRQHandler(void)
#ifndef MIOS32_SPI0_CS_PORT
#define MIOS32_SPI0_CS_PORT   GPIOA
#endif
#ifndef MIOS32_SPI0_CS_PIN
#define MIOS32_SPI0_CS_PIN    LL_GPIO_PIN_4
#endif
#ifndef MIOS32_SPI0_SCLK_PORT
#define MIOS32_SPI0_SCLK_PORT GPIOA
#endif
#ifndef MIOS32_SPI0_SCLK_PIN
#define MIOS32_SPI0_SCLK_PIN  LL_GPIO_PIN_5
#endif
#ifndef MIOS32_SPI0_SCLK_AF
#define MIOS32_SPI0_SCLK_AF  LL_GPIO_AF_0
#endif
#ifndef MIOS32_SPI0_MISO_PORT
#define MIOS32_SPI0_MISO_PORT GPIOA
#endif
#ifndef MIOS32_SPI0_MISO_PIN
#define MIOS32_SPI0_MISO_PIN  LL_GPIO_PIN_6
#endif
#ifndef MIOS32_SPI0_MISO_AF
#define MIOS32_SPI0_MISO_AF  LL_GPIO_AF_0
#endif
#ifndef MIOS32_SPI0_MOSI_PORT
#define MIOS32_SPI0_MOSI_PORT GPIOA
#endif
#ifndef MIOS32_SPI0_MOSI_PIN
#define MIOS32_SPI0_MOSI_PIN  LL_GPIO_PIN_7
#endif
#ifndef MIOS32_SPI0_MOSI_AF
#define MIOS32_SPI0_MOSI_AF  LL_GPIO_AF_0
#endif

#define MIOS32_SPI1_PTR        SPI2
#define MIOS32_SPI1_CLOCK      LL_APB1_GRP1_PERIPH_SPI2
#define MIOS32_SPI1_DMA_CLOCK  LL_AHB1_GRP1_PERIPH_DMA1
#define MIOS32_SPI1_DMA_RX_PTR DMA1
#define MIOS32_SPI1_DMA_RX_CHN LL_DMA_CHANNEL_4
#define MIOS32_SPI1_DMA_RX_REQ LL_DMAMUX_REQ_SPI2_RX
#define MIOS32_SPI1_DMA_RX_IRQ_FLAGS (LL_DMA_IFCR_CTCIF4 | LL_DMA_IFCR_CTEIF4 | LL_DMA_IFCR_CHTIF4 | LL_DMA_IFCR_CGIF4)
#define MIOS32_SPI1_DMA_TX_PTR DMA1
#define MIOS32_SPI1_DMA_TX_CHN LL_DMA_CHANNEL_5
#define MIOS32_SPI1_DMA_TX_REQ LL_DMAMUX_REQ_SPI2_TX
#define MIOS32_SPI1_DMA_TX_IRQ_FLAGS (LL_DMA_IFCR_CTCIF5 | LL_DMA_IFCR_CTEIF5 | LL_DMA_IFCR_CHTIF5 | LL_DMA_IFCR_CGIF5)
#if defined(MIOS32_SPI_SHARED_DMA_VECTOR)
#define MIOS32_SPI1_DMA_IRQ_CHANNEL DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn
#define MIOS32_SPI1_DMA_IRQHANDLER_FUNC void DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQHandler(void)
#else
#define MIOS32_SPI1_DMA_IRQ_CHANNEL DMA1_Ch4_7_DMAMUX1_OVR_IRQn
#define MIOS32_SPI1_DMA_IRQHANDLER_FUNC void DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler(void)
#endif
#ifndef MIOS32_SPI1_CS_PORT
#define MIOS32_SPI1_CS_PORT   GPIOB
#endif
#ifndef MIOS32_SPI1_CS_PIN
#define MIOS32_SPI1_CS_PIN    LL_GPIO_PIN_12
#endif
#ifndef MIOS32_SPI1_SCLK_PORT
#define MIOS32_SPI1_SCLK_PORT GPIOB
#endif
#ifndef MIOS32_SPI1_SCLK_PIN
#define MIOS32_SPI1_SCLK_PIN  LL_GPIO_PIN_10
#endif
#ifndef MIOS32_SPI1_SCLK_AF
#define MIOS32_SPI1_SCLK_AF  LL_GPIO_AF_5
#endif
#ifndef MIOS32_SPI1_MISO_PORT
#define MIOS32_SPI1_MISO_PORT GPIOB
#endif
#ifndef MIOS32_SPI1_MISO_PIN
#define MIOS32_SPI1_MISO_PIN  LL_GPIO_PIN_2
#endif
#ifndef MIOS32_SPI1_MISO_AF
#define MIOS32_SPI1_MISO_AF  LL_GPIO_AF_0
#endif
#ifndef MIOS32_SPI1_MOSI_PORT
#define MIOS32_SPI1_MOSI_PORT GPIOB
#endif
#ifndef MIOS32_SPI1_MOSI_PIN
#define MIOS32_SPI1_MOSI_PIN  LL_GPIO_PIN_11
#endif
#ifndef MIOS32_SPI1_MOSI_AF
#define MIOS32_SPI1_MOSI_AF  LL_GPIO_AF_0
#endif

// SPI2 (SPI3 peripheral) - only available on G0B0/G0B1/G0C1, see
// MIOS32_USE_SPI2 force-undef above. Pin mapping is an unverified default
// (PB3/PB4/PB5, AF6) - not yet checked against real hardware or the
// reference manual for conflicts with other peripherals on these pins.
#if defined(MIOS32_USE_SPI2)
#define MIOS32_SPI2_PTR        SPI3
#define MIOS32_SPI2_CLOCK      LL_APB1_GRP1_PERIPH_SPI3
#define MIOS32_SPI2_DMA_CLOCK  LL_AHB1_GRP1_PERIPH_DMA2
#define MIOS32_SPI2_DMA_RX_PTR DMA2
#define MIOS32_SPI2_DMA_RX_CHN LL_DMA_CHANNEL_1
#define MIOS32_SPI2_DMA_RX_REQ LL_DMAMUX_REQ_SPI3_RX
#define MIOS32_SPI2_DMA_RX_IRQ_FLAGS (LL_DMA_IFCR_CTCIF1 | LL_DMA_IFCR_CTEIF1 | LL_DMA_IFCR_CHTIF1 | LL_DMA_IFCR_CGIF1)
#define MIOS32_SPI2_DMA_TX_PTR DMA2
#define MIOS32_SPI2_DMA_TX_CHN LL_DMA_CHANNEL_2
#define MIOS32_SPI2_DMA_TX_REQ LL_DMAMUX_REQ_SPI3_TX
#define MIOS32_SPI2_DMA_TX_IRQ_FLAGS (LL_DMA_IFCR_CTCIF2 | LL_DMA_IFCR_CTEIF2 | LL_DMA_IFCR_CHTIF2 | LL_DMA_IFCR_CGIF2)
#define MIOS32_SPI2_DMA_IRQ_CHANNEL DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn
#ifndef MIOS32_SPI2_CS_PORT
#define MIOS32_SPI2_CS_PORT   GPIOB
#endif
#ifndef MIOS32_SPI2_CS_PIN
#define MIOS32_SPI2_CS_PIN    LL_GPIO_PIN_6
#endif
#ifndef MIOS32_SPI2_SCLK_PORT
#define MIOS32_SPI2_SCLK_PORT GPIOB
#endif
#ifndef MIOS32_SPI2_SCLK_PIN
#define MIOS32_SPI2_SCLK_PIN  LL_GPIO_PIN_3
#endif
#ifndef MIOS32_SPI2_SCLK_AF
#define MIOS32_SPI2_SCLK_AF  LL_GPIO_AF_6
#endif
#ifndef MIOS32_SPI2_MISO_PORT
#define MIOS32_SPI2_MISO_PORT GPIOB
#endif
#ifndef MIOS32_SPI2_MISO_PIN
#define MIOS32_SPI2_MISO_PIN  LL_GPIO_PIN_4
#endif
#ifndef MIOS32_SPI2_MISO_AF
#define MIOS32_SPI2_MISO_AF  LL_GPIO_AF_6
#endif
#ifndef MIOS32_SPI2_MOSI_PORT
#define MIOS32_SPI2_MOSI_PORT GPIOB
#endif
#ifndef MIOS32_SPI2_MOSI_PIN
#define MIOS32_SPI2_MOSI_PIN  LL_GPIO_PIN_5
#endif
#ifndef MIOS32_SPI2_MOSI_AF
#define MIOS32_SPI2_MOSI_AF  LL_GPIO_AF_6
#endif
#endif


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

	LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
	LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);

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
	LL_APB2_GRP1_EnableClock(MIOS32_SPI0_CLOCK);

	// enable DMA clock
	LL_AHB1_GRP1_EnableClock(MIOS32_SPI0_DMA_CLOCK);

	LL_DMA_DisableChannel(MIOS32_SPI0_DMA_RX_PTR, MIOS32_SPI0_DMA_RX_CHN);
	LL_DMA_DisableChannel(MIOS32_SPI0_DMA_TX_PTR, MIOS32_SPI0_DMA_TX_CHN);
	// DMA Configuration for SPI Rx Event
	DMA_InitStructure.PeriphRequest = MIOS32_SPI0_DMA_RX_REQ;
	DMA_InitStructure.PeriphOrM2MSrcAddress = LL_SPI_DMA_GetRegAddr(MIOS32_SPI0_PTR);
	DMA_InitStructure.MemoryOrM2MDstAddress = 0; // will be configured later
	DMA_InitStructure.Direction = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
	DMA_InitStructure.NbData = 0; // will be configured later
	DMA_InitStructure.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
	DMA_InitStructure.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
	DMA_InitStructure.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
	DMA_InitStructure.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
	DMA_InitStructure.Mode = LL_DMA_MODE_NORMAL;
	DMA_InitStructure.Priority = LL_DMA_PRIORITY_MEDIUM;
	LL_DMA_Init(MIOS32_SPI0_DMA_RX_PTR, MIOS32_SPI0_DMA_RX_CHN, &DMA_InitStructure);
	// DMA Configuration for SPI Tx Event
	// (partly re-using previous DMA setup)
	DMA_InitStructure.PeriphRequest = MIOS32_SPI0_DMA_TX_REQ;
	DMA_InitStructure.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
	LL_DMA_Init(MIOS32_SPI0_DMA_TX_PTR, MIOS32_SPI0_DMA_TX_CHN, &DMA_InitStructure);

	// enable SPI interrupts to DMA
	LL_SPI_EnableDMAReq_RX(MIOS32_SPI0_PTR);
	LL_SPI_EnableDMAReq_TX(MIOS32_SPI0_PTR);

	// Configure DMA interrupt
	MIOS32_IRQ_Install(MIOS32_SPI0_DMA_IRQ_CHANNEL, MIOS32_IRQ_SPI_DMA_PRIORITY);

	// initial SPI peripheral configuration
	MIOS32_SPI_TransferModeInit(0, MIOS32_SPI_MODE_CLK0_PHASE0, MIOS32_SPI_PRESCALER_128);
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
	LL_APB1_GRP1_EnableClock(MIOS32_SPI1_CLOCK);

	// enable DMA clock
	LL_AHB1_GRP1_EnableClock(MIOS32_SPI1_DMA_CLOCK);

	LL_DMA_DisableChannel(MIOS32_SPI1_DMA_RX_PTR, MIOS32_SPI1_DMA_RX_CHN);
	LL_DMA_DisableChannel(MIOS32_SPI1_DMA_TX_PTR, MIOS32_SPI1_DMA_TX_CHN);
	// DMA Configuration for SPI Rx Event
	DMA_InitStructure.PeriphRequest = MIOS32_SPI1_DMA_RX_REQ;
	DMA_InitStructure.PeriphOrM2MSrcAddress = LL_SPI_DMA_GetRegAddr(MIOS32_SPI1_PTR);
	DMA_InitStructure.MemoryOrM2MDstAddress = 0; // will be configured later
	DMA_InitStructure.Direction = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
	DMA_InitStructure.NbData = 0; // will be configured later
	DMA_InitStructure.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
	DMA_InitStructure.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
	DMA_InitStructure.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
	DMA_InitStructure.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
	DMA_InitStructure.Mode = LL_DMA_MODE_NORMAL;
	DMA_InitStructure.Priority = LL_DMA_PRIORITY_MEDIUM;
	LL_DMA_Init(MIOS32_SPI1_DMA_RX_PTR, MIOS32_SPI1_DMA_RX_CHN, &DMA_InitStructure);
	// DMA Configuration for SPI Tx Event
	// (partly re-using previous DMA setup)
	DMA_InitStructure.PeriphRequest = MIOS32_SPI1_DMA_TX_REQ;
	DMA_InitStructure.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
	LL_DMA_Init(MIOS32_SPI1_DMA_TX_PTR, MIOS32_SPI1_DMA_TX_CHN, &DMA_InitStructure);

	// enable SPI interrupts to DMA
	LL_SPI_EnableDMAReq_RX(MIOS32_SPI1_PTR);
	LL_SPI_EnableDMAReq_TX(MIOS32_SPI1_PTR);

	// Configure DMA interrupt
	MIOS32_IRQ_Install(MIOS32_SPI1_DMA_IRQ_CHANNEL, MIOS32_IRQ_SPI_DMA_PRIORITY);

	// initial SPI peripheral configuration
	MIOS32_SPI_TransferModeInit(1, MIOS32_SPI_MODE_CLK0_PHASE0, MIOS32_SPI_PRESCALER_128);
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
	LL_APB1_GRP1_EnableClock(MIOS32_SPI2_CLOCK);

	// enable DMA clock
	LL_AHB1_GRP1_EnableClock(MIOS32_SPI2_DMA_CLOCK);

	LL_DMA_DisableChannel(MIOS32_SPI2_DMA_RX_PTR, MIOS32_SPI2_DMA_RX_CHN);
	LL_DMA_DisableChannel(MIOS32_SPI2_DMA_TX_PTR, MIOS32_SPI2_DMA_TX_CHN);
	// DMA Configuration for SPI Rx Event
	DMA_InitStructure.PeriphRequest = MIOS32_SPI2_DMA_RX_REQ;
	DMA_InitStructure.PeriphOrM2MSrcAddress = LL_SPI_DMA_GetRegAddr(MIOS32_SPI2_PTR);
	DMA_InitStructure.MemoryOrM2MDstAddress = 0; // will be configured later
	DMA_InitStructure.Direction = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
	DMA_InitStructure.NbData = 0; // will be configured later
	DMA_InitStructure.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
	DMA_InitStructure.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
	DMA_InitStructure.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
	DMA_InitStructure.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
	DMA_InitStructure.Mode = LL_DMA_MODE_NORMAL;
	DMA_InitStructure.Priority = LL_DMA_PRIORITY_MEDIUM;
	LL_DMA_Init(MIOS32_SPI2_DMA_RX_PTR, MIOS32_SPI2_DMA_RX_CHN, &DMA_InitStructure);
	// DMA Configuration for SPI Tx Event
	// (partly re-using previous DMA setup)
	DMA_InitStructure.PeriphRequest = MIOS32_SPI2_DMA_TX_REQ;
	DMA_InitStructure.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
	LL_DMA_Init(MIOS32_SPI2_DMA_TX_PTR, MIOS32_SPI2_DMA_TX_CHN, &DMA_InitStructure);

	// enable SPI interrupts to DMA
	LL_SPI_EnableDMAReq_RX(MIOS32_SPI2_PTR);
	LL_SPI_EnableDMAReq_TX(MIOS32_SPI2_PTR);

	// Configure DMA interrupt (shares a vector with SPI1 on this chip - see
	// MIOS32_SPI_SHARED_DMA_VECTOR)
	MIOS32_IRQ_Install(MIOS32_SPI2_DMA_IRQ_CHANNEL, MIOS32_IRQ_SPI_DMA_PRIORITY);

	// initial SPI peripheral configuration
	MIOS32_SPI_TransferModeInit(2, MIOS32_SPI_MODE_CLK0_PHASE0, MIOS32_SPI_PRESCALER_128);
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
		GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_HIGH;
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
#if !defined(MIOS32_FAMILY_STM32G0xx)
			return -3; // slave mode not supported for this pin
#else
			// SCLK and DOUT are inputs assigned to alternate functions
			GPIO_InitStructure.Mode = LL_GPIO_MODE_ALTERNATE;
			GPIO_InitStructure.Pin = MIOS32_SPI0_SCLK_PIN;
			GPIO_InitStructure.Alternate = MIOS32_SPI0_SCLK_AF;
			LL_GPIO_Init(MIOS32_SPI0_SCLK_PORT, &GPIO_InitStructure);
			GPIO_InitStructure.Pin = MIOS32_SPI0_MOSI_PIN;
			GPIO_InitStructure.Alternate = MIOS32_SPI0_MOSI_AF;
			LL_GPIO_Init(MIOS32_SPI0_MOSI_PORT, &GPIO_InitStructure);
			// DOUT is output assigned to alternate function
			GPIO_InitStructure.Pin = MIOS32_SPI0_MISO_PIN;
			GPIO_InitStructure.Alternate = MIOS32_SPI0_MISO_AF;
			LL_GPIO_Init(MIOS32_SPI0_MISO_PORT, &GPIO_InitStructure);
#endif
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
			GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
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
#if !defined(MIOS32_FAMILY_STM32G0xx)
			return -3; // slave mode not supported for this pin
#else
			// SCLK and DOUT are inputs assigned to alternate functions
			GPIO_InitStructure.Mode = LL_GPIO_MODE_ALTERNATE;
			GPIO_InitStructure.Pin = MIOS32_SPI1_SCLK_PIN;
			GPIO_InitStructure.Alternate = MIOS32_SPI1_SCLK_AF;
			LL_GPIO_Init(MIOS32_SPI1_SCLK_PORT, &GPIO_InitStructure);
			GPIO_InitStructure.Pin = MIOS32_SPI1_MOSI_PIN;
			GPIO_InitStructure.Alternate = MIOS32_SPI1_MOSI_AF;
			LL_GPIO_Init(MIOS32_SPI1_MOSI_PORT, &GPIO_InitStructure);
			// DOUT is output assigned to alternate function
			GPIO_InitStructure.Pin = MIOS32_SPI1_MISO_PIN;
			GPIO_InitStructure.Alternate = MIOS32_SPI1_MISO_AF;
			LL_GPIO_Init(MIOS32_SPI1_MISO_PORT, &GPIO_InitStructure);
#endif
		} else {
			// SCLK and DOUT are outputs assigned to alternate functions
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
			GPIO_InitStructure.Pin = MIOS32_SPI2_SCLK_PIN;
			GPIO_InitStructure.Alternate = MIOS32_SPI2_SCLK_AF;
			LL_GPIO_Init(MIOS32_SPI2_SCLK_PORT, &GPIO_InitStructure);
			GPIO_InitStructure.Pin = MIOS32_SPI2_MOSI_PIN;
			GPIO_InitStructure.Alternate = MIOS32_SPI2_MOSI_AF;
			LL_GPIO_Init(MIOS32_SPI2_MOSI_PORT, &GPIO_InitStructure);
			// DOUT is output assigned to alternate function
			GPIO_InitStructure.Pin = MIOS32_SPI2_MISO_PIN;
			GPIO_InitStructure.Alternate = MIOS32_SPI2_MISO_AF;
			LL_GPIO_Init(MIOS32_SPI2_MISO_PORT, &GPIO_InitStructure);
		} else {
			// SCLK and DOUT are outputs assigned to alternate functions
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
//! (SPI0/SPI1 are both clocked from the 64 MHz default APB bus on this family):
//!   <LI>MIOS32_SPI_PRESCALER_2: sets clock rate 31.3 nS @ 64 MHz (32 MBit/s)
//!   <LI>MIOS32_SPI_PRESCALER_4: sets clock rate 62.5 nS @ 64 MHz (16 MBit/s)
//!   <LI>MIOS32_SPI_PRESCALER_8: sets clock rate 125 nS @ 64 MHz (8 MBit/s)
//!   <LI>MIOS32_SPI_PRESCALER_16: sets clock rate 250 nS @ 64 MHz (4 MBit/s)
//!   <LI>MIOS32_SPI_PRESCALER_32: sets clock rate 500 nS @ 64 MHz (2 MBit/s)
//!   <LI>MIOS32_SPI_PRESCALER_64: sets clock rate 1 uS @ 64 MHz (1 MBit/s)
//!   <LI>MIOS32_SPI_PRESCALER_128: sets clock rate 2 uS @ 64 MHz (0.5 MBit/s)
//!   <LI>MIOS32_SPI_PRESCALER_256: sets clock rate 4 uS @ 64 MHz (0.25 MBit/s)
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
	SPI_InitStructure.Mode 				= LL_SPI_MODE_MASTER;
	SPI_InitStructure.DataWidth 		= LL_SPI_DATAWIDTH_8BIT;
	SPI_InitStructure.NSS 				= LL_SPI_NSS_SOFT;
	SPI_InitStructure.BitOrder 			= LL_SPI_MSB_FIRST;
	SPI_InitStructure.CRCCalculation 	= LL_SPI_CRCCALCULATION_DISABLE;
	SPI_InitStructure.CRCPoly 			= 7;

	switch( spi_mode ) {
	case MIOS32_SPI_MODE_SLAVE_CLK0_PHASE0:
		SPI_InitStructure.Mode 			= LL_SPI_MODE_SLAVE;
		SPI_InitStructure.NSS  			= LL_SPI_NSS_HARD_INPUT;
	case MIOS32_SPI_MODE_CLK0_PHASE0:
		SPI_InitStructure.ClockPolarity = LL_SPI_POLARITY_LOW;
		SPI_InitStructure.ClockPhase 	= LL_SPI_PHASE_1EDGE;
		break;

	case MIOS32_SPI_MODE_SLAVE_CLK0_PHASE1:
		SPI_InitStructure.Mode 			= LL_SPI_MODE_SLAVE;
		SPI_InitStructure.NSS  			= LL_SPI_NSS_HARD_INPUT;
	case MIOS32_SPI_MODE_CLK0_PHASE1:
		SPI_InitStructure.ClockPolarity = LL_SPI_POLARITY_LOW;
		SPI_InitStructure.ClockPhase 	= LL_SPI_PHASE_2EDGE;
		break;

	case MIOS32_SPI_MODE_SLAVE_CLK1_PHASE0:
		SPI_InitStructure.Mode 			= LL_SPI_MODE_SLAVE;
		SPI_InitStructure.NSS  			= LL_SPI_NSS_HARD_INPUT;
	case MIOS32_SPI_MODE_CLK1_PHASE0:
		SPI_InitStructure.ClockPolarity = LL_SPI_POLARITY_HIGH;
		SPI_InitStructure.ClockPhase 	= LL_SPI_PHASE_1EDGE;
		break;

	case MIOS32_SPI_MODE_SLAVE_CLK1_PHASE1:
		SPI_InitStructure.Mode 			= LL_SPI_MODE_SLAVE;
		SPI_InitStructure.NSS  			= LL_SPI_NSS_HARD_INPUT;
	case MIOS32_SPI_MODE_CLK1_PHASE1:
		SPI_InitStructure.ClockPolarity = LL_SPI_POLARITY_HIGH;
		SPI_InitStructure.ClockPhase 	= LL_SPI_PHASE_2EDGE;
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

		// Insure SPI disabled for configuration
		LL_SPI_Disable(MIOS32_SPI0_PTR);
		SPI_InitStructure.BaudRate = (((u16)spi_prescaler&7)-1) << 3;
		LL_SPI_SetRxFIFOThreshold(MIOS32_SPI0_PTR, LL_SPI_RX_FIFO_TH_QUARTER);
		LL_SPI_Init(MIOS32_SPI0_PTR, &SPI_InitStructure);
		LL_SPI_SetStandard(MIOS32_SPI0_PTR, LL_SPI_PROTOCOL_MOTOROLA);
		LL_SPI_DisableNSSPulseMgt(MIOS32_SPI0_PTR);
		// enable SPI
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
		u16 prev_cr1 = MIOS32_SPI1_PTR->CR1;
		// Insure SPI disabled for configuration
		LL_SPI_Disable(MIOS32_SPI1_PTR);
		SPI_InitStructure.BaudRate = (((u16)spi_prescaler&7)-1) << 3;
		LL_SPI_SetRxFIFOThreshold(MIOS32_SPI1_PTR, LL_SPI_RX_FIFO_TH_QUARTER);
		LL_SPI_Init(MIOS32_SPI1_PTR, &SPI_InitStructure);
		LL_SPI_SetStandard(MIOS32_SPI1_PTR, LL_SPI_PROTOCOL_MOTOROLA);
		LL_SPI_DisableNSSPulseMgt(MIOS32_SPI1_PTR);
		// SPI enabled
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
		// Insure SPI disabled for configuration
		LL_SPI_Disable(MIOS32_SPI2_PTR);
		SPI_InitStructure.BaudRate = (((u16)spi_prescaler&7)-1) << 3;
		LL_SPI_SetRxFIFOThreshold(MIOS32_SPI2_PTR, LL_SPI_RX_FIFO_TH_QUARTER);
		LL_SPI_Init(MIOS32_SPI2_PTR, &SPI_InitStructure);
		LL_SPI_SetStandard(MIOS32_SPI2_PTR, LL_SPI_PROTOCOL_MOTOROLA);
		LL_SPI_DisableNSSPulseMgt(MIOS32_SPI2_PTR);
		// SPI enabled
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
	*(uint8_t*)&spi_ptr->DR = b;

	//if( spi_ptr->SR ); // dummy read due to undocumented pipelining issue :-/
	// TK: without this read (which can be done to any bus location) we could sporadically
	// get the status byte at the moment where DR is written. Accordingly, the busy bit
	// will be 0.
	// you won't see this dummy read in STM drivers, as they never have a DR write
	// followed by SR read, or as they are using SPI1/SPI2 pointers, which results into
	// some additional CPU instructions between strh/ldrh accesses.
	// We use a bus access instead of NOPs to avoid any risk for back-to-back transactions
	// over AHB (if SPI1/SPI2 pointers are used, there is still a risk for such a scenario,
	// e.g. if DMA loads the bus!)

	// TK update: the dummy read above becomes obsolete since we are checking for SPI Master mode now
	// which requires a read operation as well

	// wait until SPI transfer finished
	if( spi_ptr->CR1 & SPI_CR1_MSTR ) {
		while( spi_ptr->SR & SPI_SR_BSY );
	} else {
		while( !(spi_ptr->SR & SPI_SR_RXNE) );
	}

	// return received byte
	return *(__IO uint8_t *)&spi_ptr->DR;
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
	DMA_TypeDef *dma_tx_ptr, *dma_rx_ptr;
	u32 dma_tx_chn, dma_rx_chn;
	u32 dma_tx_irq_flags, dma_rx_irq_flags;

	switch( spi ) {
	case 0:
#ifndef MIOS32_USE_SPI0
		return -1; // disabled SPI port
#else
		spi_ptr = MIOS32_SPI0_PTR;
		dma_tx_ptr = MIOS32_SPI0_DMA_TX_PTR;
		dma_tx_chn = MIOS32_SPI0_DMA_TX_CHN;
		dma_tx_irq_flags = MIOS32_SPI0_DMA_TX_IRQ_FLAGS;
		dma_rx_ptr = MIOS32_SPI0_DMA_RX_PTR;
		dma_rx_chn = MIOS32_SPI0_DMA_RX_CHN;
		dma_rx_irq_flags = MIOS32_SPI0_DMA_RX_IRQ_FLAGS;
		break;
#endif

	case 1:
#ifndef MIOS32_USE_SPI1
		return -1; // disabled SPI port
#else
		spi_ptr = MIOS32_SPI1_PTR;
		dma_tx_ptr = MIOS32_SPI1_DMA_TX_PTR;
		dma_tx_chn = MIOS32_SPI1_DMA_TX_CHN;
		dma_tx_irq_flags = MIOS32_SPI1_DMA_TX_IRQ_FLAGS;
		dma_rx_ptr = MIOS32_SPI1_DMA_RX_PTR;
		dma_rx_chn = MIOS32_SPI1_DMA_RX_CHN;
		dma_rx_irq_flags = MIOS32_SPI1_DMA_RX_IRQ_FLAGS;
		break;
#endif

	case 2:
#ifndef MIOS32_USE_SPI2
		return -1; // disabled SPI port
#else
		spi_ptr = MIOS32_SPI2_PTR;
		dma_tx_ptr = MIOS32_SPI2_DMA_TX_PTR;
		dma_tx_chn = MIOS32_SPI2_DMA_TX_CHN;
		dma_tx_irq_flags = MIOS32_SPI2_DMA_TX_IRQ_FLAGS;
		dma_rx_ptr = MIOS32_SPI2_DMA_RX_PTR;
		dma_rx_chn = MIOS32_SPI2_DMA_RX_CHN;
		dma_rx_irq_flags = MIOS32_SPI2_DMA_RX_IRQ_FLAGS;
		break;
#endif

	default:
		return -2; // unsupported SPI port
	}

	// exit if ongoing transfer
	if( LL_DMA_GetDataLength(dma_rx_ptr, dma_rx_chn) )
		return -3;

	// set callback function
	spi_callback[spi] = callback;

	// ensure that previously received value doesn't cause DMA access
	if( spi_ptr->DR );

	// configure Rx channel - the channel must be disabled to configure new values
	LL_DMA_DisableChannel(dma_rx_ptr, dma_rx_chn);
	if( receive_buffer != NULL ) {
		LL_DMA_SetMemoryAddress(dma_rx_ptr, dma_rx_chn, (u32)receive_buffer);
		LL_DMA_SetMemoryIncMode(dma_rx_ptr, dma_rx_chn, LL_DMA_MEMORY_INCREMENT);
	} else {
		rx_dummy_byte = 0xff;
		LL_DMA_SetMemoryAddress(dma_rx_ptr, dma_rx_chn, (u32)&rx_dummy_byte);
		LL_DMA_SetMemoryIncMode(dma_rx_ptr, dma_rx_chn, LL_DMA_MEMORY_NOINCREMENT);
	}
	LL_DMA_SetDataLength(dma_rx_ptr, dma_rx_chn, len);

	// configure Tx channel - the channel must be disabled to configure new values
	LL_DMA_DisableChannel(dma_tx_ptr, dma_tx_chn);
	if( send_buffer != NULL ) {
		LL_DMA_SetMemoryAddress(dma_tx_ptr, dma_tx_chn, (u32)send_buffer);
		LL_DMA_SetMemoryIncMode(dma_tx_ptr, dma_tx_chn, LL_DMA_MEMORY_INCREMENT);
	} else {
		tx_dummy_byte = 0xff;
		LL_DMA_SetMemoryAddress(dma_tx_ptr, dma_tx_chn, (u32)&tx_dummy_byte);
		LL_DMA_SetMemoryIncMode(dma_tx_ptr, dma_tx_chn, LL_DMA_MEMORY_NOINCREMENT);
	}
	LL_DMA_SetDataLength(dma_tx_ptr, dma_tx_chn, len);

	// interrupt flags must be cleared before the DMA channel is enabled again
	dma_rx_ptr->IFCR |= dma_rx_irq_flags;
	dma_tx_ptr->IFCR |= dma_tx_irq_flags;

	// enable DMA interrupt if callback function active
	if( callback != NULL ) {
		LL_DMA_EnableIT_TC(dma_rx_ptr, dma_rx_chn);
	} else {
		LL_DMA_DisableIT_TC(dma_rx_ptr, dma_rx_chn);
	}

	// start DMA transfer
	LL_DMA_EnableChannel(dma_rx_ptr, dma_rx_chn);
	LL_DMA_EnableChannel(dma_tx_ptr, dma_tx_chn);

	// if no callback: wait until all bytes have been transmitted/received.
	// BOUNDED wait: if the RX stream got out of sync (e.g. a second context
	// entered TransferByte/TransferBlock on the same port and stole RX bytes
	// mid-transfer - MIOS32_SPI has no internal locking, serialization is the
	// caller's job), the RX counter never reaches zero. The bound is far
	// beyond any legitimate transfer time (65535 bytes at the slowest
	// prescaler), so a timeout always means a lost transfer: disable the
	// channels so the next call starts from a clean state, and report the
	// error instead of spinning forever (a 2026-08-09 hardware capture showed
	// exactly that: RX counter stuck above zero, whole application starved
	// behind this loop).
	if( callback == NULL ) {
		u32 timeout_ctr = 10000000;
		while( LL_DMA_GetDataLength(dma_rx_ptr, dma_rx_chn) ) {
			if( !--timeout_ctr ) {
				LL_DMA_DisableChannel(dma_rx_ptr, dma_rx_chn);
				LL_DMA_DisableChannel(dma_tx_ptr, dma_tx_chn);
				// the counter keeps its value across a disable - zero it, or
				// the ongoing-transfer guard at the top of this function
				// would reject every future call
				LL_DMA_SetDataLength(dma_rx_ptr, dma_rx_chn, 0);
				LL_DMA_SetDataLength(dma_tx_ptr, dma_tx_chn, 0);
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
	MIOS32_SPI0_DMA_RX_PTR->IFCR |= MIOS32_SPI0_DMA_RX_IRQ_FLAGS;

	if( spi_callback[0] != NULL )
		spi_callback[0]();
}

#if defined(MIOS32_SPI_SHARED_DMA_VECTOR)
// SPI1 (DMA1) and SPI2 (DMA2) share this vector on this chip - check each
// DMA controller's own status flags to know which one actually fired before
// servicing/clearing it.
MIOS32_SPI1_DMA_IRQHANDLER_FUNC
{
#ifdef MIOS32_USE_SPI1
	if( DMA1->ISR & MIOS32_SPI1_DMA_RX_IRQ_FLAGS ) {
		MIOS32_SPI1_DMA_RX_PTR->IFCR |= MIOS32_SPI1_DMA_RX_IRQ_FLAGS;
		if( spi_callback[1] != NULL )
			spi_callback[1]();
	}
#endif
#ifdef MIOS32_USE_SPI2
	if( DMA2->ISR & MIOS32_SPI2_DMA_RX_IRQ_FLAGS ) {
		MIOS32_SPI2_DMA_RX_PTR->IFCR |= MIOS32_SPI2_DMA_RX_IRQ_FLAGS;
		if( spi_callback[2] != NULL )
			spi_callback[2]();
	}
#endif
}
#else
MIOS32_SPI1_DMA_IRQHANDLER_FUNC
{
	MIOS32_SPI1_DMA_RX_PTR->IFCR |= MIOS32_SPI1_DMA_RX_IRQ_FLAGS;

	if( spi_callback[1] != NULL )
		spi_callback[1]();
}
#endif

//! \}

#endif /* MIOS32_USE_SPI */
