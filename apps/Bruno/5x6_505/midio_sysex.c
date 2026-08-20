/*
 * SysEx Parser for MIDIO128 V3
 *
 * ==========================================================================
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

#include <mios32.h>

#include "app.h"
#include "midio_sysex.h"
#include "tr5x6_rom.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/////////////////////////////////////////////////////////////////////////////
// local definitions
/////////////////////////////////////////////////////////////////////////////


// help constant - don't change!
#define MIDIO_SYSEX_BLOCK_SIZE  (u16)512
#define MIDIO_SYSEX_INFO_SIZE  (u16)(44+8+4)

// command states
#define MIDIO_SYSEX_CMD_STATE_BEGIN 0
#define MIDIO_SYSEX_CMD_STATE_CONT  1
#define MIDIO_SYSEX_CMD_STATE_END   2

// ack/disack code
#define MIDIO_SYSEX_DISACK   0x0e
#define MIDIO_SYSEX_ACK      0x0f

// disacknowledge arguments
#define MIDIO_SYSEX_DISACK_LESS_BYTES_THAN_EXP  0x01
#define MIDIO_SYSEX_DISACK_MORE_BYTES_THAN_EXP  0x02
#define MIDIO_SYSEX_DISACK_WRONG_CHECKSUM       0x03
#define MIDIO_SYSEX_DISACK_BS_NOT_AVAILABLE     0x0a
#define MIDIO_SYSEX_DISACK_INVALID_COMMAND      0x0c

#define MIDIO_SYSEX_ACK_INFO_DONE		      	0x01
#define MIDIO_SYSEX_ACK_CONTINUE		      	0x02
#define MIDIO_SYSEX_ACK_BANK_RDY		      	0x03
#define MIDIO_SYSEX_ACK_BANK_END		      	0x04
#define MIDIO_SYSEX_ACK_CMD_END			      	0x05


#if TR5X6_UNIT_SELECT==505
		#define TR5X6_SYSEX_ACK_UNIT_TYPE      	0x50
#else // TR5X6_UNIT_SELECT==626
		#define TR5X6_SYSEX_ACK_UNIT_TYPE      	0x62
#endif
#define MIDIO_SYSEX_XFER_TIMEOUT      			1000


/////////////////////////////////////////////////////////////////////////////
// Type definitions
/////////////////////////////////////////////////////////////////////////////

typedef union {
	struct {
		u8 ALL;
	};

	struct {
		u8 CTR:3;
		u8 :1;
		u8 :1;
		u8 DEV_ID:1;
		u8 CMD:1;
		u8 MY_SYSEX:1;
	};

	struct {
		u8 BANK_RECEIVED:1;
		u8 SLOT_RECEIVED:1;
		u8 BLOCK_RECEIVED:1;
		u8 :1;
		u8 :1;
		u8 :1;
		u8 :1;
		u8 :1;
	};
} sysex_state_t;




/////////////////////////////////////////////////////////////////////////////
// Internal Prototypes
/////////////////////////////////////////////////////////////////////////////

static s32 MIDIO_SYSEX_Cmd(u8 cmd_state, u8 midi_in);
static s32 MIDIO_SYSEX_Cmd_Finished(void);
static s32 MIDIO_SYSEX_Cmd_BankDataStart(u8 cmd_state, u8 midi_in);
static s32 MIDIO_SYSEX_Cmd_BankDataEnd(u8 cmd_state, u8 midi_in);
static s32 MIDIO_SYSEX_Send_Footer(u8 force);

static s32 MIDIO_SYSEX_Cmd_ReadInfo(u8 cmd_state, u8 midi_in);
static s32 MIDIO_SYSEX_Cmd_WriteInfo(u8 cmd_state, u8 midi_in);
static s32 MIDIO_SYSEX_Cmd_ReadBlock(u8 cmd_state, u8 midi_in);
static s32 MIDIO_SYSEX_Cmd_WriteBlock(u8 cmd_state, u8 midi_in);
static s32 MIDIO_SYSEX_Cmd_Ping(u8 cmd_state, u8 midi_in);


/////////////////////////////////////////////////////////////////////////////
// constant definitions
/////////////////////////////////////////////////////////////////////////////

// SysEx header of MIDIO128
static const u8 sysex_header[5] = { 0xf0, 0x00, 0x22, 0x15, 0x44 };


/////////////////////////////////////////////////////////////////////////////
// local variables
/////////////////////////////////////////////////////////////////////////////

static sysex_state_t sysex_state;
static u8 sysex_cmd;
static u8 sysex_last_cmd;

static mios32_midi_port_t sysex_port = DEFAULT;
static u8 sysex_checksum;
static u8 sysex_bank;
static u8 sysex_slot;
static u16 sysex_block;
static u16 sysex_total_block;
static u8 sysex_byte;
static u8 sysex_received_checksum;
static u16 sysex_receive_ctr;
static u8 write_info_req;
static u8 write_block_req;
static u8 sysex_act = 0;
static s16 xfer_time_out=-1;
static u16 sysex_bank_block;
static u16 sysex_bank_block_amount;
static u8 sysex_bank_progress=0;

u8 tr5x6_sysex_block_flag=0;
tr5x6_xfer_state_t tr5x6_xfer_state;
u8 tr5x6_sysex_bank_info_refresh = 0;

// TODO: use malloc function instead of a global array to save RAM
static u8 sysex_buffer[(MIDIO_SYSEX_BLOCK_SIZE>>1)+12];
static u8 datas[TR5X6_ROM_SECTOR_SIZE];

/////////////////////////////////////////////////////////////////////////////
// This function initializes the SysEx handler
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Init(u32 mode)
{
	if( mode != 0 )
		return -1; // only mode 0 supported

	sysex_port = DEFAULT;
	sysex_state.ALL = 0;
	//temp
	//sysex_total_block=33;
	write_block_req=0;

	tr5x6_xfer_state.ALL=0;
	// install SysEx parser
	//MIOS32_MIDI_SysExCallback_Init(MIDIO_SYSEX_Parser);
	// parser called from APP_SYSEX_Parser

	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// This function sends a SysEx dump of the slot info
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Send_Info(mios32_midi_port_t port)
{
	int i;
	int sysex_buffer_ix = 0;
	u8 checksum;
	u8 c;

	// send header
	for(i=0; i<sizeof(sysex_header); ++i)
		sysex_buffer[sysex_buffer_ix++] = sysex_header[i];

	// send device id
	sysex_buffer[sysex_buffer_ix++] = MIOS32_MIDI_DeviceIDGet();

	// "write block" command (so that dump could be sent back to overwrite EEPROM w/o modifications)
	sysex_buffer[sysex_buffer_ix++] = sysex_cmd;

	// write block number
	sysex_buffer[sysex_buffer_ix++] = sysex_bank;
	checksum = sysex_bank;

	// write block number
	sysex_buffer[sysex_buffer_ix++] = sysex_slot;
	checksum += sysex_slot;

	// write block number
	sysex_buffer[sysex_buffer_ix++] = 0;		// doesn't matter here
	//checksum += 0;

	tr5x6_flash_info_t slot;
	slot.bank=sysex_bank;
	slot.slot=sysex_slot;

	// get the slot info (flash access from the MIDI task - serialized against
	// the TFT/ROM tasks, see APP_SPI_MutexTake in app.h)
	APP_SPI_MutexTake();
	if(sysex_cmd==CMD_SLOT_READ_INFO)
		TR5X6_FLASH_SlotRead(&slot);
	else TR5X6_FLASH_BankRead(&slot);
	APP_SPI_MutexGive();

	// add slot name
	for(i=0; i<22; i++) {
		c=(u8)slot.name[i];
		// 7bit format - 8th bit discarded
		u8 c_msb = (u8)(c >>4);
		u8 c_lsb = (u8)(c & 0xf);
		sysex_buffer[sysex_buffer_ix++] = c_msb;
		checksum += c_msb;
		sysex_buffer[sysex_buffer_ix++] = c_lsb;
		checksum += c_lsb;
	}
	// add slot color
	c = (u8)(slot.color >>28);
	sysex_buffer[sysex_buffer_ix++] = c;
	checksum += c;
	c = (u8)((slot.color >> 24) & 0xf);
	sysex_buffer[sysex_buffer_ix++] = c;
	checksum += c;
	c = (u8)((slot.color >> 20) & 0xf);
	sysex_buffer[sysex_buffer_ix++] = c;
	checksum += c;
	c = (u8)((slot.color >> 16) & 0xf);
	sysex_buffer[sysex_buffer_ix++] = c;
	checksum += c;
	c = (u8)((slot.color >> 12) & 0xf);
	sysex_buffer[sysex_buffer_ix++] = c;
	checksum += c;
	c = (u8)((slot.color >> 8) & 0xf);
	sysex_buffer[sysex_buffer_ix++] = c;
	checksum += c;
	c = (u8)((slot.color >> 4) & 0xf);
	sysex_buffer[sysex_buffer_ix++] = c;
	checksum += c;
	c = (u8)(slot.color & 0xf);
	sysex_buffer[sysex_buffer_ix++] = c;
	checksum += c;

	// add slot magic
	c = (u8)(slot.magic >>12);
	sysex_buffer[sysex_buffer_ix++] = c;
	checksum += c;
	c = (u8)((slot.magic >> 8) & 0xf);
	sysex_buffer[sysex_buffer_ix++] = c;
	checksum += c;
	c = (u8)((slot.magic >> 4) & 0xf);
	sysex_buffer[sysex_buffer_ix++] = c;
	checksum += c;
	c = (u8)(slot.magic & 0xf);
	sysex_buffer[sysex_buffer_ix++] = c;
	checksum += c;

	// send checksum
	sysex_buffer[sysex_buffer_ix++] = -checksum & 0x7f;

	// send footer
	sysex_buffer[sysex_buffer_ix++] = 0xf7;

	// finally send SysEx stream and return error status
	sysex_act = 2;
	return MIOS32_MIDI_SendSysEx(port, (u8 *)sysex_buffer, sysex_buffer_ix);
}


/////////////////////////////////////////////////////////////////////////////
// This function sends a SysEx dump of a patch block
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Send_Block(mios32_midi_port_t port)
{
	int i;
	int sysex_buffer_ix = 0;
	u8 checksum;

	// send header
	for(i=0; i<sizeof(sysex_header); ++i)
		sysex_buffer[sysex_buffer_ix++] = sysex_header[i];

	// send device id
	sysex_buffer[sysex_buffer_ix++] = MIOS32_MIDI_DeviceIDGet();

	// "write block" command (so that dump could be sent back to overwrite EEPROM w/o modifications)
	sysex_buffer[sysex_buffer_ix++] = sysex_cmd;

	// write block number
	sysex_buffer[sysex_buffer_ix++] = sysex_bank;
	checksum = sysex_bank;

	// write block number
	sysex_buffer[sysex_buffer_ix++] = sysex_slot;
	checksum += sysex_slot;

	// write block number
	sysex_buffer[sysex_buffer_ix++] = sysex_block;
	checksum += sysex_block;

	// ROM reads from the MIDI task - serialized (see APP_SPI_MutexTake)
	APP_SPI_MutexTake();
	u32 addr;
	switch(tr5x6_slots[sysex_slot].size){
	case SIZE_4K:
		tr5x6_slot_parity_t even_odd = (tr5x6_slot_parity_t)tr5x6_slots[sysex_slot].parity;
		u8 sector= (sysex_block/8);
		u8 block=sysex_block%8;

		addr = TR5X6_ROM_ProgAddr(sysex_bank) | tr5x6_slots[sysex_slot].addr_offset | (sector*TR5X6_ROM_SECTOR_SIZE) | (block<<9) | (even_odd^1);
		for(int i=0;i<(MIDIO_SYSEX_BLOCK_SIZE>>1);i++){
			u8 data = TR5X6_ROM_Read(addr);
			u8 data_msb = (u8)(data >>4);
			u8 data_lsb = (u8)(data & 0xf);
			sysex_buffer[sysex_buffer_ix++] = data_msb;
			checksum += data_msb;
			sysex_buffer[sysex_buffer_ix++] = data_lsb;
			checksum += data_lsb;
			addr +=2;

		}

		break;


	case SIZE_8K:
	case SIZE_16K:
	case SIZE_32K:

		addr = TR5X6_ROM_ProgAddr(sysex_bank) | tr5x6_slots[sysex_slot].addr_offset | (sysex_block<<8);
		for(int i=0; i<(MIDIO_SYSEX_BLOCK_SIZE>>1); addr++, i++) {
			u8 data = TR5X6_ROM_Read(addr);
			u8 data_msb = (u8)(data >>4);
			u8 data_lsb = (u8)(data & 0xf);
			sysex_buffer[sysex_buffer_ix++] = data_msb;
			checksum += data_msb;
			sysex_buffer[sysex_buffer_ix++] = data_lsb;
			checksum += data_lsb;
		}

		break;

	}
	APP_SPI_MutexGive();

	// send checksum
	sysex_buffer[sysex_buffer_ix++] = -checksum & 0x7f;

	// send footer
	sysex_buffer[sysex_buffer_ix++] = 0xf7;

	// finally send SysEx stream and return error status
	sysex_act = 2;
	return MIOS32_MIDI_SendSysEx(port, (u8 *)sysex_buffer, sysex_buffer_ix);
}


/////////////////////////////////////////////////////////////////////////////
// This function sends a SysEx acknowledge to notify the user about the received command
// expects acknowledge code (e.g. 0x0f for good, 0x0e for error) and additional argument
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Send_Ack(mios32_midi_port_t port, u8 ack_code, u8 ack_arg)
{
	int i;
	u8 buffer[10]; // should be enough?
	int buffer_ix = 0;

	// send header
	for(i=0; i<sizeof(sysex_header); ++i)
		buffer[buffer_ix++] = sysex_header[i];

	// send device id
	buffer[buffer_ix++] = MIOS32_MIDI_DeviceIDGet();
	// send ack code and argument
	buffer[buffer_ix++] = ack_code;
	buffer[buffer_ix++] = ack_arg;

	// send footer
	buffer[buffer_ix++] = 0xf7;

	// finally send SysEx stream and return error status
	sysex_act = 2;
	return MIOS32_MIDI_SendSysEx(port, (u8 *)buffer, buffer_ix);
}


/////////////////////////////////////////////////////////////////////////////
// This function is called from NOTIFY_MIDI_TimeOut() in app.c if the 
// MIDI parser runs into timeout
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_TimeOut(mios32_midi_port_t port)
{
	// if we receive a SysEx command (MY_SYSEX flag set), abort parser if port matches
	if( sysex_state.MY_SYSEX && port == sysex_port )
		MIDIO_SYSEX_Cmd_Finished();
	// bus handover back to the host touches the ROM SPI port (Addr_Set) -
	// serialized (MIDI task context)
	APP_SPI_MutexTake();
	TR5X6_ROM_HOST();
	APP_SPI_MutexGive();
	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// This function parses an incoming sysex stream for SysEx messages
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Parser(mios32_midi_port_t port, u8 midi_in)
{
	// TODO: here we could send an error notification, that multiple devices are trying to access the device
	if( sysex_state.MY_SYSEX && port != sysex_port )
		return 1; // don't forward package to APP_MIDI_NotifyPackage()

	sysex_port = port;
	// branch depending on state
	if( !sysex_state.MY_SYSEX ) {
		if( midi_in != sysex_header[sysex_state.CTR] ) {
			// incoming byte doesn't match
			MIDIO_SYSEX_Cmd_Finished();
		} else {
			if( ++sysex_state.CTR == sizeof(sysex_header) ) {
				// complete header received, waiting for data
				sysex_state.MY_SYSEX = 1;
				sysex_state.CTR=0;
				// disable merger forwarding until end of sysex message
				// TODO
				//	MIOS_MPROC_MergerDisable();
			}
		}
	} else {
		// check for end of SysEx message or invalid status byte
		if( midi_in >= 0x80 ) {
			if( midi_in == 0xf7 && sysex_state.CMD ) {
				MIDIO_SYSEX_Cmd(MIDIO_SYSEX_CMD_STATE_END, midi_in);
			}
			MIDIO_SYSEX_Cmd_Finished();
		} else {
			if( !sysex_state.DEV_ID ) {
				if(midi_in==MIOS32_MIDI_DeviceIDGet())sysex_state.DEV_ID = 1;
				else MIDIO_SYSEX_Cmd_Finished();
			}
			// check if command byte has been received
			else if( !sysex_state.CMD ) {
				sysex_state.CMD = 1;
				sysex_cmd = midi_in;
				MIDIO_SYSEX_Cmd(MIDIO_SYSEX_CMD_STATE_BEGIN, midi_in);
			}
			else
				MIDIO_SYSEX_Cmd(MIDIO_SYSEX_CMD_STATE_CONT, midi_in);
		}
	}

	return 1; // don't forward package to APP_MIDI_NotifyPackage()
}

/////////////////////////////////////////////////////////////////////////////
// This function is called at the end of a sysex command or on 
// an invalid message
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Cmd_Finished(void)
{
	// clear all status variables
	sysex_state.ALL = 0;
	sysex_cmd = CMD_IDLE;

	// enable MIDI forwarding again
	// TODO
	//  MIOS_MPROC_MergerEnable();

	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// This function sends the SysEx footer if merger enabled
// if force == 1, send the footer regardless of merger state
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Send_Footer(u8 force)
{
#if 0
	// TODO ("force" not used yet, merger not available yet)
	if( force || (MIOS_MIDI_MergerGet() & 0x01) )
		MIOS_MIDI_TxBufferPut(0xf7);
#endif

	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// This function handles the sysex commands
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Cmd(u8 cmd_state, u8 midi_in)
{
	// enter the commands here
	switch( sysex_cmd ) {
	case CMD_SLOT_READ_INFO:
	case CMD_BANK_READ_INFO:
		MIDIO_SYSEX_Cmd_ReadInfo(cmd_state, midi_in);		// cmd differs
		sysex_last_cmd = sysex_cmd;
		break;
	case CMD_SLOT_WRITE_INFO:
	case CMD_BANK_WRITE_INFO:
		MIDIO_SYSEX_Cmd_WriteInfo(cmd_state, midi_in);		// cmd differs
		sysex_last_cmd = sysex_cmd;
		break;
	case CMD_READ_BLOCK:
		MIDIO_SYSEX_Cmd_ReadBlock(cmd_state, midi_in);
		sysex_last_cmd = sysex_cmd;
		break;
	case CMD_WRITE_BLOCK:
		MIDIO_SYSEX_Cmd_WriteBlock(cmd_state, midi_in);
		sysex_last_cmd = sysex_cmd;
		break;

	case CMD_BANK_DATA_START:
		MIDIO_SYSEX_Cmd_BankDataStart(cmd_state, midi_in);
		sysex_last_cmd = sysex_cmd;
		break;
	case CMD_BANK_DATA_END:
		MIDIO_SYSEX_Cmd_BankDataEnd(cmd_state, midi_in);
		sysex_last_cmd = sysex_cmd;
		break;
	case CMD_ACK:
		MIDIO_SYSEX_Cmd_Ping(cmd_state, midi_in);
		break;
	default:
		// unknown command
		MIDIO_SYSEX_Send_Footer(0);
		MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIDIO_SYSEX_DISACK_INVALID_COMMAND);
		MIDIO_SYSEX_Cmd_Finished();
	}

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Command 01: Read Patch handler
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Cmd_ReadInfo(u8 cmd_state, u8 midi_in)
{
	switch( cmd_state ) {

	case MIDIO_SYSEX_CMD_STATE_BEGIN:
		// nothing to do
		break;

	case MIDIO_SYSEX_CMD_STATE_CONT:
		if( !sysex_state.BANK_RECEIVED ) {
			sysex_bank = midi_in; // store block number
			sysex_state.BANK_RECEIVED = 1;

		} else if( !sysex_state.SLOT_RECEIVED ) {
			sysex_slot = midi_in; // store block number
			sysex_state.SLOT_RECEIVED = 1;

		} else if( !sysex_state.BLOCK_RECEIVED ) {
			sysex_block = 0; // doesn't matter here
			sysex_state.BLOCK_RECEIVED = 1;

		} else{
			// wait for F7
		}

		break;

	default: // MIDIO_SYSEX_CMD_STATE_END
		MIDIO_SYSEX_Send_Info(sysex_port);
		tr5x6_xfer_state.STAT=XFER_INFO;
		xfer_time_out=-1;
		tr5x6_xfer_state.FLAG_INFO=1;
		tr5x6_xfer_state.FLAG_END=1;
		if(sysex_bank_block_amount){
			sysex_bank_block++;
		}
		break;
	}

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Command 02: Write Patch handler
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Cmd_WriteInfo(u8 cmd_state, u8 midi_in)
{
	switch( cmd_state ) {

	case MIDIO_SYSEX_CMD_STATE_BEGIN:
		sysex_checksum = 0; // clear checksum
		sysex_receive_ctr = 0; // clear byte counter
		sysex_received_checksum = 0;
		break;

	case MIDIO_SYSEX_CMD_STATE_CONT:
		if( !sysex_state.BANK_RECEIVED ) {
			sysex_bank = midi_in; // store block number
			sysex_state.BANK_RECEIVED = 1;

			// add to checksum
			sysex_checksum += midi_in;
		} else if( !sysex_state.SLOT_RECEIVED ) {
			sysex_slot = midi_in; // store block number
			sysex_state.SLOT_RECEIVED = 1;

			// add to checksum
			sysex_checksum += midi_in;
		} else if( !sysex_state.BLOCK_RECEIVED ) {
			sysex_block = midi_in; // store block number
			sysex_state.BLOCK_RECEIVED = 1;

			// add to checksum
			sysex_checksum += midi_in;
		} else{
			if( sysex_receive_ctr < MIDIO_SYSEX_INFO_SIZE ) {	// 16 char name + 256 databytes
				// 7bit format - 8th bit discarded
				if(sysex_receive_ctr & 1){
					sysex_buffer[sysex_receive_ctr>>1] = sysex_byte | midi_in;
				}else{
					sysex_byte = midi_in<<4;
				}
				// add to checksum
				sysex_checksum += midi_in;

			} else if( sysex_receive_ctr == MIDIO_SYSEX_INFO_SIZE ) {
				// store received checksum
				sysex_received_checksum = midi_in;

			}else {
				// wait for F7
			}

			// increment counter
			++sysex_receive_ctr;
		}

		break;

	default: // MIDIO_SYSEX_CMD_STATE_END
		//MIDIO_SYSEX_Send_Footer(0);
		if( sysex_receive_ctr < (MIDIO_SYSEX_INFO_SIZE+1) ) {
			// not enough bytes received
			MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIDIO_SYSEX_DISACK_LESS_BYTES_THAN_EXP);
			tr5x6_xfer_state.STAT=XFER_ERROR;
			xfer_time_out=-1;
			tr5x6_xfer_state.FLAG_ERROR=1;
		} else if( sysex_receive_ctr > (MIDIO_SYSEX_INFO_SIZE+1) ) {
			// too many bytes received
			MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIDIO_SYSEX_DISACK_MORE_BYTES_THAN_EXP);
			tr5x6_xfer_state.STAT=XFER_ERROR;
			xfer_time_out=-1;
			tr5x6_xfer_state.FLAG_ERROR=1;
		} else if( sysex_received_checksum != (-sysex_checksum & 0x7f) ) {
			// notify that wrong checksum has been received
			MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIDIO_SYSEX_DISACK_WRONG_CHECKSUM);
			tr5x6_xfer_state.STAT=XFER_ERROR;
			xfer_time_out=-1;
			tr5x6_xfer_state.FLAG_ERROR=1;
		} else {
			// notify that bytes have been received
			//MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_ACK, 0x00);
			// start writing the block data, erase the ROM if necessary
			// check for flash memory range
			//while(MIOS32_UART_TxBufferUsed(0)>0){};
			write_info_req=sysex_cmd;
			tr5x6_xfer_state.STAT=XFER_INFO;
			xfer_time_out=MIDIO_SYSEX_XFER_TIMEOUT;
			tr5x6_xfer_state.FLAG_INFO=1;
			if(sysex_bank_block_amount){
				if(sysex_cmd==CMD_SLOT_WRITE_INFO){
					sysex_bank_block++;
				}
			}
		}
		break;
	}

	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Command 03: Read Patch block handler
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Cmd_ReadBlock(u8 cmd_state, u8 midi_in)
{
	switch( cmd_state ) {

	case MIDIO_SYSEX_CMD_STATE_BEGIN:
		// nothing to do
		break;

	case MIDIO_SYSEX_CMD_STATE_CONT:
		if( !sysex_state.BANK_RECEIVED ) {
			sysex_bank = midi_in; // store bank number
			sysex_state.BANK_RECEIVED = 1;

		} else if( !sysex_state.SLOT_RECEIVED ) {
			sysex_slot = midi_in; // store slot number
			sysex_state.SLOT_RECEIVED = 1;

		} else if( !sysex_state.BLOCK_RECEIVED ) {
			sysex_block = midi_in; // store block number
			sysex_state.BLOCK_RECEIVED = 1;

		} else{
			// wait for F7
		}

		break;

	default: // MIDIO_SYSEX_CMD_STATE_END
			MIDIO_SYSEX_Send_Block(sysex_port);
			//MIOS32_IRQ_Enable();
			switch(tr5x6_slots[sysex_slot].size){
			case SIZE_4K:sysex_total_block=16;break;
			case SIZE_8K:sysex_total_block=32;break;
			case SIZE_16K:sysex_total_block=64;break;
			case SIZE_32K:sysex_total_block=128;break;
			}
			if(sysex_block==0){
				tr5x6_xfer_state.STAT=XFER_BEGIN;
				xfer_time_out=MIDIO_SYSEX_XFER_TIMEOUT;
				tr5x6_xfer_state.FLAG_BEGIN=1;
			}else if( sysex_block < (sysex_total_block-1)){
				tr5x6_xfer_state.STAT=XFER_CONT;
				xfer_time_out=MIDIO_SYSEX_XFER_TIMEOUT;
				tr5x6_xfer_state.FLAG_CONT=1;
			}else{
				// bus handover to the host (ROM SPI port) - MIDI task context
				APP_SPI_MutexTake();
				TR5X6_ROM_HOST();
				APP_SPI_MutexGive();
				xfer_time_out=-1;
				tr5x6_xfer_state.FLAG_END=1;
			}
			if(sysex_bank_block_amount){
				sysex_bank_block++;
			}
		break;
	}


	return 0; // no error
}
/////////////////////////////////////////////////////////////////////////////
// Command 04: Write Patch Block handler
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Cmd_WriteBlock(u8 cmd_state, u8 midi_in)
{
	switch( cmd_state ) {

	case MIDIO_SYSEX_CMD_STATE_BEGIN:
		sysex_checksum = 0; // clear checksum
		sysex_receive_ctr = 0; // clear byte counter
		sysex_received_checksum = 0;
		break;

	case MIDIO_SYSEX_CMD_STATE_CONT:
		if( !sysex_state.BANK_RECEIVED ) {
			sysex_bank = midi_in; // store block number
			sysex_state.BANK_RECEIVED = 1;

			// add to checksum
			sysex_checksum += midi_in;
		} else if( !sysex_state.SLOT_RECEIVED ) {
			sysex_slot = midi_in; // store block number
			switch(tr5x6_slots[sysex_slot].size){
			case SIZE_4K:sysex_total_block=16;break;
			case SIZE_8K:sysex_total_block=32;break;
			case SIZE_16K:sysex_total_block=64;break;
			case SIZE_32K:sysex_total_block=128;break;
			}
			sysex_state.SLOT_RECEIVED = 1;

			// add to checksum
			sysex_checksum += midi_in;
		} else if( !sysex_state.BLOCK_RECEIVED ) {
			sysex_block = midi_in; // store block number
			sysex_state.BLOCK_RECEIVED = 1;

			// add to checksum
			sysex_checksum += midi_in;
		} else{
			if( sysex_receive_ctr < MIDIO_SYSEX_BLOCK_SIZE ) {	// 16 char name + 256 databytes
				// 7bit format - 8th bit discarded
				if(sysex_receive_ctr & 1){
					sysex_buffer[sysex_receive_ctr>>1] = sysex_byte | midi_in;
				}else{
					sysex_byte = midi_in<<4;
				}
				// add to checksum
				sysex_checksum += midi_in;

			} else if( sysex_receive_ctr == MIDIO_SYSEX_BLOCK_SIZE ) {
				// store received checksum
				sysex_received_checksum = midi_in;

			}else {
				// wait for F7
			}

			// increment counter
			++sysex_receive_ctr;
		}

		break;

	default: // MIDIO_SYSEX_CMD_STATE_END
		//MIDIO_SYSEX_Send_Footer(0);
		if( sysex_receive_ctr < (MIDIO_SYSEX_BLOCK_SIZE+1) ) {
			// not enough bytes received
			MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIDIO_SYSEX_DISACK_LESS_BYTES_THAN_EXP);
			tr5x6_xfer_state.STAT=XFER_ERROR;
			xfer_time_out=-1;
			tr5x6_xfer_state.FLAG_ERROR=1;
		} else if( sysex_receive_ctr > (MIDIO_SYSEX_BLOCK_SIZE+1) ) {
			// too many bytes received
			MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIDIO_SYSEX_DISACK_MORE_BYTES_THAN_EXP);
			tr5x6_xfer_state.STAT=XFER_ERROR;
			xfer_time_out=-1;
			tr5x6_xfer_state.FLAG_ERROR=1;
		} else if( sysex_received_checksum != (-sysex_checksum & 0x7f) ) {
			// notify that wrong checksum has been received
			MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIDIO_SYSEX_DISACK_WRONG_CHECKSUM);
			tr5x6_xfer_state.STAT=XFER_ERROR;
			xfer_time_out=-1;
			tr5x6_xfer_state.FLAG_ERROR=1;
		} else {
			// notify that bytes have been received
			//MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_ACK, 0x00);

      
	 
			write_block_req=1;
			if(sysex_block==0){
				tr5x6_xfer_state.STAT=XFER_BEGIN;
				xfer_time_out=MIDIO_SYSEX_XFER_TIMEOUT;
				tr5x6_xfer_state.FLAG_BEGIN=1;
			}


		}
		break;
	}

	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Command 04: Write Patch Block handler
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Cmd_WriteInfoRequest(void)
{
	if(!write_info_req)return 0;
		u8 cmd= write_info_req;
	write_info_req=0;

		int len=1;
		u8 *buffer_ptr = &sysex_buffer[0];
		while( (*buffer_ptr != '\0') && (len<=22)  ){
			len++;
			buffer_ptr++;
		}
      
   
		sysex_buffer[21]='\0';
   
		tr5x6_flash_info_t slot;
		memcpy(slot.name, sysex_buffer, len);
		slot.color= (sysex_buffer[23]<<16) |  (sysex_buffer[24]<<8) | sysex_buffer[25];
		slot.magic= sysex_buffer[26]<<8 | sysex_buffer[27];
		slot.bank = sysex_bank;
		slot.slot = sysex_slot;
		if(cmd==CMD_BANK_WRITE_INFO){			// bank info
			if( TR5X6_FLASH_Bank_Write(slot)<0){
				TR5X6_ROM_HOST();
				tr5x6_xfer_state.STAT=XFER_ERROR;
				xfer_time_out=-1;
				tr5x6_xfer_state.FLAG_ERROR=1;
	#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
				MIOS32_MIDI_SendDebugMessage("write failed for bank#%d data\n", slot.bank);
	#endif
				MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_WRITE_FAILED);
				while(MIOS32_UART_TxBufferUsed(0)>0){};
				return -1; // no error

			}
			tr5x6_sysex_bank_info_refresh = 1;
		}else if(cmd==CMD_SLOT_WRITE_INFO){		// slot info
			if( TR5X6_FLASH_Slot_Write(slot)<0){
				TR5X6_ROM_HOST();
				tr5x6_xfer_state.STAT=XFER_ERROR;
				xfer_time_out=-1;
				tr5x6_xfer_state.FLAG_ERROR=1;
	#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
				MIOS32_MIDI_SendDebugMessage("write failed for bank#%d slot#%d data\n", slot.bank, slot.slot);
	#endif
				MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_WRITE_FAILED);
				while(MIOS32_UART_TxBufferUsed(0)>0){};
				return -1; // no error

			}
		}
		MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_ACK, MIDIO_SYSEX_ACK_INFO_DONE);
//		MIDIO_SYSEX_Send_UploadReq(sysex_port);
		//tr5x6_xfer_state.STAT=XFER_END;
		xfer_time_out=-1;
		tr5x6_xfer_state.FLAG_END=1;





	return 1; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Command 04: Write Patch Block handler
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Cmd_WriteBlockRequest(void)
{
	if(!write_block_req)return 0;

	write_block_req=0;
		u32 addr;
		switch(tr5x6_slots[sysex_slot].size){
		case SIZE_4K:
			tr5x6_slot_parity_t even_odd = (tr5x6_slot_parity_t)tr5x6_slots[sysex_slot].parity;
			u8 sector= (sysex_block/8);
			u8 block=sysex_block%8;
			u8* data_ptr;

			switch(block){
			case 0: //
				// store the existing sector in RAM
				data_ptr = &datas[0];
				addr = TR5X6_ROM_ProgAddr(sysex_bank) | tr5x6_slots[sysex_slot].addr_offset | (sector*TR5X6_ROM_SECTOR_SIZE);
				for(int i=0;i<TR5X6_ROM_SECTOR_SIZE;i++){
					u8 data = TR5X6_ROM_Read(addr++);
					*(data_ptr++) = data;
				}


			case 1 ... 6: //
			// new block to RAM
			data_ptr = &datas[0] + (block<<9) + (even_odd^1);
			for(int i=0; i<(MIDIO_SYSEX_BLOCK_SIZE>>1); i++) {
				*data_ptr = sysex_buffer[i];
				data_ptr +=2;
			}
			break;
			case 7: //
				// new block to RAM
				data_ptr = &datas[0] + (block<<9) + (even_odd^1);
				for(int i=0; i<(MIDIO_SYSEX_BLOCK_SIZE>>1); i++) {
					*data_ptr = sysex_buffer[i];
					data_ptr +=2;
				}

				addr = TR5X6_ROM_ProgAddr(sysex_bank) | tr5x6_slots[sysex_slot].addr_offset | (sector*TR5X6_ROM_SECTOR_SIZE);
				if( ((addr&0x007FFFFF) >= TR5X6_ROM_START_ADDR) && ((addr&0x007FFFFF) <= TR5X6_ROM_END_ADDR) ) {
					tr5x6_rom_status status;
					// sector erase
					if((status=TR5X6_ROM_Sector_Erase(addr, 1000))!=TR5X6_ROM_OK) {
						//MIOS32_IRQ_Enable();
						TR5X6_ROM_HOST();
						tr5x6_xfer_state.STAT=XFER_ERROR;
						xfer_time_out=-1;
						tr5x6_xfer_state.FLAG_ERROR=1;
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
						MIOS32_MIDI_SendDebugMessage("erase failed for 0x%08x: code %d\n", addr, status);
#endif
						MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_WRITE_FAILED);
						while(MIOS32_UART_TxBufferUsed(0)>0){};
						return -1; // no error
					}
					// write ROM from RAM
					for(int i=0; i<(TR5X6_ROM_SECTOR_SIZE); addr++, i++) {

						if( (status=TR5X6_ROM_Write(addr, datas[i], 1000)) != TR5X6_ROM_OK ) {

							//MIOS32_IRQ_Enable();
							TR5X6_ROM_HOST();
							tr5x6_xfer_state.STAT=XFER_ERROR;
							xfer_time_out=-1;
							tr5x6_xfer_state.FLAG_ERROR=1;
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
							MIOS32_MIDI_SendDebugMessage("write failed for data 0x%02x @0x%08x: code %d\n", sysex_buffer[i], addr, status);
#endif
							MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_WRITE_FAILED);
							while(MIOS32_UART_TxBufferUsed(0)>0){};
							return -1; // no error
						}

					}

				}else{
					// invalid address
					MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_WRONG_ADDR_RANGE);
					tr5x6_xfer_state.STAT=XFER_ERROR;
					xfer_time_out=-1;
					tr5x6_xfer_state.FLAG_ERROR=1;
					return -1; // no error
				}


				break;
			}
			break;


			case SIZE_8K:
			case SIZE_16K:
			case SIZE_32K:

				addr = TR5X6_ROM_ProgAddr(sysex_bank) | tr5x6_slots[sysex_slot].addr_offset | (sysex_block<<8);
				if( ((addr&0x007FFFFF) >= TR5X6_ROM_START_ADDR) && ((addr&0x007FFFFF) <= TR5X6_ROM_END_ADDR) ) {
					tr5x6_rom_status status;
					//TR5X6_SPI_TransferModeInit();
					//MIOS32_IRQ_Disable();
					for(int i=0; i<(MIDIO_SYSEX_BLOCK_SIZE>>1); addr++, i++) {


						if( (addr % TR5X6_ROM_SECTOR_SIZE) == 0 ) {
							if((status=TR5X6_ROM_Sector_Erase(addr, 1000))!=TR5X6_ROM_OK) {
								//MIOS32_IRQ_Enable();
								TR5X6_ROM_HOST();
								tr5x6_xfer_state.STAT=XFER_ERROR;
								xfer_time_out=-1;
								tr5x6_xfer_state.FLAG_ERROR=1;
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
								MIOS32_MIDI_SendDebugMessage("erase failed for 0x%08x: code %d\n", addr, status);
#endif
								MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_WRITE_FAILED);
								while(MIOS32_UART_TxBufferUsed(0)>0){};
								return -1; // no error
							}

						}

						if( (status=TR5X6_ROM_Write(addr, sysex_buffer[i], 1000)) != TR5X6_ROM_OK ) {

							//MIOS32_IRQ_Enable();
							TR5X6_ROM_HOST();
							tr5x6_xfer_state.STAT=XFER_ERROR;
							xfer_time_out=-1;
							tr5x6_xfer_state.FLAG_ERROR=1;
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
							MIOS32_MIDI_SendDebugMessage("write failed for data 0x%02x @0x%08x: code %d\n", sysex_buffer[i], addr, status);
#endif
							MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_WRITE_FAILED);
							while(MIOS32_UART_TxBufferUsed(0)>0){};
							return -1; // no error
						}

					}

				}else{
					// invalid address
					MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_WRONG_ADDR_RANGE);
					tr5x6_xfer_state.STAT=XFER_ERROR;
					xfer_time_out=-1;
					tr5x6_xfer_state.FLAG_ERROR=1;
					return -1; // no error
				}
				break;

		}

		//MIOS32_IRQ_Enable();
		if( sysex_block < (sysex_total_block-1)){
			//TR5X6_ROM_HOST();
			MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_ACK, MIDIO_SYSEX_ACK_CONTINUE);
			tr5x6_xfer_state.STAT=XFER_CONT;
			xfer_time_out=MIDIO_SYSEX_XFER_TIMEOUT;
			tr5x6_xfer_state.FLAG_CONT=1;
			if(sysex_bank_block_amount){
				sysex_bank_block++;
			}

		}else{
			TR5X6_ROM_HOST();
			MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_ACK, MIDIO_SYSEX_ACK_CMD_END);
			//tr5x6_xfer_state.STAT=XFER_END;
			xfer_time_out=-1;
			tr5x6_xfer_state.FLAG_END=1;
			if(sysex_bank_block_amount){
				sysex_bank_block++;
			}
		}


	return 1; // no error
}

/////////////////////////////////////////////////////////////////////////////
// This function sends an upload request
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Send_UploadReq(mios32_midi_port_t port)
{
	u8 buffer[12]; // should be enough?
	u8 *buffer_ptr = &buffer[0];
	int i;

	for(i=0; i<sizeof(sysex_header); ++i)
		*buffer_ptr++ = sysex_header[i];

	// device ID
	*buffer_ptr++ = MIOS32_MIDI_DeviceIDGet();

	// send 0x01 to request code upload
	*buffer_ptr++ = 0x01;
	// send 0x01 to request code upload
	*buffer_ptr++ = sysex_block+1;

	// send footer
	*buffer_ptr++ = 0xf7;

	// finally send SysEx stream
	sysex_act = 2;
	return MIOS32_MIDI_SendSysEx(port, (u8 *)buffer, (u32)buffer_ptr - ((u32)&buffer[0]));
}
/////////////////////////////////////////////////////////////////////////////
// Command 0F: Ping (just send back acknowledge)
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Cmd_Ping(u8 cmd_state, u8 midi_in)
{
	switch( cmd_state ) {

	case MIDIO_SYSEX_CMD_STATE_BEGIN:
		// nothing to do
		break;

	case MIDIO_SYSEX_CMD_STATE_CONT:
		// nothing to do
		break;

	default: // MIDIO_SYSEX_CMD_STATE_END
	  
		MIDIO_SYSEX_Send_Footer(0);
		// send acknowledge with unit type			  
		MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_ACK, TR5X6_SYSEX_ACK_UNIT_TYPE);
      
		break;
	}

	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Command 0F: Ping (just send back acknowledge)
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Cmd_BankDataStart(u8 cmd_state, u8 midi_in)
{

	switch( cmd_state ) {

	case MIDIO_SYSEX_CMD_STATE_BEGIN:
		// nothing to do
		break;

	case MIDIO_SYSEX_CMD_STATE_CONT:
		if( !sysex_state.BANK_RECEIVED ) {
			sysex_bank = midi_in; // store bank number
			sysex_state.BANK_RECEIVED = 1;

		} else if( !sysex_state.SLOT_RECEIVED ) {
			sysex_slot = midi_in; // used for block amount MSB
			sysex_state.SLOT_RECEIVED = 1;

		} else if( !sysex_state.BLOCK_RECEIVED ) {
			sysex_block = midi_in; 	// used for block amount MSB
			sysex_state.BLOCK_RECEIVED = 1;

		} else{
			// wait for F7
		}

		break;

	default: // MIDIO_SYSEX_CMD_STATE_END
		sysex_bank_block_amount = (sysex_slot<<7) | sysex_block;
		if(sysex_bank_block_amount)sysex_bank_progress=1;
		else sysex_bank_progress=0;
		sysex_bank_block=0;

		MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_ACK, MIDIO_SYSEX_ACK_BANK_RDY);

		break;
	}
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Command 0F: Ping (just send back acknowledge)
/////////////////////////////////////////////////////////////////////////////
s32 MIDIO_SYSEX_Cmd_BankDataEnd(u8 cmd_state, u8 midi_in)
{

	switch( cmd_state ) {

	case MIDIO_SYSEX_CMD_STATE_BEGIN:
		// nothing to do
		break;

	case MIDIO_SYSEX_CMD_STATE_CONT:
		if( !sysex_state.BANK_RECEIVED ) {
			sysex_bank = midi_in; // store bank number
			sysex_state.BANK_RECEIVED = 1;

		} else if( !sysex_state.SLOT_RECEIVED ) {
			sysex_slot = 0; // not used
			sysex_state.SLOT_RECEIVED = 1;

		} else if( !sysex_state.BLOCK_RECEIVED ) {
			sysex_block = 0; 	// not used
			sysex_state.BLOCK_RECEIVED = 1;

		} else{
			// wait for F7
		}

		break;

	default: // MIDIO_SYSEX_CMD_STATE_END
		sysex_bank_progress = 0;
		sysex_bank_block_amount = 0;
		sysex_bank_block = 0;

		MIDIO_SYSEX_Send_Ack(sysex_port, MIDIO_SYSEX_ACK, MIDIO_SYSEX_ACK_BANK_END);

		break;
	}
	return 0; // no error
}

s32 MIDIO_SYSEX_TimeOut_Period(void){
	if(xfer_time_out!=-1){
		//if(xfer_time_out==2000)MIOS32_MIDI_SendDebugMessage("time out start\n");
		--xfer_time_out;
		//MIOS32_MIDI_SendDebugMessage("%d\n", xfer_time_out);
		if(xfer_time_out<=0){
			//MIOS32_MIDI_SendDebugMessage("xfer time out!\n");
			tr5x6_xfer_state.STAT=XFER_ERROR;
			tr5x6_xfer_state.FLAG_ERROR=1;
			sysex_bank_block=0;
			sysex_bank_progress=0;
			xfer_time_out=-1;
		}

	}
	return 0; // no error
}

u8 MIDIO_SYSEX_Act(void){
	u8 act = sysex_act;
	sysex_act = 0;
	return act;
}

	  

s8 MIDIO_SYSEX_Bank_Progression(void){
	u8 progress;
	if(sysex_bank_progress==1){
		progress= (u8)((sysex_bank_block+1)*100 / sysex_bank_block_amount);
		if(sysex_bank_block>=sysex_bank_block_amount)sysex_bank_progress=0;
	}else progress=-1;
	return progress;
}

u8 MIDIO_SYSEX_Slot_Progression(void){

	return (u8)((sysex_block+1)*100 / sysex_total_block);
}

u8 MIDIO_SYSEX_Cmd_Current(void){

	return sysex_last_cmd;
}

u8 MIDIO_SYSEX_Bank_Current(void){

	return sysex_bank;
}

u8 MIDIO_SYSEX_Slot_Current(void){

	return sysex_slot;
}
