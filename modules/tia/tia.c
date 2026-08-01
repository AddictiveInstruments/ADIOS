// $Id: tia.c 1769 2013-05-02 19:45:09Z tk $
//! \defgroup TIA
//!
//! MAX72xx module driver
//!
//! \{
/* ==========================================================================
 *
 *  Copyright (C) 2013 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>

#include "tia.h"


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

//static u8 num_used_chains = TIA_NUM_CHAINS;
//static u8 chain_enable_mask;
//static u8 num_used_devices_per_chain[TIA_NUM_CHAINS];
//
//static u8 tia_digits[TIA_NUM_CHAINS][TIA_NUM_DEVICES_PER_CHAIN][8];


/////////////////////////////////////////////////////////////////////////////
// Local Prototypes
/////////////////////////////////////////////////////////////////////////////

static s32 TIA_SetCs(u8 device, u8 value);
static u8 reverse(u8 n);

/////////////////////////////////////////////////////////////////////////////
//! Initializes MAX72xx driver
//! Should be called from Init() during startup
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 TIA_Init(u32 mode)
{
  s32 status = 0;
  int chain, device;

  // currently only mode 0 supported
  if( mode != 0 )
    return -1; // unsupported mode

#if TIA_SPI_OUTPUTS_OD
  // pins in open drain mode (to pull-up the outputs to 5V)
  status |= MIOS32_SPI_IO_Init(TIA_SPI, MIOS32_SPI_PIN_DRIVER_STRONG_OD);
#else
  // pins in push-poll mode (3.3V output voltage)
  status |= MIOS32_SPI_IO_Init(TIA_SPI, MIOS32_SPI_PIN_DRIVER_STRONG);
#endif

  // ensure that CS is deactivated
  MIOS32_SPI_RC_PinSet(TIA_SPI, TIA_SPI_RC_PIN, 1); // spi, rc_pin, pin_value
  
  
  // ensure that CS is deactivated
  MIOS32_SPI_RC_PinSet(TIA_SPI, TIA_SPI_RW_PIN, 1); // spi, rc_pin, pin_value
  
  
//  // SPI Port will be initialized in TIA_Update()
//
//  num_used_chains = TIA_NUM_CHAINS;
//#if TIA_NUM_MODULES > 8
//# error "If more than 8 TIA_NUM_CHAINS should be supported, the chain_enable_mask variable type has to be changed from u8 to u16 (up to 16) or u32 (up to 32)"
//#endif
//
//  for(chain=0; chain<TIA_NUM_CHAINS; ++chain) {
//    num_used_devices_per_chain[chain] = TIA_NUM_DEVICES_PER_CHAIN;
//
//    // ensure that CS is deactivated
//    TIA_SetCs(chain, 1);
//
//    TIA_EnabledSet(chain, 1);
//    TIA_NumDevicesPerChainSet(chain, TIA_NUM_DEVICES_PER_CHAIN);
//
//    // clear all digits
//    for(device=0; device<TIA_NUM_DEVICES_PER_CHAIN; ++device) {
//      int i;
//      for(i=0; i<8; ++i) {
//	tia_digits[chain][device][i] = 0;
//      }
//    }
//
//    // enter normal operation mode
//    TIA_WriteAllRegs(chain, TIA_REG_SHUTDOWN, 0x01);
//
//    // set decode mode to 0 (no decoding)
//    TIA_WriteAllRegs(chain, TIA_REG_DECODE_MODE, 0x00);
//
//    // set maximum intensity
//    TIA_WriteAllRegs(chain, TIA_REG_INTENSITY, 0x0f);
//
//    // scan all digits
//    TIA_WriteAllRegs(chain, TIA_REG_SCAN_LIMIT, 0x07);
//
//    // ensure that display test mode disabled
//    TIA_WriteAllRegs(chain, TIA_REG_TESTMODE, 0x00);
//
//    // update the digits
//    TIA_UpdateAllDigits(chain);
//  }

  return status;
}

/////////////////////////////////////////////////////////////////////////////
//! Load a 16bit value into a selected TIA device of the given chain
/////////////////////////////////////////////////////////////////////////////
s32 TIA_WriteReg(u8 device, u8 reg, u8 value)
{
  s32 status = 0;

  if( device >= TIA_NUM )
    return -1; // invalid device
  reg = reg<<4;
  value = reverse(value);
  // init SPI port for fast frequency access
  // we will do this here, so that other handlers (e.g. AOUT) could use SPI in different modes
  // Maxmimum allowed SCLK is 10 MHz according to datasheet
  // We select prescaler 32 @120 MHz (-> ca. 250 nS period)
  status |= MIOS32_SPI_TransferModeInit(TIA_SPI, MIOS32_SPI_MODE_CLK0_PHASE0, MIOS32_SPI_PRESCALER_64);



  // shift data
  status |= MIOS32_SPI_TransferByte(TIA_SPI, value);
  status |= MIOS32_SPI_TransferByte(TIA_SPI, reg);

  // activate chip select
  status |= MIOS32_SPI_RC_PinSet(TIA_SPI, TIA_SPI_RC_PIN, 0);
  // deactivate chip select (resp. load shifted values)
  MIOS32_SPI_RC_PinSet(TIA_SPI, TIA_SPI_RC_PIN, 1);

  return status;
}

/////////////////////////////////////////////////////////////////////////////
//! Load a 8bit value into all TIA devices of the given chain
/////////////////////////////////////////////////////////////////////////////
s32 TIA_WriteAllRegs(u8 device, u8 reg, u8 value)
{
  s32 status = 0;
//
//  if( chain >= num_used_chains || !(chain_enable_mask & (1 << chain)) )
//    return -1; // invalid chain
//
//  // init SPI port for fast frequency access
//  // we will do this here, so that other handlers (e.g. AOUT) could use SPI in different modes
//  // Maxmimum allowed SCLK is 10 MHz according to datasheet
//  // We select prescaler 32 @120 MHz (-> ca. 250 nS period)
//  status |= MIOS32_SPI_TransferModeInit(TIA_SPI, MIOS32_SPI_MODE_CLK0_PHASE0, MIOS32_SPI_PRESCALER_32);
//
//  // activate chip select
//  status |= TIA_SetCs(chain, 0);
//
//  // shift data
//  int i;
//  for(i=num_used_devices_per_chain[chain]-1; i>=0; --i) {
//    status |= MIOS32_SPI_TransferByte(TIA_SPI, reg);
//    status |= MIOS32_SPI_TransferByte(TIA_SPI, value);
//  }
//
//  // deactivate chip select (resp. load shifted values)
//  status |= TIA_SetCs(chain, 1);

  return status;
}


///////////////////////////////////////////////////////////////////////////////
////! Updates a digit (0..7) of the given chain
///////////////////////////////////////////////////////////////////////////////
//s32 TIA_UpdateDigit(u8 chain, u8 digit)
//{
//  s32 status = 0;
//
//  if( chain >= num_used_chains || !(chain_enable_mask & (1 << chain)) )
//    return -1; // invalid chain
//
//  if( digit >= 8 )
//    return -2; // invalid digit
//
//  // init SPI port for fast frequency access
//  // we will do this here, so that other handlers (e.g. AOUT) could use SPI in different modes
//  // Maxmimum allowed SCLK is 10 MHz according to datasheet
//  // We select prescaler 32 @120 MHz (-> ca. 250 nS period)
//  status |= MIOS32_SPI_TransferModeInit(TIA_SPI, MIOS32_SPI_MODE_CLK0_PHASE0, MIOS32_SPI_PRESCALER_32);
//
//  // activate chip select
//  status |= TIA_SetCs(chain, 0);
//
//  // shift data
//  int i;
//  for(i=num_used_devices_per_chain[chain]-1; i>=0; --i) {
//    status |= MIOS32_SPI_TransferByte(TIA_SPI, TIA_REG_DIGIT0 + digit);
//    status |= MIOS32_SPI_TransferByte(TIA_SPI, tia_digits[chain][i][digit]);
//  }
//
//  // deactivate chip select (resp. load shifted values)
//  status |= TIA_SetCs(chain, 1);
//
//  return status;
//}
//
//
///////////////////////////////////////////////////////////////////////////////
////! Updates all digits of the given chain
///////////////////////////////////////////////////////////////////////////////
//s32 TIA_UpdateAllDigits(u8 chain)
//{
//  s32 status = 0;
//
//  if( chain >= num_used_chains || !(chain_enable_mask & (1 << chain)) )
//    return -1; // invalid chain
//
//  int digit;
//  for(digit=0; digit<8; ++digit) {
//    status |= TIA_UpdateDigit(chain, digit);
//  }
//
//  return status;
//}
//
//
///////////////////////////////////////////////////////////////////////////////
////! Updates all chains
///////////////////////////////////////////////////////////////////////////////
//s32 TIA_UpdateAllChains(void)
//{
//  s32 status = 0;
//
//  int chain;
//  for(chain=0; chain<num_used_chains; ++chain) {
//    status |= TIA_UpdateAllDigits(chain);
//  }
//
//  return status;
//}


/////////////////////////////////////////////////////////////////////////////
// Internal function to set CS line depending on chain
/////////////////////////////////////////////////////////////////////////////
static s32 TIA_SetCs(u8 device, u8 value)
{
//  switch( chain ) {
//  case 0: return MIOS32_SPI_RC_PinSet(TIA_SPI, TIA_SPI_RC_PIN, value); // spi, rc_pin, pin_value
//  case 1: return MIOS32_SPI_RC_PinSet(TIA_SPI, TIA_SPI_RW_PIN, value); // spi, rc_pin, pin_value
//
//#if TIA_NUM_CHAINS > 2
//# error "CS Line for more than 2 chains not prepared yet - please enhance here!"
//#endif
//  }


  return -1;
}



u8 reverse(u8 n) {
  const u8 lookup[16] = {
  0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
  0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf, };
   // Reverse the top and bottom nibble then swap them.
  return (lookup[n&0xf] << 4) | lookup[n>>4];
}
//! \}
