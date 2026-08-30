/*
 * terminal_test - a minimal ADIOS debug-terminal demo.
 *
 * Talk to it from ADIOS Studio's "Terminal (Debug)" (or MIOS Studio's MIOS
 * Terminal): type a command, press Enter, the core answers with DEBUG_MSG.
 * It shows the whole round trip - host text command -> core -> debug reply.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <adios.h>
#include <string.h>
#include "app.h"

// DEBUG_MSG is not a global macro in ADIOS - each module aliases it locally.
#define DEBUG_MSG ADIOS_MIDI_SendDebugMessage


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

#define LINE_MAX 80
static char line_buffer[LINE_MAX];
static u16  line_ix;


/////////////////////////////////////////////////////////////////////////////
// Local prototypes
/////////////////////////////////////////////////////////////////////////////

static s32  CONSOLE_Parse(adios_midi_port_t port, char c);
static void CONSOLE_ParseLine(char *line);


/////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////
void APP_Init(void)
{
  ADIOS_SOL_Init();

  // Route typed terminal characters (delivered one at a time by the ADIOS
  // SysEx debug handler) to our line assembler.
  ADIOS_MIDI_DebugCommandCallback_Init(CONSOLE_Parse);
  line_ix = 0;
  line_buffer[0] = 0;

  DEBUG_MSG("%s %s ready - type 'help'\n", ADIOS_APP_NAME1, ADIOS_APP_VERSION);
}


/////////////////////////////////////////////////////////////////////////////
// Terminal: called ONCE PER CHARACTER received from the host terminal.
// We buffer until a newline, then dispatch the whole line.
/////////////////////////////////////////////////////////////////////////////
static s32 CONSOLE_Parse(adios_midi_port_t port, char c)
{
  // Reply on the same port the command arrived on (restored afterwards).
  adios_midi_port_t prev = ADIOS_MIDI_DebugPortGet();
  ADIOS_MIDI_DebugPortSet(port);

  if( c == '\r' ) {
    // ignore carriage returns
  } else if( c == '\n' ) {
    CONSOLE_ParseLine(line_buffer);
    line_ix = 0;
    line_buffer[0] = 0;
  } else if( line_ix < (LINE_MAX - 1) ) {
    line_buffer[line_ix++] = c;
    line_buffer[line_ix] = 0;
  }

  ADIOS_MIDI_DebugPortSet(prev);
  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Command dispatcher - one complete line at a time.
/////////////////////////////////////////////////////////////////////////////
static void CONSOLE_ParseLine(char *line)
{
  if( line[0] == 0 ) {
    // empty line, nothing to do
  } else if( strcmp(line, "ping") == 0 ) {
    DEBUG_MSG("pong\n");
  } else if( strcmp(line, "hello") == 0 ) {
    DEBUG_MSG("Hello from %s %s!\n", ADIOS_APP_NAME1, ADIOS_APP_VERSION);
  } else if( strcmp(line, "uptime") == 0 ) {
    DEBUG_MSG("uptime: %d ms\n", (int)ADIOS_TIMESTAMP_Get());
  } else if( strcmp(line, "help") == 0 ) {
    DEBUG_MSG("commands: ping | hello | uptime | echo <text> | help\n");
  } else if( strncmp(line, "echo ", 5) == 0 ) {
    DEBUG_MSG("%s\n", line + 5);
  } else {
    DEBUG_MSG("unknown command '%s' - type 'help'\n", line);
  }
}


/////////////////////////////////////////////////////////////////////////////
// Background / periodic hooks
/////////////////////////////////////////////////////////////////////////////
void APP_Background(void)
{
}

void APP_Tick(void)
{
  // sign-of-life blink
  u32 timestamp = ADIOS_TIMESTAMP_Get();
  if( ((timestamp % 20) <= ((timestamp / 100) % 10)) & 1 )
    ADIOS_SOL_Set();
  else
    ADIOS_SOL_Clr();
}

void APP_MIDI_Tick(void)
{
}

void APP_MIDI_NotifyPackage(adios_midi_port_t port, adios_midi_package_t midi_package)
{
}

void APP_SRIO_ServicePrepare(void)
{
}

void APP_SRIO_ServiceFinish(void)
{
}

void APP_SRIN_NotifyToggle(u32 pin, u32 pin_value)
{
}

void APP_ENC_NotifyChange(u32 encoder, s32 incrementer)
{
}

void APP_ADC_NotifyChange(u32 port, u32 chn, u32 value)
{
}
