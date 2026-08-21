/*
 * Generic MIDI Router functions
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _MIDI_ROUTER_H
#define _MIDI_ROUTER_H

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////////////////////////////////
// global definitions
/////////////////////////////////////////////////////////////////////////////

// can be overruled from adios_config.h
#ifndef MIDI_ROUTER_NUM_NODES
#define MIDI_ROUTER_NUM_NODES  16
#endif

// size of SysEx buffers
// if longer SysEx strings are received, they will be forwarded directly
// in this case, multiple strings concurrently sent to the same port won't be merged correctly anymore.
#ifndef MIDI_ROUTER_SYSEX_BUFFER_SIZE
#define MIDI_ROUTER_SYSEX_BUFFER_SIZE 1024
#endif

// enable this define in adios_defines.h to allow MIDI_ROUTER_SendMIDIClockEvent(u8 evnt0, u32 bpm_tick)
// with bpm_tick > 0
#ifndef MIDI_ROUTER_COMBINED_WITH_SEQ
#define MIDI_ROUTER_COMBINED_WITH_SEQ 0
#endif

/////////////////////////////////////////////////////////////////////////////
// Type definitions
/////////////////////////////////////////////////////////////////////////////

typedef struct {
  u8 src_port; // don't use adios_midi_port_t, since data width is important for save/restore function
  u8 src_chn;  // 0 == Off, 1..16: specific source channel, 17 == All
  u8 dst_port;
  u8 dst_chn;  // 0 == Off, 1..16: specific source channel, 17 == All
} midi_router_node_entry_t;


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIDI_ROUTER_Init(u32 mode);

extern s32 MIDI_ROUTER_Receive(adios_midi_port_t port, adios_midi_package_t midi_package);
extern s32 MIDI_ROUTER_ReceiveSysEx(adios_midi_port_t port, u8 midi_in);

extern s32 MIDI_ROUTER_MIDIClockInGet(adios_midi_port_t port);
extern s32 MIDI_ROUTER_MIDIClockInSet(adios_midi_port_t port, u8 enable);

extern s32 MIDI_ROUTER_MIDIClockOutGet(adios_midi_port_t port);
extern s32 MIDI_ROUTER_MIDIClockOutSet(adios_midi_port_t port, u8 enable);

extern s32 MIDI_ROUTER_SendMIDIClockEvent(u8 evnt0, u32 bpm_tick);

extern s32 MIDI_ROUTER_TerminalHelp(void *_output_function);
extern s32 MIDI_ROUTER_TerminalParseLine(char *input, void *_output_function);
extern s32 MIDI_ROUTER_TerminalPrintConfig(void *_output_function);

/////////////////////////////////////////////////////////////////////////////
// Exported variables
/////////////////////////////////////////////////////////////////////////////

extern midi_router_node_entry_t midi_router_node[MIDI_ROUTER_NUM_NODES];
extern u32 midi_router_mclk_in;
extern u32 midi_router_mclk_out;

#ifdef __cplusplus
}
#endif

#endif /* _MIDI_ROUTER_H */
