/*
 * TR5X6 debug terminal - a navigable command menu mirroring the settings menu.
 *
 * Reached from ADIOS Studio's Terminal (Debug). The ADIOS SysEx debug handler
 * delivers typed characters ONE AT A TIME to CONSOLE_Parse, which buffers a
 * line and dispatches it. Replies go back out the same port via
 * ADIOS_MIDI_SendDebugMessage - so the terminal rides the OS debug transport,
 * independent of the application's own TR5X6_ENABLE_DEBUG_MESSAGE diagnostics.
 *
 * Navigation mirrors the on-screen menu: device_id / format / reset are direct
 * commands at the top; bank_change is a sub-menu you enter and leave with exit.
 *
 * OPT-IN: the whole module is compiled only when TR5X6_TERMINAL_ENABLED is
 * defined in adios_config.h. For now every command is a STUB that only echoes
 * what it would do - this validates the parser and the text vocabulary before
 * the real reads/writes and the deferred persistence are wired in.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#include <adios.h>
#include <string.h>
#include "tr5x6_terminal.h"
#include "tr5x6_rom.h"       // tr5x6_unit / IS_626 - the live unit type

#ifdef TR5X6_TERMINAL_ENABLED

/////////////////////////////////////////////////////////////////////////////
// Local state
/////////////////////////////////////////////////////////////////////////////
#define CONSOLE_LINE_MAX 80
static char console_line[CONSOLE_LINE_MAX];
static u16  console_ix;

// which menu (or format-wizard step) the console currently sits in
enum { CONSOLE_TOP = 0, CONSOLE_BANK_CHANGE,
       CONSOLE_FORMAT_UNIT, CONSOLE_FORMAT_CHANGE, CONSOLE_FORMAT_CONFIRM };
static u8   console_level = CONSOLE_TOP;
static u8   format_unit;               // 5 or 6, chosen in the format wizard

// The wizard offers the unit-change confirmation only when the choice differs
// from the CURRENT unit, read live from tr5x6_unit (magic 0x75=505, 0x76=626).
#define FORMAT_CUR_UNIT (IS_626 ? 6 : 5)


/////////////////////////////////////////////////////////////////////////////
// Context-sensitive command list (help, and shown on entering a sub-menu)
/////////////////////////////////////////////////////////////////////////////
static void CONSOLE_Prompt(void)
{
	switch( console_level ) {
	case CONSOLE_BANK_CHANGE:
		ADIOS_MIDI_SendDebugMessage("bank_change: control|channel|omni|receive|transmit | exit\n"); break;
	case CONSOLE_FORMAT_UNIT:
		ADIOS_MIDI_SendDebugMessage("select your unit type: 505 | 626 | exit\n"); break;
	case CONSOLE_FORMAT_CHANGE:
		ADIOS_MIDI_SendDebugMessage("confirm unit change: yes | no | exit\n"); break;
	case CONSOLE_FORMAT_CONFIRM:
		ADIOS_MIDI_SendDebugMessage("confirm format: yes | no\n"); break;
	default:
		ADIOS_MIDI_SendDebugMessage("commands: device_id | bank_change | format | reset | help\n"); break;
	}
}


// The accepted values for a setting - shown on a bare name or a "<name> ?"
// query, so the valid inputs are always discoverable from the terminal itself.
static const char *CONSOLE_Choices(const char *field)
{
	if( !strcmp(field, "device_id") ) return "0-15";
	if( !strcmp(field, "control")   ) return "pc|cc0|cc32";
	if( !strcmp(field, "channel")   ) return "1-16";
	return "on|off";   // omni, receive, transmit
}


// Strict non-negative integer parse: -1 on empty or any non-digit, so a typo
// like "device_id foo" is rejected instead of silently being read as 0.
static int CONSOLE_Num(const char *s)
{
	int n = 0;
	if( s == NULL || *s == 0 )
		return -1;
	for( ; *s; ++s ) {
		if( *s < '0' || *s > '9' )
			return -1;
		n = n * 10 + (*s - '0');
		if( n > 999 )
			return -1;
	}
	return n;
}


/////////////////////////////////////////////////////////////////////////////
// Dispatch one complete typed line
/////////////////////////////////////////////////////////////////////////////
static void CONSOLE_ParseLine(char *line)
{
	// split into "word" + "arg": arg is the rest of the line ("" if none)
	char *arg = line;
	while( *arg && *arg != ' ' ) arg++;
	if( *arg == ' ' ) { *arg++ = 0; while( *arg == ' ' ) arg++; }

	// a bare name or "<name> ?" is a QUERY: show the value + the accepted range
	int query = (arg[0] == 0) || (arg[0] == '?' && arg[1] == 0);

	if( line[0] == 0 )
		return;

	// "?" or "help" anywhere -> recall the CURRENT level's commands
	if( !strcmp(line,"help") || !strcmp(line,"?") ) {
		CONSOLE_Prompt();
		return;
	}

	// "exit" anywhere -> back to the top page, and recall its commands
	if( !strcmp(line,"exit") ) {
		console_level = CONSOLE_TOP;
		CONSOLE_Prompt();
		return;
	}

	// ---- bank_change sub-menu ----
	if( console_level == CONSOLE_BANK_CHANGE ) {
		// work on a copy, commit as ONE 16-bit store (like the on-screen menu)
		// so the MIDI RX/TX side never sees a half-updated bitfield word; then
		// defer the flash/EEPROM store to TASK_ROM_Periodic (off the bus here).
		tr5x6_bc_t bc = tr5x6_bc;

		if( !strcmp(line, "control") ) {
			if( query ) {
				const char *v = (bc.ctrl==BC_CTRL_CC00) ? "cc0" :
				                (bc.ctrl==BC_CTRL_CC32) ? "cc32" : "pc";
				ADIOS_MIDI_SendDebugMessage("control = %s (%s)\n", v, CONSOLE_Choices("control"));
			} else if( !strcmp(arg,"pc") || !strcmp(arg,"cc0") || !strcmp(arg,"cc32") ) {
				bc.ctrl = !strcmp(arg,"pc") ? BC_CTRL_PC : !strcmp(arg,"cc0") ? BC_CTRL_CC00 : BC_CTRL_CC32;
				tr5x6_bc = bc; TR5X6_ROM_BankChangeStoreRequest();
				ADIOS_MIDI_SendDebugMessage("control = %s\n", arg);
			} else {
				ADIOS_MIDI_SendDebugMessage("control: %s\n", CONSOLE_Choices("control"));
			}
		} else if( !strcmp(line, "channel") ) {
			if( query ) {
				ADIOS_MIDI_SendDebugMessage("channel = %d (%s)\n", bc.chn+1, CONSOLE_Choices("channel"));
			} else {
				int n = CONSOLE_Num(arg);
				if( n < 1 || n > 16 ) {
					ADIOS_MIDI_SendDebugMessage("channel: %s\n", CONSOLE_Choices("channel"));
				} else {
					bc.chn = (n-1) & 0x0f;
					tr5x6_bc = bc; TR5X6_ROM_BankChangeStoreRequest();
					ADIOS_MIDI_SendDebugMessage("channel = %d\n", n);
				}
			}
		} else if( !strcmp(line,"omni") || !strcmp(line,"receive") || !strcmp(line,"transmit") ) {
			u8 cur = !strcmp(line,"omni") ? bc.omni : !strcmp(line,"receive") ? bc.rx_en : bc.tx_en;
			if( query ) {
				ADIOS_MIDI_SendDebugMessage("%s = %s (%s)\n", line, cur?"on":"off", CONSOLE_Choices(line));
			} else {
				int v = !strcmp(arg,"on") ? 1 : !strcmp(arg,"off") ? 0 : -1;
				if( v < 0 ) {
					ADIOS_MIDI_SendDebugMessage("%s: %s\n", line, CONSOLE_Choices(line));
				} else {
					if( !strcmp(line,"omni") )         bc.omni  = v;
					else if( !strcmp(line,"receive") ) bc.rx_en = v;
					else                               bc.tx_en = v;
					tr5x6_bc = bc; TR5X6_ROM_BankChangeStoreRequest();
					ADIOS_MIDI_SendDebugMessage("%s = %s\n", line, v?"on":"off");
				}
			}
		} else {
			ADIOS_MIDI_SendDebugMessage("unknown '%s' in bank_change - help | exit\n", line);
		}
		return;
	}

	// ---- format wizard: unit select -> (unit change confirm) -> format confirm
	if( console_level == CONSOLE_FORMAT_UNIT ) {
		if( !strcmp(line,"505") || !strcmp(line,"626") ) {
			format_unit = (line[0] == '6') ? 6 : 5;
			if( format_unit != FORMAT_CUR_UNIT ) {
				console_level = CONSOLE_FORMAT_CHANGE;
				ADIOS_MIDI_SendDebugMessage("unit type will switch to %d please confirm: yes | no | exit\n",
				                            (format_unit == 6) ? 626 : 505);
			} else {
				console_level = CONSOLE_FORMAT_CONFIRM;
				ADIOS_MIDI_SendDebugMessage("format erases all banks - confirm: yes | no\n");
			}
		} else {
			ADIOS_MIDI_SendDebugMessage("505 | 626 | exit\n");
		}
		return;
	}
	if( console_level == CONSOLE_FORMAT_CHANGE ) {
		if( !strcmp(line,"yes") ) {
			console_level = CONSOLE_FORMAT_CONFIRM;
			ADIOS_MIDI_SendDebugMessage("format erases all banks - confirm: yes | no\n");
		} else if( !strcmp(line,"no") ) {
			console_level = CONSOLE_FORMAT_UNIT;
			ADIOS_MIDI_SendDebugMessage("select your unit type: 505 | 626 | exit\n");
		} else {
			ADIOS_MIDI_SendDebugMessage("yes | no | exit\n");
		}
		return;
	}
	if( console_level == CONSOLE_FORMAT_CONFIRM ) {
		if( !strcmp(line,"yes") ) {
			console_level = CONSOLE_TOP;
			// arm the deferred format on the port this command arrived on;
			// TASK_ROM_Periodic runs it and streams the progress bars back.
			TR5X6_ROM_FormatRequest(format_unit, ADIOS_MIDI_DebugPortGet());
		} else if( !strcmp(line,"no") ) {
			console_level = CONSOLE_TOP;
			CONSOLE_Prompt();
		} else {
			ADIOS_MIDI_SendDebugMessage("yes | no | exit\n");
		}
		return;
	}

	// ---- top level ----
	if( strcmp(line, "device_id") == 0 ) {
		u8 cur = ADIOS_MIDI_DeviceIDGet() & 0x0f;
		if( query ) {
			ADIOS_MIDI_SendDebugMessage("device_id = %d (0-15)\n", cur);
		} else {
			int n = CONSOLE_Num(arg);
			if( n < 0 || n > 15 ) {
				ADIOS_MIDI_SendDebugMessage("device_id: 0-15\n");
			} else if( n == cur ) {
				ADIOS_MIDI_SendDebugMessage("device_id already %d\n", cur);
			} else {
				// confirm on the OLD id first (the host is still on it), THEN
				// switch the RAM id, THEN defer the flash store to
				// TASK_ROM_Periodic - never write flash from this callback
				// (the 25/08 wedge).
				ADIOS_MIDI_SendDebugMessage("device_id: %d -> %d\n", cur, n);
				ADIOS_MIDI_DeviceIDSet((u8)n);
				TR5X6_ROM_DeviceIDStoreRequest((u8)n);
			}
		}
	} else if( strcmp(line, "bank_change") == 0 ) {
		console_level = CONSOLE_BANK_CHANGE;
		CONSOLE_Prompt();
	} else if( strcmp(line, "format") == 0 ) {
		console_level = CONSOLE_FORMAT_UNIT;
		ADIOS_MIDI_SendDebugMessage("select your unit type: 505 | 626 | exit\n");
	} else if( strcmp(line, "reset") == 0 ) {
		ADIOS_MIDI_SendDebugMessage("would reboot (stub)\n");
	} else {
		ADIOS_MIDI_SendDebugMessage("unknown '%s' - type help\n", line);
	}
}


/////////////////////////////////////////////////////////////////////////////
// Called ONCE PER CHARACTER from the host terminal; buffers until newline
/////////////////////////////////////////////////////////////////////////////
static s32 CONSOLE_Parse(adios_midi_port_t port, char c)
{
	// reply on the same port the command arrived on (restored afterwards)
	adios_midi_port_t prev = ADIOS_MIDI_DebugPortGet();
	ADIOS_MIDI_DebugPortSet(port);

	if( c == '\r' ) {
		// ignore carriage returns
	} else if( c == '\n' ) {
		CONSOLE_ParseLine(console_line);
		console_ix = 0;
		console_line[0] = 0;
	} else if( console_ix < (CONSOLE_LINE_MAX - 1) ) {
		console_line[console_ix++] = c;
		console_line[console_ix] = 0;
	}

	ADIOS_MIDI_DebugPortSet(prev);
	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Public init
/////////////////////////////////////////////////////////////////////////////
void TR5X6_TERMINAL_Greeting(void)
{
	ADIOS_MIDI_SendDebugMessage("%s %s terminal ready...\n", ADIOS_APP_NAME1, ADIOS_APP_VERSION);
}

// A host that carries a terminal (ADIOS Studio) PULLS our greeting with the OS
// TERM_GREETING command (0x0c) - not the info query. We print greeting AND the
// top-level command list, and reset to the top menu so the host starts clean.
static s32 CONSOLE_OnGreeting(adios_midi_port_t port)
{
	adios_midi_port_t prev = ADIOS_MIDI_DebugPortGet();
	ADIOS_MIDI_DebugPortSet(port);
	console_level = CONSOLE_TOP;
	TR5X6_TERMINAL_Greeting();
	CONSOLE_Prompt();
	ADIOS_MIDI_DebugPortSet(prev);
	return 0;
}

void TR5X6_TERMINAL_Init(void)
{
	ADIOS_MIDI_DebugCommandCallback_Init(CONSOLE_Parse);
	ADIOS_MIDI_TermGreetingCallback_Init(CONSOLE_OnGreeting);   // greet on host TERM_GREETING (0x0c)
	console_ix = 0;
	console_line[0] = 0;
	console_level = CONSOLE_TOP;
	TR5X6_TERMINAL_Greeting();   // greet on boot, so the terminal shows it is alive
}

#else  // TR5X6_TERMINAL_ENABLED not defined

// Opt-out: no terminal, no code, no cost. The caller stays #ifdef-free.
void TR5X6_TERMINAL_Init(void) { }
void TR5X6_TERMINAL_Greeting(void) { }

#endif // TR5X6_TERMINAL_ENABLED
