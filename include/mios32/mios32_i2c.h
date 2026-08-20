/*
 * Header file for the I2C driver
 *
 * Master-mode transfers on the chip's I2C ports: address a slave, send it
 * bytes, read bytes back, either blocking until done or driven by interrupt.
 *
 *
 * HOW TO USE IT
 * =============
 *
 * 1. In your mios32_config.h, name the port you want. MIOS32_I2Cn is
 *    I2C(n+1), so port 0 is I2C1:
 *
 *      #define MIOS32_USE_I2C0 1
 *
 *    MIOS32_USE_I2C derives itself from that, and the port is initialised
 *    for you. Which ports exist depends on the chip; asking for one it does
 *    not have is answered at compile time by the family driver, which also
 *    lists the pins each port can use.
 *
 * 2. Take the port, do the transfer, release it. The claim matters as soon
 *    as two parts of your code share one bus:
 *
 *      u8 buf[2] = { 0x10, 0x42 };
 *
 *      MIOS32_I2C_TransferBegin(MIOS32_I2C_PORT_I2C0, I2C_Blocking);
 *      MIOS32_I2C_Transfer(MIOS32_I2C_PORT_I2C0,
 *                          MIOS32_I2C_WRITE, 0x50 << 1, buf, 2);
 *      s32 err = MIOS32_I2C_TransferWait(MIOS32_I2C_PORT_I2C0);
 *      MIOS32_I2C_TransferFinished(MIOS32_I2C_PORT_I2C0);
 *
 *    The slave address is passed SHIFTED LEFT BY ONE - it is the 8-bit
 *    address as it appears on the wire, not the 7-bit number printed in a
 *    datasheet table. The read/write bit comes from the transfer type.
 *
 * 3. Check what happened: every call returns < 0 on failure, and the error
 *    codes below name the stage. MIOS32_I2C_ERROR_TIMEOUT means the bus is
 *    stuck - a slave holding SCL, or missing pull-ups. SLAVE_NOT_CONNECTED
 *    means nobody acknowledged that address.
 *
 * Pass I2C_Non_Blocking instead to have the transfer run on interrupt;
 * MIOS32_I2C_TransferCheck() then says whether it is still going, and
 * MIOS32_I2C_TransferWait() blocks until it is not.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _MIOS32_I2C_H
#define _MIOS32_I2C_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// Auto-derive the master switch from any individual port actually wanted -
// no need for the project to set MIOS32_USE_I2C on top of MIOS32_USE_I2C0/1/2.
// Same reasoning (and same placement) as MIOS32_USE_SPI in mios32_spi.h:
// programming_models/traditional/main.c tests the bare master switch to
// decide whether to call MIOS32_I2C_Init(), and a #define made inside
// mios32_i2c.c would never reach that separate translation unit.
#if !defined(MIOS32_USE_I2C) && (defined(MIOS32_USE_I2C0) || defined(MIOS32_USE_I2C1) || defined(MIOS32_USE_I2C2) || defined(MIOS32_USE_FMPI2C0))
#define MIOS32_USE_I2C
#endif


// Port indices accepted by every function below. Which of them actually
// exist is decided by the family driver from the processor: see the port
// tables at the head of mios32/<FAMILY>/mios32_i2c.c. Requesting a port the
// chip doesn't have is a compile-time #undef of your MIOS32_USE_I2Cn, not a
// runtime surprise.
//
// FMPI2C1 (Fast-mode Plus I2C, present on STM32F410/412/413/423/446 only) is
// deliberately NOT called "I2C3": it is a different generation of peripheral
// (v2, the same silicon as the G0's I2C), and on the F410 there is no I2C3
// at all, so numbering it in sequence would leave a hole. It keeps ST's own
// name and a port index of its own.
#define MIOS32_I2C_PORT_I2C0    0
#define MIOS32_I2C_PORT_I2C1    1
#define MIOS32_I2C_PORT_I2C2    2
#define MIOS32_I2C_PORT_FMPI2C0 3


// Timeout for a complete transfer, in units of ~1 uS. Reached only when the
// bus is stuck (a slave holding SCL low, missing pull-ups, a short); a
// healthy transfer of 4 bytes at 100 kHz takes ca. 400 uS.
#ifndef MIOS32_I2C_TIMEOUT_VALUE
#define MIOS32_I2C_TIMEOUT_VALUE 5000
#endif


// Error codes returned by MIOS32_I2C_TransferCheck()/Wait()/LastErrorGet().
#define MIOS32_I2C_ERROR_INVALID_PORT               -1
#define MIOS32_I2C_ERROR_GENERAL                    -2
#define MIOS32_I2C_ERROR_UNSUPPORTED_TRANSFER_TYPE  -3
#define MIOS32_I2C_ERROR_TIMEOUT                    -4
#define MIOS32_I2C_ERROR_ARBITRATION_LOST           -5
#define MIOS32_I2C_ERROR_BUS                        -6
#define MIOS32_I2C_ERROR_SLAVE_NOT_CONNECTED        -7
#define MIOS32_I2C_ERROR_UNEXPECTED_EVENT           -8
#define MIOS32_I2C_ERROR_RX_BUFFER_OVERRUN          -9
#define MIOS32_I2C_ERROR_TRANSFER_TOO_LONG          -10

// If a previous transfer failed, this offset is added to the error status
// reported for the next one.
#define MIOS32_I2C_ERROR_PREV_OFFSET -128


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////

// Enum members carry the MIOS32_I2C_ prefix on purpose: bare names like
// I2C_Read sit in the same namespace as the CMSIS device header, which
// defines I2C1, I2C_TypeDef and a few hundred I2C_* register macros. The
// UART -> ACIA rename was forced by exactly this kind of collision.
typedef enum {
  MIOS32_I2C_BLOCKING,
  MIOS32_I2C_NON_BLOCKING
} mios32_i2c_semaphore_t;

typedef enum {
  MIOS32_I2C_READ,
  MIOS32_I2C_WRITE,
  MIOS32_I2C_WRITE_WITHOUT_STOP   // repeated start follows (register addressing)
} mios32_i2c_transfer_t;


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_I2C_Init(u32 mode);

extern s32 MIOS32_I2C_TransferBegin(u8 i2c_port, mios32_i2c_semaphore_t semaphore_type);
extern s32 MIOS32_I2C_TransferFinished(u8 i2c_port);

extern s32 MIOS32_I2C_Transfer(u8 i2c_port, mios32_i2c_transfer_t transfer, u8 address, u8 *buffer, u16 len);
extern s32 MIOS32_I2C_TransferCheck(u8 i2c_port);
extern s32 MIOS32_I2C_TransferWait(u8 i2c_port);

extern s32 MIOS32_I2C_LastErrorGet(u8 i2c_port);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#endif /* _MIOS32_I2C_H */
