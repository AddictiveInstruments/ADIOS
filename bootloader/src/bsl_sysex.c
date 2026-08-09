// $Id: sysex.c 78 2008-10-12 22:09:23Z tk $
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

#include "bsl_sysex.h"


/////////////////////////////////////////////////////////////////////////////
// Local Macros
/////////////////////////////////////////////////////////////////////////////

#define MEM32(addr) (*((volatile u32 *)(addr)))
#define MEM16(addr) (*((volatile u16 *)(addr)))
#define MEM8(addr)  (*((volatile u8  *)(addr)))

#if defined(MIOS32_FAMILY_STM32G0xx)
// STM32: determine page size (mid density devices: 1k, high density devices: 2k)
// TODO: find a proper way, as there could be high density devices with less than 256k?)
# define FLASH_PAGE_SIZE   (0x800)

// STM32: flash memory range (BSL range excluded, size defined per-project in mios32_config.h)
#ifndef MIOS32_APP_FLASH_START_ADDR
# error "Please define MIOS32_APP_FLASH_START_ADDR in mios32_config.h (see Template/mios32_config.h)"
#endif
# define FLASH_START_ADDR  (0x08000000 + MIOS32_APP_FLASH_START_ADDR)
# define FLASH_END_ADDR    (0x08000000 + MIOS32_SYS_FlashSizeGet() - 1)


// STM32: base address of SRAM
# define SRAM_START_ADDR   (0x20000000)
# define SRAM_END_ADDR     (0x20000000 + MIOS32_SYS_RAMSizeGet() - 1)

#elif defined(MIOS32_FAMILY_STM32F10x)
// STM32: determine page size (mid density devices: 1k, high density devices: 2k)
// TODO: find a proper way, as there could be high density devices with less than 256k?)
# define FLASH_PAGE_SIZE   (MIOS32_SYS_FlashSizeGet() >= (256*1024) ? 0x800 : 0x400)

// STM32: flash memory range (16k BSL range excluded)
# define FLASH_START_ADDR  (0x08000000 + 0x4000)
# define FLASH_END_ADDR    (0x08000000 + MIOS32_SYS_FlashSizeGet() - 1)


// STM32: base address of SRAM
# define SRAM_START_ADDR   (0x20000000)
# define SRAM_END_ADDR     (0x20000000 + MIOS32_SYS_RAMSizeGet() - 1)

#elif defined(MIOS32_FAMILY_STM32F4xx)

// sector base addresses

#define MAX_FLASH_SECTOR 12
const u32 flash_sector_map[MAX_FLASH_SECTOR][3] = {
		{ 0xffffffff, LL_FLASH_Sector_0 }, /* Base @ of Sector 0, 16 Kbyte */ // TK: actually 0x08000000, ensure that it won't be taken
		{ 0x08004000, LL_FLASH_Sector_1 }, /* Base @ of Sector 1, 16 Kbyte */
		{ 0x08008000, LL_FLASH_Sector_2 }, /* Base @ of Sector 2, 16 Kbyte */
		{ 0x0800C000, LL_FLASH_Sector_3 }, /* Base @ of Sector 3, 16 Kbyte */
		{ 0x08010000, LL_FLASH_Sector_4 }, /* Base @ of Sector 4, 64 Kbyte */
		{ 0x08020000, LL_FLASH_Sector_5 }, /* Base @ of Sector 5, 128 Kbyte */
		{ 0x08040000, LL_FLASH_Sector_6 }, /* Base @ of Sector 6, 128 Kbyte */
		{ 0x08060000, LL_FLASH_Sector_7 }, /* Base @ of Sector 7, 128 Kbyte */
		{ 0x08080000, LL_FLASH_Sector_8 }, /* Base @ of Sector 8, 128 Kbyte */
		{ 0x080A0000, LL_FLASH_Sector_9 }, /* Base @ of Sector 9, 128 Kbyte */
		{ 0x080C0000, LL_FLASH_Sector_10 }, /* Base @ of Sector 10, 128 Kbyte */
		{ 0x080E0000, LL_FLASH_Sector_11 }, /* Base @ of Sector 11, 128 Kbyte */
};

static u32 flash_erase_done = 0;
#if MAX_FLASH_SECTOR > 32
# error "Please adapt value range of flash_erase_done!"
#endif

// STM32: flash memory range (16k BSL range excluded)
# define FLASH_START_ADDR  (0x08000000 + 0x4000)
# define FLASH_END_ADDR    (0x08000000 + MIOS32_SYS_FlashSizeGet() - 1)


// STM32: base address of SRAM
# define SRAM_START_ADDR   (0x20000000)
# define SRAM_END_ADDR     (0x20000000 + MIOS32_SYS_RAMSizeGet() - 1)


#else
# error "BSL not prepared for this family"
#endif


/////////////////////////////////////////////////////////////////////////////
// Internal Prototypes
/////////////////////////////////////////////////////////////////////////////

static s32 BSL_SYSEX_Cmd_ReadMem(mios32_midi_port_t port, mios32_midi_sysex_cmd_state_t cmd_state, u8 midi_in);
static s32 BSL_SYSEX_Cmd_WriteMem(mios32_midi_port_t port, mios32_midi_sysex_cmd_state_t cmd_state, u8 midi_in);

s32 BSL_SYSEX_Cmd_SetEntryOverride(mios32_midi_port_t port, mios32_midi_sysex_cmd_state_t cmd_state, u8 midi_in);
static s32 BSL_SYSEX_RecAddrAndLen(u8 midi_in);

static s32 BSL_SYSEX_SendAck(mios32_midi_port_t port, u8 ack_code, u8 ack_arg);
static s32 BSL_SYSEX_SendMem(mios32_midi_port_t port, u32 addr, u32 len);
static s32 BSL_SYSEX_WriteMem(u32 addr, u32 len, u8 *buffer);


/////////////////////////////////////////////////////////////////////////////
// Local Variables
/////////////////////////////////////////////////////////////////////////////

static bsl_sysex_rec_state_t sysex_rec_state;

static u32 sysex_addr;
static u32 sysex_len;
static u8 sysex_checksum;
static u8 sysex_received_checksum;
static u32 sysex_receive_ctr;


/////////////////////////////////////////////////////////////////////////////
// local variables
/////////////////////////////////////////////////////////////////////////////

// ensure that the buffer is located at a word boundary (required for LPC17 flash programming routines)
static u8 sysex_buffer[BSL_SYSEX_BUFFER_SIZE] __attribute__ ((aligned (8)));

static u8 halt_state;

#ifdef BSL_UPDATER
// BSL-update tool build (see bootloader/updater/): this "second bootloader",
// linked in its own window above the normal app origin, receives the NEW
// bootloader image over the standard SysEx upload protocol and writes it
// DIRECTLY into the BSL region - no staging (decision 2026-08-09: this is a
// beta-tester-only operation, a power cut during the ~10s transfer is
// accepted; every MIDI-level failure is covered by the per-block checksum,
// the read-back verification and MIOS Studio's block retries). Studio
// drives the whole two-stage sequence from ONE hex and finishes with 0x7f
// (see BSL_SYSEX_ReleaseHaltState below).
#ifndef MIOS32_UPDATER_ORIGIN_ADDR
# error "BSL_UPDATER build without MIOS32_UPDATER_ORIGIN_ADDR - run via bootloader/updater/Makefile (etc/gen_bsl_boundary.sh updater mode generates it)"
#endif
// scan parameters for locating the OLD device's persistent info block:
// candidate boundaries start at the family's minimum and advance by the
// erase granularity (same values as etc/gen_bsl_boundary.sh)
#if defined(MIOS32_FAMILY_STM32G0xx)
# define BSL_UPDATER_SCAN_FIRST 10240
# define BSL_UPDATER_SCAN_STEP  2048
#elif defined(MIOS32_FAMILY_STM32F4xx)
# define BSL_UPDATER_SCAN_FIRST 16384
# define BSL_UPDATER_SCAN_STEP  16384
#endif
// RAM copy of the device's persistent info block (device ID, fastboot...),
// captured at init BEFORE any write can destroy it - its flash page is also
// a code page of the old bootloader. Restored at the NEW boundary position
// by the final sequence.
static u8 info_block[0x100] __attribute__ ((aligned (8)));
static u8 info_found;

// write window: the updater accepts writes EXCLUSIVELY inside the
// bootloader region - exactly the range every normal BSL protects. Same
// check in BSL_SYSEX_WriteMem, opposite role.
# define BSL_FLASH_WRITE_FLOOR (0x08000000)
# define BSL_FLASH_WRITE_CEIL  (0x08000000 + MIOS32_APP_FLASH_START_ADDR - 1)
#else
# define BSL_FLASH_WRITE_FLOOR FLASH_START_ADDR
# define BSL_FLASH_WRITE_CEIL  FLASH_END_ADDR
#endif

// software-requested hold (2026-08-09): mirror of the RTC-backup
// "bootloader mode requested" flag, set by main() at boot. Unlike the
// physical BSL_HOLD pin (which the user releases by hand), this one has no
// physical release - it is cleared here, by MIOS Studio's post-upload
// "reboot" query (see BSL_SYSEX_ReleaseHaltState below). Without that, a
// software-entered bootloader could never fall through to the application.
static u8 soft_hold;

// set on the first write block - lets ReleaseHaltState() distinguish the
// PRE-upload 0x7f query (Studio confirming BL mode - keep holding) from the
// POST-upload one (Studio asking to start the app - release the soft hold)
static u8 upload_started;


/////////////////////////////////////////////////////////////////////////////
// This function initializes the SysEx handler
/////////////////////////////////////////////////////////////////////////////
s32 BSL_SYSEX_Init(u32 mode)
{
	if( mode != 0 )
		return -1; // only mode 0 supported

	// set to one when writing flash to prevent the execution of application code
	// so long flash hasn't been programmed completely
	halt_state = 0;

	soft_hold = 0;
	upload_started = 0;

#ifdef BSL_UPDATER
	// capture the device's persistent info block NOW - the first write block
	// will start erasing the pages that hold it. The OLD boundary of this
	// device may differ from the image being installed: scan the candidate
	// positions for one of the block's 0x42 confirm markers.
	info_found = 0;
	{
		u32 cand;
		for(cand=BSL_UPDATER_SCAN_FIRST; cand<=(MIOS32_UPDATER_ORIGIN_ADDR-0x08000000) && !info_found; cand+=BSL_UPDATER_SCAN_STEP) {
			u8 *block = (u8 *)(0x08000000 + cand - 0x100);
			if( block[0xd0] == 0x42 || block[0xd2] == 0x42 || block[0xc0] == 0x42 ) {
				int i;
				for(i=0; i<0x100; ++i)
					info_block[i] = block[i];
				info_found = 1;
			}
		}
	}
#endif

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Returns 1 if BSL is in halt state (e.g. code is uploaded)
/////////////////////////////////////////////////////////////////////////////
s32 BSL_SYSEX_HaltStateGet(void)
{
	return halt_state;
}


/////////////////////////////////////////////////////////////////////////////
// Software-requested hold state - set by main() from the RTC backup flag,
// polled by main()'s wait loop (same role as the physical BSL_HOLD pin)
/////////////////////////////////////////////////////////////////////////////
s32 BSL_SYSEX_SoftHoldSet(u8 hold)
{
	soft_hold = hold;
	return 0;
}

s32 BSL_SYSEX_SoftHoldGet(void)
{
	return soft_hold;
}



#ifdef BSL_UPDATER
/////////////////////////////////////////////////////////////////////////////
// Updater build: MIOS Studio's 0x7f is handled two ways.
//
// A) A bootloader image was written this session (upload_started, and a
//    plausible reset vector at 0x08000004): finish the BSL update -
//    1) restore the persistent info block (captured to RAM at init) at its
//       NEW position, boundary-0x100 (the incoming image ends well before
//       it, so that area is still erased and programmable);
//    2) clear the entry override, set the TAMP flag so the fresh bootloader
//       stays resident, reset.
//    No app-page erase: the flag alone keeps the fresh BSL resident, so the
//    updater never has to erase the page it might be sitting on - which is
//    what lets it link right at the boundary ceiling. (Trade-off: if power
//    is cut in the gap before the app is uploaded, the device reboots into
//    this still-present updater - harmless, case B below just relays.)
//
// B) Nothing written this session (an app-load request, not a BSL update):
//    behave like a normal application's 0x7f - set the flag and reset, so
//    the fresh bootloader underneath takes over and receives the app. This
//    is what makes "load an app through the updater" work: you are never
//    stuck in the updater, no BSL_HOLD needed.
/////////////////////////////////////////////////////////////////////////////
s32 BSL_SYSEX_ReleaseHaltState(void)
{
	const u32 boundary = MIOS32_APP_FLASH_START_ADDR;

	// A) a bootloader image was written -> finalize the BSL update
	u32 new_reset_vector = *(u32 *)(0x08000000 + 4);
	if( upload_started &&
	    (new_reset_vector >> 24) == 0x08 &&
	    new_reset_vector < (0x08000000 + boundary) ) {

		MIOS32_IRQ_Disable();

		// restore the info block at its NEW position (skip if the image
		// unexpectedly extends into it - the BSL itself matters more)
		if( info_found ) {
			BSL_SYSEX_WriteMem(0x08000000 + boundary - 0x100, 0x100, info_block);
		}

		MIOS32_SYS_AppEntryOverrideSet(0);
		MIOS32_SYS_BootloaderModeRequest();
		MIOS32_SYS_Reset();
		return 0; // never reached
	}

	// B) nothing written -> app-load relay: step aside for the fresh
	//    bootloader (like a normal app's reboot-to-BL), so it can receive an
	//    application upload
	MIOS32_SYS_AppEntryOverrideSet(0);
	MIOS32_SYS_BootloaderModeRequest();
	MIOS32_SYS_Reset();
	return 0; // never reached
}

#else /* !BSL_UPDATER - the normal bootloader */

/////////////////////////////////////////////////////////////////////////////
// Used by MIOS32_MIDI to release halt state instead of triggering a reset
/////////////////////////////////////////////////////////////////////////////
s32 BSL_SYSEX_ReleaseHaltState(void)
{
	// always send upload request (like if we would come out of reset)
	BSL_SYSEX_SendUploadReq(DIN0);
	BSL_SYSEX_SendUploadReq(USB0);

	// clear halt state
	halt_state = 0;

	// the POST-upload "reboot" query also releases the software-requested
	// hold, so the wait loop in main() can finally fall through to the app.
	// The PRE-upload 0x7f (no write block seen yet) keeps holding.
	if( upload_started )
		soft_hold = 0;

	return 0;
}
#endif /* BSL_UPDATER */


/////////////////////////////////////////////////////////////////////////////
// This function enhances MIOS32 SysEx commands
// it's called from MIOS32_MIDI_SYSEX_Cmd if the "MIOS32_MIDI_BSL_ENHANCEMENTS"
// switch is set (see code there for details)
/////////////////////////////////////////////////////////////////////////////
s32 BSL_SYSEX_Cmd(mios32_midi_port_t port, mios32_midi_sysex_cmd_state_t cmd_state, u8 midi_in, u8 sysex_cmd)
{
	// change debug port
	MIOS32_MIDI_DebugPortSet(port);

	// wait 2 additional seconds whenever a SysEx message has been received
	MIOS32_STOPWATCH_Reset();

	// enter the commands here
	switch( sysex_cmd ) {
	// case 0x00: // query command is implemented in MIOS32
	// case 0x0f: // ping command is implemented in MIOS32

	case 0x01:
		BSL_SYSEX_Cmd_ReadMem(port, cmd_state, midi_in);
		break;
	case 0x02:
		BSL_SYSEX_Cmd_WriteMem(port, cmd_state, midi_in);
		break;
	case 0x03:
		BSL_SYSEX_Cmd_SetEntryOverride(port, cmd_state, midi_in);
		break;

	default:
		// unknown command
		return -1; // command not supported
	}

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Command 01: Read Memory handler
// TODO: we could provide this command also during runtime, as it isn't destructive
// or it could be available as debug command 0D like known from MIOS8
/////////////////////////////////////////////////////////////////////////////
s32 BSL_SYSEX_Cmd_ReadMem(mios32_midi_port_t port, mios32_midi_sysex_cmd_state_t cmd_state, u8 midi_in)
{
	switch( cmd_state ) {

	case MIOS32_MIDI_SYSEX_CMD_STATE_BEGIN:
		// set initial receive state and address/len
		sysex_rec_state = BSL_SYSEX_REC_A3;
		sysex_addr = 0;
		sysex_len = 0;
		break;

	case MIOS32_MIDI_SYSEX_CMD_STATE_CONT:
		if( sysex_rec_state < BSL_SYSEX_REC_PAYLOAD )
			BSL_SYSEX_RecAddrAndLen(midi_in);
		break;

	default: // BSL_SYSEX_CMD_STATE_END
		// TODO: send 0xf7 if merger enabled

		// did we reach payload state?
		if( sysex_rec_state != BSL_SYSEX_REC_PAYLOAD ) {
			// not enough bytes received
			BSL_SYSEX_SendAck(port, MIOS32_MIDI_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_LESS_BYTES_THAN_EXP);
		} else {
			// send dump
			BSL_SYSEX_SendMem(port, sysex_addr, sysex_len);
		}

		break;
	}

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Command 03: set application entry override (new-generation one-click BSL
// update, 2026-08-09): payload = 5 septets MSB-first forming the 32-bit
// vector-table address of an alternate entry (the updater, linked above the
// normal app origin). Stored in a backup register, one-shot - consumed by
// main()'s jump-to-application decision on the very next boundary-exit.
// Sent by MIOS Studio right before the final 0x7f when it has uploaded an
// application whose hex does NOT start at the app/bootloader boundary.
/////////////////////////////////////////////////////////////////////////////
s32 BSL_SYSEX_Cmd_SetEntryOverride(mios32_midi_port_t port, mios32_midi_sysex_cmd_state_t cmd_state, u8 midi_in)
{
	static u32 entry_addr;
	static u8 entry_byte_ctr;

	switch( cmd_state ) {

	case MIOS32_MIDI_SYSEX_CMD_STATE_BEGIN:
		entry_addr = 0;
		entry_byte_ctr = 0;
		break;

	case MIOS32_MIDI_SYSEX_CMD_STATE_CONT:
		if( entry_byte_ctr < 5 ) {
			entry_addr = (entry_addr << 7) | (midi_in & 0x7f);
			++entry_byte_ctr;
		}
		break;

	default: // MIOS32_MIDI_SYSEX_CMD_STATE_END
		if( entry_byte_ctr != 5 ) {
			BSL_SYSEX_SendAck(port, MIOS32_MIDI_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_LESS_BYTES_THAN_EXP);
		} else if( (entry_addr >> 24) != 0x08 || (entry_addr & 3) ) {
			// not a word-aligned flash address
			BSL_SYSEX_SendAck(port, MIOS32_MIDI_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_WRONG_ADDR_RANGE);
		} else {
			MIOS32_SYS_AppEntryOverrideSet(entry_addr);
			BSL_SYSEX_SendAck(port, MIOS32_MIDI_SYSEX_ACK, 0x03); // echo the command number
		}
		break;
	}

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Command 02: Write Memory handler
/////////////////////////////////////////////////////////////////////////////
s32 BSL_SYSEX_Cmd_WriteMem(mios32_midi_port_t port, mios32_midi_sysex_cmd_state_t cmd_state, u8 midi_in)
{
	static u32 bit_ctr8 = 0;
	static u32 value8 = 0;

	switch( cmd_state ) {

	case MIOS32_MIDI_SYSEX_CMD_STATE_BEGIN:
		// set initial receive state and address/len
		sysex_rec_state = BSL_SYSEX_REC_A3;
		sysex_addr = 0;
		sysex_len = 0;
		// clear checksum and receive counters
		sysex_checksum = 0;
		sysex_received_checksum = 0;

		sysex_receive_ctr = 0;
		bit_ctr8 = 0;
		value8 = 0;
		break;

	case MIOS32_MIDI_SYSEX_CMD_STATE_CONT:
		if( sysex_rec_state < BSL_SYSEX_REC_PAYLOAD ) {
			sysex_checksum += midi_in;
			BSL_SYSEX_RecAddrAndLen(midi_in);
		} else if( sysex_rec_state == BSL_SYSEX_REC_PAYLOAD ) {
			sysex_checksum += midi_in;
			// new byte has been received - descramble and buffer it
			if( sysex_receive_ctr < BSL_SYSEX_MAX_BYTES ) {
				u8 value7 = midi_in;
				int bit_ctr7;
				for(bit_ctr7=0; bit_ctr7<7; ++bit_ctr7) {
					value8 = (value8 << 1) | ((value7 & 0x40) ? 1 : 0);
					value7 <<= 1;

					if( ++bit_ctr8 >= 8 ) {
						sysex_buffer[sysex_receive_ctr] = value8;
						bit_ctr8 = 0;
						if( ++sysex_receive_ctr >= sysex_len )
							sysex_rec_state = BSL_SYSEX_REC_CHECKSUM;
					}
				}
			}
		} else if( sysex_rec_state == BSL_SYSEX_REC_CHECKSUM ) {
			// store received checksum
			sysex_received_checksum = midi_in;
		} else {
			// too many bytes... wait for F7
			sysex_rec_state = BSL_SYSEX_REC_INVALID;
		}
		break;

	default: // MIOS32_MIDI_SYSEX_CMD_STATE_END
		// TODO: send 0xf7 if merger enabled

		// TEMPORARY DEBUG PROBE (2026-08-01): unconditional trace of what was
		// actually received for this WriteMem block, before any validation.
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
		MIOS32_MIDI_SendDebugMessage("[BSL_SYSEX] WriteMem END addr=0x%08x len=%d got=%d state=%d\n", sysex_addr, sysex_len, sysex_receive_ctr, sysex_rec_state);
#endif

		if( sysex_receive_ctr < sysex_len ) {
			// for remote analysis...
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
			MIOS32_MIDI_SendDebugMessage("[BSL_SYSEX] expected %d, got %d bytes (retry)\n", sysex_len, sysex_receive_ctr);
#endif
			// not enough bytes received
			BSL_SYSEX_SendAck(port, MIOS32_MIDI_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_LESS_BYTES_THAN_EXP);
		} else if( sysex_rec_state == BSL_SYSEX_REC_INVALID ) {
			// too many bytes received
			BSL_SYSEX_SendAck(port, MIOS32_MIDI_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_MORE_BYTES_THAN_EXP);
		} else if( sysex_received_checksum != (-sysex_checksum & 0x7f) ) {
			// notify that wrong checksum has been received
			BSL_SYSEX_SendAck(port, MIOS32_MIDI_SYSEX_DISACK, MIOS32_MIDI_SYSEX_DISACK_WRONG_CHECKSUM);
		} else {
			// enter halt state (can only be released via BSL reset)
			halt_state = 1;
			upload_started = 1;

			// write received data into memory (the updater build accepts
			// exclusively the bootloader region, every other build exclusively
			// the range above it - see BSL_FLASH_WRITE_FLOOR/CEIL)
			s32 error;
			if( (error = BSL_SYSEX_WriteMem(sysex_addr, sysex_len, sysex_buffer)) ) {
				// write failed - return negated error status
				BSL_SYSEX_SendAck(port, MIOS32_MIDI_SYSEX_DISACK, -error);
			} else {
				// notify that bytes have been received by returning checksum
				BSL_SYSEX_SendAck(port, MIOS32_MIDI_SYSEX_ACK, -sysex_checksum & 0x7f);
			}

			// enfore immediate MIDI queue flush
			// (important for retry handling: send ack before next message is processed)
			MIOS32_MIDI_Periodic_mS();
		}
		break;
	}

	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Help function to receive address and length
/////////////////////////////////////////////////////////////////////////////
static s32 BSL_SYSEX_RecAddrAndLen(u8 midi_in)
{
	if( sysex_rec_state <= BSL_SYSEX_REC_A0 ) {
		sysex_addr = (sysex_addr << 7) | ((midi_in & 0x7f) << 4);
		if( sysex_rec_state == BSL_SYSEX_REC_A0 )
			sysex_rec_state = BSL_SYSEX_REC_L3;
		else
			++sysex_rec_state;
	} else if( sysex_rec_state <= BSL_SYSEX_REC_L0 ) {
		sysex_len = (sysex_len << 7) | ((midi_in & 0x7f) << 4);
		if( sysex_rec_state == BSL_SYSEX_REC_L0 ) {
			sysex_rec_state = BSL_SYSEX_REC_PAYLOAD;
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
static s32 BSL_SYSEX_SendAck(mios32_midi_port_t port, u8 ack_code, u8 ack_arg)
{
	u8 sysex_buffer[32]; // should be enough?
	u8 *sysex_buffer_ptr = &sysex_buffer[0];
	int i;

	for(i=0; i<sizeof(mios32_midi_sysex_header); ++i)
		*sysex_buffer_ptr++ = mios32_midi_sysex_header[i];

	// device ID
	*sysex_buffer_ptr++ = MIOS32_MIDI_DeviceIDGet();

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
s32 BSL_SYSEX_SendUploadReq(mios32_midi_port_t port)
{
	u8 sysex_buffer[32]; // should be enough?
	u8 *sysex_buffer_ptr = &sysex_buffer[0];
	int i;

	for(i=0; i<sizeof(mios32_midi_sysex_header); ++i)
		*sysex_buffer_ptr++ = mios32_midi_sysex_header[i];

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
s32 BSL_SYSEX_SendMem(mios32_midi_port_t port, u32 addr, u32 len)
{
	int i;
	u8 checksum = 0;

	// send header
	u8 *sysex_buffer_ptr = &sysex_buffer[0];
	for(i=0; i<sizeof(mios32_midi_sysex_header); ++i)
		*sysex_buffer_ptr++ = mios32_midi_sysex_header[i];

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
	u8 value7 = 0;
	u8 bit_ctr7 = 0;
	i=0;
	for(i=0; i<len; ++i) {
		u8 value8 = MEM8(addr+i);
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
static s32 BSL_SYSEX_WriteMem(u32 addr, u32 len, u8 *buffer)
{
	// check for flash memory range
	if( addr >= BSL_FLASH_WRITE_FLOOR && addr <= BSL_FLASH_WRITE_CEIL ) {

#if defined(MIOS32_FAMILY_STM32G0xx)
		// check for alignment
		if( (addr % 8) || (len % 8) )
			return -MIOS32_MIDI_SYSEX_DISACK_ADDR_NOT_ALIGNED;
		// FLASH_* routines are part of the STM32 code library
		LL_FLASH_Unlock();

		LL_FLASH_Status status;
		int i;
		for(i=0; i<len; addr+=8, i+=8) {
			//MIOS32_IRQ_Disable();

			if( (addr % FLASH_PAGE_SIZE) == 0 ) {
				uint32_t page = addr/FLASH_PAGE_SIZE;
				//FLASH->SR = LL_FLASH_SR_CLEAR;
				if( (status=LL_FLASH_PageErase(LL_FLASH_BANK_1, page)) != FLASH_COMPLETE ) {
					//LL_FLASH_ClearFlag(LL_FLASH_SR_CLEAR); // clear error flags, otherwise next program attempts will fail
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
					uint32_t error=LL_FLASH_GetError();
					MIOS32_MIDI_SendDebugMessage("erase failed for 0x%08x: code %d err 0x%08x\n", addr, status, error);
#endif
					return -MIOS32_MIDI_SYSEX_DISACK_WRITE_FAILED;
				}
			}
			uint64_t data =
					((uint64_t)buffer[i+0] <<  0) |
					((uint64_t)buffer[i+1] <<  8) |
					((uint64_t)buffer[i+2] << 16) |
					((uint64_t)buffer[i+3] << 24) |
					((uint64_t)buffer[i+4] << 32) |
					((uint64_t)buffer[i+5] << 40) |
					((uint64_t)buffer[i+6] << 48) |
					((uint64_t)buffer[i+7] << 56);
			//FLASH->SR = LL_FLASH_SR_CLEAR;
			if( (status=LL_FLASH_ProgramDoubleWord(addr, data)) != FLASH_COMPLETE ) {
				//LL_FLASH_ClearFlag(LL_FLASH_SR_CLEAR); // clear error flags, otherwise next program attempts will fail
				//MIOS32_IRQ_Enable();
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
				uint32_t error=LL_FLASH_GetError();
				MIOS32_MIDI_SendDebugMessage("write failed for 0x%08x: code %d err 0x%08x\n", addr, status, error);
#endif
				//FLASH->SR = LL_FLASH_SR_CLEAR;
				return -MIOS32_MIDI_SYSEX_DISACK_WRITE_FAILED;
			}
			//MIOS32_IRQ_Enable();
			//LL_FLASH_FlushCaches();
			// TODO: verify programmed code
		}
#elif defined(MIOS32_FAMILY_STM32F10x)
		// check for alignment
		if( (addr % 4) || (len % 4) )
			return -MIOS32_MIDI_SYSEX_DISACK_ADDR_NOT_ALIGNED;
		// FLASH_* routines are part of the STM32 code library
		LL_FLASH_Unlock();

		LL_FLASH_Status status;
		int i;
		for(i=0; i<len; addr+=2, i+=2) {
			MIOS32_IRQ_Disable();
			if( (addr % FLASH_PAGE_SIZE) == 0 ) {
				if( (status=LL_FLASH_ErasePage(addr)) != FLASH_COMPLETE ) {
					LL_FLASH_ClearFlag(FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR); // clear error flags, otherwise next program attempts will fail
					MIOS32_IRQ_Enable();
					return -MIOS32_MIDI_SYSEX_DISACK_WRITE_FAILED;
				}
			}

			if( (status=LL_FLASH_ProgramHalfWord(addr, buffer[i+0] | ((u16)buffer[i+1] << 8))) != FLASH_COMPLETE ) {
				LL_FLASH_ClearFlag(FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR); // clear error flags, otherwise next program attempts will fail
				MIOS32_IRQ_Enable();
				return -MIOS32_MIDI_SYSEX_DISACK_WRITE_FAILED;
			}
			MIOS32_IRQ_Enable();

			// TODO: verify programmed code
		}
#elif defined(MIOS32_FAMILY_STM32F4xx)
		// check for alignment
		if( (addr % 4) || (len % 4) )
			return -MIOS32_MIDI_SYSEX_DISACK_ADDR_NOT_ALIGNED;
		// FLASH_* routines are part of the STM32 code library
		LL_FLASH_Unlock();

		// erase if new sector is reached
		{
			int sector;
			for(sector=0; sector<MAX_FLASH_SECTOR; ++sector) {
#ifdef BSL_UPDATER
				// updater build writes sector 0 (the BSL region itself) - its
				// map base is faked to 0xffffffff (upload protection for the
				// normal BSL), so match the real base here instead
				u32 sector_base = (sector == 0) ? 0x08000000 : flash_sector_map[sector][0];
				if( addr == sector_base ) {
#else
				if( addr == flash_sector_map[sector][0] ) {
#endif
					// erase only if really required
					// helps in the case, that an erase takes more than 1 second.
					// if this happens, MIOS Studio will retry the memory transfer, and in this case the sector will be erased again.
					// period...
					u8 erase_required = 0;
					{
						if( addr == FLASH_START_ADDR ) { // using app start address as an indicator that all flash sectors have to be erased (again)
							flash_erase_done = 0;
						}

						u32 flash_sector_mask = (1 << sector);
						if( !(flash_erase_done & flash_sector_mask) ) {
							erase_required = 1;
							flash_erase_done |= flash_sector_mask;
						}
					}

					if( erase_required ) {
						LL_FLASH_Status status = LL_FLASH_EraseSector(flash_sector_map[sector][1], LL_FLASH_VOLTRG_3);
						if( status != FLASH_COMPLETE ) {
							LL_FLASH_ClearFlag(0xffffffff); // clear error flags, otherwise next program attempts will fail
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
							MIOS32_MIDI_SendDebugMessage("erase failed for 0x%08x: code %d\n", addr, status);
#endif
							return -MIOS32_MIDI_SYSEX_DISACK_WRITE_FAILED;
						}
					}
					break;
				}
			}
		}

		int i;
		for(i=0; i<len; addr+=4, i+=4) {
			uint32_t data =
					((uint64_t)buffer[i+0] <<  0) |
					((uint64_t)buffer[i+1] <<  8) |
					((uint64_t)buffer[i+2] << 16) |
					((uint64_t)buffer[i+3] << 24);

			LL_FLASH_Status status = LL_FLASH_ProgramWord(addr, data);
			if( status != FLASH_COMPLETE ) {
				LL_FLASH_ClearFlag(0xffffffff); // clear error flags, otherwise next program attempts will fail
#ifndef MIOS32_MIDI_DISABLE_DEBUG_MESSAGE
				MIOS32_MIDI_SendDebugMessage("program flash failed for 0x%08x: code %d\n", addr, status);
#endif
				return -MIOS32_MIDI_SYSEX_DISACK_WRITE_FAILED;
			}

			LL_FLASH_DataCacheReset();
			LL_FLASH_InstructionCacheReset();

			// TODO: verify programmed code
		}

#else
# warning "Flash Programming not prepared for this family"
#endif

		return 0; // no error
	}

	// check for SRAM memory range
	if( addr >= SRAM_START_ADDR && addr <= SRAM_END_ADDR ) {

		// transfer buffer into SRAM
		memcpy((u8 *)addr, (u8 *)buffer, len);

		return 0; // no error
	}

	// invalid address
	return -MIOS32_MIDI_SYSEX_DISACK_WRONG_ADDR_RANGE;
}




