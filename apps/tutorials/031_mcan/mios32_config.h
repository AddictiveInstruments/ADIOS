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
#define MIOS32_LCD_BOOT_MSG_LINE1 "Tutorial #031"
#define MIOS32_LCD_BOOT_MSG_LINE2 "(C) 2020 TK/Antichambre"

// function used to output debug messages (must be printf compatible!)
#define DEBUG_MSG MIOS32_MIDI_SendDebugMessage
#define APP_MSG_VERBOSE 0

/* the use of MCAN must be precised. */
#define MIOS32_USE_CAN

/* CAN1 is MCAN must be precised. */
#define MIOS32_USE_CAN_MIDI

/* User input for node Id number */
#define MIOS32_CAN_MIDI_NODE_ID 0x11

/* Number MCAN MIDI Ports, default is 16 */
#define MIOS32_CAN_MIDI_NUM_PORTS 16

/* - Enhanced mode allowed (send/receive extended message)
 if necessary (if needed and requested by an other node).
 Note: Message type and cable filter is used in this mode.
 - If not precised, MCAN is in Basic Mode(not able to send/receive extended message) */
//#define MIOS32_CAN_MIDI_ENHANCED


#define MIOS32_DONT_USE_UART
#define MIOS32_DONT_USE_IIC

#define MIOS32_DONT_USE_USB_HOST
#define MIOS32_DONT_USE_USB_HS_HOST


/* Number USB MIDI Ports, default is 1 */
#define MIOS32_USB_MIDI_NUM_PORTS 8

// change MCAN verbose level here, default is 0
// note: can be changed in terminal
#define MIOS32_CAN_MIDI_VERBOSE_LEVEL 1



#endif /* _MIOS32_CONFIG_H */
