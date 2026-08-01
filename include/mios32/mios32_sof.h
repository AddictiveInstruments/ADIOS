// Sign-of-life LED functions for MIOS32 (2026-08-01)
//
// A single GPIO toggled as a heartbeat/liveness indicator - a lightweight,
// project-configurable replacement for mios32_board.c's fixed on-board LED
// (mios32_board.c is being phased out of common in favor of project-specific
// hardware code). Pin is defined per-project in mios32_config.h
// (MIOS32_SOF_LED_PORT/MIOS32_SOF_LED_PIN), with sensible defaults for known
// dev boards - see mios32_utils.c.

#ifndef _MIOS32_SOF_H
#define _MIOS32_SOF_H

extern s32 MIOS32_SOF_LED_Init(void);
extern s32 MIOS32_SOF_LED_Set(void);
extern s32 MIOS32_SOF_LED_Clr(void);
extern s32 MIOS32_SOF_LED_Tog(void);

#endif /* _MIOS32_SOF_H */
