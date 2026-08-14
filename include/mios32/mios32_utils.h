// $Id$
/*
 * Header file for MIOS32_UTILS - delay, timers, stopwatch, sign-of-life.
 *
 * ONE header for ONE implementation file. All four APIs below are realised
 * in mios32/<FAMILY>/mios32_utils.c, and always have been: what used to be
 * here was four separate headers - mios32_delay.h, mios32_timer.h,
 * mios32_stopwatch.h and mios32_sol.h - 139 lines carrying 13 prototypes
 * between four copyright blocks, four include guards and twelve empty
 * section banners. Merged 2026-08-13. Nothing included them directly;
 * mios32.h was their only reader.
 *
 * They are grouped here rather than merely stacked: all four are built on
 * the chip's timers, which is why they share an implementation file.
 *
 * (MIOS32_TIMESTAMP was considered for inclusion here and deliberately left
 * out: it is pure software - a u32 incremented by main.c's 1 mS tick - so
 * it has no business in a file whose reason to exist is the chip's timers,
 * and it keeps its own header and its own mios32/common/mios32_timestamp.c.)
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

#ifndef _MIOS32_UTILS_H
#define _MIOS32_UTILS_H

/////////////////////////////////////////////////////////////////////////////
// DELAY - busy waiting with a microsecond resolution
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_DELAY_Init(u32 mode);
extern s32 MIOS32_DELAY_Wait_uS(u16 uS);


/////////////////////////////////////////////////////////////////////////////
// TIMER - periodic interrupts for the application
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_TIMER_Init(u8 timer, u32 period, void (*_irq_handler)(void), u8 irq_priority);
extern s32 MIOS32_TIMER_ReInit(u8 timer, u32 period);
extern s32 MIOS32_TIMER_DeInit(u8 timer);


/////////////////////////////////////////////////////////////////////////////
// STOPWATCH - one-shot elapsed-time measurement
//
// The counter is 16 bit: at the 100 uS resolution it overflows after 6.55
// seconds, and ValueGet() then returns 0xffffffff for good until the
// stopwatch is restarted. Opt-in with MIOS32_USE_STOPWATCH; the timer it
// takes is STOPWATCH_TIMER_BASE (TIM17 on G0, TIM11 on F4), overridable.
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_STOPWATCH_Init(u32 resolution);
extern s32 MIOS32_STOPWATCH_Stop(void);
extern s32 MIOS32_STOPWATCH_Reset(void);
extern u32 MIOS32_STOPWATCH_ValueGet(void);


/////////////////////////////////////////////////////////////////////////////
// SOL - sign of life
//
// A single GPIO toggled as a heartbeat/liveness indicator - the lightweight,
// project-configurable replacement for the fixed on-board LED that
// mios32_board.c used to own. Opt-in with MIOS32_USE_SOL; the pin comes from
// MIOS32_SOL_PORT / MIOS32_SOL_PIN in the project's mios32_config.h, see
// mios32_utils.c for the default.
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_SOL_Init(void);
extern s32 MIOS32_SOL_Set(void);
extern s32 MIOS32_SOL_Clr(void);
extern s32 MIOS32_SOL_Tog(void);


#endif /* _MIOS32_UTILS_H */
