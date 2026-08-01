// $Id: app.h 1919 2014-01-08 19:13:48Z tk $
/*
 * Header file of application
 *
 * ==========================================================================
 *
 *  Copyright (C) <year> <your name> (<your email address>)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
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
typedef struct {
  char	name[17];
  u8    src_id;
  u8    src_port; // don't use mios32_midi_port_t, since data width is important for save/restore function
  //u8    src_chn;  // 0 == Off, 1..16: specific source channel, 17 == All
} midi_router_src_t;
typedef struct {
  char	name[17];
  u8    dst_id;
  u8    dst_port;
  //u8    dst_chn;  // 0 == Off, 1..16: specific source channel, 17 == All
} midi_router_dst_t;
/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern void APP_Init(void);
extern void APP_Background(void);
extern void APP_Tick(void);
extern void APP_MIDI_Tick(void);
extern void APP_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package);
extern void APP_SRIO_ServicePrepare(void);
extern void APP_SRIO_ServiceFinish(void);
extern void APP_DIN_NotifyToggle(u32 pin, u32 pin_value);
extern void APP_ENC_NotifyChange(u32 encoder, s32 incrementer);
extern void APP_AIN_NotifyChange(u32 pin, u32 pin_value);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////
extern u32 can_rx_ctr, can_tx_ctr;

#endif /* _APP_H */
