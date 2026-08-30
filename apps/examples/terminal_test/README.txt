terminal_test
=============

A minimal ADIOS example that demonstrates the debug terminal round-trip with
ADIOS Studio (or MIOS Studio): you type a command in the host's "Terminal
(Debug)", the core parses it and answers with DEBUG_MSG.

How it works
------------
- APP_Init() registers CONSOLE_Parse() via
      ADIOS_MIDI_DebugCommandCallback_Init(CONSOLE_Parse);
  The ADIOS SysEx layer delivers the typed line to that callback ONE CHARACTER
  AT A TIME (host input string = F0 00 22 15 32 <id> 0D 00 <ascii...> F7).
- CONSOLE_Parse() buffers characters until '\n', then dispatches the whole line
  to CONSOLE_ParseLine().
- Replies are printed with DEBUG_MSG (= ADIOS_MIDI_SendDebugMessage), which the
  OS wraps as a debug-string SysEx (... 0D 40 <ascii...> F7) back to the host,
  on the same port the command arrived on.

Commands
--------
  ping            -> pong
  hello           -> greeting with the app name/version
  uptime          -> milliseconds since boot
  echo <text>     -> echoes <text> back
  help            -> lists the commands

Target / wiring
---------------
Configured for STM32G070CB with the MIDI connector on UART2 = DIN2 (see
adios_config.h, the BSL_RELAY block) - matching the 5x6 board's FastLane wiring,
so it can be flashed there and reached from ADIOS Studio out of the box. On a
different board, change ADIOS_PROCESSOR and the UART/port in adios_config.h.

Build with your usual ADIOS toolchain (CubeIDE, or `make` with arm-none-eabi-gcc
on PATH). No LCD, no analog in, no USB (none on this chip): the transport is
DIN MIDI only.
