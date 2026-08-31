/*
 * Header of the TR5X6 debug terminal.
 *
 * A navigable command menu, reached from ADIOS Studio's Terminal (Debug), that
 * mirrors the on-screen settings menu (device_id, bank_change, format, reset).
 *
 * OPT-IN: the whole thing is compiled only when TR5X6_TERMINAL_ENABLED is
 * defined in adios_config.h. When it is not, TR5X6_TERMINAL_Init() is an empty
 * stub, so the caller (APP_Init) needs no #ifdef of its own.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _TR5X6_TERMINAL_H
#define _TR5X6_TERMINAL_H

// Registers the terminal on the ADIOS debug-command channel. Call once, on the
// normal road, after the MIDI callbacks are installed. No-op unless
// TR5X6_TERMINAL_ENABLED is defined.
extern void TR5X6_TERMINAL_Init(void);

// Prints the boot greeting ("<app> <ver> ready - type help"). Called at init,
// and can be called again (e.g. on a ping) to re-greet a connecting host.
// No-op unless TR5X6_TERMINAL_ENABLED.
extern void TR5X6_TERMINAL_Greeting(void);

#endif /* _TR5X6_TERMINAL_H */
