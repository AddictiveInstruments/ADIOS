/*
 * Header file for MIDI file parser
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _MID_PARSER_H
#define _MID_PARSER_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// can be overruled in adios_config.h
#ifndef MID_PARSER_MAX_TRACKS
#define MID_PARSER_MAX_TRACKS 32
#endif

#ifndef MID_PARSER_META_BUFFER_SIZE
#define MID_PARSER_META_BUFFER_SIZE 80
#endif


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MID_PARSER_Init(u32 mode);

extern s32 MID_PARSER_InstallFileCallbacks(void *mid_parser_read, void *mid_parser_eof, void *mid_parser_seek);
extern s32 MID_PARSER_InstallEventCallbacks(void *mid_parser_playevent, void *mid_parser_playmeta);

extern s32 MID_PARSER_FileIsValid(void);

extern s32 MID_PARSER_Read(void);
extern s32 MID_PARSER_FetchEvents(u32 tick_offset, u32 num_ticks);
extern s32 MID_PARSER_RestartSong(void);

extern s32 MIDI_PARSER_FormatGet(void);
extern s32 MIDI_PARSER_PPQN_Get(void);
extern s32 MIDI_PARSER_TrackNumGet(void);




/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////

#endif /* _MID_PARSER_H */
