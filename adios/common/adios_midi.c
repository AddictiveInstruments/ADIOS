//! \defgroup ADIOS_MIDI
//!
//! MIDI layer functions for ADIOS
//!
//! the adios_midi_package_t format complies with USB MIDI spec (details see there)
//! and is used for transfers between other MIDI ports as well.
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
#include <string.h>
#include <stdarg.h>

// The MIDI core is NOT optional - it is always compiled, like SYS/IRQ/DELAY
// (2026-08-09 decision): MIDI is the whole point of this OS, a project that
// needs neither MIDI nor the BSL is a plain CubeMX export, not an OS user.
// Only the TRANSPORTS are opt-in: ADIOS_USE_DIN_MIDI / ADIOS_USE_USB_MIDI /
// ADIOS_USE_SPI_MIDI.
#if ADIOS_MIDI_BSL_ENHANCEMENTS
// this compile switch should only be activated for the bootloader!
#include <bsl_sysex.h>
#endif

/////////////////////////////////////////////////////////////////////////////
// Global variables
/////////////////////////////////////////////////////////////////////////////

//! this global array is read from ADIOS_DIN_MIDI to
//! determine the number of MIDI bytes which are part of a package
const u8 adios_midi_pcktype_num_bytes[16] = {
		0, // 0: invalid/reserved event
		0, // 1: invalid/reserved event
		2, // 2: two-byte system common messages like MTC, Song Select, etc.
		3, // 3: three-byte system common messages like SPP, etc.
		3, // 4: SysEx starts or continues
		1, // 5: Single-byte system common message or sysex sends with following single byte
		2, // 6: SysEx sends with following two bytes
		3, // 7: SysEx sends with following three bytes
		3, // 8: Note Off
		3, // 9: Note On
		3, // a: Poly-Key Press
		3, // b: Control Change
		2, // c: Program Change
		2, // d: Channel Pressure
		3, // e: PitchBend Change
		1  // f: single byte
};

//! Number if expected bytes for a common MIDI event - 1
const u8 adios_midi_expected_bytes_common[8] = {
		2, // Note On
		2, // Note Off
		2, // Poly Preasure
		2, // Controller
		1, // Program Change
		1, // Channel Preasure
		2, // Pitch Bender
		0, // System Message - must be zero, so that adios_midi_expected_bytes_system[] will be used
};

//! Number if expected bytes for a system MIDI event - 1
const u8 adios_midi_expected_bytes_system[16] = {
		1, // SysEx Begin (endless until SysEx End F7)
		1, // MTC Data frame
		2, // Song Position
		1, // Song Select
		0, // Reserved
		0, // Reserved
		0, // Request Tuning Calibration
		0, // SysEx End

		// Note: just only for documentation, Realtime Messages don't change the running status
		0, // MIDI Clock
		0, // MIDI Tick
		0, // MIDI Start
		0, // MIDI Continue
		0, // MIDI Stop
		0, // Reserved
		0, // Active Sense
		0, // Reset
};

//! The five bytes every message of this protocol opens with. 00 22 15 is
//! the Addictive Instruments manufacturer ID; the fifth byte says WHO is
//! answering, and 0x32 is the OS - the bootloader and the ADIOS queries.
//! An APPLICATION speaking its own protocol takes its own fifth byte, so a
//! host tool knows which of the two it reached.
//! Should only be used by ADIOS internally and by the Bootloader!
const u8 adios_midi_sysex_header[5] = { 0xf0, 0x00, 0x22, 0x15, 0x32 };


/////////////////////////////////////////////////////////////////////////////
// Local types
/////////////////////////////////////////////////////////////////////////////

typedef union {
	struct {
		unsigned ALL:8;
	};

	struct {
		unsigned CTR:3;
		unsigned MY_SYSEX:1;
		unsigned CMD:1;
	} general;

	struct {
		unsigned CTR:3;
		unsigned MY_SYSEX:1;
		unsigned CMD:1;
		unsigned PING_BYTE_RECEIVED;
	} ping;
} sysex_state_t;


typedef union {
	struct {
		unsigned long long ALL;
	};

	struct {
		unsigned long long usb_receives:16;
		unsigned long long spi_receives:16;
	};
} sysex_timeout_ctr_flags_t;


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static adios_midi_port_t default_port = ADIOS_MIDI_DEFAULT_PORT;
static adios_midi_port_t debug_port   = ADIOS_MIDI_DEBUG_PORT;

static s32 (*direct_rx_callback_func)(adios_midi_port_t port, u8 midi_byte);
static s32 (*direct_tx_callback_func)(adios_midi_port_t port, adios_midi_package_t package);
static s32 (*sysex_callback_func)(adios_midi_port_t port, u8 sysex_byte);
static s32 (*timeout_callback_func)(adios_midi_port_t port);
static s32 (*debug_command_callback_func)(adios_midi_port_t port, char c);
static s32 (*filebrowser_command_callback_func)(adios_midi_port_t port, char c);

static sysex_state_t sysex_state;
static u8 sysex_device_id;
static u8 sysex_cmd;
static adios_midi_port_t last_sysex_port = DEFAULT;


// SysEx timeout counter: in order to save memory and execution time, we only
// protect a single SysEx stream for packet oriented interfaces.
// Serial interfaces (UART) are protected separately -> see ADIOS_DIN_MIDI_PackageReceive
// Approach: the first interface which sends F0 resets the timeout counter.
// The flag is reset with F7
// Once one second has passed, and the flag is still set, ADIOS_MIDI_TimeOut() will
// be called to notify the failure.
// Drawback: if another interface starts a SysEx transfer at the same time, the stream
// will be ignored, and a timeout won't be detected.
// It's unlikely that this is an issue in practice, especially if SysEx parsers only
// take streams of the first interface which sends F0 like ADIOS_MIDI_SYSEX_Parser()...
//
// If a stronger protection is desired (SysEx parser handles streams of multiple interfaces),
// it's recommented to implement a separate timeout mechanism at application side.
#if defined(ADIOS_USE_MIDI_ACT)
// Two bits per port over the whole port space, which runs contiguously from
// USB0 up to the last SPIM - so the index is simply "port - USB0", with no
// lookup table: the port numbering was laid out this way on purpose.
#define MIDI_ACT_PORTS  (SPIM7 - USB0 + 1)
#define MIDI_ACT_WORDS  (((2*MIDI_ACT_PORTS) + 31) / 32)

// TWO sets, and that is the whole mechanism. Marks always land in the current
// set; every ADIOS_MIDI_ACT_MS the current set becomes the previous one and
// the current is emptied. A read looks at BOTH. So a flag raised an instant
// before a rotation still survives a full period instead of being wiped a
// millisecond later: its lifetime is between one and two periods, never less,
// and it disappears on its own if nobody ever reads it.
//
// The alternative - a countdown per port - would cost 112 bytes and a
// 112-step loop every millisecond, against 32 bytes and two assignments per
// period here. The second set IS the counter, shared by all ports at once.
static u32 midi_act[2][MIDI_ACT_WORDS];
static u16 midi_act_ctr;

static void MIDI_ActMark(adios_midi_port_t port, u8 tx, u8 evnt0)
{
	u16 idx, bit;

	// MIDI clock is excluded: a synced setup sends it 24 times per beat and
	// would hold the indicator permanently lit, showing nothing.
	if( evnt0 == 0xf8 )
		return;

	idx = (u16)port - USB0;
	if( idx >= MIDI_ACT_PORTS )
		return; // DEFAULT/MIDI_DEBUG are resolved to a real port before this

	bit = 2*idx + (tx ? 1 : 0);

	ADIOS_IRQ_Disable(); // marks arrive from a task AND from the RX interrupt
	midi_act[0][bit / 32] |= 1UL << (bit % 32);
	ADIOS_IRQ_Enable();
}
#else
# define MIDI_ActMark(port, tx, evnt0)  // compiles to nothing at all
#endif


static u16 sysex_timeout_ctr;
static sysex_timeout_ctr_flags_t sysex_timeout_ctr_flags;


/////////////////////////////////////////////////////////////////////////////
// Local prototypes
/////////////////////////////////////////////////////////////////////////////

static s32 ADIOS_MIDI_SYSEX_Parser(adios_midi_port_t port, u8 midi_in);
static s32 ADIOS_MIDI_SYSEX_CmdFinished(adios_midi_port_t port);
static s32 ADIOS_MIDI_SYSEX_Cmd(adios_midi_port_t port, adios_midi_sysex_cmd_state_t cmd_state, u8 midi_in);
static s32 ADIOS_MIDI_SYSEX_Cmd_Query(adios_midi_port_t port, adios_midi_sysex_cmd_state_t cmd_state, u8 midi_in);
static s32 ADIOS_MIDI_SYSEX_Cmd_Debug(adios_midi_port_t port, adios_midi_sysex_cmd_state_t cmd_state, u8 midi_in);
static s32 ADIOS_MIDI_SYSEX_Cmd_Ping(adios_midi_port_t port, adios_midi_sysex_cmd_state_t cmd_state, u8 midi_in);
static s32 ADIOS_MIDI_SYSEX_SendAck(adios_midi_port_t port, u8 ack_code, u8 ack_arg);
static s32 ADIOS_MIDI_SYSEX_SendAckStr(adios_midi_port_t port, char *str);
static s32 ADIOS_MIDI_TimeOut(adios_midi_port_t port);


/////////////////////////////////////////////////////////////////////////////
//! Initializes MIDI layer
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_Init(u32 mode)
{
	s32 ret = 0;

	// currently only mode 0 supported
	if( mode != 0 )
		return -1; // unsupported mode

	// set default/debug port as defined in adios.h/adios_config.h
	default_port = ADIOS_MIDI_DEFAULT_PORT;
	debug_port = ADIOS_MIDI_DEBUG_PORT;

	// disable callback functions
	direct_rx_callback_func = NULL;
	direct_tx_callback_func = NULL;
	sysex_callback_func = NULL;
	timeout_callback_func = NULL;
	debug_command_callback_func = NULL;
	filebrowser_command_callback_func = NULL;

	// initialize interfaces
#if defined(ADIOS_USE_USB_MIDI)
	if( ADIOS_USB_MIDI_Init(0) < 0 )
		ret |= (1 << 0);
#endif

#if defined(ADIOS_USE_DIN_MIDI)
	if( ADIOS_DIN_MIDI_Init(0) < 0 )
		ret |= (1 << 1);
#endif

#if defined(ADIOS_USE_SPI_MIDI)
	if( ADIOS_SPI_MIDI_Init(0) < 0 )
		ret |= (1 << 3);
#endif

	last_sysex_port = DEFAULT;
	sysex_state.ALL = 0;

	// compile-time identity, overridable by the project (adios_midi.h) and
	// zero when it says nothing. Without ADIOS_DEVICE_ID_PERSIST this is the
	// WHOLE story: no flash is read, nothing is searched for.
	sysex_device_id = ADIOS_MIDI_DEFAULT_DEVICE_ID;

#if ADIOS_DEVICE_ID_PERSIST
	// ...otherwise the stored identity wins. Two bytes at the very top of
	// flash, confirm marker then value - the one place an application and a
	// bootloader both find without being told (see adios_sys.h). Written by
	// the application inside its own reserved pages, so it survives an
	// application upload (which only erases the pages it writes) and even an
	// interrupted one - a device left with no application still answers on
	// the right ID, which is precisely when that matters.
	u8 *persist_confirm = (u8 *)ADIOS_SYS_ADDR_PERSIST_DEVICE_ID_CONFIRM;
	u8 *persist_id      = (u8 *)ADIOS_SYS_ADDR_PERSIST_DEVICE_ID;
	if( *persist_confirm == 0x42 && *persist_id < 0x80 )
		sysex_device_id = *persist_id;
#endif

	// SysEx timeout mechanism
	sysex_timeout_ctr = 0;
	sysex_timeout_ctr_flags.ALL = 0;

	return -ret;
}


/////////////////////////////////////////////////////////////////////////////
//! This function checks the availability of a MIDI port
//! \param[in] port MIDI port (DEFAULT, USB0..USB31, DIN0..DIN15, SPIM0..SPIM15)
//! \return 1: port available
//! \return 0: port not available
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_CheckAvailable(adios_midi_port_t port)
{
	// if default/debug port: select mapped port
	if( !(port & 0xf0) ) {
		port = (port == MIDI_DEBUG) ? debug_port : default_port;
	}

	// branch depending on selected port
	switch( port & 0xf0 ) {
	case USB0:   //..15 - first controller
	case USB16:  //..31 - second controller. Same code: port - USB0 is a
	             // contiguous 0..31 across both ranges, so the transport
	             // splits it into controller and cable by itself.
#if defined(ADIOS_USE_USB_MIDI)
		return ADIOS_USB_MIDI_CheckAvailable(port - USB0);
#else
		return 0; // USB_MIDI not enabled
#endif

	case DIN0://..15
#if defined(ADIOS_USE_DIN_MIDI)
		return ADIOS_DIN_MIDI_CheckAvailable(port & 0xf);
#else
		return 0; // DIN_MIDI not enabled
#endif

	case SPIM0://..15
#if defined(ADIOS_USE_SPI_MIDI)
		return ADIOS_SPI_MIDI_CheckAvailable(port & 0xf);
#else
		return 0; // SPI_MIDI not enabled
#endif

	}

	return 0; // invalid port
}


/////////////////////////////////////////////////////////////////////////////
//! This function enables/disables running status optimisation for a given
//! MIDI OUT port to improve bandwidth if MIDI events with the same
//! status byte are sent back-to-back.<BR>
//! The optimisation is currently only used for UART based port (enabled by
//! default), USB: not required).
//! \param[in] port MIDI port (DEFAULT, USB0..USB31, DIN0..DIN15)
//! \param[in] enable 0=optimisation disabled, 1=optimisation enabled
//! \return -1 if port not available or if it doesn't support running status
//! \return 0 on success
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_RS_OptimisationSet(adios_midi_port_t port, u8 enable)
{
	// if default/debug port: select mapped port
	if( !(port & 0xf0) ) {
		port = (port == MIDI_DEBUG) ? debug_port : default_port;
	}

	// branch depending on selected port
	switch( port & 0xf0 ) {
	case USB0:   //..15 - first controller
	case USB16:  //..31 - second controller. Same code: port - USB0 is a
	             // contiguous 0..31 across both ranges, so the transport
	             // splits it into controller and cable by itself.
		return -1; // not required for USB

	case DIN0://..15
#if defined(ADIOS_USE_DIN_MIDI)
		return ADIOS_DIN_MIDI_RS_OptimisationSet(port & 0xf, enable);
#else
		return -1; // DIN_MIDI not enabled
#endif

	case SPIM0://..15
		// The SPI-MIDI transport does not implement running status
		// optimisation - the board at the far end does. This used to call
		// ADIOS_SPI_MIDI_RS_OptimisationSet(), whose entire body was an M16
		// command carrying a port mask; it moved to modules/m16 on
		// 2026-08-14 as ADIOS_SPIM_M16_RS_OptimisationSet().
		return -1; // not implemented by this transport

	}

	return -1; // invalid port
}


/////////////////////////////////////////////////////////////////////////////
//! This function returns the running status optimisation enable/disable flag
//! for the given MIDI OUT port.
//! \param[in] port MIDI port (DEFAULT, USB0..USB31, DIN0..DIN15, SPIM0..SPIM15)
//! \return -1 if port not available or if it doesn't support running status
//! \return 0 if optimisation disabled
//! \return 1 if optimisation enabled
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_RS_OptimisationGet(adios_midi_port_t port)
{
	// if default/debug port: select mapped port
	if( !(port & 0xf0) ) {
		port = (port == MIDI_DEBUG) ? debug_port : default_port;
	}

	// branch depending on selected port
	switch( port & 0xf0 ) {
	case USB0:   //..15 - first controller
	case USB16:  //..31 - second controller. Same code: port - USB0 is a
	             // contiguous 0..31 across both ranges, so the transport
	             // splits it into controller and cable by itself.
		return -1; // not required for USB

	case DIN0://..15
#if defined(ADIOS_USE_DIN_MIDI)
		return ADIOS_DIN_MIDI_RS_OptimisationGet(port & 0xf);
#else
		return -1; // DIN_MIDI not enabled
#endif

	case SPIM0://..15
		// see the Set() counterpart: moved to modules/m16 as
		// ADIOS_SPIM_M16_RS_OptimisationGet()
		return -1; // not implemented by this transport

	}

	return -1; // invalid port
}


/////////////////////////////////////////////////////////////////////////////
//! This function resets the current running status, so that it will be sent
//! again with the next MIDI Out package.
//! \param[in] port MIDI port (DEFAULT, USB0..USB31, DIN0..DIN15, SPIM0..SPIM15)
//! \return -1 if port not available or if it doesn't support running status
//! \return 0 if optimisation disabled
//! \return 1 if optimisation enabled
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_RS_Reset(adios_midi_port_t port)
{
	// if default/debug port: select mapped port
	if( !(port & 0xf0) ) {
		port = (port == MIDI_DEBUG) ? debug_port : default_port;
	}

	// branch depending on selected port
	switch( port & 0xf0 ) {
	case USB0:   //..15 - first controller
	case USB16:  //..31 - second controller. Same code: port - USB0 is a
	             // contiguous 0..31 across both ranges, so the transport
	             // splits it into controller and cable by itself.
		return -1; // not required for USB

	case DIN0://..15
#if defined(ADIOS_USE_DIN_MIDI)
		return ADIOS_DIN_MIDI_RS_Reset(port & 0xf);
#else
		return -1; // DIN_MIDI not enabled
#endif

	case SPIM0://..15
		return -1; // not required for SPI

	}

	return -1; // invalid port
}


/////////////////////////////////////////////////////////////////////////////
//! Sends a package over given port
//!
//! This is a low level function. In difference to other ADIOS_MIDI_Send* functions,
//! It allows to send packages in non-blocking mode (caller has to retry if -2 is returned)
//!
//! Before the package is forwarded, an optional Tx Callback function will be called
//! which allows to filter/monitor/route the package, or extend the MIDI transmitter
//! by custom MIDI Output ports (e.g. for internal busses, OSC, AOUT, etc.)
//! \param[in] port MIDI port (DEFAULT, USB0..USB31, DIN0..DIN15, SPIM0..SPIM15)
//! \param[in] package MIDI package
//! \return -1 if port not available
//! \return -2 buffer is full
//!         caller should retry until buffer is free again
//! \return -3 Tx Callback reported an error
//! \return 1 if package has been filtered by Tx callback
//! \return 0 on success
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_SendPackage_NonBlocking(adios_midi_port_t port, adios_midi_package_t package)
{
	// if default/debug port: select mapped port
	if( !(port & 0xf0) ) {
		port = (port == MIDI_DEBUG) ? debug_port : default_port;
	}

	// insert subport number into package
	package.cable = port & 0xf;

	// forward to Tx callback function and break if package has been filtered
	if( direct_tx_callback_func != NULL ) {
		s32 status;
		if( (status=direct_tx_callback_func(port, package)) )
			return status;
	}

	// every transport goes through this switch, so one mark covers them all
	MIDI_ActMark(port, 1, package.evnt0);

	// branch depending on selected port
	switch( port & 0xf0 ) {
	case USB0:   //..15 - first controller
	case USB16:  //..31 - second controller. Same code: port - USB0 is a
	             // contiguous 0..31 across both ranges, so the transport
	             // splits it into controller and cable by itself.
#if defined(ADIOS_USE_USB_MIDI)
		return ADIOS_USB_MIDI_PackageSend_NonBlocking(port - USB0, package);
#else
		return -1; // USB_MIDI not enabled
#endif

	case DIN0://..15
#if defined(ADIOS_USE_DIN_MIDI)
		return ADIOS_DIN_MIDI_PackageSend_NonBlocking(package.cable, package);
#else
		return -1; // DIN_MIDI not enabled
#endif

	case SPIM0://..15
#if defined(ADIOS_USE_SPI_MIDI)
		return ADIOS_SPI_MIDI_PackageSend_NonBlocking(package);
#else
		return -1; // SPI_MIDI not enabled
#endif

	default:
		// invalid port
		return -1;
	}
}


/////////////////////////////////////////////////////////////////////////////
//! Sends a package over given port
//! This is a low level function - use the remaining ADIOS_MIDI_Send* functions
//! to send specific MIDI events
//! (blocking function)
//! \param[in] port MIDI port (DEFAULT, USB0..USB31, DIN0..DIN15, SPIM0..SPIM15)
//! \param[in] package MIDI package
//! \return -1 if port not available
//! \return 0 on success
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_SendPackage(adios_midi_port_t port, adios_midi_package_t package)
{
	// if default/debug port: select mapped port
	if( !(port & 0xf0) ) {
		port = (port == MIDI_DEBUG) ? debug_port : default_port;
	}

	// insert subport number into package
	package.cable = port & 0xf;

	// forward to Tx callback function and break if package has been filtered
	if( direct_tx_callback_func != NULL ) {
		s32 status;
		if( (status=direct_tx_callback_func(port, package)) )
			return status;
	}

	// branch depending on selected port
	switch( port & 0xf0 ) {
	case USB0:   //..15 - first controller
	case USB16:  //..31 - second controller. Same code: port - USB0 is a
	             // contiguous 0..31 across both ranges, so the transport
	             // splits it into controller and cable by itself.
#if defined(ADIOS_USE_USB_MIDI)
		return ADIOS_USB_MIDI_PackageSend(port - USB0, package);
#else
		return -1; // USB_MIDI not enabled
#endif

	case DIN0://..15
#if defined(ADIOS_USE_DIN_MIDI)
		return ADIOS_DIN_MIDI_PackageSend(package.cable, package);
#else
		return -1; // DIN_MIDI not enabled
#endif

	case SPIM0://..15
#if defined(ADIOS_USE_SPI_MIDI)
		return ADIOS_SPI_MIDI_PackageSend(package);
#else
		return -1; // SPI_MIDI not enabled
#endif

	default:
		// invalid port
		return -1;
	}
}


/////////////////////////////////////////////////////////////////////////////
//! Sends a MIDI Event
//! This function is provided for a more comfortable use model
//!    o ADIOS_MIDI_SendNoteOff(port, chn, note, vel)
//!    o ADIOS_MIDI_SendNoteOn(port, chn, note, vel)
//!    o ADIOS_MIDI_SendPolyAftertouch(port, chn, note, val)
//!    o ADIOS_MIDI_SendCC(port, chn, cc, val)
//!    o ADIOS_MIDI_SendProgramChange(port, chn, prg)
//!    o ADIOS_MIDI_ChannelAftertouch(port, chn, val)
//!    o ADIOS_MIDI_PitchBend(port, chn, val)
//!
//! \param[in] port MIDI port (DEFAULT, USB0..USB31, DIN0..DIN15, SPIM0..SPIM15)
//! \param[in] evnt0 first MIDI byte
//! \param[in] evnt1 second MIDI byte
//! \param[in] evnt2 third MIDI byte
//! \return -1 if port not available
//! \return 0 on success
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_SendEvent(adios_midi_port_t port, u8 evnt0, u8 evnt1, u8 evnt2)
{
	adios_midi_package_t package;

	// MEMO: don't optimize this function by calling ADIOS_MIDI_SendSpecialEvent
	// from here, because the 4 * u8 parameter list of this function leads
	// to best compile results (4*u8 combined to a single u32)

	package.type  = evnt0 >> 4;
	package.evnt0 = evnt0;
	package.evnt1 = evnt1;
	package.evnt2 = evnt2;
	return ADIOS_MIDI_SendPackage(port, package);
}

s32 ADIOS_MIDI_SendNoteOff(adios_midi_port_t port, adios_midi_chn_t chn, u8 note, u8 vel)
{ return ADIOS_MIDI_SendEvent(port, 0x80 | chn, note, vel); }

s32 ADIOS_MIDI_SendNoteOn(adios_midi_port_t port, adios_midi_chn_t chn, u8 note, u8 vel)
{ return ADIOS_MIDI_SendEvent(port, 0x90 | chn, note, vel); }

s32 ADIOS_MIDI_SendPolyPressure(adios_midi_port_t port, adios_midi_chn_t chn, u8 note, u8 val)
{ return ADIOS_MIDI_SendEvent(port, 0xa0 | chn, note, val); }

s32 ADIOS_MIDI_SendCC(adios_midi_port_t port, adios_midi_chn_t chn, u8 cc_number, u8 val)
{ return ADIOS_MIDI_SendEvent(port, 0xb0 | chn, cc_number,   val); }

s32 ADIOS_MIDI_SendProgramChange(adios_midi_port_t port, adios_midi_chn_t chn, u8 prg)
{ return ADIOS_MIDI_SendEvent(port, 0xc0 | chn, prg,  0x00); }

s32 ADIOS_MIDI_SendAftertouch(adios_midi_port_t port, adios_midi_chn_t chn, u8 val)
{ return ADIOS_MIDI_SendEvent(port, 0xd0 | chn, val,  0x00); }

s32 ADIOS_MIDI_SendPitchBend(adios_midi_port_t port, adios_midi_chn_t chn, u16 val)
{ return ADIOS_MIDI_SendEvent(port, 0xe0 | chn, val & 0x7f, val >> 7); }


/////////////////////////////////////////////////////////////////////////////
//! Sends a special type MIDI Event
//! This function is provided for a more comfortable use model
//! It is aliased to following functions
//!    o ADIOS_MIDI_SendMTC(port, val)
//!    o ADIOS_MIDI_SendSongPosition(port, val)
//!    o ADIOS_MIDI_SendSongSelect(port, val)
//!    o ADIOS_MIDI_SendTuneRequest()
//!    o ADIOS_MIDI_SendClock()
//!    o ADIOS_MIDI_SendTick()
//!    o ADIOS_MIDI_SendStart()
//!    o ADIOS_MIDI_SendStop()
//!    o ADIOS_MIDI_SendContinue()
//!    o ADIOS_MIDI_SendActiveSense()
//!    o ADIOS_MIDI_SendReset()
//!
//! \param[in] port MIDI port (DEFAULT, USB0..USB31, DIN0..DIN15, SPIM0..SPIM15)
//! \param[in] type the event type
//! \param[in] evnt0 first MIDI byte
//! \param[in] evnt1 second MIDI byte
//! \param[in] evnt2 third MIDI byte
//! \return -1 if port not available
//! \return 0 on success
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_SendSpecialEvent(adios_midi_port_t port, u8 type, u8 evnt0, u8 evnt1, u8 evnt2)
{
	adios_midi_package_t package;

	package.type  = type;
	package.evnt0 = evnt0;
	package.evnt1 = evnt1;
	package.evnt2 = evnt2;
	return ADIOS_MIDI_SendPackage(port, package);
}


s32 ADIOS_MIDI_SendMTC(adios_midi_port_t port, u8 val)
{ return ADIOS_MIDI_SendSpecialEvent(port, 0x2, 0xf1, val, 0x00); }

s32 ADIOS_MIDI_SendSongPosition(adios_midi_port_t port, u16 val)
{ return ADIOS_MIDI_SendSpecialEvent(port, 0x3, 0xf2, val & 0x7f, val >> 7); }

s32 ADIOS_MIDI_SendSongSelect(adios_midi_port_t port, u8 val)
{ return ADIOS_MIDI_SendSpecialEvent(port, 0x2, 0xf3, val, 0x00); }

s32 ADIOS_MIDI_SendTuneRequest(adios_midi_port_t port)
{ return ADIOS_MIDI_SendSpecialEvent(port, 0x5, 0xf6, 0x00, 0x00); }

s32 ADIOS_MIDI_SendClock(adios_midi_port_t port)
{ return ADIOS_MIDI_SendSpecialEvent(port, 0x5, 0xf8, 0x00, 0x00); }

s32 ADIOS_MIDI_SendTick(adios_midi_port_t port)
{ return ADIOS_MIDI_SendSpecialEvent(port, 0x5, 0xf9, 0x00, 0x00); }

s32 ADIOS_MIDI_SendStart(adios_midi_port_t port)
{ return ADIOS_MIDI_SendSpecialEvent(port, 0x5, 0xfa, 0x00, 0x00); }

s32 ADIOS_MIDI_SendContinue(adios_midi_port_t port)
{ return ADIOS_MIDI_SendSpecialEvent(port, 0x5, 0xfb, 0x00, 0x00); }

s32 ADIOS_MIDI_SendStop(adios_midi_port_t port)
{ return ADIOS_MIDI_SendSpecialEvent(port, 0x5, 0xfc, 0x00, 0x00); }

s32 ADIOS_MIDI_SendActiveSense(adios_midi_port_t port)
{ return ADIOS_MIDI_SendSpecialEvent(port, 0x5, 0xfe, 0x00, 0x00); }

s32 ADIOS_MIDI_SendReset(adios_midi_port_t port)
{ return ADIOS_MIDI_SendSpecialEvent(port, 0x5, 0xff, 0x00, 0x00); }


/////////////////////////////////////////////////////////////////////////////
//! Sends a SysEx Stream
//!
//! This function is provided for a more comfortable use model
//! \param[in] port MIDI port (DEFAULT, USB0..USB31, DIN0..DIN15, SPIM0..SPIM15)
//! \param[in] stream pointer to SysEx stream
//! \param[in] count number of bytes
//! \return -1 if port not available
//! \return 0 on success
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_SendSysEx(adios_midi_port_t port, u8 *stream, u32 count)
{
	s32 res;
	u32 offset;
	adios_midi_package_t package;

	// MEMO: have a look into the project.lss file - gcc optimizes this code pretty well :)

	for(offset=0; offset<count;) {
		// package type depends on number of remaining bytes
		switch( count-offset ) {
		case 1:
			package.type = 0x5; // SysEx ends with following single byte.
			package.evnt0 = stream[offset++];
			package.evnt1 = 0x00;
			package.evnt2 = 0x00;
			break;
		case 2:
			package.type = 0x6; // SysEx ends with following two bytes.
			package.evnt0 = stream[offset++];
			package.evnt1 = stream[offset++];
			package.evnt2 = 0x00;
			break;
		case 3:
			package.type = 0x7; // SysEx ends with following three bytes.
			package.evnt0 = stream[offset++];
			package.evnt1 = stream[offset++];
			package.evnt2 = stream[offset++];
			break;
		default:
			package.type = 0x4; // SysEx starts or continues
			package.evnt0 = stream[offset++];
			package.evnt1 = stream[offset++];
			package.evnt2 = stream[offset++];
		}

		res=ADIOS_MIDI_SendPackage(port, package);

		// expection? (e.g., port not available)
		if( res < 0 )
			return res;
	}

	return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Sends the header of a debug string
//!
//! Example (implementation of ADIOS_MIDI_SendDebugString)
//! \code
//!   u32 len = strlen(str);
//!  
//!   ADIOS_MIDI_SendDebugStringHeader(port, 0x40, str[0]);
//!   if( len >= 2 )
//!     ADIOS_MIDI_SendDebugStringBody(port, (char *)&str[1], len-1);
//!   ADIOS_MIDI_SendDebugStringFooter(port);
//! \endcode
//!
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_SendDebugStringHeader(adios_midi_port_t port, char command, char first_byte)
{
#ifdef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
	// for bootloader to save memory
	return -1;
#else
	s32 status = 0;
	adios_midi_package_t package;

	// unfortunately doesn't work, and runtime check would be unnecessary costly
	//#if sizeof(adios_midi_sysex_header) != 5
	//# error "Please adapt ADIOS_MIDI_SendDebugString"
	//#endif

	package.type = 0x4; // SysEx starts or continues
	package.evnt0 = adios_midi_sysex_header[0];
	package.evnt1 = adios_midi_sysex_header[1];
	package.evnt2 = adios_midi_sysex_header[2];
	status |= ADIOS_MIDI_SendPackage(port, package);

	package.type = 0x4; // SysEx starts or continues
	package.evnt0 = adios_midi_sysex_header[3];
	package.evnt1 = adios_midi_sysex_header[4];
	package.evnt2 = ADIOS_MIDI_DeviceIDGet();
	status |= ADIOS_MIDI_SendPackage(port, package);

	package.type = 0x4; // SysEx starts or continues
	package.evnt0 = ADIOS_MIDI_SYSEX_DEBUG;
	package.evnt1 = command; // output string, usually 0x40
	package.evnt2 = first_byte; // will be 0x00 if string already ends (""), thats ok, ADIOS Studio can handle this
	status |= ADIOS_MIDI_SendPackage(port, package);

	return status;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Sends the body of a debug string
//!
//! Example: see ADIOS_MIDI_SendDebugStringHeader
//!
//! The string size isn't limited.
//!
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_SendDebugStringBody(adios_midi_port_t port, char *str, u32 len)
{
#ifdef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
	// for bootloader to save memory
	return -1;
#else
	s32 status = 0;
	adios_midi_package_t package;

	if( len > 0 ) {
		int i = 0;
		for(i=0; i<len; i+=3) {
			u8 b;
			u8 terminated = 0;

			package.type = 0x4; // SysEx starts or continues
			if( (b=str[i+0]) ) {
				package.evnt0 = b & 0x7f;
			} else {
				package.evnt0 = 0x00;
				terminated = 1;
			}

			if( !terminated && (b=str[i+1]) ) {
				package.evnt1 = b & 0x7f;
			} else {
				package.evnt1 = 0x00;
				terminated = 1;
			}

			if( !terminated && (b=str[i+2]) ) {
				package.evnt2 = b & 0x7f;
			} else {
				package.evnt2 = 0x00;
				terminated = 1;
			}

			status |= ADIOS_MIDI_SendPackage(port, package);
		}
	}

	return status;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Sends the footer of a debug string
//!
//! Example: see ADIOS_MIDI_SendDebugStringHeader
//!
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_SendDebugStringFooter(adios_midi_port_t port)
{
#ifdef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
	// for bootloader to save memory
	return -1;
#else
	s32 status = 0;
	adios_midi_package_t package;

	package.type = 0x5; // SysEx ends with following single byte.
	package.evnt0 = 0xf7;
	package.evnt1 = 0x00;
	package.evnt2 = 0x00;
	status |= ADIOS_MIDI_SendPackage(port, package);

	return status;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Sends a string to the ADIOS Terminal in ADIOS Studio.
//!
//! In distance to ADIOS_MIDI_SendDebugMessage this version is less costly (it
//! doesn't consume so much stack space), but the string must already be prepared.
//!
//! Example:
//! \code
//!   ADIOS_MIDI_SendDebugString("ERROR: something strange happened in myFunction()!");
//! \endcode
//!
//! The string size isn't limited.
//!
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_SendDebugString(const char *str)
{
#ifdef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
	// for bootloader to save memory
	return -1;
#else
	s32 status = 0;
	u32 len = strlen(str);

	status |= ADIOS_MIDI_SendDebugStringHeader(debug_port, 0x40, str[0]);
	if( len >= 2 )
		status |= ADIOS_MIDI_SendDebugStringBody(debug_port, (char *)&str[1], len-1);
	status |= ADIOS_MIDI_SendDebugStringFooter(debug_port);

	return status;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Sends a formatted Debug Message to the ADIOS Terminal in ADIOS Studio.
//!
//! Formatting parameters are like known from printf, e.g.
//! \code
//!   ADIOS_MIDI_SendDebugMessage("Button %d %s\n", button, value ? "depressed" : "pressed");
//! \endcode
//!
//! The MIDI port used for debugging (MIDI_DEBUG) can be declared in adios_config.h:
//! \code
//!   #define ADIOS_MIDI_DEBUG_PORT USB0
//! \endcode
//! (USB0 is the default value)
//!
//! Optionally, the port can be changed during runtime with ADIOS_MIDI_DebugPortSet
//!
//! Please note that the resulting string shouldn't be longer than 128 characters!<BR>
//! If the *format string is already longer than 100 characters an error message will
//! be sent to notify about the programming error.<BR>
//! The limit is set to save allocated stack memory! Just reduce the formated string to
//! print out the intended message.
//! \param[in] *format zero-terminated format string - 128 characters supported maximum!
//! \param ... additional arguments
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_SendDebugMessage(const char *format, ...)
{
#ifdef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
	// for bootloader to save memory
	return -1;
#else
	char str[128]; // 128 chars allowed
	va_list args;

	// failsave: if format string is longer than 100 chars, break here
	// note that this is a weak protection: if %s is used, or a lot of other format tokens,
	// the resulting string could still lead to a buffer overflow
	// other the other hand we don't want to allocate too many byte for buffer[] to save stack
	if( strlen(format) > 100 ) {
		// exit with less costly message
		return ADIOS_MIDI_SendDebugString("(ERROR: string passed to ADIOS_MIDI_SendDebugMessage() is longer than 100 chars!\n");
	} else {
		// transform formatted string into string
		va_start(args, format);
		vsprintf(str, format, args);
	}

	u32 len = strlen(str);
	u8 *str_ptr = (u8 *)str;
	int i;
	for(i=0; i<len; ++i) {
		*str_ptr++ &= 0x7f; // ensure that MIDI protocol won't be violated
	}

	return ADIOS_MIDI_SendDebugString(str);
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Sends an hex dump (formatted representation of memory content) to the 
//! ADIOS Terminal in ADIOS Studio.
//!
//! The MIDI port used for debugging (MIDI_DEBUG) can be declared in adios_config.h:
//! \code
//!   #define ADIOS_MIDI_DEBUG_PORT USB0
//! \endcode
//! (USB0 is the default value)
//!
//! Optionally, the port can be changed during runtime with ADIOS_MIDI_DebugPortSet
//! \param[in] *src pointer to memory location which should be dumped
//! \param[in] len number of bytes which should be sent
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_SendDebugHexDump(const u8 *src, u32 len)
{
	s32 status = 0;

	// check if any byte has to be sent
	if( !len )
		return 0;

	// send hex dump line by line
	const u8 *src_begin = src;
	u8 *src_end;
	for(src_end=(u8 *)((size_t)src + len - 1); src < src_end;) {
		char str[80];
		char* str_ptr = (char *)str;
		int i;

		// build line:
		// add source address
		sprintf((char *)str_ptr, "%08X ", (u32)(src-src_begin));
		str_ptr += 9;

		// add up to 16 bytes
		const u8 *src_chars = src; // for later
		for(i=0; i<16; ++i) {
			sprintf((char *)str_ptr, (src <= src_end) ? " %02X" : "   ", *src);
			str_ptr += 3;

			++src;
		}

		// add two spaces
		for(i=0; i<2; ++i)
			*str_ptr++ = ' ';

		// add characters
		for(i=0; i<16; ++i) {
			if( *src_chars < 32 || *src_chars >= 128 )
				*str_ptr++ = '.';
			else
				*str_ptr++ = *src_chars;

			if( src_chars == src_end )
				break;

			++src_chars;
		}

		// linebreak
		*str_ptr++ = '\n';

		// terminator
		*str_ptr++ = 0;

		status |= ADIOS_MIDI_SendDebugString(str);
	}

	return status;
}


/////////////////////////////////////////////////////////////////////////////
//! Processes a received package.
//!
//! Used by ADIOS_MIDI_Receive_Handler, but could also be called from an
//! application, e.g. for passing messages from "virtual ports" which are
//! not handled by ADIOS_MIDI_Receive_Handler
//!
//! \param[in] port MIDI port (DEFAULT, USB0..USB31, DIN0..DIN15, SPIM0..SPIM15)
//! \param[in] package MIDI package
//! \param[in] _callback_package typically APP_MIDI_NotifyPackage
//! \return -1 if port not available
//! \return 0 on success
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_ReceivePackage(adios_midi_port_t port, adios_midi_package_t package, void *_callback_package)
{
	MIDI_ActMark(port, 0, package.evnt0);

	void (*callback_package)(adios_midi_port_t port, adios_midi_package_t midi_package) = _callback_package;

	// remove cable number from package (ADIOS_MIDI passes it's own port number)
	package.cable = 0;

	// branch depending on package type
	if( package.type >= 0x8 && package.type < 0xf ) {
		if( callback_package != NULL )
			callback_package(port, package);
	} else {
		// service SysEx timeout counter
		if( package.evnt0 == 0xf0 || // for package.type == 0xf
				((package.type >= 4 && package.type <= 7) && package.evnt0 != 0xf6) ) { // no timeout on tune request
			// cheap timeout mechanism - see comments above the sysex_timeout_ctr declaration
			if( !sysex_timeout_ctr_flags.ALL ) {
				switch( port & 0xf0 ) {
				case USB0:   //..15 - first controller
				case USB16:  //..31 - second controller
					sysex_timeout_ctr = 0;
					sysex_timeout_ctr_flags.usb_receives = (1 << (port & 0xf));
					break;
				case DIN0://..15
					// already done in ADIOS_DIN_MIDI_PackageReceive()
					break;
				case SPIM0://..15
					sysex_timeout_ctr = 0;
					sysex_timeout_ctr_flags.spi_receives = (1 << (port & 0xf));
					break;
					// no timeout protection for remaining interfaces (yet)
				}
			}
		}

		u8 filter_sysex = 0;
		switch( package.type ) {
		case 0x0: // reserved, ignore
		case 0x1: // cable events, ignore
			if( callback_package != NULL )
				callback_package(port, package); // -> forwarded as event
			break;

		case 0x2: // Two-byte System Common messages like MTC, SongSelect, etc.
		case 0x3: // Three-byte System Common messages like SPP, etc.
			if( callback_package != NULL )
				callback_package(port, package); // -> forwarded as event
			break;

		case 0x4: // SysEx starts or continues (3 bytes)
		case 0xf: // Single byte is interpreted as SysEx as well (I noticed that portmidi sometimes sends single bytes!)

			if( package.evnt0 >= 0xf8 ) { // relevant for package type 0xf
				if( callback_package != NULL )
					callback_package(port, package); // -> realtime event is forwarded as event
				break;
			}

			ADIOS_MIDI_SYSEX_Parser(port, package.evnt0); // -> forward to ADIOS SysEx Parser
			if( package.type != 0x0f ) {
				ADIOS_MIDI_SYSEX_Parser(port, package.evnt1); // -> forward to ADIOS SysEx Parser
				ADIOS_MIDI_SYSEX_Parser(port, package.evnt2); // -> forward to ADIOS SysEx Parser
			}

#if !ADIOS_MIDI_BSL_ENHANCEMENTS // to save some memory
			if( !sysex_state.general.MY_SYSEX ) { // don't forward to application if we receive a ADIOS command
				if( sysex_callback_func != NULL ) {
					filter_sysex |= sysex_callback_func(port, package.evnt0); // -> forwarded as SysEx
					if( package.type != 0x0f ) {
						filter_sysex |= sysex_callback_func(port, package.evnt1); // -> forwarded as SysEx
						filter_sysex |= sysex_callback_func(port, package.evnt2); // -> forwarded as SysEx
					}
				}

				if( callback_package != NULL && !filter_sysex )
					callback_package(port, package);
			}
#endif
			break;

		case 0x5:   // Single-byte System Common Message or SysEx ends with following single byte.
			if( (package.evnt0 >= 0xf8) || (package.evnt0 == 0xf6) ) {
				if( callback_package != NULL )
					callback_package(port, package); // -> forwarded as event
				break;
			}
			// no >= 0xf8 or == 0xf6 event: continue!

		case 0x6:   // SysEx ends with following two bytes.
		case 0x7: { // SysEx ends with following three bytes.
			u8 num_bytes = package.type - 0x5 + 1;
			u8 current_byte = 0;

			if( num_bytes >= 1 ) {
				current_byte = package.evnt0;
				ADIOS_MIDI_SYSEX_Parser(port, current_byte); // -> forward to ADIOS SysEx Parser

#if !ADIOS_MIDI_BSL_ENHANCEMENTS // to save some memory
				if( !sysex_state.general.MY_SYSEX ) { // don't forward to application if we receive a ADIOS command
					if( sysex_callback_func != NULL )
						filter_sysex |= sysex_callback_func(port, current_byte); // -> forwarded as SysEx
				}
#endif
			}

			if( num_bytes >= 2 ) {
				current_byte = package.evnt1;
				ADIOS_MIDI_SYSEX_Parser(port, current_byte); // -> forward to ADIOS SysEx Parser

#if !ADIOS_MIDI_BSL_ENHANCEMENTS // to save some memory
				if( !sysex_state.general.MY_SYSEX ) { // don't forward to application if we receive a ADIOS command
					if( sysex_callback_func != NULL )
						filter_sysex |= sysex_callback_func(port, current_byte); // -> forwarded as SysEx
				}
#endif
			}

			if( num_bytes >= 3 ) {
				current_byte = package.evnt2;
				ADIOS_MIDI_SYSEX_Parser(port, current_byte); // -> forward to ADIOS SysEx Parser

#if !ADIOS_MIDI_BSL_ENHANCEMENTS // to save some memory
				if( !sysex_state.general.MY_SYSEX ) { // don't forward to application if we receive a ADIOS command
					if( sysex_callback_func != NULL )
						filter_sysex |= sysex_callback_func(port, current_byte); // -> forwarded as SysEx
				}
#endif
			}

			// reset timeout protection if required
			if( current_byte == 0xf7 )
				sysex_timeout_ctr_flags.ALL = 0;

#if !ADIOS_MIDI_BSL_ENHANCEMENTS // to save some memory
			if( !sysex_state.general.MY_SYSEX ) { // don't forward to application if we receive a ADIOS command
				// forward as package if not filtered
				if( callback_package != NULL && !filter_sysex )
					callback_package(port, package);
			}
#endif
		} break;
		}
	}

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Checks for incoming MIDI messages and calls callback_package function
//! with following parameters:
//! \code
//!    callback_package(adios_midi_port_t port, adios_midi_package_t midi_package)
//! \endcode
//!
//! Not for use in an application - this function is called by
//! by a task in the programming model, callback_package is APP_MIDI_NotifyPackage()
//!
//! SysEx streams can be optionally redirected to a separate callback function 
//! which can be installed via ADIOS_MIDI_SysExCallback_Init()
//!
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_Receive_Handler(void *_callback_package)
{
	// handle all USB MIDI packages, from every controller
#if defined(ADIOS_USE_USB)
	{
		s32 status;
		adios_midi_package_t package;
		u8 usb_idx;
		// A package carries a 4-bit cable and cannot say which controller it
		// came from, so the transport reports that separately: 0..31 across
		// both ranges, which USB0 + idx turns straight back into a port.
		while( (status=ADIOS_USB_MIDI_PackageReceive(&package, &usb_idx)) >= 0 ) {
			ADIOS_MIDI_ReceivePackage(USB0 + usb_idx, package, _callback_package);
		}
	}
#endif

	// handle all DIN (UART) based MIDI packages (round robin, max 10 packages because of possible timeouts)
	{
		typedef struct {
			adios_midi_port_t port;
			s32 (*receive_func)(u8 if_port, adios_midi_package_t *package);
		} midi_intf_table_t;

		const midi_intf_table_t midi_intf_table[] = {
#if defined(ADIOS_USE_DIN_MIDI)
#if defined(ADIOS_USE_UART0)
				{ DIN0, ADIOS_DIN_MIDI_PackageReceive },
#endif
#if defined(ADIOS_USE_UART1)
				{ DIN1, ADIOS_DIN_MIDI_PackageReceive },
#endif
#if defined(ADIOS_USE_UART2)
				{ DIN2, ADIOS_DIN_MIDI_PackageReceive },
#endif
#if defined(ADIOS_USE_UART3)
				{ DIN3, ADIOS_DIN_MIDI_PackageReceive },
#endif
#if defined(ADIOS_USE_UART4)
				{ DIN4, ADIOS_DIN_MIDI_PackageReceive },
#endif
#if defined(ADIOS_USE_UART5)
				{ DIN5, ADIOS_DIN_MIDI_PackageReceive },
#endif
#if defined(ADIOS_USE_UART6)
				{ DIN6, ADIOS_DIN_MIDI_PackageReceive },
#endif
#if defined(ADIOS_USE_UART7)
				{ DIN7, ADIOS_DIN_MIDI_PackageReceive },
#endif
#if defined(ADIOS_USE_UART8)
				{ DIN8, ADIOS_DIN_MIDI_PackageReceive },
#endif
#if defined(ADIOS_USE_UART9)
				{ DIN9, ADIOS_DIN_MIDI_PackageReceive },
#endif
#endif
				{ 0, NULL } // end of table
		};

		if( midi_intf_table[0].port != 0 ) {
			int packages_forwarded = 0;
			int packages_forwarded_this_round = 0;
			int intf = 0;
			do {
				adios_midi_package_t package;

				// last table entry?
				if( !midi_intf_table[intf].port ) {
					if( !packages_forwarded_this_round )
						break; // no new package

					intf = 0; // at least one package: restart
					packages_forwarded_this_round = 0;
				}

				// execute receive function
				adios_midi_port_t port = midi_intf_table[intf].port;
				s32 status = midi_intf_table[intf].receive_func(port & 0x0f, &package);

				if( status == -10 ) { // receive timeout?
					ADIOS_MIDI_TimeOut(port);
				} else if( status >= 0 ) { // message received?
					++packages_forwarded;
					++packages_forwarded_this_round;

					// handle received package
					ADIOS_MIDI_ReceivePackage(port, package, _callback_package);
				}

				++intf;
			} while( packages_forwarded < 10 );
		}
	}

	// handle all SPI MIDI packages
#if defined(ADIOS_USE_SPI_MIDI)
	{
		s32 status;
		adios_midi_package_t package;
		while( (status=ADIOS_SPI_MIDI_PackageReceive(&package)) >= 0 ) {
			ADIOS_MIDI_ReceivePackage(SPIM0 + package.cable, package, _callback_package);
		}
	}
#endif


	// SysEx timeout detected by this handler?
	if( sysex_timeout_ctr_flags.ALL && sysex_timeout_ctr > 1000 ) {
		u8 timeout_port = 0;

		// determine port
		if( sysex_timeout_ctr_flags.usb_receives ) {
			int i; // i'm missing a prio instruction in C!
			for(i=0; i<16; ++i)
				if( sysex_timeout_ctr_flags.usb_receives & (1 << i) )
					break;
			if( i >= 16 ) // failsafe
				i = 0;
			timeout_port = USB0 + i;
		} else if( sysex_timeout_ctr_flags.spi_receives ) {
			int i; // i'm missing a prio instruction in C!
			for(i=0; i<16; ++i)
				if( sysex_timeout_ctr_flags.spi_receives & (1 << i) )
					break;
			if( i >= 16 ) // failsafe
				i = 0;
			timeout_port = SPIM0 + i;
		}

		ADIOS_MIDI_TimeOut(timeout_port);
		sysex_timeout_ctr_flags.ALL = 0;
	}

	return 0;
}
/////////////////////////////////////////////////////////////////////////////
//! Returns the line activity of ONE port and CLEARS it, so each burst is
//! reported once. Flags also expire on their own after ADIOS_MIDI_ACT_MS, so
//! an indicator whose reader stops polling goes out instead of staying lit.
//! \param[in] port a MIDI port; DEFAULT and MIDI_DEBUG resolve like they do
//!            everywhere else
//! \return ADIOS_MIDI_ACT_RX and/or ADIOS_MIDI_ACT_TX, 0 if quiet - and 0
//!         always when ADIOS_USE_MIDI_ACT was not declared
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_ActGet(adios_midi_port_t port)
{
#if defined(ADIOS_USE_MIDI_ACT)
	u16 idx, bit;
	u8  w;
	u32 mask, both;

	if( !(port & 0xf0) ) // same resolution as the send path
		port = (port == MIDI_DEBUG) ? debug_port : default_port;

	idx = (u16)port - USB0;
	if( idx >= MIDI_ACT_PORTS )
		return 0;

	// A port's two bits are adjacent and start on an EVEN bit, so the pair can
	// never straddle two words: one mask, one word, one read.
	bit  = 2*idx;
	w    = bit / 32;
	mask = 3UL << (bit % 32);

	ADIOS_IRQ_Disable();
	both = (midi_act[0][w] | midi_act[1][w]) & mask;
	midi_act[0][w] &= ~mask; // read AND clear, both sets
	midi_act[1][w] &= ~mask;
	ADIOS_IRQ_Enable();

	// RX is the low bit of the pair and TX the high one - the same order as
	// ADIOS_MIDI_ACT_RX/_TX, so the stored value IS the returned value.
	return (both >> (bit % 32)) & (ADIOS_MIDI_ACT_RX | ADIOS_MIDI_ACT_TX);
#else
	(void)port;
	return 0;
#endif
}





/////////////////////////////////////////////////////////////////////////////
//! This function should be called periodically each mS to handle timeout
//! and expire counters.
//!
//! Not for use in an application - this function is called by
//! by a task in the programming model!
//! 
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_Periodic_mS(void)
{
	s32 status = 0;

#if defined(ADIOS_USE_USB_MIDI)
	status |= ADIOS_USB_MIDI_Periodic_mS();
#endif

#if defined(ADIOS_USE_DIN_MIDI)
	status |= ADIOS_DIN_MIDI_Periodic_mS();
#endif

#if defined(ADIOS_USE_SPI_MIDI)
	status |= ADIOS_SPI_MIDI_Periodic_mS();
#endif

#if defined(ADIOS_USE_MIDI_ACT)
	// rotate the activity sets - see MIDI_ActMark() above for why there are two
	if( ++midi_act_ctr >= ADIOS_MIDI_ACT_MS ) {
		u8 i;
		midi_act_ctr = 0;
		ADIOS_IRQ_Disable();
		for(i=0; i<MIDI_ACT_WORDS; ++i) {
			midi_act[1][i] = midi_act[0][i];
			midi_act[0][i] = 0;
		}
		ADIOS_IRQ_Enable();
	}
#endif


	// increment timeout counter for incoming packages
	// an incomplete event will be timed out after 1000 ticks (1 second)
	if( sysex_timeout_ctr < 65535 )
		++sysex_timeout_ctr;

	return status;
}


/////////////////////////////////////////////////////////////////////////////
//! Installs the Tx callback function which is executed by
//! ADIOS_MIDI_SendPackage_NonBlocking() before the MIDI package will be
//! forwarded to the physical interface.
//!
//! The callback allows following usecases:
//! <UL>
//!   <LI>package filter
//!   <LI>duplicating/routing packages
//!   <LI>monitoring packages (sniffer)
//!   <LI>create virtual busses; loopbacks
//!   <LI>extend available ports (e.g. by an OSC or AOUT port)<BR>
//!       It is recommented to give port extensions a port number >= 0x80 to
//!       avoid incompatibility with future ADIOS port extensions.
//! </UL>
//! \param[in] *callback_tx pointer to callback function:<BR>
//! \code
//!    s32 callback_tx(adios_midi_port_t port, adios_midi_package_t package)
//!    {
//!    }
//! \endcode
//! The package will be forwarded to the physical interface if the function 
//! returns 0.<BR>
//! Should return 1 to filter a package.
//! Should return -2 to initiate a retry (function will be called again)
//! Should return -3 to report any other error.
//! These error codes comply with ADIOS_MIDI_SendPackage_NonBlocking()
//! \return < 0 on errors
//! \note Please use the filtering capabilities with special care - if a port
//! is filtered which is also used for code upload, you won't be able to exchange
//! the erroneous code w/o starting the bootloader in hold mode after power-on.
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_DirectTxCallback_Init(s32 (*callback_tx)(adios_midi_port_t port, adios_midi_package_t package))
{
	direct_tx_callback_func = callback_tx;

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Installs the Rx callback function which is executed immediately on each
//! incoming/outgoing MIDI byte, partly from interrupt handlers.
//!
//! This function should be executed so fast as possible. It can be used
//! to trigger MIDI Rx LEDs or to trigger on MIDI clock events. In order to
//! avoid MIDI buffer overruns, the max. recommented execution time is 100 uS!
//!
//! It is possible to filter incoming MIDI bytes with the return value of the
//! callback function.<BR>
//! \param[in] *callback_rx pointer to callback function:<BR>
//! \code
//!    s32 callback_rx(adios_midi_port_t port, u8 midi_byte)
//!    {
//!    }
//! \endcode
//! The byte will be forwarded into the MIDI Rx queue if the function returns 0.<BR>
//! It will be filtered out if the callback returns != 0 (e.g. 1 for "filter", 
//! or -1 for "error").
//! \return < 0 on errors
//! \note Please use the filtering capabilities with special care - if a port
//! is filtered which is also used for code upload, you won't be able to exchange
//! the erroneous code w/o starting the bootloader in hold mode after power-on.
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_DirectRxCallback_Init(s32 (*callback_rx)(adios_midi_port_t port, u8 midi_byte))
{
	direct_rx_callback_func = callback_rx;

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! This function is used by ADIOS internal functions to forward received
//! MIDI bytes to the Rx Callback routine.
//!
//! It shouldn't be used by applications.
//! \param[in] port MIDI port (DEFAULT, USB0..USB31, DIN0..DIN15, SPIM0..SPIM15)
//! \param[in] midi_byte received MIDI byte
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_SendByteToRxCallback(adios_midi_port_t port, u8 midi_byte)
{
	// byte level too, so SysEx counts as activity even when it never
	// reaches the package stage
	MIDI_ActMark(port, 0, midi_byte);

	// note: here we could filter the user hook execution on special situations
	if( direct_rx_callback_func != NULL )
		return direct_rx_callback_func(port, midi_byte);
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! This function is used by ADIOS internal functions to forward received
//! MIDI packages to the Rx Callback routine (byte by byte)
//!
//! It shouldn't be used by applications.
//! \param[in] port MIDI port (DEFAULT, USB0..USB31, DIN0..DIN15, SPIM0..SPIM15)
//! \param[in] midi_package received MIDI package
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_SendPackageToRxCallback(adios_midi_port_t port, adios_midi_package_t midi_package)
{
	// note: here we could filter the user hook execution on special situations
	if( direct_rx_callback_func != NULL ) {
		u8 buffer[3] = {midi_package.evnt0, midi_package.evnt1, midi_package.evnt2};
		int len = adios_midi_pcktype_num_bytes[midi_package.cin];
		int i;
		s32 status = 0;
		for(i=0; i<len; ++i)
			status |= direct_rx_callback_func(port, buffer[i]);
		return status;
	}
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! This function allows to change the DEFAULT port.<BR>
//! The preset which will be used after application reset can be set in
//! adios_config.h via "#define ADIOS_MIDI_DEFAULT_PORT <port>".<BR>
//! It's set to USB0 as long as not overruled in adios_config.h
//! \param[in] port MIDI port (USB0..USB31, DIN0..DIN15, SPIM0..SPIM15)
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_DefaultPortSet(adios_midi_port_t port)
{
	if( port == DEFAULT ) // avoid recursion
		return -1;

	default_port = port;

	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! This function returns the DEFAULT port
//! \return the default port
/////////////////////////////////////////////////////////////////////////////
adios_midi_port_t ADIOS_MIDI_DefaultPortGet(void)
{
	return default_port;
}


/////////////////////////////////////////////////////////////////////////////
//! This function allows to change the MIDI_DEBUG port.<BR>
//! The preset which will be used after application reset can be set in
//! adios_config.h via "#define ADIOS_MIDI_DEBUG_PORT <port>".<BR>
//! It's set to USB0 as long as not overruled in adios_config.h
//! \param[in] port MIDI port (USB0..USB31, DIN0..DIN15, SPIM0..SPIM15)
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_DebugPortSet(adios_midi_port_t port)
{
	if( port == MIDI_DEBUG ) // avoid recursion
		return -1;

	debug_port = port;

	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! This function returns the MIDI_DEBUG port
//! \return the debug port
/////////////////////////////////////////////////////////////////////////////
adios_midi_port_t ADIOS_MIDI_DebugPortGet(void)
{
	return debug_port;
}


/////////////////////////////////////////////////////////////////////////////
//! This function sets the SysEx Device ID, which is used during parsing
//! incoming SysEx Requests to ADIOS<BR>
//! It can also be used by an application for additional parsing with the same ID.<BR>
//! ID changes will get lost after reset. It can be changed permanently by the
//! user via the bootloader update tool
//! \param[in] device_id a new (temporary) device ID (0x00..0x7f)
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_DeviceIDSet(u8 device_id)
{
	sysex_device_id = device_id & 0x7f;
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! This function returns the SysEx Device ID, which is used during parsing
//! incoming SysEx Requests to ADIOS<BR>
//! It can also be used by an application for additional parsing with the same ID.<BR>
//! The initial ID is stored inside the BSL range and will be recovered after
//! reset. It can be changed by the user with the bootloader update tool
//! \return SysEx device ID (0x00..0x7f)
/////////////////////////////////////////////////////////////////////////////
u8 ADIOS_MIDI_DeviceIDGet(void)
{
	return sysex_device_id;
}


/////////////////////////////////////////////////////////////////////////////
//! Installs an optional SysEx callback which is called by 
//! ADIOS_MIDI_Receive_Handler() to simplify the parsing of SysEx streams.
//!
//! Without this callback (or with ADIOS_MIDI_SysExCallback_Init(NULL)),
//! SysEx messages are only forwarded to APP_MIDI_NotifyPackage() in chunks of 
//! 1, 2 or 3 bytes, tagged with midi_package.type == 0x4..0x7 or 0xf
//! 
//! In this case, the application has to take care for different transmission
//! approaches which are under control of the package sender. E.g., while Windows
//! uses Package Type 4..7 to transmit a SysEx stream, PortMIDI under MacOS sends 
//! a mix of 0xf (single byte) and 0x4 (continued 3-byte) packages instead.
//! 
//! By using the SysEx callback, the type of package doesn't play a role anymore,
//! instead the application can parse a serial stream.
//!
//! ADIOS ensures, that realtime events (0xf8..0xff) are still forwarded to
//! APP_MIDI_NotifyPackage(), regardless if they are transmitted in a package
//! type 0x5 or 0xf, so that the SysEx parser doesn't need to filter out such
//! events, which could otherwise appear inside a SysEx stream.
//! 
//! \param[in] *callback_sysex pointer to callback function:<BR>
//! \code
//!    s32 callback_sysex(adios_midi_port_t port, u8 sysex_byte)
//!    {
//!       //
//!       // .. parse stream
//!       //
//!     
//!       return 1; // don't forward package to APP_MIDI_NotifyPackage()
//!    }
//! \endcode
//! If the function returns 0, SysEx bytes will be forwarded to APP_MIDI_NotifyPackage() as well.
//! With return value != 0, APP_MIDI_NotifyPackage() won't get the already processed package.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_SysExCallback_Init(s32 (*callback_sysex)(adios_midi_port_t port, u8 midi_in))
{
	sysex_callback_func = callback_sysex;

	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// This function parses an incoming sysex stream for ADIOS commands
/////////////////////////////////////////////////////////////////////////////
static s32 ADIOS_MIDI_SYSEX_Parser(adios_midi_port_t port, u8 midi_in)
{
	// ignore realtime messages (see MIDI spec - realtime messages can
	// always be injected into events/streams, and don't change the running status)
	if( midi_in >= 0xf8 )
		return 0;

	// TODO: here we could send an error notification, that multiple devices are trying to access the device
	if( sysex_state.general.MY_SYSEX && port != last_sysex_port )
		return -1;

	// USB upload is only allowed via USB0
	// this covers the scenario where other USB1..15 ports are used for MIDI Port forwarding, and the core
	// is connected to one of these ports
	// ADIOS Studio reports "Detected ADIOS response - selection not supported yet!" in this case
	// ignoring USB1..USB15 keeps multi-port hosts working
	if( port >= USB1 && port <= USB15 )
		return -1;

	last_sysex_port = port;

	// branch depending on state
	if( !sysex_state.general.MY_SYSEX ) {
		if( (sysex_state.general.CTR < sizeof(adios_midi_sysex_header) && midi_in != adios_midi_sysex_header[sysex_state.general.CTR]) ||
				(sysex_state.general.CTR == sizeof(adios_midi_sysex_header) && midi_in != sysex_device_id) ) {
			// incoming byte doesn't match
			ADIOS_MIDI_SYSEX_CmdFinished(port);
		} else {
			if( ++sysex_state.general.CTR > sizeof(adios_midi_sysex_header) ) {
				// complete header received, waiting for data
				sysex_state.general.MY_SYSEX = 1;
			}
		}
	} else {
		// check for end of SysEx message or invalid status byte
		if( midi_in >= 0x80 ) {
			if( midi_in == 0xf7 && sysex_state.general.CMD ) {
				ADIOS_MIDI_SYSEX_Cmd(port, ADIOS_MIDI_SYSEX_CMD_STATE_END, midi_in);
			}
			ADIOS_MIDI_SYSEX_CmdFinished(port);
		} else {
			// check if command byte has been received
			if( !sysex_state.general.CMD ) {
				sysex_state.general.CMD = 1;
				sysex_cmd = midi_in;
				ADIOS_MIDI_SYSEX_Cmd(port, ADIOS_MIDI_SYSEX_CMD_STATE_BEGIN, midi_in);
			}
			else
				ADIOS_MIDI_SYSEX_Cmd(port, ADIOS_MIDI_SYSEX_CMD_STATE_CONT, midi_in);
		}
	}

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// This function is called at the end of a sysex command or on 
// an invalid message
/////////////////////////////////////////////////////////////////////////////
static s32 ADIOS_MIDI_SYSEX_CmdFinished(adios_midi_port_t port)
{
	// clear all status variables
	sysex_state.ALL = 0;
	sysex_cmd = 0;
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// This function handles the sysex commands
/////////////////////////////////////////////////////////////////////////////
static s32 ADIOS_MIDI_SYSEX_Cmd(adios_midi_port_t port, adios_midi_sysex_cmd_state_t cmd_state, u8 midi_in)
{


#if ADIOS_MIDI_BSL_ENHANCEMENTS
	// this compile switch should only be activated for the bootloader!
	if( BSL_SYSEX_Cmd(port, cmd_state, midi_in, sysex_cmd) >= 0 )
		return 0; // BSL has serviced this command - no error
#endif
	switch( sysex_cmd ) {
	case 0x00:
		ADIOS_MIDI_SYSEX_Cmd_Query(port, cmd_state, midi_in);
		break;
	case 0x0d:
		ADIOS_MIDI_SYSEX_Cmd_Debug(port, cmd_state, midi_in);
		break;
	case 0x0e: // ignore to avoid loopbacks
		break;
	case 0x0f:
		ADIOS_MIDI_SYSEX_Cmd_Ping(port, cmd_state, midi_in);
		break;
	default:
		// unknown command
		// TODO: send 0xf7 if merger enabled
		ADIOS_MIDI_SYSEX_SendAck(port, ADIOS_MIDI_SYSEX_DISACK, ADIOS_MIDI_SYSEX_DISACK_INVALID_COMMAND);
		ADIOS_MIDI_SYSEX_CmdFinished(port);
	}

	return 0; // no error
}



/////////////////////////////////////////////////////////////////////////////
// Command 00: Query core informations and request BSL entry
/////////////////////////////////////////////////////////////////////////////
static s32 ADIOS_MIDI_SYSEX_Cmd_Query(adios_midi_port_t port, adios_midi_sysex_cmd_state_t cmd_state, u8 midi_in)
{
	static u8 query_req = 0;
	char str_buffer[40];

	switch( cmd_state ) {

	case ADIOS_MIDI_SYSEX_CMD_STATE_BEGIN:
		query_req = 0;
		break;

	case ADIOS_MIDI_SYSEX_CMD_STATE_CONT:
		query_req = midi_in;
		break;

	default: // ADIOS_MIDI_SYSEX_CMD_STATE_END
		switch( query_req ) {
		case 0x01: // operating system
			// (a legacy multi-cable workaround lived here - flooding the IN
			// pipe with ActiveSense on the first query, for a Windows bug in
			// the old USB stack. It leaned on an API that stack took with it,
			// and only ever compiled with more than one cable, which is why
			// it survived unnoticed until 2026-08-17.)
			// the operating system's own name. ADIOS Studio checks it before
			// starting an upload, so the two must change together - it
			// accepts both spellings (UploadHandler.cpp).
			ADIOS_MIDI_SYSEX_SendAckStr(port, "ADIOS");
			break;
		case 0x02: // Processor
			// the exact part (STM32G070CB...), not its family: the family is
			// implied by it, while the part number is what actually tells you
			// what you are talking to - flash and RAM size, peripherals.
			// Comes from the project's own PROCESSOR, see adios/adios.mk
			ADIOS_MIDI_SYSEX_SendAckStr(port, ADIOS_PROCESSOR_STR);
			break;
		case 0x03: // Chip ID
			sprintf(str_buffer, "%08x", ADIOS_SYS_ChipIDGet());
			ADIOS_MIDI_SYSEX_SendAckStr(port, (char *)str_buffer);
			break;
		case 0x04: // Serial Number
			if( ADIOS_SYS_SerialNumberGet((char *)str_buffer) >= 0 )
				ADIOS_MIDI_SYSEX_SendAckStr(port, str_buffer);
			else
				ADIOS_MIDI_SYSEX_SendAckStr(port, "?");
			break;
		case 0x05: // Flash Memory Size
			sprintf(str_buffer, "%d", ADIOS_SYS_FlashSizeGet());
			ADIOS_MIDI_SYSEX_SendAckStr(port, str_buffer);
			break;
		case 0x06: // RAM Memory Size
			sprintf(str_buffer, "%d", ADIOS_SYS_RAMSizeGet());
			ADIOS_MIDI_SYSEX_SendAckStr(port, str_buffer);
			break;
		case 0x07: // Application Name Line #1
			ADIOS_MIDI_SYSEX_SendAckStr(port, ADIOS_APP_NAME1);
			break;
		case 0x08: // Application Name Line #2
			ADIOS_MIDI_SYSEX_SendAckStr(port, ADIOS_APP_NAME2);
			break;
		case 0x09: // Application Version
			// the program's own version, declared next to its two name lines.
			// The bootloader and the BSL-update tool answer their own, so a
			// host can read which bootloader is installed, not only which
			// application is running.
			ADIOS_MIDI_SYSEX_SendAckStr(port, ADIOS_APP_VERSION);
			break;
#ifdef ADIOS_APP_FLASH_START_ADDR
		case 0x0a: // Application Flash Start Address (bootloader/app boundary, absolute address)
			sprintf(str_buffer, "%08x", 0x08000000 + ADIOS_APP_FLASH_START_ADDR);
			ADIOS_MIDI_SYSEX_SendAckStr(port, str_buffer);
			break;
#endif
		case 0x0b: // Core type (2026-08-09, for the one-click BSL update flow):
			// "APP" (default), "BSL" (bootloader build) or "UPDATER" (the
			// BSL-update tool) - lets ADIOS Studio decide whether a hex
			// targeting the protected bootloader range may be sent (UPDATER
			// only), and whether the core understands the entry-override
			// command (BSL). Legacy firmware without this query answers
			// DISACK - Studio treats that as "APP"/old-generation.
			ADIOS_MIDI_SYSEX_SendAckStr(port, ADIOS_MIDI_CORE_TYPE_STR);
			break;
		case 0x7f:
#if !ADIOS_USE_BOOTLOADER
			// nothing to reboot INTO: this core has no bootloader, so the
			// flag-and-reset below would just restart the application on
			// itself - and ADIOS Studio, which retries, would loop it. Say so
			// instead, and stay running.
			ADIOS_MIDI_SYSEX_SendAck(port, ADIOS_MIDI_SYSEX_DISACK, ADIOS_MIDI_SYSEX_DISACK_UNKNOWN_QUERY);
#elif ADIOS_MIDI_BSL_ENHANCEMENTS
			// release halt state (or sending upload request) instead of reseting the core
			BSL_SYSEX_ReleaseHaltState();
#else
			// "wait" handshake (2026-08-09): acknowledge the reboot request
			// BEFORE resetting - ADIOS Studio's one-click update flow used to
			// hear nothing at all between this query and the bootloader's
			// upload request after reset, and its wait window expired during
			// the reboot gap. On this ack (arg = 0x7f, echoing the query
			// number) Studio extends its window and waits for the BSL's
			// "ready" (the upload request). See UploadHandlerThread::run()
			// in ADIOS Studio.
			ADIOS_MIDI_SYSEX_SendAck(port, ADIOS_MIDI_SYSEX_ACK, 0x7f);
			// let the ack physically leave the wire before the reset kills
			// the peripheral: the 9-byte SysEx takes ~2.9 mS at MIDI baudrate
			ADIOS_DELAY_Wait_uS(10000);
			// reset core (this will send an upload request)
			// tell the bootloader to stay resident after this reset, so the
			// user never has to touch the physical BSL_HOLD pin for a normal
			// firmware update (implemented per family in adios_sys.c -
			// TAMP/RTC backup register, both G0xx and F4xx covered)
			ADIOS_SYS_BootloaderModeRequest();
			ADIOS_SYS_Reset();
			// at least on STM32 we will never reach this point
			// but other core families could contain an empty stumb!
#endif
			break;
		default:
			// unknown query
			ADIOS_MIDI_SYSEX_SendAck(port, ADIOS_MIDI_SYSEX_DISACK, ADIOS_MIDI_SYSEX_DISACK_UNKNOWN_QUERY);
		}
	}

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Command 0D: Debug Input/Output
/////////////////////////////////////////////////////////////////////////////
static s32 ADIOS_MIDI_SYSEX_Cmd_Debug(adios_midi_port_t port, adios_midi_sysex_cmd_state_t cmd_state, u8 midi_in)
{
#ifdef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
	// send disacknowledge
	if( cmd_state == ADIOS_MIDI_SYSEX_CMD_STATE_END )
		ADIOS_MIDI_SYSEX_SendAck(port, ADIOS_MIDI_SYSEX_DISACK, ADIOS_MIDI_SYSEX_DISACK_UNSUPPORTED_DEBUG);
#else
	static u8 debug_req = 0xff;

	switch( cmd_state ) {

	case ADIOS_MIDI_SYSEX_CMD_STATE_BEGIN:
		debug_req = 0xff;
		break;

	case ADIOS_MIDI_SYSEX_CMD_STATE_CONT:
		if( debug_req == 0xff ) {
			debug_req = midi_in;
		} else {
			switch( debug_req ) {
			case 0x00: // input string
				if( debug_command_callback_func != NULL )
					debug_command_callback_func(last_sysex_port, (char)midi_in);
				break;

			case 0x01: // input string to filebrowser
				if( filebrowser_command_callback_func != NULL )
					filebrowser_command_callback_func(last_sysex_port, (char)midi_in);
				break;

			case 0x40: // output string
			case 0x41: // output string for filebrowser
				// not supported - DisAck will be sent
				break;

			default: // others
				// not supported - DisAck will be sent
				break;
			}
		}
		break;

	default: // ADIOS_MIDI_SYSEX_CMD_STATE_END
		if( debug_req == 0x00 ) {
			// send acknowledge
			ADIOS_MIDI_SYSEX_SendAck(port, ADIOS_MIDI_SYSEX_ACK, 0x00);

			if( debug_req == 0 && debug_command_callback_func == NULL ) {
				adios_midi_port_t prev_debug_port = ADIOS_MIDI_DebugPortGet();
				ADIOS_MIDI_DebugPortSet(port);
				ADIOS_MIDI_SendDebugString("[ADIOS_MIDI_SYSEX_Cmd_Debug] command handler not implemented by application\n");
				ADIOS_MIDI_DebugPortSet(prev_debug_port);
			}

		} else if( debug_req == 0x01 && filebrowser_command_callback_func != NULL ) {
			// we expect that the filebrowser handler sends back a string
		} else {
			// send disacknowledge
			ADIOS_MIDI_SYSEX_SendAck(port, ADIOS_MIDI_SYSEX_DISACK, ADIOS_MIDI_SYSEX_DISACK_UNSUPPORTED_DEBUG);
		}
	}

	return 0; // no error
#endif
}

/////////////////////////////////////////////////////////////////////////////
// Command 0F: Ping (just send back acknowledge if no additional byte has been received)
/////////////////////////////////////////////////////////////////////////////
static s32 ADIOS_MIDI_SYSEX_Cmd_Ping(adios_midi_port_t port, adios_midi_sysex_cmd_state_t cmd_state, u8 midi_in)
{
	switch( cmd_state ) {

	case ADIOS_MIDI_SYSEX_CMD_STATE_BEGIN:
		sysex_state.ping.PING_BYTE_RECEIVED = 0;
		break;

	case ADIOS_MIDI_SYSEX_CMD_STATE_CONT:
		sysex_state.ping.PING_BYTE_RECEIVED = 1;
		break;

	default: // ADIOS_MIDI_SYSEX_CMD_STATE_END
		// TODO: send 0xf7 if merger enabled

		// send acknowledge if no additional byte has been received
		// to avoid feedback loop if two cores are directly connected
		if( !sysex_state.ping.PING_BYTE_RECEIVED )
			ADIOS_MIDI_SYSEX_SendAck(port, ADIOS_MIDI_SYSEX_ACK, 0x00);

		break;
	}

	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// This function sends a SysEx acknowledge to notify the user about the received command
// expects acknowledge code (e.g. 0x0f for good, 0x0e for error) and additional argument
/////////////////////////////////////////////////////////////////////////////
static s32 ADIOS_MIDI_SYSEX_SendAck(adios_midi_port_t port, u8 ack_code, u8 ack_arg)
{
	u8 sysex_buffer[32]; // should be enough?
	u8 *sysex_buffer_ptr = &sysex_buffer[0];
	int i;

	for(i=0; i<sizeof(adios_midi_sysex_header); ++i)
		*sysex_buffer_ptr++ = adios_midi_sysex_header[i];

	// device ID
	*sysex_buffer_ptr++ = ADIOS_MIDI_DeviceIDGet();

	// send ack code and argument
	*sysex_buffer_ptr++ = ack_code;
	*sysex_buffer_ptr++ = ack_arg;

	// send footer
	*sysex_buffer_ptr++ = 0xf7;

	// finally send SysEx stream
	return ADIOS_MIDI_SendSysEx(port, (u8 *)sysex_buffer, (u32)sysex_buffer_ptr - ((u32)&sysex_buffer[0]));
}

/////////////////////////////////////////////////////////////////////////////
// This function sends an SysEx acknowledge with a string (used on queries)
/////////////////////////////////////////////////////////////////////////////
static s32 ADIOS_MIDI_SYSEX_SendAckStr(adios_midi_port_t port, char *str)
{
	u8 sysex_buffer[128]; // should be enough?
	u8 *sysex_buffer_ptr = &sysex_buffer[0];
	int i;

	for(i=0; i<sizeof(adios_midi_sysex_header); ++i)
		*sysex_buffer_ptr++ = adios_midi_sysex_header[i];

	// device ID
	*sysex_buffer_ptr++ = ADIOS_MIDI_DeviceIDGet();

	// send ack code
	*sysex_buffer_ptr++ = ADIOS_MIDI_SYSEX_ACK;

	// send string
	for(i=0; i<100 && (str[i] != 0); ++i)
		*sysex_buffer_ptr++ = str[i];

	// send footer
	*sysex_buffer_ptr++ = 0xf7;

	// finally send SysEx stream
	return ADIOS_MIDI_SendSysEx(port, (u8 *)sysex_buffer, (u32)sysex_buffer_ptr - ((u32)&sysex_buffer[0]));
}


/////////////////////////////////////////////////////////////////////////////
//! Installs the debug command callback function which is executed on incoming
//! characters from a ADIOS Terminal
//!
//! Example:
//! \code
//! s32 CONSOLE_Parse(adios_midi_port_t port, char c)
//! {
//!   // see $ADIOS_PATH/apps/examples/midi_console/
//!   
//!   return 0; // no error
//! }
//! \endcode
//!
//! The callback function has been installed in an Init() function with:
//! \code
//!   ADIOS_MIDI_DebugCommandCallback_Init(CONSOLE_Parse);
//! \endcode
//! \param[in] callback_debug_command the callback function (NULL disables the callback)
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_DebugCommandCallback_Init(s32 (*callback_debug_command)(adios_midi_port_t port, char c))
{
	debug_command_callback_func = callback_debug_command;

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Installs the filebrowser command callback function which is executed on incoming
//! characters from aMIOS  Filebrowser
//!
//! Usage example: see terminal.c of $ADIOS_PATH/apps/controllers/midio128
//!
//! The callback function has been installed in an Init() function with:
//! \code
//!   ADIOS_MIDI_FilebrowserCommandCallback_Init(CONSOLE_Parse);
//! \endcode
//! \param[in] callback_debug_command the callback function (NULL disables the callback)
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_FilebrowserCommandCallback_Init(s32 (*filebrowser_debug_command)(adios_midi_port_t port, char c))
{
	filebrowser_command_callback_func = filebrowser_debug_command;

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Installs the Timeout callback function which is executed on incomplete
//! MIDI packages received via UART, or on incomplete SysEx streams.
//!
//! A timeout is detected after 1 second.
//!
//! On a timeout, it is recommented to reset MIDI parsing relevant variables,
//! e.g. the state of a SysEx parser.
//!
//! Example:
//! \code
//! s32 NOTIFY_MIDI_TimeOut(adios_midi_port_t port)
//! {
//!   // if my SysEx parser receives a command (MY_SYSEX flag set), abort parser if port matches
//!   if( sysex_state.general.MY_SYSEX && port == last_sysex_port )
//!     MySYSEX_CmdFinished();
//!
//!   return 0; // no error
//! }
//! \endcode
//!
//! The callback function has been installed in an Init() function with:
//! \code
//!   ADIOS_MIDI_TimeOutCallback_Init(NOTIFY_MIDI_TimeOut)
//!   {
//!   }
//! \endcode
//! \param[in] callback_timeout the callback function (NULL disables the callback)
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_MIDI_TimeOutCallback_Init(s32 (*callback_timeout)(adios_midi_port_t port))
{
	timeout_callback_func = callback_timeout;

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// This function is called if a MIDI parser runs into timeout
/////////////////////////////////////////////////////////////////////////////
static s32 ADIOS_MIDI_TimeOut(adios_midi_port_t port)
{
	// if ADIOS receives a SysEx command (MY_SYSEX flag set), abort parser if port matches
	if( sysex_state.general.MY_SYSEX && port == last_sysex_port )
		ADIOS_MIDI_SYSEX_CmdFinished(port);

	// optional hook to application
	if( timeout_callback_func != NULL )
		timeout_callback_func(port);

#ifndef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
	// this debug message should always be active, so that common users are informed about the exception
	ADIOS_MIDI_SendDebugMessage("[ADIOS_MIDI_Receive_Handler] Timeout on port 0x%02x\n", port);
#endif

	return 0; // no error
}
