// Sign-of-life LED functions for MIOS32
//
// A single GPIO toggled as a heartbeat/liveness indicator - a lightweight,
// project-configurable replacement for mios32_board.c's fixed on-board LED.
// Pin is defined per-project in mios32_config.h (MIOS32_SOL_PORT/
// MIOS32_SOL_PIN) - see mios32_utils.c for the default.

#ifndef _MIOS32_SOL_H
#define _MIOS32_SOL_H

extern s32 MIOS32_SOL_Init(void);
extern s32 MIOS32_SOL_Set(void);
extern s32 MIOS32_SOL_Clr(void);
extern s32 MIOS32_SOL_Tog(void);

#endif /* _MIOS32_SOL_H */
