/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "tr5x6_rom.h"
#include "app_lcd.h"
#include "glcd_font.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* ---------------------------------------------------------------------------
   THE ROM BUS, AND WHO OWNS WHICH ADDRESS LINE

   The sound ROM sits between the host machine and us. A latch chain, fed by
   three SPI bytes and strobed by ADR, drives the address lines; nHOST decides
   which side is in charge:

     HOST mode (nHOST=0) - the machine reads its own samples
     PROG mode (nHOST=1) - we read, write and erase the chip

   The 24-bit word handed to the latch is NOT a flat address:

     bits 0..16   A0-A16   host in HOST mode, latch in PROG mode
     bit  17      A17      MUXED - see bit 23
     bits 18..22  A18-A22  latch at all times, in BOTH modes
     bit  23      -        the mux: 1 = A17 from the latch, 0 = A17 from host

   Bit 23 exists because the two host machines cut the same chip differently:

     505   16 banks of 128K   host addresses A0-A16   bank = A17-A20
           A17 is already part of the bank number, so bit 23 is set at all
           times, host mode included (see TR5X6_ROM_Addr_Set).

     626   8 banks of 256K    host addresses A0-A17   bank = A18-A20
           A17 belongs to the machine, so bit 23 must be 0 in host mode and 1
           in PROG mode - hence TR5X6_ROM_ProgAddr_Set below.

   Get it wrong either way and the failure is silent, not loud: bit 23 low in
   PROG mode erases the low half of a bank twice and never touches the high
   half; bit 23 high in host mode freezes A17, so the machine reads the same
   half of every instrument forever.

   A21/A22 reach the latch for a larger device than the 2M fitted today.

   One more thing the addresses do not show: samples are stored EVEN/ODD
   interleaved (the SLOT_EVEN/SLOT_ODD column of tr5x6_slots), so a linear
   PROG address flips A0 - that is the `address ^= 1` in Read and Write.
   --------------------------------------------------------------------------- */

/* Defines ------------------------------------------------------------------*/

#define TR5X6_ROM_SPI		0
#define TR5X6_ROM_MAX_DELAY	1000

#define TR5X6_ROM_PORT 	GPIOA
#define TR5X6_ROM_nHOST_PIN 	LL_GPIO_PIN_0
#define TR5X6_ROM_nWR_PIN 		LL_GPIO_PIN_8
#define TR5X6_ROM_nOE_PIN 		LL_GPIO_PIN_10
#define TR5X6_ROM_nCS_PIN 		LL_GPIO_PIN_9
#define TR5X6_ROM_ADR_PIN 		LL_GPIO_PIN_4
#define TR5X6_ROM_DAT_PIN 		LL_GPIO_PIN_3
#define TR5X6_ROM_nRST_PIN 		LL_GPIO_PIN_2
//#define TR5X6_ROM_RDY_PIN 		LL_GPIO_PIN_1

#define PROG() 		ADIOS_SYS_STM_PINSET_1(TR5X6_ROM_PORT, TR5X6_ROM_nHOST_PIN)
#define HOST() 		ADIOS_SYS_STM_PINSET_0(TR5X6_ROM_PORT, TR5X6_ROM_nHOST_PIN)
#define TR5X6_ROM_RD() 	ADIOS_SYS_STM_PINSET_1(TR5X6_ROM_PORT, TR5X6_ROM_nWR_PIN)
#define TR5X6_ROM_WR() 	ADIOS_SYS_STM_PINSET_0(TR5X6_ROM_PORT, TR5X6_ROM_nWR_PIN)
#define TR5X6_ROM_OE() 	ADIOS_SYS_STM_PINSET_0(TR5X6_ROM_PORT, TR5X6_ROM_nOE_PIN)
#define TR5X6_ROM_IE() 	ADIOS_SYS_STM_PINSET_1(TR5X6_ROM_PORT, TR5X6_ROM_nOE_PIN)
#define TR5X6_ROM_CS(v) 	ADIOS_SYS_STM_PINSET(TR5X6_ROM_PORT,TR5X6_ROM_nCS_PIN,v)
#define TR5X6_ROM_ADR(v) 	ADIOS_SYS_STM_PINSET(TR5X6_ROM_PORT,TR5X6_ROM_ADR_PIN,v)
#define TR5X6_ROM_DAT(v) 	ADIOS_SYS_STM_PINSET(TR5X6_ROM_PORT,TR5X6_ROM_DAT_PIN,v)
#define TR5X6_ROM_RST(v) 	ADIOS_SYS_STM_PINSET(TR5X6_ROM_PORT,TR5X6_ROM_nRST_PIN,v)


/* Prototypes ------------------------------------------------------------------*/
void TR5X6_ROM_Addr_Set(uint32_t address);
static void TR5X6_ROM_ProgAddr_Set(uint32_t address);
void TR5X6_ROM_Data_Set(uint8_t data);
s32 TR5X6_ROM_Data_Get(void);

/* Variables ------------------------------------------------------------------*/
static u8 datas[TR5X6_FLASH_PAGE_SIZE];

u8 tr5x6_bc_ctrl = TR5X6_BC_CTRL_DEFAULT;
u8 tr5x6_bc_chn  = TR5X6_BC_CHN_DEFAULT;
u8 tr5x6_bc_omni = TR5X6_BC_OMNI_DEFAULT;
static u8 rom_bank=0;

const char tr5x6_slot_name[4][4]={"4K\0 ", "8K\0 ", "16K\0", "32K\0"};
const char tr5x6_slot_duration[4][6]={"163ms\0", "327ms\0", "655ms\0", "1.31s\0"};

s32 tr5x6_rom_format_stat;
s32 tr5x6_flash_format_stat;
// sticky count of SPI transfers the driver refused to run (-3 busy, -4 lost).
// Anything but zero means a ROM operation was aimed at a stale latch value.
s32 tr5x6_rom_spi_err;

const tr5x6_slot_t tr5x6_slots_505[16]={{"Low ConGa\0    ", "LC\0 ", 0x08000, SIZE_4K, SLOT_ODD},
		{"Hi ConGa\0     ", "HC\0 ", 0x0a000, SIZE_4K, SLOT_ODD},
		{"TimBale\0      ", "TB\0 ", 0x00000, SIZE_8K, SLOT_EVEN},
		{"Low CowBell\0  ", "LCB\0", 0x0c000, SIZE_4K, SLOT_ODD},
		{"Hi CowBell\0   ", "HCB\0", 0x0e000, SIZE_4K, SLOT_ODD},
		{"Hand ClaP\0    ", "HCP\0", 0x0c000, SIZE_4K, SLOT_EVEN},
		{"Crash Cymbal\0 ", "CC\0 ", 0x10000, SIZE_32K, SLOT_EVEN},
		{"Ride Cymbal\0  ", "RC\0 ", 0x18000, SIZE_16K, SLOT_EVEN},
		{"Bass Drum\0    ", "BD\0 ", 0x08000, SIZE_4K, SLOT_EVEN},
		{"Snare Drum\0   ", "SD\0 ", 0x0a000, SIZE_4K, SLOT_EVEN},
		{"Low Tom\0      ", "LT\0 ", 0x02000, SIZE_8K, SLOT_EVEN},
		{"Mid Tom\0      ", "MT\0 ", 0x06000, SIZE_8K, SLOT_EVEN},
		{"Hi Tom\0       ", "HT\0 ", 0x04000, SIZE_8K, SLOT_EVEN},
		{"Rim Shot\0     ", "RS\0 ", 0x0e000, SIZE_4K, SLOT_EVEN},
		{"Closed Hi-hat\0", "CH\0 ", 0x1e000, SIZE_8K, SLOT_EVEN},
		{"Open Hi-hat\0  ", "OH\0 ", 0x1c000, SIZE_8K, SLOT_EVEN}};


const tr5x6_slot_t tr5x6_slots_626[30]={
		{"CowBell\0      ", "CB\0 ", 0x08000, SIZE_4K, SLOT_ODD},
		{"Low TimBale\0  ", "LTB\0", 0x3c000, SIZE_8K, SLOT_EVEN},
		{"Hi TimBale\0   ", "HTB\0", 0x3e000, SIZE_8K, SLOT_EVEN},
		{"Low ConGa\0    ", "LC\0 ", 0x20000, SIZE_8K, SLOT_EVEN},
		{"Open Hi ConGa\0", "OHC\0", 0x00000, SIZE_8K, SLOT_EVEN},
		{"Mute Hi ConGa\0", "MHC\0", 0x28000, SIZE_4K, SLOT_EVEN},
		{"Crash Cymbal\0 ", "CC\0 ", 0x10000, SIZE_32K, SLOT_EVEN},
		{"Ride Cymbal\0  ", "RC\0 ", 0x18000, SIZE_16K, SLOT_EVEN},
		{"Bass Drum 1\0  ", "BD1\0", 0x0c000, SIZE_4K, SLOT_ODD},
		{"Snare Drum 1\0 ", "SD1\0", 0x1c000, SIZE_8K, SLOT_EVEN},
		{"Rim Shot\0     ", "RS\0 ", 0x0c000, SIZE_4K, SLOT_EVEN},
		{"Low Tom 1\0    ", "LT1\0", 0x02000, SIZE_8K, SLOT_EVEN},
		{"Mid Tom 1\0    ", "MT1\0", 0x06000, SIZE_8K, SLOT_EVEN},
		{"Hi Tom 1\0     ", "HT1\0", 0x04000, SIZE_8K, SLOT_EVEN},
		{"Closed Hi-hat\0", "CH\0 ", 0x2e000, SIZE_8K, SLOT_EVEN},
		{"Open Hi-hat\0  ", "OH\0 ", 0x0e000, SIZE_8K, SLOT_EVEN},
		{"TAMBourine\0   ", "TMB\0", 0x0a000, SIZE_4K, SLOT_ODD},
		{"Low AGogo\0    ", "LAG\0", 0x28000, SIZE_4K, SLOT_ODD},
		{"Hi AGogo\0     ", "HAG\0", 0x2a000, SIZE_4K, SLOT_ODD},
		{"Hand ClaP\0    ", "HCP\0", 0x08000, SIZE_4K, SLOT_EVEN},
		{"SHaKer\0       ", "SHK\0", 0x2a000, SIZE_4K, SLOT_EVEN},
		{"CLaVes\0       ", "CLV\0", 0x0a000, SIZE_4K, SLOT_EVEN},
		{"ChiNa Cymbal\0 ", "CNC\0", 0x30000, SIZE_32K, SLOT_EVEN},
		{"Cup\0          ", "CUP\0", 0x38000, SIZE_16K, SLOT_EVEN},
		{"Bass Drum 2\0  ", "BD2\0", 0x2c000, SIZE_4K, SLOT_ODD},
		{"Snare Drum 2\0 ", "SD2\0", 0x1e000, SIZE_8K, SLOT_EVEN},
		{"Snare Drum 3\0 ", "SD3\0", 0x2c000, SIZE_4K, SLOT_EVEN},
		{"Low Tom 2\0    ", "LT2\0", 0x22000, SIZE_8K, SLOT_EVEN},
		{"Mid Tom 2\0    ", "MT2\0", 0x26000, SIZE_8K, SLOT_EVEN},
		{"Hi Tom 2\0     ", "HT2\0", 0x24000, SIZE_8K, SLOT_EVEN}};

//                                    slots        slot bank div shift dig magic  ack cs2 a17  cs2
const tr5x6_unit_t tr5x6_unit_505 = { tr5x6_slots_505, 16, 16,  4,  17,  6, 0x75, 0x50, 0, 1 };
const tr5x6_unit_t tr5x6_unit_626 = { tr5x6_slots_626, 30,  8,  2,  18,  7, 0x76, 0x62, 1, 0 };

// The unit descriptor every user goes through. Aimed once at boot.
const tr5x6_unit_t *tr5x6_unit;

/* ROM init function */
void TR5X6_ROM_Init(void)
{
	// tr5x6_unit is ALREADY aimed when we get here - APP_Init reads the flash
	// magic (or asks the user) and points it before calling us. Everything
	// below reads it: bank shift, A17 mux, slot table.

	// configure GPIO
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* GPIO Ports Clock Enable */
	LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);

	// initialize SPI interface
	// ensure that fast pin drivers are activated, do it before
	ADIOS_SPI_IO_Init(TR5X6_ROM_SPI, ADIOS_SPI_PIN_DRIVER_STRONG);

	GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;

	GPIO_InitStruct.Pin = TR5X6_ROM_nHOST_PIN | TR5X6_ROM_nWR_PIN | TR5X6_ROM_nOE_PIN
			 | TR5X6_ROM_nCS_PIN | TR5X6_ROM_ADR_PIN | TR5X6_ROM_DAT_PIN;
	LL_GPIO_Init(TR5X6_ROM_PORT, &GPIO_InitStruct);

	// initialize SPI interface
	// init SPI port
	//TR5X6_SPI_TransferModeInit();
	ADIOS_SPI_TransferModeInit(TR5X6_ROM_SPI, ADIOS_SPI_MODE_CLK0_PHASE0, ADIOS_SPI_PRESCALER_16);

	TR5X6_ROM_DAT(1);
	TR5X6_ROM_ADR(1);
	TR5X6_ROM_RST(1);

	rom_bank = TR5X6_ROM_BankRecall();
	TR5X6_ROM_HOST();


}

/* HOST Mode */
//void TR5X6_SPI_TransferModeInit(void){
//// init SPI port
//	ADIOS_SPI_TransferModeInit(TR5X6_ROM_SPI, ADIOS_SPI_MODE_CLK0_PHASE0, ADIOS_SPI_PRESCALER_128);
//}
/* HOST Mode */
void TR5X6_ROM_HOST(void){
	TR5X6_ROM_CS(1);			// ROM Deselect
	TR5X6_ROM_RD();				// ROM Read
	TR5X6_ROM_OE();				// ROM Output enable
	HOST();						// ROM HOST Mode
	TR5X6_ROM_Addr_Set(TR5X6_ROM_HostAddr(rom_bank));
	TR5X6_ROM_CS(0);			// ROM Select
}


/* ********* */
u8 TR5X6_ROM_BankGet(void){
	return rom_bank;
}

/* ********* */
// Sets the bank. Does NOT remember it across a power cycle: on APP_HARD_REV
// 1 nothing does. Writing the number meant erasing and reprogramming a 2 Kb
// page, which freezes the core for tens of milliseconds - the G0 executes
// from the very flash it is erasing, so the UART interrupt does not run and
// around a hundred MIDI bytes are lost. Measured 25/08/2026: a burst of bank
// changes over CC#0 left the DIN parser mid-message and timed it out a
// second later. It also spent one of the page's 10 000 erase cycles per
// bank change, on the same page that carries the magic.
// APP_HARD_REV 2 stores it in the EEPROM instead, where neither applies.
s32 TR5X6_ROM_BankSet(u8 bank, u8 address_set){
	if(address_set)
		TR5X6_ROM_Addr_Set(TR5X6_ROM_HostAddr(bank));
	if(bank==rom_bank)return 0;
	rom_bank = bank;
	return 0;
}


/* ********* */
u32 TR5X6_ROM_ProgAddr(u8 bank){
	return (TR5X6_ROM_HostAddr(bank) | 0x00800000);
}
/* ********* */
u32 TR5X6_ROM_HostAddr(u8 bank){
	if(bank > tr5x6_unit->bank_num-1) bank = tr5x6_unit->bank_num-1;
	return (u32)bank << tr5x6_unit->bank_shift;
}

/* ********* */
s32 TR5X6_ROM_Read(uint32_t address){
	if((address&0x007FFFFF)>TR5X6_ROM_END_ADDR)return 0;
	address ^= 0x00000001;
	TR5X6_ROM_CS(1);				// ROM Deselect
	TR5X6_ROM_RD();					// ROM Read pin
	TR5X6_ROM_OE();					// ROM Output enable
	TR5X6_ROM_ProgAddr_Set(address);	// Set ROM address
	PROG();							// ROM PROG Mode
	TR5X6_ROM_CS(0);				// ROM Select
	s32 tmpData= 0;
	tmpData= TR5X6_ROM_Data_Get();
	TR5X6_ROM_CS(1);				// ROM Deselect
	return tmpData;
}

/* ********* */
tr5x6_rom_status TR5X6_ROM_Write(uint32_t address, uint8_t data, uint32_t Timeout){
	if((address&0x007FFFFF)>TR5X6_ROM_END_ADDR)return TR5X6_ROM_ERROR;
	address ^= 0x00000001;
	TR5X6_ROM_CS(1);			// ROM Deselect
	TR5X6_ROM_WR();			// ROM Write
	TR5X6_ROM_IE();			// ROM Input enable
	PROG();				// ROM PROG Mode
	// start sector erase sequence
	//TR5X6_ROM_Addr_Set(0x00005555);
	TR5X6_ROM_ProgAddr_Set(0x00000AAA);
	TR5X6_ROM_Data_Set(0xAA);
	TR5X6_ROM_CS(0);			// ROM Select
	TR5X6_ROM_CS(1);			// ROM Deselect
	//TR5X6_ROM_Addr_Set(0x00002AAA);
	TR5X6_ROM_ProgAddr_Set(0x00000555);
	TR5X6_ROM_Data_Set(0x55);
	TR5X6_ROM_CS(0);			// ROM Select
	TR5X6_ROM_CS(1);			// ROM Deselect
	//TR5X6_ROM_Addr_Set(0x00005555);
	TR5X6_ROM_ProgAddr_Set(0x00000AAA);
	TR5X6_ROM_Data_Set(0xA0);
	TR5X6_ROM_CS(0);			// ROM Select
	TR5X6_ROM_CS(1);			// ROM Deselect
	TR5X6_ROM_ProgAddr_Set(address);
	TR5X6_ROM_Data_Set(data);
	TR5X6_ROM_CS(0);			// ROM Select
	TR5X6_ROM_CS(1);			// ROM Deselect
	// data polling
	TR5X6_ROM_RD();			// ROM Read pin
	TR5X6_ROM_OE();			// ROM Input enable
	s32 tmpData=0;
	ADIOS_DELAY_Wait_uS(4);
	tr5x6_rom_status errorcode = TR5X6_ROM_OK;
	s32 tickstart = 0;

	// The address latch HOLDS its value and nothing else touches the bus while
	// we poll, so set it ONCE here instead of clocking three bytes out on every
	// iteration. The wait itself is set by the chip, but the bus is freed.
	TR5X6_ROM_ProgAddr_Set(address);
	while(tmpData!=data){
        /* Timeout management */
		if((tickstart++)>6000){
			TR5X6_ROM_CS(1);			// ROM Deselect
			return TR5X6_ROM_TIMEOUT;
        }else{
			TR5X6_ROM_CS(0);			// ROM Select
			tmpData=TR5X6_ROM_Data_Get();
			TR5X6_ROM_CS(1);			// ROM Deselect
			if(tmpData<0)return TR5X6_ROM_ERROR;
        }

	}
	return errorcode;
}

/* ********* */
tr5x6_rom_status TR5X6_ROM_Sector_Erase(uint32_t sector, uint32_t Timeout){
	if((sector&0x007FFFFF)>TR5X6_ROM_END_ADDR)return TR5X6_ROM_ERROR;
	TR5X6_ROM_CS(1);			// ROM Deselect
	TR5X6_ROM_WR();			// ROM Write
	TR5X6_ROM_IE();			// ROM Input enable
	PROG();				// ROM PROG Mode
	// start sector erase sequence
	//TR5X6_ROM_Addr_Set(0x00005555);
	TR5X6_ROM_ProgAddr_Set(0x00000AAA);
	TR5X6_ROM_Data_Set(0xAA);
	TR5X6_ROM_CS(0);			// ROM Select
	TR5X6_ROM_CS(1);			// ROM Deselect

	//TR5X6_ROM_Addr_Set(0x00002AAA);
	TR5X6_ROM_ProgAddr_Set(0x00000555);
	TR5X6_ROM_Data_Set(0x55);
	TR5X6_ROM_CS(0);			// ROM Select
	TR5X6_ROM_CS(1);			// ROM Deselect
	//TR5X6_ROM_Addr_Set(0x00005555);
	TR5X6_ROM_ProgAddr_Set(0x00000AAA);
	TR5X6_ROM_Data_Set(0x80);
	TR5X6_ROM_CS(0);			// ROM Select
	TR5X6_ROM_CS(1);			// ROM Deselect
	//TR5X6_ROM_Addr_Set(0x00005555);
	TR5X6_ROM_ProgAddr_Set(0x00000AAA);
	TR5X6_ROM_Data_Set(0xAA);
	TR5X6_ROM_CS(0);			// ROM Select
	TR5X6_ROM_CS(1);			// ROM Deselect
	//TR5X6_ROM_Addr_Set(0x00002AAA);
	TR5X6_ROM_ProgAddr_Set(0x00000555);
	TR5X6_ROM_Data_Set(0x55);
	TR5X6_ROM_CS(0);			// ROM Select
	TR5X6_ROM_CS(1);			// ROM Deselect
	TR5X6_ROM_ProgAddr_Set(sector);
	//TR5X6_ROM_Data_Set(0x30);
	TR5X6_ROM_Data_Set(0x50);
	TR5X6_ROM_CS(0);			// ROM Select
	TR5X6_ROM_CS(1);			// ROM Deselect
	// data polling
	TR5X6_ROM_RD();			// ROM Read pin
	TR5X6_ROM_OE();			// ROM Input enable
	s32 tmpData=0;
	ADIOS_DELAY_Wait_uS(4);
	tr5x6_rom_status errorcode = TR5X6_ROM_OK;
	// s32, not u16: this is compared against 4000000, which a u16 can never
	// reach - the guard could never fire and a stuck erase span for ever.
	s32 tickstart = 0;
	// same as in Write(): the latch holds the poll address, set it once
	TR5X6_ROM_ProgAddr_Set(sector+TR5X6_ROM_SECTOR_SIZE-1);
	while(tmpData!=0xff){
		//ADIOS_BOARD_LED_Set(1, 1);
        /* Timeout management */
		//if(ADIOS_TIMESTAMP_Get()>(tickstart+TR5X6_ROM_MAX_DELAY)){
		if((tickstart++)>4000000){
			TR5X6_ROM_CS(1);			// ROM Deselect

			return TR5X6_ROM_TIMEOUT;
        }else{
			TR5X6_ROM_CS(0);			// ROM Select
			tmpData=TR5X6_ROM_Data_Get();
			TR5X6_ROM_CS(1);			// ROM Deselect

			if(tmpData<0)return TR5X6_ROM_ERROR;
        }

	}
	//for(int i =0; i<count;i++)ADIOS_MIDI_SendDebugMessage("%08x", byte[i]);

	return errorcode;
}

/* ********* */
void TR5X6_ROM_Addr_Set(uint32_t address){
	// Set ROM address


	// On the 505 A17 is the low bit of the bank number, so it belongs to the
	// latch in BOTH modes and the mux stays on. On the 626 A17 belongs to the
	// host and the mux must follow the mode. See the bus map at the top.
	if(tr5x6_unit->a17_from_latch) address |= 0x00800000;
	// Three POLLED byte transfers, not TransferBlock. The DMA path spends some
	// twenty peripheral register writes reconfiguring both channels to move
	// three bytes - overhead that dwarfs the 12 us actually spent clocking them
	// out, and this runs 4096 times per sector read-back.
	// A refused transfer means the bytes never reached the shift register:
	// strobing ADR anyway would latch a stale address and every operation that
	// follows would aim at the wrong place, silently. So leave the latch alone.
	if( ADIOS_SPI_TransferByte(TR5X6_ROM_SPI, (address>>16) & 0xff) < 0 ||
	    ADIOS_SPI_TransferByte(TR5X6_ROM_SPI, (address>>8)  & 0xff) < 0 ||
	    ADIOS_SPI_TransferByte(TR5X6_ROM_SPI,  address       & 0xff) < 0 ) {
		++tr5x6_rom_spi_err;
		return;
	}
	TR5X6_ROM_ADR(0);
	TR5X6_ROM_ADR(1);

}

/* ********* */
/* Every address emitted in PROG mode, unlock command sequences included:
   bit 23 claims A17 for the latch. See the bus map at the top of this file. */
static void TR5X6_ROM_ProgAddr_Set(uint32_t address){
	TR5X6_ROM_Addr_Set(address | 0x00800000);
}

/* ********* */
void TR5X6_ROM_Data_Set(uint8_t data){
	TR5X6_ROM_DAT(0);
	// same rule as the address latch: no transfer, no strobe (see Addr_Set)
	if( ADIOS_SPI_TransferByte(TR5X6_ROM_SPI, data) < 0 )
		++tr5x6_rom_spi_err;
	TR5X6_ROM_DAT(1);
}

/* ********* */
s32 TR5X6_ROM_Data_Get(void){
	TR5X6_ROM_DAT(0);
	ADIOS_DELAY_Wait_uS(1);
	TR5X6_ROM_DAT(1);
	//uint8_t tmpData=0;
	//HAL_SPI_Receive(&hspi2, &tmpData, 1, 1);
	return ADIOS_SPI_TransferByte(TR5X6_ROM_SPI, 0xff);
	//return tmpData;
}

/* ********* */
s32 TR5X6_MEM_Format(void){

	tr5x6_rom_format_stat=(s32)TR5X6_ROM_OK;
	// ROM blanking
	tr5x6_rom_status errorcode = TR5X6_ROM_OK;
	//u8 sect_bmp_array[270*2];
	u8 prog_bmp_array[300*2];
	char message[60];
	//adios_lcd_bitmap_t sect_bmp = APP_LCD_BitmapInit((u8*)sect_bmp_array, 270, 16, 270, Is1BIT);
	adios_lcd_bitmap_t prog_bmp = APP_LCD_BitmapInit((u8*)prog_bmp_array, 300, 16, 300, Is1BIT);
	APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
	APP_LCD_Rectangle(89, 14+80, 302, 18, 1, APP_LCD_WHITE, 0, 0);

	u16 bad_sectors=0;
	s32 first_bad_sector=-1;
	for(int sector=0; sector<((TR5X6_ROM_END_ADDR+1)/TR5X6_ROM_SECTOR_SIZE);sector++){

		u32 addr=(sector*TR5X6_ROM_SECTOR_SIZE) | 0x00800000;
		//for(int i =0; i<600; i++)prog_bmp_array[i] = 0x00;

		// A sector that refuses to erase or to program is reported and skipped,
		// never fatal: the format has to reach the internal flash and write the
		// magic, or the machine reboots into this page for ever.
		if((errorcode=TR5X6_ROM_Sector_Erase(sector*TR5X6_ROM_SECTOR_SIZE, 1000))==TR5X6_ROM_OK){
			for(u16 b=0;b<TR5X6_ROM_SECTOR_SIZE;b++){
				if((errorcode=TR5X6_ROM_Write((u32)(addr+b), 0x80, 1000))!=TR5X6_ROM_OK){
					tr5x6_rom_format_stat=(s32)(errorcode-4);
					sprintf(message, "Write addr@0x%08x error#%d!", (u32)(addr+b-0x00800000),  errorcode);
					break;	// give this sector up, carry on with the next one
				}
			}
			if(errorcode>=0){
				sprintf(message, "Sector#%u erased, addr@0x%08x", sector, addr-0x00800000);

			}
		}else{
			tr5x6_rom_format_stat=(s32)errorcode;
			sprintf(message, "Sector#%u erase, addr@0x%08x, error#%d!", sector, addr-0x00800000, errorcode);
		}
		if(errorcode<0){
			bad_sectors++;
			if(first_bad_sector<0)first_bad_sector=sector;
		}
		ADIOS_MIDI_SendDebugMessage(message);
		APP_LCD_Rectangle(90, 15+62, 300, 12, 0, 0, 2, APP_LCD_BLACK);
		APP_LCD_PrintFormattedString(92, 15+64, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "%s", message);
		u16 progress= 100*(sector+1)/512;
		memset(&prog_bmp_array, 0x00, sizeof(prog_bmp_array));

		APP_LCD_BitmapPrintFormattedString(prog_bmp, 1.0, 150, 2, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "%u%s", progress, "%");
		APP_LCD_PrintProgress(prog_bmp, 0x00ff0000, 90, 15+80, 300, 16, 300*progress/100);

	}

	// What the ROM pass found, said before the internal flash is touched. The
	// unit is the SECTOR: a write error gives its sector up, so the format
	// never learns how many cells below it are really dead - counting those
	// takes a readback pass, which belongs to the diagnostic tool.
	if(bad_sectors)
		sprintf(message, "ROM report: %u sector%s in error (first #%d)",
				bad_sectors, (bad_sectors>1)?"s":"", first_bad_sector);
	else
		sprintf(message, "ROM report: all %u sectors formatted",
				(u32)((TR5X6_ROM_END_ADDR+1)/TR5X6_ROM_SECTOR_SIZE));
	if(tr5x6_rom_spi_err)
		ADIOS_MIDI_SendDebugMessage("ROM report: %d SPI transfers refused!", tr5x6_rom_spi_err);
	ADIOS_MIDI_SendDebugMessage(message);
	APP_LCD_Rectangle(90, 15+62, 300, 12, 0, 0, 2, APP_LCD_BLACK);
	APP_LCD_PrintFormattedString(92, 15+64, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "%s", message);

	APP_LCD_PrintString(92, 15+120, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "Formatting   FLASH...");
	APP_LCD_Rectangle(89, 14+150, 302, 18, 1, APP_LCD_WHITE, 0, 0);
	// Flash formatting
	tr5x6_flash_info_t slot;
	u8* data_ptr = &datas[0];
	memset(data_ptr, 0, TR5X6_FLASH_PAGE_SIZE);
	// Prepare slots datas
	memset(slot.ToWrite, 0, TR5X6_FLASH_INFO_SIZE);
	sprintf(slot.name, (char*)"No Name...");
	slot.color = 0x00ffffff;
	for(int b=0; b<tr5x6_unit->bank_divider;b++){
		for(int s=0; s<tr5x6_unit->slot_num;s++){
			memcpy(data_ptr, slot.ToWrite, TR5X6_FLASH_INFO_SIZE);
			data_ptr +=TR5X6_FLASH_INFO_SIZE;
		}

	}
	// Prepare banks datas
	data_ptr = &datas[0]+TR5X6_FLASH_BANK_INFO_OFFSET;
	memset(slot.ToWrite, 0, TR5X6_FLASH_INFO_SIZE);
	sprintf(slot.name, (char*)"User Bank");
	slot.color = 0x00ffffff;
	for(int b=0; b<tr5x6_unit->bank_divider;b++){
		memcpy(data_ptr, slot.ToWrite, TR5X6_FLASH_INFO_SIZE);
		data_ptr +=TR5X6_FLASH_INFO_SIZE;
	}
	for(int p=0; p<4;p++){

		// System fields of the last page. This path builds the page from
		// scratch, so unlike TR5X6_ROM_BankStore() it cannot inherit what was
		// there - the device ID has to be carried over explicitly, or a
		// format would silently reset the instrument's identity.
		if(p==3){
			datas[TR5X6_FLASH_SYS_MAGIC_OFS]     = tr5x6_unit->magic;
			datas[TR5X6_FLASH_SYS_BANK_OFS]      = 0; // stored bank num
			datas[TR5X6_FLASH_SYS_ID_CONFIRM_OFS]= 0x42;
			datas[TR5X6_FLASH_SYS_ID_OFS]        = ADIOS_MIDI_DeviceIDGet();
			datas[TR5X6_FLASH_SYS_BC_CTRL_OFS]   = TR5X6_BC_CTRL_DEFAULT;
			datas[TR5X6_FLASH_SYS_BC_CHN_OFS]    = TR5X6_BC_CHN_DEFAULT;
			datas[TR5X6_FLASH_SYS_BC_OMNI_OFS]   = TR5X6_BC_OMNI_DEFAULT;
		}
		// Page erase
		LL_FLASH_Unlock();
		LL_FLASH_Status status;
		if( (status=LL_FLASH_PageErase(LL_FLASH_BANK_1, TR5X6_FLASH_PAGE_BASE + p)) != FLASH_COMPLETE ) {
			//LL_FLASH_ClearFlag(LL_FLASH_SR_CLEAR); // clear error flags, otherwise next program attempts will fail
			uint32_t error=LL_FLASH_GetError();
			LL_FLASH_Lock();
			sprintf(message, "erase failed for page#%d: code %d err 0x%08x\n", p, status, error);
			ADIOS_MIDI_SendDebugMessage(message);
			APP_LCD_Rectangle(90, 15+62, 300, 12, 0, 0, 2, APP_LCD_BLACK);
			APP_LCD_PrintFormattedString(92, 15+64, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "%s", message);
			return -1;
		}
		// data write
		// then write datas from RAM
		u32 addr=TR5X6_FLASH_START_ADDR + (TR5X6_FLASH_PAGE_SIZE*p);
		for(int i=0; i<TR5X6_FLASH_PAGE_SIZE; addr+=8, i+=8) {
			uint64_t buff64 =
					((uint64_t)datas[i+0] <<  0) |
					((uint64_t)datas[i+1] <<  8) |
					((uint64_t)datas[i+2] << 16) |
					((uint64_t)datas[i+3] << 24) |
					((uint64_t)datas[i+4] << 32) |
					((uint64_t)datas[i+5] << 40) |
					((uint64_t)datas[i+6] << 48) |
					((uint64_t)datas[i+7] << 56);
			if( (status=LL_FLASH_ProgramDoubleWord(addr, buff64)) != FLASH_COMPLETE ) {
				uint32_t error=LL_FLASH_GetError();
				LL_FLASH_Lock();
				sprintf(message, "write failed for 0x%08x: code %d err 0x%08x\n", addr, status, error);
				ADIOS_MIDI_SendDebugMessage(message);
				APP_LCD_Rectangle(90, 15+62, 300, 12, 0, 0, 2, APP_LCD_BLACK);
				APP_LCD_PrintFormattedString(92, 15+64, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "%s", message);
				return -2;
			}
			sprintf(message, "Page#%u addr@%08x formatted", p, addr);
			ADIOS_MIDI_SendDebugMessage("%s", message);
			if(((p*0x800)+i)<8183){
				APP_LCD_Rectangle(90, 15+132, 300, 12, 0, 0, 2, APP_LCD_BLACK);
				APP_LCD_PrintFormattedString(92, 15+134, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "%s", message);
			}else{
				APP_LCD_Rectangle(90, 15+132, 300, 12, 0, 0, 2, APP_LCD_BLACK);
				APP_LCD_PrintFormattedString(92, 15+134, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "%s", "All Pages formatted.");
				ADIOS_MIDI_SendDebugMessage("All Pages formatted.");
			}
			u16 progress= 100*((p*0x800)+i+8)/8192;
			memset(&prog_bmp_array, 0x00, sizeof(prog_bmp_array));

			APP_LCD_BitmapPrintFormattedString(prog_bmp, 1.0, 150, 2, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "%u%s", progress, "%");
			APP_LCD_PrintProgress(prog_bmp, 0x00ff0000, 90, 15+150, 300, 16, 300*progress/100);
		}


	}

	return 0;
}



/* ********* */
s32 TR5X6_FLASH_Bank_Write(tr5x6_flash_info_t slot){
#ifndef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
		//ADIOS_MIDI_SendDebugMessage("bank#%d slot#%d; %s and color 0x%04x\n", slot.bank, slot.slot, slot.name, slot.color);
#endif
	// determine the page and address range depending on host 505/626
	u8 page= slot.bank/tr5x6_unit->bank_divider;		// TR5X6_FLASH_PAGE_BASE
	u32 bank_addr = TR5X6_FLASH_BANK_INFO_OFFSET + ((slot.bank%tr5x6_unit->bank_divider)*TR5X6_FLASH_INFO_SIZE);
	// store page content to RAM
	for(int i=0;i<0x800;i++)
		datas[i]=(*((volatile u8*)(TR5X6_FLASH_START_ADDR + (TR5X6_FLASH_PAGE_SIZE*page)+i)));
	// Modify datas in RAM
	for(int i=0;i<TR5X6_FLASH_INFO_SIZE;i++)
		datas[bank_addr+i]=slot.ToWrite[i];
	// refresh the whole page by erasing it
	LL_FLASH_Unlock();
	LL_FLASH_Status status;
	if( (status=LL_FLASH_PageErase(LL_FLASH_BANK_1, TR5X6_FLASH_PAGE_BASE + page)) != FLASH_COMPLETE ) {
		//LL_FLASH_ClearFlag(LL_FLASH_SR_CLEAR); // clear error flags, otherwise next program attempts will fail
#ifndef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
		uint32_t error=LL_FLASH_GetError();
		ADIOS_MIDI_SendDebugMessage("erase failed for page#%d: code %d err 0x%08x\n", page, status, error);
#endif
		LL_FLASH_Lock();
		return -1;
	}
	// then write datas from RAM
	u32 addr=TR5X6_FLASH_START_ADDR + (TR5X6_FLASH_PAGE_SIZE*page);
	for(int i=0; i<TR5X6_FLASH_PAGE_SIZE; addr+=8, i+=8) {
		uint64_t buff64 =
				((uint64_t)datas[i+0] <<  0) |
				((uint64_t)datas[i+1] <<  8) |
				((uint64_t)datas[i+2] << 16) |
				((uint64_t)datas[i+3] << 24) |
				((uint64_t)datas[i+4] << 32) |
				((uint64_t)datas[i+5] << 40) |
				((uint64_t)datas[i+6] << 48) |
				((uint64_t)datas[i+7] << 56);
		if( (status=LL_FLASH_ProgramDoubleWord(addr, buff64)) != FLASH_COMPLETE ) {
#ifndef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
			uint32_t error=LL_FLASH_GetError();
			ADIOS_MIDI_SendDebugMessage("write failed for 0x%08x: code %d err 0x%08x\n", addr, status, error);
#endif
			LL_FLASH_Lock();
			return -2;
		}
	}
	return 0;
}


/* ********* */
s32 TR5X6_FLASH_BankRead(tr5x6_flash_info_t* slot){
	// determine the page and address range depending on host 505/626
	u8 page= slot->bank/tr5x6_unit->bank_divider;
	u32 bank_addr = TR5X6_FLASH_BANK_INFO_OFFSET + ((slot->bank%tr5x6_unit->bank_divider)*TR5X6_FLASH_INFO_SIZE);
	u32 addr=TR5X6_FLASH_START_ADDR + (TR5X6_FLASH_PAGE_SIZE*page) + bank_addr;
	for(int i=0;i<TR5X6_FLASH_INFO_SIZE;i++){
		slot->ToWrite[i]=(*((volatile u8*)(addr++)));
	}
	return 0;
}



/* ********* */
// Read-modify-write of the LAST page, the one carrying the system fields:
// copies it into datas[], lets the caller change what it wants, then erases
// and reprograms it. Everything the caller does not touch is inherited - which
// is how the device-ID record survives a bank change and vice versa.
// Split out of TR5X6_ROM_BankStore() when the device ID gained its own field,
// rather than duplicating forty lines of erase-and-program.
static s32 TR5X6_ROM_SysPageWrite(void);

static s32 TR5X6_ROM_SysPageRead(void){
	u8 page= 3;
	for(int i=0;i<0x800;i++)
		datas[i]=(*((volatile u8*)(TR5X6_FLASH_START_ADDR + (TR5X6_FLASH_PAGE_SIZE*page)+i)));
	return 0;
}

s32 TR5X6_ROM_DeviceIDStore(u8 device_id){
	TR5X6_ROM_SysPageRead();
	datas[TR5X6_FLASH_SYS_ID_CONFIRM_OFS]= 0x42;
	datas[TR5X6_FLASH_SYS_ID_OFS]        = device_id & 0x7f;
	return TR5X6_ROM_SysPageWrite();
}

s32 TR5X6_ROM_BankChangeStore(u8 ctrl, u8 chn, u8 omni){
	TR5X6_ROM_SysPageRead();
	datas[TR5X6_FLASH_SYS_BC_CTRL_OFS] = ctrl;
	datas[TR5X6_FLASH_SYS_BC_CHN_OFS]  = chn;
	datas[TR5X6_FLASH_SYS_BC_OMNI_OFS] = omni;
	return TR5X6_ROM_SysPageWrite();
}

// Anything that is not a legal value reads back as the default - 0xFF above
// all, which is what a board formatted before these three bytes existed has
// in there. That is the whole migration.
void TR5X6_ROM_BankChangeRecall(void){
	u32 base = TR5X6_FLASH_START_ADDR + (TR5X6_FLASH_PAGE_SIZE*3);
	u8 ctrl = (*((volatile u8*)(base + TR5X6_FLASH_SYS_BC_CTRL_OFS)));
	u8 chn  = (*((volatile u8*)(base + TR5X6_FLASH_SYS_BC_CHN_OFS)));
	u8 omni = (*((volatile u8*)(base + TR5X6_FLASH_SYS_BC_OMNI_OFS)));
	tr5x6_bc_ctrl = (ctrl < BC_CTRL_NUM) ? ctrl : TR5X6_BC_CTRL_DEFAULT;
	tr5x6_bc_chn  = (chn && chn <= 16)   ? chn  : TR5X6_BC_CHN_DEFAULT;
	tr5x6_bc_omni = (omni <= 1)          ? omni : TR5X6_BC_OMNI_DEFAULT;
}

s32 TR5X6_ROM_BankStore(u8 bank){
	TR5X6_ROM_SysPageRead();
	// Modify datas in RAM - everything else in the page, the device-ID record
	// included, is inherited from the copy above
	datas[TR5X6_FLASH_SYS_BANK_OFS]=bank;
	return TR5X6_ROM_SysPageWrite();
}

static s32 TR5X6_ROM_SysPageWrite(void){
	u8 page= 3;		// TR5X6_FLASH_PAGE_BASE
	// refresh the whole page by erasing it
	LL_FLASH_Unlock();
	LL_FLASH_Status status;
	if( (status=LL_FLASH_PageErase(LL_FLASH_BANK_1, TR5X6_FLASH_PAGE_BASE + page)) != FLASH_COMPLETE ) {
		//LL_FLASH_ClearFlag(LL_FLASH_SR_CLEAR); // clear error flags, otherwise next program attempts will fail
#ifndef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
		uint32_t error=LL_FLASH_GetError();
		ADIOS_MIDI_SendDebugMessage("erase failed for page#%d: code %d err 0x%08x\n", page, status, error);
#endif
		LL_FLASH_Lock();
		return -1;
	}
	// then write datas from RAM
	u32 addr=TR5X6_FLASH_START_ADDR + (TR5X6_FLASH_PAGE_SIZE*page);
	for(int i=0; i<TR5X6_FLASH_PAGE_SIZE; addr+=8, i+=8) {
		uint64_t buff64 =
				((uint64_t)datas[i+0] <<  0) |
				((uint64_t)datas[i+1] <<  8) |
				((uint64_t)datas[i+2] << 16) |
				((uint64_t)datas[i+3] << 24) |
				((uint64_t)datas[i+4] << 32) |
				((uint64_t)datas[i+5] << 40) |
				((uint64_t)datas[i+6] << 48) |
				((uint64_t)datas[i+7] << 56);
		if( (status=LL_FLASH_ProgramDoubleWord(addr, buff64)) != FLASH_COMPLETE ) {
#ifndef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
			uint32_t error=LL_FLASH_GetError();
			ADIOS_MIDI_SendDebugMessage("write failed for 0x%08x: code %d err 0x%08x\n", addr, status, error);
#endif
			LL_FLASH_Lock();
			return -2;
		}
	}
	return 0;
}

/* ********* */
u8 TR5X6_ROM_BankRecall(void){
	u8 page= 3;
	u32 addr=TR5X6_FLASH_START_ADDR + (TR5X6_FLASH_PAGE_SIZE*page) + TR5X6_FLASH_SYS_BANK_OFS;
	return (*((volatile u8*)(addr++)));
}




/* ********* */
s32 TR5X6_FLASH_Slot_Write(tr5x6_flash_info_t slot){
#ifndef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
		//ADIOS_MIDI_SendDebugMessage("bank#%d slot#%d; %s and color 0x%04x\n", slot.bank, slot.slot, slot.name, slot.color);
#endif
	// determine the page and address range depending on host 505/626
	u8 page= slot.bank/tr5x6_unit->bank_divider;		// TR5X6_FLASH_PAGE_BASE
	u32 slot_addr = ((slot.bank%tr5x6_unit->bank_divider)*tr5x6_unit->slot_num + slot.slot)*TR5X6_FLASH_INFO_SIZE;
	// store page content to RAM
	for(int i=0;i<0x800;i++)
		datas[i]=(*((volatile u8*)(TR5X6_FLASH_START_ADDR + (TR5X6_FLASH_PAGE_SIZE*page)+i)));
	// Modify datas in RAM
	for(int i=0;i<TR5X6_FLASH_INFO_SIZE;i++)
		datas[slot_addr+i]=slot.ToWrite[i];
	// refresh the whole page by erasing it
	LL_FLASH_Unlock();
	LL_FLASH_Status status;
	if( (status=LL_FLASH_PageErase(LL_FLASH_BANK_1, TR5X6_FLASH_PAGE_BASE + page)) != FLASH_COMPLETE ) {
		//LL_FLASH_ClearFlag(LL_FLASH_SR_CLEAR); // clear error flags, otherwise next program attempts will fail
#ifndef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
		uint32_t error=LL_FLASH_GetError();
		ADIOS_MIDI_SendDebugMessage("erase failed for page#%d: code %d err 0x%08x\n", page, status, error);
#endif
		LL_FLASH_Lock();
		return -1;
	}
	// then write datas from RAM
	u32 addr=TR5X6_FLASH_START_ADDR + (TR5X6_FLASH_PAGE_SIZE*page);
	for(int i=0; i<TR5X6_FLASH_PAGE_SIZE; addr+=8, i+=8) {
		uint64_t buff64 =
				((uint64_t)datas[i+0] <<  0) |
				((uint64_t)datas[i+1] <<  8) |
				((uint64_t)datas[i+2] << 16) |
				((uint64_t)datas[i+3] << 24) |
				((uint64_t)datas[i+4] << 32) |
				((uint64_t)datas[i+5] << 40) |
				((uint64_t)datas[i+6] << 48) |
				((uint64_t)datas[i+7] << 56);
		if( (status=LL_FLASH_ProgramDoubleWord(addr, buff64)) != FLASH_COMPLETE ) {
#ifndef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
			uint32_t error=LL_FLASH_GetError();
			ADIOS_MIDI_SendDebugMessage("write failed for 0x%08x: code %d err 0x%08x\n", addr, status, error);
#endif
			LL_FLASH_Lock();
			return -2;
		}
	}
	return 0;
}


/* ********* */
s32 TR5X6_FLASH_SlotRead(tr5x6_flash_info_t* slot){
	// determine the page and address range depending on host 505/626
	u8 page= slot->bank/tr5x6_unit->bank_divider;
	u32 slot_addr = ((slot->bank%tr5x6_unit->bank_divider)*tr5x6_unit->slot_num + slot->slot)*TR5X6_FLASH_INFO_SIZE;
	u32 addr=TR5X6_FLASH_START_ADDR + (TR5X6_FLASH_PAGE_SIZE*page) + slot_addr;
	for(int i=0;i<TR5X6_FLASH_INFO_SIZE;i++){
		slot->ToWrite[i]=(*((volatile u8*)(addr++)));
	}
	return 0;
}


#if 0
/* ********* */
s32 TR5X6_FLASH_MemBank_Write(u8 group, u8 pattern, tr5x6_flash_mem_Bank_t bank_mem){
#ifndef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
		//ADIOS_MIDI_SendDebugMessage("bank#%d slot#%d; %s and color 0x%04x\n", slot.bank, slot.slot, slot.name, slot.color);

#endif

	u8 page= 3;		// TR5X6_FLASH_PAGE_BASE
	u32 bank_addr = 0x780;
	// store page content to RAM
	for(int i=0;i<0x800;i++)
		datas[i]=(*((volatile u8*)(TR5X6_FLASH_START_ADDR + (TR5X6_FLASH_PAGE_SIZE*page)+i)));
	// Modify datas in RAM
	if(bank_mem.memorized)
		datas[bank_addr+(group*16)+pattern]=bank_mem.ALL;
	else
		datas[bank_addr+(group*16)+pattern]=0x00;
	// refresh the whole page by erasing it
	LL_FLASH_Unlock();
	LL_FLASH_Status status;
	if( (status=LL_FLASH_PageErase(LL_FLASH_BANK_1, TR5X6_FLASH_PAGE_BASE + page)) != FLASH_COMPLETE ) {
		//LL_FLASH_ClearFlag(LL_FLASH_SR_CLEAR); // clear error flags, otherwise next program attempts will fail
#ifndef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
		uint32_t error=LL_FLASH_GetError();
		ADIOS_MIDI_SendDebugMessage("erase failed for page#%d: code %d err 0x%08x\n", page, status, error);
#endif
		LL_FLASH_Lock();
		return -1;
	}
	// then write datas from RAM
	u32 addr=TR5X6_FLASH_START_ADDR + (TR5X6_FLASH_PAGE_SIZE*page);
	for(int i=0; i<TR5X6_FLASH_PAGE_SIZE; addr+=8, i+=8) {
		uint64_t buff64 =
				((uint64_t)datas[i+0] <<  0) |
				((uint64_t)datas[i+1] <<  8) |
				((uint64_t)datas[i+2] << 16) |
				((uint64_t)datas[i+3] << 24) |
				((uint64_t)datas[i+4] << 32) |
				((uint64_t)datas[i+5] << 40) |
				((uint64_t)datas[i+6] << 48) |
				((uint64_t)datas[i+7] << 56);
		if( (status=LL_FLASH_ProgramDoubleWord(addr, buff64)) != FLASH_COMPLETE ) {
#ifndef ADIOS_MIDI_DISABLE_DEBUG_MESSAGE
			uint32_t error=LL_FLASH_GetError();
			ADIOS_MIDI_SendDebugMessage("write failed for 0x%08x: code %d err 0x%08x\n", addr, status, error);
#endif
			LL_FLASH_Lock();
			return -2;
		}
	}
	return 0;
}

/* ********* */
tr5x6_flash_mem_Bank_t TR5X6_FLASH_MemBank_Get(u8 group, u8 pattern){
	// determine the page and address range depending on host 505/626
	u8 page= 3;		// TR5X6_FLASH_PAGE_BASE
	u32 bank_addr = 0x780;
	u32 addr=TR5X6_FLASH_START_ADDR + (TR5X6_FLASH_PAGE_SIZE*page)+bank_addr+(group*16)+pattern;
	tr5x6_flash_mem_Bank_t result;
	result.ALL = (*((volatile u8*)(addr)));
	return result;
}
#endif
