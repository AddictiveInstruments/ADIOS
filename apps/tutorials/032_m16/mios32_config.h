// $Id: mios32_config.h 502 2009-05-09 14:20:30Z tk $
/*
 * Local MIOS32 configuration file
 *
 * this file allows to disable (or re-configure) default functions of MIOS32
 * available switches are listed in $MIOS32_PATH/modules/mios32/MIOS32_CONFIG.txt
 *
 */

#ifndef _MIOS32_CONFIG_H
#define _MIOS32_CONFIG_H

// The boot message which is print during startup and returned on a SysEx query
#define MIOS32_LCD_BOOT_MSG_LINE1 "Tutorial #032"
#define MIOS32_LCD_BOOT_MSG_LINE2 "(C) 2020 TK/Antichambre"

// function used to output debug messages (must be printf compatible!)
#define DEBUG_MSG MIOS32_MIDI_SendDebugMessage

// UARTs and IIC MIDI are not necessary
#define MIOS32_DONT_USE_UART
#define MIOS32_DONT_USE_UART_MIDI
#define MIOS32_DONT_USE_IIC_MIDI
#define MIOS32_DONT_USE_ENC28J60

// SPI MIDI is for EXPANDER
// note: spi_midi must be enabled!
// syntax: set spi_midi 1 then store
// using the bootloader updater firmware.
#define MIOS32_SPI_MIDI_USE_M16
// SPI port and cs selection
//#define MIOS32_SPI_MIDI_SPI 0
#define MIOS32_SPI_MIDI_SPI_RC_PIN 0
// how many SPI MIDI ports are available?
// if 0: interface disabled (default)
// other allowed values: 1..16
#define MIOS32_SPI_MIDI_NUM_PORTS 16




#endif /* _MIOS32_CONFIG_H */
