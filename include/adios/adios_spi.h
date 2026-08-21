/*
 * Header file for SPI Driver
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _ADIOS_SPI_H
#define _ADIOS_SPI_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// auto-derive the master switch from any individual port actually wanted -
// no need for the project to separately set ADIOS_USE_SPI on top of
// ADIOS_USE_SPI0/1/2. Has to live HERE (a shared header, included by every
// translation unit via adios.h) rather than locally inside adios_spi.c:
// core/main.c also checks the bare ADIOS_USE_SPI
// macro directly (to decide whether to call ADIOS_SPI_Init()) - a #define
// added only inside adios_spi.c's own .c file would never be visible there
// (separate translation unit, macros don't cross .c file boundaries).
#if !defined(ADIOS_USE_SPI) && (defined(ADIOS_USE_SPI0) || defined(ADIOS_USE_SPI1) || defined(ADIOS_USE_SPI2))
#define ADIOS_USE_SPI
#endif


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////

typedef enum {
  ADIOS_SPI_PIN_DRIVER_STRONG=0,
  ADIOS_SPI_PIN_DRIVER_STRONG_OD=1,
  ADIOS_SPI_PIN_DRIVER_WEAK=2,
  ADIOS_SPI_PIN_DRIVER_WEAK_OD=3,

  ADIOS_SPI_PIN_SLAVE_DRIVER_STRONG=4,
  ADIOS_SPI_PIN_SLAVE_DRIVER_STRONG_OD=5,
  ADIOS_SPI_PIN_SLAVE_DRIVER_WEAK=6,
  ADIOS_SPI_PIN_SLAVE_DRIVER_WEAK_OD=7,
} adios_spi_pin_driver_t;

typedef enum {
  ADIOS_SPI_MODE_CLK0_PHASE0=0,
  ADIOS_SPI_MODE_CLK0_PHASE1=1,
  ADIOS_SPI_MODE_CLK1_PHASE0=2,
  ADIOS_SPI_MODE_CLK1_PHASE1=3,

  ADIOS_SPI_MODE_SLAVE_CLK0_PHASE0=4,
  ADIOS_SPI_MODE_SLAVE_CLK0_PHASE1=5,
  ADIOS_SPI_MODE_SLAVE_CLK1_PHASE0=6,
  ADIOS_SPI_MODE_SLAVE_CLK1_PHASE1=7
} adios_spi_mode_t;

typedef enum {
  ADIOS_SPI_PRESCALER_2=0,
  ADIOS_SPI_PRESCALER_4=1,
  ADIOS_SPI_PRESCALER_8=2,
  ADIOS_SPI_PRESCALER_16=3,
  ADIOS_SPI_PRESCALER_32=4,
  ADIOS_SPI_PRESCALER_64=5,
  ADIOS_SPI_PRESCALER_128=6,
  ADIOS_SPI_PRESCALER_256=7
} adios_spi_prescaler_t;


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 ADIOS_SPI_Init(u32 mode);

extern s32 ADIOS_SPI_IO_Init(u8 spi, adios_spi_pin_driver_t spi_pin_driver);
extern s32 ADIOS_SPI_TransferModeInit(u8 spi, adios_spi_mode_t spi_mode, adios_spi_prescaler_t spi_prescaler);


extern s32 ADIOS_SPI_CS_PinSet(u8 spi, u8 pin_value);
extern s32 ADIOS_SPI_TransferByte(u8 spi, u8 b);
extern s32 ADIOS_SPI_TransferBlock(u8 spi, u8 *send_buffer, u8 *receive_buffer, u16 len, void *callback);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#endif /* _ADIOS_SPI_H */
