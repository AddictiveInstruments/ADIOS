// $Id$
/*
 * Local MIOS32 configuration file - 5x6 ROM one-shot migration tool
 *
 * ==========================================================================
 */

#ifndef _MIOS32_CONFIG_H
#define _MIOS32_CONFIG_H

// The names MIOS Studio shows in its query answer. Unambiguous on purpose:
// this thing is loaded on top of the instrument's firmware for exactly one
// boot, and you want to be able to tell at a glance that it is what is running.
#define MIOS32_LCD_BOOT_MSG_LINE1 "5x6 ROM migration"
#define MIOS32_LCD_BOOT_MSG_LINE2 "(c) 2026 B.Dupeyron"

// Everything this tool does is read flash, write flash, and say so over MIDI.
// So: no shift registers, no encoders, no analog, no motorfaders, no display,
// no OSC/COM, no SD card, no I2C - the same opt-outs 5x6_505 makes, for the
// same reason, plus the display it genuinely has and this does not.
#define MIOS32_DONT_USE_SRIO
#define MIOS32_DONT_USE_SRIN
#define MIOS32_DONT_USE_SROUT
#define MIOS32_DONT_USE_ENC
#define MIOS32_DONT_USE_AIN
#define MIOS32_DONT_USE_MF
#define MIOS32_DONT_USE_LCD
#define MIOS32_DONT_USE_OSC
#define MIOS32_DONT_USE_COM
#define MIOS32_DONT_USE_USB_HOST
#define MIOS32_DONT_USE_USB_HS_HOST

// The G070CB has no USB peripheral at all, and mios32/mios32.mk compiles no USB
// source for the G0 family - so main.c's MIOS32_USB_Init() call has to go, or
// the link fails on a function that does not exist. Not an optimisation: a
// requirement of the chip.
#define MIOS32_DONT_USE_USB
#define MIOS32_DONT_USE_USB_MIDI

// the RTC is only wound up for the bootloader-request backup register, which
// this tool never touches
#define MIOS32_SYS_DONT_INIT_RTC

// used by APP_Init's short settling delay before it reports
#define MIOS32_USE_STOPWATCH

// FreeRTOS is not needed either - the whole job happens in APP_Init()
#define APP_USE_FREERTOS 0

// The instrument's MIDI wiring, identical to 5x6_505's - this tool has to come
// up on the same physical connector as the firmware it is loaded beside.
// Relayed to the bootloader/updater builds by etc/gen_bsl_boundary.sh, exactly
// as in the application: those builds refuse to compile without it.
// BSL_RELAY_BEGIN
#define MIOS32_USE_DIN_MIDI
#define MIOS32_USE_UART2
#define MIOS32_MIDI_DEFAULT_PORT DIN2
#define MIOS32_MIDI_DEBUG_PORT DIN2
// UART2 (USART3) TX runs through an external 3V3->5V transistor stage that
// inverts the signal; every port defaults to normal polarity in the driver.
#define MIOS32_UART2_TX_INVERTED
// ...and that stage must be DRIVEN, so push-pull - see 5x6_505's own config
// for the full story of how this one bit us.
#define MIOS32_UART2_TX_OD 0
// BSL_RELAY_END

#endif /* _MIOS32_CONFIG_H */
