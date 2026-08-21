/*
 * 5x6 ROM one-shot migration tool
 *
 * WHY THIS EXISTS
 * ~~~~~~~~~~~~~~~
 * ADIOS gained a persistent SysEx device ID (ADIOS_DEVICE_ID_PERSIST). Its
 * record occupies the LAST TWO BYTES of flash, because that is the one address
 * an application and a bootloader both compute for themselves - so neither has
 * to be told where the other put it.
 *
 * The 5x6 firmware already used those two bytes: its sound-ROM magic number sat
 * in the very last one and its current-bank byte just below. Both moved down by
 * two to make room. A machine whose ROM was written by an older firmware
 * therefore carries its magic where the new firmware no longer looks - it would
 * be taken for an empty ROM and FORMATTED, losing every sound bank.
 *
 * This tool moves those two bytes, once, preserving everything else in the
 * page. It is deliberately a separate application rather than a check inside
 * the firmware: the need disappears after one run per machine, and carrying
 * that code in every future release would be carrying dead weight forever.
 *
 * SEQUENCE (enforced by the desktop tool, see the project checklist)
 *   1. upload the bootloader update  (5x6_505_bsl_updater.hex)
 *   2. upload THIS                    <- migrates the ROM, once
 *   3. upload the firmware            (5x6_505_app_only.hex)
 *
 * It also carries the device ID across: at the moment it runs, the old
 * bootloader info block is still in flash and still holds the ID a user chose
 * with the bootloader update tool. ADIOS does not read that block any more, so
 * this is the last chance to rescue the value - after this, the instrument
 * answers on the same ID it always did, without anybody re-entering it.
 *
 * ==========================================================================
 */

#include <adios.h>
#include <app_lcd.h>
#include "app.h"


/////////////////////////////////////////////////////////////////////////////
// The 5x6 ROM layout, as this tool has to know it.
//
// Deliberately spelled out here rather than #included from the firmware: this
// tool describes the OLD layout as well as the new one, which the firmware no
// longer knows about. Keeping its own copy is what lets the firmware's headers
// forget the past entirely.
/////////////////////////////////////////////////////////////////////////////

// sound-ROM magic, 505 flavour (the 626 uses another value and is not covered -
// its own field layout has not moved yet, see the project checklist)
#define ROM_MAGIC_NUMBER    75

#define ROM_PAGE_SIZE       0x800

// offsets inside the LAST page
#define OLD_BANK_OFS        0x7FE
#define OLD_MAGIC_OFS       0x7FF
#define NEW_BANK_OFS        0x7FC
#define NEW_MAGIC_OFS       0x7FD
#define ID_CONFIRM_OFS      0x7FE   // ADIOS's, 0x42 marker
#define ID_OFS              0x7FF   // ADIOS's, the device ID

static u8 page_buffer[ROM_PAGE_SIZE];


/////////////////////////////////////////////////////////////////////////////
// Moves the two system bytes down and seeds the device-ID record.
// Returns 1 if it wrote, 0 if there was nothing to do, negative on failure.
/////////////////////////////////////////////////////////////////////////////
static s32 ROM_Migrate(void)
{
  u32 last_page_addr = 0x08000000 + ADIOS_SYS_FlashSizeGet() - ROM_PAGE_SIZE;
  u32 last_page_num  = (ADIOS_SYS_FlashSizeGet() - ROM_PAGE_SIZE) / ROM_PAGE_SIZE;
  int i;

  for(i=0; i<ROM_PAGE_SIZE; ++i)
    page_buffer[i] = *(volatile u8 *)(last_page_addr + i);

  // The NEW position is tested FIRST, and that order matters: after a
  // migration the old magic offset holds the device ID, which could itself be
  // 75 and would then look like an un-migrated ROM. Checking the new position
  // first makes a second run unambiguous instead of merely unlikely.
  if( page_buffer[NEW_MAGIC_OFS] == ROM_MAGIC_NUMBER ) {
    ADIOS_MIDI_SendDebugMessage("[5x6 migration] already migrated - nothing to do\n");
    return 0;
  }

  if( page_buffer[OLD_MAGIC_OFS] != ROM_MAGIC_NUMBER ) {
    // no magic anywhere: this ROM has never been written. Leave it alone - the
    // firmware itself offers to format an empty ROM, which is not our job.
    ADIOS_MIDI_SendDebugMessage("[5x6 migration] no sound ROM found - nothing to do\n");
    return 0;
  }

  // move our two fields down...
  page_buffer[NEW_MAGIC_OFS] = ROM_MAGIC_NUMBER;
  page_buffer[NEW_BANK_OFS]  = page_buffer[OLD_BANK_OFS];

  // ...and hand the freed pair to ADIOS, carrying the instrument's identity -
  // which APP_Init already adopted, so this simply writes down what the machine
  // is currently answering on.
  u8 rescued_id = ADIOS_MIDI_DeviceIDGet();
  page_buffer[ID_CONFIRM_OFS] = 0x42;
  page_buffer[ID_OFS]         = rescued_id;

  LL_FLASH_Unlock();
  LL_FLASH_Status status;
  if( (status=LL_FLASH_PageErase(LL_FLASH_BANK_1, last_page_num)) != FLASH_COMPLETE ) {
    LL_FLASH_Lock();
    ADIOS_MIDI_SendDebugMessage("[5x6 migration] ERASE FAILED (code %d, err 0x%08x) - ROM untouched\n",
				 status, LL_FLASH_GetError());
    return -1;
  }

  u32 addr = last_page_addr;
  for(i=0; i<ROM_PAGE_SIZE; addr+=8, i+=8) {
    uint64_t data =
      ((uint64_t)page_buffer[i+0] <<  0) | ((uint64_t)page_buffer[i+1] <<  8) |
      ((uint64_t)page_buffer[i+2] << 16) | ((uint64_t)page_buffer[i+3] << 24) |
      ((uint64_t)page_buffer[i+4] << 32) | ((uint64_t)page_buffer[i+5] << 40) |
      ((uint64_t)page_buffer[i+6] << 48) | ((uint64_t)page_buffer[i+7] << 56);
    if( (status=LL_FLASH_ProgramDoubleWord(addr, data)) != FLASH_COMPLETE ) {
      LL_FLASH_Lock();
      // Worst case, and worth being explicit about: the page is erased and
      // only partly rewritten, so the sound banks in it are gone. Nothing can
      // be done from here - saying so plainly beats a silent half-write.
      ADIOS_MIDI_SendDebugMessage("[5x6 migration] WRITE FAILED at 0x%08x (code %d) - RE-SEND YOUR BANKS\n",
				   addr, status);
      return -2;
    }
  }
  LL_FLASH_Lock();

  ADIOS_MIDI_SendDebugMessage("[5x6 migration] done - bank %d kept, device ID %d stored\n",
			       page_buffer[NEW_BANK_OFS], rescued_id);
  return 1;
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called after startup to initialize the application
/////////////////////////////////////////////////////////////////////////////
void APP_Init(void)
{
  // The display is yours to start, and yours to place: put this call where
  // it belongs in your own init sequence. Uncomment to activate it - the
  // driver itself is chosen by LCD= in this project's Makefile.
  //APP_LCD_Init(0);

  // FIRST of all, take the instrument's identity. ADIOS has just read the
  // persistent record, which on a machine that has not been migrated yet does
  // not exist - so it settled on the compile-time default, 0. But the answer is
  // right there in the bootloader info block, still holding whatever ID was set
  // with the old bootloader update tool. Adopting it here means this tool talks
  // on the instrument's real ID from its first word rather than on 0, and that
  // the record it writes below simply records what is already true.
#ifdef ADIOS_SYS_ADDR_BSL_INFO_BEGIN
  {
    u8 *info_confirm = (u8 *)ADIOS_SYS_ADDR_DEVICE_ID_CONFIRM;
    u8 *info_id      = (u8 *)ADIOS_SYS_ADDR_DEVICE_ID;
    u8 *rec_confirm  = (u8 *)ADIOS_SYS_ADDR_PERSIST_DEVICE_ID_CONFIRM;
    // the record wins when it exists - it is the newer statement of the two
    if( *rec_confirm != 0x42 && *info_confirm == 0x42 && *info_id < 0x80 )
      ADIOS_MIDI_DeviceIDSet(*info_id);
  }
#endif

  // give MIOS Studio a moment to be listening before the verdict goes out:
  // the whole job is over long before a human could open a terminal
  ADIOS_DELAY_Wait_uS(50000);

  ROM_Migrate();

  ADIOS_MIDI_SendDebugMessage("[5x6 migration] device ID %d in force - upload the firmware now\n",
			       ADIOS_MIDI_DeviceIDGet());
}


/////////////////////////////////////////////////////////////////////////////
// Remaining hooks: nothing to do. This application's entire purpose is served
// by the time APP_Init() returns, and it is meant to be replaced by the real
// firmware right after.
/////////////////////////////////////////////////////////////////////////////
void APP_Background(void) { while( 1 ); }
void APP_Tick(void) {}
void APP_MIDI_Tick(void) {}
void APP_MIDI_NotifyPackage(adios_midi_port_t port, adios_midi_package_t midi_package) {}
s32 APP_SYSEX_Parser(adios_midi_port_t port, u8 midi_in) { return -1; }
void APP_SRIO_ServicePrepare(void) {}
void APP_SRIO_ServiceFinish(void) {}
void APP_SRIN_NotifyToggle(u32 pin, u32 pin_value) {}
void APP_ENC_NotifyChange(u32 encoder, s32 incrementer) {}
void APP_ADC_NotifyChange(u32 port, u32 chn, u32 value) {}
