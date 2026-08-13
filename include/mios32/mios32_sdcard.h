// $Id: mios32_sdcard.h 964 2010-03-11 23:59:32Z philetaylor $
/*
 * Header file for MMC/SD Card Driver
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _MIOS32_SDCARD_H
#define _MIOS32_SDCARD_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// An SD card is not a peripheral of the chip: it is a device at the end of an
// SPI bus, and this driver only knows how to move 512-byte sectors over it.
// Everything file-shaped lives above, in modules/ - fatfs (ChaN's FatFs) and
// dosfs bind to the four entry points SectorRead/SectorWrite/CheckAvailable/
// CSDRead, modules/file adds the MIDIbox layer that MIOS Studio's file
// browser talks to over SysEx.
//
// Opt-in since 2026-08-13: declare MIOS32_USE_SDCARD in your project's
// mios32_config.h, together with the SPI port below.

// Which SPI port carries the card. The port itself must be declared too
// (MIOS32_USE_SPI0 / MIOS32_USE_SPI1) - see the #error in mios32_sdcard.c,
// which says so at compile time rather than letting the link fail on four
// missing MIOS32_SPI_* symbols.
#ifndef MIOS32_SDCARD_SPI
#define MIOS32_SDCARD_SPI 0
#endif

// (MIOS32_SDCARD_SPI_RC_PIN was defined here and used NOWHERE - a leftover
// from the MBHP boards, where one SPI port exposed two chip select lines,
// J16:RC1 and J16:RC2. A port now has a single CS under manual GPIO control,
// named by MIOS32_SPIn_CS_PORT/_PIN in the family driver, and this driver
// drives it through MIOS32_SPI_CS_PinSet(port, level). Removed 2026-08-13.)


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////

// structure taken from Mass Storage Driver example provided by STM
typedef struct
{
  u8  CSDStruct;            /* CSD structure */
  u8  SysSpecVersion;       /* System specification version */
  u8  Reserved1;            /* Reserved */
  u8  TAAC;                 /* Data read access-time 1 */
  u8  NSAC;                 /* Data read access-time 2 in CLK cycles */
  u8  MaxBusClkFrec;        /* Max. bus clock frequency */
  u16 CardComdClasses;      /* Card command classes */
  u8  RdBlockLen;           /* Max. read data block length */
  u8  PartBlockRead;        /* Partial blocks for read allowed */
  u8  WrBlockMisalign;      /* Write block misalignment */
  u8  RdBlockMisalign;      /* Read block misalignment */
  u8  DSRImpl;              /* DSR implemented */
  u8  Reserved2;            /* Reserved */
  u32 DeviceSize;           /* Device Size */
  u8  MaxRdCurrentVDDMin;   /* Max. read current @ VDD min */
  u8  MaxRdCurrentVDDMax;   /* Max. read current @ VDD max */
  u8  MaxWrCurrentVDDMin;   /* Max. write current @ VDD min */
  u8  MaxWrCurrentVDDMax;   /* Max. write current @ VDD max */
  u8  DeviceSizeMul;        /* Device size multiplier */
  u8  EraseGrSize;          /* Erase group size */
  u8  EraseGrMul;           /* Erase group size multiplier */
  u8  WrProtectGrSize;      /* Write protect group size */
  u8  WrProtectGrEnable;    /* Write protect group enable */
  u8  ManDeflECC;           /* Manufacturer default ECC */
  u8  WrSpeedFact;          /* Write speed factor */
  u8  MaxWrBlockLen;        /* Max. write data block length */
  u8  WriteBlockPaPartial;  /* Partial blocks for write allowed */
  u8  Reserved3;            /* Reserded */
  u8  ContentProtectAppli;  /* Content protection application */
  u8  FileFormatGrouop;     /* File format group */
  u8  CopyFlag;             /* Copy flag (OTP) */
  u8  PermWrProtect;        /* Permanent write protection */
  u8  TempWrProtect;        /* Temporary write protection */
  u8  FileFormat;           /* File Format */
  u8  ECC;                  /* ECC code */
  u8  msd_CRC;                  /* CRC */
  u8  Reserved4;            /* always 1*/
} mios32_sdcard_csd_t;

// structure taken from Mass Storage Driver example provided by STM
typedef struct
{
  u8  ManufacturerID;       /* ManufacturerID */
  u16 OEM_AppliID;          /* OEM/Application ID */
  char ProdName[6];         /* Product Name */
  u8  ProdRev;              /* Product Revision */
  u32 ProdSN;               /* Product Serial Number */
  u8  Reserved1;            /* Reserved1 */
  u16 ManufactDate;         /* Manufacturing Date */
  u8  msd_CRC;              /* CRC */
  u8  Reserved2;            /* always 1*/
} mios32_sdcard_cid_t;


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_SDCARD_Init(u32 mode);

extern s32 MIOS32_SDCARD_PowerOn(void);
extern s32 MIOS32_SDCARD_PowerOff(void);
extern s32 MIOS32_SDCARD_CheckAvailable(u8 was_available);

extern s32 MIOS32_SDCARD_SendSDCCmd(u8 cmd, u32 addr, u8 crc);
extern s32 MIOS32_SDCARD_SectorRead(u32 sector, u8 *buffer);
extern s32 MIOS32_SDCARD_SectorWrite(u32 sector, u8 *buffer);

extern s32 MIOS32_SDCARD_CIDRead(mios32_sdcard_cid_t *cid);
extern s32 MIOS32_SDCARD_CSDRead(mios32_sdcard_csd_t *csd);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#endif /* _MIOS32_SDCARD_H */
