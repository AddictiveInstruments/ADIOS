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

#ifndef _MIOS32_SPI_H
#define _MIOS32_SPI_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// auto-derive the master switch from any individual port actually wanted -
// no need for the project to separately set MIOS32_USE_SPI on top of
// MIOS32_USE_SPI0/1/2. Has to live HERE (a shared header, included by every
// translation unit via mios32.h) rather than locally inside mios32_spi.c:
// core/main.c also checks the bare MIOS32_USE_SPI
// macro directly (to decide whether to call MIOS32_SPI_Init()) - a #define
// added only inside mios32_spi.c's own .c file would never be visible there
// (separate translation unit, macros don't cross .c file boundaries).
#if !defined(MIOS32_USE_SPI) && (defined(MIOS32_USE_SPI0) || defined(MIOS32_USE_SPI1) || defined(MIOS32_USE_SPI2))
#define MIOS32_USE_SPI
#endif


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////

typedef enum {
  MIOS32_SPI_PIN_DRIVER_STRONG=0,
  MIOS32_SPI_PIN_DRIVER_STRONG_OD=1,
  MIOS32_SPI_PIN_DRIVER_WEAK=2,
  MIOS32_SPI_PIN_DRIVER_WEAK_OD=3,

  MIOS32_SPI_PIN_SLAVE_DRIVER_STRONG=4,
  MIOS32_SPI_PIN_SLAVE_DRIVER_STRONG_OD=5,
  MIOS32_SPI_PIN_SLAVE_DRIVER_WEAK=6,
  MIOS32_SPI_PIN_SLAVE_DRIVER_WEAK_OD=7,
} mios32_spi_pin_driver_t;

typedef enum {
  MIOS32_SPI_MODE_CLK0_PHASE0=0,
  MIOS32_SPI_MODE_CLK0_PHASE1=1,
  MIOS32_SPI_MODE_CLK1_PHASE0=2,
  MIOS32_SPI_MODE_CLK1_PHASE1=3,

  MIOS32_SPI_MODE_SLAVE_CLK0_PHASE0=4,
  MIOS32_SPI_MODE_SLAVE_CLK0_PHASE1=5,
  MIOS32_SPI_MODE_SLAVE_CLK1_PHASE0=6,
  MIOS32_SPI_MODE_SLAVE_CLK1_PHASE1=7
} mios32_spi_mode_t;

typedef enum {
  MIOS32_SPI_PRESCALER_2=0,
  MIOS32_SPI_PRESCALER_4=1,
  MIOS32_SPI_PRESCALER_8=2,
  MIOS32_SPI_PRESCALER_16=3,
  MIOS32_SPI_PRESCALER_32=4,
  MIOS32_SPI_PRESCALER_64=5,
  MIOS32_SPI_PRESCALER_128=6,
  MIOS32_SPI_PRESCALER_256=7
} mios32_spi_prescaler_t;


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_SPI_Init(u32 mode);

extern s32 MIOS32_SPI_IO_Init(u8 spi, mios32_spi_pin_driver_t spi_pin_driver);
extern s32 MIOS32_SPI_TransferModeInit(u8 spi, mios32_spi_mode_t spi_mode, mios32_spi_prescaler_t spi_prescaler);


extern s32 MIOS32_SPI_CS_PinSet(u8 spi, u8 pin_value);
extern s32 MIOS32_SPI_TransferByte(u8 spi, u8 b);
extern s32 MIOS32_SPI_TransferBlock(u8 spi, u8 *send_buffer, u8 *receive_buffer, u16 len, void *callback);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#endif /* _MIOS32_SPI_H */
