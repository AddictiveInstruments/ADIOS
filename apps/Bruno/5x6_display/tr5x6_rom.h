/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rom.h
  * @brief   This file contains all the function prototypes for
  *          the rom.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _TR5X6_ROM_H__
#define _TR5X6_ROM_H__


/* Includes ------------------------------------------------------------------*/
#include <adios.h>

/* Defines ------------------------------------------------------------------*/
/* sound datas ROM */
#define TR5X6_ROM_SECTOR_SIZE 	0x1000	// 4K
// SST39xx040: nor flash memory range (512K)
//# define TR5X6_ROM_START_ADDR  (0x00000000)
//# define TR5X6_ROM_END_ADDR    (0x00080000 - 1)
// SST39xx168x: Flash Plus(MPF+) memory range (2M)
#define TR5X6_ROM_START_ADDR  	(0x00000000)
#define TR5X6_ROM_END_ADDR    	(0x00200000 - 1)

/* names, colors and misc ROM */
#define TR5X6_FLASH_PAGE_SIZE 0x800	// 2K
// We use the last 4 pages of the internal flash as ROM, 8K
#define TR5X6_FLASH_START_ADDR  (0x08000000 + ADIOS_SYS_FlashSizeGet() - 0x2000)	//0x0801E000
#define TR5X6_FLASH_END_ADDR    (0x08000000 + ADIOS_SYS_FlashSizeGet() - 1)		//0x0801FFFF
#define TR5X6_FLASH_PAGE_BASE	((ADIOS_SYS_FlashSizeGet() - 0x2000)/TR5X6_FLASH_PAGE_SIZE)	// 2K

/* System fields of the LAST page, anchored at its END rather than on the bank
   structure above them. That anchoring is what makes the same positions valid
   for the 505 (112 bytes of bank info per page) and the 626 (56), and for
   whatever a future variant lays out below.
   The top TWO bytes are NOT ours: ADIOS_DEVICE_ID_PERSIST keeps its record in
   the last two bytes of flash - the one address a bootloader finds without
   being told anything by the application (include/adios/adios_sys.h). Ours
   moved down by two to make room, which is why a machine formatted by an older
   firmware needs the one-shot migration app before running this one. */
#define TR5X6_FLASH_SYS_BANK_OFS		0x7FC	/* current bank  (was 0x7FE) */
#define TR5X6_FLASH_SYS_MAGIC_OFS		0x7FD	/* magic number  (was 0x7FF) */
#define TR5X6_FLASH_SYS_ID_CONFIRM_OFS	0x7FE	/* ADIOS's, 0x42 marker     */
#define TR5X6_FLASH_SYS_ID_OFS			0x7FF	/* ADIOS's, the device ID   */
#define TR5X6_FLASH_MAGIC_ADDR			(TR5X6_FLASH_END_ADDR - 2)


#define TR5X6_FLASH_INFO_SIZE 			28
// (4 banks x 16 slots) on the 505, (2 x 32) on the 626: 1792 either way.
// It never diverged - the 626 line forced 32 slots instead of its real 30
// precisely so both would land on the same offset.
#define TR5X6_FLASH_BANK_INFO_OFFSET 	(64*TR5X6_FLASH_INFO_SIZE)
/* Structures ----------------------------------------------------------------*/
typedef enum
{
  TR5X6_ROM_ERROR = -3,
  TR5X6_ROM_TIMEOUT = -2,
  TR5X6_ROM_BUSY = -1,
  TR5X6_ROM_OK = 0
}tr5x6_rom_status;

typedef enum {
	SIZE_4K=0,
	SIZE_8K,
	SIZE_16K,
	SIZE_32K
} tr5x6_slot_size_t;

typedef enum {
	SLOT_EVEN=0,
	SLOT_ODD
} tr5x6_slot_parity_t;

// ONE slot structure for both hosts - the 626 layout. The 505 never shows
// the shortnames (it has no grid), they sleep in flash, ready for the day
// the unit comes from the flash magic instead of the build.
typedef struct {
	char name[14];
	char shortname[4];
	u32 addr_offset:18;
	u32 size:2;
	u32 parity:1;
	u32 dummy:11;
} tr5x6_slot_t;

//typedef union {
//	struct __attribute__((__packed__)){
//		u8 ALL[6528];
//	};
//	struct __attribute__((__packed__)){
//		tr5x6_flash_info_t slots[256];
//		tr5x6_flash_info_t banks[16];
//		u8 diummy[1664];
//	};
//} tr5x6_flash_t;

typedef union {
	struct __attribute__((__packed__)){
		u8 ToWrite[TR5X6_FLASH_INFO_SIZE];
		u8 bank;
		u8 slot;
		u16 dummy;
	};
	struct __attribute__((__packed__)){
		char name[22];	// 22
		u32 color;		// 4
		u16 magic;		// 2
		u32 dummy1;
	};
} tr5x6_flash_info_t;

typedef union {
	struct {
		u8 ALL;
	};
	struct {
		u8 bank:4;
		u8 :3;
		u8 memorized:1;
	};
} tr5x6_flash_mem_Bank_t;

// Everything that makes a host what it is, in one place. Two const instances
// in flash, one pointer aimed at boot - the ONE line that changes the day the
// unit comes from the flash magic instead of the build.
typedef struct {
	const tr5x6_slot_t *slots;	// the slot table
	u8  slot_num;			// 16 / 30
	u8  bank_num;			// 16 / 8
	u8  bank_divider;		// bank_num/4: 4 / 2
	u8  bank_shift;			// host address shift: 17 / 18
	u8  digits_num;			// 6 / 7 seven-segment digits
	u8  magic;			// 0x75 / 0x76 - high nibble = version,
					// low nibble = unit (5 = 505, 6 = 626)
	u8  sysex_ack_type;		// 0x50 / 0x62
} tr5x6_unit_t;

extern const tr5x6_unit_t tr5x6_unit_505;
extern const tr5x6_unit_t tr5x6_unit_626;
extern const tr5x6_unit_t *tr5x6_unit;

// BOTH tables live in flash on both builds - that is the price of the one
// firmware to come. Reached through tr5x6_unit->slots, never mirrored.
extern const tr5x6_slot_t tr5x6_slots_505[16];
extern const tr5x6_slot_t tr5x6_slots_626[30];

extern const char tr5x6_slot_name[4][4];
extern const char tr5x6_slot_duration[4][6];
extern s32 tr5x6_rom_format_stat;
extern s32 tr5x6_rom_spi_err;
extern s32 tr5x6_flash_format_stat;
/* Prototypes ----------------------------------------------------------------*/
extern void TR5X6_ROM_Init(void);
extern void TR5X6_ROM_HOST(void);
extern u8 TR5X6_ROM_BankGet(void);
extern s32 TR5X6_ROM_BankSet(u8 bank, u8 address_set);
extern u32 TR5X6_ROM_ProgAddr(u8 bank);
extern u32 TR5X6_ROM_HostAddr(u8 bank);
extern s32 TR5X6_ROM_BaseAddrSet(uint8_t bank);
extern s32 TR5X6_ROM_Read(uint32_t address);
extern tr5x6_rom_status TR5X6_ROM_Write(uint32_t address, uint8_t data, uint32_t Timeout);
extern tr5x6_rom_status TR5X6_ROM_Sector_Erase(uint32_t sector, uint32_t Timeout);
extern s32 TR5X6_FLASH_SlotRead(tr5x6_flash_info_t* slot);
extern s32 TR5X6_FLASH_BankRead(tr5x6_flash_info_t* slot);
extern s32 TR5X6_MEM_Format(void);
extern s32 TR5X6_FLASH_Slot_Write(tr5x6_flash_info_t slot);
extern s32 TR5X6_FLASH_Bank_Write(tr5x6_flash_info_t slot);
extern s32 TR5X6_ROM_BankStore(u8 bank);
/* persists the SysEx device ID in the last page's system fields, so that
   ADIOS finds it at the next power-up - and so does the bootloader */
extern s32 TR5X6_ROM_DeviceIDStore(u8 device_id);
extern u8 TR5X6_ROM_BankRecall(void);
#if TR5X6_UNIT_SELECT==626
extern s32 TR5X6_ROM_BankStore_Req();
#endif
#if 0
extern s32 TR5X6_FLASH_MemBank_Write(u8 group, u8 pattern, tr5x6_flash_mem_Bank_t bank_mem);
extern tr5x6_flash_mem_Bank_t TR5X6_FLASH_MemBank_Get(u8 group, u8 pattern);
#endif

#endif /*_TR5X6_ROM_H__ */
