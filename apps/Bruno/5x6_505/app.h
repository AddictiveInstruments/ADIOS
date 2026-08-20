/*
 * Header file of application
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _APP_H
#define _APP_H


/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern void APP_Init(void);
extern void APP_Background(void);
extern void APP_Tick(void);
extern void APP_MIDI_Tick(void);
extern void APP_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package);
extern s32 APP_SYSEX_Parser(mios32_midi_port_t port, u8 midi_in);
extern void APP_SRIO_ServicePrepare(void);
extern void APP_SRIO_ServiceFinish(void);
extern void APP_SRIN_NotifyToggle(u32 pin, u32 pin_value);
extern void APP_ENC_NotifyChange(u32 encoder, s32 incrementer);
extern void APP_ADC_NotifyChange(u32 port, u32 chn, u32 value);

// SPI serialization (mutex owned by app.c - see the comment there): MUST wrap
// every SPI access sequence performed outside TASK_TFT_Periodic/-_SettingsMenu/
// -_ROM_Periodic (which take it themselves), e.g. the direct flash/ROM
// accesses in the SysEx handlers running in the MIDI task.
// Never call from an ISR (the decod capture ISRs don't touch SPI - they
// don't need it).
extern void APP_SPI_MutexTake(void);
extern void APP_SPI_MutexGive(void);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////


#endif /* _APP_H */
