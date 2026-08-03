// Sign-of-life LED functions for MIOS32
//
// A single GPIO toggled as a heartbeat/liveness indicator - a lightweight,
// project-configurable replacement for mios32_board.c's fixed on-board LED.
// Pin is defined per-project in mios32_config.h (MIOS32_SOF_LED_PORT/
// MIOS32_SOF_LED_PIN) - see mios32_utils.c for the default.

#ifndef _MIOS32_SOF_H
#define _MIOS32_SOF_H

extern s32 MIOS32_SOF_LED_Init(void);
extern s32 MIOS32_SOF_LED_Set(void);
extern s32 MIOS32_SOF_LED_Clr(void);
extern s32 MIOS32_SOF_LED_Tog(void);

#endif /* _MIOS32_SOF_H */
