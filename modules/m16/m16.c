//! \defgroup M16
//!
//! Driver for the M16 - an FPGA interface deploying 16 MIDI I/O over one
//! SPI-MIDI link.
//!
//! Split out of adios/common/adios_spi_midi.c on 2026-08-14. Everything
//! here used to live inside the transport behind ADIOS_SPI_MIDI_USE_M16
//! conditionals; the transport is now transparent and knows nothing about
//! what sits at the other end of the wire. The bodies below are moved
//! VERBATIM - only the two entry points at the end are new, and the
//! RS_Optimisation pair changed name.
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
#include "m16.h"

#if !defined(ADIOS_USE_SPI_MIDI)
# error "The M16 is reached over SPI-MIDI: add #define ADIOS_USE_SPI_MIDI to your adios_config.h."
#endif


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static u16 m16_rx_act;
static u16 m16_tx_act;
static u16 m16_ovl_act;
static adios_spim_m16_gpio_mode_t  m16_gpio_mode[3];
static u16 m16_gpio_inv[3];
static u16 m16_gpio_val[3];
static u16 rs_optimisation;

static s32 (*m16_stat_callback_func)(adios_spim_m16_cmd_t stat_cmd, u16 stat_val);


/////////////////////////////////////////////////////////////////////////////
// Local prototypes
/////////////////////////////////////////////////////////////////////////////

static s32 ADIOS_SPI_MIDI_M16_StatReceive(u32 word);
static s32 M16_RawWordCallback(u32 word);


/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
//! Installs an optional SysEx callback which is called by
//! ADIOS_SPI_MIDI_M16_StatReceive() to simplify the parsing of m16 statuses.
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_StatCallback_Init(s32 (*callback_m16_stat)(adios_spim_m16_cmd_t stat_cmd, u16 stat_val))
{
	m16_stat_callback_func = callback_m16_stat;

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_RxStatEnable(u8 enable){
	adios_midi_package_t p;
	p.cin_cable = 0x01;
	p.evnt0 = M16_CMD_RX_STAT;
	p.evnt1 = 0x00;
	p.evnt2 = enable;
	return ADIOS_SPI_MIDI_PackageSend(p);
}

/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_TxStatEnable(u8 enable){
	adios_midi_package_t p;
	p.cin_cable = 0x01;
	p.evnt0 = M16_CMD_TX_STAT;
	p.evnt1 = 0x00;
	p.evnt2 = enable;
	return ADIOS_SPI_MIDI_PackageSend(p);
}

/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_OvlStatEnable(u8 enable){
	adios_midi_package_t p;
	p.cin_cable = 0x01;
	p.evnt0 = M16_CMD_OVL_STAT;
	p.evnt1 = 0x00;
	p.evnt2 = enable;
	return ADIOS_SPI_MIDI_PackageSend(p);
}

/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPI_MIDI_M16_StatReceive(u32 word){
	u8 cmd = (u8)(word>>16);
	switch(cmd){
	case M16_CMD_RX_STAT:
		if((word & 0x0000ffff) != m16_rx_act){
		  m16_rx_act = (u16)(word & 0x0000ffff);
		  if( m16_stat_callback_func != NULL ) {
			m16_stat_callback_func(cmd, m16_rx_act);
		  }
		}
		break;
	case M16_CMD_TX_STAT:
		if((word & 0x0000ffff) != m16_tx_act){
		  m16_tx_act = (u16)(word & 0x0000ffff);
		  if( m16_stat_callback_func != NULL ) {
			m16_stat_callback_func(cmd, m16_tx_act);
		  }
		}
		break;
	case M16_CMD_OVL_STAT:
		if((word & 0x0000ffff) != m16_ovl_act){
		  m16_ovl_act = (u16)(word & 0x0000ffff);
		  if( m16_stat_callback_func != NULL ) {
			m16_stat_callback_func(cmd, m16_ovl_act);
		  }
		}
		break;
	case M16_CMD_GPIO_BASE:
		if((word & 0x0000ffff) != m16_gpio_val[0]){
		  m16_gpio_val[0] = (u16)(word & 0x0000ffff);
		  if( m16_stat_callback_func != NULL ) {
			m16_stat_callback_func(cmd, m16_gpio_val[0]);
		  }
		}
		break;
	case (M16_CMD_GPIO_BASE+0x10):
		if((word & 0x0000ffff) != m16_gpio_val[1]){
		  m16_gpio_val[1] = (u16)(word & 0x0000ffff);
		  if( m16_stat_callback_func != NULL ) {
			m16_stat_callback_func(cmd, m16_gpio_val[1]);
		  }
		}
		break;
	case (M16_CMD_GPIO_BASE+0x20):
		if((word & 0x0000ffff) != m16_gpio_val[2]){
		  m16_gpio_val[2] = (u16)(word & 0x0000ffff);
		  if( m16_stat_callback_func != NULL ) {
			m16_stat_callback_func(cmd, m16_gpio_val[2]);
		  }
		}
		break;
	default:
		break;
	}
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_GPIO_Grp_ModeSet(u8 gpio_grp, adios_spim_m16_gpio_mode_t mode){
	if(gpio_grp>2)return -1; //GPIO Group not available
	//set value
	m16_gpio_mode[gpio_grp]=mode;
	//send command
	adios_midi_package_t p;
	p.cin_cable = 0x01;
	p.evnt0 = (gpio_grp<<4)+ M16_CMD_GPIO_BASE + 0x02;
	p.evnt1 = 0x00;
	p.evnt2 = (u8)m16_gpio_mode[gpio_grp];
	ADIOS_SPI_MIDI_PackageSend(p);
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
/////////////////////////////////////////////////////////////////////////////
adios_spim_m16_gpio_mode_t ADIOS_SPIM_M16_GPIO_Grp_ModeGet(u8 gpio_grp){
	if(gpio_grp>2)return -1; //GPIO Group not available
	return 	m16_gpio_mode[gpio_grp]; // no error
}
/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_GPIO_Grp_InvSet(u8 gpio_grp,u16 value){
	if(gpio_grp>2)return -1; //GPIO Group not available
	//set value
	m16_gpio_inv[gpio_grp]=value;
	//send command
	adios_midi_package_t p;
	p.cin_cable = 0x01;
	p.evnt0 = (gpio_grp<<4)+ M16_CMD_GPIO_BASE + 0x01;
	p.evnt1 = (u8)(m16_gpio_inv[gpio_grp]>>8);
	p.evnt2 = (u8)(m16_gpio_inv[gpio_grp]&0xff);
	ADIOS_SPI_MIDI_PackageSend(p);
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_GPIO_Grp_InvGet(u8 gpio_grp){
	if(gpio_grp>2)return -1; //GPIO Group not available
	return 	m16_gpio_inv[gpio_grp]; // no error
}
/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
//! \param[in] GPIO Group (0...2) A to C
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_GPIO_Grp_Set(u8 gpio_grp,u16 value){
	if(gpio_grp>2)return -1; //GPIO Group not available
	//set value
	m16_gpio_val[gpio_grp]=value;
	//send command
	adios_midi_package_t p;
	p.cin_cable = 0x01;
	p.evnt0 = (gpio_grp<<4)+M16_CMD_GPIO_BASE;
	p.evnt1 = (u8)(m16_gpio_val[gpio_grp]>>8);
	p.evnt2 = (u8)(m16_gpio_val[gpio_grp]&0xff);
	ADIOS_SPI_MIDI_PackageSend(p);
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_GPIO_Grp_Get(u8 gpio_grp){
	if(gpio_grp>2)return -1; //GPIO Group not available
	return 	m16_gpio_val[gpio_grp]; // no error
}
/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_GPIO_InvSet(u8 gpio,u8 value){
	if(gpio>48)return -1; //GPIO Pin not available
	u8 gpio_grp = gpio>>4;
	u16 mask = 1 << (gpio &0x0f);
	m16_gpio_inv[gpio_grp] &= ~mask;
	if( value )m16_gpio_inv[gpio_grp] |= mask;
	return ADIOS_SPIM_M16_GPIO_Grp_InvSet(gpio_grp, m16_gpio_inv[gpio_grp]); // no error
}

/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_GPIO_InvGet(u8 gpio){
	if(gpio>48)return -1; //GPIO Pin not available
	u8 gpio_grp = gpio>>4;
	u16 mask = 1 << (gpio &0x0f);
	return ((m16_gpio_inv[gpio_grp] & mask)? 1 : 0);
}
/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_GPIO_Set(u8 gpio,u8 value){
	if(gpio>48)return -1; //GPIO Pin not available
	u8 gpio_grp = gpio>>4;
	u16 mask = 1 << (gpio &0x0f);
	m16_gpio_val[gpio_grp] &= ~mask;
	if( value )m16_gpio_val[gpio_grp] |= mask;
	return ADIOS_SPIM_M16_GPIO_Grp_InvSet(gpio_grp, m16_gpio_val[gpio_grp]); // no error
}

/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_GPIO_Get(u8 gpio){
	if(gpio>48)return -1; //GPIO Pin not available
	u8 gpio_grp = gpio>>4;
	u16 mask = 1 << (gpio &0x0f);
	return ((m16_gpio_val[gpio_grp] & mask)? 1 : 0);
}

/////////////////////////////////////////////////////////////////////////////
//! m16 specific function
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_SofEnable(u8 enable){
	adios_midi_package_t p;
	p.cin_cable = 0x01;
	p.evnt0 = M16_CMD_SOF_ENA;
	p.evnt1 = 0;
	p.evnt2 = enable;
	return ADIOS_SPI_MIDI_PackageSend(p);
}



/////////////////////////////////////////////////////////////////////////////
//! Running status optimisation.
//!
//! Was ADIOS_SPI_MIDI_RS_OptimisationSet/Get in the transport, whose entire
//! body was this: an M16 command carrying a 16-bit port mask. The transport
//! had no business implementing it, so it moved here with its name adjusted.
//! \param[in] spim_port the port (0..ADIOS_SPI_MIDI_NUM_PORTS-1)
//! \param[in] enable 0 to disable, 1 to enable
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_RS_OptimisationSet(u8 spim_port, u8 enable)
{
  if( spim_port >= ADIOS_SPI_MIDI_NUM_PORTS )
    return -1; // port not available

  u16 mask = 1 << spim_port;
  rs_optimisation &= ~mask;
  if( enable )rs_optimisation |= mask;
  adios_midi_package_t p;
  p.cin_cable = 0x01;
  p.evnt0 = 0x10;
  p.evnt1 = (u8)(rs_optimisation>>8);
  p.evnt2 = (u8)(rs_optimisation&0xff);
  ADIOS_SPI_MIDI_PackageSend(p);

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! \param[in] spim_port the port (0..ADIOS_SPI_MIDI_NUM_PORTS-1)
//! \return 1 if enabled, 0 if disabled, < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_RS_OptimisationGet(u8 spim_port)
{
  if( spim_port >= ADIOS_SPI_MIDI_NUM_PORTS )
    return -1; // port not available

  return (rs_optimisation & (1 << spim_port)) ? 1 : 0;
}


/////////////////////////////////////////////////////////////////////////////
//! The hook the transport calls for every received word, before it parses
//! anything. The M16 answers status on CIN 0x01; everything else is real
//! MIDI and must be let through.
//! \return 1 if the word was consumed, 0 to let the transport handle it
/////////////////////////////////////////////////////////////////////////////
static s32 M16_RawWordCallback(u32 word)
{
  if( (word & 0x0f000000) == 0x01000000 ) {
    ADIOS_SPI_MIDI_M16_StatReceive(word);
    return 1; // consumed
  }

  return 0; // not ours
}


/////////////////////////////////////////////////////////////////////////////
//! Initializes the M16. Call once, AFTER ADIOS_SPI_MIDI_Init().
//!
//! This is what the transport's own Init() used to do for the board behind
//! an #ifdef. It also registers the raw-word hook, which is how the status
//! channel reaches the parser now that the transport carries no M16 code.
//! \param[in] mode currently only mode 0 supported
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPIM_M16_Init(u32 mode)
{
  if( mode != 0 )
    return -1; // unsupported mode

  m16_stat_callback_func = NULL;
  rs_optimisation = 0;

  ADIOS_SPI_MIDI_RawWordCallback_Init(M16_RawWordCallback);

  int i;
  ADIOS_SPIM_M16_RxStatEnable(1); 	// Set RX status On
  ADIOS_SPIM_M16_TxStatEnable(1);	// Set TX status On
  ADIOS_SPIM_M16_OvlStatEnable(1);	// Set TX buffer Overload status Off
  for(i=0;i<3; i++){
	  // All GPIO Groups set to OUT and not inverted by default
	  ADIOS_SPIM_M16_GPIO_Grp_ModeSet(i, M16_GPIO_MODE_OUT);
	  ADIOS_SPIM_M16_GPIO_Grp_InvSet(i, 0x0000);
	  ADIOS_SPIM_M16_GPIO_Grp_Set(i, 0x0000);
  }
  ADIOS_SPIM_M16_SofEnable(0);

  return 0; // no error
}

//! \}
