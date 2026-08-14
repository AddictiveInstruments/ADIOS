// $Id$
/*
 * Header file for the M16 - an FPGA interface deploying 16 MIDI I/O.
 *
 * The M16 is a BOARD, not a transport. It is reached over SPI-MIDI, but it
 * is not SPI-MIDI: this module holds everything that is specific to the
 * FPGA - its command set, its GPIO groups, its activity/overload status
 * reporting and its running-status control - while mios32_spi_midi stays a
 * transparent transport that knows nothing about what sits at the other end.
 *
 * Split out of mios32/common/mios32_spi_midi.c on 2026-08-14, where it used
 * to live behind fifteen MIOS32_SPI_MIDI_USE_M16 conditionals, one of them
 * 238 lines long - about a third of the transport. The function names are
 * kept EXACTLY as they were so that applications driving an M16 need no
 * change beyond including this header.
 *
 * How it attaches to the transport, now that no #ifdef weaves it in:
 *   - MIOS32_SPIM_M16_Init() does what the transport's Init used to do for
 *     it, and registers a raw-word callback.
 *   - that callback claims the words whose CIN is 0x01 - the M16's status
 *     channel - and lets everything else through to normal MIDI parsing.
 *
 * The application must therefore call MIOS32_SPIM_M16_Init() once, after
 * MIOS32_SPI_MIDI_Init(). Nothing else changes.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

#ifndef _M16_H
#define _M16_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// The M16 deploys 16 MIDI ports over one SPI link. A project using it wants
// the transport sized accordingly, in its own mios32_config.h:
//
//   #define MIOS32_USE_SPI_MIDI
//   #define MIOS32_USE_SPI2                     // the port the M16 sits on
//   #define MIOS32_SPI_MIDI_NUM_PORTS      16
//   #define MIOS32_SPI_MIDI_SPI            2
//   #define MIOS32_SPI_MIDI_SPI_PRESCALER  MIOS32_SPI_PRESCALER_8   // ~10 MBit/s
//   #define MIOS32_SPI_MIDI_SCAN_BUFFER_SIZE 48
//
// Those five values were the M16 branch of every #ifndef in
// mios32_spi_midi.h. They are settings, not identity: the transport keeps
// its generic defaults and the board declares what it needs.

/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////

typedef enum {
  M16_CMD_SPI_DEBUG     = 0x01, 	// Set SPI bus in Loop Mode
  M16_CMD_UART_DEBUG    = 0x02, 	// Set all UARTs in Loop Mode
  M16_CMD_RX_STAT       = 0x03,		// Enable UARTs RX Status reception
  M16_CMD_TX_STAT       = 0x04, 	// Enable UARTs TX Status reception
  M16_CMD_OVL_STAT      = 0x05, 	// Enable UARTs TX Overload Status reception
  M16_CMD_SOF_ENA       = 0x0f,		// Enable m16 Sign of life Led
  M16_CMD_TX_RS       	= 0x10,		// Enable UARTs MIDI Running Status
  M16_CMD_GPIO_BASE    	= 0xa0		// Command for GPIO value
} mios32_spim_m16_cmd_t;

typedef enum {
  M16_GPIO_MODE_RX_STAT     = 0x00,		// Group is MIDI RX Activity
  M16_GPIO_MODE_TX_STAT     = 0x01, 	// Group is MIDI TX Activity
  M16_GPIO_MODE_OVL_STAT    = 0x02, 	// Group is MIDI TX Overoad Activity
  M16_GPIO_MODE_OUT    		= 0x03,		// Group is General Purpose Out
  M16_GPIO_MODE_IN    		= 0x04,		// Group is General Purpose In
} mios32_spim_m16_gpio_mode_t;


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

// call once, after MIOS32_SPI_MIDI_Init()
extern s32 MIOS32_SPIM_M16_Init(u32 mode);

extern s32 MIOS32_SPIM_M16_StatCallback_Init(s32 (*callback_m16_stat)(mios32_spim_m16_cmd_t stat_cmd, u16 stat_val));

extern s32 MIOS32_SPIM_M16_RxStatEnable(u8 enable);
extern s32 MIOS32_SPIM_M16_TxStatEnable(u8 enable);
extern s32 MIOS32_SPIM_M16_OvlStatEnable(u8 enable);

extern s32 MIOS32_SPIM_M16_GPIO_Grp_ModeSet(u8 gpio_grp,mios32_spim_m16_gpio_mode_t mode);
extern mios32_spim_m16_gpio_mode_t MIOS32_SPIM_M16_GPIO_Grp_ModeGet(u8 gpio_grp);
extern s32 MIOS32_SPIM_M16_GPIO_Grp_InvSet(u8 gpio_grp,u16 value);
extern s32 MIOS32_SPIM_M16_GPIO_Grp_InvGet(u8 gpio_grp);
extern s32 MIOS32_SPIM_M16_GPIO_Grp_Set(u8 gpio_grp,u16 value);
extern s32 MIOS32_SPIM_M16_GPIO_Grp_Get(u8 gpio_grp);
extern s32 MIOS32_SPIM_M16_GPIO_InvSet(u8 gpio,u8 value);
extern s32 MIOS32_SPIM_M16_GPIO_InvGet(u8 gpio);
extern s32 MIOS32_SPIM_M16_GPIO_Set(u8 gpio,u8 value);
extern s32 MIOS32_SPIM_M16_GPIO_Get(u8 gpio);

extern s32 MIOS32_SPIM_M16_SofEnable(u8 enable);

// Running status optimisation. It lived in the transport as
// MIOS32_SPI_MIDI_RS_OptimisationSet/Get, but its whole body was an M16
// command carrying a 16-bit port mask - so it belongs here. The generic
// MIOS32_MIDI_RS_OptimisationSet() now answers -1 for the SPIM port range,
// exactly as it already did for CAN: the transport does not implement it,
// the board does.
extern s32 MIOS32_SPIM_M16_RS_OptimisationSet(u8 spim_port, u8 enable);
extern s32 MIOS32_SPIM_M16_RS_OptimisationGet(u8 spim_port);

#endif /* _M16_H */
