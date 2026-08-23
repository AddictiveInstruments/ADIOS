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

#ifndef _MIDIO_SYSEX_H
#define _MIDIO_SYSEX_H

/////////////////////////////////////////////////////////////////////////////
// global definitions
/////////////////////////////////////////////////////////////////////////////

#define TR5X6_ROM_SECTOR_SIZE 	0x1000	// 4K

#define CMD_IDLE 				0x0
#define CMD_BANK_READ_INFO 		0x1
#define CMD_BANK_WRITE_INFO 	0x2
#define CMD_SLOT_READ_INFO 		0x3
#define CMD_SLOT_WRITE_INFO 	0x4
#define CMD_READ_BLOCK 			0x5
#define CMD_WRITE_BLOCK 		0x6
#define CMD_BANK_DATA_START 	0x7
#define CMD_BANK_DATA_END 		0x8
#define CMD_ACK 				0xf
#define CMD_DISACK 				0xe

/////////////////////////////////////////////////////////////////////////////
// Type definitions
/////////////////////////////////////////////////////////////////////////////

typedef enum{
	XFER_IDLE=0,
	XFER_END,
	XFER_INFO,
	XFER_BEGIN,
	XFER_CONT,
	XFER_ERROR
}tr5x6_xfer_status;

typedef union {
	struct {
		u8 ALL;
	};
	struct {
		u8 STAT:3;
		u8 FLAG_INFO:1;
		u8 FLAG_BEGIN:1;
		u8 FLAG_CONT:1;
		u8 FLAG_END:1;
		u8 FLAG_ERROR:1;
	};
	struct {
		u8 dummy:3;
		u8 FLAG:5;
	};
} tr5x6_xfer_state_t;

/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIDIO_SYSEX_Init(u32 mode);

extern s32 MIDIO_SYSEX_Parser(adios_midi_port_t port, u8 midi_in);

extern s32 MIDIO_SYSEX_TimeOut(adios_midi_port_t port);
extern s32 MIDIO_SYSEX_TimeOut_Period(void);

extern s32 MIDIO_SYSEX_Cmd_WriteInfoRequest(void);
extern s32 MIDIO_SYSEX_Cmd_WriteBlockRequest(void);
extern s32 MIDIO_SYSEX_Send_Info(adios_midi_port_t port);
extern s32 MIDIO_SYSEX_Send_UploadReq(adios_midi_port_t port);
extern s32 MIDIO_SYSEX_Send_Block(adios_midi_port_t port);

extern u8 MIDIO_SYSEX_Act(void);
extern s8 MIDIO_SYSEX_Bank_Progression(void);
extern u8 MIDIO_SYSEX_Slot_Progression(void);
extern u8 MIDIO_SYSEX_Slot_Current(void);
extern u8 MIDIO_SYSEX_Bank_Current(void);
extern u8 MIDIO_SYSEX_Cmd_Current(void);

/////////////////////////////////////////////////////////////////////////////
// Exported variables
/////////////////////////////////////////////////////////////////////////////
extern u8 tr5x6_sysex_block_flag;
extern u8 tr5x6_sysex_uploading;
extern u8 tr5x6_sysex_bank_info_refresh;
extern tr5x6_xfer_state_t tr5x6_xfer_state;
extern char test_name[20];
extern u16 test_color;
#endif /* _MIDIO_SYSEX_H */
