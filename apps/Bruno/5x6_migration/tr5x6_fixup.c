/*
 * 5x6 high-flash fixup - the ONE thing this tool does that the generic BSL
 * updater does not.
 *
 * WHY
 * ~~~
 * A board flashed in June 2025 lays its last flash page out the old way: the
 * sound-ROM magic in the very last byte, the bank number just below, and both
 * carrying values from the OLD numbering. The current firmware reads its magic
 * two bytes lower AND expects a different value there - a nibble-encoded
 * 0x75/0x76 instead of the flat 75/76 the old firmware wrote. A board left
 * un-fixed reads as an empty ROM, asks which machine it is, and formats -
 * losing every sound bank.
 *
 * It does not MOVE anything: it OVERWRITES the page's system fields with the
 * values the current firmware expects. Nothing in there is worth preserving on
 * APP_HARD_REV 1, where the bank number is neither written nor read any more.
 *
 * WHICH MACHINE
 * ~~~~~~~~~~~~~
 * From TR5X6_FIXUP_UNIT below, a build-time choice. The desktop application
 * knows the type - it pings the instrument before starting anything, and the
 * answer is unambiguous (0x50 or 0x62) - so it picks the image built for that
 * machine. Nothing is read back from the board and nothing is deduced.
 *
 * WHERE IT RUNS
 * ~~~~~~~~~~~~~
 * From BSL_SYSEX_ReleaseHaltState(), case A, once the new bootloader image has
 * been written: interrupts are already off and the caller owns the flash
 * sequence. The page this touches (top of flash) is far from the BSL region
 * the update just wrote, so the two never collide.
 *
 * WHAT IT DOES NOT TOUCH
 * ~~~~~~~~~~~~~~~~~~~~~~
 * The instrument's SysEx device ID. That lives in the bootloader info block at
 * boundary-0x100, and the updater already rescues it: it scans the candidate
 * boundaries for the old block, keeps it in RAM, and rewrites it at the new
 * position. Nothing here may interfere with that.
 */

#include <adios.h>
#include "tr5x6_fixup.h"

/////////////////////////////////////////////////////////////////////////////
// The target machine. 5 = TR-505, 6 = TR-626.
// The deployed beta boards are 505s; a 626 build is one edit away.
/////////////////////////////////////////////////////////////////////////////
#ifndef TR5X6_FIXUP_UNIT
# define TR5X6_FIXUP_UNIT  5
#endif

#if TR5X6_FIXUP_UNIT != 5 && TR5X6_FIXUP_UNIT != 6
# error "TR5X6_FIXUP_UNIT must be 5 (TR-505) or 6 (TR-626)"
#endif

/////////////////////////////////////////////////////////////////////////////
// The page layout the CURRENT firmware expects.
// Spelled out here rather than #included: this tool is built on its own and
// must not drag the application's headers in - but these five offsets and the
// two magics have to stay in step with apps/Bruno/5x6_display/tr5x6_rom.h:54-65.
/////////////////////////////////////////////////////////////////////////////

#define FIX_PAGE_SIZE      0x800

#define FIX_BC_OFS         0x7F6	/* tr5x6_bc_t.ALL, 16 bits LE */
#define FIX_BANK_OFS       0x7FC
#define FIX_MAGIC_OFS      0x7FD
#define FIX_ID_CONFIRM_OFS 0x7FE
#define FIX_ID_OFS         0x7FF

/* high nibble = format version, low nibble = unit */
#define FIX_MAGIC          ((TR5X6_FIXUP_UNIT == 6) ? 0x76 : 0x75)

/* TR5X6_BC_DEFAULT_ALL: ctrl=PC(0), chn=9 (channel 10, stored zero-based),
   OMNI on, receive on, transmit on */
#define FIX_BC_DEFAULT     ((u16)( (0u) | (9u<<2) | (1u<<6) | (1u<<7) | (1u<<8) ))

static u8 fix_page[FIX_PAGE_SIZE] __attribute__ ((aligned (8)));


s32 TR5X6_FIXUP_HighPage(void)
{
	u32 page_addr = 0x08000000 + ADIOS_SYS_FlashSizeGet() - FIX_PAGE_SIZE;
	u32 page_num  = (ADIOS_SYS_FlashSizeGet() - FIX_PAGE_SIZE) / FIX_PAGE_SIZE;
	int i;

	// everything below the system fields - the bank records - is carried over
	// untouched; only the tail is rewritten
	for(i=0; i<FIX_PAGE_SIZE; ++i)
		fix_page[i] = *(volatile u8 *)(page_addr + i);

	fix_page[FIX_MAGIC_OFS] = FIX_MAGIC;

	// neither written nor read on APP_HARD_REV 1 (the firmware starts on bank
	// 1) - seeded neutral so a fixed page matches a formatted one
	fix_page[FIX_BANK_OFS] = 0;

	// MIDI bank change: its offset is new, so an un-fixed board reads 0xffff
	// there. Seeding the defaults means the instrument comes up configured
	// instead of on whatever an erased word decodes to.
	fix_page[FIX_BC_OFS + 0] = (u8)(FIX_BC_DEFAULT & 0xff);
	fix_page[FIX_BC_OFS + 1] = (u8)(FIX_BC_DEFAULT >> 8);

	// the application's own copy of the device ID. Nothing reads it today -
	// the OS takes its ID from the bootloader info block - but a format writes
	// it, so a fixed-up page matches a formatted one byte for byte.
	fix_page[FIX_ID_CONFIRM_OFS] = 0x42;
	fix_page[FIX_ID_OFS]         = ADIOS_MIDI_DeviceIDGet() & 0x7f;

	LL_FLASH_Unlock();

	LL_FLASH_Status status;
	if( (status=LL_FLASH_PageErase(LL_FLASH_BANK_1, page_num)) != FLASH_COMPLETE ) {
		LL_FLASH_Lock();
		return -1;
	}

	u32 addr = page_addr;
	for(i=0; i<FIX_PAGE_SIZE; addr+=8, i+=8) {
		uint64_t data =
			((uint64_t)fix_page[i+0] <<  0) | ((uint64_t)fix_page[i+1] <<  8) |
			((uint64_t)fix_page[i+2] << 16) | ((uint64_t)fix_page[i+3] << 24) |
			((uint64_t)fix_page[i+4] << 32) | ((uint64_t)fix_page[i+5] << 40) |
			((uint64_t)fix_page[i+6] << 48) | ((uint64_t)fix_page[i+7] << 56);
		if( (status=LL_FLASH_ProgramDoubleWord(addr, data)) != FLASH_COMPLETE ) {
			LL_FLASH_Lock();
			// the page is erased and only partly rewritten: the sound banks in
			// it are gone. Saying so beats a silent half-write.
			return -2;
		}
	}

	LL_FLASH_Lock();
	return 0;
}
