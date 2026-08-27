5x6 FIRMWARE UPDATE  (Beta-Testers Only)
========================================

This updates your TR-505 / TR-626 5x6 board to the current firmware.

YOUR BANKS ARE PRESERVED. The update has been run end to end on both a
TR-626 and a TR-505 without losing a single sound.


WHAT YOU NEED
-------------

  * a MIDI interface with BOTH directions connected:
        interface MIDI OUT  ->  machine MIDI IN
        machine   MIDI OUT  ->  interface MIDI IN
    The update cannot work one-way: the board has to answer.

  * the machine switched ON and running normally.
    QUERY talks to the firmware that is running, so the machine has to be
    awake for it to answer. You switch it off later, at step 4, and not
    before.


FIRST RUN - THE SECURITY WARNING
--------------------------------

The application is not code-signed yet, so your system will complain the
first time. This is expected.

  Windows   "Windows protected your PC"
            -> click "More info", then "Run anyway".

  macOS     "cannot be opened because the developer cannot be verified"
            -> RIGHT-CLICK the application, choose "Open", then "Open"
               again in the dialog. A normal double-click will NOT work
               the first time.


HOW TO RUN IT
-------------

  1. Start the application.

  2. Pick your MIDI In and MIDI Out ports.
     Leave "Device ID" at 0 unless you changed it yourself.

  3. With the machine ON, press QUERY.
     The machine and its current firmware appear. The update then starts
     on its own - there is no second button to press.

  4. When the red instruction appears:

         PLEASE PUT THE MACHINE IN BOOTLOADER MODE
         hold LAST + UP and switch the machine on

     NOW switch the machine OFF. Then hold the two panel buttons LAST and
     UP down, and switch it back ON while still holding them.

  5. A green line appears - "you can release the panel buttons".
     Release them. From here it is automatic.

  6. DO NOT switch the machine off and DO NOT unplug anything until the
     application says it is done. It restarts the machine by itself and
     checks the result; you have nothing to power-cycle.


TWO MACHINES IN A ROW
---------------------

No need to restart the application. When the first one is finished, plug
in the second and press QUERY again. Everything resets by itself; the log
keeps both runs so you can send me the whole thing if anything goes wrong.


ABOUT RED LINES IN THE LOG
--------------------------

Red lines DURING the transfer are not a failure. They mean a MIDI message
was lost or corrupted on the cable and the block was sent again - the
application recovers from that on its own, and the data that goes in is
correct.

They do tell you something useful: your MIDI cable or interface is
dropping traffic. If you see a lot of them, try another cable or another
interface before the next board.

A line saying "MIDI LINK WAS NOT CLEAN" at the very end is the summary of
those events. The update still succeeded.


IF IT STOPS
-----------

Nothing is bricked: the machine keeps a working bootloader at every step,
and it will still answer. Send me the ENTIRE log - select it, copy it -
and tell me which machine it was. The log is what lets me tell a bad
cable from a real problem.


Addictive Instruments
