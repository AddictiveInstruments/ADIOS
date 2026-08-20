/*
 * BSL SysEx Parser
 *
 * ==========================================================================
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
#include <string.h>
#include "app.h"		// APP_SPI_MutexTake/Give

/* Includes ------------------------------------------------------------------*/
#include "tr5x6_rom.h"
#include "tr5x6_sysex.h"

/////////////////////////////////////////////////////////////////////////////
// Local types
/////////////////////////////////////////////////////////////////////////////

typedef union {
	struct {
		unsigned ALL:8;
	};

	struct {
		unsigned CTR:3;
		unsigned :1;
		unsigned :1;
		unsigned :1;
		unsigned CMD:1;
		unsigned MY_SYSEX:1;
	};

	struct {
		unsigned :1;
		unsigned :1;
		unsigned :1;
		unsigned :1;
		unsigned BLOCK_RECEIVED:1;
		unsigned :1;
		unsigned :1;
		unsigned :1;
	};
} sysex_cmd_state_t;

// receive state
typedef enum {
	TR5X6_SYSEX_REC_A3,
	TR5X6_SYSEX_REC_A2,
	TR5X6_SYSEX_REC_A1,
	TR5X6_SYSEX_REC_A0,
	TR5X6_SYSEX_REC_L3,
	TR5X6_SYSEX_REC_L2,
	TR5X6_SYSEX_REC_L1,
	TR5X6_SYSEX_REC_L0,
	TR5X6_SYSEX_REC_PAYLOAD,
	TR5X6_SYSEX_REC_CHECKSUM,
	TR5X6_SYSEX_REC_ID,
	TR5X6_SYSEX_REC_ID_OK,
	TR5X6_SYSEX_REC_INVALID
} sysex_rec_state_t;

/////////////////////////////////////////////////////////////////////////////
// Local Macros
/////////////////////////////////////////////////////////////////////////////

#define MEM32(addr) (*((volatile u32 *)(addr)))
#define MEM16(addr) (*((volatile u16 *)(addr)))
#define MEM8(addr)  (*((volatile u8  *)(addr)))

// SST39xx040: nor flash memory range (512K)
# define ROM_START_ADDR  (0x00000000)
# define ROM_END_ADDR    (0x00080000 - 1)


// command states
#define SYSEX_CMD_STATE_BEGIN 0
#define SYSEX_CMD_STATE_CONT  1
#define SYSEX_CMD_STATE_END   2
/////////////////////////////////////////////////////////////////////////////
// Internal Prototypes
/////////////////////////////////////////////////////////////////////////////
static s32 TR5X6_SYSEX_Cmd(u8 cmd_state, u8 midi_in);
static s32 TR5X6_SYSEX_CmdFinished(void);
static s32 TR5X6_SYSEX_Cmd_ReadMem(u8 cmd_state, u8 midi_in);
static s32 TR5X6_SYSEX_Cmd_WriteMem(u8 cmd_state, u8 midi_in);
static s32 TR5X6_SYSEX_RecAddrAndLen(u8 midi_in);
static s32 TR5X6_SYSEX_SendAck(mios32_midi_port_t port, u8 ack_code, u8 ack_arg);
static s32 TR5X6_SYSEX_SendMem(mios32_midi_port_t port, u32 addr, u32 len);
static s32 TR5X6_SYSEX_WriteMem(u32 addr, u32 len, u8 *buffer);


/////////////////////////////////////////////////////////////////////////////
// Local Variables
/////////////////////////////////////////////////////////////////////////////

static sysex_rec_state_t sysex_rec_state;
static sysex_cmd_state_t sysex_cmd_state;
static u8 sysex_cmd;

static mios32_midi_port_t sysex_port = DEFAULT;
static u32 sysex_addr;
static u32 sysex_len;
static u8 sysex_checksum;
static u8 sysex_received_checksum;
static u32 sysex_receive_ctr;

/////////////////////////////////////////////////////////////////////////////
// constant definitions
/////////////////////////////////////////////////////////////////////////////

// SysEx header of MIDIO128
static const u8 sysex_header[4] = { 0xf0, 0x41, 0x00, 0x1c};	// 505
//static const u8 sysex_header[5] = { 0xf0, 0x41, 0x00, 0x1d};	// 626
/////////////////////////////////////////////////////////////////////////////
// local variables
/////////////////////////////////////////////////////////////////////////////

// ensure that the buffer is located at a word boundary (required for LPC17 flash programming routines)
static u8 sysex_buffer[TR5X6_SYSEX_BUFFER_SIZE] __attribute__ ((aligned (8)));

static u8 halt_state;


/////////////////////////////////////////////////////////////////////////////
// This function initializes the SysEx handler
/////////////////////////////////////////////////////////////////////////////
s32 TR5X6_SYSEX_Init(u32 mode)
{
	if( mode != 0 )
		return -1; // only mode 0 supported

	// set to one when writing flash to prevent the execution of application code
	// so long flash hasn't been programmed completely
	halt_state = 0;
	sysex_port = DEFAULT;
	sysex_rec_state= 0;
	sysex_cmd_state.ALL = 0;
	sysex_cmd = 0;
	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Returns 1 if BSL is in halt state (e.g. code is uploaded)
/////////////////////////////////////////////////////////////////////////////
s32 TR5X6_SYSEX_HaltStateGet(void)
{
	return halt_state;
}


/////////////////////////////////////////////////////////////////////////////
// Used by MIOS32_MIDI to release halt state instead of triggering a reset
/////////////////////////////////////////////////////////////////////////////
s32 TR5X6_SYSEX_ReleaseHaltState(void)
{
	// always send upload request (like if we would come out of reset)
	// DIN2 = the instrument's physical MIDI OUT, where MIOS Studio listens
	TR5X6_SYSEX_SendUploadReq(DIN2);
	TR5X6_SYSEX_SendUploadReq(USB0);

	// clear halt state
	halt_state = 0;

	return 0;
}


/////////////////////////////////////////////////////////////////////////////
// This function parses an incoming sysex stream for SysEx messages
/////////////////////////////////////////////////////////////////////////////
s32 TR5X6_SYSEX_Parser(mios32_midi_port_t port, u8 midi_in)
{
	// TODO: here we could send an error notification, that multiple devices are trying to access the device
	if( sysex_cmd_state.MY_SYSEX && port != sysex_port )
		return 1; // don't forward package to APP_MIDI_NotifyPackage()

	sysex_port = port;

	// branch depending on state
	if( !sysex_cmd_state.MY_SYSEX ) {
		if( midi_in != sysex_header[sysex_cmd_state.CTR] ) {
			// incoming byte doesn't match
			TR5X6_SYSEX_CmdFinished();
		} else {
			if( ++sysex_cmd_state.CTR == sizeof(sysex_header) ) {
				// complete header received, waiting for data
				sysex_cmd_state.MY_SYSEX = 1;

				// disable merger forwarding until end of sysex message
				// TODO
				//	MIOS_MPROC_MergerDisable();
			}
		}
	} else {
		// check for end of SysEx message or invalid status byte
		if( midi_in >= 0x80 ) {
			if( midi_in == 0xf7 && sysex_cmd_state.CMD ) {
				TR5X6_SYSEX_Cmd(SYSEX_CMD_STATE_END, midi_in);
			}
			TR5X6_SYSEX_CmdFinished();
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
			MIOS32_MIDI_SendDebugMessage("parser end\n");
#endif
		} else {
			// check if command byte has been received
			if( !sysex_cmd_state.CMD ) {
				sysex_cmd_state.CMD = 1;
				sysex_cmd = midi_in;
				TR5X6_SYSEX_Cmd(SYSEX_CMD_STATE_BEGIN, midi_in);
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
			MIOS32_MIDI_SendDebugMessage("parser start cmd=x%02x\n", sysex_cmd);
#endif
			}
			else
				TR5X6_SYSEX_Cmd(SYSEX_CMD_STATE_CONT, midi_in);
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
			MIOS32_MIDI_SendDebugMessage("[TR5X6_SYSEX] data 0x%02x, cnt %d\n", midi_in, sysex_receive_ctr);
#endif
		}
	}

	return 1; // don't forward package to APP_MIDI_NotifyPackage()
}


/////////////////////////////////////////////////////////////////////////////
// This function is called at the end of a sysex command or on
// an invalid message
/////////////////////////////////////////////////////////////////////////////
s32 TR5X6_SYSEX_CmdFinished(void)
{
	// clear all status variables
	halt_state = 0;
	sysex_port = DEFAULT;
	sysex_rec_state= 0;
	sysex_cmd_state.ALL = 0;
	sysex_cmd = 0;

	// enable MIDI forwarding again
	// TODO
	//  MIOS_MPROC_MergerEnable();

	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// This function enhances MIOS32 SysEx commands
// it's called from MIOS32_MIDI_SYSEX_Cmd if the "MIOS32_MIDI_TR5X6_ENHANCEMENTS"
// switch is set (see code there for details)
/////////////////////////////////////////////////////////////////////////////
s32 TR5X6_SYSEX_Cmd(u8 cmd_state, u8 midi_in)
{
	//	// change debug port
	//	MIOS32_MIDI_DebugPortSet(port);
	//
	//	// wait 2 additional seconds whenever a SysEx message has been received
	//	MIOS32_STOPWATCH_Reset();

	// enter the commands here
	switch( sysex_cmd ) {
	// case 0x00: // query command is implemented in MIOS32
	// case 0x0f: // ping command is implemented in MIOS32

	case 0x01:
		TR5X6_SYSEX_Cmd_ReadMem(cmd_state, midi_in);
		break;
	case 0x02:
		TR5X6_SYSEX_Cmd_WriteMem(cmd_state, midi_in);
		break;

	default:
		// unknown command
		TR5X6_SYSEX_SendAck(sysex_port, MIOS32_MIDI_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_INVALID_COMMAND);
		break;
	}

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Command 01: Read Memory handler
// TODO: we could provide this command also during runtime, as it isn't destructive
// or it could be available as debug command 0D like known from MIOS8
/////////////////////////////////////////////////////////////////////////////
s32 TR5X6_SYSEX_Cmd_ReadMem(u8 cmd_state, u8 midi_in)
{
	switch( cmd_state ) {

	case MIOS32_MIDI_SYSEX_CMD_STATE_BEGIN:
		// set initial receive state and address/len
		sysex_rec_state = TR5X6_SYSEX_REC_A3;
		sysex_addr = 0;
		sysex_len = 0;
		break;

	case MIOS32_MIDI_SYSEX_CMD_STATE_CONT:
		if( sysex_rec_state < TR5X6_SYSEX_REC_PAYLOAD )
			TR5X6_SYSEX_RecAddrAndLen(midi_in);
		break;

	default: // TR5X6_SYSEX_CMD_STATE_END
		// TODO: send 0xf7 if merger enabled

		// did we reach payload state?
		if( sysex_rec_state != TR5X6_SYSEX_REC_PAYLOAD ) {
			// not enough bytes received
			TR5X6_SYSEX_SendAck(sysex_port, MIOS32_MIDI_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_LESS_BYTES_THAN_EXP);
		} else {
			// send dump
			TR5X6_SYSEX_SendMem(sysex_port, sysex_addr, sysex_len);
		}

		break;
	}

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Command 02: Write Memory handler
/////////////////////////////////////////////////////////////////////////////
s32 TR5X6_SYSEX_Cmd_WriteMem(u8 cmd_state, u8 midi_in)
{
	static u32 bit_ctr8 = 0;
	static u32 value8 = 0;

	switch( cmd_state ) {

	case SYSEX_CMD_STATE_BEGIN:
		// set initial receive state and address/len
		sysex_rec_state = TR5X6_SYSEX_REC_A3;
		sysex_addr = 0;
		sysex_len = 0;
		// clear checksum and receive counters
		sysex_checksum = 0;
		sysex_received_checksum = 0;

		sysex_receive_ctr = 0;
		bit_ctr8 = 0;
		value8 = 0;
		break;

	case SYSEX_CMD_STATE_CONT:
		if( sysex_rec_state < TR5X6_SYSEX_REC_PAYLOAD ) {
			sysex_checksum += midi_in;
			TR5X6_SYSEX_RecAddrAndLen(midi_in);
		} else if( sysex_rec_state == TR5X6_SYSEX_REC_PAYLOAD ) {
			sysex_checksum += midi_in;
			// new byte has been received - descramble and buffer it
			if( sysex_receive_ctr < TR5X6_SYSEX_MAX_BYTES ) {
				u8 value7 = midi_in;
				int bit_ctr7;
				for(bit_ctr7=0; bit_ctr7<7; ++bit_ctr7) {
					value8 = (value8 << 1) | ((value7 & 0x40) ? 1 : 0);
					value7 <<= 1;

					if( ++bit_ctr8 >= 8 ) {
						sysex_buffer[sysex_receive_ctr] = value8;
						bit_ctr8 = 0;
						if( ++sysex_receive_ctr >= sysex_len )
							sysex_rec_state = TR5X6_SYSEX_REC_CHECKSUM;
					}
				}
			}
		} else if( sysex_rec_state == TR5X6_SYSEX_REC_CHECKSUM ) {
			// store received checksum
			sysex_received_checksum = midi_in;
		} else {
			// too many bytes... wait for F7
			sysex_rec_state = TR5X6_SYSEX_REC_INVALID;
		}
		break;

	default: // MIOS32_MIDI_SYSEX_CMD_STATE_END
		// TODO: send 0xf7 if merger enabled

		if( sysex_receive_ctr < sysex_len ) {
			// for remote analysis...
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
			MIOS32_MIDI_SendDebugMessage("[TR5X6_SYSEX] expected %d, got %d bytes (retry)\n", sysex_len, sysex_receive_ctr);
#endif
			// not enough bytes received
			TR5X6_SYSEX_SendAck(sysex_port, MIOS32_MIDI_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_LESS_BYTES_THAN_EXP);
		} else if( sysex_rec_state == TR5X6_SYSEX_REC_INVALID ) {
			// too many bytes received
			TR5X6_SYSEX_SendAck(sysex_port, MIOS32_MIDI_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_MORE_BYTES_THAN_EXP);
		} else if( sysex_received_checksum != (-sysex_checksum & 0x7f) ) {
			// notify that wrong checksum has been received
			TR5X6_SYSEX_SendAck(sysex_port, MIOS32_MIDI_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_WRONG_CHECKSUM);
		} else {
			// enter halt state (can only be released via BSL reset)
			halt_state = 1;

			// write received data into memory
			s32 error;
			if( (error = TR5X6_SYSEX_WriteMem(sysex_addr, sysex_len, sysex_buffer)) ) {
				// write failed - return negated error status
				TR5X6_SYSEX_SendAck(sysex_port, MIOS32_MIDI_SYSEX_DISACK, -error);
			} else {
				// notify that bytes have been received by returning checksum
				TR5X6_SYSEX_SendAck(sysex_port, MIOS32_MIDI_SYSEX_ACK, -sysex_checksum & 0x7f);
			}

			// enfore immediate MIDI queue flush
			// (important for retry handling: send ack before next message is processed)
			//MIOS32_MIDI_Periodic_mS();
		}
		break;
	}

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Help function to receive address and length
/////////////////////////////////////////////////////////////////////////////
static s32 TR5X6_SYSEX_RecAddrAndLen(u8 midi_in)
{
	if( sysex_rec_state <= TR5X6_SYSEX_REC_A0 ) {
		sysex_addr = (sysex_addr << 7) | ((midi_in & 0x7f) << 4);
		if( sysex_rec_state == TR5X6_SYSEX_REC_A0 )
			sysex_rec_state = TR5X6_SYSEX_REC_L3;
		else
			++sysex_rec_state;
	} else if( sysex_rec_state <= TR5X6_SYSEX_REC_L0 ) {
		sysex_len = (sysex_len << 7) | ((midi_in & 0x7f) << 4);
		if( sysex_rec_state == TR5X6_SYSEX_REC_L0 ) {
			sysex_rec_state = TR5X6_SYSEX_REC_PAYLOAD;
		} else {
			++sysex_rec_state;
		}
	} else {
		return -1; // function shouldn't be called in this state
	}

	return 0; // no error
}



/////////////////////////////////////////////////////////////////////////////
// This function sends a SysEx acknowledge to notify the user about the received command
// expects acknowledge code (e.g. 0x0f for good, 0x0e for error) and additional argument
/////////////////////////////////////////////////////////////////////////////
static s32 TR5X6_SYSEX_SendAck(mios32_midi_port_t port, u8 ack_code, u8 ack_arg)
{
	u8 sysex_buffer[32]; // should be enough?
	u8 *sysex_buffer_ptr = &sysex_buffer[0];
	int i;

	for(i=0; i<sizeof(sysex_header); ++i)
		*sysex_buffer_ptr++ = sysex_header[i];

	// device ID
	//*sysex_buffer_ptr++ = MIOS32_MIDI_DeviceIDGet();

	// send ack code and argument
	*sysex_buffer_ptr++ = ack_code;
	*sysex_buffer_ptr++ = ack_arg;

	// send footer
	*sysex_buffer_ptr++ = 0xf7;

	// finally send SysEx stream
	return MIOS32_MIDI_SendSysEx(port, (u8 *)sysex_buffer, (u32)sysex_buffer_ptr - ((u32)&sysex_buffer[0]));
}


/////////////////////////////////////////////////////////////////////////////
// This function sends an upload request
/////////////////////////////////////////////////////////////////////////////
s32 TR5X6_SYSEX_SendUploadReq(mios32_midi_port_t port)
{
	u8 sysex_buffer[32]; // should be enough?
	u8 *sysex_buffer_ptr = &sysex_buffer[0];
	int i;

	for(i=0; i<sizeof(sysex_header); ++i)
		*sysex_buffer_ptr++ = sysex_header[i];

	// device ID
	*sysex_buffer_ptr++ = MIOS32_MIDI_DeviceIDGet();

	// send 0x01 to request code upload
	*sysex_buffer_ptr++ = 0x01;

	// send footer
	*sysex_buffer_ptr++ = 0xf7;

	// finally send SysEx stream
	return MIOS32_MIDI_SendSysEx(port, (u8 *)sysex_buffer, (u32)sysex_buffer_ptr - ((u32)&sysex_buffer[0]));
}


/////////////////////////////////////////////////////////////////////////////
// This function sends a SysEx dump of the requested memory address range
// We expect that address and length are aligned to 16
/////////////////////////////////////////////////////////////////////////////
s32 TR5X6_SYSEX_SendMem(mios32_midi_port_t port, u32 addr, u32 len)
{
	int i;
	u8 checksum = 0;

	// send header
	u8 *sysex_buffer_ptr = &sysex_buffer[0];
	for(i=0; i<sizeof(sysex_header); ++i)
		*sysex_buffer_ptr++ = sysex_header[i];

	// device ID
	*sysex_buffer_ptr++ = MIOS32_MIDI_DeviceIDGet();

	// "write mem" command (so that dump could be sent back to overwrite the memory w/o modifications)
	*sysex_buffer_ptr++ = 0x02;

	// send 32bit address (divided by 16) in 7bit format
	checksum += *sysex_buffer_ptr++ = (addr >> 25) & 0x7f;
	checksum += *sysex_buffer_ptr++ = (addr >> 18) & 0x7f;
	checksum += *sysex_buffer_ptr++ = (addr >> 11) & 0x7f;
	checksum += *sysex_buffer_ptr++ = (addr >>  4) & 0x7f;

	// send 32bit range (divided by 16) in 7bit format
	checksum += *sysex_buffer_ptr++ = (len >> 25) & 0x7f;
	checksum += *sysex_buffer_ptr++ = (len >> 18) & 0x7f;
	checksum += *sysex_buffer_ptr++ = (len >> 11) & 0x7f;
	checksum += *sysex_buffer_ptr++ = (len >>  4) & 0x7f;

	// send memory content in scrambled format (8bit values -> 7bit values)
	// ROM reads from the MIDI task - serialized (see APP_SPI_MutexTake, app.h)
	APP_SPI_MutexTake();
	u8 value7 = 0;
	u8 bit_ctr7 = 0;
	i=0;
	for(i=0; i<len; ++i) {
		u8 value8 = TR5X6_ROM_Read(addr+i);
		u8 bit_ctr8;
		for(bit_ctr8=0; bit_ctr8<8; ++bit_ctr8) {
			value7 = (value7 << 1) | ((value8 & 0x80) ? 1 : 0);
			value8 <<= 1;

			if( ++bit_ctr7 >= 7 ) {
				checksum += *sysex_buffer_ptr++ = (value7 << (7-bit_ctr7));
				value7 = 0;
				bit_ctr7 = 0;
			}
		}
	}
	APP_SPI_MutexGive();

	if( bit_ctr7 )
		checksum += *sysex_buffer_ptr++ = value7;

	// send checksum
	*sysex_buffer_ptr++ = -checksum & 0x7f;

	// send footer
	*sysex_buffer_ptr++ = 0xf7;

	// finally send SysEx stream
	return MIOS32_MIDI_SendSysEx(port, (u8 *)sysex_buffer, (u32)sysex_buffer_ptr - ((u32)&sysex_buffer[0]));
}


/////////////////////////////////////////////////////////////////////////////
// This function writes into a memory
// We expect that address and length are aligned to 4
/////////////////////////////////////////////////////////////////////////////
static s32 TR5X6_SYSEX_WriteMem(u32 addr, u32 len, u8 *buffer)
{
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
	MIOS32_MIDI_SendDebugMessage("Write begin @0x%08x for %d bytes\n", addr, len);
#endif
	// check for flash memory range
	if( addr >= ROM_START_ADDR && addr <= ROM_END_ADDR ) {

		// check for alignment
		if( (addr % 8) || (len % 8) )
			return -MIOS32_MIDI_SYSEX_DISACK_ADDR_NOT_ALIGNED;
		tr5x6_rom_status status;
		int i;
		// ROM erase/write from the MIDI task - serialized (see app.h)
		APP_SPI_MutexTake();
		for(i=0; i<len; addr++, i++) {

			if( (addr % TR5X6_ROM_SECTOR_SIZE) == 0 ) {
				if((status=TR5X6_ROM_Sector_Erase(addr, 1000))!=TR5X6_ROM_OK) {
					APP_SPI_MutexGive();
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
					MIOS32_MIDI_SendDebugMessage("erase failed for 0x%08x: code %d\n", addr, status);
#endif
					return -MIOS32_MIDI_SYSEX_DISACK_WRITE_FAILED;
				}
			}

			if( (status=TR5X6_ROM_Write(addr, buffer[i], 1000)) != TR5X6_ROM_OK ) {
				APP_SPI_MutexGive();
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
				MIOS32_MIDI_SendDebugMessage("write failed for data 0x%02x @0x%08x: code %d\n", buffer[i], addr, status);
#endif
				return -MIOS32_MIDI_SYSEX_DISACK_WRITE_FAILED;
			}
			// TODO: verify programmed code
		}
		APP_SPI_MutexGive();

		return 0; // no error
	}else{

	// invalid address
	return -MIOS32_MIDI_SYSEX_DISACK_WRONG_ADDR_RANGE;
	}

}




