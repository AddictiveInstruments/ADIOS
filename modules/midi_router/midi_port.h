/*
 * MIDI Port functions
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _MIDI_PORT_H
#define _MIDI_PORT_H

#include <adios.h>

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////////////////////////////////
// global definitions
/////////////////////////////////////////////////////////////////////////////

// number of IN ports (can be overruled from adios_config.h)
#ifndef MIDI_PORT_NUM_IN_PORTS_USB
#define MIDI_PORT_NUM_IN_PORTS_USB ADIOS_USB_MIDI_NUM_PORTS
#endif

#ifndef MIDI_PORT_NUM_IN_PORTS_UART
#define MIDI_PORT_NUM_IN_PORTS_UART ADIOS_UART_NUM
#endif

#ifndef MIDI_PORT_NUM_IN_PORTS_OSC
#define MIDI_PORT_NUM_IN_PORTS_OSC 4
#endif

#ifndef MIDI_PORT_NUM_IN_PORTS_SPIM
#define MIDI_PORT_NUM_IN_PORTS_SPIM ADIOS_SPI_MIDI_NUM_PORTS
#endif


// number of OUT ports (can be overruled from adios_config.h)
#ifndef MIDI_PORT_NUM_OUT_PORTS_USB
#define MIDI_PORT_NUM_OUT_PORTS_USB ADIOS_USB_MIDI_NUM_PORTS
#endif

#ifndef MIDI_PORT_NUM_OUT_PORTS_UART
#define MIDI_PORT_NUM_OUT_PORTS_UART ADIOS_UART_NUM
#endif

#ifndef MIDI_PORT_NUM_OUT_PORTS_OSC
#define MIDI_PORT_NUM_OUT_PORTS_OSC 4
#endif

#ifndef MIDI_PORT_NUM_OUT_PORTS_SPIM
#define MIDI_PORT_NUM_OUT_PORTS_SPIM ADIOS_SPI_MIDI_NUM_PORTS
#endif


// number of CLK ports (can be overruled from adios_config.h)
#ifndef MIDI_PORT_NUM_CLK_PORTS_USB
#define MIDI_PORT_NUM_CLK_PORTS_USB ADIOS_USB_MIDI_NUM_PORTS
#endif

#ifndef MIDI_PORT_NUM_CLK_PORTS_UART
#define MIDI_PORT_NUM_CLK_PORTS_UART ADIOS_UART_NUM
#endif

#ifndef MIDI_PORT_NUM_CLK_PORTS_OSC
#define MIDI_PORT_NUM_CLK_PORTS_OSC 4
#endif

#ifndef MIDI_PORT_NUM_CLK_PORTS_SPIM
#define MIDI_PORT_NUM_CLK_PORTS_SPIM ADIOS_SPI_MIDI_NUM_PORTS
#endif



// keep these constants consistent with the functions in midio_port.c !!!
#define MIDI_PORT_NUM_IN_PORTS  (1 + MIDI_PORT_NUM_IN_PORTS_USB  + MIDI_PORT_NUM_IN_PORTS_UART  + MIDI_PORT_NUM_IN_PORTS_OSC  + MIDI_PORT_NUM_IN_PORTS_SPIM)
#define MIDI_PORT_NUM_OUT_PORTS (1 + MIDI_PORT_NUM_OUT_PORTS_USB + MIDI_PORT_NUM_OUT_PORTS_UART + MIDI_PORT_NUM_OUT_PORTS_OSC + MIDI_PORT_NUM_OUT_PORTS_SPIM)
#define MIDI_PORT_NUM_CLK_PORTS (    MIDI_PORT_NUM_CLK_PORTS_USB + MIDI_PORT_NUM_CLK_PORTS_UART + MIDI_PORT_NUM_CLK_PORTS_OSC + MIDI_PORT_NUM_CLK_PORTS_SPIM)


/////////////////////////////////////////////////////////////////////////////
// Type definitions
/////////////////////////////////////////////////////////////////////////////

typedef union {
  u8 ALL;
  struct {
    u8 MIDI_CLOCK:1;
    u8 ACTIVE_SENSE:1;
  };
} midi_port_mon_filter_t;


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIDI_PORT_Init(u32 mode);

extern s32 MIDI_PORT_InNumGet(void);
extern s32 MIDI_PORT_OutNumGet(void);
extern s32 MIDI_PORT_ClkNumGet(void);

extern char *MIDI_PORT_InNameGet(u8 port_ix);
extern char *MIDI_PORT_OutNameGet(u8 port_ix);
extern char *MIDI_PORT_ClkNameGet(u8 port_ix);

extern adios_midi_port_t MIDI_PORT_InPortGet(u8 port_ix);
extern adios_midi_port_t MIDI_PORT_OutPortGet(u8 port_ix);
extern adios_midi_port_t MIDI_PORT_ClkPortGet(u8 port_ix);

extern u8 MIDI_PORT_InIxGet(adios_midi_port_t port);
extern u8 MIDI_PORT_OutIxGet(adios_midi_port_t port);
extern u8 MIDI_PORT_ClkIxGet(adios_midi_port_t port);

extern s32 MIDI_PORT_InCheckAvailable(adios_midi_port_t port);
extern s32 MIDI_PORT_OutCheckAvailable(adios_midi_port_t port);
extern s32 MIDI_PORT_ClkCheckAvailable(adios_midi_port_t port);

extern adios_midi_package_t MIDI_PORT_OutPackageGet(adios_midi_port_t port);
extern adios_midi_package_t MIDI_PORT_InPackageGet(adios_midi_port_t port);

extern s32 MIDI_PORT_MonFilterSet(midi_port_mon_filter_t filter);
extern midi_port_mon_filter_t MIDI_PORT_MonFilterGet(void);

extern s32 MIDI_PORT_Period1mS(void);

extern s32 MIDI_PORT_NotifyMIDITx(adios_midi_port_t port, adios_midi_package_t package);
extern s32 MIDI_PORT_NotifyMIDIRx(adios_midi_port_t port, adios_midi_package_t package);

extern s32 MIDI_PORT_EventNameGet(adios_midi_package_t package, char *label, u8 num_chars);

/////////////////////////////////////////////////////////////////////////////
// Exported variables
/////////////////////////////////////////////////////////////////////////////


#ifdef __cplusplus
}
#endif

#endif /* _MIDI_PORT_H */
