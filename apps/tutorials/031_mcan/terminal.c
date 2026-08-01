// $Id: terminal.c 1824 2013-09-15 16:50:28Z tk $
/*
 * The command/configuration Terminal
 *
 * ==========================================================================
 *
 *  Copyright (C) 2010 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>
#include <string.h>

#include "app.h"
#include "terminal.h"



/////////////////////////////////////////////////////////////////////////////
// Local defines
/////////////////////////////////////////////////////////////////////////////

#define STRING_MAX 100 // recommended size for file transfers via FILE_BrowserHandler()

const char err_state_str[8][32] ={
{"Last Err: No Error            "},
{"Last Err: Stuff Error         "},
{"Last Err: Form Error          "},
{"Last Err: Acknowledgment Error"},
{"Last Err: Bit Recessive Error "},
{"Last Err: Bit Dominant Error  "},
{"Last Err: CRC Error           "},
{"Last Err: Software Set Error  "}
};

const char bus_state_str[4][32] ={
  {"Bus Off!!!"},
  {"Is Passive"},
  {"In Warning"},
  {"Bus is Ok."}
};

/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static char line_buffer[STRING_MAX];
static u16 line_ix;


/////////////////////////////////////////////////////////////////////////////
// Local prototypes
/////////////////////////////////////////////////////////////////////////////
static s32 get_dec(char *word);

/////////////////////////////////////////////////////////////////////////////
// Initialisation
/////////////////////////////////////////////////////////////////////////////
s32 TERMINAL_Init(u32 mode)
{
  // install the callback function which is called on incoming characters from MIOS Terminal
  MIOS32_MIDI_DebugCommandCallback_Init(TERMINAL_Parse);

  // clear line buffer
  line_buffer[0] = 0;
  line_ix = 0;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Parser
/////////////////////////////////////////////////////////////////////////////
s32 TERMINAL_Parse(mios32_midi_port_t port, char byte)
{
  // temporary change debug port (will be restored at the end of this function)
  mios32_midi_port_t prev_debug_port = MIOS32_MIDI_DebugPortGet();
  MIOS32_MIDI_DebugPortSet(port);

  if( byte == '\r' ) {
    // ignore
  } else if( byte == '\n' ) {
    //MUTEX_MIDIOUT_TAKE;
    TERMINAL_ParseLine(line_buffer, MIOS32_MIDI_SendDebugMessage);
    //MUTEX_MIDIOUT_GIVE;
    line_ix = 0;
    line_buffer[line_ix] = 0;
  } else if( line_ix < (STRING_MAX-1) ) {
    line_buffer[line_ix++] = byte;
    line_buffer[line_ix] = 0;
  }

  // restore debug port
  MIOS32_MIDI_DebugPortSet(prev_debug_port);

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Parser for a complete line - also used by shell.c for telnet
/////////////////////////////////////////////////////////////////////////////
s32 TERMINAL_ParseLine(char *input, void *_output_function)
{
  void (*out)(char *format, ...) = _output_function;
  char *separators = " \t";
  char *brkt;
  char *parameter;
  
  // since strtok_r works destructive (separators in *input replaced by NUL), we have to restore them
  // on an unsuccessful call (whenever this function returns < 1)
  int input_len = strlen(input);
  
  if( (parameter = strtok_r(input, separators, &brkt)) ) {
    
    if( strcasecmp(parameter, "mcan") == 0 ) {
      //MCAN_TerminalPrintStatus(_output_function);
      return 1; // command taken
    } else if( strcasecmp(parameter, "mcan_reconnect") == 0 ) {
      MIOS32_CAN_Init(0);
      //out("Scanning for MCAN nodes...");
      return 1; // command taken
    } else if( strcasecmp(parameter, "mcan_report") == 0 ) {
      TERMINAL_PrintReport(_output_function);
      //out("Scanning for MCAN nodes...");
      return 1; // command taken
    } else if( strcasecmp(parameter, "mcan_last_err") == 0 ) {
      TERMINAL_PrintReport(_output_function);
      //out("Scanning for MCAN nodes...");
      return 1; // command taken
    } else if( strcasecmp(parameter, "mcan_report_reset") == 0 ) {
      MIOS32_CAN_ReportReset(0);
      MIOS32_CAN_ReportReset(1);
      out("All Report informations were resetted.");
      return 1; // command taken
    } else if( strcasecmp(parameter, "set") == 0 ) {
      if( !(parameter = strtok_r(NULL, separators, &brkt)) ) {
        out("Missing parameter after 'set'!");
        return 1; // command taken
      }
      
      if( strcasecmp(parameter, "mcan_id") == 0 ) {
        s32 value = -1;
        if( (parameter = strtok_r(NULL, separators, &brkt)) )
          value = get_dec(parameter);
        
        if( value < 0x00 || value > 0x7f ) {
          out("Expecting value between 0x00..0x7f!");
          return 1; // command taken
        }
        
        if( MIOS32_CAN_MIDI_NodeIDSet(value) >= 0 ) {
          out("MCAN ID changed to 0x%02x.", MIOS32_CAN_MIDI_NodeIDGet());
        } else {
          out("Failed to change ID!");
        }
        return 1; // command taken
        
      } else if( strcasecmp(parameter, "mcan_verbose") == 0 ) {
        s32 value = -1;
        if( (parameter = strtok_r(NULL, separators, &brkt)) )
          value = get_dec(parameter);
        
        if( value < 0 || value > 3 ) {
          out("Expecting value between 0..3!");
          return 1; // command taken
        }
        
        if( MIOS32_CAN_MIDI_VerboseSet(value) >= 0 ) {
          out("MCAN Verbose Level set to %d.", MIOS32_CAN_MIDI_VerboseGet());
        } else {
          out("Failed to change verbose level!");
        }
        return 1; // command taken
        
      } else {
        // out("Unknown command - type 'help' to list available commands!");
      }
    }
  }
  
  // restore input line (replace NUL characters by spaces)
  int i;
  char *input_ptr = input;
  for(i=0; i<input_len; ++i, ++input_ptr)
    if( !*input_ptr )
      *input_ptr = ' ';
  
  if( (parameter = strtok_r(input, separators, &brkt)) ) {
    if( strcmp(parameter, "help") == 0 ) {
      out("Welcome to " MIOS32_LCD_BOOT_MSG_LINE1 "!");
      out("Following commands are available:");
      out("  mcan:                            prints status informations");
      out("  mcan_report:                     prints report about MCANx informations");
      out("  mcan_report_reset:               resets report about MCANx informations");
      out("  mcan_last_err                    prints last CAN bus error");
      out("  mcan_reconnect:                  reconnects MCAN node to the bus");
      out("  set mcan_id <0x00..0x7f>:        changes my MCAN ID (current ID: 0x%02x)\n", MIOS32_CAN_MIDI_NodeIDGet());
      out("  set mcan_verbose <0..4>:         enables MCAN debug messages (verbose level: %d)\n",  MIOS32_CAN_MIDI_VerboseGet());
      out("  reset:                             resets the MIDIbox (!)\n");
      out("  help:                              this page");
    } else if( strcmp(parameter, "reset") == 0 ) {
      MIOS32_SYS_Reset();
    } else {
      out("Unknown command - type 'help' to list available commands!");
    }
  }

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Keyboard Configuration (can also be called from external)
/////////////////////////////////////////////////////////////////////////////
s32 TERMINAL_PrintReport(void *_output_function)
{
  void (*out)(char *format, ...) = _output_function;
  
  out("MCANx Node Id: 0x%02x", MIOS32_CAN_MIDI_NodeIDGet());
  
  if( (MIOS32_MIDI_CheckAvailable(MCAN0) || MIOS32_MIDI_CheckAvailable(MCAN0+16)) == 0){
    out("There's no MCANx available for MIDI!");
  }else{
    if( MIOS32_MIDI_CheckAvailable(MCAN0) ){
      can_stat_report_t report;
      MIOS32_CAN_ReportGetCurr(0, &report);
      out("MCAN0 Rx Packets: %d", report.rx_packets_ctr);
      out("MCAN0 Tx Packets: %d", report.tx_packets_ctr);
      out("MCAN0 Rx Buffer Packets lost: %d, with err:%d", report.rx_packets_err, report.rx_last_buff_err);
      out("MCAN0 Bus Status: %s:", bus_state_str[report.bus_state +1]);
      out("- Rx err counter:%d,", report.bus_curr_err.rec);
      out("- Tx err counter:%d,", report.bus_curr_err.tec);
      out("- %s,",err_state_str[report.bus_curr_err.lec]);
      out("- %s,",report.bus_curr_err.ewgf ? "Warning!" : "No warning");
      out("- %s.",report.bus_curr_err.epvf ? "Passive!" : "Not passive");

    }
      
    if( MIOS32_MIDI_CheckAvailable(MCAN0+16) ){

      
    }
  }

//    out("MCAN Verbose Level: %d", MCAN_VerboseLevelGet());//}
  
  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// help function which parses a decimal or hex value
// returns >= 0 if value is valid
// returns -1 if value is invalid
/////////////////////////////////////////////////////////////////////////////
s32 get_dec(char *word)
{
  if( word == NULL )
    return -1;
  
  char *next;
  long l = strtol(word, &next, 0);
  
  if( word == next )
    return -1;
  
  return l; // value is valid
}
