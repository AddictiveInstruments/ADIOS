// $Id: mios32_uart_midi.c 2312 2016-02-27 23:04:51Z tk $
//! \defgroup MIOS32_DIN_MIDI
//!
//! UART MIDI layer for MIOS32
//!
//! Applications shouldn't call these functions directly, instead please use \ref MIOS32_MIDI layer functions
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
#if defined(MIOS32_USE_DIN_MIDI)

/////////////////////////////////////////////////////////////////////////////
// Local Types
/////////////////////////////////////////////////////////////////////////////

typedef struct {
  mios32_midi_package_t package;
  u8 running_status;
  u8 expected_bytes;
  u8 wait_bytes;
  u8 sysex_ctr;
  u16 timeout_ctr;
} midi_rec_t;

// per-port MIDI parser state (midi_rec) plus the running-status-optimisation
// state (rs_last/rs_expire_ctr, previously 2 separate arrays) - grouped into
// one struct so a single named-instance + single pointer-table per port
// covers all of it, instead of 3 parallel tables.
typedef struct {
  midi_rec_t rec;
  u8 rs_last;
  u16 rs_expire_ctr;
} din_midi_state_t;


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

// fixed pointer-table size matching the full DIN0..DIN15 port range
// (mios32_midi.h) - this file is family-agnostic (no MIOS32_UART_MAX_PORTS
// visible here, that's a *local* #define inside each family's own
// mios32_uart.c, a separate translation unit) so it can't size this table
// to a smaller, family-specific bound. A handful of always-NULL slots for
// ports that don't exist on a given chip cost only a few bytes each (just
// the pointer); the real savings come from not allocating the struct itself
// for any port not compiled in via MIOS32_USE_UARTx.
#define MIOS32_DIN_MIDI_MAX_PORTS 16

#if defined(MIOS32_USE_UART0)
static din_midi_state_t din0_midi_state;
#endif
#if defined(MIOS32_USE_UART1)
static din_midi_state_t din1_midi_state;
#endif
#if defined(MIOS32_USE_UART2)
static din_midi_state_t din2_midi_state;
#endif
#if defined(MIOS32_USE_UART3)
static din_midi_state_t din3_midi_state;
#endif
#if defined(MIOS32_USE_UART4)
static din_midi_state_t din4_midi_state;
#endif
#if defined(MIOS32_USE_UART5)
static din_midi_state_t din5_midi_state;
#endif
#if defined(MIOS32_USE_UART6)
static din_midi_state_t din6_midi_state;
#endif
#if defined(MIOS32_USE_UART7)
static din_midi_state_t din7_midi_state;
#endif
#if defined(MIOS32_USE_UART8)
static din_midi_state_t din8_midi_state;
#endif
#if defined(MIOS32_USE_UART9)
static din_midi_state_t din9_midi_state;
#endif
#if defined(MIOS32_USE_UART10)
static din_midi_state_t din10_midi_state;
#endif
#if defined(MIOS32_USE_UART11)
static din_midi_state_t din11_midi_state;
#endif
#if defined(MIOS32_USE_UART12)
static din_midi_state_t din12_midi_state;
#endif
#if defined(MIOS32_USE_UART13)
static din_midi_state_t din13_midi_state;
#endif
#if defined(MIOS32_USE_UART14)
static din_midi_state_t din14_midi_state;
#endif
#if defined(MIOS32_USE_UART15)
static din_midi_state_t din15_midi_state;
#endif

static din_midi_state_t * const din_midi_state_ptr[MIOS32_DIN_MIDI_MAX_PORTS] = {
#if defined(MIOS32_USE_UART0)
  &din0_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART1)
  &din1_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART2)
  &din2_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART3)
  &din3_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART4)
  &din4_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART5)
  &din5_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART6)
  &din6_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART7)
  &din7_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART8)
  &din8_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART9)
  &din9_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART10)
  &din10_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART11)
  &din11_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART12)
  &din12_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART13)
  &din13_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART14)
  &din14_midi_state,
#else
  NULL,
#endif
#if defined(MIOS32_USE_UART15)
  &din15_midi_state,
#else
  NULL,
#endif
};

// running status optimisation enable/disable bitmask - one bit per port,
// widened from the original u8 (only ever safe for 8 ports) to u16 since
// this file now covers up to MIOS32_DIN_MIDI_MAX_PORTS (16) ports; a u8
// would silently truncate "1 << din_port" to 0 for any port >= 8, breaking
// RS optimisation for those ports without any compile-time warning.
static u16 rs_optimisation;

u8 din_midi_act;	// those flags must be cleared once read

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


// internal function to reset the record structure
static s32 MIOS32_DIN_MIDI_RecordReset(u8 din_port)
{
  if( din_port >= MIOS32_DIN_MIDI_MAX_PORTS || din_midi_state_ptr[din_port] == NULL )
    return -1; // port not available

  midi_rec_t *midix = &din_midi_state_ptr[din_port]->rec;

  midix->package.ALL = 0;
  midix->running_status = 0x00;
  midix->expected_bytes = 0x00;
  midix->wait_bytes = 0x00;
  midix->sysex_ctr = 0x00;
  midix->timeout_ctr = 0;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Initializes UART MIDI layer
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
//! \note Applications shouldn't call this function directly, instead please use \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DIN_MIDI_Init(u32 mode)
{
  int i;

  // currently only mode 0 supported
  if( mode != 0 )
    return -1; // unsupported mode

  // initialize MIDI record (RecordReset/RS_Reset silently skip any port
  // slot that's NULL - i.e. not compiled in via MIOS32_USE_UARTx)
  for(i=0; i<MIOS32_DIN_MIDI_MAX_PORTS; ++i)
    MIOS32_DIN_MIDI_RecordReset(i);

  // enable running status optimisation by default for all ports
  // clear timeout counters
  rs_optimisation = ~0; // -> all-one
  for(i=0; i<MIOS32_DIN_MIDI_MAX_PORTS; ++i)
    MIOS32_DIN_MIDI_RS_Reset(i);

  // this module is only compiled in if MIOS32_USE_DIN_MIDI is set, so the
  // U(S)ART interface is always wanted here - no need to gate on individual
  // port ASSIGNMENT values (they all default to 1 regardless of which ports
  // are actually enabled at the driver level, see mios32_uart.h)
  if( MIOS32_UART_Init(0) < 0 )
    return -1; // initialisation of U(S)ART Interface failed

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! This function can be used to determine, if a UART interface is available
//! \param[in] din_port UART number (0..15)
//! \return 1: interface available
//! \return 0: interface not available
//! \note Applications shouldn't call this function directly, instead please use \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DIN_MIDI_CheckAvailable(u8 din_port)
{
  return MIOS32_UART_IsAssignedToMIDI(din_port) >= 1; // UART assigned to MIDI?
}


/////////////////////////////////////////////////////////////////////////////
//! This function enables/disables running status optimisation for a given
//! MIDI OUT port to improve bandwidth if MIDI events with the same
//! status byte are sent back-to-back.<BR>
//! Note that the optimisation is enabled by default.
//! \param[in] din_port UART number (0..15)
//! \param[in] enable 0=optimisation disabled, 1=optimisation enabled
//! \return -1 if port not available
//! \return 0 on success
//! \note Applications shouldn't call this function directly, instead please use \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DIN_MIDI_RS_OptimisationSet(u8 din_port, u8 enable)
{
  if( din_port >= MIOS32_DIN_MIDI_MAX_PORTS || din_midi_state_ptr[din_port] == NULL )
    return -1; // port not available

  u16 mask = 1 << din_port;
  rs_optimisation &= ~mask;
  if( enable )
    rs_optimisation |= mask;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! This function returns the running status optimisation enable/disable flag
//! for the given MIDI OUT port.
//! \param[in] din_port UART number (0..15)
//! \return -1 if port not available
//! \return 0 if optimisation disabled
//! \return 1 if optimisation enabled
//! \note Applications shouldn't call this function directly, instead please use \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DIN_MIDI_RS_OptimisationGet(u8 din_port)
{
  if( din_port >= MIOS32_DIN_MIDI_MAX_PORTS || din_midi_state_ptr[din_port] == NULL )
    return -1; // port not available

  return (rs_optimisation & (1 << din_port)) ? 1 : 0;
}


/////////////////////////////////////////////////////////////////////////////
//! This function resets the current running status, so that it will be sent
//! again with the next MIDI Out package.
//! \param[in] din_port UART number (0..15)
//! \return -1 if port not available
//! \return < 0 on errors
//! \note Applications shouldn't call this function directly, instead please use \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DIN_MIDI_RS_Reset(u8 din_port)
{
  if( din_port >= MIOS32_DIN_MIDI_MAX_PORTS || din_midi_state_ptr[din_port] == NULL )
    return -1; // port not available

  MIOS32_IRQ_Disable();
  din_midi_state_ptr[din_port]->rs_last = 0xff;
  din_midi_state_ptr[din_port]->rs_expire_ctr = 0;
  MIOS32_IRQ_Enable();

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! This function should be called periodically each mS to handle timeout
//! and expire counters.
//!
//! Not for use in an application - this function is called from
//! MIOS32_MIDI_Periodic_mS(), which is called by a task in the programming
//! model!
//!
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DIN_MIDI_Periodic_mS(void)
{
  u8 din_port;

  MIOS32_IRQ_Disable();
  for(din_port=0; din_port<MIOS32_DIN_MIDI_MAX_PORTS; ++din_port) {
    din_midi_state_t *state = din_midi_state_ptr[din_port];
    if( state == NULL )
      continue;

    // increment the expire counters for running status optimisation.
    //
    // The running status will expire after 1000 ticks (1 second)
    // to ensure, that the current status will be sent at least each second
    // to cover the case that the MIDI cable is (re-)connected during runtime.
    if( state->rs_expire_ctr < 65535 )
      ++state->rs_expire_ctr;

    // increment timeout counter for incoming packages
    // an incomplete event will be timed out after 1000 ticks (1 second)
    if( state->rec.timeout_ctr < 65535 )
      ++state->rec.timeout_ctr;
  }
  MIOS32_IRQ_Enable();
  // (atomic operation not required in MIOS32_DIN_MIDI_PackageSend_NonBlocking() due to single-byte accesses)

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! This function sends a new MIDI package to the selected DIN_MIDI port
//! \param[in] din_port DIN_MIDI module number (0..15)
//! \param[in] package MIDI package
//! \return 0: no error
//! \return -1: DIN_MIDI device not available
//! \return -2: DIN_MIDI buffer is full
//!             caller should retry until buffer is free again
//! \note Applications shouldn't call this function directly, instead please use \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DIN_MIDI_PackageSend_NonBlocking(u8 din_port, mios32_midi_package_t package)
{
  // exit if UART port not available
  if( !MIOS32_DIN_MIDI_CheckAvailable(din_port) )
    return -1;
  if( din_port >= MIOS32_DIN_MIDI_MAX_PORTS || din_midi_state_ptr[din_port] == NULL )
    return -1;

  din_midi_state_t *state = din_midi_state_ptr[din_port];

  u8 len = mios32_midi_pcktype_num_bytes[package.cin];
  if( len ) {
    u8 buffer[3] = {package.evnt0, package.evnt1, package.evnt2};

    if( state->rs_expire_ctr > 1000 ) {
      // the current RS is expired each second to ensure that a status byte will be sent
      // if the MIDI cable is (re)connected during runtime
      MIOS32_DIN_MIDI_RS_Reset(din_port);
#if 0
      // for optional monitoring of the optimisation
      MIOS32_MIDI_SendDebugMessage("[MIOS32_DIN_MIDI:%d] RS 0x%02x expired!\n", din_port);
#endif
    } else {
      if( (rs_optimisation & (1 << din_port)) &&
	  package.cin >= NoteOff && package.cin <= PitchBend &&
	  len > 1 ) { // (len check is a failsafe measure)
	if( package.evnt0 == state->rs_last ) {
	  buffer[0] = package.evnt1;
	  buffer[1] = package.evnt2;
	  --len;
#if 0
	  // for optional monitoring of the optimisation
	  MIOS32_MIDI_SendDebugMessage("[MIOS32_DIN_MIDI:%d] RS optimized (%02x) %02x %02x\n", din_port, package.evnt0, package.evnt1, package.evnt2);
#endif
	} else {
	  // new running status
	  state->rs_expire_ctr = 0;
	}
      }
    }

    // note: packages != Note Off, On, ... Pitch Bend will disable running status - thats acceptable
    // only realtime events won't touch it (according to MIDI spec)
    if( package.evnt0 < 0xf8 )
      state->rs_last = package.evnt0;


    switch( MIOS32_UART_TxBufferPutMore(din_port, buffer, len) ) {
      case  0: return  0; // transfer successfull
      case -2: return -2; // buffer full, request retry
      default: return -1; // UART error
    }

  } else {
    return 0; // no bytes to send -> no error
  }
}

/////////////////////////////////////////////////////////////////////////////
//! This function sends a new MIDI package to the selected DIN_MIDI port
//! (blocking function)
//! \param[in] din_port DIN_MIDI module number (0..15)
//! \param[in] package MIDI package
//! \return 0: no error
//! \return -1: DIN_MIDI device not available
//! \note Applications shouldn't call this function directly, instead please use \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DIN_MIDI_PackageSend(u8 din_port, mios32_midi_package_t package)
{
  s32 error;

  while( (error=MIOS32_DIN_MIDI_PackageSend_NonBlocking(din_port, package)) == -2);

  return error;
}


/////////////////////////////////////////////////////////////////////////////
//! This function checks for a new package
//! \param[in] din_port DIN_MIDI module number (0..15)
//! \param[out] package pointer to MIDI package (received package will be put into the given variable
//! \return 0: no error
//! \return -1: no package in buffer
//! \return -10: incoming MIDI package timed out (incomplete package received)
//! \note Applications shouldn't call this function directly, instead please use \ref MIOS32_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_DIN_MIDI_PackageReceive(u8 din_port, mios32_midi_package_t *package)
{
  // exit if UART port not available
  if( !MIOS32_DIN_MIDI_CheckAvailable(din_port) )
    return -1;
  if( din_port >= MIOS32_DIN_MIDI_MAX_PORTS || din_midi_state_ptr[din_port] == NULL )
    return -1;

  // parses the next incoming byte(s), stop until we got a complete MIDI event
  // (-> complete package) and forward it to the caller
  midi_rec_t *midix = &din_midi_state_ptr[din_port]->rec;// simplify addressing of midi record
  u8 package_complete = 0;
  s32 status;
  while( !package_complete && (status=MIOS32_UART_RxBufferGet(din_port)) >= 0 ) {
    u8 byte = (u8)status;

    if( byte & 0x80 ) { // new MIDI status
      if( byte >= 0xf8 ) { // events >= 0xf8 don't change the running status and can just be forwarded
	// Realtime messages don't change the running status and can be sent immediately
	// They also don't touch the timeout counter!
	package->cin = 0xf; // F: single byte
	package->evnt0 = byte;
	package->evnt1 = 0x00;
	package->evnt2 = 0x00;
	package_complete = 1;
      } else {
	midix->running_status = byte;
	midix->expected_bytes = mios32_midi_expected_bytes_common[(byte >> 4) & 0x7];

	if( !midix->expected_bytes ) { // System Message, take number of bytes from expected_bytes_system[] array
	  midix->expected_bytes = mios32_midi_expected_bytes_system[byte & 0xf];

	  if( byte == 0xf0 ) {
	    midix->package.evnt0 = 0xf0; // midix->package.evnt0 only used by SysEx handler for continuous data streams!
	    midix->sysex_ctr = 0x01;
	  } else if( byte == 0xf7 ) {
	    switch( midix->sysex_ctr ) {
 	      case 0:
		midix->package.cin = 5; // 5: SysEx ends with single byte
		midix->package.evnt0 = 0xf7;
		midix->package.evnt1 = 0x00;
		midix->package.evnt2 = 0x00;
		break;
	      case 1:
		midix->package.cin = 6; // 6: SysEx ends with two bytes
		// midix->package.evnt0 = // already stored
		midix->package.evnt1 = 0xf7;
		midix->package.evnt2 = 0x00;
		break;
	      default:
		midix->package.cin = 7; // 7: SysEx ends with three bytes
		// midix->package.evnt0 = // already stored
		// midix->package.evnt1 = // already stored
		midix->package.evnt2 = 0xf7;
		break;
	    }
	    *package = midix->package;
	    package_complete = 1; // -> forward to caller
	    midix->sysex_ctr = 0x00; // ensure that next F7 will just send F7
	  } else if( !midix->expected_bytes ) {
	    // e.g. tune request (with no additional byte)
	    midix->package.cin = 5; // 5: SysEx ends with single byte
	    midix->package.evnt0 = byte;
	    midix->package.evnt1 = 0x00;
	    midix->package.evnt2 = 0x00;
	    *package = midix->package;
	    package_complete = 1; // -> forward to caller
	  }
	}

	midix->wait_bytes = midix->expected_bytes;
	midix->timeout_ctr = 0; // reset timeout counter
      }
    } else {
      if( midix->running_status == 0xf0 ) {
	switch( ++midix->sysex_ctr ) {
  	  case 1:
	    midix->package.evnt0 = byte;
	    break;
	  case 2:
	    midix->package.evnt1 = byte;
	    break;
	  default: // 3
	    midix->package.evnt2 = byte;

	    // Send three-byte event
	    midix->package.cin = 4;  // 4: SysEx starts or continues
	    *package = midix->package;
	    package_complete = 1; // -> forward to caller
	    midix->sysex_ctr = 0x00; // reset and prepare for next packet
	    midix->timeout_ctr = 0; // reset timeout counter
	}
      } else { // Common MIDI message or 0xf1 >= status >= 0xf7
	if( !midix->wait_bytes ) {
	  // received new MIDI event with running status
	  midix->wait_bytes = midix->expected_bytes - 1;
	  midix->timeout_ctr = 0; // reset timeout counter
	} else {
	  --midix->wait_bytes;
	}

	if( midix->expected_bytes == 1 ) {
	  midix->package.evnt1 = byte;
	  midix->package.evnt2 = 0x00;
	} else {
	  if( midix->wait_bytes )
	    midix->package.evnt1 = byte;
	  else
	    midix->package.evnt2 = byte;
	}

	if( !midix->wait_bytes ) {
	  if( (midix->running_status & 0xf0) != 0xf0 ) {
	    midix->package.cin = midix->running_status >> 4; // common MIDI message
	  } else {
	    switch( midix->expected_bytes ) { // MEMO: == 0 comparison was a bug in original MBHP_USB code
  	      case 0:
		midix->package.cin = 5; // 5: SysEx common with one byte
		break;
  	      case 1:
		midix->package.cin = 2; // 2: SysEx common with two bytes
		break;
  	      default:
		midix->package.cin = 3; // 3: SysEx common with three bytes
		break;
	    }
	  }

	  midix->package.evnt0 = midix->running_status;
	  // midix->package.evnt1 = // already stored
	  // midix->package.evnt2 = // already stored
	  *package = midix->package;
	  package_complete = 1; // -> forward to caller
	}
      }
    }
  }

  // incoming MIDI package timed out (incomplete package received)
  if( midix->wait_bytes && midix->timeout_ctr > 1000 ) { // 1000 mS = 1 second
    // stop waiting
    MIOS32_DIN_MIDI_RecordReset(din_port);
    // notify that incomplete package has been received
    return -10;
  }

  // return 0 if new package in buffer, otherwise -1
  return package_complete ? 0 : -1;
}


#endif /* MIOS32_USE_DIN_MIDI */
