//! \defgroup FREERTOS_UTILS
//!
//! Utility functions for FreeRTOS
//!
//! Contains functions for performance stat measurements as desribed under:
//! http://www.freertos.org/index.html?http://www.freertos.org/rtos-run-time-stats.html
//!
//! In order to enable performance measurements, add following definitions to
//! your adios_config.h file:
//! \code
//! #define configGENERATE_RUN_TIME_STATS           1
//! #if configGENERATE_RUN_TIME_STATS
//! #define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS  FREERTOS_UTILS_PerfCounterInit
//! #define portGET_RUN_TIME_COUNTER_VALUE          FREERTOS_UTILS_PerfCounterGet
//! #endif
//! \endcode
//!
//! Add following include statement to your Makefile:
//! \code
//! # For performance measurings
//! include $(ADIOS_PATH)/modules/freertos_utils/freertos_utils.mk
//! \endcode
//!
//! Performance stats can be sent to the ADIOS Terminal via:
//! \code
//!   FREERTOS_UTILS_RunTimeStats();
//! \endcode
//! e.g. when a special button has been pushed, or a special MIDI event
//! has been received.
//!
//! Example for a stat table sent to the ADIOS Terminal:
//! \code
//! 00000072321902 ms | Task                 Abs Time             % Time
//! 00000072321902 ms | ================================================
//! 00000072321902 ms | 
//! 00000072321904 ms | Period1mS            113775               5
//! 00000072321905 ms | IDLE                 836250               42
//! 00000072321905 ms | MSD                  19461                <1
//! 00000072321905 ms | Hooks                69434                3
//! 00000072321905 ms | Hooks                260256               13
//! 00000072321905 ms | Period1mS_LP         387894               19
//! 00000072321905 ms | Period1S             34842                1
//! 00000072321906 ms | MIDI                 199942               10
//! 00000072321906 ms | Pattern              26228                1
//! 00000072321906 ms | 
//! \endcode
//! 
//! Note that this performance measurement method is expensive!!!<BR>
//! It should only be used for debugging purposes!
//! 
//! \{
/* ==========================================================================
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
#include <FreeRTOS.h>
#include <portmacro.h>
#include <task.h>
#include "freertos_utils.h"


/////////////////////////////////////////////////////////////////////////////
// Local functions
/////////////////////////////////////////////////////////////////////////////

#if configGENERATE_RUN_TIME_STATS
static void PerfTimerIRQ(void);
static s32 FREERTOS_UTILS_PrintBuffer(char *buffer);
#endif


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static u32 perf_counter;


/////////////////////////////////////////////////////////////////////////////
//! Add this function via 
//! \code
//! #define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS FREERTOS_UTILS_PerfCounterInit
//! \endcode
//! to your adios_config.h file
//!
//! Optionally
//! \code
//! // which ADIOS timer should be used for performance measurements (0..2)
//! #define FREERTOS_UTILS_PERF_TIMER 2
//! 
//! // at which interval should it be called (interval should be much less than 1 mS!)
//! #define FREERTOS_UTILS_PERF_TIMER_PERIOD 10 // uS
//! \endcode
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 FREERTOS_UTILS_PerfCounterInit(void)
{
  perf_counter = 0;

#if configGENERATE_RUN_TIME_STATS == 0
  return -1; // configGENERATE_RUN_TIME_STATS not activated
#else
  return ADIOS_TIMER_Init(FREERTOS_UTILS_PERF_TIMER, FREERTOS_UTILS_PERF_TIMER_PERIOD, PerfTimerIRQ, ADIOS_IRQ_PRIO_HIGHEST);
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Add this function via 
//! \code
//! #define portGET_RUN_TIME_COUNTER_VALUE FREERTOS_UTILS_PerfCounterGet
//! \endcode
//! to your adios_config.h file
//! \return 32bit counter value
/////////////////////////////////////////////////////////////////////////////
u32 FREERTOS_UTILS_PerfCounterGet(void)
{
  return perf_counter;
}


/////////////////////////////////////////////////////////////////////////////
//! Use this function to send the performance stats to the ADIOS terminal
//! via MIDI
//! \return < 0 if MIDI output failed
/////////////////////////////////////////////////////////////////////////////
s32 FREERTOS_UTILS_RunTimeStats(void)
{
#if configGENERATE_RUN_TIME_STATS == 0 && configUSE_TRACE_FACILITY == 0
  return ADIOS_MIDI_SendDebugString("configGENERATE_RUN_TIME_STATS and configUSE_TRACE_FACILITY not activated!\n");  
#else
  signed char buffer[400];
  s32 status = 0;

#if configGENERATE_RUN_TIME_STATS
  // print stats
  status |= ADIOS_MIDI_SendDebugString("================================================\n");
  status |= ADIOS_MIDI_SendDebugString("Task, Abs Time, %% Time\n");
  vTaskGetRunTimeStats(buffer);
  status |= FREERTOS_UTILS_PrintBuffer((char *)buffer);
#endif

#if configUSE_TRACE_FACILITY
  // print task list
  status |= ADIOS_MIDI_SendDebugString("================================================\n");
  status |= ADIOS_MIDI_SendDebugMessage("Task, Status, Priority, StackRemaining/%d, TCB Number\n", sizeof(portSTACK_TYPE));
  vTaskList(buffer);
  status |= FREERTOS_UTILS_PrintBuffer((char *)buffer);
#endif

  status |= ADIOS_MIDI_SendDebugString("================================================\n");

  return status;
#endif
}



/////////////////////////////////////////////////////////////////////////////
// Sends individual SysEx strings to ADIOS terminal which are separated by \n
/////////////////////////////////////////////////////////////////////////////
#if configGENERATE_RUN_TIME_STATS
static s32 FREERTOS_UTILS_PrintBuffer(char *buffer)
{
  s32 status = 0;

  // print buffer line by line
  char *buffer_start = buffer;
  char *buffer_end = buffer;
  u8 end_of_string;
  do {
    // scan for end of line
    while( *buffer_end != '\n' && *buffer_end != 0 ) ++buffer_end;
    // end of string reached?
    end_of_string = *buffer_end == 0;
    // terminate line
    *buffer_end = 0;
    // print line
    status |= ADIOS_MIDI_SendDebugString(buffer_start);
    // continue with next line
    buffer_start = ++buffer_end;
    // until 0 was read
  } while( status >= 0 && !end_of_string );

  return status;
}
#endif

//! \}


#if configGENERATE_RUN_TIME_STATS != 0
// this interrupt increments the performance counter
static void PerfTimerIRQ(void)
{
  ++perf_counter;
}
#endif
